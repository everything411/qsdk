/*
 **************************************************************************
 * Copyright (c) 2014-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023,2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

/**
 * nss_capwapmgr_legacy.h
 *	CAPWAP manager for NSS
 */
#ifndef __NSS_CAPWAPMGR_LEGACY_H
#define __NSS_CAPWAPMGR_LEGACY_H

#include <linux/rtnetlink.h>
#include <linux/types.h>

#include <ppe_drv_iface.h>
#include <netfn_capwapmgr.h>
#include <netfn_capwap_types.h>

#define NSS_CAPWAPMGR_MAX_TUNNELS		32
					/** Maximum number of tunnels currently supported. */
#define MAX_VLAN_DEPTH 2

/*
 * Maxmimum values for rule configuration parameters.
 */
#define NSS_CAPWAP_MAX_MTU		NETFN_CAPWAP_MAX_BUF_SZ
				/**< Maximum MTU supported by NSS FW. */
#define NSS_CAPWAP_MAX_BUFFER_SIZE	NETFN_CAPWAP_MAX_BUF_SZ
				/**< Maximum buffer-size supported by NSS FW. */
#define NSS_CAPWAP_MAX_FRAGMENTS	NETFN_CAPWAP_MAX_FRAGS
				/**< Maximum fragments for reassembly. */

/*
 * CAPWAP Rule configure message flags
 */
#define NSS_CAPWAP_TUNNEL_IPV4		2
				/**< IPv4 tunnel. */
#define NSS_CAPWAP_TUNNEL_IPV6		3
				/**< IPv6 tunnel. */
#define NSS_CAPWAP_TUNNEL_UDP		4
				/**< UDP tunnel. */
#define NSS_CAPWAP_TUNNEL_UDPLite	5
				/**< UDPLite tunnel. */

/*
 * CAPWAP tunnel create and type flags. These flags are used
 * to determine packet header size during encapsulation.
 */
#define NSS_CAPWAP_RULE_CREATE_VLAN_CONFIGURED	0x1
				/**< VLAN Configured for CAPWAP tunnel. */
#define NSS_CAPWAP_RULE_CREATE_PPPOE_CONFIGURED	0x2
				/**< PPPoE configured for CAPWAP tunnel. */
#define NSS_CAPWAP_ENCAP_UDPLITE_HDR_CSUM	0x4
				/**< Generate only UDP-Lite header checksum. Otherwise whole UDP-Lite payload. */

#define NSS_CAPWAPMGR_FEATURE_DTLS_ENABLED              NETFN_CAPWAP_FEATURES_DTLS      /* Tunnel enabled DTLS. */
#define NSS_CAPWAPMGR_FEATURE_INNER_TRUSTSEC_ENABLED    NETFN_CAPWAP_FEATURES_INNER_SGT      /* Tunnel enabled inner trustsec. */
#define NSS_CAPWAPMGR_FEATURE_OUTER_TRUSTSEC_ENABLED    NETFN_CAPWAP_FEATURES_OUTER_SGT      /* Tunnel enabled outer trustsec. */
#define NSS_CAPWAPMGR_FEATURE_WIRELESS_QOS_ENABLED      NETFN_CAPWAP_FEATURES_WLAN_QOS      /* Tunnel enabled wireless QoS. */

/*
 * CAPWAP version
 */
#define NSS_CAPWAP_VERSION_V1		NETFN_CAPWAP_VERSION_V1
				/**< RFC CAPWAP version. */
#define NSS_CAPWAP_VERSION_V2		NETFN_CAPWAP_VERSION_V2
				/**< Initial CAPWAP version for a customer. */

/*
 * Type of packet. These are mutually exclusive fields.
 */
#define NSS_CAPWAP_PKT_TYPE_UNKNOWN	0x0000
				/**< Don't know the type of CAPWAP packet. */
#define NSS_CAPWAP_PKT_TYPE_CONTROL	NETFN_CAPWAP_PKT_TYPE_CTRL
				/** It's a control CAPWAP packet src_port=5247. */
#define NSS_CAPWAP_PKT_TYPE_DATA	NETFN_CAPWAP_PKT_TYPE_DATA
				/**< It's a data CAPWAP packet src_port=5246. */

/*
 * Addtional fields for identifying what's there in the packet.
 */
#define NSS_CAPWAP_PKT_TYPE_WIRELESS_INFO	NETFN_CAPWAP_PKT_TYPE_WINFO
				/**< W=1, wireless info present. */
#define NSS_CAPWAP_PKT_TYPE_802_11		NETFN_CAPWAP_PKT_TYPE_80211
				/**< T=1, then set wbid=1. */
#define NSS_CAPWAP_PKT_TYPE_802_3		NETFN_CAPWAP_PKT_TYPE_8023
				/**< Data is in 802.3 format. */
#define NSS_CAPWAP_PKT_TYPE_SCS_ID_VALID	NETFN_CAPWAP_PKT_TYPE_SCS_ID_VALID
				/**< Inner SCS ID valid. */
#define NSS_CAPWAP_PKT_TYPE_SDWF_ID_VALID	NETFN_CAPWAP_PKT_TYPE_SDWF_ID_VALID
				/**< Inner SDWF ID valid. */

#define NSS_CAPWAP_WINFO_SZ NETFN_CAPWAP_WIRELESS_INFO_LEN
				/**< Size of Wireless Info. */

/**
 *
 * NOTE: The following structure is a shallow copy of the netfn_capwap_prehdr
 * structure in netfn_capwap_types.h file.
 * When making changes to this structure, we need to make sure that the
 * netfn_capwap_prehdr is also updated.
 *
 * nss_capwap_metaheader
 *	CAPWAP metaheader per-packet for both encap (TX) and decap (RX).
 */
struct nss_capwap_metaheader {
	uint8_t version;	/**< CAPWAP version. */
	uint8_t rid;		/**< Radio ID. */
	uint16_t tunnel_id;	/**< Tunnel-ID. */
	uint8_t dscp;		/**< DSCP value. */
	uint8_t vlan_pcp;	/**< VLAN priority .P marking. */
	uint16_t type;		/**< Type of CAPWAP packet & What was there in CAPWAP header. */
	uint16_t nwireless;	/**< Number of wireless info sections in CAPWAP header. */
	uint16_t wireless_qos;	/**< 802.11e qos info. */
	uint16_t outer_sgt;	/**< Security Group Tag value in the TrustSec header. */
	uint16_t inner_sgt;	/**< Security Group Tag value in the TrustSec header. */
	uint32_t flow_id;	/**< Flow identification. */
	union {
		struct {
			uint16_t vapid;		/**< VAP ID info. */
			uint16_t reserved;	/**< Reserved for backwards comparibility. */
		};

		uint32_t scs_sdwf_id;		/**< SCS/SDWF ID. */
	};

	/*
	 * Put the wl_info at last so we don't have to do copy if 802.11 to 802.3 conversion did not happen.
	 */
	uint8_t wl_info[NSS_CAPWAP_WINFO_SZ];	/**< Wireless info preserved from the original packet. */
} __packed __aligned(4);


/**
 * nss_ipv4_create
 *	Information for an IPv4 flow or connection create rule.
 *
 * All fields must be passed in host-endian order.
 */
struct nss_ipv4_create {
	int32_t src_interface_num;
				/**< Source interface number (virtual or physical). */
	int32_t dest_interface_num;
				/**< Destination interface number (virtual or physical). */
	int32_t protocol;	/**< L4 protocol, e.g., TCP or UDP. */
	uint32_t flags;		/**< Flags associated with this rule. */
	uint32_t from_mtu;	/**< MTU of the incoming interface. */
	uint32_t to_mtu;	/**< MTU of the outgoing interface. */
	uint32_t src_ip;	/**< Source IP address. */
	int32_t src_port;	/**< Source L4 port, e.g., TCP or UDP port. */
	uint32_t src_ip_xlate;	/**< Translated source IP address (used with SNAT). */
	int32_t src_port_xlate;	/**< Translated source L4 port (used with SNAT). */
	uint32_t dest_ip;	/**< Destination IP address. */
	int32_t dest_port;	/**< Destination L4 port, e.g., TCP or UDP port. */
	uint32_t dest_ip_xlate;
			/**< Translated destination IP address (used with DNAT). */
	int32_t dest_port_xlate;
			/**< Translated destination L4 port (used with DNAT). */
	uint8_t src_mac[ETH_ALEN];
			/**< Source MAC address. */
	uint8_t dest_mac[ETH_ALEN];
			/**< Destination MAC address. */
	uint8_t src_mac_xlate[ETH_ALEN];
			/**< Translated source MAC address (post-routing). */
	uint8_t dest_mac_xlate[ETH_ALEN];
			/**< Translated destination MAC address (post-routing). */
	uint8_t flow_window_scale;	/**< Window scaling factor (TCP). */
	uint32_t flow_max_window;	/**< Maximum window size (TCP). */
	uint32_t flow_end;		/**< TCP window end. */
	uint32_t flow_max_end;		/**< TCP window maximum end. */
	uint32_t flow_pppoe_if_exist;
			/**< Flow direction: PPPoE interface existence flag. */
	int32_t flow_pppoe_if_num;
			/**< Flow direction: PPPoE interface number. */
	uint16_t ingress_vlan_tag;	/**< Ingress VLAN tag expected for this flow. */
	uint8_t return_window_scale;
			/**< Window scaling factor of the return direction (TCP). */
	uint32_t return_max_window;
			/**< Maximum window size of the return direction. */
	uint32_t return_end;
			/**< Flow end for the return direction. */
	uint32_t return_max_end;
			/**< Flow maximum end for the return direction. */
	uint32_t return_pppoe_if_exist;
			/**< Return direction: PPPoE interface existence flag. */
	int32_t return_pppoe_if_num;
			/**< Return direction: PPPoE interface number. */
	uint16_t egress_vlan_tag;	/**< Egress VLAN tag expected for this flow. */
	uint8_t spo_needed;		/**< Indicates whether SPO is required. */
	struct net_device *top_ndev;	/**< Netdevice associated with the top interface. */
	uint32_t param_a1;		/**< Custom parameter 1. */
	uint32_t param_a2;		/**< Custom parameter 2. */
	uint32_t param_a3;		/**< Custom parameter 3. */
	uint32_t param_a4;		/**< Custom parameter 4. */
	uint32_t qos_tag;		/**< Deprecated, will be removed soon. */
	uint32_t flow_qos_tag;		/**< QoS tag value for the flow direction. */
	uint32_t return_qos_tag;	/**< QoS tag value for the return direction. */
	uint8_t dscp_itag;		/**< DSCP marking tag. */
	uint8_t dscp_imask;		/**< DSCP marking input mask. */
	uint8_t dscp_omask;		/**< DSCP marking output mask. */
	uint8_t dscp_oval;		/**< DSCP marking output value. */
	uint16_t vlan_itag;		/**< VLAN marking tag. */
	uint16_t vlan_imask;		/**< VLAN marking input mask. */
	uint16_t vlan_omask;		/**< VLAN marking output mask. */
	uint16_t vlan_oval;		/**< VLAN marking output value. */
	uint32_t in_vlan_tag[MAX_VLAN_DEPTH];
			/**< Ingress VLAN tag expected for this flow. */
	uint32_t out_vlan_tag[MAX_VLAN_DEPTH];
			/**< Egress VLAN tag expected for this flow. */
	uint8_t flow_dscp;		/**< IP DSCP value for the flow direction. */
	uint8_t return_dscp;		/**< IP DSCP value for the return direction. */
};

/**
* nss_ipv6_create
*	Information for an IPv6 flow or connection create rule.
*
* All fields must be passed in host-endian order.
*/
struct nss_ipv6_create {
       int32_t src_interface_num;
		       /**< Source interface number (virtual or physical). */
       int32_t dest_interface_num;
		       /**< Destination interface number (virtual or physical). */
       int32_t protocol;	/**< L4 protocol, e.g., TCP or UDP,. */
       uint32_t flags;		/**< Flags associated with this rule. */
       uint32_t from_mtu;	/**< MTU of the incoming interface. */
       uint32_t to_mtu;	/**< MTU of the outgoing interface. */
       uint32_t src_ip[4];	/**< Source IP address. */
       int32_t src_port;	/**< Source L4 port, e.g., TCP or UDP port. */
       uint32_t dest_ip[4];	/**< Destination IP address. */
       int32_t dest_port;	/**< Destination L4 port, e.g., TCP or UDP port. */
       uint8_t src_mac[ETH_ALEN];	/**< Source MAC address. */
       uint8_t dest_mac[ETH_ALEN];	/**< Destination MAC address. */
       uint8_t flow_window_scale;	/**< Window scaling factor (TCP). */
       uint32_t flow_max_window;	/**< Maximum window size (TCP). */
       uint32_t flow_end;		/**< TCP window end. */
       uint32_t flow_max_end;		/**< TCP window maximum end. */
       uint32_t flow_pppoe_if_exist;
		       /**< Flow direction: PPPoE interface existence flag. */
       int32_t flow_pppoe_if_num;
		       /**< Flow direction: PPPoE interface number. */
       uint16_t ingress_vlan_tag;
		       /**< Ingress VLAN tag expected for this flow. */
       uint8_t return_window_scale;
		       /**< Window scaling factor (TCP) for the return direction. */
       uint32_t return_max_window;
		       /**< Maximum window size (TCP) for the return direction. */
       uint32_t return_end;
		       /**< End for the return direction. */
       uint32_t return_max_end;
		       /**< Maximum end for the return direction. */
       uint32_t return_pppoe_if_exist;
		       /**< Return direction: PPPoE interface existence flag. */
       int32_t return_pppoe_if_num;
		       /**< Return direction: PPPoE interface number. */
       uint16_t egress_vlan_tag;	/**< Egress VLAN tag expected for this flow. */
       uint32_t qos_tag;		/**< Deprecated; will be removed soon. */
       uint32_t flow_qos_tag;		/**< QoS tag value for flow direction. */
       uint32_t return_qos_tag;	/**< QoS tag value for the return direction. */
       uint8_t dscp_itag;		/**< DSCP marking tag. */
       uint8_t dscp_imask;		/**< DSCP marking input mask. */
       uint8_t dscp_omask;		/**< DSCP marking output mask. */
       uint8_t dscp_oval;		/**< DSCP marking output value. */
       uint16_t vlan_itag;		/**< VLAN marking tag. */
       uint16_t vlan_imask;		/**< VLAN marking input mask. */
       uint16_t vlan_omask;		/**< VLAN marking output mask. */
       uint16_t vlan_oval;		/**< VLAN marking output value. */
       uint32_t in_vlan_tag[MAX_VLAN_DEPTH];
				       /**< Ingress VLAN tag expected for this flow. */
       uint32_t out_vlan_tag[MAX_VLAN_DEPTH];
				       /**< Egress VLAN tag expected for this flow. */
       uint8_t flow_dscp;		/**< IP DSCP value for flow direction. */
       uint8_t return_dscp;		/**< IP DSCP value for the return direction. */
       struct net_device *top_ndev;	/**< Netdevice associated with the top interface. */
};

/**
 * nss_capwap_ip
 *	IP versions.
 */
struct nss_capwap_ip {
	/**
	 * Union of IPv4 and IPv6 IP addresses.
	 */
	union {
		uint32_t ipv4;		/**< IPv4 address. */
		uint32_t ipv6[4];	/**< IPv6 address. */
	} ip;		/**< Union of IPv4 and IPv6 IP addresses. */
};

/**
 * nss_capwap_encap_rule
 *	Encapsulation information for a CAPWAP tunnel.
 */
struct nss_capwap_encap_rule {
	struct  nss_capwap_ip src_ip;	/**< Source IP. */
	uint32_t src_port;		/**< Source port. */
	struct nss_capwap_ip dest_ip;	/**< Destination IP. */
	uint32_t dest_port;		/**< Destination port. */
	uint32_t path_mtu;		/**< MTU on the path. */
	uint8_t ttl;			/**< TTL configured for this tunnel. */
	uint8_t tos;			/**< Tos for encapsulation. */
};

/**
 * nss_capwap_decap_rule
 *	Decapsulation information for a CAPWAP tunnel.
 */
struct nss_capwap_decap_rule {
	uint32_t reserved;		/**< Reserved. */
	uint32_t max_fragments;		/**< Maximum number of fragments expected. */
	uint32_t max_buffer_size;	/**< Maximum size of the payload buffer. */
};

/**
 * nss_capwap_rule_msg
 *	CAPWAP rule message.
 *
 * The same rule structure applies for both encapsulation and decapsulation
 * in a tunnel.
 */
struct nss_capwap_rule_msg {
	struct nss_capwap_encap_rule encap;	/**< Encapsulation portion of the rule. */
	struct nss_capwap_decap_rule decap;	/**< Decapsulation portion of the rule. */
	uint32_t reserved0;			/**< Reserved. */
	int8_t reserved1;			/**< Reserved. */
	uint8_t type_flags;			/**< VLAN or PPPOE is configured. */
	uint8_t l3_proto;
						/**< Prototype is NSS_CAPWAP_TUNNEL_IPV4 or NSS_CAPWAP_TUNNEL_IPV6. */
	uint8_t which_udp;			/**< Tunnel uses the UDP or UDPLite protocol. */
	uint32_t mtu_adjust;			/**< MTU is reserved for a DTLS process. */
	uint32_t reserved2;			/**< Reserved */
	uint32_t enabled_features;
						/**< Tunnel enabled features bit flag. */
	/*
	 * Parameters for each features
	 */
	uint32_t reserved3;			/**< Reserved. */
	uint8_t bssid[ETH_ALEN];		/**< BSSID value. */
	uint16_t outer_sgt_value;
						/**< Security Group Tag value configured for this tunnel. */

};

/**
 * nss_capwapmgr_status_t
 *	CAPWAP status enums
 */
typedef enum {
	NSS_CAPWAPMGR_SUCCESS,					/**< Configuration successful */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_ENABLED = 100,	/**< Tunnel is enabled. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_DISABLED,		/**< Tunnel is disabled. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_NOT_CFG,		/**< Tunnel is not configured yet. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_EXISTS,		/**< Tunnel already exisits. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_DOES_NOT_EXIST, /**< Tunnel does not exist. */
	NSS_CAPWAPMGR_MAX_TUNNEL_COUNT_EXCEEDED,	/**< Exceeding msximum allowed tunnels. */
	NSS_CAPWAPMGR_FAILURE_DI_ALLOC_FAILED,		/**< Dynamic interface alloc failed. */
	NSS_CAPWAPMGR_FAILURE_CAPWAP_RULE,		/**< Failed to create CAPWAP rule. */
	NSS_CAPWAPMGR_FAILURE_IP_RULE,			/**< Failed to create IP rule. */
	NSS_CAPWAPMGR_INVALID_IP_RULE,			/**< Invalid IP rule for the tunnel. */
	NSS_CAPWAPMGR_FAILURE_REGISTER_NSS,		/**< Failed to register with NSS. */
	NSS_CAPWAPMGR_FAILURE_CMD_TIMEOUT,		/**< NSS Driver Command timed-out. */
	NSS_CAPWAPMGR_FAILURE_INVALID_REASSEMBLY_TIMEOUT,/**< Invalid reasm timeout. */
	NSS_CAPWAPMGR_FAILURE_INVALID_PATH_MTU,		/**< Invalid path mtu. */
	NSS_CAPWAPMGR_FAILURE_INVALID_MAX_FRAGMENT,	/**< Invalid max fragment. */
	NSS_CAPWAPMGR_FAILURE_INVALID_BUFFER_SIZE,	/**< Invalid buffer size. */
	NSS_CAPWAPMGR_FAILURE_INVALID_L3_PROTO,		/**< Invalid Layer3 protocol. */
	NSS_CAPWAPMGR_FAILURE_INVALID_UDP_PROTO,	/**< Invalid UDP protocol. */
	NSS_CAPWAPMGR_FAILURE_INVALID_VERSION,		/**< Invalid capwap version. */
	NSS_CAPWAPMGR_FAILURE_IP_DESTROY_RULE,		/**< Destroy IP rule failed. */
	NSS_CAPWAPMGR_FAILURE_CAPWAP_DESTROY_RULE,	/**< Destroy capwap rule failed. */
	NSS_CAPWAPMGR_FAILURE_INVALID_TYPE_FLAG,	/**< Invalid type. */
	NSS_CAPWAPMGR_FAILRUE_INTERNAL_DECAP_NETDEV_ALLOC_FAILED,
							/**< Internal DL netdevice alloc failed. */
	NSS_CAPWAPMGR_FAILRUE_INTERNAL_ENCAP_NETDEV_ALLOC_FAILED,
  							/**< Internal UL netdevice alloc failed. */
	NSS_CAPWAPMGR_FAILURE_DECAP_VP_ALLOC,		/**< DL PPE VP alloc failed. */
	NSS_CAPWAPMGR_FAILURE_ENCAP_VP_ALLOC,		/**< UL PPE VP alloc failed. */
	NSS_CAPWAPMGR_FAILURE_VP_FREE,			/**<PPE VP free failed. */
	NSS_CAPWAPMGR_FAILURE_VP_MTU_SET,		/**< PPE VP MTU set failed. */
	NSS_CAPWAPMGR_FAILURE_UPDATE_VP_NUM,	/**< Update VP number failed. */
	NSS_CAPWAPMGR_INVALID_NETDEVICE,		/**< Invalid CAPWAP netdevice. */
	NSS_CAPWAPMGR_FAILURE_CONFIGURE_DSCP_MAP,	/**< Failed to configure dscp_map. */
	NSS_CAPWAPMGR_FAILURE_CREATE_UDF_PROFILE,	/**< Failed creating user defined profile. */
	NSS_CAPWAPMGR_FAILURE_ACL_RULE_ALREADY_EXIST,	/**< ACL rule already exist. */
	NSS_CAPWAPMGR_FAILURE_ADD_ACL_RULE,		/**< Failed adding ACL rule. */
	NSS_CAPWAPMGR_FAILURE_BIND_ACL_LIST,		/**< Failed to bind ACL list. */
	NSS_CAPWAPMGR_FAILURE_UNBIND_ACL_LIST,		/**< Failed to unbind ACL list. */
	NSS_CAPWAPMGR_FAILURE_ACL_UNAVAILABLE,		/**< ACL rule unavailable. */
	NSS_CAPWAPMGR_FAILURE_MEM_UNAVAILABLE,		/**< Failed to alloc memory. */
	NSS_CAPWAPMGR_FAILURE_DSCP_RULE_ID_INVALID,	/**< DSCP rule ID invalid. */
	NSS_CAPWAPMGR_FAILURE_DSCP_RULE_ID_NOT_IN_USE,	/**< DSCP rule not in use. */
	NSS_CAPWAPMGR_FAILURE_DSCP_RULE_DELETE_FAILED,	/**< DSCP rule delete failed. */
	NSS_CAPWAPMGR_FAILURE_CONFIG_TRUSTSEC_RX,	/**< Failed to configure trustsec receive node. */
	NSS_CAPWAPMGR_FAILURE_BIND_ACL_RULE,		/**< Failed to bind the acl to the physical port. */
	NSS_CAPWAPMGR_FAILURE_BIND_VPORT,		/**< Failed to bind the virtual port to the physical port. */
	NSS_CAPWAPMGR_FAILURE_UNBIND_VPORT,		/**< Failed to unbind the virtual port from the physical port. */
	NSS_CAPWAPMGR_FAILURE_TRUSTSEC_RULE_EXISTS,	/**< TrustSec rule already exists. */
	NSS_CAPWAPMGR_FAILURE_TX_PORT_GET,		/**< Failed to get the physical port associated to the UL virtual port. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_ID_SET,		/**< Failed to set UL tunnel id. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_ID_GET,		/**< Failed to get the tunnel id associated to the UL virtual port. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_ENCAP_ENTRY_ADD,	/**< Failed to add tunnel encap entry. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_ENCAP_ENTRY_GET,	/**< Failed to get tunnel encap entry. */
	NSS_CAPWAPMGR_FAILURE_TUNNEL_ENCAP_ENTRY_DELETE,	/**< Failed to delete tunnel encap entry. */
	NSS_CAPWAPMGR_FAILURE_TRUSTSEC_VP_NUM_UPDATE,	/**< Failed to update TrustSec virtual port number. */
	NSS_CAPWAPMGR_FAILURE_CONFIGURE_TRUSTSEC_TX,	/**< Failed to configure TrustSec Tx rule. */
	NSS_CAPWAPMGR_FAILURE_DSCP_ACL_INIT,		/**< Failed to initialize DSCP ACL related objects. */
	NSS_CAPWAPMGR_FAILURE_LEGACY_MAX,		/**< Max legacy status */
	NSS_CAPWAPMGR_ERROR_NULL_WAN_NDEV,		/**< NULL WAN net device */
	NSS_CAPWAPMGR_ERROR_NULL_TOP_NDEV,		/**< NULL top net device */
	NSS_CAPWAPMGR_ERROR_UNSUPPORTED_TUPLE_TYPE,	/**< Unsupported tuple type */
	NSS_CAPWAPMGR_ERROR_UNSUPPORTED_L4_PROTO,	/**< Unsupported l4 protocol */
	NSS_CAPWAPMGR_ERROR_TUN_ALLOC,			/**< Memory allocation for tunnel ctx failed */
	NSS_CAPWAPMGR_ERROR_TUN_FREE,			/**< Tunnel free failed in offload engine */
	NSS_CAPWAPMGR_ERROR_FLOW_RULE_CREATE,		/**< Flow rule create failed */
	NSS_CAPWAPMGR_ERROR_FLOW_RULE_DESTROY,		/**< Flow rule destroy failed */
	NSS_CAPWAPMGR_ERROR_TUNID_ADD,			/**< Add tunnel under tunid dev */
	NSS_CAPWAPMGR_ERROR_TUNID_DEL,			/**< Delete tunnel under tunid dev */
	NSS_CAPWAPMGR_ERROR_TUNNEL_CONTEXT_GET,		/**< Failed to get tunnel context */
	NSS_CAPWAPMGR_ERROR_TUNID_FREE,			/**< Failed to free tunid capwap dev */
	NSS_CAPWAPMGR_ERROR_DTLS_ALLOC,			/**< Failed to allocated DTLS tunnel */
	NSS_CAPWAPMGR_ERROR_DTLS_BIND,			/**< Failed to Bind DTLS Net Device to capwap Net Device */
	NSS_CAPWAPMGR_ERROR_STATS_GET,	 		/**< Failed to get tunnel stats */
	NSS_CAPWAPMGR_ERROR_INVALID_CFG, 		/**< Invalid tunnel create configuration */
	NSS_CAPWAPMGR_ERROR_TUN_ENABLED,	 	/**< Updating tunnel config when tunnel is enabled */
	NSS_CAPWAPMGR_ERROR_TUN_DEINIT,			/**< Failed to Deinitialize the tunnel */
	NSS_CAPWAPMGR_ERROR_TUN_INIT,			/**< Failed to Initialize the tunnel */
	NSS_CAPWAPMGR_ERROR_DTLS_CFG, 			/**< Invalid DTLS configuration */
	NSS_CAPWAPMGR_ERROR_DTLS_SESSION_SWITCH,	/**< DTLS Session Switch failed */
	NSS_CAPWAPMGR_ERROR_DTLS_DECAP_SESSION_ADD,	/**< DTLS Encap Session Add Failed */
	NSS_CAPWAPMGR_ERROR_DTLS_ENCAP_SESSION_ADD,	/**< DTLS Decap session Add Failed */
	NSS_CAPWAPMGR_ERROR_DTLS_TUN_NOT_CONFIGURED,	 /**< DTLS Tunnel is not configured */
	NSS_CAPWAPMGR_ERROR_DTLS_ENABLED, 		/**< DTLS Tunnel enabled */
	NSS_CAPWAPMGR_ERROR_TUNID_INACTIVE, 		/**< Tunnel ID inactive */
	NSS_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE,		/**< Tunnel ID out of range */
	NSS_CAPWAPMGR_ERROR_TUNID_ACTIVE,		/**< Tunnel Id Active */
	NSS_CAPWAPMGR_ERROR_FLOW_COOKIE_DB_ALLOC,	/**< Failed to instantiate the Flow Cookie DataBase */
	NSS_CAPWAPMGR_ERROR_FLOW_COOKIE_ADD, 		/**< Failed to add the Flow Cookie in Flow Cookie DataBase */
	NSS_CAPWAPMGR_ERROR_FLOW_COOKIE_DEL,		/**< Failed to delete the Flow Cookie from Flow Cookie DataBase */
	NSS_CAPWAPMGR_ERROR_CAPWAP_CFG,			/**< Invalid CAPWAP config. */
	NSS_CAPWAPMGR_ERROR_MAX,
} nss_capwapmgr_status_t;

/**
 * nss_capwap_pn_stats
 *	capwap offload statistics (64-bit version).
 */
struct nss_capwap_pn_stats {
	uint64_t rx_packets;		/**< Number of packets received. */
	uint64_t rx_bytes;		/**< Number of bytes received. */
	uint64_t rx_dropped;		/**< Number of dropped Rx packets. */
	uint64_t rx_errors;             /**< Total rx error. */
	uint64_t tx_packets;		/**< Number of packets transmitted. */
	uint64_t tx_bytes;		/**< Number of bytes transmitted. */
	uint64_t tx_dropped;		/**< Number of dropped Tx packets. */
	uint64_t tx_errors;             /**< Total tx error. */
};

/**
 * nss_capwap_tunnel_stats
 *	Per-tunnel statistics seen by the HLOS.
 */
struct nss_capwap_tunnel_stats {
	struct nss_capwap_pn_stats pnode_stats;	/**< Common offload statistics. */
	uint64_t dtls_pkts;		/**< Number of DTLS packets flowing through. */

	/*
	 * Rx/decap stats
	 */
	uint64_t rx_dup_frag;		/**< Number of duplicate fragments. */
	uint64_t rx_segments;		/**< Number of segments or fragments. */

	/**
	 * Packets dropped because they are larger than the payload size.
	 */
	uint64_t rx_oversize_drops;

	uint64_t Reserved0;
			/**< Reserved */
	uint64_t Reserved1;
			/**< Reserved */
	uint64_t Reserved2;
			/**< Reserved */
	uint64_t rx_csum_drops;
			/**< Packets dropped because of a checksum mismatch. */
	uint64_t rx_malformed;
			/**< Packets dropped because of a malformed packet. */
	uint64_t Reserved3;
			/**< Reserved */
	uint64_t rx_frag_gap_drops;
			/**< Packets dropped because of a non-sequential fragment offset. */

	/*
	 * Tx/encap stats
	 */
	uint64_t Reserved4;
			/**< Reserved */
	uint64_t tx_segments;		/**< Number of segments or fragments. */
	uint64_t tx_queue_full_drops;
			/**< Packets dropped because the queue is full. */
	uint64_t tx_mem_failure_drops;
			/**< Packets dropped because of a memory failure. */

	uint64_t Reserved5;
			/**< Reserved */
	uint64_t tx_dropped_ver_mis;
			/**< Packets dropped because of a version mismatch. */
	uint64_t Reserved6;
			/**< Reserved. */
	uint64_t tx_dropped_hroom;
			/**< Packets dropped because of insufficent headroom. */
	uint64_t tx_dropped_dtls;
			/**< Packets dropped because of a DTLS packet. */
	uint64_t tx_dropped_nwireless;
			/**< Packets dropped because the nwireless field information is wrong. */

	uint32_t Reserved7;
			/**< Reserved. */
	uint64_t rx_control_pkts;
			/**< Number of control packets. */
	uint64_t rx_keepalive_pkts;
			/**< Keep alive packets received. */
	uint64_t rx_fast_reasm;
			/**< Fast re-assembly hits*/
	uint64_t rx_slow_reasm;
			/**< Slow re-assembly hits*/
	uint64_t rx_drop_dec_failure;
			/**< Failed during decapsulation.*/
	uint64_t rx_drop_max_frags;
			/**< Exceeds max fragments allowed (10). */
	uint64_t rx_drop_missing_frags;
			/**< Re-assembly failure because of missing fragments. */
	uint64_t tx_keepalive_pkts;
			/**< Keep alive packets received by offload engine. */
};

/**
 * NSS DTLS manager flags
 */
#define NSS_DTLSMGR_HDR_IPV6		NETFN_DTLS_FLAG_IPV6	/**< L3 header is v6 or v4 */
#define NSS_DTLSMGR_HDR_UDPLITE		NETFN_DTLS_FLAG_UDPLITE	/**< L4 header is UDP-Lite or UDP */
#define NSS_DTLSMGR_HDR_CAPWAP		NETFN_DTLS_FLAG_CAPWAP	/**< CAPWAP-DTLS or DTLS header */
#define NSS_DTLSMGR_ENCAP_UDPLITE_CSUM	NETFN_DTLS_FLAG_UDPLITE_CSUM /**< UDPlite only header checksum */

/**
 * DTLS protocol version
 */
enum nss_dtlsmgr_dtlsver {
	NSS_DTLSMGR_VERSION_1_0,	/**< Protocol v1.0. */
	NSS_DTLSMGR_VERSION_1_2,	/**< Protocol v1.2. */
};

/**
 * NSS DTLS manager supported cryptographic algorithms
 */
enum nss_dtlsmgr_algo {
	NSS_DTLSMGR_ALGO_AES_CBC_SHA1_HMAC,	/**< AES_CBC_SHA1_HMAC. */
	NSS_DTLSMGR_ALGO_AES_CBC_SHA256_HMAC,	/**< AES_CBC_SHA256_HMAC. */
	NSS_DTLSMGR_ALGO_3DES_CBC_SHA1_HMAC,	/**< 3DES_CBC_SHA1_HMAC. */
	NSS_DTLSMGR_ALGO_3DES_CBC_SHA256_HMAC,	/**< 3DES_CBC_SHA256_HMAC. */
	NSS_DTLSMGR_ALGO_AES_GCM,		/**< AES_GCM. */
	NSS_DTLSMGR_ALGO_MAX
};

/**
 * NSS DTLS manager cryptographic structure to represent key and its length.
 */
struct nss_dtlsmgr_crypto_data {
	const uint8_t *data;		/**< Pointer to key or nonce. */
	uint16_t len;			/**< Length of the key. */
};

/**
 * NSS DTLS manager cryptographic data
 */
struct nss_dtlsmgr_crypto {
	enum nss_dtlsmgr_algo algo;			/**< DTLS manager cryptographic algorithm. */
	struct nss_dtlsmgr_crypto_data cipher_key;	/**< Cipher key. */
	struct nss_dtlsmgr_crypto_data auth_key;	/**< Authentication key. */
	struct nss_dtlsmgr_crypto_data nonce;		/**< Nonce. */
};

/**
 * NSS DTLS manager session encapsulation data
 */
struct nss_dtlsmgr_encap_config {
	struct nss_dtlsmgr_crypto crypto;	/**< Encapsulation crypto configuration. */
	enum nss_dtlsmgr_dtlsver ver;		/**< Version used in DTLS header. */
	uint32_t sip[4];			/**< Source IP address. */
	uint32_t dip[4];			/**< Destination IP address. */
	uint16_t sport;				/**< Source UDP port. */
	uint16_t dport;				/**< Destination UDP port. */
	uint16_t epoch;				/**< Epoch. */
	uint8_t ip_ttl;				/**< IP time to live. */
	uint8_t dscp;				/**< DSCP. */
	bool dscp_copy;				/**< Flag to check if DSCP needs to be copied. */
	bool df;				/**< Flag to check fragmentation. */
};

/**
 * NSS DTLS manager session decapsulation data
 */
struct nss_dtlsmgr_decap_config {
	struct nss_dtlsmgr_crypto crypto;	/**< Decap Crypto configuration. */
	uint32_t nexthop_ifnum;			/**< NSS I/F number to forward after de-capsulation. */
	uint16_t window_size;			/**< Anti-Replay window size. */
};

/**
 * NSS DTLS manager session definition
 */
struct nss_dtlsmgr_config {
	uint32_t flags;					/**< DTLS header flags. */

	struct nss_dtlsmgr_encap_config encap;		/**< Encap data. */
	struct nss_dtlsmgr_decap_config decap;		/**< Decap data. */
};

/**
 * NSS DTLS manager session tx/rx cipher update parameters
 */
struct nss_dtlsmgr_config_update {
	struct nss_dtlsmgr_crypto crypto;	/**< Crypto algorithm and key data. */
	uint16_t epoch;				/**< Epoch. */
	uint16_t window_size;			/**< Anti-Replay window size. */
};

/*
 * netfn_capwapmgr_legacy2crypto
 *	API to convert legacy crypto config to netfn crypto.
 */
extern netfn_capwapmgr_ret_t netfn_capwapmgr_legacy2crypto(struct netfn_dtls_crypto *netfn_crypto, struct nss_dtlsmgr_crypto *legacy_crypto);

/*
 * netfn_capwapgr_legacy2dtls.
 *	API to convert legacy DTLS config to netfn config.
 */
extern netfn_capwapmgr_ret_t netfn_capwapmgr_legacy2dtls(struct nss_dtlsmgr_config *dtls_data, struct netfn_dtls_cfg *enc, struct netfn_dtls_cfg *dec);

/*
 * netfn_capwapmgr_legacy_rule2tun_cfg()
 *	API to convert legacy rules to netfn rules.
 */
extern netfn_capwapmgr_ret_t netfn_capwapmgr_legacy_rule2tun_cfg(struct nss_ipv4_create *v4, struct nss_ipv6_create *v6, struct nss_capwap_rule_msg *capwap_rule, struct nss_dtlsmgr_config *dtls_data, struct netfn_capwapmgr_tun_cfg *cfg, uint8_t tunnel_id);

/*
 * netfn_capwapmgr_stats2legacy_stats()
 *	API to convert netfn stats to legacy stats.
 */
extern void netfn_capwapmgr_stats2legacy_stats(struct netfn_capwap_tun_stats *netfn_stats, struct nss_capwap_tunnel_stats *legacy_stats);

/*
 * netfn_capwapmgr_status2legacy_status()
 *	API to convert netfn status to legacy status.
 */
extern nss_capwapmgr_status_t netfn_capwapmgr_status2legacy_status(netfn_capwapmgr_ret_t netfn_status);

/**
 * nss_capwapmgr_netdev_create
 *	Creates a CAPWAP netdevice.
 *
 * @return
 * Pointer to a newly created netdevice.
 *
 * @note
 * First CAPWAP interface name is capwap0 and so on.
 */
static inline struct net_device *nss_capwapmgr_netdev_create(void)
{
	struct net_device *dev = NULL;

	dev =  netfn_capwapmgr_tunid_dev_alloc();

	return dev;
}

/**
 * nss_capwapmgr_ipv4_tunnel_create
 *	Creates an IPv4 CAPWAP tunnel.
 *
 * @datatypes
 * net_device \n
 * nss_ipv4_create \n
 * nss_capwap_rule_msg \n
 * nss_dtlsmgr_config
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] IPv4	IPv4 rule structure.
 * @param[in] CAPWAP	CAPWAP rule structure.
 * @param[in] DTLS	DTLS config data.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_ipv4_tunnel_create(struct net_device *dev, uint8_t tunnel_id,
			struct nss_ipv4_create *ip_rule, struct nss_capwap_rule_msg *capwap_rule, struct nss_dtlsmgr_config *dtls_data)
{
	struct netfn_capwapmgr_tun_cfg netfn_cfg = {0};
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;

	/*
	 * Convert legacy rules to netfn rules.
	 */
	netfn_capwapmgr_legacy_rule2tun_cfg(ip_rule, NULL, capwap_rule, dtls_data, &netfn_cfg, tunnel_id);

	/*
	 * TODO: Move to RCU lock to protect tunid object.
	 */
	netfn_ret = netfn_capwapmgr_tunid_add(dev, tunnel_id, &netfn_cfg);

	/*
	 * Convert the return status from netfn to legacy.
	 */
	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_ipv6_tunnel_create
 *	Creates an IPv6 CAPWAP tunnel.
 *
 * @datatypes
 * net_device \n
 * nss_ipv6_create \n
 * nss_capwap_rule_msg \n
 * nss_dtlsmgr_config
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of thethe tunnel.
 * @param[in] IPv6	IPv6 rule structure.
 * @param[in] CAPWAP	CAPWAP rule structure.
 * @param[in] DTLS	DTLS config data.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_ipv6_tunnel_create(struct net_device *dev, uint8_t tunnel_id,
			struct nss_ipv6_create *ip_rule, struct nss_capwap_rule_msg *capwap_rule, struct nss_dtlsmgr_config *dtls_data)
{
	struct netfn_capwapmgr_tun_cfg netfn_cfg = {0};
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;

	/*
	 * Convert legacy rules to netfn rules.
	 */
	netfn_capwapmgr_legacy_rule2tun_cfg(NULL, ip_rule, capwap_rule, dtls_data, &netfn_cfg, tunnel_id);

	/*
	 * TODO: Move to RCU lock to protect tunid object.
	 */
	netfn_ret = netfn_capwapmgr_tunid_add(dev, tunnel_id, &netfn_cfg);

	/*
	 * Convert the return status from netfn to legacy.
	 */
	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_enable_tunnel
 *	Enable a CAPWAP tunnel.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of thethe tunnel.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_enable_tunnel(struct net_device *dev, uint8_t tunnel_id)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;

	netfn_ret = netfn_capwapmgr_tunid_toggle_state(dev, tunnel_id, true);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_disable_tunnel
 *	Enable a CAPWAP tunnel.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_disable_tunnel(struct net_device *dev, uint8_t tunnel_id)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;

	netfn_ret = netfn_capwapmgr_tunid_toggle_state(dev, tunnel_id, false);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 *nss_capwapmgr_update_path_mtu
 *	Updates Path MTU of a CAPWAP tunnel.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] mtu	New path MTU.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_update_path_mtu(struct net_device *dev, uint8_t tunnel_id, uint32_t mtu)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_UPDATE_MTU;
	cfg.update_cfg.mtu = mtu;

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_update_dest_mac_addr
 *	Updates Destination MAC Address of a CAPWAP tunnel.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] mac_addr	New MAC Address.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_update_dest_mac_addr(struct net_device *dev, uint8_t tunnel_id, uint8_t *mac_addr)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_UPDATE_DEST_MAC;
	memcpy(cfg.update_cfg.dest_mac, mac_addr, ETH_ALEN);

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_update_src_interface
 *	Updates Source Interface number.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] src_interface_number	New interface number.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_update_src_interface(struct net_device *dev, uint8_t tunnel_id, int32_t src_interface_num)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_UPDATE_SRC_INTERFACE;
	cfg.update_cfg.dev = ppe_drv_dev_get_by_iface_idx(src_interface_num);

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_change_version
 *	Changes version of a CAPWAP tunnel.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] ver	CAPWAP version.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_change_version(struct net_device *dev, uint8_t tunnel_id, uint8_t ver)
{
	return NSS_CAPWAPMGR_SUCCESS;
}

/**
 * nss_capwapmgr_tunnel_destroy
 *	Destroy a CAPWAP tunnel.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 *
 * @return
 * nss_capwapmgr_status_t
 *
 * @note
 * CAPWAP tunnel must be disabled before destroy operation.
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_tunnel_destroy(struct net_device *dev, uint8_t tunnel_id)
{
	/*
	 * TODO: Use RCU locks to protect tunid object.
	 */
	netfn_capwapmgr_ret_t netfn_ret = netfn_ret = netfn_capwapmgr_tunid_del(dev, tunnel_id);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_netdev_destroy
 *	Destroy a netdevice.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 *
 * @return
 * nss_capwapmgr_status_t
 *
 * @note
 * CAPWAP tunnel must be destroyed first.
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_netdev_destroy(struct net_device *netdev)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;

	netfn_ret = netfn_capwapmgr_tunid_dev_free(netdev);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_tunnel_stats
 *	Gets CAPWAP tunnel stats.
 *
 * @datatypes
 * net_device \n
 * nss_capwap_tunnel_stats
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] pointer	to struct nss_capwap_tunnel_stats
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_tunnel_stats(struct net_device *dev, uint8_t tunnel_id,
							struct nss_capwap_tunnel_stats *stats)
{
	struct netfn_capwap_tun_stats netfn_stats = {0};

	netfn_capwapmgr_stats2legacy_stats(&netfn_stats, stats);

	return NSS_CAPWAPMGR_SUCCESS;
}

/**
 * nss_capwapmgr_get_dtls_netdev
 *	Get the DTLS net_device associated to the CAPWAP tunnel
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice
 * @param[in] tunnel_id
 *
 * @return
 * Pointer to struct net_device
 *
 * @note This API hold the NET_DEVICE reference; after use the caller must perform
 * "dev_put" to release the reference.
 */
static inline struct net_device *nss_capwapmgr_get_dtls_netdev(struct net_device *dev, uint8_t tunnel_id)
{
	return netfn_capwapmgr_tunid_get_dtls_dev(dev, tunnel_id);
}

/**
 * nss_capwapmgr_configure_dtls
 *	Configure dtls settings of a capwap tunnel.
 *
 * @datatypes
 * net_device \n
 * nss_dtlsmgr_config
 *
 * @param[in] netdevice.
 * @param[in] tunnel_id.
 * @param[in] enable or disable
 * @param[in] dtls configuration
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_configure_dtls(struct net_device *dev, uint8_t tunnel_id,
		uint8_t enable_dtls, struct nss_dtlsmgr_config *in_data)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	if (enable_dtls) {
		cfg.type = NETFN_CAPWAPMGR_UPDATE_DTLS_ENABLE;
		netfn_capwapmgr_legacy2dtls(in_data, &cfg.update_cfg.dtls.enc, &cfg.update_cfg.dtls.dec);
	} else {
		cfg.type = NETFN_CAPWAPMGR_UPDATE_DTLS_DISABLE;
	}

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_dtls_rekey_rx_cipher_update
 *	RX cipher update for a CAPWAP DTLS tunnel
 *
 * @datatypes
 * net_device \n
 * nss_dtlsmgr_config_update
 *
 * @param[in] netdevice
 * @param[in] tunnel_id
 * @param[in] dtls configuration update
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_dtls_rekey_rx_cipher_update(struct net_device *dev, uint8_t tunnel_id,
		struct nss_dtlsmgr_config_update *udata)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_UPDATE_DTLS_DECAP_SESSION;
	netfn_capwapmgr_legacy2crypto(&cfg.update_cfg.dtls.dec.base, &udata->crypto);

	cfg.update_cfg.dtls.dec.epoch = udata->epoch;
	cfg.update_cfg.dtls.dec.replay_win = udata->window_size;

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_dtls_rekey_tx_cipher_update
 *	TX cipher update for a CAPWAP DTLS tunnel
 *
 * @datatypes
 * net_device \n
 * nss_dtlsmgr_config_update
 *
 * @param[in] netdevice
 * @param[in] tunnel_id
 * @param[in] dtls configuration update
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_dtls_rekey_tx_cipher_update(struct net_device *dev, uint8_t tunnel_id,
		struct nss_dtlsmgr_config_update *udata)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_UPDATE_DTLS_ENCAP_SESSION;
	netfn_capwapmgr_legacy2crypto(&cfg.update_cfg.dtls.enc.base, &udata->crypto);

	cfg.update_cfg.dtls.enc.epoch = udata->epoch;

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_dtls_rekey_rx_cipher_switch
 *	RX cipher switch for a CAPWAP DTLS tunnel
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice
 * @param[in] tunnel_id
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_dtls_rekey_rx_cipher_switch(struct net_device *dev, uint8_t tunnel_id)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_DTLS_DECAP_SESSION_SWITCH;

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_dtls_rekey_tx_cipher_switch
 *	TX cipher switch for a CAPWAP DTLS tunnel
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice
 * @param[in] tunnel_id
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_dtls_rekey_tx_cipher_switch(struct net_device *dev, uint8_t tunnel_id)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};

	cfg.type = NETFN_CAPWAPMGR_DTLS_ENCAP_SESSION_SWITCH;

	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/*
 * Flow rule add types. Mutually exclusive fields.
 * This indicates whether SCS or SDWF ID is configured for inner packet lookup.
 */
#define NSS_CAPWAP_FLOW_ATTR_SCS_VALID NETFN_CAPWAP_PKT_TYPE_SCS_ID_VALID
			/**< SCS Identification valid in flow attributes. */
#define NSS_CAPWAP_FLOW_ATTR_SDWF_VALID NETFN_CAPWAP_PKT_TYPE_SDWF_ID_VALID
			/**< SDWF Identification valid in flow attributes. */

/**
 * nss_capwap_flow_attr
 *	Inner Flow attributes.
 */
struct nss_capwap_flow_attr {
	uint8_t type;			/**< Type to indicate if SCS is valid or SAWF is valid. */
	uint32_t flow_id;		/**< Flow Identification. */
	uint32_t scs_sdwf_id;		/**< SCS or SDWF Identification. */
};

/**
 * nss_capwapmgr_flow_info
 *	Inner flow information.
 */
struct nss_capwapmgr_flow_info {
	uint16_t ip_version;	/**< IP version. */
	uint16_t protocol;	/**< Protocol. */
	uint32_t src_ip[4];	/**< Source IP address. */
	uint32_t dst_ip[4];	/**< Destination IP address. */
	uint16_t src_port;	/**< Source port. */
	uint16_t dst_port;	/**< Destination port. */
	struct nss_capwap_flow_attr flow_attr;
				/**< Flow attributes. */
};

/**
 * nss_capwapmgr_add_flow_rule
 *	Send a flow rule add message to NSS.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] flow_info	Flow information.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_add_flow_rule(struct net_device *dev, uint8_t tunnel_id, struct nss_capwapmgr_flow_info *flow_info)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};
	netfn_tuple_t *tuple;
	struct netfn_flow_cookie *nfc;

	cfg.type = NETFN_CAPWAPMGR_ADD_NETFN_FLOW_COOKIE;

	tuple = &cfg.update_cfg.fci.tuple;
	tuple->tuple_type = NETFN_TUPLE_5TUPLE;

	if (flow_info->ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		tuple->ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V4;
		tuple->tuples.tuple_5.src_ip.ip4.s_addr = htonl(flow_info->src_ip[0]);
		tuple->tuples.tuple_5.dest_ip.ip4.s_addr = htonl(flow_info->dst_ip[0]);
		tuple->tuples.tuple_5.l4_src_ident = htons(flow_info->src_port);
		tuple->tuples.tuple_5.l4_dest_ident = htons(flow_info->dst_port);
		tuple->tuples.tuple_5.protocol = (uint8_t)flow_info->protocol;
	} else {
		tuple->ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V6;

		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[0] = htonl(flow_info->src_ip[0]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[1] = htonl(flow_info->src_ip[1]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[2] = htonl(flow_info->src_ip[2]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[3] = htonl(flow_info->src_ip[3]);
		tuple->tuples.tuple_5.l4_src_ident = htons(flow_info->src_port);

		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[0] = htonl(flow_info->dst_ip[0]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[1] = htonl(flow_info->dst_ip[1]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[2] = htonl(flow_info->dst_ip[2]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[3] = htonl(flow_info->dst_ip[3]);
		tuple->tuples.tuple_5.l4_dest_ident = htons(flow_info->dst_port);

		tuple->tuples.tuple_5.protocol = (uint8_t)flow_info->protocol;
	}

	nfc = &cfg.update_cfg.fci.nfc;
	nfc->valid_flag = flow_info->flow_attr.type;
	nfc->flow_id = flow_info->flow_attr.flow_id;
	nfc->scs_sdwf_hdl = flow_info->flow_attr.scs_sdwf_id;
	rtnl_lock();
	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);
	rtnl_unlock();

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}

/**
 * nss_capwapmgr_del_flow_rule
 *	Send a flow rule delete message to NSS.
 *
 * @datatypes
 * net_device
 *
 * @param[in] netdevice	CAPWAP netdevice.
 * @param[in] tunnel_id	Tunnel ID of the tunnel.
 * @param[in] flow_info Flow information.
 *
 * @return
 * nss_capwapmgr_status_t
 */
static inline nss_capwapmgr_status_t nss_capwapmgr_del_flow_rule(struct net_device *dev, uint8_t tunnel_id, struct nss_capwapmgr_flow_info *flow_info)
{
	netfn_capwapmgr_ret_t netfn_ret = NETFN_CAPWAPMGR_SUCCESS;
	struct netfn_capwapmgr_tun_update cfg = {0};
	netfn_tuple_t *tuple;

	cfg.type = NETFN_CAPWAPMGR_DEL_NETFN_FLOW_COOKIE;

	tuple = &cfg.update_cfg.fci.tuple;
	tuple->tuple_type = NETFN_TUPLE_5TUPLE;

	if (flow_info->ip_version == NETFN_FLOWMGR_TUPLE_IP_VERSION_V4) {
		tuple->ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V4;
		tuple->tuples.tuple_5.src_ip.ip4.s_addr = htonl(flow_info->src_ip[0]);
		tuple->tuples.tuple_5.dest_ip.ip4.s_addr = htonl(flow_info->dst_ip[0]);
		tuple->tuples.tuple_5.l4_src_ident = htons(flow_info->src_port);
		tuple->tuples.tuple_5.l4_dest_ident = htons(flow_info->dst_port);
		tuple->tuples.tuple_5.protocol = (uint8_t)flow_info->protocol;
	} else {
		tuple->ip_version = NETFN_FLOWMGR_TUPLE_IP_VERSION_V6;

		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[0] = htonl(flow_info->src_ip[0]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[1] = htonl(flow_info->src_ip[1]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[2] = htonl(flow_info->src_ip[2]);
		tuple->tuples.tuple_5.src_ip.ip6.s6_addr32[3] = htonl(flow_info->src_ip[3]);
		tuple->tuples.tuple_5.l4_src_ident = htons(flow_info->src_port);

		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[0] = htonl(flow_info->dst_ip[0]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[1] = htonl(flow_info->dst_ip[1]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[2] = htonl(flow_info->dst_ip[2]);
		tuple->tuples.tuple_5.dest_ip.ip6.s6_addr32[3] = htonl(flow_info->dst_ip[3]);
		tuple->tuples.tuple_5.l4_dest_ident = htons(flow_info->dst_port);

		tuple->tuples.tuple_5.protocol = (uint8_t)flow_info->protocol;
	}

	rtnl_lock();
	netfn_ret = netfn_capwapmgr_tunid_update(dev, tunnel_id, &cfg);
	rtnl_unlock();

	return netfn_capwapmgr_status2legacy_status(netfn_ret);
}



#if defined(NETFN_CAPWAPMGR_ONE_NETDEV)
/**
 * nss_capwapmgr_get_netdev
 *	Returns netdevice used by NSS CAPWAP manager
 *
 *@param void
 *
 *@return Pointer to struct net_device
 */
extern struct net_device *nss_capwapmgr_get_netdev(void);
#endif /* NETFN_CAPWAPMGR_ONE_NETDEV */

#endif /* __NSS_CAPWAPMGR_LEGACY_H */
