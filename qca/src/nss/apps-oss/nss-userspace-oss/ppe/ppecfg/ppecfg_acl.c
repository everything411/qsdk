/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG ACL handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_acl.h"

static int ppecfg_acl_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_acl_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_acl_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_acl_rule_prio_upd(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * Rule add parameters
 */
static struct ppecfg_param dev_params[PPECFG_ACL_DEV_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_DEV_NAME, "dev_name="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DEV_TYPE, "dev_type="),
};

/*
 * Rule add parameters
 */
static struct ppecfg_param smac_params[PPECFG_ACL_SMAC_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SMAC_VAL, "sval="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SMAC_NVAL, "sval!="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SMAC_MASK, "smask="),
};

/*
 * Rule add parameters
 */
static struct ppecfg_param dmac_params[PPECFG_ACL_DMAC_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_DMAC_VAL, "dval="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DMAC_NVAL, "dval!="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DMAC_MASK, "dmask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param cvid_params[PPECFG_ACL_CVID_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_TAGGED, "ctag="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_TAGGED_MASK, "ctag_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_VAL, "cvid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_MASK, "cvid_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_RANGE, "cvid_range_en="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param svid_params[PPECFG_ACL_SVID_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SVID_TAG, "stag="),
	 PPECFG_PARAM_INIT(PPECFG_ACL_SVID_TAG_MASK, "stag_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SVID_MIN, "svid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SVID_MASK, "svid_max_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SVID_RANGE, "svid_range_en="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param cpcp_params[PPECFG_ACL_CPCP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_CPCP_MIN, "cpcp="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CPCP_MASK, "cpcp_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param spcp_params[PPECFG_ACL_SPCP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SPCP_MIN, "spcp="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SPCP_MASK, "spcp_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param pppoe_params[PPECFG_ACL_PPPOE_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_PPPOE_VAL, "pppoe_sess="),
	PPECFG_PARAM_INIT(PPECFG_ACL_PPPOE_MASK, "pppoe_sess_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_PPPOE_NVAL, "pppoe_sess!="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param ether_params[PPECFG_ACL_ETHER_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_ETHER_MIN, "l2_proto="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ETHER_MASK, "l2_proto_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ETHER_NVAL, "l2_proto!="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param tos_tc_params[PPECFG_ACL_TOS_TC_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_TOS_TC_MIN, "tos_tc="),
	PPECFG_PARAM_INIT(PPECFG_ACL_TOS_TC_MASK, "tos_tc_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param sip_params[PPECFG_ACL_SIP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SIP_TYPE, "sip_is_v6="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SIP_VAL, "sip_val="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SIP_NVAL, "sip_val!="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SIP_MASK, "sip_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param dip_params[PPECFG_ACL_DIP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_DIP_TYPE, "dip_is_v6="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DIP_VAL, "dip_val="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DIP_NVAL, "dip_val!="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DIP_MASK, "dip_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param sport_params[PPECFG_ACL_SPORT_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SPORT_MIN, "sport_min="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SPORT_MASK, "sport_max="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SPORT_RANGE, "sport_range_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SPORT_NVAL, "sport_min!="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param dport_params[PPECFG_ACL_DPORT_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_DPORT_MIN, "dport_min="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DPORT_MASK, "dport_max="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DPORT_RANGE, "dport_range_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DPORT_NVAL, "dport_min!="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param ttl_params[PPECFG_ACL_TTL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_TTL_MIN, "ttl_limit="),
	PPECFG_PARAM_INIT(PPECFG_ACL_TTL_MASK, "ttl_limit_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param l3_len_param[PPECFG_ACL_L3_LEN_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_L3_LEN_MIN, "l3_len="),
	PPECFG_PARAM_INIT(PPECFG_ACL_L3_LEN_MASK, "l3_len_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_L3_LEN_RANGE, "l3_len_range_en="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param ctpid_params[PPECFG_ACL_CTPID_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_CTPID_VAL, "ctpid_val="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CTPID_MASK, "ctpid_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param stpid_params[PPECFG_ACL_STPID_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_STPID_VAL, "stpid_val="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CTPID_MASK, "stpid_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param dhcp_type_params[PPECFG_ACL_DHCP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_DHCP_TYPE, "dhcp_type="),
	PPECFG_PARAM_INIT(PPECFG_ACL_DHCP_TYPE_MASK, "dhcp_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param mc_type_params[PPECFG_ACL_MC_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_MC_TYPE, "mc_type="),
	PPECFG_PARAM_INIT(PPECFG_ACL_MC_TYPE_MASK, "mc_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param cdei_params[PPECFG_ACL_CDEI_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_CDEI_VAL, "cdei="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CDEI_MASK, "cdei_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param sdei_params[PPECFG_ACL_SDEI_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SDEI_VAL, "sdei="),
	PPECFG_PARAM_INIT(PPECFG_ACL_SDEI_MASK, "sdei_mask="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param action_params[PPECFG_ACL_ACTION_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_FWD_CMD, "fwd_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_SERVICE_CODE, "service_code="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_ENQUEUE_PRI, "enqueue_pri="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_QID, "qid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_PCP, "c_pcp="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_PCP, "s_pcp="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_TOS_TC, "tos-tc="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CVID, "c_vid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_SVID, "s_vid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_DEST, "dest_dev="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_REDIR_CORE, "redir_core="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_POLICER_ID, "policer_id="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_MIRROR_EN, "mirror_en="),
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_EXCEPTION_EDIT_EN, "exception_edit_en="),
#endif
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_PCP_CMD, "c_pcp_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_PCP_CMD, "s_pcp_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_VID_CMD, "c_vid_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_VID_CMD, "s_vid_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_PID_CMD, "c_tpid_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_PID, "c_tpid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_PID_CMD, "s_tpid_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_PID, "s_tpid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_DEI_CMD, "c_dei_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_CTAG_DEI, "c_dei="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_DEI_CMD, "s_dei_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_STAG_DEI, "s_dei="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_COUNTER_ID, "counter_id="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_COUNTER_MODE, "counter_mode="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_TAGS_TO_RMV_CMD, "tags_to_remove_cmd="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_INT_DP, "int_dp="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_SRC_INFO, "src_info="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ACTION_SRC_INFO_TYPE, "src_info_type="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param rule_add_params[PPECFG_ACL_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_RULE_ID, "rule_id="),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_DEV, "dev", dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_POST_ROUTE_EN, "post_route_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_FLOW_QOS_OVERRIDE, "flow_qos_override="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_PRIORITY, "priority="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_SRC_SC, "src_sc="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_OUTER_HEADER, "outer_header_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_METADATA, "metadata_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_GROUP, "group="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_L4_PROTO, "l4_proto="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_RULE_DIR, "flow_dir="),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_SMAC, "smac", smac_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_DMAC, "dmac", dmac_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_CVID, "cvid", cvid_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_SVID, "svid", svid_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_CPCP, "cpcp", cpcp_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_SPCP, "spcp", spcp_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_PPPOE, "pppoe", pppoe_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_ETHER, "ether_type", ether_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_SIP, "sip", sip_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_DIP, "dip", dip_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_SPORT, "sport", sport_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_DPORT, "dport", dport_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_TOS_TC, "tos_tc", tos_tc_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_TTL, "ttl_hop", ttl_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_L3_LEN, "l3_len", l3_len_param, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_CTPID, "ctpid", ctpid_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_STPID, "stpid", stpid_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_CDEI, "cdei", cdei_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_SDEI, "sdei", sdei_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_DHCP_TYPE, "dhcp_type", dhcp_type_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_MC_TYPE, "mc_type", mc_type_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_ACTION, "action", action_params, ppecfg_param_iter_tbl),
};

/*
 * rule del parameters
 */
static struct ppecfg_param rule_del_params[PPECFG_ACL_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_DEL_RULE_ID, "rule_id="),
};

/*
 * rule priority update parameters
 */
static struct ppecfg_param rule_prio_upd_params[PPECFG_ACL_RULE_UPDATE_PRI_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_UPDATE_PRI_RULE_ID, "rule_id="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_UPDATE_PRI_PRIORITY, "priority="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_acl_cmd' should also get updated
 */
struct ppecfg_param ppecfg_acl_params[PPECFG_ACL_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", rule_add_params, ppecfg_acl_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", rule_del_params, ppecfg_acl_rule_del),
	PPECFG_PARAMFUNC_INIT("cmd=flush", ppecfg_acl_rule_flush),
	PPECFG_PARAMLIST_INIT("cmd=rule_prio_update", rule_prio_upd_params, ppecfg_acl_rule_prio_upd),
};

/*
 * ppecfg_acl_rule_add()
 *	handle ACL rule add
 */
static int ppecfg_acl_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_acl_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
	uint8_t is_v6;
	uint8_t mirror_en;
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	uint8_t exception_edit_en;
#endif
	bool bool_val = false;
	char type[10];
	char cmd[20];

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

	nss_ppenl_acl_init_rule(&nl_msg, NSS_PPE_ACL_CREATE_RULE_MSG);

	for (int index = PPECFG_ACL_RULE_ADD_RULE_ID; index <= PPECFG_ACL_RULE_ADD_ACTION; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_ACL_RULE_ADD_RULE_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_ACL_RULE_ADD_DEV:
			char dev_type[16] = {0};

			/* Derive dev_type */
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_DEV].sub_params;
			data = sub_params[PPECFG_ACL_DEV_TYPE].data;
			error = ppecfg_param_get_str(data, sizeof(dev_type), &dev_type);
			if (error < 0) {
				nl_msg.rule.dev_type = PPE_ACL_RULE_DEV_TYPE_SRC_DEV;
				ppecfg_log_info("Take the default as src info\n");
			}

			if (strcmp(dev_type, "flow") == 0) {
				nl_msg.rule.dev_type = PPE_ACL_RULE_DEV_TYPE_FLOW;
			} else if (strcmp(dev_type, "sc") == 0) {
				nl_msg.rule.dev_type = PPE_ACL_RULE_DEV_TYPE_SC;
			} else if (strcmp(dev_type, "dst") == 0) {
				nl_msg.rule.dev_type = PPE_ACL_RULE_DEV_TYPE_DEST_L2_PORT;
			} else if (strcmp(dev_type, "dst_l3") == 0) {
				nl_msg.rule.dev_type = PPE_ACL_RULE_DEV_TYPE_DEST_L3_PORT;
			} else {
				nl_msg.rule.dev_type = PPE_ACL_RULE_DEV_TYPE_SRC_DEV;
				ppecfg_log_info("Valid Inputs type=[flow|sc|src|dst|dst_l3], name=[flow|sc_value|dev_name]\n");
			}

			data = sub_params[PPECFG_ACL_DEV_NAME].data;
			/* Derive SC and dev_name */
			if (strcmp(dev_type, "sc") == 0)
				error = ppecfg_param_get_int(data, sizeof(nl_msg.rule.dev.sc), &nl_msg.rule.dev.sc);
			else if (strcmp(dev_type, "flow") != 0)
				error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.dev.dev_name), &nl_msg.rule.dev.dev_name);

			if (error < 0) {
				ppecfg_log_warn("Missing required sc_val or dev_name\n");
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_ACL_RULE_ADD_SRC_SC:
			if (nl_msg.rule.dev_type != PPE_ACL_RULE_DEV_TYPE_SC) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.dev.sc);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			ppecfg_log_info("nl_msg.rule.src.sc: %d\n", nl_msg.rule.dev.sc);
			break;

		case PPECFG_ACL_RULE_ADD_POST_ROUTE_EN:
			error = ppecfg_param_get_bool(sub_params->data, &bool_val);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (bool_val == true) {
				nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_POST_RT_EN;
			}

			bool_val = false;
			break;

		case PPECFG_ACL_RULE_ADD_FLOW_QOS_OVERRIDE:
			error = ppecfg_param_get_bool(sub_params->data, &bool_val);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (bool_val == true) {
				nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_FLOW_QOS_OVERRIDE;
			}

			bool_val = false;
			break;

		case PPECFG_ACL_RULE_ADD_OUTER_HEADER:
			error = ppecfg_param_get_bool(sub_params->data, &bool_val);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (bool_val == true) {
				nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_OUTER_HDR_MATCH;
			}

			bool_val = false;
			break;

		case PPECFG_ACL_RULE_ADD_METADATA:
			error = ppecfg_param_get_bool(sub_params->data, &bool_val);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (bool_val == true) {
				nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_METADATA_EN;
			}

			bool_val = false;
			break;

		case PPECFG_ACL_RULE_ADD_PRIORITY:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.cmn.pri);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_PRI_EN;
			break;

		case PPECFG_ACL_RULE_ADD_GROUP:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.cmn.group);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			ppecfg_log_info("nl_msg.rule.cmn.group: %d\n", nl_msg.rule.cmn.group);
			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_GROUP_EN;
			break;

		case PPECFG_ACL_RULE_ADD_L4_PROTO:
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR_VALID;
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR].rule.proto_nexthdr.l3_v4proto_v6nexthdr);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_ACL_RULE_ADD_RULE_DIR:
			char flow_dir[5];

			error = ppecfg_param_get_str(sub_params->data, sizeof(flow_dir), &flow_dir);
			if (error) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (strcmp("us", flow_dir) == 0) {
				nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLOW_DIR_TYPE_US;
			} else if (strcmp("ds", flow_dir) == 0) {
				/*
				 * ACL rule shall be binded with destination info.
				 */
				if (nl_msg.rule.dev_type != PPE_ACL_RULE_DEV_TYPE_DEST_L2_PORT &&
						nl_msg.rule.dev_type != PPE_ACL_RULE_DEV_TYPE_DEST_L3_PORT) {
					printf("ACL rule shall be binded with dest_dev info\n");
					goto print_error;
				}
				nl_msg.rule.cmn.cmn_flags &= ~PPE_ACL_RULE_CMN_FLOW_DIR_TYPE_US;
			} else {
				ppecfg_log_warn("Valid Inputs: [us][ds]\n");
				goto print_error;
			}
			break;

		case PPECFG_ACL_RULE_ADD_SMAC:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_SMAC].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SMAC_VALID;

			data = sub_params[PPECFG_ACL_SMAC_VAL].data;
			error = ppecfg_param_verify_mac(data, nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac);
			if (!error && !(sub_params[PPECFG_ACL_SMAC_NVAL].data)) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			data = sub_params[PPECFG_ACL_SMAC_NVAL].data;
			if (data) {
				error = ppecfg_param_verify_mac(data, nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac);
				if (!error) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			}

			data = sub_params[PPECFG_ACL_SMAC_MASK].data;
			if (data) {
				error = ppecfg_param_verify_mac(data, nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule.smac.mac_mask);
				if (!error) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SMAC].rule_flags |= PPE_ACL_RULE_FLAG_MAC_MASK;
			}

			break;

		case PPECFG_ACL_RULE_ADD_DMAC:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_DMAC].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DMAC_VALID;

			data = sub_params[PPECFG_ACL_DMAC_VAL].data;
			error = ppecfg_param_verify_mac(data, nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac);
			if (!error && !(sub_params[PPECFG_ACL_DMAC_NVAL].data)) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			data = sub_params[PPECFG_ACL_DMAC_NVAL].data;
			if (data) {
				error = ppecfg_param_verify_mac(data, nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac);
				if (!error) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			}

			data = sub_params[PPECFG_ACL_DMAC_MASK].data;
			if (data) {
				error = ppecfg_param_verify_mac(data, nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule.dmac.mac_mask);
				if (!error) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DMAC].rule_flags |= PPE_ACL_RULE_FLAG_MAC_MASK;
			}
			break;

		case PPECFG_ACL_RULE_ADD_CVID:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_CVID].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_CVID_VALID;

			data = sub_params[PPECFG_ACL_CVID_VAL].data;
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.vid_min);
			if (error) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_CVID_TAGGED].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.tag_fmt);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.tag_fmt_mask = 0xff;
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_STAG_FMT;
			}

			data = sub_params[PPECFG_ACL_CVID_TAGGED_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.tag_fmt_mask);
				if (error) {
					goto print_error;
				}
			}

			data = sub_params[PPECFG_ACL_CVID_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.vid_mask_max);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags |= PPE_ACL_RULE_FLAG_CVID_MASK;
			}

			data = sub_params[PPECFG_ACL_CVID_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(sub_params->data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags & PPE_ACL_RULE_FLAG_CVID_MASK)) {
					goto print_error;
				}

				if (bool_val == true) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags |= PPE_ACL_RULE_FLAG_CVID_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags &= ~PPE_ACL_RULE_FLAG_CVID_MASK;
				}
			}

			bool_val = false;
			break;

		case PPECFG_ACL_RULE_ADD_SVID:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_SVID].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SVID_VALID;

			data = sub_params[PPECFG_ACL_SVID_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.vid_min);
			if (error) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_SVID_TAG].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.tag_fmt);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.tag_fmt_mask = 0xff;
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_STAG_FMT;
			}

			data = sub_params[PPECFG_ACL_SVID_TAG_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.tag_fmt_mask);
				if (error) {
					goto print_error;
				}
			}

			data = sub_params[PPECFG_ACL_SVID_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.vid_mask_max);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_SVID_MASK;
			}

			data = sub_params[PPECFG_ACL_SVID_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(sub_params->data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags & PPE_ACL_RULE_FLAG_SVID_MASK)) {
					goto print_error;
				}

				if (bool_val == true) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_SVID_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags &= ~PPE_ACL_RULE_FLAG_SVID_MASK;
				}

				bool_val = false;
			}
			break;

		case PPECFG_ACL_RULE_ADD_CPCP:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_CPCP].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_CPCP_VALID;

			data = sub_params[PPECFG_ACL_CPCP_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule.cpcp.pcp);
			if (error) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_CPCP_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule.cpcp.pcp_mask);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule_flags |= PPE_ACL_RULE_FLAG_CPCP_MASK;
			}
			break;

		case PPECFG_ACL_RULE_ADD_SPCP:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_SPCP].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SPCP_VALID;

			data = sub_params[PPECFG_ACL_SPCP_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule.spcp.pcp);
			if (error) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_SPCP_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule.spcp.pcp_mask);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule_flags |= PPE_ACL_RULE_FLAG_SPCP_MASK;
			}
			break;

		case PPECFG_ACL_RULE_ADD_PPPOE:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_PPPOE].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS_VALID;

			data = sub_params[PPECFG_ACL_PPPOE_VAL].data;
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule.pppoe_sess.pppoe_session_id);
			if (error && !(sub_params[PPECFG_ACL_PPPOE_NVAL].data)) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_PPPOE_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule.pppoe_sess.pppoe_session_id_mask);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule_flags |= PPE_ACL_RULE_FLAG_PPPOE_MASK;
			}

			data = sub_params[PPECFG_ACL_PPPOE_NVAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule.pppoe_sess.pppoe_session_id);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			}

			break;

		case PPECFG_ACL_RULE_ADD_ETHER:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_ETHER].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE_VALID;

			data = sub_params[PPECFG_ACL_ETHER_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto);
			if (error && !(sub_params[PPECFG_ACL_ETHER_NVAL].data)) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_ETHER_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto_mask);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule_flags |= PPE_ACL_RULE_FLAG_ETHTYPE_MASK;
			}

			data = sub_params[PPECFG_ACL_ETHER_NVAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule.ether_type.l2_proto);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			}

			break;

		case PPECFG_ACL_RULE_ADD_SIP:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_SIP].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SIP_VALID;

			data = sub_params[PPECFG_ACL_SIP_TYPE].data;
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &is_v6);
			if (error) {
				goto print_error;
			}

			if (is_v6 == 1) {
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_type = PPE_ACL_IP_TYPE_V6;
				ppecfg_log_trace("Ipv6 address : %pI6h\n", &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
			} else if (is_v6 == 0){
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_type = PPE_ACL_IP_TYPE_V4;
				ppecfg_log_trace("Ipv4 address :%pI4h\n", &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
			} else {
				ppecfg_log_trace("wrong ip address type \n");
				goto print_error;
			}

			if (is_v6 == 1) {
				data = sub_params[PPECFG_ACL_SIP_VAL].data;
				error = ppecfg_param_get_ipaddr(data, sizeof(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
				if (error && !(sub_params[PPECFG_ACL_SIP_NVAL].data)) {
					goto print_error;
				}

				data = sub_params[PPECFG_ACL_SIP_NVAL].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
				}

				data = sub_params[PPECFG_ACL_SIP_MASK].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_mask),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_mask);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_FLAG_SIP_MASK;
				}
			}

			if (is_v6 == 0) {
				data = sub_params[PPECFG_ACL_SIP_VAL].data;
				error = ppecfg_param_get_ipaddr(data, sizeof(uint32_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip[0]);
				if (error) {
					goto print_error;
				}

				data = sub_params[PPECFG_ACL_SIP_NVAL].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(uint32_t),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip[0]);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
				}

				data = sub_params[PPECFG_ACL_SIP_MASK].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(uint32_t),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule.sip.ip_mask[0]);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SIP].rule_flags |= PPE_ACL_RULE_FLAG_SIP_MASK;
				}
			}
			break;

		case PPECFG_ACL_RULE_ADD_DIP:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_DIP].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DIP_VALID;

			data = sub_params[PPECFG_ACL_DIP_TYPE].data;
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &is_v6);
			if (error) {
				goto print_error;
			}

			if (is_v6 == 1) {
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_type = PPE_ACL_IP_TYPE_V6;
			} else if (is_v6 == 0){
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_type = PPE_ACL_IP_TYPE_V4;
			} else {
				goto print_error;
			}

			if (is_v6 == 1) {
				data = sub_params[PPECFG_ACL_DIP_VAL].data;
				error = ppecfg_param_get_ipaddr(data, sizeof(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip);
				if (error && !(sub_params[PPECFG_ACL_DIP_NVAL].data)) {
					goto print_error;
				}

				data = sub_params[PPECFG_ACL_DIP_NVAL].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
				}

				data = sub_params[PPECFG_ACL_DIP_MASK].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_mask),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_mask);
					if (error) {
						goto print_error;
					}
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_FLAG_DIP_MASK;
				}
			}

			if (is_v6 == 0) {
				data = sub_params[PPECFG_ACL_DIP_VAL].data;
				error = ppecfg_param_get_ipaddr(data, sizeof(uint32_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip[0]);
				if (error && !(sub_params[PPECFG_ACL_DIP_NVAL].data)) {
					goto print_error;
				}

				data = sub_params[PPECFG_ACL_DIP_NVAL].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(uint32_t),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip[0]);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
				}

				data = sub_params[PPECFG_ACL_DIP_MASK].data;
				if (data) {
					error = ppecfg_param_get_ipaddr(data, sizeof(uint32_t),
							&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule.dip.ip_mask[0]);
					if (error) {
						goto print_error;
					}

					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DIP].rule_flags |= PPE_ACL_RULE_FLAG_DIP_MASK;
				}
			}
			break;

		case PPECFG_ACL_RULE_ADD_SPORT:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_SPORT].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SPORT_VALID;

			uint16_t sport_val;
			data = sub_params[PPECFG_ACL_SPORT_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &sport_val);
			if (error && !(sub_params[PPECFG_ACL_SPORT_NVAL].data)) {
				goto print_error;
			}

			nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule.sport.l4_port_min = ntohs(sport_val);

			data = sub_params[PPECFG_ACL_SPORT_NVAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &sport_val);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule.sport.l4_port_min = ntohs(sport_val);
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			}

			data = sub_params[PPECFG_ACL_SPORT_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &sport_val);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule.sport.l4_port_max_mask = ntohs(sport_val);
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags |= PPE_ACL_RULE_FLAG_SPORT_MASK;
			}

			data = sub_params[PPECFG_ACL_SPORT_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(sub_params->data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if(!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags & PPE_ACL_RULE_FLAG_SPORT_MASK)) {
					goto print_error;
				}

				if (bool_val == true) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags |= PPE_ACL_RULE_FLAG_SPORT_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPORT].rule_flags &= ~PPE_ACL_RULE_FLAG_SPORT_MASK;
				}

				bool_val = false;
			}
			break;

		case PPECFG_ACL_RULE_ADD_DPORT:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_DPORT].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DPORT_VALID;

			uint16_t dport_val;
			data = sub_params[PPECFG_ACL_DPORT_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint16_t), &dport_val);
			if (error && !(sub_params[PPECFG_ACL_DPORT_NVAL].data)) {
				goto print_error;
			}

			nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule.dport.l4_port_min = ntohs(dport_val);

			data = sub_params[PPECFG_ACL_DPORT_NVAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &dport_val);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule.dport.l4_port_min = ntohs(dport_val);
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags |= PPE_ACL_RULE_GEN_FLAG_INVERSE_EN;
			}

			data = sub_params[PPECFG_ACL_DPORT_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &dport_val);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule.dport.l4_port_max_mask = ntohs(dport_val);
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags |= PPE_ACL_RULE_FLAG_DPORT_MASK;
			}

			data = sub_params[PPECFG_ACL_DPORT_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(sub_params->data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags & PPE_ACL_RULE_FLAG_DPORT_MASK)) {
					goto print_error;
				}

				if (bool_val == true) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags |= PPE_ACL_RULE_FLAG_DPORT_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DPORT].rule_flags &= ~PPE_ACL_RULE_FLAG_DPORT_MASK;
				}

				bool_val = false;
			}
			break;

		case PPECFG_ACL_RULE_ADD_TOS_TC:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_TOS_TC].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_TOS_TC_VALID;

			data = sub_params[PPECFG_ACL_TOS_TC_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule.tos_tc.l3_tos_tc);
			if (error) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_TOS_TC_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule.tos_tc.l3_tos_tc_mask);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule_flags |= PPE_ACL_RULE_FLAG_TOS_TC_MASK;
			}
			break;

		case PPECFG_ACL_RULE_ADD_TTL:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_TTL].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT_VALID;

			data = sub_params[PPECFG_ACL_TTL_MIN].data;
			error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT].rule.ttl_hop.hop_limit);
			if (error) {
				goto print_error;
			}

			data = sub_params[PPECFG_ACL_TTL_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT].rule.ttl_hop.hop_limit_mask);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT].rule_flags |= PPE_ACL_RULE_FLAG_TTL_HOPLIMIT_MASK;
			}
			break;

		case PPECFG_ACL_RULE_ADD_L3_LEN:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_L3_LEN].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_IP_LEN_VALID;

			data = sub_params[PPECFG_ACL_L3_LEN_MIN].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule.l3_len.l3_length_min);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			data = sub_params[PPECFG_ACL_L3_LEN_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule.l3_len.l3_length_mask_max);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags |=
					PPE_ACL_RULE_FLAG_IPLEN_MASK;
			}

			data = sub_params[PPECFG_ACL_L3_LEN_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
				if (bool_val) {
					if (!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags &
								PPE_ACL_RULE_FLAG_IPLEN_MASK)) {
						ppecfg_log_data_error(sub_params);
						goto done;
					}
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags |=
						PPE_ACL_RULE_FLAG_IPLEN_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_IP_LEN].rule_flags &= ~PPE_ACL_RULE_FLAG_IPLEN_MASK;
				}
			}

			bool_val = false;
			break;

		case PPECFG_ACL_RULE_ADD_CTPID:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_CTPID].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_CTPID_VALID;

			data = sub_params[PPECFG_ACL_CTPID_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CTPID].rule.ctpid.tpid_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			data = sub_params[PPECFG_ACL_CTPID_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CTPID].rule.ctpid.tpid_mask);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CTPID].rule_flags |= PPE_ACL_RULE_FLAG_CTPID_EN;
			}

			break;

		case PPECFG_ACL_RULE_ADD_STPID:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_STPID].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_STPID_VALID;

			data = sub_params[PPECFG_ACL_STPID_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_STPID].rule.stpid.tpid_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			data = sub_params[PPECFG_ACL_STPID_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_STPID].rule.stpid.tpid_mask);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_STPID].rule_flags |= PPE_ACL_RULE_FLAG_STPID_EN;
			}

			break;

		case PPECFG_ACL_RULE_ADD_CDEI:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_CDEI].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_CDEI_VALID;

			data = sub_params[PPECFG_ACL_CDEI_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CDEI].rule.cdei.dei);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			data = sub_params[PPECFG_ACL_CDEI_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CDEI].rule.cdei.dei_mask);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CDEI].rule_flags |= PPE_ACL_RULE_FLAG_CDEI_EN;
			}

			break;

		case PPECFG_ACL_RULE_ADD_SDEI:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_SDEI].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_SDEI_VALID;

			data = sub_params[PPECFG_ACL_SDEI_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SDEI].rule.sdei.dei);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			data = sub_params[PPECFG_ACL_SDEI_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SDEI].rule.sdei.dei_mask);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SDEI].rule_flags |= PPE_ACL_RULE_FLAG_SDEI_EN;
			}

			break;

		case PPECFG_ACL_RULE_ADD_DHCP_TYPE:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_DHCP_TYPE].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE_VALID;

			data = sub_params[PPECFG_ACL_DHCP_TYPE].data;

			if (data) {
				error = ppecfg_param_get_str(data, sizeof(type), &type);
				if (error) {
					goto print_error;
				}

				if (strcmp("no_dhcp", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_NON;
				} else if (strcmp("v4", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_V4;
				} else if (strcmp("no_dhcp_or_v4", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_V4_NON;
				} else if (strcmp("v6", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_V6;
				} else if (strcmp("no_dhcpv6", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_V6_NON;
				} else if (strcmp("v4_or_v6", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_V6_V4;
				} else if (strcmp("any", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_type = PPE_ACL_RULE_DHCP_TYPE_ALL;
				} else {
					ppecfg_log_info("Valid Inputs:\n [no_dhcp][v4][no_dhcp_or_v4][v6][no_dhcp_or_v6][v4_or_v6][any]\n");
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_mask = 0xff;
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule_flags |= PPE_ACL_RULE_FLAG_DHCP_TYPE_EN;
			}

			data = sub_params[PPECFG_ACL_DHCP_TYPE_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE].rule.dhcp_type.dhcp_mask);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			break;

		case PPECFG_ACL_RULE_ADD_MC_TYPE:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_MC_TYPE].sub_params;
			nl_msg.rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_MC_TYPE_VALID;

			data = sub_params[PPECFG_ACL_MC_TYPE].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(type), &type);
				if (error) {
					goto print_error;
				}

				if (strcmp("no_mc", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_NON;
				} else if (strcmp("ip", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_IP;
				} else if (strcmp("no_mc_or_ip", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_IP_NON;
				} else if (strcmp("nonip", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_NONIP;
				} else if (strcmp("no_mc_or_nonip", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_NONIP_NON;
				} else if (strcmp("ip_or_nonip", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_IP_NONIP;
				} else if (strcmp("any", type) == 0) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_type = PPE_ACL_RULE_MC_TYPE_ALL;
				} else {
					ppecfg_log_info("[no_mc][ip][no_mc_or_ip][nonip][no_mc_or_nonip][ip_or_nonip][any]\n");
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_mask = 0xff;
				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule_flags |= PPE_ACL_RULE_FLAG_MC_TYPE_EN;
			}

			data = sub_params[PPECFG_ACL_MC_TYPE_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t),
						&nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_MC_TYPE].rule.mc_type.mc_mask);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
			}

			break;

		case PPECFG_ACL_RULE_ADD_ACTION:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_ACTION].sub_params;
			char fwd_cmd[10];

			data = sub_params[PPECFG_ACL_ACTION_FWD_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(fwd_cmd), &fwd_cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("FWD", fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_FWD;
				} else if (strcmp("DROP", fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_DROP;
				} else if (strcmp("COPY", fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_COPY;
				} else if (strcmp("REDIR", fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_REDIR;
				} else {
					ppecfg_log_info("Valid Inputs: [FWD][DROP][COPY][REDIR]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_FW_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_SERVICE_CODE].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.service_code);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_SERVICE_CODE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_ENQUEUE_PRI].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.enqueue_pri);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_ENQUEUE_PRI_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_QID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.qid);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_QID_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_CTAG_PCP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.ctag_pcp);
				if (data && error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CHANGE_EN;

			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_PCP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.stag_pcp);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_TOS_TC].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.tos_tc);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_TOS_TC_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_CVID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.action.cvid);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_CVID_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_SVID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.action.svid);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_SVID_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_DEST].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.action.dst.dev_name), &nl_msg.rule.action.dst.dev_name);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DEST_INFO_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_REDIR_CORE].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.redir_core);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_REDIR_TO_CORE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_POLICER_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.action.policer_id);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_POLICER_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_MIRROR_EN].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &mirror_en);
				if (error) {
					goto print_error;
				}
				if (mirror_en == 1) {
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_MIRROR_EN;
				}
			}

#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
			data = sub_params[PPECFG_ACL_ACTION_EXCEPTION_EDIT_EN].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &exception_edit_en);
				if (error) {
					goto print_error;
				}
				if (exception_edit_en == 1) {
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_EXCEPTION_EDIT_EN;
				}
			}
#endif

			data = sub_params[PPECFG_ACL_ACTION_CTAG_PCP_CMD].data;

			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_REPLACE;
				} else if (strcmp("CPY_SPCP", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_CPY_FRM_ORIG_SPCP;
				} else if (strcmp("CPY_CPCP", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_CPY_FRM_ORIG_CPCP;
				} else if (strcmp("DSCP2PBIT_0", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 0;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_DSCP_TO_PBIT;
				}else if (strcmp("DSCP2PBIT_1", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 1;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_DSCP_TO_PBIT;
				} else if (strcmp("TAG_REPLACE", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_REPLACE_PCP;
				} else if (strcmp("TAG_CPY_SPCP", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_SPCP;
				} else if (strcmp("TAG_CPY_CPCP", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_CPCP;
				} else if (strcmp("TAG_DSCP2PBIT_0", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 0;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_DSCP_TO_PBIT;
				} else if (strcmp("TAG_DSCP2PBIT_1", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 1;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_DSCP_TO_PBIT;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.ctag_pcp_cmd = PPE_ACL_PCP_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SPCP][CPY_CPCP][DSCP2PBIT_0][]DSCP2PBIT_1[TAG_REPLACE]\n");
					ppecfg_log_info("[TAG_CPY_SPCP][TAG_CPY_CPCP][TAG_DSCP2PBIT_0][TAG_DSCP2PBIT_1][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_PCP_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_REPLACE;
				} else if (strcmp("CPY_SPCP", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_CPY_FRM_ORIG_SPCP;
				} else if (strcmp("CPY_CPCP", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_CPY_FRM_ORIG_CPCP;
				} else if (strcmp("DSCP2PBIT_0", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 0;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_DSCP_TO_PBIT;
				} else if (strcmp("DSCP2PBIT_1", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 1;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_DSCP_TO_PBIT;
				} else if (strcmp("TAG_REPLACE", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_REPLACE_PCP;
				} else if (strcmp("TAG_CPY_SPCP", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_SPCP;
				} else if (strcmp("TAG_CPY_CPCP", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_CPY_FRM_ORIG_CPCP;
				} else if (strcmp("TAG_DSCP2PBIT_0", cmd) == 0) {
					nl_msg.rule.action.dscp_pbit_map_idx = 0;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_DSCP_TO_PBIT;
				} else if (strcmp("TAG_DSCP2PBIT_1", cmd) == 1) {
					nl_msg.rule.action.dscp_pbit_map_idx = 1;
					nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX;
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_ADD_TAG_DSCP_TO_PBIT;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.stag_pcp_cmd = PPE_ACL_PCP_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SPCP][CPY_CPCP][DSCP2PBIT_0][DSCP2PBIT_1][TAG_REPLACE]\n");
					ppecfg_log_info("[TAG_CPY_SPCP][TAG_CPY_CPCP][TAG_DSCP2PBIT_0][TAG_DSCP2PBIT_1][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_CTAG_VID_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.ctag_vid_cmd = PPE_ACL_VID_REPLACE;
				} else if (strcmp("CPY_SVID", cmd) == 0) {
					nl_msg.rule.action.ctag_vid_cmd = PPE_ACL_VID_CPY_FRM_ORIG_SVID;
				} else if (strcmp("CPY_CVID", cmd) == 0) {
					nl_msg.rule.action.ctag_vid_cmd = PPE_ACL_VID_CPY_FRM_ORIG_CVID;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.ctag_vid_cmd = PPE_ACL_VID_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SVID][CPY_CVID][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_CTAG_VID_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_VID_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.stag_vid_cmd = PPE_ACL_VID_REPLACE;
				} else if (strcmp("CPY_SVID", cmd) == 0) {
					nl_msg.rule.action.stag_vid_cmd = PPE_ACL_VID_CPY_FRM_ORIG_SVID;
				} else if (strcmp("CPY_CVID", cmd) == 0) {
					nl_msg.rule.action.stag_vid_cmd = PPE_ACL_VID_CPY_FRM_ORIG_CVID;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.stag_vid_cmd = PPE_ACL_VID_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SVID][CPY_CVID][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_STAG_VID_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_CTAG_PID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.ctag_pid);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_CTAG_PID_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_CTAG_PID_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_REPLACE;
				} else if (strcmp("CPY_SPID", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_CPY_FRM_ORIG_STPID;
				} else if (strcmp("CPY_CPID", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_CPY_FRM_ORIG_CTPID;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SPID][CPY_CPID][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_CTAG_PID_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_PID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.stag_pid);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_STAG_PID_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_PID_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_REPLACE;
				} else if (strcmp("CPY_SPID", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_CPY_FRM_ORIG_STPID;
				} else if (strcmp("CPY_CPID", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_CPY_FRM_ORIG_CTPID;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.ctag_pid_cmd = PPE_ACL_PID_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SPID][CPY_CPID][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_STAG_PID_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_CTAG_DEI].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.ctag_dei);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_CTAG_DEI_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_CTAG_DEI_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.ctag_dei_cmd = PPE_ACL_DEI_REPLACE;
				} else if (strcmp("CPY_SDEI", cmd) == 0) {
					nl_msg.rule.action.ctag_dei_cmd = PPE_ACL_DEI_CPY_FRM_ORIG_SDEI;
				} else if (strcmp("CPY_CDEI", cmd) == 0) {
					nl_msg.rule.action.ctag_dei_cmd = PPE_ACL_DEI_CPY_FRM_ORIG_CDEI;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.ctag_dei_cmd = PPE_ACL_DEI_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SDEI][CPY_CDEI][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_CTAG_DEI_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_DEI].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.stag_dei);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_STAG_DEI_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_STAG_DEI_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("REPLACE", cmd) == 0) {
					nl_msg.rule.action.stag_dei_cmd = PPE_ACL_DEI_REPLACE;
				} else if (strcmp("CPY_SDEI", cmd) == 0) {
					nl_msg.rule.action.stag_dei_cmd = PPE_ACL_DEI_CPY_FRM_ORIG_SDEI;
				} else if (strcmp("CPY_CDEI", cmd) == 0) {
					nl_msg.rule.action.stag_dei_cmd = PPE_ACL_DEI_CPY_FRM_ORIG_CDEI;
				} else if (strcmp("NONE", cmd) == 0) {
					nl_msg.rule.action.stag_dei_cmd = PPE_ACL_DEI_UNCHANGED;
				} else {
					ppecfg_log_info("Valid Inputs: [REPLACE][CPY_SDEI][CPY_CDEI][NONE]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags_ext |= PPE_ACL_RULE_ACTION_FLAG_STAG_DEI_CMD;
			}

			data = sub_params[PPECFG_ACL_ACTION_COUNTER_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.counter_id);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_COUNTER_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_COUNTER_MODE].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("PON", cmd) == 0) {
					nl_msg.rule.action.counter_mode = PPE_ACL_PON_PM;
				} else if (strcmp("VLAN", cmd) == 0){
					nl_msg.rule.action.counter_mode = PPE_ACL_VLAN_DEV;
				} else {
					ppecfg_log_info("Valid Inputs: [PON][VLAN]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_COUNTER_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_TAGS_TO_RMV_CMD].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.tags_to_rmv);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_TAGS_TO_RMV_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_INT_DP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.int_dp);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_INT_DP_CHANGE_EN;
			}

			data = sub_params[PPECFG_ACL_ACTION_SRC_INFO].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.src_info);
				if (error) {
					goto print_error;
				}
				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_SRC_INFO;
			}

			data = sub_params[PPECFG_ACL_ACTION_SRC_INFO_TYPE].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(cmd), &cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("PON_L3_IF", cmd) == 0) {
					nl_msg.rule.action.src_info_type = PPE_ACL_SRC_INFO_TYPE_L3_IF;
				} else if (strcmp("PON_VP", cmd) == 0) {
					nl_msg.rule.action.src_info_type = PPE_ACL_SRC_INFO_TYPE_VP;
				} else {
					ppecfg_log_info("Valid Inputs: [PON_L3_IF][PON_VP]\n");
					goto print_error;
				}

				nl_msg.rule.action.flags |= PPE_ACL_RULE_ACTION_FLAG_SRC_INFO;
			}
			break;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_acl_rule_add(&nl_msg);
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
 * ppecfg_acl_rule_del()
 *	handle ACL rule delete
 */
static int ppecfg_acl_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_acl_rule nl_msg = {{0}};
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

	nss_ppenl_acl_init_rule(&nl_msg, NSS_PPE_ACL_DESTROY_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_ACL_RULE_DEL_RULE_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_acl_rule_del(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_acl_rule_flush()
 *	Function to flush all acl rules
 */
static int ppecfg_acl_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_acl_rule nl_acl_msg = {{0}};
	int error;

	nss_ppenl_acl_init_rule(&nl_acl_msg, NSS_PPE_ACL_FLUSH_RULE_MSG);

	error = nss_ppenl_acl_rule_flush(&nl_acl_msg);
	if (error) {
		ppecfg_log_warn("Flush policer rule failed!\n");
		return error;
	}

	return error;
}

/*
 * ppecfg_acl_rule_prio_upd()
 *	Function to update priority of acl rules
 */
static int ppecfg_acl_rule_prio_upd(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_acl_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	bool rule_id_set = false;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_acl_init_rule(&nl_msg, NSS_PPE_ACL_UPDATE_PRI_RULE_MSG);

	for (int index = PPECFG_ACL_RULE_UPDATE_PRI_RULE_ID; index < PPECFG_ACL_RULE_UPDATE_PRI_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_ACL_RULE_UPDATE_PRI_RULE_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			rule_id_set = true;
			break;

		case PPECFG_ACL_RULE_UPDATE_PRI_PRIORITY:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.cmn.pri);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_PRI_EN;
			break;
		}
	}

	if (!rule_id_set || !(nl_msg.rule.cmn.cmn_flags & PPE_ACL_RULE_CMN_FLAG_PRI_EN)) {
		ppecfg_log_warn("Required both Rule ID and priority value\n");
		goto done;
	}

	error = nss_ppenl_acl_rule_prio_upd(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}
