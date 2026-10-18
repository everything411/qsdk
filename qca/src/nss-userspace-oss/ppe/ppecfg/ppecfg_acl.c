/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_VAL, "cvid="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_MASK, "cvid_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_CVID_RANGE, "cvid_range_en="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param svid_params[PPECFG_ACL_SVID_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_SVID_TAG, "stag="),
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
	PPECFG_PARAM_INIT(PPECFG_ACL_ETHER_MIN, "l4_proto="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ETHER_MASK, "l4_proto_mask="),
	PPECFG_PARAM_INIT(PPECFG_ACL_ETHER_NVAL, "l4_proto!="),
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
};

/*
 * rule add parameters
 */
static struct ppecfg_param rule_add_params[PPECFG_ACL_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_RULE_ID, "rule_id="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_DEV, "src_dev="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_POST_ROUTE_EN, "post_route_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_FLOW_QOS_OVERRIDE, "flow_qos_override="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_PRIORITY, "priority="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_SRC_SC, "src_sc="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_OUTER_HEADER, "outer_header_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_METADATA, "metadata_en="),
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_ADD_GROUP, "group="),
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
	PPECFG_PARAMARR_INIT(PPECFG_ACL_RULE_ADD_ACTION, "action", action_params, ppecfg_param_iter_tbl),
};

/*
 * rule del parameters
 */
static struct ppecfg_param rule_del_params[PPECFG_ACL_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_ACL_RULE_DEL_RULE_ID, "rule_id="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_acl_cmd' should also get updated
 */
struct ppecfg_param ppecfg_acl_params[PPECFG_ACL_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", rule_add_params, ppecfg_acl_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", rule_del_params, ppecfg_acl_rule_del),
	PPECFG_PARAMFUNC_INIT("cmd=flush", ppecfg_acl_rule_flush),
};

/*
 * ppecfg_acl_rule_add()
 * 	handle ACL rule add
 */
static int ppecfg_acl_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_acl_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
	uint8_t is_v6;
	uint8_t mirror_en;
	bool bool_val = false;

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
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.rule.src.dev_name), &nl_msg.rule.src.dev_name);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (strcmp("flow" , nl_msg.rule.src.dev_name) == 0) {
				nl_msg.rule.stype = PPE_ACL_RULE_SRC_TYPE_FLOW;
			} else if (strcmp("sc" , nl_msg.rule.src.dev_name) == 0) {
				memset(nl_msg.rule.src.dev_name, 0, sizeof(nl_msg.rule.src.dev_name));
				nl_msg.rule.stype = PPE_ACL_RULE_SRC_TYPE_SC;
			} else {
				nl_msg.rule.stype = PPE_ACL_RULE_SRC_TYPE_DEV;
			}

			break;

		case PPECFG_ACL_RULE_ADD_SRC_SC:
			if (nl_msg.rule.stype != PPE_ACL_RULE_SRC_TYPE_SC) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.src.sc);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			printf("nl_msg.rule.src.sc: %d\n", nl_msg.rule.src.sc);
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

			printf("nl_msg.rule.cmn.group: %d\n", nl_msg.rule.cmn.group);
			nl_msg.rule.cmn.cmn_flags |= PPE_ACL_RULE_CMN_FLAG_GROUP_EN;
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
			}

			data = sub_params[PPECFG_ACL_CVID_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule.cvid.vid_mask_max);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_MASK;
			}

			data = sub_params[PPECFG_ACL_CVID_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(sub_params->data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags & PPE_ACL_RULE_FLAG_VID_MASK)) {
					goto print_error;
				}

				if (bool_val == true) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CVID].rule_flags &= ~PPE_ACL_RULE_FLAG_VID_MASK;
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
			}

			data = sub_params[PPECFG_ACL_SVID_MASK].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule.svid.vid_mask_max);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_VID_MASK;
			}

			data = sub_params[PPECFG_ACL_SVID_RANGE].data;
			if (data) {
				error = ppecfg_param_get_bool(sub_params->data, &bool_val);
				if (error < 0) {
					ppecfg_log_data_error(sub_params);
					goto done;
				}

				if (!(nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags & PPE_ACL_RULE_FLAG_VID_MASK)) {
					goto print_error;
				}

				if (bool_val == true) {
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags |= PPE_ACL_RULE_FLAG_SVID_RANGE;
					nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SVID].rule_flags &= ~PPE_ACL_RULE_FLAG_VID_MASK;
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

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_CPCP].rule_flags |= PPE_ACL_RULE_FLAG_PCP_MASK;
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

				nl_msg.rule.rules[PPE_ACL_RULE_MATCH_TYPE_SPCP].rule_flags |= PPE_ACL_RULE_FLAG_PCP_MASK;
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


		case PPECFG_ACL_RULE_ADD_ACTION:
			sub_params = param->sub_params[PPECFG_ACL_RULE_ADD_ACTION].sub_params;
			char fwd_cmd[10];

			data = sub_params[PPECFG_ACL_ACTION_FWD_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(fwd_cmd), &fwd_cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("FWD" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_FWD;
				} else if (strcmp("DROP" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_DROP;
				} else if (strcmp("COPY" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_COPY;
				} else if (strcmp("REDIR" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_ACL_FWD_CMD_REDIR;
				} else {
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
 * 	handle ACL rule delete
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
 * 	Function to flush all acl rules
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
