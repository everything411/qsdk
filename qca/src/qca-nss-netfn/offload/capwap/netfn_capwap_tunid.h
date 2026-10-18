/*
 * netfn_capwap_tunid.h
 *	Network function's CAPWAP offload tunnel id defintions.
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
#ifndef __NETFN_CAPWAP_TUNID_H
#define __NETFN_CAPWAP_TUNID_H

#define NETFN_CAPWAP_MAX_IDS 32

/*
 * netfn_capwap_tunid
 *	Tunnel id.
 */
struct netfn_capwap_tunid {
	struct net_device *dev;					/* Pointer to dummy dev. */
	struct net_device *tunnels[NETFN_CAPWAP_MAX_IDS];	/* Tunnel dev array. */
	struct netfn_capwap_pkt_stats stats;			/* Packet stats for tunid dev. */
	DECLARE_BITMAP(map, NETFN_CAPWAP_MAX_IDS); 		/* Mapping of active/deactive tunnels. */
	spinlock_t lock;					/* Tunnel ID lock. */
	uint8_t pvt[] __attribute__((aligned (sizeof(uint32_t)))); /* User private area. */
};
#endif /* __NETFN_CAPWAP_TUNID_H */
