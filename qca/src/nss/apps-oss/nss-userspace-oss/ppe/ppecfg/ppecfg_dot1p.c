/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG DOT1P handler
 */

#include <nss_ppenl_base.h>

#include "ppecfg_hlos.h"
#include "ppecfg_param.h"
#include "ppecfg_dot1p.h"
#include "nss_ppenl_dot1p_if.h"

#define PPECFG_DOT1P_MANDATORY_FIELDS_MASK 0x3F
#define PPECFG_DOT1P_MANDATORY_POLICER_ID_MASK 0x3
#define PPECFG_DOT1P_MANDATORY_DEFAULT_FIELDS_MASK 0x3
#define PPECFG_DOT1P_IS_NEGATIVE(val) ((val) < 0)

static int ppecfg_dot1p_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_rule_add_default(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_rule_pause(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_rule_resume(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_rule_get_pause_state(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_policer_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_policer_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_dot1p_policer_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * rule action parameters DOT1P
 */
static struct ppecfg_param dot1p_action_params[PPECFG_DOT1P_ACTION_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_GEM_PORT_ID, "gem_port="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_PQ, "pq="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_BASE_PQ, "base_pq="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_SC, "service_code="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_INT_DP, "int_dp="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_DST_INFO, "dst_action_info="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ACTION_FWD_CMD, "fwd_cmd="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param dot1p_rule_add_params[PPECFG_DOT1P_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_RULE_ID, "rule_id="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_SRC_PORT_TYPE, "src_port_type="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_SRC_INFO, "src_info="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_DST_PORT_TYPE, "dst_port_type="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_DST_INFO, "dst_info="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_VID, "vid="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_PCP, "pcp="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_DEI, "dei="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_DSCP, "dscp="),
	PPECFG_PARAMARR_INIT(PPECFG_DOT1P_RULE_ADD_ACTION, "action", dot1p_action_params, ppecfg_param_iter_tbl),
};

/*
 * rule policer id action parameters DOT1P
 */
static struct ppecfg_param dot1p_policer_action_params[PPECFG_DOT1P_POLICER_ACTION_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_POLICER_ACTION_US_POLICER_ID, "us_policer_id="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_POLICER_ACTION_US_POLICER_EN, "us_policer_id_en="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_POLICER_ACTION_DS_POLICER_EN, "ds_policer_id_en="),
};

/*
 * rule add policer_id parameters
 */
static struct ppecfg_param dot1p_rule_policer_add_params[PPECFG_DOT1P_RULE_ADD_POLICER_ID_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_ADD_GEMPORT_ID, "gemport="),
	PPECFG_PARAMARR_INIT(PPECFG_DOT1P_RULE_ADD_POLICER_ACTION, "action", dot1p_policer_action_params, ppecfg_param_iter_tbl),
};

/*
 * rule del parameters
 */
static struct ppecfg_param dot1p_del_params[PPECFG_DOT1P_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_DEL_RULE_ID, "rule_id="),
};

/*
 * rule flush parameters
 */
static struct ppecfg_param dot1p_flush_params[PPECFG_DOT1P_RULE_FLUSH_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_FLUSH_EX_FIRST, "except_first"),
};

/*
 * rule pause parameters
 */
static struct ppecfg_param dot1p_pause_params[PPECFG_DOT1P_RULE_PAUSE_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_RULE_PAUSE_EX_GEM0, "except_gemport_0"),
};

/*
 * policer rule del parameters
 */
static struct ppecfg_param dot1p_policer_del_params[PPECFG_DOT1P_POLICER_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_POLICER_RULE_DEL_GEMPORT_ID, "gemport_id="),
};

/*
 * rule add default parameters
 */
static struct ppecfg_param dot1p_add_def_params[PPECFG_DOT1P_ADD_DEFAULT_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ADD_DEFAULT_VID, "vid="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ADD_DEFAULT_PCP, "pcp="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ADD_DEFAULT_DEI, "dei="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ADD_DEFAULT_DSCP, "dscp="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ADD_DEFAULT_DSCP_MASK, "dscp_mask="),
	PPECFG_PARAM_INIT(PPECFG_DOT1P_ADD_DEFAULT_GEN_MISS_CMD, "gen_miss_cmd="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_dot1p_cmd' should also get updated
 */
struct ppecfg_param ppecfg_dot1p_params[PPECFG_DOT1P_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", dot1p_rule_add_params, ppecfg_dot1p_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", dot1p_del_params, ppecfg_dot1p_rule_del),
	PPECFG_PARAMLIST_INIT("cmd=rule_flush", dot1p_flush_params, ppecfg_dot1p_rule_flush),
	PPECFG_PARAMLIST_INIT("cmd=add_def", dot1p_add_def_params, ppecfg_dot1p_rule_add_default),
	PPECFG_PARAMLIST_INIT("cmd=rule_pause", dot1p_pause_params, ppecfg_dot1p_rule_pause),
	PPECFG_PARAMFUNC_INIT("cmd=rule_resume", ppecfg_dot1p_rule_resume),
	PPECFG_PARAMFUNC_INIT("cmd=rule_get_pause_state", ppecfg_dot1p_rule_get_pause_state),
	PPECFG_PARAMLIST_INIT("cmd=add_policer_id", dot1p_rule_policer_add_params, ppecfg_dot1p_policer_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=policer_rule_del", dot1p_policer_del_params, ppecfg_dot1p_policer_rule_del),
	PPECFG_PARAMFUNC_INIT("cmd=policer_rule_flush", ppecfg_dot1p_policer_rule_flush),
};

/*
 * ppecfg_dot1p_rule_add()
 * 	handle DOT1P rule add
 */
static int ppecfg_dot1p_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
	int mask = 0;
	bool is_pq_or_base_pq_set = false;
	char buf[32];

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_dot1p_init_rule(&nl_msg, NSS_PPE_DOT1P_CREATE_RULE_MSG);

	for (int index = PPECFG_DOT1P_RULE_ADD_RULE_ID; index < PPECFG_DOT1P_RULE_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_DOT1P_RULE_ADD_RULE_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.rule.rule_id);
			if (error < 0) {
				goto print_error;
			}
			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.rule_id)) || (nl_msg.rule.rule_id > PPE_DOT1P_MAX_RULE_ID)) {
				ppecfg_log_warn("Rule id: %d Please specify rule id between [0 - %d]\n",
								nl_msg.rule.rule_id, (PPE_DOT1P_MAX_RULE_ID));
				error = -EINVAL;
				goto done;
			}
			mask |= 0x01;
			nl_msg.rule.valid_flags |= PPE_DOT1P_RULE_FLAG_RULE_ID;
			break;

		case PPECFG_DOT1P_RULE_ADD_SRC_PORT_TYPE:
			error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
			if (error < 0) {
				goto print_error;
			}
			if (strcasecmp(buf, "BITMAP") == 0) {
				nl_msg.rule.rule.src_port_type = PPE_DOT1P_PORT_TYPE_BITMAP;
			}
			if (strcasecmp(buf, "PORT") == 0) {
				nl_msg.rule.rule.src_port_type = PPE_DOT1P_PORT_TYPE_PORT;
			}
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_SRC_PORT_TYPE;
			break;

		case PPECFG_DOT1P_RULE_ADD_SRC_INFO:
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.rule.rule.src_info), &nl_msg.rule.rule.src_info);
			if (error < 0) {
				goto print_error;
			}
			mask |= 0x02;
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_SRC_INFO;
			break;

		case PPECFG_DOT1P_RULE_ADD_DST_PORT_TYPE:
			error = ppecfg_param_get_str(sub_params->data, sizeof(buf), buf);
			if (error < 0) {
				goto print_error;
			}
			if (strcasecmp(buf, "BITMAP") == 0) {
				nl_msg.rule.rule.dst_port_type = PPE_DOT1P_PORT_TYPE_BITMAP;
			}
			if (strcasecmp(buf, "PORT") == 0) {
				nl_msg.rule.rule.dst_port_type = PPE_DOT1P_PORT_TYPE_PORT;
			}
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_DST_PORT_TYPE;
			break;

		case PPECFG_DOT1P_RULE_ADD_DST_INFO:
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.rule.rule.dst_info), &nl_msg.rule.rule.dst_info);
			if (error < 0) {
				goto print_error;
			}
			mask |= 0x02;
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_DST_INFO;
			break;

		case PPECFG_DOT1P_RULE_ADD_VID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.rule.vid);
			if (error < 0) {
				goto print_error;
			}
			if (PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.rule.vid)) {
				ppecfg_log_warn("Please specify a positive 12 bit value\n");
				error = -EINVAL;
				goto done;
			}
			mask |= 0x02;
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_VID;
			break;

		case PPECFG_DOT1P_RULE_ADD_PCP:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.rule.pcp);
			if (error < 0) {
				goto print_error;
			}
			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.rule.pcp)) || (nl_msg.rule.rule.pcp > PPE_DOT1P_MAX_PCP_VAL)) {
				ppecfg_log_warn("PCP value: %d Maximum possible PCP value %d",
									nl_msg.rule.rule.pcp, PPE_DOT1P_MAX_PCP_VAL);
				error = -EINVAL;
				goto done;
			}
			mask |= 0x02;
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_PCP;
			break;

		case PPECFG_DOT1P_RULE_ADD_DEI:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.rule.dei);
			if (error < 0) {
				goto print_error;
			}
			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.rule.dei)) || (nl_msg.rule.rule.dei > PPE_DOT1P_MAX_DEI_VAL)) {
				ppecfg_log_warn("DEI value :%d Maximum possible DEI value: %d",
									nl_msg.rule.rule.dei, PPE_DOT1P_MAX_DEI_VAL);
				error = -EINVAL;
				goto done;
			}
			mask |= 0x02;
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_DEI;
			break;

		case PPECFG_DOT1P_RULE_ADD_DSCP:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.rule.dscp);
			if (error < 0) {
				goto print_error;
			}

			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.rule.dscp)) || (nl_msg.rule.rule.dscp > PPE_DOT1P_MAX_DSCP_VAL)) {
				ppecfg_log_warn("DSCP value: %d  Maximum possible dscp value: %d",
									nl_msg.rule.rule.dscp, PPE_DOT1P_MAX_DSCP_VAL);
				error = -EINVAL;
				goto done;
			}

			mask |= 0x02;
			nl_msg.rule.rule.rule_flags |= PPE_DOT1P_RULE_FLAG_DSCP;
			break;

		case PPECFG_DOT1P_RULE_ADD_ACTION:
			sub_params = param->sub_params[PPECFG_DOT1P_RULE_ADD_ACTION].sub_params;
			mask |= 0x04;
			char fwd_cmd[10];

			data = sub_params[PPECFG_DOT1P_ACTION_FWD_CMD].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(fwd_cmd), &fwd_cmd);
				if (error) {
					goto print_error;
				}

				if (strcmp("FWD" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_DOT1P_CMD_FWD;
				} else if (strcmp("DROP" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_DOT1P_CMD_DROP;
				} else if (strcmp("COPY" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_DOT1P_CMD_COPY;
				} else if (strcmp("REDIR" , fwd_cmd) == 0) {
					nl_msg.rule.action.fwd_cmd = PPE_DOT1P_CMD_REDIR;
				} else {
					ppecfg_log_warn("Specify correct fwd_cmd value\n");
					error = -EINVAL;
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_FWD_CMD;
				mask |= 0x08;
			}

			data = sub_params[PPECFG_DOT1P_ACTION_GEM_PORT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.rule.action.gem_port);
				if (error) {
					goto print_error;
				}
				if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.action.gem_port)) || (nl_msg.rule.action.gem_port >= PPE_DOT1P_MAX_GEMPORT_VAL)) {
					ppecfg_log_warn("Maximum possible gemport value is %d", PPE_DOT1P_MAX_GEMPORT_VAL);
					error = -EINVAL;
					goto done;
				}

				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_GEM_PORT_ID;
				mask |= 0x10;
			}

			data = sub_params[PPECFG_DOT1P_ACTION_PQ].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.pq);
				if (error) {
					goto print_error;
				}

				if (is_pq_or_base_pq_set) {
					ppecfg_log_warn("User can specify only pq or base_pq for a gemport\n");
					error = -EINVAL;
					goto done;
				}

				if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.action.pq)) || (nl_msg.rule.action.pq >= PPE_DOT1P_MAX_PQ_VAL)) {
					ppecfg_log_warn("pq value: %d Maximum possible PQ value: %d",
									nl_msg.rule.action.pq, PPE_DOT1P_MAX_PQ_VAL);
					error = -EINVAL;
					goto done;
				}

				is_pq_or_base_pq_set = true;
				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_PQ;
				mask |= 0x20;
			}

			data = sub_params[PPECFG_DOT1P_ACTION_BASE_PQ].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.base_pq);
				if (error) {
					goto print_error;
				}

				if (is_pq_or_base_pq_set) {
					ppecfg_log_warn("User can specify only pq or base_pq for a gemport\n");
					error = -EINVAL;
					goto done;
				}

				if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.action.base_pq)) || (nl_msg.rule.action.base_pq >= PPE_DOT1P_MAX_PQ_VAL)) {
					ppecfg_log_warn("base pq value: %d Maximum possible base PQ value: %d",
									nl_msg.rule.action.base_pq, PPE_DOT1P_MAX_PQ_VAL);
					error = -EINVAL;
					goto done;
				}

				is_pq_or_base_pq_set = true;
				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_BASE_PQ;
				mask |= 0x20;
			}

			data = sub_params[PPECFG_DOT1P_ACTION_SC].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.svc_code);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_SVC_CODE;
			}

			data = sub_params[PPECFG_DOT1P_ACTION_INT_DP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.int_dp);
				if (error) {
					goto print_error;
				}
				if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.action.int_dp)) ||
						(nl_msg.rule.action.int_dp > PPE_DOT1P_MAX_INT_DP_VAL)) {
					ppecfg_log_warn("int_dp value: %d Maximum possible int_dp value %d",
								nl_msg.rule.action.int_dp, PPE_DOT1P_MAX_INT_DP_VAL);
					error = -EINVAL;
					goto done;
				}

				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_INT_DP;
			}

			data = sub_params[PPECFG_DOT1P_ACTION_DST_INFO].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.action.dst_info), &nl_msg.rule.action.dst_info);
				if (error < 0) {
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_DOT1P_ACTION_FLAG_DST_INFO;
			}
			break;
		}
	}

	if ((mask & PPECFG_DOT1P_MANDATORY_FIELDS_MASK) != PPECFG_DOT1P_MANDATORY_FIELDS_MASK) {
		ppecfg_log_warn("mask value: %x please specify all mandatory fields: [rule_id], [src_info]\n"
			"OR [vid] OR [pcp] OR [dst_info] OR [dei] OR [DSCP],\n"
			 "[action], [fwd_cmd], [gem_port], [pq] OR [base_pq] \n", mask);
		error = -EINVAL;
		goto done;
	}

	ppecfg_log_info("action_flags = %d rule_flags = %d\n", nl_msg.rule.action.action_flags, nl_msg.rule.rule.rule_flags);

	/*
	 * send message
	 */
	error = nss_ppenl_dot1p_rule_send(&nl_msg);
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
 * ppecfg_dot1p_rule_del()
 * 	handle DOT1P rule delete
 */
static int ppecfg_dot1p_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_msg = {{0}};
	int error;

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_dot1p_init_rule(&nl_msg, NSS_PPE_DOT1P_DELETE_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_DOT1P_RULE_DEL_RULE_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.rule_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_dot1p_rule_send(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_dot1p_rule_add_deafult()
 * 	Function to add default dot1p rule
 */
static int ppecfg_dot1p_rule_add_default(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	int mask = 0;

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

	nss_ppenl_dot1p_init_rule(&nl_msg, NSS_PPE_DOT1P_CREATE_DEF_RULE_MSG);

	for (int index = PPECFG_DOT1P_ADD_DEFAULT_VID; index < PPECFG_DOT1P_ADD_DEFAULT_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_DOT1P_ADD_DEFAULT_VID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.def_rule.vid);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			if (PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.def_rule.vid)) {
				ppecfg_log_warn("Please specify a positive 12 bit value\n");
				error = -EINVAL;
				goto done;
			}
			mask |= 0x01;
			nl_msg.rule.def_rule.def_rule_flags |= PPE_DOT1P_RULE_FLAG_VID;
			break;

		case PPECFG_DOT1P_ADD_DEFAULT_PCP:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.def_rule.pcp);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.def_rule.pcp)) || (nl_msg.rule.def_rule.pcp > PPE_DOT1P_MAX_PCP_VAL)) {
				ppecfg_log_warn("pcp value: %d Maximum possible PCP value: %d",
								nl_msg.rule.def_rule.pcp, PPE_DOT1P_MAX_PCP_VAL);
				error = -EINVAL;
				goto done;
			}
			mask |= 0x01;
			nl_msg.rule.def_rule.def_rule_flags |= PPE_DOT1P_RULE_FLAG_PCP;
			break;

		case PPECFG_DOT1P_ADD_DEFAULT_DEI:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.def_rule.dei);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.def_rule.dei)) || (nl_msg.rule.def_rule.dei > PPE_DOT1P_MAX_DEI_VAL)) {
				ppecfg_log_warn("DEI value: %d Maximum possible DEI value %d",
								nl_msg.rule.def_rule.dei, PPE_DOT1P_MAX_DEI_VAL);
				error = -EINVAL;
				goto done;
			}
			mask |= 0x01;
			nl_msg.rule.def_rule.def_rule_flags |= PPE_DOT1P_RULE_FLAG_DEI;
			break;

		case PPECFG_DOT1P_ADD_DEFAULT_DSCP:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.def_rule.dscp);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.def_rule.dscp)) || (nl_msg.rule.def_rule.dscp > PPE_DOT1P_MAX_DSCP_VAL)) {
				ppecfg_log_warn("DSCP value: %d Maximum possible dscp value %d",
									nl_msg.rule.def_rule.dscp, PPE_DOT1P_MAX_DSCP_VAL);
				error = -EINVAL;
				goto done;
			}

			mask |= 0x01;
			nl_msg.rule.def_rule.def_rule_flags |= PPE_DOT1P_RULE_FLAG_DSCP;
			break;

		case PPECFG_DOT1P_ADD_DEFAULT_DSCP_MASK:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.def_rule.dscp_mask);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			if (PPECFG_DOT1P_IS_NEGATIVE(nl_msg.rule.def_rule.dscp_mask)) {
				ppecfg_log_warn("Specify non negative dscp mask value.");
				error = -EINVAL;
				goto done;
			}

			mask |= 0x01;
			nl_msg.rule.def_rule.def_rule_flags |= PPE_DOT1P_RULE_FLAG_DSCP_MASK;
			break;

		case PPECFG_DOT1P_ADD_DEFAULT_GEN_MISS_CMD:
			char gen_miss_cmd[10];
			error = ppecfg_param_get_str(sub_params->data, sizeof(gen_miss_cmd), &gen_miss_cmd);

			if (error) {
				goto print_error;
			}
			if (strcmp("FWD" , gen_miss_cmd) == 0) {
				nl_msg.rule.def_rule.gen_miss_cmd = PPE_DOT1P_CMD_FWD;
			} else if (strcmp("DROP" , gen_miss_cmd) == 0) {
				nl_msg.rule.def_rule.gen_miss_cmd = PPE_DOT1P_CMD_DROP;
			} else if (strcmp("COPY" , gen_miss_cmd) == 0) {
				nl_msg.rule.def_rule.gen_miss_cmd = PPE_DOT1P_CMD_COPY;
			} else if (strcmp("REDIR" , gen_miss_cmd) == 0) {
				nl_msg.rule.def_rule.gen_miss_cmd = PPE_DOT1P_CMD_REDIR;
			} else {
				ppecfg_log_warn("Specify correct gen_miss_cmd value\n");
				error = -EINVAL;
				goto done;
			}

			nl_msg.rule.def_rule.def_rule_flags |= PPE_DOT1P_RULE_FLAG_GEN_MISS_CMD;
			mask |= 0x02;
			break;

		}
	}

	if ((mask & PPECFG_DOT1P_MANDATORY_DEFAULT_FIELDS_MASK) != PPECFG_DOT1P_MANDATORY_DEFAULT_FIELDS_MASK) {
		ppecfg_log_warn("mask value: %x please specify all mandatory fields:\n"
				"[vid] OR [pcp] OR [dei] OR [dscp] OR [dscp_mask] AND gen_miss_cmd\n", mask);
		error = -EINVAL;
		goto print_error;
	}

	ppecfg_log_info("default rule_flags = %d\n", nl_msg.rule.def_rule.def_rule_flags);

	nl_msg.rule.valid_flags |= PPE_DOT1P_RULE_FLAG_DEFAULT_RULE;

	/*
	 * send message
	 */
	error = nss_ppenl_dot1p_rule_send(&nl_msg);
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
 * ppecfg_dot1p_policer_rule_add()
 * 	handle DOT1P policer rule add
 */
static int ppecfg_dot1p_policer_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
	int policer_mask = 0;

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_dot1p_init_rule(&nl_msg, NSS_PPE_DOT1P_POLICER_CREATE_RULE_MSG);

	for (int index = PPECFG_DOT1P_RULE_ADD_GEMPORT_ID; index < PPECFG_DOT1P_RULE_ADD_POLICER_ID_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_DOT1P_RULE_ADD_GEMPORT_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.policer_rule.gemport_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			if ((PPECFG_DOT1P_IS_NEGATIVE(nl_msg.policer_rule.gemport_id)) ||
					(nl_msg.policer_rule.gemport_id >= PPE_DOT1P_MAX_GEMPORT_VAL)) {
				ppecfg_log_warn("gemport value: %d Please specify gemport id between [0 - %d]\n",
							nl_msg.policer_rule.gemport_id, (PPE_DOT1P_MAX_GEMPORT_VAL));
				error = -EINVAL;
				goto done;
			}
			policer_mask |= 0x01;
			nl_msg.policer_rule.valid_flags |= PPE_DOT1P_POLICER_RULE_FLAG_GEM_PORT_ID;
			nl_msg.policer_rule.rule.rule_flags |= PPE_DOT1P_POLICER_RULE_FLAG_GEM_PORT_ID;
			nl_msg.policer_rule.rule.gemport_id = nl_msg.policer_rule.gemport_id;
			break;

		case PPECFG_DOT1P_RULE_ADD_POLICER_ACTION:
			sub_params = param->sub_params[PPECFG_DOT1P_RULE_ADD_POLICER_ACTION].sub_params;
			char us_policer_en[10];
			char ds_policer_en[10];

			data = sub_params[PPECFG_DOT1P_POLICER_ACTION_US_POLICER_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint16_t), &nl_msg.policer_rule.action.policer_id);
				if (error) {
					goto print_error;
				}

				if ((nl_msg.policer_rule.action.policer_id <= PPE_DOT1P_MIN_USER_POLICER_ID) ||
						(nl_msg.policer_rule.action.policer_id > PPE_DOT1P_MAX_USER_POLICER_ID)) {
					ppecfg_log_warn("policer id value: %d policer id ranges from %d to %d ",
								nl_msg.policer_rule.action.policer_id, PPE_DOT1P_MIN_USER_POLICER_ID,
								PPE_DOT1P_MAX_USER_POLICER_ID);
					error = -EINVAL;
					goto done;
				}
				nl_msg.policer_rule.action.action_flags |= PPE_DOT1P_POLICER_ACTION_FLAG_POLICER_ID;
				policer_mask |= 0x1;
			}

			data = sub_params[PPECFG_DOT1P_POLICER_ACTION_US_POLICER_EN].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(us_policer_en), &us_policer_en);
				if (error) {
					goto print_error;
				}

				if (strcmp("ENABLE" , us_policer_en) == 0) {
					nl_msg.policer_rule.action.us_policer_en = true;
				} else {
					nl_msg.policer_rule.action.us_policer_en = false;
				}

				nl_msg.policer_rule.action.action_flags |= PPE_DOT1P_POLICER_ACTION_FLAG_US_POLICER_EN;
				policer_mask |= 0x2;
			}

			data = sub_params[PPECFG_DOT1P_POLICER_ACTION_DS_POLICER_EN].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(ds_policer_en), &ds_policer_en);
				if (error) {
					goto print_error;
				}

				if (strcmp("ENABLE" , ds_policer_en) == 0) {
					nl_msg.policer_rule.action.ds_policer_en = true;
				} else {
					nl_msg.policer_rule.action.ds_policer_en = false;
				}

				nl_msg.policer_rule.action.action_flags |= PPE_DOT1P_POLICER_ACTION_FLAG_DS_POLICER_EN;
				policer_mask |= 0x2;
			}

			break;
		}
	}

	if ((policer_mask)  && ((policer_mask & PPECFG_DOT1P_MANDATORY_POLICER_ID_MASK)
						!= PPECFG_DOT1P_MANDATORY_POLICER_ID_MASK)) {
		ppecfg_log_warn("mask value: %x Upstream policer id enable it for upstream/downstream flow\n"
				"OR\n"
				"If enabling policer id for upstream/downstream flow specify us_policer_id\n:",
				policer_mask);
		error = -EINVAL;
		goto done;
	}

	ppecfg_log_info("policer action_flags = %d policer rule_flags = %d\n",
			nl_msg.policer_rule.action.action_flags, nl_msg.policer_rule.rule.rule_flags);

	/*
	 * send message
	 */
	error = nss_ppenl_dot1p_rule_send(&nl_msg);
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
 * ppecfg_dot1p_rule_flush()
 * 	Function to flush all dot1p rules
 */
static int ppecfg_dot1p_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_dot1p_msg = {{0}};
	bool except_first = false;
	int error;

	/*
	 * Check for except_first in the input arguments
	 */
	for (int i = 0; i < match->total; i++) {
		if (strcmp(match->args[i], "except_first") == 0) {
			except_first = true;
			break;
		}
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error && match->total > 3) {
		ppecfg_log_arg_error(param);
		return error;
	}

	nss_ppenl_dot1p_init_rule(&nl_dot1p_msg, NSS_PPE_DOT1P_FLUSH_RULE_MSG);
	nl_dot1p_msg.rule.except_flag = except_first;

	error = nss_ppenl_dot1p_rule_send(&nl_dot1p_msg);
	if (error) {
		ppecfg_log_warn("Flush dot1p rule failed!\n");
		return error;
	}

	return error;
}

/*
 * ppecfg_dot1p_policer_rule_flush()
 * 	Function to flush all dot1p policer rules
 */
static int ppecfg_dot1p_policer_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_dot1p_msg = {{0}};
	int error;

	nss_ppenl_dot1p_init_rule(&nl_dot1p_msg, NSS_PPE_DOT1P_POLICER_FLUSH_RULE_MSG);

	error = nss_ppenl_dot1p_rule_send(&nl_dot1p_msg);
	if (error) {
		ppecfg_log_warn("Flush dot1p policer rule failed!\n");
		return error;
	}

	return error;
}

/*
 * ppecfg_dot1p_policer_rule_del()
 * 	handle DOT1P policer rule delete
 */
static int ppecfg_dot1p_policer_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_msg = {{0}};
	int error;

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_dot1p_init_rule(&nl_msg, NSS_PPE_DOT1P_POLICER_DELETE_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_DOT1P_POLICER_RULE_DEL_GEMPORT_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.policer_rule.gemport_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_dot1p_rule_send(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_dot1p_rule_pause()
 * 	Function to pause provisioning all dot1p rules
 */
static int ppecfg_dot1p_rule_pause(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_dot1p_msg = {{0}};
	bool except_gemport_0 = false;
	int error;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	/*
	 * Check for except_first in the input arguments
	 */
	for (int i = 0; i < match->total; i++) {
		if (strcmp(match->args[i], "except_gemport_0") == 0) {
			except_gemport_0 = true;
			ppecfg_log_warn("pause dot1p rule except gemport 0\n");
			break;
		}
	}

	nss_ppenl_dot1p_init_rule(&nl_dot1p_msg, NSS_PPE_DOT1P_PAUSE_RULE_MSG);
	nl_dot1p_msg.rule.pause_except_flag = except_gemport_0;

	error = nss_ppenl_dot1p_rule_send(&nl_dot1p_msg);
	if (error) {
		ppecfg_log_warn("pause dot1p rule failed!\n");
		return error;
	}

	return error;
}

/*
 * ppecfg_dot1p_rule_resume()
 * 	Function to resume provisioning all dot1p rules
 */
static int ppecfg_dot1p_rule_resume(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_dot1p_msg = {{0}};
	int error;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL \n");
		return -EINVAL;
	}

	nss_ppenl_dot1p_init_rule(&nl_dot1p_msg, NSS_PPE_DOT1P_RESUME_RULE_MSG);

	error = nss_ppenl_dot1p_rule_send(&nl_dot1p_msg);
	if (error) {
		ppecfg_log_warn("Resume dot1p rule failed!\n");
		return error;
	}

	return error;
}

/*
 * ppecfg_dot1p_rule_get_pause_state()
 * 	Function to get state of dot1p rules
 */
static int ppecfg_dot1p_rule_get_pause_state(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_dot1p_rule nl_dot1p_msg = {{0}};
	int error = 0;

	nss_ppenl_dot1p_init_rule(&nl_dot1p_msg, NSS_PPE_DOT1P_GET_STATE_RULE_MSG);

	error = nss_ppenl_dot1p_rule_send(&nl_dot1p_msg);
	if (error) {
		ppecfg_log_warn("Get state dot1p rule failed!\n");
		return error;
	}

	return error;
}
