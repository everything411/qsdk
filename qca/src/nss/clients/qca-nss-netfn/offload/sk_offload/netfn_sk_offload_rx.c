/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_sk_offload_rx.c
 *      Network Sock Offload Receive Processing.
 */
#include <linux/ip.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <net/protocol.h>
#include <net/udp.h>
#include <net/gro.h>

#include "netfn_sk_offload_priv.h"


#define __is_cache_empty(gro_state) (!((gro_state)->head_cache.skb))
#define UDP_GRO_MAX_LEN (64 * 1024)
#define UDP_GRO_MAX_SEGS 64

static uint32_t iperf_get_seq(struct sk_buff *skb)
{
	struct iperf_hdr *hdr = (struct iperf_hdr *)(skb_transport_header(skb) + sizeof(struct udphdr));

	return ntohl(hdr->seq);
}

/*
 * __rx_gro_udp_check
 *	Coalesce check for UDP
 */
static enum netfn_sk_offload_rx_action __rx_gro_udp_check(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_stats_ctx *stats;
	uint32_t skip_merge = 0, udp_len;
	struct udphdr *udph;

	udph = udp_hdr(skb);
	udp_len = ntohs(udph->len);

	stats = this_cpu_ptr(ctx->stats);

	if (__is_cache_empty(gro_state)) {
		/*
		 * Don't Coalesce if checksum is not verified by EDMA
		 * or no data is present.
		 */
		skip_merge += !!(skb->ip_summed ^ CHECKSUM_UNNECESSARY);
		skip_merge += !!(skb->len <= sizeof(*udph));

		if (skip_merge) {
			stats->fail_start_coalesce++;
			return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
		}

		return NETFN_SK_OFFLOAD_RX_GRO_START;
	}

	/*
	 * Dont coalesce if there is payload length mismatch
	 * or sequence number mismatch
	 * or total length will exceed the maximum buffer length
	 * or if checksum is not verified by EDMA
	 * or no data is present
	 */
	skip_merge += !!(udp_len != gro_state->udp.len);

	skip_merge += !!(gro_state->udp.next_seq != iperf_get_seq(skb));
	skip_merge += !!(skb->ip_summed ^ CHECKSUM_UNNECESSARY);
	skip_merge += !!((gro_state->head_cache.len + skb->len) > UDP_GRO_MAX_LEN);

	if (skip_merge) {
		stats->fail_coalesce++;
		return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
	}

	/*
	 * Check the max limit of segments including the head and current SKB
	 */
	if ((skb_queue_len(&gro_state->frag_q) + 2) == UDP_GRO_MAX_SEGS) {
		return NETFN_SK_OFFLOAD_RX_GRO_FLUSH;
	}

	return NETFN_SK_OFFLOAD_RX_GRO_QUEUE;
}

/*
 * __rx_gro_tcp_check
 *	Coalesce check for TCP
 */
static enum netfn_sk_offload_rx_action __rx_gro_tcp_check(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_stats_ctx *stats;
	struct tcphdr *tcph = tcp_hdr(skb);
	uint32_t skip_merge = 0;
	uint32_t hlen;
	__be32 flags;

	stats = this_cpu_ptr(ctx->stats);
	flags = tcp_flag_word(tcph);
	hlen = tcph->doff * 4;

	if (__is_cache_empty(gro_state)) {
		/*
		 * Don't Coalesce if flag other then ACK is set,
		 * or checksum is not verified by EDMA.
		 * or this is pure ack (No data is present).
		 */
		skip_merge += !!((tcp_flag_word(tcph) & htonl(GENMASK(23,16))) & ~TCP_FLAG_ACK);
		skip_merge += !!(skb->ip_summed ^ CHECKSUM_UNNECESSARY);
		skip_merge += !!(skb->len <= hlen);

		if (!skip_merge) return NETFN_SK_OFFLOAD_RX_GRO_START;

		stats->fail_start_coalesce++;
		return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
	}

#define TCP_SKIP_FLAGS ~((__be32)(TCP_FLAG_FIN | TCP_FLAG_PSH))

	/*
	 * TCP Flags must be the same as previous, Allow coalesce for FIN & PSH.
	 */
	skip_merge += !!((flags ^ gro_state->tcp.flag_word) & TCP_SKIP_FLAGS);

	/*
	 * TCP sequence check.
	 */
	skip_merge += !!(tcph->ack_seq ^ gro_state->tcp.ack_seq);
	skip_merge += !!(ntohl(tcph->seq) ^ gro_state->tcp.next_seq);

	/*
	 * Data offset has to be same.
	 */
	skip_merge += !!(hlen ^ gro_state->tcp.hlen);

	/*
	 * Checksum and payload check;
	 * If, we can't coalesce the new SKB then flush the previous list
	 * and Deliver current packet without GRO
	 */
	skip_merge += !!(skb->ip_summed ^ CHECKSUM_UNNECESSARY);
	skip_merge += !!(skb->len <= hlen);
	if (skip_merge) {
		stats->fail_coalesce++;
		return NETFN_SK_OFFLOAD_RX_GRO_SKIP;
	}

	/*
	 * If FIN and PSH was set, then end the chain with this packet.
	 */
	return !!(flags & ~TCP_SKIP_FLAGS) ? NETFN_SK_OFFLOAD_RX_GRO_FLUSH : NETFN_SK_OFFLOAD_RX_GRO_QUEUE;
}

/*
 * __rx_except
 *      Exception the packets to Linux
 */
void __rx_except(struct netfn_sk_offload_ctx *ctx, struct sk_buff_head *head)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct net_device *dev = NULL;
	struct sk_buff *tmp, *skb;

	/*
	 * De-queue all packets from the frag queue and notify Linux
	 */
	skb_queue_walk_safe(head, skb, tmp) {
		__skb_unlink(skb, head);

		/*
		 * Fetch the netdev from skb_iif and use it to set skb dev.
		 */
		dev = dev_get_by_index(&init_net, skb->skb_iif);
		if (!dev) {
			pr_warn_ratelimited("%px: net dev not found for SKB\n", skb);
			return;
		}

		skb->dev = dev;

		/*
		 * Reset the headers and protocol
		 */
		skb_push(skb, -skb_network_offset(skb));

		netif_receive_skb(skb);
		dev_put(dev);
	}

	/*
	 * Reset local state.
	 */
	memset(gro_state, 0, sizeof(*gro_state));
	skb_queue_head_init(&gro_state->frag_q);
}

/*
 * __rx_get_dst
 *	Fetch the dst from socket
 */
struct dst_entry *__rx_get_dst(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct dst_entry *dst;
	struct sock *sk = NULL;

	sk = ctx->sk;
	dst = rcu_dereference(sk->sk_rx_dst);

	/*
	 * Check if dst cache is obsolete
	 */
	return dst ? dst_check(dst, sk->sk_rx_dst_cookie) : NULL;

}

/*
 * __rx_gro_flush
 *	Flush the SKB
 */
static void __rx_gro_flush(struct netfn_sk_offload_ctx *ctx)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct sk_buff *first_frag = skb_peek(&gro_state->frag_q);
	uint8_t ip_proto = ctx->tuple.tuples.tuple_5.protocol;
	struct sk_buff **p_skb, *iter, *h_skb;
	struct dst_entry *dst;
	uint32_t head_len;

	h_skb =	gro_state->head_cache.skb;
	BUG_ON(!h_skb);

	head_len = h_skb->len;

	dst = __rx_get_dst(ctx, h_skb);
	if (!dst) {
		struct netfn_sk_offload_stats_ctx *stats;

		stats = this_cpu_ptr(ctx->stats);
		stats->dst_miss_pkts++;

		/*
		 * Link the first SKB with the frag chain
		 */
		__skb_queue_head(&gro_state->frag_q, h_skb);
		__rx_except(ctx, &gro_state->frag_q);
		return;
	}

	skb_dst_set_noref(h_skb, dst);

	/*
	 * If this is GRO'd SKB, We need to update head SKB.
	 * - First SKB's original length will become head length
	 * - First SKB length will be total length.
	 * - All coalesced SKB length will become data length.
	 */
	p_skb = &skb_shinfo(h_skb)->frag_list;

	h_skb->len = gro_state->head_cache.len;
	h_skb->truesize = gro_state->head_cache.truesize;
	h_skb->data_len = gro_state->head_cache.len - head_len;

	/*
	 * We may receive jumbo packet as frag_list for head SKB itself.
	 * Hence, Add Gro'd SKB to end of frag_list
	 */
	skb_walk_frags(h_skb, iter) {
		if (!iter->next) {
			p_skb = &iter->next;
			break;
		}
	}

	if (!skb_queue_empty(&gro_state->frag_q)) {
		struct sk_buff *last_frag = gro_state->frag_q.prev;

		first_frag->prev = NULL;
		last_frag->next = NULL;
		*p_skb = first_frag;
	}

	/*
	 * Reset local state.
	 */
	memset(gro_state, 0, sizeof(*gro_state));
	skb_queue_head_init(&gro_state->frag_q);

	/*
	 * Correct headers.
	 * We are not correcting checksum as we are directly giving SKB to TCP/UDP Protocol stack.
	 */
	if (h_skb->protocol == htons(ETH_P_IP)) {
		struct net_protocol *proto4 = rcu_dereference(inet_protos[ip_proto]);
		iph_set_totlen(ip_hdr(h_skb), h_skb->len + ip_hdrlen(h_skb));
		proto4->handler(h_skb);
	} else {
		struct inet6_protocol *proto6 = rcu_dereference(inet6_protos[ip_proto]);
		ipv6_hdr(h_skb)->payload_len = htons(h_skb->len);
		proto6->handler(h_skb);
	}
}

/*
 * __rx_gro_purge
 *	Clear the GRO list
 */
void __rx_gro_purge(struct netfn_sk_offload_ctx *ctx)
{
	struct netfn_sk_offload_rx_state *gro_state;
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_stats *stats;
	struct sk_buff *h_skb;
	int count;

	stats = this_cpu_ptr(so->stats);

	gro_state = &ctx->gro_state;
	h_skb = gro_state->head_cache.skb;

	/*
	 * If, nothing is present in GRO queue, return
	 */
	if (!h_skb) return;

	__skb_queue_head(&gro_state->frag_q, h_skb);
	count = skb_queue_len(&gro_state->frag_q);
	stats->pkts_dropped += count;

	pr_warn_ratelimited("%px: Socket already destroyed, purge remaining packet(%d)\n", ctx, count);

	__skb_queue_purge(&gro_state->frag_q);

	memset(gro_state, 0, sizeof(*gro_state));
	skb_queue_head_init(&gro_state->frag_q);
}

/*
 * __rx_tcp_completion
 *      NAPI completion callback to
 *      flush the GROed TCP packets
 */
static void __rx_tcp_completion(void *info)
{
	struct netfn_sk_offload_rx_state *gro_state;
	struct netfn_sk_offload_ctx *ctx = info;
	struct netfn_sk_offload_stats_ctx *stats;
	struct sk_buff *skb;
	struct tcphdr *tcph;

	stats = this_cpu_ptr(ctx->stats);
	gro_state = &ctx->gro_state;

	/*
	 * We may have flushed the head before
	 * NAPI completion (i.e TCP PSH)
	 */
	if (__is_cache_empty(gro_state)) {
		return;
	}

	skb = gro_state->head_cache.skb;

	tcph = tcp_hdr(skb);
	tcp_flag_word(tcph) |= gro_state->tcp.flag_word;

	stats->napi_flush++;
	__rx_gro_flush(ctx);
}

/*
 * __rx_udp_completion
 *      NAPI completion callback to
 *      flush the GROed UDP packets
 */
static void __rx_udp_completion(void *info)
{
	struct netfn_sk_offload_rx_state *gro_state;
	struct netfn_sk_offload_ctx *ctx = info;
	struct netfn_sk_offload_stats_ctx *stats;
	struct sk_buff *skb;
	struct udphdr *udph;

	gro_state = &ctx->gro_state;

	/*
	 * We maye have flushed the head before
	 * NAPI completion
	 */
	if (__is_cache_empty(gro_state)) {
		return;
	}

	stats = this_cpu_ptr(ctx->stats);
	skb = gro_state->head_cache.skb;

	/*
	 * Update GSO fields
	 */
	skb_shinfo(skb)->gso_type |= SKB_GSO_UDP_L4;
	skb_shinfo(skb)->gso_size = gro_state->udp.len - sizeof(*udph);
	skb_shinfo(skb)->gso_segs = gro_state->head_cache.gso_segs;

	/*
	 * Correct udp payload len
	 */
	udph = udp_hdr(skb);
	udph->len = htons(gro_state->head_cache.len);

	stats->napi_flush++;
	__rx_gro_flush(ctx);

}

/*
 * __rx_deliver
 *	Deliver the NON GRO packet
 */
static void __rx_deliver(struct netfn_sk_offload_ctx *ctx,  struct sk_buff *skb)
{
	uint8_t ip_proto = ctx->tuple.tuples.tuple_5.protocol;
	struct dst_entry *dst = __rx_get_dst(ctx, skb);

	if (!dst) {
		struct netfn_sk_offload_stats_ctx *stats;
		struct sk_buff_head head;

		skb_queue_head_init(&head);
		__skb_queue_tail(&head, skb);

		stats = this_cpu_ptr(ctx->stats);
		stats->dst_miss_pkts++;
		__rx_except(ctx, &head);
		return;
	}

	skb_dst_set_noref(skb, dst);

	/*
	 * Hand over to linux
	 */
	if (skb->protocol == htons(ETH_P_IP)) {
		struct net_protocol *proto4 = rcu_dereference(inet_protos[ip_proto]);
		proto4->handler(skb);
	} else {
		struct inet6_protocol *proto6 = rcu_dereference(inet6_protos[ip_proto]);
		proto6->handler(skb);
	}
}

/*
 * __rx_gro_queue
 *	Merge SKB into current GRO state
 */
static void __rx_gro_queue(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_stats_ctx *stats;

	stats = this_cpu_ptr(ctx->stats);

	/*
	 * Update the head cache for the current fragment
	 */
	gro_state->head_cache.len += skb->len;
	gro_state->head_cache.truesize +=  skb->truesize;
	gro_state->head_cache.gso_segs++;

	__skb_queue_tail(&gro_state->frag_q, skb);
	stats->coalesce++;

}

/*
 * __rx_udp_gro_flush
 *	Update UDP headers and Flush the SKB
 */
static void __rx_udp_gro_flush(struct netfn_sk_offload_ctx *ctx)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct udphdr *udph;

	udph = udp_hdr(gro_state->head_cache.skb);

	/*
	 * End the current chain and Flush it to Linux
	 * Correct GRO state for Linux
	 * Correct UDP header length.
	 */
	skb_shinfo(gro_state->head_cache.skb)->gso_type |= SKB_GSO_UDP_L4;
	skb_shinfo(gro_state->head_cache.skb)->gso_size = gro_state->udp.len - sizeof(*udph);
	skb_shinfo(gro_state->head_cache.skb)->gso_segs = gro_state->head_cache.gso_segs;

	udph->len = htons(gro_state->head_cache.len);
	__rx_gro_flush(ctx);
}


/*
 * __rx_gro_state_init
 *	Initialise GRO head
 */
static void __rx_gro_state_init(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct netfn_sk_offload_stats_ctx *stats;

	stats = this_cpu_ptr(ctx->stats);

	/*
	 * Notify NAPI completion for SW GRO packets.
	 */
	dev_napi_notify_completion(&ctx->napi_info);

	/*
	 * Reset GRO field
	 */
	*(u32 *)&NAPI_GRO_CB(skb)->zeroed = 0;

	/*
	 * Reset our local state for coalesce,
	 */
	skb_queue_head_init(&gro_state->frag_q);

	/* Save the current SKB as the new head */
	gro_state->head_cache.len = skb->len;
	gro_state->head_cache.truesize = skb->truesize;
	gro_state->head_cache.skb = skb;
	gro_state->head_cache.gso_segs = 1;

	stats->start_coalesce++;
}

/*
 * __rx_tcp
 *	Perform SW GRO on TCP packets
 */
static void __rx_tcp(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_cb_info *info)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct sk_buff *skb = info->skb;
	struct tcphdr *tcph = tcp_hdr(skb);
	int gro_action;
	uint32_t hlen;
	__be32 flags;

	hlen = tcph->doff * 4;
	flags = tcp_flag_word(tcph);

	gro_action = __rx_gro_tcp_check(ctx, skb);

	switch (gro_action) {
	case NETFN_SK_OFFLOAD_RX_GRO_START:
		/*
		 * We shouldn't be holding any chain.
		 */
		WARN_ON(!__is_cache_empty(gro_state));

		/*
		 * Start a new chain with received packet.
		 * Update TCP specific data in gro_state
		 */
		gro_state->tcp.hlen = hlen;
		gro_state->tcp.flag_word = flags;
		gro_state->tcp.ack_seq = tcph->ack_seq;
		gro_state->tcp.next_seq = ntohl(tcph->seq) + (skb->len - hlen);

		__rx_gro_state_init(ctx, skb);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_QUEUE:
		/*
		 * We are going to coalesce data, Pull TCP header as well.
		 */
		__skb_pull(skb, hlen);

		/*
		 * Update GRO state for next packet.
		 */
		gro_state->tcp.flag_word |= (flags & (TCP_FLAG_FIN | TCP_FLAG_PSH));
		gro_state->tcp.next_seq += skb->len;

		/*
		 * Queue the packet to existing chain and continue.
		 */
		__rx_gro_queue(ctx, skb);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_FLUSH:
		/*
		 * We are going to coalesce data, Pull TCP header as well.
		 */
		__skb_pull(skb, hlen);

		/*
		 * Save TCP header state
		 */
		gro_state->tcp.flag_word |= (flags & (TCP_FLAG_FIN | TCP_FLAG_PSH));
		gro_state->tcp.next_seq += skb->len;

		/*
		 * Queue the packet to existing chain and Flush it to Linux.
		 */
		__rx_gro_queue(ctx, skb);

		/*
		 * Restore the TCP flags in Head SKB.
		 * (PSH/FIN Packet could have modified it).
		 */
		tcph = tcp_hdr(gro_state->head_cache.skb);
		tcp_flag_word(tcph) |= gro_state->tcp.flag_word;

		__rx_gro_flush(ctx);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_SKIP:
	      	/*
		 * If we have active GRO chain, flush it
		 */
		if (!__is_cache_empty(gro_state))  {
			tcph = tcp_hdr(gro_state->head_cache.skb);
			tcp_flag_word(tcph) |= gro_state->tcp.flag_word;
			__rx_gro_flush(ctx);
		}

		/*
		 * Packet can't be GRO'd deliver as it is
		 */
		__rx_deliver(ctx, skb);
	}
}

/*
 * __rx_udp
 *      Perform SW GRO on UDP packets
 */
static void __rx_udp(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_cb_info *info)
{
	struct netfn_sk_offload_rx_state *gro_state = &ctx->gro_state;
	struct sk_buff *skb = info->skb;
	struct udphdr *udph;
	int gro_action;

	udph = udp_hdr(skb);
	gro_action = __rx_gro_udp_check(ctx, skb);

	switch (gro_action) {
	case NETFN_SK_OFFLOAD_RX_GRO_START:
		/*
		 * We shouldn't be holding any chain.
		 */
		WARN_ON(!__is_cache_empty(gro_state));

		/*
		 * Start a new chain with received packet.
		 * Update UDP specific data in gro_state
		 */
		gro_state->udp.len = ntohs(udph->len);
		gro_state->udp.next_seq = iperf_get_seq(skb) + 1;

		__rx_gro_state_init(ctx, skb);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_QUEUE:
		/*
		 * We are going to coalesce data, Pull UDP header as well.
		 */
		__skb_pull(skb, sizeof(*udph));

		/*
		 * Update GRO state for next packet.
		 */
		gro_state->udp.next_seq = iperf_get_seq(skb) + 1;

		/*
		 * Queue the packet to existing chain and continue.
		 */
		__rx_gro_queue(ctx, skb);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_FLUSH:
		__skb_pull(skb, sizeof(*udph));
		gro_state->udp.next_seq = iperf_get_seq(skb) + 1;
		__rx_gro_queue(ctx, skb);
		__rx_udp_gro_flush(ctx);
		break;

	case NETFN_SK_OFFLOAD_RX_GRO_SKIP:
		/*
		 * If we have active GRO chain, flush it.
		 */
		if (!__is_cache_empty(gro_state))  {
			/*
			 * Update head SKB and flush SKB
			 */
			__rx_udp_gro_flush(ctx);
		}

		/*
		 * Packet can't be GRO'd deliver as it is
		 */
		__rx_deliver(ctx, skb);
	}
}

/*
 * netfn_sk_offload_rx()
 *      Netfn Sock Offload Rx processing
 */
bool netfn_sk_offload_rx(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_ctx *ctx = NULL;
	struct netfn_sk_offload_stats_ctx *stats;
	struct netfn_sk_offload_stats *so_stats;
	uint16_t flow_idx = info->flow_idx;
	struct sk_buff *skb = info->skb;

	rcu_read_lock_bh();

	so_stats = this_cpu_ptr(so->stats);

	/*
	 * Fetch the ctx using flow entry
	 */
	ctx = netfn_sk_offload_db_get_ctx(flow_idx);
	if (!ctx) {
		pr_warn_ratelimited("%d: Connection already destroyed\n", flow_idx);
		so_stats->rx_fail_ctx++;
		rcu_read_unlock_bh();
		goto drop;
	}

	skb->protocol = ctx->etype;

	/*
	 * Set skb fields
	 */
	skb->pkt_type = PACKET_HOST;

	stats = this_cpu_ptr(ctx->stats);

	/*
	 * Update the global and context stats
	 */
	so_stats->rx_pkts++;
	so_stats->rx_bytes += skb->len;
	stats->rx_pkts++;
	stats->rx_bytes += skb->len;

	/*
	 * Pull IP header as packet will be delivered to TCP stack.
	 */
	skb_reset_network_header(skb);
	__skb_pull(skb, ctx->iphdr_sz);
	skb_reset_transport_header(skb);

	if (!ctx->gro_rcv) {
		rcu_read_unlock_bh();
		goto drop;
	}

	ctx->gro_rcv(ctx, info);
	rcu_read_unlock_bh();

	return true;

drop:
	so_stats->pkts_dropped++;
	dev_kfree_skb(skb);
	return true;
}

/*
 * netfn_sk_offload_rx_init()
 *      Netfn Sock Offload Rx Initialization
 */
void netfn_sk_offload_rx_init(struct netfn_sk_offload_ctx *ctx)
{
	void *napi_notify, *gro_rcv;
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	void *gro_rcv_pages;
#endif
	struct sock *sk = ctx->sk;

	ctx->iphdr_sz = (sk->sk_family == AF_INET) ? sizeof(struct iphdr) : sizeof(struct ipv6hdr);
	ctx->etype = (sk->sk_family == AF_INET) ? htons(ETH_P_IP) : htons(ETH_P_IPV6);

	switch (sk->sk_protocol) {
	case IPPROTO_TCP:
		gro_rcv = __rx_tcp;
#ifdef NETFN_SK_OFFLOAD_HW_GRO
		gro_rcv_pages = __rx_page_tcp;
#endif
		napi_notify = __rx_tcp_completion;
		break;
	case IPPROTO_UDP:
		gro_rcv = __rx_udp;
#ifdef NETFN_SK_OFFLOAD_HW_GRO
		gro_rcv_pages = __rx_page_udp;
#endif
		napi_notify = __rx_udp_completion;
		break;
	default:
		WARN_ON(1);
		return;
	}

	ctx->gro_rcv = gro_rcv;
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	ctx->gro_rcv_pages = gro_rcv_pages;
#endif

	/*
	 * Initialise the NAPI completion callback
	 */
	napi_notifier_info_init(&ctx->napi_info, napi_notify, ctx);
}
