/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_vp_public.h
 *	NSS PPE VP Public definitions.
 */

#ifndef _PPE_VP_PUBLIC_H_
#define _PPE_VP_PUBLIC_H_

#include <linux/module.h>
#include <net/xdp.h>
#include <ppe_drv_port.h>

struct ppe_drv_iface;

#define PPE_VP_FLAG_DISABLE_TTL_DEC	0x1	/**< Set = TTL Decrement disabled, clear = TTL Decrement enabled */
#define PPE_VP_FLAG_REDIR_ENABLE	0x2	/**< When set, the packets destined to VP are redirect to VP queue without RPS */
#define PPE_VP_FLAG_IPSEC_FULL_INLINE	0x4	/**< Set = IPSEC full inline is enabled, clear is IPsec full inline is not enabled */

#define PPE_VP_DS_INVALID_NODE_ID	0xFF	/**< Invalid node id value */

/**
 * PPE VP field update flags.
 */
#define PPE_VP_UPDATE_FLAG_VP_CORE_MASK		0x1	/**< Flag to indicate core mask update */
#define PPE_VP_UPDATE_FLAG_VP_USR_TYPE		0x2	/**< Flag to indicate user type update */
#define PPE_VP_UPDATE_FLAG_VP_MPSK_EN		0x4	/**< Flag to indicate MPSK bit update */
#define PPE_VP_UPDATE_FLAG_VP_SRC_CB		0x8	/**< Flag to indicate source cb update */
#define PPE_VP_UPDATE_FLAG_VP_SRC_CB_DATA	0x10	/**< Flag to indicate source cb data update */
#define PPE_VP_UPDATE_FLAG_INVALID		0xFF	/**< Flag to indicate invalid update */

/**
 * @addtogroup ppe_vp_public_subsystem
 * @{
 */

/**
 * ppe_vp_hw_stats_t
 *	 PPE VP port statistics.
 */
typedef struct ppe_drv_port_hw_stats ppe_vp_hw_stats_t;

/**
 * ppe_vp_status
 *	Types of PPE VP statuses.
 */
typedef enum ppe_vp_status {
	PPE_VP_STATUS_SUCCESS = 0,	/**< VP Success */
	PPE_VP_STATUS_INVALID_TYPE,	/**< VP Invalid */
	PPE_VP_STATUS_PPEIFACE_ALLOC_FAIL,
					/**< VP PPE iface alloc failed */
	PPE_VP_STATUS_GET_VP_FAIL,	/**< VP PPE init failed */
	PPE_VP_STATUS_MAC_CLEAR_FAIL,	/**< VP PPE init failed */
	PPE_VP_STATUS_MAC_SET_FAIL,	/**< VP PPE init failed */
	PPE_VP_STATUS_MTU_SET_FAIL,	/**< VP PPE init failed */
	PPE_VP_STATUS_VP_INIT_FAIL,	/**< VP PPE init failed */
	PPE_VP_STATUS_VP_DEINIT_FAIL,	/**< VP PPE init failed */
	PPE_VP_STATUS_PORT_INVALID,	/**< VP PPE init failed */
	PPE_VP_STATUS_VP_ALLOC_FAIL,	/**< VP alloc failed */
	PPE_VP_STATUS_VP_FREE_FAIL,	/**< VP free  failed */
	PPE_VP_STATUS_FAILURE,		/**< VP Failure */
	PPE_VP_STATUS_VP_QUEUE_SET_FAILED,
					/**< VP to Queue map failed */
	PPE_VP_STATUS_HW_VP_STATS_CLEAR_FAILED,
					/**< Failed to clear PPE VP hardware statistics */
	PPE_VP_STATUS_UPDATE_FAIL,	/**< VP PPE update failed */
	PPE_VP_STATUS_PORT_ALLOC_FAIL,	/**< PPE port alloc failed */
	PPE_VP_STATUS_PORT_SET_FAIL,	/**< PPE port set failed */
	PPE_VP_STATUS_PORT_LIST_ADD_FAIL,	/** PPE port addition to list failed */
	PPE_VP_STATUS_PORT_LIST_GET_FAIL,	/** PPE port get failed from list */
	PPE_VP_STATUS_PORT_LIST_DEL_FAIL,	/** PPE port deletion from list failed */
	PPE_VP_STATUS_MAX,		/**< Maximum VP statuses */
} ppe_vp_status_t;

typedef int16_t ppe_vp_num_t;

/*
 * ppe_vp_cb_mdata_type
 *	Metadata type
 */
enum ppe_vp_cb_mdata_type {
	PPE_VP_CB_MDATA_TYPE_NONE = 0,		/**< No specific metadata */
	PPE_VP_CB_MDATA_TYPE_HW_GRO = 1,	/**< HW GRO metadata */
};

/*
 * ppe_vp_rx_hw_gro_bit
 *      HW GRO flags in ppe_vp_cb_mdata_info.hw_gro_flags.
 */
enum ppe_vp_rx_hw_gro_bit {
	PPE_VP_RX_HW_GRO_EN_BIT = 0,	/* HW GRO is enabled */
	PPE_VP_RX_HW_GRO_MORE_BIT,	/* HW GRO more segments */
	PPE_VP_RX_HW_GRO_TCP_FIN_BIT,	/* HW GRO fin segment */
	PPE_VP_RX_HW_GRO_TCP_PSH_BIT,	/* HW GRO psh segment */
	PPE_VP_RX_HW_GRO_MAX,
};

#define PPE_VP_RX_HW_GRO_EN		BIT(PPE_VP_RX_HW_GRO_EN_BIT)
#define PPE_VP_RX_HW_GRO_MORE		BIT(PPE_VP_RX_HW_GRO_MORE_BIT)
#define PPE_VP_RX_HW_GRO_TCP_FIN	BIT(PPE_VP_RX_HW_GRO_TCP_FIN_BIT)
#define PPE_VP_RX_HW_GRO_TCP_PSH	BIT(PPE_VP_RX_HW_GRO_TCP_PSH_BIT)

/*
 * ppe_vp_cb_mdata_info
 *	PPE VP metadata info
 */
struct ppe_vp_cb_mdata_info {
	enum ppe_vp_cb_mdata_type mdata_type;	/**< Metadata type */
	uint32_t hw_gro_flags;			/**< HW GRO flags (PPE_VP_RX_HW_GRO_*) */
};

/**
 * ppe_vp_cb_info
 *	Information for VP callback
 *	to process exception packet.
 */
struct ppe_vp_cb_info {
	uint8_t ip_summed;		/**< IP checksum */
	bool fake_mac_present;		/** Fake MAC header present */
	struct sk_buff *skb;		/**< skb */
	struct napi_struct *napi;	/**< RX napi */
	struct net_device *phys_dev;	/**< Physical dev for tunnel */
	uint32_t flow_idx;		/**< Flow index of a packet */
	uint8_t service_code;		/**< Service code */
	struct ppe_vp_cb_mdata_info mdata_info;	/**< Metadata info */
};

/**
 * ppe_vp_xdp_cb_info
 *	Information for VP XDP callback to process XDP packets.
 */
struct ppe_vp_xdp_cb_info {
	struct xdp_buff *xdp;		/**< Single XDP buffer payload */
	struct xdp_buff **xdp_vec;	/**< Array of XDP buffers */
	uint32_t total_bytes;		/**< Total payload bytes across all XDP buffers */
	uint32_t flow_idx;		/**< Flow index of the packet */
	struct ppe_vp_cb_mdata_info mdata_info;	/**< Metadata info (e.g. HW GRO) */
};

/**
 * Callback function for VP Rx.
 *
 * @datatypes
 * net_device
 * sk_buff
 *
 * @param[in] ppe_vp_cb_info	Pointer to exception information.
 * @param[in] cb_data		Pointer to the callback data.
 */
typedef bool(*ppe_vp_callback_t)(struct ppe_vp_cb_info *, void *cb_data);

/**
 * Callback function for VP XDP Rx.
 *
 * @param[in] ppe_vp_xdp_cb_info	Pointer to XDP callback information.
 * @param[in] cb_data			Pointer to the callback data.
 */
typedef bool(*ppe_vp_xdp_callback_t)(struct ppe_vp_xdp_cb_info *, void *cb_data);

/**
 * Callback function for VP Rx list.
 *
 * @datatypes
 * net_device
 * sk_buff
 *
 * @param[in] net_device  	Pointer to the net device.
 * @param[in] sk_buff_head	Pointer to the skb list head.
 * @param[in] cb_data     	Pointer to the callback data.
 */
typedef bool(*ppe_vp_list_callback_t)(struct net_device *, struct sk_buff_head *, void *cb_data);

/**
 * ppe_vp_type
 *	Types of VPs
 */
typedef enum ppe_vp_type {
	PPE_VP_TYPE_SW_L2,		/**< VP type for L2 SW interfaces */
	PPE_VP_TYPE_SW_L3,		/**< VP type for L3 SW interfaces */
	PPE_VP_TYPE_SW_PO,		/**< VP type for point offload tunnels */
	PPE_VP_TYPE_HW_L2TUN,		/**< VP type for L2 HW tunnels */
	PPE_VP_TYPE_HW_L3TUN,		/**< VP type for L3 HW tunnels */
	PPE_VP_TYPE_MAX,		/**< Maximum VP types */
} ppe_vp_type_t;

/**
 * Callback function for VP HW port statistics.
 *
 * @datatypes
 * net_device
 * ppe_vp_hw_stats_t
 *
 * @param[in] net_device Pointer to the net device.
 * @param[in,out] ppe_vp_hw_stats_t Pointer to PPE-VP HW stats structure.
 */
typedef bool(*ppe_vp_stats_callback_t)(struct net_device *, ppe_vp_hw_stats_t *);

/*
 * ppe_vp_user_type
 *	Types of VPs user
 */
typedef enum ppe_vp_user_type {
	PPE_VP_USER_TYPE_NONE = 0,	/**< Non VP use case >*/
	PPE_VP_USER_TYPE_PASSIVE,	/**< VP for Passive use case */
	PPE_VP_USER_TYPE_ACTIVE,	/**< VP for Active use case */
	PPE_VP_USER_TYPE_DS,		/**< VP for Direct-Switch use case */
	PPE_VP_USER_TYPE_MAX,		/**< Maximum VP User types */
} ppe_vp_user_type_t;

/*
 * ppe_vp_netdev_type
 *	Types of VP netdev
 */
enum ppe_vp_net_dev_type {
	PPE_VP_NET_DEV_TYPE_WIFI = 1,	/**< VP netdev is of type Wi-Fi */
	PPE_VP_NET_DEV_TYPE_NETFN_OL,	/**< NETFN netdev */
	PPE_VP_NET_DEV_TYPE_MAX,		/**< Maximum VP netdev types */
};

/*
 * ppe_vp_net_dev_pvt_flags
 *	Flags of a netdev
 */
enum ppe_vp_net_dev_pvt_flags {
	PPE_VP_NET_DEV_FLAG_IS_MLD = 1,	/**< Is MLD net dev */
	PPE_VP_NET_DEV_FLAG_MAX,		/**< Maximum netdev flags */
};

/**
 * ppe_vp_ui
 *	Data structure for VP update information.
 */
struct ppe_vp_ui {
	enum ppe_vp_user_type usr_type;	/**< VP user type */
	void *src_cb;			/**< Source callback to be registered from VP owner */
	void *cb_data;			/**< Callback data */
	uint8_t mpsk_en;		/**< MPSK indication */
	uint8_t core_mask;		/**< Updated Core to be used for a particular VP flow */
	uint16_t update_flags;		/**< Flags to indicate the update fields */
	ppe_vp_stats_callback_t stats_cb;		/**< Callback function for VP HW port statistics.*/
};

/**
 * ppe_vp_ai
 *	Data structure VP allocation.
 */
struct ppe_vp_ai {
	ppe_vp_type_t type;		/**< VP type */
	ppe_vp_callback_t dst_cb;	/**< VP dst callback */
	ppe_vp_list_callback_t dst_list_cb;	/**< VP dst callback */
	ppe_vp_xdp_callback_t dst_xdp_cb;	/**< VP dst XDP callback */
	void *dst_cb_data;		/**< VP dst callback data */
	ppe_vp_callback_t src_cb;	/**< VP src callback */
	void *src_cb_data;		/**< VP src callback data */
	ppe_vp_stats_callback_t stats_cb;
					/**< VP src callback */
	uint32_t xmit_port;		/**< Physical port number */
	uint32_t flags;			/**< PPE VP flags */
	uint8_t queue_num;		/**< Queue number */
	uint8_t core_mask;		/**< Core to be used for a particular VP flow */
	ppe_vp_status_t status;		/**< VP return status */
	enum ppe_vp_user_type usr_type;	/**< VP user type */
	enum ppe_vp_net_dev_type net_dev_type;
					/**< VP netdev type */
	enum ppe_vp_net_dev_pvt_flags net_dev_flags;
					/**< VP netdev flags */
	bool fdb_learn_enabled;
};

/*
 * ppe_vp_get_netdev_by_port_num()
 *	Get the netdevice for port number.
 *
 * @param[in] port_num   VP port number.
 *
 * @return
 * Netdevice for the port number.
 */
struct net_device *ppe_vp_get_netdev_by_port_num(ppe_vp_num_t port_num);

/*
 * ppe_vp_mac_addr_clear()
 *	Clear the MAC address for the virtual port.
 *
 * @param[in] port_num   VP port number.
 *
 * @return
 * Success status of clearing MAC address.
 */
extern ppe_vp_status_t ppe_vp_mac_addr_clear(ppe_vp_num_t port_num);

/*
 * ppe_vp_mac_addr_set()
 *	Set the MAC address for the virtual port.
 *
 * @param[in] port_num   VP port number.
 * @param[in] mac_addr   MAC Address.
 *
 * @return
 * Success status of setting MAC address.
 */
extern ppe_vp_status_t ppe_vp_mac_addr_set(ppe_vp_num_t port_num, uint8_t *mac_addr);

/*
 * ppe_vp_mtu_get()
 *	Get the MTU for the virtual port.
 *
 * @param[in] port_num   VP port number.
 * @param[in] mtu        Pointer to MTU variable.
 *
 * @return
 * Success status of getting MTU.
 */
extern ppe_vp_status_t ppe_vp_mtu_get(ppe_vp_num_t port_num, uint16_t *mtu);

/*
 * ppe_vp_mtu_set()
 *	Set the MTU for the virtual port.
 *
 * @param[in] port_num   VP port number.
 * @param[in] mtu        MTU.
 *
 * @return
 * Success status of setting MTU.
 */
extern ppe_vp_status_t ppe_vp_mtu_set(ppe_vp_num_t port_num, uint16_t mtu);

/*
 * ppe_vp_free()
 *	Free PPE VP interface.
 *
 * @param[in] port_num   VP port number.
 *
 * @return
 * Success status of VP free.
 */
extern ppe_vp_status_t ppe_vp_free(ppe_vp_num_t port_num);

/*
 * ppe_vp_free_dev()
 *	Free PPE VP interface and its netdevice.
 *
 * @param[in] netdev    VP netdevice pointer.
 *
 * @return
 * Void.
 */
extern void ppe_vp_free_dev(struct net_device *vp_dev);

/*
 * ppe_vp_cfg_update()
 *	Update a PPE VP interface.
 *
 * @param[in] vp_num     VP number.
 * @param[in] vpui       VP update info.
 *
 * @return
 * Status of the API.
 */
extern ppe_vp_status_t ppe_vp_cfg_update(ppe_vp_num_t vp_num, struct ppe_vp_ui *vpui);

/*
 * ppe_vp_alloc()
 *	Allocate a PPE VP interface.
 *
 * @param[in] netdev     VP allocator netdevice.
 * @param[in] vpai       VP allocation info.
 *
 * @return
 * VP number.
 */
extern ppe_vp_num_t ppe_vp_alloc(struct net_device *netdev, struct ppe_vp_ai *vpai);

/*
 * ppe_vp_alloc_dev()
 *	Allocate a PPE VP interface and netdevice.
 *
 * @param[in] netdev     VP allocator netdevice.
 * @param[in] vpai       VP allocation info.
 *
 * @return
 * pointer to ppe vp netdevice.
 */
extern struct net_device *ppe_vp_alloc_dev(struct net_device *netdev, struct ppe_vp_ai *vpai);

/*
 * ppe_vp_user_type_get()
 *	Get the user type associated with the PPE VP interface.
 *
 * @param[in] vp_num VP number.
 *
 * @return
 * User type of VP.
 */
extern ppe_vp_user_type_t ppe_vp_user_type_get(ppe_vp_num_t vp_num);

/*
 * ppe_vp_update_vp_stats_cb()
 * 	Update VP stats callback
 *
 * @param[in] vp_num		VP number.
 * @param[in] stats_cb		Statistics callback function pointer.
 *
 * @return
 * Status of the API.
 */
extern ppe_vp_status_t ppe_vp_update_vp_stats_cb(int16_t vp_num, ppe_vp_stats_callback_t stats_cb);


/**
 * ppe_vp_veip_alloc_vps()
 *      Allocate VP structures for both GW and PON ports.
 *
 * @param[in] ppe_iface         PPE interface.
 * @param[in] gw_port_num       Gateway port number.
 * @param[in] pon_port_num      PON port number.
 * @param[in] netdev            Netdevice for the VP.
 * @param[in] vpai              VP allocation info.
 *
 * @return
 * 0 on success, -1 on failure.
 */
extern int ppe_vp_veip_alloc_vps(struct ppe_drv_iface *ppe_iface, uint8_t gw_port_num, uint8_t pon_port_num,
			  struct net_device *netdev, struct ppe_vp_ai *vpai);

/**
 * ppe_vp_veip_free_vps()
 *      Free VP structures for VEIP.
 *
 * @param[in] ppe_iface         PPE interface.
 *
 * @return
 * None.
 */
extern void ppe_vp_veip_free_vps(struct ppe_drv_iface *ppe_iface);
/** @} */ /* end_addtogroup ppe_vp_public_subsystem */

#endif /* _PPE_VP_PUBLIC_H_ */
