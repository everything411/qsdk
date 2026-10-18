/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_PORTCTRL_API_H_
#define _HTTPPE_PORTCTRL_API_H_

#define MRU_MTU_CTRL_TBL_MAX_ENTRY	MRU_MTU_CTRL_TBL_NUM

sw_error_t
httppe_mru_mtu_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_default_pcp_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_default_pcp_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_dscp_qos_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_dscp_qos_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_dscp_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_dscp_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_dscp_res_prec_force_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_dscp_res_prec_force_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_flow_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_flow_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mru_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mru_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mru_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mru_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mtu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mtu_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_qos_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_qos_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_qos_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_qos_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_res_prec_force_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pcp_res_prec_force_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_post_acl_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_post_acl_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pre_acl_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pre_acl_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pre_ipo_inner_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pre_ipo_inner_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pre_ipo_outer_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_pre_ipo_outer_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_preheader_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_preheader_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_rx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_rx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_source_filtering_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_source_filtering_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_source_filtering_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_source_filtering_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_mru_mtu_ctrl_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_TX_COUNTER_TBL_REG_MAX_ENTRY	PORT_TX_COUNTER_TBL_REG_NUM

sw_error_t
httppe_port_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value);

sw_error_t
httppe_port_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value);

sw_error_t
httppe_port_tx_counter_tbl_reg_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
httppe_port_tx_counter_tbl_reg_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
httppe_port_tx_counter_tbl_reg_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_port_tx_counter_tbl_reg_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VP_TX_COUNTER_TBL_REG_MAX_ENTRY	VP_TX_COUNTER_TBL_REG_NUM

sw_error_t
httppe_vp_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value);

sw_error_t
httppe_vp_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value);

sw_error_t
httppe_vp_tx_counter_tbl_reg_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
httppe_vp_tx_counter_tbl_reg_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
httppe_vp_tx_counter_tbl_reg_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_vp_tx_counter_tbl_reg_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
