// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../core.h"
#include <linux/mm.h>
#include <linux/io.h>
#include "../debug.h"
#include "../dp_rx.h"
#include "../dp_tx.h"
#include "../dp_peer.h"
#include "../hif.h"
#include "../dp_cmn.h"
#include "../hal.h"
#include "../dp_tx_mon.h"
#include "dp_rx.h"
#include "dp.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "dp_telemetry.h"
#include "hal.h"
#include "dp_peer.h"
#include "dp_ast.h"
#include "dp_pool.h"
#include "dp_htt.h"
#include "dp_tx_flow_info.h"
#include "dp_mon.h"
#include "umac_reset.h"
#include "mgmt_rx.h"
#include "dp_peer.h"
#include "hal_queue.h"
#include "ppeds.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ipa/dp_ipa_pub.h"
#endif /* CPTCFG_QCN_EXTN */

extern struct ppe_ds_wlan_ops_v2 ppeds_wifi8_wlanops_v2;

static int ath12k_wifi8_non_cumac_dp_service_srng(struct ath12k_dp *dp,
						  struct ath12k_ext_irq_grp *irq_grp,
						  int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0, j;
	int tot_work_done = 0;
	u8 ring_mask;

	if (dp->hw_params->ring_mask->rx_mon_dest[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->rx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			for (j = 0; j < dp->hw_params->num_rxdma_per_pdev; j++) {
				int id = i * dp->hw_params->num_rxdma_per_pdev + j;

				if (ring_mask & BIT(id)) {
					work_done =
					ath12k_dp_rx_mon_process_ring(dp, id,
								      napi, budget);
					budget -= work_done;
					tot_work_done += work_done;

					if (budget <= 0)
						goto done;
				}
			}
		}
	}

	if (dp->hw_params->ring_mask->tx_mon_dest[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->tx_mon_dest[grp_id];
		for (i = 0; i < dp->num_radios; i++) {
			int mac_id = i;

			if (ring_mask & BIT(mac_id)) {
				work_done =
				ath12k_dp_tx_mon_process_ring(dp, mac_id, napi,
							      budget);
				budget -= work_done;
				tot_work_done += work_done;

				if (budget <= 0)
					goto done;
			}
		}
	}

	if (dp->hw_params->ring_mask->ase_status[grp_id]) {
		work_done = ath12k_wifi8_dp_rx_ase_cmd_status_handler(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->host2rxmon[grp_id])
		ath12k_dp_mon_rx_process_low_thres(dp);

	if (dp->hw_params->ring_mask->host2txmon[grp_id])
		ath12k_dp_mon_tx_process_low_thres(dp);

done:
	return tot_work_done;
}

static int ath12k_wifi8_cumac_dp_service_srng(struct ath12k_dp *dp,
					      struct ath12k_ext_irq_grp *irq_grp,
					      int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0;
	int tot_work_done = 0;
	u8 rx_mask, tx_mask;

	/* Return early if UMAC reset is in progress */
	if (ath12k_dp_umac_reset_in_progress(dp->ab))
		return 0;

	rx_mask = dp->hw_params->ring_mask->rx[grp_id];
	tx_mask = dp->hw_params->ring_mask->tx[grp_id];

	if (dp->hw_params->ring_mask->tx_exception[grp_id]) {
		work_done = ath12k_wifi8_dp_tx_exception_handler(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->tcl_status[grp_id]) {
		work_done = ath12k_wifi8_dp_tx_cmd_status_handler(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->tqm_status[grp_id]) {
		work_done = ath12k_wifi8_dp_tx_process_tqm_status(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	/*
	 * Group 14: dedicated interrupt for fw_tqm2sw_ring.
	 * C-TQM routes FW-owned buffer completions here when
	 * HAL_TQM_HOST_STATUS_RING=1 is set in the SW2TQM sync command.
	 * This is a V1-only hardware WAR; skip entirely on V2 and later.
	 */
	if (dp->hw_params->tqm2sw_fw_war &&
	    dp->hw_params->ring_mask->tqm2sw_fw[grp_id]) {
		work_done = ath12k_wifi8_dp_tqm2sw_fw_handler(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->sam_status[grp_id]) {
		work_done = ath12k_wifi8_dp_tx_process_sam_status(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	while (tx_mask) {
		i = fls(tx_mask) - 1;
		tx_mask ^= 1 << i;
		work_done = ath12k_wifi8_dp_tx_completion_handler(dp, i, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	while (rx_mask) {
		i = fls(rx_mask) - 1;
		rx_mask ^= 1 << i;
		work_done = ath12k_wifi8_dp_rx_process(dp, i, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_err[grp_id]) {
		work_done = ath12k_wifi8_dp_rx_process_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->reo_status[grp_id]) {
		work_done = ath12k_wifi8_dp_rx_process_reo_status(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->reo_flush[grp_id]) {
		work_done = ath12k_wifi8_dp_rx_process_reo_flush_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->tx_peer_telemetry[grp_id]) {
		work_done = ath12k_wifi8_dp_process_tx_peer_telemetry(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_peer_telemetry[grp_id]) {
		work_done = ath12k_wifi8_dp_process_rx_peer_telemetry(dp, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	tot_work_done += ath12k_wifi8_non_cumac_dp_service_srng(dp, irq_grp, budget);
done:
	return tot_work_done;
}

static int ath12k_wifi8_dp_reoq_lut_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int ret;

	if (!ab->hw_params->reoq_lut_support)
		return 0;

	ret = ath12k_dp_alloc_reoq_lut(ab, &dp->reoq_lut);
	if (ret) {
		ath12k_warn(ab, "failed to allocate memory for reoq table");
		return ret;
	}

	return 0;
}

static int ath12k_wifi8_dp_reoq_lut_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!ab->hw_params->reoq_lut_support)
		return 0;

	/* Bits in the register have address [39:8] LUT base address to be
	 * allocated such that LSBs are assumed to be zero. Also, current
	 * design supports paddr up to 4 GB max hence it fits in 32 bit register only
	 */
	ath12k_hal_write_reoq_lut_addr(ab, dp->reoq_lut.paddr >> 8);
	ath12k_hal_write_ml_reoq_lut_addr(ab, dp->reoq_lut.paddr >> 8);

	ath12k_hal_reoq_lut_addr_read_enable(ab);
	ath12k_hal_reoq_lut_set_max_peerid(ab);

	return 0;
}

static void ath12k_wifi8_dp_reoq_lut_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	if (!ab->hw_params->reoq_lut_support)
		return;

	if (dp->reoq_lut.vaddr_unaligned) {
		ath12k_hal_dma_free_coherent(ab->dev, dp->reoq_lut.size,
					     dp->reoq_lut.vaddr_unaligned,
					     dp->reoq_lut.paddr_unaligned);
		dp->reoq_lut.vaddr_unaligned = NULL;
	}
}

static void ath12k_wifi8_dp_umac_free(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	if (dp_wifi8->init_done) {
		ath12k_err(ab, "Umac free called before umac deinit");
		WARN_ON_ONCE(1);
		return;
	}

	if (!dp_wifi8->alloc_done)
		return;

#ifdef CPTCFG_EXT_IPA_OFFLOAD
	ath12k_dp_ipa_plugin_deregister_ops_extn(dp->ab);
#endif

	ath12k_wifi8_dp_rx_ring_cleanup(ab);
	ath12k_wifi8_dp_reoq_lut_cleanup(ab);
	ath12k_wifi8_dp_telemetry_ring_cleanup(ab);
	ath12k_wifi8_dp_tx_ring_cleanup(ab);
	ath12k_dp_srng_common_cleanup(ab);
	ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);
	ath12k_wbm_idle_ring_cleanup(ab);
	ath12k_dp_cc_rx_free(ab);
	ath12k_dp_bank_profiles_free(ab);

	dp_wifi8->alloc_done = false;
	ath12k_info(ab, "CUMAC de-alloc successful");
}

static void ath12k_wifi8_dp_umac_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8 =
				ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);

	if (!dp_wifi8->init_done) {
		ath12k_info(ab, "DP de-init is already complete. Skip de-init");
		return;
	}

	if (!ab->is_cumac_chip) {
		ath12k_warn(ab, "Skipping ring deinit for non-cumac target");
		dp_wifi8->init_done = false;
		ath12k_hif_ext_irq_cleanup(ab);
		return;
	}

	ath12k_wifi8_dp_tx_congestion_control_deinit(dp);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops && dp->ppe.ppe_ops->ath12k_ppeds_stop)
		dp->ppe.ppe_ops->ath12k_ppeds_stop(ab);
	if (ab->dp->ppe.ppe_ops && dp->ppe.ppe_ops->ath12k_ppeds_detach)
		dp->ppe.ppe_ops->ath12k_ppeds_detach(ab);
	ath12k_nss_plugin_unregister_ops(ab);
#endif

	ath12k_dp_cc_deinit(ab);
	ath12k_dp_deinit_bank_profiles(ab);
	ath12k_dp_rx_reo_cmd_list_cleanup(ab);
	ath12k_wifi8_dp_tx_tqm_cmd_list_cleanup(ab);
	ath12k_wifi8_dp_telemetry_deinit(dp);
	ath12k_dp_pn_counter_page_free(dp->dp_hw_grp);
	ath12k_wifi8_dp_tx_pool_destroy(dp->dp_hw_grp);
	ath12k_dp_ast_table_deinit(dp->dp_hw_grp);
	ath12k_dp_ast_table_free(dp->dp_hw_grp);

	atomic_set(&dp_hw_group_wifi8->retry_work_active, 0);
	cancel_delayed_work_sync(&dp_hw_group_wifi8->dp_htt_retry_dwork);
	del_timer_sync(&dp_hw_group_wifi8->hw_grp_timer);
	if (dp_hw_group_wifi8->mec_timer_key)
		ath12k_dp_hw_group_del_timer_entry(dp->dp_hw_grp,
						   dp_hw_group_wifi8->mec_timer_key);
	dp_wifi8->init_done = false;
	dp_hw_group_wifi8->cumac_dp = NULL;
	ath12k_hif_ext_irq_cleanup(ab);
	ath12k_info(ab, "CUMAC de-init successful");
}

static void ath12k_wifi8_set_ext_irq_affinity(struct ath12k_dp *dp)
{
	int i, j, ret;

	for (i = 0; i < ARRAY_SIZE(ath12k_wifi8_ext_irq_grp_affinity); i++) {
		struct ath12k_ext_irq_grp *irq_grp = &dp->ab->ext_irq_grp[i];
		int cpu = ath12k_wifi8_ext_irq_grp_affinity[i];

		if (!cpu_online(cpu)) {
			ath12k_err(dp->ab,
				   "ext irq grp %d: cpu%d is offline, skipping affinity\n",
				   i, cpu);
			continue;
		}

		for (j = 0; j < irq_grp->num_irq; j++) {
			int irq = dp->ab->irq_num[irq_grp->irqs[j]];

			ret = irq_set_affinity(irq, cpumask_of(cpu));
			if (ret)
				ath12k_err(dp->ab,
					   "ext irq grp %d: failed to set affinity for irq %d to cpu%d, ret %d\n",
					   i, irq, cpu, ret);
		}
	}
}

static int
ath12k_wifi8_enable_hif_interrupts(struct ath12k_dp *dp,
				   int (*handler)(struct ath12k_dp *dp,
						  struct ath12k_ext_irq_grp *grp,
						  int budget))
{
	int ret;

	ret = ath12k_hif_ext_irq_setup(dp->ab, handler, dp);
	if (ret) {
		ath12k_warn(dp, "hif ext irq setup failed %d\n", ret);
		return ret;
	}

	ath12k_wifi8_set_ext_irq_affinity(dp);

	return 0;
}

static int ath12k_wifi8_dp_umac_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct hal_srng *srng = NULL;
	u32 n_link_desc = 0;
	int ret;

	if (dp_wifi8->alloc_done) {
		ath12k_info(ab, "DP alloc already done. Skip re-alloc");
		return 0;
	}

	if (!ab->is_cumac_chip) {
		ath12k_info(ab, "Skip DP allocation for non-cumac");
		return 0;
	}

	ath12k_dp_init_ring_size(ab);

	ret = ath12k_wbm_idle_ring_alloc(ab, &n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to alloc wbm_idle_ring: %d\n", ret);
		return ret;
	}

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];
	n_link_desc = dp->wbm_idle_ring.num_entries;

	ret = ath12k_dp_link_desc_alloc(ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, srng,
					n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to alloc link desc: %d\n", ret);
		goto fail_wbm_idle_ring_cleanup;
	}

	ret = ath12k_dp_cc_rx_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to alloc rx cookie converter %d\n", ret);
		goto fail_link_desc_cleanup;
	}

	ret = ath12k_dp_bank_profiles_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup bank profiles %d\n", ret);
		goto fail_dp_cc_rx_free;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/*
	 * Set the Auto index and hw buffer manager.
	 */
	if (ath12k_ppeds_txrx_hw_auto_idx &&
	    ab->dp->hw_params->ds_txrx_hw_auto_idx) {
		ab->dp->ppe.txrx_hw_auto_idx = 1;
	}

	if (ath12k_ppeds_hw_buff_mgmt &&
	    ab->dp->hw_params->ds_hw_buff_mgmt) {
		ab->dp->ppe.hw_buff_mgmt = 1;
	}
#endif

	ret = ath12k_dp_srng_common_alloc(ab);
	if (ret)
		goto fail_dp_bank_profiles_free;

	ret = ath12k_wifi8_dp_tx_ring_alloc(ab);
	if (ret)
		goto fail_cmn_srng_cleanup;

	ret = ath12k_wifi8_dp_telemetry_ring_alloc(ab);
	if (ret)
		goto fail_tx_ring_cleanup;

	ret = ath12k_wifi8_dp_reoq_lut_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to alloc reoq table %d\n", ret);
		goto fail_telemetry_ring_cleanup;
	}

	ret = ath12k_wifi8_dp_rx_ring_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "rx alloc failed ret = %d\n", ret);
		goto fail_reoq_lut_cleanup;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_wifi8_dp_srng_ppeds_alloc(ab);
	if (ret) {
		ath12k_warn(ab, "failed to alloc ppe-ds srngs :%d\n", ret);
		goto fail_rx_ring_cleanup;
	}
#endif

	dp_wifi8->alloc_done = true;
	ath12k_info(ab, "CUMAC alloc successful");
	return 0;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
fail_rx_ring_cleanup:
	ath12k_wifi8_dp_rx_ring_cleanup(ab);
#endif

fail_reoq_lut_cleanup:
	ath12k_wifi8_dp_reoq_lut_cleanup(ab);

fail_telemetry_ring_cleanup:
	ath12k_wifi8_dp_telemetry_ring_cleanup(ab);

fail_tx_ring_cleanup:
	ath12k_wifi8_dp_tx_ring_cleanup(ab);

fail_cmn_srng_cleanup:
	ath12k_dp_srng_common_cleanup(ab);

fail_dp_bank_profiles_free:
	ath12k_dp_bank_profiles_free(ab);

fail_dp_cc_rx_free:
	ath12k_dp_cc_rx_free(ab);

fail_link_desc_cleanup:
	ath12k_dp_link_desc_cleanup(ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);

fail_wbm_idle_ring_cleanup:
	ath12k_wbm_idle_ring_cleanup(ab);

	return ret;
}

static int ath12k_wifi8_dp_umac_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_group_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	struct ath12k_dp_hw_grp_timer_entry_param timer_param = {0};
	struct hal_srng *srng;
	u32 n_link_desc;
	int i, ret;

	if (dp_wifi8->init_done) {
		ath12k_info(ab, "DP init is already done. Skip re-init");
		return 0;
	}

	if (ab->is_cumac_chip) {
		ath12k_info(ab, "chip_id: %d is CUMAC\n", ab->device_id);
	} else {
		ath12k_wifi8_enable_hif_interrupts(dp, ath12k_wifi8_non_cumac_dp_service_srng);
		dp_wifi8->init_done = true;
		ath12k_hal_tx_set_pcp_tid_map(ab, dp->dp_hw_grp->pcp_tid_map);
		ath12k_hal_tx_set_tid_map_precedence
			(ab, dp->dp_hw_grp->tid_map_precedence);
		ath12k_info(ab, "Skipping ring init for non-cumac target");
		return 0;
	}

	if (dp_hw_group_wifi8->cumac_dp) {
		ath12k_info(ab, "CUMAC init is already done. Skip re-init");
		ath12k_hal_tx_set_pcp_tid_map(ab, dp->dp_hw_grp->pcp_tid_map);
		ath12k_hal_tx_set_tid_map_precedence
			(ab, dp->dp_hw_grp->tid_map_precedence);
		dp_wifi8->init_done = true;
		return 0;
	}

	INIT_LIST_HEAD(&dp->reo_cmd_list);
	INIT_LIST_HEAD(&dp->reo_cmd_cache_flush_list);
	INIT_LIST_HEAD(&dp->reo_cmd_update_rx_queue_list);
	spin_lock_init(&dp->reo_cmd_update_rx_queue_lock);
	spin_lock_init(&dp->reo_cmd_lock);
	INIT_LIST_HEAD(&dp->tqm_cmd_list);
	spin_lock_init(&dp->tqm_cmd_lock);

	dp->reo_cmd_cache_flush_count = 0;
	atomic_set(&dp->tqm_cmd_num, 0);
	atomic_set(&dp_wifi8->sam_cmd_num, 0);
	dp->idle_link_rbm =
			ath12k_hal_get_idle_link_rbm(&ab->hal, ab->device_id);

	ret = ath12k_wbm_idle_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to init wbm_idle_ring: %d\n", ret);
		return ret;
	}

	n_link_desc = ab->dp->wbm_idle_ring.num_entries;
	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];

	/* memset wbm link desc pool to 0 before desc_setup */
	ath12k_dp_clear_link_desc_pool(dp);

	ret = ath12k_dp_link_desc_init(ab, dp->link_desc_banks,
				       HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to init link desc: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_cc_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup cookie converter %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_init_bank_profiles(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup bank profiles %d\n", ret);
		goto fail_hw_cc_deinit;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_nss_plugin_register_ops(ab);
	if (ret) {
		ath12k_warn(ab, "failed to register nss plugin %d\n", ret);
		goto fail_bank_profiles_deinit;
	}

	if (ab->dp->ppe.ppe_ops && dp->ppe.ppe_ops->ath12k_ppeds_attach) {
		ret = dp->ppe.ppe_ops->ath12k_ppeds_attach(ab);
		if (ret) {
			ath12k_warn(ab, "failed to attach PPE DS %d\n", ret);
			goto fail_nss_plugin_register;
		}
	}
#endif

	ath12k_wifi8_hal_tx_configure_bank_register_default(ab);

	ret = ath12k_dp_srng_common_init(ab);
	if (ret)
		goto fail_ppeds_attach;

	ret = ath12k_wifi8_dp_tx_ring_init(ab);
	if (ret)
		goto fail_ppeds_attach;

	for (i = 0; i < HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX; i++)
		ath12k_hal_tx_set_dscp_tid_map(ab, ath12k_default_dscp_tid_map, i);

	ret = ath12k_wifi8_dp_reoq_lut_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to init reoq table %d\n", ret);
		goto fail_ppeds_attach;
	}

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		dp->tx_ring[i].tcl_data_ring_id = i;

	for (i = 0; i < HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX; i++)
		ath12k_hal_tx_set_dscp_tid_map(ab, ath12k_default_dscp_tid_map, i);

	ath12k_hal_tx_set_pcp_tid_map(ab, dp->dp_hw_grp->pcp_tid_map);
	ath12k_hal_tx_set_tid_map_precedence(ab, dp->dp_hw_grp->tid_map_precedence);

	ret = ath12k_wifi8_dp_rx_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "rx init failed ret = %d\n", ret);
		goto fail_ppeds_attach;
	}

	ret = ath12k_wifi8_dp_telemetry_umac_init(ab);
	if (ret) {
		ath12k_warn(ab, "telemetry init failed ret = %d\n", ret);
		goto fail_ppeds_attach;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_wifi8_dp_srng_ppeds_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to init ppe-ds srngs :%d\n", ret);
		goto fail_ppeds_attach;
	}
#endif

	/* Initialize cumac pointer in hw_group */
	dp_hw_group_wifi8->cumac_dp = dp;

	INIT_LIST_HEAD(&dp_hw_group_wifi8->timer_list_head);
	spin_lock_init(&dp_hw_group_wifi8->hw_grp_timer_lock);
	timer_setup(&dp_hw_group_wifi8->hw_grp_timer,
		    ath12k_dp_hw_group_timer_fn, 0);

	INIT_LIST_HEAD(&dp_hw_group_wifi8->mec_entry_list_head);

	timer_param.timeout_ms = ATH12K_MEC_TIMEOUT_MS;
	timer_param.callback = ath12k_mec_entry_expire_handler;
	ret = ath12k_dp_hw_group_add_timer_entry(dp->dp_hw_grp, &timer_param);
	if (ret) {
		ath12k_warn(ab, "failed to setup mec timer ret = %d\n", ret);
		del_timer_sync(&dp_hw_group_wifi8->hw_grp_timer);
		goto fail_ppeds_attach;
	}
	dp_hw_group_wifi8->mec_timer_key = timer_param.key_value;

	ret = ath12k_wifi8_dp_rx_flow_fse_cache_operation(ab,
							  DP_FST_CACHE_INVALIDATE_FULL,
							  NULL);
	if (ret) {
		ath12k_err(ab, "Unable to invalidate Full cache ret %d", ret);
		goto fail_ppeds_attach;
	}

	ret = ath12k_dp_ast_table_alloc(dp);
	if (ret) {
		ath12k_warn(ab, "dp ast table alloc failed %d\n", ret);
		goto fail_ppeds_attach;
	}

	ret = ath12k_dp_ast_table_init(dp->dp_hw_grp);
	if (ret) {
		ath12k_warn(ab, "dp ast table init failed %d\n", ret);
		goto fail_ast_table_free;
	}

	ret = ath12k_dp_pn_counter_page_init(dp->dp_hw_grp);
	if (ret) {
		ath12k_warn(ab, "dp pn counter page init failed %d\n", ret);
		goto fail_ast_table_free;
	}

	ret = ath12k_wifi8_dp_tx_pool_create(dp->dp_hw_grp);
	if (ret) {
		ath12k_warn(ab, "dp pool create for queues failed %d\n", ret);
		goto fail_pn_counter_page_free;
	}

	ret = ath12k_wifi8_dp_tx_congestion_control_init(dp);
	if (ret) {
		ath12k_warn(ab, "dp congestion control init failed %d\n", ret);
		goto fail_pool_destroy;
	}

	ret = ath12k_wifi8_dp_telemetry_init(dp);
	if (ret) {
		ath12k_warn(ab, "dp telemetry init failed %d\n", ret);
		goto fail_congestion_control;
	}

	spin_lock_init(&dp_hw_group_wifi8->htt_cmd_retry_lock);
	INIT_DELAYED_WORK(&dp_hw_group_wifi8->dp_htt_retry_dwork,
			  ath12k_dp_tx_htt_retry_work);
	atomic_set(&dp_hw_group_wifi8->retry_work_active, 1);

	ath12k_wifi8_enable_hif_interrupts(dp, ath12k_wifi8_cumac_dp_service_srng);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops &&
		ab->dp->ppe.ppe_ops->ath12k_ppeds_start) {
		ret = dp->ppe.ppe_ops->ath12k_ppeds_start(ab);
		if (ret) {
			ath12k_err(ab, "failed to start DP PPEDS\n");
			goto fail_htt_retry;
		}
	}
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_start(ab);
#endif

#ifdef CPTCFG_QCN_EXTN
	ret = ath12k_wifi8_dp_ipa_init(ab);
	if (ret) {
		ath12k_err(ab, "IPA: ipa init failed");
		goto fail_congestion_control;
	}
#endif /* CPTCFG_QCN_EXTN */

	dp_wifi8->init_done = true;
	ath12k_info(ab, "CUMAC init successful");
	return 0;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
fail_htt_retry:
	atomic_set(&dp_hw_group_wifi8->retry_work_active, 0);
	cancel_delayed_work_sync(&dp_hw_group_wifi8->dp_htt_retry_dwork);
	ath12k_wifi8_dp_telemetry_deinit(dp);
#endif

fail_congestion_control:
	ath12k_wifi8_dp_tx_congestion_control_deinit(dp);

fail_pool_destroy:
	ath12k_wifi8_dp_tx_pool_destroy(dp->dp_hw_grp);

fail_pn_counter_page_free:
	ath12k_dp_pn_counter_page_free(dp->dp_hw_grp);

fail_ast_table_free:
	ath12k_dp_ast_table_free(dp->dp_hw_grp);

fail_ppeds_attach:
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops && dp->ppe.ppe_ops->ath12k_ppeds_detach)
		dp->ppe.ppe_ops->ath12k_ppeds_detach(ab);

fail_nss_plugin_register:
	ath12k_nss_plugin_unregister_ops(ab);

fail_bank_profiles_deinit:
#endif
	ath12k_dp_deinit_bank_profiles(ab);
fail_hw_cc_deinit:
	ath12k_dp_cc_deinit(ab);


	return ret;
}

static int ath12k_wifi8_dp_umac_setup(struct ath12k_dp *dp)
{
	int ret;

	ret = ath12k_wifi8_dp_umac_alloc(dp);
	if (ret)
		return ret;

	ret = ath12k_wifi8_dp_umac_init(dp);
	if (ret) {
		ath12k_wifi8_dp_umac_free(dp);
		return ret;
	}

	return 0;
}

void ath12k_wifi8_srng_hw_ring_disable(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->tx_exception);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->tcl_status_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->tcl_cmd_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->tqm_status_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->tqm_cmd_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->rx_ase_cmd_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->rx_ase_status_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->fse_cmd_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->reo_flush_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->tx_peer_telemetry_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->rx_peer_telemetry_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->sam_cmd_ring);
	ath12k_dp_srng_hw_disable(ab, &dp_wifi8->sam_status_ring);
}

int ath12k_ase_srng_setup(struct ath12k_dp *dp)
{
	int ret;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	ret = ath12k_dp_srng_alloc(ab, &dp_wifi8->rx_ase_status_ring, HAL_ASE_STATUS_RING,
				   0, 0, DP_RX_ASE_STATUS_RING_SIZE);
	if (ret) {
		ath12k_warn(ab, "failed to alloc ase srng: %d\n", ret);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, &dp_wifi8->rx_ase_status_ring, HAL_ASE_STATUS_RING,
				  0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init ase srng: %d\n", ret);
		ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_ase_status_ring);
		return ret;
	}

	return 0;
}

static int ath12k_wifi8_dp_op_device_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	int ret;

	dp->tcl_metadata_ver = HTT_OPTION_TCL_METADATA_VER_V3;
	dp->htt_tx_mon_cfg_msg_size =
			(u16)sizeof(struct htt_tx_mon_ring_selection_cfg_cmd);
	dp->htt_tx_mon_cfg_version = 1; /* wifi8 (Boron) extended message layout */

	dp_wifi8->dp_ppe2wbm_use_dedicated_pool = DP_PPE2WBM_DEDICATED_POOL;
	if (dp_wifi8->dp_ppe2wbm_use_dedicated_pool)
		dp_wifi8->num_ppe2wbm_refill_rings = DP_PPE2WBM_REFILL_RING_MAX;
	else
		dp_wifi8->num_ppe2wbm_refill_rings = 1;

	ret = ath12k_dp_mon_rx_init(dp);
	if (ret) {
		ath12k_warn(ab, "failed to init rxdma rings ret = %d\n", ret);
		goto fail_dp_mon_rx_deinit;
	}

	ret = ath12k_ase_srng_setup(dp);
	if (ret) {
		ath12k_warn(dp->ab, "failed to setup ase status ring : %d\n", ret);
		goto fail_dp_mon_rx_deinit;
	}

	ret = ath12k_dp_mon_tx_srng_init(dp);
	if (ret) {
		ath12k_warn(dp->ab, "Tx Mon: failed to setup rings ret = %d\n", ret);
		goto fail_dp_ase_ring_free;
	}

	return 0;

fail_dp_ase_ring_free:
	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_ase_status_ring);
fail_dp_mon_rx_deinit:
	ath12k_dp_mon_rx_deinit(dp);
	return ret;
}

static void ath12k_wifi8_dp_op_device_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	if (!ab || ab->is_bypassed)
		return;

	ath12k_dp_srng_cleanup(ab, &dp_wifi8->rx_ase_status_ring);
	ath12k_dp_mon_tx_srng_deinit(dp);
	ath12k_dp_mon_rx_deinit(dp);
}

static int ath12k_wifi8_dp_op_mlo_init(struct ath12k_dp *dp)
{
	int ret;

	if (dp->ab->is_bypassed)
		return 0;

	ret = ath12k_wifi8_dp_umac_setup(dp);

	if (ret) {
		ath12k_warn(dp, "dp umac setup failed %d\n", ret);
		return ret;
	}

	return 0;
}

static void ath12k_wifi8_dp_op_mlo_deinit(struct ath12k_dp *dp)
{
	ath12k_wifi8_dp_umac_deinit(dp);
}

static struct ath12k_dp_hw_group *ath12k_wifi8_dp_hw_group_alloc(void)
{
	struct ath12k_dp_hw_group *dp_hw_grp;
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	int i;

	dp_hw_grp = kzalloc(sizeof(*dp_hw_grp) + sizeof(*dp_hw_grp_wifi8), GFP_KERNEL);
	if (!dp_hw_grp)
		return NULL;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	dp_hw_grp_wifi8->dp_hw_grp = dp_hw_grp;
	for (i = 0; i < ATH12K_MAX_STATS_ID; i++) {
		dp_hw_grp_wifi8->stats_id_map[i].dp_peer_id =
			ATH12K_MLO_PEER_ID_INVALID;
		dp_hw_grp_wifi8->stats_id_map[i].tid = ATH12K_INVALID_TID;
		dp_hw_grp_wifi8->stats_id_map[i].hw_link_id =
			ATH12K_DP_HW_LINK_ID_INVALID;
	}

	memcpy(dp_hw_grp->pcp_tid_map, ath12k_default_pcp_tid_map,
	       sizeof(dp_hw_grp->pcp_tid_map));
	/* Initialize SMD transition lock */
	spin_lock_init(&dp_hw_grp->smd_transition_lock);
	dp_hw_grp->smd_parked_ext_ctx = NULL;
	dp_hw_grp->smd_old_peer_id = 0;
	eth_zero_addr(dp_hw_grp->smd_target_mld_addr);

	return dp_hw_grp;
}

static void ath12k_wifi8_dp_vif_configure(struct ath12k_dp *dp,
					  struct ath12k_vif *ahvif,
					  enum ath12k_dp_op_type optype)
{
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	u32 old_bank_config, new_bank_config;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp *central_dp;
	struct ath12k_base *central_ab =
		ath12k_dp_get_ab_from_dp_hw_group(dp->dp_hw_grp);
	int bank_id;

	if (!central_ab) {
		ath12k_err(dp->ab, "Central umac not configured unable to dp_vif");
		return;
	}

	central_dp = central_ab->dp;
	ath12k_dbg(central_ab, ATH12K_DBG_DP_TX,
		   "vif configure vdev type = %d optype = %d\n",
		   ahvif->vdev_type, optype);

	if (optype == ATH12K_DP_OP_DEINIT) {
		if (dp_vif->bank_id != DP_INVALID_BANK_ID)
			ath12k_dp_tx_put_bank_profile(central_dp, dp_vif->bank_id);

		/* Reset VDEV multicast packet control bits for this vdev_id
		 * to prevent stale entry on vdev_id reuse.
		 */
		ath12k_wifi8_hal_vdev_mcast_ctrl_set(central_ab, dp_vif->dp_vif_id,
						HAL_TX_PACKET_CONTROL_CONFIG_DEFAULT);
		ath12k_mac_vif_unref(central_dp, ahvif->vif);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		if (!ab->dp->ppe.ppe_ops ||
			!ab->dp->ppe.ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile) {
			ath12k_err(ab, "vp_prof dealloc hdlr not present during deinit");
			return;
		}
		ab->dp->ppe.ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab,
				dp_vif->ppe_vp_profile_idx, ahvif->vif->type);
#endif
		return;
	} else if (optype == ATH12K_DP_OP_INIT) {
		/*TODO keep vdev_id check disabled for initial emulation */
		dp_vif->bank_id = DP_INVALID_BANK_ID;

		if (ahvif->vdev_type != WMI_VDEV_TYPE_STA &&
		    ahvif->vdev_type != WMI_VDEV_TYPE_AP) {
			ath12k_dbg(central_ab, ATH12K_DBG_DP_TX, "ignoring bank init vdev type = %d\n",
				   ahvif->vdev_type);
			return;
		}

		dp_vif->vdev_id_check_en = false;
		ath12k_dp_update_vdev_search(ahvif);

		/* Disable ADDRX search for STA vdev */
		if (ahvif->vdev_type == WMI_VDEV_TYPE_STA)
			dp_vif->hal_addr_search_flags &= ~HAL_TX_ADDRX_EN;

		/* convert vdev params into hal_tx_bank_config */
		new_bank_config = ath12k_wifi8_dp_tx_get_vdev_bank_config(ab, ahvif, 0,
									  false);
		bank_id = ath12k_dp_tx_get_bank_profile(central_dp, new_bank_config);
		dp_vif->bank_id = bank_id;

		ath12k_wifi8_hal_vdev_mcast_ctrl_set
			(central_ab, dp_vif->dp_vif_id,
			 HAL_TX_PACKET_CONTROL_CONFIG_DEFAULT);

		/* TODO: error path for bank id failure */
		if (bank_id == DP_INVALID_BANK_ID) {
			ath12k_err(central_ab, "Failed to initialize DP TX Banks");
			return;
		}
	} else if (optype == ATH12K_DP_OP_UPDATE) {
		if (dp_vif->bank_id == DP_INVALID_BANK_ID) {
			ath12k_dbg(central_ab, ATH12K_DBG_DP_TX, "bank id is not inited type = %d\n",
				   ahvif->vdev_type);
			return;
		}

		old_bank_config = ath12k_dp_tx_get_bank_config_from_id(central_dp,
								   dp_vif->bank_id);
		new_bank_config = ath12k_wifi8_dp_tx_get_vdev_bank_config(ab, ahvif,
									  0, false);
		if (old_bank_config != new_bank_config) {
			ath12k_dp_tx_put_bank_profile(central_dp, dp_vif->bank_id);
			bank_id = ath12k_dp_tx_get_bank_profile(central_dp,
								new_bank_config);
			dp_vif->bank_id = bank_id;
		}
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/*
	 * PPEDS - Configure the vp profile during vif configure.
	 */
	ath12k_wifi8_ppeds_attach_vif(ab, ahvif,
			dp_vif->ahvif_id,
			dp_vif->bank_id,
			HAL_TX_WILD_CARD_LINK_ID);
#endif
}

static void ath12k_wifi8_dp_link_vif_configure(struct ath12k_dp *dp,
					       struct ath12k_vif *ahvif,
					       u8 link_id,
					       enum ath12k_dp_op_type optype)
{
	struct ath12k_dp_vif *dp_vif = &ahvif->dp_vif;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];
	struct ath12k_link_vif *arvif = ahvif->link[link_id];
	struct ath12k *ar = arvif->ar;

	if (optype == ATH12K_DP_OP_DEINIT)
		return;

	if (optype == ATH12K_DP_OP_INIT) {
		dp_link_vif->ab = dp->ab;
		dp_link_vif->vdev_id = arvif->vdev_id;
		dp_link_vif->lmac_id = ar->lmac_id;
		dp_link_vif->pdev_idx = ar->pdev_idx;

		/* TODO change to dp_vif_id allocation later for
		 * now assign to link vdev_id
		 */
		dp_vif->dp_vif_id = dp_link_vif->vdev_id;

		dp_link_vif->tcl_metadata =
			u32_encode_bits(1, HTT_TCL_META_DATA_TYPE_V3) |
			u32_encode_bits(arvif->vdev_id,
					HTT_TCL_META_DATA_VDEV_ID_V3) |
			u32_encode_bits(dp_link_vif->pdev_idx,
					HTT_TCL_META_DATA_PDEV_ID_V3);

		/* set HTT extension valid bit to 0 by default */
		dp_link_vif->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT_V3;
	}

	ath12k_wifi8_dp_vif_configure(dp, ahvif, ATH12K_DP_OP_UPDATE);
}

void ath12k_wifi8_reset_device_dp_stats(struct ath12k_dp *dp)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	memset(&dp_wifi8->stats, 0, sizeof(struct ath12k_wifi8_dp_stats));
	ath12k_wifi8_global_ast_stats_reset(dp);
}

static ssize_t ath12k_wifi8_dump_device_dp_stats(struct ath12k_dp *dp,
						 char *buf, int size)
{
	int len = 0;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_wifi8_tx_exc_stats *stats = &dp_wifi8->stats.tx_exc_stats;
	struct ath12k_wifi8_rx_stats *rx_stats = &dp_wifi8->stats.rx_stats;

	len += scnprintf(buf + len, size - len, "\nTx Exception stats:\n");
	len += scnprintf(buf + len, size - len, "-------------------\n");
	len += scnprintf(buf + len, size - len, "total_exceptions:%u\n",
			 stats->tx_exceptions);
	len += scnprintf(buf + len, size - len, "null_flowq_packets:%u\n",
			 stats->null_flowq_pkts);
	len += scnprintf(buf + len, size - len, "reinject_packets:%u\n",
			 stats->reinject_pkts);
	len += scnprintf(buf + len, size - len, "invalid_desc:%u\n",
			 stats->invalid_desc);
	len += scnprintf(buf + len, size - len, "\nFailure Reasons::\n");
	len += scnprintf(buf + len, size - len, "vdev_id_check_fail:%u\n",
			 stats->vdev_id_check_fail);
	len += scnprintf(buf + len, size - len, "addrx_invalid:%u\n",
			 stats->addrx_invalid);
	len += scnprintf(buf + len, size - len, "addrx_timeout:%u\n",
			 stats->addrx_timeout);
	len += scnprintf(buf + len, size - len, "msdu_drop:%u\n",
			 stats->msdu_drop);
	len += scnprintf(buf + len, size - len, "illegal_packets:%u\n",
			 stats->illegal_pkts);
	len += scnprintf(buf + len, size - len, "illegal_packet_hdr:%u\n",
			 stats->illegal_pkt_hdr);
	len += scnprintf(buf + len, size - len, "peer_ptr_null:%u\n",
			 stats->peer_ptr_null);
	len += scnprintf(buf + len, size - len, "bank_not_configured:%u\n",
			 stats->bank_not_configured);
	len += scnprintf(buf + len, size - len, "msdu_len_err:%u\n",
			 stats->msdu_len_err);
	len += scnprintf(buf + len, size - len, "to_sw_pkts:%u\n",
			 stats->to_sw_pkts);
	len += scnprintf(buf + len, size - len, "parse_err:%u\n",
			 stats->parse_err);
	len += scnprintf(buf + len, size - len, "classify_info_sel_exceed:%u\n",
			 stats->classify_info_sel_exceed);
	len += scnprintf(buf + len, size - len, "bank_id_exceed:%u\n",
			 stats->bank_id_exceed);
	len += scnprintf(buf + len, size - len, "buf_len_err:%u\n",
			 stats->buf_len_err);

	len += scnprintf(buf + len, size - len,
			 "\nRx flush count: %u\nRx mgmt flush count: %u\n"
			 " NULL desc: %u\n",
			 rx_stats->rx_flush_pkts,
			 rx_stats->rx_mgmt_flush_pkts,
			 rx_stats->rx_flush_null_descs);

	len += ath12k_wifi8_global_ast_stats(dp, buf + len, size - len);
	return len;
}

static ssize_t ath12k_wifi8_dump_srng_stats(struct ath12k_dp *dp,
					    char *buf, int size)
{
	int len = 0;
	int i;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_TX_COMPLETION,
						  dp->tx_ring[i].tcl_comp_ring.ring_id,
						  buf + len, size - len);

	for (i = 0; i < DP_WBM_REFILL_RING_MAX; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_WBM_BUF,
						  dp_wifi8->wbm_refill_ring[i].ring_id,
						  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_WBM_IDLE_BUF,
					  dp_wifi8->wbm_idle_buf_ring.ring_id,
					  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_TX_EXCEPTION,
					  dp_wifi8->tx_exception.ring_id,
					  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_TCL_CMD,
					  dp_wifi8->tcl_cmd_ring.ring_id,
					  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_TCL_STATUS,
					  dp_wifi8->tcl_status_ring.ring_id,
					  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_PEER_TX_TELEMETRY,
					  dp_wifi8->tx_peer_telemetry_ring.ring_id,
					  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_PEER_RX_TELEMETRY,
					  dp_wifi8->rx_peer_telemetry_ring.ring_id,
					  buf + len, size - len);

	return len;
}

static inline u32 ath12k_dp_compute_gcd(u32 num1, u32 num2)
{
	u32 temp;

	while (num2 != 0) {
		temp = num2;
		num2 = num1 % num2;
		num1 = temp;
	}

	return num1;
}

/* should be called after aquiring the spin lock */
static inline void
ath12k_dp_recompute_update_scale_factor(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8)
{
	struct dp_hw_grp_timer_entry *timer_entry;
	u32 new_timer_val = 0;

	list_for_each_entry(timer_entry, &dp_hw_grp_wifi8->timer_list_head, list) {
		if (new_timer_val == 0)
			new_timer_val = timer_entry->timeout_ms;
		else
			new_timer_val = ath12k_dp_compute_gcd(new_timer_val,
							      timer_entry->timeout_ms);
	}
	dp_hw_grp_wifi8->current_timer_val = new_timer_val;

	list_for_each_entry(timer_entry, &dp_hw_grp_wifi8->timer_list_head, list) {
		timer_entry->scaling_factor =
		dp_hw_grp_wifi8->current_timer_val ?
		(timer_entry->timeout_ms / dp_hw_grp_wifi8->current_timer_val) : 0;
	}
}

void ath12k_dp_hw_group_timer_fn(struct timer_list *timer)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			from_timer(dp_hw_grp_wifi8, timer, hw_grp_timer);
	struct dp_hw_grp_timer_entry *timer_entry, *temp_entry;

	spin_lock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);
	list_for_each_entry_safe(timer_entry, temp_entry,
				 &dp_hw_grp_wifi8->timer_list_head, list) {
		timer_entry->counter++;
		if (timer_entry->counter >= timer_entry->scaling_factor) {
			if (timer_entry->cmd_callback)
				timer_entry->cmd_callback(dp_hw_grp_wifi8,
							  timer_entry->arg);
			timer_entry->counter = 0;
		}
	}

	if (dp_hw_grp_wifi8->current_timer_val && dp_hw_grp_wifi8->timer_entry_count)
		mod_timer(&dp_hw_grp_wifi8->hw_grp_timer,
			  jiffies + msecs_to_jiffies(dp_hw_grp_wifi8->current_timer_val));

	spin_unlock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);
}

int ath12k_dp_hw_group_add_timer_entry(struct ath12k_dp_hw_group *dp_hw_grp,
				       struct ath12k_dp_hw_grp_timer_entry_param *param)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct dp_hw_grp_timer_entry *timer_entry;

	if (!param->timeout_ms) {
		ath12k_err(NULL, "Invalid timer timeout");
		return -EINVAL;
	}

	timer_entry = kzalloc(sizeof(*timer_entry), GFP_ATOMIC);
	if (!timer_entry)
		return -ENOMEM;

	timer_entry->timeout_ms = param->timeout_ms;
	timer_entry->cmd_callback = param->callback;
	timer_entry->counter = 0;
	timer_entry->arg = param->arg;

	spin_lock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);
	list_add_tail(&timer_entry->list, &dp_hw_grp_wifi8->timer_list_head);
	dp_hw_grp_wifi8->timer_entry_count++;

	param->key_value = dp_hw_grp_wifi8->timer_entry_count;
	timer_entry->key_value = dp_hw_grp_wifi8->timer_entry_count;

	if (!dp_hw_grp_wifi8->current_timer_val)
		dp_hw_grp_wifi8->current_timer_val = timer_entry->timeout_ms;
	else
		dp_hw_grp_wifi8->current_timer_val =
			ath12k_dp_compute_gcd(dp_hw_grp_wifi8->current_timer_val,
					      timer_entry->timeout_ms);

	list_for_each_entry(timer_entry, &dp_hw_grp_wifi8->timer_list_head, list) {
		timer_entry->scaling_factor =
			timer_entry->timeout_ms / dp_hw_grp_wifi8->current_timer_val;
	}

	mod_timer(&dp_hw_grp_wifi8->hw_grp_timer,
		  jiffies + msecs_to_jiffies(dp_hw_grp_wifi8->current_timer_val));
	spin_unlock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);

	return 0;
}

bool ath12k_dp_hw_group_del_timer_entry(struct ath12k_dp_hw_group *dp_hw_grp,
					u16 key_value)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
			ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct dp_hw_grp_timer_entry *timer_entry, *temp_entry;
	bool entry_found = false;

	spin_lock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);
	list_for_each_entry_safe(timer_entry, temp_entry,
				 &dp_hw_grp_wifi8->timer_list_head, list) {
		if (timer_entry->key_value == key_value) {
			list_del(&timer_entry->list);
			dp_hw_grp_wifi8->timer_entry_count--;
			entry_found = true;
			break;
		}
	}

	if (!entry_found) {
		spin_unlock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);
		return entry_found;
	}

	ath12k_dp_recompute_update_scale_factor(dp_hw_grp_wifi8);

	if (dp_hw_grp_wifi8->current_timer_val && dp_hw_grp_wifi8->timer_entry_count)
		mod_timer(&dp_hw_grp_wifi8->hw_grp_timer,
			  jiffies + msecs_to_jiffies(dp_hw_grp_wifi8->current_timer_val));
	else
		del_timer(&dp_hw_grp_wifi8->hw_grp_timer);

	spin_unlock_bh(&dp_hw_grp_wifi8->hw_grp_timer_lock);

	kfree(timer_entry);
	return entry_found;
}

int ath12k_wifi8_dp_fetch_replenish_ring_id(struct ath12k_dp *dp)
{
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);

	return dp_wifi8->wbm_idle_buf_ring.ring_id;
}

int ath12k_wifi8_fetch_smd_ctx(struct ath12k_base *ab, struct ath12k_dp_hw *dp_hw,
			       struct ieee80211_vif *vif, struct ath12k_dp_smd_ctx *ctx,
			       void (*rx_cb)(struct ath12k_dp *dp, void *ctx,
					     struct hal_reo_status *reo_status),
			       void (*tx_cb)(struct ath12k_dp *dp, void *ctx,
					     u8 *addr, u8 tid))
{
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_hal_reo_cmd cmd = {0};
	struct ath12k_dp_rx_tid *rx_tid;
	struct ath12k_dp_smd_ctx smd_data = {};
	enum hal_reo_cmd_type cmd_type;
	int ret, tid;
	bool sent = false;
	bool clear_vld = ath12k_wifi8_clear_vld_after_smd_ctx_fetch &&
			 ctx->in.rx_tid_bitmap == 0xff;

	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(dp_hw, ctx->peer_addr, vif);

	if (!dp_peer) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	for (tid = 0; tid < ab->hal.hal_params->num_tids; tid++) {
		if (!(ctx->in.rx_tid_bitmap & BIT(tid)))
			continue;

		rx_tid = &dp_peer->rx_tid[tid];
		spin_lock_bh(&rx_tid->tid_lock);
		if (!rx_tid->active || !rx_tid->paddr) {
			ath12k_dbg(ab, ATH12K_DBG_DP_RX,
				   "skip smd ctx fetch tid %d peer %pM active %d paddr 0x%llx\n",
				   tid, ctx->peer_addr, rx_tid->active,
				   (unsigned long long)rx_tid->paddr);
			spin_unlock_bh(&rx_tid->tid_lock);
			continue;
		}
		smd_data.out.tid = tid;
		memcpy(smd_data.peer_addr, ctx->peer_addr, ETH_ALEN);

		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;

		if (clear_vld && !ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(tid)) {
			struct ath12k_reo_cmd_entry cmds[2];
			struct ath12k_reo_dp_cmd_desc descs[2];

			memset(cmds, 0, sizeof(cmds));
			memset(descs, 0, sizeof(descs));

			cmds[0].type = HAL_REO_CMD_GET_QUEUE_STATS;
			cmds[0].cmd = cmd;
			cmds[1].type = HAL_REO_CMD_UPDATE_RX_QUEUE;
			ath12k_wifi8_peer_rx_tid_reo_clear_vld_cmd_init(rx_tid,
									&cmds[1].cmd);

			descs[0].data = &smd_data;
			descs[0].len = sizeof(smd_data);
			descs[0].cb = rx_cb;
			descs[1].data = rx_tid;
			descs[1].len = sizeof(*rx_tid);

			ret = ath12k_wifi8_dp_reo_cmd_send_highprio_n(ab, cmds,
								      descs,
								      ARRAY_SIZE(cmds));
		} else {
			cmd_type = HAL_REO_CMD_GET_QUEUE_STATS;
			ret = ath12k_wifi8_dp_reo_cmd_send_highprio(ab, &smd_data,
								    sizeof(smd_data),
								    cmd_type, &cmd,
								    rx_cb);
		}

		if (ret) {
			ath12k_warn(ab,
				    "failed to send fetch smd ctx for rx tid queue, tid %d (%d)\n",
				    rx_tid->tid, ret);
			spin_unlock_bh(&rx_tid->tid_lock);
			spin_unlock_bh(&dp_hw->peer_hash_lock);
			return ret;
		}

		sent = true;

		ath12k_dbg(ab, ATH12K_DBG_DP_RX,
			   "smd ctx fetch sent tid %d peer %pM ring REO_CMD1\n",
			   rx_tid->tid, smd_data.peer_addr);

		spin_unlock_bh(&rx_tid->tid_lock);
	}


	if (!sent) {
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return -ENOENT;
	}

	ret = ath12k_dp_peer_fetch_smd_tx_ctx(ab, dp_peer,
					      ctx->in.tx_tid_bitmap,
					      ctx->in.tx_tid_ba_size, tx_cb);
	if (ret) {
		ath12k_warn(ab, "failed to fetch smd ctx tx queues, peer_id %d (%d)\n",
			    dp_peer->peer_id, ret);
		spin_unlock_bh(&dp_hw->peer_hash_lock);
		return ret;
	}
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	return 0;
}

static int ath12k_wifi8_dp_pool_coredump_seg_count(struct pool_ctxt_t *ctxt)
{
	struct mem_page_t *page;
	int i, cnt = 0;

	if (!ctxt || !ctxt->pages.mem_pages || !ctxt->pages.num_pages)
		return 0;

	for (i = 0; i < ctxt->pages.num_pages; i++) {
		page = &ctxt->pages.mem_pages[i];
		if (page->page_v_addr_start && page->page_p_addr)
			cnt++;
	}

	return cnt;
}

static
bool ath12k_wifi8_dp_ast_coredump_ready(struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8,
					struct ath12k_dp_global_ast_table **out_ast_base)
{
	struct ath12k_dp_global_ast_table *ast_base = &dp_hw_grp_wifi8->dp_ast_base;

	*out_ast_base = ast_base;

	return ast_base->ast_vaddr_aligned && ast_base->ast_paddr &&
	       ast_base->hw_ast_table_size;
}

static int ath12k_wifi8_dp_get_coredump_seg_count(struct ath12k_dp *dp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_dp_global_ast_table *ast_base;
	int seg_cnt = 0;

	if (!dp->dp_hw_grp)
		return 0;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8)
		return 0;

	if (ath12k_wifi8_dp_ast_coredump_ready(dp_hw_grp_wifi8, &ast_base))
		seg_cnt++;

	seg_cnt += ath12k_wifi8_dp_pool_coredump_seg_count(dp_hw_grp_wifi8->msduq_ctxt);
	seg_cnt += ath12k_wifi8_dp_pool_coredump_seg_count(dp_hw_grp_wifi8->mpduq_ctxt);

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
		   "coredump seg count query: total required=%d\n", seg_cnt);

	return seg_cnt;
}

static int ath12k_wifi8_dp_pool_get_coredump_segs(struct ath12k_base *ab,
						  struct pool_ctxt_t *ctxt,
						  enum ath12k_fw_crash_dump_type type,
						  const char *tag,
						  struct ath12k_dump_segment *seg_arr,
						  int max_segs)
{
	struct mem_page_t *page;
	int i, cnt = 0;

	if (!ctxt || !ctxt->pages.mem_pages || !ctxt->pages.num_pages)
		return 0;

	for (i = 0; i < ctxt->pages.num_pages && cnt < max_segs; i++) {
		page = &ctxt->pages.mem_pages[i];
		if (!page->page_v_addr_start || !page->page_p_addr)
			continue;

		seg_arr[cnt].vaddr = page->page_v_addr_start;
		seg_arr[cnt].addr  = page->page_p_addr;
		seg_arr[cnt].len   = ctxt->pages.page_size;
		seg_arr[cnt].type  = type;
		cnt++;
	}

	ath12k_dbg(ab, ATH12K_DBG_DP_TX,
		   "%s coredump: collected %d of %u pages (page_size=%zu)\n",
		   tag, cnt, ctxt->pages.num_pages, ctxt->pages.page_size);

	return cnt;
}

static int ath12k_wifi8_dp_get_coredump_segs(struct ath12k_dp *dp,
					     struct ath12k_dump_segment *seg_arr,
					     int max_segs)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8;
	struct ath12k_dp_global_ast_table *ast_base;
	struct device *dev = dp->dev;
	dma_addr_t paddr;
	int seg_cnt = 0;

	if (!max_segs || !dp->dp_hw_grp)
		return 0;

	dp_hw_grp_wifi8 = ath12k_get_dp_hw_group_wifi8(dp->dp_hw_grp);
	if (!dp_hw_grp_wifi8)
		return 0;

	if (ath12k_wifi8_dp_ast_coredump_ready(dp_hw_grp_wifi8, &ast_base)) {
		paddr = ast_base->ast_paddr;
		ath12k_core_dma_sync_single_for_cpu(dev, paddr,
						    ast_base->hw_ast_table_size,
						    DMA_BIDIRECTIONAL);

		seg_arr[seg_cnt].vaddr = ast_base->ast_vaddr_aligned;
		seg_arr[seg_cnt].addr  = paddr;
		seg_arr[seg_cnt].len   = ast_base->hw_ast_table_size;
		seg_arr[seg_cnt].type  = FW_CRASH_DUMP_AST_DATA;
		seg_cnt++;
	} else {
		ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
			   "AST coredump: table not ready, skipping AST segment\n");
	}

	seg_cnt += ath12k_wifi8_dp_pool_get_coredump_segs(dp->ab,
							  dp_hw_grp_wifi8->msduq_ctxt,
							  FW_CRASH_DUMP_MSDU_DATA,
							  "MSDU",
							  &seg_arr[seg_cnt],
							  max_segs - seg_cnt);
	seg_cnt += ath12k_wifi8_dp_pool_get_coredump_segs(dp->ab,
							  dp_hw_grp_wifi8->mpduq_ctxt,
							  FW_CRASH_DUMP_MPDU_DATA,
							  "MPDU",
							  &seg_arr[seg_cnt],
							  max_segs - seg_cnt);

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_TX,
		   "coredump segs filled: total=%d\n", seg_cnt);

	return seg_cnt;
}

static struct ath12k_dp_arch_ops ath12k_wifi8_dp_arch_ops = {
	.dp_op_device_init = ath12k_wifi8_dp_op_device_init,
	.dp_op_device_deinit = ath12k_wifi8_dp_op_device_deinit,
	.dp_msdu_htt_connect = ath12k_wifi8_dp_msdu_htt_connect,
	.dp_op_mlo_init = ath12k_wifi8_dp_op_mlo_init,
	.dp_op_mlo_deinit = ath12k_wifi8_dp_op_mlo_deinit,
	.dp_tx_get_vdev_bank_config = ath12k_wifi8_dp_tx_get_vdev_bank_config,
	.dp_reo_cmd_send = ath12k_wifi8_dp_reo_cmd_send,
	.setup_pn_check_reo_cmd = ath12k_wifi8_dp_setup_pn_check_reo_cmd,
	.rx_peer_tid_delete = ath12k_wifi8_dp_rx_peer_tid_delete,
	.dp_mlo_peer_tid_teardown_ready = ath12k_wifi8_dp_mlo_peer_tid_teardown_ready,
	.reo_cache_flush = ath12k_wifi8_dp_reo_cache_flush,
	.rx_link_desc_return = ath12k_wifi8_dp_rx_link_desc_return,
	.peer_rx_tid_reo_update = ath12k_wifi8_peer_rx_tid_reo_update,
	.alloc_reo_qdesc = ath12k_wifi8_dp_alloc_reo_qdesc,
	.peer_rx_tid_qref_setup = ath12k_wifi8_peer_rx_tid_qref_setup,
	.dp_pdev_alloc = ath12k_wifi8_dp_pdev_alloc,
	.dp_pdev_free = ath12k_wifi8_dp_pdev_free,
	.rx_fst_attach = ath12k_wifi8_dp_rx_fst_attach,
	.rx_fst_detach = ath12k_wifi8_dp_rx_fst_detach,
	.rx_flow_dump_entry = ath12k_wifi8_dp_rx_flow_dump_entry,
	.rx_flow_add_entry = ath12k_wifi8_dp_rx_flow_add_entry,
	.rx_flow_delete_entry = ath12k_wifi8_dp_rx_flow_delete_entry,
	.rx_flow_delete_all_entries = ath12k_wifi8_dp_rx_flow_delete_all_entries,
	.dump_fst_table = ath12k_wifi8_dp_dump_fst_table,
	.dp_hw_group_alloc = ath12k_wifi8_dp_hw_group_alloc,
	.sdwf_reinject_handler = ath12k_wifi8_sdwf_reinject_handler,
	.dp_peer_create = ath12k_wifi8_dp_peer_create,
	.dp_peer_delete = ath12k_wifi8_dp_peer_delete,
	.dp_peer_assoc = ath12k_wifi8_dp_peer_assoc,
	.dp_link_peer_assign_id = ath12k_wifi8_dp_link_peer_assign_id,
	.dp_link_peer_unassign_id = ath12k_wifi8_dp_link_peer_unassign_id,
	.peer_cleanup_indication = ath12k_dp_htt_peer_cleanup_indication,
	.peer_drv_cleanup_indication = ath12k_dp_peer_cleanup_indication,
	.dp_ppeds_tx_completion_handler = ath12k_wifi8_ppeds_tx_completion_handler,
	.dp_link_peer_assoc = ath12k_wifi8_dp_link_peer_assoc,
	.dp_get_peer_mgmt_flowq = ath12k_wifi8_get_mgmt_flowq,
	.dp_get_peer_holq = ath12k_wifi8_get_holq,
	.dp_vif_configure = ath12k_wifi8_dp_vif_configure,
	.dp_link_vif_configure = ath12k_wifi8_dp_link_vif_configure,
	.rx_flow_fse_cache_operation = ath12k_wifi8_dp_rx_flow_fse_cache_operation,
	.get_peer_init_status = ath12k_wifi8_dp_get_peer_init_status,
	.fetch_rx_desc_replenish_ring_id = ath12k_wifi8_dp_fetch_replenish_ring_id,
	.dp_tx_set_ast = ath12k_wifi8_dp_tx_set_ast,
	/* UMAC reset operations */
	.umac_reset_handle_pre_reset = ath12k_wifi8_umac_reset_handle_pre_reset,
	.umac_reset_handle_post_reset_start =
				ath12k_wifi8_umac_reset_handle_post_reset_start,
	.umac_reset_handle_post_reset_complete =
				ath12k_wifi8_umac_reset_handle_post_reset_complete,
	.umac_reset_handle_init_recovery =
				ath12k_wifi8_umac_reset_handle_init_recovery,
	.umcmn_irq_config = ath12k_wifi8_umcmn_irq_config,
	.umcmn_irq_free = ath12k_wifi8_umcmn_irq_free,
	.umcmn_irq_enable = ath12k_wifi8_umcmn_irq_enable,
	.umcmn_irq_disable = ath12k_wifi8_umcmn_irq_disable,
	.umcmn_timer_config = ath12k_wifi8_umcmn_timer_config,
	.umcmn_timer_enable = ath12k_wifi8_umcmn_timer_enable,
	.umcmn_timer_free = ath12k_wifi8_umcmn_timer_free,
	.dump_srng_stats = ath12k_wifi8_dump_srng_stats,
	.dump_device_dp_stats = ath12k_wifi8_dump_device_dp_stats,
	.reset_device_dp_stats = ath12k_wifi8_reset_device_dp_stats,
	.dp_assoc_link_update = ath12k_wifi8_dp_assoc_link_update,
	.dp_tx_mcast_send = ath12k_wifi8_dp_tx_mcast_send,
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	.dp_ast_param_get = ath12k_wifi8_dp_peer_ast_param_get,
#endif
	.peer_rx_tid_reo_update_for_smd = ath12k_wifi8_peer_rx_tid_reo_update_for_smd,
	.dp_peer_fetch_smd_ctx = ath12k_wifi8_fetch_smd_ctx,
	.dp_qos_queue_setup = ath12k_wifi8_qos_queue_setup,
	.peer_tx_tid_update_for_smd = ath12k_wifi8_peer_tx_tid_update_for_smd,
	.dump_svc_sorted_list = ath12k_wifi8_dp_tx_dump_svc_sorted_list,
	.update_tx_msdu_flow = ath12k_wifi8_dp_tx_update_msdu_flow,
	.dump_congestion_ctrl_stats = ath12k_wifi8_dp_tx_dump_congestion_ctrl_stats,
	.dump_congestion_recovery_hist = ath12k_wifi8_dp_tx_dump_congestion_recovery_hist,
	.set_congestion_ctrl_param = ath12k_wifi8_dp_tx_set_congestion_ctrl_param,
	.dp_smd_prep_transfer_ext_ctx = ath12k_wifi8_dp_smd_prep_transfer_ext_ctx,
	.dp_smd_exec_activate_links = ath12k_wifi8_dp_smd_exec_activate_links,
	.dp_smd_prep_rx_tid = ath12k_wifi8_dp_smd_prep_rx_tid,
	.dp_smd_exec_rx_tid = ath12k_wifi8_dp_smd_exec_rx_tid,
	.dp_smd_clear_old_peer_rx_lut = ath12k_wifi8_dp_smd_clear_old_peer_rx_lut,
	.dp_smd_abort_prep = ath12k_wifi8_dp_smd_abort_prep,
	.peer_tx_tid_sn_reset = ath12k_wifi8_peer_tx_tid_sn_reset,
	.peer_rx_tid_svld_reset = ath12k_wifi8_peer_rx_tid_svld_reset,
	.get_coredump_seg_count = ath12k_wifi8_dp_get_coredump_seg_count,
	.get_coredump_segs = ath12k_wifi8_dp_get_coredump_segs,
};

struct ath12k_dp *ath12k_wifi8_dp_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp;
	struct ath12k_dp_wifi8 *dp_wifi8;
	int ret;

	dp = kzalloc(sizeof(*dp) + sizeof(*dp_wifi8), GFP_KERNEL);
	if (!dp)
		return NULL;

	dp_wifi8 = ath12k_get_dp_wifi8(dp);
	dp_wifi8->dp = dp;

	dp->arch_ops = &ath12k_wifi8_dp_arch_ops;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	dp->ppe.ppeds_wlanops = &ppeds_wlan_ops_v2_wifi8;
	dp->ppe.ppe_ops = &ath12k_wifi8_arch_ppeds_ops;

	/* TODO: DS: revisit this for new DS design in WDS mode */
	if (ath12k_ppe_ds_enabled) {
		if (ath12k_frame_mode != ATH12K_HW_TXRX_ETHERNET) {
			ath12k_warn(ab,
				    "Force enabling Ethernet frame mode in PPE DS for AP and STA modes.\n");
			/* MESH and WDS VAPs will still use NATIVE_WIFI mode
			 * @ath12k_mac_update_vif_offload()
			 * TODO: add device capability check
			 */
			ath12k_ppe_ds_enabled = 0;
		} else if (ab->hw_params->ds_support) {
			set_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags);
		}
	}
#endif

	dp->ab = ab;
	dp->dev = ab->dev;
	dp->hw_params = ab->hw_params;
	dp->hw_peer_stats_support = true;
	dp->hal = &ab->hal;
	dp->global_peer_id_supported = true;

	ret = ath12k_dp_mon_init(dp);
	if (ret) {
		ath12k_warn(dp, "dp_mon_init failed %d\n", ret);
		goto dp_err;
	}

	ath12k_wifi8_dp_mon_ops_register(dp);

	ath12k_dp_mon_cfg_init(dp);

	ret = ath12k_dp_mon_rx_alloc(dp);
	if (ret) {
		ath12k_warn(dp->ab, "failed to setup rxdma rings ret = %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_dp_mon_tx_srng_alloc(dp);
	if (ret) {
		ath12k_warn(ab, "Tx Mon: failed to setup rings ret = %d\n", ret);
		goto dp_err;
	}

	return dp;
dp_err:
	ath12k_wifi8_dp_deinit(dp);
	return NULL;
}

void ath12k_wifi8_dp_deinit(struct ath12k_dp *dp)
{
	ath12k_dp_mon_tx_srng_free(dp);
	ath12k_dp_mon_rx_free(dp);
	ath12k_dp_mon_deinit(dp);
	ath12k_wifi8_dp_umac_free(dp);
	kfree(dp);
}

void ath12k_wifi8_dp_tx_tqm_cmd_list_cleanup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_tqm_cmd *cmd, *tmp;

	spin_lock_bh(&dp->tqm_cmd_lock);
	list_for_each_entry_safe(cmd, tmp, &dp->tqm_cmd_list, list) {
		list_del(&cmd->list);
		spin_unlock_bh(&dp->tqm_cmd_lock);
		/* Drive forced cleanup so peer_ext_ctx is freed even when
		 * the TQM status response never arrived (e.g. wifi down).
		 */
		if (cmd->handler)
			cmd->handler(dp, (void *)&cmd->data, NULL);
		kfree(cmd);
		spin_lock_bh(&dp->tqm_cmd_lock);
	}
	spin_unlock_bh(&dp->tqm_cmd_lock);
}
