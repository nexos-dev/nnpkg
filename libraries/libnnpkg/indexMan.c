/*
    indexMan.c - manages index
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

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <libnex/base.h>
#include <libnex/safemalloc.h>
#include <libnex/unicode.h>
#include <nnpkg/fsstuff.h>
#include <nnpkg/pkg.h>
#include <nnpkg/transaction.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Returns a concatenated path
static inline StringRef32_t* makeCatedPath (char32_t* str1, char32_t* str2)
{
    size_t len = c32len (str1) + c32len (str2) + 2;
    char32_t* str = malloc_s ((len) * sizeof (char32_t));
    if (!str)
        return NULL;
    c32lcpy (str, str1, len);
    c32lcat (str, U"/", len);
    c32lcat (str, str2, len);
    return StrRefCreate (str);
}

// Returns a concatenated path, converting second component to UTF-32
static inline StringRef32_t* makeCatedPathFromSys (char32_t* str1, char* str2)
{
    size_t str2len = strlen (str2);
    char32_t* ustr = malloc_s ((str2len + 1) * sizeof (char32_t));
    if (!ustr)
        return NULL;
    mbstate_t mbstate = {0};
    mbstoc32s (ustr, str2, (str2len + 1) * sizeof (char32_t), str2len + 1, &mbstate);
    StringRef32_t* retRef = makeCatedPath (str1, ustr);
    free (ustr);
    return retRef;
}

static inline char* makeHostStr (char32_t* ustr)
{
    size_t len = (c32len (ustr) * MB_CUR_MAX) + 1;
    char* out = malloc_s (len);
    if (!out)
        return NULL;
    mbstate_t mbState = {0};
    c32stombs (out, ustr, len, &mbState);
    return out;
}

// Destroys index entries
void idxEntryDestroy (const void* data)
{
    NnpkgIdxEntry_t* ent = (NnpkgIdxEntry_t*) data;
    StrRefDestroy (ent->destFile);
    StrRefDestroy (ent->srcFile);
    free ((void*) data);
}

// Collects index entries
NNPKG_PUBLIC ListHead_t* IdxCollectEntries (NnpkgTransCb_t* cb, NnpkgPackage_t* pkg)
{
    assert (pkg->prefix);
    ListHead_t* idxList = ListCreate ("NnpkgIdxEntry_t", false, 0);
    if (!idxList)
    {
        cb->error = NNPKG_ERR_OOM;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return NULL;
    }
    ListSetDestroy (idxList, idxEntryDestroy);
    // Iterate through directories
    static char32_t* dirs[] =
        {U"bin", U"sbin", U"etc", U"share", U"libexec", U"var", U"lib", U"include"};
    for (int i = 0; i < ARRAY_SIZE (dirs); ++i)
    {
        // Prepare prefixed paths
        StringRef32_t* curDir = makeCatedPath (StrRefGet (pkg->prefix), dirs[i]);
        if (!curDir)
        {
            cb->error = NNPKG_ERR_OOM;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            ListDestroy (idxList);
            return NULL;
        }
        // Iterate through source directory
        DIR* srcDir = opendir (UnicodeToHost (StrRefGet (curDir)));
        if (!srcDir)
        {
            if (errno != ENOENT)
            {
                cb->error = NNPKG_ERR_SYS;
                cb->sysErrno = errno;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                StrRefDestroy (curDir);
                ListDestroy (idxList);
                return NULL;
            }
            else
            {
                StrRefDestroy (curDir);
                continue;
            }
        }
        // Prepare indexed path
        StringRef32_t* idxedPath =
            makeCatedPath (StrRefGet (cb->conf->idxPath), dirs[i]);
        if (!idxedPath)
        {
            cb->error = NNPKG_ERR_OOM;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            ListDestroy (idxList);
            StrRefDestroy (curDir);
            return NULL;
        }
        struct dirent* curDirEnt = readdir (srcDir);
        while (curDirEnt)
        {
            // SKip . and ..
            if (!strcmp (curDirEnt->d_name, ".") ||
                !strcmp (curDirEnt->d_name, ".."))
            {
                curDirEnt = readdir (srcDir);
                continue;
            }
            // Create path in prefix
            StringRef32_t* prefixPath =
                makeCatedPathFromSys (StrRefGet (curDir), curDirEnt->d_name);
            if (!prefixPath)
            {
                cb->error = NNPKG_ERR_OOM;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                StrRefDestroy (curDir);
                StrRefDestroy (idxedPath);
                closedir (srcDir);
                ListDestroy (idxList);
                return NULL;
            }
            // Create path in index
            StringRef32_t* idxPath =
                makeCatedPathFromSys (StrRefGet (idxedPath), curDirEnt->d_name);
            if (!idxPath)
            {
                cb->error = NNPKG_ERR_OOM;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                StrRefDestroy (curDir);
                StrRefDestroy (idxedPath);
                StrRefDestroy (prefixPath);
                closedir (srcDir);
                ListDestroy (idxList);
                return NULL;
            }
            // We have source and destination, prepare entry
            NnpkgIdxEntry_t* idxEntry = malloc_s (sizeof (NnpkgIdxEntry_t));
            if (!idxEntry)
            {
                cb->error = NNPKG_ERR_OOM;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                StrRefDestroy (idxPath);
                StrRefDestroy (prefixPath);
                StrRefDestroy (curDir);
                StrRefDestroy (idxedPath);
                closedir (srcDir);
                ListDestroy (idxList);
                return NULL;
            }
            idxEntry->destFile = idxPath;
            idxEntry->srcFile = prefixPath;
            // Add to list
            ListAddBack (idxList, idxEntry, 0);
            curDirEnt = readdir (srcDir);
        }
        StrRefDestroy (curDir);
        StrRefDestroy (idxedPath);
        closedir (srcDir);
    }
    return idxList;
}

// Write stuff to index
NNPKG_PUBLIC bool IdxWriteIndex (NnpkgTransCb_t* cb, ListHead_t* idxList)
{
    // Iterate through list
    ListEntry_t* entry = ListFront (idxList);
    while (entry)
    {
        NnpkgIdxEntry_t* idxEnt = ListEntryData (entry);
        // Create symbolic link
        char* srcFile = makeHostStr (StrRefGet (idxEnt->srcFile));
        if (!srcFile)
        {
            cb->error = NNPKG_ERR_OOM;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            return NULL;
        }
        char* destFile = makeHostStr (StrRefGet (idxEnt->destFile));
        if (!destFile)
        {
            cb->error = NNPKG_ERR_OOM;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            free (srcFile);
            return NULL;
        }
        if (symlink (srcFile, destFile) == -1 && errno != EEXIST)
        {
            cb->error = NNPKG_ERR_SYS;
            cb->sysErrno = errno;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            free (srcFile);
            free (destFile);
            return NULL;
        }
        free (srcFile);
        free (destFile);
        entry = ListIterate (entry);
    }
    return true;
}
