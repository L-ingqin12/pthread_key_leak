# linux_native.toolchain.cmake
# 用 OHOS NDK clang 编译本机 Linux x86_64 可执行文件
# 链接 libc++_static 方式与 OHOS 完全一致，可直接在 WSL 运行验证泄漏

set(NDK "/root/command-line-tools/sdk/default/openharmony/native")
set(LLVM_BIN "${NDK}/llvm/bin")

set(CMAKE_C_COMPILER   "${LLVM_BIN}/clang")
set(CMAKE_CXX_COMPILER "${LLVM_BIN}/clang++")
set(CMAKE_AR           "${LLVM_BIN}/llvm-ar")
set(CMAKE_RANLIB       "${LLVM_BIN}/llvm-ranlib")

# 本机 Linux 的 libc++ 路径
set(_inc_target "${NDK}/llvm/include/x86_64-unknown-linux-gnu/c++/v1")
set(_inc_cxx    "${NDK}/llvm/include/c++/v1")
set(_ndk_lib    "${NDK}/llvm/lib/x86_64-unknown-linux-gnu")

set(CMAKE_CXX_FLAGS_INIT
    "-stdlib=libc++ -nostdinc++ -I${_inc_target} -I${_inc_cxx}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-stdlib=libc++ -L${_ndk_lib} -Wl,-rpath,${_ndk_lib}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT
    "-stdlib=libc++ -L${_ndk_lib} -Wl,-rpath,${_ndk_lib}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 让 plugin/CMakeLists.txt 走 OHOS 分支，使用 libc++_static
set(CMAKE_SYSTEM_NAME "OHOS"                     CACHE STRING "")
set(OHOS_TRIPLE       "x86_64-unknown-linux-gnu" CACHE STRING "")

message(STATUS "[native-sim] CXX    : ${LLVM_BIN}/clang++")
message(STATUS "[native-sim] inc    : ${_inc_target}")
message(STATUS "[native-sim] lib    : ${_ndk_lib}")
message(STATUS "[native-sim] triple : x86_64-unknown-linux-gnu")
