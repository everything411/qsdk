/*
 *****************************************************************************
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ****************************************************************************
 */

/*
 * netfn_flow_cookie_hash.c
 *     Hash Functionality File for Flow Cookie DB Module.
 */

#include <linux/in.h>
#include <linux/types.h>
#include <linux/limits.h>
#include <linux/hash.h>
#include <linux/ip.h>
#include <netfn_types.h>
#include <netfn_flow_cookie.h>
#include "netfn_flow_cookie_hash.h"
#include "netfn_flow_cookie_db.h"

/*
 * netfn_flow_cookie_db_hash()
 *      Calculates Hash Index from the 5 tuple info.
 */
uint32_t netfn_flow_cookie_db_hash(struct netfn_flow_cookie_db *db, struct netfn_tuple *t)
{

	switch (t->tuple_type) {
	case NETFN_TUPLE_5TUPLE: {
		struct netfn_tuple_5tuple *t5 = &t->tuples.tuple_5;
		uint32_t hash = 0;

		if (t->ip_version == IPVERSION) {
			uint32_t dst_ip = t5->dest_ip.ip4.s_addr;
			uint32_t src_ip = t5->src_ip.ip4.s_addr;

			hash ^= (ntohl(src_ip ^ dst_ip));
		} else {
			uint32_t *dst_ip = t5->dest_ip.ip6.s6_addr32;
			uint32_t *src_ip = t5->src_ip.ip6.s6_addr32;

			hash ^= (ntohl(src_ip[0] ^ dst_ip[0]));
			hash ^= (ntohl(src_ip[1] ^ dst_ip[1]));
			hash ^= (ntohl(src_ip[2] ^ dst_ip[2]));
			hash ^= (ntohl(src_ip[3] ^ dst_ip[3]));
		}

		/*
		 * hash_32 needs a 32-bit key & num bits
		 */
		hash ^= (t5->protocol ^ (ntohs(t5->l4_src_ident ^ t5->l4_dest_ident)));
		hash = hash_32(hash, db->max_bits);

		pr_debug("%p: Hash computed - %x\n", t, hash);
		return hash;

	}

	default:
		pr_warn("%p: Unsupported tuple format(%d)\n", t, t->tuple_type);
		return U32_MAX;
	}

	return U32_MAX;
}
