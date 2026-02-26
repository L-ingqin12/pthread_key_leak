cmake_minimum_required(VERSION 3.18)

add_library(plugin SHARED plugin.cpp)
target_link_libraries(plugin PRIVATE STDEXEC::stdexec)
target_compile_features(plugin PRIVATE cxx_std_20)

# PLUGIN_LINK_MODE=static  → 复现泄漏
# PLUGIN_LINK_MODE=shared  → 改用 libc++_shared（需主程序也用 shared）
# PLUGIN_LINK_MODE=fix     → 静态链接 + --wrap 修复（默认推荐）
set(PLUGIN_LINK_MODE "fix" CACHE STRING "static/shared/fix")
message(STATUS "[plugin] PLUGIN_LINK_MODE = ${PLUGIN_LINK_MODE}")

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    set(_ndk     "/root/command-line-tools/sdk/default/openharmony/native")
    set(_ndk_lib "${_ndk}/llvm/lib/${OHOS_TRIPLE}")

    if(PLUGIN_LINK_MODE STREQUAL "shared")
        target_link_libraries(plugin PRIVATE -L${_ndk_lib} -lc++_shared pthread)
        target_link_options(plugin PRIVATE -Wl,-rpath,${_ndk_lib})

    elseif(PLUGIN_LINK_MODE STREQUAL "fix")
        # 静态嵌入 libc++_static + --wrap 拦截 pthread_key_create
        target_link_libraries(plugin PRIVATE
                -Wl,--push-state,-Bstatic
                ${_ndk_lib}/libc++_static.a
                ${_ndk_lib}/libc++abi.a
                -Wl,--pop-state
                -Wl,--exclude-libs,libc++_static.a
                -Wl,--exclude-libs,libc++abi.a
                pthread
        )
        # 关键：拦截 plugin.so 内所有 pthread_key_create 调用
        target_link_options(plugin PRIVATE
                -Wl,--wrap=pthread_key_create
        )

    else() # static - 复现泄漏
        message("ohos -static in")
        target_link_libraries(plugin PRIVATE
                -Wl,--push-state,-Bstatic
                ${_ndk_lib}/libc++_static.a
                ${_ndk_lib}/libc++abi.a
                -Wl,--pop-state
                -Wl,--exclude-libs,libc++_static.a
                -Wl,--exclude-libs,libc++abi.a
                pthread
        )
    endif()

    target_link_options(plugin PRIVATE
            -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/plugin.map
    )

elseif(UNIX AND NOT APPLE)
    if(PLUGIN_LINK_MODE STREQUAL "fix")
        target_link_options(plugin PRIVATE
                -static-libstdc++ -static-libgcc
                -Wl,--exclude-libs,libstdc++.a,libgcc.a
                -Wl,--wrap=pthread_key_create
        )
    elseif(PLUGIN_LINK_MODE STREQUAL "shared")
        target_link_libraries(plugin PRIVATE stdc++)
    else()
        message("unix -static in")
        target_link_options(plugin PRIVATE
                -static-libstdc++ -static-libgcc
                -Wl,--exclude-libs,libstdc++.a,libgcc.a
        )
    endif()
    target_link_options(plugin PRIVATE
            -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/plugin.map
    )
    target_link_libraries(plugin PRIVATE pthread)

elseif(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(LLVM_MINGW_LIB "D:/SoftWare/Promgram/MinGW/x86_64-w64-mingw32/lib")
    target_compile_options(plugin PRIVATE -stdlib=libc++)
    target_link_options(plugin PRIVATE
            -Wl,-Bstatic
            ${LLVM_MINGW_LIB}/libc++.a ${LLVM_MINGW_LIB}/libc++abi.a
            -Wl,-Bdynamic -Wl,--enable-auto-import)
endif()

set_target_properties(plugin PROPERTIES
        OUTPUT_NAME "leak_plugin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)