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

#include <linux/if.h>
#include <linux/kernel.h>
#include <linux/netstandby.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/version.h>
#include <net/genetlink.h>

#include <netstandby_nl_if.h>
#include "netstandby_nl_cmn.h"
#include "netstandby_nl.h"
#include "netstandby_main.h"

/*
 * Netstandby NL family definition
 */
struct netstandby_family {
	uint8_t *name;		/* name of the family */
	netstandby_nl_cmn_fn_t entry;	/* entry function of the family */
	netstandby_nl_cmn_fn_t exit;	/* exit function of the family */
	bool valid;		/* valid or invalid */
};

/*
 * Family handler table
 */
static struct netstandby_family family_handlers[] = {
	{
		/*
		 * nl_netstandby
		 */
		.name = NETSTANDBY_NL_FAMILY,		/* Power Save FAMILY */
		.entry = NETSTANDBY_NL_INIT,		/* Init */
		.exit = NETSTANDBY_NL_EXIT,		/* exit */
		.valid = CONFIG_NETSTANDBY,		/* 1 or 0 */
	},
};

#define NETSTANDBY_NL_FAMILY_HANDLER_SZ ARRAY_SIZE(family_handlers)

/*
 * netstandby_nl_cmn_get_data()
 *	Returns start of payload data
 */
void *netstandby_nl_cmn_get_data(struct sk_buff *skb)
{
	return genlmsg_data(NLMSG_DATA(skb->data));
}

/*
 * netstandby_nl_cmn_copy_msg()
 *	copy a existing NETLINK message into a new one
 *
 * NOTE: this returns the new SKB/message
 * 	This will be used for response
 */
struct sk_buff *netstandby_nl_cmn_copy_msg(struct sk_buff *orig)
{
	struct sk_buff *copy;
	struct netstandby_nl_cmn *cm;

	cm = netstandby_nl_cmn_get_data(orig);

	copy = skb_copy(orig, GFP_KERNEL);
	if (!copy) {
		netstandby_warn("%d:unable to copy incoming message of len(%d)\n", cm->pid, orig->len);
		return NULL;
	}

	return copy;
}

/*
 * netstandby_nl_cmn_ucast_resp_internal()
 *	send the response to the user (PID)
 *
 * NOTE: this assumes the socket to be available for reception
 */
int netstandby_nl_cmn_ucast_resp_internal(struct sk_buff *skb, struct sock *sk, pid_t pid)
{
	return nlmsg_unicast(sk, skb, pid);
}

/*
 * netstandby_nl_cmn_ucast_resp()
 *	send the response to the user (PID)
 *
 * NOTE: this assumes the socket to be available for reception
 */
int netstandby_nl_cmn_ucast_resp(struct sk_buff *skb)
{
	struct netstandby_nl_cmn *cm;
	struct net *net;

	cm = genlmsg_data(NLMSG_DATA(skb->data));
	net = (struct net *)cm->sock_data;
	cm->sock_data = 0;

	/*
	 * End the message as no more updates are left to happen
	 * After this message is assumed to be read-only
	 */
	genlmsg_end(skb, cm);
	return genlmsg_unicast(net, skb, cm->pid);
}

/*
 * netstandby_nl_cmn_get_msg()
 *	verifies and returns the message pointer
 */
struct netstandby_nl_cmn *netstandby_nl_cmn_get_msg(struct genl_family *family, struct genl_info *info, uint16_t cmd)
{
	struct netstandby_nl_cmn *cm;
	uint32_t pid;

	pid =  info->snd_portid;

	/*
	 * validate the common message header version & magic
	 */
	cm = info->userhdr;
	if (netstandby_nl_cmn_chk_ver(cm, family->version) == false) {
		netstandby_warn("%d, %s: version mismatch (%d)\n", pid, family->name, cm->version);
		return NULL;
	}

	/*
	 * check if the message len arrived matches with expected len
	 */
	if (netstandby_nl_cmn_get_cmd_len(cm) != family->hdrsize) {
		netstandby_warn("%d, %s: invalid command len (%d)\n", pid, family->name, netstandby_nl_cmn_get_cmd_len(cm));
		return NULL;
	}

	cm->pid = pid;
	cm->sock_data = (netstandby_ptr_t)genl_info_net(info);

	return cm;
}

/*
 * netstandby_nl_cmn_init()
 *	init module
 */
void netstandby_nl_cmn_init(void)
{
	struct netstandby_family *family = NULL;
	int i = 0;

	/*
	 * initialize the handler families, the intention to init the
	 * families that are marked active
	 */
	family = &family_handlers[0];

	for (i = 0; i < NETSTANDBY_NL_FAMILY_HANDLER_SZ; i++, family++) {
		/*
		 * Check if the family exists
		 */
		if (!family->valid || !family->entry) {
			netstandby_info("skipping family:%s\n", family->name);
			netstandby_info("valid = %d, entry = %d\n", family->valid, !!family->entry);
			continue;
		}

		netstandby_info("attaching family:%s\n", family->name);
		family->entry();
	}

	return;
}

/*
 * netstandby_cmn_exit()
 *	deinit module
 */
void netstandby_nl_cmn_exit(void)
{
	struct netstandby_family *family = NULL;
	int i = 0;

	/*
	 * initialize the handler families
	 */
	family = &family_handlers[0];

	for (i = 0; i < NETSTANDBY_NL_FAMILY_HANDLER_SZ; i++, family++) {
		/*
		 * Check if the family exists
		 */
		if (!family->valid || !family->exit) {
			netstandby_info("skipping family:%s\n", family->name);
			netstandby_info("valid = %d, exit = %d\n", family->valid, !!family->exit);
			continue;
		}

		netstandby_info("detaching family:%s\n", family->name);

		family->exit();
	}
}
