// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/* This file contains the definitions related to the monitor quad ring
 * model
 */

#include "../../dp_mon.h"
#include "dp_mon.h"
#include "../../debug.h"
#include "hal_mon.h"
#include "dp_rx.h"
#include "../../dp_mon_filter.h"
#include "hal_qcn9074.h"
#include "../ath12k_cmn_extn.h"

/* Allocate undecoded_meta and phyrx_abort_info if not already done.
 * Called by both ops since either may arrive first.
 */
static int
ath12k_wifi6_dp_mon_undecoded_alloc(struct ath12k_pdev_mon_dp_extn *extn)
{
	if (!extn->undecoded_meta) {
		extn->undecoded_meta =
			kzalloc(sizeof(*extn->undecoded_meta), GFP_KERNEL);
		if (!extn->undecoded_meta) {
			ath12k_err(NULL, "undecoded: failed to allocate undecoded_meta\n");
			return -ENOMEM;
		}
	}
	if (!extn->phyrx_abort_info) {
		extn->phyrx_abort_info =
			kzalloc(sizeof(*extn->phyrx_abort_info), GFP_KERNEL);
		if (!extn->phyrx_abort_info) {
			ath12k_err(NULL, "undecoded: failed to allocate phyrx_abort_info\n");
			kfree(extn->undecoded_meta);
			extn->undecoded_meta = NULL;
			return -ENOMEM;
		}
	}
	return 0;
}

static void
ath12k_wifi6_dp_mon_undecoded_free(struct ath12k_pdev_mon_dp_extn *extn)
{
	kfree(extn->phyrx_abort_info);
	extn->phyrx_abort_info = NULL;
	kfree(extn->undecoded_meta);
	extn->undecoded_meta = NULL;
}

static int
ath12k_wifi6_dp_mon_rx_undecoded_metadata_capture_set(struct ath12k_pdev_dp *dp_pdev,
						      u32 value)
{
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (!dp_pdev->dp_mon_pdev)
		return -EINVAL;

	mon_dp_extn = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;
	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);

	if (value) {
		ret = ath12k_wifi6_dp_mon_undecoded_alloc(mon_dp_extn);
		if (ret)
			return ret;
		mon_dp_extn->undecoded_meta->undecoded_metadata_capture = true;
		if (mon_ops && mon_ops->rx_undecoded_metadata_config_filter)
			mon_ops->rx_undecoded_metadata_config_filter(dp_pdev);
	} else {
		if (mon_dp_extn->undecoded_meta)
			mon_dp_extn->undecoded_meta->undecoded_metadata_capture = false;
		if (mon_ops && mon_ops->rx_undecoded_metadata_reset_filter)
			mon_ops->rx_undecoded_metadata_reset_filter(dp_pdev);
		ath12k_wifi6_dp_mon_undecoded_free(mon_dp_extn);
	}
	return 0;
}

static int
ath12k_wifi6_dp_mon_rx_undecoded_phy_err_mask_set(struct ath12k_pdev_dp *dp_pdev,
						  u32 mask, u32 mask_cont)
{
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	int ret;

	if (!dp_pdev->dp_mon_pdev)
		return -EINVAL;

	mon_dp_extn = &dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;

	ret = ath12k_wifi6_dp_mon_undecoded_alloc(mon_dp_extn);
	if (ret)
		return ret;

	mon_dp_extn->undecoded_meta->phyrx_error_mask = mask;
	mon_dp_extn->undecoded_meta->phyrx_error_mask_cont = mask_cont;

	if (!mon_dp_extn->undecoded_meta->undecoded_metadata_capture)
		return 0;

	mon_ops = ath12k_dp_mon_ops_get(dp_pdev->dp);
	if (mon_ops && mon_ops->rx_undecoded_metadata_config_filter)
		mon_ops->rx_undecoded_metadata_config_filter(dp_pdev);
	return 0;
}

const struct ath12k_dp_arch_mon_ops ath12k_wifi6_dp_arch_mon_quad_ring_ops = {
	.rx_srng_setup = ath12k_wifi6_dp_mon_rx_srng_setup,
	.rx_srng_cleanup = ath12k_wifi6_dp_mon_rx_srng_cleanup,
	.rx_ring_init = ath12k_wifi6_dp_mon_rx_ring_init,
	.rx_ring_deinit = ath12k_wifi6_dp_mon_rx_ring_deinit,
	.rx_htt_srng_setup = ath12k_wifi6_dp_mon_rx_htt_srng_setup,
	.mon_pdev_alloc = ath12k_dp_mon_pdev_alloc,
	.mon_pdev_free = ath12k_dp_mon_pdev_free,
	.mon_pdev_rx_srng_setup = ath12k_wifi6_dp_mon_pdev_rx_srng_setup,
	.mon_pdev_rx_srng_cleanup = ath12k_wifi6_dp_mon_pdev_rx_srng_cleanup,
	.mon_pdev_rx_htt_srng_setup = ath12k_wifi6_dp_mon_pdev_rx_htt_srng_setup,
	.mon_pdev_rx_attach = ath12k_dp_mon_pdev_rx_attach,
	.setup_mon_link_desc = ath12k_wifi6_mon_setup_mon_link_desc,
	.cleanup_mon_link_desc = ath12k_wifi6_mon_cleanup_mon_link_desc,
	.mon_pdev_rx_mpdu_list_init = NULL,
	.mon_rx_srng_process = ath12k_wifi6_dp_mon_rx_quad_ring_process,
	.update_telemetry_stats = NULL,
	.rx_filter_alloc = ath12k_dp_mon_rx_filter_alloc,
	.rx_filter_free = ath12k_dp_mon_rx_filter_free,
	.rx_stats_enable = NULL,
	.rx_stats_disable = NULL,
	.rx_filter_update = ath12k_wifi6_dp_mon_rx_update_ring_filter,
	.rx_monitor_mode_set = ath12k_wifi6_dp_mon_rx_monitor_mode_set,
	.rx_monitor_mode_reset = ath12k_wifi6_dp_mon_rx_monitor_mode_reset,
	.rx_undecoded_metadata_config_filter = ath12k_wifi6_dp_mon_rx_undecoded_metadata_config_filter,
	.rx_undecoded_metadata_reset_filter = ath12k_wifi6_dp_mon_rx_undecoded_metadata_reset_filter,
	.rx_undecoded_metadata_capture_set = ath12k_wifi6_dp_mon_rx_undecoded_metadata_capture_set,
	.rx_undecoded_phy_err_mask_set = ath12k_wifi6_dp_mon_rx_undecoded_phy_err_mask_set,
	.setup_ppdu_desc = NULL,
	.cleanup_ppdu_desc = NULL,
	.rx_nrp_set = NULL,
	.rx_nrp_reset = NULL,
	.mon_rx_wmask = NULL,
	.rx_enable_packet_filters = NULL,
	.pktlog_config = NULL,
	.htt_rx_filter_rxmon_cfg = NULL,
	.htt_rx_phy_err_filter_cfg = ath12k_wifi6_dp_mon_rx_htt_phy_err_filter_cfg,
};

static int ath12k_wifi6_dp_mon_rx_buf_replenish(struct ath12k_dp *dp);
static int ath12k_wifi6_dp_mon_rx_pool_alloc(struct ath12k_dp *dp);
static int ath12k_wifi6_dp_mon_rx_pool_init(struct ath12k_dp *dp);
static void ath12k_wifi6_dp_mon_rx_status_pool_free(struct ath12k_dp *dp, int mac_id);
static void ath12k_wifi6_dp_mon_rx_buf_ring_pool_free(struct ath12k_dp *dp);

int ath12k_wifi6_dp_mon_rx_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ret = ath12k_dp_srng_alloc(ab, srng,
					   HAL_RXDMA_MONITOR_STATUS, 0, i,
					   HAL_RX_MON_STATUS_RING_SIZE);
		if (ret) {
			ath12k_warn(dp, "failed to alloc mon status ring %d\n", i);
			return ret;
		}
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ret = ath12k_dp_srng_alloc(ab,
					   &dp_mon->rxdma_mon_buf_ring.refill_buf_ring,
					   HAL_RXDMA_MONITOR_BUF, 0, 0,
					   ath12k_wifi6_dp_get_rx_mon_refill_ring_size(ab));
		if (ret) {
			ath12k_warn(dp, "failed to alloc HAL_RXDMA_MONITOR_BUF %d\n", ret);
			return ret;
		}

		ret = ath12k_dp_srng_alloc(ab, &dp_mon->rxdma_mon_desc_ring,
					   HAL_RXDMA_MONITOR_DESC,
					   0, 0,
					   HAL_RX_MON_DESC_RING_SIZE);
		if (ret) {
			ath12k_warn(dp, "failed to alloc mon desc ring %d\n", ret);
			return ret;
		}
	}

	ret = ath12k_wifi6_dp_mon_rx_pool_alloc(dp);
	if (ret) {
		ath12k_warn(dp, "failed to alloc mon rx desc pool %d\n", ret);
		return ret;
	}

	return 0;
}

int ath12k_wifi6_dp_mon_rx_ring_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ret = ath12k_dp_srng_init(ab, srng,
					  HAL_RXDMA_MONITOR_STATUS, 0, i);
		if (ret) {
			ath12k_warn(dp, "failed to init mon status ring %d\n", i);
			return ret;
		}
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ret = ath12k_dp_srng_init(ab,
					  &dp_mon->rxdma_mon_buf_ring.refill_buf_ring,
					  HAL_RXDMA_MONITOR_BUF, 0, 0);
		if (ret) {
			ath12k_warn(dp, "failed to init HAL_RXDMA_MONITOR_BUF %d\n", ret);
			return ret;
		}

		ret = ath12k_dp_srng_init(ab, &dp_mon->rxdma_mon_desc_ring,
					  HAL_RXDMA_MONITOR_DESC, 0, 0);
		if (ret) {
			ath12k_warn(dp, "failed to init mon desc ring %d\n", ret);
			return ret;
		}
	}

	ret = ath12k_wifi6_dp_mon_rx_pool_init(dp);
	if (ret) {
		ath12k_warn(dp, "failed to init mon rx desc pool %d\n", ret);
		return ret;
	}

	ret = ath12k_wifi6_dp_mon_rx_buf_replenish(dp);
	if (ret) {
		ath12k_warn(dp, "failed to replenish mon rx buffers %d\n", ret);
		return ret;
	}

	return 0;
}

void ath12k_wifi6_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct dp_srng *srng;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		srng = &dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring;
		ath12k_dp_srng_cleanup(ab, srng);
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ath12k_dp_srng_cleanup(ab, &dp_mon->rxdma_mon_desc_ring);
		ath12k_dp_srng_cleanup(ab, &dp_mon->rxdma_mon_buf_ring.refill_buf_ring);
	}

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++)
		ath12k_wifi6_dp_mon_rx_status_pool_free(dp, i);

	if (ath12k_dp_mon_rxdma1_enable(dp))
		ath12k_wifi6_dp_mon_rx_buf_ring_pool_free(dp);
}

static int
ath12k_wifi6_dp_mon_rx_status_bufs_replenish(struct ath12k_dp *dp,
					     struct dp_rxdma_mon_ring *rx_ring,
					     struct list_head *used_list,
					     int req_entries,
					     struct ath12k_dp_rx_desc_pool *rx_desc_pool)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_buffer_addr *desc;
	struct hal_srng *srng;
	struct ath12k_dp_wifi6_rx_desc_info *rx_desc, *tmp_rx_desc;
	struct sk_buff *skb;
	dma_addr_t paddr;
	int ret = 0;
	int allocated_entries = 0;

	list_for_each_entry_safe(rx_desc, tmp_rx_desc, used_list, list) {
		skb = dev_alloc_skb(rx_desc_pool->buf_size);
		if (!skb)
			break;

		skb_reserve(skb, RX_MON_STATUS_BUF_RESERVATION);
		if (!IS_ALIGNED((unsigned long)skb->data,
				RX_MON_STATUS_BUF_ALIGN)) {
			skb_pull(skb,
				 PTR_ALIGN(skb->data, RX_MON_STATUS_BUF_ALIGN) -
				 skb->data);
		}

		memset(skb->data, 0, RX_MON_STATUS_BUF_SIZE);
		paddr = dma_map_single(ab->dev, skb->data,
				       rx_desc_pool->buf_size,
				       DMA_FROM_DEVICE);
		if (dma_mapping_error(ab->dev, paddr)) {
			dev_kfree_skb_any(skb);
			rx_desc->skb = NULL;
			ret = -EIO;
			 goto out;
		}

		allocated_entries++;
		rx_desc->skb = skb;
		rx_desc->paddr = ATH12K_SKB_RXCB(skb)->paddr = paddr;
		rx_desc->in_use = 1;
		rx_desc->unmapped = 0;

	}

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
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

		desc = ath12k_wifi6_hal_srng_src_get_cur_desc_n_move_next(ab, srng);
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
			skb = rx_desc->skb;
			if (skb) {
				dma_unmap_single(ab->dev, rx_desc->paddr,
						 rx_desc_pool->buf_size,
						 DMA_FROM_DEVICE);
				dev_kfree_skb_any(skb);
				rx_desc->skb = NULL;
				rx_desc->unmapped = 1;
			}
			rx_desc->in_use = 0;
		}
		spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
		list_splice_tail(used_list, &rx_desc_pool->rx_desc_free_list);
		spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
	}

	return ret;
}

static int
ath12k_wifi6_dp_mon_rx_status_pool_alloc(struct ath12k_dp *dp, int mac_id)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u16 ring_size;
	int ret;

	rx_desc_pool = &dp_wifi6->rx_desc_status[mac_id];
	ring_size = HAL_RX_MON_STATUS_RING_SIZE;

	ret = ath12k_wifi6_dp_rx_desc_pool_alloc(dp, rx_desc_pool, ring_size);
	if (ret) {
		ath12k_warn(dp, "rx desc pool alloc failed %d\n", ret);
		return ret;
	}

	rx_desc_pool->dest_frag_enable = false;
	rx_desc_pool->buf_size = RX_MON_STATUS_BUF_SIZE;

	return 0;
}

static int
ath12k_wifi6_dp_mon_rx_status_pool_init(struct ath12k_dp *dp, int mac_id)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u16 ring_size;
	int ret;

	rx_desc_pool = &dp_wifi6->rx_desc_status[mac_id];
	ring_size = HAL_RX_MON_STATUS_RING_SIZE;

	ret = ath12k_wifi6_rx_desc_pool_init(dp, rx_desc_pool, mac_id, ring_size);
	if (ret) {
		ath12k_warn(dp, "rx desc pool init failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int
ath12k_wifi6_dp_mon_rx_status_buf_replenish(struct ath12k_dp *dp, int mac_id)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	struct dp_rxdma_mon_ring *rx_ring;
	struct dp_srng *rx_status_srng;
	int num_entries;
	size_t req_entries;
	LIST_HEAD(list);
	int ret = 0;

	rx_desc_pool = &dp_wifi6->rx_desc_status[mac_id];
	rx_ring = &dp_mon->rx_mon_status_refill_ring[mac_id];
	rx_status_srng = &rx_ring->refill_buf_ring;

	num_entries = rx_ring->refill_buf_ring.size /
		ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_STATUS);
	rx_ring->bufs_max = num_entries;

	req_entries = ath12k_wifi6_dp_get_req_entries_from_buf_ring(dp, rx_status_srng,
								    &list,
								    rx_desc_pool);
	if (req_entries)
		ret = ath12k_wifi6_dp_mon_rx_status_bufs_replenish(dp,
								   rx_ring, &list,
								   req_entries,
								   rx_desc_pool);
	else
		ath12k_warn(dp, "No required entries available for mon buf ring\n");

	return ret;
}

static int
ath12k_wifi6_dp_mon_buf_pool_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u16 ring_size;
	int ret;

	rx_desc_pool = &dp_wifi6->rx_desc_mon;
	ring_size = ath12k_wifi6_dp_get_rx_mon_refill_ring_size(ab);

	ret = ath12k_wifi6_dp_rx_desc_pool_alloc(dp, rx_desc_pool, ring_size);
	if (ret) {
		ath12k_warn(dp, "rx desc pool alloc failed %d\n", ret);
		return ret;
	}

	rx_desc_pool->dest_frag_enable = true;
	rx_desc_pool->buf_size = ATH12K_DP_MON_RX_BUF_SIZE;

	return 0;
}

static int
ath12k_wifi6_dp_mon_buf_pool_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u16 ring_size;
	int ret;

	rx_desc_pool = &dp_wifi6->rx_desc_mon;
	ring_size = ath12k_wifi6_dp_get_rx_mon_refill_ring_size(ab);

	ret = ath12k_wifi6_rx_desc_pool_init(dp, rx_desc_pool, 0, ring_size);
	if (ret) {
		ath12k_warn(dp, "rx desc pool init failed %d\n", ret);
		return ret;
	}

	return 0;
}

static int
ath12k_wifi6_dp_mon_buf_ring_replenish(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	int num_entries;
	struct dp_rxdma_mon_ring *rx_ring;
	struct dp_srng *rx_mon_buf_srng;
	size_t req_entries;
	LIST_HEAD(list);
	int ret = 0;

	rx_desc_pool = &dp_wifi6->rx_desc_mon;
	rx_ring = &dp_mon->rxdma_mon_buf_ring;
	rx_mon_buf_srng = &rx_ring->refill_buf_ring;

	num_entries = rx_ring->refill_buf_ring.size /
			ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_BUF);

	rx_ring->bufs_max = num_entries;

	req_entries = ath12k_wifi6_dp_get_req_entries_from_buf_ring(dp, rx_mon_buf_srng,
								    &list,
								    rx_desc_pool);
	if (req_entries) {
		ret = ath12k_wifi6_dp_buf_replenish(dp, rx_mon_buf_srng, &list,
						    req_entries, rx_desc_pool);
	} else {
		ath12k_warn(dp, "No required entries available for mon buf ring\n");
		ret = -EINVAL;
	}

	return ret;
}

static int ath12k_wifi6_dp_mon_rx_pool_alloc(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	int i, ret = 0;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ret = ath12k_wifi6_dp_mon_rx_status_pool_alloc(dp, i);
		if (ret) {
			ath12k_err(ab, "failed to alloc monitor status ring pool\n");
			return ret;
		}
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ret = ath12k_wifi6_dp_mon_buf_pool_alloc(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static int ath12k_wifi6_dp_mon_rx_pool_init(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	int i, ret = 0;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ret = ath12k_wifi6_dp_mon_rx_status_pool_init(dp, i);
		if (ret) {
			ath12k_err(ab, "failed to init monitor status ring pool\n");
			return ret;
		}
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ret = ath12k_wifi6_dp_mon_buf_pool_init(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static int ath12k_wifi6_dp_mon_rx_buf_replenish(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	int i, ret = 0;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ret = ath12k_wifi6_dp_mon_rx_status_buf_replenish(dp, i);
		if (ret) {
			ath12k_err(ab, "failed to init monitor status ring\n");
			return ret;
		}
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ret = ath12k_wifi6_dp_mon_buf_ring_replenish(dp);
		if (ret)
			return ret;
	}

	return 0;
}

static
void ath12k_wifi6_dp_mon_rx_status_buf_free(struct ath12k_dp *dp, int mac_id)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	struct sk_buff *skb;
	int i;
	dma_addr_t paddr;

	rx_desc_pool = &dp_wifi6->rx_desc_status[mac_id];
	spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);

	if (!rx_desc_pool->mon_desc_pool) {
		spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
		return;
	}

	for (i = 0; i < rx_desc_pool->pool_size; i++) {
		if (rx_desc_pool->mon_desc_pool[i].in_use != 1)
			continue;
		skb = rx_desc_pool->mon_desc_pool[i].skb;
		paddr = rx_desc_pool->mon_desc_pool[i].paddr;

		if (!(rx_desc_pool->mon_desc_pool[i].unmapped)) {
			dma_unmap_single(ab->dev, paddr, rx_desc_pool->buf_size,
					 DMA_FROM_DEVICE);
			rx_desc_pool->mon_desc_pool[i].unmapped = 1;
		}
		dev_kfree_skb_any(skb);
		rx_desc_pool->mon_desc_pool[i].skb = NULL;
		rx_desc_pool->mon_desc_pool[i].in_use = 0;
	}

	spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
}

static
void ath12k_wifi6_dp_mon_rx_status_pool_free(struct ath12k_dp *dp, int mac_id)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &dp_wifi6->rx_desc_status[mac_id];
	kfree(rx_desc_pool->mon_desc_pool);
	rx_desc_pool->mon_desc_pool = NULL;
}

static
void ath12k_wifi6_dp_mon_rx_buf_ring_buf_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u8 *vaddr;
	dma_addr_t paddr;
	int i;

	rx_desc_pool = &dp_wifi6->rx_desc_mon;

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
		page_frag_free(vaddr);
		rx_desc_pool->mon_desc_pool[i].vaddr = NULL;
		rx_desc_pool->mon_desc_pool[i].in_use = 0;
	}

	spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
}

static
void ath12k_wifi6_dp_mon_rx_buf_ring_pool_free(struct ath12k_dp *dp)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;

	rx_desc_pool = &dp_wifi6->rx_desc_mon;
	kfree(rx_desc_pool->mon_desc_pool);
	rx_desc_pool->mon_desc_pool = NULL;
}

void ath12k_wifi6_dp_mon_rx_ring_deinit(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	int i;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++)
		ath12k_wifi6_dp_mon_rx_status_buf_free(dp, i);

	if (ath12k_dp_mon_rxdma1_enable(dp))
		ath12k_wifi6_dp_mon_rx_buf_ring_buf_free(dp);
}

int ath12k_wifi6_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;
	u32 ring_id;
	int i, ret;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++) {
		ring_id = dp_mon->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, i, HAL_RXDMA_MONITOR_STATUS);
		if (ret) {
			ath12k_warn(ab, "failed to configure mon_status_refill_ring%d %d\n", i, ret);
			return ret;
		}
	}

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		ring_id = dp_mon->rxdma_mon_buf_ring.refill_buf_ring.ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, 0, HAL_RXDMA_MONITOR_BUF);
		if (ret) {
			ath12k_warn(ab, "failed to configure rxdma_mon_buf_ring %d\n", ret);
			return ret;
		}

		ring_id = dp_mon->rxdma_mon_desc_ring.ring_id;
		ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, 0, HAL_RXDMA_MONITOR_DESC);
		if (ret) {
			ath12k_warn(ab, "failed to configure mon_desc_ring %d\n", ret);
			return ret;
		}
	}
	return 0;
}

int ath12k_wifi6_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	int srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, mac_id);
	struct dp_srng *ring = &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[srng_id];
	int ret;

	if (!ath12k_dp_mon_rxdma1_enable(dp))
		return 0;

	ret = ath12k_dp_srng_alloc(ab, ring,
				    HAL_RXDMA_MONITOR_DST,
				    0, srng_id,
				    HAL_RX_MON_DST_RING_SIZE);
	if (ret) {
		ath12k_warn(dp, "failed to alloc mon dest ring %d\n", srng_id);
		return ret;
	}

	ret = ath12k_dp_srng_init(ab, ring,
				   HAL_RXDMA_MONITOR_DST,
				   0, srng_id);
	if (ret) {
		ath12k_warn(dp, "failed to init mon dest ring %d\n", srng_id);
		return ret;
	}

        return 0;
}

void ath12k_wifi6_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	int i;

	if (!ath12k_dp_mon_rxdma1_enable(dp))
		return;

	for (i = 0; i < ab->hw_params->num_rxdma_per_pdev; i++)
		ath12k_dp_srng_cleanup(ab, &dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[i]);
}

int ath12k_wifi6_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	int srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, mac_id);
	u32 ring_id = dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[srng_id].ring_id;
	int ret;

	if (!ath12k_dp_mon_rxdma1_enable(dp))
		return 0;

	ret = ath12k_dp_tx_htt_srng_setup(ab, ring_id, srng_id, HAL_RXDMA_MONITOR_DST);
	if (ret) {
		ath12k_warn(ab, "failed to configure mon_dest_ring%d %d\n", srng_id, ret);
		return ret;
	}

	return 0;
}

int ath12k_wifi6_mon_setup_mon_link_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct hal_srng *mon_desc_srng;
	u32 entry_sz, n_link_desc;
	int ret;

	if (!ath12k_dp_mon_rxdma1_enable(dp))
		return 0;

	mon_desc_srng = &ab->hal.srng_list[dp_mon_pdev->dp_mon->rxdma_mon_desc_ring.ring_id];
	entry_sz = ath12k_hal_srng_get_entrysize(ab, HAL_RXDMA_MONITOR_DESC);

	n_link_desc = dp_mon_pdev->dp_mon->rxdma_mon_desc_ring.size / entry_sz;

	ret = ath12k_dp_link_desc_setup(ab, dp_mon_pdev->mon_data.link_desc_banks,
					HAL_RXDMA_MONITOR_DESC, mon_desc_srng,
					n_link_desc);
	if (ret) {
		ath12k_warn(ab, "mon_link_desc_pool_setup() failed\n");
		return ret;
	}
	return 0;
}

void ath12k_wifi6_mon_cleanup_mon_link_desc(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;

	if (!ath12k_dp_mon_rxdma1_enable(dp))
		return;

	ath12k_dp_link_desc_cleanup(ab, dp_mon_pdev->mon_data.link_desc_banks,
				    HAL_RXDMA_MONITOR_DESC,
				    &dp_mon_pdev->dp_mon->rxdma_mon_desc_ring);
}

struct sk_buff *
ath12k_wifi6_dp_mon_rx_alloc_status_buf(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *skb = NULL;
	dma_addr_t paddr;

	skb = dev_alloc_skb(RX_MON_STATUS_BUF_SIZE);
	if (!skb)
		goto fail_alloc_skb;

	if (!IS_ALIGNED((unsigned long)skb->data, RX_MON_STATUS_BUF_ALIGN))
		skb_pull(skb, PTR_ALIGN(skb->data, RX_MON_STATUS_BUF_ALIGN) - skb->data);

	memset(skb->data, 0, RX_MON_STATUS_BUF_SIZE);

	paddr = dma_map_single(ab->dev, skb->data,
			       RX_MON_STATUS_BUF_SIZE,
			       DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(ab->dev, paddr)))
		goto fail_free_skb;

	ATH12K_SKB_RXCB(skb)->paddr = paddr;
	return skb;

fail_free_skb:
	dev_kfree_skb_any(skb);
fail_alloc_skb:
	return NULL;
}

static enum dp_mon_status_buf_state
ath12k_wifi6_dp_mon_rx_buf_done(struct ath12k_base *ab, struct hal_srng *srng,
				struct ath12k_mon_data *pmon,
				struct dp_rxdma_mon_ring *rx_ring)
{
	struct hal_tlv_hdr *tlv;
	struct sk_buff *skb;
	struct ath12k_buffer_addr *status_desc;
	struct ath12k_dp_wifi6_rx_desc_info *rx_desc;
	struct ath12k_dp *dp = ab->dp;
	dma_addr_t paddr;
	u32 cookie;
	u8 rbm;

	status_desc = ath12k_wifi6_hal_srng_src_next_next_peek(ab, srng);
	if (!status_desc)
		return DP_MON_STATUS_NO_DMA;

	ath12k_wifi6_hal_rx_buf_addr_info_get(status_desc, &paddr, &cookie, &rbm);

	rx_desc = ath12k_wifi6_dp_get_rx_status_desc(dp , cookie);

	if (!rx_desc || !rx_desc->skb)
		return DP_MON_STATUS_NO_DMA;

	skb = rx_desc->skb;

	dma_sync_single_for_cpu(ab->dev, rx_desc->paddr,
				skb->len + skb_tailroom(skb),
				DMA_FROM_DEVICE);

	tlv = (struct hal_tlv_hdr *)skb->data;
	if (le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG) != HAL_RX_STATUS_BUFFER_DONE) {
		pmon->rx_mon_stats.status_tlv_tag_err++;
		return DP_MON_STATUS_REPLINISH;
	}
	pmon->rx_mon_stats.status_buf_done_war++;
	return DP_MON_STATUS_REPLINISH;
}

static int ath12k_wifi6_dp_mon_rx_reap_status_ring(struct ath12k_pdev_dp *pdev_dp,
						   int mac_id,
						   int *budget,
						   struct sk_buff_head *skb_list)
{
	int srng_id, num_buffs_reaped = 0;
	enum dp_mon_status_buf_state reap_status;
	struct dp_rxdma_mon_ring *rx_ring;
	struct ath12k_mon_data *pmon;
	struct ath12k_skb_rxcb *rxcb;
	struct hal_tlv_hdr *tlv;
	struct ath12k_buffer_addr *rx_mon_status_desc;
	struct ath12k_dp_wifi6_rx_desc_info *rx_desc = NULL;
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	struct hal_srng *srng;
	struct ath12k_dp *dp;
	struct ath12k_base *ab;
	struct ath12k_dp_mon *dp_mon;
	struct sk_buff *skb;
	dma_addr_t paddr;
	u32 cookie, tag;
	u8 rbm;

	dp = pdev_dp->dp;
	ab = dp->ab;
	dp_mon = dp->dp_mon;
	pmon = &pdev_dp->dp_mon_pdev->mon_data;
	srng_id = ath12k_hw_mac_id_to_srng_id(ab->hw_params, mac_id);
	rx_ring = &dp_mon->rx_mon_status_refill_ring[srng_id];

	srng = &ab->hal.srng_list[rx_ring->refill_buf_ring.ring_id];
	rx_desc_pool = ath12k_wifi6_dp_get_rx_desc_pool(dp, mac_id);

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	while (likely((rx_mon_status_desc =
	       ath12k_hal_srng_src_next_peek(ab, srng)) && (*budget)--)) {

		ath12k_wifi6_hal_rx_buf_addr_info_get(rx_mon_status_desc, &paddr,
						      &cookie, &rbm);
		rx_desc = NULL;

		if (paddr) {
			rx_desc = ath12k_wifi6_dp_get_rx_status_desc(dp, cookie);
			if (!rx_desc || !rx_desc->skb) {
				pmon->rx_mon_stats.status_desc_invalid++;
				ath12k_warn(ab,
					    "rx monitor status with invalid cookie %u\n",
					    cookie);
				goto move_next;
			}

			if (rx_desc->paddr != paddr) {
				pmon->rx_mon_stats.rx_err_desc_sanity_fail++;
				ath12k_hal_srng_src_get_next_entry(ab, srng);
				continue;
			}

			skb = rx_desc->skb;
			dma_sync_single_for_cpu(ab->dev, rx_desc->paddr,
						skb->len + skb_tailroom(skb),
						DMA_FROM_DEVICE);

			tlv = (struct hal_tlv_hdr *)skb->data;
			tag = le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG);
			if (tag != HAL_RX_STATUS_BUFFER_DONE) {
				 ath12k_dbg(ab, ATH12K_DBG_DP_MON,
					    "mon status DONE not set tag 0x%x cookie %u\n",
					    tag, cookie);

				/*
				 * RxDMA status done bit might not be set even
				 * though tp is moved by HW.
				 *
				 * If done status is missing:
				 * 1. As per MAC team's suggestion,
				 *    when HP + 1 entry is peeked and if DMA
				 *    is not done and if HP + 2 entry's DMA done
				 *    is set, skip HP + 1 entry and start
				 *    processing in next interrupt.
				 * 2. If HP + 2 entry's DMA done is not set,
				 *    poll onto HP + 1 entry DMA done to be set.
				 *    Check status for same buffer next time.
				 */
				reap_status =
					ath12k_wifi6_dp_mon_rx_buf_done(ab, srng,
									pmon, rx_ring);
				if (reap_status == DP_MON_STATUS_NO_DMA)
					continue;
				else if (reap_status == DP_MON_STATUS_REPLINISH) {
					if (!rx_desc->unmapped) {
						dma_unmap_single(ab->dev, rx_desc->paddr,
								 rx_desc_pool->buf_size,
								 DMA_FROM_DEVICE);
						rx_desc->unmapped = 1;
					}
				}
				dev_kfree_skb_any(skb);
				rx_desc->skb = NULL;
				rx_desc->in_use = 0;
				goto move_next;
			}

			if (ath12k_dp_mon_rx_set_pktlen(skb, RX_MON_STATUS_BUF_SIZE)) {
				if (!rx_desc->unmapped) {
					dma_unmap_single(ab->dev, rx_desc->paddr,
							rx_desc_pool->buf_size,
							DMA_FROM_DEVICE);
					rx_desc->unmapped = 1;
				}
				dev_kfree_skb_any(skb);
				rx_desc->skb = NULL;
				rx_desc->in_use = 0;
				goto move_next;
			}

			if (!rx_desc->unmapped) {
				dma_unmap_single(ab->dev, rx_desc->paddr,
						 rx_desc_pool->buf_size,
						 DMA_FROM_DEVICE);
				rx_desc->unmapped = 1;
			}

			__skb_queue_tail(skb_list, skb);
			rx_desc->skb = NULL;
			rx_desc->in_use = 0;
		} else {
			/* fetch rx_desc from the rx_desc_free_list */
			spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
			rx_desc = list_first_entry_or_null(&rx_desc_pool->rx_desc_free_list,
							   struct ath12k_dp_wifi6_rx_desc_info,
							   list);
			if (rx_desc)
				list_del(&rx_desc->list);
			spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
		}

move_next:
		if (!rx_desc) {
			ath12k_warn(ab, "no free rx_desc available for mon status\n");
			ath12k_wifi6_hal_rx_buf_addr_info_set(rx_mon_status_desc,
							      0, 0, rx_desc_pool->owner);
			num_buffs_reaped++;
			break;
		}

		skb = ath12k_wifi6_dp_mon_rx_alloc_status_buf(dp);
		if (!skb) {
			if (rx_desc_pool) {
				spin_lock_bh(&rx_desc_pool->rx_mon_desc_lock);
				list_add_tail(&rx_desc->list,
					      &rx_desc_pool->rx_desc_free_list);
				spin_unlock_bh(&rx_desc_pool->rx_mon_desc_lock);
			}

			ath12k_wifi6_hal_rx_buf_addr_info_set(rx_mon_status_desc,
							      0, 0, rx_desc_pool->owner);
			num_buffs_reaped++;
			break;
		}

		rxcb = ATH12K_SKB_RXCB(skb);
		rx_desc->skb = skb;
		rx_desc->paddr = rxcb->paddr;
		rx_desc->unmapped = 0;
		rx_desc->in_use = 1;
		cookie = rx_desc->cookie;

		ath12k_wifi6_hal_rx_buf_addr_info_set(rx_mon_status_desc,
						      rx_desc->paddr,
						      cookie,
						      rx_desc_pool->owner);

		ath12k_hal_srng_src_get_next_entry(ab, srng);
		num_buffs_reaped++;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return num_buffs_reaped;
}

static void
ath12k_wifi6_dp_mon_rx_add_desc_used_list(struct ath12k_dp_wifi6_rx_desc_info *rx_desc,
					  struct list_head *used_list)
{
	rx_desc->vaddr =  NULL;
	rx_desc->skb = NULL;
	rx_desc->in_use = 0;
	list_add_tail(&rx_desc->list, used_list);
}

static enum hal_rx_mon_status
ath12k_wifi6_dp_mon_rx_parse_dest(struct ath12k_pdev_dp *dp_pdev,
				  struct sk_buff *skb)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct hal_tlv_hdr *tlv;
	struct hal_tlv_parsed_hdr tlv_parsed_hdr = {0};
	enum hal_rx_mon_status hal_status;
	u16 tlv_tag, tlv_len, tlv_userid;
	u8 *ptr = skb->data;

	do {
		tlv = (struct hal_tlv_hdr *)ptr;
		tlv_tag = le32_get_bits(tlv->tl, HAL_TLV_HDR_TAG);
		tlv_len = le32_get_bits(tlv->tl, HAL_TLV_HDR_LEN);
		tlv_userid = le32_get_bits(tlv->tl, HAL_TLV_USR_ID);
		ptr += sizeof(*tlv);

		/* The actual length of PPDU_END is the combined length of many PHY
		 * TLVs that follow. Skip the TLV header and
		 * rx_rxpcu_classification_overview that follows the header to get to
		 * next TLV.
		 */

		if (tlv_tag == HAL_RX_PPDU_END)
			tlv_len = sizeof(struct hal_rx_rxpcu_classification_overview);

		tlv_parsed_hdr.tag = tlv_tag;
		tlv_parsed_hdr.len = tlv_len;
		tlv_parsed_hdr.userid = tlv_userid;
		tlv_parsed_hdr.data = ptr;

		hal_status =
			ath12k_wifi6_hal_mon_rx_parse_status_tlv(dp_pdev->dp->hal,
								 &pmon->mon_ppdu_info,
								 &tlv_parsed_hdr);
		ptr += tlv_len;
		ptr = PTR_ALIGN(ptr, HAL_TLV_ALIGN);

		if (((ptr - skb->data) >= RX_MON_STATUS_BUF_SIZE) ||
		    (RX_MON_STATUS_BUF_SIZE - (ptr - skb->data) < HAL_TLV_ALIGN))
			break;

	} while ((hal_status == HAL_RX_MON_STATUS_PPDU_NOT_DONE) ||
		 (hal_status == HAL_RX_MON_STATUS_RX_HDR) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_START) ||
		 (hal_status == HAL_RX_MON_STATUS_MPDU_END) ||
		 (hal_status == HAL_RX_MON_STATUS_MSDU_END));

	return hal_status;
}

static void
ath12k_wifi6_dp_mon_rx_remove_raw_frame_fcs(struct ath12k_pdev_dp *dp_pdev,
					    struct sk_buff **head_msdu,
					    struct sk_buff **tail_msdu,
					    u16 rx_mon_pkt_tlv_size)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	void *addr;

	if (unlikely(!head_msdu || !tail_msdu || !(*head_msdu)))
		return;

	/* Strip FCS_LEN for Raw frame */
	addr = 	ath12k_dp_mon_skb_get_frag_addr(*head_msdu, 0);
	addr -= rx_mon_pkt_tlv_size; /* need to check this */

	if (ath12k_wifi6_dp_mon_rx_decap_format_get(addr) ==
	    DP_RX_DECAP_TYPE_RAW) {
		uint8_t fcs_len_left = HAL_RX_MON_FCS_LEN;

		if (skb_shinfo(*tail_msdu)->nr_frags >= 2) {
			uint8_t last_f = skb_shinfo(*tail_msdu)->nr_frags - 1;
			uint8_t last_frag_size =
				ath12k_dp_mon_get_frag_size_by_idx(dp, *tail_msdu,
								   last_f);
			if (last_frag_size <= HAL_RX_MON_FCS_LEN) {
				ath12k_dp_mon_skb_remove_frag(dp, *tail_msdu, last_f,
							      ATH12K_DP_MON_RX_BUF_SIZE);
				fcs_len_left -= last_frag_size;
			}
		}

		skb_coalesce_rx_frag(*tail_msdu, (skb_shinfo(*tail_msdu)->nr_frags - 1),
				     -fcs_len_left, 0);
	}
}

static u32 ath12k_wifi6_dp_mon_rx_comp_ppduid(u32 msdu_ppdu_id, u32 *ppdu_id)
{
	u32 ret = 0;

	if ((*ppdu_id < msdu_ppdu_id) &&
	    ((msdu_ppdu_id - *ppdu_id) < DP_NOT_PPDU_ID_WRAP_AROUND)) {
		/* Hold on mon dest ring, and reap mon status ring. */
		*ppdu_id = msdu_ppdu_id;
		ret = msdu_ppdu_id;
	} else if ((*ppdu_id > msdu_ppdu_id) &&
		((*ppdu_id - msdu_ppdu_id) > DP_NOT_PPDU_ID_WRAP_AROUND)) {
		/* PPDU ID has exceeded the maximum value and will
		 * restart from 0.
		 */
		*ppdu_id = msdu_ppdu_id;
		ret = msdu_ppdu_id;
	}
	return ret;
}

static void
ath12k_wifi6_dp_mon_rx_adjust_frag_len(u32 *total_len, u32 *frag_len,
				       u32 max_limit)
{
	if (*total_len >= max_limit) {
		*frag_len = max_limit;
		*total_len -= *frag_len;
	} else {
		*frag_len = *total_len;
		*total_len = 0;
	}
}

static void
ath12k_wifi6_dp_mon_rx_parse_desc(struct ath12k_pdev_dp *dp_pdev,
				  struct hal_rx_msdu_desc_info *msdu_info,
				  bool *is_frag, u32 *total_frag_len,
				  u32 *frag_len, void *rx_desc_tlv,
				  bool *is_frag_non_raw)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct hal_mon_rx_msdu_info frame_info;
	u16 tot_payload_len = ATH12K_DP_MON_RX_BUF_SIZE - sizeof(struct hal_rx_desc);

	if (msdu_info->msdu_flags & RX_MSDU_DESC_INFO0_MSDU_CONTINUATION) {
		/* First buffer of MSDU */
		if (!(*is_frag)) {
			/* Set total frag_len from msdu_len */
			*total_frag_len = msdu_info->msdu_len;

			*is_frag = true;
			if (DP_RX_DECAP_TYPE_RAW ==
			    ath12k_wifi6_dp_mon_rx_decap_format_get(rx_desc_tlv)) {
				frame_info.is_decap_raw = 1;
			} else {
				ath12k_warn(dp, "Non-RAW decap format detected\n");
				*is_frag_non_raw = true;
			}
			ath12k_wifi6_dp_mon_rx_adjust_frag_len(total_frag_len, frag_len,
							       tot_payload_len);
		} else {
			/* Continuation Middle frame */
			ath12k_wifi6_dp_mon_rx_adjust_frag_len(total_frag_len, frag_len,
							       tot_payload_len);
			if (*is_frag_non_raw)
				frame_info.is_decap_raw = 0;
			else
				frame_info.is_decap_raw = 1;
		}
		ath12k_wifi6_dp_mon_rx_priv_info_set(rx_desc_tlv,
						      (u8 *)&frame_info,
						      sizeof(frame_info));
	} else {
		/* Last buffer of MSDU spread among multiple buffer */
		if (*is_frag) {
			ath12k_wifi6_dp_mon_rx_adjust_frag_len(total_frag_len, frag_len,
							       tot_payload_len);
			if (*is_frag_non_raw)
				frame_info.is_decap_raw = 0;
			else
				frame_info.is_decap_raw = 1;
		} else {
			/* MSDU with single buffer */
			*frag_len = msdu_info->msdu_len;
			if (DP_RX_DECAP_TYPE_RAW ==
			    ath12k_wifi6_dp_mon_rx_decap_format_get(rx_desc_tlv)) {
				frame_info.is_decap_raw = 1;
			} else {
				ath12k_warn(dp, "Non-RAW decap format detected\n");
				*is_frag_non_raw = true;
			}
		}
		ath12k_wifi6_dp_mon_rx_priv_info_set(rx_desc_tlv,
						      (u8 *)&frame_info,
						      sizeof(frame_info));
		/* Reset bool after complete processing of MSDU */
		*is_frag = false;
		*is_frag_non_raw = false;
	}
}

static int
ath12k_wifi6_dp_mon_rx_add_msdu_to_list(struct ath12k_dp *dp,
					struct sk_buff **head_msdu,
					struct sk_buff **last,
					void *rx_desc_tlv, u32 frag_len,
					u16 rx_mon_pkt_tlv_size)
{
	struct ath12k_base *ab = dp->ab;
	u32 num_frags;
	struct sk_buff *msdu_curr;

	/* Here head_msdu and *head_msdu must not be NULL */
	/* Dont add frag to skb if frag length is zero. Drop frame */
	if (unlikely(!frag_len || !head_msdu || !(*head_msdu))) {
		ath12k_err(ab, "[%s] frag_len[%d] || head_msdu[%pK] || "
			   "*head_msdu is Null while adding frag to skb\n",
			   __func__, frag_len, head_msdu);
		return -EINVAL;
	}

	/* In case of first desc of MPDU, assign curr msdu to *head_msdu */
	if (!skb_shinfo(*head_msdu)->nr_frags)
		msdu_curr = *head_msdu;
	else
		msdu_curr = *last;

	/* Current msdu must not be NULL */
	if (unlikely(!msdu_curr)) {
		ath12k_err(ab, "[%s] Current msdu can't be Null while"
			   "adding frag to skb\n", __func__);
		return -EINVAL;
	}

	num_frags = skb_shinfo(msdu_curr)->nr_frags;
	if (num_frags < MAX_SKB_FRAGS) {
		ath12k_dp_mon_add_rx_frag(msdu_curr, rx_desc_tlv, rx_mon_pkt_tlv_size,
					  frag_len, false);
		if (*last != msdu_curr)
			*last = msdu_curr;
		return 0;
	}

	/* Execution will reach here only if num_frags == MAX_SKB_FRAGS */
	msdu_curr = NULL;
	if (ath12k_wifi6_dp_rx_mon_alloc_parent_buf(&msdu_curr)) {
		ath12k_err(ab, "failed to allocate subsequent parent buffer to hold all frag\n");
		return -EINVAL;
	}

	ath12k_dp_mon_add_rx_frag(msdu_curr, rx_desc_tlv, rx_mon_pkt_tlv_size,
				  frag_len, false);

	/* Add allocated nbuf in the chain */
	(*last)->next = msdu_curr;

	/* Assign current msdu to last to avoid traversal */
	*last = msdu_curr;

	return 0;
}

static void
ath12k_wifi6_dp_mon_rx_free_msdu_list(struct ath12k_dp *dp,
				      void *rx_desc_tlv,
				      struct sk_buff **head_msdu,
				      struct sk_buff **last, struct sk_buff **tail_msdu)
{
	struct sk_buff *next;

	page_frag_free(rx_desc_tlv);

	if (head_msdu) {
		while (*head_msdu) {
			next = (*head_msdu)->next;
			dev_kfree_skb_any(*head_msdu);
			*head_msdu = next;
		}
	}

	if (head_msdu)
		*head_msdu = NULL;
	if (last)
		*last = NULL;
	if (tail_msdu)
		*tail_msdu = NULL;
}

static bool
ath12k_wifi6_dp_mon_rxdesc_mpdu_valid(struct ath12k_dp *dp,
				      struct hal_rx_desc *rx_desc)
{
	struct ath12k_base *ab = dp->ab;
	u32 tlv_tag;

	tlv_tag = hal_rx_desc_get_mpdu_start_tag(&ab->hal, rx_desc);

	return tlv_tag == HAL_RX_MPDU_START;
}

static int
ath12k_wifi6_dp_mon_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					uint8_t mac_id)
{
	struct ath12k_base *ab = dp->ab;
	u32 ring_id;
	struct hal_srng *srng;
	void *src_srng_desc;
	int ret = 0;

	ring_id = dp->dp_mon->rxdma_mon_desc_ring.ring_id;
	srng = &ab->hal.srng_list[ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	src_srng_desc = ath12k_hal_srng_src_get_next_entry(ab, srng);

	if (src_srng_desc) {
		struct ath12k_buffer_addr *src_desc = src_srng_desc;
		*src_desc = *((struct ath12k_buffer_addr *)buf_addr_info);
	} else {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON,
			   "Monitor Link Desc Ring mac_id[%d] Full",
			   mac_id);
		ret = -ENOMEM;
	}

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return ret;
}

static int
ath12k_wifi6_dp_monitor_rx_link_desc_return(struct ath12k_dp *dp,
					    struct ath12k_buffer_addr *buf_addr_info,
					    uint8_t mac_id,
					    enum hal_wbm_rel_bm_act action)
{
	if (ath12k_dp_mon_rxdma1_enable(dp))
		return ath12k_wifi6_dp_mon_rx_link_desc_return(dp, buf_addr_info,
							       mac_id);
	else
		return ath12k_wifi6_dp_rx_link_desc_return(dp, buf_addr_info, action);
}

static void
ath12k_dp_mon_rx_init_tail_msdu(struct sk_buff **head_msdu, struct sk_buff *last,
				struct sk_buff **tail_msdu)
{
	if (!head_msdu || !(*head_msdu)) {
		*tail_msdu = NULL;
		return;
	}

	if (last)
		last->next = NULL;

	*tail_msdu = last;
}

static u32
ath12k_wifi6_dp_mon_rx_mpdu_pop(struct ath12k_pdev_dp *dp_pdev, int mac_id,
				void *ring_entry, struct sk_buff **head_msdu,
				struct sk_buff **tail_msdu,
				struct list_head *used_list,
				u32 *npackets, u32 *ppdu_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct ath12k_buffer_addr rx_link_buf_info;
	struct hal_rx_buf_info buf_info;
	u32 msdu_ppdu_id = 0, msdu_cnt = 0, total_len = 0, frag_len = 0;
	bool is_frag, is_first_msdu, drop_mpdu = false, is_frag_non_raw = false;
	struct hal_reo_entrance_ring *ent_desc =
		(struct hal_reo_entrance_ring *)ring_entry;
	u32 rx_bufs_used = 0, i = 0;
	void *msdu_link_desc;
	struct sk_buff *last = NULL;
	struct hal_rx_msdu_list msdu_list;
	void *rx_desc_tlv;
	u16 num_msdus = 0;
	dma_addr_t buf_paddr;
	u8 *data = NULL;
	u16 rx_mon_pkt_tlv_size = ab->hal.hal_desc_sz;

	ath12k_wifi6_hal_rx_reo_ent_buf_paddr_get(ring_entry,
						  &buf_info,
						  &msdu_cnt);
	spin_lock_bh(&pmon->mon_lock);

	if (le32_get_bits(ent_desc->info1,
			  HAL_REO_ENTR_RING_INFO1_RXDMA_PUSH_REASON) ==
			  HAL_REO_DEST_RING_PUSH_REASON_ERR_DETECTED) {
		u8 rxdma_err = le32_get_bits(ent_desc->info1,
					     HAL_REO_ENTR_RING_INFO1_RXDMA_ERROR_CODE);
		if (rxdma_err == HAL_REO_ENTR_RING_RXDMA_ECODE_FLUSH_REQUEST_ERR ||
		    rxdma_err == HAL_REO_ENTR_RING_RXDMA_ECODE_MPDU_LEN_ERR ||
		    rxdma_err == HAL_REO_ENTR_RING_RXDMA_ECODE_OVERFLOW_ERR) {
			drop_mpdu = true;
			pmon->rx_mon_stats.dest_mpdu_drop++;
		}
	}

	is_frag = false;
	is_first_msdu = true;

	do {
		if (!msdu_cnt) {
			drop_mpdu = true;
			pmon->rx_mon_stats.invalid_msdu_cnt++;
		}

		/* WAR for duplicate link descriptors received from HW */
		if (pmon->mon_last_linkdesc_paddr == buf_info.paddr) {
			pmon->rx_mon_stats.dup_mon_linkdesc_cnt++;
			spin_unlock_bh(&pmon->mon_lock);
			return rx_bufs_used;
		}

		msdu_link_desc = ath12k_wifi6_dp_rx_cookie_to_mon_link_desc(dp_pdev,
									    &buf_info,
									    pmon);
		/* Need to handle !msdu_link_desc */

		ath12k_hal_rx_msdu_list_get(&ab->hal, msdu_link_desc, &msdu_list,
					    &num_msdus);

		for (i = 0; i < num_msdus; i++) {
			struct ath12k_dp_wifi6_rx_desc_info *rx_desc;
			struct ath12k_dp_rx_desc_pool *rx_desc_pool;

			rx_desc = ath12k_wifi6_dp_get_rx_desc(dp,
							      msdu_list.sw_cookie[i]);
			if (!rx_desc) {
				drop_mpdu = true;
				pmon->mon_last_linkdesc_paddr = buf_info.paddr;
				pmon->rx_mon_stats.empty_mon_sw_desc_cnt++;
				continue;
			}

			buf_paddr = rx_desc->paddr;
			if (pmon->mon_last_buf_cookie == msdu_list.sw_cookie[i] ||
			    !(rx_desc->vaddr) || msdu_list.paddr[i] != buf_paddr ||
			    !rx_desc->in_use) {
				ath12k_dbg(ab, ATH12K_DBG_DP_MON,
					   "i %d last_cookie %d is same\n",
					   i, pmon->mon_last_buf_cookie);
				drop_mpdu = true;
				pmon->rx_mon_stats.dup_mon_buf_cnt++;
				pmon->mon_last_linkdesc_paddr = buf_info.paddr;
				continue;
			}

			if (rx_desc->unmapped == 0) {
				rx_desc_pool = ath12k_wifi6_dp_rx_get_mon_desc_pool(dp);
				ath12k_core_dma_unmap_page(ab->dev, rx_desc->paddr,
							   rx_desc_pool->buf_size,
							   DMA_FROM_DEVICE);
				rx_desc->unmapped = 1;
			}

			if (drop_mpdu) {
				pmon->mon_last_linkdesc_paddr = buf_info.paddr;
				page_frag_free(rx_desc->vaddr);
				goto next_msdu;
			}

			data = rx_desc->vaddr;
			rx_desc_tlv = data;

			ath12k_dbg(ab, ATH12K_DBG_DP_MON, "i=%d ppdu_id=%x"
				   " num_msdus = %u\n", i, *ppdu_id, num_msdus);

			if (is_first_msdu) {
				if (!ath12k_wifi6_dp_mon_rxdesc_mpdu_valid(dp,
									   rx_desc_tlv)) {
					drop_mpdu = true;
					page_frag_free(rx_desc->vaddr);
					pmon->mon_last_linkdesc_paddr = buf_info.paddr;
					goto next_msdu;
				}

				msdu_ppdu_id =
					ath12k_wifi6_hal_rx_desc_get_mpdu_ppdu_id(ring_entry);
				is_first_msdu = false;

				ath12k_dbg(ab, ATH12K_DBG_DP_MON, "msdu_ppdu_id=%x\n",
					   msdu_ppdu_id);

				if (ath12k_wifi6_dp_mon_rx_comp_ppduid(msdu_ppdu_id,
								       ppdu_id)) {
					spin_unlock_bh(&pmon->mon_lock);
					return rx_bufs_used;
				}

				if (*ppdu_id == msdu_ppdu_id)
					pmon->rx_mon_stats.ppdu_id_match++;
				else
					pmon->rx_mon_stats.ppdu_id_mismatch++;

				pmon->mon_last_linkdesc_paddr = buf_info.paddr;

				if (ath12k_wifi6_dp_rx_mon_alloc_parent_buf(head_msdu)) {
					page_frag_free(rx_desc->vaddr);
					ath12k_warn(dp, "Failed to allocate parent buffer to hold all the frags\n");
					drop_mpdu = true;
					goto next_msdu;
				}
			}

			ath12k_wifi6_dp_mon_rx_parse_desc(dp_pdev,
							  &msdu_list.msdu_info[i],
							  &is_frag, &total_len,
							  &frag_len,
							  rx_desc_tlv,
							  &is_frag_non_raw);

			if (!is_frag && msdu_cnt)
				msdu_cnt--;

			ath12k_dbg(ab, ATH12K_DBG_DP_MON, "total_len %u frag_len %u"
				   " flags %u\n", total_len, frag_len,
				   msdu_list.msdu_info[i].msdu_flags);

			if (ath12k_wifi6_dp_mon_rx_add_msdu_to_list(dp, head_msdu,
								    &last, rx_desc_tlv,
								    frag_len,
								    rx_mon_pkt_tlv_size)) {
				ath12k_wifi6_dp_mon_rx_free_msdu_list(dp, rx_desc_tlv,
								      head_msdu,
								      &last,
								      tail_msdu);
				drop_mpdu = true;
				goto next_msdu;
			}

next_msdu:
			pmon->mon_last_buf_cookie = msdu_list.sw_cookie[i];
			rx_bufs_used++;
			ath12k_wifi6_dp_mon_rx_add_desc_used_list(rx_desc, used_list);
		}

		/*
		 * Store the current link buffer into to the local
		 * structure to be  used for release purpose.
		 */
		ath12k_wifi6_hal_rx_buf_addr_info_set(&rx_link_buf_info,
						      buf_info.paddr,
						      buf_info.sw_cookie,
						      buf_info.rbm);

		ath12k_wifi6_dp_mon_rx_next_link_desc_get(msdu_link_desc,
							  &buf_info);
		if (ath12k_wifi6_dp_monitor_rx_link_desc_return(dp, &rx_link_buf_info,
								mac_id,
								HAL_WBM_REL_BM_ACT_PUT_IN_IDLE))
			ath12k_dbg(ab, ATH12K_DBG_DP_MON,
				   "monitor link desc return failed\n");
	} while (buf_info.paddr);

	ath12k_dp_mon_rx_init_tail_msdu(head_msdu, last, tail_msdu);
	ath12k_wifi6_dp_mon_rx_remove_raw_frame_fcs(dp_pdev, head_msdu, tail_msdu,
						    rx_mon_pkt_tlv_size);

	spin_unlock_bh(&pmon->mon_lock);

	return rx_bufs_used;
}

static void
ath12k_wifi6_dp_mon_rx_fraglist_prepare(struct sk_buff *head_msdu,
					struct sk_buff *tail_msdu)
{
	struct sk_buff *msdu, *mpdu_buf, *head_frag_list;
	u32 frag_list_sum_len;

	/* Single skb accommodating MPDU worth Data */
	if (tail_msdu == head_msdu)
		return;

	mpdu_buf = head_msdu;
	frag_list_sum_len = 0;

	msdu = head_msdu->next;
	/* msdu can't be NULL here as it is multiple skb case here */

	/* Head frag list to point to second skb */
	head_frag_list = msdu;

	while (msdu) {
		frag_list_sum_len += msdu->len;
		msdu = msdu->next;
	}

	/* Append second skb to the frag list of head skb */
	skb_shinfo(mpdu_buf)->frag_list = head_frag_list;
	mpdu_buf->data_len += frag_list_sum_len;
	mpdu_buf->len += frag_list_sum_len;

	/* Make Parent skb next to NULL */
	mpdu_buf->next = NULL;
}

static struct sk_buff *
ath12k_wifi6_dp_mon_rx_mpdu_restitch(struct ath12k_pdev_dp *dp_pdev,
				     struct sk_buff *head_msdu,
				     struct sk_buff *tail_msdu,
				     struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct hal_mon_rx_msdu_info frame_info;
	void *rx_desc;
	u16 rx_mon_pkt_tlv_size = ab->hal.hal_desc_sz;
	u16 num_frags;

	if (!head_msdu || !tail_msdu)
		goto mpdu_restitch_fail;

	rx_desc =  ath12k_dp_mon_skb_get_frag_addr(head_msdu, 0);
	rx_desc -= rx_mon_pkt_tlv_size;

	/* Need to handle msdu len error */
	if (ath12k_wifi6_dp_rx_desc_mpdu_len_err_get(rx_desc)) {
		/* need to stat counter for mon_rx_drop */
		return NULL;
	}

	memset(&frame_info, 0, sizeof(struct hal_mon_rx_msdu_info));
	ath12k_wifi6_dp_mon_rx_priv_info_get(rx_desc,(u8 *)&frame_info,
					     sizeof(struct hal_mon_rx_msdu_info));

	/* Look for FCS error*/
	num_frags = skb_shinfo(tail_msdu)->nr_frags;

	rx_desc = ath12k_dp_mon_skb_get_frag_addr(tail_msdu, num_frags - 1);
	rx_desc -= rx_mon_pkt_tlv_size;

	ppdu_info->rs_fcs_err = ath12k_wifi6_dp_rx_desc_mpdu_fcs_err_get(rx_desc);

	if (frame_info.is_decap_raw == 1)
		ath12k_wifi6_dp_mon_rx_fraglist_prepare(head_msdu, tail_msdu);

	return head_msdu;

mpdu_restitch_fail:
	ath12k_dbg(ab, ATH12K_DBG_DP_MON,
		   "%pK: mpdu_stitch_fail head_msdu %pK", dp_pdev, head_msdu);
	return NULL;
}

static
int ath12k_wifi6_dp_mon_rx_update_band_and_get_freq(struct ath12k_pdev_dp *dp_pdev,
						    u16 channel_num,
						    struct ieee80211_rx_status *rxs)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct ath12k *ar =  dp_pdev->ar;
	struct ieee80211_channel *channel;
	int freq = -1;

	if (unlikely(rxs->band == NUM_NL80211_BANDS ||
		     !ath12k_ar_to_hw(ar)->wiphy->bands[rxs->band])) {
		ath12k_dbg(ab, ATH12K_DBG_DP_MON,
			   "sband is NULL for status band %d channel_num %d pdev_id %d\n",
			   rxs->band, channel_num, ar->pdev_idx);

		spin_lock_bh(&ar->data_lock);
		channel = ar->rx_channel;
		if (channel) {
			rxs->band = channel->band;
			channel_num =
				ieee80211_frequency_to_channel(channel->center_freq);
		}
		spin_unlock_bh(&ar->data_lock);
	}

	if (rxs->band < NUM_NL80211_BANDS)
		freq = ieee80211_channel_to_frequency(channel_num, rxs->band);

	return freq;
}

static
int ath12k_wifi6_dp_mon_rx_deliver_mpdu(struct ath12k_pdev_dp *dp_pdev,
					struct sk_buff *head_msdu,
					struct sk_buff *tail_msdu,
					struct hal_rx_mon_ppdu_info *ppdu_info,
					struct napi_struct *napi)
{
	struct ath12k_dp *dp = dp_pdev->dp;
	struct ath12k_base *ab = dp->ab;
	struct sk_buff *mpdu, *next_mpdu;
	struct ieee80211_rx_status rxs = {0};
	int freq_update = -1;

	if (!dp_pdev) {
		goto fail;
	}

	mpdu = ath12k_wifi6_dp_mon_rx_mpdu_restitch(dp_pdev, head_msdu, tail_msdu,
						       ppdu_info);
	/* If MPDU restitch fails, free buffers*/
	if (!mpdu)
		goto fail;

	ath12k_dp_mon_fill_rx_stats_info(ppdu_info, &rxs);

	freq_update = ath12k_wifi6_dp_mon_rx_update_band_and_get_freq(dp_pdev,
								      ppdu_info->chan_num,
								      &rxs);
	if (freq_update != -1)
		rxs.freq = freq_update;

	ath12k_dp_mon_update_radiotap(dp_pdev, ppdu_info, mpdu, &rxs);

	rxs.flag |= RX_FLAG_ONLY_MONITOR;
	if (skb_shinfo(mpdu)->nr_frags)
		rxs.flag |= RX_FLAG_AMSDU_MORE;

	if (ppdu_info->rs_fcs_err)
		rxs.flag |= RX_FLAG_FAILED_FCS_CRC;

	ath12k_dp_mon_rx_deliver_skb(dp_pdev, napi, mpdu, &rxs, ppdu_info);

	return 0;
fail:
	mpdu = head_msdu;
	while (mpdu) {
		next_mpdu = mpdu->next;
		dev_kfree_skb_any(mpdu);
		ath12k_dbg(ab, ATH12K_DBG_DP_MON, "mpdu deliver failed mpdu = %pK"
			   "len = %u\n", mpdu, mpdu->len);
		mpdu = next_mpdu;
	}
	return -EINVAL;
}

/* The destination ring processing is stuck if the destination is not
 * moving while status ring moves 16 PPDU. The destination ring processing
 * skips this destination ring PPDU as a workaround.
 */
#define MON_DEST_RING_STUCK_MAX_CNT 16

static
void ath12k_wifi6_dp_mon_rx_dest_process(struct ath12k_pdev_dp *dp_pdev, int mac_id,
					 u32 quota, struct napi_struct *napi)
{
	struct ath12k_dp *dp;
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_mon_data *pmon;
	struct ath12k_pdev_mon_stats *rx_mon_stats;
	u32 ppdu_id, rx_bufs_used = 0, ring_id;
	u32 mpdu_rx_bufs_used, npackets = 0;
	struct ath12k_dp_mon *dp_mon;
	struct ath12k_base *ab;
	void *ring_entry, *mon_dst_srng;
	LIST_HEAD(rx_desc_used_list);
	struct hal_srng *srng;

	if (!dp_pdev) {
		ath12k_dbg(NULL, ATH12K_DBG_DP_MON, "dp pdev is null for mac_id %d\n",
			   mac_id);
		return;
	}

	dp = dp_pdev->dp;
	dp_mon_pdev = dp_pdev->dp_mon_pdev;
	dp_mon = dp->dp_mon;
	ab = dp->ab;
	pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;

	ring_id = ath12k_wifi6_dp_rxdma_get_mon_dst_ring(dp_pdev, mac_id);
	srng = &ab->hal.srng_list[ring_id];

	mon_dst_srng = &ab->hal.srng_list[ring_id];

	if (!mon_dst_srng || !srng->initialized) {
		ath12k_warn(dp, "Monitor destination ring is not initialized\n");
		return;
	}

	spin_lock_bh(&srng->lock);

	ath12k_hal_srng_access_begin(ab, mon_dst_srng);

	ppdu_id = pmon->mon_ppdu_info.ppdu_id;
	rx_mon_stats = &pmon->rx_mon_stats;

	while (likely(ring_entry = ath12k_hal_srng_dst_peek(ab, mon_dst_srng))) {
		struct sk_buff *head_msdu, *tail_msdu;

		head_msdu = NULL;
		tail_msdu = NULL;

		mpdu_rx_bufs_used =
			ath12k_wifi6_dp_mon_rx_mpdu_pop(dp_pdev, mac_id, ring_entry,
							&head_msdu, &tail_msdu,
							&rx_desc_used_list,
							&npackets, &ppdu_id);

		rx_bufs_used += mpdu_rx_bufs_used;

		if (mpdu_rx_bufs_used) {
			dp_mon->mon_dest_ring_stuck_cnt = 0;
		} else {
			dp_mon->mon_dest_ring_stuck_cnt++;
			rx_mon_stats->dest_mon_not_reaped++;
		}

		if (dp_mon->mon_dest_ring_stuck_cnt > MON_DEST_RING_STUCK_MAX_CNT) {
			rx_mon_stats->dest_mon_stuck++;
			ath12k_dbg(ab, ATH12K_DBG_DP_MON,
				   "status ring ppdu_id=%d dest ring ppdu_id=%d"
				   " mon_dest_ring_stuck_cnt=%d dest_mon_not_reaped=%u"
				   " dest_mon_stuck=%u\n",
				   pmon->mon_ppdu_info.ppdu_id, ppdu_id,
				   dp_mon->mon_dest_ring_stuck_cnt,
				   rx_mon_stats->dest_mon_not_reaped,
				   rx_mon_stats->dest_mon_stuck);
			spin_lock_bh(&pmon->mon_lock);
			pmon->mon_ppdu_info.ppdu_id = ppdu_id;
			spin_unlock_bh(&pmon->mon_lock);
			continue;
		}

		if (ppdu_id != pmon->mon_ppdu_info.ppdu_id) {
			spin_lock_bh(&pmon->mon_lock);
			pmon->mon_ppdu_status = DP_PPDU_STATUS_START;
			spin_unlock_bh(&pmon->mon_lock);
			rx_mon_stats->status_ring_ppdu_id_hist[
				rx_mon_stats->ppdu_id_hist_idx] =
				pmon->mon_ppdu_info.ppdu_id;
			rx_mon_stats->dest_ring_ppdu_id_hist[
				rx_mon_stats->ppdu_id_hist_idx] = ppdu_id;
			rx_mon_stats->ppdu_id_hist_idx =
				(rx_mon_stats->ppdu_id_hist_idx + 1) &
					(MAX_PPDU_ID_HIST - 1);
			ath12k_dbg(ab, ATH12K_DBG_DP_MON, "ppdu_id %x !="
				   " pmon->mon_ppdu_info.ppdu_id %x\n", ppdu_id,
				   pmon->mon_ppdu_info.ppdu_id);
			break;
		}

		if (likely(head_msdu && tail_msdu)) {
			ath12k_wifi6_dp_mon_rx_deliver_mpdu(dp_pdev,
							    head_msdu,
							    tail_msdu,
							    &pmon->mon_ppdu_info,
							    napi);
			rx_mon_stats->dest_mpdu_done++;
		}

		ring_entry = ath12k_hal_srng_dst_get_next_entry(ab,
								mon_dst_srng);
	}
	ath12k_hal_srng_access_end(ab, mon_dst_srng);

	spin_unlock_bh(&srng->lock);

	if (rx_bufs_used) {
		rx_mon_stats->dest_ppdu_done++;
		ath12k_wifi6_dp_buf_replenish(dp,
					      ath12k_dp_rxdma_get_mon_buf_ring(dp),
					      &rx_desc_used_list, 0,
					      ath12k_wifi6_dp_rx_get_mon_desc_pool(dp));
	}
}

static inline void
ath12k_wifi6_dp_mon_update_scan_radio_stats(struct ath12k_pdev_dp *pdev_dp,
					    struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev;
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn;
	struct ath12k_dp_rx_scan_radio_stats *stats;
	u32 num_users, user;

	dp_mon_pdev = pdev_dp->dp_mon_pdev;
	if (!dp_mon_pdev || !ppdu_info)
		return;

	mon_dp_extn = &dp_mon_pdev->pdev_mon_dp_extn;
	stats = &mon_dp_extn->rx_scan_radio_stats;

	num_users = ppdu_info->num_users;
	for (user = 0; user < num_users; user++) {
		struct hal_rx_user_status *rx_user_status =
			&ppdu_info->userstats[user];

		stats->rx_ok_pkts += rx_user_status->mpdu_cnt_fcs_ok;
		stats->rx_ok_bytes += rx_user_status->mpdu_ok_byte_count;
		stats->rx_err_pkts += rx_user_status->mpdu_cnt_fcs_err;
		stats->rx_err_bytes += rx_user_status->mpdu_err_byte_count;
	}

	stats->rx_mgmt_pkts +=
		ppdu_info->ppdu_info_extn.frm_type_info.rx_mgmt_cnt;
	stats->rx_ctrl_pkts +=
		ppdu_info->ppdu_info_extn.frm_type_info.rx_ctrl_cnt;
	stats->rx_data_pkts +=
		ppdu_info->ppdu_info_extn.frm_type_info.rx_data_cnt;
}

/**
 * ath12k_wifi6_dp_rx_is_valid_undecoded_frame() - Check if abort reason is
 *     covered by the configured PHY error mask
 * @err_mask: 64-bit combined PHY error mask
 * @err_code: PHY abort reason code to validate
 *
 * Return: true if the error code is within range and set in the mask
 */
static inline bool
ath12k_wifi6_dp_rx_is_valid_undecoded_frame(u64 err_mask, u8 err_code)
{
	if (err_code < ATH12K_WIFI6_PHYRX_ERR_MAX &&
	    (err_mask & (1ULL << err_code)))
		return true;
	return false;
}

static inline bool
ath12k_wifi6_dp_rx_mon_check_phyrx_abort(struct ath12k_pdev_dp *pdev_dp,
					 struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn;

	if (!dp_mon_pdev)
		return false;

	mon_dp_extn = &dp_mon_pdev->pdev_mon_dp_extn;

	if (!mon_dp_extn->undecoded_meta ||
	    !ppdu_info->ppdu_info_extn.phyrx_abort_info)
		return false;

	return (mon_dp_extn->undecoded_meta->undecoded_metadata_capture &&
		ppdu_info->ppdu_info_extn.phyrx_abort_info->phyrx_abort);
}

static void
ath12k_wifi6_dp_rx_handle_ppdu_undecoded_metadata(struct ath12k_pdev_dp *pdev_dp,
						  struct hal_rx_mon_ppdu_info *ppdu_info)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn;

	if (!dp_mon_pdev)
		return;

	mon_dp_extn = &dp_mon_pdev->pdev_mon_dp_extn;
	u64 mask64;
	u8 abort_reason;

	mask64 = ((u64)mon_dp_extn->undecoded_meta->phyrx_error_mask_cont << ATH12K_WIFI6_PHYRX_ERR_MASK_CONT_SHIFT) |
		 mon_dp_extn->undecoded_meta->phyrx_error_mask;
	abort_reason = ppdu_info->ppdu_info_extn.phyrx_abort_info->phyrx_abort_reason;

	if (!ath12k_wifi6_dp_rx_is_valid_undecoded_frame(mask64, abort_reason))
		return;

	mon_dp_extn->undecoded_meta->rx_undecoded_count++;
	if (abort_reason < ATH12K_WIFI6_PHYRX_ERR_MAX)
		mon_dp_extn->undecoded_meta->rx_undecoded_error[abort_reason]++;

	/* TODO: Report undecoded frame metadata to userspace via cfg80211 vendor
	 * event or Linux notifier chain. This should include:
	 * - PHY abort reason (ppdu_info->ppdu_info_extn.phyrx_abort_reason)
	 * - SIG/CRC fields from ppdu_info->ppdu_info_extn
	 * - Other relevant PPDU metadata (channel, timestamp, RSSI, etc.)
	 */
}

static inline void
ath12k_wifi6_dp_mon_handle_ppdu_undecoded_metadata(struct ath12k_pdev_dp *pdev_dp,
						   struct hal_rx_mon_ppdu_info *ppdu_info,
						   struct ath12k_mon_data *pmon)
{
	ath12k_wifi6_dp_rx_handle_ppdu_undecoded_metadata(pdev_dp, ppdu_info);
	pmon->mon_ppdu_status = DP_PPDU_STATUS_START;
}

void
ath12k_wifi6_dp_mon_rx_htt_phy_err_filter_cfg(void *ptr,
					      struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd = ptr;

	if (!tlv_filter->phy_err_filter_valid)
		return;

	cmd->info2 |= le32_encode_bits(tlv_filter->fp_phy_err,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO2_FP_PHY_ERR);
	cmd->info2 |= le32_encode_bits(tlv_filter->fp_phy_err_buf_src,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO2_FP_PHY_ERR_BUF_SRC);
	cmd->info2 |= le32_encode_bits(tlv_filter->fp_phy_err_buf_dest,
				       HTT_RX_RING_SELECTION_CFG_CMD_INFO2_FP_PHY_ERR_BUF_DEST);
	cmd->phy_err_mask = le32_encode_bits(tlv_filter->phy_err_mask,
					     HTT_RX_RING_SELECTION_CFG_CMD_PHY_ERR_MASK);
	cmd->phy_err_mask_cont = le32_encode_bits(tlv_filter->phy_err_mask_cont,
						  HTT_RX_RING_SELECTION_CFG_CMD_PHY_ERR_MASK_CONT);
}

int ath12k_wifi6_dp_mon_rx_quad_ring_process(struct ath12k_pdev_dp *pdev_dp, int mac_id,
					     struct napi_struct *napi, int *budget)
{
	struct ath12k *ar = pdev_dp->ar;
	struct ath12k_pdev_mon_dp *dp_mon_pdev = pdev_dp->dp_mon_pdev;
	struct ath12k_mon_data *pmon = (struct ath12k_mon_data *)&dp_mon_pdev->mon_data;
	struct ath12k_pdev_mon_stats *rx_mon_stats = &pmon->rx_mon_stats;
	struct hal_rx_mon_ppdu_info *ppdu_info = &pmon->mon_ppdu_info;
	enum hal_rx_mon_status hal_status;
	struct sk_buff_head skb_list;
	int num_buffs_reaped;
	struct sk_buff *skb;

	__skb_queue_head_init(&skb_list);

	num_buffs_reaped = ath12k_wifi6_dp_mon_rx_reap_status_ring(pdev_dp, mac_id,
								   budget, &skb_list);
	if (!num_buffs_reaped)
		goto exit;

	while ((skb = __skb_dequeue(&skb_list))) {
		ppdu_info->peer_id = HAL_INVALID_PEERID;
		/* Restore phyrx_abort_info from per-pdev storage after per-PPDU
		 * memset of hal_rx_mon_ppdu_info in hal_mon.c
		 */
		if (dp_mon_pdev) {
			ppdu_info->ppdu_info_extn.phyrx_abort_info =
				dp_mon_pdev->pdev_mon_dp_extn.phyrx_abort_info;
			if (ppdu_info->ppdu_info_extn.phyrx_abort_info)
				ppdu_info->ppdu_info_extn.phyrx_abort_info->phyrx_abort = 0;
		} else {
			ppdu_info->ppdu_info_extn.phyrx_abort_info = NULL;
		}

		hal_status = ath12k_wifi6_dp_mon_rx_parse_dest(pdev_dp, skb);

		ath12k_wifi6_dp_mon_update_scan_radio_stats(pdev_dp, ppdu_info);

		if (hal_status == HAL_TLV_STATUS_PPDU_DONE) {
			if (ath12k_wifi6_dp_rx_mon_check_phyrx_abort(pdev_dp,
								     ppdu_info)) {
				ath12k_wifi6_dp_mon_handle_ppdu_undecoded_metadata(pdev_dp,
										   ppdu_info,
										   pmon);
			} else if (ar->monitor_started &&
				   pmon->mon_ppdu_status == DP_PPDU_STATUS_START) {
				rx_mon_stats->status_ppdu_done++;
				pmon->mon_ppdu_status = DP_PPDU_STATUS_DONE;
				ath12k_wifi6_dp_mon_rx_dest_process(pdev_dp, mac_id,
								    *budget, napi);
				pmon->mon_ppdu_status = DP_PPDU_STATUS_START;
			}
		}

		dev_kfree_skb_any(skb);
	}
exit:
	return num_buffs_reaped;
}

void ath12k_wifi6_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp *dp_mon_pdev = dp_pdev->dp_mon_pdev;
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn =
		&dp_mon_pdev->pdev_mon_dp_extn;

	ath12k_wifi6_dp_mon_rx_mon_mode_config_filter(dp_pdev);
	if (mon_dp_extn->undecoded_meta &&
	    mon_dp_extn->undecoded_meta->undecoded_metadata_capture)
		ath12k_wifi6_dp_mon_rx_undecoded_metadata_config_filter(dp_pdev);
}

void ath12k_wifi6_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev)
{
	struct ath12k_pdev_mon_dp_extn *mon_dp_extn =
		&dp_pdev->dp_mon_pdev->pdev_mon_dp_extn;

	if (mon_dp_extn->undecoded_meta)
		ath12k_wifi6_dp_mon_rx_undecoded_metadata_reset_filter(dp_pdev);
	ath12k_wifi6_dp_mon_rx_mon_mode_reset_filter(dp_pdev);
}
