// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __DATASTORE_ERR_H__
#define __DATASTORE_ERR_H__

/* Success return codes */
#define CMP_SUCCESS            0
#define INFO_BLK_VALID_SUCCESS 0
#define QSEE_STOR_SUCCESS      0

/* Data Mgr return codes */
#define DATA_STR_NOT_INITIALIZED    0x100
#define DATA_STR_DATA_CORRUPTION    0x101
#define DATA_STR_INVALID_HEADERS    0x102
#define DATA_STR_INAPPROPRIATE_AGES 0x103
#define DATA_STR_VERSION_MISMATCH   0x104
#define DATA_STR_MEMORY_CORRUPTION  0x105

/* Var store return codes */
#define VAR_STR_NOT_INITIALIZED   0x106
#define VAR_STR_DATA_CORRUPTION   0x107

extern int gVarDataStorageError;

#endif /* __DATASTORE_ERR_H__ */
