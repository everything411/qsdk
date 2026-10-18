/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv_tun_custom.h"
#include <ppe_drv/ppe_drv.h>
#include "ppe_drv_tun_cmn_ctx.h"
#include "ppe_drv_tun_prgm_prsr.h"
#include "ppe_drv_tun.h"
#include "ppe_drv_tun_udf.h"

/*
 * ppe_drv_tun_custom_prgm_udf_entry_free
 *      Custom Program udf entry free
 */
static void ppe_drv_tun_custom_prgm_udf_entry_free(struct kref *kref)
{
	struct ppe_drv_tun_prgm_prsr_custom_prgm_udf *pgm_udf;
	uint8_t parser_idx;

	pgm_udf = container_of(kref, struct ppe_drv_tun_prgm_prsr_custom_prgm_udf, ref);
	parser_idx = pgm_udf->prsr->parser_idx;

	/*
	 * Only deconfigure from HW if the entry was successfully written to hardware.
	 * This guards against spurious HW deconfigure on entries that failed during
	 * configuration (e.g. error paths where kref was initialized but HW write failed).
	 */
	if (pgm_udf->hw_configured) {
		ppe_drv_tun_prgm_prsr_prgm_udf_deconfigure(parser_idx, &pgm_udf->udf);
	}

	/*
	 * Reset the program UDF slot to all zeros for future reuse
	 */
	memset(&pgm_udf->udf, 0, sizeof(struct ppe_drv_tun_prgm_prsr_prgm_udf));
	pgm_udf->hw_configured = false;
}

/*
 * ppe_drv_tun_custom_prgm_udf_dref
 *      custom program udf deref
 */
bool ppe_drv_tun_custom_prgm_udf_dref(struct ppe_drv_tun_prgm_prsr_custom_prgm_udf *pgm_udf, int index)
{
	ppe_drv_assert(kref_read(&pgm_udf->ref), "ref count under run for prgm udf index: %d", index);

	if (kref_put(&pgm_udf->ref, ppe_drv_tun_custom_prgm_udf_entry_free)) {
		ppe_drv_trace("reference count is 0 for udf index: %d", index);
	} else {
		ppe_drv_trace("%p: index: %u ref dec:%u", pgm_udf, index, kref_read(&pgm_udf->ref));
	}

	return true;
}

/*
 * ppe_drv_tun_custom_prgm_udf_ref
 *      custom program udf ref
 */
void ppe_drv_tun_custom_prgm_udf_ref(struct ppe_drv_tun_prgm_prsr_custom_prgm_udf *pgm_udf, int index)
{
	kref_get(&pgm_udf->ref);

	ppe_drv_assert(kref_read(&pgm_udf->ref), "%p: ref count rollover for udf index:%d parser_idx = %d", pgm_udf, index, pgm_udf->prsr->parser_idx);
	ppe_drv_trace("%p: idx: prsr idx %u udf idx %d ref inc:%u", pgm_udf, pgm_udf->prsr->parser_idx, index, kref_read(&pgm_udf->ref));
}

/*
 * ppe_drv_tun_custom_prgm_udf_get_free_index
 *     Get free instance of custom program udf
 */
int ppe_drv_tun_custom_prgm_udf_get_free_index(struct ppe_drv_tun_prgm_prsr *pgm)
{
	int index = 0;
	struct ppe_drv_tun_prgm_prsr_custom_prgm_udf *pgm_udf;

	/*
	 * Return first free instance
	 */
	for (index = 0; index < PPE_DRV_TUN_CUSTOM_PRGM_UDF_MAX; index++) {
		pgm_udf = &pgm->ctx.data.custom.prgm_udf[index];
		if (kref_read(&pgm_udf->ref)) {
			continue;
		}

		kref_init(&pgm_udf->ref);
		ppe_drv_trace("%p: Free program udf instance found for custom tunnel, index: %d", pgm_udf, index);
		return index;
	}

	ppe_drv_warn("%p: Free tunnel udf instance is not found", pgm);
	return -1;
}

/*
 * ppe_drv_tun_custom_prgm_udf_config_exists
 *     Check if custom program udf configuration exists
 */
int ppe_drv_tun_custom_prgm_udf_config_exists(struct ppe_drv_tun_prgm_prsr *pgm, struct ppe_drv_tun_prgm_prsr_prgm_udf *udf)
{
	struct ppe_drv_tun_prgm_prsr_prgm_udf *prsr_udf = NULL;
	struct ppe_drv_tun_prgm_prsr_custom_prgm_udf *cust_udf = NULL;
	int i, j;
	bool match = false;

	/*
	 * Add NULL check to prevent crash during error paths
	 */
	if (!pgm || !udf) {
		ppe_drv_warn("Program Parser instance/ UDF instance is invalid (pgm=%p, udf=%p)", pgm, udf);
		return -1;
	}

	if (pgm->ctx.mode != PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L2 && pgm->ctx.mode != PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L3) {
		ppe_drv_warn("Parser mode is not set to custom mode for index %d", pgm->parser_idx);
		return -1;
	}

	for (j = 0; j < PPE_DRV_TUN_CUSTOM_PRGM_UDF_MAX; j++) {
		cust_udf = &pgm->ctx.data.custom.prgm_udf[j];
		prsr_udf = &cust_udf->udf;

		if (!kref_read(&cust_udf->ref)) {
			continue;
		}

		match = true;

		for (i = 0; i < PPE_DRV_TUN_PRGM_UDF_MAX; i++) {
			if (prsr_udf->udf_val[i] != udf->udf_val[i] ||
					prsr_udf->udf_mask[i] != udf->udf_mask[i]) {
				match = false;
				break;
			}
		}

		if (!match || prsr_udf->action_bitmap != udf->action_bitmap ||
				prsr_udf->udf_bitmap != udf->udf_bitmap ||
				prsr_udf->hdr_len != udf->hdr_len ||
				prsr_udf->inner_hdr != udf->inner_hdr) {
			continue;
		}

		return j;
	}

	return -1;
}

/*
 * ppe_drv_tun_custom_prgm_prsr_deconfigure
 *      Deconfigure Custom Program Parser instance
 */
bool ppe_drv_tun_custom_prgm_prsr_deconfigure(struct ppe_drv_tun_prgm_prsr *program)
{
	struct ppe_drv *p = ppe_drv_gbl;
	bool ret = true;
	int i;

	/*
	 * Deconfigure parser from hardware.
	 * Do not return early on failure - UDF entries must still be cleaned up
	 * regardless of whether the hardware parser deconfigure succeeds (e.g.,
	 * if the parser was never configured in hardware due to an earlier error).
	 */
	if (!ppe_drv_tun_prgm_prsr_deconfigure(&program->ctx.prsr_cfg, program->parser_idx)) {
		ppe_drv_warn("%p: program entry delete failed for Custom Tunnel at prgm index %d", p, program->parser_idx);
		ret = false;
	}

	/*
	 * Clean up all program UDF entries owned by this parser.
	 * These are type-matching UDF entries (udf0/1/2) that were configured
	 * when the parser was first created.
	 */
	for (i = 0; i < PPE_DRV_TUN_CUSTOM_PRGM_UDF_MAX; i++) {
		if (kref_read(&program->ctx.data.custom.prgm_udf[i].ref)) {
			ppe_drv_tun_custom_prgm_udf_dref(&program->ctx.data.custom.prgm_udf[i], i);
		}
	}

	return ret;
}

/*
 * ppe_drv_tun_custom_tun_udf_configure
 *     Configure tunnel UDF for custom tunnel at ptdc level
 */
bool ppe_drv_tun_custom_tun_udf_configure(struct ppe_drv_tun_decap *ptdc, struct ppe_drv_tun_cmn_ctx *th)
{
	struct ppe_drv_tun_udf_profile udf_pf = {0};
	struct ppe_drv_tun_cmn_ctx_instance_match *inst_match;

	if (!ptdc || !ptdc->pgm_prsr) {
		ppe_drv_warn("Invalid ptdc or parser not configured");
		return false;
	}

	inst_match = &th->tun.user.decap_data.instance_match;

	/*
	 * Check if instance UDFs are enabled
	 */
	if (!inst_match->instance_udf0_en && !inst_match->instance_udf1_en) {
		ppe_drv_trace("Instance UDFs not enabled, skipping tun_udf configuration");
		return true;
	}

	/*
	 * Build UDF profile configuration for instance identification
	 */
	if (inst_match->instance_udf0_en) {
		ppe_drv_tun_udf_bitmask_set(&udf_pf, PPE_DRV_TUN_CUSTOM_UDF3);
		udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF3].offset = th->tun.user.decap_data.instance_udf0_offset;

		/*
		 * Set offset type based on configuration
		 */
		if (th->tun.user.decap_data.offset_type == PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L2_START) {
			udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF3].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L2;
		} else if (th->tun.user.decap_data.offset_type == PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L3_START) {
			udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF3].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L3;
		} else {
			udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF3].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L4;
		}
	}

	if (inst_match->instance_udf1_en) {
		ppe_drv_tun_udf_bitmask_set(&udf_pf, PPE_DRV_TUN_CUSTOM_UDF4);
		udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF4].offset = th->tun.user.decap_data.instance_udf1_offset;

		/*
		 * Set offset type
		 */
		if (th->tun.user.decap_data.offset_type == PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L2_START) {
			udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF4].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L2;
		} else if (th->tun.user.decap_data.offset_type == PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L3_START) {
			udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF4].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L3;
		} else {
			udf_pf.udf[PPE_DRV_TUN_CUSTOM_UDF4].offset_type = PPE_DRV_TUN_UDF_OFFSET_TYPE_L4;
		}
	}

	udf_pf.l3_match = false;
	udf_pf.l4_match = false;
	udf_pf.program_match = true;
	udf_pf.program_type = ptdc->pgm_prsr->parser_idx;

	/*
	 * Configure tunnel UDF at ptdc level (per-tunnel).
	 * This function will search for existing matching entry or allocate new one.
	 * Reference counting is handled automatically.
	 */
	ptdc->tun_udf = ppe_drv_tun_udf_entry_configure(&udf_pf);
	if (!ptdc->tun_udf) {
		ppe_drv_warn("Failed to configure/find tunnel udf for ptdc %p", ptdc);
		return false;
	}

	return true;
}

/*
 * ppe_drv_tun_custom_tun_udf_deconfigure
 *     Custom tunnel UDF deconfigure - now works with per-tunnel tun_udf
 */
bool ppe_drv_tun_custom_tun_udf_deconfigure(struct ppe_drv_tun *ptun)
{
	struct ppe_drv_tun_cmn_ctx_instance_match *inst_match;

	if (!ptun->ptdc) {
		ppe_drv_warn("Tunnel decap not configured");
		return false;
	}

	inst_match = &ptun->th.tun.user.decap_data.instance_match;

	/*
	 * Check if tunnel UDF needs to be deconfigured.
	 * If instance UDFs are not enabled, there's nothing to deconfigure.
	 */
	if (!inst_match->instance_udf0_en && !inst_match->instance_udf1_en) {
		ppe_drv_trace("Instance UDFs not enabled for custom tunnel %p", ptun);
		return true;
	}

	/*
	 * If tun_udf is NULL, it was never configured (possibly due to allocation failure).
	 * This can happen during error paths where tunnel creation partially succeeded.
	 */
	if (!ptun->ptdc->tun_udf) {
		ppe_drv_trace("Tunnel UDF not configured for custom tunnel %p (partial configuration)", ptun);
		return true;
	}

	/*
	 * Dereference the tunnel UDF.
	 * When ref count reaches 0, it will be freed automatically.
	 */
	ppe_drv_tun_udf_entry_dref(ptun->ptdc->tun_udf);
	ptun->ptdc->tun_udf = NULL;

	return true;
}

/*
 * ppe_drv_tun_custom_prgm_prsr_configure
 *     Custom program parser configure - rewritten for array-based type_match API
 */
struct ppe_drv_tun_prgm_prsr *ppe_drv_tun_custom_prgm_prsr_configure(struct ppe_drv_tun_decap *ptdc,
					struct ppe_drv_tun_cmn_ctx *th)
{
	struct ppe_drv_tun_prgm_prsr *pgm = NULL;
	struct ppe_drv_tun_prgm_prsr_decap_cfg dcap_cfg = {0};
	struct ppe_drv_tun_cmn_ctx_type_match *type_match;
	struct ppe_drv_tun_cmn_ctx_instance_match *inst_match;
	uint8_t match_bitmap;
	int i, pgm_udf_idx[PPE_DRV_TUN_PRGM_UDF_MAX_ENTRIES];

	/* Initialize pgm_udf_idx array */
	for (i = 0; i < PPE_DRV_TUN_PRGM_UDF_MAX_ENTRIES; i++) {
		pgm_udf_idx[i] = -1;
	}

	dcap_cfg.mode = (th->type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2) ? PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L2 :
								PPE_DRV_TUN_PROGRAM_MODE_CUSTOM_L3;

	match_bitmap = th->tun.user.decap_data.match_bitmap;

	if (ppe_drv_tun_cust_match_flag_test(match_bitmap, PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_SIP_EN)) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_SRC_IP);
	}

	if (ppe_drv_tun_cust_match_flag_test(match_bitmap, PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_DIP_EN)) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_DEST_IP);
	}

	if (ppe_drv_tun_cust_match_flag_test(match_bitmap, PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_SPORT_EN)) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_SPORT);
	}

	if (ppe_drv_tun_cust_match_flag_test(match_bitmap, PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_DPORT_EN)) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_DPORT);
	}

	if (ppe_drv_tun_cust_match_flag_test(match_bitmap, PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_PROTO_EN)) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_L4_PROTO);
	}

	inst_match = &th->tun.user.decap_data.instance_match;

	if (inst_match->instance_udf0_en) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_UDF0);
		dcap_cfg.key.udf0_id = PPE_DRV_TUN_CUSTOM_UDF3;
		dcap_cfg.key.udf0_mask = inst_match->instance_udf0_mask;
	}

	if (inst_match->instance_udf1_en) {
		dcap_cfg.key.key_bitmap |= PPE_DRV_TUN_BIT(PPE_DRV_TUN_PRGM_PRSR_DECAP_KEY_UDF1);
		dcap_cfg.key.udf1_id = PPE_DRV_TUN_CUSTOM_UDF4;
		dcap_cfg.key.udf1_mask = inst_match->instance_udf1_mask;
	}

	dcap_cfg.prsr_cfg.ip_ver = (th->l3.flags & PPE_DRV_TUN_CMN_CTX_L3_IPV4) ? PPE_DRV_TUN_PRGM_PRSR_IP_VER_IPV4 : PPE_DRV_TUN_PRGM_PRSR_IP_VER_IPV6;

	if (th->tun.user.decap_data.offset_type == PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L2_START) {
		dcap_cfg.prsr_cfg.outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_ETH;
		dcap_cfg.prsr_cfg.protocol = th->l2.eth_type;
		dcap_cfg.prsr_cfg.protocol_mask = PPE_DRV_TUN_CUSTOM_ETHER_PROTOCOL_MASK;
	} else if (th->tun.user.decap_data.offset_type == PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L3_START) {
		dcap_cfg.prsr_cfg.outer_hdr = (dcap_cfg.prsr_cfg.ip_ver == PPE_DRV_TUN_PRGM_PRSR_IP_VER_IPV4) ?
								PPE_DRV_TUN_PRGM_PRSR_OHDR_IPV4 : PPE_DRV_TUN_PRGM_PRSR_OHDR_IPV6;
		dcap_cfg.prsr_cfg.protocol = th->l3.proto;
		dcap_cfg.prsr_cfg.protocol_mask = PPE_DRV_TUN_CUSTOM_IP_PROTOCOL_MASK;
	} else {
		uint32_t dst = (uint32_t)ntohs(th->tun.user.udph.dest);
		uint32_t src = (uint32_t)ntohs(th->tun.user.udph.source);
		dcap_cfg.prsr_cfg.outer_hdr = PPE_DRV_TUN_PRGM_PRSR_OHDR_UDP;
		dcap_cfg.prsr_cfg.protocol = (dst << 16) | src;
		dcap_cfg.prsr_cfg.protocol_mask = PPE_DRV_TUN_CUSTOM_UDP_PROTOCOL_MASK;
	}

	dcap_cfg.prsr_cfg.inner_mode = PPE_DRV_TUN_PRGM_PRSR_INNER_MODE_UDF;
	dcap_cfg.prsr_cfg.pos_mode = PPE_DRV_TUN_PRGM_PRSR_POS_MODE_START;
	dcap_cfg.prsr_cfg.conf.udf.hdr_len = th->tun.user.decap_data.decap_data_len;

	type_match = &th->tun.user.decap_data.type_match;

	/*
	 * Configure UDF offsets from type_match entries
	 */
	if (type_match->num_entries > 0) {
		dcap_cfg.prsr_cfg.conf.udf.udf_offset[0] = th->tun.user.decap_data.udf0_offset;
		dcap_cfg.prsr_cfg.conf.udf.udf_offset[1] = th->tun.user.decap_data.udf1_offset;
		dcap_cfg.prsr_cfg.conf.udf.udf_offset[2] = th->tun.user.decap_data.udf2_offset;
	}

	/*
	 * Check if any existing parser instance can be re-used and take ref.
	 * Else allocate a new instance and take initial ref on the same.
	 */
	pgm = ppe_drv_tun_prgm_prsr_entry_alloc(dcap_cfg.mode, &dcap_cfg);
	if (!pgm) {
		ppe_drv_warn("failed to allocate Program Parser instance for tunnel type %d",th->type);
		goto err;
	}

	/*
	 * Configure Program Parser instance for parser instance which is not already configured
	 */
	if (!ppe_drv_tun_prgm_prsr_configured(pgm)) {
		if (!ppe_drv_tun_prgm_prsr_configure(&dcap_cfg.prsr_cfg, &dcap_cfg.key, pgm->parser_idx)) {
			ppe_drv_warn("Failed to configure Program Parser %d instance for custom tunnel", pgm->parser_idx);
			goto err;
		}

		/*
		 * Save the configuration to parser context for later use during deconfiguration
		 */
		memcpy(&pgm->ctx.prsr_cfg, &dcap_cfg.prsr_cfg, sizeof(dcap_cfg.prsr_cfg));
		memcpy(&pgm->ctx.key, &dcap_cfg.key, sizeof(dcap_cfg.key));

		/*
		 * Configure program UDF entries for type matching.
		 * Loop through type_match entries and create program UDF for each.
		 */
		for (i = 0; i < type_match->num_entries && i < PPE_DRV_TUN_PRGM_UDF_MAX_ENTRIES; i++) {
			struct ppe_drv_tun_cmn_ctx_type_match_entry *entry = &type_match->entries[i];
			struct ppe_drv_tun_prgm_prsr_prgm_udf *udf;

			pgm_udf_idx[i] = ppe_drv_tun_custom_prgm_udf_get_free_index(pgm);
			if (pgm_udf_idx[i] < 0) {
				ppe_drv_warn("%p: No free custom tunnel program udf entry found for program index %d", pgm, pgm->parser_idx);
				goto err;
			}

			udf = &pgm->ctx.data.custom.prgm_udf[pgm_udf_idx[i]].udf;

			/* Set inner header type based on entry */
			switch (entry->inner_pkt_type) {
			case PPE_DRV_TUN_CMN_CTX_INNER_PKT_TYPE_ETHERNET:
				udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_ETH;
				break;
			case PPE_DRV_TUN_CMN_CTX_INNER_PKT_TYPE_IPV4:
				udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV4;
				break;
			case PPE_DRV_TUN_CMN_CTX_INNER_PKT_TYPE_IPV6:
				udf->inner_hdr = PPE_DRV_TUN_PRGM_PRSR_INNER_HDR_IPV6;
				break;
			default:
				ppe_drv_warn("Invalid inner packet type %d", entry->inner_pkt_type);
				goto err;
			}

			/*
			 * Enable inner header type checking
			 */
			ppe_drv_tun_prgm_udf_action_bitmap_set(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_CHK_INNER_HDR_TYPE);

			/*
			 * Configure UDF fields if enabled
			 */
			if (entry->udf0_en) {
				udf->udf_val[0] = entry->udf0_val;
				udf->udf_mask[0] = entry->udf0_mask;
				ppe_drv_tun_prgm_udf_bitmap_set(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF0);
			}
			if (entry->udf1_en) {
				udf->udf_val[1] = entry->udf1_val;
				udf->udf_mask[1] = entry->udf1_mask;
				ppe_drv_tun_prgm_udf_bitmap_set(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF1);
			}
			if (entry->udf2_en) {
				udf->udf_val[2] = entry->udf2_val;
				udf->udf_mask[2] = entry->udf2_mask;
				ppe_drv_tun_prgm_udf_bitmap_set(udf, PPE_DRV_TUN_PRGM_PRSR_PRGM_UDF_SET_UDF2);
			}

			if (!ppe_drv_tun_prgm_prsr_prgm_udf_configure(pgm->parser_idx, udf)) {
				ppe_drv_warn("%p: program udf entry %d configuration failed for custom tunnel index %d", pgm, i, pgm->parser_idx);
				goto err;
			}

			/*
			 * Mark the UDF slot as successfully written to hardware.
			 * The kref free callback checks this flag before calling HW deconfigure,
			 * preventing HW writes on entries that never reached hardware.
			 */
			pgm->ctx.data.custom.prgm_udf[pgm_udf_idx[i]].hw_configured = true;
		}
	}

	return pgm;

err:
	/*
	 * On error, simply dereference the parser.
	 * The parser's cleanup function will handle cleanup of all owned resources.
	 */
	if (pgm) {
		ppe_drv_tun_prgm_prsr_deref(pgm);
	}
	return NULL;
}
