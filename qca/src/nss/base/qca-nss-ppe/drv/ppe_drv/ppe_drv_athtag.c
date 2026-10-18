/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/etherdevice.h>
#include <net/dsa.h>
#include <fal/fal_api.h>
#include <fal/fal_athtag.h>
#include "ppe_drv.h"
#include "ppe_drv_stats.h"

/*
 * ppe_drv_athtag_del_vp_mapping()
 *  Remove ppe ath mapping config
 */
ppe_drv_ret_t ppe_drv_athtag_del_vp_mapping(struct ppe_drv_iface *iface, unsigned int swpt_id)
{
	fal_athtag_port_mapping_t port_map = {0};
	fal_athtag_tx_cfg_t ppe_tx_cfg = {.athtag_type = 0};
	fal_athtag_rx_cfg_t ppe_rx_cfg = {.athtag_type = 0};
	uint32_t ppe_vp = 0, ppe_pp = 0;
	struct ppe_drv_iface *base_if = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	int ret;

	struct net_device *base_dev = dsa_port_to_master(dsa_port_from_netdev(iface->dev));

	spin_lock_bh(&p->lock);

	base_if = ppe_drv_iface_get_by_dev_internal(base_dev);
	if (!base_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: NULL base interface for iface\n", base_dev);
		return PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
	}

	ppe_vp = ppe_drv_iface_port_idx_get(iface);
	ppe_pp = ppe_drv_iface_port_idx_get(base_if);
	ppe_drv_iface_base_clear(iface);

	ppe_drv_trace("deleting ath cfg for ppe_vp: %u, ppe_pp: %u, swpt_id:%u.\n", ppe_vp, ppe_pp, swpt_id);

	/* set ATH mapping config */
	port_map.ath_port = swpt_id;
	port_map.int_port = 0;
	ret = fal_athtag_port_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_BOTH, &port_map);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to add athtag mapping for port %d, error: %d\n", swpt_id, ret);
		return PPE_DRV_RET_ATHTAG_MAP_ADD_FAIL;
	}

	port_map.int_port = ppe_vp;
	port_map.ath_port = 0;
	ret = fal_athtag_port_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_BOTH, &port_map);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to add athtag mapping for port %d, error: %d\n", swpt_id, ret);
		return PPE_DRV_RET_ATHTAG_MAP_ADD_FAIL;
	}

	/* set ATH RX config on master netdev's port */
	ret = fal_port_athtag_rx_get(PPE_DRV_SWITCH_ID, ppe_pp, &ppe_rx_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to get rx athtag config for port %d, error: %d\n", ppe_pp, ret);
		return PPE_DRV_RET_ATHTAG_PORT_RX_CFG_FAIL;
	}

	if (ppe_rx_cfg.athtag_en == A_TRUE && ppe_rx_cfg.athtag_type == MHT_ATHTAG_TYPE) {
		ppe_rx_cfg.athtag_en = A_FALSE;
		ppe_rx_cfg.athtag_type = 0;
		ret = fal_port_athtag_rx_set(PPE_DRV_SWITCH_ID, ppe_pp, &ppe_rx_cfg);
		if (ret != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to set rx athtag config for port %d, error: %d\n", ppe_pp, ret);
			return PPE_DRV_RET_ATHTAG_PORT_RX_CFG_FAIL;
		}
	}

	/* set ATH TX hdr config on virtual port */
	ppe_tx_cfg.athtag_en = A_FALSE;
	ret = fal_port_athtag_tx_set(PPE_DRV_SWITCH_ID, ppe_vp, &ppe_tx_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to add athtag tx config for port %d, error: %d\n", ppe_vp, ret);
		return PPE_DRV_RET_ATHTAG_PORT_TX_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_athtag_del_vp_mapping);

/*
 * ppe_drv_athtag_add_vp_mapping()
 *  Config ppe ath mapping table
 */
ppe_drv_ret_t ppe_drv_athtag_add_vp_mapping(struct ppe_drv_iface *iface, unsigned int swpt_id)
{
	fal_athtag_port_mapping_t port_map = {0};
	fal_athtag_tx_cfg_t ppe_tx_cfg = {.athtag_type = 0};
	fal_athtag_rx_cfg_t ppe_rx_cfg = {.athtag_type = 0};
	uint32_t ppe_vp = 0, ppe_pp = 0;
	struct ppe_drv_iface *base_if = NULL;
	struct ppe_drv *p = ppe_drv_gbl;
	int ret;

	struct net_device *base_dev = dsa_port_to_master(dsa_port_from_netdev(iface->dev));

	spin_lock_bh(&p->lock);

	base_if = ppe_drv_iface_get_by_dev_internal(base_dev);
	if (!base_if) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("%p: NULL base interface for iface\n", base_dev);
		return PPE_DRV_RET_BASE_IFACE_NOT_FOUND;
	}

	ppe_vp = ppe_drv_iface_port_idx_get(iface);
	ppe_pp = ppe_drv_iface_port_idx_get(base_if);
	ppe_drv_iface_base_set(iface, base_if);

	ppe_drv_trace("adding ath cfg for ppe_vp: %u, ppe_pp: %u, swpt_id:%u.\n", ppe_vp, ppe_pp, swpt_id);

	/* set ATH mapping config */
	SW_PBMP_ADD_PORT(port_map.ath_port, swpt_id);
	port_map.int_port = FAL_PORT_ID(FAL_PORT_TYPE_VPORT, ppe_vp);
	ret = fal_athtag_port_mapping_set(PPE_DRV_SWITCH_ID, FAL_DIR_BOTH, &port_map);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to add athtag mapping for port %d, error: %d\n", swpt_id, ret);
		return PPE_DRV_RET_ATHTAG_MAP_ADD_FAIL;
	}

	/* set ATH RX config on master netdev's port */
	ret = fal_port_athtag_rx_get(PPE_DRV_SWITCH_ID, ppe_pp, &ppe_rx_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to get rx athtag config for port %d, error: %d\n", ppe_pp, ret);
		return PPE_DRV_RET_ATHTAG_PORT_RX_CFG_FAIL;
	}

	if (ppe_rx_cfg.athtag_en != A_TRUE || ppe_rx_cfg.athtag_type != MHT_ATHTAG_TYPE) {
		ppe_rx_cfg.athtag_en = A_TRUE;
		ppe_rx_cfg.athtag_type = MHT_ATHTAG_TYPE;
		ret = fal_port_athtag_rx_set(PPE_DRV_SWITCH_ID, ppe_pp, &ppe_rx_cfg);
		if (ret != SW_OK) {
			spin_unlock_bh(&p->lock);
			ppe_drv_warn("Failed to set rx athtag config for port %d, error: %d\n", ppe_pp, ret);
			return PPE_DRV_RET_ATHTAG_PORT_RX_CFG_FAIL;
		}
	}

	/* set ATH TX hdr config on virtual port */
	ppe_tx_cfg.athtag_en = A_TRUE;
	ppe_tx_cfg.athtag_type = MHT_ATHTAG_TYPE;
	ppe_tx_cfg.version = FAL_ATHTAG_VER3;
	ppe_tx_cfg.action = FAL_ATHTAG_ACTION_NORMAL;
	ppe_tx_cfg.bypass_fwd_en = A_TRUE;
	ppe_tx_cfg.field_disable = A_FALSE;
	ret = fal_port_athtag_tx_set(PPE_DRV_SWITCH_ID, ppe_vp, &ppe_tx_cfg);
	if (ret != SW_OK) {
		spin_unlock_bh(&p->lock);
		ppe_drv_warn("Failed to add athtag tx config for port %d, error: %d\n", ppe_vp, ret);
		return PPE_DRV_RET_ATHTAG_PORT_TX_CFG_FAIL;
	}

	spin_unlock_bh(&p->lock);

	return PPE_DRV_RET_SUCCESS;
}
EXPORT_SYMBOL(ppe_drv_athtag_add_vp_mapping);
