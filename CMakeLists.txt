#[[
    CMakeLists.txt - contains build system for nnpkg
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
project(nnpkg VERSION 0.1.0 LANGUAGES C CXX)

option(NNPKG_ENABLE_TESTS "Enable unit tests for nnpkg" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(NNPKG_ENABLE_NLS "Enable native language support in nnpkg" ON)

# Check if we should use the SDK or not
if(CMAKE_SYSTEM_NAME STREQUAL "NexNix")
    find_package(NexNixSdk REQUIRED)
else()
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    include_directories(cmake)
endif()

# Enable testing
if(${NNPKG_ENABLE_TESTS} AND NOT ${CMAKE_CROSSCOMPILING})
    enable_testing()
endif()

include(GNUInstallDirs)
include(GettextHelp)
include(SdkCompilerTest)
include(NexTest)

# Find libintl
find_package(Intl)
if(NOT Intl_FOUND)
    message(WARNING "libintl not found, compiling without NLS (multilingual) support")
    set(NNPKG_ENABLE_NLS OFF)
endif()

# Find GNU gettext
gettext_check(GETTEXT_FOUND)
if(NOT GETTEXT_FOUND)
    message(WARNING "GNU gettext not found, compiling without NLS (multilingual) support")
    set(NNPKG_ENABLE_NLS OFF)
else()
    message(STATUS "Found GNU gettext")
endif()

if(NNPKG_ENABLE_NLS)
    # Find the locale directory base
    set(NNPKG_LOCALE_BASE "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/locale/")

    # Check which locales should be enabled
    set(NNPKG_SUPPORTED_LOCALES "en;es" CACHE STRING "Languages supported by nnpkg")
    if(NOT NNPKG_ENABLED_LOCALES)
        set(NNPKG_ENABLED_LOCALES "${NNPKG_SUPPORTED_LOCALES}" CACHE STRING "Languages to use in nnpkg")
    endif()
    # Validate locales
    foreach(locale ${NNPKG_ENABLED_LOCALES})
        # Strip en from list. en_US is the language that the message IDs are in, meaning that we
        # don't need a l10n for that
        if("${locale}" STREQUAL "en")
            list(REMOVE_ITEM NNPKG_ENABLED_LOCALES "en")
        endif()
        list(FIND NNPKG_SUPPORTED_LOCALES ${locale} locale_supported)
        if("${locale_supported}" STREQUAL "-1")
            message(FATAL_ERROR "Locale ${locale} not supported")
        endif()
    endforeach()
endif()

# Set up configuration options
set(NNPKG_CONFFILE_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_SYSCONFDIR}/nnpkg.conf"
    CACHE STRING "Path to nnpkg configuration file")
set(NNPKG_DATABASE_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/nnpkgdb"
    CACHE STRING "Path to nnpkg main database")
set(NNPKG_STRTAB_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LOCALSTATEDIR}/nnpkgstrtab"
    CACHE STRING "Path to nnpkg main string table")
set(NNPKG_INDEX_PATH "/Programs/Index" CACHE STRING "Path to program index")

# See if tests should be enabled
if(${NNPKG_ENABLE_TESTS})
    nextest_enable_tests()
endif()

# Set up debugging C flags if need be
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-O0 -g)
else()
    add_compile_options(-O3 -DNDEBUG)
endif()

# Ensure headers output in binary directory are included
include_directories(${CMAKE_BINARY_DIR})

# Chek for library visibility declarations
check_library_visibility(HAVE_DECLPSEC HAVE_VISIBILITY)

add_subdirectory(libraries)
add_subdirectory(frontend)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/config.in.h ${CMAKE_BINARY_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/data/nnpkg.conf.in ${CMAKE_BINARY_DIR}/nnpkg.conf)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS 
             ${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in
             ${CMAKE_CURRENT_SOURCE_DIR}/data/nnpkg.conf.in)

# Get directory name of nnpkg configuration file
get_filename_component(NNPKG_CONF_DIR ${NNPKG_CONFFILE_PATH} DIRECTORY)

install(FILES ${CMAKE_BINARY_DIR}/nnpkg.conf DESTINATION ${NNPKG_CONF_DIR})
