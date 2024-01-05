/*
    addPkg.c - handles package add operation
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

#include "include/nnpkg.h"
#include <assert.h>
#include <errno.h>
#include <libnex.h>
#include <nnpkg/pkg.h>
#include <nnpkg/propdb.h>
#include <stdio.h>
#include <string.h>

// Arguments
static const char* pkgPath = NULL;
static const char* confFile = NNPKG_CONFFILE_PATH;

static bool addSetPkg (actionOption_t* opt, char* arg)
{
    UNUSED (opt);
    assert (arg);
    pkgPath = arg;
    return true;
}

static bool addSetConf (actionOption_t* opt, char* arg)
{
    UNUSED (opt);
    assert (arg);
    confFile = arg;
    return true;
}

// Option table
static actionOption_t addOptions[] = {
    {'c', "conf", addSetConf, true},
    {0,   "",     addSetPkg,  true},
    {0,   NULL,   NULL,       0   }
};

actionOption_t* addGetOptions()
{
    return addOptions;
}

// Progressing hook
void addProgress (NnpkgTransCb_t* cb, int newState)
{
    switch (newState)
    {
        case NNPKG_STATE_READ_PKGCONF:
            printf ("\n  * Reading package configuration...");
            break;
        case NNPKG_STATE_ADDPKG:
            printf ("\n  * Adding package %s to database...",
                    UnicodeToHost (StrRefGet (cb->progressHint[0])));
            StrRefDestroy (cb->progressHint[0]);
            break;
        case NNPKG_STATE_WRITE_INDEX:
            printf ("\n  * Writing changes to index...");
            break;
        case NNPKG_STATE_ACCEPT:
            printf ("\nDone!\n");
            break;
        case NNPKG_TRANS_STATE_ERR: {
            printf ("\n");
            switch (cb->error)
            {
                case NNPKG_ERR_OOM:
                    error ("out of memory");
                    break;
                case NNPKG_ERR_BROKEN_DEP:
                    // NOTE: we split these fprint's to account for the limitations
                    // of UnicodeToHost
                    fprintf (stderr,
                             "%s: error: package \"%s\" ",
                             getprogname(),
                             UnicodeToHost (StrRefGet (cb->errHint[0])));
                    fprintf (stderr,
                             "dependent on non-existant package \"%s\"\n",
                             UnicodeToHost (StrRefGet (cb->errHint[1])));
                    StrRefDestroy (cb->errHint[0]);
                    StrRefDestroy (cb->errHint[1]);
                    break;
                case NNPKG_ERR_DB_LOCKED:
                    error ("unable to acquire lock on package database");
                    break;
                case NNPKG_ERR_PKG_EXIST:
                    error ("package %s already exists",
                           UnicodeToHost (StrRefGet (cb->errHint[0])));
                    StrRefDestroy (cb->errHint[0]);
                    break;
                case NNPKG_ERR_SYNTAX_ERR:
                    error ("syntax error in configuration file");
                    break;
                case NNPKG_ERR_SYS:
                    error ("system error: %s", strerror (cb->sysErrno));
                    break;
            }
        }
    }
}

bool addRunAction()
{
    // Ensure pkgPath was set
    if (!pkgPath)
    {
        error ("Package configuration file not specified");
        return false;
    }
    printf ("  * Starting transaction...");
    NnpkgTransCb_t cb = {0};
    // Prepare control block
    cb.type = NNPKG_TRANS_ADD;
    cb.confFile = confFile;
    cb.progress = addProgress;
    NnpkgTransAdd_t transData = {0};
    transData.pkgConf = pkgPath;
    cb.transactData = &transData;
    // Run transaction
    bool res = TransactExecute (&cb);
    if (!res)
        printf ("\n  * An error occurred while executing transaction. Aborting.\n");
    return res;
}
