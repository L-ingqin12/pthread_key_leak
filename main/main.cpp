/**
 * main.cpp  —  TLS / pthread_key leak detector
 *
 * 循环 dlopen/dlclose leak_plugin，每轮调用 run_plugin()，
 * 每轮后统计剩余可用 pthread_key 槽位，观察是否单调递减。
 *
 * Linux 上 PTHREAD_KEYS_MAX = 1024。
 * libstdc++ 的异常处理全局状态（__cxa_eh_globals）使用 pthread_key 存储，
 * 在某些版本的 dlclose 中不会调用 pthread_key_delete，
 * 导致每次 dlopen/throw/dlclose 泄漏若干个 key。
 * 约 128~200 轮后 pthread_key_create 返回 EAGAIN，程序 crash。
 *
 * 用法：
 *   ./leak_checker [plugin_path] [rounds] [tasks_per_round]
 *   ./leak_checker ./libleak_plugin.so 200 8
 */

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>
#  include <pthread.h>
#  include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <climits>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

// ── platform shims ────────────────────────────────────────────────────────────
#ifdef _WIN32
using LibHandle = HMODULE;
static LibHandle lib_open(const char* p)  { return LoadLibraryA(p); }
static void      lib_close(LibHandle h)   { FreeLibrary(h); }
static void*     lib_sym(LibHandle h, const char* s) {
    return reinterpret_cast<void*>(GetProcAddress(h, s));
}
static std::string last_error() {
    char buf[512] = {};
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
    const char* e = dlerror(); return e ? e : "unknown";
}
#endif

// ── TLS 用量测量 ──────────────────────────────────────────────────────────────
#ifdef _WIN32
static constexpr int WIN_TLS_TOTAL = TLS_MINIMUM_AVAILABLE + 1024; // 1088
static int tls_used()
{
    std::vector<DWORD> slots;
    slots.reserve(WIN_TLS_TOTAL);
    for (;;) {
        DWORD idx = TlsAlloc();
        if (idx == TLS_OUT_OF_INDEXES) break;
        slots.push_back(idx);
    }
    int free_count = static_cast<int>(slots.size());
    for (DWORD idx : slots) TlsFree(idx);
    return WIN_TLS_TOTAL - free_count;
}
static int tls_total() { return WIN_TLS_TOTAL; }

#else
// Linux：直接读 /proc/self/status 里的 pthread key 信息不可靠，
// 仍用穷举法，但同时记录"可用槽减少量"更直观。
static int tls_free()   // 返回当前可用 key 数
{
    std::vector<pthread_key_t> keys;
    keys.reserve(PTHREAD_KEYS_MAX);
    for (;;) {
        pthread_key_t k{};
        if (pthread_key_create(&k, nullptr) != 0) break;
        keys.push_back(k);
    }
    int free_count = static_cast<int>(keys.size());
    for (auto k : keys) pthread_key_delete(k);
    return free_count;
}
static int tls_used()  { return PTHREAD_KEYS_MAX - tls_free(); }
static int tls_total() { return static_cast<int>(PTHREAD_KEYS_MAX); }
#endif

static void report(int round, int prev_used)
{
    int used = tls_used();
    int delta = (prev_used >= 0) ? (used - prev_used) : 0;
    const char* trend = (delta > 0) ? " ▲ LEAK" : (delta < 0) ? " ▼" : "";

#ifdef _WIN32
    std::printf("[round %4d] TLS slots used   : %4d / %d  (Δ%+d)%s\n",
                round, used, tls_total(), delta, trend);
#else
    std::printf("[round %4d] pthread_keys used : %4d / %d  (Δ%+d)%s\n",
                round, used, tls_total(), delta, trend);
#endif
    std::fflush(stdout);
}

// ── entry point ───────────────────────────────────────────────────────────────
using RunPluginFn = int(*)(int);

int main(int argc, char* argv[])
{
    const char* plugin_path =
#ifdef _WIN32
        (argc >= 2) ? argv[1] : "libleak_plugin.dll";
#else
        (argc >= 2) ? argv[1] : "./libleak_plugin.so";
#endif
    int rounds     = (argc >= 3) ? std::atoi(argv[2]) : 200;
    int task_count = (argc >= 4) ? std::atoi(argv[3]) : 8;

    std::printf("=== pthread_key / TLS leak detector ===\n");
    std::printf("Plugin     : %s\n", plugin_path);
    std::printf("Rounds     : %d\n", rounds);
    std::printf("Tasks/rnd  : %d\n", task_count);
    std::printf("TLS total  : %d\n\n", tls_total());

    int prev = tls_used();
    std::printf("[round    0] pthread_keys used : %4d / %d  (baseline)\n\n",
                prev, tls_total());
    std::fflush(stdout);

    int leak_count = 0;

    for (int r = 1; r <= rounds; ++r) {

        LibHandle h = lib_open(plugin_path);
        if (!h) {
            std::fprintf(stderr,
                "\n[ERROR] dlopen(\"%s\") failed: %s\n"
                "  请确认 .so/.dll 路径正确。\n",
                plugin_path, last_error().c_str());
            return 1;
        }

        auto fn = reinterpret_cast<RunPluginFn>(lib_sym(h, "run_plugin"));
        if (!fn) {
            std::fprintf(stderr, "[ERROR] 找不到符号 run_plugin\n");
            lib_close(h);
            return 1;
        }

        fn(task_count);  // 忽略返回值，throw 已在内部捕获

        lib_close(h);

        // 每 10 轮报告一次，减少穷举测量的开销
        if (r % 10 == 0 || r == 1) {
            int cur = tls_used();
            int delta = cur - prev;
            if (delta > 0) leak_count += delta;

#ifdef _WIN32
            std::printf("[round %4d] TLS slots used   : %4d / %d  (Δ%+d)%s\n",
#else
            std::printf("[round %4d] pthread_keys used : %4d / %d  (Δ%+d)%s\n",
#endif
                        r, cur, tls_total(), delta,
                        delta > 0 ? "  ← LEAK" : "");
            std::fflush(stdout);
            prev = cur;
        }
    }

    std::printf("\n=== 结论 ===\n");
    std::printf("累计泄漏 key 数: %d\n", leak_count);
    if (leak_count > 0) {
        std::printf("结果：存在 pthread_key 泄漏！\n");
        std::printf("预计在约 %d 轮后会因 key 耗尽而 crash。\n",
                    (tls_total() - tls_used()) / std::max(1, leak_count / rounds) + rounds);
    } else {
        std::printf("结果：未检测到 pthread_key 泄漏。\n");
        std::printf("（Windows/libc++ 环境下泄漏可能不明显，请在 Linux/WSL 上验证）\n");
    }
    return 0;
}