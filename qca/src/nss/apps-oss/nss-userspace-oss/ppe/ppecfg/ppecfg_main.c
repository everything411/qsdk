/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppecfg_hlos.h"
#include "ppecfg_param.h"
#include "ppecfg_family.h"
#include "ppecfg_json_parser.h"

/*
 * Checking the next parameter to know if it is for CLI or Json parser
 * if next parameter is family, the user is giving rule by CLI
 * if next parameter is config_type=json, the user is giving rule by json parser
 */
static struct ppecfg_param cfg_param[] = {
	PPECFG_PARAMLIST_INIT("family=acl", ppecfg_acl_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("family=policer", ppecfg_policer_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("family=qos", ppecfg_qos_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("family=cosmap", ppecfg_cos_map_params, ppecfg_param_iter_tbl),
	PPECFG_PARAMLIST_INIT("family=exception", ppecfg_exception_params, ppecfg_exception_parser),
#ifdef NSS_PPE_TUN_RPS_FEATURE
	PPECFG_PARAMLIST_INIT("family=tun_cfg", ppecfg_tun_rps_params, ppecfg_param_iter_tbl),
#endif
	PPECFG_PARAMLIST_INIT("config_type=json", ppecfg_json_parser_param, ppecfg_json_parser_handler),
#ifdef NSS_PPE_DSCP_FEATURE
	PPECFG_PARAMLIST_INIT("family=dscp", ppecfg_dscp_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_PPE_PM_FEATURE
	PPECFG_PARAMLIST_INIT("family=pm", ppecfg_pm_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE
	PPECFG_PARAMLIST_INIT("family=vlan", ppecfg_vlan_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_PPE_PORT_MGMT_FEATURE
	PPECFG_PARAMLIST_INIT("family=port_mgmt", ppecfg_port_mgmt_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_PPE_DOT1P_FEATURE
	PPECFG_PARAMLIST_INIT("family=dot1p", ppecfg_dot1p_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_PPE_GEMPORT_FEATURE
	PPECFG_PARAMLIST_INIT("family=gem_port", ppecfg_gem_port_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_PPE_PORT_MIRROR_FEATURE
	PPECFG_PARAMLIST_INIT("family=port_mirror", ppecfg_port_mirror_params, ppecfg_param_iter_tbl),
#endif
#ifdef NSS_EDMA_DDRQ_FEATURE
	PPECFG_PARAMLIST_INIT("family=ddrq", ppecfg_edma_ddrq_params, ppecfg_param_iter_tbl),
#endif
};

static struct ppecfg_param root = PPECFG_PARAMLIST_INIT("ppecfg", cfg_param, NULL);

/*
 * main()
 */
int main(int argc, char *argv[])
{
	struct ppecfg_param_in match = {0};
	int error;

	match.total = argc;
	match.args = (char **)argv;

	if (argc < 2) {
		ppecfg_log_arg_error((struct ppecfg_param *)&root);
		match.cur_param = &root;
		error = -EINVAL;
		goto help;
	}

	error = ppecfg_param_iter_tbl(&root, &match);
	if (error < 0) {
		goto help;
	}

	return 0;
help:
	ppecfg_param_help(match.cur_param);

	return error;
}
