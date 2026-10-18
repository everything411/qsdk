/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_cos_mapping.c
 * NSS Netlink CoS Map Handler
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
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

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_cos_map_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_cos_map.h"
#include <ppe_cos_map.h>

/*
 * prototypes
 */
static int nss_ppenl_cos_map_ops_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_cos_map_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_cos_map_ops_flush_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_cos_map_ops_set_port_group(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_cos_map_ops[] = {
	{.cmd = NSS_PPE_COS_MAP_CREATE_RULE_MSG, .doit = nss_ppenl_cos_map_ops_create_rule,},	/* rule create */
	{.cmd = NSS_PPE_COS_MAP_DESTROY_RULE_MSG, .doit = nss_ppenl_cos_map_ops_destroy_rule,},	/* rule destroy */
	{.cmd = NSS_PPE_COS_MAP_FLUSH_RULE_MSG, .doit = nss_ppenl_cos_map_ops_flush_rule,},	/* rule flush */
	{.cmd = NSS_PPE_COS_MAP_PORT_GROUP_SET, .doit = nss_ppenl_cos_map_ops_set_port_group,},	/* port group set */
};

/*
 * CoS Map family definition
 */
static struct genl_family nss_ppenl_cos_map_family = {
	.name = NSS_PPENL_COS_MAP_FAMILY,	/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_cos_map_config),	/* NSS NETLINK CoS Map config */
	.version = NSS_PPENL_VER,	/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_COS_MAP_MAX_MSG_TYPES,	/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_cos_map_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_cos_map_ops),
};

#define NSS_PPENL_COS_MAP_OPS_SZ ARRAY_SIZE(nss_ppenl_cos_map_ops)

/*
 * nss_ppenl_cos_map_ops_create_rule()
 * 	rule create handler
 */
static int nss_ppenl_cos_map_ops_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_cos_map_config *nl_cos_map_config;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	enum ppe_cos_map_ret pt;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_cos_map_family, info, NSS_PPE_COS_MAP_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_cos_map_config = container_of(nl_cm, struct nss_ppenl_cos_map_config, cm);
	pid = nl_cm->pid;

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

	pt = ppe_cos_map_create(&nl_cos_map_config->msg.rule);
	if (pt == PPE_COS_MAP_SUCCESS) {
		nss_ppenl_info("PPE rule create success");
	} else {
		nss_ppenl_info("create rule in ppe driver failed, error = %d", pt);
	}

	nl_cos_map_config = nss_ppenl_get_data(resp);
	nl_cos_map_config->msg.rule.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_cos_map_ops_destroy_rule()
 * 	rule delete handler
 */
static int nss_ppenl_cos_map_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_cos_map_config *nl_cos_map_config;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error = 0;
	enum ppe_cos_map_ret pt;
	struct ppe_cos_map_destroy_info destroy = {0};

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_cos_map_family, info, NSS_PPE_COS_MAP_DESTROY_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule destroy data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_cos_map_config = container_of(nl_cm, struct nss_ppenl_cos_map_config, cm);
	pid = nl_cm->pid;

	destroy.rule_id = nl_cos_map_config->msg.rule.rule_id;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		goto done;
	}

	pt = ppe_cos_map_destroy(&destroy);
	if (pt == PPE_COS_MAP_SUCCESS) {
		nss_ppenl_info("PPE rule destroy success");
	} else {
		nss_ppenl_info("Delete rule in ppe driver failed, error = %d",pt);
	}

	nl_cos_map_config = nss_ppenl_get_data(resp);
	nl_cos_map_config->msg.rule.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
done:
	return error;
}

/*
 * nss_ppenl_cos_map_ops_flush_rule()
 * 	rule flush handler
 */
static int nss_ppenl_cos_map_ops_flush_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_cos_map_config *nl_cos_map_config;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_cos_map_ret pt;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_cos_map_family, info, NSS_PPE_COS_MAP_FLUSH_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule flush data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_cos_map_config = container_of(nl_cm, struct nss_ppenl_cos_map_config, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	pt = ppe_cos_map_rule_flush();
	if (pt == PPE_COS_MAP_SUCCESS) {
		nss_ppenl_info("rule flush success");
	} else {
		nss_ppenl_info("Flush rule in ppe driver failed, error = %d",pt);
	}

	nl_cos_map_config = nss_ppenl_get_data(resp);
	nl_cos_map_config->msg.rule.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_cos_map_ops_set_port_group()
 * 	port group set handler
 */
static int nss_ppenl_cos_map_ops_set_port_group(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_cos_map_config *nl_cos_map_config;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	enum ppe_cos_map_ret pt;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_cos_map_family, info, NSS_PPE_COS_MAP_PORT_GROUP_SET);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract port group config data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_cos_map_config = container_of(nl_cm, struct nss_ppenl_cos_map_config, cm);
	pid = nl_cm->pid;

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

	pt = ppe_cos_map_port_group_set(&nl_cos_map_config->msg.config);
	if (pt == PPE_COS_MAP_SUCCESS) {
		nss_ppenl_info("PPE port group set success");
	} else {
		nss_ppenl_info("port group set in ppe driver failed, error = %d", pt);
	}

	nl_cos_map_config = nss_ppenl_get_data(resp);
	nl_cos_map_config->msg.config.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_cos_map_init()
 *  handler init
 */
bool nss_ppenl_cos_map_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE CoS Map handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_cos_map_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register CoS Mapping family\n");
		return false;
	}

	return true;
}

/*
 * nss_ppenl_cos_map_exit()
 * handler exit
 */
bool nss_ppenl_cos_map_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink CoS Map handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_cos_map_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister CoS Map NETLINK family\n");
		return false;
	}

	return true;
}
