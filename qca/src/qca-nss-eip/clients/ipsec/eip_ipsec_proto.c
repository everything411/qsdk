/*
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/slab.h>
#include <linux/if.h>
#include <linux/rculist.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <net/addrconf.h>
#include <net/esp.h>
#include <net/protocol.h>
#include <net/xfrm.h>
#include <net/udp.h>

#include "eip_ipsec_priv.h"

/*
 * Original ESP ptotocol handlers
 */
static const struct net_protocol *linux_esp_handler;
static const struct inet6_protocol *linux_esp6_handler;

/*
 * eip_ipsec_proto_dec_err()
 *	Decapsulation completion callback.
 */
void eip_ipsec_proto_dec_err(void *app_data, eip_req_t req, int err)
{
	struct eip_ipsec_sa *sa = eip_ipsec_sa_ref_unless_zero((struct eip_ipsec_sa *)app_data);
	struct sk_buff *skb = eip_req2skb(req);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	struct eip_ipsec_dev *eid;

	if (unlikely(!sa)) {
		pr_debug("%px: Failed to take reference on SA\n", sa);
		consume_skb(skb);
		return;
	}

	eid = sa->eid;
	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	dev_stats->rx_fail++;

	/*
	 * Update SA statistics.
	 */
	sa_stats = this_cpu_ptr(sa->stats_pcpu);
	sa_stats->rx_pkts++;
	sa_stats->rx_bytes += skb->len;
	sa_stats->fail_transform++;

	eip_ipsec_sa_deref(sa);
	skb_dump(KERN_DEBUG, skb, false);
	consume_skb(skb);
}

/*
 * eip_ipsec_proto_dec_done()
 *	Decapsulation completion callback.
 */
void eip_ipsec_proto_dec_done(void *app_data, eip_req_t req)
{
	struct eip_ipsec_sa *sa = eip_ipsec_sa_ref_unless_zero((struct eip_ipsec_sa *)app_data);
	struct sk_buff *skb = eip_req2skb(req);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	struct eip_ipsec_dev *eid;

	if (unlikely(!sa)) {
		pr_debug("%px: Failed to take reference on SA\n", sa);
		consume_skb(skb);
		return;
	}

	eid = sa->eid;
	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	sa_stats = this_cpu_ptr(sa->stats_pcpu);

	/*
	 * Update SA statistics.
	 */
	sa_stats->rx_pkts++;
	sa_stats->rx_bytes += skb->len;

	skb_scrub_packet(skb, false);

	/*
	 * Linux requires sp in SKB when xfrm is enabled.
	 */
	if (sa->xs) {
		struct xfrm_state *x = sa->xs;
		struct sec_path *sp;

		sp = secpath_set(skb);
		if(!sp) {
			sa_stats->fail_sp_alloc++;
			goto drop;
		}

		/*
		 * TODO: Add API in linux xfrm_state_try_hold() ?
		 * This is needed to solve race condition with final ref_put() by Linux.
		 */
		if (unlikely(!refcount_inc_not_zero(&x->refcnt))) {
			goto drop;
		}

		sp->xvec[sp->len++] = x;
	}

	/*
	 * Reset General SKB fields for further processing.
	 */
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb->dev = sa->ndev;
	skb->skb_iif = sa->ndev->ifindex;
	skb->ip_summed = CHECKSUM_NONE;

	if (ip_hdr(skb)->version == IPVERSION) {
		skb->protocol = htons(ETH_P_IP);
		skb_set_transport_header(skb, sizeof(struct iphdr));
	} else {
		skb->protocol = htons(ETH_P_IPV6);
		skb_set_transport_header(skb, sizeof(struct ipv6hdr));
	}

	/*
	 * Update device statistics.
	 */
	dev_stats->rx_pkts++;
	dev_stats->rx_bytes += skb->len;

	netif_receive_skb(skb);
	eip_ipsec_sa_deref(sa);
	return;

drop:
	dev_stats->rx_fail++;
	eip_ipsec_sa_deref(sa);
	consume_skb(skb);
	return;
}

/*
 * eip_ipsec_proto_esp_rx()
 *	ESP Protocol handler for IPsec encapsulated packets.
 */
static inline int eip_ipsec_proto_esp_rx(struct eip_ipsec_sa *sa, struct sk_buff *skb)
{
	struct eip_ipsec_dev *eid = netdev_priv(sa->ndev);
	struct eip_ipsec_dev_stats *dev_stats;
	struct eip_ipsec_sa_stats *sa_stats;
	unsigned int len;
	int error;

	sa_stats = this_cpu_ptr(sa->stats_pcpu);
	dev_stats = this_cpu_ptr(eid->stats_pcpu);
	dev_stats->rx_host++;
	len = skb->len;

	/*
	 * skb->data points to ESP or UDP header. So, We need to push ip header.
	 */
	skb_push(skb, skb->data - skb_network_header(skb));

	error = eip_tr_ipsec_dec(sa->tr, skb);
	if (unlikely(error < 0)) {
		/*
		 * TODO: We need to reschedule packet during congestion.
		 */
		sa_stats->fail_enqueue++;
		goto fail;
	}

	/*
	 * Update SA statistics.
	 */
	sa_stats->tx_pkts++;
	sa_stats->tx_bytes += len;

	return 0;

fail:
	dev_stats->rx_fail++;
	consume_skb(skb);
	return 0;
}

/*
 * eip_ipsec_proto_esp4_rx()
 *	ESP Protocol handler for IPv4 IPsec encapsulated packets.
 */
static int eip_ipsec_proto_esp4_rx(struct sk_buff *skb)
{
	struct eip_ipsec_sa *sa;
	int ret;

	/*
	 * Lookup SA using <ESP SPI, IP Destination address>.
	 * We can't use skb->dev because ECM is only pushing rule for 3 tuple (Single rule for multiple decap SAs).
	 */
	sa = eip_ipsec_sa_ref_get_decap_v4(&ip_hdr(skb)->daddr, ip_esp_hdr(skb)->spi);
	if (!sa) {
		pr_debug("IPv4 SA not found %pI4n %x\n", ipv6_hdr(skb)->daddr.s6_addr32, ip_esp_hdr(skb)->spi);
		return -EHOSTUNREACH;
	}

	ret = eip_ipsec_proto_esp_rx(sa, skb);

	eip_ipsec_sa_deref(sa);
	return ret;
}

/*
 * eip_ipsec_proto_esp6_rx()
 *	ESP Protocol handler for IPv6 IPsec encapsulated packets.
 */
static int eip_ipsec_proto_esp6_rx(struct sk_buff *skb)
{
	struct eip_ipsec_sa *sa;
	int ret;

	/*
	 * Lookup SA using <ESP SPI, IP Destination address>.
	 * We can't use skb->dev because ECM is only pushing rule for 3 tuple (Single rule for multiple decap SAs).
	 */
	sa = eip_ipsec_sa_ref_get_decap_v6(ipv6_hdr(skb)->daddr.s6_addr32, ip_esp_hdr(skb)->spi);
	if (!sa) {
		pr_debug("IPv6 SA not found %pI6 %x\n", ipv6_hdr(skb)->daddr.s6_addr32, ip_esp_hdr(skb)->spi);
		return -EHOSTUNREACH;
	}

	ret = eip_ipsec_proto_esp_rx(sa, skb);

	eip_ipsec_sa_deref(sa);
	return ret;
}

/*
 * eip_ipsec_proto_udp_rx()
 *	Handle UDP encapsulated IPsec packets.
 *
 * Shell returns the following value:
 * =0 if SKB is consumed.
 * >0 if skb should be passed on to UDP socket.
 * <0 if skb should be resubmitted.
 */
static int eip_ipsec_proto_udp_rx(struct sock *sk, struct sk_buff *skb)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	size_t hdr_len = sizeof(struct udphdr) +  sizeof(struct ip_esp_hdr);
	struct ip_esp_hdr *esph;

	/*
	 * Socket has to be of type UDP_ENCAP_ESPINUDP .
	 */
	BUG_ON(udp_sk(sk)->encap_type != UDP_ENCAP_ESPINUDP);

	/*
	 * NAT-keepalive packet has udphdr & one byte payload (rfc3948).
	 */
	if (skb->len < hdr_len) {
		goto non_esp_pkt;
	}

	/*
	 * In case of non-linear SKB we would like to ensure that
	 * all the required headers are present in the first segment
	 */
	if (skb_is_nonlinear(skb) && (skb_headlen(skb) < hdr_len)) {
		if (skb_linearize(skb)) {
			dev_kfree_skb_any(skb);
			return 0;
		}

		/*
		 * skb_linearize may change header. So, reload all required pointer.
		 */
		skb_reset_transport_header(skb);
		skb_set_network_header(skb, -(int)sizeof(struct iphdr));
	}

	/*
	 * Check if packet has non-ESP marker (rfc3948)
	 */
	esph = (struct ip_esp_hdr *)(skb_transport_header(skb) + sizeof(struct udphdr));
	if (ntohl(esph->spi) == EIP_IPSEC_PROTO_NON_ESP_MARKER) {
		goto non_esp_pkt;
	}

	/*
	 * ESPinUDP, Make transport header to point ESP.
	 */
	skb_set_transport_header(skb, sizeof(struct udphdr));
	return eip_ipsec_proto_esp4_rx(skb);

non_esp_pkt:
	BUG_ON(!drv->sk_encap_rcv);
	return drv->sk_encap_rcv(sk, skb);
}

/*
 * IPv4 ESP handler
 */
static struct net_protocol esp_protocol = {
	.handler = eip_ipsec_proto_esp4_rx,
	.no_policy = 1,
	.netns_ok  = 1,
};

/*
 * IPv6 ESP handler
 */
static struct inet6_protocol esp6_protocol = {
	.handler = eip_ipsec_proto_esp6_rx,
	.flags = INET6_PROTO_NOPOLICY,
};

/*
 * eip_ipsec_proto_esp_init()
 *	Register ESP handler with the Linux.
 */
bool eip_ipsec_proto_esp_init(void)
{
	const struct net_protocol *ip_prot = &esp_protocol;

	if (inet_update_protocol(&esp_protocol, IPPROTO_ESP, &linux_esp_handler) < 0) {
		pr_err("Failed to update ESP protocol handler for IPv4\n");
		return false;
	}

	if (inet6_update_protocol(&esp6_protocol, IPPROTO_ESP, &linux_esp6_handler) < 0) {
		pr_err("Failed to update ESP protocol handler for IPv4\n");
		/*
		 * Revert v4 ESP handler to original handler.
		 */
		xchg(&linux_esp6_handler, NULL);
		inet_update_protocol(linux_esp_handler, IPPROTO_ESP, &ip_prot);
		xchg(&linux_esp_handler, NULL);
		return false;
	}

	return true;
}

/*
 * eip_ipsec_proto_udp_sock_override()
 */
bool eip_ipsec_proto_udp_sock_override(struct eip_ipsec_tuple *sa_tuple)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;
	struct udp_sock *up;
	struct sock *sk;

	rcu_read_lock();

	sk = __udp4_lib_lookup(&init_net, htonl(sa_tuple->src_ip[0]),  htons(sa_tuple->sport),
			htonl(sa_tuple->dest_ip[0]), htons(sa_tuple->dport), 0, 0, &udp_table, NULL);
	if (!sk) {
		rcu_read_unlock();
		pr_err("%px: Failed to lookup UDP socket dst(%pI4h) dport(0x%X)\n", drv, sa_tuple->dest_ip, sa_tuple->dport);
		return false;
	}

	up = udp_sk(sk);
	if (up->encap_type != UDP_ENCAP_ESPINUDP) {
		rcu_read_unlock();
		pr_err("%px: Socket type is not UDP_ENCAP_ESPINUDP (%u)\n", up, up->encap_type);
		return false;
	}

	if (!drv->sk) {
		sock_hold(sk);
		drv->sk_encap_rcv = xchg(&up->encap_rcv, eip_ipsec_proto_udp_rx);
		drv->sk = sk;
		pr_debug("%px: Overriden socket encap handler\n", up);
	}

	rcu_read_unlock();

	/*
	 * TODO: Add support for multiple Socket handling.
	 */
	WARN_ON(drv->sk != sk);
	return true;
}

/*
 * eip_ipsec_proto_udp_sock_restore()
 */
void eip_ipsec_proto_udp_sock_restore(void)
{
	struct eip_ipsec_drv *drv = &eip_ipsec_drv_g;

	if (drv->sk) {
		struct udp_sock *up = udp_sk(drv->sk);
		xchg(&up->encap_rcv, drv->sk_encap_rcv);
		sock_put(drv->sk);
		drv->sk_encap_rcv = NULL;
		drv->sk = NULL;
		pr_debug("%px: Released socket encap handler\n", up);
	}
}

/*
 * eip_ipsec_proto_esp_deinit()
 *	De-register ESP handler with the Linux.
 */
void eip_ipsec_proto_esp_deinit(void)
{
	const struct inet6_protocol *ip6_prot;
	const struct net_protocol *ip_prot;

	/*
	 * Revert v4 ESP handler to original handler.
	 */
	inet_update_protocol(linux_esp_handler, IPPROTO_ESP, &ip_prot);
	xchg(&linux_esp_handler, NULL);

	inet6_update_protocol(linux_esp6_handler, IPPROTO_ESP, &ip6_prot);
	xchg(&linux_esp6_handler, NULL);
}
