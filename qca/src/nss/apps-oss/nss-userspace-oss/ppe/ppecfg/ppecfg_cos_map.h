/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_COS_MAP_H
#define __PPECFG_COS_MAP_H

#define PPECFG_COS_MAP_HDR_VERSION 1

/*
 * PPECFG COS MAP commands
 */
enum ppecfg_cos_map_error {
	PPECFG_COS_MAP_SUCCESS,			/* Command success */
	PPECFG_COS_MAP_RULE_ID_DEL_FAILED,	/* Rule delete failed */
	PPECFG_COS_MAP_RULE_ID_ADD_FAILED,	/* Rule add failed */
	PPECFG_COS_MAP_RULE_FLUSH_FAILED,	/* Rule flush failed */
	PPECFG_COS_MAP_PORT_GROUP_SET_FAILED,	/* Port group set failed */
	PPECFG_COS_MAP_ERROR_MAX		/* Maximum error code */
};

/*
 * PPECFG COS MAP commands
 */
enum ppecfg_cos_map_cmd {
	PPECFG_COS_MAP_CMD_RULE_ADD,		/* rule add */
	PPECFG_COS_MAP_CMD_RULE_DEL,		/* rule delete */
	PPECFG_COS_MAP_CMD_RULE_FLUSH,		/* rules flush */
	PPECFG_COS_MAP_CMD_PORT_GROUP_SET,	/* port group set*/
	PPECFG_COS_MAP_CMD_MAX			/* Maximum command */
};

/*
 * PPECFG COS MAP Actions
 */
enum ppecfg_cos_map_action {
	PPECFG_COS_MAP_ACTION_DSCP, 		/* DSCP value */
	PPECFG_COS_MAP_ACTION_PCP,		/* PCP Value */
	PPECFG_COS_MAP_ACTION_DEI,		/* DEI Value */
	PPECFG_COS_MAP_ACTION_INT_PRI,		/* Int pri Value */
	PPECFG_COS_MAP_ACTION_DP,		/* Drop precedence value */
	PPECFG_COS_MAP_ACTION_MAX		/* Maximum action */
};

/*
 * PPECFG COS MAP TOS based parameters
 */
enum ppecfg_cos_map_tos {
	PPECFG_COS_MAP_TOS_DSCP_VAL,	/* DSCP value */
	PPECFG_COS_MAP_TOS_MAX		/* Maximum TOS value */
};

/*
 * PPECFG COS MAP PCP and DEI based parameters
 */
enum ppecfg_cos_map_tci {
	PPECFG_COS_MAP_TCI_PCP_VAL,	/* PCP value */
	PPECFG_COS_MAP_TCI_DEI_VAL,	/* DEI value */
	PPECFG_COS_MAP_TCI_MAX		/* Maximum TCI parameters */
};

/*
 * PPECFG COS MAP Rule add parameters
 */
enum ppecfg_cos_map_rule_add {
	PPECFG_COS_MAP_RULE_ADD_RULE_ID,	/* Rule ID */
	PPECFG_COS_MAP_RULE_ADD_GROUP_ID,	/* Group ID */
	PPECFG_COS_MAP_RULE_ADD_TOS,		/* TOS based QoS rule type */
	PPECFG_COS_MAP_RULE_ADD_TCI,      	/* TCI based QoS rule type */
	PPECFG_COS_MAP_RULE_ADD_ACTION,		/* Action fields */
	PPECFG_COS_MAP_RULE_ADD_MAX		/* Maximum Action filed */
};

/*
 * PPECFG COS MAP rule del
 */
enum ppecfg_cos_map_rule_del {
	PPECFG_COS_MAP_RULE_DEL_RULE_ID,	/* Rule ID */
	PPECFG_COS_MAP_RULE_DEL_MAX	/* Maximum delete parameter */
};

/*
 * PPECFG COS MAP group set parameters
 */
enum ppecfg_cos_map_group_set {
	PPECFG_COS_MAP_GROUP_ADD_DEV,			/* Device name */
	PPECFG_COS_MAP_GROUP_ADD_TCI_GROUP_ID,		/* PCP Group ID */
	PPECFG_COS_MAP_GROUP_ADD_TOS_GROUP_ID,		/* DSCP Group ID */
	PPECFG_COS_MAP_GROUP_ADD_MAX			/* Maximum group set parameter */
};

#endif /* __PPECFG_COS_MAP_H*/
