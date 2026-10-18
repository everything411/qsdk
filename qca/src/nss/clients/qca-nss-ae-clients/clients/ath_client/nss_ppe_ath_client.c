/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppe_ath_client.c
 *	NSS PPE ATH Client
 */
#include <linux/of.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/if_vlan.h>
#include <linux/nl80211.h>
#include <net/cfg80211.h>
#include <ppe_drv_public.h>
#include <ppe_vp_public.h>
#include <ppe_vp_tx.h>
#include "nss_ppe_ath_client.h"

/*
 * Default CPU core used for WLAN Active VP queue mapping.
 * Queue for this core is set by DP driver via ppe_drv_core2queue_mapping().
 */
#define NSS_PPE_ATH_CLIENT_WLAN_DEFAULT_CORE	0

/*
 * offload ops for netdev
 */
static struct netdev_hw_offload_ops ppe_vp_netdev_ops = {
	.recv = ppe_vp_tx_to_ppe_by_dev,
	.xmit = NULL,
};

/*
 * nss_ppe_ath_client_changeaddr_event()
 *	Change netdev MAC address.
 */
static int nss_ppe_ath_client_changeaddr_event(struct net_device *dev)
{
	int32_t vp_num;

	vp_num = ppe_drv_port_num_from_dev(dev);
	if (unlikely((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END))) {
		nss_ppe_ath_client_warn("Not a valid Virtual Port number %d dev %s\n", vp_num, dev->name);
		return NOTIFY_DONE;
	}

	ppe_vp_mac_addr_set(vp_num, (uint8_t *)dev->dev_addr);
	nss_ppe_ath_client_info("Dev: %s Change address\n", dev->name);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_ath_client_changemtu_event()
 *     Change netdev MTU.
 */
static int nss_ppe_ath_client_changemtu_event(struct net_device *dev)
{
	int32_t vp_num;

	vp_num = ppe_drv_port_num_from_dev(dev);
	if (unlikely((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END))) {
		nss_ppe_ath_client_warn("Not a valid Virtual Port number %d dev %s\n", vp_num, dev->name);
		return NOTIFY_DONE;
	}

	ppe_vp_mtu_set(vp_num, dev->mtu);
	nss_ppe_ath_client_info("Dev: %s Change MTU new mtu: %u\n", dev->name, dev->mtu);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_ath_client_register_event()
 *	ath_client handles netdev registration notification.
 */
static int nss_ppe_ath_client_register_event(struct net_device *dev)
{
	struct ppe_vp_ai vpai;
	int32_t vp_num;

	memset(&vpai, 0, sizeof(struct ppe_vp_ai));
	vpai.usr_type = PPE_VP_USER_TYPE_NONE;
	vpai.type = PPE_VP_TYPE_SW_L2;
	vpai.net_dev_type = PPE_VP_NET_DEV_TYPE_WIFI;
	vpai.core_mask = 7;
	vpai.queue_num = ppe_drv_queue_from_core(NSS_PPE_ATH_CLIENT_WLAN_DEFAULT_CORE);

	dev_hold(dev);
	vp_num = ppe_vp_alloc(dev, &vpai);
	if (unlikely((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END))) {
		nss_ppe_ath_client_warn("Not a valid Virtual Port number %d dev %s\n", vp_num, dev->name);
		dev_put(dev);
		return NOTIFY_DONE;
	}

	netdev_hw_offload_ops_register(dev, &ppe_vp_netdev_ops, "ath_client");

	nss_ppe_ath_client_info("Dev: %s VP_NUM: %d registration success\n", dev->name, vp_num);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_ath_client_unregister_event()
 *	ath_client handles netdev unregistration notification.
 */
static int nss_ppe_ath_client_unregister_event(struct net_device *dev)
{
	int32_t vp_num;

	vp_num = ppe_drv_port_num_from_dev(dev);
	if (unlikely((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END))) {
		nss_ppe_ath_client_warn("Not a valid Virtual Port number %d dev %s\n", vp_num, dev->name);
		return NOTIFY_DONE;
	}

	netdev_hw_offload_ops_unregister(dev, &ppe_vp_netdev_ops);
	synchronize_net();
	ppe_vp_free(vp_num);

	/*
	 * release the reference taken during register
	 */
	dev_put(dev);

	nss_ppe_ath_client_info("Dev: %s Unregistration done\n", dev->name);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_ath_client_netdevice_event()
 *	ath_client handles netdev operation notifications.
 */
static int nss_ppe_ath_client_netdevice_event(struct notifier_block *unused,
				unsigned long event, void *ptr)
{
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;
	struct net_device *dev = netdev_notifier_info_to_dev(info);
	struct net_device *org_dev = dev;

	dev = is_vlan_dev(dev) ? vlan_dev_real_dev(dev) : dev;
	if (!dev->ieee80211_ptr) {
		nss_ppe_ath_client_info("Dev: %s not a WLAN dev\n", dev->name);
		return NOTIFY_DONE;
	}

	switch (event) {
	case NETDEV_CHANGEADDR:
		return nss_ppe_ath_client_changeaddr_event(dev);
	case NETDEV_CHANGEMTU:
		return nss_ppe_ath_client_changemtu_event(dev);
	case NETDEV_REGISTER:

		/*
		 * Hold reference of real_dev for vlan dev event
		 */
		if (is_vlan_dev(org_dev)) {
			dev_hold(dev);
			nss_ppe_ath_client_info("VLAN Dev:%s of WLAN dev:%s\n",
							org_dev->name, dev->name);
			return NOTIFY_DONE;
		}
		return nss_ppe_ath_client_register_event(dev);
	case NETDEV_UNREGISTER:

		/*
		 * Release reference of real_dev for vlan dev unreg event.
		 */
		if (is_vlan_dev(org_dev)) {
			nss_ppe_ath_client_info("VLAN Dev:%s of WLAN dev:%s\n",
							org_dev->name, dev->name);
			dev_put(dev);
			return NOTIFY_DONE;
		}
		return nss_ppe_ath_client_unregister_event(dev);
	}

	/*
	 * Notify done for all the events we don't care
	 */
	return NOTIFY_DONE;
}

/*
 * nss_ppe_ath_client_netdevice_nb()
 *	ath_client netdevice event notifier.
 */
static struct notifier_block nss_ppe_ath_client_netdevice_nb __read_mostly = {
	.notifier_call = nss_ppe_ath_client_netdevice_event,
};

/*
 * nss_ppe_ath_client_exit_module()
 *	ath_client module exit function
 */
static void __exit nss_ppe_ath_client_exit_module(void)
{
	unregister_netdevice_notifier(&nss_ppe_ath_client_netdevice_nb);
	nss_ppe_ath_client_info("PPE ath_client Module unloaded\n");
}

/*
 * nss_ppe_ath_client_init_module()
 *	ath_client module init function
 */
static int __init nss_ppe_ath_client_init_module(void)
{
	register_netdevice_notifier(&nss_ppe_ath_client_netdevice_nb);
	nss_ppe_ath_client_info("PPE ath_client Module (Build %s) loaded\n", NSS_PPE_BUILD_ID);

	return 0;
}

module_init(nss_ppe_ath_client_init_module);
module_exit(nss_ppe_ath_client_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE Clients");
