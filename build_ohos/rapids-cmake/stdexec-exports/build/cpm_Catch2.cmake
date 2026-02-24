#=============================================================================
# Copyright (c) 2026, NVIDIA CORPORATION.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#=============================================================================


# CPM Search for Catch2
#
# Make sure we search for a build-dir config module for the CPM project
set(possible_package_dir "/mnt/d/CLionProjects/pthread_key_leak/build_ohos/_deps/catch2-build")
if(possible_package_dir AND NOT DEFINED Catch2_DIR)
  set(Catch2_DIR "${possible_package_dir}")
endif()

CPMFindPackage(
  "NAME;Catch2;VERSION;2.13.6;URL;https://github.com/catchorg/Catch2/archive/refs/tags/v2.13.6.zip"
  )

if(possible_package_dir)
  unset(possible_package_dir)
endif()
#=============================================================================
