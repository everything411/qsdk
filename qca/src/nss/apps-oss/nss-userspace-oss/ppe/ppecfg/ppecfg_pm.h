/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_PM_H
#define __PPECFG_PM_H

/*
 * PPECFG PM commands
 */
enum ppecfg_pm_cmd {
	PPECFG_PM_CMD_COUNTER_GET,			/* Get the PM Counter stats */
	PPECFG_PM_CMD_COUNTER_GEN_RULE_ADD,		/* Counter generation rule add */
	PPECFG_PM_CMD_COUNTER_GEN_RULE_DEL,		/* Counter generation rule del */
	PPECFG_PM_CMD_MAX				/* Maximum PM command */
};

/*
 * PPECFG PM counter stats get
 */
enum ppecfg_pm_counter_get {
	PPECFG_PM_COUNTER_GET_COUNTER_ID,		/* Counter ID for fetching PM counter */
	PPECFG_PM_COUNTER_GET_MAX			/* Maximum PM counter get command */
};

/*
 * PPECFG PM counter generation table rule add
 */
enum ppecfg_pm_counter_gen_rule_add {
	PPECFG_PM_COUNTER_GEN_RULE_ADD_RULE_DIR,	/* Rule direction */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_PM_DIR,		/* PM counter direction */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_COUNTER_ID,	/* Counter ID for counter generation rule */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_PORT_TYPE,	/* Port type: PORT/BITMAP/GEM_PORT */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_PORT_INFO,	/* Port information based on port type field */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_TAG_FORMAT,	/* tag format: Tagged/Untagged/All */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_VID,		/* VLAN ID value */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_PCP,		/* PCP value */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_IPMC,		/* IPMC value */
	PPECFG_PM_COUNTER_GEN_RULE_ADD_MAX
};

/*
 * PPECFG PM counter generation table rule del
 */
enum ppecfg_pm_counter_gen_rule_del {
	PPECFG_PM_COUNTER_GEN_RULE_DEL_COUNTER_ID,      /* Counter ID for counter generation rule */
	PPECFG_PM_COUNTER_GEN_RULE_DEL_MAX		/* Maximum PM gen rule del command */
};

#endif /* __PPECFG_PM_H*/

