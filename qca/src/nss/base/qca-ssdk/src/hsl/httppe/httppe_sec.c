/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_get(
		a_uint32_t dev_id,
		union l3_exception_parsing_ctrl_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + L3_EXCEPTION_PARSING_CTRL_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_l3_exception_parsing_ctrl_reg_set(
		a_uint32_t dev_id,
		union l3_exception_parsing_ctrl_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + L3_EXCEPTION_PARSING_CTRL_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_0_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_0_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_0_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_0_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_0_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_1_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_1_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_1_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_1_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_1_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_2_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_2_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_2_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_2_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_2_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_get(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_3_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_3_REG_ADDRESS,
				&value->val);
}

sw_error_t
httppe_l4_exception_parsing_ctrl_3_reg_set(
		a_uint32_t dev_id,
		union l4_exception_parsing_ctrl_3_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + L4_EXCEPTION_PARSING_CTRL_3_REG_ADDRESS,
				value->val);
}

sw_error_t
httppe_l3_exception_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exception_cmd_u *value)
{
	if (index >= L3_EXCEPTION_CMD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_EXCEPTION_CMD_ADDRESS + \
				index * L3_EXCEPTION_CMD_INC,
				&value->val);
}

sw_error_t
httppe_l3_exception_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exception_cmd_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_EXCEPTION_CMD_ADDRESS + \
				index * L3_EXCEPTION_CMD_INC,
				value->val);
}

sw_error_t
httppe_l3_exp_l2_only_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exp_l2_only_ctrl_u *value)
{
	if (index >= L3_EXP_L2_ONLY_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_EXP_L2_ONLY_CTRL_ADDRESS + \
				index * L3_EXP_L2_ONLY_CTRL_INC,
				&value->val);
}

sw_error_t
httppe_l3_exp_l2_only_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_exp_l2_only_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_EXP_L2_ONLY_CTRL_ADDRESS + \
				index * L3_EXP_L2_ONLY_CTRL_INC,
				value->val);
}

