/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/list.h>
#include <linux/string.h>
#include <linux/hrtimer.h>
#include <linux/moduleparam.h>
#include <linux/math64.h>
#include <linux/ethtool.h>
#include <linux/rtnetlink.h>
#include <net/act_api.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <linux/if_vlan.h>
#include <linux/if_pppox.h>
#include <net/ip6_checksum.h>
#ifdef NSS_UDP_ST_DRV_VP_ENABLE
#include <ppe_drv_iface.h>
#include <ppe_vp_tx.h>
#include <nss_ppe_tun_drv.h>
#endif
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
#include <nss_dp_api_if.h>
#endif
#ifdef NSS_UDP_ST_HGU
#include <ppe_drv_veip.h>
#endif
#include "nss_udp_st_public.h"

int tx_timer_flag[NR_CPUS];
static ktime_t kt;
static struct hrtimer tx_hr_timer[NR_CPUS];
static enum hrtimer_restart tx_hr_restart[NR_CPUS] = {HRTIMER_NORESTART};
static struct vlan_hdr vh;
static struct net_device *xmit_dev;
static struct pppoe_opt info;
static uint16_t l2_overhead;

struct work_struct udp_st_work[NR_CPUS];	/* Work struct */
struct workqueue_struct *udp_st_wq[NR_CPUS];	/* workqueue struct */

static uint32_t nss_udp_st_tx_max_num_pkt = NSS_UDP_ST_TX_DEFAULT_MAX_NUM_PKT;
module_param(nss_udp_st_tx_max_num_pkt, uint, 0644);
MODULE_PARM_DESC(nss_udp_st_tx_max_num_pkt, "Max packets generated per rule per timer tick");

/*
 * nss_udp_st_generate_ipv4_hdr()
 *	generate ipv4 header
 */
static inline void nss_udp_st_generate_ipv4_hdr(struct iphdr *iph, uint16_t ip_len, struct nss_udp_st_rules *rules)
{
	iph->version = 4;
	iph->ihl = 5;
	iph->tos = nust.config.dscp;
	iph->tot_len = htons(ip_len);
	iph->id = 0;
	iph->frag_off = 0;
	iph->ttl = 64;
	iph->protocol = IPPROTO_UDP;
	iph->check = 0;
	iph->saddr = htonl(rules->sip.ip.ipv4);
	iph->daddr = htonl(rules->dip.ip.ipv4);
	iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
}

/*
 * nss_udp_st_generate_ipv6_hdr()
 *	generate ipv6 header
 */
static inline void nss_udp_st_generate_ipv6_hdr(struct ipv6hdr *ipv6h, uint16_t ip_len, struct nss_udp_st_rules *rules)
{
	struct in6_addr addr;

	ipv6h->version = 6;
	memset(&ipv6h->flow_lbl, 0, sizeof(ipv6h->flow_lbl));
	ipv6h->nexthdr = IPPROTO_UDP;
	ipv6h->payload_len = htons(ip_len - sizeof(*ipv6h));
	ipv6h->hop_limit = 64;
	nss_udp_st_get_ipv6_addr_hton(rules->sip.ip.ipv6, addr.s6_addr32);
	memcpy(ipv6h->saddr.s6_addr32, addr.s6_addr32, sizeof(ipv6h->saddr.s6_addr32));
	nss_udp_st_get_ipv6_addr_hton(rules->dip.ip.ipv6, addr.s6_addr32);
	memcpy(ipv6h->daddr.s6_addr32, addr.s6_addr32, sizeof(ipv6h->daddr.s6_addr32));
}

/*
 * nss_udp_st_generate_udp_hdr()
 *	generate udp header
 */
static void nss_udp_st_generate_udp_hdr(struct udphdr *uh, uint16_t udp_len, struct nss_udp_st_rules *rules)
{

	uh->source = htons(rules->sport);
	uh->dest = htons(rules->dport);
	uh->len = htons(udp_len);
	uh->check = 0;

	if (rules->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		uh->check = csum_tcpudp_magic(htonl(rules->sip.ip.ipv4), htonl(rules->dip.ip.ipv4), udp_len, IPPROTO_UDP,
		csum_partial(uh, udp_len, 0));
	} else if (rules->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		struct in6_addr saddr;
		struct in6_addr daddr;

		nss_udp_st_get_ipv6_addr_hton(rules->sip.ip.ipv6, saddr.s6_addr32);
		nss_udp_st_get_ipv6_addr_hton(rules->dip.ip.ipv6, daddr.s6_addr32);

		uh->check = csum_ipv6_magic(&saddr, &daddr, udp_len, IPPROTO_UDP,
		csum_partial(uh, udp_len, 0));
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		return;
	}

	if (uh->check == 0) {
		uh->check = CSUM_MANGLED_0;
	}
}

/*
 * nss_udp_st_generate_eth_hdr()
 *	generate L2 header
 */
static inline void nss_udp_st_generate_eth_hdr(struct sk_buff *skb, const uint8_t *src_mac, uint8_t *dst_mac)
{
	struct ethhdr *eh = (struct ethhdr *)skb_push(skb, ETH_HLEN);
	skb_reset_mac_header(skb);

	eh->h_proto = skb->protocol;
	memcpy(eh->h_source, src_mac, ETH_ALEN);
	memcpy(eh->h_dest, dst_mac, ETH_ALEN);
}

/*
 * nss_udp_st_generate_vlan_hdr
 *	Generate VLAN header
 */
static void nss_udp_st_generate_vlan_hdr(struct sk_buff *skb, struct net_device *ndev)
{
	struct vlan_hdr *vhdr;

	skb_push(skb, VLAN_HLEN);
	vhdr = (struct vlan_hdr *)skb->data;
	vhdr->h_vlan_TCI = htons(vh.h_vlan_TCI);
	vhdr->h_vlan_encapsulated_proto = skb->protocol;
	skb->protocol = htons(vh.h_vlan_encapsulated_proto);
}

/*
 * nss_udp_st_generate_pppoe_hdr
 *	Generate PPPoE header
 */
static void nss_udp_st_generate_pppoe_hdr(struct sk_buff *skb, uint16_t ppp_protocol)
{
	struct pppoe_hdr *ph;
	unsigned char *pp;
	unsigned int data_len;

	/*
	 * Insert the PPP header protocol
	 */
	pp = skb_push(skb, 2);
	put_unaligned_be16(ppp_protocol, pp);

	data_len = skb->len;

	ph = (struct pppoe_hdr *)skb_push(skb, sizeof(*ph));
	skb_reset_network_header(skb);

	/*
	 * Headers in skb will look like in below sequence
	 *	| PPPoE hdr(6 bytes) | PPP hdr (2 bytes) | L3 hdr |
	 *
	 *	The length field in the PPPoE header indicates the length of the PPPoE payload which
	 *	consists of a 2-byte PPP header plus a skb->len.
	 */
	ph->ver = 1;
	ph->type = 1;
	ph->code = 0;
	ph->sid = (uint16_t)info.pa.sid;
	ph->length = htons(data_len);

	skb->protocol = htons(ETH_P_PPP_SES);
}

/*
 * nss_udp_st_add_seq_tstamp()
 *	fill the payload with seq num and timestamp
 */
static void nss_udp_st_add_seq_tstamp(struct sk_buff *skb, struct nss_udp_st_rules *rules)
{
	u64 time;
	unsigned char *data;
	struct nss_udp_st_timestamp_info ts_info;

	data = skb_put(skb, sizeof(struct nss_udp_st_timestamp_info));
	ts_info.seq = cpu_to_be64(rules->seq);
	rules->seq++;
	time = ktime_get_real_ns();
	do_div(time, 1000000);
	ts_info.timestamp = cpu_to_be64(time);

	memcpy(data, &ts_info, sizeof(struct nss_udp_st_timestamp_info));
}

#ifdef NSS_UDP_ST_DRV_VP_ENABLE
/*
 * nss_udp_st_tx_packets_vp()
 *	allocate, populate and send tx packet to vp
 */
static void nss_udp_st_tx_packets_vp(struct net_device *ndev, struct nss_udp_st_rules *rules, int cpu)
{
	struct ppe_vp_tx_info tx_info = {0};
	struct sk_buff *skb;
	size_t skb_sz;
	size_t pkt_sz;
	uint16_t udp_len;
	struct udphdr *uh;
	struct iphdr *iph;
	struct ipv6hdr *ipv6h;

	if (!rules->tun_dev) {
		udp_st_err("tun_dev is NULL for rule_id=%u, skipping\n", rules->rule_id);
		return;
	}

	pkt_sz = nust.config.buffer_sz;
	skb_sz = NSS_UDP_ST_MIN_HEADROOM + pkt_sz + sizeof(struct ethhdr) + NSS_UDP_ST_MIN_TAILROOM + SMP_CACHE_BYTES;

	skb = dev_alloc_skb(skb_sz);
	if (!skb) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_MEMORY_FAILURE]);
		return;
	}

	if (rules->ip_version == NSS_UDP_ST_FLAG_IPV4) {
                udp_len = pkt_sz - sizeof(*iph);
        } else if (rules->ip_version == NSS_UDP_ST_FLAG_IPV6) {
                udp_len = pkt_sz - sizeof(*ipv6h);
        } else {
                atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
                dev_kfree_skb_any(skb);
                return;
        }

	if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
		nss_udp_st_add_seq_tstamp(skb, rules);
		skb_put(skb, udp_len - sizeof(struct nss_udp_st_timestamp_info) - sizeof(*uh));
	} else {
		skb_put(skb, udp_len - sizeof(*uh));
	}

	/*
	 * tx packet
	 */
	skb->dev = rules->tun_dev;
	tx_info.type = PPE_VP_TX_TYPE_NONE;

	if (!ppe_vp_tx_to_vp(rules->vp_num, &tx_info, skb)) {
		udp_st_err("Dropping skb %pxd, edma failed to enqueue to PPE tun dev %p", skb, rules->tun_dev);
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_PACKET_DROP]);
		return;
	}

	nss_udp_st_update_stats(pkt_sz + sizeof(struct ethhdr), 1);
}
#endif

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
/*
 * nss_udp_st_tx_packets_ppe_vp()
 *	Create the skb for hw offload
 */
static void nss_udp_st_tx_packets_ppe_vp(struct net_device *ndev, struct nss_udp_st_rules *rules, struct sk_buff **out_skb)
{
	struct sk_buff *skb;
	struct udphdr *uh;
	struct iphdr *iph;
	struct ipv6hdr *ipv6h;
	size_t align_offset;
	size_t skb_sz;
	size_t pkt_sz;
	uint16_t ip_len;
	uint16_t udp_len;
	unsigned char *data;

	pkt_sz = nust.config.buffer_sz;
	ip_len = pkt_sz;

	if (rules->ip_version & NSS_UDP_ST_FLAG_IPV4) {
		udp_len = pkt_sz - sizeof(*iph);
	} else if (rules->ip_version & NSS_UDP_ST_FLAG_IPV6) {
		udp_len = pkt_sz - sizeof(*ipv6h);
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		return;
	}

	/*
	 * Allocate SKB with room for full L2+L3+L4 packet.
	 * The Ethernet header is added below so include ETH_HLEN in skb_sz.
	 */
	skb_sz = NSS_UDP_ST_MIN_HEADROOM + pkt_sz + sizeof(struct ethhdr) +
		 NSS_UDP_ST_MIN_TAILROOM + SMP_CACHE_BYTES;

	skb = dev_alloc_skb(skb_sz);
	if (!skb) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_MEMORY_FAILURE]);
		return;
	}

	align_offset = PTR_ALIGN(skb->data, NSS_UDP_ST_HW_OFFLOAD_PTR_ALIGN) - skb->data;

	if (rules->ip_version & NSS_UDP_ST_FLAG_IPV4) {
		if (rules->bridge_vlan_id) {
			skb_reserve(skb, sizeof(*uh) + sizeof(*iph) + ETH_HLEN + VLAN_HLEN + align_offset);
		} else {
			skb_reserve(skb, sizeof(*uh) + sizeof(*iph) + ETH_HLEN + align_offset);
		}
	} else {
		if (rules->bridge_vlan_id) {
			skb_reserve(skb, sizeof(*uh) + sizeof(*ipv6h) + ETH_HLEN + VLAN_HLEN + align_offset);
		} else {
			skb_reserve(skb, sizeof(*uh) + sizeof(*ipv6h) + ETH_HLEN + align_offset);
		}
	}

	data = skb_put(skb, udp_len - sizeof(*uh));
	memset(data, 0, udp_len - sizeof(*uh));

	skb_push(skb, sizeof(*uh));
	skb_reset_transport_header(skb);
	uh = udp_hdr(skb);
	nss_udp_st_generate_udp_hdr(uh, udp_len, rules);

	if (rules->ip_version & NSS_UDP_ST_FLAG_IPV4) {
		skb_push(skb, sizeof(*iph));
		skb_reset_network_header(skb);
		iph = ip_hdr(skb);
		nss_udp_st_generate_ipv4_hdr(iph, ip_len, rules);
		skb->protocol = htons(ETH_P_IP);
	} else {
		skb_push(skb, sizeof(*ipv6h));
		skb_reset_network_header(skb);
		ipv6h = ipv6_hdr(skb);
		nss_udp_st_generate_ipv6_hdr(ipv6h, ip_len, rules);
		skb->protocol = htons(ETH_P_IPV6);
	}

	/*
	 * Push VLAN header if bridge port is a VLAN device for SFU.
	 */
	if (rules->bridge_vlan_id) {
		struct vlan_hdr *vhdr;
		skb_push(skb, VLAN_HLEN);
		vhdr = (struct vlan_hdr *)skb->data;
		vhdr->h_vlan_TCI = htons(rules->bridge_vlan_id);
		vhdr->h_vlan_encapsulated_proto = skb->protocol;
		skb->protocol = htons(ETH_P_8021Q);
		pr_debug("UDP-ST: Added VLAN header with VID %u to SKB\n", rules->bridge_vlan_id);

		/*
		 * For SFU, the VP netdevice MAC is a dummy value, so use the mac
		 * address resolved for the source IP as the eth dest instead.
		 */
		pr_info("UDP-ST VP: SFU eth hdr src_mac=%pM dst_mac=%pM\n", rules->src_mac, rules->dst_mac);
		nss_udp_st_generate_eth_hdr(skb, (const uint8_t *)rules->src_mac, (uint8_t *)rules->dst_mac);
	} else {
		pr_info("UDP-ST VP: eth hdr src_mac=%pM dst_mac=%pM\n", rules->dst_mac, nss_udp_st_ppe_vp_dev_get()->dev_addr);
		nss_udp_st_generate_eth_hdr(skb, (const uint8_t *)rules->dst_mac, (uint8_t *)nss_udp_st_ppe_vp_dev_get()->dev_addr);
	}

	/*
	 * Set skb->dev to the VP netdevice
	 */
	skb->dev = nss_udp_st_ppe_vp_dev_get();

	pr_info("UDP-ST VP: About to xmit skb=%px len=%u data=%px headlen=%u\n",
		skb, skb->len, skb->data, skb_headlen(skb));
	pr_info("UDP-ST VP: IP len=%u, pkt_sz=%zu\n",
		ip_len, pkt_sz);

	*out_skb = skb;

	nss_udp_st_update_stats(ip_len + sizeof(struct ethhdr), 1);
}
#endif /* NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE */

/*
 * nss_udp_st_tx_packets()
 *	allocate, populate and send tx packet
 */
static void nss_udp_st_tx_packets(struct net_device *ndev, struct nss_udp_st_rules *rules, int cpu)
{
	struct sk_buff *skb;
	struct udphdr *uh;
	struct iphdr *iph;
	struct ipv6hdr *ipv6h;
	size_t align_offset;
	size_t skb_sz;
	size_t pkt_sz;
	uint16_t ip_len;
	uint16_t udp_len;
	unsigned char *data;
	uint16_t ppp_protocol;
	uint8_t payload_sz = 0;

	pkt_sz = nust.config.buffer_sz;
	ip_len = pkt_sz;

	if (rules->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		udp_len = pkt_sz - sizeof(*iph);
	} else if (rules->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		udp_len = pkt_sz - sizeof(*ipv6h);
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		return;
	}

	skb_sz = NSS_UDP_ST_MIN_HEADROOM + pkt_sz + sizeof(struct ethhdr) + NSS_UDP_ST_MIN_TAILROOM + SMP_CACHE_BYTES;

	skb = dev_alloc_skb(skb_sz);
	if (!skb) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_MEMORY_FAILURE]);
		return;
	}

	align_offset = PTR_ALIGN(skb->data, SMP_CACHE_BYTES) - skb->data;
	skb_reserve(skb, NSS_UDP_ST_MAX_HEADROOM + align_offset + sizeof(uint16_t));

	/*
	 * Add complete payload first.
	 */
	if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
		nss_udp_st_add_seq_tstamp(skb, rules);
		payload_sz = sizeof(struct nss_udp_st_timestamp_info);
	}

	if (rules->ip_version & NSS_UDP_ST_FLAG_IPV4) {
		data = skb_put(skb, pkt_sz - sizeof(*iph) - sizeof(*uh) - payload_sz);
		memset(data, 0, pkt_sz - sizeof(*iph) - sizeof(*uh) - payload_sz);
	} else if (rules->ip_version & NSS_UDP_ST_FLAG_IPV6) {
		data = skb_put(skb, pkt_sz - sizeof(*ipv6h) - sizeof(*uh) - payload_sz);
		memset(data, 0, pkt_sz - sizeof(*ipv6h) - sizeof(*uh) - payload_sz);
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		kfree_skb(skb);
		return;
	}

	/*
	 * populate udp header (after payload is complete)
	 */
	skb_push(skb, sizeof(*uh));
	skb_reset_transport_header(skb);
	uh = udp_hdr(skb);
	nss_udp_st_generate_udp_hdr(uh, udp_len, rules);

	/*
	 * populate ipv4 or ipv6 header
	 */
	if (rules->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		skb_push(skb, sizeof(*iph));
		skb_reset_network_header(skb);
		iph = ip_hdr(skb);
		nss_udp_st_generate_ipv4_hdr(iph, ip_len, rules);
	} else if (rules->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		skb_push(skb, sizeof(*ipv6h));
		skb_reset_network_header(skb);
		ipv6h = ipv6_hdr(skb);
		nss_udp_st_generate_ipv6_hdr(ipv6h, ip_len, rules);
	}

	switch (ndev->type) {
	case ARPHRD_PPP:
		if (rules->ip_version == NSS_UDP_ST_FLAG_IPV4) {
			ppp_protocol = PPP_IP;
		} else {
			ppp_protocol = PPP_IPV6;
		}

		nss_udp_st_generate_pppoe_hdr(skb, ppp_protocol);
		pkt_sz += NSS_UDP_ST_PPPOE_OVERHEAD;

		if(is_vlan_dev(info.dev)) {
			nss_udp_st_generate_vlan_hdr(skb, info.dev);
			pkt_sz += NSS_UDP_ST_VLAN_OVERHEAD;
		}

		/*
		 * populate ethernet header
		 */
		nss_udp_st_generate_eth_hdr(skb, xmit_dev->dev_addr, info.pa.remote);
		break;

	case ARPHRD_ETHER:
		if (rules->ip_version ==  NSS_UDP_ST_FLAG_IPV4) {
			skb->protocol = htons(ETH_P_IP);
		} else {
			skb->protocol = htons(ETH_P_IPV6);
		}

		if(is_vlan_dev(ndev)) {
			nss_udp_st_generate_vlan_hdr(skb, ndev);
			pkt_sz += NSS_UDP_ST_VLAN_OVERHEAD;
		}

		/*
		 * populate ethernet header
		 */
		nss_udp_st_generate_eth_hdr(skb, xmit_dev->dev_addr, rules->dst_mac);
		break;

	default:
		break;
	}

	/*
	 * tx packet
	 */
	skb->dev = xmit_dev;
	skb_set_queue_mapping(skb, cpu);
	if (xmit_dev->netdev_ops->ndo_start_xmit(skb, xmit_dev) != NETDEV_TX_OK) {
		kfree_skb(skb);
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_PACKET_DROP]);
		return;
	}

	nss_udp_st_update_stats(pkt_sz + sizeof(struct ethhdr), 1);
}

/*
 * nss_udp_st_set_dev()
 *	get net_device
 */
static bool nss_udp_st_set_dev(void)
{
	nust_dev = dev_get_by_name(&init_net, nust.config.net_dev);
	if (!nust_dev) {
		udp_st_err("Cannot find the net device\n");
		return false;
	}

	return true;
}

/*
 * nss_udp_st_tx_valid()
 *	check if test time has elapsed
 */
bool nss_udp_st_tx_valid(void)
{
	long long elapsed = atomic64_read(&nust.stats.timer_stats[NSS_UDP_ST_STATS_TIME_ELAPSED]);

	if (elapsed < (nust.time * 1000)) {
		return true;
	}

	return false;
}

/*
 * nss_udp_st_vlan_iface_config
 *      Configure the WLAN interface as VLAN
 */
static int nss_udp_st_vlan_iface_config(struct net_device *dev)
{
	xmit_dev = vlan_dev_next_dev(dev);
	if (!xmit_dev) {
		udp_st_err("Cannot find the physical net device\n");
		return -1;
	}

	/* Allowing double tagged VLAN for hw offload */
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
	if (!(nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) &&
			(is_vlan_dev(xmit_dev) || xmit_dev->type != ARPHRD_ETHER)) {
#else
	if (is_vlan_dev(xmit_dev) || xmit_dev->type != ARPHRD_ETHER) {
#endif
		udp_st_warn("%px: QinQ or non-ethernet VLAN master (%s) is not supported\n", dev,
				xmit_dev->name);
		return -1;
	}

	vh.h_vlan_TCI = vlan_dev_vlan_id(dev);
	vh.h_vlan_encapsulated_proto = ntohs(vlan_dev_vlan_proto(dev));

	return 0;
}


/*
 * nss_udp_st_pppoe_iface_config
 *	Configure the WLAN interface as PPPoE
 */
int nss_udp_st_pppoe_iface_config(struct net_device *dev)
{
	struct ppp_channel *ppp_chan[1];
	int channel_count;
	int channel_protocol;
	int ret = 0;

	/*
	 * Gets the PPPoE channel information.
	 */
	channel_count = ppp_hold_channels(dev, ppp_chan, 1);
	if (channel_count != 1) {
		udp_st_warn("%px: Unable to get the channel for device: %s\n", dev, dev->name);
		return -1;
	}

	channel_protocol = ppp_channel_get_protocol(ppp_chan[0]);
	if (channel_protocol != PX_PROTO_OE) {
		udp_st_warn("%px: PPP channel protocol is not PPPoE for device: %s\n", dev, dev->name);
		ppp_release_channels(ppp_chan, 1);
		return -1;
	}

	if (pppoe_channel_addressing_get(ppp_chan[0], &info)) {
		udp_st_warn("%px: Unable to get the PPPoE session information for device: %s\n", dev, dev->name);
		ppp_release_channels(ppp_chan, 1);
		return -1;
	}

	/*
	 * Check if the next device is a VLAN (eth0-eth0.100-pppoe-wan)
	 */
	if (is_vlan_dev(info.dev)) {
		/*
		 * Next device is a VLAN device (eth0.100)
		 */
		if (nss_udp_st_vlan_iface_config(info.dev) < 0) {
			udp_st_warn("%px: Unable to get PPPoE's VLAN device's (%s) next dev\n", dev,
 info.dev->name);
			ret = -1;
			goto fail;
		}
	} else {
		/*
		 * PPPoE interface can be created on linux bridge, OVS bridge and LAG devices.
		 * udp_st doesn't support these hierarchies.
		 */
		if ((info.dev->priv_flags & (IFF_EBRIDGE | IFF_OPENVSWITCH))
			|| ((info.dev->flags & IFF_MASTER) && (info.dev->priv_flags & IFF_BONDING))) {
			udp_st_warn("%px: PPPoE over bridge and LAG interfaces are not supported, dev: %s info.dev: %s\n",dev, dev->name, info.dev->name);
			ret = -1;
			goto fail;

		}

		/*
		 * PPPoE only (eth0-pppoe-wan)
		 */
		xmit_dev = info.dev;
	}

	nust.pppoe_info.dev = info.dev;
	nust.pppoe_info.pppoe_session_id = info.pa.sid;
	memcpy(nust.pppoe_info.remote_mac, info.pa.remote, ETH_ALEN);

fail:
	dev_put(info.dev);
	ppp_release_channels(ppp_chan, 1);
	return ret;
}

#ifdef NSS_UDP_ST_DRV_VP_ENABLE

/*
 * nss_udp_st_dummy_netdev_setup()
 *	Netdev setup function.
 */
static void nss_udp_st_dummy_netdev_setup(struct net_device *dev)
{
	dev->addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
	dev->needed_headroom = NSS_UDP_ST_MIN_HEADROOM;
	dev->needed_tailroom = NSS_UDP_ST_MIN_TAILROOM;
	dev->type = ARPHRD_VOID;
	dev->ethtool_ops = NULL;
	dev->header_ops = NULL;
	dev->netdev_ops = NULL;
	dev->priv_destructor = NULL;

	memcpy((void*)dev->dev_addr, "\x00\x00\x00\x00\x00\x00", dev->addr_len);
	memset(dev->broadcast, 0xff, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
}

/*
 * nss_udp_st_tun_setup
 * 	initialize encap entry for tx vp
 */
static bool nss_udp_st_tun_setup(struct nss_udp_st_rules *rule)
{

	ppe_drv_iface_t iface_idx;
	struct ppe_drv_iface *iface = NULL;
	struct udphdr uh;
	struct iphdr iph;
	struct ipv6hdr ipv6h;
	size_t pkt_sz;
	uint16_t udp_len;
	uint16_t ip_len;
	uint32_t port_num;

	struct ppe_drv_tun_cmn_ctx tun_hdr = {0};
	struct ppe_drv_tun_cmn_ctx_l2 *l2 = &tun_hdr.l2;
	struct ppe_drv_tun_cmn_ctx_l3 *l3 = &tun_hdr.l3;
	struct ppe_drv_tun_cmn_ctx_cust_udp_st *l4 = &tun_hdr.tun.cust.cust_tun.udp_st;

	/*
	 * Get the PPE port associated with the egress interface.
	 */
	iface_idx = ppe_drv_iface_idx_get_by_dev(nust_dev);
	if (iface_idx == -1) {
		udp_st_err("Failed to get iface index\n");
		return false;
	}

	iface = ppe_drv_iface_get_by_idx(iface_idx);
	if (!iface) {
		udp_st_err("Failed to get iface using index %d\n", iface_idx);
		return false;
	}

	port_num = ppe_drv_iface_port_idx_get(iface);
	if (port_num == -1) {
		udp_st_err("Failed to get port using iface: %d\n", iface_idx);
		return false;
	}

	if (rule->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		udp_len = pkt_sz - sizeof(struct iphdr);
		l2->eth_type = ETH_P_IP;
	} else if (rule->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		udp_len = pkt_sz - sizeof(struct ipv6hdr);
		l2->eth_type = ETH_P_IPV6;
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		return false;
	}

	/*
	 * Set the tunnel type to custom and custom type to udpst.
	 */
	tun_hdr.type = PPE_DRV_TUN_CMN_CTX_TYPE_CUST;
	tun_hdr.tun.cust.cust_type = PPE_DRV_TUN_CMN_CTX_CUST_TYPE_UDP_ST;

	/*
	 * populate L2 header
	 */
	memcpy(&l2->smac, nust_dev->dev_addr, ETH_ALEN);
	memcpy(&l2->dmac, rule->dst_mac, ETH_ALEN);
	l2->xmit_port = port_num;

	if (is_vlan_dev(nust_dev)) {
		l2->vlan[0].tpid = ntohs(vlan_dev_vlan_proto(nust_dev));
		l2->vlan[0].tci = vlan_dev_vlan_id(nust_dev);
		l2->flags |= PPE_DRV_TUN_CMN_CTX_L2_CVLAN_VALID;
	}

	/*
	 * populate ipv4 or ipv6  header
	 */
	if (rule->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		nss_udp_st_generate_ipv4_hdr(&iph, ip_len, rule);
		l3->saddr[0] = iph.saddr;
		l3->daddr[0] = iph.daddr;
		l3->ttl = iph.ttl;
		l3->dscp = iph.tos >> 2;
		l3->proto = iph.protocol;
		l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV4;
	} else if (rule->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		nss_udp_st_generate_ipv6_hdr(&ipv6h, ip_len, rule);
		l3->saddr[0] = ipv6h.saddr.s6_addr32[0];
		l3->saddr[1] = ipv6h.saddr.s6_addr32[1];
		l3->saddr[2] = ipv6h.saddr.s6_addr32[2];
		l3->saddr[3] = ipv6h.saddr.s6_addr32[3];

		l3->daddr[0] = ipv6h.daddr.s6_addr32[0];
		l3->daddr[1] = ipv6h.daddr.s6_addr32[1];
		l3->daddr[2] = ipv6h.daddr.s6_addr32[2];
		l3->daddr[3] = ipv6h.daddr.s6_addr32[3];

		l3->ttl = ipv6h.hop_limit;
		l3->proto = ipv6h.nexthdr;
		l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV6;
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		return false;
	}

	/*
	 * populate udp header
	 */
	nss_udp_st_generate_udp_hdr(&uh, udp_len, rule);
	l4->sport = uh.source;
	l4->dport = uh.dest;

	/*
	 * Allocate internal tunnel net device
	 */
	rule->tun_dev = alloc_netdev(0,"udpst_tun%d",
				NET_NAME_ENUM, nss_udp_st_dummy_netdev_setup);
	if (!rule->tun_dev) {
		udp_st_err("Error allocating internal tunnel dev\n");
		return false;
	}

	if (!ppe_tun_setup(rule->tun_dev, &tun_hdr)) {
		udp_st_err("failed to configure tunnel\n");
		free_netdev(rule->tun_dev);
		return false;
	}

	/*
	 * Get and maintain vp_num as we cannot get this from an hr timer context
	 */
	rule->vp_num = ppe_drv_port_num_from_dev(rule->tun_dev);
	if (unlikely((rule->vp_num < PPE_DRV_VIRTUAL_START) || (rule->vp_num >= PPE_DRV_VIRTUAL_END))) {
		udp_st_err("Not a valid Virtual Port number %d dev %s", rule->vp_num, rule->tun_dev->name);
		nss_udp_st_tun_destroy(rule->tun_dev);
		return false;
	}

	return true;
}

/*
 * nss_udp_st_tun_setup_for_all_rules
 * 	Setup PPE tunnels for all the rules
 */
static uint8_t nss_udp_st_tun_setup_for_all_rules(void)
{
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;

	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		if (pos->tun_dev == NULL) {
			if (!nss_udp_st_tun_setup(pos)) {
				return 0;
			}
		}
	}

	return 1;
}
#endif

/*
 * nss_udp_st_tx_work_send_packets()
 *	generate and send packets per rule
 *	must hold ref of the cpu before calling
 */
static void nss_udp_st_tx_work_send_packets(int cpu)
{
	int i = 0;
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;

	if (!nss_udp_st_tx_valid()  || nust.mode == NSS_UDP_ST_STOP ) {
		dev_put(nust_dev);
		tx_hr_restart[cpu] = HRTIMER_NORESTART;
		put_cpu();
		return;
	}

	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		if (pos->cpu != cpu) {
			continue;
		}

		for (i = 0; i < nss_udp_st_tx_num_pkt; i++) {
			/*
			 * check if test time has elapsed or test has been stopped
			 */
			if (!nss_udp_st_tx_valid() || nust.mode == NSS_UDP_ST_STOP ) {
				dev_put(nust_dev);
				tx_hr_restart[cpu] = HRTIMER_NORESTART;
				put_cpu();
				return;
			}

#ifdef NSS_UDP_ST_DRV_VP_ENABLE
			if (nust.config.flags & NSS_UDP_ST_FLAGS_VP) {
				nss_udp_st_tx_packets_vp(nust_dev, pos, cpu);
			} else
#endif
			{
				nss_udp_st_tx_packets(nust_dev, pos, cpu);
			}
		}
	}
	put_cpu();
	tx_hr_restart[cpu] = HRTIMER_RESTART;
}

/*
 * nss_udp_st_tx_rate_config
 *	API to configure number of packets to be sent out per timer interrupt.
 *
 * If burst_size is configured, update the timer interval, else update the burst_size keeping
 * timer interval set to 100HZ (10ms).
 */
static void nss_udp_st_tx_rate_config(uint64_t total_bps)
{
	uint64_t bits_per_pkt = (nust.config.buffer_sz + l2_overhead) * 8;

	/*
	 * If burst_size is configured, calculate the timer freq else
	 * calculate number of pkts to send per rule per 10 ms
	 */
	if (!nust.config.burst_size) {
		nust.config.timer_freq = NSS_UDP_ST_TX_DEFAULT_TIMER_FREQ;
		nss_udp_st_tx_num_pkt = div_u64(total_bps , (nust.rule_count * bits_per_pkt * nust.config.timer_freq));
		nss_udp_st_tx_num_pkt++;
	} else {
		nust.config.timer_freq = div_u64(total_bps , (nust.rule_count * bits_per_pkt * nust.config.burst_size));
		if (nust.config.timer_freq == 0 || nust.config.timer_freq > NSS_UDP_ST_TX_MAX_TIMER_FREQ) {
			pr_warn("Un-supported timer freq %u, using default freq of 100 Hz\n", nust.config.timer_freq);
			nust.config.timer_freq = NSS_UDP_ST_TX_DEFAULT_TIMER_FREQ;
			nss_udp_st_tx_num_pkt = div_u64(total_bps , (nust.rule_count * bits_per_pkt * nust.config.timer_freq));
			nss_udp_st_tx_num_pkt++;
		} else {
			nss_udp_st_tx_num_pkt = nust.config.burst_size + 1;
		}
	}

	/*
	 * Cap the number of packets generated per rule per timer tick to avoid
	 * bursts large enough to overrun the TX ring / OOM the system.
	 */
	if (nss_udp_st_tx_num_pkt > nss_udp_st_tx_max_num_pkt) {
		pr_warn("Requested pkt/tick %llu exceeds max %u, capping\n",
			nss_udp_st_tx_num_pkt, nss_udp_st_tx_max_num_pkt);
		nss_udp_st_tx_num_pkt = nss_udp_st_tx_max_num_pkt;
	}

	/*
	 * Set the timer freq based on the config.
	 * This will be used by hrtimer_start and hrtimer_forward APIs.
	 */
	kt = ns_to_ktime(DIV_ROUND_CLOSEST_ULL(NSEC_PER_SEC, nust.config.timer_freq));
}

/*
 * nss_udp_st_tx_init()
 *	initialize speedtest for tx
 */
static bool nss_udp_st_tx_init(void)
{
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;

	if (!nss_udp_st_set_dev()) {
		udp_st_err("Failed to set dev\n");
		return false;
	}

	if (nust.config.buffer_sz < NSS_UDP_ST_BUFFER_SIZE_MIN) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
		udp_st_err("Invalid buffer size given: %u. Min buffer size required: %u", nust.config.buffer_sz, NSS_UDP_ST_BUFFER_SIZE_MIN);
		dev_put(nust_dev);
		return false;
	}

	if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
		uint32_t dev_mtu = nust_dev->mtu;

		if (nust.config.buffer_sz > dev_mtu) {
			atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
			udp_st_err("Invalid buffer size given: %u. Max buffer size allowed: %u", nust.config.buffer_sz, dev_mtu);
			dev_put(nust_dev);
			return false;
		}

	} else {
		if (nust.config.buffer_sz > NSS_UDP_ST_BUFFER_SIZE_MAX) {
			atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
			udp_st_err("Invalid buffer size given: %u. Max buffer size allowed: %u", nust.config.buffer_sz, NSS_UDP_ST_BUFFER_SIZE_MAX);
			dev_put(nust_dev);
			return false;
		}

		if (nust_dev->type == ARPHRD_PPP && nust.config.buffer_sz > (NSS_UDP_ST_BUFFER_SIZE_MAX - NSS_UDP_ST_PPPOE_OVERHEAD)) {
			atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
			udp_st_err("Invalid buffer size given: %u. Max buffer size allowed for pppoe: %u", nust.config.buffer_sz, (NSS_UDP_ST_BUFFER_SIZE_MAX - NSS_UDP_ST_PPPOE_OVERHEAD));
			dev_put(nust_dev);
			return false;
		}
	}

	/*
	 * Check all ports are specified
	 */
	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		if (pos->sport == 0 || pos->dport == 0) {
			udp_st_err("Unable to tx with arbitrary ports: sport = %u, dport = %u", pos->sport, pos->dport);
			dev_put(nust_dev);
			return false;
		}
	}

	return true;
}

/*
 * nss_udp_st_tx_get_link_speed
 *	API to get link speed on physical interface on which speed test will run.
 *
 * This API has to be called with physical dev only.
 */
static uint32_t nss_udp_st_tx_get_link_speed(struct net_device *dev)
{
	struct ethtool_link_ksettings ecmd;
	uint32_t speed_mbps;
	int ret;

	if (!dev) {
		pr_err("Null physical dev\n");
		return 0;
	}

	if (!netif_carrier_ok(dev)) {
		pr_warn("Net device down %s\n", dev->name);
		return 0;
	}

	rtnl_lock();
	ret = __ethtool_get_link_ksettings(dev, &ecmd);
	rtnl_unlock();

	if (ret) {
		pr_warn("Failed to read ethtool stats. Ret =  %d, dev %s\n", ret, dev->name);
		return 0;
	}

	speed_mbps = ecmd.base.speed;
	if (speed_mbps == SPEED_UNKNOWN) {
		pr_warn("Link up, speed unknown %s\n", dev->name);
		return 0;
	}

	pr_info("nss_udp_st_tx_get_link_speed: using physical device %s\n", dev->name);
	return speed_mbps;
}

/*
 * nss_udp_st_validate_rate
 *	Reject a configured rate that exceeds the max bandwidth (link speed)
 *	of the given physical interface.
 */
/*
 * nss_udp_st_get_xmit_dev()
 *	Return the currently resolved TX physical device
 */
struct net_device *nss_udp_st_get_xmit_dev(void)
{
	return xmit_dev;
}

bool nss_udp_st_validate_rate(struct net_device *dev, uint32_t rate)
{
	uint32_t link_speed;

	if (!rate) {
		return true;
	}

	if (!dev) {
		pr_warn("Cannot validate rate: dev is NULL\n");
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_RATE]);
		return false;
	}

	link_speed = nss_udp_st_tx_get_link_speed(dev);
	if (!link_speed) {
		pr_warn("Cannot determine link speed of dev %s\n", dev->name);
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_RATE]);
		return false;
	}

	pr_info("Current link speed of interface %s: %u Mbps\n", dev->name, link_speed);

	if (rate > link_speed) {
		pr_warn("Requested rate %u Mbps exceeds max bandwidth %u Mbps of interface %s, rejecting\n",
			rate, link_speed, dev->name);
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_RATE]);
		return false;
	}

	return true;
}

/*
 * nss_udp_st_tx_rate_change
 *	Dynamically change the rate of speedtest
 */
bool nss_udp_st_tx_rate_change(uint32_t rate)
{
	uint64_t total_bps;
	uint32_t link_speed;

	if (!xmit_dev) {
		pr_warn("xmit dev NULL\n");
		return false;
	}

	link_speed = nss_udp_st_tx_get_link_speed(xmit_dev);
	if (!link_speed) {
		pr_warn("Failed to get link speed of dev %s\n", xmit_dev->name);
		return false;
	}

	if (link_speed < rate) {
		pr_warn("Link speed %u is lower than the configured rate %u, Limiting tx rate to link speed\n", link_speed, rate);
		rate = link_speed;
	}

	/*
	 * Convert Mbps to bps
	 */
	total_bps = (uint64_t)rate * 1000000;

	nss_udp_st_tx_rate_config(total_bps);
	return true;
}

/*
 * nss_udp_st_tun_destroy
 *	Destroy PPE tunnel associated with the connection.
 */
#ifdef NSS_UDP_ST_DRV_VP_ENABLE
void nss_udp_st_tun_destroy(struct net_device *dev) {
	if (nust.dir == NSS_UDP_ST_TX) {
		ppe_tun_destroy(dev);
		free_netdev(dev);
	}
}
#endif

/*
 * nss_udp_st_hrtimer_cleanup()
 *	cancel hrtimer
 */
void nss_udp_st_hrtimer_cleanup(void)
{
	int cpu = get_cpu();
	hrtimer_cancel(&tx_hr_timer[cpu]);
	tx_hr_restart[cpu] = HRTIMER_NORESTART;
	put_cpu();
}

/*
 * nss_udp_st_hrtimer_callback()
 *	hrtimer callback function
 */
static enum hrtimer_restart nss_udp_st_hrtimer_callback(struct hrtimer *timer)
{
	int cpu = get_cpu();

	nss_udp_st_tx_work_send_packets(cpu);
	if(tx_hr_restart[cpu] == HRTIMER_RESTART) {
		hrtimer_forward_now(timer, kt);
	}
	return tx_hr_restart[cpu];
}

/*
 * nss_udp_st_hrtimer_init()
 *	initialize hrtimer
 */
void nss_udp_st_hrtimer_init(struct hrtimer *hrt, int cpu)
{
	tx_hr_restart[cpu] = HRTIMER_RESTART;
	hrtimer_init(hrt, CLOCK_REALTIME, HRTIMER_MODE_ABS_HARD);
	hrt->function = &nss_udp_st_hrtimer_callback;
}

/*
 * nss_udp_st_tx_wq_cb
 *	Callback function used by the workqueue to start the hr timer
 */
static void nss_udp_st_tx_wq_cb(struct work_struct *usw)
{
	int cpu = get_cpu();
	dev_hold(xmit_dev);
	if (!tx_timer_flag[cpu]) {
		nss_udp_st_hrtimer_init(&tx_hr_timer[cpu], cpu);
		hrtimer_start(&tx_hr_timer[cpu], kt, HRTIMER_MODE_ABS_PINNED_HARD);
		tx_timer_flag[cpu] = 1;
	} else {
		hrtimer_restart(&tx_hr_timer[cpu]);
	}
	put_cpu();
}

#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
/*
 * nss_udp_st_tx_hw_offload_send_packets()
 *	Send single packet per rule for hw_offload path
 */
static bool nss_udp_st_tx_hw_offload_send_packets(void)
{
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;
	struct net_device *real_dev __maybe_unused;
	int total_count = 0;
	int failed = 0;
	bool is_veip __maybe_unused;
	bool is_gem_port = false;;

	/*
	 * Count total rules for EDMA ring distribution
	 */
	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		total_count++;
	}

	if (total_count == 0) {
		pr_err("UDP-ST: No rules configured for hw_offload\n");
		return false;
	}

	pr_info("UDP-ST: Injecting %d packets for hw_offload (one per rule)\n", total_count);

#ifdef NSS_UDP_ST_HGU
	is_veip = ppe_drv_veip_is_enabled(nust_dev);
#endif
#ifdef NSS_UDP_ST_PON
	if (is_vlan_dev(nust_dev)) {
		real_dev = vlan_dev_next_dev(nust_dev);
		is_gem_port = nss_udp_st_ppe_is_gem_port(real_dev);
	}
#endif

	/*
	 * Inject one packet per rule
	 */
	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		struct sk_buff *skb = NULL;
		int slot;

		slot = atomic_fetch_add(1, &nust.xmit_idx);

		nss_udp_st_tx_packets_ppe_vp(nust_dev, pos, &skb);
		if (skb) {
			struct nss_dp_udp_st_xmit_info xmit_info = {
				.skb = skb,
				.skb_idx = slot,
				.skb_count = total_count,
				.vp_num = pos->vp_num,
				.is_veip = is_veip,
				.is_gem_port = is_gem_port,
			};
			if (nss_dp_udp_st_xmit(&xmit_info) != 0) {
				pr_err("UDP-ST: Failed to xmit packet for rule (slot=%d)\n", slot);
				kfree_skb(skb);
				atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_PACKET_DROP]);
				failed++;
			} else {
				if (pos->ip_version & NSS_UDP_ST_FLAG_IPV4) {
					pr_info("UDP-ST: Injected packet for rule: %pI4:%u -> %pI4:%u (slot=%d)\n",
						&pos->sip.ip.ipv4, pos->sport,
						&pos->dip.ip.ipv4, pos->dport, slot);
				} else {
					pr_info("UDP-ST: Injected IPv6 packet for rule (slot=%d)\n", slot);
				}
			}
		} else {
			pr_err("UDP-ST: Failed to create packet for rule\n");
			failed++;
		}
	}

	if (failed > 0) {
		pr_warn("UDP-ST: %d/%d packet injections failed\n", failed, total_count);
		return false;
	}

	pr_info("UDP-ST: Successfully injected %d packets for hw_offload\n", total_count);
	pr_info("UDP-ST: PPE will now loop these packets at configured rate\n");
	return true;
}

/*
 * nss_udp_st_tx_hw_offload()
 *	Start hw_offload TX test
 */
bool nss_udp_st_tx_hw_offload(void)
{
	if (!nss_udp_st_tx_init()) {
		pr_err("Failed to init hw_offload tx\n");
		return false;
	}

	ppe_vp_mtu_set(nss_udp_st_ppe_vp_num_get(), nust_dev->mtu);
	/*
	 * Initialize L2 overhead
	 */
	l2_overhead = sizeof(struct ethhdr);

	/*
	 * For hw_offload, handle VLAN and PPPoE interfaces
	 * The PPE flow creation handles the encapsulation, so we just need the base device
	 */
	switch (nust_dev->type) {
	case ARPHRD_PPP:
		/*
		 * PPPoE interface - extract the base device
		 */
		if (nss_udp_st_pppoe_iface_config(nust_dev) < 0) {
			pr_err("UDP-ST: Could not configure PPPoE for hw_offload, dev: %s\n", nust_dev->name);
			dev_put(nust_dev);
			return false;
		}
		l2_overhead += 8; /* PPPoE header + PPP protocol */
		if (is_vlan_dev(info.dev)) {
			l2_overhead += VLAN_HLEN;
		}
		pr_info("UDP-ST: hw_offload with PPPoE interface: %s (base: %s)\n",
			nust_dev->name, xmit_dev->name);
		break;

	case ARPHRD_ETHER:
		if ((nust_dev->priv_flags & (IFF_EBRIDGE | IFF_OPENVSWITCH))
			|| ((nust_dev->flags & IFF_MASTER) && (nust_dev->priv_flags & IFF_BONDING))) {
			pr_err("UDP-ST: Bridge and LAG interfaces not supported for hw_offload, dev: %s\n",
			       nust_dev->name);
			dev_put(nust_dev);
			return false;
		}

		if (is_vlan_dev(nust_dev)) {
			/*
			 * VLAN interface - extract the base device
			 */
			if (nss_udp_st_vlan_iface_config(nust_dev) < 0) {
				pr_err("UDP-ST: Could not configure VLAN for hw_offload, dev: %s\n", nust_dev->name);
				dev_put(nust_dev);
				return false;
			}
			l2_overhead += VLAN_HLEN;
			pr_info("UDP-ST: hw_offload with VLAN interface: %s (base: %s)\n",
				nust_dev->name, xmit_dev->name);
		} else {
			xmit_dev = nust_dev;
		}
		break;

	default:
		pr_err("UDP-ST: Unsupported interface type for hw_offload, dev: %s type: %d\n",
		       nust_dev->name, nust_dev->type);
		dev_put(nust_dev);
		return false;
	}

	pr_info("UDP-ST: hw_offload TX on interface: %s\n", nust_dev->name);
	pr_info("UDP-ST: Configured rate: %u Mbps (PPE will enforce this)\n", nust.config.rate);

	/*
	 * Send single packet per rule - PPE handles the rest
	 */
	if (!nss_udp_st_tx_hw_offload_send_packets()) {
		pr_err("UDP-ST: Failed to send packets for hw_offload\n");
		dev_put(nust_dev);
		return false;
	}

	pr_info("UDP-ST: hw_offload TX started successfully\n");
	return true;
}
#endif /* NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE */

/*
 * nss_udp_st_tx()
 *	start speedtest for tx
 */
bool nss_udp_st_tx(void)
{
	uint32_t i;
	char qname[NSS_UDP_ST_PROCESS_NAME_SZ];
	uint32_t link_speed;
	uint64_t total_bps;

	if (!nss_udp_st_tx_init()) {
		udp_st_err("Failed to init tx\n");
		return false;
	}

	/*
	 * Initialize L2 overhead with Ethernet header size
	 */
	l2_overhead = sizeof(struct ethhdr);

	switch (nust_dev->type) {
	case ARPHRD_PPP:
		if (nust.config.flags & NSS_UDP_ST_FLAGS_VP) {
			udp_st_err("PPPOE + VP speedtest is not supported\n");
			return false;
		}
		if(nss_udp_st_pppoe_iface_config(nust_dev) < 0) {
			udp_st_err("Could not configure pppoe, dev: %s\n", nust_dev->name);
			return false;
		}

		/*
		 * PPPoE: Add 6 bytes PPPoE header + 2 bytes PPP protocol
		 */
		l2_overhead += 8;

		/*
		 * Check if PPPoE is over VLAN
		 */
		if (is_vlan_dev(info.dev)) {
			l2_overhead += VLAN_HLEN;
		}
		break;

	case ARPHRD_ETHER:
		if ((nust_dev->priv_flags & (IFF_EBRIDGE | IFF_OPENVSWITCH))
			|| ((nust_dev->flags & IFF_MASTER) && (nust_dev->priv_flags & IFF_BONDING))) {
			udp_st_err("Bridge and LAG interfaces are not supported, dev: %s\n", nust_dev->name);
			return false;
		}

		if (is_vlan_dev(nust_dev)) {
			if (nss_udp_st_vlan_iface_config(nust_dev) < 0) {
				udp_st_err("Could not configure vlan, dev: %s\n", nust_dev->name);
				return false;
			}

			/*
			 * VLAN: Add 4 bytes VLAN header
			 */
			l2_overhead += VLAN_HLEN;
		} else {
			xmit_dev = nust_dev;
		}

		break;

	default:
		udp_st_err("Unsupported speedtest interface: %s\n", nust_dev->name);
		return false;
	}

	/*
	 * Check link speed on the physical transmit interface and bound rate accordingly.
	 */
	link_speed = nss_udp_st_tx_get_link_speed(xmit_dev);
	if (!link_speed) {
		pr_warn("Failed to get link speed\n");
		return false;
	}

	if (link_speed < nust.config.rate) {
		pr_warn("Link speed %u is lower than the configured rate %u, Limiting tx rate to link speed\n", link_speed, nust.config.rate);
		nust.config.rate = link_speed;
	}

	total_bps = (uint64_t)nust.config.rate * 1000000;
	nss_udp_st_tx_rate_config(total_bps);

	if (nust.config.flags & NSS_UDP_ST_FLAGS_VP) {
#ifdef NSS_UDP_ST_DRV_VP_ENABLE
		if (!nss_udp_st_tun_setup_for_all_rules()) {
			pr_err("Failed to Setup tunnels for all rules\n");
			return false;
		}
#else
		pr_err("No VP support on this SOC\n");
		return false;
#endif
	}

	udp_st_debug("Speedtest interface: %s\n", nust_dev->name);

	for (i = 0; i < NR_CPUS; i++) {
		if (!tx_timer_flag[i]) {
			snprintf(qname, NSS_UDP_ST_PROCESS_NAME_SZ, "udp_st%u", i + 1);
			udp_st_wq[i] = create_workqueue(qname);
			INIT_WORK(&udp_st_work[i], nss_udp_st_tx_wq_cb);
		}
		queue_work_on(i, udp_st_wq[i], &udp_st_work[i]);
	}

	return true;
}
