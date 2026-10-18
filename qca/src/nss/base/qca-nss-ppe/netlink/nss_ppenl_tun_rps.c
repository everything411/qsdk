/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_tun_rps.c
 *	NSS Netlink Tunnel RPS Handler
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/version.h>
#include <linux/if.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <linux/rcupdate.h>
#include <linux/etherdevice.h>
#include <linux/if_addr.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/if_vlan.h>
#include <linux/completion.h>
#include <linux/semaphore.h>
#include <linux/in.h>

#include <net/arp.h>
#include <net/genetlink.h>
#include <net/neighbour.h>
#include <net/net_namespace.h>
#include <net/route.h>
#include <net/sock.h>

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_tun_rps_if.h>
#include <ppe_tun_rps.h>
#include "nss_ppenl.h"
#include "nss_ppenl_tun_rps.h"

/*
 * Constants for validation
 */
#define NSS_PPENL_TUN_RPS_MAX_HDR_LEN 126
#define NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET 126

/*
 * prototypes
 */
static int nss_ppenl_tun_rps_validate_rule(struct nss_ppe_nl_tun_rps *rule, bool is_create);
static int nss_ppenl_tun_rps_ops_create_rule(struct sk_buff *skb, struct genl_info *info);
static int nss_ppenl_tun_rps_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info);

/*
 * nss_ppenl_tun_rps_map_to_ipv4_create()
 *	Map netlink rule structure to PPE IPv4 create message structure
 */
static int nss_ppenl_tun_rps_map_to_ipv4_create(struct nss_ppe_nl_tun_rps *nl_rule,
						 struct ppe_tun_rps_ipv4_rule_create_msg *ppe_msg)
{
	if (!nl_rule || !ppe_msg) {
		nss_ppenl_warn("nl_rule/ppe_msg structure is NULL\n");
		return -EINVAL;
	}

	memset(ppe_msg, 0, sizeof(*ppe_msg));

	/*
	 * Map IP addresses (convert to network byte order)
	 */
	ppe_msg->flow_ip = htonl(nl_rule->ipv4_sip);
	ppe_msg->return_ip = htonl(nl_rule->ipv4_dip);

	/*
	 * Map ports (convert to network byte order)
	 */
	ppe_msg->flow_ident = nl_rule->sport;
	ppe_msg->return_ident = nl_rule->dport;

	/*
	 * Direct mappings
	 */
	ppe_msg->protocol = nl_rule->protocol;
	ppe_msg->header_len = nl_rule->hdr_len;
	ppe_msg->header_len_type = (enum ppe_tun_rps_hdr_len_type)nl_rule->hdr_len_type;
	ppe_msg->inner_pkt_type = (enum ppe_tun_rps_inner_pkt_type)nl_rule->inner_pkt_type;

	/*
	 * Map user data structure
	 */
	ppe_msg->usr_data.udf[0].enable = nl_rule->usr_data.udf[0].data_en;
	ppe_msg->usr_data.udf[0].offset = nl_rule->usr_data.udf[0].data_offset;
	ppe_msg->usr_data.udf[0].val = nl_rule->usr_data.udf[0].data_val;
	ppe_msg->usr_data.udf[0].mask = nl_rule->usr_data.udf[0].data_mask;

	ppe_msg->usr_data.udf[1].enable = nl_rule->usr_data.udf[1].data_en;
	ppe_msg->usr_data.udf[1].offset = nl_rule->usr_data.udf[1].data_offset;
	ppe_msg->usr_data.udf[1].val = nl_rule->usr_data.udf[1].data_val;
	ppe_msg->usr_data.udf[1].mask = nl_rule->usr_data.udf[1].data_mask;

	ppe_msg->usr_data.udf[2].enable = nl_rule->usr_data.udf[2].data_en;
	ppe_msg->usr_data.udf[2].offset = nl_rule->usr_data.udf[2].data_offset;
	ppe_msg->usr_data.udf[2].val = nl_rule->usr_data.udf[2].data_val;
	ppe_msg->usr_data.udf[2].mask = nl_rule->usr_data.udf[2].data_mask;

	/*
	 * Map WAN interface name
	 */
	strlcpy(ppe_msg->wan_if, nl_rule->wan_if, IFNAMSIZ);

	/*
	 * Map inner IP protocol fields
	 */
	ppe_msg->inner_ip_proto.ip_proto_offset = nl_rule->inner_ip_proto.ip_proto_offset;
	ppe_msg->inner_ip_proto.ip_proto_ipv4_value = nl_rule->inner_ip_proto.ip_proto_ipv4_value;
	ppe_msg->inner_ip_proto.ip_proto_ipv4_mask = nl_rule->inner_ip_proto.ip_proto_ipv4_mask;
	ppe_msg->inner_ip_proto.ip_proto_ipv6_value = nl_rule->inner_ip_proto.ip_proto_ipv6_value;
	ppe_msg->inner_ip_proto.ip_proto_ipv6_mask = nl_rule->inner_ip_proto.ip_proto_ipv6_mask;

	/*
	 * Map PPPoE configuration
	 */
	ppe_msg->pppoe_en = nl_rule->pppoe_en;
	ppe_msg->pppoe_session_id = nl_rule->pppoe_session_id;
	memcpy(ppe_msg->pppoe_server_mac, nl_rule->pppoe_server_mac, ETH_ALEN);

	/*
	 * Map header matching fields based on header type
	 */
	switch (nl_rule->hdr_len_type) {
	case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
		ppe_msg->header_match.l2_match.eth_protocol = nl_rule->header_match.l2_match.eth_protocol;
		ppe_msg->header_match.l2_match.eth_protocol_mask = nl_rule->header_match.l2_match.eth_protocol_mask;
		break;
	case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
		ppe_msg->header_match.l3_match.ip_protocol = nl_rule->header_match.l3_match.ip_protocol;
		ppe_msg->header_match.l3_match.ip_protocol_mask = nl_rule->header_match.l3_match.ip_protocol_mask;
		break;
	case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
		ppe_msg->header_match.l4_match.udp_sport = nl_rule->header_match.l4_match.udp_sport;
		ppe_msg->header_match.l4_match.udp_dport = nl_rule->header_match.l4_match.udp_dport;
		break;
	default:
		/*
		 * Initialize to zero for unknown header types
		 */
		nss_ppenl_warn("Unknown header length type %d setting to default %d\n", nl_rule->hdr_len_type, 0);
		memset(&ppe_msg->header_match, 0, sizeof(ppe_msg->header_match));
		break;
	}

	return 0;
}

/*
 * nss_ppenl_tun_rps_map_to_ipv6_create()
 *	Map netlink rule structure to PPE IPv6 create message structure
 */
static int nss_ppenl_tun_rps_map_to_ipv6_create(struct nss_ppe_nl_tun_rps *nl_rule,
						 struct ppe_tun_rps_ipv6_rule_create_msg *ppe_msg)
{
	int i;

	if (!nl_rule || !ppe_msg) {
		nss_ppenl_warn("nl_rule/ppe_msg structure is NULL\n");
		return -EINVAL;
	}

	memset(ppe_msg, 0, sizeof(*ppe_msg));

	/*
	 * Map IPv6 addresses (convert to network byte order)
	 */
	for (i = 0; i < 4; i++) {
		ppe_msg->flow_ip[i] = htonl(nl_rule->ipv6_sip[i]);
		ppe_msg->return_ip[i] = htonl(nl_rule->ipv6_dip[i]);
	}

	/*
	 * Map ports (convert to network byte order)
	 */
	ppe_msg->flow_ident = nl_rule->sport;
	ppe_msg->return_ident = nl_rule->dport;

	/*
	 * Direct mappings
	 */
	ppe_msg->protocol = nl_rule->protocol;
	ppe_msg->header_len = nl_rule->hdr_len;
	ppe_msg->header_len_type = (enum ppe_tun_rps_hdr_len_type)nl_rule->hdr_len_type;
	ppe_msg->inner_pkt_type = (enum ppe_tun_rps_inner_pkt_type)nl_rule->inner_pkt_type;

	/*
	 * Map user data structure
	 */
	ppe_msg->usr_data.udf[0].enable = nl_rule->usr_data.udf[0].data_en;
	ppe_msg->usr_data.udf[0].offset = nl_rule->usr_data.udf[0].data_offset;
	ppe_msg->usr_data.udf[0].val = nl_rule->usr_data.udf[0].data_val;
	ppe_msg->usr_data.udf[0].mask = nl_rule->usr_data.udf[0].data_mask;

	ppe_msg->usr_data.udf[1].enable = nl_rule->usr_data.udf[1].data_en;
	ppe_msg->usr_data.udf[1].offset = nl_rule->usr_data.udf[1].data_offset;
	ppe_msg->usr_data.udf[1].val = nl_rule->usr_data.udf[1].data_val;
	ppe_msg->usr_data.udf[1].mask = nl_rule->usr_data.udf[1].data_mask;

	ppe_msg->usr_data.udf[2].enable = nl_rule->usr_data.udf[2].data_en;
	ppe_msg->usr_data.udf[2].offset = nl_rule->usr_data.udf[2].data_offset;
	ppe_msg->usr_data.udf[2].val = nl_rule->usr_data.udf[2].data_val;
	ppe_msg->usr_data.udf[2].mask = nl_rule->usr_data.udf[2].data_mask;

	/*
	 * Map WAN interface name
	 */
	strlcpy(ppe_msg->wan_if, nl_rule->wan_if, IFNAMSIZ);

	/*
	 * Map inner IP protocol fields
	 */
	ppe_msg->inner_ip_proto.ip_proto_offset = nl_rule->inner_ip_proto.ip_proto_offset;
	ppe_msg->inner_ip_proto.ip_proto_ipv4_value = nl_rule->inner_ip_proto.ip_proto_ipv4_value;
	ppe_msg->inner_ip_proto.ip_proto_ipv4_mask = nl_rule->inner_ip_proto.ip_proto_ipv4_mask;
	ppe_msg->inner_ip_proto.ip_proto_ipv6_value = nl_rule->inner_ip_proto.ip_proto_ipv6_value;
	ppe_msg->inner_ip_proto.ip_proto_ipv6_mask = nl_rule->inner_ip_proto.ip_proto_ipv6_mask;

	/*
	 * Map PPPoE configuration
	 */
	ppe_msg->pppoe_en = nl_rule->pppoe_en;
	ppe_msg->pppoe_session_id = nl_rule->pppoe_session_id;
	memcpy(ppe_msg->pppoe_server_mac, nl_rule->pppoe_server_mac, ETH_ALEN);

	/*
	 * Map header matching fields based on header type
	 */
	switch (nl_rule->hdr_len_type) {
	case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
		ppe_msg->header_match.l2_match.eth_protocol = nl_rule->header_match.l2_match.eth_protocol;
		ppe_msg->header_match.l2_match.eth_protocol_mask = nl_rule->header_match.l2_match.eth_protocol_mask;
		break;
	case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
		ppe_msg->header_match.l3_match.ip_protocol = nl_rule->header_match.l3_match.ip_protocol;
		ppe_msg->header_match.l3_match.ip_protocol_mask = nl_rule->header_match.l3_match.ip_protocol_mask;
		break;
	case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
		ppe_msg->header_match.l4_match.udp_sport = nl_rule->header_match.l4_match.udp_sport;
		ppe_msg->header_match.l4_match.udp_dport = nl_rule->header_match.l4_match.udp_dport;
		break;
	default:
		/*
		 * Initialize to zero for unknown header types
		 */
		nss_ppenl_warn("Unknown header length type %d setting to default %d\n", nl_rule->hdr_len_type, 0);
		memset(&ppe_msg->header_match, 0, sizeof(ppe_msg->header_match));
		break;
	}

	return 0;
}

/*
 * nss_ppenl_tun_rps_map_to_ipv4_destroy()
 *	Map netlink rule structure to PPE IPv4 destroy message structure
 */
static int nss_ppenl_tun_rps_map_to_ipv4_destroy(struct nss_ppe_nl_tun_rps *nl_rule,
						  struct ppe_tun_rps_ipv4_rule_destroy_msg *ppe_msg)
{
	if (!nl_rule || !ppe_msg) {
		nss_ppenl_warn("nl_rule/ppe_msg structure is NULL\n");
		return -EINVAL;
	}

	memset(ppe_msg, 0, sizeof(*ppe_msg));

	/*
	 * Map IP addresses (convert to network byte order)
	 */
	ppe_msg->flow_ip = htonl(nl_rule->ipv4_sip);
	ppe_msg->return_ip = htonl(nl_rule->ipv4_dip);

	/*
	 * Map ports (convert to network byte order)
	 */
	ppe_msg->flow_ident = nl_rule->sport;
	ppe_msg->return_ident = nl_rule->dport;

	/*
	 * Direct mappings
	 */
	ppe_msg->protocol = nl_rule->protocol;

	/*
	 * Map user data structure
	 */
	ppe_msg->usr_data.udf[0].enable = nl_rule->usr_data.udf[0].data_en;
	ppe_msg->usr_data.udf[0].offset = nl_rule->usr_data.udf[0].data_offset;
	ppe_msg->usr_data.udf[0].val = nl_rule->usr_data.udf[0].data_val;
	ppe_msg->usr_data.udf[0].mask = nl_rule->usr_data.udf[0].data_mask;

	ppe_msg->usr_data.udf[1].enable = nl_rule->usr_data.udf[1].data_en;
	ppe_msg->usr_data.udf[1].offset = nl_rule->usr_data.udf[1].data_offset;
	ppe_msg->usr_data.udf[1].val = nl_rule->usr_data.udf[1].data_val;
	ppe_msg->usr_data.udf[1].mask = nl_rule->usr_data.udf[1].data_mask;

	ppe_msg->usr_data.udf[2].enable = nl_rule->usr_data.udf[2].data_en;
	ppe_msg->usr_data.udf[2].offset = nl_rule->usr_data.udf[2].data_offset;
	ppe_msg->usr_data.udf[2].val = nl_rule->usr_data.udf[2].data_val;
	ppe_msg->usr_data.udf[2].mask = nl_rule->usr_data.udf[2].data_mask;

	return 0;
}

/*
 * nss_ppenl_tun_rps_map_to_ipv6_destroy()
 *	Map netlink rule structure to PPE IPv6 destroy message structure
 */
static int nss_ppenl_tun_rps_map_to_ipv6_destroy(struct nss_ppe_nl_tun_rps *nl_rule,
						  struct ppe_tun_rps_ipv6_rule_destroy_msg *ppe_msg)
{
	int i;

	if (!nl_rule || !ppe_msg) {
		nss_ppenl_warn("nl_rule/ppe_msg structure is NULL\n");
		return -EINVAL;
	}

	memset(ppe_msg, 0, sizeof(*ppe_msg));

	/*
	 * Map IPv6 addresses (convert to network byte order)
	 */
	for (i = 0; i < 4; i++) {
		ppe_msg->flow_ip[i] = htonl(nl_rule->ipv6_sip[i]);
		ppe_msg->return_ip[i] = htonl(nl_rule->ipv6_dip[i]);
	}

	/*
	 * Map ports (convert to network byte order)
	 */
	ppe_msg->flow_ident = nl_rule->sport;
	ppe_msg->return_ident = nl_rule->dport;

	/*
	 * Direct mappings
	 */
	ppe_msg->protocol = nl_rule->protocol;

	/*
	 * Map user data structure
	 */
	ppe_msg->usr_data.udf[0].enable = nl_rule->usr_data.udf[0].data_en;
	ppe_msg->usr_data.udf[0].offset = nl_rule->usr_data.udf[0].data_offset;
	ppe_msg->usr_data.udf[0].val = nl_rule->usr_data.udf[0].data_val;
	ppe_msg->usr_data.udf[0].mask = nl_rule->usr_data.udf[0].data_mask;

	ppe_msg->usr_data.udf[1].enable = nl_rule->usr_data.udf[1].data_en;
	ppe_msg->usr_data.udf[1].offset = nl_rule->usr_data.udf[1].data_offset;
	ppe_msg->usr_data.udf[1].val = nl_rule->usr_data.udf[1].data_val;
	ppe_msg->usr_data.udf[1].mask = nl_rule->usr_data.udf[1].data_mask;

	ppe_msg->usr_data.udf[2].enable = nl_rule->usr_data.udf[2].data_en;
	ppe_msg->usr_data.udf[2].offset = nl_rule->usr_data.udf[2].data_offset;
	ppe_msg->usr_data.udf[2].val = nl_rule->usr_data.udf[2].data_val;
	ppe_msg->usr_data.udf[2].mask = nl_rule->usr_data.udf[2].data_mask;

	return 0;
}

/*
 * nss_ppenl_tun_rps_validate_rule()
 *	Comprehensive input validation for tunnel RPS rules with support for partial 5-tuple matching
 */
static int nss_ppenl_tun_rps_validate_rule(struct nss_ppe_nl_tun_rps *rule, bool is_create)
{
	bool has_match_criteria = false;
	int match_count = 0;

	if (!rule) {
		nss_ppenl_warn("NULL rule parameter\n");
		return -EINVAL;
	}

	nss_ppenl_info("Validating tunnel RPS rule for partial 5-tuple matching:\n");
	nss_ppenl_info("IP Version: %s\n", rule->is_ipv6 ? "IPv6" : "IPv4");

	if (is_create) {
		/*
		 * Validate header length type
		 */
		if (rule->hdr_len_type > PPE_TUN_RPS_HDR_LEN_TYPE_UDP) {
			nss_ppenl_warn("Invalid header length type %d (max: PPE_TUN_RPS_HDR_LEN_TYPE_UDP)\n", rule->hdr_len_type);
			return -EINVAL;
		}

		/*
		 * Validate inner packet type
		 */
		if (rule->inner_pkt_type > PPE_TUN_RPS_INNER_PKT_TYPE_IP) {
			nss_ppenl_warn("Invalid inner packet type %d (max: PPE_TUN_RPS_INNER_PKT_TYPE_IP)\n", rule->inner_pkt_type);
			return -EINVAL;
		}

		/*
		 * Validate header length
		 */
		if (rule->hdr_len > NSS_PPENL_TUN_RPS_MAX_HDR_LEN) {
			nss_ppenl_warn("Invalid header length %d (max: %d)\n",
					rule->hdr_len, NSS_PPENL_TUN_RPS_MAX_HDR_LEN);
			return -EINVAL;
		}

		/*
		 * Validate that header length is even (multiple of 2) as required by hardware/SSDK
		 */
		if (rule->hdr_len % 2 != 0) {
			nss_ppenl_warn("Header length %d must be an even number (multiple of 2)\n", rule->hdr_len);
			return -EINVAL;
		}
	}

	/*
	 * Check for at least one matching criterion - zero values are now treated as wildcards
	 */
	if (!rule->is_ipv6) {
		/*
		 * IPv4 case - zero addresses are allowed as wildcards
		 */
		nss_ppenl_info("SIP IPv4: %pI4\n", &rule->ipv4_sip);
		if (rule->ipv4_sip != 0) {
			has_match_criteria = true;
			match_count++;
			nss_ppenl_info("SIP: WILL MATCH (non-zero)\n");
		} else {
			nss_ppenl_info("SIP: WILDCARD (zero - will match any)\n");
		}

		nss_ppenl_info("DIP IPv4: %pI4\n", &rule->ipv4_dip);
		if (rule->ipv4_dip != 0) {
			has_match_criteria = true;
			match_count++;
			nss_ppenl_info("DIP: WILL MATCH (non-zero)\n");
		} else {
			nss_ppenl_info("DIP: WILDCARD (zero - will match any)\n");
		}
	} else {
		/*
		 * IPv6 case - only all-zero addresses (::) are treated as wildcards
		 */
		bool sip_all_zero = (rule->ipv6_sip[0] == 0 && rule->ipv6_sip[1] == 0 &&
				rule->ipv6_sip[2] == 0 && rule->ipv6_sip[3] == 0);
		bool dip_all_zero = (rule->ipv6_dip[0] == 0 && rule->ipv6_dip[1] == 0 &&
				rule->ipv6_dip[2] == 0 && rule->ipv6_dip[3] == 0);

		nss_ppenl_info("SIP IPv6: %pI6\n", rule->ipv6_sip);
		if (!sip_all_zero) {
			has_match_criteria = true;
			match_count++;
			nss_ppenl_info("SIP: WILL MATCH (non-zero)\n");
		} else {
			nss_ppenl_info("SIP: WILDCARD (all zeros - will match any)\n");
		}

		nss_ppenl_info("DIP IPv6: %pI6\n", rule->ipv6_dip);
		if (!dip_all_zero) {
			has_match_criteria = true;
			match_count++;
			nss_ppenl_info("DIP: WILL MATCH (non-zero)\n");
		} else {
			nss_ppenl_info("DIP: WILDCARD (all zeros - will match any)\n");
		}
	}

	/*
	 * Check ports and protocol - zero values are treated as wildcards
	 */
	nss_ppenl_info("Source Port: %d\n", rule->sport);
	if (rule->sport != 0) {
		has_match_criteria = true;
		match_count++;
		nss_ppenl_info("Source Port: WILL MATCH (non-zero)\n");
	} else {
		nss_ppenl_info("Source Port: WILDCARD (zero - will match any)\n");
	}

	nss_ppenl_info("Destination Port: %d\n", rule->dport);
	if (rule->dport != 0) {
		has_match_criteria = true;
		match_count++;
		nss_ppenl_info("Destination Port: WILL MATCH (non-zero)\n");
	} else {
		nss_ppenl_info("Destination Port: WILDCARD (zero - will match any)\n");
	}

	nss_ppenl_info("Protocol: %d\n", rule->protocol);
	if (rule->protocol != 0) {
		has_match_criteria = true;
		match_count++;
		nss_ppenl_info("Protocol: WILL MATCH (non-zero)\n");
	} else {
		nss_ppenl_info("Protocol: WILDCARD (zero - will match any)\n");
	}

	/*
	 * UDF fields also count as matching criteria
	 */
	if (rule->usr_data.udf[0].data_en) {
		has_match_criteria = true;
		match_count++;
		nss_ppenl_info("UDF1: ENABLED (will match)\n");
	} else {
		nss_ppenl_info("UDF1: DISABLED\n");
	}

	if (rule->usr_data.udf[1].data_en) {
		has_match_criteria = true;
		match_count++;
		nss_ppenl_info("UDF2: ENABLED (will match)\n");
	} else {
		nss_ppenl_info("UDF2: DISABLED\n");
	}

	if (rule->usr_data.udf[2].data_en) {
		has_match_criteria = true;
		match_count++;
		nss_ppenl_info("UDF3: ENABLED (will match)\n");
	} else {
		nss_ppenl_info("UDF3: DISABLED\n");
	}

	nss_ppenl_info("Total matching fields: %d\n", match_count);

	/*
	 * Require at least one matching criterion
	 */
	if (!has_match_criteria) {
		nss_ppenl_warn("VALIDATION FAILED: Rule must have at least one matching criterion (non-zero field or enabled UDF)\n");
		return -EINVAL;
	}

	nss_ppenl_info("VALIDATION PASSED: Rule has %d matching criteria for partial 5-tuple matching\n", match_count);

	/*
	 * Validate user data fields
	 */
	if (rule->usr_data.udf[0].data_en) {
		if (rule->usr_data.udf[0].data_offset > NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET) {
			nss_ppenl_warn("Invalid udf[0].data_offset %d (max: %d)\n",
					rule->usr_data.udf[0].data_offset, NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET);
			return -EINVAL;
		}

		if (rule->usr_data.udf[0].data_offset % 2 != 0) {
			nss_ppenl_warn("udf[0].data_offset %d must be an even number (multiple of 2)\n",
					rule->usr_data.udf[0].data_offset);
			return -EINVAL;
		}

		if (is_create && ((rule->usr_data.udf[0].data_offset + 2) > rule->hdr_len)) {
			nss_ppenl_warn("udf[0].data_offset %d + 2 exceeds hdr_len %d\n",
					rule->usr_data.udf[0].data_offset, rule->hdr_len);
			return -EINVAL;
		}

		if (is_create && (rule->hdr_len_type == PPE_TUN_RPS_HDR_LEN_TYPE_ETH) && (rule->usr_data.udf[0].data_offset < 14)) {
			nss_ppenl_warn("udf[0].data_offset %d is within outer Ethernet header (< 14)\n",
					rule->usr_data.udf[0].data_offset);
			return -EINVAL;
		}

		/*
		 * Validate that mask is not zero when field is enabled
		 */
		if (rule->usr_data.udf[0].data_mask == 0) {
			nss_ppenl_warn("udf[0].data_mask cannot be zero when enabled\n");
			return -EINVAL;
		}

		nss_ppenl_info("UDF1 validation passed: offset=%d, val=0x%x, mask=0x%x\n",
				rule->usr_data.udf[0].data_offset, rule->usr_data.udf[0].data_val,
				rule->usr_data.udf[0].data_mask);
	}

	if (rule->usr_data.udf[1].data_en) {
		if (rule->usr_data.udf[1].data_offset > NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET) {
			nss_ppenl_warn("Invalid udf[1].data_offset %d (max: %d)\n",
					rule->usr_data.udf[1].data_offset, NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET);
			return -EINVAL;
		}

		if (rule->usr_data.udf[1].data_offset % 2 != 0) {
			nss_ppenl_warn("udf[1].data_offset %d must be an even number (multiple of 2)\n",
					rule->usr_data.udf[1].data_offset);
			return -EINVAL;
		}

		if (is_create && ((rule->usr_data.udf[1].data_offset + 2) > rule->hdr_len)) {
			nss_ppenl_warn("udf[1].data_offset %d + 2 exceeds hdr_len %d\n",
					rule->usr_data.udf[1].data_offset, rule->hdr_len);
			return -EINVAL;
		}

		if (is_create && (rule->hdr_len_type == PPE_TUN_RPS_HDR_LEN_TYPE_ETH) && (rule->usr_data.udf[1].data_offset < 14)) {
			nss_ppenl_warn("udf[1].data_offset %d is within outer Ethernet header (< 14)\n",
					rule->usr_data.udf[1].data_offset);
			return -EINVAL;
		}

		if (rule->usr_data.udf[1].data_mask == 0) {
			nss_ppenl_warn("udf[1].data_mask cannot be zero when enabled\n");
			return -EINVAL;
		}

		nss_ppenl_info("UDF2 validation passed: offset=%d, val=0x%x, mask=0x%x\n",
				rule->usr_data.udf[1].data_offset, rule->usr_data.udf[1].data_val,
				rule->usr_data.udf[1].data_mask);
	}

	if (rule->usr_data.udf[2].data_en) {
		if (rule->usr_data.udf[2].data_offset > NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET) {
			nss_ppenl_warn("Invalid udf[2].data_offset %d (max: %d)\n",
					rule->usr_data.udf[2].data_offset, NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET);
			return -EINVAL;
		}

		if (rule->usr_data.udf[2].data_offset % 2 != 0) {
			nss_ppenl_warn("udf[2].data_offset %d must be an even number (multiple of 2)\n",
					rule->usr_data.udf[2].data_offset);
			return -EINVAL;
		}

		if (is_create && ((rule->usr_data.udf[2].data_offset + 2) > rule->hdr_len)) {
			nss_ppenl_warn("udf[2].data_offset %d + 2 exceeds hdr_len %d\n",
					rule->usr_data.udf[2].data_offset, rule->hdr_len);
			return -EINVAL;
		}

		if (is_create && (rule->hdr_len_type == PPE_TUN_RPS_HDR_LEN_TYPE_ETH) && (rule->usr_data.udf[2].data_offset < 14)) {
			nss_ppenl_warn("udf[2].data_offset %d is within outer Ethernet header (< 14)\n",
					rule->usr_data.udf[2].data_offset);
			return -EINVAL;
		}

		if (rule->usr_data.udf[2].data_mask == 0) {
			nss_ppenl_warn("udf[2].data_mask cannot be zero when enabled\n");
			return -EINVAL;
		}

		nss_ppenl_info("UDF3 validation passed: offset=%d, val=0x%x, mask=0x%x\n",
				rule->usr_data.udf[2].data_offset, rule->usr_data.udf[2].data_val,
				rule->usr_data.udf[2].data_mask);
	}

	/*
	 * Validate PPPoE configuration
	 */
	if (is_create && rule->pppoe_en) {
		/*
		 * If PPPoE is enabled, Session ID must be non-zero
		 */
		if (rule->pppoe_session_id == 0) {
			nss_ppenl_warn("PPPoE session ID cannot be zero when PPPoE is enabled\n");
			return -EINVAL;
		}

		/*
		 * If PPPoE is enabled, Server MAC must be non-zero
		 */
		if (is_zero_ether_addr(rule->pppoe_server_mac)) {
			nss_ppenl_warn("PPPoE server MAC cannot be zero when PPPoE is enabled\n");
			return -EINVAL;
		}
	}

	/*
	 * Validate inner IP protocol configuration when inner_pkt_type is IP
	 */
	if (is_create && (rule->inner_pkt_type == 1)) {
		uint16_t default_mask = 0xFFFF;

		nss_ppenl_info("Validating inner IP protocol configuration\n");

		/*
		 * udf[2] cannot be enabled when inner_pkt_type is IP
		 */
		if (rule->usr_data.udf[2].data_en) {
			nss_ppenl_warn("udf[2] cannot be enabled when inner_pkt_type is IP\n");
			return -EINVAL;
		}

		/*
		 * All 5 inner IP proto fields are mandatory
		 */
		if (rule->inner_ip_proto.ip_proto_offset == 0) {
			nss_ppenl_warn("ip_proto_offset is mandatory when inner_pkt_type is IP\n");
			return -EINVAL;
		}

		/*
		 * Either IPv4, IPv6 protocol or both needs to be matched
		 */
		if (rule->inner_ip_proto.ip_proto_ipv4_value == 0 &&
		    rule->inner_ip_proto.ip_proto_ipv6_value == 0) {
			nss_ppenl_warn("At least one inner IP protocol value must be matched when inner_pkt_type is IP\n");
			return -EINVAL;
		}

		/*
		 * At least one inner IP protocol mask must be non-zero
		 */
		if (rule->inner_ip_proto.ip_proto_ipv4_mask == 0 &&
		    rule->inner_ip_proto.ip_proto_ipv6_mask == 0) {
			nss_ppenl_warn("At least one of ip_proto_ipv4_mask or ip_proto_ipv6_mask must be non-zero when inner_pkt_type is IP\n");
			return -EINVAL;
		}

		/*
		 * If one mask is provided and the other is not, use the provided mask for the other case.
		 * If both are zero, default to 0xFFFF (though handled by validation above).
		 */
		if (rule->inner_ip_proto.ip_proto_ipv4_mask != 0) {
			default_mask = rule->inner_ip_proto.ip_proto_ipv4_mask;
		} else if (rule->inner_ip_proto.ip_proto_ipv6_mask != 0) {
			default_mask = rule->inner_ip_proto.ip_proto_ipv6_mask;
		}

		if (rule->inner_ip_proto.ip_proto_ipv4_mask == 0) {
			rule->inner_ip_proto.ip_proto_ipv4_mask = default_mask;
			rule->inner_ip_proto.ip_proto_ipv4_value = 0;
		}

		if (rule->inner_ip_proto.ip_proto_ipv6_mask == 0) {
			rule->inner_ip_proto.ip_proto_ipv6_mask = default_mask;
			rule->inner_ip_proto.ip_proto_ipv6_value = 0;
		}

		/*
		 * Validate offset bounds
		 */
		if (rule->inner_ip_proto.ip_proto_offset > NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET) {
			nss_ppenl_warn("ip_proto_offset %d exceeds maximum %d\n",
					rule->inner_ip_proto.ip_proto_offset, NSS_PPENL_TUN_RPS_MAX_UDF_OFFSET);
			return -EINVAL;
		}

		/*
		 * Validate that inner IP protocol offset is an even number (multiple of 2)
		 */
		if (rule->inner_ip_proto.ip_proto_offset % 2 != 0) {
			nss_ppenl_warn("ip_proto_offset %d must be an even number (multiple of 2)\n",
					rule->inner_ip_proto.ip_proto_offset);
			return -EINVAL;
		}

		/*
		 * Validate that inner IP protocol offset does not exceed header length
		 */
		if ((rule->inner_ip_proto.ip_proto_offset + 2) > rule->hdr_len) {
			nss_ppenl_warn("ip_proto_offset %d + 2 exceeds hdr_len %d\n",
					rule->inner_ip_proto.ip_proto_offset, rule->hdr_len);
			return -EINVAL;
		}

		/*
		 * Validate that inner IP protocol offset is not within Ethernet header if type is eth
		 */
		if ((rule->hdr_len_type == PPE_TUN_RPS_HDR_LEN_TYPE_ETH) &&
		    (rule->inner_ip_proto.ip_proto_offset < 14)) {
			nss_ppenl_warn("ip_proto_offset %d falls within Ethernet header (< 14)\n",
					rule->inner_ip_proto.ip_proto_offset);
			return -EINVAL;
		}

		nss_ppenl_info("Inner IP proto config: offset=%d, ipv4_val=0x%x, ipv4_mask=0x%x, ipv6_val=0x%x, ipv6_mask=0x%x\n",
				rule->inner_ip_proto.ip_proto_offset,
				rule->inner_ip_proto.ip_proto_ipv4_value,
				rule->inner_ip_proto.ip_proto_ipv4_mask,
				rule->inner_ip_proto.ip_proto_ipv6_value,
				rule->inner_ip_proto.ip_proto_ipv6_mask);
	}

	/*
	 * Validate WAN interface name for create operations only
	 */
	if (is_create) {
		if (rule->wan_if[0] == '\0') {
			nss_ppenl_warn("WAN interface name cannot be empty\n");
			return -EINVAL;
		}
	}

	/*
	 * Ensure WAN interface name is null-terminated
	 */
	rule->wan_if[IFNAMSIZ - 1] = '\0';

	nss_ppenl_info("WAN interface: %s\n", rule->wan_if);

	nss_ppenl_info("Netlink validation completed successfully - rule ready for PPE driver\n");
	return 0;
}

/*
 * operation table called by the generic netlink layer based on the command
 */
static struct genl_ops nss_ppenl_tun_rps_ops[] = {
	{.cmd = NSS_PPE_TUN_RPS_CREATE_RULE_MSG, .doit = nss_ppenl_tun_rps_ops_create_rule,},
	{.cmd = NSS_PPE_TUN_RPS_DESTROY_RULE_MSG, .doit = nss_ppenl_tun_rps_ops_destroy_rule,},
};

/*
 * Tunnel RPS family definition
 */
static struct genl_family nss_ppenl_tun_rps_family = {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(4, 9, 0))
	.id = GENL_ID_GENERATE,
#endif
	.name = NSS_PPENL_TUN_RPS_FAMILY,
	.hdrsize = sizeof(struct nss_ppenl_tun_rps_rule),
	.version = NSS_PPENL_VER,
	.maxattr = NSS_PPE_TUN_RPS_MAX_MSG_TYPES,
	.netnsok = true,
	.pre_doit = NULL,
	.post_doit = NULL,
	.ops = nss_ppenl_tun_rps_ops,
	.n_ops = ARRAY_SIZE(nss_ppenl_tun_rps_ops),
};

static void nss_ppenl_tun_rps_rule_dump(struct nss_ppe_nl_tun_rps *rule) {
	nss_ppenl_info("%px tunnel RPS rule dump from netlink\n"
			"is_ipv6: %d\n"
			"protocol: %d\n"
			"sport: %d\n"
			"dport: %d\n"
			"hdr_len_type: %d\n"
			"hdr_len: %d\n"
			"inner_pkt_type: %d\n"
			"udf[0]_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n"
			"udf[1]_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n"
			"udf[2]_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n",
			rule, rule->is_ipv6,
			rule->protocol,
			rule->sport,
			rule->dport,
			rule->hdr_len_type,
			rule->hdr_len,
			rule->inner_pkt_type,
			rule->usr_data.udf[0].data_en, rule->usr_data.udf[0].data_offset,
			rule->usr_data.udf[0].data_val, rule->usr_data.udf[0].data_mask,
			rule->usr_data.udf[1].data_en, rule->usr_data.udf[1].data_offset,
			rule->usr_data.udf[1].data_val, rule->usr_data.udf[1].data_mask,
			rule->usr_data.udf[2].data_en, rule->usr_data.udf[2].data_offset,
			rule->usr_data.udf[2].data_val, rule->usr_data.udf[2].data_mask);

	/*
	 * Dump IP addresses
	 */
	if (rule->is_ipv6) {
		nss_ppenl_info("SIP IPv6: %pI6c\n", rule->ipv6_sip);
		nss_ppenl_info("DIP IPv6: %pI6c\n", rule->ipv6_dip);
	} else {
		nss_ppenl_info("SIP IPv4: %pI4\n", &rule->ipv4_sip);
		nss_ppenl_info("DIP IPv4: %pI4\n", &rule->ipv4_dip);
	}

	/*
	 * Dump header match fields based on header type
	 */
	switch (rule->hdr_len_type) {
	case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
		nss_ppenl_info("Header Match Type: ETH\n");
		nss_ppenl_info("  eth_protocol: 0x%04x\n", rule->header_match.l2_match.eth_protocol);
		nss_ppenl_info("  eth_protocol_mask: 0x%04x\n", rule->header_match.l2_match.eth_protocol_mask);
		break;
	case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
		nss_ppenl_info("Header Match Type: IP\n");
		nss_ppenl_info("  ip_protocol: %d\n", rule->header_match.l3_match.ip_protocol);
		nss_ppenl_info("  ip_protocol_mask: 0x%02x\n", rule->header_match.l3_match.ip_protocol_mask);
		break;
	case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
		nss_ppenl_info("Header Match Type: UDP\n");
		nss_ppenl_info("  udp_sport: %d\n", rule->header_match.l4_match.udp_sport);
		nss_ppenl_info("  udp_dport: %d\n", rule->header_match.l4_match.udp_dport);
		break;
	default:
		nss_ppenl_info("Header Match Type: UNKNOWN (%d)\n", rule->hdr_len_type);
		break;
	}

	/*
	 * Dump inner IP protocol fields when inner_pkt_type is IP
	 */
	if (rule->inner_pkt_type == 1) {
		nss_ppenl_info("Inner IP Proto: offset=%d, ipv4_val=0x%x, ipv4_mask=0x%x, ipv6_val=0x%x, ipv6_mask=0x%x\n",
				rule->inner_ip_proto.ip_proto_offset,
				rule->inner_ip_proto.ip_proto_ipv4_value,
				rule->inner_ip_proto.ip_proto_ipv4_mask,
				rule->inner_ip_proto.ip_proto_ipv6_value,
				rule->inner_ip_proto.ip_proto_ipv6_mask);
	}

	/*
	 * Dump PPPoE configuration
	 */
	if (rule->pppoe_en) {
		nss_ppenl_info("PPPoE: enabled, session_id=%u, server_mac=%pM\n",
				rule->pppoe_session_id, rule->pppoe_server_mac);
	} else {
		nss_ppenl_info("PPPoE: disabled\n");
	}
}

/*
 * nss_ppenl_tun_rps_ops_create_rule()
 * 	rule create handler
 */
static int nss_ppenl_tun_rps_ops_create_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_tun_rps_rule *nl_tun_rps_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
	int ret;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_tun_rps_family, info, NSS_PPE_TUN_RPS_CREATE_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract tunnel RPS rule create data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_tun_rps_rule = container_of(nl_cm, struct nss_ppenl_tun_rps_rule, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * Validate rule parameters before processing
	 */
	error = nss_ppenl_tun_rps_validate_rule(&nl_tun_rps_rule->rule, true);
	if (error < 0) {
		nss_ppenl_warn("Rule validation failed: %d\n", error);
		return error;
	}

	/*
	 * Copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("unable to save response data from NL buffer (pid: %d)\n", pid);
		return -ENOMEM;
	}

	nss_ppenl_tun_rps_rule_dump(&nl_tun_rps_rule->rule);

	/*
	 * Call PPE tunnel RPS rule creation API based on IP version
	 */
	if (nl_tun_rps_rule->rule.is_ipv6) {
		struct ppe_tun_rps_ipv6_rule_create_msg ppe_msg;
		enum ppe_tun_rps_ret ppe_ret;

		error = nss_ppenl_tun_rps_map_to_ipv6_create(&nl_tun_rps_rule->rule, &ppe_msg);
		if (error < 0) {
			nss_ppenl_warn("Failed to map IPv6 rule to PPE format: %d\n", error);
			status = error;
		} else {
			ppe_ret = ppe_tun_rps_ipv6_rule_create(&ppe_msg);
			if (ppe_ret == PPE_TUN_RPS_RET_SUCCESS) {
				nss_ppenl_info("%s: PPE IPv6 tunnel RPS rule create success\n", __func__);
				status = 0;
			} else {
				nss_ppenl_warn("PPE IPv6 tunnel RPS rule create failed: %d\n", ppe_ret);
				/*
				 * Convert PPE error to Linux error code
				 */
				status = -EINVAL;
			}
		}
	} else {
		struct ppe_tun_rps_ipv4_rule_create_msg ppe_msg;
		enum ppe_tun_rps_ret ppe_ret;

		error = nss_ppenl_tun_rps_map_to_ipv4_create(&nl_tun_rps_rule->rule, &ppe_msg);
		if (error < 0) {
			nss_ppenl_warn("Failed to map IPv4 rule to PPE format: %d\n", error);
			status = error;
		} else {
			ppe_ret = ppe_tun_rps_ipv4_rule_create(&ppe_msg);
			if (ppe_ret == PPE_TUN_RPS_RET_SUCCESS) {
				nss_ppenl_info("%s: PPE IPv4 tunnel RPS rule create success\n", __func__);
				status = 0;
			} else {
				nss_ppenl_warn("PPE IPv4 tunnel RPS rule create failed: %d\n", ppe_ret);
				/*
				 * Convert PPE error to Linux error code
				 */
				status = -EINVAL;
			}
		}
	}

	if (status == 0) {
		nss_ppenl_info("%s: PPE tunnel RPS rule create success\n", __func__);
	} else {
		nss_ppenl_info("create tunnel RPS rule in ppe driver failed, error = %d\n", status);
	}

	nl_tun_rps_rule->rule.ret = status;
	ret = nl_tun_rps_rule->rule.ret;

	/*
	 * Send the response code to user application
	 */
	nl_tun_rps_rule = nss_ppenl_get_data(resp);
	nl_tun_rps_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_tun_rps_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_tun_rps_ops_destroy_rule()
 *	rule delete handler
 */
static int nss_ppenl_tun_rps_ops_destroy_rule(struct sk_buff *skb, struct genl_info *info)
{
	struct nss_ppenl_tun_rps_rule *nl_tun_rps_rule;
	struct nss_ppenl_cmn *nl_cm;
	struct sk_buff *resp;
	uint32_t pid;
	int error, status = 0;
	int ret;

	/*
	 * Extract the message payload
	 */
	nl_cm = nss_ppenl_get_msg(&nss_ppenl_tun_rps_family, info, NSS_PPE_TUN_RPS_DESTROY_RULE_MSG);
	if (!nl_cm) {
		nss_ppenl_warn("unable to extract tunnel RPS rule destroy data\n");
		return -EINVAL;
	}

	/*
	 * Message validation required before accepting the configuration
	 */
	nl_tun_rps_rule = container_of(nl_cm, struct nss_ppenl_tun_rps_rule, cm);
	pid = nl_cm->pid;
	nss_ppenl_info("%s: pid: %d\n", __func__, pid);

	/*
	 * Validate rule parameters before processing
	 */
	error = nss_ppenl_tun_rps_validate_rule(&nl_tun_rps_rule->rule, false);
	if (error < 0) {
		nss_ppenl_warn("Rule validation failed: %d\n", error);
		return error;
	}

	/*
	 * Copy the NL message for response
	 */
	resp = nss_ppenl_copy_msg(skb);
	if (!resp) {
		nss_ppenl_warn("unable to save response data from NL buffer (pid: %d)\n", pid);
		return -ENOMEM;
	}

	nss_ppenl_tun_rps_rule_dump(&nl_tun_rps_rule->rule);

	/*
	 * Call PPE tunnel RPS rule destruction API based on IP version
	 */
	if (nl_tun_rps_rule->rule.is_ipv6) {
		struct ppe_tun_rps_ipv6_rule_destroy_msg ppe_msg;
		enum ppe_tun_rps_ret ppe_ret;

		error = nss_ppenl_tun_rps_map_to_ipv6_destroy(&nl_tun_rps_rule->rule, &ppe_msg);
		if (error < 0) {
			nss_ppenl_warn("Failed to map IPv6 rule to PPE format: %d\n", error);
			status = error;
		} else {
			ppe_ret = ppe_tun_rps_ipv6_rule_destroy(&ppe_msg);
			if (ppe_ret == PPE_TUN_RPS_RET_SUCCESS) {
				nss_ppenl_info("%s: PPE IPv6 tunnel RPS rule destroy success\n", __func__);
				status = 0;
			} else {
				nss_ppenl_warn("PPE IPv6 tunnel RPS rule destroy failed: %d\n", ppe_ret);
				/*
				 * Convert PPE error to Linux error code
				 */
				status = -EINVAL;
			}
		}
	} else {
		struct ppe_tun_rps_ipv4_rule_destroy_msg ppe_msg;
		enum ppe_tun_rps_ret ppe_ret;

		error = nss_ppenl_tun_rps_map_to_ipv4_destroy(&nl_tun_rps_rule->rule, &ppe_msg);
		if (error < 0) {
			nss_ppenl_warn("Failed to map IPv4 rule to PPE format: %d\n", error);
			status = error;
		} else {
			ppe_ret = ppe_tun_rps_ipv4_rule_destroy(&ppe_msg);
			if (ppe_ret == PPE_TUN_RPS_RET_SUCCESS) {
				nss_ppenl_info("%s: PPE IPv4 tunnel RPS rule destroy success\n", __func__);
				status = 0;
			} else {
				nss_ppenl_warn("PPE IPv4 tunnel RPS rule destroy failed: %d\n", ppe_ret);
				/*
				 * Convert PPE error to Linux error code
				 */
				status = -EINVAL;
			}
		}
	}

	if (status != 0) {
		nss_ppenl_warn("unable to destroy tunnel RPS rule in ppe driver, error = %d\n", status);
		nl_tun_rps_rule->rule.ret = status;
	} else {
		nl_tun_rps_rule->rule.ret = 0;
	}

	ret = nl_tun_rps_rule->rule.ret;

	/*
	 * Send the response back to user application
	 */
	nl_tun_rps_rule = nss_ppenl_get_data(resp);
	nl_tun_rps_rule->rule.ret = ret;

	nss_ppenl_trace("Sending response to userspace: ret %d\n", nl_tun_rps_rule->rule.ret);
	nss_ppenl_ucast_resp(resp);
	return 0;
}

/*
 * nss_ppenl_tun_rps_init()
 *	handler init
 */
bool nss_ppenl_tun_rps_init(void)
{
	int error;

	nss_ppenl_info("Init NSS PPE Tunnel RPS handler\n");

	/*
	 * Register NETLINK ops with the family
	 */
	error = genl_register_family(&nss_ppenl_tun_rps_family);
	if (error != 0) {
		nss_ppenl_info_always("Error: unable to register Tunnel RPS family\n");
		return false;
	}

	nss_ppenl_info("size of the msg in netlink = %d\n", (uint32_t)sizeof(struct nss_ppenl_tun_rps_rule));
	return true;
}

/*
 * nss_ppenl_tun_rps_exit()
 *	handler exit
 */
bool nss_ppenl_tun_rps_exit(void)
{
	int error;

	nss_ppenl_info("Exit NSS netlink Tunnel RPS handler\n");

	/*
	 * Unregister the ops family
	 */
	error = genl_unregister_family(&nss_ppenl_tun_rps_family);
	if (error != 0) {
		nss_ppenl_info_always("unable to unregister Tunnel RPS NETLINK family\n");
		return false;
	}

	return true;
}
