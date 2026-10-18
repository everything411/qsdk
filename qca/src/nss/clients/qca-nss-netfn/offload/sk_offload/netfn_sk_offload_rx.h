/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * @file netfn_sk_offload_rx.h
 * Netfn Sock Offload Rx processing.
 */

/*
 * NETFN_MAX_SKB_FRAGS - Maximum xdp_frame entries for page pool GRO
 */
#define NETFN_MAX_SKB_FRAGS  (MAX_SKB_FRAGS + 1)

/*
 * netfn_sk_offload_rx_xdp
 *	XDP buff-based page pool GRO state.
 */
struct netfn_sk_offload_rx_xdp {
	struct xdp_buff vec[NETFN_MAX_SKB_FRAGS];
				/* XDP buffer entries for GRO coalescing */
	uint8_t nr_xdp;		/* Total number of xdp buf */
	uint8_t nr_scatters;	/* Total scatters across nr_xdp */
};

/*
 * netfn_sk_offload_rx_action
 *	GRO coalesce check actions
 */
enum netfn_sk_offload_rx_action {
	NETFN_SK_OFFLOAD_RX_GRO_NONE,		/* No action */
	NETFN_SK_OFFLOAD_RX_GRO_SKIP,           /* Skip GRO */
	NETFN_SK_OFFLOAD_RX_GRO_START,          /* Start new GRO chain */
	NETFN_SK_OFFLOAD_RX_GRO_QUEUE,          /* Coalesce packets */
	NETFN_SK_OFFLOAD_RX_GRO_FLUSH,          /* Coalesce packet and Flush chain */
	NETFN_SK_OFFLAOD_RX_GRO_RESTART,        /* Flush the existing GRO chain and start new chain */
};

/*
 * iperf_hdr
 * 	IPerf header data
 */
struct iperf_hdr {
	uint32_t sec;		/* Time in seconds */
	uint32_t usec;		/* Time in micro seconds */
	uint32_t seq;		/* Sequence number */
};

/*
 * netfn_sk_offload_rx_state
 *  GRO state per connection
 */
struct netfn_sk_offload_rx_state {
	/*
	 * TODO: Change to union of netfn_sk_offload_rx_pages & netfn_sk_offload_rx_skb.
	 */
	struct {
		struct sk_buff *skb;		/* First packet in the GRO list */
		uint32_t len;			/* Total packet length */
		uint32_t truesize;		/* Total truesize of all SKBs */
		uint8_t gso_segs;		/* Total number of segments */
	} head_cache;

	union {
		struct sk_buff_head frag_q;		/* GRO using SKB frag list */
		struct netfn_sk_offload_rx_xdp xdp_q;		/* GRO using page pool */
	};

	union {
		struct {
			uint16_t hlen;		/* TCP header length */
			uint32_t flag_word;	/* Flag word of head TCP header */
			uint32_t ack_seq;	/* Ack sequence in head TCP header */
			uint32_t next_seq;	/* Expected next data sequence */
		} tcp;

		struct {
			uint16_t len;		/* UDP length */
			uint32_t next_seq;	/* Payload sequence number if available */
		} udp;
	};
};
/*
 * netfn_sk_offload_rx_init()
 *      Netfn Sock Offload Rx Initialization
 */
void netfn_sk_offload_rx_init(struct netfn_sk_offload_ctx *ctx);

/*
 * netfn_sk_offload_rx()
 *      Netfn Sock Offload Rx Processing.
 */
bool netfn_sk_offload_rx(struct ppe_vp_cb_info *info, void *cb_data);
void __rx_gro_purge(struct netfn_sk_offload_ctx *ctx);
void __rx_except(struct netfn_sk_offload_ctx *ctx, struct sk_buff_head *head);
struct dst_entry *__rx_get_dst(struct netfn_sk_offload_ctx *ctx, struct sk_buff *skb);

#ifdef NETFN_SK_OFFLOAD_HW_GRO
/* Declarations for XDP page pool GRO path */
bool netfn_sk_offload_rx_page(struct ppe_vp_xdp_cb_info *info, void *cb_data);
void __rx_page_tcp(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_xdp_cb_info *info);
void __rx_page_udp(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_xdp_cb_info *info);
#endif
