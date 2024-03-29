#[[
    CMakeLists.txt - contains build system for nnpkg frontend
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
project(nnpkg-cli LANGUAGES C)

list(APPEND NNPKG_CLI_SOURCES main.c initDb.c addPkg.c)

# Set up PO files
if(NNPKG_ENABLE_NLS)
    gettext_files(DOMAIN nnpkg
                  OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/po"
                  LANGS "${NNPKG_ENABLED_LOCALES}"
                  SOURCES "${NNPKG_CLI_SOURCES}")
endif()

add_executable(nnpkg ${NNPKG_CLI_SOURCES})
set_target_properties(nnpkg PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(nnpkg PRIVATE nnpkgman)

install(TARGETS nnpkg DESTINATION ${CMAKE_INSTALL_BINDIR})
