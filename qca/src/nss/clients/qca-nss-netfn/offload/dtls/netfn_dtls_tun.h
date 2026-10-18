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

#ifndef __NETFN_DTLS_TUN_H
#define __NETFN_DTLS_TUN_H

#define NETFN_DTLS_TUN_MAX_HEADROOM 128           /* Size of the buffer headroom. */
#define NETFN_DTLS_TUN_MAX_TAILROOM 192           /* Size of the buffer tailroom. */

/*
 * DTLS device statistics.
 */
struct netfn_dtls_tun_stats {
	uint64_t session_alloc;		/* Number of session allcated */
	uint64_t session_free;		/* Number of session freed */
	uint64_t encap_switch;		/* Number of Session switch in encap side */
	uint64_t decap_switch;		/* Number of Session switch in Decap side */

	/*
	 * Tx is for encapsulation packets.
	 */
	uint64_t tx_pkts;		/* Encap Packet transmitted */
	uint64_t tx_bytes;		/* Encap Bytes transmitted */
	uint64_t tx_vp_exp;		/* Encap Packet transmitted via VP exception */
	uint64_t tx_host;		/* Encap packet trasmitted via Host */
	uint64_t tx_fail;		/* Encapsulation failure */
	uint64_t tx_fail_expand;	/* Failed to expand SKB with low headroom */
	uint64_t tx_fail_session;	/* session not found failure */

	/*
	 * Rx is for decapsulation packets.
	 */
	uint64_t rx_pkts;		/* Decap Packet received */
	uint64_t rx_bytes;		/* Decap Bytes received */
	uint64_t rx_fail;		/* Decapsulation failure */
	uint64_t rx_fail_linearize;	/* Linearization failed */
	uint64_t rx_fail_session;	/* session not found failure */
	uint64_t rx_fail_ctrl;		/* Not APP-data packet */
};

/*
 * DTLS tunnel state for Encap & Decap session.
 */
struct netfn_dtls_tun_state {
	struct netfn_dtls_session __rcu *active;	/* Current Active session */
	struct netfn_dtls_session __rcu *pending;	/* Pending Session waiting for CCSpec */
};

/*
 * DTLS device object.
 */
struct netfn_dtls_tun {
	struct net_device *dev;	/* Linux netdevice representation for this device */
	struct net_device *vp_dev;	/* VP netdevice associated with tunnel. */
	netfn_dtls_rx_handler_t __rcu cb;	/* Data Callback */
	void __rcu *cb_data;			/* Appdata for callback */

	struct netfn_dtls_tun_state enc;	/* Encap State */
	struct netfn_dtls_tun_state dec;	/* Decap state */
	struct mutex lock;		/* Common lock for list manipulation */
	struct netfn_dtls_tun_stats __percpu *stats_pcpu;	/* Device statistics */
	struct dentry *dentry;		/* Driver debugfs dentry */

	uint8_t user_pvt[];		/* Memory allocated for caller use */
};

#endif /* !__NETFN_DTLS_TUN_H */
