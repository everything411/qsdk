/*
 * netfn_tcpst_priv.h
 *	Netfn TCP Speedtest private header
 *
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __NETFN_TCPST_PRIV_H
#define __NETFN_TCPST_PRIV_H

#include <linux/in.h>
#include <linux/in6.h>
#include <linux/uio.h>
#include <linux/if.h>

#include <net/sock.h>
#include <net/protocol.h>

#include <netfn_tcpst.h>
#include <netfn_types.h>
#include <ppe_vp_public.h>

#define NETFN_TCPST_HTTP_RESPONSE_LEN 8192	/* Max response length expected */
#define NETFN_TCPST_TIMER_FWD_BIT 1		/* Indicates if timer is forwarded or not */
#define NETFN_TCPST_TIMER_COMP_BIT 2		/* Indicates if timer has completed or not */

/*
 * Global tcp speedtest object
 */
extern struct netfn_tcpst g_tcpst;

/*
 * netfn_tcpst_stats_ext
 *	Extended debug statistics
 */
struct netfn_tcpst_stats_ext {
	uint64_t vp_rx_pkts;			/* Total VP pkts received */
	uint64_t vp_rx_bytes;			/* Total VP bytes received */
	uint64_t dst_miss_pkts;			/* Total times dst wasn't found */
	uint64_t coalesce;			/* Total pkts coalesced to head */
	uint64_t start_coalesce;		/* Total pkts started coalesced */
	uint64_t fail_coalesce;			/* Total pkts failed to coalesced */
	uint64_t fail_start_coalesce;		/* Total pkts failed to coalesced start */
	uint64_t napi_flush;			/* GRO flushed by Linux Napi complete */
	uint64_t test_rx_pkts;			/* Total test pkts rcvd */
	uint64_t test_rx_bytes;			/* Total test bytes rcvd */
	uint64_t test_tx_pkts;			/* Total test pkts sent */
	uint64_t test_tx_bytes;			/* Total test bytes sent */
};

/*
 * netfn_tcpst_gro_state
 *	GRO state
 */
struct netfn_tcpst_gro_state {
	struct sk_buff *head;			/* head packet holding frag_list */
	struct sk_buff *first_frag;		/* frag_list Head */
	struct sk_buff *last_frag;		/* last packet */
	int protocol;				/* skb->protocol */
	struct {
		uint32_t len;			/* Total packet length */
		uint32_t truesize;		/* Total truesize of all SKBs */
	} head_cache;

	uint16_t frag_cnt;			/* Number of packet in list */
	uint16_t hlen;				/* TCP header length */
	uint32_t flag_word;			/* Flag word of head TCP header */
	uint32_t ack_seq;			/* Ack sequence in head TCP header */
	uint32_t next_seq;			/* Expected next data sequence */
};

/*
 * netfn_tcpst_conn
 *	Per connection object
 */
struct netfn_tcpst_conn {
	struct netfn_tcpst_ctx *ctx;		/* Pointer to context */
	struct net_device *eth_dev;		/* Ethernet device for transmission */
	struct net_device *dst_dev;		/* Destination device for packet RX */
	int dst_dev_ifindex;			/* Cached destination device ifindex */
	struct socket *sock;			/* Socket used for this flow */
	struct hrtimer hrtimer;			/* Speedtest control timer */
	struct work_struct work;		/* Work */
	union {
		struct netfn_tcpst_http http;	/* HTTP configuration */
		struct netfn_tcpst_raw raw;	/* RAW TCP test configuration */
	};

	struct netfn_tcpst_stats stats;		/* Debug statistics per connection */
	struct netfn_tcpst_gro_state gro_state;	/* Connection GRO state */
	struct netfn_tcpst_stats_ext ext_stats;	/* Extended debug statistcs per connection */
	unsigned long timer_state;		/* Maintains state of timer */
	uint64_t test_bytes;			/* Maximum test bytes to be tx/rx */
	uint16_t offset;			/* Offset duration for test */
	uint16_t duration;			/* Duration of the test */
	char *buf;				/* Buffer for tx/rx */
	uint64_t buf_len;			/* Buffer length to configure */
};

/*
 * netfn_tcpst_ctx
 *	Test context info
 */
struct netfn_tcpst_ctx {
	struct netfn_tcpst_conn conn[NR_CPUS];	/* Connections */
	enum netfn_tcpst_state state;		/* State of the test */
	enum netfn_tcpst_test test;		/* Test mode */
	struct workqueue_struct *wq;		/* Workqueue for all workers */
	struct net_device *vp_dev;		/* VP netdev */
	ppe_vp_num_t vp_num;			/* VP number associated with netdev */
	int num_conns;				/* Total connections started */
	atomic_t comp_conn;			/* Number of connections completed */
};

/*
 * netfn_tcpst
 *	Global driver object
 */
struct netfn_tcpst {
	struct netfn_tcpst_ctx __rcu *ctx;	/* Context per test run */
	struct mutex lock;			/* Global lock */
	struct dentry *dentry;			/* Debug entry for tcp speedtest */
};

/*
 * netfn_tcpst_state_set()
 *	Sets the state for context
 */
static inline enum netfn_tcpst_state netfn_tcpst_state_set(struct netfn_tcpst_ctx *ctx, enum netfn_tcpst_state new,
		enum netfn_tcpst_state old)
{
	/*
	 * If the context is already removed we are already aborted
	 */
	return cmpxchg(&ctx->state, old, new);
}

/*
 * netfn_tcpst_state_get()
 *	Gets the state of context
 */
static inline enum netfn_tcpst_state netfn_tcpst_state_get(struct netfn_tcpst_ctx *ctx)
{
	return READ_ONCE(ctx->state);
}

/*
 * netfn_tcpst_update_eth_stats()
 * 	Updates the ethernet statistics
 */
static inline void netfn_tcpst_update_eth_stats(struct net_device *dev, struct netfn_tcpst_stats *stats)
{
	struct rtnl_link_stats64 rtnl_stats;

	dev_get_stats(dev, &rtnl_stats);
	stats->eth_bytes.rcvd = rtnl_stats.rx_bytes - stats->eth_bytes.rcvd;
	stats->eth_bytes.sent = rtnl_stats.tx_bytes - stats->eth_bytes.sent;
}

/*
 * netfn_tcpst_is_http()
 *	Returns true if we running http test
 */
static inline bool netfn_tcpst_is_http(struct netfn_tcpst_ctx *ctx)
{
	return (ctx->test == NETFN_TCPST_TEST_HTTP_UPLOAD || ctx->test == NETFN_TCPST_TEST_HTTP_DOWNLOAD);
}
#endif
