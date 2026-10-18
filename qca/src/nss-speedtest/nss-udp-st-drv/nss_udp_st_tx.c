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
#include <linux/string.h>
#include <linux/hrtimer.h>
#include <linux/math64.h>
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
#include "nss_udp_st_public.h"

int tx_timer_flag[NR_CPUS];
static ktime_t kt;
static struct hrtimer tx_hr_timer[NR_CPUS];
static enum hrtimer_restart tx_hr_restart[NR_CPUS] = {HRTIMER_NORESTART};
static struct vlan_hdr vh;
static struct net_device *xmit_dev;
static struct pppoe_opt info;

struct work_struct udp_st_work[NR_CPUS];	/* Work struct */
struct workqueue_struct *udp_st_wq[NR_CPUS];	/* workqueue struct */

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

	if (rules->flags & NSS_UDP_ST_FLAG_IPV4) {
		uh->check = csum_tcpudp_magic(rules->sip.ip.ipv4, rules->dip.ip.ipv4, udp_len, IPPROTO_UDP,
		csum_partial(uh, udp_len, 0));
	} else if (rules->flags & NSS_UDP_ST_FLAG_IPV6) {
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
	struct sk_buff *skb;
	size_t skb_sz;
	size_t pkt_sz;

	pkt_sz = nust.config.buffer_sz;
	skb_sz = NSS_UDP_ST_MIN_HEADROOM + pkt_sz + sizeof(struct ethhdr) + NSS_UDP_ST_MIN_TAILROOM + SMP_CACHE_BYTES;

	skb = dev_alloc_skb(skb_sz);
	if (!skb) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_MEMORY_FAILURE]);
		return;
	}

	if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
		nss_udp_st_add_seq_tstamp(skb, rules);
		skb_put(skb, pkt_sz - sizeof(struct nss_udp_st_timestamp_info));
	} else {
		skb_put(skb, pkt_sz);
	}

	/*
	 * tx packet
	 */
	skb->dev = rules->tun_dev;

	if (!ppe_vp_tx_to_vp(rules->vp_num, skb)) {
		pr_err("Dropping skb %pxd, edma failed to enqueue to PPE tun dev %p", skb, rules->tun_dev);
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_PACKET_DROP]);
		return;
	}

	nss_udp_st_update_stats(pkt_sz);
}
#endif

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

	if (rules->flags & NSS_UDP_ST_FLAG_IPV4) {
		udp_len = pkt_sz - sizeof(*iph);
	} else if (rules->flags & NSS_UDP_ST_FLAG_IPV6) {
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

	if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
		nss_udp_st_add_seq_tstamp(skb, rules);
		payload_sz = sizeof(struct nss_udp_st_timestamp_info);
	}

	/*
	 * populate udp header
	 */
	skb_push(skb, sizeof(*uh));
	skb_reset_transport_header(skb);
	uh = udp_hdr(skb);
	nss_udp_st_generate_udp_hdr(uh, udp_len, rules);

	/*
	 * populate ipv4 or ipv6  header
	 */
	if (rules->flags & NSS_UDP_ST_FLAG_IPV4) {
		skb_push(skb, sizeof(*iph));
		skb_reset_network_header(skb);
		iph = ip_hdr(skb);
		nss_udp_st_generate_ipv4_hdr(iph, ip_len, rules);
		data = skb_put(skb, pkt_sz - sizeof(*iph) - sizeof(*uh));
		memset(data + payload_sz, 0, pkt_sz - sizeof(*iph) - sizeof(*uh) - payload_sz);
	} else if (rules->flags & NSS_UDP_ST_FLAG_IPV6) {
		skb_push(skb, sizeof(*ipv6h));
		skb_reset_network_header(skb);
		ipv6h = ipv6_hdr(skb);
		nss_udp_st_generate_ipv6_hdr(ipv6h, ip_len, rules);
		data = skb_put(skb, pkt_sz - sizeof(*ipv6h) - sizeof(*uh));
		memset(data + payload_sz, 0, pkt_sz - sizeof(*ipv6h) - sizeof(*uh) - payload_sz);
	} else {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_IP_VERSION]);
		kfree_skb(skb);
		return;
	}

	switch (ndev->type) {
	case ARPHRD_PPP:
		if (rules->flags & NSS_UDP_ST_FLAG_IPV4) {
			ppp_protocol = PPP_IP;
		} else {
			ppp_protocol = PPP_IPV6;
		}

		nss_udp_st_generate_pppoe_hdr(skb, ppp_protocol);

		if(is_vlan_dev(info.dev)) {
			nss_udp_st_generate_vlan_hdr(skb, info.dev);
		}

		/*
		 * populate ethernet header
		 */
		nss_udp_st_generate_eth_hdr(skb, xmit_dev->dev_addr, info.pa.remote);
		break;

	case ARPHRD_ETHER:
		if (rules->flags & NSS_UDP_ST_FLAG_IPV4) {
			skb->protocol = htons(ETH_P_IP);
		} else {
			skb->protocol = htons(ETH_P_IPV6);
		}

		if(is_vlan_dev(ndev)) {
			nss_udp_st_generate_vlan_hdr(skb, ndev);
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

	nss_udp_st_update_stats(ip_len + sizeof(struct ethhdr));
}

/*
 * nss_udp_st_set_dev()
 *	get net_device
 */
static bool nss_udp_st_set_dev(void)
{
	nust_dev = dev_get_by_name(&init_net, nust.config.net_dev);
	if (!nust_dev) {
		pr_err("Cannot find the net device\n");
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
	nust.mode = NSS_UDP_ST_STOP;
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
		pr_err("Cannot find the physical net device\n");
		return -1;
	}

	if (is_vlan_dev(xmit_dev) || xmit_dev->type != ARPHRD_ETHER) {
		pr_warn("%px: QinQ or non-ethernet VLAN master (%s) is not supported\n", dev,
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
static int nss_udp_st_pppoe_iface_config(struct net_device *dev)
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
		pr_warn("%px: Unable to get the channel for device: %s\n", dev, dev->name);
		return -1;
	}

	channel_protocol = ppp_channel_get_protocol(ppp_chan[0]);
	if (channel_protocol != PX_PROTO_OE) {
		pr_warn("%px: PPP channel protocol is not PPPoE for device: %s\n", dev, dev->name);
		ppp_release_channels(ppp_chan, 1);
		return -1;
	}

	if (pppoe_channel_addressing_get(ppp_chan[0], &info)) {
		pr_warn("%px: Unable to get the PPPoE session information for device: %s\n", dev, dev->name);
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
			pr_warn("%px: Unable to get PPPoE's VLAN device's (%s) next dev\n", dev,
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
			pr_warn("%px: PPPoE over bridge and LAG interfaces are not supported, dev: %s info.dev: %s\n",dev, dev->name, info.dev->name);
			ret = -1;
			goto fail;

		}

		/*
		 * PPPoE only (eth0-pppoe-wan)
		 */
		xmit_dev = info.dev;
	}

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
		pr_err("Failed to get iface index\n");
		return false;
	}

	iface = ppe_drv_iface_get_by_idx(iface_idx);
	if (!iface) {
		pr_err("Failed to get iface using index %d\n", iface_idx);
		return false;
	}

	port_num = ppe_drv_iface_port_idx_get(iface);
	if (port_num == -1) {
		pr_err("Failed to get port using iface: %d\n", iface_idx);
		return false;
	}

	if (rule->flags & NSS_UDP_ST_FLAG_IPV4) {
		udp_len = pkt_sz - sizeof(struct iphdr);
		l2->eth_type = ETH_P_IP;
	} else if (rule->flags & NSS_UDP_ST_FLAG_IPV6) {
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

	/*
	 * populate ipv4 or ipv6  header
	 */
	if (rule->flags & NSS_UDP_ST_FLAG_IPV4) {
		nss_udp_st_generate_ipv4_hdr(&iph, ip_len, rule);
		l3->saddr[0] = iph.saddr;
		l3->daddr[0] = iph.daddr;
		l3->ttl = iph.ttl;
		l3->dscp = iph.tos >> 2;
		l3->proto = iph.protocol;
		l3->flags = PPE_DRV_TUN_CMN_CTX_L3_IPV4;
	} else if (rule->flags & NSS_UDP_ST_FLAG_IPV6) {
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
		pr_err("Error allocating internal tunnel dev\n");
		return false;
	}

	if (!ppe_tun_setup(rule->tun_dev, &tun_hdr)) {
		pr_err("failed to configure tunnel\n");
		free_netdev(rule->tun_dev);
		return false;
	}

	/*
	 * Get and maintain vp_num as we cannot get this from an hr timer context
	 */
	rule->vp_num = ppe_drv_port_num_from_dev(rule->tun_dev);
	if (unlikely((rule->vp_num < PPE_DRV_VIRTUAL_START) || (rule->vp_num >= PPE_DRV_VIRTUAL_END))) {
		pr_err("Not a valid Virtual Port number %d dev %s", rule->vp_num, rule->tun_dev->name);
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
 * nss_udp_st_tx_rate_change
 *	Dynamically change the rate of speedtest
 */
bool nss_udp_st_tx_rate_change(uint32_t rate)
{
	uint64_t total_bps;

	if (nust.config.rate > NSS_UDP_ST_RATE_MAX) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_RATE]);
		return false;
	}

	/*
	 * Convert Mbps to bps
	 */
	total_bps = (uint64_t)rate * 1000000;

	/*
	 * calculate number of pkts to send per rule per 10 ms
	 */
	nss_udp_st_tx_num_pkt = div_u64(total_bps , (nust.rule_count * (nust.config.buffer_sz + sizeof(struct ethhdr)) * 8 * NSS_UDP_ST_TX_TIMER));
	nss_udp_st_tx_num_pkt++;

	return true;
}

/*
 * nss_udp_st_tx_init()
 *	initialize speedtest for tx
 */
static bool nss_udp_st_tx_init(void)
{
	uint64_t total_bps;
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;

	if (nust.config.rate > NSS_UDP_ST_RATE_MAX) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_RATE]);
		return false;
	}

	if (nust.config.buffer_sz < NSS_UDP_ST_BUFFER_SIZE_MIN) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
		return false;
	}

	if (nust.config.buffer_sz > NSS_UDP_ST_BUFFER_SIZE_MAX) {
		atomic64_inc(&nust.stats.errors[NSS_UDP_ST_ERROR_INCORRECT_BUFFER_SIZE]);
		return false;
	}

	/*
	 * Check all ports are specified
	 */
	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		if (pos->sport == 0 || pos->dport == 0) {
			pr_err("Unable to tx with arbitrary ports: sport = %u, dport = %u", pos->sport, pos->dport);
			return false;
		}
	}

	/*
	 * Convert Mbps to bps
	 */
	total_bps = (uint64_t)nust.config.rate * 1000000;

	/*
	 * calculate number of pkts to send per rule per 10 ms
	 */
	nss_udp_st_tx_num_pkt = div_u64(total_bps , (nust.rule_count * (nust.config.buffer_sz + sizeof(struct ethhdr)) * 8 * NSS_UDP_ST_TX_TIMER));
	nss_udp_st_tx_num_pkt++;
	if(!nss_udp_st_set_dev()) {
		pr_err("Failed to set dev\n");
		return false;
	}

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
	kt = ktime_set(0,10000000);
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
		hrtimer_start(&tx_hr_timer[cpu], kt, HRTIMER_MODE_ABS_HARD);
		tx_timer_flag[cpu] = 1;
	} else {
		hrtimer_restart(&tx_hr_timer[cpu]);
	}
	put_cpu();
}

/*
 * nss_udp_st_tx()
 *	start speedtest for tx
 */
bool nss_udp_st_tx(void)
{
	uint32_t i;
	char qname[NSS_UDP_ST_PROCESS_NAME_SZ];

	if (!nss_udp_st_tx_init()) {
		pr_err("Failed to init tx\n");
		return false;
	}

	switch (nust_dev->type) {
	case ARPHRD_PPP:
		if (nust.config.flags & NSS_UDP_ST_FLAGS_VP) {
			pr_err("PPPOE + VP speedtest is not supported\n");
			return false;
		}
		if(nss_udp_st_pppoe_iface_config(nust_dev) < 0) {
			pr_err("Could not configure pppoe, dev: %s\n", nust_dev->name);
			return false;
		}
		break;

	case ARPHRD_ETHER:
		if ((nust_dev->priv_flags & (IFF_EBRIDGE | IFF_OPENVSWITCH))
			|| ((nust_dev->flags & IFF_MASTER) && (nust_dev->priv_flags & IFF_BONDING))) {
			pr_err("Bridge and LAG interfaces are not supported, dev: %s\n", nust_dev->name);
			return false;
		}

		if (is_vlan_dev(nust_dev)) {
			if (nust.config.flags & NSS_UDP_ST_FLAGS_VP) {
				pr_err("VLAN + VP speedtest is not supported\n");
				return false;
			}
			if (nss_udp_st_vlan_iface_config(nust_dev) < 0) {
				pr_err("Could not configure vlan, dev: %s\n", nust_dev->name);
				return false;
			}
		} else {
			xmit_dev = nust_dev;
		}

		break;

	default:
		pr_err("Unsupported speedtest interface: %s\n", nust_dev->name);
		return false;
	}

	pr_debug("Speedtest interface: %s\n", nust_dev->name);

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
