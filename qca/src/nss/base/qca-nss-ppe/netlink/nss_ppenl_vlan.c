/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_vlan.c
 *	NSS Netlink VLAN Handler
 */

#include <linux/kernel.h>
#include <linux/types.h>

#include <net/genetlink.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_vlan_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_vlan.h"

/*
 * prototypes
 */
static int nss_ppenl_vlan_ops_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_vlan_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_vlan_ops_flush_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_vlan_ops[] = {
	{.cmd = NSS_PPE_VLAN_CREATE_RULE_MSG, .doit = nss_ppenl_vlan_ops_create_rule,},		/* VLAN rule create */
	{.cmd = NSS_PPE_VLAN_DESTROY_RULE_MSG, .doit = nss_ppenl_vlan_ops_destroy_rule,},	/* VLAN rule destroy */
	{.cmd = NSS_PPE_VLAN_FLUSH_RULE_MSG, .doit = nss_ppenl_vlan_ops_flush_rule,},		/* VLAN rule flush */
};

/*
 * VLAN family definition
 */
static struct genl_family nss_ppenl_vlan_family = {
	.name = NSS_PPENL_VLAN_FAMILY,			/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_vlan_rule),	/* NSS NETLINK VLAN rule */
	.version = NSS_PPENL_VER,			/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_VLAN_MAX_MSG_TYPES,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_vlan_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_vlan_ops),
};

#define NSS_PPENL_VLAN_OPS_SZ ARRAY_SIZE(nss_ppenl_vlan_ops)

/*
 * ppe_vlan_rule_dump_rule()
 *	Dumps vlan rule and action fields.
 */
static void ppe_vlan_rule_dump_rule(struct ppe_vlan_rule *rule) {
	nss_ppenl_info("%px rule dump from netlink\n"
			"rule_id: %d\n"
			"src_dev: %s\n"
			"port_type: %d\n"
			"stag_format: %d\n"
			"svid: %u\n"
			"spcp: %u\n"
			"sdei: %u\n"
			"ctag_format: %d\n"
			"cvid: %u\n"
			"cpcp: %u\n"
			"cdei: %u\n"
			"frame_type: %d\n"
			"proto: %u\n"
			"vsi: %u\n"
			"vni_resv_type: %d\n"
			"vni_resv: %u\n"
		"stpid: %u\n"
		"ctpid: %u\n"
		"dhcp_type: %d\n"
			"mc_type: %d\n",
		rule, rule->rule_id, rule->dev_name,
		rule->rule_f.port_type, rule->rule_f.stag_format,
		rule->rule_f.svid, rule->rule_f.spcp, rule->rule_f.sdei, rule->rule_f.ctag_format,
		rule->rule_f.cvid, rule->rule_f.cpcp, rule->rule_f.cdei, rule->rule_f.frame_type,
		rule->rule_f.proto, rule->rule_f.vsi, rule->rule_f.vni_resv_type, rule->rule_f.vni_resv,
		rule->rule_f.stpid, rule->rule_f.ctpid, rule->rule_f.dhcp_type, rule->rule_f.mc_type);

	switch (rule->rule_f.port_type) {
	case PPE_VLAN_PORT_TYPE_BITMAP:
		nss_ppenl_info("port_bitmap: %d\n", rule->rule_f.port_val.port_bitmap);
		break;
	case PPE_VLAN_PORT_TYPE_PORT:
		nss_ppenl_info("dev_name: %s\n", rule->rule_f.port_val.dev_name);
		break;
	case PPE_VLAN_PORT_TYPE_GEM_PORT:
		nss_ppenl_info("gem_port: %d\n", rule->rule_f.port_val.gem_port);
		break;
	default:
		nss_ppenl_info("Invalid port type for dumping rule\n");
	}

	nss_ppenl_info("%px: action dump:\n"
			"swap_svid_cvid: %d\n"
			"svid_xlate_cmd: %d\n"
			"svidxlate: %u\n"
			"cvid_xlate_cmd: %d\n"
			"cvidxlate: %u\n"
			"swap_spcp_cpcp: %d\n"
			"spcp_xlate_cmd: %d\n"
			"spcptranslation: %u\n"
			"cpcp_xlate_cmd: %d\n"
			"cpcptranslation: %u\n"
			"swap_sdei_cdei: %d\n"
			"sdei_xlate_cmd: %d\n"
			"sdeitranslation: %u\n"
			"cdei_xlate_cmd: %d\n"
			"cdeitranslation: %u\n"
			"tags_to_remove: %u\n"
		"stpid_cmd: %d\n"
		"stpid_action: %u\n"
		"ctpid_cmd: %d\n"
		"ctpid_action: %u\n"
			"counter_id: %u\n"
			"counter_mode: %d\n"
			"vsitranslation: %u\n"
			"src_info_type: %d\n"
			"src_info: %s\n"
			"vni_resv_action: %u\n"
			"fwd_cmd: %d\n"
			"service_code: %u\n"
			"dest_info: %s\n",
		rule, rule->action_f.swap_svid_cvid, rule->action_f.svid_xlate_cmd,
		rule->action_f.svidxlate, rule->action_f.cvid_xlate_cmd, rule->action_f.cvidxlate,
		rule->action_f.swap_spcp_cpcp, rule->action_f.spcp_xlate_cmd, rule->action_f.spcptranslation,
		rule->action_f.cpcp_xlate_cmd, rule->action_f.cpcptranslation, rule->action_f.swap_sdei_cdei,
		rule->action_f.sdei_xlate_cmd, rule->action_f.sdeitranslation, rule->action_f.cdei_xlate_cmd,
	rule->action_f.cdeitranslation, rule->action_f.tags_to_remove, rule->action_f.stpid_cmd,
	rule->action_f.stpid_action, rule->action_f.ctpid_cmd, rule->action_f.ctpid_action,
		rule->action_f.counter_id, rule->action_f.counter_mode, rule->action_f.vsitranslation,
		rule->action_f.src_info_type, rule->action_f.src_info, rule->action_f.vni_resv_action,
		rule->action_f.fwd_cmd, rule->action_f.sc, rule->action_f.dest_info);
}

/*
 * nss_ppenl_vlan_ops_create_rule()
 *	rule create handler
 */
static int nss_ppenl_vlan_ops_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_vlan_rule *nl_vlan_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
	ppe_vlan_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_vlan_family, info, NSS_PPE_VLAN_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_vlan_rule = container_of(nl_cm, struct nss_ppenl_vlan_rule, cm);
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

	ppe_vlan_rule_dump_rule(&nl_vlan_rule->rule);

	status = ppe_vlan_rule_create(&nl_vlan_rule->rule);
	if (status == PPE_VLAN_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE rule create success\n", __func__);
	} else {
		nss_ppenl_info("create rule in ppe driver failed, error = %d\n", status);
		nl_vlan_rule->rule.ret = status;
	}

	ret = nl_vlan_rule->rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_vlan_rule = nss_ppenl_get_data(resp);
	nl_vlan_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_vlan_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_ppe_ops_destroy_rule()
 *	rule delete handler
 */
static int nss_ppenl_vlan_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_vlan_rule *nl_vlan_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status;
	ppe_vlan_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_vlan_family, info, NSS_PPE_VLAN_DESTROY_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule destroy data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_vlan_rule = container_of(nl_cm, struct nss_ppenl_vlan_rule, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		goto done;
	}

	status = ppe_vlan_rule_destroy(nl_vlan_rule->rule.rule_id);
	if (status != PPE_VLAN_RET_SUCCESS) {
		nss_ppenl_warn("unable to destroy rule in ppe driver, error = %d\n", status);
		return -EINVAL;
	}

	ret = nl_vlan_rule->rule.ret;

	/*
	 * Send the response back to user application
	 */
	nl_vlan_rule = nss_ppenl_get_data(resp);
	nl_vlan_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: rule_id %d, ret %d\n",
			nl_vlan_rule->rule.rule_id, nl_vlan_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
done:
	return error;
}

/*
 * nss_ppenl_vlan_ops_flush_rule()
 *	rule flush handler
 */
static int nss_ppenl_vlan_ops_flush_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_vlan_rule *nl_vlan_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	ppe_vlan_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_vlan_family, info, NSS_PPE_VLAN_FLUSH_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract rule flush data, %p\n", skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_vlan_rule = container_of(nl_cm, struct nss_ppenl_vlan_rule, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("%d:unable to save response data from NL buffer, %p\n", pid, skb);
		return -ENOMEM;
	}

	if (nl_vlan_rule->rule.except_flag == true)
	{
		ret = ppe_vlan_rule_flush(PPE_VLAN_FLUSH_TYPE_EXCEPT_FIRST);
	} else {
		ret = ppe_vlan_rule_flush(PPE_VLAN_FLUSH_TYPE_ALL);
	}
	if (ret != PPE_VLAN_RET_SUCCESS) {
		nss_ppenl_warn("unable to flush rule in ppe driver, error = %d\n", ret);
		return -EINVAL;
	}

	/*
	 * Send the response back to user application
	 */
	nl_vlan_rule = nss_ppenl_get_data(resp);
	nl_vlan_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_vlan_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_vlan_init()
 *	handler init
 */
bool nss_ppenl_vlan_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE VLAN handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_vlan_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register VLAN family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_vlan_rule));
	return true;
}
EXPORT_SYMBOL(nss_ppenl_vlan_init);

/*
 * nss_ppenl_vlan_exit()
 *	handler exit
 */
bool nss_ppenl_vlan_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink VLAN handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_vlan_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister VLAN NETLINK family\n");
		return false;
	}

	return true;
}
EXPORT_SYMBOL(nss_ppenl_vlan_exit);
