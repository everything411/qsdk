/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_L3_FLOW_API_H_
#define _JHPPE_L3_FLOW_API_H_


sw_error_t
jhppe_app_udp_port_cfg_get(
		a_uint32_t dev_id,
		union app_udp_port_cfg_u *value);

sw_error_t
jhppe_app_udp_port_cfg_set(
		a_uint32_t dev_id,
		union app_udp_port_cfg_u *value);

sw_error_t
jhppe_app_udp_port_cfg_ip_ver_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_app_udp_port_cfg_ip_ver_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_app_udp_port_cfg_port_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_app_udp_port_cfg_port_type_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_app_udp_port_cfg_port_value_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_app_udp_port_cfg_port_value_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_app_udp_port_cfg_udp_type_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_app_udp_port_cfg_udp_type_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_nat_t_cfg_get(
		a_uint32_t dev_id,
		union nat_t_cfg_u *value);

sw_error_t
jhppe_nat_t_cfg_set(
		a_uint32_t dev_id,
		union nat_t_cfg_u *value);

sw_error_t
jhppe_nat_t_cfg_app_udp_port_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_nat_t_cfg_app_udp_port_map_set(
		a_uint32_t dev_id,
		unsigned int value);

#define L3_FLOW_KEY_GEN_MAX_ENTRY	L3_FLOW_KEY_GEN_NUM

sw_error_t
jhppe_l3_flow_key_gen_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_gen_u *value);

sw_error_t
jhppe_l3_flow_key_gen_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_gen_u *value);

sw_error_t
jhppe_l3_flow_key_gen_app_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_app_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_gen_app_type_vld_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_app_type_vld_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_gen_l3_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_l3_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_gen_l3_type_vld_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_l3_type_vld_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_gen_l4_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_l4_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_gen_l4_type_vld_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_l4_type_vld_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_gen_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_gen_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L3_FLOW_KEY_SEL_MAX_ENTRY	L3_FLOW_KEY_SEL_NUM

sw_error_t
jhppe_l3_flow_key_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_sel_u *value);

sw_error_t
jhppe_l3_flow_key_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_flow_key_sel_u *value);

sw_error_t
jhppe_l3_flow_key_sel_dip_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_dip_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_dport_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_dport_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_ip_prot_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_ip_prot_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_sip_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_sip_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_sport_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_sport_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_udf0_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_udf0_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_udf0_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_udf0_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_udf0_mask_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_udf0_mask_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_udf1_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_udf1_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_udf1_inc_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_udf1_inc_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_flow_key_sel_udf1_mask_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_flow_key_sel_udf1_mask_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L3_MY_MAC_TBL_MAX_ENTRY	L3_MY_MAC_TBL_NUM

sw_error_t
jhppe_l3_my_mac_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_my_mac_tbl_u *value);

sw_error_t
jhppe_l3_my_mac_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l3_my_mac_tbl_u *value);

sw_error_t
jhppe_l3_my_mac_tbl_l3_if_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_my_mac_tbl_l3_if_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_my_mac_tbl_mac_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_l3_my_mac_tbl_mac_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
jhppe_l3_my_mac_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_my_mac_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_my_mac_tbl_vsi_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_my_mac_tbl_vsi_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l3_my_mac_tbl_vsi_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l3_my_mac_tbl_vsi_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_FLOW_6TUPLE_TBL_MAX_ENTRY	IN_FLOW_6TUPLE_TBL_NUM

sw_error_t
jhppe_in_flow_6tuple_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_6tuple_tbl_u *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_flow_6tuple_tbl_u *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_age_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_age_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_copy_to_cpu_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_copy_to_cpu_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_counter_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_counter_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_cvlan_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_cvlan_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_de_acce_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_de_acce_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_entry_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_entry_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_fwd_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_fwd_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_host_addr_index_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_host_addr_index_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_host_addr_index_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_host_addr_index_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_ip_addr_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_ip_addr_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint64_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_ip_protocol_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_ip_protocol_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_dport_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_dport_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_port1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_port1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_port2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_port2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_sport_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_l4_sport_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_match_more_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_match_more_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_nat_action1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_nat_action1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop4_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop4_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop4_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_next_hop4_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_pmtu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_pmtu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_pmtu_check_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_pmtu_check_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_port_vp1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_port_vp1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_port_vp2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_port_vp2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_port_vp_valid1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_port_vp_valid1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_pri_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_pri_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_protocol_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_protocol_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_routing_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_routing_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_src_l3_if_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_src_l3_if_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_src_l3_if_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_src_l3_if_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_svlan_fmt_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_svlan_fmt_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_syn_toggle_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_syn_toggle_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_udf0_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_udf0_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_udf1_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_udf1_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_vlan_fmt_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_vlan_fmt_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_flow_6tuple_tbl_vpn_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_flow_6tuple_tbl_vpn_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
