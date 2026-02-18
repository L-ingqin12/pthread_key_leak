#include <iostream>
#include <stdexcept>
#include <stdexec/execution.hpp>
namespace ex = stdexec;

void func() {
    throw std::runtime_error("test exception from C++");
}

// 定义一个 sender，包装 func
auto make_task() {
    return ex::just(func) | ex::then([](auto f){
        try {
            f();
        } catch (...) {
            // 捕获内部异常，也可以选择 rethrow
            std::cout << "捕获到内部 C++ 异常\n";
            throw;  // 重新抛给最外层
        }
    });
}

extern "C" {
    void do_something() {
        try {
            // 执行 sender
            ex::sync_wait(make_task());
        } catch (const std::exception& e) {
            std::cout << "最外层捕获异常: " << e.what() << "\n";
        } catch (...) {
            std::cout << "最外层捕获未知异常\n";
        }
    }
}
