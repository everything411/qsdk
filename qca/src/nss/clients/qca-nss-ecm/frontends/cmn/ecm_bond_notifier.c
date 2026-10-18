/*
 **************************************************************************
 * Copyright (c) 2014-2017, 2020-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * ecm_bond_notifier.c
 * 	Bonding notifier functionality.
 */

#include <linux/version.h>
#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/icmp.h>
#include <linux/debugfs.h>
#include <linux/kthread.h>
#include <linux/pkt_sched.h>
#include <linux/string.h>
#include <net/route.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <asm/uaccess.h>	/* for put_user */
#include <net/ipv6.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/tcp.h>

#include <linux/inetdevice.h>
#include <linux/if_arp.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_bridge.h>
#include <linux/if_bridge.h>
#include <net/bonding.h>
#include <net/arp.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_acct.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_l4proto.h>
#include <net/netfilter/nf_conntrack_zones.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/ipv4/nf_conntrack_ipv4.h>
#include <net/netfilter/ipv4/nf_defrag_ipv4.h>

/*
 * Debug output levels
 * 0 = OFF
 * 1 = ASSERTS / ERRORS
 * 2 = 1 + WARN
 * 3 = 2 + INFO
 * 4 = 3 + TRACE
 */
#define DEBUG_LEVEL ECM_BOND_NOTIFIER_DEBUG_LEVEL

#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_tracker_udp.h"
#include "ecm_tracker_tcp.h"
#include "ecm_tracker_datagram.h"
#include "ecm_db.h"
#include "ecm_classifier_default.h"
#include "ecm_interface.h"

/*
 * Default path for syctl
 */
#define ECM_BOND_NOTIFIER_PATH "net/ecm/ecm_bond_notifier"

/*
 * Locking of the classifier - concurrency control
 */
static DEFINE_SPINLOCK(ecm_bond_notifier_lock);             /* Protect against SMP access between netfilter, events and private threaded function. */

/*
 * Debugfs dentry object.
 */
static struct dentry *ecm_bond_notifier_dentry;

/*
 * sysctl table header
 */
static struct ctl_table_header *ecm_bond_notifier_ctl_table_header;

/*
 * General operational control
 */
static int ecm_bond_notifier_stopped = 0;                   /* When non-zero further traffic will not be processed */

/*
 * ecm_bond_notifier_bond_cb
 *	Bond driver notifier
 */
static struct bond_cb ecm_bond_notifier_bond_cb;

/*
 * ecm_bond_notifier_bond_link_down()
 *	Callback when a link goes down on a LAG slave
 */
static void ecm_bond_notifier_bond_link_down(struct net_device *slave_dev)
{
	struct net_device *master;

	/*
	 * If operations have stopped then do not process event
	 */
	spin_lock_bh(&ecm_bond_notifier_lock);
	if (unlikely(ecm_bond_notifier_stopped)) {
		spin_unlock_bh(&ecm_bond_notifier_lock);
		DEBUG_WARN("Ignoring bond link down event - stopped\n");
		return;
	}
	spin_unlock_bh(&ecm_bond_notifier_lock);

	/*
	 * A net device that is a LAG slave has lost link.
	 * Due to the heiarchical nature of network topologies, this can
	 * change the packet transmit path for any connection that is using
	 * a device that it sitting "higher" in the heirarchy. Regenerate all
	 * connections using the LAG master.
	 */

	master = ecm_interface_get_and_hold_dev_master(slave_dev);
	if (!master) {
		DEBUG_WARN("No master dev\n");
		return;
	}
	ecm_interface_dev_defunct_connections(master);
	dev_put(master);
}

/*
 * ecm_bond_notifier_bond_link_up()
 *	Callback when a device is enslaved by a LAG master device
 */
static void ecm_bond_notifier_bond_link_up(struct net_device *slave_dev)
{
	struct net_device *master;

	/*
	 * If operations have stopped then do not process event
	 */
	spin_lock_bh(&ecm_bond_notifier_lock);
	if (unlikely(ecm_bond_notifier_stopped)) {
		spin_unlock_bh(&ecm_bond_notifier_lock);
		DEBUG_WARN("Ignoring bond enslave event - stopped\n");
		return;
	}
	spin_unlock_bh(&ecm_bond_notifier_lock);

	/*
	 * Tricky to handle, this one.
	 * A net device that is a LAG slave has become active.
	 * Due to the heiarchical nature of network topologies, this can change the packet transmit path
	 * for any connection that is using a device that it sitting "higher" in the heirarchy.
	 * Regenerate all connections using the LAG master.
	 */

	master = ecm_interface_get_and_hold_dev_master(slave_dev);
	if (!master) {
		DEBUG_WARN("No master dev\n");
		return;
	}
	ecm_interface_dev_defunct_connections(master);
	dev_put(master);
}

/*
 * ecm_bond_notifier_bond_delete_by_slave()
 *	Callback when defunct a LAG slave
 */
static void ecm_bond_notifier_bond_delete_by_slave(struct net_device *slave_dev)
{
	struct net_device *master;

	/*
	 * If operations have stopped then do not process event
	 */
	spin_lock_bh(&ecm_bond_notifier_lock);
	if (unlikely(ecm_bond_notifier_stopped)) {
		spin_unlock_bh(&ecm_bond_notifier_lock);
		DEBUG_WARN("Ignoring bond defunct event - stopped\n");
		return;
	}
	spin_unlock_bh(&ecm_bond_notifier_lock);

	/*
	 * A net device that is a LAG slave has to
	 * defunct all the connection.
	 * Due to the heiarchical nature of network topologies, this can
	 * change the packet transmit path for any connection that is using
	 * a device that it sitting "higher" in the heirarchy.
	 */
	master = ecm_interface_get_and_hold_dev_master(slave_dev);
	if (!master) {
		DEBUG_WARN("Failed to find 'master' for the slave:%s\n", slave_dev->name);
		return;
	}

	ecm_interface_dev_defunct_connections(master);
	dev_put(master);
}

/*
 * ecm_bond_notifier_bond_delete_by_mac()
 *     This is a call back to delete all the rules with the mac address
 */
static void ecm_bond_notifier_bond_delete_by_mac(uint8_t *mac)
{
	DEBUG_INFO("Bond notifier for node %pM\n", mac);

	ecm_interface_node_connections_defunct(mac, ECM_DB_IP_VERSION_IGNORE);
}

/*
 * ecm_bond_notifier_stop_handler()
 * 	Proc handler for enable or disable ECM bond notifier
 */
static int ecm_bond_notifier_stop_handler(ECM_CTL_TABLE_CONST struct ctl_table *ctl, int write, void *buffer, size_t *lenp, loff_t *ppos)
{
	/*
	 * Usage:
	 *
	 * Enable ECM bond notifier
	 * echo 0 > /proc/sys/net/ecm/ecm_bond_notifier/stop
	 *
	 * Disable ECM bond notifier
	 * echo 1 > /proc/sys/net/ecm/ecm_bond_notifier/stop
	 *
	 * To read status:
	 * cat /proc/sys/net/ecm/ecm_bond_notifier/stop
	 */
	int ret;
	int original_value;

	/*
	 * Take the original value in case to restore it
	 * due to invalid input
	 */
	original_value = ecm_bond_notifier_stopped;
	ret = proc_dointvec(ctl, write, buffer, lenp, ppos);
	if (ret || (!write)) {
		return ret;
	}

	if ((ecm_bond_notifier_stopped != 0) && (ecm_bond_notifier_stopped != 1)) {
		DEBUG_WARN("Invalid input, Valid values 0/1\n");
		ecm_bond_notifier_stopped = original_value;
		return -EINVAL;
	}

	return ret;
}

static struct ctl_table ecm_bond_notifier_ctl_table[] = {
	{
		.procname	= "stop",
		.data		= &ecm_bond_notifier_stopped,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &ecm_bond_notifier_stop_handler,
	},
};

void ecm_bond_notifier_stop(int num)
{
	ecm_bond_notifier_stopped = num;
}
EXPORT_SYMBOL(ecm_bond_notifier_stop);

/*
 * ecm_bond_notifier_init()
 */
int ecm_bond_notifier_init(struct dentry *dentry)
{
	DEBUG_INFO("ECM Bonding Notifier init\n");

	/*
	 * Register sysctl table for bond notifier
	 */
	ecm_bond_notifier_ctl_table_header = register_sysctl(ECM_BOND_NOTIFIER_PATH, ecm_bond_notifier_ctl_table);
	if (!ecm_bond_notifier_ctl_table_header) {
		DEBUG_ERROR("Failed to create ecm bond notifier stopped file in sysctl\n");
		return -1;
	}

	if (!ecm_debugfs_create_dir("ecm_bond_notifier", dentry, &ecm_bond_notifier_dentry)) {
		DEBUG_ERROR("Failed to create ecm bond notifier directory in debugfs\n");
		unregister_sysctl_table(ecm_bond_notifier_ctl_table_header);
		return -1;
	}

	ecm_debugfs_create_u32("stop", S_IRUGO | S_IWUSR, ecm_bond_notifier_dentry,
					(u32 *)&ecm_bond_notifier_stopped);

	/*
	 * Register Link Aggregation callbacks with the bonding driver
	 */
	ecm_bond_notifier_bond_cb.bond_cb_link_up = ecm_bond_notifier_bond_link_up;
	ecm_bond_notifier_bond_cb.bond_cb_link_down = ecm_bond_notifier_bond_link_down;
	ecm_bond_notifier_bond_cb.bond_cb_delete_by_slave = ecm_bond_notifier_bond_delete_by_slave;
	ecm_bond_notifier_bond_cb.bond_cb_delete_by_mac = ecm_bond_notifier_bond_delete_by_mac;
	bond_register_cb(&ecm_bond_notifier_bond_cb);

	return 0;
}
EXPORT_SYMBOL(ecm_bond_notifier_init);

/*
 * ecm_bond_notifier_exit()
 */
void ecm_bond_notifier_exit(void)
{
	DEBUG_INFO("ECM Bonding Notifier exit\n");

	/*
	 * Unregister from the bond driver
	 */
	bond_unregister_cb();

	/*
	 * Remove the debugfs files recursively.
	 */
	if (ecm_bond_notifier_dentry) {
		ecm_debugfs_remove_recursive(ecm_bond_notifier_dentry);
	}

	/*
	 * Unregister sysctl table
	 */
	if (ecm_bond_notifier_ctl_table_header) {
		unregister_sysctl_table(ecm_bond_notifier_ctl_table_header);
	}
}
EXPORT_SYMBOL(ecm_bond_notifier_exit);
