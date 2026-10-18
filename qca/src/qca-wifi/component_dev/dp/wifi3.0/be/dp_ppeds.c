/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/netdevice.h>
#include <qdf_nbuf.h>
#include <dp_types.h>
#include <dp_internal.h>
#include <hal_be_hw_headers.h>
#include <dp_peer.h>
#include <dp_tx.h>
#include <dp_rx.h>
#include <be/dp_be_tx.h>
#include <hal_tx.h>
#include <hal_be_api.h>
#include <be/dp_be.h>
#include <ppe_vp_public.h>
#include "dp_ppeds.h"
#include <ppe_ds_wlan.h>
#include <wlan_osif_priv.h>
#include <dp_tx_desc.h>
#include "dp_rx.h"
#include "dp_rx_buffer_pool.h"
#include "dp_be_rx.h"

#define DS_NAPI_BUDGET_TO_INTERNAL_BUDGET(n, s) (((n) << (s)) - 1)
#define DS_INTERNAL_BUDGET_TO_NAPI_BUDGET(n, s) (((n) + 1) >> (s))
#define DS_NAPI_SHIFT 2

/**
 * dp_ppe_ds_ppe2tcl_irq_handler- Handle ppe2tcl ring interrupt
 * @irq: irq numer
 * @ctxt: context passed to the handler
 *
 * Handle ppe2tcl ring interrupt
 *
 * Return: IRQ_HANDLED
 */
irqreturn_t dp_ppe_ds_ppe2tcl_irq_handler(int irq, void *ctxt)
{
	ppe_ds_ppe2tcl_wlan_handle_intr(ctxt);

	return IRQ_HANDLED;
}

/*
 * dp_ppe_ds_reo2ppe_irq_handler: Handle reo2ppe ring interrupt
 * @irq: IRQ number
 * @ctxt: IRQ handler context
 *
 * Return: IRQ handle status
 */
irqreturn_t dp_ppe_ds_reo2ppe_irq_handler(int irq, void *ctxt)
{
	ppe_ds_reo2ppe_wlan_handle_intr(ctxt);

	return IRQ_HANDLED;
}

/**
 * dp_get_ppe_ds_ctxt - Get context from ppe ds driver
 * @soc: dp soc
 *
 * Get context from ppe driver
 *
 * Return: context
 */
void *dp_get_ppe_ds_ctxt(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	ppe_ds_wlan_handle_t *ppeds_handle =
				(ppe_ds_wlan_handle_t *)be_soc->ppeds_handle;

	if (!ppeds_handle)
		return NULL;

	return ppe_ds_wlan_get_intr_ctxt(ppeds_handle);
}

#if defined(QCA_DP_NBUF_FAST_PPEDS)
#define dp_nbuf_alloc_ppe_ds(d, s, r, a, p) \
	qdf_nbuf_alloc_ppe_ds(d, s, r, a, p)
#else
#define dp_nbuf_alloc_ppe_ds(d, s, r, a, p) \
	qdf_nbuf_alloc_simple(d, s, r, a, p)
#endif

/**
 * dp_ppeds_deinit_ppe_vp_tbl_be - PPE VP table dealoc
 * @be_soc: BE SoC
 *
 * Deallocate the VP table
 *
 * Return: QDF_STATUS
 */
static void dp_ppeds_deinit_ppe_vp_tbl_be(struct dp_soc_be *be_soc)
{
	if (be_soc->ppe_vp_tbl)
		qdf_mem_free(be_soc->ppe_vp_tbl);
	qdf_mutex_destroy(&be_soc->ppe_vp_tbl_lock);
}

/**
 * dp_ppeds_init_ppe_vp_tbl_be - PPE VP table alloc
 * @be_soc: BE SoC
 *
 * Allocate the VP table
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_ppeds_init_ppe_vp_tbl_be(struct dp_soc_be *be_soc)
{
	int num_ppe_vp_entries, i;

	num_ppe_vp_entries =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	be_soc->ppe_vp_tbl =
		qdf_mem_malloc(num_ppe_vp_entries *
			       sizeof(*be_soc->ppe_vp_tbl));

	if (!be_soc->ppe_vp_tbl)
		return QDF_STATUS_E_NOMEM;

	qdf_mutex_create(&be_soc->ppe_vp_tbl_lock);

	for (i = 0; i < num_ppe_vp_entries; i++)
		be_soc->ppe_vp_tbl[i].is_configured = false;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_deinit_ppe_vp_search_idx_tbl_be - PPE VP search index table dealoc
 * @be_soc: BE SoC
 *
 * Deallocate the search index table
 *
 * Return: QDF_STATUS
 */
static void dp_ppeds_deinit_ppe_vp_search_idx_tbl_be(struct dp_soc_be *be_soc)
{
	if (be_soc->ppe_vp_search_idx_tbl)
		qdf_mem_free(be_soc->ppe_vp_search_idx_tbl);
}

/**
 * dp_ppeds_init_search_idx_tbl_be - PPE VP search index table  alloc
 * @be_soc: BE SoC
 *
 * Allocate the search index table
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_ppeds_init_ppe_vp_search_idx_tbl_be(struct dp_soc_be *be_soc)
{
	int num_vp_search_idx_entries, i;

	num_vp_search_idx_entries =
		hal_tx_get_num_ppe_vp_search_idx_tbl_entries(be_soc->soc.hal_soc);

	be_soc->ppe_vp_search_idx_tbl =
		qdf_mem_malloc(num_vp_search_idx_entries *
			       sizeof(*be_soc->ppe_vp_search_idx_tbl));

	if (!be_soc->ppe_vp_search_idx_tbl)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < num_vp_search_idx_entries; i++)
		be_soc->ppe_vp_search_idx_tbl[i].is_configured = false;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_deinit_ppe_vp_profile_be - PPE VP profile dealoc
 * @be_soc: BE SoC
 *
 * Deallocate the VP profiles
 *
 * Return: void
 */
static void dp_ppeds_deinit_ppe_vp_profile_be(struct dp_soc_be *be_soc)
{
	if (be_soc->ppe_vp_profile)
		qdf_mem_free(be_soc->ppe_vp_profile);
}

/**
 * dp_ppeds_init_ppe_vp_profile_be - PPE VP profiles alloc
 * @be_soc: BE SoC
 *
 * Allocate the ppe vp profiles
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS dp_ppeds_init_ppe_vp_profile_be(struct dp_soc_be *be_soc)
{
	int num_ppe_vp_entries, i;

	num_ppe_vp_entries =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	be_soc->ppe_vp_profile =
		(struct dp_ppe_vp_profile *)qdf_mem_malloc(num_ppe_vp_entries *
			       sizeof(struct dp_ppe_vp_profile));

	if (!be_soc->ppe_vp_profile)
		return QDF_STATUS_E_NOMEM;

	for (i = 0; i < num_ppe_vp_entries; i++)
		be_soc->ppe_vp_profile[i].is_configured = false;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_alloc_vp_search_idx_tbl_entry_be() - allocate search idx table
 * @be_soc: BE SoC
 * @be_vdev: BE VAP
 *
 * PPE VP seach index table entry alloc
 *
 * Return: Return PPE VP index to be used
 */
static int dp_ppeds_alloc_vp_search_idx_tbl_entry_be(struct dp_soc_be *be_soc,
					   struct dp_vdev_be *be_vdev)
{
	int num_ppe_vp_search_idx_max, i;

	num_ppe_vp_search_idx_max =
		hal_tx_get_num_ppe_vp_search_idx_tbl_entries(be_soc->soc.hal_soc);

	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);
	if (be_soc->num_ppe_vp_search_idx_entries == num_ppe_vp_search_idx_max) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("Maximum ppe_vp search idx count reached for vap");
		return -ENOSPC;
	}

	/*
	 * Increase the count in atomic context.
	 */
	be_soc->num_ppe_vp_search_idx_entries++;

	for (i = 0; i < num_ppe_vp_search_idx_max; i++) {
		if (!be_soc->ppe_vp_search_idx_tbl[i].is_configured)
			break;
	}

	/*
	 * As we already found the current count not maxed out, we should
	 * always find a non configured entry.
	 */
	if (dp_assert_always_internal(!(i == num_ppe_vp_search_idx_max))) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("Table entry index crosses max allowed");
		return -ENOSPC;
	}

	be_soc->ppe_vp_search_idx_tbl[i].is_configured = true;
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
	return i;
}

/**
 * dp_ppeds_dealloc_search_idx_tbl_entry_be() - PPE VP search idx table dealloc
 * @be_soc: BE SoC
 * @ppe_vp_search_idx: PPE VP search index number
 *
 * PPE VP search index table entry dealloc
 *
 * Return: void
 */
static void dp_ppeds_dealloc_vp_search_idx_tbl_entry_be(struct dp_soc_be *be_soc,
					      int ppe_vp_search_idx)
{
	int num_ppe_vp_search_idx_max;

	num_ppe_vp_search_idx_max =
		hal_tx_get_num_ppe_vp_search_idx_tbl_entries(be_soc->soc.hal_soc);

	if (ppe_vp_search_idx < 0 || ppe_vp_search_idx >= num_ppe_vp_search_idx_max) {
		dp_err("INvalid PPE VP search table free index");
		return;
	}

	/*
	 * Release the current index for reuse.
	 */
	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);
	if (!be_soc->ppe_vp_search_idx_tbl[ppe_vp_search_idx].is_configured) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("PPE VP search idx table is not configured at idx:%d", ppe_vp_search_idx);
		return;
	}

	be_soc->ppe_vp_search_idx_tbl[ppe_vp_search_idx].is_configured = false;
	be_soc->num_ppe_vp_search_idx_entries--;
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
}

/**
 * dp_ppeds_get_vdev_vp_config_be() - Get the PPE vp config
 * @be_vdev: BE vdev
 * @vp_cfg: VP config
 *
 * Get the VDEV config for PPE
 *
 * Return: void
 */
static void dp_ppeds_get_vdev_vp_config_be(struct dp_vdev_be *be_vdev,
					    union hal_tx_ppe_vp_config *vp_cfg,
					    struct dp_ppe_vp_profile *ppe_vp_profile)
{
	struct dp_vdev *vdev = &be_vdev->vdev;

	/*
	 * Update the parameters frm profile.
	 */
	vp_cfg->vp_num = ppe_vp_profile->vp_num;

	/* This is valid when index based search is
	 * enabled for STA vap in bank register
	 */
	vp_cfg->search_idx_reg_num = ppe_vp_profile->search_idx_reg_num;

	vp_cfg->use_ppe_int_pri = ppe_vp_profile->use_ppe_int_pri;
	vp_cfg->to_fw = ppe_vp_profile->to_fw;
	vp_cfg->drop_prec_enable = ppe_vp_profile->drop_prec_enable;

	vp_cfg->vdev_id = vdev->vdev_id;

	if (ppe_vp_profile->ref_count == 1) {
		vp_cfg->pmac_id = vdev->lmac_id;
		vp_cfg->bank_id = be_vdev->bank_id;
	} else {
		vp_cfg->pmac_id = PPE_VP_CFG_WILDCARD_LMAC_ID;
		vp_cfg->bank_id = be_vdev->splitphy_ds_bank_id;
	}
}

/**
 * dp_tx_ppeds_cfg_astidx_cache_mapping - Set ppe index mapping table value
 * @soc: DP SoC context
 * @vdev: DP vdev
 * @peer_map: map if true, unmap if false
 *
 * Return: void
 */
void dp_tx_ppeds_cfg_astidx_cache_mapping(struct dp_soc *soc,
					  struct dp_vdev *vdev, bool peer_map)
{
	union hal_tx_ppe_idx_map_config ppeds_idx_mapping = {0};
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_ppe_vp_profile *vp_profile;
	struct cdp_ds_vp_params vp_params = {0};
	struct cdp_soc_t *cdp_soc = &soc->cdp_soc;

	if (!cdp_soc->ol_ops->get_ppeds_profile_info_for_vap) {
		dp_err("%p: no registered callback to get ppe vp profile info\n", vdev);
		return;
	}

	if (cdp_soc->ol_ops->get_ppeds_profile_info_for_vap(soc->ctrl_psoc,
							    vdev->vdev_id,
							    &vp_params) == QDF_STATUS_E_NULL_VALUE) {
		dp_err("%p: Failed to get ppeds profile for vap\n", vdev);
		return;
	}

	if (vp_params.ppe_vp_type != PPE_VP_USER_TYPE_DS) {
		dp_warn("%p: PPEDS is not enabled for vap\n", vdev);
		return;
	}

	vp_profile = &be_soc->ppe_vp_profile[vp_params.ppe_vp_profile_idx];
	if (vdev->opmode == wlan_op_mode_sta) {
		/*
		 * Peer map event provides bss_ast_idx and bss_ast_hash params
		 * which are used as search index and cache set numbers.
		 */
		if (peer_map) {
			ppeds_idx_mapping.search_idx = vdev->bss_ast_idx;
			ppeds_idx_mapping.cache_set = vdev->bss_ast_hash;
		}

		hal_ppeds_cfg_ast_override_map_reg(soc->hal_soc,
						   vp_profile->search_idx_reg_num,
						   &ppeds_idx_mapping);
	}
}

/**
 * dp_ppeds_setup_vp_entry_be: Setup the PPE VP entry
 * be_soc: Be Soc
 * be_vdev: BE vdev
 *
 * Setup the PPE VP entry for HAL
 *
 * Return: void
 */
static void dp_ppeds_setup_vp_entry_be(struct dp_soc_be *be_soc,
					struct dp_vdev_be *be_vdev,
					struct dp_ppe_vp_profile *ppe_vp_profile)
{
	int ppe_vp_num_idx;
	union hal_tx_ppe_vp_config vp_cfg = {0};

	dp_ppeds_get_vdev_vp_config_be(be_vdev, &vp_cfg, ppe_vp_profile);

	ppe_vp_num_idx = ppe_vp_profile->ppe_vp_num_idx;

	hal_tx_populate_ppe_vp_entry(be_soc->soc.hal_soc,
				     &vp_cfg,
				     ppe_vp_num_idx);
}

/**
 * dp_tx_ppeds_vp_profile_update() - Update ppe vp profile with new bank data
 * @be_soc: BE Soc handle
 * @be_vdev: pointer to be_vdev structure
 *
 * The function updates the vp profile with new bank information
 *
 * Return: void
 */
void dp_tx_ppeds_vp_profile_update(struct dp_soc_be *be_soc,
				   struct dp_vdev_be *be_vdev)
{
	struct dp_ppe_vp_profile *ppe_vp_profile;
	int i;

	/* Iterate through ppe vp profile and update for VAPs with same vdev_id */
	for (i = 0; i < be_soc->num_ppe_vp_profiles; i++) {
		if (!be_soc->ppe_vp_profile[i].is_configured)
			continue;

		if (be_soc->ppe_vp_profile[i].vdev_id == be_vdev->vdev.vdev_id) {
			ppe_vp_profile = &be_soc->ppe_vp_profile[i];
			dp_ppeds_setup_vp_entry_be(be_soc, be_vdev, ppe_vp_profile);
		}
	}
}

/**
 * dp_ppeds_alloc_vp_tbl_entry_be() - PPE VP entry alloc
 * @be_soc: BE SoC
 * @be_vdev: BE VAP
 *
 * PPE VP table entry alloc
 *
 * Return: Return PPE VP index to be used
 */
static int dp_ppeds_alloc_vp_tbl_entry_be(struct dp_soc_be *be_soc,
					   struct dp_vdev_be *be_vdev)
{
	int num_ppe_vp_max, i;

	num_ppe_vp_max =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);
	if (be_soc->num_ppe_vp_entries == num_ppe_vp_max) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("Maximum ppe_vp count reached for vap");
		return -ENOSPC;
	}

	/*
	 * Increase the count in atomic context.
	 */
	be_soc->num_ppe_vp_entries++;

	for (i = 0; i < num_ppe_vp_max; i++) {
		if (!be_soc->ppe_vp_tbl[i].is_configured)
			break;
	}

	/*
	 * As we already found the current count not maxed out, we should
	 * always find a non configured entry.
	 */
	if (dp_assert_always_internal(!(i == num_ppe_vp_max))) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("Table entry index crosses max allowed");
		return -ENOSPC;
	}

	be_soc->ppe_vp_tbl[i].is_configured = true;
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
	return i;
}

/**
 * dp_ppeds_alloc_ppe_vp_profile_be() - PPE VP profile alloc
 * @be_soc: BE SoC
 * @ppe_vp_profile: ppe vp profile
 * @vp_num: vp number
 *
 * PPE VP profile alloc
 *
 * Return: Return PPE VP index to be used
 */
static int dp_ppeds_alloc_ppe_vp_profile_be(struct dp_soc_be *be_soc,
					    struct dp_ppe_vp_profile **ppe_vp_profile,
					    int vp_num)
{
	int num_ppe_vp_max, i;

	num_ppe_vp_max =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);

	for (i = 0; i < num_ppe_vp_max; i++) {
		if (be_soc->ppe_vp_profile[i].is_configured &&
			vp_num == be_soc->ppe_vp_profile[i].vp_num) {
			dp_info("vp profile with num %d is reused", vp_num);
			goto end;
		}
	}

	if (be_soc->num_ppe_vp_profiles == num_ppe_vp_max) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("Maximum ppe_vp count reached for soc");
		return -ENOSPC;
	}

	/*
	 * Increase the count in atomic context.
	 */
	be_soc->num_ppe_vp_profiles++;

	for (i = 0; i < num_ppe_vp_max; i++) {
		if (!be_soc->ppe_vp_profile[i].is_configured)
			break;
	}

	/*
	 * As we already found the current count not maxed out, we should
	 * always find an non configured entry.
	 */
	if (dp_assert_always_internal(!(i == num_ppe_vp_max))) {
		dp_err("Table entry index crosses max allowed");
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		return -ENOSPC;
	}

	be_soc->ppe_vp_profile[i].is_configured = true;
end:
	be_soc->ppe_vp_profile[i].ref_count++;
	*ppe_vp_profile = &be_soc->ppe_vp_profile[i];
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
	return i;
}

/**
 * dp_ppeds_dealloc_vp_tbl_entry_be() - PPE VP entry dealloc
 * @be_soc: BE SoC
 * @ppe_vp_num_idx: PPE VP index
 *
 * PPE VP table entry dealloc
 *
 * Return: void
 */
static void dp_ppeds_dealloc_vp_tbl_entry_be(struct dp_soc_be *be_soc,
					      int ppe_vp_num_idx)
{
	int num_ppe_vp_max;
	union hal_tx_ppe_vp_config vp_cfg = {0};

	num_ppe_vp_max =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	if (ppe_vp_num_idx < 0 || ppe_vp_num_idx >= num_ppe_vp_max) {
		dp_err("INvalid PPE VP free index");
		return;
	}

	/*
	 * Clear the HAL specific register for this PPE VP index.
	 */
	hal_tx_populate_ppe_vp_entry(be_soc->soc.hal_soc,
				     &vp_cfg,
				     ppe_vp_num_idx);

	/*
	 * Release the current index for reuse.
	 */
	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);
	if (!be_soc->ppe_vp_tbl[ppe_vp_num_idx].is_configured) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("PPE VP is not configured at idx:%d", ppe_vp_num_idx);
		return;
	}

	be_soc->ppe_vp_tbl[ppe_vp_num_idx].is_configured = false;
	be_soc->num_ppe_vp_entries--;
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
}

/**
 * dp_ppeds_dealloc_ppe_vp_profile_be() - PPE VP profile dealloc
 * @be_soc: BE SoC
 * @ppe_vp_profile_idx: PPE VP profile index
 * @vdev_opmode: vdev op mode
 *
 * PPE VP profile entry dealloc
 *
 * Return: void
 */
static void dp_ppeds_dealloc_ppe_vp_profile_be(struct dp_soc_be *be_soc,
					       int ppe_vp_profile_idx,
					       enum wlan_op_mode vdev_opmode)
{
	int num_ppe_vp_max;
	struct dp_ppe_vp_profile *vp_profile;

	num_ppe_vp_max =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	if (ppe_vp_profile_idx < 0 || ppe_vp_profile_idx >= num_ppe_vp_max) {
		dp_err("INvalid PPE VP profile free index");
		return;
	}

	/*
	 * Release the current index for reuse.
	 */
	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);
	if (!be_soc->ppe_vp_profile[ppe_vp_profile_idx].is_configured) {
		qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
		dp_err("PPE VP profile is not configured at idx:%d", ppe_vp_profile_idx);
		return;
	}

	be_soc->ppe_vp_profile[ppe_vp_profile_idx].ref_count--;

	if (!be_soc->ppe_vp_profile[ppe_vp_profile_idx].ref_count) {
		vp_profile = &be_soc->ppe_vp_profile[ppe_vp_profile_idx];

		be_soc->ppe_vp_profile[ppe_vp_profile_idx].is_configured = false;
		be_soc->num_ppe_vp_profiles--;

		dp_ppeds_dealloc_vp_tbl_entry_be(be_soc,
				vp_profile->ppe_vp_num_idx);
		/*
		 * For STA mode ast index table reg also needs to be cleaned
		 */
		if (vdev_opmode == wlan_op_mode_sta)
			dp_ppeds_dealloc_vp_search_idx_tbl_entry_be(be_soc, vp_profile->search_idx_reg_num);
	}
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
}

/**
 * dp_ppeds_tx_desc_free_nolock() - PPE DS tx desc free nolocks
 * @be_soc: SoC
 * @tx_desc: tx desc
 *
 * PPE DS tx desc free without locks
 *
 * Return: void
 */
static
void dp_ppeds_tx_desc_free_nolock(struct dp_soc *soc,
				  struct dp_tx_desc_s *tx_desc)
{
	dp_ppeds_tx_desc_pool_ctx *pool = dp_get_ppeds_tx_desc_pool(soc);
	tx_desc->nbuf = NULL;
	tx_desc->flags = 0;

	tx_desc->next = pool->freelist;
	pool->freelist = tx_desc;
	pool->num_allocated--;
	pool->num_free++;
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_ppeds_enable_txcomp_irq() - ppeds tx completion irq enable
 * @be_soc: soc handle
 *
 */
static void dp_ppeds_enable_txcomp_irq(struct dp_soc_be *be_soc)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);

	dp_ppeds_enable_irq(&be_soc->soc,
			    &be_soc->ppeds_wbm_release_ring);

	dp_umac_reset_trigger_pre_reset_notify_cb(soc);
}

static void dp_ppeds_tx_desc_reset(void *ctxt, void *elem, void *elem_list)
{
	struct dp_soc *soc = (struct dp_soc *)ctxt;
	struct dp_tx_desc_s *tx_desc = (struct dp_tx_desc_s *)elem;
	qdf_nbuf_t *nbuf_list = (qdf_nbuf_t *)elem_list;
	qdf_nbuf_t nbuf = NULL;

	if (tx_desc->nbuf) {
		nbuf = tx_desc->nbuf;
		__dp_tx_outstanding_dec(soc);
		dp_ppeds_tx_desc_free_nolock(soc, tx_desc);

		if (nbuf) {
			nbuf->next = *nbuf_list;
			*nbuf_list = nbuf;
		}
	}
}

/**
 * dp_ppeds_tx_desc_pool_reset() - PPE DS tx desc pool reset
 * @soc: SoC
 *
 * PPE DS tx desc pool reset
 *
 * Return: void
 */
void
dp_ppeds_tx_desc_pool_reset(struct dp_soc *soc,
			    qdf_nbuf_t *nbuf_list)
{
	dp_ppeds_tx_desc_pool_ctx *pool = dp_get_ppeds_tx_desc_pool(soc);

	if (pool) {
		TX_DESC_LOCK_LOCK(&pool->lock);

		qdf_tx_desc_pool_free_bufs(soc, &pool->desc_pages,
					   pool->elem_size, pool->elem_count,
					   true, &dp_ppeds_tx_desc_reset,
					   nbuf_list);

		TX_DESC_LOCK_UNLOCK(&pool->lock);
		pool->hotlist = NULL;
		pool->hot_list_len = 0;
	}
}
#else
static void dp_ppeds_enable_txcomp_irq(struct dp_soc_be *be_soc)
{
	dp_ppeds_enable_irq(&be_soc->soc,
			    &be_soc->ppeds_wbm_release_ring);
}
#endif

/**
 * dp_ppeds_tx_comp_poll() - ppeds tx completion napi poll
 * @napi: napi handle
 * @budget: Budget
 *
 * Ppeds tx completion napi poll funciton
 *
 * Return: Work done
 */
static int dp_ppeds_tx_comp_poll(struct napi_struct *napi, int budget)
{
	int work_done;
	int shift = DS_NAPI_SHIFT;
	int norm_budget = DS_NAPI_BUDGET_TO_INTERNAL_BUDGET(budget, shift);
	struct dp_soc_be *be_soc =
		qdf_container_of(napi, struct dp_soc_be, ppeds_napi_ctxt.napi);
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);

	qdf_atomic_set_bit(DP_PPEDS_TX_COMP_NAPI_BIT,
			   &soc->service_rings_running);
	work_done = dp_ppeds_tx_comp_handler(be_soc, norm_budget);
	work_done = DS_INTERNAL_BUDGET_TO_NAPI_BUDGET(work_done, shift);
	qdf_atomic_clear_bit(DP_PPEDS_TX_COMP_NAPI_BIT,
			     &soc->service_rings_running);

	if (budget > work_done) {
		napi_complete(napi);
		dp_ppeds_enable_txcomp_irq(be_soc);
	}

	return (work_done > budget) ? budget : work_done;
}

/**
 * dp_ppeds_del_napi_ctxt() - delete ppeds tx completion napi context
 * @be_soc: BE SoC
 *
 * Delete ppeds tx completion napi context
 *
 * Return: None
 */
static void dp_ppeds_del_napi_ctxt(struct dp_soc_be *be_soc)
{
	struct dp_ppeds_napi *napi_ctxt = &be_soc->ppeds_napi_ctxt;

	netif_napi_del(&napi_ctxt->napi);
}

/**
 * dp_ppeds_add_napi_ctxt() - Add ppeds tx completion napi context
 * @be_soc: BE SoC
 *
 * Add ppeds tx completion napi context
 *
 * Return: None
 */
static void dp_ppeds_add_napi_ctxt(struct dp_soc_be *be_soc)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);
	struct dp_ppeds_napi *napi_ctxt = &be_soc->ppeds_napi_ctxt;
	int napi_budget =
	wlan_cfg_get_dp_soc_ppeds_tx_comp_napi_budget(soc->wlan_cfg_ctx);

	qdf_net_if_create_dummy_if((struct qdf_net_if *)&napi_ctxt->ndev);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	netif_napi_add(&napi_ctxt->ndev, &napi_ctxt->napi,
		       dp_ppeds_tx_comp_poll, napi_budget);
#else
	netif_napi_add_weight(&napi_ctxt->ndev, &napi_ctxt->napi,
			dp_ppeds_tx_comp_poll, napi_budget);
#endif
}

#ifdef QCA_DP_OPTIMIZED_TX_DESC
/**
 * dp_ppeds_tx_desc_pool_comp_alloc() - PPE DS allocate memory for
 * Tx desc completion status
 * @tx_desc_pool: Tx desc pool handle
 * @quota: Number of buffers
 *
 * PPE DS allocate tx desc pool completion status
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
dp_ppeds_tx_desc_pool_comp_alloc(dp_ppeds_tx_desc_pool_ctx *tx_desc_pool,
				 uint16_t quota)
{
	if (!tx_desc_pool)
		return QDF_STATUS_NOT_INITIALIZED;

	tx_desc_pool->comp =
		qdf_mem_malloc(sizeof(struct hal_tx_desc_comp_s) * quota);

	if (!tx_desc_pool->comp)
		return QDF_STATUS_E_NOMEM;

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_tx_desc_pool_comp_free() - PPE DS free tx desc completion status
 * @tx_desc_pool: tx desc pool handle
 *
 * PPE DS free tx desc completion status
 *
 * Return: none
 */
static inline void
dp_ppeds_tx_desc_pool_comp_free(dp_ppeds_tx_desc_pool_ctx *tx_desc_pool)
{
	if (tx_desc_pool && tx_desc_pool->comp) {
		qdf_mem_free(tx_desc_pool->comp);
		tx_desc_pool->comp = NULL;
	}
}
#else
static inline QDF_STATUS
dp_ppeds_tx_desc_pool_comp_alloc(dp_ppeds_tx_desc_pool_ctx *tx_desc_pool,
				 uint16_t quota)
{
	return QDF_STATUS_SUCCESS;
}

static inline void
dp_ppeds_tx_desc_pool_comp_free(dp_ppeds_tx_desc_pool_ctx *tx_desc_pool)
{
}
#endif /* QCA_DP_OPTIMIZED_TX_DESC */

/**
 * dp_ppeds_tx_desc_pool_alloc() - PPE DS allocate tx desc pool
 * @soc: SoC
 * @num_elem: num of dec elements
 *
 * PPE DS allocate tx desc pool
 *
 * Return: status
 */
static QDF_STATUS
dp_ppeds_tx_desc_pool_alloc(struct dp_soc *soc, uint16_t num_elem)
{
	uint32_t desc_size;
	dp_ppeds_tx_desc_pool_ctx *tx_desc_pool;
	int napi_budget =
		wlan_cfg_get_dp_soc_ppeds_tx_comp_napi_budget(soc->wlan_cfg_ctx);
	uint16_t quota = 0;

	desc_size =  qdf_get_pwr2(sizeof(struct dp_tx_desc_s));
	tx_desc_pool =  dp_get_ppeds_tx_desc_pool(soc);
	dp_desc_multi_pages_mem_alloc(soc, QDF_DP_TX_PPEDS_DESC_TYPE,
				      &tx_desc_pool->desc_pages,
				      desc_size, num_elem,
				      0, true);

	if (!tx_desc_pool->desc_pages.num_pages) {
		dp_err("Multi page alloc fail, tx desc");
		return QDF_STATUS_E_NOMEM;
	}

	quota = DS_NAPI_BUDGET_TO_INTERNAL_BUDGET(napi_budget, DS_NAPI_SHIFT);
	if (QDF_STATUS_SUCCESS !=
	    dp_ppeds_tx_desc_pool_comp_alloc(tx_desc_pool, quota)) {
		dp_err("ppeds tx desc pool comp mem alloc failure");
		return QDF_STATUS_E_NOMEM;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_tx_desc_pool_free() - PPE DS free tx desc pool
 * @soc: SoC
 *
 * PPE DS free tx desc pool
 *
 * Return: none
 */
static void dp_ppeds_tx_desc_pool_free(struct dp_soc *soc)
{
	dp_ppeds_tx_desc_pool_ctx *tx_desc_pool;

	tx_desc_pool = dp_get_ppeds_tx_desc_pool(soc);
	dp_ppeds_tx_desc_pool_comp_free(tx_desc_pool);
	if (tx_desc_pool->desc_pages.num_pages)
		dp_desc_multi_pages_mem_free(soc, QDF_DP_TX_PPEDS_DESC_TYPE,
					     &tx_desc_pool->desc_pages, 0,
					     true);
}

/**
 * dp_ppeds_tx_desc_pool_setup() - PPE DS tx desc pool setup
 * @soc: SoC
 * @num_elem: num of dec elements
 * @pool_id: Pool id
 *
 * PPE DS tx desc pool setup
 *
 * Return: status
 */
static QDF_STATUS dp_ppeds_tx_desc_pool_setup(struct dp_soc *soc,
					      uint16_t num_elem,
					      uint8_t pool_id)
{
	dp_ppeds_tx_desc_pool_ctx *tx_desc_pool;
	struct dp_hw_cookie_conversion_t *cc_ctx;
	struct dp_spt_page_desc *page_desc;
	struct dp_tx_desc_s *tx_desc;
	uint32_t ppt_idx = 0;
	uint32_t avail_entry_index = 0;

	if (!num_elem) {
		dp_err("desc_num 0 !!");
		return QDF_STATUS_E_FAILURE;
	}

	tx_desc_pool = dp_get_ppeds_tx_desc_pool(soc);
	cc_ctx  = dp_get_ppeds_cookie_conv_ctx(soc);

	tx_desc = tx_desc_pool->freelist;
	page_desc = &cc_ctx->page_desc_base[0];
	while (tx_desc) {
		if (avail_entry_index == 0) {
			if (ppt_idx >= cc_ctx->total_page_num) {
				dp_alert("insufficient secondary page tables");
				qdf_assert_always(0);
			}
			page_desc = &cc_ctx->page_desc_base[ppt_idx++];
		}

		/* put each TX Desc VA to SPT pages and
		 * get corresponding ID
		 */
		DP_CC_SPT_PAGE_UPDATE_VA(page_desc->page_v_addr,
					 avail_entry_index,
					 tx_desc);
		tx_desc->id =
			dp_cc_desc_id_generate(page_desc->ppt_index,
					       avail_entry_index);
		tx_desc->pool_id = pool_id;
		dp_tx_desc_set_magic(tx_desc, DP_TX_MAGIC_PATTERN_FREE);
		tx_desc = tx_desc->next;
		avail_entry_index = (avail_entry_index + 1) &
					DP_CC_SPT_PAGE_MAX_ENTRIES_MASK;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_tx_desc_pool_init() - PPE DS tx desc pool init
 * @soc: SoC
 *
 * PPE DS tx desc pool init
 *
 * Return: status
 */
static QDF_STATUS dp_ppeds_tx_desc_pool_init(struct dp_soc *soc)
{
	dp_ppeds_tx_desc_pool_ctx *tx_desc_pool;
	uint32_t desc_size, num_elem;

	desc_size = qdf_get_pwr2(sizeof(struct dp_tx_desc_s));
	num_elem = wlan_cfg_get_dp_soc_ppeds_num_tx_desc(soc->wlan_cfg_ctx);
	tx_desc_pool = dp_get_ppeds_tx_desc_pool(soc);

	if (qdf_mem_multi_page_link(soc->osdev,
				    &tx_desc_pool->desc_pages,
				    desc_size, num_elem, true)) {
		dp_err("invalid tx desc allocation -overflow num link");
		return QDF_STATUS_E_FAULT;
	}

	tx_desc_pool->freelist = (struct dp_tx_desc_s *)
		*tx_desc_pool->desc_pages.cacheable_pages;

	tx_desc_pool->hotlist = NULL;
	/* Set unique IDs for each Tx descriptor */
	if (dp_ppeds_tx_desc_pool_setup(soc, num_elem, DP_TX_PPEDS_POOL_ID) !=
					QDF_STATUS_SUCCESS) {
		dp_err("ppeds_tx_desc_pool_setup failed");
		return QDF_STATUS_E_FAULT;
	}

	tx_desc_pool->elem_size = desc_size;
	tx_desc_pool->num_free = num_elem;
	tx_desc_pool->num_allocated = 0;
	tx_desc_pool->hot_list_len = 0;
	TX_DESC_LOCK_CREATE(&tx_desc_pool->lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_tx_desc_clean_up() -  Clean up the ppeds tx dexcriptors
 * @ctxt: context passed
 * @elem: element to be cleaned up
 * @elem_list: element list
 *
 */
static
void dp_ppeds_tx_desc_clean_up(void *ctxt, void *elem, void *elem_list)
{
	struct dp_soc *soc = (struct dp_soc *)ctxt;
	struct dp_tx_desc_s *tx_desc = (struct dp_tx_desc_s *)elem;

	if (tx_desc->nbuf) {
		qdf_nbuf_free(tx_desc->nbuf);
		__dp_tx_outstanding_dec(soc);
		dp_ppeds_tx_desc_free_nolock(soc, tx_desc);
	}
}

/**
 * dp_ppeds_tx_desc_pool_cleanup() - PPE DS tx desc pool cleanup
 * @soc: SoC
 * @tx_desc_pool: TX desc pool
 *
 * PPE DS tx desc pool cleanup
 *
 * Return: void
 */
static void
dp_ppeds_tx_desc_pool_cleanup(struct dp_soc_be *be_soc,
			      dp_ppeds_tx_desc_pool_ctx *tx_desc_pool)
{
	struct dp_spt_page_desc *page_desc;
	int i = 0;
	struct dp_hw_cookie_conversion_t *cc_ctx;
	dp_ppeds_tx_desc_pool_ctx *pool = dp_get_ppeds_tx_desc_pool(&be_soc->soc);

	if (pool) {
		TX_DESC_LOCK_LOCK(&pool->lock);

		qdf_tx_desc_pool_free_bufs(&be_soc->soc, &pool->desc_pages,
					   pool->elem_size, pool->elem_count,
					   true, &dp_ppeds_tx_desc_clean_up,
					   NULL);

		pool->freelist = NULL;
		pool->hotlist = NULL;

		TX_DESC_LOCK_UNLOCK(&pool->lock);
	}

	cc_ctx  = dp_get_ppeds_cookie_conv_ctx(&be_soc->soc);

	for (i = 0; i < cc_ctx->total_page_num; i++) {
		page_desc = &cc_ctx->page_desc_base[i];
		qdf_mem_zero(page_desc->page_v_addr, qdf_page_size);
	}
}

/**
 * dp_ppeds_tx_desc_pool_deinit() - PPE DS tx desc pool deinitialize
 * @be_soc: SoC
 *
 * PPE DS tx desc pool deinit
 *
 * Return: void
 */
static void dp_ppeds_tx_desc_pool_deinit(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	dp_ppeds_tx_desc_pool_ctx *tx_desc_pool = dp_get_ppeds_tx_desc_pool(soc);

	dp_ppeds_tx_desc_pool_cleanup(be_soc, tx_desc_pool);
	TX_DESC_POOL_MEMBER_CLEAN(tx_desc_pool);
	TX_DESC_LOCK_DESTROY(&tx_desc_pool->lock);
}

/**
 * dp_tx_borrow_tx_desc() - API to borrow tx descs from regular pool
 * @be_soc: SoC
 *
 * Borrow Tx descs from regular pool when DS Tx desc pool is empty
 *
 * Return: tx descriptor
 */
static inline
struct dp_tx_desc_s *dp_tx_borrow_tx_desc(struct dp_soc *soc)
{
	struct dp_tx_desc_s *tx_desc = dp_tx_desc_alloc(soc, qdf_get_cpu());

	if (tx_desc)
		tx_desc->flags = 0;

	return tx_desc;
}

/**
 * dp_ppeds_tx_desc_alloc() - PPE DS tx desc alloc
 * @be_soc: SoC
 *
 * PPE DS tx desc alloc
 *
 * Return: tx desc
 */
static inline
struct dp_tx_desc_s *dp_ppeds_tx_desc_alloc(struct dp_soc_be *be_soc)
{
	struct dp_tx_desc_s *tx_desc = NULL;
	dp_ppeds_tx_desc_pool_ctx *pool = be_soc->global_pool;

	TX_DESC_LOCK_LOCK(&pool->lock);

	if (pool->hotlist) {
		tx_desc = pool->hotlist;
		tx_desc->flags = 0;
		pool->hotlist = pool->hotlist->next;
		pool->hot_list_len--;
		if (pool->hot_list_len)
			dp_tx_prefetch_desc(pool->hotlist);
		else
			dp_tx_prefetch_desc(pool->freelist);
	} else {
		if (__dp_tx_limit_check(&be_soc->soc)) {
			TX_DESC_LOCK_UNLOCK(&pool->lock);
			goto failed;
		}

		tx_desc = pool->freelist;

		/* Pool is exhausted */
		if (!tx_desc) {
#ifndef QCA_SUPPORT_DP_GLOBAL_CTX
			if (qdf_atomic_read(&be_soc->borrow_count) <
					be_soc->borrow_limit) {
				/* Try to borrow from regular tx desc pools */
				tx_desc = dp_tx_borrow_tx_desc(&be_soc->soc);
				if (!tx_desc) {
					TX_DESC_LOCK_UNLOCK(&pool->lock);
					goto failed;
				}
				qdf_atomic_inc(&be_soc->borrow_count);
			} else {
#endif
				TX_DESC_LOCK_UNLOCK(&pool->lock);
				goto failed;
#ifndef QCA_SUPPORT_DP_GLOBAL_CTX
			}
#endif
		} else {
			tx_desc->flags = 0;
			pool->freelist = pool->freelist->next;
			dp_tx_prefetch_desc(pool->freelist);
			pool->num_allocated++;
			pool->num_free--;
		}
		__dp_tx_outstanding_inc(&be_soc->soc);
	}

	tx_desc->flags |= DP_TX_DESC_FLAG_ALLOCATED;
	tx_desc->flags |= DP_TX_DESC_FLAG_PPEDS;

	TX_DESC_LOCK_UNLOCK(&pool->lock);

	return tx_desc;
failed:
	be_soc->ppeds_stats.tx.desc_alloc_failed++;
	return NULL;
}

/**
 * dp_ppeds_tx_desc_free() - PPE DS tx desc free
 * @be_soc: SoC
 * @tx_desc: tx desc
 *
 * PPE DS tx desc free
 *
 * Return: nbuf to free
 */
qdf_nbuf_t dp_ppeds_tx_desc_free(struct dp_soc *soc, struct dp_tx_desc_s *tx_desc)
{
	dp_ppeds_tx_desc_pool_ctx *pool = NULL;
	qdf_nbuf_t nbuf = NULL;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

#ifndef QCA_SUPPORT_DP_GLOBAL_CTX
	if (tx_desc->pool_id != DP_TX_PPEDS_POOL_ID) {
		nbuf = tx_desc->nbuf;
		if (tx_desc->flags & DP_TX_DESC_FLAG_SPECIAL)
			dp_tx_spcl_desc_free(soc, tx_desc, tx_desc->pool_id);
		else
			dp_tx_desc_free(soc, tx_desc, tx_desc->pool_id);

		__dp_tx_outstanding_dec(soc);
		qdf_atomic_dec(&be_soc->borrow_count);

		return nbuf;
	}
#endif
	pool = be_soc->global_pool;
	TX_DESC_LOCK_LOCK(&pool->lock);
	if (pool->hot_list_len < be_soc->dp_ppeds_txdesc_hotlist_len &&
			tx_desc->nbuf) {
		tx_desc->next = pool->hotlist;
		pool->hotlist = tx_desc;
		pool->hot_list_len++;
	} else {
		if (tx_desc->nbuf)
			__dp_tx_outstanding_dec(soc);
		nbuf = tx_desc->nbuf;
		tx_desc->nbuf = NULL;
		tx_desc->flags = 0;
		tx_desc->next = pool->freelist;

		pool->freelist = tx_desc;
		pool->num_allocated--;
		pool->num_free++;
	}
	TX_DESC_LOCK_UNLOCK(&pool->lock);

	return nbuf;
}

/**
 * dp_ppeds_get_batched_tx_desc() - Get Tx descriptors in a buffer array
 * @ppeds_handle: PPE-DS WLAN handle
 * @arr: Tx buffer array to be filled
 * @num_buff_req: Number of buffer to be allocated
 * @buff_size: Size of the buffer to be allocated
 * @headroom: Headroom required in the allocated buffer
 *
 * PPE-DS get buffer and the Tx descriptors in an array
 *
 * Return: Number of buffers allocated
 */
static
uint32_t dp_ppeds_get_batched_tx_desc(ppe_ds_wlan_handle_t *ppeds_handle,
				      struct ppe_ds_wlan_txdesc_elem *arr,
				      uint32_t num_buff_req,
				      uint32_t buff_size,
				      uint32_t headroom)
{
	uint32_t i = 0;
	struct dp_tx_desc_s *tx_desc;
	struct dp_soc *soc = *((struct dp_soc **)ppe_ds_wlan_priv(ppeds_handle));
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	qdf_dma_addr_t paddr;

	for (i = 0; i < num_buff_req; i++) {
		qdf_nbuf_t nbuf = NULL;

		tx_desc = dp_ppeds_tx_desc_alloc(be_soc);
		if (!tx_desc) {
			dp_err("ran out of txdesc");
			DP_DSB;
			break;
		}

		if (!tx_desc->nbuf) {
			/*
			 * Get a free skb.
			 */
			nbuf = dp_nbuf_alloc_ppe_ds(soc->osdev, buff_size, 0, 0, 0);
			if (qdf_unlikely(!nbuf)) {
				dp_ppeds_tx_desc_free(soc, tx_desc);
				break;
			}

			/*
			 * Reserve headrrom
			 */
			qdf_nbuf_reserve(nbuf, headroom);

			/*
			 * Map(Get Phys address)
			 */
			if (!nbuf->recycled_for_ds) {
				qdf_nbuf_dma_inv_range_no_dsb((void *)nbuf->data,
						(void *)(nbuf->data + buff_size - headroom));
				nbuf->recycled_for_ds = 1;
			}
			paddr = (qdf_dma_addr_t)qdf_mem_virt_to_phys(nbuf->data);

			tx_desc->pdev = NULL;
			tx_desc->nbuf = nbuf;
			tx_desc->dma_addr = paddr;
		}

		arr[i].opaque_lo = tx_desc->id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = tx_desc->dma_addr;
	}

	DP_DSB;

	return i;
}

/**
 * dp_ppeds_release_rx_desc() - Release the Rx descriptors from buffer array
 * @ppeds_handle: PPE-DS WLAN handle
 * @arr: Rx buffer array containing Rx descriptors
 * @count: Number of Rx descriptors to be freed
 *
 * Return: void
 */
static void dp_ppeds_release_rx_desc(ppe_ds_wlan_handle_t *ppeds_handle,
					struct ppe_ds_wlan_rxdesc_elem *arr, uint16_t count)
{
	struct dp_rx_desc *rx_desc;
	struct dp_soc *soc = *((struct dp_soc **)ppe_ds_wlan_priv(ppeds_handle));
	union dp_rx_desc_list_elem_t *head[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT] = {0};
	union dp_rx_desc_list_elem_t *tail[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT] = {0};
	uint32_t rx_bufs_reaped[WLAN_MAX_MLO_CHIPS][MAX_PDEV_CNT] = {0};
	struct dp_srng *dp_rxdma_srng;
	struct rx_desc_pool *rx_desc_pool;
	struct dp_soc *replenish_soc;
	uint32_t i, mac_id;
	uint8_t chip_id;

	for (i = 0; i < count; i++) {
		if ((i + 1) != count)
			qdf_prefetch((struct dp_rx_desc *)arr[i + 1].cookie);

		rx_desc = dp_rx_desc_ppeds_cookie_2_va(soc, arr[i].cookie);
		if (rx_desc == NULL) {
			dp_err("Rx descriptor is NULL\n");
			continue;
		}

		rx_bufs_reaped[rx_desc->chip_id][rx_desc->pool_id]++;

		dp_rx_add_to_free_desc_list_reuse(&head[rx_desc->chip_id][rx_desc->pool_id],
						  &tail[rx_desc->chip_id][rx_desc->pool_id],
						  rx_desc);

		if ((i + 2) != count)
			qdf_prefetch((struct dp_rx_desc *)arr[i + 2].cookie);
	}

	for (chip_id = 0; chip_id < WLAN_MAX_MLO_CHIPS; chip_id++) {
		for (mac_id = 0; mac_id < MAX_PDEV_CNT; mac_id++) {

			if (!rx_bufs_reaped[chip_id][mac_id])
				continue;

			replenish_soc = dp_rx_replenish_soc_get(soc, chip_id);

			dp_rxdma_srng = &replenish_soc->rx_refill_buf_ring[mac_id];
			rx_desc_pool = &replenish_soc->rx_desc_buf[mac_id];

			dp_rx_comp2refill_replenish(replenish_soc, mac_id,
						    dp_rxdma_srng, rx_desc_pool,
						    rx_bufs_reaped[chip_id][mac_id],
						    &head[chip_id][mac_id],
						    &tail[chip_id][mac_id]);
		}
	}
}

/**
 * dp_ppeds_release_tx_desc_single() - Release the Tx descriptor
 * @ppeds_handle: PPE-DS WLAN handle
 * @cookie: Cookie to get the descriptor to be freed
 *
 * PPE-DS release single Tx descriptor API
 *
 * Return: void
 */
static
void dp_ppeds_release_tx_desc_single(ppe_ds_wlan_handle_t *ppeds_handle,
				     uint32_t cookie)
{
	return;
}

/**
 * dp_ppeds_set_tcl_prod_idx() - Set produce index of the PPE2TCL ring
 * @ppeds_handle: PPE-DS WLAN handle
 * @tcl_prod_idx: Producer index to be set
 *
 * Set producer index of the PPE2TCL ring
 *
 * Return: void
 */
static void dp_ppeds_set_tcl_prod_idx(ppe_ds_wlan_handle_t *ppeds_handle,
				      uint16_t tcl_prod_idx)
{
	struct dp_soc_be *soc =
			*((struct dp_soc_be **)ppe_ds_wlan_priv(ppeds_handle));
	struct dp_soc *dpsoc = (struct dp_soc *)soc;
	struct dp_srng *ppe2tcl_ring = &soc->ppe2tcl_ring;

	hal_srng_src_set_hp(ppe2tcl_ring->hal_srng, tcl_prod_idx);
	hal_update_ring_util(dpsoc->hal_soc, ppe2tcl_ring->hal_srng,
			     PPE2TCL, &soc->ppe2tcl_ring.stats);

	dp_tx_ring_access_end_wrapper(dpsoc, ppe2tcl_ring->hal_srng, 0);
}

/**
 * dp_ppeds_get_tcl_cons_idx() - Get consumer index of the PPE2TCL ring
 * @ppeds_handle: PPE-DS WLAN handle
 *
 * Get consumer index of the PPE2TCL ring
 *
 * Return: Consumer index of the PPE2TCL ring
 */
static uint16_t dp_ppeds_get_tcl_cons_idx(ppe_ds_wlan_handle_t *ppeds_handle)
{
	struct dp_soc_be *soc =
			*((struct dp_soc_be **)ppe_ds_wlan_priv(ppeds_handle));
	struct dp_srng *ppe2tcl_ring = &soc->ppe2tcl_ring;

	return hal_srng_src_get_tpidx(ppe2tcl_ring->hal_srng);
}

/**
 * dp_ppeds_set_reo_cons_idx() - Set consumer index of the REO2PPE ring
 * @ppeds_handle: PPE-DS WLAN handle
 * @tcl_prod_idx: Producer index to be set
 *
 * Set the consumer index of the PPE2TCL ring
 *
 * Return: void
 */
static void dp_ppeds_set_reo_cons_idx(ppe_ds_wlan_handle_t *ppeds_handle,
				      uint16_t reo_cons_idx)
{
	struct dp_soc_be *soc =
			*((struct dp_soc_be **)ppe_ds_wlan_priv(ppeds_handle));
	struct dp_soc *dpsoc = (struct dp_soc *)soc;
	struct dp_srng *reo2ppe_ring = &soc->reo2ppe_ring;

	hal_srng_dst_set_tp(reo2ppe_ring->hal_srng, reo_cons_idx);
	hal_srng_access_end_unlocked(dpsoc->hal_soc, reo2ppe_ring->hal_srng);
}

/**
 * dp_ppeds_get_reo_prod_idx() - Get producer index of the REO2PPE ring
 * @ppeds_handle: PPE-DS WLAN handle
 *
 * Get producer index of the REO2PPE ring
 *
 * Return: Producer index of the REO2PPE ring
 */
static uint16_t dp_ppeds_get_reo_prod_idx(ppe_ds_wlan_handle_t *ppeds_handle)
{
	struct dp_soc_be *soc =
			*((struct dp_soc_be **)ppe_ds_wlan_priv(ppeds_handle));
	struct dp_srng *reo2ppe_ring = &soc->reo2ppe_ring;

	return hal_srng_dst_get_hpidx(reo2ppe_ring->hal_srng);
}

/**
 * dp_ppeds_toggle_srng_intr() - Enable/Disable irq of ppe2tcl ring
 * @ppeds_handle: PPE-DS WLAN handle
 * @enable: enable/disable bit
 *
 * Return: None
 */
static inline void
dp_ppeds_enable_srng_intr(ppe_ds_wlan_handle_t *ppeds_handle, bool enable)
{
	struct dp_soc *soc =
			*((struct dp_soc **)ppe_ds_wlan_priv(ppeds_handle));
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (enable)
		dp_ppeds_enable_irq(&be_soc->soc, &be_soc->ppe2tcl_ring);
	else
		dp_ppeds_disable_irq(&be_soc->soc, &be_soc->ppe2tcl_ring);
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_ppeds_notify_napi_done() - Notify calback about NAPI complete.
 * @ppeds_handle: PPE-DS WLAN handle
 *
 * Return: None
 */
static inline
void dp_ppeds_notify_napi_done(ppe_ds_wlan_handle_t *ppeds_handle)
{
	struct dp_soc *soc =
			*((struct dp_soc **)ppe_ds_wlan_priv(ppeds_handle));

	qdf_atomic_clear_bit(DP_PPEDS_NAPI_DONE_BIT,
			     &soc->service_rings_running);

	if (soc)
		dp_umac_reset_trigger_pre_reset_notify_cb(soc);
}
#endif

/**
 * ppeds_ops
 *	PPE-DS WLAN operations
 */
static struct ppe_ds_wlan_ops ppeds_ops = {
	.get_tx_desc_many = dp_ppeds_get_batched_tx_desc,
	.release_tx_desc_single = dp_ppeds_release_tx_desc_single,
	.enable_tx_consume_intr = dp_ppeds_enable_srng_intr,
	.set_tcl_prod_idx  = dp_ppeds_set_tcl_prod_idx,
	.set_reo_cons_idx = dp_ppeds_set_reo_cons_idx,
	.get_tcl_cons_idx = dp_ppeds_get_tcl_cons_idx,
	.get_reo_prod_idx = dp_ppeds_get_reo_prod_idx,
	.release_rx_desc = dp_ppeds_release_rx_desc,
#ifdef DP_UMAC_HW_RESET_SUPPORT
	.notify_napi_done = dp_ppeds_notify_napi_done,
#endif
};

uint32_t dp_ppeds_get_node_id_be(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->ppeds_handle) {
		dp_err("%p: DS is not enabled on this SOC", soc);
		return PPE_VP_DS_INVALID_NODE_ID;
	}
	return be_soc->dp_ppeds_node_id;
}

bool dp_ppeds_process_mpsk_exception(struct cdp_soc_t *soc_hdl,
				   qdf_nbuf_t nbuf) {
	struct dp_peer *dp_peer = NULL;
	struct dp_txrx_peer *txrx_peer = NULL;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	uint16_t vlan_id = 0;
	uint8_t tid = 0;
	bool ret = false;
	qdf_ether_header_t *eh;

	eh = (qdf_ether_header_t *)qdf_nbuf_data(nbuf);

	dp_peer = dp_find_peer_by_macaddr(soc, eh->ether_shost, DP_VDEV_ALL, DP_MOD_ID_DS);
	if (!dp_peer) {
		dp_err("%pK: No peer found for MPSK", soc);
		return false;
	}

	txrx_peer = dp_get_txrx_peer(dp_peer);
	if (!txrx_peer) {
		dp_err("%pK: No txrx peer found for dp peer", dp_peer);
		dp_peer_unref_delete(dp_peer, DP_MOD_ID_DS);
		return false;
	}

	vlan_id = txrx_peer->vlan_id;

	if (!vlan_id) {
		dp_info("%pK: This is a classic client for MPSK", dp_peer);
		dp_peer_unref_delete(dp_peer, DP_MOD_ID_DS);
		return true;
	}

	/*
	 * Add a VLAN header in the packet.
	 */
	tid = qdf_nbuf_get_tid_val(nbuf);
	ret = dp_rx_multipass_add_vlan(txrx_peer, nbuf, tid);

	dp_peer_unref_delete(dp_peer, DP_MOD_ID_DS);

	return ret;
}

void dp_ppeds_stats_sync_be(struct cdp_soc_t *soc_hdl,
			    uint16_t vdev_id,
			    struct cdp_ds_vp_params *vp_params,
			    void *stats) {
	struct dp_peer *dp_peer = NULL;
	struct dp_txrx_peer *txrx_peer;
	struct dp_vdev *vdev;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	ppe_vp_hw_stats_t *vp_stats = (ppe_vp_hw_stats_t *)stats;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_DS);
	if (!vdev) {
		dp_err("%pK: No vdev found for soc, vdev id %d", soc, vdev_id);
		return;
	}

	/*
	 * In case of wds ext mode, get the corresponding peer
	 * and update the stats in the same.
	 */
	if (vp_params->wds_ext_mode) {
		dp_peer = dp_peer_get_ref_by_id(soc, vp_params->peer_id, DP_MOD_ID_DS);
		if (!dp_peer) {
			dp_err("%pK: No peer found for WDS ext", vdev);
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_DS);
			return;
		}

		txrx_peer = dp_get_txrx_peer(dp_peer);
		if (!txrx_peer) {
			dp_err("%pK: No txrx peer found for dp peer", dp_peer);
			dp_peer_unref_delete(dp_peer, DP_MOD_ID_DS);
			dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_DS);
			return;
		}

		DP_PEER_STATS_FLAT_INC_PKT(txrx_peer, to_stack,
					   vp_stats->rx_pkt_cnt,
					   vp_stats->rx_byte_cnt);
		DP_PEER_PER_PKT_STATS_INC_PKT(txrx_peer, rx.ppeds_drop,
					      vp_stats->rx_drop_pkt_cnt,
					      vp_stats->rx_drop_byte_cnt, 0);

		dp_peer_unref_delete(dp_peer, DP_MOD_ID_DS);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_DS);
		return;
	}

	/*
	 * In case of non wds ext, update the stats in
	 * vdev structure.
	 */
	DP_STATS_INC_PKT(vdev, rx.to_stack, vp_stats->rx_pkt_cnt,
			 vp_stats->rx_byte_cnt);
	DP_STATS_INC_PKT(vdev, rx.ppeds_drop, vp_stats->rx_drop_pkt_cnt,
			 vp_stats->rx_drop_byte_cnt);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_DS);
	return;
}

/**
 * dp_ppeds_vp_setup_on_fw_recovery() - Recover DS VP on FW recovery
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @vdev_id: vdev id
 * @vp_profile_idx: VP profile index
 *
 * Setup DS VP port on FW recovery.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ppeds_vp_setup_on_fw_recovery(struct cdp_soc_t *soc_hdl,
					    uint8_t vdev_id,
					    uint16_t vp_profile_idx)
{
	struct dp_ppe_vp_profile *vp_profile = NULL;
	struct dp_vdev *vdev;
	struct dp_vdev_be *be_vdev;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;

	be_soc = dp_get_be_soc_from_dp_soc(soc);

	/*
	 * Check DS handle
	 */
	if (!be_soc->ppeds_handle) {
		dp_err("DS is not enabled on this SOC");
		return QDF_STATUS_E_INVAL;
	}

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_DS);
	if (!vdev) {
		dp_err("%p: Could not find vdev for id:%d", be_soc, vdev_id);
		return QDF_STATUS_E_INVAL;
	}

	vp_profile = &be_soc->ppe_vp_profile[vp_profile_idx];

	be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	dp_ppeds_setup_vp_entry_be(be_soc, be_vdev, vp_profile);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_DS);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_entry_alloc_vdev_be() - allocate the PPE DS VP entry
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @vp_arg: PPE VP opaque
 * @ppe_vp_num: Allocated PPE VP number
 * @vp_params: PPE VP ds related params
 *
 * Allocate a DS VP port
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ppeds_entry_alloc_vdev_be(struct cdp_soc_t *soc_hdl,
					void *vp_arg, int32_t *ppe_vp_num,
					struct cdp_ds_vp_params *vp_params)
{
	struct net_device *dev = vp_params->dev;
	struct dp_soc *soc = NULL;
	struct dp_soc_be *be_soc = NULL;
	int16_t vp_num;
	QDF_STATUS ret;

	soc = cdp_soc_t_to_dp_soc(soc_hdl);
	be_soc = dp_get_be_soc_from_dp_soc(soc);

	/*
	 * Enable once PPEDS module is ready
	 */
	if (!be_soc->ppeds_handle) {
		dp_err("%p: DS is not enabled on this SOC", soc);
		ret = QDF_STATUS_E_INVAL;
		goto fail;
	}

	vp_num = ppe_ds_wlan_vp_alloc(be_soc->ppeds_handle, dev, vp_arg);
	if (vp_num < 0) {
		dp_err("vp alloc failed dev %s\n", dev->name);
		ret = QDF_STATUS_E_FAULT;
		goto fail;
	}

	*ppe_vp_num = vp_num;

	return QDF_STATUS_SUCCESS;

fail:
	return ret;
}

static inline void dp_ppeds_set_splitphy_bank_id(struct dp_vdev_be *be_vdev,
						 struct dp_vdev *ptnr_vdev,
						 void *arg)
{
	struct dp_vdev_be *ptnr_vdev_be = dp_get_be_vdev_from_dp_vdev(ptnr_vdev);
	struct dp_soc_be *ptnr_be_soc = dp_get_be_soc_from_dp_soc(ptnr_vdev->pdev->soc);

	if (be_vdev->vdev.pdev->soc == ptnr_vdev->pdev->soc) {
		ptnr_vdev_be->splitphy_ds_bank_id =
				dp_tx_get_bank_profile(ptnr_be_soc, ptnr_vdev_be, false);
		/* update the partner vp profile */
		dp_tx_ppeds_vp_profile_update(ptnr_be_soc, ptnr_vdev_be);
	}
}
/**
 * dp_ppeds_attach_vdev_be() - PPE DS table entry alloc
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @vdev_id: vdev id
 * @vp_arg: PPE VP opaque
 * @vp_num: PPE VP number
 * @vp_params: PPE VP ds related params
 *
 * attach DS VP port
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ppeds_attach_vdev_be(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				   void *vp_arg, int32_t vp_num, struct cdp_ds_vp_params *vp_params)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;
	struct dp_vdev *vdev;
	struct dp_vdev_be *be_vdev;
	struct dp_peer *dp_peer, *mld_peer, *tgt_peer = NULL;
	struct dp_ppe_vp_profile *vp_profile = NULL;
	uint32_t peer_id;
	int8_t ppe_vp_idx;
	int8_t ppe_vp_profile_idx;
	int16_t ppe_vp_search_tbl_idx = -1;
	bool wds_ext_mode = vp_params->wds_ext_mode;
	QDF_STATUS ret;

	be_soc = dp_get_be_soc_from_dp_soc(soc);

	/*
	 * Enable once PPEDS module is ready
	 */
	if (!be_soc->ppeds_handle) {
		dp_err("DS is not enabled on this SOC");
		ret = QDF_STATUS_E_INVAL;
		goto fail0;
	}

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev) {
		dp_err("%p: Could not find vdev for id:%d", be_soc, vdev_id);
		ret = QDF_STATUS_E_INVAL;
		goto fail0;
	}

	/*
	 * Check if PPEDS could be enabled for Vdev
	 */
	if (vdev->wrap_vdev || vdev->nawds_enabled ||
	    vdev->opmode == wlan_op_mode_monitor ||
	    vdev->mesh_vdev || vdev->mesh_rx_filter) {
		dp_err("Unsupported Vdev config for id:%d", vdev_id);
		ret = QDF_STATUS_E_NOSUPPORT;
		goto fail1;
	}

	/*
	 * Allocate a PPE-VP profile for a vap / wds_ext node.
	 */
	ppe_vp_profile_idx = dp_ppeds_alloc_ppe_vp_profile_be(be_soc, &vp_profile, vp_num);
	if (!vp_profile) {
		dp_err("%p: Failed to allocate VP profile for VP :%d", be_soc, vp_num);
		ret = QDF_STATUS_E_RESOURCES;
		goto fail1;
	}

	be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);

	if (vp_profile->ref_count == 1) {
		ppe_vp_idx = dp_ppeds_alloc_vp_tbl_entry_be(be_soc, be_vdev);
		if (ppe_vp_idx < 0) {
			dp_err("%p: Failed to allocate PPE VP idx for vdev_id:%d", be_soc, vdev->vdev_id);
			ret = QDF_STATUS_E_RESOURCES;
			goto fail2;
		}

		/*
		 * For STA mode AST override is used; So a index register table would be needed
		 */
		if (vdev->opmode == wlan_op_mode_sta) {
			ppe_vp_search_tbl_idx = dp_ppeds_alloc_vp_search_idx_tbl_entry_be(be_soc, be_vdev);
			if (ppe_vp_search_tbl_idx < 0) {
				dp_err("%p: Failed to allocate PPE VP search table idx for vdev_id:%d",
						be_soc, vdev->vdev_id);
				ret = QDF_STATUS_E_RESOURCES;
				goto fail2;
			}

			vp_profile->search_idx_reg_num = ppe_vp_search_tbl_idx;
		}

		vp_profile->vp_num = vp_num;
		vp_profile->ppe_vp_num_idx = ppe_vp_idx;
		vp_profile->to_fw = 0;
		vp_profile->use_ppe_int_pri = 0;
		vp_profile->drop_prec_enable = 0;
		vp_profile->vdev_id = vdev_id;
	} else {
		be_vdev->splitphy_ds_bank_id =
				dp_tx_get_bank_profile(be_soc, be_vdev, false);
		dp_mlo_iter_ptnr_vdev(be_soc, be_vdev,
				dp_ppeds_set_splitphy_bank_id,
				NULL,
				DP_MOD_ID_DS,
				DP_LINK_VDEV_ITER,
				0);
	}

	/*
	 * For the sta mode fill up the index reg number.
	 */
	dp_ppeds_setup_vp_entry_be(be_soc, be_vdev, vp_profile);

	if (wds_ext_mode) {
		peer_id = vp_params->peer_id;
		dp_peer = dp_peer_get_ref_by_id(soc, peer_id, DP_MOD_ID_CDP);
		if (!dp_peer) {
			dp_err("%p: No peer found for WDS ext\n", vdev);
			ret = QDF_STATUS_E_INVAL;
			goto fail4;
		}

		tgt_peer = dp_peer;

		/*
		 * In case of WDS EXT MLO case, dp_peer is the primary
		 * link peer, hence obtain the corresponding MLD peer
		 * and set the newly created VP number as a src_info
		 * for all its link peers.
		 */
		if (!IS_DP_LEGACY_PEER(dp_peer)) {
			mld_peer = DP_GET_MLD_PEER_FROM_PEER(dp_peer);
			if (!mld_peer) {
				dp_err("%p: No MLD peer for WDS ext\n", vdev);
				ret = QDF_STATUS_E_INVAL;
				goto fail4;
			}

			tgt_peer = mld_peer;
		}

		if (dp_peer_setup_ppeds_be(soc, tgt_peer, be_vdev,
					   (void *)vp_profile) != QDF_STATUS_SUCCESS) {
			ret = QDF_STATUS_E_RESOURCES;
			goto fail4;
		}

		dp_peer_unref_delete(dp_peer, DP_MOD_ID_CDP);
	}

	vp_params->ppe_vp_profile_idx = ppe_vp_profile_idx;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);

	return QDF_STATUS_SUCCESS;

fail4:
	if (dp_peer)
		dp_peer_unref_delete(dp_peer, DP_MOD_ID_CDP);

fail2:
	dp_ppeds_dealloc_ppe_vp_profile_be(be_soc, ppe_vp_profile_idx,
					   vdev->opmode);
fail1:
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
fail0:
	return ret;
}

/**
 * dp_ppeds_entry_free_vdev_be() - Free the PPE DS port
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @vp_num: vp number
 *
 * Free the PPE DS port
 *
 * Return: void
 */
void dp_ppeds_entry_free_vdev_be(struct cdp_soc_t *soc_hdl, int32_t vp_num)
{

	struct dp_soc *soc;
	struct dp_soc_be *be_soc;

	soc = cdp_soc_t_to_dp_soc(soc_hdl);
	if (!soc) {
		dp_err("CDP soch handle is NULL");
		return;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);

	ppe_ds_wlan_vp_free(be_soc->ppeds_handle, vp_num);
}

/**
 * dp_ppeds_detach_vdev_be() - Detach the PPE DS port from vdev
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @vdev_id: vdev id
 *
 * Detach the PPE DS port from BE VAP
 *
 * Return: void
 */
void dp_ppeds_detach_vdev_be(struct cdp_soc_t *soc_hdl, uint8_t vdev_id, struct cdp_ds_vp_params *vp_params)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;
	struct dp_ppe_vp_profile *vp_profile;
	struct dp_vdev *vdev;
	struct dp_vdev_be *be_vdev;
	int16_t ppe_vp_profile_idx = -1;

	if (!soc) {
		dp_err("CDP soch handle is NULL");
		return;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev) {
		dp_err("%p: Could not find vdev for id:%d", be_soc, vdev_id);
		return;
	}

	/*
	 * Extract the VP profile from the BE vap.
	 */
	be_vdev = dp_get_be_vdev_from_dp_vdev(vdev);
	ppe_vp_profile_idx = vp_params->ppe_vp_profile_idx;

	vp_profile = &be_soc->ppe_vp_profile[ppe_vp_profile_idx];
	if (!vp_profile->is_configured) {
		dp_err("%p: Invalid PPE VP profile for vdev_id:%d",
		       be_vdev, vdev_id);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return;
	}

	dp_ppeds_dealloc_ppe_vp_profile_be(be_soc, ppe_vp_profile_idx,
					   vdev->opmode);

	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
}

/**
 * dp_ppeds_detach_vp_profile() - detach ppe vp profile during vdev detach
 * @be_soc: BE Soc handle
 * @be_vdev: pointer to be_vdev structure
 *
 * The function detach the the vp profile during vdev detach
 *
 * Return: void
 */
void dp_ppeds_detach_vp_profile(struct dp_soc_be *be_soc,
				struct dp_vdev_be *be_vdev)
{
	struct dp_ppe_vp_profile *ppe_vp_profile;
	int num_ppe_vp_max, i;

	if (!be_soc->ppeds_handle) {
		dp_err("DS is not enabled on this SOC");
		return;
	}

	num_ppe_vp_max =
		hal_tx_get_num_ppe_vp_tbl_entries(be_soc->soc.hal_soc);

	/* Iterate through ppe vp profile and
	 * update for VAPs with same vdev_id
	 */
	for (i = 0; i < num_ppe_vp_max; i++) {
		/* none of the profiles are configured */
		if (!be_soc->num_ppe_vp_profiles)
			break;

		if (!be_soc->ppe_vp_profile[i].is_configured)
			continue;

		if (be_soc->ppe_vp_profile[i].vdev_id ==
							be_vdev->vdev.vdev_id) {
			ppe_vp_profile = &be_soc->ppe_vp_profile[i];

			ppe_ds_wlan_vp_free(be_soc->ppeds_handle,
					    ppe_vp_profile->vp_num);

			dp_ppeds_dealloc_ppe_vp_profile_be(be_soc, i,
							be_vdev->vdev.opmode);
		}
	}
}

/*
 * dp_ppeds_set_int_pri2tid_be() - Set up INT_PRI to TID
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @pri2tid: Priority to TID table
 *
 * Setup the INT_PRI to TID table
 *
 * Return: void
 */
void dp_ppeds_set_int_pri2tid_be(struct cdp_soc_t *soc_hdl,
				  uint8_t *pri2tid)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;
	union hal_tx_ppe_pri2tid_map0_config map0 = {0};
	union hal_tx_ppe_pri2tid_map1_config map1 = {0};
	uint32_t value = 0;

	if (!soc) {
		dp_err("CDP soch handle is NULL");
		return;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);

	map0.int_pri0 = pri2tid[0];
	map0.int_pri1 = pri2tid[1];
	map0.int_pri2 = pri2tid[2];
	map0.int_pri3 = pri2tid[3];
	map0.int_pri4 = pri2tid[4];
	map0.int_pri5 = pri2tid[5];
	map0.int_pri6 = pri2tid[6];
	map0.int_pri7 = pri2tid[7];
	map0.int_pri8 = pri2tid[8];
	map0.int_pri9 = pri2tid[9];

	value = map0.val;

	hal_tx_set_int_pri2tid(be_soc->soc.hal_soc, value, 0);
	map1.int_pri10 = pri2tid[10];
	map1.int_pri11 = pri2tid[11];
	map1.int_pri12 = pri2tid[12];
	map1.int_pri13 = pri2tid[13];
	map1.int_pri14 = pri2tid[14];
	map1.int_pri15 = pri2tid[15];

	value = map1.val;

	hal_tx_set_int_pri2tid(be_soc->soc.hal_soc, value, 1);
}

/**
 * dp_ppeds_update_int_pri2tid_be() - Update INT_PRI to TID
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @pri: Priority value
 * @tid: TID mapped to the priority value
 *
 * Update the tid for a specific INT_PRI
 *
 * Return: void
 */
void dp_ppeds_update_int_pri2tid_be(struct cdp_soc_t *soc_hdl,
				     uint8_t pri, uint8_t tid)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;

	if (!soc) {
		dp_err("CDP soch handle is NULL");
		return;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	hal_tx_update_int_pri2tid(be_soc->soc.hal_soc, pri, tid);
}

/**
 * dp_ppeds_dump_ppe_vp_tbl_be() - Dump the PPE VP entries
 * @soc_hdl: CDP SoC Tx/Rx handle
 *
 * Dump the PPE VP entries of a SoC
 *
 * Return: void
 */
void dp_ppeds_dump_ppe_vp_tbl_be(struct cdp_soc_t *soc_hdl)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;

	if (!soc) {
		dp_err("CDP soch handle is NULL");
		return;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);

	qdf_mutex_acquire(&be_soc->ppe_vp_tbl_lock);
	hal_tx_dump_ppe_vp_entry(be_soc->soc.hal_soc);
	qdf_mutex_release(&be_soc->ppe_vp_tbl_lock);
}

/**
 * dp_ppeds_vdev_enable_pri2tid_be() - Enable PPE PRI2TID conversion for a vap
 * @soc_hdl: CDP SoC Tx/Rx handle
 * @vdev_id : vdev id
 * @val: Boolean to enable/disable PRI2TID mapping
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ppeds_vdev_enable_pri2tid_be(struct cdp_soc_t *soc_hdl,
				       uint8_t vdev_id, bool val)
{
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_soc_be *be_soc;
	struct dp_vdev *vdev;
	struct dp_ppe_vp_profile *vp_profile;
	struct cdp_ds_vp_params vp_params = {0};
	int8_t ppe_vp_idx;

	if (!soc) {
		dp_err("CDP soc handle is NULL");
		return QDF_STATUS_E_FAULT;
	}

	be_soc = dp_get_be_soc_from_dp_soc(soc);
	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_CDP);
	if (!vdev) {
		dp_err("%p: Could not find vdev for id:%d", be_soc, vdev_id);
		return QDF_STATUS_E_FAULT;
	}

	/*
	 * Extract the VP profile from the BE vap.
	 */
	if (!soc_hdl->ol_ops->get_ppeds_profile_info_for_vap) {
		dp_err("%p: Register ppeds profile info before use\n", soc_hdl);
		return QDF_STATUS_E_FAULT;
	}

	if (soc_hdl->ol_ops->get_ppeds_profile_info_for_vap(soc->ctrl_psoc,
							    vdev_id, &vp_params) == QDF_STATUS_E_NULL_VALUE) {
		dp_err("%p: Failed to get ppeds profile for vap\n", vdev);
		return QDF_STATUS_E_NULL_VALUE;
	}

	/*
	 * Test PPE-DS enabled value
	 */
	if (vp_params.ppe_vp_type != PPE_VP_USER_TYPE_DS) {
		dp_err("%p: PPE-DS is not enabled on this vdev_id:%d",
		       be_soc, vdev_id);
		dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
		return QDF_STATUS_E_FAULT;
	}

	vp_profile = &be_soc->ppe_vp_profile[vp_params.ppe_vp_profile_idx];
	ppe_vp_idx = vp_profile->ppe_vp_num_idx;

	hal_tx_enable_pri2tid_map(be_soc->soc.hal_soc, val, ppe_vp_idx);
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return QDF_STATUS_SUCCESS;
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_ppeds_napi_disable_be() - Stop the PPE-DS instance
 * @soc: DP SoC
 *
 * Return: void
 */
static inline void dp_ppeds_napi_disable_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_ppeds_napi *napi_ctxt = &be_soc->ppeds_napi_ctxt;

	if (!dp_check_umac_reset_in_progress(soc))
		napi_disable(&napi_ctxt->napi);
}

/**
 * dp_ppeds_napi_enable_be() - Stop the PPE-DS instance
 * @soc: DP SoC
 *
 * Return: void
 */
static inline void dp_ppeds_napi_enable_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_ppeds_napi *napi_ctxt = &be_soc->ppeds_napi_ctxt;

	if (!dp_check_umac_reset_in_progress(soc))
		napi_enable(&napi_ctxt->napi);
}

/**
 * dp_ppeds_txdesc_pool_deinit_be() - Stop the PPE-DS instance
 * @soc: DP SoC
 *
 * Return: void
 */
static inline void dp_ppeds_txdesc_pool_deinit_be(struct dp_soc *soc)
{
	/**
	 * Tx desc pool deinit handled in umac post reset.
	 */
	if (!dp_check_umac_reset_in_progress(soc)) {
		if (dp_ppeds_tx_desc_pool_deinit_be()) {
			dp_ppeds_tx_desc_pool_deinit(soc);
			dp_ppeds_clear_tx_desc_in_soc_be(soc);
		}
	}
}

/**
 * dp_ppeds_get_umac_reset_progress() - Get the umac reset progress flag
 * @soc: DP SoC
 * @ppe_ds_wlan_ctx_info_handle: Information handle
 *
 * Return: void
 */
static inline void
dp_ppeds_get_umac_reset_progress(struct dp_soc *soc,
				 struct ppe_ds_wlan_ctx_info_handle *info_hdl)
{
	info_hdl->umac_reset_inprogress = dp_check_umac_reset_in_progress(soc);
}

#else
static inline void dp_ppeds_napi_disable_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_ppeds_napi *napi_ctxt = &be_soc->ppeds_napi_ctxt;

	napi_disable(&napi_ctxt->napi);
}

static inline void dp_ppeds_napi_enable_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct dp_ppeds_napi *napi_ctxt = &be_soc->ppeds_napi_ctxt;

	napi_enable(&napi_ctxt->napi);
}

static inline void dp_ppeds_txdesc_pool_deinit_be(struct dp_soc *soc)
{
	if (dp_ppeds_tx_desc_pool_deinit_be()) {
		dp_ppeds_tx_desc_pool_deinit(soc);
		dp_ppeds_clear_tx_desc_in_soc_be(soc);
	}
}

static inline void
dp_ppeds_get_umac_reset_progress(struct dp_soc *soc,
				 struct ppe_ds_wlan_ctx_info_handle *info_hdl)
{
	info_hdl->umac_reset_inprogress = 0;
}
#endif

/**
 * dp_ppeds_target_supported - Check target supports ppeds
 * @target_type: Device ID
 *
 * Check target supports ppeds
 *
 * Return: true if supported , false if not supported
 */
bool dp_ppeds_target_supported(int target_type)
{
	/*
	 * Add ppeds capable wlan target type here.
	 */
	switch (target_type) {
	case TARGET_TYPE_QCN9224:
	case TARGET_TYPE_QCN6432:
	case TARGET_TYPE_QCA5424:
		return true;

	default:
		return false;
	}
}

/**
 * dp_ppeds_stop_soc_be() - Stop the PPE-DS instance
 * @soc: DP SoC
 *
 * Return: void
 */
void dp_ppeds_stop_soc_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct ppe_ds_wlan_ctx_info_handle wlan_info_hdl;

	if (!be_soc->ppeds_handle || be_soc->ppeds_stopped)
		return;

	be_soc->ppeds_stopped = 1;

	dp_info("stopping ppe ds for soc %pK ", soc);

	dp_ppeds_napi_disable_be(soc);

	dp_ppeds_get_umac_reset_progress(soc, &wlan_info_hdl);
	ppe_ds_wlan_instance_stop(be_soc->ppeds_handle,
				  &wlan_info_hdl);

	dp_ppeds_txdesc_pool_deinit_be(soc);
}

/**
 * dp_ppeds_start_soc_be() - Start the PPE-DS instance
 * @soc: DP SoC
 *
 * Return: void
 */
QDF_STATUS dp_ppeds_start_soc_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);
	struct ppe_ds_wlan_ctx_info_handle wlan_info_hdl;

	if (!be_soc->ppeds_handle) {
		dp_err("%p: ppeds not allocated", soc);
		return QDF_STATUS_SUCCESS;
	}

	dp_ppeds_get_umac_reset_progress(soc, &wlan_info_hdl);
	if (!wlan_info_hdl.umac_reset_inprogress) {
		if (dp_ppeds_tx_desc_pool_init_be()) {
			if (dp_ppeds_tx_desc_pool_init(soc) != QDF_STATUS_SUCCESS) {
				dp_err("%p: ppeds tx desc global pool init failed", soc);
				return QDF_STATUS_SUCCESS;
			}

		}
			dp_ppeds_fill_tx_desc_in_soc_be(soc);
	}

	dp_ppeds_napi_enable_be(soc);

	be_soc->ppeds_stopped = 0;

	qdf_atomic_set(&be_soc->borrow_count, 0);

	dp_info("starting ppe ds for soc %pK ", soc);

	/* Complete the EDMA UMAC reset during instance start */
	wlan_info_hdl.umac_reset_inprogress = 0;

	if (ppe_ds_wlan_instance_start(be_soc->ppeds_handle,
				       &wlan_info_hdl) != 0) {
		dp_err("%p: ppeds start failed", soc);

		if (dp_ppeds_tx_desc_pool_deinit_be()) {
			dp_ppeds_tx_desc_pool_deinit(soc);
			dp_ppeds_clear_tx_desc_in_soc_be(soc);
		}

		return QDF_STATUS_SUCCESS;
	}

	be_soc->dp_ppeds_node_id =
				ppe_ds_wlan_get_node_id(be_soc->ppeds_handle);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_register_soc_be() - Register the PPE-DS instance
 * @be_soc: BE SoC
 * @idx: ppeds indices
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ppeds_register_soc_be(struct dp_soc_be *be_soc,
				    struct dp_ppe_ds_idxs *idx)
{
	struct ppe_ds_wlan_reg_info reg_info = {0};
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);

	if (!be_soc->ppeds_handle) {
		dp_err("%p: ppeds not attached", soc);
		return QDF_STATUS_SUCCESS;
	}

	reg_info.ppe2tcl_ba = be_soc->ppe2tcl_ring.base_paddr_aligned;
	reg_info.reo2ppe_ba = be_soc->reo2ppe_ring.base_paddr_aligned;
	reg_info.ppe2tcl_num_desc = be_soc->ppe2tcl_ring.num_entries;
	reg_info.reo2ppe_num_desc = be_soc->reo2ppe_ring.num_entries;
	if (ppe_ds_wlan_inst_register(be_soc->ppeds_handle, &reg_info) != true) {
		dp_err("%p: ppeds registeration failed", be_soc);
		return QDF_STATUS_SUCCESS;
	}

	idx->ppe2tcl_start_idx = reg_info.ppe2tcl_start_idx;
	idx->reo2ppe_start_idx = reg_info.reo2ppe_start_idx;
	be_soc->ppeds_int_mode_enabled = reg_info.ppe_ds_int_mode_enabled;
	be_soc->dp_ppeds_node_id = reg_info.dp_ppeds_node_id;

	dp_info("ppe2tcl_start_idx %d reo2ppe_start_idx %d node id %d\n",
		idx->ppe2tcl_start_idx, idx->reo2ppe_start_idx,
		be_soc->dp_ppeds_node_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_ppeds_detach_soc_be() - De-attach the PPE-DS instance
 * @be_soc: BE SoC
 *
 * Return: void
 */
void dp_ppeds_detach_soc_be(struct dp_soc_be *be_soc)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);

	if (!be_soc->ppeds_handle)
		return;

	dp_ppeds_del_napi_ctxt(be_soc);

	if (dp_ppeds_tx_cookie_detach_be()) {
		dp_hw_cookie_conversion_detach(be_soc, dp_get_ppeds_cookie_conv_ctx(soc));
		dp_ppeds_tx_cookie_free_mem();
	}

	if (dp_ppeds_tx_desc_pool_free_be()) {
		dp_ppeds_tx_desc_pool_free(soc);
		dp_ppeds_tx_desc_pool_free_mem();
	}

	ppe_ds_wlan_inst_free(be_soc->ppeds_handle);
	be_soc->ppeds_handle = NULL;

	dp_ppeds_deinit_ppe_vp_search_idx_tbl_be(be_soc);
	dp_ppeds_deinit_ppe_vp_profile_be(be_soc);
	dp_ppeds_deinit_ppe_vp_tbl_be(be_soc);
}

#ifdef DP_UMAC_HW_RESET_SUPPORT
/**
 * dp_ppeds_handle_attached() - Check if ppeds handle attached
 * @be_soc: BE SoC
 *
 * Return: true if ppeds handle attached else false.
 */
bool dp_ppeds_handle_attached(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (be_soc->ppeds_handle)
		return true;

	return false;
}

/**
 * dp_ppeds_interrupt_start_be() - Start all the PPEDS interrupts
 * @be_soc: BE SoC
 *
 * Return: void
 */
void dp_ppeds_interrupt_start_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->ppeds_handle)
		return;

	dp_ppeds_enable_irq(&be_soc->soc,
			    &be_soc->ppeds_wbm_release_ring);

	dp_ppeds_enable_irq(&be_soc->soc, &be_soc->reo2ppe_ring);
}

/**
 * dp_ppeds_interrupt_stop_be() - Stop all the PPEDS interrupts
 * @be_soc: BE SoC
 * @enable: enable/disable bit
 *
 * Return: void
 */
void dp_ppeds_interrupt_stop_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->ppeds_handle)
		return;

	dp_ppeds_disable_irq(&be_soc->soc,
			     &be_soc->ppeds_wbm_release_ring);

	dp_ppeds_disable_irq(&be_soc->soc, &be_soc->reo2ppe_ring);
}

/**
 * dp_ppeds_service_status_update_be() - Update the PPEDS service status
 * @be_soc: BE SoC
 * @enable: enable/disable bit
 *
 * Return: void
 */
void dp_ppeds_service_status_update_be(struct dp_soc *soc, bool enable)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->ppeds_handle)
		return;

	if (enable)
		qdf_atomic_set_bit(DP_PPEDS_NAPI_DONE_BIT,
				   &soc->service_rings_running);

	ppe_ds_wlan_service_status_update(be_soc->ppeds_handle, enable);
}

/**
 * dp_ppeds_is_enabled() - Check if PPEDS enabled on SoC.
 * @be_soc: BE SoC
 *
 * Return: void
 */
bool dp_ppeds_is_enabled_on_soc(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	return !!be_soc->ppeds_handle;
}
#endif

/**
 * dp_ppeds_handle_tx_comp() - PPE DS handle irq
 * @irq: irq number
 * @ctxt: irq context
 *
 * PPE DS handle irq function
 *
 * Return: irq status
 */
irqreturn_t dp_ppeds_handle_tx_comp(int irq, void *ctxt)
{
	struct dp_soc_be *be_soc =
			dp_get_be_soc_from_dp_soc((struct dp_soc *)ctxt);
	struct napi_struct *napi = &be_soc->ppeds_napi_ctxt.napi;

	if (!be_soc->ppeds_handle)
		return IRQ_NONE;

	dp_ppeds_disable_irq(&be_soc->soc, &be_soc->ppeds_wbm_release_ring);

	napi_schedule(napi);

	return IRQ_HANDLED;
}

QDF_STATUS dp_ppeds_init_soc_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->ppeds_handle)
		return QDF_STATUS_SUCCESS;

	be_soc->dp_ppeds_txdesc_hotlist_len =
	wlan_cfg_get_dp_soc_ppeds_tx_desc_hotlist_len(soc->wlan_cfg_ctx);
#ifndef QCA_SUPPORT_DP_GLOBAL_CTX
	be_soc->borrow_limit =
	wlan_cfg_get_dp_soc_ppeds_tx_desc_borrow_limit(soc->wlan_cfg_ctx);
	qdf_atomic_init(&be_soc->borrow_count);
#endif
	return dp_hw_cookie_conversion_init(be_soc,
			dp_get_ppeds_cookie_conv_ctx(soc));
}

QDF_STATUS dp_ppeds_deinit_soc_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	if (!be_soc->ppeds_handle)
		return QDF_STATUS_SUCCESS;

	return dp_hw_cookie_conversion_deinit(be_soc,
				dp_get_ppeds_cookie_conv_ctx(soc));
}

/**
 * dp_ppeds_attach_soc_be() - Attach the PPE-DS instance
 * @be_soc: BE SoC
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_ppeds_attach_soc_be(struct dp_soc_be *be_soc)
{
	struct dp_soc *soc = DP_SOC_BE_GET_SOC(be_soc);
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	struct dp_soc_be **besocptr;
	uint32_t num_elem;

	num_elem = wlan_cfg_get_dp_soc_ppeds_num_tx_desc(soc->wlan_cfg_ctx);

	if (dp_ppeds_tx_cookie_attach_be()) {
		qdf_status =
			dp_hw_cookie_conversion_attach(be_soc, dp_get_ppeds_cookie_conv_ctx(soc),
					num_elem, QDF_DP_TX_PPEDS_DESC_TYPE,
					DP_TX_PPEDS_POOL_ID);

		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			goto fail1;
	}

	if (dp_ppeds_tx_desc_pool_alloc_be())
		if (dp_ppeds_tx_desc_pool_alloc(soc, num_elem) != QDF_STATUS_SUCCESS) {
			dp_err("%p: Failed to allocate global ppeds tx desc pool", be_soc);
			goto fail2;
		}

	if (dp_ppeds_init_ppe_vp_tbl_be(be_soc) != QDF_STATUS_SUCCESS) {
		dp_err("%p: Failed to init ppe vp tbl", be_soc);
		goto fail3;
	}

	if (dp_ppeds_init_ppe_vp_search_idx_tbl_be(be_soc) != QDF_STATUS_SUCCESS) {
		dp_err("%p: Failed to init ppe vp tbl", be_soc);
		goto fail4;
	}

	if (dp_ppeds_init_ppe_vp_profile_be(be_soc) != QDF_STATUS_SUCCESS) {
		dp_err("%p: Failed to init ppe vp profiles", be_soc);
		goto fail5;
	}

	be_soc->ppeds_handle =
			ppe_ds_wlan_inst_alloc(&ppeds_ops,
						   sizeof(struct dp_soc_be *));
	if (!be_soc->ppeds_handle) {
		dp_err("%p: Failed to allocate ppeds soc instance", be_soc);
		goto fail6;
	}

	dp_ppeds_add_napi_ctxt(be_soc);

	dp_info("Allocated PPEDS handle\n");

	besocptr = (struct dp_soc_be **)ppe_ds_wlan_priv(be_soc->ppeds_handle);
	*besocptr = be_soc;

	return QDF_STATUS_SUCCESS;

fail6:
	dp_ppeds_deinit_ppe_vp_profile_be(be_soc);
fail5:
	dp_ppeds_deinit_ppe_vp_search_idx_tbl_be(be_soc);
fail4:
	dp_ppeds_deinit_ppe_vp_tbl_be(be_soc);
fail3:
	if (dp_ppeds_tx_desc_pool_free_be()) {
		dp_ppeds_tx_desc_pool_free(soc);
		dp_ppeds_tx_desc_pool_free_mem();
	}
fail2:
	if (dp_ppeds_tx_cookie_detach_be()) {
		dp_hw_cookie_conversion_detach(be_soc, dp_get_ppeds_cookie_conv_ctx(soc));
		dp_ppeds_tx_cookie_free_mem();
	}
fail1:
	dp_err("Could not allocate PPEDS handle\n");
	return QDF_STATUS_E_FAILURE;

}

/**
 * dp_ppeds_fill_tx_desc_in_soc_be() - Initialize the global_pool in be_soc structure.
 * @soc: SoC
 *
 */
inline void dp_ppeds_fill_tx_desc_in_soc_be(struct dp_soc *soc)
{
	dp_ppeds_tx_desc_pool_ctx *tx_desc_pool;
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	tx_desc_pool = dp_get_ppeds_tx_desc_pool(soc);
	be_soc->global_pool = tx_desc_pool;
}

/**
 * dp_ppeds_clear_tx_desc_in_soc_be() - Clear the global_pool in be_soc structure.
 * @soc: SoC
 *
 */
inline void dp_ppeds_clear_tx_desc_in_soc_be(struct dp_soc *soc)
{
	struct dp_soc_be *be_soc = dp_get_be_soc_from_dp_soc(soc);

	be_soc->global_pool = NULL;
}

#ifdef QCA_SUPPORT_DP_GLOBAL_CTX
/**
 * dp_ppeds_tx_desc_pool_alloc_be() - Checks if the pool is to be allocated
 * in the respective context.
 *
 * Allocates the pool once for all SOCs in the global context,
 * and every time in the SOC context.
 *
 * Return: bool
 */
inline bool dp_ppeds_tx_desc_pool_alloc_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	if (dp_global->tx_desc_ppeds_pool_alloc_cnt == 0) {
		dp_global->tx_desc_ppeds_pool_alloc_cnt++;
		dp_ppeds_tx_desc_pool_alloc_mem();
		return true;
	}

	dp_global->tx_desc_ppeds_pool_alloc_cnt++;
	return false;
}

/**
 * dp_ppeds_tx_desc_pool_free_be() - Checks if the pool is to be freed
 * in the respective context.
 *
 * Frees the pool once for all SOCs in the global context,
 * and every time in the SOC context.
 *
 * Return: bool
 */
inline bool dp_ppeds_tx_desc_pool_free_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	dp_global->tx_desc_ppeds_pool_alloc_cnt--;
	if (dp_global->tx_desc_ppeds_pool_alloc_cnt == 0)
		return true;

	return false;
}

/**
 * dp_ppeds_tx_desc_pool_init_be() - Checks if the pool is to be initialized
 * in the respective context
 *
 * Initializes the pool once for all SOCs in the global context,
 * and every time in the SOC context.
 *
 * Return: bool
 */
inline bool dp_ppeds_tx_desc_pool_init_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	if (dp_global->tx_desc_ppeds_pool_init_cnt == 0) {
		dp_global->tx_desc_ppeds_pool_init_cnt++;
		return true;
	}
	dp_global->tx_desc_ppeds_pool_init_cnt++;
	return false;
}

/**
 * dp_ppeds_tx_desc_pool_deinit_be() - Checks if the pool is to be deinitializes
 * in the respective context
 *
 * Deintializes the pool once for all SOCs in the global context,
 * and every time in the SOC context.
 *
 * Return: bool
 */
inline bool dp_ppeds_tx_desc_pool_deinit_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	dp_global->tx_desc_ppeds_pool_init_cnt--;
	if (dp_global->tx_desc_ppeds_pool_init_cnt == 0)
		return true;

	return false;
}

/**
 * dp_ppeds_tx_cookie_detach_be() - Checks if the cookie is to be detached
 * in the respective context
 *
 * Detaches the cookie once for all SOCs in the global context,
 * and every time in the SOC context.
 *
 * Return: bool
 */
inline bool dp_ppeds_tx_cookie_detach_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	dp_global->tx_cookie_ppeds_ctx_alloc_cnt--;
	if (dp_global->tx_cookie_ppeds_ctx_alloc_cnt == 0)
		return true;

	return false;
}

/**
 * dp_ppeds_tx_cookie_attach_be() - Checks if the cookie is to be attached
 * in the respective context
 *
 * Attaches the cookies once for all SOCs in the global context,
 * and every time in the SOC context.
 *
 * Return: bool
 */
inline bool dp_ppeds_tx_cookie_attach_be(void)
{
	struct dp_global_context *dp_global = wlan_objmgr_get_global_ctx();

	if (dp_global->tx_cookie_ppeds_ctx_alloc_cnt == 0) {
		dp_global->tx_cookie_ppeds_ctx_alloc_cnt++;
		dp_ppeds_tx_cookie_alloc_mem();
		return true;
	}
	dp_global->tx_cookie_ppeds_ctx_alloc_cnt++;
	return false;
}

/**
 * dp_ppeds_tx_desc_pool_alloc_mem() - PPE DS allocate mem
 *
 * PPE DS allocate mem
 */
inline void dp_ppeds_tx_desc_pool_alloc_mem(void)
{
	struct dp_global_context *dp_global = NULL;

	dp_global = wlan_objmgr_get_global_ctx();

	dp_global->ppeds_tx_desc =
		qdf_mem_malloc(sizeof(struct dp_tx_desc_pool_s));
}

/**
 * dp_ppeds_tx_cookie_alloc_mem() - PPE DS cookie allocate mem
 *
 * PPE DS cookie allocate mem
 */
inline void dp_ppeds_tx_cookie_alloc_mem(void)
{
	struct dp_global_context *dp_global = NULL;

	dp_global = wlan_objmgr_get_global_ctx();

	dp_global->ppeds_tx_cc_ctx =
		qdf_mem_malloc(sizeof(struct dp_hw_cookie_conversion_t));
}

/**
 * dp_ppeds_tx_desc_pool_free_mem() - PPE DS free memory
 *
 * PPE DS free pool mem
 */
inline void dp_ppeds_tx_desc_pool_free_mem(void)
{
	struct dp_global_context *dp_global = NULL;

	dp_global = wlan_objmgr_get_global_ctx();
	if (!dp_global->ppeds_tx_desc)
		return;

	qdf_mem_free(dp_global->ppeds_tx_desc);
	dp_global->ppeds_tx_desc = NULL;
}

/**
 * dp_ppeds_tx_cookie_free_mem() - PPE DS free cookie memory
 *
 * PPE DS free cookie mem
 */
inline void dp_ppeds_tx_cookie_free_mem(void)
{
	struct dp_global_context *dp_global = NULL;

	dp_global = wlan_objmgr_get_global_ctx();
	if (!dp_global->ppeds_tx_cc_ctx)
		return;

	qdf_mem_free(dp_global->ppeds_tx_cc_ctx);
	dp_global->ppeds_tx_cc_ctx = NULL;
}
#else
inline bool dp_ppeds_tx_desc_pool_alloc_be(void)
{
	return true;
}
inline bool dp_ppeds_tx_desc_pool_free_be(void)
{
	return true;
}
inline bool dp_ppeds_tx_desc_pool_init_be(void)
{
	return true;
}
inline bool dp_ppeds_tx_desc_pool_deinit_be(void)
{
	return true;
}
inline bool dp_ppeds_tx_cookie_detach_be(void)
{
	return true;
}
inline bool dp_ppeds_tx_cookie_attach_be(void)
{
	return true;
}
inline void dp_ppeds_tx_desc_pool_alloc_mem(void)
{
}
inline void dp_ppeds_tx_cookie_alloc_mem(void)
{
}
inline void dp_ppeds_tx_desc_pool_free_mem(void)
{
}
inline void dp_ppeds_tx_cookie_free_mem(void)
{
}
#endif
