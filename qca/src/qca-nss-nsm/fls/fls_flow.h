/*
 **************************************************************************
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
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
 **************************************************************************
 */

#include <net/netfilter/nf_conntrack.h>
#include <linux/jiffies.h>

#define FLS_FLOW_STATS_PUSH_PERIOD msecs_to_jiffies(1000)
#define FLS_FLOW_FLAG_BREAK 0x01
#define FLS_FLOW_FLAG_RESET 0x02

struct fls_flow_tm {
	uint32_t src_ip_addr[4];		/* Source Ip Address */
	uint32_t dst_ip_addr[4];		/* Destination Ip Address */
	uint16_t src_port;			/* Source Port */
	uint16_t dst_port;			/* Destination Port */
	uint8_t src_mac_addr[ETH_ALEN];		/* Source Neighbor Mac */
	uint8_t dst_mac_addr[ETH_ALEN];		/* Desination Neighbor Mac */
	uint8_t proto;				/* Protocol */
	uint8_t ip_version;			/* Ip Version */
	uint64_t org_bytes;			/* Original Direction Bytes */
	uint64_t ret_bytes;			/* Return Direction Bytes */
	uint64_t org_pkts;			/* Original Direction Packets */
	uint64_t ret_pkts;			/* Return Direction Packets */
	char src_if[IFNAMSIZ];			/* Buffer containing source interface name */
	char dst_if[IFNAMSIZ];			/* Buffer containing destination interface name */
	uint8_t flags;				/* Flags used for processing by TM APP */
};

struct fls_flow_udp_clf {
	uint32_t src_ip_addr[4];	/* Source Ip Address */
	uint32_t dst_ip_addr[4];	/* Destination Ip Address */
	uint16_t src_port;		/* Source Port */
	uint16_t dst_port;		/* Destination Port */
	uint8_t proto;			/* Protocol */
	uint8_t ip_version;		/* Ip Version */
	uint32_t org_dscp;		/* Original direction DSCP */
	uint32_t ret_dscp;		/* Return direction DSCP */
	uint64_t org_bytes;		/* Original Direction Bytes */
	uint64_t ret_bytes;		/* Return Direction Bytes */
	bool is_src_wiphy;		/* Is source wireless device? */
	bool is_dst_wiphy;		/* Is destination wireless device? */
	uint8_t flags;			/* Flags used for processing by TM APP */
};

extern int udp_clf_enabled;

bool fls_flow_init(void);
void fls_flow_deinit(void);
