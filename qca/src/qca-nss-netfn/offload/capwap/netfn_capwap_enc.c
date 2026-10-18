/*
 * netfn_capwap_enc.c
 *	Network function's CAPWAP offload encapsulation.
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

#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/ipv6.h>
#include <linux/if_pppox.h>

#include <net/udp.h>

#include "netfn_capwap.h"
#include "netfn_capwap_priv.h"
#include "netfn_capwap_hdr.h"
#include "netfn_capwap_tun.h"
#include "netfn_capwap_enc.h"

#define NETFN_CAPWAP_MAX_STRLEN 25
#define NETFN_CAPWAP_ENC_STATS_MAX (sizeof(struct netfn_capwap_enc_stats) / sizeof(uint64_t))

/*
 * Encap statistics strings
 */
static int8_t *g_enc_stats_str[] = {
	"pkts_rcvd",
	"num_frags",
	"keepalive_pkts",
	"drop_mem_alloc",
	"drop_queue_full",
	"err_dev_tx",
	"err_ver_mis",
	"err_direct_dtls",
	"err_nwireless_len",
	"err_insufficient_hroom",
	"res1",
	"res2",
};

/*
 * netfn_capwap_enc_winfo()
 *	Encapsulate with winfo
 */
static inline void netfn_capwap_enc_winfo(struct netfn_capwap_enc *enc, struct sk_buff *skb, struct netfn_capwap_enc_mdata *mdata)
{
	struct netfn_capwap_winfo *winfo = (struct netfn_capwap_winfo *)mdata->phdr.wl_info;

	/*
	 * Add winfo only if set in winfo.
	 */
	if (mdata->phdr.type & NETFN_CAPWAP_PKT_TYPE_WINFO) {
		memcpy(__skb_push(skb, sizeof(*winfo)), winfo, sizeof(*winfo));
	}
}

/*
 * netfn_capwap_enc_capwap()
 *	Encapsulate the CAPWAP header
 */
static inline void netfn_capwap_enc_capwap(struct netfn_capwap_enc *enc, struct sk_buff *skb, struct netfn_capwap_enc_mdata *mdata)
{
	struct netfn_capwap_hdr *nch = (struct netfn_capwap_hdr *)__skb_push(skb, sizeof(*nch));
	struct netfn_capwap_prehdr *phdr = &mdata->phdr;
	union netfn_capwap_hdr_info info = {0};
	union netfn_capwap_hdr_frag frag = {0};
	uint8_t hlen = sizeof(*nch);

	/*
	 * Check if this packet has wireless info.
	 */
	if (phdr->type & NETFN_CAPWAP_PKT_TYPE_WINFO) {
		bool is_winfo = !!(mdata->first_frag || !mdata->frag);
		hlen += (is_winfo * phdr->nwireless * NETFN_CAPWAP_WIRELESS_INFO_LEN);
		info.fields.wbit = is_winfo;
	}

	/*
	 * If packet is 802.11 packet.
	 */
	if (phdr->type & NETFN_CAPWAP_PKT_TYPE_80211) {
		info.fields.tbit = info.fields.wbid = 1;
	}

	netfn_capwap_hdr_set_radio_id(&info, phdr->rid);
	info.fields.fbit = mdata->frag;
	info.fields.lbit = mdata->frag_end;

	info.fields.kbit = !!(phdr->type & NETFN_CAPWAP_PKT_TYPE_ENC_KEEPALIVE);
	info.fields.mbit = 0;
	info.fields.flags = 0;
	frag.fields.id = mdata->frag_id;
	netfn_capwap_hdr_set_hlen(&info, hlen);
	netfn_capwap_hdr_set_frag_offset(&frag, mdata->frag_offset);

	nch->info = htonl(info.word);
	nch->frag = htonl(frag.word);
}

/*
 * netfn_capwap_enc_l4()
 *	Encapsulate the UDP/UDPLite header
 */
static inline void netfn_capwap_enc_l4(struct netfn_capwap_enc *enc, struct sk_buff *skb, struct netfn_capwap_enc_mdata *mdata)
{
	struct udphdr *udph = (struct udphdr *)__skb_push(skb, sizeof(*udph));
	struct netfn_capwap_enc_hdr *hdr = &enc->hdr;

	skb_reset_transport_header(skb);

	udph->dest = hdr->dest_port;
	udph->source = hdr->src_port;

	/*
	 * udph->len is set to payload len in case protocol is udp,
	 * else indicates checksum coverage for udplite protocol.
	 */
	udph->len = (hdr->proto_next_hdr == IPPROTO_UDP) ? htons(skb->len) : htons(hdr->csum_cov);

	/*
	 * check = 0 means no checksum, however the intent is to have it offloaded to EDMA.
	 * skb->ip_summed should be set to CHECKSUM_PARTIAL
	 */
	udph->check = 0;
}

/*
 * netfn_capwap_enc_l3()
 *	Encapsulate the IP header
 */
static inline void netfn_capwap_enc_l3(struct netfn_capwap_enc *enc, struct sk_buff *skb, struct netfn_capwap_enc_mdata *mdata)
{
	struct netfn_capwap_enc_hdr *hdr = &enc->hdr;
	struct ipv6hdr *ip6h;
	struct iphdr *iph;

	skb_reset_network_header(skb);
	skb->ip_summed = CHECKSUM_PARTIAL;

	if (hdr->ip_version == IPVERSION) {
		iph = (struct iphdr *)__skb_push(skb, sizeof(*iph));
		skb->protocol = htons(ETH_P_IP);

		iph->version = 4;
		iph->ihl = sizeof(*iph) >> 2;
		iph->tos = (mdata->phdr.dscp << 2);
		iph->tot_len = htons(skb->len);
		iph->id = htons(enc->ipv4_id++);
		iph->frag_off = 0;
		iph->ttl = hdr->ttl;
		iph->frag_off = htons(IP_DF);
		iph->protocol = hdr->proto_next_hdr;
		memcpy(&iph->saddr, &hdr->src_ip.ip4, sizeof(iph->saddr));
		memcpy(&iph->daddr, &hdr->dest_ip.ip4, sizeof(iph->daddr));
		return;
	}

	/*
	 * IPv6 header encapsulation
	 */
	ip6h = (struct ipv6hdr *)__skb_push(skb, sizeof(*ip6h));
	skb->protocol = htons(ETH_P_IPV6);

	ip6h->version = 6;
	ip6_flow_hdr(ip6h, (mdata->phdr.dscp << 2), 0);	/* Sets dscp and flow label. */
	ip6h->nexthdr = hdr->proto_next_hdr;
	ip6h->payload_len = htons(skb->len - sizeof(*ip6h));
	ip6h->hop_limit = hdr->ttl;
	memcpy(ip6h->saddr.s6_addr32, &hdr->src_ip.ip6, sizeof(ip6h->saddr.s6_addr32));
	memcpy(ip6h->daddr.s6_addr32, &hdr->dest_ip.ip6, sizeof(ip6h->daddr.s6_addr32));
}

/*
 * netfn_capwap_enc_snap_hdr()
 *	Copy snap header with next protocol.
 */
static inline void netfn_capwap_enc_snap_hdr(struct netfn_capwap_enc *enc, struct sk_buff *skb)
{
	struct __snap_hdr {
		uint8_t hdr[NETFN_CAPWAP_SNAP_HDR_LEN];
	};
	uint16_t *next_proto = (uint16_t *)__skb_push(skb, sizeof(skb->protocol));
	struct __snap_hdr *snap = __skb_push(skb, sizeof(*snap));

	*snap = *((struct __snap_hdr *)enc->snap_hdr);
	*next_proto = skb->protocol;
}

static inline void netfn_capwap_enc_add_hdr(struct netfn_capwap_enc *enc, struct sk_buff *skb, struct netfn_capwap_enc_mdata *mdata)
{
	/*
	 * We insert winfo if preheader has winfo type set and if:
	 * 1. If its first frag OR
	 * 2. If its a non-fragmented packet
	 */
	if (mdata->first_frag || !mdata->frag) {
		netfn_capwap_enc_winfo(enc, skb, mdata);
	}

	netfn_capwap_enc_capwap(enc, skb, mdata);

	/*
	 * We don't encapsulate with UDP and IP if dtls is enabled.
	 * DTLS adds these for us.
	 * SKB Priority used to store TOS.
	 */
	if (enc->features & NETFN_CAPWAP_FEATURES_DTLS) {
		skb->priority = mdata->phdr.dscp << 2;
		return;
	}

	netfn_capwap_enc_l4(enc, skb, mdata);	/* UDP or UDPLite */
	netfn_capwap_enc_l3(enc, skb, mdata);
}

/*
 * netfn_capwap_enc_80211()
 *	Converts the inner capwap packet from 802.3 to 802.11
 */
static inline void netfn_capwap_enc_80211(struct netfn_capwap_enc *enc, struct sk_buff *skb, void *preh)
{
	struct netfn_capwap_prehdr *phdr = preh;
	struct ieee80211_hdr_3addr *wlan_hdr;
	uint16_t fc = 0, nwireless;
	uint8_t src_mac[ETH_ALEN];
	uint8_t dst_mac[ETH_ALEN];
	struct ethhdr *eth;

	eth = (struct ethhdr *)skb->data;
	__skb_pull(skb, sizeof(*eth));
	nwireless = phdr->nwireless;

	/*
	 * Copy mac addresses.
	 */
	ether_addr_copy(src_mac, eth->h_source);
	ether_addr_copy(dst_mac, eth->h_dest);
	skb->protocol = eth->h_proto;

	/*
	 * Insert trustsec if enabled.
	 */
	if (enc->features & NETFN_CAPWAP_FEATURES_INNER_SGT) {
		/*
		 * TODO: Add support when trustsec is enabled.
		 */
		BUG_ON(1);
	}

	/*
	 * Encapsulate with snap header.
	 */
	netfn_capwap_enc_snap_hdr(enc, skb);

	/*
	 * Insert QoS header.
	 */
	if (enc->features & NETFN_CAPWAP_FEATURES_WLAN_QOS) {
		uint16_t *qos = (uint16_t *)__skb_push(skb, sizeof(phdr->wireless_qos));

		fc |= IEEE80211_STYPE_QOS_DATA;
		*qos = phdr->wireless_qos;
	}

	/*
	 * Insert the wlan header.
	 */
	wlan_hdr = (struct ieee80211_hdr_3addr *)__skb_push(skb, sizeof(*wlan_hdr));
	skb_reset_inner_mac_header(skb);

	/*
	 * Copy over the MAC addresses.
	 */
	ether_addr_copy(wlan_hdr->addr1, (uint8_t *)&enc->bssid);
	ether_addr_copy(wlan_hdr->addr2, src_mac);
	ether_addr_copy(wlan_hdr->addr3, dst_mac);

	/*
	 * Fill frame control, duration and sequence.
	 */
	fc |= (IEEE80211_FTYPE_DATA | IEEE80211_FCTL_TODS);
	wlan_hdr->frame_control = htons(fc);

	/*
	 * Set duration and sequence.
	 */
	wlan_hdr->duration_id = htons(0);
	wlan_hdr->seq_ctrl = htons(phdr->vapid);
}

/*
 * netfn_capwap_enc_frag()
 *	CAPWAP fragmentation routine
 */
static inline bool netfn_capwap_enc_frag(struct netfn_capwap_enc *enc, struct sk_buff *orig_skb, struct sk_buff_head *q_frag, struct netfn_capwap_enc_mdata *mdata)
{
	struct netfn_capwap_enc_stats *stats = &enc->stats;
	uint8_t *next_data = orig_skb->data;
	uint16_t total_len = orig_skb->len;
	uint16_t headroom, tailroom;
	uint16_t mtu = enc->mtu;
	int offset = 0;
	int frag_len;

	headroom = skb_headroom(orig_skb);
	tailroom = skb_tailroom(orig_skb);

	/*
	 * If buffer size is less than next hop MTU no need to fragment.
	 */
	mdata->frag_id = enc->frag_id++;
	if (total_len <= mtu) {
		NETFN_CAPWAP_TUN_STATS_INC(&stats->num_frags);
		netfn_capwap_enc_add_hdr(enc, orig_skb, mdata);
		__skb_queue_head(q_frag, orig_skb);
		return true;
	}

	frag_len = mtu;
	total_len -= frag_len;
	skb_trim(orig_skb, frag_len);
	next_data += frag_len;

	NETFN_CAPWAP_TUN_STATS_INC(&stats->num_frags);
	mdata->frag_end = false;
	mdata->first_frag = true;
	mdata->frag_offset = offset;
	mdata->frag = true;
	offset += orig_skb->len;
	netfn_capwap_enc_add_hdr(enc, orig_skb, mdata);
	__skb_queue_head(q_frag, orig_skb);

	/*
	 * This is a fragment loop.
	 */
	while (total_len > 0) {
		struct sk_buff *skb;
		ssize_t skb_size;

		frag_len = min(total_len, mtu);
		total_len -= frag_len;
		skb_size = frag_len + enc->outer_hdr_len + headroom + tailroom;

		skb = netdev_alloc_skb_fast(NULL, skb_size);
		if (!skb) {
#ifdef CONFIG_64BIT
			pr_warn_ratelimited("%px: Unable to allocate memory for fragment len(%ld).\n", enc, skb_size);
#else
			pr_warn_ratelimited("%px: Unable to allocate memory for fragment len(%d).\n", enc, skb_size);
#endif
			NETFN_CAPWAP_TUN_STATS_INC(&stats->drop_mem_alloc);
			return false;
		}

		skb_reserve(skb, enc->outer_hdr_len + headroom);
		prefetchw(skb->data);
		memcpy(__skb_put(skb, frag_len), next_data, frag_len);
		NETFN_CAPWAP_TUN_STATS_INC(&stats->num_frags);
		mdata->frag_end = !total_len;
		mdata->first_frag = false;
		mdata->frag_offset = offset;
		mdata->frag = true;
		offset += skb->len;
		netfn_capwap_enc_add_hdr(enc, skb, mdata);
		__skb_queue_tail(q_frag, skb);
		next_data += frag_len;
	}

	return true;
}

/*
 * netfn_capwap_enc_stats_clear()
 *	Clears the encap statistics
 *
 * TODO: Add locks for writing to encap statistics
 */
static ssize_t netfn_capwap_enc_stats_clear(struct file *fp, const char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_capwap_dec *enc = fp->private_data;
	uint32_t reset;

	if (kstrtou32_from_user(ubuf, sz, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	memset(&enc->stats, 0, sizeof(struct netfn_capwap_enc_stats));

	return sz;
}


/*
 * netfn_capwap_enc_stats_show()
 *	Prints the encap stats.
 */
static ssize_t netfn_capwap_enc_stats_show(struct file *fp, char __user *ubuf, size_t sz, loff_t *ppos)
{
	struct netfn_capwap_enc *enc = fp->private_data;
	uint64_t *stats = (uint64_t *)&enc->stats;
	size_t enc_stats_sz, stats_sz;
	ssize_t bytes_read = 0;
	size_t size_wr = 0;
	char *buf;
	int i;

	enc_stats_sz = sizeof(struct netfn_capwap_enc_stats) / sizeof(uint64_t) * NETFN_CAPWAP_ENC_STATS_STRLEN;
	stats_sz = (NETFN_CAPWAP_ENC_STATS_WIDTH * NETFN_CAPWAP_ENC_STATS_STRLEN) + enc_stats_sz;

	buf = vzalloc(stats_sz);
	if (!buf) {
		pr_warn("%px: Unable to allocate memory for stats.\n", fp);
		return -ENOMEM;
	}

	/*
	 * Print encap stats.
	 */
	BUILD_BUG_ON(ARRAY_SIZE(g_enc_stats_str) != NETFN_CAPWAP_ENC_STATS_MAX);
	size_wr += scnprintf(buf + size_wr, stats_sz - size_wr, "\n---------------[encap stats start]-------------\n");

	for (i = 0; i < ARRAY_SIZE(g_enc_stats_str); i++, stats++) {
		size_t len = stats_sz - size_wr;
		char *start = buf + size_wr;

		size_wr += scnprintf(start, len, "%-*s\t\t = %llu\n", NETFN_CAPWAP_MAX_STRLEN, g_enc_stats_str[i], READ_ONCE(*stats));
	}

	size_wr += scnprintf(buf + size_wr, stats_sz - size_wr, "---------------[encap stats end]--------------\n");

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
	.read = netfn_capwap_enc_stats_show,
	.write = netfn_capwap_enc_stats_clear
};

/*
 * netfn_capwap_enc_stats_read()
 *	Reads encap stats.
 */
void netfn_capwap_enc_stats_read(struct netfn_capwap_enc *enc, struct netfn_capwap_tun_stats *stats)
{
	uint64_t *dest = (uint64_t *)&stats->enc;
	uint64_t *src = (uint64_t *)&enc->stats;
	int i;

	for (i = 0; i < (sizeof(enc->stats) / sizeof(uint64_t)); i++) {
		*(uint64_t *)dest = *(uint64_t *)src;
	}
}

/*
 * netfn_capwap_enc()
 *	Parse, fragment and encapsulate.
 */
void netfn_capwap_enc(struct netfn_capwap_enc *enc, struct sk_buff *orig_skb, struct sk_buff_head *q_frag)
{
	struct netfn_capwap_enc_mdata mdata = {0};
	struct netfn_capwap_prehdr *phdr;
	struct sk_buff_head tmp;

	skb_queue_head_init(&tmp);
	NETFN_CAPWAP_TUN_STATS_INC(&enc->stats.pkts_rcvd);

	/*
	 * Pull the meta header.
	 */
	mdata.phdr = NETFN_CAPWAP_CB(orig_skb)->phdr;
	phdr = &mdata.phdr;

	/*
	 * Bypass capwap processing if BYPASS bit is set.
	 */
	if (unlikely(phdr->type & NETFN_CAPWAP_PKT_TYPE_BYPASS_KEEPALIVE)) {
		struct netfn_capwap_enc_stats *stats = &enc->stats;

		NETFN_CAPWAP_TUN_STATS_INC(&stats->keepalive_pkts);
		__skb_queue_head(q_frag, orig_skb);
		return;
	}

	/*
	 * If inner is 802.3 then we convert it to 802.11
	 */
	if (phdr->type & NETFN_CAPWAP_PKT_TYPE_8023) {
		netfn_capwap_enc_80211(enc, orig_skb, &mdata.phdr);
		mdata.phdr.type |= NETFN_CAPWAP_PKT_TYPE_80211;
	}

	/*
	 * Fragment the skb if needed and Encapsulate.
	 */
	if (unlikely(!netfn_capwap_enc_frag(enc, orig_skb, &tmp, &mdata))) {
		pr_warn_ratelimited("%px: Fragmentation failed\n", enc);
		dev_kfree_skb_list_fast(&tmp);
		return;
	}

	skb_queue_splice_tail(&tmp, q_frag);
	return;
}

/*
 * netfn_capwap_enc_init()
 *	Encapsulation initialization.
 */
bool netfn_capwap_enc_init(struct netfn_capwap_enc *enc, struct netfn_capwap_enc_cfg *cfg, struct netfn_tuple *ntuple, struct netfn_capwap_tun *nct)
{
	struct netfn_tuple_5tuple *tuple = &ntuple->tuples.tuple_5;
	struct netfn_capwap_enc_hdr *hdr = &enc->hdr;

	ether_addr_copy(&enc->bssid, cfg->bssid);
	enc->outer_hdr_len = sizeof(struct netfn_capwap_winfo);
	enc->features = nct->features;

	/*
	 * Add vlan and pppoe overhead.
	 */
	if (enc->flags & NETFN_CAPWAP_ENC_FLAG_VLAN) {
		enc->outer_hdr_len += sizeof(struct vlan_hdr);
	}

	if (enc->flags & NETFN_CAPWAP_ENC_FLAG_PPPOE) {
		enc->outer_hdr_len += sizeof(struct pppoe_hdr);
	}

	if (ntuple->ip_version == IPVERSION) {
		hdr->ip_version = ntuple->ip_version;
		hdr->src_ip.ip4 = tuple->src_ip.ip4;
		hdr->dest_ip.ip4 = tuple->dest_ip.ip4;

		enc->outer_hdr_len += sizeof(struct iphdr);
	} else {
		hdr->ip_version = ntuple->ip_version;

		hdr->src_ip.ip6 = tuple->src_ip.ip6;
		hdr->dest_ip.ip6 = tuple->dest_ip.ip6;

		enc->outer_hdr_len += sizeof(struct ipv6hdr);
	}

	enc->frag_id = 1;
	enc->ipv4_id = 1;
	hdr->tos = cfg->tos;
	hdr->csum_cov = cfg->csum_cov;
	hdr->src_port = tuple->l4_src_ident;
	hdr->dest_port = tuple->l4_dest_ident;

	if ((tuple->protocol != IPPROTO_UDP) && (tuple->protocol != IPPROTO_UDPLITE)) {
		pr_warn("%p: Invalid L4 proto(%d).\n", enc, tuple->protocol);
		return false;
	}

	hdr->proto_next_hdr = tuple->protocol;

	enc->outer_hdr_len += sizeof(struct udphdr);
	enc->outer_hdr_len += sizeof(struct netfn_capwap_hdr);
	enc->mtu = ((cfg->mtu - enc->outer_hdr_len) & ~(sizeof(uint64_t) - 1));
	memcpy(enc->snap_hdr, cfg->snap_hdr, NETFN_CAPWAP_SNAP_HDR_LEN);
	hdr->ttl = cfg->ttl;

	/*
	 * Allocate debugfs entry.
	 */
	if (!debugfs_create_file("encap", S_IRUGO, nct->dentry, enc, &file_ops)) {
		pr_warn("%p: Failed to create encap debug entry.\n", enc);
		return false;
	}

	return true;
}

/*
 * netfn_capwap_enc_mtu_update()
 *	Update the encap MTU
 */
void netfn_capwap_enc_mtu_update(struct netfn_capwap_enc *enc, unsigned int mtu)
{
	enc->mtu = ((mtu - enc->outer_hdr_len) & ~(sizeof(uint64_t) - 1));
}

/*
 * netfn_capwap_enc_get_err_stats()
 *     Accumulates encapsulation error stats and return sum
 */
uint64_t netfn_capwap_enc_get_err_stats(struct netfn_capwap_enc *enc)
{
	struct netfn_capwap_enc_stats *stats = &enc->stats;
	uint64_t err_stats = 0;

	err_stats += stats->err_ver_mis;
	err_stats += stats->err_direct_dtls;
	err_stats += stats->err_nwireless_len;
	err_stats += stats->err_insufficient_hroom;
	err_stats += stats->err_dev_tx;

	return 0;
}

/*
 * netfn_capwap_enc_get_drop_stats()
 *	Accumulates encapsulation drop stats and return sum
 */
uint64_t netfn_capwap_enc_get_drop_stats(struct netfn_capwap_enc *enc)
{
	struct netfn_capwap_enc_stats *stats = &enc->stats;
	uint64_t drop_stats = 0;

	drop_stats += stats->drop_mem_alloc;
	drop_stats += stats->drop_queue_full;

	return drop_stats;
}
