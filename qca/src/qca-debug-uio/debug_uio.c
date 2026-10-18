/*
**************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
**************************************************************************
*/

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uio_driver.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/notifier.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/major.h>
#include <linux/atomic.h>
#include <asm/barrier.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>
#include "debug_uio.h"

/* -----------------------------------------------------------------------
 * Device Tree compatible strings
 * ----------------------------------------------------------------------- */

#define DEBUG_UIO_DT_COMPAT_FIRMWARE "qcom,debug-uio-firmware"
#define DEBUG_UIO_DT_COMPAT_HOST     "qcom,debug-uio-host"
#define DEBUG_UIO_DT_COMPAT_NSS      "qcom,debug-uio-nss"

static const char *debug_uio_dt_compat[DEBUG_UIO_DEV_MAX] = {
	DEBUG_UIO_DT_COMPAT_FIRMWARE,
	DEBUG_UIO_DT_COMPAT_HOST,
	DEBUG_UIO_DT_COMPAT_NSS,
};

/* -----------------------------------------------------------------------
 * Device Tree memory tracking structure
 * ----------------------------------------------------------------------- */

/**
 * struct debug_uio_dt_mem - Per-device device tree memory information
 * @np:            Device tree node pointer
 * @rmem:          Reserved memory region information
 * @base_addr:     Virtual base address (from ioremap)
 * @is_dt_memory:  true if memory is from device tree, false if kzalloc'd
 */
struct debug_uio_dt_mem {
	struct device_node *np;
	struct reserved_mem *rmem;
	void __iomem *base_addr;
	bool is_dt_memory;
};

/* -----------------------------------------------------------------------
 * Module-level state
 * ----------------------------------------------------------------------- */

static struct class *dump_class;
static int dump_major;

/*
 * info_global[] – one entry per UIO device (firmware / host / nss).
 * Each entry wraps the UIO framework's uio_info plus a spinlock that
 * protects the mem[] descriptor array from concurrent alloc/free calls.
 */
static struct debug_uio_info info_global[DEBUG_UIO_DEV_MAX];

/*
 * dev_global[] – the struct device registered with the driver core for
 * each UIO device.  The UIO framework requires a parent struct device
 * when registering a uio_info.
 */
static struct device *dev_global[DEBUG_UIO_DEV_MAX];

/*
 * mem[][] – kernel virtual-address pointers to the kzalloc'd pages that
 * back each (device, map) slot.  Indexed [dev_type][map_type].
 * Stored separately from info->mem[].addr (which holds the physical
 * address) so that kfree() can be called directly without a
 * phys_to_virt() round-trip.
 */
static void *mem[DEBUG_UIO_DEV_MAX][DEBUG_UIO_MAPS_PER_DEV_MAX];

/*
 * notifier_chains[][] – one blocking notifier head per (device, map) pair.
 * Kernel drivers register callbacks here to receive interrupt events that
 * originate from userspace via the IOCTL_SEND_INTERRUPT ioctl.
 */
static struct blocking_notifier_head
	notifier_chains[DEBUG_UIO_DEV_MAX][DEBUG_UIO_MAPS_PER_DEV_MAX];

/*
 * app_status_notifier_chain – single global chain (not indexed by
 * device/map) for the userspace-app-is-up/down signal sent via
 * IOCTL_APP_STATUS.  Subsystem drivers register on this chain instead of
 * notifier_chains[][] because the event isn't tied to a specific UIO
 * device or map.
 */
static struct blocking_notifier_head app_status_notifier_chain;

/*
 * dt_mem[] – device tree memory tracking for each UIO device.
 * Stores device tree node, reserved memory info, and virtual base address
 * for devices that use device tree memory allocation.
 */
static struct debug_uio_dt_mem dt_mem[DEBUG_UIO_DEV_MAX];

/* -----------------------------------------------------------------------
 * UIO framework callbacks
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_dev_release() - Driver-core release callback for a UIO device.
 *
 * Description:
 *   Called by the driver core when the last reference to the struct device
 *   is dropped (i.e. after device_unregister() completes).  The struct
 *   device memory is managed by dev_global[] and freed separately in
 *   debug_uio_cleanup_dev(), so nothing needs to be done here.
 *
 * Input:
 *   @dev – Pointer to the struct device being released.
 *
 * Output:
 *   None.
 *
 * Return:
 *   void.
 */
static void debug_uio_dev_release(struct device *dev)
{
	return;
}

/**
 * debug_uio_open() - UIO open callback; called when userspace opens /dev/uioN.
 *
 * Description:
 *   Invoked by the UIO framework each time a userspace process opens the
 *   character device node for this UIO device.  No per-open state is
 *   required, so the function returns success immediately.
 *
 * Input:
 *   @info  – Pointer to the uio_info descriptor for this device.
 *   @inode – Inode of the opened device node.
 *
 * Output:
 *   None.
 *
 * Return:
 *   0 always (success).
 */
static int debug_uio_open(struct uio_info *info, struct inode *inode)
{
	return 0;
}

/**
 * debug_uio_release() - UIO release callback; called when userspace closes
 *                       /dev/uioN.
 *
 * Description:
 *   Invoked by the UIO framework when the last file descriptor for this
 *   UIO device is closed by userspace.  No per-open state was allocated
 *   in debug_uio_open(), so nothing needs to be cleaned up here.
 *
 * Input:
 *   @info  – Pointer to the uio_info descriptor for this device.
 *   @inode – Inode of the closed device node.
 *
 * Output:
 *   None.
 *
 * Return:
 *   0 always (success).
 */
static int debug_uio_release(struct uio_info *info, struct inode *inode)
{
	return 0;
}

/**
 * debug_uio_irq_handler() - UIO IRQ handler.
 *
 * Description:
 *   Registered as the IRQ handler for each UIO device.  Because the IRQ
 *   type is set to UIO_IRQ_CUSTOM, the UIO framework never installs a real
 *   hardware IRQ line; instead this handler is invoked internally whenever
 *   uio_event_notify() is called from kernel code (e.g. from
 *   debug_uio_notify() or debug_uio_write_data()).  The handler simply
 *   logs the event at INFO level and returns IRQ_HANDLED.
 *
 * Input:
 *   @irq  – IRQ number (always UIO_IRQ_CUSTOM for this driver).
 *   @info – Pointer to the uio_info descriptor for the device that
 *           generated the event.
 *
 * Output:
 *   None.
 *
 * Return:
 *   IRQ_HANDLED always.
 */
static irqreturn_t debug_uio_irq_handler(int irq, struct uio_info *info)
{
	pr_info("irq: %d.", irq);
	return IRQ_HANDLED;
}

/**
 * debug_uio_irq_control() - UIO IRQ enable/disable callback.
 *
 * Description:
 *   Called by the UIO framework to enable (@irq_on = 1) or disable
 *   (@irq_on = 0) the IRQ for a UIO device.  Because UIO_IRQ_CUSTOM
 *   devices do not use a real hardware IRQ line, no action is required.
 *
 * Input:
 *   @info   – Pointer to the uio_info descriptor for this device.
 *   @irq_on – 1 to enable the IRQ, 0 to disable it.
 *
 * Output:
 *   None.
 *
 * Return:
 *   0 always (success).
 */
static int debug_uio_irq_control(struct uio_info *info, s32 irq_on)
{
	return 0;
}

/**
 * debug_uio_mmap() - Custom mmap handler for UIO memory maps.
 *
 * Description:
 *   Called by the UIO framework when userspace calls mmap() on /dev/uioN.
 *   The UIO framework encodes the zero-based map index in vma->vm_pgoff
 *   (map 0 = interrupt ring, map 1 = data ring).  This function uses that
 *   index to look up the physical address stored in info->mem[vm_pgoff].addr
 *   and calls remap_pfn_range() to map the corresponding physical page(s)
 *   into the userspace virtual address range described by @vma.
 *
 * Input:
 *   @info – Pointer to the uio_info descriptor; info->mem[] holds the
 *           physical addresses and sizes of all maps for this device.
 *   @vma  – The vm_area_struct describing the userspace mapping request.
 *           vma->vm_pgoff is the zero-based map index.
 *           vma->vm_start / vm_end define the target virtual address range.
 *
 * Output:
 *   The physical pages backing the selected map are mapped into the
 *   userspace virtual address range [vma->vm_start, vma->vm_end).
 *
 * Return:
 *   0 on success.
 *   Non-zero (negative errno) if remap_pfn_range() fails.
 */
static int debug_uio_mmap(struct uio_info *info, struct vm_area_struct *vma)
{
	u32 ret;
	unsigned long pfn;

	pfn = (info->mem[vma->vm_pgoff].addr) >> PAGE_SHIFT;

	ret = remap_pfn_range(vma, vma->vm_start, pfn,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (ret)
		pr_info("remap_pfn_range failed for map %lu\n", vma->vm_pgoff);

	return ret;
}

/* -----------------------------------------------------------------------
 * Memory helpers
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_round_up_to_page_size() - Round a byte count up to the next
 *                                     PAGE_SIZE boundary.
 *
 * Description:
 *   Ensures that every allocation passed to remap_pfn_range() covers
 *   whole pages.  Partial-page allocations cannot be safely remapped
 *   because the kernel page allocator works at page granularity.
 *
 * Input:
 *   @size – Requested size in bytes.
 *
 * Output:
 *   None.
 *
 * Return:
 *   @size rounded up to the nearest multiple of PAGE_SIZE.
 *   If @size is already a multiple of PAGE_SIZE it is returned unchanged.
 */
static size_t debug_uio_round_up_to_page_size(size_t size)
{
	return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/**
 * debug_uio_alloc_mem() - Allocate (or replace) the kernel memory that
 *                         backs a UIO map slot.
 *
 * Description:
 *   Allocates a contiguous, zero-filled kernel buffer of at least
 *   @map_size bytes (rounded up to PAGE_SIZE) and registers it as the
 *   backing memory for map @map_type of device @dev_type.  If a previous
 *   allocation already exists for that slot it is freed first.  The
 *   physical address of the new buffer is stored in the UIO framework's
 *   mem[@map_type] descriptor so that userspace can mmap() it.
 *   Coherent (DMA-consistent) allocation is not supported.
 *
 * Input:
 *   @dev_type    – Target UIO device (DEBUG_UIO_DEV_FIRMWARE /
 *                  DEBUG_UIO_DEV_HOST / DEBUG_UIO_DEV_NSS).
 *   @map_type    – Target map slot (DEBUG_UIO_MAP_TYPE_INTERRUPT = 0,
 *                  DEBUG_UIO_MAP_TYPE_DATA = 1).
 *   @map_size    – Requested size in bytes; rounded up to PAGE_SIZE.
 *   @is_coherent – Must be false; coherent allocation is not implemented.
 *
 * Output:
 *   info_global[dev_type].info->mem[map_type] is updated with the new
 *   physical address, size, memtype, and name.
 *   mem[dev_type][map_type] is updated with the new virtual address.
 *
 * Return:
 *   Kernel virtual address of the allocated buffer on success.
 *   NULL if @is_coherent is true, or if kzalloc() fails (ENOMEM).
 */
void *debug_uio_alloc_mem(enum debug_uio_dev dev_type,
			int map_type, unsigned long map_size, bool is_coherent)
{
	void *memory;
	struct uio_info *info;
	unsigned long flags;

	if (is_coherent) {
		pr_info("Coherent allocation not supported.\n");
		return NULL;
	}

	map_size = debug_uio_round_up_to_page_size(map_size);
	memory = kzalloc(map_size, GFP_KERNEL);
	if (!memory) {
		pr_info("Memory allocation failed for %s.\n",
			debug_uio_map_type_str[dev_type][map_type]);
		return NULL;
	}

	info = info_global[dev_type].info;

	spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

	/*
	 * Free any previous allocation for this slot.
	 * Use mem[][] (virtual address) directly rather than converting
	 * the stored physical address back with phys_to_virt().
	 */
	if (mem[dev_type][map_type]) {
		kfree(mem[dev_type][map_type]);
		mem[dev_type][map_type] = NULL;
	}

	info->mem[map_type].addr    = (phys_addr_t)virt_to_phys(memory);
	info->mem[map_type].size    = map_size;
	info->mem[map_type].memtype = UIO_MEM_LOGICAL;
	info->mem[map_type].name    = debug_uio_map_type_str[dev_type][map_type];

	mem[dev_type][map_type] = memory;

	spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
	return memory;
}
EXPORT_SYMBOL(debug_uio_alloc_mem);

/**
 * debug_uio_get_mem() - Retrieve the kernel virtual address of an
 *                       already-allocated UIO map.
 *
 * Description:
 *   Returns the kernel virtual address of the buffer currently backing
 *   map @map_type for device @dev_type.  Kernel drivers can use this
 *   address to read or write the ring-buffer structures directly without
 *   going through the UIO mmap path.  The device spinlock is held during
 *   the lookup to guard against a concurrent debug_uio_free_mem() call.
 *
 * Input:
 *   @dev_type – Target UIO device (DEBUG_UIO_DEV_FIRMWARE /
 *               DEBUG_UIO_DEV_HOST / DEBUG_UIO_DEV_NSS).
 *   @map_type – Target map slot (DEBUG_UIO_MAP_TYPE_INTERRUPT = 0,
 *               DEBUG_UIO_MAP_TYPE_DATA = 1).
 *
 * Output:
 *   None.
 *
 * Return:
 *   Kernel virtual address of the map buffer if the map is allocated.
 *   NULL if the map has not been allocated or has already been freed.
 */
void *debug_uio_get_mem(enum debug_uio_dev dev_type, int map_type)
{
	struct uio_info *info;
	unsigned long flags;
	void *addr = NULL;

	info = info_global[dev_type].info;

	spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

	if (info->mem[map_type].name)
		addr = mem[dev_type][map_type];

	spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
	return addr;
}
EXPORT_SYMBOL(debug_uio_get_mem);

/**
 * debug_uio_free_mem() - Release the kernel memory backing a UIO map and
 *                        clear the map descriptor.
 *
 * Description:
 *   Frees the buffer previously allocated for map @map_type of device
 *   @dev_type and clears the corresponding UIO mem[] descriptor (name,
 *   addr, and size are set to NULL/0).  After this call the map slot
 *   appears unallocated; any subsequent mmap() attempt by userspace for
 *   that map will fail.  The device spinlock is held during the operation
 *   to prevent races with concurrent alloc or get calls.  The function
 *   is a no-op if the slot was not allocated.
 *
 * Input:
 *   @dev_type – Target UIO device (DEBUG_UIO_DEV_FIRMWARE /
 *               DEBUG_UIO_DEV_HOST / DEBUG_UIO_DEV_NSS).
 *   @map_type – Target map slot (DEBUG_UIO_MAP_TYPE_INTERRUPT = 0,
 *               DEBUG_UIO_MAP_TYPE_DATA = 1).
 *
 * Output:
 *   mem[dev_type][map_type] set to NULL.
 *   info->mem[map_type].{name, addr, size} cleared to NULL/0.
 *
 * Return:
 *   void.
 */
void debug_uio_free_mem(enum debug_uio_dev dev_type, int map_type)
{
	struct uio_info *info;
	unsigned long flags;

	info = info_global[dev_type].info;

	spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

	if (mem[dev_type][map_type]) {
		kfree(mem[dev_type][map_type]);
		mem[dev_type][map_type] = NULL;

		/* Clear the UIO map descriptor so the slot looks unallocated. */
		info->mem[map_type].name = NULL;
		info->mem[map_type].addr = 0;
		info->mem[map_type].size = 0;
	}

	spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
}
EXPORT_SYMBOL(debug_uio_free_mem);

/* -----------------------------------------------------------------------
 * Ring-buffer helpers
 *
 * Architecture
 * ============
 * Each UIO device exposes two shared-memory maps to userspace:
 *
 *   Map 0 – interrupt ring  (struct debug_uio_interrupt_ring_buffer)
 *     Written by the kernel via debug_uio_notify() when a driver event
 *     occurs.  Each entry records which device/map raised the event and
 *     where in the mmap window the associated data lives (offset + size).
 *
 *   Map 1 – data ring  (struct debug_uio_data_ring_buffer)
 *     Written by the kernel via debug_uio_write_data() with the actual
 *     payload bytes.  Userspace dequeues entries after being woken by a
 *     read() on /dev/uioN.
 *
 * Both rings are lock-free SPSC (single-producer / single-consumer)
 * queues.  The kernel is always the sole producer; userspace is always
 * the sole consumer.  Atomic reads/writes combined with smp_rmb() /
 * smp_wmb() barriers ensure correct ordering without a mutex.
 *
 * Ring invariant:
 *   FULL  when (rear + 1) % CAPACITY == front
 *   EMPTY when rear == front
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_ring_buffer_init() - Zero-fill a ring buffer and reset its
 *                                front and rear indices to 0.
 *
 * Description:
 *   Called once per map slot during module initialisation (debug_uio_init).
 *   Selects the correct ring-buffer struct layout based on @map_type
 *   (interrupt ring for map 0, data ring for map 1), zeroes the payload
 *   array with memset(), and initialises the atomic front and rear
 *   counters to 0 so the ring starts in the EMPTY state.
 *
 * Input:
 *   @buffer   – Kernel virtual address of the page allocated for this map.
 *               The ring-buffer struct is overlaid at the start of this page.
 *   @map_type – DEBUG_UIO_MAP_TYPE_INTERRUPT (0) selects
 *               struct debug_uio_interrupt_ring_buffer.
 *               Any other value selects
 *               struct debug_uio_data_ring_buffer.
 *
 * Output:
 *   The ring-buffer struct at @buffer is zero-filled and its front/rear
 *   atomics are set to 0.
 *
 * Return:
 *   void.
 */
static void debug_uio_ring_buffer_init(void *buffer, int map_type)
{
	if (map_type == DEBUG_UIO_MAP_TYPE_INTERRUPT) {
		struct debug_uio_interrupt_ring_buffer *rb =
			(struct debug_uio_interrupt_ring_buffer *)buffer;

		memset(rb->buffer, 0, sizeof(rb->buffer));
		atomic_set(&rb->front, 0);
		atomic_set(&rb->rear,  0);
	} else {
		struct debug_uio_data_ring_buffer *rb =
			(struct debug_uio_data_ring_buffer *)buffer;

		memset(rb->buffer, 0, sizeof(rb->buffer));
		atomic_set(&rb->front, 0);
		atomic_set(&rb->rear,  0);
	}
}

/**
 * debug_uio_interrupt_ring_enqueue() - Enqueue one interrupt-metadata
 *                                      entry into the interrupt ring (map 0).
 *
 * Description:
 *   Implements the producer side of the lock-free SPSC interrupt ring.
 *   Reads the current rear and front indices under smp_rmb() to ensure
 *   the latest consumer-written front value is visible.  If the ring is
 *   not full, populates a debug_uio_intr_data entry with the supplied
 *   device index, map type, offset, and size, copies it into
 *   rb->buffer[rear], then advances the rear pointer under smp_wmb() so
 *   the consumer sees the new entry only after it is fully written.
 *
 * Input:
 *   @rb       – Pointer to the interrupt ring buffer (start of the
 *               interrupt map page).  Must not be NULL.
 *   @dev_type – Device index (as resolved by debug_uio_find_dev_by_name())
 *               stored in the entry's uioId field.
 *   @map_type – Map type stored in the entry's mapId field.  Always
 *               DEBUG_UIO_MAP_TYPE_INTERRUPT when called from
 *               debug_uio_notify().
 *   @offset   – Byte offset from the map base stored in the entry's
 *               offset_from_base field.
 *   @size     – Data size in bytes stored in the entry's
 *               size_in_bytes_to_read field.
 *
 * Output:
 *   One debug_uio_intr_data entry is written to rb->buffer[rear] and
 *   rb->rear is advanced to next_rear.
 *
 * Return:
 *   true  – Entry successfully enqueued.
 *   false – @rb is NULL, or the ring is full (all RING_SIZE slots occupied).
 */
static bool debug_uio_interrupt_ring_enqueue(
		struct debug_uio_interrupt_ring_buffer *rb,
		int dev_type, int map_type,
		uint32_t offset, uint32_t size)
{
	int front, rear, next_rear;
	struct debug_uio_intr_data entry;

	if (!rb) {
		pr_info("interrupt ring buffer pointer is NULL\n");
		return false;
	}

	/*
	 * Read barrier: ensure we see the latest front value written by
	 * the consumer (userspace) before we check for fullness.
	 */
	smp_rmb();
	rear      = atomic_read(&rb->rear);
	front     = atomic_read(&rb->front);
	next_rear = (rear + 1) % RING_SIZE;

	if (next_rear == front) {
		pr_info("interrupt ring buffer is full (dev=%d)\n", dev_type);
		return false;
	}

	/* Populate the entry. */
	entry.uioId                 = (uint8_t)dev_type;
	entry.mapId                 = (uint8_t)map_type;
	entry.offset_from_base      = offset;
	entry.size_in_bytes_to_read = size;
	memset(entry.payload.raw, 0, sizeof(entry.payload.raw));

	memcpy(&rb->buffer[rear], &entry, sizeof(struct debug_uio_intr_data));

	/*
	 * Write barrier: ensure the entry bytes are visible to the consumer
	 * before we advance the rear pointer.  A second wmb after the
	 * atomic_set ensures the updated rear is flushed to memory.
	 */
	smp_wmb();
	atomic_set(&rb->rear, next_rear);
	smp_wmb();

	return true;
}

/**
 * debug_uio_data_ring_enqueue() - Enqueue one data payload entry into the
 *                                 data ring (map 1).
 *
 * Description:
 *   Implements the producer side of the lock-free SPSC data ring.
 *   Reads the current rear and front indices under smp_rmb().  If the
 *   ring is not full, clamps @data_size to MAX_BUFFER_SIZE, copies
 *   @data_size bytes from @data_buf into a local debug_uio_data entry,
 *   writes the entry to rb->buffer[rear], then advances the rear pointer
 *   under smp_wmb().  If @data_buf is NULL or @data_size is 0 an empty
 *   (zero-filled) entry is enqueued as a bare notification token.
 *
 *   When @overwrite is true and the ring is full, the oldest entry is
 *   silently discarded by advancing the front index before writing the
 *   new entry.  This keeps the ring at MAX_NUM_DATA_BUFFERS - 1 live
 *   entries and always preserves the most recent events.
 *
 *   When @overwrite is false (the default / backward-compatible path),
 *   a full ring causes the new entry to be dropped and false is returned,
 *   identical to the original behaviour.
 *
 * Input:
 *   @rb        – Pointer to the data ring buffer (start of map 1 page).
 *                Must not be NULL.
 *   @data_buf  – Pointer to the payload bytes to copy.  May be NULL if
 *                @data_size is 0.
 *   @data_size – Number of bytes to copy from @data_buf.  Silently clamped
 *                to MAX_BUFFER_SIZE if larger.
 *   @overwrite – If true, overwrite the oldest entry when the ring is full
 *                instead of dropping the new entry.
 *                If false, preserve the original drop-on-full behaviour.
 *
 * Output:
 *   One debug_uio_data entry is written to rb->buffer[rear] and rb->rear
 *   is advanced to next_rear.  When @overwrite is true and the ring was
 *   full, rb->front is also advanced to discard the oldest entry.
 *
 * Return:
 *   true  – Entry successfully enqueued.
 *   false – @rb is NULL, or the ring is full and @overwrite is false
 *           (all MAX_NUM_DATA_BUFFERS slots occupied, entry dropped).
 */
static bool debug_uio_data_ring_enqueue(
		struct debug_uio_data_ring_buffer *rb,
		const void *data_buf, uint32_t data_size,
		bool overwrite)
{
	int front, rear, next_rear;
	struct debug_uio_data entry;

	if (!rb) {
		pr_info("data ring buffer pointer is NULL\n");
		return false;
	}

	smp_rmb();
	rear      = atomic_read(&rb->rear);
	front     = atomic_read(&rb->front);
	next_rear = (rear + 1) % MAX_NUM_DATA_BUFFERS;

	if (next_rear == front) {
		if (!overwrite) {
			/*
			 * Backward-compatible default: drop the new entry
			 * and let the caller return -ENOSPC, exactly as the
			 * original code did.
			 */
			pr_info("data ring buffer is full\n");
			return false;
		}

		/*
		 * Overwrite mode (opt-in): discard the oldest entry by
		 * advancing front, then fall through to write the new entry
		 * at rear.  The ring always retains the last
		 * MAX_NUM_DATA_BUFFERS - 1 live entries.
		 */
		pr_info("data ring buffer full, overwriting oldest entry\n");
		smp_wmb();
		atomic_set(&rb->front, (front + 1) % MAX_NUM_DATA_BUFFERS);
		smp_wmb();
	}

	/* Clamp and copy payload. */
	if (data_size > MAX_BUFFER_SIZE)
		data_size = MAX_BUFFER_SIZE;

	memset(&entry, 0, sizeof(entry));
	if (data_buf && data_size)
		memcpy(entry.payload.data, data_buf, data_size);

	memcpy(&rb->buffer[rear], &entry, sizeof(struct debug_uio_data));

	smp_wmb();
	atomic_set(&rb->rear, next_rear);
	smp_wmb();

	return true;
}

/* -----------------------------------------------------------------------
 * Public kernel API (exported symbols)
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_find_dev_by_name() - Resolve a device name string to an
 *                                internal device index.
 *
 * Description:
 *   Iterates over debug_uio_dev_str[] and returns the index of the first
 *   entry that matches @dev_name using strcmp().  By centralising the
 *   name-to-index translation here, calling drivers do not need to include
 *   or depend on the debug_uio_dev enum — they only need to know the
 *   agreed device name string registered in debug_uio_dev_str[].
 *
 * Input:
 *   @dev_name – Null-terminated device name string to look up.
 *               May be NULL (returns -ENODEV immediately).
 *
 * Output:
 *   None.
 *
 * Return:
 *   Valid device index on success.
 *   -ENODEV if @dev_name is NULL or does not match any registered device.
 */
static int debug_uio_find_dev_by_name(const char *dev_name)
{
	int i;

	if (!dev_name)
		return -ENODEV;

	for (i = 0; i < DEBUG_UIO_DEV_MAX; i++) {
		if (strcmp(debug_uio_dev_str[i], dev_name) == 0)
			return i;
	}

	pr_info("Unknown UIO device name \"%s\"\n", dev_name);
	return -ENODEV;
}

/**
 * debug_uio_notify() - Signal an interrupt event to userspace by writing
 *                      metadata into the interrupt ring buffer (map 0).
 *
 * Description:
 *   The primary kernel→userspace interrupt callback.  Called by a kernel
 *   driver when an interrupt or notable event occurs that userspace needs
 *   to be informed about.  The function:
 *     1. Resolves @dev_name to a device index via
 *        debug_uio_find_dev_by_name().
 *     2. Constructs a debug_uio_intr_data entry with the device index,
 *        DEBUG_UIO_MAP_TYPE_INTERRUPT, @offset, and @size.
 *     3. Enqueues the entry at the rear of the interrupt ring buffer
 *        using the lock-free SPSC algorithm with smp_wmb() barriers.
 *     4. Calls uio_event_notify() to increment the UIO event counter and
 *        wake any userspace thread blocked in read() on /dev/uioN.
 *   The map type is always INTERRUPT and is not a parameter, keeping the
 *   calling driver's code simple and preventing misuse.
 *
 * Input:
 *   @dev_name – Null-terminated device name string matching one of the
 *               registered UIO device names (see debug_uio_dev_str[]).
 *   @offset   – Byte offset from the start of the mmap window where the
 *               data associated with this interrupt lives.
 *   @size     – Size in bytes of the data at @offset.
 *
 * Output:
 *   One debug_uio_intr_data entry appended to the interrupt ring buffer
 *   of the named device.  Userspace woken via uio_event_notify().
 *
 * Return:
 *    0       – Success; entry enqueued and userspace notified.
 *   -ENODEV  – @dev_name is NULL or not recognised.
 *   -EINVAL  – UIO info not initialised, or interrupt map not allocated.
 *   -ENOSPC  – Interrupt ring is full; entry dropped.
 */
int debug_uio_notify(const char *dev_name, uint32_t offset, uint32_t size)
{
	int dev_type;
	struct uio_info *info;
	struct debug_uio_interrupt_ring_buffer *intr_rb;

	dev_type = debug_uio_find_dev_by_name(dev_name);
	if (dev_type < 0)
		return dev_type;

	info = info_global[dev_type].info;
	if (!info) {
		pr_info("UIO info not initialised for \"%s\"\n", dev_name);
		return -EINVAL;
	}

	if (!info->mem[DEBUG_UIO_MAP_TYPE_INTERRUPT].addr) {
		pr_info("Interrupt map not allocated for \"%s\"\n", dev_name);
		return -EINVAL;
	}

	/* Use stored virtual address directly instead of phys_to_virt() */
	intr_rb = (struct debug_uio_interrupt_ring_buffer *)
		mem[dev_type][DEBUG_UIO_MAP_TYPE_INTERRUPT];

	/*
	 * map_type in the ring entry is always INTERRUPT because this
	 * function is exclusively for interrupt-ring writes.
	 */
	if (!debug_uio_interrupt_ring_enqueue(intr_rb, dev_type,
					      DEBUG_UIO_MAP_TYPE_INTERRUPT,
					      offset, size)) {
		pr_info("Interrupt ring enqueue failed for \"%s\"\n", dev_name);
		return -ENOSPC;
	}

	/* Wake userspace. */
	uio_event_notify(info);

	return 0;
}
EXPORT_SYMBOL(debug_uio_notify);

/**
 * debug_uio_write_data() - Write a data payload into the data ring buffer
 *                          (map 1) and signal userspace.
 *
 * Description:
 *   Companion to debug_uio_notify().  Called by a kernel driver to push
 *   raw data bytes into the data ring buffer of the named UIO device.
 *   While debug_uio_notify() tells userspace WHERE data is (offset + size
 *   in the mmap window), debug_uio_write_data() gives userspace the actual
 *   bytes via the data ring so userspace can dequeue them without an
 *   additional mmap read.  The function:
 *     1. Resolves @dev_name to a device index.
 *     2. Clamps @data_size to MAX_BUFFER_SIZE if it exceeds that limit.
 *     3. Copies @data_size bytes from @data into a new ring entry at the
 *        rear of the data ring buffer with smp_wmb() barriers.
 *     4. Calls uio_event_notify() to wake userspace.
 *
 * Input:
 *   @dev_name  – Null-terminated device name string matching one of the
 *                registered UIO device names (see debug_uio_dev_str[]).
 *   @data      – Pointer to the payload bytes to enqueue.  May be NULL
 *                only if @data_size is 0.
 *   @data_size – Number of bytes to copy.  Clamped to MAX_BUFFER_SIZE
 *                if larger.
 *
 * Output:
 *   One debug_uio_data entry containing a copy of @data appended to the
 *   data ring buffer of the named device.  Userspace woken via
 *   uio_event_notify().
 *
 * Return:
 *    0       – Success; entry enqueued and userspace notified.
 *   -ENODEV  – @dev_name is NULL or not recognised.
 *   -EINVAL  – UIO info not initialised, or data map not allocated.
 *   -ENOSPC  – Data ring is full (all MAX_NUM_DATA_BUFFERS slots occupied);
 *              entry dropped.
 */
int debug_uio_write_data(const char *dev_name,
			const void *data, uint32_t data_size)
{
	int dev_type;
	struct uio_info *info;
	struct debug_uio_data_ring_buffer *data_rb;

	dev_type = debug_uio_find_dev_by_name(dev_name);
	if (dev_type < 0)
		return dev_type;

	info = info_global[dev_type].info;
	if (!info) {
		pr_info("UIO info not initialised for \"%s\"\n", dev_name);
		return -EINVAL;
	}

	if (!info->mem[DEBUG_UIO_MAP_TYPE_DATA].addr) {
		pr_info("Data map not allocated for \"%s\"\n", dev_name);
		return -EINVAL;
	}

	/* Use stored virtual address directly instead of phys_to_virt() */
	data_rb = (struct debug_uio_data_ring_buffer *)
		mem[dev_type][DEBUG_UIO_MAP_TYPE_DATA];

	/*
	 * Pass the per-device overwrite flag.  Callers that have never
	 * called debug_uio_set_overwrite_mode() get overwrite_on_full==false
	 * (kzalloc zero-initialises the struct), preserving the original
	 * drop-on-full / -ENOSPC behaviour.
	 */
	if (!debug_uio_data_ring_enqueue(data_rb, data, data_size,
					 info_global[dev_type].overwrite_on_full)) {
		pr_info("Data ring enqueue failed for \"%s\"\n", dev_name);
		return -ENOSPC;
	}

	/* Wake userspace. */
	uio_event_notify(info);

	return 0;
}
EXPORT_SYMBOL(debug_uio_write_data);

/**
 * debug_uio_register_notifier() - Register a callback to receive interrupt
 *                                 events sent from userspace.
 *
 * Description:
 *   Registers @nb with the blocking notifier chain for the (device, map)
 *   pair identified by @dev_type and @map_type.  When userspace calls
 *   ioctl(IOCTL_SEND_INTERRUPT) with a matching uioId and mapId, the
 *   kernel fires all callbacks on that chain, passing the 4-byte raw
 *   payload as the data argument.  Multiple drivers may register on the
 *   same chain; they are called in notifier_block.priority order.
 *   Typically called from a driver's probe() or init() function.
 *
 * Input:
 *   @nb       – Caller's notifier_block with .notifier_call set to the
 *               callback function:
 *               int cb(struct notifier_block *nb, unsigned long action,
 *                      void *data)
 *               where @data points to debug_uio_intr_data.payload.raw.
 *   @dev_type – Device whose chain to register on (a valid debug_uio_dev
 *               enum value less than DEBUG_UIO_DEV_MAX).
 *   @map_type – Map whose chain to register on (a valid debug_uio_map_type
 *               enum value less than DEBUG_UIO_MAP_TYPE_MAX).
 *
 * Output:
 *   @nb inserted into notifier_chains[@dev_type][@map_type].
 *
 * Return:
 *    0        – Success.
 *   Negative  – Error from blocking_notifier_chain_register() (e.g.
 *               -EEXIST if @nb is already on this chain).
 */
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
 *
 *     @enable = true   (opt-in)
 *       The oldest entry in the data ring is silently discarded to make
 *       room for the new one.  The ring always retains the last
 *       MAX_NUM_DATA_BUFFERS events.  debug_uio_write_data() never
 *       returns -ENOSPC while overwrite mode is active.
 *
 *   The flag is stored in info_global[dev_type].overwrite_on_full and
 *   is read lock-free in the enqueue hot path.  Because it is a single
 *   bool written by one thread and read by another, READ_ONCE / plain
 *   assignment is sufficient on all supported architectures.
 *
 *   Backward compatibility guarantee:
 *     info_global[] is allocated with kzalloc(), so overwrite_on_full is
 *     zero-initialised (false) at module load time.  Drivers that do not
 *     call this function automatically get the original drop behaviour.
 *
 * Input:
 *   @dev_name – Null-terminated device name string matching one of the
 *               registered UIO device names ("firmware", "host", "nss").
 *   @enable   – true  to enable overwrite-on-full (opt-in).
 *               false to restore the original drop-on-full behaviour.
 *
 * Output:
 *   info_global[dev_type].overwrite_on_full is set to @enable.
 *
 * Return:
 *    0       – Success.
 *   -ENODEV  – @dev_name is NULL or not recognised.
 */
int debug_uio_set_overwrite_mode(const char *dev_name, bool enable)
{
	int dev_type;

	dev_type = debug_uio_find_dev_by_name(dev_name);
	if (dev_type < 0)
		return dev_type;

	info_global[dev_type].overwrite_on_full = enable;
	pr_info("Data ring overwrite mode %s for \"%s\"\n",
		enable ? "enabled" : "disabled", dev_name);
	return 0;
}
EXPORT_SYMBOL(debug_uio_set_overwrite_mode);

int debug_uio_register_notifier(struct notifier_block *nb,
				enum debug_uio_dev dev_type, int map_type)
{
	return blocking_notifier_chain_register(
		&notifier_chains[dev_type][map_type], nb);
}
EXPORT_SYMBOL(debug_uio_register_notifier);

/**
 * debug_uio_unregister_notifier() - Unregister a previously registered
 *                                   notifier callback.
 *
 * Description:
 *   Removes @nb from the blocking notifier chain for the (device, map)
 *   pair identified by @dev_type and @map_type.  After this call the
 *   callback will no longer be invoked when userspace sends an interrupt
 *   event for that pair.  Must be called before @nb is freed (e.g. in
 *   the driver's remove() or exit()) to prevent a use-after-free when
 *   the next ioctl event fires.
 *
 * Input:
 *   @nb       – Same notifier_block pointer passed to
 *               debug_uio_register_notifier().
 *   @dev_type – Must match the debug_uio_dev value used during registration.
 *   @map_type – Must match the debug_uio_map_type value used during
 *               registration.
 *
 * Output:
 *   @nb removed from notifier_chains[@dev_type][@map_type].
 *
 * Return:
 *    0        – Success.
 *   Negative  – Error from blocking_notifier_chain_unregister() (e.g.
 *               -ENOENT if @nb was not found on the chain).
 */
int debug_uio_unregister_notifier(struct notifier_block *nb,
			enum debug_uio_dev dev_type, int map_type)
{
	return blocking_notifier_chain_unregister(
		&notifier_chains[dev_type][map_type], nb);
}
EXPORT_SYMBOL(debug_uio_unregister_notifier);

/**
 * debug_uio_register_app_status_notifier() - Register a callback to receive
 *                                            userspace-app up/down events.
 *
 * Description:
 *   Registers @nb with the global app-status notifier chain.  Whenever a
 *   userspace app calls ioctl(IOCTL_APP_STATUS) on
 *   /dev/debug_uio_char_dev, every callback on this chain is invoked with
 *   @action set to the app's is_up flag (1 or 0) and @data pointing to a
 *   NUL-terminated app name string.  Unlike debug_uio_register_notifier(),
 *   this chain is not scoped to a UIO device or map — drivers filter by
 *   app name themselves inside their callback.
 *
 *   Per the current design there is no status replay: a driver that
 *   registers after an app has already signalled "up" will not see that
 *   past event, only future ones, matching how debug_uio_notify() and
 *   debug_uio_register_notifier() already behave.
 *
 * Input:
 *   @nb – Caller's notifier_block with .notifier_call set to the callback
 *         function:
 *         int cb(struct notifier_block *nb, unsigned long action, void *data)
 *         where @action is is_up (0/1) and @data is a const char * app
 *         name valid only for the duration of the callback — copy it if
 *         it needs to be kept.
 *
 * Output:
 *   @nb inserted into app_status_notifier_chain.
 *
 * Return:
 *    0        – Success.
 *   Negative  – Error from blocking_notifier_chain_register() (e.g.
 *               -EEXIST if @nb is already on this chain).
 */
int debug_uio_register_app_status_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&app_status_notifier_chain, nb);
}
EXPORT_SYMBOL(debug_uio_register_app_status_notifier);

/**
 * debug_uio_unregister_app_status_notifier() - Unregister a previously
 *                                              registered app-status
 *                                              callback.
 *
 * Description:
 *   Removes @nb from the global app-status notifier chain.  Must be
 *   called before @nb is freed (e.g. in the driver's remove() or exit())
 *   to prevent a use-after-free when the next IOCTL_APP_STATUS event
 *   fires.
 *
 * Input:
 *   @nb – Same notifier_block pointer passed to
 *         debug_uio_register_app_status_notifier().
 *
 * Output:
 *   @nb removed from app_status_notifier_chain.
 *
 * Return:
 *    0        – Success.
 *   Negative  – Error from blocking_notifier_chain_unregister() (e.g.
 *               -ENOENT if @nb was not found on the chain).
 */
int debug_uio_unregister_app_status_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&app_status_notifier_chain, nb);
}
EXPORT_SYMBOL(debug_uio_unregister_app_status_notifier);

/* -----------------------------------------------------------------------
 * Character device – ioctl interface
 *
 * The char device /dev/debug_uio_char_dev provides a single ioctl
 * (IOCTL_SEND_INTERRUPT) that lets userspace inject an interrupt event
 * into the kernel notifier chain for a specific (device, map) pair.
 * This is the reverse direction of debug_uio_notify(): userspace → kernel.
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_ioctl() - Handle ioctl commands from userspace on
 *                     /dev/debug_uio_char_dev.
 *
 * Description:
 *   Handles two commands:
 *
 *   IOCTL_SEND_INTERRUPT allows userspace to inject an interrupt event
 *   into the kernel.  The function copies a debug_uio_intr_data struct
 *   from userspace, validates the uioId and mapId fields, and fires the
 *   blocking notifier chain for the corresponding (device, map) pair.
 *   All registered kernel-driver callbacks on that chain are invoked
 *   synchronously with the 4-byte raw payload as the data argument.
 *
 *   IOCTL_APP_STATUS allows a userspace app to announce that it is up or
 *   down.  The function copies a debug_uio_app_status_data struct from
 *   userspace and fires the global app_status_notifier_chain — not scoped
 *   to any device/map — with the is_up flag as the notifier action and
 *   the NUL-terminated app name as the data argument.
 *
 * Input:
 *   @file – File descriptor of the open /dev/debug_uio_char_dev node.
 *   @cmd  – Ioctl command code.  IOCTL_SEND_INTERRUPT and IOCTL_APP_STATUS
 *           are handled; all other values return -ENOTTY.
 *   @arg  – For IOCTL_SEND_INTERRUPT: userspace pointer to a
 *           struct debug_uio_intr_data containing:
 *             .uioId   – Target device index (0–2).
 *             .mapId   – Target map index (0–1).
 *             .payload – 4-byte raw interrupt payload passed to callbacks.
 *           For IOCTL_APP_STATUS: userspace pointer to a
 *           struct debug_uio_app_status_data containing:
 *             .app_name – Name of the announcing app (need not be
 *                         NUL-terminated by the caller; the kernel forces
 *                         NUL-termination before use).
 *             .is_up    – 1 if the app is up, 0 if it is going down.
 *
 * Output:
 *   For IOCTL_SEND_INTERRUPT: all notifier callbacks registered on
 *   notifier_chains[data.uioId][data.mapId] are invoked with
 *   data.payload.raw as the data argument.
 *   For IOCTL_APP_STATUS: all notifier callbacks registered on
 *   app_status_notifier_chain are invoked with status.app_name as the
 *   data argument and status.is_up as the action.
 *
 * Return:
 *    0       – Success; notifier chain fired.
 *   -EFAULT  – copy_from_user() failed (bad userspace pointer).
 *   -EINVAL  – data.uioId is out of range (>= DEBUG_UIO_DEV_MAX).
 *   -ENOTTY  – data.mapId refers to an unallocated map, or @cmd is
 *              not a recognised command.
 */
static long debug_uio_ioctl(struct file *file, unsigned int cmd,
			unsigned long arg)
{
	struct debug_uio_intr_data data;
	struct uio_info *info;

	switch (cmd) {
	case IOCTL_SEND_INTERRUPT:
		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			pr_info("copy_from_user failed.\n");
			return -EFAULT;
		}

		pr_info("IOCTL_SEND_INTERRUPT: uioId=%d mapId=%d\n",
			data.uioId, data.mapId);

		if (data.uioId >= DEBUG_UIO_DEV_MAX) {
			pr_info("Invalid uioId %d\n", data.uioId);
			return -EINVAL;
		}

		info = info_global[data.uioId].info;

		if (!info->mem[data.mapId].name) {
			pr_info("Map %d not allocated for device %d\n",
				data.mapId, data.uioId);
			return -ENOTTY;
		}

		blocking_notifier_call_chain(
			&notifier_chains[data.uioId][data.mapId],
			0, (void *)(uintptr_t)data.payload.raw);
		break;

	case IOCTL_APP_STATUS: {
		struct debug_uio_app_status_data status;

		if (copy_from_user(&status, (void __user *)arg, sizeof(status))) {
			pr_info("copy_from_user failed for IOCTL_APP_STATUS.\n");
			return -EFAULT;
		}

		/* Defensive: userspace input may not be NUL-terminated. */
		status.app_name[DEBUG_UIO_APP_NAME_MAX - 1] = '\0';

		pr_info("IOCTL_APP_STATUS: app=\"%s\" is_up=%u\n",
			status.app_name, status.is_up);

		blocking_notifier_call_chain(&app_status_notifier_chain,
					      status.is_up, status.app_name);
		break;
	}

	default:
		pr_info("Unknown ioctl command 0x%x\n", cmd);
		return -ENOTTY;
	}

	return 0;
}

static const struct file_operations fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = debug_uio_ioctl,
};

/* -----------------------------------------------------------------------
 * Device Tree parsing
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_parse_dt_memory() - Parse device tree to get reserved memory
 *                                for a specific UIO device.
 *
 * Description:
 *   Attempts to locate and map a device tree reserved memory region for
 *   the specified device.  The function performs the following steps:
 *     1. Searches for a device tree node matching the device-specific
 *        compatible string (e.g., "qcom,debug-uio-firmware").
 *     2. Looks up the associated reserved memory region.
 *     3. Validates that the region is large enough (>= 2 * PAGE_SIZE).
 *     4. Maps the physical memory to a kernel virtual address via ioremap.
 *     5. Stores the node pointer, reserved memory info, and virtual address
 *        in dt_mem[dev_type] for later use.
 *
 *   If any step fails, the function logs an informational message and
 *   returns an error code, allowing the caller to fall back to kzalloc.
 *
 * Input:
 *   @dev_type – Target UIO device (DEBUG_UIO_DEV_FIRMWARE /
 *               DEBUG_UIO_DEV_HOST / DEBUG_UIO_DEV_NSS).
 *
 * Output:
 *   On success: dt_mem[dev_type] is populated with device tree node,
 *   reserved memory info, virtual base address, and is_dt_memory flag
 *   is set to true.
 *
 * Return:
 *    0       – Success; device tree memory found and mapped.
 *   -ENODEV  – No device tree node found for this device.
 *   -EINVAL  – Reserved memory not found or size too small.
 *   -ENOMEM  – ioremap failed.
 */
static int debug_uio_parse_dt_memory(enum debug_uio_dev dev_type)
{
	struct device_node *np;
	struct reserved_mem *rmem;
	void __iomem *base_addr;

	/* Step 1: Find device tree node by compatible string */
	np = of_find_compatible_node(NULL, NULL, debug_uio_dt_compat[dev_type]);
	if (!np) {
		pr_info("No DT node for %s, using kzalloc fallback\n",
			   debug_uio_dev_str[dev_type]);
		return -ENODEV;
	}

	/* Step 2: Get reserved memory region */
	rmem = of_reserved_mem_lookup(np);
	if (!rmem) {
		pr_info("No reserved memory for %s\n",
			   debug_uio_dev_str[dev_type]);
		of_node_put(np);
		return -EINVAL;
	}

	/* Validate size: must be at least 2 * PAGE_SIZE for 2 maps */
	if (rmem->size < 2 * PAGE_SIZE) {
		pr_info("Reserved memory too small for %s: %llu bytes (min %lu)\n",
			   debug_uio_dev_str[dev_type],
			   (unsigned long long)rmem->size,
			   2 * PAGE_SIZE);
		of_node_put(np);
		return -EINVAL;
	}

	/* Step 3: Map physical memory to virtual address using memremap
	 * instead of ioremap because we need normal memory semantics for
	 * atomic operations and memset in ring buffer initialization.
	 * MEMREMAP_WB provides write-back cacheable memory mapping.
	 */
	base_addr = memremap(rmem->base, rmem->size, MEMREMAP_WB);
	if (!base_addr) {
		pr_info("memremap failed for %s (base=0x%llx size=0x%llx)\n",
			   debug_uio_dev_str[dev_type],
			   (unsigned long long)rmem->base,
			   (unsigned long long)rmem->size);
		of_node_put(np);
		return -ENOMEM;
	}

	/* Store device tree memory information */
	dt_mem[dev_type].np = np;
	dt_mem[dev_type].rmem = rmem;
	dt_mem[dev_type].base_addr = base_addr;
	dt_mem[dev_type].is_dt_memory = true;

	pr_info("DT memory for %s: phys=0x%llx virt=%px size=0x%llx\n",
		   debug_uio_dev_str[dev_type],
		   (unsigned long long)rmem->base,
		   base_addr,
		   (unsigned long long)rmem->size);

	return 0;
}

/* -----------------------------------------------------------------------
 * Module init / exit helpers
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_cleanup_mem() - Free all kzalloc'd map buffers.
 *
 * Description:
 *   Iterates over every (device, map) slot and frees the kzalloc'd page
 *   stored in mem[][].  Device tree memory is NOT freed here (it's
 *   unmapped separately in debug_uio_cleanup_dt()).  Called both on error
 *   unwind during init and during module exit.  The device spinlock is
 *   held per device while freeing its maps to prevent races with any
 *   concurrent alloc/get calls.
 *
 * Input:
 *   None.
 *
 * Output:
 *   All mem[dev_type][map_type] entries set to NULL.
 *
 * Return:
 *   void.
 */
static void debug_uio_cleanup_mem(void)
{
	int dev_type, map_type, num_maps;
	unsigned long flags;

	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		num_maps = debug_uio_num_maps_per_device[dev_type];
		spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

		/* Only free kzalloc'd memory, not device tree memory */
		if (!dt_mem[dev_type].is_dt_memory) {
			for (map_type = 0; map_type < num_maps; map_type++) {
				if (mem[dev_type][map_type]) {
					kfree(mem[dev_type][map_type]);
					mem[dev_type][map_type] = NULL;
				}
			}
		} else {
			/* Just clear pointers for DT memory */
			for (map_type = 0; map_type < num_maps; map_type++) {
				mem[dev_type][map_type] = NULL;
			}
		}

		spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
	}
}

/**
 * debug_uio_cleanup_dt() - Clean up device tree resources.
 *
 * Description:
 *   Iterates over all devices and releases device tree resources:
 *   unmaps ioremap'd memory and releases device tree node references.
 *   Called during module exit after all other cleanup is complete.
 *
 * Input:
 *   None.
 *
 * Output:
 *   All dt_mem[] entries are cleared and resources released.
 *
 * Return:
 *   void.
 */
static void debug_uio_cleanup_dt(void)
{
	int dev_type;

	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		if (dt_mem[dev_type].is_dt_memory) {
			/* Unmap memremap'd memory */
			if (dt_mem[dev_type].base_addr) {
				memunmap(dt_mem[dev_type].base_addr);
				dt_mem[dev_type].base_addr = NULL;
			}

			/* Release device tree node reference */
			if (dt_mem[dev_type].np) {
				of_node_put(dt_mem[dev_type].np);
				dt_mem[dev_type].np = NULL;
			}

			dt_mem[dev_type].rmem = NULL;
			dt_mem[dev_type].is_dt_memory = false;

			pr_info("Cleaned up DT resources for %s\n",
				   debug_uio_dev_str[dev_type]);
		}
	}
}

/**
 * debug_uio_cleanup_uio() - Free the uio_info structs allocated during init.
 *
 * Description:
 *   Iterates over info_global[] and frees each uio_info struct that was
 *   allocated by debug_uio_init().  Called on error unwind and on exit
 *   after uio_unregister_device() has already been called for each device.
 *
 * Input:
 *   None.
 *
 * Output:
 *   All info_global[dev_type].info pointers set to NULL.
 *
 * Return:
 *   void.
 */
static void debug_uio_cleanup_uio(void)
{
	int dev_type;

	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		if (info_global[dev_type].info) {
			kfree(info_global[dev_type].info);
			info_global[dev_type].info = NULL;
		}
	}
}

/**
 * debug_uio_cleanup_dev() - Free the struct device objects allocated during
 *                           init.
 *
 * Description:
 *   Iterates over dev_global[] and frees each struct device that was
 *   allocated by debug_uio_init().  Called on error unwind and on exit
 *   after device_unregister() has already been called for each device.
 *
 * Input:
 *   None.
 *
 * Output:
 *   All dev_global[dev_type] pointers set to NULL.
 *
 * Return:
 *   void.
 */
static void debug_uio_cleanup_dev(void)
{
	int dev_type;

	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		if (dev_global[dev_type]) {
			kfree(dev_global[dev_type]);
			dev_global[dev_type] = NULL;
		}
	}
}

/**
 * debug_uio_dev_unregister() - Unregister the first @count struct devices
 *                              from the driver core.
 *
 * Description:
 *   Calls device_del() and put_device() for dev_global[0] through
 *   dev_global[count-1]. Used both in the error unwind path (where only
 *   a subset of devices may have been registered) and in debug_uio_exit()
 *   (where all devices are unregistered by passing DEBUG_UIO_DEV_MAX).
 *
 * Input:
 *   @count – Number of devices to unregister, starting from index 0.
 *            Typically dev_type (the index at which registration failed)
 *            or DEBUG_UIO_DEV_MAX (to unregister all).
 *
 * Output:
 *   The first @count entries in dev_global[] are unregistered from the
 *   driver core.
 *
 * Return:
 *   void.
 */
static void debug_uio_dev_unregister(int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (dev_global[i]) {
			device_del(dev_global[i]);
			put_device(dev_global[i]);
		}
	}
}

/**
 * debug_uio_unregister() - Unregister the first @count UIO devices from
 *                          the UIO framework.
 *
 * Description:
 *   Calls uio_unregister_device() for info_global[0].info through
 *   info_global[count-1].info.  Used both in the error unwind path and
 *   in debug_uio_exit().  Must be called before debug_uio_cleanup_uio()
 *   because uio_unregister_device() dereferences the uio_info pointer.
 *
 * Input:
 *   @count – Number of UIO devices to unregister, starting from index 0.
 *            Typically dev_type (the index at which registration failed)
 *            or DEBUG_UIO_DEV_MAX (to unregister all).
 *
 * Output:
 *   The first @count UIO devices are removed from the UIO framework and
 *   their /dev/uioN nodes are destroyed.
 *
 * Return:
 *   void.
 */
static void debug_uio_unregister(int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (info_global[i].info)
			uio_unregister_device(info_global[i].info);
	}
}

/* -----------------------------------------------------------------------
 * Module exit
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_exit() - Deinitialise the debug_uio module.
 *
 * Description:
 *   Tears down all resources created by debug_uio_init() in strict reverse
 *   order to avoid use-after-free and reference-count issues:
 *     1. Destroy the char device node and class.
 *     2. Unregister the char device major number.
 *     3. Unregister all UIO devices from the UIO framework.
 *     4. Unregister all struct devices from the driver core.
 *     5. Free the struct device objects.
 *     6. Free the uio_info structs.
 *     7. Free the kzalloc'd map memory pages.
 *     8. Clean up device tree resources (iounmap and of_node_put).
 *
 * Input:
 *   None.
 *
 * Output:
 *   All module resources released; /dev/uio{0,1,2} and
 *   /dev/debug_uio_char_dev nodes removed.
 *
 * Return:
 *   void.
 */
static void __exit debug_uio_exit(void)
{
	device_destroy(dump_class, MKDEV(dump_major, 0));
	class_destroy(dump_class);
	unregister_chrdev(dump_major, DEVICE_NAME);
	debug_uio_unregister(DEBUG_UIO_DEV_MAX);
	debug_uio_dev_unregister(DEBUG_UIO_DEV_MAX);
	debug_uio_cleanup_dev();
	debug_uio_cleanup_uio();
	debug_uio_cleanup_mem();
	debug_uio_cleanup_dt();
}

/* -----------------------------------------------------------------------
 * Module init
 * ----------------------------------------------------------------------- */

/**
 * debug_uio_init() - Initialise the debug_uio module.
 *
 * Description:
 *   Creates one UIO device per configured subsystem, each with an interrupt
 *   ring map and a data ring map, and registers a char device for the ioctl
 *   interface.  Proceeds in seven steps; any failure triggers a
 *   reverse-order unwind via goto:
 *
 *   Step 1 – Allocate one PAGE_SIZE buffer per (device, map) slot and
 *            call debug_uio_ring_buffer_init() to zero-fill the ring and
 *            reset front/rear to 0.
 *   Step 2 – Allocate a uio_info struct per device, populate its name,
 *            callbacks, and mem[] descriptors (physical address, size,
 *            memtype, name, offs=0).
 *   Step 3 – Allocate a struct device per UIO device and set its name
 *            and release callback.
 *   Step 4 – Register each struct device with the driver core via
 *            device_register().
 *   Step 5 – Register each UIO device with the UIO framework via
 *            uio_register_device(), creating /dev/uioN for each device.
 *   Step 6 – Initialise the blocking notifier chain array with
 *            BLOCKING_INIT_NOTIFIER_HEAD() for every (device, map) pair.
 *   Step 7 – Register the char device (DEVICE_NAME), create its class
 *            and device node /dev/debug_uio_char_dev for the ioctl
 *            interface.
 *
 * Input:
 *   None.
 *
 * Output:
 *   On success: one /dev/uioN node per registered device and
 *   /dev/debug_uio_char_dev are created and ready for use.
 *   info_global[], dev_global[], mem[][], and notifier_chains[][] are
 *   fully initialised.
 *
 * Return:
 *    0       – Module loaded successfully.
 *   -ENOMEM  – A kzalloc() or uio_register_device() allocation failed.
 *   Other negative errno – device_register() or register_chrdev() failed.
 */
static int __init debug_uio_init(void)
{
	int ret = 0;
	int dev_type, map_type, num_maps;
	struct uio_info *info;
	struct device *dev;
	void *memory;
	struct device *dump_dev;

	/* ------------------------------------------------------------------
	 * Step 1: Allocate map memory and initialise ring buffers.
	 *         Try device tree first, fall back to kzalloc if unavailable.
	 * ------------------------------------------------------------------ */
	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		num_maps = debug_uio_num_maps_per_device[dev_type];

		/* Try to parse device tree memory for this device */
		ret = debug_uio_parse_dt_memory(dev_type);

		for (map_type = 0; map_type < num_maps; map_type++) {
			if (ret == 0 && dt_mem[dev_type].is_dt_memory) {
				/* Use device tree memory */
				size_t map_size = dt_mem[dev_type].rmem->size / num_maps;
				size_t offset = map_type * map_size;

				/* Cast to char* for proper pointer arithmetic */
				memory = (void *)((char *)dt_mem[dev_type].base_addr + offset);
				mem[dev_type][map_type] = memory;

				pr_info("Using DT memory for %s map %d: virt=%px offset=0x%zx size=0x%zx\n",
					   debug_uio_dev_str[dev_type], map_type,
					   memory, offset, map_size);
			} else {
				/* Fall back to kzalloc */
				memory = kzalloc(PAGE_SIZE, GFP_KERNEL);
				if (!memory) {
					pr_info("Memory allocation failed for dev=%d map=%d\n",
						   dev_type, map_type);
					ret = -ENOMEM;
					goto clean_uio_map;
				}

				mem[dev_type][map_type] = memory;

				pr_info("Using kzalloc for %s map %d: virt=%px size=0x%lx\n",
					   debug_uio_dev_str[dev_type], map_type,
					   memory, PAGE_SIZE);
			}

			/*
			 * Initialise the ring buffer that occupies the start
			 * of this page.  map_type selects which ring-buffer
			 * struct layout to use.
			 */
			debug_uio_ring_buffer_init(memory, map_type);
		}

		/* Reset ret to 0 for next device */
		ret = 0;
	}

	/* ------------------------------------------------------------------
	 * Step 2: Allocate and populate uio_info structs.
	 * ------------------------------------------------------------------ */
	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
		if (!info) {
			pr_info("Failed to allocate uio_info for dev=%d\n",
				dev_type);
			ret = -ENOMEM;
			goto clean_uio_info;
		}

		info->name       = debug_uio_dev_str[dev_type];
		info->version    = "0.0.1";
		info->open       = debug_uio_open;
		info->release    = debug_uio_release;
		info->irq        = UIO_IRQ_CUSTOM;
		info->handler    = debug_uio_irq_handler;
		info->irqcontrol = debug_uio_irq_control;
		info->mmap       = debug_uio_mmap;

		num_maps = debug_uio_num_maps_per_device[dev_type];

		for (map_type = 0; map_type < num_maps; map_type++) {
			if (dt_mem[dev_type].is_dt_memory) {
				/* Use device tree physical address and size */
				size_t map_size = dt_mem[dev_type].rmem->size / num_maps;
				size_t offset = map_type * map_size;

				info->mem[map_type].addr = dt_mem[dev_type].rmem->base + offset;
				info->mem[map_type].size = map_size;
			} else {
				/* Use kzalloc'd memory physical address */
				info->mem[map_type].addr =
					(phys_addr_t)virt_to_phys(mem[dev_type][map_type]);
				info->mem[map_type].size = PAGE_SIZE;
			}

			info->mem[map_type].memtype = UIO_MEM_LOGICAL;
			info->mem[map_type].name    =
				debug_uio_map_type_str[dev_type][map_type];
			/*
			 * offs is the byte offset of the region within its
			 * physical page.  Our allocations are page-aligned
			 * so offs is always 0.
			 */
			info->mem[map_type].offs = 0;
		}

		info_global[dev_type].info = info;
		spin_lock_init(&info_global[dev_type].uio_lock);
	}

	/* ------------------------------------------------------------------
	 * Step 3: Allocate and initialize struct device objects.
	 * ------------------------------------------------------------------ */
	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		dev = kzalloc(sizeof(struct device), GFP_KERNEL);
		if (!dev) {
			ret = -ENOMEM;
			goto clean_uio_dev;
		}

		/* Initialize device structure before setting properties */
		device_initialize(dev);
		dev_set_name(dev, "%s", debug_uio_dev_str[dev_type]);
		dev->release = debug_uio_dev_release;
		dev->parent  = NULL;

		dev_global[dev_type] = dev;
	}

	/* ------------------------------------------------------------------
	 * Step 4: Add struct devices to the driver core.
	 *         Use device_add() instead of device_register() since we
	 *         already called device_initialize() in Step 3.
	 * ------------------------------------------------------------------ */
	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		dev = dev_global[dev_type];

		ret = device_add(dev);
		if (ret) {
			pr_info("device_add failed for dev=%d ret=%d\n",
				dev_type, ret);
			/* Unregister previously added devices */
			while (--dev_type >= 0) {
				device_del(dev_global[dev_type]);
			}
			goto clean_uio_dev;
		}
	}

	/* ------------------------------------------------------------------
	 * Step 5: Register UIO devices with the UIO framework.
	 * ------------------------------------------------------------------ */
	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		info = info_global[dev_type].info;
		dev  = dev_global[dev_type];

		if (uio_register_device(dev, info) < 0) {
			ret = -ENOMEM;
			debug_uio_unregister(dev_type);
			goto uio_dev_reg_failed;
		}

		pr_info("Registered UIO device: %s\n",
			debug_uio_dev_str[dev_type]);
	}

	/* ------------------------------------------------------------------
	 * Step 6: Initialise blocking notifier chains.
	 * ------------------------------------------------------------------ */
	for (dev_type = 0; dev_type < DEBUG_UIO_DEV_MAX; dev_type++) {
		for (map_type = 0; map_type < DEBUG_UIO_MAPS_PER_DEV_MAX; map_type++) {
			BLOCKING_INIT_NOTIFIER_HEAD(
				&notifier_chains[dev_type][map_type]);
			pr_info("Notifier chain [%d][%d] initialised\n",
				dev_type, map_type);
		}
	}

	BLOCKING_INIT_NOTIFIER_HEAD(&app_status_notifier_chain);
	pr_info("App status notifier chain initialised\n");

	/* ------------------------------------------------------------------
	 * Step 7: Register the char device for the ioctl interface.
	 * ------------------------------------------------------------------ */
	dump_major = register_chrdev(UNNAMED_MAJOR, DEVICE_NAME, &fops);
	if (dump_major < 0) {
		ret = dump_major;
		pr_err("register_chrdev failed: %d\n", ret);
		goto uio_info_reg_failed;
	}

	dump_class = class_create(CLASS_NAME);
	if (IS_ERR(dump_class)) {
		ret = PTR_ERR(dump_class);
		pr_err("class_create failed: %d\n", ret);
		goto class_failed;
	}

	dump_dev = device_create(dump_class, NULL, MKDEV(dump_major, 0),
				NULL, DEVICE_NAME);
	if (IS_ERR(dump_dev)) {
		ret = PTR_ERR(dump_dev);
		pr_err("device_create failed: %d\n", ret);
		goto device_failed;
	}

	pr_info("debug_uio: module loaded (%d devices, %d maps each)\n",
		DEBUG_UIO_DEV_MAX, DEBUG_UIO_MAPS_PER_DEV_MAX);
	return 0;

	/* ------------------------------------------------------------------
	 * Error unwind – reverse order of init steps.
	 * ------------------------------------------------------------------ */
device_failed:
	class_destroy(dump_class);
class_failed:
	unregister_chrdev(dump_major, DEVICE_NAME);
uio_info_reg_failed:
	debug_uio_unregister(DEBUG_UIO_DEV_MAX);
uio_dev_reg_failed:
	debug_uio_dev_unregister(DEBUG_UIO_DEV_MAX);
clean_uio_dev:
	debug_uio_cleanup_dev();
clean_uio_info:
	debug_uio_cleanup_uio();
clean_uio_map:
	debug_uio_cleanup_mem();
	return ret;
}

module_init(debug_uio_init);
module_exit(debug_uio_exit);

MODULE_DESCRIPTION("DEBUG UIO");
MODULE_LICENSE("Dual BSD/GPL");
