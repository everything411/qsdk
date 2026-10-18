/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_QM_API_H_
#define _HTTPPE_QM_API_H_

sw_error_t
httppe_mc_enq_ctrl_get(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value);

sw_error_t
httppe_mc_enq_ctrl_set(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value);

#define PORT_VSI_ENQUEUE_MAP_MAX_ENTRY	PORT_VSI_ENQUEUE_MAP_NUM

sw_error_t
httppe_port_vsi_enqueue_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vsi_enqueue_map_u *value);

sw_error_t
httppe_port_vsi_enqueue_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vsi_enqueue_map_u *value);

sw_error_t
httppe_port_vsi_enqueue_map_enqueue_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_port_vsi_enqueue_map_enqueue_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_port_vsi_enqueue_map_enqueue_vp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_port_vsi_enqueue_map_enqueue_vp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
httppe_flush_cfg_get(
		a_uint32_t dev_id,
		union flush_cfg_u *value);

sw_error_t
httppe_flush_cfg_set(
		a_uint32_t dev_id,
		union flush_cfg_u *value);

sw_error_t
httppe_flush_cfg_flush_all_queues_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_flush_cfg_flush_all_queues_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_flush_cfg_flush_busy_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_flush_cfg_flush_busy_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_flush_cfg_flush_dst_port_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_flush_cfg_flush_dst_port_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_flush_cfg_flush_qid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_flush_cfg_flush_qid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_flush_cfg_flush_status_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_flush_cfg_flush_status_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
httppe_flush_cfg_flush_wt_time_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
httppe_flush_cfg_flush_wt_time_set(
		a_uint32_t dev_id,
		unsigned int value);

#define AC_UNI_QUEUE_CFG_TBL_MAX_ENTRY	AC_UNI_QUEUE_CFG_TBL_NUM

sw_error_t
httppe_ac_uni_queue_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cfg_tbl_u *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cfg_tbl_u *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_color_aware_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_color_aware_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_force_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_force_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_grn_min_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_grn_min_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_min_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_min_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_min_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_min_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_grn_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_grn_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_grp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_grp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_pre_alloc_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_pre_alloc_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_red_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_red_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_ceiling_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_ceiling_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_dynamic_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_dynamic_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_weight_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_weight_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_wred_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_wred_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_yel_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cfg_tbl_ac_cfg_yel_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_MUL_QUEUE_CFG_TBL_MAX_ENTRY	AC_MUL_QUEUE_CFG_TBL_NUM

sw_error_t
httppe_ac_mul_queue_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cfg_tbl_u *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cfg_tbl_u *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_color_aware_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_color_aware_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_force_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_force_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_red_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_red_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_yel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_yel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_grn_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_grn_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_grp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_grp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_pre_alloc_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_pre_alloc_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_red_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_red_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_shared_ceiling_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_shared_ceiling_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_yel_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cfg_tbl_ac_cfg_yel_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_GRP_CFG_TBL_MAX_ENTRY	AC_GRP_CFG_TBL_NUM

sw_error_t
httppe_ac_grp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cfg_tbl_u *value);

sw_error_t
httppe_ac_grp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cfg_tbl_u *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_cfg_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_cfg_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_cfg_color_aware_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_cfg_color_aware_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_cfg_force_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_cfg_force_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_dp_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_dp_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_gap_grn_red_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_gap_grn_red_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_gap_grn_yel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_gap_grn_yel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_grn_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_grn_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_palloc_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_palloc_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_red_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_red_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_yel_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cfg_tbl_ac_grp_yel_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_UNI_QUEUE_CNT_TBL_MAX_ENTRY	AC_UNI_QUEUE_CNT_TBL_NUM

sw_error_t
httppe_ac_uni_queue_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cnt_tbl_u *value);

sw_error_t
httppe_ac_uni_queue_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cnt_tbl_u *value);

sw_error_t
httppe_ac_uni_queue_cnt_tbl_ac_uni_queue_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_cnt_tbl_ac_uni_queue_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_MUL_QUEUE_CNT_TBL_MAX_ENTRY	AC_MUL_QUEUE_CNT_TBL_NUM

sw_error_t
httppe_ac_mul_queue_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cnt_tbl_u *value);

sw_error_t
httppe_ac_mul_queue_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cnt_tbl_u *value);

sw_error_t
httppe_ac_mul_queue_cnt_tbl_ac_mul_queue_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_cnt_tbl_ac_mul_queue_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_GRP_CNT_TBL_MAX_ENTRY	AC_GRP_CNT_TBL_NUM

sw_error_t
httppe_ac_grp_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cnt_tbl_u *value);

sw_error_t
httppe_ac_grp_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cnt_tbl_u *value);

sw_error_t
httppe_ac_grp_cnt_tbl_ac_grp_alloc_used_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cnt_tbl_ac_grp_alloc_used_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_cnt_tbl_ac_grp_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_cnt_tbl_ac_grp_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_UNI_QUEUE_DROP_STATE_TBL_MAX_ENTRY	AC_UNI_QUEUE_DROP_STATE_TBL_NUM

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_drop_state_tbl_u *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_drop_state_tbl_u *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_grn_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_grn_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_grn_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_grn_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_red_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_red_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_red_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_red_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_yel_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_yel_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_yel_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_uni_queue_drop_state_tbl_yel_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_MUL_QUEUE_DROP_STATE_TBL_MAX_ENTRY	AC_MUL_QUEUE_DROP_STATE_TBL_NUM

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_drop_state_tbl_u *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_drop_state_tbl_u *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_grn_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_grn_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_grn_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_grn_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_red_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_red_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_red_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_red_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_yel_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_yel_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_yel_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_mul_queue_drop_state_tbl_yel_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_GRP_DROP_STATE_TBL_MAX_ENTRY	AC_GRP_DROP_STATE_TBL_NUM

sw_error_t
httppe_ac_grp_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_drop_state_tbl_u *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_drop_state_tbl_u *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_grn_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_grn_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_drop_state_tbl_grn_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_grn_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_drop_state_tbl_red_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_red_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_drop_state_tbl_red_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_red_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_drop_state_tbl_yel_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_yel_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_ac_grp_drop_state_tbl_yel_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_ac_grp_drop_state_tbl_yel_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_DEQ_OPR_TBL_MAX_ENTRY	OQ_DEQ_OPR_TBL_NUM

sw_error_t
httppe_oq_deq_opr_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_deq_opr_tbl_u *value);

sw_error_t
httppe_oq_deq_opr_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_deq_opr_tbl_u *value);

sw_error_t
httppe_oq_deq_opr_tbl_deq_drop_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_deq_opr_tbl_deq_drop_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_deq_opr_tbl_deq_high_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_deq_opr_tbl_deq_high_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_HEAD_UNI_TBL_MAX_ENTRY	OQ_HEAD_UNI_TBL_NUM

sw_error_t
httppe_oq_head_uni_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_uni_tbl_u *value);

sw_error_t
httppe_oq_head_uni_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_uni_tbl_u *value);

sw_error_t
httppe_oq_head_uni_tbl_empty_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_uni_tbl_empty_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_uni_tbl_head_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_uni_tbl_head_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_uni_tbl_head_pkt_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_uni_tbl_head_pkt_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_uni_tbl_tail_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_uni_tbl_tail_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_HEAD_MUL_TBL_MAX_ENTRY	OQ_HEAD_MUL_TBL_NUM

sw_error_t
httppe_oq_head_mul_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_mul_tbl_u *value);

sw_error_t
httppe_oq_head_mul_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_head_mul_tbl_u *value);

sw_error_t
httppe_oq_head_mul_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_mul_tbl_empty_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_empty_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_mul_tbl_head_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_head_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_mul_tbl_head_pkt_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_head_pkt_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_mul_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_mul_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_head_mul_tbl_tail_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_head_mul_tbl_tail_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_UNI_TBL_MAX_ENTRY	OQ_LL_UNI_TBL_NUM

sw_error_t
httppe_oq_ll_uni_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_uni_tbl_u *value);

sw_error_t
httppe_oq_ll_uni_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_uni_tbl_u *value);

sw_error_t
httppe_oq_ll_uni_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_uni_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P0_TBL_MAX_ENTRY	OQ_LL_MUL_P0_TBL_NUM

sw_error_t
httppe_oq_ll_mul_p0_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p0_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p0_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p0_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P1_TBL_MAX_ENTRY	OQ_LL_MUL_P1_TBL_NUM

sw_error_t
httppe_oq_ll_mul_p1_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p1_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p1_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p1_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P2_TBL_MAX_ENTRY	OQ_LL_MUL_P2_TBL_NUM

sw_error_t
httppe_oq_ll_mul_p2_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p2_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p2_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p2_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P3_TBL_MAX_ENTRY	OQ_LL_MUL_P3_TBL_NUM

sw_error_t
httppe_oq_ll_mul_p3_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p3_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p3_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p3_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P4_TBL_MAX_ENTRY	OQ_LL_MUL_P4_TBL_NUM

sw_error_t
httppe_oq_ll_mul_p4_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p4_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p4_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p4_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P5_TBL_MAX_ENTRY	OQ_LL_MUL_P5_TBL_NUM

sw_error_t
httppe_oq_ll_mul_p5_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p5_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p5_tbl_u *value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_oq_ll_mul_p5_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_queue_tx_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_tx_counter_tbl_u *value);

sw_error_t
httppe_queue_tx_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_tx_counter_tbl_u *value);

sw_error_t
httppe_uni_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uni_drop_cnt_tbl_u *value);

sw_error_t
httppe_uni_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uni_drop_cnt_tbl_u *value);

#endif
