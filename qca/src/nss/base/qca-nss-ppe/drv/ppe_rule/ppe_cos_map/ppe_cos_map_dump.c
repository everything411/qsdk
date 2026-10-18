/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <ppe_drv_public.h>
#include <ppe_cos_map.h>
#include "ppe_cos_map.h"
#include "ppe_cos_map_dump.h"

/*
 * ppe_cos_map_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_cos_map_dump_write_reset(struct ppe_cos_map_dump_instance *pqmdi, char *base_prefix)
{
	int result;

	pqmdi->msgp = pqmdi->msg;
	pqmdi->msg_len = 0;

	result = snprintf(pqmdi->prefix, PPE_COS_MAP_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_COS_MAP_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	pqmdi->prefix_level = 0;
	pqmdi->prefix_levels[pqmdi->prefix_level] = result;

	return 0;
}

/*
 * ppe_cos_map_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_cos_map_dump_prefix_add(struct ppe_cos_map_dump_instance *pqmdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	if (pqmdi->prefix_level >= (PPE_COS_MAP_DUMP_PREFIX_MAX - 1)) {
		return -1;
	}

	pxsz = pqmdi->prefix_levels[pqmdi->prefix_level];
	pxremain = PPE_COS_MAP_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(pqmdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pqmdi->prefix_level++;
	pqmdi->prefix_levels[pqmdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_cos_map_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_cos_map_dump_prefix_index_add(struct ppe_cos_map_dump_instance *pqmdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = pqmdi->prefix_levels[pqmdi->prefix_level];
	pxremain = PPE_COS_MAP_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(pqmdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	pqmdi->prefix_level++;
	pqmdi->prefix_levels[pqmdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_cos_map_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_cos_map_dump_prefix_remove(struct ppe_cos_map_dump_instance *pqmdi)
{
	int pxsz;

	pqmdi->prefix_level--;
	pxsz = pqmdi->prefix_levels[pqmdi->prefix_level];
	pqmdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_cos_map_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_cos_map_dump_write(struct ppe_cos_map_dump_instance *pqmdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	if (pqmdi->msg_len >= PPE_COS_MAP_DUMP_BUFFER_SIZE) {
		return -1;
	}

	remain = PPE_COS_MAP_DUMP_BUFFER_SIZE - pqmdi->msg_len;
	ptr = pqmdi->msg + pqmdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", pqmdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	pqmdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	pqmdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	pqmdi->msg_len += result;
	return 0;
}

/*
 * ppe_cos_map_dump_one_rule()
 *	Prepare CoS map dump information for a rule.
 */
static int ppe_cos_map_dump_one_rule(struct ppe_cos_map_dump_instance *pqmdi,
				struct ppe_cos_map_rule *rule)
{
	int result = 0;

	/*
	 * Rule information
	 */
	if ((result = ppe_cos_map_dump_prefix_add(pqmdi, "rule"))) {
		goto error;
	}

	if ((result = ppe_cos_map_dump_prefix_index_add(pqmdi, pqmdi->rule_cnt))) {
		goto prefix_remove;
	}

	if ((result = ppe_cos_map_dump_write(pqmdi, "id", "%d", rule->rule_id))) {
		goto index_remove;
	}

	if ((result = ppe_cos_map_dump_write(pqmdi, "group", "%d", rule->cfg.group_id))) {
		goto index_remove;
	}

	switch (rule->type_flag) {
		case PPE_COS_MAP_RULE_TOS:
				if ((result = ppe_cos_map_dump_write(pqmdi, "type", "%s", "TOS"))) {
					goto index_remove;
				}

				if ((result = ppe_cos_map_dump_write(pqmdi, "dscp_val", "%d", rule->dscp_val))) {
					goto index_remove;
				}
			break;


		case PPE_COS_MAP_RULE_TCI:
				if ((result = ppe_cos_map_dump_write(pqmdi, "type", "%s", "TCI"))) {
					goto index_remove;
				}

				if ((result = ppe_cos_map_dump_write(pqmdi, "pcp_val", "%d", rule->pcp_val))) {
					goto index_remove;
				}

				if ((result = ppe_cos_map_dump_write(pqmdi, "dei_val", "%d", rule->dei_val))) {
					goto index_remove;
				}
			break;
	}

	if ((result = ppe_cos_map_dump_prefix_add(pqmdi, "action"))) {
		goto index_remove;
	}

	if (rule->cfg.map.flags & PPE_COS_MAP_ACTION_SET_DSCP) {
		if ((result = ppe_cos_map_dump_write(pqmdi, "dscp", "%d", rule->cfg.map.dscp))) {
			goto action_remove;
		}
	}

	if (rule->cfg.map.flags & PPE_COS_MAP_ACTION_SET_PCP) {
		if ((result = ppe_cos_map_dump_write(pqmdi, "pcp", "%d", rule->cfg.map.pcp))) {
			goto action_remove;
		}
	}

	if (rule->cfg.map.flags & PPE_COS_MAP_ACTION_SET_INT_PRI) {
		if ((result = ppe_cos_map_dump_write(pqmdi, "int_pri", "%d", rule->cfg.map.pri))) {
			goto action_remove;
		}
	}

	if (rule->cfg.map.flags & PPE_COS_MAP_ACTION_SET_DEI) {
		if ((result = ppe_cos_map_dump_write(pqmdi, "dei", "%d", rule->cfg.map.dei))) {
			goto action_remove;
		}
	}

	if (rule->cfg.map.flags & PPE_COS_MAP_ACTION_SET_DP) {
		if ((result = ppe_cos_map_dump_write(pqmdi, "dp", "%d", rule->cfg.map.dp))) {
			goto action_remove;
		}
	}

action_remove:
	/*
	 * Remove the 'action' prefix for next interation
	 */
	if ((result = ppe_cos_map_dump_prefix_remove(pqmdi))) {
		goto error;
	}

index_remove:
	/*
	 * Remove the 'index' prefix for next interation
	 */
	if ((result = ppe_cos_map_dump_prefix_remove(pqmdi))) {
		goto error;
	}

prefix_remove:
	/*
	 * Remove the rule prefix for next interation
	 */
	if ((result = ppe_cos_map_dump_prefix_remove(pqmdi))) {
		goto error;
	}

error:
	return result;

}

/*
 * ppe_cos_map_dump_all_rules()
 *	Prepare CoS map dump information for all the rules.
 */
static int ppe_cos_map_dump_all_rules(struct ppe_cos_map_dump_instance *pqmdi)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_rule *rule = NULL;
	int result;

	/*
	 * Dump all rules.
	 */
	if (!list_empty(&g_cos_map->active_rules)) {
		pqmdi->rule_cnt = 0;
		list_for_each_entry(rule,  &g_cos_map->active_rules, list) {
			result = ppe_cos_map_dump_one_rule(pqmdi, rule);
			if (result < 0) {
				ppe_cos_map_warn("%p: failed to collect dump for rule: %p", g_cos_map, rule);
				return result;
			}

			pqmdi->rule_cnt++;
		}
	}

	return 0;
}

/*
 * ppe_cos_map_dump_all_port_groups()
 *	Prepare CoS map dump information for all the port groups.
 */
static int ppe_cos_map_dump_all_port_groups(struct ppe_cos_map_dump_instance *pqmdi)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	struct ppe_cos_map_port_group *port_grp = NULL;
	int result = 0;

	/*
	 * Dump all configured port groups.
	 */
	if ((result = ppe_cos_map_dump_prefix_add(pqmdi, "port"))) {
		goto done;
	}

	/*
	 * Dump all port group configs.
	 */
	if (!list_empty(&g_cos_map->port_list)) {
		list_for_each_entry(port_grp,  &g_cos_map->port_list, list) {
			if ((result = ppe_cos_map_dump_prefix_index_add(pqmdi,  port_grp->port_id))) {
				goto done;
			}

			if ((result = ppe_cos_map_dump_write(pqmdi, "dev", "%s", port_grp->dev))) {
				goto done;
			}

			if ((result = ppe_cos_map_dump_write(pqmdi, "tci_group_id", "%d", port_grp->cfg.tci_grp_id))) {
				goto done;
			}

			if ((result = ppe_cos_map_dump_write(pqmdi, "tos_group_id", "%d", port_grp->cfg.tos_grp_id))) {
				goto done;
			}

			/*
			 * Remove the 'index' prefix for next interation
			 */
			if ((result = ppe_cos_map_dump_prefix_remove(pqmdi))) {
				goto done;
			}
		}
	}

done:
	/*
	 * Remove the port prefix
	 */
	result = ppe_cos_map_dump_prefix_remove(pqmdi);
	return result;
}

/*
 * ppe_cos_map_dump_all()
 *	Prepare CoS map dump information for all port groups and rules.
 */
static int ppe_cos_map_dump_all(struct ppe_cos_map_dump_instance *pqmdi)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	int result;

	if ((result = ppe_cos_map_dump_write_reset(pqmdi, "cos"))) {
		return result;
	}

	spin_lock_bh(&g_cos_map->lock);
	result = ppe_cos_map_dump_all_port_groups(pqmdi);
	if (result < 0) {
		spin_unlock_bh(&g_cos_map->lock);
		ppe_cos_map_warn("%p: failed to collect dump for shapers", g_cos_map);
		return result;
	}

	result = ppe_cos_map_dump_all_rules(pqmdi);
	if (result < 0) {
		spin_unlock_bh(&g_cos_map->lock);
		ppe_cos_map_warn("%p: failed to collect dump for interfaces", g_cos_map);
		return result;
	}

	spin_unlock_bh(&g_cos_map->lock);
	return 0;
}

/*
 * ppe_cos_map_dump_dev_open()
 *	Open the character device file for CoS dump.
 */
static int ppe_cos_map_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_cos_map_dump_instance *pqmdi;
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;

	/*
	 * Allocate state information for the reading
	 */
	ppe_cos_map_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	pqmdi = (struct ppe_cos_map_dump_instance *)kzalloc(sizeof(struct ppe_cos_map_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!pqmdi) {
		ppe_cos_map_warn("%p: unable to allocate memory for dump instance", g_cos_map);
		return -ENOMEM;
	}

	pqmdi->dump_en = true;
	file->private_data = pqmdi;

	return 0;
}

/*
 * ppe_cos_map_dump_dev_release()
 *	Close the character device file for CoS dump.
 */
static int ppe_cos_map_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_cos_map_dump_instance *pqmdi = (struct ppe_cos_map_dump_instance *)file->private_data;
	if (pqmdi) {
		kfree(pqmdi);
	}

	return 0;
}

/*
 * ppe_cos_map_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_cos_map_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_cos_map_dump_instance *pqmdi;
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;

	pqmdi = (struct ppe_cos_map_dump_instance *)file->private_data;
	if (!pqmdi) {
		ppe_cos_map_warn("%p: unable to find dump instance", g_cos_map);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (pqmdi->msg_len) {
		goto read_output;
	}

	if (pqmdi->dump_en) {
		if (ppe_cos_map_dump_all(pqmdi)) {
			ppe_cos_map_warn("Failed to create CoS map dump\n");
			return -EIO;
		}

		pqmdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = pqmdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, pqmdi->msgp, bytes_read)) {
		return -EIO;
	}

	pqmdi->msg_len -= bytes_read;
	pqmdi->msgp += bytes_read;

	ppe_cos_map_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			pqmdi, bytes_read, pqmdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_cos_map_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_cos_map_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with dot1p dump character device
 */
static struct file_operations ppe_cos_map_dump_fops = {
	.read = ppe_cos_map_dump_dev_read,
	.write = ppe_cos_map_dump_dev_write,
	.open = ppe_cos_map_dump_dev_open,
	.release = ppe_cos_map_dump_dev_release
};

/*
 * ppe_gem-port_dump_exit()
 *	Unregister character device for CoS dump
 */
void ppe_cos_map_dump_exit(void)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	debugfs_remove_recursive(g_cos_map->dentry);
	unregister_chrdev(g_cos_map->cos_map_dump_major_id, "ppe_cos_map_dump_dev");
}

/*
 * ppe_cos_map_dump_init()
 *	Register a character device for CoS dump
 */
bool ppe_cos_map_dump_init(struct dentry *dentry)
{
	struct ppe_cos_map_base *g_cos_map = &gbl_ppe_cos_map;
	int dev_id = -1;

	debugfs_create_u32("ppe_cos_map_dump", S_IRUGO, g_cos_map->dentry, (u32 *)&g_cos_map->cos_map_dump_major_id);

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_cos_map_dump_dev", &ppe_cos_map_dump_fops);
	if (dev_id < 0) {
		debugfs_remove_recursive(g_cos_map->dentry);
		ppe_cos_map_warn("%p: Failed to register chrdev %d\n", g_cos_map, dev_id);
		return false;
	}

	g_cos_map->cos_map_dump_major_id = dev_id;
	ppe_cos_map_trace("%p: cos map dump dev major id %d\n", g_cos_map, g_cos_map->cos_map_dump_major_id);

	return true;
}
