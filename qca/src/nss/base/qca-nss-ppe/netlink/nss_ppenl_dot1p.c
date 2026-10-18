/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_dot1p.c
 *	NSS Netlink DOT1P Handler
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
#include <nss_ppenl_dot1p_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_dot1p.h"

/*
 * prototypes
 */
static int nss_ppenl_dot1p_ops_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_ops_flush_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_ops_create_def_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_ops_resume_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_ops_pause_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_ops_get_state_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_policer_ops_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_policer_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_dot1p_policer_ops_flush_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_dot1p_ops[] = {
	{.cmd = NSS_PPE_DOT1P_CREATE_RULE_MSG, .doit = nss_ppenl_dot1p_ops_create_rule,},		/* rule create */
	{.cmd = NSS_PPE_DOT1P_DELETE_RULE_MSG, .doit = nss_ppenl_dot1p_ops_destroy_rule,},		/* rule destroy */
	{.cmd = NSS_PPE_DOT1P_FLUSH_RULE_MSG, .doit = nss_ppenl_dot1p_ops_flush_rule,},			/* rule flush */
	{.cmd = NSS_PPE_DOT1P_CREATE_DEF_RULE_MSG, .doit = nss_ppenl_dot1p_ops_create_def_rule,},	/* rule create default */
	{.cmd = NSS_PPE_DOT1P_PAUSE_RULE_MSG, .doit = nss_ppenl_dot1p_ops_pause_rule,},			/* rule pause */
	{.cmd = NSS_PPE_DOT1P_RESUME_RULE_MSG, .doit = nss_ppenl_dot1p_ops_resume_rule,},		/* rule resume */
	{.cmd = NSS_PPE_DOT1P_GET_STATE_RULE_MSG, .doit = nss_ppenl_dot1p_ops_get_state_rule,},		/* get state of rule*/
	{.cmd = NSS_PPE_DOT1P_POLICER_CREATE_RULE_MSG, .doit = nss_ppenl_dot1p_policer_ops_create_rule,}, /* rule create for policer*/
	{.cmd = NSS_PPE_DOT1P_POLICER_DELETE_RULE_MSG, .doit = nss_ppenl_dot1p_policer_ops_destroy_rule,},/* rule destroy for policer*/
	{.cmd = NSS_PPE_DOT1P_POLICER_FLUSH_RULE_MSG, .doit = nss_ppenl_dot1p_policer_ops_flush_rule,},	  /* rule flush for policer */
};

/*
 * DOT1P family definition
 */
static struct genl_family nss_ppenl_dot1p_family = {
	.name = NSS_PPENL_DOT1P_FAMILY,			/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_dot1p_rule),	/* NSS NETLINK DOT1P rule */
	.version = NSS_PPENL_VER,			/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_DOT1P_MAX_MSG_TYPES,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_dot1p_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_dot1p_ops),
};

#define NSS_PPENL_DOT1P_OPS_SZ ARRAY_SIZE(nss_ppenl_dot1p_ops)

static void ppe_dot1p_rule_dump_rule(struct ppe_dot1p_rule *rule) {
	if (!(rule->valid_flags & PPE_DOT1P_RULE_FLAG_DEFAULT_RULE)) {
		nss_ppenl_info("%px rule dump from netlink\n"
				"valid_flags: %d"
				"rule_id: %d\n"
				"src_info: %s\n"
				"dst_info: %s\n"
				"vid : %d\n"
				"pcp : %d\n"
				"dei : %d\n"
				"dscp : %d\n"
				"rule_flags: %d\n",
				rule, rule->valid_flags,
				rule->rule_id,
				rule->rule.src_info,
				rule->rule.dst_info,
				rule->rule.vid,
				rule->rule.pcp,
				rule->rule.dei,
				rule->rule.dscp, rule->rule.rule_flags);

		nss_ppenl_info("%px: action dump: gem_port_id: %d, base_pq:%d, pq: %d\n"
				"svc_code:%d, policer_id:%d, us_policer_en:%d\n"
				"ds_policer_en:%d int_dp: %d\n"
				"dst_info: %s, fwd_cmd: %d, action_flags: %d\n",
				rule, rule->action.gem_port,
				rule->action.base_pq,
				rule->action.pq,
				rule->action.svc_code,
				rule->action.policer_id,
				rule->action.us_policer_en,
				rule->action.ds_policer_en,
				rule->action.int_dp,
				rule->action.dst_info,
				rule->action.fwd_cmd,
				rule->action.action_flags);
		}

	if (rule->valid_flags & PPE_DOT1P_RULE_FLAG_DEFAULT_RULE) {
		nss_ppenl_info("%px: default config: vid: %d, pcp: %d\n"
				"dei:%d, dscp:%d, dscp_mask: %d\n"
				"def_rule_flags: %d\n",
				rule, rule->def_rule.vid,
				rule->def_rule.pcp,
				rule->def_rule.dei,
				rule->def_rule.dscp,
				rule->def_rule.dscp_mask,
				rule->def_rule.def_rule_flags);
	}
}

/*
 * nss_ppenl_dot1p_ops_create_rule()
 * 	rule create handler
 */
static int nss_ppenl_dot1p_ops_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
        ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
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
	nl_dot1p_rule->rule.userspace_rule = true;

	ppe_dot1p_rule_dump_rule(&nl_dot1p_rule->rule);
	status = ppe_dot1p_rule_create(&nl_dot1p_rule->rule);
	if (status == PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE rule create success\n", __func__);
	} else {
		nss_ppenl_info("create rule in ppe driver failed, error = %d\n", status);
	}

	ret = nl_dot1p_rule->rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: rule_id %d, ret %d\n", nl_dot1p_rule->rule.rule_id, nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_policer_ops_create_rule()
 * 	Policer rule create handler
 */
static int nss_ppenl_dot1p_policer_ops_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
        ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_POLICER_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
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
	nl_dot1p_rule->rule.userspace_rule = true;

	status = ppe_dot1p_policer_rule_create(&nl_dot1p_rule->policer_rule);
	if (status == PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE rule create success\n", __func__);
	} else {
		nss_ppenl_info("create policer rule in ppe driver failed, error = %d\n", status);
	}

	ret = nl_dot1p_rule->policer_rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->policer_rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: gemport id %d, ret %d\n", nl_dot1p_rule->policer_rule.gemport_id, nl_dot1p_rule->policer_rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_ops_destroy_rule()
 * 	rule delete handler
 */
static int nss_ppenl_dot1p_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_DELETE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule destroy data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
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

	status = ppe_dot1p_rule_delete(nl_dot1p_rule->rule.rule_id);
	if (status != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to create rule in ppe driver, error = %d\n", status);
		return -EINVAL;
	}

	ret = nl_dot1p_rule->rule.ret;

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: rule_id %d, ret %d\n", nl_dot1p_rule->rule.rule_id, nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
done:
	return error;
}

/*
 * nss_ppenl_dot1p_ops_flush_rule()
 * 	rule flush handler
 */
static int nss_ppenl_dot1p_ops_flush_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_FLUSH_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule flush data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	if (nl_dot1p_rule->rule.except_flag == true)
	{
		ret = ppe_dot1p_rule_flush(PPE_DOT1P_FLUSH_TYPE_ALL_EXCEPT_FIRST);
	} else {
		ret = ppe_dot1p_rule_flush(PPE_DOT1P_FLUSH_TYPE_ALL);
	}

	if (ret != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to flush rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_policer_ops_destroy_rule()
 * 	policer rule delete handler
 */
static int nss_ppenl_dot1p_policer_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_POLICER_DELETE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule destroy data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
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

	status = ppe_dot1p_policer_rule_delete(nl_dot1p_rule->policer_rule.gemport_id);
	if (status != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to delete policer rule in ppe driver, error = %d\n", status);
		return -EINVAL;
	}

	ret = nl_dot1p_rule->policer_rule.ret;

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->policer_rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: gemport_id %d, ret %d\n", nl_dot1p_rule->policer_rule.gemport_id, nl_dot1p_rule->policer_rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
done:
	return error;
}

/*
 * nss_ppenl_dot1p_policer_ops_flush_rule()
 * 	policer rule flush handler
 */
static int nss_ppenl_dot1p_policer_ops_flush_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_FLUSH_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule flush data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	ret = ppe_dot1p_policer_rule_flush(PPE_DOT1P_FLUSH_TYPE_ALL);

	if (ret != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to flush policer rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->policer_rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_dot1p_rule->policer_rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_ops_create_def_rule()
 * 	rule create handler
 */
static int nss_ppenl_dot1p_ops_create_def_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
        ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_CREATE_DEF_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract default rule create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
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

	ppe_dot1p_rule_dump_rule(&nl_dot1p_rule->rule);
	status = ppe_dot1p_rule_create(&nl_dot1p_rule->rule);
	if (status == PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE default rule create success\n", __func__);
	} else {
		nss_ppenl_info("create default rule in ppe driver failed, error = %d\n", status);
	}

	ret = nl_dot1p_rule->rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_ops_pause_rule()
 * 	rule pause handler
 */
static int nss_ppenl_dot1p_ops_pause_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_PAUSE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule pause data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	if (nl_dot1p_rule->rule.pause_except_flag == true)
	{
		ret = ppe_dot1p_rule_pause(PPE_DOT1P_PAUSE_TYPE_ALL_EXCEPT_GEM0);
	} else {
		ret = ppe_dot1p_rule_pause(PPE_DOT1P_PAUSE_TYPE_ALL);
	}

	if (ret != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to pause rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_ops_resume_rule()
 * 	rule resume handler
 */
static int nss_ppenl_dot1p_ops_resume_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_RESUME_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule pause data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	ret = ppe_dot1p_rule_resume();
	if (ret != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to resume rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_ops_get_state_rule()
 * 	get state handler
 */
static int nss_ppenl_dot1p_ops_get_state_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dot1p_rule *nl_dot1p_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	uint8_t rule_state;
	ppe_dot1p_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dot1p_family, info, NSS_PPE_DOT1P_GET_STATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule pause data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dot1p_rule = container_of(nl_cm, struct nss_ppenl_dot1p_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	ret = ppe_dot1p_rule_get_state(&rule_state);
	if (ret != PPE_DOT1P_RET_SUCCESS) {
		nss_ppenl_warn("unable to resume rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_dot1p_rule = nss_ppenl_get_data(resp);
	nl_dot1p_rule->rule.ret = ret;
	nl_dot1p_rule->rule.rule_state = rule_state;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_dot1p_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dot1p_init()
 * 	handler init
 */
bool nss_ppenl_dot1p_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE DOT1P handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_dot1p_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register DOT1P family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_dot1p_rule));
	return true;
}

/*
 * nss_ppenl_dot1p_exit()
 *	handler exit
 */
bool nss_ppenl_dot1p_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink DOT1P handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_dot1p_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister DOT1P NETLINK family\n");
		return false;
	}

	return true;
}
