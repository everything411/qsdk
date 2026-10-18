/*
 * netfn_capwap_dec.h
 *	Network function's CAPWAP offload decapsulation defintions.
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

#ifndef __NETFN_CAPWAP_DEC_H
#define __NETFN_CAPWAP_DEC_H

#define NETFN_CAPWAP_DEC_STATS_STRLEN 20
#define NETFN_CAPWAP_DEC_STATS_WIDTH 82

#define NETFN_CAPWAP_DEC_PHDR_TYPE_MASK 0xFFFF

struct netfn_capwap_tun;

/*
 * netfn_capwap_dec_ctx
 *	Reassembly context
 */
struct netfn_capwap_dec_ctx {
	struct sk_buff *reasm_cache;		/* Fast reassembly buffer. */
	struct netfn_capwap_frags reasm_table[NETFN_CAPWAP_TUN_MAX_REASM_WIN];
						/* Slow reassembly frags queues. */
	struct netfn_capwap_dec *dec;		/* Back pointer to parent. */
};

/*
 * netfn_capwap_dec
 *	Decapsulation state
 */
struct netfn_capwap_dec {
	struct netfn_capwap_dec_ctx __rcu *ctx;	/* Decap reassembly context. */
	struct netfn_capwap_dec_stats stats;	/* Decap tunnel stats. */

	uint32_t max_frags;			/* Maximum number of frags expected. */
	uint32_t max_payload_sz;		/* Maximum size of payload buffer. */
	uint32_t features;			/* Features enabled for the tunnel. */
	uint32_t id;				/* Tunnel identifier. */
};

/*
 * netfn_capwap_dec_stats_read()
 *	Reads decap stats.
 */
void netfn_capwap_dec_stats_read(struct netfn_capwap_dec *dec, struct netfn_capwap_tun_stats *stats);

/*
 * netfn_capwap_dec_rx_skbs()
 *	Decapsulate skbs.
 */
bool netfn_capwap_dec_rx_skbs(struct netfn_capwap_dec *dec, struct sk_buff_head *q_head);

/*
 * netfn_capwap_dec_init()
 *	Initializes the decap object.
 */
bool netfn_capwap_dec_init(struct netfn_capwap_dec *dec, struct netfn_capwap_dec_cfg *cfg, struct netfn_capwap_tun *nct);

/*
 * netfn_capwap_dec_get_err_stats()
 *	Accumulates decapsulation error stats and return sum
 */
uint64_t netfn_capwap_dec_get_err_stats(struct netfn_capwap_dec *dec);

/*
 * netfn_capwap_dec_get_drop_stats()
 *	Accumulates decapsulation drop stats and return sum
 */
uint64_t netfn_capwap_dec_get_drop_stats(struct netfn_capwap_dec *dec);
#endif /* __NETFN_CAPWAP_DEC_H */
