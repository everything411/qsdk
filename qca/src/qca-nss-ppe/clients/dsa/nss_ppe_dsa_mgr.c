/*
 **************************************************************************
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

/*
 * nss_ppe_dsa_mgr.c
 *	NSS PPE DSA manager
 */
#include <linux/of.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/dsa/8021q.h>
#include <net/dsa.h>

#include <ppe_drv_public.h>
#include <nss_ppe_vlan_mgr.h>
#include "nss_ppe_dsa_mgr.h"

/*
 * nss_ppe_dsa_mgr_changeaddr_event()
 *	Change dsa netdev MAC address.
 */
static int nss_ppe_dsa_mgr_changeaddr_event(struct netdev_notifier_info *info, struct dsa_port *dp)
{
	struct net_device *slave = dp->slave;

	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_dsa_mgr_info("slave:%s, MAC Addr change requested.\n", slave->name);
		nss_ppe_vlan_mgr_changeaddr_event(info);
	}
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_changemtu_event()
 *     Change dsa netdev MTU.
 */
static int nss_ppe_dsa_mgr_changemtu_event(struct netdev_notifier_info *info, struct dsa_port *dp)
{
	struct net_device *slave = dp->slave;

	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_dsa_mgr_info("slave:%s, idx:%u, proto:%d. \n", slave->name, dp->index, dp->cpu_dp->tag_ops->proto);
		nss_ppe_vlan_mgr_changemtu_event(info);
	}
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_register_event()
 *	dsa_mgr handles dsa netdev registration notification.
 */
static int nss_ppe_dsa_mgr_register_event(struct dsa_port *dp)
{
	struct net_device *master = dsa_port_to_master(dp);
	struct net_device *slave = dp->slave;

	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_vlan_mgr_dsa_vp_create(slave, master);
	}

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_unregister_event()
 *	dsa_mgr handles dsa netdev unregistration notification.
 */
static int nss_ppe_dsa_mgr_unregister_event(struct dsa_port *dp)
{
	struct net_device *slave = dp->slave;

	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_vlan_mgr_dsa_vp_destroy(slave);
	}
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_netdevice_event()
 *	dsa_mgr handles dsa netdev operation notifications.
 */
static int nss_ppe_dsa_mgr_netdevice_event(struct notifier_block *unused,
				unsigned long event, void *ptr)
{
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	struct dsa_port *dp = dsa_port_from_netdev(dev);
	if (IS_ERR(dp))
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_CHANGEADDR:
		return nss_ppe_dsa_mgr_changeaddr_event(info, dp);
	case NETDEV_CHANGEMTU:
		return nss_ppe_dsa_mgr_changemtu_event(info, dp);
	case NETDEV_REGISTER:
		return nss_ppe_dsa_mgr_register_event(dp);
	case NETDEV_UNREGISTER:
		return nss_ppe_dsa_mgr_unregister_event(dp);
	}

	/*
	 * Notify done for all the events we don't care
	 */
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_netdevice_nb()
 *	dsa_mgr netdevice event notifier.
 */
static struct notifier_block nss_ppe_dsa_mgr_netdevice_nb __read_mostly = {
	.notifier_call = nss_ppe_dsa_mgr_netdevice_event,
};

/*
 * nss_ppe_dsa_mgr_exit_module()
 *	dsa_mgr module exit function
 */
static void __exit nss_ppe_dsa_mgr_exit_module(void)
{
	unregister_netdevice_notifier(&nss_ppe_dsa_mgr_netdevice_nb);

	nss_ppe_dsa_mgr_info("PPE DSA MGR Module unloaded\n");
}

/*
 * nss_ppe_dsa_mgr_init_module()
 *	dsa_mgr module init function
 */
static int __init nss_ppe_dsa_mgr_init_module(void)
{
	register_netdevice_notifier(&nss_ppe_dsa_mgr_netdevice_nb);

	nss_ppe_dsa_mgr_info("PPE DSA MGR Module (Build %s) loaded\n", NSS_PPE_BUILD_ID);

	return 0;
}

module_init(nss_ppe_dsa_mgr_init_module);
module_exit(nss_ppe_dsa_mgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE DSA manager");
