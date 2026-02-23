/**
 * main.cpp - pthread_key / TLS leak detector
 *
 * 复现场景：Android NDK c++_static / llvm libc++_static
 *   - plugin.so 静态嵌入 C++ 运行时完整副本
 *   - 每次 dlopen 时运行时重新初始化，调用 pthread_key_create 注册内部 key
 *   - dlclose 时不完整清理，pthread_key_delete 未调用
 *   - 循环后 key 槽位耗尽，pthread_key_create 返回 EAGAIN -> crash
 *
 * 用法：
 *   ./leak_checker [plugin_path] [rounds] [tasks_per_round]
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <pthread.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <vector>
#include <string>

// ── platform shims ────────────────────────────────────────────────────────────
#ifdef _WIN32
using LibHandle = HMODULE;
static LibHandle lib_open(const char* p)  { return LoadLibraryA(p); }
static void      lib_close(LibHandle h)   { FreeLibrary(h); }
static void*     lib_sym(LibHandle h, const char* s) {
    return reinterpret_cast<void*>(GetProcAddress(h, s));
}
static std::string last_error() {
    char buf[512]{};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr,
                   GetLastError(), 0, buf, sizeof(buf), nullptr);
    return buf;
}
#else
using LibHandle = void*;
static LibHandle lib_open(const char* p)  { return dlopen(p, RTLD_NOW | RTLD_LOCAL); }
static void      lib_close(LibHandle h)   { dlclose(h); }
static void*     lib_sym(LibHandle h, const char* s) { return dlsym(h, s); }
static std::string last_error() {
    const char* e = dlerror();
    return e ? e : "unknown";
}
#endif

// ── TLS key 用量测量 ──────────────────────────────────────────────────────────
#ifdef _WIN32
static constexpr int TLS_TOTAL = TLS_MINIMUM_AVAILABLE + 1024; // 1088
static int tls_used() {
    std::vector<DWORD> slots;
    slots.reserve(TLS_TOTAL);
    for (;;) {
        DWORD idx = TlsAlloc();
        if (idx == TLS_OUT_OF_INDEXES) break;
        slots.push_back(idx);
    }
    int free_n = static_cast<int>(slots.size());
    for (DWORD idx : slots) TlsFree(idx);
    return TLS_TOTAL - free_n;
}
static int tls_total() { return TLS_TOTAL; }
#else
static int tls_used() {
    std::vector<pthread_key_t> keys;
    keys.reserve(PTHREAD_KEYS_MAX);
    for (;;) {
        pthread_key_t k{};
        if (pthread_key_create(&k, nullptr) != 0) break;
        keys.push_back(k);
    }
    int free_n = static_cast<int>(keys.size());
    for (auto k : keys) pthread_key_delete(k);
    return PTHREAD_KEYS_MAX - free_n;
}
static int tls_total() { return PTHREAD_KEYS_MAX; }
#endif

using RunPluginFn = int(*)(int);

int main(int argc, char* argv[])
{
    // 强制 UTF-8 输出，避免乱码
    setbuf(stdout, nullptr);

    const char* plugin_path =
#ifdef _WIN32
        (argc >= 2) ? argv[1] : "libleak_plugin.dll";
#else
        (argc >= 2) ? argv[1] : "./libleak_plugin.so";
#endif
    int rounds     = (argc >= 3) ? std::atoi(argv[2]) : 200;
    int task_count = (argc >= 4) ? std::atoi(argv[3]) : 8;

    printf("=== pthread_key leak detector ===\n");
    printf("Plugin     : %s\n", plugin_path);
    printf("Rounds     : %d\n", rounds);
    printf("Tasks/rnd  : %d\n", task_count);
    printf("Key total  : %d\n\n", tls_total());

    int baseline = tls_used();
    printf("[round    0] keys used: %4d / %d  (baseline)\n\n",
           baseline, tls_total());
    fflush(stdout);

    int prev = baseline;
    int total_leaked = 0;

    for (int r = 1; r <= rounds; ++r) {
        LibHandle h = lib_open(plugin_path);
        if (!h) {
            fprintf(stderr, "\n[ERROR] dlopen failed: %s\n", last_error().c_str());
            return 1;
        }

        auto fn = reinterpret_cast<RunPluginFn>(lib_sym(h, "run_plugin"));
        if (!fn) {
            fprintf(stderr, "[ERROR] symbol run_plugin not found\n");
            lib_close(h);
            return 1;
        }

        fn(task_count);
        lib_close(h);

        // 每 10 轮统计一次（减少穷举测量开销）
        if (r % 10 == 0 || r == 1) {
            int cur   = tls_used();
            int delta = cur - prev;
            total_leaked += (delta > 0 ? delta : 0);

            printf("[round %4d] keys used: %4d / %d  (delta: %+d)%s\n",
                   r, cur, tls_total(), delta,
                   delta > 0 ? "  <-- LEAK" : "");
            fflush(stdout);
            prev = cur;
        }
    }

    int final_used = tls_used();
    int net_leaked = final_used - baseline;

    printf("\n=== Result ===\n");
    printf("Baseline  : %d keys\n", baseline);
    printf("Final     : %d keys\n", final_used);
    printf("Net leak  : %d keys\n", net_leaked);

    if (net_leaked > 0) {
        int remaining = tls_total() - final_used;
        int crash_est = rounds + remaining / (net_leaked / rounds + 1);
        printf("Status    : LEAK CONFIRMED\n");
        printf("Est crash : ~%d rounds\n", crash_est);
        return 1;
    } else {
        printf("Status    : no leak detected\n");
        printf("Note: if using static C++ runtime in plugin, check:\n");
        printf("  ldd libleak_plugin.so  (should NOT list libstdc++.so)\n");
        printf("  nm -D libleak_plugin.so | grep cxa\n");
        return 0;
    }
}