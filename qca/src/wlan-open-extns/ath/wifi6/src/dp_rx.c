// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/kernel.h>
#include <linux/skbuff.h>
#include "../../debug.h"
#include "../../dp_mon.h"
#include "dp_rx.h"
#include "hal_qcn9074.h"

int ath12k_wifi6_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action)
{
	struct hal_wbm_release_ring *desc;
	struct ath12k_base *ab = dp->ab;
	struct hal_srng *srng;
	int ret = 0;

	srng = &ab->hal.srng_list[dp->wbm_desc_rel_ring.ring_id];

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, srng);

	desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
	if (!desc) {
		ret = -ENOBUFS;
		goto exit;
	}

	ath12k_wifi6_hal_rx_msdu_link_desc_set(ab, desc, buf_addr_info, action);

exit:
	ath12k_hal_srng_access_end(ab, srng);

	spin_unlock_bh(&srng->lock);

	return ret;
}

int ath12k_wifi6_dp_rx_process(struct ath12k_dp *dp, int ring_id,
			       struct napi_struct *napi, int budget)
{
	return 0;
}

int ath12k_wifi6_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget)
{
	return 0;
}

int ath12k_wifi6_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget)
{
	return 0;
}

int ath12k_wifi6_dp_rxdma_ring_sel_config_qcn9074(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 0;
	tlv_filter.fp_ctrl_filter = 0;
	tlv_filter.fp_data_filter = 0;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;

	ath12k_dbg(ab, ATH12K_DBG_DATA,
		   "Configuring compact tlv masks rx_mpdu_start_wmask 0x%x rx_msdu_end_wmask 0x%x\n",
		   tlv_filter.rx_mpdu_start_wmask, tlv_filter.rx_msdu_end_wmask);

	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
					       HAL_RXDMA_BUF,
					       DP_RX_BUFFER_SIZE,
					       ATH12K_PKTLOG_DISABLED,
					       &tlv_filter);
	return ret;
}

int ath12k_wifi6_dp_rxdma_ring_sel_config_qcn9160(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	u32 ring_id;
	int ret;
	u32 hal_rx_desc_sz = ab->hal.hal_desc_sz;

	ring_id = dp->rx_mac_buf_ring[0].ring_id;

	tlv_filter.rx_filter = HTT_RX_TLV_FLAGS_RXDMA_RING;
	tlv_filter.rxmon_disable = true;
	tlv_filter.enable_fp = 0;
	tlv_filter.fp_ctrl_filter = 0;
	tlv_filter.fp_data_filter = 0;
	tlv_filter.offset_valid = true;
	tlv_filter.rx_packet_offset = hal_rx_desc_sz;
	tlv_filter.rx_mpdu_end_offset = __cpu_to_le16(offsetof
			(struct hal_rx_desc_qcn9160, mpdu_end_tag));
	tlv_filter.rx_mpdu_start_offset = __cpu_to_le16(offsetof
			(struct hal_rx_desc_qcn9160, mpdu_start_tag));
	tlv_filter.rx_msdu_end_offset = __cpu_to_le16(offsetof
			(struct hal_rx_desc_qcn9160, msdu_end_tag));
	tlv_filter.rx_msdu_start_offset = __cpu_to_le16(offsetof
			(struct hal_rx_desc_qcn9160, msdu_start_tag));

	ret = ath12k_dp_tx_htt_rx_filter_setup(ab, ring_id, 0,
			HAL_RXDMA_BUF,
			DP_RX_BUFFER_SIZE,
			ATH12K_PKTLOG_DISABLED,
			&tlv_filter);
	return ret;
}

void ath12k_wifi6_dp_rx_process_reo_status(struct ath12k_dp *dp)
{
}

int ath12k_wifi6_dp_rx_htt_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	u32 ring_id;
	int i, ret;

	/* TODO: Need to verify the HTT setup for QCN9074 */
	ring_id = dp->rx_refill_buf_ring.refill_buf_ring.ring_id;
	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, 0, HAL_RXDMA_BUF);
	if (ret) {
		ath12k_warn(ab, "failed to configure rx_refill_buf_ring %d\n",
			    ret);
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
					i, HAL_RXDMA_BUF);
			if (ret) {
				ath12k_warn(ab, "failed to configure rx_mac_buf_ring%d %d\n",
						i, ret);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ring_id = dp->rxdma_err_dst_ring[i].ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id,
						  i, HAL_RXDMA_DST);
		if (ret) {
			ath12k_warn(ab, "failed to configure rxdma_err_dest_ring%d %d\n",
				    i, ret);
			return ret;
		}
	}

	ret = ath12k_dp_mon_rx_htt_setup(dp);
	if (ret) {
		ath12k_warn(ab, "Failed to setup rxdma monitor rings\n");
		return ret;
	}

	ret = ab->hw_params->hw_ops->rxdma_ring_sel_config(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma ring selection config\n");
		return ret;
	}

	return 0;
}

void ath12k_wifi6_dp_pdev_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k *ar;
	int i;

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], NULL);
	}
	spin_unlock_bh(&dp->dp_lock);

	synchronize_rcu();

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		ath12k_fw_stats_free(&ar->fw_stats);

		if (ar->dp.dp_mon_pdev_configured) {
			ath12k_dp_mon_pdev_rx_free(&ar->dp);
			ath12k_dp_mon_pdev_deinit(&ar->dp);

			ar->dp.dp_mon_pdev_configured = false;
		}
	}

}

int ath12k_wifi6_dp_pdev_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_pdev_dp *dp_pdev;
	struct ath12k *ar;
	int ret;
	int i;

	ret = ath12k_wifi6_dp_rx_htt_setup(ab);
	if (ret)
		goto out;

	/* TODO: Per-pdev rx ring unlike tx ring which is mapped to different AC's */
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		memset(&ar->stats, 0, sizeof(struct ath12k_pdev_ctrl_path_stats));
		dp_pdev = &ar->dp;

		dp_pdev->hw = ar->ah->hw;
		dp_pdev->dp = dp;
		/* Below linking is a temporary linking to handle few cases like cac
		 * timeout, active pdev etc in dp rx. Some flags/fileds can be added
		 * in dp_pdev to remove ar dependencies in the performance critical
		 * path.
		 *
		 * TODO: remove this once those dependencies are resolved.
		 */
		dp_pdev->ar = ar;
		dp_pdev->dp_hw = &ar->ah->dp_hw;
		dp_pdev->hw_link_id = ar->hw_link_id;

		if (!dp_pdev->dp_mon_pdev_configured) {
			ret = ath12k_dp_mon_pdev_init(dp_pdev);
			if (ret) {
				ath12k_warn(ab, "failed to initialize mon pdev %d\n", i);
				goto err;
			}

			ret = ath12k_dp_mon_pdev_rx_alloc(dp_pdev, i);
			if (ret)
				goto err;

			ret = ath12k_dp_mon_pdev_rx_htt_setup(dp_pdev, i);
			if (ret)
				goto err;

			dp_pdev->dp_mon_pdev_configured = true;
		}
	}

	spin_lock_bh(&dp->dp_lock);
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		rcu_assign_pointer(dp->dp_pdevs[ar->pdev_idx], &ar->dp);
	}
	spin_unlock_bh(&dp->dp_lock);

	dp->num_radios = ab->num_radios;


	return ret;
err:
	ath12k_wifi6_dp_pdev_free(ab);

out:
	return ret;
}

void ath12k_wifi6_dp_rx_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	int i;
	u8 *vaddr;
	dma_addr_t paddr;

	rx_desc_pool = &dp_wifi6->rx_desc_buf;

	spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
	if (!rx_desc_pool->mon_desc_pool) {
		spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
		return;
	}

	for (i = 0; i < rx_desc_pool->pool_size; i++) {
		if (rx_desc_pool->mon_desc_pool[i].in_use != 1)
			continue;

		vaddr = rx_desc_pool->mon_desc_pool[i].vaddr;
		paddr = rx_desc_pool->mon_desc_pool[i].paddr;

		if (!(rx_desc_pool->mon_desc_pool[i].unmapped)) {
			ath12k_core_dma_unmap_page(dp->dev, paddr,
						   rx_desc_pool->buf_size,
						   DMA_FROM_DEVICE);
			rx_desc_pool->mon_desc_pool[i].unmapped = 1;
		}
		if (vaddr)
			page_frag_free(vaddr);
		rx_desc_pool->mon_desc_pool[i].vaddr = NULL;
	}

	kfree(rx_desc_pool->mon_desc_pool);
	rx_desc_pool->mon_desc_pool = NULL;
	spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
}

void ath12k_wifi6_dp_rx_ring_free(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i;

	ath12k_dp_srng_cleanup(ab, &dp->rx_refill_buf_ring.refill_buf_ring);

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++)
			ath12k_dp_srng_cleanup(ab, &dp->rx_mac_buf_ring[i]);
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++)
		ath12k_dp_srng_cleanup(ab, &dp->rxdma_err_dst_ring[i]);
}

int ath12k_wifi6_dp_rx_ring_alloc(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret;

	ret = ath12k_dp_srng_alloc(ab,
				   &dp->rx_refill_buf_ring.refill_buf_ring,
				   HAL_RXDMA_BUF, 0, 0,
				   ath12k_wifi6_dp_get_rx_refill_ring_size(ab));
	if (ret) {
		ath12k_warn(ab, "failed to alloc rx_refill_buf_ring\n");
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ret = ath12k_dp_srng_alloc(ab,
						   &dp->rx_mac_buf_ring[i],
						   HAL_RXDMA_BUF, 1,
						   i, DP_RX_MAC_BUF_RING_SIZE);
			if (ret) {
				ath12k_warn(ab, "failed to alloc rx_mac_buf_ring %d\n",
					    i);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ret = ath12k_dp_srng_alloc(ab, &dp->rxdma_err_dst_ring[i],
					   HAL_RXDMA_DST, 0, i,
					   ath12k_wifi6_dp_get_rx_mon_err_dst_ring_size(ab));
		if (ret) {
			ath12k_warn(ab, "failed to alloc rxdma_err_dst_ring %d\n", i);
			return ret;
		}
	}

	return 0;
}

int ath12k_wifi6_dp_rx_ring_init(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	int i, ret;

	ret = ath12k_dp_srng_init(ab,
				  &dp->rx_refill_buf_ring.refill_buf_ring,
				  HAL_RXDMA_BUF, 0, 0);
	if (ret) {
		ath12k_warn(ab, "failed to init rx_refill_buf_ring\n");
		return ret;
	}

	if (ab->hw_params->rx_mac_buf_ring) {
		for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
			ret = ath12k_dp_srng_init(ab,
						  &dp->rx_mac_buf_ring[i],
						  HAL_RXDMA_BUF, 1, i);
			if (ret) {
				ath12k_warn(ab, "failed to init rx_mac_buf_ring %d\n",
					    i);
				return ret;
			}
		}
	}

	for (i = 0; i < ab->hw_params->num_rxdma_dst_ring; i++) {
		ret = ath12k_dp_srng_init(ab, &dp->rxdma_err_dst_ring[i],
					  HAL_RXDMA_DST, 0, i);
		if (ret) {
			ath12k_warn(ab, "failed to init rxdma_err_dst_ring %d\n", i);
			return ret;
		}
	}

	ret = ath12k_wifi6_dp_rxdma_buf_setup(ab);
	if (ret) {
		ath12k_warn(ab, "failed to setup rxdma ring\n");
		return ret;
	}

	return 0;
}

size_t ath12k_wifi6_dp_list_cut_nodes(struct list_head *list,
				      struct list_head *head, size_t count)
{
	struct list_head *cur;
	struct ath12k_dp_wifi6_rx_desc_info *rx_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(list);
		goto out;
	}

	list_for_each(cur, head) {
		if (!count)
			break;

		rx_desc = list_entry(cur, struct ath12k_dp_wifi6_rx_desc_info, list);
		ath12k_wifi6_dp_rx_desc_reset(rx_desc);
		count--;
		nodes++;
	}

	list_cut_before(list, head, cur);

out:
	return nodes;
}

size_t ath12k_wifi6_dp_get_req_entries_from_buf_ring(struct ath12k_dp *dp,
						     struct dp_srng *rx_ring,
						     struct list_head *list,
						     struct ath12k_dp_rx_desc_pool *rx_desc_pool)
{
	struct hal_srng *srng;
	struct ath12k_base *ab = dp->ab;
	size_t num_free, req_entries;

	srng = &dp->hal->srng_list[rx_ring->ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!num_free) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return 0;
	}
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
	req_entries = ath12k_wifi6_dp_list_cut_nodes(list,
						     &rx_desc_pool->rx_desc_free_list,
						     num_free);
	spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);

	return req_entries;
}

int ath12k_wifi6_dp_rx_desc_pool_alloc(struct ath12k_dp *dp,
				       struct ath12k_dp_rx_desc_pool *rx_desc_pool,
				       u16 ring_size)
{
	rx_desc_pool->mon_desc_pool = kcalloc(ring_size,
					      sizeof(*rx_desc_pool->mon_desc_pool),
					      GFP_KERNEL);
	if (!rx_desc_pool->mon_desc_pool)
		return -ENOMEM;

	return 0;
}

int ath12k_wifi6_rx_desc_pool_init(struct ath12k_dp *dp,
				   struct ath12k_dp_rx_desc_pool *rx_desc_pool,
				   int mac_id, u16 ring_size)
{
	int i;
	u8 pool_id = mac_id;

	if (!rx_desc_pool->mon_desc_pool)
		return -EINVAL;

	INIT_LIST_HEAD(&rx_desc_pool->rx_desc_free_list);
	spin_lock_init(&rx_desc_pool->rx_mon_desc_lock);

	spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);

	for (i = 0; i < ring_size; i++) {
		rx_desc_pool->mon_desc_pool[i].cookie = i | (pool_id <<
					ATH12K_WIFI6_RX_DESC_COOKIE_POOL_ID_SHIFT);
		rx_desc_pool->mon_desc_pool[i].pool_id = pool_id;
		rx_desc_pool->mon_desc_pool[i].in_use = 0;
		rx_desc_pool->mon_desc_pool[i].unmapped = 1;
		INIT_LIST_HEAD(&rx_desc_pool->mon_desc_pool[i].list);
		list_add_tail(&rx_desc_pool->mon_desc_pool[i].list,
			      &rx_desc_pool->rx_desc_free_list);
	}
	spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);

	rx_desc_pool->pool_size = ring_size;
	rx_desc_pool->owner = ath12k_wifi6_dp_rx_get_rbm_id(dp);
	rx_desc_pool->buf_alignment = DP_RX_BUFFER_ALIGN_SIZE;

	return 0;
}

static void
ath12k_wifi6_dp_rx_handle_desc(struct ath12k_dp *dp,
			       struct ath12k_dp_wifi6_rx_desc_info *rx_desc,
			       struct ath12k_dp_rx_desc_pool *rx_desc_pool)
{
	u8 *vaddr = rx_desc->vaddr;

	ath12k_dbg(dp->ab, ATH12K_DBG_DP_MON,
		   "rx_handle_desc: cookie 0x%x pool %u cleared in_use=%u unmapped=%u vaddr=%p\n",
		   rx_desc->cookie, rx_desc->pool_id,
		   rx_desc->in_use, rx_desc->unmapped, vaddr);
	if (vaddr) {
		ath12k_core_dma_unmap_page(dp->dev, rx_desc->paddr,
					   rx_desc_pool->buf_size,
					   DMA_FROM_DEVICE);
		page_frag_free(vaddr);
		rx_desc->vaddr = NULL;
		/* Reflect that the buffer is no longer in use
		 * and its DMA mapping has been torn down.
		 */
		rx_desc->in_use = 0;
		rx_desc->unmapped = 1;
	}

	list_del(&rx_desc->list);
	spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
	list_add_tail(&rx_desc->list, &rx_desc_pool->rx_desc_free_list);
	spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
}

int ath12k_wifi6_dp_buf_replenish(struct ath12k_dp *dp,
				  struct dp_srng *buf_ring,
				  struct list_head *used_list,
				  int req_entries,
				  struct ath12k_dp_rx_desc_pool *rx_desc_pool)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_buffer_addr *desc;
	struct hal_srng *srng;
	struct ath12k_dp_wifi6_rx_desc_info *rx_desc, *tmp_rx_desc;
	struct page *page;
	dma_addr_t paddr;
	u8 *vaddr;
	unsigned long offset;
	int allocated_entries = 0;
	int ret = 0;

	list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {
		if (unlikely(rx_desc->in_use != 0)) {
			ath12k_warn(dp,
				    "Invalid in_use %d, possibly already in use desc\n",
				     rx_desc->in_use);
			ath12k_wifi6_dp_rx_handle_desc(dp, rx_desc, rx_desc_pool);
			continue;
		}

		vaddr = page_frag_alloc(&rx_desc_pool->rx_mon_pf_cache,
					rx_desc_pool->buf_size,
					GFP_ATOMIC);
		if (unlikely(!vaddr)) {
			ret = -ENOMEM;
			goto out;
		}
		page = virt_to_head_page(vaddr);
		offset = ((void *)vaddr) - page_address(page);
		paddr = ath12k_core_dma_map_page(ab->dev, page, offset,
						 rx_desc_pool->buf_size,
						 DMA_FROM_DEVICE);
		if (unlikely(dma_mapping_error(ab->dev, paddr))) {
			page_frag_free(vaddr);
			rx_desc->vaddr = NULL;
			ret = -EIO;
			goto out;
		}
		allocated_entries++;

		rx_desc->vaddr = vaddr;
		rx_desc->paddr = paddr;
		rx_desc->magic = ATH12K_DP_RX_DESC_MAGIC;
		rx_desc->in_use = 1;
		rx_desc->unmapped = 0;

		ath12k_dbg(ab, ATH12K_DBG_DP_MON,
			   "replenish: cookie 0x%x pool %u in_use=%u unmapped=%u paddr=%p vaddr=%p\n",
			   rx_desc->cookie, rx_desc->pool_id,
			   rx_desc->in_use, rx_desc->unmapped, &rx_desc->paddr, rx_desc->vaddr);
	}

	srng = &ab->hal.srng_list[buf_ring->ring_id];
	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);
	while (allocated_entries > 0) {
		rx_desc = list_first_entry_or_null(used_list,
						   struct ath12k_dp_wifi6_rx_desc_info,
						   list);
		if (unlikely(!rx_desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		desc = ath12k_hal_srng_src_get_next_entry(ab, srng);
		if (unlikely(!desc)) {
			ret = -ENOSPC;
			goto ring_unlock;
		}

		list_del(&rx_desc->list);
		allocated_entries--;

		ath12k_wifi6_hal_rx_buf_addr_info_set(desc, rx_desc->paddr,
						      rx_desc->cookie,
						      rx_desc_pool->owner);
	}

ring_unlock:
	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

out:
	if (unlikely(!list_empty(used_list))) {
		list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {

			ath12k_dbg(ab, ATH12K_DBG_DP_MON,
				   "replenish-out: cookie 0x%x pool %u in_use=%u unmapped=%u\n",
				   rx_desc->cookie, rx_desc->pool_id,
				   rx_desc->in_use, rx_desc->unmapped);

			vaddr = rx_desc->vaddr;
			if (vaddr) {
				ath12k_core_dma_unmap_page(ab->dev, rx_desc->paddr,
							   rx_desc_pool->buf_size,
							   DMA_FROM_DEVICE);
				page_frag_free(vaddr);
				rx_desc->vaddr = NULL;
			}
			ath12k_wifi6_dp_rx_desc_reset(rx_desc);
			rx_desc->in_use = 0;
			rx_desc->unmapped = 1;
		}

		spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
		list_splice_tail(used_list, &rx_desc_pool->rx_desc_free_list);
		spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
	}

	return ret;
}

int ath12k_wifi6_dp_rxdma_buf_setup(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ab);
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	int num_entries;
	struct dp_rxdma_ring *rx_ring;
	struct dp_srng *rx_buf_srng;
	size_t req_entries;
	LIST_HEAD(list);
	u16 ring_size;
	int ret;

	rx_desc_pool = &dp_wifi6->rx_desc_buf;
	ring_size = ath12k_wifi6_dp_get_rx_refill_ring_size(ab);

	ret = ath12k_wifi6_dp_rx_desc_pool_alloc(dp, rx_desc_pool, ring_size);
	if (ret) {
		ath12k_warn(dp, "rx desc pool alloc failed %d\n", ret);
		return ret;
	}

	ret = ath12k_wifi6_rx_desc_pool_init(dp, rx_desc_pool, 0, ring_size);
	if (ret) {
		ath12k_warn(dp, "rx desc pool init failed %d\n", ret);
		return ret;
	}
	rx_desc_pool->dest_frag_enable =  true;
	rx_desc_pool->buf_size = ATH12K_DP_MON_RX_BUF_SIZE;

	rx_ring = &dp->rx_refill_buf_ring;
	rx_buf_srng = &rx_ring->refill_buf_ring;

	num_entries = rx_ring->refill_buf_ring.size /
			ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_BUF);

	rx_ring->bufs_max = num_entries;

	req_entries = ath12k_wifi6_dp_get_req_entries_from_buf_ring(dp, rx_buf_srng,
								    &list,
								    rx_desc_pool);
	if (req_entries)
		ret = ath12k_wifi6_dp_buf_replenish(dp, rx_buf_srng, &list, req_entries,
						    rx_desc_pool);
	else
		ath12k_warn(dp, "No required entries available for mon buf ring\n");

	return ret;
}
