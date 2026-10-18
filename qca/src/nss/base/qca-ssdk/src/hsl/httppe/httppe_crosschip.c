/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_mdio_master_ctrl0_get(
		a_uint32_t dev_id,
		union mdio_master_ctrl0_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + MDIO_MASTER_CTRL0_ADDRESS,
				&value->val);
}

sw_error_t
httppe_mdio_master_ctrl0_set(
		a_uint32_t dev_id,
		union mdio_master_ctrl0_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + MDIO_MASTER_CTRL0_ADDRESS,
				value->val);
}

sw_error_t
httppe_mdio_master_ctrl1_get(
		a_uint32_t dev_id,
		union mdio_master_ctrl1_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + MDIO_MASTER_CTRL1_ADDRESS,
				&value->val);
}

sw_error_t
httppe_mdio_master_ctrl1_set(
		a_uint32_t dev_id,
		union mdio_master_ctrl1_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + MDIO_MASTER_CTRL1_ADDRESS,
				value->val);
}

sw_error_t
httppe_vp_bp_thres_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_thres_u *value)
{
	if (index >= VP_BP_THRES_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + VP_BP_THRES_ADDRESS + \
				index * VP_BP_THRES_INC,
				&value->val);
}

sw_error_t
httppe_vp_bp_thres_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_thres_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + VP_BP_THRES_ADDRESS + \
				index * VP_BP_THRES_INC,
				value->val);
}

sw_error_t
httppe_vp_bp_cnt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_cnt_u *value)
{
	if (index >= VP_BP_CNT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + VP_BP_CNT_ADDRESS + \
				index * VP_BP_CNT_INC,
				&value->val);
}

sw_error_t
httppe_vp_bp_cnt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_bp_cnt_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_BM_CSR_BASE_ADDR + VP_BP_CNT_ADDRESS + \
				index * VP_BP_CNT_INC,
				value->val);
}

sw_error_t
httppe_crosschip_queue_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union crosschip_queue_ctrl_u *value)
{
	if (index >= CROSSCHIP_QUEUE_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + CROSSCHIP_QUEUE_CTRL_ADDRESS + \
				index * CROSSCHIP_QUEUE_CTRL_INC,
				&value->val);
}

sw_error_t
httppe_crosschip_queue_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union crosschip_queue_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + CROSSCHIP_QUEUE_CTRL_ADDRESS + \
				index * CROSSCHIP_QUEUE_CTRL_INC,
				value->val);
}

sw_error_t
httppe_crosschip_bp_ctrl_get(
		a_uint32_t dev_id,
		union crosschip_bp_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + CROSSCHIP_BP_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
httppe_crosschip_bp_ctrl_set(
		a_uint32_t dev_id,
		union crosschip_bp_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_GLOBAL_BASE_ADDR + CROSSCHIP_BP_CTRL_ADDRESS,
				value->val);
}


