/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <linux/sysctl.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/rcupdate.h>
#include <linux/rwlock_types.h>
#include <linux/hashtable.h>
#include <net/vxlan.h>
#include <net/fib_notifier.h>
#include "nss_ppe_vxlanmgr_priv.h"
#include "nss_ppe_vxlanmgr_tun_stats.h"
#include "nss_ppe_tun_drv.h"
#include "ppe_drv_tun_cmn_ctx.h"
#include "nss_ppe_bridge_mgr.h"

/*
 * Module parameter to configure the destination port of VXLAN tunnel.
 * PPE supports single destination port for all the VXLAN tunnels.
 */
int dstport = IANA_VXLAN_UDP_PORT;
int dstport_gpe = IANA_VXLAN_GPE_UDP_PORT;

/*
 * Module parameter for ecn, dscp and ttl configuration
 */
uint8_t encap_ecn_mode = PPE_DRV_TUN_CMN_CTX_ENCAP_ECN_NO_UPDATE;
module_param(encap_ecn_mode, byte, 0644);
MODULE_PARM_DESC(encap_ecn_mode, "Encap ECN mode 0:NO_UPDATE, 1:RFC3168_LIMIT_RFC6040_CMPAT, 2:RFC3168_FULL, 3:RFC4301_RFC6040_NORMAL");

uint8_t decap_ecn_mode = PPE_DRV_TUN_CMN_CTX_DECAP_ECN_RFC3168_MODE;
module_param(decap_ecn_mode, byte, 0644);
MODULE_PARM_DESC(decap_ecn_mode, "Decap ECN mode 0:RFC3168, 1:RFC4301, 2:RFC6040");

bool inherit_dscp = false;
module_param(inherit_dscp, bool, 0644);
MODULE_PARM_DESC(inherit_dscp, "DSCP 0:Dont Inherit inner, 1:Inherit inner");

bool inherit_ttl = false;
module_param(inherit_ttl, bool, 0644);
MODULE_PARM_DESC(inherit_ttl, "TTL 0:Dont Inherit inner, 1:Inherit inner");

/*
 * VxLAN context
 */
struct nss_ppe_vxlanmgr_ctx vxlan_ctx;

/*
 * Extern variable for VXLAN fdb notifier.
 */
extern struct notifier_block nss_ppe_vxlanmgr_switchdev_fdb_notifier;

/*
 * Extern variable for fib update notifier.
 */
extern struct notifier_block nss_ppe_vxlanmgr_fib_update_nb;

/*
 * nss_ppe_vxlanmgr_netdev_event()
 *	Netdevice notifier for NSS VxLAN manager module
 */
int nss_ppe_vxlanmgr_netdev_event(struct notifier_block *nb, unsigned long event, void *dev)
{
	struct net_device *netdev = netdev_notifier_info_to_dev(dev);
	struct vxlan_dev *priv;

	/*
	 * Return if it's not a vxlan netdev
	 */
	if (!netif_is_vxlan(netdev)) {
		return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_CHANGEMTU:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_CHANGEMTU: name %s", netdev, netdev->name);
		priv = netdev_priv(netdev);
		if (priv->cfg.flags & VXLAN_F_GPE) {
			nss_ppe_vxlanmgr_gpe_all_remotes_set_mtu(netdev, netdev->mtu);
			break;
		}

		nss_ppe_vxlanmgr_all_remotes_set_mtu(netdev, netdev->mtu);
		break;

	case NETDEV_BR_LEAVE:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_BR_LEAVE: name %s", netdev, netdev->name);
		nss_ppe_vxlanmgr_all_remotes_decap_disable(netdev);
		nss_ppe_vxlanmgr_all_remotes_leave_bridge(netdev);
		break;

	case NETDEV_BR_JOIN:
		nss_ppe_vxlanmgr_trace("%px: NETDEV_BR_JOIN: name %s", netdev, netdev->name);
		nss_ppe_vxlanmgr_all_remotes_join_bridge(netdev);
		nss_ppe_vxlanmgr_all_remotes_decap_enable(netdev);
		break;

	default:
		nss_ppe_vxlanmgr_trace("%px: Unhandled notifier event %lu name %s", netdev, event, netdev->name);
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
		nss_ppe_vxlanmgr_warn("failed to disable the VXLAN tunnels.");
	}

	if (!ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE, false)) {
		nss_ppe_vxlanmgr_warn("failed to disable the VXLAN-GPE tunnels.");
	}

	ret = unregister_fib_notifier(&init_net, &nss_ppe_vxlanmgr_fib_update_nb);
	if (ret) {
		nss_ppe_vxlanmgr_warn("Failed to unregister fib notifier: error %d", ret);
	}

	ret = unregister_netdevice_notifier(&nss_ppe_vxlanmgr_netdev_notifier);
	if (ret) {
		nss_ppe_vxlanmgr_warn("failed to unregister netdevice notifier: error %d", ret);
	}

	unregister_switchdev_notifier(&nss_ppe_vxlanmgr_switchdev_fdb_notifier);
	nss_ppe_vxlanmgr_wq_exit();
	nss_ppe_vxlanmgr_gpe_wq_exit();

	/*
	 * delete_all_remotes() is called after the notifiers and workqueues are
	 * torn down to ensure no new tunnel events can be queued while destruction
	 * is in progress. delete_all_remotes() must precede tun_stats_dentry_deinit():
	 * tunnel_destroy() (called per remote) invokes tun_stats_dentry_remove(),
	 * which guards on vxlan_ctx.dentry being non-NULL; calling dentry_deinit()
	 * first would null that pointer and leave per-tunnel debugfs entries orphaned.
	 */
	nss_ppe_vxlanmgr_delete_all_remotes();

	nss_ppe_vxlanmgr_tun_stats_dentry_deinit();

	nss_ppe_vxlanmgr_info("disabled all vxlan tunnels. VXLAN module unloaded");
}

/*
 * nss_ppe_vxlanmgr_init_module()
 *	Tunnel vxlan module init function
 */
int __init nss_ppe_vxlanmgr_init_module(void)
{
	int ret;

	vxlan_ctx.nack_limit = NSS_PPE_VXLANMGR_VP_STATUS_DEFAULT_MAX_NACK;

	if ((dstport < NSS_PPE_VXLANMGR_DST_PORT_MIN) || (dstport > NSS_PPE_VXLANMGR_DST_PORT_MAX) ||
		(dstport_gpe < NSS_PPE_VXLANMGR_DST_PORT_MIN) || (dstport_gpe > NSS_PPE_VXLANMGR_DST_PORT_MAX)) {
		nss_ppe_vxlanmgr_warn("Invalid VXLAN dport:%u, dport_gpe: %u", dstport, dstport_gpe);
		return -1;
	}

	if (dstport == dstport_gpe) {
		nss_ppe_vxlanmgr_warn("VXLAN and VXLAN-GPE destination port are same");
		return -1;
	}

	if (!ppe_tun_configure_vxlan_dport(dstport)) {
		nss_ppe_vxlanmgr_warn("configuring the destination port of the VXLAN failed");
		return -1;
	}

	if (!ppe_tun_configure_vxlan_gpe_dport(dstport_gpe)) {
		nss_ppe_vxlanmgr_warn("configuring the destination port of the VXLAN-GPE failed");
		return -1;
	}

	if (nss_ppe_vxlanmgr_wq_init() < 0) {
		nss_ppe_vxlanmgr_warn("Failed to initialize work queue");
		return -1;
	}

	if (nss_ppe_vxlanmgr_gpe_wq_init() < 0) {
		nss_ppe_vxlanmgr_wq_exit();
		nss_ppe_vxlanmgr_warn("Failed to initialize VXLAN-GPE work queue");
		return -1;
	}

	if (!nss_ppe_vxlanmgr_tun_dentry_init()) {
		nss_ppe_vxlanmgr_warn("Failed to create debugfs entry");
		goto wq_exit;
	}

	ret = register_netdevice_notifier(&nss_ppe_vxlanmgr_netdev_notifier);
	if (ret) {
		nss_ppe_vxlanmgr_warn("Failed to register netdevice notifier: error %d", ret);
		goto stats_dentry_deinit;
	}

	ret = register_fib_notifier(&init_net, &nss_ppe_vxlanmgr_fib_update_nb, NULL, NULL);
	if (ret) {
		nss_ppe_vxlanmgr_warn("Failed to register fib notifier: error %d", ret);
		goto stats_dentry_deinit;
	}

	register_switchdev_notifier(&nss_ppe_vxlanmgr_switchdev_fdb_notifier);

	nss_ppe_vxlanmgr_info("Module %s loaded", NSS_PPE_BUILD_ID);

	return 0;

stats_dentry_deinit:
	nss_ppe_vxlanmgr_tun_stats_dentry_deinit();

wq_exit:
	nss_ppe_vxlanmgr_wq_exit();
	nss_ppe_vxlanmgr_gpe_wq_exit();

	return -1;
}

module_init(nss_ppe_vxlanmgr_init_module);
module_exit(nss_ppe_vxlanmgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE VxLAN manager");

module_param(dstport, int, 0644);
MODULE_PARM_DESC(dstport, "VXLAN destination port number");

module_param(dstport_gpe, int, 0644);
MODULE_PARM_DESC(dstport_gpe, "VXLAN-GPE destination port number");
