/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/inetdevice.h>
#include <net/ip.h>
#include "nss_ppe_bridge_mgr.h"

/*
 * nss_ppe_bridge_mgr_stats_str
 * 	NSS PPE Bridge manager statistics
 */
static const char *nss_ppe_bridge_mgr_stats_str[] = {
	"delete_instance_success",	/* Successfully deleted bridge instance from bridge list */
	"create_instance_success",	/* Bridge manager instance created successfully */
	"ppe_unregister_success",	/* Bridge unregister event succcess from PPE */
	"ppe_register_br_success",	/* Bridge register to PPE succcess */
	"vlan_over_br_notify_success",	/* Vlan over bridge notify event success */
	"ppe_leave_br_success",		/* PPE leave from bridge event success */
	"del_bond_slave_success",	/* Bond slave interface deleted from bond master */
	"add_bond_slave_success",	/* Bond slave interface added to bond master */
	"bond_fdb_join_success",	/* FDB updation success when Bond joined bridge */
	"bond_fdb_leave_success",	/* FDB updation success when Bond left bridge */
	"bond_slave_changeupper_success",/* Add/remove of bond slave to bridge success */
	"bond_master_join_success",	/* Bond master join to bridge successfull */
	"bond_master_leave_success",	/* Bond master leave from bridge successfull */
	"changemtu_success",		/* change MTU event success */
	"changeaddr_success",		/* MAC address change success */
	"is_ppe_success",		/* Device is represented on PPE */
	"wan_intf_add_success",		/* WAN interface added via handler */
	"wan_intf_del_success",		/* WAN interface deleted via handler */
	"fdb_handler_success",		/* Successfully wrote into /proc/sys/ filesystem */
	"fdb_del_notify_success",	/* FDB Delete notify success */
	"find_instance_success",	/* Successfully found bridge instance */
	"leave_br_success",		/* Net device bridge leave success */
	"join_success",			/* Net device bridge join success */
	"unregister_success",		/* Unregister bridge from bridge database success */
	"register_br_success",		/* Register device to db success */
	"no_ppe_iface",			/* Failed to find PPE interface */
	"no_bridge_ovs_master",		/* Failed due to dev is not bridge and OVS master */
	"no_master_upper_dev",		/* No upper dev master */
	"no_vlan_real_dev",		/* Can't find the real device for VLAN */
	"no_br_fdb_event",		/* No bridge in FDB event */
	"no_br_dev",			/* No bridge device */
	"no_wan_overwrite",		/* Not able to overwrite WAN interface as it exists */
	"dev_not_bridge_master",	/* Failed due to dev is not bridge master */
	"intf_not_wan",			/* Interface is not Wan */
	"dev_not_ppe",			/* Dev is not PPE interface */
	"org_src_not_phy_intf",		/* Original source not a physical interface */
	"new_src_ppe_phy_intf",		/* New source is PPE physical interface */
	"instance_alloc_fail",		/* Failed to allocate nss_ppe_bridge_mgr_pvt instance */
	"ppe_iface_alloc_fail",		/* Failed to allocate PPE iface instance */
	"deinit_fail",			/* Failed to de-initialize bridge */
	"vsi_alloc_fail",		/* Failed to allocate bridge vsi */
	"mac_clear_fail",		/* Failed to clear MAC address */
	"mac_set_fail",			/* Failed to set MAC address */
	"mtu_set_fail",			/* Failed to set MTU */
	"disable_fdb_fail",		/* Failed to disable FDB learning */
	"enable_fdb_fail",		/* Failed to enable FDB learning */
	"delete_fdb_fail",		/* Failed to delete FDB entry */
	"fdb_del_notify_disable",	/* Notify fdb delete event disabled */
	"fdb_del_invalid",		/* FDB delete not allowed */
	"invalid_action",		/* Failed due to bridge invalid action */
	"stp_state_set_fail",		/* Failed to set STP state */
	"wan_in_bridge",		/* WAN is part of bridge */
	"ppe_leave_br_fail",		/* PPE failed to leave from bridge */
	"changeupper_event_fail",	/* Change upper dev event fail */
	"identical_mac",		/* Device and bridge have same MAC address */
	"procfs_write_fail",		/* Failed to write into /proc/sys/ filesystem */
	"leave_br_fail",		/* Device failed to leave bridge */
	"join_br_fail",			/* VLAN failed to join bridge */
	"slave_join_fail",		/* Slave of bond failed to join bridge */
	"slave_leave_fail",		/* Slave of bond failed to leave bridge */
	"vlan_join_fail",		/* VLAN failed to join bridge */
	"vlan_leave_fail",		/* VLAN failed to leave bridge */
	"invalid_instance",		/* Failed to find bridge pvt instance */
	"invalid_dev",			/* Net device does not exist */
	"invalid_real_dev",		/* Invalid real dev */
	"dev_bond_slave",		/* Device is bond slave */
	"real_dev_bond_master",		/* Real dev is bond master */
	"del_bond_slave_fail",		/* Failed to delete slave from bridge */
	"add_bond_slave_fail",		/* Failed to add slave to bridge */
	"bond_slave_exists",		/* Failed to join/leave bond, as bond slaves already exist in bridge */
	"bond_master_not_in_br",	/* Bond master is not part of bridge */
	"bond_master_join_fail",	/* Bond master failed to join bridge */
	"bond_master_leave_fail",	/* Bond master failed to leave brdige */
	"bond_master_vlan_over_br_leave_fail",	/* Bond master failed to leave brdige due to VLAN over bridge case */
	"in_xlate_rule_del_fail",	/* Failed to delete ingress xlate rule */
	"in_xlate_rule_add_fail",	/* Failed to add ingress xlate rule */
	"ppe_registeration_fail",	/* Bridge registration to database failed */
};

/*
 * nss_ppe_bridge_mgr_stats_show()
 *	Read PPE bridge_mgr statistics
 */
static int nss_ppe_bridge_mgr_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct nss_ppe_bridge_mgr_context *ctx = &br_mgr_ctx;
	atomic64_t *stats = (atomic64_t *)&ctx->stats;
	int i;

	seq_puts(m, "\nPPE Bridge manager stats:\n\n");

	for (i = 0; i < sizeof(struct nss_ppe_bridge_mgr_stats) / sizeof(uint64_t); i++, stats++) {
		seq_printf(m, "\t\t [%s]:  %llu\n", nss_ppe_bridge_mgr_stats_str[i], atomic64_read(stats));
	}

	return 0;
}

/*
 * nss_ppe_bridge_mgr_stats_open
 *	Open PPE bridge_mgr stats
 */
static int nss_ppe_bridge_mgr_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, nss_ppe_bridge_mgr_stats_show, inode->i_private);
}

/*
 * nss_ppe_bridge_mgr_stats_ops
 *	File operations for PPE bridge_mgr stats
 */
static const struct file_operations nss_ppe_bridge_mgr_stats_ops = {
	.open = nss_ppe_bridge_mgr_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * nss_ppe_bridge_mgr_stats_deinit()
 *	Cleanup the debugfs tree.
 */
void nss_ppe_bridge_mgr_stats_deinit(struct nss_ppe_bridge_mgr_context *ctx)
{
	if (ctx->dentry) {
		debugfs_remove_recursive(ctx->dentry);
		ctx->dentry = NULL;
	}
}

/*
 * nss_ppe_bridge_mgr_stats_init()
 *	Create bridge_mgr statistics debugfs entry.
 */
bool nss_ppe_bridge_mgr_stats_init(struct nss_ppe_bridge_mgr_context *ctx)
{
	/*
	 * Initialize debugfs directory.
	 */
	struct dentry *parent;
	struct dentry *clients;
	struct dentry *dentry;

	parent = debugfs_lookup("qca-nss-ppe", NULL);
	if (!parent) {
		nss_ppe_bridge_mgr_warn("parent debugfs entry for qca-nss-ppe not present\n");
		return false;
	}

	clients = debugfs_lookup("clients", parent);
	if (!clients) {
		nss_ppe_bridge_mgr_warn("clients debugfs entry inside qca-nss-ppe not present\n");
		return false;
	}

	ctx->dentry = debugfs_create_dir("bridge", clients);
	if (!ctx->dentry) {
		nss_ppe_bridge_mgr_warn("Bridge manager debugfs entry inside qca-nss-ppe/clients could not be created\n");
		return false;
	}

	dentry = debugfs_create_file("stats", S_IRUGO,
			ctx->dentry, NULL, &nss_ppe_bridge_mgr_stats_ops);
	if (!dentry) {
		nss_ppe_bridge_mgr_warn("Debugfs file creation failed\n");
		debugfs_remove_recursive(ctx->dentry);
		ctx->dentry = NULL;
		return false;
	}

	return true;
}
