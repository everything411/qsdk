/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_HTTPPE_PORTVLAN_
#define _ADPT_HTTPPE_PORTVLAN_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_httppe_port_qinq_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode);

sw_error_t
adpt_httppe_port_qinq_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_qinq_role_t *mode);

sw_error_t
adpt_httppe_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
adpt_httppe_tpid_get(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
adpt_httppe_egress_tpid_set(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
adpt_httppe_egress_tpid_get(a_uint32_t dev_id, fal_tpid_t *tpid);

sw_error_t
adpt_httppe_port_vlan_trans_adv_add(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
adpt_httppe_port_vlan_trans_adv_del(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
adpt_httppe_port_vlan_trans_adv_get(a_uint32_t dev_id,
		fal_port_vlan_direction_t direction, a_uint32_t index,
		fal_vlan_trans_adv_rule_t *rule, fal_vlan_trans_adv_action_t *action);

sw_error_t
adpt_httppe_port_vlan_trans_adv_set(a_uint32_t dev_id,
		fal_port_vlan_direction_t direction, a_uint32_t index,
		fal_vlan_trans_adv_rule_t *rule, fal_vlan_trans_adv_action_t *action);

sw_error_t
adpt_httppe_port_vlan_trans_adv_getfirst(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
adpt_httppe_port_vlan_trans_adv_getnext(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		fal_vlan_trans_adv_rule_t * rule, fal_vlan_trans_adv_action_t * action);

sw_error_t
adpt_httppe_port_vlantag_egmode_set(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t stag_mode, a_uint32_t ctag_mode, a_uint32_t mask);

sw_error_t
adpt_httppe_port_vlantag_egmode_get(a_uint32_t dev_id, fal_port_t port_id,
                            a_uint32_t *stag_mode, a_uint32_t *ctag_mode);

sw_error_t
adpt_httppe_port_vlantag_vsi_egmode_enable_set(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t enable);

sw_error_t
adpt_httppe_port_vlantag_vsi_egmode_enable_get(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t *enable);

sw_error_t
adpt_httppe_port_eg_vlan_tx_counting_en_set(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t value);

sw_error_t
adpt_httppe_port_eg_vlan_tx_counting_en_get(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t *value);

sw_error_t
adpt_httppe_port_default_pcp_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_tag_t *default_tag);

sw_error_t
adpt_httppe_port_default_pcp_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_tag_t *default_tag);

sw_error_t
adpt_httppe_port_vlan_xlt_miss_fwd_cmd_get(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t *value);

sw_error_t
adpt_httppe_port_vlan_xlt_miss_fwd_cmd_set(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t value);

sw_error_t
adpt_httppe_port_ingress_vlan_filter_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_ingress_vlan_filter_t *filter);

sw_error_t
adpt_httppe_port_ingress_vlan_filter_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_ingress_vlan_filter_t *filter);

sw_error_t
adpt_httppe_global_qinq_mode_set(a_uint32_t dev_id, fal_global_qinq_mode_t *mode);

sw_error_t
adpt_httppe_global_qinq_mode_get(a_uint32_t dev_id, fal_global_qinq_mode_t *mode);
sw_error_t
adpt_httppe_portvlan_vpmember_get(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t * mem_port_map);

sw_error_t
adpt_httppe_portvlan_vpmember_update(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t mem_port_map);

sw_error_t
adpt_httppe_portvlan_vpmember_add(a_uint32_t dev_id, fal_port_t port_id, fal_port_t mem_port_id);

sw_error_t
adpt_httppe_portvlan_vpmember_del(a_uint32_t dev_id, fal_port_t port_id, fal_port_t mem_port_id);

sw_error_t
adpt_httppe_vlan_trans_dscp_pcp_mapping_set(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t pcp);

sw_error_t
adpt_httppe_vlan_trans_dscp_pcp_mapping_get(a_uint32_t dev_id, fal_port_vlan_direction_t direction,
		a_uint8_t group_id, a_uint8_t dscp, a_uint8_t *pcp);

sw_error_t
adpt_httppe_private_vlan_rx_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg);

sw_error_t
adpt_httppe_private_vlan_rx_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_pvtvlan_rx_cfg_t *cfg);

sw_error_t
adpt_httppe_private_vlan_mapping_set(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping);

sw_error_t
adpt_httppe_private_vlan_mapping_get(a_uint32_t dev_id, fal_direction_t direction,
		fal_pvtvlan_map_t *port_mapping);

sw_error_t
adpt_httppe_private_vlan_tx_cfg_set(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg);

sw_error_t
adpt_httppe_private_vlan_tx_cfg_get(a_uint32_t dev_id, fal_pvtvlan_tx_cfg_t *cfg);

sw_error_t
adpt_httppe_port_isol_ctrl_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);

sw_error_t
adpt_httppe_port_isol_ctrl_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);

sw_error_t
adpt_httppe_port_isol_group_set(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp);

sw_error_t
adpt_httppe_port_isol_group_get(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp);

sw_error_t
adpt_httppe_port_isol_action_ctrl_set(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl);

sw_error_t
adpt_httppe_port_isol_action_ctrl_get(a_uint32_t dev_id,
		fal_port_isol_act_ctrl_t *act_ctrl);

sw_error_t
adpt_httppe_port_isol_action_set(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act);

sw_error_t
adpt_httppe_port_isol_action_get(a_uint32_t dev_id,
		fal_port_isol_act_idx_t *isol_id, fal_port_isol_act_t *isol_act);

#ifndef IN_PORTVLAN_MINI
sw_error_t
adpt_httppe_port_tag_propagation_set(a_uint32_t dev_id,
		fal_port_t port_id, a_uint32_t mask, fal_port_vlan_direction_t direction,
		a_uint32_t value_pcp, a_uint32_t value_dei);

sw_error_t
adpt_httppe_port_tag_propagation_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_vlan_direction_t direction,
		a_uint32_t *value_pcp, a_uint32_t *value_dei);

sw_error_t
adpt_httppe_port_vlan_counter_get(a_uint32_t dev_id,
		a_uint32_t cnt_index, fal_port_vlan_counter_t *counter);

sw_error_t
adpt_httppe_port_vlan_counter_cleanup(a_uint32_t dev_id,
		a_uint32_t cnt_index);

sw_error_t
adpt_httppe_port_egress_vlan_filter_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter);

sw_error_t
adpt_httppe_port_egress_vlan_filter_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter);
#endif
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
