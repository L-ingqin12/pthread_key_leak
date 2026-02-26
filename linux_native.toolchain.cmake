# linux_native.toolchain.cmake
# 用 OHOS NDK clang 编译本机 Linux x86_64，
# 但链接 x86_64-linux-ohos 的 libc++_static.a（模拟 OHOS 静态链接场景）

set(NDK "/root/command-line-tools/sdk/default/openharmony/native")
set(LLVM_BIN "${NDK}/llvm/bin")

set(CMAKE_C_COMPILER   "${LLVM_BIN}/clang")
set(CMAKE_CXX_COMPILER "${LLVM_BIN}/clang++")
set(CMAKE_AR           "${LLVM_BIN}/llvm-ar")
set(CMAKE_RANLIB       "${LLVM_BIN}/llvm-ranlib")

# 本机运行时用 x86_64-unknown-linux-gnu 的动态 libc++（主程序）
set(_inc_target "${NDK}/llvm/include/x86_64-unknown-linux-gnu/c++/v1")
set(_inc_cxx    "${NDK}/llvm/include/c++/v1")
set(_lib_native "${NDK}/llvm/lib/x86_64-unknown-linux-gnu")

# plugin 静态链接用 x86_64-linux-ohos 的 libc++_static.a
# 这两个 .a 文件的 ABI 在 x86_64 上兼容，可以混用
set(_lib_ohos   "${NDK}/llvm/lib/x86_64-linux-ohos")

set(CMAKE_CXX_FLAGS_INIT
        "-stdlib=libc++ -nostdinc++ -I${_inc_target} -I${_inc_cxx}")
set(CMAKE_EXE_LINKER_FLAGS_INIT
        "-stdlib=libc++ -L${_lib_native} -Wl,-rpath,${_lib_native}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT
        "-stdlib=libc++ -L${_lib_native} -Wl,-rpath,${_lib_native}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 让 plugin/CMakeLists.txt 走 OHOS 分支
# OHOS_TRIPLE 指向有 libc++_static.a 的目录
set(CMAKE_SYSTEM_NAME "OHOS"              CACHE STRING "")
set(OHOS_TRIPLE       "x86_64-linux-ohos" CACHE STRING "")

message(STATUS "[native-sim] CXX        : ${LLVM_BIN}/clang++")
message(STATUS "[native-sim] inc        : ${_inc_target}")
message(STATUS "[native-sim] lib native : ${_lib_native}")
message(STATUS "[native-sim] lib ohos   : ${_lib_ohos}")