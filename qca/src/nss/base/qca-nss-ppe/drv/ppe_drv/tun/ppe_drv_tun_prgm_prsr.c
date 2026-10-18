/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/if_tunnel.h>
#include <fal_tunnel.h>
#include <ppe_drv/ppe_drv.h>
#include "ppe_drv_tun_prgm_prsr.h"
#include "ppe_drv_tun.h"

/*
 * Global Constant to convert program type to FAL tunnel type
 */
const fal_tunnel_type_t program_to_tunnel_type_map[PPE_DRV_TUN_PRGM_TYPE_MAP_SIZE] = {
	FAL_TUNNEL_TYPE_PROGRAM0,
	FAL_TUNNEL_TYPE_PROGRAM1,
	FAL_TUNNEL_TYPE_PROGRAM2,
	FAL_TUNNEL_TYPE_PROGRAM3,
	FAL_TUNNEL_TYPE_PROGRAM4,
	FAL_TUNNEL_TYPE_PROGRAM5,
#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	FAL_TUNNEL_TYPE_PROGRAM6,
	FAL_TUNNEL_TYPE_PROGRAM7,
	FAL_TUNNEL_TYPE_PROGRAM8,
	FAL_TUNNEL_TYPE_PROGRAM9,
	FAL_TUNNEL_TYPE_PROGRAM10,
	FAL_TUNNEL_TYPE_PROGRAM11,
	FAL_TUNNEL_TYPE_PROGRAM12,
	FAL_TUNNEL_TYPE_PROGRAM13,
	FAL_TUNNEL_TYPE_PROGRAM14,
	FAL_TUNNEL_TYPE_PROGRAM15,
#endif
	FAL_TUNNEL_TYPE_INVALID_TUNNEL	/* Sentinel value for invalid indices */
};

#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
/*
 *  Get FAL protocol mode from Program Parser entry position mode
 */
static fal_tunnel_program_pos_mode_t ppe_drv_tun_get_proto_pos_modemode(enum ppe_drv_tun_prgm_prsr_proto_pos_mode mode)
{
    switch (mode) {
        case PPE_DRV_TUN_PRGM_PRSR_PROTO_POS_MODE_END:
            return FAL_TUNNEL_PROGRAM_POS_MODE_END;
        case PPE_DRV_TUN_PRGM_PRSR_PROTO_POS_MODE_START:
            return FAL_TUNNEL_PROGRAM_POS_MODE_START;
        default:
            return FAL_TUNNEL_PROGRAM_POS_MODE_END;
    }
}
#endif

/*
 * ppe_drv_tun_prgm_prsr_free
 *	free program parser instance
 */
static void ppe_drv_tun_prgm_prsr_entry_free(struct kref *kref)
{
	struct ppe_drv_tun_prgm_prsr *pgm =  container_of(kref, struct ppe_drv_tun_prgm_prsr, ref);

	switch (pgm->ctx.mode) {
	case PPE_DRV_TUN_PROGRAM_MODE_GRE:
		if (!ppe_drv_tun_prgm_prsr_gre_deconfigure(pgm)) {
			ppe_drv_warn("%p: error deleting tunnel progamable parser entry for type: %d, mode %d", pgm, pgm->parser_idx, pgm->ctx.mode);
			}
		break;

	case PPE_DRV_TUN_PROGRAM_MODE_L2TP_V2:
		if (!ppe_drv_tun_prgm_prsr_l2tp_deconfigure(pgm)){
			ppe_drv_warn("%p: error deleting tunnel progamable parser entry for type: %d, mode %d",
					pgm, pgm->parser_idx, pgm->ctx.mode);
		}
		break;

#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	case PPE_DRV_TUN_PROGRAM_MODE_TPR_RPS:
		if (!ppe_drv_tun_rps_prgm_prsr_deconfigure(pgm)){
			ppe_drv_warn("%p: error deleting tunnel progamable parser entry for type: %d, mode %d",
					pgm, pgm->parser_idx, pgm->ctx.mode);
		}
		break;
#endif

	case PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L2:
	case PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L3:
		if (!ppe_drv_tun_custom_prgm_prsr_deconfigure(pgm)) {
			ppe_drv_warn("%p: error deconfiguring custom tunnel parser entry for type: %d, mode %d",
					pgm, pgm->parser_idx, pgm->ctx.mode);
		}
		break;

	default:
		ppe_drv_warn("%p: unknown programable parser mode %d", pgm, pgm->ctx.mode);
		break;
	}

	memset(&pgm->ctx, 0, sizeof(pgm->ctx));
}

/*
 * ppe_drv_tun_prgm_prsr_deref
 *	Release reference taken on Program Parser Instance
 */
bool ppe_drv_tun_prgm_prsr_deref(struct ppe_drv_tun_prgm_prsr *pgm)
{
	uint8_t parser_idx = pgm->parser_idx;

	ppe_drv_assert(kref_read(&pgm->ref), "ref count under run for program%d tunnel parser", pgm->parser_idx);
	if (kref_put(&pgm->ref, ppe_drv_tun_prgm_prsr_entry_free)) {
		ppe_drv_warn("reference count is 0 for programable parser index: %d", parser_idx);
		return true;
	}
	ppe_drv_trace("%p: mode: %u ref dec:%u", pgm, pgm->ctx.mode, kref_read(&pgm->ref));

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_ref
 *	Take reference of Program Parser Instance
 */
void ppe_drv_tun_prgm_prsr_ref(struct ppe_drv_tun_prgm_prsr *pgm)
{
	kref_get(&pgm->ref);

	ppe_drv_assert(kref_read(&pgm->ref), "%p: ref count rollover for program type:%d", pgm, pgm->parser_idx);
	ppe_drv_trace("%p: mode: %u ref inc:%u", pgm, pgm->ctx.mode, kref_read(&pgm->ref));
}

/*
 * ppe_drv_tun_prgm_prsr_deconfigure
 * 	Configure program parser instance
 */
bool ppe_drv_tun_prgm_prsr_deconfigure(struct ppe_drv_tun_prgm_prsr_cfg *prsr_cfg, uint8_t parser_idx)
{
	fal_tunnel_decap_key_t ptdkcfg =  {0};
	fal_tunnel_program_entry_t pgm = {0};
	fal_tunnel_program_cfg_t cfg = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	fal_tunnel_type_t tunnel_type;
	sw_error_t err;
#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	fal_tunnel_decap_miss_action_t dma = {0};
#endif

	/*
	 * reset  decap key configuration as the same program can be used for other
	 * tunnels after free
	 */
	tunnel_type = ppe_drv_tun_get_tunnel_type_from_pgm_type(parser_idx);
	if (tunnel_type ==  FAL_TUNNEL_TYPE_INVALID_TUNNEL) {
		ppe_drv_warn("%p: Invalid tunnel type for parser_idx %d", p, parser_idx);
		return false;
	}

	err = fal_tunnel_decap_key_set(PPE_DRV_SWITCH_ID, tunnel_type, &ptdkcfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Tunnel Decap key reset failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	/*
	 * Reset Decap miss action configurations
	 */
	err = fal_tunnel_decap_miss_action_set(PPE_DRV_SWITCH_ID, tunnel_type, &dma);
	if (err != SW_OK) {
		ppe_drv_warn("%p: decap miss action configuration reset failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}
#endif
	/*
	 * Delete program entry
	 */
	pgm.ip_ver = prsr_cfg->ip_ver;
	pgm.outer_hdr_type = PPE_DRV_TUN_PRGM_PRSR_OUT_HDR_TO_FAL_OUT_HDR(prsr_cfg->outer_hdr);
	pgm.protocol = prsr_cfg->protocol;
	pgm.protocol_mask = prsr_cfg->protocol_mask;
#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	pgm.protocol_pos_valid = prsr_cfg->protocol_pos_valid;
	pgm.protocol_pos_mode = ppe_drv_tun_get_proto_pos_modemode(prsr_cfg->proto_pos_mode);
	pgm.protocol_pos_offset = prsr_cfg->protocol_pos_offset;
	pgm.tuple_id_valid = prsr_cfg->tuple_id_valid;
	pgm.tuple_id = prsr_cfg->tuple_id;
#endif

	err = fal_tunnel_program_entry_del(PPE_DRV_SWITCH_ID, parser_idx, &pgm);
	if (err != SW_OK) {
		ppe_drv_warn("%p: program entry delete failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

	/*
	 * Reset Program configurations set for tunnel
	 */
	err = fal_tunnel_program_cfg_set(PPE_DRV_SWITCH_ID, parser_idx, &cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: program entry configuration reset failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_configure
 * 	Configure program parser instance
 */
bool ppe_drv_tun_prgm_prsr_configure(struct ppe_drv_tun_prgm_prsr_cfg *prsr_cfg, struct ppe_drv_tun_prgm_prsr_decap_key *key, uint8_t parser_idx)
{
	fal_tunnel_decap_key_t ptdkcfg =  {0};
	fal_tunnel_program_entry_t pgm = {0};
	fal_tunnel_program_cfg_t cfg = {0};
	struct ppe_drv *p = ppe_drv_gbl;
	fal_tunnel_type_t tunnel_type;
	sw_error_t err;
#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	fal_tunnel_decap_miss_action_t dma = {0};
#endif

	/*
	 * Set decap key configurations
	 */
	ptdkcfg.key_bmp = key->key_bitmap;
	ptdkcfg.tunnel_info_mask = key->tunnel_info_mask;
	ptdkcfg.udf0_idx = key->udf0_id;
	ptdkcfg.udf1_idx = key->udf1_id;
	ptdkcfg.udf0_mask = key->udf0_mask;
	ptdkcfg.udf1_mask = key->udf1_mask;

#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	/*
	 * Index used to match lower/upper 16 bits of tunnel_info
	 */
	ptdkcfg.tunnel_info_udf0_idx = key->tunnel_info_udf0_id;
	ptdkcfg.tunnel_info_udf1_idx = key->tunnel_info_udf1_id;
#endif

	tunnel_type = ppe_drv_tun_get_tunnel_type_from_pgm_type(parser_idx);
	if (tunnel_type == FAL_TUNNEL_TYPE_INVALID_TUNNEL) {
		ppe_drv_warn("%p: Invalid tunnel type for parser_idx %d", p, parser_idx);
		return false;
	}

	err = fal_tunnel_decap_key_set(PPE_DRV_SWITCH_ID, tunnel_type, &ptdkcfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Tunnel Decap key set failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	/*
	 * Set decap miss action for this tunnel type
	 */
	dma.decap_en = key->decap_en_action;
	dma.service_code_en = key->service_code_en;
	dma.service_code = key->service_code;
	err = fal_tunnel_decap_miss_action_set(PPE_DRV_SWITCH_ID, tunnel_type, &dma);
	if (err != SW_OK) {
		ppe_drv_warn("%p: Tunnel Decap miss action set failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}
#endif

	/*
	 * Configure tunnel program entry
	 */
	pgm.ip_ver = prsr_cfg->ip_ver;
	pgm.outer_hdr_type = PPE_DRV_TUN_PRGM_PRSR_OUT_HDR_TO_FAL_OUT_HDR(prsr_cfg->outer_hdr);
	pgm.protocol = prsr_cfg->protocol;
	pgm.protocol_mask = prsr_cfg->protocol_mask;
#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	pgm.protocol_pos_valid = prsr_cfg->protocol_pos_valid;
	pgm.protocol_pos_mode = ppe_drv_tun_get_proto_pos_modemode(prsr_cfg->proto_pos_mode);
	pgm.protocol_pos_offset = prsr_cfg->protocol_pos_offset;
	pgm.tuple_id_valid = prsr_cfg-> tuple_id_valid;
	pgm.tuple_id = prsr_cfg->tuple_id;
#endif
	err = fal_tunnel_program_entry_add(PPE_DRV_SWITCH_ID, parser_idx, &pgm);
	if (err != SW_OK) {
		ppe_drv_warn("%p: program entry add failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

	/*
	 * Set program Configuration
	 */
	cfg.inner_type_mode = prsr_cfg->inner_mode;
	cfg.program_pos_mode = prsr_cfg->pos_mode;
	if (prsr_cfg->inner_mode == PPE_DRV_TUN_PRGM_PRSR_INNER_MODE_FIX) {
		 /*
		  * Caller must ensure conf.fix is properly initialized
		  */
		cfg.inner_hdr_type = (fal_hdr_type_t)prsr_cfg->conf.fix.inner_hdr;
	} else {
		/*
		 * UDF mode configurations.
		 */
		cfg.basic_hdr_len = prsr_cfg->conf.udf.hdr_len;
		cfg.opt_len_unit = prsr_cfg->conf.udf.len_unit;
		cfg.opt_len_mask = prsr_cfg->conf.udf.len_mask;
		cfg.udf_offset[0] = prsr_cfg->conf.udf.udf_offset[0];
		cfg.udf_offset[1] = prsr_cfg->conf.udf.udf_offset[1];
		cfg.udf_offset[2] = prsr_cfg->conf.udf.udf_offset[2];
	}

	err = fal_tunnel_program_cfg_set(PPE_DRV_SWITCH_ID, parser_idx, &cfg);
	if (err != SW_OK) {
		ppe_drv_warn("%p: program entry configuration failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_prgm_udf_fill
 *     Fill FAL udf structure based on UDF configuration
 */
bool ppe_drv_tun_prgm_prsr_prgm_udf_fill(fal_tunnel_program_udf_t *fal_udf, struct ppe_drv_tun_prgm_prsr_prgm_udf *udf)
{
	int i;

	fal_udf->field_flag = udf->udf_bitmap;

	for (i = 0; i < PPE_DRV_TUN_PRGM_UDF_MAX; i++) {
		if (ppe_drv_tun_prgm_udf_bitmap_check(udf, i)) {
			fal_udf->udf_val[i] = udf->udf_val[i];
			fal_udf->udf_mask[i] = udf->udf_mask[i];
		}
	}

	fal_udf->action_flag = udf->action_bitmap;

	if (ppe_drv_tun_prgm_udf_action_bitmap_check(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE)) {
		fal_udf->action_flag |= FAL_TUNNEL_PROGRAM_UDF_ACTION_INNER_HDR_TYPE;
		fal_udf->inner_hdr_type = (fal_hdr_type_t)udf->inner_hdr;
	}

	if (ppe_drv_tun_prgm_udf_action_bitmap_check(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_HDR_LEN)) {
		fal_udf->action_flag |= FAL_TUNNEL_PROGRAM_UDF_ACTION_UDF_HDR_LEN;
		fal_udf->udf_hdr_len = udf->hdr_len;
	}

	if (ppe_drv_tun_prgm_udf_action_bitmap_check(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_EXCEPTION_EN)) {
		fal_udf->action_flag |= FAL_TUNNEL_PROGRAM_UDF_ACTION_EXCEPTION_EN;
	}

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure
 *     Deonfigure Program UDF entry
 */
bool ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(uint8_t parser_idx, struct ppe_drv_tun_prgm_prsr_prgm_udf *udf)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_tunnel_program_udf_t fal_udf = {0};
	sw_error_t err;

	ppe_drv_tun_prgm_prsr_prgm_udf_fill(&fal_udf, udf);

	err = fal_tunnel_program_udf_del(PPE_DRV_SWITCH_ID, parser_idx, &fal_udf);
	if (err != SW_OK) {
		ppe_drv_warn("%p: program udf entry del failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_prgm_udf_configure
 *     Configure Program UDF entry
 */
bool ppe_drv_tun_prgm_prsr_prgm_udf_configure(uint8_t parser_idx, struct ppe_drv_tun_prgm_prsr_prgm_udf *udf)
{
	struct ppe_drv *p = ppe_drv_gbl;
	fal_tunnel_program_udf_t fal_udf = {0};
	sw_error_t err;

	ppe_drv_tun_prgm_prsr_prgm_udf_fill(&fal_udf, udf);

	err = fal_tunnel_program_udf_add(PPE_DRV_SWITCH_ID, parser_idx, &fal_udf);
	if (err != SW_OK) {
		ppe_drv_warn("%p: program udf entry configuration failed for Program%d with error %d", p, parser_idx, err);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_configured
 *	Check if the program parser is already configured
 */
bool ppe_drv_tun_prgm_prsr_configured(struct ppe_drv_tun_prgm_prsr *pgm)
{
	return ((kref_read(&pgm->ref) > 1) ?  true : false);
}

/*
 * ppe_drv_tun_prgm_prsr_type_configured
 *	Check if the program parser is already configured
 */
bool ppe_drv_tun_prgm_prsr_type_allocated(enum ppe_drv_tun_prgm_prsr_mode prsr_mode)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_prgm_prsr *pgm = p->pgm;
	int i;

	if (prsr_mode == PPE_DRV_TUN_PROGRAM_MODE_NONE) {
		ppe_drv_trace("%p: invalid parser type search\n", p);
		return false;
	}

	for (i = 0; i < PPE_DRV_TUN_PRGM_PRSR_MAX; i++) {
		if (pgm[i].ctx.mode == prsr_mode) {
			return true;
		}
	}

	return false;
}

/*
 * ppe_drv_tun_prgm_prsr_fixed_cfg_equal
 *	Compare Program Parser Fixed configurations
 *	a -> represents exisiting configuration
 *	b -> proposed new configurations
 */
static bool ppe_drv_tun_prgm_prsr_fixed_cfg_equal(struct ppe_drv_tun_prgm_prsr_fixed_cfg *a,
							struct ppe_drv_tun_prgm_prsr_fixed_cfg *b)
{
	if (!a || !b) {
		return false;
	}

	return (a->inner_hdr == b->inner_hdr) &&
	       (a->hdr_len == b->hdr_len) &&
	       (a->len_unit == b->len_unit);
}

/*
 * ppe_drv_tun_prgm_prsr_udf_cfg_equal
 *     Compare program parser udf configurations
 *	a -> represents exisiting configuration
 *	b -> proposed new configurations
 */
static bool ppe_drv_tun_prgm_prsr_udf_cfg_equal(struct ppe_drv_tun_prgm_prsr_prgm_udf_cfg *a,
						struct ppe_drv_tun_prgm_prsr_prgm_udf_cfg *b)
{
	int i;

	if (!a || !b) {
		return false;
	}

	if ((a->hdr_len != b->hdr_len) || (a->len_unit != b->len_unit) ||
			(a->len_mask != b->len_mask)) {
		return false;
	}

	for (i = 0; i < PPE_DRV_TUN_PRGM_UDF_MAX; i++) {
		if (a->udf_offset[i] != b->udf_offset[i])
			return false;
	}

	return true;
}

/*
 * ppe_drv_tun_prgm_prsr_cfg_equal
 *     Compare program parser cfg settings
 */
static bool ppe_drv_tun_prgm_prsr_cfg_equal(struct ppe_drv_tun_prgm_prsr_cfg *a,
						struct ppe_drv_tun_prgm_prsr_cfg *b)
{
	struct ppe_drv *p __maybe_unused = ppe_drv_gbl;

	if (!a || !b) {
		return false;
	}

	if ((a->ip_ver != b->ip_ver) || (a->outer_hdr != b->outer_hdr) ||
			(a->inner_mode != b->inner_mode) || (a->pos_mode != b->pos_mode) ||
			(a->protocol != b->protocol) || (a->protocol_mask != b->protocol_mask)) {
		ppe_drv_trace("%p: Program Parser configurations doesnt match", p);
		return false;
	}

#ifdef NSS_PPE_TUNNEL_ENHANCED_PARSER
	if ((a->protocol_pos_valid != b->protocol_pos_valid) || (a->proto_pos_mode != b->proto_pos_mode) ||
			(a->protocol_pos_offset != b->protocol_pos_offset) || (a->tuple_id_valid != b->tuple_id_valid) ||
			(a->tuple_id != b->tuple_id)) {
		ppe_drv_trace("%p: Program Parser enhanced configurations doesnt match", p);
		return false;
	}
#endif

	switch (a->inner_mode) {
		case PPE_DRV_TUN_PRGM_PRSR_INNER_MODE_FIX:
			return ppe_drv_tun_prgm_prsr_fixed_cfg_equal(&a->conf.fix, &b->conf.fix);

		case PPE_DRV_TUN_PRGM_PRSR_INNER_MODE_UDF:
			return ppe_drv_tun_prgm_prsr_udf_cfg_equal(&a->conf.udf, &b->conf.udf);

		default:
			return false;
	}
}

/*
 * ppe_drv_tun_prgm_prsr_compare_config()
 *	Check if parser configuration exists
 */
static bool ppe_drv_tun_prgm_prsr_compare_config(struct ppe_drv_tun_prgm_prsr *pgm,
							struct ppe_drv_tun_prgm_prsr_decap_cfg *dcap_cfg)
{
	struct ppe_drv_tun_prgm_prsr_cfg *prsr_cfg = NULL;
	if (!pgm || !dcap_cfg) {
		ppe_drv_trace("Invalid input to compare Program Parser configurations\n");
		return false;
	}

	if(!ppe_drv_tun_prgm_prsr_configured(pgm)) {
		ppe_drv_trace("%p: Cannot Compare as Program Parser instance is not initialized\n", pgm);
		return false;
	}

	prsr_cfg = &dcap_cfg->prsr_cfg;

	switch (pgm->ctx.mode) {
	case PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L2:
	case PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L3:
		/*
		 * For custom tunnels, reuse is based solely on outer header configuration
		 * equality (outer_hdr type, protocol, hdr_len, UDF offsets).
		 * Type-match UDF entries (inner packet type validation) are configured
		 * once when the parser is first created and are shared across all tunnels
		 * that reuse this parser instance.
		 */
		if (ppe_drv_tun_prgm_prsr_cfg_equal(&pgm->ctx.prsr_cfg, prsr_cfg)) {
			return true;
		}

		return false;

	case PPE_DRV_TUN_PROGRAM_MODE_GRE:
	case PPE_DRV_TUN_PROGRAM_MODE_L2TP_V2:
		/*
		 * For predefined modes, parser can be reused without configuration check
		 * as they are always updated with fixed configurations which would match
		 */
		return true;

	case PPE_DRV_TUN_PROGRAM_MODE_TPR_RPS:
		/*
		 * TPR configured Parsers are not expected to be re-used
		 */
		return false;

	default:
		ppe_drv_trace("%p: Unknown parser mode: %d\n", pgm, pgm->ctx.mode);
		return false;
	}
}

/*
 * ppe_drv_tun_prgm_prsr_reuse
 *      Check if the Parser instance can be reused based on Parser mode
 */
static bool ppe_drv_tun_prgm_prsr_reuse(enum ppe_drv_tun_prgm_prsr_mode mode)
{
	switch (mode) {
	case PPE_DRV_TUN_PROGRAM_MODE_GRE:
	case PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L2:
	case PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L3:
	case PPE_DRV_TUN_PROGRAM_MODE_L2TP_V2:
		return true;
		break;

	case PPE_DRV_TUN_PROGRAM_MODE_TPR_RPS:
		return false;
		break;

	default:
		ppe_drv_warn("Unexpected Program Parser mode %d", mode);
		break;
	}

	return false;
}

/*
 * ppe_drv_tun_prgm_prsr_entry_alloc
 *	Get free instance of program parser
 */
struct ppe_drv_tun_prgm_prsr *ppe_drv_tun_prgm_prsr_entry_alloc(enum ppe_drv_tun_prgm_prsr_mode mode,
							struct ppe_drv_tun_prgm_prsr_decap_cfg *dcap_cfg)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_tun_prgm_prsr *pgm = p->pgm;
	int i, free_index = -1;

	/*
	 * check if the required tunnel is already configured in any program mode.
	 * If configured take a reference and return.
	 * Else assign a free programable parser instance.
	 */
	for (i = 0; i < PPE_DRV_TUN_PRGM_PRSR_MAX; i++) {
		/*
		 * If prsr_cfg check is set check the configurations with existing parsers
		 * to check if it can be reused along with the type
		 */
		if (pgm[i].ctx.mode == mode && ppe_drv_tun_prgm_prsr_reuse(mode)) {
			if (dcap_cfg && ppe_drv_tun_prgm_prsr_compare_config(&pgm[i], dcap_cfg)) {
				if(ppe_drv_tun_prgm_prsr_configured(&pgm[i])) {
					ppe_drv_tun_prgm_prsr_ref(&pgm[i]);
					return &pgm[i];
				}
			} else if (!dcap_cfg) {
				ppe_drv_tun_prgm_prsr_ref(&pgm[i]);
				return &pgm[i];
			}
		}

		if (free_index == -1 &&
				pgm[i].ctx.mode == PPE_DRV_TUN_PROGRAM_MODE_NONE) {
			free_index = i;
		}
	}

	if (free_index == -1) {
		ppe_drv_warn("%p: No free programable Parser index found\n", pgm);
		return NULL;
	}

	/*
	 * Take reference on the programable parser instance
	 * and return the same
	 */
	kref_init(&pgm[free_index].ref);
	pgm[free_index].ctx.mode = mode;

	ppe_drv_trace("%p: Parser allocated index : %d  mode: %u ref inc:%u", &pgm[free_index], free_index,
			pgm[free_index].ctx.mode, kref_read(&pgm[free_index].ref));

	if (mode == PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L2 || mode == PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L3) {
		struct ppe_drv_tun_prgm_prsr_custom_prgm_udf *prgm_udf;
		prgm_udf = pgm[free_index].ctx.data.custom.prgm_udf;

		for (i=0; i < PPE_DRV_TUN_CUSTOM_PRGM_UDF_MAX; i++) {
			prgm_udf[i].prsr = &pgm[free_index];
		}
	}

	ppe_drv_trace("%p: mode: %u ref inc:%u", &pgm[free_index], pgm[free_index].ctx.mode, kref_read(&pgm[free_index].ref));
	return &pgm[free_index];
}

/*
 * ppe_drv_tun_prgm_prsr_free
 *	Free memory allocated for tunnel Program parser
 */
void ppe_drv_tun_prgm_prsr_free(struct ppe_drv_tun_prgm_prsr *program_parser)
{
	nss_ppe_drv_minidump_free(program_parser, "ppe_drv_tun_prgm_prsr");
	kfree(program_parser);
}

/*
 * ppe_drv_tun_prgm_prsr_alloc
 *	Initialize tunnel program parser structure
 */
struct ppe_drv_tun_prgm_prsr *ppe_drv_tun_prgm_prsr_alloc(struct ppe_drv *p)
{
	struct ppe_drv_tun_prgm_prsr *pgm;
	int index;

	ppe_drv_assert(!p->pgm, "%p: tunnel program parser entries already allocated", p);

	pgm = kzalloc(sizeof(struct ppe_drv_tun_prgm_prsr) * PPE_DRV_TUN_PRGM_PRSR_MAX, GFP_KERNEL);
	if (!pgm) {
		ppe_drv_warn("%p: failed to allocate program parser entries", p);
		return NULL;
	}

	nss_ppe_drv_minidump_log(pgm, sizeof(struct ppe_drv_tun_prgm_prsr) * PPE_DRV_TUN_PRGM_PRSR_MAX, "ppe_drv_tun_prgm_prsr");

	for (index = 0; index < PPE_DRV_TUN_PRGM_PRSR_MAX; index++) {
		pgm[index].parser_idx = index;
		pgm[index].ctx.mode = PPE_DRV_TUN_PROGRAM_MODE_NONE;
	}

	return pgm;
}
