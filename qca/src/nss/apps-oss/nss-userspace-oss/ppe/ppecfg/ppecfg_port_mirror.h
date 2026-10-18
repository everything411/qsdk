/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_PORT_MIRROR_H
#define __PPECFG_PORT_MIRROR_H

/*
 * ppecfg_port_mirror_cmd
 *	Top-level command identifiers for port-mirror operations.
 */
enum ppecfg_port_mirror_cmd {
	PPECFG_PORT_MIRROR_CMD_EN,	/**< Enable mirror on a port */
	PPECFG_PORT_MIRROR_CMD_DIS,	/**< Disable mirror on a port */
	PPECFG_PORT_MIRROR_CMD_MAX
};

/*
 * ppecfg_port_mirror_cfg_params
 *	Sub-parameters for the "cmd=cfg" command.
 */
enum ppecfg_port_mirror_cfg_params {
	PPECFG_PORT_MIRROR_CFG_PORT_NAME,	/**< port_name */
	PPECFG_PORT_MIRROR_CFG_DIR,		/**< ingress/egress mirroring. */
	PPECFG_PORT_MIRROR_CFG_MAX
};

#endif /* __PPECFG_PORT_MIRROR_H */
