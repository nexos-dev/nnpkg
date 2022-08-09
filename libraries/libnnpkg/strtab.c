/*
    strtab.c - contains string table management
    Copyright 2022 The NexNix Project

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    There should be a copy of the License distributed in a file named
    LICENSE, if not, you may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/// @file strtab.c

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <libnex/char32.h>
#include <libnex/error.h>
#include <libnex/safemalloc.h>
#include <libnex/safestring.h>
#include <nnpkg/propdb.h>
#include <nnpkg/transaction.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct _strtabHdr
{
    uint64_t sig;      ///< Signature of file
    uint8_t verMaj;    ///< Major version number
    uint8_t verMin;    ///< Minor version number
    uint16_t pad;
} __attribute__ ((packed)) NnpkgStrtabHdr_t;

// Header constants
#define NNPKG_SIGNATURE        0x7878807571686600
#define NNPKG_CURRENT_VERSION  0
#define NNPKG_CURRENT_REVISION 1

// Alignment helper
static inline size_t strtabAlign (size_t val)
{
    return (val & (sizeof (char32_t) - 1))
               ? ((val + sizeof (char32_t)) & ~(sizeof (char32_t) - 1))
               : val;
}

NNPKG_PUBLIC bool PropDbInitStrtab (const char* fileName)
{
    // Ensure it doesn't already exist
    struct stat st;
    if (stat (fileName, &st) == -1)
    {
        if (errno != ENOENT)
        {
            error ("%s: %s", fileName, strerror (errno));
            return false;
        }
    }
    else
    {
        // String table already exists, error out
        error (_ ("string table already exists"));
        return false;
    }
    int fd = 0;
    if ((fd = creat (fileName, 0644)) == -1)
    {
        error ("%s: %s", fileName, strerror (errno));
        return false;
    }
    NnpkgStrtabHdr_t strtab = {0};
    strtab.sig = NNPKG_SIGNATURE;
    strtab.verMaj = NNPKG_CURRENT_VERSION;
    strtab.verMin = NNPKG_CURRENT_REVISION;
    if (write (fd, &strtab, sizeof (NnpkgStrtabHdr_t)) == -1)
    {
        error ("%s: %s", fileName, strerror (errno));
        return false;
    }
    close (fd);
    return true;
}

NNPKG_PUBLIC bool PropDbOpenStrtab (NnpkgTransCb_t* cb,
                                    NnpkgPropDb_t* db,
                                    const char* fileName)
{
    // Get size of database
    struct stat st;
    if (stat (fileName, &st) == -1)
    {
        cb->error = NNPKG_ERR_SYS;
        cb->sysErrno = errno;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        free (db);
        return NULL;
    }
    db->strtabSz = st.st_size;
    db->strtabOff = st.st_size;
    // Open database
    db->strtabFd = open (fileName, O_RDWR);
    if (db->strtabFd == -1)
    {
        cb->error = NNPKG_ERR_SYS;
        cb->sysErrno = errno;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        return false;
    }
    // Map database
    db->strtabBase =
        mmap (NULL, db->strtabSz, PROT_READ, MAP_PRIVATE, db->strtabFd, 0);
    if (!db->strtabBase)
    {
        cb->error = NNPKG_ERR_SYS;
        cb->sysErrno = errno;
        TransactSetState (cb, NNPKG_TRANS_STATE_ERR);
        close (db->strtabFd);
        return false;
    }
    return true;
}

NNPKG_PUBLIC size_t PropDbAddString (NnpkgPropDb_t* db, const char32_t* s)
{
    // Write it out
    size_t len = c32len (s);
    pwrite (db->strtabFd, s, (len + 1) * sizeof (char32_t), (off_t) db->strtabOff);
    size_t ret = db->strtabOff;
    db->strtabOff += strtabAlign ((len + 1) * sizeof (char32_t));
    db->strtabSz += strtabAlign ((len + 1) * sizeof (char32_t));
    return ret;
}

NNPKG_PUBLIC const char32_t* PropDbGetString (NnpkgPropDb_t* db, size_t idx)
{
    if (idx > db->strtabSz)
        assert (!"String index out of bounds");
    // Read it in
    return (char32_t*) ((void*) db->strtabBase + idx);
}

NNPKG_PUBLIC void PropDbCloseStrtab (NnpkgPropDb_t* db)
{
    munmap (db->strtabBase, db->strtabSz);
    close (db->strtabFd);
}
