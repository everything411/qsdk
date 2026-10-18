/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_l2_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_spec_service_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_SPEC_SERVICE_TBL_ADDRESS + \
				index * L2_SPEC_SERVICE_TBL_INC,
				value->val,
				sizeof(union l2_spec_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l2_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_spec_service_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_SPEC_SERVICE_TBL_ADDRESS + \
				index * L2_SPEC_SERVICE_TBL_INC,
				value->val,
				sizeof(union l2_spec_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bypass_bitmap;
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bypass_bitmap = value;
	ret = jhppe_l2_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bypass_bitmap_ext_1 << 6 | \
		reg_val.bf.bypass_bitmap_ext_0;
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bypass_bitmap_ext_1 = value >> 6;
	reg_val.bf.bypass_bitmap_ext_0 = value & (((a_uint64_t)1<<6)-1);
	ret = jhppe_l2_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_new_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bypass_bitmap_new;
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_new_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bypass_bitmap_new = value;
	ret = jhppe_l2_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.post_bypass_bitmap;
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.post_bypass_bitmap = value;
	ret = jhppe_l2_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_rx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.rx_cnt_en;
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_rx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_cnt_en = value;
	ret = jhppe_l2_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_cnt_en;
	return ret;
}

sw_error_t
jhppe_l2_spec_service_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union l2_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_l2_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_cnt_en = value;
	ret = jhppe_l2_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_in_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_spec_service_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + IN_SPEC_SERVICE_TBL_ADDRESS + \
				index * IN_SPEC_SERVICE_TBL_INC,
				value->val,
				sizeof(union in_spec_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_in_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_spec_service_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				INGRESS_VLAN_BASE_ADDR + IN_SPEC_SERVICE_TBL_ADDRESS + \
				index * IN_SPEC_SERVICE_TBL_INC,
				value->val,
				sizeof(union in_spec_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_in_spec_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union in_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bypass_bitmap;
	return ret;
}

sw_error_t
jhppe_in_spec_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union in_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bypass_bitmap = value;
	ret = jhppe_in_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_in_spec_service_tbl_rx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union in_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.rx_counting_en;
	return ret;
}

sw_error_t
jhppe_in_spec_service_tbl_rx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union in_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_in_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.rx_counting_en = value;
	ret = jhppe_in_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_spec_service_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_SPEC_SERVICE_TBL_ADDRESS + \
				index * EG_SPEC_SERVICE_TBL_INC,
				value->val,
				sizeof(union eg_spec_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_eg_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_spec_service_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				NSS_PTX_CSR_BASE_ADDR + EG_SPEC_SERVICE_TBL_ADDRESS + \
				index * EG_SPEC_SERVICE_TBL_INC,
				value->val,
				sizeof(union eg_spec_service_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.field_update_action;
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.field_update_action = value;
	ret = jhppe_eg_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.field_update_action_ext;
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.field_update_action_ext = value;
	ret = jhppe_eg_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.field_update_action_ext1;
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.field_update_action_ext1 = value;
	ret = jhppe_eg_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_ip_length_update_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.ip_length_update;
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_ip_length_update_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ip_length_update = value;
	ret = jhppe_eg_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_tx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tx_counting_en;
	return ret;
}

sw_error_t
jhppe_eg_spec_service_tbl_tx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union eg_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_eg_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tx_counting_en = value;
	ret = jhppe_eg_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tl_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_spec_service_tbl_u *value)
{
	if (index >= TL_SPEC_SERVICE_TBL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_SPEC_SERVICE_TBL_ADDRESS + \
				index * TL_SPEC_SERVICE_TBL_INC,
				&value->val);
}

sw_error_t
jhppe_tl_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_spec_service_tbl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_SPEC_SERVICE_TBL_ADDRESS + \
				index * TL_SPEC_SERVICE_TBL_INC,
				value->val);
}

sw_error_t
jhppe_tl_spec_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tl_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tl_spec_service_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.bypass_bitmap;
	return ret;
}

sw_error_t
jhppe_tl_spec_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tl_spec_service_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tl_spec_service_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.bypass_bitmap = value;
	ret = jhppe_tl_spec_service_tbl_set(dev_id, index, &reg_val);
	return ret;
}

