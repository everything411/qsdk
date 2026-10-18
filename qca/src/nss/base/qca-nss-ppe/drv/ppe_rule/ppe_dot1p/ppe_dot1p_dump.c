/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_dot1p.h"
#include "ppe_dot1p_dump.h"

/*
 * forward command strings
 */
static char *ppe_dot1p_dump_cmd_str[] = {
	"FWD",
	"DROP",
	"COPY",
	"REDIR",
};

/*
 * ppe_dot1p_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_dot1p_dump_write_reset(struct ppe_dot1p_dump_instance *ddi, char *base_prefix)
{
	int result;

	ddi->msgp = ddi->msg;
	ddi->msg_len = 0;

	result = snprintf(ddi->prefix, PPE_DOT1P_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_DOT1P_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	ddi->prefix_level = 0;
	ddi->prefix_levels[ddi->prefix_level] = result;

	return 0;
}

/*
 * ppe_dot1p_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_dot1p_dump_prefix_add(struct ppe_dot1p_dump_instance *ddi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = ddi->prefix_levels[ddi->prefix_level];
	pxremain = PPE_DOT1P_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(ddi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	ddi->prefix_level++;
	ddi->prefix_levels[ddi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_dot1p_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_dot1p_dump_prefix_index_add(struct ppe_dot1p_dump_instance *ddi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = ddi->prefix_levels[ddi->prefix_level];
	pxremain = PPE_DOT1P_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(ddi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	ddi->prefix_level++;
	ddi->prefix_levels[ddi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_dot1p_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_dot1p_dump_prefix_remove(struct ppe_dot1p_dump_instance *ddi)
{
	int pxsz;

	ddi->prefix_level--;
	pxsz = ddi->prefix_levels[ddi->prefix_level];
	ddi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_dot1p_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_dot1p_dump_write(struct ppe_dot1p_dump_instance *ddi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_DOT1P_DUMP_BUFFER_SIZE - ddi->msg_len;
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
 * ppe_dot1p_dump_def()
 *	Fill DOT1P dump information with the default DOT1P rule.
 */
int ppe_dot1p_dump_def(struct ppe_dot1p_dump_instance *ddi, struct ppe_dot1p_def_rule *r_def)
{
	int result;

	/*
	 * Default Rule information
	 */
	if ((result = ppe_dot1p_dump_prefix_add(ddi, "default"))) {
		goto error;
	}

	if (r_def->def_rule_flags & PPE_DOT1P_RULE_FLAG_VID) {
		if ((result = ppe_dot1p_dump_write(ddi, "vid", "%d", r_def->vid))) {
		goto error;
		}
	}

	if (r_def->def_rule_flags & PPE_DOT1P_RULE_FLAG_PCP) {
		if ((result = ppe_dot1p_dump_write(ddi, "pcp_info", "%d", r_def->pcp))) {
			goto error;
		}
	}

	if (r_def->def_rule_flags & PPE_DOT1P_RULE_FLAG_DEI) {
		if ((result = ppe_dot1p_dump_write(ddi, "dei_info", "%d", r_def->dei))) {
			goto error;
		}
	}

	if (r_def->def_rule_flags & PPE_DOT1P_RULE_FLAG_DSCP) {
		if ((result = ppe_dot1p_dump_write(ddi, "dscp_info", "%d", r_def->dscp))) {
			goto error;
		}
	}

	if (r_def->def_rule_flags & PPE_DOT1P_RULE_FLAG_DSCP_MASK) {
		if ((result = ppe_dot1p_dump_write(ddi, "dscp_mask", "%d", r_def->dscp_mask))) {
			goto error;
		}
	}

	if (r_def->def_rule_flags & PPE_DOT1P_RULE_FLAG_GEN_MISS_CMD) {
		if ((result = ppe_dot1p_dump_write(ddi, "gen_miss_cmd", "%s", ppe_dot1p_dump_cmd_str[r_def->gen_miss_cmd]))) {
			goto error;
		}
	}

	/*
	 * Remove 'rule' from action.
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}
error:
	return result;
}

/*
 * ppe_dot1p_policer_dump_one()
 *	Fill DOT1P dump information for one DOT1P policer rule.
 */
int ppe_dot1p_policer_dump_one(struct ppe_dot1p_dump_instance *ddi, struct ppe_dot1p_policer *dot1p)
{
	int result;
	struct ppe_dot1p_policer_rule_match *r;
	struct ppe_dot1p_policer_rule_action *r_action;
	r = &dot1p->rule.rule;
	uint8_t hw_index;


	ppe_drv_dot1p_policer_get_hw_index(dot1p->ctx, &hw_index);

	/*
	 * Current DOT1P count.
	 */
	if ((result = ppe_dot1p_dump_prefix_index_add(ddi, ddi->dot1p_cnt))) {
		goto error;
	}

	/*
	 * Rule information
	 */
	if ((result = ppe_dot1p_dump_prefix_add(ddi, "rule_policer"))) {
		goto error;
	}

	if ((result = ppe_dot1p_dump_write(ddi, "rule_hw_index", "%d", hw_index))) {
		goto error;
	}

	if ((result = ppe_dot1p_dump_write(ddi, "gemport_id", "%d", dot1p->gemport_id))) {
		goto error;
	}

	/*
	 * Remove 'rule' from action.
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}

	/*
	 * Action information.
	 */
	r_action = &dot1p->rule.action;
	if ((result = ppe_dot1p_dump_prefix_add(ddi, "action_policer"))) {
		goto error;
	}

	if (r_action->action_flags & PPE_DOT1P_POLICER_ACTION_FLAG_POLICER_ID) {
		if ((result = ppe_dot1p_dump_write(ddi, "policer_id", "%d", r_action->policer_id))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN) {
		if ((result = ppe_dot1p_dump_write(ddi, "us_policer_en", "%d", r_action->us_policer_en))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN) {
		if ((result = ppe_dot1p_dump_write(ddi, "ds_policer_en", "%d", r_action->ds_policer_en))) {
			goto error;
		}
	}

	/*
	 * Remove the 'action' prefix for next interation
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}

	/*
	 * Remove the index prefix for next interation
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_dot1p_dump_one()
 *	Fill DOT1P dump information for one DOT1P rule.
 */
int ppe_dot1p_dump_one(struct ppe_dot1p_dump_instance *ddi, struct ppe_dot1p *dot1p)
{
	int result;
	struct ppe_dot1p_rule_match *r;
	struct ppe_dot1p_rule_action *r_action;
	struct ppe_dot1p_def_rule *r_def;
	r_def = &dot1p->rule.def_rule;
	r = &dot1p->rule.rule;
	uint8_t hw_index;


	ppe_drv_dot1p_get_hw_index(dot1p->ctx, &hw_index);

	/*
	 * Current DOT1P count.
	 */
	if ((result = ppe_dot1p_dump_prefix_index_add(ddi, ddi->dot1p_cnt))) {
		goto error;
	}

	/*
	 * Rule information
	 */
	if ((result = ppe_dot1p_dump_prefix_add(ddi, "rule"))) {
		goto error;
	}

	if ((result = ppe_dot1p_dump_write(ddi, "rule_hw_index", "%d", hw_index))) {
		goto error;
	}

	if ((result = ppe_dot1p_dump_write(ddi, "rule_id", "%d", dot1p->rule_id))) {
		goto error;
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_SRC_INFO) {
		if ((result = ppe_dot1p_dump_write(ddi, "src_info", "%s", r->src_info))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_DST_INFO) {
		if ((result = ppe_dot1p_dump_write(ddi, "dst_info", "%s", r->dst_info))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_VID) {
		if ((result = ppe_dot1p_dump_write(ddi, "vid", "%d", r->vid))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_PCP) {
		if ((result = ppe_dot1p_dump_write(ddi, "pcp_info", "%d", r->pcp))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_DEI) {
		if ((result = ppe_dot1p_dump_write(ddi, "dei_info", "%d", r->dei))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_DOT1P_RULE_FLAG_DSCP) {
		if ((result = ppe_dot1p_dump_write(ddi, "dscp_info", "%d", r->dscp))) {
			goto error;
		}
	}

	/*
	 * Remove 'rule' from action.
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}

	/*
	 * Action information.
	 */
	r_action = &dot1p->rule.action;
	if ((result = ppe_dot1p_dump_prefix_add(ddi, "action"))) {
		goto error;
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_GEM_PORT_ID) {
		if ((result = ppe_dot1p_dump_write(ddi, "gem_port", "%d", r_action->gem_port))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_PQ) {
		if ((result = ppe_dot1p_dump_write(ddi, "pq", "%d", r_action->pq))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_BASE_PQ) {
		if ((result = ppe_dot1p_dump_write(ddi, "base_pq", "%d", r_action->base_pq))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_ENQ_VP) {
		if ((result = ppe_dot1p_dump_write(ddi, "enq_vp", "%d", r_action->enq_vp))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_INT_PRI) {
		if ((result = ppe_dot1p_dump_write(ddi, "int_pri", "%d", r_action->int_pri))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_SVC_CODE) {
		if ((result = ppe_dot1p_dump_write(ddi, "svc_code", "%d", r_action->svc_code))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_INT_DP) {
		if ((result = ppe_dot1p_dump_write(ddi, "int_dp", "%d", r_action->int_dp))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_FWD_CMD) {
		if ((result = ppe_dot1p_dump_write(ddi, "fwd_cmd", "%s",
						ppe_dot1p_dump_cmd_str[r_action->fwd_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_DOT1P_ACTION_FLAG_DST_INFO) {
		if ((result = ppe_dot1p_dump_write(ddi, "dst_info", "%s", r_action->dst_info))) {
			goto error;
		}
	}

	/*
	 * Remove the 'action' prefix for next interation
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}

	/*
	 * Remove the index prefix for next interation
	 */
	if ((result = ppe_dot1p_dump_prefix_remove(ddi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_dot1p_dump_all()
 *	Prepare DOT1P dump information for all the active DOT1P rules.
 */
static bool ppe_dot1p_dump_all(struct ppe_dot1p_dump_instance *ddi)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	struct ppe_dot1p *dot1p;
	struct ppe_dot1p_policer *dot1p_policer;
	int result;

	if ((result = ppe_dot1p_dump_write_reset(ddi, "dot1p"))) {
		return result;
	}

	/*
	 * Get the first available DOT1P rule ID.
	 */
	spin_lock_bh(&dot1p_g->lock);
	if (dot1p_g->def_rule.def_rule_flags != 0) {
		ppe_dot1p_dump_def(ddi, &dot1p_g->def_rule);
	}
	if (!list_empty(&dot1p_g->active_rules)) {
		ddi->dot1p_cnt = 0;
		list_for_each_entry(dot1p, &dot1p_g->active_rules, list) {
			result = ppe_dot1p_dump_one(ddi, dot1p);
			if (result < 0) {
				ppe_dot1p_warn("%p: failed to collect dump for dot1p: %p", dot1p_g, dot1p);
				spin_unlock_bh(&dot1p_g->lock);
				return result;
			}

			ddi->dot1p_cnt++;
		}
	}
	if (!list_empty(&dot1p_g->active_policer_rules)) {
		ddi->dot1p_cnt = 0;
		list_for_each_entry(dot1p_policer, &dot1p_g->active_policer_rules, list) {
			result = ppe_dot1p_policer_dump_one(ddi, dot1p_policer);
			if (result < 0) {
				ppe_dot1p_warn("%p: failed to collect dump for dot1p policer: %p", dot1p_g, dot1p);
				spin_unlock_bh(&dot1p_g->lock);
				return result;
			}

			ddi->dot1p_cnt++;
		}
	}

	spin_unlock_bh(&dot1p_g->lock);

	return 0;
}

/*
 * ppe_dot1p_dump_dev_open()
 *	Open the character device file for DOT1P dump.
 */
static int ppe_dot1p_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_dot1p_dump_instance *ddi;
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	/*
	 * Allocate state information for the reading
	 */
	ppe_dot1p_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	ddi = (struct ppe_dot1p_dump_instance *)kzalloc(sizeof(struct ppe_dot1p_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!ddi) {
		ppe_dot1p_warn("%p: unable to allocate memory for dump instance", dot1p_g);
		return -ENOMEM;
	}

	ddi->dump_en = true;
	file->private_data = ddi;

	return 0;
}

/*
 * ppe_dot1p_dump_dev_release()
 *	Close the character device file for DOT1P dump.
 */
static int ppe_dot1p_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_dot1p_dump_instance *ddi = (struct ppe_dot1p_dump_instance *)file->private_data;
	if (ddi) {
		kfree(ddi);
	}

	return 0;
}

/*
 * ppe_dot1p_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_dot1p_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_dot1p_dump_instance *ddi;
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;

	ddi = (struct ppe_dot1p_dump_instance *)file->private_data;
	if (!ddi) {
		ppe_dot1p_warn("%p: unable to find dump instance", dot1p_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (ddi->msg_len) {
		goto read_output;
	}

	if (ddi->dump_en) {
		if (ppe_dot1p_dump_all(ddi)) {
			ppe_dot1p_warn("Failed to create dot1p dump\n");
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

	ppe_dot1p_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			ddi, bytes_read, ddi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_dot1p_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_dot1p_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with dot1p dump character device
 */
static struct file_operations ppe_dot1p_dump_fops = {
	.read = ppe_dot1p_dump_dev_read,
	.write = ppe_dot1p_dump_dev_write,
	.open = ppe_dot1p_dump_dev_open,
	.release = ppe_dot1p_dump_dev_release
};

/*
 * ppe_dot1p_dump_exit()
 *	Unregister character device for DOT1P dump
 */
void ppe_dot1p_dump_exit(void)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	debugfs_remove_recursive(dot1p_g->dentry);
	unregister_chrdev(dot1p_g->dot1p_dump_major_id, "ppe_dot1p_dump_dev");
}

/*
 * ppe_dot1p_dump_init()
 *	Register a character device for DOT1P dump
 */
bool ppe_dot1p_dump_init(struct dentry *dentry)
{
	struct ppe_dot1p_base *dot1p_g = &ppe_dot1p_gbl;
	int dev_id = -1;
	dot1p_g->dentry = debugfs_create_dir("ppe-dot1p", dentry);
	if (!dot1p_g->dentry) {
		ppe_dot1p_warn("%p: Unable to create DOT1P debugfs directory\n", dot1p_g);
		return false;
	}

	debugfs_create_u32("ppe_dot1p_dump", S_IRUGO, dot1p_g->dentry, (u32 *)&dot1p_g->dot1p_dump_major_id);

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_dot1p_dump_dev", &ppe_dot1p_dump_fops);
	if (dev_id < 0) {
		debugfs_remove_recursive(dot1p_g->dentry);
		ppe_dot1p_warn("%p: Failed to register chrdev %d\n", dot1p_g, dev_id);
		return false;
	}

	dot1p_g->dot1p_dump_major_id = dev_id;
	ppe_dot1p_trace("%p: dot1p dump dev major id %d\n", dot1p_g, dot1p_g->dot1p_dump_major_id);

	return true;
}
