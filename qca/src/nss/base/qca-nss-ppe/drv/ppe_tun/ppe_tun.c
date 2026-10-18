/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/netdevice.h>
#include <linux/version.h>
#include <ppe_drv_port.h>
#include <ppe_drv_tun_cmn_ctx.h>
#include <ppe_drv_tun_public.h>
#include <asm/cmpxchg.h>
#include <ppe_vp_tx.h>
#include "ppe_tun.h"

struct ppe_tun_priv *ptp;
static bool ppe_tun_tx_to_ppe(struct net_device *dev, struct sk_buff *skb);
static ppe_vp_type_t ppe_tun_get_tun_vp_type(enum ppe_drv_tun_cmn_ctx_type type);

/*
 * Offload ops for tunnels which supports encapsulation in hardware
 */
static struct netdev_hw_offload_ops ppe_tun_netdev_ops = {
	.xmit = ppe_tun_tx_to_ppe,
	.recv = NULL
};

/*
 * ppe_tun_netdev_unregister_ol_ops()
 *	register tunnel offload ops
 */
static inline void ppe_tun_netdev_unregister_ol_ops(struct ppe_tun *tun)
{
	if (tun->dev) {
		netdev_hw_offload_ops_unregister(tun->dev, &ppe_tun_netdev_ops);
	}
}

/*
 * ppe_tun_netdev_register_ol_ops()
 *	register tunnel offload ops
 */
static inline int ppe_tun_netdev_register_ol_ops(struct ppe_tun *tun)
{
	return netdev_hw_offload_ops_register(tun->dev, &ppe_tun_netdev_ops, "ppe_tun");
}

/*
 * ppe_tun_allow_accel()
 *	Check if particular tunnel type acceleration supported.
 */
static bool ppe_tun_allow_accel(enum ppe_drv_tun_cmn_ctx_type type)
{
	switch (type) {
	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP:
		if (ptp->tun_accel.ppe_tun_gretap_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE gretap acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN:
		if (ptp->tun_accel.ppe_tun_vxlan_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE vxlan acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6:
		if (ptp->tun_accel.ppe_tun_ipip6_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE ipip6 acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_MAPT:
		if (ptp->tun_accel.ppe_tun_mapt_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE mapt acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2:
		if (ptp->tun_accel.ppe_tun_l2tp_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE l2tp acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_CUST:
		if (ptp->tun_accel.ppe_tun_cust_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE custom tunnel acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE:
		if (ptp->tun_accel.ppe_tun_vxlan_gpe_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE vxlan gpe acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN:
		if (ptp->tun_accel.ppe_tun_gretun_accel) {
			return true;
		}

		ppe_tun_warn("%p: PPE gretun acceleration is not enabled", ptp);
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2:
	case PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3:
		return true;

	default:
		break;
	}

	ppe_tun_warn("%p: Accel type %u is invalid", ptp, type);
	return false;
}

/*
 * ppe_tun_hybrid_offload_status_enabled()
 *	Check if tunnel type has hybrid offload support enabled.
 */
static bool ppe_tun_hybrid_offload_status_enabled(enum ppe_drv_tun_cmn_ctx_type type)
{
	switch (type) {
	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP:
		if (ptp->tun_hb_info.en_gretap_hybrid_ol) {
			return true;
		}

		ppe_tun_warn("%p: PPE gretap hybrid offload is not enabled", ptp);
		break;

	default:
		break;
	}

	ppe_tun_warn("%p: Hybrid offload operation for tunnel type %u is invalid", ptp, type);
	return false;
}

/*
 * ppe_tun_set_tun_data()
 *	Set tunnel information
 */
bool ppe_tun_set_tun_data(struct ppe_tun *tun, ppe_tun_data *tun_data)
{
	ppe_tun_data *tunnel_data = tun->tun_data;

	if (!tunnel_data) {
		ppe_tun_trace("%p: tunnel info not allocated for tunnel", tun);
		return false;
	}

	if (tun->type == PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2) {
		tunnel_data->l2tp_info.session_id = tun_data->l2tp_info.session_id;
		tunnel_data->l2tp_info.tunnel_id = tun_data->l2tp_info.tunnel_id;
		return true;
	}

	if (tun->type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2 ||
			tun->type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3) {
		tunnel_data->cust_info.custom_tunnel_data = tun_data->cust_info.custom_tunnel_data;
		tunnel_data->cust_info.exp_cb = tun_data->cust_info.exp_cb;
		tunnel_data->cust_info.stats_cb = tun_data->cust_info.stats_cb;
		return true;
	}

	ppe_tun_warn("%p: invalid tunnel type %d to set tunnel info", ptp, tun->type);
	return false;
}

/*
 * ppe_tun_set_tun_data()
 *	allocate tunnel data memory
 */
ppe_tun_data *ppe_tun_alloc_tun_data(void)
{
	ppe_tun_data *tun_data;

	tun_data = kzalloc(sizeof(ppe_tun_data), GFP_ATOMIC);
	if (!tun_data) {
		ppe_tun_trace("%p: ppe tun data allocation failed\n", ptp);
	}

	return tun_data;
}

/*
 * ppe_tun_ctx_free()
 *	Free ppe tunnel allocated instance
 */
static void ppe_tun_ctx_free(struct kref *kref)
{
	struct ppe_tun *tun = container_of(kref, struct ppe_tun, ref);
	ppe_vp_num_t vp_num;

	spin_lock_bh(&ptp->lock);
	ptp->tun[tun->idx] = NULL;
	spin_unlock_bh(&ptp->lock);

	if (tun->tun_data) {
		kfree(tun->tun_data);
	}

	vp_num = tun->vp_num;
	ppe_vp_free(vp_num);

	nss_ppe_tun_minidump_free(tun, "ppe_tun");
	kfree(tun);
	atomic_inc(&ptp->total_free);
	atomic_dec(&ptp->free_pending);
}

/*
 * ppe_tun_deref()
 *	Taken reference of tunnel instance
 */
static bool ppe_tun_deref(struct ppe_tun *tun)
{
	if (kref_put(&tun->ref, ppe_tun_ctx_free)) {
		ppe_tun_trace("%p: reference count is 0 for tun at index: %u", tun, tun->idx);
		return true;
	}

	ppe_tun_trace("%p: tun_idx: %u ref dec:%u", tun, tun->idx, kref_read(&tun->ref));
	return false;
}

/*
 * ppe_tun_ref()
 *	Taken reference of tunnel instance
 */
static struct ppe_tun *ppe_tun_ref(struct ppe_tun *tun)
{
	kref_get(&tun->ref);

	ppe_tun_trace("%p: tun %u ref inc:%u", tun, tun->idx, kref_read(&tun->ref));
	return tun;
}

/*
 * ppe_tun_get_tun_by_netdev_and_ref()
 *	Get PPE tunnel by netdev and get ref
 */
static struct ppe_tun *ppe_tun_get_tun_by_netdev_and_ref(struct net_device *dev)
{
	struct ppe_tun *tun;
	int i = 0;

	spin_lock_bh(&ptp->lock);
	for (i = 0; i < PPE_TUN_MAX; i++) {
		tun = ptp->tun[i];
		if (tun && (tun->dev == dev)) {
			ppe_tun_ref(tun);
			spin_unlock_bh(&ptp->lock);
			return tun;
		}
	}
	spin_unlock_bh(&ptp->lock);

	return NULL;
}

/*
 * ppe_tun_stats()
 *	Update the netdevice stats
 */
bool ppe_tun_stats(struct net_device *dev, ppe_vp_hw_stats_t *stats)
{
	struct ppe_tun *tun;
	bool ret;

	tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun) {
		ppe_tun_trace("%p: ppe tun stats update failed: tunnel not found", tun);
		return false;
	}

	if (!(tun->state & PPE_TUN_STATE_CONFIGURED)) {
		ppe_tun_trace("%p: tunnel is not configured yet", tun);
		ret = false;
		goto done;
	}

	if (!tun->stats_excp) {
		ppe_tun_trace("%p: tunnel stats callback not present", tun);
		ret = false;
		goto done;
	}

	ret = tun->stats_excp(dev, stats, tun->tun_data);

done:
	ppe_tun_deref(tun);
	return ret;
}

/*
 * ppe_tun_tx_to_ppe()
 *	Queue packets to destination tunnel VP
 */
static bool ppe_tun_tx_to_ppe(struct net_device *dev, struct sk_buff *skb)
{
	struct ppe_vp_tx_info tx_info = {0};
	struct ppe_tun *tun;

	tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun || !(tun->state & PPE_TUN_STATE_ACTIVATED)) {
		ppe_tun_warn("%p: failed to queue packet to VP tunnel not found", tun);
		goto err;
	}

	if ((skb->len + tun->tun_header_len) > tun->tun_xmit_port_mtu) {
		ppe_tun_warn("%p: post encap packet length %d exceeds xmit port mtu %d", tun,
				(skb->data_len + tun->tun_header_len), tun->tun_xmit_port_mtu);
		goto err;
	}

	if (!ppe_vp_tx_to_vp(tun->vp_num, &tx_info, skb)) {
		/*
		 * skb is consumed in ppe_vp_tx_to_vp if tx fails.
		 * Hence no need to free the same here. Just accounting
		 * for failure count here.
		 */
	       atomic64_inc(&tun->tun_hybrid_offload_tx_fail_cnt);
       } else {
	       atomic64_inc(&tun->tun_hybrid_offload_tx_pkt_cnt);
       }

       ppe_tun_deref(tun);
       return true;

err:
       if (tun) {
               atomic64_inc(&tun->tun_hybrid_offload_tx_fail_cnt);
               ppe_tun_deref(tun);
       }

	return false;
}


/*
 * ppe_tun_get_tun_by_vp_num_and_ref()
 *	Get tunnel by vp_num
 */
static struct ppe_tun *ppe_tun_get_tun_by_vp_num_and_ref(ppe_vp_num_t vp_num)
{
	struct ppe_tun *tun;
	int i = 0;

	spin_lock_bh(&ptp->lock);
	for (i = 0; i < PPE_TUN_MAX; i++) {
		tun = ptp->tun[i];
		if (tun != NULL) {
			if (tun->vp_num == vp_num) {
				ppe_tun_ref(tun);
				spin_unlock_bh(&ptp->lock);
				return tun;
			}
		}
	}
	spin_unlock_bh(&ptp->lock);

	ppe_tun_warn("%p: No VP instance for vp_num %u", ptp, vp_num);
	return NULL;
}

/*
 * ppe_tun_deactivate_internal()
 *	Common deactivation path shared by netdev-level and conn-entry-level deactivation.
 *
 * Caller must hold a lookup ref on tun and is responsible for releasing it once this
 * function returns. This function only releases the ref taken during this flow's
 * activation (on the success path), since that ref is the one it owns.
 */
static bool ppe_tun_deactivate_internal(struct ppe_tun *tun, void *vdestroy_rule, bool is_flush)
{
	bool unregister_ops = false;

	/*
	 * Guard against deactivating a tunnel already deactivated (e.g. by flush).
	 */
	spin_lock_bh(&ptp->lock);
	if (!(tun->state & PPE_TUN_STATE_ACTIVATED)) {
		spin_unlock_bh(&ptp->lock);
		ppe_tun_trace("%p: tunnel already deactivated, skipping\n", tun);
		return true;
	}
	spin_unlock_bh(&ptp->lock);

	if (!ppe_drv_tun_deactivate(tun->vp_num, vdestroy_rule, is_flush)) {
		ppe_tun_warn("%p: tunnel could not be deactivated", tun);
		return false;
	}

	/*
	 * Other flows are still using the hardware tunnel VP (e.g. MAP-T) - this
	 * flow's ref is released below, but the wrapper state stays intact until
	 * the hardware tunnel is actually inactive.
	 */
	if (ppe_drv_tun_is_active(tun->vp_num)) {
		return true;
	}

	/*
	 * Core hardware tunnel is now inactive - clear wrapper state and unregister
	 * offload ops.
	 */
	spin_lock_bh(&ptp->lock);
	if (tun->state & PPE_TUN_STATE_ACTIVATED) {
		tun->state &= ~PPE_TUN_STATE_ACTIVATED;
		unregister_ops = true;
	}
	spin_unlock_bh(&ptp->lock);

	if (unregister_ops) {
		ppe_tun_netdev_unregister_ol_ops(tun);
	}

	return true;
}

/*
 * ppe_tun_deactivate_with_conn_entry()
 *	Deactivate the ppe tunnel
 */
static bool ppe_tun_deactivate_with_conn_entry(uint8_t vp_num, void *vdestroy_rule, bool is_flush)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_vp_num_and_ref(vp_num);
	bool status;

	if (!tun) {
		return false;
	}

	status = ppe_tun_deactivate_internal(tun, vdestroy_rule, is_flush);
	if (!status) {
		ppe_tun_warn("%p: failed to deactivate tunnel with vp num: %u", tun, vp_num);
		ppe_tun_deref(tun);
		return status;
	}

	ppe_tun_info("%p: VP %u tunnel connection deactivated", tun, vp_num);

	/*
	 * Release reference taken on tun  at start of this function
	 */
	ppe_tun_deref(tun);

	/*
	 * Release the ref taken during tunnel activation.
	 */
	ppe_tun_deref(tun);

	return status;
}

/*
 * ppe_tun_activate_internal()
 *	Common activation path shared by netdev-level and conn-entry-level activation.
 *
 * Caller must hold a lookup ref on tun. On success this function keeps that ref
 * held on behalf of the activated flow (released by the matching deactivate); on
 * failure the ref is left untouched and the caller must release it.
 */
static bool ppe_tun_activate_internal(struct ppe_tun *tun, void *vcreate_rule)
{
	bool register_ops = false;

	if (!ppe_drv_tun_activate(tun->vp_num, vcreate_rule)) {
		ppe_tun_warn("%p: tunnel could not be activated", tun);
		return false;
	}

	tun->phys_dev = ppe_drv_port_get_vp_phys_dev(tun->dev);

	spin_lock_bh(&ptp->lock);
	if (!(tun->state & PPE_TUN_STATE_ACTIVATED)) {
		tun->state |= PPE_TUN_STATE_ACTIVATED;
		register_ops = true;
	}
	spin_unlock_bh(&ptp->lock);

	if (register_ops && ppe_tun_hybrid_offload_status_enabled(tun->type)) {
		if (ppe_tun_netdev_register_ol_ops(tun) < 0) {
			ppe_tun_warn("%p: Could not attach offload ops to tunnel", tun);
		}
	}

	if (ppe_drv_tun_header_length_get(tun->vp_num, &tun->tun_header_len)) {
		ppe_tun_warn("%p: Could not get tunnel header length", tun);
	}

	if (ppe_drv_tun_xmit_port_mtu_get(tun->vp_num, &tun->tun_xmit_port_mtu)) {
		ppe_tun_warn("%p: Could not get tunnel xmit port MTU", tun);
	}

	return true;
}

/*
 * ppe_tun_activate_with_conn_entry()
 *	Activate the ppe tunnel
 */
static bool ppe_tun_activate_with_conn_entry(uint8_t vp_num, void *create_rule)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_vp_num_and_ref(vp_num);
	if (!tun) {
		return false;
	}

	if (!ppe_tun_activate_internal(tun, create_rule)) {
		ppe_tun_deref(tun);
		return false;
	}

	ppe_tun_info("%p: VP %u tunnel activated", tun, vp_num);

	/*
	 * We should not deref tun here. we should hold ref on tun till deactivate.
	 */
	return true;
}

/*
 * ppe_tun_exception_dest_cb
 *	Callback handler for destination exception packets.
 */
static bool ppe_tun_exception_dest_cb(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;
	struct net_device *dev = skb->dev;
	ppe_tun_exception_method_t cb;
	struct ppe_tun *tun;

	tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun) {
		ppe_tun_warn("%p: tunnel not found for dev %s", ptp, dev->name);
		goto free_skb;
	}

	atomic64_inc(&tun->exception_packet);
	atomic64_add(skb->len, &tun->exception_bytes);

	cb = tun->dest_excp;
	if (!cb) {
		ppe_tun_warn("%p: No registered callback for destination exception %s", tun, dev->name);
		ppe_tun_deref(tun);
		goto free_skb;
	}

	cb(info, tun->tun_data);
	ppe_tun_deref(tun);

	return true;

free_skb:
	dev_kfree_skb_any(skb);
	return false;
}

/*
 * ppe_tun_exception_src_cb
 *	Callback handler for src exception packets.
 */
static bool ppe_tun_exception_src_cb(struct ppe_vp_cb_info *info, void *cb_data)
{
	struct sk_buff *skb = info->skb;
	struct net_device *dev = skb->dev;
	ppe_tun_exception_method_t cb;
	struct ppe_tun *tun;

	tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun) {
		ppe_tun_warn("%p: tunnel not found for dev %s", ptp, dev->name);
		goto free_skb;
	}

	atomic64_inc(&tun->exception_packet);
	atomic64_add(skb->len, &tun->exception_bytes);

	cb = tun->src_excp;
	if (!cb) {
		ppe_tun_deref(tun);
		ppe_tun_warn("%p: No registered callback for source exception %s", tun, dev->name);
		goto free_skb;
	}


	info->phys_dev = tun->phys_dev;

	if (info->fake_mac_present) {
		/*
		 * Tunnel exceptioned packets are not expected to contain
		 * fake MAC header.
		 * Move the skb->data to skip the fake MAC header
		 * Note: This scenario is anticipated in WLAN(Passive VP) + L3 tunnel case
		 */
		skb_pull_inline(skb, ETH_HLEN);
	}

	cb(info, tun->tun_data);
	ppe_tun_deref(tun);

	return true;

free_skb:
	dev_kfree_skb_any(skb);
	return false;
}

/*
 * ppe_tun_mtu_get()
 *	Get a tunnel mtu
 */
uint32_t ppe_tun_mtu_get(struct net_device *dev)
{
	uint16_t mtu;
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);

	if (!tun) {
		ppe_tun_warn("%p: Tunnel not found for dev %s", tun, dev->name);
		return false;
	}

	if (ppe_vp_mtu_get(tun->vp_num, &mtu) != PPE_VP_STATUS_SUCCESS) {
		ppe_tun_warn("%p: MTU Failed to get for dev %s", tun, dev->name);
		ppe_tun_deref(tun);
		return false;
	}

	ppe_tun_deref(tun);
	return mtu;
}
EXPORT_SYMBOL(ppe_tun_mtu_get);

/*
 * ppe_tun_mtu_set()
 *	Set a tunnel mtu
 */
bool ppe_tun_mtu_set(struct net_device *dev, uint32_t mtu)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	ppe_vp_num_t vp_num;
	bool status = false;

	if (!tun) {
		ppe_tun_warn("%p: Tunnel not found for dev %s", ptp, dev->name);
		return status;
	}

	vp_num = tun->vp_num;
	if (PPE_VP_STATUS_SUCCESS == ppe_vp_mtu_set(vp_num, mtu)) {
		status = true;
	}

	ppe_tun_deref(tun);
	return status;
}
EXPORT_SYMBOL(ppe_tun_mtu_set);

/*
 * ppe_tun_exception_packet_get()
 *	Get the number of exception packets and bytes the netdevice has seen.
 */
bool ppe_tun_exception_packet_get(struct net_device *dev, uint64_t *num_pkt, uint64_t *num_bytes)
{
	struct ppe_tun *tun;
	int i = 0;

	spin_lock_bh(&ptp->lock);
	for (i = 0; i < PPE_TUN_MAX; i++) {
		tun = ptp->tun[i];
		if (tun && (tun->dev == dev)) {
			*num_pkt = atomic64_read(&tun->exception_packet);
			*num_bytes = atomic64_read(&tun->exception_bytes);
			spin_unlock_bh(&ptp->lock);
			return true;
		}
	}
	spin_unlock_bh(&ptp->lock);

	return false;
}
EXPORT_SYMBOL(ppe_tun_exception_packet_get);

/*
 * ppe_tun_get_active_tun_cnt()
 *	Get the active number of struct ppe_tun
 */
uint32_t ppe_tun_get_active_tun_cnt(void)
{
	uint16_t count;

	count = PPE_TUN_MAX - atomic_read(&ptp->total_free);
	return count;
}
EXPORT_SYMBOL(ppe_tun_get_active_tun_cnt);

/*
 * ppe_tun_deactivate()
 *	Deactivate the struct ppe_tun
 */
bool ppe_tun_deactivate(struct net_device *dev)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	bool is_flush = false;

	if (!tun) {
		return false;
	}

	/*
	 * ppe_tun_deactivate_internal() guards against a tunnel that was never
	 * activated (e.g. ppe_tun_destroy called after a failed ppe_tun_activate,
	 * or a partially-created tunnel), avoiding an unbalanced deref.
	 */
	if (!ppe_tun_deactivate_internal(tun, NULL, is_flush)) {
		ppe_tun_warn("%p: failed to deactivate tunnel for dev %s", tun, dev->name);
		ppe_tun_deref(tun);
		return false;
	}

	ppe_tun_info("%p: Tunnel deactivated for dev %s", tun, dev->name);

	/*
	 * release reference on tunnel taken at start of this function
	 */
	ppe_tun_deref(tun);

	/*
	 * Release the ref taken during tunnel activation.
	 */
	ppe_tun_deref(tun);

	return true;
}
EXPORT_SYMBOL(ppe_tun_deactivate);

/*
 * ppe_tun_activate()
 *	Activate the struct ppe_tun
 */
bool ppe_tun_activate(struct net_device *dev)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);

	if (!tun) {
		ppe_tun_warn("%p: tunnel not found for dev %s", ptp, dev->name);
		return false;
	}

	if (!ppe_tun_activate_internal(tun, NULL)) {
		ppe_tun_warn("%p: tunnel activate failed for dev %s", ptp, dev->name);
		ppe_tun_deref(tun);
		return false;
	}

	ppe_tun_info("%p: tunnel activated for dev %s", tun, dev->name);

	/*
	 * We should not deref tun here. we should hold ref on tun till deactivate.
	 */
	return true;
}
EXPORT_SYMBOL(ppe_tun_activate);

/*
 * ppe_tun_xcpn_mode_get()
 *	Get the exception mode based on tunnel type.
 */
static uint8_t ppe_tun_xcpn_mode_get(struct ppe_tun *tun)
{
	uint8_t action = PPE_TUN_XCPN_MODE_0;

	switch (tun->type) {
	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP:
		action = ptp->xcpn_mode.gretap;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6:
		action = ptp->xcpn_mode.ipip6;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2:
		/*
		 * exception mode is PPE_TUN_XCPN_MODE_1 by default
		 * for L2TP
		 */
		action = ptp->xcpn_mode.l2tp;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN:
		action = ptp->xcpn_mode.gretun;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN:
		action = ptp->xcpn_mode.vxlan;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2:
	case PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3:
		action = tun->exp_mode;
		break;

	default:
		ppe_tun_info("Tunnel type %u is invalid or doesn't support xcpn mode.", tun->type);

	}

	tun->exp_mode = action;
	return action;
}

/*
 * ppe_tun_get_tun_vp_type()
 *	Return tunnel VP type from tunnel type.
 */
static ppe_vp_type_t ppe_tun_get_tun_vp_type(enum ppe_drv_tun_cmn_ctx_type type)
{
	switch (type) {
	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP:
	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN:
	case PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2:
		return PPE_VP_TYPE_HW_L2TUN;

	case PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6:
	case PPE_DRV_TUN_CMN_CTX_TYPE_MAPT:
	case PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2:
	case PPE_DRV_TUN_CMN_CTX_TYPE_CUST:
	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE:
	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN:
	case PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3:
		return PPE_VP_TYPE_HW_L3TUN;

	default:
		return PPE_VP_TYPE_MAX;
	}
}

/*
 * ppe_tun_conf_accel()
 *	Enable / Disable acceleration for a tunnel type
 */
bool ppe_tun_conf_accel(enum ppe_drv_tun_cmn_ctx_type type, bool action)
{
	struct ppe_tun *tun;
	int i;

	/*
	 * Allow setting tunnel accel mode to disabled only if ppe_tun of
	 * netdev is deconfigured.
	 */
	for (i = 0; (action == PPE_TUN_DISABLE) && (i < PPE_TUN_MAX); i++) {
		spin_lock_bh(&ptp->lock);
		tun = ptp->tun[i];
		if (!tun) {
			spin_unlock_bh(&ptp->lock);
			continue;
		}

		spin_unlock_bh(&ptp->lock);

		if (tun->state & PPE_TUN_STATE_CONFIGURED) {
			printk("dev %s should be down before disabling acceleration\n", tun->dev->name);
			return false;
		}

		atomic_inc(&ptp->free_pending);

		/*
		 * Release the reference taken during kref init
		 */
		ppe_tun_deref(tun);
	}

	switch (type) {
	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP:
		ptp->tun_accel.ppe_tun_gretap_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN:
		ptp->tun_accel.ppe_tun_vxlan_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6:
		ptp->tun_accel.ppe_tun_ipip6_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_MAPT:
		ptp->tun_accel.ppe_tun_mapt_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2:
		ptp->tun_accel.ppe_tun_l2tp_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_CUST:
		ptp->tun_accel.ppe_tun_cust_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE:
		ptp->tun_accel.ppe_tun_vxlan_gpe_accel = action;
		break;

	case PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN:
		ptp->tun_accel.ppe_tun_gretun_accel = action;
		break;

	default:
		ppe_tun_info("%p: Tunnel type %u is invalid", ptp, type);
		return false;
	}

	ppe_tun_info("%p: Acceleration %s for tunnel type %u", ptp, (action) ? ("enabled") : ("disabled"), type);
	return true;
}
EXPORT_SYMBOL(ppe_tun_conf_accel);

/*
 * ppe_tun_deconfigure()
 *	Disable the tunnel
 */
bool ppe_tun_deconfigure(struct net_device *dev)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);

	if (!tun) {
		ppe_tun_warn("%p: could not find tunnel for dev %s", ptp, dev->name);
		return false;
	}

	if (!ppe_drv_tun_deconfigure(tun->vp_num)) {
		ppe_tun_warn("%p: failed to disable tunnel for dev %s", tun, dev->name);
		ppe_tun_deref(tun);
		return false;
	}

	xchg(&tun->src_excp, NULL);
	xchg(&tun->dest_excp, NULL);
	xchg(&tun->stats_excp, NULL);
	xchg(&tun->phys_dev, NULL);

	tun->state &= ~PPE_TUN_STATE_CONFIGURED;
	ppe_tun_info("%p: Tunnel disabled for dev %s", tun, dev->name);
	ppe_tun_deref(tun);
	return true;
}
EXPORT_SYMBOL(ppe_tun_deconfigure);

/*
 * ppe_tun_configure()
 *	Configure a struct ppe_tun
 */
bool ppe_tun_configure(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr, struct ppe_tun_excp *tun_cb)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	void *add_cb = ppe_tun_activate_with_conn_entry;
	void *del_cb = ppe_tun_deactivate_with_conn_entry;
	ppe_vp_num_t vp_num;
	uint8_t action;
	bool status;

	if (!tun) {
		ppe_tun_warn("%p: tunnel not found for dev %s", ptp, dev->name);
		return false;
	}

	vp_num = tun->vp_num;

	if (tun_cb->tun_data && !ppe_tun_set_tun_data(tun, tun_cb->tun_data)) {
		ppe_tun_trace("%p: tunnel info set failed", tun);
		ppe_tun_deref(tun);
		return false;
	}

	if ((tun->type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2 ||
			tun->type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3)) {
		add_cb = NULL;
		del_cb = NULL;
		tun->exp_mode = tun_hdr->tun.user.decap_data.exp_mode;
	}

	status = ppe_drv_tun_configure(vp_num, tun_hdr, add_cb, del_cb);
	if (!status) {
		ppe_tun_trace("%p: tunnel driver configuration failed", tun);
		ppe_tun_deref(tun);
		return false;
	}

	/*
	 * Set exception mode.
	 */
	action = ppe_tun_xcpn_mode_get(tun);
	if (!ppe_drv_port_xcpn_mode_set(vp_num, action)) {
		ppe_tun_warn("%p: xcpn_mode set failed for dev %s", ptp, dev->name);
		ppe_drv_tun_deconfigure(vp_num);
		ppe_tun_deref(tun);
		return false;
	}

	tun->src_excp = tun_cb->src_excp_method;
	tun->dest_excp = tun_cb->dest_excp_method;
	tun->stats_excp = tun_cb->stats_update_method;


	tun->state |= PPE_TUN_STATE_CONFIGURED;
	ppe_tun_info("%p: Tunnel is configured at idx:%d", tun, tun->idx);
	ppe_tun_deref(tun);
	return true;
}
EXPORT_SYMBOL(ppe_tun_configure);

/*
 * ppe_tun_hybrid_ol_ctx_get
 * 	return tunnel vp number if hybrid offload is enabled and tunnel is active else return -1
 */
int16_t ppe_tun_hybrid_ol_ctx_get(struct net_device *dev)
{
	int16_t vp = -1;
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun) {
		goto exit;
	}

	if (!ppe_tun_hybrid_offload_status_enabled(tun->type)) {
		ppe_tun_deref(tun);
		goto exit;
	}

	if (tun->state & PPE_TUN_STATE_ACTIVATED) {
		vp = tun->vp_num;
	}

	ppe_tun_deref(tun);

exit:
	return vp;
}
EXPORT_SYMBOL(ppe_tun_hybrid_ol_ctx_get);

/*
 * ppe_tun_free()
 *	Free a struct ppe_tun
 */
bool ppe_tun_free(struct net_device *dev)
{
	struct ppe_tun *tun = ppe_tun_get_tun_by_netdev_and_ref(dev);

	if (!tun) {
		ppe_tun_info("%p: could not find tunnel for dev %s", ptp, dev->name);
		return false;
	}

	atomic_inc(&ptp->free_pending);

	ppe_tun_info("%p: tunnel is freed for dev %s", tun, dev->name);
	ppe_tun_deref(tun);

	/*
	 * This additional deref is for taken during kref_init
	 */
	ppe_tun_deref(tun);
	return true;
}
EXPORT_SYMBOL(ppe_tun_free);

/*
 * ppe_tun_alloc()
 *	Allocate a struct ppe_tun.
 */
bool ppe_tun_alloc(struct net_device *dev, enum ppe_drv_tun_cmn_ctx_type type)
{
	struct ppe_tun *tun;
	struct ppe_vp_ai vpai = {0};
	int32_t idx;
	ppe_vp_num_t vp_num;

	if (!atomic_read(&ptp->total_free)) {
		ppe_tun_info("%p: Max tunnel %u limit reached", ptp, PPE_TUN_MAX);
		return false;
	}

	/*
	 * Check if acceleration is allowed for this tunnel type
	 */
	if (!ppe_tun_allow_accel(type)) {
		return false;
	}

	ppe_tun_info("%p: PPE Tun Allocating dev:%p type:%d", ptp, dev, type);

	tun = kzalloc(sizeof(struct ppe_tun), GFP_ATOMIC);
	if (!tun) {
		atomic_inc(&ptp->alloc_fail);
		ppe_tun_warn("%p: ppe_tun alloc failed for dev %s", ptp, dev->name);
		return false;
	}

	/*
	 * Allocate PPE VP
	 */
	vpai.type = ppe_tun_get_tun_vp_type(type);
	if (vpai.type == PPE_VP_TYPE_MAX) {
		ppe_tun_warn("%p: tunnel type %u is invalid", dev, type);
		atomic_inc(&ptp->alloc_fail);
		kfree(tun);
		return false;
	}

	vpai.dst_cb = &ppe_tun_exception_dest_cb;
	vpai.src_cb = &ppe_tun_exception_src_cb;
	vpai.dst_cb_data = NULL;
	vpai.src_cb_data = NULL;
	vpai.stats_cb = ppe_tun_stats;

	/*
	 * For Tunnel VP the xmit port is determined during outer rule push. hence
	 * setting it as invalid
	 */
	vpai.xmit_port = PPE_DRV_PORT_ID_INVALID;

	vp_num = ppe_vp_alloc(dev, &vpai);
	if (vp_num == -1) {
		ppe_tun_warn("%p: vp alloc failed for dev %s", ptp, dev->name);
		atomic_inc(&ptp->alloc_fail);
		kfree(tun);
		return false;
	}

	/*
	 * Use the vp_num to index
	 */
	idx = PPE_TUN_VP_NUM_TO_IDX(vp_num);

	spin_lock_bh(&ptp->lock);
	if (ptp->tun[idx] != NULL) {
		spin_unlock_bh(&ptp->lock);
		ppe_tun_assert(false, "%p: Tunnel %p at index %u already allocated", dev, ptp->tun[idx], idx);
		ppe_vp_free(vp_num);
		kfree(tun);
		return false;
	}

	tun->dev = dev;
	tun->type = type;
	tun->vp_num = vp_num;
	tun->idx = idx;

	if (type == PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2 ||
			type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2 ||
			type == PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3) {
		tun->tun_data = ppe_tun_alloc_tun_data();
		if (!tun->tun_data) {
			spin_unlock_bh(&ptp->lock);
			ppe_vp_free(vp_num);
			kfree(tun);
			return false;
		}
	}

	nss_ppe_tun_minidump_log(tun, sizeof(struct ppe_tun), "ppe_tun");

	kref_init(&tun->ref);

	ptp->tun[idx] = tun;
	spin_unlock_bh(&ptp->lock);

	atomic_dec(&ptp->total_free);

	ppe_tun_info("%p: tunnel allocated for dev: %s idx: %u vp: %u", tun, dev->name, tun->idx, tun->vp_num);

	return true;
}
EXPORT_SYMBOL(ppe_tun_alloc);

/*
 * ppe_tun_setup()
 *	Alloc and Activate the tunnel
 */
bool ppe_tun_setup(struct net_device *dev, struct ppe_drv_tun_cmn_ctx *tun_hdr)
{
	struct ppe_tun_excp tun_cb = {0};
	bool status;

	status = ppe_tun_alloc(dev, tun_hdr->type);
	if (!status) {
		ppe_tun_warn("%p: tunnel allocation failed", dev);
		return false;
	}

	status = ppe_tun_configure(dev, tun_hdr, &tun_cb);
	if (!status) {
		ppe_tun_free(dev);
		ppe_tun_warn("%p: tunnel configuration failed", dev);
		return false;
	}

	status = ppe_tun_activate(dev);
	if (!status) {
		ppe_tun_deconfigure(dev);
		ppe_tun_free(dev);
		ppe_tun_warn("%p: tunnel activation failed", dev);
		return false;
	}

	ppe_tun_info("%p: tunnel setup success", dev);
	return true;
}
EXPORT_SYMBOL(ppe_tun_setup);

/*
 * ppe_tun_parse_custom_tun
 *     Parser Custom tunnel user parameter
 */
static bool ppe_tun_parse_custom_tun(struct ppe_tun_custom_tun *ctun, struct ppe_drv_tun_cmn_ctx *ctx)
{
	/*
	 * Fill L2 information
	 */
	ctx->l2.eth_type = ctun->l2.eth_type;

	ctx->l2.smac[0] = htons(ctun->l2.smac[0]);
	ctx->l2.smac[1] = htons(ctun->l2.smac[1]);
	ctx->l2.smac[2] = htons(ctun->l2.smac[2]);

	ctx->l2.dmac[0] = htons(ctun->l2.dmac[0]);
	ctx->l2.dmac[1] = htons(ctun->l2.dmac[1]);
	ctx->l2.dmac[2] = htons(ctun->l2.dmac[2]);

	ctx->l2.xmit_port = ppe_drv_port_num_from_ifname(ctun->l2.phy_port);
	if (ctx->l2.xmit_port == PPE_DRV_PORT_ID_INVALID) {
		ppe_tun_warn("%p: Invalid xmit port for %s", ctun, ctun->l2.phy_port);
		return false;
	}

	if (ctun->inner_type == PPE_TUN_INNER_TYPE_ETHER) {
		ctx->type = PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L2;
	} else if (ctun->inner_type == PPE_TUN_INNER_TYPE_IP) {
		ctx->type = PPE_DRV_TUN_CMN_CTX_TYPE_CUSTOM_L3;
	} else {
		ppe_tun_warn("%p: invalid inner type %d", ctun, ctun->inner_type);
		return false;
	}

	if (ctun->l2.vlan_enabled) {
		ctx->l2.vlan[0].tpid = ctun->l2.vlan.cvlan_tpid;
		ctx->l2.vlan[0].tci = ctun->l2.vlan.cvlan_tci;
		ctx->l2.flags |= PPE_DRV_TUN_CMN_CTX_L2_CVLAN_VALID;

		if (ctun->l2.vlan.svlan_valid) {
			ctx->l2.vlan[1].tpid = ctun->l2.vlan.svlan_tpid;
			ctx->l2.vlan[1].tci = ctun->l2.vlan.svlan_tci;
			ctx->l2.flags |= PPE_DRV_TUN_CMN_CTX_L2_SVLAN_VALID;
		}
	}

	if (ctun->l2.pppoe_enabled) {
		memcpy(&ctx->l2.pppoe.ph, &ctun->l2.pppoe.ph, sizeof(ctx->l2.pppoe.ph));
		ctx->l2.pppoe.ppp_proto = ctun->l2.pppoe.ppp_protocol;
		memcpy(ctx->l2.pppoe.server_mac, ctun->l2.pppoe.pppoe_server_mac, sizeof(ctx->l2.pppoe.server_mac));
		ctx->l2.flags |= PPE_DRV_TUN_CMN_CTX_L2_PPPOE_VALID;
	}

	/*
	 * Fill L3 information
	 */
	if (ctun->l3.l3_type == PPE_TUN_L3_HDR_TYPE_IPV4) {
		ctx->l3.flags |= PPE_DRV_TUN_CMN_CTX_L3_IPV4;
		ctx->l3.saddr[0] = ctun->l3.saddr[0];
		ctx->l3.daddr[0] = ctun->l3.daddr[0];
		ctx->l3.proto = ctun->l3.proto;
	} else if (ctun->l3.l3_type == PPE_TUN_L3_HDR_TYPE_IPV6) {
		ctx->l3.flags |= PPE_DRV_TUN_CMN_CTX_L3_IPV6;
		ctx->l3.saddr[0] = ctun->l3.saddr[0];
		ctx->l3.saddr[1] = ctun->l3.saddr[1];
		ctx->l3.saddr[2] = ctun->l3.saddr[2];
		ctx->l3.saddr[3] = ctun->l3.saddr[3];
		ctx->l3.daddr[0] = ctun->l3.daddr[0];
		ctx->l3.daddr[1] = ctun->l3.daddr[1];
		ctx->l3.daddr[2] = ctun->l3.daddr[2];
		ctx->l3.daddr[3] = ctun->l3.daddr[3];
		ctx->l3.proto = ctun->l3.proto;
	} else {
		ppe_tun_warn("%p: invalid L3 header type %d", ctun, ctun->l3.l3_type);
		return false;
	}

	/*
	 * Fill L4 information
	 */
	if (ctun->udp_hdr_valid) {
		ctx->tun.user.udp_hdr_valid = true;
		memcpy(&ctx->tun.user.udph, &ctun->udph, sizeof(ctx->tun.user.udph));
	}

	(!ctun->ecap.copy_ttl) ? (ctx->l3.ttl = ctun->ecap.ttl) : (ctx->l3.flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_TTL);
	(!ctun->ecap.copy_dscp) ? (ctx->l3.dscp = ctun->ecap.dscp) : (ctx->l3.flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DSCP);
	if (ctun->ecap.copy_df_bit) {
		ctx->l3.flags |= PPE_DRV_TUN_CMN_CTX_L3_INHERIT_DF;
	} else if (ctun->ecap.df_bit) {
		ctx->l3.flags |= PPE_DRV_TUN_CMN_CTX_L3_DF_BIT_SET;
	}

	/*
	 * Copy encap configurations
	 */
	ctx->l3.encap_ecn_mode = (enum ppe_drv_tun_cmn_ctx_encap_ecn)ctun->ecap.ecn_mode;

	if (ctun->ecap.encap_hdr_data_len > PPE_TUN_ENCAP_HDR_DATA_LEN) {
		ppe_tun_warn("%p: encap_hdr_data_len %u exceeds maximum %u",
			     ctun, ctun->ecap.encap_hdr_data_len, PPE_TUN_ENCAP_HDR_DATA_LEN);
		return false;
	}

	ctx->tun.user.encap_data.encap_hdr_data_len = ctun->ecap.encap_hdr_data_len;
	memcpy(ctx->tun.user.encap_data.encap_hdr_data, ctun->ecap.encap_hdr_data,
	       ctun->ecap.encap_hdr_data_len);
	if (ctun->ecap.proto_update.proto_update_en) {
		if (ctun->ecap.proto_update.proto_size > PPE_TUN_ENCAP_HDR_DATA_LEN) {
			ppe_tun_warn("%p: proto_size %u exceeds maximum %u bits",
				     ctun, ctun->ecap.proto_update.proto_size, PPE_TUN_ENCAP_HDR_DATA_LEN);
			return false;
		}

		ctx->tun.user.encap_data.proto_update_en = true;
		ctx->tun.user.encap_data.proto_offset = ctun->ecap.proto_update.proto_offset;
		ctx->tun.user.encap_data.proto_size = ctun->ecap.proto_update.proto_size;
		ctx->tun.user.encap_data.ipv4_proto = ctun->ecap.proto_update.ipv4_proto;
		ctx->tun.user.encap_data.ipv6_proto = ctun->ecap.proto_update.ipv6_proto;
	}

	/*
	 * Copy decap configurations
	 */
	ctx->l3.decap_ecn_mode = (enum ppe_drv_tun_cmn_ctx_decap_ecn)ctun->dcap.dcap_ecn;
	if (ctun->dcap.ctrl.sip_match)
		ppe_drv_tun_cust_match_flag_set(&ctx->tun.user.decap_data.match_bitmap,
						PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_SIP_EN);
	if (ctun->dcap.ctrl.dip_match)
		ppe_drv_tun_cust_match_flag_set(&ctx->tun.user.decap_data.match_bitmap,
						PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_DIP_EN);
	if (ctun->dcap.ctrl.proto_match)
		ppe_drv_tun_cust_match_flag_set(&ctx->tun.user.decap_data.match_bitmap,
						PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_PROTO_EN);
	if (ctun->dcap.ctrl.sport_match)
		ppe_drv_tun_cust_match_flag_set(&ctx->tun.user.decap_data.match_bitmap,
						PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_SPORT_EN);
	if (ctun->dcap.ctrl.dport_match)
		ppe_drv_tun_cust_match_flag_set(&ctx->tun.user.decap_data.match_bitmap,
						PPE_DRV_TUN_CMN_CTX_CUST_MATCH_DCAP_DPORT_EN);

	if (ctun->dcap.id ==  PPE_TUN_DECAP_TUN_IDENTIFIER_ETHERNET_PROTO) {
		ctx->tun.user.decap_data.tunnel_identifier = PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_ETH_PROTO;
	} else if (ctun->dcap.id ==  PPE_TUN_DECAP_TUN_IDENTIFIER_IP_PROTO) {
		ctx->tun.user.decap_data.tunnel_identifier = PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_IP_PROTO;
	} else if (ctun->dcap.id == PPE_TUN_DECAP_TUN_IDENTIFIER_UDP_PORT) {
		ctx->tun.user.decap_data.tunnel_identifier = PPE_DRV_TUN_CMN_CTX_CUST_TUN_ID_UDP_PORT;
	} else {
		ppe_tun_warn("%p: invalid tunnel identifier type %d", ctun, ctun->dcap.id);
		return false;
	}

	if (ctun->dcap.offset_type == PPE_TUN_CUSTOM_TUN_DECAP_UDF_OFFSET_L2_START) {
		ctx->tun.user.decap_data.offset_type = PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L2_START;
	} else if (ctun->dcap.offset_type == PPE_TUN_CUSTOM_TUN_DECAP_UDF_OFFSET_L3_START) {
		ctx->tun.user.decap_data.offset_type = PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L3_START;
	} else if (ctun->dcap.offset_type == PPE_TUN_CUSTOM_TUN_DECAP_UDF_OFFSET_L4_START) {
		ctx->tun.user.decap_data.offset_type = PPE_DRV_TUN_CMN_CTX_CUST_TUN_OFFSET_L4_START;
	} else {
		ppe_tun_warn("%p: invalid offset type %d", ctun, ctun->dcap.offset_type);
		return false;
	}

	/*
	 * Copy UDF offset configurations
	 */
	ctx->tun.user.decap_data.udf0_offset = ctun->dcap.udf_offset.udf0_offset;
	ctx->tun.user.decap_data.udf1_offset = ctun->dcap.udf_offset.udf1_offset;
	ctx->tun.user.decap_data.udf2_offset = ctun->dcap.udf_offset.udf2_offset;
	ctx->tun.user.decap_data.instance_udf0_offset = ctun->dcap.udf_offset.instance_udf0_offset;
	ctx->tun.user.decap_data.instance_udf1_offset = ctun->dcap.udf_offset.instance_udf1_offset;

	/*
	 * Copy instance match configuration (tunnel UDF for instance identification)
	 */
	ctx->tun.user.decap_data.instance_match.instance_udf0_en = ctun->dcap.instance_match.instance_udf0_en;
	ctx->tun.user.decap_data.instance_match.instance_udf0_val = ctun->dcap.instance_match.instance_udf0_val;
	ctx->tun.user.decap_data.instance_match.instance_udf0_mask = ctun->dcap.instance_match.instance_udf0_mask;
	ctx->tun.user.decap_data.instance_match.instance_udf1_en = ctun->dcap.instance_match.instance_udf1_en;
	ctx->tun.user.decap_data.instance_match.instance_udf1_val = ctun->dcap.instance_match.instance_udf1_val;
	ctx->tun.user.decap_data.instance_match.instance_udf1_mask = ctun->dcap.instance_match.instance_udf1_mask;

	if (ctun->dcap.udp_csum_zero_en) {
		ctx->tun.user.decap_data.udp_csum_zero_en = true;
	}

	/*
	 * Copy type match configuration (program UDF entries for tunnel type identification)
	 * Note: type_match contains multiple entries with udf0/1/2 fields for packet type matching
	 */
	if (ctun->dcap.type_match.num_entries == 0 ||
			ctun->dcap.type_match.num_entries > PPE_TUN_PRGM_UDF_MAX_ENTRIES) {
		ppe_tun_warn("%p: type_match.num_entries %u is invalid (must be 1-%u)",
			     ctun, ctun->dcap.type_match.num_entries, PPE_TUN_PRGM_UDF_MAX_ENTRIES);
		return false;
	}

	memcpy(&ctx->tun.user.decap_data.type_match, &ctun->dcap.type_match, sizeof(ctx->tun.user.decap_data.type_match));

	ctx->tun.user.decap_data.exp_mode = ctun->dcap.dcap_exp_mode;
	ctx->tun.user.decap_data.decap_data_len = ctun->dcap.outer_hdr_length;

	return true;
}

/*
 * ppe_tun_custom_tun_exp_handler
 *     Custom tunnel Exception packet handler
 */
static bool ppe_tun_custom_tun_exp_handler(struct ppe_vp_cb_info *info, ppe_tun_data *tun_data)
{
	ppe_tun_custom_exp_method_t cb = tun_data->cust_info.exp_cb;

	if (!cb) {
		ppe_tun_warn("No exception callback registered for custom tunnel, dropping packet\n");
		dev_kfree_skb_any(info->skb);
		return false;
	}

	if (!cb(info->skb, tun_data->cust_info.custom_tunnel_data)) {
		ppe_tun_warn("exception packet handling failed\n");
		return false;
	}

	return true;
}

/*
 * ppe_tun_custom_tun_stats_update
 *    Custom tunnel Stats update handler
 */
static bool ppe_tun_custom_tun_stats_update(struct net_device *dev, ppe_tun_hw_stats *stats, ppe_tun_data *tun_data)
{
	ppe_tun_custom_stats_method_t cb = tun_data->cust_info.stats_cb;
	struct ppe_drv_tun_custom_tun_stats dev_stats = {0};

	if (!cb) {
		ppe_tun_trace("No stats callback registered for custom tunnel");
		return true;
	}

	dev_stats.rx_pkt_cnt = stats->rx_pkt_cnt;
	dev_stats.rx_byte_cnt = stats->rx_byte_cnt;
	dev_stats.rx_drop_pkt_cnt = stats->rx_drop_pkt_cnt;
	dev_stats.rx_drop_byte_cnt = stats->rx_drop_byte_cnt;
	dev_stats.tx_pkt_cnt = stats->tx_pkt_cnt;
	dev_stats.tx_byte_cnt = stats->tx_byte_cnt;
	dev_stats.tx_drop_pkt_cnt = stats->tx_drop_pkt_cnt;
	dev_stats.tx_drop_byte_cnt = stats->tx_drop_byte_cnt;

	cb(dev, &dev_stats, tun_data->cust_info.custom_tunnel_data);

	return true;
}

/*
 * ppe_tun_destroy_custom_tunnel
 *      Destroy custom tunnel
 */
ppe_tun_ret_t ppe_tun_destroy_custom_tunnel(struct net_device *dev)
{
	if (!ppe_tun_deactivate(dev)) {
		ppe_tun_warn("%p: Custom tunnel deactivation failed", dev);
		return PPE_TUN_RET_CUSTOM_TUN_DEACTIVATE_FAILED;
	}

	if (!ppe_tun_deconfigure(dev)) {
		ppe_tun_warn("%p: Custom tunnel deconfigure failed", dev);
		return PPE_TUN_RET_CUSTOM_TUN_DECONFIGURE_FAILED;
	}

	if (!ppe_tun_free(dev)) {
		ppe_tun_warn("%p: Custom tunnel free failed", dev);
		return PPE_TUN_RET_CUSTOM_TUN_FREE_FAILED;
	}

	ppe_tun_info("%p: Custom tunnel destroy successful", dev);
	return PPE_TUN_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_tun_destroy_custom_tunnel);

/*
 * ppe_tun_create_custom_tunnel
 *      Create custom tunnel
 */
ppe_tun_ret_t ppe_tun_create_custom_tunnel(struct net_device *dev, struct ppe_tun_custom_tun *ctun)
{
	struct ppe_drv_tun_cmn_ctx ctx = {0};
	struct ppe_tun_excp tun_cb = {0};
	ppe_tun_data tun_data = {0};

	if (!ppe_tun_parse_custom_tun(ctun, &ctx)) {
		ppe_tun_warn("Failed to parse custom tunnel args");
		return PPE_TUN_RET_CUSTOM_TUN_INVALID_INPUT;
	}

	if (!ppe_tun_alloc(dev, ctx.type)) {
		ppe_tun_warn("Failed to alloc tunnel instance for %s dev", dev->name);
		return PPE_TUN_RET_CUSTOM_TUN_ALLOC_FAILED;
	}

	tun_cb.tun_data = &tun_data;
	tun_cb.src_excp_method = ppe_tun_custom_tun_exp_handler;
	tun_cb.stats_update_method = ppe_tun_custom_tun_stats_update;
	tun_cb.tun_data->cust_info.custom_tunnel_data = ctun->tunnel_data;
	tun_cb.tun_data->cust_info.exp_cb = ctun->excp_cb;
	tun_cb.tun_data->cust_info.stats_cb = ctun->stats_cb;

	if (!ppe_tun_configure(dev, &ctx, &tun_cb)) {
		ppe_tun_warn("Failed to configure tunnel for %s dev", dev->name);
		goto err_cleanup_alloc;
	}

	if (!ppe_tun_activate(dev)) {
		ppe_tun_warn("Failed to Activate tunnel for %s dev", dev->name);
		goto err_cleanup_configure;
	}

	return PPE_TUN_RET_SUCCESS;

err_cleanup_configure:
	ppe_tun_deconfigure(dev);
	ppe_tun_free(dev);
	return PPE_TUN_RET_CUSTOM_TUN_ACTIVATE_FAILED;

err_cleanup_alloc:
	ppe_tun_free(dev);
	return PPE_TUN_RET_CUSTOM_TUN_CONFIGURE_FAILED;
}
EXPORT_SYMBOL(ppe_tun_create_custom_tunnel);

/*
 * ppe_tun_destroy()
 *	deactivate and free the tunnel
 */
bool ppe_tun_destroy(struct net_device *dev)
{
	if (!ppe_tun_deactivate(dev)) {
		ppe_tun_warn("%p: tunnel deactivation failed", dev);
		return false;
	}

	if (!ppe_tun_deconfigure(dev)) {
		ppe_tun_warn("%p: tunnel deconfigure failed", dev);
		return false;
	}

	if (!ppe_tun_free(dev)) {
		ppe_tun_warn("%p: tunnel free failed", dev);
		return false;
	}

	ppe_tun_info("%p: tunnel destroy successful", dev);
	return true;
}
EXPORT_SYMBOL(ppe_tun_destroy);

/*
 * ppe_tun_decap_disable()
 *	Disable tunnel decapsulation
 */
bool ppe_tun_decap_disable(struct net_device *dev)
{
	struct ppe_tun *tun;
	bool ret;

	tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun) {
		ppe_tun_assert(0, "%p: Failed disabling decap for %s", tun, dev->name);
		return false;
	}

	ret = ppe_drv_tun_decap_disable_by_port_num(tun->vp_num);
	if (!ret) {
		ppe_tun_warn("%p, Failed to disable the decap for %s", tun, dev->name);
	}

	ppe_tun_trace("%px: Successfully enabled decap for nss_dev %s", tun, dev->name);
	ppe_tun_deref(tun);
	return ret;
}
EXPORT_SYMBOL(ppe_tun_decap_disable);

/*
 * ppe_tun_decap_enable()
 *	Enable tunnel decapsulation
 */
bool ppe_tun_decap_enable(struct net_device *dev)
{
	struct ppe_tun *tun;
	bool ret;

	tun = ppe_tun_get_tun_by_netdev_and_ref(dev);
	if (!tun) {
		ppe_tun_assert(0, "%p: Failed disabling decap for %s", tun, dev->name);
		return false;
	}

	ret = ppe_drv_tun_decap_enable_by_port_num(tun->vp_num);
	if (!ret) {
		ppe_tun_warn("%p, Failed to disable the decap for %s", tun, dev->name);
	}

	ppe_tun_trace("%px: Successfully enabled decap for nss_dev %s", tun, dev->name);
	ppe_tun_deref(tun);
	return ret;
}
EXPORT_SYMBOL(ppe_tun_decap_enable);

/*
 * ppe_tun_configure_vxlan_dport()
 *	Configure the VXLAN destination port
 */
bool ppe_tun_configure_vxlan_dport(uint16_t dport)
{
	ppe_tun_trace("Configuring the destination port of VXLAN dport: %u", dport);
	return ppe_drv_tun_configure_vxlan_and_dport(dport);
}
EXPORT_SYMBOL(ppe_tun_configure_vxlan_dport);

/*
 * ppe_tun_configure_vxlan_gpe_dport()
 *	Configure the VXLAN-GPE destination port
 */
bool ppe_tun_configure_vxlan_gpe_dport(uint16_t dport)
{
	ppe_tun_trace("Configuring the destination port of VXLAN-GPE dport: %u", dport);
	return ppe_drv_tun_configure_vxlan_gpe_and_dport(dport);
}
EXPORT_SYMBOL(ppe_tun_configure_vxlan_gpe_dport);

/*
 * ppe_tun_l2tp_port_set()
 *      Set L2TP source and destination  port
 */
bool ppe_tun_l2tp_port_set(uint16_t sport, uint16_t dport)
{
	ppe_tun_trace("Set L2TP sport: %u, dport: %u", sport, dport);
	return ppe_drv_tun_l2tp_port_set(sport, dport);
}
EXPORT_SYMBOL(ppe_tun_l2tp_port_set);

/*
 * ppe_tun_l2tp_port_get()
 *      get L2TP source and destination port
 */
bool ppe_tun_l2tp_port_get(uint16_t *sport, uint16_t *dport)
{
	return ppe_drv_tun_l2tp_port_get(sport, dport);
}
EXPORT_SYMBOL(ppe_tun_l2tp_port_get);

/*
 * ppe_tun_l2tp_read()
 *	l2tp read handler
 */
static ssize_t ppe_tun_l2tp_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "l2tp accel %s\n", (ptp->tun_accel.ppe_tun_l2tp_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_l2tp_write()
 *	l2tp write handler
 */
static ssize_t ppe_tun_l2tp_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for l2tp configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for l2tp configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_L2TP_V2, res);

	return len;
}

const struct file_operations ppe_tun_l2tp_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_l2tp_write,
	.read = ppe_tun_l2tp_read,
};

/*
 * ppe_tun_cust_read()
 *	custom_tunnel read handler
 */
static ssize_t ppe_tun_cust_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "custom_tunnel accel %s\n", (ptp->tun_accel.ppe_tun_cust_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_cust_write()
 *	custom_tunnel write handler
 */
static ssize_t ppe_tun_cust_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for custom_tunnel configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for custom_tunnel configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_CUST, res);

	return len;
}

/*
 * ppe_tun_cust_file_ops
 * Custom tunnel enable/disable file ops.
 */
const struct file_operations ppe_tun_cust_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_cust_write,
	.read = ppe_tun_cust_read,
};

/*
 * ppe_tun_gretap_read()
 *	gretap read handler
 */
static ssize_t ppe_tun_gretap_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "gretap accel %s\n", (ptp->tun_accel.ppe_tun_gretap_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_gretap_write()
 *	gretap write handler
 */
static ssize_t ppe_tun_gretap_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for gretap configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for gretap configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETAP, res);

	return len;
}

const struct file_operations ppe_tun_gretap_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_gretap_write,
	.read = ppe_tun_gretap_read,
};

/*
 * ppe_tun_gretun_read()
 *	gretun read handler
 */
static ssize_t ppe_tun_gretun_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "gretun accel %s\n", (ptp->tun_accel.ppe_tun_gretun_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_gretun_write()
 *	gretap write handler
 */
static ssize_t ppe_tun_gretun_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for gretun configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for gretun configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_GRETUN, res);

	return len;
}

const struct file_operations ppe_tun_gretun_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_gretun_write,
	.read = ppe_tun_gretun_read,
};

/*
 * ppe_tun_vxlan_read()
 *	vxlan read handler
 */
static ssize_t ppe_tun_vxlan_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "vxlan accel %s\n", (ptp->tun_accel.ppe_tun_vxlan_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_vxlan_write()
 *	vxlan write handler
 */
static ssize_t ppe_tun_vxlan_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for vxlan configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for vxlan configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN, res);

	return len;
}

const struct file_operations ppe_tun_vxlan_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_vxlan_write,
	.read = ppe_tun_vxlan_read,
};

/*
 * ppe_tun_mapt_read()
 *	mapt read handler
 */
static ssize_t ppe_tun_mapt_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "mapt accel %s\n", (ptp->tun_accel.ppe_tun_mapt_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_mapt_write()
 *	mapt write handler
 */
static ssize_t ppe_tun_mapt_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for mapt configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for mapt configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_MAPT, res);

	return len;
}

const struct file_operations ppe_tun_mapt_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_mapt_write,
	.read = ppe_tun_mapt_read,
};

/*
 * ppe_tun_ipip6_read()
 *	ipip6 read handler
 */
static ssize_t ppe_tun_ipip6_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];

	len = snprintf(lbuf, sizeof(lbuf), "ipip6 accel %s\n", (ptp->tun_accel.ppe_tun_ipip6_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_ipip6_write()
 *	ipip6 write handler
 */
static ssize_t ppe_tun_ipip6_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for ipip6 configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for ipip6 configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_IPIP6, res);

	return len;
}

const struct file_operations ppe_tun_ipip6_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_ipip6_write,
	.read = ppe_tun_ipip6_read,
};

/*
 * ppe_tun_stats_show()
 *	Read ppe tunnel statistics.
 */
static int ppe_tun_stats_show(struct seq_file *m, void __attribute__((unused))*ptr)
{
	struct ppe_tun *tun;
	int i = 0;

	seq_puts(m, "\n################ PPE Tunnel Statistics Start ################\n");
	seq_puts(m, "PPE tunnel driver stats\n");
	seq_printf(m, "\tMax tunnels: %u\n", PPE_TUN_MAX);
	seq_printf(m, "\tActive tunnels: %u\n", (PPE_TUN_MAX - atomic_read(&ptp->total_free)));
	seq_printf(m, "\tDelete pending: %u\n", atomic_read(&ptp->free_pending));
	seq_printf(m, "\tAlloc fail: %u\n", atomic_read(&ptp->alloc_fail));
	seq_puts(m, "\n");

	seq_puts(m, "PPE tunnel stats\n");

	spin_lock_bh(&ptp->lock);
	for (i = 0; i < PPE_TUN_MAX; i++) {
		tun = ptp->tun[i];
		if (tun) {
			seq_printf(m, "dev: %s\n", tun->dev->name);
			seq_printf(m, "\t index %d\n", tun->idx);
			seq_printf(m, "\t VP number %d\n", tun->vp_num);
			seq_printf(m, "\t type %u\n", tun->type);
			seq_printf(m, "\t physical dev: %s\n", (tun->phys_dev) ? (tun->phys_dev->name) : (""));
			seq_printf(m, "\t exception packet: %llu\n", atomic64_read(&tun->exception_packet));
			seq_printf(m, "\t exception bytes: %llu\n", atomic64_read(&tun->exception_bytes));
			seq_printf(m, "\t hybrid_offload_tx_fail_cnt: %llu\n", atomic64_read(&tun->tun_hybrid_offload_tx_fail_cnt));
			seq_printf(m, "\t hybrid_offload_tx_pkt_cnt: %llu\n", atomic64_read(&tun->tun_hybrid_offload_tx_pkt_cnt));
			seq_puts(m, "\n");
		}
	}
	spin_unlock_bh(&ptp->lock);

	seq_puts(m, "\n################ PPE Tunnel Statistics End  ################\n");

	return 0;
}

/*
 * ppe_tun_stats_open()
 */
static int ppe_tun_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ppe_tun_stats_show, inode->i_private);
}

/*
 * nss_ppe_tunipip6_stats_ops
 *	File operations for PPE tunnel driver stats
 */
static const struct file_operations ppe_tun_stats_ops = {
	.open = ppe_tun_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release
};

/*
 * ppe_tun_xcpn_gretap_read()
 *	gretap xcpn read handler
 */
static ssize_t ppe_tun_xcpn_gretap_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];
	uint8_t xcpn_mode = ptp->xcpn_mode.gretap;

	len = snprintf(lbuf, sizeof(lbuf), "Gretap xcpn mode %u \n", xcpn_mode);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_xcpn_gretap_write()
 *	gretap xcpn write handler
 */
static ssize_t ppe_tun_xcpn_gretap_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for gretap configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for gretap configuration", ptp);
		return status;
	}

	ptp->xcpn_mode.gretap = (uint8_t) res;

	return len;
}

const struct file_operations ppe_tun_gretap_xcpn_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_xcpn_gretap_write,
	.read = ppe_tun_xcpn_gretap_read,
};

/*
 * ppe_tun_xcpn_gretun_read()
 *	gretun xcpn read handler
 */
static ssize_t ppe_tun_xcpn_gretun_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];
	uint8_t xcpn_mode = ptp->xcpn_mode.gretun;

	len = snprintf(lbuf, sizeof(lbuf), "Gretun xcpn mode %u \n", xcpn_mode);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_xcpn_gretun_write()
 *	gretun xcpn write handler
 */
static ssize_t ppe_tun_xcpn_gretun_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for gretun configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for gretun configuration", ptp);
		return status;
	}

	ptp->xcpn_mode.gretun = (uint8_t) res;

	return len;
}

const struct file_operations ppe_tun_gretun_xcpn_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_xcpn_gretun_write,
	.read = ppe_tun_xcpn_gretun_read,
};

/*
 * ppe_tun_xcpn_ipip6_read()
 *	ipip6 xcpn read handler
 */
static ssize_t ppe_tun_xcpn_ipip6_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];
	uint8_t xcpn_mode = ptp->xcpn_mode.ipip6;

	len = snprintf(lbuf, sizeof(lbuf), "IPIP6 xcpn mode %u \n", xcpn_mode);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_xcpn_ipip6_write()
 *	ipip6 xcpn write handler
 */
static ssize_t ppe_tun_xcpn_ipip6_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for ipip6 configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for ipip6 configuration", ptp);
		return status;
	}

	ptp->xcpn_mode.ipip6 = (uint8_t) res;

	return len;
}

const struct file_operations ppe_tun_ipip6_xcpn_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_xcpn_ipip6_write,
	.read = ppe_tun_xcpn_ipip6_read,
};

/*
 * ppe_tun_xcpn_l2tp_read()
 *	l2tp xcpn read handler
 */
static ssize_t ppe_tun_xcpn_l2tp_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];
	uint8_t xcpn_mode = ptp->xcpn_mode.l2tp;

	len = snprintf(lbuf, sizeof(lbuf), "L2TP xcpn mode %u \n", xcpn_mode);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_xcpn_l2tp_write()
 *	l2tp xcpn write handler
 */
static ssize_t ppe_tun_xcpn_l2tp_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for l2tp configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for l2tp configuration", ptp);
		return status;
	}

	ptp->xcpn_mode.l2tp = (uint8_t) res;

	return len;
}

const struct file_operations ppe_tun_l2tp_xcpn_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_xcpn_l2tp_write,
	.read = ppe_tun_xcpn_l2tp_read,
};

/*
 * ppe_tun_xcpn_vxlan_read()
 *	vxlan xcpn read handler
 */
static ssize_t ppe_tun_xcpn_vxlan_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[24];
	uint8_t xcpn_mode = ptp->xcpn_mode.vxlan;

	len = snprintf(lbuf, sizeof(lbuf), "Vxlan xcpn mode %u \n", xcpn_mode);

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_xcpn_vxlan_write()
 *	vxlan xcpn write handler
 */
static ssize_t ppe_tun_xcpn_vxlan_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for vxlan configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for vxlan configuration", ptp);
		return status;
	}

	ptp->xcpn_mode.vxlan = (uint8_t) res;

	return len;
}

const struct file_operations ppe_tun_vxlan_xcpn_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_xcpn_vxlan_write,
	.read = ppe_tun_xcpn_vxlan_read,
};

/*
 * ppe_tun_hybrid_ol_cfg_read()
 *	gretap hybrid offload read handler
 */
static ssize_t ppe_tun_hybrid_ol_cfg_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[60];

	len = snprintf(lbuf, sizeof(lbuf), "gretap hybrid offload status: %s \n",
			(ptp->tun_hb_info.en_gretap_hybrid_ol) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_gretap_hb_offload_write()
 *	gretap hybrid offload write handler
 */
static ssize_t ppe_tun_gretap_hb_offload_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for gretap hybrid offload configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for gretap hybrid offload configuration", ptp);
		return status;
	}

	ptp->tun_hb_info.en_gretap_hybrid_ol = (uint8_t)res;

	return len;
}
const struct file_operations ppe_tun_gretap_hb_offload_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_gretap_hb_offload_write,
	.read = ppe_tun_hybrid_ol_cfg_read,
};

/*
 * ppe_tun_vxlan_gpe_read()
 *	vxlan-gpe read handler
 */
static ssize_t ppe_tun_vxlan_gpe_read(struct file *f, char *buf, size_t count, loff_t *offset)
{
	int len;
	char lbuf[32];

	len = snprintf(lbuf, sizeof(lbuf), "vxlan-gpe accel %s\n", (ptp->tun_accel.ppe_tun_vxlan_gpe_accel) ? ("enabled") : ("disabled"));

	return simple_read_from_buffer(buf, count, offset, lbuf, len);
}

/*
 * ppe_tun_vxlan_gpe_write()
 *	vxlan-gpe write handler
 */
static ssize_t ppe_tun_vxlan_gpe_write(struct file *f, const char *buffer, size_t len, loff_t *offset)
{
	ssize_t size;
	char data[16];
	bool res;
	int status;

	size = simple_write_to_buffer(data, sizeof(data), offset, buffer, len);
	if (size < 0) {
		ppe_tun_warn("%p: Error reading the input for vxlan gpe configuration", ptp);
		return size;
	}

	status = kstrtobool(data, &res);
	if (status) {
		ppe_tun_warn("%p: Error reading the input for vxlan gpe configuration", ptp);
		return status;
	}

	ppe_tun_conf_accel(PPE_DRV_TUN_CMN_CTX_TYPE_VXLAN_GPE, res);

	return len;
}

const struct file_operations ppe_tun_vxlan_gpe_file_fops = {
	.owner = THIS_MODULE,
	.write = ppe_tun_vxlan_gpe_write,
	.read = ppe_tun_vxlan_gpe_read,
};

static void ppe_tun_unreg_netdev_ol_ops_all(void) {
	struct ppe_tun *tun;
	int i;
	bool sync = false;

	spin_lock_bh(&ptp->lock);
	for (i = 0; i < PPE_TUN_MAX; i++) {
		tun = ptp->tun[i];
		if (tun) {
			ppe_tun_netdev_unregister_ol_ops(tun);
			sync = true;
		}
	}
	spin_unlock_bh(&ptp->lock);

	if (sync) {
		synchronize_rcu();
	}
}

/*
 * ppe_tun_module_init()
 *	module init for ppe tunnel driver
 */
static int __init ppe_tun_module_init(void)
{
	struct dentry *dir;
	ppe_acl_ret_t ret;
	struct ppe_acl_rule *rule;

	ptp = kzalloc(sizeof(struct ppe_tun_priv), GFP_ATOMIC);
	if (!ptp) {
		ppe_tun_warn("memory allocation for ptp failed");
		return -ENOMEM;
	}

	rule = kzalloc(sizeof(struct ppe_acl_rule), GFP_ATOMIC);
	if (!rule) {
		kfree(ptp);
		ppe_tun_warn("memory allocation for acl rule failed");
		return -ENOMEM;
	}

	nss_ppe_tun_minidump_log(ptp, sizeof(struct ppe_tun_priv), "ppe_tun_priv");

	/*
	 * Initialize base lock.
	 */
	spin_lock_init(&ptp->lock);

	ptp->tun_accel.ppe_tun_gretap_accel = true;
	ptp->tun_accel.ppe_tun_vxlan_accel = true;
	ptp->tun_accel.ppe_tun_ipip6_accel = true;
	ptp->tun_accel.ppe_tun_mapt_accel = true;
	ptp->tun_accel.ppe_tun_l2tp_accel = true;
	ptp->tun_accel.ppe_tun_cust_accel = true;
	ptp->tun_accel.ppe_tun_vxlan_gpe_accel = true;
	ptp->tun_accel.ppe_tun_gretun_accel = true;

	ptp->xcpn_mode.gretap = PPE_TUN_XCPN_MODE_1;
	ptp->xcpn_mode.ipip6 = PPE_TUN_XCPN_MODE_1;
	ptp->xcpn_mode.l2tp = PPE_TUN_XCPN_MODE_1;
	ptp->xcpn_mode.gretun = PPE_TUN_XCPN_MODE_1;
	ptp->xcpn_mode.vxlan = PPE_TUN_XCPN_MODE_1;

	/*
	 * Tunnel hybrid offload operations.
	 * Enabled by default
	 */
	ptp->tun_hb_info.en_gretap_hybrid_ol = true;

	atomic_set(&ptp->total_free, PPE_TUN_MAX);

	dir = debugfs_lookup("qca-nss-ppe", NULL);
	if (!dir) {
		ppe_tun_warn("%p: Unable to get parent debugfs entry", ptp);
		goto fail;
	}

	ptp->dentry = debugfs_create_dir("ppe_tun", dir);
	if (!ptp->dentry) {
		ppe_tun_warn("%p: Failed to create debugfs entry for ppe-tun", ptp);
		goto fail;
	}

	dir = debugfs_create_file("stats", S_IRUGO, ptp->dentry, NULL, &ppe_tun_stats_ops);
	if (!dir) {
		ppe_tun_warn("%p: Failed to create debugfs entry for stats", ptp);
		goto fail;
	}

	dir = debugfs_create_dir("accel_mode", ptp->dentry);
	if (!dir) {
		ppe_tun_warn("%p: Failed to create debugfs entry for accel_mode", ptp);
		goto fail;
	}

	/*
	 * Acceleration Operation Nodes
	 */
	if (!debugfs_create_file("gretap", 0644, dir, NULL, &ppe_tun_gretap_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for gretap");
	}
	if (!debugfs_create_file("vxlan", 0644, dir, NULL, &ppe_tun_vxlan_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for vxlan");
	}
	if (!debugfs_create_file("ipip6", 0644, dir, NULL, &ppe_tun_ipip6_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for ipip6");
	}
	if (!debugfs_create_file("mapt", 0644, dir, NULL, &ppe_tun_mapt_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for mapt");
	}
	if (!debugfs_create_file("l2tp", 0644, dir, NULL, &ppe_tun_l2tp_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for l2tp");
	}
	if (!debugfs_create_file("cust", 0644, dir, NULL, &ppe_tun_cust_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for custom tunnel");
	}
	if (!debugfs_create_file("vxlan-gpe", 0644, dir, NULL, &ppe_tun_vxlan_gpe_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for vxlan-gpe");
	}
	if (!debugfs_create_file("gretun", 0644, dir, NULL, &ppe_tun_gretun_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for gretun");
	}
	dir = debugfs_create_dir("xcpn_mode", ptp->dentry);
	if (!dir) {
		ppe_tun_warn("%p: Failed to create debugfs entry for xcpn_mode", ptp);
		goto fail;
	}

	/*
	 * Exception Mode Operation Nodes
	 */
	if (!debugfs_create_file("gretap", 0644, dir, NULL, &ppe_tun_gretap_xcpn_file_fops)) {
		ppe_tun_warn("Failed to create debugfs entry for gretap");
	}
	if (!debugfs_create_file("ipip6", 0644, dir, NULL, &ppe_tun_ipip6_xcpn_file_fops)) {
		ppe_tun_warn("failed to create debugfs entry for ipip6");
	}

	if (!debugfs_create_file("l2tp", 0644, dir, NULL, &ppe_tun_l2tp_xcpn_file_fops)) {
		ppe_tun_warn("failed to create debugfs entry for l2tp");
	}

	if (!debugfs_create_file("gretun", 0644, dir, NULL, &ppe_tun_gretun_xcpn_file_fops)) {
		ppe_tun_warn("failed to create debugfs entry for gretun");
	}

	if (!debugfs_create_file("vxlan", 0644, dir, NULL, &ppe_tun_vxlan_xcpn_file_fops)) {
		ppe_tun_warn("failed to create debugfs entry for vxlan");
	}

	/*
	 * Hybrid offload debug fs
	 */
	dir = debugfs_create_dir("hybrid_offload", ptp->dentry);
	if (!dir) {
		ppe_tun_warn("%p: Failed to create debugfs entry for hybrid_offload", ptp);
		goto fail;
	}

	if (!debugfs_create_file("gretap", 0644, dir, NULL, &ppe_tun_gretap_hb_offload_file_fops)) {
		ppe_tun_warn("failed to create hybrid offload debugfs ntry got gretap");
	}

	rule->cmn.cmn_flags = rule->cmn.cmn_flags | PPE_ACL_RULE_CMN_FLAG_NO_RULEID;
	rule->dev_type = PPE_ACL_RULE_DEV_TYPE_SC;
	rule->action.fwd_cmd = PPE_ACL_FWD_CMD_REDIR;
	rule->valid_flags = (1 << PPE_ACL_RULE_MATCH_TYPE_DEFAULT);
	rule->action.flags = PPE_ACL_RULE_ACTION_FLAG_FW_CMD;
	rule->dev.sc = PPE_DRV_SC_L2_TUNNEL_EXCEPTION;
	ret = ppe_acl_rule_create(rule);
	if (ret != PPE_ACL_RET_SUCCESS) {
		ppe_tun_warn("Failed to create ACL rule for VXLAN tunnels. error:%d", ret);
		goto fail;
	}

	ptp->ppe_tun_l2_tunnel_rule_id = rule->rule_id;

	kfree(rule);
	ppe_tun_info("ppe tunnel driver initialized");
	return 0;

fail:
	debugfs_remove_recursive(ptp->dentry);

	nss_ppe_tun_minidump_free(ptp, "ppe_tun_priv");
	kfree(ptp);
	kfree(rule);
	ptp = NULL;

	ppe_tun_warn("ppe tunnel driver initialization failed");
	return -EIO;
}
module_init(ppe_tun_module_init);

/*
 * ppe_tun_module_exit()
 *	module exit for ppe tunnel driver
 */
static void __exit ppe_tun_module_exit(void)
{
	/*
	 * unregister all the tunnels with netdev offload ops registered.
	 */
	ppe_tun_unreg_netdev_ol_ops_all();

	debugfs_remove_recursive(ptp->dentry);

	ppe_acl_rule_destroy(ptp->ppe_tun_l2_tunnel_rule_id);

	nss_ppe_tun_minidump_free(ptp, "ppe_tun_priv");
	kfree(ptp);
}
module_exit(ppe_tun_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("NSS PPE tunnel driver");
