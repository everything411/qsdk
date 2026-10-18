/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_sfe.c
 *	Netfn flow manager sfe file
 */

#include <linux/types.h>
#include <netfn_flowmgr.h>
#include <flowmgr/netfn_flowmgr_priv.h>
#include "netfn_flowmgr_sfe.h"
#include "netfn_flowmgr_sfe_stats.h"
#include "netfn_flowmgr_sfe_ipv4.h"
#include "netfn_flowmgr_sfe_ipv6.h"

/*
 * netfn_flowmgr_sfe_validate_vlan_info()
 *	Validate vlan info in original and reply direction
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_validate_vlan_info(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	uint32_t org_inner_ingress_vlan_tag;
	uint32_t org_inner_egress_vlan_tag;
	uint32_t org_outer_ingress_vlan_tag;
	uint32_t org_outer_egress_vlan_tag;
	uint32_t reply_inner_ingress_vlan_tag;
	uint32_t reply_inner_egress_vlan_tag;
	uint32_t reply_outer_ingress_vlan_tag;
	uint32_t reply_outer_egress_vlan_tag;
	struct netfn_flowmgr_debug_stats *stats;
	netfn_flowmgr_ret_t status = NETFN_FLOWMGR_RET_SUCCESS;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];
	/*
	 * Original direction inner and outer vlan rules
	 */
	org_inner_ingress_vlan_tag = original->rule_info.vlan_rule.inner.ingress_vlan_tag;
	org_inner_egress_vlan_tag = original->rule_info.vlan_rule.inner.egress_vlan_tag;
	org_outer_ingress_vlan_tag = original->rule_info.vlan_rule.outer.ingress_vlan_tag;
	org_outer_egress_vlan_tag = original->rule_info.vlan_rule.outer.egress_vlan_tag;

	/*
	 * Reply direction inner and outer vlan rules
	 */
	reply_inner_ingress_vlan_tag = reply->rule_info.vlan_rule.inner.ingress_vlan_tag;
	reply_inner_egress_vlan_tag = reply->rule_info.vlan_rule.inner.egress_vlan_tag;
	reply_outer_ingress_vlan_tag = reply->rule_info.vlan_rule.outer.ingress_vlan_tag;
	reply_outer_egress_vlan_tag = reply->rule_info.vlan_rule.outer.egress_vlan_tag;

	/*
	 * Inner VLAN(primary) rule match
	 */
	if ((org_inner_ingress_vlan_tag != reply_inner_egress_vlan_tag) || (org_inner_egress_vlan_tag != reply_inner_ingress_vlan_tag)) {
		netfn_flowmgr_warn("Inner VLAN config mismatch in org and reply direction\n");
		netfn_flowmgr_warn("org_inner_ingress_vlan_tag = 0x%x, org_inner_egress_vlan_tag = 0x%x\n",
						org_inner_ingress_vlan_tag, org_inner_egress_vlan_tag);
		netfn_flowmgr_warn("reply_inner_ingress_vlan_tag = 0x%x, reply_inner_egress_vlan_tag = 0x%x\n",
						reply_inner_ingress_vlan_tag, reply_inner_egress_vlan_tag);
		netfn_flowmgr_stats_inc(&stats->validate_vlan_inner_mismatch);
		status = NETFN_FLOWMGR_RET_SFE_INNER_VLAN_MISMATCH;
	}

	/*
	 * Outer VLAN(secondary) rule match
	 */
	if ((org_outer_ingress_vlan_tag != reply_outer_egress_vlan_tag) || (org_outer_egress_vlan_tag != reply_outer_ingress_vlan_tag)) {
		netfn_flowmgr_warn("Outer VLAN config mismatch in org and reply direction\n");
		netfn_flowmgr_warn("org_outer_ingress_vlan_tag = 0x%x, org_outer_egress_vlan_tag = 0x%x\n",
						org_outer_ingress_vlan_tag, org_outer_egress_vlan_tag);
		netfn_flowmgr_warn("reply_outer_ingress_vlan_tag = 0x%x, reply_outer_egress_vlan_tag = 0x%x\n",
						reply_outer_ingress_vlan_tag, reply_outer_egress_vlan_tag);
		netfn_flowmgr_stats_inc(&stats->validate_vlan_outer_mismatch);
		status = NETFN_FLOWMGR_RET_SFE_OUTER_VLAN_MISMATCH;
	}
	return status;
}

/*
 * netfn_flowmgr_sfe_get_stats()
 *	Get SFE stats for a single connection
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_get_stats(struct netfn_flowmgr_flow_conn_stats *stats)
{
	if (stats->tuple.ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		return netfn_flowmgr_sfe_v4_get_stats(stats);
	}
	else if (stats->tuple.ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		return netfn_flowmgr_sfe_v6_get_stats(stats);
	}

	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INCORRECT_IP_VERSION, 0);
}

/*
 * netfn_flowmgr_sfe_destroy_rule()
 *	destroys flow in sfe
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_destroy_rule(struct netfn_flowmgr_destroy_rule *original, struct netfn_flowmgr_destroy_rule *reply)
{
	enum netfn_flowmgr_tuple_ip_version org_ipver = original->tuple.ip_version;
	enum netfn_flowmgr_tuple_ip_version reply_ipver = reply->tuple.ip_version;

	/*
	 * IP version check
	 */
	if (org_ipver != reply_ipver) {
		netfn_flowmgr_warn("IP version is not matching for original and reply direction\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_IP_VERSION_MISMATCH, 0);
	}

	if(org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		return netfn_flowmgr_sfe_destroy_v4_rule(original, reply);
	} else if (org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		return netfn_flowmgr_sfe_destroy_v6_rule(original, reply);
	}

	netfn_flowmgr_warn("Incorrect IP version\n");
	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INCORRECT_IP_VERSION, 0);
}

/*
 * netfn_flowmgr_sfe_create_rule()
 *	accels flow in sfe
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_create_rule(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply)
{
	enum netfn_flowmgr_tuple_ip_version org_ipver;
	enum netfn_flowmgr_tuple_ip_version reply_ipver;

	org_ipver = original->tuple.ip_version;
	reply_ipver = reply->tuple.ip_version;

	/*
	 * IP version check
	 */
	if (org_ipver != reply_ipver) {
		netfn_flowmgr_warn("IP version is not matching for original and reply direction\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_IP_VERSION_MISMATCH, 0);
	}

	if(org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		return netfn_flowmgr_sfe_create_v4_rule(original, reply);
	} else if (org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		return netfn_flowmgr_sfe_create_v6_rule(original, reply);
	}

	netfn_flowmgr_warn("Incorrect IP version\n");
	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INCORRECT_IP_VERSION, 0);
}

/*
 * netfn_flowmgr_sfe_init()
 *	SFE Initialization
 */
bool netfn_flowmgr_sfe_init(void)
{
	struct netfn_flowmgr_ae_ops sfe_ops = {0};

	/*
	 * Set Callback Pointers
	 */
	sfe_ops.create_rule = netfn_flowmgr_sfe_create_rule;
	sfe_ops.destroy_rule = netfn_flowmgr_sfe_destroy_rule;
	sfe_ops.get_stats = netfn_flowmgr_sfe_get_stats;
	sfe_ops.add_dscp_priority = NULL;
	sfe_ops.del_dscp_priority = NULL;

	/*
	 * Register the callbacks with netfn core.
	 */
	netfn_flowmgr_ae_ops_register(NETFN_FLOWMGR_AE_TYPE_SFE, &sfe_ops);

	if (!netfn_flowmgr_sfe_ipv4_init()) {
		netfn_flowmgr_warn("Netfn flowmgr SFE IPv4 initialization failed\n");
		return false;
	}
	if (!netfn_flowmgr_sfe_ipv6_init()) {
		netfn_flowmgr_warn("Netfn flowmgr SFE IPv6 initialization failed\n");
		return false;
	}
	return true;
}

/*
 * netfn_flowmgr_sfe_deinit()
 *	SFE Exit
 */
void netfn_flowmgr_sfe_deinit(void)
{
	/*
	 * Unregister the callbacks with netfn core.
	 */
	netfn_flowmgr_ae_ops_unregister(NETFN_FLOWMGR_AE_TYPE_SFE);

	netfn_flowmgr_sfe_ipv4_deinit();
	netfn_flowmgr_sfe_ipv6_deinit();
}
