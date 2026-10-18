/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
hppe_rss_hash_mask_reg_get(
		a_uint32_t dev_id,
		union rss_hash_mask_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MASK_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_rss_hash_mask_reg_set(
		a_uint32_t dev_id,
		union rss_hash_mask_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MASK_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_rss_hash_seed_reg_get(
		a_uint32_t dev_id,
		union rss_hash_seed_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_SEED_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_rss_hash_seed_reg_set(
		a_uint32_t dev_id,
		union rss_hash_seed_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_SEED_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_rss_hash_mix_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_reg_u *value)
{
	if (index >= RSS_HASH_MIX_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MIX_REG_ADDRESS + \
				index * RSS_HASH_MIX_REG_INC,
				&value->val);
}

sw_error_t
hppe_rss_hash_mix_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MIX_REG_ADDRESS + \
				index * RSS_HASH_MIX_REG_INC,
				value->val);
}

sw_error_t
hppe_rss_hash_fin_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_reg_u *value)
{
	if (index >= RSS_HASH_FIN_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_FIN_REG_ADDRESS + \
				index * RSS_HASH_FIN_REG_INC,
				&value->val);
}

sw_error_t
hppe_rss_hash_fin_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_FIN_REG_ADDRESS + \
				index * RSS_HASH_FIN_REG_INC,
				value->val);
}

sw_error_t
hppe_rss_hash_mask_ipv4_reg_get(
		a_uint32_t dev_id,
		union rss_hash_mask_ipv4_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MASK_IPV4_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_rss_hash_mask_ipv4_reg_set(
		a_uint32_t dev_id,
		union rss_hash_mask_ipv4_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MASK_IPV4_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_rss_hash_seed_ipv4_reg_get(
		a_uint32_t dev_id,
		union rss_hash_seed_ipv4_reg_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_SEED_IPV4_REG_ADDRESS,
				&value->val);
}

sw_error_t
hppe_rss_hash_seed_ipv4_reg_set(
		a_uint32_t dev_id,
		union rss_hash_seed_ipv4_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_SEED_IPV4_REG_ADDRESS,
				value->val);
}

sw_error_t
hppe_rss_hash_mix_ipv4_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_ipv4_reg_u *value)
{
	if (index >= RSS_HASH_MIX_IPV4_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MIX_IPV4_REG_ADDRESS + \
				index * RSS_HASH_MIX_IPV4_REG_INC,
				&value->val);
}

sw_error_t
hppe_rss_hash_mix_ipv4_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_ipv4_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_MIX_IPV4_REG_ADDRESS + \
				index * RSS_HASH_MIX_IPV4_REG_INC,
				value->val);
}

sw_error_t
hppe_rss_hash_fin_ipv4_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_ipv4_reg_u *value)
{
	if (index >= RSS_HASH_FIN_IPV4_REG_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_FIN_IPV4_REG_ADDRESS + \
				index * RSS_HASH_FIN_IPV4_REG_INC,
				&value->val);
}

sw_error_t
hppe_rss_hash_fin_ipv4_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_ipv4_reg_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_FIN_IPV4_REG_ADDRESS + \
				index * RSS_HASH_FIN_IPV4_REG_INC,
				value->val);
}

#if defined(MRPPE)
sw_error_t
mrppe_toeplitz_secret_key_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_secret_key_u *value)
{
	if (index >= TOEPLITZ_SECRET_KEY_NUM)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_SECRET_KEY_ADDRESS + \
				index * TOEPLITZ_SECRET_KEY_INC,
				&value->val);
}

sw_error_t
mrppe_toeplitz_secret_key_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_secret_key_u *value)
{
	if (index >= TOEPLITZ_SECRET_KEY_NUM)
		return SW_OUT_OF_RANGE;
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_SECRET_KEY_ADDRESS + \
				index * TOEPLITZ_SECRET_KEY_INC,
				value->val);
}

sw_error_t
mrppe_rss_hash_type_sel_get(
		a_uint32_t dev_id,
		union rss_hash_type_sel_u *value)
{
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_TYPE_SEL_ADDRESS,
				&value->val);
}

sw_error_t
mrppe_rss_hash_type_sel_set(
		a_uint32_t dev_id,
		union rss_hash_type_sel_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + RSS_HASH_TYPE_SEL_ADDRESS,
				value->val);
}

sw_error_t
mrppe_toeplitz_hash_type_sel_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_0_u *value)
{
	if (index >= TOEPLITZ_HASH_TYPE_SEL_0_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_HASH_TYPE_SEL_0_ADDRESS + \
				index * TOEPLITZ_HASH_TYPE_SEL_0_INC,
				&value->val);
}

sw_error_t
mrppe_toeplitz_hash_type_sel_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_0_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_HASH_TYPE_SEL_0_ADDRESS + \
				index * TOEPLITZ_HASH_TYPE_SEL_0_INC,
				value->val);
}

sw_error_t
mrppe_toeplitz_hash_type_sel_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_1_u *value)
{
	if (index >= TOEPLITZ_HASH_TYPE_SEL_1_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_HASH_TYPE_SEL_1_ADDRESS + \
				index * TOEPLITZ_HASH_TYPE_SEL_1_INC,
				&value->val);
}

sw_error_t
mrppe_toeplitz_hash_type_sel_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_1_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_HASH_TYPE_SEL_1_ADDRESS + \
				index * TOEPLITZ_HASH_TYPE_SEL_1_INC,
				value->val);
}

sw_error_t
mrppe_toeplitz_hash_type_tuple_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_tuple_u *value)
{
	if (index >= TOEPLITZ_HASH_TYPE_TUPLE_MAX_ENTRY)
		return SW_OUT_OF_RANGE;
	return hppe_reg_get(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_HASH_TYPE_TUPLE_ADDRESS + \
				index * TOEPLITZ_HASH_TYPE_TUPLE_INC,
				&value->val);
}

sw_error_t
mrppe_toeplitz_hash_type_tuple_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_tuple_u *value)
{
	return hppe_reg_set(
				dev_id,
				IPO_CSR_BASE_ADDR + TOEPLITZ_HASH_TYPE_TUPLE_ADDRESS + \
				index * TOEPLITZ_HASH_TYPE_TUPLE_INC,
				value->val);
}

sw_error_t
mrppe_rss_hash_type_sel_toeplitz_enable_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union rss_hash_type_sel_u reg_val;
	sw_error_t ret = SW_OK;

	ret = mrppe_rss_hash_type_sel_get(dev_id, &reg_val);
	*value = reg_val.bf.toeplitz_enable;
	return ret;
}

sw_error_t
mrppe_rss_hash_type_sel_toeplitz_enable_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union rss_hash_type_sel_u reg_val;
	sw_error_t ret = SW_OK;

	ret = mrppe_rss_hash_type_sel_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.toeplitz_enable = value;
	ret = mrppe_rss_hash_type_sel_set(dev_id, &reg_val);
	return ret;
}

sw_error_t
mrppe_rss_hash_type_sel_extract_bit_pos_get(
		a_uint32_t dev_id,
		a_uint32_t *value)
{
	union rss_hash_type_sel_u reg_val;
	sw_error_t ret = SW_OK;

	ret = mrppe_rss_hash_type_sel_get(dev_id, &reg_val);
	*value = reg_val.bf.extract_bit_pos;
	return ret;
}

sw_error_t
mrppe_rss_hash_type_sel_extract_bit_pos_set(
		a_uint32_t dev_id,
		a_uint32_t value)
{
	union rss_hash_type_sel_u reg_val;
	sw_error_t ret = SW_OK;

	ret = mrppe_rss_hash_type_sel_get(dev_id, &reg_val);
	if (SW_OK != ret)
		return ret;
	reg_val.bf.extract_bit_pos = value;
	ret = mrppe_rss_hash_type_sel_set(dev_id, &reg_val);
	return ret;
}

#endif
