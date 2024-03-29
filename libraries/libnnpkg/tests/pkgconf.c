/*
    pkgconf.c - contains test suite for package configuration manager
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

/// @file pkgconf.c

#include <stdio.h>
#define NEXTEST_NAME "pkgconf"
#ifdef NNPKG_ENABLE_NLS
#include <libintl.h>
#endif
#include <libnex/progname.h>
#include <libnex/stringref.h>
#include <locale.h>
#include <nextest.h>
#include <nnpkg/pkg.h>

void progHandler (NnpkgTransCb_t* cb, int state)
{
    printf ("%d\n", cb->error);
}

int main (int argc, char** argv)
{
    setprogname (argv[0]);
#ifdef NNPKG_ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ("libnnpkg", NNPKG_LOCALE_BASE);
#endif
    NnpkgTransCb_t cb;
    cb.progress = progHandler;
    TEST_BOOL (PkgParseMainConf (&cb, NNPKG_CONFFILE_PATH),
               "PkgParseMainConf success");
    NnpkgDbLocation_t* dbLoc = &cb.conf->dbLoc;
    TEST_BOOL (PkgOpenDb (&cb, dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL),
               "PkgOpenDb() success");
    NnpkgPackage_t* pkg = PkgReadConf (&cb, "pkgconf.conf");
    TEST_BOOL (pkg, "PkgReadConf() success");
    TEST_BOOL (!c32cmp (StrRefGet (pkg->id), U"test"), "Package validity 1");
    TEST_BOOL (!c32cmp (StrRefGet (pkg->description), U"A test package"),
               "Package validity 2");
    TEST_BOOL (!c32cmp (StrRefGet (pkg->prefix), U"/home/nexos/Programs/Test"),
               "Package validity 3");
    TEST_BOOL (pkg->isDependency, "Package validity 4");
    ObjDestroy (&pkg->obj);
    PkgCloseDbs();
    StrRefDestroy (dbLoc->dbPath);
    StrRefDestroy (dbLoc->strtabPath);
    return 0;
}
