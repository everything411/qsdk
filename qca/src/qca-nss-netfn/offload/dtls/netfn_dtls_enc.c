/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <net/addrconf.h>

#include "netfn_dtls_priv.h"

/*
 * netfn_dtls_enc_err()
 *	Encapsulation error completion callback.
 */
void netfn_dtls_enc_err(void *app_data, eip_req_t req, int err)
{
	struct netfn_dtls_session_stats *ses_stats;
	struct netfn_dtls_session *ses = app_data;
	struct sk_buff *skb = eip_req2skb(req);
	struct netfn_dtls_tun_stats *tun_stats;
	struct netfn_dtls_tun *tun;

	pr_debug("%px: Encapsulation failed with err(%d)\n", skb, err);

	tun = ses->tun;
	tun_stats = this_cpu_ptr(tun->stats_pcpu);
	tun_stats->tx_fail++;

	/*
	 * Update session statistics.
	 */
	ses_stats = this_cpu_ptr(ses->stats_pcpu);
	ses_stats->rx_pkts++;
	ses_stats->rx_bytes += skb->len;
	ses_stats->fail_transform++;

	consume_skb(skb);
}

/*
 * netfn_dtls_enc_done()
 *	Encapsulation completion callback for IPv4/IPv6.
 */
void netfn_dtls_enc_done(void *app_data, eip_req_t req)
{
	struct netfn_dtls_session_stats *ses_stats;
	struct netfn_dtls_session *ses = app_data;
	struct sk_buff *skb = eip_req2skb(req);
	struct netfn_dtls_tun_stats *tun_stats;
	struct netfn_dtls_tun *tun;
	struct net_device *next;

	tun = ses->tun;
	next = tun->vp_dev;
	tun_stats = this_cpu_ptr(tun->stats_pcpu);
	ses_stats = this_cpu_ptr(ses->stats_pcpu);
	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);

	/*
	 * Update session statistics.
	 */
	ses_stats->rx_pkts++;
	ses_stats->rx_bytes += skb->len;

	/*
	 * Reset General SKB fields for further processing.
	 * skb->protocol is already set in Driver based on IPv4/IPv6
	 */
	skb_dst_drop(skb);
	nf_reset_ct(skb);
	skb->skb_iif = tun->dev->ifindex;
	skb->dev = next;

	/*
	 * We are sending it to VP netdevice. Which is controlled path,
	 * and don't require unnecessary overhead of dev_queue_xmit()
	 */
	if (next->netdev_ops->ndo_start_xmit(skb, next) != NETDEV_TX_OK) {
		tun_stats->tx_fail++;
		dev_kfree_skb(skb);
		return;
	}

	tun_stats->tx_pkts++;
	tun_stats->tx_bytes += skb->len;
}

/*
 * netfn_dtls_enc()
 *	Encapsulates plaintext packet.
 */
void netfn_dtls_enc(struct netfn_dtls_session *ses, struct sk_buff *skb)
{
	struct netfn_dtls_tun *tun = ses->tun;
	struct netfn_dtls_session_stats *ses_stats;
	struct netfn_dtls_tun_stats *tun_stats;
	unsigned int len;
	int error;

	tun_stats = this_cpu_ptr(tun->stats_pcpu);
	ses_stats = this_cpu_ptr(ses->stats_pcpu);
	len = skb->len;

	error = eip_tr_dtls_enc(ses->tr, skb);
	if (unlikely(error < 0)) {
		/*
		 * TODO: We need to reschedule packet during congestion.
		 */
		ses_stats->fail_enqueue++;
		goto fail;
	}

	/*
	 * Update statistics.
	 */
	ses_stats->tx_pkts++;
	ses_stats->tx_bytes += len;
	return;
fail:
	tun_stats->tx_fail++;
	consume_skb(skb);
}

/*
 * netfn_dtls_enc_init()
 *	Initialize encapsulation handler.
 */
void netfn_dtls_enc_init(struct netfn_dtls_session *ses, struct eip_tr_info *tr_info)
{
	tr_info->dtls.app_data = ses;
	tr_info->dtls.cb = netfn_dtls_enc_done;
	tr_info->dtls.err_cb = netfn_dtls_enc_err;
}
