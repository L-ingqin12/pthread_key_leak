/**
 * plugin.cpp
 *
 * 修复方案B：拦截 pthread_key_create，在 dlclose 时统一 delete
 *
 * 原理：
 *   用 -Wl,--wrap=pthread_key_create 拦截 plugin.so 内所有的
 *   pthread_key_create 调用（包括 libc++_static 内部的），
 *   记录所有创建的 key，在 __attribute__((destructor)) 里统一清理。
 *
 * 效果：
 *   dlclose(plugin.so) → plugin_fini() → pthread_key_delete(all keys)
 *   → 下次 dlopen 重新创建 → key 总数不增长 → 不再 crash
 */

#ifdef _WIN32
#  define PLUGIN_API __declspec(dllexport)
#else
#  define PLUGIN_API __attribute__((visibility("default")))
#endif

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>
#ifdef __clang__
#  pragma clang diagnostic pop
#endif

#include <atomic>
#include <stdexcept>
#include <string>
#include <thread>
#include <cstdio>
#include <pthread.h>
#include <vector>
#include <mutex>

// ── pthread_key 拦截 ──────────────────────────────────────────────────────────
// 通过 -Wl,--wrap=pthread_key_create 将 plugin.so 内所有的
// pthread_key_create 调用重定向到 __wrap_pthread_key_create

#ifndef _WIN32

// 真实函数由链接器通过 --wrap 提供
extern "C" int __real_pthread_key_create(pthread_key_t* key, void (*dtor)(void*));

namespace {
    std::vector<pthread_key_t> g_tracked_keys;
    std::mutex                 g_keys_mutex;
}

extern "C" int __wrap_pthread_key_create(pthread_key_t* key, void (*dtor)(void*))
{
    int ret = __real_pthread_key_create(key, dtor);
    if (ret == 0) {
        std::lock_guard<std::mutex> lk(g_keys_mutex);
        g_tracked_keys.push_back(*key);
    }
    return ret;
}

__attribute__((constructor))
static void plugin_init()
{
    std::lock_guard<std::mutex> lk(g_keys_mutex);
    g_tracked_keys.clear();
}

__attribute__((destructor))
static void plugin_fini()
{
    // dlclose 时调用：删除本 .so 生命周期内创建的所有 key
    std::lock_guard<std::mutex> lk(g_keys_mutex);
    for (pthread_key_t k : g_tracked_keys) {
        pthread_key_delete(k);
    }
    std::fprintf(stderr, "[plugin] fini: deleted %zu pthread keys\n",
             g_tracked_keys.size());  // 此时已 clear，打印 0；改为提前记录
    g_tracked_keys.clear();

}

#endif // !_WIN32

// ── 业务逻辑 ──────────────────────────────────────────────────────────────────

static void do_work(int id)
{
    if (id % 2 != 0) {
        throw std::runtime_error(
            std::string("task ") + std::to_string(id) + " failed intentionally");
    }
    volatile int x = id * id;
    (void)x;
}

extern "C" PLUGIN_API
int run_plugin(int task_count)
{
    std::atomic<int> failure_count{0};

    try {
        const unsigned hw = std::max(2u, std::thread::hardware_concurrency());
        exec::static_thread_pool pool(hw);
        auto sched = pool.get_scheduler();

#ifdef __clang__
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
        auto pipeline =
            stdexec::just()
            | stdexec::transfer(sched)
            | stdexec::bulk(
                static_cast<std::size_t>(task_count),
                [&](std::size_t idx) noexcept {
                    try {
                        do_work(static_cast<int>(idx));
                    } catch (const std::exception& ex) {
                        ++failure_count;
                        std::fprintf(stderr,
                            "[plugin] caught in task %zu: %s\n",
                            idx, ex.what());
                    } catch (...) {
                        ++failure_count;
                    }
                });
#ifdef __clang__
#  pragma clang diagnostic pop
#endif

        stdexec::sync_wait(std::move(pipeline));

    } catch (const std::exception& ex) {
        std::fprintf(stderr, "[plugin] infrastructure exception: %s\n", ex.what());
        return 2;
    }

    return (failure_count.load() > 0) ? 1 : 0;
}