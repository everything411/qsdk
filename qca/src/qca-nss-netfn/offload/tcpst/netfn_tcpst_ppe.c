/*
 * netfn_tcpst_ppe.c
 *	Netfn TCP Speedtest PPE acceleration helper
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/module.h>
#include <linux/ip.h>
#include <linux/in6.h>
#include <linux/debugfs.h>
#include <linux/if_pppox.h>
#include <linux/if_vlan.h>
#include <linux/if_arp.h>
#include <net/inet_sock.h>
#include <netfn_flowmgr.h>
#include <ppe_drv.h>

/*
 * netfn_tcpst_ppe_get_macaddr()
 *	Fetch mac address using dst neigbour.
 */
static bool netfn_tcpst_ppe_get_macaddr(struct dst_entry *dst, void *daddr, uint8_t *mac_addr)
{
	struct neighbour *neigh;
	uint8_t nud_state;

	/*
	 * retrieve the neighbour
	 */
	neigh = dst_neigh_lookup(dst, daddr);
	if (!neigh) {
		pr_err("%px: neighbour lookup failed\n",  dst);
		return false;
	}

	read_lock_bh(&neigh->lock);
	nud_state = neigh->nud_state;
	ether_addr_copy(mac_addr, neigh->ha);
	read_unlock_bh(&neigh->lock);
	neigh_release(neigh);

	if ((neigh->nud_state & NUD_VALID) == 0) {
		pr_err("%px: neighbour state is invalid\n", dst);
		return false;
	}

	return true;
}

/*
 * netfn_tcpst_ppe_get_vlan()
 *	Parse and fill VLAN information in create rule.
 */
static struct net_device *netfn_tcpst_ppe_get_vlan(struct net_device *dev, struct netfn_flowmgr_vlan_rule *orig, struct netfn_flowmgr_vlan_rule *reply)
{
	struct net_device *next_dev;

	/*
	 * Check if the next isn't null
	 */
	next_dev = vlan_dev_next_dev(dev);
	if (!next_dev) {
		pr_warn("Cannot find the physical net_device\n");
		return NULL;
	}

	/*
	 * We don't want next dev to be VLAN
	 * Next dev should be ethernet device
	 */
	if (is_vlan_dev(next_dev) || (next_dev->type != ARPHRD_ETHER)) {
		pr_warn("%px: QinQ or non-ethernet VLAN master (%s) is not supported\n", dev,
				next_dev->name);
		return NULL;
	}

	orig->inner.egress_vlan_tag = reply->inner.ingress_vlan_tag = vlan_dev_vlan_id(dev);
	orig->inner.ingress_vlan_tag = reply->inner.egress_vlan_tag = 0xfff;
	orig->outer.egress_vlan_tag = reply->outer.ingress_vlan_tag = 0xfff;
	orig->outer.ingress_vlan_tag = reply->outer.egress_vlan_tag = 0xfff;

	pr_debug("%px: VLAN rule configured with dev (%s) vlan_id(%u)\n", dev, dev->name, vlan_dev_vlan_id(dev));
	return next_dev;
}

/*
 * netfn_tcpst_ppe_get_pppoe()
 *	Parse and fill PPPoE information in create rule
 */
static struct net_device *netfn_tcpst_ppe_get_pppoe(struct net_device *dev, struct netfn_flowmgr_pppoe_rule *orig, struct netfn_flowmgr_pppoe_rule *reply)
{
	struct ppp_channel *channel[1] = {NULL};
	struct net_device *next_dev = NULL;
	int px_proto, err = 0;
	struct pppoe_opt opt;
	int ppp_ch_count;

	if (ppp_is_multilink(dev)) {
		pr_warn("%px: channel is multilink PPP\n", dev);
		return false;
	}

	ppp_ch_count = ppp_hold_channels(dev, channel, 1);
	pr_debug("%px: PPP hold channel ret %d\n", dev, ppp_ch_count);
	if (ppp_ch_count != 1) {
		pr_warn("%px: hold channel for netdevice failed\n", dev);
		return false;
	}

	px_proto = ppp_channel_get_protocol(channel[0]);
	if (px_proto != PX_PROTO_OE) {
		pr_warn("%px: session socket is not of type PX_PROTO_OE\n", dev);
		goto release;
	}

	if (pppoe_channel_addressing_get(channel[0], &opt)) {
		pr_warn("%px: failed to get pppoe options information\n", dev);
		goto release;
	}

	/*
	 * pppoe_channel_addressing_get returns held device.
	 * And we are already holding parent device reference so release it.
	 */
	next_dev = opt.dev;
	dev_put(next_dev);

	/*
	 * Update the PPPoE rule information and set PPPoE valid flag.
	 */
	orig->session_id = reply->session_id = ntohs((uint16_t)opt.pa.sid);
	memcpy(orig->server_mac, opt.pa.remote, ETH_ALEN);
	memcpy(reply->server_mac, opt.pa.remote, ETH_ALEN);

	/*
	 * PPPoE interface can be created on linux bridge, OVS bridge and LAG devices.
	 * TCPST doesn't support these hierarchies.
	 */
	err += (next_dev->priv_flags & (IFF_EBRIDGE | IFF_OPENVSWITCH));
	err += ((next_dev->flags & IFF_MASTER) && (next_dev->priv_flags & IFF_BONDING));
	if (err) {
		pr_warn("%px: PPPoE over Bridge/LAG unsupported, dev: %s next_dev: %s\n",
				dev, dev->name, next_dev->name);
		next_dev = NULL;
		goto release;
	}

release:
	ppp_release_channels(channel, 1);
	pr_debug("%px: Update PPE PPPoE flow rule with session_id = %u, remote_mac = %pM\n",
			dev, orig->session_id, orig->server_mac);

	return next_dev;
}

/*
 * netfn_tcpst_ppe_accel()
 */
struct net_device *netfn_tcpst_ppe_accel(struct sock *sk, struct net_device *vp_dev)
{
	struct netfn_flowmgr_flow_info *orig_info, *reply_info;
	struct netfn_tuple_5tuple *orig_tuple, *reply_tuple;
	struct netfn_flowmgr_create_rule reply = {0};
	struct netfn_flowmgr_create_rule orig = {0};
	struct net_device *ret = NULL;
	netfn_flowmgr_ret_t status;
	struct net_device *out_dev;
	struct inet_sock *inet;
	struct dst_entry *dst;

	orig_tuple = &orig.tuple.tuples.tuple_5;
	reply_tuple = &reply.tuple.tuples.tuple_5;
	orig_info = &orig.flow_info;
	reply_info = &reply.flow_info;

	/*
	 * dst should be already filled during connect()
	 */
	dst = sk_dst_get(sk);
	if (!dst) {
		pr_err("%px: Failed to get dst from socket\n", sk);
		return NULL;
	}

	/*
	 * Fill Orginal tuple.
	 */
	inet = inet_sk(sk);
	if (sk->sk_family == AF_INET) {
		orig_tuple->src_ip.ip4.s_addr = inet->inet_rcv_saddr;
		orig_tuple->dest_ip.ip4.s_addr = inet->inet_daddr;
		orig.tuple.ip_version = IPVERSION;
	} else {
		orig_tuple->src_ip.ip6 = inet->sk.sk_v6_rcv_saddr;
		orig_tuple->dest_ip.ip6 = inet->sk.sk_v6_daddr;
		orig.tuple.ip_version = 6;
	}

	out_dev = dst->dev;
	orig.tuple.tuple_type = NETFN_TUPLE_5TUPLE;
	orig_tuple->l4_src_ident = inet->inet_sport;;
	orig_tuple->l4_dest_ident = inet->inet_dport;
	orig_tuple->protocol = IPPROTO_TCP;

	/*
	 * If PPPoE is present, fill PPPoE specific information in create rule
	 */
	if (out_dev->type == ARPHRD_PPP) {
		reply.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_PPPOE;

		out_dev = netfn_tcpst_ppe_get_pppoe(out_dev,  &orig.rule_info.pppoe_rule, &reply.rule_info.pppoe_rule);
		if (!out_dev) {
			pr_warn("%px: Failed to configure pppoe\n", dst->dev);
			goto release;
		}
	}

	/*
	 * If VLAN is present, fill VLAN specific information in create rule
	 */
	if (is_vlan_dev(out_dev)) {
		orig.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN;
		reply.rule_info.rule_valid_flags |= NETFN_FLOWMGR_VALID_RULE_FLAG_VLAN;

		out_dev = netfn_tcpst_ppe_get_vlan(out_dev, &orig.rule_info.vlan_rule, &reply.rule_info.vlan_rule);
		if (!out_dev) {
			pr_warn("%px: Failed to configure VLAN\n", dst->dev);
			goto release;
		}
	}

	/*
	 * Fill original device info
	 */
	orig_info->out_dev = out_dev;
	orig_info->top_outdev = dst->dev;
	orig_info->in_dev = orig_info->top_indev = vp_dev;
	orig_info->flow_mtu = READ_ONCE(out_dev->mtu);

	memcpy(orig_info->flow_src_mac, out_dev->dev_addr, ETH_ALEN);

	if (!netfn_tcpst_ppe_get_macaddr(dst, &orig_tuple->dest_ip, orig_info->flow_dest_mac)) {
		pr_err("%px: Failed to get neigbour mac address\n", dst);
		goto release;
	}

	/*
	 * Reply is exact reverse of original.
	 */
	memcpy(&reply_tuple->dest_ip, &orig_tuple->src_ip, sizeof(reply_tuple->dest_ip));
	memcpy(&reply_tuple->src_ip, &orig_tuple->dest_ip, sizeof(reply_tuple->src_ip));
	memcpy(reply_info->flow_dest_mac, orig_info->flow_src_mac, ETH_ALEN);
	memcpy(reply_info->flow_src_mac, orig_info->flow_dest_mac, ETH_ALEN);

	reply_tuple->l4_dest_ident = orig_tuple->l4_src_ident;
	reply_tuple->l4_src_ident = orig_tuple->l4_dest_ident;
	reply_tuple->protocol = orig_tuple->protocol;
	reply.tuple.ip_version = orig.tuple.ip_version;
	reply.tuple.tuple_type = orig.tuple.tuple_type;

	/*
	 * Fill device.
	 */
	reply_info->in_dev = out_dev;
	reply_info->top_indev  = dst->dev;
	reply_info->out_dev = reply_info->top_outdev = vp_dev;
	reply_info->flow_mtu = orig_info->flow_mtu;

	orig.flow_flags |= NETFN_FLOWMGR_FLOW_FLAG_VP_FLOW;
	reply.flow_flags |= NETFN_FLOWMGR_FLOW_FLAG_VP_FLOW;

	status = netfn_flowmgr_rule_accel(&orig, &reply, NETFN_FLOWMGR_ACCEL_MODE_PPE);
	if (NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(status) != NETFN_FLOWMGR_RET_SUCCESS) {
		pr_err("%px: Failed to create flow rule status(%x)\n", sk, status);
		goto release;
	}

	/*
	 * Hold device reference as we are returning it to caller.
	 * Deref: netfn_tcpst_ppe_decel()
	 */
	dev_hold(out_dev);
	ret = out_dev;

release:
	dst_release(dst);
	return ret;
}

/*
 * netfn_tcpst_ppe_decel()
 */
void netfn_tcpst_ppe_decel(struct sock *sk, struct net_device *out_dev)
{
	struct netfn_tuple_5tuple *reply_tuple, *orig_tuple;
	struct netfn_flowmgr_destroy_rule reply = {0};
	struct netfn_flowmgr_destroy_rule orig = {0};
	netfn_flowmgr_ret_t status;
	struct inet_sock *inet;

	reply_tuple = &reply.tuple.tuples.tuple_5;
	orig_tuple = &orig.tuple.tuples.tuple_5;

	/*
	 * Fill Orginal tuple.
	 */
	inet = inet_sk(sk);
	if (sk->sk_family == AF_INET) {
		orig_tuple->src_ip.ip4.s_addr = inet->inet_rcv_saddr;
		orig_tuple->dest_ip.ip4.s_addr = inet->inet_daddr;
		orig.tuple.ip_version = IPVERSION;
	} else {
		orig_tuple->src_ip.ip6 = inet->sk.sk_v6_rcv_saddr;
		orig_tuple->dest_ip.ip6 = inet->sk.sk_v6_daddr;
		orig.tuple.ip_version = 6;
	}

	orig.tuple.tuple_type = NETFN_TUPLE_5TUPLE;
	orig_tuple->l4_src_ident = inet->inet_sport;
	orig_tuple->l4_dest_ident = inet->inet_dport;
	orig_tuple->protocol = IPPROTO_TCP;

	/*
	 * Reply is exact reverse of original.
	 */
	memcpy(&reply_tuple->dest_ip, &orig_tuple->src_ip, sizeof(reply_tuple->dest_ip));
	memcpy(&reply_tuple->src_ip, &orig_tuple->dest_ip, sizeof(reply_tuple->src_ip));

	reply_tuple->l4_dest_ident = orig_tuple->l4_src_ident;
	reply_tuple->l4_src_ident = orig_tuple->l4_dest_ident;
	reply_tuple->protocol = orig_tuple->protocol;
	reply.tuple.ip_version = orig.tuple.ip_version;
	reply.tuple.tuple_type = NETFN_TUPLE_5TUPLE;

	status = netfn_flowmgr_rule_decel(&orig, &reply, NETFN_FLOWMGR_ACCEL_MODE_PPE);
	if (NETFN_FLOWMGR_GET_NETFN_ERROR_CODE(status) != NETFN_FLOWMGR_RET_SUCCESS) {
		pr_err("%px: Failed to destroy flow rule status(%x)\n", sk, status);
	}

	/*
	 * Ref: netfn_tcpst_ppe_accel()
	 */
	dev_hold(out_dev);
}
