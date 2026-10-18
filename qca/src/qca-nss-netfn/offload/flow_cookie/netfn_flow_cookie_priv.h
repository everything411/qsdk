/*
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

/*
 * @file netfn_flow_cookie_priv.h
 * Netfn Flow Cookie Functionality Manager header file.
 */

#include "netfn_flow_cookie_db.h"

#ifndef __NETFN_FLOW_COOKIE_H
#define __NETFN_FLOW_COOKIE_H

/*
 * netfn_flow_cookie_ctx
 *	 Netfn flow cookie global context.
 */
struct netfn_flow_cookie_ctx {
	struct dentry *dentry;								/* Debugfs entry */
};

extern struct netfn_flow_cookie_ctx *netfn_flow_cookie_ctx_get(void);

#endif /*__NETFN_FLOW_COOKIE_H */
