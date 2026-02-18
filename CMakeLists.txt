#cmake_minimum_required(VERSION 4.1)
#project(pthread_key_leak)
#
#set(CMAKE_CXX_STANDARD 23)
#set(CMAKE_CXX_STANDARD_REQUIRED ON)
#set(CMAKE_CXX_EXTENSIONS OFF)
#
#
#if (WIN32)
#    find_package(dlfcn-win32 REQUIRED)
#    set(CMAKE_DL_LIBS dl)
#endif ()
##include(rapids-cmake-24.02.00/RAPIDS.cmake)
##include(icm-1.5.7)
##include(CPM.cmake)
##add_subdirectory(stdexec-nvhpc-25.09)
## 添加 CPM
##include(./CPM.cmake)
# file(
#   DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake
#   ${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake
# )
# include(${CMAKE_CURRENT_BINARY_DIR}/cmake/CPM.cmake)
## 获取 stdexec
#CPMFindPackage(
#        NAME stdexec
#        GITHUB_REPOSITORY NVIDIA/stdexec
#        GIT_TAG main
#)
#add_library(throw throw.cpp)
##target_compile_options(throw PRIVATE -fconcepts)
#
#target_link_libraries(throw pthread stdexec "D:/Program Files/JetBrains/CLion 2021.2.2/bin/mingw/lib/gcc/x86_64-w64-mingw32/11.2.0/libstdc++.a")
#
#add_executable(pthread_key_leak main.cpp )
#target_link_libraries(pthread_key_leak PRIVATE ${CMAKE_DL_LIBS} pthread)

cmake_minimum_required(VERSION 3.18)
project(pthread_key_leak CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# GCC 10 的 CMAKE_CXX_STANDARD 20 有时生成 -std=c++2a 而非 -std=c++20，
# stdexec 检查 __cplusplus == 202002L 会失败。
# 强制写入 CMAKE_CXX_FLAGS 确保全局生效，包括 stdexec 子目录。
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    string(FIND "${CMAKE_CXX_FLAGS}" "-std=c++" _pos)
    if(_pos EQUAL -1)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
    endif()
endif()

# ── 关闭 stdexec 所有可选特性 ─────────────────────────────────────────────────
foreach(_opt
        STDEXEC_BUILD_EXAMPLES STDEXEC_BUILD_TESTS STDEXEC_BUILD_DOCS
        STDEXEC_ENABLE_CUDA STDEXEC_ENABLE_NVHPC STDEXEC_ENABLE_IO_URING
        STDEXEC_ENABLE_LIBDISPATCH BUILD_TESTING BUILD_EXAMPLES BUILD_DOCS
)
    set(${_opt} OFF CACHE BOOL "" FORCE)
endforeach()

set(RAPIDS_CMAKE_CPM_DOWNLOAD OFF CACHE BOOL "" FORCE)
set(CPM_DOWNLOAD_ALL          OFF CACHE BOOL "" FORCE)

include(FetchContent)

set(LOCAL "${CMAKE_CURRENT_SOURCE_DIR}/third_party")

if(NOT EXISTS "${LOCAL}/stdexec/CMakeLists.txt")
    message(FATAL_ERROR
            "找不到 third_party/stdexec，请在 WSL 中运行：\n"
            "  cd /mnt/d/CLionProjects/pthread_key_leak\n"
            "  mkdir -p third_party third_party/cpm\n"
            "  ln -sfn $(pwd)/cmake-build-debug/_deps/stdexec-src      third_party/stdexec\n"
            "  ln -sfn $(pwd)/cmake-build-debug/_deps/icm-src           third_party/icm\n"
            "  ln -sfn $(pwd)/cmake-build-debug/_deps/rapids-cmake-src  third_party/rapids-cmake\n"
            "  ln -sfn $(pwd)/cmake-build-debug/cmake/CPM_0.38.5.cmake  third_party/cpm/CPM_0.38.5.cmake"
    )
endif()

# CPM 本地路径覆盖
foreach(_dep icm rapids-cmake)
    if(EXISTS "${LOCAL}/${_dep}")
        set(CPM_${_dep}_SOURCE "${LOCAL}/${_dep}" CACHE STRING "" FORCE)
    endif()
endforeach()
if(EXISTS "${LOCAL}/cpm/CPM_0.38.5.cmake")
    set(CPM_DOWNLOAD_LOCATION "${LOCAL}/cpm/CPM_0.38.5.cmake" CACHE STRING "" FORCE)
endif()

# FetchContent 预注册
foreach(_dep stdexec icm rapids-cmake)
    if(EXISTS "${LOCAL}/${_dep}")
        FetchContent_Declare(${_dep} SOURCE_DIR "${LOCAL}/${_dep}")
        string(REPLACE "-" "_" _safe "${_dep}")
        if(NOT "${_safe}" STREQUAL "${_dep}")
            FetchContent_Declare(${_safe} SOURCE_DIR "${LOCAL}/${_dep}")
        endif()
    endif()
endforeach()

# EXCLUDE_FROM_ALL 防止 examples/tests 被编译
add_subdirectory("${LOCAL}/stdexec" stdexec-build EXCLUDE_FROM_ALL)

add_subdirectory(plugin)
add_subdirectory(main)