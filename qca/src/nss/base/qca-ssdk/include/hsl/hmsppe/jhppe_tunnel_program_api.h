/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _JHPPE_TUNNEL_PROGRAM_API_H_
#define _JHPPE_TUNNEL_PROGRAM_API_H_

#define TPR_HDR_MATCH_CTRL_MAX_ENTRY	TPR_HDR_MATCH_CTRL_NUM

sw_error_t
jhppe_tpr_hdr_match_ctrl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_ctrl_u *value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_match_ctrl_u *value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_mode_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_mode_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_offset_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_hdr_match_ctrl_protocol_pos_offset_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#define TPR_HDR_TUPLE_MATCH_MAX_ENTRY	TPR_HDR_TUPLE_MATCH_NUM

sw_error_t
jhppe_tpr_hdr_tuple_match_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_tuple_match_u *value);

sw_error_t
jhppe_tpr_hdr_tuple_match_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union tpr_hdr_tuple_match_u *value);

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_incl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
jhppe_tpr_hdr_tuple_match_tuple_id_incl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

#endif
