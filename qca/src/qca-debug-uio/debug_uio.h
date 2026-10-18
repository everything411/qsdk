/*
**************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
**************************************************************************
*/

#ifndef __DEBUG_UIO_H_
#define __DEBUG_UIO_H_

/*
 * BUG FIX: Removed "#include "debug_uio.h"" — a header must NEVER include
 *          itself. This caused a circular include that would prevent
 *          compilation entirely.
 *
 * Include the public API header instead so that the enum types
 * (debug_uio_dev, debug_uio_map_type) are visible to this file.
 */
#include <linux/uio_driver.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include "exports/debug_uio_public.h"

/* -----------------------------------------------------------------------
 * Module-wide constants
 * ----------------------------------------------------------------------- */

#define DEBUG_UIO_DEV_MAX		3
#define DEBUG_UIO_MAPS_PER_DEV_MAX	2

/*
 * BUG FIX: RING_SIZE was used in struct debug_uio_interrupt_ring_buffer
 *          but was never defined anywhere, causing a compile error.
 *          16 is chosen as a power-of-two so the modulo in the enqueue
 *          path can be replaced with a bitmask if needed in the future.
 */
#define RING_SIZE			16

#define MAX_NUM_DATA_BUFFERS		4

/*
 * MAX_BUFFER_SIZE is the usable payload bytes per data-ring entry.
 * Chosen so that struct debug_uio_data fits comfortably inside one
 * cache line cluster and the full data ring fits within one PAGE_SIZE.
 */
#define MAX_BUFFER_SIZE			1020

#define DEVICE_NAME	"debug_uio_char_dev"
#define CLASS_NAME	"debug_uio_class"

#define IOCTL_SEND_INTERRUPT	_IOW('u', 1, struct debug_uio_intr_data)

/* -----------------------------------------------------------------------
 * Debug level
 *
 * BUG FIX: DEBUG_LEVEL was referenced by the debug macros below but was
 *          never defined, causing compile errors when dynamic debug is
 *          disabled.  Default to INFO (3) so warnings and info messages
 *          are visible during development.  Override on the compiler
 *          command line with -DDEBUG_LEVEL=<n> if needed.
 * ----------------------------------------------------------------------- */
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 3
#endif

/* -----------------------------------------------------------------------
 * Debug macros
 *
 * DEBUG_LEVEL controls which messages are compiled in when
 * CONFIG_DYNAMIC_DEBUG is not set:
 *   0 = OFF
 *   1 = ASSERT / ERROR
 *   2 = 1 + WARN
 *   3 = 2 + INFO
 *   4 = 3 + TRACE
 * ----------------------------------------------------------------------- */

#if (DEBUG_LEVEL < 1)
#define DEBUG_ASSERT(s, ...)
#define DEBUG_ERROR(s, ...)
#else
#define DEBUG_ASSERT(c, s, ...) \
	do { if (!(c)) { pr_emerg("ASSERT: %s:%d:" s, __func__, __LINE__, ##__VA_ARGS__); BUG(); } } while (0)
#define DEBUG_ERROR(s, ...) \
	pr_err("%s:%d:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if defined(CONFIG_DYNAMIC_DEBUG)
#define DEBUG_WARN(s, ...)  pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define DEBUG_INFO(s, ...)  pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define DEBUG_TRACE(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else

#if (DEBUG_LEVEL < 2)
#define DEBUG_WARN(s, ...)
#else
#define DEBUG_WARN(s, ...)  pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (DEBUG_LEVEL < 3)
#define DEBUG_INFO(s, ...)
#else
#define DEBUG_INFO(s, ...)  pr_notice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (DEBUG_LEVEL < 4)
#define DEBUG_TRACE(s, ...)
#else
#define DEBUG_TRACE(s, ...) pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#endif /* CONFIG_DYNAMIC_DEBUG */

/* -----------------------------------------------------------------------
 * Data structures shared between debug_uio.c and the ring-buffer helpers
 * ----------------------------------------------------------------------- */

/*
 * debug_uio_intr_data
 *	Metadata written into the interrupt ring buffer (map 0) each time a
 *	kernel driver calls debug_uio_notify().  Userspace reads this ring to
 *	learn which device/map raised an event and where the associated data
 *	lives in the mmap'd window.
 */
struct debug_uio_intr_data {
	uint8_t  uioId;			/* UIO device id (debug_uio_dev) */
	uint8_t  mapId;			/* Map id (debug_uio_map_type) */
	uint32_t offset_from_base;	/* Byte offset from map base address */
	uint32_t size_in_bytes_to_read;	/* Number of bytes to read */
	union {
		uint8_t raw[4];		/* Raw interrupt payload */
	} payload;
};

/*
 * debug_uio_data
 *	One entry in the data ring buffer (map 1).  The payload field holds
 *	up to MAX_BUFFER_SIZE bytes of opaque data written by the kernel
 *	driver via debug_uio_write_data().
 */
struct debug_uio_data {
	union {
		char data[MAX_BUFFER_SIZE];	/* Opaque data payload */
	} payload;
};

#define DEBUG_UIO_APP_NAME_MAX 32

/*
 * debug_uio_app_status_data
 *	Payload for IOCTL_APP_STATUS.  Sent by a userspace app to announce
 *	that it is up or down.  Unlike debug_uio_intr_data, this is not tied
 *	to any UIO device/map — app_name lets subsystem drivers filter for
 *	the specific app(s) they depend on.
 */
struct debug_uio_app_status_data {
	char    app_name[DEBUG_UIO_APP_NAME_MAX];
	uint8_t is_up;			/* 1 = up, 0 = down */
};

#define IOCTL_APP_STATUS	_IOW('u', 2, struct debug_uio_app_status_data)

/*
 * debug_uio_interrupt_ring_buffer
 *	Lock-free single-producer / single-consumer ring buffer stored at
 *	the beginning of map 0.  front and rear are atomic so that the
 *	kernel writer and the userspace reader do not need a mutex.
 *
 *	Layout in shared memory (map 0):
 *	  [atomic_t front][atomic_t rear][intr_data[0] ... intr_data[RING_SIZE-1]]
 */
struct debug_uio_interrupt_ring_buffer {
	atomic_t front;					/* Consumer (userspace) read index */
	atomic_t rear;					/* Producer (kernel) write index  */
	struct debug_uio_intr_data buffer[RING_SIZE];	/* Circular array of entries */
};

/*
 * debug_uio_data_ring_buffer
 *	Lock-free ring buffer stored at the beginning of map 1.
 *
 *	Layout in shared memory (map 1):
 *	  [atomic_t front][atomic_t rear][data[0] ... data[MAX_NUM_DATA_BUFFERS-1]]
 */
struct debug_uio_data_ring_buffer {
	atomic_t front;					/* Consumer (userspace) read index */
	atomic_t rear;					/* Producer (kernel) write index  */
	struct debug_uio_data buffer[MAX_NUM_DATA_BUFFERS];	/* Circular array */
};

/* -----------------------------------------------------------------------
 * Per-device metadata kept in kernel space only (not shared with userspace)
 * ----------------------------------------------------------------------- */

/*
 * debug_uio_info
 *	Wraps the UIO framework's uio_info together with a spinlock that
 *	protects the mem[] array entries from concurrent alloc/free.
 *
 *	overwrite_on_full controls the data ring full policy:
 *	  false (default) – drop the new entry and return -ENOSPC.
 *	                    This is the original behaviour; all existing
 *	                    callers that never call debug_uio_set_overwrite_mode()
 *	                    continue to work without any change.
 *	  true            – silently discard the oldest entry and write the
 *	                    new one, so the ring always holds the last
 *	                    MAX_NUM_DATA_BUFFERS events.
 *	                    Must be explicitly enabled per-device via
 *	                    debug_uio_set_overwrite_mode().
 *
 *	Because info_global[] is allocated with kzalloc(), overwrite_on_full
 *	is zero-initialised (false) at module load time, guaranteeing
 *	backward-compatible drop behaviour unless the caller opts in.
 */
struct debug_uio_info {
	struct uio_info *info;		/* UIO framework device descriptor   */
	spinlock_t uio_lock;		/* Protects info->mem[] modifications */
	bool overwrite_on_full;		/* Data ring full policy (default: false = drop) */
};

/* -----------------------------------------------------------------------
 * Module-level lookup tables
 *
 * BUG FIX 1: The original declaration
 *     int debug_uio_num_maps_per_device[DEBUG_UIO_DEV_MAX] = {DEBUG_UIO_MAPS_PER_DEV_MAX};
 *   only initialises element [0]; elements [1] and [2] are zero-initialised
 *   by the C standard, meaning devices HOST and NSS would appear to have
 *   0 maps.  All three entries must be explicitly set.
 *
 * BUG FIX 2: The original debug_uio_map_type_str only had one row
 *   ({"interrupt", "data"}) for three devices.  The remaining two rows
 *   were zero-initialised (NULL pointers), causing NULL-pointer dereferences
 *   whenever the HOST or NSS device paths were exercised.
 *
 * BUG FIX 3: Variables defined in a header (without 'static') produce a
 *   "multiple definition" linker error if the header is ever included by
 *   more than one translation unit.  Marking them 'static' gives each
 *   translation unit its own private copy, which is safe here because
 *   debug_uio.h is only included by debug_uio.c.
 * ----------------------------------------------------------------------- */

static int debug_uio_num_maps_per_device[DEBUG_UIO_DEV_MAX] = {
	DEBUG_UIO_MAPS_PER_DEV_MAX,	/* DEBUG_UIO_DEV_FIRMWARE */
	DEBUG_UIO_MAPS_PER_DEV_MAX,	/* DEBUG_UIO_DEV_HOST     */
	DEBUG_UIO_MAPS_PER_DEV_MAX,	/* DEBUG_UIO_DEV_NSS      */
};

static const char *debug_uio_dev_str[DEBUG_UIO_DEV_MAX] = {
	"firmware",	/* DEBUG_UIO_DEV_FIRMWARE */
	"host",		/* DEBUG_UIO_DEV_HOST     */
	"nss",		/* DEBUG_UIO_DEV_NSS      */
};

static const char *debug_uio_map_type_str[DEBUG_UIO_DEV_MAX][DEBUG_UIO_MAPS_PER_DEV_MAX] = {
	{ "firmware_interrupt", "firmware_data" },	/* DEBUG_UIO_DEV_FIRMWARE */
	{ "host_interrupt",     "host_data"     },	/* DEBUG_UIO_DEV_HOST     */
	{ "nss_interrupt",      "nss_data"      },	/* DEBUG_UIO_DEV_NSS      */
};

#endif /* __DEBUG_UIO_H_ */
