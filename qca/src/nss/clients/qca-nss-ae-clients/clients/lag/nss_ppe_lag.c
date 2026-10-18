/*
 * Copyright (c) 2016-2017, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppe_lag.c
 *	LAG Interface manager
 */
#include <linux/etherdevice.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <net/bonding.h>
#include "nss_ppe_lag_private.h"
#include <ppe_drv_lag.h>
#include <nss_ppe_vlan_mgr.h>

DEFINE_SPINLOCK(nss_ppe_lag_spinlock);
struct nss_ppe_lag_ctx gbl = {0};
struct nss_ppe_lag_bond_entry bond_entry[NSS_PPE_LAG_MAX_BOND_DEVICES];

/*
 * nss_ppe_bond_dev_get_id()
 *	Get bond_id from a bond interface
 */
int32_t nss_ppe_bond_dev_get_id(struct net_device *bond_dev)
{
	int i;
	int index = -1;

	spin_lock(&nss_ppe_lag_spinlock);

	for (i = 0; i < NSS_PPE_LAG_MAX_BOND_DEVICES; i++) {
		if (!bond_entry[i].in_use)
			continue;

		if (bond_entry[i].bond_dev == bond_dev) {
			index = i;
			break;
		}
	}

	spin_unlock(&nss_ppe_lag_spinlock);

	return index;
}

/*
 * nss_ppe_lag_update_slave()
 *	Monitor bonding slaves and send updates to hardware
 */
static int nss_ppe_lag_update_slave(struct netdev_notifier_info *info)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	struct net_device *slave_dev = netdev_notifier_info_to_dev(info);
	struct net_device *bond_dev;
	int32_t bond_id = -1;
	uint32_t i, j;
	struct nss_ppe_lag_bond_entry *entry;
	ppe_drv_ret_t ret;

	/*
	 * Notifier info points to netdev_notifier_changeupper_info structure
	 */
	struct netdev_notifier_changeupper_info *cu_info = (struct netdev_notifier_changeupper_info *)info;

	if (!cu_info->upper_dev) {
		nss_ppe_lag_trace("%px: Upper dev not present for dev: %s\n", info, slave_dev->name);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_upper_dev);
		return NOTIFY_DONE;
	}

	if (!netif_is_bond_master(cu_info->upper_dev)) {
		nss_ppe_lag_trace("%px: Upper dev is not LAG for dev: %s\n", info, slave_dev->name);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_bond_master);
		return NOTIFY_DONE;
	}

	if (!netif_is_bond_slave(slave_dev)) {
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_bond_slave);
		return NOTIFY_DONE;
	}

	if (!ppe_drv_iface_get_by_dev(slave_dev)) {
		nss_ppe_lag_warn("%px: Slave interface is unknown to PPE slave name: %s\n",
				info, slave_dev->name);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_unknown_slave_dev);
		return NOTIFY_DONE;
	}

	bond_dev = cu_info->upper_dev;
	if (cu_info->linking) {

		/*
		 * Slave is added to the LAG master
		 * Figure out the aggregation id of the bond master
		 */
		bond_id = nss_ppe_bond_dev_get_id(bond_dev);
		if ((bond_id < 0) || (bond_id >= NSS_PPE_LAG_MAX_BOND_DEVICES)) {
			nss_ppe_lag_warn("Invalid LAG group id 0x%x\n", bond_id);
			nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_lag_group_id);
			return NOTIFY_DONE;
		}

		/*
		 * bond_dev is valid when slave is Enslaved
		*/
		nss_ppe_lag_info("%px: Interface %s added in LAG ID=%d: %s\n",
				 bond_dev, slave_dev->name, bond_id, bond_dev->name);

		spin_lock(&nss_ppe_lag_spinlock);

		entry = &bond_entry[bond_id];

		for (i = 0; i < NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID; i++) {
			if (!entry->slaves[i]) {
				break;
			}
		}

		if (i == NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID) {
			spin_unlock(&nss_ppe_lag_spinlock);
			nss_ppe_lag_warn("%px: More than max %d slaves are added\n",
					bond_dev, NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID);
			nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_slave_full);
			return NOTIFY_DONE;
		}

		/*
		 * Make sure the bond device has a valid ppe interface.
		 */
		if (!entry->iface) {
			spin_unlock(&nss_ppe_lag_spinlock);
			nss_ppe_lag_warn("%px: Lag device is not a valid ppe interface\n", bond_dev);
			nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_ppe_interface);
			return NOTIFY_DONE;
		}

		entry->slaves[i] = slave_dev;
		ret = ppe_drv_lag_join(entry->iface, slave_dev);
		if (ret != PPE_DRV_RET_SUCCESS) {
			entry->slaves[i] = NULL;
			spin_unlock(&nss_ppe_lag_spinlock);
			nss_ppe_lag_warn("%px: Unable to join LAG slave in PPE\n", bond_dev);
			nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_slave_join_fail);
			return NOTIFY_DONE;
		}

		spin_unlock(&nss_ppe_lag_spinlock);

		if (nss_ppe_vlan_mgr_add_bond_slave(bond_dev, slave_dev)) {
			nss_ppe_lag_warn("%px: Adding vlan for %s dev failed\n", slave_dev, slave_dev->name);
			nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_vlan_add_fail);
		}

		return NOTIFY_DONE;
	}

	/*
	 * Slave is removed from bond
	 */
	spin_lock(&nss_ppe_lag_spinlock);
	for (i = 0; i < NSS_PPE_LAG_MAX_BOND_DEVICES; i++) {
		entry = &bond_entry[i];
		for (j = 0; j < NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID; j++) {
			if (entry->slaves[j] == slave_dev) {
				entry->slaves[j] = NULL;
				bond_id = entry->bond_id;
				break;
			}
		}

		if (bond_id != -1) {
			break;
		}
	}

	if (bond_id == -1) {
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_bond_id_invalid);
		return NOTIFY_DONE;
	}

	/*
	 * Make sure the bond device has a valid ppe interface.
	 */
	if (!entry->iface) {
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_warn("%px: Lag device is not a valid ppe interface\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_ppe_interface);
		return NOTIFY_DONE;
	}

	nss_ppe_lag_info("%px: Interface %s removed from LAG ID=%d\n", bond_dev, slave_dev->name, bond_id);

	ret = ppe_drv_lag_leave(entry->iface, slave_dev);
	if (ret != PPE_DRV_RET_SUCCESS) {
		entry->slaves[j] = slave_dev;
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_warn("%px: Unable to deinitialize LAG session in PPE\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_deinit_fail);
		return NOTIFY_DONE;
	}

	spin_unlock(&nss_ppe_lag_spinlock);

	if (nss_ppe_vlan_mgr_delete_bond_slave(slave_dev)) {
		nss_ppe_lag_warn("%px: Delete vlan for %s dev failed\n", slave_dev, slave_dev->name);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_vlan_del_fail);
	}
	return NOTIFY_DONE;
}

/*
 * nss_ppe_lag_unregister_event()
 *	Unregister LAG interface
 */
static int nss_ppe_lag_unregister_event(struct netdev_notifier_info *info)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	struct nss_ppe_lag_bond_entry *entry;
	ppe_drv_ret_t ret, ret_mac;
	int32_t bond_id;
	uint8_t i;
	struct net_device *bond_dev;

	bond_dev = netdev_notifier_info_to_dev(info);
	if (!netif_is_bond_master(bond_dev)) {
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_bond_master);
		return NOTIFY_DONE;
	}

	/*
	 * Figure out the aggregation id of this slave
	 */
	bond_id = nss_ppe_bond_dev_get_id(bond_dev);
	if ((bond_id < 0) || (bond_id >= NSS_PPE_LAG_MAX_BOND_DEVICES)) {
		nss_ppe_lag_warn("%px: Invalid LAG group id 0x%x\n", bond_dev, bond_id);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_lag_group_id);
		return NOTIFY_DONE;
	}

	spin_lock(&nss_ppe_lag_spinlock);
	entry = &bond_entry[bond_id];

	/*
	 * Make sure the bond device has a valid ppe interface.
	 */
	if (!entry->iface) {
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_warn("%px: Lag device is not a valid ppe interface\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_ppe_interface);
		return NOTIFY_DONE;
	}

	/*
	 * There may be active slaves while the lag interface is deleted.
	 * Go through the list of slaves and remove each one of them from lag.
	 */
	for (i = 0; i < NSS_PPE_LAG_MAX_SLAVES_PER_BOND_ID; i++) {
		if (entry->slaves[i]) {
			ppe_drv_lag_leave(entry->iface, entry->slaves[i]);
			entry->slaves[i] = NULL;
		}
	}

	ret_mac = ppe_drv_iface_mac_addr_clear(entry->iface);

	ret = ppe_drv_lag_deinit(entry->iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_warn("%px: Unable to deinitialize LAG session in PPE\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_deinit_fail);
		return NOTIFY_DONE;
	}

	ppe_drv_iface_deref(entry->iface);
	bond_entry[bond_id].bond_dev = NULL;
	bond_entry[bond_id].in_use = 0;
	entry->iface = NULL;
	entry->bond_id = -1;
	spin_unlock(&nss_ppe_lag_spinlock);
	if (ret_mac != PPE_DRV_RET_SUCCESS) {
		nss_ppe_lag_warn("%px: failed to clear MAC address, error = %d\n", bond_dev, ret);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_mac_clear_fail);
	}

	nss_ppe_lag_info("%px: Bond interface (%s) is destroyed=%d\n", bond_dev, bond_dev->name, bond_id);
	nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_unregister_event_success);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_bond_dev_allocate_id()
 *	Allocate bond_id for a bond interface
 */
static int32_t nss_ppe_bond_dev_allocate_id(struct net_device *bond_dev)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	int i, index = -1;

	spin_lock(&nss_ppe_lag_spinlock);

	for (i = 0; i < NSS_PPE_LAG_MAX_BOND_DEVICES; i++) {
		if (bond_entry[i].in_use) {
			if (bond_entry[i].bond_dev == bond_dev) {
				nss_ppe_lag_warn("%px: Bond interface (%s) is already registered(id = %d)\n", bond_dev, bond_dev->name, i);
				spin_unlock(&nss_ppe_lag_spinlock);
				nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_bond_interface_exist);
				return -1;
			}
			continue;
		}

		if (index == -1)
			index = i;
	}

	if (index == -1) {
		nss_ppe_lag_warn("%px: No more bond id's remaining\n", bond_dev);
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_bond_id_full);
		return index;
	}

	bond_entry[index].bond_dev = bond_dev;
	bond_entry[index].in_use = true;
	spin_unlock(&nss_ppe_lag_spinlock);
	return index;
}

/*
 * nss_ppe_lag_register_event()
 *	Register LAG interface
 */
static int nss_ppe_lag_register_event(struct netdev_notifier_info *info)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	struct nss_ppe_lag_bond_entry *entry;
	ppe_drv_ret_t ret;
	int32_t bond_id;
	struct net_device *bond_dev = netdev_notifier_info_to_dev(info);

	if (!netif_is_bond_master(bond_dev)) {
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_bond_master);
		return NOTIFY_DONE;
	}

	/*
	 * Assign bond_id to the lag interface
	 */
	bond_id = nss_ppe_bond_dev_allocate_id(bond_dev);
	if (bond_id < 0) {
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_bond_id_invalid);
		return NOTIFY_DONE;
	}

	spin_lock(&nss_ppe_lag_spinlock);
	entry = &bond_entry[bond_id];

	entry->iface = ppe_drv_iface_alloc(PPE_DRV_IFACE_TYPE_LAG, bond_dev);
	if (!entry->iface) {
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_warn("%px: LAG PPE iface alloc failed\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_iface_alloc_fail);
		return NOTIFY_DONE;
	}

	ret = ppe_drv_lag_init(entry->iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_drv_iface_deref(entry->iface);
		entry->iface = NULL;
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_warn("%px: Unable to initialize LAG session in PPE\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_init_fail);
		return NOTIFY_DONE;
	}

	entry->mtu = bond_dev->mtu;
	entry->bond_id = bond_id;
	ether_addr_copy(entry->dev_addr, bond_dev->dev_addr);
	spin_unlock(&nss_ppe_lag_spinlock);


	ret = ppe_drv_iface_mac_addr_set(entry->iface, entry->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_lag_warn("%px: failed to set mac_addr, error = %d \n", bond_dev, ret);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_mac_set_fail);
		goto fail;
	}

	ret = ppe_drv_iface_mtu_set(entry->iface, entry->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_lag_warn("%px: failed to set mtu, error = %d \n", bond_dev, ret);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_mtu_set_fail);
		goto fail2;
	}

	gbl.entry = bond_entry;
	nss_ppe_lag_info("%px: Bond interface (%s) is created=%d\n", bond_dev, bond_dev->name, bond_id);
	nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_register_event_success);
	return NOTIFY_DONE;

fail2:
	ppe_drv_iface_mac_addr_clear(entry->iface);
	nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_event_fail);

fail:
	ppe_drv_lag_deinit(entry->iface);
	ppe_drv_iface_deref(entry->iface);
	nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_event_fail);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_lag_changemtu_event()
 *     Change LAG MTU.
 */
static int nss_ppe_lag_changemtu_event(struct netdev_notifier_info *info)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	struct nss_ppe_lag_bond_entry *entry;
	ppe_drv_ret_t ret;
	int32_t bond_id;
	struct net_device *bond_dev = netdev_notifier_info_to_dev(info);
	if (!netif_is_bond_master(bond_dev)) {
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_bond_master);
		return NOTIFY_DONE;
	}

	/*
	 * Figure out the aggregation id of this slave
	 */
	bond_id = nss_ppe_bond_dev_get_id(bond_dev);
	if ((bond_id < 0) || (bond_id >= NSS_PPE_LAG_MAX_BOND_DEVICES)) {
		nss_ppe_lag_warn("%px: Invalid LAG group id 0x%x\n", bond_dev, bond_id);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_lag_group_id);
		return NOTIFY_DONE;
	}

	spin_lock(&nss_ppe_lag_spinlock);
	entry = &bond_entry[bond_id];

	if (entry->mtu == bond_dev->mtu) {
		spin_unlock(&nss_ppe_lag_spinlock);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_change_mtu_event_success);
		return NOTIFY_DONE;
	}
	spin_unlock(&nss_ppe_lag_spinlock);

	/*
	 * Make sure the bond device has a valid ppe interface.
	 */
	if (!entry->iface) {
		nss_ppe_lag_warn("%px: Lag device is not a valid ppe interface\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_ppe_interface);
		return NOTIFY_DONE;
	}

	nss_ppe_lag_info("%px: MTU changed to %d, \n", bond_dev, bond_dev->mtu);
	ret = ppe_drv_iface_mtu_set(entry->iface, bond_dev->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_lag_warn("%px: failed to set mtu, error = %d \n", bond_dev, ret);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_mtu_set_fail);
		return NOTIFY_BAD;
	}

	spin_lock(&nss_ppe_lag_spinlock);
	entry->mtu = bond_dev->mtu;
	spin_unlock(&nss_ppe_lag_spinlock);
	nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_change_mtu_event_success);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_lag_changeaddr_event()
 *	Change LAG MAC address.
 */
static int nss_ppe_lag_changeaddr_event(struct netdev_notifier_info *info)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;
	struct nss_ppe_lag_bond_entry *entry;
	ppe_drv_ret_t ret;
	int32_t bond_id;
	struct net_device *bond_dev = netdev_notifier_info_to_dev(info);
	if (!netif_is_bond_master(bond_dev)) {
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_no_bond_master);
		return NOTIFY_DONE;
	}

	/*
	 * Figure out the aggregation id of this slave
	 */
	bond_id = nss_ppe_bond_dev_get_id(bond_dev);
	if ((bond_id < 0) || (bond_id >= NSS_PPE_LAG_MAX_BOND_DEVICES)) {
		nss_ppe_lag_warn("%px: Invalid LAG group id 0x%x\n", bond_dev, bond_id);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_lag_group_id);
		return NOTIFY_DONE;
	}

	spin_lock(&nss_ppe_lag_spinlock);
	entry = &bond_entry[bond_id];
	spin_unlock(&nss_ppe_lag_spinlock);

	/*
	 * Make sure the bond device has a valid ppe interface.
	 */
	if (!entry->iface) {
		nss_ppe_lag_warn("%px: Lag device is not a valid ppe interface\n", bond_dev);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_invalid_ppe_interface);
		return NOTIFY_DONE;
	}

	ret = ppe_drv_iface_mac_addr_clear(entry->iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_lag_warn("%px: failed to clear MAC address, error = %d\n", bond_dev, ret);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_mac_clear_fail);
		return NOTIFY_DONE;
	}

	ret = ppe_drv_iface_mac_addr_set(entry->iface, (uint8_t *)bond_dev->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		nss_ppe_lag_warn("%px: failed to set mac_addr, error = %d \n", bond_dev, ret);
		nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_mac_set_fail);
		return NOTIFY_DONE;
	}

	spin_lock(&nss_ppe_lag_spinlock);
	ether_addr_copy(entry->dev_addr, bond_dev->dev_addr);
	spin_unlock(&nss_ppe_lag_spinlock);
	nss_ppe_lag_stats_inc(&ctx->stats.ppe_lag_change_addr_event_success);
	return NOTIFY_DONE;
}

/*
 * nss_ppe_lag_netdevice_event()
 *	Handle events received from network stack
 */
static int nss_ppe_lag_netdevice_event(struct notifier_block *unused,
		unsigned long event, void *ptr)
{
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;

	switch (event) {
	case NETDEV_REGISTER:
		return nss_ppe_lag_register_event(info);
	case NETDEV_UNREGISTER:
		return nss_ppe_lag_unregister_event(info);
	case NETDEV_CHANGEADDR:
		return nss_ppe_lag_changeaddr_event(info);
	case NETDEV_CHANGEMTU:
		return nss_ppe_lag_changemtu_event(info);
	}

	return NOTIFY_DONE;

}

/*
 * nss_ppe_lag_event_notifier_drv()
 *	LAG handles ppe drv operation notifications.
 */
static int nss_ppe_lag_event_notifier_drv(struct ppe_drv_notifier_ops *unused,
				int event, struct netdev_notifier_info *info)
{
	switch (event) {
	case PPE_DRV_EVENT_CHANGEUPPER:
		nss_ppe_lag_update_slave(info);
		break;
	}

	return NOTIFY_DONE;
}

/* register netdev notifier callback */
static struct notifier_block nss_ppe_lag_netdevice __read_mostly = {
	.notifier_call = nss_ppe_lag_netdevice_event,
};

/* register ppe drv netdev notifier callback */
static struct ppe_drv_notifier_ops ppe_drv_notifier_ops_lag __read_mostly = {
	.notifier_call = nss_ppe_lag_event_notifier_drv,
	.priority = PPE_DRV_NOTIFIER_PRI_1,
};

/*
 * nss_ppe_lag_exit()
 *	Cleanup NSS LAG client and exit
 */
void __exit nss_ppe_lag_exit(void)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;

	/*
	 * De-initialize debugfs.
	 */
	nss_ppe_lag_stats_deinit(ctx);

	nss_ppe_lag_dump_exit();
	unregister_netdevice_notifier(&nss_ppe_lag_netdevice);
	ppe_drv_notifier_ops_unregister(&ppe_drv_notifier_ops_lag);
	nss_ppe_lag_info("LAG Manager Removed\n");
}

/*
 * nss_ppe_lag_init()
 *	Initialize NSS LAG client
 */
int __init nss_ppe_lag_init(void)
{
	struct nss_ppe_lag_ctx *ctx = &gbl;

	/*
	 * Create the debugfs directory for LAG.
	 */
	if (!nss_ppe_lag_stats_init(ctx)) {
		nss_ppe_lag_trace("Failed to initialize debugfs\n");
		return -1;
	}

	int ret = register_netdevice_notifier(&nss_ppe_lag_netdevice);
	if (ret) {
		nss_ppe_lag_warn("Failed to register NETDEV notifier, error=%d\n", ret);
		return ret;
	}

	ppe_drv_notifier_ops_register(&ppe_drv_notifier_ops_lag);
	nss_ppe_vlan_mgr_register_bond_dev_get_id_cb(nss_ppe_bond_dev_get_id);

	nss_ppe_lag_dump_init(ctx);
	nss_ppe_lag_info("LAG Manager Installed\n");
	return ret;
}

module_init(nss_ppe_lag_init);
module_exit(nss_ppe_lag_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE LAG Client");
