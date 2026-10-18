// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // for size_t, not part of stdint.h
#include "UsefulBuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MDSemantics {
    size_t meta_len;   // Maximum length of meta-data (derived from data)
    size_t data_len;   // Maximum length of data, sometimes called slot_len
    uint32_t version;  // Version.  0 for backwards-compatible unversioned stores
    char const* name;  // For informational logging

    // Produce a buffer suitable for storage in a persistent table of contents.
    int32_t (*getMeta)(struct MDSemantics const* self,
                       UsefulBufC data,
                       void* context,
                       UsefulBuf meta);

    // Used to check whether an entry is used or unused.
    bool (*isMetaVacant)(struct MDSemantics const* self, UsefulBufC meta);

    // Used to check whether a slot is used or unused.
    bool (*isDataVacant)(struct MDSemantics const* self, UsefulBufC data);

    // Used to import licenses from one store config to other.
    bool (*importLicense)(void* context, UsefulBufC data, uint16_t handle);

} MDSemantics;

#include "BlockStorage.h"

/**
 * Note on handles.  The handle is the index of the entry in the meta-data table.
 * For split storage, this is the same as the absolute index of the data slot.
 * For a unified store, the meta-data occupies slot 0, so when fetching the data
 * slot we have to add one to the handle to get the slot number.
 */
typedef uint16_t MDTableHandle;
#define INVALID_MDTABLE_HANDLE 0xFFFF

#define REVOCATION_LIST_SLOT_COUNT 1

typedef int32_t (*MDVisit)(void* visitor,
                           void* context,
                           MDTableHandle handle,
                           UsefulBufC meta,
                           int* read_more);

/**
 * Meta-Data Table module
 *
 * This module is responsible for associating MD blobs with data blobs.
 * There is only one implementation of this interface.  It depends on
 * an implementation of MDSemantics to generate meta-data from data blobs
 * and to validate data blobs themselves.  It depends on an implementation
 * of BlockStorage to persistently read and write meta-data and data.
 */

typedef struct {
    // Parameters calculated and checked during initialization
    //! Copied from MDSemantics
    size_t meta_len;
    //! Largest number of items that can be stored.  Calculated during
    //  initialization.  May be < slots_per_store * store_count.
    size_t meta_count;
    //! Copied from MDSemantics
    size_t slot_len;
    //! Calculated during initialization
    size_t slots_per_store;

    // Pointers to implementations
    MDSemantics const* semantics;
    BlockStorage* data_storage;
    BlockStorage* meta_storage;  // if NULL, meta_buf is stored in data slot 0

    // Buffers for data and meta-data
    // length of meta_buf is meta_len * meta_count, rounded up to block size
    UsefulBuf meta_buf;
    // length of data_buf is_slot_len * slots_per_store, rounded up to block size
    UsefulBuf data_buf;

    // Other
    bool empty;
    uint16_t current_data_store;
    MDTableHandle* visited;
} MDTable;

int32_t MDTable_init(MDTable* self,
                     MDSemantics const* sem,
                     BlockStorage* data_storage,
                     BlockStorage* meta_storage);
void MDTable_destroy(MDTable* self);
static bool __maybe_unused MDTable_isEmpty(MDTable* self) { return self->empty; }

// Allow client to provide meta-data to avoid re-calculation
int32_t MDTable_installMeta(MDTable* self, UsefulBufC meta, UsefulBufC data, bool flush);
// Allow client to not provide meta-data to avoid duplicate code
int32_t MDTable_install(MDTable* self, UsefulBufC data);

int32_t MDTable_updateMeta(MDTable* self,
                           MDTableHandle handle,
                           UsefulBufC meta,
                           UsefulBufC data);
int32_t MDTable_update(MDTable* self, MDTableHandle handle, UsefulBufC data);
int32_t MDTable_updateRevocationList(MDTable* self, UsefulBufC revList);
int32_t MDTable_erase(MDTable* self, MDTableHandle handle);

int32_t MDTable_visit(MDTable* self, MDVisit visit, void* visitor, void* context);

UsefulBufC MDTable_getMeta(MDTable* self, MDTableHandle handle);
UsefulBufC MDTable_getData(MDTable* self, MDTableHandle handle);
UsefulBuf MDTable_getDataAsUB(MDTable* self, MDTableHandle handle);
UsefulBufC MDTable_getRevocationList(MDTable* self);

int32_t MDTable_RevokeLicense(MDTable* self, MDTableHandle handle);
int32_t MDTable_flushMeta(MDTable* self);
int32_t MDTable_flushData(MDTable* self);
void MDTable_resetMeta(MDTable* self);

#ifdef __cplusplus
}
#endif
