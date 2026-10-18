/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _SP_API_H
#define _SP_API_H

#include <linux/types.h>

/*
 * sp_rule_match_flag
 * 	Rule match mask
 */
#define	SP_RULE_FLAG_MATCH_ALWAYS_TRUE		0x01		/* Rule match always true. s(skip field matching) mask */
#define	SP_RULE_FLAG_MATCH_UP			0x02		/* Match up in 802.11 qos control mask */
#define	SP_RULE_FLAG_MATCH_UP_SENSE		0x04 		/* UP in 802.11 qos control match sense mask */
#define	SP_RULE_FLAG_MATCH_SOURCE_MAC		0x08		/* Match source mac address mask */
#define	SP_RULE_FLAG_MATCH_SOURCE_MAC_SENSE	0x10		/* Match source mac address mask */
#define	SP_RULE_FLAG_MATCH_DST_MAC		0x20		/* Match destination mac address mask */
#define	SP_RULE_FLAG_MATCH_DST_MAC_SENSE	0x40		/* Destination mac address match sense mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV4		0x80		/* Match source ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV4_SENSE	0x100		/* Source ipv4 address sense mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV6		0x200		/* Match source ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_SRC_IPV6_SENSE	0x400		/* Source ipv6 address sense mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV4		0x800		/* Match destination ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV4_SENSE	0x1000		/* Destination ipv4 address sense mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV6		0x2000		/* Match destination ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_DST_IPV6_SENSE	0x4000		/* Destination ipv6 address sense mask */
#define	SP_RULE_FLAG_MATCH_SRC_PORT		0x8000		/* Match source port match mask */
#define	SP_RULE_FLAG_MATCH_SRC_PORT_SENSE	0x10000		/* Source port sense mask */
#define	SP_RULE_FLAG_MATCH_DST_PORT		0x20000		/* Match destination port match mask */
#define	SP_RULE_FLAG_MATCH_DST_PORT_SENSE	0x40000		/* Destination port sense mask */
#define	SP_RULE_FLAG_MATCH_PROTOCOL		0x80000		/* Match protocol match mask */
#define	SP_RULE_FLAG_MATCH_PROTOCOL_SENSE	0x100000	/* Protocol sense mask */
#define	SP_RULE_FLAG_MATCH_VLAN_ID		0x200000	/* Match VLAN id match mask */
#define	SP_RULE_FLAG_MATCH_VLAN_ID_SENSE	0x400000	/* VLAN id sense mask */
#define	SP_RULE_FLAG_MATCH_DSCP			0x800000	/* Match dscp match mask */
#define	SP_RULE_FLAG_MATCH_DSCP_SENSE		0x1000000	/* DSCP sense mask */

/*
 * sp_rule_match_flag_sawf
 * 	Rule match mask for SAWF
 */
#define	SP_RULE_FLAG_MATCH_SAWF_SOURCE_MAC		0x01		/* Match sawf source mac address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_MAC			0x02		/* Match sawf destination mac address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4		0x04		/* Match sawf source ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6		0x08		/* Match sawf source ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV4		0x10		/* Match sawf destination ipv4 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV6		0x20		/* Match sawf destination ipv6 address match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_PORT		0x40		/* Match sawf source port match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_PORT		0x80		/* Match sawf destination port match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_PROTOCOL		0x100		/* Match sawf protocol match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_VLAN_ID			0x200		/* Match sawf VLAN id match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DSCP			0x400		/* Match sawf dscp match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP		0x800		/* Sawf Vlan priority match mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV4_MASK		0x1000		/* Sawf Source ipv4 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV4_MASK		0x2000		/* Sawf Destination ipv4 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_IPV6_MASK		0x4000		/* Sawf Source ipv6 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_IPV6_MASK		0x8000		/* Sawf Destination ipv6 address mask */
#define	SP_RULE_FLAG_MATCH_SAWF_DSCP_REMARK		0x10000		/* Sawf Match dscp remark mask */
#define	SP_RULE_FLAG_MATCH_SAWF_VLAN_PCP_REMARK		0x20000		/* Sawf Vlan priority remark mask */
#define	SP_RULE_FLAG_MATCH_SAWF_IP_VERSION_TYPE		0x40000		/* Sawf IP version type match mask */

/*
 * sp_rule_match_flag_sawf
 * 	Rule match mask for SAWF
 */
#define	SP_RULE_FLAG_MATCH_SCS_SPI			0x80000		/* SCS SPI match mask*/
#define SP_RULE_FLAG_MATCH_MSCS_TID_BITMAP		0x100000	/* MSCS Bitmap match*/
#define SP_RULE_FLAG_MATCH_PRIORITY_LIMIT		0x200000	/* Priority Limit value*/
#define	SP_RULE_FLAG_MATCH_DST_IFINDEX			0x400000	/* Destination Interface Index mask */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_START	0x800000	/* Match sawf source port range start */
#define	SP_RULE_FLAG_MATCH_SAWF_SRC_PORT_RANGE_END	0x1000000	/* Match sawf source port range end */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_START	0x2000000	/* Match sawf destination port range start */
#define	SP_RULE_FLAG_MATCH_SAWF_DST_PORT_RANGE_END	0x4000000	/* Match sawf destination port range end */
#define SP_RULE_FLAG_MATCH_SAWF_SRC_IFACE		0x8000000	/* Match sawf source interface */
#define SP_RULE_FLAG_MATCH_SAWF_DST_IFACE		0x10000000	/* Match sawf destination interface */

#define IPV6_ADDR_LEN		4

#define	SP_RULE_INVALID_SERVICE_CLASS_ID	0xFF		/* Invalid Service class ID */
#define	SP_RULE_INVALID_DSCP_REMARK		0xFF		/* Invalid DSCP remark */
#define	SP_RULE_INVALID_VLAN_PCP_REMARK		0xFF		/* Invalid vlan PCP remark */
#define	SP_RULE_INVALID_RULE_ID			0xFFFFFFFF	/* Invalid Rule ID */
#define	SP_RULE_INVALID_VLAN_TCI		0xFFFF		/* Invalid vlan tci */
#define	SP_RULE_INVALID_PRIORITY		0xFF		/* Invalid Priority value */
#define SP_RULE_INVALID_MSCS_TID_BITMAP		0x00		/* Invalid MSCS Bitmap */
#define SP_RULE_INVALID_IPV4_FRAG_THRESH        0x00            /* Invalid fragmentation threshold */

#define SP_RULE_MAX_VDEV_PER_ML 7
#define WLAN_SSID_MAX_LEN	32	/* Max ssid len */
#define SP_MAPDB_RADIO_FLAG_AND	"AND"	/* Radio mode AND */
#define SP_MAPDB_RADIO_FLAG_OR	"OR"	/* Radio mode OR */
#define SP_RULE_VDEV_BAND_LENGTH	4	/* Max VDEV Band len as string */
#define SP_RULE_VDEV_BANDWIDTH_LENGTH	6	/* Max VDEV Bandwidth len as string */
#define SP_RULE_BAND_VALID	1	/* Indicates valid band */
#define SP_RULE_PCP_VALID	0x4	/* Defined as WLAN driver */
#define SP_RULE_AC_VALID	0x8	/* Defined as WLAN driver */

/*
 * sp_mapdb_radio_band
 * 	WLAN supported bands
 */
enum sp_mapdb_radio_band {
	SP_MAPDB_RADIO_BAND_INVALID,	/* Invalid radio band */
	SP_MAPDB_RADIO_BAND_2G,			/* 2G radio band */
	SP_MAPDB_RADIO_BAND_5G,			/* 5G radio band */
	SP_MAPDB_RADIO_BAND_5GH,		/* 5GH radio band */
	SP_MAPDB_RADIO_BAND_5GL,		/* 5GL radio band */
	SP_MAPDB_RADIO_BAND_6G,			/* 6G radio band */
};

/*
 * sp_mapdb_radio_bandwidth
 * 	WLAN supported radio bandwidth
 */
enum sp_mapdb_radio_bandwidth {
	SP_MAPDB_RADIO_BANDWIDTH_INVALID,		/* Invalid radio bandwidth */
	SP_MAPDB_RADIO_BANDWIDTH_20,		/* Bandwidth 20Mhz */
	SP_MAPDB_RADIO_BANDWIDTH_40,		/* Bandwidth 40Mhz */
	SP_MAPDB_RADIO_BANDWIDTH_80,		/* Bandwidth 80Mhz */
	SP_MAPDB_RADIO_BANDWIDTH_160,		/* Bandwidth 160Mhz */
	SP_MAPDB_RADIO_BANDWIDTH_80_80,		/* Bandwidth 80_80Mhz */
	SP_MAPDB_RADIO_BANDWIDTH_320,		/* Bandwidth 320Mhz */
};

/*
 * sp_mapdb_update_results
 * 	Result values of rule update.
 */
enum sp_mapdb_update_results {
	SP_MAPDB_UPDATE_RESULT_ERR_TBLFULL, 		/* The rule table is full. */
	SP_MAPDB_UPDATE_RESULT_ERR_TBLEMPTY, 		/* The rule table is empty. */
	SP_MAPDB_UPDATE_RESULT_ERR_ALLOCNODE, 		/* Failed at allocating memory for a rule node. (struct sp_mapdb_rule_node) */
	SP_MAPDB_UPDATE_RESULT_ERR_ALLOCHASH, 		/* Failed at allocating memory for a hashentry. */
	SP_MAPDB_UPDATE_RESULT_ERR_RULENOEXIST, 	/* There is no such rule with the given rule id. */
	SP_MAPDB_UPDATE_RESULT_ERR_UNKNOWNBIT, 		/* Unknown add/remove filter bit. */
	SP_MAPDB_UPDATE_RESULT_ERR_SINGLE_WRITER,	/* Single writer protection violation. */
	SP_MAPDB_UPDATE_RESULT_ERR_INVALIDENTRY,	/* Invalid entry of rule field. */
	SP_MAPDB_UPDATE_RESULT_ERR_NEWRULE_NULLPTR,	/* New rule is a null pointer. */
	SP_MAPDB_UPDATE_RESULT_ERR, 			/* Delimiter */
	SP_MAPDB_UPDATE_RESULT_SUCCESS_ADD, 		/* Successful rule add */
	SP_MAPDB_UPDATE_RESULT_SUCCESS_DELETE, 		/* Successful rule deletion */
	SP_MAPDB_UPDATE_RESULT_SUCCESS_MODIFY, 		/* Successful rule modification */
	SP_MAPDB_UPDATE_RESULT_LAST,
};
typedef enum sp_mapdb_update_results sp_mapdb_update_result_t;

/*
 * sp_mapdb_notify_types
 * 	Types of notification.
 */
enum sp_mapdb_notify_types {
	SP_MAPDB_ADD_RULE,				/* Notify rule has been added. */
	SP_MAPDB_REMOVE_RULE,				/* Notify rule has been removed. */
	SP_MAPDB_MODIFY_RULE,				/* Notify rule has been modified. */
};
typedef enum sp_mapdb_notify_types sp_mapdb_notify_type_t;

/*
 * sp_mapdb_prioritization_sync_types
 * 	Types of notification.
 */
enum sp_mapdb_prioritization_sync_types {
	SP_MAPDB_SYNC_PRIORITIZED,			/* Flow is prioritized for the first time */
	SP_MAPDB_SYNC_UPDATED,				/* Flow priotization is updated. */
	SP_MAPDB_SYNC_DEPRIORITIZED,			/* Flow is deprioritized */
};

/*
 * sp_mapdb_add_remove_filter_types
 * 	Possible value of Add-remove filter bit.
 */
enum sp_mapdb_add_remove_filter_types {
	SP_MAPDB_ADD_REMOVE_FILTER_DELETE, 		/* Delete a rule. */
	SP_MAPDB_ADD_REMOVE_FILTER_ADD, 		/* Add a rule. */
	SP_MAPDB_ADD_REMOVE_FILTER_QUERY,		/* Query rule table */
};
typedef enum sp_mapdb_add_remove_filter_types sp_mapdb_add_remove_filter_type_t;

/*
 * sp_rule_ae_type
 *	This enum defines different Ae types.
 */
enum sp_rule_ae_type {
	SP_RULE_AE_TYPE_DEFAULT,			/* ae type default. */
	SP_RULE_AE_TYPE_PPE,				/* ae type ppe. */
	SP_RULE_AE_TYPE_SFE,				/* ae type sfe. */
	SP_RULE_AE_TYPE_PPE_DS,				/* ae type ppe-ds. */
	SP_RULE_AE_TYPE_PPE_VP,				/* ae type ppe-vp. */
	SP_RULE_AE_TYPE_NONE,				/* no ae type. */
};

struct sp_rule_inner {

	/*
	 * Flag bits for rule match
	 */
	uint32_t flags;

	/*
	 * Flag bits for sawf rule match
	 */
	uint32_t flags_sawf;

	/*
	 * The value of rule_output determines how to set the pcp value
	 * to be marked in the matched packet.
	 * Maximum possible value is 7.
	 */
	uint16_t rule_output;

	/*
	 * UP in 802.11 qos control
	 */
	uint8_t user_priority;

	/*
	 * Source mac address
	 * If “match source mac address” flag bit is set,
	 * this field shall be included, otherwise this field shall be omitted.
	 */
	uint8_t sa[ETH_ALEN];

	/*
	 * Destination mac address
	 * If “match destination mac address” flag
	 * bit is set, this field shall be included,
	 * otherwise this field shall be omitted.
	 */
	uint8_t da[ETH_ALEN];

	/*
	 * Source ipv4 address
	 */
	uint32_t src_ipv4_addr;
	/*
	 * Source ipv6 address
	 */
	uint32_t src_ipv6_addr[IPV6_ADDR_LEN];
	/*
	 * Destination ipv4 address
	 */
	uint32_t dst_ipv4_addr;
	/*
	 * Destination ipv6 address
	 */
	uint32_t dst_ipv6_addr[IPV6_ADDR_LEN];
	/*
	 * Source port number
	 */
	uint16_t src_port;
	/*
	 * Destination port number
	 */
	uint16_t dst_port;
	/*
	 * VLAN id
	 */
	uint16_t vlan_id;
	/*
	 * Protocol number
	 */
	uint8_t protocol_number;
	/*
	 * DSCP value
	 */
	uint8_t dscp;
	/*
	 * DSCP remark
	 */
	uint8_t dscp_remark;
	/*
	 * Service interval
	 * Specified msec
	 * This is min latency expectation and is used
	 * by Wi-Fi FW for peer tid queue scheduling
	 */
	uint8_t service_interval_dl;
	uint8_t service_interval_ul;
	/*
	 * Burst size
	 * Specified in bytes
	 * This is used by Wi-Fi FW for peer tid queue
	 * scheduling
	 */
	uint32_t burst_size_dl;
	uint32_t burst_size_ul;
	/*
	 * Vlan Priority
	 */
	uint8_t vlan_pcp;
	/*
	 * Vlan Priority Remark
	 */
	uint8_t vlan_pcp_remark;
	/*
	 * Service class id
	 */
	uint8_t service_class_id;
	/*
	 * Source ipv4 address mask
	 */
	uint32_t src_ipv4_addr_mask;

	/*
	 * Destination ipv4 address mask
	 */
	uint32_t dst_ipv4_addr_mask;

	/*
	 * Source ipv6 address mask
	 */
	uint32_t src_ipv6_addr_mask[IPV6_ADDR_LEN];

	/*
	 * Destination ipv6 address mask
	 */
	uint32_t dst_ipv6_addr_mask[IPV6_ADDR_LEN];

	/*
	 * Match pattern supported for SCS classifier (SPI is supported)
	 */
	uint32_t match_pattern_value;

	/*
	 * Match pattern mask supported for SCS classifier (SPI is supported)
	 */
	uint32_t match_pattern_mask;

	/*
	 * IP version type
	 */
	uint8_t ip_version_type;

	/*
	 * MSCS tid  Bitmap
	 */
	uint8_t mscs_tid_bitmap;

	/*
	 * Priority limit range
	 */
	uint8_t priority_limit;

	/*
	 * Destination Interface Index
	 */
	uint8_t dst_ifindex;

	/*
	 * Source port range start
	 */
	uint16_t src_port_range_start;

	/*
	 * Source port range end
	 */
	uint16_t src_port_range_end;

	/*
	 * Destination port range start
	 */
	uint16_t dst_port_range_start;

	/*
	 * Destination port range end
	 */
	uint16_t dst_port_range_end;

	/*
	 * Acceleration engine type
	 */
	enum sp_rule_ae_type ae_type;

	/*
	 * Source interface
	 */
	char src_iface[IFNAMSIZ];

	/*
	 * Destination interface
	 */
	char dst_iface[IFNAMSIZ];

	/*
	 * Source Interface Index
	 */
	uint8_t src_ifindex;
	/*
	 * Threshold size to initiate fragmentation
	 */
	uint16_t ipv4_frag_thresh;

	/*
	 * Flag indicating WLAN specific parameters
	 */
	uint8_t wlan_flow;

	/*
	 * Transmitter mac
	 */
	uint8_t transmitter_mac[ETH_ALEN];

	/*
	 * Receiver mac
	 */
	uint8_t receiver_mac[ETH_ALEN];

	/*
	 * Array of radio bands for each possible vdes
	 */
	uint8_t radio_band[SP_RULE_MAX_VDEV_PER_ML];

	/*
	 * Array of radio channels for each possible VDEVs
	 */
	uint8_t radio_channel[SP_RULE_MAX_VDEV_PER_ML];

	/*
	 * Array of radio bandwidth for each possible VDEVs
	 */
	uint8_t radio_bandwidth[SP_RULE_MAX_VDEV_PER_ML];

	/*
	 * band mode
	 */
	uint8_t band_mode;

	/*
	 * channel mode
	 */
	uint8_t channel_mode;

	/*
	 * bandwidth mode
	 */
	uint8_t bandwidth_mode;

	/*
	 * bssid
	 */
	uint8_t bssid[ETH_ALEN];

	/*
	 * ssid length
	 */
	uint8_t ssid_len;

	/*
	 * ssid
	 */
	char ssid[WLAN_SSID_MAX_LEN];

	/*
	 * access class
	 */
	uint8_t access_class;

	/*
	 * priority
	 */
	uint8_t priority;

	/*
	 * valid access class
	 */
	bool valid_ac;
};

/*
 * sp_rule_radio_info
 * 	This structure stores radio metadata from user
 */
struct sp_rule_radio_info {

	/*
	 * array of radio band from user
	 */
	char radio_band[SP_RULE_MAX_VDEV_PER_ML][SP_RULE_VDEV_BAND_LENGTH];

	/*
	 * array of radio bandwidth from user
	 */
	char radio_bandwidth[SP_RULE_MAX_VDEV_PER_ML][SP_RULE_VDEV_BANDWIDTH_LENGTH];

	/*
	 * array of radio channel from user
	 */
	uint8_t radio_channel[SP_RULE_MAX_VDEV_PER_ML];
};

/*
 * sp_rule_classifier_type
 * 	This enum defines rule classifier type
 */
enum sp_rule_classifier_type {
	SP_RULE_TYPE_SAWF_INVALID,	/* For invalid type */
	SP_RULE_TYPE_MESH,	/* For non-sawf rule */
	SP_RULE_TYPE_SAWF,	/* For SAWF rule */
	SP_RULE_TYPE_SCS,	/* For SCS rule */
	SP_RULE_TYPE_MSCS,	/* For MSCS rule */
	SP_RULE_TYPE_SAWF_SCS,	/* For SAWF-SCS rule type */
	SP_RULE_TYPE_SAWF_IFLI,	/* For IFLI rule type */
	SP_RULE_TYPE_SAWF_MAX,	/* Max SP rule type */
};

/*
 * sp_rule
 *	This struct stores Service Prioritization rule structure.
 */
struct sp_rule {
	u_int32_t id;						/* Service prioritization rule identifier */
	u_int32_t key;						/* Key for IFLI cases. */
	sp_mapdb_add_remove_filter_type_t cmd;			/* Command type. 1 means add 0 means delete. */
	struct sp_rule_inner inner;				/* Inner structure */
	uint8_t rule_precedence;				/* Rule precedence – higher number means higher priority. */
	uint8_t classifier_type;				/* rule_type:0 for Emesh case, 1 for SAWF, 2 for SCS */
};

/*
 * sp_3tuple_info
 * 	This structure lists 3 tuple parameters related to source/destination
 */
struct sp_3tuple_info {
	uint8_t mac[ETH_HLEN];		/* MAC address */
	uint16_t port;			/* Port ID */
	union {
		uint32_t ipv4_addr;		/* IPV4 address */
		uint32_t ipv6_addr[4];		/* IPV6 address */
	} ip;
};

/*
 * sp_rule_input_params
 * 	This structure lists input parameters for ecm query
 */
struct sp_rule_input_params {
	struct sp_3tuple_info src;		/* Source 3 tuple parameters */
	struct sp_3tuple_info dst;		/* Destination 3 tuple parameters */
	uint32_t spi;				/* SPI for ipsec packet */
	uint16_t protocol;			/* Protocol number */
	uint8_t dscp;				/* DSCP value */
	uint8_t ip_version_type;		/* IP Version type */
	uint16_t vlan_tci;			/* Vlan TCI */
	uint8_t dst_ifindex;			/* Destination interface index */
	uint8_t dev_addr[ETH_HLEN];		/* Netdevice address in case of WDS EXT case */
	uint8_t src_ifindex;			/* Source Interface Index */
	struct net_device *src_dev;		/* src dev */
	struct net_device *dest_dev;		/* dest dev */
};

/*
 * sp_rule_output_params
 * 	This structure lists output parameters from SPM to ECM
 */
struct sp_rule_output_params {
	uint8_t service_class_id;	/* Service class ID */
	uint8_t priority;		/* Priority */
	uint8_t dscp_remark;		/* DSCP remark */
	uint8_t vlan_pcp_remark;	/* Vlan PCP remark */
	uint32_t rule_id;		/* Rule ID */
	uint8_t sawf_rule_type;		/* rule based flag*/
	enum sp_rule_ae_type ae_type;	/* acceleration engine mode */
	uint32_t key;			/* Key to get hash bucket index*/
	uint16_t ipv4_frag_thresh;	/* Threshold size to initiate fragmentation */
};

/*
 * sp_rule_del_params
 *	This structure contains parameters necessary to delete tuple based rules
 */
struct sp_rule_del_params {
	uint32_t rule_id;		/* Rule ID */
	uint32_t key;			/* Key to get hash bucket index */
	uint32_t src_ip[4];		/* Source IP Address */
	uint32_t dest_ip[4];		/* Destination IP Address */
	uint16_t src_port;		/* Source Port */
	uint16_t dest_port;		/* Destination Port */
	int protocol;			/* Protocl number */
	int ip_version;			/* IP version */
};

/*
 * sp_rule_wifi_plugin_metadata
 * 	contains parameters for plugin module
 */
struct sp_rule_wifi_plugin_metadata {
	uint8_t access_class;	/* access class */
	uint8_t valid_flags;	/* valid flags */
	uint8_t band_mode;	/* band mode */
	uint8_t channel_mode;	/* channel mode */
	uint8_t bandwidth_mode;	/* bandwidth mode */
	uint8_t skb_prio;	/* skb priority for the packet */
	uint8_t priority;	/* user given priority */
	uint8_t ssid_len;	/* ssid len */
	uint8_t src_mac[ETH_ALEN];	/*Sender's mac */
	uint8_t dest_mac[ETH_ALEN];	/*Receiver's mac */
	uint8_t bssid[ETH_ALEN];	/* bssid */
	uint8_t ra_mac[ETH_ALEN];	/* ra_mac */
	uint8_t ta_mac[ETH_ALEN];	/* ta_mac */
	uint8_t radio_band[SP_RULE_MAX_VDEV_PER_ML];	/* radio band */
	uint8_t radio_bw[SP_RULE_MAX_VDEV_PER_ML];	/* radio bandwidth */
	uint8_t radio_chan[SP_RULE_MAX_VDEV_PER_ML];	/* radio channel */
	uint8_t pcp;	/* Receiver's pcp value */
	uint8_t dscp;	/* Receiver's dscp value */
	struct net_device *dest_dev;	/* dest dev */
	struct net_device *src_dev;	/* src dev */
	char ssid[WLAN_SSID_MAX_LEN];	/* ssid */
} __attribute__((packed));

/*
 * callback for plugin
 * 	Emesh SP rule query callback to which Wi-Fi plugin module will register
 */
typedef bool (*sp_rule_wifi_sawf_rule_query_callback_t)(struct sp_rule_wifi_plugin_metadata *wifi_info);

/*
 * callback for plugin
 * 	Emesh sp rule validate callback to which Wi-Fi plugin module will register
 */
typedef bool (*sp_rule_wifi_sawf_rule_validate_callback_t)(struct sp_rule_wifi_plugin_metadata *wifi_info);

/*
 * Data structure for emesh-sp wifi-plugin callbacks
 */
struct emesh_sp_wifi_sawf_callbacks {
	sp_rule_wifi_sawf_rule_query_callback_t sawf_rule_query_callback;	/* WLAN rule query callback */
	sp_rule_wifi_sawf_rule_validate_callback_t sawf_rule_validate_callback;	/* WLAN rule validate callback */
};

/*
 * Register WLAN rule query callback with plugin
 */
int sp_mapdb_wifi_plugin_query_wlan_rule_cb_register(struct emesh_sp_wifi_sawf_callbacks *wifi_plugin_cb);

/*
 * Unregister WLAN rule query callback with plugin
 */
void sp_mapdb_wifi_plugin_query_wlan_rule_cb_unregister(void);

/*
 * Register WLAN rule validate callback with plugin
 */
int sp_mapdb_wifi_plugin_validate_wlan_rule_cb_register(struct emesh_sp_wifi_sawf_callbacks *wifi_plugin_cb);

/*
 * Unregister WLAN rule validate callback with plugin
 */
void sp_mapdb_wifi_plugin_validate_wlan_rule_cb_unregister(void);

/*
 * sp_rm_sync_msg
 *	This structure repressents the msg contents of a msg sent from ECM to RM
 */
struct __attribute__((__packed__)) sp_rm_sync_msg {
	uint8_t flow_sid;		/* Flow Service class ID */
	uint8_t return_sid;		/* Return Service class ID */
	uint32_t src_ip[4];		/* Src IP Addr */
	uint32_t dst_ip[4];		/* Dst IP Addr */
	uint16_t src_port;		/* Src Port */
	uint16_t dst_port;		/* Dst Port */
	uint16_t protocol;		/* Protocol number */
	uint8_t ip_version;		/* IPv4 or IPv6 */
	uint8_t src_mac[6];		/* Src Mac Addr */
	uint8_t dst_mac[6];		/* Dst Mac Addr */
	uint8_t sync_type;		/* Is this due to flow being priorited, updated, or deprioritized */
};

sp_mapdb_update_result_t sp_mapdb_rule_update(struct sp_rule *newrule);

void sp_mapdb_get_wlan_latency_params(struct sk_buff *skb, uint8_t *service_interval_dl, uint32_t *burst_size_dl, uint8_t *service_interval_ul, uint32_t *burst_size_ul, uint8_t *smac, uint8_t *dmac);
void sp_mapdb_apply(struct sk_buff *skb, uint8_t *smac, uint8_t *dmac);
void sp_mapdb_notifier_register(struct notifier_block *nb);
void sp_mapdb_notifier_unregister(struct notifier_block *nb);
void sp_mapdb_ruletable_flush(void);
void sp_mapdb_ifli_rule_flush(struct sp_rule_del_params *del_params);
void sp_mapdb_rule_apply_sawf(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *rule_output);
int sp_mapdb_rm_sync(struct sp_rm_sync_msg *rm_msg);
void sp_mapdb_apply_scs(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *rule_output);
void sp_mapdb_apply_mscs(struct sk_buff *skb, struct sp_rule_input_params *params,
			      struct sp_rule_output_params *output);
char *sp_mapdb_get_classifier_type_str(enum sp_rule_classifier_type type);
#endif
