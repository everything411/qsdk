/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_PORT_MGMT_H
#define __PPECFG_PORT_MGMT_H

#define PPECFG_PORT_MGMT_HDR_VERSION 4

/*
 * PPECFG PORT_MGMT commands
 *	Top‑level command identifiers for Port Management operations.
 */
enum ppecfg_port_mgmt_cmd {
	PPECFG_PORT_MGMT_CMD_PORT_ISOL_SET,			/* Port isolation set */
	PPECFG_PORT_MGMT_CMD_PORT_ISOL_GET,			/* Port isolation get */
	PPECFG_PORT_MGMT_CMD_DEF_ISOL_SET,			/* Default Isol set */
	PPECFG_PORT_MGMT_CMD_MAC_LEARN_LIMIT_SET,		/* Mac learn limit */
	PPECFG_PORT_MGMT_CMD_MAC_FILTER_SET,			/* Mac Filter set */
	PPECFG_PORT_MGMT_CMD_MAC_FILTER_CLR,			/* MAc Filter clear */
	PPECFG_PORT_MGMT_CMD_OMCI_PORT_ADD,			/* OMCI port add */
	PPECFG_PORT_MGMT_CMD_OMCI_PORT_DEL,			/* OMCI port delete */
	PPECFG_PORT_MGMT_CMD_OMCI_PORT_FLUSH,			/* OMCI port flush */
	PPECFG_PORT_MGMT_CMD_MAX
};

/*
 * PPECFG PORT_MGMT port isolation set
 *	Attributes used when setting port isolation via Netlink.
 */
enum ppecfg_port_mgmt_port_isol_set {
	PPECFG_PORT_MGMT_PORT_ISOL_SET_PORT_NAME,   /**< Port name associated with the isolation rule */
	PPECFG_PORT_MGMT_PORT_ISOL_SET_OMCI_ID,     /**< OMCI ID for which isolation is configured */
	PPECFG_PORT_MGMT_PORT_ISOL_SET_MAX	    /**< Max attribute indicator */
};

/*
 * PPECFG PORT_MGMT action control set
 *	Attributes for enabling isolation actions during configuration.
 */
enum ppecfg_port_mgmt_act_ctrl_set {
	PPECFG_PORT_MGMT_ACT_CTRL_SET_MC_ISOL_EN,   /**< Enable/disable multicast isolation */
	PPECFG_PORT_MGMT_ACT_CTRL_SET_BC_ISOL_EN,   /**< Enable/disable broadcast isolation */
	PPECFG_PORT_MGMT_ACT_CTRL_SET_MAX	    /**< Max attribute indicator */
};

/*
 * PORT_MGMT mac learn limit set parameters
 */
enum ppecfg_port_mgmt_port_lrn_limit_set {
	PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_ENABLE,		/* enable mac learn limit */
	PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_PORT_NAME,		/* Port Name */
	PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_PORT_LEARN_LIMIT,	/* Mac learn limit */
	PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_LRN_EXCEED_ACTION,	/* learn limit exceed action */
	PPECFG_PORT_MGMT_PORT_LRN_LIMIT_SET_MAX
};

/*
 * PORT_MGMT mac filter set parameters
 */
enum ppecfg_port_mgmt_mac_filter_set {
        PPECFG_PORT_MGMT_MAC_FILTER_SET_BLOCK,			/* Block mac address */
	PPECFG_PORT_MGMT_MAC_FILTER_SET_FID_NAME,		/* Specify filter ID */
        PPECFG_PORT_MGMT_MAC_FILTER_SET_MAX
};

/*
 * PORT_MGMT mac filter clear parameters
 */
enum ppecfg_port_mgmt_mac_filter_clr {
	PPECFG_PORT_MGMT_MAC_FILTER_CLR_BLOCK,			/* Allow the blocked mac addr */
	PPECFG_PORT_MGMT_MAC_FILTER_CLR_FID_NAME,		/* Specify filter ID */
        PPECFG_PORT_MGMT_MAC_FILTER_CLR_MAX
};

/**
 * ppecfg_port_mgmt_omci_port_set
 *	Parameters for OMCI port add/del commands.
 */
enum ppecfg_port_mgmt_omci_port_set {
	PPECFG_PORT_MGMT_OMCI_PORT_SET_DEV,			/* Device name (dev=<ifname>) */
	PPECFG_PORT_MGMT_OMCI_PORT_SET_MAX
};

#endif /* __PPECFG_PORT_MGMT_H*/
