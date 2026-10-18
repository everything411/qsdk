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
#include "mmap_telemetry.h"

static struct class *dump_class;
static int dump_major;

static struct mmap_telemetry_uio_info info_global[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX];
static struct device *dev_global[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX];
static void *mem[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX][MAX_UIO_MAPS] = {0};

/*
 * Define the custom notifier chain
 */
static struct blocking_notifier_head notifier_chains[MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX][MAX_UIO_MAPS];

/*
 * mmap_telemetry_release
 *	This is to release the device
 */
static void mmap_telemetry_dev_release(struct device *dev)
{
	return;
}

/*
 * mmap_telemetry_uio_open
 *	This is to open the uio device
 */
static int mmap_telemetry_uio_open(struct uio_info *info, struct inode *inode)
{
	return 0;
}

/*
 * mmap_telemetry_uio_release
 *	This is to release the uio device
 */
static int mmap_telemetry_uio_release(struct uio_info *info, struct inode *inode)
{
	return 0;
}

/*
 * mmap_telemetry_uio_irq_handler
 *	This is to handle the uio irq.
 */
static irqreturn_t mmap_telemetry_uio_irq_handler(int irq, struct uio_info *info)
{
	DEBUG_INFO("irq: %d.", irq);

	return IRQ_HANDLED;
}

/*
 * mmap_telemetry_uio_irq_control
 *	This is to control the uio irq.
 */
static int mmap_telemetry_uio_irq_control(struct uio_info *info, s32 irq_on)
{
	return 0;
}

/*
 * mmap_telemetry_mmap
 * 	This is to mmap the uio memory.
 */
static int mmap_telemetry_mmap(struct uio_info *info, struct vm_area_struct *vma)
{
	u32 ret;
	u32 pfn;

	pfn = (info->mem[vma->vm_pgoff].addr) >> PAGE_SHIFT;

	ret = remap_pfn_range(vma, vma->vm_start, pfn,
			vma->vm_end - vma->vm_start, vma->vm_page_prot);
	if (ret) {
		/*
		 * Error Handle
		 */
		DEBUG_WARN("remap_pfn_range failed");
	}

	return ret;
}

/*
 * mmap_telemetry_round_up_to_page_size
 *	Round up the size to next multiple of PAGE_SIZE
 */
static size_t mmap_telemetry_round_up_to_page_size(size_t size)
{
	return (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

/*
 * mmap_telemetry_alloc_mem
 * 	Allocate the mmap memory.
 */
void *mmap_telemetry_alloc_mem(enum mmap_telemetry_uio_dev_category dev_type,
			int map_type, unsigned long map_size, bool is_coherent)
{
	void *memory;
	struct uio_info *info = info_global[dev_type].info;
	unsigned long flags;

	if (is_coherent) {
		DEBUG_WARN("Not supported. \n");
		return NULL;
	}

	map_size = mmap_telemetry_round_up_to_page_size(map_size);
	memory = kzalloc(map_size, GFP_KERNEL);
	if (!memory) {
		DEBUG_WARN("Memory allocation failed for %s telemetry. \n", mmap_telemetry_mem_type_str[dev_type][map_type]);
		return NULL;
	}

	spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

	if (info->mem[map_type].addr) {
		kfree(phys_to_virt(info->mem[map_type].addr));
	}

	info->mem[map_type].addr = (phys_addr_t)(virt_to_phys(memory));
	info->mem[map_type].size = map_size;
	info->mem[map_type].memtype = UIO_MEM_LOGICAL;
	info->mem[map_type].name = mmap_telemetry_mem_type_str[dev_type][map_type];

	mem[dev_type][map_type] = memory;

	spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
        return memory;
}
EXPORT_SYMBOL(mmap_telemetry_alloc_mem);

/*
 * mmap_telemetry_get_mem
 *	Get the mmap memory address.
 */
void *mmap_telemetry_get_mem(enum mmap_telemetry_uio_dev_category dev_type, int map_type)
{
	struct uio_info *info = info_global[dev_type].info;
	unsigned long flags;
	void *addr = NULL;

	spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

	if (info->mem[map_type].name) {
		addr = phys_to_virt(info->mem[map_type].addr);
		spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
		return addr;
	}

	spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
	return NULL;
}
EXPORT_SYMBOL(mmap_telemetry_get_mem);

/*
 * mmap_telemetry_free_mem
 *	Free the mmap memory.
 */
void mmap_telemetry_free_mem(enum mmap_telemetry_uio_dev_category dev_type, int map_type)
{
	struct uio_info *info = info_global[dev_type].info;
	unsigned long flags;

	spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

	if (info->mem[map_type].name) {
		kfree(phys_to_virt(info->mem[map_type].addr));
		info->mem[map_type].name = NULL;
		info->mem[map_type].addr = 0;
		info->mem[map_type].size = 0;
		mem[dev_type][map_type] = NULL;
	}

	spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
}
EXPORT_SYMBOL(mmap_telemetry_free_mem);

/*
 * mmap_telemetry_ring_buffer_init
 *	Initialize the ring buffer
 */
static void mmap_telemetry_ring_buffer_init(struct mmap_telemetry_ring_buffer *rb)
{
	memset(rb->buffer, 0, sizeof(rb->buffer));

	atomic_set(&rb->front, 0);
	atomic_set(&rb->rear, 0);
}

/*
 * mmap_telemetry_ring_buffer_enqueue
 *	Enqueue the ring buffer
 */
static bool __maybe_unused mmap_telemetry_ring_buffer_enqueue(struct mmap_telemetry_ring_buffer *rb,
			int dev_type, int map_type, uint32_t offset, uint32_t size)
{
	int front;
	int next_rear;
	int rear;
	struct mmap_telemetry_intr_data data;

	if (!rb) {
		DEBUG_WARN("ring buffer is NULL");
		return false;
	}

	/*
	 * Read memory barrier
	 */
	smp_rmb();
	rear = atomic_read(&rb->rear);
	front = atomic_read(&rb->front);
	next_rear = (rear + 1) % RING_SIZE;

	/*
	 * Check if queue is full
	 */
	if (next_rear == front) {
		DEBUG_WARN("ring buffer is full");
		return false;
	}

	data.uioId = dev_type;
	data.mapId = map_type;
	data.offset_from_base = offset;
	data.size_in_bytes_to_read = size;

	memcpy(&rb->buffer[rear], &data, sizeof(struct mmap_telemetry_intr_data));

	/*
	 * Ensure the data is visible before updating rear
	 */
	smp_wmb();

	/*
	 * Advance rear pointer
	 */
	atomic_set(&rb->rear, next_rear);
	smp_wmb();

	return true;
}

/*
 * mmap_telemetry_ioctl
 *	Handle ioctl messages from the userspace
 */
static long mmap_telemetry_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mmap_telemetry_intr_data data;
	struct uio_info *info;

	switch (cmd) {
	case IOCTL_SEND_INTERRUPT:
		if (copy_from_user((void *)(uintptr_t)&data, (void __user *)arg, sizeof(data))) {
			DEBUG_WARN("Data copy from user failed.");
			return -EFAULT;
		}

		DEBUG_INFO("Received ioctl for mapId: %d, uioId: %d\n", data.mapId, data.uioId);
		info = info_global[data.uioId].info;

		if (!info->mem[data.mapId].name) {
			DEBUG_WARN("Wrong Map Id.");
			return -ENOTTY;
		}

		blocking_notifier_call_chain(&notifier_chains[data.uioId][data.mapId], 0, (void *)data.payload.raw);
		break;

	default:
		DEBUG_WARN("Ioctl command not found.");
		return -ENOTTY;
	}

	return 0;
}

/*
 * mmap_telemetry_notify
 *	Notify when data is written
 */
int mmap_telemetry_notify(enum mmap_telemetry_uio_dev_category dev_type,
				int map_type, uint32_t offset, uint32_t size)
{
	struct uio_info *info;
	void *addr = NULL;

	info = info_global[0].info;
	addr = phys_to_virt(info->mem[0].addr);
	if (!mmap_telemetry_ring_buffer_enqueue((struct mmap_telemetry_ring_buffer *)addr, dev_type,map_type, offset,size)) {
		DEBUG_WARN("Ring buffer enqueue failed.");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(mmap_telemetry_notify);

/*
 * mmap_telemetry_register_notifier()
 *      Register to received mmap telemetry events.
 */
int mmap_telemetry_register_notifier(struct notifier_block *nb,
				enum mmap_telemetry_uio_dev_category dev_type, int map_type)
{
	return blocking_notifier_chain_register(&notifier_chains[dev_type][map_type], nb);
}
EXPORT_SYMBOL(mmap_telemetry_register_notifier);

/*
 * mmap_telemetry_unregister_notifier()
 *      Unregister to received mmap telemetry events.
 */
int mmap_telemetry_unregister_notifier(struct notifier_block *nb,
			enum mmap_telemetry_uio_dev_category dev_type, int map_type)
{
	return blocking_notifier_chain_unregister(&notifier_chains[dev_type][map_type], nb);
}
EXPORT_SYMBOL(mmap_telemetry_unregister_notifier);

/*
 * mmap_telemetry_cleanup_mem
 *	Clean up the uio map memory
 */
static void mmap_telemetry_cleanup_mem(void)
{
	int dev_type, map_type, num_maps;
	unsigned long flags;

	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		num_maps = mmap_telemetry_num_maps_per_category[dev_type];
		spin_lock_irqsave(&info_global[dev_type].uio_lock, flags);

		for (map_type = 0; map_type < num_maps; map_type++) {
			if(mem[dev_type][map_type]) {
				kfree(mem[dev_type][map_type]);
				mem[dev_type][map_type] = NULL;
			}
		}

		spin_unlock_irqrestore(&info_global[dev_type].uio_lock, flags);
	}
}

/*
 * mmap_telemetry_cleanup_uio
 *	Clean up the uio info structures
 */
static void mmap_telemetry_cleanup_uio(void)
{
	int dev_type;
	struct uio_info *info;

	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		info = info_global[dev_type].info;

		if (info) {
			kfree(info);
		}
	}
}

/*
 * mmap_telemetry_cleanup_dev
 *	Clean up the device structures
 */
static void mmap_telemetry_cleanup_dev(void)
{
	int dev_type;
	struct device *dev;

	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		dev = dev_global[dev_type];

		if (dev) {
			kfree(dev);
		}
	}
}

/*
 * mmap_telemetry_dev_unregister
 *	Unregister the device structures
 */
static void mmap_telemetry_dev_unregister(enum mmap_telemetry_uio_dev_category dev_type)
{
	int i;
	struct device *dev;

	for (i = 0; i < dev_type; i++) {
		dev = dev_global[i];

		if (dev) {
			device_unregister(dev);
		}
	}
}

/*
 * mmap_telemetry_uio_unregister
 *	Unregister the uio info structures
 */
static void mmap_telemetry_uio_unregister(enum mmap_telemetry_uio_dev_category dev_type)
{
	int i;
	struct uio_info *info;

	for (i = 0; i < dev_type; i++) {
		info = info_global[i].info;

		if (info) {
			uio_unregister_device(info);
		}
	}
}

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = mmap_telemetry_ioctl
};

/*
 * mmap_telemetry_exit
 *	Deinitialize the mmap telemetry module.
 */
static void __exit mmap_telemetry_exit(void)
{
	device_destroy(dump_class, MKDEV(dump_major, 0));
	class_destroy(dump_class);
	unregister_chrdev(dump_major, DEVICE_NAME);
	mmap_telemetry_uio_unregister(MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX);
	mmap_telemetry_dev_unregister(MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX);
	mmap_telemetry_cleanup_dev();
	mmap_telemetry_cleanup_uio();
	mmap_telemetry_cleanup_mem();
}

/*
 * mmap_telemetry_init
 * 	Initialize the mmap telemetry module.
 */
static int __init mmap_telemetry_init(void)
{
	int ret = 0;
	int dev_type, map_type;
	int num_maps;
	struct uio_info *info;
	struct device *dev;
	void *memory;
	struct device *dump_dev;

	/*
	 * Allocate the uio maps.
	 */
	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		num_maps = mmap_telemetry_num_maps_per_category[dev_type];

		for (map_type = 0; map_type < num_maps; map_type++) {
			memory = kzalloc(PAGE_SIZE, GFP_KERNEL);
			if (!memory) {
				DEBUG_WARN("Memory allocation failed for telemetry. \n");
				ret = -ENOMEM;
				goto clean_uio_map;
			}

			mem[dev_type][map_type] = memory;
		}
	}

	/*
	 * Allocate the uio info structures
	 */
	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		info = kzalloc(sizeof(struct uio_info), GFP_KERNEL);
		if (!info) {
			DEBUG_WARN("Failed to allocate meory for uio info.\n");
			ret = -ENOMEM;
			goto clean_uio_info;
		}

		info->name = mmap_telemetry_uio_dev_category_str[dev_type];
		info->version = "0.0.1";
		info->open = mmap_telemetry_uio_open;
		info->release = mmap_telemetry_uio_release;
		info->irq = UIO_IRQ_CUSTOM;
		info->handler = mmap_telemetry_uio_irq_handler;
		info->irqcontrol = mmap_telemetry_uio_irq_control;
		info->mmap = mmap_telemetry_mmap;

		num_maps = mmap_telemetry_num_maps_per_category[dev_type];

		for (map_type = 0; map_type < num_maps; map_type++) {
			info->mem[map_type].addr = (phys_addr_t)(virt_to_phys(mem[dev_type][map_type]));
			info->mem[map_type].size = PAGE_SIZE;
			info->mem[map_type].memtype = UIO_MEM_LOGICAL;
			info->mem[map_type].name = mmap_telemetry_mem_type_str[dev_type][map_type];
			info->mem[map_type].offs = map_type * PAGE_SIZE;
		}

		info_global[dev_type].info = info;
		spin_lock_init(&info_global[dev_type].uio_lock);
	}

	/*
	 * Allocate the uio devices.
	 */
	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		dev = kzalloc(sizeof(struct device), GFP_KERNEL);
		if (!dev) {
			ret = -ENOMEM;
			goto clean_uio_dev;
		}

		dev_set_name(dev, mmap_telemetry_uio_dev_category_str[dev_type]);
		dev->release = mmap_telemetry_dev_release;
		dev->parent = NULL;

		dev_global[dev_type] = dev;
	}

	/*
	 * Register the devices.
	 */
	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		dev = dev_global[dev_type];

		ret = device_register(dev);
		if (ret) {
			DEBUG_WARN("Failed to register the telemetry uio device.\n");
			mmap_telemetry_dev_unregister(dev_type);
			goto clean_uio_dev;
		}
	}

	/*
	 * Register the uio devices.
	 */
	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		info = info_global[dev_type].info;
		dev = dev_global[dev_type];

		if (uio_register_device(dev, info) < 0) {
			ret = -ENOMEM;
			mmap_telemetry_uio_unregister(dev_type);
			goto uio_dev_reg_failed;
		}

		DEBUG_INFO("Registered UIO handler %s\n", mmap_telemetry_uio_dev_category_str[dev_type]);
	}

	/*
	 * Initialize the notifier chains
	 */
	for (dev_type = 0; dev_type < MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX; dev_type++) {
		for (map_type = 0; map_type < MAX_UIO_MAPS; map_type++) {
			BLOCKING_INIT_NOTIFIER_HEAD(&notifier_chains[dev_type][map_type]);
			DEBUG_INFO("Notifier chain [%d][%d] initialized\n", dev_type, map_type);
		}
	}

	/*
	 * Register char device
	 */
	dump_major = register_chrdev(UNNAMED_MAJOR, DEVICE_NAME, &fops);
	if (dump_major < 0) {
		ret = dump_major;
		pr_err("Unable to allocate a major number err = %d\n", ret);
		goto uio_info_reg_failed;
	}

	dump_class = class_create(CLASS_NAME);

	if (IS_ERR(dump_class)) {
		ret = PTR_ERR(dump_class);
		pr_err("Unable to create dump class = %d\n", ret);
		goto class_failed;
	}

	/*
	 * Create char device
	 */
	dump_dev = device_create(dump_class, NULL, MKDEV(dump_major, 0), NULL, DEVICE_NAME);
	if (IS_ERR(dump_dev)) {
		ret = PTR_ERR(dump_dev);
		pr_err("Unable to create a device err = %d\n", ret);
		goto device_failed;
	}

	/*
	 * Resize mmap for sxc interrupt handling
	 */
	if (!mmap_telemetry_alloc_mem(MMAP_TELEMETRY_UIO_DEV_CATEGORY_SXC, MMAP_TELEMETRY_MEM_TYPE_INTERRUPT, INTERRUPT_MAP_SIZE, false)) {
		pr_err("Unable to allocate map for sxc interrupt\n");
		ret = -ENOMEM;
		device_destroy(dump_class, MKDEV(dump_major, 0));
		goto device_failed;
	}

	/*
	 * Allocate mmap for all wlfw devices
	 */
	for (map_type = MMAP_TELEMETRY_MEM_TYPE_WLFW0; map_type < MMAP_TELEMETRY_MEM_TYPE_WLFW_MAX; map_type++) {
		if (!mmap_telemetry_alloc_mem(MMAP_TELEMETRY_UIO_DEV_CATEGORY_WLFW, map_type, WLFW_MAP_SIZE, false)) {
			pr_err("Unable to allocate map for wlfw%d interrupt\n", map_type - MMAP_TELEMETRY_MEM_TYPE_WLFW0);
			ret = -ENOMEM;
			device_destroy(dump_class, MKDEV(dump_major, 0));
			goto device_failed;
		}
	}

	info = info_global[0].info;
	memory = phys_to_virt(info->mem[0].addr);

	/*
	 * Initialize interrupt ring buffer
	 */
	mmap_telemetry_ring_buffer_init((struct mmap_telemetry_ring_buffer *)memory);

	return 0;

device_failed:
	class_destroy(dump_class);
class_failed:
	unregister_chrdev(dump_major, DEVICE_NAME);
uio_info_reg_failed:
	mmap_telemetry_uio_unregister(MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX);
uio_dev_reg_failed:
	mmap_telemetry_dev_unregister(MMAP_TELEMETRY_UIO_DEV_CATEGORY_MAX);
clean_uio_dev:
	mmap_telemetry_cleanup_dev();
clean_uio_info:
	mmap_telemetry_cleanup_uio();
clean_uio_map:
	mmap_telemetry_cleanup_mem();
	return ret;
}

module_init(mmap_telemetry_init);
module_exit(mmap_telemetry_exit);

MODULE_DESCRIPTION("MMAP TELEMETRY");
MODULE_LICENSE("Dual BSD/GPL");
