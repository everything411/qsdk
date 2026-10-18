/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG VLAN handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include "nss_ppenl_vlan_if.h"
#include "ppecfg_param.h"
#include "ppecfg_vlan.h"

#define MANDATORY_VLAN_FIELDS_MASK 0x0F
#define OPTIONAL_VLAN_FIELDS_MASK  0xF0

static int ppecfg_vlan_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_vlan_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_vlan_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * action add parameters
 */
static struct ppecfg_param action_params[PPECFG_VLAN_ACTION_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_VID_SWP, "swap_vid="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SVID_XLT_CMD, "svid_xlate_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SVID_XLT_VAL, "svid_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CVID_XLT_CMD, "cvid_xlate_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CVID_XLT_VAL, "cvid_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_PCP_SWP, "swap_pcp="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SPCP_XLT_CMD, "spcp_xlate_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SPCP_XLT_VAL, "spcp_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CPCP_XLT_CMD, "cpcp_xlate_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CPCP_XLT_VAL, "cpcp_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_DEI_SWP, "swap_dei="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SDEI_XLT_CMD, "sdei_xlate_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SDEI_XLT_VAL, "sdei_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CDEI_XLT_CMD, "cdei_xlate_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CDEI_XLT_VAL, "cdei_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_TAGS_TO_REMOVE, "tags_to_remove="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_STPID_CMD, "stpid_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_STPID, "stpid_action="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CTPID_CMD, "ctpid_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CTPID, "ctpid_action="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CNTR_ID, "counter_id="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_CNTR_MODE, "counter_mode="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_VSI_XLT_VAL, "vsi_xlate="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SRC_INFO_TYP, "src_info_type="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SRC_INFO_VAL, "src_info="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_VNI_RESV_VAL, "vni_resv_action="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_FWD_CMD, "fwd_cmd="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_SERVICE_CODE, "svc_code="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_ACTION_DEST_INFO, "dest_info="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param rule_add_params[PPECFG_VLAN_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_RULE_ID, "rule_id="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_PORT_TYPE, "port_type="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_PORT_VAL, "port_info="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_RULE_DIR, "rule_dir="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_STAG_FORMAT, "stagformat="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_SVID_VAL, "svid="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_SPCP_VAL, "spcp="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_SDEI_VAL, "sdei="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_CTAG_FORMAT, "ctagformat="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_CVID_VAL, "cvid="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_CPCP_VAL, "cpcp="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_CDEI_VAL, "cdei="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_FTYPE_VAL, "frame_type="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_PROTO_VAL, "protocol="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_VSI_VAL, "vsi="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_VNI_RESV_TYP, "vni_resv_type="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_VNI_RESV_VAL, "vni_resv="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_STPID, "stpid="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_CTPID, "ctpid="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_DHCP_TYPE, "dhcp_type="),
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_ADD_MC_TYPE, "mc_type="),
	PPECFG_PARAMARR_INIT(PPECFG_VLAN_RULE_ADD_ACTION, "action", action_params, ppecfg_param_iter_tbl),

};

/*
 * rule del parameters
 */
static struct ppecfg_param rule_del_params[PPECFG_VLAN_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_DEL_RULE_ID, "rule_id="),
};

/*
 * rule flush parameters
 */
static struct ppecfg_param rule_flush_params[PPECFG_VLAN_RULE_FLUSH_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_VLAN_RULE_FLUSH_FLAG, "except_first"),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_vlan_cmd' should also get updated
 */
struct ppecfg_param ppecfg_vlan_params[PPECFG_VLAN_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", rule_add_params, ppecfg_vlan_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", rule_del_params, ppecfg_vlan_rule_del),
	PPECFG_PARAMLIST_INIT("cmd=rule_flush", rule_flush_params, ppecfg_vlan_rule_flush),
};

/*
 * ppecfg_vlan_rule_add()
 *	Handle VLAN rule add
 */
static int ppecfg_vlan_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_vlan_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
	uint8_t field_mask = 0;
	char buf[32];

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_vlan_init_rule(&nl_msg, NSS_PPE_VLAN_CREATE_RULE_MSG);

	for (int index = PPECFG_VLAN_RULE_ADD_RULE_ID; index < PPECFG_VLAN_RULE_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];

		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
			case PPECFG_VLAN_RULE_ADD_RULE_ID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_RULE_ID;
				field_mask |= 0x01;
				break;

			case PPECFG_VLAN_RULE_ADD_PORT_TYPE:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "bitmap") == 0) {
					nl_msg.rule.rule_f.port_type = PPE_VLAN_PORT_TYPE_BITMAP;
				} else if (strcasecmp(buf, "port") == 0) {
					nl_msg.rule.rule_f.port_type = PPE_VLAN_PORT_TYPE_PORT;
				} else if (strcasecmp(buf, "gem_port") == 0) {
					nl_msg.rule.rule_f.port_type = PPE_VLAN_PORT_TYPE_GEM_PORT;
				} else {
					ppecfg_log_error("Invalid port_type: %s. Valid: bitmap, port, gem_port\n", buf);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_PORT_TYPE;

				field_mask |= 0x02;
				break;

			case PPECFG_VLAN_RULE_ADD_PORT_VAL:
				if (!(field_mask & 0x02)) {
					ppecfg_log_error("port_type must be provided before port_info\n");
					error = -EINVAL;
					goto done;
				}

				if (nl_msg.rule.rule_f.port_type == PPE_VLAN_PORT_TYPE_BITMAP) {
					error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.port_val.port_bitmap);
				} else if (nl_msg.rule.rule_f.port_type == PPE_VLAN_PORT_TYPE_PORT) {
					error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.rule.rule_f.port_val.dev_name),
							nl_msg.rule.rule_f.port_val.dev_name);
				} else if (nl_msg.rule.rule_f.port_type == PPE_VLAN_PORT_TYPE_GEM_PORT) {
					error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.port_val.gem_port);
				} else {
					ppecfg_log_error("Unhandled port_type %d\n", nl_msg.rule.rule_f.port_type);
					error = -EINVAL;
				}

				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_PORT_VAL;

				field_mask |= 0x04;
				break;

			case PPECFG_VLAN_RULE_ADD_RULE_DIR:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "upstream") == 0) {
					nl_msg.rule.rule_dir = PPE_VLAN_RULE_DIR_INGRESS;
				} else if (strcasecmp(buf, "downstream") == 0) {
					nl_msg.rule.rule_dir = PPE_VLAN_RULE_DIR_EGRESS;
				} else {
					ppecfg_log_error("Invalid rule_dir: %s. Valid: upstream, downstream\n", buf);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_FLOW_DIR;
				field_mask |= 0x08;
				break;

			case PPECFG_VLAN_RULE_ADD_STAG_FORMAT:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "untag") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_UNTAGGED;
				} else if (strcasecmp(buf, "pri") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_PRIORITY;
				} else if (strcasecmp(buf, "tag") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_TAGGED;
				} else if (strcasecmp(buf, "pri_untag") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_PRI_UNTAG;
				} else if (strcasecmp(buf, "tag_untag") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_TAG_UNTAG;
				} else if (strcasecmp(buf, "pri_tag") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_PRI_TAG;
				} else if (strcasecmp(buf, "all") == 0) {
					nl_msg.rule.rule_f.stag_format = PPE_VLAN_TAG_FORMAT_ALL;
				} else {
					ppecfg_log_error("Invalid stagformat: %s. Valid: untag, pri, tag, pri_untag, tag_untag, pri_tag, all\n", buf);
					goto done;
				}

				field_mask |= 0x10;
				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_STAG_FORMAT;

				break;

			case PPECFG_VLAN_RULE_ADD_SVID_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.svid);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.svid > 4095) {
					ppecfg_log_error("Invalid svid=%u. Valid range is 0-4095\n", nl_msg.rule.rule_f.svid);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_SVID_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_SPCP_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.spcp);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.spcp > 7) {
					ppecfg_log_error("Invalid spcp=%u. Valid range is 0-7\n", nl_msg.rule.rule_f.spcp);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_SPCP_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_SDEI_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.sdei);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.sdei > 1) {
					ppecfg_log_error("Invalid sdei=%u. Valid range is 0-1\n", nl_msg.rule.rule_f.sdei);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_SDEI_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_CTAG_FORMAT:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "untag") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_UNTAGGED;
				} else if (strcasecmp(buf, "pri") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_PRIORITY;
				} else if (strcasecmp(buf, "tag") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_TAGGED;
				} else if (strcasecmp(buf, "pri_untag") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_PRI_UNTAG;
				} else if (strcasecmp(buf, "tag_untag") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_TAG_UNTAG;
				} else if (strcasecmp(buf, "pri_tag") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_PRI_TAG;
				} else if (strcasecmp(buf, "all") == 0) {
					nl_msg.rule.rule_f.ctag_format = PPE_VLAN_TAG_FORMAT_ALL;
				} else {
					ppecfg_log_error("Invalid ctagformat: %s. Valid: untag, pri, tag, pri_untag, tag_untag, pri_tag, all\n", buf);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_CTAG_FORMAT;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_CVID_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.cvid);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.cvid > 4095) {
					ppecfg_log_error("Invalid cvid=%u. Valid range is 0-4095\n", nl_msg.rule.rule_f.cvid);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_CVID_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_CPCP_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.cpcp);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.cpcp > 7) {
					ppecfg_log_error("Invalid cpcp=%u. Valid range is 0-7\n", nl_msg.rule.rule_f.cpcp);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_CPCP_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_CDEI_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.cdei);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.cdei > 1) {
					ppecfg_log_error("Invalid cdei=%u. Valid range is 0-1\n", nl_msg.rule.rule_f.cdei);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_CDEI_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_FTYPE_VAL:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "ethernet") == 0) {
					nl_msg.rule.rule_f.frame_type = PPE_VLAN_FRAME_TYPE_ETHERNET;
				} else if (strcasecmp(buf, "rfc_1024") == 0 || strcasecmp(buf, "rfc1024") == 0) {
					nl_msg.rule.rule_f.frame_type = PPE_VLAN_FRAME_TYPE_RFC_1024;
				} else if (strcasecmp(buf, "llc_other") == 0 || strcasecmp(buf, "llc") == 0) {
					nl_msg.rule.rule_f.frame_type = PPE_VLAN_FRAME_TYPE_LLC_OTHER;
				} else if (strcasecmp(buf, "ethorrfc1024") == 0) {
					nl_msg.rule.rule_f.frame_type = PPE_VLAN_FRAME_TYPE_ETHORRFC1024;
				} else {
					ppecfg_log_error("Invalid frame_type: %s. Valid: ethernet, rfc_1024, llc, ethorrfc1024\n", buf);
					goto done;
				}

				field_mask |= 0x10;
				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_FTYPE_VAL;

				break;

			case PPECFG_VLAN_RULE_ADD_PROTO_VAL:
				{
					uint32_t val;
					error = ppecfg_param_get_int(sub_params->data, sizeof(val), &val);
					if (error < 0) {
						ppecfg_log_data_error(sub_params);
						goto done;
					}

					if (val > 65535) {
						ppecfg_log_error("Invalid protocol=%u. Valid range is 0-65535\n", val);
						goto done;
					}

					nl_msg.rule.rule_f.proto = val;
					field_mask |= 0x10;
					nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_PROTO_VAL;

					break;
				}

			case PPECFG_VLAN_RULE_ADD_VSI_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_f.vsi);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (nl_msg.rule.rule_f.vsi > 63) {
					ppecfg_log_error("Invalid vsi=%u. Valid range is 0-63\n", nl_msg.rule.rule_f.vsi);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_VSI_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_VNI_RESV_TYP:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "vni_only") == 0) {
					nl_msg.rule.rule_f.vni_resv_type = PPE_VLAN_VNI_RESV_TYPE_VNI_ONLY;
				} else if (strcasecmp(buf, "vni_resv") == 0) {
					nl_msg.rule.rule_f.vni_resv_type = PPE_VLAN_VNI_RESV_TYPE_VNI_RESV;
				} else {
					ppecfg_log_error("Invalid vni_resv_type: %s. Valid: vni_only, vni_resv\n", buf);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_VNI_RESV_TYP;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_VNI_RESV_VAL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.rule.rule_f.vni_resv), &nl_msg.rule.rule_f.vni_resv);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_VNI_RESV_VAL;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_STPID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.rule_f.stpid);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_STPID;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_CTPID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.rule_f.ctpid);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_CTPID;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_DHCP_TYPE:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "non_dhcp") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_NON_DHCP;
				} else if (strcasecmp(buf, "dhcp_v4") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_DHCP_V4;
				} else if (strcasecmp(buf, "non_dhcp_v4") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_NON_DHCP_V4;
				} else if (strcasecmp(buf, "dhcp_v6") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_DHCP_V6;
				} else if (strcasecmp(buf, "non_dhcp_v6") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_NON_DHCP_V6;
				} else if (strcasecmp(buf, "dhcp_v4_v6") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_DHCP_V4_V6;
				} else if (strcasecmp(buf, "all") == 0) {
					nl_msg.rule.rule_f.dhcp_type = PPE_VLAN_DHCP_TYPE_ALL;
				} else {
					ppecfg_log_error("Invalid dhcp_type: %s. Valid: non_dhcp, dhcp_v4, non_dhcp_v4, dhcp_v6, non_dhcp_v6, dhcp_v4_v6, all\n", buf);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_DHCP_TYPE;

				field_mask |= 0x10;
				break;

			case PPECFG_VLAN_RULE_ADD_MC_TYPE:
				error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (strcasecmp(buf, "non_mc") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_NON_MC;
				} else if (strcasecmp(buf, "ip_mc") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_IP_MC;
				} else if (strcasecmp(buf, "non_mc_ip_mc") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_NON_MC_IP_MC;
				} else if (strcasecmp(buf, "non_ip_mc") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_NON_IP_MC;
				} else if (strcasecmp(buf, "non_mc_non_ip_mc") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_NON_MC_NON_IP_MC;
				} else if (strcasecmp(buf, "ip_mc_non_ip_mc") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_IP_MC_NON_IP_MC;
				} else if (strcasecmp(buf, "all") == 0) {
					nl_msg.rule.rule_f.mc_type = PPE_VLAN_MC_TYPE_ALL;
				} else {
					ppecfg_log_error("Invalid mc_type: %s. Valid: non_mc, ip_mc, non_mc_ip_mc, non_ip_mc, non_mc_non_ip_mc, ip_mc_non_ip_mc, all\n", buf);
					goto done;
				}

				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_MC_TYPE;

				field_mask |= 0x10;
				break;


			case PPECFG_VLAN_RULE_ADD_ACTION:
				nl_msg.rule.rule_f.rule_flags |= PPE_VLAN_RULE_FLAG_ACTION;
				sub_params = param->sub_params[PPECFG_VLAN_RULE_ADD_ACTION].sub_params;
				data = sub_params[PPECFG_VLAN_ACTION_VID_SWP].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "enable") == 0) {
						nl_msg.rule.action_f.swap_svid_cvid = true;
					} else if (strcasecmp(buf, "disable") == 0) {
						nl_msg.rule.action_f.swap_svid_cvid = false;
					} else {
						ppecfg_log_error("Invalid swap_vid: %s. Valid: enable, disable\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_VID_SWP;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SVID_XLT_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.svid_xlate_cmd = PPE_VLAN_XLT_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "add") == 0) {
						nl_msg.rule.action_f.svid_xlate_cmd = PPE_VLAN_XLT_CMD_ADD;
					} else if (strcasecmp(buf, "del") == 0) {
						nl_msg.rule.action_f.svid_xlate_cmd = PPE_VLAN_XLT_CMD_DEL;
					} else if (strcasecmp(buf, "cp_svid") == 0) {
						nl_msg.rule.action_f.svid_xlate_cmd = PPE_VLAN_XLT_CMD_CP_SVID;
					} else if (strcasecmp(buf, "cp_cvid") == 0) {
						nl_msg.rule.action_f.svid_xlate_cmd = PPE_VLAN_XLT_CMD_CP_CVID;
					} else {
						ppecfg_log_error("Invalid svid_xlate_cmd: %s. Valid: unchange, add, del, cp_svid, cp_cvid\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SVID_XLT_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SVID_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.rule.action_f.svidxlate);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.svidxlate > 4095) {
						ppecfg_log_error("Invalid svid_xlate=%u. Valid range is 0-4095\n", nl_msg.rule.action_f.svidxlate);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SVID_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CVID_XLT_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.cvid_xlate_cmd = PPE_VLAN_XLT_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "add") == 0) {
						nl_msg.rule.action_f.cvid_xlate_cmd = PPE_VLAN_XLT_CMD_ADD;
					} else if (strcasecmp(buf, "del") == 0) {
						nl_msg.rule.action_f.cvid_xlate_cmd = PPE_VLAN_XLT_CMD_DEL;
					} else if (strcasecmp(buf, "cp_svid") == 0) {
						nl_msg.rule.action_f.cvid_xlate_cmd = PPE_VLAN_XLT_CMD_CP_SVID;
					} else if (strcasecmp(buf, "cp_cvid") == 0) {
						nl_msg.rule.action_f.cvid_xlate_cmd = PPE_VLAN_XLT_CMD_CP_CVID;
					} else {
						ppecfg_log_error("Invalid cvid_xlate_cmd: %s. Valid: unchange, add, del, cp_svid, cp_cvid\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CVID_XLT_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CVID_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.rule.action_f.cvidxlate);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.cvidxlate > 4095) {
						ppecfg_log_error("Invalid cvid_xlate=%u. Valid range is 0-4095\n", nl_msg.rule.action_f.cvidxlate);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CVID_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_PCP_SWP].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "enable") == 0) {
						nl_msg.rule.action_f.swap_spcp_cpcp = true;
					} else if (strcasecmp(buf, "disable") == 0) {
						nl_msg.rule.action_f.swap_spcp_cpcp = false;
					} else {
						ppecfg_log_error("Invalid swap_pcp: %s. Valid: enable, disable\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_PCP_SWP;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SPCP_XLT_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "replace") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_REPLACE;
					} else if (strcasecmp(buf, "cp_spcp") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_CP_SPCP;
					} else if (strcasecmp(buf, "cp_cpcp") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_CP_CPCP;
					} else if (strcasecmp(buf, "dscp-pcp0") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_DSCP_PCP0;
					} else if (strcasecmp(buf, "dscp-pcp1") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_DSCP_PCP1;
					} else if (strcasecmp(buf, "add_rep_pcp") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_REP_PCP;
					} else if (strcasecmp(buf, "add_cp_spcp") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_CP_SPCP;
					} else if (strcasecmp(buf, "add_cp_cpcp") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_CP_CPCP;
					} else if (strcasecmp(buf, "add_dscp-pcp0") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP0;
					} else if (strcasecmp(buf, "add_dscp-pcp1") == 0) {
						nl_msg.rule.action_f.spcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP1;
					} else {
						ppecfg_log_error("Invalid spcp_xlate_cmd: %s\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SPCP_XLT_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SPCP_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action_f.spcptranslation);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.spcptranslation > 7) {
						ppecfg_log_error("Invalid spcp_xlate=%u. Valid range is 0-7\n", nl_msg.rule.action_f.spcptranslation);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SPCP_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CPCP_XLT_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "replace") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_REPLACE;
					} else if (strcasecmp(buf, "cp_spcp") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_CP_SPCP;
					} else if (strcasecmp(buf, "cp_cpcp") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_CP_CPCP;
					} else if (strcasecmp(buf, "dscp-pcp0") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_DSCP_PCP0;
					} else if (strcasecmp(buf, "dscp-pcp1") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_DSCP_PCP1;
					} else if (strcasecmp(buf, "add_rep_pcp") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_REP_PCP;
					} else if (strcasecmp(buf, "add_cp_spcp") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_CP_SPCP;
					} else if (strcasecmp(buf, "add_cp_cpcp") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_CP_CPCP;
					} else if (strcasecmp(buf, "add_dscp-pcp0") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP0;
					} else if (strcasecmp(buf, "add_dscp-pcp1") == 0) {
						nl_msg.rule.action_f.cpcp_xlate_cmd = PPE_VLAN_PCP_XLT_CMD_ADD_DSCP_PCP1;
					} else {
						ppecfg_log_error("Invalid cpcp_xlate_cmd: %s\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CPCP_XLT_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CPCP_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action_f.cpcptranslation);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.cpcptranslation > 7) {
						ppecfg_log_error("Invalid cpcp_xlate=%u. Valid range is 0-7\n", nl_msg.rule.action_f.cpcptranslation);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CPCP_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_DEI_SWP].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "enable") == 0) {
						nl_msg.rule.action_f.swap_sdei_cdei = true;
					} else if (strcasecmp(buf, "disable") == 0) {
						nl_msg.rule.action_f.swap_sdei_cdei = false;
					} else {
						ppecfg_log_error("Invalid swap_dei: %s. Valid: enable, disable\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_DEI_SWP;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SDEI_XLT_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.sdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "replace") == 0) {
						nl_msg.rule.action_f.sdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_REPLACE;
					} else if (strcasecmp(buf, "cp_sdei") == 0) {
						nl_msg.rule.action_f.sdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_CP_SDEI;
					} else if (strcasecmp(buf, "cp_cdei") == 0) {
						nl_msg.rule.action_f.sdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_CP_CDEI;
					} else {
						ppecfg_log_error("Invalid sdei_xlate_cmd: %s. Valid: unchange, replace, cp_sdei, cp_cdei\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SDEI_XLT_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SDEI_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action_f.sdeitranslation);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.sdeitranslation > 1) {
						ppecfg_log_error("Invalid sdei_xlate=%u. Valid range is 0-1\n", nl_msg.rule.action_f.sdeitranslation);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SDEI_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CDEI_XLT_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.cdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "replace") == 0) {
						nl_msg.rule.action_f.cdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_REPLACE;
					} else if (strcasecmp(buf, "cp_sdei") == 0) {
						nl_msg.rule.action_f.cdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_CP_SDEI;
					} else if (strcasecmp(buf, "cp_cdei") == 0) {
						nl_msg.rule.action_f.cdei_xlate_cmd = PPE_VLAN_DEI_XLT_CMD_CP_CDEI;
					} else {
						ppecfg_log_error("Invalid cdei_xlate_cmd: %s. Valid: unchange, replace, cp_sdei, cp_cdei\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CDEI_XLT_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CDEI_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action_f.cdeitranslation);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.cdeitranslation > 1) {
						ppecfg_log_error("Invalid cdei_xlate=%u. Valid range is 0-1\n", nl_msg.rule.action_f.cdeitranslation);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CDEI_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_TAGS_TO_REMOVE].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action_f.tags_to_remove);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_TAGS_TO_REMOVE;

				}

				data = sub_params[PPECFG_VLAN_ACTION_STPID_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.stpid_cmd = PPE_VLAN_TPID_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "replace") == 0) {
						nl_msg.rule.action_f.stpid_cmd = PPE_VLAN_TPID_CMD_REPLACE;
					} else if (strcasecmp(buf, "cp_stpid") == 0) {
						nl_msg.rule.action_f.stpid_cmd = PPE_VLAN_TPID_CMD_CP_STPID;
					} else if (strcasecmp(buf, "cp_ctpid") == 0) {
						nl_msg.rule.action_f.stpid_cmd = PPE_VLAN_TPID_CMD_CP_CTPID;
					} else {
						ppecfg_log_error("Invalid stag_cmd: %s. Valid: unchange, replace, cp_stpid, cp_ctpid\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_STPID_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_STPID].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.action_f.stpid_action);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_STPID;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CTPID_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "unchange") == 0) {
						nl_msg.rule.action_f.ctpid_cmd = PPE_VLAN_TPID_CMD_UNCHANGE;
					} else if (strcasecmp(buf, "replace") == 0) {
						nl_msg.rule.action_f.ctpid_cmd = PPE_VLAN_TPID_CMD_REPLACE;
					} else if (strcasecmp(buf, "cp_stpid") == 0) {
						nl_msg.rule.action_f.ctpid_cmd = PPE_VLAN_TPID_CMD_CP_STPID;
					} else if (strcasecmp(buf, "cp_ctpid") == 0) {
						nl_msg.rule.action_f.ctpid_cmd = PPE_VLAN_TPID_CMD_CP_CTPID;
					} else {
						ppecfg_log_error("Invalid ctag_cmd: %s. Valid: unchange, replace, cp_stpid, cp_ctpid\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CTPID_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CTPID].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.action_f.ctpid_action);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CTPID;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CNTR_ID].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.rule.action_f.counter_id);
					if (error) {
						goto print_error;
					}

					if (nl_msg.rule.action_f.counter_id > 127) {
						ppecfg_log_error("Invalid counter_id=%u. Valid range is 0-127\n", nl_msg.rule.action_f.counter_id);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CNTR_ID;

				}

				data = sub_params[PPECFG_VLAN_ACTION_CNTR_MODE].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "vlan") == 0) {
						nl_msg.rule.action_f.counter_mode = PPE_VLAN_COUNTER_MODE_VLAN;
					} else if (strcasecmp(buf, "pon_pm") == 0) {
						nl_msg.rule.action_f.counter_mode = PPE_VLAN_COUNTER_MODE_PON_PM;
					} else {
						ppecfg_log_error("Invalid counter_mode: %s. Valid: vlan, pon_pm\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_CNTR_MODE;

				}

				data = sub_params[PPECFG_VLAN_ACTION_VSI_XLT_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.rule.action_f.vsitranslation);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_VSI_XLT_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SRC_INFO_TYP].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "vp") == 0) {
						nl_msg.rule.action_f.src_info_type = PPE_VLAN_SRC_INFO_TYPE_VP;
					} else if (strcasecmp(buf, "l3_if") == 0) {
						nl_msg.rule.action_f.src_info_type = PPE_VLAN_SRC_INFO_TYPE_L3_IF;
					} else {
						ppecfg_log_error("Invalid src_info_type: %s. Valid: vp, l3_if\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SRC_INFO_TYP;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SRC_INFO_VAL].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.action_f.src_info), &nl_msg.rule.action_f.src_info);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SRC_INFO_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_VNI_RESV_VAL].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.rule.action_f.vni_resv_action);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_VNI_RESV_VAL;

				}

				data = sub_params[PPECFG_VLAN_ACTION_FWD_CMD].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(buf), buf);
					if (error) {
						goto print_error;
					}

					if (strcasecmp(buf, "forward") == 0) {
						nl_msg.rule.action_f.fwd_cmd = PPE_VLAN_FWD_CMD_FORWARD;
					} else if (strcasecmp(buf, "drop") == 0) {
						nl_msg.rule.action_f.fwd_cmd = PPE_VLAN_FWD_CMD_DROP;
					} else if (strcasecmp(buf, "copy") == 0) {
						nl_msg.rule.action_f.fwd_cmd = PPE_VLAN_FWD_CMD_COPY;
					} else if (strcasecmp(buf, "redirect") == 0) {
						nl_msg.rule.action_f.fwd_cmd = PPE_VLAN_FWD_CMD_REDIRECT;
					} else {
						ppecfg_log_error("Invalid fwd_cmd: %s. Valid: forward, drop, copy, redirect\n", buf);
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_FWD_CMD;

				}

				data = sub_params[PPECFG_VLAN_ACTION_SERVICE_CODE].data;
				if (data) {
					error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action_f.sc);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_SVC_CODE;

				}

				data = sub_params[PPECFG_VLAN_ACTION_DEST_INFO].data;
				if (data) {
					error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.action_f.dest_info), nl_msg.rule.action_f.dest_info);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.action_f.action_flags |= PPE_VLAN_ACTION_FLAG_DEST_INFO;

				}

				break;
		}
	}

	if ((nl_msg.rule.rule_dir == PPE_VLAN_RULE_DIR_EGRESS) && (nl_msg.rule.rule_f.port_type == PPE_VLAN_PORT_TYPE_GEM_PORT)) {
		ppecfg_log_warn("GEM_PORT is not supported with downstream rule_dir\n");
		error = -EINVAL;
		goto done;
	}

	if ((field_mask & MANDATORY_VLAN_FIELDS_MASK) != MANDATORY_VLAN_FIELDS_MASK) {
		ppecfg_log_warn("Missing mandatory fields: [rule_id], [port_type], [port_info], [rule_dir]\n");
		error = -EINVAL;
		goto done;
	}

	if ((field_mask & OPTIONAL_VLAN_FIELDS_MASK) == 0) {
		ppecfg_log_warn("At least one optional field must be provided: \n"
				"[stagformat], [svid], [spcp], [sdei], [ctagformat], [cvid], [cpcp], [cdei], [vsi],\n"
				"[vni_resv_type], [vni_resv], [frame_type], [protocol], [stag], [ctag], [dhcp_type], [mc_type]\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_vlan_rule_add(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		return error;
	}

	return error;

print_error:
	ppecfg_log_data_error(sub_params);
done:
	return error;
}

/*
 * ppecfg_vlan_rule_del()
 *	Handle VLAN rule delete
 */
static int ppecfg_vlan_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_vlan_rule nl_msg = {{0}};
	int error;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_vlan_init_rule(&nl_msg, NSS_PPE_VLAN_DESTROY_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_VLAN_RULE_DEL_RULE_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_vlan_rule_del(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}

/*
 * ppe_vlan_rule_flush()
 *	Function to flush vlan rules
 */
static int ppecfg_vlan_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_vlan_rule nl_vlan_msg = {{0}};
	bool except_first = false;
	int error;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	/*
	 * Check for except_first in the input arguments
	 */
	for (int i = 0; i < match->total; i++) {
		if (strcmp(match->args[i], "except_first") == 0) {
			except_first = true;
			break;
		}
	}

	/*
	 * Only parse sub-params if optional arg is present
	 */
	if (except_first) {
		error = ppecfg_param_iter_tbl(param, match);
		if (error) {
			ppecfg_log_arg_error(param);
			return error;
		}
	}

	nss_ppenl_vlan_init_rule(&nl_vlan_msg, NSS_PPE_VLAN_FLUSH_RULE_MSG);
	nl_vlan_msg.rule.except_flag = except_first;

	error = nss_ppenl_vlan_rule_flush(&nl_vlan_msg);
	if (error) {
		ppecfg_log_warn("Flush vlan rule failed!\n");
		return error;
	}

	return 0;
}
