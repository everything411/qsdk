/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <net/genetlink.h>
#include <net/sock.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_pm_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_pm.h"

static int nss_ppenl_pm_ops_counter_get(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_pm_ops_counter_gen_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_pm_ops_counter_gen_destroy_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_pm_ops[] = {
	{.cmd = NSS_PPE_PM_COUNTER_GET_MSG, .doit = nss_ppenl_pm_ops_counter_get,},				/* PM counter stats get */
	{.cmd = NSS_PPE_PM_COUNTER_GEN_CREATE_RULE_MSG, .doit = nss_ppenl_pm_ops_counter_gen_create_rule,},	/* PM counter gen rule create */
	{.cmd = NSS_PPE_PM_COUNTER_GEN_DESTROY_RULE_MSG, .doit = nss_ppenl_pm_ops_counter_gen_destroy_rule,},	/* PM counter gen rule destroy */
};

/*
 * PM family definition
 */
static struct genl_family nss_ppenl_pm_family = {
	.name = NSS_PPENL_PM_FAMILY,			/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_pm_info),	/* NSS NETLINK PM rule */
	.version = NSS_PPENL_VER,			/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_PM_MAX_MSG_TYPES,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_pm_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_pm_ops),
};

#define NSS_PPENL_PM_OPS_SZ ARRAY_SIZE(nss_ppenl_pm_ops)

/*
 * ppe_pm_counter_stats()
 *	Dumps the PM counter stats
 */
static void ppe_pm_counter_stats( struct ppe_pm_counter *counter) {
	nss_ppenl_info("%px PM counter stats from netlink\n"
			"Counter ID: %u\n"
			"Unicast Packets: %llu\n"
			"Broadcast Packets: %llu\n"
			"Multicast Packets: %llu\n"
			"Oversized Packets: %llu\n"
			"Total Bytes: %llu\n"
			"Frames of size 64 bytes: %llu\n"
			"Frames of size 65-127 bytes: %llu\n"
			"Frames of size 128-255 bytes: %llu\n"
			"Frames of size 256-511 bytes: %llu\n"
			"Frames of size 512-1023 bytes: %llu\n"
			"Frames of size 1024-1518 bytes: %llu\n",
			counter, counter->counter_id,
			counter->ucast_packet, counter->bcast_packet, counter->mcast_packet,
			counter->oversize, counter->octets, counter->frame_64,
			counter->frame_65_127, counter->frame_128_255, counter->frame_256_511,
			counter->frame_512_1023, counter->frame_1024_1518);
}

/*
 * ppe_pm_counter_gen_rule_dump()
 *	Dumps the PM counter gen rule
 */
static void ppe_pm_counter_gen_rule_dump(struct ppe_pm_counter_gen *rule) {
	nss_ppenl_info("%px PM counter gen rule dump from netlink\n"
			"rule_dir: %u\n"
			"pm_dir: %u\n"
			"counter ID: %u\n"
			"port_type: %u\n"
			"tag_format: %u\n"
			"vlan_id: %u\n"
			"pcp: %u\n"
			"ipmc: %u\n",
			rule, rule->rule_dir, rule->pm_dir, rule->counter_id,
			rule->port_type, rule->tag_format,
			rule->vid, rule->pcp, rule->ipmc);

	switch (rule->port_type) {
	case PPE_PM_PORT_TYPE_BITMAP:
		nss_ppenl_info("port_bitmap: %d\n", rule->port.port_bitmap);
		break;
	case PPE_PM_PORT_TYPE_PORT:
		nss_ppenl_info("dev_name: %s\n", rule->port.dev_name);
		break;
	case PPE_PM_PORT_TYPE_GEMPORT:
		nss_ppenl_info("gem_port: %d\n", rule->port.gem_port);
		break;
	default:
		nss_ppenl_info("Invalid port type for dumping rule\n");
	}
}

/*
 * nss_ppenl_pm_ops_counter_get()
 *	PM counter fetching handler
 */
static int nss_ppenl_pm_ops_counter_get(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_pm_info *nl_pm_rule_resp;
	struct nss_ppenl_pm_info  *nl_pm_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
	ppe_pm_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_pm_family, info, NSS_PPE_PM_COUNTER_GET_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PM counter get match info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_pm_rule = container_of(nl_cm, struct nss_ppenl_pm_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	status = ppe_pm_counter_get(&nl_pm_rule->counter_info);
	if (status == PPE_PM_RET_SUCCESS) {
		ppe_pm_counter_stats(&nl_pm_rule->counter_info);
	} else {
		nss_ppenl_info("PM counter stats fetching is failed = %d\n", status);
	}

	ret = nl_pm_rule->counter_info.ret;

	/*
	 * Copy full counter stats into the response payload and send to user application
	 */
	nl_pm_rule_resp = nss_ppenl_get_data(resp);
	memcpy(&nl_pm_rule_resp->counter_info, &nl_pm_rule->counter_info,
			sizeof(nl_pm_rule_resp->counter_info));
	nl_pm_rule_resp->counter_info.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_pm_rule_resp->counter_info.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_pm_ops_counter_gen_create_rule()
 *	PM counter gen rule create handler
 */
static int nss_ppenl_pm_ops_counter_gen_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_pm_info  *nl_pm_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
	ppe_pm_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_pm_family, info, NSS_PPE_PM_COUNTER_GEN_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PM counter get match info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_pm_rule = container_of(nl_cm, struct nss_ppenl_pm_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	/*
	 * Rule dump
	 */
	ppe_pm_counter_gen_rule_dump(&nl_pm_rule->counter_gen);

	status = ppe_pm_counter_gen_rule_create(&nl_pm_rule->counter_gen);
	if (status == PPE_PM_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE rule create success\n", __func__);
	} else {
		nss_ppenl_info("create rule in ppe driver failed, error = %d\n", status);
	}

	ret = nl_pm_rule->counter_gen.ret;

	/*
	 * Send the response code to user application
	 */
	nl_pm_rule = nss_ppenl_get_data(resp);
	nl_pm_rule->counter_gen.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_pm_rule->counter_gen.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_pm_ops_counter_gen_destroy_rule()
 *	PM counter gen rule destroy handler
 */
static int nss_ppenl_pm_ops_counter_gen_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_pm_info  *nl_pm_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error = 0;
	ppe_pm_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_pm_family, info, NSS_PPE_PM_COUNTER_GEN_DESTROY_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract PM counter get match info\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_pm_rule = container_of(nl_cm, struct nss_ppenl_pm_info, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	/*
	 * Rule dump
	 */
	ppe_pm_counter_gen_rule_dump(&nl_pm_rule->counter_gen);

	ret = ppe_pm_counter_gen_rule_destroy(nl_pm_rule->counter_gen.counter_id);
	if (ret == PPE_PM_RET_SUCCESS) {
		nss_ppenl_info("PPE PM counter gen rule destroy successfully\n");
	} else {
		nss_ppenl_info("destroy rule in ppe driver failed, error = %d\n", ret);
	}

	/*
	 * Send the response code to user application
	 */
	nl_pm_rule = nss_ppenl_get_data(resp);
	nl_pm_rule->counter_gen.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_pm_rule->counter_gen.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;

}

/*
 * nss_ppenl_pm_init()
 *	handler init
 */
bool nss_ppenl_pm_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE PM handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_pm_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register PM family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_pm_info));

	return true;
}
EXPORT_SYMBOL(nss_ppenl_pm_init);

/*
 * nss_ppenl_pm_exit()
 *	handler exit
 */
bool nss_ppenl_pm_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink PM handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_pm_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister PM  NETLINK family\n");
		return false;
	}

	return true;
}
EXPORT_SYMBOL(nss_ppenl_pm_exit);
