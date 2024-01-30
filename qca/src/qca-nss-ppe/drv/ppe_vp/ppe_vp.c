/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <net/sch_generic.h>
#include "ppe_vp_base.h"
#include "ppe_vp_rx.h"

extern struct ppe_vp_base vp_base;

/*
 * ppe_vp_get_netdev_by_port_num()
 *	Get netdevice form port number.
 */
struct net_device *ppe_vp_get_netdev_by_port_num(ppe_vp_num_t port_num)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_vp *vp;
	struct net_device *netdev = NULL;

	rcu_read_lock();
	vp = ppe_vp_base_get_vp_by_port_num(port_num);
	if (!vp) {
		rcu_read_unlock();
		ppe_vp_warn("%px: VP already freed, cannot get VP for port num %d", pvb, port_num);
		return NULL;
	}

	netdev = vp->netdev;
	rcu_read_unlock();

	return netdev;
}
EXPORT_SYMBOL(ppe_vp_get_netdev_by_port_num);

/*
 * ppe_vp_mac_addr_clear()
 *	Clear the MAC address for the virtual port.
 */
ppe_vp_status_t ppe_vp_mac_addr_clear(ppe_vp_num_t port_num)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_drv_iface *ppe_iface;
	struct ppe_vp *vp;
	ppe_drv_ret_t ret;
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;

	rcu_read_lock();
	vp = ppe_vp_base_get_vp_by_port_num(port_num);
	if (!vp) {
		ppe_vp_warn("%px: VP already freed, cannot get VP for port num %d", pvb, port_num);
		status = PPE_VP_STATUS_GET_VP_FAIL;
		goto mac_clear_fail;
	}

	ppe_iface = vp->ppe_iface;
	ret = ppe_drv_iface_mac_addr_clear(ppe_iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP mac clear failed, Err code %d", pvb, vp->netdev, ppe_iface, ret);
		status = PPE_VP_STATUS_MAC_CLEAR_FAIL;
		goto mac_clear_fail;
	}

	ppe_vp_info("%px: netdev: %px, vp %px at port num %u, MAC cleared", pvb, vp->netdev, vp, port_num);

mac_clear_fail:
	rcu_read_unlock();
	return status;
}
EXPORT_SYMBOL(ppe_vp_mac_addr_clear);

/*
 * ppe_vp_mac_addr_set()
 *	Set MAC address for the virtual port.
 */
ppe_vp_status_t ppe_vp_mac_addr_set(ppe_vp_num_t port_num, uint8_t *mac_addr)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_drv_iface *ppe_iface;
	struct ppe_vp *vp;
	ppe_drv_ret_t ret;
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;

	rcu_read_lock();
	vp = ppe_vp_base_get_vp_by_port_num(port_num);
	if (!vp) {
		ppe_vp_warn("%px: VP already freed, cannot get VP for port num %d", pvb, port_num);
		status = PPE_VP_STATUS_GET_VP_FAIL;
		goto mac_set_fail;
	}

	ppe_iface = vp->ppe_iface;
	ret = ppe_drv_iface_mac_addr_set(ppe_iface, mac_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP MAC set to %pM failed, Err code %d", pvb, vp->netdev, ppe_iface, mac_addr, ret);
		status = PPE_VP_STATUS_MAC_SET_FAIL;
		goto mac_set_fail;
	}

	ppe_vp_info("%px: netdev: %px, vp %px at port num %u, MAC changed to %pM", pvb, vp->netdev, vp, port_num, mac_addr);

mac_set_fail:
	rcu_read_unlock();
	return status;
}
EXPORT_SYMBOL(ppe_vp_mac_addr_set);

/*
 * ppe_vp_mtu_get()
 *	Get MTU for the virtual port.
 */
ppe_vp_status_t ppe_vp_mtu_get(ppe_vp_num_t port_num, uint16_t *mtu)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_vp *vp;
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;

	rcu_read_lock();
	vp = ppe_vp_base_get_vp_by_port_num(port_num);
	if (!vp) {
		ppe_vp_warn("%px: VP already freed, cannot get VP for port num %d", pvb, port_num);
		status = PPE_VP_STATUS_GET_VP_FAIL;
		goto mtu_get_fail;
	}

	*mtu = vp->mtu;

mtu_get_fail:
	rcu_read_unlock();
	return status;
}
EXPORT_SYMBOL(ppe_vp_mtu_get);

/*
 * ppe_vp_mtu_set()
 *	Set MTU for the virtual port.
 */
ppe_vp_status_t ppe_vp_mtu_set(ppe_vp_num_t port_num, uint16_t mtu)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_drv_iface *ppe_iface;
	struct ppe_vp *vp;
	ppe_drv_ret_t ret;
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;

	rcu_read_lock();
	vp = ppe_vp_base_get_vp_by_port_num(port_num);
	if (!vp) {
		ppe_vp_warn("%px: VP already freed, cannot get VP for port num %d", pvb, port_num);
		status = PPE_VP_STATUS_GET_VP_FAIL;
		goto mtu_set_fail;
	}

	ppe_iface = vp->ppe_iface;
	ret = ppe_drv_iface_mtu_set(ppe_iface, mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP mtu set to %u failed, Err code %d", pvb, vp->netdev, ppe_iface, mtu, ret);
		status = PPE_VP_STATUS_MTU_SET_FAIL;
		goto mtu_set_fail;
	}

	vp->mtu = mtu;
	ppe_vp_info("%px: netdev: %px, vp %px at port num %u, MTU changed to %u", pvb, vp->netdev, vp, port_num, mtu);

mtu_set_fail:
	rcu_read_unlock();
	return status;
}
EXPORT_SYMBOL(ppe_vp_mtu_set);

/*
 * ppe_vp_free()
 *	Free the earlier allocated VP.
 */
ppe_vp_status_t ppe_vp_free(ppe_vp_num_t port_num)
{
	struct ppe_vp_base *pvb = &vp_base;
	struct ppe_drv_iface *ppe_iface;
	struct net_device *netdev;
	struct ppe_vp *vp;
	ppe_vp_stats_callback_t stats_cb;
	ppe_vp_hw_stats_t hw_stats;
	ppe_drv_ret_t ret;
	ppe_vp_status_t status = PPE_VP_STATUS_SUCCESS;

	rcu_read_lock();
	vp = ppe_vp_base_get_vp_by_port_num(port_num);
	if (!vp) {
		ppe_vp_warn("%px: VP already freed, cannot get VP for port num %d", pvb, port_num);
		status = PPE_VP_STATUS_GET_VP_FAIL;
		rcu_read_unlock();
		goto free_fail;
	}

	ppe_iface = vp->ppe_iface;

	/*
	 * Map VP to PPE queue zero
	 */
	ret = ppe_drv_iface_ucast_queue_set(ppe_iface, 0);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: port_num: %x, ppe vp ucast queue reset failed", pvb, port_num);
		status = PPE_VP_STATUS_VP_QUEUE_SET_FAILED;
		rcu_read_unlock();
		goto free_fail;
	}

	/*
	 * De-Initialize the virtual port in PPE.
	 */
	ret = ppe_drv_vp_deinit(ppe_iface);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP deinit failed, Err code %d", pvb, vp->netdev, ppe_iface, ret);
		status = PPE_VP_STATUS_VP_DEINIT_FAIL;
		rcu_read_unlock();
		goto free_fail;
	}

	ppe_drv_iface_deref(ppe_iface);

	spin_lock_bh(&vp->lock);
	vp->ppe_iface = NULL;
	vp->port_num = -1;
	vp->mtu = 0;
	vp->flags &= ~PPE_VP_FLAG_VP_ACTIVE;
	vp->dst_cb = NULL;
	vp->src_cb = NULL;
	stats_cb = vp->stats_cb;
	vp->stats_cb = NULL;
	netdev = vp->netdev;
	vp->netdev_if_num = 0;
	vp->netdev = NULL;

	/*
	 * Clear out the stats for this VP. Also free out the per cpu stats.
	 */
	memcpy(&hw_stats, &vp->vp_stats.vp_hw_stats, sizeof(ppe_vp_hw_stats_t));

	/*
	 * Clear the stats for this VP. We can only clear non-per CPU stats.
	 * Also clear the per cpu stats.
	 */
	ppe_vp_stats_reset_vp_stats(&vp->vp_stats);

	vp->pvb = NULL;
	spin_unlock_bh(&vp->lock);

	if (stats_cb) {
		stats_cb(netdev, &hw_stats);
	}

	dev_put(netdev);
	rcu_read_unlock();

	/*
	 * Free the VP.
	 * Returning from this status of failure should not lead to recalling of this API.
	 * The VP is already freed.
	 */
	if (!ppe_vp_base_free_vp(port_num)) {
		ppe_vp_warn("%px: Unable to free VP for port %u", pvb, port_num);
		status = PPE_VP_STATUS_VP_FREE_FAIL;
		dev_hold(netdev);
	}

free_fail:
	return status;
}
EXPORT_SYMBOL(ppe_vp_free);

/*
 * ppe_vp_alloc()
 *	Allocate a new virtual port.
 */
ppe_vp_num_t ppe_vp_alloc(struct net_device *netdev, struct ppe_vp_ai *vpai)
{
	struct ppe_vp_base *pvb = &vp_base;
	enum ppe_vp_type type = vpai->type;
	struct ppe_vp *vp;
	struct ppe_drv_iface *ppe_iface;
	enum ppe_drv_iface_type ppe_type;
	int32_t pp_num;
	ppe_drv_ret_t ret;

	if (!netdev) {
		ppe_vp_warn("%px: Netdev is NULL", pvb);
		vpai->status = PPE_VP_STATUS_FAILURE;
		return -1;
	}

	if (type < PPE_VP_TYPE_SW_L2 || type >= PPE_VP_TYPE_MAX) {
		ppe_vp_warn("%px: Invalid PPE VP type %d requested", pvb, type);
		vpai->status = PPE_VP_STATUS_FAILURE;
		return -1;
	}

	switch (type) {
		case PPE_VP_TYPE_SW_L2:
		case PPE_VP_TYPE_SW_L3:
			ppe_type = PPE_DRV_IFACE_TYPE_VIRTUAL;
			break;

		case PPE_VP_TYPE_SW_PO:
			ppe_type = PPE_DRV_IFACE_TYPE_VIRTUAL_PO;
			break;

		case PPE_VP_TYPE_HW_L2TUN:
			ppe_type = PPE_DRV_IFACE_TYPE_VP_L2_TUN;
			break;

		case PPE_VP_TYPE_HW_L3TUN:
			ppe_type = PPE_DRV_IFACE_TYPE_VP_L3_TUN;
			break;

		default:
			ppe_vp_warn("%p: Incorrect interface type: %d", pvb, type);
			vpai->status = PPE_VP_STATUS_INVALID_TYPE;
			return -1;
	}

	/*
	 * Allocate a new PPE interface.
	 */
	ppe_iface = ppe_drv_iface_alloc(ppe_type, netdev);
	if (!ppe_iface) {
		ppe_vp_warn("%px: netdev: %px, ppe type %d, ppe interface allocation fail", pvb, netdev, ppe_type);
		vpai->status = PPE_VP_STATUS_PPEIFACE_ALLOC_FAIL;
		return -1;
	}

	/*
	 * Initialize the virtual port in PPE.
	 */
	ret = ppe_drv_vp_init(ppe_iface, vpai->core_mask, vpai->usr_type, vpai->net_dev_type);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP initialization failed, Err code %d", pvb, netdev, ppe_iface, ret);
		vpai->status = PPE_VP_STATUS_VP_INIT_FAIL;
		goto init_fail;
	}

	pp_num = ppe_drv_port_num_from_dev(netdev);
	if (pp_num == -1) {
		ppe_vp_warn("%px: received invalid port num %d for netdev %s, ppe_iface %px", pvb, pp_num, netdev->name, ppe_iface);
		vpai->status = PPE_VP_STATUS_PORT_INVALID;
		goto pp_num_fail;
	}

	ret = ppe_drv_iface_mac_addr_set(ppe_iface, netdev->dev_addr);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP MAC set to %pM failed, Err code %d", pvb, netdev, ppe_iface, netdev->dev_addr, ret);
		vpai->status = PPE_VP_STATUS_MAC_SET_FAIL;
		goto pp_num_fail;
	}

	ret = ppe_drv_iface_mtu_set(ppe_iface, netdev->mtu);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe iface %px PPE VP mtu set to %u failed, Err code %d", pvb, netdev, ppe_iface, netdev->mtu, ret);
		vpai->status = PPE_VP_STATUS_MTU_SET_FAIL;
		goto mtu_set_failed;
	}

	ret = ppe_drv_iface_ucast_queue_set(ppe_iface, vpai->queue_num);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_vp_warn("%px: netdev: %px, ppe vp ucast queue %d set failed", pvb, netdev, vpai->queue_num);
		goto alloc_fail;
	}

	/*
	 * Clear ppe hardware stats for the vp before using it.
	 */
	if (!ppe_drv_port_clear_hw_vp_stats(pp_num)) {
	       ppe_vp_warn("%px: port_num: %x, ppe vp port reset hw stats failed", pvb, pp_num);
	       vpai->status = PPE_VP_STATUS_HW_VP_STATS_CLEAR_FAILED;
	       goto alloc_fail;
	}

	ppe_vp_trace("%px: netdev: %px, ppe vp %d ucase queue %d set", pvb, netdev, pp_num, vpai->queue_num);

	/*
	 * Get the main vp object from local list.
	 */
	vp = ppe_vp_base_alloc_vp((uint8_t)pp_num);
	if (!vp) {
		ppe_vp_warn("%px: Unable to allocate a new VP for ppe port %d", pvb, pp_num);
		vpai->status = PPE_VP_STATUS_VP_ALLOC_FAIL;
		goto alloc_fail;
	}

	spin_lock_bh(&vp->lock);
	vp->pvb = pvb;
	vp->netdev = netdev;
	dev_hold(netdev);
	vp->ppe_iface = ppe_iface;
	vp->netdev_if_num = netdev->ifindex;
	vp->vp_type = type;
	vp->port_num = pp_num;
	vp->mtu = netdev->mtu;
	vp->flags = PPE_VP_FLAG_VP_ACTIVE;
	if (vpai->src_cb) {
		vp->src_cb = vpai->src_cb;
		vp->src_cb_data = vpai->src_cb_data;
	} else {
		vp->src_cb = ppe_vp_rx_process_cb;
	}
	vp->stats_cb = vpai->stats_cb;

	vp->vp_stats.misc_info.netdev_if_num = netdev->ifindex;
	vp->vp_stats.misc_info.ppe_port_num = pp_num;

	/*
	 * Check if the virtual port device has registered a callback
	 * to use. If not, we try to fast-transmit
	 */
	if (vpai->dst_cb) {
		vp->dst_cb = vpai->dst_cb;
		vp->dst_cb_data = vpai->dst_cb_data;
	} else {
		/*
		 * TODO: To be dynamically toggled when qdisc is enabled
		 * on the interface.
		 */
		vp->flags |= PPE_VP_FLAG_VP_FAST_XMIT;
	}

	spin_unlock_bh(&vp->lock);

	return pp_num;

alloc_fail:
	ppe_drv_iface_mtu_set(ppe_iface, 0);

mtu_set_failed:
	ppe_drv_iface_mac_addr_clear(ppe_iface);

pp_num_fail:
	ppe_drv_vp_deinit(ppe_iface);

init_fail:
	ppe_drv_iface_deref(ppe_iface);
	return -1;
}
EXPORT_SYMBOL(ppe_vp_alloc);
