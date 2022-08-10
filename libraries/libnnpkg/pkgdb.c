/*
    pkgdb.c - contains package database abstraction
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

/// @file pkgdb.c

#include <assert.h>
#include <libnex/error.h>
#include <libnex/safemalloc.h>
#include <libnex/safestring.h>
#include <libnex/unicode.h>
#include <nnpkg/pkg.h>
#include <nnpkg/propdb.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Serialized dependency
typedef struct _dbdep
{
    uint32_t idx;      ///< String index of name
    uint8_t verOp;     ///< Version operator. See pkg.h. Unused for now
    uint8_t ver[3];    ///< Version being affected by verOp.
                       ///< ver[0] contains major, ver[1] contains minor, and ver[2]
                       ///< contains revision. Unused for now
} __attribute__ ((packed)) propDbPkgDep_t;

// Package serialized representation
typedef struct _dbpkg
{
    uint32_t description;    ///< String table index of description
    uint32_t prefix;         ///< String tabe index of prefix
    uint16_t pkgType;        ///< Type of this package. See pkg.h for valid types
    uint8_t isDependency;    ///< If this package is auto-removable
    uint8_t resvd[9];
    propDbPkgDep_t deps[60];
} __attribute__ ((packed)) propDbPkg_t;

// Destroys a package
void pkgDestroy (const Object_t* obj)
{
    NnpkgPackage_t* pkg = ObjGetContainer (obj, NnpkgPackage_t, obj);
    if (pkg->id)
        StrRefDestroy (pkg->id);
    if (pkg->description)
        StrRefDestroy (pkg->description);
    if (pkg->prefix)
        StrRefDestroy (pkg->prefix);
    if (pkg->prop)
        ObjDeRef (&pkg->prop->obj);
    // Destroy dependencies
    if (pkg->deps)
        ListDestroy (pkg->deps);
    free (pkg);
}

NNPKG_PUBLIC NnpkgPropDb_t* PkgDbOpen (NnpkgTransCb_t* cb, NnpkgDbLocation_t* dbLoc)
{
    NnpkgPropDb_t* db = PropDbOpen (cb, dbLoc);
    if (!db)
        return NULL;
    db->strtabPath = StrRefNew (dbLoc->strtabPath);
    db->dbPath = StrRefNew (dbLoc->dbPath);
    return db;
}

NNPKG_PUBLIC void PkgDbClose (NnpkgPropDb_t* db)
{
    StrRefDestroy (db->strtabPath);
    StrRefDestroy (db->dbPath);
    PropDbClose (db);
}

NNPKG_PUBLIC bool PkgDbAddPackage (NnpkgTransCb_t* cb,
                                   NnpkgPropDb_t* db,
                                   NnpkgPackage_t* pkg)
{
    // Ensure conflicting ID doesn't exist
    NnpkgProp_t propToCheck;
    memset (&propToCheck, 0, sizeof (NnpkgProp_t));
    if (ListFindEntryBy (db->propsToAdd, StrRefGet (pkg->id)) ||
        PropDbFindProp (db, StrRefGet (pkg->id), &propToCheck))
    {
        if (propToCheck.id)
            StrRefDestroy (propToCheck.id);
        cb->error = NNPKG_ERR_PKG_EXIST;
        cb->errHint[0] = StrRefNew (pkg->id);
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    // Initialize new property
    NnpkgProp_t* prop = malloc_s (sizeof (NnpkgProp_t));
    if (!prop)
    {
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    prop->id = StrRefNew (pkg->id);
    prop->type = NNPKG_PROP_TYPE_PKG;
    propDbPkg_t* internalPkg = calloc_s (sizeof (propDbPkg_t));
    if (!internalPkg)
    {
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    internalPkg->isDependency = pkg->isDependency;
    internalPkg->pkgType = pkg->type;
    internalPkg->description = PropDbAddString (db, StrRefGet (pkg->description));
    internalPkg->prefix = PropDbAddString (db, StrRefGet (pkg->prefix));
    // Set up dependency info. Note that we don't automatically add dependencies to
    // database
    int i = 0;
    ListEntry_t* depEntry = ListFront (pkg->deps);
    while (depEntry)
    {
        NnpkgPackage_t* dep = ListEntryData (depEntry);
        internalPkg->deps[i].idx = PropDbAddString (db, StrRefGet (dep->id));
        depEntry = ListIterate (depEntry);
        ++i;
    }
    prop->data = internalPkg;
    prop->dataLen = sizeof (propDbPkg_t);
    // Add it to database
    if (!PropDbAddProp (cb, db, prop))
        return false;
    // Point to prop in pkg
    pkg->prop = ObjGetContainer (ObjRef (&prop->obj), NnpkgProp_t, obj);
    return true;
}

NnpkgPackage_t* pkgDbFindPackage (NnpkgTransCb_t* cb,
                                  NnpkgPropDb_t* db,
                                  const char32_t* name,
                                  bool findingDep)
{
    // Find property
    NnpkgProp_t* prop = malloc_s (sizeof (NnpkgProp_t));
    if (!prop)
    {
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return NULL;
    }
    if (!PropDbFindProp (db, name, prop))
    {
        if (!findingDep)
        {
            cb->error = NNPKG_ERR_PKG_NO_EXIST;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        }
        free (prop);
        return NULL;
    }
    // Initialize package
    NnpkgPackage_t* pkg = malloc_s (sizeof (NnpkgPackage_t));
    if (!pkg)
    {
        ObjDestroy (&prop->obj);
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return NULL;
    }
    pkg->id = StrRefNew (prop->id);
    // Get internal property representation
    propDbPkg_t* intProp = prop->data;
    pkg->description = StrRefCreate (PropDbGetString (db, intProp->description));
    StrRefNoFree (pkg->description);
    pkg->prefix = StrRefCreate (PropDbGetString (db, intProp->prefix));
    StrRefNoFree (pkg->prefix);
    pkg->type = intProp->pkgType;
    pkg->isDependency = intProp->isDependency;
    pkg->prop = prop;
    pkg->deps = ListCreate ("NnpkgPackage_t", true, offsetof (NnpkgPackage_t, obj));
    if (!pkg->deps)
    {
        ObjDestroy (&prop->obj);
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return NULL;
    }
    // Add each dependency
    int i = 0;
    while (intProp->deps[i].idx)
    {
        // Find package
        NnpkgPackage_t* dep =
            pkgDbFindPackage (cb,
                              db,
                              PropDbGetString (db, intProp->deps[i].idx),
                              true);
        // If we can't find the package, than we return -1 to indicate a broken
        // dependency As we go up the recursion chain, we will keep returning -1 to
        // prevent overwriting diagnostic info.
        // The actual interface will normalize -1 to NULL for consistency's sake
        if (!dep)
        {
            ObjDestroy (&prop->obj);
            ListDestroy (pkg->deps);
            cb->error = NNPKG_ERR_BROKEN_DEP;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            cb->errHint[0] = pkg->id;
            cb->errHint[1] =
                StrRefCreate (PropDbGetString (db, intProp->deps[i].idx));
            StrRefNoFree (cb->errHint[1]);
            return (NnpkgPackage_t*) -1;
        }
        else if (dep == (NnpkgPackage_t*) -1)
        {
            ObjDestroy (&prop->obj);
            ListDestroy (pkg->deps);
            return (NnpkgPackage_t*) -1;
        }
        ListAddBack (pkg->deps, dep, 0);
        ++i;
    }
    ObjCreate ("NnpkgPackage_t", &pkg->obj);
    ObjSetDestroy (&pkg->obj, pkgDestroy);
    return pkg;
}

NNPKG_PUBLIC NnpkgPackage_t* PkgDbFindPackage (NnpkgTransCb_t* cb,
                                               NnpkgPropDb_t* db,
                                               const char32_t* name)
{
    NnpkgPackage_t* pkg = pkgDbFindPackage (cb, db, name, false);
    // Normalize -1 return value to NULL, as error info is in the control block
    return (pkg == (NnpkgPackage_t*) -1) ? NULL : pkg;
}

NNPKG_PUBLIC bool PkgDbRemovePackage (NnpkgTransCb_t* cb,
                                      NnpkgPropDb_t* db,
                                      NnpkgPackage_t* pkg)
{
    assert (pkg->prop);
    return PropDbRemoveProp (cb, db, pkg->prop);
}
