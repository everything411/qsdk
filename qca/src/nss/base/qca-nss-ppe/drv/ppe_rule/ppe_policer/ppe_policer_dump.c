/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_policer.h"
#include "ppe_policer_dump.h"
#include <linux/netdevice.h>

/*
 * ppe_policer_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_policer_dump_write_reset(struct ppe_policer_dump_instance *pdi, char *base_prefix)
{
	int result;

	pdi->msgp = pdi->msg;
	pdi->msg_len = 0;

	result = snprintf(pdi->prefix, PPE_POLICER_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_POLICER_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	pdi->prefix_level = 0;
	pdi->prefix_levels[pdi->prefix_level] = result;

	return 0;
}

/*
 * ppe_policer_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_policer_dump_prefix_add(struct ppe_policer_dump_instance *pdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pdi->prefix_levels[pdi->prefix_level];
	pxremain = PPE_POLICER_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(pdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pdi->prefix_level++;
	pdi->prefix_levels[pdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_policer_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_policer_dump_prefix_index_add(struct ppe_policer_dump_instance *pdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pdi->prefix_levels[pdi->prefix_level];
	pxremain = PPE_POLICER_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(pdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pdi->prefix_level++;
	pdi->prefix_levels[pdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_policer_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_policer_dump_prefix_remove(struct ppe_policer_dump_instance *pdi)
{
	int pxsz;

	pdi->prefix_level--;
	pxsz = pdi->prefix_levels[pdi->prefix_level];
	pdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_policer_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_policer_dump_write(struct ppe_policer_dump_instance *pdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_POLICER_DUMP_BUFFER_SIZE - pdi->msg_len;
	ptr = pdi->msg + pdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", pdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	pdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	pdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	pdi->msg_len += result;
	return 0;
}

/*
 * ppe_policer_dump_one()
 *	Fill POLICER dump information for one POLICER rule.
 */
int ppe_policer_dump_one(struct ppe_policer_dump_instance *pdi, struct ppe_policer *policer)
{
	int result;
	bool policer_type;
	struct ppe_drv_policer_hw_stats pol_ctx = {0};
	struct ppe_policer_create_info *policer_info = &policer->policer_info;
	policer_type = policer_info->policer_type;
	bool is_complete = false;

	/*
	 * Rule information
	 */
	if ((result = ppe_policer_dump_prefix_add(pdi, "rule"))) {
		goto error;
	}

	if (policer_type == PPE_POLICER_TYPE_PORT) {
		if ((result = ppe_policer_dump_prefix_add(pdi, "port-policer"))) {
			goto error;
		}

		if ((result = ppe_policer_dump_prefix_index_add(pdi, pdi->port_policer_cnt))) {
			goto error;
		}
	} else {
		if ((result = ppe_policer_dump_prefix_add(pdi, "acl-policer"))) {
			goto error;
		}

		if ((result = ppe_policer_dump_prefix_index_add(pdi, pdi->acl_policer_cnt))) {
			goto error;
		}
	}

	if (policer_type == PPE_POLICER_TYPE_PORT) {
		if ((result = ppe_policer_dump_write(pdi, "dev", "%s", policer_info->name))) {
			goto error;
		}

		if (policer->policer_info.rule_id) {
			if ((result = ppe_policer_dump_write(pdi, "user_id", "%u", policer_info->rule_id))) {
				goto error;
			}
		}

	} else {
		if ((result = ppe_policer_dump_write(pdi, "user_id", "%u", policer_info->rule_id))) {
			goto error;
		}

		if (!policer->num_dir) {
			goto dump_info;
		} else {
			if ((result = ppe_policer_dump_write(pdi, "Direction", "%u", policer_info->dir))) {
				goto error;
			}

			if (policer_info->dir == PPE_POLICER_DIRECTION_US) {
				if ((result = ppe_policer_dump_prefix_add(pdi, "US"))) {
					goto error;
				}
			} else if (policer_info->dir == PPE_POLICER_DIRECTION_DS) {
				if ((result = ppe_policer_dump_prefix_add(pdi, "DS"))) {
					goto error;
				}
			}
		}
	}

dump_info:
	if ((result = ppe_policer_dump_write(pdi, "CIR", "%u", policer_info->config.committed_rate))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "CBS", "%u", policer_info->config.committed_burst_size))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "EIR", "%u", policer_info->config.peak_rate))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "EBS", "%u", policer_info->config.peak_burst_size))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "METER_MODE", "%d", policer_info->config.mode))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "METER_UNIT", "%d", policer_info->config.meter_unit))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "METER_EN", "%d", policer_info->config.meter_enable))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "COUPLE_EN", "%d", policer_info->config.couple_enable))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "COLOUR_AWARE", "%d", policer_info->config.colour_aware))) {
		goto error;
	}

	if ((result = ppe_policer_dump_prefix_add(pdi, "yellow_action_info"))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_pri", "%d", policer_info->config.action_info.yellow_pri))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_dp", "%d", policer_info->config.action_info.yellow_dp))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_pcp", "%d", policer_info->config.action_info.yellow_pcp))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_dei", "%d", policer_info->config.action_info.yellow_dei))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_dscp", "%d", policer_info->config.action_info.yellow_dscp))) {
		goto error;
	}

	/*
	 * Remove the 'yellow_action_info' prefix
	 */
	if ((result = ppe_policer_dump_prefix_remove(pdi))) {
		goto error;
	}

	if (policer_type == PPE_POLICER_TYPE_ACL) {
		ppe_drv_policer_port_get_hw_stats(&pol_ctx, policer->drv_ctx.port_ctx);

	} else {
		ppe_drv_policer_acl_get_hw_stats(&pol_ctx, policer->drv_ctx.acl_ctx, (ppe_drv_policer_direction_t)policer_info->dir);
	}

	if ((result = ppe_policer_dump_prefix_add(pdi, "stats"))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "green_packet_counter", "%llu", pol_ctx.hw_cntrs.gpc))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "green_byte_counter", "%llu", pol_ctx.hw_cntrs.gbc))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_packet_counter", "%llu", pol_ctx.hw_cntrs.ypc))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "yellow_byte_counter", "%llu", pol_ctx.hw_cntrs.ybc))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "red_packet_counter", "%llu", pol_ctx.hw_cntrs.rpc))) {
		goto error;
	}

	if ((result = ppe_policer_dump_write(pdi, "red_byte_counter", "%llu", pol_ctx.hw_cntrs.rbc))) {
		goto error;
	}

	/*
	 * Remove the 'stats' prefix
	 */
	if ((result = ppe_policer_dump_prefix_remove(pdi))) {
		goto error;
	}

	/*
	 * Remove the 'direction' prefix
	 */
	if (policer->num_dir) {
		if ((result = ppe_policer_dump_prefix_remove(pdi))) {
			goto error;
		}

		/*
		 * Initialize to get the policer info for other direction policer
		 */
		if ((!is_complete) && (policer->num_dir == 2)) {
			policer_info = &policer->other_dir_policer_info;
			if ((result = ppe_policer_dump_write(pdi, "Direction", "%u", policer_info->dir))) {
				goto error;
			}

			if (policer_info->dir == PPE_POLICER_DIRECTION_US) {
				if ((result = ppe_policer_dump_prefix_add(pdi, "US"))) {
					goto error;
				}
			} else if (policer_info->dir == PPE_POLICER_DIRECTION_DS) {
				if ((result = ppe_policer_dump_prefix_add(pdi, "DS"))) {
					goto error;
				}
			}
			is_complete = true;
			goto dump_info;
		}
	}

	/*
	 * Remove prefix for policer index
	 */
	if ((result = ppe_policer_dump_prefix_remove(pdi))) {
		goto error;
	}

	/*
	 * Remove prefix for policer type
	 */
	if ((result = ppe_policer_dump_prefix_remove(pdi))) {
		goto error;
	}

	/*
	 * Remove prefix for rule
	 */
	if ((result = ppe_policer_dump_prefix_remove(pdi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_policer_dump_all()
 *	Prepare policer dump information for all the active POLICER rules.
 */
static bool ppe_policer_dump_all(struct ppe_policer_dump_instance *pdi)
{
	struct ppe_policer_base *policer_g = &gbl_ppe_policer;
	struct ppe_policer *policer;
	int result;

	if ((result = ppe_policer_dump_write_reset(pdi, "policer"))) {
		return result;
	}

	/*
	 * Get the first available policer rule ID.
	 */
	spin_lock_bh(&policer_g->lock);
	pdi->port_policer_cnt = 0;
	pdi->acl_policer_cnt = 0;

	if (!list_empty(&policer_g->port_active_rules)) {
		list_for_each_entry(policer, &policer_g->port_active_rules, list) {
			result = ppe_policer_dump_one(pdi, policer);
			if (result < 0) {
				pr_err("%p: failed to collect dump for port policer: %p", policer_g, policer);
				spin_unlock_bh(&policer_g->lock);
				return result;
			}

			pdi->port_policer_cnt++;
		}

	}

	if (!list_empty(&policer_g->acl_active_rules)) {
		list_for_each_entry(policer, &policer_g->acl_active_rules, list) {
			result = ppe_policer_dump_one(pdi, policer);
			if (result < 0) {
				pr_err("%p: failed to collect dump for acl policer: %p", policer_g, policer);
				spin_unlock_bh(&policer_g->lock);
				return result;
			}

			pdi->acl_policer_cnt++;
		}
	}


	spin_unlock_bh(&policer_g->lock);

	return 0;
}

/*
 * ppe_policer_dump_dev_open()
 *	Open the character device file for POLICER dump.
 */
static int ppe_policer_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_policer_dump_instance *pdi;
	struct ppe_policer_base *policer_g = &gbl_ppe_policer;

	/*
	 * Allocate state information for the repding
	 */
	ppe_policer_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	pdi = (struct ppe_policer_dump_instance *)kzalloc(sizeof(struct ppe_policer_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!pdi) {
		pr_err("%p: unable to allocate memory for dump instance", policer_g);
		return -ENOMEM;
	}

	pdi->dump_en = true;
	file->private_data = pdi;

	return 0;
}

/*
 * ppe_policer_dump_dev_release()
 *	Close the character device file for POLICER dump.
 */
static int ppe_policer_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_policer_dump_instance *pdi = (struct ppe_policer_dump_instance *)file->private_data;
	if (pdi) {
		kfree(pdi);
	}

	return 0;
}

/*
 * ppe_policer_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_policer_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_policer_dump_instance *pdi;
	struct ppe_policer_base *policer_g = &gbl_ppe_policer;

	pdi = (struct ppe_policer_dump_instance *)file->private_data;
	if (!pdi) {
		pr_err("%p: unable to find dump instance", policer_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (pdi->msg_len) {
		goto read_output;
	}

	if (pdi->dump_en) {
		if (ppe_policer_dump_all(pdi)) {
			pr_err("Failed to create policer dump\n");
			return -EIO;
		}

		pdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = pdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, pdi->msgp, bytes_read)) {
		return -EIO;
	}

	pdi->msg_len -= bytes_read;
	pdi->msgp += bytes_read;

	pr_err("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			pdi, bytes_read, pdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_policer_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_policer_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with policer dump character device
 */
static struct file_operations ppe_policer_dump_fops = {
	.read = ppe_policer_dump_dev_read,
	.write = ppe_policer_dump_dev_write,
	.open = ppe_policer_dump_dev_open,
	.release = ppe_policer_dump_dev_release
};

/*
 * ppe_policer_dump_exit()
 *	Unregister character device for POLICER dump
 */
void ppe_policer_dump_exit(void)
{
	struct ppe_policer_base *policer_g = &gbl_ppe_policer;

	unregister_chrdev(policer_g->policer_dump_major_id, "ppe_policer_dump_dev");
}

/*
 * ppe_policer_dump_init()
 *	Register a character device for POLICER dump
 */
bool ppe_policer_dump_init(struct dentry *dentry)
{
	struct ppe_policer_base *policer_g = &gbl_ppe_policer;
	int dev_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!debugfs_create_u32("ppe_policer_dump", S_IRUGO, policer_g->dentry, (u32 *)&policer_g->policer_dump_major_id)) {
		pr_err("%p: Failed to create ppe state dev major file in debugfs\n", policer_g);
		return false;
	}
#else
	debugfs_create_u32("ppe_policer_dump", S_IRUGO, policer_g->dentry, (u32 *)&policer_g->policer_dump_major_id);
#endif

	debugfs_create_u32("ppe_policer_dump", S_IRUGO, policer_g->dentry, (u32 *)&policer_g->policer_dump_major_id);

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_policer_dump_dev", &ppe_policer_dump_fops);
	if (dev_id < 0) {
		pr_err("%p: Failed to register chrdev %d\n", policer_g, dev_id);
		goto err_chrdev;
	}

	policer_g->policer_dump_major_id = dev_id;
	pr_err("%p: policer dump dev major id %d\n", policer_g, policer_g->policer_dump_major_id);

	return true;

err_chrdev:
	debugfs_remove(policer_g->dentry);
	return false;

}
