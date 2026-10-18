/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_TUN_RPS_H
#define __PPECFG_TUN_RPS_H

#define PPECFG_TUN_RPS_HDR_VERSION 1

/*
 * PPE Tunnel Program Parser (TPR) maximum offset and header length bounds (in bytes).
 * SSDK TPR offset registers (e.g. UDF0_OFFSET) are 6-bit fields operating in 2-byte steps,
 * the maximum representable offset is 63 * 2 = 126.
 */
#define PPECFG_TUN_RPS_TPR_MAX_OFFSET 126

/*
 * PPE CFG RPS header length definitions
 */
#define PPECFG_TUN_RPS_HDR_LEN_TYPE_INVALID -1
#define PPECFG_TUN_RPS_HDR_LEN_TYPE_ETH	0
#define PPECFG_TUN_RPS_HDR_LEN_TYPE_IP	1
#define PPECFG_TUN_RPS_HDR_LEN_TYPE_UDP	2

/*
 * PPE CFG RPS inner packet type definitions
 */
#define PPECFG_TUN_RPS_INNER_PKT_TYPE_INVALID	-1
#define PPECFG_TUN_RPS_INNER_PKT_TYPE_ETH	0
#define PPECFG_TUN_RPS_INNER_PKT_TYPE_IP	1

/*
 * PPECFG Tunnel RPS Error
 */
enum ppecfg_tun_rps_error {
	PPECFG_TUN_RPS_SUCCESS,			/* Success */
	PPECFG_TUN_RPS_RULE_ADD_FAILED,		/* Rule add failed */
	PPECFG_TUN_RPS_RULE_DEL_FAILED,		/* Rule delete failed */
	PPECFG_TUN_RPS_ERROR_MAX
};

/*
 * PPECFG Tunnel RPS commands
 */
enum ppecfg_tun_rps_cmd {
	PPECFG_TUN_RPS_CMD_RULE_ADD,	/* Add RPS rule */
	PPECFG_TUN_RPS_CMD_RULE_DEL,	/* Delete RPS rule */
	PPECFG_TUN_RPS_CMD_MAX
};

/*
 * PPECFG Tunnel RPS user data fields
 */
enum ppecfg_tun_rps_usr_data1 {
	PPECFG_TUN_RPS_USR_DATA1_EN,		/* User data 1 enable */
	PPECFG_TUN_RPS_USR_DATA1_OFFSET,	/* User data 1 offset */
	PPECFG_TUN_RPS_USR_DATA1_VAL,		/* User data 1 value */
	PPECFG_TUN_RPS_USR_DATA1_MASK,		/* User data 1 mask */
	PPECFG_TUN_RPS_USR_DATA1_MAX
};

/*
 * PPECFG Tunnel RPS user data 2 fields
 */
enum ppecfg_tun_rps_usr_data2 {
	PPECFG_TUN_RPS_USR_DATA2_EN,		/* User data 2 enable */
	PPECFG_TUN_RPS_USR_DATA2_OFFSET,	/* User data 2 offset */
	PPECFG_TUN_RPS_USR_DATA2_VAL,		/* User data 2 value */
	PPECFG_TUN_RPS_USR_DATA2_MASK,		/* User data 2 mask */
	PPECFG_TUN_RPS_USR_DATA2_MAX
};

/*
 * PPECFG Tunnel RPS user data 3 fields
 */
enum ppecfg_tun_rps_usr_data3 {
	PPECFG_TUN_RPS_USR_DATA3_EN,		/* User data 3 enable */
	PPECFG_TUN_RPS_USR_DATA3_OFFSET,	/* User data 3 offset */
	PPECFG_TUN_RPS_USR_DATA3_VAL,		/* User data 3 value */
	PPECFG_TUN_RPS_USR_DATA3_MASK,		/* User data 3 mask */
	PPECFG_TUN_RPS_USR_DATA3_MAX
};

/*
 * PPECFG Tunnel RPS rule add parameters
 */
enum ppecfg_tun_rps_rule_add {
	PPECFG_TUN_RPS_RULE_ADD_IS_IPV6,	/* IPv6 flag */
	PPECFG_TUN_RPS_RULE_ADD_SIP,		/* Source IP */
	PPECFG_TUN_RPS_RULE_ADD_DIP,		/* Destination IP */
	PPECFG_TUN_RPS_RULE_ADD_PROTO,		/* Protocol */
	PPECFG_TUN_RPS_RULE_ADD_SPORT,		/* Source port */
	PPECFG_TUN_RPS_RULE_ADD_DPORT,		/* Destination port */
	PPECFG_TUN_RPS_RULE_ADD_HDR_LEN_TYPE,	/* Header length type */
	PPECFG_TUN_RPS_RULE_ADD_HDR_LEN,	/* Header length */
	PPECFG_TUN_RPS_RULE_ADD_USR_DATA1,	/* User data 1 fields */
	PPECFG_TUN_RPS_RULE_ADD_USR_DATA2,	/* User data 2 fields */
	PPECFG_TUN_RPS_RULE_ADD_USR_DATA3,	/* User data 3 fields */
	PPECFG_TUN_RPS_RULE_ADD_INNER_PKT_TYPE,	/* Inner packet type */
	PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL,	/* Ethernet protocol field */
	PPECFG_TUN_RPS_RULE_ADD_ETH_PROTOCOL_MASK, /* Ethernet protocol mask */
	PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL,	/* IP protocol field */
	PPECFG_TUN_RPS_RULE_ADD_IP_PROTOCOL_MASK, /* IP protocol mask */
	PPECFG_TUN_RPS_RULE_ADD_UDP_SPORT,	/* UDP source port */
	PPECFG_TUN_RPS_RULE_ADD_UDP_DPORT,	/* UDP destination port */
	PPECFG_TUN_RPS_RULE_ADD_WAN_IF,		/* WAN interface name */
	PPECFG_TUN_RPS_RULE_ADD_INNER_IP_PROTO_OFFSET,		/* Inner IP protocol offset */
	PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_VAL,		/* Inner IPv4 protocol value */
	PPECFG_TUN_RPS_RULE_ADD_INNER_IPV4_PROTO_MASK,		/* Inner IPv4 protocol mask */
	PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_VAL,		/* Inner IPv6 protocol value */
	PPECFG_TUN_RPS_RULE_ADD_INNER_IPV6_PROTO_MASK,		/* Inner IPv6 protocol mask */
	PPECFG_TUN_RPS_RULE_ADD_PPPOE_EN,			/* PPPoE enable */
	PPECFG_TUN_RPS_RULE_ADD_PPPOE_SESSION_ID,		/* PPPoE session ID */
	PPECFG_TUN_RPS_RULE_ADD_PPPOE_SERVER_MAC,		/* PPPoE server MAC */
	PPECFG_TUN_RPS_RULE_ADD_MAX
};

/*
 * PPECFG Tunnel RPS rule delete parameters
 */
enum ppecfg_tun_rps_rule_del {
	PPECFG_TUN_RPS_RULE_DEL_IS_IPV6,	/* IPv6 flag */
	PPECFG_TUN_RPS_RULE_DEL_SIP,		/* Source IP */
	PPECFG_TUN_RPS_RULE_DEL_DIP,		/* Destination IP */
	PPECFG_TUN_RPS_RULE_DEL_PROTO,		/* Protocol */
	PPECFG_TUN_RPS_RULE_DEL_SPORT,		/* Source port */
	PPECFG_TUN_RPS_RULE_DEL_DPORT,		/* Destination port */
	PPECFG_TUN_RPS_RULE_DEL_USR_DATA1,	/* User data 1 fields */
	PPECFG_TUN_RPS_RULE_DEL_USR_DATA2,	/* User data 2 fields */
	PPECFG_TUN_RPS_RULE_DEL_USR_DATA3,	/* User data 3 fields */
	PPECFG_TUN_RPS_RULE_DEL_MAX
};

#endif /* __PPECFG_TUN_RPS_H */
