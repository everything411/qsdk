/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_TUNNEL_API_H_
#define _JHPPE_TUNNEL_API_H_

#define TPR_TUPLE_IP_ADDR_0_MAX_ENTRY	TPR_TUPLE_IP_ADDR_0_NUM

sw_error_t
jhppe_tpr_tuple_ip_addr_0_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_0_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_0_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_0_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_0_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_0_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_IP_ADDR_1_MAX_ENTRY	TPR_TUPLE_IP_ADDR_1_NUM

sw_error_t
jhppe_tpr_tuple_ip_addr_1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_1_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_1_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_1_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_1_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_IP_ADDR_2_MAX_ENTRY	TPR_TUPLE_IP_ADDR_2_NUM

sw_error_t
jhppe_tpr_tuple_ip_addr_2_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_2_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_2_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_2_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_2_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_2_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_IP_ADDR_3_MAX_ENTRY	TPR_TUPLE_IP_ADDR_3_NUM

sw_error_t
jhppe_tpr_tuple_ip_addr_3_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_3_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_3_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_3_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_3_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_3_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_IP_ADDR_CTRL_MAX_ENTRY	TPR_TUPLE_IP_ADDR_CTRL_NUM

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_ctrl_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_ip_addr_ctrl_u *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_ver_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_ip_addr_ctrl_ver_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_L4_PORT_MAX_ENTRY	TPR_TUPLE_L4_PORT_NUM

sw_error_t
jhppe_tpr_tuple_l4_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_l4_port_u *value);

sw_error_t
jhppe_tpr_tuple_l4_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_l4_port_u *value);

sw_error_t
jhppe_tpr_tuple_l4_port_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_l4_port_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_tuple_l4_port_port_value_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_l4_port_port_value_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_ADDR_MATCH_MAX_ENTRY	TPR_TUPLE_ADDR_MATCH_NUM

sw_error_t
jhppe_tpr_tuple_addr_match_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_addr_match_u *value);

sw_error_t
jhppe_tpr_tuple_addr_match_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_addr_match_u *value);

sw_error_t
jhppe_tpr_tuple_addr_match_addr_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_addr_match_addr_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_PORT_MATCH_MAX_ENTRY	TPR_TUPLE_PORT_MATCH_NUM

sw_error_t
jhppe_tpr_tuple_port_match_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_port_match_u *value);

sw_error_t
jhppe_tpr_tuple_port_match_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_port_match_u *value);

sw_error_t
jhppe_tpr_tuple_port_match_port_map_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_port_match_port_map_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_TUPLE_MATCH_CTRL_MAX_ENTRY	TPR_TUPLE_MATCH_CTRL_NUM

sw_error_t
jhppe_tpr_tuple_match_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_match_ctrl_u *value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_tuple_match_ctrl_u *value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_tunnel_type_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_tuple_id_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_tuple_id_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_tuple_match_ctrl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TUNNEL_TYPE_MAPPING_MAX_ENTRY	TUNNEL_TYPE_MAPPING_NUM

sw_error_t
jhppe_tunnel_type_mapping_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tunnel_type_mapping_u *value);

sw_error_t
jhppe_tunnel_type_mapping_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tunnel_type_mapping_u *value);

sw_error_t
jhppe_tunnel_type_mapping_app_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tunnel_type_mapping_app_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tunnel_type_mapping_outer_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tunnel_type_mapping_outer_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
