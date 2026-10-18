// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "dp_tx_flow_info.h"
#include "dp.h"
#include "../debug.h"
#include "dp_htt.h"
#include "dp_peer.h"
#include "dp_pool.h"
#include "dp_tx_queue.h"

int ath12k_dp_tx_classify_info_alloc(struct ath12k_dp_hw_group *dp_hw_grp,
				     dma_addr_t *tx_classify_info_paddr,
				     void **tx_classify_info_vaddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	size_t alloc_size = ATH12K_NUM_TX_CLASSIFY_BANKS *
			    ATH12K_TX_CLASSIFY_INFO_SIZE_SINGLE;
	void *vaddr;
	dma_addr_t paddr;

	vaddr = kzalloc(alloc_size, GFP_ATOMIC);
	if (!vaddr)
		return -ENOMEM;

	paddr = ath12k_core_dma_map_single(dev, vaddr, alloc_size, DMA_BIDIRECTIONAL);
	if (!paddr) {
		kfree(vaddr);
		return -ENOMEM;
	}

	*tx_classify_info_vaddr = vaddr;
	*tx_classify_info_paddr = paddr;
	return 0;
}

void ath12k_dp_tx_classify_info_free(struct ath12k_dp_hw_group *dp_hw_grp,
				     dma_addr_t tx_classify_info_paddr,
				     void *tx_classify_info_vaddr)
{
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);

	ath12k_core_dma_unmap_single(dev, tx_classify_info_paddr,
				     ATH12K_NUM_TX_CLASSIFY_BANKS *
				     ATH12K_TX_CLASSIFY_INFO_SIZE_SINGLE,
				     DMA_BIDIRECTIONAL);
	kfree(tx_classify_info_vaddr);
}

void ath12k_dp_pn_counter_page_free(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info = dp_hw_grp_wifi8->pn_page_info;
	int i;

	if (!pn_info || !dev)
		return;
	for (i = 0; i < dp_hw_grp_wifi8->num_pn_pages; i++) {
		if (!pn_info[i].vaddr) {
			ath12k_err(NULL, "pn_page %d is NULL\n", i);
			continue;
		}
		ath12k_core_dma_unmap_single(dev, pn_info[i].paddr,
					     PAGE_SIZE, DMA_BIDIRECTIONAL);
		kfree(pn_info[i].vaddr);
		pn_info[i].vaddr = NULL;
	}

	kfree(pn_info);
	dp_hw_grp_wifi8->pn_page_info = NULL;
}

int ath12k_dp_pn_counter_page_init(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct device *dev = ath12k_dp_get_dev_from_dp_hw_group(dp_hw_grp);
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info;
	u8 num_pn_pages = ATH12K_DP_PN_NUM_PAGE;
	int ret;
	int i;

	if (dp_hw_grp_wifi8->pn_page_info)
		return 0;

	pn_info = kcalloc(num_pn_pages, sizeof(struct ath12k_pn_page_info),
			  GFP_KERNEL);

	if (!pn_info) {
		ath12k_warn(ab, "Failed to allocate PN page info");
		return -ENOMEM;
	}

	for (i = 0; i < num_pn_pages; i++) {
		pn_info[i].vaddr = kzalloc(PAGE_SIZE, GFP_KERNEL);
		if (!pn_info[i].vaddr) {
			ret = -ENOMEM;
			goto free;
		}

		pn_info[i].paddr = ath12k_core_dma_map_single(dev,
							      pn_info[i].vaddr,
							      PAGE_SIZE,
							      DMA_BIDIRECTIONAL);
		if (!pn_info[i].paddr) {
			ath12k_warn(ab, "PN page DMA error");
			ret = -ENOMEM;
			goto free;
		}
		if (pn_info[i].paddr & ATH12K_SPT_4K_ALIGN_CHECK) {
			ath12k_warn(ab, "PN page allocated memory is not 4K aligned");
			ret = -EINVAL;
			goto free;
		}
	}

	dp_hw_grp_wifi8->num_pn_pages = num_pn_pages;
	dp_hw_grp_wifi8->pn_page_info = (void *)pn_info;
	return 0;

free:
	ath12k_dp_pn_counter_page_free(dp_hw_grp);
	return ret;
}

dma_addr_t ath12k_dp_get_page_paddr(struct ath12k_dp_hw_group *dp_hw_grp,
				    u16 sw_peer_id)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info = dp_hw_grp_wifi8->pn_page_info;
	u8 page_index = u16_get_bits(sw_peer_id, ATH12K_DP_PN_PAGE_INDEX);
	u8 page_offset = u16_get_bits(sw_peer_id, ATH12K_DP_PN_PAGE_OFFSET);
	dma_addr_t page_paddr;

	if (!pn_info)
		return 0;

	page_paddr = pn_info[page_index].paddr;
	page_offset = page_offset * ATH12K_DP_PN_COUNTER_SIZE;
	return (dma_addr_t)(((u8 *)page_paddr) + page_offset);
}

void *ath12k_dp_get_page_vaddr(struct ath12k_dp_hw_group *dp_hw_grp,
			       u16 sw_peer_id)
{
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
					ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	struct ath12k_pn_page_info *pn_info = dp_hw_grp_wifi8->pn_page_info;
	u8 page_index = u16_get_bits(sw_peer_id, ATH12K_DP_PN_PAGE_INDEX);
	u8 page_offset = u16_get_bits(sw_peer_id, ATH12K_DP_PN_PAGE_OFFSET);
	void *page_vaddr;

	if (!pn_info)
		return NULL;

	page_vaddr = pn_info[page_index].vaddr;
	page_offset = page_offset * ATH12K_DP_PN_COUNTER_SIZE;
	return (void *)(((u8 *)page_vaddr) + page_offset);
}

int ath12k_wifi8_dp_tx_pool_create(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);
	u32 aligned_size;
	int queue_id;

	spin_lock_init(&dp_hw_grp_wifi8->tx_pool_lock);
	dp_hw_grp_wifi8->msduq_ctxt = init_memory_pool(ab, MSDU_STRUCT_SZ,
						       NUM_TOTAL_MSDU_QUEUES,
#ifdef PLATFORM_SDX
						       false);
#else
						       true);
#endif
	if (!dp_hw_grp_wifi8->msduq_ctxt)
		goto err_msduq_alloc;

	if (dma_map_pages(ab, dp_hw_grp_wifi8->msduq_ctxt)) {
		ath12k_err(ab, "DMA MAP failed for MSDUQ\n");
		goto err_msduq_dma;
	}

	aligned_size = sizeof(struct ath12k_dp_msdu_q_info);
	aligned_size = roundup_pow_of_two(aligned_size);
	dp_hw_grp_wifi8->sw_msduq_ctxt = init_memory_pool(ab, aligned_size,
							  NUM_TOTAL_MSDU_QUEUES,
#ifdef PLATFORM_SDX
							  false);
#else
							  true);
#endif
	if (!dp_hw_grp_wifi8->sw_msduq_ctxt)
		goto err_sw_msduq_alloc;

	dp_hw_grp_wifi8->mpduq_ctxt = init_memory_pool(ab, MPDU_STRUCT_SZ,
						       NUM_TOTAL_MPDU_QUEUES,
#ifdef PLATFORM_SDX
						       false);
#else
						       true);
#endif
	if (!dp_hw_grp_wifi8->mpduq_ctxt)
		goto err_mpduq_alloc;

	if (dma_map_pages(ab, dp_hw_grp_wifi8->mpduq_ctxt)) {
		ath12k_err(ab, "DMA MAP failed for MPDUQ\n");
		goto err_mpduq_dma;
	}

	aligned_size = sizeof(struct ath12k_dp_mpdu_q_info);
	aligned_size = roundup_pow_of_two(aligned_size);
	dp_hw_grp_wifi8->sw_mpduq_ctxt = init_memory_pool(ab, aligned_size,
							  NUM_TOTAL_MPDU_QUEUES,
#ifdef PLATFORM_SDX
							  false);
#else
							  true);
#endif
	if (!dp_hw_grp_wifi8->sw_mpduq_ctxt)
		goto err_sw_mpduq_alloc;

	bitmap_zero(dp_hw_grp_wifi8->msduq_sam_id_alloc_map,
		    MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED);
	bitmap_zero(dp_hw_grp_wifi8->mpduq_sam_id_alloc_map,
		    MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED);

	/*
	 * Set the last 5 queue IDs in every 256-queue block as unsupported queues.
	 * SAM MSDUQ IDs 251-255, 507-511, 763-767, 1019-1023 .... 3835-3839, 4091-4095
	 * SAM MPDUQ IDs 251-255, 507-511, 763-767, 1019-1023
	 * are invalid queue ids in allocated bitmap in v1 HW.
	 */
	if (ab->hw_rev == ATH12K_HW_QCN9625_HW10) {
		for (queue_id = 251; queue_id < MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED;
		     queue_id += 256) {
			bitmap_set(dp_hw_grp_wifi8->msduq_sam_id_alloc_map, queue_id, 5);

			if (queue_id < MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED)
				bitmap_set(dp_hw_grp_wifi8->mpduq_sam_id_alloc_map,
					   queue_id, 5);
		}
	}

	dp_hw_grp_wifi8->last_msduq_sam_id = MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED - 1;
	dp_hw_grp_wifi8->last_mpduq_sam_id = MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED - 1;
	spin_lock_init(&dp_hw_grp_wifi8->sam_id_lock);

	return 0;
err_sw_mpduq_alloc:
	dma_unmap_pages(ab, dp_hw_grp_wifi8->mpduq_ctxt);
err_mpduq_dma:
	pool_destroy(ab, dp_hw_grp_wifi8->mpduq_ctxt);
	dp_hw_grp_wifi8->mpduq_ctxt = NULL;
err_mpduq_alloc:
	pool_destroy(ab, dp_hw_grp_wifi8->sw_msduq_ctxt);
	dp_hw_grp_wifi8->sw_msduq_ctxt = NULL;
err_sw_msduq_alloc:
	dma_unmap_pages(ab, dp_hw_grp_wifi8->msduq_ctxt);
err_msduq_dma:
	pool_destroy(ab, dp_hw_grp_wifi8->msduq_ctxt);
	dp_hw_grp_wifi8->msduq_ctxt = NULL;
err_msduq_alloc:
	return -ENOMEM;
}

void ath12k_wifi8_dp_tx_pool_destroy(struct ath12k_dp_hw_group *dp_hw_grp)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_hw_group_wifi8 *dp_hw_grp_wifi8 =
		ath12k_get_dp_hw_group_wifi8(dp_hw_grp);

	if (dp_hw_grp_wifi8->msduq_ctxt) {
		dma_unmap_pages(ab, dp_hw_grp_wifi8->msduq_ctxt);
		pool_destroy(ab, dp_hw_grp_wifi8->msduq_ctxt);
		dp_hw_grp_wifi8->msduq_ctxt = NULL;
	}
	if (dp_hw_grp_wifi8->sw_msduq_ctxt) {
		pool_destroy(ab, dp_hw_grp_wifi8->sw_msduq_ctxt);
		dp_hw_grp_wifi8->sw_msduq_ctxt = NULL;
	}
	if (dp_hw_grp_wifi8->mpduq_ctxt) {
		dma_unmap_pages(ab, dp_hw_grp_wifi8->mpduq_ctxt);
		pool_destroy(ab, dp_hw_grp_wifi8->mpduq_ctxt);
		dp_hw_grp_wifi8->mpduq_ctxt = NULL;
	}
	if (dp_hw_grp_wifi8->sw_mpduq_ctxt) {
		pool_destroy(ab, dp_hw_grp_wifi8->sw_mpduq_ctxt);
		dp_hw_grp_wifi8->sw_mpduq_ctxt = NULL;
	}
}

int ath12k_dp_tx_mcast_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					 struct ath12k_dp_peer *dp_peer)
{
	struct ath12k_dp_tx_flow_info *tx_info =
		ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	struct ath12k_dp_mpdu_q_info *mpduq;
	struct ath12k_dp_msdu_q_info *msduq;
	struct list_head mpduq_list_head;
	struct list_head msduq_list_head;
	int ret;

	if (!tx_info)
		return -EINVAL;

	INIT_LIST_HEAD(&mpduq_list_head);
	INIT_LIST_HEAD(&msduq_list_head);
	spin_lock_bh(&tx_info->tx_q_lock);

	/* mcast mpdu queue */
	mpduq = tx_info->mcast_mpduq;
	if (mpduq && mpduq->mpduq_state == ATH12K_TX_Q_CREATED)
		list_add_tail(&mpduq->list, &mpduq_list_head);

	/* mcast msdu queue */
	msduq = tx_info->mcast_msduq;
	if (msduq && msduq->msduq_state == ATH12K_TX_Q_CREATED)
		list_add_tail(&msduq->list, &msduq_list_head);

	ret = ath12k_dp_tx_htt_peer_msduq_mpduq_setup(dp_hw_grp,
						      dp_peer,
						      &mpduq_list_head,
						      &msduq_list_head,
						      ATH12K_GROUP_MAX_RADIO,
						      true);
	spin_unlock_bh(&tx_info->tx_q_lock);
	return ret;
}

int ath12k_dp_tx_peer_msduq_mpduq_setup(struct ath12k_dp_hw_group *dp_hw_grp,
					struct ath12k_dp_peer *dp_peer,
					u8 link_id)
{
	struct ath12k_base *ab = ath12k_dp_get_ab_from_dp_hw_group(dp_hw_grp);
	struct ath12k_dp_tx_flow_info *tx_info =
		ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	struct list_head mpduq_pending_list_head;
	struct list_head msduq_pending_list_head;
	struct ath12k_dp_mpdu_q_info *mpduq;
	struct ath12k_dp_msdu_q_info *msduq;
	int i;
	int j;
	int ret;

	if (!tx_info)
		return -EINVAL;

	INIT_LIST_HEAD(&mpduq_pending_list_head);
	INIT_LIST_HEAD(&msduq_pending_list_head);
	spin_lock_bh(&tx_info->tx_q_lock);
	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "smd txq-setup: peer %pM peer_id=%u link_id=%u assoc_links=0x%lx txq_links=0x%lx\n",
			 dp_peer->addr, dp_peer->peer_id, link_id,
			 tx_info->assoc_hw_links_bitmap,
			 tx_info->txq_hw_links_bitmap);
	/* data tid queues */
	for (i = 0; i < ATH12K_MAX_NUM_DATA_TIDS; i++) {
		mpduq = tx_info->tid_info[i].mpduq;
		if (mpduq &&
		    (mpduq->mpduq_state == ATH12K_TX_Q_CREATED ||
		     mpduq->mpduq_state == ATH12K_TX_Q_MODIFIED))
			list_add_tail(&mpduq->list, &mpduq_pending_list_head);

		for (j = 0; j < ATH12K_MAX_DP_MSDUQ_PER_TID; j++) {
			msduq = tx_info->tid_info[i].msduq[j];
			if (msduq &&
			    (msduq->msduq_state == ATH12K_TX_Q_CREATED ||
			     msduq->msduq_state == ATH12K_TX_Q_MODIFIED))
				list_add_tail(&msduq->list, &msduq_pending_list_head);
		}
	}

	ret = ath12k_dp_tx_htt_peer_msduq_mpduq_setup(dp_hw_grp, dp_peer,
						      &mpduq_pending_list_head,
						      &msduq_pending_list_head,
						      link_id,
						      false);
	spin_unlock_bh(&tx_info->tx_q_lock);

	ath12k_dbg_level(ab, ATH12K_DBG_PEER, ATH12K_DBG_L1,
			 "smd txq-setup: HTT setup ret=%d assoc_links=0x%lx txq_links=0x%lx\n",
			 ret, tx_info->assoc_hw_links_bitmap,
			 tx_info->txq_hw_links_bitmap);

	return ret;
}

int ath12k_wifi8_qos_queue_setup(struct ath12k_dp_hw_group *dp_hw_grp,
				 struct ath12k_dp_peer *dp_peer,
				 u8 msduq, u16 qos_id,
				 u8 qm_id, bool txop_intent)
{
	u32 tid;
	struct ath12k_dp_tx_flow_info *tx_info;
	enum htt_tx_tid_msduq_mpdu_type flow_type;
	struct ath12k_dp_tx_queue_metadata tx_queue_params = {0};
	int ret;

	tx_info = ath12k_dp_get_tx_flow_info_from_peer(dp_peer);
	if (!tx_info)
		return -ENOENT;

	tid = u32_get_bits(msduq, MSDUQ_TID);

	spin_lock_bh(&tx_info->tx_q_lock);
	if (!tx_info->tid_info[tid].msduq[HTT_TID_MSDUQ_CUSTOM_0])
		flow_type = HTT_TID_MSDUQ_CUSTOM_0;
	else
		flow_type = HTT_TID_MSDUQ_CUSTOM_1;
	spin_unlock_bh(&tx_info->tx_q_lock);

	tx_queue_params.tidno = tid;
	tx_queue_params.flow_type = flow_type;
	tx_queue_params.encap_type = HAL_TCL_ENCAP_TYPE_ETHERNET;
	tx_queue_params.q_params.svc_id = qos_id & 0xFF;
	tx_queue_params.q_params.qm_id = qm_id;
	tx_queue_params.q_params.txop_intent = txop_intent;

	ret = ath12k_peer_alloc_dynamic_queue(dp_hw_grp, dp_peer,
					      &tx_queue_params);

	return ret;
}
