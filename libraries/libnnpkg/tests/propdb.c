/*
    propdb.c - contains database test driver
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

/// @file propdb.c

#include <stdio.h>
#define NEXTEST_NAME "propdb"
#include <errno.h>
#include <libnex/error.h>
#include <libnex/progname.h>
#include <locale.h>
#include <nextest.h>
#include <nnpkg/pkg.h>
#include <nnpkg/propdb.h>
#include <nnpkg/transaction.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main (int argc, char** argv)
{
    setprogname (argv[0]);
    setlocale (LC_ALL, "");
    bindtextdomain ("libnnpkg", NNPKG_LOCALE_BASE);
    // Remove old database
    NnpkgTransCb_t cb;
    TEST_BOOL (PkgParseMainConf (&cb, NNPKG_CONFFILE_PATH),
               "PkgParseMainConf success");
    NnpkgDbLocation_t* dbLoc = &cb.conf->dbLoc;
    StringRef_t* pkgDb = dbLoc->dbPath;
    StringRef_t* strtab = dbLoc->strtabPath;
    if (unlink (StrRefGet (pkgDb)) == -1 && errno != ENOENT)
    {
        error ("%s: %s", pkgDb, strerror (errno));
        return 1;
    }
    if (unlink (StrRefGet (strtab)) == -1 && errno != ENOENT)
    {
        error ("%s: %s", strtab, strerror (errno));
        return 1;
    }
    // Initialize database
    PropDbCreate (dbLoc);
    // Try opening the database
    NnpkgPropDb_t* db = PropDbOpen (&cb, dbLoc);
    TEST_BOOL (db, "PropDbOpen() success status");
    // Test that database header is intact
    TEST ((*((uint64_t*) db->memBase)),
          0x7878807571686600,
          "PropDbOpen() database integrity");
    // Test that database is locked
    TEST (PropDbOpen (&cb, dbLoc), NULL, "property database is locked");
    PropDbClose (db);
    // Test that database is actually unlocked
    db = PropDbOpen (&cb, dbLoc);
    TEST_BOOL (db, "PropDbClose() unlocking");
    PropDbClose (db);
    db = PropDbOpen (&cb, dbLoc);
    TEST_BOOL (db, "PropDbOpen() success");
    // Add a package to it
    // NOTE: There are no leaks here, even though we re-malloc prop a lot
    // This is simply because PropDbClose() frees prop
    NnpkgProp_t* prop = calloc (sizeof (NnpkgProp_t), 1);
    prop->id = StrRefCreate (U"testPkg");
    StrRefNoFree (prop->id);
    prop->type = NNPKG_PROP_TYPE_PKG;
    prop->data = strdup ("test data");
    prop->dataLen = strlen (prop->data);
    PropDbAddProp (&cb, db, prop);
    // Commit to database
    PropDbClose (db);
    db = PropDbOpen (&cb, dbLoc);
    TEST_BOOL (db, "PropDbOpen() success");
    // Test finding it
    prop = calloc (sizeof (NnpkgProp_t), 1);
    memset (prop, 0, sizeof (NnpkgProp_t));
    TEST_BOOL (PropDbFindProp (db, U"testPkg", prop), "PropDbFindProp() success");
    TEST (prop->type, NNPKG_PROP_TYPE_PKG, "PropDbFindProp() output validity 1");
    TEST_BOOL (!c32cmp (StrRefGet (prop->id), U"testPkg"),
               "PropDbFindProp() output validity 2");
    TEST_BOOL (!strcmp (prop->data, "test data"),
               "PropDbFindProp() output validity 3");
    // Test removing it
    TEST_BOOL (PropDbRemoveProp (&cb, db, prop), "PropDbRemoveProp() success");
    // Commit to database
    PropDbClose (db);
    db = PropDbOpen (&cb, dbLoc);
    TEST_BOOL (db, "PropDbOpen() success");
    // Ensure we can't find property anymore
    TEST_BOOL (!PropDbFindProp (db, U"testPkg", prop), "PropDbRemoveProp()");
    // Add a new property
    prop = calloc (sizeof (NnpkgProp_t), 1);
    prop->id = StrRefCreate (U"testPkg");
    StrRefNoFree (prop->id);
    prop->type = NNPKG_PROP_TYPE_PKG;
    prop->data = strdup ("test data");
    prop->dataLen = strlen (prop->data);
    NnpkgProp_t* prop2 = calloc (sizeof (NnpkgProp_t), 1);
    prop2->id = StrRefCreate (U"test2Pkg");
    StrRefNoFree (prop2->id);
    prop2->type = NNPKG_PROP_TYPE_PKG;
    prop2->data = strdup ("test data");
    prop2->dataLen = strlen (prop2->data);
    PropDbAddProp (&cb, db, prop);
    PropDbAddProp (&cb, db, prop2);
    PropDbClose (db);
    db = PropDbOpen (&cb, dbLoc);
    // Ensure property is at correct location
    prop = malloc (sizeof (NnpkgProp_t));
    TEST_BOOL (PropDbFindProp (db, U"testPkg", prop), "PropDbFindProp() success");
    TEST (prop->internal, db->memBase + 28, "PropDbAddProp() on reused entry");
    PropDbClose (db);
    ObjDeRef (&prop->obj);
    StrRefDestroy (pkgDb);
    StrRefDestroy (strtab);
    return 0;
}
