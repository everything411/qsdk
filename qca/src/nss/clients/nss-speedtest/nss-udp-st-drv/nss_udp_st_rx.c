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
#include <ppe_vp_public.h>
#include <ppe_rfs.h>
#include <linux/if_pppox.h>

DEFINE_SPINLOCK(pre_routing_hook_list_lock);

static const struct net_device_ops nss_udp_st_dummy_netdev_ops;
static struct net_device *recv_dev;

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
 * nss_udp_st_rx_fill_ipv4_rule_create_msg()
 *		Fill ppe_rfs_ipv4_rule_create_msg
 */
static void nss_udp_st_rx_fill_ipv4_rule_create_msg(struct ppe_rfs_ipv4_rule_create_msg *pr4rc, struct nss_udp_st_rules *rule) {
	pr4rc->rule_flags |= PPE_RFS_V4_RULE_FLAG_FLOW_VALID | PPE_RFS_V4_RULE_FLAG_RETURN_VALID;
	pr4rc->rule_flags |= PPE_RFS_V4_RULE_FLAG_PASSIVE_FLOW;
	pr4rc->rule_flags |= PPE_RFS_V4_RULE_UDP_ST;
	pr4rc->tuple.flow_ip = rule->dip.ip.ipv4;
	pr4rc->tuple.return_ip = rule->sip.ip.ipv4;
	pr4rc->tuple.flow_ident = rule->dport;
	pr4rc->tuple.return_ident = rule->sport;
	pr4rc->tuple.protocol = IPPROTO_UDP;
	pr4rc->qos_rule.flow_qos_tag = rule->rule_id;
	return;
}


/*
 * nss_udp_st_rx_fill_ipv6_rule_create_msg()
 *		Fill ppe_rfs_ipv6_rule_create_msg
 */
static void nss_udp_st_rx_fill_ipv6_rule_create_msg(struct ppe_rfs_ipv6_rule_create_msg *pr6rc, struct nss_udp_st_rules *rule) {
	pr6rc->rule_flags |= PPE_RFS_V6_RULE_FLAG_FLOW_VALID | PPE_RFS_V6_RULE_FLAG_RETURN_VALID;
	pr6rc->rule_flags |= PPE_RFS_V6_RULE_FLAG_PASSIVE_FLOW;
	pr6rc->rule_flags |= PPE_RFS_V6_RULE_UDP_ST;
	memcpy(pr6rc->tuple.flow_ip, rule->dip.ip.ipv6, sizeof(pr6rc->tuple.flow_ip));
	memcpy(pr6rc->tuple.return_ip, rule->sip.ip.ipv6, sizeof(pr6rc->tuple.return_ip));
	pr6rc->tuple.flow_ident = rule->dport;
	pr6rc->tuple.return_ident = rule->sport;
	pr6rc->tuple.protocol = IPPROTO_UDP;
	pr6rc->qos_rule.flow_qos_tag = rule->rule_id;
	return;
}

/*
 * nss_udp_st_rx_fill_ipv4_rule_destroy_msg()
 *		Fill ppe_rfs_ipv4_rule_destroy_msg
 */
static void nss_udp_st_rx_fill_ipv4_rule_destroy_msg(struct ppe_rfs_ipv4_rule_destroy_msg *pr4rd, struct nss_udp_st_rules *rule) {
	pr4rd->tuple.flow_ip = rule->dip.ip.ipv4;
	pr4rd->tuple.return_ip = rule->sip.ip.ipv4;
	pr4rd->tuple.flow_ident = rule->dport;
	pr4rd->tuple.return_ident = rule->sport;
	pr4rd->tuple.protocol = IPPROTO_UDP;
	return;
}

/*
 * nss_udp_st_rx_fill_ipv6_rule_destroy_msg()
 *		Fill ppe_rfs_ipv6_rule_destroy_msg
 */
static void nss_udp_st_rx_fill_ipv6_rule_destroy_msg(struct ppe_rfs_ipv6_rule_destroy_msg *pr6rd, struct nss_udp_st_rules *rule) {
	memcpy(pr6rd->tuple.flow_ip, rule->dip.ip.ipv6, sizeof(pr6rd->tuple.flow_ip));
	memcpy(pr6rd->tuple.return_ip, rule->sip.ip.ipv6, sizeof(pr6rd->tuple.return_ip));
	pr6rd->tuple.flow_ident = rule->dport;
	pr6rd->tuple.return_ident = rule->sport;
	pr6rd->tuple.protocol = IPPROTO_UDP;
	return;
}
/*
 * nss_udp_st_rx_rfs_exception_rule_create()
 *		For exception case (i.e. dport == NSS_UDP_ST_EXCEPTION_DPORT) create PPE RFS entry
 */
static bool nss_udp_st_rx_rfs_exception_rule_create(struct nss_udp_st_rules *rule)
{
	struct ppe_rfs_ipv4_rule_create_msg pr4rc = {0};
	struct ppe_rfs_ipv6_rule_create_msg pr6rc = {0};
	struct net_device *dev;

	if (!is_dummy_vp_exists) {
		udp_st_err("dummy vp is not created");
		return false;
	}

	dev = ppe_vp_get_netdev_by_port_num(nust.dummy_vp_num);
	if(!dev) {
		udp_st_err("dev is null");
		return false;
	}

	nust_dev = dev_get_by_name(&init_net, nust.config.net_dev);
	if(!nust_dev) {
		udp_st_err("nust_dev is null");
		return false;
	}

	if (rule->ip_version == NSS_UDP_ST_FLAG_IPV4) {
		if (!ppe_rfs_rule_eligible_get(recv_dev->ifindex, dev->ifindex, false, 0)) {
			udp_st_err("Flow is not RFS eligible, flow interface num: %d, return interface num: %d\n", nust_dev->ifindex, dev->ifindex);
			dev_put(nust_dev);
			return false;
		}
		pr4rc.conn_rule.return_interface_num = dev->ifindex;
		pr4rc.conn_rule.flow_interface_num = recv_dev->ifindex;
		pr4rc.conn_rule.return_top_interface_num = dev->ifindex;
		pr4rc.conn_rule.flow_top_interface_num = nust_dev->ifindex;
		pr4rc.conn_rule.flow_mtu = ETH_DATA_LEN;
		pr4rc.conn_rule.return_mtu = ETH_DATA_LEN;
		nss_udp_st_rx_fill_ipv4_rule_create_msg(&pr4rc, rule);
		if (ppe_rfs_ipv4_rule_create(&pr4rc) != PPE_RFS_RET_SUCCESS){
			atomic64_inc(&nust.stats.rx_exception_pkt_cnt);
			dev_put(nust_dev);
			return false;
		}
	} else if (rule->ip_version == NSS_UDP_ST_FLAG_IPV6) {
		if (!ppe_rfs_rule_eligible_get(recv_dev->ifindex, dev->ifindex, false, 0)) {
			udp_st_err("Flow is not RFS eligible, flow interface num: %d, return interface num: %d\n", nust_dev->ifindex, dev->ifindex);
			dev_put(nust_dev);
			return false;
		}
		pr6rc.conn_rule.return_interface_num = dev->ifindex;
		pr6rc.conn_rule.flow_interface_num = recv_dev->ifindex;
		pr6rc.conn_rule.return_top_interface_num = dev->ifindex;
		pr6rc.conn_rule.flow_top_interface_num = nust_dev->ifindex;
		pr6rc.conn_rule.flow_mtu = ETH_DATA_LEN;
		pr6rc.conn_rule.return_mtu = ETH_DATA_LEN;
		nss_udp_st_rx_fill_ipv6_rule_create_msg(&pr6rc, rule);
		if (ppe_rfs_ipv6_rule_create(&pr6rc) != PPE_RFS_RET_SUCCESS){
			atomic64_inc(&nust.stats.rx_exception_pkt_cnt);
			dev_put(nust_dev);
			return false;
		}
	}

	dev_put(nust_dev);
	return true;
}

/*
 * nss_udp_st_ipv4_exception_rule()
 * 	check if ipv4 exception rule matches
 */
static bool nss_udp_st_ipv4_exception_rule_match(struct nss_udp_st_rules *rule, uint32_t pkt_daddr, uint32_t pkt_saddr, uint16_t pkt_dport, uint16_t pkt_sport) {
	return ((rule->ip_version == NSS_UDP_ST_FLAG_IPV4) &&
		(rule->sip.ip.ipv4 == pkt_daddr) &&
		(rule->dip.ip.ipv4 == pkt_saddr) &&
		(rule->sport == pkt_dport) &&
		((rule->dport == NSS_UDP_ST_EXCEPTION_DPORT) || rule->dport == pkt_sport));
}

/*
 * nss_udp_st_ipv6_exception_rule()
 * 	check if ipv6 exception rule matches
 */
static bool nss_udp_st_ipv6_exception_rule_match(struct nss_udp_st_rules *rule, struct in6_addr pkt_daddr, struct in6_addr pkt_saddr, uint16_t pkt_dport, uint16_t pkt_sport) {
	return ((rule->ip_version == NSS_UDP_ST_FLAG_IPV6) &&
		(nss_udp_st_compare_ipv6(rule->sip.ip.ipv6, pkt_daddr.s6_addr32)) &&
		(nss_udp_st_compare_ipv6(rule->dip.ip.ipv6, pkt_saddr.s6_addr32)) &&
		(rule->sport == pkt_dport) &&
		((rule->dport == NSS_UDP_ST_EXCEPTION_DPORT) || rule->dport == pkt_sport));
}

/*
 * nss_udp_st_rx_receive_skb()
 *	Called from nss-dp inplace of netif_receive_skb for processing of udp_st packets.
 */
void nss_udp_st_rx_receive_skb(struct sk_buff *skb)
{
	struct nss_udp_st_rules *rule = NULL;
	struct nss_udp_st_rules *n = NULL;
	uint8_t rule_id = 0;

	rule_id = (skb->mark & NSS_UDP_ST_RULE_ID_MASK);
	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {
		if (rule->rule_id == rule_id) {
			if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
				nss_udp_st_process_payload(skb, rule, rule->ip_version);
			}
			nss_udp_st_update_stats(skb->len, 1);
			kfree_skb(skb);
			return;
		}
	}

	/*
	 * Check if the rule_id is the exception rule id.
	 */
	spin_lock_bh(&pre_routing_hook_list_lock);
	for (int i = 0; i < exception_rules_cnt; i++) {
		rule = &exception_dport_rules[i];
		if (rule->rule_id == rule_id) {
			if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
				nss_udp_st_process_payload(skb, rule, rule->ip_version);
			}
			spin_unlock_bh(&pre_routing_hook_list_lock);
			nss_udp_st_update_stats(skb->len, 1);
			kfree_skb(skb);
			return;
		}
	}
	spin_unlock_bh(&pre_routing_hook_list_lock);
	kfree_skb(skb);
	return;
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
	spin_lock_bh(&pre_routing_hook_list_lock);
	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {
		/*
		 * If incoming packet matches 5tuple, it is a speedtest packet.
		 * Increase Rx packet stats and drop packet.
		 */
		if (!nss_udp_st_ipv4_exception_rule_match(rule, ntohl(iph->daddr), ntohl(iph->saddr), ntohs(uh->dest), ntohs(uh->source))) {
			continue;
		}
		if (exception_rules_cnt < 255) {
			exception_dport_rules[exception_rules_cnt].ip_version = NSS_UDP_ST_FLAG_IPV4;
			exception_dport_rules[exception_rules_cnt].sip.ip.ipv4 = rule->sip.ip.ipv4;
			exception_dport_rules[exception_rules_cnt].dip.ip.ipv4 = rule->dip.ip.ipv4;
			exception_dport_rules[exception_rules_cnt].sport = rule->sport;
			exception_dport_rules[exception_rules_cnt].dport = ntohs(uh->source);
			exception_dport_rules[exception_rules_cnt].rule_id = nust.rule_count + exception_rules_cnt + 1;

			if (nss_udp_st_rx_rfs_exception_rule_create(&exception_dport_rules[exception_rules_cnt])) {
				exception_rules_cnt++;
			}
		}
		if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
			nss_udp_st_process_payload(skb, rule, NSS_UDP_ST_FLAG_IPV4);
		}
		spin_unlock_bh(&pre_routing_hook_list_lock);
		nss_udp_st_update_stats(skb->len, 1);
		kfree_skb(skb);
		return NF_STOLEN;
	}
	spin_unlock_bh(&pre_routing_hook_list_lock);

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
	spin_lock_bh(&pre_routing_hook_list_lock);
	list_for_each_entry_safe(rule, n, &nust.rules.list, list) {
		/*
		 * If incoming packet matches 5tuple, it is a speedtest packet.
		 * Increase Rx packet stats and drop packet.
		 */
		if (!nss_udp_st_ipv6_exception_rule_match(rule, daddr, saddr, ntohs(uh->dest), ntohs(uh->source))) {
			continue;
		}
		if (exception_rules_cnt < 255) {
			exception_dport_rules[exception_rules_cnt].ip_version = NSS_UDP_ST_FLAG_IPV6;
			memcpy(exception_dport_rules[exception_rules_cnt].sip.ip.ipv6, rule->sip.ip.ipv6, sizeof(exception_dport_rules[exception_rules_cnt].sip.ip.ipv6));
			memcpy(exception_dport_rules[exception_rules_cnt].dip.ip.ipv6, rule->dip.ip.ipv6, sizeof(exception_dport_rules[exception_rules_cnt].dip.ip.ipv6));
			exception_dport_rules[exception_rules_cnt].sport = rule->sport;
			exception_dport_rules[exception_rules_cnt].dport = ntohs(uh->source);
			exception_dport_rules[exception_rules_cnt].rule_id = nust.rule_count + exception_rules_cnt + 1;

			if (nss_udp_st_rx_rfs_exception_rule_create(&exception_dport_rules[exception_rules_cnt])) {
				exception_rules_cnt++;
			}
		}
		if (nust.config.flags & NSS_UDP_ST_FLAGS_TIMESTAMP) {
			nss_udp_st_process_payload(skb, rule, NSS_UDP_ST_FLAG_IPV6);
		}
		spin_unlock_bh(&pre_routing_hook_list_lock);
		nss_udp_st_update_stats(skb->len, 1);
		kfree_skb(skb);
		return NF_STOLEN;
	}
	spin_unlock_bh(&pre_routing_hook_list_lock);

	return NF_ACCEPT;
}

/*
 * nss_udp_st_rx_dummy_netdev_setup()
 *	setup dummy netdevice
 */
static void nss_udp_st_rx_dummy_netdev_setup(struct net_device *dev)
{
	dev->addr_len = ETH_ALEN;
	dev->mtu = ETH_DATA_LEN;
	dev->needed_headroom = NSS_UDP_ST_MIN_HEADROOM;
	dev->needed_tailroom = NSS_UDP_ST_MIN_TAILROOM;
	dev->type = ARPHRD_VOID;
	dev->ethtool_ops = NULL;
	dev->header_ops = NULL;
	dev->netdev_ops = &nss_udp_st_dummy_netdev_ops;
	dev->priv_destructor = NULL;

	memcpy((void*)dev->dev_addr, "\x00\x00\x00\x00\x00\x00", dev->addr_len);
	memset(dev->broadcast, 0xff, dev->addr_len);
	memcpy(dev->perm_addr, dev->dev_addr, dev->addr_len);
}

/*
 * nss_udp_st_rx_free_dummy_vp()
 *	free dummy vp and dummy netdevice
 */
void nss_udp_st_rx_free_dummy_vp(int32_t dummy_vp_num)
{
	struct net_device *dummy_netdev = ppe_vp_get_netdev_by_port_num(dummy_vp_num);
	ppe_vp_free(dummy_vp_num);

        if (dummy_netdev) {
		unregister_netdev(dummy_netdev);
		free_netdev(dummy_netdev);
	}

	return;
}

/*
 * nss_udp_st_rx_dummy_vp_alloc()
 *	alloc and register dummy netdevice
 *	alloc dummy passive vp
 */
int32_t nss_udp_st_rx_dummy_vp_alloc(void)
{
	struct ppe_vp_ai vpai;
	int32_t vp_num = -1;
	int status = 0;
	struct net_device *dummy_netdev = alloc_netdev(0, "dummy_netdev%d",
                                       NET_NAME_ENUM, nss_udp_st_rx_dummy_netdev_setup); 
	if (!dummy_netdev) {
		udp_st_err("dummy_netdev alloc failed");
		return -1;
	}

	status = register_netdev(dummy_netdev);
        if (status < 0) {
                udp_st_err("%px: Failed to register dummy netdevice, error(%d)\n", dummy_netdev, status);
		free_netdev(dummy_netdev);
                return -1;
        }

	memset(&vpai, 0, sizeof(struct ppe_vp_ai));
	vpai.usr_type = PPE_VP_USER_TYPE_PASSIVE;
	vpai.type = PPE_VP_TYPE_SW_L3;
	vpai.core_mask = nust.config.cpu_bitmap;

	vp_num = ppe_vp_alloc(dummy_netdev, &vpai);
	if (vp_num == -1) {
		udp_st_err("vp_alloc failed");
		unregister_netdev(dummy_netdev);
		free_netdev(dummy_netdev);
	}
	return vp_num;
}

/*
 * nss_udp_st_rx_rfs_rule_create()
 *	create PPE RFS rule entry
 */
bool nss_udp_st_rx_rfs_rule_create() {
	struct nss_udp_st_rules *pos = NULL;
	struct nss_udp_st_rules *n = NULL;
	struct ppe_rfs_ipv4_rule_create_msg pr4rc = {0};
	struct ppe_rfs_ipv6_rule_create_msg pr6rc = {0};
	struct net_device *dev;
	struct pppoe_opt info;
	struct ppp_channel *ppp_chan[1];
        int channel_count;
        int channel_protocol;

	/*
	 * HW offload creates the PPE flow explicitly via nss_udp_st_ppe_create_flows().
	 * Pushing an RFS rule for the same 5-tuple here would race with that path and
	 * collide in the PPE flow table, so skip RFS entirely for the offload case.
	 */
	if (nust.config.flags & NSS_UDP_ST_FLAGS_HW_OFFLOAD) {
		udp_st_trace("HW offload enabled, skipping RFS rule creation\n");
		return true;
	}

	if (!is_dummy_vp_exists) {
		udp_st_err("dummy vp is not created");
		return false;
	}

	dev = ppe_vp_get_netdev_by_port_num(nust.dummy_vp_num);
	if(!dev) {
		udp_st_err("dev is null");
		return false;
	}

	nust_dev = dev_get_by_name(&init_net, nust.config.net_dev);
	if(!nust_dev) {
		udp_st_err("nust_dev is null");
		return false;
	}

	recv_dev = nust_dev;

	if (nust_dev->type == ARPHRD_PPP) {
		/*
		 * Gets the PPPoE channel information.
		 */
		channel_count = ppp_hold_channels(nust_dev, ppp_chan, 1);
		if (channel_count != 1) {
			udp_st_warn("%px: Unable to get the channel for device: %s\n", dev, dev->name);
			dev_put(nust_dev);
			return false;
		}

		channel_protocol = ppp_channel_get_protocol(ppp_chan[0]);
		if (channel_protocol != PX_PROTO_OE) {
			udp_st_warn("%px: PPP channel protocol is not PPPoE for device: %s\n", nust_dev, nust_dev->name);
			ppp_release_channels(ppp_chan, 1);
			dev_put(nust_dev);
			return false;
		}

		if (pppoe_channel_addressing_get(ppp_chan[0], &info)) {
			udp_st_warn("%px: Unable to get the PPPoE session information for device: %s\n", nust_dev, nust_dev->name);
			ppp_release_channels(ppp_chan, 1);
			dev_put(nust_dev);
			return false;
		}

		recv_dev = info.dev;
	}

	if (is_vlan_dev(recv_dev)) {
		recv_dev = vlan_dev_next_dev(recv_dev);
	}


	if (!recv_dev) {
		udp_st_err("recv_dev is null");
	}

	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		if (pos->dport == NSS_UDP_ST_EXCEPTION_DPORT) {
			continue;
		}

		if (pos->ip_version == NSS_UDP_ST_FLAG_IPV4) {
			if (!ppe_rfs_rule_eligible_get(recv_dev->ifindex, dev->ifindex, false, 0)) {
				udp_st_err("Flow is not RFS eligible, flow interface num: %d, return interface num: %d\n", nust_dev->ifindex, dev->ifindex);
				dev_put(nust_dev);
				return false;
			}
			pr4rc.conn_rule.return_interface_num = dev->ifindex;
			pr4rc.conn_rule.flow_interface_num = recv_dev->ifindex;
			pr4rc.conn_rule.return_top_interface_num = dev->ifindex;
			pr4rc.conn_rule.flow_top_interface_num = nust_dev->ifindex;
			pr4rc.conn_rule.flow_mtu = ETH_DATA_LEN;
			pr4rc.conn_rule.return_mtu = ETH_DATA_LEN;
			nss_udp_st_rx_fill_ipv4_rule_create_msg(&pr4rc, pos);
			if (ppe_rfs_ipv4_rule_create(&pr4rc) != PPE_RFS_RET_SUCCESS) {
				udp_st_err("%p: Error in creating PPE RFS rule\n", &pr4rc);
				dev_put(nust_dev);
				return false;
			}
		} else if (pos->ip_version == NSS_UDP_ST_FLAG_IPV6) {
			if (!ppe_rfs_rule_eligible_get(recv_dev->ifindex, dev->ifindex, false, 0)) {
				udp_st_err("Flow is not RFS eligible, flow interface num: %d, return interface num: %d\n", nust_dev->ifindex, dev->ifindex);
				dev_put(nust_dev);
				return false;
			}
			pr6rc.conn_rule.return_interface_num = dev->ifindex;
			pr6rc.conn_rule.flow_interface_num = recv_dev->ifindex;
			pr6rc.conn_rule.return_top_interface_num = dev->ifindex;
			pr6rc.conn_rule.flow_top_interface_num = nust_dev->ifindex;
			pr6rc.conn_rule.flow_mtu = ETH_DATA_LEN;
			pr6rc.conn_rule.return_mtu = ETH_DATA_LEN;
			nss_udp_st_rx_fill_ipv6_rule_create_msg(&pr6rc, pos);
			if (ppe_rfs_ipv6_rule_create(&pr6rc) != PPE_RFS_RET_SUCCESS){
				udp_st_err("%p: Error in creating PPE RFS rule\n", &pr6rc);
				dev_put(nust_dev);
				return false;
			}
		}

	}

	dev_put(nust_dev);
	return true;
}

/*
 * nss_udp_st_rx_rfs_rule_destroy()
 *	destroy PPE RFS rule entry
 */
void nss_udp_st_rx_rfs_rule_destroy()
{
	struct nss_udp_st_rules *pos = NULL;
        struct nss_udp_st_rules *n = NULL;
	struct ppe_rfs_ipv4_rule_destroy_msg pr4rd = {0};
	struct ppe_rfs_ipv6_rule_destroy_msg pr6rd = {0};
	list_for_each_entry_safe(pos, n, &nust.rules.list, list) {
		if (pos->dport == NSS_UDP_ST_EXCEPTION_DPORT)
			continue;

		if (pos->ip_version == NSS_UDP_ST_FLAG_IPV4) {
			nss_udp_st_rx_fill_ipv4_rule_destroy_msg(&pr4rd, pos);
			if (ppe_rfs_ipv4_rule_destroy(&pr4rd) != PPE_RFS_RET_SUCCESS) {
				udp_st_err("%p: Error in deleting PPE RFS rule\n", &pr4rd);
			}
		} else if (pos->ip_version == NSS_UDP_ST_FLAG_IPV6) {
			nss_udp_st_rx_fill_ipv6_rule_destroy_msg(&pr6rd, pos);
			if (ppe_rfs_ipv6_rule_destroy(&pr6rd) != PPE_RFS_RET_SUCCESS) {
				udp_st_err("%p: Error in deleting PPE RFS rule\n", &pr6rd);
			}
		}
	}

	spin_lock_bh(&pre_routing_hook_list_lock);
	for (int i = 0; i < exception_rules_cnt; i++) {
		pos = &exception_dport_rules[i];

		if (pos->ip_version == NSS_UDP_ST_FLAG_IPV4) {
			nss_udp_st_rx_fill_ipv4_rule_destroy_msg(&pr4rd, pos);
			if (ppe_rfs_ipv4_rule_destroy(&pr4rd) != PPE_RFS_RET_SUCCESS) {
				udp_st_err("%p: Error in deleting PPE RFS rule\n", &pr4rd);
			}
		} else if (pos->ip_version == NSS_UDP_ST_FLAG_IPV6) {
			nss_udp_st_rx_fill_ipv6_rule_destroy_msg(&pr6rd, pos);
			if (ppe_rfs_ipv6_rule_destroy(&pr6rd) != PPE_RFS_RET_SUCCESS) {
				udp_st_err("%p: Error in deleting PPE RFS rule\n", &pr6rd);
			}
		}
	}
	spin_unlock_bh(&pre_routing_hook_list_lock);

	return;
}

