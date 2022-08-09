/*
    propdb.h - contains property database interface
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

/// @file propdb.h

#ifndef _NNPKGDB_H
#define _NNPKGDB_H

#include <config.h>
#include <libintl.h>
#include <libnex/char32.h>
#include <libnex/list.h>
#include <libnex/stringref.h>
#include <nnpkg/transaction.h>
#include <stddef.h>

typedef struct _dbProp propDbProperty_t;

// Package databse type
typedef struct _nnpkgDb
{
    void* memBase;                  // mmap'ed base address of database
    int fd;                         // File descriptor of database
    size_t sz;                      // Size of database
    void* strtabBase;               // Base of mmap'ed string table
    int strtabFd;                   // File descriptor of string table
    size_t strtabSz;                // String table size
    size_t strtabOff;               // Offset pointer to string table
    ListHead_t* propsToAdd;         // Properties that need to be added to database
    ListHead_t* propsToRm;          // Properties to be removed
    propDbProperty_t* allocMark;    // Place to start allocations from
    size_t propsLeft;               // Number of properties after allocation mark
    size_t numFreeProps;            // Number of freee properties in database
    StringRef_t* dbPath;            // Path of database
    StringRef_t* strtabPath;        // Path of string table
} NnpkgPropDb_t;

// Property
typedef struct _nnpkgProp
{
    Object_t obj;           // Object of property
    StringRef32_t* id;      // ID of this property
    unsigned short type;    // Type of property
    void* data;             // Extra data for property
    size_t dataLen;         // Length of extra data
    void* internal;         // Internal property representation
} NnpkgProp_t;

// Property types
#define NNPKG_PROP_TYPE_INVALID 0
#define NNPKG_PROP_TYPE_PKG     1
#define NNPKG_PROP_TYPE_STRING  2

// Size of a property in the database
#define PROPDB_PROP_SIZE 512

// Location structure
typedef struct _dbLoc
{
    StringRef_t* dbPath;
    StringRef_t* strtabPath;
} NnpkgDbLocation_t;

/// Creates a new property database and writes it out to disk
NNPKG_PUBLIC bool PropDbCreate (NnpkgDbLocation_t* dbLoc);

/// Opens up property database from disk
NNPKG_PUBLIC NnpkgPropDb_t* PropDbOpen (NnpkgTransCb_t* cb,
                                        NnpkgDbLocation_t* dbLoc);

/// Closes property database
NNPKG_PUBLIC void PropDbClose (NnpkgPropDb_t* db);

/// Adds a property to the database
NNPKG_PUBLIC bool PropDbAddProp (NnpkgTransCb_t* cb,
                                 NnpkgPropDb_t* db,
                                 NnpkgProp_t* prop);

/// Finds a property in the database
NNPKG_PUBLIC bool PropDbFindProp (NnpkgPropDb_t* db,
                                  const char32_t* name,
                                  NnpkgProp_t* out);

/// Removes a property from the database
NNPKG_PUBLIC bool PropDbRemoveProp (NnpkgTransCb_t* cb,
                                    NnpkgPropDb_t* db,
                                    const NnpkgProp_t* prop);

/// Initializes string table
NNPKG_PUBLIC bool PropDbInitStrtab (const char* path);

/// Opens the string table
NNPKG_PUBLIC bool PropDbOpenStrtab (NnpkgTransCb_t* cb,
                                    NnpkgPropDb_t* db,
                                    const char* fileName);

/// Writes string, returning the index
NNPKG_PUBLIC size_t PropDbAddString (NnpkgPropDb_t* db, const char32_t* s);

/// Finds a string in the database
NNPKG_PUBLIC const char32_t* PropDbGetString (NnpkgPropDb_t* db, size_t idx);

/// Closes the string table
NNPKG_PUBLIC void PropDbCloseStrtab (NnpkgPropDb_t* db);

/// Destroys a property
LIBNEX_PUBLIC void PropDbDestroyProp (const void* data);

#endif
