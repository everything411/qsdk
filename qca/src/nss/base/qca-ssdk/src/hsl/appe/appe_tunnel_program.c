/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/**
 * @defgroup
 * @{
 */
#include "hsl_reg.h"

sw_error_t
appe_tpr_hdr_match_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_0_u *value)
{
	if (index >= TPR_HDR_MATCH_0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_0_ADDRESS + \
				index * TPR_HDR_MATCH_0_INC,
				&value->val);
}

sw_error_t
appe_tpr_hdr_match_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_0_ADDRESS + \
				index * TPR_HDR_MATCH_0_INC,
				value->val);
}

sw_error_t
appe_tpr_hdr_match_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_1_u *value)
{
	if (index >= TPR_HDR_MATCH_1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_1_ADDRESS + \
				index * TPR_HDR_MATCH_1_INC,
				&value->val);
}

sw_error_t
appe_tpr_hdr_match_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_1_ADDRESS + \
				index * TPR_HDR_MATCH_1_INC,
				value->val);
}

sw_error_t
appe_tpr_hdr_match_2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_2_u *value)
{
	if (index >= TPR_HDR_MATCH_2_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_2_ADDRESS + \
				index * TPR_HDR_MATCH_2_INC,
				&value->val);
}

sw_error_t
appe_tpr_hdr_match_2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_2_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_HDR_MATCH_2_ADDRESS + \
				index * TPR_HDR_MATCH_2_INC,
				value->val);
}

sw_error_t
appe_tpr_program_hdr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_hdr_u *value)
{
	if (index >= TPR_PROGRAM_HDR_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_HDR_ADDRESS + \
				index * TPR_PROGRAM_HDR_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_hdr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_hdr_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_HDR_ADDRESS + \
				index * TPR_PROGRAM_HDR_INC,
				value->val);
}

sw_error_t
appe_tpr_program_result_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_result_u *value)
{
	if (index >= TPR_PROGRAM_RESULT_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_RESULT_ADDRESS + \
				index * TPR_PROGRAM_RESULT_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_result_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_result_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_RESULT_ADDRESS + \
				index * TPR_PROGRAM_RESULT_INC,
				value->val);
}

sw_error_t
appe_tpr_program_udf_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_ctrl_u *value)
{
	if (index >= TPR_PROGRAM_UDF_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_CTRL_ADDRESS + \
				index * TPR_PROGRAM_UDF_CTRL_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_udf_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_CTRL_ADDRESS + \
				index * TPR_PROGRAM_UDF_CTRL_INC,
				value->val);
}

sw_error_t
appe_tpr_program_udf_data_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_data_0_u *value)
{
	if (index >= TPR_PROGRAM_UDF_DATA_0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_DATA_0_ADDRESS + \
				index * TPR_PROGRAM_UDF_DATA_0_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_udf_data_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_data_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_DATA_0_ADDRESS + \
				index * TPR_PROGRAM_UDF_DATA_0_INC,
				value->val);
}

sw_error_t
appe_tpr_program_udf_data_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_data_1_u *value)
{
	if (index >= TPR_PROGRAM_UDF_DATA_1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_DATA_1_ADDRESS + \
				index * TPR_PROGRAM_UDF_DATA_1_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_udf_data_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_data_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_DATA_1_ADDRESS + \
				index * TPR_PROGRAM_UDF_DATA_1_INC,
				value->val);
}

sw_error_t
appe_tpr_program_udf_mask_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_mask_0_u *value)
{
	if (index >= TPR_PROGRAM_UDF_MASK_0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_MASK_0_ADDRESS + \
				index * TPR_PROGRAM_UDF_MASK_0_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_udf_mask_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_mask_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_MASK_0_ADDRESS + \
				index * TPR_PROGRAM_UDF_MASK_0_INC,
				value->val);
}

sw_error_t
appe_tpr_program_udf_mask_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_mask_1_u *value)
{
	if (index >= TPR_PROGRAM_UDF_MASK_1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_MASK_1_ADDRESS + \
				index * TPR_PROGRAM_UDF_MASK_1_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_udf_mask_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_mask_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_MASK_1_ADDRESS + \
				index * TPR_PROGRAM_UDF_MASK_1_INC,
				value->val);
}

sw_error_t
appe_tpr_program_udf_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_action_u *value)
{
	if (index >= TPR_PROGRAM_UDF_ACTION_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_ACTION_ADDRESS + \
				index * TPR_PROGRAM_UDF_ACTION_INC,
				&value->val);
}

sw_error_t
appe_tpr_program_udf_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_program_udf_action_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_PROGRAM_UDF_ACTION_ADDRESS + \
				index * TPR_PROGRAM_UDF_ACTION_INC,
				value->val);
}

sw_error_t
appe_tpr_program_hdr_hdr_type_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tpr_program_hdr_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tpr_program_hdr_get(dev_id, index, &reg_val);
	*value = reg_val.bf.hdr_type_map;
	return ret;
}

sw_error_t
appe_tpr_program_hdr_hdr_type_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tpr_program_hdr_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tpr_program_hdr_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.hdr_type_map = value;
	ret = appe_tpr_program_hdr_set(dev_id, index, &reg_val);
	return ret;
}

