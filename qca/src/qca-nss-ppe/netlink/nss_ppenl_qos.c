/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_qos.c
 * NSS Netlink QOS Handler
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
#include <nss_ppenl_qos_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_qos.h"
#include <ppe_qos.h>

/*
 * prototypes
 */
static int nss_ppenl_qos_ops_get_int_pri(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_qos_ops[] = {
	{.cmd = NSS_PPE_QOS_GET_INT_PRI, .doit = nss_ppenl_qos_ops_get_int_pri,},	/* req create */
};

/*
 * QOS family definition
 */
static struct genl_family nss_ppenl_qos_family = {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0))
	.id = GENL_ID_GENERATE,	/* Auto generate ID */
#endif
	.name = NSS_PPENL_QOS_FAMILY,	/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_qos_req),	/* NSS NETLINK Policer req */
	.version = NSS_PPENL_VER,	/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_QOS_MAX_MSG_TYPES,	/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_qos_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_qos_ops),
};

#define NSS_PPENL_QOS_OPS_SZ ARRAY_SIZE(nss_ppenl_qos_ops)

/*
 * nss_ppenl_qos_ops_get_int_pri()
 * req create handler
 */
static int nss_ppenl_qos_ops_get_int_pri(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	enum ppe_qos_ret pt;
	struct ppe_qos_req req = {0};

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_GET_INT_PRI);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract req create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling req API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	memcpy(&req.dev, nl_qos_req->config.dev, sizeof(nl_qos_req->config.dev));
	req.handle_id = nl_qos_req->config.handle_id;
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		error = -ENOMEM;
		nss_ppenl_ucast_resp(skb);
		return error;
	}

	pt = ppe_qos_get_int_pri_func(&req);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE qos req success");
	} else {
		nss_ppenl_info("Input data is invalid, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->config.ret = pt;
	nl_qos_req->config.int_pri = req.int_pri;
	nl_qos_req->config.ucast_qid = req.ucast_qid;
	nl_qos_req->config.port_id = req.port_id;

	nss_ppenl_info("Returned values from PPE driver callback are:\n"
			"int_pri = %d, \n ucast_qid = %d, \n handle_id = %d\n",
			req.int_pri, req.ucast_qid, req.handle_id);

	nss_ppenl_ucast_resp(resp);
	return 0;
}

bool nss_ppenl_qos_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE QOS handler\n");

	/*
	 * Register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_qos_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register ACL family\n");
		return false;
	}

	return true;
}

/*
 * nss_ppenl_qos_exit()
 * handler exit
 */
bool nss_ppenl_qos_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink QOS handler\n");

	/*
	 * Unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_qos_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister QOS NETLINK family\n");
		return false;
	}

	return true;
}
