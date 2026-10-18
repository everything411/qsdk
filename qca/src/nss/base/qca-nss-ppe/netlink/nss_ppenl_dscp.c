/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_dscp.c
 *	NSS Netlink DSCP Handler
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/netlink.h>
#include <net/genetlink.h>
#include <net/sock.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_dscp_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_dscp.h"

/*
 * prototypes
 */
static int nss_ppenl_dscp_ops_dscp_p_tbl_configure(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_dscp_ops[] = {
	{.cmd = NSS_PPE_DSCP_CONFIG_RULE_MSG, .doit = nss_ppenl_dscp_ops_dscp_p_tbl_configure,},	/* DSCP_PBIT rule configure */
};

/*
 * DSCP family definition
 */
static struct genl_family nss_ppenl_dscp_family = {
	.name = NSS_PPENL_DSCP_FAMILY,			/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_dscp_rule),	/* NSS NETLINK DSCP rule */
	.version = NSS_PPENL_VER,			/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_DSCP_MAX_MSG_TYPES,		/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_dscp_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_dscp_ops),
};

#define NSS_PPENL_DSCP_OPS_SZ ARRAY_SIZE(nss_ppenl_dscp_ops)

/*
 * ppe_dscp_dump_rule()
 * 	Dumps of DSCP to P bit rule
 */
static void ppe_dscp_dump_rule(struct ppe_dscp_rule *rule) {
	nss_ppenl_info("%px dscp to p bit rule dump from netlink\n"
			"dir: %d\n"
			"ecn: %d\n"
			"dscp: %d\n"
			"pcp0: %d\n"
			"pcp1: %d\n",
			rule, rule->dir, rule->ecn,
			rule->dscp_val, rule->pcp0, rule->pcp1);
}

/*
 * nss_ppenl_dscp_ops_dscp_p_tbl_configure()
 *	Handler for configuring DSCP_P PPE table.
 */
static int nss_ppenl_dscp_ops_dscp_p_tbl_configure(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_dscp_rule *nl_dscp_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int status = 0;
	ppe_dscp_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_dscp_family, info, NSS_PPE_DSCP_CONFIG_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule configure data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_dscp_rule = container_of(nl_cm, struct nss_ppenl_dscp_rule, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	ppe_dscp_dump_rule(&nl_dscp_rule->rule);
	status = ppe_dscp_p_tbl_configure(&nl_dscp_rule->rule);
	if (status == PPE_DSCP_RET_SUCCESS) {
		nss_ppenl_info("%s: DSCP_P_BIT rule configure success\n", __func__);
	} else {
		nss_ppenl_info("config of dscp to p bit table config failed, error = %d\n", status);
	}

	ret = nl_dscp_rule->rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_dscp_rule = nss_ppenl_get_data(resp);
	nl_dscp_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n",  nl_dscp_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_dscp_init()
 *	handler init
 */
bool nss_ppenl_dscp_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE DSCP handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_dscp_family);
	if (error != 0) {
		nss_ppenl_info_always("Error %d: unable to register DSCP family\n", error);
		return false;
	}

	nss_ppenl_info("Registered DSCP netlink family '%s' with %u ops\n", nss_ppenl_dscp_family.name,
					nss_ppenl_dscp_family.n_ops);
	return true;
}
EXPORT_SYMBOL(nss_ppenl_dscp_init);

/*
 * nss_ppenl_dscp_exit()
 *	handler exit
 */
bool nss_ppenl_dscp_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink DSCP handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_dscp_family);
	if (error != 0) {
		nss_ppenl_info_always("Error %d: unable to unregister DSCP NETLINK family\n", error);
		return false;
	}

	return true;
}
EXPORT_SYMBOL(nss_ppenl_dscp_exit);

