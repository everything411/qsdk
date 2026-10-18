/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <ppe_drv.h>
#include <ppe_drv_v4.h>
#include <ppe_drv_v6.h>
#include <ppe_tun_rps.h>
#include "ppe_tun_rps.h"
#include <ppe_drv_tun_rps.h>

/*
 * Constants for validation
 */
#define PPE_TUN_RPS_MAX_HDR_LEN 126
#define PPE_TUN_RPS_MAX_UDF_OFFSET 126

struct ppe_tun_rps gbl_tun_rps;

/*
 * ppe_tun_rps_rule_dump
 *      Dump tunnel RPS rule
 */
static void ppe_tun_rps_rule_dump(void *rule, bool is_create)
{
	if (is_create) {
		struct ppe_drv_tun_rps_rule_create *prpsc = (struct ppe_drv_tun_rps_rule_create *)rule;
		ppe_tun_rps_info("Tunnel RPS rule dump from PPE rule layer\n");
		ppe_tun_rps_info("is_ipv6: %d\n", (prpsc->cmn.rule_type == PPE_DRV_TUN_RPS_RULE_TYPE_IPV6));
		ppe_tun_rps_info("protocol: %d\n", prpsc->cmn.protocol);
		ppe_tun_rps_info("sport: %d\n", prpsc->cmn.flow_ident);
		ppe_tun_rps_info("dport: %d\n", prpsc->cmn.return_ident);
		ppe_tun_rps_info("hdr_len_type: %d\n", prpsc->offset_type);
		ppe_tun_rps_info("hdr_len: %d\n", prpsc->header_len);
		ppe_tun_rps_info("inner_pkt_type: %d\n", prpsc->inner_type);
		ppe_tun_rps_info("usr_data1_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsc->cmn.udf0_en, prpsc->cmn.udf0_offset, prpsc->cmn.udf0_val, prpsc->cmn.udf0_mask);
		ppe_tun_rps_info("usr_data2_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsc->cmn.udf1_en, prpsc->cmn.udf1_offset, prpsc->cmn.udf1_val, prpsc->cmn.udf1_mask);
		ppe_tun_rps_info("usr_data3_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsc->cmn.udf2_en, prpsc->cmn.udf2_offset, prpsc->cmn.udf2_val, prpsc->cmn.udf2_mask);
		ppe_tun_rps_info("WAN interface: %s\n", prpsc->wan_if);

		/*
		 * Dump IP addresses
		 */
		if (prpsc->cmn.rule_type == PPE_DRV_TUN_RPS_RULE_TYPE_IPV4) {
			ppe_tun_rps_info("SIP IPv4: %pI4\n", &prpsc->cmn.flow_ip[0]);
			ppe_tun_rps_info("DIP IPv4: %pI4\n", &prpsc->cmn.return_ip[0]);
		} else {
			ppe_tun_rps_info("SIP IPv6: %pI6\n", prpsc->cmn.flow_ip);
			ppe_tun_rps_info("DIP IPv6: %pI6\n", prpsc->cmn.return_ip);
		}

		/*
		 * Dump header match fields based on offset type
		 */
		switch (prpsc->offset_type) {
		case PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH:
			ppe_tun_rps_info("Header Match Type: L2 (ETH)\n");
			ppe_tun_rps_info("  eth_protocol: 0x%04x\n", prpsc->cmn.header_match.l2_match.eth_protocol);
			ppe_tun_rps_info("  eth_protocol_mask: 0x%04x\n", prpsc->cmn.header_match.l2_match.eth_protocol_mask);
			break;
		case PPE_DRV_TUN_RPS_OFFSET_TYPE_IP:
			ppe_tun_rps_info("Header Match Type: L3 (IP)\n");
			ppe_tun_rps_info("  ip_protocol: %d\n", prpsc->cmn.header_match.l3_match.ip_protocol);
			ppe_tun_rps_info("  ip_protocol_mask: 0x%02x\n", prpsc->cmn.header_match.l3_match.ip_protocol_mask);
			break;
		case PPE_DRV_TUN_RPS_OFFSET_TYPE_UDP:
			ppe_tun_rps_info("Header Match Type: L4 (UDP)\n");
			ppe_tun_rps_info("  udp_sport: %d\n", prpsc->cmn.header_match.l4_match.udp_sport);
			ppe_tun_rps_info("  udp_dport: %d\n", prpsc->cmn.header_match.l4_match.udp_dport);
			break;
		default:
			ppe_tun_rps_info("Header Match Type: UNKNOWN (%d)\n", prpsc->offset_type);
			break;
		}

		/*
		 * Dump inner IP protocol fields when inner_pkt_type is IP
		 */
		if (prpsc->inner_type == PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP) {
			ppe_tun_rps_info("Inner IP Proto: offset=%d, ipv4_val=0x%x, ipv4_mask=0x%x, ipv6_val=0x%x, ipv6_mask=0x%x\n",
					prpsc->inner_ip_proto.ip_proto_offset,
					prpsc->inner_ip_proto.ip_proto_ipv4_value,
					prpsc->inner_ip_proto.ip_proto_ipv4_mask,
					prpsc->inner_ip_proto.ip_proto_ipv6_value,
					prpsc->inner_ip_proto.ip_proto_ipv6_mask);
		}

		/*
		 * Dump PPPoE configuration
		 */
		if (prpsc->pppoe_en) {
			ppe_tun_rps_info("PPPoE: enabled, session_id=%u, server_mac=%pM\n",
					prpsc->pppoe_session_id, prpsc->pppoe_server_mac);
		} else {
			ppe_tun_rps_info("PPPoE: disabled\n");
		}
	} else {
		struct ppe_drv_tun_rps_rule_destroy *prpsd = (struct ppe_drv_tun_rps_rule_destroy *)rule;
		ppe_tun_rps_info("Tunnel RPS rule destroy dump from PPE rule layer\n");
		ppe_tun_rps_info("is_ipv6: %d\n", (prpsd->cmn.rule_type == PPE_DRV_TUN_RPS_RULE_TYPE_IPV6));
		ppe_tun_rps_info("protocol: %d\n", prpsd->cmn.protocol);
		ppe_tun_rps_info("sport: %d\n", prpsd->cmn.flow_ident);
		ppe_tun_rps_info("dport: %d\n", prpsd->cmn.return_ident);
		ppe_tun_rps_info("usr_data1_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsd->cmn.udf0_en, prpsd->cmn.udf0_offset, prpsd->cmn.udf0_val, prpsd->cmn.udf0_mask);
		ppe_tun_rps_info("usr_data2_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsd->cmn.udf1_en, prpsd->cmn.udf1_offset, prpsd->cmn.udf1_val, prpsd->cmn.udf1_mask);
		ppe_tun_rps_info("usr_data3_en: %d, offset: %d, val: 0x%x, mask: 0x%x\n", prpsd->cmn.udf2_en, prpsd->cmn.udf2_offset, prpsd->cmn.udf2_val, prpsd->cmn.udf2_mask);

		/*
		 * Dump IP addresses
		 */
		if (prpsd->cmn.rule_type == PPE_DRV_TUN_RPS_RULE_TYPE_IPV4) {
			ppe_tun_rps_info("SIP IPv4: %pI4\n", &prpsd->cmn.flow_ip[0]);
			ppe_tun_rps_info("DIP IPv4: %pI4\n", &prpsd->cmn.return_ip[0]);
		} else {
			ppe_tun_rps_info("SIP IPv6: %pI6\n", prpsd->cmn.flow_ip);
			ppe_tun_rps_info("DIP IPv6: %pI6\n", prpsd->cmn.return_ip);
		}
	}
}

/*
 * ppe_tun_rps_create_fill_header_match
 *      Fill header matching fields to tunnel RPS create structure
 */
static void ppe_tun_rps_create_fill_header_match(void *header_match, struct ppe_drv_tun_rps_rule_create *create, enum ppe_tun_rps_hdr_len_type hdr_type)
{
	struct ppe_tun_rps_l2_match *l2;
	struct ppe_tun_rps_l3_match *l3;
	struct ppe_tun_rps_l4_match *l4;

	if (!header_match) {
		return;
	}

	/*
	 * Initialize header match union to zero
	 */
	memset(&create->cmn.header_match, 0, sizeof(create->cmn.header_match));

	switch (hdr_type) {
	case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
		l2 = (struct ppe_tun_rps_l2_match *)header_match;
		create->cmn.header_match.l2_match.eth_protocol = l2->eth_protocol;
		create->cmn.header_match.l2_match.eth_protocol_mask = l2->eth_protocol_mask;
		break;

	case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
		l3 = (struct ppe_tun_rps_l3_match *)header_match;
		create->cmn.header_match.l3_match.ip_protocol = l3->ip_protocol;
		create->cmn.header_match.l3_match.ip_protocol_mask = l3->ip_protocol_mask;
		break;

	case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
		l4 = (struct ppe_tun_rps_l4_match *)header_match;
		create->cmn.header_match.l4_match.udp_sport = l4->udp_sport;
		create->cmn.header_match.l4_match.udp_dport = l4->udp_dport;
		break;

	default:
		/*
		 * Unknown header type - already initialized to zero
		 */
		break;
	}
}

/*
 * ppe_tun_rps_create_fill_udf_data
 *      Fill User data to tunnel RPS create structure
 */
static void ppe_tun_rps_create_fill_udf_data(struct ppe_tun_rps_user_data *user_data, struct ppe_drv_tun_rps_rule_create *create)
{
	if (user_data->udf[0].enable) {
		create->cmn.udf0_en = user_data->udf[0].enable;
		create->cmn.udf0_offset = user_data->udf[0].offset;
		create->cmn.udf0_val = user_data->udf[0].val;
		create->cmn.udf0_mask = user_data->udf[0].mask;
	}

	if (user_data->udf[1].enable) {
		create->cmn.udf1_en = user_data->udf[1].enable;
		create->cmn.udf1_offset = user_data->udf[1].offset;
		create->cmn.udf1_val = user_data->udf[1].val;
		create->cmn.udf1_mask = user_data->udf[1].mask;
	}

	if (user_data->udf[2].enable) {
		create->cmn.udf2_en = user_data->udf[2].enable;
		create->cmn.udf2_offset = user_data->udf[2].offset;
		create->cmn.udf2_val = user_data->udf[2].val;
		create->cmn.udf2_mask = user_data->udf[2].mask;
	}
}

/*
 * ppe_tun_rps_destroy_fill_udf_data
 *      Fill user data to tunnel RPS destroy structure
 */
static void ppe_tun_rps_destroy_fill_udf_data(struct ppe_tun_rps_user_data *user_data, struct ppe_drv_tun_rps_rule_destroy *destroy)
{
	if (user_data->udf[0].enable) {
		destroy->cmn.udf0_en = user_data->udf[0].enable;
		destroy->cmn.udf0_offset = user_data->udf[0].offset;
		destroy->cmn.udf0_val = user_data->udf[0].val;
		destroy->cmn.udf0_mask = user_data->udf[0].mask;
	}

	if (user_data->udf[1].enable) {
		destroy->cmn.udf1_en = user_data->udf[1].enable;
		destroy->cmn.udf1_offset = user_data->udf[1].offset;
		destroy->cmn.udf1_val = user_data->udf[1].val;
		destroy->cmn.udf1_mask = user_data->udf[1].mask;
	}

	if (user_data->udf[2].enable) {
		destroy->cmn.udf2_en = user_data->udf[2].enable;
		destroy->cmn.udf2_offset = user_data->udf[2].offset;
		destroy->cmn.udf2_val = user_data->udf[2].val;
		destroy->cmn.udf2_mask = user_data->udf[2].mask;
	}
}

/*
 * ppe_tun_rps_validate_ip_addresses()
 *	Validate IP addresses and check if they provide match criteria
 */
static inline bool ppe_tun_rps_validate_ip_addresses(struct ppe_drv_tun_rps_rule_create *create, bool is_ipv6)
{
	bool has_match_criteria = false;

	if (!is_ipv6) {
		if (create->cmn.flow_ip[0] != 0) {
			has_match_criteria = true;
			ppe_tun_rps_info("SIP: non-zero (will match)\n");
		}

		if (create->cmn.return_ip[0] != 0) {
			has_match_criteria = true;
			ppe_tun_rps_info("DIP: non-zero (will match)\n");
		}
	} else {
		bool sip_all_zero = (create->cmn.flow_ip[0] == 0 && create->cmn.flow_ip[1] == 0 &&
				     create->cmn.flow_ip[2] == 0 && create->cmn.flow_ip[3] == 0);
		bool dip_all_zero = (create->cmn.return_ip[0] == 0 && create->cmn.return_ip[1] == 0 &&
				     create->cmn.return_ip[2] == 0 && create->cmn.return_ip[3] == 0);

		if (!sip_all_zero) {
			has_match_criteria = true;
			ppe_tun_rps_info("SIP IPv6: non-zero (will match)\n");
		}

		if (!dip_all_zero) {
			has_match_criteria = true;
			ppe_tun_rps_info("DIP IPv6: non-zero (will match)\n");
		}
	}

	return has_match_criteria;
}

/*
 * ppe_tun_rps_validate_ports_protocol()
 *	Validate ports and protocol and check if they provide match criteria
 */
static inline bool ppe_tun_rps_validate_ports_protocol(struct ppe_drv_tun_rps_rule_create *create)
{
	bool has_match_criteria = false;

	if (create->cmn.flow_ident != 0) {
		has_match_criteria = true;
		ppe_tun_rps_info("Source Port: non-zero (will match)\n");
	}

	if (create->cmn.return_ident != 0) {
		has_match_criteria = true;
		ppe_tun_rps_info("Destination Port: non-zero (will match)\n");
	}

	if (create->cmn.protocol != 0) {
		has_match_criteria = true;
		ppe_tun_rps_info("Protocol: non-zero (will match)\n");
	}

	return has_match_criteria;
}

/*
 * ppe_tun_rps_validate_udf_fields()
 *	Validate UDF fields and check if they provide match criteria
 */
static inline bool ppe_tun_rps_validate_udf_fields(struct ppe_drv_tun_rps_rule_create *create, enum ppe_tun_rps_ret *ret)
{
	bool has_match_criteria = false;

	if (create->cmn.udf0_en) {
		has_match_criteria = true;

		if (create->cmn.udf0_offset > PPE_TUN_RPS_MAX_UDF_OFFSET) {
			ppe_tun_rps_warn("Invalid user_data1_offset %d (max: %d)\n",
					create->cmn.udf0_offset, PPE_TUN_RPS_MAX_UDF_OFFSET);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->cmn.udf0_offset % 2 != 0) {
			ppe_tun_rps_warn("user_data1_offset %d must be an even number (multiple of 2)\n",
					create->cmn.udf0_offset);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if ((create->cmn.udf0_offset + 2) > create->header_len) {
			ppe_tun_rps_warn("user_data1_offset %d + 2 exceeds header length %d\n",
					create->cmn.udf0_offset, create->header_len);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->offset_type == PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH && create->cmn.udf0_offset < 14) {
			ppe_tun_rps_warn("user_data1_offset %d falls within Ethernet header (< 14)\n",
					create->cmn.udf0_offset);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->cmn.udf0_mask == 0) {
			ppe_tun_rps_warn("user_data1_mask cannot be zero when enabled\n");
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_MASK;
			return false;
		}

		ppe_tun_rps_info("UDF1: enabled and validated\n");
	}

	if (create->cmn.udf1_en) {
		has_match_criteria = true;

		if (create->cmn.udf1_offset > PPE_TUN_RPS_MAX_UDF_OFFSET) {
			ppe_tun_rps_warn("Invalid user_data2_offset %d (max: %d)\n",
					create->cmn.udf1_offset, PPE_TUN_RPS_MAX_UDF_OFFSET);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->cmn.udf1_offset % 2 != 0) {
			ppe_tun_rps_warn("user_data2_offset %d must be an even number (multiple of 2)\n",
					create->cmn.udf1_offset);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if ((create->cmn.udf1_offset + 2) > create->header_len) {
			ppe_tun_rps_warn("user_data2_offset %d + 2 exceeds header length %d\n",
					create->cmn.udf1_offset, create->header_len);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->offset_type == PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH && create->cmn.udf1_offset < 14) {
			ppe_tun_rps_warn("user_data2_offset %d falls within Ethernet header (< 14)\n",
					create->cmn.udf1_offset);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->cmn.udf1_mask == 0) {
			ppe_tun_rps_warn("user_data2_mask cannot be zero when enabled\n");
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_MASK;
			return false;
		}

		ppe_tun_rps_info("UDF2: enabled and validated\n");
	}

	if (create->cmn.udf2_en) {
		has_match_criteria = true;

		if (create->cmn.udf2_offset > PPE_TUN_RPS_MAX_UDF_OFFSET) {
			ppe_tun_rps_warn("Invalid user_data3_offset %d (max: %d)\n",
					create->cmn.udf2_offset, PPE_TUN_RPS_MAX_UDF_OFFSET);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->cmn.udf2_offset % 2 != 0) {
			ppe_tun_rps_warn("user_data3_offset %d must be an even number (multiple of 2)\n",
					create->cmn.udf2_offset);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if ((create->cmn.udf2_offset + 2) > create->header_len) {
			ppe_tun_rps_warn("user_data3_offset %d + 2 exceeds header length %d\n",
					create->cmn.udf2_offset, create->header_len);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->offset_type == PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH && create->cmn.udf2_offset < 14) {
			ppe_tun_rps_warn("user_data3_offset %d falls within Ethernet header (< 14)\n",
					create->cmn.udf2_offset);
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
			return false;
		}

		if (create->cmn.udf2_mask == 0) {
			ppe_tun_rps_warn("user_data3_mask cannot be zero when enabled\n");
			*ret = PPE_TUN_RPS_RET_INVALID_UDF_MASK;
			return false;
		}

		ppe_tun_rps_info("UDF3: enabled and validated\n");
	}

	*ret = PPE_TUN_RPS_RET_SUCCESS;
	return has_match_criteria;
}

/*
 * ppe_tun_rps_validate_pppoe()
 *	Validate PPPoE configuration
 */
static inline bool ppe_tun_rps_validate_pppoe(struct ppe_drv_tun_rps_rule_create *create)
{
	if (create->pppoe_session_id == 0) {
		ppe_tun_rps_warn("Invalid PPPoE session ID: 0\n");
		return false;
	}

	if (is_zero_ether_addr(create->pppoe_server_mac)) {
		ppe_tun_rps_warn("Invalid PPPoE server MAC: %pM\n", create->pppoe_server_mac);
		return false;
	}

	return true;
}

/*
 * ppe_tun_rps_validate_create_rule()
 *	Comprehensive validation for tunnel RPS create rules
 */
static enum ppe_tun_rps_ret ppe_tun_rps_validate_create_rule(struct ppe_drv_tun_rps_rule_create *create)
{
	bool has_match_criteria = false;
	bool is_ipv6 = false;
	enum ppe_tun_rps_ret ret;

	if (create->cmn.rule_type == PPE_DRV_TUN_RPS_RULE_TYPE_IPV6) {
		is_ipv6 = true;
	}

	/*
	 * Validate WAN interface name first
	 */
	if (create->wan_if[0] == '\0') {
		ppe_tun_rps_warn("WAN interface name is mandatory\n");
		return PPE_TUN_RPS_RET_INVALID_WAN_IF;
	}

	ppe_tun_rps_info("WAN interface: %s\n", create->wan_if);

	/*
	 * Validate header length bounds
	 */
	if (create->header_len > PPE_TUN_RPS_MAX_HDR_LEN) {
		ppe_tun_rps_warn("Invalid header length %d (max: %d)\n",
				create->header_len, PPE_TUN_RPS_MAX_HDR_LEN);
		return PPE_TUN_RPS_RET_INVALID_HDR_LEN;
	}

	/*
	 * Validate that header length is even (multiple of 2) as required by hardware/SSDK
	 */
	if (create->header_len % 2 != 0) {
		ppe_tun_rps_warn("Header length %d must be an even number (multiple of 2)\n",
				create->header_len);
		return PPE_TUN_RPS_RET_INVALID_HDR_LEN;
	}

	/*
	 * Check for at least one matching criterion
	 * Zero values are treated as wildcards
	 */
	if (ppe_tun_rps_validate_ip_addresses(create, is_ipv6)) {
		has_match_criteria = true;
	}

	if (ppe_tun_rps_validate_ports_protocol(create)) {
		has_match_criteria = true;
	}

	/*
	 * Validate UDF fields
	 */
	if (ppe_tun_rps_validate_udf_fields(create, &ret)) {
		has_match_criteria = true;
	} else if (ret != PPE_TUN_RPS_RET_SUCCESS) {
		return ret;
	}

	/*
	 * Require at least one matching criterion
	 */
	if (!has_match_criteria) {
		ppe_tun_rps_warn("Rule must have at least one matching criterion\n");
		return PPE_TUN_RPS_RET_NO_MATCH_CRITERIA;
	}

	/*
	 * Validate PPPoE fields only if PPPoE is enabled
	 */
	if (create->pppoe_en && !ppe_tun_rps_validate_pppoe(create)) {
		return PPE_TUN_RPS_RET_FAILURE;
	}

	ppe_tun_rps_info("PPE rule layer validation passed\n");
	return PPE_TUN_RPS_RET_SUCCESS;
}

/*
 * ppe_tun_rps_validate_inner_ip_config()
 *	Validate inner IP protocol configuration
 *	When inner_pkt_type is IP, all 5 IP proto fields are mandatory and user_data3 cannot be enabled
 */
static enum ppe_tun_rps_ret ppe_tun_rps_validate_inner_ip_config(struct ppe_drv_tun_rps_rule_create *create)
{
	uint16_t default_mask = 0xFFFF;

	/*
	 * Only validate when inner packet type is IP
	 */
	if (create->inner_type != PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP) {
		return PPE_TUN_RPS_RET_SUCCESS;
	}

	ppe_tun_rps_info("Validating inner IP protocol configuration\n");

	/*
	 * When inner_pkt_type is IP, user_data3 cannot be enabled
	 * (it's reserved internally for IP protocol matching)
	 * udf0- > user_data1
	 */
	if (create->cmn.udf2_en) {
		ppe_tun_rps_warn("user_data3 cannot be enabled when inner_pkt_type is IP\n");
		return PPE_TUN_RPS_RET_INVALID_UDF3_WITH_INNER_IP;
	}

	/*
	 * All 5 inner IP protocol fields are mandatory when inner_pkt_type is IP
	 */
	if (create->inner_ip_proto.ip_proto_offset == 0) {
		ppe_tun_rps_warn("ip_proto_offset is mandatory when inner_pkt_type is IP\n");
		return PPE_TUN_RPS_RET_INVALID_INNER_IP_CONFIG;
	}

	/*
	 * Either IPv4, IPv6 protocol or both needs to be matched
	 */
	if (create->inner_ip_proto.ip_proto_ipv4_value == 0 &&
	    create->inner_ip_proto.ip_proto_ipv6_value == 0) {
		ppe_tun_rps_warn("At least one inner IP protocol value must be matched when inner_pkt_type is IP\n");
		return PPE_TUN_RPS_RET_INVALID_INNER_IP_CONFIG;
	}

	/*
	 * At least one inner IP protocol mask must be non-zero
	 */
	if (create->inner_ip_proto.ip_proto_ipv4_mask == 0 &&
	    create->inner_ip_proto.ip_proto_ipv6_mask == 0) {
		ppe_tun_rps_warn("At least one of ip_proto_ipv4_mask or ip_proto_ipv6_mask must be non-zero\n");
		return PPE_TUN_RPS_RET_INVALID_INNER_IP_CONFIG;
	}

	/*
	 * If one mask is provided and the other is not, use the provided mask for the other case.
	 * If both are zero, default to 0xFFFF (though handled by validation above).
	 */
	if (create->inner_ip_proto.ip_proto_ipv4_mask != 0) {
		default_mask = create->inner_ip_proto.ip_proto_ipv4_mask;
	} else if (create->inner_ip_proto.ip_proto_ipv6_mask != 0) {
		default_mask = create->inner_ip_proto.ip_proto_ipv6_mask;
	}

	if (create->inner_ip_proto.ip_proto_ipv4_mask == 0) {
		create->inner_ip_proto.ip_proto_ipv4_mask = default_mask;
		create->inner_ip_proto.ip_proto_ipv4_value = 0;
	}

	if (create->inner_ip_proto.ip_proto_ipv6_mask == 0) {
		create->inner_ip_proto.ip_proto_ipv6_mask = default_mask;
		create->inner_ip_proto.ip_proto_ipv6_value = 0;
	}

	/*
	 * Validate offset bounds
	 */
	if (create->inner_ip_proto.ip_proto_offset > PPE_TUN_RPS_MAX_UDF_OFFSET) {
		ppe_tun_rps_warn("ip_proto_offset %d exceeds maximum %d\n",
				create->inner_ip_proto.ip_proto_offset, PPE_TUN_RPS_MAX_UDF_OFFSET);
		return PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
	}

	/*
	 * Validate that inner IP protocol offset is an even number (multiple of 2)
	 */
	if (create->inner_ip_proto.ip_proto_offset % 2 != 0) {
		ppe_tun_rps_warn("ip_proto_offset %d must be an even number (multiple of 2)\n",
				create->inner_ip_proto.ip_proto_offset);
		return PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
	}

	/*
	 * Validate that inner IP protocol offset does not exceed header length
	 */
	if ((create->inner_ip_proto.ip_proto_offset + 2) > create->header_len) {
		ppe_tun_rps_warn("ip_proto_offset %d + 2 exceeds header length %d\n",
				create->inner_ip_proto.ip_proto_offset, create->header_len);
		return PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
	}

	/*
	 * Validate that inner IP protocol offset is not within Ethernet header if type is eth
	 */
	if (create->offset_type == PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH &&
	    create->inner_ip_proto.ip_proto_offset < 14) {
		ppe_tun_rps_warn("ip_proto_offset %d falls within Ethernet header (< 14)\n",
				create->inner_ip_proto.ip_proto_offset);
		return PPE_TUN_RPS_RET_INVALID_UDF_OFFSET;
	}

	ppe_tun_rps_info("Inner IP protocol config: offset=%d, ipv4_val=0x%x, ipv4_mask=0x%x, ipv6_val=0x%x, ipv6_mask=0x%x\n",
			create->inner_ip_proto.ip_proto_offset,
			create->inner_ip_proto.ip_proto_ipv4_value,
			create->inner_ip_proto.ip_proto_ipv4_mask,
			create->inner_ip_proto.ip_proto_ipv6_value,
			create->inner_ip_proto.ip_proto_ipv6_mask);

	ppe_tun_rps_info("Inner IP protocol validation passed\n");
	return PPE_TUN_RPS_RET_SUCCESS;
}

/*
 * ppe_tun_rps_match_single_udf()
 *	Compare a single UDF match criteria for parity with disabled UDF bypass
 */
static inline bool ppe_tun_rps_match_single_udf(struct ppe_tun_rps_udf *create_udf, struct ppe_tun_rps_udf *destroy_udf)
{
	if (create_udf->enable != destroy_udf->enable) {
		return false;
	}

	if (!create_udf->enable) {
		/*
		 * Ignore uninitialized fields
		 */
		return true;
	}

	return (create_udf->offset == destroy_udf->offset &&
		create_udf->val == destroy_udf->val &&
		create_udf->mask == destroy_udf->mask);
}

/*
 * ppe_tun_rps_match_udf()
 *	Compare UDF match criteria for rule parity
 */
static inline bool ppe_tun_rps_match_udf(struct ppe_tun_rps_user_data *create_ud,
					 struct ppe_tun_rps_user_data *destroy_ud)
{
	return (ppe_tun_rps_match_single_udf(&create_ud->udf[0], &destroy_ud->udf[0]) &&
		ppe_tun_rps_match_single_udf(&create_ud->udf[1], &destroy_ud->udf[1]) &&
		ppe_tun_rps_match_single_udf(&create_ud->udf[2], &destroy_ud->udf[2]));
}

/*
 * ppe_tun_rps_match_ipv4()
 *	Check if IPv4 create and destroy rules match exactly (5-tuple + UDFs)
 */
static inline bool ppe_tun_rps_match_ipv4(struct ppe_tun_rps_ipv4_rule_create_msg *create,
					  struct ppe_tun_rps_ipv4_rule_destroy_msg *destroy)
{
	return (create->flow_ip == destroy->flow_ip &&
		create->return_ip == destroy->return_ip &&
		create->flow_ident == destroy->flow_ident &&
		create->return_ident == destroy->return_ident &&
		create->protocol == destroy->protocol &&
		ppe_tun_rps_match_udf(&create->usr_data, &destroy->usr_data));
}

/*
 * ppe_tun_rps_match_ipv6()
 *	Check if IPv6 create and destroy rules match exactly (5-tuple + UDFs)
 */
static inline bool ppe_tun_rps_match_ipv6(struct ppe_tun_rps_ipv6_rule_create_msg *create,
					  struct ppe_tun_rps_ipv6_rule_destroy_msg *destroy)
{
	return (create->flow_ip[0] == destroy->flow_ip[0] &&
		create->flow_ip[1] == destroy->flow_ip[1] &&
		create->flow_ip[2] == destroy->flow_ip[2] &&
		create->flow_ip[3] == destroy->flow_ip[3] &&
		create->return_ip[0] == destroy->return_ip[0] &&
		create->return_ip[1] == destroy->return_ip[1] &&
		create->return_ip[2] == destroy->return_ip[2] &&
		create->return_ip[3] == destroy->return_ip[3] &&
		create->flow_ident == destroy->flow_ident &&
		create->return_ident == destroy->return_ident &&
		create->protocol == destroy->protocol &&
		ppe_tun_rps_match_udf(&create->usr_data, &destroy->usr_data));
}

/*
 * ppe_tun_rps_ipv4_rule_create
 *      Tunnel RPS IPv4 rule create
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv4_rule_create(struct ppe_tun_rps_ipv4_rule_create_msg *create_ipv4)
{
	struct ppe_drv_tun_rps_rule_create prpsc = {0};
	ppe_drv_ret_t ret;
	struct ppe_tun_rps *g_rps = &gbl_tun_rps;
	enum ppe_tun_rps_ret validation_ret;

	if (!create_ipv4) {
		ppe_tun_rps_warn("Invalid parameter: create_ipv4 is NULL\n");
		return PPE_TUN_RPS_RET_FAILURE;
	}

	atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule);

	/*
	 * Populate driver structure first
	 */
	prpsc.cmn.flow_ip[0] = create_ipv4->flow_ip;
	prpsc.cmn.return_ip[0] = create_ipv4->return_ip;
	prpsc.cmn.flow_ident = create_ipv4->flow_ident;
	prpsc.cmn.return_ident = create_ipv4->return_ident;
	prpsc.cmn.protocol = create_ipv4->protocol;
	prpsc.header_len = create_ipv4->header_len;
	prpsc.cmn.rule_type = PPE_DRV_TUN_RPS_RULE_TYPE_IPV4;

	/*
	 * Map Enums
	 */
	switch (create_ipv4->header_len_type) {
		case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
			prpsc.offset_type = PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH;
			break;
		case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
			prpsc.offset_type = PPE_DRV_TUN_RPS_OFFSET_TYPE_IP;
			break;
		case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
			prpsc.offset_type = PPE_DRV_TUN_RPS_OFFSET_TYPE_UDP;
			break;
		default:
			ppe_tun_rps_warn("Invalid Tunnel RPS Header length type: %d\n", create_ipv4->header_len_type);
			atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule_fail);
			return PPE_TUN_RPS_RET_INVALID_HDR_TYPE;
	}

	switch (create_ipv4->inner_pkt_type) {
		case PPE_TUN_RPS_INNER_PKT_TYPE_ETH:
			prpsc.inner_type = PPE_DRV_TUN_RPS_INNER_PKT_TYPE_ETH;
			break;
		case PPE_TUN_RPS_INNER_PKT_TYPE_IP:
			prpsc.inner_type = PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP;
			break;
		default:
			ppe_tun_rps_warn("Invalid Tunnel RPS Inner packet type: %d\n", create_ipv4->inner_pkt_type);
			atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule_fail);
			return PPE_TUN_RPS_RET_INVALID_INNER_PKT_TYPE;
	}

	ppe_tun_rps_create_fill_udf_data(&create_ipv4->usr_data, &prpsc);
	ppe_tun_rps_create_fill_header_match(&create_ipv4->header_match, &prpsc, create_ipv4->header_len_type);

	/*
	 * Copy WAN interface name
	 */
	strlcpy(prpsc.wan_if, create_ipv4->wan_if, IFNAMSIZ);

	/*
	 * Copy inner IP protocol fields to ppe_drv structure
	 */
	memcpy(&prpsc.inner_ip_proto, &create_ipv4->inner_ip_proto, sizeof(prpsc.inner_ip_proto));

	/*
	 * Copy PPPoE fields
	 */
	prpsc.pppoe_en = create_ipv4->pppoe_en;
	prpsc.pppoe_session_id = create_ipv4->pppoe_session_id;
	memcpy(prpsc.pppoe_server_mac, create_ipv4->pppoe_server_mac, ETH_ALEN);

	/*
	 * Validate populated structure
	 */
	validation_ret = ppe_tun_rps_validate_create_rule(&prpsc);
	if (validation_ret != PPE_TUN_RPS_RET_SUCCESS) {
		ppe_tun_rps_warn("IPv4 rule validation failed: %d\n", validation_ret);
		atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule_fail);
		return validation_ret;
	}

	/*
	 * Validate inner IP protocol configuration
	 */
	validation_ret = ppe_tun_rps_validate_inner_ip_config(&prpsc);
	if (validation_ret != PPE_TUN_RPS_RET_SUCCESS) {
		ppe_tun_rps_warn("IPv4 inner IP config validation failed: %d\n", validation_ret);
		atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule_fail);
		return validation_ret;
	}

	ppe_tun_rps_rule_dump(&prpsc, true);

	/*
	 * Allocate rule entry
	 */
	struct ppe_tun_rps_rule_entry *entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry) {
		ppe_tun_rps_warn("IPv4 rule entry allocation failed\n");
		atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule_fail);
		return PPE_TUN_RPS_RET_FAILURE;
	}

	entry->ip_proto = IPPROTO_IP;
	memcpy(&entry->msg.v4, create_ipv4, sizeof(*create_ipv4));

	ret = ppe_drv_tun_rps_rule_create(&prpsc);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_tun_rps_warn("Error in pushing RPS rules with error code %d\n", ret);
		kfree(entry);
		atomic64_inc(&g_rps->stats.v4_create_ppe_tun_rps_rule_fail);
		return PPE_TUN_RPS_RET_FAILURE;
	}

	spin_lock_bh(&gbl_tun_rps.lock);
	list_add_tail(&entry->list, &gbl_tun_rps.rules_list);
	spin_unlock_bh(&gbl_tun_rps.lock);

	return PPE_TUN_RPS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_tun_rps_ipv4_rule_create);

/*
 * ppe_tun_rps_ipv6_rule_create()
 *	Tunnel RPS IPv6 rule create
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv6_rule_create(struct ppe_tun_rps_ipv6_rule_create_msg *create_ipv6)
{
	struct ppe_drv_tun_rps_rule_create prpsc = {0};
	ppe_drv_ret_t ret;
	struct ppe_tun_rps *g_rps = &gbl_tun_rps;
	enum ppe_tun_rps_ret validation_ret;

	if (!create_ipv6) {
		ppe_tun_rps_warn("Invalid parameter: create_ipv6 is NULL\n");
		return PPE_TUN_RPS_RET_FAILURE;
	}

	atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule);

	/*
	 * Populate driver structure first
	 */
	memcpy(prpsc.cmn.flow_ip, create_ipv6->flow_ip, sizeof(create_ipv6->flow_ip));
	memcpy(prpsc.cmn.return_ip, create_ipv6->return_ip, sizeof(create_ipv6->return_ip));
	prpsc.cmn.flow_ident = create_ipv6->flow_ident;
	prpsc.cmn.return_ident = create_ipv6->return_ident;
	prpsc.cmn.protocol = create_ipv6->protocol;
	prpsc.cmn.rule_type = PPE_DRV_TUN_RPS_RULE_TYPE_IPV6;
	prpsc.header_len = create_ipv6->header_len;

	switch (create_ipv6->header_len_type) {
		case PPE_TUN_RPS_HDR_LEN_TYPE_ETH:
			prpsc.offset_type = PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH;
			break;
		case PPE_TUN_RPS_HDR_LEN_TYPE_IP:
			prpsc.offset_type = PPE_DRV_TUN_RPS_OFFSET_TYPE_IP;
			break;
		case PPE_TUN_RPS_HDR_LEN_TYPE_UDP:
			prpsc.offset_type = PPE_DRV_TUN_RPS_OFFSET_TYPE_UDP;
			break;
		default:
			ppe_tun_rps_warn("Invalid Tunnel RPS Header length type: %d\n", create_ipv6->header_len_type);
			atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule_fail);
			return PPE_TUN_RPS_RET_INVALID_HDR_TYPE;
	}

	switch (create_ipv6->inner_pkt_type) {
		case PPE_TUN_RPS_INNER_PKT_TYPE_ETH:
			prpsc.inner_type = PPE_DRV_TUN_RPS_INNER_PKT_TYPE_ETH;
			break;
		case PPE_TUN_RPS_INNER_PKT_TYPE_IP:
			prpsc.inner_type = PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP;
			break;
		default:
			ppe_tun_rps_warn("Invalid Tunnel RPS Inner packet type: %d\n", create_ipv6->inner_pkt_type);
			atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule_fail);
			return PPE_TUN_RPS_RET_INVALID_INNER_PKT_TYPE;
	}

	ppe_tun_rps_create_fill_udf_data(&create_ipv6->usr_data, &prpsc);
	ppe_tun_rps_create_fill_header_match(&create_ipv6->header_match, &prpsc, create_ipv6->header_len_type);

	/*
	 * Copy WAN interface name
	 */
	strlcpy(prpsc.wan_if, create_ipv6->wan_if, IFNAMSIZ);

	/*
	 * Copy inner IP protocol fields to ppe_drv structure
	 */
	memcpy(&prpsc.inner_ip_proto, &create_ipv6->inner_ip_proto, sizeof(prpsc.inner_ip_proto));

	/*
	 * Copy PPPoE fields
	 */
	prpsc.pppoe_en = create_ipv6->pppoe_en;
	prpsc.pppoe_session_id = create_ipv6->pppoe_session_id;
	memcpy(prpsc.pppoe_server_mac, create_ipv6->pppoe_server_mac, ETH_ALEN);

	/*
	 * Validate populated structure
	 */
	validation_ret = ppe_tun_rps_validate_create_rule(&prpsc);
	if (validation_ret != PPE_TUN_RPS_RET_SUCCESS) {
		ppe_tun_rps_warn("IPv6 rule validation failed: %d\n", validation_ret);
		atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule_fail);
		return validation_ret;
	}

	/*
	 * Validate inner IP protocol configuration
	 */
	validation_ret = ppe_tun_rps_validate_inner_ip_config(&prpsc);
	if (validation_ret != PPE_TUN_RPS_RET_SUCCESS) {
		ppe_tun_rps_warn("IPv6 inner IP config validation failed: %d\n", validation_ret);
		atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule_fail);
		return validation_ret;
	}

	ppe_tun_rps_rule_dump(&prpsc, true);

	/*
	 * Allocate rule entry
	 */
	struct ppe_tun_rps_rule_entry *entry = kzalloc(sizeof(*entry), GFP_ATOMIC);
	if (!entry) {
		ppe_tun_rps_warn("IPv6 rule entry allocation failed\n");
		atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule_fail);
		return PPE_TUN_RPS_RET_FAILURE;
	}

	entry->ip_proto = IPPROTO_IPV6;
	memcpy(&entry->msg.v6, create_ipv6, sizeof(*create_ipv6));

	ret = ppe_drv_tun_rps_rule_create(&prpsc);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_tun_rps_warn("Error in pushing RPS rules with error code %d\n", ret);
		kfree(entry);
		atomic64_inc(&g_rps->stats.v6_create_ppe_tun_rps_rule_fail);
		return PPE_TUN_RPS_RET_FAILURE;
	}

	spin_lock_bh(&gbl_tun_rps.lock);
	list_add_tail(&entry->list, &gbl_tun_rps.rules_list);
	spin_unlock_bh(&gbl_tun_rps.lock);

	return PPE_TUN_RPS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_tun_rps_ipv6_rule_create);

/*
 * ppe_tun_rps_ipv4_rule_destroy()
 *	Tunnel RPS IPv4 rule destroy
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv4_rule_destroy(struct ppe_tun_rps_ipv4_rule_destroy_msg *destroy_ipv4)
{
	struct ppe_drv_tun_rps_rule_destroy prpsd = {0};
	ppe_drv_ret_t ret;
	struct ppe_tun_rps *g_rps = &gbl_tun_rps;

	if (!destroy_ipv4) {
		ppe_tun_rps_warn("Invalid parameter: destroy_ipv4 is NULL\n");
		return PPE_TUN_RPS_RET_FAILURE;
	}

	atomic64_inc(&g_rps->stats.v4_destroy_ppe_tun_rps_rule);

	/*
	 * Copy IPV4 5-tuple information
	 */
	prpsd.cmn.flow_ip[0] = destroy_ipv4->flow_ip;
	prpsd.cmn.return_ip[0] = destroy_ipv4->return_ip;
	prpsd.cmn.flow_ident = destroy_ipv4->flow_ident;
	prpsd.cmn.return_ident = destroy_ipv4->return_ident;
	prpsd.cmn.protocol = destroy_ipv4->protocol;
	prpsd.cmn.rule_type = PPE_DRV_TUN_RPS_RULE_TYPE_IPV4;

	ppe_tun_rps_destroy_fill_udf_data(&destroy_ipv4->usr_data, &prpsd);

	ppe_tun_rps_rule_dump(&prpsd, false);

	ret = ppe_drv_tun_rps_rule_destroy(&prpsd);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_tun_rps_warn("Error destroying RPS rules with error code %d\n", ret);
		atomic64_inc(&g_rps->stats.v4_destroy_ppe_tun_rps_rule_fail);
		return PPE_TUN_RPS_RET_FAILURE;
	}

	/*
	 * Remove entry from rules list
	 */
	struct ppe_tun_rps_rule_entry *entry, *temp;
	struct ppe_tun_rps_rule_entry *deleted_entry = NULL;
	spin_lock_bh(&g_rps->lock);
	list_for_each_entry_safe(entry, temp, &g_rps->rules_list, list) {
		if (entry->ip_proto == IPPROTO_IP && ppe_tun_rps_match_ipv4(&entry->msg.v4, destroy_ipv4)) {
			list_del(&entry->list);
			deleted_entry = entry;
			break;
		}
	}
	spin_unlock_bh(&g_rps->lock);

	if (deleted_entry) {
		kfree(deleted_entry);
	}

	return PPE_TUN_RPS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_tun_rps_ipv4_rule_destroy);

/*
 * ppe_tun_rps_ipv6_rule_destroy()
 *	Tunnel RPS IPv6 rule destroy
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv6_rule_destroy(struct ppe_tun_rps_ipv6_rule_destroy_msg *destroy_ipv6)
{
	struct ppe_drv_tun_rps_rule_destroy prpsd = {0};
	ppe_drv_ret_t ret;
	struct ppe_tun_rps *g_rps = &gbl_tun_rps;

	if (!destroy_ipv6) {
		ppe_tun_rps_warn("Invalid parameter: destroy_ipv6 is NULL\n");
		return PPE_TUN_RPS_RET_FAILURE;
	}

	atomic64_inc(&g_rps->stats.v6_destroy_ppe_tun_rps_rule);

	/*
	 * Copy 5-tuple information
	 */
	memcpy(prpsd.cmn.flow_ip, destroy_ipv6->flow_ip, sizeof(destroy_ipv6->flow_ip));
	memcpy(prpsd.cmn.return_ip, destroy_ipv6->return_ip, sizeof(destroy_ipv6->return_ip));
	prpsd.cmn.flow_ident = destroy_ipv6->flow_ident;
	prpsd.cmn.return_ident = destroy_ipv6->return_ident;
	prpsd.cmn.protocol = destroy_ipv6->protocol;
	prpsd.cmn.rule_type = PPE_DRV_TUN_RPS_RULE_TYPE_IPV6;

	ppe_tun_rps_destroy_fill_udf_data(&destroy_ipv6->usr_data, &prpsd);

	ppe_tun_rps_rule_dump(&prpsd, false);

	ret = ppe_drv_tun_rps_rule_destroy(&prpsd);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_tun_rps_warn("Error destroying RPS rules with error code %d\n", ret);
		atomic64_inc(&g_rps->stats.v6_destroy_ppe_tun_rps_rule_fail);
		return PPE_TUN_RPS_RET_FAILURE;
	}

	/*
	 * Remove entry from rules list
	 */
	struct ppe_tun_rps_rule_entry *entry, *temp;
	struct ppe_tun_rps_rule_entry *deleted_entry = NULL;
	spin_lock_bh(&g_rps->lock);
	list_for_each_entry_safe(entry, temp, &g_rps->rules_list, list) {
		if (entry->ip_proto == IPPROTO_IPV6 && ppe_tun_rps_match_ipv6(&entry->msg.v6, destroy_ipv6)) {
			list_del(&entry->list);
			deleted_entry = entry;
			break;
		}
	}
	spin_unlock_bh(&g_rps->lock);

	if (deleted_entry) {
		kfree(deleted_entry);
	}

	return PPE_TUN_RPS_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_tun_rps_ipv6_rule_destroy);

/*
 * ppe_tun_rps_init()
 *	Initialize tunnel RPS module.
 */
int ppe_tun_rps_init(struct dentry *root)
{
	int ret;

	spin_lock_init(&gbl_tun_rps.lock);
	INIT_LIST_HEAD(&gbl_tun_rps.rules_list);

	atomic64_set(&gbl_tun_rps.stats.v4_create_ppe_tun_rps_rule, 0);
	atomic64_set(&gbl_tun_rps.stats.v4_create_ppe_tun_rps_rule_fail, 0);
	atomic64_set(&gbl_tun_rps.stats.v4_destroy_ppe_tun_rps_rule, 0);
	atomic64_set(&gbl_tun_rps.stats.v4_destroy_ppe_tun_rps_rule_fail, 0);
	atomic64_set(&gbl_tun_rps.stats.v6_create_ppe_tun_rps_rule, 0);
	atomic64_set(&gbl_tun_rps.stats.v6_create_ppe_tun_rps_rule_fail, 0);
	atomic64_set(&gbl_tun_rps.stats.v6_destroy_ppe_tun_rps_rule, 0);
	atomic64_set(&gbl_tun_rps.stats.v6_destroy_ppe_tun_rps_rule_fail, 0);

	ret = ppe_tun_rps_stats_debugfs_init(root);
	if (ret < 0) {
		ppe_tun_rps_warn("Failed to initialize debugfs for ppe_tun_rps\n");
		return ret;
	}

	ppe_tun_rps_info("PPE Tunnel RPS module initialized\n");
	return 0;
}

/*
 * ppe_tun_rps_deinit()
 *	De-Initialize tunnel RPS module.
 */
void ppe_tun_rps_deinit(void)
{
	struct ppe_tun_rps_rule_entry *entry, *temp;
	struct list_head free_list;

	INIT_LIST_HEAD(&free_list);

	ppe_tun_rps_stats_debugfs_exit();

	/*
	 * Move all active rule entries to free_list under lock
	 */
	spin_lock_bh(&gbl_tun_rps.lock);
	list_splice_init(&gbl_tun_rps.rules_list, &free_list);
	spin_unlock_bh(&gbl_tun_rps.lock);

	/*
	 * Free all entries safely outside of the spinlock
	 */
	list_for_each_entry_safe(entry, temp, &free_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}

	ppe_tun_rps_info("PPE Tunnel RPS module exited\n");
}

/*
 * ppe_tun_rps_flush_rules()
 *	Flush all configured rules from hardware and tracked memory list (safe pop-and-destroy)
 */
void ppe_tun_rps_flush_rules(void)
{
	struct ppe_tun_rps *g_rps = &gbl_tun_rps;

	ppe_tun_rps_info("Flushing all active tunnel RPS rules\n");

	while (1) {
		struct ppe_tun_rps_rule_entry *entry = NULL;

		spin_lock_bh(&g_rps->lock);
		if (!list_empty(&g_rps->rules_list)) {
			entry = list_first_entry(&g_rps->rules_list, struct ppe_tun_rps_rule_entry, list);
			list_del(&entry->list);
		}
		spin_unlock_bh(&g_rps->lock);

		if (!entry) {
			/*
			 * No more rules left to flush
			 */
			break;
		}

		if (entry->ip_proto != IPPROTO_IPV6) {
			struct ppe_drv_tun_rps_rule_destroy prpsd = {0};
			prpsd.cmn.flow_ip[0] = entry->msg.v4.flow_ip;
			prpsd.cmn.return_ip[0] = entry->msg.v4.return_ip;
			prpsd.cmn.flow_ident = entry->msg.v4.flow_ident;
			prpsd.cmn.return_ident = entry->msg.v4.return_ident;
			prpsd.cmn.protocol = entry->msg.v4.protocol;
			prpsd.cmn.rule_type = PPE_DRV_TUN_RPS_RULE_TYPE_IPV4;
			ppe_tun_rps_destroy_fill_udf_data(&entry->msg.v4.usr_data, &prpsd);

			ppe_drv_tun_rps_rule_destroy(&prpsd);
		} else {
			struct ppe_drv_tun_rps_rule_destroy prpsd = {0};
			memcpy(prpsd.cmn.flow_ip, entry->msg.v6.flow_ip, sizeof(entry->msg.v6.flow_ip));
			memcpy(prpsd.cmn.return_ip, entry->msg.v6.return_ip, sizeof(entry->msg.v6.return_ip));
			prpsd.cmn.flow_ident = entry->msg.v6.flow_ident;
			prpsd.cmn.return_ident = entry->msg.v6.return_ident;
			prpsd.cmn.protocol = entry->msg.v6.protocol;
			prpsd.cmn.rule_type = PPE_DRV_TUN_RPS_RULE_TYPE_IPV6;
			ppe_tun_rps_destroy_fill_udf_data(&entry->msg.v6.usr_data, &prpsd);

			ppe_drv_tun_rps_rule_destroy(&prpsd);
		}

		kfree(entry);
	}
}
