/*
    fsstuff.h - contains file system management functions
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

#ifndef _FSSTUFF_H
#define _FSSTUFF_H

#include <config.h>
#include <libnex/list.h>
#include <libnex/stringref.h>
#include <nnpkg/pkg.h>
#include <nnpkg/transaction.h>

/// Index entry structure
typedef struct _idxentry
{
    StringRef_t* srcFile;     /// Source file of entry
    StringRef_t* destFile;    /// Destination file of entry
} NnpkgIdxEntry_t;

/// Collects index entries
NNPKG_PUBLIC ListHead_t* IdxCollectEntries (NnpkgTransCb_t* cb, NnpkgPackage_t* pkg);

/// Write stuff to index
NNPKG_PUBLIC bool IdxWriteIndex (NnpkgTransCb_t* cb, ListHead_t* idxList);

#endif
