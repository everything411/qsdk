/*
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
 */

#ifndef __NETFN_PKT_STEER_PRIV_H
#define __NETFN_PKT_STEER_PRIV_H

#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <netfn_pkt_steer.h>

#define NETFN_PKT_STEER_INC(i, max) \
	({ \
	 (((i) + 1) & ((max) - 1)); \
	 })

#define NETFN_PKT_STEER_SUB(i, val, max) \
	({ \
	 (((i) - (val) + (max)) & ((max) - 1)); \
	 })

#define NETFN_PKT_STEER_AVAIL_COUNT(head, tail, max) \
		NETFN_PKT_STEER_SUB(head, tail, max)

#endif /* __NETFN_PKT_STEER_PRIV_H */
