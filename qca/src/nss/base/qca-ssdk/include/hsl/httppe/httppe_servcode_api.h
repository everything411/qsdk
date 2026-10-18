/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HTTPPE_SERVCODE_API_H_
#define _HTTPPE_SERVCODE_API_H_

#define IN_L2_SERVICE_TBL_MAX_ENTRY	IN_L2_SERVICE_TBL_NUM

sw_error_t
httppe_in_l2_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_l2_service_tbl_u *value);

sw_error_t
httppe_in_l2_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_l2_service_tbl_u *value);

sw_error_t
httppe_in_l2_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_bypass_bitmap_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_bypass_bitmap_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_bypass_bitmap_new_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_bypass_bitmap_new_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_direction_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_direction_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_dst_port_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_dst_port_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_dst_port_id_valid_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_dst_port_id_valid_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_rx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_rx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_in_l2_service_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_in_l2_service_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_SERVICE_TBL_MAX_ENTRY	EG_SERVICE_TBL_NUM

sw_error_t
httppe_eg_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_service_tbl_u *value);

sw_error_t
httppe_eg_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_service_tbl_u *value);

sw_error_t
httppe_eg_service_tbl_ath_disable_bit_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_ath_disable_bit_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_ath_from_cpu_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_ath_from_cpu_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_ath_hdr_insert_dis_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_ath_hdr_insert_dis_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_ath_hdr_type_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_ath_hdr_type_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_ath_port_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_ath_port_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_field_update_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_field_update_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_field_update_action_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_field_update_action_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_field_update_action_ext1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_field_update_action_ext1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_hw_services_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_hw_services_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_ip_length_update_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_ip_length_update_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_next_service_code_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_next_service_code_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_offset_sel_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_offset_sel_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
httppe_eg_service_tbl_tx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
httppe_eg_service_tbl_tx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
