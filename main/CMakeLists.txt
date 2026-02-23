#cmake_minimum_required(VERSION 3.18)
#
#add_executable(main_app main.cpp)
#target_compile_features(main_app PRIVATE cxx_std_20)
#
## ── 链接 libc++ ───────────────────────────────────────────────────────────────
##
## 环境：Clang 22 (x86_64-w64-windows-gnu / llvm-mingw)
##   - cfg 文件硬编码使用 libc++ 头文件，-stdlib=libc++ 被忽略
##   - libc++.a / libc++abi.a 位于：
##     D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib/
##   - CMake 未自动将此路径传给链接器
##
## 解决：用绝对路径直接引用 .a 文件，完全绕过库名搜索
#
#if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND WIN32)
#    set(LLVM_MINGW_LIB "D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib")
#    target_link_libraries(main_app PRIVATE
#            ${LLVM_MINGW_LIB}/libc++.dll.a      # 动态链接 libc++ (main 用动态即可)
##            ${LLVM_MINGW_LIB}/libc++abi.a
#    )
#elseif(UNIX)
#    # Linux/WSL: GCC + libstdc++ 自动链接，只需 -ldl
#    target_link_libraries(main_app PRIVATE dl)
#endif()
#
#set_target_properties(main_app PROPERTIES
#        OUTPUT_NAME "leak_checker"
#        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
#)

cmake_minimum_required(VERSION 3.18)

add_executable(main_app main.cpp)
target_compile_features(main_app PRIVATE cxx_std_20)

if(UNIX AND NOT APPLE)
    # 主程序：动态链接 libstdc++（模拟主进程用 c++_shared）
    # 不加 -static-libstdc++，走系统动态库
    target_link_libraries(main_app PRIVATE pthread dl)

elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(LLVM_MINGW_LIB "D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib")
    target_compile_options(main_app PRIVATE -stdlib=libc++)
    target_link_libraries(main_app PRIVATE
            ${LLVM_MINGW_LIB}/libc++.dll.a
            ${LLVM_MINGW_LIB}/libc++abi.a
    )
endif()

set_target_properties(main_app PROPERTIES
        OUTPUT_NAME "leak_checker"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)