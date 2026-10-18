/*
 * Copyright (c) 2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include "sw.h"
#include "hppe_rss_reg.h"
#include "hppe_rss.h"
#include "adpt.h"

#define ADPT_RSS_HASH_IP_MIX_MAX_NUM 4

sw_error_t
adpt_hppe_rss_hash_config_set(a_uint32_t dev_id, fal_rss_hash_mode_t mode,
			fal_rss_hash_config_t * config)
{
	a_uint32_t index;
	union rss_hash_mask_reg_u rss_hash_mask_ipv6 = {0};
	union rss_hash_seed_reg_u rss_hash_seed_ipv6 = {0};
	union rss_hash_mix_reg_u rss_hash_mix_ipv6[11] = {0};
	union rss_hash_fin_reg_u rss_hash_fin_ipv6[5] = {0};
	union rss_hash_mask_ipv4_reg_u rss_hash_mask_ipv4 = {0};
	union rss_hash_seed_ipv4_reg_u rss_hash_seed_ipv4 = {0};
	union rss_hash_mix_ipv4_reg_u rss_hash_mix_ipv4[5] = {0};
	union rss_hash_fin_ipv4_reg_u rss_hash_fin_ipv4[5] = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	if (mode == FAL_RSS_HASH_IPV4V6 || mode == FAL_RSS_HASH_IPV4ONLY)
	{
		rss_hash_mask_ipv4.bf.mask = config->hash_mask & 0x1fffff;
		rss_hash_mask_ipv4.bf.fragment = config->hash_fragment_mode;

		rss_hash_seed_ipv4.bf.seed = config->hash_seed;

		rss_hash_mix_ipv4[0].bf.hash_mix = config->hash_sip_mix[0] & 0x1f;
		rss_hash_mix_ipv4[1].bf.hash_mix = config->hash_dip_mix[0] & 0x1f;
		rss_hash_mix_ipv4[2].bf.hash_mix = config->hash_protocol_mix & 0x1f;
		rss_hash_mix_ipv4[3].bf.hash_mix = config->hash_dport_mix & 0x1f;
		rss_hash_mix_ipv4[4].bf.hash_mix = config->hash_sport_mix & 0x1f;

		for (index = 0; index < RSS_HASH_FIN_IPV4_REG_NUM; index++)
		{
			rss_hash_fin_ipv4[index].bf.fin_inner =
					config->hash_fin_inner[index] & 0x1f;
			rss_hash_fin_ipv4[index].bf.fin_outer =
					config->hash_fin_outer[index] & 0x1f;
		}

		SW_RTN_ON_ERROR(hppe_rss_hash_mask_ipv4_reg_set(dev_id, &rss_hash_mask_ipv4));
		SW_RTN_ON_ERROR(hppe_rss_hash_seed_ipv4_reg_set(dev_id, &rss_hash_seed_ipv4));
		for (index = 0; index < RSS_HASH_MIX_IPV4_REG_NUM; index++)
		{
			SW_RTN_ON_ERROR(hppe_rss_hash_mix_ipv4_reg_set(dev_id, index,
					&rss_hash_mix_ipv4[index]));
		}
		for (index = 0; index < RSS_HASH_FIN_IPV4_REG_NUM; index++)
		{
			SW_RTN_ON_ERROR(hppe_rss_hash_fin_ipv4_reg_set(dev_id, index,
					&rss_hash_fin_ipv4[index]));
		}
	}

	if (mode == FAL_RSS_HASH_IPV4V6 || mode == FAL_RSS_HASH_IPV6ONLY)
	{
		rss_hash_mask_ipv6.bf.mask = config->hash_mask & 0x1fffff;
		rss_hash_mask_ipv6.bf.fragment = config->hash_fragment_mode;

		rss_hash_seed_ipv6.bf.seed = config->hash_seed;

		for (index = 0; index < ADPT_RSS_HASH_IP_MIX_MAX_NUM; index++)
		{
			rss_hash_mix_ipv6[index].bf.hash_mix =
					config->hash_sip_mix[index] & 0x1f;
			rss_hash_mix_ipv6[index + ADPT_RSS_HASH_IP_MIX_MAX_NUM].bf.hash_mix =
					config->hash_dip_mix[index] & 0x1f;
		}
		rss_hash_mix_ipv6[8].bf.hash_mix = config->hash_protocol_mix & 0x1f;
		rss_hash_mix_ipv6[9].bf.hash_mix = config->hash_dport_mix & 0x1f;
		rss_hash_mix_ipv6[10].bf.hash_mix = config->hash_sport_mix & 0x1f;

		for (index = 0; index < RSS_HASH_FIN_REG_NUM; index++)
		{
			rss_hash_fin_ipv6[index].bf.fin_inner =
					config->hash_fin_inner[index] & 0x1f;
			rss_hash_fin_ipv6[index].bf.fin_outer =
					config->hash_fin_outer[index] & 0x1f;
		}

		SW_RTN_ON_ERROR(hppe_rss_hash_mask_reg_set(dev_id, &rss_hash_mask_ipv6));
		SW_RTN_ON_ERROR(hppe_rss_hash_seed_reg_set(dev_id, &rss_hash_seed_ipv6));
		for (index = 0; index < RSS_HASH_MIX_REG_NUM; index++)
		{
			SW_RTN_ON_ERROR(hppe_rss_hash_mix_reg_set(dev_id, index,
					&rss_hash_mix_ipv6[index]));
		}
		for (index = 0; index < RSS_HASH_FIN_REG_NUM; index++)
		{
			SW_RTN_ON_ERROR(hppe_rss_hash_fin_reg_set(dev_id, index,
					&rss_hash_fin_ipv6[index]));
		}
	}

	return SW_OK;
}

sw_error_t
adpt_hppe_rss_hash_config_get(a_uint32_t dev_id, fal_rss_hash_mode_t mode,
			fal_rss_hash_config_t * config)
{
	a_uint32_t index;
	union rss_hash_mask_reg_u rss_hash_mask_ipv6 = {0};
	union rss_hash_seed_reg_u rss_hash_seed_ipv6 = {0};
	union rss_hash_mix_reg_u rss_hash_mix_ipv6[11] = {0};
	union rss_hash_fin_reg_u rss_hash_fin_ipv6[5] = {0};
	union rss_hash_mask_ipv4_reg_u rss_hash_mask_ipv4 = {0};
	union rss_hash_seed_ipv4_reg_u rss_hash_seed_ipv4 = {0};
	union rss_hash_mix_ipv4_reg_u rss_hash_mix_ipv4[5] = {0};
	union rss_hash_fin_ipv4_reg_u rss_hash_fin_ipv4[5] = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	SW_RTN_ON_ERROR(hppe_rss_hash_mask_ipv4_reg_get(dev_id, &rss_hash_mask_ipv4));
	SW_RTN_ON_ERROR(hppe_rss_hash_seed_ipv4_reg_get(dev_id, &rss_hash_seed_ipv4));
	for (index = 0; index < RSS_HASH_MIX_IPV4_REG_NUM; index++)
	{
		SW_RTN_ON_ERROR(hppe_rss_hash_mix_ipv4_reg_get(dev_id, index,
				&rss_hash_mix_ipv4[index]));
	}
	for (index = 0; index < RSS_HASH_FIN_IPV4_REG_NUM; index++)
	{
		SW_RTN_ON_ERROR(hppe_rss_hash_fin_ipv4_reg_get(dev_id, index,
				&rss_hash_fin_ipv4[index]));
	}

	SW_RTN_ON_ERROR(hppe_rss_hash_mask_reg_get(dev_id, &rss_hash_mask_ipv6));
	SW_RTN_ON_ERROR(hppe_rss_hash_seed_reg_get(dev_id, &rss_hash_seed_ipv6));
	for (index = 0; index < RSS_HASH_MIX_REG_NUM; index++)
	{
		SW_RTN_ON_ERROR(hppe_rss_hash_mix_reg_get(dev_id, index,
				&rss_hash_mix_ipv6[index]));
	}
	for (index = 0; index < RSS_HASH_FIN_REG_NUM; index++)
	{
		SW_RTN_ON_ERROR(hppe_rss_hash_fin_reg_get(dev_id, index,
				&rss_hash_fin_ipv6[index]));
	}

	if (mode == FAL_RSS_HASH_IPV4ONLY)
	{
		config->hash_mask = rss_hash_mask_ipv4.bf.mask;
		config->hash_fragment_mode = rss_hash_mask_ipv4.bf.fragment;
		config->hash_seed = rss_hash_seed_ipv4.bf.seed;
		config->hash_sip_mix[0] = rss_hash_mix_ipv4[0].bf.hash_mix;
		config->hash_dip_mix[0] = rss_hash_mix_ipv4[1].bf.hash_mix;
		config->hash_protocol_mix = rss_hash_mix_ipv4[2].bf.hash_mix;
		config->hash_dport_mix = rss_hash_mix_ipv4[3].bf.hash_mix;
		config->hash_sport_mix = rss_hash_mix_ipv4[4].bf.hash_mix;
		for (index = 0; index < RSS_HASH_FIN_IPV4_REG_NUM; index++)
		{
			config->hash_fin_inner[index] = rss_hash_fin_ipv4[index].bf.fin_inner;
			config->hash_fin_outer[index] = rss_hash_fin_ipv4[index].bf.fin_outer;
		}
	}
	else if (mode == FAL_RSS_HASH_IPV6ONLY)
	{
		config->hash_mask = rss_hash_mask_ipv6.bf.mask;
		config->hash_fragment_mode = rss_hash_mask_ipv6.bf.fragment;
		config->hash_seed = rss_hash_seed_ipv6.bf.seed;
		for (index = 0; index < ADPT_RSS_HASH_IP_MIX_MAX_NUM; index++)
		{
			config->hash_sip_mix[index] = rss_hash_mix_ipv6[index].bf.hash_mix;
			config->hash_dip_mix[index] =
				rss_hash_mix_ipv6[index + ADPT_RSS_HASH_IP_MIX_MAX_NUM].bf.hash_mix;
		}
		config->hash_protocol_mix = rss_hash_mix_ipv6[8].bf.hash_mix;
		config->hash_dport_mix = rss_hash_mix_ipv6[9].bf.hash_mix;
		config->hash_sport_mix = rss_hash_mix_ipv6[10].bf.hash_mix;
		for (index = 0; index < RSS_HASH_FIN_REG_NUM; index++)
		{
			config->hash_fin_inner[index] = rss_hash_fin_ipv6[index].bf.fin_inner;
			config->hash_fin_outer[index] = rss_hash_fin_ipv6[index].bf.fin_outer;
		}
	}
	else
	{
		if ((rss_hash_mask_ipv4.bf.mask == rss_hash_mask_ipv6.bf.mask) &&
			(rss_hash_mask_ipv4.bf.fragment == rss_hash_mask_ipv6.bf.fragment) &&
			(rss_hash_seed_ipv4.bf.seed == rss_hash_seed_ipv6.bf.seed) &&
			(rss_hash_mix_ipv4[0].bf.hash_mix == rss_hash_mix_ipv6[0].bf.hash_mix) &&
			(rss_hash_mix_ipv4[1].bf.hash_mix == rss_hash_mix_ipv6[4].bf.hash_mix) &&
			(rss_hash_mix_ipv4[2].bf.hash_mix == rss_hash_mix_ipv6[8].bf.hash_mix) &&
			(rss_hash_mix_ipv4[3].bf.hash_mix == rss_hash_mix_ipv6[9].bf.hash_mix) &&
			(rss_hash_mix_ipv4[4].bf.hash_mix == rss_hash_mix_ipv6[10].bf.hash_mix) &&
			(rss_hash_fin_ipv4[0].bf.fin_inner == rss_hash_fin_ipv6[0].bf.fin_inner) &&
			(rss_hash_fin_ipv4[1].bf.fin_inner == rss_hash_fin_ipv6[1].bf.fin_inner) &&
			(rss_hash_fin_ipv4[2].bf.fin_inner == rss_hash_fin_ipv6[2].bf.fin_inner) &&
			(rss_hash_fin_ipv4[3].bf.fin_inner == rss_hash_fin_ipv6[3].bf.fin_inner) &&
			(rss_hash_fin_ipv4[4].bf.fin_inner == rss_hash_fin_ipv6[4].bf.fin_inner) &&
			(rss_hash_fin_ipv4[0].bf.fin_outer == rss_hash_fin_ipv6[0].bf.fin_outer) &&
			(rss_hash_fin_ipv4[1].bf.fin_outer == rss_hash_fin_ipv6[1].bf.fin_outer) &&
			(rss_hash_fin_ipv4[2].bf.fin_outer == rss_hash_fin_ipv6[2].bf.fin_outer) &&
			(rss_hash_fin_ipv4[3].bf.fin_outer == rss_hash_fin_ipv6[3].bf.fin_outer) &&
			(rss_hash_fin_ipv4[4].bf.fin_outer == rss_hash_fin_ipv6[4].bf.fin_outer))
		{
			config->hash_mask = rss_hash_mask_ipv6.bf.mask;
			config->hash_fragment_mode = rss_hash_mask_ipv6.bf.fragment;
			config->hash_seed = rss_hash_seed_ipv6.bf.seed;
			for (index = 0; index < ADPT_RSS_HASH_IP_MIX_MAX_NUM; index++)
			{
				config->hash_sip_mix[index] = rss_hash_mix_ipv6[index].bf.hash_mix;
				config->hash_dip_mix[index] =
				rss_hash_mix_ipv6[index + ADPT_RSS_HASH_IP_MIX_MAX_NUM].bf.hash_mix;
			}
			config->hash_protocol_mix = rss_hash_mix_ipv6[8].bf.hash_mix;
			config->hash_dport_mix = rss_hash_mix_ipv6[9].bf.hash_mix;
			config->hash_sport_mix = rss_hash_mix_ipv6[10].bf.hash_mix;
			for (index = 0; index < RSS_HASH_FIN_REG_NUM; index++)
			{
				config->hash_fin_inner[index] =
						rss_hash_fin_ipv6[index].bf.fin_inner;
				config->hash_fin_outer[index] =
						rss_hash_fin_ipv6[index].bf.fin_outer;
			}
		}
		else
			return SW_FAIL;
	}

	return SW_OK;
}

#if defined(MRPPE)
a_uint32_t
_get_toeplitz_hash_config_by_index(a_uint32_t dev_id, a_uint32_t index,
	fal_toeplitz_hash_config_t *hash_cfg)
{
	union toeplitz_hash_type_sel_0_u t_sel0 = {0};
	union toeplitz_hash_type_sel_1_u t_sel1 = {0};
	union toeplitz_hash_type_tuple_u t_tup = {0};

	ADPT_NULL_POINT_CHECK(hash_cfg);

	if (SW_OK != mrppe_toeplitz_hash_type_sel_0_get(dev_id, index, &t_sel0) ||
		SW_OK != mrppe_toeplitz_hash_type_sel_1_get(dev_id, index, &t_sel1) ||
		SW_OK != mrppe_toeplitz_hash_type_tuple_get(dev_id, index, &t_tup))
		return 0;

	hash_cfg->ip_ver_flg = t_sel0.bf.ip_ver_flag;
	hash_cfg->ip_frag_flg = t_sel0.bf.ip_frag_flag;
	hash_cfg->sec_flg = t_sel0.bf.security_flag;
	hash_cfg->ip_prot_flg = t_sel0.bf.ip_prot_flag;
	hash_cfg->ip_prot = t_sel0.bf.ip_prot;
	hash_cfg->l4_port_flg = t_sel1.bf.l4_port_flag;
	hash_cfg->l4_port = t_sel1.bf.l4_port;
	hash_cfg->l4_type_flg = t_sel1.bf.l4_type_flag;

	hash_cfg->sip_en = t_tup.bf.sip_en;
	hash_cfg->dip_en = t_tup.bf.dip_en;
	hash_cfg->sport_en = t_tup.bf.sport_en;
	hash_cfg->dport_en = t_tup.bf.dport_en;
	hash_cfg->spi_en = t_tup.bf.spi_en;
	hash_cfg->udf_0_en = t_tup.bf.udf_0_en;
	hash_cfg->udf_1_en = t_tup.bf.udf_1_en;
	hash_cfg->ip_prot_en = t_tup.bf.ip_prot_en;
	hash_cfg->hash_flag = t_tup.bf.hash_flag;

	return t_sel1.bf.valid;
}

sw_error_t
_insert_toeplitz_hash_config_by_index(a_uint32_t dev_id, a_uint32_t index,
	fal_toeplitz_hash_config_t *hash_cfg, a_bool_t is_valid)
{
	sw_error_t rtn = SW_OK;
	union toeplitz_hash_type_sel_0_u t_sel0 = {0};
	union toeplitz_hash_type_sel_1_u t_sel1 = {0};
	union toeplitz_hash_type_tuple_u t_tup = {0};

	ADPT_NULL_POINT_CHECK(hash_cfg);

	t_sel0.bf.ip_ver_flag = hash_cfg->ip_ver_flg;
	t_sel0.bf.ip_frag_flag = hash_cfg->ip_frag_flg;
	t_sel0.bf.security_flag = hash_cfg->sec_flg;
	t_sel0.bf.ip_prot_flag = hash_cfg->ip_prot_flg;
	t_sel0.bf.ip_prot = hash_cfg->ip_prot;
	rtn = mrppe_toeplitz_hash_type_sel_0_set(dev_id, index, &t_sel0);
	SW_RTN_ON_ERROR(rtn);

	t_sel1.bf.l4_port_flag = hash_cfg->l4_port_flg;
	t_sel1.bf.l4_port = hash_cfg->l4_port;
	t_sel1.bf.l4_type_flag = hash_cfg->l4_type_flg;
	t_sel1.bf.valid = (is_valid == A_TRUE) ? 1 : 0;
	rtn = mrppe_toeplitz_hash_type_sel_1_set(dev_id, index, &t_sel1);
	SW_RTN_ON_ERROR(rtn);

	t_tup.bf.sip_en = hash_cfg->sip_en;
	t_tup.bf.dip_en = hash_cfg->dip_en;
	t_tup.bf.sport_en = hash_cfg->sport_en;
	t_tup.bf.dport_en = hash_cfg->dport_en;
	t_tup.bf.spi_en = hash_cfg->spi_en;
	t_tup.bf.udf_0_en = hash_cfg->udf_0_en;
	t_tup.bf.udf_1_en = hash_cfg->udf_1_en;
	t_tup.bf.ip_prot_en = hash_cfg->ip_prot_en;

	if (is_valid == A_TRUE)
		t_tup.bf.hash_flag = (hash_cfg->sport_en && hash_cfg->dport_en) ? \
			RSS_5TUPLE_HASH : RSS_3TUPLE_HASH;
	else
		t_tup.bf.hash_flag = RSS_NO_HASH;

	rtn = mrppe_toeplitz_hash_type_tuple_set(dev_id, index, &t_tup);
	SW_RTN_ON_ERROR(rtn);

	return rtn;
}

a_uint32_t
_check_if_toeplitz_hash_config_equal(fal_toeplitz_hash_config_t *hash_cfg1,
	fal_toeplitz_hash_config_t *hash_cfg2)
{
	ADPT_NULL_POINT_CHECK(hash_cfg1);
	ADPT_NULL_POINT_CHECK(hash_cfg2);

	if (hash_cfg1->ip_ver_flg == hash_cfg2->ip_ver_flg &&
		hash_cfg1->ip_frag_flg == hash_cfg2->ip_frag_flg &&
		hash_cfg1->sec_flg == hash_cfg2->sec_flg &&
		hash_cfg1->ip_prot_flg == hash_cfg2->ip_prot_flg &&
		hash_cfg1->ip_prot == hash_cfg2->ip_prot &&
		hash_cfg1->l4_port_flg == hash_cfg2->l4_port_flg &&
		hash_cfg1->l4_port == hash_cfg2->l4_port &&
		hash_cfg1->l4_type_flg == hash_cfg2->l4_type_flg &&
		hash_cfg1->sip_en == hash_cfg2->sip_en &&
		hash_cfg1->dip_en == hash_cfg2->dip_en &&
		hash_cfg1->sport_en == hash_cfg2->sport_en &&
		hash_cfg1->dport_en == hash_cfg2->dport_en &&
		hash_cfg1->spi_en == hash_cfg2->spi_en &&
		hash_cfg1->udf_0_en == hash_cfg2->udf_0_en &&
		hash_cfg1->udf_1_en == hash_cfg2->udf_1_en &&
		hash_cfg1->ip_prot_en == hash_cfg2->ip_prot_en)
		return 1;

	return 0;
}

sw_error_t
adpt_mrppe_toeplitz_hash_secret_key_set(a_uint32_t dev_id,
	fal_toeplitz_secret_key_t *secret_key)
{
	union toeplitz_secret_key_u value = {0};
	a_uint32_t index = 0;
	sw_error_t rtn = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(secret_key);

	for (index = 0; index < TOEPLITZ_HASH_SECRET_KEY_NUM; index++) {
		value.bf.key_word = secret_key->key[index];
		rtn = mrppe_toeplitz_secret_key_set(dev_id, index, &value);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_mrppe_toeplitz_hash_secret_key_get(a_uint32_t dev_id,
	fal_toeplitz_secret_key_t *secret_key)
{
	union toeplitz_secret_key_u value = {0};
	a_uint32_t index = 0;
	sw_error_t rtn = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(secret_key);

	for (index = 0; index < TOEPLITZ_HASH_SECRET_KEY_NUM; index++) {
		rtn = mrppe_toeplitz_secret_key_get(dev_id, index, &value);
		SW_RTN_ON_ERROR(rtn);

		secret_key->key[index] = value.bf.key_word;
	}

	return rtn;
}

sw_error_t
adpt_mrppe_rsshash_algm_set(a_uint32_t dev_id, fal_rss_hash_algm_t *rsshash_algm)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t hash_type = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(rsshash_algm);

	switch (rsshash_algm->hash_algm) {
		case FAL_RSS_LEGACY_HASH:
			hash_type = 0;
			break;
		case FAL_RSS_TOEPLITZ_HASH:
			hash_type = 1;
			break;
		default:
			SSDK_ERROR("invalid hash type: %d\n", rsshash_algm->hash_algm);
			return SW_BAD_VALUE;
	}

	rtn = mrppe_rss_hash_type_sel_toeplitz_enable_set(dev_id, hash_type);
	SW_RTN_ON_ERROR(rtn);

	if (rsshash_algm->hash_algm == FAL_RSS_TOEPLITZ_HASH) {
		rtn = mrppe_rss_hash_type_sel_extract_bit_pos_set(dev_id, rsshash_algm->extract_pos);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_mrppe_rsshash_algm_get(a_uint32_t dev_id, fal_rss_hash_algm_t *rsshash_algm)
{
	sw_error_t rtn = SW_OK;
	a_uint32_t hash_type = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(rsshash_algm);

	rtn = mrppe_rss_hash_type_sel_toeplitz_enable_get(dev_id, &hash_type);
	SW_RTN_ON_ERROR(rtn);

	switch (hash_type) {
		case 0:
			rsshash_algm->hash_algm = FAL_RSS_LEGACY_HASH;
			break;
		case 1:
			rsshash_algm->hash_algm = FAL_RSS_TOEPLITZ_HASH;
			break;
		default:
			SSDK_ERROR("invalid hash type: %d\n", hash_type);
			return SW_BAD_VALUE;
	}

	if (hash_type == FAL_RSS_TOEPLITZ_HASH) {
		rtn = mrppe_rss_hash_type_sel_extract_bit_pos_get(dev_id, &rsshash_algm->extract_pos);
		SW_RTN_ON_ERROR(rtn);
	}

	return rtn;
}

sw_error_t
adpt_mrppe_toeplitz_hash_config_add(a_uint32_t dev_id,
	fal_toeplitz_hash_config_t *toeplitz_cfg)
{
	a_uint32_t index = 0, entry_valid = 0, entry_sign = 0, entry_index = 0;
	fal_toeplitz_hash_config_t tmp_cfg = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(toeplitz_cfg);

	for (index = 0; index < TOEPLITZ_HASH_CONFIG_NUM; index++) {
		memset(&tmp_cfg, 0, sizeof(fal_toeplitz_hash_config_t));

		entry_valid = _get_toeplitz_hash_config_by_index(dev_id, index, &tmp_cfg);
		if (entry_valid) {
			if (_check_if_toeplitz_hash_config_equal(&tmp_cfg, toeplitz_cfg))
				return SW_ALREADY_EXIST;
		} else if (!entry_sign) {
			entry_index = index;
			entry_sign = 1;
		}
	}

	if (!entry_sign)
		return SW_NO_RESOURCE;

	return _insert_toeplitz_hash_config_by_index(dev_id, entry_index, toeplitz_cfg, A_TRUE);
}

sw_error_t
adpt_mrppe_toeplitz_hash_config_del(a_uint32_t dev_id,
	fal_toeplitz_hash_config_t *toeplitz_cfg)
{
	a_uint32_t index = 0, entry_valid = 0;
	fal_toeplitz_hash_config_t tmp_cfg = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(toeplitz_cfg);

	for (index = 0; index < TOEPLITZ_HASH_CONFIG_NUM; index++) {
		memset(&tmp_cfg, 0, sizeof(fal_toeplitz_hash_config_t));

		entry_valid = _get_toeplitz_hash_config_by_index(dev_id, index, &tmp_cfg);
		if (entry_valid && _check_if_toeplitz_hash_config_equal(&tmp_cfg, toeplitz_cfg)) {
			memset(&tmp_cfg, 0, sizeof(fal_toeplitz_hash_config_t));

			return _insert_toeplitz_hash_config_by_index(dev_id, index, &tmp_cfg, A_FALSE);
		}
	}

	return SW_NOT_FOUND;
}

sw_error_t
adpt_mrppe_toeplitz_hash_config_getfirst(a_uint32_t dev_id,
	fal_toeplitz_hash_config_t *toeplitz_cfg)
{
	a_uint32_t index = 0, entry_valid = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(toeplitz_cfg);

	for (index = 0; index < TOEPLITZ_HASH_CONFIG_NUM; index++) {
		entry_valid = _get_toeplitz_hash_config_by_index(dev_id, index, toeplitz_cfg);
		if (entry_valid)
			return SW_OK;
	}

	return SW_NO_MORE;
}

sw_error_t
adpt_mrppe_toeplitz_hash_config_getnext(a_uint32_t dev_id,
	fal_toeplitz_hash_config_t *toeplitz_cfg)
{
	a_uint32_t index = 0, entry_valid = 0, sign_tag = 0;
	fal_toeplitz_hash_config_t tmp_cfg = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(toeplitz_cfg);

	for (index = 0; index < TOEPLITZ_HASH_CONFIG_NUM; index++) {
		memset(&tmp_cfg, 0, sizeof(fal_toeplitz_hash_config_t));

		entry_valid = _get_toeplitz_hash_config_by_index(dev_id, index, &tmp_cfg);
		if (entry_valid) {
			if (sign_tag) {
				aos_mem_copy(toeplitz_cfg, &tmp_cfg, sizeof(fal_toeplitz_hash_config_t));
				return SW_OK;
			}

			if (_check_if_toeplitz_hash_config_equal(&tmp_cfg, toeplitz_cfg))
				sign_tag = 1;
		}
	}

	return SW_NO_MORE;
}
#endif

sw_error_t adpt_hppe_rss_hash_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

	p_adpt_api->adpt_rss_hash_config_set = adpt_hppe_rss_hash_config_set;
	p_adpt_api->adpt_rss_hash_config_get = adpt_hppe_rss_hash_config_get;
#if defined(MRPPE)
	p_adpt_api->adpt_toeplitz_hash_secret_key_set = adpt_mrppe_toeplitz_hash_secret_key_set;
	p_adpt_api->adpt_toeplitz_hash_secret_key_get = adpt_mrppe_toeplitz_hash_secret_key_get;
	p_adpt_api->adpt_rsshash_algm_set = adpt_mrppe_rsshash_algm_set;
	p_adpt_api->adpt_rsshash_algm_get = adpt_mrppe_rsshash_algm_get;
	p_adpt_api->adpt_toeplitz_hash_config_add = adpt_mrppe_toeplitz_hash_config_add;
	p_adpt_api->adpt_toeplitz_hash_config_del = adpt_mrppe_toeplitz_hash_config_del;
	p_adpt_api->adpt_toeplitz_hash_config_getfirst = adpt_mrppe_toeplitz_hash_config_getfirst;
	p_adpt_api->adpt_toeplitz_hash_config_getnext = adpt_mrppe_toeplitz_hash_config_getnext;
#endif
	return SW_OK;
}

/**
 * @}
 */


