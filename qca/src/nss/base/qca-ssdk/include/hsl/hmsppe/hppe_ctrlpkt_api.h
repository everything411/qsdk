/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_CTRLPKT_API_H_
#define _HPPE_CTRLPKT_API_H_

#define ETHERTYPE_CTRL_MAX_ENTRY	ETHERTYPE_CTRL_NUM

sw_error_t
hppe_ethertype_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ethertype_ctrl_u *value);

sw_error_t
hppe_ethertype_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union ethertype_ctrl_u *value);

sw_error_t
hppe_ethertype_ctrl_ethertype_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ethertype_ctrl_ethertype_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_ethertype_ctrl_ethertype_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_ethertype_ctrl_ethertype_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define L2_CPU_CODE_CTRL_MAX_ENTRY	L2_CPU_CODE_CTRL_NUM

sw_error_t
hppe_l2_cpu_code_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_cpu_code_ctrl_u *value);

sw_error_t
hppe_l2_cpu_code_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_cpu_code_ctrl_u *value);

sw_error_t
hppe_l2_cpu_code_ctrl_enqueue_disable_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l2_cpu_code_ctrl_enqueue_disable_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_l2_cpu_code_ctrl_exception_fmt_ctrl_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define APP_CTRL_MAX_ENTRY	APP_CTRL_NUM

sw_error_t
hppe_app_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value);

sw_error_t
hppe_app_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union app_ctrl_u *value);

sw_error_t
hppe_app_ctrl_cmd_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_cmd_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_ethertype_include_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_ethertype_include_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_ethertype_index_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_ethertype_index_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_in_stg_byp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_in_stg_byp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_in_vlan_fltr_byp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_in_vlan_fltr_byp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_l2_sec_byp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_l2_sec_byp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_port_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_port_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_portbitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_portbitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_portbitmap_include_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_portbitmap_include_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_protocol_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_protocol_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_protocol_include_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_protocol_include_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_rfdb_include_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_rfdb_include_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_rfdb_index_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_rfdb_index_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_sg_byp_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_sg_byp_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
hppe_app_ctrl_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
hppe_app_ctrl_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
