/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_vlan.h"
#include "ppe_vlan_dump.h"

/*
 * ppe_vlan_dump_rule_dir
 *	PPE VLAN rule direction strings.
 */
static char *ppe_vlan_dump_rule_dir[] = {
	"none",
	"upstream",
	"downstream",
};

/*
 * ppe_vlan_dump_tag_format
 *	PPE VLAN tag format strings.
 */
static char *ppe_vlan_dump_tag_format[] = {
	"untagged",
	"priority",
	"tagged",
	"pri_untag",
	"tag_untag",
	"pri_tag",
	"all",
};

/*
 * ppe_vlan_dump_port_type
 *	PPE VLAN port type strings.
 */
static char *ppe_vlan_dump_port_type[] = {
	"bitmap",
	"port",
	"gemport",
};

/*
 * ppe_vlan_dump_frame_type
 *	PPE VLAN frame type strings.
 */
static char *ppe_vlan_dump_frame_type[] = {
	"ethernet",
	"rfc_1024",
	"llc_other",
	"eth_or_rfc1024",
};

/*
 * ppe_vlan_dump_vni_resv_type
 *	PPE VLAN VNI reserve type strings.
 */
static char *ppe_vlan_dump_vni_resv_type[] = {
	"vni_only",
	"vni_resv",
};

/*
 * ppe_vlan_dump_dhcp_type
 *	PPE VLAN DHCP type strings.
 */
static char *ppe_vlan_dump_dhcp_type[] = {
	"non_dhcp",
	"dhcp_v4",
	"non_dhcp_v4",
	"dhcp_v6",
	"non_dhcp_v6",
	"dhcp_v4_v6",
	"all",
};

/*
 * ppe_vlan_dump_mc_type
 *	PPE VLAN MC type strings.
 */
static char *ppe_vlan_dump_mc_type[] = {
	"non_mc",
	"ip_mc",
	"non_mc_ip_mc",
	"non_ip_mc",
	"non_mc_non_ip_mc",
	"ip_mc_non_ip_mc",
	"all",
};

/*
 * ppe_vlan_dump_xlt_cmd
 *	PPE VLAN translation command strings.
 */
static char *ppe_vlan_dump_xlt_cmd[] = {
	"unchange",
	"add",
	"del",
	"cp_svid",
	"cp_cvid",
};

/*
 * ppe_vlan_dump_pcp_xlt_cmd
 *	PPE VLAN PCP translation command strings.
 */
static char *ppe_vlan_dump_pcp_xlt_cmd[] = {
	"unchange",
	"replace",
	"cp_spcp",
	"cp_cpcp",
	"dscp_pcp0",
	"dscp_pcp1",
	"add_rep_pcp",
	"add_cp_spcp",
	"add_cp_cpcp",
	"add_dscp_pcp0",
	"add_dscp_pcp1",
};

/*
 * ppe_vlan_dump_dei_xlt_cmd
 *	PPE VLAN DEI translation command strings.
 */
static char *ppe_vlan_dump_dei_xlt_cmd[] = {
	"unchange",
	"replace",
	"cp_sdei",
	"cp_cdei",
};

/*
 * ppe_vlan_dump_tpid_cmd
 *	PPE VLAN TPID command strings.
 */
static char *ppe_vlan_dump_tpid_cmd[] = {
	"unchange",
	"replace",
	"cp_stpid",
	"cp_ctpid",
};

/*
 * ppe_vlan_dump_counter_mode
 *	PPE VLAN counter mode strings.
 */
static char *ppe_vlan_dump_counter_mode[] = {
	"vlan",
	"pon_pm",
};

/*
 * ppe_vlan_dump_src_info_type
 *	PPE VLAN source info type strings.
 */
static char *ppe_vlan_dump_src_info_type[] = {
	"vp",
	"l3_if",
};

/*
 * ppe_vlan_dump_fwd_cmd
 *	PPE VLAN forward command strings.
 */
static char *ppe_vlan_dump_fwd_cmd[] = {
	"forward",
	"drop",
	"copy",
	"redirect",
};

/*
 * ppe_vlan_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_vlan_dump_write_reset(struct ppe_vlan_dump_instance *vdi, char *base_prefix)
{
	int result;

	vdi->msgp = vdi->msg;
	vdi->msg_len = 0;

	result = snprintf(vdi->prefix, PPE_VLAN_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_VLAN_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	vdi->prefix_level = 0;
	vdi->prefix_levels[vdi->prefix_level] = result;

	return 0;
}

/*
 * ppe_vlan_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_vlan_dump_prefix_add(struct ppe_vlan_dump_instance *vdi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = vdi->prefix_levels[vdi->prefix_level];
	pxremain = PPE_VLAN_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(vdi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	vdi->prefix_level++;
	vdi->prefix_levels[vdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_vlan_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_vlan_dump_prefix_index_add(struct ppe_vlan_dump_instance *vdi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = vdi->prefix_levels[vdi->prefix_level];
	pxremain = PPE_VLAN_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(vdi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	vdi->prefix_level++;
	vdi->prefix_levels[vdi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_vlan_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_vlan_dump_prefix_remove(struct ppe_vlan_dump_instance *vdi)
{
	int pxsz;

	vdi->prefix_level--;
	pxsz = vdi->prefix_levels[vdi->prefix_level];
	vdi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_vlan_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_vlan_dump_write(struct ppe_vlan_dump_instance *vdi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_VLAN_DUMP_BUFFER_SIZE - vdi->msg_len;
	ptr = vdi->msg + vdi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", vdi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	vdi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	vdi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	vdi->msg_len += result;
	return 0;
}

/*
 * ppe_vlan_dump_one()
 *	Fill VLAN dump information for one VLAN rule.
 */
int ppe_vlan_dump_one(struct ppe_vlan_dump_instance *vdi, struct ppe_vlan *vlan)
{
	int result;
	struct ppe_vlan_rule_match *r;
	struct ppe_vlan_rule_action *r_action;
	r = &vlan->rule.rule_f;

	/*
	 * Current VLAN count.
	 */
	if ((result = ppe_vlan_dump_prefix_index_add(vdi, vdi->vlan_cnt))) {
		goto error;
	}

	/*
	 * Rule information
	 */
	if ((result = ppe_vlan_dump_prefix_add(vdi, "rule"))) {
		goto error;
	}

	if ((result = ppe_vlan_dump_write(vdi, "rule_id", "%d", vlan->rule_id))) {
		goto error;
	}

	if ((result = ppe_vlan_dump_write(vdi, "rule_dir", "%s", ppe_vlan_dump_rule_dir[vlan->rule.rule_dir]))) {
		goto error;
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_PORT_TYPE) {
		if ((result = ppe_vlan_dump_write(vdi, "port_type", "%s", ppe_vlan_dump_port_type[r->port_type]))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_PORT_VAL) {
		switch (r->port_type) {
		case PPE_VLAN_PORT_TYPE_BITMAP:
			if ((result = ppe_vlan_dump_write(vdi, "port_val", "%u", r->port_val.port_bitmap))) {
				goto error;
			}
			break;
		case PPE_VLAN_PORT_TYPE_GEM_PORT:
			if ((result = ppe_vlan_dump_write(vdi, "port_val", "%u", r->port_val.gem_port))) {
				goto error;
			}
			break;
		default:
			if ((result = ppe_vlan_dump_write(vdi, "port_val", "%s", r->port_val.dev_name))) {
				goto error;
			}
			break;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_STAG_FORMAT) {
		if ((result = ppe_vlan_dump_write(vdi, "stag_format", "%s", ppe_vlan_dump_tag_format[r->stag_format]))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_SVID_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "svid_val", "%d", r->svid))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_SPCP_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "spcp_val", "%d", r->spcp))) {
			goto error;
		}
	}


	if (r->rule_flags & PPE_VLAN_RULE_FLAG_SDEI_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "sdei_val", "%d", r->sdei))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_CTAG_FORMAT) {
		if ((result = ppe_vlan_dump_write(vdi, "ctag_format", "%s", ppe_vlan_dump_tag_format[r->ctag_format]))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_CVID_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "cvid_val", "%d", r->cvid))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_CPCP_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "cpcp_val", "%d", r->cpcp))) {
			goto error;
		}
	}


	if (r->rule_flags & PPE_VLAN_RULE_FLAG_CDEI_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "cdei_val", "%d", r->cdei))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_FTYPE_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "frame_type", "%s", ppe_vlan_dump_frame_type[r->frame_type]))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_PROTO_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "protocol", "0x%x", r->proto))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_VSI_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "vsi", "%d", r->vsi))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_VNI_RESV_TYP) {
		if ((result = ppe_vlan_dump_write(vdi, "vni_resv_type", "%s", ppe_vlan_dump_vni_resv_type[r->vni_resv_type]))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_VNI_RESV_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "vni_resv", "%d", r->vni_resv))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_STPID) {
		if ((result = ppe_vlan_dump_write(vdi, "stpid", "0x%x", r->stpid))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_CTPID) {
		if ((result = ppe_vlan_dump_write(vdi, "ctpid", "0x%x", r->ctpid))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_DHCP_TYPE) {
		if ((result = ppe_vlan_dump_write(vdi, "dhcp_type", "%s", ppe_vlan_dump_dhcp_type[r->dhcp_type]))) {
			goto error;
		}
	}

	if (r->rule_flags & PPE_VLAN_RULE_FLAG_MC_TYPE) {
		if ((result = ppe_vlan_dump_write(vdi, "mc_type", "%s", ppe_vlan_dump_mc_type[r->mc_type]))) {
			goto error;
		}
	}

	/*
	 * Remove 'rule' from action.
	 */
	if ((result = ppe_vlan_dump_prefix_remove(vdi))) {
		goto error;
	}

	/*
	 * Action information
	 */
	r_action = &vlan->rule.action_f;
	if ((result = ppe_vlan_dump_prefix_add(vdi, "action"))) {
		goto error;
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_VID_SWP) {
		if ((result = ppe_vlan_dump_write(vdi, "swap_vid", "%d", r_action->swap_svid_cvid))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SVID_XLT_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "svid_xlate_cmd", "%s", ppe_vlan_dump_xlt_cmd[r_action->svid_xlate_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SVID_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "svid_xlate", "%d", r_action->svidxlate))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CVID_XLT_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "cvid_xlate_cmd", "%s", ppe_vlan_dump_xlt_cmd[r_action->cvid_xlate_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CVID_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "cvid_xlate", "%d", r_action->cvidxlate))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_PCP_SWP) {
		if ((result = ppe_vlan_dump_write(vdi, "swap_pcp", "%d", r_action->swap_spcp_cpcp))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SPCP_XLT_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "spcp_xlate_cmd", "%s", ppe_vlan_dump_pcp_xlt_cmd[r_action->spcp_xlate_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SPCP_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "spcp_xlate", "%d", r_action->spcptranslation))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CPCP_XLT_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "cpcp_xlate_cmd", "%s", ppe_vlan_dump_pcp_xlt_cmd[r_action->cpcp_xlate_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CPCP_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "cpcp_xlate", "%d", r_action->cpcptranslation))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_DEI_SWP) {
		if ((result = ppe_vlan_dump_write(vdi, "swap_dei", "%d", r_action->swap_sdei_cdei))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SDEI_XLT_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "sdei_xlate_cmd", "%s", ppe_vlan_dump_dei_xlt_cmd[r_action->sdei_xlate_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SDEI_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "sdei_xlate", "%d", r_action->sdeitranslation))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CDEI_XLT_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "cdei_xlate_cmd", "%s", ppe_vlan_dump_dei_xlt_cmd[r_action->cdei_xlate_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CDEI_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "cdei_xlate", "%d", r_action->cdeitranslation))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_TAGS_TO_REMOVE) {
		if ((result = ppe_vlan_dump_write(vdi, "tags_to_remove", "%d", r_action->tags_to_remove))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_STPID_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "stpid_cmd", "%s", ppe_vlan_dump_tpid_cmd[r_action->stpid_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_STPID) {
		if ((result = ppe_vlan_dump_write(vdi, "stpid", "0x%x", r_action->stpid_action))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CTPID_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "ctpid_cmd", "%s", ppe_vlan_dump_tpid_cmd[r_action->ctpid_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CTPID) {
		if ((result = ppe_vlan_dump_write(vdi, "ctpid", "0x%x", r_action->ctpid_action))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_MODE) {
		if ((result = ppe_vlan_dump_write(vdi, "counter_mode", "%s", ppe_vlan_dump_counter_mode[r_action->counter_mode]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_CNTR_ID) {
		if ((result = ppe_vlan_dump_write(vdi, "counter_id", "%d", r_action->counter_id))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_VSI_XLT_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "vsi_xlate", "%d", r_action->vsitranslation))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SRC_INFO_TYP) {
		if ((result = ppe_vlan_dump_write(vdi, "src_info_type", "%s", ppe_vlan_dump_src_info_type[r_action->src_info_type]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SRC_INFO_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "src_info", "%s", r_action->src_info))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_VNI_RESV_VAL) {
		if ((result = ppe_vlan_dump_write(vdi, "vni_resv", "%d", r_action->vni_resv_action))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_FWD_CMD) {
		if ((result = ppe_vlan_dump_write(vdi, "fwd_cmd", "%s", ppe_vlan_dump_fwd_cmd[r_action->fwd_cmd]))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_SVC_CODE) {
		if ((result = ppe_vlan_dump_write(vdi, "svc_code", "%d", r_action->sc))) {
			goto error;
		}
	}

	if (r_action->action_flags & PPE_VLAN_ACTION_FLAG_DEST_INFO) {
		if ((result = ppe_vlan_dump_write(vdi, "dest_info", "%s", r_action->dest_info))) {
			goto error;
		}
	}

	/*
	 * Remove the 'action' prefix for next interation
	 */
	if ((result = ppe_vlan_dump_prefix_remove(vdi))) {
		goto error;
	}

	/*
	 * Remove the index prefix for next interation
	 */
	if ((result = ppe_vlan_dump_prefix_remove(vdi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_vlan_dump_all()
 *	Prepare vlan dump information for all the active VLAN rules.
 */
static bool ppe_vlan_dump_all(struct ppe_vlan_dump_instance *vdi)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	struct ppe_vlan *vlan;
	int result;

	if ((result = ppe_vlan_dump_write_reset(vdi, "vlan"))) {
		return result;
	}

	/*
	 * Get the first available vlan rule ID.
	 */
	spin_lock_bh(&vlan_g->lock);
	if (!list_empty(&vlan_g->active_rules)) {
		vdi->vlan_cnt = 0;
		list_for_each_entry(vlan, &vlan_g->active_rules, list) {
			result = ppe_vlan_dump_one(vdi, vlan);
			if (result < 0) {
				ppe_vlan_warn("%p: failed to collect dump for vlan: %p", vlan_g, vlan);
				spin_unlock_bh(&vlan_g->lock);
				return result;
			}

			vdi->vlan_cnt++;
		}
	}

	spin_unlock_bh(&vlan_g->lock);

	return 0;
}

/*
 * ppe_vlan_dump_dev_open()
 *	Open the character device file for VLAN dump.
 */
static int ppe_vlan_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_vlan_dump_instance *vdi;
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	/*
	 * Allocate state information for the reading
	 */
	ppe_vlan_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	vdi = (struct ppe_vlan_dump_instance *)kzalloc(sizeof(struct ppe_vlan_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!vdi) {
		ppe_vlan_warn("%p: unable to allocate memory for dump instance", vlan_g);
		return -ENOMEM;
	}

	vdi->dump_en = true;
	file->private_data = vdi;

	return 0;
}

/*
 * ppe_vlan_dump_dev_release()
 *	Close the character device file for VLAN dump.
 */
static int ppe_vlan_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_vlan_dump_instance *vdi = (struct ppe_vlan_dump_instance *)file->private_data;

	if (vdi) {
		kfree(vdi);
	}

	return 0;
}

/*
 * ppe_vlan_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_vlan_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_vlan_dump_instance *vdi;
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	vdi = (struct ppe_vlan_dump_instance *)file->private_data;
	if (!vdi) {
		ppe_vlan_warn("%p: unable to find dump instance", vlan_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (vdi->msg_len) {
		goto read_output;
	}

	if (vdi->dump_en) {
		if (ppe_vlan_dump_all(vdi)) {
			ppe_vlan_warn("Failed to create vlan dump\n");
			return -EIO;
		}

		vdi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = vdi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, vdi->msgp, bytes_read)) {
		return -EIO;
	}

	vdi->msg_len -= bytes_read;
	vdi->msgp += bytes_read;

	ppe_vlan_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			vdi, bytes_read, vdi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_vlan_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_vlan_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with vlan dump character device
 */
static struct file_operations ppe_vlan_dump_fops = {
	.read = ppe_vlan_dump_dev_read,
	.write = ppe_vlan_dump_dev_write,
	.open = ppe_vlan_dump_dev_open,
	.release = ppe_vlan_dump_dev_release
};

/*
 * ppe_vlan_dump_exit()
 *	Unregister character device for VLAN dump
 */
void ppe_vlan_dump_exit(void)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;

	unregister_chrdev(vlan_g->vlan_dump_major_id, "ppe_vlan_dump_dev");
}

/*
 * ppe_vlan_dump_init()
 *	Register a character device for VLAN dump
 */
bool ppe_vlan_dump_init(struct dentry *dentry)
{
	struct ppe_vlan_base *vlan_g = &ppe_vlan_gbl;
	int dev_id = -1;

	vlan_g->dentry = debugfs_create_dir("ppe-vlan", dentry);
	if (!vlan_g->dentry) {
		ppe_vlan_warn("%p: Unable to create VLAN debugfs directory\n", vlan_g);
		return -1;
	}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!debugfs_create_u32("ppe_vlan_dump", S_IRUGO, vlan_g->dentry, (u32 *)&vlan_g->vlan_dump_major_id)) {
		ppe_vlan_warn("%p: Failed to create ppe state dev major file in debugfs\n", vlan_g);
		return false;
	}
#else
	debugfs_create_u32("ppe_vlan_dump", S_IRUGO, vlan_g->dentry, (u32 *)&vlan_g->vlan_dump_major_id);
#endif

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_vlan_dump_dev", &ppe_vlan_dump_fops);
	if (dev_id < 0) {
		ppe_vlan_warn("%p: Failed to register chrdev %d\n", vlan_g, dev_id);
		return false;
	}

	vlan_g->vlan_dump_major_id = dev_id;
	ppe_vlan_trace("%p: vlan dump dev major id %d\n", vlan_g, vlan_g->vlan_dump_major_id);

	return true;
}
