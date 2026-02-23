cmake_minimum_required(VERSION 3.18)

add_library(plugin SHARED plugin.cpp)
target_link_libraries(plugin PRIVATE STDEXEC::stdexec)
target_compile_features(plugin PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
    # ── 复现 c++_static 场景：静态嵌入 libstdc++ 进 plugin.so ────────────────
    # GCC：-static-libstdc++ -static-libgcc 将运行时静态链接进 .so
    # 效果等同于 Android NDK 的 c++_static：
    #   每次 dlopen 时 .so 内部的 libstdc++ 重新初始化，注册新 pthread_key
    #   dlclose 时不完整清理 → 泄漏
    target_link_options(plugin PRIVATE
            -static-libstdc++
            -static-libgcc
            -Wl,--exclude-libs,libstdc++.a  # 隐藏静态嵌入的符号，防止被主程序抢占
            -Wl,--exclude-libs,libgcc.a
    )
    target_link_libraries(plugin PRIVATE pthread)

    # version script：只导出 run_plugin
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/plugin.map")
        target_link_options(plugin PRIVATE
                -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/plugin.map
        )
    endif()

elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

set_target_properties(plugin PROPERTIES
        OUTPUT_NAME "leak_plugin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)