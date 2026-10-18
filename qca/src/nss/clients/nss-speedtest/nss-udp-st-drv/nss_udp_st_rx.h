/*
 **************************************************************************
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __NSS_UDP_ST_RX_H
#define __NSS_UDP_ST_RX_H

#include <linux/init.h>
#include <linux/netfilter.h>

/*
 * nss_udp_st_rx_free_dummy_vp()
 *      free dummy vp and dummy netdevice
 */
void nss_udp_st_rx_free_dummy_vp(int32_t dummy_vp_num);

/*
 * nss_udp_st_rx_rfs_rule_destroy()
 *      destroy PPE RFS rule entry
 */
void nss_udp_st_rx_rfs_rule_destroy(void);

/*
 * nss_udp_st_rx_dummy_vp_alloc()
 *      alloc and register dummy netdevice
 *      alloc dummy passive vp
 */
int32_t nss_udp_st_rx_dummy_vp_alloc(void);

/*
 * nss_udp_st_rx_rfs_rule_create()
 *      create PPE RFS rule entry
 */
bool nss_udp_st_rx_rfs_rule_create(void);

/*
 * nss_udp_st_rx_receive_skb()
 *	Called from nss-dp inplace of netif_receive_skb for processing of udp_st packets.
 */
void nss_udp_st_rx_receive_skb(struct sk_buff *skb);

/*
 * nss_udp_st_rx_ipv4_pre_routing_hook()
 *      pre-routing hook into netfilter packet monitoring point for IPv4
 */
unsigned int nss_udp_st_rx_ipv4_pre_routing_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);

/*
 * nss_udp_st_rx_ipv6_pre_routing_hook()
 *      pre-routing hook into netfilter packet monitoring point for IPv6
 */
unsigned int nss_udp_st_rx_ipv6_pre_routing_hook(void *priv, struct sk_buff *skb, const struct nf_hook_state *state);

#endif /*NSS_UDP_ST_RX_H*/
