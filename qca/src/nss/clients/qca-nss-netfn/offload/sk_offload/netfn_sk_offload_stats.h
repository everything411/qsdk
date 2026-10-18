/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_sk_offload_stats.h
 *      Network Sock Offload Flow Statistics.
 */

#define NETFN_SOCK_OFFLOAD_MAX_STR_LEN 64       /* Maximum print lenght */

struct netfn_sk_offload_ctx;

/*
 * netfn_sk_offload_stats
 *	Debug statistics for all the connections.
 */
struct netfn_sk_offload_stats {
	uint64_t total_conns;			/* Total connections */
	uint64_t ctx_alloc;			/* Context alloc success */
	uint64_t ctx_alloc_fail_nomem;		/* Context alloc fail due to no memory */
	uint64_t ctx_alloc_fail_max_conn; 	/* Context alloc fail due to Max Conns */
	uint64_t ctx_alloc_fail_stats;		/* Context alloc fail due to stats failure */
	uint64_t ctx_alloc_fail_rule;		/* Context alloc fail due to Rule failure */
	uint64_t ctx_alloc_fail_sanity;		/* Context alloc fail due to Sanity failure */
	uint64_t ctx_dealloc;			/* Context dealloc success */
	uint64_t rx_pkts;			/* Rx pkts received */
	uint64_t rx_bytes;			/* Rx bytes received */
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	uint64_t hw_gro_rx_pkts;			/* GRO Rx pkts received */
	uint64_t hw_gro_rx_bytes;			/* GRO Rx bytes received */
#endif
	uint64_t tx_pkts;			/* Tx pkts sent */
	uint64_t tx_bytes;			/* Tx bytes sent */
	uint64_t rx_fail_ctx;			/* Total ctx retrieve failures */
	uint64_t rx_fail_hdr;			/* Insufficient header length failures */
	uint64_t pkts_dropped;                  /* Total packets dropped */
};

/*
 * netfn_sk_offload_stats_ctx
 *	Statistics per connection.
 */
struct netfn_sk_offload_stats_ctx {
	uint64_t rx_pkts;			/* Total pkts rcvd */
	uint64_t rx_bytes;			/* Total bytes rcvd */
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	uint64_t hw_gro_rx_pkts;		/* Total pkts rcvd from HW GRO */
	uint64_t hw_gro_rx_bytes;		/* Total bytes rcvd from HW GRO */
#endif
	uint64_t dst_miss_pkts;			/* Total times dst wasn't found */
	uint64_t tx_pkts;			/* Tx pkts sent */
	uint64_t tx_bytes;			/* Tx bytes sent */
	uint64_t coalesce;                      /* Total pkts coalesced to head */
	uint64_t start_coalesce;                /* Total pkts started coalesced */
	uint64_t fail_coalesce;			/* Total pkts failed to coalesced */
	uint64_t fail_start_coalesce;           /* Total pkts failed to start coalescing */
	uint64_t napi_flush;                    /* GRO flushed by Linux Napi complete */
	uint64_t fail_xdp_frame_convert;	/* xdp_convert_buff_to_frame() failed (tailroom check) */
	uint64_t fail_skb_build;		/* napi_build_skb() failed for page pool SKB */
	uint64_t rx_scatter_pkts;		/* Scatter-gather (multi-buffer) packets received */
	uint64_t rx_scatter_delivered;		/* Scatter-gather packets delivered to Linux */
};

/*
 * Netfn Sock Offload Global stats APIs.
 */
int netfn_sk_offload_stats_init(void);
void netfn_sk_offload_stats_deinit(void);
