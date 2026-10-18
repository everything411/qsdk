/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_GEM_PORT_H
#define __PPECFG_GEM_PORT_H

#define PPECFG_GEM_PORT_HDR_VERSION 4

/*
 * PPECFG GEM_PORT commands
 */
enum ppecfg_gem_port_error {
	PPECFG_GEM_PORT_SUCCESS,			/* flow add or del sucess */
	PPECFG_GEM_PORT_RULE_ID_DEL_FAILED,		/* flow delete failed */
	PPECFG_GEM_PORT_RULE_ID_ADD_FAILED,		/* flow add failed. */
	PPECFG_GEM_PORT_ERROR_MAX
};

/*
 * PPECFG GEM_PORT commands
 */
enum ppecfg_gem_port_cmd {
	PPECFG_GEM_PORT_CMD_RULE_ADD,		/* flow add */
	PPECFG_GEM_PORT_CMD_RULE_DEL,		/* flow delete */
	PPECFG_GEM_PORT_CMD_RULE_FLUSH,		/* flush rules */
	PPECFG_GEM_PORT_CMD_MAX
};

/*
 * PPECFG GEM_PORT Actions
 */
enum ppecfg_gem_port_action {
	PPECFG_GEM_PORT_ACTION_SRC_INFO,        	/* Source port */
	PPECFG_GEM_PORT_ACTION_INT_PRI,			/* int pri (0-15) */
	PPECFG_GEM_PORT_ACTION_INT_DP,            	/* Drop precedence optional */
	PPECFG_GEM_PORT_ACTION_DST_PORT_TYPE,           /* Destination port type */
	PPECFG_GEM_PORT_ACTION_DST_INFO,            	/* Destination info */
	PPECFG_GEM_PORT_ACTION_DST_SELECTION,		/* Destination info input source type is it fdb or gem port mapping table*/
	PPECFG_GEM_PORT_ACTION_MAX
};

/*
 * PPECFG GEM_PORT Rule add parameters
 */
enum ppecfg_gem_port_rule_add {
	PPECFG_GEM_PORT_RULE_ADD_GEM_PORT_ID,         	/* Gem port ID */
	PPECFG_GEM_PORT_RULE_ADD_ACTION,           	/* Action fields */
	PPECFG_GEM_PORT_RULE_ADD_MAX
};

/*
 * PPECFG GEM_PORT rule del
 */
enum ppecfg_gem_port_rule_del {
	PPECFG_GEM_PORT_RULE_DEL_GEM_PORT_ID,		/* GEM_PORT ID */
	PPECFG_GEM_PORT_RULE_DEL_MAX
};

#endif /* __PPECFG_GEM_PORT_H*/
