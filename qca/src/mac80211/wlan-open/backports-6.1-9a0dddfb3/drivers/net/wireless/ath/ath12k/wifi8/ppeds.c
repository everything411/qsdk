// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include "../debug.h"
#include "../debugfs_sta.h"
#include "hw.h"
#include "../peer.h"
#include <linux/dma-mapping.h>
#include <linux/cacheflush.h>
#include "../hif.h"
#include "../ppe.h"
#include "../fse.h"
#include "hal.h"
#include "../ppe_public.h"
#include "ppeds.h"
#include "dp_tx.h"
#include "../pcic.h"
#include "dp.h"
#include "../umac_reset.h"

extern bool ath12k_fse_enable;

static int ath12k_wifi8_dp_ppeds_alloc_ppe_vp_profile(struct ath12k_base *ab,
			struct ath12k_dp_ppe_vp_profile **vp_profile,
			int vp_num)
{
	int ppe_vp_idx = vp_num - PPE_VP_WIFI8_START_IDX;
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	/* If a VP is already allocated with requested vp number, then return
	 * the same VP instead of creating a new profile
	 */
	if (ppe_vp_idx < 0 ||
			ppe_vp_idx >= PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Invalid vp_num :%d\n", vp_num);
		return -ENOSR;
	}

	if (central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].is_configured &&
			vp_num == central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].vp_num) {
		ath12k_dbg(central_ab, ATH12K_DBG_PPE,
				"vp profile with num %d will be reused\n", vp_num);
		goto end;
	}

	if (central_ab->dp->ppe.num_ppe_vp_profiles == PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	if (!central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].is_configured) {
		central_ab->dp->ppe.num_ppe_vp_profiles++;
		central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].is_configured = true;
	}

end:
	central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].ref_count++;
	*vp_profile = &central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx];
	return ppe_vp_idx;
}

static void
ath12k_dp_ppeds_dealloc_vp_search_idx_tbl_entry(struct ath12k_base *ab,
						int ppe_vp_search_idx)
{
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (ppe_vp_search_idx < 0 || ppe_vp_search_idx >= PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Invalid PPE VP search table free index");
		return;
	}

	ath12k_dbg(central_ab, ATH12K_DBG_PPE, "dealloc ppe_vp_search_idx %d\n",
			ppe_vp_search_idx);

	if (!central_ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_search_idx]) {
		ath12k_err(central_ab,
				"PPE VP search idx table is not configured at idx:%d",
				ppe_vp_search_idx);
		return;
	}

	central_ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_search_idx] = 0;
	central_ab->dp->ppe.num_ppe_vp_search_idx_entries--;
}

static void ath12k_dp_ppeds_dealloc_vp_tbl_entry(struct ath12k_base *ab,
						 int ppe_vp_num_idx)
{
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (ppe_vp_num_idx < 0 || ppe_vp_num_idx >= PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Invalid PPE VP free index");
		return;
	}

	/* Prevent register write if SOC is in recovery */
	if (!test_bit(ATH12K_FLAG_RECOVERY, &central_ab->dev_flags))
		ath12k_dp_ppeds_tx_set_ppe_vp_entry(central_ab, NULL, ppe_vp_num_idx,
						    0, 0, 0);

	if (!central_ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_num_idx]) {
		ath12k_err(central_ab,
				"PPE VP is not configured at idx:%d", ppe_vp_num_idx);
		return;
	}

	central_ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_num_idx] = 0;
	central_ab->dp->ppe.num_ppe_vp_entries--;
}

static void
ath12k_wifi8_dp_ppeds_dealloc_ppe_vp_profile(struct ath12k_base *ab,
					int ppe_vp_profile_idx,
					enum nl80211_iftype type)
{
	bool dealloced = false;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (ppe_vp_profile_idx < 0 || ppe_vp_profile_idx >= PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Invalid PPE VP profile free index");
		return;
	}

	vp_profile = &central_ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];

	if (!vp_profile->is_configured) {
		ath12k_err(central_ab, "PPE VP profile is not configured at idx:%d",
				ppe_vp_profile_idx);
		return;
	}

	vp_profile->ref_count--;

	if (!vp_profile->ref_count) {
		vp_profile->is_configured = false;
		central_ab->dp->ppe.num_ppe_vp_profiles--;
		dealloced = true;

		/* For STA mode ast index table reg also needs to be cleaned */
		if (type == NL80211_IFTYPE_STATION)
			ath12k_dp_ppeds_dealloc_vp_search_idx_tbl_entry(central_ab,
						vp_profile->search_idx_reg_num);

		ath12k_dp_ppeds_dealloc_vp_tbl_entry(central_ab,
				vp_profile->ppe_vp_num_idx);
	}

	if (dealloced)
		ath12k_dbg(central_ab, ATH12K_DBG_PPE, "%s success\n", __func__);
}

static int ath12k_wifi8_dp_ppeds_alloc_vp_tbl_entry(struct ath12k_base *ab,
						int ppe_vp_profile_idx)
{
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (central_ab->dp->ppe.num_ppe_vp_profiles == PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	if (central_ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_profile_idx]) {
		ath12k_dbg(central_ab, ATH12K_DBG_PPE,
				"Entry exist:vp_tbl enty alloc failed:%d\n",
				ppe_vp_profile_idx);
		return ppe_vp_profile_idx;
	}

	central_ab->dp->ppe.num_ppe_vp_entries++;
	central_ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_profile_idx] = 1;
	return ppe_vp_profile_idx;
}

static int ath12k_wifi8_dp_ppeds_alloc_vp_search_idx_tbl_entry(struct ath12k_base *ab,
					int ppe_vp_profile_idx)
{
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (central_ab->dp->ppe.num_ppe_vp_entries == PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	if (central_ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_profile_idx]) {
		ath12k_dbg(central_ab, ATH12K_DBG_PPE,
				"Entry exist:vp_srch_idx_tbl alloc failed:%d\n",
				ppe_vp_profile_idx);
		return (ppe_vp_profile_idx & PPE_VP_WIFI8_SEARCH_INDEX_REG_NUM_MASK);
	}

	central_ab->dp->ppe.num_ppe_vp_search_idx_entries++;
	central_ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_profile_idx] = 1;
	return (ppe_vp_profile_idx & PPE_VP_WIFI8_SEARCH_INDEX_REG_NUM_MASK);
}

struct ath12k_dp_ppe_vp_profile *
ath12k_wifi8_dp_ppeds_get_vp_profile(struct ath12k_base *ab,
		int vp_num)
{
	int ppe_vp_idx = vp_num - PPE_VP_WIFI8_START_IDX;
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &central_ab->dev_flags))
		return NULL;

	/* If a VP is already allocated with requested vp number, then return
	 * the same VP instead of creating a new profile
	 */
	if (ppe_vp_idx < 0 ||
			ppe_vp_idx >= PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Invalid vp_num :%d\n", vp_num);
		return NULL;
	}

	if (!central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].is_configured ||
		vp_num != central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].vp_num) {
		ath12k_dbg(central_ab, ATH12K_DBG_PPE, "Invalid vp_num :%d\n", vp_num);
		return NULL;
	}

	return &central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx];
}

int ath12k_wifi8_ppeds_attach_vif(struct ath12k_base *ab,
				struct ath12k_vif *ahvif,
				u32 vdev_id, int bank_id, u8 lmac_id)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	struct ath12k_dp_ppe_vp_profile *vp_profile = NULL;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	int ppe_vp_profile_idx, ppe_vp_tbl_idx = -1;
	int ppe_vp_search_tbl_idx = -1;
	int ret;
	enum nl80211_iftype vif_type;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;
	int vp_num = ahvif->dp_vif.ppe_vp_num;

	if (!wdev)
		return -EOPNOTSUPP;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (vp_num <= 0)
		return 0;

	if (ahvif->vif->type != NL80211_IFTYPE_AP &&
		ahvif->vif->type != NL80211_IFTYPE_STATION) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "DS is not supported for vap type %d\n", ahvif->vif->type);
		return 0;
	}

	/*Allocate a ppe vp profile for a vap */
	spin_lock(&ppe->ppe_vp_tbl_lock);
	if (!ppe_ops->ath12k_dp_ppeds_alloc_ppe_vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "Alloc handle not present:%s",
			   wdev->netdev->name);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}
	ppe_vp_profile_idx =
		ppe_ops->ath12k_dp_ppeds_alloc_ppe_vp_profile(ab,
				&vp_profile, vp_num);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "flows for %s cannot get vp_profile",
			   wdev->netdev->name);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}

	if (!ppe_ops->ath12k_dp_ppeds_alloc_vp_tbl_entry) {
		ath12k_err(ab, "Alloc failed vdev_id:%d", vdev_id);
		ret = -ENOSR;
		goto dealloc_vp_profile;
	}
	ppe_vp_tbl_idx =
		ppe_ops->ath12k_dp_ppeds_alloc_vp_tbl_entry(ab,
				ppe_vp_profile_idx);
	if (ppe_vp_tbl_idx < 0) {
		ath12k_err(ab, "Failed to allocate PPE VP idx for vdev_id:%d", vdev_id);
		ret = -ENOSR;
		goto dealloc_vp_profile;
	}

	if (ahvif->vif->type == NL80211_IFTYPE_STATION) {
		if (!ppe_ops->ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry) {
			ath12k_err(ab,
					"Failed srch idx tbl alloc - vdev_id:%d",
					vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}
		ppe_vp_search_tbl_idx =
			ppe_ops->ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry(ab,
					ppe_vp_profile_idx);
		if (ppe_vp_search_tbl_idx < 0) {
			ath12k_err(ab,
				"Failed to alloc srch tbl idx for vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}
		vp_profile->search_idx_reg_num = ppe_vp_search_tbl_idx;
	}

	vp_profile->vp_num = vp_num;
	vp_profile->ppe_vp_num_idx = ppe_vp_tbl_idx;
	vp_profile->to_fw = 0;
	vp_profile->use_ppe_int_pri = 0;
	vp_profile->drop_prec_enable = 0;
	vp_profile->entry_valid = true;

	ahvif->dp_vif.ppe_vp_profile_idx = ppe_vp_profile_idx;

	ath12k_dp_ppeds_tx_set_ppe_vp_entry(ab, vp_profile,
					    ppe_vp_profile_idx,
					    vdev_id, bank_id, lmac_id);

	spin_unlock(&ppe->ppe_vp_tbl_lock);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"PPEDS vp profile setup success soc:%d node_id:%d vdev_id %d\n",
			ab->dp->ppe.ppeds_soc_idx, ab->dp->ppe.ds_node_id, vdev_id);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"vpnum:%d ppe_vp_idx:%d ppe_vp_tbl_idx:%d to_fw %d int_pri %d\n",
			vp_num, ppe_vp_profile_idx, ppe_vp_tbl_idx, vp_profile->to_fw,
			vp_profile->use_ppe_int_pri);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"prec_en %d search_idx_reg_num %d\n",
			vp_profile->drop_prec_enable, vp_profile->search_idx_reg_num);

	return 0;

dealloc_vp_profile:
	vif_type = ahvif->vif->type;
	if (!ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile) {
		ath12k_err(ab, "Failed to dealloc vp profile:vdev_id:%d",
				vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return ret;
	}
	ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab,
			ppe_vp_profile_idx, vif_type);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return ret;
}

irqreturn_t ath12k_wifi8_ds_ppe2tcl_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_ppe2tcl_intr(ab->dp->ppe.ds_node_id);

	return IRQ_HANDLED;
}

irqreturn_t ath12k_wifi8_ds_reo2ppe_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_reo2ppe_intr(ab->dp->ppe.ds_node_id);
	return IRQ_HANDLED;
}

void ath12k_ppeds_wifi8_set_tcl_prod_idx(int ds_node_id, u16 tcl_prod_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	uint16_t tcl_ring_idx = 0;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring[tcl_ring_idx].ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_prod_cnt++;

	srng->u.src_ring.hp = tcl_prod_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}

u16 ath12k_ppeds_wifi8_get_tcl_cons_idx(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 tp;
	uint16_t tcl_ring_idx = 0;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_cons_cnt++;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring[tcl_ring_idx].ring_id];
	tp = readl(srng->u.src_ring.tp_addr);

	return tp / srng->entry_size;
}

void ath12k_ppeds_wifi8_set_reo_cons_idx(int ds_node_id,
				      u16 reo_cons_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	uint16_t reo_ring_idx = 0;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring[reo_ring_idx].ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_cons_cnt++;

	srng->u.src_ring.hp = reo_cons_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}

u16 ath12k_ppeds_wifi8_get_reo_prod_idx(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 hp;
	uint16_t reo_ring_idx = 0;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring[reo_ring_idx].ring_id];
	hp = readl(srng->u.dst_ring.hp_addr);
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_prod_cnt++;
	return hp / srng->entry_size;
}

void ath12k_ppeds_wifi8_enable_srng_intr(int ds_node_id, bool enable)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (ab->dp->ppe.txrx_hw_auto_idx) {
		ath12k_info(ab, "PPEDS: Data ring auto index en");
		return;
	}

	if (enable) {
		if (!ab->stats_disable)
			ab->dp->ppe.ppeds_stats.enable_intr_cnt++;

		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_PPE2TCL);
	} else {
		if (!ab->stats_disable)
			ab->dp->ppe.ppeds_stats.disable_intr_cnt++;

		ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_PPE2TCL);
	}
}

bool ath12k_ppeds_wifi8_free_rx_desc_v2(struct ppe_ds_wlan_rxdesc_elem *arr,
				  struct ath12k_base *ab, int index,
				  u16 *idx_of_ab)
{
	struct ath12k_rx_desc_info *rx_desc;
	struct sk_buff *skb;

	rx_desc = (struct ath12k_rx_desc_info *)arr[idx_of_ab[index]].cookie;

	if (rx_desc->device_id != ab->device_id)
		return false;

	skb = rx_desc->skb;
	rx_desc->skb = NULL;

	spin_lock_bh(&ab->dp->rx_desc_lock);
	list_add_tail(&rx_desc->list, &ab->dp->rx_desc_free_list);
	spin_unlock_bh(&ab->dp->rx_desc_lock);

	if (!skb) {
		ath12k_err(ab, "ppeds rx desc with no skb when freeing\n");
		return false;
	}

	/* When recycled_for_ds is set, packet is used by DS rings and never has
	 * touched by host. So, buffer unmap can be skipped.
	 */
	if (!skb->recycled_for_ds) {
		dmac_inv_range_no_dsb(skb->data, skb->data + (skb->len +
				      skb_tailroom(skb)));
		dma_unmap_single_attrs(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
				       skb->len + skb_tailroom(skb),
				       DMA_FROM_DEVICE, DMA_ATTR_SKIP_CPU_SYNC);
	}

	skb->recycled_for_ds = 0;
	skb->fast_recycled = 0;
	dev_kfree_skb_any(skb);
	return true;
}
EXPORT_SYMBOL(ath12k_ppeds_wifi8_free_rx_desc_v2);

int ath12k_wifi8_dp_rx_bufs_replenish_ppeds(struct ath12k_base *ab, int req_entries,
				      u16 *idx, struct ppe_ds_wlan_rxdesc_elem *arr)
{
	struct hal_srng *ppe2wbm_refill_srng;
	struct ath12k_buffer_addr *rxdma_desc;
	u32 cookie;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc;
	int count = 0, num_remain, i;
	u8 mgr =  ab->hal.hal_params->rx_buf_rbm;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(ab->dp);
	u32 ring_id;
	int cpu_id = smp_processor_id();

	if (!dp_wifi8->dp_ppe2wbm_use_dedicated_pool)
		ring_id = dp_wifi8->wbm_refill_ring[cpu_id %
						    DP_WBM_REFILL_RING_MAX].ring_id;
	else
		ring_id = dp_wifi8->ppe2wbm_refill_ring[PPE2WBM_SW_REFILL_RING].ring_id;

	ppe2wbm_refill_srng = &ab->hal.srng_list[ring_id];

	spin_lock_bh(&ppe2wbm_refill_srng->lock);
	ath12k_hal_srng_access_begin(ab, ppe2wbm_refill_srng);

	num_remain = req_entries;
	for (i = 0 ; i < req_entries; i++) {
		if ((i + 1) < (req_entries - 1))
			prefetch((struct ath12k_rx_desc_info *)arr[idx[i + 1]].cookie);

		rx_desc = (struct ath12k_rx_desc_info *)arr[idx[i]].cookie;
		if (!rx_desc)
			break;

		if (!rx_desc->skb) {
			ath12k_err(ab, "ppeds rx desc with no skb when reusing!\n");
			break;
		}

		cookie = rx_desc->cookie;
		paddr = rx_desc->paddr;

		rxdma_desc = ath12k_hal_srng_src_get_next_entry(ab, ppe2wbm_refill_srng);
		if (!rxdma_desc)
			break;

		ath12k_hal_rx_buf_addr_info_set(rxdma_desc, paddr, cookie, mgr);
		num_remain--;
	}

	ath12k_hal_srng_access_end(ab, ppe2wbm_refill_srng);
	spin_unlock_bh(&ppe2wbm_refill_srng->lock);

	/* move any remaining descriptors to free list */
	for (; i < req_entries; i++)
		count += ath12k_ppeds_wifi8_free_rx_desc_v2(arr, ab, i, idx);

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.num_rx_desc_freed += count;

	return 0;
}

/*TODO: Fetch this directly from ppe_ds, allocating dynamically will increase CPU */
#ifndef PPE_DS_TXCMPL_DEF_BUDGET
#define PPE_DS_TXCMPL_DEF_BUDGET 256
#endif

void ath12k_ppeds_wifi8_release_rx_desc(int ds_node_id,
				     struct ppe_ds_wlan_rxdesc_elem *arr, u16 count)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_base *src_ab = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	u32 rx_bufs_reaped[ATH12K_MAX_SOCS] = {0};
	struct ath12k_rx_desc_info *rx_desc;
	struct ath12k_rx_desc_info *cookie_to_rxdesc;
	int device_id;
	u32 i = 0, new_size, num_free_desc;
	u16 *tmp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_rx_desc_cnt += count;

	if (unlikely(count > ab->dp->ppe.ppeds_rx_num_elem)) {
		new_size = sizeof(u16) * count;
		for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
			tmp = krealloc(ab->dp->ppe.ppeds_rx_idx[device_id], new_size,
					GFP_ATOMIC);
			if (!tmp) {
				ath12k_err(ab, "ppeds:Rx desc alloc failed for size:%u\n",
					   count);
				goto err_h_alloc_failure;
			}

			ab->dp->ppe.ppeds_rx_idx[device_id] = tmp;
		}

		ab->dp->ppe.ppeds_rx_num_elem = count;
		ab->dp->ppe.ppeds_stats.num_rx_desc_realloc += device_id;
	}

	for (i = 0; i < count; i++) {
		if (ab->dp->ppe.hw_buff_mgmt) {
			cookie_to_rxdesc = ath12k_dp_get_rx_desc(ab->dp, arr[i].cookie);
			arr[i].cookie = (unsigned long)cookie_to_rxdesc;
		}

		rx_desc = (struct ath12k_rx_desc_info *)arr[i].cookie;
		if (!rx_desc) {
			ath12k_err(ab, "error: rx desc is null\n");
			continue;
		}

		device_id = rx_desc->device_id;
		/* Maintain indexes of arr per ab separately, which can accessed easily
		 * during per ab's rxdma srng replenish
		 */
		ab->dp->ppe.ppeds_rx_idx[device_id][rx_bufs_reaped[device_id]] = i;
		rx_bufs_reaped[device_id]++;
	}

	for (device_id = 0; device_id < ATH12K_MAX_SOCS; device_id++) {
		if (!rx_bufs_reaped[device_id])
			continue;

		src_ab = ag->ab[device_id];
		ath12k_wifi8_dp_rx_bufs_replenish_ppeds(src_ab, rx_bufs_reaped[device_id],
				&ab->dp->ppe.ppeds_rx_idx[device_id][0],
				arr);
	}

	return;

err_h_alloc_failure:
	for (device_id = 0; device_id < ag->num_hw; device_id++) {
		src_ab = ag->ab[device_id];
		num_free_desc = 0;
		for (i = 0; i < count; i++)
			num_free_desc +=
				ath12k_ppeds_wifi8_free_rx_desc_v2(arr, src_ab, i,
					&src_ab->dp->ppe.ppeds_rx_idx[device_id][0]);
		if (!src_ab->stats_disable)
			src_ab->dp->ppe.ppeds_stats.num_rx_desc_freed += num_free_desc;
	}
}

void ath12k_ppeds_wifi8_release_tx_desc_single(int ds_node_id,
					    u32 cookie)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_tx_single_cnt++;
}

u32 ath12k_ppeds_wifi8_get_batched_tx_desc(int ds_node_id,
					struct ppe_ds_wlan_txdesc_elem *arr,
					u32 num_buff_req,
					u32 buff_size,
					u32 headroom)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp_hw_group *dp_hw_grp = ab->dp->dp_hw_grp;
	u32 *used_cnt;
	int i = 0;
	int allocated = 0;
	struct sk_buff *skb = NULL;
	int flags = GFP_ATOMIC;
	dma_addr_t paddr;
	struct ath12k_ppeds_tx_desc_info *desc = NULL, *tmp;
	struct ath12k_ppeds_stats *ppeds_stats = &ab->dp->ppe.ppeds_stats;

#if LINUX_VERSION_IS_GEQ(4, 4, 0)
	flags = flags & ~__GFP_KSWAPD_RECLAIM;
#endif

	spin_lock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	list_for_each_entry_safe(desc, tmp, &dp_hw_grp->ppeds_tx_desc_reuse_list, list) {
		if (!num_buff_req)
			break;

		list_del(&desc->list);
		desc->in_use = true;
		desc->device_id = ab->device_id;

		dp_hw_grp->ppeds_tx_desc_reuse_list_len--;

		prefetch(list_next_entry(desc, list));
		num_buff_req--;

		arr[i].opaque_lo = desc->desc_id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = desc->paddr;
		allocated++;
		i++;
	}

	if (!num_buff_req) {
		used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
		(*used_cnt) += allocated;

		spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);
		goto update_stats_and_ret;
	}

	list_for_each_entry_safe(desc, tmp, &dp_hw_grp->ppeds_tx_desc_free_list, list) {
		if (!num_buff_req)
			break;

		list_del(&desc->list);
		desc->in_use = true;
		desc->device_id = ab->device_id;

		if (likely(!desc->skb)) {
		       /* In skb recycler, if recyler module allocates the buffers
			* already used by DS module to DS, then memzero, shinfo
			* reset can be avoided, since the DS packets were not
			* processed by SW
			*/
			skb = __netdev_alloc_skb_no_skb_reset(NULL, buff_size, flags);
			if (unlikely(!skb)) {
				desc->in_use = false;
				list_add_tail(&desc->list,
						&dp_hw_grp->ppeds_tx_desc_free_list);
				break;
			}

			skb_reserve(skb, headroom);
			if (!skb->recycled_for_ds) {
				dmac_inv_range_no_dsb((void *)skb->data,
						(void *)skb->data + buff_size - headroom);
				skb->recycled_for_ds = 1;
			}

			paddr = virt_to_phys(skb->data);

			desc->skb = skb;
			desc->paddr = paddr;
		} else {
			pr_warn("skb found in ppeds_tx_desc_free_list");
		}

		prefetch(list_next_entry(desc, list));
		num_buff_req--;

		arr[i].opaque_lo = desc->desc_id;
		arr[i].opaque_hi = 0;
		arr[i].buff_addr = desc->paddr;
		allocated++;
		i++;
	}

	used_cnt = this_cpu_ptr(dp_hw_grp->ppeds_tx_desc_used_cnt);
	(*used_cnt) += allocated;

	spin_unlock_bh(&dp_hw_grp->ppeds_tx_desc_lock);

	dsb(st);

update_stats_and_ret:
	if (unlikely(num_buff_req))
		ppeds_stats->tx_desc_alloc_fails += num_buff_req;

	if (unlikely(!ab->stats_disable)) {
		ppeds_stats->get_tx_desc_cnt++;
		ppeds_stats->tx_desc_allocated += allocated;
	}

	return allocated;
}

static int ath12k_dp_ppeds_tx_comp_poll(struct napi_struct *napi, int budget)
{
	struct ath12k_dp *dp = container_of(napi, struct ath12k_dp,
						ppe.ppeds_napi_ctxt.napi);
	struct ath12k_base *ab = dp->ab;
	int total_budget = (budget << 2) - 1;
	int work_done;

	if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags)) {
		napi_complete(napi);
		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_TX_COMPLETION);
		return 0;
	}
	work_done = dp->arch_ops->dp_ppeds_tx_completion_handler(ab, total_budget);

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tx_desc_freed += work_done;

	work_done = (work_done + 1) >> 2;

	if (budget > work_done) {
		napi_complete(napi);
		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_TX_COMPLETION);
	}

	return (work_done > budget) ? budget : work_done;

}

/* PPE-DS release interrupt */
irqreturn_t ath12k_wifi8_dp_ppeds_handle_tx_comp(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_TX_COMPLETION);
	napi_schedule(&napi_ctxt->napi);
	return IRQ_HANDLED;
}

int ath12k_ppe_napi_budget = NAPI_POLL_WEIGHT;
static int ath12k_dp_ppeds_add_napi_ctxt(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;
	int ret;

	ret = init_dummy_netdev((struct net_device *)&napi_ctxt->ndev);
	if (ret) {
		ath12k_err(ab, "dummy netdev init fail\n");
		return -ENOSR;
	}

#if LINUX_VERSION_IS_GEQ(6, 1, 0)
	netif_napi_add_weight(&napi_ctxt->ndev, &napi_ctxt->napi,
			      ath12k_dp_ppeds_tx_comp_poll, ath12k_ppe_napi_budget);
#else
	netif_napi_add(&napi_ctxt->ndev, &napi_ctxt->napi,
		       ath12k_dp_ppeds_tx_comp_poll);
#endif

	return 0;
}

static void ath12k_dp_ppeds_del_napi_ctxt(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;

	netif_napi_del(&napi_ctxt->napi);
	ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);
}

void ath12k_ppeds_wifi8_notify_napi_done(int ds_node_id)
{
	enum dp_umac_reset_tx_cmd tx_cmd = ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE;
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	ath12k_umac_reset_notify_target_sync_and_send(ab, tx_cmd,
			ab->dp->ppe.task_id);
}

uint8_t ath12k_ppeds_wifi8_get_wlan_arch_mode(void)
{
	return PPEDS_ARCH_MODE_WIFI8;
}

void ath12k_ppeds_get_rxfill_ring_info_v2(int ds_node_id,
					  struct ppe_ds_wlan_rxfill_ring_info *info)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (unlikely(!ab))
		return;

	if (!ab->dp->ppe.nss_plugin_ops ||
	    !ab->dp->ppe.nss_plugin_ops->get_rxfill_ring_info) {
		ath12k_err(ab, "PPEDS get_rxfill_ring_info not available\n");
		return;
	}

	info->arch_mode = PPE_DS_WIFI_ARCH_MODE_WIFI8;

	ab->dp->ppe.nss_plugin_ops->get_rxfill_ring_info(ds_node_id, info);
}

struct ppe_ds_wlan_ops_v2 ppeds_wlan_ops_v2_wifi8 = {
	.get_tx_desc_many = ath12k_ppeds_wifi8_get_batched_tx_desc,
	.release_tx_desc_single = ath12k_ppeds_wifi8_release_tx_desc_single,
	.enable_tx_consume_intr = ath12k_ppeds_wifi8_enable_srng_intr,
	.set_tcl_prod_idx  = ath12k_ppeds_wifi8_set_tcl_prod_idx,
	.set_reo_cons_idx = ath12k_ppeds_wifi8_set_reo_cons_idx,
	.get_tcl_cons_idx = ath12k_ppeds_wifi8_get_tcl_cons_idx,
	.get_reo_prod_idx = ath12k_ppeds_wifi8_get_reo_prod_idx,
	.release_rx_desc = ath12k_ppeds_wifi8_release_rx_desc,
	.notify_napi_done = ath12k_ppeds_wifi8_notify_napi_done,
	.get_wlan_arch_mode = ath12k_ppeds_wifi8_get_wlan_arch_mode,
};

int ath12k_wifi8_ppeds_attach(struct ath12k_base *ab)
{
	int i, ret, ds_node_id;

	if (!ath12k_ppe_ds_wifi8_enabled) {
		clear_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags);
		return 0;
	}

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	/*
	 * REO2PPE cookie conversion configuration.
	 */
	ab->hal.hal_ops->hal_ppeds_reo2ppe_cc_config(ab);

	ath12k_dp_ppeds_tx_cmem_init(ab);

	spin_lock_init(&ab->dp->ppe.ppe_vp_tbl_lock);

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		ab->dp->ppe.ppe_vp_tbl_registered[i] = 0;
		ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 0;
		ab->dp->ppe.ppe_vp_profile[i].is_configured = false;
	}

	ds_node_id = ab->dp->ppe.nss_plugin_ops->ds_inst_alloc(&ppeds_wlan_ops_v2_wifi8,
							sizeof(struct ath12k_base *));

	if (ds_node_id < 0) {
		ath12k_err(ab, "Failed to get DS node id for device_id %d\n",
			   ab->device_id);
		return -ENOSR;
	}
	ab->dp->ppe.ds_node_id = ds_node_id;
	ds_node_map[ds_node_id] = ab;

	WARN_ON(ab->dp->ppe.ppeds_soc_idx != -1);
	/* dec ppeds_soc_idx to start from 0 */
	ab->dp->ppe.ppeds_soc_idx = atomic_inc_return(&ath12k_num_ppeds_nodes) - 1;

	ath12k_info(ab, "PPEDS attach ab %p ppeds_soc_idx %d num_ppeds_nodes %d\n",
			ab, ab->dp->ppe.ppeds_soc_idx,
			atomic_read(&ath12k_num_ppeds_nodes));

	ret = ath12k_dp_ppeds_add_napi_ctxt(ab);
	if (ret)
		return -ENOSR;

	ath12k_info(ab, "PPEDS instance alloc success - ds_node_id:%d\n", ds_node_id);

	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		ab->dp->ppe.ppeds_rx_idx[i] =
					kzalloc((sizeof(u16) * PPE_DS_TXCMPL_DEF_BUDGET),
						      GFP_ATOMIC);
		if (!ab->dp->ppe.ppeds_rx_idx[i]) {
			ath12k_err(ab, "Failed to alloc mem ppeds_rx_idx\n");
			goto err_ppeds_attach;
		}
	}

	ab->dp->ppe.ppeds_rx_num_elem = PPE_DS_TXCMPL_DEF_BUDGET;
	ath12k_info(ab, "PPEDS attach success\n");

	return 0;

err_ppeds_attach:
	ab->dp->ppe.ppeds_rx_num_elem = 0;
	for (i = i - 1; i >= 0; i--) {
		kfree(ab->dp->ppe.ppeds_rx_idx[i]);
		ab->dp->ppe.ppeds_rx_idx[i] = NULL;
	}

	return -ENOMEM;
}

int ath12k_wifi8_ppeds_detach(struct ath12k_base *ab)
{
	int i;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	ath12k_dp_ppeds_del_napi_ctxt(ab);
	ath12k_dp_ppeds_cc_desc_cleanup(ab);

	/* free ppe-ds interrupts before freeing the instance */
	ath12k_hif_ppeds_free_interrupts(ab);

	ab->dp->ppe.nss_plugin_ops->ds_inst_free(ab->dp->ppe.ds_node_id);

	ab->dp->ppe.ppeds_soc_idx = -1;
	atomic_dec(&ath12k_num_ppeds_nodes);

	for (i = 0; i < PPE_VP_ENTRIES_MAX; i++) {
		ab->dp->ppe.ppe_vp_tbl_registered[i] = 0;
		ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 0;
		ab->dp->ppe.ppe_vp_profile[i].is_configured = false;
	}

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS detach success\n");

	if (ab->dp->ppe.ppeds_rx_num_elem) {
		ab->dp->ppe.ppeds_rx_num_elem = 0;
		for (i = 0; i < ab->ag->num_hw; i++) {
			kfree(ab->dp->ppe.ppeds_rx_idx[i]);
			ab->dp->ppe.ppeds_rx_idx[i] = NULL;
		}
	}

	return 0;
}

int ath12k_wifi8_dp_ppeds_start(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;
	struct ppe_ds_wlan_ctx_info_handle wlan_info_hdl;
	bool umac_reset_inprogress;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	umac_reset_inprogress = ath12k_dp_umac_reset_in_progress(ab);

	if (!umac_reset_inprogress)
		napi_enable(&napi_ctxt->napi);

	ab->dp->ppe.ppeds_stopped = 0;
	wlan_info_hdl.umac_reset_inprogress = 0;

	if (ab->dp->ppe.nss_plugin_ops->ds_inst_start(&wlan_info_hdl,
						      ab->dp->ppe.ds_node_id) != 0)
		return -EINVAL;

	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"PPEDS start success device_id %d ds_node_id %d ppeds_soc_idx %d",
			ab->device_id, ab->dp->ppe.ds_node_id, ab->dp->ppe.ppeds_soc_idx);
	return 0;
}

void ath12k_wifi8_dp_ppeds_stop(struct ath12k_base *ab)
{
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;
	struct ppe_ds_wlan_ctx_info_handle wlan_info_hdl;
	bool umac_reset_in_progress;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return;
	}

	umac_reset_in_progress = ath12k_dp_umac_reset_in_progress(ab);

	if (ab->dp->ppe.ppeds_stopped) {
		ath12k_warn(ab, "PPE DS aleady stopped!\n");
		return;
	}

	ab->dp->ppe.ppeds_stopped = 1;

	if (!umac_reset_in_progress)
		napi_disable(&napi_ctxt->napi);

	wlan_info_hdl.umac_reset_inprogress = umac_reset_in_progress;

	ab->dp->ppe.nss_plugin_ops->ds_inst_stop(&wlan_info_hdl,
						 ab->dp->ppe.ds_node_id);

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS stop success\n");
}

int ath12k_wifi8_dp_ppeds_register_soc(struct ath12k_dp *dp, struct dp_ppe_ds_idxs *idx)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k *ar = ab->pdevs[0].ar;
	struct hal_srng *ppe2tcl_ring, *reo2ppe_ring, *tqm2ppe_ring, *ppe2wbm_ring;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ppe_ds_wlan_arch_reg_info reg_info = {0};
	struct ppe_ds_wlan_reg_data_ring_cfg *ring_info =
				&reg_info.wifi8_cfg.data_ring.ring_info;
	struct ppe_ds_wlan_reg_data_ring_hptp_cfg *txrx_info =
				&reg_info.wifi8_cfg.data_ring.txrx_info;
	struct ppe_ds_wlan_reg_hbm_ring_cfg *hbm_ring_info =
				&reg_info.wifi8_cfg.hw_buf_mgmt.ring_info;
	struct ppe_ds_wlan_reg_hbm_ring_hptp_cfg *hbm_txrx_info =
				&reg_info.wifi8_cfg.hw_buf_mgmt.txrx_info;
	int ppe2tcl_irq_type, reo2ppe_irq_type;
	u32 reg_base;
	u32 reg_offset;
	dma_addr_t reg_paddr;
	uint32_t ring_idx;
	u32 ring_id;
	u32 ppe2tcl_rings_max;
	u32 reo2ppe_rings_max;

	reg_info.wifi_arch_mode = ath12k_ppeds_wifi8_get_wlan_arch_mode();

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	if (!(ath12k_ppeds_reo2ppe_rings_max) ||
		(ath12k_ppeds_reo2ppe_rings_max > ATH12K_REO2PPE_MAX_RINGS) ||
		!(ath12k_ppeds_ppe2tcl_rings_max) ||
		(ath12k_ppeds_ppe2tcl_rings_max > ATH12K_PPE2TCL_MAX_RINGS)) {
		ath12k_warn(ab, "Invalid ring max ppe2tcl:%d reo2ppe:%d\n",
				ath12k_ppeds_ppe2tcl_rings_max,
				ath12k_ppeds_reo2ppe_rings_max);
		return -EOPNOTSUPP;
	}

	for (ring_idx = 0; ring_idx < ath12k_ppeds_ppe2tcl_rings_max; ring_idx++) {

		ppe2tcl_ring = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring[ring_idx].ring_id];
		ring_info->ppe2tcl_ba[ring_idx] = dp->ppe.ppe2tcl_ring[ring_idx].paddr;
		ring_info->ppe2tcl_num_desc[ring_idx] = DP_PPE2TCL_RING_SIZE;
		ring_info->num_ppe2tcl = ath12k_ppeds_ppe2tcl_rings_max;
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS reg - PPE2TCL ring_id:%d ppe2tcl_ring:%p\n",
				dp->ppe.ppe2tcl_ring[ring_idx].ring_id,
				ppe2tcl_ring);
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS PPE2TCL num_desc:%d num_rings:%d\n",
				ring_info->ppe2tcl_num_desc[ring_idx],
				ring_info->num_ppe2tcl);

		if (dp->ppe.txrx_hw_auto_idx) {
			reg_offset = ppe2tcl_ring->hwreg_base[HAL_SRNG_REG_GRP_R2];
			reg_paddr = ath12k_hif_ppeds_get_pci_window_umac_reg_paddr(ab,
							reg_offset);
			if (!reg_paddr) {
				ath12k_err(ab, "PPEDS PPE2TCL pci window addr invalid");
				return -EINVAL;
			}
			txrx_info->wlan_ppe2tcl_hp_addr[ring_idx].paddr = reg_paddr;
			txrx_info->wlan_ppe2tcl_hp_addr[ring_idx].vaddr =
							ppe2tcl_ring->u.src_ring.hp_addr;
			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS - Data ring auto index - reg_offset:%x\n",
					reg_offset);
			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS - ppe2tcl_paddr:%pad vaddr:%p\n",
					&reg_paddr, ppe2tcl_ring->u.src_ring.hp_addr);
			reg_info.wifi8_cfg.data_ring_auto_index_en = true;
		} else {
			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS - Data ring auto index disabled\n");
		}

	}

	for (ring_idx = 0; ring_idx < ath12k_ppeds_reo2ppe_rings_max; ring_idx++) {
		reo2ppe_ring = &ab->hal.srng_list[dp->ppe.reo2ppe_ring[ring_idx].ring_id];
		ring_info->reo2ppe_ba[ring_idx] = dp->ppe.reo2ppe_ring[ring_idx].paddr;
		ring_info->reo2ppe_num_desc[ring_idx] = DP_REO2PPE_RING_SIZE;
		ring_info->num_reo2ppe = ath12k_ppeds_reo2ppe_rings_max;
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS reg - REO2PPE ring_id:%d reo2ppe_ring:%p\n",
				dp->ppe.reo2ppe_ring[ring_idx].ring_id,
				reo2ppe_ring);
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS REO2PPE ring_size:%d num_reo2ppe_ring:%d\n",
				ring_info->reo2ppe_num_desc[ring_idx],
				ring_info->num_reo2ppe);

		if (dp->ppe.txrx_hw_auto_idx) {
			reg_base = reo2ppe_ring->hwreg_base[HAL_SRNG_REG_GRP_R2];
			reg_offset = reg_base + (HAL_REO1_RING_TP - HAL_REO1_RING_HP);
			reg_paddr = ath12k_hif_ppeds_get_pci_window_umac_reg_paddr(ab,
					reg_offset);
			if (!reg_paddr) {
				ath12k_err(ab, "PPEDS REO2PPE pci window addr invalid");
				return -EINVAL;
			}
			txrx_info->wlan_reo2ppe_tp_addr[ring_idx].paddr = reg_paddr;
			txrx_info->wlan_reo2ppe_tp_addr[ring_idx].vaddr =
					reo2ppe_ring->u.dst_ring.tp_addr;
			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS  - Data ring auto index - reg_offset:%x\n",
					reg_offset);
			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS - reo2ppe_paddr:%pad vaddr:%p\n",
					&reg_paddr, reo2ppe_ring->u.dst_ring.tp_addr);
			reg_info.wifi8_cfg.data_ring_auto_index_en = true;
		} else {
			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS - Data ring auto index disabled\n");
		}

	}

	if (dp->ppe.hw_buff_mgmt) {
		reg_info.wifi8_cfg.hw_buff_mgmt_en = true;
		ring_id = dp_wifi8->ppe2wbm_refill_ring[PPE2WBM_HW_REFILL_RING].ring_id;
		tqm2ppe_ring = &ab->hal.srng_list[dp->ppe.tqm2ppe_txcmp_ring.ring_id];
		ppe2wbm_ring = &ab->hal.srng_list[ring_id];

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS:HBM - TQM2PPE ring_id:%d PPE2WBM ring_id:%d\n",
				dp->ppe.tqm2ppe_txcmp_ring.ring_id,
				dp_wifi8->ppe2wbm_refill_ring[0].ring_id);

		/* HW buffer manager - TQM2PPE ring and PPE2WBM */
		hbm_ring_info->tqm2ppe_ba = dp->ppe.tqm2ppe_txcmp_ring.paddr;
		hbm_ring_info->tqm2ppe_num_desc = DP_TQM2PPE_RING_SIZE;
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS:HBM tqm2ppe_ba:%pad num_desc:%u\n",
				&hbm_ring_info->tqm2ppe_ba,
				hbm_ring_info->tqm2ppe_num_desc);

		hbm_ring_info->ppe2wbm_ba = dp_wifi8->ppe2wbm_refill_ring[0].paddr;
		hbm_ring_info->ppe2wbm_num_desc = ath12k_ppeds_ppe2wbm_ring_size;
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS:HBM pp2wbm_ba:%pad num_desc:%u\n",
				&hbm_ring_info->ppe2wbm_ba,
				hbm_ring_info->ppe2wbm_num_desc);

		/* HW buffer manager - TQM2PPE ring  auto index set */
		reg_base = tqm2ppe_ring->hwreg_base[HAL_SRNG_REG_GRP_R2];
		reg_offset = reg_base + (HAL_REO1_RING_TP - HAL_REO1_RING_HP);
		reg_paddr =
			ath12k_hif_ppeds_get_pci_window_umac_reg_paddr(ab, reg_offset);
		hbm_txrx_info->wlan_tqm2ppe_tp_addr.paddr = reg_paddr;
		hbm_txrx_info->wlan_tqm2ppe_tp_addr.vaddr =
							tqm2ppe_ring->u.dst_ring.tp_addr;

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS:HBM - TQM2PPE auto idx tqm2ppe_tp:%x paddr %pad\n",
				reg_offset, &reg_paddr);

		/* ======HW buffer manager - PPE2WBM ring  auto index set ======*/
		reg_base = ppe2wbm_ring->hwreg_base[HAL_SRNG_REG_GRP_R2];
		reg_offset = reg_base;
		reg_paddr =
			ath12k_hif_ppeds_get_pci_window_umac_reg_paddr(ab, reg_offset);
		hbm_txrx_info->wlan_ppe2wbm_hp_addr.paddr = reg_paddr;
		hbm_txrx_info->wlan_ppe2wbm_hp_addr.vaddr =
							ppe2wbm_ring->u.src_ring.hp_addr;

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS:HBM - PPE2WBM auto idx ppe2wbm_hp:%x paddr %pad\n",
				reg_offset, &reg_paddr);

		reg_info.wifi8_cfg.hw_buff_mgmt_en = true;
	}

	if (ar) {
		spin_lock_bh(&ar->data_lock);
		reg_info.freq.low_freq = ar->chan_info.low_freq;
		reg_info.freq.high_freq = ar->chan_info.high_freq;
		spin_unlock_bh(&ar->data_lock);
	}

	if (ab->dp->ppe.nss_plugin_ops->ds_inst_register_wifi_arch_mode(&reg_info,
						ab->dp->ppe.ds_node_id) != true) {
		ath12k_err(ab, "ppeds inst register failed ");
		return -EINVAL;
	}

	if (reg_info.wifi8_cfg.data_ring_auto_index_en) {
		ppe2tcl_rings_max = ath12k_ppeds_ppe2tcl_rings_max;
		for (ring_idx = 0; ring_idx < ppe2tcl_rings_max; ring_idx++) {
			ppe2tcl_irq_type = (ring_idx * ATH12K_PPEDS_IRQ_NEXT_RING) +
						PPEDS_IRQ_PPE2TCL;

			ring_id = dp->ppe.ppe2tcl_ring[ring_idx].ring_id;

			/*
			 * Disable interrupt.
			 */
			ath12k_hif_ppeds_irq_disable(ab, ppe2tcl_irq_type);

			ppe2tcl_ring = &ab->hal.srng_list[ring_id];

			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS:HBM auto_idx ring_id:%d ppe2tcl_ring:%p\n",
					dp->ppe.ppe2tcl_ring[ring_idx].ring_id,
					ppe2tcl_ring);
			ab->hal.hal_ops->hal_srng_idx_update_addr(ab,
					ppe2tcl_ring, NULL, 0,
					txrx_info->edma_rxdesc_cons_addr[ring_idx].vaddr,
					txrx_info->edma_rxdesc_cons_addr[ring_idx].paddr);

			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS:HBM PPE2TCL srng change idx ptr done\n");
		}

		reo2ppe_rings_max = ath12k_ppeds_reo2ppe_rings_max;
		for (ring_idx = 0; ring_idx < reo2ppe_rings_max; ring_idx++) {
			reo2ppe_irq_type =
				(ring_idx * ATH12K_PPEDS_IRQ_NEXT_RING) +
				PPEDS_IRQ_REO2PPE;

			/*
			 * Disable interrupt.
			 */
			ath12k_hif_ppeds_irq_disable(ab, reo2ppe_irq_type);

			ring_id = dp->ppe.reo2ppe_ring[ring_idx].ring_id;
			reo2ppe_ring =  &ab->hal.srng_list[ring_id];

			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS:HBM auto_idx ring_id:%d reo2ppe_ring:%p\n",
					dp->ppe.reo2ppe_ring[ring_idx].ring_id,
					reo2ppe_ring);
			ab->hal.hal_ops->hal_srng_idx_update_addr(ab, reo2ppe_ring,
					txrx_info->edma_txdesc_prod_addr[ring_idx].vaddr,
					txrx_info->edma_txdesc_prod_addr[ring_idx].paddr,
					NULL, 0);

			ath12k_dbg(ab, ATH12K_DBG_PPE,
					"PPEDS:HBM REO2PPE srng change idx ptr done\n");
		}

		/*
		 * Hw buffer manager auto index for TQM2PPE and PPE2WBM rings.
		 */
		if (dp->ppe.hw_buff_mgmt) {
			ab->hal.hal_ops->hal_srng_idx_update_addr(ab, tqm2ppe_ring,
				hbm_txrx_info->edma_rxfill_prod_addr.vaddr,
				hbm_txrx_info->edma_rxfill_prod_addr.paddr,
						NULL, 0);

			ab->hal.hal_ops->hal_srng_idx_update_addr(ab,
				ppe2wbm_ring, NULL, 0,
				hbm_txrx_info->edma_txcmpl_cons_addr.vaddr,
				hbm_txrx_info->edma_txcmpl_cons_addr.paddr);
		}
	} else {

		idx->ppe2tcl_start_idx = reg_info.wifi7_cfg.ppe2tcl_start_idx;
		idx->reo2ppe_start_idx = reg_info.wifi7_cfg.reo2ppe_start_idx;
		ab->dp->ppe.ppeds_int_mode_enabled =
					reg_info.wifi7_cfg.ppe_ds_int_mode_enabled;
	}

	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"PPEDS register success\n");
	ath12k_dbg(ab,
		ATH12K_DBG_PPE,
		"PPEDS register success device_id:%d ppe2tcl_idx:0x%x reo2ppe_idx:0x%x",
		ab->device_id, idx->ppe2tcl_start_idx, idx->reo2ppe_start_idx);

	return 0;
}

static int ath12k_wifi8_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
				enum hal_ring_type type, int ring_num,
				int num_entries)
{
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

	ring->size = (num_entries * entry_sz) + HAL_RING_BASE_ALIGN - 1;
#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst and tx completion rings from cacheable memory */
		switch (type) {
		case HAL_REO_DST:
			cached = true;
			break;
		default:
			cached = false;
		}
	}
#else
	cached = true;
#endif
	if (cached) {
		ring->vaddr_unaligned = kzalloc(ring->size, GFP_KERNEL);
		ring->paddr_unaligned = virt_to_phys(ring->vaddr_unaligned);
	} else {
		ring->vaddr_unaligned = dma_alloc_coherent(ab->dev, ring->size,
							   &ring->paddr_unaligned,
							   GFP_KERNEL);
	}
	if (!ring->vaddr_unaligned)
		return -ENOMEM;

	memset(ring->vaddr_unaligned, 0, ring->size);
	ring->vaddr = PTR_ALIGN(ring->vaddr_unaligned, HAL_RING_BASE_ALIGN);
	ring->paddr = ring->paddr_unaligned + ((unsigned long)ring->vaddr -
			(unsigned long)ring->vaddr_unaligned);

	return 0;
}

int ath12k_wifi8_ppeds_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
			       enum hal_ring_type type, int ring_num,
			       int num_entries)
{
	int ret;

	ret = ath12k_wifi8_dp_srng_alloc(ab, ring, type, ring_num, num_entries);
	if (ret != 0)
		ath12k_warn(ab, "Failed to allocate dp srng ring.\n");

	return 0;
}

static int ath12k_wifi8_ppeds_dp_srng_init(struct ath12k_base *ab, struct dp_srng *ring,
				     enum hal_ring_type type, int ring_num,
				     int mac_id, int num_entries,
				     u32 restore_idx)
{
	int ret;

	ret = ath12k_dp_srng_init_idx(ab, ring, type, ring_num, mac_id,
				      num_entries, restore_idx);
	if (ret != 0)
		ath12k_warn(ab, "Failed to initialize dp srng ring.\n");

	return 0;
}

int ath12k_wifi8_dp_srng_ppeds_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	int ret, size;
	uint8_t idx;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!(ath12k_ppeds_reo2ppe_rings_max) ||
		(ath12k_ppeds_reo2ppe_rings_max > ATH12K_REO2PPE_MAX_RINGS) ||
		!(ath12k_ppeds_ppe2tcl_rings_max) ||
		(ath12k_ppeds_ppe2tcl_rings_max > ATH12K_PPE2TCL_MAX_RINGS)) {
		ath12k_warn(ab, "Invalid ring max ppe2tcl:%d reo2ppe:%d\n",
				ath12k_ppeds_ppe2tcl_rings_max,
				ath12k_ppeds_reo2ppe_rings_max);
		return -EINVAL;
	}

	ath12k_info(ab, "PPEDS srng alloc\n");
	for (idx = 0; idx < ath12k_ppeds_reo2ppe_rings_max; idx++) {
		ath12k_info(ab, "PPEDS: Before alloc reo2ppe[%d] ring_id=%d\n",
		       idx, dp->ppe.reo2ppe_ring[idx].ring_id);
		ret = ath12k_wifi8_ppeds_dp_srng_alloc(ab, &dp->ppe.reo2ppe_ring[idx],
				HAL_REO2PPE,
				0, DP_REO2PPE_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up reo2ppe ring:%d ring_num:%d\n",
					ret, idx);
			return ret;
		}
		ath12k_info(ab, "PPEDS After alloc reo2ppe[%d] ring_id=%d paddr=%pad\n",
				idx, dp->ppe.reo2ppe_ring[idx].ring_id,
				&dp->ppe.reo2ppe_ring[idx].paddr);
	}

	for (idx = 0; idx < ath12k_ppeds_ppe2tcl_rings_max; idx++) {
		ret = ath12k_wifi8_ppeds_dp_srng_alloc(ab, &dp->ppe.ppe2tcl_ring[idx],
				HAL_PPE2TCL,
				0, DP_PPE2TCL_RING_SIZE);
		if (ret) {
			ath12k_warn(ab, "failed to set up ppe2tcl ring :%d\n", ret);
			return ret;
		}
		ath12k_info(ab, "PPEDS alloc ppe2tcl[%d] ring_id=%d paddr=%pad\n",
				idx, dp->ppe.ppe2tcl_ring[idx].ring_id,
				&dp->ppe.ppe2tcl_ring[idx].paddr);
	}

	if (dp->ppe.hw_buff_mgmt) {
		ret = ath12k_dp_srng_alloc(ab, &dp->ppe.tqm2ppe_txcmp_ring,
					   HAL_TQM2PPE,
					   PPEDS_TQM2PPE_TX_CMPLN_RING_NUM, 0,
					   DP_TQM2PPE_RING_SIZE);
		if (ret) {
			ath12k_err(ab,
				   "failed to alloc wbm2sw ppeds tx completion ring :%d\n",
				   ret);
			return ret;
		}
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->ppe.ppeds_comp_ring.ppeds_txcmpl_ring,
				   HAL_TX_COMPLETION,
				   PPEDS_TX_CMPLN_RING_NUM, 0,
				   DP_TX_COMP_PPEDS_RING_SIZE);
	if (ret) {
		ath12k_err(ab,
			   "failed to alloc TQM2SW ppeds tx completion ring :%d\n",
			   ret);
		return ret;
	}

	size = sizeof(struct hal_tqm2sw_completion_ring) * DP_TX_COMP_PPEDS_RING_SIZE;
	dp->ppe.ppeds_comp_ring.tx_status_head = 0;
	dp->ppe.ppeds_comp_ring.tx_status_tail = DP_TX_COMP_PPEDS_RING_SIZE - 1;
	dp->ppe.ppeds_comp_ring.tx_status = kmalloc(size, GFP_KERNEL);

	if (!dp->ppe.ppeds_comp_ring.tx_status) {
		ath12k_err(ab, "PPEDS tx status completion buffer alloc failed\n");
		return -ENOMEM;
	}

	return 0;
}

int ath12k_wifi8_dp_srng_ppeds_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	struct dp_ppe_ds_idxs restore_idx = {0};
	int ret;
	u8 idx;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!dp->ppe.txrx_hw_auto_idx) {
		ret = ath12k_wifi8_dp_ppeds_register_soc(dp, &restore_idx);
		if (ret) {
			ath12k_err(ab, "ppeds registration failed\n");
			goto err;
		}
	}

	for (idx = 0; idx < ath12k_ppeds_reo2ppe_rings_max; idx++) {
		ret = ath12k_wifi8_ppeds_dp_srng_init(ab, &dp->ppe.reo2ppe_ring[idx],
				HAL_REO2PPE,
				0, 0, DP_REO2PPE_RING_SIZE,
				restore_idx.reo2ppe_start_idx);
		if (ret) {
			ath12k_warn(ab, "failed to set up reo2ppe ring :%d\n", ret);
			goto err;
		}
		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"PPEDS SRNG init reo2ppe[%d] ring_id=%d restore_idx=%d\n",
				idx, dp->ppe.reo2ppe_ring[idx].ring_id,
				restore_idx.reo2ppe_start_idx);
	}

	for (idx = 0; idx < ath12k_ppeds_ppe2tcl_rings_max; idx++) {
		ret = ath12k_wifi8_ppeds_dp_srng_init(ab, &dp->ppe.ppe2tcl_ring[idx],
				HAL_PPE2TCL,
				0, 0, DP_PPE2TCL_RING_SIZE,
				restore_idx.ppe2tcl_start_idx);
		if (ret) {
			ath12k_warn(ab, "failed to set up ppe2tcl ring :%d\n", ret);
			goto err;
		}

		ath12k_dbg(ab, ATH12K_DBG_PPE,
				"Init ppe2tcl[%d] ring_id=%d restore_idx=%d\n",
				idx,
				dp->ppe.ppe2tcl_ring[idx].ring_id,
				restore_idx.ppe2tcl_start_idx);
	}

	ath12k_hal_reo_config_reo2ppe_dest_info(ab);

	if (dp->ppe.hw_buff_mgmt) {
		ret = ath12k_dp_srng_init(ab, &dp->ppe.tqm2ppe_txcmp_ring,
					  HAL_TQM2PPE,
					  PPEDS_TQM2PPE_TX_CMPLN_RING_NUM, 0);
		if (ret) {
			ath12k_err(ab,
				"failed to init wbm2sw ppeds tx completion ring :%d\n",
				ret);
			goto err;
		}
	}

	if (dp->ppe.txrx_hw_auto_idx) {
		ret = ath12k_wifi8_dp_ppeds_register_soc(dp, &restore_idx);
		if (ret) {
			ath12k_err(ab, "ppeds registration failed\n");
			goto err;
		}
	}

	ret = ath12k_dp_srng_init(ab, &dp->ppe.ppeds_comp_ring.ppeds_txcmpl_ring,
				  HAL_TX_COMPLETION,
				  PPEDS_TX_CMPLN_RING_NUM, 0);
	if (ret) {
		ath12k_err(ab,
			   "failed to init TQM2SW ppeds tx completion ring :%d\n",
			   ret);
		goto err;
	}

	/* RBM mapping for PPE2TCL ring */
	ath12k_hal_tx_config_rbm_mapping(ab, PPEDS_PPE2TCL1,
					 HAL_RX_BUF_RBM_SW6_BM,
					 HAL_PPE2TCL);

err:
	/* caller takes care of calling ath12k_dp_srng_ppeds_cleanup */
	return ret;
}

void ath12k_wifi8_dp_srng_ppeds_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	uint8_t ring_idx;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	for (ring_idx = 0; ring_idx < ath12k_ppeds_ppe2tcl_rings_max; ring_idx++)
		ath12k_dp_srng_cleanup(ab, &dp->ppe.ppe2tcl_ring[ring_idx]);

	for (ring_idx = 0; ring_idx < ath12k_ppeds_reo2ppe_rings_max; ring_idx++)
		ath12k_dp_srng_cleanup(ab, &dp->ppe.reo2ppe_ring[ring_idx]);

	kfree(dp->ppe.ppeds_comp_ring.tx_status);
	dp->ppe.ppeds_comp_ring.tx_status = NULL;

	ath12k_dp_srng_cleanup(ab, &dp->ppe.ppeds_comp_ring.ppeds_txcmpl_ring);
	ath12k_dp_srng_cleanup(ab, &dp->ppe.tqm2ppe_txcmp_ring);
}

void ath12k_wifi8_dp_ppeds_interrupt_stop(struct ath12k_base *ab)
{
	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	if (!ab->dp->ppe.hw_buff_mgmt)
		ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_REO2PPE);

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_TX_COMPLETION);
}

void ath12k_wifi8_dp_ppeds_interrupt_start(struct ath12k_base *ab)
{

	if (!ab->dp->ppe.hw_buff_mgmt)
		ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_REO2PPE);

	ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_TX_COMPLETION);
}

void ath12k_wifi8_ppeds_free_pci_interrupts(struct ath12k_base *ab)
{
	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL]);
	free_irq(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_PPE2TCL], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE]);
	free_irq(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_REO2PPE], ab);

	disable_irq_nosync(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_TX_COMPLETION]);
	free_irq(ab->dp->ppe.ppeds_irq[PPEDS_IRQ_TX_COMPLETION], ab);
}

struct ath12k_dp_ppe_vp_profile *
ath12k_wifi8_dp_ppeds_get_vp_profile_from_idx(struct ath12k_base *ab,
		uint32_t ppe_vp_idx)
{
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(ab->dp->dp_hw_grp);

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &central_ab->dev_flags))
		return NULL;

	/* If a VP is already allocated with requested vp number, then return
	 * the same VP instead of creating a new profile
	 */
	if (ppe_vp_idx < 0 ||
			ppe_vp_idx >= PPE_VP_WIFI8_ENTRIES_MAX) {
		ath12k_err(central_ab, "Invalid vp_idx:%d\n", ppe_vp_idx);
		return NULL;
	}

	if (!central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].is_configured) {
		ath12k_dbg(central_ab, ATH12K_DBG_PPE,
				"vp_idx:%d not configured\n", ppe_vp_idx);
		return NULL;
	}

	return &central_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx];
}

int ath12k_wifi8_dp_ppeds_get_bank_lmac_id(struct ath12k_base *ab,
		struct ath12k *ar,
		struct ath12k_link_vif *arvif,
		struct ath12k_dp_ppe_vp_profile *vp_profile,
		u8 *bank_id, u8 *lmac_id)
{
	*lmac_id = HAL_TX_WILD_CARD_LINK_ID;
	*bank_id = arvif->ahvif->dp_vif.bank_id;
	return 0;
}

struct ath12k_ppeds_arch_ops ath12k_wifi8_arch_ppeds_ops  = {
	.ath12k_ppeds_ppe2tcl_irq_handler = ath12k_wifi8_ds_ppe2tcl_irq_handler,
	.ath12k_ppeds_reo2ppe_irq_handler = ath12k_wifi8_ds_reo2ppe_irq_handler,
	.ath12k_ppeds_ppe2tcl_tx_compln = ath12k_wifi8_dp_ppeds_handle_tx_comp,
	.ath12k_ppeds_start = ath12k_wifi8_dp_ppeds_start,
	.ath12k_ppeds_stop = ath12k_wifi8_dp_ppeds_stop,
	.ath12k_ppeds_attach = ath12k_wifi8_ppeds_attach,
	.ath12k_ppeds_detach = ath12k_wifi8_ppeds_detach,
	.ath12k_ppeds_register_soc = ath12k_wifi8_dp_ppeds_register_soc,
	.ath12k_ppeds_srng_cleanup = ath12k_wifi8_dp_srng_ppeds_cleanup,
	.ath12k_ppeds_interrupt_start = ath12k_wifi8_dp_ppeds_interrupt_start,
	.ath12k_ppeds_interrupt_stop = ath12k_wifi8_dp_ppeds_interrupt_stop,
	.ath12k_dp_ppeds_alloc_ppe_vp_profile =
				ath12k_wifi8_dp_ppeds_alloc_ppe_vp_profile,
	.ath12k_dp_ppeds_dealloc_ppe_vp_profile =
				ath12k_wifi8_dp_ppeds_dealloc_ppe_vp_profile,
	.ath12k_dp_ppeds_alloc_vp_tbl_entry = ath12k_wifi8_dp_ppeds_alloc_vp_tbl_entry,
	.ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry =
				ath12k_wifi8_dp_ppeds_alloc_vp_search_idx_tbl_entry,
	.ath12k_dp_ppeds_get_vp_profile_from_idx =
				ath12k_wifi8_dp_ppeds_get_vp_profile_from_idx,
	.ath12k_dp_ppeds_get_bank_lmac_id =
				ath12k_wifi8_dp_ppeds_get_bank_lmac_id,
};
