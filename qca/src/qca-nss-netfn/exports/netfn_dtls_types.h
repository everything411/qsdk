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

#ifndef __NETFN_DTLS_TYPES_H
#define __NETFN_DTLS_TYPES_H

#include <netfn_types.h>
#include <linux/netdevice.h>

/*
 * netfn_dtls_crypto
 *	DTLS Base meta information for session allocation.
 */
struct netfn_dtls_crypto {
	struct {
		uint8_t *key_data;			/* Pointer to the Key array */
		uint16_t key_len;			/* Length of the key */
	} cipher, auth;

	uint32_t nonce;				/* nonce value for certain cipher algorithm */
	const int8_t *algo_name;		/* AEAD algorithm name for cipher operation */
};

/*
 * netfn_dtls_cfg
 *	DTLS meta information for session allocation.
 */
struct netfn_dtls_cfg {
	struct netfn_dtls_crypto base;		/* Base meta information */
	uint32_t replay_win;			/* replay window */
	uint32_t flags;				/* context flags */
#define NETFN_DTLS_FLAG_ENC      BIT(0)	/* set = Encapsulation, clear = Decapsulation */
#define NETFN_DTLS_FLAG_IPV6     BIT(1)	/* set = IPv6 tuple, clear = IPv4 tuple */
#define NETFN_DTLS_FLAG_UDPLITE  BIT(2)	/* set = UDPlite, clear = UDP encapsulation */
#define NETFN_DTLS_FLAG_CAPWAP   BIT(3)	/* set = Capwap hdr enable, clear = capwap hdr disable */
#define NETFN_DTLS_FLAG_CP_TOS  BIT(4)	/* set = copy IP TOS value from SKB, clear = use default TOS */
#define NETFN_DTLS_FLAG_CP_DF   BIT(5)	/* set = copy IP DF value from SKB, clear = use default DF */
#define NETFN_DTLS_FLAG_UDPLITE_CSUM BIT(6)	/* set = header checksum only, clear = Full checksum */

	__be16 version;				/* DTLS version (0xFFFE/v1.0 or 0xFDFE/v1.2) */
	__be16 epoch;				/* Unique epoch value */
	uint8_t df;				/* Default DF value for outer IP */
	uint8_t tos;				/* Default TOS/DS Field value for outer IP */
	uint8_t hop_limit;			/* Default TTL limit for outer IP */
};

#endif /* __NETFN_DTLS_TYPES_H */
