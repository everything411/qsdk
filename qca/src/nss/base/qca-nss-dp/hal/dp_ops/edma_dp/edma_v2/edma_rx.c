/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <asm/cacheflush.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <ppe_drv_public.h>
#include <ppe_drv_sc.h>
#include <nss_dp_vp.h>
#include <nss_dp_udp_st.h>
#include <linux/phy.h>
#include <linux/if_vlan.h>
#include <net/page_pool/helpers.h>
#include "edma.h"
#include "edma_debug.h"
#include "edma_regs.h"
#include "edma_cfg_rx.h"
#include "nss_dp_dev.h"
#include "syn_dev.h"
#include <linux/debug_mem_usage.h>

extern nss_dp_vp_rx_cb_t nss_dp_vp_rx_reg_cb;
extern struct nss_dp_vp_ctx g_vp_ctx;

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
#define EDMA_MAX_ORDER 10
#define EDMA_MAX_BULK_PAGE_ALLOC_SZ  (PAGE_SIZE *  (1 << EDMA_MAX_ORDER))
#endif

#ifdef CONFIG_DEBUG_MEM_USAGE
extern unsigned long __wrap___get_free_pages(gfp_t gfp_mask, unsigned int order);
extern void __wrap_free_pages(unsigned long addr, unsigned int order);
#endif
/*
 * Callback for udp st rx processing.
 */
nss_dp_udp_st_rx_cb_t nss_dp_udp_st_rx_cb = NULL;

/*
 * edma_rx_wifi_qos_none()
 *	Get the remaining 20-bit tree_id and process
 */
static void edma_rx_wifi_qos_none(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				  struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				  struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

/*
 * edma_rx_wifi_qos_sawf()
 *	In case of SAWF, fetch the SAWF metadata from Tree ID.
 */
static void edma_rx_wifi_qos_sawf(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				  struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				  struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

/*
 * edma_rx_wifi_qos_scs()
 *	In case of SCS, fetch the wifi_qos from Tree ID.
 */
static void edma_rx_wifi_qos_scs(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				 struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				 struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

/*
 * edma_rx_wifi_qos_wifi_tid()
 *	In case of HLOS TID OVERRIDE MODE, fetch the metadata from Tree ID.
 */
static void edma_rx_wifi_qos_wifi_tid(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				      struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				      struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

/*
 * edma_rx_wifi_qos_mlo_assist()
 *	In case of MLO, fetch the MLO metadata from Tree ID.
 */
static void edma_rx_wifi_qos_mlo_assist(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
					struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
					struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

/*
 * edma_rx_wifi_qos_udp_st()
 *	In case of UDP-ST, fetch the UDP-ST metadata from Tree ID.
 */
static void edma_rx_wifi_qos_udp_st(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				    struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				    struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec);

static edma_rx_wifi_qos_handler_t edma_rx_wifi_qos_handlers[] = {
	[PPE_DRV_TREE_ID_TYPE_NONE] = edma_rx_wifi_qos_none,		/**< PPE_DRV_TREE_ID_TYPE_NONE */
	[PPE_DRV_TREE_ID_TYPE_SAWF] = edma_rx_wifi_qos_sawf,		/**< PPE_DRV_TREE_ID_TYPE_SAWF */
	[PPE_DRV_TREE_ID_TYPE_SCS] = edma_rx_wifi_qos_scs,		/**< PPE_DRV_TREE_ID_TYPE_SCS */
	[PPE_DRV_TREE_ID_TYPE_WIFI_TID] = edma_rx_wifi_qos_wifi_tid,	/**< PPE_DRV_TREE_ID_TYPE_WIFI_TID */
	[PPE_DRV_TREE_ID_TYPE_MLO_ASSIST] = edma_rx_wifi_qos_mlo_assist,	/**< PPE_DRV_TREE_ID_TYPE_MLO_ASSIST */
	[PPE_DRV_TREE_ID_TYPE_UDP_ST] = edma_rx_wifi_qos_udp_st		/**< PPE_DRV_TREE_ID_TYPE_UDP_ST */
};

/*
 * nss_dp_udp_st_rx_register_cb()
 *	Register handler for udp st rx processing.
 */
void nss_dp_udp_st_rx_register_cb(nss_dp_udp_st_rx_cb_t cb)
{
	nss_dp_udp_st_rx_cb = cb;
	return;
}
EXPORT_SYMBOL(nss_dp_udp_st_rx_register_cb);

/*
 * edma_rx_process_capwap_vp()
 *	Forward capwap packet to VP module for processing.
 */
static inline void edma_rx_process_capwap_vp(struct nss_dp_vp_ctx *ctx, uint16_t dvp, struct sk_buff *skb)
{
	struct nss_dp_vp_node *node;
	uint16_t vpi;

	vpi = PPE_DRV_GET_VP_IDX(dvp);
	set_bit(vpi, ctx->active_vps);
	node = &ctx->nodes[vpi];

	node->info.bytes += skb->len;
	node->info.dvp = dvp;

	skb->ip_summed = CHECKSUM_COMPLETE;
	mem_debug_update_skb(skb);
	__skb_queue_tail(&node->head, skb);

	return;
}

/*
 * edma_rx_checksum_verify()
 *	get hw checksum status
 */
static inline uint8_t edma_rx_checksum_verify(struct edma_rxdesc_desc *rxdesc_desc,
							struct sk_buff* skb)
{
	uint8_t pid = EDMA_RXDESC_PID_GET(rxdesc_desc);

	skb_checksum_none_assert(skb);

	if (likely(EDMA_RX_PID_IS_IPV4(pid))) {
		if (likely(EDMA_RXDESC_L3CSUM_STATUS_GET(rxdesc_desc))
			&& likely(EDMA_RXDESC_L4CSUM_STATUS_GET(rxdesc_desc))) {
			return CHECKSUM_UNNECESSARY;
		}
	} else if (likely(EDMA_RX_PID_IS_IPV6(pid))) {
		if (likely(EDMA_RXDESC_L4CSUM_STATUS_GET(rxdesc_desc))) {
			return CHECKSUM_UNNECESSARY;
		}
	}

	return skb->ip_summed;
}

/*
 * edma_rx_process_vp()
 *	Forward packet to VP module for processing.
 */
static inline void edma_rx_process_vp(struct edma_rxdesc_desc *rxdesc_desc, struct edma_rxdesc_ring *rxdesc_ring,
			struct sk_buff *skb, struct nss_dp_vp_rx_info *vprxi_p)
{
	struct nss_dp_vp_rx_data rx_data = {
		.type = NSS_DP_VP_RX_TYPE_SKB,
		.skb = skb,
	};
	nss_dp_vp_rx_cb_t edma_rx_vp_cb;
	uint32_t dst_port;

	rcu_read_lock();

	edma_rx_vp_cb = rcu_dereference(nss_dp_vp_rx_reg_cb);
	if (unlikely(!edma_rx_vp_cb)) {
		struct edma_pcpu_stats *pcpu_stats;
		struct edma_rx_stats *rx_stats;
		struct nss_dp_dev *vp_dev;

		rcu_read_unlock();
		if (net_ratelimit()) {
			edma_warn("VP packet recieved but edma vp callback \
					not registered yet, skb:%px\n", skb);
		}

		vp_dev = netdev_priv(skb->dev);
		mem_debug_update_skb(skb);
		dev_kfree_skb_any(skb);

		pcpu_stats = &vp_dev->dp_info.pcpu_stats;
		rx_stats = this_cpu_ptr(pcpu_stats->rx_stats);
		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_vp_uninitialized++;
		u64_stats_update_end(&rx_stats->syncp);
		return;
	}

	dst_port = EDMA_RXDESC_DST_INFO_GET(rxdesc_desc);
	if (likely((dst_port & ~EDMA_RXDESC_DST_PORT_ID_MASK) == EDMA_RXDESC_DST_PORT)) {
		vprxi_p->dvp = EDMA_RXDESC_DST_PORT_ID_GET(rxdesc_desc);
	} else {
		vprxi_p->dvp = 0;
	}

	vprxi_p->l3offset = EDMA_RXDESC_L3_OFFSET_GET(rxdesc_desc);
	vprxi_p->svp = EDMA_RXDESC_SRC_INFO_GET(rxdesc_desc) & EDMA_RXDESC_PORTNUM_BITS;
	vprxi_p->napi = &rxdesc_ring->napi;
	vprxi_p->ip_summed = edma_rx_checksum_verify(rxdesc_desc, skb);
	vprxi_p->fake_mac = EDMA_RXDESC_FAKE_MAC_GET(rxdesc_desc);

	/*
	 * Pass the packet to VP to process
	 */
	mem_debug_update_skb(skb);
	edma_rx_vp_cb(&rx_data, vprxi_p);
	rcu_read_unlock();
}

#if defined(NSS_DP_EDMA_LOOPBACK_SUPPORT)
/*
 * edma_rx_free_buffer_loopback()
 *	Free list of Rx buffers to the Rx fill ring
 */
void edma_rx_free_buffer_loopback(void)
{
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	int i = 0;

	for (i = 0; i < EDMA_MAX_LOOPBACK_BUF; i++) {
		if (egc->buf_info[i].loopback_buf)
			free_pages(egc->buf_info[i].loopback_buf, egc->buf_info[i].loopback_order);
	}
}

/*
 * edma_rx_alloc_buffer_loopback()
 *	Write a given list of Rx buffers to the Rx fill ring
 */
bool edma_rx_alloc_buffer_loopback(struct edma_rxfill_ring *rxfill_ring, int alloc_count)
{
	uint32_t i, j = 0, loop_count, tot_memory, rem_tot_memory;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint32_t buf_len = rxfill_ring->buf_len;
	struct edma_rxfill_desc *rxfill_desc;
	uint32_t order, count;
	unsigned char *data;
	uint16_t prod_idx;

	/*
	 * Cache align the required buffer len
	 */
	if (buf_len % L1_CACHE_BYTES) {
		buf_len = (buf_len + L1_CACHE_BYTES) & (~(L1_CACHE_BYTES - 1));
	}

	/*
	 * Calculate the total memory and order that can be allocated in once; in kernel an max order of 10 is supported
	 * which indicates 1024 pages i.e 4MB of data
	 */
	tot_memory = buf_len * alloc_count;

	/*
	 * calculate the total loop of 4MB that will be required to fulfill this request
	 * The below might leave some delta memory in case total memory is not a multiple of 4MB
	 */
	loop_count = tot_memory / EDMA_MAX_BULK_PAGE_ALLOC_SZ;

	for (i = 0; i < loop_count; i++) {
		data = (unsigned char *)__get_free_pages(__GFP_NOWARN, EDMA_MAX_ORDER);
		if (!data) {
			edma_warn("Unable to allocate free pages for order: %d and alloc_count:%d\n", EDMA_MAX_ORDER, alloc_count);
			return false;
		}

		/*
		 * Store the order for free
		 */
		egc->buf_info[i].loopback_buf = (unsigned long)data;
		egc->buf_info[i].loopback_order = EDMA_MAX_ORDER;
	}

	/* If the tot_memory is less than EDMA_MAX_BULK_PAGE_ALLOC_SZ or remaining memroy after above allocation,
	 * then allocate here.
	 */
	rem_tot_memory = tot_memory - (loop_count * ((EDMA_MAX_BULK_PAGE_ALLOC_SZ / buf_len) * buf_len));
	if (rem_tot_memory) {
		order = get_order(rem_tot_memory);
		data = (unsigned char *)__get_free_pages(__GFP_NOWARN, order);
		if (!data) {
			edma_warn("Unable to allocate free pages for order: %d and alloc_count:%d\n", order, alloc_count);
			edma_rx_free_buffer_loopback();
			return false;
		}

		egc->buf_info[i].loopback_buf = (unsigned long)data;
		egc->buf_info[i].loopback_order = order;
		loop_count++;
	}

	/*
	 * Get RXFILL ring producer index
	 */
	prod_idx = rxfill_ring->prod_idx;

	/*
	 * Run the loop to fill buffers
	 */
	for (i = 0; i < loop_count; i++) {
		dma_addr_t buff_addr;
		data = (unsigned char *)egc->buf_info[i].loopback_buf;
		order = egc->buf_info[i].loopback_order;

		count = ((1 << order) * PAGE_SIZE) / buf_len;
		buff_addr = (dma_addr_t)virt_to_phys(data);
		for (j = 0; j < count; j++) {
			/*
			 * Last loop_count might not have to fill the entire alloc_new_count buffers; hence relying on
			 * prod_idx to break from the loop
			 */
			if (prod_idx == rxfill_ring->count)
				break;

			/*
			 * Get RXFILL descriptor
			 */
			rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, prod_idx);

			/*
			 * Map Rx buffer for DMA
			 */
			EDMA_RXFILL_BUFFER_ADDR_SET(rxfill_desc, buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
			EDMA_RXFILL_BUFFER_ADDR_HI_SET(rxfill_desc, buff_addr);
#endif

			/*
			 * there is no SKB to fill.
			 */
			EDMA_RXFILL_OPAQUE_LO_SET(rxfill_desc, NULL);
		#ifdef __LP64__
			EDMA_RXFILL_OPAQUE_HI_SET(rxfill_desc, NULL);
		#endif

			/*
			 * Save buffer size in RXFILL descriptor
			 */
			EDMA_RXFILL_PACKET_LEN_SET(rxfill_desc, ((uint32_t)(buf_len) & EDMA_RXFILL_BUF_SIZE_MASK));

			prod_idx = prod_idx + 1;

			/*
			 * Perform endianness conversion before writing to HW
			 */
			EDMA_RXFILL_ENDIAN_SET(rxfill_desc);
			buff_addr = buff_addr + buf_len;
		}
	}

	edma_info("loopback ring total memory %u loop_count %u alloc_count %u total alloc %u\n", tot_memory, loop_count, alloc_count, prod_idx);

	if (likely(j)) {
		/*
		 * Make sure the information written to the descriptors
		 * is updated before writing to the hardware.
		 */
		edma_dsb();

		edma_reg_write(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id),
								prod_idx);
		rxfill_ring->prod_idx = prod_idx;
	}

	return true;
}
#endif

/*
 * edma_rx_alloc_buffer_list()
 *	Write a given list of Rx buffers to the Rx fill ring
 */
static inline int edma_rx_alloc_buffer_list(struct edma_rxfill_ring *rxfill_ring, int reap_count)
{
	struct edma_rxfill_desc *rxfill_desc;
	struct edma_rx_fill_stats *rxfill_stats = &rxfill_ring->rx_fill_stats;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	struct list_head rx_skb_alloc;
	uint16_t prod_idx, start_idx, cons_idx;
	uint16_t num_alloc = 0, alloc_count;
	uint16_t avail_desc = 0;
	uint32_t rx_alloc_size = rxfill_ring->alloc_size;
	uint32_t buf_len = rxfill_ring->buf_len;
	bool page_mode = rxfill_ring->page_mode;
	INIT_LIST_HEAD(&rx_skb_alloc);

	/*
	 * Get RXFILL ring producer index
	 */
	prod_idx = rxfill_ring->prod_idx;
	start_idx = prod_idx;

	/*
	 * When tracking ring util stats is enabled via procfs,
	 * we will compute avail desc and compute how much percentage the ring is full.
	 * Above stats are maintained at ring level.
	 */
	if (unlikely(egc->enable_ring_util_stats)) {
		cons_idx = edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id)) & EDMA_RXFILL_CONS_IDX_MASK;
		avail_desc = EDMA_DESC_AVAIL_COUNT(cons_idx, prod_idx, EDMA_RX_RING_SIZE);

		edma_update_ring_stats(avail_desc, EDMA_RX_RING_SIZE,
				       &rxfill_ring->rx_fill_stats.ring_stats);
	}

	rxfill_ring->num_rxfill_pending += reap_count;
	alloc_count = rxfill_ring->num_rxfill_pending;
	while (likely(alloc_count--)) {
		struct sk_buff *skb_alloc;

		/*
		 * Allocate one skb and add to refill list
		 */
		skb_alloc = netdev_alloc_skb_fast(NULL, rx_alloc_size);
		if (likely(skb_alloc)) {
			mem_debug_update_skb(skb_alloc);
			list_add_tail(&skb_alloc->list, &rx_skb_alloc);
			num_alloc++;
		} else {
			u64_stats_update_begin(&rxfill_stats->syncp);
			++rxfill_stats->alloc_failed;
			u64_stats_update_end(&rxfill_stats->syncp);
		}
	}

	rxfill_ring->num_rxfill_pending -= num_alloc;

	while (likely(!list_empty(&rx_skb_alloc))) {
		void *page_addr = NULL;
		struct page *pg;
		struct sk_buff *skb;
		dma_addr_t buff_addr;

		/*
		 * Detach the current SKB to use from the list,
		 * and prefetch the next SKB's cache lines.
		 */
		skb = list_first_entry(&rx_skb_alloc, struct sk_buff, list);
		if (likely(!list_entry_is_head(skb->next, &rx_skb_alloc, list))) {
			prefetch(skb->next);
			prefetch((uint8_t *)(skb->next) + 128);
			prefetch((uint8_t *)(skb->next) + 192);
		}
		list_del_init(&skb->list);
		skb->next = skb->prev = NULL;

		/*
		 * Reserve headroom
		 */
		skb_reserve(skb, EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN);

		/*
		 * Map Rx buffer for DMA
		 */
		if (likely(!page_mode)) {
			buff_addr = (dma_addr_t)virt_to_phys(skb->data);
		} else {
			pg = alloc_page(GFP_ATOMIC);
			if (unlikely(!pg)) {
				u64_stats_update_begin(&rxfill_stats->syncp);
				++rxfill_stats->page_alloc_failed;
				u64_stats_update_end(&rxfill_stats->syncp);
				mem_debug_update_skb(skb);
				dev_kfree_skb_any(skb);
				edma_debug("edma_gbl_ctx:%px Unable to allocate page", edma_gbl_ctx);
				break;
			}

			/*
			 * Get virtual address of allocated page
			 */
			page_addr = page_address(pg);
			buff_addr = (dma_addr_t)virt_to_phys(page_addr);
			skb_fill_page_desc(skb, 0, pg, 0, PAGE_SIZE);
			edma_dmac_inv_range_no_dsb(page_addr, (page_addr + PAGE_SIZE));
		}

		/*
		 * Get RXFILL descriptor
		 */
		rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, prod_idx);
#ifdef CONFIG_IO_COHERENCY
		/*
		 * With CONFIG_IO_COHERENCY, the Rxfill descriptors are cacheable.
		 * Prefetch the Rxfill descriptor.
		 */
		prefetchw(rxfill_desc);
#endif

		/*
		 * Set up Buffer high address.
		 */
		EDMA_RXFILL_BUFFER_ADDR_SET(rxfill_desc, buff_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
		EDMA_RXFILL_BUFFER_ADDR_HI_SET(rxfill_desc, buff_addr);
#endif

		/*
		 * Store skb in opaque
		 */
		EDMA_RXFILL_OPAQUE_LO_SET(rxfill_desc, skb);
#ifdef __LP64__
		EDMA_RXFILL_OPAQUE_HI_SET(rxfill_desc, skb);
#endif

		/*
		 * Save buffer size in RXFILL descriptor
		 */
		EDMA_RXFILL_PACKET_LEN_SET(rxfill_desc, ((uint32_t)(buf_len) & EDMA_RXFILL_BUF_SIZE_MASK));

		/*
		 * Perform endianness conversion before writing to HW
		 */
		EDMA_RXFILL_ENDIAN_SET(rxfill_desc);

		/*
		 * Invalidate skb->data
		 * A73 flush operation does an invalidate operation as well.
		 * If the packet is fast transmitted and hence fast recycled,
		 * we can be assured that invalidate was already done at the
		 * time of previous transmit
		 */
		if (unlikely(!skb->fast_recycled)) {
			edma_dmac_inv_range_no_dsb((void *)skb->data,
					      (void *)(skb->data + rx_alloc_size -
					      EDMA_RX_SKB_HEADROOM -
					      NET_IP_ALIGN));

		}
		skb->fast_recycled = 0;

		prod_idx = (prod_idx + 1) & EDMA_RX_RING_SIZE_MASK;
	}

	if (likely(num_alloc)) {

		/*
		 * Make sure the information written to the descriptors
		 * is updated before writing to the hardware.
		 */
		edma_dsb();

		edma_reg_write(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id),
								prod_idx);
		rxfill_ring->prod_idx = prod_idx;
	}

	return num_alloc;
}

#ifdef EDMA_ALLOC_PAGE_POOL_MODE
/*
 * edma_rx_alloc_pages()
 *	Write a given list of Rx buffers to the Rx fill ring
 */
static inline int edma_rx_alloc_pages(struct edma_rxfill_ring *rxfill_ring, int reap_count)
{
	struct edma_rxfill_desc *rxfill_desc;
	struct edma_rx_fill_stats *rxfill_stats = &rxfill_ring->rx_fill_stats;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	uint16_t prod_idx, start_idx, cons_idx;
	uint16_t num_alloc = 0, alloc_count;
	uint16_t avail_desc = 0;
	uint32_t rx_alloc_size = rxfill_ring->alloc_size;
	uint32_t buf_len = rxfill_ring->buf_len;

	/*
	 * Get RXFILL ring producer index
	 */
	prod_idx = rxfill_ring->prod_idx;
	start_idx = prod_idx;

	/*
	 * When tracking ring util stats is enabled via procfs,
	 * we will compute avail desc and compute how much percentage the ring is full.
	 * Above stats are maintained at ring level.
	 */
	if (unlikely(egc->enable_ring_util_stats)) {
		cons_idx = edma_reg_read(EDMA_REG_RXFILL_CONS_IDX(rxfill_ring->ring_id)) & EDMA_RXFILL_CONS_IDX_MASK;
		avail_desc = EDMA_DESC_AVAIL_COUNT(cons_idx, prod_idx, EDMA_RX_RING_SIZE);

		edma_update_ring_stats(avail_desc, EDMA_RX_RING_SIZE,
				&rxfill_ring->rx_fill_stats.ring_stats);
	}

	rxfill_ring->num_rxfill_pending += reap_count;
	alloc_count = rxfill_ring->num_rxfill_pending;

	while (likely(alloc_count--)) {
		dma_addr_t data_addr;
		struct page *page;
		void *opaque;
		void *buff;
		int offset;

		/*
		 * Allocate fragment from page pool.
		 * TODO: If this is multi-order allocation then on failure we can do slow allocation using kmem.
		 */
		page = page_pool_dev_alloc_frag(rxfill_ring->page_pool, &offset, rx_alloc_size);
		if (!page) {
			u64_stats_update_begin(&rxfill_stats->syncp);
			++rxfill_stats->alloc_failed;
			u64_stats_update_end(&rxfill_stats->syncp);
			goto done;
		}

		buff = page_address(page) + offset;

		/*
		 * Reserve headroom
		 */
		data_addr = (dma_addr_t)virt_to_phys(buff + EDMA_RX_SKB_HEADROOM + NET_IP_ALIGN);
		opaque = buff;

		/*
		 * Get RXFILL descriptor
		 */
		rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, prod_idx);
#ifdef CONFIG_IO_COHERENCY
		/*
		 * With CONFIG_IO_COHERENCY, the Rxfill descriptors are cacheable.
		 * Prefetch the Rxfill descriptor.
		 */
		prefetchw(rxfill_desc);
#endif

		/*
		 * Set up Buffer high address.
		 */
		EDMA_RXFILL_BUFFER_ADDR_SET(rxfill_desc, data_addr);
#if defined(NSS_DP_HIGHMEM_SUPP)
		EDMA_RXFILL_BUFFER_ADDR_HI_SET(rxfill_desc, data_addr);
#endif

		/*
		 * Store buffer in opaque
		 */
		EDMA_RXFILL_OPAQUE_LO_SET(rxfill_desc, opaque);
#ifdef __LP64__
		EDMA_RXFILL_OPAQUE_HI_SET(rxfill_desc, opaque);
#endif

		/*
		 * Save buffer size in RXFILL descriptor
		 */
		EDMA_RXFILL_PACKET_LEN_SET(rxfill_desc, ((uint32_t)(buf_len) & EDMA_RXFILL_BUF_SIZE_MASK));

		/*
		 * Perform endianness conversion before writing to HW
		 */
		EDMA_RXFILL_ENDIAN_SET(rxfill_desc);

		prod_idx = (prod_idx + 1) & EDMA_RX_RING_SIZE_MASK;
		num_alloc++;
	}

done:
	if (likely(num_alloc)) {

		/*
		 * Make sure the information written to the descriptors
		 * is updated before writing to the hardware.
		 */
		edma_dsb();

		edma_reg_write(EDMA_REG_RXFILL_PROD_IDX(rxfill_ring->ring_id),
				prod_idx);
		rxfill_ring->prod_idx = prod_idx;
		rxfill_ring->num_rxfill_pending -= num_alloc;
	}

	return num_alloc;
}
#endif

/*
 * edma_rx_alloc_buffer()
 *	Alloc Rx buffers for one RxFill ring
 */
int edma_rx_alloc_buffer(struct edma_rxfill_ring *rxfill_ring, int alloc_count)
{
	return edma_rx_alloc_buffer_list(rxfill_ring, alloc_count);
}

/*
 * edma_rx_sawf_sc_stats_update()
 *	Update per service-class stats.
 */
static inline void edma_rx_sawf_sc_stats_update(uint64_t pkt_length, struct edma_sawf_sc_stats *sawf_sc_stats)
{
	u64_stats_update_begin(&sawf_sc_stats->syncp);
	sawf_sc_stats->rx_bytes += pkt_length;
	sawf_sc_stats->rx_packets++;
	u64_stats_update_end(&sawf_sc_stats->syncp);
}

/*
 * edma_rx_wifi_qos_none()
 *	Get the remaining 20-bit tree_id and process
 */
static void edma_rx_wifi_qos_none(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				  struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				  struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec)
{
	/*
	 * TODO: Get the remaining 20-bit tree_id and process.
	 */
}

/*
 * edma_rx_wifi_qos_sawf()
 *	In case of SAWF, fetch the SAWF metadata from Tree ID.
 */
static void edma_rx_wifi_qos_sawf(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				  struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				  struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec)
{
	uint32_t sawf_mark;
	uint8_t wifi_qos;
#ifdef NSS_DP_EDMA_FLOW_COOKIE_SUPPORT
	/*
	 * Lower 16 bit is obtained from primary desc and
	 * upper 2 bits from secondary desc
	 */
	sawf_mark = EDMA_RXDESC_FLOW_COOKIE_GET(rxdesc_head);
	sawf_mark |= (EDMA_RXDESC_TREE_ID_GET(rxdesc_sec) & 0x3) << 16;
#else
	sawf_mark = EDMA_RXDESC_SAWF_MARK_GET(rxdesc_sec);
#endif
	wifi_qos = EDMA_RXDESC_WIFI_QOS_GET(rxdesc_head);

	/*
	 * Configure skb->mark with SAWF metadata.
	 */
	skb->mark = EDMA_RX_SAWF_METADATA_CONSTRUCT(sawf_mark, wifi_qos);
	edma_debug("%px : SAWF mark configured = 0x%x\n", egc, skb->mark);
}

/*
 * edma_rx_wifi_qos_scs()
 *	In case of SCS, fetch the wifi_qos from Tree ID.
 */
static void edma_rx_wifi_qos_scs(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				 struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				 struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec)
{
	uint8_t wifi_qos = EDMA_RXDESC_WIFI_QOS_GET(rxdesc_head);

	/*
	 * Configure skb->mark with wifi_qos metadata.
	 */
	skb->mark = wifi_qos;
	edma_debug("%px : SCS mark configured = 0x%x\n", egc, skb->mark);
}

/*
 * edma_rx_wifi_qos_wifi_tid()
 *	In case of HLOS TID OVERRIDE MODE, fetch the metadata from Tree ID.
 */
static void edma_rx_wifi_qos_wifi_tid(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				      struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				      struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec)
{
	uint8_t wifi_qos = EDMA_RXDESC_WIFI_QOS_GET(rxdesc_head);

	/*
	 * Configure skb->skb_priority with metadata.
	 */
	skb->priority = wifi_qos;
	edma_debug("%px : HLOS TID OVERRIDE priority configured = 0x%d\n", egc, skb->priority);
}

/*
 * edma_rx_wifi_qos_mlo_assist()
 *	In case of MLO, fetch the MLO metadata from Tree ID.
 */
static void edma_rx_wifi_qos_mlo_assist(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
					struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
					struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec)
{
	uint32_t mlo_mark;
	uint8_t wifi_qos = EDMA_RXDESC_WIFI_QOS_GET(rxdesc_head);
#ifdef NSS_DP_EDMA_FLOW_COOKIE_SUPPORT
	/*
	 * Lower 16 bit is obtained from primary desc and
	 * upper 2 bits from secondary desc
	 */
	mlo_mark = EDMA_RXDESC_FLOW_COOKIE_GET(rxdesc_head);
	mlo_mark |= (EDMA_RXDESC_TREE_ID_GET(rxdesc_sec) & 0x3) << 16;
#else
	mlo_mark = EDMA_RXDESC_MLO_MARK_GET(rxdesc_sec);
#endif

	/*
	 * Configure skb->mark with MLO metadata.
	 */
	skb->mark = EDMA_RX_MLO_METADATA_CONSTRUCT(mlo_mark, wifi_qos);
	edma_debug("%px : mlo mark configured = 0x%x\n", egc, skb->mark);
}

/*
 * edma_rx_wifi_qos_udp_st()
 *	In case of UDP-ST, fetch the UDP-ST metadata from Tree ID.
 */
static void edma_rx_wifi_qos_udp_st(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring,
				    struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb,
				    struct nss_dp_vp_rx_info *vprxi_p, struct edma_rxdesc_sec_desc *rxdesc_sec)
{
	uint32_t udp_st_mark = EDMA_RXDESC_UDP_ST_MARK_GET(rxdesc_sec);

	/*
	 * Configure skb->mark with UDP-ST metadata.
	 */
	skb->mark = EDMA_RX_UDP_ST_METADATA_CONSTRUCT(udp_st_mark);
	edma_debug("%px : udp-st mark configured = 0x%x\n", egc, skb->mark);
}

/*
 * edma_rx_handle_wifi_qos_packets()
 *	Handle packets with wifi qos enabled.
 */
static void edma_rx_handle_wifi_qos_packets(struct edma_gbl_ctx *egc, struct edma_rxdesc_ring *rxdesc_ring, struct edma_rxdesc_desc *rxdesc_head, struct sk_buff *skb, struct nss_dp_vp_rx_info *vprxi_p)
{
	uint16_t desc_index, next_desc_index;
	struct edma_rxdesc_sec_desc *rxdesc_sec, *next_rxdesc_sec;
	ppe_drv_tree_id_type_t tree_id_type;

	desc_index = ((uint8_t *)rxdesc_head - (uint8_t *)rxdesc_ring->pdesc) >> EDMA_RXDESC_SIZE_SHIFT;
	rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, desc_index);

	/*
	 * Depending on the use-case, sometime PPE generate the same CPU
	 * code for every packet, prefetch the next secondary descriptor
	 * to handle such cases.
	 */
	next_desc_index = (desc_index + 1) & EDMA_RX_RING_SIZE_MASK;
	next_rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, next_desc_index);
	prefetch(next_rxdesc_sec);

	tree_id_type = EDMA_RXDESC_TREE_ID_TYPE_GET(rxdesc_sec);

	/*
	 * Fetch the flow index of the packet if it is valid
	 */
	if (EDMA_RX_SDESC_FLOW_IDX_VALID_GET(rxdesc_sec)) {
		vprxi_p->flow_idx = EDMA_RX_SDESC_FLOW_IDX_GET(rxdesc_sec);
	}

	/*
	 * Set the flag if Qdisc valid bit is set in the tree-id field
	 */
	vprxi_p->qdisc_valid = !!EDMA_RXDESC_HOST_QDISC_VALID_GET(rxdesc_sec);

	if (tree_id_type < ARRAY_SIZE(edma_rx_wifi_qos_handlers)) {
		edma_rx_wifi_qos_handlers[tree_id_type](egc, rxdesc_ring, rxdesc_head, skb, vprxi_p, rxdesc_sec);
	} else {
		edma_debug("%p : Invalid tree-id type = %u\n", egc, tree_id_type);
	}
}

/*
 * edma_rx_handle_host_qdisc_packets()
 *	Handle packet which needs qdisc processing in host.
 */
static bool edma_rx_handle_host_qdisc_packets(struct edma_rxdesc_ring *rxdesc_ring,
		struct edma_rxdesc_desc *rxdesc_head,
		struct sk_buff *skb)
{
	struct edma_rxdesc_sec_desc *rxdesc_sec;
	struct net_device *bottom_dev = NULL;
	struct net_device *qdisc_dev = NULL;
	struct edma_pcpu_stats *pcpu_stats;
	struct edma_rx_stats *rx_stats;
	bool flow_idx_valid = false;
	struct nss_dp_dev *dp_dev;
	struct ethhdr *ethh;
	uint16_t desc_index;
	uint16_t l3offset;
	uint32_t dst_port;
	int32_t flow_idx;
	uint8_t flags;

	/*
	 * Get stats for the netdevice
	 */
	dp_dev = netdev_priv(skb->dev);
	pcpu_stats = &dp_dev->dp_info.pcpu_stats;
	rx_stats = this_cpu_ptr(pcpu_stats->rx_stats);

	desc_index = ((uint8_t *)rxdesc_head - (uint8_t *)rxdesc_ring->pdesc) >> EDMA_RXDESC_SIZE_SHIFT;
	rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, desc_index);

	flow_idx_valid = EDMA_RX_SDESC_FLOW_IDX_VALID_GET(rxdesc_sec);
	if (unlikely(!flow_idx_valid)) {
		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_get_host_qdisc_dev_fail++;
		u64_stats_update_end(&rx_stats->syncp);
		edma_debug("Flow index is not valid\n");
		goto qdisc_drop;
	}

	/*
	 * Qdisc metadata is RCU protected in ppe driver.
	 */
	rcu_read_lock();

	flow_idx = EDMA_RX_SDESC_FLOW_IDX_GET(rxdesc_sec);
	flags = ppe_drv_get_qdisc_rule_flag(flow_idx);

	qdisc_dev = ppe_drv_get_and_hold_qdisc_netdev(flow_idx);
	if (unlikely(!qdisc_dev)) {
		rcu_read_unlock();
		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_get_host_qdisc_dev_fail++;
		u64_stats_update_end(&rx_stats->syncp);
		edma_debug("Qdisc netdevice not found, flag: 0x%X\n", flags);
		goto qdisc_drop;
	}

	/*
	 * Use bottom dev as qdisc netdevice if Qdisc is configured on bottom
	 * interface, otherwise find the bottom dev from the destination port
	 * info in EDMA descriptor.
	 */
	bottom_dev = qdisc_dev;
	if (unlikely(flags & PPE_DRV_HOST_QDISC_ON_NON_BOTTOM_IFACE)) {
		dst_port = EDMA_RXDESC_DST_INFO_GET(rxdesc_head);
		bottom_dev = ppe_drv_port_num_to_dev(dst_port);
		if (unlikely(!bottom_dev)) {
			dev_put(qdisc_dev);
			rcu_read_unlock();
			u64_stats_update_begin(&rx_stats->syncp);
			rx_stats->rx_get_host_qdisc_dev_fail++;
			u64_stats_update_end(&rx_stats->syncp);
			edma_debug("Bottom netdevice not found for host assisted qdisc flow: 0%X\n", dst_port);
			goto qdisc_drop;
		}
	}

	rcu_read_unlock();

	/*
	 * Update skb fields before sending for Qdisc processing.
	 */
	ethh = (struct ethhdr *)skb->data;
	l3offset = EDMA_RXDESC_L3_OFFSET_GET(rxdesc_head);

	skb_reset_mac_header(skb);
	skb_set_network_header(skb, l3offset);
	skb->protocol = ethh->h_proto;
	skb->dev = qdisc_dev;
	skb->priority = ppe_drv_get_qos_tag(flow_idx);
	skb->fast_forwarded = 1;

	if (likely(dev_fast_xmit_qdisc(skb, qdisc_dev, bottom_dev))) {
		dev_put(qdisc_dev);
		return true;
	}

	dev_put(qdisc_dev);

	/*
	 * Update failure stats.
	 */
	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_host_qdisc_xmit_fail++;
	u64_stats_update_end(&rx_stats->syncp);

	/*
	 * Drop the packet as its modified by PPE and we should not send it back to stack.
	 */
qdisc_drop:
	mem_debug_update_skb(skb);
	dev_kfree_skb_any(skb);
	return true;
}

/*
 * edma_rx_handle_sc_cc_packets()
 *	Handle packets with service code or CPU code.
 *
 * NOTE: We give higher priority to CPU code, since they are related
 * to exception and required to flush an existing decelerated flow by
 * hardware.
 */
static inline bool edma_rx_handle_sc_cc_packets(struct edma_gbl_ctx *egc,
		struct edma_rxdesc_ring *rxdesc_ring,
		struct edma_rxdesc_desc *rxdesc_head,
		struct sk_buff *skb)
{
	uint16_t desc_index, next_desc_index;
	uint32_t dst_port;
	uint32_t src_port;
	uint8_t cpu_code, service_code;
	bool cpu_code_valid, is_ptp_sc, acl_info_valid = false;
	struct edma_rxdesc_sec_desc *rxdesc_sec, *next_rxdesc_sec;
	struct ppe_drv_cc_metadata cc_info = {0};
	struct ppe_drv_sc_metadata sc_info = {0};
	struct ppe_drv_acl_metadata acl_info = {0};

	cpu_code_valid = EDMA_RXDESC_CPU_CODE_VALID_GET(rxdesc_head);
	service_code = EDMA_RXDESC_SERVICE_CODE_GET(rxdesc_head);
	is_ptp_sc = (service_code == PPE_DRV_SC_PTP);

	if (likely(cpu_code_valid) || unlikely(is_ptp_sc)) {
		desc_index = ((uint8_t *)rxdesc_head - (uint8_t *)rxdesc_ring->pdesc) >> EDMA_RXDESC_SIZE_SHIFT;
		rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, desc_index);

		/*
		 * Depending on the use-case, sometime PPE generate the same CPU
		 * code for every packet, prefetch the next secondary descriptor
		 * to handle such cases.
		 */
		next_desc_index = (desc_index + 1) & EDMA_RX_RING_SIZE_MASK;
		next_rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, next_desc_index);
		prefetch(next_rxdesc_sec);
	}

	/*
	 * The primary descriptor has CPU code valid indication bit while
	 * the CPU code is available in secondary descriptor.
	 */
	if (likely(cpu_code_valid)) {
		cpu_code = EDMA_RXDESC_CPU_CODE_GET(rxdesc_sec);

		/*
		 * Get the ACL id from EDMA secondary descriptor as well.
		 */
		if (unlikely(EDMA_RXDESC_ACL_IDX_VALID_GET(rxdesc_sec))) {
			acl_info.acl_hw_index = EDMA_RXDESC_ACL_IDX_GET(rxdesc_sec);
			acl_info.cpu_code = cpu_code;
			acl_info_valid = true;
			cc_info.acl_hw_index = acl_info.acl_hw_index;
			cc_info.acl_index_valid = true;
		}

		cc_info.cpu_code = cpu_code;
		cc_info.fake_mac = EDMA_RXDESC_FAKE_MAC_GET(rxdesc_head);

		src_port = EDMA_RXDESC_SRC_INFO_GET(rxdesc_head);
		if (likely(((src_port & EDMA_RXDESC_SRCINFO_TYPE_MASK) == EDMA_RXDESC_SRCINFO_TYPE_PORTID) &&
				(EDMA_RXDESC_PORT_ID_GET(src_port) & EDMA_RXDESC_VP_PORT_MASK))) {
			cc_info.src_vp_num = EDMA_RXDESC_SRC_PORT_ID_GET(rxdesc_head);
			cc_info.is_src_vp = true;
		}

		if (cpu_code && ppe_drv_cc_process_skbuff(&cc_info, skb)) {
			return true;
		}
	}

	/*
	 * Process if there is any service code.
	 */
	if (likely(service_code)) {
#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
		if (unlikely(is_ptp_sc)) {
			if (likely(EDMA_RX_SDESC_TSTAMP_VALID_GET(rxdesc_sec))) {
				/* Extract timestamp from the second descriptor */
				sc_info.ts_nsec = EDMA_RX_SDESC_TSTAMP_LO_GET(rxdesc_sec);  /* 32-bit nanoseconds */
				sc_info.ts_sec = EDMA_RX_SDESC_TSTAMP_HI_GET(rxdesc_sec);  /* 8-bit seconds */
			}

		}
#endif
		/*
		 * Check if this is host assisted Qdisc flow.
		 * Do not process the exception packet here for host assisted qdisc flow,
		 * this is required for updating the PPE rule in reverse direction.
		 */
		if (unlikely((service_code == PPE_DRV_SC_HOST_QOS) && !cpu_code_valid)) {
			return edma_rx_handle_host_qdisc_packets(rxdesc_ring, rxdesc_head, skb);
		}

		/*
		 * Fill the service code metadata structure.
		 */
		sc_info.service_code = service_code;
		dst_port = EDMA_RXDESC_DST_INFO_GET(rxdesc_head);
		if (likely(((dst_port & ~EDMA_RXDESC_DST_PORT_ID_MASK) == EDMA_RXDESC_DST_PORT) &&
				(EDMA_RXDESC_PORT_ID_GET(dst_port) & EDMA_RXDESC_VP_PORT_MASK))) {
			sc_info.vp_num = EDMA_RXDESC_DST_PORT_ID_GET(rxdesc_head);
		}

		/*
		 * Service codes can return true / false based on the callbacks registered to them.
		 */
		if (unlikely(ppe_drv_sc_process_skbuff(&sc_info, skb))) {
			return true;
		}
	}

	/*
	 * check if the ACL ID is valid or not, if yes,
	 * process the packet based on ACL ID post CPU and service code
	 * processing is done.
	 */
	if (unlikely(acl_info_valid)) {
		if (ppe_drv_acl_process_skbuff(&acl_info, skb)) {
			return true;
		}
	}

	return false;
}

/*
 * edma_rx_insert_vlan()
 *	API to insert the VLAN tag on the Rx Packet.
 *
 * We use the following API to insert the VLAN header
 * onto the desired packets of interest
 * with the relevant header details as passed
 * from the sysctl cmd.
 */
static int edma_rx_insert_vlan(struct nss_dp_dev *dp_dev, struct sk_buff *skb)
{
	struct ethhdr *eth_hdr;
	__be16 eth_type;

	eth_hdr = (struct ethhdr *)skb->data;
	eth_type = eth_hdr->h_proto;

	if (!eth_type || ((eth_type != dp_dev->vlan_info.ether_types[0]) &&
		(eth_type != dp_dev->vlan_info.ether_types[1]))) {
		return -1;
	}

	skb_push(skb, VLAN_HLEN);
	memmove(skb->data, skb->data + VLAN_HLEN, ETH_ALEN * 2);

	*(__be32 *)(skb->data + ETH_ALEN * 2) = dp_dev->vlan_info.vlan_tag_info;

	return 0;
}

/*
 * edma_rx_dp_extension_process()
 *	API to perform the extended jobs
 *	on the Rx Packet.
 *
 * We use the following API to perform various
 * functionalities on the desired packets of interest
 * with the relevant header details as passed
 * from the sysctl cmd.
 */
static int edma_rx_dp_extension_process(struct nss_dp_dev *dp_dev, struct sk_buff *skb)
{
	if(unlikely(dp_dev->vlan_info.vlan_en)) {
		return edma_rx_insert_vlan(dp_dev, skb);
	}

	return -1;
}

/*
 * edma_rx_handle_scatter_frames()
 *	Handle scattered packets in Rx direction
 *
 * This function should free the SKB in case of failure.
 */
static void edma_rx_handle_scatter_frames(struct edma_gbl_ctx *egc,
		struct edma_rxdesc_ring *rxdesc_ring,
		struct edma_rxdesc_desc *rxdesc_desc,
		struct sk_buff *skb)
{
	uint16_t desc_index;
        struct edma_rxdesc_sec_desc *rxdesc_sec;
	struct nss_dp_dev *dp_dev;
	struct edma_pcpu_stats *pcpu_stats;
	struct edma_rx_stats *rx_stats;
	struct sk_buff *skb_head;
	struct net_device *dev;
	struct nss_dp_vp_rx_info vprxi = {0};
	uint32_t pkt_length;
	skb_frag_t *frag = NULL;
	bool page_mode = rxdesc_ring->rxfill->page_mode;
	ppe_drv_tree_id_type_t tree_id_type;

	/*
	 * Get packet length
	 */
	pkt_length = EDMA_RXDESC_PACKET_LEN_GET(rxdesc_desc);
	edma_debug("edma_gbl_ctx:%px skb:%px fragment pkt_length:%u\n", egc, skb, pkt_length);

	/*
	 * For fraglist case
	 */
	if (likely(!page_mode)) {

		/*
		 * Invalidate the buffer received from the HW
		 */
		edma_dmac_inv_range((void *)skb->data,
				(void *)(skb->data + pkt_length));

		if (!(rxdesc_ring->head)) {
			skb_put(skb, pkt_length);
			rxdesc_ring->head = skb;
			rxdesc_ring->last = NULL;
			rxdesc_ring->pdesc_head = rxdesc_desc;
			/*
			 * TODO: It is safer to save the descriptor value here instead of the pointer,
			 * since descriptor may be overwritten by HW after function returns.
			 */
			return;
		}

		/*
		 * If head is present and got next desc.
		 * Append it to the fraglist of head if this is second frame
		 * If not second frame append to tail
		 */
		skb_put(skb, pkt_length);
		if (!skb_has_frag_list(rxdesc_ring->head)) {
			skb_shinfo(rxdesc_ring->head)->frag_list = skb;
		} else {
			rxdesc_ring->last->next = skb;
		}

		rxdesc_ring->last = skb;
		rxdesc_ring->last->next = NULL;
		rxdesc_ring->head->len += pkt_length;
		rxdesc_ring->head->data_len += pkt_length;
		rxdesc_ring->head->truesize += skb->truesize;

		goto process_next_scatter;
	}

	/*
	 * Manage fragments for page mode
	 */
	frag = &skb_shinfo(skb)->frags[0];
	edma_dmac_inv_range((void *)skb_frag_page(frag), (void *)(skb_frag_page(frag) + pkt_length));

	if (!(rxdesc_ring->head)) {
		skb->len = pkt_length;
		skb->data_len = pkt_length;
		skb->truesize = SKB_TRUESIZE(PAGE_SIZE);
		rxdesc_ring->head = skb;
		rxdesc_ring->last = NULL;
		rxdesc_ring->pdesc_head = rxdesc_desc;
		return;
	}

	/*
	 * Append current frag at correct index as nr_frag of parent
	 */
	skb_add_rx_frag(rxdesc_ring->head, skb_shinfo(rxdesc_ring->head)->nr_frags,
			skb_frag_page(frag), 0, pkt_length, PAGE_SIZE);
	skb_shinfo(skb)->nr_frags = 0;

	/*
	 * Free the SKB after we have appended its frag page to the head skb
	 */
	mem_debug_update_skb(skb);
	dev_kfree_skb_any(skb);

process_next_scatter:
	/*
	 * If there are more segments for this packet,
	 * then we have nothing to do. Otherwise process
	 * last segment and send packet to stack
	 */
	if (EDMA_RXDESC_MORE_BIT_GET(rxdesc_desc)) {
		return;
	}

	skb_head = rxdesc_ring->head;
	dev = skb_head->dev;

	/*
	 * Check Rx checksum offload status.
	 */
	if (likely(dev->features & NETIF_F_RXCSUM)) {
		skb_head->ip_summed = edma_rx_checksum_verify(rxdesc_desc, skb_head);
	}

	/*
	 * Get stats for the netdevice
	 */
	dp_dev = netdev_priv(dev);
	pcpu_stats = &dp_dev->dp_info.pcpu_stats;
	rx_stats = this_cpu_ptr(pcpu_stats->rx_stats);

	if (unlikely(page_mode)) {
		if (unlikely(!pskb_may_pull(skb_head, ETH_HLEN))) {
			/*
			 * Discard the SKB that we have been building,
			 * in addition to the SKB linked to current descriptor.
			 */
			mem_debug_update_skb(skb_head);
			dev_kfree_skb_any(skb_head);
			rxdesc_ring->head = NULL;
			rxdesc_ring->last = NULL;
			rxdesc_ring->pdesc_head = NULL;

			u64_stats_update_begin(&rx_stats->syncp);
			rx_stats->rx_nr_frag_headroom_err++;
			u64_stats_update_end(&rx_stats->syncp);

			return;
		}
	}

	/*
	 * In some cases like PPE tunnel when mode 1 is enabled
	 * the skb data will be pointing to outer header and if
	 * packet decap is successful then data offset will point
	 * to inner payload.
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (unlikely(!__pskb_pull(skb_head, EDMA_RXDESC_DATA_OFFSET_GET(rxdesc_ring->pdesc_head)))) {
#else
	if (unlikely(!pskb_pull(skb_head, EDMA_RXDESC_DATA_OFFSET_GET(rxdesc_ring->pdesc_head)))) {
#endif
		/*
		 * Discard the SKB that we have been building,
		 * in addition to the SKB linked to current descriptor.
		 */
		mem_debug_update_skb(skb_head);
		dev_kfree_skb_any(skb_head);
		rxdesc_ring->head = NULL;
		rxdesc_ring->last = NULL;
		rxdesc_ring->pdesc_head = NULL;

		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_nr_frag_headroom_err++;
		u64_stats_update_end(&rx_stats->syncp);

		return;
	}

	/*
	 * TODO: Do a batched update of the stats per netdevice.
	 */
	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_pkts++;
	rx_stats->rx_bytes += skb_head->len;
	rx_stats->rx_nr_frag_pkts += (uint64_t)page_mode;
	rx_stats->rx_fraglist_pkts += (uint64_t)(!page_mode);
	u64_stats_update_end(&rx_stats->syncp);

	edma_debug("edma_gbl_ctx:%px skb:%px Jumbo pkt_length:%u\n", egc, skb_head, skb_head->len);

	/*
	 * Check if primary descriptor is not NULL.
	 */
	if (likely(rxdesc_ring->pdesc_head)) {
		/*
		 * NOTE:
		 * 1. We are combining both the checks together here to reduce
		 *    a branch instruction in regular data path processing.
		 *
		 * 2. If the service code/cpu code processing consumes the packet,
		 *    don't send it to stack otherwise continue with regular processing.
		 */
		struct edma_rxdesc_desc *pdesc_head = rxdesc_ring->pdesc_head;
		if (unlikely(EDMA_RXDESC_SC_CC_VALID_GET(rxdesc_ring->pdesc_head) && edma_rx_handle_sc_cc_packets(egc, rxdesc_ring, pdesc_head, skb_head))) {
			rxdesc_ring->head = NULL;
			rxdesc_ring->last = NULL;
			rxdesc_ring->pdesc_head = NULL;
			return;
		}

		vprxi.flow_idx = EDMA_RX_SDESC_FLOW_IDX_INVALID;

		/*
		 * See if this packet is tagged with valid wifi qos.
		 * WiFi-QoS flag needs to be set for tree_id processing.
		 */
		if (unlikely(EDMA_RXDESC_WIFI_QOS_FLAG_VALID_GET(rxdesc_ring->pdesc_head))) {
			edma_rx_handle_wifi_qos_packets(egc, rxdesc_ring, pdesc_head, skb_head, &vprxi);
		}
	}

	/*
	 * Check if packet is meant for VP processing
	 */
	if (unlikely(EDMA_RXDESC_SRC_DST_INFO_GET(rxdesc_desc) & EDMA_RXDESC_SRC_DST_VP_MASK)) {
		mem_debug_update_skb(skb_head);
		edma_rx_process_vp(rxdesc_ring->pdesc_head, rxdesc_ring, skb_head, &vprxi);
		rxdesc_ring->head = NULL;
		rxdesc_ring->last = NULL;
		rxdesc_ring->pdesc_head = NULL;
		return;
	}

	/*
	 * Perform the RX DP Extension processing
	 * using the relevant details as configured.
	 */
	if (unlikely(edma_dp_extension_en)) {
		edma_rx_dp_extension_process(dp_dev, skb_head);
	}

	skb_head->protocol = eth_type_trans(skb_head, dev);

	/*
	 * Send packet up the stack
	 */
	mem_debug_update_skb(skb_head);
        desc_index = ((uint8_t *)rxdesc_ring->pdesc_head - (uint8_t *)rxdesc_ring->pdesc) >> EDMA_RXDESC_SIZE_SHIFT;
        rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, desc_index);
	tree_id_type = EDMA_RXDESC_TREE_ID_TYPE_GET(rxdesc_sec);
	if (tree_id_type == PPE_DRV_TREE_ID_TYPE_UDP_ST && likely(nss_dp_udp_st_rx_cb)) {
		nss_dp_udp_st_rx_cb(skb_head);
	} else {
#if defined(NSS_DP_ENABLE_NAPI_GRO)
		napi_gro_receive(&rxdesc_ring->napi, skb_head);
#else
		netif_receive_skb(skb_head);
#endif
	}

	rxdesc_ring->head = NULL;
	rxdesc_ring->last = NULL;
	rxdesc_ring->pdesc_head = NULL;
}

/*
 * edma_rx_handle_capwap_inear_packets()
 *	Handle linear packets
 */
void edma_rx_handle_capwap_linear_packets(struct edma_gbl_ctx *egc,
		struct edma_rxdesc_ring *rxdesc_ring,
		struct edma_rxdesc_desc *rxdesc_desc,
		struct sk_buff *skb, struct net_device *dev)
{
	struct edma_pcpu_stats *pcpu_stats;
	struct edma_rx_stats *rx_stats;
	uint32_t pkt_length, dst_port;
	struct nss_dp_vp_ctx *ctx;
	struct nss_dp_dev *dp_dev;
	skb_frag_t *frag = NULL;
	bool page_mode;
	uint16_t dvp;

	/*
	 * Get stats for the netdevice
	 */
	dp_dev = netdev_priv(dev);
	pcpu_stats = &dp_dev->dp_info.pcpu_stats;
	rx_stats = this_cpu_ptr(pcpu_stats->rx_stats);

	/*
	 * Get packet length
	 */
	pkt_length = EDMA_RXDESC_PACKET_LEN_GET(rxdesc_desc);

	page_mode = rxdesc_ring->rxfill->page_mode;
	if (unlikely(page_mode)) {

		/*
		 * Handle linear packet in page mode
		 */
		frag = &skb_shinfo(skb)->frags[0];
		edma_dmac_inv_range((void *)skb_frag_page(frag),
				(void *)(skb_frag_page(frag) + pkt_length));
		skb_add_rx_frag(skb, 0, skb_frag_page(frag), 0, pkt_length, PAGE_SIZE);

		/*
		 * Pull ethernet header into SKB data area for header processing
		 */
		if (unlikely(!pskb_may_pull(skb, ETH_HLEN))) {
			u64_stats_update_begin(&rx_stats->syncp);
			rx_stats->rx_nr_frag_headroom_err++;
			u64_stats_update_end(&rx_stats->syncp);
			mem_debug_update_skb(skb);
			dev_kfree_skb_any(skb);
			return;
		}

		goto send_to_vp;
	}

	/*
	 * Invalidate the buffer received from the HW
	 */
	edma_dmac_inv_range_no_dsb((void *)skb->data,
			(void *)(skb->data + pkt_length));
	skb_put(skb, pkt_length);

send_to_vp:

	/*
	 * In some cases like PPE tunnel when mode 1 is enabled
	 * the skb data will be pointing to outer header and if
	 * packet decap is successful then data offset will point
	 * to inner payload.
	 */
	__skb_pull(skb, EDMA_RXDESC_DATA_OFFSET_GET(rxdesc_desc));

	/*
	 * TODO: Do a batched update of the stats per netdevice.
	 */
	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_pkts++;
	rx_stats->rx_bytes += pkt_length;
	rx_stats->rx_nr_frag_pkts += (uint64_t)page_mode;
	u64_stats_update_end(&rx_stats->syncp);

	edma_debug("edma_gbl_ctx:%px, skb:%px pkt_length:%u\n",
			egc, skb, skb->len);

	dst_port = EDMA_RXDESC_DST_INFO_GET(rxdesc_desc);
	if (unlikely((dst_port & ~EDMA_RXDESC_DST_PORT_ID_MASK) != EDMA_RXDESC_DST_PORT)) {
		goto send_to_linux;
	}

	/*
	 * FIXME: Check for valid dvp
	 * Check for virtual max
	 */
	dvp = EDMA_RXDESC_DST_PORT_ID_GET(rxdesc_desc);
	if ((dvp < PPE_DRV_VIRTUAL_START) || (dvp >= PPE_DRV_PORTS_MAX)) {
		goto send_to_linux;
	}

	ctx = this_cpu_ptr(&g_vp_ctx);
	edma_rx_process_capwap_vp(ctx, dvp, skb);
	return;

send_to_linux:
	skb->protocol = eth_type_trans(skb, dev);
	mem_debug_update_skb(skb);
	netif_receive_skb(skb);
	return;
}

/*
 * edma_rx_handle_linear_packets()
 *	Handle linear packets
 *
 * Return false if packet is consumed by this function for error cases or VP/SC cases.
 * Return true otherwise, for caller to deliver packet to the stack.
 */
static inline bool edma_rx_handle_linear_packets(struct edma_gbl_ctx *egc,
		struct edma_rxdesc_ring *rxdesc_ring,
		struct edma_rxdesc_desc *rxdesc_desc,
		struct nss_dp_dev *dp_dev,
		struct sk_buff *skb)
{
	struct edma_pcpu_stats *pcpu_stats;
	struct edma_rx_stats *rx_stats;
	struct nss_dp_vp_rx_info vprxi = {0};
	uint32_t pkt_length;
	skb_frag_t *frag = NULL;
	bool page_mode = rxdesc_ring->rxfill->page_mode;

	mem_debug_update_skb(skb);
	/*
	 * Get stats for the netdevice
	 */
	pcpu_stats = &dp_dev->dp_info.pcpu_stats;
	rx_stats = this_cpu_ptr(pcpu_stats->rx_stats);

	/*
	 * Get packet length
	 */
	pkt_length = EDMA_RXDESC_PACKET_LEN_GET(rxdesc_desc);

	if (likely(!page_mode)) {

		/*
		 * Invalidate the buffer received from the HW
		 */
		edma_dmac_inv_range((void *)skb->data,
				(void *)(skb->data + pkt_length));
		skb_put(skb, pkt_length);
		goto send_to_stack;
	}

	/*
	 * Handle linear packet in page mode
	 */
	frag = &skb_shinfo(skb)->frags[0];
	edma_dmac_inv_range((void *)skb_frag_page(frag),
			(void *)(skb_frag_page(frag) + pkt_length));
	skb_add_rx_frag(skb, 0, skb_frag_page(frag), 0, pkt_length, PAGE_SIZE);

	/*
	 * Pull ethernet header into SKB data area for header processing
	 */
	if (unlikely(!pskb_may_pull(skb, ETH_HLEN))) {
		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_nr_frag_headroom_err++;
		u64_stats_update_end(&rx_stats->syncp);
		mem_debug_update_skb(skb);
		dev_kfree_skb_any(skb);
		return false;
	}

send_to_stack:

	/*
	 * In some cases like PPE tunnel when mode 1 is enabled
	 * the skb data will be pointing to outer header and if
	 * packet decap is successful then data offset will point
	 * to inner payload.
	 */
	__skb_pull(skb, EDMA_RXDESC_DATA_OFFSET_GET(rxdesc_desc));

	/*
	 * Check Rx checksum offload status.
	 */
	if (likely(skb->dev->features & NETIF_F_RXCSUM)) {
		skb->ip_summed = edma_rx_checksum_verify(rxdesc_desc, skb);
	}

	/*
	 * TODO: Do a batched update of the stats per netdevice.
	 */
	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_pkts++;
	rx_stats->rx_bytes += pkt_length;
	rx_stats->rx_nr_frag_pkts += (uint64_t)page_mode;
	u64_stats_update_end(&rx_stats->syncp);

	edma_debug("edma_gbl_ctx:%px, skb:%px pkt_length:%u\n",
			egc, skb, skb->len);
	/*
	 * See if this packet is tagged with a special service code
	 * or CPU code.
	 */
	if (unlikely(EDMA_RXDESC_SC_CC_VALID_GET(rxdesc_desc))) {
		/*
		 * NOTE:
		 * 1. We are combining both the checks together here to reduce
		 *    a branch instruction in regular data path processing.
		 *
		 * 2. If the service code/cpu code processing consumes the packet,
		 *    don't send it to stack otherwise continue with regular processing.
		 */
		mem_debug_update_skb(skb);
		if (edma_rx_handle_sc_cc_packets(egc, rxdesc_ring, rxdesc_desc, skb)) {
			return false;
		}
	}

	vprxi.flow_idx = EDMA_RX_SDESC_FLOW_IDX_INVALID;

	/*
	 * See if this packet is tagged with valid wifi qos.
	 * WiFi-QoS flag needs to be set for tree_id processing.
	 */
	if (unlikely(EDMA_RXDESC_WIFI_QOS_FLAG_VALID_GET(rxdesc_desc))) {
		edma_rx_handle_wifi_qos_packets(egc, rxdesc_ring, rxdesc_desc, skb, &vprxi);
	}

	/*
	 * Check if packet is meant for VP processing
	 */
	if (EDMA_RXDESC_SRC_DST_INFO_GET(rxdesc_desc) & EDMA_RXDESC_SRC_DST_VP_MASK) {
		mem_debug_update_skb(skb);
		edma_rx_process_vp(rxdesc_desc, rxdesc_ring, skb, &vprxi);
		return false;
	}

	/*
	 * Perform the RX DP Extension processing
	 * using the relevant details as configured.
	 */
	if (unlikely(edma_dp_extension_en)) {
		mem_debug_update_skb(skb);
		edma_rx_dp_extension_process(dp_dev, skb);
	}

	return true;
}

/*
 * edma_rx_get_src_port_and_dev()
 *	Get source port and corresponding net device.
 */
static inline struct net_device *edma_rx_get_src_dev(
		struct edma_gbl_ctx *egc,
		struct edma_rx_desc_stats *rxdesc_stats,
		struct edma_rxdesc_desc *rxdesc_desc,
		struct sk_buff *skb)
{
	struct net_device *ndev = NULL;
	uint32_t src_info = EDMA_RXDESC_SRC_INFO_GET(rxdesc_desc);
	uint8_t src_port_num;

	/*
	 * Check src_info
	 */
	if (likely((src_info & EDMA_RXDESC_SRCINFO_TYPE_MASK)
				== EDMA_RXDESC_SRCINFO_TYPE_PORTID)) {
		src_port_num = src_info & EDMA_RXDESC_PORTNUM_BITS;
	} else {
		if (net_ratelimit()) {
			edma_debug("Src_info_type:0x%x. Drop skb:%px\n",
					(src_info & EDMA_RXDESC_SRCINFO_TYPE_MASK), skb);
		}

		u64_stats_update_begin(&rxdesc_stats->syncp);
		++rxdesc_stats->src_port_inval_type;
		u64_stats_update_end(&rxdesc_stats->syncp);
		return NULL;
	}

	/*
	 * Packet with PP source
	 */
	if (unlikely(src_port_num <= NSS_DP_HAL_MAX_PORTS)) {
		if (unlikely(src_port_num < NSS_DP_START_IFNUM)) {
			if (net_ratelimit()) {
				edma_warn("Port number error :%d \
						Drop skb:%px\n",
						src_port_num, skb);
			}

			u64_stats_update_begin(&rxdesc_stats->syncp);
			++rxdesc_stats->src_port_inval;
			u64_stats_update_end(&rxdesc_stats->syncp);
			return NULL;
		}

		/*
		 * Get netdev for this port using the source port
		 * number as index into the netdev array. We need to
		 * subtract one since the indices start form '0' and
		 * port numbers start from '1'.
		 */
		ndev = egc->netdev_arr[src_port_num - 1];
		goto done;
	}

	if (unlikely(src_port_num < PPE_DRV_VIRTUAL_START)) {
		if (net_ratelimit()) {
			edma_warn("Port number error :%d. \
				Drop skb:%px\n",
				src_port_num, skb);
		}

		u64_stats_update_begin(&rxdesc_stats->syncp);
		++rxdesc_stats->src_port_inval;
		u64_stats_update_end(&rxdesc_stats->syncp);
		return NULL;
	}

	/*
	 * Last netdev corresponds to VP dummy netdev
	 */
	ndev = egc->netdev_arr[NSS_DP_MAX_PORTS - 1];

done:
	if (likely(ndev))
		return ndev;

	if (net_ratelimit()) {
		edma_warn("Netdev Null src_info_type:0x%x. Drop skb:%px\n",
				src_port_num, skb);
	}

	u64_stats_update_begin(&rxdesc_stats->syncp);
	++rxdesc_stats->src_port_inval_netdev;
	u64_stats_update_end(&rxdesc_stats->syncp);
	return NULL;
}

#ifdef CONFIG_SKB_TIMESTAMP
/*
 * edma_rx_get_tstamp()
 *	Compute the timestamp received in the secondary descriptor
 */
static inline uint64_t edma_rx_get_tstamp(struct edma_rxdesc_sec_desc *rxsec_desc)
{
	uint32_t nsecs, secs;

	nsecs = EDMA_RX_SDESC_TSTAMP_LO_GET(rxsec_desc);
	secs = EDMA_RX_SDESC_TSTAMP_HI_GET(rxsec_desc);
	return EDMA_TIMESTAMP_TO_USEC(secs, nsecs);
}

/*
 * edma_rx_read_gmac_timer()()
 *	API to read the GMAC timer register
 */
static inline uint64_t edma_rx_read_gmac_timer(struct edma_gbl_ctx *egc)
{
	uint32_t nsecs, secs;

	nsecs = readl(egc->tstamp_nsec);
	secs = readl(egc->tstamp_sec) & EDMA_TIMESTAMP_SEC_MASK;
	return EDMA_TIMESTAMP_TO_USEC(secs, nsecs);
}
#endif

/*
 * edma_rx_get_src_capwap_dev()
 *	Get source port and corresponding net device.
 */
struct net_device *edma_rx_get_src_capwap_dev(struct edma_gbl_ctx *egc,
		struct edma_rx_desc_stats *rxdesc_stats,
		struct edma_rxdesc_desc *rxdesc_desc,
		struct sk_buff *skb)
{
	struct net_device *ndev;
	uint32_t src_info = EDMA_RXDESC_SRC_INFO_GET(rxdesc_desc);
	uint8_t src_port_num;

	/*
	 * Check src_info
	 */
	if (unlikely((src_info & EDMA_RXDESC_SRCINFO_TYPE_MASK) != EDMA_RXDESC_SRCINFO_TYPE_PORTID)) {
		if (net_ratelimit()) {
			edma_warn("Src_info_type:0x%x. Drop skb:%px\n",
					(src_info &
					 EDMA_RXDESC_SRCINFO_TYPE_MASK),
					skb);
		}
		u64_stats_update_begin(&rxdesc_stats->syncp);
		++rxdesc_stats->src_port_inval_type;
		u64_stats_update_end(&rxdesc_stats->syncp);
		return NULL;
	}

	src_port_num = src_info & EDMA_RXDESC_PORTNUM_BITS;
	if (likely(((src_port_num >= PPE_DRV_VIRTUAL_START) && (src_port_num < PPE_DRV_PORTS_MAX)))) {
		ndev = egc->netdev_arr[NSS_DP_MAX_PORTS - 1];
		if (likely(ndev)) {
			return ndev;
		}

		if (net_ratelimit()) {
			edma_warn("Netdev Null src_info_type:0x%x. Drop skb:%px\n",
								src_port_num, skb);
		}
		u64_stats_update_begin(&rxdesc_stats->syncp);
		++rxdesc_stats->src_port_inval_netdev;
		u64_stats_update_end(&rxdesc_stats->syncp);
		return NULL;
	}

	if (unlikely(src_port_num <= NSS_DP_HAL_MAX_PORTS)) {
		if (unlikely(src_port_num < NSS_DP_START_IFNUM)) {
			if (net_ratelimit()) {
				edma_warn("Port number error :%d. Drop skb:%px\n",
								src_port_num, skb);
			}
			u64_stats_update_begin(&rxdesc_stats->syncp);
			++rxdesc_stats->src_port_inval;
			u64_stats_update_end(&rxdesc_stats->syncp);
			return NULL;
		}

		/*
		 * Get netdev for this port using the source port
		 * number as index into the netdev array. We need to
		 * subtract one since the indices start form '0' and
		 * port numbers start from '1'.
		 */
		ndev = egc->netdev_arr[src_port_num - 1];
	}

	if (likely(ndev)) {
		return ndev;
	}

	if (net_ratelimit()) {
		edma_warn("Netdev Null src_info_type:0x%x. Drop skb:%px\n", src_port_num, skb);
	}
	u64_stats_update_begin(&rxdesc_stats->syncp);
	++rxdesc_stats->src_port_inval_netdev;
	u64_stats_update_end(&rxdesc_stats->syncp);
	return NULL;
}

#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
/*
 * edma_rx_hwtstamp()
 *	Extract RX hardware timestamp from service code data.
 *
 * This function extracts the RX timestamp from the service code data, the RX
 * timestamp is from RX second descriptor.
 *
 * RX Descriptor Format:
 * - Word 0: TIMESTAMP_LO (32 bits nanoseconds)
 * - Word 1 [7:0]: TIMESTAMP_HI (8 bits seconds)
 * - Word 3 [23]: TIMESTAMP_VALID flag
 *
 * @skb: Socket buffer to attach timestamp to
 * @sc_data: Pointer to service code data
 */
static void edma_rx_hwtstamp(struct sk_buff *skb,
			     struct ppe_drv_sc_metadata *sc_data)
{
	struct skb_shared_hwtstamps *hwts;
	struct nss_dp_dev *dp_dev;
	struct syn_hal_dev *shd;
	struct syn_ptp_priv *ptp_priv;
	void __iomem *mac_base;
	u64 ns;
	u32 sys_sec;
	u32 ts_sec;

	if (unlikely(!sc_data)) {
		edma_err("sc_data is null\n");
		return;
	}

	/* Ensure skb->dev is valid before dereferencing */
	if (unlikely(!skb || !skb->dev)) {
		edma_err("skb or skb->dev is null\n");
		return;
	}

	/* Get the system time in seconds (32-bit) */
	dp_dev = netdev_priv(skb->dev);
	if (!dp_dev || !dp_dev->gmac_hal_ctx) {
		edma_err("dp_dev or gmac_hal_ctx is null\n");
		/* Failed to get device context */
		return;
	}

	/*
	 * Only syn_hal_dev (XGMAC) supports PTP hardware timestamping.
	 * qcom_hal_dev does NOT have ptp_priv at the same struct offset
	 */
	if (!dp_dev->gmac_hal_ops || !dp_dev->gmac_hal_ops->hwtstamp_set) {
		edma_debug("HAL does not support PTP timestamping\n");
		return;
	}

	shd = (struct syn_hal_dev *)dp_dev->gmac_hal_ctx;
	ptp_priv = shd->ptp_priv;
	if (!ptp_priv) {
		edma_debug("PTP clock is not defined\n");
		return;
	}

	if (ptp_priv->tstamp_config.rx_filter == HWTSTAMP_FILTER_NONE) {
		edma_debug("PTP function is not enabled\n");
		return;
	}

	mac_base = dp_dev->gmac_hal_ctx->mac_base;
	if (!mac_base) {
		edma_err("mac_base is null\n");
		return;
	}

	/*
	 * The EDMA RX descriptor provides only the lower 8 bits of the
	 * seconds value. Read the current system time to determine the
	 * upper 24 bits and reconstruct the full timestamp.
	 */
	sys_sec = hal_read_reg(mac_base, 0xd08);
	ts_sec = (sys_sec & 0xFFFFFF00) | sc_data->ts_sec;

	/* Handle rollover of 8 bits second:
	 * if combined time is in future, decrement upper bits.
	 */
	if (ts_sec > sys_sec)
		ts_sec -= 0x100;

	/* DEBUG: Print raw timestamp values */
	edma_debug("RX TS: ts_sec=0x%x, ts_nsec=0x%x, sys_sec=0x%x\n",
		   sc_data->ts_sec, sc_data->ts_nsec, sys_sec);

	/* Combine into 64-bit nanoseconds and store in SKB */
	ns = ((u64)ts_sec * NSEC_PER_SEC) + sc_data->ts_nsec;
	hwts = skb_hwtstamps(skb);
	memset(hwts, 0, sizeof(*hwts));
	hwts->hwtstamp = ns_to_ktime(ns);

	/* Increment success counter */
	ptp_priv->rx_ts_success++;
}
#endif

/*
 * edma_rx_reap_capwap()
 *	Reap Rx descriptors
 */
static uint32_t edma_rx_reap_capwap(struct edma_gbl_ctx *egc, int budget,
				struct edma_rxdesc_ring *rxdesc_ring)
{
	struct edma_rxdesc_desc *rxdesc_desc, *pf_desc = NULL;
	struct edma_rx_desc_stats *rxdesc_stats = &rxdesc_ring->rx_desc_stats;
	struct nss_dp_vp_ctx *ctx = this_cpu_ptr(&g_vp_ctx);
	uint32_t work_to_do, work_done = 0;
	uint16_t prod_idx, cons_idx, end_idx;
	nss_dp_vp_rx_cb_t edma_rx_vp_cb;
	uint16_t cons_idx_1 = 0;
	uint16_t cons_idx_2 = 0;
	struct list_head rx_list;
	uint16_t bit;

	INIT_LIST_HEAD(&rx_list);

	/*
	 * Get Rx ring producer and consumer indices
	 */
	cons_idx = rxdesc_ring->cons_idx;

	/*
	 * Record ring utilization for capwap ring
	 */
	if (unlikely(egc->enable_ring_util_stats)) {
		prod_idx = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->ring_id)) & EDMA_RXDESC_PROD_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(prod_idx, cons_idx, EDMA_RX_RING_SIZE);

		edma_update_ring_stats(work_to_do, EDMA_RX_RING_SIZE,
				&rxdesc_ring->rx_desc_stats.ring_stats);
	}

	if (likely(rxdesc_ring->work_leftover > budget)) {
		work_to_do = budget;
	} else {
		prod_idx =
			edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->ring_id)) &
			EDMA_RXDESC_PROD_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(prod_idx,
				cons_idx, EDMA_RX_RING_SIZE);
		rxdesc_ring->work_leftover = work_to_do;
		if (likely(work_to_do > budget)) {
			work_to_do = budget;
		}
	}

	rxdesc_ring->work_leftover -= work_to_do;

	end_idx = (cons_idx + work_to_do) & EDMA_RX_RING_SIZE_MASK;

	rxdesc_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx);

	/*
	 * Invalidate all the cached descriptors
	 * that'll be processed.
	 */
	if (end_idx > cons_idx) {
		edma_dmac_inv_range_no_dsb((void *)rxdesc_desc,
			(void *)(rxdesc_desc + work_to_do));
	} else {
		edma_dmac_inv_range_no_dsb((void *)rxdesc_ring->pdesc,
			(void *)(rxdesc_ring->pdesc + end_idx));
		edma_dmac_inv_range_no_dsb((void *)rxdesc_desc,
			(void *)(rxdesc_ring->pdesc + EDMA_RX_RING_SIZE));
	}

	/*
	 * TODO: Handle refill failures using retry
	 */
	edma_rx_alloc_buffer_list(rxdesc_ring->rxfill, work_to_do);

	/*
	 * Prefetch upto 3 Rx descriptors.
	 */
	prefetch(rxdesc_desc);
	if (likely(work_to_do >= 3)) {
		cons_idx_1 = (cons_idx + 1) & EDMA_RX_RING_SIZE_MASK;
		pf_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx_1);
		prefetch(pf_desc);

		cons_idx_2 = (cons_idx_1 + 1) & EDMA_RX_RING_SIZE_MASK;
		pf_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx_2);
		prefetch(pf_desc);
	}

	while (likely(work_to_do--)) {
		struct net_device *ndev;
		struct sk_buff *skb;

		/*
		 * Get opaque from RXDESC
		 */
		skb = (struct sk_buff *)EDMA_RXDESC_OPAQUE_GET(rxdesc_desc);

		if (likely(!(rxdesc_ring->head))) {
			ndev = edma_rx_get_src_capwap_dev(egc, rxdesc_stats, rxdesc_desc, skb);
			if(unlikely(!ndev)) {
				mem_debug_update_skb(skb);
				dev_kfree_skb_any(skb);

				/*
				 * Update work done
				 */
				work_done++;

				/*
				 * Update consumer index
				 */
				cons_idx = (cons_idx + 1) & EDMA_RX_RING_SIZE_MASK;

				/*
				 * Get the next Rx descriptor.
				 */
				rxdesc_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx);
				continue;
			}

			/*
			 * Prefetch the third skb and the fourth descriptor
			 */
			if (likely(work_to_do >= 3)) {
				struct sk_buff *pf_skb;
				pf_skb = (struct sk_buff *)EDMA_RXDESC_OPAQUE_GET(pf_desc);
				prefetch(pf_skb);
				prefetch((uint8_t *)pf_skb + 64);
				prefetch((uint8_t *)pf_skb + 128);
				cons_idx_2 = (cons_idx_2 + 1) & EDMA_RX_RING_SIZE_MASK;

				pf_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx_2);
				prefetch(pf_desc);
			}

			/*
			 * Update skb fields for head skb
			 */
			skb->dev = ndev;
			skb->skb_iif = ndev->ifindex;
			skb_set_int_pri(skb, EDMA_RXDESC_INT_PRI_GET(rxdesc_desc));

			/*
			 * Handle linear packets
			 */
			if (likely(!EDMA_RXDESC_MORE_BIT_GET(rxdesc_desc))) {
				edma_rx_handle_capwap_linear_packets(egc, rxdesc_ring, rxdesc_desc, skb, ndev);
				goto next_rx_desc;
			}
		}

		/*
		 * Handle scatter frame processing for first/middle/last segments
		 */
		mem_debug_update_skb(skb);
		edma_rx_handle_scatter_frames(egc, rxdesc_ring, rxdesc_desc, skb);

next_rx_desc:
		/*
		 * Update work done
		 */
		work_done++;

		/*
		 * Update consumer index
		 */
		cons_idx = (cons_idx + 1) & EDMA_RX_RING_SIZE_MASK;

		/*
		 * Get the next Rx descriptor.
		 */
		rxdesc_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx);
	}

	edma_dsb();

	rcu_read_lock();
	edma_rx_vp_cb = rcu_dereference(nss_dp_vp_rx_reg_cb);
	for_each_set_bit(bit, ctx->active_vps, PPE_DRV_VIRTUAL_MAX) {
		struct nss_dp_vp_node *node = &ctx->nodes[bit];
		struct nss_dp_vp_rx_info rx_info = {0};
		struct nss_dp_vp_rx_data rx_data ={0};
		struct sk_buff_head tmp;

		rx_info.dvp = node->info.dvp;
		rx_info.napi = &rxdesc_ring->napi;
		rx_info.total_bytes = node->info.bytes;

		skb_queue_head_init(&tmp);
		skb_queue_splice_init(&node->head, &tmp);

		clear_bit(bit, ctx->active_vps);
		memset(&node->info, 0, sizeof(node->info));

		if (unlikely(!edma_rx_vp_cb)) {
			dev_kfree_skb_list_fast(&tmp);
			continue;
		}

		rx_data.type = NSS_DP_VP_RX_TYPE_SKB_LIST;
		rx_data.skb_head = &tmp;
		edma_rx_vp_cb(&rx_data, &rx_info);
	}

	rcu_read_unlock();

	edma_reg_write(EDMA_REG_RXDESC_CONS_IDX(rxdesc_ring->ring_id), cons_idx);
	rxdesc_ring->cons_idx = cons_idx;

	return work_done;
}

/*
 * edma_rx_reap()
 *	Reap Rx descriptors
 */
static uint32_t edma_rx_reap(struct edma_gbl_ctx *egc, int budget,
				struct edma_rxdesc_ring *rxdesc_ring)
{
	struct edma_rxdesc_desc *rxdesc_desc, *pf_desc = NULL;
	struct edma_rxdesc_sec_desc *rxdesc_sec;
	struct edma_rx_desc_stats *rxdesc_stats = &rxdesc_ring->rx_desc_stats;
	uint32_t work_to_do, work_done = 0, num_alloc, num_reap;
	uint16_t prod_idx, cons_idx, end_idx;
	uint16_t cons_idx_1, cons_idx_2;
	struct sk_buff *cur_skb = NULL, *next_skb = NULL;
	struct edma_rxfill_ring *rxfill_ring;
	struct list_head rx_list;
	ppe_drv_tree_id_type_t tree_id_type;
	INIT_LIST_HEAD(&rx_list);

	/*
	 * Get Rx ring producer and consumer indices
	 */
	cons_idx = rxdesc_ring->cons_idx;

	if (unlikely(egc->enable_ring_util_stats)) {
		prod_idx = edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->ring_id)) & EDMA_RXDESC_PROD_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(prod_idx, cons_idx, EDMA_RX_RING_SIZE);

		edma_update_ring_stats(work_to_do, EDMA_RX_RING_SIZE,
				       &rxdesc_ring->rx_desc_stats.ring_stats);
	}

	if (likely(rxdesc_ring->work_leftover > budget)) {
		work_to_do = budget;
	} else {
		prod_idx =
			edma_reg_read(EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->ring_id)) &
			EDMA_RXDESC_PROD_IDX_MASK;
		work_to_do = EDMA_DESC_AVAIL_COUNT(prod_idx,
				cons_idx, EDMA_RX_RING_SIZE);
		rxdesc_ring->work_leftover = work_to_do;
		if (likely(work_to_do > budget)) {
			work_to_do = budget;
		}
	}

	rxdesc_ring->work_leftover -= work_to_do;

	end_idx = (cons_idx + work_to_do) & EDMA_RX_RING_SIZE_MASK;

	rxdesc_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx);
	rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, cons_idx);

	/*
	 * Invalidate all the cached descriptors
	 * that'll be processed.
	 */
	if (end_idx > cons_idx) {
		edma_dmac_inv_range_no_dsb((void *)rxdesc_desc,
			(void *)(rxdesc_desc + work_to_do));
		edma_dmac_inv_range_no_dsb((void *)rxdesc_sec,
			(void *)(rxdesc_sec + work_to_do));
	} else {
		edma_dmac_inv_range_no_dsb((void *)rxdesc_ring->pdesc,
			(void *)(rxdesc_ring->pdesc + end_idx));
		edma_dmac_inv_range_no_dsb((void *)rxdesc_ring->sdesc,
			(void *)(rxdesc_ring->sdesc + end_idx));
		edma_dmac_inv_range_no_dsb((void *)rxdesc_desc,
			(void *)(rxdesc_ring->pdesc + EDMA_RX_RING_SIZE));
		edma_dmac_inv_range_no_dsb((void *)rxdesc_sec,
			(void *)(rxdesc_ring->sdesc + EDMA_RX_RING_SIZE));
	}

	/*
	 * TODO: Handle refill failures using retry
	 */
	rxfill_ring = rxdesc_ring->rxfill;
	num_reap = work_to_do;
	num_alloc = edma_rx_alloc_buffer_list(rxfill_ring, work_to_do);

	/*
	 * Prefetch upto 3 Rx descriptors.
	 */
	prefetch(rxdesc_desc);
	if (likely(work_to_do >= 3)) {
		cons_idx_1 = (cons_idx + 1) & EDMA_RX_RING_SIZE_MASK;
		pf_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx_1);
		prefetch(pf_desc);

		cons_idx_2 = (cons_idx_1 + 1) & EDMA_RX_RING_SIZE_MASK;
		pf_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx_2);
		prefetch(pf_desc);
	}

	while (likely(work_to_do--)) {
		struct net_device *ndev;
		struct sk_buff *skb;

		/*
		 * Get opaque from RXDESC
		 */
		skb = (struct sk_buff *)EDMA_RXDESC_OPAQUE_GET(rxdesc_desc);
		mem_debug_update_skb(skb);

#ifdef CONFIG_SKB_TIMESTAMP
		if (EDMA_RX_SDESC_TSTAMP_VALID_GET(rxdesc_sec)) {
			uint64_t pkt_time, cur_time;

			pkt_time = edma_rx_get_tstamp(rxdesc_sec);
			cur_time = edma_rx_read_gmac_timer(egc);

			if (likely(cur_time > pkt_time)) {
				skb->delta_ts0 = cur_time - pkt_time;
				skb->delta_ts1 = EDMA_TIMESTAMP_NSEC_TO_USEC(ktime_get_ns());
				edma_debug("skb: %p, pkt_time: %llu, cur_time: %llu, delta_ts0: %llu, delta_ts1: %llu\n",
						skb, pkt_time, cur_time,
						skb->delta_ts0, skb->delta_ts1);
			}
		}
#endif

		/*
		 * Handle linear packets or initial segments first
		 */
		if (likely(!(rxdesc_ring->head))) {
			ndev = edma_rx_get_src_dev(egc, rxdesc_stats, rxdesc_desc, skb);
			if(unlikely(!ndev)) {
				mem_debug_update_skb(skb);
				dev_kfree_skb_any(skb);
				goto next_rx_desc;
			}

			/*
			 * Update skb fields for head skb
			 */
			skb->dev = ndev;
			skb_set_int_pri(skb, EDMA_RXDESC_INT_PRI_GET(rxdesc_desc));
			skb->skb_iif = ndev->ifindex;

			/*
			 * Prefetch the third skb and the fourth descriptor
			 */
			if (likely(work_to_do >= 3)) {
				struct sk_buff *pf_skb;
				void *data;

				pf_skb = (struct sk_buff *)EDMA_RXDESC_OPAQUE_GET(pf_desc);
				data = phys_to_virt(EDMA_RXDESC_BUFFER_ADDR_GET(pf_desc));
				prefetch(pf_skb);
				prefetch((uint8_t *)pf_skb + 64);
				prefetch((uint8_t *)pf_skb + 128);
				prefetch((uint8_t *)pf_skb + 192);
				prefetch((uint8_t *)data);
				cons_idx_2 = (cons_idx_2 + 1) & EDMA_RX_RING_SIZE_MASK;

				pf_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx_2);
				prefetch(pf_desc);
			}

			/*
			 * Handle linear packets
			 */
			if (likely(!EDMA_RXDESC_MORE_BIT_GET(rxdesc_desc))) {
				struct nss_dp_dev *dp_dev = netdev_priv(skb->dev);

				if (likely(edma_rx_handle_linear_packets(egc, rxdesc_ring, rxdesc_desc, dp_dev, skb))) {
					tree_id_type = EDMA_RXDESC_TREE_ID_TYPE_GET(rxdesc_sec);
					if (tree_id_type == PPE_DRV_TREE_ID_TYPE_UDP_ST && likely(nss_dp_udp_st_rx_cb)) {
						nss_dp_udp_st_rx_cb(skb);
					} else if (unlikely(ndev->features & NETIF_F_GRO)) {
						skb->protocol = eth_type_trans(skb, ndev);
						mem_debug_update_skb(skb);
						napi_gro_receive(&rxdesc_ring->napi, skb);
					} else if (test_bit(__NSS_DP_NO_LIST, &dp_dev->flags)) {
						prefetch(skb_shinfo(skb));
						skb->protocol = eth_type_trans(skb, skb->dev);
						mem_debug_update_skb(skb);
						netif_receive_skb(skb);
					} else {
						list_add_tail(&skb->list, &rx_list);
					}
				}
				goto next_rx_desc;
			}
		}

		/*
		 * Handle scatter frame processing for first/middle/last segments
		 */
		mem_debug_update_skb(skb);
		edma_rx_handle_scatter_frames(egc, rxdesc_ring, rxdesc_desc, skb);

next_rx_desc:
		/*
		 * Update work done
		 */
		work_done++;

		/*
		 * Update consumer index
		 */
		cons_idx = (cons_idx + 1) & EDMA_RX_RING_SIZE_MASK;

		/*
		 * Get the next Rx descriptor.
		 */
		rxdesc_desc = EDMA_RXDESC_PRI_DESC(rxdesc_ring, cons_idx);
#ifdef CONFIG_SKB_TIMESTAMP
		rxdesc_sec = EDMA_RXDESC_SEC_DESC(rxdesc_ring, cons_idx);
#endif
	}

	edma_reg_write(EDMA_REG_RXDESC_CONS_IDX(rxdesc_ring->ring_id), cons_idx);
	rxdesc_ring->cons_idx = cons_idx;

	/*
	 * Prefetch the packet data for the next skbuff, and the skbuff
	 * structure for next and next-next skbuffs for optimal performance.
	 */
	list_for_each_entry_safe(cur_skb, next_skb, &rx_list, list) {
		if (likely(!list_entry_is_head(next_skb, &rx_list, list))) {
			prefetch(next_skb);
			prefetch((uint8_t *)(next_skb) + 64);
			prefetch((uint8_t *)(next_skb) + 128);
			prefetch((uint8_t *)(next_skb) + 192);
			prefetch(next_skb->data);
			prefetch(skb_shinfo(next_skb));
		}

		skb_list_del_init(cur_skb);
		cur_skb->protocol = eth_type_trans(cur_skb, cur_skb->dev);
		netif_receive_skb(cur_skb);
	}

	if (unlikely(rxfill_ring->num_rxfill_pending >=
			(EDMA_RX_RING_SIZE - EDMA_RXFILL_UGT_THRESHOLD))) {

		edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id),
				egc->rxfill_intr_mask);
	}

	return work_done;
}

/*
 * edma_rx_napi_capwap_poll()
 *	EDMA RX NAPI handler
 */
int edma_rx_napi_capwap_poll(struct napi_struct *napi, int budget)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)napi;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	int32_t work_done = 0;
	uint32_t status;

	do {
		work_done += edma_rx_reap_capwap(egc, budget - work_done, rxdesc_ring);
		if (likely(work_done >= budget)) {
			return work_done;
		}

		/*
		 * Check if there are more packets to process
		 */
		status = EDMA_RXDESC_RING_INT_STATUS_MASK &
			edma_reg_read(
				EDMA_REG_RXDESC_INT_STAT(rxdesc_ring->ring_id));
	} while (likely(status));

	/*
	 * No more packets to process. Finish NAPI processing.
	 */
	napi_complete(napi);

	/*
	 * Set RXDESC ring interrupt mask
	 */
	edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
						egc->rxdesc_intr_mask);

	return work_done;
}

/*
 * edma_rx_napi_poll()
 *	EDMA RX NAPI handler
 */
int edma_rx_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)napi;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	int32_t work_done = 0;
	uint32_t status;

	do {
		work_done += edma_rx_reap(egc, budget - work_done, rxdesc_ring);
		if (likely(work_done >= budget)) {
			return work_done;
		}

		/*
		 * Check if there are more packets to process
		 */
		status = EDMA_RXDESC_RING_INT_STATUS_MASK &
			edma_reg_read(
				EDMA_REG_RXDESC_INT_STAT(rxdesc_ring->ring_id));
	} while (likely(status));

	/*
	 * No more packets to process. Finish NAPI processing.
	 */
	napi_complete(napi);

	/*
	 * Set RXDESC ring interrupt mask
	 */
	edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id),
						egc->rxdesc_intr_mask);

	return work_done;
}

/*
 * edma_rx_handle_irq()
 *	Process RX IRQ and schedule napi
 */
irqreturn_t edma_rx_handle_irq(int irq, void *ctx)
{
	struct edma_rxdesc_ring *rxdesc_ring = (struct edma_rxdesc_ring *)ctx;
	struct edma_rx_desc_stats *rxdesc_stats = &rxdesc_ring->rx_desc_stats;

	edma_debug("irq: irq=%d rxdesc_ring_id=%u\n", irq, rxdesc_ring->ring_id);

	/*
	 * Disable RxDesc interrupt
	 */
	edma_reg_write(EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->ring_id), EDMA_MASK_INT_DISABLE);

	if (likely(napi_schedule_prep(&rxdesc_ring->napi))) {
		__napi_schedule(&rxdesc_ring->napi);

		u64_stats_update_begin(&rxdesc_stats->syncp);
		++rxdesc_stats->rx_napi_sched;
		u64_stats_update_end(&rxdesc_stats->syncp);
	}

	return IRQ_HANDLED;
}

/*
 * edma_rxfill_intr_timer()
 *	Delayed rx-fill interrupt timer.
 */
void edma_rxfill_intr_timer(struct timer_list *tm)
{
	struct edma_rxfill_ring *rxfill_ring = from_timer(rxfill_ring, tm, delayed_intr);
	struct edma_gbl_ctx *egc = edma_gbl_ctx;

	/*
	 * Being called from a delayed timer, reset the rxfill interrupt attempts
	 * to allow interrupts to try to replenish buffers until the max attempt.
	 */
	rxfill_ring->rxfill_intr_attempt = 0;
	edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id), egc->rxfill_intr_mask);
}

/*
 * edma_rxfill_napi_poll()
 *	EDMA RXfill NAPI handler
 */
int edma_rxfill_napi_poll(struct napi_struct *napi, int budget)
{
	struct edma_rxfill_ring *rxfill_ring = (struct edma_rxfill_ring *)napi;
	struct edma_gbl_ctx *egc = edma_gbl_ctx;
	int32_t work_done = 0;
	uint32_t refill_attempt = 0;

	/*
	 * We try maximum descriptor re-fill to avoid getting into low
	 * threshold interrupt repeatedly. For a lower budget, the hardware
	 * consumes all the available buffers in rx-fill ring and raise subsequent
	 * low threshold interrrupt immediately without allowing rx-reap to proceed.
	 */
	do {
		edma_rx_alloc_buffer(rxfill_ring, 0);
		if (likely(!rxfill_ring->num_rxfill_pending)) {
			break;
		}

		/*
		 * In low memory situations, the allocation may fail.
		 * Return after certain number of retry to allow other NAPIs to get processed.
		 */
		refill_attempt++;
	} while (likely(refill_attempt < EDMA_RXFILL_ONE_INTR_ATTEMPT_MAX));

	/*
	 * Either all the empty buffers are replenished or we exhausted maximum
	 * retry attempts. Finish NAPI processing and let HW generate another
	 * low threshold interrupt if we still remain out of empty buffers.
	 */
	napi_complete(napi);

	/*
	 * Maintain a state to detect rx-fill urg interrupt flood.
	 * Count for how many interrupts we are not able to completely
	 * replenish the rx-fill ring.
	 */
	if (unlikely(rxfill_ring->num_rxfill_pending)) {
		rxfill_ring->rxfill_intr_attempt++;
	} else {
		rxfill_ring->rxfill_intr_attempt = 0;
	}

	/*
	 * Set RXFILL ring interrupt mask if
	 *
	 * During flood leave low threshold interrupt disabled and reenable
	 * it through a timer after a while to allow system to recover from
	 * a temporary OOM situation.
	 */
	if (rxfill_ring->rxfill_intr_attempt < EDMA_RXFILL_INTR_ATTEMPT_MAX) {
		edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id), egc->rxfill_intr_mask);
	} else {
		mod_timer(&rxfill_ring->delayed_intr, jiffies + msecs_to_jiffies(EDMA_RXFILL_DELAY_INTR_MS));
	}

	return work_done;
}

/*
 * edma_rx_mac_tstamp_consume()
 *	Extract and attach XGMAC hardware timestamp
 */
static bool edma_rx_mac_tstamp_consume(struct sk_buff *skb,
				       struct ppe_drv_sc_metadata *sc_data)
{
#if IS_ENABLED(CONFIG_PTP_1588_CLOCK)
	/* Extract and attach XGMAC hardware timestamp */
	edma_rx_hwtstamp(skb, sc_data);
#endif

	/* Continue processing, don't consume packet */
	return false;
}

/*
 * edma_rx_phy_tstamp_consume()
 *	Receive skb for PHY timestamping (internal function)
 */
static bool edma_rx_phy_tstamp_consume(struct sk_buff *skb)
{
	struct net_device *ndev = skb->dev;

	/*
	 * The PTP_CLASS_ value 0 is passed to phy driver, which will be
	 * set to the correct PTP class value by calling ptp_classify_raw
	 * in drv->rxtstamp function.
	 */
	if (ndev && ndev->phydev && ndev->phydev->drv &&
	    phy_has_rxtstamp(ndev->phydev)) {
		skb->protocol = eth_type_trans(skb, ndev);

		if (likely(phy_rxtstamp(ndev->phydev, skb, 0))) {
			return true;
		} else {
			__skb_push(skb, ETH_HLEN);
			edma_debug("Timestamp is not enabled with PHY driver");
		}
	}

	return false;
}

/*
 * edma_rx_tstamp_buf()
 *	Wrapper function for PTP timestamp processing
 *	Handles both PHY and XGMAC timestamps with priority
 */
bool edma_rx_tstamp_buf(void *app_data, struct sk_buff *skb, void *sc_data)
{
	/*
	 * Try PHY timestamping first (higher priority)
	 * If PHY consumes the packet, return true
	 */
	if (edma_rx_phy_tstamp_consume(skb)) {
		return true;
	}

	/*
	 * PHY didn't consume packet, apply XGMAC timestamp
	 * This always returns false (doesn't consume packet)
	 */
	return edma_rx_mac_tstamp_consume(skb, sc_data);
}

/*
 * edma_rxfill_handle_irq()
 *	Process RXFill IRQ and schedule napi
 */
irqreturn_t edma_rxfill_handle_irq(int irq, void *ctx)
{
	struct edma_rxfill_ring *rxfill_ring = (struct edma_rxfill_ring *)ctx;

	edma_debug("irq: irq=%d rxfill_ring_id=%u\n", irq, rxfill_ring->ring_id);

	if (likely(napi_schedule_prep(&rxfill_ring->napi))) {

		__napi_schedule(&rxfill_ring->napi);
	}

	/*
	 * Disable Rxfill interrupt
	 */
	edma_reg_write(EDMA_REG_RXFILL_INT_MASK(rxfill_ring->ring_id), EDMA_MASK_INT_DISABLE);
	return IRQ_HANDLED;
}
