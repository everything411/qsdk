/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <ppe_drv_iface.h>
#include "nss_ppe_lag_private.h"

/*
 * Character device stuff - used to communicate status back to user space
 */
static int nss_ppe_lag_dump_major_id = 0;	/* Major ID of registered char dev from which we can dump out state to userspace */

/*
 * nss_ppe_lag_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 *
 * Returns 0 on success
 */
static int nss_ppe_lag_dump_write_reset(struct nss_ppe_lag_dump_instance *ldi, char *prefix)
{
	int result;

	ldi->msgp = ldi->msg;
	ldi->msg_len = 0;

	result = snprintf(ldi->prefix, NSS_PPE_LAG_DUMP_PREFIX_SIZE, "%s", prefix);
	if ((result < 0) || (result >= NSS_PPE_LAG_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	ldi->prefix_level = 0;
	ldi->prefix_levels[ldi->prefix_level] = result;

	return 0;
}

/*
 * nss_ppe_lag_dump_prefix_add()
 *	Add another level to the prefix
 */
static int nss_ppe_lag_dump_prefix_add(struct nss_ppe_lag_dump_instance *ldi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = ldi->prefix_levels[ldi->prefix_level];
	pxremain = NSS_PPE_LAG_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(ldi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	ldi->prefix_level++;
	ldi->prefix_levels[ldi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * nss_ppe_lag_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
static int nss_ppe_lag_dump_prefix_index_add(struct nss_ppe_lag_dump_instance *ldi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = ldi->prefix_levels[ldi->prefix_level];
	pxremain = NSS_PPE_LAG_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(ldi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	ldi->prefix_level++;
	ldi->prefix_levels[ldi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * nss_ppe_lag_dump_prefix_remove()
 *	Remove level from the prefix
 */
static int nss_ppe_lag_dump_prefix_remove(struct nss_ppe_lag_dump_instance *ldi)
{
	int pxsz;

	ldi->prefix_level--;
	pxsz = ldi->prefix_levels[ldi->prefix_level];
	ldi->prefix[pxsz] = 0;

	return 0;
}

/*
 * nss_ppe_lag_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
static int nss_ppe_lag_dump_write(struct nss_ppe_lag_dump_instance *ldi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = NSS_PPE_LAG_DUMP_BUFFER_SIZE - ldi->msg_len;
	ptr = ldi->msg + ldi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", ldi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	ldi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	ldi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	ldi->msg_len += result;
	return 0;
}

/*
 * nss_ppe_lag_dump_one()
 *	Prepare lag dump information for a device.
 */
static bool nss_ppe_lag_dump_one(struct nss_ppe_lag_dump_instance *ldi, struct nss_ppe_lag_bond_entry *entry)
{
	struct ppe_drv_iface *iface = entry->iface;
	uint16_t index = ppe_drv_iface_get_index(iface);
	int status;
	int i;

	/*
	 * Current LAG count.
	 */
	if ((status = nss_ppe_lag_dump_prefix_index_add(ldi, ldi->bond_cnt))) {
		goto error;
	}

	/*
	 * Device information
	 */
	if ((status = nss_ppe_lag_dump_prefix_add(ldi, "bond"))) {
		goto error;
	}

	if ((status = nss_ppe_lag_dump_write(ldi, "bond_id", "%d", entry->bond_id))) {
		goto error;
	}

	if ((status = nss_ppe_lag_dump_write(ldi, "bond_dev", "%s", entry->bond_dev->name))) {
		goto error;
	}

	if ((status = nss_ppe_lag_dump_write(ldi, "in_use", "%d", entry->in_use))) {
		goto error;
	}

	ldi->slave_cnt = 0;

	for (i = 0; i < NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID; i++) {
		if (entry->slaves[i] == NULL) {
			break;
		}

		if ((status = nss_ppe_lag_dump_prefix_index_add(ldi, ldi->slave_cnt))) {
			goto error;
		}

		if ((status = nss_ppe_lag_dump_write(ldi, "slave", "%s", entry->slaves[i]->name))) {
			goto error;
		}

		if ((status = nss_ppe_lag_dump_prefix_remove(ldi))) {
			goto error;
		}

		ldi->slave_cnt++;
	}

	if ((status = nss_ppe_lag_dump_write(ldi, "iface_index", "%d", index))) {
		goto error;
	}

	if ((status = nss_ppe_lag_dump_write(ldi, "dev_addr", "%pM", entry->dev_addr))) {
		goto error;
	}

	/*
	 * Remove the 'bond' prefix for next interation
	 */
	if ((status = nss_ppe_lag_dump_prefix_remove(ldi))) {
		goto error;
	}

	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((status = nss_ppe_lag_dump_prefix_remove(ldi))) {
		goto error;
	}

error:
	return status;
}

/*
 * nss_ppe_lag_dump_all()
 *	Prepare lag dump information for all the devices.
 */
static bool nss_ppe_lag_dump_all(struct nss_ppe_lag_dump_instance *ldi)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	struct nss_ppe_lag_bond_entry *entry = ctx->entry;
	int status;
	int i;

	if ((status = nss_ppe_lag_dump_write_reset(ldi, "Interface"))) {
		return status;
	}

	/*
	 * Get the information of all LAG devices.
	 */
	ldi->bond_cnt = 0;
	for (i = 0; i < NSS_PPE_LAG_MAX_BOND_DEVICES; i++) {
		if (entry[i].in_use) {
			status = nss_ppe_lag_dump_one(ldi, &entry[i]);
			if (status < 0) {
				nss_ppe_lag_warn("failed to collect dump for bond device");
				return status;
			}
			ldi->bond_cnt++;
		}
	}

	return 0;
}

/*
 * nss_ppe_lag_dump_dev_open()
 *	Opens the special char device file which we use to dump our state.
 */
static int nss_ppe_lag_dump_dev_open(struct inode *inode, struct file *file)
{
	struct nss_ppe_lag_dump_instance *ldi;

	nss_ppe_lag_info("LAG dump open\n");

	/*
	 * Allocate state information for reading
	 */
	nss_ppe_lag_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	ldi = (struct nss_ppe_lag_dump_instance *)kzalloc(sizeof(struct nss_ppe_lag_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!ldi) {
		return -ENOMEM;
	}

	ldi->dump_en = 1;
	file->private_data = ldi;

	return 0;
}

/*
 * nss_ppe_lag_dump_dev_release()
 *	Close the character device file for LAG dump.
 */
static int nss_ppe_lag_dump_dev_release(struct inode *inode, struct file *file)
{
	struct nss_ppe_lag_dump_instance *ldi = (struct nss_ppe_lag_dump_instance *)file->private_data;

	if (ldi) {
		kfree(ldi);
	}

	return 0;
}

/*
 * nss_ppe_lag_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t nss_ppe_lag_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct nss_ppe_lag_dump_instance *ldi;

	ldi = (struct nss_ppe_lag_dump_instance *)file->private_data;
	if (!ldi) {
		nss_ppe_lag_warn("unable to find dump instance");
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (ldi->msg_len) {
		goto read_output;
	}

	if (ldi->dump_en) {
		if (nss_ppe_lag_dump_all(ldi)) {
			nss_ppe_lag_warn("Failed to create LAG dump\n");
			return -EIO;
		}

		ldi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = ldi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, ldi->msgp, bytes_read)) {
		return -EIO;
	}

	ldi->msg_len -= bytes_read;
	ldi->msgp += bytes_read;

	nss_ppe_lag_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			ldi, bytes_read, ldi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * nss_ppe_lag_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t nss_ppe_lag_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	return -EINVAL;
}

/*
 * File operations associated with lag dump character device
 */
static struct file_operations nss_ppe_lag_dump_fops = {
	.read = nss_ppe_lag_dump_dev_read,
	.write = nss_ppe_lag_dump_dev_write,
	.open = nss_ppe_lag_dump_dev_open,
	.release = nss_ppe_lag_dump_dev_release
};

/*
 * nss_ppe_lag_dump_exit()
 *	Unregister character device for LAG dump
 */
void nss_ppe_lag_dump_exit(void)
{
	unregister_chrdev(nss_ppe_lag_dump_major_id, "ppe_lag_dump");
}

/*
 * nss_ppe_lag_dump_init()
 */
int nss_ppe_lag_dump_init(struct nss_ppe_lag_ctx *ctx)
{
	int dev_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!debugfs_create_u32("ppe_lag_dump", S_IRUGO, ctx->dentry,
				(u32 *)&nss_ppe_lag_dump_major_id)) {
		nss_ppe_lag_warn("Failed to create lag dump file in debugfs\n");
		goto init_cleanup;
	}
#else
	debugfs_create_u32("ppe_lag_dump", S_IRUGO, ctx->dentry,
				(u32 *)&nss_ppe_lag_dump_major_id);
#endif

	/*
	 * Register a char device that we will use to provide a dump of our state
	 */
	dev_id = register_chrdev(0, "ppe_lag_dump_dev", &nss_ppe_lag_dump_fops);
	if (dev_id < 0) {
		nss_ppe_lag_warn("Failed to register chrdev %d\n", dev_id);
		goto init_cleanup;
	}

	nss_ppe_lag_dump_major_id = dev_id;
	nss_ppe_lag_trace("registered chr dev major id assigned %d\n", nss_ppe_lag_dump_major_id);

	return 0;

init_cleanup:
	return dev_id;
}
