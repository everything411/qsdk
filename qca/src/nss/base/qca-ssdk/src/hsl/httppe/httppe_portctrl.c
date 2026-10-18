/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_mru_mtu_ctrl_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + MRU_MTU_CTRL_TBL_ADDRESS + \
				index * MRU_MTU_CTRL_TBL_INC,
				value->val,
				sizeof(union mru_mtu_ctrl_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_mru_mtu_ctrl_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union mru_mtu_ctrl_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + MRU_MTU_CTRL_TBL_ADDRESS + \
				index * MRU_MTU_CTRL_TBL_INC,
				value->val,
				sizeof(union mru_mtu_ctrl_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_port_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_TX_COUNTER_TBL_REG_ADDRESS + \
				index * PORT_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union port_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_port_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + PORT_TX_COUNTER_TBL_REG_ADDRESS + \
				index * PORT_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union port_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_vp_tx_counter_tbl_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + VP_TX_COUNTER_TBL_REG_ADDRESS + \
				index * VP_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union vp_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_vp_tx_counter_tbl_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_tx_counter_tbl_reg_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + VP_TX_COUNTER_TBL_REG_ADDRESS + \
				index * VP_TX_COUNTER_TBL_REG_INC,
				value->val,
				sizeof(union vp_tx_counter_tbl_reg_u)/sizeof(a_uint32_t));
}

