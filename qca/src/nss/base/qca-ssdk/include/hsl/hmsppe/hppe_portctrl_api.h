/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_PORTCTRL_API_H_
#define _HPPE_PORTCTRL_API_H_

#define IPR_PKT_NUM_TBL_REG_MAX_ENTRY	IPR_PKT_NUM_TBL_REG_NUM

sw_error_t
hppe_ipr_pkt_num_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_pkt_num_tbl_reg_u *value);

sw_error_t
hppe_ipr_pkt_num_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_pkt_num_tbl_reg_u *value);

sw_error_t
hppe_ipr_pkt_num_tbl_reg_ipr_pkt_num_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ipr_pkt_num_tbl_reg_ipr_pkt_num_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IPR_BYTE_LOW_REG_REG_MAX_ENTRY	IPR_BYTE_LOW_REG_REG_NUM

sw_error_t
hppe_ipr_byte_low_reg_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_low_reg_reg_u *value);

sw_error_t
hppe_ipr_byte_low_reg_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_low_reg_reg_u *value);

sw_error_t
hppe_ipr_byte_low_reg_reg_ipr_byte_low_reg_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ipr_byte_low_reg_reg_ipr_byte_low_reg_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IPR_BYTE_HIGH_REG_MAX_ENTRY	IPR_BYTE_HIGH_REG_NUM

sw_error_t
hppe_ipr_byte_high_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_high_reg_u *value);

sw_error_t
hppe_ipr_byte_high_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_high_reg_u *value);

sw_error_t
hppe_ipr_byte_high_reg_clear_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ipr_byte_high_reg_clear_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ipr_byte_high_reg_ipr_byte_high_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ipr_byte_high_reg_ipr_byte_high_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_IN_FORWARD_MAX_ENTRY	PORT_IN_FORWARD_NUM

sw_error_t
hppe_port_in_forward_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_in_forward_u *value);

sw_error_t
hppe_port_in_forward_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_in_forward_u *value);

sw_error_t
hppe_port_in_forward_source_filtering_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_in_forward_source_filtering_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MC_MTU_CTRL_TBL_MAX_ENTRY	MC_MTU_CTRL_TBL_NUM

sw_error_t
hppe_mc_mtu_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mc_mtu_ctrl_tbl_u *value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mc_mtu_ctrl_tbl_u *value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define LINK_OAM_CTRL_MAX_ENTRY	LINK_OAM_CTRL_NUM

sw_error_t
hppe_link_oam_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value);

sw_error_t
hppe_link_oam_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value);

sw_error_t
hppe_link_oam_ctrl_loopback_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_link_oam_ctrl_loopback_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MRU_MTU_CTRL_TBL_MAX_ENTRY	MRU_MTU_CTRL_TBL_NUM

sw_error_t
hppe_mru_mtu_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_default_pcp_dei_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_default_pcp_dei_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_qos_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_qos_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_force_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_dscp_res_prec_force_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_flow_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_flow_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_group_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_group_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_qos_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_force_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pcp_res_prec_force_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_post_acl_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_post_acl_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_acl_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_acl_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_inner_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_inner_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_outer_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_pre_ipo_outer_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_preheader_res_prec_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_preheader_res_prec_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_rx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_rx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_source_filtering_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_source_filtering_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_source_filtering_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_source_filtering_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_src_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_src_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_TX_COUNTER_TBL_REG_MAX_ENTRY	PORT_TX_COUNTER_TBL_REG_NUM

sw_error_t
hppe_port_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value);

sw_error_t
hppe_port_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value);

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_tx_counter_tbl_reg_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define VP_TX_COUNTER_TBL_REG_MAX_ENTRY	VP_TX_COUNTER_TBL_REG_NUM

sw_error_t
hppe_vp_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value);

sw_error_t
hppe_vp_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value);

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_packets_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_vp_tx_counter_tbl_reg_tx_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_ENABLE_MAX_ENTRY	MAC_ENABLE_NUM

sw_error_t
hppe_mac_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_enable_u *value);

sw_error_t
hppe_mac_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_enable_u *value);

sw_error_t
hppe_mac_enable_duplex_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_enable_duplex_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_enable_rx_flow_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_enable_rx_flow_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_enable_rxmac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_enable_rxmac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_enable_tx_flow_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_enable_tx_flow_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_enable_txmac_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_enable_txmac_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_SPEED_MAX_ENTRY	MAC_SPEED_NUM

sw_error_t
hppe_mac_speed_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_speed_u *value);

sw_error_t
hppe_mac_speed_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_speed_u *value);

sw_error_t
hppe_mac_speed_mac_speed_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_speed_mac_speed_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_CTRL2_MAX_ENTRY	MAC_CTRL2_NUM

sw_error_t
hppe_mac_ctrl2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_ctrl2_u *value);

sw_error_t
hppe_mac_ctrl2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_ctrl2_u *value);

sw_error_t
hppe_mac_ctrl2_crc_rsv_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_crc_rsv_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_crs_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_crs_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_ipg_dec_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_ipg_dec_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_ipg_dec_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_ipg_dec_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_mac_loop_back_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_mac_loop_back_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_mac_lpi_tx_idle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_mac_lpi_tx_idle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_maxfr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_maxfr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_test_pause_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_ctrl2_test_pause_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_DBG_CTRL_MAX_ENTRY	MAC_DBG_CTRL_NUM

sw_error_t
hppe_mac_dbg_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_ctrl_u *value);

sw_error_t
hppe_mac_dbg_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_ctrl_u *value);

sw_error_t
hppe_mac_dbg_ctrl_edxsdfr_transmit_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_ctrl_edxsdfr_transmit_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_dbg_ctrl_high_ipg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_ctrl_high_ipg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_dbg_ctrl_ipgr1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_ctrl_ipgr1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_dbg_ctrl_mac_ipg_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_ctrl_mac_ipg_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_dbg_ctrl_mac_len_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_ctrl_mac_len_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_DBG_ADDR_MAX_ENTRY	MAC_DBG_ADDR_NUM

sw_error_t
hppe_mac_dbg_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_addr_u *value);

sw_error_t
hppe_mac_dbg_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_addr_u *value);

sw_error_t
hppe_mac_dbg_addr_mac_debug_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_addr_mac_debug_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_DBG_DATA_MAX_ENTRY	MAC_DBG_DATA_NUM

sw_error_t
hppe_mac_dbg_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_data_u *value);

sw_error_t
hppe_mac_dbg_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_dbg_data_u *value);

sw_error_t
hppe_mac_dbg_data_mac_debug_data_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_dbg_data_mac_debug_data_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define MAC_JUMBO_SIZE_MAX_ENTRY	MAC_JUMBO_SIZE_NUM

sw_error_t
hppe_mac_jumbo_size_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_jumbo_size_u *value);

sw_error_t
hppe_mac_jumbo_size_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mac_jumbo_size_u *value);

sw_error_t
hppe_mac_jumbo_size_mac_jumbo_size_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_mac_jumbo_size_mac_jumbo_size_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
