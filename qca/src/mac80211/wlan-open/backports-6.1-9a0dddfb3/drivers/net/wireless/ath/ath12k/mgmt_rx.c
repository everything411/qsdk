// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "mgmt_rx.h"
#include "hif.h"
#include "hal.h"
#include "debug.h"

void ath12k_mgmt_irq_grp_setup(struct ath12k_mgmt *mgmt)
{
	int i;

	for (i = 0; i < mgmt->num_irq_grp; i++) {
		struct ath12k_mgmt_irq_grp *irq_grp = &mgmt->irq_grp[i];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		INIT_WORK(&irq_grp->intr_wq, irq_grp->irq_grp_handler);
#else
		tasklet_setup(&irq_grp->intr_tq, irq_grp->irq_grp_handler);
#endif
	}
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_setup);

void ath12k_mgmt_irq_grp_cleanup(struct ath12k_mgmt *mgmt)
{
	int i;

	for (i = 0; i < mgmt->num_irq_grp; i++) {
		struct ath12k_mgmt_irq_grp *irq_grp = &mgmt->irq_grp[i];
#if LINUX_VERSION_IS_GEQ(6, 13, 0)
		cancel_work_sync(&irq_grp->intr_wq);
#else
		tasklet_kill(&irq_grp->intr_tq);
#endif
	}
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_cleanup);

void ath12k_mgmt_irq_grp_enable(struct ath12k_mgmt_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		enable_irq(irq_grp->irqs[i]);
}
EXPORT_SYMBOL(ath12k_mgmt_irq_grp_enable);

void ath12k_mgmt_irq_grp_disable(struct ath12k_mgmt_irq_grp *irq_grp)
{
	int i;

	for (i = 0; i < irq_grp->num_irq; i++)
		disable_irq_nosync(irq_grp->irqs[i]);
}

int ath12k_mgmt_device_init(struct ath12k_mgmt *mgmt)
{
	/* Skip device_init if mgmt is not supported */
	if (!mgmt)
		return 0;

	return ath12k_mgmt_arch_op_device_init(mgmt);
}

void ath12k_mgmt_device_deinit(struct ath12k_mgmt *mgmt)
{
	if (!mgmt)
		return;

	if (test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &mgmt->ab->dev_flags) ||
	    mgmt->ab->umcmn_fatal_received) {
		ath12k_info(mgmt->ab, "Skip mgmt deinit during Q6 BCR RESET\n");
		return;
	}

	ath12k_mgmt_arch_op_device_deinit(mgmt);
}

static void ath12k_mgmt_srng_msi_setup(struct ath12k_base *ab,
				       struct hal_srng_params *ring_params,
				       enum hal_ring_type type, int grp_id)
{
	int num_vectors;
	u32 base_user_data, base_vector, addr_lo, addr_hi;
	int ret;

	switch (type) {
	case HAL_REO_DST_MGMT:
	case HAL_REO_EXCEPTION_MGMT:
		break;
	default:
#ifdef CPTCFG_QCN_EXTN
		if (!ath12k_mgmt_srng_type_valid_extn(ab, type))
			break;
#endif
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "Ring (type=%u) not part of mgmt_group for msi setup", type);
		return;
	}

	ret = ath12k_hif_get_user_msi_vector(ab, "MGMT",
					     &num_vectors, &base_user_data,
					     &base_vector);
	if (ret)
		return;

	if (grp_id < 0) {
		ath12k_dbg(ab, ATH12K_DBG_PCI,
			   "Ring part of invalid mgmt_group; ring_type: %u, grp_id: %d",
			   type, grp_id);
		ring_params->msi_addr = 0;
		ring_params->msi_data = 0;
		return;
	}

	ath12k_hif_get_msi_address(ab, &addr_lo, &addr_hi);

	ring_params->msi_addr = addr_lo;
	ring_params->msi_addr |= (dma_addr_t)(((uint64_t)addr_hi) << 32);
	ring_params->msi_data = base_user_data + (grp_id % num_vectors);
	ring_params->flags |= HAL_SRNG_FLAGS_MSI_INTR;
}

int ath12k_mgmt_srng_setup(struct ath12k_base *ab, struct mgmt_srng *ring,
			   enum hal_ring_type type, int ring_num,
			   int mac_id, int grp_id, int num_entries)
{
	struct hal_srng_params params = { 0 };
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	int ret;
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

	ring->size = (num_entries * entry_sz) + HAL_RING_BASE_ALIGN - 1;

#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst rings from cacheable memory */
		if (type == HAL_REO_DST_MGMT)
			cached = true;
	}
#else
	cached = true;
#endif
	if (ath12k_dp_umac_reset_in_progress(ab))
		goto skip_dma_alloc;

#ifdef CPTCFG_QCN_EXTN
	ath12k_mgmt_srng_setup_extn(ab, type, &cached, &params);
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

skip_dma_alloc:
	ring->vaddr = PTR_ALIGN(ring->vaddr_unaligned, HAL_RING_BASE_ALIGN);
	ring->paddr = ring->paddr_unaligned + ((unsigned long)ring->vaddr -
		      (unsigned long)ring->vaddr_unaligned);

	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = num_entries;

	ath12k_mgmt_srng_msi_setup(ab, &params, type, grp_id);

	switch (type) {
	case HAL_REO_DST_MGMT:
		params.intr_batch_cntr_thres_entries = HAL_SRNG_INT_BATCH_THRESHOLD_RX;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
		break;
	case HAL_REO_EXCEPTION_MGMT:
	case HAL_WBM_BUF_MGMT:
	case HAL_WBM_IDLE_BUF_MGMT:
		params.intr_batch_cntr_thres_entries =
					HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
	default:
#ifdef CPTCFG_QCN_EXTN
		if (!ath12k_mgmt_srng_type_valid_extn(ab, type))
			break;
#endif
		ath12k_warn(ab, "Not a valid ring type for MGMT :%d", type);
		return -EINVAL;
	}

	if (cached) {
		params.flags |= HAL_SRNG_FLAGS_CACHED;
		ring->cached = 1;
	}

	ret = ath12k_hal_srng_setup_idx(ab, type, ring_num, mac_id, &params, 0);
	if (ret < 0) {
		ath12k_warn(ab, "Failed to setup mgmt srng: %d ring_id %d",
			    ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;

	return 0;
}
EXPORT_SYMBOL(ath12k_mgmt_srng_setup);

void ath12k_mgmt_srng_cleanup(struct ath12k_base *ab, struct mgmt_srng *ring)
{
	if (!ring->vaddr_unaligned)
		return;

	if (ring->cached)
		kfree(ring->vaddr_unaligned);
	else
		ath12k_hal_dma_free_coherent(ab->dev, ring->size,
					     ring->vaddr_unaligned,
					     ring->paddr_unaligned);

	ring->vaddr_unaligned = NULL;
}
EXPORT_SYMBOL(ath12k_mgmt_srng_cleanup);

struct ath12k_rx_desc_info *ath12k_mgmt_get_rx_desc_from_cookie(struct ath12k_mgmt *mgmt,
								u32 cookie)
{
	u16 block, slot;

	block = u32_get_bits(cookie, MGMT_RX_DESC_BLOCK_MASK);
	slot = u32_get_bits(cookie, MGMT_RX_DESC_SLOT_MASK);

	if (block >= NUM_MGMT_RX_DESC_BLOCKS ||
	    !mgmt->rx_desc_baddr[block] ||
	    slot >= MGMT_RX_DESC_BLOCK_SIZE)
		return NULL;

	return &mgmt->rx_desc_baddr[block][slot];
}
EXPORT_SYMBOL(ath12k_mgmt_get_rx_desc_from_cookie);

int ath12k_mgmt_rx_desc_init(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_rx_desc_info *rx_descs;
	u16 i, j;

	spin_lock_init(&mgmt->rx_desc_lock);
	INIT_LIST_HEAD(&mgmt->rx_desc_free_list);

	spin_lock_bh(&mgmt->rx_desc_lock);

	for (i = 0; i < NUM_MGMT_RX_DESC_BLOCKS; i++) {
		rx_descs = kcalloc(MGMT_RX_DESC_BLOCK_SIZE, sizeof(*rx_descs),
				   GFP_ATOMIC);
		if (!rx_descs) {
			spin_unlock_bh(&mgmt->rx_desc_lock);
			/* Clean up previously allocated descriptors */
			while (i > 0) {
				i--;
				kfree(mgmt->rx_desc_baddr[i]);
				mgmt->rx_desc_baddr[i] = NULL;
			}
			return -ENOMEM;
		}

		mgmt->rx_desc_baddr[i] = &rx_descs[0];

		for (j = 0; j < MGMT_RX_DESC_BLOCK_SIZE; j++) {
			rx_descs[j].cookie = ath12k_mgmt_gen_rx_desc_cookie(i, j);
			rx_descs[j].magic = ATH12K_MGMT_RX_DESC_MAGIC;
			rx_descs[j].device_id = ab->device_id;
			rx_descs[j].in_use = false;

			list_add_tail(&rx_descs[j].list, &mgmt->rx_desc_free_list);
		}
	}

	spin_unlock_bh(&mgmt->rx_desc_lock);

	return 0;
}
EXPORT_SYMBOL(ath12k_mgmt_rx_desc_init);

void ath12k_mgmt_rx_desc_cleanup(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	struct ath12k_rx_desc_info *desc_info;
	LIST_HEAD(used_list);
	struct sk_buff *skb;
	bool in_umac_reset;
	int i, j;

	in_umac_reset = test_bit(ATH12K_FLAG_UMAC_RECOVERY_IN_PROGRESS, &ab->dev_flags);

	spin_lock_bh(&mgmt->rx_desc_lock);

	for (i = 0; i < NUM_MGMT_RX_DESC_BLOCKS; i++) {
		desc_info = mgmt->rx_desc_baddr[i];
		if (!desc_info)
			continue;

		for (j = 0; j < MGMT_RX_DESC_BLOCK_SIZE; j++) {
			if (!desc_info[j].in_use)
				continue;

			skb = desc_info[j].skb;
			if (!skb)
				continue;

			if (in_umac_reset) {
				list_add_tail(&desc_info[j].list, &used_list);
				continue;
			}

			ath12k_core_dma_unmap_single(ab->dev, ATH12K_SKB_RXCB(skb)->paddr,
						     skb->len + skb_tailroom(skb),
						     DMA_FROM_DEVICE);
			dev_kfree_skb_any(skb);
		}

		if (!in_umac_reset) {
			kfree(desc_info);
			mgmt->rx_desc_baddr[i] = NULL;
		}
	}

	spin_unlock_bh(&mgmt->rx_desc_lock);

	if (!list_empty(&used_list))
		ath12k_mgmt_rx_replenish_buffs(ab->mgmt, &used_list, true);
}
EXPORT_SYMBOL(ath12k_mgmt_rx_desc_cleanup);

size_t ath12k_mgmt_rx_desc_list_cut_nodes(struct list_head *used_list,
					  struct list_head *rx_desc_list,
					  size_t count)
{
	struct list_head *curr;
	struct ath12k_rx_desc_info *rx_desc;
	size_t nodes = 0;

	if (!count) {
		INIT_LIST_HEAD(used_list);
		goto out;
	}

	list_for_each(curr, rx_desc_list) {
		if (!count)
			break;

		rx_desc = list_entry(curr, struct ath12k_rx_desc_info, list);

		count--;
		nodes++;
	}

	list_cut_before(used_list, rx_desc_list, curr);

out:
	return nodes;
}
EXPORT_SYMBOL(ath12k_mgmt_rx_desc_list_cut_nodes);

size_t ath12k_mgmt_get_req_entries_from_refill_ring(struct ath12k_base *ab,
						    struct mgmt_srng *rx_refill_ring,
						    struct list_head *list)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;
	size_t num_free, req_entries;
	struct hal_srng *srng;

	srng = &ab->hal.srng_list[rx_refill_ring->ring_id];

	spin_lock_bh(&srng->lock);
	ath12k_hal_srng_access_begin(ab, srng);

	num_free = ath12k_hal_srng_src_num_free(ab, srng, true);
	if (!num_free) {
		ath12k_hal_srng_access_end(ab, srng);
		spin_unlock_bh(&srng->lock);
		return 0;
	}

	spin_lock_bh(&mgmt->rx_desc_lock);
	req_entries = ath12k_mgmt_rx_desc_list_cut_nodes(list, &mgmt->rx_desc_free_list,
							 num_free);
	spin_unlock_bh(&mgmt->rx_desc_lock);

	ath12k_hal_srng_access_end(ab, srng);
	spin_unlock_bh(&srng->lock);

	return req_entries;
}
EXPORT_SYMBOL(ath12k_mgmt_get_req_entries_from_refill_ring);

struct sk_buff *ath12k_mgmt_rx_get_mmpdu_last_buf(struct sk_buff_head *mmpdu_list,
						  struct sk_buff *first)
{
	struct sk_buff *skb;
	struct ath12k_skb_rxcb *rxcb = ATH12K_SKB_RXCB(first);

	if (!rxcb->is_continuation)
		return first;

	skb_queue_walk(mmpdu_list, skb) {
		rxcb = ATH12K_SKB_RXCB(skb);
		if (!rxcb->is_continuation)
			return skb;
	}

	return NULL;
}
EXPORT_SYMBOL(ath12k_mgmt_rx_get_mmpdu_last_buf);

int ath12k_mgmt_htt_setup(struct ath12k_hw_group *ag)
{
	struct ath12k_base *partner_ab;
	int i, ret;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (!partner_ab || partner_ab->is_bypassed || !partner_ab->mgmt)
			continue;

		ret = ath12k_mgmt_arch_op_htt_setup(partner_ab->mgmt);
		if (ret) {
			ath12k_err(partner_ab, "Failed to set up MGMT HTT: %d", ret);
			return ret;
		}
	}

	return 0;
}

bool ath12k_mgmt_override_mld_tx(struct ath12k_base *ab)
{
	struct ath12k_mgmt *mgmt = ab->mgmt;

	if (!mgmt)
		return true;

	return ath12k_mgmt_arch_op_override_mld_tx(mgmt);
}
