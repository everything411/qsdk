/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/inetdevice.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <net/gre.h>
#include "gre_mgr.h"
#ifdef GRE_MGR_FE_PPE_ENABLE
#include "ppe_gre_mgr.h"
#endif
#include <nss_client_mgr.h>

static struct gre_mgr_cmn_ctx *global;

#if defined(GRE_MGR_FE_PPE_ENABLE)
nss_client_mgr_fe_type_t front_end_selected = NSS_CLIENT_MGR_FE_TYPE_PPE;
#elif defined(GRE_MGR_FE_IPA_ENABLE)
nss_client_mgr_fe_type_t front_end_selected = NSS_CLIENT_MGR_FE_TYPE_IPA;
#else
nss_client_mgr_fe_type_t front_end_selected = NSS_CLIENT_MGR_FE_TYPE_MAX;
#endif

/*
 * gre_mgr_netdev_register()
 * 	Handle NETDEV_REGISTER event in the frontend selected
 */
bool gre_mgr_netdev_register(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	if (!gre_ctx->netdev_register(netdev, gre_tun_type)) {
		gre_mgr_warning("%px: Netdev %s not able to register \n", netdev, netdev->name);
		return false;
	}

	return true;
}

/*
 * gre_mgr_netdev_unregister()
 * 	Handle NETDEV_UNREGISTER event in the frontend selected
 */
void gre_mgr_netdev_unregister(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev)
{
	gre_ctx->netdev_unregister(netdev);
	gre_mgr_warning("%px: Netdev %s unregistered \n", netdev, netdev->name);
}

/*
 * gre_mgr_netdev_up()
 * 	Handle NETDEV_UP event in the frontend selected
 */
bool gre_mgr_netdev_up(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	if (!gre_ctx->netdev_up(netdev, gre_tun_type)) {
		gre_mgr_warning("%px: Netdev %s up event failed \n", netdev, netdev->name);
		return false;
	}
	return true;
}

/*
 * gre_mgr_netdev_down()
 * 	Handle NETDEV_DOWN event in the frontend selected
 */
void gre_mgr_netdev_down(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev)
{
	gre_ctx->netdev_down(netdev);
	gre_mgr_warning("%px: Netdev %s is down\n", netdev, netdev->name);
}

/*
 * gre_mgr_netdev_changemtu()
 * 	Hanlde MTU change event in the frontend selected
 */
bool gre_mgr_netdev_changemtu(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev)
{
	if (!gre_ctx->netdev_changemtu(netdev)) {
		gre_mgr_warning("%px: Netdev %s changemtu event failed \n", netdev, netdev->name);
		return false;
	}
	return true;
}

/*
 * gre_mgr_netdev_brleave()
 * 	bridge leave event relevant only for GRETAP netdevices
 */
bool gre_mgr_netdev_brleave(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	if (gre_tun_type != GRE_MGR_TUN_CMN_CTX_TYPE_GRETAP) {
		return false;
	}

	if (!gre_ctx->netdev_bridgeleave(netdev)) {
		gre_mgr_warning("%px: Netdev %s bridge leave event failed \n", netdev, netdev->name);
		return false;
	}

	return true;
}

/*
 * gre_mgr_netdev_brjoin()
 * 	bridge join event relevant only for GRETAP netdevices
 */
bool gre_mgr_netdev_brjoin(struct gre_mgr_cmn_ctx *gre_ctx, struct net_device *netdev, enum gre_mgr_cmn_ctx_type gre_tun_type)
{
	if (gre_tun_type != GRE_MGR_TUN_CMN_CTX_TYPE_GRETAP) {
		return false;
	}

	if (!gre_ctx->netdev_bridgejoin(netdev)) {
		gre_mgr_warning("%px: Netdev %s bridge join event failed\n", netdev, netdev->name);
		return false;
	}

	return true;
}

/*
 * gre_mgr_dev_event()
 * 	Net device notifier for GRE Tunnel
 */
static int gre_mgr_dev_event(struct notifier_block *nb, unsigned long event, void *info)
{
	struct gre_mgr_cmn_ctx *gre_ctx = global;
	struct net_device *netdev = netdev_notifier_info_to_dev(info);
	enum gre_mgr_cmn_ctx_type gre_tun_type;

	if (gre_tunnel_is_fallback_dev(netdev) || gre6_tunnel_is_fallback_dev(netdev)) {
		gre_mgr_warning("%p: GRE tunnel creation skipped for fb dev %s\n", netdev, netdev->name);
		return NOTIFY_DONE;
	}

	if (netif_is_ip6gretap(netdev) || netif_is_gretap(netdev)) {
		gre_tun_type = GRE_MGR_TUN_CMN_CTX_TYPE_GRETAP;
	} else if ((netdev->type == ARPHRD_IPGRE) || (netdev->type == ARPHRD_IP6GRE)) {
		gre_tun_type = GRE_MGR_TUN_CMN_CTX_TYPE_GRETUN;
	} else {
		return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_REGISTER:
		gre_mgr_netdev_register(gre_ctx, netdev, gre_tun_type);
		break;

	case NETDEV_UNREGISTER:
		gre_mgr_netdev_unregister(gre_ctx, netdev);
		break;

	case NETDEV_UP:
		gre_mgr_netdev_up(gre_ctx, netdev, gre_tun_type);
		break;

	case NETDEV_DOWN:
		gre_mgr_netdev_down(gre_ctx, netdev);
		break;

	case NETDEV_CHANGEMTU:
		gre_mgr_netdev_changemtu(gre_ctx, netdev);
		break;

	/*
	 * Bridge leave / join events are relevant only for GRETAP netdevices and
	 * not for GRETUN device.
	 */
	case NETDEV_BR_LEAVE:
		gre_mgr_netdev_brleave(gre_ctx, netdev, gre_tun_type);
		break;

	case NETDEV_BR_JOIN:
		gre_mgr_netdev_brjoin(gre_ctx, netdev, gre_tun_type);
		break;

	default:
		gre_mgr_trace("%px: Unhandled notifier dev %s event %x\n", netdev, netdev->name, (int)event);
		break;
	}

	return NOTIFY_DONE;
}

/*
 * Linux Net device Notifier
 */
struct notifier_block gre_mgr_netdevice_nb = {
	.notifier_call = gre_mgr_dev_event,
};

/*
 * gre_mgr_init_module()
 * 	GRE client module init function
 */
static int __init gre_mgr_init_module(void)
{
	global = (struct gre_mgr_cmn_ctx *)kzalloc(sizeof(struct gre_mgr_cmn_ctx), GFP_ATOMIC);
	if (!global) {
		gre_mgr_warning("Unable to allocate gre ctx\n");
		return -1;
	}

	switch (front_end_selected) {
#ifdef GRE_MGR_FE_PPE_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_PPE:
		if (!ppe_gre_mgr_init(global)) {
			gre_mgr_warning("%p Unable to register PPE frontend\n", global);
			kfree(global);
			return -1;
		}
		break;
#endif
#ifdef GRE_MGR_FE_IPA_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_IPA:
		/*
		 * Register IPA frontend
		 */
		gre_mgr_warning("%p: Frontend IPA\n", global);
		kfree(global);
		return -1;
#endif
	default:
		gre_mgr_warning("%p: Frontend for GRE is not supported\n", global);
		kfree(global);
		return -1;
	}

	register_netdevice_notifier(&gre_mgr_netdevice_nb);
	gre_mgr_info("GRE Client Manager registered\n");
	return 0;
}

/*
 * gre_mgr_exit_module()
 * 	GRE client module exit function
 */
static void __exit gre_mgr_exit_module(void)
{
	/*
	 * Unregister net device notification
	 */
	unregister_netdevice_notifier(&gre_mgr_netdevice_nb);

	switch (front_end_selected) {
#ifdef GRE_MGR_FE_PPE_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_PPE:
		ppe_gre_mgr_exit(global);
		break;
#endif
#ifdef GRE_MGR_FE_IPA_ENABLE
	case NSS_CLIENT_MGR_FE_TYPE_IPA:
		/*
		 * Unregister IPA frontend
		 */
		break;
#endif
	default:
		gre_mgr_warning("%p: Frontend for GRE is not supported\n", global);
	}

	kfree(global);
	gre_mgr_info("GRE Manager module unloaded\n");
}

module_init(gre_mgr_init_module);
module_exit(gre_mgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("GRE MGR Client Driver");
