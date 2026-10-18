/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_RSS_API_H_
#define _HPPE_RSS_API_H_


sw_error_t
hppe_rss_hash_mask_reg_get(
		a_uint32_t dev_id,
		union rss_hash_mask_reg_u *value);

sw_error_t
hppe_rss_hash_mask_reg_set(
		a_uint32_t dev_id,
		union rss_hash_mask_reg_u *value);

sw_error_t
hppe_rss_hash_mask_reg_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_mask_reg_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_rss_hash_mask_reg_rss_hash_fragment_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_mask_reg_rss_hash_fragment_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_rss_hash_seed_reg_get(
		a_uint32_t dev_id,
		union rss_hash_seed_reg_u *value);

sw_error_t
hppe_rss_hash_seed_reg_set(
		a_uint32_t dev_id,
		union rss_hash_seed_reg_u *value);

sw_error_t
hppe_rss_hash_seed_reg_seed_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_seed_reg_seed_set(
		a_uint32_t dev_id,
		unsigned int value);

#define RSS_HASH_MIX_REG_MAX_ENTRY	RSS_HASH_MIX_REG_NUM

sw_error_t
hppe_rss_hash_mix_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_reg_u *value);

sw_error_t
hppe_rss_hash_mix_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_reg_u *value);

sw_error_t
hppe_rss_hash_mix_reg_hash_mix_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rss_hash_mix_reg_hash_mix_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define RSS_HASH_FIN_REG_MAX_ENTRY	RSS_HASH_FIN_REG_NUM

sw_error_t
hppe_rss_hash_fin_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_reg_u *value);

sw_error_t
hppe_rss_hash_fin_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_reg_u *value);

sw_error_t
hppe_rss_hash_fin_reg_fin_inner_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rss_hash_fin_reg_fin_inner_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_rss_hash_fin_reg_fin_outer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rss_hash_fin_reg_fin_outer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_rss_hash_mask_ipv4_reg_get(
		a_uint32_t dev_id,
		union rss_hash_mask_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_mask_ipv4_reg_set(
		a_uint32_t dev_id,
		union rss_hash_mask_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_mask_ipv4_reg_mask_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_mask_ipv4_reg_mask_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_rss_hash_mask_ipv4_reg_rss_hash_fragment_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_mask_ipv4_reg_rss_hash_fragment_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
hppe_rss_hash_seed_ipv4_reg_get(
		a_uint32_t dev_id,
		union rss_hash_seed_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_seed_ipv4_reg_set(
		a_uint32_t dev_id,
		union rss_hash_seed_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_seed_ipv4_reg_seed_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_seed_ipv4_reg_seed_set(
		a_uint32_t dev_id,
		unsigned int value);

#define RSS_HASH_MIX_IPV4_REG_MAX_ENTRY	RSS_HASH_MIX_IPV4_REG_NUM

sw_error_t
hppe_rss_hash_mix_ipv4_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_mix_ipv4_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_mix_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_mix_ipv4_reg_hash_mix_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rss_hash_mix_ipv4_reg_hash_mix_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define RSS_HASH_FIN_IPV4_REG_MAX_ENTRY	RSS_HASH_FIN_IPV4_REG_NUM

sw_error_t
hppe_rss_hash_fin_ipv4_reg_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_fin_ipv4_reg_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union rss_hash_fin_ipv4_reg_u *value);

sw_error_t
hppe_rss_hash_fin_ipv4_reg_fin_inner_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rss_hash_fin_ipv4_reg_fin_inner_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_rss_hash_fin_ipv4_reg_fin_outer_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_rss_hash_fin_ipv4_reg_fin_outer_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);


sw_error_t
hppe_rss_hash_type_sel_get(
		a_uint32_t dev_id,
		union rss_hash_type_sel_u *value);

sw_error_t
hppe_rss_hash_type_sel_set(
		a_uint32_t dev_id,
		union rss_hash_type_sel_u *value);

sw_error_t
hppe_rss_hash_type_sel_extract_bit_pos_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_type_sel_extract_bit_pos_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_rss_hash_type_sel_toeplitz_enable_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_rss_hash_type_sel_toeplitz_enable_set(
		a_uint32_t dev_id,
		unsigned int value);

#define TOEPLITZ_HASH_TYPE_SEL_0_MAX_ENTRY	TOEPLITZ_HASH_TYPE_SEL_0_NUM

sw_error_t
hppe_toeplitz_hash_type_sel_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_0_u *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_0_u *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_frag_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_frag_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_prot_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_prot_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_prot_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_prot_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_ver_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_ip_ver_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_security_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_0_security_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TOEPLITZ_HASH_TYPE_SEL_1_MAX_ENTRY	TOEPLITZ_HASH_TYPE_SEL_1_NUM

sw_error_t
hppe_toeplitz_hash_type_sel_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_1_u *value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_sel_1_u *value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_l4_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_l4_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_l4_port_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_l4_port_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_l4_type_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_l4_type_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_sel_1_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TOEPLITZ_HASH_TYPE_TUPLE_MAX_ENTRY	TOEPLITZ_HASH_TYPE_TUPLE_NUM

sw_error_t
hppe_toeplitz_hash_type_tuple_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_tuple_u *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union toeplitz_hash_type_tuple_u *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_dip_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_dip_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_dport_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_dport_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_hash_flag_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_hash_flag_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_ip_prot_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_ip_prot_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_sip_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_sip_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_spi_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_spi_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_sport_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_sport_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_udf_0_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_udf_0_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_toeplitz_hash_type_tuple_udf_1_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_toeplitz_hash_type_tuple_udf_1_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
