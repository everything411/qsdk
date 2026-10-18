// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdlib.h>
#include <stdio.h>
#include "include/DataMgrStorage.h"
#include "include/DataStoreMgr.h"
#include "include/qwes_store.h"
#include "include/Utils.h"

/** QWES License Store partition - Minimum DataStores Supported */
#define MIN_PARTITION_COUNT 1

/** RPMB access using DataMgr. */
#define DM_STORE_COPIES 2
#define DM_SECTOR_FOR_HEADER 1
#define DM_SECTOR_FOR_FOOTER 1
#define DM_SECTOR_FOR_META_HEADER 1
#define DM_SECTORS_OVERHEAD (DM_STORE_COPIES * (DM_SECTOR_FOR_HEADER + \
                             DM_SECTOR_FOR_FOOTER + DM_SECTOR_FOR_META_HEADER))

#ifdef OFF_TARGET
#include "qsee_fs.h"
static uint32_t DMInit(DataMgrHandle* HandlePtr, TableInfoType* TableInfoPtr) {
    return TableInfoPtr->PartitionType == 0 ? DATA_STORE_SUCCESS : DATA_STORE_ABORTED;
}

static uint32_t DMOpen(DataMgrHandle* HandlePtr, TableInfoType* TableInfoPtr) {
    return TableInfoPtr->PartitionType == 0 ? DATA_STORE_SUCCESS : DATA_STORE_ABORTED;
}

static uint32_t DMClose(DataMgrHandle Handle) {
    return DATA_STORE_SUCCESS;
}

static uint32_t DMWrite(DataMgrHandle Handle, uint32_t LbaCount, void* Buffer) {
    return DATA_STORE_ABORTED;
}

static uint32_t DMRead(DataMgrHandle Handle,
                         uint32_t Lba,
                         uint32_t LbaCount,
                         void* Buffer) {
    QWES_LOG_DEBUG("read %u +> %u", Lba, LbaCount);
    int fd = open("/mnt/vendor/persist/data/qweslicstore.bin", O_RDONLY);
    if (fd >= 0) {
        // Skip header
        lseek(fd, 0x2000, SEEK_SET);
        size_t bytes = read(fd, Buffer, LbaCount * 4096);
        close(fd);
        QWES_LOG_DEBUG("read %zu bytes", bytes);
        return bytes == LbaCount * 4096 ? DATA_STORE_SUCCESS : DATA_STORE_ABORTED;
    }
    return DATA_STORE_ABORTED;
}

static uint32_t DMGetInfo(DataMgrHandle Handle,
                            uint32_t* LbaCount,
                            uint32_t* LbaSize) {
    *LbaCount = 12;
    *LbaSize = 4096;
    return DATA_STORE_SUCCESS;
}
#else
#define DMInit DataMgrInit
#define DMOpen DataMgrOpen
#define DMClose DataMgrClose
#define DMRead DataMgrRead
#define DMWrite DataMgrWrite
#define DMGetInfo DataMgrGetInfo
#endif

// Validate Input Parameters for read and write, and calculate slot_count
int DataMgrStorage_checkParams(DataMgrStorage* self,
                               size_t store,
                               uint8_t const* data,
                               size_t len,
                               uint32_t* slot_count) {
    if (!self->init || !data) {
        return -1;
    }
    if (store >= self->base.store_count || len > self->base.store_len) {
        return -1;
    }
    if ((len < self->base.block_size) || (len % self->base.block_size != 0)) {
        return -1;
    }
    *slot_count = len / self->base.block_size;
    return 0;
}

/**
Reads the data from the given DataStore.
*/
static int32_t DataMgrStorage_read(BlockStorage* base,
                                   size_t store,
                                   uint8_t* data,
                                   size_t len) {
    DataMgrStorage* self = (DataMgrStorage*)base;
    uint32_t slot_count = 0;
    if (DataMgrStorage_checkParams(self, store, data, len, &slot_count)) {
        //QWES_LOG_ERROR("internal: %zu, %p, %zu", store, data, len);
        return DATA_STORE_GENERIC_ERROR;
    }
    return DMRead(self->handles[store], 0, slot_count, data);
}

/**
Write the data to the given DataStore.
*/
int32_t DataMgrStorage_write(BlockStorage* base,
                             size_t store,
                             const uint8_t* data,
                             size_t len) {
    DataMgrStorage* self = (DataMgrStorage*)base;
    uint32_t slot_count = 0;
    if (DataMgrStorage_checkParams(self, store, data, len, &slot_count)) {
        //QWES_LOG_ERROR("internal: %zu, %p, %zu", store, data, len);
        return DATA_STORE_GENERIC_ERROR;
    }
    // Note:  have to cast away the const because DataMgrWrite doesn't have it
    return DMWrite(self->handles[store], slot_count, (void*)data);
}

/**
 * Attempt to open the given partition.
 * There is a 1:1 mapping between partition_id and store_id.
 * On success, this function will store the opened handle for further use.
 */
static int32_t DataMgrStorage_probePartition(DataMgrStorage* self,
                                             UsefulBufC guid,
                                             uint32_t storage_type,
                                             uint32_t partition_id,
                                             uint32_t partition_offset) {
    DataMgrHandle handle = NULL;
    TableInfoType info;
    memset(&info, 0, sizeof(TableInfoType));

    info.DeviceId = storage_type;
    memscpy(info.PartitionGuid, sizeof(info.PartitionGuid), guid.ptr, guid.len);
    
#if 0
    if (QWES_STORE_EMMC_RPMB == storage_type) {
        info.PartitionType = partition_id + partition_offset;
    } else {
        info.PartitionType = partition_id;
    }
#endif
    info.PartitionType = partition_id;

    uint32_t status = DMOpen(&handle, &info, self->store);
    if (status != DATA_STORE_SUCCESS) {
        //QWES_LOG_ERROR("open(%d) failed with %u", partition_id, status);
    }
    if (status == DATA_STORE_VOLUME_CORRUPTED) {
        status = DMInit(&handle, &info, self->store);
        if (status != DATA_STORE_SUCCESS) {
            //QWES_LOG_ERROR("init(%d) failed with %u", partition_id, status);
        }
    }
    if (status != DATA_STORE_SUCCESS) {
        return DATA_STORE_GENERIC_ERROR;
    }

    uint32_t slot_count, slot_size;
    status = DMGetInfo(handle, &slot_count, &slot_size);
    if (DATA_STORE_SUCCESS == status) {
        //QWES_LOG_INFO("probe(%u) succeeded", partition_id);
        self->handles[partition_id] = handle;
        if (!self->base.store_len) {
            // First partition
            self->base.store_len = slot_count * slot_size;
            self->base.block_size = slot_size;
        } else {
            // Nth partition.  Sanity check.
            if (self->base.store_len != slot_count * slot_size ||
                self->base.block_size != slot_size) {
                //QWES_LOG_ERROR("Partition mismatch: %zu/%zu vs %u*%u",
                               //self->base.store_len, self->base.block_size, slot_count,
                               //slot_size);
            }
        }
    } else {
        DMClose(handle);
    }
    return status;
}

void DataMgrStorage_destroy(BlockStorage* base) {
    DataMgrStorage* self = (DataMgrStorage*)base;
    // Close open handles
    // Should only have to go to store_count, but just in case...
    for (size_t i = 0; i < MAX_STORE_INSTANCES; i++) {
        if (self->handles[i] != NULL) {
            DMClose(self->handles[i]);
            self->handles[i] = NULL;
        }
    }
}

#if 0
static int32_t roundUp(size_t value, size_t modulus, size_t* result) {
    if (0 == modulus || 0 == value) {
        printf("Invalid range");
        return DATA_STORE_GENERIC_ERROR;
    }
    size_t remainder = value % modulus;
    size_t needed = remainder ? modulus - remainder : 0;
    if (value <= SIZE_MAX - needed) {
        *result = value + needed;
        return DATA_STORE_SUCCESS;
    }
    printf("overflow, %zu + %zu", value, needed);
    return DATA_STORE_GENERIC_ERROR;
}

static int32_t DataMgrStorage_InitPartition(DataMgrStorage* self,
                                            uint32_t partition,
                                            size_t size) {
    // device handle is no longer needed after we open the partition
    qwes_store_device_handle_t dev = 0;
    qwes_store_client_handle_t client = 0;
    qwes_store_device_info_t dev_info = {0};
    int32_t qwes_err = QWES_STORE_ERROR;

    // Init Device
    qwes_err = self->store.device_init(QWES_STORE_EMMC_RPMB, NULL, &dev);
    if (qwes_err != QWES_STORE_SUCCESS) {
        //QWES_LOG_ERROR("init %d", qwes_err);
        return DATA_STORE_GENERIC_ERROR;
    }
    qwes_err = self->store.device_get_info(&dev, &dev_info);
    if (qwes_err || 0 == dev_info.bytes_per_sector) {
        //QWES_LOG_ERROR("get_info err %d bps %u", qwes_err, dev_info.bytes_per_sector);
        return DATA_STORE_GENERIC_ERROR;
    }

    // Round off the given input size to a multiple of sector size
    if (roundUp(size, dev_info.bytes_per_sector, &size)) {
        return DATA_STORE_GENERIC_ERROR;
    }
    // Calculate the number of sectors
    size_t sectors = (((size * DM_STORE_COPIES)/dev_info.bytes_per_sector)
                                                    + DM_SECTORS_OVERHEAD);

    // Open Partition and collect client info
    qwes_err = self->store.open_partition(&dev, partition, &client);
    if (QWES_STORE_SUCCESS == qwes_err) {
        //QWES_LOG_INFO("Opened existing partition 0x%x", partition);
        // Partition exists.  Make sure its size is what we expect.
        qwes_store_client_info_t client_info = {0};
        qwes_err = self->store.client_get_info(&client, &client_info);
        if (dev_info.bytes_per_sector != client_info.bytes_per_sector ||
            sectors != client_info.total_sectors) {
            //QWES_LOG_ERROR("mismatch: %u*%lu vs %u*%u", dev_info.bytes_per_sector, sectors,
              //             client_info.bytes_per_sector, client_info.total_sectors);
            return DATA_STORE_GENERIC_ERROR;
        }
    } else {
        // Open failed.  Attempt to create partition
        //QWES_LOG_INFO("Creating partition 0x%x", partition);

        // Check sectors available or not
        if (sectors > dev_info.available_sectors) {
            //QWES_LOG_ERROR("%u sectors, %lu needed", dev_info.available_sectors, sectors);
            return DATA_STORE_GENERIC_ERROR;
        }

        // Create and open partition
        qwes_err = self->store.add_partition(&dev, partition, sectors);
        if (qwes_err) {
            //QWES_LOG_ERROR("add_partition %d", qwes_err);
            return DATA_STORE_GENERIC_ERROR;
        }
        qwes_err = self->store.open_partition(&dev, partition, &client);
        if (qwes_err) {
            //QWES_LOG_ERROR("open_partition %d", qwes_err);
            return DATA_STORE_GENERIC_ERROR;
        }
    }

    return DATA_STORE_SUCCESS;
}

static int32_t DataMgrStorage_CreatePartitions(DataMgrStorage* self,
                                               uint32_t partition,
                                               size_t size,
                                               size_t count) {
    if (self->store.storage_init(size, count)) {
        //QWES_LOG_ERROR("SFS storage initialization failed");
        return DATA_STORE_GENERIC_ERROR;
    }

    // Supports creating multiple partitions of similar size.
    for (int i = 0; i < count; i++) {
        if (DataMgrStorage_InitPartition(self, (partition+i), size)) {
            return DATA_STORE_GENERIC_ERROR;
        }
    }

    return DATA_STORE_SUCCESS;
}
#endif

int32_t DataMgrStorage_init(DataMgrStorage* self,
                            QWESStore store,
                            UsefulBufC guid,
                            uint32_t storage_type,
                            bool read_only,
                            PartitionInfo info) {
    // Check for re-initialization
    if (self->init) {
        return DATA_STORE_SUCCESS;
    }

    self->store = store;
#if 0
    if (QWES_STORE_EMMC_RPMB == storage_type) {
        if (DataMgrStorage_CreatePartitions(self, info.partition, info.size, info.count)) {
            return DATA_STORE_GENERIC_ERROR;
        }
    }
#endif

    // Probe the partition table
    uint32_t count;
    for (count = 0; count < MAX_STORE_INSTANCES; count++) {
        if (DATA_STORE_SUCCESS != DataMgrStorage_probePartition(self, guid, storage_type,
                                                       count, info.partition)) {
            break;
        }
    }
    if (count < MIN_PARTITION_COUNT) {
        // We must have at least one partition
        //QWES_LOG_ERROR("No partitions found for the given GUID");
        return DATA_STORE_GENERIC_ERROR;
    }

    self->base.store_count = count;
    self->base.read_only = read_only;
    self->base.read = DataMgrStorage_read;
    self->base.write = DataMgrStorage_write;
    self->base.destroy = DataMgrStorage_destroy;
    self->init = true;
    return DATA_STORE_SUCCESS;
}

DataMgrStorage* DataMgrStorage_new(QWESStore store,
                                   UsefulBufC guid,
                                   uint32_t storage_type,
                                   bool read_only,
                                   PartitionInfo info) {
    DataMgrStorage* retval = (DataMgrStorage*)qwes_malloc(sizeof(DataMgrStorage));
    if (retval &&
        DataMgrStorage_init(retval, store, guid, storage_type,
                            read_only, info)) {
        qwes_free(retval);
        retval = NULL;
    }
    return retval;
}
