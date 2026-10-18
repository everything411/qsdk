/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
static int nss_ppenl_qos_ops_create_shaper(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_delete_shaper(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_create_interface_queues(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_flush_interface_queues(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_set_interface_shaper(struct sk_buff *skb, struct genl_info *info);
#if defined(CONFIG_NSS_PPENL_PON_PORT)
static int nss_ppenl_qos_ops_get_tcont_stats(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_reset_tcont_credit(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_map_pq_to_tcont(struct sk_buff *skb, struct genl_info *info);
#endif
static int nss_ppenl_qos_ops_set_queue_tm(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_set_queue_limit(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_set_interface_queue_ctrl(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_set_ucast_prio_map(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_qos_ops_set_mcast_prio_map(struct sk_buff *skb, struct genl_info *info);

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_qos_ops[] = {
	{.cmd = NSS_PPE_QOS_GET_INT_PRI, .doit = nss_ppenl_qos_ops_get_int_pri,},	/* req int pri info */
	{.cmd = NSS_PPE_QOS_CREATE_SHAPER, .doit = nss_ppenl_qos_ops_create_shaper,},	/* create shaper profile */
	{.cmd = NSS_PPE_QOS_DELETE_SHAPER, .doit = nss_ppenl_qos_ops_delete_shaper,},	/* flush shaper profile */
	{.cmd = NSS_PPE_QOS_CREATE_INTERFACE_QUEUES, .doit = nss_ppenl_qos_ops_create_interface_queues,},	/* create port queues */
	{.cmd = NSS_PPE_QOS_FLUSH_INTERFACE_QUEUES, .doit = nss_ppenl_qos_ops_flush_interface_queues,},	/* flush port queues */
	{.cmd = NSS_PPE_QOS_SET_INTERFACE_SHAPER, .doit = nss_ppenl_qos_ops_set_interface_shaper,},	/* set interface shaper */
#if defined(CONFIG_NSS_PPENL_PON_PORT)
	{.cmd = NSS_PPE_QOS_GET_TCONT_STATS, .doit = nss_ppenl_qos_ops_get_tcont_stats,},	/* get Tcont stats */
	{.cmd = NSS_PPE_QOS_RESET_TCONT_CREDIT, .doit = nss_ppenl_qos_ops_reset_tcont_credit,},	/* reset Tcont credit */
	{.cmd = NSS_PPE_QOS_MAP_PQ_TO_TCONT, .doit = nss_ppenl_qos_ops_map_pq_to_tcont,},	/* priority queue to Tcont mapping */
#endif
	{.cmd = NSS_PPE_QOS_SET_QUEUE_TM, .doit = nss_ppenl_qos_ops_set_queue_tm,},	/* set queue traffic management */
	{.cmd = NSS_PPE_QOS_SET_QUEUE_LIMIT, .doit = nss_ppenl_qos_ops_set_queue_limit,},	/* set queue limit and thresholds */
	{.cmd = NSS_PPE_QOS_SET_INTERFACE_QUEUE_CTRL, .doit = nss_ppenl_qos_ops_set_interface_queue_ctrl,},	/* set interface queue control */
	{.cmd = NSS_PPE_QOS_SET_UCAST_PRIO_MAP, .doit = nss_ppenl_qos_ops_set_ucast_prio_map,},	/* set unicast priority map */
	{.cmd = NSS_PPE_QOS_SET_MCAST_PRIO_MAP, .doit = nss_ppenl_qos_ops_set_mcast_prio_map,},	/* set multicast priority map */
};

/*
 * QOS family definition
 */
static struct genl_family nss_ppenl_qos_family = {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0))
	.id = GENL_ID_GENERATE,	/* Auto generate ID */
#endif
	.name = NSS_PPENL_QOS_FAMILY,	/* family name string */
	.hdrsize = sizeof(struct nss_ppenl_qos_req),	/* NSS NETLINK QoS req */
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
 *	req create handler
 */
static int nss_ppenl_qos_ops_get_int_pri(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
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

	memcpy(&req.dev, nl_qos_req->msg.config.dev, sizeof(nl_qos_req->msg.config.dev));
	req.handle_id = nl_qos_req->msg.config.handle_id;
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_get_int_pri_func(&req);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE qos req success");
	} else {
		nss_ppenl_info("Input data is invalid, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.config.ret = pt;
	nl_qos_req->msg.config.int_pri = req.int_pri;
	nl_qos_req->msg.config.ucast_qid = req.ucast_qid;
	nl_qos_req->msg.config.port_id = req.port_id;

	nss_ppenl_info("Returned values from PPE driver callback are:\n"
			"int_pri = %d, \n ucast_qid = %d, \n handle_id = %d\n",
			req.int_pri, req.ucast_qid, req.handle_id);

	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_set_queue_limit()
 *	Set Queue limit and threshold
 */
static int nss_ppenl_qos_ops_set_queue_limit(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_SET_QUEUE_LIMIT);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract queue threshold data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_set_queue_limit(&nl_qos_req->msg.limit_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE queue TM info reset success");
	} else {
		nss_ppenl_info("resetting queue TM info in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.limit_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_set_queue_tm()
 *	Set Queue traffic management handler
 */
static int nss_ppenl_qos_ops_set_queue_tm(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_SET_QUEUE_TM);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract queue TM data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_set_queue_tm(&nl_qos_req->msg.tm_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE queue TM info set success");
	} else {
		nss_ppenl_info("resetting queue TM info in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.tm_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

#if defined(CONFIG_NSS_PPENL_PON_PORT)
/*
 * nss_ppenl_qos_ops_get_tcont_stats()
 *	Get Tcont stats request
 */
static int nss_ppenl_qos_ops_get_tcont_stats(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;
	struct ppe_qos_tcont_stats_info stats = {0};

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_GET_TCONT_STATS);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract stats request data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling req API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	stats.tcont_id = nl_qos_req->msg.stats_info.tcont_id;

	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_get_tcont_stats(&stats);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE qos stats req success for Tcont ID:%d", stats.tcont_id);
	} else {
		nss_ppenl_info("Input data is invalid for Tcont:%d, error = %d", stats.tcont_id, pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.stats_info.ret = pt;
	nl_qos_req->msg.stats_info.pend_bytes = stats.pend_bytes;
	nl_qos_req->msg.stats_info.credit = stats.credit;

	nss_ppenl_info("Tcont ID:%d pending bytes:%llu credit:%d:\n",
			stats.tcont_id, stats.pend_bytes, stats.credit);

	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_reset_tcont_credit()
 *	Reset given Tcont credit to 0.
 */
static int nss_ppenl_qos_ops_reset_tcont_credit(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_RESET_TCONT_CREDIT);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract Tcont data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_reset_tcont_credit(&nl_qos_req->msg.stats_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE reset Tcont credit success for Tcont ID:%d", nl_qos_req->msg.stats_info.tcont_id);
	} else {
		nss_ppenl_info("Reset Tcont:%d credit in ppe driver failed, error = %d", nl_qos_req->msg.stats_info.tcont_id, pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.stats_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_map_pq_to_tcont()
 * 	Map priority queue to Tcont handler
 */
static int nss_ppenl_qos_ops_map_pq_to_tcont(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_MAP_PQ_TO_TCONT);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract queue TM data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_map_pq_to_tcont(&nl_qos_req->msg.pq_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE priority queue mapping success");
	} else {
		nss_ppenl_info("mapping PQ to tcont in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.pq_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}
#endif

/*
 * nss_ppenl_qos_ops_set_interface_shaper()
 *	Create port queues handler
 */
static int nss_ppenl_qos_ops_set_interface_shaper(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_SET_INTERFACE_SHAPER);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract interface shaper data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_set_interface_shaper(&nl_qos_req->msg.if_shaper_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("interface shaper set success");
	} else {
		nss_ppenl_info("setting interface shaperfailed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.if_shaper_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_delete_shaper()
 *	Delete shaper profile
 */
static int nss_ppenl_qos_ops_delete_shaper(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_DELETE_SHAPER);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract shaper data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_delete_shaper(&nl_qos_req->msg.shaper_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE shaper info reset success");
	} else {
		nss_ppenl_info("resetting shaper info in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.shaper_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_create_shaper()
 *	Create shaper profile
 */
static int nss_ppenl_qos_ops_create_shaper(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_CREATE_SHAPER);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract shaper data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_create_shaper(&nl_qos_req->msg.shaper_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE shaper info set success");
	} else {
		nss_ppenl_info("setting shaper info in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.shaper_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_flush_interface_queues()
 *	Delete port queues handler
 */
static int nss_ppenl_qos_ops_flush_interface_queues(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_FLUSH_INTERFACE_QUEUES);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract queue TM data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_flush_interface_queues(&nl_qos_req->msg.if_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE port queues flush success");
	} else {
		nss_ppenl_info("flushing port queues in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.if_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_create_interface_queues()
 *	Create port queues handler
 */
static int nss_ppenl_qos_ops_create_interface_queues(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_CREATE_INTERFACE_QUEUES);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract port queues data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling rule API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_create_interface_queues(&nl_qos_req->msg.if_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE port queues creation success");
	} else {
		nss_ppenl_info("creating port queues in ppe driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.if_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_set_interface_queue_ctrl()
 *	Set interface queue control (enqueue/dequeue enable/disable)
 */
static int nss_ppenl_qos_ops_set_interface_queue_ctrl(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_SET_INTERFACE_QUEUE_CTRL);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract queue control data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_set_interface_queue_ctrl(&nl_qos_req->msg.queue_ctrl_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE interface queue control set success");
	} else {
		nss_ppenl_info("setting interface queue control failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.queue_ctrl_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_ops_set_ucast_prio_map()
 *	Set unicast priority map
 */
static int nss_ppenl_qos_ops_set_ucast_prio_map(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_SET_UCAST_PRIO_MAP);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract unicast priority map data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_set_ucast_prio_map(&nl_qos_req->msg.ucast_prio_map_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE unicast priority map set success");
	} else {
		nss_ppenl_info("Setting unicast priority map in PPE driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.ucast_prio_map_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}


/*
 * nss_ppenl_qos_ops_set_mcast_prio_map()
 *	Set multicast priority map
 */
static int nss_ppenl_qos_ops_set_mcast_prio_map(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_qos_req *nl_qos_req;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	enum ppe_qos_ret pt;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_qos_family, info, NSS_PPE_QOS_SET_MCAST_PRIO_MAP);
	if (!nl_cm) {
		nss_ppenl_info("unable to extract multicast priority map data\n");
		nss_ppenl_ucast_resp(skb);
		return -EINVAL;
	}

	/*
	 * Validate config message before calling API
	 */
	nl_qos_req = container_of(nl_cm, struct nss_ppenl_qos_req, cm);
	pid = nl_cm->pid;

	/*
	 * copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_info("%d:unable to save response data from NL buffer\n", pid);
		nss_ppenl_ucast_resp(skb);
		return -ENOMEM;
	}

	pt = ppe_qos_set_mcast_prio_map(&nl_qos_req->msg.mcast_prio_map_info);
	if (pt == PPE_QOS_SUCCESS) {
		nss_ppenl_info("PPE multicast priority map set success");
	} else {
		nss_ppenl_info("Setting multicast priority map in PPE driver failed, error = %d", pt);
	}

	nl_qos_req = nss_ppenl_get_data(resp);
	nl_qos_req->msg.mcast_prio_map_info.ret = pt;
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_qos_init()
 *	handler init
 */
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
 *	handler exit
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
