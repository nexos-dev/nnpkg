/*
    pkg.h - contains package interface
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

/// @file pkg.h

#ifndef _PKG_H
#define _PKG_H

#include <config.h>
#include <libnex/char32.h>
#include <libnex/list.h>
#include <libnex/object.h>
#include <nnpkg/propdb.h>
#include <stdbool.h>

// Forward declaration of control block to prevent recursive dependencies
// in headers
typedef struct _nnpkgact NnpkgTransCb_t;

// nnpkg configuration structure
typedef struct _nnpkgConf
{
    NnpkgDbLocation_t dbLoc;    ///< Location of package database
} NnpkgMainConf_t;

// Package type
typedef struct _nnpkg
{
    Object_t obj;                  ///< Underlying object of package
    StringRef32_t* id;             ///< ID of this package
    StringRef32_t* description;    ///< Description of package
    StringRef32_t* prefix;         ///< Prefix where files are placed
    bool isDependency;      ///< If this package can be auto-removed when no package
                            ///< is dependent on it
    unsigned short type;    ///< Type of this package. Can be NNPKG_PKG_TYPE_PACKAGE.
                            ///< Meta-packages coming soon :)
    ListHead_t* deps;       ///< List of dependencies
    NnpkgProp_t* prop;      ///< Internal database property
} NnpkgPackage_t;

// Package types
#define NNPKG_PKG_TYPE_PACKAGE 1

// Package database type
typedef struct _nnpkgdb
{
    Object_t obj;               ///< Underlying object of database
    NnpkgPropDb_t* propDb;      ///< Property database underlying this package
    unsigned short type;        ///< Type of this package database. Either source or
                                ///< destination
    unsigned short location;    ///< Location of database. Either local or remote
} NnpkgPackageDb_t;

// Add package transaction
typedef struct _nnpkgtransadd
{
    NnpkgPackage_t* pkg;    ///< Package being added
    const char* pkgConf;    ///< Path of package cofiguration file
} NnpkgTransAdd_t;

// Database types and locations
#define NNPKGDB_TYPE_SOURCE 1
#define NNPKGDB_TYPE_DEST   2

#define NNPKGDB_LOCATION_LOCAL  1
#define NNPKGDB_LOCATION_REMOTE 2

// Function to manipulate a package database

/// Opens up the package database
NNPKG_PUBLIC NnpkgPropDb_t* PkgDbOpen (NnpkgTransCb_t* cb, NnpkgDbLocation_t* dbLoc);

/// Close the package database
NNPKG_PUBLIC void PkgDbClose (NnpkgPropDb_t* db);

/// Adds package to database
NNPKG_PUBLIC bool PkgDbAddPackage (NnpkgTransCb_t* cb,
                                   NnpkgPropDb_t* db,
                                   NnpkgPackage_t* pkg);

/// Finds a package in the database
NNPKG_PUBLIC NnpkgPackage_t* PkgDbFindPackage (NnpkgTransCb_t* cb,
                                               NnpkgPropDb_t* db,
                                               const char32_t* name);

/// Removes a package
NNPKG_PUBLIC bool PkgDbRemovePackage (NnpkgTransCb_t* cb,
                                      NnpkgPropDb_t* db,
                                      NnpkgPackage_t* pkg);

// Package configuration functions

/// Parses configuration of a package configuration file
NNPKG_PUBLIC NnpkgPackage_t* PkgReadConf (NnpkgTransCb_t* cb, const char* file);

// Functions to manage package databases

/// Opens up a package database
NNPKG_PUBLIC bool PkgOpenDb (NnpkgTransCb_t* cb,
                             NnpkgDbLocation_t* dbPath,
                             unsigned short type,
                             unsigned short location);

/// Closes all open databases
NNPKG_PUBLIC void PkgCloseDbs();

/// Adds a package to dest database
NNPKG_PUBLIC bool PkgAddPackage (NnpkgTransCb_t* cb, NnpkgPackage_t* pkg);

/// Removes a package from the dest database
NNPKG_PUBLIC bool PkgRemovePackage (NnpkgTransCb_t* cb, NnpkgPackage_t* pkg);

/// Finds a package in the first available database
NNPKG_PUBLIC NnpkgPackage_t* PkgFindPackage (NnpkgTransCb_t* cb,
                                             const char32_t* name);

/// Parse configuration file for nnpkg
NNPKG_PUBLIC bool PkgParseMainConf (NnpkgTransCb_t* cb, const char* file);

/// Destroys main configuration
NNPKG_PUBLIC void PkgDestroyMainConf();

#endif
