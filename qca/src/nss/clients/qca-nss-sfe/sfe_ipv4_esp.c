/*
 * sfe_ipv4_esp.c
 *	Shortcut forwarding engine - IPv4 ESP implementation
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debug_mem_usage.h>
#include <linux/skbuff.h>
#include <net/protocol.h>
#include <net/ip.h>
#include <linux/etherdevice.h>
#include <linux/lockdep.h>
#ifdef CONFIG_XFRM
#include <net/xfrm.h>
#endif

#include "sfe_debug.h"
#include "sfe_api.h"
#include "sfe.h"
#include "sfe_flow_cookie.h"
#include "sfe_ipv4.h"
#include "sfe_ipv4_esp.h"
#include "sfe_vlan.h"
#include "sfe_trustsec.h"
#include "sfe_pppoe.h"

/*
 * sfe_ipv4_recv_esp()
 *	Handle ESP packet receives and forwarding
 */
int sfe_ipv4_recv_esp(struct sfe_ipv4 *si, struct sk_buff *skb, struct net_device *dev,
		unsigned int len, struct iphdr *iph, unsigned int ihl,
		bool sync_on_find, struct sfe_l2_info *l2_info, bool tun_outer)
{
	struct sfe_ipv4_connection_match *cm;
	struct net_device *xmit_dev;
	struct net_protocol *ipprot;
	netdev_features_t features;
#ifdef CONFIG_IPQ_PON
	struct net_device *offload_xmit_dev;
	struct netdev_hw_offload_ops *offload_ops;
#endif
	bool vlan_passthrough = false;
	bool passthrough;
	bool bridge_flow;
	bool fast_xmit;
	bool hw_csum;
	__be32 src_ip;
	__be32 dest_ip;
	bool ret;
	u8 ttl;
	u8 ingress_flags = 0;
	__be16 sport = 0;
	__be16 dport = 0;
#if defined(SFE_ESP_SPI_PASSTH_ENABLE)
	struct ip_esp_hdr *esp = NULL;
	u32 spi = 0;
#ifdef CONFIG_XFRM
	struct xfrm_state *x = NULL;
#endif
#endif

	/*
	 * Is our packet too short to contain a valid ESP header?
	 */
	if (unlikely(!pskb_may_pull(skb, (sizeof(struct ip_esp_hdr) + ihl)))) {
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_ESP_HEADER_INCOMPLETE);
		DEBUG_TRACE("%px: packet too short for ESP header\n", skb);
		return 0;
	}

#if defined(SFE_ESP_SPI_PASSTH_ENABLE)
	/*
	 * For IPSec passthrough with SPI cases, extract the SPI from the
	 * packet and use it as sport and dport for connection lookup.
	 * The upper 16 bits shall be used as sport and lower 16 as dport.
	 */
	if (sfe_is_spi_based_esp_passthrough_enabled()) {
		/*
		 * If this is not a passthrough packet, then dont use ESP index to match the connection.
		 */
		esp = (struct ip_esp_hdr *)((uint8_t *)iph + ihl);
#ifdef CONFIG_XFRM
		x = xfrm_state_lookup_byspi(dev_net(skb->dev), esp->spi, AF_INET);
		if (x) {
			xfrm_state_put(x);
			goto xfrm_check_exit;
		}
#endif
		spi = ntohl(esp->spi);
		sport = SFE_ESP_SPI_TO_SPORT(spi);
		dport = SFE_ESP_SPI_TO_DPORT(spi);
	}

#ifdef CONFIG_XFRM
xfrm_check_exit:
#endif
#endif

	/*
	 * Read the IP address from the iphdr, and set the src/dst ports to 0.
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
		cm = sfe_ipv4_find_connection_match_rcu(si, dev, l2_info, IPPROTO_ESP, src_ip, sport, dest_ip, dport);
	}
#else
	cm = sfe_ipv4_find_connection_match_rcu(si, dev, l2_info, IPPROTO_ESP, src_ip, sport, dest_ip, dport);
#endif
	if (unlikely(!cm)) {
		rcu_read_unlock();
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_ESP_NO_CONNECTION);
		DEBUG_TRACE("no connection found for esp packet\n");
		return 0;
	}

	/*
	 * Source interface validate.
	 */
	if (unlikely((cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_SRC_INTERFACE_CHECK) && (cm->match_dev != dev))) {
		struct sfe_ipv4_connection *c = cm->connection;
		int ret;

		spin_lock_bh(&si->lock);
		ret = sfe_ipv4_remove_connection(si, c);
		spin_unlock_bh(&si->lock);

		if (ret) {
			sfe_ipv4_flush_connection(si, c, SFE_RULE_SYNC_REASON_FLUSH);
		}
		rcu_read_unlock();
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INVALID_SRC_IFACE);
		DEBUG_TRACE("flush on wrong source interface check failure\n");
		return 0;
	}

	passthrough = cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PASSTHROUGH;
	bridge_flow = !!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_BRIDGE_FLOW);

	/*
	 * If our packet has been marked as "sync on find" we can't actually
	 * forward it in the fast path, but now that we've found an associated
	 * connection we need sync its status before exception it to slow path unless
	 * it is passthrough (packets not directed to DUT) packet.
	 * TODO: revisit to ensure that pass through traffic is not bypassing firewall for fragmented cases
	 */
	if (unlikely(sync_on_find) && !passthrough) {
		sfe_ipv4_sync_status(si, cm->connection, SFE_RULE_SYNC_REASON_STATS);
		rcu_read_unlock();
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_ESP_IP_OPTIONS_OR_INITIAL_FRAGMENT);
		DEBUG_TRACE("%px: sfe: sync on find\n", cm);
		return 0;
	}

	/*
	 * Do we expect an ingress VLAN tag for this flow?
	 */
#ifdef SFE_BRIDGE_VLAN_FILTERING_ENABLE
	ingress_flags = cm->vlan_filter_rule.ingress_flags;
#endif
	if (unlikely(!sfe_vlan_validate_ingress_tag(skb, cm->ingress_vlan_hdr_cnt, cm->ingress_vlan_hdr, l2_info, ingress_flags))) {
		if (!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_BRIDGE_VLAN_PASSTHROUGH)) {
			rcu_read_unlock();
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INGRESS_VLAN_TAG_MISMATCH);
			DEBUG_TRACE("VLAN tag mismatch. skb=%px\n", skb);
			return 0;
		}

		vlan_passthrough = true;
		this_cpu_inc(si->stats_pcpu->bridge_vlan_passthrough_forwarded64);
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

	/*
	 * Check if skb was cloned. If it was, unclone it.
	 */
	if (unlikely(skb_cloned(skb))) {
		DEBUG_TRACE("%px: skb is a cloned skb\n", skb);

		if (unlikely(skb_shared(skb)) || unlikely(skb_unclone(skb, GFP_ATOMIC))) {
			rcu_read_unlock();
			DEBUG_WARN("Failed to unclone the cloned skb\n");
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_UNCLONE_FAILED);
			return 0;
		}

		/*
		 * Update the iphdr pointer with the uncloned skb's data area.
		 */
		iph = (struct iphdr *)skb->data;
	}

	/*
	 * For PPPoE packets, match server and session id
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
				ether_addr_equal((u8*)cm->pppoe_remote_mac, eth->h_source);

		if (unlikely(!pppoe_match)) {
			DEBUG_TRACE("%px: PPPoE session ID %d and %d or MAC %pM and %pM did not match\n",
					skb, cm->pppoe_session_id, sfe_l2_pppoe_session_id_get(l2_info),
					cm->pppoe_remote_mac, eth->h_source);
			rcu_read_unlock();
			sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_INVALID_PPPOE_SESSION);
			return 0;
		}

		skb->protocol = htons(l2_info->protocol);
		this_cpu_inc(si->stats_pcpu->pppoe_decap_packets_forwarded64);
	} else if (unlikely(sfe_l2_parse_flag_check(l2_info, SFE_L2_PARSE_FLAGS_PPPOE_INGRESS))) {

		/*
		 * If packet contains PPPoE header but CME doesn't contain PPPoE flag yet we are exceptioning
		 * the packet to linux unless its bridge flow
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

	/*
	 * Enable HW csum if rx checksum is verified and xmit interface is CSUM offload capable.
	 */
	hw_csum = !!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_CSUM_OFFLOAD) && (skb->ip_summed == CHECKSUM_UNNECESSARY);

	/*
	 * Do we have to perform translations of the source address?
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_XLATE_SRC)) {
		iph->saddr = cm->xlate_src_ip;
	}

	/*
	 * Do we have to perform translations of the destination address?
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_XLATE_DEST)) {
		iph->daddr = cm->xlate_dest_ip;
	}

	/*
	 * proto decap packet.
	 *	Invoke the inet_protocol handler for delivery of the packet.
	 */
	ipprot = rcu_dereference(cm->proto);
	if (likely(ipprot)) {
		skb_reset_network_header(skb);
		skb_pull(skb, ihl);
		skb_reset_transport_header(skb);
		xmit_dev = cm->xmit_dev;
		skb->dev = xmit_dev;

		mem_debug_update_skb(skb);
		ret = ipprot->handler(skb);
		if (ret) {
			rcu_read_unlock();
			this_cpu_inc(si->stats_pcpu->packets_not_forwarded64);
			DEBUG_TRACE("ESP handler returned error %u\n", ret);
			return 0;
		}

		/*
		 * Update traffic stats.
		 */
		atomic_inc(&cm->rx_packet_count);
		atomic_add(len, &cm->rx_byte_count);

		rcu_read_unlock();
		this_cpu_inc(si->stats_pcpu->packets_forwarded64);
		return 1;
	}

	/*
	 * esp passthrough / ip local out scenarios.
	 */
	/*
	 * If our packet is larger than the MTU of the transmit interface then
	 * we can't forward it easily.
	 */
	if (unlikely(len > cm->xmit_dev_mtu)) {
		sfe_ipv4_sync_status(si, cm->connection, SFE_RULE_SYNC_REASON_STATS);
		rcu_read_unlock();
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_ESP_NEEDS_FRAGMENTATION);
		DEBUG_TRACE("%px: sfe: larger than MTU\n", cm);
		return 0;
	}

	/*
	 * Check if skb has enough headroom to write L2 headers
	 */
	if (unlikely(skb_headroom(skb) < cm->l2_hdr_size)) {
		rcu_read_unlock();
		DEBUG_TRACE("%px: Not enough headroom: %u\n", skb, skb_headroom(skb));
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_NO_HEADROOM);
		return 0;
	}

	/*
	 * need to ensure that TTL is >=2.
	 */
	ttl = iph->ttl;
	if (!bridge_flow && (ttl < 2) && passthrough) {
		sfe_ipv4_sync_status(si, cm->connection, SFE_RULE_SYNC_REASON_STATS);
		rcu_read_unlock();

		DEBUG_TRACE("%px: sfe: TTL too low\n", skb);
		sfe_ipv4_exception_stats_inc(si, SFE_IPV4_EXCEPTION_EVENT_ESP_SMALL_TTL);
		return 0;
	}

	/*
	 * Set SKB packet type to PACKET_HOST
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PACKET_HOST)) {
		skb->pkt_type = PACKET_HOST;
	}

	/*
	 * decrement TTL by 1.
	 */
	iph->ttl = (ttl - (u8)(!bridge_flow && !tun_outer));

	/*
	 * Update DSCP
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_DSCP_REMARK)) {
		iph->tos = (iph->tos & SFE_IPV4_DSCP_MASK) | cm->dscp;
	}

	/*
	 * Replace the IP checksum.
	 */
	if (likely(hw_csum)) {
		skb->ip_summed = CHECKSUM_PARTIAL;
	} else {
		iph->check = sfe_ipv4_gen_ip_csum(iph);
	}

	/*
	 * Update traffic stats.
	 */
	atomic_inc(&cm->rx_packet_count);
	atomic_add(len, &cm->rx_byte_count);

	xmit_dev = cm->xmit_dev;
	skb->dev = xmit_dev;

	if (unlikely(vlan_passthrough)) {
		struct ethhdr *eth = eth_hdr(skb);
		__skb_push(skb, l2_info->vlan_hdr_cnt *VLAN_HLEN);
		skb_reset_network_header(skb);
		skb->protocol = eth->h_proto;
	}

	/*
	 * For PPPoE flows, add PPPoE header before L2 header is added.
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PPPOE_ENCAP)) {
		sfe_pppoe_add_header(skb, cm->pppoe_session_id, PPP_IP);
		this_cpu_inc(si->stats_pcpu->pppoe_encap_packets_forwarded64);
	}

	/*
	 * Check to see if we need to add VLAN tags
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_INSERT_EGRESS_VLAN_TAG)) {
		sfe_vlan_add_tag(skb, cm->egress_vlan_hdr_cnt, cm->egress_vlan_hdr);
	}

	/*
	 * write the layer - 2 header.
	 */
	if (likely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_WRITE_L2_HDR)) {
		if (unlikely(!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_WRITE_FAST_ETH_HDR))) {
			dev_hard_header(skb, xmit_dev, ntohs(skb->protocol),
					cm->xmit_dest_mac, cm->xmit_src_mac, len);
		} else {
			/*
			 * For the simple case we write this really fast.
			 */
			struct ethhdr *eth = (struct ethhdr *)__skb_push(skb, ETH_HLEN);
			eth->h_proto = skb->protocol;
			ether_addr_copy((u8 *)eth->h_dest, (u8 *)cm->xmit_dest_mac);
			ether_addr_copy((u8 *)eth->h_source, (u8 *)cm->xmit_src_mac);
		}
	}

	/*
	 * Update priority and int_pri of skb
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PRIORITY_REMARK)) {
		skb->priority = cm->priority;
#if defined(SFE_PPE_QOS_SUPPORTED)
		skb_set_int_pri(skb, cm->int_pri);
#endif
	}

	/*
	 * Mark outgoing packet.
	 */
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_MARK)) {
		skb->mark = cm->mark;
	}

	/*
	 * For the first packets, check if it could got fast xmit.
	 */
	if (unlikely(!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_FAST_XMIT_FLOW_CHECKED)
				&& (cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_FAST_XMIT_DEV_ADMISSION))){
		cm->features = netif_skb_features(skb);
		if (likely(sfe_fast_xmit_check(skb, cm->features))) {
			cm->flags |= SFE_IPV4_CONNECTION_MATCH_FLAG_FAST_XMIT;
		}
		cm->flags |= SFE_IPV4_CONNECTION_MATCH_FLAG_FAST_XMIT_FLOW_CHECKED;
	}

	features = cm->features;
	fast_xmit = !!(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_FAST_XMIT);

	this_cpu_inc(si->stats_pcpu->packets_forwarded64);

#ifdef CONFIG_IPQ_PON
	if (unlikely(cm->flags & SFE_IPV4_CONNECTION_MATCH_FLAG_PPE_VEIP_PON_VP_VALID)) {
		offload_xmit_dev = cm->offload_xmit_dev;

		if (likely(offload_xmit_dev)) {
			offload_ops = rcu_dereference(offload_xmit_dev->offload_ops);
			if (likely(offload_ops && offload_ops->xmit)) {
				mem_debug_update_skb(skb);
				if (likely(offload_ops->xmit(offload_xmit_dev, skb))) {
					this_cpu_inc(si->stats_pcpu->ppe_veip_pon_vp_tx64);
					rcu_read_unlock();
					return 1;
				}
			}
		}
	}
#endif

	rcu_read_unlock();
	prefetch(skb_shinfo(skb));


	/*
	 * We do per packet condition check before we could fast xmit the
	 * packet.
	 */
	mem_debug_update_skb(skb);
	if (likely(fast_xmit && dev_fast_xmit(skb, xmit_dev, features))) {
		this_cpu_inc(si->stats_pcpu->packets_fast_xmited64);
		return 1;
	}

	/*
	 * Mark that this packet has been fast forwarded.
	 */
	skb->fast_forwarded = 1;

	mem_debug_update_skb(skb);
	dev_queue_xmit(skb);
	return 1;
}
