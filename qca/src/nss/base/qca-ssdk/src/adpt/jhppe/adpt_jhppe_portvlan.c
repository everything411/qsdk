/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#include "hsl_reg.h"
#include "adpt.h"
#include "fal_portvlan.h"

sw_error_t
adpt_jhppe_vlan_trans_dscp_pcp_mapping_set(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp)
{
	sw_error_t rv = SW_OK;

	if (direction == FAL_PORT_VLAN_INGRESS || direction == FAL_PORT_VLAN_ALL) {
		switch(group_id) {
		case 0:
			rv = jhppe_in_vlan_dscp_pbit_map_tbl_pcp_0_set(dev_id, dscp, pcp);
			break;
		case 1:
			rv = jhppe_in_vlan_dscp_pbit_map_tbl_pcp_1_set(dev_id, dscp, pcp);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	}

	SW_RTN_ON_ERROR(rv);

	if (direction == FAL_PORT_VLAN_EGRESS || direction == FAL_PORT_VLAN_ALL) {
		switch(group_id) {
		case 0:
			rv = jhppe_dscp_pbit_map_tbl_pcp_0_set(dev_id, dscp, pcp);
			break;
		case 1:
			rv = jhppe_dscp_pbit_map_tbl_pcp_1_set(dev_id, dscp, pcp);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	}

	return rv;
}

sw_error_t
adpt_jhppe_vlan_trans_dscp_pcp_mapping_get(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp)
{
	a_uint32_t val = 0;
	sw_error_t rv;

	if (direction == FAL_PORT_VLAN_INGRESS) {
		switch(group_id) {
		case 0:
			rv = jhppe_in_vlan_dscp_pbit_map_tbl_pcp_0_get(dev_id, dscp, &val);
			break;
		case 1:
			rv = jhppe_in_vlan_dscp_pbit_map_tbl_pcp_1_get(dev_id, dscp, &val);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	} else if (direction == FAL_PORT_VLAN_EGRESS) {
		switch (group_id) {
		case 0:
			rv = jhppe_dscp_pbit_map_tbl_pcp_0_get(dev_id, dscp, &val);
			break;
		case 1:
			rv = jhppe_dscp_pbit_map_tbl_pcp_1_get(dev_id, dscp, &val);
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			break;
		}
	} else {
		rv = SW_NOT_SUPPORTED;
	}

	*pcp = val;
	return rv;
}

sw_error_t
adpt_jhppe_private_vlan_rx_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_ctrl_u rx_ctrl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);
	a_uint32_t index = port_value - 1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	if (port_value == SSDK_PHYSICAL_PORT0 || port_value > SSDK_MAX_PORT_NUM) {
		return SW_BAD_PARAM;
	}

	aos_mem_zero(&rx_ctrl, sizeof(union vlan_to_port_mapping_ctrl_u));

	rv = jhppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &rx_ctrl);
	SW_RTN_ON_ERROR(rv);

	rx_ctrl.bf.private_tag_en = cfg->map_en;
	rx_ctrl.bf.tpid = cfg->tpid;

	rv = jhppe_vlan_to_port_mapping_ctrl_set(dev_id, index, &rx_ctrl);

	return rv;
}

sw_error_t
adpt_jhppe_private_vlan_rx_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_ctrl_u rx_ctrl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);
	a_uint32_t index = port_value - 1;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	if (port_value == SSDK_PHYSICAL_PORT0 || port_value > SSDK_MAX_PORT_NUM) {
		return SW_BAD_PARAM;
	}

	aos_mem_zero(&rx_ctrl, sizeof(union vlan_to_port_mapping_ctrl_u));

	rv = jhppe_vlan_to_port_mapping_ctrl_get(dev_id, index, &rx_ctrl);
	SW_RTN_ON_ERROR(rv);

	cfg->map_en = rx_ctrl.bf.private_tag_en;
	cfg->tpid = rx_ctrl.bf.tpid;

	return SW_OK;
}

static sw_error_t adpt_jhppe_vlan_2port_map_tbl_find(a_uint32_t dev_id,
		fal_pvtvlan_map_t *port_mapping, a_uint32_t *index)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_tbl_u vlan_2port;
	a_uint32_t i;

	for (i = 0; i < VLAN_TO_PORT_MAPPING_TBL_NUM; i++) {
		rv = jhppe_vlan_to_port_mapping_tbl_get(dev_id, i, &vlan_2port);
		SW_RTN_ON_ERROR(rv);

		if (!vlan_2port.bf.valid)
			continue;

		if (port_mapping->vlan_tci == vlan_2port.bf.tci_value &&
			port_mapping->vlan_tci_mask == vlan_2port.bf.tci_mask) {
			*index = i;
			return SW_OK;
		}
	}

	return SW_NOT_FOUND;
}

static sw_error_t adpt_jhppe_vlan_2port_map_tbl_insert(a_uint32_t dev_id,
		fal_pvtvlan_map_t *port_mapping, a_uint32_t *index)
{
	sw_error_t rv = SW_OK;
	union vlan_to_port_mapping_tbl_u vlan_2port;
	a_uint32_t i;

	for (i = 0; i < VLAN_TO_PORT_MAPPING_TBL_NUM; i++) {
		rv = jhppe_vlan_to_port_mapping_tbl_get(dev_id, i, &vlan_2port);
		SW_RTN_ON_ERROR(rv);

		if (vlan_2port.bf.valid)
			continue;

		vlan_2port.bf.tci_value = port_mapping->vlan_tci;
		vlan_2port.bf.tci_mask = port_mapping->vlan_tci_mask;
		vlan_2port.bf.port_vp = port_mapping->int_port;
		vlan_2port.bf.valid = A_TRUE;
		*index = i;

		rv = jhppe_vlan_to_port_mapping_tbl_set(dev_id, i, &vlan_2port);
		return rv;
	}

	return SW_NO_RESOURCE;
}

sw_error_t
adpt_jhppe_private_vlan_mapping_set(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping)
{
	sw_error_t rv = SW_OK;
	a_bool_t need_rollback = A_FALSE;
	a_uint32_t ingress_index = 0;
	a_bool_t ingress_inserted = A_FALSE;
	union vlan_to_port_mapping_tbl_u ingress_backup;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_mapping);

	if (direction == FAL_DIR_INGRESS || direction == FAL_DIR_BOTH) {
		union vlan_to_port_mapping_tbl_u vlan_2port;
		a_uint32_t index;

		rv = adpt_jhppe_vlan_2port_map_tbl_find(dev_id, port_mapping, &index);

		if (rv == SW_OK) {
			/* backup original ingress cfg/idx for fail rollback in in/eg cfg both configured case */
			if (direction == FAL_DIR_BOTH) {
				rv = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &ingress_backup);
				SW_RTN_ON_ERROR(rv);

				ingress_index = index;
				need_rollback = A_TRUE;
			}

			if (port_mapping->ptmap_en) {
				vlan_2port.bf.tci_value = port_mapping->vlan_tci;
				vlan_2port.bf.tci_mask = port_mapping->vlan_tci_mask;
				vlan_2port.bf.port_vp = port_mapping->int_port;
				vlan_2port.bf.valid = A_TRUE;
			} else {
				aos_mem_zero(&vlan_2port, sizeof(union vlan_to_port_mapping_tbl_u));
			}

			rv = jhppe_vlan_to_port_mapping_tbl_set(dev_id, index, &vlan_2port);
			SW_RTN_ON_ERROR(rv);
		} else {
			if (port_mapping->ptmap_en) {
				rv = adpt_jhppe_vlan_2port_map_tbl_insert(dev_id, port_mapping, &index);
				SW_RTN_ON_ERROR(rv);

				/* recored new entry indx for fail rollback in in/eg cfg both configured case */
				if (direction == FAL_DIR_BOTH) {
					ingress_inserted = A_TRUE;
					ingress_index = index;
					need_rollback = A_TRUE;
				}
			} else {
				return SW_NOT_FOUND;
			}
		}
	}

	if (direction == FAL_DIR_EGRESS || direction == FAL_DIR_BOTH) {
		union eg_vp_tbl_u port_2vlan;

		rv = appe_egress_vp_tbl_get(dev_id, port_mapping->int_port, &port_2vlan);
		if (rv != SW_OK)
			goto rollback;

		if (port_mapping->ptmap_en) {
			port_2vlan.bf.private_tag_en = A_TRUE;
			port_2vlan.bf.private_tag_tci = port_mapping->vlan_tci;
		} else {
			port_2vlan.bf.private_tag_en = A_FALSE;
			port_2vlan.bf.private_tag_tci = 0;
		}

		rv = appe_egress_vp_tbl_set(dev_id, port_mapping->int_port, &port_2vlan);
		if (rv != SW_OK)
			goto rollback;
	}

	return rv;

rollback:
	/* for in/eg cfg both configured case need to rollback ingress cfg when egress process failed */
	if (need_rollback) {
		sw_error_t rollback_rv = SW_OK;

		if (ingress_inserted) {
			union vlan_to_port_mapping_tbl_u vlan_2port_clear;
			aos_mem_zero(&vlan_2port_clear, sizeof(union vlan_to_port_mapping_tbl_u));

			rollback_rv = jhppe_vlan_to_port_mapping_tbl_set(dev_id, ingress_index, &vlan_2port_clear);
		} else {
			rollback_rv = jhppe_vlan_to_port_mapping_tbl_set(dev_id, ingress_index, &ingress_backup);
		}

		if (rollback_rv != SW_OK) {
			SSDK_ERROR("Rollback operation failed with error: %d, original error: %d\n", rollback_rv, rv);
		}
	}

	return rv;
}

sw_error_t
adpt_jhppe_private_vlan_mapping_get(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping)
{
	sw_error_t rv = SW_OK;

	if (direction == FAL_DIR_INGRESS) {
		union vlan_to_port_mapping_tbl_u vlan_2port;
		a_uint32_t index;
		rv = adpt_jhppe_vlan_2port_map_tbl_find(dev_id, port_mapping, &index);
		SW_RTN_ON_ERROR(rv);

		rv = jhppe_vlan_to_port_mapping_tbl_get(dev_id, index, &vlan_2port);
		SW_RTN_ON_ERROR(rv);

		port_mapping->ptmap_en = vlan_2port.bf.valid;
		port_mapping->int_port = vlan_2port.bf.port_vp;
	} else if (direction == FAL_DIR_EGRESS) {
		union eg_vp_tbl_u port_2vlan;

		rv = appe_egress_vp_tbl_get(dev_id, port_mapping->int_port, &port_2vlan);
		SW_RTN_ON_ERROR(rv);

		port_mapping->ptmap_en = port_2vlan.bf.private_tag_en;
		port_mapping->vlan_tci = port_2vlan.bf.private_tag_tci;
	} else
		return SW_NOT_SUPPORTED;

	return rv;
}

sw_error_t
adpt_jhppe_private_vlan_tx_cfg_set(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union eg_global_ctrl_u tx_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	aos_mem_zero(&tx_ctrl, sizeof(union eg_global_ctrl_u));

	rv = hppe_eg_global_ctrl_get(dev_id, &tx_ctrl);
	SW_RTN_ON_ERROR(rv);

	tx_ctrl.bf.private_tag_tpid = cfg->tpid;

	rv = hppe_eg_global_ctrl_set(dev_id, &tx_ctrl);

	return rv;
}

sw_error_t
adpt_jhppe_private_vlan_tx_cfg_get(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg)
{
	sw_error_t rv = SW_OK;
	union eg_global_ctrl_u tx_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	aos_mem_zero(&tx_ctrl, sizeof(union eg_global_ctrl_u));

	rv = hppe_eg_global_ctrl_get(dev_id, &tx_ctrl);
	SW_RTN_ON_ERROR(rv);

	cfg->tpid = tx_ctrl.bf.private_tag_tpid;

	return SW_OK;
}

sw_error_t
adpt_jhppe_port_isol_action_ctrl_set(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_global_conf_u reg_val;

	rv = hppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	reg_val.bf.bc_pvlan_isol_en = act_ctrl->bc_isol_en;
	reg_val.bf.mc_pvlan_isol_en = act_ctrl->mc_isol_en;

	rv = hppe_l2_global_conf_set(dev_id, &reg_val);

	return rv;
}

sw_error_t
adpt_jhppe_port_isol_action_ctrl_get(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_global_conf_u reg_val;

	rv = hppe_l2_global_conf_get(dev_id, &reg_val);
	SW_RTN_ON_ERROR(rv);

	act_ctrl->bc_isol_en = reg_val.bf.bc_pvlan_isol_en;
	act_ctrl->mc_isol_en = reg_val.bf.mc_pvlan_isol_en;

	return SW_OK;
}

sw_error_t
adpt_jhppe_port_isol_action_set(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act)
{
	sw_error_t rv = SW_OK;
	union port_isol_action_u pp_isol_act;
	union vp_isol_action_tbl_u vp_isol_act;

	if (isol_id->isol_type == FAL_ISOL_ACT_PPORT) {
		rv = jhppe_port_isol_action_get(dev_id, isol_id->pport_id, &pp_isol_act);
		SW_RTN_ON_ERROR(rv);

		pp_isol_act.bf.action = isol_act->act_bitmap[0] & ((1 << PORT_ISOL_ACTION_ACTION_LEN) - 1);

		rv = jhppe_port_isol_action_set(dev_id, isol_id->pport_id, &pp_isol_act);
		SW_RTN_ON_ERROR(rv);
	} else {
		a_uint32_t i;

		rv = jhppe_vp_isol_action_tbl_get(dev_id, isol_id->isol_group_id, &vp_isol_act);
		SW_RTN_ON_ERROR(rv);

		for (i = 0; i < FAL_ISOL_ACT_ARR_SIZE; i++) {
			vp_isol_act.val[i] = isol_act->act_bitmap[i];
		}

		rv = jhppe_vp_isol_action_tbl_set(dev_id, isol_id->isol_group_id, &vp_isol_act);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_jhppe_port_isol_action_get(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act)
{
	sw_error_t rv = SW_OK;
	union port_isol_action_u pp_isol_act;
	union vp_isol_action_tbl_u vp_isol_act;

	if (isol_id->isol_type == FAL_ISOL_ACT_PPORT) {
		rv = jhppe_port_isol_action_get(dev_id, isol_id->pport_id, &pp_isol_act);
		SW_RTN_ON_ERROR(rv);

		isol_act->act_bitmap[0] = pp_isol_act.bf.action;
	} else {
		a_uint32_t i;

		rv = jhppe_vp_isol_action_tbl_get(dev_id, isol_id->isol_group_id, &vp_isol_act);
		SW_RTN_ON_ERROR(rv);

		for (i = 0; i < FAL_ISOL_ACT_ARR_SIZE; i++) {
			isol_act->act_bitmap[i] = vp_isol_act.val[i];
		}
	}

	return rv;
}

sw_error_t
adpt_jhppe_port_isol_ctrl_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_pre_tbl;
	union l2_vp_port_post_tbl_u l2_vp_post_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(isol_ctrl);

	aos_mem_zero(&l2_vp_pre_tbl, sizeof(union l2_vp_port_tbl_u));
	aos_mem_zero(&l2_vp_post_tbl, sizeof(union l2_vp_port_post_tbl_u));

	if (isol_ctrl->dir == FAL_DIR_INGRESS || isol_ctrl->dir == FAL_DIR_BOTH) {
		rv = appe_l2_vp_port_tbl_get(dev_id, port_value, &l2_vp_pre_tbl);
		SW_RTN_ON_ERROR(rv);

		l2_vp_pre_tbl.bf.isol_profile_en = isol_ctrl->enable;
		l2_vp_pre_tbl.bf.isol_profile = isol_ctrl->isol_group_id;

		rv = appe_l2_vp_port_tbl_set(dev_id, port_value, &l2_vp_pre_tbl);
		SW_RTN_ON_ERROR(rv);
	}

	if (isol_ctrl->dir == FAL_DIR_EGRESS || isol_ctrl->dir == FAL_DIR_BOTH) {
		rv = jhppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_post_tbl);
		SW_RTN_ON_ERROR(rv);

		l2_vp_post_tbl.bf.isol_profile_en = isol_ctrl->enable;
		l2_vp_post_tbl.bf.isol_profile = isol_ctrl->isol_group_id;

		rv = jhppe_l2_vp_port_post_tbl_set(dev_id, port_value, &l2_vp_post_tbl);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_jhppe_port_isol_ctrl_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl)
{
	sw_error_t rv = SW_OK;
	union l2_vp_port_tbl_u l2_vp_pre_tbl;
	union l2_vp_port_post_tbl_u l2_vp_post_tbl;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(isol_ctrl);

	aos_mem_zero(&l2_vp_pre_tbl, sizeof(union l2_vp_port_tbl_u));
	aos_mem_zero(&l2_vp_post_tbl, sizeof(union l2_vp_port_post_tbl_u));

	rv = appe_l2_vp_port_tbl_get(dev_id, port_value, &l2_vp_pre_tbl);
	SW_RTN_ON_ERROR(rv);
	rv = jhppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_post_tbl);
	SW_RTN_ON_ERROR(rv);

	if (isol_ctrl->dir == FAL_DIR_BOTH) {
		if (l2_vp_pre_tbl.bf.isol_profile_en != l2_vp_post_tbl.bf.isol_profile_en ||
			l2_vp_pre_tbl.bf.isol_profile != l2_vp_post_tbl.bf.isol_profile)
			return SW_NOT_SUPPORTED;

		isol_ctrl->enable = l2_vp_pre_tbl.bf.isol_profile_en;
		isol_ctrl->isol_group_id = l2_vp_pre_tbl.bf.isol_profile;
	}

	if (isol_ctrl->dir == FAL_DIR_INGRESS) {
		isol_ctrl->enable = l2_vp_pre_tbl.bf.isol_profile_en;
		isol_ctrl->isol_group_id = l2_vp_pre_tbl.bf.isol_profile;
	}

	if (isol_ctrl->dir == FAL_DIR_EGRESS) {
		isol_ctrl->enable = l2_vp_post_tbl.bf.isol_profile_en;
		isol_ctrl->isol_group_id = l2_vp_post_tbl.bf.isol_profile;
	}

	return rv;
}

