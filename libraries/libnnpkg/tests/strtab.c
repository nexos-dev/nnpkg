/*
    strtab.c - contains database string table driver
    Copyright 2022 The NexNix Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    There should be a copy of the License distributed in a file named
    LICENSE, if not, you may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/// @file strtab.c

#include <stdio.h>
#define NEXTEST_NAME "propdb_strtab"
#include <errno.h>
#include <libintl.h>
#include <libnex/error.h>
#include <libnex/progname.h>
#include <locale.h>
#include <nextest.h>
#include <nnpkg/pkg.h>
#include <nnpkg/propdb.h>
#include <string.h>
#include <unistd.h>

int main (int argc, char** argv)
{
    setprogname (argv[0]);
    setlocale (LC_ALL, "");
    bindtextdomain ("libnnpkg", NNPKG_LOCALE_BASE);
    TEST_BOOL (PkgParseMainConf (NNPKG_CONFFILE_PATH), "PkgParseMainConf success");
    NnpkgDbLocation_t* dbLoc = &PkgGetMainConf()->dbLoc;
    // Remove old table
    StringRef_t* strtab = dbLoc->strtabPath;
    if (unlink (StrRefGet (strtab)) == -1 && errno != ENOENT)
    {
        error ("%s: %s", StrRefGet (strtab), strerror (errno));
        return 1;
    }
    // Create it
    PropDbInitStrtab (StrRefGet (strtab));
    NnpkgPropDb_t propDb;
    TEST_BOOL (PropDbOpenStrtab (&propDb, StrRefGet (strtab)),
               "PropDbOpenStrtab() success");
    // Test writing 2 strings
    size_t idx = PropDbAddString (&propDb, U"Test string");
    TEST_BOOL (!c32cmp (PropDbGetString (&propDb, idx), U"Test string"),
               "PropDbAddString() and PropDbGetString()");

    size_t idx2 = PropDbAddString (&propDb, U"Test string 2");
    TEST_BOOL (!c32cmp (PropDbGetString (&propDb, idx2), U"Test string 2"),
               "PropDbAddString() and PropDbGetString() 2");
    PropDbCloseStrtab (&propDb);
    StrRefDestroy (strtab);
    return 0;
}
