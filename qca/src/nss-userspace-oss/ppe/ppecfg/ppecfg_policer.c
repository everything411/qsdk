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
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_MODE,"meter_mode="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_UNIT,"meter_unit="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COMMITTED_RATE, "committed_rate="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COMMITTED_BURST_SIZE, "committed_burst_size="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_PEAK_RATE, "peak_rate="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_PEAK_BURST_SIZE, "peak_burst_size="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_METER_ENABLE, "meter_enable="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COUPLE_ENABLE, "couple_enable="),
	PPECFG_PARAM_INIT(PPECFG_POLICER_RULE_ADD_COLOUR_AWARE, "colour_aware_enable="),
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

	for (int index = PPECFG_POLICER_RULE_ADD_IS_PORT_POLICER; index <= PPECFG_POLICER_RULE_ADD_YELLOW_DSCP; index++) {
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
			goto done;
		}

		if (nl_msg.config.committed_rate > PPECFG_POLICER_MAX_INFO_RATE_BYTE) {
			ppecfg_log_error("Maximum committed rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_BYTE);
			goto done;
		}

		if (nl_msg.config.peak_rate < PPECFG_POLICER_MIN_INFO_RATE_BYTE) {
			ppecfg_log_error("Minimum peak rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_BYTE);
			goto done;
		}

		if (nl_msg.config.peak_rate > PPECFG_POLICER_MAX_INFO_RATE_BYTE) {
			ppecfg_log_error("Maximum peak rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_BYTE);
			goto done;
		}

	} else {
		if (nl_msg.config.committed_rate < PPECFG_POLICER_MIN_INFO_RATE_FRAME) {
			ppecfg_log_error("Minimum committed rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_FRAME);
			goto done;
		}

		if (nl_msg.config.committed_rate > PPECFG_POLICER_MAX_INFO_RATE_FRAME) {
			ppecfg_log_error("Maximum committed rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_FRAME);
			goto done;
		}

		if (nl_msg.config.peak_rate < PPECFG_POLICER_MIN_INFO_RATE_FRAME) {
			ppecfg_log_error("Minimum peak rate : %d\n", PPECFG_POLICER_MIN_INFO_RATE_FRAME);
			goto done;
		}

		if (nl_msg.config.peak_rate > PPECFG_POLICER_MAX_INFO_RATE_FRAME) {
			ppecfg_log_error("Maximum peak rate : %d\n", PPECFG_POLICER_MAX_INFO_RATE_FRAME);
			goto done;
		}
	}

	if (!nl_msg.config.meter_unit) {
		if ((nl_msg.config.peak_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_BYTE) || (nl_msg.config.committed_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_BYTE)) {
			ppecfg_log_error("Maximum burst size : %d\n", PPECFG_POLICER_MAX_BURST_SIZE_BYTE);
			goto done;
		}

	} else {
		if ((nl_msg.config.peak_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_FRAME) || (nl_msg.config.committed_burst_size > PPECFG_POLICER_MAX_BURST_SIZE_FRAME)) {
			ppecfg_log_error("Maximum burst size : %d\n", PPECFG_POLICER_MAX_BURST_SIZE_FRAME);
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
