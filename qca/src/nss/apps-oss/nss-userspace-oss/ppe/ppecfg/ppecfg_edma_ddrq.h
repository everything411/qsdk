/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_EDMA_DDRQ_H
#define __PPECFG_EDMA_DDRQ_H

#define PPECFG_EDMA_DDRQ_PORT_MIN	1
#define PPECFG_EDMA_DDRQ_PORT_MAX	255
#define PPECFG_EDMA_DDRQ_QUEUE_ID_MIN	0
#define PPECFG_EDMA_DDRQ_QUEUE_ID_MAX	159
#define PPECFG_EDMA_DDRQ_GRP_ID_MIN	0
#define PPECFG_EDMA_DDRQ_GRP_ID_MAX	3
#define PPECFG_EDMA_DDRQ_SHRD_WT_MIN	0
#define PPECFG_EDMA_DDRQ_SHRD_WT_MAX	7

#define PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(x)	(((x) < 0) ? 1 : 0)
#define PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(x)	((((x) != 0) && ((x) != 1)) ? 1 : 0)

/*
 * PPECFG EDMA DDRQ commands
 */
enum ppecfg_edma_ddrq_cmd {
	PPECFG_EDMA_DDRQ_CMD_CFG,			/* EDMA per DDRQ configuration command */
	PPECFG_EDMA_DDRQ_CMD_GRP_CFG,			/* EDMA DDRQ group configuration command */
	PPECFG_EDMA_DDRQ_CMD_MAX			/* EDMA DDRQ Maximum command */
};

/*
 * PPECFG EDMA DDRQ rule parameters
 */
enum ppecfg_edma_ddrq_rule_ddrq_cfg {
	PPECFG_EDMA_DDRQ_CFG_RULE_PORT_ID,		/* Port ID */
	PPECFG_EDMA_DDRQ_CFG_RULE_QUEUE_ID,		/* Queue id */
	PPECFG_EDMA_DDRQ_CFG_RULE_GET_CMD,		/* Get command knob (for getting the current configuration) */
	PPECFG_EDMA_DDRQ_CFG_RULE_STATE,		/* EDMA DDRQ state knob (enable/disable) */
	PPECFG_EDMA_DDRQ_CFG_RULE_AC_EN,		/* Admission control knob */
	PPECFG_EDMA_DDRQ_CFG_RULE_BP_EN,		/* Backpressure knob */
	PPECFG_EDMA_DDRQ_CFG_RULE_COLOR_AWARE,		/* Color aware knob */
	PPECFG_EDMA_DDRQ_CFG_RULE_ECN_MARK_EN,		/* ECN mark knob */
	PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_GRN_MIN,	/* Gap between green max and green min */
	PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_RED_MIN,	/* Gap between green max and red min */
	PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_RED_MAX,	/* Gap between green max and red max */
	PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_YEL_MIN,	/* Gap between green max and yellow min */
	PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_YEL_MAX,	/* Gap between green max and yellow max */
	PPECFG_EDMA_DDRQ_CFG_RULE_GRN_RESUME_OFFSET,	/* Green resume offset */
	PPECFG_EDMA_DDRQ_CFG_RULE_GRP_ID,		/* EDMA DDRQ group id */
	PPECFG_EDMA_DDRQ_CFG_RULE_PRE_ALLOC_LIMIT,	/* Prealloc limit */
	PPECFG_EDMA_DDRQ_CFG_RULE_RED_RESUME_OFFSET,	/* Red resume offset */
	PPECFG_EDMA_DDRQ_CFG_RULE_YEL_RESUME_OFFSET,	/* Yellow resume offset */
	PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_CEILING,	/* Shared ceiling */
	PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_DYNAMIC,	/* Shared dynamic */
	PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_WEIGHT,	/* Shared weight */
	PPECFG_EDMA_DDRQ_CFG_RULE_WRED_EN,		/* WRED enable knob */
	PPECFG_EDMA_DDRQ_CFG_RULE_MAX,			/* Maximum configuration */
};

/*
 * PPECFG EDMA DDRQ group rule parameters
 */
enum ppecfg_edma_ddrq_rule_ddrq_grp_cfg {
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRP_ID,			/* EDMA DDRQ group id */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GET_CMD,			/* Get command knob (for getting the current configuration) */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_AC_EN,			/* Admission control knob */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_COLOR_AWARE,		/* Color aware knob */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_DP_THRD,			/* Drop threshold */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_GRN_RED,		/* Gap between green and red */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRN_YEL,			/* Gap between green and yellow */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_GRN_RESUME_OFFSET,	/* Green resume offset */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_RED_RESUME_OFFSET,	/* Red resume offset */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_YEL_RESUME_OFFSET,	/* Yellow resume offset */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_SHRD_LIMIT,		/* Shared limit */
	PPECFG_EDMA_DDRQ_GRP_CFG_RULE_MAX,			/* Maximum configuration */
};

#endif /* __PPECFG_EDMA_DDRQ_H */
