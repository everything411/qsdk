/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_qos.h"

static int ppecfg_qos_get_int_pri(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_create_shaper(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_delete_shaper(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_create_interface_queues(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_flush_interface_queues(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_set_interface_shaper(struct ppecfg_param *param, struct ppecfg_param_in *match);
#ifdef NSS_PPE_PON_PORT_FEATURE
static int ppecfg_qos_get_tcont_stats(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_reset_tcont_credit(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_map_pq_to_tcont(struct ppecfg_param *param, struct ppecfg_param_in *match);
#endif
static int ppecfg_qos_set_queue_tm(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_set_queue_limit(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_set_interface_queue_ctrl(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_set_ucast_prio_map(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_qos_set_mcast_prio_map(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * qos_rule add parameters
 */
static struct ppecfg_param get_int_pri_params[PPECFG_QOS_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_DEV,"dev="),
	PPECFG_PARAM_INIT(PPECFG_QOS_HANDLE_ID, "handle_id="),
};

/*
 * interface_type_dev params
 */
static struct ppecfg_param interface_type_dev_params[PPECFG_QOS_INTERFACE_DEV_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_INTERFACE_DEV_NAME,"dev_name="),
};

/*
 * interface_type_tcont params
 */
static struct ppecfg_param interface_type_tcont_params[PPECFG_QOS_TCONT_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_TCONT_ID,"tcont_id="),
};

/*
 * reset_port_queues params
 */
static struct ppecfg_param flush_interface_queues_params[PPECFG_QOS_INTERFACE_FLUSH_MAX] = {
	PPECFG_PARAMARR_INIT(PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP, "DEV", interface_type_dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_QOS_INTERFACE_TYPE_TCONT, "TCONT", interface_type_tcont_params, ppecfg_param_iter_tbl),
};

/*
 * create_interface_queues params
 */
static struct ppecfg_param create_interface_queues_params[PPECFG_QOS_INTERFACE_MAX] = {
	PPECFG_PARAMARR_INIT(PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP, "DEV", interface_type_dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_QOS_INTERFACE_TYPE_TCONT, "TCONT", interface_type_tcont_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_QOS_INTERFACE_NUM_QUEUES, "num_queues="),
	PPECFG_PARAM_INIT(PPECFG_QOS_INTERFACE_QUEUE_TYPE, "queue_type="),
};

/*
 * set_interface_shaper params
 */
static struct ppecfg_param set_interface_shaper_params[PPECFG_QOS_SHAPER_INTERFACE_MAX] = {
	PPECFG_PARAMARR_INIT(PPECFG_QOS_SHAPER_INTERFACE_TYPE_PHYSICAL, "DEV", interface_type_dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_QOS_SHAPER_INTERFACE_TYPE_TCONT, "TCONT", interface_type_tcont_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_INTERFACE_SHAPER_NAME, "shaper_name="),
};

/*
 * delete_shaper params
 */
static struct ppecfg_param delete_shaper_params[PPECFG_QOS_SHAPER_DELETE_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_NAME,"name="),
};

/*
 * create_shaper params
 */
static struct ppecfg_param create_shaper_params[PPECFG_QOS_SHAPER_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_NAME,"name="),
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_CIR, "cir="),
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_EIR, "eir="),
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_CBS, "cbs="),
	PPECFG_PARAM_INIT(PPECFG_QOS_SHAPER_EBS, "ebs="),
};

#ifdef NSS_PPE_PON_PORT_FEATURE
/*
 * map_pq_to_tcont params
 */
static struct ppecfg_param map_pq_to_tcont_params[PPECFG_QOS_PQ_MAP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_PQ_MAP_QUEUE_ID, "queue_id="),
	PPECFG_PARAM_INIT(PPECFG_QOS_PQ_MAP_TCONT_ID,"tcont_id="),
};

/*
 * tcont_stats params
 */
static struct ppecfg_param tcont_stats_params[PPECFG_QOS_TCONT_STATS_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_TCONT_STATS_TCONT_ID, "tcont_id="),
};
#endif

/*
 * set_queue_tm params
 */
static struct ppecfg_param set_queue_tm_params[PPECFG_QOS_QUEUE_TM_MAX] = {
	PPECFG_PARAMARR_INIT(PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_PHYSICAL, "DEV", interface_type_dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_TCONT, "TCONT", interface_type_tcont_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_TM_QUEUE_TYPE, "queue_type="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_TM_ID, "queue_id="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_TM_PRIORITY, "priority="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_TM_WEIGHT, "weight="),
};

/*
 * set_queue_limit params
 */
static struct ppecfg_param set_queue_limit_params[PPECFG_QOS_QUEUE_LIMIT_MAX] = {
	PPECFG_PARAMARR_INIT(PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_PHYSICAL, "DEV", interface_type_dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_TCONT, "TCONT", interface_type_tcont_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_QUEUE_TYPE, "queue_type="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_ID, "queue_id="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_CEILING, "ceiling="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_COLOR_EN, "color_en="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_WRED_EN, "wred_en="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_GREEN_MIN_OFF, "green_min_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_YELLOW_MAX_OFF, "yellow_max_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_YELLOW_MIN_OFF, "yellow_min_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_RED_MAX_OFF, "red_max_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_RED_MIN_OFF, "red_min_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_GREEN_RESUME_OFF, "green_resume_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_YELLOW_RESUME_OFF, "yellow_resume_off="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_LIMIT_RED_RESUME_OFF, "red_resume_off="),
};

/*
 * set_interface_queue_ctrl params
 */
static struct ppecfg_param set_interface_queue_ctrl_params[PPECFG_QOS_QUEUE_CTRL_MAX] = {
	PPECFG_PARAMARR_INIT(PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_PHYSICAL, "DEV", interface_type_dev_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMARR_INIT(PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_TCONT, "TCONT", interface_type_tcont_params, ppecfg_param_iter_tbl),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_CTRL_MODE, "mode="),
	PPECFG_PARAM_INIT(PPECFG_QOS_QUEUE_CTRL_STATE, "state="),
};

/*
 * set_ucast_prio_map params
 */
static struct ppecfg_param set_ucast_prio_map_params[PPECFG_QOS_UCAST_PRIO_MAP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_UCAST_PRIO_MAP_DEV_NAME, "dev_name="),
	PPECFG_PARAM_INIT(PPECFG_QOS_UCAST_PRIO_MAP_PRIO_MAP, "prio_map="),
};

/*
 * set_mcast_prio_map params
 */
static struct ppecfg_param set_mcast_prio_map_params[PPECFG_QOS_MCAST_PRIO_MAP_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_QOS_MCAST_PRIO_MAP_DEV_NAME, "dev_name="),
	PPECFG_PARAM_INIT(PPECFG_QOS_MCAST_PRIO_MAP_PRIO_MAP, "prio_map="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_qos_cmd' should also get updated
 * Supported Qos commands
 */
struct ppecfg_param ppecfg_qos_params[PPECFG_QOS_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=get_int_pri", get_int_pri_params, ppecfg_qos_get_int_pri),
	PPECFG_PARAMLIST_INIT("cmd=create_shaper", create_shaper_params, ppecfg_qos_create_shaper),
	PPECFG_PARAMLIST_INIT("cmd=delete_shaper", delete_shaper_params, ppecfg_qos_delete_shaper),
	PPECFG_PARAMLIST_INIT("cmd=create_interface_queues", create_interface_queues_params, ppecfg_qos_create_interface_queues),
	PPECFG_PARAMLIST_INIT("cmd=flush_interface_queues", flush_interface_queues_params, ppecfg_qos_flush_interface_queues),
	PPECFG_PARAMLIST_INIT("cmd=set_interface_shaper", set_interface_shaper_params, ppecfg_qos_set_interface_shaper),
#ifdef NSS_PPE_PON_PORT_FEATURE
	PPECFG_PARAMLIST_INIT("cmd=map_pq_to_tcont", map_pq_to_tcont_params, ppecfg_qos_map_pq_to_tcont),
	PPECFG_PARAMLIST_INIT("cmd=get_tcont_stats", tcont_stats_params, ppecfg_qos_get_tcont_stats),
	PPECFG_PARAMLIST_INIT("cmd=reset_tcont_credit", tcont_stats_params, ppecfg_qos_reset_tcont_credit),
#endif
	PPECFG_PARAMLIST_INIT("cmd=set_queue_tm", set_queue_tm_params, ppecfg_qos_set_queue_tm),
	PPECFG_PARAMLIST_INIT("cmd=set_queue_limit", set_queue_limit_params, ppecfg_qos_set_queue_limit),
	PPECFG_PARAMLIST_INIT("cmd=set_interface_queue_ctrl", set_interface_queue_ctrl_params, ppecfg_qos_set_interface_queue_ctrl),
	PPECFG_PARAMLIST_INIT("cmd=set_ucast_prio_map", set_ucast_prio_map_params, ppecfg_qos_set_ucast_prio_map),
	PPECFG_PARAMLIST_INIT("cmd=set_mcast_prio_map", set_mcast_prio_map_params, ppecfg_qos_set_mcast_prio_map),
};

/*
 * ppecfg_qos_create_shaper()
 *	Handles qos create shaper
 */
static int ppecfg_qos_create_shaper(struct ppecfg_param *param, struct ppecfg_param_in *match)
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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_CREATE_SHAPER);

	for (int index = PPECFG_QOS_SHAPER_NAME; index < PPECFG_QOS_SHAPER_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_SHAPER_NAME:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_NAME];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.msg.shaper_info.name), &nl_msg.msg.shaper_info.name);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_SHAPER_CIR:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_CIR];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.shaper_info.cir), &nl_msg.msg.shaper_info.cir);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_SHAPER_EIR:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_EIR];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.shaper_info.eir), &nl_msg.msg.shaper_info.eir);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_SHAPER_CBS:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_CBS];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.shaper_info.cbs), &nl_msg.msg.shaper_info.cbs);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_SHAPER_EBS:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_EBS];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.shaper_info.ebs), &nl_msg.msg.shaper_info.ebs);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;
		}
	}

	if (!strlen(nl_msg.msg.shaper_info.name)) {
		ppecfg_log_error("Please provide valid shaper name\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_set_mcast_prio_map()
 * Handle qos set multicast priority map
 */
static int ppecfg_qos_set_mcast_prio_map(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	char *prio_map_str;
	char *token, *saveptr;
	int i = 0;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_SET_MCAST_PRIO_MAP);

	/*
	 * Process dev_name parameter
	 */
	sub_params = &param->sub_params[PPECFG_QOS_MCAST_PRIO_MAP_DEV_NAME];
	if (sub_params->valid) {
		error = ppecfg_param_get_str(sub_params->data,
				sizeof(nl_msg.msg.mcast_prio_map_info.if_data.interface.dev),
				&nl_msg.msg.mcast_prio_map_info.if_data.interface.dev);
		if (error < 0) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}
	} else {
		ppecfg_log_error("Device name is required\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Set interface type to physical
	 */
	nl_msg.msg.mcast_prio_map_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;

	/*
	 * Process prio_map parameter - comma-separated list of 16 values
	 */
	sub_params = &param->sub_params[PPECFG_QOS_MCAST_PRIO_MAP_PRIO_MAP];
	if (sub_params->valid) {
		prio_map_str = strdup(sub_params->data);
		if (!prio_map_str) {
			ppecfg_log_error("Memory allocation failed\n");
			error = -ENOMEM;
			goto done;
		}

		/*
		 * Parse comma-separated values
		 */
		token = strtok_r(prio_map_str, ",", &saveptr);
		while (token && i < PPECFG_QOS_MAX_PRIORITY) {
			int val = atoi(token);
			if (val < 0 || val > PPECFG_QOS_MCAST_QUEUE_CLASS_MAX) {
				ppecfg_log_error("Multicast queue class values must be between 0 and %d\n",
						PPECFG_QOS_MCAST_QUEUE_CLASS_MAX);
				free(prio_map_str);
				error = -EINVAL;
				goto done;
			}
			nl_msg.msg.mcast_prio_map_info.prio_map[i++] = val;
			token = strtok_r(NULL, ",", &saveptr);
		}

		free(prio_map_str);

		if (i != PPECFG_QOS_MAX_PRIORITY) {
			ppecfg_log_error("Priority map must contain exactly 16 values\n");
			error = -EINVAL;
			goto done;
		}
	} else {
		ppecfg_log_error("Priority map is required\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_qos_set_interface_queue_ctrl()
 *	Handle qos set interface queue control (enqueue/dequeue enable/disable)
 */
static int ppecfg_qos_set_interface_queue_ctrl(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	int count = 0;
	char *data;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL");
		return -EINVAL;
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error < 0) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_SET_INTERFACE_QUEUE_CTRL);

	for (int index = PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_PHYSICAL; index < PPECFG_QOS_QUEUE_CTRL_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_PHYSICAL:
			sub_params = param->sub_params[PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_PHYSICAL].sub_params;
			data = sub_params[PPECFG_QOS_INTERFACE_DEV_NAME].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.msg.queue_ctrl_info.if_data.interface.dev),
						&nl_msg.msg.queue_ctrl_info.if_data.interface.dev);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				nl_msg.msg.queue_ctrl_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
			}
			count++;
			break;

		case PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_TCONT:
			sub_params = param->sub_params[PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_TCONT].sub_params;
			data = sub_params[PPECFG_QOS_TCONT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(nl_msg.msg.queue_ctrl_info.if_data.interface.tcont_id),
						&nl_msg.msg.queue_ctrl_info.if_data.interface.tcont_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				nl_msg.msg.queue_ctrl_info.if_data.type = PPE_QOS_INTERFACE_TYPE_TCONT;
			}
			count++;
			break;

		case PPECFG_QOS_QUEUE_CTRL_MODE:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_CTRL_MODE];
			data = sub_params->data;

			if (!strcmp(data, "enqueue")) {
				nl_msg.msg.queue_ctrl_info.mode = PPE_QOS_QUEUE_CTRL_MODE_ENQUEUE;
			} else if (!strcmp(data, "dequeue")) {
				nl_msg.msg.queue_ctrl_info.mode = PPE_QOS_QUEUE_CTRL_MODE_DEQUEUE;
			} else {
				ppecfg_log_error("Invalid mode value. Use 'enqueue' or 'dequeue'\n");
				error = -EINVAL;
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_CTRL_STATE:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_CTRL_STATE];
			data = sub_params->data;

			if (!strcmp(data, "enable")) {
				nl_msg.msg.queue_ctrl_info.state = PPE_QOS_QUEUE_CTRL_STATE_ENABLE;
			} else if (!strcmp(data, "disable")) {
				nl_msg.msg.queue_ctrl_info.state = PPE_QOS_QUEUE_CTRL_STATE_DISABLE;
			} else if (!strcmp(data, "drop")) {
				nl_msg.msg.queue_ctrl_info.state = PPE_QOS_QUEUE_CTRL_STATE_DROP;
			} else {
				ppecfg_log_error("Invalid state value. Use 'enable', 'disable', or 'drop'\n");
				error = -EINVAL;
				goto done;
			}
			break;
		}
	}

	if (count > 1) {
		ppecfg_log_error("Only one interface type is allowed\n");
		error = -EINVAL;
		goto done;
	}

	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_qos_delete_shaper()
 *	Handles qos delete shaper
 */
static int ppecfg_qos_delete_shaper(struct ppecfg_param *param, struct ppecfg_param_in *match)
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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_DELETE_SHAPER);

	for (int index = PPECFG_QOS_SHAPER_NAME; index < PPECFG_QOS_SHAPER_DELETE_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_SHAPER_NAME:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_NAME];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.msg.shaper_info.name), &nl_msg.msg.shaper_info.name);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;
		}
	}

	if (!strlen(nl_msg.msg.shaper_info.name)) {
		ppecfg_log_error("Please provide valid shaper name\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_create_interface_queues()
 *	Handle qos create interface queues
 */
static int ppecfg_qos_create_interface_queues(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	int count = 0;
	char *data;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_CREATE_INTERFACE_QUEUES);

	for (int index = PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP; index < PPECFG_QOS_INTERFACE_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP:
			sub_params = param->sub_params[PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP].sub_params;
			data = sub_params[PPECFG_QOS_INTERFACE_DEV_NAME].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.msg.if_info.if_data.interface.dev), &nl_msg.msg.if_info.if_data.interface.dev);
				if (error) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.if_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
			count++;
			break;

		case PPECFG_QOS_INTERFACE_TYPE_TCONT:
			sub_params = param->sub_params[PPECFG_QOS_INTERFACE_TYPE_TCONT].sub_params;

			data = sub_params[PPECFG_QOS_TCONT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(nl_msg.msg.if_info.if_data.interface.tcont_id), &nl_msg.msg.if_info.if_data.interface.tcont_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.if_info.if_data.type = PPE_QOS_INTERFACE_TYPE_TCONT;
			count++;
			break;

		case PPECFG_QOS_INTERFACE_NUM_QUEUES:
			sub_params = &param->sub_params[PPECFG_QOS_INTERFACE_NUM_QUEUES];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.if_info.num_queues);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_QOS_INTERFACE_QUEUE_TYPE:
			/*
			 * parse optional queue_type from user_config, default ucast
			 */
			char qtype[10];
			error = ppecfg_param_get_str(sub_params->data, sizeof(qtype), qtype);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			nl_msg.msg.if_info.queue_type = PPE_QOS_QUEUE_TYPE_UCAST;
			data = sub_params->data;
			if (data) {
				if (!strcmp(data, "ucast")) {
					nl_msg.msg.if_info.queue_type = PPE_QOS_QUEUE_TYPE_UCAST;
				} else if (!strcmp(data, "mcast")) {
					nl_msg.msg.if_info.queue_type = PPE_QOS_QUEUE_TYPE_MCAST;
				} else {
					ppecfg_log_error("Only ucast/mcast string is allowed\n");
					error = -EINVAL;
					goto done;
				}
			}
			break;
		}
	}

	/*
	 * Check if only one interface type is configured by the user.
	 */
	if (count > 1) {
		ppecfg_log_error("Only one interface type is allowed\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}

done:
	return error;
}

/*
 * ppecfg_qos_flush_interface_queues()
 *	Handles qos interface flush queues
 */
static int ppecfg_qos_flush_interface_queues(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	int count = 0;
	char *data;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_FLUSH_INTERFACE_QUEUES);

	for (int index = PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP; index < PPECFG_QOS_INTERFACE_FLUSH_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP:
			sub_params = param->sub_params[PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP].sub_params;

			data = sub_params[PPECFG_QOS_INTERFACE_DEV_NAME].data;
			if (data) {
					error = ppecfg_param_get_str(data, sizeof(nl_msg.msg.if_info.if_data.interface.dev), &nl_msg.msg.if_info.if_data.interface.dev);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.if_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
			count++;
			break;

		case PPECFG_QOS_INTERFACE_TYPE_TCONT:
			sub_params = param->sub_params[PPECFG_QOS_INTERFACE_TYPE_TCONT].sub_params;

			data = sub_params[PPECFG_QOS_TCONT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(nl_msg.msg.if_info.if_data.interface.tcont_id), &nl_msg.msg.if_info.if_data.interface.tcont_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.if_info.if_data.type = PPE_QOS_INTERFACE_TYPE_TCONT;
			count++;
			break;
		}
	}

	/*
	 * Check if only one interface type is configured by the user.
	 */
	if (count > 1) {
		ppecfg_log_error("Only one interface type is allowed\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_set_interface_shaper()
 *	Handle qos interface shaper setting
 */
static int ppecfg_qos_set_interface_shaper(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	int count = 0;
	char *data;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_SET_INTERFACE_SHAPER);

	for (int index = PPECFG_QOS_SHAPER_INTERFACE_TYPE_PHYSICAL; index < PPECFG_QOS_SHAPER_INTERFACE_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_SHAPER_INTERFACE_TYPE_PHYSICAL:
			sub_params = param->sub_params[PPECFG_QOS_SHAPER_INTERFACE_TYPE_PHYSICAL].sub_params;

			data = sub_params[PPECFG_QOS_INTERFACE_DEV_NAME].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.msg.if_shaper_info.if_data.interface.dev), &nl_msg.msg.if_shaper_info.if_data.interface.dev);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.if_shaper_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
			count++;
			break;

		case PPECFG_QOS_SHAPER_INTERFACE_TYPE_TCONT:
			sub_params = param->sub_params[PPECFG_QOS_SHAPER_INTERFACE_TYPE_TCONT].sub_params;

			data = sub_params[PPECFG_QOS_TCONT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(nl_msg.msg.if_shaper_info.if_data.interface.tcont_id), &nl_msg.msg.if_shaper_info.if_data.interface.tcont_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.if_shaper_info.if_data.type = PPE_QOS_INTERFACE_TYPE_TCONT;
			count++;
			break;

		case PPECFG_QOS_SHAPER_INTERFACE_SHAPER_NAME:
			sub_params = &param->sub_params[PPECFG_QOS_SHAPER_INTERFACE_SHAPER_NAME];
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.msg.if_shaper_info.shaper_name), &nl_msg.msg.if_shaper_info.shaper_name);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;
		}
	}

	/*
	 * Check if only one interface type is configured by the user.
	 */
	if (count > 1) {
		ppecfg_log_error("Only one interface type is allowed\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

#ifdef NSS_PPE_PON_PORT_FEATURE
/*
 * ppecfg_qos_map_pq_to_tcont()
 * 	Handle qos mapping of PQ to Tcont
 */
static int ppecfg_qos_map_pq_to_tcont(struct ppecfg_param *param, struct ppecfg_param_in *match)
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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_MAP_PQ_TO_TCONT);

	for (int index = PPECFG_QOS_PQ_MAP_QUEUE_ID; index < PPECFG_QOS_PQ_MAP_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_PQ_MAP_QUEUE_ID:
			sub_params = &param->sub_params[PPECFG_QOS_PQ_MAP_QUEUE_ID];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.pq_info.queue_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_QOS_PQ_MAP_TCONT_ID:
			sub_params = &param->sub_params[PPECFG_QOS_PQ_MAP_TCONT_ID];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.pq_info.tcont_id), &nl_msg.msg.pq_info.tcont_id);
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
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_get_tcont_stats()
 * Handle qos get Tcont stats
 */
static int ppecfg_qos_get_tcont_stats(struct ppecfg_param *param, struct ppecfg_param_in *match)
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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_GET_TCONT_STATS);

	for (int index = PPECFG_QOS_TCONT_STATS_TCONT_ID; index < PPECFG_QOS_TCONT_STATS_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_TCONT_STATS_TCONT_ID:
			sub_params = &param->sub_params[PPECFG_QOS_TCONT_STATS_TCONT_ID];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.stats_info.tcont_id), &nl_msg.msg.stats_info.tcont_id);
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
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_reset_tcont_credit()
 * Handle qos reset tcont credit request
 */
static int ppecfg_qos_reset_tcont_credit(struct ppecfg_param *param, struct ppecfg_param_in *match)
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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_RESET_TCONT_CREDIT);

	for (int index = PPECFG_QOS_TCONT_STATS_TCONT_ID; index < PPECFG_QOS_TCONT_STATS_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_TCONT_STATS_TCONT_ID:
			sub_params = &param->sub_params[PPECFG_QOS_TCONT_STATS_TCONT_ID];
			error = ppecfg_param_get_int(sub_params->data, sizeof(nl_msg.msg.stats_info.tcont_id), &nl_msg.msg.stats_info.tcont_id);
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
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}
#endif

/*
 * ppecfg_qos_set_queue_tm()
 * 	Handle qos set queue tm
 */
static int ppecfg_qos_set_queue_tm(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	int count = 0;
	char *data;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_SET_QUEUE_TM);

	/*
	 * setting default priority and weight for optional params
	 */
	nl_msg.msg.tm_info.priority = 0;
	nl_msg.msg.tm_info.weight = 1;

	for (int index = PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_PHYSICAL; index < PPECFG_QOS_QUEUE_TM_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_PHYSICAL:
			sub_params = param->sub_params[PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_PHYSICAL].sub_params;

			data = sub_params[PPECFG_QOS_INTERFACE_DEV_NAME].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.msg.tm_info.if_data.interface.dev), &nl_msg.msg.tm_info.if_data.interface.dev);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.tm_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
			count++;
			break;

		case PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_TCONT:
			sub_params = param->sub_params[PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_TCONT].sub_params;

			data = sub_params[PPECFG_QOS_TCONT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(nl_msg.msg.tm_info.if_data.interface.tcont_id), &nl_msg.msg.tm_info.if_data.interface.tcont_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.tm_info.if_data.type = PPE_QOS_INTERFACE_TYPE_TCONT;
			count++;
			break;

		case PPECFG_QOS_QUEUE_TM_QUEUE_TYPE:
			/*
			 * parse optional queue_type from user_config, default ucast
			 */
			char qtype[10];
			error = ppecfg_param_get_str(sub_params->data, sizeof(qtype), qtype);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			nl_msg.msg.tm_info.queue_type = PPE_QOS_QUEUE_TYPE_UCAST;
			data = sub_params->data;
			if (data) {
				if (!strcmp(data, "ucast")) {
					nl_msg.msg.tm_info.queue_type = PPE_QOS_QUEUE_TYPE_UCAST;
				} else if (!strcmp(data, "mcast")) {
					nl_msg.msg.tm_info.queue_type = PPE_QOS_QUEUE_TYPE_MCAST;
				} else {
					ppecfg_log_error("Only ucast/mcast string is allowed\n");
					error = -EINVAL;
					goto done;
				}
			}
			break;

		case PPECFG_QOS_QUEUE_TM_ID:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_TM_ID];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.tm_info.queue_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_QOS_QUEUE_TM_PRIORITY:
			/* optional */
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.tm_info.priority);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		case PPECFG_QOS_QUEUE_TM_WEIGHT:
			/* optional */
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.tm_info.weight);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;
		}
	}

	/*
	 * Check if only one interface type is configured by the user.
	 */
	if (count > 1) {
		ppecfg_log_error("Only one interface type is allowed\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_set_queue_limit()
 * 	Handle qos set queue limit
 */
static int ppecfg_qos_set_queue_limit(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	int count = 0;
	char *data;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_SET_QUEUE_LIMIT);

	/*
	 * setting default priority and weight for optional params
	 */
	for (int index = PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_PHYSICAL; index < PPECFG_QOS_QUEUE_LIMIT_MAX; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
		case PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_PHYSICAL:
			sub_params = param->sub_params[PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_PHYSICAL].sub_params;

			data = sub_params[PPECFG_QOS_INTERFACE_DEV_NAME].data;
			if (data) {
				error = ppecfg_param_get_str(data, sizeof(nl_msg.msg.limit_info.if_data.interface.dev), &nl_msg.msg.limit_info.if_data.interface.dev);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.limit_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;
			count++;
			break;

		case PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_TCONT:
			sub_params = param->sub_params[PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_TCONT].sub_params;

			data = sub_params[PPECFG_QOS_TCONT_ID].data;
			if (data) {
				error = ppecfg_param_get_int(data, sizeof(nl_msg.msg.limit_info.if_data.interface.tcont_id), &nl_msg.msg.limit_info.if_data.interface.tcont_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
			}

			nl_msg.msg.limit_info.if_data.type = PPE_QOS_INTERFACE_TYPE_TCONT;
			count++;
			break;

		case PPECFG_QOS_QUEUE_LIMIT_QUEUE_TYPE:
			/*
			 * parse optional queue_type from user_config, default ucast
			 */
			char qtype[10];
			error = ppecfg_param_get_str(sub_params->data, sizeof(qtype), qtype);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			nl_msg.msg.limit_info.queue_type = PPE_QOS_QUEUE_TYPE_UCAST;
			data = sub_params->data;
			if (data) {
				if (!strcmp(data, "ucast")) {
					nl_msg.msg.limit_info.queue_type = PPE_QOS_QUEUE_TYPE_UCAST;
				} else if (!strcmp(data, "mcast")) {
					nl_msg.msg.limit_info.queue_type = PPE_QOS_QUEUE_TYPE_MCAST;
				} else {
					ppecfg_log_error("Only ucast/mcast string is allowed\n");
					error = -EINVAL;
					goto done;
				}			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_ID:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_ID];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.queue_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_CEILING:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_CEILING];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.ceiling);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_COLOR_EN:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_COLOR_EN];
			error = ppecfg_param_get_bool(sub_params->data, &nl_msg.msg.limit_info.color_en);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_WRED_EN:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_WRED_EN];
			error = ppecfg_param_get_bool(sub_params->data, &nl_msg.msg.limit_info.wred_en);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_GREEN_MIN_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_GREEN_MIN_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.green_min_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_YELLOW_MAX_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_YELLOW_MAX_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.yellow_max_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_YELLOW_MIN_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_YELLOW_MIN_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.yellow_min_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_RED_MAX_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_RED_MAX_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.red_max_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_RED_MIN_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_RED_MIN_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.red_min_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_GREEN_RESUME_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_GREEN_RESUME_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.green_resume_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_YELLOW_RESUME_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_YELLOW_RESUME_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.yellow_resume_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;

		case PPECFG_QOS_QUEUE_LIMIT_RED_RESUME_OFF:
			sub_params = &param->sub_params[PPECFG_QOS_QUEUE_LIMIT_RED_RESUME_OFF];
			error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.msg.limit_info.red_resume_off);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}
			break;
		}
	}

	/*
	 * Check if only one interface type is configured by the user.
	 */
	if (count > 1) {
		ppecfg_log_error("Only one interface type is allowed\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_get_int_pri()
 * 	Handle qos get int pri
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
			error = ppecfg_param_get_str(sub_params->data, sizeof(nl_msg.msg.config.dev), &nl_msg.msg.config.dev);
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
			error = ppecfg_param_get_handle(sub_params->data, &nl_msg.msg.config.handle_id);
			if (error < 0) {
				ppecfg_log_arg_error(sub_params);
				goto done;
			}

			break;

		}
	}

	if (!(*nl_msg.msg.config.dev)) {
		ppecfg_log_error("Please provide valid physical interface name\n");
		error = -EINVAL;
		goto done;
	} else if (!nl_msg.msg.config.handle_id) {
		ppecfg_log_error("Please provide a Qdisc handle ID/leaf class ID to fetch details\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
done:
	return error;
}

/*
 * ppecfg_qos_set_ucast_prio_map()
 * Handle qos set unicast priority map
 */
static int ppecfg_qos_set_ucast_prio_map(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_qos_req nl_msg = {{0}};
	int error;
	struct ppecfg_param *sub_params;
	char *prio_map_str;
	char *token, *saveptr;
	int i = 0;

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

	nss_ppenl_qos_init_req(&nl_msg, NSS_PPE_QOS_SET_UCAST_PRIO_MAP);

	/*
	 * Process dev_name parameter
	 */
	sub_params = &param->sub_params[PPECFG_QOS_UCAST_PRIO_MAP_DEV_NAME];
	if (sub_params->valid) {
		error = ppecfg_param_get_str(sub_params->data,
				sizeof(nl_msg.msg.ucast_prio_map_info.if_data.interface.dev),
				&nl_msg.msg.ucast_prio_map_info.if_data.interface.dev);
		if (error < 0) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}
	} else {
		ppecfg_log_error("Device name is required\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Set interface type to physical
	 */
	nl_msg.msg.ucast_prio_map_info.if_data.type = PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP;

	/*
	 * Process prio_map parameter - comma-separated list of 16 values
	 */
	sub_params = &param->sub_params[PPECFG_QOS_UCAST_PRIO_MAP_PRIO_MAP];
	if (sub_params->valid) {
		prio_map_str = strdup(sub_params->data);
		if (!prio_map_str) {
			ppecfg_log_error("Memory allocation failed\n");
			error = -ENOMEM;
			goto done;
		}

		/*
		 * Parse comma-separated values
		 */
		token = strtok_r(prio_map_str, ",", &saveptr);
		while (token && i < PPECFG_QOS_MAX_PRIORITY) {
			int val = atoi(token);
			if (val < 0 || val > 7) {
				ppecfg_log_error("Unicast priority map values must be between 0 and 7\n");
				free(prio_map_str);
				error = -EINVAL;
				goto done;
			}
			nl_msg.msg.ucast_prio_map_info.prio_map[i++] = val;
			token = strtok_r(NULL, ",", &saveptr);
		}

		free(prio_map_str);

		if (i != PPECFG_QOS_MAX_PRIORITY) {
			ppecfg_log_error("Priority map must contain exactly 16 values\n");
			error = -EINVAL;
			goto done;
		}
	} else {
		ppecfg_log_error("Priority map is required\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_qos_send_req(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}

done:
	return error;
}
