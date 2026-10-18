/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw.h"
#include "hsl_htt_reg.h"
#include "fal_qm.h"
#include "adpt.h"
#include "hppe_global_reg.h"
#include "hppe_global.h"

#define UCAST_QUEUE_ID_MAX	256
#define ALL_QUEUE_ID_MAX	300
#define UCAST_QUEUE_ITEMS	6
#define MCAST_QUEUE_ITEMS	3
#define DROP_INC	0x10

extern sw_error_t adpt_hppe_qm_enqueue_ctrl_get(a_uint32_t dev_id, a_uint32_t queue_id,
		a_bool_t *enable);

sw_error_t
adpt_httppe_queue_flush(
		a_uint32_t dev_id,
		fal_port_t port,
		a_uint16_t queue_id)
{
#define PPE_QUEUE_FLUSH_CHECK_ROUND_MAX		0X2000
	union flush_cfg_u flush_cfg;
	union clk_gating_ctrl_u clk_gate_ctrl;
	a_uint32_t round = PPE_QUEUE_FLUSH_CHECK_ROUND_MAX, qm_clk_gate_bak = 0;
	sw_error_t rv;
	a_bool_t enable = A_TRUE;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&flush_cfg, 0, sizeof(flush_cfg));
	memset(&clk_gate_ctrl, 0, sizeof(clk_gate_ctrl));


	if (queue_id < ALL_QUEUE_ID_MAX) {
		rv = adpt_hppe_qm_enqueue_ctrl_get(dev_id, queue_id, &enable);
		SW_RTN_ON_ERROR(rv);

		if (enable) {
			SSDK_ERROR("queue %d should be disabled before flushing queue\n",
					queue_id);
			return SW_NOT_READY;
		}
	}

	httppe_flush_cfg_get(dev_id, &flush_cfg);

	if (queue_id == 0xffff) {
		flush_cfg.bf.flush_all_queues = 1;
		flush_cfg.bf.flush_qid = 0;
	} else {
		flush_cfg.bf.flush_all_queues = 0;
		flush_cfg.bf.flush_qid = queue_id;
	}
	flush_cfg.bf.flush_dst_port = FAL_PORT_ID_VALUE(port);
	flush_cfg.bf.flush_busy = 1;

	hppe_clk_gating_ctrl_get(dev_id, &clk_gate_ctrl);
	qm_clk_gate_bak = clk_gate_ctrl.bf.qm_clk_gate_en;

	/* Disable QM clock gate for accelerating queue flush */
	clk_gate_ctrl.bf.qm_clk_gate_en = A_FALSE;
	hppe_clk_gating_ctrl_set(dev_id, &clk_gate_ctrl);

	httppe_flush_cfg_set(dev_id, &flush_cfg);
	while (flush_cfg.bf.flush_busy && --round) {
		httppe_flush_cfg_get(dev_id, &flush_cfg);
	}

	/* Restore QM clock gate */
	clk_gate_ctrl.bf.qm_clk_gate_en = qm_clk_gate_bak;
	hppe_clk_gating_ctrl_set(dev_id, &clk_gate_ctrl);

	if (round == 0) {
		SSDK_ERROR("queue %d flush busily\n", queue_id);
		return SW_BUSY;
	}

	if (!flush_cfg.bf.flush_status) {
		SSDK_ERROR("queue %d flush fail %d\n", queue_id, flush_cfg.bf.flush_status);
		return SW_FAIL;
	}

	SSDK_DEBUG("queue %d flush with round %d successfully\n", queue_id,
			PPE_QUEUE_FLUSH_CHECK_ROUND_MAX - round);

	return SW_OK;
}

sw_error_t
adpt_httppe_ac_dynamic_threshold_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg)
{
	sw_error_t rv = SW_OK;
	union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;

	memset(&ac_uni_queue_cfg_tbl, 0, sizeof(ac_uni_queue_cfg_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	rv = httppe_ac_uni_queue_cfg_tbl_get(dev_id, queue_id, &ac_uni_queue_cfg_tbl);
	if( rv != SW_OK )
		return rv;

	cfg->wred_enable = ac_uni_queue_cfg_tbl.bf.ac_cfg_wred_en;
	cfg->color_enable = ac_uni_queue_cfg_tbl.bf.ac_cfg_color_aware;
	cfg->shared_weight = ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_weight;
	cfg->green_min_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_grn_min;
	cfg->yel_max_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_max;
	cfg->yel_min_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_0 | \
					ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET);
	cfg->red_max_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_max;
	cfg->red_min_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_min;
	cfg->yel_resume_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_yel_resume_offset;
	cfg->red_resume_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_0 | \
					ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET);
	cfg->ceiling = ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_0 |
		ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET);
	cfg->green_resume_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_0 |
		ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET);
	cfg->status = ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_dynamic;

	return SW_OK;
}

sw_error_t
adpt_httppe_ac_dynamic_threshold_set(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		fal_ac_dynamic_threshold_t *cfg)
{
	sw_error_t rv = SW_OK;
	union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;

	memset(&ac_uni_queue_cfg_tbl, 0, sizeof(ac_uni_queue_cfg_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	rv = httppe_ac_uni_queue_cfg_tbl_get(dev_id, queue_id, &ac_uni_queue_cfg_tbl);
	if( rv != SW_OK )
		return rv;

	ac_uni_queue_cfg_tbl.bf.ac_cfg_wred_en = cfg->wred_enable;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_color_aware = cfg->color_enable;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_dynamic = 1;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_grn_min = cfg->green_min_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_max = cfg->yel_max_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_0 = cfg->yel_min_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_1 = cfg->yel_min_off >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET);
	ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_max = cfg->red_max_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_min = cfg->red_min_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_0 = cfg->green_resume_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_1 = cfg->green_resume_off >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET);
	ac_uni_queue_cfg_tbl.bf.ac_cfg_yel_resume_offset = cfg->yel_resume_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_0 = cfg->red_resume_off;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_1 = cfg->red_resume_off >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET);
	ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_weight = cfg->shared_weight;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_0 = cfg->ceiling;
	ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_1 = cfg->ceiling >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET);

	return httppe_ac_uni_queue_cfg_tbl_set(dev_id, queue_id, &ac_uni_queue_cfg_tbl);
}

sw_error_t
adpt_httppe_ac_static_threshold_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg)
{
	sw_error_t rv;
	ADPT_DEV_ID_CHECK(dev_id);

	cfg->status = A_TRUE;

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		rv = httppe_ac_grp_cfg_tbl_get(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

		cfg->color_enable = ac_grp_cfg_tbl.bf.ac_cfg_color_aware;
		cfg->green_max = ac_grp_cfg_tbl.bf.ac_grp_dp_thrd_0 |
			ac_grp_cfg_tbl.bf.ac_grp_dp_thrd_1 << SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_DP_THRD_OFFSET);
		cfg->yel_max_off = ac_grp_cfg_tbl.bf.ac_grp_gap_grn_yel;
		cfg->red_max_off = ac_grp_cfg_tbl.bf.ac_grp_gap_grn_red;
		cfg->green_resume_off = ac_grp_cfg_tbl.bf.ac_grp_grn_resume_offset;
		cfg->yel_resume_off = ac_grp_cfg_tbl.bf.ac_grp_yel_resume_offset_0 |
					ac_grp_cfg_tbl.bf.ac_grp_yel_resume_offset_1 << SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_OFFSET);
		cfg->red_resume_off = ac_grp_cfg_tbl.bf.ac_grp_red_resume_offset;

		return rv;

	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->obj_id < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
			rv = httppe_ac_uni_queue_cfg_tbl_get(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);
			cfg->wred_enable = ac_uni_queue_cfg_tbl.bf.ac_cfg_wred_en;
			cfg->color_enable = ac_uni_queue_cfg_tbl.bf.ac_cfg_color_aware;
			cfg->green_min_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_grn_min;
			cfg->yel_max_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_max;
			cfg->yel_min_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_0 |
					ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET);
			cfg->red_max_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_max;
			cfg->red_min_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_min;
			cfg->green_resume_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_0 |
				ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET);
			cfg->yel_resume_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_yel_resume_offset;
			cfg->red_resume_off = ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_0 |
					ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET);
			cfg->green_max = ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_0 |
				ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_1 << SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET);
			cfg->status = !ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_dynamic;
			return rv;

		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
			rv = httppe_ac_mul_queue_cfg_tbl_get(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
			cfg->color_enable = ac_mul_queue_cfg_tbl.bf.ac_cfg_color_aware;
			cfg->green_max = ac_mul_queue_cfg_tbl.bf.ac_cfg_shared_ceiling;
			cfg->red_max_off = ac_mul_queue_cfg_tbl.bf.ac_cfg_gap_grn_red;
			cfg->yel_max_off= ac_mul_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_0 |
						ac_mul_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_1 << SW_FIELD_OFFSET_IN_WORD(AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_OFFSET);
			cfg->green_resume_off = ac_mul_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset;
			cfg->yel_resume_off = ac_mul_queue_cfg_tbl.bf.ac_cfg_yel_resume_offset;
			cfg->red_resume_off = ac_mul_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_0 |
				ac_mul_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_1 << SW_FIELD_OFFSET_IN_WORD(AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET);
			return rv;
		}
	} else
		return SW_FAIL;
}

sw_error_t
adpt_httppe_ac_static_threshold_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_static_threshold_t *cfg)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		httppe_ac_grp_cfg_tbl_get(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

		ac_grp_cfg_tbl.bf.ac_cfg_color_aware = cfg->color_enable;
		ac_grp_cfg_tbl.bf.ac_grp_dp_thrd_0 = cfg->green_max;
		ac_grp_cfg_tbl.bf.ac_grp_dp_thrd_1 = cfg->green_max >> SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_DP_THRD_OFFSET);
		ac_grp_cfg_tbl.bf.ac_grp_gap_grn_yel = cfg->yel_max_off;
		ac_grp_cfg_tbl.bf.ac_grp_gap_grn_red = cfg->red_max_off;
		ac_grp_cfg_tbl.bf.ac_grp_grn_resume_offset = cfg->green_resume_off;
		ac_grp_cfg_tbl.bf.ac_grp_yel_resume_offset_0 = cfg->yel_resume_off;
		ac_grp_cfg_tbl.bf.ac_grp_yel_resume_offset_1 = cfg->yel_resume_off >> SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_YEL_RESUME_OFFSET_OFFSET);
		ac_grp_cfg_tbl.bf.ac_grp_red_resume_offset = cfg->red_resume_off;

		return httppe_ac_grp_cfg_tbl_set(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->obj_id < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
			httppe_ac_uni_queue_cfg_tbl_get(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_wred_en = cfg->wred_enable;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_color_aware = cfg->color_enable;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_dynamic = 0;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_grn_min = cfg->green_min_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_max = cfg->yel_max_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_0 = cfg->yel_min_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_min_1 = cfg->yel_min_off >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_MIN_OFFSET);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_max = cfg->red_max_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_gap_grn_red_min = cfg->red_min_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_yel_resume_offset = cfg->yel_resume_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_0 = cfg->red_resume_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_1 = cfg->red_resume_off >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_0 = cfg->green_resume_off;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset_1 = cfg->green_resume_off >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_GRN_RESUME_OFFSET_OFFSET);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_0 = cfg->green_max;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_shared_ceiling_1 = cfg->green_max >> SW_FIELD_OFFSET_IN_WORD(AC_UNI_QUEUE_CFG_TBL_AC_CFG_SHARED_CEILING_OFFSET);
			return httppe_ac_uni_queue_cfg_tbl_set(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);;

		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
			httppe_ac_mul_queue_cfg_tbl_get(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
			ac_mul_queue_cfg_tbl.bf.ac_cfg_color_aware = cfg->color_enable;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_grn_resume_offset = cfg->green_resume_off;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_yel_resume_offset = cfg->yel_resume_off;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_0 = cfg->red_resume_off;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_red_resume_offset_1 = cfg->red_resume_off >> SW_FIELD_OFFSET_IN_WORD(AC_MUL_QUEUE_CFG_TBL_AC_CFG_RED_RESUME_OFFSET_OFFSET);
			ac_mul_queue_cfg_tbl.bf.ac_cfg_gap_grn_red = cfg->red_max_off;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_0 = cfg->yel_max_off;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_gap_grn_yel_1 = cfg->yel_max_off >> SW_FIELD_OFFSET_IN_WORD(AC_MUL_QUEUE_CFG_TBL_AC_CFG_GAP_GRN_YEL_OFFSET);
			ac_mul_queue_cfg_tbl.bf.ac_cfg_shared_ceiling = cfg->green_max;
			return httppe_ac_mul_queue_cfg_tbl_set(dev_id,
					obj->obj_id -UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
		}
	} else
		return SW_FAIL;
}

sw_error_t
adpt_httppe_ac_ctrl_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_ctrl_t *cfg)
{
	sw_error_t rv = SW_OK;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));
		rv = httppe_ac_grp_cfg_tbl_get(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

		cfg->ac_en = ac_grp_cfg_tbl.bf.ac_cfg_ac_en;
		cfg->ac_fc_en = ac_grp_cfg_tbl.bf.ac_cfg_force_ac_en;
	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->obj_id < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
			rv = httppe_ac_uni_queue_cfg_tbl_get(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);
			cfg->ac_en = ac_uni_queue_cfg_tbl.bf.ac_cfg_ac_en;
			cfg->ac_fc_en = ac_uni_queue_cfg_tbl.bf.ac_cfg_force_ac_en;

		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
			rv = httppe_ac_mul_queue_cfg_tbl_get(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
			cfg->ac_en = ac_mul_queue_cfg_tbl.bf.ac_cfg_ac_en;
			cfg->ac_fc_en = ac_mul_queue_cfg_tbl.bf.ac_cfg_force_ac_en;
		}
	} else
		return SW_FAIL;

	return rv;
}

sw_error_t
adpt_httppe_ac_ctrl_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		fal_ac_ctrl_t *cfg)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));
		httppe_ac_grp_cfg_tbl_get(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

		ac_grp_cfg_tbl.bf.ac_cfg_ac_en = cfg->ac_en;
		ac_grp_cfg_tbl.bf.ac_cfg_force_ac_en = cfg->ac_fc_en;
		return httppe_ac_grp_cfg_tbl_set(dev_id, obj->obj_id, &ac_grp_cfg_tbl);
	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->obj_id < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
			httppe_ac_uni_queue_cfg_tbl_get(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_ac_en = cfg->ac_en;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_force_ac_en = cfg->ac_fc_en;
			return httppe_ac_uni_queue_cfg_tbl_set(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);

		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
			httppe_ac_mul_queue_cfg_tbl_get(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
			ac_mul_queue_cfg_tbl.bf.ac_cfg_ac_en = cfg->ac_en;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_force_ac_en = cfg->ac_fc_en;
			return httppe_ac_mul_queue_cfg_tbl_set(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
		}
	} else
		return SW_FAIL;

}

sw_error_t
adpt_httppe_ac_queue_group_get(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		a_uint8_t *group_id)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(group_id);


	if (queue_id < UCAST_QUEUE_ID_MAX) {
		union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
		rv = httppe_ac_uni_queue_cfg_tbl_get(dev_id,
				queue_id,
				&ac_uni_queue_cfg_tbl);
		*group_id = ac_uni_queue_cfg_tbl.bf.ac_cfg_grp_id;

	} else {
		union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
		rv = httppe_ac_mul_queue_cfg_tbl_get(dev_id,
				queue_id - UCAST_QUEUE_ID_MAX,
				&ac_mul_queue_cfg_tbl);
		*group_id = ac_mul_queue_cfg_tbl.bf.ac_cfg_grp_id;
	}

	return rv;
}

sw_error_t
adpt_httppe_ac_queue_group_set(
		a_uint32_t dev_id,
		a_uint32_t queue_id,
		a_uint8_t group_id)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);


	if (queue_id < UCAST_QUEUE_ID_MAX) {
		union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
		httppe_ac_uni_queue_cfg_tbl_get(dev_id,
				queue_id,
				&ac_uni_queue_cfg_tbl);
		ac_uni_queue_cfg_tbl.bf.ac_cfg_grp_id = group_id;
		return httppe_ac_uni_queue_cfg_tbl_set(dev_id,
				queue_id,
				&ac_uni_queue_cfg_tbl);

	} else {
		union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
		httppe_ac_mul_queue_cfg_tbl_get(dev_id,
				queue_id - UCAST_QUEUE_ID_MAX,
				&ac_mul_queue_cfg_tbl);
		ac_mul_queue_cfg_tbl.bf.ac_cfg_grp_id = group_id;
		return httppe_ac_mul_queue_cfg_tbl_set(dev_id,
				queue_id - UCAST_QUEUE_ID_MAX,
				&ac_mul_queue_cfg_tbl);
	}

	return rv;
}

sw_error_t
adpt_httppe_ac_group_buffer_get(
		a_uint32_t dev_id,
		a_uint8_t group_id,
		fal_ac_group_buffer_t *cfg)
{
	sw_error_t rv = SW_OK;
	union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

	memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	rv = httppe_ac_grp_cfg_tbl_get(dev_id, group_id, &ac_grp_cfg_tbl);
	if( rv != SW_OK )
		return rv;

	cfg->prealloc_buffer = ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_0 |
		ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_1 << SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET);
	cfg->total_buffer = ac_grp_cfg_tbl.bf.ac_grp_limit;

	return SW_OK;
}

sw_error_t
adpt_httppe_ac_group_buffer_set(
		a_uint32_t dev_id,
		a_uint8_t group_id,
		fal_ac_group_buffer_t *cfg)
{
	sw_error_t rv = SW_OK;
	union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

	memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	rv = httppe_ac_grp_cfg_tbl_get(dev_id, group_id, &ac_grp_cfg_tbl);
	if( rv != SW_OK )
		return rv;

	ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_0 = cfg->prealloc_buffer;
	ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_1 = cfg->prealloc_buffer >> SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET);
	ac_grp_cfg_tbl.bf.ac_grp_limit = cfg->total_buffer;

	return httppe_ac_grp_cfg_tbl_set(dev_id, group_id, &ac_grp_cfg_tbl);;
}

sw_error_t
adpt_httppe_ac_prealloc_buffer_set(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		a_uint16_t num)
{
	ADPT_DEV_ID_CHECK(dev_id);

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));
		httppe_ac_grp_cfg_tbl_get(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

		ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_0 = num;
		ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_1 = num >> SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET);

		return httppe_ac_grp_cfg_tbl_set(dev_id, obj->obj_id, &ac_grp_cfg_tbl);

	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->obj_id < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
			httppe_ac_uni_queue_cfg_tbl_get(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_pre_alloc_limit = num;
			return httppe_ac_uni_queue_cfg_tbl_set(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);;

		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
			httppe_ac_mul_queue_cfg_tbl_get(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
			ac_mul_queue_cfg_tbl.bf.ac_cfg_pre_alloc_limit = num;
			return httppe_ac_mul_queue_cfg_tbl_set(dev_id,
					obj->obj_id -UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
		}
	} else
		return SW_FAIL;
}

sw_error_t
adpt_httppe_ac_prealloc_buffer_get(
		a_uint32_t dev_id,
		fal_ac_obj_t *obj,
		a_uint16_t *num)
{
	sw_error_t rv = SW_OK;
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(num);

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		rv = httppe_ac_grp_cfg_tbl_get(dev_id, obj->obj_id, &ac_grp_cfg_tbl);
		*num = ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_0 |
			ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_1 << SW_FIELD_OFFSET_IN_WORD(AC_GRP_CFG_TBL_AC_GRP_PALLOC_LIMIT_OFFSET);

		return rv;

	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->obj_id < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;
			rv = httppe_ac_uni_queue_cfg_tbl_get(dev_id,
					obj->obj_id,
					&ac_uni_queue_cfg_tbl);
			*num = ac_uni_queue_cfg_tbl.bf.ac_cfg_pre_alloc_limit;
		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;
			rv = httppe_ac_mul_queue_cfg_tbl_get(dev_id,
					obj->obj_id - UCAST_QUEUE_ID_MAX,
					&ac_mul_queue_cfg_tbl);
			*num = ac_mul_queue_cfg_tbl.bf.ac_cfg_pre_alloc_limit;
		}
		return rv;
	} else
		return SW_FAIL;
}

extern a_uint32_t adpt_hppe_mcast_queue_dropcnt_start_addr_get(a_uint32_t dev_id, a_uint32_t queue_id);
sw_error_t
adpt_httppe_queue_counter_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_queue_stats_t *info)
{
	sw_error_t rv = SW_OK;
	union queue_tx_counter_tbl_u tx_cnt;
	union ac_mul_queue_cnt_tbl_u mul_cnt;
	union ac_uni_queue_cnt_tbl_u uni_cnt;
	a_uint32_t i = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(info);

	if (queue_id >= ALL_QUEUE_ID_MAX)
		return SW_BAD_VALUE;

	rv = httppe_queue_tx_counter_tbl_get(dev_id, queue_id, &tx_cnt);
	if( rv != SW_OK )
		return rv;
	if (queue_id >= UCAST_QUEUE_ID_MAX) {
		a_uint32_t start_addr = 0;
		union mul_p5_drop_cnt_tbl_u drop_cnt;

		rv = httppe_ac_mul_queue_cnt_tbl_get(dev_id,
				queue_id - UCAST_QUEUE_ID_MAX, &mul_cnt);
		if( rv != SW_OK )
			return rv;
		info->pending_buff_num = mul_cnt.bf.ac_mul_queue_cnt;
		start_addr = adpt_hppe_mcast_queue_dropcnt_start_addr_get(dev_id, queue_id);
		for (i = 0; i < MCAST_QUEUE_ITEMS; i++) {
			hppe_reg_tbl_get(dev_id, start_addr + i*DROP_INC, drop_cnt.val, 3);
			info->drop_packets[i+3] = drop_cnt.bf.mul_p5_drop_pkt;
			info->drop_bytes[i+3] = (a_uint64_t)drop_cnt.bf.mul_p5_drop_byte_0 |
					(a_uint64_t)drop_cnt.bf.mul_p5_drop_byte_1 <<32;
		}
	} else {
		union uni_drop_cnt_tbl_u uni_drop_cnt;

		rv = httppe_ac_uni_queue_cnt_tbl_get(dev_id, queue_id, &uni_cnt);
		if( rv != SW_OK )
			return rv;
		info->pending_buff_num = uni_cnt.bf.ac_uni_queue_cnt;
		for (i = 0; i < UCAST_QUEUE_ITEMS; i++) {
			httppe_uni_drop_cnt_tbl_get(dev_id, queue_id*UCAST_QUEUE_ITEMS+i, &uni_drop_cnt);
			info->drop_packets[i] = uni_drop_cnt.bf.uni_drop_pkt;
			info->drop_bytes[i] = (a_uint64_t)uni_drop_cnt.bf.uni_drop_byte_0 |
					(a_uint64_t)uni_drop_cnt.bf.uni_drop_byte_1 <<32;
		}
	}
	info->tx_packets = tx_cnt.bf.tx_packets;
	info->tx_bytes = (a_uint64_t)tx_cnt.bf.tx_bytes_0 | (a_uint64_t)tx_cnt.bf.tx_bytes_1 << 32;

	return SW_OK;
}

sw_error_t
adpt_httppe_qm_threshold_reset(a_uint32_t dev_id, a_uint32_t queue_id)
{
	a_uint16_t ceiling, green_max, weight, resume_offset;
	adpt_ppe_type_t chip_type = adpt_ppe_type_get(dev_id);

	switch (chip_type) {
		case HTTPPE_TYPE:
			ceiling = 2200;
			weight = 7;
			resume_offset = 36;
			green_max = 250;
			break;
		default:
			SSDK_ERROR("Unsupported chip type: %d\n", chip_type);
			return SW_OUT_OF_RANGE;
	}

	if (queue_id < SSDK_L0SCHEDULER_UCASTQ_CFG_MAX) {
		fal_ac_dynamic_threshold_t  dthresh_cfg;

		memset(&dthresh_cfg, 0, sizeof(dthresh_cfg));
		dthresh_cfg.shared_weight = weight;
		dthresh_cfg.ceiling = ceiling;
		dthresh_cfg.green_resume_off = resume_offset;

		return adpt_httppe_ac_dynamic_threshold_set(dev_id, queue_id, &dthresh_cfg);
	} else {
		fal_ac_static_threshold_t sthresh_cfg;
		fal_ac_obj_t obj;

		memset(&sthresh_cfg, 0, sizeof(sthresh_cfg));
		sthresh_cfg.green_max = green_max;
		sthresh_cfg.green_resume_off = resume_offset;

		obj.type = FAL_AC_QUEUE;
		obj.obj_id = queue_id;

		return adpt_httppe_ac_static_threshold_set(dev_id, &obj, &sthresh_cfg);
	}
}

sw_error_t
adpt_httppe_queue_counter_ctrl_get(a_uint32_t dev_id, a_bool_t *cnt_en)
{
	sw_error_t rv = SW_OK;
	union eg_bridge_config_u eg_bridge_config;

	memset(&eg_bridge_config, 0, sizeof(eg_bridge_config));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cnt_en);

	rv = httppe_eg_bridge_config_get(dev_id, &eg_bridge_config);
	if( rv != SW_OK )
		return rv;

	*cnt_en = eg_bridge_config.bf.queue_cnt_en;

	return SW_OK;
}

sw_error_t
adpt_httppe_queue_counter_ctrl_set(a_uint32_t dev_id, a_bool_t cnt_en)
{
	sw_error_t rv = SW_OK;
	union eg_bridge_config_u eg_bridge_config;

	memset(&eg_bridge_config, 0, sizeof(eg_bridge_config));
	ADPT_DEV_ID_CHECK(dev_id);


	rv = httppe_eg_bridge_config_get(dev_id, &eg_bridge_config);
	if( rv != SW_OK )
		return rv;

	eg_bridge_config.bf.queue_cnt_en = cnt_en;
	return httppe_eg_bridge_config_set(dev_id, &eg_bridge_config);
}

sw_error_t
adpt_httppe_queue_counter_cleanup(a_uint32_t dev_id, a_uint32_t queue_id)
{
	union queue_tx_counter_tbl_u tx_cnt = {0};
	a_uint32_t i = 0;
	a_uint32_t val[3] = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	if (queue_id >= ALL_QUEUE_ID_MAX)
		return SW_BAD_VALUE;

	httppe_queue_tx_counter_tbl_set(dev_id, queue_id, &tx_cnt);

	if (queue_id >= UCAST_QUEUE_ID_MAX) {
		a_uint32_t start_addr = 0;

		start_addr = adpt_hppe_mcast_queue_dropcnt_start_addr_get(dev_id, queue_id);
		for (i = 0; i < MCAST_QUEUE_ITEMS; i++) {
			hppe_reg_tbl_set(dev_id, start_addr + i*DROP_INC, val, 3);
		}
	} else {
		union uni_drop_cnt_tbl_u uni_drop_cnt;

		memset(&uni_drop_cnt, 0, sizeof(uni_drop_cnt));
		for (i = 0; i < UCAST_QUEUE_ITEMS; i++) {
			httppe_uni_drop_cnt_tbl_set(dev_id, queue_id*UCAST_QUEUE_ITEMS+i, &uni_drop_cnt);
		}
	}

	return SW_OK;
}

sw_error_t
adpt_httppe_qm_port_source_profile_set(
		a_uint32_t dev_id, fal_port_t port, a_uint32_t src_profile)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;
	a_uint32_t index = FAL_PORT_ID_VALUE(port);

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&mru_mtu_ctrl_tbl, 0, sizeof(mru_mtu_ctrl_tbl));

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, index, &mru_mtu_ctrl_tbl);
	if (rv != SW_OK)
		return rv;

	mru_mtu_ctrl_tbl.bf.src_profile = src_profile;
	return httppe_mru_mtu_ctrl_tbl_set(dev_id, index, &mru_mtu_ctrl_tbl);
}

sw_error_t
adpt_httppe_qm_port_source_profile_get(
		a_uint32_t dev_id, fal_port_t port, a_uint32_t *src_profile)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;
	a_uint32_t index = FAL_PORT_ID_VALUE(port);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(src_profile);
	memset(&mru_mtu_ctrl_tbl, 0, sizeof(mru_mtu_ctrl_tbl));

	rv = httppe_mru_mtu_ctrl_tbl_get(dev_id, index, &mru_mtu_ctrl_tbl);
	if (rv != SW_OK)
		return rv;

	*src_profile = mru_mtu_ctrl_tbl.bf.src_profile;

	return SW_OK;
}

sw_error_t
adpt_httppe_qm_mcast_enqueue_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t ucast_enqueue_en)
{
	union mc_enq_ctrl_u reg_val;
	struct qca_phy_priv *priv;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (FAL_PORT_ID_VALUE(port_id) > SW_MAX_NR_PORT)
		return SW_OUT_OF_RANGE;

	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	aos_mutex_lock(&priv->ppe_qm_lock.qm_mutex_lock);

	rv = httppe_mc_enq_ctrl_get(dev_id, &reg_val);
	if (rv != SW_OK)
		goto unlock_and_exit;

	if (ucast_enqueue_en == A_TRUE) {
		/* nothing to do if mc_enq_ctrl is enabled on the same port */
		if (reg_val.bf.uc_enq_en == A_TRUE &&
				reg_val.bf.uc_port_id == FAL_PORT_ID_VALUE(port_id)) {
			rv = SW_OK;
			goto unlock_and_exit;
		} else {
			reg_val.bf.uc_enq_en = A_TRUE;
			reg_val.bf.uc_port_id = FAL_PORT_ID_VALUE(port_id);
		}

	} else {
		/* only need to disable if mc_enq_ctrl is enabled on the same port,
		 * nothing to do if mc_enq_ctrl is disabled or enabled on another port.
		 */
		if (reg_val.bf.uc_enq_en == A_TRUE &&
				reg_val.bf.uc_port_id == FAL_PORT_ID_VALUE(port_id))
			reg_val.bf.uc_enq_en = A_FALSE;
		else {
			rv = SW_OK;
			goto unlock_and_exit;
		}

	}

	rv = httppe_mc_enq_ctrl_set(dev_id, &reg_val);

unlock_and_exit:
	aos_mutex_unlock(&priv->ppe_qm_lock.qm_mutex_lock);
	return rv;
}

sw_error_t
adpt_httppe_qm_mcast_enqueue_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
		a_bool_t *ucast_enqueue_en)
{
	union mc_enq_ctrl_u reg_val;
	struct qca_phy_priv *priv;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ucast_enqueue_en);

	if (FAL_PORT_ID_VALUE(port_id) > SW_MAX_NR_PORT)
		return SW_OUT_OF_RANGE;

	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	aos_mutex_lock(&priv->ppe_qm_lock.qm_mutex_lock);

	rv = httppe_mc_enq_ctrl_get(dev_id, &reg_val);
	if (rv != SW_OK) {
		aos_mutex_unlock(&priv->ppe_qm_lock.qm_mutex_lock);
		return rv;
	}

	if (reg_val.bf.uc_enq_en == A_TRUE && reg_val.bf.uc_port_id == FAL_PORT_ID_VALUE(port_id))
		*ucast_enqueue_en = A_TRUE;
	else
		*ucast_enqueue_en = A_FALSE;

	aos_mutex_unlock(&priv->ppe_qm_lock.qm_mutex_lock);

	return SW_OK;
}
