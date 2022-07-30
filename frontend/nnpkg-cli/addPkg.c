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
#include <libnex.h>
#include <nnpkg/pkg.h>
#include <nnpkg/propdb.h>
#include <stdio.h>

// Arguments
static const char* pkgPath = NULL;

static bool addSetPkg (actionOption_t* opt, char* arg)
{
    UNUSED (opt);
    assert (arg);
    pkgPath = arg;
    return true;
}

// Option table
static actionOption_t addOptions[] = {
    {0, "",   addSetPkg, true},
    {0, NULL, NULL,      0   }
};

actionOption_t* addGetOptions()
{
    return addOptions;
}

bool addRunAction()
{
    // Ensure pkgPath was set
    if (!pkgPath)
    {
        error ("Package configuration file not specified");
        return false;
    }
    // NOTE: database handling is very bad right now. We need a better system.
    // For now, just grab the default database and call PropOpenDb()
    NnpkgDbLocation_t dbLoc;
    if (!PkgDbGetPath (&dbLoc))
        return false;
    if (!PkgOpenDb (&dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL))
        return false;
    // Parse configuration of package
    NnpkgPackage_t* newPkg = PkgReadConf (pkgPath);
    if (!newPkg)
        return false;
    // Finally, add it
    printf ("Adding package %s to database...\n",
            UnicodeToHost (StrRefGet (newPkg->id)));
    if (!PkgAddPackage (newPkg))
        return false;
    printf ("done\n");
    PkgCloseDbs();
    return true;
}
