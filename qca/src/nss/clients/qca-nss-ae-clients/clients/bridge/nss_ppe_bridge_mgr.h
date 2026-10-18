/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _NSS_PPE_BRIDGE_MGR_H_
#define _NSS_PPE_BRIDGE_MGR_H_

#ifdef CONFIG_QCA_MINIDUMP
#include <soc/qcom/ctx-save.h>
#endif

#if (NSS_PPE_BRIDGE_MGR_DEBUG_LEVEL < 1)
#define nss_ppe_bridge_mgr_assert(fmt, args...)
#else
#define nss_ppe_bridge_mgr_assert(c) BUG_ON(!(c))
#endif /* NSS_PPE_BRIDGE_MGR_DEBUG_LEVEL */

/*
 * Compile messages for dynamic enable/disable
 */
#define nss_ppe_bridge_mgr_always(s, ...) pr_alert(s, ##__VA_ARGS__)

#if defined(CONFIG_DYNAMIC_DEBUG)
#define nss_ppe_bridge_mgr_warn(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_bridge_mgr_info(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#define nss_ppe_bridge_mgr_trace(s, ...) \
		pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (NSS_PPE_BRIDGE_MGR_DEBUG_LEVEL < 2)
#define nss_ppe_bridge_mgr_warn(s, ...)
#else
#define nss_ppe_bridge_mgr_warn(s, ...) \
		pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_BRIDGE_MGR_DEBUG_LEVEL < 3)
#define nss_ppe_bridge_mgr_info(s, ...)
#else
#define nss_ppe_bridge_mgr_info(s, ...) \
		prnotice("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (NSS_PPE_BRIDGE_MGR_DEBUG_LEVEL < 4)
#define nss_ppe_bridge_mgr_trace(s, ...)
#else
#define nss_ppe_bridge_mgr_trace(s, ...) \
		pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

/*
 * Maximum PHY_PORT number
 */
#define NSS_PPE_BRIDGE_MGR_PHY_PORT_MAX 6

#define NSS_PPE_BRIDGE_MGR_SWITCH_ID 0
#define NSS_PPE_BRIDGE_MGR_SPANNING_TREE_ID 0

/*
 * Buffer sizes
 */
#define NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_SIZE 128
#define NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_MAX 10
#define NSS_PPE_BRIDGE_MGR_DUMP_BUFFER_SIZE 3072000

/*
 * struct nss_ppe_bridge_mgr_stats
 *	Bridge manager stats structure
 */
struct nss_ppe_bridge_mgr_stats {
	atomic64_t ppe_bridge_mgr_delete_instance_success;		/* Successfully deleted bridge instance from bridge list */
	atomic64_t ppe_bridge_mgr_create_instance_success;		/* Bridge manager instance created successfully */
	atomic64_t ppe_bridge_mgr_ppe_unregister_success;	/* Bridge unregister event succcess from PPE */
	atomic64_t ppe_bridge_mgr_ppe_register_br_success;		/* Bridge register to PPE succcess */
	atomic64_t ppe_bridge_mgr_vlan_over_br_notify_success;	/* Vlan over bridge notify event success */
	atomic64_t ppe_bridge_mgr_ppe_leave_br_success;			/* PPE leave from bridge event success */
	atomic64_t ppe_bridge_mgr_del_bond_slave_success;		/* Bond slave interface deleted from bond master */
	atomic64_t ppe_bridge_mgr_add_bond_slave_success;		/* Bond slave interface added to bond master */
	atomic64_t ppe_bridge_mgr_bond_fdb_join_success;		/* FDB updation success when Bond joined bridge */
	atomic64_t ppe_bridge_mgr_bond_fdb_leave_success;		/* FDB updation success when Bond left bridge */
	atomic64_t ppe_bridge_mgr_bond_slave_changeupper_success;	/* Add/remove of bond slave to bridge success */
	atomic64_t ppe_bridge_mgr_bond_master_join_success;		/* Bond master join to bridge successfull */
	atomic64_t ppe_bridge_mgr_bond_master_leave_success;		/* Bond master leave from bridge successfull */
	atomic64_t ppe_bridge_mgr_changemtu_success;			/* change MTU event success */
	atomic64_t ppe_bridge_mgr_changeaddr_success;		/* MAC address change success */
	atomic64_t ppe_bridge_mgr_is_ppe_success;			/* Device is represented on PPE */
	atomic64_t ppe_bridge_mgr_wan_intf_add_success;			/* WAN interface added via handler */
	atomic64_t ppe_bridge_mgr_wan_intf_del_success;			/* WAN interface deleted via handler */
	atomic64_t ppe_bridge_mgr_fdb_handler_success;		/* Successfully wrote into /proc/sys/ filesystem */
	atomic64_t ppe_bridge_mgr_fdb_del_notify_success;		/* FDB Delete notify success */
	atomic64_t ppe_bridge_mgr_find_instance_success;		/* Successfully found bridge instance */
	atomic64_t ppe_bridge_mgr_leave_br_success;			/* Net device bridge leave success */
	atomic64_t ppe_bridge_mgr_join_success;			/* Net device bridge join success */
	atomic64_t ppe_bridge_mgr_unregister_success;	/* Unregister bridge from bridge database success */
	atomic64_t ppe_bridge_mgr_register_br_success;		/* Register device to db success */
	atomic64_t ppe_bridge_mgr_no_ppe_iface;				/* Failed to find PPE interface */
	atomic64_t ppe_bridge_mgr_no_bridge_ovs_master;			/* Failed due to dev is not bridge and OVS master */
	atomic64_t ppe_bridge_mgr_no_master_upper_dev;			/* No upper dev master */
	atomic64_t ppe_bridge_mgr_no_vlan_real_dev;			/* Can't find the real device for VLAN*/
	atomic64_t ppe_bridge_mgr_no_br_fdb_event;			/* No bridge in FDB event */
	atomic64_t ppe_bridge_mgr_no_br_dev;				/* No bridge device */
	atomic64_t ppe_bridge_mgr_no_wan_overwrite;			/* Not able to overwrite WAN interface as it exists */
	atomic64_t ppe_bridge_mgr_dev_not_bridge_master;		/* Failed due to dev is not bridge master */
	atomic64_t ppe_bridge_mgr_intf_not_wan;				/* Interface is not Wan */
	atomic64_t ppe_bridge_mgr_dev_not_ppe;				/* Dev is not PPE interface */
	atomic64_t ppe_bridge_mgr_org_src_not_phy_intf;			/* Original source not a physical interface */
	atomic64_t ppe_bridge_mgr_new_src_ppe_phy_intf;			/* New source is PPE physical interface */
	atomic64_t ppe_bridge_mgr_instance_alloc_fail;			/* Failed to allocate nss_ppe_bridge_mgr_pvt instance */
	atomic64_t ppe_bridge_mgr_ppe_iface_alloc_fail;			/* Failed to allocate PPE iface instance */
	atomic64_t ppe_bridge_mgr_deinit_fail;			/* Failed to de-initialize bridge */
	atomic64_t ppe_bridge_mgr_vsi_alloc_fail;			/* Failed to allocate bridge vsi */
	atomic64_t ppe_bridge_mgr_mac_clear_fail;		 	/* Failed to clear MAC address */
	atomic64_t ppe_bridge_mgr_mac_set_fail;				/* Failed to set MAC address */
	atomic64_t ppe_bridge_mgr_mtu_set_fail;				/* Failed to set MTU */
	atomic64_t ppe_bridge_mgr_disable_fdb_fail;			/* Failed to disable FDB learning */
	atomic64_t ppe_bridge_mgr_enable_fdb_fail;			/* Failed to enable FDB learning */
	atomic64_t ppe_bridge_mgr_delete_fdb_fail;			/* Failed to delete FDB entry */
	atomic64_t ppe_bridge_mgr_fdb_del_notify_disable;		/* Notify fdb delete event disabled */
	atomic64_t ppe_bridge_mgr_fdb_del_invalid;			/* FDB delete not allowed */
	atomic64_t ppe_bridge_mgr_invalid_action;			/* Failed due to bridge invalid action */
	atomic64_t ppe_bridge_mgr_stp_state_set_fail;			/* Failed to set STP state */
	atomic64_t ppe_bridge_mgr_wan_in_bridge;			/* WAN is part of bridge */
	atomic64_t ppe_bridge_mgr_ppe_leave_br_fail;			/* PPE failed to leave from bridge */
	atomic64_t ppe_bridge_mgr_changeupper_event_fail;		/* Change upper dev event fail */
	atomic64_t ppe_bridge_mgr_identical_mac;			/* Device and bridge have same MAC address */
	atomic64_t ppe_bridge_mgr_procfs_write_fail;			/* Failed to write into /proc/sys/ filesystem */
	atomic64_t ppe_bridge_mgr_leave_br_fail;			/* Device failed to leave bridge */
	atomic64_t ppe_bridge_mgr_join_br_fail;			/* VLAN failed to join bridge */
	atomic64_t ppe_bridge_mgr_slave_join_fail;			/* Slave of bond failed to join bridge */
	atomic64_t ppe_bridge_mgr_slave_leave_fail;			/* Slave of bond failed to leave bridge */
	atomic64_t ppe_bridge_mgr_vlan_join_fail;			/* VLAN failed to join bridge */
	atomic64_t ppe_bridge_mgr_vlan_leave_fail;			/* VLAN failed to leave bridge */
	atomic64_t ppe_bridge_mgr_invalid_instance;			/* Failed to find bridge pvt instance */
	atomic64_t ppe_bridge_mgr_invalid_dev;				/* Net device does not exist */
	atomic64_t ppe_bridge_mgr_invalid_real_dev;			/* Invalid real dev */
	atomic64_t ppe_bridge_mgr_dev_bond_slave;			/* Device is bond slave */
	atomic64_t ppe_bridge_mgr_real_dev_bond_master;			/* Real dev is bond master */
	atomic64_t ppe_bridge_mgr_del_bond_slave_fail;			/* Failed to delete slave from bridge */
	atomic64_t ppe_bridge_mgr_add_bond_slave_fail;			/* Failed to add slave to bridge */
	atomic64_t ppe_bridge_mgr_bond_slave_exists;		/* Failed to join/leave bond, as bond slaves already exist in bridge */
	atomic64_t ppe_bridge_mgr_bond_master_not_in_br;		/* Bond master is not part of bridge */
	atomic64_t ppe_bridge_mgr_bond_master_join_fail;		/* Bond master failed to join bridge */
	atomic64_t ppe_bridge_mgr_bond_master_leave_fail;		/* Bond master failed to leave brdige */
	atomic64_t ppe_bridge_mgr_bond_master_vlan_over_br_leave_fail;	/* Bond master failed to leave brdige due to VLAN over bridge case */
	atomic64_t ppe_bridge_mgr_in_xlate_rule_del_fail;		/* Failed to delete ingress xlate rule */
	atomic64_t ppe_bridge_mgr_in_xlate_rule_add_fail;		/* Failed to add ingress xlate rule */
	atomic64_t ppe_bridge_mgr_ppe_registeration_fail;		/* Bridge registration to database failed */
};

/*
 * struct nss_ppe_brdige_mgr_context
 *	bridge manager context structure
 */
struct nss_ppe_bridge_mgr_context {
	struct list_head list;		/* List of bridge instance */
	spinlock_t lock;		/* Lock to protect bridge instance */
	struct net_device *wan_netdev;		/* WAN interface netdevice */

	char wan_ifname[IFNAMSIZ];	/* WAN interface name */
	struct ctl_table_header *nss_ppe_bridge_mgr_header;	/* bridge sysctl */
	struct nss_ppe_bridge_mgr_stats stats;		/* PPE Bridge manager statistics */
	struct dentry *dentry;			/* Root dentry for Bridge client */
};

extern struct nss_ppe_bridge_mgr_context br_mgr_ctx;

/*
 * struct nss_ppe_bridge_mgr_pvt
 *	bridge manager private structure
 */
struct nss_ppe_bridge_mgr_pvt {
	struct list_head list;			/* List of bridge instance */
	struct net_device *dev;			/* Bridge netdevice */
	struct ppe_drv_iface *iface;		/* PPE bridge iface */
	int bond_slave_num;			/* Total number of bond devices added into
						   bridge device */
	bool wan_if_enabled;			/* Is WAN interface enabled? */
	bool fdb_lrn_enabled;			/* Keep track of FDB Learning status */
	bool br_flood_en;			/* Flood to other ports in bridge */
	struct net_device *wan_netdev;		/* WAN interface netdevice */
	uint32_t mtu;				/* MTU for bridge */
	uint8_t dev_addr[ETH_ALEN];		/* MAC address for bridge */
	atomic64_t bridge_vlan_iface_cnt;		/* Number of VLAN interfaces over bridge */
};

/*
 * struct nss_ppe_bridge_mgr_dump_instance
 *	Structure used as an instance for bridge manager dump
 */
struct nss_ppe_bridge_mgr_dump_instance {
	uint16_t br_cnt;					/* Number of bridge  */
	uint16_t slave_cnt;					/* Number of Slave devices  */
	char prefix[NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_SIZE];	/* This is the prefix added to every message written */
	int prefix_levels[NSS_PPE_BRIDGE_MGR_DUMP_PREFIX_MAX];	/* How many nested prefixes supported */
	int prefix_level;					/* Prefix nest level */
	char msg[NSS_PPE_BRIDGE_MGR_DUMP_BUFFER_SIZE];		/* The message written / being returned to the reader */
	char *msgp;						/* Points into the msg buffer as we output it to the reader piece by piece */
	int msg_len;						/* Length of the msg buffer still to be written out */
	bool dump_en;						/* Enable dump once the file is open */
};

/*
 * nss_ppe_bridge_mgr_stats_inc()
 * 	Increment stats counter.
 */
static inline void nss_ppe_bridge_mgr_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

/*
 * nss_ppe_bridge_mgr_minidump_log()
 *	To log data structures into minidump output
 */
static inline void nss_ppe_bridge_mgr_minidump_log(void *start_addr, uint64_t size, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_add_segments((uint64_t)(uintptr_t)(start_addr), size, QCA_WDT_LOG_DUMP_TYPE_MOD, name, MINIDUMP_CRASH_TYPE_NSS, "qca_nss_ppe_bridge_mgr") != 0)
		pr_warn("minidump_log failed for structure type %s at address %p\n", name, start_addr);
#endif
}

/*
 * nss_ppe_bridge_mgr_minidump_free()
 *	To unregister data structures from minidump tlv
 */
static inline void nss_ppe_bridge_mgr_minidump_free(void *start_addr, const char *name) {
#ifdef CONFIG_QCA_MINIDUMP
	if (minidump_remove_segments((uint64_t)(uintptr_t)(start_addr)) != 0)
		pr_warn("minidump_free failed for structure %s at address %p\n", name, start_addr);
#endif
}

struct nss_ppe_bridge_mgr_pvt *nss_ppe_bridge_mgr_find_instance(struct net_device *dev);
int nss_ppe_bridge_mgr_leave_bridge(struct net_device *dev, struct net_device *bridge_dev);
int nss_ppe_bridge_mgr_join_bridge(struct net_device *dev, struct net_device *bridge_dev);
int nss_ppe_bridge_mgr_unregister_br(struct net_device *dev);
int nss_ppe_bridge_mgr_register_br(struct net_device *dev);
void nss_ppe_bridge_mgr_ovs_init(void);
void nss_ppe_bridge_mgr_ovs_exit(void);
void nss_ppe_bridge_mgr_stats_deinit(struct nss_ppe_bridge_mgr_context *ctx);
bool nss_ppe_bridge_mgr_stats_init(struct nss_ppe_bridge_mgr_context *ctx);
int nss_ppe_bridge_mgr_dump_init(struct nss_ppe_bridge_mgr_context *ctx);
void nss_ppe_bridge_mgr_dump_exit(void);

#endif
