/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_DOT1P_H
#define __PPECFG_DOT1P_H

#define PPECFG_DOT1P_HDR_VERSION 4

/*
 * PPECFG DOT1P commands
 */
enum ppecfg_dot1p_cmd {
	PPECFG_DOT1P_CMD_RULE_ADD,		/* dot1p rule add */
	PPECFG_DOT1P_CMD_RULE_DEL,		/* dot1p rule delete */
	PPECFG_DOT1P_CMD_RULE_FLUSH,		/* dot1p rules flush */
	PPECFG_DOT1P_CMD_RULE_ADD_DEFAULT,	/* dot1p rule add default */
	PPECFG_DOT1P_CMD_RULE_PAUSE,		/* dot1p rule pause */
	PPECFG_DOT1P_CMD_RULE_RESUME,		/* dot1p rule resume */
	PPECFG_DOT1P_CMD_RULE_GET_PAUSE_STATE,	/* dot1p rule get pause state */
	PPECFG_DOT1P_CMD_POLICER_RULE_ADD,	/* dot1p add policer id */
	PPECFG_DOT1P_CMD_POLICER_RULE_DEL,	/* dot1p delete policer rule */
	PPECFG_DOT1P_CMD_POLICER_RULE_FLUSH,	/* dot1p flush policer rules */
	PPECFG_DOT1P_CMD_MAX
};

/*
 * PPECFG DOT1P Actions
 */
enum ppecfg_dot1p_action {
	PPECFG_DOT1P_ACTION_GEM_PORT_ID,        /* GEM port ID */
	PPECFG_DOT1P_ACTION_PQ,			/* Priority Queue (0-127) */
	PPECFG_DOT1P_ACTION_BASE_PQ,		/* Base pq selection option */
	PPECFG_DOT1P_ACTION_SC,			/* service code optional */
	PPECFG_DOT1P_ACTION_INT_DP,            	/* Drop precedence optional */
	PPECFG_DOT1P_ACTION_DST_INFO,           /* Destination info optional */
	PPECFG_DOT1P_ACTION_FWD_CMD,        	/* Forward command */
	PPECFG_DOT1P_ACTION_MAX
};

/*
 * PPECFG DOT1P Rule add parameters
 */
enum ppecfg_dot1p_rule_add {
	PPECFG_DOT1P_RULE_ADD_RULE_ID,		/* Rule ID */
	PPECFG_DOT1P_RULE_ADD_SRC_PORT_TYPE,	/* Type of src info i.e. BITMAP or PORT */
	PPECFG_DOT1P_RULE_ADD_SRC_INFO,         /* Source Dev info */
	PPECFG_DOT1P_RULE_ADD_DST_PORT_TYPE,	/* Type of dst info i.e. BITMAP or PORT */
	PPECFG_DOT1P_RULE_ADD_DST_INFO,         /* Destination  Dev info*/
	PPECFG_DOT1P_RULE_ADD_VID,      	/* VID value */
	PPECFG_DOT1P_RULE_ADD_PCP,        	/* PCP value */
	PPECFG_DOT1P_RULE_ADD_DEI,        	/* DEI value */
	PPECFG_DOT1P_RULE_ADD_DSCP,        	/* DSCP value */
	PPECFG_DOT1P_RULE_ADD_ACTION,           /* Action fields */
	PPECFG_DOT1P_RULE_ADD_MAX
};

/*
 * PPECFG Policer DOT1P Actions
 */
enum ppecfg_dot1p_policer_action {
	PPECFG_DOT1P_POLICER_ACTION_US_POLICER_ID,      /* Upstream Policer id value */
	PPECFG_DOT1P_POLICER_ACTION_US_POLICER_EN,      /* Enable upstream policer id optional */
	PPECFG_DOT1P_POLICER_ACTION_DS_POLICER_EN,      /* Enable downstream Policer id optional */
	PPECFG_DOT1P_POLICER_ACTION_MAX
};

/*
 * PPECFG DOT1P Rule add policer id parameters
 */
enum ppecfg_dot1p_rule_add_policer_id {
	PPECFG_DOT1P_RULE_ADD_GEMPORT_ID,        /* GEM port ID */
	PPECFG_DOT1P_RULE_ADD_POLICER_ACTION,     /* Action fields */
	PPECFG_DOT1P_RULE_ADD_POLICER_ID_MAX
};

/*
 * PPECFG DOT1P flow del
 */
enum ppecfg_dot1p_rule_del {
	PPECFG_DOT1P_RULE_DEL_RULE_ID,		/* Rule ID */
	PPECFG_DOT1P_RULE_DEL_MAX
};

/*
 * PPECFG DOT1P flow flush
 */
enum ppecfg_dot1p_rule_flush {
	PPECFG_DOT1P_RULE_FLUSH_EX_FIRST,	/* flush all rules except the first */
	PPECFG_DOT1P_RULE_FLUSH_MAX
};

/*
 * PPECFG DOT1P flow pause
 */
enum ppecfg_dot1p_rule_pause {
	PPECFG_DOT1P_RULE_PAUSE_EX_GEM0,	/* Pause all rules except gemport0 */
	PPECFG_DOT1P_RULE_PAUSE_MAX
};

/*
 * PPECFG DOT1P flow add default
 */
enum ppecfg_dot1p_add_default {
	PPECFG_DOT1P_ADD_DEFAULT_VID,		/* Add default rule vlan info. */
	PPECFG_DOT1P_ADD_DEFAULT_PCP,		/* Add default rule PCP info. */
	PPECFG_DOT1P_ADD_DEFAULT_DEI,		/* Add default rule DEI info. */
	PPECFG_DOT1P_ADD_DEFAULT_DSCP,		/* Add default rule DSCP info. */
	PPECFG_DOT1P_ADD_DEFAULT_DSCP_MASK,	/* Add default rule DSCP mask info. */
	PPECFG_DOT1P_ADD_DEFAULT_GEN_MISS_CMD,	/* Add default rule gen miss cmd. */
	PPECFG_DOT1P_ADD_DEFAULT_MAX
};

/*
 * PPECFG DOT1P policer flow del
 */
enum ppecfg_dot1p_policer_rule_del {
	PPECFG_DOT1P_POLICER_RULE_DEL_GEMPORT_ID,		/* GEMPORT ID */
	PPECFG_DOT1P_POLICER_RULE_DEL_MAX
};

#endif /* __PPECFG_DOT1P_H*/
