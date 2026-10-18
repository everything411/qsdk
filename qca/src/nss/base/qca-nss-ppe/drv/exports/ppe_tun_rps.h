/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_tun_rps.h
 *	NSS PPE Tunnel RPS definitions.
 */

#ifndef _PPE_TUN_RPS_H_
#define _PPE_TUN_RPS_H_
#include <linux/types.h>
#include <linux/if.h>

/**
 * ppe_tun_rps_ret
 *	Return values for PPE tunnel RPS operations
 */
enum ppe_tun_rps_ret {
        PPE_TUN_RPS_RET_SUCCESS = 0,   /**< operation succeeded */
        PPE_TUN_RPS_RET_FAILURE = 1,   /**< operation failed */
        PPE_TUN_RPS_RET_INVALID_HDR_TYPE = 2,   /**< Invalid Header length */
        PPE_TUN_RPS_RET_INVALID_INNER_PKT_TYPE = 3,   /**< Invalid Inner packet type */
        PPE_TUN_RPS_RET_INVALID_HDR_LEN = 4,   /**< Header length exceeds maximum */
        PPE_TUN_RPS_RET_INVALID_UDF_OFFSET = 5,   /**< UDF offset exceeds maximum */
        PPE_TUN_RPS_RET_INVALID_UDF_MASK = 6,   /**< UDF mask is zero when enabled */
        PPE_TUN_RPS_RET_NO_MATCH_CRITERIA = 7,   /**< No matching criteria specified */
        PPE_TUN_RPS_RET_INVALID_INNER_IP_CONFIG = 8,   /**< Invalid inner IP protocol configuration */
        PPE_TUN_RPS_RET_INVALID_UDF3_WITH_INNER_IP = 9,   /**< UDF3 cannot be enabled with inner IP type */
        PPE_TUN_RPS_RET_INVALID_WAN_IF = 10,   /**< Invalid or empty WAN interface name */
};

/**
 * ppe_tun_rps_hdr_len_type
 * 	Header length offset type for RPS rule
 */
enum ppe_tun_rps_hdr_len_type {
	PPE_TUN_RPS_HDR_LEN_TYPE_ETH = 0,	/**< header length offset type from Start of Ethernet header*/
	PPE_TUN_RPS_HDR_LEN_TYPE_IP,		/**< header length offset type from Start of IP header */
	PPE_TUN_RPS_HDR_LEN_TYPE_UDP,		/**< header length offset type from Start of UDP header */
};

/*
 * ppe_tun_rps_inner_pkt_type
 * 	Inner packet type for RPS rule
 */
enum ppe_tun_rps_inner_pkt_type {
	PPE_TUN_RPS_INNER_PKT_TYPE_ETH = 0,	/**< Inner packet type Ethernet */
	PPE_TUN_RPS_INNER_PKT_TYPE_IP,	/**< Inner packet type IP */
};

/**
 * ppe_tun_rps_udf
 *	User Defined Field structure for tunnel identification
 */
struct ppe_tun_rps_udf {
	u8 enable;		/**< Enable flag for this UDF */
	u8 offset;		/**< Offset in bytes from offset_type defined */
	u16 val;		/**< Value to match */
	u16 mask;		/**< Value mask */
};

/**
 * ppe_tun_rps_user_data
 *      Tunnel RPS user data to be matched
 *      (Up to 3 16-bit fields can be matched along with 5-tuple to identify a tunnel)
 */
struct ppe_tun_rps_user_data {
	struct ppe_tun_rps_udf udf[3];	/**< Array of up to 3 user defined fields */
};

/*
 * ppe_tun_rps_inner_ip_proto
 *	Inner IP protocol matching structure (used when inner_pkt_type is IP)
 *	When inner packet type is IP, user_data3 is reserved internally for IP protocol matching
 */
struct ppe_tun_rps_inner_ip_proto {
	u16 ip_proto_offset;		/**< Offset to IP protocol field */
	u16 ip_proto_ipv4_value;	/**< IPv4 protocol value to match */
	u16 ip_proto_ipv4_mask;		/**< IPv4 protocol mask */
	u16 ip_proto_ipv6_value;	/**< IPv6 protocol value to match */
	u16 ip_proto_ipv6_mask;		/**< IPv6 protocol mask */
};

/**
 * ppe_tun_rps_l2_match
 *	Layer 2 (Ethernet) header matching structure
 */
struct ppe_tun_rps_l2_match {
	u16 eth_protocol;		/**< Ethernet protocol field */
	u16 eth_protocol_mask;		/**< Ethernet protocol mask */
};

/**
 * ppe_tun_rps_l3_match
 *	Layer 3 (IP) header matching structure
 */
struct ppe_tun_rps_l3_match {
	u8 ip_protocol;			/**< IP protocol field */
	u8 ip_protocol_mask;		/**< IP protocol mask */
};

/**
 * ppe_tun_rps_l4_match
 *	Layer 4 (UDP) header matching structure
 */
struct ppe_tun_rps_l4_match {
	u16 udp_sport;			/**< UDP source port */
	u16 udp_dport;			/**< UDP destination port */
};

/**
 * ppe_tun_rps_header_match
 *	Union of header-specific matching structures
 */
union ppe_tun_rps_header_match {
	struct ppe_tun_rps_l2_match l2_match;	/**< Layer 2 (Ethernet) header matching */
	struct ppe_tun_rps_l3_match l3_match;	/**< Layer 3 (IP) header matching */
	struct ppe_tun_rps_l4_match l4_match;	/**< Layer 4 (UDP) header matching */
};

/*
 * ppe_tun_rps_ipv4_rule_create_msg
 *	Tunnel RPS IPv4 create rule message structure
 */
struct ppe_tun_rps_ipv4_rule_create_msg {
	__be32 flow_ip;		/**< Flow IP address. */
	__be32 return_ip;	/**< Return IP address. */
	__be16 flow_ident;	/**< Flow identifier, e.g.,TCP/UDP port. */
	__be16 return_ident;	/**< Return identifier, e.g., TCP/UDP port. */
	u8 protocol;		/**< Protocol */
	u8 header_len;		/**< Tunnel outer header length */
	enum ppe_tun_rps_hdr_len_type header_len_type;	/**< 0 - header length from Start outer L2(Eth) header
				     1 - header length from Start of outer L3(IP) header
				     2 - header length from Start outer UDP header */
	enum ppe_tun_rps_inner_pkt_type inner_pkt_type;	/**< 0 - Tunnel inner packet type is Ethernet
								     1 - Tunnel inner packet type is IP */
	struct ppe_tun_rps_user_data usr_data;		/**< Additional User data fields to match */
	struct ppe_tun_rps_inner_ip_proto inner_ip_proto;	/**< Inner IP protocol matching (mandatory when inner_pkt_type is IP) */
	union ppe_tun_rps_header_match header_match;	/**< Header-specific matching fields */
	char wan_if[IFNAMSIZ];				/**< WAN interface name */
	uint16_t pppoe_session_id;			/**< PPPoE session ID */
	uint8_t pppoe_server_mac[6];			/**< PPPoE server MAC */
	uint8_t pppoe_en;				/**< PPPoE session enable */
};

/*
 * ppe_tun_rps_ipv6_rule_create_msg
 *      Tunnel RPS IPv6 create rule message structure
 */
struct ppe_tun_rps_ipv6_rule_create_msg {
	__be32 flow_ip[4];	/**< Flow IP address. */
	__be32 return_ip[4];	/**< Return IP address. */
	__be16 flow_ident;	/**< Flow identifier, e.g.,TCP/UDP port. */
	__be16 return_ident;	/**< Return identifier, e.g., TCP/UDP port. */
	u8 protocol;		/**< Protocol */
	u8 header_len;		/**< Tunnel outer header length */
	enum ppe_tun_rps_hdr_len_type header_len_type;	/**< 0 - header length from Start outer L2(Eth) header
				     1 - header length from Start of outer L3(IP) header
				     2 - header length from Start outer UDP header */
	enum ppe_tun_rps_inner_pkt_type inner_pkt_type;	/**< 0 - Tunnel inner packet type is Ethernet
								     1 - Tunnel inner packet type is IP */
	struct ppe_tun_rps_user_data usr_data;		/**< Additional User data fields to match */
	struct ppe_tun_rps_inner_ip_proto inner_ip_proto;	/**< Inner IP protocol matching (mandatory when inner_pkt_type is IP) */
	union ppe_tun_rps_header_match header_match;	/**< Header-specific matching fields */
	char wan_if[IFNAMSIZ];				/**< WAN interface name */
	uint16_t pppoe_session_id;			/**< PPPoE session ID */
	uint8_t pppoe_server_mac[6];			/**< PPPoE server MAC */
	uint8_t pppoe_en;				/**< PPPoE session enable */
};

/*
 * ppe_tun_rps_ipv4_rule_destroy_msg
 *      Tunnel RPS IPv4 delete rule message structure
 */
struct ppe_tun_rps_ipv4_rule_destroy_msg {
	__be32 flow_ip;		/**< Flow IP address. */
	__be32 return_ip;	/**< Return IP address. */
	__be16 flow_ident;	/**< Flow identifier, e.g.,TCP/UDP port. */
	__be16 return_ident;	/**< Return identifier, e.g., TCP/UDP port. */
	u8 protocol;		/**< Protocol*/
	struct ppe_tun_rps_user_data usr_data;		/**< Additional User data fields to match */
};

/*
 * ppe_tun_rps_ipv6_rule_destroy_msg
 *      Tunnel RPS IPv6 delete rule message structure
 */
struct ppe_tun_rps_ipv6_rule_destroy_msg {
	__be32 flow_ip[4];	/**< Flow IP address. */
	__be32 return_ip[4];	/**< Return IP address. */
	__be16 flow_ident;	/**< Flow identifier, e.g.,TCP/UDP port. */
	__be16 return_ident;	/**< Return identifier, e.g., TCP/UDP port. */
	u8 protocol;		/**< Protocol*/
	struct ppe_tun_rps_user_data usr_data;		/**< Additional User data fields to match */
};

/**
 * ppe_tun_rps_ipv4_rule_create
 *	Push PPE IPV4 tunnel RPS rules to PPE.
 *
 * @datatype
 * ppe_tun_rps_ipv4_rule_create_msg
 *
 * @param[in] msg   Pointer to the PPE IPv4 tunnel RPS rule create message.
 *
 * @return
 * status of ipv4 rule create
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv4_rule_create(struct ppe_tun_rps_ipv4_rule_create_msg *msg);

/**
 * ppe_tun_rps_ipv4_rule_destroy
 *	Destroy PPE IPV4 tunnel RPS rules pushed to PPE.
 *
 * @datatype
 * ppe_tun_rps_ipv4_rule_destroy_msg
 *
 * @param[in] msg   Pointer to the PPE IPv4 tunnel RPS rule destroy message.
 *
 * @return
 * status of ipv4 rule destroy
 */
 enum ppe_tun_rps_ret ppe_tun_rps_ipv4_rule_destroy(struct ppe_tun_rps_ipv4_rule_destroy_msg *msg);

/**
 * ppe_tun_rps_ipv6_rule_create
 *	Push PPE IPv6 tunnel RPS rules to PPE.
 *
 * @datatype
 * ppe_tun_rps_ipv6_rule_create_msg
 *
 * @param[in] msg   Pointer to the IPv6 PPE tunnel RPS rule create message.
 *
 * @return
 * status of ipv6 rule create
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv6_rule_create(struct ppe_tun_rps_ipv6_rule_create_msg *msg);

/**
 * ppe_tun_rps_ipv6_rule_destroy
 *	Destroy IPv6 PPE tunnel RPS rules pushed to PPE.
 *
 * @datatype
 * ppe_tun_rps_ipv6_rule_destroy_msg
 *
 * @param[in] msg   Pointer to the IPv6 tunnel RPS rule destroy message.
 *
 * @return
 * status of ipv6 rule destroy
 */
enum ppe_tun_rps_ret ppe_tun_rps_ipv6_rule_destroy(struct ppe_tun_rps_ipv6_rule_destroy_msg *msg);
#endif /* _PPE_TUN_RPS_H_ */
