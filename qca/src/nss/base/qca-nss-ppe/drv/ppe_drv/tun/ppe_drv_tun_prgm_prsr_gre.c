/*
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <ppe_drv/ppe_drv.h>
#include "ppe_drv_tun_prgm_prsr.h"
#include "ppe_drv_tun.h"

/*
 * ppe_drv_tun_prgm_prsr_gre_deconfigure
 *     deconfigure Programmable tunnel parser for L2/L3 GRE tunnel
 */
bool ppe_drv_tun_prgm_prsr_gre_deconfigure(struct ppe_drv_tun_prgm_prsr *pgm_psr)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_prgm_prsr_gre *gre_data =  &pgm_psr->ctx.data.gre;

	ppe_drv_assert((pgm_psr->ctx.mode == PPE_DRV_TUN_PROGRAM_MODE_GRE), "program mode not GRE for program type : %d", pgm_psr->parser_idx);

	if (!ppe_drv_tun_prgm_prsr_deconfigure(&pgm_psr->ctx.prsr_cfg, pgm_psr->parser_idx)) {
		ppe_drv_warn("%p: program entry delete failed for L3 GRE Tunnel", p);
		return false;
	}

	/*
	 * delete Program UDF entry for GRETUN IPv4
	 */
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(pgm_psr->parser_idx, &gre_data->ipv4_udf)) {
		ppe_drv_warn("%p: program UDF entry delete failed for L3 GRE Tunnel", p);
		return false;
	}

	/*
	 * delete Program UDF entry for GRETUN IPv6
	 */
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(pgm_psr->parser_idx, &gre_data->ipv6_udf)) {
		ppe_drv_warn("%p: program UDF entry delete failed for L3 GRE Tunnel", p);
		return false;
	}

	/*
	 * delete Program UDF entry for GRETAP
	 */
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(pgm_psr->parser_idx, &gre_data->eth_udf)) {
		ppe_drv_warn("%p: program UDF entry delete failed for L2 GRETAP Tunnel", p);
		return false;
	}

	/*
	 * delete Program UDF entry for GRETAP + CSUM
	 */
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(pgm_psr->parser_idx, &gre_data->eth_csum_udf)) {
		ppe_drv_warn("%p: program UDF entry delete failed for L2 GRETAP Tunnel + CSUM", p);
		return false;
	}
	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_gre_configure
 *     Configure Program tunnel parser for L2/L3 GRE tunnel
 */
bool ppe_drv_tun_prgm_prsr_gre_configure(struct ppe_drv_tun_prgm_prsr *program_parser)
{
	struct ppe_drv *p = ppe_drv_gbl;
	uint8_t parser_idx = program_parser->parser_idx;
	struct ppe_drv_tun_prgm_prsr_cfg *cfg = &program_parser->ctx.prsr_cfg;
	struct ppe_drv_tun_prgm_prsr_decap_key *key = &program_parser->ctx.key;
	struct ppe_drv_tun_prgm_prsr_gre *gre_data =  &program_parser->ctx.data.gre;
	struct ppe_drv_tun_prgm_prsr_prgm_udf *ipv4_udf = &gre_data->ipv4_udf;
	struct ppe_drv_tun_prgm_prsr_prgm_udf *ipv6_udf = &gre_data->ipv6_udf;
	struct ppe_drv_tun_prgm_prsr_prgm_udf *eth_udf = &gre_data->eth_udf;
	struct ppe_drv_tun_prgm_prsr_prgm_udf *eth_csum_udf = &gre_data->eth_csum_udf;

	ppe_drv_assert((program_parser->ctx.mode == PPE_DRV_TUN_PROGRAM_MODE_GRE), "program mode not GRE for program type : %d", parser_idx);

	if (ppe_drv_tun_prgm_prsr_configured(program_parser)) {
		ppe_drv_trace("%p: Tunnel Programable Parser is already configured for L2/L3 GRE", p);
		return true;
	}

	key->key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_SRC_IP);
	key->key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_DEST_IP);
	key->key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_L4_PROTO);

	/*
	 * Configure tunnel program entry for GRETUN without key
	 */
	cfg->ip_ver = PPE_DRV_TUN_PRGM_PRSR_IP_VER_IPV4_IPV6;
	cfg->outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_GRE;

	/*
	 * Match the 32 bit data from start of GRE header.
	 * Match the first 16 bits which contains flags
	 * Do not match the next 32 bits which contains protocol.
	 * Protocol match requires UDF entries
	 */
	cfg->protocol = PPE_DRV_TUN_DECAP_GRE_CFG_PROTOCOL;
	cfg->protocol_mask = PPE_DRV_TUN_DECAP_GRE_CFG_PROTOCOL_MASK;

	/*
	 * Configure the program registers for GRE.
	 */
	cfg->inner_mode = PPE_DRV_TUN_PRGM_PRSR_INNER_MODE_UDF;
	cfg->pos_mode = PPE_DRV_TUN_PRGM_PRSR_POS_MODE_START;
	cfg->pos_mode = PPE_DRV_TUN_PRGM_PRSR_POS_MODE_START;
	cfg->conf.udf.hdr_len = PPE_DRV_TUN_DECAP_GRE_BASIC_HEADER_LEN;
	cfg->conf.udf.udf_offset[0] = PPE_DRV_TUN_DECAP_GRE_FLAGS_OFFSET;
	cfg->conf.udf.udf_offset[1] = PPE_DRV_TUN_DECAP_GRE_PROTO_OFFSET;

	if (!ppe_drv_tun_prgm_prsr_configure(cfg, key, parser_idx)) {
		ppe_drv_warn("%p: program entry configuration failed for GRE", p);
		return false;
	}

	/*
	 * UDF entry to match L3 GRE tunnel with IPv4
	 */
	ppe_drv_tun_prgm_udf_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
	ppe_drv_tun_prgm_udf_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);

	ipv4_udf->udf_val[0] = 0;
	ipv4_udf->udf_val[1] = ETH_P_IP;
	ipv4_udf->udf_mask[0] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	ipv4_udf->udf_mask[1] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	ipv4_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV4;
	ppe_drv_tun_prgm_udf_action_bitmap_set(ipv4_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
	ipv4_udf->hdr_len = 0;
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_configure(parser_idx, ipv4_udf)) {
		ppe_drv_warn("%p: program udf entry configuration failed for IPv4 GRETUN", p);
		return false;
	}

	/*
	 * UDF entry to match L3 GRE tunnel with IPv6
	 */
	ppe_drv_tun_prgm_udf_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
	ppe_drv_tun_prgm_udf_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
	ipv6_udf->udf_val[0] = 0;
	ipv6_udf->udf_val[1] = ETH_P_IPV6;
	ipv6_udf->udf_mask[0] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	ipv6_udf->udf_mask[1] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	ipv6_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV6;
	ppe_drv_tun_prgm_udf_action_bitmap_set(ipv6_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
	ipv6_udf->hdr_len = 0;
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_configure(parser_idx, ipv6_udf)) {
		ppe_drv_warn("%p: program udf entry configuration failed for IPv6 GRETUN", p);
		return false;
	}

	/*
	 * UDF entry to match L2 GRE tunnel without key
	 */
	ppe_drv_tun_prgm_udf_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
	ppe_drv_tun_prgm_udf_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
	eth_udf->udf_val[0] = 0;
	eth_udf->udf_val[1] = ETH_P_TEB;
	eth_udf->udf_mask[0] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	eth_udf->udf_mask[1] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	eth_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
	ppe_drv_tun_prgm_udf_action_bitmap_set(eth_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
	eth_udf->hdr_len = 0;
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_configure(parser_idx, eth_udf)) {
		ppe_drv_warn("%p: program udf entry configuration failed for GRETAP", p);
		return false;
	}

	/*
	 * Add additional entry in program parser UDF to match GRE without key + CSUM enabled
	 */
	ppe_drv_tun_prgm_udf_bitmap_set(eth_csum_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
	ppe_drv_tun_prgm_udf_bitmap_set(eth_csum_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
	eth_csum_udf->udf_val[0] = PPE_DRV_TUN_DECAP_GRE_CSUM_ENABLED;
	eth_csum_udf->udf_val[1] = ETH_P_TEB;
	eth_csum_udf->udf_mask[0] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;
	eth_csum_udf->udf_mask[1] = PPE_DRV_TUN_DECAP_GRE_UDF_MASK;

	/*
	 * Enable header length for this UDF field as checksum field is 2 bytes
	 * When checksum is included as per RFC offset field is also mandatory hence the total
	 * additional length to be included would be checksum field length + offset field length.
	 * We dont support Routing field configuration so offset field value would always be set to 0.
	 */
	ppe_drv_tun_prgm_udf_action_bitmap_set(eth_csum_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_HDR_LEN);
	eth_csum_udf->hdr_len = PPE_DRV_TUN_DECAP_GRE_CSUM_LENGTH + PPE_DRV_TUN_DECAP_GRE_OFFSET_LENGTH;

	ppe_drv_tun_prgm_udf_action_bitmap_set(eth_csum_udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);
	eth_csum_udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
	if (!ppe_drv_tun_prgm_prsr_prgm_udf_configure(parser_idx, eth_csum_udf)) {
		ppe_drv_warn("%p: program udf entry configuration failed for GRETAP", p);
		return false;
	}
	return true;
}
