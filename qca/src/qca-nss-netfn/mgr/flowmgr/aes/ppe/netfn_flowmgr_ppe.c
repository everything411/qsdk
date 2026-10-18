/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_ppe.c
 *	Netfn flow manager ppe file
 */
#include <linux/types.h>
#include <linux/if_ether.h>
#include <netfn_flowmgr.h>
#include <flowmgr/netfn_flowmgr_priv.h>
#include "netfn_flowmgr_ppe.h"
#include "netfn_flowmgr_ppe_ipv4.h"
#include "netfn_flowmgr_ppe_ipv6.h"

/*
 * Maximum queue priority supported per core
 */
#define NETFN_MAX_PRI_PER_CORE  8

/*
 * netfn_flowmgr_ppe_create_rule()
 *	Create ppe flow rule
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_create_rule(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply)
{
	enum netfn_flowmgr_tuple_ip_version org_ipver;
	enum netfn_flowmgr_tuple_ip_version reply_ipver;

	org_ipver = original->tuple.ip_version;
	reply_ipver = reply->tuple.ip_version;

	/*
	 * IP version check
	 * Both original and reply direction should have matching ip version.
	 */
	if (org_ipver != reply_ipver) {
		netfn_flowmgr_warn("IP version is not matching for original and reply direction\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_IP_VERSION_MISMATCH, 0);
	}

	if(org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		/*
		 * If IP version is v4, call v4 create rule API
		 */
		return netfn_flowmgr_ppe_create_v4_rule(original, reply);
	}
	else if (org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		/*
		 * If IP version is v6, call v6 create rule API
		 */
		return netfn_flowmgr_ppe_create_v6_rule(original, reply);
	} else {
		netfn_flowmgr_warn("Incorrect IP version\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INCORRECT_IP_VERSION, 0);
	}
}

/*
 * netfn_flowmgr_ppe_destroy_rule()
 *	Destroy ppe flow rule
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_destroy_rule(struct netfn_flowmgr_destroy_rule *original, struct netfn_flowmgr_destroy_rule *reply)
{
	enum netfn_flowmgr_tuple_ip_version org_ipver;
	enum netfn_flowmgr_tuple_ip_version reply_ipver;

	org_ipver = original->tuple.ip_version;
	reply_ipver = reply->tuple.ip_version;

	/*
	 * IP version check
	 * Both original and reply direction should have matching ip version.
	 */
	if (org_ipver != reply_ipver) {
		netfn_flowmgr_warn("IP version is not matching for original and reply direction\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_IP_VERSION_MISMATCH, 0);
	}

	if(org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		/*
		 * If IP version is v4, call v4 destroy rule API
		 */
		return netfn_flowmgr_ppe_destroy_v4_rule(original, reply);
	} else if (org_ipver == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		/*
		 * If IP version is v6, call v6 destroy rule API
		 */
		return netfn_flowmgr_ppe_destroy_v6_rule(original, reply);
	} else {
		netfn_flowmgr_warn("Incorrect IP version\n");
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INCORRECT_IP_VERSION, 0);
	}
}

/*
 * netfn_flowmgr_ppe_endianess_be_to_le()
 *	Change the endianess from network to host order.
 */
void netfn_flowmgr_ppe_endianess_be_to_le(uint32_t ip_addr[], int ip_version)
{
	/*
	 * Change the endianess of IPv4 addr from network to host
	 */
	if (ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		ip_addr[0] = ntohl(ip_addr[0]);
	}

	/*
	 * Change the endianess of IPv6 addr from network to host
	 */
	if (ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		ip_addr[0] = ntohl(ip_addr[0]);
		ip_addr[1] = ntohl(ip_addr[1]);
		ip_addr[2] = ntohl(ip_addr[2]);
		ip_addr[3] = ntohl(ip_addr[3]);
	}
}

/*
 * netfn_flowmgr_ppe_v6_get_stats()
 *	Get PPE IPv6 stats
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_v6_get_stats(struct netfn_flowmgr_flow_conn_stats *stats)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct ppe_drv_v6_flow_conn_stats ppe_stats;
	struct netfn_flowmgr_debug_stats *dbg_stats;
	netfn_tuple_type_t tuple_type;
	ppe_drv_ret_t ppe_status;

	dbg_stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_PPE];

	tuple_type = stats->tuple.tuple_type;

	/*
	 * Check tuple type
	 */
	if (!(tuple_type == NETFN_TUPLE_3TUPLE) && (!(tuple_type == NETFN_TUPLE_5TUPLE))) {
		netfn_flowmgr_warn("Unsupported tupple type in PPE acceleration mode, tuple_type = %d\n", tuple_type);
		netfn_flowmgr_stats_inc(&dbg_stats->validate_unsupported_tuple_type);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_UNSUPPORTED_TUPLE_TYPE, 0);
	}

	ppe_stats.tuple.protocol = stats->tuple.tuples.tuple_3.protocol;
	memcpy(ppe_stats.tuple.flow_ip, &stats->tuple.tuples.tuple_3.src_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
	memcpy(ppe_stats.tuple.return_ip, &stats->tuple.tuples.tuple_3.dest_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
        if (tuple_type == NETFN_TUPLE_5TUPLE) {
                ppe_stats.tuple.protocol = stats->tuple.tuples.tuple_5.protocol;
		memcpy(ppe_stats.tuple.flow_ip, &stats->tuple.tuples.tuple_5.src_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
		memcpy(ppe_stats.tuple.return_ip, &stats->tuple.tuples.tuple_5.dest_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
                ppe_stats.tuple.flow_ident = (uint32_t)ntohs(stats->tuple.tuples.tuple_5.l4_src_ident);
                ppe_stats.tuple.return_ident = (uint32_t)ntohs(stats->tuple.tuples.tuple_5.l4_dest_ident);
        }

	/*
	 * Change endianess
	 */
	netfn_flowmgr_ppe_endianess_be_to_le(ppe_stats.tuple.flow_ip, NETFN_FLOWMGR_TUPLE_IP_VERSION_V6);
	netfn_flowmgr_ppe_endianess_be_to_le(ppe_stats.tuple.return_ip, NETFN_FLOWMGR_TUPLE_IP_VERSION_V6);

	ppe_status = ppe_drv_v6_get_conn_stats(&ppe_stats);
	if (ppe_status != PPE_DRV_RET_SUCCESS) {
		netfn_flowmgr_warn("PPE v6 get ppe_stats failed, ppe_status = %d\n", ppe_status);
		netfn_flowmgr_warn("PPE v6 get ppe_stats failed for below tuples:\n"
					"protocol = %u\n"
					"flow_ip = %pI6\n"
					"return_ip = %pI6\n"
					"flow_ident = %u\n"
					"return ident = %u\n",
					ppe_stats.tuple.protocol,
					&ppe_stats.tuple.flow_ip,
					&ppe_stats.tuple.return_ip,
					ppe_stats.tuple.flow_ident,
					ppe_stats.tuple.return_ident);
		netfn_flowmgr_stats_inc(&dbg_stats->v6_get_stats_failed);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_GET_PPE_STATS_FAILED, ppe_status);
	}

	/*
	 * Copy ppe stats to the conn stats structure to update the user stats.
	 */
	stats->conn_stats.ip_version = ETH_P_IPV6;
	stats->conn_stats.protocol = ppe_stats.conn_sync.protocol;

	/*
	 * original direction stats
	 */
	memcpy(stats->conn_stats.org_src_ip, ppe_stats.conn_sync.flow_ip, sizeof(uint32_t) * 4);
	memcpy(stats->conn_stats.org_dest_ip, ppe_stats.conn_sync.return_ip, sizeof(uint32_t) * 4);
	stats->conn_stats.org_src_ident = ppe_stats.conn_sync.flow_ident;
	stats->conn_stats.org_dest_ident = ppe_stats.conn_sync.return_ident;
	stats->conn_stats.org_tx_pkt_count = ppe_stats.conn_sync.flow_tx_packet_count;
	stats->conn_stats.org_rx_pkt_count = ppe_stats.conn_sync.flow_rx_packet_count;
	stats->conn_stats.org_tx_byte_count = ppe_stats.conn_sync.flow_tx_byte_count;
	stats->conn_stats.org_rx_byte_count = ppe_stats.conn_sync.flow_rx_byte_count;

	/*
	 * reply direction stats
	 */
	memcpy(stats->conn_stats.reply_src_ip, ppe_stats.conn_sync.flow_ip, sizeof(uint32_t) * 4);
	memcpy(stats->conn_stats.reply_dest_ip, ppe_stats.conn_sync.return_ip, sizeof(uint32_t) * 4);
	stats->conn_stats.reply_src_ident = ppe_stats.conn_sync.return_ident;
	stats->conn_stats.reply_dest_ident = ppe_stats.conn_sync.flow_ident;
	stats->conn_stats.reply_tx_pkt_count = ppe_stats.conn_sync.return_tx_packet_count;
	stats->conn_stats.reply_rx_pkt_count = ppe_stats.conn_sync.return_rx_packet_count;
	stats->conn_stats.reply_tx_byte_count = ppe_stats.conn_sync.return_tx_byte_count;
	stats->conn_stats.reply_rx_byte_count = ppe_stats.conn_sync.return_rx_byte_count;

	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_SUCCESS, ppe_status);
}

/*
 * netfn_flowmgr_ppe_v4_get_stats()
 *	Get PPE IPv4 stats
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_v4_get_stats(struct netfn_flowmgr_flow_conn_stats *stats)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct ppe_drv_v4_flow_conn_stats ppe_stats;
	struct netfn_flowmgr_debug_stats *dbg_stats;
	netfn_tuple_type_t tuple_type;
	ppe_drv_ret_t ppe_status;

	dbg_stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_PPE];

	tuple_type = stats->tuple.tuple_type;

	/*
	 * Check tuple type
	 */
	if (!(tuple_type == NETFN_TUPLE_3TUPLE) && (!(tuple_type == NETFN_TUPLE_5TUPLE))) {
		netfn_flowmgr_warn("Unsupported tupple type in PPE acceleration mode, tuple_type = %d\n", tuple_type);
		netfn_flowmgr_stats_inc(&dbg_stats->validate_unsupported_tuple_type);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_UNSUPPORTED_TUPLE_TYPE, 0);
	}

	ppe_stats.tuple.protocol = stats->tuple.tuples.tuple_3.protocol;
	ppe_stats.tuple.flow_ip = (uint32_t)ntohl(stats->tuple.tuples.tuple_3.src_ip.ip4.s_addr);
	ppe_stats.tuple.return_ip = (uint32_t)ntohl(stats->tuple.tuples.tuple_3.dest_ip.ip4.s_addr);
	if (tuple_type == NETFN_TUPLE_5TUPLE) {
                ppe_stats.tuple.protocol = stats->tuple.tuples.tuple_5.protocol;
                ppe_stats.tuple.flow_ip = (uint32_t)ntohl(stats->tuple.tuples.tuple_5.src_ip.ip4.s_addr);
                ppe_stats.tuple.return_ip = (uint32_t)ntohl(stats->tuple.tuples.tuple_5.dest_ip.ip4.s_addr);
                ppe_stats.tuple.flow_ident = (uint32_t)ntohs(stats->tuple.tuples.tuple_5.l4_src_ident);
                ppe_stats.tuple.return_ident = (uint32_t)ntohs(stats->tuple.tuples.tuple_5.l4_dest_ident);
        }

	ppe_status = ppe_drv_v4_get_conn_stats(&ppe_stats);
	if (ppe_status != PPE_DRV_RET_SUCCESS) {
		netfn_flowmgr_warn("PPE v4 get ppe_stats failed, ppe_status = %d\n", ppe_status);
		netfn_flowmgr_warn("PPE v4 get ppe_stats failed for below tuples:\n"
					"protocol = %u\n"
					"flow_ip = %pI4\n"
					"return_ip = %pI4\n"
					"flow_ident = %u\n"
					"return ident = %u\n",
					ppe_stats.tuple.protocol,
					&ppe_stats.tuple.flow_ip,
					&ppe_stats.tuple.return_ip,
					ppe_stats.tuple.flow_ident,
					ppe_stats.tuple.return_ident);
		netfn_flowmgr_stats_inc(&dbg_stats->v4_get_stats_failed);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_GET_PPE_STATS_FAILED, ppe_status);
	}

	/*
	 * Copy ppe stats to the conn stats structure to update the user stats.
	 */
	stats->conn_stats.ip_version = ETH_P_IP;
	stats->conn_stats.protocol = ppe_stats.conn_sync.protocol;

	/*
	 * original direction stats
	 */
	stats->conn_stats.org_src_ip[0] = ppe_stats.conn_sync.flow_ip;
	stats->conn_stats.org_dest_ip[0] = ppe_stats.conn_sync.return_ip;
	stats->conn_stats.org_src_ident = ppe_stats.conn_sync.flow_ident;
	stats->conn_stats.org_dest_ident = ppe_stats.conn_sync.return_ident;
	stats->conn_stats.org_tx_pkt_count = ppe_stats.conn_sync.flow_tx_packet_count;
	stats->conn_stats.org_rx_pkt_count = ppe_stats.conn_sync.flow_rx_packet_count;
	stats->conn_stats.org_tx_byte_count = ppe_stats.conn_sync.flow_tx_byte_count;
	stats->conn_stats.org_rx_byte_count = ppe_stats.conn_sync.flow_rx_byte_count;

	/*
	 * reply direction stats
	 */
	stats->conn_stats.reply_src_ip[0] = ppe_stats.conn_sync.return_ip;
	stats->conn_stats.reply_dest_ip[0] = ppe_stats.conn_sync.flow_ip;
	stats->conn_stats.reply_src_ident = ppe_stats.conn_sync.return_ident;
	stats->conn_stats.reply_dest_ident = ppe_stats.conn_sync.flow_ident;
	stats->conn_stats.reply_tx_pkt_count = ppe_stats.conn_sync.return_tx_packet_count;
	stats->conn_stats.reply_rx_pkt_count = ppe_stats.conn_sync.return_rx_packet_count;
	stats->conn_stats.reply_tx_byte_count = ppe_stats.conn_sync.return_tx_byte_count;
	stats->conn_stats.reply_rx_byte_count = ppe_stats.conn_sync.return_rx_byte_count;

	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_SUCCESS, ppe_status);
}

/*
 * netfn_flowmgr_ppe_get_stats()
 *	Get PPE stats
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_get_stats(struct netfn_flowmgr_flow_conn_stats *stats)
{
	if (stats->tuple.ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		return netfn_flowmgr_ppe_v4_get_stats(stats);
	}
	else if (stats->tuple.ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V6) {
		return netfn_flowmgr_ppe_v6_get_stats(stats);
	}

	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INCORRECT_IP_VERSION, 0);
}

/*
 * netfn_flowmgr_ppe_dscp_pri_add()
 *	Add dscp based prioritization
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_dscp_pri_add(struct netfn_flowmgr_dscp_priority *dscpinfo)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_debug_stats *stats;
	struct ppe_acl_rule acl_rule = {0};
	struct net_device *netdev = NULL;
	int16_t queue_num = 0;
	uint8_t core_id = 0;
	uint8_t priority_queue = 0;
	uint8_t ppe_queue_id = 0;
	ppe_acl_ret_t ppe_acl_status;
	netfn_flowmgr_ret_t status = 0;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_PPE];

	netfn_flowmgr_stats_inc(&stats->dscp_priority_add_req);

	/*
	 * Check if the core id is valid or not
	 */
	core_id = dscpinfo->core_id;
	if (core_id >= NR_CPUS) {
		netfn_flowmgr_warn("Invalid core-id:%u\n", core_id);
		netfn_flowmgr_stats_inc(&stats->dscp_priority_add_req_fail_inval_core_id);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_FAIL_INVALID_PARAM, 0);
	}

	/*
	 * Check if the priority queue is valid or not
	 */
	priority_queue = dscpinfo->priority;
	if (priority_queue >= NETFN_MAX_PRI_PER_CORE) {
		netfn_flowmgr_warn("Invalid priority_queue:%u\n", priority_queue);
		netfn_flowmgr_stats_inc(&stats->dscp_priority_add_req_fail_inval_pri_queue);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_FAIL_INVALID_PARAM, 0);
	}

	/*
	 * Check if the src netdevice is valid or not
	 */
	netdev = dscpinfo->src_dev;
	if (!netdev) {
		netfn_flowmgr_warn("No WAN net device\n");
		netfn_flowmgr_stats_inc(&stats->dscp_priority_add_req_fail_no_wan_dev);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_INVALID_DSCP_RULE, 0);
	}

	/*
	 * Create acl rule
	 */
	acl_rule.valid_flags |= PPE_ACL_RULE_MATCH_TYPE_TOS_TC_VALID;

	/*
	 * Bind the ACL rule to the WAN port
	 */
	acl_rule.stype = PPE_ACL_RULE_SRC_TYPE_DEV;

	strlcpy(acl_rule.src.dev_name, netdev->name, IFNAMSIZ);

	/*
	 * tos_tc_val in dscpinfo takes full 8-bit value:
	 * IPv4 header: 8-bit ToS [DSCP(6) & ECN(2)]
	 * IPv6 header: 8-bit TC [DSCP(6) & ECN(2)]
	 */
	acl_rule.rules[PPE_ACL_RULE_MATCH_TYPE_TOS_TC].rule.tos_tc.l3_tos_tc = dscpinfo->tos_tc_val;

	/*
	 * Get base queue for a given core
	 */
	queue_num = ppe_drv_queue_from_core(dscpinfo->core_id);

	/*
	 * Calculate the queue based on given priority queue
	 */
	ppe_queue_id = queue_num + dscpinfo->priority;

	netfn_flowmgr_trace("Base queue = %u, ppe_queue_id = %u", queue_num, ppe_queue_id);

	/*
	 * Action
	 */
	acl_rule.cmn.cmn_flags |=  PPE_ACL_RULE_CMN_FLAG_NO_RULEID;
	acl_rule.action.flags = PPE_ACL_RULE_ACTION_FLAG_QID_EN;
	acl_rule.action.qid = ppe_queue_id;

	/*
	 * Call ppe acl rule create API
	 */
	ppe_acl_status = ppe_acl_rule_create(&acl_rule);
	if (ppe_acl_status != PPE_ACL_RET_SUCCESS) {
		netfn_flowmgr_warn("Failed to create ACL rule for dscp prioritization, error:%d", ppe_acl_status);
		netfn_flowmgr_stats_inc(&stats->dscp_priority_add_req_rule_create_fail);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_DSCP_RULE_ADD_FAILED, ppe_acl_status);
	}

	dscpinfo->rule_id = acl_rule.rule_id;
	netfn_flowmgr_trace("DSCP priority added, rule_id = %u", dscpinfo->rule_id);

	return status;
}

/*
 * netfn_flowmgr_ppe_dscp_pri_del()
 *	Delete dscp based prioritization
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_dscp_pri_del(int16_t rule_id)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_debug_stats *stats;
	netfn_flowmgr_ret_t status = 0;
	ppe_acl_ret_t ppe_acl_status;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_PPE];

	netfn_flowmgr_stats_inc(&stats->dscp_priority_del_req);
	/*
	 * Call ppe acl rule destroy API with rule id obtained during create
	 */
	ppe_acl_status = ppe_acl_rule_destroy(rule_id);
	if (ppe_acl_status != PPE_ACL_RET_SUCCESS) {
                netfn_flowmgr_warn("Failed to destroy ACL rule for ruleid:%d, error:%d", rule_id, ppe_acl_status);
		netfn_flowmgr_stats_inc(&stats->dscp_priority_del_req_rule_destroy_fail);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_DSCP_RULE_DEL_FAILED, ppe_acl_status);
        }

	netfn_flowmgr_trace("ACL rule for rule_id:%d destroyed successfully\n", rule_id);
	return status;
}

/*
 * netfn_flowmgr_ppe_validate_vlan_info()
 *	Validate vlan info in original and reply direction
 */
netfn_flowmgr_ret_t netfn_flowmgr_ppe_validate_vlan_info(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply)
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

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_PPE];
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
		status = NETFN_FLOWMGR_RET_PPE_INNER_VLAN_MISMATCH;
	}

	/*
	 * Outer VLAN(secondary) rule match
	 */
	if ((org_outer_ingress_vlan_tag != reply_outer_egress_vlan_tag) || (org_outer_egress_vlan_tag != reply_outer_ingress_vlan_tag)) {
		netfn_flowmgr_warn("Outer VLAN config mismatch in org and reply direction\n");
		netfn_flowmgr_stats_inc(&stats->validate_vlan_outer_mismatch);
		status = NETFN_FLOWMGR_RET_PPE_OUTER_VLAN_MISMATCH;
	}
	return status;
}

/*
 * netfn_flowmgr_ppe_deinit()
 *	De-initialization of PPE callbacks
 */
void netfn_flowmgr_ppe_deinit(void)
{
	/*
	 * Unregister the callbacks with netfn core.
	 */
	netfn_flowmgr_ae_ops_unregister(NETFN_FLOWMGR_AE_TYPE_PPE);

	/*
	 * De-initialize ppe ipv4
	 */
	netfn_flowmgr_ppe_ipv4_deinit();

	/*
	 * De-initialize ppe ipv6
	 */
	netfn_flowmgr_ppe_ipv6_deinit();
}

/*
 * netfn_flowmgr_ppe_init()
 *	PPE initialization.
 */
bool netfn_flowmgr_ppe_init(void)
{
	bool status;
	struct netfn_flowmgr_ae_ops ppe_ops = {0};
	netfn_flowmgr_ae_type_t ae_type = NETFN_FLOWMGR_AE_TYPE_PPE;

	/*
	 * Assign PPE rule function pointers.
	 */
	ppe_ops.create_rule = netfn_flowmgr_ppe_create_rule;
	ppe_ops.destroy_rule = netfn_flowmgr_ppe_destroy_rule;
	ppe_ops.get_stats = netfn_flowmgr_ppe_get_stats;
	ppe_ops.add_dscp_priority = netfn_flowmgr_ppe_dscp_pri_add;
	ppe_ops.del_dscp_priority = netfn_flowmgr_ppe_dscp_pri_del;

	/*
	 * Register the callbacks with netfn core.
	 */
	netfn_flowmgr_ae_ops_register(ae_type, &ppe_ops);

	status = netfn_flowmgr_ppe_ipv4_init();
	if (!status) {
		netfn_flowmgr_warn("Netfn flowmgr PPE IPv4 initialization failed\n");
		return false;
	}

	status = netfn_flowmgr_ppe_ipv6_init();
	if (!status) {
		netfn_flowmgr_warn("Netfn flowmgr PPE IPv6 initialization failed\n");
		return false;
	}

	return true;
}
