/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_mcast.c
 * NSS Netlink Multicast Handler
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
#include <nss_ppenl_mcast_if.h>
#include "nss_ppenl.h"
#include "nss_ppenl_mcast.h"
#include <ppe_mcast.h>

/*
 * prototypes
 */
static int nss_ppenl_mcast_ops_create_mcast_entry(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_mcast_ops_delete_mcast_entry(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_mcast_ops[] = {
	{.cmd = NSS_PPE_MCAST_CREATE_ENTRY, .doit = nss_ppenl_mcast_ops_create_mcast_entry,},	/* req create */
	{.cmd = NSS_PPE_MCAST_DELETE_ENTRY, .doit = nss_ppenl_mcast_ops_delete_mcast_entry,},	/* req create */
};

/*
 * Multicast family definition
 */
static struct genl_family nss_ppenl_mcast_family = {
	.name = NSS_PPENL_MCAST_FAMILY,	/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_mcast_req),	/* NSS NETLINK multicast req */
	.version = NSS_PPENL_VER,	/* Set it to NSS_PPENL_VER version */
	.maxattr = NSS_PPE_MCAST_MAX_MSG_TYPES,	/* maximum commands supported */
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_mcast_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_mcast_ops),
};

#define NSS_PPENL_MCAST_OPS_SZ ARRAY_SIZE(nss_ppenl_mcast_ops)

/*
 * nss_ppenl_mcast_ops_create_mcast_entry()
 * req create handler
 */
static int nss_ppenl_mcast_ops_create_mcast_entry(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_mcast_req *nl_mcast_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	enum ppe_mcast_ret pt;
	struct ppe_mcast_entry_info mcast_info = {0};

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_mcast_family, info, NSS_PPE_MCAST_CREATE_ENTRY);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract req create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling req API
	 */
	nl_mcast_req = container_of(nl_cm, struct nss_ppenl_mcast_req, cm);
	pid = nl_cm->pid;

	memcpy(&mcast_info.dev, nl_mcast_req->mc_entry.dev, sizeof(nl_mcast_req->mc_entry.dev));
	mcast_info.vlan_enabled = nl_mcast_req->mc_entry.vlan_enabled;
	mcast_info.vlan_id = nl_mcast_req->mc_entry.vlan_id;
	mcast_info.sip_enabled = nl_mcast_req->mc_entry.sip_enabled;
	mcast_info.is_v4 = nl_mcast_req->mc_entry.is_v4;
	mcast_info.sip.v4 = nl_mcast_req->mc_entry.sip.v4;
	memcpy(&mcast_info.sip.v6, nl_mcast_req->mc_entry.sip.v6, sizeof(nl_mcast_req->mc_entry.sip.v6));
	mcast_info.gip.v4 = nl_mcast_req->mc_entry.gip.v4;
	memcpy(&mcast_info.gip.v6, nl_mcast_req->mc_entry.gip.v6, sizeof(nl_mcast_req->mc_entry.gip.v6));

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

	pt = ppe_mcast_create_entry(&mcast_info);
	if (pt == PPE_MCAST_SUCCESS) {
		nss_ppenl_info("PPE multicast entry creation creation success");
	} else {
		nss_ppenl_info("creating multicast entry in ppe driver failed, error = %d", pt);
	}

	nl_mcast_req = nss_ppenl_get_data(resp);
	nl_mcast_req->ret = pt;
	nss_ppenl_ucast_resp(resp);

	return 0;
}

/*
 * nss_ppenl_mcast_ops_delete_mcast_entry()
 * req delete handler
 */
static int nss_ppenl_mcast_ops_delete_mcast_entry(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_mcast_req *nl_mcast_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error;
	enum ppe_mcast_ret pt;
	struct ppe_mcast_entry_info mcast_info = {0};

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_mcast_family, info, NSS_PPE_MCAST_DELETE_ENTRY);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract req create data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling req API
	 */
	nl_mcast_req = container_of(nl_cm, struct nss_ppenl_mcast_req, cm);
	pid = nl_cm->pid;

	memcpy(&mcast_info.dev, nl_mcast_req->mc_entry.dev, sizeof(nl_mcast_req->mc_entry.dev));
	mcast_info.vlan_enabled = nl_mcast_req->mc_entry.vlan_enabled;
	mcast_info.vlan_id = nl_mcast_req->mc_entry.vlan_id;
	mcast_info.sip_enabled = nl_mcast_req->mc_entry.sip_enabled;
	mcast_info.is_v4 = nl_mcast_req->mc_entry.is_v4;
	mcast_info.sip.v4 = nl_mcast_req->mc_entry.sip.v4;
	memcpy(&mcast_info.sip.v6, nl_mcast_req->mc_entry.sip.v6, sizeof(nl_mcast_req->mc_entry.sip.v6));
	mcast_info.gip.v4 = nl_mcast_req->mc_entry.gip.v4;
	memcpy(&mcast_info.gip.v6, nl_mcast_req->mc_entry.gip.v6, sizeof(nl_mcast_req->mc_entry.gip.v6));

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

	pt = ppe_mcast_delete_entry(&mcast_info);
	if (pt == PPE_MCAST_SUCCESS) {
		nss_ppenl_info("PPE multicast entry creation creation success");
	} else {
		nss_ppenl_info("creating multicast entry in ppe driver failed, error = %d", pt);
	}

	nl_mcast_req = nss_ppenl_get_data(resp);
	nl_mcast_req->ret = pt;
	nss_ppenl_ucast_resp(resp);

	return 0;
}

bool nss_ppenl_mcast_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE multicast handler\n");

	/*
	 * Register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_mcast_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register multicast family\n");
		return false;
	}

	return true;
}

/*
 * nss_ppenl_mcast_exit()
 * handler exit
 */
bool nss_ppenl_mcast_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink multicast handler\n");

	/*
	 * Unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_mcast_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister multicast NETLINK family\n");
		return false;
	}

	return true;
}
