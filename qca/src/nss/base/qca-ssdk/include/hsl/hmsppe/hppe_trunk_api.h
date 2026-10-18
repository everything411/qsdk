/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_TRUNK_API_H_
#define _HPPE_TRUNK_API_H_


sw_error_t
hppe_trunk_hash_field_reg_get(
		a_uint32_t dev_id,
		union trunk_hash_field_reg_u *value);

sw_error_t
hppe_trunk_hash_field_reg_set(
		a_uint32_t dev_id,
		union trunk_hash_field_reg_u *value);

sw_error_t
hppe_trunk_hash_field_reg_dst_ip_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_dst_ip_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_l4_dst_port_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_l4_dst_port_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_l4_src_port_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_l4_src_port_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_mac_da_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_mac_da_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_mac_sa_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_mac_sa_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_src_ip_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_src_ip_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_src_port_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_src_port_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_udf0_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_udf0_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_udf1_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_udf1_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_udf2_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_udf2_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
hppe_trunk_hash_field_reg_udf3_incl_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
hppe_trunk_hash_field_reg_udf3_incl_set(
		a_uint32_t dev_id,
		unsigned int value);

#define TRUNK_FILTER_MAX_ENTRY	TRUNK_FILTER_NUM

sw_error_t
hppe_trunk_filter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union trunk_filter_u *value);

sw_error_t
hppe_trunk_filter_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union trunk_filter_u *value);

sw_error_t
hppe_trunk_filter_mem_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_filter_mem_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TRUNK_MEMBER_MAX_ENTRY	TRUNK_MEMBER_NUM

sw_error_t
hppe_trunk_member_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union trunk_member_u *value);

sw_error_t
hppe_trunk_member_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union trunk_member_u *value);

sw_error_t
hppe_trunk_member_member_0_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_0_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_1_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_1_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_2_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_2_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_3_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_3_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_4_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_4_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_5_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_5_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_6_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_6_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_trunk_member_member_7_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_trunk_member_member_7_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define PORT_TRUNK_ID_MAX_ENTRY	PORT_TRUNK_ID_NUM

sw_error_t
hppe_port_trunk_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_trunk_id_u *value);

sw_error_t
hppe_port_trunk_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union port_trunk_id_u *value);

sw_error_t
hppe_port_trunk_id_trunk_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_trunk_id_trunk_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_port_trunk_id_trunk_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_port_trunk_id_trunk_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
