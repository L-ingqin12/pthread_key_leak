# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/mnt/d/CLionProjects/pthread_key_leak/third_party/icm"
  "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-build"
  "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix"
  "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix/tmp"
  "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix/src/icm-populate-stamp"
  "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix/src"
  "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix/src/icm-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix/src/icm-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/mnt/d/CLionProjects/pthread_key_leak/build/_deps/icm-subbuild/icm-populate-prefix/src/icm-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
