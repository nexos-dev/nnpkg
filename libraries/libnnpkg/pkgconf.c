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

// Configuration line number
static int lineNo = 0;

#define EXPECTING_SETTINGS 1

// What we are expecting
static int expecting = 0;

// Current property being operated on
static StringRef32_t* curProp = NULL;

union val
{
    int64_t numVal;
    StringRef32_t* strVal;
};

// Configuration structure
static NnpkgMainConf_t conf = {0};

NnpkgPackage_t* pkgDbFindPackage (NnpkgTransCb_t* cb,
                                  NnpkgPropDb_t* db,
                                  const char32_t* name,
                                  bool findingDep);

void pkgDestroy (const Object_t* obj);

NNPKG_PUBLIC NnpkgMainConf_t* PkgGetMainConf()
{
    return &conf;
}

bool pkgConfAddProperty (StringRef32_t* newProp,
                         union val* val,
                         bool isStart,
                         int dataType)
{
    if (isStart)
    {
        curProp = newProp;
        return true;
    }
    if (expecting == EXPECTING_SETTINGS)
    {
        // Check if this is the database path
        if (!c32cmp (StrRefGet (curProp), U"packageDb"))
        {
            if (dataType != DATATYPE_STRING)
            {
                error ("%s:%d: property \"packageDb\" requires a string value",
                       ConfGetFileName(),
                       lineNo);
                return false;
            }
            // Convert to host encoding
            size_t hostLen = (c32len (StrRefGet (val->strVal)) * MB_CUR_MAX) + 1;
            char* hostStrtab = malloc_s (hostLen);
            if (!hostStrtab)
                return false;
            mbstate_t mbstate;
            c32stombs (hostStrtab, StrRefGet (val->strVal), hostLen, &mbstate);
            conf.dbLoc.dbPath = StrRefCreate (hostStrtab);
        }
        // Check if this is the string table path
        else if (!c32cmp (StrRefGet (curProp), U"strtab"))
        {
            if (dataType != DATATYPE_STRING)
            {
                error ("%s:%d: property \"strtab\" requires a string value",
                       ConfGetFileName(),
                       lineNo);
                return false;
            }
            // Convert to host encoding
            size_t hostLen = (c32len (StrRefGet (val->strVal)) * MB_CUR_MAX) + 1;
            char* hostStrtab = malloc_s (hostLen);
            if (!hostStrtab)
                return false;
            mbstate_t mbstate;
            c32stombs (hostStrtab, StrRefGet (val->strVal), hostLen, &mbstate);
            conf.dbLoc.strtabPath = StrRefCreate (hostStrtab);
        }
        else
        {
            error ("%s:%d property \"%s\" unrecognized",
                   ConfGetFileName(),
                   lineNo,
                   UnicodeToHost (StrRefGet (curProp)));
            return false;
        }
    }
    else
        assert (!"Invalid expected block");
    return true;
}

// Parse configuration file for nnpkg
NNPKG_PUBLIC bool PkgParseMainConf (NnpkgTransCb_t* cb, const char* file)
{
    ListHead_t* blocks = ConfInit (file);
    if (!blocks)
    {
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    // Parse blocks
    ListEntry_t* curEntry = ListFront (blocks);
    while (curEntry)
    {
        ConfBlock_t* block = ListEntryData (curEntry);
        lineNo = block->lineNo;
        // Figure out block type
        if (!c32cmp (StrRefGet (block->blockType), U"settings"))
        {
            // Ensure there is no block name
            if (block->blockName)
            {
                error ("%s:%d: block type \"settings\" does not accept a name",
                       ConfGetFileName(),
                       lineNo);
                ConfFreeParseTree (blocks);
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                return false;
            }
            expecting = EXPECTING_SETTINGS;
        }
        else
        {
            error ("%s:%d: invalid block type %s specified",
                   ConfGetFileName(),
                   lineNo,
                   UnicodeToHost (StrRefGet (block->blockType)));
            ConfFreeParseTree (blocks);
            cb->error = NNPKG_ERR_SYNTAX_ERR;
            TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
            return false;
        }
        // Apply the properties
        ListHead_t* propsList = block->props;
        ListEntry_t* propEntry = ListFront (propsList);
        while (propEntry)
        {
            ConfProperty_t* curProp = ListEntryData (propEntry);
            lineNo = curProp->lineNo;
            // Start a new property
            if (!pkgConfAddProperty (curProp->name, NULL, true, 0))
            {
                ConfFreeParseTree (blocks);
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                return false;
            }
            // Add all the actual values
            for (int i = 0; i < curProp->nextVal; ++i)
            {
                lineNo = curProp->lineNo;
                // Obtain value
                union val val;
                if (curProp->vals[i].type == DATATYPE_IDENTIFIER ||
                    curProp->vals[i].type == DATATYPE_STRING)
                {
                    val.strVal = curProp->vals[i].str;
                }
                else
                    val.numVal = curProp->vals[i].numVal;
                if (!pkgConfAddProperty (NULL, &val, false, curProp->vals[i].type))
                {
                    ConfFreeParseTree (blocks);
                    cb->error = NNPKG_ERR_SYNTAX_ERR;
                    TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                    return false;
                }
            }
            propEntry = ListIterate (propEntry);
        }
        curEntry = ListIterate (curEntry);
    }
    // Ensure database and string table path are valid
    if (!conf.dbLoc.dbPath)
    {
        error ("%s: package database path not specified", ConfGetFileName());
        ConfFreeParseTree (blocks);
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    ConfFreeParseTree (blocks);
    cb->conf = &conf;
    return true;
}

// Destroys main configuration
NNPKG_PUBLIC void PkgDestroyMainConf()
{
    StrRefDestroy (conf.dbLoc.dbPath);
    StrRefDestroy (conf.dbLoc.strtabPath);
}

NNPKG_PUBLIC NnpkgPackage_t* PkgReadConf (NnpkgTransCb_t* cb, const char* file)
{
    // Parse file
    ListHead_t* blocks = ConfInit (file);
    if (!blocks)
    {
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return NULL;
    }
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
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        ObjDestroy (&pkgOut->obj);
        return NULL;
    }
    if (ListIterate (ListFront (blocks)))
    {
        error ("%s: only one package block supported in a configuration file",
               ConfGetFileName());
        ConfFreeParseTree (blocks);
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        ObjDestroy (&pkgOut->obj);
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
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        ObjDestroy (&pkgOut->obj);
        return NULL;
    }
    if (!block->blockName)
    {
        error ("%s:%d: block name required for block type \"%s\"",
               ConfGetFileName(),
               lineNo,
               UnicodeToHost (StrRefGet (block->blockType)));
        ConfFreeParseTree (blocks);
        cb->error = NNPKG_ERR_SYNTAX_ERR;
        ObjDestroy (&pkgOut->obj);
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
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
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                ObjDestroy (&pkgOut->obj);
                return NULL;
            }
            if (prop->vals[0].type != DATATYPE_STRING)
            {
                error ("%s:%d: property \"%s\" requires string value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                ObjDestroy (&pkgOut->obj);
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
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                ObjDestroy (&pkgOut->obj);
                return NULL;
            }
            if (prop->vals[0].type != DATATYPE_STRING)
            {
                error ("%s:%d: property \"%s\" requires string value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                ObjDestroy (&pkgOut->obj);
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
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                return NULL;
            }
            if (prop->vals[0].type != DATATYPE_IDENTIFIER)
            {
                error ("%s:%d: property \"%s\" requires boolean value",
                       ConfGetFileName(),
                       lineNo,
                       UnicodeToHost (StrRefGet (prop->name)));
                ConfFreeParseTree (blocks);
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                ObjDestroy (&pkgOut->obj);
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
                cb->error = NNPKG_ERR_SYNTAX_ERR;
                TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                ObjDestroy (&pkgOut->obj);
                return NULL;
            }
        }
        else if (!c32cmp (StrRefGet (prop->name), U"dependencies"))
        {
            bool depError = false;
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
                    cb->error = NNPKG_ERR_SYNTAX_ERR;
                    TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                    ObjDestroy (&pkgOut->obj);
                    return NULL;
                }
                // Find this package
                // TODO: versioning support
                ListEntry_t* curEntry = ListFront (cb->pkgDbs);
                NnpkgPackage_t* pkg = NULL;
                while (curEntry && !pkg)
                {
                    pkg = pkgDbFindPackage (cb,
                                            ListEntryData (curEntry),
                                            StrRefGet (propVal->id),
                                            true);
                    curEntry = ListIterate (curEntry);
                }
                if (pkg)
                    ListAddBack (pkgOut->deps, pkg, 0);
                else
                {
                    cb->error = NNPKG_ERR_BROKEN_DEP;
                    cb->errHint[0] = StrRefNew (pkgOut->id);
                    cb->errHint[1] = StrRefNew (propVal->id);
                    TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
                    depError = true;
                }
            }
            if (depError)
            {
                ConfFreeParseTree (blocks);
                ObjDestroy (&pkgOut->obj);
                return NULL;
            }
        }
        propEntry = ListIterate (propEntry);
    }
    ConfFreeParseTree (blocks);
    return pkgOut;
}
