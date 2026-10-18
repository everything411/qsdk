/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <ppe_drv_sc.h>

#define PPE_VP_TX_MACID_TAG		0xBB
#define PPE_VP_TX_MACID_TAG_SHIFT	24
#define PPE_VP_TX_MACID_MASK		0x0000000F
#define PPE_VP_TX_INVALID_MACID		0

/*
 * ppe_vp_tx_type_info
 *	Type of VP TX packet.
 */
enum ppe_vp_tx_type_info {
	PPE_VP_TX_TYPE_NONE = 0,		/**< No specific type */
	PPE_VP_TX_TYPE_IPSEC_ENCAP,		/**< IPsec encapsulation */
	PPE_VP_TX_TYPE_IPSEC_DECAP,		/**< IPsec decapsulation */
	PPE_VP_TX_TYPE_IPSEC_3_TUPLE_ENCAP,		/**< IPsec encapsulation with 3-tuple lookup */
};

/*
 * ppe_vp_tx_info
 *	Generic TX info for PPE VP packet transmission.
 *	Callers provide high-level intent; PPE resolves HW specifics internally.
 */
struct ppe_vp_tx_info {
	uint32_t valid_flags;		/**< Reserved for future use */
	enum ppe_vp_tx_type_info type;	/**< Packet type */
};

/*
 * ppe_vp_tx_set_macid_tag()
 *      API to set the valid macid tag in skb->mark for active VP WLAN connection.
 *
 * @param[IN] mark mark of the skb
 *
 * @return
 * updated mark value with macid tag.
 */
static inline uint32_t ppe_vp_tx_set_macid_tag(uint32_t mark) {
	return ((PPE_VP_TX_MACID_TAG << PPE_VP_TX_MACID_TAG_SHIFT) | mark);
}

/*
 * ppe_vp_tx_get_macid()
 *      API to get macid from skb->mark if it has valid macid tag.
 *
 * @param[IN] mark   mark of the skb.
 *
 * @return
 * macid if tag is valid otherwise return PPE_VP_TX_INVALID_MACID.
 */
static inline uint8_t ppe_vp_tx_get_macid(uint32_t mark) {
	uint8_t mark_tag = mark >> PPE_VP_TX_MACID_TAG_SHIFT;

	if (likely(mark_tag == PPE_VP_TX_MACID_TAG)) {
		return (mark & PPE_VP_TX_MACID_MASK);
	}

	return PPE_VP_TX_INVALID_MACID;
}

/*
 * ppe_vp_tx_to_ppe()
 *      API for PPE VP user to enqueue packet for PPE processing
 *
 * @param[IN] vp_num   VP num corresponding to its interface.
 * @param[IN] skb  Socket buffer to be enqueued.
 *
 * @return
 * true if packet is consumed by the API or false if the packet is not consumed.
 */
bool ppe_vp_tx_to_ppe(int32_t vp_num, struct sk_buff *skb);

/*
 * ppe_vp_tx_to_ppe_by_dev()
 *      API for PPE VP user to enqueue packet for PPE processing on a particular dev.
 *
 * @param[IN] dev	net device on which the packet has to xmit.
 * @param[IN] skb	Socket buffer to be enqueued.
 *
 * @return
 * true if packet is consumed by the API or false if the packet is not consumed.
 */
bool ppe_vp_tx_to_ppe_by_dev(struct net_device *dev, struct sk_buff *skb);

/*
 * ppe_vp_tx_to_vp()
 *      API for transmitting the packets to VP in PPE.
 *
 * @param[IN] vp_num   VP num corresponding to its interface.
 * @param[IN] tx_info  TX info with optional type (encap/decap).
 * @param[IN] skb      Socket buffer to be enqueued.
 *
 * @return
 * true if packet is successfully forwarded to VP in PPE, false if its dropped.
 */
bool ppe_vp_tx_to_vp(int32_t vp_num, struct ppe_vp_tx_info *tx_info, struct sk_buff *skb);

/*
 * ppe_vp_tx_to_ppe_by_sc()
 *      API for PPE VP user to enqueue packet for PPE processing with caller selected service code.
 *
 * @param[IN] vp_num   VP num corresponding to its interface.
 * @param[IN] skb  Socket buffer to be enqueued.
 * @param[IN] sc   Service code to use for PPE enqueue.
 *
 * @return
 * true if packet is consumed by the API or false if the packet is not consumed.
 */
bool ppe_vp_tx_to_ppe_by_sc(int32_t vp_num, struct sk_buff *skb, ppe_drv_sc_t sc);
