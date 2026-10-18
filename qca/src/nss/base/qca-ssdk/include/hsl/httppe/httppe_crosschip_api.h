/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_CROSSCHIP_API_H_
#define _HTTPPE_CROSSCHIP_API_H_

#define CROSSCHIP_QUEUE_CTRL_MAX_ENTRY	CROSSCHIP_QUEUE_CTRL_NUM

sw_error_t
httppe_mdio_master_ctrl0_get(
		a_uint32_t dev_id,
		union mdio_master_ctrl0_u *value);

sw_error_t
httppe_mdio_master_ctrl0_set(
		a_uint32_t dev_id,
		union mdio_master_ctrl0_u *value);

sw_error_t
httppe_mdio_master_ctrl1_get(
		a_uint32_t dev_id,
		union mdio_master_ctrl1_u *value);

sw_error_t
httppe_mdio_master_ctrl1_set(
		a_uint32_t dev_id,
		union mdio_master_ctrl1_u *value);
sw_error_t
httppe_crosschip_queue_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union crosschip_queue_ctrl_u *value);

sw_error_t
httppe_crosschip_queue_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union crosschip_queue_ctrl_u *value);

sw_error_t
httppe_crosschip_bp_ctrl_get(
		a_uint32_t dev_id,
		union crosschip_bp_ctrl_u *value);

sw_error_t
httppe_crosschip_bp_ctrl_set(
		a_uint32_t dev_id,
		union crosschip_bp_ctrl_u *value);

#define VP_BP_THRES_MAX_ENTRY	VP_BP_THRES_NUM

sw_error_t
httppe_vp_bp_thres_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_thres_u *value);

sw_error_t
httppe_vp_bp_thres_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_thres_u *value);

#define VP_BP_CNT_MAX_ENTRY	VP_BP_CNT_NUM

sw_error_t
httppe_vp_bp_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_cnt_u *value);

sw_error_t
httppe_vp_bp_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_cnt_u *value);
#endif
