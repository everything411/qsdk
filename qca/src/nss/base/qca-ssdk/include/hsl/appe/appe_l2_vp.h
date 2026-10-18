/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _APPE_VP_H_
#define _APPE_VP_H_
#include "appe_l2_vp_reg.h"

#define ALDER_L2_BASE_ADDR    0x60000
#define VP_LRN_LIMIT_COUNTER_MAX_ENTRY	256
#define L2_VP_PORT_TBL_MAX_ENTRY	256
#define PORT_VSI_ENQUEUE_MAP_MAX_ENTRY	544

sw_error_t
appe_vp_lrn_limit_counter_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union vp_lrn_limit_counter_u *value);

sw_error_t
appe_l2_vp_port_tbl_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_tbl_u *value);

sw_error_t
appe_l2_vp_port_tbl_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		union l2_vp_port_tbl_u *value);

sw_error_t
appe_l2_vp_port_tbl_promisc_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_promisc_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_l2_vp_port_tbl_port_isolation_bitmap_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_port_isolation_bitmap_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_l2_vp_port_tbl_dst_info_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_dst_info_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_l2_vp_port_tbl_app_ctrl_profile_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_app_ctrl_profile_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_l2_vp_port_tbl_invalid_vsi_forwarding_en_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_invalid_vsi_forwarding_en_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);

sw_error_t
appe_l2_vp_port_tbl_physical_port_get(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t *value);

sw_error_t
appe_l2_vp_port_tbl_physical_port_set(
		a_uint32_t dev_id,
		a_uint32_t index,
		a_uint32_t value);
#endif
