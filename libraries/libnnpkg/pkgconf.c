/*
    pkgconf.c - contains package configuration file manager
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

/// @file pkgconf.c

#include <assert.h>
#include <libconf.h>
#include <libnex/error.h>
#include <libnex/list.h>
#include <libnex/safemalloc.h>
#include <libnex/unicode.h>
#include <nnpkg/pkg.h>
#include <stdio.h>
#include <string.h>

// Contained in pkgdb.c
void pkgDestroy (const Object_t* obj);

NNPKG_PUBLIC NnpkgPackage_t* PkgReadConf (const char* file)
{
    // Parse file
    ListHead_t* blocks = ConfInit (file);
    if (!blocks)
        return NULL;
    NnpkgPackage_t* pkgOut = calloc_s (sizeof (NnpkgPackage_t));
    if (!pkgOut)
        return NULL;
    pkgOut->deps =
        ListCreate ("NnpkgPackage_t", true, offsetof (NnpkgPackage_t, obj));
    ObjCreate ("NnpkgPackage_t", &pkgOut->obj);
    ObjSetDestroy (&pkgOut->obj, pkgDestroy);
    if (!ListFront (blocks))
    {
        error ("%s: empty package configuaration file", ConfGetFileName());
        ConfFreeParseTree (blocks);
        return NULL;
    }
    if (ListIterate (ListFront (blocks)))
    {
        error ("%s: only one package block supported in a configuration file",
               ConfGetFileName());
        ConfFreeParseTree (blocks);
        return NULL;
    }
    // Iterate through blocks
    int lineNo = 0;
    ConfBlock_t* block = ListEntryData (ListFront (blocks));
    lineNo = block->lineNo;
    if (c32cmp (StrRefGet (block->blockType), U"package") != 0)
    {
        error ("%s:%d: unrecognized block type \"%s\"",
               ConfGetFileName(),
               lineNo,
               UnicodeToHost (StrRefGet (block->blockType)));
        ConfFreeParseTree (blocks);
        return NULL;
    }
    if (!block->blockName)
    {
        error ("%s:%d: block name required for block type \"%s\"",
               ConfGetFileName(),
               lineNo,
               UnicodeToHost (StrRefGet (block->blockType)));
        ConfFreeParseTree (blocks);
        return NULL;
    }
    pkgOut->id = StrRefNew (block->blockName);
    // Go through properties
    ListEntry_t* propEntry = ListFront (block->props);
    while (propEntry)
    {
        ConfProperty_t* prop = ListEntryData (propEntry);
        lineNo = prop->lineNo;
        // Figure out the property type
        if (!c32cmp (StrRefGet (prop->name), U"description"))
        {
            if (prop->nextVal != 1)
            {
                error ("%s:%d: property \"%s\" requires exactly one value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
            if (prop->vals[0].type != DATATYPE_STRING)
            {
                error ("%s:%d: property \"%s\" requires string value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
            pkgOut->description = StrRefNew (prop->vals[0].str);
        }
        else if (!c32cmp (StrRefGet (prop->name), U"prefix"))
        {
            if (prop->nextVal != 1)
            {
                error ("%s:%d: property \"%s\" requires exactly one value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
            if (prop->vals[0].type != DATATYPE_STRING)
            {
                error ("%s:%d: property \"%s\" requires string value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
            pkgOut->prefix = StrRefNew (prop->vals[0].str);
        }
        else if (!c32cmp (StrRefGet (prop->name), U"isDependency"))
        {
            if (prop->nextVal != 1)
            {
                error ("%s:%d: property \"%s\" requires exactly one value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
            if (prop->vals[0].type != DATATYPE_IDENTIFIER)
            {
                error ("%s:%d: property \"%s\" requires boolean value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
            const char32_t* val = StrRefGet (prop->vals[0].id);
            if (!c32cmp (val, U"true"))
                pkgOut->isDependency = true;
            else if (!c32cmp (val, U"false"))
                pkgOut->isDependency = false;
            else
            {
                error ("%s:%d: property \"%s\" requires boolean value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                return NULL;
            }
        }
        else if (!c32cmp (StrRefGet (prop->name), U"dependencies"))
        {
            for (int i = 0; i < prop->nextVal; ++i)
            {
                ConfPropVal_t* propVal = &prop->vals[i];
                lineNo = propVal->lineNo;
                // Sanity checks
                if (propVal->type != DATATYPE_IDENTIFIER)
                {
                    error ("%s:%d: property \"%s\" requires identifier value",
                           ConfGetFileName(),
                           lineNo,
                           UnicodeToHost (StrRefGet (prop->name)));
                    return NULL;
                }
                // Find this package
                // TODO: versioning support
                NnpkgPackage_t* pkg = PkgFindPackage (StrRefGet (propVal->id));
                if (!pkg)
                    return NULL;
                ListAddBack (pkgOut->deps, pkg, 0);
            }
        }
        propEntry = ListIterate (propEntry);
    }
    ConfFreeParseTree (blocks);
    return pkgOut;
}
