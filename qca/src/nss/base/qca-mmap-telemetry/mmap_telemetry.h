/*
**************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
**************************************************************************
*/

#ifndef __MMAP_TELEMETRY_H_
#define __MMAP_TELEMETRY_H_

#include <linux/uio_driver.h>
#include "mmap_telemetry_public.h"
#include <linux/types.h>
#include <linux/atomic.h>

#define RING_SIZE 64
#define INTERRUPT_MAP_SIZE 32768

#define WLFW_MAP_SIZE ((128 * 1024)*2)  //for 2 PDEV each 128K

#define DEVICE_NAME "mmap_telemetry_char_dev"
#define CLASS_NAME  "mmap_telemetry_class"
#define IOCTL_SEND_INTERRUPT _IOW('u', 1, struct mmap_telemetry_intr_data)

/*
 * The following are debug macros used throughout the MMAP telemetry.
 *
 * The DEBUG_LEVEL enables the followings based on its value,
 * when dynamic debug option is disabled.
 *
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */

#if (DEBUG_LEVEL < 1)
#define DEBUG_ASSERT(s, ...)
#define DEBUG_ERROR(s, ...)
#else
#define DEBUG_ASSERT(c, s, ...) if (!(c)) { pr_emerg("ASSERT: %s:%d:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__); BUG(); }
#define DEBUG_ERROR(s, ...) pr_err("%s:%d:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * Compile messages for dynamic enable/disable
 */
#define DEBUG_WARN(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DEBUG_INFO(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define DEBUG_TRACE(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels
 */
#if (DEBUG_LEVEL < 2)
#define DEBUG_WARN(s, ...)
#else
#define DEBUG_WARN(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (DEBUG_LEVEL < 3)
#define DEBUG_INFO(s, ...)
#else
#define DEBUG_INFO(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (DEBUG_LEVEL < 4)
#define DEBUG_TRACE(s, ...)
#else
#define DEBUG_TRACE(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * mmap_telemetry_intr_data
 *	Struct to define the UIO interrupt data
 */
struct mmap_telemetry_intr_data {
        int uioId;			/* UIO Id */
        int mapId;			/* Map Id */
        uint32_t offset_from_base;	/* offset from base address */
        uint32_t size_in_bytes_to_read;	/* Size in bytes to read */
        union {
                uint8_t raw[256];	/* Raw data */
        } payload;			/* Payload for interrupt data */

};

/*
 * mmap_telemetry_ring_buffer
 *	Ring buffer for interrupt handling
 */
struct mmap_telemetry_ring_buffer {
        atomic_t front;			/* Front pointer */
        atomic_t rear;			/* Rear pointer */
        struct mmap_telemetry_intr_data buffer[RING_SIZE];	/* Interrupt data */
};

int mmap_telemetry_num_maps_per_category[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX] = {MMAP_TELEMETRY_MEM_TYPE_SXC_MAX,
                                                                                 MMAP_TELEMETRY_MEM_TYPE_WLFW_MAX};	/* To store number of maps per UIO device */

/*
 * mmap_telemetry_uio_dev_category_str
 *	mmap telemetry uio device category strings
 */
const char* mmap_telemetry_uio_dev_category_str[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX] = {
	"sxc",
	"wlfw"
};

/*
 * mmap_telemetry_mem_type_str
 *	mmap telemetry memory type strings
 */
const char* mmap_telemetry_mem_type_str[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX][MAX_UIO_MAPS] = {
	{"interrupt", "sac", "sdc"},
	{"wlfw0", "wlfw1", "wlfw2", "wlfw3"},
};

/*
 * mmap_telemetry_uio_info
 *	mmap telemetry uio info structure
 */
struct mmap_telemetry_uio_info {
	struct uio_info *info;	/* UIO device information */
	spinlock_t uio_lock;	/* Lock for protecting UIO device */
};

#endif // __MMAP_TELEMETRY_H_
