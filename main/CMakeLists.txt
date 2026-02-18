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
    # 主程序：动态链接 libc++（模拟 app 主进程用 c++_shared）
    find_library(LIBCXX_SHARED c++ PATHS
            /usr/lib/llvm-12/lib
            /usr/lib/llvm-14/lib
            /usr/lib/x86_64-linux-gnu
            NO_DEFAULT_PATH)

    if(LIBCXX_SHARED)
        get_filename_component(_llvm_lib_dir "${LIBCXX_SHARED}" DIRECTORY)
        get_filename_component(_llvm_root "${_llvm_lib_dir}" DIRECTORY)
        set(_llvm_inc "${_llvm_root}/include/c++/v1")
        if(EXISTS "${_llvm_inc}")
            target_compile_options(main_app PRIVATE
                    -stdlib=libc++ -nostdinc++ -I${_llvm_inc})
        else()
            target_compile_options(main_app PRIVATE -stdlib=libc++)
        endif()
        target_link_options(main_app PRIVATE
                -stdlib=libc++ -L${_llvm_lib_dir} -Wl,-rpath,${_llvm_lib_dir})
    else()
        # 回退：用 libstdc++
    endif()
    target_link_libraries(main_app PRIVATE pthread dl)

elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(LLVM_MINGW_LIB "D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib")
    target_compile_options(main_app PRIVATE -stdlib=libc++)
    target_link_libraries(main_app PRIVATE
            ${LLVM_MINGW_LIB}/libc++.dll.a
            ${LLVM_MINGW_LIB}/libc++abi.a)
endif()

set_target_properties(main_app PROPERTIES
        OUTPUT_NAME "leak_checker"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)