/*
 * netfn_capwapmgr.h
 *	Network function's CAPWAP manager public APIs.
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NETFN_CAPWAPMGR_H
#define __NETFN_CAPWAPMGR_H

#include <netfn_flowmgr.h>
#include <netfn_capwap.h>
#include <netfn_dtls_types.h>
#include <netfn_flow_cookie.h>

#define NETFN_CAPWAPMGR_EXT_VALID_VLAN 0x0001		/**< VLAN rule valid */
#define NETFN_CAPWAPMGR_EXT_VALID_PPPOE 0x0002		/**< PPPoE rule valid */
#define NETFN_CAPWAPMGR_EXT_VALID_DMAC_XLATE 0x0004	/**< DMAC Xlate rule valid */
#define NETFN_CAPWAPMGR_EXT_VALID_RESERVED 0x0008	/**< Reserved */
#define NETFN_CAPWAPMGR_EXT_VALID_DTLS_ENC 0x0010	/**< DTLS Encap session valid */
#define NETFN_CAPWAPMGR_EXT_VALID_DTLS_DEC 0x0020	/**< DTLS Decap session valid */

/**
 * netfn_capwapmgr_tun_cfg
 *	Tunnel create configuration.
 */
struct netfn_capwapmgr_tun_cfg {
	netfn_tuple_t tuple;		/**< 5 tuple information */
	struct netfn_flowmgr_flow_info flow;
					/**< Flow rule */
	struct netfn_capwap_tun_cfg capwap;
					/**< CAPWAP rule */

	/* Optional or extended configuration */
	struct {
		uint32_t ext_valid_flags;
					/**< Valid flag */
		struct netfn_flowmgr_vlan_rule vlan;
					/**< Vlan rule */
		struct netfn_flowmgr_pppoe_rule pppoe;
					/**< PPPoE rule */
		struct netfn_flowmgr_mac_xlate_rule mac;
					/**< MAC transalte rule */
		struct netfn_dtls_cfg enc;
					/**< DTLS encap session configuration */
		struct netfn_dtls_cfg dec;
					/**< DTLS decap session configuration */
	} ext_cfg;

	uint32_t reserved[8];		/**< Reserved */
};

/**
 * netfn_capwapmgr_tun_update_t
 *	Tunnel update type.
 */
typedef enum {
	NETFN_CAPWAPMGR_UPDATE_CAPWAP_VER,	/**< Update CAPWAP version */
	NETFN_CAPWAPMGR_UPDATE_DEST_MAC,	/**< Update Destination MAC address */
	NETFN_CAPWAPMGR_UPDATE_MTU,		/**< Update PATH MTU */
	NETFN_CAPWAPMGR_UPDATE_SRC_INTERFACE,	/**< Update Source Interface */
	NETFN_CAPWAPMGR_UPDATE_DTLS_ENABLE,	/**< Add DTLS config */
	NETFN_CAPWAPMGR_UPDATE_DTLS_DISABLE,	/**< Add DTLS config */
	NETFN_CAPWAPMGR_UPDATE_DTLS_ENCAP_SESSION,
						/**< Update DTLS encap session */
	NETFN_CAPWAPMGR_UPDATE_DTLS_DECAP_SESSION,
						/**< Update DTLS deecap session */
	NETFN_CAPWAPMGR_ADD_NETFN_FLOW_COOKIE,
                                                /**< Add Netfn Flow Cookie */
	NETFN_CAPWAPMGR_DEL_NETFN_FLOW_COOKIE,
						/**< Delete Netfn Flow Cookie */
	NETFN_CAPWAPMGR_DTLS_ENCAP_SESSION_SWITCH,
						/**< Switch DTLS encap session */
	NETFN_CAPWAPMGR_DTLS_DECAP_SESSION_SWITCH,
						/**< Switch DTLS decap session */
} netfn_capwapmgr_tun_update_t;

/**
 * netfn_capwapmgr_fc_info
 *      Flow Cookie Config Information.
 */
struct netfn_capwapmgr_fc_info {
	netfn_tuple_t tuple;		/**< Tuple Information */
	struct netfn_flow_cookie nfc;	/**< Flow Cookie */
};

/**
 * netfn_capwapmgr_tun_update
 *	Tunnel Configuration update.
 */
struct netfn_capwapmgr_tun_update {
	netfn_capwapmgr_tun_update_t type;
	union {
		struct {
			struct netfn_dtls_cfg enc;
					/**< DTLS encap session */
			struct netfn_dtls_cfg dec;
					/**< DTLS decap session */
		} dtls;

		struct netfn_capwapmgr_fc_info fci;
					/**< Flow Cookie Config Info */
		struct net_device *dev;
					/**< Source interface */
		uint32_t mtu;		/**< Path MTU */
		uint8_t dest_mac[ETH_ALEN];
					/**< Destination MAC */
		uint8_t ver;		/**< CAPWAP version */
		uint8_t reserved[7];	/**< Reserved */
	} update_cfg;
};

/*
 * netfn_capwapmgr_flow_stats
 *	Flow offload stats assocaited with the tunnel.
 */
struct netfn_capwapmgr_flow_stats {
	uint64_t flow_tx_pkts;		/**< Flow interface Tx packets */
	uint64_t flow_tx_bytes;		/**< Flow interface Tx bytes */
	uint64_t flow_rx_pkts;		/**< Flow interface Rx packets */
	uint64_t flow_rx_bytes;		/**< Flow interface Rx bytes */

	uint64_t return_tx_pkts;	/**< Return interface Tx packets */
	uint64_t return_tx_bytes;	/**< Return interface Tx bytes */
	uint64_t return_rx_pkts;	/**< Return interface Rx packets */
	uint64_t return_rx_bytes;	/**< Return interface Rx bytes */

	uint64_t reserved[2];		/**< Reserved */
};

/**
 * netfn_capwapmgr_tun_stats
 *	Tunnel Packet statistics.
 */
struct netfn_capwapmgr_tun_stats {
	struct netfn_capwapmgr_flow_stats flow;	/**< Flow offload statistics */
	struct netfn_capwap_tun_stats capwap;	/**< CAPWAP offload statistics */
};

/*
 * netfn_capwapmgr_ret_t
 *	CAPWAP manager return status.
 */
typedef enum {
       NETFN_CAPWAPMGR_SUCCESS = 0,				/**< Success */
       NETFN_CAPWAPMGR_ERROR_NULL_WAN_NDEV = 1,			/**< NULL WAN net device */
       NETFN_CAPWAPMGR_ERROR_NULL_TOP_NDEV = 2,			/**< NULL top net device */
       NETFN_CAPWAPMGR_ERROR_UNSUPPORTED_TUPLE_TYPE = 3,	/**< Unsupported tuple type */
       NETFN_CAPWAPMGR_ERROR_UNSUPPORTED_L4_PROTO = 4,		/**< Unsupported l4 protocol */
       NETFN_CAPWAPMGR_ERROR_TUN_ALLOC = 5,			/**< Memory allocation for tunnel ctx failed */
       NETFN_CAPWAPMGR_ERROR_TUN_FREE = 6,			/**< Tunnel free failed in offload engine */
       NETFN_CAPWAPMGR_ERROR_FLOW_RULE_CREATE = 7,		/**< Flow rule create failed */
       NETFN_CAPWAPMGR_ERROR_FLOW_RULE_DESTROY = 8,		/**< Flow rule destroy failed */
       NETFN_CAPWAPMGR_ERROR_TUNID_ADD = 9,			/**< Add tunnel under tunid dev */
       NETFN_CAPWAPMGR_ERROR_TUNID_DEL = 10,			/**< Delete tunnel under tunid dev */
       NETFN_CAPWAPMGR_ERROR_TUNNEL_CONTEXT_GET = 11,		/**< Failed to get tunnel context */
       NETFN_CAPWAPMGR_ERROR_TUNID_FREE = 12,			/**< Failed to free tunid capwap dev */
       NETFN_CAPWAPMGR_ERROR_DTLS_ALLOC = 13,			/**< Failed to allocated DTLS tunnel */
       NETFN_CAPWAPMGR_ERROR_DTLS_BIND = 14,			/**< Failed to Bind DTLS Net Device to capwap Net Device */
       NETFN_CAPWAPMGR_ERROR_STATS_GET = 15,			/**< Failed to get tunnel stats */
       NETFN_CAPWAPMGR_ERROR_INVALID_CFG = 16,			/**< Invalid tunnel create configuration */
       NETFN_CAPWAPMGR_ERROR_TUN_ENABLED = 17,			/**< Updating tunnel config when tunnel is enabled */
       NETFN_CAPWAPMGR_ERROR_TUN_DEINIT = 18,			/**< Failed to Deinitialize the tunnel */
       NETFN_CAPWAPMGR_ERROR_TUN_INIT = 19,			/**< Failed to Initialize the tunnel */
       NETFN_CAPWAPMGR_ERROR_DTLS_CFG = 20,			/**< Invalid DTLS configuration */
       NETFN_CAPWAPMGR_ERROR_DTLS_SESSION_SWITCH = 21,		/**< DTLS Session Switch failed */
       NETFN_CAPWAPMGR_ERROR_DTLS_DECAP_SESSION_ADD = 22,	/**< DTLS Encap Session Add Failed */
       NETFN_CAPWAPMGR_ERROR_DTLS_ENCAP_SESSION_ADD = 23,	/**< DTLS Decap session Add Failed */
       NETFN_CAPWAPMGR_ERROR_DTLS_TUN_NOT_CONFIGURED = 24,	/**< DTLS Tunnel is not configured */
       NETFN_CAPWAPMGR_ERROR_DTLS_ENABLED = 25,			/**< DTLS Tunnel enabled */
       NETFN_CAPWAPMGR_ERROR_TUNID_INACTIVE = 26,		/**< Tunnel ID inactive */
       NETFN_CAPWAPMGR_ERROR_TUNID_OUT_OF_RANGE = 27,		/**< Tunnel ID out of range */
       NETFN_CAPWAPMGR_ERROR_TUNID_ACTIVE = 28,			/**< Tunnel Id Active */
       NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_DB_ALLOC = 29,		/**< Failed to instantiate the Flow Cookie DataBase */
       NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_ADD = 30,		/**< Failed to add the Flow Cookie in Flow Cookie DataBase */
       NETFN_CAPWAPMGR_ERROR_FLOW_COOKIE_DEL = 31,		/**< Failed to delete the Flow Cookie from Flow Cookie DataBase */
       NETFN_CAPWAPMGR_ERROR_CAPWAP_CFG = 32,			/**< Invalid CAPWAP config */
       NETFN_CAPWAPMGR_ERROR_MAX

} netfn_capwapmgr_ret_t;


/*
 * netfn_capwapmgr_tun_alloc()
 *	Allocate a CAPWAP tunnel.
 * @cfg: Tunnel create configuration.
 * @return: Net device associated with the tunnel.
 */
struct net_device *netfn_capwapmgr_tun_alloc(struct netfn_capwapmgr_tun_cfg *cfg);

/*
 * netfn_capwapmgr_tun_free()
 *	Free CAPWAP tunnel.
 * @dev: capwap net device.
 * @return: CAPWAP manager return status.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_free(struct net_device *dev);

/*
 * netfn_capwapmgr_tun_update()
 *	Update tunnel configuration.
 * @dev: capwap net device.
 * @cfg: Update tunnel configuration
 * @return: Net device associated with the tunnel.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_update(struct net_device *dev, struct netfn_capwapmgr_tun_update *cfg);

/*
 * netfn_capwapmgr_tun_get_dtls_dev()
 *	Get the DTLS dev associated with the tunnel.
 * @dev: capwap net device.
 * @return: DTLS net device.
 */
struct net_device *netfn_capwapmgr_tun_get_dtls_dev(struct net_device *dev);

/*
 * netfn_capwapmgr_tun_get_stats()
 *	Get stats associated with capwap tunnel
 * @dev: CAPWAP dev
 * @tun_id: Tunnel ID.
 * @stats: Tunnel stats to be filled.
 * @return: CAPWAP manager return status.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tun_get_stats(struct net_device *dev, struct netfn_capwapmgr_tun_stats *stats);

/*
 * netfn_capwapmgr_tunid_dev_alloc()
 *	Allocate a CAPWAP tunid dev.
 * @void:
 * @return: tunid CAPWAP net device.
 */
struct net_device *netfn_capwapmgr_tunid_dev_alloc(void);

/*
 * netfn_capwapmgr_tunid_dev_free()
 *	Free CAPWAP tunid dev.
 * @dev: tunid capwap net device.
 * @return: capwap manager return status.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_dev_free(struct net_device *dev);

/*
 * netfn_capwapmgr_tunid_add()
 *	Adds a new tunnel with specific ID under the tunid dev.
 * @dev: tunid capwap dev.
 * @tun_id: tunnel ID.
 * @cfg: Tunnel confgiruation.
 * @return: capwap manager return type
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_add(struct net_device *dev, uint8_t tun_id, struct netfn_capwapmgr_tun_cfg *cfg);

/*
 * netfn_capwapmgr_tunid_del()
 *	Deletes the tunnel with a specific ID under the tunid dev.
 * @dev: tunid CAPWAP dev
 * @return: capwap manager return type.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_del(struct net_device *dev, uint8_t tun_id);

/*
 * netfn_capwapmgr_tunid_get_dtls_dev()
 *	Get DTLS dev associated with the tunnel
 * @dev: tunid CAPWAP dev
 * @tun_id: Tunnel Id
 * @return: DTLS device associated with the tunnel id.
 */
struct net_device *netfn_capwapmgr_tunid_get_dtls_dev(struct net_device *dev, uint8_t tun_id);

/*
 * netfn_capwapmgr_tunid_update()
 *	Update tunnel configuration
 * @dev: tunid CAPWAP dev.
 * @tun_id: Tunnel ID.
 * @cfg: Tunnel update configuration.
 * @return: CAPWAP manager return status.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_update(struct net_device *dev, uint8_t tun_id, struct netfn_capwapmgr_tun_update *cfg);

/*
 * netfn_capwapmgr_tunid_get_stats()
 *	Get stats associated with the tunnel ID
 * @dev: tunid CAPWAP dev
 * @tun_id: Tunnel ID.
 * @stats: Tunnel stats to be filled.
 * @return: CAPWAP manager return status.
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_get_stats(struct net_device *dev, uint8_t tun_id, struct netfn_capwapmgr_tun_stats *stats);

/*
 * netfn_capwapmgr_tunid_toggle_state()
 *	Enable/disable a tunid tunnel
 *@dev: Tunnel ID dev.
 *@tun_id: Tunnel ID
 *@enable: Swith to enable/disable the tunnel
 *@return: CAPWAP manager return status
 */
netfn_capwapmgr_ret_t netfn_capwapmgr_tunid_toggle_state(struct net_device *dev, uint8_t tun_id, bool enable);
#endif /* __NETFN_CAPWAPMGR_H */
