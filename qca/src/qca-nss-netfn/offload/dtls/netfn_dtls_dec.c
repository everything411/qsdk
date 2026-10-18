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

#include "netfn_dtls_priv.h"

/*
 * netfn_dtls_dec_err()
 *	Decapsulation completion callback.
 */
void netfn_dtls_dec_err(void *app_data, eip_req_t req, int err)
{
	struct netfn_dtls_session_stats *ses_stats;
	struct netfn_dtls_session *ses = app_data;
	struct sk_buff *skb = eip_req2skb(req);
	struct netfn_dtls_tun_stats *tun_stats;
	struct netfn_dtls_tun *tun;

	tun = ses->tun;
	tun_stats = this_cpu_ptr(tun->stats_pcpu);
	tun_stats->rx_fail++;

	/*
	 * Update session statistics.
	 */
	ses_stats = this_cpu_ptr(ses->stats_pcpu);
	ses_stats->rx_pkts++;
	ses_stats->rx_bytes += skb->len;
	ses_stats->fail_transform++;

	dev_kfree_skb_any(skb);
}

/*
 * netfn_dtls_dec_done()
 *	Decapsulation completion callback.
 */
void netfn_dtls_dec_done(void *app_data, eip_req_t req)
{
	struct netfn_dtls_session_stats *ses_stats;
	struct netfn_dtls_session *ses = app_data;
	struct sk_buff *skb = eip_req2skb(req);
	struct netfn_dtls_tun_stats *tun_stats;
	netfn_dtls_rx_handler_t cb;
	struct netfn_dtls_tun *tun;

	tun = ses->tun;
	tun_stats = this_cpu_ptr(tun->stats_pcpu);
	ses_stats = this_cpu_ptr(ses->stats_pcpu);

	/*
	 * Update statistics.
	 */
	ses_stats->rx_pkts++;
	ses_stats->rx_bytes += skb->len;
	tun_stats->rx_pkts++;
	tun_stats->rx_bytes += skb->len;

	skb_scrub_packet(skb, false);

	/*
	 * Reset General SKB fields for further processing.
	 * Header pointer is already set by Driver.
	 */
	skb->dev = tun->dev;
	skb->skb_iif = tun->dev->ifindex;
	skb->ip_summed = CHECKSUM_NONE;

	rcu_read_lock_bh();

	cb = rcu_dereference_bh(tun->cb);
	if (likely(cb)) {
		cb(skb, rcu_dereference_bh(tun->cb_data));

		rcu_read_unlock_bh();
		return;
	}

	rcu_read_unlock_bh();

	/*
	 * Exception it to Linux/rx_handler.
	 */
	netif_receive_skb(skb);
}

/*
 * netfn_dtls_dec()
 *	Decapsulate DTLS packet. SKB->data should point to IP header.
 */
void netfn_dtls_dec(struct netfn_dtls_session *ses, struct sk_buff *skb)
{
	struct netfn_dtls_session_stats *ses_stats;
	struct netfn_dtls_tun_stats *tun_stats;
	struct netfn_dtls_tun *tun = ses->tun;
	unsigned int len;
	int error;

	ses_stats = this_cpu_ptr(ses->stats_pcpu);
	tun_stats = this_cpu_ptr(tun->stats_pcpu);
	len = skb->len;

	error = eip_tr_dtls_dec(ses->tr, skb);
	if (unlikely(error < 0)) {
		/*
		 * TODO: We need to reschedule packet during congestion.
		 */
		ses_stats->fail_enqueue++;
		tun_stats->rx_fail++;
		dev_kfree_skb_any(skb);
		return;
	}

	/*
	 * Update session statistics.
	 */
	ses_stats->tx_pkts++;
	ses_stats->tx_bytes += len;
}

/*
 * netfn_dtls_dec_init()
 *	Initialize Decapsulation handler.
 */
void netfn_dtls_dec_init(struct netfn_dtls_session *ses, struct eip_tr_info *tr_info)
{
	tr_info->dtls.app_data = ses;
	tr_info->dtls.cb = netfn_dtls_dec_done;
	tr_info->dtls.err_cb = netfn_dtls_dec_err;
}
