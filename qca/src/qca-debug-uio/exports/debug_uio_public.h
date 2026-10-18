/*
**************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
**************************************************************************
*/

#ifndef __DEBUG_UIO_PUBLIC_H_
#define __DEBUG_UIO_PUBLIC_H_

#include <linux/types.h>
#include <linux/notifier.h>

/*
 * debug_uio_public.h
 *
 * Public API for the debug_uio kernel module.
 *
 * Overview
 * --------
 * debug_uio creates one UIO (Userspace I/O) device per registered subsystem.
 * Each device exposes two shared-memory maps to userspace:
 *
 *   Interrupt map  –  (struct debug_uio_interrupt_ring_buffer)
 *             A lock-free circular buffer of interrupt-metadata entries.
 *             Each entry records which device raised an event and where in
 *             the mmap window the associated data lives (offset + size).
 *
 *   Data map  –  (struct debug_uio_data_ring_buffer)
 *             A lock-free circular buffer of raw data payloads.
 *             Each entry holds up to MAX_BUFFER_SIZE bytes of opaque data
 *             written by the kernel driver.
 *
 * Kernel drivers interact with this module through the APIs below.
 * Userspace is notified of new entries via a read() on /dev/uioN, which
 * unblocks whenever uio_event_notify() is called by the kernel.
 *
 * Userspace can also send interrupt events back to kernel drivers through
 * the ioctl(IOCTL_SEND_INTERRUPT) interface on /dev/debug_uio_char_dev.
 *
 * The set of supported devices and maps is defined by the constants
 * DEBUG_UIO_DEV_MAX and DEBUG_UIO_MAPS_PER_DEV_MAX.  Adding a new device
 * or map requires only updating those constants and the corresponding
 * lookup tables — no API changes are needed.
 */

/* -----------------------------------------------------------------------
 * Enumerations
 * ----------------------------------------------------------------------- */

/*
 * debug_uio_dev
 *
 * Identifies which UIO device is being addressed.
 * Used by the memory-management APIs (alloc/get/free) and the notifier
 * registration APIs.  The callback APIs (notify / write_data) use a
 * name string instead so that callers do not need to include this enum.
 * New devices can be added by extending this enum before DEBUG_UIO_DEV_MAX.
 */
typedef enum debug_uio_dev {
	DEBUG_UIO_DEV_FIRMWARE = 0,	/* UIO device for the firmware subsystem  */
	DEBUG_UIO_DEV_HOST,		/* UIO device for the host subsystem      */
	DEBUG_UIO_DEV_NSS,		/* UIO device for the NSS subsystem       */
	DEBUG_UIO_DEV_MAX		/* Sentinel – not a valid device index    */
} debug_uio_dev;

/*
 * debug_uio_map_type
 *
 * Identifies which per-device memory map is being addressed.
 *
 *   INTERRUPT – interrupt ring buffer; written by debug_uio_notify().
 *   DATA      – data ring buffer;      written by debug_uio_write_data().
 *
 * New map types can be added by extending this enum before
 * DEBUG_UIO_MAP_TYPE_MAX and updating DEBUG_UIO_MAPS_PER_DEV_MAX.
 */
typedef enum debug_uio_map_type {
	DEBUG_UIO_MAP_TYPE_INTERRUPT = 0,	/* Map 0: interrupt metadata ring */
	DEBUG_UIO_MAP_TYPE_DATA,		/* Map 1: data payload ring       */
	DEBUG_UIO_MAP_TYPE_MAX			/* Sentinel – not a valid map index */
} debug_uio_map_type;

/* -----------------------------------------------------------------------
 * Memory management APIs
 *
 * These APIs are used by subsystem drivers that need to resize or replace
 * the default PAGE_SIZE allocation that debug_uio creates at module load
 * time.  Under normal operation the default allocation is sufficient and
 * these APIs do not need to be called.
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_alloc_mem() - Allocate (or replace) the kernel memory that
 *                         backs a UIO map.
 *
 * Description:
 *   Allocates a contiguous kernel buffer of at least @map_size bytes
 *   (rounded up to the next PAGE_SIZE boundary) and registers it as the
 *   backing memory for map @map_type of device @dev_type.  If a previous
 *   allocation already exists for that slot it is freed first.
 *
 *   The allocated buffer is zero-filled.  Its physical address is stored
 *   in the UIO framework's mem[] descriptor so that userspace can mmap()
 *   it via /dev/uioN.
 *
 *   Note: coherent (DMA-consistent) allocation is not supported; passing
 *   is_coherent = true will return NULL immediately.
 *
 * Input:
 *   @dev_type    – Target UIO device (a valid debug_uio_dev enum value,
 *                  less than DEBUG_UIO_DEV_MAX).
 *   @map_type    – Target map slot (a valid debug_uio_map_type enum value,
 *                  less than DEBUG_UIO_MAP_TYPE_MAX).
 *   @map_size    – Requested size in bytes.  Internally rounded up to the
 *                  next PAGE_SIZE boundary.
 *   @is_coherent – Must be false.  Coherent allocation is not implemented.
 *
 * Output:
 *   The UIO framework's mem[@map_type] descriptor for @dev_type is updated
 *   with the new physical address, size, and name.
 *
 * Return:
 *   Kernel virtual address of the allocated buffer on success.
 *   NULL if @is_coherent is true, if @map_size is 0, or if the kernel
 *   cannot satisfy the allocation (ENOMEM).
 */
void *debug_uio_alloc_mem(enum debug_uio_dev dev_type, int map_type,
			unsigned long map_size, bool is_coherent);

/**
 * debug_uio_get_mem() - Retrieve the kernel virtual address of an
 *                       already-allocated UIO map.
 *
 * Description:
 *   Returns the kernel virtual address of the memory buffer that currently
 *   backs map @map_type for device @dev_type.  The caller can use this
 *   address to read or write the ring-buffer structures directly from
 *   kernel space without going through the UIO mmap path.
 *
 *   The function holds the device spinlock while reading the address to
 *   guard against a concurrent debug_uio_free_mem() call.
 *
 * Input:
 *   @dev_type – Target UIO device (a valid debug_uio_dev enum value,
 *               less than DEBUG_UIO_DEV_MAX).
 *   @map_type – Target map slot (a valid debug_uio_map_type enum value,
 *               less than DEBUG_UIO_MAP_TYPE_MAX).
 *
 * Output:
 *   None.
 *
 * Return:
 *   Kernel virtual address of the map buffer if the map is allocated.
 *   NULL if the map has not been allocated yet or has been freed.
 */
void *debug_uio_get_mem(enum debug_uio_dev dev_type, int map_type);

/**
 * debug_uio_free_mem() - Release the kernel memory backing a UIO map.
 *
 * Description:
 *   Frees the buffer previously allocated for map @map_type of device
 *   @dev_type and clears the corresponding UIO mem[] descriptor (name,
 *   addr, size are all set to zero/NULL).  After this call the map slot
 *   appears unallocated; any subsequent mmap() attempt by userspace for
 *   that map will fail.
 *
 *   The function holds the device spinlock while modifying the descriptor
 *   to prevent races with concurrent alloc or get calls.
 *
 * Input:
 *   @dev_type – Target UIO device (a valid debug_uio_dev enum value,
 *               less than DEBUG_UIO_DEV_MAX).
 *   @map_type – Target map slot (a valid debug_uio_map_type enum value,
 *               less than DEBUG_UIO_MAP_TYPE_MAX).
 *
 * Output:
 *   The UIO mem[@map_type] descriptor for @dev_type is cleared.
 *
 * Return:
 *   void – no return value.  The function is a no-op if the map was not
 *   allocated.
 */
void debug_uio_free_mem(enum debug_uio_dev dev_type, int map_type);

/* -----------------------------------------------------------------------
 * Kernel-driver callback APIs
 *
 * These are the primary APIs called by subsystem drivers (firmware, host,
 * NSS) to push events and data to userspace.  The caller identifies the
 * target device by its name string ("firmware", "host", or "nss") so that
 * it does not need to include or depend on the debug_uio_dev enum.
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_notify() - Signal an interrupt event to userspace by writing
 *                      metadata into the interrupt ring buffer (map 0).
 *
 * Description:
 *   Called by a kernel driver when an interrupt or notable event occurs
 *   that userspace needs to be informed about.  The function performs the
 *   following steps:
 *
 *     1. Resolves @dev_name to an internal device index by comparing it
 *        against the registered device name strings ("firmware", "host",
 *        "nss").
 *     2. Constructs an interrupt-metadata entry containing the device
 *        index, map type (always DEBUG_UIO_MAP_TYPE_INTERRUPT), @offset,
 *        and @size.
 *     3. Enqueues the entry at the rear of the interrupt ring buffer
 *        (map 0) for the resolved device using a lock-free SPSC algorithm
 *        with smp_wmb() barriers to guarantee visibility.
 *     4. Calls uio_event_notify() to increment the UIO event counter and
 *        wake any userspace thread blocked in read() on /dev/uioN.
 *
 *   Userspace reads the interrupt ring to learn which device raised the
 *   event and at which offset/size in the mmap window the associated data
 *   can be found.
 *
 *   The map type is always INTERRUPT and is therefore not a parameter;
 *   this keeps the calling driver's code simple and avoids misuse.
 *
 * Input:
 *   @dev_name – Null-terminated device name string matching one of the
 *               registered UIO device names (see debug_uio_dev_str[]).
 *   @offset   – Byte offset from the start of the mmap window where the
 *               data associated with this interrupt lives.  Userspace uses
 *               this to locate the data after dequeuing the ring entry.
 *   @size     – Size in bytes of the data at @offset.  Userspace uses this
 *               to know how many bytes to read.
 *
 * Output:
 *   One entry is appended to the interrupt ring buffer of the named device.
 *   Userspace is woken via uio_event_notify().
 *
 * Return:
 *    0          – Success; entry enqueued and userspace notified.
 *   -ENODEV     – @dev_name is NULL or does not match any registered device.
 *   -EINVAL     – Internal UIO info structure is not initialised, or the
 *                 interrupt map has not been allocated.
 *   -ENOSPC     – The interrupt ring buffer is full (all RING_SIZE slots
 *                 are occupied); the entry was dropped.
 */
int debug_uio_notify(const char *dev_name, uint32_t offset, uint32_t size);

/**
 * debug_uio_write_data() - Write a data payload into the data ring buffer
 *                          (map 1) and signal userspace.
 *
 * Description:
 *   Called by a kernel driver to push raw data bytes into the data ring
 *   buffer (map 1) of the named UIO device.  This is the companion to
 *   debug_uio_notify():
 *
 *     - debug_uio_notify()    tells userspace WHERE data is
 *                             (offset + size in the mmap window).
 *     - debug_uio_write_data() gives userspace the actual data bytes
 *                             via the data ring, so userspace can dequeue
 *                             them without an additional mmap read.
 *
 *   The function performs the following steps:
 *
 *     1. Resolves @dev_name to an internal device index.
 *     2. Clamps @data_size to MAX_BUFFER_SIZE (1020 bytes) if it exceeds
 *        that limit.
 *     3. Copies @data_size bytes from @data into a new ring entry at the
 *        rear of the data ring buffer (map 1) using smp_wmb() barriers.
 *     4. Calls uio_event_notify() to wake userspace.
 *
 *   Each ring entry holds one independent payload.  The ring has
 *   MAX_NUM_DATA_BUFFERS (4) slots; if all slots are occupied the entry
 *   is dropped and -ENOSPC is returned.
 *
 * Input:
 *   @dev_name   – Null-terminated device name string matching one of the
 *                 registered UIO device names (see debug_uio_dev_str[]).
 *   @data       – Pointer to the data buffer to copy into the ring.
 *                 May be NULL only if @data_size is 0 (enqueues an empty
 *                 entry as a bare notification).
 *   @data_size  – Number of bytes to copy from @data.  Values larger than
 *                 MAX_BUFFER_SIZE are silently clamped to MAX_BUFFER_SIZE.
 *
 * Output:
 *   One entry containing a copy of @data is appended to the data ring
 *   buffer of the named device.  Userspace is woken via uio_event_notify().
 *
 * Return:
 *    0          – Success; entry enqueued and userspace notified.
 *   -ENODEV     – @dev_name is NULL or does not match any registered device.
 *   -EINVAL     – Internal UIO info structure is not initialised, or the
 *                 data map has not been allocated.
 *   -ENOSPC     – The data ring buffer is full (all MAX_NUM_DATA_BUFFERS
 *                 slots are occupied); the entry was dropped.
 */
int debug_uio_write_data(const char *dev_name,
			const void *data, uint32_t data_size);

/* -----------------------------------------------------------------------
 * Notifier registration APIs
 *
 * These APIs allow kernel drivers to register callbacks that are invoked
 * when userspace sends an interrupt event via the ioctl interface on
 * /dev/debug_uio_char_dev.  This is the reverse direction of
 * debug_uio_notify(): userspace → kernel.
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_register_notifier() - Register a callback to receive interrupt
 *                                 events sent from userspace.
 *
 * Description:
 *   Registers @nb with the blocking notifier chain associated with the
 *   (device, map) pair identified by @dev_type and @map_type.  When
 *   userspace calls ioctl(IOCTL_SEND_INTERRUPT) with a matching uioId and
 *   mapId, the kernel invokes all registered notifier callbacks on that
 *   chain, passing the raw interrupt payload as the data argument.
 *
 *   A driver typically calls this during its probe() or init() function
 *   to subscribe to events from a specific device/map combination.
 *   Multiple drivers may register on the same chain; they are called in
 *   priority order as defined by the notifier_block.priority field.
 *
 * Input:
 *   @nb       – Pointer to the caller's notifier_block.  The .notifier_call
 *               field must point to the callback function with signature:
 *               int callback(struct notifier_block *nb,
 *                            unsigned long action, void *data)
 *               where @data is a pointer to the 4-byte raw payload from
 *               the ioctl struct debug_uio_intr_data.payload.raw.
 *   @dev_type – Device whose notifier chain to register on
 *               (a valid debug_uio_dev enum value less than
 *               DEBUG_UIO_DEV_MAX).
 *   @map_type – Map whose notifier chain to register on
 *               (a valid debug_uio_map_type enum value less than
 *               DEBUG_UIO_MAP_TYPE_MAX).
 *
 * Output:
 *   @nb is inserted into the blocking notifier chain for
 *   notifier_chains[@dev_type][@map_type].
 *
 * Return:
 *    0          – Success; @nb is now registered.
 *   Negative    – Error code from blocking_notifier_chain_register()
 *                 (e.g. -EEXIST if @nb is already registered on this chain).
 */
int debug_uio_register_notifier(struct notifier_block *nb,
			enum debug_uio_dev dev_type, int map_type);

/* -----------------------------------------------------------------------
 * Data ring overwrite mode API
 *
 * By default the data ring uses a drop-on-full policy (backward-compatible
 * with all existing callers).  Drivers that prefer to always capture the
 * most recent events can opt in to overwrite mode on a per-device basis
 * by calling debug_uio_set_overwrite_mode().
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_set_overwrite_mode() - Enable or disable overwrite-on-full for
 *                                  the data ring of a named UIO device.
 *
 * Description:
 *   Controls the behaviour of debug_uio_write_data() when the data ring
 *   buffer (map 1) is full:
 *
 *     @enable = false  (default after module load)
 *       Preserves the original behaviour: the new entry is dropped and
 *       debug_uio_write_data() returns -ENOSPC.  All existing callers
 *       that never invoke this function continue to work unchanged.
 *       This is the backward-compatible default — no action is required
 *       from drivers that want the original drop behaviour.
 *
 *     @enable = true   (opt-in)
 *       The oldest entry in the data ring is silently discarded to make
 *       room for the new one.  The ring always retains the last
 *       MAX_NUM_DATA_BUFFERS events.  debug_uio_write_data() will never
 *       return -ENOSPC while overwrite mode is active for this device.
 *
 *   The setting is per-device and takes effect immediately.  It can be
 *   toggled at runtime (e.g. enabled during a debug session and disabled
 *   afterwards to restore normal back-pressure signalling).
 *
 * Input:
 *   @dev_name – Null-terminated device name string matching one of the
 *               registered UIO device names ("firmware", "host", "nss").
 *   @enable   – true  to enable overwrite-on-full (opt-in).
 *               false to restore the original drop-on-full behaviour.
 *
 * Output:
 *   The overwrite policy for the named device is updated immediately.
 *
 * Return:
 *    0       – Success.
 *   -ENODEV  – @dev_name is NULL or does not match any registered device.
 */
int debug_uio_set_overwrite_mode(const char *dev_name, bool enable);

/**
 * debug_uio_unregister_notifier() - Unregister a previously registered
 *                                   notifier callback.
 *
 * Description:
 *   Removes @nb from the blocking notifier chain associated with the
 *   (device, map) pair identified by @dev_type and @map_type.  After this
 *   call the callback will no longer be invoked when userspace sends an
 *   interrupt event for that (device, map) pair.
 *
 *   A driver typically calls this during its remove() or exit() function
 *   to clean up its subscription before the notifier_block is freed.
 *   Failing to unregister before freeing @nb will cause a use-after-free
 *   when the next ioctl event fires.
 *
 * Input:
 *   @nb       – Pointer to the same notifier_block that was passed to
 *               debug_uio_register_notifier().
 *   @dev_type – Must match the debug_uio_dev value used during registration.
 *   @map_type – Must match the debug_uio_map_type value used during
 *               registration.
 *
 * Output:
 *   @nb is removed from the blocking notifier chain for
 *   notifier_chains[@dev_type][@map_type].
 *
 * Return:
 *    0          – Success; @nb has been removed.
 *   Negative    – Error code from blocking_notifier_chain_unregister()
 *                 (e.g. -ENOENT if @nb was not found on the chain).
 */
int debug_uio_unregister_notifier(struct notifier_block *nb,
			enum debug_uio_dev dev_type, int map_type);

/* -----------------------------------------------------------------------
 * App status notifier registration APIs
 *
 * These APIs let kernel drivers learn when a userspace app announces that
 * it is up or down via ioctl(IOCTL_APP_STATUS) on
 * /dev/debug_uio_char_dev.  Unlike debug_uio_register_notifier(), this
 * chain is global — it is not scoped to a UIO device or map, since the
 * event itself (an app's liveness) isn't tied to either.
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_register_app_status_notifier() - Register a callback to
 *                                            receive userspace-app
 *                                            up/down events.
 *
 * Description:
 *   Registers @nb with the global app-status notifier chain.  When a
 *   userspace app calls ioctl(IOCTL_APP_STATUS), every callback on this
 *   chain is invoked with the app's is_up flag (1 or 0) as the notifier
 *   action and a pointer to the app's NUL-terminated name as the data
 *   argument.  A driver typically calls this during its probe() or
 *   init() function and inspects the app name itself to decide whether
 *   the event is relevant to it.
 *
 *   There is no status replay: a driver that registers after an app has
 *   already signalled "up" will not see that past event, only future
 *   ones — the same behaviour as debug_uio_register_notifier().
 *
 * Input:
 *   @nb – Pointer to the caller's notifier_block.  The .notifier_call
 *         field must point to the callback function with signature:
 *         int callback(struct notifier_block *nb,
 *                      unsigned long action, void *data)
 *         where @action is the is_up flag (0/1) and @data is a
 *         const char * pointing to the NUL-terminated app name.  The
 *         pointer is only valid for the duration of the callback — copy
 *         the name if it needs to be kept.
 *
 * Output:
 *   @nb is inserted into the global app-status notifier chain.
 *
 * Return:
 *    0          – Success; @nb is now registered.
 *   Negative    – Error code from blocking_notifier_chain_register()
 *                 (e.g. -EEXIST if @nb is already registered on this
 *                 chain).
 */
int debug_uio_register_app_status_notifier(struct notifier_block *nb);

/**
 * debug_uio_unregister_app_status_notifier() - Unregister a previously
 *                                              registered app-status
 *                                              callback.
 *
 * Description:
 *   Removes @nb from the global app-status notifier chain.  After this
 *   call the callback will no longer be invoked when a userspace app
 *   sends an IOCTL_APP_STATUS event.
 *
 *   A driver typically calls this during its remove() or exit() function
 *   to clean up its subscription before the notifier_block is freed.
 *   Failing to unregister before freeing @nb will cause a use-after-free
 *   when the next IOCTL_APP_STATUS event fires.
 *
 * Input:
 *   @nb – Pointer to the same notifier_block that was passed to
 *         debug_uio_register_app_status_notifier().
 *
 * Output:
 *   @nb is removed from the global app-status notifier chain.
 *
 * Return:
 *    0          – Success; @nb has been removed.
 *   Negative    – Error code from blocking_notifier_chain_unregister()
 *                 (e.g. -ENOENT if @nb was not found on the chain).
 */
int debug_uio_unregister_app_status_notifier(struct notifier_block *nb);

#endif /* __DEBUG_UIO_PUBLIC_H_ */
