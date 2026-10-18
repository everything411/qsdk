/*
 * netfn_capwap_tun.h
 *	Network function's CAPWAP offload tunnel defintions.
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
#ifndef __NETFN_CAPWAP_TUN_H
#define __NETFN_CAPWAP_TUN_H

#include "netfn_pkt_steer.h"
#include "netfn_capwap_dec.h"
#include "netfn_capwap_enc.h"

#define NETFN_CAPWAP_TUN_STATS_INC(sptr) ((*(uint64_t *) sptr)++)
#define NETFN_CAPWAP_TUN_STATS_ADD(sptr, value) ({ \
		*((uint64_t *)sptr) += value; \
		})

/*
 * netfn_capwap_tun
 * 	CAPWAP tunnel structure
 */
struct netfn_capwap_tun {
	struct netfn_capwap *nc;		/* Pointer to driver object. */
	struct net_device *dev;			/* Pointer to tunnel dev. */
	struct net_device *next_dev;		/* Pointer to nexthop dev. */
	struct net_device *vp_dev;		/* Pointer to VP dev. */
	uint32_t features;			/* Features enabled for tunnel. */

	struct netfn_flow_cookie_db __rcu *db;	/* Store the DB handle here */

	struct netfn_capwap_pkt_stats stats;	/* Tunnel stats. */
	struct netfn_capwap_enc enc;		/* Tunnel encap object. */
	struct netfn_capwap_dec dec;		/* Tunnel decap object. */
	struct dentry *dentry;			/* Debug entry for the tunnel. */

	struct netfn_pkt_steer tx_steer;	/* Tx data packet steering handle. */
	struct netfn_pkt_steer rx_steer;	/* Rx data packet steering handle. */
	struct netfn_pkt_steer rx_steer_pri;	/* Rx priority packet steering handle. */
	uint32_t flags;				/* Flags for this tunnel. */
	uint16_t id;				/* Tunnel identifier. */
	uint8_t pvt[] __attribute__((aligned (sizeof(uint32_t))));
						/* User private area pointer. */
};

#endif /* __NETFN_CAPWAP_TUN_H */
