/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_FDB_API_H_
#define _HPPE_FDB_API_H_


sw_error_t
hppe_fdb_tbl_op_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_u *value);

sw_error_t
hppe_fdb_tbl_op_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_u *value);

sw_error_t
hppe_fdb_tbl_op_byp_rslt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_byp_rslt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_op_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_op_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_op_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_op_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_byp_rslt_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_byp_rslt_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_hash_block_bitmap_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_hash_block_bitmap_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_op_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_op_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_op_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_op_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_op_rslt_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_rslt_u *value);

sw_error_t
hppe_fdb_tbl_op_rslt_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_rslt_u *value);

sw_error_t
hppe_fdb_tbl_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_rslt_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_rslt_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_rslt_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_rslt_op_rslt_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_rslt_valid_cnt_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_rslt_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_cmd_id_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_cmd_id_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_entry_index_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_entry_index_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_op_rslt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_op_rslt_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_valid_cnt_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_valid_cnt_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_age_timer_get(
		a_uint32_t dev_id,
		union age_timer_u *value);

sw_error_t
hppe_age_timer_set(
		a_uint32_t dev_id,
		union age_timer_u *value);

sw_error_t
hppe_age_timer_age_val_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_age_timer_age_val_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_l2_global_conf_get(
		a_uint32_t dev_id,
		union l2_global_conf_u *value);

sw_error_t
hppe_l2_global_conf_set(
		a_uint32_t dev_id,
		union l2_global_conf_u *value);

sw_error_t
hppe_l2_global_conf_age_ctrl_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_age_ctrl_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_age_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_age_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_bc_pvlan_isol_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_bc_pvlan_isol_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_dot1p_mapper_pcp_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_dot1p_mapper_pcp_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_dot1p_mapper_vlan_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_dot1p_mapper_vlan_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_failover_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_failover_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_fdb_hash_full_fwd_cmd_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_fdb_hash_full_fwd_cmd_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_fdb_hash_mode_1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_flow_cpy_escape_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_flow_cpy_escape_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_ipmc_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_ipmc_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_0_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_0_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_1_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_ipmc_hash_mode_1_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_ipmc_mismatch_act_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_ipmc_mismatch_act_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_lrn_ctrl_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_lrn_ctrl_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_lrn_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_lrn_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_mc_dmac_check_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_mc_dmac_check_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_mc_pvlan_isol_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_mc_pvlan_isol_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_mc_vlan_match_mode_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_mc_vlan_match_mode_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_l2_global_conf_service_code_loop_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_l2_global_conf_service_code_loop_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_rslt_data0_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data0_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data0_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data0_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data0_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data0_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_rslt_data1_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data1_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data1_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data1_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data1_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data1_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_rslt_data2_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data2_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data2_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_rslt_data2_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data2_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_rslt_data2_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_op_data0_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_data0_u *value);

sw_error_t
hppe_fdb_tbl_op_data0_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_data0_u *value);

sw_error_t
hppe_fdb_tbl_op_data0_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_data0_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_op_data1_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_data1_u *value);

sw_error_t
hppe_fdb_tbl_op_data1_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_data1_u *value);

sw_error_t
hppe_fdb_tbl_op_data1_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_data1_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_op_data2_get(
		a_uint32_t dev_id,
		union fdb_tbl_op_data2_u *value);

sw_error_t
hppe_fdb_tbl_op_data2_set(
		a_uint32_t dev_id,
		union fdb_tbl_op_data2_u *value);

sw_error_t
hppe_fdb_tbl_op_data2_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_op_data2_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_data0_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data0_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_data0_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data0_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_data0_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_data0_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_data1_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data1_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_data1_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data1_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_data1_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_data1_data_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_fdb_tbl_rd_op_data2_get(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data2_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_data2_set(
		a_uint32_t dev_id,
		union fdb_tbl_rd_op_data2_u *value);

sw_error_t
hppe_fdb_tbl_rd_op_data2_data_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_fdb_tbl_rd_op_data2_data_set(
		a_uint32_t dev_id,
		unsigned int value);

#define PORT_BRIDGE_CTRL_MAX_ENTRY	PORT_BRIDGE_CTRL_NUM

sw_error_t
hppe_port_bridge_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_bridge_ctrl_u *value);

sw_error_t
hppe_port_bridge_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_bridge_ctrl_u *value);

sw_error_t
hppe_port_bridge_ctrl_new_addr_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_new_addr_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_bridge_ctrl_new_addr_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_new_addr_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_bridge_ctrl_port_isolation_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_port_isolation_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_bridge_ctrl_promisc_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_promisc_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_bridge_ctrl_station_move_fwd_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_station_move_fwd_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_bridge_ctrl_station_move_lrn_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_station_move_lrn_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_bridge_ctrl_txmac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_bridge_ctrl_txmac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_LRN_LIMIT_CTRL_MAX_ENTRY	PORT_LRN_LIMIT_CTRL_NUM

sw_error_t
hppe_port_lrn_limit_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_ctrl_u *value);

sw_error_t
hppe_port_lrn_limit_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_ctrl_u *value);

sw_error_t
hppe_port_lrn_limit_ctrl_lrn_lmt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_lrn_limit_ctrl_lrn_lmt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_lrn_limit_ctrl_lrn_lmt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_lrn_limit_ctrl_lrn_lmt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_lrn_limit_ctrl_lrn_lmt_exceed_fwd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_lrn_limit_ctrl_lrn_lmt_exceed_fwd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_LRN_LIMIT_COUNTER_MAX_ENTRY	PORT_LRN_LIMIT_COUNTER_NUM

sw_error_t
hppe_port_lrn_limit_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_counter_u *value);

sw_error_t
hppe_port_lrn_limit_counter_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_lrn_limit_counter_u *value);

sw_error_t
hppe_port_lrn_limit_counter_lrn_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_lrn_limit_counter_lrn_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define RFDB_TBL_MAX_ENTRY	RFDB_TBL_NUM

sw_error_t
hppe_rfdb_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfdb_tbl_u *value);

sw_error_t
hppe_rfdb_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rfdb_tbl_u *value);

sw_error_t
hppe_rfdb_tbl_mac_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_rfdb_tbl_mac_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_rfdb_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rfdb_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define FDB_TBL_MAX_ENTRY	FDB_TBL_NUM

sw_error_t
hppe_fdb_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union fdb_tbl_u *value);

sw_error_t
hppe_fdb_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union fdb_tbl_u *value);

#endif
