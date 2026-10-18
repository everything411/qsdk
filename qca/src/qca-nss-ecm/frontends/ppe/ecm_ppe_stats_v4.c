/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include "ecm_ppe_stats_v4.h"

struct ecm_ppe_stats_v4 ppe_v4_stats;

static const char *ecm_ppe_stats_v4_exception_ported_name_str[] = {
	"v4_exception_ported_accel_not_permitted",
	"v4_exception_ported_no_mem",
	"v4_exception_ported_no_from_interfaces",
	"v4_exception_ported_no_to_interfaces",
	"v4_exception_ported_from_iface_ingress_qdisc_unsupported",
	"v4_exception_ported_from_iface_qdisc_unsupported",
	"v4_exception_ported_from_iface_bridge_cascade",
	"v4_exception_ported_from_iface_ovs_bridge_cascade",
	"v4_exception_ported_from_iface_ovs_bridge_unsupported",
	"v4_exception_ported_from_iface_more_than_one_pppoe_unsupported",
	"v4_exception_ported_from_iface_pppoe_flow_invalid",
	"v4_exception_ported_from_iface_only_two_vlans_supported",
	"v4_exception_ported_from_iface_vlan_not_enabled",
	"v4_exception_ported_from_iface_ipsec_not_enabled",
	"v4_exception_ported_from_iface_vxlan_additional_ignore",
	"v4_exception_ported_from_iface_vxlanmgr_vp_creation_in_progress",
	"v4_exception_ported_from_iface_vxlan_retried_enough",
	"v4_exception_ported_from_iface_vxlan_not_enabled",
	"v4_exception_ported_from_iface_invalid_iface_id",
	"v4_exception_ported_from_iface_invalid_bottom_iface",
	"v4_exception_ported_from_iface_invalid_top_iface",
	"v4_exception_ported_from_iface_dsa_qinq_unsupported",
	"v4_exception_ported_from_iface_dsa_unsupported",
	"v4_exception_ported_to_iface_ingress_qdisc_unsupported",
	"v4_exception_ported_to_iface_qdisc_unsupported",
	"v4_exception_ported_to_iface_bridge_cascade",
	"v4_exception_ported_to_iface_ovs_bridge_cascade",
	"v4_exception_ported_to_iface_ovs_bridge_unsupported",
	"v4_exception_ported_to_iface_more_than_one_pppoe_unsupported",
	"v4_exception_ported_to_iface_pppoe_flow_invalid",
	"v4_exception_ported_to_iface_only_two_vlans_supported",
	"v4_exception_ported_to_iface_vlan_not_enabled",
	"v4_exception_ported_to_iface_vxlan_additional_ignore",
	"v4_exception_ported_to_iface_vxlanmgr_vp_creation_in_progress",
	"v4_exception_ported_to_iface_vxlan_retried_enough",
	"v4_exception_ported_to_iface_vxlan_not_enabled",
	"v4_exception_ported_to_iface_invalid_iface_id",
	"v4_exception_ported_to_iface_invalid_bottom_iface",
	"v4_exception_ported_to_iface_invalid_top_iface",
	"v4_exception_ported_to_iface_dsa_qinq_unsupported",
	"v4_exception_ported_to_iface_dsa_unsupported",
	"v4_exception_ported_ppe_offload_disabled",
	"v4_exception_ported_regen_occurred",
	"v4_exception_ported_immediate_flush",
	"v4_exception_ported_decelerate_pending",
	"v4_exception_ported_ppe_accel_failed",
	"v4_exception_ported_bridge_vlan_filter_unsupported",
	"v4_exception_ported_tunipip6_fmr_flow_offload_unsupported"
};

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
static const char *ecm_ppe_stats_v4_exception_non_ported_name_str[] = {
	"v4_exception_non_ported_accel_not_permitted",
	"v4_exception_non_ported_no_mem",
	"v4_exception_non_ported_no_from_interfaces",
	"v4_exception_non_ported_no_to_interfaces",
	"v4_exception_non_ported_invalid_bottom_iface",
	"v4_exception_non_ported_invalid_top_iface",
	"v4_exception_non_ported_from_iface_invalid_iface_id",
	"v4_exception_non_ported_from_iface_ingress_qdisc_unsupported",
	"v4_exception_non_ported_from_iface_qdisc_unsupported",
	"v4_exception_non_ported_from_iface_bridge_cascade",
	"v4_exception_non_ported_from_iface_ovs_bridge_cascade",
	"v4_exception_non_ported_from_iface_ovs_bridge_unsupported",
	"v4_exception_non_ported_from_iface_gretap_iface_mtu_unknown",
	"v4_exception_non_ported_from_iface_gretun_iface_mtu_unknown",
	"v4_exception_non_ported_from_iface_more_than_one_pppoe_unsupported",
	"v4_exception_non_ported_from_iface_pppoe_flow_invalid",
	"v4_exception_non_ported_from_iface_only_two_vlans_supported",
	"v4_exception_non_ported_from_iface_vlan_not_enabled",
	"v4_exception_non_ported_from_iface_ipsec_not_enabled",
	"v4_exception_non_ported_to_iface_invalid_iface_id",
	"v4_exception_non_ported_to_iface_ingress_qdisc_unsupported",
	"v4_exception_non_ported_to_iface_qdisc_unsupported",
	"v4_exception_non_ported_to_iface_bridge_cascade",
	"v4_exception_non_ported_to_iface_ovs_bridge_cascade",
	"v4_exception_non_ported_to_iface_ovs_bridge_unsupported",
	"v4_exception_non_ported_to_iface_more_than_one_pppoe_unsupported",
	"v4_exception_non_ported_to_iface_pppoe_flow_invalid",
	"v4_exception_non_ported_to_iface_only_two_vlans_supported",
	"v4_exception_non_ported_to_iface_vlan_not_enabled",
	"v4_exception_non_ported_to_iface_ipsec_not_enabled",
	"v4_exception_non_ported_ppe_offload_disabled",
	"v4_exception_non_ported_regen_occurred",
	"v4_exception_non_ported_immediate_flush",
	"v4_exception_non_ported_decelerate_pending",
	"v4_exception_non_ported_ppe_accel_failed",
	"v4_exception_non_ported_bridge_vlan_filter_unsupported"
};
#endif

void ecm_ppe_stats_v4_inc(ecm_ppe_stats_v4_exception_type_t stat_type, int stat_idx)
{
	switch (stat_type) {
	case ECM_PPE_STATS_V4_EXCEPTION_PORTED:
		atomic64_inc(&ppe_v4_stats.ecm_ppe_stats_v4_exception_ported[stat_idx]);
		break;
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	case ECM_PPE_STATS_V4_EXCEPTION_NON_PORTED:
		atomic64_inc(&ppe_v4_stats.ecm_ppe_stats_v4_exception_non_ported[stat_idx]);
		break;
#endif
	default:
		DEBUG_TRACE("PPE Fallback Event type unknown.\n");
		break;
	}
}

static int ecm_ppe_stats_v4_exception_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	int i = 0;

	seq_puts(m, "\nECM PPE IPv4 EXCEPTION STATS\n\n");

	seq_puts(m, "\nPorted Stats:\n\n");
	for (i = 0; i < ECM_PPE_STATS_V4_EXCEPTION_PORTED_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_ppe_stats_v4_exception_ported_name_str[i], \
					atomic64_read(&ppe_v4_stats.ecm_ppe_stats_v4_exception_ported[i]));
	}

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	seq_puts(m, "\nNon-Ported Stats:\n\n");
	for (i = 0; i < ECM_PPE_STATS_V4_EXCEPTION_NON_PORTED_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_ppe_stats_v4_exception_non_ported_name_str[i], \
					atomic64_read(&ppe_v4_stats.ecm_ppe_stats_v4_exception_non_ported[i]));
	}
#endif

	return 0;
}


static int ecm_ppe_stats_v4_exception_open(struct inode *inode, struct file *file)
{
	return single_open(file, ecm_ppe_stats_v4_exception_show, inode->i_private);
}

const struct file_operations ecm_ppe_stats_v4_exception_file_ops = {
	.open = ecm_ppe_stats_v4_exception_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int ecm_ppe_stats_v4_debugfs_init(struct dentry *dentry)
{
	if (!debugfs_create_file("ecm_ppe_v4_exception_stats", S_IRUGO, dentry,
				NULL, &ecm_ppe_stats_v4_exception_file_ops)) {
		return -1;
	}

	return 0;
}
