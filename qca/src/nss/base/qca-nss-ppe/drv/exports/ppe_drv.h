/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv.h
 *	NSS PPE driver definitions.
 */

#ifndef _PPE_DRV_H_
#define _PPE_DRV_H_

/**
 * @addtogroup ppe_drv_subsystem
 * @{
 */
#include <linux/module.h>
#include <linux/if_ether.h>
#include "ppe_drv_iface.h"
#include <linux/netdevice.h>
#ifdef PPE_DRV_VEIP_FEATURE_SUPPORT
#include "ppe_drv_veip.h"
#endif
#include "ppe_drv_sc.h"

/*
 * FSE flags
 */
#define PPE_DRV_FSE_IPV4 0x00000001	/**< Indicate if flow is IPv4 to FSE. */
#define PPE_DRV_FSE_IPV6 0x00000002	/**< Indicate if flow is IPv6 to FSE. */
#define PPE_DRV_FSE_DS 0x00000004	/**< Indicate if flow is Direct switch. */

/*
 * PPE INT PRI Min/Max values
 */
#define PPE_DRV_INT_PRI_MIN 0
#define PPE_DRV_INT_PRI_MAX 15

/*
 * PPE Multicast Queue Class Max value
 */
#define PPE_DRV_MCAST_QUEUE_CLASS_MAX 4			/**< Maximum multicast queue class (0-3) */

#define PPE_DRV_MHT_SWITCH_ID	1			/**< MHT switch ID */

/*
 * PPE Assist feature flags
 */
#define PPE_DRV_ASSIST_FEATURE_RFS_ETH		0x00000001      /* PPE Assist feature to configure RFS */
#define PPE_DRV_ASSIST_FEATURE_RFS_WLAN		0x00000002      /* PPE Assist feature to configure RFS for WLAN flows */
#define PPE_DRV_ASSIST_FEATURE_PRIORITY		0x00000004      /* PPE Assist feature to configure Priority */
#define PPE_DRV_ASSIST_FEATURE_RFS_TUNNEL	0x00000008	/* PPE Assist feature to configure RFS for Tunnel flows */
#define PPE_DRV_ASSIST_FEATURE_RFS_DEFAULT	0x00000010      /* PPE Assist feature to configure RFS */
#define PPE_DRV_ASSIST_FEATURE_RFS_UDP_ST	0x00000020      /* PPE Assist feature to configure RFS for udp-st flows */
#define PPE_DRV_ASSIST_FEATURE_RFS_MCAST	0x00000040      /* PPE Assist feature to configure RFS for multicast flows */
#define PPE_DRV_ASSIST_FEATURE_RFS_UNKNOWN_DST	0x00000080	/* PPE Assist feature to configure RFS for unknown destination */

#define PPE_DRV_SERVICE_CLASS_IS_VALID(sc)	((sc >= PPE_DRV_SAWF_SC_START) && (sc <= PPE_DRV_SAWF_SC_END))

/*
 * PPE QOS VALID FLAGS
 */
#define PPE_DRV_VALID_FLAG_FLOW_PPE_QOS		0x01	/**< PPE QOS is enabled in flow direction. */
#define PPE_DRV_VALID_FLAG_RETURN_PPE_QOS	0x02	/**< PPE QOS is enabled in return direction. */

/*
 * ACL/POLICER VALID FLAGS
 */
#define PPE_DRV_VALID_FLAG_FLOW_ACL		0x01	/**< ACL is enabled in flow direction. */
#define PPE_DRV_VALID_FLAG_RETURN_ACL		0x02	/**< ACL is enabled in return direction. */
#define PPE_DRV_VALID_FLAG_FLOW_POLICER		0x01	/**< Policer is enabled in flow direction. */
#define PPE_DRV_VALID_FLAG_RETURN_POLICER	0x02	/**< Policer is enabled in return direction. */

#define PPE_DRV_VALID_TUN_SRC_MAC_FLOW 0x01
		/**< MAC address for the flow interface is valid. */
#define PPE_DRV_VALID_TUN_SRC_MAC_RETURN 0x02
		/**< MAC address for the return interface is valid. */

/*
 * Host Qdisc classification id
 */
#define PPE_DRV_HOST_QDISC_CLASS_ID_DEF_VAL		0x0

/*
 * Host Qdisc flags
 */
#define PPE_DRV_HOST_QDISC_INVALID			0x0
#define PPE_DRV_HOST_QDISC_DEV_FAST_XMIT_VP		0x1
#define PPE_DRV_HOST_QDISC_DEV_QUEUE_XMIT		0x2
#define PPE_DRV_HOST_QDISC_DEV_FAST_XMIT_QDISC		0x4
#define PPE_DRV_HOST_QDISC_ON_BOTTOM_IFACE		0x8
#define PPE_DRV_HOST_QDISC_ON_NON_BOTTOM_IFACE		0x10

/*
 * Qdisc direction flag
 */
#define PPE_DRV_QDISC_DIR_FLOW_VALID		0x01	/**< Qdisc rules are for flow direction. */
#define PPE_DRV_QDISC_DIR_RETURN_VALID		0x02	/**< Qdisc rules are for return direction. */
#define PPE_DRV_QDISC_DIR_INVALID		0x04	/**< Qdisc rules are for direction is invalid. */

#define PPE_DRV_WIFI_INVALID_AST_INFO		0xFFFF
#define PPE_DRV_WIFI_INVALID_PEER_ID		0xFFFF
#define PPE_DRV_WIFI_INVALID_STREAM_ID		0xFF

/*
 * SPI value update
 */
#define PPE_DRV_ESP_SPI_SHIFT			0x10
#define PPE_DRV_ESP_SPI_MASK			0xFFFF

/*
 * SAWF mark update flags
 */
#define PPE_DRV_SAWF_MARK_FLOW_UPDATE		0x0001
#define PPE_DRV_SAWF_MARK_RETURN_UPDATE		0x0002
#define PPE_DRV_SAWF_SHIFT_TAG			24
#define PPE_DRV_SAWF_GET_TAG(x)			(x >> PPE_DRV_SAWF_SHIFT_TAG)
#define PPE_DRV_SAWF_VALID_TAG			0xAA
#define PPE_DRV_SAWF_VALID_BIT(x)		((x >> 20) & 0x4)

#define PPE_DRV_FLOW_COOKIE_STREAM_ID_VALID_SHIFT	14
#define PPE_DRV_FLOW_COOKIE_STREAM_ID_VALID_SET(x)	((x) |= (1U << PPE_DRV_FLOW_COOKIE_STREAM_ID_VALID_SHIFT))

#define PPE_DRV_LOOPBACK_FEATURE_TYPE_DISABLED		0x0	/* loopback feature is disabled */
#define PPE_DRV_LOOPBACK_FEATURE_TYPE_DEFAULT		0x1	/* Read dts file to enable loopback feature */
#define PPE_DRV_LOOPBACK_FEATURE_TYPE_EXT_DDR_UPSTREAM	0x2	/* PON extended DDR upstream loopback feature */
#define PPE_DRV_LOOPBACK_FEATURE_TYPE_GRETAP_MAPT	0x4	/* GRETAP to MAPT loopback feature */
#define PPE_DRV_LOOPBACK_FEATURE_TYPE_V6_HAIRPIN_NAT	0x8	/* Hairpin NAT loopback feature */
#define PPE_DRV_LOOPBACK_FEATURE_TYPE_DDR_BUFFER	0x10	/* Speed mismatch DDR buffering loopback feature */

#define PPE_DRV_JUMBO_MAX				9216	/* Maximum MRU value */

/*
 * Update rule type
 */
#define PPE_DRV_UPDATE_RULE_TYPE_QOS	0x01
#define PPE_DRV_UPDATE_RULE_TYPE_DSCP	0x02
#define PPE_DRV_UPDATE_RULE_TYPE_SAWF	0x04

/*
 * ppe_drv_ip_type
 *	Types of IP addresses handled
 */
enum ppe_drv_ip_type {
	PPE_DRV_IP_TYPE_V4,	/**< IPv4 unicast IP-type. */
	PPE_DRV_IP_TYPE_V6,	/**< IPv6 unicast IP-type. */
	PPE_DRV_IP_TYPE_MC_V4,	/**< IPv4 multicast IP-type. */
	PPE_DRV_IP_TYPE_MC_V6	/**< IPv6 multicast IP-type. */
};

/*
 * ppe_drv_sawf_sc_type
 *	Total types of service class
 */
typedef enum ppe_drv_sawf_sc_type {
	PPE_DRV_SAWF_SC_NONE = 0,	/**< Invalid service class. */
	PPE_DRV_SAWF_SC_START,		/**< SAWF service class start. */
	PPE_DRV_SAWF_SC_END = 128,	/**< SAWF service class end. */
	PPE_DRV_SAWF_SC_MAX,		/**< Maximum number of SAWF service classes */
} ppe_drv_sawf_sc_t;

/*
 * ppe_drv_tree_id_type
 *	Type for different Tree-ID configuration
 */
typedef enum ppe_drv_tree_id_type {
	PPE_DRV_TREE_ID_TYPE_NONE = 0,	/**< Normal processing */
	PPE_DRV_TREE_ID_TYPE_SAWF,	/**< SAWF usecase */
	PPE_DRV_TREE_ID_TYPE_WIFI_TID,	/**< HLOS TID usecase */
	PPE_DRV_TREE_ID_TYPE_SCS,	/**< SCS usecase */
	PPE_DRV_TREE_ID_TYPE_MLO_ASSIST,	/**< MLO usecase */
	PPE_DRV_TREE_ID_TYPE_WIFI8_PPEDS,	/**< Wifi8 PPEDS usecase */
	PPE_DRV_TREE_ID_TYPE_UDP_ST,	/**< UDP-ST usecase */
	PPE_DRV_TREE_ID_TYPE_MAX = 16,	/**< Maximum number of supported types */
} ppe_drv_tree_id_type_t;

/*
 * ppe_drv_rule_dir
 *	Rule direction
 */
typedef enum ppe_drv_rule_dir {
	PPE_DRV_RULE_INGRESS = 1,	/**< Ingress: Rule Direction. */
	PPE_DRV_RULE_EGRESS = 2,	/**< Egress: Rule Direction. */
} ppe_drv_rule_dir_t;

/*
 * ppe_drv_fse_tuple
 *	fse tuple
 */
struct ppe_drv_fse_tuple {
	uint32_t src_ip[4];		/**< Flow IP address. */
	uint32_t src_port;		/**< Flow identifier (e.g., TCP or UDP port). */
	uint32_t dest_ip[4];		/**< Return IP address. */
	uint32_t dest_port;		/**< Return identifier (e.g., TCP or UDP port). */
	uint8_t protocol;		/**< Protocol number. */
};

/*
 * ppe_drv_fse_rule_info
 *	Information to pass from PPE to FSE module
 */
struct ppe_drv_fse_rule_info {
	struct ppe_drv_fse_tuple tuple;		/**< 5 tuple information. */
	struct net_device *dev;			/**< VAP netdevice. */
	uint32_t flags;				/**< Info flag */
	uint8_t vp_num;				/**< Virtual port number. */
	uint8_t macid;				/**< Macid of egress port. */
};

/**
 * Information for source MAC address rules.
 */
struct ppe_drv_src_mac_rule {
	uint32_t mac_valid_flags;       /**< MAC address validity flags. */
	uint8_t flow_src_mac[6];       /**< Source MAC address for the flow direction. */
	uint8_t return_src_mac[6];     /**< Source MAC address for the return direction. */
};

/*
 * ppe_drv_fse_ops
 *	FSE operations
 */
struct ppe_drv_fse_ops {
	bool (*create_fse_rule)(struct ppe_drv_fse_rule_info *finfo);	/**< Function pointer to create FSE rules. */
	bool (*destroy_fse_rule)(struct ppe_drv_fse_rule_info *finfo);	/**< Function pointer to destroy FSE rules. */
};

/**
 * ppe_drv_pppoe_session
 *	Information for PPPoE session.
 */
struct ppe_drv_pppoe_session {
	uint16_t session_id;				/**< Session id */
	uint8_t server_mac[ETH_ALEN];			/**< Server MAC address */
};

/**
 * ppe_drv_pppoe_rule
 *	Information for PPPoE connection rules.
 */
struct ppe_drv_pppoe_rule {
	struct ppe_drv_pppoe_session flow_session;		/**< Flow PPPoE session */
	struct ppe_drv_pppoe_session return_session;		/**< Return PPPoE session */
};

/**
 * ppe_drv_dscp_rule
 *	Information for DSCP connection rules.
 */
struct ppe_drv_dscp_rule {
	uint8_t flow_dscp;		/**< Egress DSCP value for the flow direction. */
	uint8_t return_dscp;		/**< Egress DSCP value for the return direction. */
};

/**
 * ppe_drv_vlan_info
 *	Information for ingress and egress VLANs.
 */
struct ppe_drv_vlan_info {
	uint32_t ingress_vlan_tag;	/**< VLAN tag for the ingress packets. */
	uint32_t egress_vlan_tag;	/**< VLAN tag for egress packets. */
};

/**
 * ppe_drv_vlan_rule
 *	Information for VLAN connection rules.
 */
struct ppe_drv_vlan_rule {
	struct ppe_drv_vlan_info primary_vlan;		/* Primary VLAN info */
	struct ppe_drv_vlan_info secondary_vlan;	/* Secondary VLAN info */
};

/**
 * ppe_drv_qos_rule
 *	Information for QoS connection rules.
 */
struct ppe_drv_qos_rule {
	uint32_t flow_qos_tag;		/**< QoS tag associated with this rule for the flow direction. */
	uint32_t return_qos_tag;	/**< QoS tag associated with this rule for the return direction. */
	uint8_t flow_int_pri;		/**< PPE INT_PRI corresponding to flow_qos_tag when PPE Qdisc is configured. */
	uint8_t return_int_pri;		/**< PPE INT_PRI corresponding to return_qos_tag when PPE Qdisc is configured. */
	uint8_t qos_valid_flags;	/**< FLAGS to identify PPE QOS. */
	uint8_t reserved[1];		/**< Reserved; padding for alignment. */
};

/**
 * ppe_drv_vp_dl_qdisc_rule
 * 	Information for Linux Qdisc rules.
 */
struct ppe_drv_vp_dl_qdisc_rule {
	uint8_t flow_flags;
		/**< Flow direction Qdisc flags to understand the how many qdiscs are in hierarchy. */
	uint8_t return_flags;
		/**< Return direction Qdisc flags to understand the how many qdiscs are in hierarchy. */
	uint32_t flow_class_id;
		/**< Flow direction QoS tag. */
	uint32_t return_class_id;
		/**< Return direction QoS tag. */
	struct net_device *flow_netdev;
		/**< Flow direction Qdisc net device where the packets should be transmitted. */
	struct net_device *return_netdev;
		/**< Return direction Qdisc net device where the packets should be transmitted. */
};

/**
 * ppe_drv_accel_rule_dir
 *	Information about direction to accelerate.
 */
struct ppe_drv_accel_rule_dir {
	bool flow_accel;	/**< Set if acceleration enabled in flow direction. */
	bool return_accel;	/**< Set if acceleration enabled in return direction. */
};

/**
 * ppe_drv_top_if_rule
 *	Information for top interface in hierarchy.
 */
struct ppe_drv_top_if_rule {
	ppe_drv_iface_t rx_if;		/**< Top PPE interface for from direction */
	ppe_drv_iface_t tx_if;		/**< Top PPE interface for return direction */
};

/**
 * ppe_drv_service_class_rule
 *	Service class related information.
 */
struct ppe_drv_service_class_rule {
	uint32_t flow_mark;		/**< SAWF metadata information in flow direction. */
	uint32_t return_mark;		/**< SAWF metadata information in return direction. */
	uint8_t flow_service_class;	/**< Service class id in flow direction. */
	uint8_t return_service_class;	/**< Service class id in return direction. */
	uint16_t valid_flags;		/**< Flags associated with SAWF mark update. */
};

/**
 * ppe_drv_wifi_rule
 *	Wi-Fi metadata and DS related information.
 */
struct ppe_drv_wifi_mdata_rule {
	uint32_t flow_mark;		/**< Wi-Fi metadata information in flow direction. */
	uint32_t return_mark;		/**< Wi-Fi metadata information in return direction. */
	uint32_t flow_ds_node_mdata;	/**< DS metadata in flow direction. */
	uint32_t return_ds_node_mdata;	/**< DS metadata in return direction. */
	uint16_t flow_ast_info;		/**< Flow direction WLAN peer AST info (cache_id + idx merged) */
	uint16_t return_ast_info;		/**< Return direction WLAN peer AST info (cache_id + idx merged) */
	uint8_t flow_stream_id;		/**< Flow direction WLAN stream ID for telemetry */
	uint8_t return_stream_id;		/**< return direction WLAN stream ID for telemetry */
	uint16_t flow_peer_id;		/**< Flow direction WLAN peer ID */
	uint16_t return_peer_id;		/**< return direction WLAN peer ID */
};

/**
 * ppe_drv_spi_rule
 *	PPE ESP SPI rule
 */
struct ppe_drv_spi_rule {
	uint32_t l_spi;				/**< Local SPI. */
	uint32_t r_spi;				/**< Remote SPI. */
};

/*
 * ppe_drv_nsm_queue_drop_stats
 *	Per-queue stats to be send to NSM.
 */
struct ppe_drv_nsm_queue_drop_stats {
	uint64_t drop_packets;		/**< Drop packets for given queue and item. */
	uint64_t drop_bytes;		/**< Drop bytes for given queue and item. */
};

/*
 * ppe_drv_nsm_flow_stats
 *	Per-flow stats to be send to NSM.
 */
struct ppe_drv_nsm_flow_stats {
	uint64_t rx_packets;		/**< Packets received on the ethernet port. */
	uint64_t rx_bytes;		/**< Bytes received on the ethernet port */
};

/*
 * ppe_drv_nsm_sawf_sc_stats
 *	Per-service class stats to be send to NSM.
 */
struct ppe_drv_nsm_sawf_sc_stats {
	uint64_t rx_packets;		/**< Packets recieved on the ethernet port. */
	uint64_t rx_bytes;		/**< Bytes recieved on the ethernet port. */
	uint16_t flow_count;		/**< Number of flows per service class. */
};

/*
 * ppe_drv_nsm_stats
 *	Information to be send to NSM.
 */
struct ppe_drv_nsm_stats {
	struct ppe_drv_nsm_sawf_sc_stats sawf_sc_stats;		/**< Per-service class stats. */
	struct ppe_drv_nsm_flow_stats flow_stats;		/**< Per-flow stats. */
	struct ppe_drv_nsm_queue_drop_stats queue_stats;	/**< Per-queue stats. */
};

/*
 * ppe_drv_acl_policer_rule_type
 *	Rule type to indicate flow+acl or flow+policer combination.
 */
typedef enum ppe_drv_acl_policer_rule_type {
	PPE_DRV_RULE_TYPE_FLOW_ACL,		/**< Flow needs to be combined with ACL. */
	PPE_DRV_RULE_TYPE_FLOW_POLICER,		/**< Flow needs to be combined with Policer. */
} ppe_drv_acl_policer_rule_t;

/*
 * ppe_drv_acl_policer_rule
 *	ACL/POLICER rule ID for each direction
 *
 * Note: This is used when the packets matching the flow need to be rate-limited (policed)
 * 	or filtered by an ACL rule, post flow processing.
 */
struct ppe_drv_acl_policer_rule {
	ppe_drv_acl_policer_rule_t type;	/**< Whether ACL or Policer rule? */
	bool pkt_noedit;			/**< Is packet edit required. */
	union {
		struct {
			uint8_t flags;			/**< Valid flag. */
			uint32_t flow_acl_id;		/**< ACL rule ID in flow direction. */
			uint32_t return_acl_id;		/**< ACL rule ID in return direction. */
		} acl;
		struct {
			uint8_t flags;			/**< Valid flag. */
			uint32_t flow_policer_id;	/**< Policer ID in flow direction. */
			uint32_t return_policer_id;	/**< Policer ID in return direction. */
		} policer;
	} rule_id;
};

/*
 * ppe_drv_stats_sync_reason
 *	Stats sync reasons.
 */
enum ppe_drv_stats_sync_reason {
	PPE_DRV_STATS_SYNC_REASON_STATS,	/* Sync is to synchronize stats */
	PPE_DRV_STATS_SYNC_REASON_FLUSH,	/* Sync is to flush a connection entry */
	PPE_DRV_STATS_SYNC_REASON_EVICT,	/* Sync is to evict a connection entry */
	PPE_DRV_STATS_SYNC_REASON_DESTROY,	/* Sync is to destroy a connection entry */
};

/*
 * ppe_drv_notifier_ops_priority
 *	Priority order for client manager
 *	To-do: Introduce new file for ppe-drv notifier
 */
enum ppe_drv_notifier_ops_priority {
	PPE_DRV_NOTIFIER_PRI_1 = 1,		/* Client manager notifier having priority one */
	PPE_DRV_NOTIFIER_PRI_2 = 2,		/* Client manager notifier having priority two*/
	PPE_DRV_NOTIFIER_MAX,		/* Notifier maximum priority */
};

/*
 * ppe_drv_notifier_event
 *	PPE drv notifier events.
 */
enum ppe_drv_notifier_event {
	PPE_DRV_EVENT_INALID,		/* Invalid event */
	PPE_DRV_EVENT_CHANGEUPPER,	/* Change upper event */
	PPE_DRV_EVENT_MAX,		/* Maximum event */
};

struct ppe_drv_notifier_ops;

/**
 * ppe_drv_notifier_fn_t
 *	Function pointer for notifier
 */
typedef int (*ppe_drv_notifier_fn_t)(struct ppe_drv_notifier_ops *nb,
				int event, struct netdev_notifier_info *info);
/**
 * ppe_drv_notifier_ops
 *	Notifier operations registered with different clients
 */
struct ppe_drv_notifier_ops {
	ppe_drv_notifier_fn_t notifier_call;		/* Notifier callback registered with ppe drv */
	int priority;					/* Priority of notifier callback */
	struct list_head entry;				/* List head */
};

/*
 * ppe_drv_flow_rule_dir
 *	Identifies the direction of flow entry
 */
typedef enum ppe_drv_flow_rule_dir {
	PPE_DRV_FLOW_RULE_DIR_FLOW = 0,		/* Rule is of flow direction */
	PPE_DRV_FLOW_RULE_DIR_RETURN = 1,	/* Rule is of return direction */
} ppe_drv_flow_rule_dir_t;

/*
 * ppe_drv_unidir_update_info
 *	Unidirection rule update
 */
struct ppe_drv_unidir_update_info {
	struct ppe_drv_qos_rule qos;	/**< QoS information to update. */
	struct ppe_drv_dscp_rule dscp; /**< DSCP information to update. */
	ppe_drv_flow_rule_dir_t dir;	/**< Flow rule direction to update. */
	uint16_t valid_flags;		/**< Flags associated with QoS/DSCP update. */
};

/**
 * enum ppe_drv_ret
 *	PPE return status
 */
typedef enum ppe_drv_ret {
	PPE_DRV_RET_SUCCESS = 0,			/**< Success */
	PPE_DRV_POLICER_CREATE_FAIL,			/**< Create Fail */
	PPE_DRV_POLICER_DESTROY_FAIL,			/**< Destroy Fail */
	PPE_DRV_POLICER_DESTROY_SUCCESS,		/**< Destroy success */
	PPE_DRV_RET_IFACE_INVALID,			/**< Failure due to Invalid PPE interface */
	PPE_DRV_RET_FAILURE_NOT_SUPPORTED,		/**< Failure due to unsupported feature */
	PPE_DRV_RET_FAILURE_NO_RESOURCE,		/**< Failure due to out of resource */
	PPE_DRV_RET_FAILURE_INVALID_PARAM,		/**< Failure due to invalid parameter */
	PPE_DRV_RET_PORT_NOT_FOUND,			/**< Port not found */
	PPE_DRV_RET_VSI_NOT_FOUND,			/**< VSI not found */
	PPE_DRV_RET_VSI_MEMBER_NOT_FOUND,		/**< VSI member not found */
	PPE_DRV_RET_VSI_MEMBER_NOT_SET,			/**< VSI memeber not set */
	PPE_DRV_RET_L3_IF_NOT_FOUND,			/**< L3_IF not found */
	PPE_DRV_RET_PORT_ALLOC_FAIL,			/**< Port allocation fails */
	PPE_DRV_RET_L3_IF_ALLOC_FAIL,			/**< L3_IF allocation fails */
	PPE_DRV_RET_L3_IF_PORT_ATTACH_FAIL,		/**< L3_IF PORT attach fails */
	PPE_DRV_RET_L3_IF_TTL_DEC_BYPASS_FAIL,		/**< L3_IF TTL decrement bypass fails */
	PPE_DRV_RET_VSI_ALLOC_FAIL,			/**< VSI allocation fails */
	PPE_DRV_RET_MAC_ADDR_CLEAR_CFG_FAIL,		/**< Mac address clear configuration fails */
	PPE_DRV_RET_MAC_ADDR_SET_CFG_FAIL,		/**< Mac address set configuration fails */
	PPE_DRV_RET_MTU_CFG_FAIL,			/**< MTU configuration fails */
	PPE_DRV_RET_DEL_MAC_FDB_FAIL,			/**< Failed to delete FDB entry by MAC */
	PPE_DRV_RET_STA_MOVE_FAIL,			/**< Failed to configure station movement */
	PPE_DRV_RET_NEW_ADDR_LRN_FAIL,			/**< Failed to configure new address learning */
	PPE_DRV_RET_FDB_FLUSH_VSI_FAIL,			/**< Failed to flush FDB entries by VSI */
	PPE_DRV_RET_MEM_IF_INVALID_PORT,		/**< Failed to find port for member interface */
	PPE_DRV_RET_STP_STATE_FAIL,			/**< Failed to set STP state on the bridge port */
	PPE_DRV_RET_IFACE_L3_IF_FAIL,			/**< Failed to find L3_IF for the interface */
	PPE_DRV_RET_PPPOE_ALLOC_FAIL,			/**< PPPOE session allocation failure */
	PPE_DRV_RET_L3_IF_PPPOE_FAIL,			/**< PPPOE session not attached to L3_IF */
	PPE_DRV_RET_L3_IF_PPPOE_SET_FAIL,		/**< Failed to set PPPOE session information in L3_IF */
	PPE_DRV_RET_BASE_IFACE_NOT_FOUND,		/**< Base interface not found */
	PPE_DRV_RET_VLAN_TPID_FAIL,			/**< VLAN TPID not found */
	PPE_DRV_RET_VLAN_TPID_CONFIG_FAIL,		/**< VLAN TPID configuration failed */
	PPE_DRV_RET_PORT_ROLE_FAIL,			/**< Port role configuration failed */
	PPE_DRV_RET_VLAN_RULE_INVALID,			/**< VLAN rule invalid */
	PPE_DRV_RET_INGRESS_VLAN_FAIL,			/**< Ingress vlan configuration failed */
	PPE_DRV_RET_EGRESS_VLAN_FAIL,			/**< Egress vlan configuration failed */
	PPE_DRV_RET_INGRESS_VLAN_MAP_FAIL,		/**< Ingress vlan map configuration failed */
	PPE_DRV_RET_EGRESS_VLAN_MAP_FAIL,		/**< Egress vlan map configuration failed */
	PPE_DRV_RET_VLAN_INGRESS_DEL_FAIL,		/**< Ingress vlan deletion configuration failed */
	PPE_DRV_RET_VLAN_EGRESS_DEL_FAIL,		/**< Egress vlan deletion configuration failed */
	PPE_DRV_RET_FAILURE_INVALID_HIERARCHY,		/**< Failure due to invalid hierarchy */
	PPE_DRV_RET_FAILURE_SNAT_DNAT_SIMUL,		/**< Failure due to both snat and dnat requested */
	PPE_DRV_RET_FAILURE_INVALID_NF_TARGET,		/**< Failure due to invalid Netfilter Target used */
	PPE_DRV_RET_FAILURE_NOT_BRIDGE_SLAVES,		/**< Failure due to from and to interfaces not in same bridge */
	PPE_DRV_RET_FAILURE_IFACE_PORT_MAP,
	PPE_DRV_RET_FAILURE_CREATE_COLLISSION,		/**< Failure due to create collision */
	PPE_DRV_RET_FAILURE_CREATE_OOM,			/**< Failure due to memory allocation failed */
	PPE_DRV_RET_FAILURE_FLOW_ADD_FAIL,		/**< Failure due to flow addition failed in hardware */
	PPE_DRV_RET_FAILURE_DESTROY_NO_CONN,		/**< Failure due to connection not found in hardware */
	PPE_DRV_RET_FAILURE_NO_MATCHING_CONN,		/**< Failure due to connection not found in hardware */
	PPE_DRV_RET_FAILURE_DESTROY_FAIL,		/**< Failure due to connection not found in hardware */
	PPE_DRV_RET_FAILURE_FLUSH_FAIL,			/**< Flush failure */
	PPE_DRV_RET_FAILURE_BRIDGE_NAT,			/**< Failure due to Bridge + NAT flows */
	PPE_DRV_RET_QUEUE_CFG_FAIL,			/**< Failure in queue configuration */
	PPE_DRV_RET_FAILURE_TUN_CE_ADD_FAILURE,		/**< Failure in adding tunnel connection entry */
	PPE_DRV_RET_FAILURE_TUN_CE_DEL_FAILURE,		/**< Failure in removing tunnel connection entry */
	PPE_DRV_RET_INVALID_VP_NUM,			/**< Invalid VP number */
	PPE_DRV_RET_TUN_ADD_CE_NULL,			/**< Add connection entry callback is NULL */
	PPE_DRV_RET_INVALID_EIP_SERVICE,		/**< Invalid inline EIP service */
	PPE_DRV_RET_FAILURE_DUMMY_RULE,			/**< Failed to push rule to PPE for passive VP */
	PPE_DRV_RET_INVALID_USER_TYPE,			/**< Failed to push rule to PPE for DS flow when user type is not DS */
	PPE_DRV_RET_ADD_FDB_FAIL,			/**< Failed to add new fdb entry */
	PPE_DRV_RET_SET_FDB_AGEING_TIME_FAIL,		/**< Failed to set Ageing time */
	PPE_DRV_RET_SET_MIRROR_FAIL,			/**<Failed to set Mirror interface */
	PPE_DRV_RET_SET_MIRROR_IN_FAIL,			/**< Failed to set Mirror ingress */
	PPE_DRV_RET_SET_MIRROR_EG_FAIL,			/**< Failed to set Mirror egress */
	PPE_DRV_RET_FLUSH_FDB_BY_PORT_FAIL,		/**< Failed to flush FDB by port */
	PPE_DRV_RET_FLUSH_FDB_FAIL,			/**< Failed to flush all FDB */
	PPE_DRV_RET_SET_MIRROR_ANALYSIS_FAIL,		/**< Failed to set Mirror analysis port */
	PPE_DRV_RET_GET_MIRROR_ANALYSIS_FAIL,		/**< Failed to get Mirror analysis port */
	PPE_DRV_RET_GET_MIRROR_ANALYSIS_NO_PORT,	/**< No port is set for mirror analysis */
	PPE_DRV_RET_QOS_QUEUE_CFG_FAIL,			/**< QoS queue configuration failed. */
	PPE_DRV_RET_QOS_SCHEDULER_CFG_FAIL,		/**< QoS scheduler configuration failed. */
	PPE_DRV_RET_QOS_SHAPER_CFG_FAIL,		/**< QoS shaper configuration failed. */
	PPE_DRV_RET_QOS_PORT_CFG_FAIL,			/**< QoS port configuration failed. */
	PPE_DRV_RET_QOS_TCONT_CFG_FAIL,			/**< QoS tcont configuration failed. */
	PPE_DRV_RET_QOS_TCONT_STATS_FAIL,		/**< QoS tcont statistics get failed. */
	PPE_DRV_RET_PORT_NO_OFFLOAD,			/**< Offload is disabled on the PPE port */
	PPE_DRV_RET_ACL_RULE_INVALID,			/**< ACL rule invalid */
	PPE_DRV_RET_ACL_RULE_ADD_FAIL,			/**< Failed to add ACL rule */
	PPE_DRV_RET_ACL_RULE_BIND_FAIL,			/**< Failed to bind ACL rule to src */
	PPE_DRV_RET_ACL_RULE_PRI_UPDATE_FAIL,		/**< Failed to update the acl rule priority */
	PPE_DRV_RET_POLICER_RULE_BIND_FAIL,		/**< Failed to bind Policer rule */
	PPE_DRV_RET_BASE_DEV_NOT_FOUND,			/**< Base Device not found */
	PPE_DRV_RET_BASE_PORT_NOT_FOUND,		/**< Base Port not found */
	PPE_DRV_RET_FAILURE_FLOW_CONFIGURE_FAIL,	/**< Failure due to FSE flow configuration failed */
	PPE_DRV_RET_NO_TOP_RX_IF,			/**< Failure due to no corresponding top interface */
	PPE_DRV_RET_INVALID_DEV_TYPE,			/**< Invalid port netdev type */
	PPE_DRV_RET_FAILURE_INVALID_DS_NODE,		/**< Invalid DS node */
	PPE_DRV_RET_ENQ_VP_ALLOC_FAIL,			/**< Enqueue vport allocation failure */
	PPE_DRV_RET_ENQ_VP_TO_PRI_PROF_FAIL,		/**< Enqueue vport to pri profile map failure */
	PPE_DRV_RET_ENQ_VP_QID_SET_FAIL,		/**< Enqueue vport queue id set failure */
	PPE_DRV_RET_ENQ_VP_EN_FAIL,			/**< Enqueue vport enable failure */
	PPE_DRV_RET_METADATA_TO_ENQ_VP_FAIL,		/**< Metadata to enqueue vport fetch failed */
	PPE_DRV_RET_ENQ_VP_QID_RESET_FAIL,		/**< Enqueue vport queue id reset failure */
	PPE_DRV_RET_ENQ_VP_DISABLE_FAIL,		/**< Enqueue vport disable failed */
	PPE_DRV_RET_FAILURE_PREFIX_ADD_FAIL,		/**< Failure in adding an entry into Prefix Table */
	PPE_DRV_RET_FAILURE_IID_ADD_FAIL,		/**< Failure in adding an entry into IID Table */
	PPE_DRV_RET_HOST_QDISC_CFG_FAIL,		/**< Host Qdisc configuration failed. */
	PPE_DRV_RET_SAWF_MARK_UPDATE_FAIL,		/**< SAWF mark update failed. */
	PPE_DRV_RET_GRETAP_TO_MAPT_FLOW_ADD,		/**< GRETAP to MAPT flow rule addition */
	PPE_DRV_RET_GRETAP_TO_MAPT_FLOW_ADD_FAIL,	/**< GRETAP to MAPT flow rule addition failed. */
	PPE_DRV_RET_MAC_LEARN_EXCEPT_CFG_FAIL,		/**< Failed to configure MAC learn exception. */
	PPE_DRV_RET_ATHTAG_MAP_ADD_FAIL,	/**< ATHTAG port mapping add failed. */
	PPE_DRV_RET_ATHTAG_PORT_RX_CFG_FAIL,	/**< ATHTAG port RX config failed. */
	PPE_DRV_RET_ATHTAG_PORT_TX_CFG_FAIL,	/**< ATHTAG port TX config failed. */
	PPE_DRV_RET_FAILURE_V4_ASSIST_RULE,		/**< Failed to create RFS rule */
	PPE_DRV_RET_NPTV6_HAIRPIN_DEL_FAIL,		/**< Failed to destroy nptv6 context for hairpin nat. */
	PPE_DRV_RET_HAIRPIN_LOOPBACK_FEATURE_FAIL,	/**< NPTv6 Hairpin NAT not enabled in EDMA. */
	PPE_DRV_RET_NPTV6_HAIRPIN_FEATURE_FAIL,		/**< NPTv6 Hairpin NAT failed. */
	PPE_DRV_RET_EIP_LOOKUP_MODE_SET_FAIL,		/**< EIP lookup mode failed to set. */
	PPE_DRV_RET_IN_VLAN_DSCP_PBIT_TBL_CONFIG_FAIL,	/**< Failed to configure IN_VLAN_DSCP_PBIT table. */
	PPE_DRV_RET_DSCP_PBIT_TBL_CONFIG_FAIL,		/**< Failed to configure DSCP_PBIT_MAP table. */
	PPE_DRV_RET_DSCP_PBIT_TBL_RST_FAIL,		/**< Failed to reset DSCP_PBIT_MAP table. */
	PPE_DRV_RET_L2_DSCP_PBIT_TBL_CONFIG_FAIL,	/**< Failed to configure L2_DSCP_PBIT_MAP table. */
	PPE_DRV_RET_FAILURE_VLAN_PCP_MARKING,		/**< Failure due to invalid VLAN PCP marking */
	PPE_DRV_RET_PM_GEN_RULE_INVALID,		/**< PM gen rule invalid */
	PPE_DRV_RET_IN_PM_COUNTER_GEN_CREATE_FAIL,	/**< Failed to create the PM counter gen rule in PRE_IPO_PM_COUNTER_TBL. */
	PPE_DRV_RET_EG_PM_COUNTER_GEN_CREATE_FAIL,	/**< Failed to create the PM counter gen rule in EG_PM_COUNTER_TBL. */
	PPE_DRV_RET_PM_COUNTER_GET_FAIL,		/**< Failed to get the PM counter stats. */
	PPE_DRV_RET_WLAN_METADATA_UPDATE_FAIL,		/**< Failure to update wlan qos entry which includes fields such as wifi_qos, tree_id, flow_cookie, etc. */
	PPE_DRV_RET_QOS_UPDATE_FAIL,			/**< Failure to update generic qos entry which includes fields such as int_pri, dscp, pcp values. */
	PPE_DRV_RET_FLOW_ENTRY_UPDATE_FAIL,		/**< Failure to update flow entry in hardware */
	PPE_DRV_RET_UNIDIR_CONN_MATCH_FAIL,		/**< Failure to find connection object at the time of unidirection update */
	PPE_DRV_RET_PORT_MGMT_ISOL_CONFIG_FAIL,		/**< Failed to configure the port isolation. */
	PPE_DRV_RET_PORT_MGMT_ACT_CONFIG_FAIL,		/**< Failed to configure the action control info. */
	PPE_DRV_RET_PORT_MGMT_ISOL_DEFAULT_FAIL,	/**< Failed to configure the default isol config. */
	PPE_DRV_RET_PORT_MGMT_MAC_LRN_LMT_CONFIG_FAIL,	/**< Failed to configure the mac learn limit. */
	PPE_DRV_RET_PORT_MGMT_FID_GET_FAIL,		/**< Failed to get the vsi for the port. */
	PPE_DRV_RET_PORT_MGMT_MAC_FILTER_CLR_FAIL,	/**< Failed to clear the mac filter table. */
	PPE_DRV_RET_PORT_MGMT_MAC_FILTER_SET_FAIL,	/**< Failed to config the mac filter. */
	PPE_DRV_RET_PORT_MGMT_PORT_IN_USE,		/**< Failed to delete port from OMCI list. */
	PPE_DRV_RET_UCAST_PRIO_TBL_MAP_FAIL,		/**< Failed to set profile id for a port. */
	PPE_DRV_RET_DOT1P_RULE_INVALID,			/**< DOT1P rule invalid */
	PPE_DRV_RET_DOT1P_RULE_ADD_FAIL,		/**< Failed to add DOT1P rule */
	PPE_DRV_RET_DOT1P_RULE_BIND_GEMPORT_FAIL,	/**< Failed to bind DOT1P rule to gemport cfg */
	PPE_DRV_RET_DOT1P_RULE_ENABLE_FAIL,		/**< Failed to enable gemport gen */
	PPE_DRV_RET_DOT1P_RULE_BIND_POLICER_ID_FAIL,	/**< Failed to bind DOT1P rule to policer id */
	PPE_DRV_RET_DOT1P_GLOBAL_CFG_FAIL,		/**< DOT1P global config failed */
	PPE_DRV_RET_DOT1P_DEFAULT_CFG_FAIL,		/**< DOT1P default config failed */
	PPE_DRV_RET_GEMPORT_RULE_INVALID,		/**< Gemport rule invalid */
	PPE_DRV_RET_GEMPORT_RULE_ADD_FAIL,		/**< Failed to add Gemport rule */
	PPE_DRV_RET_GEMPORT_RULE_ENABLE_FAIL,		/**< Failed to enable gemport mapping */
	PPE_DRV_RET_COS_MAP_CFG_FAIL,			/**< Failed to configure QoS mapping. */
	PPE_DRV_RET_VLAN_RULE_HW_ID_NOT_FOUND,		/**< Invalid HW index in VLAN tables */
	PPE_DRV_RET_VEIP_LIST_DEL_FAIL,			/**< VEIP list deletion failed. */
	PPE_DRV_RET_VEIP_VSI_REMAP_FAIL,		/**< VSI Remapping failed. */
	PPE_DRV_RET_VEIP_VPGROUP_SET_FAIL,		/**< VP group set failed in EG_VP table. */
	PPE_DRV_RET_VEIP_VPGROUP_CLEAR_FAIL,		/**< VP group clear failed in EG_VP table. */
	PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD,		/**< HGU US flow rule addition. */
	PPE_DRV_RET_VEIP_HGU_US_FLOW_ADD_FAIL,		/**< HGU US flow rule addition failed. */
	PPE_DRV_RET_VEIP_VSI_ATTACH_FAIL,		/**< VSI attach failed for VEIP VP. */
	PPE_DRV_RET_VEIP_VSI_DETACH_FAIL,		/**< VSI detach failed for VEIP VP. */
	PPE_DRV_RET_VEIP_INIT_FAIL,			/**< VEIP init failed. */
	PPE_DRV_RET_MCAST_ENTRY_ADD_FAIL,		/**< Multicast entry add fail */
	PPE_DRV_RET_MCAST_ENTRY_DELETE_FAIL, 		/**< Multicast entry delete fail */
	PPE_DRV_RET_MCAST_STATUS_SET_FAIL,		/**< Multicast set status failed */
	PPE_DRV_RET_MCAST_GLOBAL_CFG_FAIL,		/**< Multicast global configuration failed */
	PPE_DRV_RET_TUN_RPS_CREATE_RULE_FAIL,		/**< RPS Create Rule return failed */
	PPE_DRV_RET_TUN_RPS_DESTROY_RULE_FAIL,		/**< RPS Destroy Rule return failed */
	PPE_DRV_RET_FAILURE_NON_PASSTH_ESP_SPI_FLOW,	/**< IPSEC SPI based pass through is enabled. */
	PPE_DRV_RET_VLAN_VPN_MODE_SET_FAIL,		/**< Failure to set the vlan mode */
	PPE_DRV_RET_ACCEL_RESUME_FAIL,			/**< Failed to resume the accel. */
	PPE_DRV_RET_MAX,				/**< Max limit. */
} ppe_drv_ret_t;

/**
 * ppe_drv_get_dentry
 *	Get PPE debugfs entry.
 *
 * @return
 * ppe dentry.
 */
struct dentry *ppe_drv_get_dentry(void);

/**
 * ppe_drv_queue_from_core
 *	Provide queue for a specific core.
 *
 * @param[in] core core_id.
 *
 * @return
 * queue-ID for a specific core or -1 for incorrect core ID.
 */
int16_t ppe_drv_queue_from_core(uint8_t core);

/**
 * ppe_drv_loopback_base_queue
 *	Configure loopback base queue
 *
 * @param[in] queue_id Loopback queue_id
 * @param[in] loopback_feature_type loopback ring feature type
 *
 * @return
 * none.
 */
void ppe_drv_loopback_base_queue(uint8_t queue_id, uint32_t loopback_feature_type);

/**
 * ppe_drv_gro_core2queue_mapping
 *	Provide core to queue gro mapping.
 *
 * @param[in] core core_id.
 * @param[in] queue_id  queue_id.
 *
 * @return
 * none.
 */
void ppe_drv_gro_core2queue_mapping(uint8_t core, uint8_t queue_id);

/**
 * ppe_drv_core2queue_mapping
 *	Provide core to queue mapping.
 *
 * @param[in] core core_id.
 * @param[in] queue_id  queue_id.
 *
 * @return
 * none.
 */
void ppe_drv_core2queue_mapping(uint8_t core, uint8_t queue_id);

/*
 * ppe_drv_nsm_queue_stats_update()
 *	Update stats in NSM for given queue id.
 *
 * @param[IN] nsm_stats		Pointer to stats structure in NSM.
 * @param[IN] queue_id		Queue ID corresponding to which stats are needed.
 * @param[IN] item_id		Drop Item ID for the Queue.
 *
 * @return
 * Status of the API.
 */
extern bool ppe_drv_nsm_queue_stats_update(struct ppe_drv_nsm_stats *nsm_stats, uint32_t queue_id, uint8_t item_id);

/**
 * ppe_drv_fse_ops_unregister
 *	Unregister fse ops with ppe driver.
 *
 * @return
 * none.
 */
void ppe_drv_fse_ops_unregister(void);

/**
 * ppe_drv_fse_ops_register
 *	Register fse ops with ppe driver.
 *
 * @param[IN] ops Pointer to FSE operation structure in PPE.
 *
 * @return
 * true or false.
 */
bool ppe_drv_fse_ops_register(struct ppe_drv_fse_ops *ops);

/**
 * ppe_drv_fse_feature_enable
 *	Enable PPE-FSE feature.
 *
 * @return
 * none.
 */
void ppe_drv_fse_feature_enable(void);

/**
 * ppe_drv_fse_feature_disable
 *	Disable PPE-FSE feature.
 *
 * @return
 * none.
 */
void ppe_drv_fse_feature_disable(void);

/**
 * ppe_drv_nsm_sawf_sc_stats_read()
 *	Update stats in NSM for given service class
 *
 * @param[IN] nsm_stats		Pointer to stats structure in NSM.
 * @param[IN] service_class	Service class corresponding to which stats are needed.
 *
 * @return
 * Status of the API.
 */
extern bool ppe_drv_nsm_sawf_sc_stats_read(struct ppe_drv_nsm_stats *nsm_stats, uint8_t service_class);

/**
 * ppe_drv_notifier_register
 *	Register notifier ops with ppe driver.
 *
 * @param[IN] ops pointer to notifier structure in PPE.
 *
 * @return
 * none.
 */
extern void ppe_drv_notifier_ops_register(struct ppe_drv_notifier_ops *notifier_ops);

/**
 * ppe_drv_notifier_unregister
 *	Register notfier ops with ppe driver.
 *
 * @param[IN] ops pointer to notifier structure in PPE.
 *
 * @return
 * none.
 */
extern void ppe_drv_notifier_ops_unregister(struct ppe_drv_notifier_ops *notifier_ops);

/**
 * ppe_drv_is_mht_dev
 *	Check the ppe iface MHT switch port flag.
 *
 * @datatypes
 * net_device
 *
 * @param[in] dev	netdevice pointer.
 *
 * @return
 * true or false
 */
bool ppe_drv_is_mht_dev(struct net_device *dev);

/**
 * ppe_drv_is_veip_dev
 *	Check if the net device maps to a VEIP PPE interface.
 *
 * @datatypes
 * net_device
 *
 * @param[in] dev	netdevice pointer.
 *
 * @return
 * true or false
 */
bool ppe_drv_is_veip_dev(struct net_device *dev);

/*
 * ppe_drv_mht_port_from_fdb()
 *	Get the port id corresponding to the destination
 *	mac address and vid
 */
int32_t ppe_drv_mht_port_from_fdb(uint8_t *dmac, uint16_t vid);

/**
 * ppe_drv_isram_queue_profile_init
 *	API to set ISRAMQ base for all the PPE ports
 *
 * @param[in] isram_qbase	ISRAM queue base
 *
 * @return
 * true or false
 */
bool ppe_drv_isram_queue_profile_init(uint8_t isram_qbase);

/**
 * ppe_drv_ds_map_node_to_queue
 *	Provides node to queue mapping.
 *
 * @param[in] node_id	node id.
 * @param[in] queue_id	queue_id.
 *
 * @return
 * Success or error code.
 */
ppe_drv_ret_t ppe_drv_ds_map_node_to_queue(uint8_t node_id, uint8_t queue_id);

/**
 * ppe_drv_ds_map_free
 *	Provides unmapping of node with enqueue VP and queue.
 *
 * @param[in] node_id	Node ID.
 *
 * @return
 * Success or error code.
 */
ppe_drv_ret_t ppe_drv_ds_map_free(uint8_t node_id);

/**
 * ppe_drv_get_qos_tag
 * 	Returns QoS tag
 *
 * @datatypes
 * int32_t
 *
 * @param[in] flow_index	Flow index.
 *
 * @return
 * Net device
 */
uint32_t ppe_drv_get_qos_tag(int32_t flow_index);

/**
 * ppe_drv_get_and_hold_qdisc_netdev
 *	Holds and returns netdev of flow index.
 *
 * @datatypes
 * int32_t
 *
 * @param[in] flow_index	Flow index.
 *
 * @return
 * Net device
 */
struct net_device *ppe_drv_get_and_hold_qdisc_netdev(int32_t flow_index);

/**
 * ppe_drv_get_qdisc_rule_flag
 *	Returns flags for qdisc of flow index.
 *
 * @datatypes
 * int32_t
 *
 * @param[in] flow_index	Flow index.
 *
 * @return
 * Qdisc rule flag
 */
int8_t ppe_drv_get_qdisc_rule_flag(int32_t flow_index);

/**
 * ppe_drv_wlan_rfs_enable_set
 *	Set global wlan rfs enable variable.
 *
 * @param[in] enable		W-LAN rfs flows are enabled
 *
 * @return
 * none.
 */
void ppe_drv_wlan_rfs_enable_set(bool enable);

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
/**
 * ppe_drv_get_tpid_index
 *	Return TPID table index for the given TPID value.
 *
 * @datatypes
 * uint16_t
 *
 * @param[in] tpid   TPID value to look up.
 * @param[in] type   TPID type string (e.g., "stag", "ctag"), used for logging.
 *
 * @return
 * TPID index (0-based) on success; negative value on failure
 * (e.g., TPID not supported or type invalid).
 */
int ppe_drv_get_tpid_index(uint16_t tpid, const char *type);

/**
 * ppe_drv_get_vsi_num
 *	Get the number of VSIs supported by the PPE driver.
 *
 * @datatypes
 * int
 *
 * @return
 * Total number of supported VSIs (>=0). Returns a negative value on error.
 * Valid VSI indices range from 0 to (returned_value - 1).
 */
int ppe_drv_get_vsi_num(void);
#endif

/**
 * ppe_drv_set_ddrq_en_bitmask
 *	Set the PPE global DDRQ enable bitmask.
 *
 * @param[in] q_base Queue base
 * @param[in] q_cnt Queue count
 * @param[in] state DDRQ state
 *
 * @return
 * Success or error code.
 */
int ppe_drv_set_ddrq_en_bitmask(uint32_t q_base, uint32_t q_cnt, bool state);

/*
 * ppe_drv_l2vp_sc_add
 * 	Add the sc configs in L2_VP port tbl.
 *
 * @datatypes
 * int32_t
 *
 * @param[in] dest_port_id    UNI port id for PON.
 *
 * @return
 * Success or error code.
 */
bool ppe_drv_l2vp_sc_add(uint32_t dest_port_id);

/*
 * ppe_drv_l2vp_sc_rmv()
 *      Remove the sc configs in L2_VP port tbl.
 *
 * @datatypes
 * int32_t
 *
 * @param[in] dest_port_id    UNI port id for PON.
 *
 * @return
 * Success or error code.
 */
bool ppe_drv_l2vp_sc_rmv(uint32_t dest_port_id);

/**
 * ppe_drv_confgiure_ucast_prio_map_tbl
 *	Configure unicast priority map table for RFS/DS flows
 *
 * @param[in] profile_id Profile ID for which priority map is to be configured.
 * @param[in] prio_map   Array of priority values.
 *
 * @return
 * true if successful, false otherwise.
 */
bool ppe_drv_confgiure_ucast_prio_map_tbl(uint8_t profile_id, uint8_t *prio_map);

/**
 * ppe_drv_port_mcast_priority_class_set
 *	Set multicast priority to queue class mapping for a port.
 *
 * @param[in] port_id    Port ID (must be < PPE_DRV_PHYSICAL_MAX).
 * @param[in] priority   Priority value (must be < PPE_DRV_MAX_PRIORITY).
 * @param[in] queue_class Queue class (0-(PPE_DRV_MCAST_QUEUE_CLASS_MAX - 1)).
 *
 * @return
 * true if successful, false otherwise.
 */
bool ppe_drv_port_mcast_priority_class_set(uint32_t port_id, uint8_t priority, uint8_t queue_class);

#ifdef NSS_PPE_PON_SUPPORT
/**
 * ppe_drv_pon_get_pq_config
 *	Provides pq to enq_vp and int_pri mapping.
 *
 * @param[in] pq	priority queue.
 * @param[in] enq_vp	enqueue vp.
 * @param[in] int_pri	int_pri.
 *
 * @return
 * true or false
 */
bool ppe_drv_pon_get_pq_config(uint8_t pq, uint8_t *enq_vp, uint8_t *int_pri);

/**
 * ppe_drv_pon_map_enq_vp_to_base_pq
 *	Provides enq_vp mapped to the given base pq.
 *
 * @param[in] base_pq	base priority queue.
 * @param[in] enq_vp	enqueue vp.
 *
 * @return
 * success or error code
 */
ppe_drv_ret_t ppe_drv_pon_map_enq_vp_to_base_pq(uint8_t base_pq, uint8_t *enq_vp);

/**
 * ppe_drv_pon_iface_get
 *	Returns PON intereface.
 *
 * @return
 * Pointer to PON interface.
 */
struct ppe_drv_iface *ppe_drv_pon_iface_get(void);

/**
 * ppe_drv_is_pon_enable
 *	Checks whether PON is enabled on this RDP.
 *
 * @return
 * true if PON interface is set, false otherwise.
 */
bool ppe_drv_is_pon_enable(void);
#endif

/**
 * ppe_drv_pon_port_max_pq_get
 *	Returns maximum PQs assigned to PON port
 *
 * @return
 * Number of PQs.
 */
int ppe_drv_pon_port_max_pq_get(void);

/**
 * ppe_drv_pon_port_start_pq_get
 *	Returns start (base) queue number assigned to PON port
 *
 * @return
 * Start PQ number.
 */
int ppe_drv_pon_port_start_pq_get(void);

/**
 * ppe_drv_hw_gro_feature_set
 *	Set HW GRO feature state
 *
 * @param[in] dev	Netdevice on which HW GRO is enabled or disabled
 * @param[in] enable	Enable or disable for HW GRO
 *
 * @return
 * none.
 */
bool ppe_drv_hw_gro_feature_set(struct net_device *dev, bool enable);

/**
 * ppe_drv_get_unknown_dst_rfs_enable
 *	Get the unknown_dst_rfs_enable value
 *
 * @return
 * unknown_dst_rfs_enable
 */
int ppe_drv_get_unknown_dst_rfs_enable(void);

/*
 * ppe_drv_l2vp_mirror_sc_add
 *      Add the sc configs in L2_VP port tbl.
 *
 * @datatypes
 * int32_t
 *
 * @param[in] dest_port_id    UNI port id for PON.
 *
 * @return
 * Success or error code.
 */
bool ppe_drv_l2vp_mirror_sc_add(uint32_t dest_port_id);

/*
 * ppe_drv_l2vp_mirror_sc_rmv()
 *      Remove the sc configs in L2_VP port tbl.
 *
 * @datatypes
 * int32_t
 *
 * @param[in] dest_port_id    UNI port id for PON.
 *
 * @return
 * Success or error code.
 */
bool ppe_drv_l2vp_mirror_sc_rmv(uint32_t dest_port_id);

#endif /* _PPE_DRV_H_ */
