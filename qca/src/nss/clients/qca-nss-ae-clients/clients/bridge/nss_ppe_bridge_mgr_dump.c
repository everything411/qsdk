/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/version.h>
#include <linux/inetdevice.h>
#include <linux/netdevice.h>
#include <ppe_drv_iface.h>
#include "nss_ppe_bridge_mgr.h"

/*
 * Character device stuff - used to communicate status back to user space
 */
static int nss_ppe_bridge_mgr_dump_major_id = 0;	/* Major ID of registered char dev from which we can dump out state to userspace */

/*
 * nss_ppe_bridge_mgr_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 *
 * Returns 0 on success
 */
static int nss_ppe_bridge_mgr_dump_write_reset(struct nss_ppe_bridge_mgr_dump_instance *bdi, char *prefix)
{
	int result;

	bdi->msgp = bdi->msg;
	bdi->msg_len = 0;

	result = snprintf(bdi->prefix, NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_SIZE, "%s", prefix);
	if ((result < 0) || (result >= NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	bdi->prefix_level = 0;
	bdi->prefix_levels[bdi->prefix_level] = result;

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_prefix_add()
 *	Add another level to the prefix
 */
static int nss_ppe_bridge_mgr_dump_prefix_add(struct nss_ppe_bridge_mgr_dump_instance *bdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = bdi->prefix_levels[bdi->prefix_level];
	pxremain = NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(bdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	bdi->prefix_level++;
	bdi->prefix_levels[bdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
static int nss_ppe_bridge_mgr_dump_prefix_index_add(struct nss_ppe_bridge_mgr_dump_instance *bdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = bdi->prefix_levels[bdi->prefix_level];
	pxremain = NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(bdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	bdi->prefix_level++;
	bdi->prefix_levels[bdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_prefix_remove()
 *	Remove level from the prefix
 */
static int nss_ppe_bridge_mgr_dump_prefix_remove(struct nss_ppe_bridge_mgr_dump_instance *bdi)
{
	int pxsz;

	bdi->prefix_level--;
	pxsz = bdi->prefix_levels[bdi->prefix_level];
	bdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
static int nss_ppe_bridge_mgr_dump_write(struct nss_ppe_bridge_mgr_dump_instance *bdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = NSS_PPE_BRIDGE_MGR_DUMP_BUFFER_SIZE - bdi->msg_len;
	ptr = bdi->msg + bdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", bdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	bdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	bdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	bdi->msg_len += result;
	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_one()
 *	Prepare bridge_mgr dump information for a device.
 */
static bool nss_ppe_bridge_mgr_dump_one(struct nss_ppe_bridge_mgr_dump_instance *bdi, struct nss_ppe_bridge_mgr_pvt *b_pvt)
{
	struct ppe_drv_iface *iface = b_pvt->iface;
	uint16_t index = ppe_drv_iface_get_index(iface);
	int status;

	/*
	 * Current Bridge manager count.
	 */
	if ((status = nss_ppe_bridge_mgr_dump_prefix_index_add(bdi, bdi->br_cnt))) {
		goto error;
	}

	/*
	 * Device information
	 */
	if ((status = nss_ppe_bridge_mgr_dump_prefix_add(bdi, "bridge"))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "dev", "%s", b_pvt->dev->name))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "iface_index", "%d", index))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "bond_slave_num", "%d", b_pvt->bond_slave_num))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "wan_if_enabled", "%d", b_pvt->wan_if_enabled))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "fdb_lrn_enabled", "%d", b_pvt->fdb_lrn_enabled))) {
		goto error;
	}

	if (b_pvt->wan_netdev) {
		if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "wan_netdev", "%s", b_pvt->wan_netdev->name))) {
			goto error;
		}
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "mtu", "%d", b_pvt->mtu))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "dev_addr", "%pM", b_pvt->dev_addr))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "bridge_vlan_iface_cnt", "%d", b_pvt->bridge_vlan_iface_cnt))) {
		goto error;
	}

	if ((status = nss_ppe_bridge_mgr_dump_write(bdi, "br_flood_en", "%d", b_pvt->br_flood_en))) {
                goto error;
        }

	/*
	 * Remove the 'bridge' prefix for next interation
	 */
	if ((status = nss_ppe_bridge_mgr_dump_prefix_remove(bdi))) {
		goto error;
	}

	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((status = nss_ppe_bridge_mgr_dump_prefix_remove(bdi))) {
		goto error;
	}

error:
	return status;
}

/*
 * nss_ppe_bridge_mgr_dump_all()
 *	Prepare bridge_mgr dump information for all the devices.
 */
static bool nss_ppe_bridge_mgr_dump_all(struct nss_ppe_bridge_mgr_dump_instance *bdi)
{
	struct nss_ppe_bridge_mgr_context *ctx = &br_mgr_ctx;
	struct nss_ppe_bridge_mgr_pvt *b_pvt;
	int status;

	if ((status = nss_ppe_bridge_mgr_dump_write_reset(bdi, "Interface"))) {
		return status;
	}

	/*
	 * Get the list of bridges.
	 */
	spin_lock_bh(&ctx->lock);
	if (!list_empty(&ctx->list)) {
		bdi->br_cnt = 0;
		list_for_each_entry(b_pvt, &ctx->list, list) {
			status = nss_ppe_bridge_mgr_dump_one(bdi, b_pvt);
			if (status < 0) {
				nss_ppe_bridge_mgr_warn("%p: failed to collect dump for bridge: %p", ctx, b_pvt);
				spin_unlock_bh(&ctx->lock);
				return status;
			}

			bdi->br_cnt++;
		}
	}

	spin_unlock_bh(&ctx->lock);

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_dev_open()
 *	Opens the special char device file which we use to dump our state.
 */
static int nss_ppe_bridge_mgr_dump_dev_open(struct inode *inode, struct file *file)
{
	struct nss_ppe_bridge_mgr_dump_instance *bdi;

	nss_ppe_bridge_mgr_info("Bridge manager dump open\n");

	/*
	 * Allocate state information for rebding
	 */
	nss_ppe_bridge_mgr_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	bdi = (struct nss_ppe_bridge_mgr_dump_instance *)kzalloc(sizeof(struct nss_ppe_bridge_mgr_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!bdi) {
		return -ENOMEM;
	}

	bdi->dump_en = 1;
	file->private_data = bdi;

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_dev_release()
 *	Close the character device file for Bridge manager dump.
 */
static int nss_ppe_bridge_mgr_dump_dev_release(struct inode *inode, struct file *file)
{
	struct nss_ppe_bridge_mgr_dump_instance *bdi = (struct nss_ppe_bridge_mgr_dump_instance *)file->private_data;

	if (bdi) {
		kfree(bdi);
	}

	return 0;
}

/*
 * nss_ppe_bridge_mgr_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t nss_ppe_bridge_mgr_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct nss_ppe_bridge_mgr_dump_instance *bdi;

	bdi = (struct nss_ppe_bridge_mgr_dump_instance *)file->private_data;
	if (!bdi) {
		nss_ppe_bridge_mgr_warn("unable to find dump instance");
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (bdi->msg_len) {
		goto read_output;
	}

	if (bdi->dump_en) {
		if (nss_ppe_bridge_mgr_dump_all(bdi)) {
			nss_ppe_bridge_mgr_warn("Failed to create Bridge manager dump\n");
			return -EIO;
		}

		bdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = bdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, bdi->msgp, bytes_read)) {
		return -EIO;
	}

	bdi->msg_len -= bytes_read;
	bdi->msgp += bytes_read;

	nss_ppe_bridge_mgr_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n", bdi, bytes_read, bdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * nss_ppe_bridge_mgr_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t nss_ppe_bridge_mgr_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	return -EINVAL;
}

/*
 * File operations associated with bridge_mgr dump character device
 */
static struct file_operations nss_ppe_bridge_mgr_dump_fops = {
	.read = nss_ppe_bridge_mgr_dump_dev_read,
	.write = nss_ppe_bridge_mgr_dump_dev_write,
	.open = nss_ppe_bridge_mgr_dump_dev_open,
	.release = nss_ppe_bridge_mgr_dump_dev_release
};

/*
 * nss_ppe_bridge_mgr_dump_exit()
 *	Unregister character device for Bridge manager dump
 */
void nss_ppe_bridge_mgr_dump_exit(void)
{
	unregister_chrdev(nss_ppe_bridge_mgr_dump_major_id, "ppe_bridge_mgr_dump");
}

/*
 * nss_ppe_bridge_mgr_dump_init()
 */
int nss_ppe_bridge_mgr_dump_init(struct nss_ppe_bridge_mgr_context *ctx)
{
	int dev_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!debugfs_create_u32("ppe_bridge_mgr_dump", S_IRUGO, ctx->dentry,
				(u32 *)&nss_ppe_bridge_mgr_dump_major_id)) {
		nss_ppe_bridge_mgr_warn("Failed to create bridge_mgr dump file in debugfs\n");
		goto init_cleanup;
	}
#else
	debugfs_create_u32("ppe_bridge_mgr_dump", S_IRUGO, ctx->dentry,
				(u32 *)&nss_ppe_bridge_mgr_dump_major_id);
#endif

	/*
	 * Register a char device that we will use to provide a dump of our state
	 */
	dev_id = register_chrdev(0, "ppe_bridge_mgr_dump_dev", &nss_ppe_bridge_mgr_dump_fops);
	if (dev_id < 0) {
		nss_ppe_bridge_mgr_warn("Failed to register chrdev %d\n", dev_id);
		goto init_cleanup;
	}

	nss_ppe_bridge_mgr_dump_major_id = dev_id;
	nss_ppe_bridge_mgr_trace("registered chr dev major id assigned %d\n", nss_ppe_bridge_mgr_dump_major_id);

	return 0;

init_cleanup:
	debugfs_remove_recursive(ctx->dentry);
	ctx->dentry = NULL;
	return dev_id;
}
