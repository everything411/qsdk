/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <ppe_mcast.h>
#include "ppe_mcast.h"
#include "ppe_mcast_dump.h"

/*
 * ppe_mcast_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_mcast_dump_write_reset(struct ppe_mcast_dump_instance *pmdi, char *base_prefix)
{
	int result;

	pmdi->msgp = pmdi->msg;
	pmdi->msg_len = 0;

	result = snprintf(pmdi->prefix, PPE_MCAST_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_MCAST_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	pmdi->prefix_level = 0;
	pmdi->prefix_levels[pmdi->prefix_level] = result;

	return 0;
}

/*
 * ppe_mcast_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_mcast_dump_prefix_add(struct ppe_mcast_dump_instance *pmdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pmdi->prefix_levels[pmdi->prefix_level];
	pxremain = PPE_MCAST_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(pmdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pmdi->prefix_level++;
	pmdi->prefix_levels[pmdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_mcast_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_mcast_dump_prefix_index_add(struct ppe_mcast_dump_instance *pmdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pmdi->prefix_levels[pmdi->prefix_level];
	pxremain = PPE_MCAST_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(pmdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pmdi->prefix_level++;
	pmdi->prefix_levels[pmdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_mcast_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_mcast_dump_prefix_remove(struct ppe_mcast_dump_instance *pmdi)
{
	int pxsz;

	pmdi->prefix_level--;
	pxsz = pmdi->prefix_levels[pmdi->prefix_level];
	pmdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_mcast_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_mcast_dump_write(struct ppe_mcast_dump_instance *pmdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_MCAST_DUMP_BUFFER_SIZE - pmdi->msg_len;
	ptr = pmdi->msg + pmdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", pmdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	pmdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	pmdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	pmdi->msg_len += result;
	return 0;
}

/*
 * ppe_mcast_dump_one_entry()
 *	Prepare multicast dump information for an entry.
 */
static bool ppe_mcast_dump_one_entry(struct ppe_mcast_dump_instance *pmdi,
				struct ppe_drv_mcast_entry *mc_entry,
				int entry_num)
{
	int result;

	/*
	 * Multicast entry information
	 */
	if ((result = ppe_mcast_dump_prefix_add(pmdi, "entry"))) {
		goto error;
	}

	if ((result = ppe_mcast_dump_prefix_index_add(pmdi, entry_num))) {
		goto error;
	}

	if (mc_entry->sip_valid) {
		if (mc_entry->is_v4) {
			if ((result = ppe_mcast_dump_write(pmdi, "sip address", "%pI4", &mc_entry->sip.v4))) {
				goto error;
			}
		} else {
			if ((result = ppe_mcast_dump_write(pmdi, "sip address", "%pI6", &mc_entry->sip.v6))) {
				goto error;
			}
		}
	}

	if (mc_entry->vlan_valid) {
		if ((result = ppe_mcast_dump_write(pmdi, "vlan ID", "%d", mc_entry->vid))) {
			goto error;
		}
	}

	if ((result = ppe_mcast_dump_write(pmdi, "port", "%d", mc_entry->port))) {
		goto error;
	}

	if ((result = ppe_mcast_dump_write(pmdi, "fwd cmd", "%d", mc_entry->fwd_cmd))) {
		goto error;
	}

	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((result = ppe_mcast_dump_prefix_remove(pmdi))) {
		goto error;
	}

	/*
	 * Remove the queue prefix for next interation
	 */
	if ((result = ppe_mcast_dump_prefix_remove(pmdi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_mcast_dump_one_group()
 *	Fill multicast dump information for one group.
 */
int ppe_mcast_dump_one_group(struct ppe_mcast_dump_instance *pmdi, struct ppe_mcast_group *group)
{
	int result;
	struct ppe_mcast_entry *mc_entry;
	int entry_num = 0;
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	int i;

	/*
	 * Rule information
	 */
	if ((result = ppe_mcast_dump_prefix_add(pmdi, "group"))) {
		goto error;
	}

	/*
	 * Current multicast group count.
	 */
	if ((result = ppe_mcast_dump_prefix_index_add(pmdi, pmdi->mcgrp_cnt))) {
		goto error;
	}

	if (group->unsupported_dst_cnt || group->omci_not_managed) {
		goto prefix_remove;
	}

	/*
	 * Dump Group address
	 */
	if (group->is_v4) {
		if ((result = ppe_mcast_dump_write(pmdi, "group address", "%pI4", &group->gip.v4))) {
			goto error;
		}
	} else {
		if ((result = ppe_mcast_dump_write(pmdi, "group address", "%pI6", &group->gip.v6))) {
			goto error;
		}
	}

	/*
	 * Dump all entries of the group using hash table
	 */
	hash_for_each(g_mcast->mc_hash_table, i, mc_entry, hash_node) {
		if (mc_entry->entry.is_v4 == group->is_v4) {
			if ((mc_entry->entry.is_v4 && mc_entry->entry.gip.v4 == group->gip.v4) ||
				(!mc_entry->entry.is_v4 && ppe_drv_mcast_v6_addr_equal(mc_entry->entry.gip.v6, group->gip.v6))) {
				entry_num++;
				ppe_mcast_dump_one_entry(pmdi, &mc_entry->entry, entry_num);
			}
		}
	}

prefix_remove:
	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((result = ppe_mcast_dump_prefix_remove(pmdi))) {
		goto error;
	}

	/*
	 * Remove the group prefix for next interation
	 */
	if ((result = ppe_mcast_dump_prefix_remove(pmdi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_mcast_dump_all_groups()
 *	Prepare multicast dump information for all the active multicast groups.
 */
static bool ppe_mcast_dump_all_groups(struct ppe_mcast_dump_instance *pmdi)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	struct ppe_mcast_group *group;
	int result;

	/*
	 * Dump all multicast groups.
	 */
	if (!list_empty(&g_mcast->mc_grp_list)) {
		pmdi->mcgrp_cnt = 0;
		list_for_each_entry(group, &g_mcast->mc_grp_list, list) {
			result = ppe_mcast_dump_one_group(pmdi, group);
			if (result < 0) {
				ppe_mcast_warn("%p: failed to collect dump for multicast group: %p", g_mcast, group);
				return result;
			}

			pmdi->mcgrp_cnt++;
		}
	}
	return 0;
}

/*
 * ppe_mcast_dump_all()
 *	Prepare multicast dump information for all the active multicast groups.
 */
static bool ppe_mcast_dump_all(struct ppe_mcast_dump_instance *pmdi)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	int result;

	if ((result = ppe_mcast_dump_write_reset(pmdi, "mcast"))) {
		return result;
	}

	spin_lock_bh(&g_mcast->lock);
	result = ppe_mcast_dump_all_groups(pmdi);
	if (result < 0) {
		spin_unlock_bh(&g_mcast->lock);
		ppe_mcast_warn("%p: failed to collect dump for groups", g_mcast);
		return result;
	}

	spin_unlock_bh(&g_mcast->lock);
	return 0;
}

/*
 * ppe_mcast_dump_dev_open()
 *	Open the character device file for multicast dump.
 */
static int ppe_mcast_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_mcast_dump_instance *pmdi;
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;

	/*
	 * Allocate state information for the reading
	 */
	ppe_mcast_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	pmdi = (struct ppe_mcast_dump_instance *)kzalloc(sizeof(struct ppe_mcast_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!pmdi) {
		ppe_mcast_warn("%p: unable to allocate memory for dump instance", g_mcast);
		return -ENOMEM;
	}

	pmdi->dump_en = true;
	file->private_data = pmdi;

	return 0;
}

/*
 * ppe_mcast_dump_dev_release()
 *	Close the character device file for multicast dump.
 */
static int ppe_mcast_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_mcast_dump_instance *pmdi = (struct ppe_mcast_dump_instance *)file->private_data;
	if (pmdi) {
		kfree(pmdi);
	}

	return 0;
}

/*
 * ppe_mcast_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_mcast_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_mcast_dump_instance *pmdi;
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;

	pmdi = (struct ppe_mcast_dump_instance *)file->private_data;
	if (!pmdi) {
		ppe_mcast_warn("%p: unable to find dump instance", g_mcast);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (pmdi->msg_len) {
		goto read_output;
	}

	if (pmdi->dump_en) {
		if (ppe_mcast_dump_all(pmdi)) {
			ppe_mcast_warn("Failed to create multicast dump\n");
			return -EIO;
		}

		pmdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = pmdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, pmdi->msgp, bytes_read)) {
		return -EIO;
	}

	pmdi->msg_len -= bytes_read;
	pmdi->msgp += bytes_read;

	ppe_mcast_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			pmdi, bytes_read, pmdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_mcast_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_mcast_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with dot1p dump character device
 */
static struct file_operations ppe_mcast_dump_fops = {
	.read = ppe_mcast_dump_dev_read,
	.write = ppe_mcast_dump_dev_write,
	.open = ppe_mcast_dump_dev_open,
	.release = ppe_mcast_dump_dev_release
};

/*
 * ppe_mcast_dump_exit()
 *	Unregister character device for multicast dump
 */
void ppe_mcast_dump_exit(void)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;

	unregister_chrdev(g_mcast->mcast_dump_major_id, "ppe_mcast_dump_dev");
}

/*
 * ppe_mcast_dump_init()
 *	Register a character device for multicast dump
 */
bool ppe_mcast_dump_init(struct dentry *dentry)
{
	struct ppe_mcast *g_mcast = &gbl_ppe_mcast;
	int dev_id = -1;

	debugfs_create_u32("ppe_mcast_dump", S_IRUGO, g_mcast->dentry, (u32 *)&g_mcast->mcast_dump_major_id);

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_mcast_dump_dev", &ppe_mcast_dump_fops);
	if (dev_id < 0) {
		ppe_mcast_warn("%p: Failed to register chrdev %d\n", g_mcast, dev_id);
		return false;
	}

	g_mcast->mcast_dump_major_id = dev_id;
	ppe_mcast_trace("%p: mcast dump dev major id %d\n", g_mcast, g_mcast->mcast_dump_major_id);

	return true;
}
