/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/debugfs.h>
#include <asm/cacheflush.h>
#ifdef NSS_DP_PON_SUPPORT
#include <fal/fal_pon.h>
#endif
#include "nss_dp_dev.h"
#include "edma_regs.h"
#include "edma_debug.h"
#include "edma.h"
#include <ppe_drv_sc.h>
#include "syn_dev.h"
#include <linux/debug_mem_usage.h>

#ifdef CONFIG_IPQ_PON
#include "nss_dp_gem.h"

/* Extern for GEM callbacks */
extern nss_dp_gem_tx_cb_t nss_dp_gem_tx_reg_cb_g;
extern void *nss_dp_gem_tx_app_data_g;
#endif

#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
extern int syn_ptp_get_tx_hwtstamp_by_id(struct syn_hal_dev *shd,
					  u16 pkt_id,
					  struct skb_shared_hwtstamps *shhwtstamps);

/*
 * edma_tx_mac_tstamp_buf()
 *	Enable MAC hardware timestamping for XGMAC
 *
 * This function enables MAC-level hardware timestamping for XGMAC PTP.
 * It sets the timestamp enable bit (bit 11) in the EDMA TX descriptor
 * word 1 and marks the SKB with SKBTX_IN_PROGRESS flag for later
 * timestamp retrieval from MAC registers.
 *
 * Note: This is different from PHY timestamping. MAC timestamps are
 * read from XGMAC registers (0xd30, 0xd34) in the TX completion handler.
 */
static inline void edma_tx_mac_tstamp_buf(struct nss_dp_dev *dp_dev,
					  struct sk_buff *skb,
					  struct edma_pri_txdesc *desc)
{
	static atomic_t pkt_id_counter = ATOMIC_INIT(0);
	struct nss_gmac_hal_dev *nghd;
	struct syn_hal_dev *shd;
	u16 pkt_id;

	/* Get HAL device structure */
	nghd = (struct nss_gmac_hal_dev *)dp_dev->gmac_hal_ctx;
	if (!nghd || !nghd->mac_base)
		return;

	/* Check if this is a Synopsys XGMAC with PTP support */
	shd = container_of(nghd, struct syn_hal_dev, nghd);
	if (!shd->ptp_priv)
		return;

	/* Generate sequential packet ID (1-1023) with wraparound */
	pkt_id = (atomic_inc_return(&pkt_id_counter) % 1023) + 1;

	/* Save packet ID and originating interface in SKB control buffer */
	EDMA_TX_CB(skb)->ptp_pkt_id = pkt_id;
	EDMA_TX_CB(skb)->dp_dev = dp_dev;

	/* Mark SKB as waiting for hardware timestamp */
	skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;

	/* Write packet ID to EDMA descriptor */
	EDMA_TXDESC_TIMESTAMP_EN_SET(desc, 1);
	EDMA_TXDESC_TIMESTAMP_TAG_EN_SET(desc, 1);
	EDMA_TXDESC_TIMESTAMP_TAG_SET(desc, pkt_id);
}

/*
 * edma_tx_complete_mac_tstamp()
 *	Retrieve MAC TX timestamp in completion
 *
 * This function retrieves the TX timestamp from XGMAC MAC registers
 * and reports it to the network stack via skb_tstamp_tx().
 * Called from TX completion handler before freeing the SKB.
 *
 * Note: The MAC hardware may take a few microseconds to capture the
 * timestamp after packet transmission. This function polls the hardware
 * registers to wait for timestamp availability before reporting it.
 */
static inline void edma_tx_complete_mac_tstamp(struct sk_buff *skb)
{
	struct nss_dp_dev *orig_dp_dev;
	struct nss_gmac_hal_dev *nghd;
	struct syn_hal_dev *shd;
	struct skb_shared_hwtstamps shhwtstamps;
	u16 pkt_id;
	int ret;

	/* Get originating interface from SKB control buffer */
	orig_dp_dev = EDMA_TX_CB(skb)->dp_dev;
	if (!orig_dp_dev)
		return;

	/* Get HAL device structure from originating interface */
	nghd = (struct nss_gmac_hal_dev *)orig_dp_dev->gmac_hal_ctx;
	if (!nghd)
		return;

	shd = container_of(nghd, struct syn_hal_dev, nghd);
	if (!shd->ptp_priv)
		return;

	/* Retrieve packet ID from SKB control buffer */
	pkt_id = EDMA_TX_CB(skb)->ptp_pkt_id;

	/* Look up timestamp by packet ID */
	ret = syn_ptp_get_tx_hwtstamp_by_id(shd, pkt_id, &shhwtstamps);
	if (ret == 0) {
		/* Timestamp found - report it to PTP stack */
		skb_tstamp_tx(skb, &shhwtstamps);
	} else if (ret == -ENODATA) {
		/* Timestamp not available - may occur if FIFO is not drained yet */
		if (net_ratelimit()) {
			dev_warn(&orig_dp_dev->netdev->dev,
				"TX timestamp not found for pkt_id=%u\n", pkt_id);
		}
	}
}
#endif

/*
 * edma_tx_complete()
 *	Reap Tx descriptors
 */
uint32_t edma_tx_complete(uint32_t work_to_do, struct edma_txcmpl_ring *txcmpl_ring)
{
	struct edma_txcmpl_desc *txcmpl;
	struct edma_tx_cmpl_stats *txcmpl_stats = &txcmpl_ring->tx_cmpl_stats;
	uint32_t prod_idx = 0;
	uint32_t cons_idx;
	uint32_t data;
	struct sk_buff *skb;
	struct sk_buff_head h;
	uint32_t txcmpl_errors;
	uint32_t avail, count;
	uint8_t cpu_id;
	uint32_t end_idx;
	uint32_t more_bit = 0;
	struct netdev_queue *nq;
#ifdef CONFIG_IO_COHERENCY
	struct edma_txcmpl_desc *pf_desc;
	uint32_t pf_cons_idx;
#endif

	cons_idx = txcmpl_ring->cons_idx;

	if (likely(txcmpl_ring->avail_pkt >= work_to_do)) {
		avail = work_to_do;
	} else {

		/*
		 * Get TXCMPL ring producer index
		 */
		data = edma_reg_read(EDMA_REG_TXCMPL_PROD_IDX(txcmpl_ring->id));
		prod_idx = data & EDMA_TXCMPL_PROD_IDX_MASK;

		avail = EDMA_DESC_AVAIL_COUNT(prod_idx, cons_idx, txcmpl_ring->count);
		txcmpl_ring->avail_pkt = avail;

		if (unlikely(!avail)) {
			edma_debug("No available descriptors are pending for %d txcmpl ring\n",
					txcmpl_ring->id);
			u64_stats_update_begin(&txcmpl_stats->syncp);
			++txcmpl_stats->no_pending_desc;
			u64_stats_update_end(&txcmpl_stats->syncp);
			return 0;
		}

		avail = min(avail, work_to_do);
	}

	count = avail;

	end_idx = (cons_idx + avail) & EDMA_TX_RING_SIZE_MASK(txcmpl_ring->count);
	txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);

	if (end_idx > cons_idx) {
		edma_dmac_inv_range_no_dsb((void *)txcmpl, txcmpl + avail);
	} else {
		edma_dmac_inv_range_no_dsb(txcmpl_ring->desc, txcmpl_ring->desc + end_idx);
		edma_dmac_inv_range_no_dsb((void *)txcmpl, txcmpl_ring->desc + txcmpl_ring->count);
	}

	dsb(st);

	skb_queue_head_init(&h);

#ifdef CONFIG_IO_COHERENCY
	if (likely(avail >= 5)) {
		/*
		 * Fetching the (5th (cons_idx + 4)) descriptor for prefetch purpose
		 */
		pf_cons_idx = ((cons_idx + 4) & EDMA_TX_RING_SIZE_MASK(txcmpl_ring->count));
		pf_desc = EDMA_TXCMPL_DESC(txcmpl_ring, pf_cons_idx);
	}
#endif

	/*
	 * TODO:
	 * Instead of freeing the skb, it might be better to save and use
	 * for Rxfill.
	 */
	while (likely(avail--)) {

#ifdef CONFIG_IO_COHERENCY
		if (likely(avail >= 5)) {
			struct sk_buff *pf_skb;

			/*
			 * Prefetch 5th skb's 128B cache line from the current index
			 */
			pf_skb = (struct sk_buff *)EDMA_TXCMPL_OPAQUE_GET(pf_desc);
			prefetch((uint8_t *)pf_skb + 128);

			/*
			 * Prefetch 6th descriptor's cache line from the current index
			 */
			pf_cons_idx = ((pf_cons_idx + 1) & EDMA_TX_RING_SIZE_MASK(txcmpl_ring->count));
			pf_desc = EDMA_TXCMPL_DESC(txcmpl_ring, pf_cons_idx);
			prefetch(pf_desc);
		}
#endif

		/*
		 * The last descriptor holds the SKB pointer for scattered frames.
		 * So skip the descriptors with more bit set.
		 */
		more_bit = EDMA_TXCMPL_MORE_BIT_GET(txcmpl);
		if (unlikely(more_bit)) {
			u64_stats_update_begin(&txcmpl_stats->syncp);
			++txcmpl_stats->desc_with_more_bit;
			u64_stats_update_end(&txcmpl_stats->syncp);
			cons_idx = ((cons_idx + 1) & EDMA_TX_RING_SIZE_MASK(txcmpl_ring->count));
			txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);
			continue;
		}

		/*
		 * Find and free the skb for Tx completion
		 */
		skb = (struct sk_buff *)EDMA_TXCMPL_OPAQUE_GET(txcmpl);
		if (unlikely(!skb)) {
			if (net_ratelimit()) {
				edma_warn("Invalid skb: cons_idx:%u prod_idx:%u word2:%x word3:%x\n",
						cons_idx, prod_idx, txcmpl->word2, txcmpl->word3);
			}

			u64_stats_update_begin(&txcmpl_stats->syncp);
			++txcmpl_stats->invalid_buffer;
			u64_stats_update_end(&txcmpl_stats->syncp);
		} else {
			edma_debug("skb:%px, skb->len %d, skb->data_len %d, cons_idx:%d prod_idx:%d word2:0x%x word3:0x%x\n",
					skb, skb->len, skb->data_len, cons_idx, prod_idx, txcmpl->word2, txcmpl->word3);

			txcmpl_errors = EDMA_TXCOMP_RING_ERROR_GET(txcmpl->word3);
			if (unlikely(txcmpl_errors)) {
				/*
				 * Demux the txcmpl error type.
				 * There can multiple txcmpl errors in the same descriptors.
				 * Hence we need to check for all the set
				 * bits instead of just the first one.
				 */
				long bit_pos;
				u64_stats_update_begin(&txcmpl_stats->syncp);
				bit_pos = __builtin_ffs(txcmpl_errors);
				while (bit_pos) {
					if (EDMA_DEBUG_LEVEL > 2) {
						if (net_ratelimit()) {
							edma_info("Error 0x%0x observed in tx complete %d ring\n",
									txcmpl_errors, txcmpl_ring->id);
						}
					}

					++txcmpl_stats->errors[bit_pos - 1];
					txcmpl_errors = txcmpl_errors & ~(0x1 << (bit_pos - 1));
					bit_pos = __builtin_ffs(txcmpl_errors);
				}
				u64_stats_update_end(&txcmpl_stats->syncp);
			}

#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
			/* Retrieve MAC TX timestamp if needed */
			if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS)) {
				edma_tx_complete_mac_tstamp(skb);
			}
#endif

			/*
			 * Fast-recycle the SKB with a list, if skb is originally allocated
			 * from recycler and has been fast trasmitted
			 */
			if (likely(skb->fast_xmit) && likely(skb->is_from_recycler)) {
				mem_debug_update_skb(skb);
				dev_check_skb_fast_recyclable(skb);
				__skb_queue_head(&h, skb);
			} else {
				mem_debug_update_skb(skb);
				dev_kfree_skb(skb);
			}
		}

		cons_idx = ((cons_idx + 1) & EDMA_TX_RING_SIZE_MASK(txcmpl_ring->count));
		txcmpl = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);
	}

	if (likely(!skb_queue_empty(&h))) {
		dev_kfree_skb_list_fast(&h);
	}

	txcmpl_ring->cons_idx = cons_idx;
	txcmpl_ring->avail_pkt -= count;

	edma_debug("TXCMPL:%u count:%u prod_idx:%u cons_idx:%u\n",
			txcmpl_ring->id, count, prod_idx, cons_idx);
	edma_reg_write(EDMA_REG_TXCMPL_CONS_IDX(txcmpl_ring->id), cons_idx);

	/*
	 * If tx_requeue_stop disabled (tx_requeue_stop = 0)
	 * Fetch the tx queue of interface and check if it is stopped.
	 * if queue is stopped and interface is up, wake up this queue.
	 */
	if (likely(!dp_global_ctx.tx_requeue_stop)) {
		cpu_id = smp_processor_id();
		nq = netdev_get_tx_queue(txcmpl_ring->napi.dev, cpu_id);
		if (unlikely(netif_tx_queue_stopped(nq)) && netif_carrier_ok(txcmpl_ring->napi.dev)) {
			edma_debug("Waking queue number %d, for interface %s\n", cpu_id, txcmpl_ring->napi.dev->name);
			__netif_tx_lock(nq, cpu_id);
			netif_tx_wake_queue(nq);
			__netif_tx_unlock(nq);
		}
	}

	return count;
}

/*
 * edma_tx_napi_poll()
 *	EDMA TX NAPI handler
 */
int edma_tx_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_txcmpl_ring *txcmpl_ring = (struct edma_txcmpl_ring *)napi;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t txcmpl_intr_status;
	int work_done = 0;
	uint32_t reg_data;

	do {
		work_done += edma_tx_complete(budget - work_done, txcmpl_ring);
		if (work_done >= budget) {
			return work_done;
		}

		reg_data = edma_reg_read(EDMA_REG_TX_INT_STAT(txcmpl_ring->id));
		txcmpl_intr_status = reg_data & EDMA_TXCMPL_RING_INT_STATUS_MASK;
	} while (txcmpl_intr_status);

	/*
	 * No more packets to process. Finish NAPI processing.
	 */
	napi_complete(napi);

	/*
	 * Set TXCMPL ring interrupt mask
	 */
	edma_reg_write(EDMA_REG_TX_INT_MASK(txcmpl_ring->id),
			egc->txcmpl_intr_mask);

	return work_done;
}

/*
 * edma_tx_handle_irq()
 *	Process TX IRQ and schedule napi
 */
irqreturn_t edma_tx_handle_irq(int irq, void *ctx)
{
	struct edma_txcmpl_ring *txcmpl_ring = (struct edma_txcmpl_ring *)ctx;

	edma_debug("irq: irq=%d txcmpl_ring_id=%u\n", irq, txcmpl_ring->id);
	if (likely(napi_schedule_prep(&txcmpl_ring->napi))) {
		/*
		 * Disable TxCmpl intr
		 */
		edma_reg_write(EDMA_REG_TX_INT_MASK(txcmpl_ring->id),
				EDMA_MASK_INT_DISABLE);
		__napi_schedule(&txcmpl_ring->napi);
	}

	return IRQ_HANDLED;
}

/*
 * edma_tx_desc_init()
 *	Initializes the Tx descriptor
 */
static inline void edma_tx_desc_init(struct edma_pri_txdesc *txdesc)
{
	memset(txdesc, 0, sizeof(struct edma_pri_txdesc));
}

/*
 * edma_tx_is_tso_eligible()
 *	Check if the skb is eligible for TSO (TCP Segmentation Offload)
 */
static inline bool edma_tx_is_tso_eligible(struct sk_buff *skb)
{
	if (skb_is_gso(skb)) {
		if ((skb_shinfo(skb)->gso_type == SKB_GSO_TCPV4) ||
				(skb_shinfo(skb)->gso_type == SKB_GSO_TCPV6)) {
			return true;
		}
	}

	return false;
}

/*
 * edma_tx_fill_tso_desc()
 *	Populate common TSO fields on Tx descriptor
 */
static inline void edma_tx_fill_tso_desc(struct edma_pri_txdesc *txd, struct sk_buff *skb,
					struct edma_tx_stats *stats)
{
	uint32_t mss;

	/*
	 * Check if the packet needs TSO
	 * This will be mostly true for SG packets.
	 */
	if (unlikely(!edma_tx_is_tso_eligible(skb))) {
		return;
	}

	mss = skb_shinfo(skb)->gso_size;

	/*
	 * If MSS<256, HW will do TSO using MSS=256,
	 * if MSS>10K, HW will do TSO using MSS=10K,
	 * else HW will report error 0x200000 in Tx Cmpl
	 */
	if (mss < EDMA_TX_TSO_MSS_MIN)
		mss = EDMA_TX_TSO_MSS_MIN;
	else if (mss > EDMA_TX_TSO_MSS_MAX)
		mss = EDMA_TX_TSO_MSS_MAX;

	EDMA_TXDESC_TSO_ENABLE_SET(txd, 1);
	EDMA_TXDESC_MSS_SET(txd, mss);

	/*
	 * Update tso stats
	 */
	u64_stats_update_begin(&stats->syncp);
	stats->tx_tso_pkts++;
	u64_stats_update_end(&stats->syncp);
}

/*
 * edma_tx_skb_nr_frags()
 *	Process Tx for skb with nr_frags
 */
static uint32_t edma_tx_skb_nr_frags(struct edma_txdesc_ring *txdesc_ring, struct edma_pri_txdesc **txdesc,
		struct nss_dp_dev *dp_dev, struct sk_buff *skb, uint32_t *hw_next_to_use)
{
	uint8_t i = 0;
	uint32_t nr_frags = 0, buf_len = 0, num_descs = 0, start_idx = 0, end_idx = 0;
	struct edma_pri_txdesc *txd = *txdesc;
	dma_addr_t buff_addr;
	uint8_t pt_val;

	if (unlikely(skb_cloned(skb))) {
		pt_val = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
	} else if (!(skb->fast_forwarded || skb->fast_xmit)) {
		pt_val = edma_ddrq_sp_cloned_pt_mode;
	} else {
		pt_val = dp_dev->pt_info.dst_pt_mode_val;
	}

	/*
	 * Hold onto the index mapped to *txdesc.
	 * This will be the index previous to that of current *hw_next_to_use
	 */
	start_idx = (((*hw_next_to_use) + EDMA_TX_RING_SIZE_MASK(txdesc_ring->count)) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count));

	/*
	 * Handle if the skb has nr_frags
	 */
	nr_frags = skb_shinfo(skb)->nr_frags;
	num_descs = nr_frags;
	i = 0;
	while (nr_frags--) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];
		buf_len = skb_frag_size(frag);

		/*
		 * Zero size segment can lead EDMA HW to hang so, we don't want to process them.
		 * Zero size segment can happen during TSO operation if there is nothing but header
		 * in the primary segment.
		 */
		if (unlikely(buf_len == 0)) {
			num_descs--;
			i++;
			continue;
		}

		/*
		 * Setting the MORE bit on the previous Tx descriptor.
		 * Note: We will flush this descriptor as well later.
		 */
		EDMA_TXDESC_MORE_BIT_SET(txd, 1);
		EDMA_TXDESC_ENDIAN_SET(txd);

		txd = EDMA_TXDESC_PRI_DESC(txdesc_ring, *hw_next_to_use);
		edma_tx_desc_init(txd);
		buff_addr = (dma_addr_t)virt_to_phys(skb_frag_address(frag));
		EDMA_TXDESC_BUFFER_ADDR_SET(txd, buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
		EDMA_TXDESC_BUFFER_ADDR_HI_SET(txd, buff_addr);
#endif
		EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, pt_val);
		edma_dmac_clean_range_no_dsb((void *)skb_frag_address(frag),
				(void *)(skb_frag_address(frag) + buf_len));

		EDMA_TXDESC_DATA_LEN_SET(txd, buf_len);

		/*
		 * Setting up the TSO bit even for frags apart
		 * from the head skb.
		 */
		if (unlikely(edma_tx_is_tso_eligible(skb))) {
			EDMA_TXDESC_TSO_ENABLE_SET(txd, 1);
		}

		*hw_next_to_use = ((*hw_next_to_use + 1) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count));
		i++;
	}

	EDMA_TXDESC_ENDIAN_SET(txd);

	/*
	 * This will be the index previous to that of current *hw_next_to_use
	 */
	end_idx = (((*hw_next_to_use) + EDMA_TX_RING_SIZE_MASK(txdesc_ring->count)) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count));

	/*
	 * Need to flush from initial *txdesc to accomodate for MORE bit change.
	 * Need to flush all the descriptors but last-one that we filled as well.
	 */
	if (end_idx > start_idx) {
		edma_dmac_clean_range_no_dsb((void *)(*txdesc),
				((*txdesc) + num_descs));
	} else {
		edma_dmac_clean_range_no_dsb(txdesc_ring->pdesc,
				txdesc_ring->pdesc + end_idx);
		edma_dmac_clean_range_no_dsb((void *)(*txdesc),
				txdesc_ring->pdesc + txdesc_ring->count);
	}

	*txdesc = txd;
	return num_descs;
}

/*
 * edma_tx_fill_vp_desc()
 *	Enable PPE processing with VP as source/dest port
 */
static inline void edma_tx_fill_vp_desc(struct nss_dp_dev *dp_dev, struct edma_pri_txdesc *txd,
			struct sk_buff *skb, struct nss_dp_vp_tx_info *dptxi)
{
	if (unlikely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		EDMA_TXDESC_ADV_OFFLOAD_SET(txd);
		EDMA_TXDESC_IP_CSUM_SET(txd);
		EDMA_TXDESC_L4_CSUM_SET(txd);
	}

	EDMA_TXDESC_SERVICE_CODE_SET(txd, dptxi->sc);

	/*
	 * Set fake mac bit needed for L3 interfaces
	 */
	EDMA_TXDESC_FAKE_MAC_HDR_SET(txd, dptxi->fake_mac);

	/*
	 * Set Source/Dest port information in the descriptor
	 */
	if (dptxi->svp) {
		EDMA_SRC_INFO_SET(txd, dptxi->svp);
		EDMA_DST_INFO_SET(txd, 0);
		(dptxi->flags & NSS_DP_VP_TX_FLAG_PKT_MODE) ?
			EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, dptxi->pkt_mode):
			EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, dp_dev->pt_info.src_pt_mode_val);
	} else {
		EDMA_SRC_INFO_SET(txd, 0);
		EDMA_DST_INFO_SET(txd, dptxi->dvp);
		(dptxi->flags & NSS_DP_VP_TX_FLAG_PKT_MODE) ?
			EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, dptxi->pkt_mode):
			EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, dp_dev->pt_info.dst_pt_mode_val);
	}
}

/*
 * edma_tx_fill_pp_desc()
 *	Populate descriptor fields to bypass PPE processing and forward
 */
static inline void edma_tx_fill_pp_desc(struct nss_dp_dev *dp_dev, struct edma_pri_txdesc *txd,
					struct sk_buff *skb)
{
	/*
	 * Offload L3/L4 checksum computation
	 */
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		EDMA_TXDESC_ADV_OFFLOAD_SET(txd);
		EDMA_TXDESC_IP_CSUM_SET(txd);
		EDMA_TXDESC_L4_CSUM_SET(txd);
	}

	EDMA_DST_INFO_SET(txd, dp_dev->macid);
	EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, dp_dev->pt_info.dst_pt_mode_val);
	EDMA_TXDESC_SERVICE_CODE_SET(txd, dp_dev->pt_info.sc);
	EDMA_TXDESC_INT_PRI_SET(txd, skb_get_int_pri(skb));

	/*
	 * Set the src info as destination dev in case if
	 * port mirroring is enabled - to receive the packet back in
	 * DP with valid source port.
	 */
#if defined NSS_DP_PORT_MIRROR_EN
	EDMA_SRC_INFO_SET(txd, dp_dev->macid);
#endif
}

/*
 * edma_tx_skb_first_desc()
 *	Process the Tx for the first descriptor required for skb
 */
static struct edma_pri_txdesc *edma_tx_skb_first_desc(struct nss_dp_dev *dp_dev, struct edma_txdesc_ring *txdesc_ring,
					struct nss_dp_vp_tx_info *dptxi, struct sk_buff *skb, uint32_t *hw_next_to_use,
					struct edma_tx_stats *stats, void *gem_txi_info)
{
	uint32_t buf_len = 0;
	struct edma_pri_txdesc *txd = NULL;
	dma_addr_t buff_addr;

	/*
	 * Get the packet length
	 */
	buf_len = skb_headlen(skb);

	txd = EDMA_TXDESC_PRI_DESC(txdesc_ring, *hw_next_to_use);
#ifdef CONFIG_IO_COHERENCY
	prefetchw(txd);
#endif

	edma_tx_desc_init(txd);

	if (unlikely(skb_headroom(skb) < EDMA_DDRQ_PREHEADER_SIZE)) {
		if (pskb_expand_head(skb, EDMA_DDRQ_PREHEADER_SIZE, 0, GFP_ATOMIC)) {
			edma_err("Can't expand skb: %px\n", skb);
			return NULL;
		}
	}

	/*
	 * Set the data pointer as the buffer address in the descriptor.
	 */
	buff_addr = (dma_addr_t)virt_to_phys((skb->data - EDMA_DDRQ_PREHEADER_SIZE));
#ifdef NSS_DP_PCIE_BUS_ENABLE
	buff_addr = edma_phy_addr_trans(buff_addr);
#endif
	EDMA_TXDESC_BUFFER_ADDR_SET(txd, buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
	EDMA_TXDESC_BUFFER_ADDR_HI_SET(txd, buff_addr);
#endif

	EDMA_TXDESC_DATA_OFFSET_SET(txd, EDMA_DDRQ_PREHEADER_SIZE);

	/*
	 * Set packet length in the descriptor
	 */
	EDMA_TXDESC_DATA_LEN_SET(txd, buf_len);

	if (dptxi) {
		edma_tx_fill_vp_desc(dp_dev, txd, skb, dptxi);
	} else {
		edma_tx_fill_pp_desc(dp_dev, txd, skb);
	}

	edma_tx_fill_tso_desc(txd, skb, stats);

	if (unlikely(skb_cloned(skb))) {
		EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA);
	} else if (!(skb->fast_forwarded || skb->fast_xmit)) {
		EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, edma_ddrq_sp_cloned_pt_mode);
	}

#ifdef CONFIG_IPQ_PON
	struct nss_dp_gem_tx_info *gem_txi = (struct nss_dp_gem_tx_info *)gem_txi_info;

	/* GEM TX */
	if (dp_dev->gem_port) {
		/* src port */
		if (gem_txi->src_dev_valid) {
			int src_port_id = nss_dp_get_port_num(gem_txi->src_dev);
			if (src_port_id != NSS_DP_INVALID_INTERFACE) {
				EDMA_SRC_INFO_CLEAR(txd);
				EDMA_SRC_INFO_SET(txd, src_port_id);
			}
		}
	}
#endif
	edma_dmac_clean_range_no_dsb((void *)(skb->data - EDMA_DDRQ_PREHEADER_SIZE),
			 (void *)(skb->data + buf_len + EDMA_DDRQ_PREHEADER_SIZE));

	*hw_next_to_use = (*hw_next_to_use + 1) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count);

	return txd;
}

/*
 * edma_tx_skb_sg_fill_desc()
 *	API to fill SG skb into Tx descriptor. Handles both nr_frags and fraglist cases.
 */
static uint32_t edma_tx_skb_sg_fill_desc(struct nss_dp_dev *dp_dev, struct edma_txdesc_ring *txdesc_ring,
		struct edma_pri_txdesc **txdesc, struct sk_buff *skb, uint32_t *hw_next_to_use,
		struct edma_tx_stats *stats)
{
	uint32_t buf_len = 0, num_descs = 0;
	struct sk_buff *iter_skb = NULL;
	uint32_t num_sg_frag_list = 0;
	struct edma_pri_txdesc *txd = *txdesc;
	dma_addr_t buff_addr;
	uint8_t pt_val = 0;

	/*
	 * Head skb processed already
	 */
	num_descs++;

	/*
	 * Process skb with nr_frags
	 */
	if (unlikely(skb_shinfo(skb)->nr_frags)) {
		num_descs += edma_tx_skb_nr_frags(txdesc_ring, &txd, dp_dev, skb, hw_next_to_use);
		u64_stats_update_begin(&stats->syncp);
		stats->tx_nr_frag_pkts++;
		u64_stats_update_end(&stats->syncp);
	}

	if (unlikely(skb_cloned(skb))) {
		pt_val = EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA;
	} else if (!(skb->fast_forwarded || skb->fast_xmit)) {
		pt_val = edma_ddrq_sp_cloned_pt_mode;
	} else {
		pt_val = dp_dev->pt_info.dst_pt_mode_val;
	}
	if (unlikely(skb_has_frag_list(skb))) {
		struct edma_pri_txdesc *start_desc = NULL;
		uint32_t start_idx = 0, end_idx = 0;

		/*
		 * Hold onto the index mapped to txd.
		 * This will be the index previous to that of current *hw_next_to_use
		 */
	start_idx = (((*hw_next_to_use) + EDMA_TX_RING_SIZE_MASK(txdesc_ring->count)) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count));
	start_desc = txd;

		/*
		 * Walk through all fraglist skbs
		 */
		skb_walk_frags(skb, iter_skb) {
			uint32_t num_nr_frag = 0;

			/*
			 * This case could happen during the packet decapsulation. All header content might be removed.
			 */
			buf_len = skb_headlen(iter_skb);
			if (unlikely(buf_len == 0)) {
				goto skip_primary;
			}

			/*
			 * We make sure to flush this descriptor later
			 */
			EDMA_TXDESC_MORE_BIT_SET(txd, 1);
			EDMA_TXDESC_ENDIAN_SET(txd);

			txd = EDMA_TXDESC_PRI_DESC(txdesc_ring, *hw_next_to_use);
			edma_tx_desc_init(txd);
			buff_addr = (dma_addr_t)virt_to_phys(iter_skb->data);
			EDMA_TXDESC_BUFFER_ADDR_SET(txd, buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
			EDMA_TXDESC_BUFFER_ADDR_HI_SET(txd, buff_addr);
#endif
			EDMA_TXDESC_PASS_THROUGH_MODE_SET(txd, pt_val);

			edma_dmac_clean_range_no_dsb((void *)iter_skb->data,
					(void *)(iter_skb->data + buf_len));

			EDMA_TXDESC_DATA_LEN_SET(txd, buf_len);

			/*
			 * Setting up the TSO bit even for the fraglist packets apart
			 * from the head skb.
			 */
			if (unlikely(edma_tx_is_tso_eligible(skb))) {
				EDMA_TXDESC_TSO_ENABLE_SET(txd, 1);
			}

		*hw_next_to_use = (*hw_next_to_use + 1) & EDMA_TX_RING_SIZE_MASK(txdesc_ring->count);
		num_descs += 1;
		num_sg_frag_list += 1;

			/*
			 * skb fraglist skb can have nr_frags
			 */
skip_primary:
			if (unlikely(skb_shinfo(iter_skb)->nr_frags)) {
				num_nr_frag = edma_tx_skb_nr_frags(txdesc_ring, &txd, dp_dev, iter_skb, hw_next_to_use);
				num_descs += num_nr_frag;
				num_sg_frag_list += num_nr_frag;

				/*
				 * Update fraglist with nr_frag stats
				 */
				u64_stats_update_begin(&stats->syncp);
				stats->tx_fraglist_with_nr_frags_pkts++;
				u64_stats_update_end(&stats->syncp);
			}
		}

		EDMA_TXDESC_ENDIAN_SET(txd);

		/*
		 * This will be the index previous to
		 * that of current *hw_next_to_use
		 */
		end_idx = (((*hw_next_to_use) + EDMA_TX_RING_SIZE_MASK(txdesc_ring->count)) &
					EDMA_TX_RING_SIZE_MASK(txdesc_ring->count));

		/*
		 * Need to flush from initial txd to accomodate for MORE bit change.
		 * Need to flush all the descriptors but last-one that we filled as well.
		 * This may result double flush if fraglist iter_skb has nr_frags which is rare.
		 */
		if (end_idx > start_idx) {
			edma_dmac_clean_range_no_dsb((void *)(start_desc),
					((start_desc) + num_sg_frag_list));
		} else {
			edma_dmac_clean_range_no_dsb(txdesc_ring->pdesc,
					txdesc_ring->pdesc + end_idx);
			edma_dmac_clean_range_no_dsb((void *)(start_desc),
					txdesc_ring->pdesc + txdesc_ring->count);
		}

		/*
		 * Update frag_list stats
		 */
		u64_stats_update_begin(&stats->syncp);
		stats->tx_fraglist_pkts++;
		u64_stats_update_end(&stats->syncp);
	}

	edma_debug("skb:%px num_descs_filled: %u, nr_frags %u frag_list fragments %u\n",
			skb, num_descs, skb_shinfo(skb)->nr_frags, num_sg_frag_list);

	*txdesc = txd;
	return num_descs;
}

/*
 * edma_tx_avail_desc()
 *	Get available Tx descriptors
 */
static uint32_t edma_tx_avail_desc(struct edma_txdesc_ring *txdesc_ring, uint32_t hw_next_to_use)
{
	uint32_t data = 0, avail = 0, hw_next_to_clean = 0;

	data = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(txdesc_ring->id));
	hw_next_to_clean = data & EDMA_TXDESC_CONS_IDX_MASK;

	avail = EDMA_DESC_AVAIL_COUNT(hw_next_to_clean - 1, hw_next_to_use, txdesc_ring->count);

	return avail;
}

/*
 * edma_tx_phy_tstamp_buf_internal()
 *	Send skb for PHY timestamping (internal function)
 *
 * Returns true if PHY driver handles the timestamp, false otherwise.
 */
static inline bool edma_tx_phy_tstamp_buf_internal(struct net_device *ndev, struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	/*
	 * Function drv->txtstamp will create a clone of skb if necessary,
	 * the PTP_CLASS_ value 0 is passed to phy driver, which will be
	 * set to the correct PTP class value by calling ptp_classify_raw
	 * in the drv->txtstamp function.
	 */
	if (ndev && ndev->phydev && ndev->phydev->drv && ndev->phydev->drv->txtstamp) {
		ndev->phydev->drv->txtstamp(ndev->phydev, skb, 0);
		return true;
	}
#else
	if (phy_has_txtstamp(ndev->phydev)) {
		phy_txtstamp(ndev->phydev, skb, 0);
		return true;
	}
#endif
	return false;
}

/*
 * edma_tx_tstamp_buf()
 *	Wrapper function for PTP TX timestamp processing
 *	Handles both PHY and XGMAC timestamps with priority
 *
 * This function tries PHY timestamping first (higher priority).
 * If PHY doesn't handle it, XGMAC timestamping is enabled.
 */
static inline void edma_tx_tstamp_buf(struct nss_dp_dev *dp_dev, struct net_device *ndev,
				      struct sk_buff *skb, struct edma_pri_txdesc *txdesc)
{
	/*
	 * Try PHY timestamping first (higher priority)
	 * If PHY handles it, we're done
	 */
	if (edma_tx_phy_tstamp_buf_internal(ndev, skb)) {
		return;
	}

#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
	/*
	 * PHY didn't handle timestamp, enable XGMAC timestamp
	 */
	edma_tx_mac_tstamp_buf(dp_dev, skb, txdesc);
#endif
}

#ifdef CONFIG_IPQ_PON
/*
 * edma_tx_process_gem()
 *    API to process GEM TX handling for a packet.
 */
enum edma_tx edma_tx_process_gem(struct sk_buff *skb,
		struct nss_dp_gem_tx_info *gem_txi)
{
	nss_dp_gem_tx_cb_t gem_tx_cb;
	void *app_data;
	bool consumed;

	/* Initialize gem structure */
	gem_txi->src_dev       = NULL;
	gem_txi->src_dev_valid = A_FALSE;
	gem_txi->service_code  = PPE_DRV_SC_BYPASS_ALL; /* default */

	/* If there's no GEM TX callback, nothing to do—proceed as OK. */
	rcu_read_lock();
	gem_tx_cb = rcu_dereference(nss_dp_gem_tx_reg_cb_g);
	if (unlikely(!gem_tx_cb)) {
		rcu_read_unlock();
		return EDMA_TX_GEM_FAIL;
	}

	app_data = rcu_dereference(nss_dp_gem_tx_app_data_g);

	/* Allow memory debug accounting around the callback. */
	mem_debug_update_skb(skb);
	consumed = gem_tx_cb(app_data, skb, gem_txi);
	mem_debug_update_skb(skb);

	if (unlikely(consumed)) {
		rcu_read_unlock();
		return EDMA_TX_GEM_CONSUMED;
	}
	rcu_read_unlock();

	return EDMA_TX_OK;
}
#endif

/*
 * edma_tx_ring_xmit()
 *	API to transmit a packet.
 */
enum edma_tx edma_tx_ring_xmit(struct net_device *netdev, struct nss_dp_vp_tx_info *dptxi, struct sk_buff *skb,
				struct edma_txdesc_ring *txdesc_ring,
				struct edma_tx_stats *stats)
{
	struct nss_dp_dev *dp_dev = netdev_priv(netdev);
	struct edma_tx_desc_stats *txdesc_stats = &txdesc_ring->tx_desc_stats;
	uint32_t hw_next_to_use = 0, cons_idx = 0, work_to_do = 0;
	uint32_t num_tx_desc_needed = 0, num_desc_filled = 0;
	struct edma_pri_txdesc *txdesc = NULL;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	void *gem_txi = NULL;
#ifdef CONFIG_IPQ_PON
	enum edma_tx ret;
	struct nss_dp_gem_tx_info real_gem_txi = {0};
	gem_txi = &real_gem_txi;
#endif
	hw_next_to_use = txdesc_ring->prod_idx;

	mem_debug_update_skb(skb);

#ifdef NSS_DP_TX_SMALL_PACKET_WAR
	if (unlikely(skb->len < NSS_DP_EDMA_TX_MIN_PKT_SZ)) {
		if (skb_put_padto(skb, NSS_DP_EDMA_TX_MIN_PKT_SZ)) {
			/*
			 * skb_put_padto() already freed the skb via kfree_skb()
			 * on failure. Do NOT access skb after this point.
			 * Return EDMA_TX_FAIL_SKB_FREED so callers know not to
			 * free the skb again.
			 */
			edma_debug("skb padding failed to minimum length ring: %d\n",
					txdesc_ring->id);
			u64_stats_update_begin(&txdesc_stats->syncp);
			++txdesc_stats->tx_pad_fail;
			u64_stats_update_end(&txdesc_stats->syncp);
			return EDMA_TX_FAIL_SKB_FREED;
		}
	}
#endif

	if (unlikely(!(txdesc_ring->avail_desc)))  {
		txdesc_ring->avail_desc = edma_tx_avail_desc(txdesc_ring, hw_next_to_use);
		if (unlikely(!txdesc_ring->avail_desc)) {
			edma_debug("No available descriptors are present at %d ring\n",
					txdesc_ring->id);

			u64_stats_update_begin(&txdesc_stats->syncp);
			++txdesc_stats->no_desc_avail;
			u64_stats_update_end(&txdesc_stats->syncp);
			return EDMA_TX_FAIL_NO_DESC;
		}

		if (unlikely(egc->enable_ring_util_stats)) {
			cons_idx = edma_reg_read(EDMA_REG_TXDESC_CONS_IDX(txdesc_ring->id)) & EDMA_TXDESC_CONS_IDX_MASK;
			work_to_do = EDMA_DESC_AVAIL_COUNT(txdesc_ring->prod_idx, cons_idx, txdesc_ring->count);
			edma_update_ring_stats(work_to_do, txdesc_ring->count,
					       &txdesc_stats->ring_stats);
		}
	}

#ifdef CONFIG_IPQ_PON
	if (dp_dev->gem_port) {
		ret = edma_tx_process_gem(skb, gem_txi);
			if (unlikely(ret != EDMA_TX_OK))
				return ret;
	}
#endif

#if defined(CONFIG_SKB_FAST_RECYCLABLE_DEBUG_ENABLE)
	if (likely(skb->fast_xmit) && likely(skb->is_from_recycler)) {
		dev_check_skb_fast_recyclable(skb);
	}
#endif

	/*
	 * Process head skb for linear skb
	 * Process head skb + nr_frags + fraglist for non linear skb
	 */
	if (likely(!skb_is_nonlinear(skb))) {
		txdesc = edma_tx_skb_first_desc(dp_dev, txdesc_ring, dptxi, skb, &hw_next_to_use, stats, gem_txi);
		if (!txdesc) {
			return EDMA_TX_FAIL;
		}

		/* Handle PTP timestamping (PHY priority, XGMAC fallback) */
		if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
			edma_tx_tstamp_buf(dp_dev, netdev, skb, txdesc);
		}

		EDMA_TXDESC_ENDIAN_SET(txdesc);
		num_desc_filled++;

		EDMA_TXDESC_OPAQUE_SET(txdesc, skb);
		/*
		 * We set fast_recycled flag if packet has taken the
		 * SFE fast transmit path, so that any Rx DMA driver
		 * that allocates this packet later can avoid
		 * an invalidate operation
		 */
		if (likely(skb->fast_xmit) && likely(skb->is_from_recycler)) {
			skb->fast_recycled = 1;
		}
	} else {
		num_tx_desc_needed = edma_tx_num_descs_for_sg(skb);

		/*
		 * HW does not support TSO for packets with more than 32 (48 In IPQ54xx) segments.
		 * HW hangs up if it sees more than 32  (48 In IPQ54xx) segments.
		 * Kernel Perform GSO for such packets with netdev gso_max_segs set to 32 (48 In IPQ54xx).
		 */
		if (unlikely(num_tx_desc_needed > EDMA_TX_TSO_SEG_MAX)) {
			edma_debug("Number of segments %u more than %u for %d ring\n",
					num_tx_desc_needed, EDMA_TX_TSO_SEG_MAX, txdesc_ring->id);
			u64_stats_update_begin(&txdesc_stats->syncp);
			++txdesc_stats->tso_max_seg_exceed;
			u64_stats_update_end(&txdesc_stats->syncp);

			u64_stats_update_begin(&stats->syncp);
			stats->tx_tso_drop_pkts++;
			u64_stats_update_end(&stats->syncp);

			return EDMA_TX_FAIL;
		}

		if (unlikely(num_tx_desc_needed > txdesc_ring->avail_desc)) {
			txdesc_ring->avail_desc = edma_tx_avail_desc(txdesc_ring, hw_next_to_use);
			if (num_tx_desc_needed > txdesc_ring->avail_desc) {
				u64_stats_update_begin(&txdesc_stats->syncp);
				++txdesc_stats->no_desc_avail;
				u64_stats_update_end(&txdesc_stats->syncp);
				edma_debug("Not enough available descriptors are present at %d ring for SG packet. Needed %d, currently available %d\n",
					txdesc_ring->id, num_tx_desc_needed, txdesc_ring->avail_desc);
				return EDMA_TX_FAIL_NO_DESC;
			}
		}

		txdesc = edma_tx_skb_first_desc(dp_dev, txdesc_ring, dptxi, skb, &hw_next_to_use, stats, gem_txi);
		if (!txdesc) {
			return EDMA_TX_FAIL;
		}

		/* Handle PTP timestamping (PHY priority, XGMAC fallback) */
		if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
			edma_tx_tstamp_buf(dp_dev, netdev, skb, txdesc);
		}

		EDMA_TXDESC_OPAQUE_SET(txdesc, skb);

		num_desc_filled = edma_tx_skb_sg_fill_desc(dp_dev, txdesc_ring, &txdesc, skb, &hw_next_to_use, stats);
	}

	/*
	 * Set the skb pointer to the descriptor's opaque field/s
	 * on the last descriptor of the packet/SG packet.
	 */

	/*
	 * Flush the last descriptor.
	 */
	edma_dmac_clean_range_no_dsb(txdesc, txdesc + 1);

	/*
	 * Update producer index
	 */
	txdesc_ring->prod_idx = hw_next_to_use & EDMA_TXDESC_PROD_IDX_MASK;
	txdesc_ring->avail_desc -= num_desc_filled;

	edma_debug("%s: skb:%px tx_ring:%u proto:0x%x skb->len:%d\n"
			" port:%u prod_idx:%u ip_summed:0x%x\n",
			netdev->name, skb, txdesc_ring->id, ntohs(skb->protocol),
			skb->len, dp_dev->macid, hw_next_to_use, skb->ip_summed);

	/*
	 * Make sure the information written to the descriptors
	 * is updated before writing to the hardware.
	 */
	edma_dsb();

	edma_reg_write(EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id),
			txdesc_ring->prod_idx);

	u64_stats_update_begin(&stats->syncp);
	stats->tx_pkts++;
	stats->tx_bytes += skb->len;
	u64_stats_update_end(&stats->syncp);

	return EDMA_TX_OK;
}
