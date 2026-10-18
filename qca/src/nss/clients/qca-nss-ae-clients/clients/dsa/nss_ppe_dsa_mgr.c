/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppe_dsa_mgr.c
 *	NSS PPE DSA manager
 */
#include <linux/of.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/dsa/8021q.h>
#include <linux/etherdevice.h>
#include <net/dst_metadata.h>

#include <ppe_drv_public.h>
#include <nss_ppe_vlan_mgr.h>
#include <ppe_vp_public.h>
#include "nss_ppe_dsa_mgr.h"

#ifdef CONFIG_NET_DSA

static struct nss_ppe_dsa_mgr_context g_dsa_ctx;

#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
#ifdef NSS_ATH_DSA_VP_FDB_LEARN_EN
static bool dsa_fdb_learn_enabled = true;
#else
static bool dsa_fdb_learn_enabled = false;
#endif
module_param(dsa_fdb_learn_enabled, bool, 0644);
MODULE_PARM_DESC(dsa_fdb_learn_enabled, "DSA fdb learning is enabled");

/*
 * nss_ppe_dsa_mgr_alloc_metadata_dst()
 *	Alloc metadata dst for dsa port
 */
static struct metadata_dst *nss_ppe_dsa_mgr_alloc_metadata_dst(struct nss_ppe_dsa_pvt *dsa_pvt)
{
	struct metadata_dst	*dsa_meta = metadata_dst_alloc(0, METADATA_HW_PORT_MUX, GFP_ATOMIC);
	if (!dsa_meta) {
		nss_ppe_dsa_mgr_warn("Metadata alloc failed for port %d.\n", dsa_pvt->swpt_id);
		return NULL;
	}

	dsa_meta->u.port_info.port_id = dsa_pvt->swpt_id;

	return dsa_meta;
}

/*
 * nss_ppe_dsa_mgr_free_metadata_dst()
 *	Free metadata dst of dsa port
 */
static void nss_ppe_dsa_mgr_free_metadata_dst(struct nss_ppe_dsa_pvt *dsa_pvt)
{
	metadata_dst_free(dsa_pvt->dsa_meta);
	dsa_pvt->dsa_meta = NULL;
}

/*
 * nss_ppe_dsa_mgr_create_instance()
 *	Create dsa instance
 */
static struct nss_ppe_dsa_pvt *nss_ppe_dsa_mgr_create_instance(struct net_device *dev,
		struct dsa_port *dp)
{
	struct nss_ppe_dsa_pvt *dsa_pvt = kzalloc(sizeof(*dsa_pvt), GFP_KERNEL);
	if (!dsa_pvt) {
		nss_ppe_dsa_mgr_warn("%px: Allocation to private structure failed: %s\n",
						dev, dev->name);
		return NULL;
	}

	spin_lock(&g_dsa_ctx.lock);
	dsa_pvt->mtu = dev->mtu;
	ether_addr_copy(dsa_pvt->dev_addr, dev->dev_addr);
	dsa_pvt->dev = dev;
	dsa_pvt->swpt_id = dp->index;
	dsa_pvt->dsa_meta = nss_ppe_dsa_mgr_alloc_metadata_dst(dsa_pvt);
	kref_init(&dsa_pvt->ref);

	list_add(&dsa_pvt->item, &g_dsa_ctx.list);
	spin_unlock(&g_dsa_ctx.lock);

	nss_ppe_dsa_mgr_trace("DSA mgr pvt create for dev:%s, dsa port: %d, pvt pt: %d.\n",
		dev->name, dp->index, dsa_pvt->swpt_id);

	return dsa_pvt;
}

/*
 * nss_ppe_dsa_mgr_instance_find_and_ref()
 *	Increases the references of dsa_pvt instance.
 */
static struct nss_ppe_dsa_pvt *nss_ppe_dsa_mgr_instance_find_and_ref(
						struct net_device *dev)
{
	struct nss_ppe_dsa_pvt *dsa_pvt = NULL;

	spin_lock(&g_dsa_ctx.lock);
	list_for_each_entry(dsa_pvt, &g_dsa_ctx.list, item) {
		if (dsa_pvt->dev == dev) {
			kref_get(&dsa_pvt->ref);
			spin_unlock(&g_dsa_ctx.lock);
			return dsa_pvt;
		}
	}
	spin_unlock(&g_dsa_ctx.lock);

	return NULL;
}

/*
 * nss_ppe_dsa_mgr_instance_free()
 *	Destroy dsa instance
 */
static void nss_ppe_dsa_mgr_instance_free(struct kref *kref)
{
	struct nss_ppe_dsa_pvt *dsa_pvt = container_of(kref, struct nss_ppe_dsa_pvt, ref);

	nss_ppe_dsa_mgr_trace("DSA mgr pvt free for dsa port: %d.\n", dsa_pvt->swpt_id);

	spin_lock(&g_dsa_ctx.lock);
	if (!list_empty(&dsa_pvt->item)) {
		list_del(&dsa_pvt->item);
	}
	nss_ppe_dsa_mgr_free_metadata_dst(dsa_pvt);
	spin_unlock(&g_dsa_ctx.lock);

	kfree(dsa_pvt);
}

/*
 * nss_ppe_dsa_mgr_instance_deref()
 *	Decreases the references of dsa_pvt instance.
 */
static bool nss_ppe_dsa_mgr_instance_deref(struct nss_ppe_dsa_pvt *dsa_pvt)
{
	if (kref_put(&dsa_pvt->ref, nss_ppe_dsa_mgr_instance_free)) {
		nss_ppe_dsa_mgr_trace("%p: ref is 0.", dsa_pvt);
		return true;
	}

	nss_ppe_dsa_mgr_trace("%p: ref is %d for port: %u", dsa_pvt, kref_read(&dsa_pvt->ref), dsa_pvt->swpt_id);
	return false;
}

/*
 * nss_ppe_dsa_mgr_change_addr()
 *	Change resepctive l3if MAC per netdev MAC change
 */
static int nss_ppe_dsa_mgr_change_addr(struct net_device *slave_dev)
{
	ppe_drv_ret_t ret;

	struct nss_ppe_dsa_pvt *dsa_pvt = nss_ppe_dsa_mgr_instance_find_and_ref(slave_dev);
	if (!dsa_pvt) {
		nss_ppe_dsa_mgr_warn("%px: Interface not found name: %s\n",
						slave_dev, slave_dev->name);
		return NOTIFY_DONE;
	}

	spin_lock(&g_dsa_ctx.lock);
	if (!memcmp(dsa_pvt->dev_addr, slave_dev->dev_addr, ETH_ALEN)) {
		spin_unlock(&g_dsa_ctx.lock);
		nss_ppe_dsa_mgr_instance_deref(dsa_pvt);
		return NOTIFY_DONE;
	}

	ret = ppe_drv_iface_mac_addr_clear(dsa_pvt->iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_unlock(&g_dsa_ctx.lock);
		nss_ppe_dsa_mgr_warn("%s: Failed to clear MAC address, error = %d\n", slave_dev->name, ret);
		nss_ppe_dsa_mgr_instance_deref(dsa_pvt);
		return NOTIFY_BAD;
	}

	ret = ppe_drv_iface_mac_addr_set(dsa_pvt->iface, (uint8_t *)slave_dev->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		spin_unlock(&g_dsa_ctx.lock);
		nss_ppe_dsa_mgr_warn("%s: Failed to change MAC address, error = %d\n", slave_dev->name, ret);
		nss_ppe_dsa_mgr_instance_deref(dsa_pvt);
		return NOTIFY_BAD;
	}

	ether_addr_copy(dsa_pvt->dev_addr, slave_dev->dev_addr);
	spin_unlock(&g_dsa_ctx.lock);

	nss_ppe_dsa_mgr_trace("%s: MAC changed to %pM, updated PPE\n", slave_dev->name, slave_dev->dev_addr);
	nss_ppe_dsa_mgr_instance_deref(dsa_pvt);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_change_mtu()
 *	Change resepctive port MTU per netdev MTU change
 */
static int nss_ppe_dsa_mgr_change_mtu(struct net_device *slave_dev)
{
	ppe_drv_ret_t ret;
	uint32_t old_mtu;

	struct nss_ppe_dsa_pvt *dsa_pvt = nss_ppe_dsa_mgr_instance_find_and_ref(slave_dev);
	if (!dsa_pvt) {
		nss_ppe_dsa_mgr_warn("%px: Interface not found name: %s\n",
						slave_dev, slave_dev->name);
		return NOTIFY_DONE;
	}

	old_mtu = dsa_pvt->mtu;

	spin_lock(&g_dsa_ctx.lock);
	if (dsa_pvt->mtu == slave_dev->mtu) {
		spin_unlock(&g_dsa_ctx.lock);
		nss_ppe_dsa_mgr_instance_deref(dsa_pvt);
		return NOTIFY_DONE;
	}

	dsa_pvt->mtu = slave_dev->mtu;

	ret = ppe_drv_iface_mtu_set(dsa_pvt->iface, slave_dev->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		dsa_pvt->mtu = old_mtu;
		spin_unlock(&g_dsa_ctx.lock);

		nss_ppe_dsa_mgr_warn("%s: Failed to change MTU(%d) in PPE, error = %d\n",
			slave_dev->name, slave_dev->mtu, ret);
		nss_ppe_dsa_mgr_instance_deref(dsa_pvt);
		return NOTIFY_BAD;
	}
	spin_unlock(&g_dsa_ctx.lock);

	nss_ppe_dsa_mgr_trace("%s: MTU changed to %d, PPE updated\n", slave_dev->name, slave_dev->mtu);
	nss_ppe_dsa_mgr_instance_deref(dsa_pvt);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_configure_ppe_vp()
 *  Config ppe ath mapping table based on vp
 */
static int nss_ppe_dsa_mgr_configure_ppe_vp(struct nss_ppe_dsa_pvt *dsa_pvt)
{
	return ppe_drv_athtag_add_vp_mapping(dsa_pvt->iface, dsa_pvt->swpt_id);
}

/*
 * nss_ppe_dsa_mgr_destroy_ppe_vp()
 *  Destroy ppe ath mapping table based on vp
 */
static int nss_ppe_dsa_mgr_destroy_ppe_vp(struct nss_ppe_dsa_pvt *dsa_pvt)
{
	return ppe_drv_athtag_del_vp_mapping(dsa_pvt->iface, dsa_pvt->swpt_id);
}

/*
 * nss_ppe_dsa_mgr_vp_rx_cb()
 * 	Rx handler for packets with ATH HDR based DSA VP
 */
static bool nss_ppe_dsa_mgr_vp_rx_cb(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;
	struct net_device *master = NULL;
	struct nss_ppe_dsa_pvt *dsa_pvt = NULL;

	if (!dsa_slave_dev_check(skb->dev)) {
		nss_ppe_dsa_mgr_warn("%px: It's not DSA interface: %s\n",
						skb->dev, skb->dev->name);
		return false;
	}

	dsa_pvt = nss_ppe_dsa_mgr_instance_find_and_ref(skb->dev);
	if (!dsa_pvt) {
		nss_ppe_dsa_mgr_warn("%px: Interface not found name: %s\n",
						skb->dev, skb->dev->name);
		return false;
	}

	/* metadata dst update */
	skb_dst_set_noref(skb, &dsa_pvt->dsa_meta->dst);

	nss_ppe_dsa_mgr_instance_deref(dsa_pvt);

	master = dsa_port_to_master(dsa_port_from_netdev(skb->dev));
	if (!master) {
		nss_ppe_dsa_mgr_warn("%s: failed to obtain master", skb->dev->name);
		return false;
	}

	/* master netdev update */
	skb->dev = master;
	skb->skb_iif = master->ifindex;
	skb->protocol = eth_type_trans(skb, skb->dev);

	if (likely(master->features & NETIF_F_RXCSUM)) {
		skb->ip_summed = info->ip_summed;
	}

	if (unlikely(master->features & NETIF_F_GRO)) {
		napi_gro_receive(info->napi, skb);
	} else {
		netif_receive_skb(skb);
	}

	return true;
}

/*
 * nss_ppe_dsa_mgr_alloc_ppe_vp()
 *  Alloc available vp from ppe-vp module
 */
static int nss_ppe_dsa_mgr_alloc_ppe_vp(struct nss_ppe_dsa_pvt *dsa_pvt,
		struct net_device *dev, struct net_device *master_dev)
{
	struct ppe_drv_iface *base_if = NULL;
	uint32_t queue_id = 0, pp_id = 0;
	struct ppe_vp_ai vpai = {0};
#ifdef NSS_ATH_DSA_VP_QUEUE_SUPPORT
	int16_t htt_queue;
#endif

	base_if = ppe_drv_iface_get_by_dev(master_dev);
	if (!base_if) {
		nss_ppe_dsa_mgr_warn("%px: %s: couldn't get PPE iface\n", master_dev, master_dev->name);
		return -1;
	}

	pp_id = ppe_drv_iface_port_idx_get(base_if);
	if (pp_id == -1) {
		nss_ppe_dsa_mgr_warn("%px: %s:%d is not valid port\n", master_dev, master_dev->name, pp_id);
		return -1;
	}

	queue_id = ppe_drv_port_ucast_queue_get_by_port(pp_id);
	if (queue_id < 0) {
		nss_ppe_dsa_mgr_warn("Invalid queue id for master dev: %s\n", master_dev->name);
		return -1;
	}

#ifdef NSS_ATH_DSA_VP_QUEUE_SUPPORT
	/*
	 * For DSA slaves under a Huntington switch, assign a per-port queue
	 * using the DSA switch port index.
	 */
	if (ppe_drv_iface_is_htt(base_if)) {
		htt_queue = ppe_drv_port_htt_queue_base_get(pp_id, dsa_pvt->swpt_id);
		if (htt_queue < 0) {
			nss_ppe_dsa_mgr_warn("Invalid HTT queue for dev %s port %d swpt_id %d\n",
					dev->name, pp_id, dsa_pvt->swpt_id);
			return -1;
		}
		queue_id = htt_queue;
	}
#endif

	vpai.type = PPE_VP_TYPE_SW_L2;
	vpai.queue_num = queue_id;
	vpai.xmit_port = pp_id;
	vpai.fdb_learn_enabled = dsa_fdb_learn_enabled;

	vpai.dst_cb = NULL;
	/* when src_cb is NULL, the pkt will go to lanX directly */
	vpai.src_cb = nss_ppe_dsa_mgr_vp_rx_cb;

	/* allocate VP for dsa interface */
	if (ppe_vp_alloc(dev, &vpai) < 0) {
		nss_ppe_dsa_mgr_warn("vp alloc failed for dev %s status: %d", dev->name, vpai.status);
		return -1;
	}

	/* iface, port, l3if created by ppe_vp_alloc */
	dsa_pvt->iface = ppe_drv_iface_get_by_dev(dev);
	if (!dsa_pvt->iface) {
		nss_ppe_dsa_mgr_warn("dsa iface get failed: %s", dev->name);
		return -1;
	}

	nss_ppe_dsa_mgr_trace("%s: pp_id: %d, queue_id: %d, vp_id:%d.\n", dev->name, pp_id, queue_id,
		ppe_drv_iface_port_idx_get(dsa_pvt->iface));

	return 0;
}

/*
 * nss_ppe_dsa_mgr_free_ppe_vp()
 *  Free the allocated vp
 */
static int nss_ppe_dsa_mgr_free_ppe_vp(struct nss_ppe_dsa_pvt *dsa_pvt)
{
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;
	uint32_t ppe_vp = ppe_drv_iface_port_idx_get(dsa_pvt->iface);

	status = ppe_vp_free(ppe_vp);
	if (status != PPE_VP_STATUS_SUCCESS) {
		nss_ppe_dsa_mgr_warn("failed to free VP %u.\n", ppe_vp);
	}

	return 0;
}

/*
 * nss_ppe_dsa_mgr_dsa_vp_create()
 *	Create dsa private instance and config PPE per DSA interface
 */
static int nss_ppe_dsa_mgr_dsa_vp_create(struct net_device *dev,
		struct net_device *master_dev, struct dsa_port *dp)
{
	struct nss_ppe_dsa_pvt *dsa_pvt = nss_ppe_dsa_mgr_create_instance(dev, dp);
	if (!dsa_pvt) {
		nss_ppe_dsa_mgr_warn("DSA instance creation failed for dev:%s\n", dev->name);
		return -1;
	}

	if (nss_ppe_dsa_mgr_alloc_ppe_vp(dsa_pvt, dev, master_dev)) {
		nss_ppe_dsa_mgr_warn("DSA VP alloc failed for dev:%s\n", dev->name);
		return -1;
	}

	if (nss_ppe_dsa_mgr_configure_ppe_vp(dsa_pvt)) {
		nss_ppe_dsa_mgr_warn("DSA VP config failed for dev:%s\n", dev->name);
		return -1;
	}

	nss_ppe_dsa_mgr_trace("DSA mgr vp creation for dev:%s, dsa port: %d, pvt pt: %d, vp: %d.\n",
		dev->name, dp->index, dsa_pvt->swpt_id, ppe_drv_iface_port_idx_get(dsa_pvt->iface));

	return 0;
}

/*
 * nss_ppe_dsa_mgr_dsa_vp_destroy()
 *	Destroy dsa private instance and remove PPE config per DSA interface
 */
static int nss_ppe_dsa_mgr_dsa_vp_destroy(struct net_device *dev)
{
	struct nss_ppe_dsa_pvt *dsa_pvt = nss_ppe_dsa_mgr_instance_find_and_ref(dev);
	if (!dsa_pvt) {
		nss_ppe_dsa_mgr_warn("DSA instance is NULL for dev:%s\n", dev->name);
		return -1;
	}

	/* deconfig ath base on vp */
	if (nss_ppe_dsa_mgr_destroy_ppe_vp(dsa_pvt)) {
		nss_ppe_dsa_mgr_warn("remove DSA VP ath config failed for dev:%s\n", dev->name);
		return -1;
	}

	/* free vp */
	if (nss_ppe_dsa_mgr_free_ppe_vp(dsa_pvt)) {
		nss_ppe_dsa_mgr_warn("remove DSA VP config failed for dev:%s\n", dev->name);
		return -1;
	}

	nss_ppe_dsa_mgr_trace("DSA mgr vp destroy for dev:%s, pvt pt: %d, vp: %d.\n",
		dev->name, dsa_pvt->swpt_id, ppe_drv_iface_port_idx_get(dsa_pvt->iface));

	/* release for ref increased by nss_ppe_dsa_mgr_instance_find_and_ref */
	nss_ppe_dsa_mgr_instance_deref(dsa_pvt);

	/* release for ref increased by nss_ppe_dsa_mgr_dsa_vp_create */
	nss_ppe_dsa_mgr_instance_deref(dsa_pvt);

	return 0;
}
#endif

/*
 * nss_ppe_dsa_mgr_proto_state_find()
 *	Find protocol state for a device
 */
static struct nss_ppe_dsa_proto_state *nss_ppe_dsa_mgr_proto_state_find(struct net_device *dev)
{
	struct nss_ppe_dsa_proto_state *state = NULL;

	spin_lock(&g_dsa_ctx.proto_lock);
	list_for_each_entry(state, &g_dsa_ctx.proto_list, item) {
		if (state->dev == dev) {
			spin_unlock(&g_dsa_ctx.proto_lock);
			return state;
		}
	}
	spin_unlock(&g_dsa_ctx.proto_lock);

	return NULL;
}

/*
 * nss_ppe_dsa_mgr_proto_state_get()
 *	Get protocol for a device, return NONE if not found
 */
static enum dsa_tag_protocol nss_ppe_dsa_mgr_proto_state_get(struct net_device *dev)
{
	struct nss_ppe_dsa_proto_state *state = nss_ppe_dsa_mgr_proto_state_find(dev);
	return state ? state->proto : DSA_TAG_PROTO_NONE;
}

/*
 * nss_ppe_dsa_mgr_proto_state_set()
 *	Set protocol for a device, create state if not exists
 */
static int nss_ppe_dsa_mgr_proto_state_set(struct net_device *dev, enum dsa_tag_protocol proto)
{
	struct nss_ppe_dsa_proto_state *state = nss_ppe_dsa_mgr_proto_state_find(dev);

	if (state) {
		/* Update existing state */
		spin_lock(&g_dsa_ctx.proto_lock);
		state->proto = proto;
		spin_unlock(&g_dsa_ctx.proto_lock);
		return 0;
	}

	/* Create new state */
	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state) {
		nss_ppe_dsa_mgr_warn("%s: Failed to allocate protocol state\n", dev->name);
		return -ENOMEM;
	}

	state->dev = dev;
	state->proto = proto;

	spin_lock(&g_dsa_ctx.proto_lock);
	list_add(&state->item, &g_dsa_ctx.proto_list);
	spin_unlock(&g_dsa_ctx.proto_lock);

	nss_ppe_dsa_mgr_trace("%s: Created protocol state, proto=%d\n", dev->name, proto);
	return 0;
}

/*
 * nss_ppe_dsa_mgr_proto_state_remove()
 *	Remove protocol state for a device
 */
static void nss_ppe_dsa_mgr_proto_state_remove(struct net_device *dev)
{
	struct nss_ppe_dsa_proto_state *state = nss_ppe_dsa_mgr_proto_state_find(dev);

	if (state) {
		spin_lock(&g_dsa_ctx.proto_lock);
		list_del(&state->item);
		spin_unlock(&g_dsa_ctx.proto_lock);
		kfree(state);
		nss_ppe_dsa_mgr_trace("%s: Removed protocol state\n", dev->name);
	}
}

/*
 * nss_ppe_dsa_mgr_changeaddr_event()
 *	Change dsa netdev MAC address.
 */
static int nss_ppe_dsa_mgr_changeaddr_event(struct netdev_notifier_info *info, struct dsa_port *dp)
{
	struct net_device *slave __maybe_unused = dp->slave;

	nss_ppe_dsa_mgr_trace("slave:%s, proto: %d, MAC Addr change requested.\n", slave->name,
		dp->cpu_dp->tag_ops->proto);

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		return nss_ppe_vlan_mgr_changeaddr_event(info);
	}
#endif

#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_4B_QCA == dp->cpu_dp->tag_ops->proto) {
		return nss_ppe_dsa_mgr_change_addr(slave);
	}
#endif
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_changemtu_event()
 *  Change dsa netdev MTU.
 */
static int nss_ppe_dsa_mgr_changemtu_event(struct netdev_notifier_info *info, struct dsa_port *dp)
{
	struct net_device *slave __maybe_unused = dp->slave;

	nss_ppe_dsa_mgr_trace("slave:%s, idx:%u, proto:%d. \n", slave->name,
		dp->index, dp->cpu_dp->tag_ops->proto);

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		return nss_ppe_vlan_mgr_changemtu_event(info);
	}
#endif

#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_4B_QCA == dp->cpu_dp->tag_ops->proto) {
		return nss_ppe_dsa_mgr_change_mtu(slave);
	}
#endif
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_register_event()
 *	dsa_mgr handles dsa netdev registration notification.
 */
static int nss_ppe_dsa_mgr_register_event(struct dsa_port *dp)
{
	struct net_device *master = dsa_port_to_master(dp);
	struct net_device *slave = dp->slave;

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_vlan_mgr_dsa_vp_create(slave, master);
	}
#endif

#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_4B_QCA == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_dsa_mgr_dsa_vp_create(slave, master, dp);
	}
#endif

	/* Init protocol node for this device */
	nss_ppe_dsa_mgr_proto_state_set(slave, dp->cpu_dp->tag_ops->proto);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_unregister_event()
 *	dsa_mgr handles dsa netdev unregistration notification.
 */
static int nss_ppe_dsa_mgr_unregister_event(struct dsa_port *dp)
{
	struct net_device *slave = dp->slave;

#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_QCA_8021Q == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_vlan_mgr_dsa_vp_destroy(slave);
	}
#endif

#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	if (DSA_TAG_PROTO_4B_QCA == dp->cpu_dp->tag_ops->proto) {
		nss_ppe_dsa_mgr_dsa_vp_destroy(slave);
	}
#endif

	/* Uninit protocol node for this device */
	nss_ppe_dsa_mgr_proto_state_remove(slave);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_netdevice_event()
 *	dsa_mgr handles dsa netdev operation notifications.
 */
static int nss_ppe_dsa_mgr_netdevice_event(struct notifier_block *unused,
				unsigned long event, void *ptr)
{
	struct netdev_notifier_info *info = (struct netdev_notifier_info *)ptr;
	struct net_device *dev = netdev_notifier_info_to_dev(info);

	struct dsa_port *dp = dsa_port_from_netdev(dev);
	if (IS_ERR(dp))
		return NOTIFY_DONE;

	switch (event) {
	case NETDEV_CHANGEADDR:
		return nss_ppe_dsa_mgr_changeaddr_event(info, dp);
	case NETDEV_CHANGEMTU:
		return nss_ppe_dsa_mgr_changemtu_event(info, dp);
	case NETDEV_REGISTER:
		return nss_ppe_dsa_mgr_register_event(dp);
	case NETDEV_UNREGISTER:
		return nss_ppe_dsa_mgr_unregister_event(dp);
	}

	/*
	 * Notify done for all the events we don't care
	 */
	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_netdevice_nb
 *	dsa_mgr netdevice event notifier.
 */
static struct notifier_block nss_ppe_dsa_mgr_netdevice_nb __read_mostly = {
	.notifier_call = nss_ppe_dsa_mgr_netdevice_event,
};

/*
 * nss_ppe_dsa_mgr_tag_proto_change()
 *	Change PPE config per proto
 */
static int nss_ppe_dsa_mgr_tag_proto_change(struct net_device *dev, enum dsa_tag_protocol proto)
{
	struct dsa_port *dp = dsa_port_from_netdev(dev);
	struct net_device *master = dsa_port_to_master(dp);
	enum dsa_tag_protocol old_proto;

	if (!dsa_slave_dev_check(dev))
		return NOTIFY_DONE;

	/* Get current protocol from per-device state */
	old_proto = nss_ppe_dsa_mgr_proto_state_get(dev);

	/* If already using this protocol, nothing to do */
	if (old_proto == proto) {
		nss_ppe_dsa_mgr_trace("%s: Already using tag protocol %d\n", dev->name, proto);
		return NOTIFY_DONE;
	}

	nss_ppe_dsa_mgr_info("%s: Changing tag protocol from %d to %d\n", dev->name, old_proto, proto);

	/* Teardown old protocol configuration */
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	if (old_proto == DSA_TAG_PROTO_QCA_8021Q)
		nss_ppe_vlan_mgr_dsa_vp_destroy(dev);
#endif
#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	if (old_proto == DSA_TAG_PROTO_4B_QCA)
		nss_ppe_dsa_mgr_dsa_vp_destroy(dev);
#endif

	/* Setup new protocol configuration */
	/* For DSA_TAG_PROTO_NONE, all chip supported, no configuration for PPE besides teardown old proto */
	if (proto == DSA_TAG_PROTO_NONE)
		nss_ppe_dsa_mgr_info("%s: Switching to DSA_TAG_PROTO_NONE, no new configuration needed\n", dev->name);
#ifdef NSS_VLAN_BASED_DSA_SUPPORT
	else if (proto == DSA_TAG_PROTO_QCA_8021Q)
		nss_ppe_vlan_mgr_dsa_vp_create(dev, master);
#endif
#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	else if (proto == DSA_TAG_PROTO_4B_QCA)
		nss_ppe_dsa_mgr_dsa_vp_create(dev, master, dp);
#endif

	/* Update protocol state for this device */
	nss_ppe_dsa_mgr_proto_state_set(dev, proto);

	nss_ppe_dsa_mgr_info("%s: Tag protocol changed to %d successfully\n", dev->name, proto);

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_dsa_event_nb()
 *	Receive proto change event from DSA
 */
static int nss_ppe_dsa_mgr_dsa_event_nb(struct notifier_block *unused,
		unsigned long event, void *ptr)
{
	switch (event) {
		case DSA_NOTIFIER_TAG_CHG:
			struct dsa_notifier_tag_proto_chg *info = (struct dsa_notifier_tag_proto_chg *)ptr;

			nss_ppe_dsa_mgr_trace("%s, proto: %d.\n", info->info.dev->name, info->proto);
			return nss_ppe_dsa_mgr_tag_proto_change(info->info.dev, info->proto);

		default:
			nss_ppe_dsa_mgr_warn("DSA event %lu is not supported\n", event);
	}

	return NOTIFY_DONE;
}

/*
 * nss_ppe_dsa_mgr_dsa_notifier_nb
 *	dsa_mgr dsa proto change event notifier.
 */
static struct notifier_block nss_ppe_dsa_mgr_dsa_notifier_nb = {
	.notifier_call = nss_ppe_dsa_mgr_dsa_event_nb,
};

/*
 * nss_ppe_dsa_mgr_exit_module()
 *	dsa_mgr module exit function
 */
static void __exit nss_ppe_dsa_mgr_exit_module(void)
{
	unregister_dsa_blocking_notifier(&nss_ppe_dsa_mgr_dsa_notifier_nb);
	unregister_netdevice_notifier(&nss_ppe_dsa_mgr_netdevice_nb);

	nss_ppe_dsa_mgr_info("PPE DSA MGR Module unloaded\n");
}

/*
 * nss_ppe_dsa_mgr_init_module()
 *	dsa_mgr module init function
 */
static int __init nss_ppe_dsa_mgr_init_module(void)
{
#ifdef NSS_ATH_HDR_BASED_DSA_SUPPORT
	INIT_LIST_HEAD(&g_dsa_ctx.list);
	spin_lock_init(&g_dsa_ctx.lock);
#endif
	INIT_LIST_HEAD(&g_dsa_ctx.proto_list);
	spin_lock_init(&g_dsa_ctx.proto_lock);

	register_dsa_blocking_notifier(&nss_ppe_dsa_mgr_dsa_notifier_nb);
	register_netdevice_notifier(&nss_ppe_dsa_mgr_netdevice_nb);

	nss_ppe_dsa_mgr_info("PPE DSA MGR Module (Build %s) loaded\n", NSS_PPE_BUILD_ID);

	return 0;
}

#else /* !CONFIG_NET_DSA */

static int __init nss_ppe_dsa_mgr_init_module(void)
{
	return 0;
}

static void __exit nss_ppe_dsa_mgr_exit_module(void)
{
}

#endif /* CONFIG_NET_DSA */

module_init(nss_ppe_dsa_mgr_init_module);
module_exit(nss_ppe_dsa_mgr_exit_module);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE DSA manager");
