/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_JHPPE_PORTVLAN_
#define _ADPT_JHPPE_PORTVLAN_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_vlan_trans_dscp_pcp_mapping_set(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp);
sw_error_t
adpt_jhppe_vlan_trans_dscp_pcp_mapping_get(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp);
sw_error_t
adpt_jhppe_private_vlan_rx_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg);
sw_error_t
adpt_jhppe_private_vlan_rx_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg);
sw_error_t
adpt_jhppe_private_vlan_mapping_set(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping);
sw_error_t
adpt_jhppe_private_vlan_mapping_get(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping);
sw_error_t
adpt_jhppe_private_vlan_tx_cfg_set(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg);
sw_error_t
adpt_jhppe_private_vlan_tx_cfg_get(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg);

sw_error_t
adpt_jhppe_port_isol_ctrl_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);
sw_error_t
adpt_jhppe_port_isol_ctrl_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);
sw_error_t
adpt_jhppe_port_isol_action_ctrl_set(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl);
sw_error_t
adpt_jhppe_port_isol_action_ctrl_get(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl);
sw_error_t
adpt_jhppe_port_isol_action_set(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act);
sw_error_t
adpt_jhppe_port_isol_action_get(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
