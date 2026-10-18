// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "include/DataStoreMgr.h"
#include "include/Crc32.h"
#include "include/VarDataStoreError.h"
#include "include/Utils.h"

/* Write method, currently only supported type is simple ping pong */
#define WRITE_METHOD_SIMPLE_PING_PONG_TYPE    1

/* Max data store copies. Ideally should be max 2 */
#define MAX_DATA_STORE_COPIES                 2

/* Data store signature */
#define DATA_STORE_MGR_SIGNATURE   ('D' | ('S' << 8) | ('t' << 16) | ('r' << 24))

/* Max age value of the info Block.
 * NOTE: Do not change, code has tight dependency on this */
#define MAX_AGE_VALUE                          4

/* Macros to help pack 2 ages for comparison and selection of latest ones */
#define PACK_AGES(x,y)       ((x & (MAX_AGE_VALUE - 1)) | ((y & (MAX_AGE_VALUE - 1)) << 2))

#define INVALID_LATEST_STORE_VALUE     0xFFFF

/* Info block stored in the data store */
typedef struct __attribute__((__packed__)) {
  uint32_t        Signature;      // Signature
  uint16_t        Version;        // Data store Version
  uint16_t        Age;            // Age of the data store
  uint16_t        WriteMethod;    // Write method followed for this store
  uint16_t        CfgDataSize;    // Size of any client's config data that's stored (TBD)
  uint16_t        MaxPayloadSectors;// Max available data blocks (debug info only)
  uint16_t        ActivePayloadSectors; //active sectors in use
  uint32_t        SyncCount;      // Counter field  (debug info only)
  uint64_t        Reserved0;
  uint64_t        Reserved1;      // Reserved for future enhancements
  uint32_t        Crc32;          // CRC32 covering all the 508 bytes of this header
} DataStoreInfo;

#define CONTEXT_SIGNATURE      0xDA7AC787

/* Data store context to manage multiple clients */
typedef  struct{
  uint32_t                      ContextSignature;
  DataStoreInfo               *StoreInfoPtr;        // Info block
  qwes_store_device_handle_t   mDeviceHandle;// = NULL;
  qwes_store_client_handle_t   mClientHandle;// = NULL;
  qwes_store_client_info_t     mClientInfo;

  uint32_t                      MaxDataSectors;
  uint32_t                      PartitionSlotCount;
  uint32_t                      DataSectorSize;

  uint16_t                      LatestValidStore; // Latest copy that's valid
  uint8_t                       InUse;            // This context struct is in use
  uint8_t                       WriteHappened;

  TableInfoType               TableInfo;

  uint32_t                      Headers[MAX_DATA_STORE_COPIES];  // Headers block offset
  uint32_t                      Footers[MAX_DATA_STORE_COPIES];  // Headers block offset
  uint32_t                      DataLoc[MAX_DATA_STORE_COPIES];  // Headers block offset
  DataStoreInfo              *CurrentHeader;
  DataStoreInfo              *TempHeader;

  QWESStore                   store;
}DataStoreCtx;

#define IS_CONTEXT_VALID(h)    ((h)->ContextSignature == CONTEXT_SIGNATURE)

/* Define as many instances we need to support */
DataStoreCtx StoreCtx[MAX_STORE_INSTANCES];

/* If all operating conditions have enough stack put this on stack to save 1KB
 * Since these are needed only once during initialization */

static
int DeallocateCtx (DataStoreCtx **CtxPtr)
{
  memset(*CtxPtr, 0, sizeof(DataStoreCtx));
  return DATA_STORE_SUCCESS;
}

/* Allocate a free context */
static
int AllocateCtx (DataStoreCtx** CtxPtr)
{
  int i;

  if (!CtxPtr)
    return DATA_STORE_INVALID_PARAMETER;

  for (i = 0; i < MAX_STORE_INSTANCES; ++i)
  {
    if (StoreCtx[i].InUse == 0)
    {
      /* Clear the context */
      memset(&StoreCtx[i], 0, sizeof(DataStoreCtx));

      StoreCtx[i].InUse = 1;
      *CtxPtr = &StoreCtx[i];
      return DATA_STORE_SUCCESS;
    }
  }
  return DATA_STORE_OUT_OF_RESOURCES;
}

static
int ValidateOldInfoBlk (DataStoreInfo * InfBlkp)
{
  uint32_t CrcVal, OldStructCrcSize;
  uint32_t *CurrentCrc = NULL;
  switch(InfBlkp->Version)
  {
    case 0x0100:
    case 0x0101:
    case 0x0102:
      //old version had crc at end of 512B block
      OldStructCrcSize = 0x200 - sizeof(uint32_t);
      CurrentCrc = (uint32_t *) (uintptr_t)(((uintptr_t)InfBlkp + OldStructCrcSize));
      qwes_CalculateCrc32 ((uint8_t*) InfBlkp,
                      OldStructCrcSize,
                      &CrcVal);
      if (CrcVal != *CurrentCrc)
      {
        gVarDataStorageError = DATA_STR_DATA_CORRUPTION;
        return DATA_STR_DATA_CORRUPTION;
      }
      else
        return 0;

    default:
      gVarDataStorageError = DATA_STR_DATA_CORRUPTION;
      return DATA_STR_DATA_CORRUPTION;
  }
}

static
int CompareMemZero(const void *MemoryBlock, uint32_t MemSize)
{
    uint32_t i;
    uint8_t *Block = (uint8_t*)MemoryBlock;
    for (i = 0; i < MemSize; i++)
    {
        if (Block[i] != 0)
            return -1;
    }
    return 0;
}

/* Validate info block with CRC */
static
int ValidateInfoBlk (DataStoreInfo * InfBlkp)
{
  uint32_t CrcVal = 0;
  uint32_t MemSize = sizeof(DataStoreInfo);

  if (CompareMemZero(InfBlkp, MemSize) == CMP_SUCCESS)
  {
    return DATA_STR_NOT_INITIALIZED;
  }

  if ((InfBlkp->Signature != DATA_STORE_MGR_SIGNATURE) ||
      (InfBlkp->Version > DATA_STORE_LATEST_VERSION))
  {
    return DATA_STR_DATA_CORRUPTION;
  }

  if ((InfBlkp->Version < DATA_STORE_LATEST_VERSION) &&
      (InfBlkp->Version >= 0x100))
  {
    return ValidateOldInfoBlk (InfBlkp);
  }

  //TODO: Version check for backward compatibility
  qwes_CalculateCrc32 ((uint8_t*)InfBlkp,
                  (uintptr_t)(&InfBlkp->Crc32) - (uintptr_t)InfBlkp,
                  &CrcVal);
  if (CrcVal != InfBlkp->Crc32)
  {
    return DATA_STR_DATA_CORRUPTION;
  }
  return 0;
}

/* Udpate CRC */
static
void UpdateHdrCRC (DataStoreInfo* InfoPtr)
{
  uint32_t CrcVal = 0;

  if (InfoPtr->Version != DATA_STORE_LATEST_VERSION)
    InfoPtr->Version = DATA_STORE_LATEST_VERSION;
  qwes_CalculateCrc32 ((uint8_t*)InfoPtr,
                  (uintptr_t)(&InfoPtr->Crc32) - (uintptr_t)InfoPtr,
                  &CrcVal);
  InfoPtr->Crc32 = CrcVal;
}

/* Advance Age and udpate CRC */
static
void UpdateHdrAgeAndCRC (DataStoreInfo* InfoPtr)
{
  uint32_t CrcVal = 0;

  if (InfoPtr->Version != DATA_STORE_LATEST_VERSION)
    InfoPtr->Version = DATA_STORE_LATEST_VERSION;

  /* Update sync counter */
  InfoPtr->SyncCount++;

  /* Update the hdr Age and CRC */
  InfoPtr->Age++;
  if (InfoPtr->Age >= MAX_AGE_VALUE)
    InfoPtr->Age = 0;
  qwes_CalculateCrc32 ((uint8_t*)InfoPtr,
                  (uintptr_t)(&InfoPtr->Crc32) - (uintptr_t)InfoPtr,
                  &CrcVal);
  InfoPtr->Crc32 = CrcVal;
}

/*
 *
 *     PUBLIC ROUTINES
 *
 * */

#define HANDLE_TO_CONTEXT(h)  ((DataStoreCtx*)h)


/* Read of the complete data need to be done to cache in memory, If full data is not read
 * then write would overwrite the storage device */
uint32_t
DataMgrRead (DataMgrHandle      Handle,
             uint32_t             Slot,
             uint32_t             SlotCount,
             void*              Buffer)
{
  if (Handle == NULL)
    return DATA_STORE_INVALID_PARAMETER;

  DataStoreCtx *DataStore = HANDLE_TO_CONTEXT(Handle);
  uint32_t Status = DATA_STORE_SUCCESS;
  if (!IS_CONTEXT_VALID(DataStore) ||
      (SlotCount > DataStore->MaxDataSectors) ||
      ((Slot + SlotCount) > DataStore->Footers[DataStore->LatestValidStore]) ||
      (Buffer == NULL))
    return DATA_STORE_INVALID_PARAMETER;

  Slot = Slot + DataStore->DataLoc[DataStore->LatestValidStore];
  Status  = DataStore->store.read((void*)DataStore->mClientHandle, Slot, SlotCount, Buffer);
  if (Status != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    return DATA_STORE_DEVICE_ERROR;
  }

  return DATA_STORE_SUCCESS;
}


uint32_t
DataMgrWrite (DataMgrHandle      Handle,
              uint32_t             SlotCount,
              void*              Buffer)
{
  if (Handle == NULL)
    return DATA_STORE_INVALID_PARAMETER;

  DataStoreCtx *DataStore = HANDLE_TO_CONTEXT(Handle);
  uint32_t Status = DATA_STORE_SUCCESS;
  uint32_t NewLvs, Slot;
  if (!IS_CONTEXT_VALID(DataStore) ||
      (SlotCount > DataStore->MaxDataSectors) ||
      (Buffer == NULL))
  {
    return DATA_STORE_INVALID_PARAMETER;
  }

  DataStore->StoreInfoPtr->ActivePayloadSectors = SlotCount;
  // Update Header/Footer information
  UpdateHdrCRC (DataStore->StoreInfoPtr);

  NewLvs = DataStore->LatestValidStore + 1;
  if (NewLvs >= MAX_DATA_STORE_COPIES)
    NewLvs = 0;

  if (DataStore->store.write ((void*)DataStore->mClientHandle,
                  DataStore->Headers[NewLvs],
                  1,
                  (uint8_t*)DataStore->StoreInfoPtr) != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    return DATA_STORE_DEVICE_ERROR;
  }

  Slot = DataStore->DataLoc[NewLvs];

  Status = DataStore->store.write((void*)DataStore->mClientHandle,
                      Slot,
                      SlotCount,
                      Buffer);
  if (Status != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    return DATA_STORE_DEVICE_ERROR;
  }

  //now we can update footer

  if (DataStore->store.write ((void*)DataStore->mClientHandle,
                  DataStore->Footers[NewLvs],
                  1,
                  (uint8_t*)DataStore->StoreInfoPtr) != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    return DATA_STORE_DEVICE_ERROR;
  }

  UpdateHdrAgeAndCRC (DataStore->StoreInfoPtr);
  DataStore->LatestValidStore = NewLvs;

  return Status;
}

uint32_t
DataMgrClose (DataMgrHandle     Handle)
{
  uint32_t Status = DATA_STORE_SUCCESS;
  if (Handle == NULL)
    return DATA_STORE_INVALID_PARAMETER;

  DataStoreCtx *DataStore = HANDLE_TO_CONTEXT(Handle);

  //free allocations
  qwes_free(DataStore->StoreInfoPtr);
  qwes_free(DataStore->CurrentHeader);
  qwes_free(DataStore->TempHeader);
  
  Status = DataStore->store.close((void*)DataStore->mClientHandle);
  if (Status != QWES_STORE_SUCCESS)
  {
    Status = DATA_STORE_DEVICE_ERROR;
  }

  //Zero out context
  DeallocateCtx(&DataStore);

  return Status;
}

static
void *CopyMem(void *DestinationBuffer,
              uint32_t Dest_Length,
              const void *SourceBuffer,
              uint32_t Src_Length)
{
    uint32_t Copy_Length = (Dest_Length <= Src_Length) ? Dest_Length : Src_Length;
    if (Copy_Length && DestinationBuffer && SourceBuffer) {
        return (void*)memmove(DestinationBuffer, SourceBuffer, Copy_Length);
    }

    return NULL;
}

/** Opens DataMgr instance **/
uint32_t
DataMgrOpen (DataMgrHandle*  HandlePtr,
             TableInfoType*  TableInfoPtr,
             QWESStore       store)
{
  uint32_t Status = DATA_STORE_SUCCESS;
  DataStoreInfo *InfoHdr, *InfoFtr;
  uint32_t Age0=0, Age1=0;
  uint32_t Hdr0Valid = 0, Hdr1Valid = 0;
  uint32_t Hdr0NotInit = 0, Hdr1NotInit = 0;
  int InfoBlkValid = 0xF;
  uint8_t part_name[] = "0:LICENSE";

  DataStoreCtx *DataStore;

  if ((TableInfoPtr == NULL) || (HandlePtr == NULL))
    return DATA_STORE_INVALID_PARAMETER;

  DataStore = HANDLE_TO_CONTEXT(*HandlePtr);

  Status = AllocateCtx(&DataStore);
  if (Status != DATA_STORE_SUCCESS)
    return Status;

  DataStore->TableInfo.DeviceId = TableInfoPtr->DeviceId;
  CopyMem(&DataStore->TableInfo.PartitionGuid,
          sizeof(DataStore->TableInfo.PartitionGuid),
          &TableInfoPtr->PartitionGuid,
          sizeof(TableInfoPtr->PartitionGuid));
  DataStore->TableInfo.PartitionType = TableInfoPtr->PartitionType;

  DataStore->store = store;
#if 0
  /* Initialize the device and try to open the partition */
  Status = DataStore->store.device_init((qwes_store_device_id_type) DataStore->TableInfo.DeviceId,
                                 DataStore->TableInfo.PartitionGuid,
                                 &DataStore->mDeviceHandle);
  if ((Status != QWES_STORE_SUCCESS) || (DataStore->mDeviceHandle == 0))
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto CleanupContext;
  }
#endif
  Status = DataStore->store.open_partition(part_name,
                                    (void**)&DataStore->mClientHandle);
  if ((Status != QWES_STORE_SUCCESS) || (DataStore->mClientHandle == 0))
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto CleanupContext;
  }

  Status = DataStore->store.client_get_info((void*)DataStore->mClientHandle, &DataStore->mClientInfo);
  if (Status != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto CleanupContext;
  }
  /* Round off to even count */
  DataStore->ContextSignature = CONTEXT_SIGNATURE;
  DataStore->PartitionSlotCount = DataStore->mClientInfo.partition_size_in_slots & (~0x1);

  /*Vaidate PartitionSlotCount size*/
  if(DataStore->PartitionSlotCount < MIN_STORAGE_TABLE_BLOCKS)
  {
    Status = DATA_STORE_BUFFER_TOO_SMALL;
    goto CleanupContext;
  }

  DataStore->MaxDataSectors = (DataStore->PartitionSlotCount / 2) - 2;
  DataStore->DataSectorSize = DataStore->mClientInfo.slot_size;
  DataStore->Headers[0] = 0;
  DataStore->Footers[0] = DataStore->MaxDataSectors + 1;
  DataStore->DataLoc[0] = DataStore->Headers[0] + 1;
  DataStore->Headers[1] = DataStore->Footers[0] + 1;
  DataStore->Footers[1] = DataStore->Headers[1] + DataStore->MaxDataSectors + 1;
  DataStore->DataLoc[1] = DataStore->Headers[1] + 1;


  /* Allocate full slot worth of data for reading */
  DataStore->StoreInfoPtr = qwes_malloc(DataStore->mClientInfo.slot_size);
  DataStore->CurrentHeader = qwes_malloc(DataStore->mClientInfo.slot_size);
  DataStore->TempHeader = qwes_malloc(DataStore->mClientInfo.slot_size);
  if ((DataStore->StoreInfoPtr == NULL) || (DataStore->CurrentHeader == NULL) || (DataStore->TempHeader == NULL))
  {
    Status = DATA_STORE_OUT_OF_RESOURCES;
    goto CleanupContext;
  }

  InfoHdr = DataStore->StoreInfoPtr;
  InfoFtr = DataStore->TempHeader;
  /* Read the Header and footer for first copy */
  if (DataStore->store.read((void*)DataStore->mClientHandle,
      DataStore->Headers[0], 1, (uint8_t*) InfoHdr) != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto Cleanup;
  }

  InfoBlkValid = ValidateInfoBlk (InfoHdr);
  if (InfoBlkValid == DATA_STR_NOT_INITIALIZED)
  {
    if (DataStore->store.read((void*)DataStore->mClientHandle,
        DataStore->Footers[0], 1, (uint8_t*) InfoFtr) != QWES_STORE_SUCCESS)
    {
      gVarDataStorageError = Status;
      Status = DATA_STORE_DEVICE_ERROR;
      goto Cleanup;
    }
    if (memcmp(InfoHdr, InfoFtr, sizeof(DataStoreInfo)) == CMP_SUCCESS)
      Hdr0NotInit = 1;
  }
  else if (InfoBlkValid == INFO_BLK_VALID_SUCCESS)
  {
    if (DataStore->store.read((void*)DataStore->mClientHandle,
        DataStore->Footers[0], 1, (uint8_t*) InfoFtr) != QWES_STORE_SUCCESS)
    {
      gVarDataStorageError = Status;
      Status = DATA_STORE_DEVICE_ERROR;
      goto Cleanup;
    }
    if (memcmp(InfoHdr, InfoFtr, sizeof(DataStoreInfo)) == CMP_SUCCESS)
    {
      Hdr0Valid = 1;
      Age0 = InfoHdr->Age;
      InfoHdr = DataStore->CurrentHeader;
    }
  }

  /* Read the Header and footer for second copy */
  if (DataStore->store.read((void*)DataStore->mClientHandle,
      DataStore->Headers[1], 1, (uint8_t*) InfoHdr) != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto Cleanup;
  }

  InfoBlkValid = ValidateInfoBlk (InfoHdr);
  if (InfoBlkValid == DATA_STR_NOT_INITIALIZED)
  {
    if (DataStore->store.read((void*)DataStore->mClientHandle,
        DataStore->Footers[1], 1, (uint8_t*) InfoFtr) != QWES_STORE_SUCCESS)
    {
      gVarDataStorageError = Status;
      Status = DATA_STORE_DEVICE_ERROR;
      goto Cleanup;
    }
    if (memcmp(InfoHdr, InfoFtr, sizeof(DataStoreInfo)) == CMP_SUCCESS)
      Hdr1NotInit = 1;
  }
  else if (InfoBlkValid == INFO_BLK_VALID_SUCCESS)
  {
    if (DataStore->store.read((void*)DataStore->mClientHandle,
        DataStore->Footers[1], 1, (uint8_t*) InfoFtr) != QWES_STORE_SUCCESS)
    {
      gVarDataStorageError = Status;
      Status = DATA_STORE_DEVICE_ERROR;
      goto Cleanup;
    }
    if (memcmp(InfoHdr, InfoFtr, sizeof(DataStoreInfo)) == CMP_SUCCESS)
    {
      Hdr1Valid = 1;
      Age1 = InfoHdr->Age;
    }
  }

  /* Decide based on Header/Footer parsing */
  if (Hdr0NotInit && Hdr1NotInit)
  {
    gVarDataStorageError = DATA_STR_NOT_INITIALIZED;
    Status = DATA_STORE_VOLUME_CORRUPTED;
    goto Cleanup;
  }

  if (!Hdr0Valid && !Hdr1Valid)
  {
    gVarDataStorageError = DATA_STR_INVALID_HEADERS;
    Status = DATA_STORE_VOLUME_CORRUPTED;
    goto Cleanup;
  }

  /* Select a copy based on how the validation go, if both are valid
   * we have to do age based selection */
  if (Hdr0Valid && !Hdr1Valid)
  {
    /* NOTE: DataStore->StoreInfo already had Header for first copy */
  }
  else if (Hdr1Valid && !Hdr0Valid)
  {
    DataStore->LatestValidStore = 1;
    CopyMem (DataStore->StoreInfoPtr, sizeof(*(DataStore->StoreInfoPtr)), InfoHdr,
             sizeof(*InfoHdr));
  }
  else
  {
    /* Since this switch statement is hardcoded to the max age, generate
     * build error if the value changes without the switch statement */
#if MAX_AGE_VALUE > 4
  #error Incompatible Max Age value for Switch statement
#endif
    /* Both are valid, need to determine which is the latest ones */
    switch (PACK_AGES(Age0, Age1))
    {
        /* Age0 is latest */
      case PACK_AGES(1, 0):
      case PACK_AGES(2, 1):
      case PACK_AGES(3, 2):
      case PACK_AGES(0, 3):
        DataStore->LatestValidStore = 0;
        /* NOTE: DataStore->StoreInfo already had Header for first copy */
        break;

        /* Age1 is latest */
      case PACK_AGES(0, 1):
      case PACK_AGES(1, 2):
      case PACK_AGES(2, 3):
      case PACK_AGES(3, 0):
        DataStore->LatestValidStore = 1;
        /* Last read into InfoHdr was for the second copy */
        CopyMem (DataStore->StoreInfoPtr,
                 sizeof(*(DataStore->StoreInfoPtr)),
                 InfoHdr,
                 sizeof(*InfoHdr));
        break;

        /* All the following cases are ERROR cases */
      case PACK_AGES(0, 2):
      case PACK_AGES(1, 3):
      case PACK_AGES(2, 0):
      case PACK_AGES(3, 1):
        /* Ages are wrong distant apart */

      case PACK_AGES(0, 0):
      case PACK_AGES(1, 1):
      case PACK_AGES(2, 2):
      case PACK_AGES(3, 3):
      default:
        /* Security Violation, tampering detected, Ages cannot be same */
        gVarDataStorageError = DATA_STR_INAPPROPRIATE_AGES;
        Status = DATA_STORE_VOLUME_CORRUPTED;
        goto Cleanup;
    }
  }

  /* Make sure the version hasn't moved up in storage or sw module didn't
   * got downgraded to incompatible version */
  if ((DataStore->StoreInfoPtr->Version > DATA_STORE_LATEST_VERSION) ||
      (DataStore->StoreInfoPtr->WriteMethod != WRITE_METHOD_SIMPLE_PING_PONG_TYPE))
  {
    gVarDataStorageError = DATA_STR_VERSION_MISMATCH;
    Status = DATA_STORE_VOLUME_CORRUPTED;
    goto Cleanup;
  }

  UpdateHdrAgeAndCRC(DataStore->StoreInfoPtr);
  *HandlePtr = (DataMgrHandle) DataStore;
  return DATA_STORE_SUCCESS;

Cleanup:
  if (DataStore->StoreInfoPtr)
    qwes_free(DataStore->StoreInfoPtr);
  if (DataStore->CurrentHeader)
    qwes_free(DataStore->CurrentHeader);
  if (DataStore->TempHeader)
    qwes_free(DataStore->TempHeader);
CleanupContext:
  if(DataStore->mClientHandle)
    DataStore->store.close((void*)DataStore->mClientHandle);
  DeallocateCtx(&DataStore);
  return Status;
}



/* Initialize context, detect/validate the copies of the data stores and
 * prepare to use the latest ones */
uint32_t
DataMgrInit (DataMgrHandle* HandlePtr,
             TableInfoType*  TableInfoPtr,
	     QWESStore       store)
{
  uint32_t Status = DATA_STORE_SUCCESS;
  DataStoreCtx *DataStore;
  uint8_t part_name[] = "0:LICENSE";

  if ((TableInfoPtr == NULL) || (HandlePtr == NULL))
    return DATA_STORE_INVALID_PARAMETER;

  DataStore = HANDLE_TO_CONTEXT(*HandlePtr);

  Status = AllocateCtx(&DataStore);
  if (Status != DATA_STORE_SUCCESS)
    return Status;

  DataStore->TableInfo.DeviceId = TableInfoPtr->DeviceId;
  CopyMem(&DataStore->TableInfo.PartitionGuid,
          sizeof(DataStore->TableInfo.PartitionGuid),
          &TableInfoPtr->PartitionGuid,
          sizeof(TableInfoPtr->PartitionGuid));
  DataStore->TableInfo.PartitionType = TableInfoPtr->PartitionType;
  DataStore->store = store;
#if 0
  Status = DataStore->store.device_init((qwes_store_device_id_type)DataStore->TableInfo.DeviceId,
                                 DataStore->TableInfo.PartitionGuid,
                                 &DataStore->mDeviceHandle);
  if ((Status != QWES_STORE_SUCCESS) || (DataStore->mDeviceHandle == 0))
    goto Cleanup;
#endif
#if 0
  Status = DataStore->store.open_partition(&DataStore->mDeviceHandle,
                                    DataStore->TableInfo.PartitionType,
                                    &DataStore->mClientHandle);
#endif
  Status = DataStore->store.open_partition(part_name,
                                    (void**)&DataStore->mClientHandle);
  if ((Status != QWES_STORE_SUCCESS) || (DataStore->mClientHandle == 0))
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto Cleanup;
  }

  Status = DataStore->store.client_get_info((void*)DataStore->mClientHandle, &DataStore->mClientInfo);
  if (Status != QWES_STORE_SUCCESS)
  {
    gVarDataStorageError = Status;
    Status = DATA_STORE_DEVICE_ERROR;
    goto Cleanup;
  }

  /* Will help identify if we got a valid image or need to initialize first */
  DataStore->LatestValidStore = INVALID_LATEST_STORE_VALUE;

  DataStore->ContextSignature = CONTEXT_SIGNATURE;
  DataStore->PartitionSlotCount = DataStore->mClientInfo.partition_size_in_slots & (~0x1);
  DataStore->MaxDataSectors = (DataStore->PartitionSlotCount / 2) - 2;
  DataStore->DataSectorSize = DataStore->mClientInfo.slot_size;
  DataStore->Headers[0] = 0;
  DataStore->Footers[0] = DataStore->MaxDataSectors + 1;
  DataStore->DataLoc[0] = DataStore->Headers[0] + 1;
  DataStore->Headers[1] = DataStore->Footers[0] + 1;
  DataStore->Footers[1] = DataStore->Headers[1] + DataStore->MaxDataSectors + 1;
  DataStore->DataLoc[1] = DataStore->Headers[1] + 1;

  DataStore->StoreInfoPtr = qwes_malloc(DataStore->mClientInfo.slot_size);
  if (DataStore->StoreInfoPtr == NULL)
  {
    Status = DATA_STORE_OUT_OF_RESOURCES;
    goto Cleanup;
  }

  DataStore->StoreInfoPtr->Signature = DATA_STORE_MGR_SIGNATURE;
  DataStore->StoreInfoPtr->Version = DATA_STORE_LATEST_VERSION;
  DataStore->StoreInfoPtr->Age = 0;
  DataStore->StoreInfoPtr->WriteMethod = WRITE_METHOD_SIMPLE_PING_PONG_TYPE;
  DataStore->StoreInfoPtr->MaxPayloadSectors = DataStore->MaxDataSectors;
  DataStore->StoreInfoPtr->ActivePayloadSectors = 0;
  DataStore->StoreInfoPtr->CfgDataSize = 0;
  DataStore->StoreInfoPtr->SyncCount = 0;

  DataStore->LatestValidStore = 0;
  UpdateHdrAgeAndCRC(DataStore->StoreInfoPtr);
  *HandlePtr = (DataMgrHandle) DataStore;
  return DATA_STORE_SUCCESS;

Cleanup:
  DeallocateCtx(&DataStore);
  return Status;
}

uint32_t
DataMgrGetInfo (DataMgrHandle   Handle,
                uint32_t         *SlotCount,
                uint32_t         *SlotSize)
{
  if (Handle == NULL)
    return DATA_STORE_INVALID_PARAMETER;

  DataStoreCtx *DataStore = HANDLE_TO_CONTEXT(Handle);

  if (!IS_CONTEXT_VALID(DataStore))
    return DATA_STORE_INVALID_PARAMETER;

  *SlotCount = DataStore->MaxDataSectors;
  *SlotSize = DataStore->DataSectorSize;

  return DATA_STORE_SUCCESS;
}
