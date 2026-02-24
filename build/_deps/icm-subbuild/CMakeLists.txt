# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.25.1)

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(icm-populate NONE)



include(ExternalProject)
ExternalProject_Add(icm-populate
                     "UPDATE_DISCONNECTED" "False"
                    SOURCE_DIR          "/mnt/d/CLionProjects/pthread_key_leak/third_party/icm"
                    BINARY_DIR          "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


