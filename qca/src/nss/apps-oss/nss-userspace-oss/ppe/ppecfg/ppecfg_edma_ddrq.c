/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file PPECFG EDMA DDRQ handler
 */

#include "ppecfg_hlos.h"
#include <nss_ppenl_base.h>

#include "ppecfg_param.h"
#include "ppecfg_edma_ddrq.h"

static int ppecfg_edma_ddrq_config(struct ppecfg_param *param, struct ppecfg_param_in *match);
static int ppecfg_edma_ddrq_grp_config(struct ppecfg_param *param, struct ppecfg_param_in *match);

/*
 * EDMA DDRQ configuration parameters
 */
static struct ppecfg_param ppecfg_edma_ddrq_cfg_params[PPECFG_EDMA_DDRQ_CFG_RULE_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_PORT_ID, "port_id="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_QUEUE_ID, "queue_id="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GET_CMD, "get_cmd="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_STATE, "state="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_AC_EN, "ac_en="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_BP_EN, "bp_en="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_COLOR_AWARE, "color_aware="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_ECN_MARK_EN, "ecn_mark_en="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_GRN_MIN, "gap_grn_grn_min="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_RED_MIN, "gap_grn_red_min="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_RED_MAX, "gap_grn_red_max="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_YEL_MIN, "gap_grn_yel_min="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_YEL_MAX, "gap_grn_yel_max="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GRN_RESUME_OFFSET, "grn_resume_offset="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_GRP_ID, "group_id="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_PRE_ALLOC_LIMIT, "pre_alloc_limit="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_RED_RESUME_OFFSET, "red_resume_offset="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_YEL_RESUME_OFFSET, "yel_resume_offset="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_CEILING, "shared_ceiling="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_DYNAMIC, "shared_dynamic="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_WEIGHT, "shared_weight="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_CFG_RULE_WRED_EN, "wred_en="),
};

/*
 * EDMA DDRQ group parameters
 */
static struct ppecfg_param ppecfg_edma_ddrq_grp_cfg_params[PPECFG_EDMA_DDRQ_GRP_CFG_RULE_MAX] = {
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRP_ID, "group_id="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GET_CMD, "get_cmd="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_AC_EN, "ac_en="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_DP_THRD, "dp_thrd="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_COLOR_AWARE, "color_aware="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_GRN_RED, "gap_grn_red="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRN_YEL, "gap_grn_yel="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_GRN_RESUME_OFFSET, "grn_resume_offset="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_RED_RESUME_OFFSET, "red_resume_offset="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_YEL_RESUME_OFFSET, "yel_resume_offset="),
	PPECFG_PARAM_INIT(PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_SHRD_LIMIT, "shared_limit="),
};

/*
 * EDMA parameters
 */
struct ppecfg_param ppecfg_edma_ddrq_params[PPECFG_EDMA_DDRQ_CMD_MAX] = {
	PPECFG_PARAMLIST_INIT("cmd=ddrq_config", ppecfg_edma_ddrq_cfg_params, ppecfg_edma_ddrq_config),
	PPECFG_PARAMLIST_INIT("cmd=ddrq_grp_config", ppecfg_edma_ddrq_grp_cfg_params, ppecfg_edma_ddrq_grp_config),
};

/*
 * ppecfg_edma_ddrq_config_validate()
 *	API to validate DDRQ configuration parameters
 */
static int ppecfg_edma_ddrq_config_validate(struct nss_ppenl_edma_ddrq_config *ddrq_cfg)
{
	/*
	 * The below validation logic are meant to only checks some of the
	 * general ranges/values for the input configuration parameters
	 * before proceeding to invoke the base module's configuration API.
	 */
	if (ddrq_cfg->ddrq_obj.cfg_type == NSS_DP_DDRQ_CFG_TYPE_PORT) {
		if ((ddrq_cfg->ddrq_obj.cfg_id < PPECFG_EDMA_DDRQ_PORT_MIN) ||
				 (ddrq_cfg->ddrq_obj.cfg_id > PPECFG_EDMA_DDRQ_PORT_MAX)) {
			ppecfg_log_error("Invalid DDRQ port number. Allowed range is %d to %d\n",
					PPECFG_EDMA_DDRQ_PORT_MIN, PPECFG_EDMA_DDRQ_PORT_MAX);
			return -EINVAL;
		}
	} else if (ddrq_cfg->ddrq_obj.cfg_type== NSS_DP_DDRQ_CFG_TYPE_QUEUE) {
		if ((ddrq_cfg->ddrq_obj.cfg_id < PPECFG_EDMA_DDRQ_QUEUE_ID_MIN) ||
				 (ddrq_cfg->ddrq_obj.cfg_id > PPECFG_EDMA_DDRQ_QUEUE_ID_MAX)) {
			ppecfg_log_error("Invalid DDRQ queue number. Allowed range is %d to %d\n",
					PPECFG_EDMA_DDRQ_QUEUE_ID_MIN, PPECFG_EDMA_DDRQ_QUEUE_ID_MAX);
			return -EINVAL;
		}
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_grn_min != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_grn_min)) {
		ppecfg_log_error("DDRQ gap_grn_grn_min parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_min != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_min)) {
		ppecfg_log_error("DDRQ gap_grn_red_min parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_max != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_max)) {
		ppecfg_log_error("DDRQ gap_grn_red_max parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_min != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_min)) {
		ppecfg_log_error("DDRQ gap_grn_yel_min parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_max != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_max)) {
		ppecfg_log_error("DDRQ gap_grn_yel_max parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_grn_resume_offset != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_grn_resume_offset)) {
		ppecfg_log_error("DDRQ grn_resume_offset parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_red_resume_offset != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_red_resume_offset)) {
		ppecfg_log_error("DDRQ red_resume_offset parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_yel_resume_offset != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_yel_resume_offset)) {
		ppecfg_log_error("DDRQ yel_resume_offset parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_grp_id != NSS_DP_DDRQ_INV_VAL) &&
			((ddrq_cfg->ddrq_ac_cfg.ac_cfg_grp_id < PPECFG_EDMA_DDRQ_GRP_ID_MIN) ||
			 (ddrq_cfg->ddrq_ac_cfg.ac_cfg_grp_id > PPECFG_EDMA_DDRQ_GRP_ID_MAX))) {
		ppecfg_log_error("Invalid DDRQ group number. Allowed range is %d to %d\n",
				PPECFG_EDMA_DDRQ_GRP_ID_MIN, PPECFG_EDMA_DDRQ_GRP_ID_MAX);
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_pre_alloc_limit != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_pre_alloc_limit)) {
		ppecfg_log_error("DDRQ pre_alloc_limit parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_ceiling != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_ceiling)) {
		ppecfg_log_error("DDRQ shared_ceiling parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_dynamic != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_dynamic)) {
		ppecfg_log_error("Invalid DDRQ shared dynamic value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_weight != NSS_DP_DDRQ_INV_VAL) &&
			((ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_weight < PPECFG_EDMA_DDRQ_SHRD_WT_MIN) ||
			 (ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_weight > PPECFG_EDMA_DDRQ_SHRD_WT_MAX))) {
		ppecfg_log_error("Invalid DDRQ shared weight value. Allowed range is %d to %d\n",
				PPECFG_EDMA_DDRQ_SHRD_WT_MIN, PPECFG_EDMA_DDRQ_SHRD_WT_MAX);
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ddrq_state != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ddrq_state)) {
		ppecfg_log_error("Invalid DDRQ state value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_ac_en != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ac_cfg_ac_en)) {
		ppecfg_log_error("Invalid DDRQ ac_en value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_bp_en != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ac_cfg_bp_en)) {
		ppecfg_log_error("Invalid DDRQ bp_en value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_color_aware != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ac_cfg_color_aware)) {
		ppecfg_log_error("Invalid DDRQ color_aware value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_ecn_mark_en != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ac_cfg_ecn_mark_en)) {
		ppecfg_log_error("Invalid DDRQ ecn_mark_en value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_cfg->ddrq_ac_cfg.ac_cfg_wred_en != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_cfg->ddrq_ac_cfg.ac_cfg_wred_en)) {
		ppecfg_log_error("Invalid DDRQ wred_en value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * ppecfg_edma_ddrq_grp_config_validate()
 *	API to validate DDRQ group configuration parameters
 */
static int ppecfg_edma_ddrq_grp_config_validate(struct nss_ppenl_edma_ddrq_grp_config *ddrq_grp_cfg)
{
	/*
	 * The below validation logic are meant to only checks some of the
	 * general ranges/values for the input configuration parameters
	 * before proceeding to invoke the base module's configuration API.
	 */
	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_dp_thrd != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_dp_thrd)) {
		ppecfg_log_error("DDRQ group dp_thrd parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_red != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_red)) {
		ppecfg_log_error("DDRQ group gap_grn_red parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_yel != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_yel)) {
		ppecfg_log_error("DDRQ group gap_grn_yel parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_grn_resume_offset != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_grn_resume_offset)) {
		ppecfg_log_error("DDRQ group grn_resume_offset parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_red_resume_offset != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_red_resume_offset)) {
		ppecfg_log_error("DDRQ group red_resume_offset parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_yel_resume_offset != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_yel_resume_offset)) {
		ppecfg_log_error("DDRQ group yel_resume_offset parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_shrd_limit != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NEGATIVE(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_shrd_limit)) {
		ppecfg_log_error("DDRQ group gap_shrd_limit parameter value can't be negative\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->grp_id != NSS_DP_DDRQ_INV_VAL) &&
			((ddrq_grp_cfg->grp_id < PPECFG_EDMA_DDRQ_GRP_ID_MIN) ||
			 (ddrq_grp_cfg->grp_id > PPECFG_EDMA_DDRQ_GRP_ID_MAX))) {
		ppecfg_log_error("Invalid DDRQ group id. Allowed range is %d to %d\n",
				PPECFG_EDMA_DDRQ_GRP_ID_MIN, PPECFG_EDMA_DDRQ_GRP_ID_MAX);
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_ac_en != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_ac_en)) {
		ppecfg_log_error("Invalid DDRQ group ac_en value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	if ((ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_color_aware != NSS_DP_DDRQ_INV_VAL) &&
			PPECFG_EDMA_DDRQ_IS_PARAM_NOT_BOOL(ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_color_aware)) {
		ppecfg_log_error("Invalid DDRQ group color_aware value. Allowed range is 0 & 1\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * ppecfg_edma_ddrq_config()
 *	API to handle EDMA DDRQ rule
 */
static int ppecfg_edma_ddrq_config(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_edma_ddrq_rule nl_msg = {0};
	int error, count = 0;
	struct ppecfg_param *sub_params;
	struct nss_ppenl_edma_ddrq_config *ddrq_cfg;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL");
		return -EINVAL;
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error < 0) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_edma_ddrq_init_rule(&nl_msg, NSS_PPE_EDMA_DDRQ_CFG_RULE_MSG);
	ddrq_cfg = &nl_msg.msg.ddrq_cfg;

	/*
	 * Set the initial invalid value to all the available parameters
	 * before commencing the parsing of the user provided inputs.
	 */
	ddrq_cfg->ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_NONE;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_grn_min = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_min= NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_max = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_min = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_max = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_grn_resume_offset = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_red_resume_offset = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_yel_resume_offset = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_pre_alloc_limit = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_ceiling = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_dynamic = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_weight = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_ac_en = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_bp_en = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_color_aware = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_ecn_mark_en = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_grp_id = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ac_cfg_wred_en = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->ddrq_ac_cfg.ddrq_state = NSS_DP_DDRQ_INV_VAL;
	ddrq_cfg->get_cmd = false;

	for (int index = PPECFG_EDMA_DDRQ_CFG_RULE_PORT_ID; index <= PPECFG_EDMA_DDRQ_CFG_RULE_WRED_EN; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
			case PPECFG_EDMA_DDRQ_CFG_RULE_PORT_ID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_obj.cfg_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				ddrq_cfg->ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_PORT;
				count++;
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_QUEUE_ID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_obj.cfg_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				ddrq_cfg->ddrq_obj.cfg_type = NSS_DP_DDRQ_CFG_TYPE_QUEUE;
				count++;
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GET_CMD:
				error = ppecfg_param_get_bool(sub_params->data, &ddrq_cfg->get_cmd);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_GRN_MIN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_grn_min);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_RED_MIN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_min);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_RED_MAX:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_red_max);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_YEL_MIN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_min);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GAP_GRN_YEL_MAX:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_gap_grn_yel_max);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GRN_RESUME_OFFSET:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_grn_resume_offset);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_GRP_ID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_grp_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_PRE_ALLOC_LIMIT:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_pre_alloc_limit);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_RED_RESUME_OFFSET:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_red_resume_offset);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_YEL_RESUME_OFFSET:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_yel_resume_offset);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_CEILING:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_ceiling);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_DYNAMIC:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_dynamic);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_SHARED_WEIGHT:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_shared_weight);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_STATE:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_cfg->ddrq_ac_cfg.ddrq_state);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_AC_EN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_ac_en);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_BP_EN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_bp_en);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_COLOR_AWARE:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_color_aware);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_ECN_MARK_EN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_ecn_mark_en);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_CFG_RULE_WRED_EN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_cfg->ddrq_ac_cfg.ac_cfg_wred_en);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;
		}
	}

	if (!count || (count > 1)) {
		ppecfg_log_warn("Incorrect DDRQ object parameters\n");
		error = -EINVAL;
		goto done;
	}

	/*
	 * Validate the DDRQ configuration parameters
	 */
	error = ppecfg_edma_ddrq_config_validate(ddrq_cfg);
	if (error < 0) {
		ppecfg_log_warn("Incorrect DDRQ configuration parameters\n");
		goto done;
	}

	/*
	 * Send msg
	 */
	error = nss_ppenl_edma_ddrq_send_rule(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
	ppecfg_log_info("Successfully sent EDMA ddrq configuration message");
done:
	return error;
}

/*
 * ppecfg_edma_ddrq_grp_config()
 *	API to handle EDMA DDRQ group rule
 */
static int ppecfg_edma_ddrq_grp_config(struct ppecfg_param *param, struct ppecfg_param_in *match)
{
	struct nss_ppenl_edma_ddrq_rule nl_msg = {0};
	int error;
	struct ppecfg_param *sub_params;
	struct nss_ppenl_edma_ddrq_grp_config *ddrq_grp_cfg;

	if (!param || !match) {
		ppecfg_log_warn("Param or match table is NULL");
		return -EINVAL;
	}

	error = ppecfg_param_iter_tbl(param, match);
	if (error < 0) {
		ppecfg_log_arg_error(param);
		goto done;
	}

	nss_ppenl_edma_ddrq_init_rule(&nl_msg, NSS_PPE_EDMA_DDRQ_GRP_CFG_RULE_MSG);
	ddrq_grp_cfg = &nl_msg.msg.ddrq_grp_cfg;

	/*
	 * Set default values for all the config parameters
	 */
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_dp_thrd = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_red = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_yel = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_grn_resume_offset = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_red_resume_offset = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_yel_resume_offset = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_shrd_limit = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_ac_en = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_color_aware = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->grp_id = NSS_DP_DDRQ_INV_VAL;
	ddrq_grp_cfg->get_cmd = false;

	for (int index = PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRP_ID; index <= PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_SHRD_LIMIT; index++) {
		sub_params = &param->sub_params[index];
		if (sub_params->valid == false) {
			continue;
		}

		switch (index) {
			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRP_ID:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_grp_cfg->grp_id);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GET_CMD:
				error = ppecfg_param_get_bool(sub_params->data, &ddrq_grp_cfg->get_cmd);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_DP_THRD:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_dp_thrd);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_GRN_RED:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_red);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GRN_YEL:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_grn_yel);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_GRN_RESUME_OFFSET:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_grn_resume_offset);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_RED_RESUME_OFFSET:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_red_resume_offset);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_YEL_RESUME_OFFSET:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_yel_resume_offset);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_GAP_SHRD_LIMIT:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int16_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_grp_gap_shrd_limit);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_AC_EN:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_ac_en);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;

			case PPECFG_EDMA_DDRQ_GRP_CFG_RULE_COLOR_AWARE:
				error = ppecfg_param_get_int(sub_params->data, sizeof(int8_t), &ddrq_grp_cfg->ddrq_ac_grp_cfg.ac_cfg_color_aware);
				if (error < 0) {
					ppecfg_log_arg_error(sub_params);
					goto done;
				}
				break;
		}
	}

	/*
	 * Validate the DDRQ group configuration parameters
	 */
	error = ppecfg_edma_ddrq_grp_config_validate(ddrq_grp_cfg);
	if (error < 0) {
		ppecfg_log_warn("Incorrect DDRQ group configuration parameters\n");
		goto done;
	}

	/*
	 * Send msg
	 */
	error = nss_ppenl_edma_ddrq_send_rule(&nl_msg);
	if (error < 0) {
		ppecfg_log_warn("Unable to send message");
		goto done;
	}
	ppecfg_log_info("Successfully sent EDMA ddrq group configuration message");
done:
	return error;
}
