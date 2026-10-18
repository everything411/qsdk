// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __DATASTORE_MGR_H__
#define __DATASTORE_MGR_H__

#include "VarDataStoreError.h"
#include "qwes_store.h"
#include <linux/types.h>

/* U-Boot compatible type definitions */
#ifndef uint8_t
#define uint8_t u8
#endif
#ifndef uint32_t
#define uint32_t u32
#endif

#define DATA_STORE_SUCCESS               0
#define DATA_STORE_GENERIC_ERROR         1
#define DATA_STORE_INVALID_PARAMETER     10
#define DATA_STORE_UNSUPPORTED           11
#define DATA_STORE_BAD_BUFFER_SIZE       12
#define DATA_STORE_BUFFER_TOO_SMALL      13
#define DATA_STORE_DEVICE_ERROR          14
#define DATA_STORE_WRITE_PROTECTED       15
#define DATA_STORE_OUT_OF_RESOURCES      16
#define DATA_STORE_VOLUME_CORRUPTED      17
#define DATA_STORE_NOT_FOUND             18
#define DATA_STORE_ABORTED               19
#define DATA_STORE_INCOMPATIBLE_VERSION  20
#define DATA_STORE_SECURITY_VIOLATION    21

/* Define this macro to max number of instances of tables that are needed
 * by the driver. Memory consumption can be reduced by having less number
 * of instances. */
#ifndef MAX_STORE_INSTANCES
  #define MAX_STORE_INSTANCES      1
#endif

/* Minimum number of blocks required for a storage table
   - Storage Table Header, Info Block, Guid Table, Record Table,
     Data Record, Block Table
*/
#define MIN_STORAGE_TABLE_BLOCKS        6

#define DATA_STORE_LATEST_VERSION  0x0103

/* Handle for data store */
typedef struct DataStoreCtx* DataMgrHandle;

typedef struct
{
  uint32_t DeviceId;          // Device ID as defined by the storage interface
  uint8_t  PartitionGuid[16]; // Guid corresponding to physical partition on storage
  uint32_t PartitionType;     // Partition Type identifier within the physical
} TableInfoType;

/*
Opens the partition specified by TableInfoPtr, parsing
raw header/footer information and determining latest
valid copy as operating buffer.

return DATA_STORE_VOLUME_CORRUPTED if data corruptoin is observed
return DATA_STORE_DEVICE_ERROR if storage functions failed
return DATA_STORE_SUCCESS if partition was opened

updates gVarDataStorageError
*/
uint32_t
DataMgrOpen (DataMgrHandle*  DataMgrHandle,
             TableInfoType*  TableInfoPtr,
             QWESStore       store);


/*
Initializes the partition specificed by TableInfoPtr.
Creates valid headers and footers that will be used during
DataMgrWrite

return DATA_STORE_SUCCESS if context was properly initialized
*/
uint32_t
DataMgrInit(DataMgrHandle* DataMgrHandle,
            TableInfoType* TableInfoPtr,
	    QWESStore store);


/*
Reads the data from the latest valid copy into Buffer

return DATA_STORE_DEVICE_ERROR if storage functions failed
return DATA_STORE_SUCCESS if data was read

updates gVarDataStorageError with qsee_stor error code
*/
uint32_t
DataMgrRead (DataMgrHandle      Handle,
             uint32_t             Slot,
             uint32_t             SlotCount,
             void*              Buffer);


/*
Writes the data in Buffer to the latest valid copy
Updates header and footer with latest information

return DATA_STORE_DEVICE_ERROR if storage functions failed
return DATA_STORE_SUCCESS if data was written

updates gVarDataStorageError with qsee_stor error code
*/
uint32_t
DataMgrWrite (DataMgrHandle     Handle,
              uint32_t             SlotCount,
              void*              Buffer);


/*
Returns partition information to SlotCount and SlotSize
acquired during inital read of partition
*/
uint32_t
DataMgrGetInfo (DataMgrHandle   Handle,
                uint32_t         *SlotCount,
                uint32_t         *SlotSize);


/* Frees context pointed to by DataMgrHandle */
uint32_t
DataMgrClose (DataMgrHandle     Handle);


#endif /* __DATASTORE_MGR_H__ */
