/*
 **************************************************************************
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_stats.c
 *      Network function flowmgr stats
 */
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include "netfn_flowmgr_stats.h"
#include "netfn_flowmgr_priv.h"

/*
 * netfn_flowmgr_debug_stats_str
 *	Netfn flowmgr common statistics
 */
static const char * const netfn_flowmgr_debug_stats_str[] = {
	/* v4 generic stats */
	"v4_create_rule_req_fail_no_mem",		/**< No of v4 create rule failed with no memory */
	"v4_create_rule_flow_flag_mismatch",		/**< No of v4 create rule failed due to flow flag mismatched */
	"v4_create_rule_bridge_flow",			/**< No of v4 create rule for bridged flow */
	"v4_create_rule_routed_flow",			/**< No of v4 create rule for routed flow */
	"v4_create_validate_tuple_mismatch",		/**< No of v4 create rule failed due to tuple mismatch */
	"v4_create_validate_tuple_type_3",		/**< No of v4 create rule tuple type 3 */
	"v4_create_validate_tuple_type_4",		/**< No of v4 create rule tuple type 4 */
	"v4_create_validate_tuple_type_5",		/**< No of v4 create rule tuple type 5 */
	"v4_create_validate_tuple_simul_snat_dnat",	/**< No of v4 create rule failed due to simul snat and dnat */
	"v4_create_validate_tuple_invalid_snat_ip",	/**< No of v4 create rule failed due to invalid snat ip */
	"v4_create_validate_tuple_invalid_dnat_ip",	/**< No of v4 create rule failed due to invalid dnat ip */
	"v4_create_validate_tuple_invalid_snat_port",	/**< No of v4 create rule failed due to invalid snat port */
	"v4_create_validate_tuple_invalid_dnat_port",	/**< No of v4 create rule failed due to invalid dnat port */
	"v4_create_validate_tuple_ip_addr_mismatch",	/**< No of v4 create rule failed due to ip addr mismatch */
	"v4_create_validate_tuple_ident_mismatch",	/**< No of v4 create rule failed due to ident mismatch */
	"v4_create_validate_tuple_proto_mismatch",	/**< No of v4 create rule failed due to proto mismatch */
	"v4_create_validate_tuple_type_invalid",	/**< No of v4 create rule failed due to invalid tuple type */
	"v4_create_validate_tuple_invalid_nat_bridge",	/**< No of v4 create rule failed due to unsupported nat + bridge */
	"v4_create_validate_tuple_invalid_net_dev",	/**< No of v4 create rule failed due to invalid net device */
	"v4_destroy_validate_tuple_mismatch",		/**< No of v4 destroy rule failed due to tuple mismatch */
	"v4_destroy_validate_tuple_type_3",		/**< No of v4 destroy rule tuple type 3 */
	"v4_destroy_validate_tuple_type_4",		/**< No of v4 destroy rule tuple type 4 */
	"v4_destroy_validate_tuple_type_5",		/**< No of v4 destroy rule tuple type 5 */
	"v4_destroy_validate_tuple_type_invalid",	/**< No of v4 destroy rule failed due to invalid tuple type */
	"v4_destroy_validate_tuple_ip_addr_mismatch",	/**< No of v4 destroy rule failed due to ip addr mismatch */
	"v4_destroy_validate_tuple_ident_mismatch",	/**< No of v4 destroy rule failed due to ident mismatch */
	"v4_destroy_validate_tuple_proto_mismatch",	/**< No of v4 destroy rule failed due to proto mismatch */
	"v4_destroy_rule_req_fail_no_mem",		/**< No of v4 destroy rule failed with no memory */
	"v4_stats_sync_callback_not_registered",	/**< No of v4 sync requests with unregistered cb */
	"v4_stats_sync_invalid_msg_type",		/**< No of v4 sync requests with inavalid msg */
	"v4_get_stats_fail",				/**< No of v4 get stats failed */
	"v4_get_stats_tuple_type_unsupported",		/**< No of v4 get stats failed due to unspported tuple type */

	/* v4 PPE stats */
	"v4_create_ppe_rule_req",			/**< No of v4 ppe create rule requests */
	"v4_create_rule_invalid_ppe_tx_if",		/**< No of v4 create rule failed due to invalid ppe tx if */
	"v4_create_rule_invalid_ppe_rx_if",		/**< No of v4 create rule failed due to invalid ppe rx if */
	"v4_create_rule_invalid_ppe_top_tx_if",		/**< No of v4 create rule failed due to invalid ppe top tx if */
	"v4_create_rule_invalid_ppe_top_rx_if",		/**< No of v4 create rule failed due to invalid ppe top rx if */
	"v4_create_rule_ppe_success",			/**< No of v4 successful ppe create rule requests */
	"v4_create_rule_req_fail_ppe_fail",		/**< No of v4 failed ppe create rule requests */
	"v4_destroy_ppe_rule_req",			/**< No of v4 ppe destroy rule requests */
	"v4_destroy_rule_ppe_success",			/**< No of v4 successful ppe destroy rule requests */
	"v4_destroy_rule_req_fail_ppe_fail",		/**< No of v4 failed ppe destroy rule requests */
	"v4_get_stats_failed",				/**< No of v4 get stats failed requests */

	/* v4 SFE stats */
	"v4_create_sfe_rule_req",			/**< No of v4 sfe create rule requests */
	"v4_create_rule_sfe_success",			/**< No of v4 successful sfe create rule requests */
	"v4_create_rule_req_fail_sfe_fail",		/**< No of v4 failed sfe create rule requests */
	"v4_destroy_sfe_rule_req",			/**< No of v4 sfe destroy rule requests */
	"v4_destroy_rule_sfe_success",			/**< No of v4 successful sfe destroy rule requests */
	"v4_destroy_rule_req_fail_sfe_fail",		/**< No of v4 failed sfe destroy rule requests */

	/* v6 generic stats */
	"v6_create_rule_req_fail_no_mem",		/**< No of v6 create rule failed with no memory */
	"v6_create_rule_flow_flag_mismatch",		/**< No of v6 create rule failed due to flow flag mismatched */
	"v6_create_rule_bridge_flow",			/**< No of v6 create rule for bridged flow */
	"v6_create_rule_routed_flow",			/**< No of v6 create rule for routed flow */
	"v6_validate_tuple_mismatch",			/**< No of v6 create rule failed due to tuple mismatch */
	"v6_validate_tuple_type_3",			/**< No of v6 create rule tuple type 3 */
	"v6_validate_tuple_type_4",			/**< No of v6 create rule tuple type 4 */
	"v6_validate_tuple_type_5",			/**< No of v6 create rule tuple type 5 */
	"v6_validate_tuple_ip_addr_mismatch",		/**< No of v6 create rule failed due to ip addr mismatch */
	"v6_validate_tuple_ident_mismatch",		/**< No of v6 create rule failed due to ident mismatch */
	"v6_validate_tuple_proto_mismatch",		/**< No of v6 create rule failed due to proto mismatch */
	"v6_validate_tuple_type_invalid",		/**< No of v6 create rule failed due to invalid tuple type */
	"v6_create_validate_tuple_mismatch",		/**< No of v6 create rule failed due to tuple mismatch */
	"v6_create_validate_tuple_type_3",		/**< No of v6 create rule tuple type 3 */
	"v6_create_validate_tuple_type_4",		/**< No of v6 create rule tuple type 4 */
	"v6_create_validate_tuple_type_5",		/**< No of v6 create rule tuple type 5 */
	"v6_create_validate_tuple_simul_snat_dnat",	/**< No of v6 create rule failed due to simul snat and dnat */
	"v6_create_validate_tuple_invalid_snat_ip",	/**< No of v6 create rule failed due to invalid snat ip */
	"v6_create_validate_tuple_invalid_dnat_ip",	/**< No of v6 create rule failed due to invalid dnat ip */
	"v6_create_validate_tuple_invalid_snat_port",	/**< No of v6 create rule failed due to invalid snat port */
	"v6_create_validate_tuple_invalid_dnat_port",	/**< No of v6 create rule failed due to invalid dnat port */
	"v6_create_validate_tuple_ip_addr_mismatch",	/**< No of v6 create rule failed due to ip addr mismatch */
	"v6_create_validate_tuple_ident_mismatch",	/**< No of v6 create rule failed due to ident mismatch */
	"v6_create_validate_tuple_proto_mismatch",	/**< No of v6 create rule failed due to proto mismatch */
	"v6_create_validate_tuple_type_unsupported",	/**< No of v6 create rule failed due to unsupported tuple type */
	"v6_create_validate_tuple_type_invalid",	/**< No of v6 create rule failed due to invalid tuple type */
	"v6_create_validate_tuple_invalid_nat_bridge",	/**< No of v6 create rule failed due to unsupported nat + bridge */
	"v6_create_validate_tuple_invalid_net_dev",	/**< No of v6 create rule failed due to invalid net device */
	"v6_create_validate_tuple_invalid_nat_bridge",	/**< No of v6 create rule failed due to unsupported nat + bridge */
	"v6_create_validate_tuple_invalid_net_dev",	/**< No of v6 create rule failed due to invalid net device */
	"v6_destroy_validate_tuple_mismatch",		/**< No of v6 destroy rule failed due to tuple mismatch */
	"v6_destroy_validate_tuple_type_3",		/**< No of v6 destroy rule tuple type 3 */
	"v6_destroy_validate_tuple_type_4",		/**< No of v6 destroy rule tuple type 4 */
	"v6_destroy_validate_tuple_type_5",		/**< No of v6 destroy rule tuple type 5 */
	"v6_destroy_validate_tuple_type_invalid",	/**< No of v6 destroy rule failed due to invalid tuple type */
	"v6_destroy_validate_tuple_ip_addr_mismatch",	/**< No of v6 destroy rule failed due to ip addr mismatch */
	"v6_destroy_validate_tuple_ident_mismatch",	/**< No of v6 destroy rule failed due to ident mismatch */
	"v6_destroy_validate_tuple_proto_mismatch",	/**< No of v6 destroy rule failed due to proto mismatch */
	"v6_destroy_rule_req_fail_no_mem",		/**< No of v6 destroy rule failed with no memory */
	"v6_destroy_rule_req_fail_no_mem",		/**< No of v6 destroy rule failed with no memory */
	"v6_stats_sync_callback_not_registered",	/**< No of v6 sync requests with unregistered cb */
	"v6_stats_sync_invalid_msg_type",		/**< No of v6 sync requests with inavalid msg */
	"v6_get_stats_fail",				/**< No of v6 get stats failed */
	"v6_get_stats_tuple_type_unsupported",		/**< No of v6 get stats failed due to unspported tuple type */

	/* v6 PPE stats */
	"v6_create_ppe_rule_req",			/**< No of v6 ppe create rule requests */
	"v6_create_rule_invalid_ppe_tx_if",		/**< No of v6 create rule failed due to invalid ppe tx if */
	"v6_create_rule_invalid_ppe_rx_if",		/**< No of v6 create rule failed due to invalid ppe rx if */
	"v6_create_rule_invalid_ppe_top_tx_if",		/**< No of v6 create rule failed due to invalid ppe top tx if */
	"v6_create_rule_invalid_ppe_top_rx_if",		/**< No of v6 create rule failed due to invalid ppe top rx if */
	"v6_create_rule_ppe_success",			/**< No of v6 successful ppe create rule requests */
	"v6_create_rule_req_fail_ppe_fail",		/**< No of v6 failed ppe create rule requests */
	"v6_destroy_ppe_rule_req",			/**< No of v6 ppe destroy rule requests */
	"v6_destroy_rule_ppe_success",			/**< No of v6 successful ppe destroy rule requests */
	"v6_destroy_rule_req_fail_ppe_fail",		/**< No of v6 failed ppe destroy rule requests */
	"v6_get_stats_failed",				/**< No of v6 get stats failed requests */

	/* v6 SFE stats */
	"v6_create_sfe_rule_req",			/**< No of v6 sfe create rule requests */
	"v6_create_rule_sfe_success",			/**< No of v6 successful sfe create rule requests */
	"v6_create_rule_req_fail_sfe_fail",		/**< No of v6 failed sfe create rule requests */
	"v6_destroy_sfe_rule_req",			/**< No of v6 sfe destroy rule requests */
	"v6_destroy_rule_sfe_success",			/**< No of v6 successful sfe destroy rule requests */
	"v6_destroy_rule_req_fail_sfe_fail",		/**< No of v6 failed sfe destroy rule requests */

	/* common stats */
	"validate_vlan_inner_mismatch",			/**< No of v4/v6 create rule failed due to inner vlan mismatch */
	"validate_vlan_outer_mismatch",			/**< No of v4/v6 create rule failed due to outer vlan mismatch */
	"validate_unsupported_tuple_type",		/**< No of v4/v6 failure due to unsupported tuple type */
	"dscp_priority_add_req",			/**< No of add dscp priority requests */
	"dscp_priority_del_req",			/**< No of del dscp priority requests */
	"dscp_priority_add_req_fail_inval_core_id",	/**< No of add dscp priority request fail due to invalid core id */
	"dscp_priority_add_req_fail_inval_pri_queue",	/**< No of add dscp priority request fail due to invalid priority queue */
	"dscp_priority_add_req_fail_no_wan_dev",	/**< No of add dscp priority request fail due to no WAN dev */
	"dscp_priority_add_req_rule_create_fail",	/**< No of add dscp priority request fail due to rule create fail */
	"dscp_priority_del_req_rule_destroy_fail",	/**< No of del dscp priority request fail due to rule destroy fail */
};

/*
 * netfn_flowmgr_ae_type_str
 *	Netfn flowmgr AE type
 */
static const char * const netfn_flowmgr_ae_type_str[] = {
	"INVALID",
	"SFE",		/**< AE type SFE */
	"PPE",		/**< AE type PPE */
};

/*
 * netfn_flowmgr_debug_stats_show()
 *	Read netfn flowmgr debug statistics
 */
static int netfn_flowmgr_debug_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_debug_stats *ae_stats;
	netfn_flowmgr_ae_type_t ae_type;
	uint16_t stats_idx;

	/*
	 * For all AE type store and print the stats atomically.
	 */
	for (ae_type = NETFN_FLOWMGR_AE_TYPE_SFE; ae_type < NETFN_FLOWMGR_AE_TYPE_MAX; ++ae_type) {
		ae_stats = &f->stats[ae_type];
		seq_printf(m, "\nNetfn flowmgr stats for AE: %s:\n\n", netfn_flowmgr_ae_type_str[ae_type]);
		/*
		 * Iterate till debug stats length and dump the stats.
		 */
		for (stats_idx = 0; stats_idx < sizeof(struct netfn_flowmgr_debug_stats) / sizeof(uint64_t); stats_idx++) {
			seq_printf(m, "\t\t [%s]:  %llu\n", netfn_flowmgr_debug_stats_str[stats_idx],
						atomic64_read(((atomic64_t *)ae_stats + stats_idx)));
		}
	}
	return 0;
};

/*
 * netfn_flowmgr_stats_write()
 *	Clear netfn_flowmgr statistics.
 */
static ssize_t netfn_flowmgr_stats_write(struct file *filep, const char __user *buffer, size_t count, loff_t *ppos)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_debug_stats *ae_stats;
	netfn_flowmgr_ae_type_t ae_type;
	uint64_t stats_idx;
	uint32_t reset;

	if (kstrtou32_from_user(buffer, count, 0, &reset)) {
		return -EINVAL;
	}

	if (reset != 0) {
		return -EINVAL;
	}

	for (ae_type = NETFN_FLOWMGR_AE_TYPE_SFE; ae_type < NETFN_FLOWMGR_AE_TYPE_MAX; ++ae_type) {
		ae_stats = &f->stats[ae_type];
		for (stats_idx = 0; stats_idx < sizeof(struct netfn_flowmgr_debug_stats) / sizeof(uint64_t); stats_idx++) {
			atomic64_set((atomic64_t *)ae_stats + stats_idx, 0);
		}
	}

	return count;
}

/*
 * netfn_flowmgr_stats_open()
 *	Netfn flowmgr stats open callback
 */
static int netfn_flowmgr_stats_open(struct inode *inode, struct file *file)
{
        return single_open(file, netfn_flowmgr_debug_stats_show, inode->i_private);
}

/*
 * netfn_flowmgr_stats_file_ops
 *      File operations for stats
 */
const struct file_operations netfn_flowmgr_stats_file_ops = {
        .open = netfn_flowmgr_stats_open,
        .read = seq_read,
	.write = netfn_flowmgr_stats_write,
        .llseek = seq_lseek,
        .release = seq_release,
};

/*
 * netfn_flowmgr_stats_debugfs_init()
 *	Create netfn flowmgr statistics debugfs entry.
 */
int netfn_flowmgr_stats_debugfs_init(void)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	f->dentry = debugfs_create_dir("qca-nss-netfn", NULL);
	if (!f->dentry) {
		netfn_flowmgr_warn("%p: Unable to create qca-nss-netfn debugfs stats directory in flowmgr\n", f);
		return -1;
	}

	f->stats_dentry = debugfs_create_dir("flowmgr", f->dentry);
	if (!f->stats_dentry) {
		netfn_flowmgr_warn("%p: Unable to create flowmgr debugfs stats sub-directory in flowmgr\n", f);
		goto debugfs_dir_failed;
	}

	if (!debugfs_create_file("stats", S_IRUGO, f->stats_dentry,
				NULL, &netfn_flowmgr_stats_file_ops)) {
		netfn_flowmgr_warn("%p: Unable to create common statistics file entry in debugfs\n", f);
		goto debugfs_dir_failed;
	}

	return 0;

debugfs_dir_failed:
	debugfs_remove_recursive(f->dentry);
	f->dentry = NULL;
	f->stats_dentry = NULL;
	return -1;
}

/*
 * netfn_flowmgr_stats_debugfs_exit()
 *	Destroy netfn flowmgr statistics debugfs entry.
 */
void netfn_flowmgr_stats_debugfs_exit(void)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	if (f->dentry) {
		debugfs_remove_recursive(f->dentry);
		f->dentry = NULL;
		f->stats_dentry = NULL;
	}
}
