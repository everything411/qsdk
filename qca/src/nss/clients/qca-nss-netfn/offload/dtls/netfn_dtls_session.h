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

#ifndef __NETFN_DTLS_SESSION_H
#define __NETFN_DTLS_SESSION_H

struct netfn_dtls_tun;

/*
 * DTLS Session statistics.
 */
struct netfn_dtls_session_stats {
	uint64_t tx_pkts;		/* Packet enqueued to DMA */
	uint64_t tx_bytes;		/* Bytes enqueued to DMA */
	uint64_t rx_pkts;		/* Packet completed by DMA */
	uint64_t rx_bytes;		/* Byte completed by DMA */

	uint64_t fail_enqueue;		/* DMA transmit failure */
	uint64_t fail_transform;	/* transformation error */
};

/*
 * DTLS Session object.
 */
struct netfn_dtls_session {
	struct netfn_dtls_tun *tun;	/* Tunnel device */

	netfn_tuple_t tuple;		/* session tuple */
	__be16 epoch;			/* DTLS epoch */
	uint32_t flags;			/* Flags passed during allocation */
	struct eip_tr *tr;		/* Transform record allocated by HW */
	struct dentry *dentry;		/* debugfs dentry */
	struct netfn_dtls_session_stats __percpu *stats_pcpu;	/* Session statistics */

};

uint16_t netfn_dtls_session_get_overhead(struct netfn_dtls_session *ses);
void netfn_dtls_session_free(struct netfn_dtls_session *ses);
struct netfn_dtls_session *netfn_dtls_session_alloc(struct netfn_dtls_cfg *cfg, netfn_tuple_t *t, struct netfn_dtls_tun *tun);

#endif /* !__NETFN_DTLS_SESSION_H */
