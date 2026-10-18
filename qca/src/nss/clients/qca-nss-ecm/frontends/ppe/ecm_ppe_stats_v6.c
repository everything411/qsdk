/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

#include <linux/version.h>
#include <linux/types.h>
#include <net/ip.h>
#include <linux/inet.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/etherdevice.h>

#define DEBUG_LEVEL ECM_PPE_STATS_DEBUG_LEVEL
#include "ecm_types.h"
#include "ecm_db_types.h"
#include "ecm_state.h"
#include "ecm_tracker.h"
#include "ecm_classifier.h"
#include "ecm_front_end_types.h"
#include "ecm_ppe_stats_v6.h"

struct ecm_ppe_stats_v6 ppe_v6_stats;

static const char *ecm_ppe_stats_v6_exception_ported_name_str[] = {
	"v6_exception_ported_accel_not_permitted",
	"v6_exception_ported_no_mem",
	"v6_exception_ported_no_from_interfaces",
	"v6_exception_ported_no_to_interfaces",
	"v6_exception_ported_from_iface_ingress_qdisc_unsupported",
	"v6_exception_ported_from_iface_qdisc_unsupported",
	"v6_exception_ported_from_iface_bridge_cascade",
	"v6_exception_ported_from_iface_ovs_bridge_cascade",
	"v6_exception_ported_from_iface_ovs_bridge_unsupported",
	"v6_exception_ported_from_iface_more_than_one_pppoe_unsupported",
	"v6_exception_ported_from_iface_pppoe_flow_invalid",
	"v6_exception_ported_from_iface_only_two_vlans_supported",
	"v6_exception_ported_from_iface_vlan_not_enabled",
	"v6_exception_ported_from_iface_ipsec_not_enabled",
	"v6_exception_ported_from_iface_vxlan_additional_ignore",
	"v6_exception_ported_from_iface_vxlanmgr_vp_creation_in_progress",
	"v6_exception_ported_from_iface_vxlan_retried_enough",
	"v6_exception_ported_from_iface_vxlan_not_enabled",
	"v6_exception_ported_from_iface_invalid_iface_id",
	"v6_exception_ported_from_iface_invalid_bottom_iface",
	"v6_exception_ported_from_iface_invalid_top_iface",
	"v6_exception_ported_from_iface_dsa_qinq_unsupported",
	"v6_exception_ported_from_iface_dsa_unsupported",
	"v6_exception_ported_to_iface_ingress_qdisc_unsupported",
	"v6_exception_ported_to_iface_qdisc_unsupported",
	"v6_exception_ported_to_iface_bridge_cascade",
	"v6_exception_ported_to_iface_ovs_bridge_cascade",
	"v6_exception_ported_to_iface_ovs_bridge_unsupported",
	"v6_exception_ported_to_iface_more_than_one_pppoe_unsupported",
	"v6_exception_ported_to_iface_pppoe_flow_invalid",
	"v6_exception_ported_to_iface_only_two_vlans_supported",
	"v6_exception_ported_to_iface_vlan_not_enabled",
	"v6_exception_ported_to_iface_ipsec_not_enabled",
	"v6_exception_ported_to_iface_vxlanmgr_vp_creation_in_progress",
	"v6_exception_ported_to_iface_vxlan_retried_enough",
	"v6_exception_ported_to_iface_vxlan_not_enabled",
	"v6_exception_ported_to_iface_invalid_iface_id",
	"v6_exception_ported_to_iface_invalid_bottom_iface",
	"v6_exception_ported_to_iface_invalid_top_iface",
	"v6_exception_ported_to_iface_dsa_qinq_unsupported",
	"v6_exception_ported_to_iface_dsa_unsupported",
	"v6_exception_ported_ppe_offload_disabled",
	"v6_exception_ported_regen_occurred",
	"v6_exception_ported_immediate_flush",
	"v6_exception_ported_decelerate_pending",
	"v6_exception_ported_ppe_accel_failed",
	"v6_exception_ported_bridge_vlan_filter_unsupported",
	"v6_exception_ported_unidir_update_fail",
	"v6_exception_ported_bidir_update_fail"
};

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
static const char *ecm_ppe_stats_v6_exception_non_ported_name_str[] = {
	"v6_exception_non_ported_accel_not_permitted",
	"v6_exception_non_ported_no_mem",
	"v6_exception_non_ported_no_from_interfaces",
	"v6_exception_non_ported_no_to_interfaces",
	"v6_exception_non_ported_invalid_bottom_iface",
	"v6_exception_non_ported_invalid_top_iface",
	"v6_exception_non_ported_from_iface_invalid_iface_id",
	"v6_exception_non_ported_from_iface_ingress_qdisc_unsupported",
	"v6_exception_non_ported_from_iface_qdisc_unsupported",
	"v6_exception_non_ported_from_iface_bridge_cascade",
	"v6_exception_non_ported_from_iface_ovs_bridge_cascade",
	"v6_exception_non_ported_from_iface_ovs_bridge_unsupported",
	"v6_exception_non_ported_from_iface_gretap_iface_mtu_unknown",
	"v6_exception_non_ported_from_iface_gretun_iface_mtu_unknown",
	"v6_exception_non_ported_from_iface_more_than_one_pppoe_unsupported",
	"v6_exception_non_ported_from_iface_pppoe_flow_invalid",
	"v6_exception_non_ported_from_iface_only_two_vlans_supported",
	"v6_exception_non_ported_from_iface_vlan_not_enabled",
	"v6_exception_non_ported_from_iface_ipsec_not_enabled",
	"v6_exception_non_ported_to_iface_invalid_iface_id",
	"v6_exception_non_ported_to_iface_ingress_qdisc_unsupported",
	"v6_exception_non_ported_to_iface_qdisc_unsupported",
	"v6_exception_non_ported_to_iface_bridge_cascade",
	"v6_exception_non_ported_to_iface_ovs_bridge_cascade",
	"v6_exception_non_ported_to_iface_ovs_bridge_unsupported",
	"v6_exception_non_ported_to_iface_more_than_one_pppoe_unsupported",
	"v6_exception_non_ported_to_iface_pppoe_flow_invalid",
	"v6_exception_non_ported_to_iface_only_two_vlans_supported",
	"v6_exception_non_ported_to_iface_vlan_not_enabled",
	"v6_exception_non_ported_to_iface_ipsec_not_enabled",
	"v6_exception_non_ported_ppe_offload_disabled",
	"v6_exception_non_ported_regen_occurred",
	"v6_exception_non_ported_immediate_flush",
	"v6_exception_non_ported_decelerate_pending",
	"v6_exception_non_ported_ppe_accel_failed",
	"v6_exception_non_ported_bridge_vlan_filter_unsupported",
	"v6_exception_non_ported_tunipip6_fmr_flow_offload_unsupported"
};
#endif

void ecm_ppe_stats_v6_inc(struct ecm_front_end_connection_instance *feci, ecm_ppe_stats_v6_exception_type_t stat_type, int stat_idx)
{
	switch (stat_type) {
	case ECM_PPE_STATS_V6_EXCEPTION_PORTED:
		atomic64_inc(&ppe_v6_stats.ecm_ppe_stats_v6_exception_ported[stat_idx]);
		break;
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	case ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED:
		atomic64_inc(&ppe_v6_stats.ecm_ppe_stats_v6_exception_non_ported[stat_idx]);
		break;
#endif
	default:
		DEBUG_TRACE("PPE Fallback Event type unknown.\n");
		break;
	}

	/*
	 * In ECM exceptions 0 means ACCEL_NOT_PERMITTED, it may overwrite the actual AE failure
	 */
	if (stat_idx) {
		atomic64_set(&feci->ppe_accel_fail_reason, stat_idx);
		atomic64_set(&feci->unidir_accel_fail_reason, stat_idx);
	}
}

static int ecm_ppe_stats_v6_exception_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	int i = 0;

	seq_puts(m, "\nECM PPE IPv6 EXCEPTION STATS\n\n");

	seq_puts(m, "\nPorted Stats:\n\n");
	for (i = 0; i < ECM_PPE_STATS_V6_EXCEPTION_PORTED_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_ppe_stats_v6_exception_ported_name_str[i], \
					atomic64_read(&ppe_v6_stats.ecm_ppe_stats_v6_exception_ported[i]));
	}

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	seq_puts(m, "\nNon-Ported Stats:\n\n");
	for (i = 0; i < ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_ppe_stats_v6_exception_non_ported_name_str[i], \
					atomic64_read(&ppe_v6_stats.ecm_ppe_stats_v6_exception_non_ported[i]));
	}
#endif

	return 0;
}

static int ecm_ppe_stats_v6_exception_open(struct inode *inode, struct file *file)
{
	return single_open(file, ecm_ppe_stats_v6_exception_show, inode->i_private);
}

const struct file_operations ecm_ppe_stats_v6_exception_file_ops = {
	.open = ecm_ppe_stats_v6_exception_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int ecm_ppe_stats_v6_debugfs_init(struct dentry *dentry)
{
	if (!ecm_debugfs_create_file("ecm_ppe_v6_exception_stats", S_IRUGO, dentry,
				NULL, &ecm_ppe_stats_v6_exception_file_ops)) {
		return -1;
	}

	return 0;
}
