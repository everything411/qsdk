/*
 **************************************************************************
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */
#include <linux/list.h>
#include <linux/skbuff.h>
#include <net/netfilter/nf_conntrack_core.h>
#include "nss_udp_st_public.h"

/*
 * nss_udp_st_seq_check()
 *      checks for potential dropped or OOO pkts
 */
static void nss_udp_st_seq_check(struct nss_udp_st_rules *rule, uint64_t seq)
{
	if (seq > rule->seq_greatest) {
		if (seq != rule->seq_greatest + 1)
			atomic64_add(seq - rule->seq_greatest, &nust.stats.p_stats.dropped);
		rule->seq_greatest = seq;
	} else if (seq < rule->seq_greatest) {
		atomic64_sub(1, &nust.stats.p_stats.dropped);
		atomic64_inc(&nust.stats.p_stats.ooo);
	}
}

/*
 * nss_udp_st_process_payload()
 *      will process the items from skb payload
 */
static void nss_udp_st_process_payload(struct sk_buff *skb, struct nss_udp_st_rules *rule, uint8_t ip_version)
{
        uint64_t time;
	uint64_t latency = 0;
	uint64_t le_timestamp;
	uint16_t hdr_sz;
	struct nss_udp_st_timestamp_info *ts_info;

        time = ktime_get_real_ns();
	if (ip_version == NSS_UDP_ST_FLAG_IPV4) {
		hdr_sz = sizeof(struct iphdr);
	} else if (ip_version == NSS_UDP_ST_FLAG_IPV6) {
		hdr_sz = sizeof(struct ipv6hdr);
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		return;
	}
	hdr_sz += sizeof(struct udphdr);

	ts_info = (struct nss_udp_st_timestamp_info *)skb_pull(skb, hdr_sz);
	do_div(time, 1000000);
	le_timestamp = be64_to_cpu(ts_info->timestamp);
	if (time > le_timestamp) {
		latency = time - le_timestamp;
	}

	atomic64_add(latency, &nust.stats.total_latency);
	if (latency < atomic64_read(&nust.stats.p_stats.min_latency)) {
		atomic64_set(&nust.stats.p_stats.min_latency, latency);
	}

	if (latency > atomic64_read(&nust.stats.p_stats.max_latency)) {
		atomic64_set(&nust.stats.p_stats.max_latency, latency);
	}

	nss_udp_st_seq_check(rule, be64_to_cpu(ts_info->seq));
}

/*
 * nss_udp_st_rx_ipv4_pre_routing_hook()
 *	pre-routing hook into netfilter packet monitoring point for IPv4
 */
unsigned int nss_udp_st_rx_ipv4_pre_routing_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct udphdr *uh;
	struct iphdr *iph;
	struct nss_udp_st_rules *rule = NULL;
	struct nss_udp_st_rules *n = NULL;

	iph = (struct iphdr *)skb_network_header(skb);

	/*
	 * Not a UDP speedtest packet
	 */
	if (iph->protocol != IPPROTO_UDP) {
		return NF_ACCEPT;
	}

	uh = (struct udphdr *)skb_transport_header(skb);

	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {
		/*
		 * If incoming packet matches 5tuple, it is a speedtest packet.
		 * Increase Rx packet stats and drop packet.
		 */
		if ((rule->flags & NSS_UDP_ST_FLAG_IPV4) &&
			(rule->sip.ip.ipv4 == ntohl(iph->daddr)) &&
			(rule->dip.ip.ipv4 == ntohl(iph->saddr)) &&
			(rule->sport == ntohs(uh->dest)) &&
			(rule->dport == ntohs(uh->source) || rule->dport == 0)) {
				if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
					nss_udp_st_process_payload(skb, rule, NSS_UDP_ST_FLAG_IPV4);
				}
				nss_udp_st_update_stats(ntohs(iph->tot_len) + sizeof(struct ethhdr));
				kfree_skb(skb);
				return NF_STOLEN;
		}
	}
	return NF_ACCEPT;
}

/*
 * nss_udp_st_rx_ipv6_pre_routing_hook()
 *	pre-routing hook into netfilter packet monitoring point for IPv6
 */
unsigned int nss_udp_st_rx_ipv6_pre_routing_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
	struct udphdr *uh;
	struct ipv6hdr *iph;
	struct in6_addr saddr;
	struct in6_addr daddr;
	struct nss_udp_st_rules *rule = NULL;
	struct nss_udp_st_rules *n = NULL;

	iph = (struct ipv6hdr *)skb_network_header(skb);

	/*
	 * Not a UDP speedtest packet
	 */
	if (iph->nexthdr != IPPROTO_UDP) {
		return NF_ACCEPT;
	}

	uh = (struct udphdr *)skb_transport_header(skb);

	nss_udp_st_get_ipv6_addr_ntoh(iph->saddr.s6_addr32, saddr.s6_addr32);
	nss_udp_st_get_ipv6_addr_ntoh(iph->daddr.s6_addr32, daddr.s6_addr32);

	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {

		/*
		 * If incoming packet matches 5tuple, it is a speedtest packet.
		 * Increase Rx packet stats and drop packet.
		 */
		if ((rule->flags & NSS_UDP_ST_FLAG_IPV6) &&
			(nss_udp_st_compare_ipv6(rule->sip.ip.ipv6, daddr.s6_addr32)) &&
			(nss_udp_st_compare_ipv6(rule->dip.ip.ipv6, saddr.s6_addr32)) &&
			(rule->sport == ntohs(uh->dest)) &&
			(rule->dport == ntohs(uh->source) || rule->dport == 0)) {
				if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
					nss_udp_st_process_payload(skb, rule, NSS_UDP_ST_FLAG_IPV6);
				}
				nss_udp_st_update_stats(ntohs(iph->payload_len) + sizeof(struct ethhdr));
				kfree_skb(skb);
				return NF_STOLEN;
		}
	}
	return NF_ACCEPT;
}
