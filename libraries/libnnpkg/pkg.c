/*
    pkg.c - contains package database manager
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

/// @file pkg.c

#include <assert.h>
#include <libnex/list.h>
#include <libnex/safemalloc.h>
#include <nnpkg/pkg.h>

static ListHead_t* pkgDbs = NULL;          // List of package databases
static NnpkgPackageDb_t* destDb = NULL;    // Destination database

void pkgDbDestroy (const Object_t* obj)
{
    NnpkgPackageDb_t* pkgDb = ObjGetContainer (obj, NnpkgPackageDb_t, obj);
    PkgDbClose (pkgDb->propDb);
    free (pkgDb);
}

NNPKG_PUBLIC bool PkgOpenDb (NnpkgTransCb_t* cb,
                             NnpkgDbLocation_t* dbPath,
                             unsigned short type,
                             unsigned short location)
{
    NnpkgPackageDb_t* pkgDb = malloc_s (sizeof (NnpkgPackageDb_t));
    if (!pkgDb)
    {
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    ObjCreate ("NnpkgPackageDb_t", &pkgDb->obj);
    ObjSetDestroy (&pkgDb->obj, pkgDbDestroy);
    assert (type && type <= NNPKGDB_TYPE_DEST);
    assert (location && location <= NNPKGDB_LOCATION_REMOTE);
    pkgDb->type = type;
    pkgDb->location = location;
    pkgDb->propDb = PkgDbOpen (cb, dbPath);
    if (!pkgDb->propDb)
    {
        free (pkgDb);
        return false;
    }
    // See if this is the destination database
    if (type == NNPKGDB_TYPE_DEST)
    {
        assert (!destDb);
        destDb = pkgDb;
    }
    // Add to list
    if (!pkgDbs)
    {
        pkgDbs =
            ListCreate ("NnpkgPackageDb_t", true, offsetof (NnpkgPackageDb_t, obj));
        cb->pkgDbs = pkgDbs;
    }
    ListAddBack (pkgDbs, pkgDb, 0);
    return true;
}

NNPKG_PUBLIC bool PkgAddPackage (NnpkgTransCb_t* cb, NnpkgPackage_t* pkg)
{
    assert (destDb);
    return PkgDbAddPackage (cb, destDb->propDb, pkg);
}

NNPKG_PUBLIC bool PkgRemovePackage (NnpkgTransCb_t* cb, NnpkgPackage_t* pkg)
{
    assert (destDb);
    return PkgDbRemovePackage (cb, destDb->propDb, pkg);
}

NNPKG_PUBLIC NnpkgPackage_t* PkgFindPackage (NnpkgTransCb_t* cb,
                                             const char32_t* name)
{
    assert (pkgDbs);
    // Iterate through all databases
    ListEntry_t* curEntry = ListFront (pkgDbs);
    while (curEntry)
    {
        NnpkgPackageDb_t* pkgDb = ListEntryData (curEntry);
        NnpkgPackage_t* pkg = PkgDbFindPackage (cb, pkgDb->propDb, name);
        if (pkg)
            return pkg;
        curEntry = ListIterate (curEntry);
    }
    return NULL;
}

NNPKG_PUBLIC void PkgCloseDbs()
{
    ListDestroy (pkgDbs);
    destDb = NULL;
    pkgDbs = NULL;
}
