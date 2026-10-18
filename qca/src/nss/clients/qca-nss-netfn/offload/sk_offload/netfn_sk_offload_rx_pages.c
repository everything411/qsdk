/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_sk_offload_rx_pages.c
 *      Network Sock Offload XDP-based Page Pool GRO Receive Processing.
 */
#include <linux/ip.h>
#include <linux/module.h>
#include <net/protocol.h>
#include <net/udp.h>
#include <net/page_pool/helpers.h>
#include <net/xdp.h>

#include "netfn_sk_offload_priv.h"

#define __is_xdp_vec_empty(gro_state) (!(gro_state)->xdp_q.nr_xdp)

/*
 * __rx_xdp_frag_count
 *	Return total number of buffers (head + frags) in an xdp_buff.
 */
static inline uint8_t __rx_xdp_frag_count(struct xdp_buff *xdp)
{
	uint8_t frags = 1;

	if (xdp_buff_has_frags(xdp)) {
		struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);
		frags += sinfo->nr_frags;
	}

	return frags;
}

/*
 * __rx_page_iperf_get_seq
 *      Read iperf sequence number from raw page buffer.
 */
static uint32_t __rx_page_iperf_get_seq(struct ppe_vp_xdp_cb_info *info)
{
	struct xdp_buff *xdp = info->xdp;
	struct iperf_hdr *hdr = (struct iperf_hdr *)((uint8_t *)xdp->data + sizeof(struct udphdr));

	return ntohl(hdr->seq);
}

/*
 * __rx_page_hw_check
 *      HW GRO coalesce check for XDP page pool path.
 */
static int __rx_page_hw_check(struct netfn_sk_offload_ctx *ctx,
		struct ppe_vp_xdp_cb_info *info, bool is_udp)
{
	bool gro_more = !!(info->mdata_info.hw_gro_flags & PPE_VP_RX_HW_GRO_MORE);
	bool gro_en = !!(info->mdata_info.hw_gro_flags & PPE_VP_RX_HW_GRO_EN);
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_stats_ctx *stats;
	uint8_t frag_cnt;

	stats = this_cpu_ptr(ctx->stats);

	/*
	 * If no buffs are accumulated yet, this is the first packet.
	 */
	if (__is_xdp_vec_empty(gro_state)) {
		if (likely(gro_en && gro_more)) {
			return NETFN_SK_OFFLOAD_RX_GRO_START;
		}

		stats->fail_start_coalesce++;
		return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
	}

	WARN_ON(!gro_en);

	/*
	 * Check if there is room for the incoming xdp_buff (and its frags) in the GRO chain.
	 */
	frag_cnt = __rx_xdp_frag_count(info->xdp);
	if (unlikely((gro_state->xdp_q.nr_scatters + frag_cnt) > NETFN_MAX_SKB_FRAGS)) {
		stats->fail_coalesce++;
		return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
	}

	/*
	 * For UDP, validate sequence number continuity.
	 */
	if (unlikely(is_udp) && unlikely(gro_state->udp.next_seq != __rx_page_iperf_get_seq(info))) {
		stats->fail_coalesce++;
		return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
	}

	/*
	 * gro_more == 0 means this is the last segment in the HW GRO chain.
	 */
	if (likely(!gro_more)) {
		return NETFN_SK_OFFLOAD_RX_GRO_FLUSH;
	}

	return NETFN_SK_OFFLOAD_RX_GRO_QUEUE;
}

/*
 * __rx_page_gro_state_init
 *      Initialise page pool GRO state with the first XDP buffer.
 */
static void __rx_page_gro_state_init(struct netfn_sk_offload_ctx *ctx,
		struct ppe_vp_xdp_cb_info *info)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_rx_xdp *xdp_vec = &gro_state->xdp_q;
	struct netfn_sk_offload_stats_ctx *stats;

	stats = this_cpu_ptr(ctx->stats);

	xdp_vec->vec[0] = *info->xdp;
	xdp_vec->vec[0].rxq = NULL;
	xdp_vec->nr_xdp = 1;
	xdp_vec->nr_scatters = __rx_xdp_frag_count(info->xdp);

	gro_state->head_cache.len = info->total_bytes;
	gro_state->head_cache.gso_segs = 1;

	stats->start_coalesce++;
}

/*
 * __rx_page_gro_queue
 *      Append xdp_buff copy into the accumulated GRO state.
 */
static void __rx_page_gro_queue(struct netfn_sk_offload_ctx *ctx,
		struct ppe_vp_xdp_cb_info *info)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_rx_xdp *xdp_vec = &gro_state->xdp_q;
	struct netfn_sk_offload_stats_ctx *stats;

	stats = this_cpu_ptr(ctx->stats);

	xdp_vec->vec[xdp_vec->nr_xdp] = *info->xdp;
	xdp_vec->vec[xdp_vec->nr_xdp].rxq = NULL;
	xdp_vec->nr_xdp++;
	xdp_vec->nr_scatters += __rx_xdp_frag_count(info->xdp);
	gro_state->head_cache.len += info->total_bytes;
	gro_state->head_cache.gso_segs++;

	stats->coalesce++;
}

/*
 * __rx_xdp_build_skb
 *      Generic helper: build an SKB from a single xdp_buff.
 *      Independent of netfn structures; handles xdp_frags shared_info.
 *      Protocol, network-header offsets, dev and pkt_type are left to the caller.
 */
static struct sk_buff *__rx_xdp_build_skb(struct xdp_buff *xdp)
{
	struct skb_shared_info *xdp_sinfo;
	unsigned int xdp_frags_size = 0;
	unsigned int nr_frags = 0;
	bool pfmemalloc = false;
	struct sk_buff *skb;

	/*
	 * Cache head's shared_info metadata BEFORE napi_build_skb() overwrites it.
	 */
	if (xdp_buff_has_frags(xdp)) {
		xdp_sinfo = xdp_get_shared_info_from_buff(xdp);
		nr_frags = xdp_sinfo->nr_frags;
		xdp_frags_size = xdp_sinfo->xdp_frags_size;
		pfmemalloc = xdp_buff_is_frag_pfmemalloc(xdp);
	}

	skb = napi_build_skb(xdp->data_hard_start, xdp->frame_sz);
	if (unlikely(!skb))
		return NULL;

	skb_mark_for_recycle(skb);

	skb_reserve(skb, xdp->data - xdp->data_hard_start);
	__skb_put(skb, xdp->data_end - xdp->data);

	if (nr_frags > 0) {
		xdp_update_skb_shared_info(skb, nr_frags,
					   xdp_frags_size,
					   (unsigned int)xdp->frame_sz * nr_frags,
					   pfmemalloc);
	}

	return skb;
}

/*
 * __rx_xdp_build_skb_frags
 *      Build an SKB from an array of xdp_buff copies and set netfn-specific fields.
 */
static struct sk_buff *__rx_xdp_build_skb_frags(struct netfn_sk_offload_ctx *ctx,
		struct xdp_buff xdp_vec[], uint8_t nr_xdp)
{
	struct sk_buff *skb;
	uint8_t i, j;

	skb = __rx_xdp_build_skb(&xdp_vec[0]);
	if (unlikely(!skb))
		return NULL;

	for (i = 1; i < nr_xdp; i++) {
		struct xdp_buff *xdp = &xdp_vec[i];
		struct page *page = virt_to_head_page(xdp->data_hard_start);
		unsigned int offset = xdp->data - page_address(page);
		unsigned int len = xdp->data_end - xdp->data;

		skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
				page, offset, len, xdp->frame_sz);

		if (xdp_buff_has_frags(xdp)) {
			struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);

			for (j = 0; j < sinfo->nr_frags; j++) {
				skb_frag_t *frag = &sinfo->frags[j];

				skb_add_rx_frag(skb, skb_shinfo(skb)->nr_frags,
						skb_frag_page(frag),
						skb_frag_off(frag),
						skb_frag_size(frag),
						xdp->frame_sz);
			}
		}
	}

	skb->protocol = ctx->etype;
	skb->pkt_type = PACKET_HOST;
	skb->ip_summed = CHECKSUM_UNNECESSARY;
	skb->dev = ctx->dev;
	skb->skb_iif = ctx->dev->ifindex;

	skb_set_network_header(skb, -ctx->iphdr_sz);
	skb_reset_transport_header(skb);

	return skb;
}

/*
 * __rx_page_reset_state
 *      Reset the page pool GRO state after flush or exception.
 */
static void __rx_page_reset_state(struct netfn_sk_offload_ctx *ctx)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;

	memset(&gro_state->xdp_q, 0, sizeof(gro_state->xdp_q));
	memset(&gro_state->head_cache, 0, sizeof(gro_state->head_cache));
}

/*
 * __rx_page_return_xdp
 *      Return XDP buffer and all its fragments to the page pool.
 */
static void __rx_page_return_xdp(struct xdp_buff *xdp)
{
	if (xdp_buff_has_frags(xdp)) {
		struct skb_shared_info *sinfo = xdp_get_shared_info_from_buff(xdp);
		int i;

		for (i = 0; i < sinfo->nr_frags; i++) {
			skb_frag_t *frag = &sinfo->frags[i];
			napi_pp_put_page(skb_frag_page(frag), false);
		}
	}

	if (xdp->data_hard_start) {
		napi_pp_put_page(virt_to_head_page(xdp->data_hard_start), false);
		xdp->data_hard_start = NULL;
	}
}

/*
 * __rx_page_purge_xdp
 *      Return all accumulated page pool buffers.
 */
static void __rx_page_purge_xdp(struct netfn_sk_offload_rx_xdp *xdp_vec)
{
	uint8_t i;

	for (i = 0; i < xdp_vec->nr_xdp; i++) {
		struct xdp_buff *xdp = &xdp_vec->vec[i];
		__rx_page_return_xdp(xdp);
	}

	xdp_vec->nr_xdp = 0;
	xdp_vec->nr_scatters = 0;
}

/*
 * __rx_page_except
 *      Exception a single page-built SKB to the Linux stack on dst miss.
 */
static void __rx_page_except(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct netfn_sk_offload_stats_ctx *stats = this_cpu_ptr(ctx->stats);
	struct net_device *dev;

	stats->dst_miss_pkts++;

	dev = dev_get_by_index(&init_net, skb->skb_iif);
	if (unlikely(!dev)) {
		pr_warn_ratelimited("%px: net dev not found for page SKB, freeing skb\n", skb);
		dev_kfree_skb_any(skb);
		return;
	}

	skb->dev = dev;
	skb_push(skb, -skb_network_offset(skb));
	netif_receive_skb(skb);
	dev_put(dev);
}

/*
 * __rx_page_deliver
 *      Deliver a page-built SKB to the network stack via dst routing.
 */
static void __rx_page_deliver(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct netfn_sk_offload_stats_ctx *stats = this_cpu_ptr(ctx->stats);
	uint8_t ip_proto = ctx->tuple.tuples.tuple_5.protocol;
	struct dst_entry *dst = __rx_get_dst(ctx, skb);

	if (!dst) {
		__rx_page_except(ctx, skb);
		return;
	}

	skb_dst_set_noref(skb, dst);

	/* Track scatter-gather fragments delivered: 1 (linear) + nr_frags */
	stats->rx_scatter_delivered += 1 + skb_shinfo(skb)->nr_frags;

	if (skb->protocol == htons(ETH_P_IP)) {
		struct net_protocol *proto4 = rcu_dereference(inet_protos[ip_proto]);
		iph_set_totlen(ip_hdr(skb), skb->len + ip_hdrlen(skb));
		proto4->handler(skb);
	} else {
		struct inet6_protocol *proto6 = rcu_dereference(inet6_protos[ip_proto]);
		ipv6_hdr(skb)->payload_len = htons(skb->len);
		proto6->handler(skb);
	}
}

/*
 * __rx_page_gro_flush
 *      Build SKB from accumulated xdp_buff copies and deliver to the network stack.
 */
static void __rx_page_gro_flush(struct netfn_sk_offload_ctx *ctx)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	uint8_t ip_proto = ctx->tuple.tuples.tuple_5.protocol;
	struct netfn_sk_offload_stats_ctx *stats;
	uint8_t nr_xdp = gro_state->xdp_q.nr_xdp;
	struct sk_buff *skb;

	stats = this_cpu_ptr(ctx->stats);

	skb = __rx_xdp_build_skb_frags(ctx, gro_state->xdp_q.vec, nr_xdp);
	if (unlikely(!skb)) {
		stats->fail_skb_build++;
		__rx_page_purge_xdp(&gro_state->xdp_q);
		__rx_page_reset_state(ctx);
		return;
	}

	if (ip_proto == IPPROTO_TCP) {
		struct tcphdr *tcph = tcp_hdr(skb);

		tcp_flag_word(tcph) |= gro_state->tcp.flag_word;
	} else {
		struct udphdr *udph = udp_hdr(skb);

		skb_shinfo(skb)->gso_type = SKB_GSO_UDP_L4;
		skb_shinfo(skb)->gso_segs = gro_state->head_cache.gso_segs;
		skb_shinfo(skb)->gso_size = gro_state->udp.len - sizeof(*udph);
		udph->len = htons(skb->len);
	}
	__rx_page_deliver(ctx, skb);
	__rx_page_reset_state(ctx);
}

/*
 * __rx_page_tcp
 *      HW GRO TCP processing for XDP page pool packets.
 */
void __rx_page_tcp(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_xdp_cb_info *info)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct tcphdr *tcph = (struct tcphdr *)info->xdp->data;
	struct netfn_sk_offload_stats_ctx *ctx_stats;
	struct xdp_buff *xdp = info->xdp;
	uint32_t hlen = tcph->doff * 4;
	bool is_udp = false;
	struct sk_buff *skb;
	int gro_action;

	gro_action = __rx_page_hw_check(ctx, info, is_udp);
	switch (gro_action) {
	case NETFN_SK_OFFLOAD_RX_GRO_START:
		/*
		 * We shouldn't be holding any chain.
		 */
		WARN_ON(!__is_xdp_vec_empty(gro_state));

		gro_state->tcp.hlen = hlen;
		gro_state->tcp.flag_word = tcp_flag_word(tcph);

		__rx_page_gro_state_init(ctx, info);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_QUEUE:
		/*
		 * Pull TCP header before queueing payload.
		 */
		xdp->data += hlen;
		info->total_bytes -= hlen;

		__rx_page_gro_queue(ctx, info);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_FLUSH:
		uint32_t hw_gro_flags = info->mdata_info.hw_gro_flags;

		xdp->data += hlen;
		info->total_bytes -= hlen;

		if (hw_gro_flags & PPE_VP_RX_HW_GRO_TCP_PSH)
			gro_state->tcp.flag_word |= TCP_FLAG_PSH;
		if (hw_gro_flags & PPE_VP_RX_HW_GRO_TCP_FIN)
			gro_state->tcp.flag_word |= TCP_FLAG_FIN;

		__rx_page_gro_queue(ctx, info);
		__rx_page_gro_flush(ctx);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_SKIP:
		/*
		 * If we have active GRO chain, flush it.
		 */
		if (!__is_xdp_vec_empty(gro_state)) {
			__rx_page_gro_flush(ctx);
		}

		/*
		 * Packet can't be GRO'd, deliver as it is.
		 */
		skb = __rx_xdp_build_skb_frags(ctx, info->xdp, 1);
		if (unlikely(!skb)) {
			ctx_stats = this_cpu_ptr(ctx->stats);
			ctx_stats->fail_skb_build++;
			__rx_page_return_xdp(info->xdp);
			break;
		}

		__rx_page_deliver(ctx, skb);
		break;
	}
}

/*
 * __rx_page_udp
 *      HW GRO UDP processing for XDP page pool packets.
 */
void __rx_page_udp(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_xdp_cb_info *info)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_stats_ctx *ctx_stats;
	struct xdp_buff *xdp = info->xdp;
	struct sk_buff *skb;
	bool is_udp = true;
	int gro_action;

	gro_action = __rx_page_hw_check(ctx, info, is_udp);
	switch (gro_action) {
	case NETFN_SK_OFFLOAD_RX_GRO_START:
		/*
		 * We shouldn't be holding any chain.
		 */
		WARN_ON(!__is_xdp_vec_empty(gro_state));

		gro_state->udp.len = (uint16_t)info->total_bytes;
		gro_state->udp.next_seq = __rx_page_iperf_get_seq(info) + 1;
		__rx_page_gro_state_init(ctx, info);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_QUEUE:
		gro_state->udp.next_seq = __rx_page_iperf_get_seq(info) + 1;

		xdp->data += sizeof(struct udphdr);
		info->total_bytes -= sizeof(struct udphdr);

		__rx_page_gro_queue(ctx, info);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_FLUSH:
		gro_state->udp.next_seq = __rx_page_iperf_get_seq(info) + 1;

		xdp->data += sizeof(struct udphdr);
		info->total_bytes -= sizeof(struct udphdr);

		__rx_page_gro_queue(ctx, info);
		__rx_page_gro_flush(ctx);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_SKIP:
		/*
		 * If we have active GRO chain, flush it.
		 */
		if (!__is_xdp_vec_empty(gro_state)) {
			__rx_page_gro_flush(ctx);
		}

		/*
		 * Packet can't be GRO'd, deliver as it is.
		 */
		skb = __rx_xdp_build_skb_frags(ctx, info->xdp, 1);
		if (unlikely(!skb)) {
			ctx_stats = this_cpu_ptr(ctx->stats);
			ctx_stats->fail_skb_build++;
			__rx_page_return_xdp(info->xdp);
			break;
		}

		__rx_page_deliver(ctx, skb);
		break;
	}
}

/*
 * netfn_sk_offload_rx_page
 *      VP XDP page pool callback - entry point for XDP-based HW GRO packets.
 */
bool netfn_sk_offload_rx_page(struct ppe_vp_xdp_cb_info *info, void *cb_data)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_stats_ctx *stats;
	struct netfn_sk_offload_stats *so_stats;
	struct netfn_sk_offload_ctx *ctx = NULL;
	uint32_t flow_idx = info->flow_idx;
	struct xdp_buff *xdp = info->xdp;
	struct xdp_rxq_info *rxq;

	rcu_read_lock_bh();

	rxq = xdp->rxq;
	so_stats = this_cpu_ptr(so->stats);

	/*
	 * Warn and drop if the XDP buffer was not allocated from a page pool.
	 * All buffers on this VP XDP path are expected to be page pool pages.
	 */
	if (unlikely(rxq->mem.type != MEM_TYPE_PAGE_POOL)) {
		WARN_ONCE(1, "%px: XDP buffer not from page pool, mem_type: %u\n",
			  xdp, rxq->mem.type);
		goto drop;
	}

	ctx = netfn_sk_offload_db_get_ctx(flow_idx);
	if (!ctx) {
		so_stats->rx_fail_ctx++;
		goto drop;
	}

	stats = this_cpu_ptr(ctx->stats);

	so_stats->rx_pkts++;
	so_stats->rx_bytes += info->total_bytes;
	so_stats->hw_gro_rx_pkts++;
	so_stats->hw_gro_rx_bytes += info->total_bytes;
	stats->rx_pkts++;
	stats->rx_bytes += info->total_bytes;
	stats->hw_gro_rx_pkts++;
	stats->hw_gro_rx_bytes += info->total_bytes;

	/* Track scatter-gather fragments: 1 (linear) + nr_frags (scatter) */
	stats->rx_scatter_pkts += __rx_xdp_frag_count(xdp);

	/*
	 * Strip IP header before delivering to GRO logic.
	 */
	if (unlikely((unsigned int)(xdp->data_end - xdp->data) < ctx->iphdr_sz)) {
		goto drop;
	}

	xdp->data += ctx->iphdr_sz;
	info->total_bytes -= ctx->iphdr_sz;

	if (!ctx->gro_rcv_pages) {
		goto drop;
	}

	ctx->gro_rcv_pages(ctx, info);
	rcu_read_unlock_bh();

	return true;

drop:
	if (ctx && !__is_xdp_vec_empty(&ctx->gro_state)) {
		__rx_page_gro_flush(ctx);
	}

	so_stats->pkts_dropped++;
	__rx_page_return_xdp(xdp);

	rcu_read_unlock_bh();
	return true;
}
