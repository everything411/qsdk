/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "ppe_drv.h"
#include <fal/fal_fdb.h>
#include <fal/fal_ip.h>
#include <fal/fal_mirror.h>
#include <fal/fal_qm.h>
#ifdef NSS_PPE_PON_SUPPORT
#include <fal/fal_port_ctrl.h>
#include <fal/fal_tunnel.h>
#include <linux/netdevice.h>
#endif

/*
 * ppe_drv_dp_set_mirror_if()
 *	set ingress/egress mirror interface
 */
ppe_drv_ret_t ppe_drv_dp_set_mirror_if(struct ppe_drv_iface *iface,
		ppe_drv_dp_mirror_direction_t direction, bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *port;
	sw_error_t err;
	fal_port_t fal_port;

	spin_lock_bh(&p->lock);
	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface\n", iface);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

#if defined(NSS_PPE_IPQ96XX) || defined(NSS_PPE_IPQ52XX)
	/*
	 * Force L2_VP for phy and virtual ports.
	 */
	fal_port = ((port->type == PPE_DRV_PORT_VIRTUAL) ||
			(port->type == PPE_DRV_PORT_PHYSICAL)) ?
		FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port->port) :
		FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);
#else
	fal_port = (port->type == PPE_DRV_PORT_VIRTUAL) ?
		FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port->port) :
		FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);
#endif
	switch (direction) {
	case PPE_DRV_DP_MIRR_DI_IN:
		err = fal_mirr_port_in_set(PPE_DRV_SWITCH_ID, fal_port, enable);
		if (err != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to %s ingress mirror for port %u\n",
					(enable ? "enable" : "disable"), port->port);
			return PPE_DRV_RET_SET_MIRROR_IN_FAIL;
		}

		break;

	case PPE_DRV_DP_MIRR_DI_EG:
		err = fal_mirr_port_eg_set(PPE_DRV_SWITCH_ID, fal_port, enable);
		if (err != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to set %s egress mirror for port %u\n",
					(enable ? "enable" : "disable"), port->port);
			return PPE_DRV_RET_SET_MIRROR_EG_FAIL;
		}

		break;

	default:
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to set Mirror direction: %u direction \
				is not supported\n", direction);
		return PPE_DRV_RET_SET_MIRROR_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("Set %s %s mirror for port %u\n",
			(enable ? "enable" : "disable"),
			(direction == PPE_DRV_DP_MIRR_DI_IN ? "ingress" : "egress"),
			port->port);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dp_set_mirror_if);

/*
 * ppe_drv_dp_set_mirr_analysis_port()
 *	set ingress/egress mirror analysis interface
 */
ppe_drv_ret_t ppe_drv_dp_set_mirr_analysis_port(struct ppe_drv_iface *iface,
		ppe_drv_dp_mirror_direction_t direction, bool enable, uint8_t priority)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *port;
	sw_error_t err;
	fal_mirr_analysis_config_t analysis_cfg = {0};

	spin_lock_bh(&p->lock);
	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface\n", iface);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/* if api called for disable port then set default/invalid port */
	if (enable) {
		analysis_cfg.port_id = port->port;
	} else {
		analysis_cfg.port_id = PPE_DRV_MIRR_INVAL_PORT;
	}

	analysis_cfg.priority = priority;
	err = fal_mirr_analysis_config_set(PPE_DRV_SWITCH_ID, (fal_mirr_direction_t)direction,
			&analysis_cfg);
	if (err != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to set %s mirror analysis port for port num %u\n",
				(direction == PPE_DRV_DP_MIRR_DI_IN ? "ingress" : "egress"),
				port->port);
		return PPE_DRV_RET_SET_MIRROR_ANALYSIS_FAIL;
	}

	spin_unlock_bh(&p->lock);
	ppe_drv_info("%s mirror analysis port is set for port num %u\n",
			(direction == PPE_DRV_DP_MIRR_DI_IN ? "Ingress" : "Egress"),
			port->port);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dp_set_mirr_analysis_port);

/*
 * ppe_drv_dp_get_mirr_analysis_port()
 *	get ingress/egress mirror analysis interface
 */
ppe_drv_ret_t ppe_drv_dp_get_mirr_analysis_port(
		ppe_drv_dp_mirror_direction_t direction, uint8_t *port_num)
{
	struct ppe_drv *p = ppe_drv_gbl;
	sw_error_t err;
	fal_mirr_analysis_config_t analysis_cfg = {0};

	spin_lock_bh(&p->lock);
	err = fal_mirr_analysis_config_get(PPE_DRV_SWITCH_ID, (fal_mirr_direction_t)direction,
			&analysis_cfg);
	if (err != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to get %s mirror analysis port for port num\n",
				(direction == PPE_DRV_DP_MIRR_DI_IN ? "ingress" : "egress"));
		return PPE_DRV_RET_GET_MIRROR_ANALYSIS_FAIL;
	}

	spin_unlock_bh(&p->lock);

	/* if port number is PPE_DRV_MIRR_INVAL_PORT then return mo port set */
	if (analysis_cfg.port_id == PPE_DRV_MIRR_INVAL_PORT) {
		ppe_drv_info("No %s mirror analysis port is set\n",
				(direction == PPE_DRV_DP_MIRR_DI_IN ? "Ingress" : "Egress"));
		return PPE_DRV_RET_GET_MIRROR_ANALYSIS_NO_PORT;
	}

	*port_num = (uint8_t)analysis_cfg.port_id;
	return PPE_DRV_RET_SUCCESS;

}
EXPORT_SYMBOL(ppe_drv_dp_get_mirr_analysis_port);

/*
 * ppe_drv_dp_deinit()
 *	De-initialize a physical port.
 */
ppe_drv_ret_t ppe_drv_dp_deinit(struct ppe_drv_iface *iface)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_port *port;

	spin_lock_bh(&p->lock);
	l3_if = ppe_drv_iface_l3_if_get(iface);
	if (!l3_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get l3_if from iface\n", iface);
		return PPE_DRV_RET_L3_IF_NOT_FOUND;
	}

	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface\n", iface);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/*
	 * Detach l3_if from port
	 * Dereference l3_if
	 * Dereference port
	 */
	ppe_drv_port_l3_if_detach(port, l3_if);
	port->port_l3_if = NULL;
	ppe_drv_l3_if_deref(l3_if);

	ppe_drv_port_deref(port);

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dp_deinit);

/*
 * ppe_drv_dp_init()
 *	Initialize a physical port.
 */
ppe_drv_ret_t ppe_drv_dp_init(struct ppe_drv_iface *iface, uint32_t macid,
				bool mht_dev)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_l3_if *l3_if;
	struct ppe_drv_port *port;

	spin_lock_bh(&p->lock);
	port = ppe_drv_port_phy_alloc(macid, iface->dev);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get a valid port of type(%d)\n", iface, PPE_DRV_PORT_PHYSICAL);
		return PPE_DRV_RET_PORT_ALLOC_FAIL;
	}

	l3_if = ppe_drv_l3_if_alloc(PPE_DRV_L3_IF_TYPE_PORT);
	if (!l3_if) {
		ppe_drv_port_deref(port);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get a valid l3_if of type(%d)\n", iface, PPE_DRV_L3_IF_TYPE_PORT);
		return PPE_DRV_RET_L3_IF_ALLOC_FAIL;
	}

	/*
	 * Attach l3_if to port
	 */
	if (!ppe_drv_port_l3_if_attach(port, l3_if)) {
		ppe_drv_port_deref(port);
		ppe_drv_l3_if_deref(l3_if);
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to attach valid l3_if(%p) to port(%p)\n", iface, l3_if, port);
		return PPE_DRV_RET_L3_IF_PORT_ATTACH_FAIL;
	}
#ifdef NSS_PPE_SIXTH_TUPLE
	if (PPE_DRV_SIXTH_TUPLE_MODE_VLAN == ppe_drv_get_sixth_tuple_mode()) {
		fal_ip_vlan_as_flow_key_t flow_key;
		flow_key.valid = A_TRUE;
		flow_key.mode = FAL_IP_VLAN_AS_FLOW_KEY_FROM_OUTER;
		if(SW_OK != fal_ip_port_vlan_as_flow_key_set(PPE_DRV_SWITCH_ID, port->port, &flow_key)) {
			ppe_drv_port_l3_if_detach(port, l3_if);
			ppe_drv_l3_if_deref(l3_if);
			ppe_drv_port_deref(port);
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("failed to set VLAN_AS_FLOW_KEY for vp port:%d", port->port);
			return PPE_DRV_RET_VLAN_VPN_MODE_SET_FAIL;
		}
	}
#endif
	port->port_l3_if = l3_if;

	ppe_drv_iface_port_set(iface, port);
	ppe_drv_iface_l3_if_set(iface, l3_if);

	/*
	 * If MHT device is set, configure iface with mht flag.
	 */
	if (mht_dev)
		iface->flags |= PPE_DRV_IFACE_FLAG_MHT_SWITCH_VALID;

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dp_init);

/*
 * ppe_drv_dp_set_ppe_offload_enable_flag()
 *	API to set PPE offload enable flag in PPE port
 */
ppe_drv_ret_t ppe_drv_dp_set_ppe_offload_enable_flag(struct ppe_drv_iface *iface,
		bool disable)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *port;

	spin_lock_bh(&p->lock);
	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface\n", iface);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/*
	 * Set the OFFLOAD enabled flag in the PPE port if the input
	 * flag indicates so.
	 */
	if (!disable) {
		port->flags |= PPE_DRV_PORT_FLAG_OFFLOAD_ENABLED;
		if_bm_to_offload |= (1 << (port->port - 1));
	}

	spin_unlock_bh(&p->lock);
	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dp_set_ppe_offload_enable_flag);

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_drv_dp_gem_enable()
 *	API to set GEM type enable flag in PPE port
 *	And do enqueue vp to pq mapping.
 *	After releasing the lock, invokes the registered QoS resource
 *	allocation callback so that ppe_qos can set up PON port tcont
 *	resources.
 */
ppe_drv_ret_t ppe_drv_dp_gem_enable(struct ppe_drv_iface *iface,
		bool enable)
{
	struct ppe_drv *p = ppe_drv_gbl;
	struct ppe_drv_port *port;
	ppe_drv_ret_t status = PPE_DRV_RET_SUCCESS;
	uint32_t pon_port_id = 0;

	spin_lock_bh(&p->lock);
	port = ppe_drv_iface_port_get(iface);
	if (!port) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: unable to get port from iface\n", iface);
		return PPE_DRV_RET_PORT_NOT_FOUND;
	}

	/*
	 * Set the GEM flag in the PPE port and map enqueue VPs to PQs.
	 * Save the port ID before releasing the lock.
	 */
	if (enable) {
		port->flags |= PPE_DRV_PORT_FLAG_PORT_GEM;
		gem_port_bitmap |= (1 << (port->port));
		if (gem_port_bitmap && p->port_mgmt)
			p->port_mgmt->us_port_id = __builtin_ffs(gem_port_bitmap) - 1;
		status = ppe_drv_pon_map_enqueue_vp_to_pq(port);
		p->pon_iface = iface;
		pon_port_id = port->port;
	}

	spin_unlock_bh(&p->lock);

	/*
	 * When GEM is enabled, map multicast traffic to unicast queues on the
	 * PON port so T-CONT scheduling works correctly.
	 */
	if (enable && (status == PPE_DRV_RET_SUCCESS)) {
		sw_error_t sw_err;
		sw_err = fal_qm_mcast_enqueue_ctrl_set(PPE_DRV_SWITCH_ID, pon_port_id, A_TRUE);
		if (sw_err != SW_OK) {
			ppe_drv_warn("%p: failed to set mcast enqueue ctrl to PON port %d\n",
					iface, pon_port_id);
		}
	}

	/*
	 * Invoke the QoS resource allocation callback for the PON port
	 * outside p->lock to avoid deadlock - the callback calls
	 * ppe_drv_qos_port_res_get() which also acquires p->lock.
	 */
	if (enable && (status == PPE_DRV_RET_SUCCESS) && pon_port_id && p->pon_port_alloc_cb) {
		p->pon_port_alloc_cb(pon_port_id);
	}

	return status;
}
EXPORT_SYMBOL(ppe_drv_dp_gem_enable);

/*
 * ppe_drv_dp_pon_stats_update()
 *	Read HW counters and accumulate rollover-safe deltas into p->pon_stats_acc.
 */
void ppe_drv_dp_pon_stats_update(struct ppe_drv *p)
{
	struct ppe_drv_port *port;
	fal_port_cnt_t cur;
	uint32_t p_port;

	if (!p->pon_iface)
		return;

	port = ppe_drv_iface_port_get(p->pon_iface);
	if (!port)
		return;

	p_port = FAL_PORT_ID(FAL_PORT_TYPE_PPORT, port->port);
	if (fal_port_cnt_get(PPE_DRV_SWITCH_ID, p_port, &cur) != SW_OK) {
		ppe_drv_warn("pon_stats: fal_port_cnt_get failed for port %u\n", port->port);
		return;
	}

	p->pon_stats_acc.rx_pkt_cnt  += (cur.rx_pkt_cnt - p->pon_stats_prev.rx_pkt_cnt +
			FAL_TUNNEL_DECAP_PKT_CNT_MASK + 1) & FAL_TUNNEL_DECAP_PKT_CNT_MASK;
	p->pon_stats_acc.rx_byte_cnt += cur.rx_byte_cnt - p->pon_stats_prev.rx_byte_cnt;
	p->pon_stats_acc.tx_pkt_cnt  += (cur.tx_pkt_cnt - p->pon_stats_prev.tx_pkt_cnt +
			FAL_TUNNEL_DECAP_PKT_CNT_MASK + 1) & FAL_TUNNEL_DECAP_PKT_CNT_MASK;
	p->pon_stats_acc.tx_byte_cnt += cur.tx_byte_cnt - p->pon_stats_prev.tx_byte_cnt;
	memcpy(&p->pon_stats_prev, &cur, sizeof(fal_port_cnt_t));
}

/*
 * ppe_drv_dp_get_pon_stats()
 *	Fill stats with the 64-bit accumulated PON port counters.
 */
void ppe_drv_dp_get_pon_stats(struct ppe_drv_port_hw_stats *stats)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);
	memcpy(stats, &p->pon_stats_acc, sizeof(struct ppe_drv_port_hw_stats));
	spin_unlock_bh(&p->lock);
}
EXPORT_SYMBOL(ppe_drv_dp_get_pon_stats);

#else
ppe_drv_ret_t ppe_drv_dp_gem_enable(struct ppe_drv_iface *iface,
                bool enable)
{
	return PPE_DRV_RET_SUCCESS;
}
#endif

#ifdef NSS_PPE_DSA_QUEUE_SUPPORT
/*
 * ppe_drv_dp_htt_enable()
 *      API to set the HTT switch status on PPE port.
 */
ppe_drv_ret_t ppe_drv_dp_htt_enable(struct ppe_drv_iface *iface, bool htt_dev)
{
	struct ppe_drv *p = ppe_drv_gbl;

	spin_lock_bh(&p->lock);

	/*
	 * If HTT device is set, configure iface with htt flag.
	 */
	if (htt_dev) {
		iface->flags |= PPE_DRV_IFACE_FLAG_HTT_SWITCH_VALID;
	} else {
		iface->flags &= ~PPE_DRV_IFACE_FLAG_HTT_SWITCH_VALID;
	}

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_dp_htt_enable);
#endif
