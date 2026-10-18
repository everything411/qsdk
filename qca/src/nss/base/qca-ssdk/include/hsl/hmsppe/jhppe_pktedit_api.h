/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_PKTEDIT_API_H_
#define _JHPPE_PKTEDIT_API_H_


sw_error_t
jhppe_tpr_pkt_len_ctrl_get(
		a_uint32_t dev_id,
		union tpr_pkt_len_ctrl_u *value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_set(
		a_uint32_t dev_id,
		union tpr_pkt_len_ctrl_u *value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_org_len_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_org_len_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_00_check_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_00_check_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_f8_check_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tpr_pkt_len_ctrl_snap_oui_f8_check_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_pkt_len_ctrl_get(
		a_uint32_t dev_id,
		union pkt_len_ctrl_u *value);

sw_error_t
jhppe_pkt_len_ctrl_set(
		a_uint32_t dev_id,
		union pkt_len_ctrl_u *value);

sw_error_t
jhppe_pkt_len_ctrl_org_len_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_pkt_len_ctrl_org_len_en_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_00_check_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_00_check_set(
		a_uint32_t dev_id,
		unsigned int value);

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_f8_check_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_pkt_len_ctrl_snap_oui_f8_check_set(
		a_uint32_t dev_id,
		unsigned int value);


sw_error_t
jhppe_tl_pkt_len_ctrl_get(
		a_uint32_t dev_id,
		union tl_pkt_len_ctrl_u *value);

sw_error_t
jhppe_tl_pkt_len_ctrl_set(
		a_uint32_t dev_id,
		union tl_pkt_len_ctrl_u *value);

sw_error_t
jhppe_tl_pkt_len_ctrl_org_len_en_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
jhppe_tl_pkt_len_ctrl_org_len_en_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
