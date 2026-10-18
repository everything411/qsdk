/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_tpr_pkt_len_ctrl_get(
		a_uint32_t dev_id,
		union tpr_pkt_len_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PKT_LEN_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_set(
		a_uint32_t dev_id,
		union tpr_pkt_len_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PKT_LEN_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_org_len_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.org_len_en;
	return ret;
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_org_len_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.org_len_en = value;
	ret = jhppe_tpr_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_00_check_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.snap_oui_00_check;
	return ret;
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_00_check_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.snap_oui_00_check = value;
	ret = jhppe_tpr_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_f8_check_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tpr_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.snap_oui_f8_check;
	return ret;
}

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_f8_check_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tpr_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.snap_oui_f8_check = value;
	ret = jhppe_tpr_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_pkt_len_ctrl_get(
		a_uint32_t dev_id,
		union pkt_len_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPR_CSR_BASE_ADDR + PKT_LEN_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_pkt_len_ctrl_set(
		a_uint32_t dev_id,
		union pkt_len_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPR_CSR_BASE_ADDR + PKT_LEN_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_pkt_len_ctrl_org_len_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.org_len_en;
	return ret;
}

sw_error_t
jhppe_pkt_len_ctrl_org_len_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.org_len_en = value;
	ret = jhppe_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_00_check_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.snap_oui_00_check;
	return ret;
}

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_00_check_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.snap_oui_00_check = value;
	ret = jhppe_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_f8_check_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.snap_oui_f8_check;
	return ret;
}

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_f8_check_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.snap_oui_f8_check = value;
	ret = jhppe_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
jhppe_tl_pkt_len_ctrl_get(
		a_uint32_t dev_id,
		union tl_pkt_len_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_PKT_LEN_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
jhppe_tl_pkt_len_ctrl_set(
		a_uint32_t dev_id,
		union tl_pkt_len_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_PKT_LEN_CTRL_ADDRESS,
				value->val);
}

sw_error_t
jhppe_tl_pkt_len_ctrl_org_len_en_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union tl_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tl_pkt_len_ctrl_get(dev_id, &reg_val);
	*value = reg_val.bf.org_len_en;
	return ret;
}

sw_error_t
jhppe_tl_pkt_len_ctrl_org_len_en_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union tl_pkt_len_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tl_pkt_len_ctrl_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.org_len_en = value;
	ret = jhppe_tl_pkt_len_ctrl_set(dev_id, &reg_val);
	return ret;
}

