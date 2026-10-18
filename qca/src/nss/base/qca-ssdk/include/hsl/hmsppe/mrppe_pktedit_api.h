/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _MRPPE_PKTEDIT_API_H_
#define _MRPPE_PKTEDIT_API_H_


sw_error_t
mrppe_ipr_spare_reg_get(
		a_uint32_t dev_id,
		union ipr_spare_reg_u *value);

sw_error_t
mrppe_ipr_spare_reg_set(
		a_uint32_t dev_id,
		union ipr_spare_reg_u *value);

sw_error_t
mrppe_ipr_spare_reg_inner_ip_len_gap_exp_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_ipr_spare_reg_inner_ip_len_gap_exp_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mrppe_ipr_spare_reg_inner_ip_len_update_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_ipr_spare_reg_inner_ip_len_update_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mrppe_ipr_spare_reg_inner_ip_padding_exp_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_ipr_spare_reg_inner_ip_padding_exp_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mrppe_ipr_spare_reg_payload_checksum_strip_padding_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_ipr_spare_reg_payload_checksum_strip_padding_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
mrppe_ipr_spare_reg_spare_reg_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
mrppe_ipr_spare_reg_spare_reg_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
