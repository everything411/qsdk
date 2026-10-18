/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

/*
 * netfn_flow_auto.h
 *	Netfn flow-auto
 */
#ifndef __NETFN_FLOW_AUTO_H
#define __NETFN_FLOW_AUTO_H

#include <linux/if.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <linux/of.h>
#include <linux/types.h>
#include <linux/version.h>
#include <net/genetlink.h>
#include <linux/msg.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>

#include <linux/inet.h>
#include <netfn_flowmgr.h>

/*
 * netfn_auto_tuple_type
 * 	Tuple Type
 */
enum netfn_auto_tuple_types {
	NETFN_AUTO_THREE_TUPLE,	/* 3-tuple is valid */
	NETFN_AUTO_FOUR_TUPLE,	/* 4-tuple is valid */
	NETFN_AUTO_FIVE_TUPLE,	/* 5-tuple is valid */
	NETFN_AUTO_TUPLE_MAX,
};

/*
 * netfn_auto_ip_version
 * 	IP version
 */
enum netfn_auto_ip_version {
	NETFN_AUTO_IPV4 = 4,	/* IPv4 */
	NETFN_AUTO_IPV6 = 6,	/* IPv6 */
	NETFN_AUTO_IP_MAX,
};

/* netfn_auto_flowmgr_cmds
 * 	Flowmgr generic cmds
 */
enum netfn_auto_flowmgr_cmds {
	NETFN_AUTO_FLOWMGR_CMD_RULE_UNSPEC,	/* Must NOT use element 0 */
	NETFN_AUTO_FLOWMGR_GENL_CMD,	/* Flowmgr generic cmd */
	NETFN_AUTO_FLOWMGR_CMD_MAX,	/* Flowmgr cmd max */
};

/*
 * netfn_auto_flowmgr_cmds
 * 	Capwapmgr generic cmds
 */
enum netfn_auto_capwapmgr_cmds {
	NETFN_AUTO_CAPWAPMGR_CMD_RULE_UNSPEC,	/* Must NOT use element 0 */
	NETFN_AUTO_CAPWAPMGR_GENL_CMD,	/* Capwapmgr generic cmd */
	NETFN_AUTO_CAPWAPMGR_CMD_MAX,	/* Capwapmgr cmd max */
};

/*
 * netfn_auto_flowcookie_cmds
 * 	Flowcookie generic cmds.
 */
enum netfn_auto_flowcookie_cmds {
	NETFN_AUTO_FLOWCOOKIE_CMD_RULE_UNSPEC,	/* Must NOT use element 0 */
	NETFN_AUTO_FLOWCOOKIE_GENL_CMD,	/* Flowcookie generic cmd */
	NETFN_AUTO_FLOWCOOKIE_CMD_MAX,	/* Flowcookie cmd max */
};

/*
 * flowmgr attributes
 */
enum netfn_auto_flowmgr_gnl_attr {
	DONT_USE,
	ACCEL_MODE = 71,
	FLOW_FLAGS = 153,
	TUPLE_INFO = 18,
	FLOW_INFO = 62,
	RULE_INFO = 184,
	CMD = 203,
	FLOWMGR_ATTR_MAX,
};

/*
 * capwapmgr attributes
 */
enum netfn_auto_capwapmgr_gnl_attr {
	CAPWAPMGR_DONT_USE,
	MAX_FRAGS = 67,
	CAPWAP_TUN_DEV = 69,
	L2_INFO = 48,
	CAPWAP_VER = 71,
	ENABLE_DTLS = 74,
	IP_RULE = 127,
	CAPWAP_RULE = 128,
	DTLS_DATA = 85,
	DECAP = 146,
	BSSID = 238,
	FLAGS = 140,
	ENCAP = 138,
	REASM_WINDOW_SIZE = 192,
	CAPWAP_TUN_RULE = 200,
	FEATURES = 202,
	MAX_PAYLOAD_SIZE = 252,
	CAPWAPMGR_ATTR_MAX,
};

/*
 * Flowcookie attributes
 */
enum netfn_auto_flowcookie_gnl_attr {
	FLOWCOOKIE_CMD_UNSPEC,
	FLOWCOOKIE_RULE_ADD_STATUS = 10,
	FLOWCOOKIE_RULE_DELETE_STATUS = 20,
	FLOWCOOKIE_HASH_TABLE = 5,
	FLOWCOOKIE_INFO = 12,
	FLOWCOOKIE_SAWF_ID = 7,
	FLOWCOOKIE_FLOW_ID = 32,
	FLOWCOOKIE_HASH_TABLE_SIZE = 185,
	FLOWCOOKIE_FLOW_MARK = 160,
	FLOWCOOKIE_SAWF_HANDLE = 148,
	FLOWCOOKIE_TUPLE_INFO = 255,
	FLOWCOOKIE_ATTR_MAX,
};

enum netfn_auto_flow_cookie_rule_update {
	FLOW_COOKIE_RULE_UPDATE_FAIL = 0,
	FLOW_COOKIE_RULE_UPDATE_SUCCESS = 1,
};

/*
 * tuple info attributes
 */
enum netfn_auto_tuple_info_parse {
	TUPLE_DONT_USE,
	TUPLE_IP_VERSION = 208,
	TUPLE_TYPE = 234,
	TUPLE_SRC_IP = 252,
	TUPLE_DEST_IP = 150,
	TUPLE_PORT = 37,
	TUPLE_SRC_PORT = 70,
	TUPLE_DEST_PORT = 168,
	TUPLE_PROTOCOL = 170,
	TUPLE_MAX,
};

/*
 * flow info attributes
 */
enum netfn_auto_flow_info_parse {
	FLOW_INFO_DONT_USE,
	FLOW_IN_DEV = 193,
	FLOW_OUT_DEV = 134,
	FLOW_SRC_MAC = 100,
	FLOW_DEST_MAC = 62,
	FLOW_TOP_INDEV = 116,
	FLOW_TOP_OUTDEV = 173,
	FLOW_MTU = 54,
	FLOW_INFO_MAX,
};

/*
 * flowmgr rule info attributes
 */
enum netfn_auto_rule_info_parse {
	RULE_DONT_USE,
	VALID_FLAGS = 37,
	VALID_FLAGS_RETURN = 40,
	IP_XLATE_RULE = 46,
	MAC_XLATE_RULE = 114,
	VLAN_RULE = 95,
	DSCP_RULE = 66,
	QOS_RULE = 157,
	PPPOE_RULE = 150,
	FLOW_COOKIE_RULE = 23,
	VLAN_FILTER_RULE = 194,
	UDP_LITE_RULE = 192,
	TRUSTSEC_RULE = 57,
	ACL_POLICER_RULE = 75,
	RULE_INFO_MAX,
};

/*
 * ip xlate rule info attributes
 */
enum netfn_auto_rule_ip_xlate {
	RULE_IP_DONT_USE,
	IP_VERSION = 35,
	SRC_IP = 211,
	DEST_IP = 95,
	SRC_PORT = 45,
	DEST_PORT = 57,
	IP_XLATE_MAX,
};

/*
 * mac xlate rule info attributes
 */
enum netfn_auto_rule_mac_xlate {
	DEST_MAC = 1,
	SRC_MAC = 117,
	MAC_XLATE_MAX,
};

/*
 * vlan rule attributes
 */
enum netfn_auto_vlan_rule {
	VLAN_DONT_USE,
	INNER_INGRESS = 7,
	VLAN_TPID_INNER = 15,
	INNER_EGRESS = 151,
	VLAN_TPID_OUTER = 188,
	OUTER_INGRESS = 244,
	OUTER_EGRESS = 50,
	INNER = 135,
	OUTER = 212,
	VLAN_RULE_MAX,
};

/*
 * dscp rule attributes.
 */
enum netfn_auto_dscp_rule {
	DSCP_DONT_USE,
	DSCP_VAL = 211,
	DSCP_RULE_MAX,
};

/*
 * qos rule attributes
 */
enum netfn_auto_qos_rule {
	QOS_DONT_USE,
	WIFI_QOS_TAG = 241,
	PRIORITY = 201,
	NET_DEV = 188,
	QOS_RULE_MAX,
};

/*
 * pppoe rule attributes
 */
enum netfn_auto_pppoe_rule {
	PPPOE_RULE_DONT_USE,
	SESSION_ID = 69,
	SERVER_MAC = 58,
	PPPOE_RULE_MAX,
};

/*
 * flow cookie rule
 */
enum netfn_auto_flow_cookie_rule {
	FLOW_COOKIE_DONT_USE,
	FLOW_ID = 67,
	FLOW_COOKIE_MAX,
};

/*
 * vlan filter rule
 */
enum netfn_auto_vlan_filter_rule {
	VLAN_FILTER_DONT_USE,
	VLAN_TAG = 95,
	VLAN_FLAGS = 48,
	VLAN_FILTER_MAX,
};

/*
 * udp lite rule
 */
enum netfn_auto_udp_lite_rule {
	UDP_LITE_DONT_USE,
	CHECKSUM_COVERAGE = 33,
	UDP_LITE_MAX,
};

/*
 * trustsec rule
 */
enum netfn_auto_trustsec_rule {
	TRUSTSEC_DONT_USE,
	SGT = 49,
	TRUSTSEC_RULE_MAX,
};

/*
 * acl policer rule
 */
enum netfn_auto_acl_policer_rule {
	ACL_RULE_DONT_USE,
	RULE_TYPE = 44,
	RULE_ID = 105,
	ACL_RULE_MAX,
};

/*
 * flowmgr flags
 */
enum netfn_auto_flowmgr_flags {
	BRIDGE = 144,
	DS = 192,
	VP = 187,
	VLAN = 124,
	PPPOE = 135,
	PPPOE_ORG = 246,
	PPPOE_REPLY = 156,
	DSCP_MARKING_ORG = 114,
	DSCP_MARKING_REPLY = 200,
	TRUSTSEC = 162,
	QOS = 110,
	QOS_ORG = 63,
	QOS_REPLY = 133,
	SAWF = 36,
	QDISC = 217,
	QDISC_ORG = 152,
	QDISC_REPLY = 206,
	VLAN_FILTER = 203,
	UDP_LITE = 253,
	SRC_NAT = 49,
	DST_NAT = 24,
	MAC = 136,
	NOEDIT_ORG = 71,
	NOEDIT_REPLY = 157,
	TCP = 240,
	FLOWMGR_FLAG_MAX = 255,
};

enum netfn_auto_flowmgr_ret_valid_flags {
	PPPOE_RETURN = 158,
	DSCP_MARKING_RETURN = 194,
	QOS_RETURN = 193,
	NOEDIT_RETURN = 217,
	FLOWMGR_RET_VALID_FLAGS_MAX,
};

enum netfn_auto_capwap_tunnel_create_encap_rule {
	TOS = 21,
	TTL = 123,
	CAPWAP_TUNNEL_CREATE_ENCAP_MAX,
};

enum netfn_auto_capwap_flags {
	DMAC_XLATE = 141,
	DTLS_ENC = 155,
	DTLS_DEC = 227,
	CAPWAP_FLAGS_MAX = 254,
};

enum netfn_auto_dtls_config_flags {
	ENC = 43,
	IPV6 = 152,
	CAPWAP = 189,
	DF = 207,
	DTLS_CONFIG_FLAGS_MAX,
};

enum netfn_auto_capwap_tunnel_create {
	SRC_IF_NUM = 246,
	DEST_IF_NUM = 106,
	PROTOCOL = 53,
	FROM_MTU = 86,
	TO_MTU = 255,
	DEST_IP_XLATE = 20,
	DEST_PORT_XLATE = 226,
	SRC_MAC_XLATE = 190,
	DEST_MAC_XLATE = 154,
	FLOW_WINDOW_SCALE = 71,
	FLOW_MAX_WINDOW = 21,
	FLOW_END = 251,
	FLOW_MAX_END = 18,
	FLOW_PPPOE_IF_EXIST = 28,
	FLOW_PPPOE_IF_NUM = 115,
	INGRESS_VLAN_TAG = 63,
	RETURN_INFO = 60,
	RETURN_WINDOW_SCALE = 141,
	RETURN_MAX_WINDOW = 59,
	RETURN_END = 229,
	RETURN_MAX_END = 208,
	RETURN_PPPOE_IF_EXIST = 26,
	RETURN_PPPOE_IF_NUM = 185,
	EGRESS_VLAN_TAG = 205,
	SPO_NEEDED = 81,
	TOP_NDEV = 172,
	CUST_PARAM_A1 = 147,
	CUST_PARAM_A2 = 38,
	CUST_PARAM_A3 = 185,
	CUST_PARAM_A4 = 76,
	CAPWAPMGR_TUN_UPDATE_TYPE = 88,
	DSCP_INFO = 224,
	QOS_INFO = 119,
	QOS_TAG = 225,
	FLOW_QOS_TAG = 224,
	RETURN_QOS_TAG = 94,
	DSCP_ITAG = 229,
	DSCP_IMASK = 203,
	DSCP_OMASK = 165,
	DSCP_OVAL = 248,
	VLAN_ITAG = 112,
	VLAN_IMASK = 232,
	VLAN_OMASK = 234,
	VLAN_OVAL = 113,
	IN_VLAN_TAG = 141,
	OUT_VLAN_TAG = 6,
	FLOW_DSCP = 52,
	RETURN_DSCP = 78,
	SRC_IP_XLATE = 24,
	SRC_PORT_XLATE = 230,
	TUNNEL_ID = 69,
	PATH_MTU = 197,
	REASSEMBLY_TIMEOUT = 188,
	MAX_BUFFER_SIZE = 168,
	STATS_TIMER = 22,
	RPS = 68,
	TYPE_FLAGS = 49,
	L3_PROTO = 193,
	WHICH_UDP = 210,
	MTU_ADJUST = 171,
	GMAC_IFNUM = 131,
	ENABLED_FEATURES = 44,
	DTLS_INNER_IF_NUM = 189,
	OUTER_SGT_VALUE = 79,
	DTLSMGR_ALGO = 252,
	DTLS_CONFIG = 23,
	DTLS_CRYPTO_DATA = 205,
	DTLS_CRYPTO_LEN = 228,
	DTLS_CRYPTO_CIPHER_KEY_DATA = 75 ,
	DTLS_CRYPTO_CIPHER_KEY_LEN = 210,
	DTLS_CRYPTO_AUTHKEY_DATA = 155,
	DTLS_CRYPTO_AUTHKEY_LEN = 34,
	DTLS_CRYPTO_NONCE_DATA = 173,
	DTLS_CRYPTO_NONCE_LEN = 132,
	DTLS_SIP = 227,
	DTLS_DIP = 106,
	DTLS_VERSION = 71,
	DTLS_CRYPTO = 96,
	DTLS_CRYPTO_ALGO = 166,
	DTLS_SPORT = 125,
	DTLS_DPORT = 252,
	DTLS_EPOCH = 22,
	DTLS_IP_TTL = 255,
	DTLS_DSCP = 219,
	DTLS_DSCP_COPY = 113,
	DTLS_DF = 159,
	DTLS_WINDOW_SIZE = 183,
	DTLS_NEXTHOP_IFNUM = 13,
	NEXTHOP_IFNUM = 125,
	WINDOW_SIZE = 167,
	MTU = 99,
	SNAP_HDR = 104,
	DTLS_HOP_LIMIT = 244,
	CAPWAP_TUNNEL_CREATE_MAX = 270,
};

enum netfn_auto_capwap_legacy_vlan_tag {
	OUT_VLAN_TAG0 = 2,
	IN_VLAN_TAG0 = 135,
	OUT_VLAN_TAG1 = 149,
	IN_VLAN_TAG1 = 244,
	VLAN_TAG_MAX,
};

enum netfn_auto_capwap_legacy_flowcookie {
	ADD = 212,
	SCS_SDWF_ID = 25,
	TYPE = 141,
	CAPWAP_LEGACY_FLOWCOOKIE_MAX,
};

#define NETFN_AUTO_FLOWMGR_GNL_MAX (FLOWMGR_ATTR_MAX + 1)
#define NETFN_AUTO_FLOWCOOKIE_GNL_MAX (FLOWCOOKIE_ATTR_MAX + 1)
#define NETFN_AUTO_CAPWAPMGR_GNL_MAX (CAPWAPMGR_ATTR_MAX + 1)
#define NETFN_STR_TO_INT_PRIME 16777619
#define NETFN_STR_TO_INT_HASH 2166136261

#if defined(CONFIG_DYNAMIC_DEBUG)
/*
 * If dynamic debug is enabled, use pr_debug.
 */
#define netfn_auto_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netfn_auto_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define netfn_auto_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else

/*
 * Statically compile messages at different levels, when dynamic debug is disabled.
 */
#if (NETFN_AUTO_DEBUG_LEVEL < 2)
#define netfn_auto_warn(s, ...)
#else
#define netfn_auto_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NETFN_AUTO_DEBUG_LEVEL < 3)
#define netfn_auto_info(s, ...)
#else
#define netfn_auto_info(s, ...) pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (NETFN_AUTO_DEBUG_LEVEL < 4)
#define netfn_auto_trace(s, ...)
#else
#define netfn_auto_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif

/*
 * debug message for module init and exit
 */
#define netfn_auto_info_always(s, ...) printk(KERN_INFO"%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)

/*
 * netfn_auto_verify_mac()
 * 	Copy mac address
 */
bool netfn_auto_verify_mac(char *str_mac, uint8_t original_mac[])
{
	int ret;

	if (!original_mac || !str_mac) {
		return false;
	}

	ret = sscanf(str_mac, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &original_mac[0], &original_mac[1], &original_mac[2],
			&original_mac[3], &original_mac[4], &original_mac[5]);

	if (ret != ETH_ALEN) {
		return false;
	}

	return true;
}

/*
 * netfn_auto_verify_ipv4()
 * 	Copy ipv4 address
 */
bool netfn_auto_verify_ipv4(const char *ip_str, struct in_addr *ip4)
{
	netfn_auto_info("INSIDE NETFN_AUTO_VERIFY_IP\n");
	if (!in4_pton(ip_str, strlen(ip_str), (uint8_t *)&ip4->s_addr, '\0', NULL)) {
		netfn_auto_warn("invalid source IP V4 value: %s\n", ip_str);
		return false;
	}

	netfn_auto_info("IPv4 addr<Original> :%pI4\n", &ip4->s_addr);

	return true;
}

/*
 * netfn_auto_verify_ipv6()
 * 	Copy ipv6 address
 */
bool netfn_auto_verify_ipv6(const char *ip_str, struct in6_addr *ip6)
{
	if (!in6_pton(ip_str, -1, (uint8_t *)&ip6->s6_addr, -1, NULL)) {
		netfn_auto_warn("invalid source IP V4 value: %s\n", ip_str);
		return false;
	}

	netfn_auto_info("IPv6 addr<Original> :%pI6\n", &ip6->s6_addr);

	return true;
}

/*
 * netfn_auto_verify_ip()
 * 	Copy ipv4 address
 */
bool netfn_auto_verify_ip(const char *ip_str, uint32_t *original_ip_addr, uint8_t ip_version)
{

	if (!ip_str) {
		return false;
	}

	if (ip_version == NETFN_AUTO_IPV4) {
		if (!in4_pton(ip_str, strlen(ip_str), (uint8_t *)&original_ip_addr[0], '\0', NULL)) {
			netfn_auto_warn("invalid source IP V4 value: %s\n", ip_str);
			return false;
		}
		netfn_auto_info("IPv4 addr<Original> :%pI4\n", original_ip_addr);

	} else if (ip_version == NETFN_AUTO_IPV6) {
		if (!in6_pton(ip_str, -1, (uint8_t *)original_ip_addr, -1, NULL)) {
			netfn_auto_warn("invalid source IP V4 value: %s\n", ip_str);
			return false;
		}

		netfn_auto_info("IPv4 addr<Original> :%pI6\n", original_ip_addr);

	}

	return true;
}

/*
 * netfn_auto_parse_tuple()
 * 	parse tuple info.
 */
bool netfn_auto_parse_tuple(struct netfn_tuple *original, struct nlattr *tuple)
{
	int rem, tuple_type, tuple_ip_version, port;
	uint32_t protocol;
	struct nlattr *attr;
	netfn_auto_info("INSIDE NETFN_AUTO TUPLE PARSE\n");
	nla_for_each_nested(attr, tuple, rem) {
		char *sip, *dip;
		netfn_auto_info("attr->nla_type: %d\n", attr->nla_type);
		if(attr->nla_type == TUPLE_TYPE) {
			tuple_type = nla_get_u32(attr);
			netfn_auto_info("tuple type: %d\n", tuple_type);
			original->tuple_type = tuple_type;

		} else if(attr->nla_type == TUPLE_IP_VERSION) {
			tuple_ip_version = nla_get_u32(attr);
			netfn_auto_info("tuple ip version: %d\n", tuple_ip_version);
			original->ip_version = tuple_ip_version;
		}

		switch(tuple_type) {
			case NETFN_AUTO_THREE_TUPLE:
				if(attr->nla_type == TUPLE_PROTOCOL) {
					protocol = nla_get_u32(attr);
					netfn_auto_info("Netlink tuple protocol : %d\n", protocol);
					original->tuples.tuple_3.protocol = (uint8_t)protocol;

				} else if (attr->nla_type == TUPLE_SRC_IP) {
					sip = nla_data(attr);
					netfn_auto_info("Netlink Tuple SIP: %s\n", sip);
					if (tuple_ip_version == NETFN_AUTO_IPV4) {
						if(!netfn_auto_verify_ipv4(sip, &original->tuples.tuple_3.src_ip.ip4)) {
							return false;
						}

					} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
						if(!netfn_auto_verify_ipv6(sip, &original->tuples.tuple_3.src_ip.ip6)) {
							return false;
						}
					}

					break;

				} else if(attr->nla_type == TUPLE_DEST_IP) {
					dip = nla_data(attr);
					netfn_auto_info("Netlink Tuple DIP: %s\n", dip);
					if (tuple_ip_version == NETFN_AUTO_IPV4) {
						if(!netfn_auto_verify_ipv4(dip, &original->tuples.tuple_3.dest_ip.ip4)) {
							return false;
						}

					} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
						if(!netfn_auto_verify_ipv6(sip, &original->tuples.tuple_3.dest_ip.ip6)) {
							return false;
						}
					}
				}

				break;

			case NETFN_AUTO_FOUR_TUPLE:
				if(attr->nla_type == TUPLE_PROTOCOL) {
					protocol = nla_get_u32(attr);
					netfn_auto_info("Netlink tuple protocol : %d\n", protocol);
					original->tuples.tuple_4.protocol = (uint8_t)protocol;

				} else if (attr->nla_type == TUPLE_SRC_IP) {
					sip = nla_data(attr);
					netfn_auto_info("Netlink Tuple SIP: %s\n", sip);
					if (tuple_ip_version == NETFN_AUTO_IPV4) {
						if(!netfn_auto_verify_ipv4(sip, &original->tuples.tuple_4.src_ip.ip4)) {
							return false;
						}

					} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
						if(!netfn_auto_verify_ipv6(sip, &original->tuples.tuple_4.src_ip.ip6)) {
							return false;
						}
					}

				} else if(attr->nla_type == TUPLE_DEST_IP) {
					dip = nla_data(attr);
					netfn_auto_info("Netlink Tuple DIP: %s\n", dip);
					if (tuple_ip_version == NETFN_AUTO_IPV4) {
						if(!netfn_auto_verify_ipv4(dip, &original->tuples.tuple_4.dest_ip.ip4)) {
							return false;
						}

					} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
						if(!netfn_auto_verify_ipv6(sip, &original->tuples.tuple_4.dest_ip.ip6)) {
							return false;
						}
					}

				} else if(attr->nla_type == TUPLE_PORT) {
					port = nla_get_u32(attr);
					netfn_auto_info("Netlink tuple port : %d\n", port);
					original->tuples.tuple_4.l4_ident = (__be16)port;
				}
				break;

			case NETFN_AUTO_FIVE_TUPLE:
				if(attr->nla_type == TUPLE_PROTOCOL) {
					protocol = nla_get_u32(attr);
					netfn_auto_info("Netlink tuple protocol : %d\n", protocol);
					original->tuples.tuple_5.protocol = (uint8_t)protocol;

				} else if (attr->nla_type == TUPLE_SRC_IP) {
					sip = nla_data(attr);
					netfn_auto_info("Netlink Tuple SIP: %s\n", sip);
					if (tuple_ip_version == NETFN_AUTO_IPV4) {
						if(!netfn_auto_verify_ipv4(sip, &original->tuples.tuple_5.src_ip.ip4)) {
							return false;
						}

					} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
						if(!netfn_auto_verify_ipv6(sip, &original->tuples.tuple_5.src_ip.ip6)) {
							return false;
						}
					}

				} else if(attr->nla_type == TUPLE_DEST_IP) {
					dip = nla_data(attr);
					netfn_auto_info("Netlink Tuple DIP: %s\n", dip);
					if (tuple_ip_version == NETFN_AUTO_IPV4) {
						if(!netfn_auto_verify_ipv4(dip, &original->tuples.tuple_5.dest_ip.ip4)) {
							return false;
						}

					} else if (tuple_ip_version == NETFN_AUTO_IPV6) {
						if(!netfn_auto_verify_ipv6(dip, &original->tuples.tuple_5.dest_ip.ip6)) {
							return false;
						}
					}

				} else if (attr->nla_type == TUPLE_SRC_PORT) {
					port = nla_get_u32(attr);
					netfn_auto_info("Netlink tuple src_port : %d\n", port);
					original->tuples.tuple_5.l4_src_ident = (__be16)port;

				} else if (attr->nla_type == TUPLE_DEST_PORT) {
					port = nla_get_u32(attr);
					netfn_auto_info("Netlink tuple dest port : %d\n", port);
					original->tuples.tuple_5.l4_dest_ident = (__be16)port;
				}
				break;

			default:
				netfn_auto_warn("Wrong tuple type\n");
				return false;
		}
	}

	netfn_auto_info("netfn_auto tuple info parse success\n");
	return true;
}

/*
 * netfn_auto_capwapmgr_parse_flow_info()
 * 	parse flow info for capwapmgr.
 */
bool netfn_auto_capwapmgr_parse_flow_info(struct netfn_flowmgr_flow_info *flow, struct nlattr *flow_info)
{
	char *dev_name, *smac, *dmac;
	struct nlattr *attr = NULL;
	uint32_t flow_mtu;
	int rem;

	nla_for_each_nested(attr ,flow_info, rem) {
		netfn_auto_info("FLOW_INFO attr->nla_type: %d\n", attr->nla_type);
		switch (attr->nla_type)
		{
		case FLOW_IN_DEV:
			dev_name = nla_data(attr);
			netfn_auto_info("FLOW_IN_DEV: %s\n", dev_name);
			flow->in_dev = dev_get_by_name(&init_net, dev_name);
			break;

		case FLOW_OUT_DEV:
			dev_name = nla_data(attr);
			netfn_auto_info("FLOW_OUT_DEV: %s\n", dev_name);
			flow->out_dev = dev_get_by_name(&init_net, dev_name);
			break;

		case FLOW_SRC_MAC:
			smac = nla_data(attr);
			netfn_auto_info("FLOW_SRC_MAC: %s\n", smac);
			if (!netfn_auto_verify_mac(smac, flow->flow_src_mac)) {
				netfn_auto_warn("Invalid flow src mac address\n");
				return false;
			}
			break;

		case FLOW_DEST_MAC:
			dmac = nla_data(attr);
			netfn_auto_info("FLOW_DEST_MAC: %s\n", dmac);
			if (!netfn_auto_verify_mac(dmac, flow->flow_dest_mac)) {
				netfn_auto_warn("Invalid flow dest mac address\n");
				return false;
			}
			break;

		case FLOW_TOP_INDEV:
			dev_name = nla_data(attr);
			netfn_auto_info("FLOW_TOP_INDEV: %s\n", dev_name);
			flow->top_indev = dev_get_by_name(&init_net, dev_name);
			break;

		case FLOW_TOP_OUTDEV:
			dev_name = nla_data(attr);
			netfn_auto_info("FLOW_TOP_OUTDEV: %s\n", dev_name);
			flow->top_outdev = dev_get_by_name(&init_net, dev_name);
			break;

		case FLOW_MTU:
			flow_mtu = nla_get_u32(attr);
			netfn_auto_info("FLOW_MTU: %d\n", flow_mtu);
			flow->flow_mtu = flow_mtu;
			break;

		default:
			netfn_auto_warn("WRONG attr: %d in FLOW INFO\n", attr->nla_type);
			return false;
		}
	}

	netfn_auto_info("netfn_auto flow info parse success\n");
	return true;
}

#endif
