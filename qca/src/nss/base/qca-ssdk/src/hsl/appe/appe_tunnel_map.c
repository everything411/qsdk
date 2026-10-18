/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
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
appe_tl_map_lpm_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_lpm_counter_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_LPM_COUNTER_ADDRESS + \
				index * TL_MAP_LPM_COUNTER_INC,
				value->val,
				sizeof(union tl_map_lpm_counter_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_lpm_counter_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_lpm_counter_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_LPM_COUNTER_ADDRESS + \
				index * TL_MAP_LPM_COUNTER_INC,
				value->val,
				sizeof(union tl_map_lpm_counter_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_rule_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_rule_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_RULE_TBL_ADDRESS + \
				index * TL_MAP_RULE_TBL_INC,
				value->val,
				sizeof(union tl_map_rule_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_rule_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_rule_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_RULE_TBL_ADDRESS + \
				index * TL_MAP_RULE_TBL_INC,
				value->val,
				sizeof(union tl_map_rule_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_lpm_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_lpm_tbl_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_LPM_TBL_ADDRESS + \
				index * TL_MAP_LPM_TBL_INC,
				value->val,
				sizeof(union tl_map_lpm_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_lpm_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_lpm_tbl_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_LPM_TBL_ADDRESS + \
				index * TL_MAP_LPM_TBL_INC,
				value->val,
				sizeof(union tl_map_lpm_tbl_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_lpm_act_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_lpm_act_u *value)
{
	return hppe_reg_tbl_get(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_LPM_ACT_ADDRESS + \
				index * TL_MAP_LPM_ACT_INC,
				value->val,
				sizeof(union tl_map_lpm_act_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_lpm_act_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_map_lpm_act_u *value)
{
	return hppe_reg_tbl_set(
				dev_id,
				TUNNEL_LOOKUP_BASE_ADDR + TL_MAP_LPM_ACT_ADDRESS + \
				index * TL_MAP_LPM_ACT_INC,
				value->val,
				sizeof(union tl_map_lpm_act_u)/sizeof(a_uint32_t));
}

sw_error_t
appe_tl_map_lpm_tbl_prefix_len_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tl_map_lpm_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tl_map_lpm_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.prefix_len;
	return ret;
}

sw_error_t
appe_tl_map_lpm_tbl_prefix_len_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tl_map_lpm_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tl_map_lpm_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.prefix_len = value;
	ret = appe_tl_map_lpm_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_tl_map_lpm_tbl_ipv6_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value)
{
	union tl_map_lpm_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tl_map_lpm_tbl_get(dev_id, index, &reg_val);
	*value = (a_uint64_t)reg_val.bf.ipv6_addr_1 << 32 | \
		reg_val.bf.ipv6_addr_0;
	return ret;
}

sw_error_t
appe_tl_map_lpm_tbl_ipv6_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value)
{
	union tl_map_lpm_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tl_map_lpm_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.ipv6_addr_1 = value >> 32;
	reg_val.bf.ipv6_addr_0 = value & (((a_uint64_t)1<<32)-1);
	ret = appe_tl_map_lpm_tbl_set(dev_id, index, &reg_val);
	return ret;
}

sw_error_t
appe_tl_map_lpm_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value)
{
	union tl_map_lpm_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tl_map_lpm_tbl_get(dev_id, index, &reg_val);
	*value = reg_val.bf.valid;
	return ret;
}

sw_error_t
appe_tl_map_lpm_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value)
{
	union tl_map_lpm_tbl_u reg_val;
	sw_error_t ret = SW_OK;

	ret = appe_tl_map_lpm_tbl_get(dev_id, index, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.valid = value;
	ret = appe_tl_map_lpm_tbl_set(dev_id, index, &reg_val);
	return ret;
}

