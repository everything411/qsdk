/*
 * netfn_capwap_priv.h
 *	Network function's CAPWAP offload private definitions.
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef __NETFN_CAPWAP_PRIV_H
#define __NETFN_CAPWAP_PRIV_H

#include <asm/bitops.h>

extern struct netfn_capwap global_nc;
extern ulong fwd_cores_mask;
extern ulong capwap_core;
extern int tx_napi_budget;
extern int rx_napi_budget;

#define NETFN_CAPWAP_RX_BUDGET 64
#define NETFN_CAPWAP_TX_BUDGET 32

#define NETFN_CAPWAP_RPS_CORE ((find_first_bit(&fwd_cores_mask, NR_CPUS)) % (NR_CPUS))
#define NETFN_CAPWAP_TUN_STATS_STRLEN 20	/* Stats string length. */
#define NETFN_CAPWAP_TUN_STATS_WIDTH 82		/* Stats string width. */
#define NETFN_CAPWAP_FWD_CORES_MASK 4	/* Default values for core masks */
#define NETFN_CAPWAP_CORE 3		/* Default values for core */
#define NETFN_CAPWAP_FRAG_IDX(frag_id, max) ({ \
		BUILD_BUG_ON_NOT_POWER_OF_2(max); \
		((frag_id) & ((max) - 1)); \
		})

/*
 * netfn_capwap
 *	Driver level instance
 */
struct netfn_capwap {
	struct dentry *dentry;			/* Debug entry for driver. */
	spinlock_t lock;			/* Driver level instance lock. */
};

/*
 * netfn_capwap_frags
 *	Fragmentation/reassembly management object.
 */
struct netfn_capwap_frags {
	struct sk_buff_head list;	/* Reassembly fragment queue. */
	int frag_id;			/* Frag-ID, associated with reasm queue */
	uint16_t frag_sz;		/* Current size of skb received. */
	uint16_t tot_sz;		/* Total size of the skb received. */
};

/*
 * netfn_capwap_frags_init()
 *	Init the frags object.
 */
static inline void netfn_capwap_frags_init(struct netfn_capwap_frags *frags)
{
	skb_queue_head_init(&frags->list);
	frags->frag_id = -1;
	frags->frag_sz = frags->tot_sz = 0;
}

/*
 * netfn_capwap_frags_add()
 *	Adds fragment to frag list.
 */
void netfn_capwap_frags_add(struct netfn_capwap_frags *frags, struct sk_buff *skb);

#endif /* __NETFN_CAPWAP_PRIV_H */
