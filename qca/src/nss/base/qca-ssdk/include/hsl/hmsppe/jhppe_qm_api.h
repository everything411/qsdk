/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_QM_API_H_
#define _JHPPE_QM_API_H_


sw_error_t
jhppe_mc_enq_ctrl_get(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value);

sw_error_t
jhppe_mc_enq_ctrl_set(
		a_uint32_t dev_id,
		union mc_enq_ctrl_u *value);

sw_error_t
jhppe_mc_enq_ctrl_dot1p_pon_vp_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_mc_enq_ctrl_dot1p_pon_vp_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_mc_enq_ctrl_uc_enq_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_mc_enq_ctrl_uc_enq_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_mc_enq_ctrl_uc_port_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_mc_enq_ctrl_uc_port_id_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_enq_ctrl_ext_get(
		a_uint32_t dev_id,
		union enq_ctrl_ext_u *value);

sw_error_t
jhppe_enq_ctrl_ext_set(
		a_uint32_t dev_id,
		union enq_ctrl_ext_u *value);

sw_error_t
jhppe_enq_ctrl_ext_cpu_code_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_ext_cpu_code_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_ext_cpu_code_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_ext_cpu_code_1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_ext_cpu_code_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_ext_cpu_code_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_ext_drop_cpu_code_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_ext_drop_cpu_code_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_ext_rsv_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_ext_rsv_set(
		a_uint32_t dev_id,
		unsigned int value);

#define UNIQ_FLOWCTRL_STATUS_MAX_ENTRY	UNIQ_FLOWCTRL_STATUS_NUM

sw_error_t
jhppe_uniq_flowctrl_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_flowctrl_status_u *value);

sw_error_t
jhppe_uniq_flowctrl_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_flowctrl_status_u *value);

sw_error_t
jhppe_uniq_flowctrl_status_uniq_flowctrl_status_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_uniq_flowctrl_status_uniq_flowctrl_status_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UNIQ_Q_MAP_TBL_MAX_ENTRY	UNIQ_Q_MAP_TBL_NUM

sw_error_t
jhppe_uniq_q_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_q_map_tbl_u *value);

sw_error_t
jhppe_uniq_q_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uniq_q_map_tbl_u *value);

sw_error_t
jhppe_uniq_q_map_tbl_queue_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_uniq_q_map_tbl_queue_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);


sw_error_t
jhppe_enq_ctrl_get(
		a_uint32_t dev_id,
		union enq_ctrl_u *value);

sw_error_t
jhppe_enq_ctrl_set(
		a_uint32_t dev_id,
		union enq_ctrl_u *value);

sw_error_t
jhppe_enq_ctrl_pass_through0_direc_enq_dis_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through0_direc_enq_dis_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_eg_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_eg_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through0_src_profile_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through1_direc_enq_dis_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through1_direc_enq_dis_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_eg_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_eg_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through1_src_profile_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through2_direc_enq_dis_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through2_direc_enq_dis_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_eg_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_eg_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through2_src_profile_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through3_direc_enq_dis_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through3_direc_enq_dis_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_eg_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_eg_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_pass_through3_src_profile_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_qid_mismatch_enq_cpu_code_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_qid_mismatch_enq_cpu_code_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_queue_map_rule_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_queue_map_rule_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_enq_ctrl_uc_enq_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_enq_ctrl_uc_enq_en_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_cm_ctrl_0_get(
		a_uint32_t dev_id,
		union cm_ctrl_0_u *value);

sw_error_t
jhppe_cm_ctrl_0_set(
		a_uint32_t dev_id,
		union cm_ctrl_0_u *value);

sw_error_t
jhppe_cm_ctrl_0_park_fifo_free_head_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_0_park_fifo_free_head_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_0_park_fifo_free_tail_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_0_park_fifo_free_tail_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_0_park_fifo_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_0_park_fifo_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_cm_ctrl_1_get(
		a_uint32_t dev_id,
		union cm_ctrl_1_u *value);

sw_error_t
jhppe_cm_ctrl_1_set(
		a_uint32_t dev_id,
		union cm_ctrl_1_u *value);

sw_error_t
jhppe_cm_ctrl_1_park_fifo_p0_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_1_park_fifo_p0_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_1_park_fifo_p1_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_1_park_fifo_p1_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_1_park_fifo_p2_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_1_park_fifo_p2_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_cm_ctrl_2_get(
		a_uint32_t dev_id,
		union cm_ctrl_2_u *value);

sw_error_t
jhppe_cm_ctrl_2_set(
		a_uint32_t dev_id,
		union cm_ctrl_2_u *value);

sw_error_t
jhppe_cm_ctrl_2_park_fifo_p3_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_2_park_fifo_p3_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_2_park_fifo_p4_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_2_park_fifo_p4_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_2_park_fifo_p5_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_2_park_fifo_p5_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_cm_ctrl_3_get(
		a_uint32_t dev_id,
		union cm_ctrl_3_u *value);

sw_error_t
jhppe_cm_ctrl_3_set(
		a_uint32_t dev_id,
		union cm_ctrl_3_u *value);

sw_error_t
jhppe_cm_ctrl_3_park_fifo_p6_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_3_park_fifo_p6_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_3_park_fifo_p7_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_3_park_fifo_p7_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_cm_ctrl_3_park_fifo_p8_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_3_park_fifo_p8_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_cm_ctrl_4_get(
		a_uint32_t dev_id,
		union cm_ctrl_4_u *value);

sw_error_t
jhppe_cm_ctrl_4_set(
		a_uint32_t dev_id,
		union cm_ctrl_4_u *value);

sw_error_t
jhppe_cm_ctrl_4_prefetch_tc_bwm_xoff_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_cm_ctrl_4_prefetch_tc_bwm_xoff_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_ddrq_ctrl_get(
		a_uint32_t dev_id,
		union ddrq_ctrl_u *value);

sw_error_t
jhppe_ddrq_ctrl_set(
		a_uint32_t dev_id,
		union ddrq_ctrl_u *value);

sw_error_t
jhppe_ddrq_ctrl_ddrq_ensch_empty_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_ddrq_ctrl_ddrq_ensch_empty_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_ddrq_ctrl_ddrq_ensch_head_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_ddrq_ctrl_ddrq_ensch_head_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_ddrq_ctrl_ddrq_ensch_tail_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_ddrq_ctrl_ddrq_ensch_tail_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_sram_qcnt_ctrl_get(
		a_uint32_t dev_id,
		union sram_qcnt_ctrl_u *value);

sw_error_t
jhppe_sram_qcnt_ctrl_set(
		a_uint32_t dev_id,
		union sram_qcnt_ctrl_u *value);

sw_error_t
jhppe_sram_qcnt_ctrl_sramq_clear_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_sram_qcnt_ctrl_sramq_clear_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_sram_qcnt_ctrl_sramq_go_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_sram_qcnt_ctrl_sramq_go_set(
		a_uint32_t dev_id,
		unsigned int value);

#define SRAM_UNIQ_CNT_STATUS_MAX_ENTRY	SRAM_UNIQ_CNT_STATUS_NUM

sw_error_t
jhppe_sram_uniq_cnt_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_status_u *value);

sw_error_t
jhppe_sram_uniq_cnt_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_status_u *value);

sw_error_t
jhppe_sram_uniq_cnt_status_status_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_status_status_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
jhppe_sram_mulq_cnt_status_get(
		a_uint32_t dev_id,
		union sram_mulq_cnt_status_u *value);

sw_error_t
jhppe_sram_mulq_cnt_status_set(
		a_uint32_t dev_id,
		union sram_mulq_cnt_status_u *value);

sw_error_t
jhppe_sram_mulq_cnt_status_status_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_sram_mulq_cnt_status_status_set(
		a_uint32_t dev_id,
		unsigned int value);

#define MCAST_PRIORITY_MAP8_MAX_ENTRY	MCAST_PRIORITY_MAP8_NUM

sw_error_t
jhppe_mcast_priority_map8_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map8_u *value);

sw_error_t
jhppe_mcast_priority_map8_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map8_u *value);

sw_error_t
jhppe_mcast_priority_map8_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_mcast_priority_map8_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UCAST_QUEUE_MAP_RULE_TBL_MAX_ENTRY	UCAST_QUEUE_MAP_RULE_TBL_NUM

sw_error_t
jhppe_ucast_queue_map_rule_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_rule_tbl_u *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_rule_tbl_u *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_info_hi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_info_hi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_type_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_0_type_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_info_hi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_info_hi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_type_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_field_1_type_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_int_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_int_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_int_pri_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_int_pri_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_pass_through_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_pass_through_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_slice_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_slice_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_src_profile_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_src_profile_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_rule_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UCAST_QUEUE_MAP_ACTION_TBL_MAX_ENTRY	UCAST_QUEUE_MAP_ACTION_TBL_NUM

sw_error_t
jhppe_ucast_queue_map_action_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_action_tbl_u *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_action_tbl_u *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_force_index_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_force_index_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_force_index_type_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_force_index_type_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_profile_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_profile_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_queue_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_queue_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_queue_map_base_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_queue_map_base_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_queueid_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_map_action_tbl_queueid_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UCAST_QUEUE_CTRL_TBL_MAX_ENTRY	UCAST_QUEUE_CTRL_TBL_NUM

sw_error_t
jhppe_ucast_queue_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_ctrl_tbl_u *value);

sw_error_t
jhppe_ucast_queue_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_ctrl_tbl_u *value);

sw_error_t
jhppe_ucast_queue_ctrl_tbl_ddrq_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_ctrl_tbl_ddrq_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_ucast_queue_ctrl_tbl_qid_mismatch_check_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ucast_queue_ctrl_tbl_qid_mismatch_check_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define DDRQ_ENSCH_TBL_MAX_ENTRY	DDRQ_ENSCH_TBL_NUM

sw_error_t
jhppe_ddrq_ensch_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_ensch_tbl_u *value);

sw_error_t
jhppe_ddrq_ensch_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_ensch_tbl_u *value);

sw_error_t
jhppe_ddrq_ensch_tbl_ddrq_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ddrq_ensch_tbl_ddrq_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define DDRQ_PKT_CNT_TBL_MAX_ENTRY	DDRQ_PKT_CNT_TBL_NUM

sw_error_t
jhppe_ddrq_pkt_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_pkt_cnt_tbl_u *value);

sw_error_t
jhppe_ddrq_pkt_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_pkt_cnt_tbl_u *value);

sw_error_t
jhppe_ddrq_pkt_cnt_tbl_ddrq_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ddrq_pkt_cnt_tbl_ddrq_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define DDRQ_BYTE_CNT_TBL_MAX_ENTRY	DDRQ_BYTE_CNT_TBL_NUM

sw_error_t
jhppe_ddrq_byte_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_byte_cnt_tbl_u *value);

sw_error_t
jhppe_ddrq_byte_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_byte_cnt_tbl_u *value);

sw_error_t
jhppe_ddrq_byte_cnt_tbl_ddrq_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_ddrq_byte_cnt_tbl_ddrq_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

#define SRAM_UNIQ_CNT_QID_MAP_MAX_ENTRY	SRAM_UNIQ_CNT_QID_MAP_NUM

sw_error_t
jhppe_sram_uniq_cnt_qid_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_qid_map_u *value);

sw_error_t
jhppe_sram_uniq_cnt_qid_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_qid_map_u *value);

sw_error_t
jhppe_sram_uniq_cnt_qid_map_uniq_cnt_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_qid_map_uniq_cnt_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_uniq_cnt_qid_map_uniq_cnt_id_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_qid_map_uniq_cnt_id_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define SRAM_UNIQ_CNT_CFG_TBL_MAX_ENTRY	SRAM_UNIQ_CNT_CFG_TBL_NUM

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_cfg_tbl_u *value);

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_cfg_tbl_u *value);

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_uniq_hi_th_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_uniq_hi_th_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_uniq_hi_th_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_cfg_tbl_uniq_hi_th_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define SRAM_UNIQ_CNT_TBL_MAX_ENTRY	SRAM_UNIQ_CNT_TBL_NUM

sw_error_t
jhppe_sram_uniq_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_tbl_u *value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_uniq_cnt_tbl_u *value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_peak_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_peak_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_peak_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_peak_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_uniq_cnt_tbl_uniq_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define SRAM_MULQ_CNT_CFG_TBL_MAX_ENTRY	SRAM_MULQ_CNT_CFG_TBL_NUM

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_cfg_tbl_u *value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_cfg_tbl_u *value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_hi_th_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_hi_th_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_hi_th_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_hi_th_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_qid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_qid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_qid_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_cfg_tbl_mulq_qid_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define SRAM_MULQ_CNT_TBL_MAX_ENTRY	SRAM_MULQ_CNT_TBL_NUM

sw_error_t
jhppe_sram_mulq_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_tbl_u *value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union sram_mulq_cnt_tbl_u *value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_peak_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_peak_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_peak_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_peak_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_sram_mulq_cnt_tbl_mulq_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define QUEUE_MAP_TBL_MAX_ENTRY	QUEUE_MAP_TBL_NUM

sw_error_t
jhppe_queue_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_map_tbl_u *value);

sw_error_t
jhppe_queue_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_map_tbl_u *value);

sw_error_t
jhppe_queue_map_tbl_tcont_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_queue_map_tbl_tcont_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_queue_map_tbl_tcont_id_vld_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_queue_map_tbl_tcont_id_vld_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TCONT_BYTE_CNT_TBL_MAX_ENTRY	TCONT_BYTE_CNT_TBL_NUM

sw_error_t
jhppe_tcont_byte_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_byte_cnt_tbl_u *value);

sw_error_t
jhppe_tcont_byte_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_byte_cnt_tbl_u *value);

sw_error_t
jhppe_tcont_byte_cnt_tbl_tcont_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_tcont_byte_cnt_tbl_tcont_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

#define DDRQ_DEBIT_TBL_MAX_ENTRY	DDRQ_DEBIT_TBL_NUM

sw_error_t
jhppe_ddrq_debit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_debit_tbl_u *value);

sw_error_t
jhppe_ddrq_debit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_debit_tbl_u *value);

sw_error_t
jhppe_ddrq_debit_tbl_ddrq_debit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ddrq_debit_tbl_ddrq_debit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TCONT_CREDIT_TBL_MAX_ENTRY	TCONT_CREDIT_TBL_NUM

sw_error_t
jhppe_tcont_credit_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_credit_tbl_u *value);

sw_error_t
jhppe_tcont_credit_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tcont_credit_tbl_u *value);

sw_error_t
jhppe_tcont_credit_tbl_tcont_credit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tcont_credit_tbl_tcont_credit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define DDRQ_COMP_TBL_MAX_ENTRY	DDRQ_COMP_TBL_NUM

sw_error_t
jhppe_ddrq_comp_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_comp_tbl_u *value);

sw_error_t
jhppe_ddrq_comp_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ddrq_comp_tbl_u *value);

sw_error_t
jhppe_ddrq_comp_tbl_comp_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_ddrq_comp_tbl_comp_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_LL_MUL_P8_TBL_MAX_ENTRY	OQ_LL_MUL_P8_TBL_NUM

sw_error_t
jhppe_oq_ll_mul_p8_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p8_tbl_u *value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_ll_mul_p8_tbl_u *value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_egress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_egress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_ingress_mirr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_ingress_mirr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_next_pointer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_next_pointer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_normal_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_oq_ll_mul_p8_tbl_normal_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P8_DROP_CNT_TBL_MAX_ENTRY	MUL_P8_DROP_CNT_TBL_NUM

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p8_drop_cnt_tbl_u *value);

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p8_drop_cnt_tbl_u *value);

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_mul_p8_drop_cnt_tbl_mul_p8_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
