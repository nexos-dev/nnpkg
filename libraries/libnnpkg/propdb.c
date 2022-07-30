/*
    propdb.c - contains property database engine
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
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <nnpkg/propdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libnex.h>

// TODO: This is very hacked together, quick and dirty, and slow.
// We really should be using an open-addressed hash table and
// allow for longer strings. But that is a future improvement

// File format structures. All fields are little endian
typedef struct _dbHeader
{
    uint64_t sig;       // Contains 0x7878807571686600, which is "NNPKGDB\0" in ASCII
    uint8_t version;    // Major version of this databse
    uint8_t revision;         // Revision of this database
    uint16_t size;            // Size of this header
    uint32_t crc32;           // Checksum of this header
    uint32_t numProps;        // Number of properties in database
    uint32_t numFreeProps;    // Number of free properties
    uint32_t propSize;
} __attribute__ ((packed)) propDbHeader_t;

// Header constants
#define NNPKG_SIGNATURE        0x7878807571686600
#define NNPKG_CURRENT_VERSION  0
#define NNPKG_CURRENT_REVISION 1

typedef struct _dbProp
{
    uint32_t id;       // String ID of ID
    uint32_t crc32;    // Checksum of this property
    uint16_t type;     // Property type
    uint8_t resvd[2];
} __attribute__ ((packed)) propDbProperty_t;

NNPKG_PUBLIC bool PropDbCreate (NnpkgDbLocation_t* dbLoc)
{
    const char* fileName = StrRefGet (dbLoc->dbPath);
    const char* strtab = StrRefGet (dbLoc->strtabPath);
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
        // Package database already exists, error out
        error (_ ("package database already exists"));
        return false;
    }
    // Create it
    char* dir = strdup (fileName);
    if (mkdir (dirname (dir), 0755) == -1 && errno != EEXIST)
    {
        error ("%s: %s", fileName, strerror (errno));
        free (dir);
        return false;
    }
    free (dir);
    int fd = 0;
    if ((fd = creat (fileName, 0644)) == -1)
    {
        error ("%s: %s", fileName, strerror (errno));
        return false;
    }
    // Initialize header
    propDbHeader_t hdr;
    hdr.sig = EndianChange64 (NNPKG_SIGNATURE, ENDIAN_LITTLE);
    hdr.version = NNPKG_CURRENT_VERSION;
    hdr.revision = NNPKG_CURRENT_REVISION;
    hdr.size = sizeof (propDbHeader_t);
    hdr.numProps = 0;
    hdr.numFreeProps = 0;
    hdr.propSize = PROPDB_PROP_SIZE;
    hdr.crc32 = 0;
    hdr.crc32 = Crc32Calc ((uint8_t*) &hdr, sizeof (propDbHeader_t));
    // Write it out
    if (write (fd, &hdr, sizeof (propDbHeader_t)) == -1)
    {
        error ("%s:%s", fileName, strerror (errno));
        return false;
    }
    close (fd);
    return PropDbInitStrtab (strtab);
}

// Destroys a property
void propDestroy (const Object_t* data)
{
    NnpkgProp_t* prop = ObjGetContainer (data, NnpkgProp_t, obj);
    StrRefDestroy (prop->id);
    free (prop->data);
    free (prop);
}

// Destroys a found property
void propDestroyFound (const Object_t* data)
{
    NnpkgProp_t* prop = ObjGetContainer (data, NnpkgProp_t, obj);
    StrRefDestroy (prop->id);
    free (prop);
}

// Finds a property in the add list
bool propAddListFind (const ListEntry_t* entry, const void* data)
{
    const char32_t* id = data;
    if (!c32cmp (StrRefGet (((NnpkgProp_t*) ListEntryData (entry))->id), id))
        return true;
    else
        return false;
}

NNPKG_PUBLIC NnpkgPropDb_t* PropDbOpen (NnpkgDbLocation_t* dbLoc)
{
    const char* fileName = StrRefGet (dbLoc->dbPath);
    const char* strtab = StrRefGet (dbLoc->strtabPath);
    // Prepare state
    NnpkgPropDb_t* db = calloc_s (sizeof (NnpkgPropDb_t));
    if (!db)
        return NULL;
    // Get size of database
    struct stat st;
    if (stat (fileName, &st) == -1)
    {
        error ("%s: %s", fileName, strerror (errno));
        free (db);
        return NULL;
    }
    db->sz = st.st_size;
    // Open database
    db->fd = open (fileName, O_RDWR);
    if (db->fd == -1)
    {
        error ("%s: %s", fileName, strerror (errno));
        free (db);
        return NULL;
    }
    // Lock database
    if (flock (db->fd, LOCK_EX | LOCK_NB) == -1)
    {
        if (errno == EWOULDBLOCK)
        {
            // Package database is locked
            error ("failed to acquire package database lock");
        }
        else
            error ("%s: %s", fileName, strerror (errno));
        close (db->fd);
        free (db);
        return NULL;
    }
    // Map database
    db->memBase = mmap (NULL, db->sz, PROT_READ | PROT_WRITE, MAP_SHARED, db->fd, 0);
    if (!db->memBase)
    {
        error ("%s: %s", fileName, strerror (errno));
        flock (db->fd, LOCK_UN);
        close (db->fd);
        free (db);
        return NULL;
    }
    // Initialize packages-to-add
    db->propsToAdd = ListCreate ("NnpkgProp_t", true, offsetof (NnpkgProp_t, obj));
    ListSetFindBy (db->propsToAdd, propAddListFind);
    if (!db->propsToAdd)
    {
        flock (db->fd, LOCK_UN);
        close (db->fd);
        munmap (db->memBase, db->sz);
        free (db);
        return NULL;
    }
    // Initialize packages-to-remove
    db->propsToRm = ListCreate ("NnpkgProp_t", true, offsetof (NnpkgProp_t, obj));
    if (!db->propsToRm)
    {
        flock (db->fd, LOCK_UN);
        close (db->fd);
        munmap (db->memBase, db->sz);
        ListDestroy (db->propsToAdd);
        free (db);
        return NULL;
    }
    propDbHeader_t* dbHdr = (propDbHeader_t*) db->memBase;
    db->numFreeProps = dbHdr->numFreeProps;
    assert (dbHdr->propSize == PROPDB_PROP_SIZE);
    if (!PropDbOpenStrtab (db, strtab))
    {
        flock (db->fd, LOCK_UN);
        close (db->fd);
        munmap (db->memBase, db->sz);
        ListDestroy (db->propsToAdd);
        ListDestroy (db->propsToRm);
        free (db);
        return NULL;
    }
    return db;
}

// Allocates a new property database entry
// This algorithm is disgraceful. We really need to use a hash table
propDbProperty_t* propDbAllocProp (NnpkgPropDb_t* db)
{
    // Check if there is even a point in trying
    propDbHeader_t* dbHdr = (propDbHeader_t*) db->memBase;
    if (!db->numFreeProps)
        return NULL;
    // Begin the loop
    // Check if we can use allocation marker
    propDbProperty_t* curProp = NULL;
    size_t propsLeft = 0;
    if (db->allocMark)
    {
        curProp = db->allocMark;
        propsLeft = db->propsLeft;
    }
    else
    {
        curProp = (propDbProperty_t*) (db->memBase + sizeof (propDbHeader_t));
        propsLeft = dbHdr->numProps;
    }
    for (int i = 0; i < propsLeft; ++i)
    {
        // Check if this entry is free
        if (curProp->type == NNPKG_PROP_TYPE_INVALID)
        {
            --db->numFreeProps;
            db->propsLeft = propsLeft - i;
            db->allocMark = curProp + 1;
            return curProp;
        }
        ++curProp;
    }
    return NULL;
}

// Serizalizes a property
void propDbSerializeProp (NnpkgPropDb_t* db,
                          NnpkgProp_t* prop,
                          propDbProperty_t* dbEntry)
{
    assert (dbEntry);
    assert (prop->dataLen <= (PROPDB_PROP_SIZE - sizeof (propDbProperty_t)));
    // Write out string
    dbEntry->id = PropDbAddString (db, StrRefGet (prop->id));
    dbEntry->type = prop->type;
    // Copy over other data
    memcpy (dbEntry + 1, prop->data, prop->dataLen);
    // Set CRC32
    dbEntry->crc32 = Crc32Calc ((uint8_t*) dbEntry, PROPDB_PROP_SIZE);
}

NNPKG_PUBLIC bool PropDbAddProp (NnpkgPropDb_t* db, NnpkgProp_t* prop)
{
    ObjCreate ("NnpkgProp_t", &prop->obj);
    ObjSetDestroy (&prop->obj, propDestroy);
    // Add to list of properties to add
    if (!ListAddBack (db->propsToAdd, prop, 0))
    {
        error ("out of memory");
        return false;
    }
    return true;
}

NNPKG_PUBLIC bool PropDbRemoveProp (NnpkgPropDb_t* db, const NnpkgProp_t* prop)
{
    assert (prop->internal);
    return ListAddBack (db->propsToRm, prop, 0);
}

NNPKG_PUBLIC bool PropDbFindProp (NnpkgPropDb_t* db,
                                  const char32_t* name,
                                  NnpkgProp_t* out)
{
    assert (out);
    // Loop through database
    propDbProperty_t* prop =
        (propDbProperty_t*) (db->memBase + sizeof (propDbHeader_t));
    propDbHeader_t* dbHdr = db->memBase;
    for (int i = 0; i < dbHdr->numProps; ++i)
    {
        if (!c32cmp (PropDbGetString (db, prop->id), name))
        {
            // Prepare property
            out->id = StrRefCreate (PropDbGetString (db, prop->id));
            StrRefNoFree (out->id);
            out->type = prop->type;
            out->data = prop + 1;
            out->dataLen = PROPDB_PROP_SIZE - sizeof (propDbProperty_t);
            out->internal = prop;
            ObjCreate ("NnpkgProp_t", &out->obj);
            ObjSetDestroy (&out->obj, propDestroyFound);
            return true;
        }
        prop = (((void*) prop + PROPDB_PROP_SIZE));
    }
    return false;
}

NNPKG_PUBLIC void PropDbClose (NnpkgPropDb_t* db)
{
    size_t curEnd = db->sz;
    propDbHeader_t* dbHdr = (propDbHeader_t*) db->memBase;
    uint32_t numProps = dbHdr->numProps;
    // Remove properties that need to be removed
    ListEntry_t* curEntry = ListFront (db->propsToRm);
    while (curEntry)
    {
        NnpkgProp_t* prop = ListEntryData (curEntry);
        // Clear property
        memset (prop->internal, 0, sizeof (propDbProperty_t));
        ++db->numFreeProps;
        curEntry = ListIterate (curEntry);
    }
    // Commit properties that need to be added
    curEntry = ListFront (db->propsToAdd);
    while (curEntry)
    {
        NnpkgProp_t* prop = ListEntryData (curEntry);
        // Allocate a new property
        propDbProperty_t* newProp = propDbAllocProp (db);
        // Check if we need to expand the database or not
        if (!newProp)
        {
            // Serialize property, then expand database
            newProp = calloc_s (PROPDB_PROP_SIZE);
            propDbSerializeProp (db, prop, newProp);
            // Write out to file
            pwrite (db->fd, newProp, PROPDB_PROP_SIZE, curEnd);
            curEnd += PROPDB_PROP_SIZE;
            free (newProp);
        }
        else
            propDbSerializeProp (db, prop, newProp);
        ++numProps;
        curEntry = ListIterate (curEntry);
    }
    // Set header fields that need to be updated
    dbHdr->numProps = numProps;
    dbHdr->numFreeProps = db->numFreeProps;
    // Recompute CRC32 of header
    dbHdr->crc32 = 0;
    dbHdr->crc32 = Crc32Calc ((uint8_t*) dbHdr, sizeof (propDbHeader_t));
    // Cleanup
    PropDbCloseStrtab (db);
    munmap (db->memBase, db->sz);
    ListDestroy (db->propsToAdd);
    ListDestroy (db->propsToRm);
    flock (db->fd, LOCK_UN);
    close (db->fd);
    free (db);
}
