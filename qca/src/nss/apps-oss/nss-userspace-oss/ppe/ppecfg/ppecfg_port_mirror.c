/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG port-mirror handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>
#include <nss_ppenl_port_mirror_if.h>
#include <nss_ppenl_port_mirror_api.h>
#include "ppecfg_param.h"
#include "ppecfg_port_mirror.h"

/*
 * cfg sub-parameters: port_name= and dir=
 */
static struct ppecfg_param port_mirror_cfg_params[PPECFG_PORT_MIRROR_CFG_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_PORT_MIRROR_CFG_PORT_NAME,    "port_name="),
	PPECFG_PARAM_INIT(PPECFG_PORT_MIRROR_CFG_DIR,          "dir="),
};

static int ppecfg_port_mirror_cfg_enable(struct ppecfg_param *param,
					 struct ppecfg_param_in *match);
static int ppecfg_port_mirror_cfg_disable(struct ppecfg_param *param,
					  struct ppecfg_param_in *match);

/*
 * Top-level command table.
 */
struct ppecfg_param ppecfg_port_mirror_params[PPECFG_PORT_MIRROR_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=enable", port_mirror_cfg_params,
			      ppecfg_port_mirror_cfg_enable),
	PPECFG_PARAMLIST_INIT("cmd=disable", port_mirror_cfg_params,
			      ppecfg_port_mirror_cfg_disable),
};

static int ppecfg_port_mirror_cfg_common(struct ppecfg_param *param,
                                         struct ppecfg_param_in *match,
                                         bool mirror_en)
{
        struct nss_ppenl_port_mirror_rule nl_msg = {{0}};
        struct ppecfg_param *sub_params;
        char dir_str[16];
        int error;

        if (!param || !match) {
                ppecfg_log_warn("Param or match table is NULL\n");
                return -EINVAL;
        }

        error = ppecfg_param_iter_tbl(param, match);
        if (error) {
                ppecfg_log_arg_error(param);
                return error;
        }

        nss_ppenl_port_mirror_rule_init(&nl_msg,
                                        NSS_PPE_PORT_MIRROR_CFG_MSG);

        /*
         * Extract port_name.
         */
        sub_params = &param->sub_params[PPECFG_PORT_MIRROR_CFG_PORT_NAME];
        error = ppecfg_param_get_str(sub_params->data,
                                     IFNAMSIZ,
                                     nl_msg.cfg.port_name);
        if (error) {
                ppecfg_log_arg_error(sub_params);
                return error;
        }

        /*
         * Extract dir.
         */
        sub_params = &param->sub_params[PPECFG_PORT_MIRROR_CFG_DIR];
        error = ppecfg_param_get_str(sub_params->data,
                                     sizeof(dir_str),
                                     dir_str);
        if (error) {
                ppecfg_log_arg_error(sub_params);
                return error;
        }

        if (!strcmp(dir_str, "ingress")) {
                nl_msg.cfg.dir = NSS_PPE_PORT_MIRROR_DIR_INGRESS;
        } else if (!strcmp(dir_str, "egress")) {
                nl_msg.cfg.dir = NSS_PPE_PORT_MIRROR_DIR_EGRESS;
        } else {
                ppecfg_log_warn("Invalid dir '%s': use ingress|egress\n",
                                dir_str);
                return -EINVAL;
        }

        nl_msg.cfg.mirror_en = mirror_en;

        error = nss_ppenl_port_mirror_cfg(&nl_msg);
        if (error < 0) {
                ppecfg_log_warn("Unable to send port-mirror message\n");
                return error;
        }

        ppecfg_log_info("port-mirror %s: port=%s dir=%s\n",
                        mirror_en ? "enabled" : "disabled",
                        nl_msg.cfg.port_name,
                        dir_str);

        return 0;
}

static int ppecfg_port_mirror_cfg_enable(struct ppecfg_param *param,
                                         struct ppecfg_param_in *match)
{
        return ppecfg_port_mirror_cfg_common(param, match, true);
}

static int ppecfg_port_mirror_cfg_disable(struct ppecfg_param *param,
                                          struct ppecfg_param_in *match)
{
        return ppecfg_port_mirror_cfg_common(param, match, false);
}
