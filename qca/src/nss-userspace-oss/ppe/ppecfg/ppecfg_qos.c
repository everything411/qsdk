/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_qos.h"

static int ppecfg_qos_get_int_pri(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * qos_rule add parameters
 */
static struct ppecfg_param get_int_pri_params[PPECFG_QOS_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_DEV,"dev="),
	PPECFG_PARAM_INIT(PPECFG_QOS_HANDLE_ID, "handle_id="),
};


/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_qos_cmd' should also get updated
 * Supported Qos commands
 */
struct ppecfg_param ppecfg_qos_params[PPECFG_QOS_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=get_int_pri", get_int_pri_params, ppecfg_qos_get_int_pri),
};


/*
 * ppecfg_qos_get_int_pri()
 * Handle qos get int pri
 */
static int ppecfg_qos_get_int_pri(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_GET_INT_PRI);
	for (int index = PPECFG_QOS_DEV; index < PPECFG_QOS_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_DEV:
			/*
			 * Parse dev name for port qos
			 */
			sub_params = &param->sub_params[PPECFG_QOS_DEV];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.config.dev), &nl_msg.config.dev);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_QOS_HANDLE_ID:
			/*
			 * Parse rule id from user
			 */
			sub_params = &param->sub_params[PPECFG_QOS_HANDLE_ID];
			error = ppecfg_param_get_handle(sub_params->data, &nl_msg.config.handle_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		}
	}

	if (!(*nl_msg.config.dev)) {
		ppecfg_log_error("Please provide valid physical interface name\n");
		goto done;
	} else if (!nl_msg.config.handle_id) {
		ppecfg_log_error("Please provide a Qdisc handle ID/leaf class ID to fetch details\n");
		goto done;
	}
	/*
	 * send message
	 */
	error = nss_ppenl_qos_get_int_pri(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}
