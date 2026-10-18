/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <nss_dp_vp.h>
#include <ppe_drv_port.h>
#include <ppe_drv_sc.h>
#include "ppe_vp_base.h"
#include "ppe_vp_tx.h"
#include <linux/debug_mem_usage.h>

extern struct ppe_vp_base vp_base;

/*
 * ppe_vp_tx_to_ppe_by_sc()
 *	Process packet received from PPE VP user with caller selected service code.
 *	When it returns false, user can Rx the skb to stack.
 *	Expect edma/dp to return false.
 */
bool ppe_vp_tx_to_ppe_by_sc(int32_t vp_num, struct sk_buff *skb, ppe_drv_sc_t sc)
{

	struct ppe_vp **vpa = &vp_base.vp_table.vp_allocator[0];
	struct ppe_vp *svp;
	struct nss_dp_vp_tx_info dptxi = {};
	struct ppe_vp_tx_stats *tx_stats;
	unsigned int len = 0;

	/*
	 * Check if VP exists and forward to PPE
	 */
	rcu_read_lock();
	svp = rcu_dereference(vpa[vp_num - PPE_DRV_VIRTUAL_START]);
	if (unlikely(!svp || !(svp->flags & PPE_VP_FLAG_VP_ACTIVE))) {
		/*
		 * VP is not active, return skb to the user.
		 */
		atomic64_inc(&vp_base.base_stats.tx_vp_inactive);
		rcu_read_unlock();
		ppe_vp_info("%px: VP inactive for VP num %d, returning skb %px", svp, vp_num, skb);
		mem_debug_update_skb(skb);
		return false;
	}

	tx_stats = this_cpu_ptr(svp->vp_stats.tx_stats);
	dptxi.fake_mac = false;

	if (svp->vp_type == PPE_VP_TYPE_SW_L3) {
		dptxi.fake_mac = true;
		skb_push(skb, ETH_HLEN);
	}

	dptxi.svp = vp_num;
	dptxi.sc = sc;

	/*
	 * FSE metadata at WLAN side stores macid and relays it from WLAN driver through skb->mark
	 * If VP is of wifi type, extract macid from skb->mark and assign it to dptxi.egress_macid
	 */
	if (likely(svp->flags & PPE_VP_FLAG_EGRESS_MACID_VALID)) {
		dptxi.egress_macid = ppe_vp_tx_get_macid(skb->mark);
	}

	/*
	 * This would enable making list of skb's while freeing in edma tx complete
	 */
	if (likely(!skb_is_nonlinear(skb))) {
		skb->fast_xmit = 1;
	}

	len = skb->len;

	/*
	 * If enqueue to PPE fails, better drop else
	 * this could cause out of order packets. Hence returning
	 * status as true to the user.
	 */
	mem_debug_update_skb(skb);
	if (NETDEV_TX_OK != nss_dp_vp_xmit(vp_base.edma_vp_dev, &dptxi, skb)) {
		ppe_vp_info("Dropping skb %pxd, edma failed to enqueue to PPE, VP %d", skb, vp_num);
		u64_stats_update_begin(&tx_stats->syncp);
		tx_stats->tx_drops++;
		u64_stats_update_end(&tx_stats->syncp);
		skb->fast_xmit = 0;
		mem_debug_update_skb(skb);
		dev_kfree_skb_any(skb);
		rcu_read_unlock();
		return true;
	}

	u64_stats_update_begin(&tx_stats->syncp);
	tx_stats->tx_pkts++;
	tx_stats->tx_bytes += len;
	u64_stats_update_end(&tx_stats->syncp);

	rcu_read_unlock();
	return true;
}
EXPORT_SYMBOL(ppe_vp_tx_to_ppe_by_sc);

/*
 * ppe_vp_tx_to_ppe()
 *	Process packet received from ppe vp user.
 *	When it returns false, user can Rx the skb to stack.
 *	Expect edma/dp to return false.
 */
bool ppe_vp_tx_to_ppe(int32_t vp_num, struct sk_buff *skb)
{
	return ppe_vp_tx_to_ppe_by_sc(vp_num, skb, PPE_DRV_SC_SPF_BYPASS);
}
EXPORT_SYMBOL(ppe_vp_tx_to_ppe);

/*
 * ppe_vp_tx_to_vp()
 *	process the packets destined to VP in PPE
 *
 * skbs are consumed by this function regardless of the return
 * value.
 */
bool ppe_vp_tx_to_vp(int32_t vp_num, struct ppe_vp_tx_info *tx_info, struct sk_buff *skb)
{
	struct ppe_vp **vpa = &vp_base.vp_table.vp_allocator[0];
	struct ppe_vp *dvp;
	struct nss_dp_vp_tx_info dptxi = {};
	struct ppe_vp_rx_stats *rx_stats;
	unsigned int len = 0;

	/*
	 * Check if DVP exists and forward to PPE
	 */
	rcu_read_lock();
	dvp = rcu_dereference(vpa[vp_num - PPE_DRV_VIRTUAL_START]);
	if (unlikely(!dvp || !(dvp->flags & PPE_VP_FLAG_VP_ACTIVE))) {
		atomic64_inc(&vp_base.base_stats.rx_dvp_inactive);
		rcu_read_unlock();

		ppe_vp_info("%px: Dest VP %d inactive, dropping skb %px", dvp, vp_num, skb);
		skb->fast_xmit = 0;
		mem_debug_update_skb(skb);
		dev_kfree_skb_any(skb);
		return false;
	}

	rx_stats = this_cpu_ptr(dvp->vp_stats.rx_stats);
	dptxi.fake_mac = false;

	/*
	 * L3 VPs and HW L3 tunnels do not carry an Ethernet header, so prepend a fake one for PPE.
	 */
	if (dvp->vp_type == PPE_VP_TYPE_SW_L3 || dvp->vp_type == PPE_VP_TYPE_HW_L3TUN) {
		dptxi.fake_mac = true;
		skb_push(skb, ETH_HLEN);
	}

	dptxi.dvp = vp_num;
	dptxi.svp = 0;
	dptxi.sc = PPE_DRV_SC_FMAC_BYPASS;

	switch (tx_info->type) {
	case PPE_VP_TX_TYPE_IPSEC_ENCAP:
		/*
		 * Send IPsec packet to EIP for encapsulation.
		 */
		dptxi.flags |= NSS_DP_VP_TX_FLAG_PKT_MODE;
		dptxi.pkt_mode = NSS_DP_VP_FULL_PKT_MODE;
		dptxi.sc = PPE_DRV_SC_IPSEC_EDMA_VP_ENCAP;
		break;

	case PPE_VP_TX_TYPE_IPSEC_DECAP:
		/*
		 * Send IPsec packet to EIP for decapsulation.
		 */
		dptxi.flags |= NSS_DP_VP_TX_FLAG_PKT_MODE;
		dptxi.pkt_mode = NSS_DP_VP_FULL_PKT_MODE;
		dptxi.sc = PPE_DRV_SC_IPSEC_EDMA_VP_DECAP;
		break;

	case PPE_VP_TX_TYPE_IPSEC_3_TUPLE_ENCAP:
		/*
		 * Send IPsec packet to EIP for encapsulation using EIP's 3 tuple lookup.
		 */
		dptxi.flags |= NSS_DP_VP_TX_FLAG_PKT_MODE;
		dptxi.pkt_mode = NSS_DP_VP_FULL_PKT_MODE;
		dptxi.sc = PPE_DRV_SC_IPSEC_EDMA_VP_3_TUPLE_ENCAP;
		break;

	default:
		break;
	}

	len = skb->len;

	/*
	 * If enqueue to PPE fails, drop the packet.
	 */
	mem_debug_update_skb(skb);
	if (NETDEV_TX_OK != nss_dp_vp_xmit(vp_base.edma_vp_dev, &dptxi, skb)) {
		ppe_vp_info("Dropping skb %pxd, edma failed to enqueue to VP %d", skb, vp_num);
		u64_stats_update_begin(&rx_stats->syncp);
		rx_stats->rx_drops++;
		u64_stats_update_end(&rx_stats->syncp);
		skb->fast_xmit = 0;
		mem_debug_update_skb(skb);
		dev_kfree_skb_any(skb);
		rcu_read_unlock();
		return false;
	}

	u64_stats_update_begin(&rx_stats->syncp);
	rx_stats->rx_pkts++;
	rx_stats->rx_bytes += len;
	u64_stats_update_end(&rx_stats->syncp);

	rcu_read_unlock();
	return true;
}
EXPORT_SYMBOL(ppe_vp_tx_to_vp);

/*
 * ppe_vp_tx_to_ppe_by_dev()
 *	Wrapper API to transmit the packet via PPE-VP when VP client
 *	has the context of netdevice but not the actual VP number.
 *	This API gets the VP from dev and calls the actual VP TX API.
 *	Ex: ATH driver VP TX
 */
bool ppe_vp_tx_to_ppe_by_dev(struct net_device *dev, struct sk_buff *skb)
{
	int32_t vp_num;

	if (unlikely(!dev)) {
		ppe_vp_warn(" Netdev is NULL in VP TX !");
		return false;
	}

	vp_num = ppe_drv_port_num_from_dev(dev);
	if (unlikely((vp_num < PPE_DRV_VIRTUAL_START) || (vp_num >= PPE_DRV_VIRTUAL_END))) {
		ppe_vp_warn("Not a valid Virtual Port number %d dev %s", vp_num, dev->name);
		return false;
	}

	/*
	 * skb->mark contains egress_macid.
	 * Amend skb->mark with valid macid tag.
	 * Note: Presently ppe_vp_tx_to_ppe_by_dev is called from wifi only.
	 * If this api is called for any other vp type in future, then
	 * vp type check is required to set macid tag for wifi type vp only.
	 */
	skb->mark = ppe_vp_tx_set_macid_tag(skb->mark);
	mem_debug_update_skb(skb);
	return ppe_vp_tx_to_ppe(vp_num, skb);
}
EXPORT_SYMBOL(ppe_vp_tx_to_ppe_by_dev);
