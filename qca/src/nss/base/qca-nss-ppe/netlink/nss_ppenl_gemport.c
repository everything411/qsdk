/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_gemport.c
 *	NSS Netlink GEM PORT Handler
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
#include <nss_ppenl_gemport_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_gemport.h"

/*
 * prototypes
 */
static int nss_ppenl_gem_port_ops_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_gem_port_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_gem_port_ops_flush_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_gem_port_ops[] = {
	{.cmd = NSS_PPE_GEM_PORT_CREATE_RULE_MSG, .doit = nss_ppenl_gem_port_ops_create_rule,},	/* rule create */
	{.cmd = NSS_PPE_GEM_PORT_DELETE_RULE_MSG, .doit = nss_ppenl_gem_port_ops_destroy_rule,},	/* rule destroy */
	{.cmd = NSS_PPE_GEM_PORT_FLUSH_RULE_MSG, .doit = nss_ppenl_gem_port_ops_flush_rule,},	/* rule flush */
};

/*
 * GEM PORT family definition
 */
static struct genl_family nss_ppenl_gem_port_family = {
	.name = NSS_PPENL_GEM_PORT_FAMILY,			/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_gem_port_rule),	/* NSS NETLINK GEM PORT rule */
	.version = NSS_PPENL_VER,				/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_GEM_PORT_MAX_MSG_TYPES,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_gem_port_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_gem_port_ops),
};

#define NSS_PPENL_GEM_PORT_OPS_SZ ARRAY_SIZE(nss_ppenl_gem_port_ops)

static void ppe_gem_port_rule_dump_rule(struct ppe_gem_port_rule *rule) {
	nss_ppenl_info("%px rule dump from netlink\n"
			"valid_flags: %d"
			"gem_port_id : %d\n"
			"rule_flags: %d\n",
			rule, rule->valid_flags,
			rule->rule.gem_port_id,
			rule->rule.rule_flags);

	nss_ppenl_info("%px: action dump: src_info %s, int_pri %d\n"
			"dst_info %s, int_dp %d, policer_id %d\n"
			"dst_selection %d, action_flags %d\n",
			rule, rule->action.src_info,
			rule->action.int_pri,
			rule->action.dst_info,
			rule->action.int_dp,
			rule->action.policer_id,
			rule->action.dst_selection,
			rule->action.action_flags);
}

/*
 * nss_ppenl_gem_port_ops_create_rule()
 * 	rule create handler
 */
static int nss_ppenl_gem_port_ops_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_gem_port_rule *nl_gem_port_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
        ppe_gem_port_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_gem_port_family, info, NSS_PPE_GEM_PORT_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_gem_port_rule = container_of(nl_cm, struct nss_ppenl_gem_port_rule, cm);
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
	 * setting that the rule is from userspace
	 */
	nl_gem_port_rule->rule.userspace_rule = true;

	ppe_gem_port_rule_dump_rule(&nl_gem_port_rule->rule);
	status = ppe_gem_port_rule_create(&nl_gem_port_rule->rule);
	if (status == PPE_GEM_PORT_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE rule create success\n", __func__);
	} else {
		nss_ppenl_info("create rule in ppe driver failed, error = %d\n", status);
	}

	ret = nl_gem_port_rule->rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_gem_port_rule = nss_ppenl_get_data(resp);
	nl_gem_port_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: gem_port_id %d, ret %d\n", nl_gem_port_rule->rule.rule.gem_port_id, nl_gem_port_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_gem_port_ops_destroy_rule()
 * 	rule delete handler
 */
static int nss_ppenl_gem_port_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_gem_port_rule *nl_gem_port_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status;
	ppe_gem_port_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_gem_port_family, info, NSS_PPE_GEM_PORT_DELETE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule destroy data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_gem_port_rule = container_of(nl_cm, struct nss_ppenl_gem_port_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		goto done;
	}

	status = ppe_gem_port_rule_delete(nl_gem_port_rule->rule.rule.gem_port_id);
	if (status != PPE_GEM_PORT_RET_SUCCESS) {
		nss_ppenl_warn("unable to delete rule in ppe driver, error = %d\n", status);
		return -EINVAL;
	}

	ret = nl_gem_port_rule->rule.ret;

	/*
	 * Send the response back to user application
	 */
	nl_gem_port_rule = nss_ppenl_get_data(resp);
	nl_gem_port_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: gem_port_id %d, ret %d\n", nl_gem_port_rule->rule.rule.gem_port_id, nl_gem_port_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
done:
	return error;
}

/*
 * nss_ppenl_gem_port_ops_flush_rule()
 * 	rule flush handler
 */
static int nss_ppenl_gem_port_ops_flush_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_gem_port_rule *nl_gem_port_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	ppe_gem_port_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_gem_port_family, info, NSS_PPE_GEM_PORT_FLUSH_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule flush data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_gem_port_rule = container_of(nl_cm, struct nss_ppenl_gem_port_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	ret = ppe_gem_port_rule_flush(PPE_GEM_PORT_FLUSH_TYPE_USERSPACE);
	if (ret != PPE_GEM_PORT_RET_SUCCESS) {
		nss_ppenl_warn("unable to flush rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_gem_port_rule = nss_ppenl_get_data(resp);
	nl_gem_port_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_gem_port_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_gem_port_init()
 * 	handler init
 */
bool nss_ppenl_gem_port_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE GEM PORT handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_gem_port_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register GEM PORT family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_gem_port_rule));
	return true;
}

/*
 * nss_ppenl_gem_port_exit()
 *	handler exit
 */
bool nss_ppenl_gem_port_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink GEM PORT handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_gem_port_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister GEM PORT NETLINK family\n");
		return false;
	}

	return true;
}
