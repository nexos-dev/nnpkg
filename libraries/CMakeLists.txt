#[[
    CMakeLists.txt - contains build system for libraries
    Copyright 2022 The NexNix Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

         http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
]]

cmake_minimum_required(VERSION 3.00)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
cmake_policy(SET CMP0077 NEW)

set(CHARDET_BUILDONLY ON)
set(LIBCONF_BUILDONLY ON)
set(LIBCONF_LINK_DEPS OFF)
set(LIBNEX_BUILDONLY ON)

add_subdirectory(libchardet)
add_subdirectory(libnex)
add_subdirectory(libconf)
add_subdirectory(libnnpkg)
