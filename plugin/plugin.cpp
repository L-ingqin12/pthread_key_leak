/**
 * plugin.cpp
 *
 * 动态库内部实现：
 *   - 使用 stdexec bulk 并行执行任务
 *   - 奇数任务 throw，触发 libstdc++ 异常处理路径
 *     （__cxa_get_globals 会调用 pthread_key_create 注册 EH key）
 *   - dlclose 后 libstdc++ 不清理该 key → 泄漏
 *   - 循环足够多次后 pthread_key_create 失败 → crash
 *
 * 对外只暴露：int run_plugin(int task_count);
 */

#ifdef _WIN32
#  define PLUGIN_API __declspec(dllexport)
#else
#  define PLUGIN_API __attribute__((visibility("default")))
#endif

// Suppress deprecated bulk warning
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

static void do_work(int id)
{
    if (id % 2 != 0) {
        // throw 会触发 __cxa_get_globals() → pthread_key_create (若首次)
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
    } catch (...) {
        std::fprintf(stderr, "[plugin] unknown exception\n");
        return 3;
    }

    return (failure_count.load() > 0) ? 1 : 0;
}