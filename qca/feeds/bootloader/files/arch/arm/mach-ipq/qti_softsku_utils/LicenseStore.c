// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdlib.h>
#include <stdio.h>
#include "include/LicenseStore.h"
#include "include/MDTable.h"
#include "include/object.h"
#include "include/Utils.h"

#define OIDUB(bytes) \
    { (uint8_t[]) bytes, sizeof((uint8_t[])bytes) }

#define QWESLICSTORE_GPT_GUID                              \
    {                                                      \
        0x93, 0x3C, 0xAB, 0x7B, 0x73, 0x5F, 0x02, 0x4D,    \
        0xB8, 0xCB, 0x5B, 0x9F, 0x89, 0x9D, 0x29, 0xA8     \
    }


#define MAX_LICENSE_ID_LEN 16
//id len + id
#define THIN_TOC_ENTRY_SIZE (4 + MAX_LICENSE_ID_LEN)
#define LICENSE_MANAGER_SLOT_SIZE (4 * 1024)
#define LICENSE_DEVICE_ID_IS_SOC 1

typedef struct LicenseMeta {
    UsefulBufC identifier;
    uint8_t device_id_len;
    uint8_t device_id;
} LicenseMeta;


static const UsefulBufC gpt_data_guid = OIDUB(QWESLICSTORE_GPT_GUID);

int32_t LicenseStore_initForOffTargetUse(LicenseStore* self) {
    return LICENSE_STORE_SUCCESS;
}

void LicenseMeta_init(LicenseMeta* self) {
    self->identifier = NULLUsefulBufC;
    self->device_id_len = 0;
    self->device_id = 0;
}


static int32_t ThinToc_getMeta(MDSemantics const* self,
                               UsefulBufC data,
                               void* context,
                               UsefulBuf meta) {
    QWES_LOG_ERROR("Hello");
    return Object_ERROR;
}

static int getClientIdAndLicenseFromUB(UsefulBufC ub,
                                       UsefulBufC* client_id,
                                       UsefulBufC* license) {
    UsefulInputBuf in = {0};
    UsefulInputBuf_Init(&in, ub);

    // Client ID is optional
    UsefulBufC cid = NULLUsefulBufC;

#if 0
    // The following will produce a NULLUsefulBufC if any overflow occurs
    uint32_t len = UsefulInputBuf_GetUint32(&in);
    if (len) {
        cid = UsefulInputBuf_GetUsefulBuf(&in, len);
    }
#endif
    // Then read the license.
    uint32_t len = UsefulInputBuf_GetUint64(&in);
    //uint32_t temp = UsefulInputBuf_GetUint32(&in);
    // Zero-length license is invalid, but GetUsefulBuf will allow it
    if (len) {
        UsefulBufC lic = UsefulInputBuf_GetUsefulBuf(&in, len);
        if (!UsefulBuf_IsNULLC(lic)) {
            *client_id = cid;
            *license = lic;
            return 0;
        }
    }
    return -1;
}

static bool ThinToc_importLicense(void* context,
                                  UsefulBufC data,
                                  uint16_t handle) {
    QWES_LOG_ERROR("Not required");
    return false;
}


static bool ThinToc_isDataVacant(MDSemantics const* self, UsefulBufC data) {
    UsefulBufC client_id, license;
    if (getClientIdAndLicenseFromUB(data, &client_id, &license)) {
        return true;
    }
    return false;
}

/**
 * @brief decode the data from "in" and populate the given meta data
 *
 * On failure, the contents of meta are set to a safe empty value.
 *
 * @return non-zero if the input data was invalid in any way.
 */
static int readMetaFromBuffer(UsefulInputBuf* in, LicenseMeta* meta) {
    // Safe defaults
    meta->identifier = NULLUsefulBufC;
    meta->device_id_len = 0;
    meta->device_id = 0;

    // This variable can temporarily be invalid, since we are filling its members
    // with potentially random data.  The entryStatus call below performs sanity
    // checks so that invalid data can not "escape" into our in-memory TOC.
    LicenseMeta temp;
    LicenseMeta_init(&temp);
    temp.identifier.len = UsefulInputBuf_GetUint32(in);
    const uint8_t* ident =
        (const uint8_t*)(UsefulInputBuf_GetBytes(in, temp.identifier.len));
    if (!ident || !temp.identifier.len) {
        return -1;
    }
    temp.identifier.ptr = ident;
    temp.device_id_len = UsefulInputBuf_GetByte(in);
    if (temp.device_id_len) {
        temp.device_id = UsefulInputBuf_GetByte(in);
    }
    *meta = temp;
    return 0;
}

static bool ThinToc_isMetaVacant(MDSemantics const* self, UsefulBufC meta_buf) {
    LicenseMeta meta;
    LicenseMeta_init(&meta);
    UsefulInputBuf in = {0};
    UsefulInputBuf_Init(&in, meta_buf);
    return readMetaFromBuffer(&in, &meta);
}

MDTable* LicenseStore_get(void) {
    static MDTable singleton = {0};
    return &singleton;
}

int32_t LicenseStore_init(LicenseStore* self, QWESStore store) {
    if (!self) {
        return LICENSE_STORE_INVALID_ARGUMENTS;
    }

    if (!store.open_partition ||
        //!store.device_init ||
        //!store.storage_init ||
        //!store.device_get_info ||
        !store.client_get_info ||
        //!store.add_partition ||
        !store.read ||
        !store.write ||
	!store.close) {
        return LICENSE_STORE_INVALID_ARGUMENTS;
    }

    if (!self->storage) {
        /**
            Partition information , GUID and storage type is not required
	    as store APIs wil take care of it.
        */
        PartitionInfo info = {0, 0, 0};

        self->storage = DataMgrStorage_new(store, gpt_data_guid, 0, false, info);

        if (!self->storage) {
            return LICENSE_STORE_NO_MEMORY;
        }
    }

    static const MDSemantics LicenseSem = {
        // See WriteMetaToBuffer.
        .meta_len = THIN_TOC_ENTRY_SIZE,
        .data_len = LICENSE_MANAGER_SLOT_SIZE,
        // version 0 for backwards compatibility
        .version = 0,
        .name = "License Store (legacy)",
        .getMeta = ThinToc_getMeta,
        .isMetaVacant = ThinToc_isMetaVacant,
        .isDataVacant = ThinToc_isDataVacant,
        .importLicense = ThinToc_importLicense,
    };

    // Sanity checks
    // Fat ToC should always be stored separately, even if both
    // licenses and fat ToC are stored in RPMB.
    //
    // At least one storage must be available
    MDSemantics const* sem = &LicenseSem;
    return MDTable_init(LicenseStore_get(), sem, (BlockStorage*)(self->storage), NULL);
}

int32_t install_license (void* visitor,
                           void* context,
                           MDTableHandle handle,
                           UsefulBufC meta,
                           int* read_more)
{

   //LicenseStore* self = (LicenseStore*)visitor;
   UsefulBufC client_id = NULLUsefulBufC;
   UsefulBufC license = NULLUsefulBufC;

    // Parse meta to check device_id_len and device_id
    LicenseMeta lic_meta;
    LicenseMeta_init(&lic_meta);
    UsefulInputBuf in = {0};
    UsefulInputBuf_Init(&in, meta);
    readMetaFromBuffer(&in, &lic_meta);

    // If device_id_len is present, only install licenses of soc
    if (lic_meta.device_id_len && lic_meta.device_id != LICENSE_DEVICE_ID_IS_SOC) {
        return LICENSE_STORE_SUCCESS;
    }

    // Get key blob from stored data
    UsefulBufC slot = MDTable_getData(LicenseStore_get(), handle);
    if (!UsefulBuf_IsNULLOrEmptyC(slot)) {
        getClientIdAndLicenseFromUB(slot, &client_id, &license);
	/* call install license ssg call*/
	qwes_install_license((void*)license.ptr, license.len, handle);
    }

    return LICENSE_STORE_SUCCESS;
}


int32_t LicenseStore_installAllLicenses(LicenseStore* self) {

    int32_t status = LICENSE_STORE_SUCCESS;

    status = MDTable_visit(LicenseStore_get(), install_license, (void*)self,
                         NULL);

    return status;
}

int32_t LicenseStore_appendNewLicense(LicenseStore* self,
                                      uint64_t licenseId,
                                      const void* license_ptr,
                                      size_t license_len) {
    return LICENSE_STORE_SUCCESS;
}

int32_t LicenseStore_deleteLicensesById(LicenseStore* self,
                                        uint64_t* licenseIds_ptr,
                                        size_t licenseIds_len) {
    return LICENSE_STORE_SUCCESS;
}

int32_t LicenseStore_deinit(LicenseStore* self) {
    if (!self) {
        return LICENSE_STORE_INVALID_ARGUMENTS;
    }

#if 0
    if (self->storage) {
        self->storage->base.destroy(&self->storage->base);
        qwes_free(self->storage);
        self->storage = NULL;
    }
#endif

    MDTable_destroy(LicenseStore_get());
    return LICENSE_STORE_SUCCESS;
}
