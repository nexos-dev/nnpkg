/*
    config.in.h - contains configuration template for nnpkg
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
*/

// clang-format off

#ifndef _CONFIG_H
#define _CONFIG_H

#include <libintl.h>

#define NNPKG_VERSION "@CMAKE_PROJECT_VERSION@"
#define NNPKG_LOCALE_BASE "@NNPKG_LOCALE_BASE@"
#cmakedefine NNPKG_ENABLE_NLS
#cmakedefine HAVE_VISIBILITY
#cmakedefine HAVE_DECLSPEC_EXPORT

// Get visibility stuff right
#ifdef HAVE_VISIBILITY
#define NNPKG_PUBLIC __attribute__ ((visibility ("default")))
#elif defined HAVE_DECLSPEC_EXPORT
#ifdef IN_LIBNNPKG
#define NNPKG_PUBLIC __declspec(dllexport)
#else
#define NNPKG_PUBLIC __declspec(dllimport)
#endif
#else
#define NNPKG_PUBLIC
#endif

#define NNPKG_DATABASE "nnpkgdb"
#define NNPKG_DATABASE_PATH "@NNPKG_DATABASE_PATH@"
#define NNPKG_STRTAB "nnpkgstr"

// i18n stuff
#ifdef NNPKG_ENABLE_NLS
#define _(s) (gettext (s))
#else
#define _(s) (s)
#endif

#endif
