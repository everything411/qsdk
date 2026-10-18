/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_QM_API_H_
#define _HPPE_QM_API_H_

#define QUEUE_TX_COUNTER_TBL_MAX_ENTRY	QUEUE_TX_COUNTER_TBL_NUM

sw_error_t
hppe_queue_tx_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_tx_counter_tbl_u *value);

sw_error_t
hppe_queue_tx_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union queue_tx_counter_tbl_u *value);

sw_error_t
hppe_queue_tx_counter_tbl_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_queue_tx_counter_tbl_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_queue_tx_counter_tbl_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_queue_tx_counter_tbl_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_flush_cfg_get(
		a_uint32_t dev_id,
		union flush_cfg_u *value);

sw_error_t
hppe_flush_cfg_set(
		a_uint32_t dev_id,
		union flush_cfg_u *value);

sw_error_t
hppe_flush_cfg_flush_all_queues_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flush_cfg_flush_all_queues_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flush_cfg_flush_busy_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flush_cfg_flush_busy_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flush_cfg_flush_dst_port_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flush_cfg_flush_dst_port_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flush_cfg_flush_qid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flush_cfg_flush_qid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flush_cfg_flush_status_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flush_cfg_flush_status_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flush_cfg_flush_wt_time_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flush_cfg_flush_wt_time_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_in_mirror_priority_ctrl_get(
		a_uint32_t dev_id,
		union in_mirror_priority_ctrl_u *value);

sw_error_t
hppe_in_mirror_priority_ctrl_set(
		a_uint32_t dev_id,
		union in_mirror_priority_ctrl_u *value);

sw_error_t
hppe_in_mirror_priority_ctrl_priority_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_mirror_priority_ctrl_priority_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_eg_mirror_priority_ctrl_get(
		a_uint32_t dev_id,
		union eg_mirror_priority_ctrl_u *value);

sw_error_t
hppe_eg_mirror_priority_ctrl_set(
		a_uint32_t dev_id,
		union eg_mirror_priority_ctrl_u *value);

sw_error_t
hppe_eg_mirror_priority_ctrl_priority_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_mirror_priority_ctrl_priority_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_ucast_default_hash_get(
		a_uint32_t dev_id,
		union ucast_default_hash_u *value);

sw_error_t
hppe_ucast_default_hash_set(
		a_uint32_t dev_id,
		union ucast_default_hash_u *value);

sw_error_t
hppe_ucast_default_hash_hash_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_ucast_default_hash_hash_set(
		a_uint32_t dev_id,
		unsigned int value);

#define MCAST_PRIORITY_MAP0_MAX_ENTRY	MCAST_PRIORITY_MAP0_NUM

sw_error_t
hppe_mcast_priority_map0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map0_u *value);

sw_error_t
hppe_mcast_priority_map0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map0_u *value);

sw_error_t
hppe_mcast_priority_map0_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map0_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP1_MAX_ENTRY	MCAST_PRIORITY_MAP1_NUM

sw_error_t
hppe_mcast_priority_map1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map1_u *value);

sw_error_t
hppe_mcast_priority_map1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map1_u *value);

sw_error_t
hppe_mcast_priority_map1_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map1_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP2_MAX_ENTRY	MCAST_PRIORITY_MAP2_NUM

sw_error_t
hppe_mcast_priority_map2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map2_u *value);

sw_error_t
hppe_mcast_priority_map2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map2_u *value);

sw_error_t
hppe_mcast_priority_map2_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map2_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP3_MAX_ENTRY	MCAST_PRIORITY_MAP3_NUM

sw_error_t
hppe_mcast_priority_map3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map3_u *value);

sw_error_t
hppe_mcast_priority_map3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map3_u *value);

sw_error_t
hppe_mcast_priority_map3_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map3_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP4_MAX_ENTRY	MCAST_PRIORITY_MAP4_NUM

sw_error_t
hppe_mcast_priority_map4_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map4_u *value);

sw_error_t
hppe_mcast_priority_map4_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map4_u *value);

sw_error_t
hppe_mcast_priority_map4_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map4_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP5_MAX_ENTRY	MCAST_PRIORITY_MAP5_NUM

sw_error_t
hppe_mcast_priority_map5_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map5_u *value);

sw_error_t
hppe_mcast_priority_map5_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map5_u *value);

sw_error_t
hppe_mcast_priority_map5_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map5_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP6_MAX_ENTRY	MCAST_PRIORITY_MAP6_NUM

sw_error_t
hppe_mcast_priority_map6_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map6_u *value);

sw_error_t
hppe_mcast_priority_map6_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map6_u *value);

sw_error_t
hppe_mcast_priority_map6_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map6_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_PRIORITY_MAP7_MAX_ENTRY	MCAST_PRIORITY_MAP7_NUM

sw_error_t
hppe_mcast_priority_map7_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map7_u *value);

sw_error_t
hppe_mcast_priority_map7_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_priority_map7_u *value);

sw_error_t
hppe_mcast_priority_map7_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_priority_map7_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UCAST_QUEUE_MAP_TBL_MAX_ENTRY	UCAST_QUEUE_MAP_TBL_NUM

sw_error_t
hppe_ucast_queue_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_tbl_u *value);

sw_error_t
hppe_ucast_queue_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_queue_map_tbl_u *value);

sw_error_t
hppe_ucast_queue_map_tbl_profile_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ucast_queue_map_tbl_profile_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ucast_queue_map_tbl_queue_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ucast_queue_map_tbl_queue_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UCAST_HASH_MAP_TBL_MAX_ENTRY	UCAST_HASH_MAP_TBL_NUM

sw_error_t
hppe_ucast_hash_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_hash_map_tbl_u *value);

sw_error_t
hppe_ucast_hash_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_hash_map_tbl_u *value);

sw_error_t
hppe_ucast_hash_map_tbl_hash_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ucast_hash_map_tbl_hash_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UCAST_PRIORITY_MAP_TBL_MAX_ENTRY	UCAST_PRIORITY_MAP_TBL_NUM

sw_error_t
hppe_ucast_priority_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_priority_map_tbl_u *value);

sw_error_t
hppe_ucast_priority_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ucast_priority_map_tbl_u *value);

sw_error_t
hppe_ucast_priority_map_tbl_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ucast_priority_map_tbl_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MCAST_QUEUE_MAP_TBL_MAX_ENTRY	MCAST_QUEUE_MAP_TBL_NUM

sw_error_t
hppe_mcast_queue_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_queue_map_tbl_u *value);

sw_error_t
hppe_mcast_queue_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mcast_queue_map_tbl_u *value);

sw_error_t
hppe_mcast_queue_map_tbl_class_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mcast_queue_map_tbl_class_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_UNI_QUEUE_CFG_TBL_MAX_ENTRY	AC_UNI_QUEUE_CFG_TBL_NUM

sw_error_t
hppe_ac_uni_queue_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cfg_tbl_u *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cfg_tbl_u *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_color_aware_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_color_aware_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_force_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_force_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_grn_min_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_grn_min_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_min_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_red_min_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_max_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_max_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_min_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_gap_grn_yel_min_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_grn_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_grn_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_grp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_grp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_pre_alloc_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_pre_alloc_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_red_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_red_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_ceiling_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_ceiling_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_dynamic_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_dynamic_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_weight_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_shared_weight_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_wred_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_wred_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_yel_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cfg_tbl_ac_cfg_yel_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_MUL_QUEUE_CFG_TBL_MAX_ENTRY	AC_MUL_QUEUE_CFG_TBL_NUM

sw_error_t
hppe_ac_mul_queue_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cfg_tbl_u *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cfg_tbl_u *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_color_aware_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_color_aware_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_force_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_force_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_red_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_red_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_yel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_gap_grn_yel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_grn_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_grn_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_grp_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_grp_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_pre_alloc_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_pre_alloc_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_red_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_red_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_shared_ceiling_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_shared_ceiling_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_yel_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cfg_tbl_ac_cfg_yel_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_GRP_CFG_TBL_MAX_ENTRY	AC_GRP_CFG_TBL_NUM

sw_error_t
hppe_ac_grp_cfg_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cfg_tbl_u *value);

sw_error_t
hppe_ac_grp_cfg_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_cfg_tbl_u *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_cfg_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_cfg_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_cfg_color_aware_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_cfg_color_aware_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_cfg_force_ac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_cfg_force_ac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_dp_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_dp_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_gap_grn_red_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_gap_grn_red_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_gap_grn_yel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_gap_grn_yel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_grn_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_grn_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_palloc_limit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_palloc_limit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_red_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_red_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_yel_resume_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_cfg_tbl_ac_grp_yel_resume_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_UNI_QUEUE_CNT_TBL_MAX_ENTRY	AC_UNI_QUEUE_CNT_TBL_NUM

sw_error_t
hppe_ac_uni_queue_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cnt_tbl_u *value);

sw_error_t
hppe_ac_uni_queue_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_cnt_tbl_u *value);

sw_error_t
hppe_ac_uni_queue_cnt_tbl_ac_uni_queue_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_cnt_tbl_ac_uni_queue_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_MUL_QUEUE_CNT_TBL_MAX_ENTRY	AC_MUL_QUEUE_CNT_TBL_NUM

sw_error_t
hppe_ac_mul_queue_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cnt_tbl_u *value);

sw_error_t
hppe_ac_mul_queue_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_cnt_tbl_u *value);

sw_error_t
hppe_ac_mul_queue_cnt_tbl_ac_mul_queue_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_cnt_tbl_ac_mul_queue_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_UNI_QUEUE_DROP_STATE_TBL_MAX_ENTRY	AC_UNI_QUEUE_DROP_STATE_TBL_NUM

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_drop_state_tbl_u *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_uni_queue_drop_state_tbl_u *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_grn_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_grn_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_grn_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_grn_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_red_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_red_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_red_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_red_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_yel_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_yel_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_yel_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_uni_queue_drop_state_tbl_yel_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_MUL_QUEUE_DROP_STATE_TBL_MAX_ENTRY	AC_MUL_QUEUE_DROP_STATE_TBL_NUM

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_drop_state_tbl_u *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_mul_queue_drop_state_tbl_u *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_grn_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_grn_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_grn_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_grn_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_red_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_red_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_red_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_red_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_yel_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_yel_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_yel_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_mul_queue_drop_state_tbl_yel_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define AC_GRP_DROP_STATE_TBL_MAX_ENTRY	AC_GRP_DROP_STATE_TBL_NUM

sw_error_t
hppe_ac_grp_drop_state_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_drop_state_tbl_u *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ac_grp_drop_state_tbl_u *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_grn_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_grn_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_drop_state_tbl_grn_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_grn_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_drop_state_tbl_red_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_red_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_drop_state_tbl_red_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_red_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_drop_state_tbl_yel_drop_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_yel_drop_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ac_grp_drop_state_tbl_yel_resume_thrd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ac_grp_drop_state_tbl_yel_resume_thrd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_ENQ_OPR_TBL_MAX_ENTRY	OQ_ENQ_OPR_TBL_NUM

sw_error_t
hppe_oq_enq_opr_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_enq_opr_tbl_u *value);

sw_error_t
hppe_oq_enq_opr_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_enq_opr_tbl_u *value);

sw_error_t
hppe_oq_enq_opr_tbl_enq_disable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_oq_enq_opr_tbl_enq_disable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define OQ_DEQ_OPR_TBL_MAX_ENTRY	OQ_DEQ_OPR_TBL_NUM

sw_error_t
hppe_oq_deq_opr_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_deq_opr_tbl_u *value);

sw_error_t
hppe_oq_deq_opr_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union oq_deq_opr_tbl_u *value);

sw_error_t
hppe_oq_deq_opr_tbl_deq_drop_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_oq_deq_opr_tbl_deq_drop_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_oq_deq_opr_tbl_deq_high_pri_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_oq_deq_opr_tbl_deq_high_pri_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define UNI_DROP_CNT_TBL_MAX_ENTRY	UNI_DROP_CNT_TBL_NUM

sw_error_t
hppe_uni_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uni_drop_cnt_tbl_u *value);

sw_error_t
hppe_uni_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union uni_drop_cnt_tbl_u *value);

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_uni_drop_cnt_tbl_uni_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P0_DROP_CNT_TBL_MAX_ENTRY	MUL_P0_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p0_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p0_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p0_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p0_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p0_drop_cnt_tbl_mul_p0_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P1_DROP_CNT_TBL_MAX_ENTRY	MUL_P1_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p1_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p1_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p1_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p1_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p1_drop_cnt_tbl_mul_p1_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P2_DROP_CNT_TBL_MAX_ENTRY	MUL_P2_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p2_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p2_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p2_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p2_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p2_drop_cnt_tbl_mul_p2_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P3_DROP_CNT_TBL_MAX_ENTRY	MUL_P3_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p3_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p3_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p3_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p3_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p3_drop_cnt_tbl_mul_p3_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P4_DROP_CNT_TBL_MAX_ENTRY	MUL_P4_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p4_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p4_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p4_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p4_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p4_drop_cnt_tbl_mul_p4_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P5_DROP_CNT_TBL_MAX_ENTRY	MUL_P5_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p5_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p5_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p5_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p5_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p5_drop_cnt_tbl_mul_p5_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P6_DROP_CNT_TBL_MAX_ENTRY	MUL_P6_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p6_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p6_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p6_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p6_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p6_drop_cnt_tbl_mul_p6_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MUL_P7_DROP_CNT_TBL_MAX_ENTRY	MUL_P7_DROP_CNT_TBL_NUM

sw_error_t
hppe_mul_p7_drop_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p7_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p7_drop_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mul_p7_drop_cnt_tbl_u *value);

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_byte_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_byte_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_pkt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mul_p7_drop_cnt_tbl_mul_p7_drop_pkt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
