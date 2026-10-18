/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "hsl_reg.h"

sw_error_t
jhppe_tpr_hdr_match_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_ctrl_u *value)
{
	if (index >= TPR_HDR_MATCH_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_CTRL_ADDRESS + \
				index * TPR_HDR_MATCH_CTRL_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_CTRL_ADDRESS + \
				index * TPR_HDR_MATCH_CTRL_INC,
				value->val);
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_hdr_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.protocol_pos_en;
	return ret;
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_hdr_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.protocol_pos_en = value;
	ret = jhppe_tpr_hdr_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_hdr_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.protocol_pos_mode;
	return ret;
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_hdr_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.protocol_pos_mode = value;
	ret = jhppe_tpr_hdr_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_hdr_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_match_ctrl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.protocol_pos_offset;
	return ret;
}

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_hdr_match_ctrl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_match_ctrl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.protocol_pos_offset = value;
	ret = jhppe_tpr_hdr_match_ctrl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_hdr_tuple_match_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_tuple_match_u *value)
{
	if (index >= TPR_HDR_TUPLE_MATCH_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_TUPLE_MATCH_ADDRESS + \
				index * TPR_HDR_TUPLE_MATCH_INC,
				&value->val);
}

sw_error_t
jhppe_tpr_hdr_tuple_match_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_tuple_match_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_TUPLE_MATCH_ADDRESS + \
				index * TPR_HDR_TUPLE_MATCH_INC,
				value->val);
}

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_hdr_tuple_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_tuple_match_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tuple_id;
	return ret;
}

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_hdr_tuple_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_tuple_match_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tuple_id = value;
	ret = jhppe_tpr_hdr_tuple_match_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_hdr_tuple_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_tuple_match_get(dev_id, index, &reg_val);
	*value = reg_val.bf.tuple_id_incl;
	return ret;
}

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_hdr_tuple_match_u reg_val;
	sw_error_t ret = SW_OK;

	ret = jhppe_tpr_hdr_tuple_match_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.tuple_id_incl = value;
	ret = jhppe_tpr_hdr_tuple_match_set(dev_id, index, &reg_val);
	return ret;
}

