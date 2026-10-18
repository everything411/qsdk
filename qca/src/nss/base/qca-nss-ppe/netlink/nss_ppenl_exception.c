/*
 * Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved.
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
 * nss_ppenl_exception.c
 * NSS Netlink EXCEPTION Handler
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
#include <nss_ppenl_exception_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_exception.h"

/*
 * prototypes
 */
static int nss_ppenl_exception_ops_config_exception(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_exception_ops[] = {
	{.cmd = NSS_PPENL_EXCEPTION_CONFIG_EXCEPTION_MSG, .doit = nss_ppenl_exception_ops_config_exception,},	/* exception config */
};

/*
 * EXCEPTION family definition
 */
static struct genl_family nss_ppenl_exception_family = {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0))
	.id = GENL_ID_GENERATE,	/* Auto generate ID */
#endif
	.name = NSS_PPENL_EXCEPTION_FAMILY,	/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_exception_rule),	/* NSS NETLINK Exception rule */
	.version = NSS_PPENL_VER,	/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPENL_EXCEPTION_MAX_MSG_TYPES,	/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_exception_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_exception_ops),
};

#define NSS_PPENL_EXCEPTION_OPS_SZ ARRAY_SIZE(nss_ppenl_exception_ops)

/*
 * ppe_exception_config_dump_rule()
 *	Exception dump
 */
static void ppe_exception_config_dump_rule(struct ppe_drv_cc_usr_exception_info *rule)
{
#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	nss_ppenl_info("%px rule dump from netlink\n"
					"CPU code: %d\n"
					"Flush: %d\n"
					"Deaccel: %d\n"
					"Flow type: %d\n"
					"Action: %d\n"
					"Tun profile: %d\n"
					"Exception_edit_enable: %d\n",
					rule, rule->code,
					rule->flush_en,
					rule->deaccel_en,
					rule->flow_type,
					rule->action,
					rule->tun_profile,
					rule->exception_edit_en);
#else
	nss_ppenl_info("%px rule dump from netlink\n"
					"CPU code: %d\n"
					"Flush: %d\n"
					"Deaccel: %d\n"
					"Flow type: %d\n"
					"Action: %d\n"
					"Tun profile: %d\n",
					rule, rule->code,
					rule->flush_en,
					rule->deaccel_en,
					rule->flow_type,
					rule->action,
					rule->tun_profile);
#endif
}

/*
 * nss_ippenl_exception_ops_config_rule()
 * 	rule config handler
 */
static int nss_ppenl_exception_ops_config_exception(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_exception_rule *nl_exception_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
	ppe_drv_cc_usr_ret_t ret;

	/*
	 * extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_exception_family, info, NSS_PPENL_EXCEPTION_CONFIG_EXCEPTION_MSG);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract rule config data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_exception_rule = container_of(nl_cm, struct nss_ppenl_exception_rule, cm);
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

	ppe_exception_config_dump_rule(&nl_exception_rule->info);

	status = ppe_drv_cc_exception_configure(&nl_exception_rule->info);
	if (status == PPE_DRV_CC_USR_RET_SUCCESS) {
		nss_ppenl_info("%s: PPE rule config success\n", __func__);
	} else {
		nss_ppenl_info("config rule in ppe driver failed, error = %d\n", status);
	}

	ret = nl_exception_rule->info.ret;

	/*
	 * Send the response code to user application
	 */
	nl_exception_rule = nss_ppenl_get_data(resp);
	nl_exception_rule->info.ret = ret;

	nss_ppenl_trace("Sending response to userspace: Exception code %d, ret %d\n", nl_exception_rule->info.code, nl_exception_rule->info.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}


/*
 * nss_ppenl_exception_init()
 *  handler init
 */
bool nss_ppenl_exception_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE EXCEPTION handler\n");

	/*
	 * register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_exception_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register EXCEPTION family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_exception_rule));
	return true;
}

/*
 * nss_ppenl_exception_exit()
 * handler exit
 */
bool nss_ppenl_exception_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink EXCEPTION handler\n");

	/*
	 * unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_exception_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister EXCEPTION NETLINK family\n");
		return false;
	}

	return true;
}
