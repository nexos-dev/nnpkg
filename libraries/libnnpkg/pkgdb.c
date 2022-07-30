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

StringRef_t* pkgGetPkgDb()
{
    const char* path = NNPKG_DATABASE_PATH;
    size_t pathLen = strlen (path);
    size_t dbLen = strlen (NNPKG_DATABASE);
    size_t nameLen = pathLen + dbLen + 2;
    char* s = malloc_s (nameLen);
    if (!s)
        return NULL;
    strlcpy (s, path, nameLen);
    // Add trailing '/' if need be
    if (path[pathLen] != '/')
    {
        strcat (s, "/");
        ++nameLen;
    }
    strlcat (s, NNPKG_DATABASE, dbLen + nameLen);
    return StrRefCreate (s);
}

StringRef_t* pkgDbGetStrtab()
{
    const char* path = NNPKG_DATABASE_PATH;
    size_t pathLen = strlen (path);
    size_t dbLen = strlen (NNPKG_STRTAB);
    size_t nameLen = pathLen + dbLen + 2;
    char* s = malloc_s (nameLen);
    if (!s)
        return NULL;
    if (strlcpy (s, path, nameLen) >= nameLen)
        warn (_ ("path truncation detected"));
    // Add trailing '/' if need be
    if (path[pathLen] != '/')
    {
        strcat (s, "/");
        ++nameLen;
    }
    if (strlcat (s, NNPKG_STRTAB, dbLen + nameLen) >= (dbLen + nameLen))
    {
        error (_ ("path truncation detected"));
        return NULL;
    }
    return StrRefCreate (s);
}

NNPKG_PUBLIC bool PkgDbGetPath (NnpkgDbLocation_t* out)
{
    out->dbPath = pkgGetPkgDb();
    if (!out->dbPath)
        return false;
    out->strtabPath = pkgDbGetStrtab();
    return true;
}

NNPKG_PUBLIC bool PkgDbCreate()
{
    NnpkgDbLocation_t dbLoc;
    if (!PkgDbGetPath (&dbLoc))
        return false;
    bool res = PropDbCreate (&dbLoc);
    StrRefDestroy (dbLoc.dbPath);
    StrRefDestroy (dbLoc.strtabPath);
    return res;
}

// Destroys a package
void pkgDestroy (const Object_t* obj)
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

NNPKG_PUBLIC NnpkgPropDb_t* PkgDbOpen (NnpkgDbLocation_t* dbLoc)
{
    NnpkgPropDb_t* db = PropDbOpen (dbLoc);
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

NNPKG_PUBLIC bool PkgDbAddPackage (NnpkgPropDb_t* db, NnpkgPackage_t* pkg)
{
    // Ensure conflicting ID doesn't exist
    NnpkgProp_t propToCheck;
    if (ListFindEntryBy (db->propsToAdd, StrRefGet (pkg->id)) ||
        PropDbFindProp (db, StrRefGet (pkg->id), &propToCheck))
    {
        error ("package \"%s\" already exists in database",
               UnicodeToHost (StrRefGet (pkg->id)));
        return false;
    }
    // Initialize new property
    NnpkgProp_t* prop = malloc_s (sizeof (NnpkgProp_t));
    if (!prop)
        return false;
    prop->id = StrRefNew (pkg->id);
    prop->type = NNPKG_PROP_TYPE_PKG;
    propDbPkg_t* internalPkg = calloc_s (sizeof (propDbPkg_t));
    if (!internalPkg)
    {
        free (prop);
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
    PropDbAddProp (db, prop);
    // Point to prop in pkg
    pkg->prop = ObjGetContainer (ObjRef (&prop->obj), NnpkgProp_t, obj);
    return true;
}

NNPKG_PUBLIC NnpkgPackage_t* PkgDbFindPackage (NnpkgPropDb_t* db,
                                               const char32_t* name)
{
    // Find property
    NnpkgProp_t* prop = malloc_s (sizeof (NnpkgProp_t));
    if (!prop)
        return (NnpkgPackage_t*) -1;
    if (!PropDbFindProp (db, name, prop))
    {
        error ("package \"%s\" does not exist in database", UnicodeToHost (name));
        free (prop);
        return NULL;
    }
    // Initialize package
    NnpkgPackage_t* pkg = malloc_s (sizeof (NnpkgPackage_t));
    if (!pkg)
    {
        ObjDestroy (&prop->obj);
        return (NnpkgPackage_t*) -1;
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
        error ("out of memory");
        ObjDestroy (&prop->obj);
        return (NnpkgPackage_t*) -1;
    }
    // Add each dependency
    int i = 0;
    while (intProp->deps[i].idx)
    {
        // Find package
        NnpkgPackage_t* dep =
            PkgDbFindPackage (db, PropDbGetString (db, intProp->deps[i].idx));
        if (!dep)
            return NULL;
        ListAddBack (pkg->deps, dep, 0);
        ++i;
    }
    ObjCreate ("NnpkgPackage_t", &pkg->obj);
    ObjSetDestroy (&pkg->obj, pkgDestroy);
    return pkg;
}

NNPKG_PUBLIC bool PkgDbRemovePackage (NnpkgPropDb_t* db, NnpkgPackage_t* pkg)
{
    assert (pkg->prop);
    return PropDbRemoveProp (db, pkg->prop);
}
