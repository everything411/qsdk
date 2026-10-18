/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG GEM_PORT handler
 */

#include <nss_ppenl_base.h>

#include "ppecfg_hlos.h"
#include "ppecfg_param.h"
#include "ppecfg_gemport.h"
#include "nss_ppenl_gemport_if.h"

#define PPECFG_GEMPORT_MANDATORY_FIELDS_MASK 0x03

static int ppecfg_gem_port_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_gem_port_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_gem_port_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * rule action parameters GEM PORT
 */
static struct ppecfg_param gem_port_action_params[PPECFG_GEM_PORT_ACTION_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_ACTION_SRC_INFO, "src_info="),
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_ACTION_INT_PRI, "int_pri="),
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_ACTION_INT_DP, "int_dp="),
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_ACTION_DST_PORT_TYPE, "dst_port_type="),
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_ACTION_DST_INFO, "dst_info="),
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_ACTION_DST_SELECTION, "dst_selection="),
};

/*
 * rule add parameters
 */
static struct ppecfg_param gem_port_rule_add_params[PPECFG_GEM_PORT_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_RULE_ADD_GEM_PORT_ID, "gem_port="),
	PPECFG_PARAMARR_INIT(PPECFG_GEM_PORT_RULE_ADD_ACTION, "action", gem_port_action_params, ppecfg_param_iter_tbl),
};

/*
 * rule del parameters
 */
static struct ppecfg_param gem_port_rule_del_params[PPECFG_GEM_PORT_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_GEM_PORT_RULE_DEL_GEM_PORT_ID, "gem_port_id="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_gem_port_cmd' should also get updated
 */
struct ppecfg_param ppecfg_gem_port_params[PPECFG_GEM_PORT_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", gem_port_rule_add_params, ppecfg_gem_port_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", gem_port_rule_del_params, ppecfg_gem_port_rule_del),
	PPECFG_PARAMFUNC_INIT("cmd=flush", ppecfg_gem_port_rule_flush),
};

/*
 * ppecfg_gem_port_rule_add()
 * 	handle GEM_PORT rule add
 */
static int ppecfg_gem_port_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_gem_port_rule nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;
	char *data;
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

	nss_ppenl_gem_port_init_rule(&nl_msg, NSS_PPE_GEM_PORT_CREATE_RULE_MSG);

	for (int index = PPECFG_GEM_PORT_RULE_ADD_GEM_PORT_ID; index < PPECFG_GEM_PORT_RULE_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == 0) {
			continue;
		}

		switch (index) {
		case PPECFG_GEM_PORT_RULE_ADD_GEM_PORT_ID:
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.rule.rule.gem_port_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			if (nl_msg.rule.rule.gem_port_id >= PPE_GEM_PORT_MAX_ID) {
				ppecfg_log_warn("Please specify gemport between [0 - %d]\n", (PPE_GEM_PORT_MAX_ID));
				error = -EINVAL;
				goto print_error;
			}
			mask |= 0x01;
			nl_msg.rule.rule.rule_flags |= PPE_GEM_PORT_RULE_FLAG_GEM_PORT_ID;
			break;

		case PPECFG_GEM_PORT_RULE_ADD_ACTION:
			sub_params = param->sub_params[PPECFG_GEM_PORT_RULE_ADD_ACTION].sub_params;
			mask |= 0x02;
			char dst_selection[10];
			char buf[32];

			data = sub_params[PPECFG_GEM_PORT_ACTION_DST_SELECTION].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(dst_selection), &dst_selection);
				if (error) {
					goto print_error;
				}

				/*
				 * selecting destination from FDB means no destination information
				 * is programmed from GEM_PORT_MAPPING_TBL.
				 */
				if (strcmp("FDB" , dst_selection) == 0) {
					nl_msg.rule.action.dst_selection = PPE_GEM_PORT_DST_FDB_TBL;
				} else if (strcmp("GEMPORT" , dst_selection) == 0) {
					nl_msg.rule.action.dst_selection = PPE_GEM_PORT_DST_GEM_PORT_TBL;
				} else {
					ppecfg_log_info("Please select FDB or GEMPORT as dst selection\n");
					error = -EINVAL;
					goto done;
				}
				nl_msg.rule.action.action_flags |= PPE_GEM_PORT_ACTION_FLAG_DST_SELECTION;
			}

			data = sub_params[PPECFG_GEM_PORT_ACTION_INT_PRI].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.int_pri);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_GEM_PORT_ACTION_FLAG_INT_PRI;
			}

			data = sub_params[PPECFG_GEM_PORT_ACTION_SRC_INFO].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.action.src_info), &nl_msg.rule.action.src_info);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_GEM_PORT_ACTION_FLAG_SRC_INFO;
			}

			data = sub_params[PPECFG_GEM_PORT_ACTION_INT_DP].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(uint8_t), &nl_msg.rule.action.int_dp);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_GEM_PORT_ACTION_FLAG_INT_DP;
			}

			data = sub_params[PPECFG_GEM_PORT_ACTION_DST_PORT_TYPE].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(buf), &buf);
				if (error < 0) {
					goto print_error;
				}
				if (strcasecmp(buf, "BITMAP") == 0) {
					nl_msg.rule.action.dst_port_type = PPE_GEM_PORT_TYPE_BITMAP;
				}
				if (strcasecmp(buf, "PORT") == 0) {
					nl_msg.rule.action.dst_port_type = PPE_GEM_PORT_TYPE_PORT;
				}
				nl_msg.rule.action.action_flags |= PPE_GEM_PORT_ACTION_FLAG_DST_PORT_TYPE;
			}

			data = sub_params[PPECFG_GEM_PORT_ACTION_DST_INFO].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.rule.action.dst_info), &nl_msg.rule.action.dst_info);
				if (error) {
					goto print_error;
				}

				nl_msg.rule.action.action_flags |= PPE_GEM_PORT_ACTION_FLAG_DST_INFO;
			} else if ((nl_msg.rule.action.action_flags & PPE_GEM_PORT_ACTION_FLAG_DST_SELECTION) &&
					(nl_msg.rule.action.dst_selection == PPE_GEM_PORT_DST_GEM_PORT_TBL)) {
						error = -EINVAL;
						ppecfg_log_info("Please specify dst_info if dst_selection is gemport\n");
						goto done;
			}
			break;
		}
	}

	if ((mask & PPECFG_GEMPORT_MANDATORY_FIELDS_MASK) != PPECFG_GEMPORT_MANDATORY_FIELDS_MASK) {
		ppecfg_log_warn("mask value: %02X please specify all mandatory fields: [gem_port_id] and action \n", mask);
		error = -EINVAL;
		goto print_error;
	}

	ppecfg_log_info("action_flags = %d rule_flags = %d \n", nl_msg.rule.action.action_flags, nl_msg.rule.rule.rule_flags);

	/*
	 * send message
	 */
	error = nss_ppenl_gem_port_rule_send(&nl_msg);
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
 * ppecfg_gem_port_rule_del()
 * 	handle GEM PORT rule delete
 */
static int ppecfg_gem_port_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_gem_port_rule nl_msg = {{0}};
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

	nss_ppenl_gem_port_init_rule(&nl_msg, NSS_PPE_GEM_PORT_DELETE_RULE_MSG);

	/*
	 * extract selectors
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_GEM_PORT_RULE_DEL_GEM_PORT_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint16_t), &nl_msg.rule.rule.gem_port_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_gem_port_rule_send(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_gem_port_rule_flush()
 * 	Function to flush all gem port rules
 */
static int ppecfg_gem_port_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_gem_port_rule nl_gem_port_msg = {{0}};
	int error;

	nss_ppenl_gem_port_init_rule(&nl_gem_port_msg, NSS_PPE_GEM_PORT_FLUSH_RULE_MSG);

	error = nss_ppenl_gem_port_rule_send(&nl_gem_port_msg);
	if (error) {
		ppecfg_log_warn("Flush gem port rule failed!\n");
		return error;
	}

	return error;
}
