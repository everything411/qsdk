// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "../core.h"
#include "../debug.h"
#include "../dp_rx.h"
#include "../dp_tx.h"
#include "../hif.h"
#include "../dp_cmn.h"
#include "dp_rx.h"
#include "dp.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "hal.h"
#include "ppeds.h"
#include "dp_mon.h"
#include "dp_peer.h"
#include "../wmi.h"
#include "umac_reset.h"
#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */
#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
#include "../qcn_extns/ipa/dp_ipa_pub.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */

static int ath12k_wifi7_dp_service_srng(struct ath12k_dp *dp,
					struct ath12k_ext_irq_grp *irq_grp,
					int budget)
{
	struct napi_struct *napi = &irq_grp->napi;
	struct ath12k_base *ab = dp->ab;
	int grp_id = irq_grp->grp_id;
	int work_done = 0;
	int i = 0, j;
	int tot_work_done = 0;
	u8 ring_mask, rx_mask, tx_mask;
	struct hal_srng *refill_srng;

	/* Return early if UMAC reset is in progress */
	if (ath12k_dp_umac_reset_in_progress(ab))
		return 0;

	rx_mask = dp->hw_params->ring_mask->rx[grp_id];
	tx_mask = dp->hw_params->ring_mask->tx[grp_id];

	while (tx_mask) {
		i = fls(tx_mask) - 1;
		tx_mask ^= 1 << i;
		work_done = ath12k_wifi7_dp_tx_completion_handler(dp, i, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_err[grp_id]) {
		work_done = ath12k_wifi7_dp_rx_process_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_wbm_rel[grp_id]) {
		work_done = ath12k_wifi7_dp_rx_process_wbm_err(dp, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;

		if (budget <= 0)
			goto done;
	}

	while (rx_mask) {
		i = fls(rx_mask) - 1;
		rx_mask ^= 1 << i;
		work_done = ath12k_wifi7_dp_rx_process(dp, i, napi, budget);
		budget -= work_done;
		tot_work_done += work_done;
		if (budget <= 0)
			goto done;
	}

	if (dp->hw_params->ring_mask->rx_mon_status[grp_id]) {
		ring_mask = dp->hw_params->ring_mask->rx_mon_status[grp_id];
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

	if (dp->hw_params->ring_mask->reo_status[grp_id])
		ath12k_wifi7_dp_rx_process_reo_status(dp);

	if (dp->hw_params->ring_mask->host2rxdma[grp_id]) {
		struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
		LIST_HEAD(list);
		size_t req_entries;

		refill_srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
		req_entries = ath12k_dp_get_req_entries_from_buf_ring(dp->ab,
				refill_srng,
				&list,
				DP_RX_DEFAULT_POOL);
		if (req_entries)
			ath12k_dp_rx_bufs_replenish(dp, refill_srng, &list, false);
	}

	if (dp->hw_params->ring_mask->host2rxmon[grp_id])
		ath12k_dp_mon_rx_process_low_thres(dp);

	if (dp->hw_params->ring_mask->host2txmon[grp_id])
		ath12k_dp_mon_tx_process_low_thres(dp);

	/* TODO: Implement handler for other interrupts */

done:
	return tot_work_done;
}

static int ath12k_wifi7_dp_reoq_lut_alloc(struct ath12k_base *ab)
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

	ret = ath12k_dp_alloc_reoq_lut(ab, &dp->ml_reoq_lut);
	if (ret) {
		ath12k_warn(ab, "failed to allocate memory for ML reoq table");
		ath12k_hal_dma_free_coherent(ab->dev, dp->reoq_lut.size,
					     dp->reoq_lut.vaddr_unaligned,
					     dp->reoq_lut.paddr_unaligned);
		dp->reoq_lut.vaddr_unaligned = NULL;
		return ret;
	}

	return 0;
}

static int ath12k_wifi7_dp_reoq_lut_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);

	memset(dp->reoq_lut.vaddr_unaligned, 0, dp->reoq_lut.size);
	memset(dp->ml_reoq_lut.vaddr_unaligned, 0, dp->ml_reoq_lut.size);

	/* Bits in the register have address [39:8] LUT base address to be
	 * allocated such that LSBs are assumed to be zero. Also, current
	 * design supports paddr up to 4 GB max hence it fits in 32 bit register only
	 */
	ath12k_hal_write_reoq_lut_addr(ab, dp->reoq_lut.paddr >> 8);
	ath12k_hal_write_ml_reoq_lut_addr(ab, dp->ml_reoq_lut.paddr >> 8);

	ath12k_hal_reoq_lut_addr_read_enable(ab);
	ath12k_hal_reoq_lut_set_max_peerid(ab);

	return 0;
}

static void ath12k_wifi7_dp_reoq_lut_cleanup(struct ath12k_base *ab)
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

	if (dp->ml_reoq_lut.vaddr_unaligned) {
		ath12k_hal_dma_free_coherent(ab->dev, dp->ml_reoq_lut.size,
					     dp->ml_reoq_lut.vaddr_unaligned,
					     dp->ml_reoq_lut.paddr_unaligned);
		dp->ml_reoq_lut.vaddr_unaligned = NULL;
	}
}

static int ath12k_wifi7_dp_op_device_init(struct ath12k_dp *dp)
{
	int ret;
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng = NULL;
	u32 n_link_desc = 0;
	int i;

	dp->tcl_metadata_ver = HTT_OPTION_TCL_METADATA_VER_V2;
	dp->htt_tx_mon_cfg_msg_size =
			(u16)offsetof(struct htt_tx_mon_ring_selection_cfg_cmd,
				      tlv_word_mask_in6);
	dp->htt_tx_mon_cfg_version = 0; /* wifi7 baseline */
	INIT_LIST_HEAD(&dp->reo_cmd_list);
	INIT_LIST_HEAD(&dp->reo_cmd_cache_flush_list);
	INIT_LIST_HEAD(&dp->reo_cmd_update_rx_queue_list);
	spin_lock_init(&dp->reo_cmd_update_rx_queue_lock);
	spin_lock_init(&dp->reo_cmd_lock);

	ret = ath12k_hif_ext_irq_setup(dp->ab, ath12k_wifi7_dp_service_srng, dp);
	if (ret)
		return ret;

#ifdef CPTCFG_QCN_EXTN
	if (ath12k_cfg_get(ab, ATH12K_INI_DP_UCAST_ENABLE_AST_OVERRIDE))
		set_bit(ATH12K_FLAG_UCAST_ENABLE_AST_OVERRIDE, &ab->dev_flags);
#endif /* CPTCFG_QCN_EXTN */

	dp->reo_cmd_cache_flush_count = 0;
	dp->idle_link_rbm =
			ath12k_hal_get_idle_link_rbm(&ab->hal, ab->device_id);

	ret = ath12k_wbm_idle_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup wbm_idle_ring: %d\n", ret);
		goto fail_irq_cleanup;
	}

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];
	n_link_desc = dp->wbm_idle_ring.num_entries;

	/* memset wbm link desc pool to 0 before desc_setup */
	ath12k_dp_clear_link_desc_pool(dp);

	ret = ath12k_dp_link_desc_init(ab, dp->link_desc_banks,
				       HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		ath12k_warn(ab, "failed to setup link desc: %d\n", ret);
		goto fail_irq_cleanup;
	}

	ret = ath12k_dp_cc_init(ab);

	if (ret) {
		ath12k_warn(ab, "failed to setup cookie converter %d\n", ret);
		goto fail_irq_cleanup;
	}

	ret = ath12k_dp_init_bank_profiles(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup bank profiles %d\n", ret);
		goto fail_hw_cc_cleanup;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ret = ath12k_nss_plugin_register_ops(ab);
	if (ret) {
		ath12k_warn(ab, "failed to register nss plugin %d\n", ret);
		goto fail_dp_bank_profiles_cleanup;
	}

	ret = dp->ppe.ppe_ops->ath12k_ppeds_attach(ab);
	if (ret) {
		ath12k_warn(ab, "failed to attach PPE DS %d\n", ret);
		goto fail_nss_plugin_unregister;
	}
#endif

	ret = ath12k_dp_srng_common_init(ab);
	if (ret)
		goto fail_ppeds_detach;

	ret = ath12k_wifi7_dp_tx_ring_init(ab);
	if (ret)
		goto fail_ppeds_detach;

	ret = ath12k_wifi7_dp_reoq_lut_init(ab);
	if (ret) {
		ath12k_warn(ab, "failed to init reoq table %d\n", ret);
		goto fail_ppeds_detach;
	}

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		dp->tx_ring[i].tcl_data_ring_id = i;

	for (i = 0; i < HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX; i++)
		ath12k_hal_tx_set_dscp_tid_map(ab, ath12k_default_dscp_tid_map, i);

	ath12k_hal_tx_set_pcp_tid_map(ab, dp->dp_hw_grp->pcp_tid_map);
	ath12k_hal_tx_set_tid_map_precedence(ab, dp->dp_hw_grp->tid_map_precedence);

	ret = ath12k_wifi7_dp_rx_ring_init(ab);
	if (ret) {
		ath12k_warn(ab, "rx init failed ret = %d\n", ret);
		goto fail_ppeds_detach;
	}

	ret = ath12k_dp_mon_rx_init(dp);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma rings ret = %d\n", ret);
		goto fail_dp_mon_rx_deinit;
	}

	ret = ath12k_dp_mon_tx_srng_init(dp);
	if (ret) {
		ath12k_warn(ab, "Tx Mon: failed to setup rings ret = %d\n", ret);
		goto fail_dp_mon_rx_deinit;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops &&
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_start)
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_start(ab);
#endif

#if defined(CPTCFG_EXT_IPA_OFFLOAD) && defined(CPTCFG_QCN_EXTN)
	ret = ath12k_wifi7_dp_ipa_init(ab);
	if (ret) {
		ath12k_err(dp->ab, "IPA: ipa init failed");
		goto fail_dp_mon_rx_deinit;
	}
#endif /* CPTCFG_EXT_IPA_OFFLOAD && CPTCFG_QCN_EXTN */

	return 0;

fail_dp_mon_rx_deinit:
	ath12k_dp_mon_rx_deinit(dp);

fail_ppeds_detach:
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	dp->ppe.ppe_ops->ath12k_ppeds_detach(ab);

fail_nss_plugin_unregister:
	ath12k_nss_plugin_unregister_ops(ab);

fail_dp_bank_profiles_cleanup:
#endif
	ath12k_dp_deinit_bank_profiles(ab);
fail_hw_cc_cleanup:
	ath12k_dp_cc_deinit(ab);

fail_irq_cleanup:
	ath12k_hif_ext_irq_cleanup(dp->ab);

	return ret;

}

static int ath12k_wifi7_dp_op_mlo_init(struct ath12k_dp *dp)
{
	int ret;

	ath12k_dp_partner_cc_init(dp->ab);

	ret = ath12k_wifi7_dp_rx_flow_fse_cache_operation(dp->ab,
							  DP_FST_CACHE_INVALIDATE_FULL,
							  NULL);
	if (ret) {
		ath12k_err(dp->ab, "Unable to invalidate Full cache ret %d", ret);
		return ret;
	}

	return 0;
}

static void ath12k_wifi7_dp_op_device_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (!dp->ab)
		return;

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	dp->ppe.ppe_ops->ath12k_ppeds_stop(ab);
	dp->ppe.ppe_ops->ath12k_ppeds_detach(ab);
#endif
	ath12k_dp_cc_deinit(ab);
	ath12k_dp_deinit_bank_profiles(ab);

	ath12k_dp_rx_reo_cmd_list_cleanup(ab);

	ath12k_dp_mon_rx_deinit(dp);
	ath12k_dp_mon_tx_srng_deinit(dp);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	ath12k_nss_plugin_unregister_ops(ab);
#endif

	ath12k_hif_ext_irq_cleanup(dp->ab);
}

static struct ath12k_dp_hw_group *ath12k_wifi7_dp_hw_group_alloc(void)
{
	struct ath12k_dp_hw_group *dp_hw_grp;
	u8 i;

	dp_hw_grp = kzalloc(sizeof(*dp_hw_grp), GFP_KERNEL);
	if (!dp_hw_grp) {
		pr_err("failed to allocate dp_hw_group\n");
		return NULL;
	}

	for (i = 0; i < ATH12K_DP_PCP_TID_MAP_SIZE; i++)
		dp_hw_grp->pcp_tid_map[i] = i;

	return dp_hw_grp;
}

static void ath12k_wifi7_dp_vif_configure(struct ath12k_dp *dp,
					  struct ath12k_vif *ahvif,
					  enum ath12k_dp_op_type optype)
{
	ath12k_dp_update_vdev_search(ahvif);
}

static void ath12k_wifi7_dp_link_vif_configure(struct ath12k_dp *dp,
					       struct ath12k_vif *ahvif,
					       u8 link_id,
					       enum ath12k_dp_op_type optype)
{
	struct ath12k_base *ab = dp->ab;
	int bank_id;
	bool mec_support;
	struct ath12k_dp_link_vif *dp_link_vif = &ahvif->dp_vif.dp_link_vif[link_id];
	struct ath12k_link_vif *arvif = ahvif->link[link_id];
	struct ath12k *ar = arvif->ar;
	u32 old_bank_config, new_bank_config;

	if (optype == ATH12K_DP_OP_DEINIT) {
		if (dp_link_vif->bank_id != DP_INVALID_BANK_ID)
			ath12k_dp_tx_put_bank_profile(dp, dp_link_vif->bank_id);

		/* Reset VDEV multicast packet control bits for this vdev_id
		 * to prevent stale entry on vdev_id reuse.
		 */
		ath12k_wifi7_hal_vdev_mcast_ctrl_set(ab, arvif->vdev_id,
				HAL_TX_PACKET_CONTROL_CONFIG_TO_FW_EXCEPTION);
		ath12k_mac_vif_unref(dp, ahvif->vif);
		return;
	} else if (optype == ATH12K_DP_OP_INIT) {
		dp_link_vif->ab = ab;
		dp_link_vif->vdev_id = arvif->vdev_id;
		dp_link_vif->lmac_id = ar->lmac_id;
		dp_link_vif->pdev_idx = ar->pdev_idx;

		dp_link_vif->tcl_metadata = u32_encode_bits(1, HTT_TCL_META_DATA_TYPE) |
			u32_encode_bits(arvif->vdev_id,
					HTT_TCL_META_DATA_VDEV_ID) |
			u32_encode_bits(dp_link_vif->pdev_idx,
					HTT_TCL_META_DATA_PDEV_ID);

		/* set HTT extension valid bit to 0 by default */
		dp_link_vif->tcl_metadata &= ~HTT_TCL_META_DATA_VALID_HTT;

		new_bank_config = ath12k_wifi7_dp_tx_get_vdev_bank_config(ab, ahvif,
									  link_id,
									  false);
		bank_id = ath12k_dp_tx_get_bank_profile(ath12k_ab_to_dp(ab),
							new_bank_config);
		dp_link_vif->bank_id = bank_id;

		mec_support = test_bit(WMI_SERVICE_MEC_AGING_TIMER_SUPPORT,
				       ab->wmi_ab.svc_map);

		if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
		    ath12k_frame_mode == ATH12K_HW_TXRX_ETHERNET &&
		    mec_support) {
			ath12k_wmi_pdev_set_timer_for_mec(ar, arvif->vdev_id,
					WMI_PDEV_MEC_AGING_TIMER_THRESHOLD_VALUE);
			ath12k_wifi7_hal_vdev_mcast_ctrl_set(ab, arvif->vdev_id,
					HAL_TX_PACKET_CONTROL_CONFIG_MEC_NOTIFY);
		}

		/* TODO: error path for bank id failure */
		if (bank_id == DP_INVALID_BANK_ID) {
			ath12k_err(ar->ab, "Failed to initialize DP TX Banks");
			return;
		}
	} else if (optype == ATH12K_DP_OP_UPDATE) {
		old_bank_config =
			ath12k_dp_tx_get_bank_config_from_id(dp, dp_link_vif->bank_id);
		new_bank_config = ath12k_wifi7_dp_tx_get_vdev_bank_config(ab, ahvif,
									  link_id,
									  false);
		if (old_bank_config != new_bank_config) {
			ath12k_dp_tx_put_bank_profile(dp, dp_link_vif->bank_id);
			bank_id = ath12k_dp_tx_get_bank_profile(dp, new_bank_config);
			dp_link_vif->bank_id = bank_id;
		}
	}
}

static ssize_t ath12k_wifi7_dump_srng_stats(struct ath12k_dp *dp,
					    char *buf, int size)
{
	int len = 0;
	struct ath12k_base *ab = dp->ab;
	int i;

	for (i = 0; i < ab->hw_params->max_tx_ring; i++)
		len += ath12k_hal_dump_ring_stats(ab, HAL_WBM2SW_RELEASE,
						  dp->tx_ring[i].tcl_comp_ring.ring_id,
						  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_WBM2SW_RELEASE,
					  dp->rx_rel_ring.ring_id,
					  buf + len, size - len);

	len += ath12k_hal_dump_ring_stats(ab, HAL_RXDMA_BUF,
					  dp->rx_refill_buf_ring.refill_buf_ring.ring_id,
					  buf + len, size - len);

	return len;
}

int ath12k_wifi7_dp_fetch_replenish_ring_id(struct ath12k_dp *dp)
{
	struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;

	return rx_ring->refill_buf_ring.ring_id;
}

static int ath12k_wifi7_dp_qos_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					   struct ath12k_dp_peer *dp_peer,
					   u8 msduq, u16 qos_id,
					   u8 qm_id, bool txop_intent)
{
	return 0;
}

static struct ath12k_dp_arch_ops ath12k_wifi7_dp_arch_ops = {
	.dp_op_device_init = ath12k_wifi7_dp_op_device_init,
	.dp_op_device_deinit = ath12k_wifi7_dp_op_device_deinit,
	.dp_op_mlo_init = ath12k_wifi7_dp_op_mlo_init,
	.dp_tx_get_vdev_bank_config = ath12k_wifi7_dp_tx_get_vdev_bank_config,
	.dp_reo_cmd_send = ath12k_wifi7_dp_reo_cmd_send,
	.setup_pn_check_reo_cmd = ath12k_wifi7_dp_setup_pn_check_reo_cmd,
	.rx_peer_tid_delete = ath12k_wifi7_dp_rx_peer_tid_delete,
	.dp_mlo_peer_tid_teardown_ready = ath12k_wifi7_dp_mlo_peer_tid_teardown_ready,
	.reo_cache_flush = ath12k_wifi7_dp_reo_cache_flush,
	.rx_link_desc_return = ath12k_wifi7_dp_rx_link_desc_return,
	.peer_rx_tid_reo_update = ath12k_wifi7_peer_rx_tid_reo_update,
	.alloc_reo_qdesc = ath12k_wifi7_dp_alloc_reo_qdesc,
	.peer_rx_tid_qref_setup = ath12k_wifi7_peer_rx_tid_qref_setup,
	.dp_pdev_alloc = ath12k_wifi7_dp_pdev_alloc,
	.dp_pdev_free = ath12k_wifi7_dp_pdev_free,
	.rx_fst_attach = ath12k_wifi7_dp_rx_fst_attach,
	.rx_fst_detach = ath12k_wifi7_dp_rx_fst_detach,
	.rx_flow_dump_entry = ath12k_wifi7_dp_rx_flow_dump_entry,
	.rx_flow_add_entry = ath12k_wifi7_dp_rx_flow_add_entry,
	.rx_flow_delete_entry = ath12k_wifi7_dp_rx_flow_delete_entry,
	.rx_flow_delete_all_entries = ath12k_wifi7_dp_rx_flow_delete_all_entries,
	.dump_fst_table = ath12k_wifi7_dp_dump_fst_table,
	.dp_hw_group_alloc = ath12k_wifi7_dp_hw_group_alloc,
	.peer_migrate_reo_cmd = ath12k_wifi7_dp_peer_migrate_reo_cmd,
	.sdwf_reinject_handler = ath12k_wifi7_sdwf_reinject_handler,
	.dp_peer_create = ath12k_wifi7_dp_peer_create,
	.dp_peer_delete = ath12k_wifi7_dp_peer_delete,
	.dp_peer_assoc = ath12k_wifi7_dp_peer_assoc,
	.dp_ppeds_tx_completion_handler = ath12k_wifi7_ppeds_tx_completion_handler,
	.dp_vif_configure = ath12k_wifi7_dp_vif_configure,
	.dp_link_vif_configure = ath12k_wifi7_dp_link_vif_configure,
	.rx_flow_fse_cache_operation = ath12k_wifi7_dp_rx_flow_fse_cache_operation,
	.fetch_rx_desc_replenish_ring_id = ath12k_wifi7_dp_fetch_replenish_ring_id,
	.dp_tx_mcast_send = ath12k_wifi7_dp_tx_mcast_send,
	.dp_tx_set_ast = ath12k_wifi7_dp_tx_set_ast,
	/* UMAC reset operations */
	.umac_reset_handle_pre_reset = ath12k_wifi7_umac_reset_handle_pre_reset_wrapper,
	.umac_reset_handle_post_reset_start =
			ath12k_wifi7_umac_reset_handle_post_reset_start_wrapper,
	.umac_reset_handle_post_reset_complete =
			ath12k_wifi7_umac_reset_handle_post_reset_complete_wrapper,
	.dump_srng_stats = ath12k_wifi7_dump_srng_stats,
	.dp_qos_queue_setup = ath12k_wifi7_dp_qos_queue_setup,
};

/* TODO: remove export once this file is built with wifi7 ko */
struct ath12k_dp *ath12k_wifi7_dp_init(struct ath12k_base *ab)
{
	struct hal_srng *srng;
	struct ath12k_dp *dp;
	u32 n_link_desc = 0;
	int ret;

	dp = kzalloc(sizeof(*dp), GFP_KERNEL);
	if (!dp)
		return NULL;

	dp->arch_ops = &ath12k_wifi7_dp_arch_ops;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	dp->ppe.ppeds_wlanops = &ppeds_wlanops_v2;
	dp->ppe.ppe_ops = &ath12k_wifi7_arch_ppeds_ops;
#endif
	dp->ab = ab;
	dp->dev = ab->dev;
	dp->hw_params = ab->hw_params;
	dp->hw_peer_stats_support = false;
	dp->hal = &ab->hal;
	dp->global_peer_id_supported = false;

	/* allocate ring desc here*/

	ab->dp = dp;

	ath12k_dp_init_ring_size(ab);

	ret = ath12k_wbm_idle_ring_alloc(ab, &n_link_desc);
	if (ret) {
		ath12k_err(ab, "wbm ring alloc failed %d\n", ret);
		goto dp_err;
	}

	srng = &ab->hal.srng_list[dp->wbm_idle_ring.ring_id];
	n_link_desc = dp->wbm_idle_ring.num_entries;

	ret = ath12k_dp_link_desc_alloc(ab, dp->link_desc_banks,
					HAL_WBM_IDLE_LINK, srng, n_link_desc);
	if (ret) {
		ath12k_err(ab, "dp link desc alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_dp_cc_rx_alloc(ab);
	if (ret) {
		ath12k_err(ab, "dp cc rx alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_dp_bank_profiles_alloc(ab);
	if (ret) {
		ath12k_err(ab, "dp bank profile alloc failed %d\n", ret);
		goto dp_err;
	}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
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

	ret = ath12k_dp_srng_common_alloc(ab);
	if (ret) {
		ath12k_err(ab, "dp srng common alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_wifi7_dp_tx_ring_alloc(ab);
	if (ret) {
		ath12k_err(ab, "dp tx ring alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_wifi7_dp_reoq_lut_alloc(ab);
	if (ret) {
		ath12k_err(ab, "dp reoq alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_wifi7_dp_rx_ring_alloc(ab);
	if (ret) {
		ath12k_err(ab, "dp rx ring alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_dp_mon_init(dp);
	if (ret) {
		ath12k_warn(dp, "dp_mon_init failed %d\n", ret);
		goto dp_err;
	}

	ath12k_wifi7_dp_mon_ops_register(dp);

	ath12k_dp_mon_cfg_init(dp);

	ret = ath12k_dp_mon_rx_alloc(dp);
	if (ret) {
		ath12k_err(ab, "dp mon rx alloc failed %d\n", ret);
		goto dp_err;
	}

	ret = ath12k_dp_mon_tx_srng_alloc(dp);
	if (ret) {
		ath12k_warn(ab, "Tx Mon: failed to setup rings ret = %d\n", ret);
		goto dp_err;
	}

	return dp;
dp_err:
	ath12k_wifi7_dp_deinit(dp);
	ab->dp = NULL;
	return NULL;
}

void ath12k_wifi7_dp_deinit(struct ath12k_dp *dp)
{
	ath12k_dp_mon_tx_srng_free(dp);
	ath12k_dp_mon_rx_free(dp);
	ath12k_dp_mon_deinit(dp);
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	ath12k_dp_ipa_plugin_deregister_ops_extn(dp->ab);
#endif
	ath12k_wifi7_dp_rx_ring_free(dp->ab);
	ath12k_wifi7_dp_reoq_lut_cleanup(dp->ab);
	ath12k_wifi7_dp_tx_ring_cleanup(dp->ab);
	ath12k_dp_srng_common_cleanup(dp->ab);
	ath12k_dp_bank_profiles_free(dp->ab);
	ath12k_dp_cc_rx_free(dp->ab);
	ath12k_dp_link_desc_cleanup(dp->ab, dp->link_desc_banks,
				    HAL_WBM_IDLE_LINK, &dp->wbm_idle_ring);
	ath12k_wbm_idle_ring_cleanup(dp->ab);
	kfree(dp);
}
