/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG COS MAPPING handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_cos_map.h"

static int ppecfg_cos_map_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_cos_map_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_cos_map_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_cos_map_port_group_set(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * Rule add DSCP parameters
 */
static struct ppecfg_param tos_params[PPECFG_COS_MAP_TOS_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_TOS_DSCP_VAL, "dscp_val="),
};

/*
 * Rule add PCP and DEI parameters
 */
static struct ppecfg_param tci_params[PPECFG_COS_MAP_TCI_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_TCI_PCP_VAL, "pcp_val="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_TCI_DEI_VAL, "dei_val="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param action_params[PPECFG_COS_MAP_ACTION_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_ACTION_DSCP, "dscp="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_ACTION_PCP, "pcp="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_ACTION_DEI, "dei="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_ACTION_INT_PRI, "int_pri="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_ACTION_DP, "dp="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param rule_add_params[PPECFG_COS_MAP_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_RULE_ADD_RULE_ID, "rule_id="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_RULE_ADD_GROUP_ID, "group_id="),
	PPECFG_PARAMARR_INIT(PPECFG_COS_MAP_RULE_ADD_TOS, "TOS", tos_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_COS_MAP_RULE_ADD_TCI, "TCI", tci_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_COS_MAP_RULE_ADD_ACTION, "action", action_params, ppecfg_param_iter_tbl),
};

/*
 * rule del parameters
 */
static struct ppecfg_param rule_del_params[PPECFG_COS_MAP_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_RULE_DEL_RULE_ID, "rule_id="),
};

/*
 * port group set parameters
 */
static struct ppecfg_param port_group_set_params[PPECFG_COS_MAP_GROUP_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_GROUP_ADD_DEV, "dev="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_GROUP_ADD_TCI_GROUP_ID, "tci_group_id="),
	PPECFG_PARAM_INIT(PPECFG_COS_MAP_GROUP_ADD_TOS_GROUP_ID, "tos_group_id="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_cos_map_cmd' should also get updated
 */
struct ppecfg_param ppecfg_cos_map_params[PPECFG_COS_MAP_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", rule_add_params, ppecfg_cos_map_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", rule_del_params, ppecfg_cos_map_rule_del),
	PPECFG_PARAMFUNC_INIT("cmd=flush", ppecfg_cos_map_rule_flush),
	PPECFG_PARAMLIST_INIT("cmd=port_group_set", port_group_set_params, ppecfg_cos_map_port_group_set),
};

/*
 * ppecfg_cos_map_port_group_set()
 * 	handle port group set
 */
static int ppecfg_cos_map_port_group_set(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_cos_map_config nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	int val;

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

	nss_ppenl_cos_map_init_config(&nl_msg, NSS_PPE_COS_MAP_PORT_GROUP_SET);

	for (int index = PPECFG_COS_MAP_RULE_ADD_RULE_ID; index <= PPECFG_COS_MAP_GROUP_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (!sub_params->valid) {
			continue;
		}

		switch (index) {
		case PPECFG_COS_MAP_GROUP_ADD_DEV:
			sub_params = &param->sub_params[PPECFG_COS_MAP_GROUP_ADD_DEV];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.msg.config.dev), &nl_msg.msg.config.dev);
			if (error < 0) {
				ppecfg_log_error("Device name is required\n");
				goto done;
			}

			break;

		case PPECFG_COS_MAP_GROUP_ADD_TCI_GROUP_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &val);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (!val) {
				nl_msg.msg.config.tci_grp_id = PPE_COS_MAP_PORT_GROUP_0;
			} else if (val == PPE_COS_MAP_PORT_GROUP_1) {
				nl_msg.msg.config.tci_grp_id = PPE_COS_MAP_PORT_GROUP_1;
			} else {
				ppecfg_log_error("invalid group id\n");
				error = -EINVAL;
				goto done;
			}

			break;

		case PPECFG_COS_MAP_GROUP_ADD_TOS_GROUP_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &val);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				error = -EINVAL;
				goto done;
			}

			if (!val) {
				nl_msg.msg.config.tos_grp_id = PPE_COS_MAP_PORT_GROUP_0;
			} else if (val == PPE_COS_MAP_PORT_GROUP_1) {
				nl_msg.msg.config.tos_grp_id = PPE_COS_MAP_PORT_GROUP_1;
			} else {
				ppecfg_log_error("invalid group id\n");
				error = -EINVAL;
				goto done;
			}

			break;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_cos_map_send_config(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
	}

done:
	return error;
}

/*
 * ppecfg_cos_map_rule_add()
 * 	handle COS MAP rule add
 */
static int ppecfg_cos_map_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_cos_map_config nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;

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

	nss_ppenl_cos_map_init_config(&nl_msg, NSS_PPE_COS_MAP_CREATE_RULE_MSG);

	for (int index = PPECFG_COS_MAP_RULE_ADD_RULE_ID; index <= PPECFG_COS_MAP_RULE_ADD_ACTION; index++) {
		sub_params = &param->sub_params[index];
		if (!sub_params->valid) {
			continue;
		}

		switch (index) {
		case PPECFG_COS_MAP_RULE_ADD_RULE_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.rule.rule_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_COS_MAP_RULE_ADD_GROUP_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.rule.group_id);
			if (error < 0) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_COS_MAP_RULE_ADD_TOS:
			sub_params = param->sub_params[PPECFG_COS_MAP_RULE_ADD_TOS].sub_params;

			data = sub_params[PPECFG_COS_MAP_TOS_DSCP_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.msg.rule.dscp_val);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.type_flag |= PPE_COS_MAP_RULE_TOS;
			}

			break;

		case PPECFG_COS_MAP_RULE_ADD_TCI:
			sub_params = param->sub_params[PPECFG_COS_MAP_RULE_ADD_TCI].sub_params;

			data = sub_params[PPECFG_COS_MAP_TCI_PCP_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.msg.rule.pcp_val);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.type_flag |= PPE_COS_MAP_RULE_TCI;
			}

			data = sub_params[PPECFG_COS_MAP_TCI_DEI_VAL].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.msg.rule.dei_val);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.type_flag |= PPE_COS_MAP_RULE_TCI;
			}

			break;

		case PPECFG_COS_MAP_RULE_ADD_ACTION:
			sub_params = param->sub_params[PPECFG_COS_MAP_RULE_ADD_ACTION].sub_params;

			data = sub_params[PPECFG_COS_MAP_ACTION_DSCP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint32_t), &nl_msg.msg.rule.action_info.dscp);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.action_info.flags |= PPE_COS_MAP_ACTION_SET_DSCP;
			}

			data = sub_params[PPECFG_COS_MAP_ACTION_PCP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.msg.rule.action_info.pcp);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.action_info.flags |= PPE_COS_MAP_ACTION_SET_PCP;
			}

			data = sub_params[PPECFG_COS_MAP_ACTION_DEI].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.msg.rule.action_info.dei);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.action_info.flags |= PPE_COS_MAP_ACTION_SET_DEI;
			}

			data = sub_params[PPECFG_COS_MAP_ACTION_INT_PRI].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.msg.rule.action_info.pri);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.action_info.flags |= PPE_COS_MAP_ACTION_SET_INT_PRI;
			}

			data = sub_params[PPECFG_COS_MAP_ACTION_DP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.msg.rule.action_info.dp);
				if (error) {
					goto print_error;
				}

				nl_msg.msg.rule.action_info.flags |= PPE_COS_MAP_ACTION_SET_DP;

			}
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_cos_map_send_config(&nl_msg);
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
 * ppecfg_cos_map_rule_del()
 * 	handle CoS map rule delete
 */
static int ppecfg_cos_map_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_cos_map_config nl_msg = {{0}};
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

	nss_ppenl_cos_map_init_config(&nl_msg, NSS_PPE_COS_MAP_DESTROY_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_COS_MAP_RULE_DEL_RULE_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.rule.rule_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_cos_map_send_config(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_cos_map_rule_flush()
 * 	Function to flush all cos_map rules
 */
static int ppecfg_cos_map_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_cos_map_config nl_cos_map_msg = {{0}};
	int error;

	nss_ppenl_cos_map_init_config(&nl_cos_map_msg, NSS_PPE_COS_MAP_FLUSH_RULE_MSG);

	error = nss_ppenl_cos_map_send_config(&nl_cos_map_msg);
	if (error) {
		ppecfg_log_warn("Flush QoS map rule failed!\n");
		return error;
	}

	return error;
}
