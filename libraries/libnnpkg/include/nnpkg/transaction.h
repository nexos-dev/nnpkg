/*
    transaction.h - contains definition of transaction layer
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

/// @file transaction.g

#ifndef _TRANSACTION_H
#define _TRANSACTION_H

#include <config.h>
#include <libnex/list.h>
#include <libnex/stringref.h>

// Configuration struct forward decl
typedef struct _nnpkgConf NnpkgMainConf_t;

// Error codes
#define NNPKG_ERR_NONE         0
#define NNPKG_ERR_OOM          1    /// Out of memory
#define NNPKG_ERR_SYS          2    /// Error is contained in errno
#define NNPKG_ERR_DB_LOCKED    3    // Database is locked
#define NNPKG_ERR_PKG_NO_EXIST 4    // Package does not exist
#define NNPKG_ERR_PKG_EXIST    5    // Package does exist
#define NNPKG_ERR_BROKEN_DEP   6    // Dependency is broken
#define NNPKG_ERR_SYNTAX_ERR \
    7    // Syntax error in file. Error has been printed already
         // FIXME: This won't work when we add a GUI frontend

// Transaction types
#define NNPKG_TRANS_ADD 1

// Transaction states
#define NNPKG_TRANS_STATE_ERR      1
#define NNPKG_STATE_ADDPKG         2
#define NNPKG_STATE_INIT_PKGSYS    3
#define NNPKG_STATE_READ_PKGCONF   4
#define NNPKG_STATE_ACCEPT         5
#define NNPKG_STATE_CLEANUP_PKGSYS 6
#define NNPKG_STATE_COLLECT_INDEX  7
#define NNPKG_STATE_WRITE_INDEX    8

// Transaction structure
typedef struct _nnpkgact
{
    // State information
    int state;    ///< Current state of transaction
    int type;     ///< Type of transaction being performed
    void (*progress) (struct _nnpkgact* cb,
                      int newState);    ///< Called when we transition to a new state

    // Error information
    int error;                    ///< Error code if an error occurred
    int sysErrno;                 ///< Saved errno
    StringRef32_t* errHint[5];    ///< Useful diagnostic data in error
#define progressHint errHint

    // Internal configuration
    ListHead_t* pkgDbs;       ///< Databases loaded
    const char* confFile;     ///< Configuration file
    NnpkgMainConf_t* conf;    ///< Configuration of program

    // Block of data pertaining to transaction type
    void* transactData;
} NnpkgTransCb_t;

/// Sets state, performing any special processing that must be done
NNPKG_PUBLIC void TransactSetState (NnpkgTransCb_t* cb, int state);

/// Executes transaction state machine
NNPKG_PUBLIC bool TransactExecute (NnpkgTransCb_t* cb);

#endif
