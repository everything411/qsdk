/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * nss_ppe_vxlanmgr.c
 *	VxLAN netdev events
 */
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/rcupdate.h>
#include <net/vxlan.h>
#include "nss_ppe_vxlanmgr_priv.h"
#include "nss_ppe_vxlanmgr_tun_stats.h"
#include "nss_ppe_tun_drv.h"
#include "ppe_drv_tun_cmn_ctx.h"

/*
 * VxLAN context
 */
struct nss_ppe_vxlanmgr_ctx vxlan_ctx;

/*
 * nss_ppe_vxlanmgr_netdev_event()
 *	Netdevice notifier for NSS VxLAN manager module
 */
static int nss_ppe_vxlanmgr_netdev_event(struct notifier_block *nb, unsigned long event, void *dev)
{
	struct net_device *netdev = netdev_notifier_info_to_dev(dev);
	struct nss_ppe_vxlanmgr_tun_ctx *tun_ctx;

	/*
	 * Return if it's not a vxlan netdev
	 */
	if (!netif_is_vxlan(netdev)) {
		return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_DOWN:
		tun_ctx = nss_ppe_vxlanmgr_tunnel_ctx_dev_get(netdev);
		if (!tun_ctx) {
			nss_ppe_vxlanmgr_warn("%px: tun_ctx is NULL\n", netdev);
			return NOTIFY_DONE;
		}
		tun_ctx->remote_detected = false;
		nss_ppe_vxlanmgr_trace("%px: NETDEV_DOWN: event %lu name %s\n", netdev, event, netdev->name);
		ppe_tun_deconfigure(netdev);
		return NOTIFY_DONE;

	case NETDEV_UP:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_UP: event %lu name %s\n", netdev, event, netdev->name);
		return NOTIFY_DONE;

	case NETDEV_UNREGISTER:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_UNREGISTER: event %lu name %s\n", netdev, event, netdev->name);
		return nss_ppe_vxlanmgr_tunnel_destroy(netdev);

	case NETDEV_REGISTER:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_REGISTER: event %lu name %s\n", netdev, event, netdev->name);
		return nss_ppe_vxlanmgr_tunnel_create(netdev);

	case NETDEV_CHANGEMTU:
		ppe_tun_mtu_set(netdev, netdev->mtu);
		break;

	case NETDEV_BR_LEAVE:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_BR_LEAVE: event %lu name %s\n", netdev, event, netdev->name);
		if (!ppe_tun_decap_disable(netdev)) {
			nss_ppe_vxlanmgr_warn("%p: Failed disabling decap at index %s", netdev, netdev->name);
		}
		return NOTIFY_DONE;

	case NETDEV_BR_JOIN:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_BR_JOIN: event %lu name %s\n", netdev, event, netdev->name);
		if (!ppe_tun_decap_enable(netdev)) {
			nss_ppe_vxlanmgr_warn("%p: Failed enabling decap at index %s", netdev, netdev->name);
		}
		return NOTIFY_DONE;

	default:
		nss_ppe_vxlanmgr_trace("%px: Unhandled notifier event %lu name %s\n", netdev, event, netdev->name);
	}
	return NOTIFY_DONE;
}

/*
 * Linux Net device Notifier
 */
static struct notifier_block nss_ppe_vxlanmgr_netdev_notifier = {
	.notifier_call = nss_ppe_vxlanmgr_netdev_event,
};

/*
 * nss_ppe_vxlanmgr_exit_module()
 *	Tunnel vxlan module exit function
 */
void __exit nss_ppe_vxlanmgr_exit_module(void)
{
	int ret;

	if (!ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN, false)) {
		nss_ppe_vxlanmgr_warn("failed to disable the VXLAN tunnels.\n");
	}

	nss_ppe_vxlanmgr_tun_stats_dentry_deinit();

	ret = unregister_netdevice_notifier(&nss_ppe_vxlanmgr_netdev_notifier);
	if (ret) {
		nss_ppe_vxlanmgr_warn("failed to unregister netdevice notifier: error %d\n", ret);
	}

	nss_ppe_vxlanmgr_info("disabled all vxlan tunnels. VXLAN module unloaded\n");
}

/*
 * nss_ppe_vxlanmgr_init_module()
 *	Tunnel vxlan module init function
 */
int __init nss_ppe_vxlanmgr_init_module(void)
{
	int ret;

	INIT_LIST_HEAD(&vxlan_ctx.list);
	spin_lock_init(&vxlan_ctx.tun_lock);

	if (!nss_ppe_vxlanmgr_tun_stats_dentry_init()) {
		nss_ppe_vxlanmgr_warn("Failed to create debugfs entry\n");
		return -1;
	}

	ret = register_netdevice_notifier(&nss_ppe_vxlanmgr_netdev_notifier);
	if (ret) {
		nss_ppe_vxlanmgr_tun_stats_dentry_deinit();
		nss_ppe_vxlanmgr_warn("Failed to register netdevice notifier: error %d\n", ret);
		return -1;
	}

	nss_ppe_vxlanmgr_info("Module %s loaded\n", NSS_PPE_BUILD_ID);
	return 0;
}

module_init(nss_ppe_vxlanmgr_init_module);
module_exit(nss_ppe_vxlanmgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE VxLAN manager");
