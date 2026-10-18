/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#if defined(CONFIG_CPU_BIG_ENDIAN)
#include "hppe_rss_reg_be.h"
#else
/**
 * @defgroup
 * @{
 */
#ifndef HPPE_RSS_REG_H
#define HPPE_RSS_REG_H

/*[register] RSS_HASH_MASK_REG*/
#define RSS_HASH_MASK_REG
#define RSS_HASH_MASK_REG_ADDRESS 0x4318
#define RSS_HASH_MASK_REG_NUM     1
#define RSS_HASH_MASK_REG_INC     0x4
#define RSS_HASH_MASK_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_MASK_REG_DEFAULT 0x0
	/*[field] MASK*/
	#define RSS_HASH_MASK_REG_MASK
	#define RSS_HASH_MASK_REG_MASK_OFFSET  0
	#define RSS_HASH_MASK_REG_MASK_LEN     21
	#define RSS_HASH_MASK_REG_MASK_DEFAULT 0x0
	/*[field] FRAGMENT*/
	#define RSS_HASH_MASK_REG_FRAGMENT
	#define RSS_HASH_MASK_REG_FRAGMENT_OFFSET  28
	#define RSS_HASH_MASK_REG_FRAGMENT_LEN     1
	#define RSS_HASH_MASK_REG_FRAGMENT_DEFAULT 0x0

struct rss_hash_mask_reg {
	a_uint32_t  mask:21;
	a_uint32_t  _reserved0:7;
	a_uint32_t  fragment:1;
	a_uint32_t  _reserved1:3;
};

union rss_hash_mask_reg_u {
	a_uint32_t val;
	struct rss_hash_mask_reg bf;
};

/*[register] RSS_HASH_SEED_REG*/
#define RSS_HASH_SEED_REG
#define RSS_HASH_SEED_REG_ADDRESS 0x431c
#define RSS_HASH_SEED_REG_NUM     1
#define RSS_HASH_SEED_REG_INC     0x4
#define RSS_HASH_SEED_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_SEED_REG_DEFAULT 0x0
	/*[field] SEED*/
	#define RSS_HASH_SEED_REG_SEED
	#define RSS_HASH_SEED_REG_SEED_OFFSET  0
	#define RSS_HASH_SEED_REG_SEED_LEN     32
	#define RSS_HASH_SEED_REG_SEED_DEFAULT 0x0

struct rss_hash_seed_reg {
	a_uint32_t  seed:32;
};

union rss_hash_seed_reg_u {
	a_uint32_t val;
	struct rss_hash_seed_reg bf;
};

/*[register] RSS_HASH_MIX_REG*/
#define RSS_HASH_MIX_REG
#define RSS_HASH_MIX_REG_ADDRESS 0x4320
#define RSS_HASH_MIX_REG_NUM     11
#define RSS_HASH_MIX_REG_INC     0x4
#define RSS_HASH_MIX_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_MIX_REG_DEFAULT 0x0
	/*[field] HASH_MIX*/
	#define RSS_HASH_MIX_REG_HASH_MIX
	#define RSS_HASH_MIX_REG_HASH_MIX_OFFSET  0
	#define RSS_HASH_MIX_REG_HASH_MIX_LEN     5
	#define RSS_HASH_MIX_REG_HASH_MIX_DEFAULT 0x0

struct rss_hash_mix_reg {
	a_uint32_t  hash_mix:5;
	a_uint32_t  _reserved0:27;
};

union rss_hash_mix_reg_u {
	a_uint32_t val;
	struct rss_hash_mix_reg bf;
};

/*[register] RSS_HASH_FIN_REG*/
#define RSS_HASH_FIN_REG
#define RSS_HASH_FIN_REG_ADDRESS 0x4350
#define RSS_HASH_FIN_REG_NUM     5
#define RSS_HASH_FIN_REG_INC     0x4
#define RSS_HASH_FIN_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_FIN_REG_DEFAULT 0x0
	/*[field] FIN_INNER*/
	#define RSS_HASH_FIN_REG_FIN_INNER
	#define RSS_HASH_FIN_REG_FIN_INNER_OFFSET  0
	#define RSS_HASH_FIN_REG_FIN_INNER_LEN     5
	#define RSS_HASH_FIN_REG_FIN_INNER_DEFAULT 0x0
	/*[field] FIN_OUTER*/
	#define RSS_HASH_FIN_REG_FIN_OUTER
	#define RSS_HASH_FIN_REG_FIN_OUTER_OFFSET  5
	#define RSS_HASH_FIN_REG_FIN_OUTER_LEN     5
	#define RSS_HASH_FIN_REG_FIN_OUTER_DEFAULT 0x0

struct rss_hash_fin_reg {
	a_uint32_t  fin_inner:5;
	a_uint32_t  fin_outer:5;
	a_uint32_t  _reserved0:22;
};

union rss_hash_fin_reg_u {
	a_uint32_t val;
	struct rss_hash_fin_reg bf;
};

/*[register] RSS_HASH_MASK_IPV4_REG*/
#define RSS_HASH_MASK_IPV4_REG
#define RSS_HASH_MASK_IPV4_REG_ADDRESS 0x4380
#define RSS_HASH_MASK_IPV4_REG_NUM     1
#define RSS_HASH_MASK_IPV4_REG_INC     0x4
#define RSS_HASH_MASK_IPV4_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_MASK_IPV4_REG_DEFAULT 0x0
	/*[field] MASK*/
	#define RSS_HASH_MASK_IPV4_REG_MASK
	#define RSS_HASH_MASK_IPV4_REG_MASK_OFFSET  0
	#define RSS_HASH_MASK_IPV4_REG_MASK_LEN     21
	#define RSS_HASH_MASK_IPV4_REG_MASK_DEFAULT 0x0
	/*[field] FRAGMENT*/
	#define RSS_HASH_MASK_IPV4_REG_FRAGMENT
	#define RSS_HASH_MASK_IPV4_REG_FRAGMENT_OFFSET  28
	#define RSS_HASH_MASK_IPV4_REG_FRAGMENT_LEN     1
	#define RSS_HASH_MASK_IPV4_REG_FRAGMENT_DEFAULT 0x0

struct rss_hash_mask_ipv4_reg {
	a_uint32_t  mask:21;
	a_uint32_t  _reserved0:7;
	a_uint32_t  fragment:1;
	a_uint32_t  _reserved1:3;
};

union rss_hash_mask_ipv4_reg_u {
	a_uint32_t val;
	struct rss_hash_mask_ipv4_reg bf;
};

/*[register] RSS_HASH_SEED_IPV4_REG*/
#define RSS_HASH_SEED_IPV4_REG
#define RSS_HASH_SEED_IPV4_REG_ADDRESS 0x4384
#define RSS_HASH_SEED_IPV4_REG_NUM     1
#define RSS_HASH_SEED_IPV4_REG_INC     0x4
#define RSS_HASH_SEED_IPV4_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_SEED_IPV4_REG_DEFAULT 0x0
	/*[field] SEED*/
	#define RSS_HASH_SEED_IPV4_REG_SEED
	#define RSS_HASH_SEED_IPV4_REG_SEED_OFFSET  0
	#define RSS_HASH_SEED_IPV4_REG_SEED_LEN     32
	#define RSS_HASH_SEED_IPV4_REG_SEED_DEFAULT 0x0

struct rss_hash_seed_ipv4_reg {
	a_uint32_t  seed:32;
};

union rss_hash_seed_ipv4_reg_u {
	a_uint32_t val;
	struct rss_hash_seed_ipv4_reg bf;
};

/*[register] RSS_HASH_MIX_IPV4_REG*/
#define RSS_HASH_MIX_IPV4_REG
#define RSS_HASH_MIX_IPV4_REG_ADDRESS 0x4390
#define RSS_HASH_MIX_IPV4_REG_NUM     5
#define RSS_HASH_MIX_IPV4_REG_INC     0x4
#define RSS_HASH_MIX_IPV4_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_MIX_IPV4_REG_DEFAULT 0x0
	/*[field] HASH_MIX*/
	#define RSS_HASH_MIX_IPV4_REG_HASH_MIX
	#define RSS_HASH_MIX_IPV4_REG_HASH_MIX_OFFSET  0
	#define RSS_HASH_MIX_IPV4_REG_HASH_MIX_LEN     5
	#define RSS_HASH_MIX_IPV4_REG_HASH_MIX_DEFAULT 0x0

struct rss_hash_mix_ipv4_reg {
	a_uint32_t  hash_mix:5;
	a_uint32_t  _reserved0:27;
};

union rss_hash_mix_ipv4_reg_u {
	a_uint32_t val;
	struct rss_hash_mix_ipv4_reg bf;
};

/*[register] RSS_HASH_FIN_IPV4_REG*/
#define RSS_HASH_FIN_IPV4_REG
#define RSS_HASH_FIN_IPV4_REG_ADDRESS 0x43b0
#define RSS_HASH_FIN_IPV4_REG_NUM     5
#define RSS_HASH_FIN_IPV4_REG_INC     0x4
#define RSS_HASH_FIN_IPV4_REG_TYPE    REG_TYPE_RW
#define RSS_HASH_FIN_IPV4_REG_DEFAULT 0x0
	/*[field] FIN_INNER*/
	#define RSS_HASH_FIN_IPV4_REG_FIN_INNER
	#define RSS_HASH_FIN_IPV4_REG_FIN_INNER_OFFSET  0
	#define RSS_HASH_FIN_IPV4_REG_FIN_INNER_LEN     5
	#define RSS_HASH_FIN_IPV4_REG_FIN_INNER_DEFAULT 0x0
	/*[field] FIN_OUTER*/
	#define RSS_HASH_FIN_IPV4_REG_FIN_OUTER
	#define RSS_HASH_FIN_IPV4_REG_FIN_OUTER_OFFSET  5
	#define RSS_HASH_FIN_IPV4_REG_FIN_OUTER_LEN     5
	#define RSS_HASH_FIN_IPV4_REG_FIN_OUTER_DEFAULT 0x0

struct rss_hash_fin_ipv4_reg {
	a_uint32_t  fin_inner:5;
	a_uint32_t  fin_outer:5;
	a_uint32_t  _reserved0:22;
};

union rss_hash_fin_ipv4_reg_u {
	a_uint32_t val;
	struct rss_hash_fin_ipv4_reg bf;
};

#if defined(MRPPE)
/*[register] TOEPLITZ_SECRET_KEY*/
#define TOEPLITZ_SECRET_KEY
#define TOEPLITZ_SECRET_KEY_ADDRESS 0x4420
#define TOEPLITZ_SECRET_KEY_NUM     11
#define TOEPLITZ_SECRET_KEY_INC     0x4
#define TOEPLITZ_SECRET_KEY_TYPE    REG_TYPE_RW
//#define TOEPLITZ_SECRET_KEY_DEFAULT 0x0
	/*[field] KEY_WORD*/
	#define TOEPLITZ_SECRET_KEY_KEY_WORD
	#define TOEPLITZ_SECRET_KEY_KEY_WORD_OFFSET  0
	#define TOEPLITZ_SECRET_KEY_KEY_WORD_LEN     32
	//#define TOEPLITZ_SECRET_KEY_KEY_WORD_DEFAULT 0x0

struct toeplitz_secret_key {
	a_uint32_t  key_word:32;
};

union toeplitz_secret_key_u {
	a_uint32_t val;
	struct toeplitz_secret_key bf;
};

/*[register] RSS_HASH_TYPE_SEL*/
#define RSS_HASH_TYPE_SEL
#define RSS_HASH_TYPE_SEL_ADDRESS 0x4450
#define RSS_HASH_TYPE_SEL_NUM     1
#define RSS_HASH_TYPE_SEL_INC     0x4
#define RSS_HASH_TYPE_SEL_TYPE    REG_TYPE_RW
#define RSS_HASH_TYPE_SEL_DEFAULT 0x1
	/*[field] TOEPLITZ_ENABLE*/
	#define RSS_HASH_TYPE_SEL_TOEPLITZ_ENABLE
	#define RSS_HASH_TYPE_SEL_TOEPLITZ_ENABLE_OFFSET  0
	#define RSS_HASH_TYPE_SEL_TOEPLITZ_ENABLE_LEN     1
	#define RSS_HASH_TYPE_SEL_TOEPLITZ_ENABLE_DEFAULT 0x1
	/*[field] EXTRACT_BIT_POS*/
	#define RSS_HASH_TYPE_SEL_EXTRACT_BIT_POS
	#define RSS_HASH_TYPE_SEL_EXTRACT_BIT_POS_OFFSET  8
	#define RSS_HASH_TYPE_SEL_EXTRACT_BIT_POS_LEN     5
	#define RSS_HASH_TYPE_SEL_EXTRACT_BIT_POS_DEFAULT 0x0

struct rss_hash_type_sel {
	a_uint32_t  toeplitz_enable:1;
	a_uint32_t  _reserved0:7;
	a_uint32_t  extract_bit_pos:5;
	a_uint32_t  _reserved1:19;
};

union rss_hash_type_sel_u {
	a_uint32_t val;
	struct rss_hash_type_sel bf;
};

/*[register] TOEPLITZ_HASH_TYPE_SEL_0*/
#define TOEPLITZ_HASH_TYPE_SEL_0
#define TOEPLITZ_HASH_TYPE_SEL_0_ADDRESS 0x4500
#define TOEPLITZ_HASH_TYPE_SEL_0_NUM     12
#define TOEPLITZ_HASH_TYPE_SEL_0_INC     0x4
#define TOEPLITZ_HASH_TYPE_SEL_0_TYPE    REG_TYPE_RW
#define TOEPLITZ_HASH_TYPE_SEL_0_DEFAULT 0x0
	/*[field] IP_VER_FLAG*/
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_VER_FLAG
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_VER_FLAG_OFFSET  0
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_VER_FLAG_LEN     2
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_VER_FLAG_DEFAULT 0x0
	/*[field] IP_FRAG_FLAG*/
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_FRAG_FLAG
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_FRAG_FLAG_OFFSET  2
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_FRAG_FLAG_LEN     2
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_FRAG_FLAG_DEFAULT 0x0
	/*[field] SECURITY_FLAG*/
	#define TOEPLITZ_HASH_TYPE_SEL_0_SECURITY_FLAG
	#define TOEPLITZ_HASH_TYPE_SEL_0_SECURITY_FLAG_OFFSET  8
	#define TOEPLITZ_HASH_TYPE_SEL_0_SECURITY_FLAG_LEN     2
	#define TOEPLITZ_HASH_TYPE_SEL_0_SECURITY_FLAG_DEFAULT 0x0
	/*[field] IP_PROT*/
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_OFFSET  16
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_LEN     8
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_DEFAULT 0x0
	/*[field] IP_PROT_FLAG*/
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_FLAG
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_FLAG_OFFSET  24
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_FLAG_LEN     1
	#define TOEPLITZ_HASH_TYPE_SEL_0_IP_PROT_FLAG_DEFAULT 0x0

struct toeplitz_hash_type_sel_0 {
	a_uint32_t  ip_ver_flag:2;
	a_uint32_t  ip_frag_flag:2;
	a_uint32_t  _reserved0:4;
	a_uint32_t  security_flag:2;
	a_uint32_t  _reserved1:6;
	a_uint32_t  ip_prot:8;
	a_uint32_t  ip_prot_flag:1;
	a_uint32_t  _reserved2:7;
};

union toeplitz_hash_type_sel_0_u {
	a_uint32_t val;
	struct toeplitz_hash_type_sel_0 bf;
};

/*[register] TOEPLITZ_HASH_TYPE_SEL_1*/
#define TOEPLITZ_HASH_TYPE_SEL_1
#define TOEPLITZ_HASH_TYPE_SEL_1_ADDRESS 0x4600
#define TOEPLITZ_HASH_TYPE_SEL_1_NUM     12
#define TOEPLITZ_HASH_TYPE_SEL_1_INC     0x4
#define TOEPLITZ_HASH_TYPE_SEL_1_TYPE    REG_TYPE_RW
#define TOEPLITZ_HASH_TYPE_SEL_1_DEFAULT 0x0
	/*[field] L4_PORT*/
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_OFFSET  0
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_LEN     16
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_DEFAULT 0x0
	/*[field] L4_PORT_FLAG*/
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_FLAG
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_FLAG_OFFSET  16
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_FLAG_LEN     2
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_PORT_FLAG_DEFAULT 0x0
	/*[field] L4_TYPE_FLAG*/
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_TYPE_FLAG
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_TYPE_FLAG_OFFSET  18
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_TYPE_FLAG_LEN     2
	#define TOEPLITZ_HASH_TYPE_SEL_1_L4_TYPE_FLAG_DEFAULT 0x0
	/*[field] VALID*/
	#define TOEPLITZ_HASH_TYPE_SEL_1_VALID
	#define TOEPLITZ_HASH_TYPE_SEL_1_VALID_OFFSET  31
	#define TOEPLITZ_HASH_TYPE_SEL_1_VALID_LEN     1
	#define TOEPLITZ_HASH_TYPE_SEL_1_VALID_DEFAULT 0x0

struct toeplitz_hash_type_sel_1 {
	a_uint32_t  l4_port:16;
	a_uint32_t  l4_port_flag:2;
	a_uint32_t  l4_type_flag:2;
	a_uint32_t  _reserved0:11;
	a_uint32_t  valid:1;
};

union toeplitz_hash_type_sel_1_u {
	a_uint32_t val;
	struct toeplitz_hash_type_sel_1 bf;
};

/*[register] TOEPLITZ_HASH_TYPE_TUPLE*/
#define TOEPLITZ_HASH_TYPE_TUPLE
#define TOEPLITZ_HASH_TYPE_TUPLE_ADDRESS 0x4700
#define TOEPLITZ_HASH_TYPE_TUPLE_NUM     12
#define TOEPLITZ_HASH_TYPE_TUPLE_INC     0x4
#define TOEPLITZ_HASH_TYPE_TUPLE_TYPE    REG_TYPE_RW
#define TOEPLITZ_HASH_TYPE_TUPLE_DEFAULT 0x20003
	/*[field] SIP_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_SIP_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_SIP_EN_OFFSET  0
	#define TOEPLITZ_HASH_TYPE_TUPLE_SIP_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_SIP_EN_DEFAULT 0x1
	/*[field] DIP_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_DIP_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_DIP_EN_OFFSET  1
	#define TOEPLITZ_HASH_TYPE_TUPLE_DIP_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_DIP_EN_DEFAULT 0x1
	/*[field] SPORT_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPORT_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPORT_EN_OFFSET  2
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPORT_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPORT_EN_DEFAULT 0x0
	/*[field] DPORT_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_DPORT_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_DPORT_EN_OFFSET  3
	#define TOEPLITZ_HASH_TYPE_TUPLE_DPORT_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_DPORT_EN_DEFAULT 0x0
	/*[field] SPI_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPI_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPI_EN_OFFSET  4
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPI_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_SPI_EN_DEFAULT 0x0
	/*[field] UDF_0_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_0_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_0_EN_OFFSET  5
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_0_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_0_EN_DEFAULT 0x0
	/*[field] UDF_1_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_1_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_1_EN_OFFSET  6
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_1_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_UDF_1_EN_DEFAULT 0x0
	/*[field] IP_PROT_EN*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_IP_PROT_EN
	#define TOEPLITZ_HASH_TYPE_TUPLE_IP_PROT_EN_OFFSET  7
	#define TOEPLITZ_HASH_TYPE_TUPLE_IP_PROT_EN_LEN     1
	#define TOEPLITZ_HASH_TYPE_TUPLE_IP_PROT_EN_DEFAULT 0x0
	/*[field] HASH_FLAG*/
	#define TOEPLITZ_HASH_TYPE_TUPLE_HASH_FLAG
	#define TOEPLITZ_HASH_TYPE_TUPLE_HASH_FLAG_OFFSET  16
	#define TOEPLITZ_HASH_TYPE_TUPLE_HASH_FLAG_LEN     2
	#define TOEPLITZ_HASH_TYPE_TUPLE_HASH_FLAG_DEFAULT 0x2

struct toeplitz_hash_type_tuple {
	a_uint32_t  sip_en:1;
	a_uint32_t  dip_en:1;
	a_uint32_t  sport_en:1;
	a_uint32_t  dport_en:1;
	a_uint32_t  spi_en:1;
	a_uint32_t  udf_0_en:1;
	a_uint32_t  udf_1_en:1;
	a_uint32_t  ip_prot_en:1;
	a_uint32_t  _reserved0:8;
	a_uint32_t  hash_flag:2;
	a_uint32_t  _reserved1:14;
};

union toeplitz_hash_type_tuple_u {
	a_uint32_t val;
	struct toeplitz_hash_type_tuple bf;
};
#endif

#endif
#endif
