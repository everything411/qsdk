/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#ifndef _PPE_DRV_TUN_RPS_H_
#define _PPE_DRV_TUN_RPS_H_

/*
 * ppe_drv_tun_rps_rule_type
 *	RPS rule type enumeration
 */
enum ppe_drv_tun_rps_rule_type {
	PPE_DRV_TUN_RPS_RULE_TYPE_IPV4 = 0,	/* IPv4 rule type */
	PPE_DRV_TUN_RPS_RULE_TYPE_IPV6 = 1,	/* IPv6 rule type */
};

#define PPE_DRV_TUN_TPR_IPV6_ADDR_WORDS 4
#define PPE_DRV_TUN_TPR_IPV4_ADDR_INDEX 0
#define PPE_DRV_TUN_TPR_UDF0_INDEX 0
#define PPE_DRV_TUN_TPR_UDF1_INDEX 1
#define PPE_DRV_TUN_TPR_UDF2_INDEX 2

/*
 * Utility macro for bit operations
 */
#define PPE_DRV_TUN_TPR_SET_BIT(bit) (1U << (bit))

/*
 * ppe_drv_tun_rps_offset_type
 * 	Offset type for RPS rule
 */
enum ppe_drv_tun_rps_offset_type {
	PPE_DRV_TUN_RPS_OFFSET_TYPE_ETH,	/* Offset from start of L2 (Ethernet) header */
	PPE_DRV_TUN_RPS_OFFSET_TYPE_IP,		/* Offset from start of L3 (IP) header */
	PPE_DRV_TUN_RPS_OFFSET_TYPE_UDP,	/* Offset from start of L4 (UDP) header */
};

/*
 * ppe_drv_tun_rps_inner_pkt_type
 * 	Inner packet type for RPS rule
 */
enum ppe_drv_tun_rps_inner_pkt_type {
	PPE_DRV_TUN_RPS_INNER_PKT_TYPE_ETH = 0,	/* Inner packet type Ethernet */
	PPE_DRV_TUN_RPS_INNER_PKT_TYPE_IP,	/* Inner packet type IP */
};

/*
 * ppe_drv_tun_rps_l2_match
 *	Ethernet header matching structure
 */
struct ppe_drv_tun_rps_l2_match {
	uint16_t eth_protocol;		/**< Ethernet protocol field */
	uint16_t eth_protocol_mask;	/**< Ethernet protocol mask */
};

/*
 * ppe_drv_tun_rps_l3_match
 *	IP header matching structure
 */
struct ppe_drv_tun_rps_l3_match {
	uint8_t ip_protocol;		/**< IP protocol field */
	uint8_t ip_protocol_mask;	/**< IP protocol mask */
};

/*
 * ppe_drv_tun_rps_l4_match
 *	UDP header matching structure
 */
struct ppe_drv_tun_rps_l4_match {
	uint16_t udp_sport;		/**< UDP source port */
	uint16_t udp_dport;		/**< UDP destination port */
};

/*
 * ppe_drv_tun_rps_header_match
 *	Union of header-specific matching structures
 */
union ppe_drv_tun_rps_header_match {
	struct ppe_drv_tun_rps_l2_match l2_match;	/**< Ethernet header matching */
	struct ppe_drv_tun_rps_l3_match l3_match;	/**< IP header matching */
	struct ppe_drv_tun_rps_l4_match l4_match;	/**< UDP header matching */
};

/*
 * ppe_drv_tun_rps_inner_ip_proto
 *	Inner IP protocol matching structure for inner packet type IP
 */
struct ppe_drv_tun_rps_inner_ip_proto {
	uint16_t ip_proto_offset;	/**< Offset to IP protocol field */
	uint16_t ip_proto_ipv4_value;	/**< IPv4 protocol value */
	uint16_t ip_proto_ipv4_mask;	/**< IPv4 protocol mask */
	uint16_t ip_proto_ipv6_value;	/**< IPv6 protocol value */
	uint16_t ip_proto_ipv6_mask;	/**< IPv6 protocol mask */
};

/*
 * ppe_drv_tun_rps_rule_common
 *	Common fields for RPS rule create and destroy operations
 */
struct ppe_drv_tun_rps_rule_common {
	uint32_t flow_ip[4];		/**< Flow IP address (index 0 for IPv4, all 4 for IPv6) */
	uint32_t flow_ident;		/**< Flow identifier (e.g., TCP or UDP port) */
	uint32_t return_ip[4];		/**< Return IP address (index 0 for IPv4, all 4 for IPv6) */
	uint32_t return_ident;		/**< Return identifier (e.g., TCP or UDP port) */
	uint16_t udf0_val;		/**< UDF0 expected value for matching */
	uint16_t udf1_val;		/**< UDF1 expected value for matching */
	uint16_t udf2_val;		/**< UDF2 expected value for matching */
	uint16_t udf0_mask;		/**< UDF0 value mask */
	uint16_t udf1_mask;		/**< UDF1 value mask */
	uint16_t udf2_mask;		/**< UDF2 value mask */
	uint8_t rule_type;		/**< IP version: 0 - IPv4, 1 - IPv6 */
	uint8_t protocol;		/**< IP protocol number (TCP=6, UDP=17, etc.) */
	uint8_t udf0_en;		/**< Enable UDF0 for tunnel type identification */
	uint8_t udf1_en;		/**< Enable UDF1 for tunnel type identification */
	uint8_t udf2_en;		/**< Enable UDF2 for tunnel type identification */
	uint8_t udf0_offset;		/**< UDF0 offset in bytes from offset_type base */
	uint8_t udf1_offset;		/**< UDF1 offset in bytes from offset_type base */
	uint8_t udf2_offset;		/**< UDF2 offset in bytes from offset_type base */
	union ppe_drv_tun_rps_header_match header_match;	/**< Header-specific matching fields */
};

/*
 * ppe_drv_tun_rps_rule_create
 *	Structure for creating RPS rules for tunnel traffic
 */
struct ppe_drv_tun_rps_rule_create {
	struct ppe_drv_tun_rps_rule_common cmn;		/**< Common rule fields */
	enum ppe_drv_tun_rps_offset_type offset_type;	/**< Base offset type for UDF calculations */
	enum ppe_drv_tun_rps_inner_pkt_type inner_type;	/**< Inner packet type Ethernet/IP */
	uint16_t pppoe_session_id;			/**< PPPoE session ID */
	uint8_t header_len;				/**< Tunnel outer header length in bytes */
	uint8_t pppoe_en;				/**< PPPoE session enable */
	uint8_t pppoe_server_mac[ETH_ALEN];		/**< PPPoE server mac */
	char wan_if[IFNAMSIZ];				/**< WAN interface name */
	struct ppe_drv_tun_rps_inner_ip_proto inner_ip_proto;	/**< Inner IP protocol matching fields */
};

/*
 * ppe_drv_tun_rps_rule_destroy
 *	RPS rule destroy structure
 */
struct ppe_drv_tun_rps_rule_destroy {
	struct ppe_drv_tun_rps_rule_common cmn;		/**< Common rule fields */
};

/**
 * ppe_drv_tun_rps_rule_create
 *	Create PPE Tunnel RPS rule.
 *
 * @datatype
 * ppe_drv_tun_rps_rule_create
 *
 * @param[in] rule_create pointer to Rule create structure.
 *
 * @return
 * status of tunnel RPS rule create
 */
ppe_drv_ret_t ppe_drv_tun_rps_rule_create(struct ppe_drv_tun_rps_rule_create *rule_create);

/**
 * ppe_drv_tun_rps_rule_destroy
 *	Destroy PPE Tunnel RPS rule.
 *
 * @datatype
 * ppe_drv_tun_rps_rule_destroy
 *
 * @param[in] rule_destroy pointer to Rule destroy structure.
 *
 * @return
 * status of tunnel RPS rule destroy
 */
ppe_drv_ret_t ppe_drv_tun_rps_rule_destroy(struct ppe_drv_tun_rps_rule_destroy *rule_destroy);
#endif /* _PPE_DRV_TUN_RPS_H_ */
