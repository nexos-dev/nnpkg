/*
    transaction.c - contains main transaction manager
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
#include <libnex/safemalloc.h>
#include <nnpkg/pkg.h>
#include <nnpkg/transaction.h>

// Reports the next valid state for specified control block
int transactNextState (NnpkgTransCb_t* cb)
{
    // Check if this an unconditional transition
    switch (cb->state)
    {
        case NNPKG_TRANS_STATE_ERR:
            return NNPKG_TRANS_STATE_ERR;
        case NNPKG_STATE_ACCEPT:
            return NNPKG_STATE_ACCEPT;
    }
    // Do type-dependent transitions
    switch (cb->type)
    {
        case NNPKG_TRANS_ADD: {
            switch (cb->state)
            {
                case NNPKG_STATE_ADDPKG:
                    return NNPKG_STATE_CLEANUP_PKGSYS;
                case NNPKG_STATE_INIT_PKGSYS:
                    return NNPKG_STATE_READ_PKGCONF;
                case NNPKG_STATE_READ_PKGCONF:
                    return NNPKG_STATE_ADDPKG;
                case NNPKG_STATE_CLEANUP_PKGSYS:
                    return NNPKG_STATE_ACCEPT;
                default:
                    assert (!"Invalid state");
            }
        }
        default:
            assert (!"Invalid transaction type");
    }
    assert (!"Invalid state");
}

// Cleans up package system
bool transactCleanupPkgSys (NnpkgTransCb_t* cb)
{
    PkgCloseDbs();
    PkgDestroyMainConf();
    return true;
}

// Sets state, performing any special processing that must be done
NNPKG_PUBLIC void TransactSetState (NnpkgTransCb_t* cb, int state)
{
    cb->state = state;
    // Set up progress info
    switch (cb->state)
    {
        case NNPKG_STATE_ADDPKG: {
            NnpkgTransAdd_t* addTrans = cb->transactData;
            cb->progressHint[0] = StrRefNew (addTrans->pkg->id);
            break;
        }
    }
    // Run progressing hook
    cb->progress (cb, state);
}

// Prepares package system
bool transactRunInit (NnpkgTransCb_t* cb)
{
    // Parse configuration
    if (!PkgParseMainConf (cb, cb->confFile))
        return false;    // No extra cleanup needed
    // Open local database
    if (!PkgOpenDb (cb, &cb->conf->dbLoc, NNPKGDB_TYPE_DEST, NNPKGDB_LOCATION_LOCAL))
        return false;
    return true;
}

// Reads in package configuration
bool transactReadPkgConf (NnpkgTransCb_t* cb, NnpkgTransAdd_t* transAdd)
{
    transAdd->pkg = PkgReadConf (cb, transAdd->pkgConf);
    if (!transAdd->pkg)
    {
        transactCleanupPkgSys (cb);
        return false;
    }
    return true;
}

// Executes add operation
bool transactAddPkg (NnpkgTransCb_t* cb, NnpkgTransAdd_t* transAdd)
{
    if (!PkgAddPackage (cb, transAdd->pkg))
    {
        transactCleanupPkgSys (cb);
        ObjDestroy (&transAdd->pkg->obj);
        return false;
    }
    return true;
}

// Runs current state of state machine
bool transactRunState (NnpkgTransCb_t* cb)
{
    switch (cb->state)
    {
        case NNPKG_STATE_ACCEPT:
            break;
        case NNPKG_STATE_INIT_PKGSYS:
            return transactRunInit (cb);
        case NNPKG_STATE_READ_PKGCONF:
            return transactReadPkgConf (cb, cb->transactData);
        case NNPKG_STATE_ADDPKG:
            return transactAddPkg (cb, cb->transactData);
        case NNPKG_STATE_CLEANUP_PKGSYS:
            return transactCleanupPkgSys (cb);
    }
    return true;
}

// Executes transaction state machine
NNPKG_PUBLIC bool TransactExecute (NnpkgTransCb_t* cb)
{
    TransactSetState (cb, NNPKG_STATE_INIT_PKGSYS);
    // Run each state
    while (cb->state != NNPKG_STATE_ACCEPT)
    {
        if (!transactRunState (cb))
            return false;
        TransactSetState (cb, transactNextState (cb));
    }
    return true;
}
