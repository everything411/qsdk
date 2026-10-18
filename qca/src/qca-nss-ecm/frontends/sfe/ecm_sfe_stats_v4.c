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

#define DEBUG_LEVEL ECM_SFE_STATS_DEBUG_LEVEL
#include "ecm_types.h"
#include "ecm_sfe_stats_v4.h"

struct ecm_sfe_stats_v4 sfe_v4_stats;

static const char *ecm_sfe_stats_v4_exception_multicast_name_str[] = {
	"v4_exception_multicast_accel_not_permitted",
	"v4_exception_multicast_no_mem",
	"v4_exception_multicast_no_from_interfaces",
	"v4_exception_multicast_from_iface_invalid_bottom_iface",
	"v4_exception_multicast_from_iface_macvlan_not_enabled",
	"v4_exception_multicast_from_iface_only_two_vlans_supported",
	"v4_exception_multicast_from_iface_vlan_not_enabled",
	"v4_exception_multicast_from_iface_more_than_one_pppoe_unsupported",
	"v4_exception_multicast_from_iface_pppoe_flow_invalid",
	"v4_exception_multicast_from_iface_dsa_unsupported",
	"v4_exception_multicast_from_iface_dsa_qinq_unsupported",
	"v4_exception_multicast_no_to_interfaces",
	"v4_exception_multicast_to_iface_type_bridge_ignore",
	"v4_exception_multicast_to_iface_invalid_iface_id",
	"v4_exception_multicast_to_iface_lag_additional_ignore",
	"v4_exception_multicast_to_iface_lag_not_present",
	"v4_exception_multicast_to_iface_lag_not_mlo",
	"v4_exception_multicast_to_iface_lag_not_enabled",
	"v4_exception_multicast_to_iface_more_than_one_pppoe_unsupported",
	"v4_exception_multicast_to_iface_pppoe_flow_invalid",
	"v4_exception_multicast_to_iface_only_two_vlans_supported",
	"v4_exception_multicast_to_iface_vlan_not_enabled",
	"v4_exception_multicast_to_iface_dsa_unsupported",
	"v4_exception_multicast_to_iface_dsa_qinq_unsupported",
	"v4_exception_multicast_regen_occurred",
	"v4_exception_multicast_tx_failed"
};

static const char *ecm_sfe_stats_v4_exception_ported_name_str[] = {
	"v4_exception_ported_accel_not_permitted",
	"v4_exception_ported_no_mem",
	"v4_exception_ported_no_from_interfaces",
	"v4_exception_ported_no_to_interfaces",
	"v4_exception_ported_invalid_bottom_iface",
	"v4_exception_ported_from_iface_bridge_cascade",
	"v4_exception_ported_from_iface_ovs_bridge_cascade",
	"v4_exception_ported_from_iface_ovs_internal_cascade",
	"v4_exception_ported_from_iface_ovs_bridge_unsupported",
	"v4_exception_ported_from_iface_more_than_one_pppoe_unsupported",
	"v4_exception_ported_from_iface_pppoe_flow_invalid",
	"v4_exception_ported_from_iface_only_two_vlans_supported",
	"v4_exception_ported_from_iface_vlan_not_enabled",
	"v4_exception_ported_from_iface_macvlan_not_enabled",
	"v4_exception_ported_from_iface_only_one_ipsec_supported",
	"v4_exception_ported_from_iface_ipsec_not_enabled",
	"v4_exception_ported_from_iface_lag_not_enabled",
	"v4_exception_ported_from_iface_vxlan_not_enabled",
	"v4_exception_ported_from_iface_dsa_qinq_unsupported",
	"v4_exception_ported_from_iface_dsa_unsupported",
	"v4_exception_ported_to_iface_bridge_cascade",
	"v4_exception_ported_to_iface_ovs_bridge_cascade",
	"v4_exception_ported_to_iface_ovs_internal_cascade",
	"v4_exception_ported_to_iface_ovs_bridge_unsupported",
	"v4_exception_ported_to_iface_more_than_one_pppoe_unsupported",
	"v4_exception_ported_to_iface_pppoe_flow_invalid",
	"v4_exception_ported_to_iface_only_two_vlans_supported",
	"v4_exception_ported_to_iface_vlan_not_enabled",
	"v4_exception_ported_to_iface_macvlan_not_enabled",
	"v4_exception_ported_to_iface_only_one_ipsec_supported",
	"v4_exception_ported_to_iface_ipsec_not_enabled",
	"v4_exception_ported_to_iface_lag_not_enabled",
	"v4_exception_ported_to_iface_vxlan_not_enabled",
	"v4_exception_ported_to_iface_dsa_qinq_unsupported",
	"v4_exception_ported_to_iface_dsa_unsupported",
	"v4_exception_ported_mht_port_failed",
	"v4_exception_ported_regen_occurred",
	"v4_exception_ported_tx_failed"
};

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
static const char *ecm_sfe_stats_v4_exception_non_ported_name_str[] = {
	"v4_exception_non_ported_accel_not_permitted",
	"v4_exception_non_ported_no_mem",
	"v4_exception_non_ported_no_from_interfaces",
	"v4_exception_non_ported_no_to_interfaces",
	"v4_exception_non_ported_invalid_bottom_iface",
	"v4_exception_non_ported_from_iface_type_bridge_ignore",
	"v4_exception_non_ported_from_iface_type_ovs_bridge_ignore",
	"v4_exception_non_ported_from_iface_ovs_bridge_unsupported",
	"v4_exception_non_ported_from_iface_gretap|l2tpv3_iface_mtu_unknown",
	"v4_exception_non_ported_from_iface_gretrun_iface_mtu_unknown",
	"v4_exception_non_ported_from_iface_more_than_one_pppoe_unsupported",
	"v4_exception_non_ported_from_iface_pppoe_flow_invalid",
	"v4_exception_non_ported_from_iface_only_two_vlans_supported",
	"v4_exception_non_ported_from_iface_vlan_not_enabled",
	"v4_exception_non_ported_from_iface_only_one_ipsec_supported",
	"v4_exception_non_ported_from_iface_ipsec_not_enabled",
	"v4_exception_non_ported_from_iface_pptp_iface_mtu_unknown",
	"v4_exception_non_ported_from_iface_pptp_not_enabled",
	"v4_exception_non_ported_from_iface_lag_not_enabled",
	"v4_exception_non_ported_to_iface_type_bridge_ignore",
	"v4_exception_non_ported_to_iface_type_ovs_bridge_ignore",
	"v4_exception_non_ported_to_iface_ovs_bridge_unsupported",
	"v4_exception_non_ported_to_iface_more_than_one_pppoe_unsupported",
	"v4_exception_non_ported_to_iface_pppoe_flow_invalid",
	"v4_exception_non_ported_to_iface_only_two_vlans_supported",
	"v4_exception_non_ported_to_iface_vlan_not_enabled",
	"v4_exception_non_ported_to_iface_only_one_ipsec_supported",
	"v4_exception_non_ported_to_iface_ipsec_not_enabled",
	"v4_exception_non_ported_to_iface_lag_not_enabled",
	"v4_exception_non_ported_mht_port_failed",
	"v4_exception_non_ported_regen_occurred",
	"v4_exception_non_ported_tx_failed"
};
#endif

void ecm_sfe_stats_v4_inc(ecm_sfe_stats_v4_exception_type_t stat_type, int stat_idx)
{
	switch (stat_type) {
	case ECM_SFE_STATS_V4_EXCEPTION_MULTICAST:
		atomic64_inc(&sfe_v4_stats.ecm_sfe_stats_v4_exception_multicast[stat_idx]);
		break;
	case ECM_SFE_STATS_V4_EXCEPTION_PORTED:
		atomic64_inc(&sfe_v4_stats.ecm_sfe_stats_v4_exception_ported[stat_idx]);
		break;
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	case ECM_SFE_STATS_V4_EXCEPTION_NON_PORTED:
		atomic64_inc(&sfe_v4_stats.ecm_sfe_stats_v4_exception_non_ported[stat_idx]);
		break;
#endif
	default:
		DEBUG_TRACE("SFE Fallback Event type unknown.\n");
		break;
	}
}

static int ecm_sfe_stats_v4_exception_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	int i = 0;

	seq_puts(m, "\nECM SFE IPv4 EXCEPTION STATS\n\n");

    seq_puts(m, "\nMulticast Stats:\n\n");
	for (i = 0; i < ECM_SFE_STATS_V4_EXCEPTION_MULTICAST_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_sfe_stats_v4_exception_multicast_name_str[i], \
					atomic64_read(&sfe_v4_stats.ecm_sfe_stats_v4_exception_multicast[i]));
	}

	seq_puts(m, "\nPorted Stats:\n\n");
	for (i = 0; i < ECM_SFE_STATS_V4_EXCEPTION_PORTED_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_sfe_stats_v4_exception_ported_name_str[i], \
					atomic64_read(&sfe_v4_stats.ecm_sfe_stats_v4_exception_ported[i]));
	}

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	seq_puts(m, "\nNon-Ported Stats:\n\n");
	for (i = 0; i < ECM_SFE_STATS_V4_EXCEPTION_NON_PORTED_MAX; i++) {
		seq_printf(m, "\t\t %-70s:  %-10llu\n", ecm_sfe_stats_v4_exception_non_ported_name_str[i], \
					atomic64_read(&sfe_v4_stats.ecm_sfe_stats_v4_exception_non_ported[i]));
	}
#endif

	return 0;
}


static int ecm_sfe_stats_v4_exception_open(struct inode *inode, struct file *file)
{
	return single_open(file, ecm_sfe_stats_v4_exception_show, inode->i_private);
}

const struct file_operations ecm_sfe_stats_v4_exception_file_ops = {
	.open = ecm_sfe_stats_v4_exception_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

int ecm_sfe_stats_v4_debugfs_init(struct dentry *dentry)
{
	if (!debugfs_create_file("ecm_sfe_v4_exception_stats", S_IRUGO, dentry,
				NULL, &ecm_sfe_stats_v4_exception_file_ops)) {
		return -1;
	}

	return 0;
}
