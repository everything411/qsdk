/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_COUNTER_API_H_
#define _APPE_COUNTER_API_H_

#define PORT_VP_RX_CNT_MODE_MAX_ENTRY	PORT_VP_RX_CNT_MODE_NUM

sw_error_t
appe_port_vp_rx_cnt_mode_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vp_rx_cnt_mode_tbl_u *value);

sw_error_t
appe_port_vp_rx_cnt_mode_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_vp_rx_cnt_mode_tbl_u *value);

sw_error_t
appe_port_vp_rx_cnt_mode_tbl_cnt_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_port_vp_rx_cnt_mode_tbl_cnt_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_RX_CNT_TBL_MAX_ENTRY	PORT_RX_CNT_TBL_NUM

sw_error_t
appe_port_rx_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_rx_cnt_tbl_u *value);

sw_error_t
appe_port_rx_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_rx_cnt_tbl_u *value);

sw_error_t
appe_port_rx_cnt_tbl_rx_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
appe_port_rx_cnt_tbl_rx_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
appe_port_rx_cnt_tbl_rx_drop_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
appe_port_rx_cnt_tbl_rx_drop_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
appe_port_rx_cnt_tbl_rx_drop_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_port_rx_cnt_tbl_rx_drop_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_port_rx_cnt_tbl_rx_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_port_rx_cnt_tbl_rx_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PHY_PORT_RX_CNT_TBL_MAX_ENTRY	PHY_PORT_RX_CNT_TBL_NUM

sw_error_t
appe_phy_port_rx_cnt_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union phy_port_rx_cnt_tbl_u *value);

sw_error_t
appe_phy_port_rx_cnt_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union phy_port_rx_cnt_tbl_u *value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_drop_byte_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_drop_byte_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_drop_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_drop_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_pkt_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_phy_port_rx_cnt_tbl_rx_pkt_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
