/*
    pkg.c - contains package database test driver
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

/// @file pkg.c

#include <stdio.h>
#define NEXTEST_NAME "pkgdb"
#include <errno.h>
#ifdef NNPKG_ENABLE_NLS
#include <libintl.h>
#endif
#include <libnex/error.h>
#include <libnex/progname.h>
#include <libnex/safemalloc.h>
#include <locale.h>
#include <nextest.h>
#include <nnpkg/pkg.h>
#include <string.h>
#include <unistd.h>

void progHandler (NnpkgTransCb_t* cb, int state)
{
    printf ("%d\n", cb->error);
}

// Destroys a package
static pkgDestroy (const Object_t* obj)
{
    NnpkgPackage_t* pkg = ObjGetContainer (obj, NnpkgPackage_t, obj);
    StrRefDestroy (pkg->id);
    if (pkg->description)
        StrRefDestroy (pkg->description);
    if (pkg->prefix)
        StrRefDestroy (pkg->prefix);
    if (pkg->prop)
        ObjDeRef (&pkg->prop->obj);
    // Destroy dependencies
    ListDestroy (pkg->deps);
    free (pkg);
}

int main (int argc, char** argv)
{
    setprogname (argv[0]);
#ifdef NNPKG_ENABLE_NLS
    setlocale (LC_ALL, "");
    bindtextdomain ("libnnpkg", NNPKG_LOCALE_BASE);
#endif
    // Remove old database(s)
    NnpkgTransCb_t cb;
    cb.progress = progHandler;
    TEST_BOOL (PkgParseMainConf (&cb, NNPKG_CONFFILE_PATH),
               "PkgParseMainConf success");
    NnpkgDbLocation_t* dbLoc = &cb.conf->dbLoc;
    if (unlink (StrRefGet (dbLoc->dbPath)) == -1 && errno != ENOENT)
    {
        error ("%s: %s", dbLoc->dbPath, strerror (errno));
        return 1;
    }
    if (unlink (StrRefGet (dbLoc->strtabPath)) == -1 && errno != ENOENT)
    {
        error ("%s: %s", dbLoc->strtabPath, strerror (errno));
        return 1;
    }
    TEST_BOOL (PropDbCreate (dbLoc), "PkgDbCreate() success");
    TEST_BOOL (PkgOpenDb (&cb, dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL),
               "PkgDbOpen() success");
    NnpkgPackage_t* pkg = calloc_s (sizeof (NnpkgPackage_t));
    if (!pkg)
        return 1;
    pkg->id = StrRefCreate (U"pkgtest");
    StrRefNoFree (pkg->id);
    pkg->description = StrRefCreate (U"This is a test package that does nothing");
    StrRefNoFree (pkg->description);
    pkg->isDependency = false;
    ObjCreate ("NnpkgPackage_t", &pkg->obj);
    ObjSetDestroy (&pkg->obj, pkgDestroy);
    pkg->prefix = StrRefCreate (U"Package prefix");
    StrRefNoFree (pkg->prefix);
    pkg->type = NNPKG_PKG_TYPE_PACKAGE;
    pkg->deps = ListCreate ("NnpkgPackage_t", true, offsetof (NnpkgPackage_t, obj));
    PkgAddPackage (&cb, pkg);
    NnpkgPackage_t* pkg2 = calloc_s (sizeof (NnpkgPackage_t));
    if (!pkg2)
        return 1;
    pkg2->id = StrRefCreate (U"pkgtest2");
    StrRefNoFree (pkg2->id);
    pkg2->description = StrRefCreate (U"This is a test package that does nothing");
    StrRefNoFree (pkg2->description);
    pkg2->isDependency = false;
    ObjCreate ("NnpkgPackage_t", &pkg2->obj);
    ObjSetDestroy (&pkg2->obj, pkgDestroy);
    pkg2->prefix = StrRefCreate (U"Package prefix");
    StrRefNoFree (pkg2->prefix);
    pkg2->type = NNPKG_PKG_TYPE_PACKAGE;
    pkg2->deps = ListCreate ("NnpkgPackage_t", true, offsetof (NnpkgPackage_t, obj));
    PkgAddPackage (&cb, pkg2);
    NnpkgPackage_t* pkg3 = calloc_s (sizeof (NnpkgPackage_t));
    if (!pkg3)
        return 1;
    pkg3->id = StrRefCreate (U"pkgtest3");
    StrRefNoFree (pkg3->id);
    pkg3->description = StrRefCreate (U"This is a test package that does nothing");
    StrRefNoFree (pkg3->description);
    pkg3->isDependency = false;
    ObjCreate ("NnpkgPackage_t", &pkg3->obj);
    ObjSetDestroy (&pkg3->obj, pkgDestroy);
    pkg3->prefix = StrRefCreate (U"Package prefix");
    StrRefNoFree (pkg3->prefix);
    pkg3->type = NNPKG_PKG_TYPE_PACKAGE;
    pkg3->deps = ListCreate ("NnpkgPackage_t", true, offsetof (NnpkgPackage_t, obj));
    ListAddBack (pkg3->deps,
                 ObjGetContainer (ObjRef (&pkg2->obj), NnpkgPackage_t, obj),
                 0);
    ListAddBack (pkg3->deps,
                 ObjGetContainer (ObjRef (&pkg->obj), NnpkgPackage_t, obj),
                 0);
    PkgAddPackage (&cb, pkg3);
    PkgCloseDbs();
    ObjDestroy (&pkg->obj);
    ObjDestroy (&pkg2->obj);
    ObjDestroy (&pkg3->obj);
    TEST_BOOL (PkgOpenDb (&cb, dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL),
               "PkgOpenDb() success");
    pkg2 = PkgFindPackage (&cb, U"pkgtest3");
    TEST_BOOL (pkg2, "PkgDbFindPackage() success");
    TEST_BOOL (pkg2 != (NnpkgPackage_t*) -1, "PkgDbFindPackage() success 2");
    TEST_BOOL (!c32cmp (StrRefGet (pkg2->id), U"pkgtest3"),
               "PkgDbFindPackage() validity");
    TEST_BOOL (!c32cmp (StrRefGet (pkg2->description),
                        U"This is a test package that does nothing"),
               "PkgDbFindPackage() validity 2")
    TEST (pkg2->type, NNPKG_PKG_TYPE_PACKAGE, "PkgDbFindPackage() validity 3");
    pkg3 = ListEntryData (ListFront (pkg2->deps));
    TEST_BOOL (!c32cmp (StrRefGet (pkg3->id), U"pkgtest2"),
               "PkgDbFindPackage() validity 4");
    pkg3 = ListEntryData (pkg2->deps->front->next);
    TEST_BOOL (!c32cmp (StrRefGet (pkg3->id), U"pkgtest"),
               "PkgDbFindPackage() validity 5");
    ObjDeRef (&pkg2->obj);
    PkgCloseDbs();
    TEST_BOOL (PkgOpenDb (&cb, dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL),
               "PkgOpenDb() success");
    pkg2 = PkgFindPackage (&cb, U"pkgtest");
    TEST_BOOL (PkgRemovePackage (&cb, pkg2), "PkgDbRemovePackage success");
    PkgCloseDbs();
    PkgOpenDb (&cb, dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL);
    pkg2 = PkgFindPackage (&cb, U"pkgtest");
    TEST_BOOL (!pkg2, "PkgDbRemovePackage() validity");
    PkgCloseDbs();
    StrRefDestroy (dbLoc->dbPath);
    StrRefDestroy (dbLoc->strtabPath);
    return 0;
}
