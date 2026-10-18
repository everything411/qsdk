/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/if_pppox.h>
#include <net/ip.h>
#include <net/bonding.h>
#include <ppe_drv.h>
#include <ppe_drv_pppoe_session.h>
#include "pppoe_mgr.h"
#include "pppoe_stats.h"
#include "ppe_pppoe_mgr.h"

#define HASH_BUCKET_SIZE 2  /* ( 2^ HASH_BUCKET_SIZE ) == 4 */

/*
 * Store all the PPPoE session.
 */
static DEFINE_HASHTABLE(ppe_pppoe_session_table, HASH_BUCKET_SIZE);
static DEFINE_SPINLOCK(ppe_pppoe_lock);

/*
 * struct ppe_pppoe_mgr_session_entry
 *	Structure for PPPoE session entry into HASH table
 */
struct ppe_pppoe_mgr_session_entry {
	struct pppoe_mgr_session_entry pppoe_pvt;	/* Base PPPoE-mgr private instance */
	struct ppe_drv_iface *iface;			/* PPE PPPoE iface */
	struct hlist_node session_list;			/* Hash list for sessions */
};

/*
 * ppe_pppoe_mgr_add_session()
 *	Add PPPoE session from the hash table.
 */
static void ppe_pppoe_mgr_add_session(struct ppe_pppoe_mgr_session_entry *ppe_entry, struct net_device *dev)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;

	/*
	 * There is no need for protecting simultaneous addition &
	 * deletion of PPPoE sesion entry as the PPP notifier chain
	 * call back is called with mutex lock.
	 */
	hash_add_rcu(ppe_pppoe_session_table,
		&ppe_entry->session_list,
		dev->ifindex);

	pppoe_stats_inc(&ctx->stats.pppoe_session_add_success);
}

/*
 * ppe_pppoe_mgr_remove_session()
 *	Remove PPPoE session from the hash table.
 */
static void ppe_pppoe_mgr_remove_session(struct ppe_pppoe_mgr_session_entry *ppe_entry)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;
	struct pppoe_mgr_session_entry *entry = &ppe_entry->pppoe_pvt;
	struct pppoe_mgr_session_info *info __maybe_unused = &entry->info;

	pppoe_mgr_info("%px: Remove PPPoE session with session_id=%u server_mac=%pM local_mac %pM\n",
				   entry, info->session_id, info->server_mac, info->local_mac);

	pppoe_stats_inc(&ctx->stats.pppoe_session_remove_success);
	hash_del_rcu(&ppe_entry->session_list);
}

/*
 * ppe_pppoe_mgr_disconnect()
 *	PPPoE interface's disconnect event handler
 */
static int ppe_pppoe_mgr_disconnect(struct net_device *dev)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;
	struct ppe_pppoe_mgr_session_entry *ppe_entry;
	bool found = false;
	struct pppoe_mgr_session_entry *entry;
	struct hlist_node *temp;
	ppe_drv_ret_t ret;
	struct ppe_drv_iface *iface;

	/*
	 * check whether the interface is of type PPP
	 */
	if (dev->type != ARPHRD_PPP || !(dev->flags & IFF_POINTOPOINT)) {
		return NOTIFY_DONE;
	}

	spin_lock(&ppe_pppoe_lock);
	hash_for_each_possible_safe(ppe_pppoe_session_table, ppe_entry,
				     temp, session_list, dev->ifindex) {
		entry = &ppe_entry->pppoe_pvt;
		if (entry->dev != dev) {
			continue;
		}

		/*
		 * In the hash list, there must be only one entry match with this net device.
		 * delete the entry after finding it
		 */
		iface = ppe_entry->iface;
		found = true;
		ppe_pppoe_mgr_remove_session(ppe_entry);
		break;
	}
	spin_unlock(&ppe_pppoe_lock);

	if (!found) {
		pppoe_mgr_warn("%px: PPPoE session is not found for device: %s\n", dev, dev->name);
		pppoe_stats_inc(&ctx->stats.pppoe_session_not_found);
		return NOTIFY_DONE;
	}

	synchronize_rcu();
	ret = ppe_drv_pppoe_session_deinit(iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pppoe_mgr_warn("%px: Unable to deinitialize PPPoE session in PPE\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_session_deinit_failure);
	}

	ppe_drv_iface_deref(iface);
	pppoe_mgr_minidump_free(ppe_entry, "ppe_pppoe_mgr_session_entry");
	kfree(ppe_entry);
	pppoe_stats_inc(&ctx->stats.pppoe_disconnect_event_success);
	return NOTIFY_DONE;
}

/*
 * ppe_pppoe_mgr_connect()
 *	PPPoE interface's connect event handler
 */
static int ppe_pppoe_mgr_connect(struct net_device *dev)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;
	struct pppoe_opt opt;
	struct pppoe_mgr_session_entry *entry = NULL;
	struct ppe_pppoe_mgr_session_entry *ppe_entry = NULL;
	struct pppoe_mgr_session_info *info;
	struct net_device *actual_dev = dev;
	ppe_drv_ret_t ret;
	struct ppe_drv_iface *iface;

	/*
	 * check whether the interface is of type PPP
	 */
	if (dev->type != ARPHRD_PPP || !(dev->flags & IFF_POINTOPOINT)) {
		return NOTIFY_DONE;
	}

	if (!pppoe_mgr_get_session(dev, &opt)) {
		pppoe_mgr_warn("%px: Unable to get PPPoE session from the netdev\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_get_session_failure);
		return NOTIFY_DONE;
	}

	/*
	 * get_session releases the reference on opt.dev internally.
	 * Take a new reference to keep opt.dev valid throughout this handler.
	 */
	dev_hold(opt.dev);

	iface = ppe_drv_iface_alloc(PPE_DRV_IFACE_TYPE_PPPOE, dev);
	if (!iface) {
		pppoe_mgr_warn("%px: PPPoE PPE iface alloc failed\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_iface_alloc_failure);
		dev_put(opt.dev);
		return NOTIFY_DONE;
	}

	ppe_entry = (struct ppe_pppoe_mgr_session_entry *)kzalloc(sizeof(struct ppe_pppoe_mgr_session_entry),
				GFP_KERNEL);
	if (!ppe_entry) {
		ppe_drv_iface_deref(iface);
		pppoe_mgr_warn("%px: failed to allocate PPE PPPoE session entry\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_add_session_failure);
		dev_put(opt.dev);
		return NOTIFY_DONE;
	}

	pppoe_mgr_minidump_log(ppe_entry, sizeof(struct ppe_pppoe_mgr_session_entry), "ppe_pppoe_mgr_session_entry");

	pppoe_mgr_init_session(dev, &opt, &ppe_entry->pppoe_pvt);
	ppe_entry->iface = iface;
	spin_lock(&ppe_pppoe_lock);
	ppe_pppoe_mgr_add_session(ppe_entry, dev);
	spin_unlock(&ppe_pppoe_lock);

	entry = &ppe_entry->pppoe_pvt;
	info = &entry->info;

	/*
	 * PPPoE connection could be on a bridge device like br-wan or on bond device like bond0.
	 * So, check if the opt device is bridge or bond.
	 */
	actual_dev = opt.dev;
	if (netif_is_bond_master(opt.dev)) {
		int32_t bondid = -1;
#if defined(BONDING_SUPPORT)
		bondid = bond_get_id(opt.dev);
#endif
		if (bondid < 0) {
			pppoe_mgr_warn("%px: Invalid LAG group id 0x%x\n", dev, bondid);
			pppoe_stats_inc(&ctx->stats.pppoe_invalid_lag_group_id);
			goto fail;
		}
	}

	ret = ppe_drv_pppoe_session_init(ppe_entry->iface, actual_dev, info->session_id, info->server_mac, info->local_mac);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pppoe_mgr_warn("%px: Unable to initialize PPPoE session in PPE\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_session_init_failure);
		goto fail;
	}

	ret = ppe_drv_iface_mtu_set(ppe_entry->iface, actual_dev->mtu - PPPOE_SES_HLEN);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pppoe_mgr_warn("%px: failed to set mtu, error = %d \n", dev, ret);
		pppoe_stats_inc(&ctx->stats.pppoe_iface_mtu_set_failure);
		goto fail2;
	}

	entry->mtu = dev->mtu;

	pppoe_mgr_info("%px: session_id %d server_mac %pM local_mac %pM base_if %s\n",
			       dev, info->session_id,
			       info->server_mac, info->local_mac, opt.dev->name);

	pppoe_stats_inc(&ctx->stats.pppoe_connect_event_success);
	dev_put(opt.dev);
	return NOTIFY_DONE;

fail2:
	ret = ppe_drv_pppoe_session_deinit(ppe_entry->iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pppoe_mgr_warn("%px: Unable to deinitialize PPPoE session in PPE\n", dev);
		pppoe_stats_inc(&ctx->stats.pppoe_session_deinit_failure);
	}

fail:
	ppe_drv_iface_deref(iface);
	spin_lock(&ppe_pppoe_lock);
	ppe_pppoe_mgr_remove_session(ppe_entry);
	spin_unlock(&ppe_pppoe_lock);
	synchronize_rcu();
	kfree(ppe_entry);
	dev_put(opt.dev);
	return NOTIFY_DONE;
}

/*
 * ppe_pppoe_mgr_changemtu_event()
 *	Change PPPoE MTU.
 */
static int ppe_pppoe_mgr_changemtu_event(struct net_device *dev)
{
	struct pppoe_stats_ctx *ctx = &ctx_gbl;
	bool found = false;
	struct ppe_pppoe_mgr_session_entry *ppe_entry;
	struct pppoe_mgr_session_entry *entry;
	struct hlist_node *temp;
	ppe_drv_ret_t ret;

	/*
	 * check whether the interface is of type PPP
	 */
	if (dev->type != ARPHRD_PPP || !(dev->flags & IFF_POINTOPOINT)) {
		return NOTIFY_DONE;
	}

	spin_lock(&ppe_pppoe_lock);
	hash_for_each_possible_safe(ppe_pppoe_session_table, ppe_entry,
				     temp, session_list, dev->ifindex) {
		entry = &ppe_entry->pppoe_pvt;
		if (entry->dev != dev) {
			continue;
		}

		/*
		 * In the hash list, there must be only one entry match with this net device.
		 */
		found = true;
		break;
	}
	spin_unlock(&ppe_pppoe_lock);

	if (!found) {
		pppoe_mgr_warn("%px: PPPoE session is not found for device: %s\n", dev, dev->name);
		pppoe_stats_inc(&ctx->stats.pppoe_session_not_found);
		return NOTIFY_DONE;
	}

	entry = &ppe_entry->pppoe_pvt;
	if (entry->mtu == dev->mtu) {
		return NOTIFY_DONE;
	}

	pppoe_mgr_info("%px: MTU changed to %d, \n", dev, dev->mtu);
	ret = ppe_drv_iface_mtu_set(ppe_entry->iface, dev->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		pppoe_mgr_warn("%px: failed to set mtu, error = %d \n", dev, ret);
		pppoe_stats_inc(&ctx->stats.pppoe_iface_mtu_set_failure);
		return NOTIFY_BAD;
	}

	entry->mtu = dev->mtu;
	return NOTIFY_DONE;
}

/*
 * ppe_pppoe_mgr_exit()
 *	Prepare for PPPoE module unload
 */
void ppe_pppoe_mgr_exit(struct pppoe_mgr_cmn_ctx *pppoe_ctx)
{
	memset(pppoe_ctx, 0, sizeof(struct pppoe_mgr_cmn_ctx));
}

/*
 * ppe_pppoe_mgr_ctx_init()
 *	Prepare for PPPoE module load
 */
void ppe_pppoe_mgr_init(struct pppoe_mgr_cmn_ctx *pppoe_ctx)
{
	pppoe_ctx->pppoe_connect = ppe_pppoe_mgr_connect;
	pppoe_ctx->pppoe_disconnect = ppe_pppoe_mgr_disconnect;
	pppoe_ctx->pppoe_changemtu = ppe_pppoe_mgr_changemtu_event;
}
