/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _HPPE_PORTCTRL_H_
#define _HPPE_PORTCTRL_H_


#define MAC_ENABLE_MAX_ENTRY	6
#define MAC_SPEED_MAX_ENTRY	6
#define GOL_MAC_ADDR0_MAX_ENTRY	6
#define GOL_MAC_ADDR1_MAX_ENTRY	6
#define MAC_CTRL0_MAX_ENTRY	6
#define MAC_CTRL1_MAX_ENTRY	6
#define MAC_CTRL2_MAX_ENTRY	6
#define MAC_DBG_CTRL_MAX_ENTRY	6
#define MAC_DBG_ADDR_MAX_ENTRY	6
#define MAC_DBG_DATA_MAX_ENTRY	6
#define MAC_JUMBO_SIZE_MAX_ENTRY	6
#define MC_MTU_CTRL_TBL_MAX_ENTRY	8
#define MRU_MTU_CTRL_TBL_MAX_ENTRY	256
#define RX_FIFO_CFG_MAX_ENTRY	8
#define TDM_CFG_MAX_ENTRY	128
#define PORT_IN_FORWARD_MAX_ENTRY	8
#define PORT_TX_COUNTER_TBL_REG_MAX_ENTRY	8
#define VP_TX_COUNTER_TBL_REG_MAX_ENTRY	256
#define IPR_PKT_NUM_TBL_REG_MAX_ENTRY	8
#define IPR_BYTE_LOW_REG_REG_MAX_ENTRY	8
#define IPR_BYTE_HIGH_REG_MAX_ENTRY	8

#define PPE_CAPACITY_PORT_NUM	256
#define LINK_OAM_CTRL_MAX_ENTRY	8

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
hppe_tdm_ctrl_set(
		a_uint32_t dev_id,
		union tdm_ctrl_u *value);

sw_error_t
hppe_rx_fifo_cfg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifo_cfg_u *value);

sw_error_t
hppe_rx_fifo_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rx_fifo_cfg_u *value);

sw_error_t
hppe_tdm_cfg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tdm_cfg_u *value);

sw_error_t
hppe_drop_stat_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value);

sw_error_t
hppe_drop_stat_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_stat_u *value);

sw_error_t
hppe_mac_ctrl2_mac_tx_thd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_crs_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mac_ctrl2_maxfr_set(
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
hppe_mac_dbg_ctrl_high_ipg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

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

sw_error_t
hppe_mru_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mru_mtu_ctrl_tbl_mru_set(
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
hppe_mru_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_mc_mtu_ctrl_tbl_mtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

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
hppe_drop_stat_bytes_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
hppe_drop_stat_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
hppe_drop_stat_pkts_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_drop_stat_pkts_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#ifdef MRPPE
sw_error_t
mrppe_lpi_port_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_enable_u *value);

sw_error_t
mrppe_lpi_port_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_enable_u *value);

sw_error_t
mrppe_lpi_wakeup_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_wakeup_timer_u *value);

sw_error_t
mrppe_lpi_wakeup_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_wakeup_timer_u *value);

sw_error_t
mrppe_lpi_sleep_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_sleep_timer_u *value);

sw_error_t
mrppe_lpi_sleep_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_sleep_timer_u *value);

sw_error_t
mrppe_lpi_1us_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_1us_cnt_u *value);

sw_error_t
mrppe_lpi_1us_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_1us_cnt_u *value);
#else
sw_error_t
hppe_lpi_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_enable_u *value);

sw_error_t
hppe_lpi_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_enable_u *value);

sw_error_t
hppe_lpi_timer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_timer_u *value);

sw_error_t
hppe_lpi_timer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_port_timer_u *value);

sw_error_t
hppe_lpi_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_cnt_u *value);

sw_error_t
hppe_lpi_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union lpi_cnt_u *value);
#endif
sw_error_t
hppe_drop_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cnt_u *value);

sw_error_t
hppe_drop_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union drop_cnt_u *value);

sw_error_t
hppe_drop_cnt_drop_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_drop_cnt_drop_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ipr_pkt_num_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_pkt_num_tbl_reg_u *value);

sw_error_t
hppe_ipr_byte_low_reg_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ipr_byte_low_reg_reg_u *value);

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
hppe_ipr_pkt_num_tbl_reg_packets_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ipr_byte_low_reg_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ipr_byte_high_reg_bytes_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_link_oam_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value);

sw_error_t
appe_link_oam_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union link_oam_ctrl_u *value);

sw_error_t
appe_link_oam_ctrl_loopback_state_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_link_oam_ctrl_loopback_state_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
#endif

