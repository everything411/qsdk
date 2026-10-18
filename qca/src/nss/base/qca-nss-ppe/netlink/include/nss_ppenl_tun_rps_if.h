/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_tun_rps_if.h
 *	NSS PPE Netlink Tunnel RPS
 */
#ifndef __NSS_PPENL_TUN_RPS_IF_H
#define __NSS_PPENL_TUN_RPS_IF_H

/**
 * Tunnel RPS Configure Family
 */
#define NSS_PPENL_TUN_RPS_FAMILY "nss_ppenl_rps"

/*
 * TODO: Restrict export of this file to kernel space.
 */

/*
 * nss_ppenl_tun_rps_user_data_val
 *	Individual user data field structure
 */
struct nss_ppenl_tun_rps_user_data_val {
	uint16_t data_val;		/**< Data value */
	uint16_t data_mask;		/**< Data mask */
	uint8_t data_offset;		/**< Data offset */
	uint8_t data_en;		/**< Data enable */
};

/*
 * nss_ppenl_tun_rps_usr_data
 *	Tunnel RPS user data match structure
 */
struct nss_ppenl_tun_rps_usr_data {
	struct nss_ppenl_tun_rps_user_data_val udf[3];	/**< User data fields array */
};

/*
 * nss_ppenl_tun_rps_l2_match
 *	Layer 2 (Ethernet) header matching structure
 */
struct nss_ppenl_tun_rps_l2_match {
	uint16_t eth_protocol;		/**< Ethernet protocol field */
	uint16_t eth_protocol_mask;	/**< Ethernet protocol mask */
};

/*
 * nss_ppenl_tun_rps_l3_match
 *	Layer 3 (IP) header matching structure
 */
struct nss_ppenl_tun_rps_l3_match {
	uint8_t ip_protocol;		/**< IP protocol field */
	uint8_t ip_protocol_mask;	/**< IP protocol mask */
};

/*
 * nss_ppenl_tun_rps_l4_match
 *	Layer 4 (UDP) header matching structure
 */
struct nss_ppenl_tun_rps_l4_match {
	uint16_t udp_sport;		/**< UDP source port */
	uint16_t udp_dport;		/**< UDP destination port */
};

/*
 * nss_ppenl_tun_rps_header_match
 *	Union of header-specific matching structures
 */
union nss_ppenl_tun_rps_header_match {
	struct nss_ppenl_tun_rps_l2_match l2_match;	/**< Layer 2 (Ethernet) header matching */
	struct nss_ppenl_tun_rps_l3_match l3_match;	/**< Layer 3 (IP) header matching */
	struct nss_ppenl_tun_rps_l4_match l4_match;	/**< Layer 4 (UDP) header matching */
};

/*
 * nss_ppenl_tun_rps_inner_ip_proto
 *	Inner IP protocol matching structure for inner packet type IP
 */
struct nss_ppenl_tun_rps_inner_ip_proto {
	uint16_t ip_proto_offset;		/**< Offset to IP protocol field */
	uint16_t ip_proto_ipv4_value;		/**< IPv4 protocol value */
	uint16_t ip_proto_ipv4_mask;		/**< IPv4 protocol mask */
	uint16_t ip_proto_ipv6_value;		/**< IPv6 protocol value */
	uint16_t ip_proto_ipv6_mask;		/**< IPv6 protocol mask */
};

/**
 * Tunnel RPS rule structure for netlink communication
 * Fields are ordered for optimal memory alignment to minimize padding
 */
struct nss_ppe_nl_tun_rps {
	union {
		uint32_t ipv4_sip;			/**< IPv4 source IP */
		uint32_t ipv6_sip[4];			/**< IPv6 source IP */
	};
	union {
		uint32_t ipv4_dip;			/**< IPv4 dest IP */
		uint32_t ipv6_dip[4];			/**< IPv6 dest IP */
	};
	int ret;					/**< Return status */
	struct nss_ppenl_tun_rps_usr_data usr_data;	/**< User data fields */
	char wan_if[IFNAMSIZ];				/**< WAN interface name */
	struct nss_ppenl_tun_rps_inner_ip_proto inner_ip_proto;	/**< Inner IP protocol matching fields */
	union nss_ppenl_tun_rps_header_match header_match;	/**< Header-specific matching fields */
	uint16_t sport;					/**< Source port */
	uint16_t dport;					/**< Destination port */
	uint16_t pppoe_session_id;			/**< PPPoE session ID */
	uint8_t pppoe_server_mac[6];			/**< PPPoE server MAC */
	uint8_t protocol;				/**< IP protocol */
	uint8_t hdr_len_type;				/**< Header length type */
	uint8_t hdr_len;				/**< Header length */
	uint8_t inner_pkt_type;				/**< Inner packet type */
	bool is_ipv6;					/**< IPv4/IPv6 flag */
	bool pppoe_en;					/**< PPPoE session enable */
};

/**
 * @brief Tunnel RPS rule
 */
struct nss_ppenl_tun_rps_rule {
	struct nss_ppenl_cmn cm;			/**< common message header */
	struct nss_ppe_nl_tun_rps rule;			/**< Tunnel RPS rule message */
};

/*
 * @brief Message types
 */
enum nss_ppe_tun_rps_message_types {
	NSS_PPE_TUN_RPS_CREATE_RULE_MSG,		/**< Tunnel RPS rule create message */
	NSS_PPE_TUN_RPS_DESTROY_RULE_MSG,		/**< Tunnel RPS rule destroy message */
	NSS_PPE_TUN_RPS_MAX_MSG_TYPES,			/**< Maximum message type */
};

/**
 * @brief NETLINK Tunnel RPS message init
 *
 * @param rule[IN] NSS NETLINK Tunnel RPS rule
 * @param type[IN] Tunnel RPS message type
 */
static inline void nss_ppenl_tun_rps_rule_init(struct nss_ppenl_tun_rps_rule *rule, enum nss_ppe_tun_rps_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_tun_rps_rule), type);
}

#endif /* __NSS_PPENL_TUN_RPS_IF_H */
