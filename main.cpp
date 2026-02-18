// test.c
// #ifdef _WIN32
// #include <windows.h>   // 如果使用 Windows API 替代方案，这里会用到
// #else
#include "dlfcn.h"
// #endif
// #ifdef _WIN32
//     #define LIB_HANDLE HMODULE
//     #define LIB_LOAD(lib) LoadLibrary(lib)
//     #define LIB_SYM(handle, sym) GetProcAddress(handle, sym)
//     #define LIB_CLOSE(handle) FreeLibrary(handle)
//     #define LIB_ERROR() "GetLastError()" // 实际可用 GetLastError 获取错误码
//     #define LIB_SUFFIX ".dll"            // Windows 动态库后缀
// #else
//     #define LIB_HANDLE void*
//     #define LIB_LOAD(lib) dlopen(lib, RTLD_LAZY)
//     #define LIB_SYM(handle, sym) dlsym(handle, sym)
//     #define LIB_CLOSE(handle) dlclose(handle)
//     #define LIB_ERROR() dlerror()
//     #define LIB_SUFFIX ".so"             // Linux 动态库后缀
// #endif
// #endif
#include <pthread.h>
#include <stdio.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <cerrno>

// 检测是否还能创建新key（返回1表示有剩余，0表示已耗尽）
int can_create_key(void) {
    pthread_key_t key;
    int ret = pthread_key_create(&key, NULL);
    if (ret == 0) {
        pthread_key_delete(key);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    // if (argc != 2) {
    //     fstd::cout << stderr, "用法: %s <leak|clean|throw>\n", argv[0]);
    //     return 1;
    // }


    const char *lib_name;
    // if (argv[1][0] == 'l') lib_name = "./libleak.so";
    // else if (argv[1][0] == 'c') lib_name = "./libclean.so";
    // else if (argv[1][0] == 't') lib_name = "./libthrow.so";
    // else {
    //     fstd::cout << stderr, "未知参数，请使用 leak/clean/throw\n");
    //     return 1;
    // }
    lib_name = "./libthrow.dll";

    void *handle;
    void (*func)(void);
    int cycle = 0;
    std::cout << "hello word" << std::endl;
    while (1) {
        try {
            handle = dlopen(lib_name, RTLD_LAZY);
            if (!handle) {
                std::cout <<  "dlopen 失败:\n" << dlerror() << std::endl;
                break;
            }

            *(void**)(&func) = dlsym(handle, "do_something");
            if (!func) {
                std::cout <<   "dlsym 失败\n"<< std::endl;
                dlclose(handle);
                break;
            }

            func();

            if (can_create_key()) {
                std::cout << "循环 ,"<< ++cycle << "剩余key足够\n" << std::endl;
            } else {
                std::cout << "循环 "<< ++cycle <<"警告！无法创建新key，可能已耗尽！\n" << std::endl;
                break;  // 可选择停止
            }

            dlclose(handle);
            std::cout << "库已卸载\n\n" << std::endl;
            usleep(50);  // 0.5秒
        } catch (std::exception &e) {
            std::cout << e.what() << std::endl;
        }

    }
    return 0;
}
