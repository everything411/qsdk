/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_DSCP_H
#define __PPECFG_DSCP_H

/*
 * PPECFG DSCP commands
 */
enum ppecfg_dscp_cmd {
	PPECFG_CMD_DSCP_CONFIG,			/* DSCP to P bit table add */
	PPECFG_DSCP_CMD_MAX			/* Maximum DSCP Command */
};

/*
 * PPECFG DSCP PCP fields
 */
enum ppecfg_dscp_pcp {
	PPECFG_DSCP_PCP0,			/* PCP0 Value */
	PPECFG_DSCP_PCP1,			/* PCP1 Value */
	PPECFG_DSCP_PCP_MAX			/* Maximum PCP Parameter */
};

/*
 * PPECFG DSCP to P bit rule add
 */
enum ppecfg_dscp_rule_add {
	PPECFG_DSCP_RULE_ADD_DIR,		/* Direction */
	PPECFG_DSCP_RULE_ADD_ECN,		/* ECN Value */
	PPECFG_DSCP_RULE_ADD_DSCP,		/* DSCP Value */
	PPECFG_DSCP_RULE_ADD_PCP,		/* PCP Value */
	PPECFG_DSCP_RULE_ADD_MAX		/* Maximum Add Parameter */
};

#endif /* __PPECFG_DSCP_H*/
