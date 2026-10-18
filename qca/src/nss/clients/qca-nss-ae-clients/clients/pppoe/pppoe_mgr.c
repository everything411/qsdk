/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/etherdevice.h>
#include <linux/if_pppox.h>
#include "pppoe_mgr.h"
#include "pppoe_stats.h"
#ifdef PPPOE_MGR_FE_PPE_ENABLE
#include "ppe_pppoe_mgr.h"
#endif

struct pppoe_mgr_cmn_ctx *global;

#if defined(PPPOE_MGR_FE_PPE_ENABLE)
nss_client_mgr_fe_type_t front_end_selected = NSS_CLIENT_MGR_FE_TYPE_PPE;
#elif defined(PPPOE_MGR_FE_IPA_ENABLE)
nss_client_mgr_fe_type_t front_end_selected = NSS_CLIENT_MGR_FE_TYPE_IPA;
#else
nss_client_mgr_fe_type_t front_end_selected = NSS_CLIENT_MGR_FE_TYPE_MAX;
#endif

struct pppoe_stats_ctx ctx_gbl = {0};

/*
 * pppoe_mgr_get_session()
 *	Retrieve PPPoE session associated with this netdevice if any
 */
bool pppoe_mgr_get_session(struct net_device *dev, struct pppoe_opt *opt)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;
	struct ppp_channel *channel[1] = {NULL};
	int px_proto;
	int ppp_ch_count;

	if (ppp_is_multilink(dev)) {
		pppoe_mgr_warn("%px: channel is multilink PPP\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_get_session_multilink_ppp);
		return false;
	}

	ppp_ch_count = ppp_hold_channels(dev, channel, 1);
	pppoe_mgr_info("%px: PPP hold channel ret %d\n", dev, ppp_ch_count);
	if (ppp_ch_count != 1) {
		pppoe_mgr_warn("%px: hold channel for netdevice failed\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_get_session_hold_channel_failed);
		return false;
	}

	px_proto = ppp_channel_get_protocol(channel[0]);
	if (px_proto != PX_PROTO_OE) {
		pppoe_mgr_warn("%px: session socket is not of type PX_PROTO_OE\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_get_session_proto_get_failed);
		ppp_release_channels(channel, 1);
		return false;
	}

	if (pppoe_channel_addressing_get(channel[0], opt)) {
		pppoe_mgr_warn("%px: failed to get addressing information\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_get_session_addres_get_failed);
		ppp_release_channels(channel, 1);
		return false;
	}

	/*
	 * pppoe_channel_addressing_get returns held device.
	 * So, put it back here.
	 */
	dev_put(opt->dev);
	ppp_release_channels(channel, 1);
	return true;
}

/*
 * pppoe_mgr_init_session()
 *	Initialize PPPoE session entry.
 */
void pppoe_mgr_init_session(struct net_device *dev, struct pppoe_opt *opt, struct pppoe_mgr_session_entry *entry)

{
	struct pppoe_mgr_session_info *info;

	info = &entry->info;

	/*
	 * Get session info
	 */
	info->session_id = (uint16_t)ntohs((uint16_t)opt->pa.sid);
	ether_addr_copy(info->server_mac, opt->pa.remote);
	ether_addr_copy(info->local_mac, opt->dev->dev_addr);

	pppoe_mgr_info("%px: Added PPPoE session with session_id=%u server_mac=%pM local_mac %pM\n",
			       dev, info->session_id, info->server_mac, info->local_mac);

	entry->dev = dev;
}

/*
 * pppoe_mgr_connect()
 *	PPPoE channel connect.
 */
static int pppoe_mgr_connect(struct pppoe_mgr_cmn_ctx *ctx, struct net_device *dev)
{
	return ctx->pppoe_connect(dev);
}

/*
 * pppoe_mgr_disconnect()
 *	PPPoE channel disconnect.
 */
static int pppoe_mgr_disconnect(struct pppoe_mgr_cmn_ctx *ctx, struct net_device *dev)
{
	return ctx->pppoe_disconnect(dev);
}

/*
 * pppoe_mgr_changemtu_event()
 *	PPPoE change mtu of the PPPoE netdevice.
 */
static int pppoe_mgr_changemtu_event(struct pppoe_mgr_cmn_ctx *ctx, struct net_device *dev)
{
	return ctx->pppoe_changemtu(dev);
}

/*
 * pppoe_mgr_channel_notifier_handler()
 *	PPPoE channel notifier handler.
 */
static int pppoe_mgr_channel_notifier_handler(struct notifier_block *nb,
							unsigned long event,
							void *arg)
{
	struct pppoe_mgr_cmn_ctx *ctx = global;
	struct net_device *dev = (struct net_device *)arg;

	switch (event) {
	case PPP_CHANNEL_CONNECT:
		return pppoe_mgr_connect(ctx, dev);

	case PPP_CHANNEL_DISCONNECT:
		return pppoe_mgr_disconnect(ctx, dev);

	case NETDEV_CHANGEMTU:
		return pppoe_mgr_changemtu_event(ctx, dev);

	default:
		pppoe_mgr_info("%px: Unhandled channel event: %lu\n", dev, event);
		break;
	}

	return NOTIFY_DONE;
}

struct notifier_block pppoe_mgr_channel_notifier_nb = {
	.notifier_call = pppoe_mgr_channel_notifier_handler,
};

/*
 * pppoe_mgr_exit_module
 *	PPPoE module exit function
 */
static void __exit pppoe_mgr_exit_module(void)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;

	/*
	 * Unregister the module from the PPP channel events.
	 */
	ppp_channel_connection_unregister_notify(&pppoe_mgr_channel_notifier_nb);

	switch (front_end_selected) {
#ifdef PPPOE_MGR_FE_PPE_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_PPE:
		ppe_pppoe_mgr_exit(global);
		break;
#endif

#ifdef PPPOE_MGR_FE_IPA_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_IPA:
		/*
		 * Unregister IPA frontend
		 */
		break;
#endif
	default:
		printk(KERN_WARNING "%p %s: Frontend not supported\n", global, __func__);
		break;
	}

	/*
	 * De-initialize debugfs.
	 */
	pppoe_stats_deinit(ctx);

	kfree(global);
}

/*
 * pppoe_mgr_init_module()
 *	PPPoE module init function
 */
static int __init pppoe_mgr_init_module(void)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;

	global = (struct pppoe_mgr_cmn_ctx *)kzalloc(sizeof(struct pppoe_mgr_cmn_ctx), GFP_ATOMIC);
	if (!global) {
		printk(KERN_WARNING "%s Unable to allocate PPPoE ctx\n", __func__);
		return -1;
	}

	/*
	 * Create the debugfs directory for statistics.
	 */
	if (!pppoe_stats_init(ctx)) {
		pppoe_mgr_trace("Failed to initialize debugfs");
		return -1;
	}

	switch (front_end_selected) {
#ifdef PPPOE_MGR_FE_PPE_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_PPE:
		ppe_pppoe_mgr_init(global);
		break;
#endif

#ifdef PPPOE_MGR_FE_IPA_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_IPA:
		/*
		 * Register IPA frontend
		 */
		printk(KERN_INFO "%p %s: Frontend IPA\n", global, __func__);
		kfree(global);
		return -1;
#endif

	default:
		printk(KERN_WARNING "%p %s: Frontend not supported\n", global, __func__);
		kfree(global);
		return -1;
	}

	/*
	 * Register the module to the PPP channel events.
	 */
	ppp_channel_connection_register_notify(&pppoe_mgr_channel_notifier_nb);
	return 0;
}

module_init(pppoe_mgr_init_module);
module_exit(pppoe_mgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS pppoe manager");
