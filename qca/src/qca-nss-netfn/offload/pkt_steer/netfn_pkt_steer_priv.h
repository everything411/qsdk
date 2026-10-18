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

#ifndef __NETFN_PKT_STEER_PRIV_H
#define __NETFN_PKT_STEER_PRIV_H

#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <netfn_pkt_steer.h>

#define NETFN_PKT_STEER_INC(i) \
	({ \
	 BUILD_BUG_ON_NOT_POWER_OF_2(NETFN_PKT_STEER_QUEUE_DEPTH); \
	 (((i) + 1) & ((NETFN_PKT_STEER_QUEUE_DEPTH) - 1)); \
	 })

#define NETFN_PKT_STEER_SUB(i, val, max) \
	({ \
	 BUILD_BUG_ON_NOT_POWER_OF_2(max); \
	 (((i) - (val) + (max)) & ((max) - 1)); \
	 })

#define NETFN_PKT_STEER_AVAIL_COUNT(head, tail) \
		NETFN_PKT_STEER_SUB(head, tail, NETFN_PKT_STEER_QUEUE_DEPTH)

/*
 * netfn_pkt_steer_fifo
 * 	skb queue for napi operations
 */
struct netfn_pkt_steer_fifo {
	struct sk_buff *skbs[NETFN_PKT_STEER_QUEUE_DEPTH];
						/* Packet queue */
	atomic64_t prod;			/* Produce index */
	atomic64_t cons;			/* Consumer index */
	struct netfn_pkt_steer_stats stats;	/* statistics */
};

/*
 * netfn_pkt_steer_pcpu
 * 	Per Core NAPI structure.
 */
struct netfn_pkt_steer_pcpu {
	struct netfn_pkt_steer *ps;		/* Cache Parent object to be passed in cb */
	netfn_pkt_steer_recv_t cb;		/* Cache calback */

	struct napi_struct napi;		/* NAPI object */
	call_single_data_t csd;			/* IPI descriptor */
	atomic_t ipi_queued;			/* Previous IPI is not yet completed */
	atomic_t ipi_masked;			/* IPI is masked by consumer */
	atomic_t queued;			/* Packets in CPU's Queue */

	struct netfn_pkt_steer_fifo fifo[NR_CPUS];	/* Cross CPU SKB Queue */
};

#endif /* __NETFN_PKT_STEER_PRIV_H */
