/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
appe_l2_flow_hit_exp_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_flow_hit_exp_ctrl_u *value)
{
	if (index >= L2_FLOW_HIT_EXP_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L2_FLOW_HIT_EXP_CTRL_ADDRESS + \
				index * L2_FLOW_HIT_EXP_CTRL_INC,
				&value->val);
}

sw_error_t
appe_l2_flow_hit_exp_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_flow_hit_exp_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L2_FLOW_HIT_EXP_CTRL_ADDRESS + \
				index * L2_FLOW_HIT_EXP_CTRL_INC,
				value->val);
}

sw_error_t
appe_l3_flow_hit_exp_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_hit_exp_ctrl_u *value)
{
	if (index >= L3_FLOW_HIT_EXP_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_HIT_EXP_CTRL_ADDRESS + \
				index * L3_FLOW_HIT_EXP_CTRL_INC,
				&value->val);
}

sw_error_t
appe_l3_flow_hit_exp_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_hit_exp_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_HIT_EXP_CTRL_ADDRESS + \
				index * L3_FLOW_HIT_EXP_CTRL_INC,
				value->val);
}

sw_error_t
appe_l3_flow_hit_miss_exp_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_hit_miss_exp_ctrl_u *value)
{
	if (index >= L3_FLOW_HIT_MISS_EXP_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_HIT_MISS_EXP_CTRL_ADDRESS + \
				index * L3_FLOW_HIT_MISS_EXP_CTRL_INC,
				&value->val);
}

sw_error_t
appe_l3_flow_hit_miss_exp_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_hit_miss_exp_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L3_FLOW_HIT_MISS_EXP_CTRL_ADDRESS + \
				index * L3_FLOW_HIT_MISS_EXP_CTRL_INC,
				value->val);
}

sw_error_t
appe_l2_flow_hit_miss_exp_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_flow_hit_miss_exp_ctrl_u *value)
{
	if (index >= L2_FLOW_HIT_MISS_EXP_CTRL_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPE_L3_BASE_ADDR + L2_FLOW_HIT_MISS_EXP_CTRL_ADDRESS + \
				index * L2_FLOW_HIT_MISS_EXP_CTRL_INC,
				&value->val);
}

sw_error_t
appe_l2_flow_hit_miss_exp_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_flow_hit_miss_exp_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L3_BASE_ADDR + L2_FLOW_HIT_MISS_EXP_CTRL_ADDRESS + \
				index * L2_FLOW_HIT_MISS_EXP_CTRL_INC,
				value->val);
}

sw_error_t
appe_l2_excep_ctrl_get(
		a_uint32_t dev_id,
		union l2_excep_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPE_L2_BASE_ADDR + L2_EXCEP_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
appe_l2_excep_ctrl_set(
		a_uint32_t dev_id,
		union l2_excep_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPE_L2_BASE_ADDR + L2_EXCEP_CTRL_ADDRESS,
				value->val);
}

sw_error_t
appe_tl_exception_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exception_cmd_u *value)
{
	if (index >= TL_EXCEPTION_CMD_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXCEPTION_CMD_ADDRESS + \
				index * TL_EXCEPTION_CMD_INC,
				&value->val);
}

sw_error_t
appe_tl_exception_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exception_cmd_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXCEPTION_CMD_ADDRESS + \
				index * TL_EXCEPTION_CMD_INC,
				value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile0_u *value)
{
	if (index >= TL_EXP_CTRL_PROFILE0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE0_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE0_INC,
				&value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE0_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE0_INC,
				value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile1_u *value)
{
	if (index >= TL_EXP_CTRL_PROFILE1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE1_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE1_INC,
				&value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE1_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE1_INC,
				value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile2_u *value)
{
	if (index >= TL_EXP_CTRL_PROFILE2_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE2_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE2_INC,
				&value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile2_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE2_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE2_INC,
				value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile3_u *value)
{
	if (index >= TL_EXP_CTRL_PROFILE3_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE3_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE3_INC,
				&value->val);
}

sw_error_t
appe_tl_exp_ctrl_profile3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_exp_ctrl_profile3_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_EXP_CTRL_PROFILE3_ADDRESS + \
				index * TL_EXP_CTRL_PROFILE3_INC,
				value->val);
}

sw_error_t
appe_tpr_exception_ctrl_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_exception_ctrl_0_u *value)
{
	if (index >= TPR_EXCEPTION_CTRL_0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_EXCEPTION_CTRL_0_ADDRESS + \
				index * TPR_EXCEPTION_CTRL_0_INC,
				&value->val);
}

sw_error_t
appe_tpr_exception_ctrl_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_exception_ctrl_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_EXCEPTION_CTRL_0_ADDRESS + \
				index * TPR_EXCEPTION_CTRL_0_INC,
				value->val);
}

sw_error_t
appe_tpr_exception_ctrl_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_exception_ctrl_1_u *value)
{
	if (index >= TPR_EXCEPTION_CTRL_1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_EXCEPTION_CTRL_1_ADDRESS + \
				index * TPR_EXCEPTION_CTRL_1_INC,
				&value->val);
}

sw_error_t
appe_tpr_exception_ctrl_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_exception_ctrl_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_EXCEPTION_CTRL_1_ADDRESS + \
				index * TPR_EXCEPTION_CTRL_1_INC,
				value->val);
}

sw_error_t
appe_tpr_l3_exception_parsing_ctrl_get(
		a_uint32_t dev_id,
		union tpr_l3_exception_parsing_ctrl_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L3_EXCEPTION_PARSING_CTRL_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_l3_exception_parsing_ctrl_set(
		a_uint32_t dev_id,
		union tpr_l3_exception_parsing_ctrl_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L3_EXCEPTION_PARSING_CTRL_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_0_get(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_0_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_0_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_0_set(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_0_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_1_get(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_1_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_1_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_1_set(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_1_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_2_get(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_2_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_2_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_2_set(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_2_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_2_ADDRESS,
				value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_3_get(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_3_u *value)
{
	return hppe_reg_get(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_3_ADDRESS,
				&value->val);
}

sw_error_t
appe_tpr_l4_exception_parsing_ctrl_3_set(
		a_uint32_t dev_id,
		union tpr_l4_exception_parsing_ctrl_3_u *value)
{
	return hppe_reg_set(
				dev_id,
				TUNNEL_PARSER_BASE_ADDR + TPR_L4_EXCEPTION_PARSING_CTRL_3_ADDRESS,
				value->val);
}

