/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_policer.h"

static int ppecfg_policer_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_policer_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_policer_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 *  policer_rule add parameters
 */
static struct ppecfg_param rule_add_params[PPECFG_POLICER_RULE_ADD_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_IS_PORT_POLICER, "port_policer="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_DEV,"dev="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_RULE_ID, "rule_id="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_DIRECTION, "direction="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_MODE,"meter_mode="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_UNIT,"meter_unit="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COMMITTED_RATE, "committed_rate="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COMMITTED_BURST_SIZE, "committed_burst_size="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_PEAK_RATE, "peak_rate="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_PEAK_BURST_SIZE, "peak_burst_size="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_ENABLE, "meter_enable="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COUPLE_ENABLE, "couple_enable="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COLOUR_AWARE, "colour_aware_enable="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_FLAG, "meter_flag="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_YELLOW_DP, "yellow_dp="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_YELLOW_INT_PRI, "yellow_int_pri="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_YELLOW_PCP, "yellow_pcp="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_YELLOW_DEI, "yellow_dei="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_YELLOW_DSCP, "yellow_dscp="),
};

/*
 * acl_policer_rule del parameters
 */
static struct ppecfg_param rule_del_params[PPECFG_POLICER_RULE_DEL_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_DEL_IS_PORT_POLICER, "port_policer="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_DEL_DEV, "dev="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_DEL_RULE_ID, "rule_id="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_policer_cmd' should also get updated
 * Supported Policer commands
 */
struct ppecfg_param ppecfg_policer_params[PPECFG_POLICER_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=rule_add", rule_add_params, ppecfg_policer_rule_add),
	PPECFG_PARAMLIST_INIT("cmd=rule_del", rule_del_params, ppecfg_policer_rule_del),
	PPECFG_PARAMFUNC_INIT("cmd=flush", ppecfg_policer_rule_flush),
};

/*
 * ppecfg_policer_del()
 * 	handle policer rule delete
 */
static int ppecfg_policer_rule_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_policer_rule nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL");
		return -EINVAL;
	}

	/*
	 *
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error < 0) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_policer_init_rule(&nl_msg, NSS_PPE_POLICER_DESTROY_RULE_MSG);

	for (int index = PPECFG_POLICER_RULE_DEL_IS_PORT_POLICER; index <= PPECFG_POLICER_RULE_DEL_RULE_ID; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch(index) {
		case PPECFG_POLICER_RULE_DEL_IS_PORT_POLICER:
			/*
			 * parse policer choice from user
			*/
			sub_params = &param->sub_params[PPECFG_POLICER_RULE_DEL_IS_PORT_POLICER];
			error = ppecfg_param_get_bool(sub_params->data,&nl_msg.config.is_port_policer);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_DEL_DEV:
			/*
			* parse dev name for port policer
			*/
			sub_params = &param->sub_params[PPECFG_POLICER_RULE_DEL_DEV];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.config.dev), &nl_msg.config.dev);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_DEL_RULE_ID:
				/*
				* parse rule id from user
				*/
				sub_params = &param->sub_params[PPECFG_POLICER_RULE_DEL_RULE_ID];
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.config.policer_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				break;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_policer_rule_del(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_policer_rule_add()
 * 	handle policer rule add
 */
static int ppecfg_policer_rule_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_policer_rule nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL");
		return -EINVAL;
	}

	/*
	 * iterate through the param table to identify the matched arguments and
	 * populate the argument list
	 */
	error = ppecfg_param_iter_tbl(param, match);
	if (error < 0) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_policer_init_rule(&nl_msg, NSS_PPE_POLICER_CREATE_RULE_MSG);

	/*
	 * setting meter_mode, couple_enable, colour_aware as enable, user should pass 0 to disable
	 */
	nl_msg.config.meter_enable = 1;
	nl_msg.config.couple_enable = 1;
	nl_msg.config.colour_aware = 1;
	/*
	 * setting direction, user should pass 1/2 to set US/DS
	 */
	nl_msg.config.dir = 0;

	for (int index = PPECFG_POLICER_RULE_ADD_IS_PORT_POLICER; index < PPECFG_POLICER_RULE_ADD_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_POLICER_RULE_ADD_IS_PORT_POLICER:
			/*
			* parse port_policer from user
			*/
			error = ppecfg_param_get_bool(sub_params->data,&nl_msg.config.is_port_policer);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_DEV:
			/*
			* parse dev name for port policer
			*/
			sub_params = &param->sub_params[PPECFG_POLICER_RULE_ADD_DEV];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.config.dev), &nl_msg.config.dev);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_RULE_ID:
				/*
				* parse rule id from user
				*/
				sub_params = &param->sub_params[PPECFG_POLICER_RULE_ADD_RULE_ID];
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.config.policer_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				break;

		case PPECFG_POLICER_RULE_ADD_DIRECTION:
			/*
			 * parse optional direction from user_config, default 0
			 */
			char dir[5];

			error = ppecfg_param_get_str(sub_params->data, sizeof(dir), &dir);
			if (error) {
				ppecfg_log_data_error(sub_params);
				goto done;
			}

			if (strcmp("us", dir) == 0) {
				nl_msg.config.dir = PPECFG_POLICER_DIRECTION_US;
			} else if (strcmp("ds", dir) == 0) {
				nl_msg.config.dir = PPECFG_POLICER_DIRECTION_DS;
			} else {
				ppecfg_log_warn("Valid Inputs: [us][ds]\n");
				error = -EINVAL;
				goto done;
			}
			break;

		case PPECFG_POLICER_RULE_ADD_METER_MODE:
			/*
			* parse optional meter mode from user_config, default 0
			*/
			error = ppecfg_param_get_bool(sub_params->data,&nl_msg.config.meter_mode);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_METER_UNIT:
			/*
			* parse option meter_unit, default 0 Byte based
			*/
			error = ppecfg_param_get_bool(sub_params->data,&nl_msg.config.meter_unit);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_COMMITTED_RATE:
			/*
			* parse committed_rate from user_config
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.config.committed_rate);

			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_COMMITTED_BURST_SIZE:
			/*
			* Parse committed_brust_size from user_config
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.config.committed_burst_size);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_PEAK_RATE:
			/*
			* parse optional EIR from user_config for meter mode is RFC 2697 or RFC 4115
			* meter_mode 0 ==> 2698 (ALL)
			* meter_mode 1 && EIR ==> 4115 (ALL)
			* meter_mode 1 && no EIR ==> 2697 (CIR,CBS,EBS)
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.config.peak_rate);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_PEAK_BURST_SIZE:
			/*
			* parse peak_burst_size from user_config
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.config.peak_burst_size);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_METER_ENABLE:
			/*
			* parse optional meter_enable from user_config, default 1
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t),&nl_msg.config.meter_enable);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_COUPLE_ENABLE:
			/*
			* parse optional couple_enable from user_config, default 1
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.couple_enable);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_COLOUR_AWARE:
			/*
			* parse optional colour_aware from user_config,default 1
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.colour_aware);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_METER_FLAG:
			/*
			* Parse optional meter_flag from user_config, default 0
			* Accepts single or multiple flag values separated by underscore
			*
			* Valid packet types (5 types total):
			*   - uc
			*   - uuc
			*   - mc
			*   - umc
			*   - bc
			*
			* Single type examples:
			*   meter_flag=uc, meter_flag=mc, meter_flag=bc
			*
			* Multiple type examples (ALL combinations are supported):
			*   meter_flag=bc_mc
			*   meter_flag=uc_mc_bc
			*   meter_flag=uc_uuc_mc_umc_bc
			*   ... any combination of the 5 packet types is valid
			*/
			{
				char meter_flag_str[128];
				char *token;
				char *saveptr;
				char temp_str[128];
				uint32_t combined_flags = 0;
				bool flag_found = false;
				uint32_t duplicate_check = 0;

				error = ppecfg_param_get_str(sub_params->data, sizeof(meter_flag_str), meter_flag_str);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}

				/*
				 * Make a copy of the input string for tokenization
				 * since strtok_r modifies the original string
				 */
				strlcpy(temp_str, meter_flag_str, sizeof(temp_str) - 1);
				temp_str[sizeof(temp_str) - 1] = '\0';

				/*
				 * Parse the string and combine multiple flags using underscore as delimiter
				 */
				token = strtok_r(temp_str, "_", &saveptr);
				while (token != NULL) {
					flag_found = false;

					if (strcasecmp(token, "uc") == 0) {
						if (duplicate_check & PPECFG_POLICER_METER_FLAG_UNICAST) {
							ppecfg_log_error("Duplicate meter_flag: \'uc\' specified multiple times\n");
							error = -EINVAL;
							goto done;
						}
						combined_flags |= PPECFG_POLICER_METER_FLAG_UNICAST;
						duplicate_check |= PPECFG_POLICER_METER_FLAG_UNICAST;
						flag_found = true;
					} else if (strcasecmp(token, "uuc") == 0) {
						if (duplicate_check & PPECFG_POLICER_METER_FLAG_UNKNOWN_UNICAST) {
							ppecfg_log_error("Duplicate meter_flag: 'uuc' specified multiple times\n");
							error = -EINVAL;
							goto done;
						}
						combined_flags |= PPECFG_POLICER_METER_FLAG_UNKNOWN_UNICAST;
						duplicate_check |= PPECFG_POLICER_METER_FLAG_UNKNOWN_UNICAST;
						flag_found = true;
					} else if (strcasecmp(token, "umc") == 0) {
						if (duplicate_check & PPECFG_POLICER_METER_FLAG_UNKNOWN_MULTICAST) {
							ppecfg_log_error("Duplicate meter_flag: 'umc' specified multiple times\n");
							error = -EINVAL;
							goto done;
						}
						combined_flags |= PPECFG_POLICER_METER_FLAG_UNKNOWN_MULTICAST;
						duplicate_check |= PPECFG_POLICER_METER_FLAG_UNKNOWN_MULTICAST;
						flag_found = true;
					} else if (strcasecmp(token, "mc") == 0) {
						if (duplicate_check & PPECFG_POLICER_METER_FLAG_MULTICAST) {
							ppecfg_log_error("Duplicate meter_flag: \'mc\' specified multiple times\n");
							error = -EINVAL;
							goto done;
						}
						combined_flags |= PPECFG_POLICER_METER_FLAG_MULTICAST;
						duplicate_check |= PPECFG_POLICER_METER_FLAG_MULTICAST;
						flag_found = true;
					} else if (strcasecmp(token, "bc") == 0) {
						if (duplicate_check & PPECFG_POLICER_METER_FLAG_BROADCAST) {
							ppecfg_log_error("Duplicate meter_flag: \'bc\' specified multiple times\n");
							error = -EINVAL;
							goto done;
						}
						combined_flags |= PPECFG_POLICER_METER_FLAG_BROADCAST;
						duplicate_check |= PPECFG_POLICER_METER_FLAG_BROADCAST;
						flag_found = true;
					}

					if (!flag_found) {
						ppecfg_log_error("Invalid meter_flag token: '%s'\n", token);
						ppecfg_log_error("Valid packet types: 'uc', 'uuc', 'mc', 'umc', 'bc'\n");
						ppecfg_log_error("Combine multiple types using underscore (e.g., 'bc_mc')\n");
						error = -EINVAL;
						goto done;
					}

					token = strtok_r(NULL, "_", &saveptr);
				}

				/*
				 * Check if at least one valid flag was parsed
				 */
				if (combined_flags == 0) {
					ppecfg_log_error("No valid meter_flag found in: '%s'\n", meter_flag_str);
					ppecfg_log_error("Valid packet types: 'uc', 'uuc', 'mc', 'umc', 'bc'\n");
					ppecfg_log_error("Combine multiple types using underscore (e.g., 'bc_mc')\n");
					error = -EINVAL;
					goto done;
				}

				nl_msg.config.meter_flag = combined_flags;
				nl_msg.config.meter_flag_valid = 1;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_YELLOW_DP:
			/*
			* Parse optional yellow_dp from user_config, default 0
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.action_info.yellow_dp);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_YELLOW_INT_PRI:
			/*
			* parse optional yellow_int_pri from user_config, default 0
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.action_info.yellow_int_pri);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_YELLOW_PCP:
			/*
			* parse optional yellow_pcp from user_config, default 0
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.action_info.yellow_pcp);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_YELLOW_DEI:
			/*
			* parse optional yellow_dei from user_config, default 0
			*/
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.action_info.yellow_dei);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_POLICER_RULE_ADD_YELLOW_DSCP:
			/*
			* parse optional dscp only in case of ACL, default 0
			*/
			if (!nl_msg.config.is_port_policer) {
				error = ppecfg_param_get_int(sub_params->data, sizeof(uint8_t), &nl_msg.config.action_info.yellow_dscp);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}
		}
	}

	/*
	 * Checking Min Max values for CIR, EIR, CBS, EBS
	 */
	if(!nl_msg.config.meter_unit) {
		if (nl_msg.config.committed_rate < PPECFG_POLICER_MIN_INFO_RATE_BYTE) {
			ppecfg_log_error("Minimum committed rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_BYTE);
			error = -EINVAL;
			goto done;
		}

		if (nl_msg.config.committed_rate > PPECFG_POLICER_MAX_INFO_RATE_BYTE) {
			ppecfg_log_error("Maximum committed rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_BYTE);
			error = -EINVAL;
			goto done;
		}

		if (nl_msg.config.peak_rate < PPECFG_POLICER_MIN_INFO_RATE_BYTE) {
			ppecfg_log_error("Minimum peak rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_BYTE);
			error = -EINVAL;
			goto done;
		}

		if (nl_msg.config.peak_rate > PPECFG_POLICER_MAX_INFO_RATE_BYTE) {
			ppecfg_log_error("Maximum peak rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_BYTE);
			error = -EINVAL;
			goto done;
		}

	} else {
		if (nl_msg.config.committed_rate < PPECFG_POLICER_MIN_INFO_RATE_FRAME) {
			ppecfg_log_error("Minimum committed rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_FRAME);
			error = -EINVAL;
			goto done;
		}

		if (nl_msg.config.committed_rate > PPECFG_POLICER_MAX_INFO_RATE_FRAME) {
			ppecfg_log_error("Maximum committed rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_FRAME);
			error = -EINVAL;
			goto done;
		}

		if (nl_msg.config.peak_rate < PPECFG_POLICER_MIN_INFO_RATE_FRAME) {
			ppecfg_log_error("Minimum peak rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_FRAME);
			error = -EINVAL;
			goto done;
		}

		if (nl_msg.config.peak_rate > PPECFG_POLICER_MAX_INFO_RATE_FRAME) {
			ppecfg_log_error("Maximum peak rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_FRAME);
			error = -EINVAL;
			goto done;
		}
	}

	if (!nl_msg.config.meter_unit) {
		if ((nl_msg.config.peak_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_BYTE) || (nl_msg.config.committed_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_BYTE)) {
			ppecfg_log_error("Maximum burst size : %d\n", PPECFG_POLICER_MAX_BURST_SIZE_BYTE);
			error = -EINVAL;
			goto done;
		}

	} else {
		if ((nl_msg.config.peak_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_FRAME) || (nl_msg.config.committed_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_FRAME)) {
			ppecfg_log_error("Maximum burst size : %d\n", PPECFG_POLICER_MAX_BURST_SIZE_FRAME);
			error = -EINVAL;
			goto done;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_policer_rule_add(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_policer_rule_flush()
 * 	Function to flush all policer rules
 */
static int ppecfg_policer_rule_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_policer_rule nl_policer_msg = {{0}};
	int error;

	nss_ppenl_policer_init_rule(&nl_policer_msg, NSS_PPE_POLICER_FLUSH_RULE_MSG);
	error = nss_ppenl_policer_rule_flush(&nl_policer_msg);

	if (error) {
		ppecfg_log_warn("Flush rule failed!\n");
	}

	return error;
}
