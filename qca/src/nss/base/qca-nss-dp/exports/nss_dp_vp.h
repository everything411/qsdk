/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_DP_VP_H__
#define __NSS_DP_VP_H__

#include <linux/bitmap.h>
#include <ppe_drv_public.h>

#define NSS_DP_VP_TX_FLAG_PKT_MODE BIT(0)		/**< Packet mode is set */

/*
 * nss_dp_vp_pkt_mode
 *	VP TX DDRQ packet mode.
 *	Values match EDMA hardware: EDMA_TXDESC_PASS_THROUGH_MODE_192B=0x0,
 *	EDMA_TXDESC_PASS_THROUGH_MODE_128B=0x1, EDMA_TXDESC_PASS_THROUGH_MODE_0B=0x2,
 *	EDMA_TXDESC_PASS_THROUGH_MODE_FULL_DATA=0x3
 */
enum nss_dp_vp_pkt_mode {
	NSS_DP_VP_PKT_MODE_192 = 0,	/**< DDRQ passthrough mode 192 bytes */
	NSS_DP_VP_PKT_MODE_128 = 1,	/**< DDRQ passthrough mode 128 bytes */
	NSS_DP_VP_PKT_MODE_0 = 2,	/**< DDRQ passthrough mode 0 byte */
	NSS_DP_VP_FULL_PKT_MODE = 3,	/**< DDRQ full packet mode */
};

/*
 * nss_dp_vp_tx_info
 *	VP Tx info.
 */
struct nss_dp_vp_tx_info {
	uint32_t flags;				/**< VP Tx flags. */
	uint8_t sc;				/**< Service code. */
	uint8_t svp;				/**< Source VP number. */
	uint8_t dvp;				/**< Destination VP number. */
	uint8_t egress_macid;			/**< Egress Port Mac Id. */
	enum nss_dp_vp_pkt_mode pkt_mode;	/**< Packet mode */
	bool fake_mac;			/**< Needs Fake Mac. */
};

#define NSS_DP_VP_MAX_BUF_DESCS  (MAX_SKB_FRAGS + 1)

/*
 * nss_dp_vp_rx_type
 *	VP Rx payload type
 */
enum nss_dp_vp_rx_type {
	NSS_DP_VP_RX_TYPE_SKB = 0,	/**< Payload type is SKB */
	NSS_DP_VP_RX_TYPE_SKB_LIST,	/**< Payload type is SKB list */
	NSS_DP_VP_RX_TYPE_XDP,		/**< Payload type is a single XDP buffer */
	NSS_DP_VP_RX_TYPE_XDP_VEC,	/**< Payload type is an array of XDP buffers */
};

/*
 * nss_dp_vp_rx_data
 *	VP Rx data container
 */
struct nss_dp_vp_rx_data {
	enum nss_dp_vp_rx_type type;		/* Payload type: SKB, list, or pages */
	union {
		struct sk_buff *skb;		/* Single SKB (NSS_DP_VP_RX_TYPE_SKB) */
		struct sk_buff_head *skb_head;	/* SKB list head (NSS_DP_VP_RX_TYPE_SKB_LIST) */
		struct xdp_buff *xdp;		/* Single XDP payload (NSS_DP_VP_RX_TYPE_XDP) */
		struct xdp_buff **xdp_vec;	/* Array of XDP buffers (NSS_DP_VP_RX_TYPE_XDP_VEC) */
	};
};

/*
 * nss_dp_vp_rx_hw_gro_bit
 *      HW GRO flags in nss_dp_vp_rx_info.hw_gro_flags.
 */
enum nss_dp_vp_rx_hw_gro_bit {
	NSS_DP_VP_RX_HW_GRO_EN_BIT = 0,		/* HW GRO is enabled */
	NSS_DP_VP_RX_HW_GRO_MORE_BIT,		/* HW GRO more segments */
	NSS_DP_VP_RX_HW_GRO_TCP_FIN_BIT,	/* HW GRO fin segment */
	NSS_DP_VP_RX_HW_GRO_TCP_PSH_BIT,	/* HW GRO psh segment */
	NSS_DP_VP_RX_HW_GRO_MAX,
};

#define NSS_DP_VP_RX_HW_GRO_EN		BIT(NSS_DP_VP_RX_HW_GRO_EN_BIT)
#define NSS_DP_VP_RX_HW_GRO_MORE	BIT(NSS_DP_VP_RX_HW_GRO_MORE_BIT)
#define NSS_DP_VP_RX_HW_GRO_TCP_FIN	BIT(NSS_DP_VP_RX_HW_GRO_TCP_FIN_BIT)
#define NSS_DP_VP_RX_HW_GRO_TCP_PSH	BIT(NSS_DP_VP_RX_HW_GRO_TCP_PSH_BIT)

/*
 * nss_dp_vp_rx_info
 *	VP info struct struct
 */
struct nss_dp_vp_rx_info {
	struct napi_struct *napi;	/* RX NAPI */
	uint32_t total_bytes;		/* Total bytes carried by batch of skbs or pages */
	int32_t flow_idx;		/* Flow index of a packet */
	uint16_t l3offset;		/* L3 offset of packet */
	uint8_t dvp;			/* Destination VP number */
	uint8_t svp;			/* Source VP number */
	uint8_t ip_summed;		/* IP checksum */
	uint8_t fake_mac:1,		/* Fake Mac Present */
		qdisc_valid:1,		/* Qdisc valid */
		reserved:6;		/* Reserved */
	uint32_t hw_gro_flags;		/* HW GRO flags (NSS_DP_VP_HW_GRO_FLAGS_*) */
	uint8_t service_code;		/* Service code */
};

/*
 * nss_dp_vp_node_info
 *	PPE VP node info
 */
struct nss_dp_vp_node_info {
	uint32_t bytes;			/* Total bytes carried batch of skbs */
	uint8_t dvp;			/* Destination VP */
};

/*
 * nss_dp_vp_node
 *	Node for VP specific operations(batching)
 */
struct nss_dp_vp_node {
	struct sk_buff_head head;		/* Skb list */
	struct nss_dp_vp_node_info info;	/* VP node info */
};

/*
 * nss_dp_vp_rx_cb_t
 *	Vp rx handler callback typedef
 */
typedef void (*nss_dp_vp_rx_cb_t)(struct nss_dp_vp_rx_data *rx_data, struct nss_dp_vp_rx_info *vprxi);

/*
 * nss_dp_vp_ctx
 *	Context per VP node
 */
struct nss_dp_vp_ctx {
	DECLARE_BITMAP(active_vps, PPE_DRV_VIRTUAL_MAX);
	struct nss_dp_vp_node nodes[PPE_DRV_VIRTUAL_MAX];
};

/**
 * nss_dp_vp_rx_register_cb
 *	Register handler for VP rx processing.
 *
 * @datatypes
 * nss_dp_vp_rx_cb_t
 *
 * @param[in] nss_dp_vp_tx_info Pointer to VP rx handler.
 *
 * @return
 * True or false.
 */
bool nss_dp_vp_rx_register_cb(nss_dp_vp_rx_cb_t cb);

/**
 * nss_dp_vp_rx_unregister_cb
 *	Unregister VP handler for VP rx processing.
 *
 * @datatypes
 * None.
 *
 * @param[in] nss_dp_vp_tx_info Pointer to VP rx handler.
 *
 * @return
 * None.
 */
void nss_dp_vp_rx_unregister_cb(void);

/**
 * nss_dp_vp_xmit
 *	Transmits a packet to the appropriate VP netdevice.
 *
 * @datatypes
 * net_device
 * nss_dp_vp_tx_info
 * sk_buff
 *
 * @param[in] net_device Pointer to the netdev structure.
 * @param[in] nss_dp_vp_tx_info Pointer to the VP info structure.
 * @param[in] skb Pointer to the packet.
 *
 * @return
 * Tx status.
 */
netdev_tx_t nss_dp_vp_xmit(struct net_device *netdev, struct nss_dp_vp_tx_info *info, struct sk_buff *skb);

/**
 * nss_dp_vp_init()
 *	Initialize virtual port netdevice.
 *
 * @return
 * Netdevice for the VP port.
 */
struct net_device *nss_dp_vp_init(void);

/**
 * nss_dp_vp_deinit()
 *	De-initialize virtual port netdevice.
 *
 * @return
 * Status of virtual port deinit.
 */
bool nss_dp_vp_deinit(struct net_device *netdev);

#endif	/** __NSS_DP_VP_H__ */
