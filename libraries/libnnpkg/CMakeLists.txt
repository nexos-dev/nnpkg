#[[
    CMakeLists.txt - contains build system for nnpkg main library
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

list(APPEND LIBNNPKG_SOURCES 
            propdb.c 
            pkg.c 
            pkgdb.c 
            strtab.c 
            pkgconf.c
            transaction.c
            indexMan.c)

# Set up PO files
if(NNPKG_ENABLE_NLS)
    gettext_files(DOMAIN libnnpkg
                  OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/po"
                  LANGS "${NNPKG_ENABLED_LOCALES}"
                  SOURCES "${LIBNNPKG_SOURCES}")
endif()

add_library(nnpkgman ${LIBNNPKG_SOURCES})

# Ensure symbols are hidden by default
if(BUILD_SHARED_LIBS)
    set_target_properties(nnpkgman PROPERTIES C_VISIBILITY_PRESET hidden)
endif()

target_include_directories(nnpkgman PUBLIC include)
target_compile_definitions(nnpkgman PRIVATE IN_LIBNNPKG)
target_link_libraries(nnpkgman PUBLIC nex conf)

install(TARGETS nnpkgman)

# Create test suites
list(APPEND LIBNNPKG_TESTS propdb pkg pkgconf strtab)
foreach(test ${LIBNNPKG_TESTS})
    nextest_add_library_test(NAME ${test}
                             SOURCE tests/${test}.c
                             LIBS nnpkgman
                             DEFINES IN_LIBNNPKG
                             WORKDIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)
endforeach()

