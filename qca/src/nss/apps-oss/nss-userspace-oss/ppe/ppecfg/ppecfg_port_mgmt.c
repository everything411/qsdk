/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG PORT_MGMT handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include <nss_ppenl_port_mgmt_if.h>
#include <nss_ppenl_port_mgmt_api.h>
#include <ppe_port_mgmt.h>
#include "ppecfg_param.h"
#include "ppecfg_port_mgmt.h"

#define MANDATORY_FIELDS_MASK 0x01
#define OPTIONAL_FIELDS_MASK  0x7E
#define PORT_FIELDS_MASK      0x06

static int ppecfg_port_mgmt_port_isolation_set(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_act_ctrl_set(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_isol_default(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_mac_lrn_limit_set(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_mac_filter_set(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_mac_filter_clear(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_omci_port_add(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_omci_port_del(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_port_mgmt_omci_port_flush(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * PORT_MGMT mac learn limit set parameters
 */
static struct ppecfg_param port_lrn_limit_set_params[PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_ENABLE, "mac_learn_limit_en="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_PORT_NAME, "port_name="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_PORT_LEARN_LIMIT, "port_learn_limit="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_LRN_EXCEED_ACTION, "lrn_exceed_action="),
};

/*
 * PORT_MGMT port isolation set parameters
 */
static struct ppecfg_param port_isol_set_params[PPECFG_PORT_MGMT_PORT_ISOL_SET_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_PORT_ISOL_SET_PORT_NAME, "port_name="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_PORT_ISOL_SET_OMCI_ID, "omci_id="),
};

/*
 * PORT_MGMT port action control set parameters
 */
static struct ppecfg_param act_ctrl_set_params[PPECFG_PORT_MGMT_ACT_CTRL_SET_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_ACT_CTRL_SET_MC_ISOL_EN, "mc_isol_en="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_ACT_CTRL_SET_BC_ISOL_EN, "bc_isol_en="),
};

/*
 * PORT_MGMT port isolation get parameters
 */
static struct ppecfg_param mac_filter_set_params[PPECFG_PORT_MGMT_MAC_FILTER_SET_MAX] = {
        PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_MAC_FILTER_SET_BLOCK, "mac_addr="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_MAC_FILTER_SET_FID_NAME, "fid_name="),
};

/*
 * PORT_MGMT port isolation get parameters
 */
static struct ppecfg_param mac_filter_clr_params[PPECFG_PORT_MGMT_MAC_FILTER_CLR_MAX] = {
        PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_MAC_FILTER_CLR_BLOCK, "mac_addr="),
        PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_MAC_FILTER_CLR_FID_NAME, "fid_name="),
};

/*
 * PORT_MGMT OMCI port add/del parameters
 */
static struct ppecfg_param omci_port_set_params[PPECFG_PORT_MGMT_OMCI_PORT_SET_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PORT_MGMT_OMCI_PORT_SET_DEV, "dev="),
};

/*
 * NOTE: whenever this table is updated, the 'enum ppecfg_port_mgmt_cmd' should also get updated
 */
struct ppecfg_param ppecfg_port_mgmt_params[PPECFG_PORT_MGMT_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=port_isol_set", port_isol_set_params, ppecfg_port_mgmt_port_isolation_set),
	PPECFG_PARAMLIST_INIT("cmd=act_ctrl_set", act_ctrl_set_params, ppecfg_port_mgmt_act_ctrl_set),
	PPECFG_PARAMFUNC_INIT("cmd=isol_default", ppecfg_port_mgmt_isol_default),
	PPECFG_PARAMLIST_INIT("cmd=mac_learn_limit_set", port_lrn_limit_set_params, ppecfg_port_mgmt_mac_lrn_limit_set),
	PPECFG_PARAMLIST_INIT("cmd=mac_filter_set", mac_filter_set_params, ppecfg_port_mgmt_mac_filter_set),
	PPECFG_PARAMLIST_INIT("cmd=mac_filter_clear", mac_filter_clr_params, ppecfg_port_mgmt_mac_filter_clear),
	PPECFG_PARAMLIST_INIT("cmd=omci_port_add", omci_port_set_params, ppecfg_port_mgmt_omci_port_add),
	PPECFG_PARAMLIST_INIT("cmd=omci_port_del", omci_port_set_params, ppecfg_port_mgmt_omci_port_del),
	PPECFG_PARAMFUNC_INIT("cmd=omci_port_flush", ppecfg_port_mgmt_omci_port_flush),
};

/*
 * ppecfg_port_mgmt_port_isolation_set()
 *	Handle PORT_MGMT port isolation set
 */
static int ppecfg_port_mgmt_port_isolation_set(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
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

	/*
	 * Initialize the PORT_MGMT message
	 */
	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_PORT_ISOL_SET_MSG);

	/*
	 * extract port name
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_PORT_ISOL_SET_PORT_NAME];
	error = ppecfg_param_get_str(sub_params->data, IFNAMSIZ, &nl_msg.isol.port_name);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * extract OMCI ID
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_PORT_ISOL_SET_OMCI_ID];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.isol.omci_id);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_port_isolation_set(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}
	ppecfg_log_info("Configured successfully, Port_name: %s Omci_ID: %d\n", nl_msg.isol.port_name, nl_msg.isol.omci_id);
done:
	return error;
}

/*
 * ppecfg_port_mgmt_act_ctrl_set()
 *	Handle PORT_MGMT port action control set
 */
static int ppecfg_port_mgmt_act_ctrl_set(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
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

	/*
	 * Initialize the PORT_MGMT message
	 */
	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_ACT_CTRL_SET_MSG);

	/*
	 * extract MC Isolation Enable
	 */
	struct ppecfg_param *sub_params = &param->sub_params[PPECFG_PORT_MGMT_ACT_CTRL_SET_MC_ISOL_EN];
	if (sub_params->valid != 0) {
		error = ppecfg_param_get_bool(sub_params->data, &nl_msg.isol.mc_isol_en);
		if (error) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}
	}

	/*
	 * extract BC Isolation Enable
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_ACT_CTRL_SET_BC_ISOL_EN];
	if (sub_params->valid != 0) {
		error = ppecfg_param_get_bool(sub_params->data, &nl_msg.isol.bc_isol_en);
		if (error) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_act_ctrl_set(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

	ppecfg_log_info("Configured successfully, BC_ISOL_EN: %d MC_ISOL_EN: %d\n", nl_msg.isol.bc_isol_en, nl_msg.isol.mc_isol_en);
done:
	return error;
}

static int ppecfg_port_mgmt_isol_default(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	int error;

	/*
	 * Initialize the PORT_MGMT message
	 */
	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_PORT_ISOL_DEF_MSG);

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_isol_default(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

	ppecfg_log_info("Isol is set to default successfully\n");
done:
	return error;
}

/*
 * ppecfg_port_mgmt_mac_lrn_limit_set()
 *      Handle PORT_MGMT mac learn limit set
 */
static int ppecfg_port_mgmt_mac_lrn_limit_set(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
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

	/*
	 * Initialize the PORT_MGMT message
	 */
	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_MAC_LRN_LIMIT_SET_MSG);

	/*
	 * extract enable flag
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_ENABLE];
	error = ppecfg_param_get_bool(sub_params->data, &nl_msg.mac_lrn_limit.port_learn_limit_en);
	if (error) {
                ppecfg_log_arg_error(sub_params);
                goto done;
        }

	/*
	 * extract port name
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_PORT_NAME];
	error = ppecfg_param_get_str(sub_params->data, IFNAMSIZ, &nl_msg.mac_lrn_limit.port_name);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * extract port learn limit
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_PORT_LEARN_LIMIT];
	error = ppecfg_param_get_int(sub_params->data, sizeof(uint32_t), &nl_msg.mac_lrn_limit.port_learn_limit);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	/*
	 * extract learn exceed action
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_LRN_EXCEED_ACTION];
	char fwd_cmd[10];
	if (sub_params->valid != 0) {
		error = ppecfg_param_get_str(sub_params->data, sizeof(fwd_cmd), &fwd_cmd);
		if (error) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}

		if (strcmp("FWD", fwd_cmd) == 0) {
			nl_msg.mac_lrn_limit.lrn_exceed_action = PPE_PORT_MGMT_FWD_CMD_FWD;
		} else if (strcmp("DROP", fwd_cmd) == 0) {
			nl_msg.mac_lrn_limit.lrn_exceed_action = PPE_PORT_MGMT_FWD_CMD_DROP;
		} else if (strcmp("COPY", fwd_cmd) == 0) {
			nl_msg.mac_lrn_limit.lrn_exceed_action = PPE_PORT_MGMT_FWD_CMD_COPY;
		} else if (strcmp("REDIR", fwd_cmd) == 0) {
			nl_msg.mac_lrn_limit.lrn_exceed_action = PPE_PORT_MGMT_FWD_CMD_REDIR;
		} else {
			ppecfg_log_info("Valid Inputs: [FWD][DROP][COPY][REDIR]\n");
			goto done;
		}

		nl_msg.mac_lrn_limit.lrn_exceed_action_en = true;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_mac_lrn_limit_set(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

	ppecfg_log_info("Configured successfully, Port_name: %s learn_limit: %d exceed_action %d\n", nl_msg.mac_lrn_limit.port_name, nl_msg.mac_lrn_limit.port_learn_limit, nl_msg.mac_lrn_limit.lrn_exceed_action);
done:
	return error;
}

/*
 * ppecfg_port_mgmt_mac_filter_set()
 *      Handle PORT_MGMT mac filter set
 */
static int ppecfg_port_mgmt_mac_filter_set(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
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

	/*
	 * Initialize the PORT_MGMT message
	 */
	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_MAC_FILTER_SET_MSG);

	/*
	 * extract mac address
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_MAC_FILTER_SET_BLOCK];
	error = ppecfg_param_verify_mac(sub_params->data, nl_msg.mac_filter.mac);
	if (!error) {
                ppecfg_log_arg_error(sub_params);
		goto done;
	}
	/*
	 * extract vsi name
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_MAC_FILTER_SET_FID_NAME];
	if (sub_params->valid != 0) {
		error = ppecfg_param_get_str(sub_params->data, IFNAMSIZ, &nl_msg.mac_filter.fid_name);
		if (error) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}

		nl_msg.mac_filter.fid_valid = true;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_mac_filter_set(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

	ppecfg_log_info("Configured successfully\n");
done:
	return error;
}

/*
 * ppecfg_port_mgmt_omci_port_add()
 *	Handle PORT_MGMT OMCI port add
 */
static int ppecfg_port_mgmt_omci_port_add(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL\n");
		return -EINVAL;
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG);

	sub_params = &param->sub_params[PPECFG_PORT_MGMT_OMCI_PORT_SET_DEV];
	error = ppecfg_param_get_str(sub_params->data, IFNAMSIZ, nl_msg.omci_port.port_name);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	error = nss_ppenl_port_mgmt_omci_port_add(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send OMCI port add message\n");
		goto done;
	}

	ppecfg_log_info("OMCI port add configured successfully\n");
done:
	return error;
}

/*
 * ppecfg_port_mgmt_omci_port_del()
 *	Handle PORT_MGMT OMCI port del
 */
static int ppecfg_port_mgmt_omci_port_del(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
	int error;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL\n");
		return -EINVAL;
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG);

	sub_params = &param->sub_params[PPECFG_PORT_MGMT_OMCI_PORT_SET_DEV];
	error = ppecfg_param_get_str(sub_params->data, IFNAMSIZ, nl_msg.omci_port.port_name);
	if (error) {
		ppecfg_log_arg_error(sub_params);
		goto done;
	}

	error = nss_ppenl_port_mgmt_omci_port_del(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send OMCI port del message\n");
		goto done;
	}

	ppecfg_log_info("OMCI port del configured successfully\n");
done:
	return error;
}

/*
 * ppecfg_port_mgmt_omci_port_flush()
 *	Handle PORT_MGMT OMCI port flush
 */
static int ppecfg_port_mgmt_omci_port_flush(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	int error;

	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG);

	error = nss_ppenl_port_mgmt_omci_port_flush(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send OMCI port flush message\n");
		goto done;
	}

	ppecfg_log_info("OMCI port flush configured successfully\n");
done:
	return error;
}

/*
 * ppecfg_port_mgmt_mac_filter_clear()
 *      Handle PORT_MGMT mac filter clear
 */
static int ppecfg_port_mgmt_mac_filter_clear(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_port_mgmt_info nl_msg = {{0}};
	struct ppecfg_param *sub_params;
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

	/*
	 * Initialize the PORT_MGMT message
	 */
	nss_ppenl_rule_port_mgmt_init(&nl_msg, NSS_PPE_PORT_MGMT_MAC_FILTER_CLR_MSG);

	/*
	 * extract mac address
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_MAC_FILTER_CLR_BLOCK];
	error = ppecfg_param_verify_mac(sub_params->data, nl_msg.mac_filter.mac);
	if (!error) {
                ppecfg_log_arg_error(sub_params);
		goto done;
	}
	/*
	 * extract vsi name
	 */
	sub_params = &param->sub_params[PPECFG_PORT_MGMT_MAC_FILTER_CLR_FID_NAME];
	if (sub_params->valid != 0) {
		error = ppecfg_param_get_str(sub_params->data, IFNAMSIZ, &nl_msg.mac_filter.fid_name);
		if (error) {
			ppecfg_log_arg_error(sub_params);
			goto done;
		}

		nl_msg.mac_filter.fid_valid = true;
	}

	/*
	 * send message
	 */
	error = nss_ppenl_port_mgmt_mac_filter_clear(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message\n");
		goto done;
	}

	ppecfg_log_info("Configured successfully\n");
done:
	return error;
}
