/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "ppe_dscp_dump.h"

/*
 * ppe_dscp_dump_write()
 *	Write out to the message buffer.
 */
int ppe_dscp_dump_write(struct ppe_dscp_dump_instance *ddi, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_DSCP_DUMP_BUFFER_SIZE - ddi->msg_len;
	ptr = ddi->msg + ddi->msg_len;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);

	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	ddi->msg_len += result;
	return 0;
}

/*
 * ppe_dscp_dump_one()
 *	Dump a single DSCP mapping entry.
 */
static int ppe_dscp_dump_one(struct ppe_dscp_dump_instance *ddi, struct ppe_dscp_db_entry *entry)
{
	int result;

	if ((result = ppe_dscp_dump_write(ddi, "TOS: %d\n", entry->tos))) {
		return result;
	}

	if ((result = ppe_dscp_dump_write(ddi, "Upstream:   PCP0: %d, PCP1: %d\n",
					entry->pcp0_upstream, entry->pcp1_upstream))) {
		return result;
	}

	if ((result = ppe_dscp_dump_write(ddi, "Downstream: PCP0: %d, PCP1: %d\n",
					entry->pcp0_downstream, entry->pcp1_downstream))) {
		return result;
	}

	return 0;
}

/*
 * ppe_dscp_dump_all()
 *	Prepare dscp dump information for all entries.
 */
static bool ppe_dscp_dump_all(struct ppe_dscp_dump_instance *ddi)
{
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;
	struct ppe_dscp_db_entry *entry;
	int result;

	ddi->msgp = ddi->msg;
	ddi->msg_len = 0;

	if ((result = ppe_dscp_dump_write(ddi, "DSCP P-bit Mapping Table\n"))) {
		return result;
	}

	spin_lock_bh(&dscp_g->lock);
	list_for_each_entry(entry, &dscp_g->dscp_map, list) {
		if (!entry->valid) {
			continue;
		}

		result = ppe_dscp_dump_one(ddi, entry);
		if (result < 0) {
			ppe_dscp_warn("%p: failed to write dump for tos: %d", dscp_g, entry->tos);
			spin_unlock_bh(&dscp_g->lock);
			return result;
		}
	}
	spin_unlock_bh(&dscp_g->lock);

	return 0;
}

/*
 * ppe_dscp_dump_dev_open()
 *	Open the character device file for DSCP dump.
 */
static int ppe_dscp_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_dscp_dump_instance *ddi;
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;

	/*
	 * Allocate state information for the reading
	 */
	ppe_dscp_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	ddi = (struct ppe_dscp_dump_instance *)kzalloc(sizeof(struct ppe_dscp_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!ddi) {
		ppe_dscp_warn("%p: unable to allocate memory for dump instance", dscp_g);
		return -ENOMEM;
	}

	ddi->dump_en = true;
	file->private_data = ddi;

	return 0;
}

/*
 * ppe_dscp_dump_dev_release()
 *	Close the character device file for DSCP dump.
 */
static int ppe_dscp_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_dscp_dump_instance *ddi = (struct ppe_dscp_dump_instance *)file->private_data;
	if (ddi) {
		kfree(ddi);
	}

	return 0;
}

/*
 * ppe_dscp_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_dscp_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_dscp_dump_instance *ddi;
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;

	ddi = (struct ppe_dscp_dump_instance *)file->private_data;
	if (!ddi) {
		ppe_dscp_warn("%p: unable to find dump instance", dscp_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (ddi->msg_len) {
		goto read_output;
	}

	if (ddi->dump_en) {
		if (ppe_dscp_dump_all(ddi)) {
			ppe_dscp_warn("Failed to create dscp dump\n");
			return -EIO;
		}

		ddi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = ddi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, ddi->msgp, bytes_read)) {
		return -EIO;
	}

	ddi->msg_len -= bytes_read;
	ddi->msgp += bytes_read;

	ppe_dscp_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			ddi, bytes_read, ddi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_dscp_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_dscp_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with dscp dump character device
 */
static struct file_operations ppe_dscp_dump_fops = {
	.read = ppe_dscp_dump_dev_read,
	.write = ppe_dscp_dump_dev_write,
	.open = ppe_dscp_dump_dev_open,
	.release = ppe_dscp_dump_dev_release
};

/*
 * ppe_dscp_dump_deinit()
 *	Unregister character device for DSCP dump
 */
void ppe_dscp_dump_deinit(void)
{
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;

	if (dscp_g->dentry) {
		debugfs_remove(dscp_g->dentry);
		dscp_g->dentry = NULL;
	}

	unregister_chrdev(dscp_g->dscp_dump_major_id, "ppe_dscp_dump_dev");
}

/*
 * ppe_dscp_dump_init()
 *	Register a character device for DSCP dump
 */
bool ppe_dscp_dump_init(struct dentry *d_rule)
{
	struct ppe_dscp_base *dscp_g = &ppe_dscp_gbl;
	int dev_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	dscp_g->dentry = debugfs_create_u32("ppe_dscp_dump", S_IRUGO, d_rule, (u32 *)&dscp_g->dscp_dump_major_id);
	if (!dscp_g->dentry) {
		ppe_dscp_warn("%p: Failed to create ppe state dev major file in debugfs\n", dscp_g);
		return false;
	}
#else
	debugfs_create_u32("ppe_dscp_dump", S_IRUGO, d_rule, (u32 *)&dscp_g->dscp_dump_major_id);
#endif

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_dscp_dump_dev", &ppe_dscp_dump_fops);
	if (dev_id < 0) {
		ppe_dscp_warn("%p: Failed to register chrdev %d\n", dscp_g, dev_id);
		return false;
	}

	dscp_g->dscp_dump_major_id = dev_id;
	ppe_dscp_trace("%p: dscp dump dev major id %d\n", dscp_g, dscp_g->dscp_dump_major_id);

	return true;
}
