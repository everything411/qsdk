/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_ipa.h"
#include "../../../qcn_extns/ipa/dp_ipa.h"
#include "../../hal_qcn9625.h"
#include <linux/iommu.h>
#include "../../../qcn_extns/ini.h"

/**
 * ath12k_wifi8_dp_rx_ipa2wbm_idle_buf_config_qcn9625() - Configure IPA2WBM
 * idle buffer ring for QCN9625
 * @ab: Pointer to ath12k base structure
 *
 * Configures the IPA to WBM idle buffer ring with appropriate TLV filters
 * for the QCN9625 chipset. This includes setting up RX filters, MPDU/MSDU
 * offsets and word masks, and configuring the __source ring__ for IPA operation.
 *
 * Return: 0 on success, negative error code on failure
 */
int
ath12k_wifi8_dp_rx_ipa2wbm_idle_buf_config_qcn9625(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	if (!dp_ipa) {
		struct ath12k_dp *central_dp = ath12k_get_central_dp(dp);
		struct ath12k_dp_wifi8 *central_dp_wifi8;

		if (!central_dp) {
			ath12k_ipa_err(ab, "ath12k IPA idle_buf_config: device_id=%d central_dp is NULL, skip\n", ab->device_id);
			return -EINVAL;
		}

		central_dp_wifi8 = ath12k_get_dp_wifi8(central_dp);
		dp_ipa = ath12k_wifi8_get_ipa_dp(central_dp_wifi8);
		if (!dp_ipa) {
			ath12k_ipa_err(ab, "ath12k IPA idle_buf_config: device_id=%d central dp_ipa NULL, IPA not enabled\n",
				       ab->device_id);
			return 0; /* IPA not enabled on central chip */
		}
	}

	ring_id = dp_ipa->ipa2wbm_idle_buf_ring.ring_id;
	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 1;
	tlv_filter.fp_ctrl_filter = FILTER_CTRL_BA_REQ;
	tlv_filter.fp_data_filter = FILTER_DATA_UCAST | FILTER_DATA_MCAST |
		FILTER_DATA_NULL;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	tlv_filter.rx_mpdu_start_offset =
		ath12k_wifi8_hal_rx_desc_get_mpdu_start_offset_qcn9625();
	tlv_filter.rx_msdu_end_offset =
		ath12k_wifi8_hal_rx_desc_get_msdu_end_offset_qcn9625();

	tlv_filter.rx_mpdu_start_wmask =
		ath12k_wifi8_hal_rx_mpdu_start_wmask_get_qcn9625();
	tlv_filter.rx_msdu_end_wmask =
		ath12k_wifi8_hal_rx_msdu_end_wmask_get_qcn9625();

	/* Source ring configuration for IPA */
	tlv_filter.rdi_based_source_cfg =
		ath12k_wifi8_hal_get_rdi_source_cfg(ab, SOURCE_RING_CTRL_IPA);

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "rx_mpdu_start_wmask:0x%x rx_msdu_end_wmask:0x%x\n",
		   tlv_filter.rx_mpdu_start_wmask,
		   tlv_filter.rx_msdu_end_wmask);

	/* rx filter - PPEDS IDLE BUF ring */
	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
					       HAL_PPE2WBM_IDLE_BUF,
					       DP_RX_BUFFER_SIZE,
					       ATH12K_PKTLOG_DISABLED,
					       &tlv_filter);
	return ret;
}

/**
 * ath12k_wifi8_dp_fill_rx_src() - Fill IPA RX ring configuration
 * @ab: Pointer to ath12k base structure
 *
 * Populates the IPA RX resource structure with hardware ring parameters
 * for both the REO destination ring (reo2sw5) and the IPA2WBM refill ring.
 * This includes configuring:
 * - Ring base physical and virtual addresses
 * - Ring sizes and tail pointer addresses
 * - Scatter-gather tables for DMA operations
 * - IOMMU address translations when applicable
 *
 * The function extracts ring parameters from the HAL SRNG structures and
 * prepares them for IPA subsystem consumption. It handles both IOMMU-enabled
 * and non-IOMMU configurations by performing appropriate address translations.
 *
 * Return: None
 */
void ath12k_wifi8_dp_fill_rx_src(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_wifi8_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);
	struct iommu_domain *domain = ath12k_dp_ipa_smmu_get_domain(ab);
	u32 ring_id;
	struct hal_srng *srng;
	struct hal_srng_params params;
	dma_addr_t base_pa_addr;
	unsigned long offset;
	int ret;

	struct ath12k_ipa_dp_rx_rsc *rx_rsc = &ipa_ctx->ipa_uc_rx_rsc;

	ring_id = dp_wifi8_ipa->reo2sw5.ring_id;
	srng = &ab->hal.srng_list[ring_id];
	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);
	rx_rsc->ipa_reo_ring_base_paddr = params.ring_base_paddr;
	rx_rsc->ipa_reo_ring_base_vaddr = params.ring_base_vaddr;
	rx_rsc->ipa_reo_ring_size = (params.num_entries * srng->entry_size) << 2;

	offset = (unsigned long)srng->u.dst_ring.tp_addr_direct -
		 (unsigned long)ab->mem;
	base_pa_addr = (unsigned long)ab->ath12k_base_extn.mem_pa;
	rx_rsc->ipa_reo_tp_paddr = (dma_addr_t)(offset + base_pa_addr);

	if (domain)
		rx_rsc->reo_pa = iommu_iova_to_phys(domain,
						    rx_rsc->ipa_reo_ring_base_paddr);
	else
		rx_rsc->reo_pa = rx_rsc->ipa_reo_ring_base_paddr;

	ret = ath12k_dp_ipa_sg_tbl_setup(ab->dev, &rx_rsc->sgtable_reo,
					 rx_rsc->ipa_reo_ring_base_vaddr,
					 rx_rsc->reo_pa, rx_rsc->ipa_reo_ring_size);
	if (ret) {
		ath12k_err(ab, "reo sgtable setup failed");
		return;
	}

	ring_id = dp_wifi8_ipa->ipa2wbm_ring[ATH12K_DP_WIFI8_IPA2WBM_IPA].ring_id;
	srng = &ab->hal.srng_list[ring_id];
	memset(&params, 0, sizeof(params));
	ath12k_hal_srng_get_params(ab, srng, &params);
	rx_rsc->ipa_rx_refill_buf_ring_base_paddr = params.ring_base_paddr;
	rx_rsc->ipa_rx_refill_buf_ring_base_vaddr = params.ring_base_vaddr;
	rx_rsc->ipa_rx_refill_buf_ring_size =
		(params.num_entries * srng->entry_size) << 2;
	rx_rsc->ipa_rx_refill_buf_hp_paddr = ab->ath12k_base_extn.mem_pa +
		((unsigned long)srng->u.src_ring.hp_addr_direct - (unsigned long)ab->mem);

	if (domain)
		rx_rsc->refill_pa =
			iommu_iova_to_phys(domain,
					   rx_rsc->ipa_rx_refill_buf_ring_base_paddr);
	else
		rx_rsc->refill_pa = rx_rsc->ipa_rx_refill_buf_ring_base_paddr;

	ret = ath12k_dp_ipa_sg_tbl_setup(ab->dev, &rx_rsc->sgtable_refill.sgtable,
					 rx_rsc->ipa_rx_refill_buf_ring_base_vaddr,
					 rx_rsc->refill_pa,
					 rx_rsc->ipa_rx_refill_buf_ring_size);
	if (ret)
		ath12k_err(ab, "IPA2WBM refill sgtable setup failed: %d", ret);
}

/**
 * ath12k_wifi8_dp_ipa_ipa2wbm_srng_free() - Free IPA2WBM SRNG resources
 * @ab: Pointer to ath12k base structure
 *
 * Cleans up and frees all IPA to WBM ring (SRNG) resources including
 * the main IPA2WBM rings and the idle buffer ring. This function should be
 * called during teardown or error cleanup paths.
 *
 */
static void ath12k_wifi8_dp_ipa_ipa2wbm_srng_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_wifi8_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);
	int i;

	for (i = 0; i < ATH12K_DP_WIFI8_IPA2WBM_MAX; i++)
		ath12k_dp_srng_cleanup(ab, &dp_wifi8_ipa->ipa2wbm_ring[i]);

	ath12k_dp_srng_cleanup(ab, &dp_wifi8_ipa->ipa2wbm_idle_buf_ring);
}

/**
 * ath12k_wifi8_dp_ipa_ipa2wbm_srng_setup() - Setup IPA2WBM rings
 * @ab: Pointer to ath12k base structure
 *
 * Initializes and sets up all IPA to WBM rings.
 * These rings are used to replenish RX bufs to IPA2WBM pool.
 *
 * Return: 0 on success, negative error code on failure. On failure,
 *         all allocated rings are cleaned up automatically.
 */
static int ath12k_wifi8_dp_ipa_ipa2wbm_srng_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_wifi8_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);
	u32 host_ring_sz = ath12k_cfg_get(ab, ATH12K_INI_DP_IPA2WBM_HOST_RING_SZ);
	u32 ipa_ring_sz = ath12k_cfg_get(ab, ATH12K_INI_DP_IPA2WBM_IPA_RING_SZ);
	u32 idle_ring_sz = ath12k_cfg_get(ab, ATH12K_INI_DP_IPA2WBM_IDLE_RING_SZ);
	int ret;

	ret = ath12k_dp_srng_setup(ab,
					&dp_wifi8_ipa->ipa2wbm_ring[0],
					HAL_PPE2WBM_BUF, 0, 0,
					ipa_ring_sz);
	if (ret) {
		ath12k_ipa_err(ab, "failed to setup IPA2WBM_HOST refill ring (0)");
		goto fail;
	}

	ret = ath12k_dp_srng_setup(ab,
					&dp_wifi8_ipa->ipa2wbm_ring[1],
					HAL_PPE2WBM_BUF, 1, 0,
					host_ring_sz);
	if (ret) {
		ath12k_ipa_err(ab, "failed to setup IPA2WBM_IPA refill ring (1)");
		goto fail;
	}

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8_ipa->ipa2wbm_idle_buf_ring,
				   HAL_PPE2WBM_IDLE_BUF, 0, 0,
				   idle_ring_sz);
	if (ret) {
		ath12k_ipa_err(ab, "failed to setup wbm idle buf ring");
		goto fail;
	}

	return 0;
fail:
	ath12k_wifi8_dp_ipa_ipa2wbm_srng_free(ab);
	return ret;
}

/**
 * ath12k_wifi8_dp_ipa_ipa2wbm_idle_buf_ring_init() - Initialize IPA2WBM idle buffer ring
 * @ab: Pointer to ath12k base structure
 *
 * Provide the initial RX buffers to IPA2WBM pool.
 *
 * Return: 0 on success, -EINVAL if ring is invalid or not initialized
 */
int ath12k_wifi8_dp_ipa_ipa2wbm_idle_buf_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_wifi8_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);
	struct hal_srng *srng = NULL;
	size_t req_entries;
	u32 ring_id = dp_wifi8_ipa->ipa2wbm_idle_buf_ring.ring_id;
	LIST_HEAD(used_list);

	if (ring_id >= HAL_SRNG_RING_ID_MAX) {
		ath12k_ipa_err(ab, "Invalid IPA2WBM ring_id: %u", ring_id);
		return -EINVAL;
	}

	srng = &ab->hal.srng_list[ring_id];
	if (!srng->initialized) {
		ath12k_ipa_err(ab, "IPA2WBM ring not initialized");
		return -EINVAL;
	}

	req_entries = ath12k_dp_get_req_entries_from_buf_ring(ab, srng,
							      &used_list,
							      DP_RX_PPE_POOL);
	if (req_entries)
		ath12k_dp_rx_bufs_replenish(ab->dp, srng, &used_list, false);

	return 0;
}

/**
 * ath12k_wifi8_dp_ipa_reo_srng_free() - Free IPA REO destination ring
 * @ab: Pointer to ath12k base structure
 *
 * Cleans up and frees the REO2SW5 destination ring used for IPA offload.
 * This ring is used to receive reordered packets from the REO block that
 * are destined for IPA processing.
 *
 */
void ath12k_wifi8_dp_ipa_reo_srng_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_wifi8_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);

	ath12k_dp_srng_cleanup(ab, &dp_wifi8_ipa->reo2sw5);
}

/**
 * ath12k_wifi8_dp_ipa_reo_srng_setup() - Setup IPA REO destination ring
 * @ab: Pointer to ath12k base structure
 *
 * Initializes and configures the REO2SW5 destination ring for IPA offload
 * operation. All the offload packets will be steered to this ring.
 *
 * Return: 0 on success, negative error code on failure. On failure,
 *         allocated resources are automatically cleaned up.
 */
static int ath12k_wifi8_dp_ipa_reo_srng_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi8 *dp_wifi8 = ath12k_get_dp_wifi8(dp);
	struct ath12k_dp_wifi8_ipa *dp_wifi8_ipa = ath12k_wifi8_get_ipa_dp(dp_wifi8);
	u32 reo2sw5_ring_sz = ath12k_cfg_get(ab, ATH12K_INI_DP_IPA_REO2SW5_RING_SIZE);
	int ret;

	ret = ath12k_dp_srng_setup(ab, &dp_wifi8_ipa->reo2sw5,
				   HAL_REO_DST, ATH12K_DP_WIFI8_IPA_REO2SW5_IDX,
				   0, reo2sw5_ring_sz);
	if (ret) {
		ath12k_ipa_err(ab, "failed to setup reo_dst_ring");
		goto err_reo_cleanup;
	}

	return 0;

err_reo_cleanup:
	ath12k_wifi8_dp_ipa_reo_srng_free(ab);
	return ret;
}

/**
 * ath12k_wifi8_dp_rx_ring_setup_ipa() - Setup RX rings for IPA offload
 * @ab: Pointer to ath12k base structure
 *
 * Main entry point for setting up all rx rings required for IPA
 * offload operation. This function orchestrates the complete IPA RX
 * ring initialization sequence including:
 * - Setting up REO2SW5 and IPA2WBM rings
 * - Initializing idle buffer ring with RX buffers
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_wifi8_dp_rx_ring_setup_ipa(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wifi8_dp_ipa_reo_srng_setup(ab);
	if (ret) {
		ath12k_ipa_err(ab, "reo2sw5 ring setup failed");
		return ret;
	}

	ret = ath12k_wifi8_dp_ipa_ipa2wbm_srng_setup(ab);
	if (ret) {
		ath12k_ipa_err(ab, "ipa2wbm ring setup failed");
		goto err_reo_free;
	}

	return 0;

err_reo_free:
	ath12k_wifi8_dp_ipa_reo_srng_free(ab);
	return ret;
}

/*
 * ath12k_wifi8_dp_ipa_rx_buf_smmu_map_unmap() - Bulk IPA SMMU map/unmap for IPA RX pool
 * @ab: Pointer to ath12k base structure
 * @create: true to map, false to unmap
 *
 * Walks the RX descriptor pool and maps (or unmaps) every PPE/IPA pool buffer
 * into the IPA SMMU domain.  Called during IPA disconnect (create=false) to
 * release SMMU mappings that were created per-buffer inside
 * ath12k_dp_rx_bufs_replenish() at ring init time.
 *
 * Return: 0 on success, negative error code on first failure.
 */
int ath12k_wifi8_dp_ipa_rx_buf_smmu_map_unmap(struct ath12k_base *ab, bool create)
{
	struct ath12k_ipa *ipa_ctx = IPA_CTX(ab);
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_rx_desc_info *rx_desc;
	struct sk_buff *skb;
	unsigned int processed = 0, failed = 0;
	int i, j, ret = 0;

	if (!ipa_is_ready()) {
		ath12k_ipa_err(ab, "IPA is not ready");
		return 0;
	}

	spin_lock_bh(&dp->rx_desc_lock);
	for (i = 0; i < ATH12K_NUM_RX_SPT_PAGES; i++) {
		rx_desc = dp->rxbaddr[i];
		if (!rx_desc)
			continue;

		for (j = 0; j < ATH12K_MAX_SPT_ENTRIES; j++) {
			skb = rx_desc[j].skb;
			if (!skb)
				continue;

			/* For unmap: only release buffers that were mapped */
			if (!create &&
			    !(ATH12K_SKB_CB(skb)->flags & ATH12K_SKB_IPA_MAP_UNMAP))
				continue;

			ret = ath12k_dp_ipa_handle_buf_smmu_map_unmap(
					ab, skb, DP_RX_BUFFER_SIZE,
					create, ipa_ctx->hdl);
			if (ret) {
				ath12k_ipa_err(ab,
					       "IPA SMMU %s failed for RX buf: %d\n",
					       create ? "map" : "unmap", ret);
				failed++;
				goto unlock;
			}
			processed++;
		}
	}
unlock:
	spin_unlock_bh(&dp->rx_desc_lock);

	ath12k_ipa_info(ab, "ath12k IPA RX SMMU %s: processed=%u failed=%u\n",
			create ? "map" : "unmap", processed, failed);

	return ret;
}

/*
 * ath12k_wifi8_dp_rx_ring_free_ipa() - Free IPA RX ring resources
 * @ab: Pointer to ath12k base structure
 *
 * Tears down and frees all IPA receive ring resources. This is the
 * cleanup counterpart to ath12k_wifi8_dp_rx_ring_setup_ipa() and
 * is called from main ath12k.
 *
 * Make sure pipe_disconnect is done before this.
 *
 */
void ath12k_wifi8_dp_rx_ring_free_ipa(struct ath12k_base *ab)
{
	ath12k_wifi8_dp_ipa_reo_srng_free(ab);
	ath12k_wifi8_dp_ipa_ipa2wbm_srng_free(ab);
}
