// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#pragma once

#include "DataMgrStorage.h"
#include "qwes_store.h"

#define LICENSE_STORE_SUCCESS            0
#define LICENSE_STORE_GENERIC_ERROR      1
#define LICENSE_STORE_OUT_BUF_TOO_SMALL  4
#define LICENSE_STORE_NO_MEMORY          5
#define LICENSE_STORE_INVALID_ARGUMENTS  10

// Do not modify the member variables
typedef struct LicenseStore {
    DataMgrStorage* storage;
} LicenseStore;

int32_t LicenseStore_init(LicenseStore* self, QWESStore store);

int32_t LicenseStore_initForOffTargetUse(LicenseStore* self);

int32_t LicenseStore_installAllLicenses(LicenseStore* self);

int32_t LicenseStore_appendNewLicense(LicenseStore* self,
                                      uint64_t licenseId,
                                      const void* license_ptr,
                                      size_t license_len);

int32_t LicenseStore_deleteLicensesById(LicenseStore* self,
                                        uint64_t* licenseIds_ptr,
                                        size_t licenseIds_len);

int32_t LicenseStore_deinit(LicenseStore* self);
