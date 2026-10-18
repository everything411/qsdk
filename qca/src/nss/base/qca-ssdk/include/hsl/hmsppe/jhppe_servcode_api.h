/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_SERVCODE_API_H_
#define _JHPPE_SERVCODE_API_H_

#define L2_SPEC_SERVICE_TBL_MAX_ENTRY	L2_SPEC_SERVICE_TBL_NUM

sw_error_t
jhppe_l2_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_spec_service_tbl_u *value);

sw_error_t
jhppe_l2_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_spec_service_tbl_u *value);

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_new_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_spec_service_tbl_bypass_bitmap_new_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_spec_service_tbl_post_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_spec_service_tbl_post_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_spec_service_tbl_rx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_spec_service_tbl_rx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_l2_spec_service_tbl_tx_cnt_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_l2_spec_service_tbl_tx_cnt_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define IN_SPEC_SERVICE_TBL_MAX_ENTRY	IN_SPEC_SERVICE_TBL_NUM

sw_error_t
jhppe_in_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_spec_service_tbl_u *value);

sw_error_t
jhppe_in_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union in_spec_service_tbl_u *value);

sw_error_t
jhppe_in_spec_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_spec_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_in_spec_service_tbl_rx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_in_spec_service_tbl_rx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define EG_SPEC_SERVICE_TBL_MAX_ENTRY	EG_SPEC_SERVICE_TBL_NUM

sw_error_t
jhppe_eg_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_spec_service_tbl_u *value);

sw_error_t
jhppe_eg_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union eg_spec_service_tbl_u *value);

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext1_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_spec_service_tbl_field_update_action_ext1_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_spec_service_tbl_ip_length_update_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_spec_service_tbl_ip_length_update_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_eg_spec_service_tbl_tx_counting_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_eg_spec_service_tbl_tx_counting_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TL_SPEC_SERVICE_TBL_MAX_ENTRY	TL_SPEC_SERVICE_TBL_NUM

sw_error_t
jhppe_tl_spec_service_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_spec_service_tbl_u *value);

sw_error_t
jhppe_tl_spec_service_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tl_spec_service_tbl_u *value);

sw_error_t
jhppe_tl_spec_service_tbl_bypass_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tl_spec_service_tbl_bypass_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
