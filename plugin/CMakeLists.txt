cmake_minimum_required(VERSION 3.18)

add_library(plugin SHARED plugin.cpp)
target_link_libraries(plugin PRIVATE STDEXEC::stdexec)
target_compile_features(plugin PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
    # ── Linux 复现 Android c++_static 场景 ───────────────────────────────────
    # plugin.so 静态嵌入 libc++ 完整副本（模拟 NDK c++_static）
    # 主程序动态链接 libc++（模拟 app 主进程）
    # 每次 dlopen/dlclose 泄漏若干 pthread_key，~128 次后耗尽 crash
    #
    # 查找静态 libc++.a 的位置
    find_library(LIBCXX_STATIC  c++  PATHS
            /usr/lib/llvm-12/lib
            /usr/lib/llvm-14/lib
            /usr/lib/x86_64-linux-gnu
            NO_DEFAULT_PATH)
    find_library(LIBCXXABI_STATIC c++abi PATHS
            /usr/lib/llvm-12/lib
            /usr/lib/llvm-14/lib
            /usr/lib/x86_64-linux-gnu
            NO_DEFAULT_PATH)

    if(LIBCXX_STATIC AND LIBCXXABI_STATIC)
        message(STATUS "[plugin] 静态嵌入 libc++: ${LIBCXX_STATIC}")
        message(STATUS "[plugin] 静态嵌入 libc++abi: ${LIBCXXABI_STATIC}")

        # 探测 include 路径
        get_filename_component(_llvm_lib_dir "${LIBCXX_STATIC}" DIRECTORY)
        get_filename_component(_llvm_root "${_llvm_lib_dir}" DIRECTORY)
        set(_llvm_inc "${_llvm_root}/include/c++/v1")

        if(EXISTS "${_llvm_inc}")
            target_compile_options(plugin PRIVATE
                    -stdlib=libc++
                    -nostdinc++
                    -I${_llvm_inc}
            )
        else()
            target_compile_options(plugin PRIVATE -stdlib=libc++)
        endif()

        # 静态链接：整个 libc++ 嵌入 plugin.so
        target_link_options(plugin PRIVATE
                -Wl,--push-state,-Bstatic
                -lc++ -lc++abi
                -Wl,--pop-state
                -Wl,--exclude-libs,libc++.a,libc++abi.a  # 不导出 libc++ 符号
        )
        target_link_libraries(plugin PRIVATE pthread)

    else()
        message(WARNING "[plugin] 未找到静态 libc++.a，回退到动态链接 libstdc++")
        target_link_libraries(plugin PRIVATE stdc++ pthread)
    endif()

elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Windows llvm-mingw：静态嵌入 libc++
    set(LLVM_MINGW_LIB "D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib")
    target_compile_options(plugin PRIVATE -stdlib=libc++)
    target_link_options(plugin PRIVATE
            -Wl,-Bstatic
            ${LLVM_MINGW_LIB}/libc++.a
            ${LLVM_MINGW_LIB}/libc++abi.a
            -Wl,-Bdynamic
            -Wl,--enable-auto-import
    )
endif()

# 只导出 run_plugin，隐藏内部符号（包括 libc++ 内部符号）
if(UNIX AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin.map")
    target_link_options(plugin PRIVATE
            -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/plugin.map
    )
endif()

set_target_properties(plugin PROPERTIES
        OUTPUT_NAME "leak_plugin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)