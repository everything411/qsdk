// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#pragma once
#include <stdbool.h>
#include "BlockStorage.h"
#include "DataStoreMgr.h"
#include "qwes_store.h"
#include "UsefulBuf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    BlockStorage base;
    DataMgrHandle handles[MAX_STORE_INSTANCES];
    QWESStore store;
    bool init;
} DataMgrStorage;

typedef struct {
    uint32_t partition;
    uint32_t size;
    uint32_t count;
} PartitionInfo;

DataMgrStorage* DataMgrStorage_new(QWESStore store,
                                   UsefulBufC guid,
                                   uint32_t storage_type,
                                   bool read_only,
                                   PartitionInfo info);

#ifdef __cplusplus
}
#endif
