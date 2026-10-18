/*
 * netfn_capwap_dec.c
 *	Network function's CAPWAP offload decapsulation.
 *
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/sysfs.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/ieee80211.h>
#include <linux/if_ether.h>
#include <linux/ip.h>

#include <net/protocol.h>
#include <net/dsfield.h>

#include "netfn_capwap.h"
#include "netfn_capwap_hdr.h"
#include "netfn_capwap_priv.h"
#include "netfn_capwap_tun.h"
#include "netfn_capwap_dec.h"

#define NETFN_CAPWAP_MAX_STRLEN 25
#define NETFN_CAPWAP_DEC_STATS_MAX (sizeof(struct netfn_capwap_dec_stats) / sizeof(uint64_t))

/*
 * Decap statistics strings
 */
static int8_t *g_dec_stats_str[] = {
	"pkts_rcvd",
	"dtls_dec_pkts",
	"control_pkts",
	"keepalive_pkts",
	"fast_reasm",
	"slow_reasm",
	"flow_cookie_no_db",
	"drop_missing_frags",
	"drop_queue_full",
	"drop_pri_queue_full",
	"err_dtls_pkts",
	"err_dec_failure",
	"err_max_frags",
	"err_large_frags",
	"err_csum_fail",
	"err_malformed",
	"err_excess_len",
	"err_nwireless_len",
	"res1",
};

/*
 * netfn_capwap_dec_is_frag_chain()
 *	Check if the 2 frags are part of same chain
 */
static inline bool netfn_capwap_dec_is_frag_chain(struct sk_buff *skb1, struct sk_buff *skb2)
{
	return NETFN_CAPWAP_CB(skb1)->frag_id == NETFN_CAPWAP_CB(skb2)->frag_id;
}

/*
 * netfn_capwap_dec_can_fast()
 *	Check if the skb is parked in fast reassembly cache already.
 */
static inline bool netfn_capwap_dec_can_fast(struct netfn_capwap_dec_ctx *ctx, uint16_t frag_id)
{
	struct sk_buff *cached_skb = ctx->reasm_cache;

	return cached_skb && (frag_id == NETFN_CAPWAP_CB(cached_skb)->frag_id);
}

/*
 * netfn_capwap_dec_can_slow()
 *	Check if the skb has a corresponding fragment already parked in queue
 */
static inline bool netfn_capwap_dec_can_slow(struct netfn_capwap_dec_ctx *ctx, uint16_t frag_id)
{
	struct netfn_capwap_frags *frags;
	uint16_t idx;

	idx = NETFN_CAPWAP_FRAG_IDX(frag_id, NETFN_CAPWAP_TUN_MAX_REASM_WIN);
	frags = &ctx->reasm_table[idx];

	return frag_id == frags->frag_id;
}

/*
 * netfn_capwap_dec_add_slow()
 *	Add new skb to slow reassembly cache.
 */
static void netfn_capwap_dec_add_slow(struct netfn_capwap_dec_ctx *ctx, uint16_t frag_id, struct sk_buff *skb,
		struct sk_buff_head *q_free)
{
	struct netfn_capwap_dec *dec = ctx->dec;
	struct netfn_capwap_frags *frags;
	struct sk_buff_head *frag_list;
	uint16_t idx;

	idx = NETFN_CAPWAP_FRAG_IDX(frag_id, NETFN_CAPWAP_TUN_MAX_REASM_WIN);
	frags = &ctx->reasm_table[idx];
	frag_list = &frags->list;

	/*
	 * If we already have frags in slow reasm queue and frag IDs don't match than
	 * free previous list and add new skb to list.
	 */
	if (!skb_queue_empty(frag_list) && (unlikely(frag_id != frags->frag_id))) {
		NETFN_CAPWAP_TUN_STATS_ADD(&dec->stats.drop_missing_frags, skb_queue_len(frag_list));
		skb_queue_splice_tail(frag_list, q_free);
		netfn_capwap_frags_init(frags);
	}

	netfn_capwap_frags_add(frags, skb);
}

/*
 * netfn_capwap_dec_get_flow_cookie
 *	Fetch the Pointer to Flow Cookie from the Netfn Flow Cookie DB.
 */
static struct netfn_flow_cookie *netfn_capwap_dec_get_flow_cookie(struct netfn_flow_cookie_db *db, uint8_t *data)
{
	struct iphdr *iph = (struct iphdr *)data;
	struct netfn_tuple t = {0};
	uint32_t l4_hdr_offset;
	uint8_t proto = 0;

	if (iph->version == IPVERSION) {
		proto = iph->protocol;
		l4_hdr_offset = sizeof(struct iphdr);
		t.ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V4;
		t.tuples.tuple_5.src_ip.ip4.s_addr = iph->saddr;
		t.tuples.tuple_5.dest_ip.ip4.s_addr = iph->daddr;
	} else {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)data;

		proto = ip6h->nexthdr;
		l4_hdr_offset = sizeof(struct ipv6hdr);
		t.ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V6;

		/*
		 * Source IP
		 */
		t.tuples.tuple_5.src_ip.ip6.s6_addr32[0] = ip6h->saddr.s6_addr32[0];
		t.tuples.tuple_5.src_ip.ip6.s6_addr32[1] = ip6h->saddr.s6_addr32[1];
		t.tuples.tuple_5.src_ip.ip6.s6_addr32[2] = ip6h->saddr.s6_addr32[2];
		t.tuples.tuple_5.src_ip.ip6.s6_addr32[3] = ip6h->saddr.s6_addr32[3];

		/*
		 * Destination IP
		 */
		t.tuples.tuple_5.dest_ip.ip6.s6_addr32[0] = ip6h->daddr.s6_addr32[0];
		t.tuples.tuple_5.dest_ip.ip6.s6_addr32[1] = ip6h->daddr.s6_addr32[1];
		t.tuples.tuple_5.dest_ip.ip6.s6_addr32[2] = ip6h->daddr.s6_addr32[2];
		t.tuples.tuple_5.dest_ip.ip6.s6_addr32[3] = ip6h->daddr.s6_addr32[3];
	}

	t.tuples.tuple_5.protocol = proto;

	/*
	 * Check if the protocol is TCP/UDP and extract the port numbers
	 */
	if (proto == IPPROTO_TCP) {
		struct tcphdr *tcph = (struct tcphdr *)(data + l4_hdr_offset);

		t.tuples.tuple_5.l4_src_ident = tcph->source;
		t.tuples.tuple_5.l4_dest_ident = tcph->dest;
		t.tuple_type = NETFN_TUPLE_5TUPLE;

		return netfn_flow_cookie_db_lookup(db, &t);
	}

	if (proto == IPPROTO_UDP) {
		struct udphdr *udph = (struct udphdr *)(data + l4_hdr_offset);

		t.tuples.tuple_5.l4_src_ident = udph->source;
		t.tuples.tuple_5.l4_dest_ident = udph->dest;
		t.tuple_type = NETFN_TUPLE_5TUPLE;

		return netfn_flow_cookie_db_lookup(db, &t);
	}

	return NULL;
}

/*
 * netfn_capwap_dec_convert()
 *	Converts 802.11 to 802.3 pkt and adds meta header
 */
static void netfn_capwap_dec_convert(struct netfn_capwap_dec *dec, struct sk_buff *skb)
{
	struct ieee80211_hdr_3addr *wlan_hdr;
	struct netfn_capwap_hdr_mdata *mdata;
	struct netfn_capwap_prehdr *phdr;
	struct netfn_flow_cookie_db *db;
	struct netfn_flow_cookie *nfc;
	struct netfn_capwap_tun *nct;
	bool is_ctrl_frame = false;
	uint8_t *start, *next_hdr;
	uint8_t smac[ETH_ALEN];
	uint8_t dmac[ETH_ALEN];
	uint32_t offset = 0;
	struct ethhdr *eth;
	__le16 frame_ctrl;
	uint16_t protocol;

	start = next_hdr = skb->data;
	mdata = NETFN_CAPWAP_CB(skb);
	phdr = &mdata->phdr;

	/*
	 * For exception packets we don't need conversion.
	 */
	if (unlikely(mdata->exception)) {
		return;
	}

	/*
	 * Check if it needs conversion, else add meta header and exit.
	 */
	if (!mdata->type_80211) {
		eth = (struct ethhdr *)next_hdr;
		skb->protocol = eth->h_proto;
		goto add_prehdr;
	}

	phdr->type |= NETFN_CAPWAP_PKT_TYPE_80211;

	/*
	 * We only convert 802.11 DATA frame. In case of non-data packets
	 * we add meta header and punt to host.
	 */
	wlan_hdr = (struct ieee80211_hdr_3addr *)next_hdr;
	frame_ctrl = ntohs(wlan_hdr->frame_control);

	if (!unlikely(ieee80211_is_data(frame_ctrl))) {
		is_ctrl_frame = true;
		goto add_prehdr;
	}

	/*
	 * Save mac addresses for constructing 802.3 frame.
	 */
	ether_addr_copy((u8 *)smac, (u8 *)wlan_hdr->addr3);
	ether_addr_copy((u8 *)dmac, (u8 *)wlan_hdr->addr1);

	/*
	 * Override dmac if bssid is used a dmac
	 */
	if (static_branch_unlikely(&skip_bss_mac)) {
		ether_addr_copy((u8 *)smac, (u8 *)wlan_hdr->addr2);
		ether_addr_copy((u8 *)dmac, (u8 *)wlan_hdr->addr3);
	}

	next_hdr += sizeof(*wlan_hdr);

	/*
	 * Strip the Qos header (802.11e)
	 */
	if (likely(ieee80211_is_data_qos(frame_ctrl))) {
		phdr->wireless_qos = *(uint16_t *)next_hdr;
		next_hdr += sizeof(phdr->wireless_qos);
	}

	next_hdr += NETFN_CAPWAP_SNAP_HDR_LEN;

	skb->protocol = protocol = *(uint16_t *)next_hdr;
	next_hdr += sizeof(uint16_t);

	/*
	 * Start writing the new ethernet header
	 */
	next_hdr -= sizeof(*eth);

	eth = (struct ethhdr *)next_hdr;
	ether_addr_copy((u8 *)eth->h_dest, (u8 *)dmac);
	ether_addr_copy((u8 *)eth->h_source, (u8 *)smac);
	eth->h_proto = protocol;
	phdr->type |= NETFN_CAPWAP_PKT_TYPE_8023;

add_prehdr:
	/*
	 * Add preheader.
	 */
	next_hdr -= sizeof(*phdr);

	phdr = (struct netfn_capwap_prehdr *)next_hdr;
	*phdr = mdata->phdr;

	__skb_push(skb, start - next_hdr);

	if (is_ctrl_frame) {
		return;
	}

	nct = container_of(dec, struct netfn_capwap_tun, dec);

	/*
	 * Fill the right offset here.
	 */
	offset = sizeof(*eth) + sizeof(*phdr);

	phdr->flow_id = 0;

	/*
	 * Optional flow - ID lookup for inner payload
	 */
	rcu_read_lock_bh();

	db = rcu_dereference(nct->db);
	if (unlikely(db) && ((ntohs(skb->protocol) == ETH_P_IP) || (ntohs(skb->protocol) == ETH_P_IPV6)))  {
		nfc = netfn_capwap_dec_get_flow_cookie(db, skb->data + offset);
		if (nfc) {

			/*
			 * Only fill if we get the cookie
			 */
			phdr->flow_id = nfc->flow_id;
			phdr->scs_sdwf_id = nfc->scs_sdwf_hdl;
			phdr->type |= (nfc->valid_flag & NETFN_CAPWAP_DEC_PHDR_TYPE_MASK);

		}
	}

	rcu_read_unlock_bh();
	return;
}

/*
 * netfn_capwap_dec_fast_reasm()
 *	Verify if given 2 skbs can be reassembled.
 */
static struct sk_buff *netfn_capwap_dec_fast_reasm(struct netfn_capwap_dec_ctx *ctx, struct sk_buff *skb,
		struct sk_buff_head *q_free)
{
	struct netfn_capwap_hdr_mdata *info1, *info2;
	struct netfn_capwap_dec *dec = ctx->dec;
	unsigned char *iter, *start, *end;
	struct sk_buff *skb1, *skb2;
	int err = 0;

	skb1 = ctx->reasm_cache;
	skb2 = skb;
	info1 = NETFN_CAPWAP_CB(skb1);
	info2 = NETFN_CAPWAP_CB(skb2);

	/*
	 * If these are jumbled frags we swap them.
	 */
	if (unlikely(info1->frag_offset > info2->frag_offset)) {
		skb1 = xchg(&skb2, skb1);
		info1 = xchg(&info2, info1);
	}

	/*
	 * Check if we have enough tailroom. Else fallback to slow reasm
	 * which allocates a new skb.
	 */
	if (unlikely(skb_tailroom(skb1) < skb2->len)) {
		return NULL;
	}

	/*
	 * Unhandled cases;
	 * 1.Mismatched fragments
	 * 2.More than 2 fragments in the chain
	 */

	err = (info1->frag_id != info2->frag_id);
	err += (skb1->len != info2->frag_offset);
	err += !!info1->frag_offset;
	err += !info2->frag_end;
	if (err) {
		return NULL;
	}

	start = skb_tail_pointer(skb1);
	end = start + skb2->len;
	for (iter = start; iter < end; iter += PREFETCH_STRIDE) {
		prefetchw(iter);
	}

	/*
	 * Coalesce the 2 skbs.
	 */
	__skb_put_data(skb1, skb2->data, skb2->len);
	__skb_queue_head(q_free, skb2);

	ctx->reasm_cache = NULL;

	NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.fast_reasm);
	return skb1;
}

/*
 * netfn_capwap_dec_linearize()
 *	Linearizes the chain of skbs
 */
static struct sk_buff *netfn_capwap_dec_linearize(struct netfn_capwap_frags *frags, struct netfn_capwap_dec_stats *stats, struct sk_buff_head *q_free)
{
	struct sk_buff *iter, *tmp, *skb = NULL;
	struct netfn_capwap_hdr_mdata *mdata;
	uint16_t skb_size, expect_offset = 0;
	struct sk_buff_head q_tmp;

	skb_size = frags->tot_sz + NETFN_CAPWAP_HEADROOM_RESERVE + NETFN_CAPWAP_TAILROOM_RESERVE;
	skb = netdev_alloc_skb_fast(NULL, skb_size);
	if (!skb) {
		pr_warn_ratelimited("%px: Unable to allocate skb of size(%d)\n", &frags->list, skb_size);
		skb_queue_splice_tail(&frags->list, q_free);
		return NULL;
	}

	skb_reserve(skb, NETFN_CAPWAP_HEADROOM_RESERVE);
	mdata = NETFN_CAPWAP_CB(skb);

	skb_queue_head_init(&q_tmp);

restart:
	skb_queue_walk_safe(&frags->list, iter, tmp) {
		uint16_t offset = NETFN_CAPWAP_CB(iter)->frag_offset;

		if (expect_offset == offset) {
			expect_offset += iter->len;
			if (offset == 0) { /* first fragment */
				*mdata = *NETFN_CAPWAP_CB(iter);
			}

			memcpy(skb->data + offset, iter->data, iter->len);
			__skb_unlink(iter, &frags->list);
			__skb_queue_tail(&q_tmp, iter);
			goto restart;
		}
	}

	__skb_put(skb, frags->tot_sz);
	skb_queue_splice_tail(&q_tmp, q_free);

	/*
	 * If we have fragments left then fragments were corrupted.
	 */
	if (!skb_queue_empty(&frags->list)) {
		NETFN_CAPWAP_TUN_STATS_INC(&stats->err_malformed);
		dev_kfree_skb(skb);
		skb = NULL;
	}

	skb_queue_splice_tail(&frags->list, q_free);
	return skb;
}

/*
 * netfn_capwap_dec_slow_reasm()
 *	Reassemble the packets.
 */
static struct sk_buff *netfn_capwap_dec_slow_reasm(struct netfn_capwap_dec_ctx *ctx, uint16_t frag_id,
		struct sk_buff_head *q_free)
{
	struct netfn_capwap_dec *dec = ctx->dec;
	struct netfn_capwap_dec_stats *stats;
	struct sk_buff *reasm_skb = NULL;
	struct netfn_capwap_frags *frags;
	struct sk_buff_head *frag_list;
	uint16_t idx;

	stats = &dec->stats;
	idx = NETFN_CAPWAP_FRAG_IDX(frag_id, NETFN_CAPWAP_TUN_MAX_REASM_WIN);

	frags = &ctx->reasm_table[idx];
	frag_list = &frags->list;

	/*
	 * If this was first frag added than we return.
	 */
	if (unlikely(skb_queue_len(frag_list) == 1)) {
		return NULL;
	}

	/*
	 * Check if total payload size exceeds max payload size
	 */
	if (unlikely(frags->tot_sz > dec->max_payload_sz)) {
		NETFN_CAPWAP_TUN_STATS_ADD(&stats->err_large_frags, skb_queue_len(frag_list));
		skb_queue_splice_tail(frag_list, q_free);
		goto done;
	}

	/*
	 * Check if the number of frags exceeds max limit.
	 */
	if (unlikely(skb_queue_len(frag_list) > dec->max_frags)) {
		NETFN_CAPWAP_TUN_STATS_ADD(&stats->err_max_frags, skb_queue_len(frag_list));
		skb_queue_splice_tail(frag_list, q_free);
		goto done;
	}

	/*
	 * This is the case where either:
	 * 1. We have received duplicate fragments.
	 * 2. Or we have received overlapping fragments.
	 */
	if (unlikely(frags->tot_sz && (frags->frag_sz > frags->tot_sz))) {
		NETFN_CAPWAP_TUN_STATS_ADD(&stats->err_excess_len, skb_queue_len(frag_list));
		skb_queue_splice_tail(frag_list, q_free);
		goto done;
	}

	/*
	 * Check if we have received all frags.
	 */
	if (frags->frag_sz != frags->tot_sz) {
		return NULL;
	}

	reasm_skb = netfn_capwap_dec_linearize(frags, stats, q_free);
	if (!reasm_skb) {
		goto done;
	}

	NETFN_CAPWAP_TUN_STATS_INC(&stats->slow_reasm);
done:
	netfn_capwap_frags_init(frags);
	return reasm_skb;
}

/*
 * netfn_capwap_dec_capwap()
 *      Decapsulates capwap
 */
static uint8_t *netfn_capwap_dec_capwap(struct netfn_capwap_dec *dec, struct sk_buff *skb, uint8_t *start, struct netfn_capwap_prehdr *preh)
{
	struct netfn_capwap_hdr *nch = (struct netfn_capwap_hdr *)start;
	uint8_t cw_hlen, next_hdr = 0, preamble;
	struct netfn_capwap_hdr_mdata *mdata;
	union netfn_capwap_hdr_info info;
	union netfn_capwap_hdr_frag frag;

	mdata = NETFN_CAPWAP_CB(skb);
	info.word = ntohl(nch->info);
	frag.word = ntohl(nch->frag);

	/*
	 * For capwap pkts preamble should be zero.
	 */
	preamble = netfn_capwap_hdr_get_preamble(info);
	if (unlikely(preamble)) {
		NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.err_dtls_pkts);
		preh->type |= NETFN_CAPWAP_PKT_TYPE_INVALID;
		return start;
	}

	/*
	 * We are sure that its a valid packet
	 */
	if (likely(!(preh->type & NETFN_CAPWAP_PKT_TYPE_CTRL))) {
		preh->type |= NETFN_CAPWAP_PKT_TYPE_DATA;
	}

	mdata->frag = netfn_capwap_hdr_has_fbit(info);
	mdata->frag_end = netfn_capwap_hdr_has_lbit(info);
	mdata->keep_alive = netfn_capwap_hdr_has_kbit(info);
	mdata->type_80211 = netfn_capwap_hdr_has_tbit(info);

	mdata->frag_id = netfn_capwap_hdr_get_frag_id(frag);
	mdata->frag_offset = netfn_capwap_hdr_get_frag_offset(frag);

	/*
	 * Pull capwap header
	 */
	next_hdr += sizeof(*nch);

	preh->version = NETFN_CAPWAP_VERSION;
	preh->rid = netfn_capwap_hdr_get_rid(info);
	cw_hlen = netfn_capwap_hdr_get_hlen(info);

	/*
	 * We skip the processing if:
	 * 1. Its keepalive packet or
	 * 2. Its not first fragment.
	 */
	if (mdata->keep_alive || (mdata->frag && mdata->frag_offset)) {
		NETFN_CAPWAP_TUN_STATS_ADD(&dec->stats.keepalive_pkts, mdata->keep_alive);
		return start + next_hdr;
	}

	/*
	 * TODO: Make this as a new function for optimization to avoid hitting
	 * above if condition twice.
	 */
	/*
	 * If wireless info is present it needs to be sent as part of preheader.
	 */
	if (netfn_capwap_hdr_has_wbit(info)) {
		struct netfn_capwap_winfo *winfo = (struct netfn_capwap_winfo *)(start + next_hdr);
		uint8_t nwireless;

		preh->type |= NETFN_CAPWAP_PKT_TYPE_WINFO;

		/*
		 * If wireless info exceeds the max sections then we punt inner packet to host.
		 */
		nwireless = cw_hlen - sizeof(*nch);
		if (unlikely((nwireless / sizeof(*winfo)) > NETFN_CAPWAP_MAX_NWIRELESS)) {
			NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.err_nwireless_len);
			return start + next_hdr;
		}

		BUILD_BUG_ON(NETFN_CAPWAP_MAX_NWIRELESS > sizeof(*winfo));
		preh->nwireless = (nwireless / sizeof(*winfo));
		*((struct netfn_capwap_winfo *)preh->wl_info) = *winfo;

		/*
		 * Pull the wirelesss information header
		 */
		next_hdr += sizeof(*winfo);
	}

	/*
	 * As this is first frag copy the meta header. That will be used post reassembly
	 * to add to reassembled skb.
	 */
	mdata->phdr = *preh;

	return (start + next_hdr);
}

/*
 * netfn_capwap_dec_l4()
 *	Common sanity function for udp and udplite.
 */
static uint8_t *netfn_capwap_dec_l4(struct netfn_capwap_dec *dec, struct sk_buff *skb, uint8_t *start, struct netfn_capwap_prehdr *preh)
{
	struct udphdr *udph = (struct udphdr *)start;

	/*
	 * Check if its a control packet.
	 */
	if (unlikely(ntohs(udph->source) == NETFN_CAPWAP_CTRL_PORT)) {
		NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.control_pkts);
		preh->type |= NETFN_CAPWAP_PKT_TYPE_CTRL;
		return start + sizeof(*udph);
	}

	return start + sizeof(*udph);
}

/*
 * netfn_capwap_dec_l3()
 *	Decapsulate L3 header
 */
static uint8_t *netfn_capwap_dec_l3(struct netfn_capwap_dec *dec, struct sk_buff *skb, uint8_t *start, struct netfn_capwap_prehdr *preh)
{
	struct ipv6hdr *ip6h;
	struct iphdr *iph;

	if (ntohs(skb->protocol) == ETH_P_IP) {
		iph = (struct iphdr *)start;

		preh->dscp = (ipv4_get_dsfield(iph) >> 2);
		skb->protocol = iph->protocol;
		return start + sizeof(*iph);
	}

	ip6h = (struct ipv6hdr *)start;

	preh->dscp = (ipv6_get_dsfield(ip6h) >> 2);
	skb->protocol = ip6h->nexthdr;
	return start + sizeof(*ip6h);
}

/*
 * netfn_capwap_dec_l2()
 *     Decapsulate L2 header
 */
static uint8_t *netfn_capwap_dec_l2(struct netfn_capwap_dec *dec, struct sk_buff *skb, uint8_t *start, struct netfn_capwap_prehdr *preh)
{
       struct ethhdr *ehdr = (struct ethhdr *)start;
       uint8_t next_hdr = 0;
       uint16_t proto;

       proto = ehdr->h_proto;
       next_hdr += ETH_HLEN;

       /*
        * Outer header has a VLAN
        */
       if (eth_type_vlan(proto)) {
               struct vlan_hdr *vh = (struct vlan_hdr *)(start + ETH_HLEN);

               proto = vh->h_vlan_encapsulated_proto;
               preh->vlan_pcp = ((ntohs(vh->h_vlan_TCI) & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT);
               next_hdr += sizeof(*vh);
       }

       skb->protocol = proto;
       return start + next_hdr;
}


/*
 * netfn_capwap_dec()
 *	Decapsulates headers. Converts inner packet from 802.11 to 802.3
 *	and adds preheader if needed.
 */
static bool netfn_capwap_dec(struct netfn_capwap_dec *dec, struct sk_buff *skb, bool *is_frag)
{
	struct netfn_capwap_prehdr preh = {0};
	struct netfn_capwap_hdr_mdata *mdata;
	uint8_t *start, *next_hdr;
	uint16_t exception = 0;

	preh.tunnel_id = dec->id;
	start = next_hdr = skb->data;

	/*
	 * We don't need to reset skb cb as we are overriding
	 * the values read for further use.
	 */
	mdata = NETFN_CAPWAP_CB(skb);

	/************************
	 * Decapsulation starts.*
	 ************************/
	/*
	 * If dtls is enabled we have skip IP and UDP/UDPLite decapsulation.
	 * Its stripped by DTLS.
	 */
	if (dec->features & NETFN_CAPWAP_FEATURES_DTLS) {
		preh.dscp = (skb->priority >> 2);
		preh.vlan_pcp = ((ntohs(skb_vlan_tag_get(skb)) & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT);
		next_hdr = netfn_capwap_dec_capwap(dec, skb, next_hdr, &preh);
		skb->priority = 0;
		NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.dtls_dec_pkts);
		goto skip_headers;
	}

	next_hdr = netfn_capwap_dec_l2(dec, skb, next_hdr, &preh);
	next_hdr = netfn_capwap_dec_l3(dec, skb, next_hdr, &preh);
	next_hdr = netfn_capwap_dec_l4(dec, skb, next_hdr, &preh);
	next_hdr = netfn_capwap_dec_capwap(dec, skb, next_hdr, &preh);

skip_headers:

	/*
	 * We exception packets if:
	 * 1. If its keepalive.
	 * 2. If its a invalid packet.
	 * 3. If its control packet and not fragmented.
	 */
	exception += mdata->keep_alive;
	exception += (preh.type & NETFN_CAPWAP_PKT_TYPE_INVALID);
	exception += ((preh.type & NETFN_CAPWAP_PKT_TYPE_CTRL) && (!mdata->frag));
	if (exception) {
		mdata->exception = 1;
		goto done;
	}

	__skb_pull(skb, next_hdr - start);
	*is_frag = !!mdata->frag;

done:
	return true;
}

/*
 * netfn_capwap_dec_stats_clear()
 *	Clears the decap statistics
 *
 * TODO: Add locks for writing to decap statistics
 */
static ssize_t netfn_capwap_dec_stats_clear(struct file *fp, const char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_capwap_dec *dec = fp->private_data;
	uint32_t reset;

	if (kstrtou32_from_user(ubuf, sz, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	memset(&dec->stats, 0, sizeof(struct netfn_capwap_dec_stats));

	return sz;
}

/*
 * netfn_capwap_dec_stats_show()
 *	Prints the decap stats.
 */
static ssize_t netfn_capwap_dec_stats_show(struct file *fp, char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_capwap_dec *dec = fp->private_data;
	uint64_t *stats = (uint64_t *)&dec->stats;
	size_t dec_stats_sz, stats_sz;
	ssize_t bytes_read = 0;
	size_t size_wr = 0;
	char *buf;
	int i;

	dec_stats_sz = sizeof(struct netfn_capwap_dec_stats) / sizeof(uint64_t) * NETFN_CAPWAP_DEC_STATS_STRLEN;
	stats_sz = (NETFN_CAPWAP_DEC_STATS_WIDTH * NETFN_CAPWAP_DEC_STATS_STRLEN) + dec_stats_sz;

	buf = vzalloc(stats_sz);
	if (!buf) {
		pr_warn("%px: Unable to allocate memory for stats.\n", fp);
		return -ENOMEM;
	}

	/*
	 * Print decap stats.
	 */
	BUILD_BUG_ON(ARRAY_SIZE(g_dec_stats_str) != NETFN_CAPWAP_DEC_STATS_MAX);
	size_wr += scnprintf(buf + size_wr, stats_sz - size_wr, "\n---------------[decap stats start]-------------\n");

	for (i = 0; i < ARRAY_SIZE(g_dec_stats_str); i++, stats++) {
		size_t len = stats_sz - size_wr;
		char *start = buf + size_wr;

		size_wr += scnprintf(start, len, "%-*s\t\t = %llu\n", NETFN_CAPWAP_MAX_STRLEN, g_dec_stats_str[i], READ_ONCE(*stats));
	}

	size_wr += scnprintf(buf + size_wr, stats_sz - size_wr, "---------------[decap stats end]--------------\n");

	bytes_read = simple_read_from_buffer(ubuf, sz, ppos, buf, size_wr);
	vfree(buf);
	return bytes_read;
}

/*
 * Capwap file operations.
 */
static const struct file_operations file_ops = {
	.open = simple_open,
	.llseek = default_llseek,
	.read = netfn_capwap_dec_stats_show,
	.write = netfn_capwap_dec_stats_clear
};

/*
 * netfn_capwap_dec_stats_read()
 *	Reads decap stats.
 */
void netfn_capwap_dec_stats_read(struct netfn_capwap_dec *dec, struct netfn_capwap_tun_stats *stats)
{
	uint64_t *dest = (uint64_t *)&stats->dec;
	uint64_t *src = (uint64_t *)&dec->stats;
	int i;

	for (i = 0; i < (sizeof(dec->stats) / sizeof(uint64_t)); i++) {
		*(uint64_t *)dest = *(uint64_t *)src;
	}
}

/*
 * netfn_capwap_dec_rx_skbs()
 *	Decapsulate and reassemble the skbs.
 */
bool netfn_capwap_dec_rx_skbs(struct netfn_capwap_dec *dec, struct sk_buff_head *q_head)
{
	struct netfn_capwap_tun *nct = container_of(dec, struct netfn_capwap_tun, dec);
	struct sk_buff *skb, *reasm_skb, *old;
	struct netfn_capwap_dec_ctx *ctx;
	struct sk_buff_head q_frags;
	struct sk_buff_head q_free;
	bool is_frag = false;
	uint16_t frag_id;

	rcu_read_lock_bh();

	ctx = rcu_dereference(dec->ctx);
	if (!ctx) {
		rcu_read_unlock_bh();
		return false;
	}

	skb_queue_head_init(&q_frags);
	skb_queue_head_init(&q_free);
	while (likely(skb_queue_len(q_head))) {
		skb = __skb_dequeue(q_head);

		if (likely(skb_queue_len(q_head))) {
			struct sk_buff *nskb = __skb_peek(q_head);
			uint8_t *ndata = nskb->data;

			prefetch(ndata);
			prefetch((uint8_t *)ndata + 64);
		}

		NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.pkts_rcvd);

		/*
		 * Decapsulate the skb.
		 */
		if (!netfn_capwap_dec(dec, skb, &is_frag)) {
			NETFN_CAPWAP_TUN_STATS_INC(&dec->stats.err_dec_failure);
			__skb_queue_head(&q_free, skb);
			continue;
		}

		/*
		 * For control packet we don't process further.
		 */
		if (unlikely(NETFN_CAPWAP_CB(skb)->exception)) {
			netfn_capwap_tun_rx_pri_deliver(nct, skb);
			continue;
		}

		/*
		 * Non-fragmented skb.
		 */
		if (!is_frag) {
			netfn_capwap_dec_convert(dec, skb);
			__skb_queue_tail(&q_frags, skb);
			continue;
		}

		frag_id = NETFN_CAPWAP_CB(skb)->frag_id;

		/*
		 * Check if we can fast reassemble the frag.
		 */
		if (netfn_capwap_dec_can_fast(ctx, frag_id)) {
			reasm_skb = netfn_capwap_dec_fast_reasm(ctx, skb, &q_free);
			if (likely(reasm_skb)) {
				netfn_capwap_dec_convert(dec, reasm_skb);
				__skb_queue_tail(&q_frags, reasm_skb);
				continue;
			}
		}

		/*
		 * Check if corresponding fragments of skb are already parked in slow reasm queue.
		 * If yes we directly send it for slow reassembly.
		 */
		if (netfn_capwap_dec_can_slow(ctx, frag_id)) {
			netfn_capwap_dec_add_slow(ctx, frag_id, skb, &q_free);
			goto slow_reasm;
		}

		/*
		 * If fast cache is empty, received SKB is a fragment of new chain.
		 */
		if (!ctx->reasm_cache) {
			xchg(&ctx->reasm_cache, skb);
			continue;
		}

		/*
		 * Pop the fast reassembly cache
		 */
		old = xchg(&ctx->reasm_cache, NULL);

		/*
		 * Check if these are same fragments or different fragments for slow reassemble
		 */
		if (netfn_capwap_dec_is_frag_chain(old, skb)) {
			netfn_capwap_dec_add_slow(ctx, frag_id, old, &q_free);
			netfn_capwap_dec_add_slow(ctx, frag_id, skb, &q_free);
			goto slow_reasm;
		}

		/*
		 * Update the fast reassembly cache and change the frag_id
		 */
		ctx->reasm_cache = skb;
		frag_id = NETFN_CAPWAP_CB(old)->frag_id;

		/*
		 * Add the old fragment and evict the existing ones from slow reassembly table
		 */
		netfn_capwap_dec_add_slow(ctx, frag_id, old, &q_free);

slow_reasm:
		reasm_skb = netfn_capwap_dec_slow_reasm(ctx, frag_id, &q_free);
		if (reasm_skb) {
			netfn_capwap_dec_convert(dec, reasm_skb);
			__skb_queue_tail(&q_frags, reasm_skb);
		}
	}

	/*
	 * Splice to original queue.
	 */
	skb_queue_splice_tail(&q_frags, q_head);
	rcu_read_unlock_bh();

	/*
	 * Free the fragments.
	 */
	dev_kfree_skb_list_fast(&q_free);
	return true;
}

/*
 * netfn_capwap_dec_init()
 *	Initializes the decap object.
 */
bool netfn_capwap_dec_init(struct netfn_capwap_dec *dec, struct netfn_capwap_dec_cfg *cfg, struct netfn_capwap_tun *nct)
{
	dec->max_frags = cfg->max_frags;
	dec->max_payload_sz = cfg->max_payload_sz;
	dec->features = nct->features;
	dec->id = nct->id;
	dec->nct = nct;

	/*
	 * TODO: Remove this once inner sgt is enabled.
	 */
	if (dec->features & NETFN_CAPWAP_FEATURES_INNER_SGT) {
		pr_warn("%px: Unsupported feature inner sgt.\n", dec);
		return false;
	}

	/*
	 * Allocate debugfs entry.
	 */
	if (!debugfs_create_file("decap", S_IRUGO, nct->dentry, dec, &file_ops)) {
		pr_warn("%p: Failed to create encap debug entry.\n", dec);
		return false;
	}

	return true;
}

/*
 * netfn_capwap_dec_get_err_stats()
 *	Accumulates decapsulation error stats and return sum
 */
uint64_t netfn_capwap_dec_get_err_stats(struct netfn_capwap_dec *dec)
{
	struct netfn_capwap_dec_stats *stats = &dec->stats;
	uint64_t err_stats = 0;

	err_stats += stats->err_dec_failure;
	err_stats += stats->err_max_frags;
	err_stats += stats->err_large_frags;
	err_stats += stats->err_csum_fail;
	err_stats += stats->err_malformed;
	err_stats += stats->err_excess_len;
	err_stats += stats->err_nwireless_len;

	return err_stats;
}

/*
 * netfn_capwap_dec_get_drop_stats()
 *	Accumulates decapsulation drop stats and return sum
 */
uint64_t netfn_capwap_dec_get_drop_stats(struct netfn_capwap_dec *dec)
{
	struct netfn_capwap_dec_stats *stats = &dec->stats;
	uint64_t drop_stats = 0;

	drop_stats += stats->drop_missing_frags;
	drop_stats += stats->drop_queue_full;
	drop_stats += stats->drop_pri_queue_full;

	return drop_stats;
}
