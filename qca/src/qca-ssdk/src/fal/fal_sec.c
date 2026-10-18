/*
 * Copyright (c) 2012, 2016-2017, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022,2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


/**
 * @defgroup fal_sec FAL_SEC
 * @{
 */
#include "sw.h"
#include "fal_sec.h"
#include "hsl_api.h"
#include "adpt.h"

#ifndef IN_SEC_MINI
sw_error_t fal_sec_norm_item_set(a_uint32_t dev_id, fal_norm_item_t item, void *value)
    DEFINE_FAL_FUNC_HSL_EXPORT(sec_norm_item_set, dev_id, item, value)

sw_error_t fal_sec_norm_item_get(a_uint32_t dev_id, fal_norm_item_t item, void *value)
    DEFINE_FAL_FUNC_HSL_EXPORT(sec_norm_item_get, dev_id, item, value)

sw_error_t fal_sec_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_l2_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l2_excep_ctrl_set, dev_id, excep_type, ctrl)

sw_error_t fal_sec_l2_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l2_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l2_excep_ctrl_get, dev_id, excep_type, ctrl)

sw_error_t fal_sec_tunnel_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_excep_ctrl_set, dev_id, excep_type, ctrl)

sw_error_t fal_sec_tunnel_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_tunnel_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_excep_ctrl_get, dev_id, excep_type, ctrl)

sw_error_t fal_sec_tunnel_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l3_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_tunnel_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l3_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_tunnel_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l4_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_tunnel_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_l4_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_tunnel_flags_excep_parser_ctrl_set(a_uint32_t dev_id, a_uint32_t index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_flags_excep_parser_ctrl_set, dev_id, index, ctrl)

sw_error_t fal_sec_tunnel_flags_excep_parser_ctrl_get(a_uint32_t dev_id, a_uint32_t index, fal_tunnel_flags_excep_parser_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_tunnel_flags_excep_parser_ctrl_get, dev_id, index, ctrl)
#endif

sw_error_t fal_sec_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l4_excep_parser_ctrl_set, dev_id, ctrl)

sw_error_t fal_sec_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l4_excep_parser_ctrl_get, dev_id, ctrl)

sw_error_t fal_sec_l3_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_ctrl_set, dev_id, excep_type, ctrl)

sw_error_t fal_sec_l3_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sec_l3_excep_ctrl_get, dev_id, excep_type, ctrl)

