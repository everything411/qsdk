// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/bitfield.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/cacheflush.h>
#include "../hif.h"
#include "../hal.h"
#include "../debug.h"
#include "hw.h"
#include "dp.h"
#include "dp_tx.h"
#include "../ppe.h"
#include "ppeds.h"
#include "../ppe_public.h"
#include "../umac_reset.h"


static int ath12k_ppeds_wifi7_alloc_ppe_vp_profile(struct ath12k_base *ab,
				struct ath12k_dp_ppe_vp_profile **vp_profile,
				int vp_num)
{
	int i;

	/* If a VP is already allocated with requested vp number, then return
	 * the same VP instead of creating a new profile
	 */
	for (i = 0; i < HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX; i++) {
		if (ab->dp->ppe.ppe_vp_profile[i].is_configured &&
			vp_num == ab->dp->ppe.ppe_vp_profile[i].vp_num) {
			ath12k_dbg(ab, ATH12K_DBG_PPE,
				   "vp profile with num %d will be reused\n", vp_num);
			goto end;
		}
	}

	if (ab->dp->ppe.num_ppe_vp_profiles == HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	for (i = 0; i < HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX; i++) {
		if (!ab->dp->ppe.ppe_vp_profile[i].is_configured)
			break;
	}

	if (i == HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		WARN_ONCE(1, "All ppe vp profile entries are in use!");
		return -ENOSR;
	}
	ab->dp->ppe.num_ppe_vp_profiles++;

	ab->dp->ppe.ppe_vp_profile[i].is_configured = true;

end:
	ab->dp->ppe.ppe_vp_profile[i].ref_count++;
	*vp_profile = &ab->dp->ppe.ppe_vp_profile[i];
	return i;
}

static void
ath12k_dp_ppeds_dealloc_vp_search_idx_tbl_entry(struct ath12k_base *ab,
						int ppe_vp_search_idx)
{
	if (ppe_vp_search_idx < 0 ||
		ppe_vp_search_idx >= HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(ab, "Invalid PPE VP search table free index");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_PPE, "dealloc ppe_vp_search_idx %d\n",
		   ppe_vp_search_idx);

	if (!ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_search_idx]) {
		ath12k_err(ab, "PPE VP search idx table is not configured at idx:%d",
			   ppe_vp_search_idx);
		return;
	}

	ab->dp->ppe.ppe_vp_search_idx_tbl_set[ppe_vp_search_idx] = 0;
	ab->dp->ppe.num_ppe_vp_search_idx_entries--;
}

static void ath12k_dp_ppeds_dealloc_vp_tbl_entry(struct ath12k_base *ab,
						 int ppe_vp_num_idx)
{
	if (ppe_vp_num_idx < 0 || ppe_vp_num_idx >= HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(ab, "Invalid PPE VP free index");
		return;
	}

	/* Prevent register write if SOC is in recovery */
	if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))
		ath12k_dp_ppeds_tx_set_ppe_vp_entry(ab, NULL, ppe_vp_num_idx,
						    0, 0, 0);

	if (!ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_num_idx]) {
		ath12k_err(ab, "PPE VP is not configured at idx:%d", ppe_vp_num_idx);
		return;
	}

	ab->dp->ppe.ppe_vp_tbl_registered[ppe_vp_num_idx] = 0;
	ab->dp->ppe.num_ppe_vp_entries--;
}

static void
ath12k_ppeds_wifi7_dealloc_ppe_vp_profile(struct ath12k_base *ab,
				       int ppe_vp_profile_idx,
				       enum nl80211_iftype type)
{
	bool dealloced = false;
	struct ath12k_dp_ppe_vp_profile *vp_profile;

	if (ppe_vp_profile_idx < 0 ||
		ppe_vp_profile_idx >= HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(ab, "Invalid PPE VP profile free index");
		return;
	}

	vp_profile = &ab->dp->ppe.ppe_vp_profile[ppe_vp_profile_idx];

	if (!vp_profile->is_configured) {
		ath12k_err(ab, "PPE VP profile is not configured at idx:%d",
				ppe_vp_profile_idx);
		return;
	}

	vp_profile->ref_count--;

	if (!vp_profile->ref_count) {
		vp_profile->is_configured = false;
		ab->dp->ppe.num_ppe_vp_profiles--;
		dealloced = true;

		/* For STA mode ast index table reg also needs to be cleaned */
		if (type == NL80211_IFTYPE_STATION)
			ath12k_dp_ppeds_dealloc_vp_search_idx_tbl_entry(ab,
					vp_profile->search_idx_reg_num);

		ath12k_dp_ppeds_dealloc_vp_tbl_entry(ab, vp_profile->ppe_vp_num_idx);
	}

	if (dealloced)
		ath12k_dbg(ab, ATH12K_DBG_PPE, "%s success\n", __func__);
}

static int ath12k_ppeds_wifi7_alloc_vp_tbl_entry(struct ath12k_base *ab,
					int ppe_vp_profile_idx)
{
	int i;

	if (ab->dp->ppe.num_ppe_vp_profiles == HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	for (i = 0; i < HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX; i++) {
		if (!ab->dp->ppe.ppe_vp_tbl_registered[i])
			break;
	}

	if (i == HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		WARN_ONCE(1, "All ppe vp table entries are in use!");
		return -ENOSR;
	}

	ab->dp->ppe.num_ppe_vp_entries++;
	ab->dp->ppe.ppe_vp_tbl_registered[i] = 1;

	return i;
}

static int ath12k_ppeds_wifi7_alloc_vp_search_idx_tbl_entry(struct ath12k_base *ab,
						int ppe_vp_profile_idx)
{
	int i;

	if (ab->dp->ppe.num_ppe_vp_entries == HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(ab, "Maximum ppe_vp count reached for soc\n");
		return -ENOSR;
	}

	for (i = 0; i < HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX; i++) {
		if (!ab->dp->ppe.ppe_vp_search_idx_tbl_set[i])
			break;
	}

	if (i == HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		WARN_ONCE(1, "All ppe vp table entries are in use!");
		return -ENOSR;
	}

	ab->dp->ppe.num_ppe_vp_search_idx_entries++;
	ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 1;

	return i;
}

static int ath12k_dp_ppeds_tx_comp_poll(struct napi_struct *napi, int budget)
{
	struct ath12k_dp *dp = container_of(napi,
					struct ath12k_dp, ppe.ppeds_napi_ctxt.napi);
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

int ath12k_ppeds_wifi7_inst_attach(struct ath12k_base *ab)
{
	int i, ret, ds_node_id;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	ath12k_dp_ppeds_tx_cmem_init(ab);

	spin_lock_init(&ab->dp->ppe.ppe_vp_tbl_lock);

	for (i = 0; i < HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX; i++) {
		ab->dp->ppe.ppe_vp_tbl_registered[i] = 0;
		ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 0;
		memset(&ab->dp->ppe.ppe_vp_profile[i], 0,
		       sizeof(struct ath12k_dp_ppe_vp_profile));
	}

	ab->dp->ppe.num_ppe_vp_profiles = 0;
	ab->dp->ppe.num_ppe_vp_entries = 0;

	ds_node_id =
		ab->dp->ppe.nss_plugin_ops->ds_inst_alloc(ab->dp->ppe.ppeds_wlanops,
						sizeof(struct ath12k_base *));

	if (ds_node_id < 0 || ds_node_id == PPE_VP_DS_INVALID_NODE_ID) {
		ath12k_err(ab, "Failed to get DS node id for device_id %d\n",
			   ab->device_id);
		return -ENOSR;
	}
	ab->dp->ppe.ds_node_id = ds_node_id;
	ds_node_map[ds_node_id] = ab;
	ab->dp->ppe.ppeds_stopped = 0;

	WARN_ON(ab->dp->ppe.ppeds_soc_idx != -1);
	/* dec ppeds_soc_idx to start from 0 */
	ab->dp->ppe.ppeds_soc_idx = atomic_inc_return(&ath12k_num_ppeds_nodes) - 1;

	ath12k_info(ab,
		   "PPEDS attach ab %p ppeds_soc_idx %d ath12k_num_ppeds_nodes %d\n",
		   ab, ab->dp->ppe.ppeds_soc_idx,
		   atomic_read(&ath12k_num_ppeds_nodes));

	ret = ath12k_dp_ppeds_add_napi_ctxt(ab);
	if (ret)
		return -ENOSR;

	ath12k_info(ab, "PPEDS attach success\n");

	for (i = 0; i < ab->ag->num_devices; i++) {
		ab->dp->ppe.ppeds_rx_idx[i] =
				kzalloc((sizeof(u16) * PPE_DS_TXCMPL_DEF_BUDGET),
						      GFP_ATOMIC);
		if (!ab->dp->ppe.ppeds_rx_idx[i]) {
			ath12k_err(ab,
				   "ppeds_rx_idx alloc failed: ring=%d\n", i);
			goto err_ppeds_attach;
		}
	}

	ab->dp->ppe.ppeds_rx_num_elem = PPE_DS_TXCMPL_DEF_BUDGET;

	return 0;

err_ppeds_attach:
	ab->dp->ppe.ppeds_rx_num_elem = 0;
	for (i = i - 1; i >= 0; i--) {
		kfree(ab->dp->ppe.ppeds_rx_idx[i]);
		ab->dp->ppe.ppeds_rx_idx[i] = NULL;
	}

	return -ENOMEM;
}
EXPORT_SYMBOL(ath12k_ppeds_wifi7_inst_attach);

int ath12k_ppeds_wifi7_inst_detach(struct ath12k_base *ab)
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

	for (i = 0; i < HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX; i++) {
		ab->dp->ppe.ppe_vp_tbl_registered[i] = 0;
		ab->dp->ppe.ppe_vp_search_idx_tbl_set[i] = 0;
		memset(&ab->dp->ppe.ppe_vp_profile[i], 0,
		       sizeof(struct ath12k_dp_ppe_vp_profile));
	}

	ab->dp->ppe.num_ppe_vp_profiles = 0;
	ab->dp->ppe.num_ppe_vp_entries = 0;

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS detach success\n");

	if (ab->dp->ppe.ppeds_rx_num_elem) {
		ab->dp->ppe.ppeds_rx_num_elem = 0;
		for (i = 0; i < ab->ag->num_devices; i++) {
			kfree(ab->dp->ppe.ppeds_rx_idx[i]);
			ab->dp->ppe.ppeds_rx_idx[i] = NULL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_ppeds_wifi7_inst_detach);

int ath12k_ppeds_wifi7_register_soc(struct ath12k_dp *dp, struct dp_ppe_ds_idxs *idx)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k *ar = ab->pdevs[0].ar;
	struct hal_srng *ppe2tcl_ring, *reo2ppe_ring;
	struct ppe_ds_wlan_reg_info reg_info = {0};

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (!ab->dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, "nss_plugin_ops not registered for device_id %d\n",
			   ab->device_id);
		return -EOPNOTSUPP;
	}

	/*
	 * BUG-ON: Wi-Fi7 supports single ring.
	 */
	BUILD_BUG_ON(PPE2TCL_RING_WIFI7 != 0);

	ppe2tcl_ring =
		&ab->hal.srng_list[dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7].ring_id];
	reo2ppe_ring =
		&ab->hal.srng_list[dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7].ring_id];

	reg_info.ppe2tcl_ba = dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7].paddr;
	reg_info.reo2ppe_ba = dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7].paddr;
	reg_info.ppe2tcl_num_desc = DP_PPE2TCL_RING_SIZE;
	reg_info.reo2ppe_num_desc = DP_REO2PPE_RING_SIZE;

	if (ar) {
		spin_lock_bh(&ar->data_lock);
		reg_info.freq.low_freq = ar->chan_info.low_freq;
		reg_info.freq.high_freq = ar->chan_info.high_freq;
		spin_unlock_bh(&ar->data_lock);
	}

	if (ab->dp->ppe.nss_plugin_ops->ds_inst_register(&reg_info,
							 dp->ppe.ds_node_id) != true) {
		ath12k_err(ab, "ppeds not attached");
		return -EINVAL;
	}

	idx->ppe2tcl_start_idx = reg_info.ppe2tcl_start_idx;
	idx->reo2ppe_start_idx = reg_info.reo2ppe_start_idx;
	ab->dp->ppe.ppeds_int_mode_enabled = reg_info.ppe_ds_int_mode_enabled;

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS register soc-success device_id %d ppe2tcl_start_idx 0x%x reo2ppe_start_idx 0x%x",
		   ab->device_id, idx->ppe2tcl_start_idx, idx->reo2ppe_start_idx);

	return 0;
}

int ath12k_ppeds_wifi7_srng_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret, size;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	/*
	 * BUG-ON: Wi-Fi7 supports single ring.
	 */
	BUILD_BUG_ON(PPE2TCL_RING_WIFI7 != 0);

	/* TODO: Use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_alloc(ab, &dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7],
					 HAL_REO2PPE,
					 0, DP_REO2PPE_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo2ppe ring :%d\n", ret);
		goto err;
	}

	/* TODO: Use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_alloc(ab, &dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7],
					 HAL_PPE2TCL,
					 0, DP_PPE2TCL_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to set up ppe2tcl ring :%d\n", ret);
		goto err;
	}

	ret = ath12k_dp_srng_alloc(ab, &dp->ppe.ppeds_comp_ring.ppeds_txcmpl_ring,
				   HAL_WBM2SW_RELEASE,
				   HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM, 0,
				   DP_PPE_WBM2SW_RING_SIZE);
	if (ret) {
		ath12k_err(ab, "failed to alloc wbm2sw ppeds tx completion ring :%d\n",
			   ret);
		goto err;
	}

	size = ath12k_hal_srng_get_entrysize(ab, HAL_WBM2SW_RELEASE) *
					     DP_TX_COMP_PPEDS_RING_SIZE;
	dp->ppe.ppeds_comp_ring.tx_status_head = 0;
	dp->ppe.ppeds_comp_ring.tx_status_tail = DP_TX_COMP_PPEDS_RING_SIZE - 1;
	dp->ppe.ppeds_comp_ring.tx_status = kmalloc(size, GFP_KERNEL);

	if (!dp->ppe.ppeds_comp_ring.tx_status) {
		ath12k_err(ab, "PPE tx status completion buffer alloc failed\n");
		ret = -ENOMEM;
		goto err;
	}

	return 0;
err:
	ath12k_ppeds_wifi7_srng_cleanup(ab);
	return ret;
}

int ath12k_ppeds_wifi7_srng_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct dp_ppe_ds_idxs restore_idx = {0};
	int ret;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	/*
	 * BUG-ON: Wi-Fi7 supports single ring.
	 */
	BUILD_BUG_ON(PPE2TCL_RING_WIFI7 != 0);

	ret = ath12k_ppeds_wifi7_register_soc(dp, &restore_idx);
	if (ret) {
		ath12k_err(ab, "ppeds registration failed\n");
		goto err;
	}

	/* TODO: Use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_init(ab, &dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7],
					HAL_REO2PPE,
					0, 0, DP_REO2PPE_RING_SIZE,
					restore_idx.reo2ppe_start_idx);
	if (ret) {
		ath12k_warn(ab, "failed to set up reo2ppe ring :%d\n", ret);
		goto err;
	}

	ath12k_hal_reo_config_reo2ppe_dest_info(ab);

	/* TODO: Use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_ppeds_dp_srng_init(ab, &dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7],
					HAL_PPE2TCL,
					0, 0, DP_PPE2TCL_RING_SIZE,
					restore_idx.ppe2tcl_start_idx);
	if (ret) {
		ath12k_warn(ab, "failed to set up ppe2tcl ring :%d\n", ret);
		goto err;
	}

	/* TODO: Use ring idx fetched from ppe for avoiding edma hang during SSR */
	ret = ath12k_dp_srng_init(ab, &dp->ppe.ppeds_comp_ring.ppeds_txcmpl_ring,
				  HAL_WBM2SW_RELEASE,
				  HAL_WBM2SW_PPEDS_TX_CMPLN_RING_NUM, 0);
	if (ret) {
		ath12k_err(ab,
			    "failed to init wbm2sw ppeds tx completion ring :%d\n",
			    ret);
		goto err;
	}
	ath12k_hal_tx_config_rbm_mapping(ab, 0,
					 HAL_WBM2SW_PPEDS_TX_CMPLN_MAP_ID,
					 HAL_PPE2TCL);

err:
	/* caller takes care of calling ath12k_dp_srng_ppeds_cleanup */
	return ret;
}

void ath12k_ppeds_wifi7_srng_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	ath12k_dp_srng_cleanup(ab, &dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7]);
	ath12k_dp_srng_cleanup(ab, &dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7]);
	kfree(dp->ppe.ppeds_comp_ring.tx_status);
	dp->ppe.ppeds_comp_ring.tx_status = NULL;
	ath12k_dp_srng_cleanup(ab, &dp->ppe.ppeds_comp_ring.ppeds_txcmpl_ring);
}

void ath12k_ppeds_wifi7_interrupt_start(struct ath12k_base *ab)
{
	ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_REO2PPE);
	ath12k_hif_ppeds_irq_enable(ab, PPEDS_IRQ_TX_COMPLETION);
}

void ath12k_ppeds_wifi7_interrupt_stop(struct ath12k_base *ab)
{
	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_REO2PPE);
	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_TX_COMPLETION);
}

int ath12k_ppeds_wifi7_inst_start(struct ath12k_base *ab)
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

	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS start success device_id %d ds_node_id %d ppeds_soc_idx %d",
		   ab->device_id, ab->dp->ppe.ds_node_id, ab->dp->ppe.ppeds_soc_idx);
	return 0;
}
EXPORT_SYMBOL(ath12k_ppeds_wifi7_inst_start);

void ath12k_ppeds_wifi7_inst_stop(struct ath12k_base *ab)
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
EXPORT_SYMBOL(ath12k_ppeds_wifi7_inst_stop);

/* PPE-DS release interrupt */
irqreturn_t ath12k_ppeds_wifi7_handle_tx_comp(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;
	struct ath12k_ppeds_napi *napi_ctxt = &ab->dp->ppe.ppeds_napi_ctxt;

	ath12k_hif_ppeds_irq_disable(ab, PPEDS_IRQ_TX_COMPLETION);
	napi_schedule(&napi_ctxt->napi);
	return IRQ_HANDLED;
}

irqreturn_t ath12k_ppeds_wifi7_ppe2tcl_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_ppe2tcl_intr(ab->dp->ppe.ds_node_id);

	return IRQ_HANDLED;
}

irqreturn_t ath12k_ppeds_wifi7_reo2ppe_irq_handler(int irq, void *ctxt)
{
	struct ath12k_base *ab = (struct ath12k_base *)ctxt;

	if (ab->dp->ppe.nss_plugin_ops)
		ab->dp->ppe.nss_plugin_ops->ds_inst_reo2ppe_intr(ab->dp->ppe.ds_node_id);
	return IRQ_HANDLED;
}

void ath12k_ppeds_set_tcl_prod_idx_v2(int ds_node_id, u16 tcl_prod_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7].ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_prod_cnt++;

	srng->u.src_ring.hp = tcl_prod_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_ppeds_set_tcl_prod_idx_v2);

u16 ath12k_ppeds_get_tcl_cons_idx_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 tp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.tcl_cons_cnt++;

	srng = &ab->hal.srng_list[dp->ppe.ppe2tcl_ring[PPE2TCL_RING_WIFI7].ring_id];
	tp = READ_ONCE(*(u32 *)srng->u.src_ring.tp_addr);
	dma_rmb();

	return tp / srng->entry_size;
}
EXPORT_SYMBOL(ath12k_ppeds_get_tcl_cons_idx_v2);

void ath12k_ppeds_set_reo_cons_idx_v2(int ds_node_id, u16 reo_cons_idx)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7].ring_id];
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_cons_cnt++;

	srng->u.src_ring.hp = reo_cons_idx * srng->entry_size;
	ath12k_hal_srng_access_end(ab, srng);
}
EXPORT_SYMBOL(ath12k_ppeds_set_reo_cons_idx_v2);

u16 ath12k_ppeds_get_reo_prod_idx_v2(int ds_node_id)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_dp *dp = ab->dp;
	struct hal_srng *srng;
	u32 hp;

	srng = &ab->hal.srng_list[dp->ppe.reo2ppe_ring[REO2PPE_RING_WIFI7].ring_id];
	hp = READ_ONCE(*(u32 *)srng->u.dst_ring.hp_addr);
	dma_rmb();
	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.reo_prod_cnt++;
	return hp / srng->entry_size;
}
EXPORT_SYMBOL(ath12k_ppeds_get_reo_prod_idx_v2);

/* enable/disable PPE2TCL irq */
void ath12k_ppeds_enable_srng_intr_v2(int ds_node_id, bool enable)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

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
EXPORT_SYMBOL(ath12k_ppeds_enable_srng_intr_v2);

bool ath12k_ppeds_free_rx_desc_v2(struct ppe_ds_wlan_rxdesc_elem *arr,
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
		ath12k_core_dmac_inv_range_no_dsb(skb->data, skb->data + (skb->len +
						  skb_tailroom(skb)));
		ath12k_core_dma_unmap_single_attrs(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
						   skb->len + skb_tailroom(skb),
						   DMA_FROM_DEVICE,
						   DMA_ATTR_SKIP_CPU_SYNC);
	}

	skb->recycled_for_ds = 0;
	skb->fast_recycled = 0;
	dev_kfree_skb_any(skb);
	return true;
}
EXPORT_SYMBOL(ath12k_ppeds_free_rx_desc_v2);

int ath12k_wifi7_dp_rx_bufs_replenish_ppeds(struct ath12k_base *ab,
					    int req_entries, u16 *idx_of_ab,
					    struct ppe_ds_wlan_rxdesc_elem *arr)
{
	struct dp_rxdma_ring *rx_ring = &ab->dp->rx_refill_buf_ring;
	struct hal_srng *rxdma_srng;
	struct ath12k_buffer_addr *rxdma_desc;
	u32 cookie;
	dma_addr_t paddr;
	struct ath12k_rx_desc_info *rx_desc;
	int count = 0, num_remain, i;
	u8 mgr = ab->hal.hal_params->rx_buf_rbm;

	rxdma_srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];

	spin_lock_bh(&rxdma_srng->lock);
	ath12k_hal_srng_access_begin(ab, rxdma_srng);

	num_remain = req_entries;
	for (i = 0 ; i < req_entries; i++) {
		if ((i + 1) < (req_entries - 1))
			prefetch((struct ath12k_rx_desc_info *)
						arr[idx_of_ab[i + 1]].cookie);

		rx_desc = (struct ath12k_rx_desc_info *)arr[idx_of_ab[i]].cookie;
		if (!rx_desc)
			break;

		if (!rx_desc->skb) {
			ath12k_err(ab, "ppeds rx desc with no skb when reusing!\n");
			break;
		}

		cookie = rx_desc->cookie;
		paddr = rx_desc->paddr;

		rxdma_desc = ath12k_hal_srng_src_get_next_entry(ab, rxdma_srng);
		if (!rxdma_desc)
			break;

		ath12k_hal_rx_buf_addr_info_set(rxdma_desc, paddr, cookie, mgr);
		num_remain--;
	}

	ath12k_hal_srng_access_end(ab, rxdma_srng);
	spin_unlock_bh(&rxdma_srng->lock);

	/* move any remaining descriptors to free list */
	for (; i < req_entries; i++)
		count += ath12k_ppeds_free_rx_desc_v2(arr, ab, i, idx_of_ab);

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.num_rx_desc_freed += count;

	return 0;
}

void ath12k_ppeds_release_rx_desc_v2(int ds_node_id,
				     struct ppe_ds_wlan_rxdesc_elem *arr,
				     u16 count)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	struct ath12k_base *src_ab = NULL;
	struct ath12k_hw_group *ag = ab->ag;
	u32 rx_bufs_reaped[ATH12K_MAX_SOCS] = {0};
	struct ath12k_rx_desc_info *rx_desc;
	int device_id;
	u32 i = 0, new_size, num_free_desc;
	u16 *idx_of_ab;
	u16 *tmp;

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_rx_desc_cnt += count;

	if (unlikely(count > ab->dp->ppe.ppeds_rx_num_elem)) {
		new_size = sizeof(u16) * count;
		for (device_id = 0; device_id < ag->num_devices; device_id++) {
			tmp = krealloc(ab->dp->ppe.ppeds_rx_idx[device_id], new_size,
				       GFP_ATOMIC);
			if (!tmp) {
				ath12k_err(ab, "ppeds: rx desc realloc failed for size %u\n",
					   count);
				goto err_h_alloc_failure;
			}

			ab->dp->ppe.ppeds_rx_idx[device_id] = tmp;
		}

		ab->dp->ppe.ppeds_rx_num_elem = count;
		ab->dp->ppe.ppeds_stats.num_rx_desc_realloc += device_id;
	}

	for (i = 0; i < count; i++) {
		if ((i + 1) < (count - 1))
			prefetch((struct ath12k_rx_desc_info *)arr[i + 1].cookie);

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

	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		if (!rx_bufs_reaped[device_id])
			continue;

		src_ab = ag->ab[device_id];
		ath12k_wifi7_dp_rx_bufs_replenish_ppeds(src_ab, rx_bufs_reaped[device_id],
							&ppe->ppeds_rx_idx[device_id][0],
							arr);
	}

	return;

err_h_alloc_failure:
	for (device_id = 0; device_id < ag->num_devices; device_id++) {
		src_ab = ag->ab[device_id];
		idx_of_ab = &src_ab->dp->ppe.ppeds_rx_idx[device_id][0];
		num_free_desc = 0;
		for (i = 0; i < count; i++)
			num_free_desc +=
				ath12k_ppeds_free_rx_desc_v2(arr, src_ab, i, idx_of_ab);
		if (!src_ab->stats_disable)
			src_ab->dp->ppe.ppeds_stats.num_rx_desc_freed += num_free_desc;
	}
}
EXPORT_SYMBOL(ath12k_ppeds_release_rx_desc_v2);

void ath12k_ppeds_release_tx_desc_single_v2(int ds_node_id, u32 cookie)
{
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	if (!ab->stats_disable)
		ab->dp->ppe.ppeds_stats.release_tx_single_cnt++;
}
EXPORT_SYMBOL(ath12k_ppeds_release_tx_desc_single_v2);

u32 ath12k_ppeds_get_batched_tx_desc_v2(int ds_node_id,
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
				ath12k_core_dmac_inv_range_no_dsb((void *)skb->data,
								  ((void *)skb->data +
								  buff_size - headroom));
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
EXPORT_SYMBOL(ath12k_ppeds_get_batched_tx_desc_v2);

void ath12k_ppeds_notify_napi_done_v2(int ds_node_id)
{
	enum dp_umac_reset_tx_cmd tx_cmd = ATH12K_UMAC_RESET_TX_CMD_PRE_RESET_DONE;
	struct ath12k_base *ab = ds_node_map[ds_node_id];

	ath12k_umac_reset_notify_target_sync_and_send(ab, tx_cmd,
						      ab->dp->ppe.task_id);
}
EXPORT_SYMBOL(ath12k_ppeds_notify_napi_done_v2);

uint8_t  ath12k_ppeds_get_wlan_arch_mode(void)
{
	return PPEDS_ARCH_MODE_WIFI7;
}

struct ath12k_dp_ppe_vp_profile *
ath12k_wifi7_dp_ppeds_get_vp_profile_from_idx(struct ath12k_base *g_ab,
		uint32_t ppe_vp_idx)
{
	if (ppe_vp_idx >= HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX) {
		ath12k_err(g_ab, "Invalid vp_idx:%u\n", ppe_vp_idx);
		return NULL;
	}

	if (!g_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx].is_configured) {
		ath12k_err(g_ab, "vp_idx:%u not configured\n", ppe_vp_idx);
		return NULL;
	}

	return &g_ab->dp->ppe.ppe_vp_profile[ppe_vp_idx];
}

int ath12k_wifi7_dp_ppeds_get_bank_lmac_id(struct ath12k_base *ab,
		struct ath12k *ar,
		struct ath12k_link_vif *arvif,
		struct ath12k_dp_ppe_vp_profile *vp_profile,
		u8 *bank_id, u8 *lmac_id)
{
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_vif *dp_link_vif =
					&arvif->ahvif->dp_vif.dp_link_vif[link_id];

	if (vp_profile->ref_count == 1) {
		*lmac_id = ar->lmac_id;
		*bank_id = dp_link_vif->bank_id;
	} else {
		*lmac_id = HAL_WILDCARD_LMAC_ID;
		*bank_id = arvif->splitphy_ds_bank_id;
	}
	return 0;
}

struct ppe_ds_wlan_ops_v2 ppeds_wlanops_v2 = {
	.get_tx_desc_many = ath12k_ppeds_get_batched_tx_desc_v2,
	.release_tx_desc_single = ath12k_ppeds_release_tx_desc_single_v2,
	.enable_tx_consume_intr = ath12k_ppeds_enable_srng_intr_v2,
	.set_tcl_prod_idx  = ath12k_ppeds_set_tcl_prod_idx_v2,
	.set_reo_cons_idx = ath12k_ppeds_set_reo_cons_idx_v2,
	.get_tcl_cons_idx = ath12k_ppeds_get_tcl_cons_idx_v2,
	.get_reo_prod_idx = ath12k_ppeds_get_reo_prod_idx_v2,
	.release_rx_desc = ath12k_ppeds_release_rx_desc_v2,
	.notify_napi_done = ath12k_ppeds_notify_napi_done_v2,
	.get_wlan_arch_mode = ath12k_ppeds_get_wlan_arch_mode,
};

struct ath12k_ppeds_arch_ops ath12k_wifi7_arch_ppeds_ops  = {
	.ath12k_ppeds_ppe2tcl_irq_handler = ath12k_ppeds_wifi7_ppe2tcl_irq_handler,
	.ath12k_ppeds_reo2ppe_irq_handler = ath12k_ppeds_wifi7_reo2ppe_irq_handler,
	.ath12k_ppeds_ppe2tcl_tx_compln = ath12k_ppeds_wifi7_handle_tx_comp,
	.ath12k_ppeds_start = ath12k_ppeds_wifi7_inst_start,
	.ath12k_ppeds_stop = ath12k_ppeds_wifi7_inst_stop,
	.ath12k_ppeds_attach = ath12k_ppeds_wifi7_inst_attach,
	.ath12k_ppeds_detach = ath12k_ppeds_wifi7_inst_detach,
	.ath12k_ppeds_register_soc = ath12k_ppeds_wifi7_register_soc,
	.ath12k_ppeds_srng_cmn_init = ath12k_ppeds_wifi7_srng_init,
	.ath12k_ppeds_srng_cmn_alloc = ath12k_ppeds_wifi7_srng_alloc,
	.ath12k_ppeds_srng_cleanup = ath12k_ppeds_wifi7_srng_cleanup,
	.ath12k_ppeds_interrupt_start = ath12k_ppeds_wifi7_interrupt_start,
	.ath12k_ppeds_interrupt_stop = ath12k_ppeds_wifi7_interrupt_stop,
	.ath12k_dp_ppeds_alloc_ppe_vp_profile = ath12k_ppeds_wifi7_alloc_ppe_vp_profile,
	.ath12k_dp_ppeds_dealloc_ppe_vp_profile =
					ath12k_ppeds_wifi7_dealloc_ppe_vp_profile,
	.ath12k_dp_ppeds_alloc_vp_tbl_entry = ath12k_ppeds_wifi7_alloc_vp_tbl_entry,
	.ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry =
					ath12k_ppeds_wifi7_alloc_vp_search_idx_tbl_entry,
	.ath12k_dp_ppeds_get_vp_profile_from_idx =
				ath12k_wifi7_dp_ppeds_get_vp_profile_from_idx,
	.ath12k_dp_ppeds_get_bank_lmac_id =
				ath12k_wifi7_dp_ppeds_get_bank_lmac_id,
};
