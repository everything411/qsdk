/*
**************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
**************************************************************************
*/

#ifndef __MMAP_TELEMETRY_PUBLIC_H_
#define __MMAP_TELEMETRY_PUBLIC_H_

/*
 * mmap_telemetry_public.h
 *      MMAP telemetry exported function headers for the telemetry framework.
 */

/*
 * Mmap Telemery uio device category enum.
 */
typedef enum mmap_telemetry_uio_dev_category {
	MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC,		/* UIO device for SXC statistics. */
	MMAP_TELEMETRY_UIO_DEV_CATEGORY_WLFW,	        /* UIO device for WLAN FW statistics. */
	/*
	 * Please add the new devices here.
	 */
	MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX		/* Indicates the last item. */
} mmap_telemetry_uio_dev_category;

/*
 * MMAP Telemetry memory map type enum.
 */
typedef enum  mmap_telemetry_mem_type_sxc {
	MMAP_TELEMETRY_MEM_TYPE_INTERRUPT,	/* Memory map for interrupt handling. */
	MMAP_TELEMETRY_MEM_TYPE_SAC,		/* Memory map for sac statistics.*/
	MMAP_TELEMETRY_MEM_TYPE_SDC,		/* Memory map for sdc statistics.*/
	MMAP_TELEMETRY_MEM_TYPE_SXC_MAX		/* Indicates the last item. */
} mmap_telemetry_mem_type_sxc;

typedef enum  mmap_telemetry_mem_type_wlfw {
	MMAP_TELEMETRY_MEM_TYPE_WLFW0,		/* Memory map for wlan firmware statistics.*/
	MMAP_TELEMETRY_MEM_TYPE_WLFW1,		/* Memory map for wlan firmware statistics.*/
	MMAP_TELEMETRY_MEM_TYPE_WLFW2,		/* Memory map for wlan firmware statistics.*/
	MMAP_TELEMETRY_MEM_TYPE_WLFW3,		/* Memory map for wlan firmware statistics.*/
	MMAP_TELEMETRY_MEM_TYPE_WLFW_MAX	/* Indicates the last item. */
} mmap_telemetry_mem_type_wlfw;
/**
 * Allocate mmap telemetry memory map.
 *
 * @param	dev_type	Indicates UIO device type.
 * @param	map_type	Indicates memory map type.
 * @param	map_size	Size of the memory map.
 * @param	is_coherent	Indicate if memory is coherent.
 *
 * @return
 * Return address of the allocated memory.
 */
void *mmap_telemetry_alloc_mem(enum mmap_telemetry_uio_dev_category dev_type, int map_type, unsigned long map_size, bool is_coherent);

/**
 * Get mmap telemetry memory map address.
 *
 * @param	dev_type	Indicates UIO device type.
 * @param	map_type	Indicates memory map type.
 *
 * @return
 * Return address of the allocated memory.
 */
void *mmap_telemetry_get_mem(enum mmap_telemetry_uio_dev_category dev_type, int map_type);

/**
 * Free mmap telemetry memory map.
 *
 * @param	dev_type	Indicates UIO device type.
 * @param	map_type	Indicates memory map type.
 *
 * @return
 * None.
 */
void mmap_telemetry_free_mem(enum mmap_telemetry_uio_dev_category dev_type, int map_type);

/**
 * Notify when data is written to memory map.
 *
 * @param	dev_type	Indicates UIO device type.
 * @param	map_type	Indicates memory map type.
 * @param	offset		Offset from the base address.
 * @param	size		Size in bytes to read.
 *
 * @return
 * Return success or failure.
 */
int mmap_telemetry_notify(enum mmap_telemetry_uio_dev_category dev_type, int map_type, uint32_t offset, uint32_t size);

/**
 * Register to received mmap telemetry events
 *
 * @param	nb		Notifier block callback.
 * @param	dev_type	Indicates UIO device type.
 * @param	map_type	Indicates memory map type.
 *
 * @return
 * Return success or failure.
 */
int mmap_telemetry_register_notifier(struct notifier_block *nb, enum mmap_telemetry_uio_dev_category dev_type, int map_type);

/**
 * Unregister to received mmap telemetry events
 *
 * @param	nb		Notifier block callback.
 * @param	dev_type	Indicates UIO device type.
 * @param	map_type	Indicates memory map type.
 *
 * @return
 * Return success or failure.
 */
int mmap_telemetry_unregister_notifier(struct notifier_block *nb, enum mmap_telemetry_uio_dev_category dev_type, int map_type);

#endif //__MMAP_TELEMETRY_PUBLIC_H_
