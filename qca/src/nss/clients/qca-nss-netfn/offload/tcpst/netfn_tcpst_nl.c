/*
 * netfn_tcpst_nl.c
 *      Netfn TCP Speedtest Netlink utility
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/netlink.h>
#include <net/genetlink.h>
#include <linux/version.h>
#include "netfn_tcpst_nl.h"

static int netfn_tcpst_nl_start(struct sk_buff *skb, struct genl_info *info);
static int netfn_tcpst_nl_stop(struct sk_buff *skb, struct genl_info *info);
static int netfn_tcpst_nl_get_stats(struct sk_buff *skb, struct genl_info *info);

/*
 * netfn_tcpst_nl_policy
 *	NLA policy for TCPST
 */
static struct nla_policy netfn_tcpst_nl_policy[NETFN_TCPST_NL_MAX + 1] = {
	[NETFN_TCPST_NL_ATTR_CFG] 	= { .type = NLA_NESTED, },
	[NETFN_TCPST_NL_ATTR_RES] 	= { .type = NLA_NESTED, },
	[NETFN_TCPST_NL_ATTR_STATS] 	= { .type = NLA_NESTED, },
};

/*
 * netfn_tcpst_nl_ops
 *	Command based operation table called by the generic netlink layer.
 */
static struct genl_ops netfn_tcpst_nl_ops[] = {
	{	.cmd		= NETFN_TCPST_NL_CMD_START,
		.doit		= netfn_tcpst_nl_start,
		.validate	= GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags		= GENL_ADMIN_PERM,
	},

	{	.cmd		= NETFN_TCPST_NL_CMD_STATS,
		.doit		= netfn_tcpst_nl_get_stats,
		.validate	= GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags		= GENL_ADMIN_PERM,
	},

	{	.cmd		= NETFN_TCPST_NL_CMD_STOP,
		.doit		= netfn_tcpst_nl_stop,
		.validate	= GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.flags		= GENL_ADMIN_PERM,
	},
};

/*
 * netfn_tcpst_nl_family
 *	TCPST general netlink family
 */
static struct genl_family netfn_tcpst_nl_family = {
	.name           = NETFN_TCPST_NL_FAMILY,
	.version        = 1,
	.hdrsize        = 0,
	.maxattr        = NETFN_TCPST_NL_MAX,
	.policy         = netfn_tcpst_nl_policy,
	.netnsok        = true,
	.module         = THIS_MODULE,
	.ops            = netfn_tcpst_nl_ops,
	.n_ops          = ARRAY_SIZE(netfn_tcpst_nl_ops),
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0))
	.resv_start_op  = NETFN_TCPST_NL_CMD_MAX,
#else
#error "Command enum needs to be corrected for older kernel version"
#endif
};

/*
 * netfn_tcpst_nl_comp_cb
 *	TCPST completion callback
 */
void netfn_tcpst_nl_comp_cb(void *cb_data, struct netfn_tcpst_result *res)
{
	*(struct netfn_tcpst_result *)cb_data = *res;
}

/*
 * netfn_tcpst_nl_start
 * 	Unpack netlink message and start TCP speed test
 */
static int netfn_tcpst_nl_start(struct sk_buff *skb, struct genl_info *info)
{
	struct netfn_tcpst_cfg *cfg = NULL;
	enum netfn_tcpst_state state;
	struct sk_buff *msg = NULL;
	struct nlattr *attr_cfg;
	char *hdr = NULL;

	attr_cfg = info->attrs[NETFN_TCPST_NL_ATTR_CFG];

	/*
	 * Check the length of received netlink message payload
	 */
	if (nla_len(attr_cfg) != sizeof(struct netfn_tcpst_cfg)) {
		pr_err("%d,%s: Invalid length\n", info->snd_portid, NETFN_TCPST_NL_FAMILY);
		return -EINVAL;
	}

	cfg = nla_data(attr_cfg);
	if (!cfg) {
		pr_err("%s:Failed to fetch cfg\n", NETFN_TCPST_NL_FAMILY);
		return -EINVAL;
	}

	/*
	 * Create netlink message to send tcpst state as response to userspace.
	 */
	msg = nlmsg_new(sizeof(state), GFP_KERNEL);
	if (!msg) {
		pr_err("%px:Failed to allocate response message\n", info);
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq, &netfn_tcpst_nl_family, 0, NETFN_TCPST_NL_CMD_START);
	if (!hdr) {
		pr_err("%px: Failed to put hdr in netlink buffer\n", info);
		nlmsg_free(msg);
		return -ENOMEM;
	}

	state = netfn_tcpst_start(cfg);
	nla_put(msg, NETFN_TCPST_NL_ATTR_STATE, sizeof(state), &state);
	genlmsg_end(msg, hdr);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
}

/*
 * netfn_tcpst_nl_stop
 *      Stop TCP speed test
 */
static int netfn_tcpst_nl_stop(struct sk_buff *skb, struct genl_info *info)
{
	struct netfn_tcpst_result result = {0};
	enum netfn_tcpst_state state;
	struct sk_buff *msg = NULL;
	void *hdr = NULL;
	int msg_size = 0;

	msg_size = sizeof(struct netfn_tcpst_result) + sizeof(enum netfn_tcpst_state);

	/*
         * Create netlink message to send tcpst state and result as
	 * response to userspace.
         */
	msg = nlmsg_new(msg_size, GFP_KERNEL);
	if (!msg) {
		pr_err("%px:Failed to allocate response message\n", skb);
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq, &netfn_tcpst_nl_family, 0, NETFN_TCPST_NL_CMD_STOP);
	if (!hdr) {
		pr_err("%px: Failed to put hdr in netlink buffer\n", skb);
		nlmsg_free(msg);
		return -ENOMEM;
	}

	state = netfn_tcpst_stop(netfn_tcpst_nl_comp_cb, &result);

	nla_put(msg, NETFN_TCPST_NL_ATTR_RES, sizeof(struct netfn_tcpst_result), &result);
	nla_put(msg, NETFN_TCPST_NL_ATTR_STATE, sizeof(state), &state);
	genlmsg_end(msg, hdr);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
}

/*
 * netfn_tcpst_nl_get_stats
 *      Get the stats and send it to userspace
 */
static int netfn_tcpst_nl_get_stats(struct sk_buff *skb, struct genl_info *info)
{
	struct netfn_tcpst_stats stats = {0};
	enum netfn_tcpst_state state;
	struct sk_buff *msg = NULL;
	char *hdr = NULL;
	int msg_size = 0;

	msg_size = sizeof(struct netfn_tcpst_stats) + sizeof(enum netfn_tcpst_state);

	/*
         * Create netlink message to send tcpst state and stats as
         * response to userspace.
         */
	msg = nlmsg_new(msg_size, GFP_KERNEL);
	if (!msg) {
		pr_err("%px:Failed to allocate response message\n", &stats);
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, info->snd_portid, info->snd_seq, &netfn_tcpst_nl_family, 0, NETFN_TCPST_NL_CMD_STATS);
	if (!hdr) {
		pr_err("%px: Failed to put hdr in netlink buffer\n", &stats);
		nlmsg_free(msg);
		return -ENOMEM;
	}

	state = netfn_tcpst_get_stats(&stats);
	nla_put(msg, NETFN_TCPST_NL_ATTR_STATS, sizeof(struct netfn_tcpst_stats), &stats);
	nla_put(msg, NETFN_TCPST_NL_ATTR_STATE, sizeof(state), &state);
	genlmsg_end(msg, hdr);

	return genlmsg_unicast(genl_info_net(info), msg, info->snd_portid);
}

/*
 * netfn_tcpst_nl_register()
 *      handler init
 */
bool netfn_tcpst_nl_register(void)
{
	int error;

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&netfn_tcpst_nl_family);
	if (error != 0) {
		pr_err("%px: Error: Unable to register TCPST netlink ops\n", &netfn_tcpst_nl_family);
		return false;
	}

	pr_info("%px: Registered TCPST netlink family\n", &netfn_tcpst_nl_family);

	return true;
}

/*
 * nss_tcpst_nl_exit()
 *      handler exit
 */
bool netfn_tcpst_nl_unregister(void)
{
	int error;

	pr_info("%px: Unregistering the TCPST netlink ops\n", &netfn_tcpst_nl_family);

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&netfn_tcpst_nl_family);
	if (error != 0) {
		pr_err("%px: Unable to unregister TCPST netlink ops\n", &netfn_tcpst_nl_family);
		return false;
	}

	return true;
}
