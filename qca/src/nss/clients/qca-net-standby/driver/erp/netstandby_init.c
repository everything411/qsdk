/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * netstandby_init.c
 *	Init Handler
 */

#include <linux/kernel.h>
#include <linux/netstandby.h>
#include <linux/module.h>
#include <linux/netlink.h>

#include <net/genetlink.h>
#include <net/sock.h>
#include <netstandby_nl_if.h>
#include "netstandby_main.h"
#include "netstandby_nl_cmn.h"

/*
 * netstandby_module_init()
 *	module init for network standby
 */
static int __init netstandby_module_init(void)
{
	struct netstandby_gbl_ctx *gbl_ctx = &gbl_netstandby_ctx;
	netstandby_nl_cmn_init();
	INIT_DELAYED_WORK(&gbl_ctx->trigger_work, netstandby_trigger_work);
	gbl_ctx->iface_configured = false;

	return 0;
}

/*
 * netstandby_module_exit()
 *	module exit for network standby
 */
static void __exit netstandby_module_exit(void)
{
	netstandby_nl_cmn_exit();
}

module_init(netstandby_module_init);
module_exit(netstandby_module_exit);

MODULE_DESCRIPTION("Network Standby Kernel Module");
MODULE_LICENSE("Dual BSD/GPL");

