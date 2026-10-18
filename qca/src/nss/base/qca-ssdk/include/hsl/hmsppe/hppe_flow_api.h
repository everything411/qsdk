/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_FLOW_API_H_
#define _HPPE_FLOW_API_H_

#define IN_FLOW_CNT_TBL_MAX_ENTRY	IN_FLOW_CNT_TBL_NUM

sw_error_t
hppe_in_flow_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_cnt_tbl_u *value);

sw_error_t
hppe_in_flow_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_cnt_tbl_u *value);

sw_error_t
hppe_in_flow_cnt_tbl_hit_byte_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_in_flow_cnt_tbl_hit_byte_counter_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_in_flow_cnt_tbl_hit_pkt_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_in_flow_cnt_tbl_hit_pkt_counter_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_flow_ctrl0_get(
		a_uint32_t dev_id,
		union flow_ctrl0_u *value);

sw_error_t
hppe_flow_ctrl0_set(
		a_uint32_t dev_id,
		union flow_ctrl0_u *value);

sw_error_t
hppe_flow_ctrl0_clk_freq_inmhz_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ctrl0_clk_freq_inmhz_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flow_ctrl0_flow_age_timer_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ctrl0_flow_age_timer_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flow_ctrl0_flow_age_timer_unit_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ctrl0_flow_age_timer_unit_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flow_ctrl0_flow_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ctrl0_flow_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flow_ctrl0_flow_hash_mode_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ctrl0_flow_hash_mode_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_flow_ctrl0_flow_hash_mode_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_ctrl0_flow_hash_mode_1_set(
		a_uint32_t dev_id,
		unsigned int value);

#define FLOW_CTRL1_MAX_ENTRY	FLOW_CTRL1_NUM

sw_error_t
hppe_flow_ctrl1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union flow_ctrl1_u *value);

sw_error_t
hppe_flow_ctrl1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union flow_ctrl1_u *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_frag_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_frag_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_miss_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_miss_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_tcp_special_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl0_tcp_special_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_frag_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_frag_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_miss_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_miss_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_tcp_special_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl1_tcp_special_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_frag_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_frag_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_miss_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_miss_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_tcp_special_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl2_tcp_special_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_frag_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_frag_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_miss_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_miss_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_tcp_special_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl3_tcp_special_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_frag_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_frag_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_miss_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_miss_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_tcp_special_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_flow_ctrl1_flow_ctl4_tcp_special_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_in_flow_tbl_op_get(
		a_uint32_t dev_id,
		union in_flow_tbl_op_u *value);

sw_error_t
hppe_in_flow_tbl_op_set(
		a_uint32_t dev_id,
		union in_flow_tbl_op_u *value);

sw_error_t
hppe_in_flow_tbl_op_byp_rslt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_byp_rslt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_op_host_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_op_host_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_op_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_op_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_op_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_op_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_in_flow_host_tbl_op_get(
		a_uint32_t dev_id,
		union in_flow_host_tbl_op_u *value);

sw_error_t
hppe_in_flow_host_tbl_op_set(
		a_uint32_t dev_id,
		union in_flow_host_tbl_op_u *value);

sw_error_t
hppe_in_flow_host_tbl_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_host_tbl_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_host_tbl_op_host_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_host_tbl_op_host_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_in_flow_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union in_flow_tbl_op_rslt_u *value);

sw_error_t
hppe_in_flow_tbl_op_rslt_set(
		a_uint32_t dev_id,
		union in_flow_tbl_op_rslt_u *value);

sw_error_t
hppe_in_flow_tbl_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_rslt_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_rslt_flow_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_rslt_flow_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_rslt_op_rslt_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_op_rslt_valid_cnt_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_flow_host_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union flow_host_tbl_op_rslt_u *value);

sw_error_t
hppe_flow_host_tbl_op_rslt_set(
		a_uint32_t dev_id,
		union flow_host_tbl_op_rslt_u *value);

sw_error_t
hppe_flow_host_tbl_op_rslt_host_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_host_tbl_op_rslt_host_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_in_flow_tbl_rd_op_get(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_u *value);

sw_error_t
hppe_in_flow_tbl_rd_op_set(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_u *value);

sw_error_t
hppe_in_flow_tbl_rd_op_byp_rslt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_byp_rslt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_op_host_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_op_host_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_op_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_op_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_op_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_op_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_in_flow_host_tbl_rd_op_get(
		a_uint32_t dev_id,
		union in_flow_host_tbl_rd_op_u *value);

sw_error_t
hppe_in_flow_host_tbl_rd_op_set(
		a_uint32_t dev_id,
		union in_flow_host_tbl_rd_op_u *value);

sw_error_t
hppe_in_flow_host_tbl_rd_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_host_tbl_rd_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_host_tbl_rd_op_host_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_host_tbl_rd_op_host_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_in_flow_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_rslt_u *value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_set(
		a_uint32_t dev_id,
		union in_flow_tbl_rd_op_rslt_u *value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_flow_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_flow_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_op_rslt_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_in_flow_tbl_rd_op_rslt_valid_cnt_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_flow_host_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union flow_host_tbl_rd_op_rslt_u *value);

sw_error_t
hppe_flow_host_tbl_rd_op_rslt_set(
		a_uint32_t dev_id,
		union flow_host_tbl_rd_op_rslt_u *value);

sw_error_t
hppe_flow_host_tbl_rd_op_rslt_host_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_flow_host_tbl_rd_op_rslt_host_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

#define IN_FLOW_TBL_MAX_ENTRY	IN_FLOW_TBL_NUM

sw_error_t
hppe_in_flow_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_tbl_u *value);

sw_error_t
hppe_in_flow_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_tbl_u *value);

#define IN_FLOW_3TUPLE_TBL_MAX_ENTRY	IN_FLOW_3TUPLE_TBL_NUM

sw_error_t
hppe_in_flow_3tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_3tuple_tbl_u *value);

sw_error_t
hppe_in_flow_3tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_3tuple_tbl_u *value);

#define IN_FLOW_IPV6_5TUPLE_TBL_MAX_ENTRY	IN_FLOW_IPV6_5TUPLE_TBL_NUM

sw_error_t
hppe_in_flow_ipv6_5tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_5tuple_tbl_u *value);

sw_error_t
hppe_in_flow_ipv6_5tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_5tuple_tbl_u *value);

#define IN_FLOW_IPV6_3TUPLE_TBL_MAX_ENTRY	IN_FLOW_IPV6_3TUPLE_TBL_NUM

sw_error_t
hppe_in_flow_ipv6_3tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_3tuple_tbl_u *value);

sw_error_t
hppe_in_flow_ipv6_3tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_ipv6_3tuple_tbl_u *value);


sw_error_t
hppe_eg_global_ctrl_get(
		a_uint32_t dev_id,
		union eg_global_ctrl_u *value);

sw_error_t
hppe_eg_global_ctrl_set(
		a_uint32_t dev_id,
		union eg_global_ctrl_u *value);

sw_error_t
hppe_eg_global_ctrl_pm_port_bitmap_for_gem_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_global_ctrl_pm_port_bitmap_for_gem_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_global_ctrl_port_cnt_byp_xlt_drop_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_global_ctrl_port_cnt_byp_xlt_drop_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_global_ctrl_prefix_xlt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_global_ctrl_prefix_xlt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_global_ctrl_private_tag_tpid_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_global_ctrl_private_tag_tpid_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_global_ctrl_vp_cnt_byp_xlt_drop_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_global_ctrl_vp_cnt_byp_xlt_drop_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_eg_global_ctrl_vsi_cnt_byp_xlt_drop_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_eg_global_ctrl_vsi_cnt_byp_xlt_drop_en_set(
		a_uint32_t dev_id,
		unsigned int value);

#define EG_IPV6_PREFIX_TBL_MAX_ENTRY	EG_IPV6_PREFIX_TBL_NUM

sw_error_t
hppe_eg_ipv6_prefix_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_ipv6_prefix_tbl_u *value);

sw_error_t
hppe_eg_ipv6_prefix_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_ipv6_prefix_tbl_u *value);

sw_error_t
hppe_eg_ipv6_prefix_tbl_length_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_ipv6_prefix_tbl_length_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_ipv6_prefix_tbl_prefix_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_eg_ipv6_prefix_tbl_prefix_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

#define EG_FLOW_IPV6_IID_TBL_MAX_ENTRY	EG_FLOW_IPV6_IID_TBL_NUM

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_ipv6_iid_tbl_u *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_ipv6_iid_tbl_u *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_iid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_iid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_iid_update_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_iid_update_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_prefix_update_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_prefix_update_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_src_dst_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_ipv6_iid_tbl_src_dst_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_FLOW_TREE_MAP_TBL_MAX_ENTRY	EG_FLOW_TREE_MAP_TBL_NUM

sw_error_t
hppe_eg_flow_tree_map_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_tree_map_tbl_u *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_flow_tree_map_tbl_u *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_flow_cookie_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_flow_cookie_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_flow_cookie_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_flow_cookie_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_gro_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_gro_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_tree_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_tree_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_tree_id_cookie_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_tree_id_cookie_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_wifi_qos_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_wifi_qos_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_eg_flow_tree_map_tbl_wifi_qos_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_eg_flow_tree_map_tbl_wifi_qos_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
