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

#ifndef __ECM_PPE_STATS_V6_H
#define __ECM_PPE_STATS_V6_H

enum ecm_ppe_stats_v6_exception_ported_events {
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_ACCEL_NOT_PERMITTED,
    /* Number of IPv6 packets ignored in ported flow as accel not permitted */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_NO_MEM,
    /* Number of IPv6 packets ignored in ported flow as rule alloc failed */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_NO_FROM_INTERFACES,
    /* Number of IPv6 packets ignored in ported flow as no interface in from interface list */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_NO_TO_INTERFACES,
    /* Number of IPv6 packets ignored in ported flow as no interface in to interface list */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_INGRESS_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface ported flow as ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface ported flow as qos ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in from interface ported flow as bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_OVS_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in from interface ported flow as ovs bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_OVS_BRIDGE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface ported flow as ovs bridge not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_MORE_THAN_ONE_PPPOE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface ported flow as multiple PPPOE not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_PPPOE_FLOW_INVALID,
    /* Number of IPv6 packets ignored in from interface ported flow as PPPOE not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_ONLY_TWO_VLANS_SUPPORTED,
    /* Number of IPv6 packets ignored in from interface ported flow as multi vlan not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_VLAN_NOT_ENABLED,
    /* Number of IPv6 packets ignored in from interface ported flow as vlan not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_IPSEC_NOT_ENABLED,
    /* Number of IPv6 packets ignored in from interface ported flow as ipsec not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_VXLAN_ADDITIONAL_IGNORE,
    /* Number of IPv6 packets ignored in from interface ported flow as only one VxLAN interface can be supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_VXLANMGR_VP_CREATION_IN_PROGRESS,
    /* Number of IPv6 packets ignored in from interface ported flow as vxlan manager vp creation is in progress */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_VXLAN_RETRIED_ENOUGH,
    /* Number of IPv6 packets ignored in from interface ported flow as vxlan retry exausted */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_VXLAN_NOT_ENABLED,
    /* Number of IPv6 packets ignored in from interface ported flow as vxlan not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_INVALID_IFACE_ID,
    /* Number of IPv6 packets ignored in from interface ported flow as ppe doesn't support the respective iface */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_INVALID_BOTTOM_IFACE,
    /* Number of IPv6 packets ignored in from interface ported flow as first interface is unknown */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_INVALID_TOP_IFACE,
    /* Number of IPv6 packets ignored in from interface ported flow as top interfaces are unknown */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_DSA_QINQ_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from DSA interface with Q-in-Q is not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_FROM_IFACE_DSA_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from DSA interface is not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_INGRESS_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface ported flow as ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface ported flow as qos ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in to interface ported flow as bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_OVS_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in to interface ported flow as ovs bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_OVS_BRIDGE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface ported flow as ovs bridge not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_MORE_THAN_ONE_PPPOE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface ported flow as multiple PPPOE not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_PPPOE_FLOW_INVALID,
    /* Number of IPv6 packets ignored in to interface ported flow as PPPOE not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_ONLY_TWO_VLANS_SUPPORTED,
    /* Number of IPv6 packets ignored in to interface ported flow as multi vlan not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_VLAN_NOT_ENABLED,
    /* Number of IPv6 packets ignored in to interface ported flow as vlan not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_IPSEC_NOT_ENABLED,
    /* Number of IPv6 packets ignored in to interface ported flow as ipsec not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_VXLANMGR_VP_CREATION_IN_PROGRESS,
    /* Number of IPv6 packets ignored in to interface ported flow as vxlan manager vp creation is in progress */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_VXLAN_RETRIED_ENOUGH,
    /* Number of IPv6 packets ignored in to interface ported flow as vxlan retry exausted */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_VXLAN_NOT_ENABLED,
    /* Number of IPv6 packets ignored in to interface ported flow as vxlan not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_INVALID_IFACE_ID,
    /* Number of IPv6 packets ignored in to interface ported flow as ppe doesn't support the respective iface */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_INVALID_BOTTOM_IFACE,
    /* Number of IPv6 packets ignored in to interface ported flow as first interface is unknown */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_INVALID_TOP_IFACE,
    /* Number of IPv6 packets ignored in to interface ported flow as top interfaces are unknown */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_DSA_QINQ_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to DSA interface with Q-in-Q is not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_TO_IFACE_DSA_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to DSA interface is not supported */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_PPE_OFFLOAD_DISABLED,
    /* Number of IPv6 packets ignored in ported flow as ppe offload is disabled */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_REGEN_OCCURRED,
    /* Number of IPv6 packets ignored in ported flow as connection regen occured */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_IMMEDIATE_FLUSH,
    /* Number of IPv6 packets ignored in ported flow as flush happened just after ppe rule IPv6 push success and before calling accelerate_done */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_DECELERATE_PENDING,
    /* Number of IPv6 packets ignored in ported flow as decelerate was pending */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_PPE_ACCEL_FAILED,
    /* Number of IPv6 packets ignored in ported flow as the ppe_drv_v6_create() failed */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_BRIDGE_VLAN_FILTER_UNSUPPORTED,
    /* Number of IPv6 packets ignored in ported flow as bridge vlan filtering not supported in PPE */
    ECM_PPE_STATS_V6_EXCEPTION_PORTED_MAX
};

#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
enum ecm_ppe_stats_v6_exception_non_ported_events {
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_ACCEL_NOT_PERMITTED,
    /* Number of IPv6 packets ignored in non-ported flow as accel not permitted */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_NO_MEM,
    /* Number of IPv6 packets ignored in non-ported flow as rule alloc failed */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_NO_FROM_INTERFACES,
    /* Number of IPv6 packets ignored in non-ported flow as no interface in from interface list */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_NO_TO_INTERFACES,
    /* Number of IPv6 packets ignored in non-ported flow as no interface in to interface list */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_INVALID_BOTTOM_IFACE,
    /* Number of IPv6 packets ignored in non-ported flow as first interface is unknown */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_INVALID_TOP_IFACE,
    /* Number of IPv6 packets ignored in non-ported flow as top interfaces are unknown */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_INVALID_IFACE_ID,
    /* Number of IPv6 packets ignored in from interface non-ported flow as ppe doesn't support the respective iface */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_INGRESS_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as qos ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in from interface non-ported flow as bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_OVS_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in from interface non-ported flow as ovs bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_OVS_BRIDGE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as ovs bridge not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_GRETAP_IFACE_MTU_UNKNOWN,
    /* Number of IPv6 packets ignored in from interface non-ported flow GRE tap interface mtu not available */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_GRETUN_IFACE_MTU_UNKNOWN,
    /* Number of IPv4 packets ignored in from interface non-ported flow GRE tun interface mtu not available */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_MORE_THAN_ONE_PPPOE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as multiple PPPOE not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_PPPOE_FLOW_INVALID,
    /* Number of IPv6 packets ignored in from interface non-ported flow as PPPOE not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_ONLY_TWO_VLANS_SUPPORTED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as multi vlan not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_VLAN_NOT_ENABLED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as vlan not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_FROM_IFACE_IPSEC_NOT_ENABLED,
    /* Number of IPv6 packets ignored in from interface non-ported flow as multiple ipsec not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_INVALID_IFACE_ID,
    /* Number of IPv6 packets ignored in to interface non-ported flow as ppe doesn't support the respective iface */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_INGRESS_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_QDISC_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as qos ingress qdisc not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in to interface non-ported flow as bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_OVS_BRIDGE_CASCADE,
    /* Number of IPv6 packets ignored in to interface non-ported flow as ovs bridge cascade not possible */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_OVS_BRIDGE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as ovs bridge not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_MORE_THAN_ONE_PPPOE_UNSUPPORTED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as multiple PPPOE not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_PPPOE_FLOW_INVALID,
    /* Number of IPv6 packets ignored in to interface non-ported flow as PPPOE not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_ONLY_TWO_VLANS_SUPPORTED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as multi vlan not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_VLAN_NOT_ENABLED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as vlan not enabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TO_IFACE_IPSEC_NOT_ENABLED,
    /* Number of IPv6 packets ignored in to interface non-ported flow as ipsec not supported */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_PPE_OFFLOAD_DISABLED,
    /* Number of IPv6 packets ignored in non-ported flow as ppe offload is disabled */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_REGEN_OCCURRED,
    /* Number of IPv6 packets ignored in non-ported flow as connection regen occured */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_IMMEDIATE_FLUSH,
    /* Number of IPv6 packets ignored in non-ported flow as flush happened just after ppe rule IPv6 push success and before calling accelerate_done */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_DECELERATE_PENDING,
    /* Number of IPv6 packets ignored in non-ported flow as decelerate was pending */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_PPE_ACCEL_FAILED,
    /* Number of IPv6 packets ignored in non-ported flow as the ppe_drv_v6_create() failed */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_BRIDGE_VLAN_FILTER_UNSUPPORTED,
    /* Number of IPv6 packets ignored in non-ported flow as bridge vlan filtering not supported in PPE */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_TUNIPIP6_FMR_FLOW_OFFLOAD_UNSUPPORTED,
    /* Number of IPv6 packets ignored in non-ported flow as tunipip6 FMR flow offload not supported in PPE */
    ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_MAX
};
#endif

typedef enum ecm_ppe_stats_v6_exception_type {
	ECM_PPE_STATS_V6_EXCEPTION_PORTED,
	ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED,
	ECM_PPE_STATS_V6_EXCEPTION_MAX
} ecm_ppe_stats_v6_exception_type_t;

struct ecm_ppe_stats_v6 {
	atomic64_t ecm_ppe_stats_v6_exception_ported[ECM_PPE_STATS_V6_EXCEPTION_PORTED_MAX];
#ifdef ECM_NON_PORTED_SUPPORT_ENABLE
	atomic64_t ecm_ppe_stats_v6_exception_non_ported[ECM_PPE_STATS_V6_EXCEPTION_NON_PORTED_MAX];
#endif
};

void ecm_ppe_stats_v6_inc(ecm_ppe_stats_v6_exception_type_t stat_type, int stat_idx);
int ecm_ppe_stats_v6_debugfs_init(struct dentry *dentry);

#endif
