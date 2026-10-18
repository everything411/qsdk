// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#pragma once

#include <linux/types.h>
#include <stddef.h>

/* U-Boot compatible type definitions */
#ifndef uint8_t
#define uint8_t u8
#endif
#ifndef uint16_t
#define uint16_t u16
#endif
#ifndef uint32_t
#define uint32_t u32
#endif
#ifndef uint64_t
#define uint64_t u64
#endif

typedef enum {
    QWES_STORE_EMMC_USER = 0,              /* User Partition in eMMC */
    QWES_STORE_EMMC_BOOT0,                 /* Boot0 Partition in eMMC */
    QWES_STORE_EMMC_BOOT1,                 /* Boot1 Partition in eMMC */
    QWES_STORE_EMMC_RPMB,                  /* RPMB Partition in eMMC */
    QWES_STORE_EMMC_GPP1,                  /* GPP1 Partition in eMMC */
    QWES_STORE_EMMC_GPP2,                  /* GPP2 Partition in eMMC */
    QWES_STORE_EMMC_GPP3,                  /* GPP3 Partition in eMMC */
    QWES_STORE_EMMC_GPP4,                  /* GPP4 Partition in eMMC */
    QWES_STORE_EMMC_ALL,                   /* Entire eMMC device */
    QWES_STORE_SPINOR_ALL = 0xb,           /* Entire SPINOR device */
    QWES_STORE_ID_RESERVED = 0x7FFFFFFF    /* Reserved: Device ID Max */
} qwes_store_device_id_type;


/* Device Information Structure */
typedef struct {
    qwes_store_device_id_type dev_id;    /* Device ID (Physical partition number) */
    uint8_t partition_guid[16];          /* GUID for the partition in the device (GPT only) */
    uint32_t bytes_per_sector;           /* Bytes per Sector */
    uint32_t total_sectors;              /* Total size in sectors */
    uint32_t available_sectors;          /* Total available sectors for new partitions */
} __attribute__ ((packed)) qwes_store_device_info_t;

/* Client Information structure */
typedef struct {
    //qwes_store_device_id_type dev_id;    /* Device ID */
    //uint8_t partition_guid[16];          /* GUID for the partition in the device (GPT only) */
    uint32_t slot_size;                  /* Bytes per Slot (fixed = SLOT_SIZE) */
    uint32_t partition_size_in_slots;    /* Total size available in slots (partition_size / slot_size) */
} __attribute__ ((packed)) qwes_store_client_info_t;

typedef uint64_t qwes_store_device_handle_t;
typedef uint64_t qwes_store_client_handle_t;

#define QWES_STORE_SUCCESS             0 /* Success */
#define QWES_STORE_ERROR               1 /* Generic failure */
#define QWES_STORE_INVALID_PARAM       2 /* Invalid arguments passed to the API */
#define QWES_STORE_NOT_FOUND           3 /* Device not found */
#define QWES_STORE_PARTI_NOT_FOUND     4 /* Partition not found */
#define QWES_STORE_OUT_OF_RESOURCES    5 /* Out of memory/other resources */
#define QWES_STORE_ACCESS_VIOLATION    6 /* Access violation */

/*----------------------------------------------------------------------------
 * Function Pointer Declarations and Documentation
 * -------------------------------------------------------------------------*/
typedef struct QWESStore {
#if 0
    int (*storage_init)(size_t partition_size,
                        size_t partition_count);

    /**
      Initialize device indicated by device_id and partition_guid.

      @param[in]  device_id         Device partition number (qwes_store_device_id_type or qsee_stor_device_id_type).
      @param[in]  partition_guid    Partition GUID (applies only for GPT partitions).
      @param[out] device_handle     Pointer to a device handle (qwes_store_device_handle_t* or qsee_stor_device_handle_t*).

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*device_init)(uint32_t device_id,
                       uint8_t* partition_guid,
                       void* device_handle);
#endif
    /**
      Open a logical partition.

      @param[in]  device_handle    Pointer to a device handle (qwes_store_device_handle_t* or qsee_stor_device_handle_t*).
      @param[in]  partition_id     Logical partition ID.
      @param[out] client_handle    Pointer to a client handle (qwes_store_client_handle_t* or qsee_stor_client_handle_t*).

      @return QWES_STORE_SUCCESS if no errors; otherwise error code.
    */
#if 0
    int (*open_partition)(void* device_handle,
                          uint32_t partition_id,
                          void* client_handle);
#endif
       int (*open_partition)(uint8_t* partition_name,
                          void** client_handle);
 
#if 0
    /**
      Returns device info.

      @param[in]  device_handle    Pointer to a device handle (qwes_store_device_handle_t* or qsee_stor_device_handle_t*).
      @param[out] device_info      Pointer to a device info structure (qwes_store_device_info_t* or qsee_stor_device_info_t*).

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*device_get_info)(void* device_handle,
                           void* device_info);
#endif
    /**
      Returns client info.

      @param[in] client_handle    Pointer to a client handle (qwes_store_client_handle_t* or qsee_stor_client_handle_t*).
      @param[in] client_info      Pointer to a client info structure (qwes_store_client_info_t* or qsee_stor_client_info_t*).

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*client_get_info)(void* client_handle,
                           void* client_info);

#if 0
    /**
      Adds a new logical partition.

      @param[in] device_handle    Pointer to a device handle (qwes_store_device_handle_t* or qsee_stor_device_handle_t*).
      @param[in] partition_id     Logical partition ID.
      @param[in] num_sectors      Number of sectors of the new logical partition.

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*add_partition)(void* device_handle,
                         uint32_t partition_id,
                         uint16_t num_sectors);
#endif
    /**
      Read num_sectors of data from start_sector to data_buffer.

      @param[in]  client_handle    Pointer to a client handle (qwes_store_client_handle_t* or qsee_stor_client_handle_t*).
      @param[in]  start_sector     Starting sector to read from.
      @param[in]  num_sectors      Number of sectors to read.
      @param[out] data_buffer      Pointer to buffer containing read data.

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*read)(void* client_handle,
                uint32_t start_slot,
                uint32_t num_slots,
                uint8_t* data_buffer);

    /**
      Write num_slots of data from data_buffer to start_slot.

      @param[in] client_handle    Pointer to a client handle (qwes_store_client_handle_t* or qsee_stor_client_handle_t*).
      @param[in] start_slot       Starting slot to write to.
      @param[in] num_slots        Number of slots to write.
      @param[in] data_buffer      Pointer to buffer containing data to be written.

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*write)(void* client_handle,
                 uint32_t start_slot,
                 uint32_t num_slots,
                 uint8_t* data_buffer);
    /**
      Closes license store handle.

      @param[in] client_handle    Pointer to a client handle (qwes_store_client_handle_t* or qsee_stor_client_handle_t*).

      @return QWES_STORE_SUCCESS if no errors; otherwise, error code.
    */
    int (*close) (void* client_handle);
} QWESStore;
