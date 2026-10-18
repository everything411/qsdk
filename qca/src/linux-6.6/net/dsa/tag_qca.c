// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 */

#include <linux/etherdevice.h>
#include <linux/bitfield.h>
#include <net/dsa.h>
#include <linux/dsa/tag_qca.h>
#include <linux/dsa/8021q.h>
#include <linux/if_vlan.h>

#include "tag_8021q.h"
#include "tag.h"

#define QCA_NAME "qca"
#define QCA4B_NAME "qca_4b"
#define QCA8021Q_NAME "qca_8021q"

static struct sk_buff *_qca_tag_xmit(struct sk_buff *skb, struct net_device *dev,
	uint32_t hdr_len)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	struct dsa_switch *ds = dp->ds;
	struct qca_tagger_data *tagger_data;
	__be16 *phdr;

	tagger_data = ds->tagger_data;

	/* when holb enable, allow hw add hdr. 
	 * also need set skb mark for holb.
	 */
	if (tagger_data->tx_hdr_offload == true) {
		skb->mark = ((HOLB_MHT_VALID_TAG << HOLB_MHT_TAG_SHIFT) | dp->index);
		return skb;
	}

	skb_push(skb, hdr_len);

	dsa_alloc_etype_header(skb, hdr_len);
	phdr = dsa_etype_header_pos_tx(skb);

	/* if 4 bytes len, need add hdr type value */
	if (hdr_len == QCA_4B_HDR_LEN) {
		*phdr = QCA_HDR_TYPE_VALUE;
		phdr = phdr + 1;
	}

	/* Set the version field, and set destination port information */
	*phdr = FIELD_PREP(QCA_HDR_XMIT_VERSION, QCA_HDR_VERSION);
	*phdr |= QCA_HDR_XMIT_FROM_CPU;
	*phdr |= FIELD_PREP(QCA_HDR_XMIT_DP_BIT, BIT(dp->index));

	*phdr = htons(*phdr);

	return skb;
}

static struct sk_buff *_qca_tag_rcv(struct sk_buff *skb, struct net_device *dev,
	uint32_t hdr_len)
{
	struct qca_tagger_data *tagger_data;
	struct dsa_port *dp = dev->dsa_ptr;
	struct dsa_switch *ds = dp->ds;
	u8 ver, pk_type;
	__be16 *phdr;
	int port;
	u16 hdr;

	//BUILD_BUG_ON(sizeof(struct qca_mgmt_ethhdr) != QCA_HDR_MGMT_HEADER_LEN + QCA_HDR_LEN);

	tagger_data = ds->tagger_data;

	if (unlikely(!pskb_may_pull(skb, hdr_len)))
		return NULL;

	phdr = dsa_etype_header_pos_rx(skb);
	if (hdr_len == QCA_4B_HDR_LEN)
		phdr = phdr + 1;

	hdr = ntohs(*phdr);

	/* Make sure the version is correct */
	ver = FIELD_GET(QCA_HDR_RECV_VERSION, hdr);
	if (unlikely(ver != QCA_HDR_VERSION))
		return NULL;

	/* Get pk type */
	pk_type = FIELD_GET(QCA_HDR_RECV_TYPE, hdr);

	/* Ethernet mgmt read/write packet */
	if (pk_type == QCA_HDR_RECV_TYPE_RW_REG_ACK) {
		if (likely(tagger_data->rw_reg_ack_handler))
			tagger_data->rw_reg_ack_handler(ds, skb);
		return NULL;
	}

	/* Ethernet MIB counter packet */
	if (pk_type == QCA_HDR_RECV_TYPE_MIB) {
		if (likely(tagger_data->mib_autocast_handler))
			tagger_data->mib_autocast_handler(ds, skb);
		return NULL;
	}

	/* Get source port information */
	port = FIELD_GET(QCA_HDR_RECV_SOURCE_PORT, hdr);

	skb->dev = dsa_master_find_slave(dev, 0, port);
	if (!skb->dev)
		return NULL;

	/* Remove QCA tag and recalculate checksum */
	skb_pull_rcsum(skb, hdr_len);
	dsa_strip_etype_header(skb, hdr_len);

	return skb;
}

static struct sk_buff *qca_tag_xmit(struct sk_buff *skb, struct net_device *dev)
{
	return _qca_tag_xmit(skb, dev, QCA_HDR_LEN);
}

static struct sk_buff *qca_tag_rcv(struct sk_buff *skb, struct net_device *dev)
{
	return _qca_tag_rcv(skb, dev, QCA_HDR_LEN);
}

static struct sk_buff *qca_4b_tag_xmit(struct sk_buff *skb, struct net_device *dev)
{
	return _qca_tag_xmit(skb, dev, QCA_4B_HDR_LEN);
}

static struct sk_buff *qca_4b_tag_rcv(struct sk_buff *skb, struct net_device *dev)
{
	return _qca_tag_rcv(skb, dev, QCA_4B_HDR_LEN);
}

static bool qca_skb_is_bpdu(struct sk_buff *skb)
{
	struct ethhdr *eth = eth_hdr(skb);

	if (memcmp(eth->h_dest, eth_stp_addr, ETH_ALEN) == 0)
		return true;

	return false;
}

static struct sk_buff *qca_8021q_tag_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct dsa_port *dp = dsa_slave_to_port(dev);
	u16 tx_vid = dsa_tag_8021q_standalone_vid(dp);
	u16 queue_mapping = skb_get_queue_mapping(skb);
	u8 pcp = netdev_txq_to_tc(dev, queue_mapping);

	/* compatible with qca rstp resv fdb */
	if (qca_skb_is_bpdu(skb))
		return qca_4b_tag_xmit(skb, dev);

	return dsa_8021q_xmit(skb, dev, ETH_P_8021Q,
			      ((pcp << VLAN_PRIO_SHIFT) | tx_vid));
}

static struct sk_buff *qca_8021q_tag_rcv(struct sk_buff *skb, struct net_device *dev)
{
	int src_port, switch_id, qos_class;
	u16 vid, tci;

	if (skb_vlan_tag_present(skb)) {
		tci = skb_vlan_tag_get(skb);
		__vlan_hwaccel_clear_tag(skb);
	} else {
		skb_push_rcsum(skb, ETH_HLEN);
		__skb_vlan_pop(skb, &tci);
		skb_pull_rcsum(skb, ETH_HLEN);
	}

	vid = tci & VLAN_VID_MASK;
	src_port = dsa_8021q_rx_source_port(vid);
	switch_id = dsa_8021q_rx_switch_id(vid);
	qos_class = (tci & VLAN_PRIO_MASK) >> VLAN_PRIO_SHIFT;

	skb->dev = dsa_master_find_slave(dev, switch_id, src_port);
	if (!skb->dev) {
		netdev_warn(dev, "Couldn't decode source port\n");
		return NULL;
	}

	skb->priority = qos_class;

	return skb;
}

static int qca_tag_connect(struct dsa_switch *ds)
{
	struct qca_tagger_data *tagger_data;

	tagger_data = kzalloc(sizeof(*tagger_data), GFP_KERNEL);
	if (!tagger_data)
		return -ENOMEM;

	ds->tagger_data = tagger_data;

	return 0;
}

static void qca_tag_disconnect(struct dsa_switch *ds)
{
	kfree(ds->tagger_data);
	ds->tagger_data = NULL;
}

static const struct dsa_device_ops qca_netdev_ops = {
	.name	= QCA_NAME,
	.proto	= DSA_TAG_PROTO_QCA,
	.connect = qca_tag_connect,
	.disconnect = qca_tag_disconnect,
	.xmit	= qca_tag_xmit,
	.rcv	= qca_tag_rcv,
	.needed_headroom = QCA_HDR_LEN,
	.promisc_on_master = true,
};
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_QCA, QCA_NAME);
DSA_TAG_DRIVER(qca_netdev_ops);

static const struct dsa_device_ops qca_4b_netdev_ops = {
	.name	= QCA4B_NAME,
	.proto	= DSA_TAG_PROTO_4B_QCA,
	.connect = qca_tag_connect,
	.disconnect = qca_tag_disconnect,
	.xmit	= qca_4b_tag_xmit,
	.rcv	= qca_4b_tag_rcv,
	.needed_headroom = QCA_4B_HDR_LEN,
	.promisc_on_master = true,
};
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_4B_QCA, QCA4B_NAME);
DSA_TAG_DRIVER(qca_4b_netdev_ops);

static const struct dsa_device_ops qca_8021q_netdev_ops = {
	.name	= QCA8021Q_NAME,
	.proto	= DSA_TAG_PROTO_QCA_8021Q,
	.connect = qca_tag_connect,
	.disconnect = qca_tag_disconnect,
	.xmit	= qca_8021q_tag_xmit,
	.rcv	= qca_8021q_tag_rcv,
	.needed_headroom = VLAN_HLEN,
	.promisc_on_master = true,
};
MODULE_ALIAS_DSA_TAG_DRIVER(DSA_TAG_PROTO_QCA_8021Q, QCA8021Q_NAME);
DSA_TAG_DRIVER(qca_8021q_netdev_ops);

static struct dsa_tag_driver *qca_tag_drivers[] = {
	&DSA_TAG_DRIVER_NAME(qca_netdev_ops),
	&DSA_TAG_DRIVER_NAME(qca_4b_netdev_ops),
	&DSA_TAG_DRIVER_NAME(qca_8021q_netdev_ops),
};
module_dsa_tag_drivers(qca_tag_drivers);

MODULE_LICENSE("GPL");
