// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "include/MDTable.h"

#include <stdlib.h>

#include "include/Utils.h"
#include "include/object.h"
//#include "crc.h"

// Constants
// Stores larger than this will only be partially used.
#ifndef MAX_MDTABLE_BUFFER
/* Max slot is 62 and slot size is 4 KB*/
#define MAX_MDTABLE_BUFFER (62 * 4 * 1024)
#endif

#define INVALID_STORE 0xFFFF
// Note:  This value cannot be changed without code changes
#define INDEX_STORE 0

#define REPAIR_TOC 0

typedef struct {
    size_t meta_len;
    size_t data_len;
    uint32_t version;
    uint32_t crc;
} MetaHeader;

#define META_HEADER_LEN sizeof(MetaHeader)

//-----------------------------------------------------------------------------
// Utility functions.
//-----------------------------------------------------------------------------

// Allocate enough space for nitems of item_len bytes each, rounded
// up to the next multiple of block_size, with overflow checks.
int32_t allocateBlocks(size_t nitems,
                       size_t item_len,
                       size_t header_len,
                       BlockStorage* storage,
                       UsefulBuf* buffer) {
    size_t nominal = 0;
    if (Object_isERROR(checked_mult_size(nitems, item_len, &nominal))) {
        QWES_LOG_ERROR("overflow %zu * %zu", nitems, item_len);
        return Object_ERROR;
    }
    if (SIZE_MAX - header_len < nominal) {
        QWES_LOG_ERROR("overflow %zu + %zu", nominal, header_len);
        return Object_ERROR;
    }
    nominal += header_len;
    size_t rounded = 0;
    if (Object_isERROR(roundUp(nominal, storage->block_size, &rounded)) ||
        rounded > storage->store_len) {
        QWES_LOG_ERROR("rounding %zu %zu %zu", nominal, storage->block_size,
                       storage->store_len);
        return Object_ERROR;
    }
    QWES_LOG_DEBUG("rounding %zu %zu %zu", nominal, storage->block_size,
                   storage->store_len);

    buffer->ptr = (uint8_t*)qwes_malloc(rounded);
    if (buffer->ptr) {
        buffer->len = rounded;
        return Object_OK;
    }
    return Object_ERROR;
}

//-----------------------------------------------------------------------------
// Internal functions
//-----------------------------------------------------------------------------
static size_t firstDataSlot(MDTable* self) {
    return self->meta_storage ? 0 : 1;
}

static uint16_t slotToStore(MDTable* self, size_t slot) {
    if (0 == self->slots_per_store) {
        return INVALID_STORE;
    }
    return (slot / self->slots_per_store);
}

static void invalidateStore(MDTable* self) {
    self->current_data_store = INVALID_STORE;
}

static int32_t readStore(MDTable* self, uint16_t store) {
    // shorthand
    BlockStorage* st = self->data_storage;
    // Sanity checks
    if (UsefulBuf_IsNULL(self->data_buf) || !st) {
        QWES_LOG_ERROR("init");
        return Object_ERROR;
    }
    int err = st->read(st, store, (uint8_t*)self->data_buf.ptr, self->data_buf.len);
    if (Object_isERROR(err)) {
        invalidateStore(self);
        return err;
    }
    self->current_data_store = store;
    return Object_OK;
}

static int32_t writeCurrentStore(MDTable* self) {
    // shorthand
    BlockStorage* st = self->data_storage;
    // Sanity checks
    if (UsefulBuf_IsNULL(self->data_buf) || !st) {
        QWES_LOG_ERROR("init");
        return Object_ERROR;
    }
    if (st->read_only) {
        QWES_LOG_ERROR("read-only");
        return Object_ERROR;
    }
    int err = st->write(st, self->current_data_store, (uint8_t*)self->data_buf.ptr,
                        self->data_buf.len);
    if (Object_isERROR(err)) {
        QWES_LOG_ERROR("write: %d", err);
    }
    return err;
}

static int32_t readMeta(MDTable* self) {
    int32_t err = Object_ERROR;
    // Sanity checks
    if (UsefulBuf_IsNULL(self->meta_buf)) {
        QWES_LOG_ERROR("init");
        return err;
    }
    if (self->meta_storage) {
        // shorthand
        QWES_LOG_DEBUG("loading meta");
        BlockStorage* st = self->meta_storage;
        err = st->read(st, 0, (uint8_t*)self->meta_buf.ptr, self->meta_buf.len);
        if (Object_isERROR(err)) {
            QWES_LOG_ERROR("read");
        }
    } else {
        QWES_LOG_DEBUG("copying meta");
        if (self->meta_buf.len == memscpy(self->meta_buf.ptr, self->meta_buf.len,
                                           self->data_buf.ptr, self->data_buf.len)) {
            err = Object_OK;
        } else {
            // should never happen based on checks above
            QWES_LOG_ERROR("truncated");
        }
    }
    return err;
}

#if 0
static uint32_t crcMeta(MetaHeader const* hdr) {
    return crc_32_calc((uint8_t const*)hdr, offsetof(MetaHeader, crc), 0xFFFFFFFF);
}
#endif

static int32_t checkMeta(MDTable* self) {
    // Sanity checks
    if (!self->semantics || !self->meta_buf.ptr ||
        self->meta_buf.len <= META_HEADER_LEN) {
        QWES_LOG_ERROR("internal");
        return Object_ERROR;
    }
    // shorthand
    MDSemantics const* sem = self->semantics;
    if (!sem->version) {
        // Non-versioned.  We are at the mercy of isMetaVacant / isSlotVacant
        QWES_LOG_INFO("unversioned");
        return Object_OK;
    }
#if 0
    // Check crc to see whether meta storage is initialized
    MetaHeader* hdr = (MetaHeader*)self->meta_buf.ptr;
    int32_t crc = crcMeta(hdr);
    if (crc != hdr->crc) {
        QWES_LOG_INFO("first-time meta init (found 0x%x vs 0x%x", hdr->crc, crc);
        UsefulBuf_Set(self->meta_buf, 0);
        hdr->meta_len = sem->meta_len;
        hdr->data_len = sem->data_len;
        hdr->version = sem->version;
        hdr->crc = crcMeta(hdr);
        QWES_LOG_DEBUG("crc = 0x%x", hdr->crc);
        return Object_OK;
    }
    // Check stored meta config against semantics
    if (sem->meta_len != hdr->meta_len || sem->data_len != hdr->data_len) {
        QWES_LOG_ERROR("storage mismatch: found %zu/%zu, expected %zu/%zu", hdr->meta_len,
                       hdr->data_len, sem->meta_len, sem->data_len);
        return Object_ERROR;
    }
    if (sem->version != hdr->version) {
        QWES_LOG_ERROR("version mismatch: found %u, expected %u", hdr->version,
                       sem->version);
        return Object_ERROR;
    }
#endif
    return Object_OK;
}

static int32_t writeMeta(MDTable* self) {
    BlockStorage* ms = self->meta_storage;
    if (ms->read_only) {
        QWES_LOG_ERROR("read-only");
        return Object_ERROR;
    }
    int32_t err = ms->write(ms, 0, (uint8_t*)self->meta_buf.ptr, self->meta_buf.len);
    if (Object_isERROR(err)) {
        QWES_LOG_ERROR("write: %d", err);
    }
    return err;
}

static int32_t copyMetaToData(MDTable* self) {
    // TOC always goes at the beginning of the data buffer.
    // meta_buf should never be longer than slot_len, so this should never fail.
    if (self->meta_buf.len != memscpy(self->data_buf.ptr, self->data_buf.len,
                                       self->meta_buf.ptr, self->meta_buf.len)) {
        QWES_LOG_ERROR("internal");
        return Object_ERROR;
    }
    return Object_OK;
}

static int32_t fetchStore(MDTable* self, uint16_t store) {
    if (UsefulBuf_IsNULL(self->data_buf) || !self->data_storage) {
        QWES_LOG_ERROR("init: %p / %p", self->data_buf, self->data_storage);
        return Object_ERROR;
    }
    if (store == self->current_data_store) {
        return Object_OK;
    }
    return readStore(self, store);
}

#define fetchSlotC(s, h) UsefulBuf_Const(fetchSlot(s, h))

static UsefulBuf fetchSlot(MDTable* self, uint16_t handle) {
    uint16_t slot = handle + firstDataSlot(self);
    uint16_t store = slotToStore(self, slot);
    if (INVALID_STORE == store) {
        // Should only happen if initialization failed
        QWES_LOG_ERROR("Configuration");
        return NULLUsefulBuf;
    }
    if (self->slots_per_store == 0) {
        QWES_LOG_ERROR("zero slots per store");
        return NULLUsefulBuf;
    }
    if (fetchStore(self, store)) {
        QWES_LOG_ERROR("fetch %u", store);
        return NULLUsefulBuf;
    }
    size_t relative_slot = slot % self->slots_per_store;
    // Check for overflow.
    size_t offset = 0;
    if (checked_mult_size(relative_slot, self->slot_len, &offset)) {
        QWES_LOG_ERROR("relative slot %zu * %zu", relative_slot, self->slot_len);
        return NULLUsefulBuf;
    }
    if (offset > self->data_buf.len || self->data_buf.len - offset < self->slot_len) {
        QWES_LOG_ERROR("offset %zu", offset);
        return NULLUsefulBuf;
    }
    return (UsefulBuf){(uint8_t*)self->data_buf.ptr + offset, self->slot_len};
}

#define getMetaC(s, sl) UsefulBuf_Const(getMeta(s, sl))

// Similar to fetchSlot, but the index buffer is always available
//
// Important Compatibility Note:  In the first release of MDTable,
// used for QWESStore in Waipio, handles, slot numbers, and ToC
// entries were identical.  So in a combined store, the zeroth ToC
// entry was empty.  However, this was not the case with the old
// LicenseStore, which used the zeroth ToC entry to hold the meta-data
// for the first slot.  For compatibility with pre-built LicenseStore
// files (e.g. qweslicstore.bin) we now duplicate the old behavior if
// the 'version' field is zero.  For compatibility with persistent
// QWESStore key data in Waipio, we have to duplicate that behavior
// if the 'version' field is NOT zero.
//
// Note that this also means we need to allow for the unused ToC entry
// zero when calculating meta_count in MDTable_init().
static UsefulBuf getMeta(MDTable* self, uint16_t handle) {
    if (handle >= self->meta_count) {
        QWES_LOG_ERROR("handle %zu", handle);
        return NULLUsefulBuf;
    }
    // See function comment
    size_t slot = handle;
    if (self->semantics->version) {
        // Note that in a split store, this has no effect
        slot += firstDataSlot(self);
    }
    size_t offset = 0;
    if (checked_mult_size(slot, self->meta_len, &offset)) {
        QWES_LOG_ERROR("overflow %zu * %zu", slot, self->meta_len);
        return NULLUsefulBuf;
    }
    if (self->semantics->version) {
        // Check for overflow and skip meta header
        // Note: meta_len is always initialized to be > META_HEADER_LEN
        if (offset > self->meta_buf.len - META_HEADER_LEN) {
            QWES_LOG_ERROR("offset %zu", offset);
            return NULLUsefulBuf;
        }
        offset += META_HEADER_LEN;
    }
    if (self->meta_buf.len - offset < self->meta_len) {
        QWES_LOG_ERROR("offset(%u/%zu) = %zu", handle, slot, offset);
        return NULLUsefulBuf;
    }
    return (UsefulBuf){(uint8_t*)self->meta_buf.ptr + offset, self->meta_len};
}

#define getRevocationListC(s) UsefulBuf_Const(getRevocationList(s))

static UsefulBuf getRevocationList(MDTable* self) {
    size_t slot = self->meta_count;
    size_t offset = 0u;

    if (checked_mult_size(slot, self->meta_len, &offset)) {
        QWES_LOG_ERROR("overflow %zu * %zu", slot, self->meta_len);
        return NULLUsefulBuf;
    }

    if (self->semantics->version) {
        // Check for overflow and skip meta header
        // Note: meta_len is always initialized to be > META_HEADER_LEN
        if (META_HEADER_LEN > self->meta_buf.len) {
            QWES_LOG_ERROR("meta_buf corrupted. len %zu", self->meta_buf.len);
            return NULLUsefulBuf;
        }
        if (offset > self->meta_buf.len - META_HEADER_LEN) {
            QWES_LOG_ERROR("offset %zu", offset);
            return NULLUsefulBuf;
        }
        offset += META_HEADER_LEN;
    }
    if ((offset > self->meta_buf.len) || (self->meta_buf.len - offset < self->meta_len)) {
        QWES_LOG_ERROR("offset %zu != %zu", offset, slot);
        return NULLUsefulBuf;
    }
    return (UsefulBuf){(uint8_t*)self->meta_buf.ptr + offset, self->meta_len};
}

// Initialize structures with meta-data stored in slot zero.  (Unified storage)
static void initMetaSlotZero(MDTable* self,
                             MDSemantics const* sem,
                             BlockStorage* data_storage) {
    // Shorthand
    size_t data_len = sem->data_len;
    size_t meta_len = sem->meta_len;

    // Figure out how much of the available storage we can actually use.
    // MAX_MDTABLE_BUFFER should be a multiple of the storage block size.
    // If not, data_buf.len may exceed MAX_MDTABLE_BUFFER by the block size.
    size_t store_len = data_storage->store_len;
    if (store_len > MAX_MDTABLE_BUFFER) {
        QWES_LOG_WARN("%zu bytes unused per store", store_len - MAX_MDTABLE_BUFFER);
        store_len = MAX_MDTABLE_BUFFER;
    }
    size_t store_count = data_storage->store_count;
    if (store_count >= INVALID_STORE) {
        store_count = INVALID_STORE - 1;
    }

    // Pre-calculation sanity checks.
    // Storage length in particular may depend on OEM system configuration.
    if (meta_len > store_len || data_len > store_len || data_len < META_HEADER_LEN) {
        QWES_LOG_ERROR("length: %zu or %zu vs %zu", meta_len, data_len, store_len);
        return;
    }
    if (!meta_len || !data_len) {
        QWES_LOG_ERROR("length: meta: %zu data: %zu", meta_len, data_len);
        return;
    }
    size_t slots_per_store = store_len / data_len;
    size_t max_meta_per_slot = (data_len - META_HEADER_LEN) / meta_len;
    // In non-legacy unified storage, ToC slot zero is unused. See getMeta for details
    size_t meta_per_slot = max_meta_per_slot;
    if (sem->version) {
        if (meta_per_slot < 2) {
            // Weird case, but we have to handle it.
            QWES_LOG_ERROR("slot too small for meta");
            return;
        }
        meta_per_slot -= 1;
    }

    size_t max_slots = 0;
    // Must have at least one slot for the index and one slot for a license.
    int32_t err = checked_mult_size(slots_per_store, store_count, &max_slots);
    if (Object_isERROR(err) || max_slots < 2) {
        QWES_LOG_ERROR("max_slots: %zu * %zu", slots_per_store, store_count);
        return;
    }
    // Subtract one for the index.  After this, max_slots is >= 1.
    max_slots--;

    // Sanity checks have passed, set size parameters
    self->meta_len = meta_len;
    // Use max_slots entries if we are limited by number of slots, or both are equal
    // Otherwise we are limited by the number of index entries we can store in one slot.
    size_t meta_count = (meta_per_slot >= max_slots) ? max_slots : meta_per_slot;
    QWES_LOG_DEBUG("meta_count = %zu, meta_per_slot = %zu, max_slots = %zu", meta_count,
                   meta_per_slot, max_slots);
    self->meta_count = meta_count;
    self->slot_len = data_len;
    self->slots_per_store = slots_per_store;

    // Note:  At least some of these overflow checks are redundant.
    // First, ANOTHER place we have to special-case for unused ToC entry 0.
    if (SIZE_MAX == meta_count) {
        QWES_LOG_ERROR("internal1");
        return;
    }
    size_t buff_meta_count = sem->version ? meta_count + 1: meta_count;
    QWES_LOG_DEBUG("buff meta count %zu, meta_len %zu", buff_meta_count, meta_len);
    // Then we can multiply and add
    size_t meta_buf_len = 0;
    if (Object_isERROR(checked_mult_size(buff_meta_count, meta_len, &meta_buf_len))) {
        QWES_LOG_ERROR("internal2");
        return;
    }
    if (SIZE_MAX - META_HEADER_LEN < meta_buf_len) {
        QWES_LOG_ERROR("internal3");
        return;
    }
    meta_buf_len += META_HEADER_LEN;
    self->meta_buf = (UsefulBuf){qwes_malloc(meta_buf_len), meta_buf_len};
    allocateBlocks(slots_per_store, data_len, 0, data_storage, &self->data_buf);
}

static void initSeparateMeta(MDTable* self,
                             MDSemantics const* sem,
                             BlockStorage* data_storage,
                             BlockStorage* meta_storage) {
    // Calculate slots per store and total slot count based on data_storage
    size_t data_len = sem->data_len;
    if (data_len > data_storage->store_len || 0 == data_len) {
        QWES_LOG_ERROR("data length: %zu vs %zu", data_len, data_storage->store_len);
        return;
    }
    size_t slots_per_store = data_storage->store_len / data_len;
    size_t slot_count = 0;
    int32_t err =
        checked_mult_size(slots_per_store, data_storage->store_count, &slot_count);
    if (Object_isERROR(err) || slot_count < 1) {
        QWES_LOG_ERROR("slot_count: %zu * %zu", slots_per_store,
                       data_storage->store_count);
        return;
    }

    // Calculate meta data entry count based on meta_storage
    size_t meta_len = sem->meta_len;
    if (meta_len > SIZE_MAX - META_HEADER_LEN ||
        meta_len + META_HEADER_LEN > meta_storage->store_len || 0 == meta_len) {
        QWES_LOG_ERROR("meta length: %zu vs %zu", meta_len, meta_storage->store_len);
        return;
    }

    // Using the last meta slot for revocation list. no need to store this info in MDTable
    size_t meta_count = ((meta_storage->store_len - META_HEADER_LEN) / meta_len) -
        REVOCATION_LIST_SLOT_COUNT;
    if (meta_count > slot_count) {
        // This includes cases where the store length is rounded up by the block size
        meta_count = slot_count;
    } else if (meta_count < slot_count) {
        QWES_LOG_WARN("config: %zu < %zu", meta_count, slot_count);
        slot_count = meta_count;
    }

    // Sanity checks have passed, set size parameters and allocate buffers
    self->meta_len = meta_len;
    self->meta_count = meta_count;
    self->slot_len = data_len;
    self->slots_per_store = slots_per_store;
    // Add another slot in meta to allocate for RevocationList
    allocateBlocks(meta_count + REVOCATION_LIST_SLOT_COUNT, meta_len, META_HEADER_LEN, meta_storage,
            &self->meta_buf);
    allocateBlocks(slots_per_store, data_len, 0, data_storage, &self->data_buf);
}

//-----------------------------------------------------------------------------
// Public functions
//-----------------------------------------------------------------------------

/**
 * Initialize an MDTable.
 *
 * On success, ownership of data_storage and meta_storage is transferred to
 * the MDTable, and they will be destructed and deleted in MDTable_destroy.
 * On failure, the client retains ownership of data_storage and meta_storage.
 */
int32_t MDTable_init(MDTable* self,
                     MDSemantics const* sem,
                     BlockStorage* data_storage,
                     BlockStorage* meta_storage) {
    // Semantics and data store are always required, and init should not be called twice
    if (!sem || !data_storage || !UsefulBuf_IsNULL(self->data_buf)) {
        QWES_LOG_ERROR("internal");
        return Object_ERROR;
    }
    QWES_LOG_INFO("Initializing storage for %s, version %d", sem->name, sem->version);
    QWES_LOG_DEBUG("data block size: %zu", data_storage->block_size);
    if (meta_storage) {
        QWES_LOG_DEBUG("meta block size: %zu", meta_storage->block_size);
        initSeparateMeta(self, sem, data_storage, meta_storage);
    } else {
        initMetaSlotZero(self, sem, data_storage);
    }
    // If either allocation failed, or initialization bailed out early, clean up
    if (UsefulBuf_IsNULL(self->meta_buf) || UsefulBuf_IsNULL(self->data_buf) ||
        self->meta_buf.len > self->data_buf.len) {
        QWES_LOG_ERROR("allocation");
        goto cleanup;
    }
#if REPAIR_TOC
    self->visited = (MDTableHandle*)qwes_calloc(meta_count, sizeof(MDTableHandle));
    if (!self->visited) {
        QWES_LOG_ERROR("visited allocation");
        goto cleanup;
    }
#endif
    // We need these values to be set for initial validation.
    // If that validation fails, we will revert these changes in cleanup.
    self->semantics = sem;
    self->data_storage = data_storage;
    self->meta_storage = meta_storage;

    QWES_LOG_DEBUG("Max %zu entries of size %zu", self->meta_count, self->slot_len);
    QWES_LOG_DEBUG("meta_buf allocated (%zu)", self->meta_buf.len);
    QWES_LOG_DEBUG("data_buf allocated (%zu)", self->data_buf.len);

    // Now load the index
    if (Object_isERROR(readStore(self, INDEX_STORE))) {
        QWES_LOG_ERROR("initial data");
        goto cleanup;
    }
    if (Object_isERROR(readMeta(self)) || Object_isERROR(checkMeta(self))) {
        QWES_LOG_ERROR("initial meta");
        goto cleanup;
    }

    self->empty = true;
    // TODO:  We need to determine a policy for what to do if a mismatch is detected.
    // This may depend on the target.
    for (uint16_t i = 0; i < self->meta_count; i++) {
        UsefulBufC meta = getMetaC(self, i);
        UsefulBufC data = fetchSlotC(self, i);
        if (UsefulBuf_IsNULLC(meta) || UsefulBuf_IsNULLC(data)) {
            QWES_LOG_ERROR("validation: %u", i);
            continue;
        }
        bool meta_vacant = sem->isMetaVacant(sem, meta);
        bool data_vacant = sem->isDataVacant(sem, data);
        QWES_LOG_INFO("status at %u: %d/%d", i, meta_vacant, data_vacant);
        if (meta_vacant != data_vacant) {
            QWES_LOG_WARN("mismatch at %u: %d/%d", i, meta_vacant, data_vacant);
            if (data_vacant) {
                QWES_LOG_ERROR("NOT rebuilding MD");
            } else {
                QWES_LOG_ERROR("data is not vacant");
                if (meta_vacant) {
                    bool imported = sem->importLicense(self,
                                                       data, i);
                    if (imported == FALSE)
		    	QWES_LOG_INFO("License import %d", imported);
                }
                QWES_LOG_ERROR("NOT erasing MD");
            }
        } else if (!meta_vacant) {
            self->empty = false;
        }
    }

    return Object_OK;

// Secondary initialization failure handling.
cleanup:
    // Make sure subsequent calls to public methods bail out.
    self->semantics = NULL;
    // We're returning an error, so the client will be cleaning up the storages.
    self->data_storage = NULL;
    self->meta_storage = NULL;
    MDTable_destroy(self);

    return Object_ERROR;
}

static void destroyStorage(BlockStorage* storage) {
    if (storage) {
        storage->destroy(storage);
        qwes_free(storage);
    }
}

void MDTable_destroy(MDTable* self) {
    qwes_shred(&self->meta_buf);
    qwes_shred(&self->data_buf);
#if REPAIR_TOC
    if (self->visited) {
        qwes_free(self->visited);
        self->visited = NULL;
    }
#endif
    destroyStorage(self->data_storage);
    self->data_storage = NULL;
    destroyStorage(self->meta_storage);
    self->meta_storage = NULL;
}

static int32_t flush(MDTable* self, size_t handle) {
    int32_t err = Object_OK;
    // If we have a separate meta storage, we always flush data, then meta.
    if (self->meta_storage) {
        err = writeCurrentStore(self);
        if (Object_isOK(err)) {
            err = writeMeta(self);
        }
        return err;
    }
    // Meta is in data slot 0, so we may have to flush two data stores.
    if (INDEX_STORE != slotToStore(self, handle + firstDataSlot(self))) {
        // First save the current store which contains the license
        err = writeCurrentStore(self);
        if (!err) {
            // Then fetch the store that contains the index
            err = fetchStore(self, INDEX_STORE);
        }
    }
    // else, the index slot is the same as the license slot, and it's already current
    if (Object_isOK(err)) {
        err = copyMetaToData(self);
        if (Object_isOK(err)) {
            err = writeCurrentStore(self);
        }
    }
    return err;
}

int32_t MDTable_update(MDTable* self, MDTableHandle handle, UsefulBufC data) {
    if (!self->semantics) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }
    UsefulBuf meta_buf = getMeta(self, handle);
    UsefulBuf slot_buf = fetchSlot(self, handle);
    if (UsefulBuf_IsNULL(meta_buf) || UsefulBuf_IsNULL(slot_buf)) {
        QWES_LOG_ERROR("handle %d", handle);
        return Object_ERROR;
    }
    if (data.len > self->slot_len) {
        QWES_LOG_ERROR("data len %zu", data.len);
        return Object_ERROR;
    }
    // Generate meta-data from data, directly into index buffer
    MDSemantics const* sem = self->semantics;
    if (!sem) {
        // Should never happen
        QWES_LOG_ERROR("internal");
        return Object_ERROR;
    }
    int32_t err = sem->getMeta(sem, data, NULL, meta_buf);
    if (Object_isERROR(err)) {
        QWES_LOG_ERROR("getMeta");
        return err;
    }
    // Length was checked previously, so this should never fail
    Ext_UsefulBuf_Copy_Mod(&slot_buf, data);
    QWES_LOG_DEBUG("handle %u", handle);
    return flush(self, handle);
}

int32_t MDTable_updateMeta(MDTable* self,
                           MDTableHandle handle,
                           UsefulBufC meta,
                           UsefulBufC data) {
    UsefulBuf meta_buf = getMeta(self, handle);
    UsefulBuf slot_buf = fetchSlot(self, handle);
    if (UsefulBuf_IsNULL(meta_buf) || UsefulBuf_IsNULL(slot_buf)) {
        QWES_LOG_ERROR("handle %d", handle);
        return Object_ERROR;
    }
    if (data.len > slot_buf.len || meta.len > meta_buf.len) {
        QWES_LOG_ERROR("len %zu>%zu or %zu>%zu", data.len, slot_buf.len, meta.len,
                       meta_buf.len);
        return Object_ERROR;
    }
    memscpy(meta_buf.ptr, meta_buf.len, meta.ptr, meta.len);
    memscpy(slot_buf.ptr, slot_buf.len, data.ptr, data.len);
    return Object_OK;
}

int32_t MDTable_erase(MDTable* self, MDTableHandle handle) {
    if (!self->semantics) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }
    UsefulBuf meta_buf = getMeta(self, handle);
    UsefulBuf slot_buf = fetchSlot(self, handle);
    if (UsefulBuf_IsNULL(meta_buf) || UsefulBuf_IsNULL(slot_buf)) {
        QWES_LOG_ERROR("handle %d", handle);
        return Object_ERROR;
    }
    QWES_LOG_DEBUG("slot %u", handle);
    memset(meta_buf.ptr, 0, meta_buf.len);
    memset(slot_buf.ptr, 0, slot_buf.len);
    return flush(self, handle);
}

// Common code for install and installMeta
static int32_t findEmptySlot(MDTable* self, MDTableHandle* slot) {
    MDSemantics const* sem = self->semantics;
    if (!sem) {
        QWES_LOG_ERROR("init");
        return Object_ERROR;
    }
    size_t const entries = self->meta_count;
    size_t i = 0;
    UsefulBuf meta = NULLUsefulBuf;
    for (i = 0; i < entries; i++) {
        meta = getMeta(self, i);
        if (UsefulBuf_IsNULL(meta)) {
            QWES_LOG_ERROR("internal");
            return Object_ERROR;
        }
        if (sem->isMetaVacant(sem, UsefulBuf_Const(meta))) {
            break;
        }
    }
    if (entries == i) {
        QWES_LOG_WARN("full");
        return Object_ERROR;
    }
    *slot = i;
    return Object_OK;
}

int32_t MDTable_install(MDTable* self, UsefulBufC data) {
    if (!self->semantics) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }
    MDTableHandle slot = 0;
    int32_t err = findEmptySlot(self, &slot);
    if (Object_isERROR(err)) {
        return err;
    }
    QWES_LOG_DEBUG("slot %zu", slot);
    return MDTable_update(self, slot, data);
}

int32_t MDTable_installMeta(MDTable* self, UsefulBufC meta, UsefulBufC data, bool atomicTransaction) {
    if (!self->semantics) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }
    MDTableHandle slot = 0;
    int32_t err = findEmptySlot(self, &slot);
    if (Object_isERROR(err)) {
        return err;
    }
    QWES_LOG_DEBUG("slot %zu", slot);
    err = MDTable_updateMeta(self, slot, meta, data);
    if (Object_isERROR(err)) {
        return err;
    }
    if (!atomicTransaction) {
        err = flush(self, slot);
    }
    return err;
}

int32_t MDTable_updateRevocationList(MDTable* self, UsefulBufC revList) {
    if (!self->semantics) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }

    UsefulBuf revMetaBuf = getRevocationList(self);
    if (UsefulBuf_IsNULL(revMetaBuf)) {
        QWES_LOG_ERROR("revocation list meta null");
        return Object_ERROR;
    }
    if (revList.len > revMetaBuf.len) {
        QWES_LOG_ERROR("revList len %zu>%zu", revList.len, revMetaBuf.len);
        return Object_ERROR;
    }
    memscpy(revMetaBuf.ptr, revMetaBuf.len, revList.ptr, revList.len);
    if (!self->meta_storage) {
        QWES_LOG_ERROR("no mem revList meta");
        return Object_ERROR;
    }

    return Object_OK;
}

/**
 * @brief Call the visitor for each license in the store, rebuilding TOC if needed.
 *
 * This function will call the visitor.visit with
 * license meta-data and handle for each valid license in the store.  If the TOC
 * is found to be invalid or incorrect during enumeration, it will be re-built,
 * and any new licenses found during the rebuild process will then also be passed
 * to the visitor.
 */
int32_t MDTable_visit(MDTable* self, MDVisit visit, void* visitor, void* context) {
    MDSemantics const* sem = self->semantics;
    if (!sem) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }

    self->empty = true;
    int32_t err = Object_OK;
    int read_more = 1;
    for (size_t i = 0; i < self->meta_count && read_more && !err; i++) {
        UsefulBufC meta = getMetaC(self, i);
        if (!sem->isMetaVacant(sem, meta)) {
            self->empty = false;
            visit(visitor, context, i, meta, &read_more);
        }
    }
#if REPAIR_TOC
    // TODO.  Maybe
    if (!self->visited) {
        QWES_LOG_ERROR("init");
        return Object_ERROR;
    }
    for (size_t i = 0; i < LICENSE_MANAGER_SLOT_COUNT; i++) {
        seof->visited[i] = INVALID_LICENSE_HANDLE;
    }

    int mismatch = 0;
    int finished = 0;
    int err = visitAndCheck(self, visit, visitor, &finished, &mismatch);
    if (err) {
        return err;
    }
    // mismatch => client detected a problem, invalidToc => internal problem.
    if (mismatch || LicenseStore::get().invalidToc()) {
        QWES_LOG_WARN("mismatch (%d/%d)", mismatch, LicenseStore::get().invalidToc());
        rebuildToc(self, context);
        // See note on EnumerateAndCheck
        if (!finished) {
            // Enumerate again, skipping successfully-processed licenses.
            err = visitAndCheck(self, visit, visitor, &finished, &mismatch);
        }
    }
    if (err) {
        QWES_LOG_ERROR("failed");
    }
#endif
    return err;
}

int32_t MDTable_RevokeLicense(MDTable* self, MDTableHandle handle) {
     if (!self->semantics) {
        QWES_LOG_ERROR("uninitialized");
        return Object_ERROR;
    }
    UsefulBuf meta_buf = getMeta(self, handle);
    if (UsefulBuf_IsNULLOrEmpty(meta_buf)) {
        QWES_LOG_ERROR("handle %d", handle);
        return Object_ERROR;
    }
    QWES_LOG_DEBUG("slot %u", handle);
    memset(meta_buf.ptr, 0, meta_buf.len);

    return Object_OK;
}

int32_t MDTable_flushMeta(MDTable* self) {
    if (!self->meta_storage) {
        return Object_ERROR;
    }

    return writeMeta(self);
}

int32_t MDTable_flushData(MDTable* self) {
    return writeCurrentStore(self);
}

void MDTable_resetMeta(MDTable* self) {
    if (Object_isERROR(readMeta(self)) || Object_isERROR(checkMeta(self))) {
        QWES_LOG_ERROR("reset meta!");
        // Make sure subsequent calls to public methods bail out.
        self->semantics = NULL;
        // We're returning an error, so the client will be cleaning up the storages.
        self->data_storage = NULL;
        self->meta_storage = NULL;
        MDTable_destroy(self);
    }
}

UsefulBufC MDTable_getMeta(MDTable* self, MDTableHandle handle) {
    return getMetaC(self, handle);
}

UsefulBufC MDTable_getData(MDTable* self, MDTableHandle handle) {
    return fetchSlotC(self, handle);
}

UsefulBuf MDTable_getDataAsUB(MDTable* self, MDTableHandle handle) {
    return fetchSlot(self, handle);
}

UsefulBufC MDTable_getRevocationList(MDTable* self) {
    return getRevocationListC(self);
}
