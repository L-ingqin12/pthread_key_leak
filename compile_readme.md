

```shell
# ohos
cmake -B build_ohos/ \
    -DOHOS_NDK_ROOT=$OHOS_SDK \
    -DCMAKE_TOOLCHAIN_FILE=$OHOS_SDK/build/cmake/ohos.toolchain.cmake \
    -DOHOS_ARCH=x86_64 \
    -DOHOS_STL=c++_static \
    -DCMAKE_MAKE_PROGRAM=$OHOS_SDK/build-tools/cmake/bin/ninja \
    -GNinja \
&& cmake \
    --build  build_ohos \
    -j$(nproc)
    
# linux native
  
cmake -B build_native_sim/ \
    -DCMAKE_TOOLCHAIN_FILE=linux_native.toolchain.cmake \
    -DPLUGIN_LINK_MODE=static \
&& cmake --build build_native_sim/ -j$(nproc)


cd /mnt/d/CLionProjects/pthread_key_leak

# ── 复现泄漏（默认）──────────────────────────────────────────────────────────
rm -rf build_native_sim/
cmake -B build_native_sim/ \
    -DCMAKE_TOOLCHAIN_FILE=linux_native.toolchain.cmake \
    -DPLUGIN_LINK_MODE=static \
&& cmake --build build_native_sim/ -j$(nproc)

echo "=== LEAK MODE ==="
build_native_sim/bin/leak_checker \
    build_native_sim/bin/libleak_plugin.so 600 8

# ── 验证修复 ──────────────────────────────────────────────────────────────────
rm -rf build_native_fix/
cmake -B build_native_fix/ \
    -DCMAKE_TOOLCHAIN_FILE=linux_native.toolchain.cmake \
    -DPLUGIN_LINK_MODE=shared \
&& cmake --build build_native_fix/ -j$(nproc)

echo "=== FIX MODE ==="
LD_LIBRARY_PATH=/root/command-line-tools/sdk/default/openharmony/native/llvm/lib/x86_64-unknown-linux-gnu \
build_native_fix/bin/leak_checker \
    build_native_fix/bin/libleak_plugin.so 1000 8
```
这是 OHOS/Android 上 `c++_shared` 的经典符号冲突问题。原因是：

```
主程序用 libc++_static
plugin 改用 libc++_shared
  → 两份 libc++ 的 typeinfo/vtable 不兼容
  → dlsym 找到符号但调用时崩溃或返回错误
  → 或者 soname 版本不匹配导致 dlsym 直接失败
```

先确认具体的错误现象：

```bash
# 加上 dlopen 的详细错误信息
cat > /tmp/test_dlsym.cpp << 'EOF'
#include <dlfcn.h>
#include <cstdio>
int main(int argc, char* argv[]) {
    dlerror(); // 清除旧错误
    void* h = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!h) {
        printf("dlopen failed: %s\n", dlerror());
        return 1;
    }
    dlerror();
    void* sym = dlsym(h, "run_plugin");
    const char* err = dlerror();
    if (err) {
        printf("dlsym failed: %s\n", err);
    } else {
        printf("dlsym OK: %p\n", sym);
    }
    dlclose(h);
}
EOF
```

但根本问题在于主程序用 `libc++_static`，这个约束不能改。**真正的解决方案是让 plugin 的 libc++ 符号对主程序完全不可见，同时 plugin 内部正确清理 key**。

有三个方向：先说清楚三个方案的原理，再选最适合你的：

---

## 三个方案详解

### 方案A：`__cxa_finalize` + destructor（侵入性最小）

```cpp
// 在 plugin.so 的 destructor 里调用
__attribute__((destructor))
static void plugin_fini() {
    extern void __cxa_finalize(void*);
    extern void* __dso_handle;
    __cxa_finalize(__dso_handle);  // 触发本 .so 注册的所有 atexit 清理
}
```

**局限**：`__cxa_finalize` 只清理 `atexit` 注册的回调，不一定覆盖 `__thread_specific_ptr` 的 key。

---

### 方案B：拦截 `pthread_key_create`，记录并在 destructor 里统一删除（最彻底）

```cpp
// 在 plugin 内部拦截所有 pthread_key_create 调用
static std::vector<pthread_key_t> g_plugin_keys;
static pthread_mutex_t g_keys_mutex = PTHREAD_MUTEX_INITIALIZER;

// 用 --wrap 链接选项拦截
extern "C" int __real_pthread_key_create(pthread_key_t*, void(*)(void*));
extern "C" int __wrap_pthread_key_create(pthread_key_t* key, void(*dtor)(void*)) {
    int ret = __real_pthread_key_create(key, dtor);
    if (ret == 0) {
        pthread_mutex_lock(&g_keys_mutex);
        g_plugin_keys.push_back(*key);
        pthread_mutex_unlock(&g_keys_mutex);
    }
    return ret;
}

__attribute__((destructor))
static void plugin_fini() {
    pthread_mutex_lock(&g_keys_mutex);
    for (auto k : g_plugin_keys) pthread_key_delete(k);
    g_plugin_keys.clear();
    pthread_mutex_unlock(&g_keys_mutex);
}
```

链接时加 `-Wl,--wrap=pthread_key_create`。

---

### 方案C：主程序缓存句柄，避免重复 dlopen（零侵入）

```cpp
// 主程序侧修改
class PluginHandle {
    void* handle_ = nullptr;
public:
    void* get() {
        if (!handle_) {
            handle_ = dlopen("plugin.so", RTLD_NOW | RTLD_LOCAL);
        }
        return handle_;  // 只 dlopen 一次，永不 dlclose
    }
    // 进程退出时 OS 自动回收，不需要显式 dlclose
};
```

---

## 推荐：方案B，直接实现

替换两个文件后对比验证：

```bash
cd /mnt/d/CLionProjects/pthread_key_leak

# 泄漏模式（对照组）
rm -rf build_leak/ && cmake -B build_leak/ \
    -DCMAKE_TOOLCHAIN_FILE=linux_native.toolchain.cmake \
    -DPLUGIN_LINK_MODE=static \
&& cmake --build build_leak/ -j$(nproc)
echo "=== LEAK ===" && build_leak/bin/leak_checker \
    build_leak/bin/libleak_plugin.so 600 8

# 修复模式
rm -rf build_fix/ && cmake -B build_fix/ \
    -DCMAKE_TOOLCHAIN_FILE=linux_native.toolchain.cmake \
    -DPLUGIN_LINK_MODE=fix \
&& cmake --build build_fix/ -j$(nproc)
echo "=== FIX ===" && build_fix/bin/leak_checker \
    build_fix/bin/libleak_plugin.so 1000 8
```

---

**方案B 的核心机制：**

```
链接时：-Wl,--wrap=pthread_key_create
  plugin.so 内所有对 pthread_key_create 的调用
  → 重定向到 __wrap_pthread_key_create
  → 记录 key 到 g_tracked_keys

dlclose(plugin.so) 时：
  → __attribute__((destructor)) plugin_fini() 被调用
  → for each key: pthread_key_delete(key)
  → key 槽位归还，下次 dlopen 重新使用
  → 永不耗尽
```

这个方案完全不改变主程序，不依赖 `c++_shared`，对业务层零侵入。