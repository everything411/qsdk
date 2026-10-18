/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * netstandby_nl.c
 *	Netstandby netlink Handler
 */

#include <linux/kernel.h>
#include <linux/netstandby.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <linux/rcupdate.h>
#include <linux/etherdevice.h>
#include <linux/if_addr.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/if_vlan.h>
#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/in.h>

#include <net/arp.h>
#include <net/genetlink.h>
#include <net/neighbour.h>
#include <net/net_namespace.h>
#include <net/route.h>
#include <net/sock.h>

#include <netstandby_nl_if.h>
#include "netstandby_nl_cmn.h"
#include "netstandby_main.h"

/*
 * prototypes
 */
static int netstandby_nl_ops_init_rule(struct sk_buff *skb, struct genl_info *info);
static int netstandby_nl_ops_standby_enter_rule(struct sk_buff *skb, struct genl_info *info);
static int netstandby_nl_ops_standby_exit_rule(struct sk_buff *skb, struct genl_info *info);
static int netstandby_nl_ops_standby_stop_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops netstandby_nl_user_ops[] = {
	{.cmd = NETSTANDBY_NL_INIT_MSG, .doit = netstandby_nl_ops_init_rule,},			/* rule init */
	{.cmd = NETSTANDBY_NL_ENTER_MSG, .doit = netstandby_nl_ops_standby_enter_rule,},	/* rule enter */
	{.cmd = NETSTANDBY_NL_EXIT_MSG, .doit = netstandby_nl_ops_standby_exit_rule,},		/* rule exit */
	{.cmd = NETSTANDBY_NL_STOP_MSG, .doit = netstandby_nl_ops_standby_stop_rule,},             /* rule stop */
};

/*
 * Netstandby family definition
 */
static struct genl_family netstandby_nl_family = {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0))
	.id = GENL_ID_GENERATE,				/* Auto generate ID */
#endif
	.name = NETSTANDBY_NL_FAMILY,			/* family name string */
	.hdrsize = sizeof(struct netstandby_nl_msg),	/* Netstandby NETLINK rule */
	.version = NETSTANDBY_NL_VER,				/* Set it to NSS_PPENL_VER version */
	.maxattr = NETSTANDBY_NL_MAX_MSG,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = netstandby_nl_user_ops,
	.n_ops = ARRAY_SIZE(netstandby_nl_user_ops),
};

#define NETSTANDBY_NL_USER_OPS_SZ ARRAY_SIZE(netstandby_nl_user_ops)

/*
 * TODO: Cleanup to create a common function for common code in init, enter and exit api
 */

/*
 * netstandby_nl_ops_stop_rule
 */
static int netstandby_nl_ops_standby_stop_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct netstandby_nl_msg *nl_user_rule;
	struct netstandby_nl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error = 0, ret;

	/*
	 * extract the message payload
	 */
	nl_cm = netstandby_nl_cmn_get_msg(&netstandby_nl_family, info, NETSTANDBY_NL_STOP_MSG);
	if (!nl_cm) {
		netstandby_warn("unable to extract rule stop data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_user_rule = container_of(nl_cm, struct netstandby_nl_msg, cm);
	pid = nl_cm->pid;
	netstandby_info("%s: pid: %d\n", __func__, pid);

	if (netstandby_deinit_msg_send(nl_user_rule) != NETSTANDBY_NL_RET_SUCCESS) {
		netstandby_warn("unable to send stop message\n");
		return -EINVAL;
	}

	/*
	 * copy the NL message for response
	 * TODO:: return response in error cases
	 */
	resp = netstandby_nl_cmn_copy_msg(skb);
	if (!resp) {
		netstandby_warn("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		return error;
	}

	ret = nl_user_rule->ret;

	/*
	 * Send the response code to user application
	 */
	nl_user_rule = netstandby_nl_cmn_get_data(resp);
	nl_user_rule->ret = ret;
	netstandby_nl_cmn_ucast_resp(resp);
	return 0;
}

/*
 * netstandby_nl_ops_init_rule()
 * 	rule create handler
 */
static int netstandby_nl_ops_init_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct netstandby_nl_msg *nl_user_rule;
	struct netstandby_nl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error = 0, ret;

	/*
	 * extract the message payload
	 */
	nl_cm = netstandby_nl_cmn_get_msg(&netstandby_nl_family, info, NETSTANDBY_NL_INIT_MSG);
	if (!nl_cm) {
		netstandby_warn("unable to extract rule init data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_user_rule = container_of(nl_cm, struct netstandby_nl_msg, cm);
	pid = nl_cm->pid;
	netstandby_info("%s: pid: %d\n", __func__, pid);

	if (netstandby_init_msg_send(nl_user_rule) != NETSTANDBY_NL_RET_SUCCESS) {
		netstandby_info("unable to send init message\n");
		return -EINVAL;
	}

	/*
	 * copy the NL message for response
	 * TODO:: return response in error cases
	 */
	resp = netstandby_nl_cmn_copy_msg(skb);
	if (!resp) {
		netstandby_warn("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		return error;
	}

	ret = nl_user_rule->ret;

	/*
	 * Send the response code to user application
	 */
	nl_user_rule = netstandby_nl_cmn_get_data(resp);
	nl_user_rule->ret = ret;
	netstandby_nl_cmn_ucast_resp(resp);
	return 0;
}

/*
 * netstandby_nl_ops_standby_enter_rule()
 * 	standby enter rule
 */
static int netstandby_nl_ops_standby_enter_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct netstandby_nl_msg *nl_user_rule;
	struct netstandby_nl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = netstandby_nl_cmn_get_msg(&netstandby_nl_family, info, NETSTANDBY_NL_ENTER_MSG);
	if (!nl_cm) {
		netstandby_warn("unable to extract enter rule data\n");
		return -EINVAL;
	}

	nl_user_rule = container_of(nl_cm, struct netstandby_nl_msg, cm);
	pid = nl_cm->pid;
	netstandby_info("%s: pid: %d\n", __func__, pid);

	if (netstandby_enter_msg_send(nl_user_rule) != NETSTANDBY_NL_RET_SUCCESS) {
		netstandby_info("unable to send enter message\n");
		return -EINVAL;
	}

	/*
	 * copy the NL message for response
	 */
	resp = netstandby_nl_cmn_copy_msg(skb);
	if (!resp) {
		netstandby_warn("%d:unable to save response data from NL buffer\n", pid);
		return -ENOMEM;
	}

	ret = nl_user_rule->ret;

	/*
	 * Send the response back to user application
	 */
	nl_user_rule = netstandby_nl_cmn_get_data(resp);
	nl_user_rule->ret = ret;

	netstandby_nl_cmn_ucast_resp(resp);
	return 0;
}

/*
 * netstandby_nl_ops_standby_exit_rule()
 * 	standby exit rule
 */
static int netstandby_nl_ops_standby_exit_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct netstandby_nl_msg *nl_user_rule;
	struct netstandby_nl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int ret;

	/*
	 * extract the message payload
	 */
	nl_cm = netstandby_nl_cmn_get_msg(&netstandby_nl_family, info, NETSTANDBY_NL_EXIT_MSG);
	if (!nl_cm) {
		netstandby_warn("unable to extract rule exit data\n");
		return -EINVAL;
	}

	nl_user_rule = container_of(nl_cm, struct netstandby_nl_msg, cm);
	pid = nl_cm->pid;
	netstandby_info("%s: pid: %d\n", __func__, pid);

	if (netstandby_exit_msg_send(nl_user_rule) != NETSTANDBY_NL_RET_SUCCESS) {
		netstandby_info("unable to send exit message\n");
		return -EINVAL;
	}

	/*
	 * copy the NL message for response
	 */
	resp = netstandby_nl_cmn_copy_msg(skb);
	if (!resp) {
		netstandby_warn("%d:unable to save response data from NL buffer\n", pid);
		return -ENOMEM;
	}

	ret = nl_user_rule->ret;

	/*
	 * Send the response back to user application
	 */
	nl_user_rule = netstandby_nl_cmn_get_data(resp);
	nl_user_rule->ret = ret;

	netstandby_nl_cmn_ucast_resp(resp);
	return 0;
}

/*
 * netstandby_nl_init()
 * 	handler init
 */
bool netstandby_nl_init(void)
{
	int error;

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&netstandby_nl_family);
	if (error != 0) {
		netstandby_info_always("Error: unable to register Network standby family\n");
		return false;
	}

	return true;
}

/*
 * netstandby_nl_init()
 *	handler exit
 */
bool netstandby_nl_exit(void)
{
	int error;

	netstandby_info("Exit netlink network standby handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&netstandby_nl_family);
	if (error != 0) {
		netstandby_info_always("unable to unregister NETLINK Network standby family\n");
		return false;
	}

	return true;
}
