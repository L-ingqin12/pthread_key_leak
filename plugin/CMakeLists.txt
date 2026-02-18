cmake_minimum_required(VERSION 3.18)

add_library(plugin SHARED plugin.cpp)
target_link_libraries(plugin PRIVATE STDEXEC::stdexec)
target_compile_features(plugin PRIVATE cxx_std_20)

# ── Linux/WSL：动态链接 libstdc++（默认），不加任何 static 标志 ───────────────
# 这是泄漏复现的关键条件：
#   dlopen  → libstdc++ 初始化 → pthread_key_create(__eh_globals_key)
#   dlclose → libstdc++ 不调用 pthread_key_delete
#   重复 N 次 → key 槽位耗尽 → pthread_key_create 返回 EAGAIN → crash
#
# 注意：不需要 -static-libstdc++，使用动态链接反而更容易复现泄漏，
#       因为每次 dlopen 同一个 .so 都会重新执行其初始化代码。

if(UNIX)
    # 只导出 run_plugin，其余符号隐藏（减少符号污染）
    target_link_options(plugin PRIVATE
            -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/plugin.map
    )
    # 显式动态链接（默认已是，这里只是明确说明）
    target_link_libraries(plugin PRIVATE stdc++ pthread)
endif()

# ── Windows：保持原有静态链接方式 ────────────────────────────────────────────
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(LLVM_MINGW_LIB "D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib")
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