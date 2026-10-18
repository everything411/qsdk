/*
 * sfe_ipv4_tun6rd.c
 *	Shortcut forwarding engine file for IPv4 TUN6RD
 *
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/debug_mem_usage.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/version.h>
#include <net/protocol.h>
#include <net/ip.h>

#include "sfe_debug.h"
#include "sfe_api.h"
#include "sfe.h"
#include "sfe_flow_cookie.h"
#include "sfe_ipv4.h"
#include "sfe_pppoe.h"
#include "sfe_vlan.h"
#include "sfe_trustsec.h"

/*
 * sfe_ipv4_recv_tun6rd()
 *	Handle TUN6RD packet receives and forwarding.
 */
int sfe_ipv4_recv_tun6rd(struct sfe_ipv4 *si, struct sk_buff *skb, struct net_device *dev,
			     unsigned int len, struct iphdr *iph, unsigned int ihl,
				 bool sync_on_find, struct sfe_l2_info *l2_info, bool tun_outer)
{
	struct net_device *offload_xmit_dev = NULL;
	u64 __percpu *offload_stat = NULL;
	__be32 src_ip;
	__be32 dest_ip;
	__be16 src_port = 0;
	__be16 dest_port = 0;
	bool passthrough;
	bool bridge_flow;
	bool hw_csum;
	struct sfe_ipv4_connection_match *cm;

	DEBUG_TRACE("%px: sfe: sfe_ipv4_recv_tun6rd called.\n", skb);

	/*
	 * Read the IP address information. Read the IP header data first
	 * because we've almost certainly got that in the cache.
	 */
	src_ip = iph->saddr;
	dest_ip = iph->daddr;

	rcu_read_lock();

	/*
	 * Look for a connection match.
	 */
#ifdef CONFIG_NF_FLOW_COOKIE
	cm = si->sfe_flow_cookie_table[skb->flow_cookie & SFE_FLOW_COOKIE_MASK].match;
	if (unlikely(!cm)) {
		cm = sfe_ipv4_find_connection_match_rcu(si, dev, l2_info, IPPROTO_IPV6, src_ip, src_port, dest_ip, dest_port);
	}
#else
	cm = sfe_ipv4_find_connection_match_rcu(si, dev, l2_info, IPPROTO_IPV6, src_ip, src_port, dest_ip, dest_port);
#endif
	if (unlikely(!cm)) {
		rcu_read_unlock();
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_TUN6RD_NO_CONNECTION);
		DEBUG_TRACE("%px: no tun6rd connection found\n", skb);
		return 0;
	}

	passthrough = cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PASSTHROUGH;

	/*
	 * If our packet has been marked as "sync on find" we will sync the status
	 * and forward it to slowpath.
	 */
	if (unlikely(sync_on_find) && !passthrough) {
		sfe_ipv4_sync_status(si, cm->connection, SFE_RULE_SYNC_REASON_STATS);
		rcu_read_unlock();
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_TUN6RD_SYNC_ON_FIND);
		DEBUG_TRACE("%px: Sync on find\n", skb);

		return 0;
	}

	bridge_flow = !!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_BRIDGE_FLOW);

	/*
	 * In the passthrough case, the packet needs to decrement ttl.
	 */
	if (!bridge_flow && passthrough) {
		if (iph->ttl < 2) {
			sfe_ipv4_sync_status(si, cm->connection, SFE_RULE_SYNC_REASON_STATS);
			rcu_read_unlock();

			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_TUN6RD_SMALL_TTL);
			DEBUG_TRACE("%px: ttl too low\n", skb);
			return 0;
		}
	}

	/*
	 * For PPPoE packets, match server MAC and session id
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PPPOE_DECAP)) {
		struct ethhdr *eth;
		bool pppoe_match;

		if (unlikely(!sfe_l2_parse_flag_check(l2_info, SFE_L2_PARSE_FLAGS_PPPOE_INGRESS))) {
			rcu_read_unlock();
			DEBUG_TRACE("%px: PPPoE header not present in packet for PPPoE rule\n", skb);
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INCORRECT_PPPOE_PARSING);
			return 0;
		}

		eth = eth_hdr(skb);

		pppoe_match = (cm->pppoe_session_id == sfe_l2_pppoe_session_id_get(l2_info)) &&
				ether_addr_equal((u8*)cm->pppoe_remote_mac, (u8 *)eth->h_source);

		if (unlikely(!pppoe_match)) {
			rcu_read_unlock();
			DEBUG_TRACE("%px: PPPoE session ID %d and %d or MAC %pM and %pM did not match\n",
					skb, cm->pppoe_session_id, sfe_l2_pppoe_session_id_get(l2_info),
					cm->pppoe_remote_mac, eth->h_source);
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INVALID_PPPOE_SESSION);
			return 0;
		}

		skb->protocol = htons(l2_info->protocol);
		this_cpu_inc(si->stats_pcpu->pppoe_decap_packets_forwarded64);
	} else if (unlikely(sfe_l2_parse_flag_check(l2_info, SFE_L2_PARSE_FLAGS_PPPOE_INGRESS))) {

		/*
		 * If packet contains PPPoE header but CME doesn't contain PPPoE flag yet we are exceptioning
		 * the packet to linux
		 */
		if (unlikely(!bridge_flow)) {
			rcu_read_unlock();
			DEBUG_TRACE("%px: CME doesn't contain PPPoE flag but packet has PPPoE header\n", skb);
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_PPPOE_NOT_SET_IN_CME);
			return 0;
		}

		/*
		 * For bridged flows when packet contains PPPoE header, restore the header back and forward
		 * to xmit interface
		 */
		__skb_push(skb, PPPOE_SES_HLEN);
		this_cpu_inc(si->stats_pcpu->pppoe_bridge_packets_forwarded64);
	}

	if (unlikely(!bridge_flow && tun_outer)) {
		if (cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_XLATE_SRC)
			iph->saddr = cm->xlate_src_ip;

		if(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_XLATE_DEST)
			iph->daddr = cm->xlate_dest_ip;
	}

	/*
	 * If cm->proto is set, it means the decap path.
	 * Otherwise we forward the packet in encap path.
	 */
	if(cm->proto) {
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
		const struct net_protocol *ipprot = cm->proto;
#else
		struct net_protocol *ipprot = cm->proto;
#endif

		/*
		 * Do we expect an ingress VLAN tag for this flow?
		 * Note: We will only have ingress tag check in decap direction.
		 * Here, no modification is needed, we only check tag match between
		 * vlan hdr stored in cm and l2_info.
		 */
		if (unlikely(!sfe_vlan_validate_ingress_tag(skb, cm->ingress_vlan_hdr_cnt, cm->ingress_vlan_hdr, l2_info, 0))) {
			rcu_read_unlock();
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INGRESS_VLAN_TAG_MISMATCH);
			DEBUG_TRACE("VLAN tag mismatch. skb=%px\n"
				"cm: %u [0]=%x/%x [1]=%x/%x\n"
				"l2_info+: %u [0]=%x/%x [1]=%x/%x\n", skb,
				cm->ingress_vlan_hdr_cnt,
				htons(cm->ingress_vlan_hdr[0].tpid), cm->ingress_vlan_hdr[0].tci,
				htons(cm->ingress_vlan_hdr[1].tpid), cm->ingress_vlan_hdr[1].tci,
				l2_info->vlan_hdr_cnt,
				htons(l2_info->vlan_hdr[0].tpid), l2_info->vlan_hdr[0].tci,
				htons(l2_info->vlan_hdr[1].tpid), l2_info->vlan_hdr[1].tci);
			return 0;
		}

		/*
		 * Do we expect a trustsec header for this flow ?
		 */
		if (unlikely(!sfe_trustsec_validate_ingress_sgt(skb, cm->ingress_trustsec_valid, &cm->ingress_trustsec_hdr, l2_info))) {
			rcu_read_unlock();
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INGRESS_TRUSTSEC_SGT_MISMATCH);
			DEBUG_TRACE("Trustsec SGT mismatch. skb=%px\n", skb);
			return 0;
		}

		skb_reset_network_header(skb);
		skb_pull(skb, ihl);
		skb_reset_transport_header(skb);

		/*
		 * ipprot->handler(skb) will always return 0;
		 * There is no way to tell whether the packet is dropped later in linux or not.
		 * Hence here inc the byte/packet count always.
		 */
		atomic_inc(&cm->rx_packet_count);
		atomic_add(len, &cm->rx_byte_count);
		rcu_read_unlock();
		this_cpu_inc(si->stats_pcpu->packets_forwarded64);
		DEBUG_TRACE("%px: %s decap done \n", skb, __func__);

		/*
		 * Update top interface for tunnel searching.
		 */
		skb->dev = cm->top_interface_dev;
		mem_debug_update_skb(skb);
		ipprot->handler(skb);
		return 1;

	}

	/*
	 * If our packet is larger than the MTU of the transmit interface then
	 * we can't forward it easily.
	 */
	if (unlikely(len > cm->xmit_dev_mtu)) {
		sfe_ipv4_sync_status(si, cm->connection, SFE_RULE_SYNC_REASON_STATS);
		rcu_read_unlock();

		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_TUN6RD_NEEDS_FRAGMENTATION);
		DEBUG_TRACE("%px: Larger than mtu\n", skb);
		return 0;
	}

	/*
	 * For PPPoE flows, add PPPoE header before L2 header is added.
	 * SFE + PPPOE flow is not supported with GSO, hence destroy the connection
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PPPOE_ENCAP)) {
		if ((unlikely(skb_shinfo(skb)->gso_segs))) {
			int ret;
			struct sfe_ipv4_connection *c = cm->connection;
			spin_lock_bh(&si->lock);
			ret = sfe_ipv4_remove_connection(si, c);
			spin_unlock_bh(&si->lock);
			if (ret) {
				sfe_ipv4_flush_connection(si, c, SFE_RULE_SYNC_REASON_FLUSH);
			}

			rcu_read_unlock();
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_GSO_NOT_SUPPORTED);
			DEBUG_TRACE("%px: GSO is not support\n", skb);
			return 0;
		}

		sfe_pppoe_add_header(skb, cm->pppoe_session_id, PPP_IP);
		this_cpu_inc(si->stats_pcpu->pppoe_encap_packets_forwarded64);
	}

	/*
	 * For trustsec flows, add trustsec header before L2 header is added.
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_INSERT_EGRESS_TRUSTSEC_SGT)) {
		sfe_trustsec_add_sgt(skb, &cm->egress_trustsec_hdr);
	}

	/*
	 * Check to see if we need to add VLAN tags
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_INSERT_EGRESS_VLAN_TAG)) {

		/*
		 * Check if skb has enough headroom to write L2 headers
		 */
		if (unlikely(skb_headroom(skb) < cm->l2_hdr_size)) {
			rcu_read_unlock();
			DEBUG_WARN("%px: Not enough headroom: %u\n", skb, skb_headroom(skb));
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_NO_HEADROOM);
			return 0;
		}
		sfe_vlan_add_tag(skb, cm->egress_vlan_hdr_cnt, cm->egress_vlan_hdr);
	}

	/*
	 * Decrement our TTL
	 */
	iph->ttl = (iph->ttl - (u8)(!bridge_flow && !tun_outer));

	/*
	 * Update DSCP
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_DSCP_REMARK)) {
		iph->tos = (iph->tos & SFE_IPV4_DSCP_MASK) | cm->dscp;
	}

	/*
	 * If HW checksum offload is not possible, full L3 checksum and incremental L4 checksum
	 * are used to update the packet. Setting ip_summed to CHECKSUM_UNNECESSARY ensures checksum is
	 * not recalculated further in packet path.
	 */
	hw_csum = !!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_CSUM_OFFLOAD) && (skb->ip_summed == CHECKSUM_UNNECESSARY);
	if (likely(hw_csum)) {
		skb->ip_summed = CHECKSUM_PARTIAL;
	} else {
		iph->check = sfe_ipv4_gen_ip_csum(iph);
	}

	/*
	 * Set SKB packet type to PACKET_HOST
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PACKET_HOST)) {
		skb->pkt_type = PACKET_HOST;
	}

	/*
	 * Update traffic stats.
	 */
	atomic_inc(&cm->rx_packet_count);
	atomic_add(len, &cm->rx_byte_count);

	skb->dev = cm->xmit_dev;

	/*
	 * Check to see if we need to write a header.
	 */
	if (likely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_WRITE_L2_HDR)) {
		if (unlikely(!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_WRITE_FAST_ETH_HDR))) {
			dev_hard_header(skb, cm->xmit_dev, ntohs(skb->protocol),
					cm->xmit_dest_mac, cm->xmit_src_mac, len);
		} else {
			struct ethhdr *eth = (struct ethhdr *)__skb_push(skb, ETH_HLEN);
			eth->h_proto = skb->protocol;
			ether_addr_copy((u8 *)eth->h_dest, (u8 *)cm->xmit_dest_mac);
			ether_addr_copy((u8 *)eth->h_source, (u8 *)cm->xmit_src_mac);
		}
	}

	/*
	 * Check if TX dev is tun vp.
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PPE_TUN_VP_VALID)) {
		offload_xmit_dev = cm->xmit_dev;
		offload_stat = &si->stats_pcpu->ppe_tun_vp_tx64;
	}
#ifdef CONFIG_IPQ_PON
	else if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PPE_VEIP_PON_VP_VALID)) {
		offload_xmit_dev = cm->offload_xmit_dev;
		offload_stat = &si->stats_pcpu->ppe_veip_pon_vp_tx64;
	}
#endif

	/*
	 * Update priority of skb.
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PRIORITY_REMARK)) {
		skb->priority = cm->priority;
	}

	/*
	 * Mark outgoing packet.
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_MARK)) {
		skb->mark = cm->mark;
	}

	this_cpu_inc(si->stats_pcpu->packets_forwarded64);

	/*
	 * For tunnel Packets check if tunnel is active in PPE.
	 * If tunnel is active queue the packet to tunnel VP.
	 */
	if (unlikely(offload_xmit_dev)) {
		struct netdev_hw_offload_ops *offload_ops = rcu_dereference(offload_xmit_dev->offload_ops);
		mem_debug_update_skb(skb);
		if (likely(offload_ops && offload_ops->xmit && offload_ops->xmit(offload_xmit_dev, skb))) {
			this_cpu_inc(*offload_stat);
			rcu_read_unlock();
			return 1;
		}
	}

	rcu_read_unlock();

	/*
	 * We're going to check for GSO flags when we transmit the packet so
	 * start fetching the necessary cache line now.
	 */
	prefetch(skb_shinfo(skb));

	/*
	 * Mark that this packet has been fast forwarded and send it on its way.
	 */
	skb->fast_forwarded = 1;
	mem_debug_update_skb(skb);

	dev_queue_xmit(skb);

	return 1;
}
