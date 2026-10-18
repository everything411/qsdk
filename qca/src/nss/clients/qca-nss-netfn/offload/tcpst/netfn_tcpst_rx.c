/*
 * netfn_tcpst_rx.c
 *	Netfn TCP Speedtest receive file
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <linux/version.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <net/gro.h>

#include "netfn_tcpst_priv.h"

/*
 * netfn_tcpst_rx_timeout()
 *	Handles rx test timeout and offset
 */
static enum hrtimer_restart netfn_tcpst_rx_timeout(struct hrtimer *timer)
{
	struct netfn_tcpst_conn *conn = container_of(timer, struct netfn_tcpst_conn, hrtimer);
	struct netfn_tcpst_stats_ext *ext_stats = &conn->ext_stats;
	struct netfn_tcpst_stats *stats = &conn->stats;

	/*
	 * We take the bytes sample at 2 instances:
	 * 1. When the offset timer expired
	 * 2. When the duration timer expired
	 */
	netfn_tcpst_update_eth_stats(conn->eth_dev, stats);
	stats->test_bytes.rcvd = (ext_stats->test_rx_bytes - stats->test_bytes.rcvd);

	pr_debug("%px: Timer expired (offset(%d) duration (%d) state(0x%lx)\n", conn, conn->offset, conn->duration, conn->timer_state);

	/*
	 * Check if offset timer expired
	 */
	if (test_and_clear_bit(NETFN_TCPST_TIMER_FWD_BIT, &conn->timer_state)) {
		stats->test.eom_time = ktime_get_real_ns();
		set_bit(NETFN_TCPST_TIMER_COMP_BIT, &conn->timer_state);
		return HRTIMER_NORESTART;
	}

	/*
	 * Set the offset timer bit and forward the timer for duration time
	 */
	set_bit(NETFN_TCPST_TIMER_FWD_BIT, &conn->timer_state);
	hrtimer_forward_now(timer, ktime_set(conn->duration, 0));
	return HRTIMER_RESTART;
}

/*
 * netfn_tcpst_rx_free()
 *	Free the SKB.
 */
static int netfn_tcpst_rx_free(struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *segs = skb_shinfo(skb)->frag_list;
	int len = skb->len;

	/*
	 * Release all socket state like destructor, socket memory.
	 */
	tcp_eat_skb(sk, skb);
	skb_orphan(skb);

	/*
	 * Recyler doesn't recycle SKB with frag_list, So de-link the list here
	 * to recycle the SKBs.
	 */
	skb_shinfo(skb)->frag_list = NULL;
	while (segs) {
		struct sk_buff *cur = segs;
		segs = segs->next;

		cur->next =  NULL;
		consume_skb(cur);
	}

	consume_skb(skb);
	return len;
}

/*
 * netfn_tcpst_rx_except)
 *	Exception the packets to Linux
 */
static void netfn_tcpst_rx_except(struct netfn_tcpst_conn *conn, struct sk_buff *skb)
{
	struct netfn_tcpst_gro_state *gro_state = &conn->gro_state;

	/*
	 * Link the first SKB with the chain
	 */
	skb->next = gro_state->first_frag;

	do {
		uint32_t proto = skb->protocol;
		struct sk_buff *next;

		next = skb->next;
		skb->next = NULL;
		skb->dev = conn->eth_dev;

		/*
		 * Reset the headers and protocol
		 */
		skb_push(skb, skb_network_header_len(skb));
		if (proto == htons(ETH_P_MAP)) {
			skb->protocol = gro_state->protocol;
		}

		netif_receive_skb(skb);
		skb = next;

	} while (skb);

	*gro_state = (struct netfn_tcpst_gro_state){0};
}

/*
 * netfn_tcpst_rx_conn()
 *	Flush the packets to linux TCP stack
 */
static inline void netfn_tcpst_rx_conn(struct netfn_tcpst_conn *conn, struct sk_buff *skb)
{
	struct netfn_tcpst_gro_state *gro_state = &conn->gro_state;
	struct inet6_protocol *proto6;
	struct net_protocol *proto4;
	struct dst_entry *dst;
	struct socket *sock;
	struct sock *sk;

	/*
	 * Check if socket exists if not flush skb to Linux and return
	 */
	sock = READ_ONCE(conn->sock);
	if (!sock) {
		netfn_tcpst_rx_except(conn, skb);
		pr_err("%px: Socket is destroyed already\n", conn);
		return;
	}

	sk = sock->sk;
	dst = rcu_dereference(sk->sk_rx_dst);

	/*
	 * Check if dst cache is obsolete else consume the skb and return
	 */
	dst = dst ? dst_check(dst, sk->sk_rx_dst_cookie) : NULL;
	if (!dst) {
		netfn_tcpst_rx_except(conn, skb);
		conn->ext_stats.dst_miss_pkts += !dst;
		return;
	}

	skb_dst_set_noref(skb, dst);

	/*
	 * If this is GRO'd SKB, We need to update head SKB.
	 * - First SKB's original length will become head length
	 * - First SKB length will be total length.
	 * - All coalesced SKB length will become data length.
	 */
	if (likely(gro_state->head)) {
		struct tcphdr *tcph = tcp_hdr(skb);
		uint32_t head_len = skb->len;

		skb->len = gro_state->head_cache.len;
		skb->truesize = gro_state->head_cache.truesize;
		skb->data_len = gro_state->head_cache.len - head_len;
		skb_shinfo(skb)->frag_list = gro_state->first_frag;

		/*
		 * Correct headers.
		 * We are not correcting checksum as we are directly giving SKB to TCP stack.
		 */
		iph_set_totlen(ip_hdr(skb), skb->len);
		tcp_flag_word(tcph) |= gro_state->flag_word;

		/*
		 * Restore SKB protocol as we changed to ETH_P_MAP.
		 */
		skb->protocol = gro_state->protocol;
	}

	/*
	 * Reset local state.
	 */
	memset(gro_state, 0, sizeof(*gro_state));

	/*
	 * Offloaded packets come back with skb->dev = 'tcpst'.
	 * Restore cached device and incoming ifindex directly.
	 */
	skb->dev = conn->dst_dev;
	skb->skb_iif = conn->dst_dev_ifindex;

	/*
	 * Hand over to linux
	 */
	if (skb->protocol == htons(ETH_P_IP)) {
		IPCB(skb)->iif = skb->skb_iif;
		proto4 = rcu_dereference(inet_protos[IPPROTO_TCP]);
		proto4->handler(skb);
		return;
	}

	IP6CB(skb)->iif = skb->skb_iif;
	proto6 = rcu_dereference(inet6_protos[IPPROTO_TCP]);
	proto6->handler(skb);
}

/*
 * netfn_tcpst_rx_gro_start()
 *	Initialize GRO head.
 */
static bool netfn_tcpst_rx_gro_start(struct netfn_tcpst_conn *conn, struct napi_struct *napi, struct sk_buff *skb)
{
	struct netfn_tcpst_gro_state *gro_state = &conn->gro_state;
	struct netfn_tcpst_stats_ext *ext_stats = &conn->ext_stats;
	struct tcphdr *tcph = tcp_hdr(skb);
	struct gro_list *gro_list;
	uint32_t skip_gro = 0;
	uint32_t bucket = 0;
	uint32_t hlen;

	gro_list = &napi->gro_hash[bucket];
	hlen = tcph->doff * 4;

	/*
	 * Don't Coalesce if flag other then ACK is set,
	 * or checksum is not verified by EDMA.
	 * or this is pure ack (No data is present).
	 */
	skip_gro += !!((tcp_flag_word(tcph) & htonl(GENMASK(23,16))) & ~TCP_FLAG_ACK);
	skip_gro += !!(skb->ip_summed ^ CHECKSUM_UNNECESSARY);
	skip_gro += !!(skb->len <= hlen);
	if (skip_gro) {
		ext_stats->fail_start_coalesce++;
		return false;
	}

	/*
	 * Reset GRO fields as we are going to use napi gro list.
	 */
	*(u32 *)&NAPI_GRO_CB(skb)->zeroed = 0;

	NAPI_GRO_CB(skb)->count = 2; /* WAR: Linux doesn't call gro_complete if count is 1. */
	NAPI_GRO_CB(skb)->age = jiffies;

	/*
	 * Reset our local state for coalesce,
	 * We will copy this to NAPI_GRO_CB() during flush.
	 */
	gro_state->first_frag = gro_state->last_frag = NULL;
	gro_state->frag_cnt = 0;
	gro_state->hlen = hlen;
	gro_state->head_cache.len = skb->len;
	gro_state->head_cache.truesize = skb->truesize;
	gro_state->flag_word = tcp_flag_word(tcph);
	gro_state->head = skb;
	gro_state->ack_seq = tcph->ack_seq;
	gro_state->next_seq = ntohl(tcph->seq) + (skb->len - hlen);

	/*
	 * We will override protocol to ETH_P_MAP for GRO.
	 */
	gro_state->protocol = skb->protocol;
	skb->protocol = htons(ETH_P_MAP);

	/*
	 * Add to NAPI List.
	 * FIXME: Flush when list goes beyond limit?
	 */
	gro_list->count++;
	list_add(&skb->list, &gro_list->list);
	__set_bit(bucket, &napi->gro_bitmask);
	ext_stats->start_coalesce++;
	return true;
}

/*
 * netfn_tcpst_rx_gro_flush()
 *	Flush the GRO list and remove from Linux napi.
 */
static void netfn_tcpst_rx_gro_flush(struct netfn_tcpst_conn *conn, struct napi_struct *napi)
{
	struct netfn_tcpst_gro_state *gro_state = &conn->gro_state;
	struct sk_buff *skb = gro_state->head;
	struct gro_list *gro_list;
	uint32_t bucket = 0;

	gro_list = &napi->gro_hash[bucket];

	skb_list_del_init(skb);
	gro_list->count--;
	if (!gro_list->count && test_bit(bucket, &napi->gro_bitmask)) {
		__clear_bit(bucket, &napi->gro_bitmask);
	}

	netfn_tcpst_rx_conn(conn, skb);
}

/*
 * netfn_tcpst_rx_gro_add()
 *	Merge SKB into current GRO state.
 */
static bool netfn_tcpst_rx_gro_add(struct netfn_tcpst_conn *conn, struct napi_struct *napi, struct sk_buff *skb)
{
	struct netfn_tcpst_gro_state *gro_state = &conn->gro_state;
	struct netfn_tcpst_stats_ext *ext_stats = &conn->ext_stats;
	struct tcphdr *tcph = tcp_hdr(skb);
	uint32_t skip_gro = 0;
	uint32_t hlen;
	__be32 flags;

	flags = tcp_flag_word(tcph);
	hlen = tcph->doff * 4;

	/*
	 * TCP Flags must be the same as previous, Allow coalesce for FIN & PSH.
	 */
	skip_gro += !!((flags ^ gro_state->flag_word) & ~(TCP_FLAG_FIN | TCP_FLAG_PSH));

	/*
	 * TCP sequence check.
	 */
	skip_gro += !!(tcph->ack_seq ^ gro_state->ack_seq);
	skip_gro += !!(ntohl(tcph->seq) ^ gro_state->next_seq);

	/*
	 * Data offset has to be same.
	 */
	skip_gro += !!(hlen ^ gro_state->hlen);

	/*
	 * We can't coalesce new SKB. Flush the previous list.
	 */
	if (skip_gro) {
		ext_stats->fail_coalesce++;
		netfn_tcpst_rx_gro_flush(conn, napi);
		return false;
	}

	/*
	 * We are going to coalesce data, Pull TCP header as well.
	 */
	__skb_pull(skb, hlen);

	/*
	 * Update GRO state for next packet.
	 */
	gro_state->flag_word |= (flags & (TCP_FLAG_FIN | TCP_FLAG_PSH));
	gro_state->next_seq += skb->len;

	/*
	 * Insert into GRO list.
	 */
	gro_state->frag_cnt++;
	gro_state->head_cache.len += skb->len;
	gro_state->head_cache.truesize +=  skb->truesize;

	/*
	 * If first_frag is empty, Then this is first frag in chain.
	 */
	if (!gro_state->first_frag) {
		gro_state->first_frag = gro_state->last_frag = skb;
	} else {
		gro_state->last_frag->next = skb;
		gro_state->last_frag = skb;
	}

	/*
	 * Flush coalesced chain if we had received PSH or FIN.
	 */
	if ((flags & (TCP_FLAG_FIN | TCP_FLAG_PSH))) {
		netfn_tcpst_rx_gro_flush(conn, napi);
	}

	ext_stats->coalesce++;
	return true;
}

/*
 * netfn_tcpst_http_get_resp()
 *	Parse and verify the response
 */
static enum netfn_tcpst_err netfn_tcpst_http_get_resp(struct netfn_tcpst_conn *conn)
{
	struct msghdr msg = {0};
	char *buf = conn->buf;
	struct kvec iov;
	int rx_bytes;
	char *data;

	/*
	 * Prepare receive message
	 */
	msg.msg_flags |= (MSG_WAITALL | MSG_TRUNC);
	iov.iov_base = buf;
	iov.iov_len = NETFN_TCPST_HTTP_RESPONSE_LEN;

	/*
	 * Wait for HTTP response
	 */
	rx_bytes = kernel_recvmsg(READ_ONCE(conn->sock), &msg, &iov, 1, NETFN_TCPST_HTTP_RESPONSE_LEN, 0);
	if (rx_bytes < 0) {
		pr_warn("%px: Error in kernel_recvmsg\n", conn);
		return NETFN_TCPST_ERR_RX;
	}

	pr_debug("%px: Response received(%s)\n", conn, buf);

	/*
	 * Check if response has "200 OK" in response
	 */
	if (!strnstr(buf, "200 OK", strlen(buf))) {
		pr_warn("%px: Invalid response\n", conn);
		return NETFN_TCPST_ERR_CONNECT;
	}

	data = strnstr(buf, "Content-Length: ", strlen(buf));
	if (!data) {
		pr_warn("%px: Can't find Content-length string\n", conn);
		return NETFN_TCPST_ERR_CONNECT;
	}

	data += sizeof("Content-Length: ") - 1;
	if (kstrtoull(data, 10, &conn->http.file_sz)) {
		pr_warn("%px: Failed to parse Content-Length value\n", conn);
		return NETFN_TCPST_ERR_CONNECT;
	}

	/*
	 * We only count the data bytes, so we subtract strlen(conn->buf)
	 * from total bytes received
	 */
	conn->ext_stats.test_rx_bytes += (rx_bytes - strlen(conn->buf));
	conn->ext_stats.test_rx_pkts += !!rx_bytes;
	return NETFN_TCPST_ERR_NONE;
}

/*
 * netfn_tcpst_http_get()
 *	HTTP GET support
 */
static bool netfn_tcpst_http_get(struct netfn_tcpst_conn *conn)
{
	enum netfn_tcpst_err err = NETFN_TCPST_ERR_NONE;
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct netfn_tcpst_http *http = &conn->http;
	struct msghdr msg = {0};
	struct kvec iov = {0};
	int len;

	/*
	 * Prepare HTTP PUT message
	 */
	msg.msg_flags |= MSG_MORE;
	iov.iov_base = http->hdr;
	iov.iov_len = http->hdr_len;

	/*
	 * Send HTTP GET request to server
	 */
	len = kernel_sendmsg(READ_ONCE(conn->sock), &msg, &iov, 1, http->hdr_len);
	if (len < 0) {
		pr_warn("%px: Error in kernel_sendmsg\n", conn);
		return false;
	}

	/*
	 * Send a GET command to request the contents of the file and
	 * record current timestamp as ROMTime.
	 */
	stats->test.rom_time = ktime_get_real_ns();

	err = netfn_tcpst_http_get_resp(conn);
	if (err != NETFN_TCPST_ERR_NONE) {
		pr_warn("%px: Invalid response received aborting\n", conn);
		return false;
	}

	/*
	 * Increment bom_time for first response only
	 */
	stats->test.bom_time = ktime_get_real_ns();
	return true;
}

/*
 * netfn_tcpst_rx_gro_comp()
 *	Flush the packet to Linux TCP stack.
 */
int netfn_tcpst_rx_gro_comp(struct sk_buff *skb, int thoff)
{
	struct netfn_tcpst_conn *conn;
	struct netfn_tcpst_ctx *ctx;

	rcu_read_lock_bh();
	ctx = rcu_dereference_bh(g_tcpst.ctx);
	if (!ctx) {
		dev_kfree_skb(skb);
		goto unlock;
	}

	conn = &ctx->conn[smp_processor_id()];
	conn->ext_stats.napi_flush++;
	netfn_tcpst_rx_conn(conn, skb);

	/*
	 * Inform linux we have consumed/forwarded the packet.
	 */
unlock:
	rcu_read_unlock_bh();
	return -EINPROGRESS;
}

/*
 * netfn_tcpst_rx_vp4()
 *	PPE-VP callback for IPv4
 */
bool netfn_tcpst_rx_vp4(struct ppe_vp_cb_info *info, void *cb_data)
{
	bool (*gro_rcv)(struct netfn_tcpst_conn *conn, struct napi_struct *napi, struct sk_buff *skb);
	struct netfn_tcpst_ctx *ctx = (struct netfn_tcpst_ctx *)cb_data;
	struct netfn_tcpst_gro_state *gro_state;
	struct netfn_tcpst_stats_ext *ext_stats;
	struct sk_buff *skb = info->skb;
	struct netfn_tcpst_conn *conn;

	/*
	 * Test is not running
	 */
	if (netfn_tcpst_state_get(ctx) != NETFN_TCPST_STATE_INPROGRESS) {
		dev_kfree_skb(skb);
		pr_debug_ratelimited("%px: Test is not running\n", ctx);
		return true;
	}

	conn = &ctx->conn[smp_processor_id()];

	/*
	 * Check if connection socket exists,
	 * if not then consume skb and return
	 */
	if (!READ_ONCE(conn->sock)) {
		dev_kfree_skb(skb);
		pr_warn("%px: Connection already destroyed\n", ctx);
		return true;
	}

	gro_state = &conn->gro_state;
	ext_stats = &conn->ext_stats;
	ext_stats->vp_rx_pkts++;

	/*
	 * Set skb fields
	 */
	skb->ip_summed = info->ip_summed;
	skb->pkt_type = PACKET_HOST;

	/*
	 * Pull IP header as packet will be delivered to tcp stack.
	 */
	skb_reset_network_header(skb);
	__skb_pull(skb, sizeof(struct iphdr));
	skb_reset_transport_header(skb);

	gro_rcv = gro_state->head ? netfn_tcpst_rx_gro_add : netfn_tcpst_rx_gro_start;
	if (!gro_rcv(conn, info->napi, skb)) {
		rcu_read_lock_bh();
		netfn_tcpst_rx_conn(conn, skb);
		rcu_read_unlock_bh();
	}

	return true;
}

/*
 * netfn_tcpst_rx_vp6()
 *	PPE-VP callback for IPv6
 */
bool netfn_tcpst_rx_vp6(struct ppe_vp_cb_info *info, void *cb_data)
{
	bool (*gro_rcv)(struct netfn_tcpst_conn *conn, struct napi_struct *napi, struct sk_buff *skb);
	struct netfn_tcpst_ctx *ctx = (struct netfn_tcpst_ctx *)cb_data;
	struct netfn_tcpst_gro_state *gro_state;
	struct netfn_tcpst_stats_ext *ext_stats;
	struct sk_buff *skb = info->skb;
	struct netfn_tcpst_conn *conn;

	/*
	 * Test is not running
	 */
	if (netfn_tcpst_state_get(ctx) != NETFN_TCPST_STATE_INPROGRESS) {
		dev_kfree_skb(skb);
		pr_debug_ratelimited("%px: Test is not running\n", ctx);
		return true;
	}

	conn = &ctx->conn[smp_processor_id()];

	/*
	 * Check if connection socket exists,
	 * if not then consume skb and return
	 */
	if (!READ_ONCE(conn->sock)) {
		dev_kfree_skb(skb);
		pr_warn("%px: Socket already destroyed\n", ctx);
		return true;
	}

	gro_state = &conn->gro_state;
	ext_stats = &conn->ext_stats;
	ext_stats->vp_rx_pkts++;

	/*
	 * Set skb fields
	 */
	skb->ip_summed = info->ip_summed;
	skb->pkt_type = PACKET_HOST;

	/*
	 * Pull IP header as packet will be deliverd to tcp stack.
	 */
	skb_reset_network_header(skb);
	__skb_pull(skb, sizeof(struct ipv6hdr));
	skb_reset_transport_header(skb);

	gro_rcv = gro_state->head ? netfn_tcpst_rx_gro_add : netfn_tcpst_rx_gro_start;
	if (!gro_rcv(conn, info->napi, skb)) {
		rcu_read_lock_bh();
		netfn_tcpst_rx_conn(conn, skb);
		rcu_read_unlock_bh();
	}

	return true;
}

/*
 * netfn_tcpst_rx_data()
 *	TCP receive data
 */
void netfn_tcpst_rx_data(struct work_struct *work)
{
	struct netfn_tcpst_conn *conn = container_of(work, struct netfn_tcpst_conn, work);
	struct netfn_tcpst_stats_ext *ext_stats = &conn->ext_stats;
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct netfn_tcpst_ctx *ctx = conn->ctx;
	bool resched, eom, timeout, abort;
	enum netfn_tcpst_state state;
	struct socket *sock;
	struct sock *sk;
	long timeo = 0;
	int rx_bytes;
	int ret;

	sock = READ_ONCE(conn->sock);
	sk = sock->sk;

	/*
	 * Read the data from socket
	 */
	lock_sock(sk);
	ret = sk_wait_data(sk, &timeo, NULL);
	if (ret < 0) {
		pr_warn("%px: Error in kernel_recvmsg aborting on cpu(%d)\n", conn, smp_processor_id());
		netfn_tcpst_state_set(conn->ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(conn->ctx));

		release_sock(sk);
		return;
	}

	rx_bytes = tcp_read_skb(sk, netfn_tcpst_rx_free);
	release_sock(sk);

	/*
	 * Increment the bytes and pkt counter
	 */
	ext_stats->test_rx_bytes += rx_bytes;
	ext_stats->test_rx_pkts += !!rx_bytes;

	/*
	 * Reschedule work if we haven't received all data or no timeout received
	 */
	eom = (ext_stats->test_rx_bytes >= conn->test_bytes);
	timeout = test_bit(NETFN_TCPST_TIMER_COMP_BIT, &conn->timer_state);
	abort = (netfn_tcpst_state_get(conn->ctx) == NETFN_TCPST_STATE_ABORTED);
	resched = (!eom && !timeout && !abort);
	if (likely(resched) && likely(queue_work_on(smp_processor_id(), ctx->wq, work))) {
		return;
	}

	pr_debug("%px: Not scheduling eom(%d) timeout(%d) abort(%d)\n", conn, eom, timeout, abort);
	state = netfn_tcpst_state_get(ctx);

	/*
	 * Mark state as completed only if all connections are done
	 */
	if (atomic_inc_return(&ctx->comp_conn) == ctx->num_conns)
		state = NETFN_TCPST_STATE_COMPLETED;

	/*
	 * Check if duration timer has expired. For timebound test
	 * the stats are captured in timer callback.
	 */
	if (timeout) {
		netfn_tcpst_state_set(conn->ctx, state, NETFN_TCPST_STATE_INPROGRESS);
		pr_debug("%px: Timeout occured, marking test completed\n", conn);
		return;
	}

	/*
	 * If EOM is received and its timebound test then we abort.
	 */
	if (eom && conn->duration) {
		netfn_tcpst_state_set(conn->ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(conn->ctx));
		pr_debug("%px: EOM received before timeout, marking test aborted\n", conn);
		return;
	}

	/*
	 * Abort the test if EOM is not received in file based test
	 */
	if (!eom) {
		netfn_tcpst_state_set(conn->ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(conn->ctx));
                pr_debug("%px: EOM is not received, marking test aborted\n", conn);
                return;
        }

	/*
	 * Update the stats
	 */
	stats->test.eom_time = ktime_get_real_ns();
	stats->test_bytes.rcvd = ext_stats->test_rx_bytes;
	netfn_tcpst_update_eth_stats(conn->eth_dev, stats);
	netfn_tcpst_state_set(conn->ctx, state, NETFN_TCPST_STATE_INPROGRESS);
	pr_debug("%px: EOM received, marking test completed\n", conn);
	return;
}

/*
 * netfn_tcpst_rx_work()
 *	TCP download support
 */
void netfn_tcpst_rx_work(struct work_struct *work)
{
	struct netfn_tcpst_conn *conn = container_of(work, struct netfn_tcpst_conn, work);
	struct netfn_tcpst_stats *stats = &conn->stats;
	struct netfn_tcpst_ctx *ctx = conn->ctx;
	enum netfn_tcpst_err err;

	/*
	 * Capture the rom and bom time for raw mode.
	 * It will be overridden for HTTP case
	 */
	stats->test.rom_time = stats->test.bom_time = ktime_get_real_ns();

	/*
	 * If in HTTP test mode then perform HTTP GET
	 */
	if (netfn_tcpst_is_http(ctx) && !netfn_tcpst_http_get(conn)) {
		pr_warn("%px: Error(%d) in connecting to HTTP server\n", work, err);
		netfn_tcpst_state_set(ctx, NETFN_TCPST_STATE_ABORTED, netfn_tcpst_state_get(ctx));
		atomic_inc(&ctx->comp_conn);
		return;
	}

	/*
	 * Initialize test bytes
	 */
	conn->test_bytes = netfn_tcpst_is_http(ctx) ? conn->http.file_sz : conn->raw.max_bytes;

	/*
	 * Start the timer only if its time based test
	 */
	if (conn->duration) {
		conn->hrtimer.function = netfn_tcpst_rx_timeout;
		hrtimer_start(&conn->hrtimer, ktime_set(conn->offset, 0), HRTIMER_MODE_REL);
	} else {
		/*
		 * File based mode, update initial ethernet stats
		 */
		netfn_tcpst_update_eth_stats(conn->eth_dev, &conn->stats);
	}

	/*
	 * Override work function for rx data path
	 */
	INIT_WORK(work, netfn_tcpst_rx_data);

	/*
	 * Schedule the work to receive the tx Data
	 */
	queue_work_on(smp_processor_id(), ctx->wq, work);
}
