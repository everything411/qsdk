/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#include "hsl_htt_reg.h"

sw_error_t
httppe_vsi_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + VSI_TBL_ADDRESS + \
				index * VSI_TBL_INC,
				value->val,
				sizeof(union vsi_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_vsi_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + VSI_TBL_ADDRESS + \
				index * VSI_TBL_INC,
				value->val,
				sizeof(union vsi_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_vsi_remap_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_remap_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + VSI_REMAP_TBL_ADDRESS + \
				index * VSI_REMAP_TBL_INC,
				value->val,
				sizeof(union vsi_remap_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_vsi_remap_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vsi_remap_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + VSI_REMAP_TBL_ADDRESS + \
				index * VSI_REMAP_TBL_INC,
				value->val,
				sizeof(union vsi_remap_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_vsi_remap_tbl_br_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.br_vsi;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_br_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.br_vsi = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.member_port_bitmap_2;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.member_port_bitmap_2 = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.member_port_bitmap_3;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.member_port_bitmap_3 = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_4_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.member_port_bitmap_4;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_4_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.member_port_bitmap_4 = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_5_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.member_port_bitmap_5;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_5_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.member_port_bitmap_5 = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_6_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.member_port_bitmap_6;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_6_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.member_port_bitmap_6 = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_7_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.member_port_bitmap_7;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_member_port_bitmap_7_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.member_port_bitmap_7 = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_vsi_remap_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.vsi_remap_en;
	return ret;
}

sw_error_t
httppe_vsi_remap_tbl_vsi_remap_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union vsi_remap_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = httppe_vsi_remap_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.vsi_remap_en = value;
	ret = httppe_vsi_remap_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
httppe_eg_vsi_counter_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_counter_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VSI_COUNTER_TBL_ADDRESS + \
				index * EG_VSI_COUNTER_TBL_INC,
				value->val,
				sizeof(union eg_vsi_counter_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
httppe_eg_vsi_counter_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_vsi_counter_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_VSI_COUNTER_TBL_ADDRESS + \
				index * EG_VSI_COUNTER_TBL_INC,
				value->val,
				sizeof(union eg_vsi_counter_tbl_u)/sizeof(a_uint32_t));
}

