/*
    initDb.c - handles database initialization action
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
#include <libnex.h>
#include <nnpkg/pkg.h>
#include <nnpkg/propdb.h>
#include <stdio.h>

// Path to configuration file
static const char* confFile = NNPKG_CONFFILE_PATH;

bool setConfPath (actionOption_t* opt, char* arg)
{
    confFile = arg;
    return true;
}

// Option table
static actionOption_t initOptions[] = {
    {'c', "conf", setConfPath, true},
    {0,   NULL,   NULL,        0   }
};

// Gets action option table
actionOption_t* initGetOptions()
{
    return initOptions;
}

static void progress (NnpkgTransCb_t* cb, int state)
{
}

// Run init action
bool initRunAction()
{
    NnpkgTransCb_t cb;
    cb.progress = progress;
    PkgParseMainConf (&cb, confFile);
    NnpkgDbLocation_t* dbLoc = &cb.conf->dbLoc;
    if (!PropDbCreate (dbLoc))
    {
        error ("Unable to create package database");
        return false;
    }
    printf ("Initialized empty package database in %s\n",
            (char*) StrRefGet (dbLoc->dbPath));
    PkgDestroyMainConf();
    return true;
}
