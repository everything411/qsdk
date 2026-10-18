/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 **************************************************************************
 */

/*
 * netfn_flow_cookie.c
 *      Network Flow Cookie Functionality Manager
 */
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <netfn_flow_cookie.h>
#include "netfn_flow_cookie_priv.h"

/*
 * We use the following global context
 * to store and use all the relevant
 * entities as parsed across various
 * files of the Flow Cookie Module.
 *
 */
static struct netfn_flow_cookie_ctx global_ctx = {0};

/*
 * netfn_flow_cookie_ctx_get()
 *	Retrieve the Flow Cookie Global Context.
 */
struct netfn_flow_cookie_ctx *netfn_flow_cookie_ctx_get(void)
{
	return &global_ctx;
}

/*
 * netfn_flow_cookie_exit_module()
 *      Netfn Flow Cookie Functionality Manager exit function
 */
void __exit netfn_flow_cookie_exit_module(void)
{
	pr_info("Unloading the Netfn Flow Cookie Module\n");
}

/*
 * netfn_flow_cookie_init_module()
 *      Netfn Flow Cookie Functionality Manager init function
 */
int __init netfn_flow_cookie_init_module(void)
{
	struct netfn_flow_cookie_ctx *ctx = &global_ctx;
	int ret = 0;

	pr_info("netfn flow cookie module loaded: (%s)\n", NSS_NETFN_BUILD_ID);

	ctx->dentry = debugfs_create_dir("qca-nss-netfn-flow-cookie", NULL);
	if (!ctx->dentry) {
		pr_warn("%p:Failed to create debugfs_entry for flow-cookie module\n", ctx);
	}

	return ret;
}

module_init(netfn_flow_cookie_init_module);
module_exit(netfn_flow_cookie_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Network Flow Cookie Module Functionality manager");
