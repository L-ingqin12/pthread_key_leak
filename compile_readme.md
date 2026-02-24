

```shell
cmake -B build_ohos/ \
    -DOHOS_NDK_ROOT=$OHOS_SDK \
    -DCMAKE_TOOLCHAIN_FILE=$OHOS_SDK/build/cmake/ohos.toolchain.cmake \
    -DOHOS_ARCH=x86_64 \
    -DOHOS_STL=c++_static \
    -DCMAKE_MAKE_PROGRAM=$OHOS_SDK/build-tools/cmake/bin/ninja \
    -GNinja \
&& cmake \
    --build  build_ohos \
    -j$(nproc)

```
