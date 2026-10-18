/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_eg_cpu_code_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_cpu_code_tbl_u *value)
{
	if (index >= EG_CPU_CODE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_CPU_CODE_TBL_ADDRESS + \
				index * EG_CPU_CODE_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_eg_cpu_code_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_cpu_code_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_CPU_CODE_TBL_ADDRESS + \
				index * EG_CPU_CODE_TBL_INC,
				value->val);
}

sw_error_t
jhppe_eg_cpu_code_tbl_cpu_code_disable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_cpu_code_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_cpu_code_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.cpu_code_disable;
	return ret;
}

sw_error_t
jhppe_eg_cpu_code_tbl_cpu_code_disable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_cpu_code_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_cpu_code_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.cpu_code_disable = value;
	ret = jhppe_eg_cpu_code_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_cpu_code_tbl_edit_enable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_cpu_code_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_cpu_code_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.edit_enable;
	return ret;
}

sw_error_t
jhppe_eg_cpu_code_tbl_edit_enable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_cpu_code_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_cpu_code_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.edit_enable = value;
	ret = jhppe_eg_cpu_code_tbl_set(dev_id, index, &reg_val);
	return ret;
}

