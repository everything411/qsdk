/*
 * netfn_capwap.c
 *	Network function's CAPWAP offload initialization.
 *
 * Copyright (c) 2023-2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/module.h>
#include <linux/version.h>
#include <linux/sysfs.h>
#include <linux/skbuff.h>
#include <linux/debugfs.h>
#include <linux/bitmap.h>
#include <linux/sysctl.h>

#include <net/protocol.h>

#include "netfn_capwap.h"
#include "netfn_capwap_hdr.h"
#include "netfn_capwap_priv.h"

/*
 * Global capwap object.
 */
struct netfn_capwap global_nc;

static bool override_mac = false;
module_param(override_mac, bool, S_IRUSR);
MODULE_PARM_DESC(override_mac, "Use addr2 & addr3 for smac & dmac respectively");

DEFINE_STATIC_KEY_FALSE(skip_bss_mac);

/*
 * netfn_capwap_frags_add()
 *	Try to add frag to frags list
 *
 *	Only time we fail to insert is if there is if there is a
 *	frag already present with same frag offset.
 */
void netfn_capwap_frags_add(struct netfn_capwap_frags *frags, struct sk_buff *skb)
{
	struct netfn_capwap_hdr_mdata *mdata = NETFN_CAPWAP_CB(skb);
	struct sk_buff_head *q_head = &frags->list;

	frags->frag_sz += skb->len;
	frags->frag_id = mdata->frag_id;

	if (mdata->frag_end) {
		frags->tot_sz = mdata->frag_offset + skb->len;
	}

	/*
	 * Blindly, save the SKB as it arrived in the order.
	 */
	__skb_queue_tail(q_head, skb);
}

/*
 * netfn_capwap_init()
 *	module init
 */
int __init netfn_capwap_init(void)
{
	struct netfn_capwap *nc = &global_nc;

	spin_lock_init(&nc->lock);

	if (override_mac) {
		static_branch_enable(&skip_bss_mac);
	}

	/*
	 * Create a debugfs entry for capwap netfn engine.
	 */
	nc->dentry = debugfs_create_dir("qca-nss-netfn-capwap", NULL);
	if (!nc->dentry) {
		pr_warn("%p, Unable to create debugsfs entry for NETFN Capwap engine\n", nc);
	}

	pr_info("NETFN CAPWAP engine loaded: (%s)\n", NSS_NETFN_BUILD_ID);
	return 0;
}

/*
 * netfn_capwap_exit()
 *	module exit
 */
void __exit netfn_capwap_exit(void)
{
	struct netfn_capwap *nc = &global_nc;

	/*
	 * Remove the debug entry.
	 */
	debugfs_remove_recursive(nc->dentry);
}

module_init(netfn_capwap_init);
module_exit(netfn_capwap_exit);
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("CAPWAP offload engine");
