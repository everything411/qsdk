/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_drv_v4.h
 *	NSS PPE IPv4 specific definitions.
 */

#ifndef _PPE_DRV_V4_H_
#define _PPE_DRV_V4_H_

/**
 * @addtogroup ppe_drv_v4_subsystem
 * @{
 */

/*
 * v4 rule flags
 */
#define PPE_DRV_V4_RULE_FLAG_BRIDGE_FLOW		0x00000001	/**< Bridge Flow */
#define PPE_DRV_V4_RULE_FLAG_ROUTED_FLOW		0x00000002	/**< Rule is for a routed connection */
#define PPE_DRV_V4_RULE_FLAG_DSCP_MARKING		0x00000004	/**< Rule creation for DSCP marking */
#define PPE_DRV_V4_RULE_FLAG_VLAN_MARKING		0x00000008	/**< Rule creation for VLAN marking */
#define PPE_DRV_V4_RULE_FLAG_FLOW_VALID			0x00000010	/**< Rule creation for flow direction */
#define PPE_DRV_V4_RULE_FLAG_RETURN_VALID		0x00000020	/**< Rule creation for return direction */
#define PPE_DRV_V4_RULE_FLAG_PPPOE_VALID		0x00000040	/**< Rule creation for PPPoe */
#define PPE_DRV_V4_RULE_FLAG_DS_FLOW			0x00000080	/**< Rule creation for DS flow */
#define PPE_DRV_V4_RULE_FLAG_VP_FLOW			0x00000100	/**< Rule creation for VP flow */
#define PPE_DRV_V4_RULE_FLAG_SRC_INTERFACE_CHECK	0x00000200	/**< Rule creation for source interface check */
#define PPE_DRV_V4_RULE_TO_BRIDGE_VLAN_NETDEV		0x00000400	/**< VLAN over bridge in egress direction */
#define PPE_DRV_V4_RULE_FROM_BRIDGE_VLAN_NETDEV		0x00000800	/**< VLAN over bridge in ingress direction */
#define PPE_DRV_V4_RULE_NOEDIT_FLOW_RULE		0x00001000	/**< Noedit rule creation for flow direction */
#define PPE_DRV_V4_RULE_NOEDIT_RETURN_RULE		0x00002000	/**< Noedit rule creation for return direction */
#define PPE_DRV_V4_RULE_FLAG_PASSIVE_FLOW		0x00004000	/**< Rule creation for passive flow */
#define PPE_DRV_V4_RULE_ORIG_FLOW_SW_MDATA_VALID	0x00008000	/**< Rule creation for Socket Offload Original flows */
#define PPE_DRV_V4_RULE_RET_FLOW_SW_MDATA_VALID		0x00010000	/**< Rule creation for Socket Offload Return flows */
#define PPE_DRV_V4_RULE_FLAG_IPSEC_DECAP_FLOW		0x00020000	/**< Rule creation for IPsec Decap flow */
#define PPE_DRV_V4_RULE_FLAG_ESP_PASS_THROUGH_SPI	0x00040000	/**< ESP SPI information is valid. */
#define PPE_DRV_V4_RULE_FLAG_UDP_ST_TX_FLOW		0x00080000	/**< Rule creation for UDP-ST TX flow */
#define PPE_DRV_V4_RULE_FLAG_UDP_ST_RX_FLOW		0x00100000	/**< Rule creation for UDP-ST RX flow */

/*
 * v4 valid flags
 */
#define PPE_DRV_V4_VALID_FLAG_FLOW_PPPOE	0x00000001  /**< PPPoE fields are valid for flow direction. */
#define PPE_DRV_V4_VALID_FLAG_RETURN_PPPOE	0x00000002  /**< PPPoE fields are valid for return direction. */
#define PPE_DRV_V4_VALID_FLAG_VLAN		0x00000004  /**< VLAN fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_QOS		0x00000008  /**< QoS fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_DSCP_MARKING	0x00000010  /**< DSCP fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_SAWF		0x00000020  /**< SAWF fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_ACL_POLICER	0x00000040  /**< ACL/Policer fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_WIFI_TID		0x00000080	/**< TID fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_SCS		0x00000100	/**< SCS fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_FLOW_WIFI_MDATA	0x00000200  /**< Wi-Fi flow metadata is valid. */
#define PPE_DRV_V4_VALID_FLAG_RETURN_WIFI_MDATA	0x00000400  /**< Wi-Fi return metadata is valid. */
#define PPE_DRV_V4_VALID_FLAG_FLOW_WIFI_DS	0x00000800  /**< Wi-Fi DS flow fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_RETURN_WIFI_DS	0x00001000  /**< Wi-Fi DS return fields are valid. */
#define PPE_DRV_V4_VALID_FLAG_WIFI_TAG		0x00002000  /**< Wi-Fi valid flag. */
#define PPE_DRV_V4_VALID_FLAG_FLOW_HOST_QDISC_VALID	0x00004000	/**< Qdisc info is valid in flow direction. */
#define PPE_DRV_V4_VALID_FLAG_RETURN_HOST_QDISC_VALID	0x00008000	/**< Qdisc info is valid in return direction. */
#define PPE_DRV_V4_VALID_FLAG_UNIDIR_RULE		0x00010000	/**< Unidirectional rule is valid. */
#define PPE_DRV_V4_VALID_FLAG_FLOW_E2E_HOST_ASSIST_QDISC	0x00020000
					/**< Qdisc info is valid for eth to eth host assisted qdisc in flow direction. */
#define PPE_DRV_V4_VALID_FLAG_RETURN_E2E_HOST_ASSIST_QDISC	0x00040000
					/**< Qdisc info is valid for eth to eth host assisted qdisc in return direction. */

#define PPE_DRV_V4_MAX_CONN_COUNT		2048

/**
 * ppe_drv_v4_5tuple
 *	Common 5-tuple information.
 */
struct ppe_drv_v4_5tuple {
	uint32_t flow_ip;		/**< Flow IP address. */
	uint32_t flow_ident;		/**< Flow identifier (e.g., TCP or UDP port). */
	uint32_t return_ip;		/**< Return IP address. */
	uint32_t return_ident;		/**< Return identier (e.g., TCP or UDP port). */
	uint32_t flow_rule_id;		/**< Unique number defined in connection manager. */
	uint8_t protocol;		/**< Protocol number. */
};

/**
 * ppe_drv_v4_connection_rule
 *	Information for creating a connection.
 */
struct ppe_drv_v4_connection_rule {
	uint8_t flow_mac[ETH_ALEN];	/**< Flow MAC address. */
	uint8_t return_mac[ETH_ALEN];	/**< Return MAC address. */
	uint32_t flow_mtu;		/**< MTU for the flow interface. */
	uint32_t return_mtu;		/**< MTU for the return interface. */
	uint32_t flow_ip_xlate;		/**< Translated flow IP address. */
	uint32_t return_ip_xlate;	/**< Translated return IP address. */
	uint32_t flow_ident_xlate;	/**< Translated flow identifier (e.g., port). */
	uint32_t return_ident_xlate;	/**< Translated return identifier (e.g., port). */
	ppe_drv_iface_t rx_if;		/**< From PPE port number */
	ppe_drv_iface_t tx_if;		/**< To PPE port number */
};

/**
 * ppe_drv_v4_rule_update_msg
 *	PPE IPv4 rule update structure
 */
struct ppe_drv_v4_rule_update_msg {
	struct ppe_drv_v4_5tuple tuple;		/**< Holds values of the original 5 tuple. */
	uint8_t rule_type;			/**< Type of rule to be updated. */

	union ppe_drv_v4_update_info {
		struct ppe_drv_unidir_update_info unidir;	/**< Unidirectional update related information. */
		struct ppe_drv_service_class_rule sawf;		/**< Service class related information. */
	} info;
};

/**
 * ppe_drv_v4_rule_create
 *	PPE IPv4 rule create structure.
 */
struct ppe_drv_v4_rule_create {
	/*
	 * Request
	 */
	uint32_t valid_flags;				/**< Bit flags associated with the validity of parameters. */
	uint32_t rule_flags;				/**< Bit flags associated with the rule. */
	struct ppe_drv_v4_5tuple tuple;			/**< Holds values of the 5 tuple. */
	struct ppe_drv_v4_connection_rule conn_rule;	/**< Basic connection-specific data. */
	struct ppe_drv_src_mac_rule src_mac_rule;           /**< Source MAC address rule. */
	struct ppe_drv_pppoe_rule pppoe_rule;		/**< PPPoE-related acceleration parameters. */
	struct ppe_drv_qos_rule qos_rule;		/**< QoS-related acceleration parameters. */
	struct ppe_drv_dscp_rule dscp_rule;		/**< DSCP-related acceleration parameters. */
	struct ppe_drv_vlan_rule vlan_rule;		/**< VLAN-related acceleration parameters. */
	struct ppe_drv_top_if_rule top_rule;		/**< Parameters related to the top interface in hierarchy. */
	struct ppe_drv_service_class_rule sawf_rule;	/**< Service class related information. */
	struct ppe_drv_acl_policer_rule ap_rule;	/**< ACL/Policer rule ID information. */
	struct ppe_drv_wifi_mdata_rule wifi_rule;	/**< Wi-Fi metadata rule ID information. */
	struct ppe_drv_vp_dl_qdisc_rule qdisc_rule;	/**< Qdisc rule information for VP DL flow. */
	struct ppe_drv_accel_rule_dir rule_dir;		/**< Information about direction to accelerate. */
	struct ppe_drv_spi_rule spi_rule;		/**< SPI for ESP flow. */

	/*
	 * Response.
	 */
	uint16_t flow_index_orig;			/**< Flow index for Original flow */
	uint16_t flow_index_ret;			/**< FLow index for Return flow. */
};

/**
 * ppe_drv_v4_rule_destroy
 *	PPE IPv4 rule destroy structure.
 */
struct ppe_drv_v4_rule_destroy {
        struct ppe_drv_v4_5tuple tuple;			/**< Holds values of the 5 tuple. */
};

/**
 * ppe_drv_v4_conn_sync
 *	PPE connection sync structure for one connection.
 */
struct ppe_drv_v4_conn_sync {
        uint8_t protocol;				/**< Protocol number. */
        uint32_t flow_ip;				/**< Flow IP address. */
        uint32_t flow_ip_xlate;				/**< Translated flow IP address. */
        uint32_t flow_ident;				/**< Flow ident (e.g. port). */
        uint32_t flow_ident_xlate;			/**< Translated flow ident (e.g. port). */
        uint32_t flow_rx_packet_count;			/**< Flow interface's RX packet count. */
        uint32_t flow_rx_byte_count;			/**< Flow interface's RX byte count. */
        uint32_t flow_tx_packet_count;			/**< Flow interface's TX packet count. */
        uint32_t flow_tx_byte_count;			/**< Flow interface's TX byte count. */
        uint32_t return_ip;				/**< Return IP address. */
        uint32_t return_ip_xlate;			/**< Translated return IP address. */
        uint32_t return_ident;				/**< Return ident (e.g. port). */
        uint32_t return_ident_xlate;			/**< Translated return ident (e.g. port). */
        uint32_t return_rx_packet_count;		/**< Return interface's RX packet count. */
        uint32_t return_rx_byte_count;			/**< Return interface's RX byte count. */
        uint32_t return_tx_packet_count;		/**< Return interface's TX packet count. */
        uint32_t return_tx_byte_count;			/**< Return interface's TX byte count. */
        enum ppe_drv_stats_sync_reason reason;		/**< Reason for the sync. */
	uint32_t flow_rule_id;				/**< Unique number defined in connection manager. */
};

/*
 * ppe_drv_v4_flow_conn_stats
 *	PPE connection stats for a single connection
 */
struct ppe_drv_v4_flow_conn_stats {
        struct ppe_drv_v4_5tuple tuple;			/**< Holds value of 5 tuple. */
        struct ppe_drv_v4_conn_sync conn_sync;		/**< Connection stats. */
};

/*
 * ppe_drv_v4_conn_sync_many
 *	PPE connection sync many structure.
 */
struct ppe_drv_v4_conn_sync_many {
        uint16_t count;					/* How many conn_sync included in this sync callback */
        struct ppe_drv_v4_conn_sync *conn_sync;		/* Connection sync array */
};

/**
 * Callback function for syncing IPv4 connection stats.
 *
 * @datatypes
 * ppe_drv_v4_conn_sync
 *
 * @param[in] app_data    Pointer to the user context registered the callback.
 * @param[in] conn_sync   Pointer to the connection sync message data structure.
 */
typedef void (*ppe_drv_v4_sync_callback_t)(void *app_data, struct ppe_drv_v4_conn_sync *conn_sync);

/**
 * ppe_drv_v4_stats_callback_unregister
 *	API to unregister IPv4 connection stats sync callback.
 *
 * @return
 */
void ppe_drv_v4_stats_callback_unregister(void);

/**
 * ppe_drv_v4_stats_callback_register
 *	API to register IPv4 connection stats sync callback.
 *
 * @param[in] cb         Pointer to the callback function.
 * @param[in] app_data   Pointer to the app data which is passed with the callback.
 *
 * @return
 * Status of the register operation.
 */
bool ppe_drv_v4_stats_callback_register(ppe_drv_v4_sync_callback_t cb, void *app_data);

/**
 * ppe_drv_v4_conn_sync_many
 *	API to get v4 connection stats.
 *
 * @param[in] cn_syn     Pointer to the buffer in which stats are filled.
 * @param[in] num_conn   Number of connection stats which can be filled in one iteration.
 *
 * @return
 * void
 */
void ppe_drv_v4_conn_sync_many(struct ppe_drv_v4_conn_sync_many *cn_syn, uint8_t num_conn);

/**
 * ppe_drv_v4_get_conn_stats
 *	API to get a single connection stats.
 *
 * @param[in] conn_stats     Pointer to the connection stats structure.
 *
 * @return
 * Status of the stats sync.
 */
ppe_drv_ret_t ppe_drv_v4_get_conn_stats(struct ppe_drv_v4_flow_conn_stats *conn_stats);

/**
 * ppe_drv_v4_policer_create
 *	Creates IPv4 policer rule in PPE.
 *
 * @datatypes
 * ppe_drv_v4_policer_create
 *
 * @param[in] create   Pointer to the NSS PPE IPv4 create rule message.
 *
 * @return
 * Status of the create operation.
 */
ppe_drv_ret_t ppe_drv_v4_policer_flow_create(struct ppe_drv_v4_rule_create *create);

/**
 * ppe_drv_v4_policer_destroy
 *	Destroy IPv4 policer rule in PPE.
 *
 * @datatypes
 * ppe_drv_v4_policer_destroy
 *
 * @param[in] destroy   Pointer to the NSS PPE IPv4 destroy rule message.
 *
 * @return
 * Status of the destroy operation.
 */
ppe_drv_ret_t ppe_drv_v4_policer_flow_destroy(struct ppe_drv_v4_rule_destroy *destroy);

/**
 * ppe_drv_v4_rfs_destroy
 * 	Destroy IPv4 RFS rule in PPE.
 *	This function is deprecated, pls use ppe_drv_v4_assist_rule_destroy() instead
 *
 * @datatypes
 * ppe_drv_v4_rule_destroy
 *
 * @param[in] destroy   Pointer to the NSS PPE IPv4 destroy rule message.
 *
 * @return
 * Status of the destroy operation.
 */
ppe_drv_ret_t ppe_drv_v4_rfs_destroy(struct ppe_drv_v4_rule_destroy *destroy);

/**
 * ppe_drv_v4_rfs_create
 * 	Creates IPv4 RFS rule in PPE.
 *	This function is deprecated, pls use ppe_drv_v4_assist_rule_create() instead
 * @datatypes
 * ppe_drv_v4_rule_create
 *
 * @param[in] create   Pointer to the NSS PPE IPv4 create rule message.
 *
 * @return
 * Status of the create operation.
 */
ppe_drv_ret_t ppe_drv_v4_rfs_create(struct ppe_drv_v4_rule_create *create);

/**
 * ppe_drv_v4_assist_rule_destroy
 *	Destroy IPv4 RFS rule in PPE.
 *
 * @param[in] destroy   Pointer to the NSS PPE ASSIST IPv4 destroy rule message.
 *
 * @return
 * Status of the destroy operation.
 */
ppe_drv_ret_t ppe_drv_v4_assist_rule_destroy(struct ppe_drv_v4_rule_destroy *destroy);

/**
 * ppe_drv_v4_assist_rule_create
 *	Creates IPv4 assist rule in PPE.
 *
 * @param[in] create   Pointer to the  PPE ASSIST IPv4 create rule message.
 * @param[in] feature  Assist feature type.
 *
 * @return
 * Status of the create operation.
 */
ppe_drv_ret_t ppe_drv_v4_assist_rule_create(struct ppe_drv_v4_rule_create *create, uint32_t feature);

/**
 * ppe_drv_v4_destroy
 *	Destroys IPv4 connection rule in PPE.
 *
 * @datatypes
 * ppe_drv_v4_rule_destroy
 *
 * @param[in] destroy   Pointer to the NSS PPE IPv4 destroy rule message.
 *
 * @return
 * Status of the destroy operation.
 */
ppe_drv_ret_t ppe_drv_v4_destroy(struct ppe_drv_v4_rule_destroy *destroy);

/**
 * ppe_drv_v4_create
 *	Creates IPv4 connection rule in PPE.
 *
 * @datatypes
 * ppe_drv_v4_rule_create
 *
 * @param[in] create   Pointer to the NSS PPE IPv4 create rule message.
 *
 * @return
 * Status of the create operation.
 */
ppe_drv_ret_t ppe_drv_v4_create(struct ppe_drv_v4_rule_create *create);

/**
 * ppe_drv_v4_rule_update
 *	Update bidirection rule in PPE.
 *
 * @datatypes
 * ppe_drv_v4_rule_update_msg
 *
 * @param[in] update_msg	Pointer to the PPE rule update message.
 *
 * @return
 * Status of the update operation.
 */
ppe_drv_ret_t ppe_drv_v4_rule_update(struct ppe_drv_v4_rule_update_msg *update_msg);

/**
 * ppe_drv_v4_nsm_stats_update
 *	Update stats in NSM for the given 5 tuple.
 *
 * @param[in] nsm_stats		Pointer to stats maintained in NSM.
 * @param[in] tuple		Pointer to 5 tuple.
 *
 * @return
 * Status of the API.
 */
extern bool ppe_drv_v4_nsm_stats_update(struct ppe_drv_nsm_stats *nsm_stats, struct ppe_drv_v4_5tuple *tuple);
/** @} */ /* end_addtogroup ppe_drv_v4_subsystem */

#endif /* _PPE_DRV_V4_H_ */
