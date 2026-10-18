/*
 * netfn_capwap_enc.h
 *	Network function's CAPWAP offload encapsulation definitions.
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

#ifndef __NETFN_CAPWAP_ENC_H
#define __NETFN_CAPWAP_ENC_H

#define NETFN_CAPWAP_ENC_STATS_STRLEN 20
#define NETFN_CAPWAP_ENC_STATS_WIDTH 82

struct netfn_capwap_tun;

/*
 * netfn_capwap_enc_hdr
 *	Encapsulation header
 */
struct netfn_capwap_enc_hdr {
	union {
		struct in_addr ip4;	/**< Source IPv4 address */
		struct in6_addr ip6;	/**< Source IPv6 address */
	} src_ip;
	union {
		struct in_addr ip4;	/**< Dest IPv4 address */
		struct in6_addr ip6;	/**< Dest IPv6 address */
	} dest_ip;
	__be16 src_port;		/* Source port */
	__be16 dest_port;		/* Destination port */
	__u8 proto_next_hdr;		/* L4 proto. */
	__u8 ip_version;		/* IP version. */
	__u8 ttl;			/* Time to live. */
	__u8 tos;			/* Type of service. */
	__u8 csum_cov;			/* Checksum coverage. */
};

/*
 * netfn_capwap_enc
 *	Encapsulation state
 */
struct netfn_capwap_enc {
	struct net_device *dev;		/* Device pointer for tunnel. */
	struct netfn_capwap_enc_stats stats;	/* Encap tunnel stats. */

	struct netfn_capwap_enc_hdr hdr;	/* Encapsulation header per tunnel. */
	uint32_t features;			/* Features enabled for the tunnel. */

	unsigned int mtu;			/* MTU considered for fragmentation. */
	uint16_t ipv4_id;		/* IPv4 ID for the tunnel. */
	uint16_t frag_id;		/* Fragment ID per ipv4_id. */

	uint16_t outer_hdr_len;		/* Outer header length for the tunnel. */
	uint8_t snap_hdr[NETFN_CAPWAP_SNAP_HDR_LEN];
					/* Custom snap header. */
	uint8_t bssid;			/* Basic service set ID. */
	uint8_t flags;			/* Check if VLAN or PPPOE is enabled. */
};
/*
 * netfn_capwap_enc_stats_read()
 *	Reads encap stats.
 */
void netfn_capwap_enc_stats_read(struct netfn_capwap_enc *enc, struct netfn_capwap_tun_stats *stats);

/*
 * netfn_capwap_enc_init()
 *	Encapsulation initialization.
 */
bool netfn_capwap_enc_init(struct netfn_capwap_enc *enc, struct netfn_capwap_enc_cfg *cfg, struct netfn_tuple *tuple, struct netfn_capwap_tun *nct);

/*
 * netfn_capwap_enc()
 *	Encapsulates the packets/s.
 */
void netfn_capwap_enc(struct netfn_capwap_enc *enc, struct sk_buff *skb, struct sk_buff_head *q_frag);

/*
 * netfn_capwap_enc_mtu_update()
 *	Update encap mtu
 */
void netfn_capwap_enc_mtu_update(struct netfn_capwap_enc *enc, unsigned int mtu);

/*
 * netfn_capwap_enc_get_err_stats()
 *	Accumulates encapsulation error stats and return sum
 */
uint64_t netfn_capwap_enc_get_err_stats(struct netfn_capwap_enc *enc);

/*
 * netfn_capwap_enc_get_drop_stats()
 *	Accumulates encapsulation drop stats and return sum
 */
uint64_t netfn_capwap_enc_get_drop_stats(struct netfn_capwap_enc *enc);

#endif /* __NETFN_CAPWAP_ENC_H */
