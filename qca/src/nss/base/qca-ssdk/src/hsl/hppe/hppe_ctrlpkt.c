/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
hppe_ethertype_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ethertype_ctrl_u *value)
{
	if (index >= ETHERTYPE_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + ETHERTYPE_CTRL_ADDRESS + \
				index * ETHERTYPE_CTRL_INC,
				&value->val);
}

sw_error_t
hppe_ethertype_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ethertype_ctrl_u *value)
{
	if (index >= ETHERTYPE_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + ETHERTYPE_CTRL_ADDRESS + \
				index * ETHERTYPE_CTRL_INC,
				value->val);
}

sw_error_t
hppe_app_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + APP_CTRL_ADDRESS + \
				index * APP_CTRL_INC,
				value->val,
				sizeof(union app_ctrl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_app_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + APP_CTRL_ADDRESS + \
				index * APP_CTRL_INC,
				value->val,
				sizeof(union app_ctrl_u)/sizeof(a_uint32_t));
}

sw_error_t
hppe_ethertype_ctrl_ethertype_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ethertype_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ethertype_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ethertype;
	return ret;
}

sw_error_t
hppe_ethertype_ctrl_ethertype_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ethertype_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ethertype_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ethertype = value;
	ret = hppe_ethertype_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
hppe_ethertype_ctrl_ethertype_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union ethertype_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ethertype_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ethertype_en;
	return ret;
}

sw_error_t
hppe_ethertype_ctrl_ethertype_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union ethertype_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = hppe_ethertype_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ethertype_en = value;
	ret = hppe_ethertype_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_l2_cpu_code_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_cpu_code_ctrl_u *value)
{
	if (index >= L2_CPU_CODE_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_CPU_CODE_CTRL_ADDRESS + \
				index * L2_CPU_CODE_CTRL_INC,
				&value->val);
}

sw_error_t
appe_l2_cpu_code_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_cpu_code_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_CPU_CODE_CTRL_ADDRESS + \
				index * L2_CPU_CODE_CTRL_INC,
				value->val);
}

sw_error_t
appe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_cpu_code_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_cpu_code_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.exception_fmt_ctrl_en;
	return ret;
}

sw_error_t
appe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_cpu_code_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_cpu_code_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.exception_fmt_ctrl_en = value;
	ret = appe_l2_cpu_code_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

#ifdef JHPPE
sw_error_t
hppe_l2_cpu_code_ctrl_enqueue_disable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_cpu_code_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_cpu_code_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.enqueue_disable;
	return ret;
}

sw_error_t
hppe_l2_cpu_code_ctrl_enqueue_disable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_cpu_code_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_l2_cpu_code_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.enqueue_disable = value;
	ret = appe_l2_cpu_code_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

#endif
