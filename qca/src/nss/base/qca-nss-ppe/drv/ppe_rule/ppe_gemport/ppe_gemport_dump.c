/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_gemport.h"
#include "ppe_gemport_dump.h"

/*
 * ppe_gem_port_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_gem_port_dump_write_reset(struct ppe_gem_port_dump_instance *ddi, char *base_prefix)
{
	int result;

	ddi->msgp = ddi->msg;
	ddi->msg_len = 0;

	result = snprintf(ddi->prefix, PPE_GEM_PORT_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_GEM_PORT_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	ddi->prefix_level = 0;
	ddi->prefix_levels[ddi->prefix_level] = result;

	return 0;
}

/*
 * ppe_gem_port_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_gem_port_dump_prefix_add(struct ppe_gem_port_dump_instance *ddi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = ddi->prefix_levels[ddi->prefix_level];
	pxremain = PPE_GEM_PORT_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(ddi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	ddi->prefix_level++;
	ddi->prefix_levels[ddi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_gem_port_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_gem_port_dump_prefix_index_add(struct ppe_gem_port_dump_instance *ddi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = ddi->prefix_levels[ddi->prefix_level];
	pxremain = PPE_GEM_PORT_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(ddi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	ddi->prefix_level++;
	ddi->prefix_levels[ddi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_gem_port_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_gem_port_dump_prefix_remove(struct ppe_gem_port_dump_instance *ddi)
{
	int pxsz;

	ddi->prefix_level--;
	pxsz = ddi->prefix_levels[ddi->prefix_level];
	ddi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_gem_port_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_gem_port_dump_write(struct ppe_gem_port_dump_instance *ddi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_GEM_PORT_DUMP_BUFFER_SIZE - ddi->msg_len;
	ptr = ddi->msg + ddi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", ddi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	ddi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	ddi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	ddi->msg_len += result;
	return 0;
}

/*
 * ppe_gem_port_dump_one()
 *	Fill GEM PORT dump information for one GEM PORT rule.
 */
int ppe_gem_port_dump_one(struct ppe_gem_port_dump_instance *ddi, struct ppe_gem_port *gem_port)
{
	int result;
	struct ppe_gem_port_rule_match *r;
	struct ppe_gem_port_rule_action *r_action;
	r = &gem_port->rule.rule;

	/*
	 * Current GEM PORT count.
	 */
	if ((result = ppe_gem_port_dump_prefix_index_add(ddi, ddi->gem_port_cnt))) {
		goto error;
	}

	/*
	 * Rule information
	 */
	if ((result = ppe_gem_port_dump_prefix_add(ddi, "rule"))) {
		goto error;
	}

	if (r->rule_flags & PPE_GEM_PORT_RULE_FLAG_GEM_PORT_ID) {
		if ((result = ppe_gem_port_dump_write(ddi, "gem_port_id", "%d", r->gem_port_id))) {
			goto error;
		}
	}

	/*
	 * Remove 'rule' from action.
	 */
	if ((result = ppe_gem_port_dump_prefix_remove(ddi))) {
		goto error;
	}

	/*
	 * Action information.
	 */
	r_action = &gem_port->rule.action;
	if ((result = ppe_gem_port_dump_prefix_add(ddi, "action"))) {
		goto error;
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_SRC_INFO) {
		if ((result = ppe_gem_port_dump_write(ddi, "src_info", "%s", r_action->src_info))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_DST_INFO) {
		if ((result = ppe_gem_port_dump_write(ddi, "dst_info", "%s", r_action->dst_info))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_INT_PRI) {
		if ((result = ppe_gem_port_dump_write(ddi, "int_pri", "%d", r_action->int_pri))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_POLICER_ID) {
		if ((result = ppe_gem_port_dump_write(ddi, "policer_id", "%d", r_action->policer_id))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_INT_DP) {
		if ((result = ppe_gem_port_dump_write(ddi, "int_dp", "%d", r_action->int_dp))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_GEM_PORT_ACTION_FLAG_DST_SELECTION) {
		if ((result = ppe_gem_port_dump_write(ddi, "dst_selection", "%d", r_action->dst_selection))) {
			goto error;
		}
	}

	/*
	 * Remove the 'action' prefix for next interation
	 */
	if ((result = ppe_gem_port_dump_prefix_remove(ddi))) {
		goto error;
	}

	/*
	 * Remove the index prefix for next interation
	 */
	if ((result = ppe_gem_port_dump_prefix_remove(ddi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_gem_port_dump_all()
 *	Prepare GEM PORT dump information for all the active GEM PORT rules.
 */
static bool ppe_gem_port_dump_all(struct ppe_gem_port_dump_instance *ddi)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	struct ppe_gem_port *gem_port;
	int result;

	if ((result = ppe_gem_port_dump_write_reset(ddi, "gem_port"))) {
		return result;
	}

	/*
	 * Get the first available GEM PORT rule ID.
	 */
	spin_lock_bh(&gem_port_g->lock);
	if (!list_empty(&gem_port_g->active_rules)) {
		ddi->gem_port_cnt = 0;
		list_for_each_entry(gem_port, &gem_port_g->active_rules, list) {
			result = ppe_gem_port_dump_one(ddi, gem_port);
			if (result < 0) {
				ppe_gem_port_warn("%p: failed to collect dump for gem port: %p", gem_port_g, gem_port);
				spin_unlock_bh(&gem_port_g->lock);
				return result;
			}

			ddi->gem_port_cnt++;
		}
	}

	spin_unlock_bh(&gem_port_g->lock);

	return 0;
}

/*
 * ppe_gem_port_dump_dev_open()
 *	Open the character device file for GEM PORT dump.
 */
static int ppe_gem_port_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_gem_port_dump_instance *ddi;
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;

	/*
	 * Allocate state information for the reading
	 */
	ppe_gem_port_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	ddi = (struct ppe_gem_port_dump_instance *)kzalloc(sizeof(struct ppe_gem_port_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!ddi) {
		ppe_gem_port_warn("%p: unable to allocate memory for dump instance", gem_port_g);
		return -ENOMEM;
	}

	ddi->dump_en = true;
	file->private_data = ddi;

	return 0;
}

/*
 * ppe_gem_port_dump_dev_release()
 *	Close the character device file for GEM PORT dump.
 */
static int ppe_gem_port_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_gem_port_dump_instance *ddi = (struct ppe_gem_port_dump_instance *)file->private_data;
	if (ddi) {
		kfree(ddi);
	}

	return 0;
}

/*
 * ppe_gem_port_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_gem_port_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_gem_port_dump_instance *ddi;
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;

	ddi = (struct ppe_gem_port_dump_instance *)file->private_data;
	if (!ddi) {
		ppe_gem_port_warn("%p: unable to find dump instance", gem_port_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (ddi->msg_len) {
		goto read_output;
	}

	if (ddi->dump_en) {
		if (ppe_gem_port_dump_all(ddi)) {
			ppe_gem_port_warn("Failed to create gem port dump\n");
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

	ppe_gem_port_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			ddi, bytes_read, ddi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_gem_port_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_gem_port_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with dot1p dump character device
 */
static struct file_operations ppe_gem_port_dump_fops = {
	.read = ppe_gem_port_dump_dev_read,
	.write = ppe_gem_port_dump_dev_write,
	.open = ppe_gem_port_dump_dev_open,
	.release = ppe_gem_port_dump_dev_release
};

/*
 * ppe_gem-port_dump_exit()
 *	Unregister character device for GEM PORT dump
 */
void ppe_gem_port_dump_exit(void)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	debugfs_remove_recursive(gem_port_g->dentry);
	unregister_chrdev(gem_port_g->gem_port_dump_major_id, "ppe_gem_port_dump_dev");
}

/*
 * ppe_gem_port_dump_init()
 *	Register a character device for GEM PORT dump
 */
bool ppe_gem_port_dump_init(struct dentry *dentry)
{
	struct ppe_gem_port_base *gem_port_g = &ppe_gem_port_gbl;
	int dev_id = -1;
	gem_port_g->dentry = debugfs_create_dir("ppe-gemport", dentry);
	if (!gem_port_g->dentry) {
		ppe_gem_port_warn("%p: Unable to create GEM PORT debugfs directory\n", gem_port_g);
		return false;
	}

	debugfs_create_u32("ppe_gemport_dump", S_IRUGO, gem_port_g->dentry, (u32 *)&gem_port_g->gem_port_dump_major_id);

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_gem_port_dump_dev", &ppe_gem_port_dump_fops);
	if (dev_id < 0) {
		debugfs_remove_recursive(gem_port_g->dentry);
		ppe_gem_port_warn("%p: Failed to register chrdev %d\n", gem_port_g, dev_id);
		return false;
	}

	gem_port_g->gem_port_dump_major_id = dev_id;
	ppe_gem_port_trace("%p: gem port dump dev major id %d\n", gem_port_g, gem_port_g->gem_port_dump_major_id);

	return true;
}
