/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <ppe_drv.h>
#include "ppe_rfs/ppe_rfs.h"
#if !defined(NSS_PPE_LOWMEM_PROFILE_16M)
#include "ppe_acl/ppe_acl.h"
#include "ppe_policer/ppe_policer.h"
#include "ppe_qos/ppe_qos.h"
#include "ppe_cos_map/ppe_cos_map.h"
#ifdef NSS_PPE_MCAST_FEATURE_SUPPORT
#include "ppe_mcast/ppe_mcast.h"
#endif
#endif
#ifdef NSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
#include "ppe_dscp/ppe_dscp.h"
#endif
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
#include "ppe_pm/ppe_pm.h"
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
#include "ppe_vlan/ppe_vlan.h"
#endif
#ifdef NSS_PPE_PORT_MGMT_SUPPORT
#include "ppe_port_mgmt/ppe_port_mgmt.h"
#endif
#if !defined(NSS_PPE_LOWMEM_PROFILE_16M) && !defined(NSS_PPE_LOWMEM_PROFILE_256M)
#include "ppe_mirror/ppe_mirror.h"
#include "ppe_priority/ppe_priority.h"
#if defined(PPE_TUN_RPS_ENABLED)
#include "ppe_tun_rps/ppe_tun_rps.h"
#endif
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
#include "ppe_dot1p/ppe_dot1p.h"
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
#include "ppe_gemport/ppe_gemport.h"
#endif

struct dentry *d_rule;

/*
 * ppe_rule_module_init()
 *	module init for ppe rule driver.
 */
static int __init ppe_rule_module_init(void)
{
	struct dentry *root = ppe_drv_get_dentry();
	if (!root) {
		printk("Unable to get root stats directory\n");
		return -1;
	}

	d_rule = debugfs_create_dir("ppe-rule", root);
	if (!d_rule) {
		printk("Unable to create debugfs stats directory in debugfs\n");
		return -1;
	}
	ppe_rfs_init(d_rule);
#if !defined(NSS_PPE_LOWMEM_PROFILE_16M)
	ppe_acl_init(d_rule);
	ppe_policer_init(d_rule);
	ppe_qos_init(d_rule);
	ppe_cos_map_init(d_rule);
#ifdef NSS_PPE_MCAST_FEATURE_SUPPORT
	ppe_mcast_init(d_rule);
#endif
#endif
#ifdef NSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
	ppe_dscp_init(d_rule);
#endif
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	ppe_pm_init(d_rule);
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	ppe_vlan_init(d_rule);
#endif
#ifdef NSS_PPE_PORT_MGMT_SUPPORT
	ppe_port_mgmt_init(d_rule);
#endif
#if !defined(NSS_PPE_LOWMEM_PROFILE_16M) && !defined(NSS_PPE_LOWMEM_PROFILE_256M)
	ppe_mirror_init(d_rule);
	ppe_priority_init(d_rule);
#if defined(PPE_TUN_RPS_ENABLED)
	ppe_tun_rps_init(d_rule);
#endif
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
	ppe_dot1p_init(d_rule);
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
	ppe_gem_port_init(d_rule);
#endif

	printk("PPE-RULE module loaded successfully\n");
	return 0;
}
module_init(ppe_rule_module_init);

/*
 * ppe_rule_module_exit()
 *	module exit for ppe rule driver.
 */
static void __exit ppe_rule_module_exit(void)
{
	ppe_rfs_deinit();

#if !defined(NSS_PPE_LOWMEM_PROFILE_16M) && !defined(NSS_PPE_LOWMEM_PROFILE_256M)
	ppe_mirror_deinit();
	ppe_priority_deinit();
#if defined(PPE_TUN_RPS_ENABLED)
	ppe_tun_rps_deinit();
#endif
#endif
#if !defined(NSS_PPE_LOWMEM_PROFILE_16M)
	ppe_policer_deinit();
	ppe_acl_deinit();
	ppe_qos_deinit();
	ppe_cos_map_deinit();
#ifdef NSS_PPE_MCAST_FEATURE_SUPPORT
	ppe_mcast_deinit();
#endif
#endif
#ifdef NSS_PPE_DSCP_PBIT_FEATURE_SUPPORT
	ppe_dscp_deinit();
#endif
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
	ppe_pm_deinit();
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	ppe_vlan_deinit();
#endif
#ifdef NSS_PPE_PORT_MGMT_SUPPORT
	ppe_port_mgmt_deinit();
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
	ppe_dot1p_deinit();
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
	ppe_gem_port_deinit();
#endif
	debugfs_remove_recursive(d_rule);
	printk("PPE-RULE module unloaded");
}
module_exit(ppe_rule_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("PPE RULE driver");
