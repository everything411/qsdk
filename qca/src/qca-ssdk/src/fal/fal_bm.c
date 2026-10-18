/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "sw.h"
#include "fal_bm.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_port_bufgroup_map_get(a_uint32_t dev_id, fal_port_t port, a_uint8_t *group)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_bufgroup_map_get, dev_id, port, group)

sw_error_t fal_bm_port_reserved_buffer_get(a_uint32_t dev_id, fal_port_t port, a_uint16_t *prealloc_buff, a_uint16_t *react_buff)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_port_reserved_buffer_get, dev_id, port, prealloc_buff, react_buff)

sw_error_t fal_bm_bufgroup_buffer_get(a_uint32_t dev_id, a_uint8_t group, a_uint16_t *buff_num)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_bufgroup_buffer_get, dev_id, group, buff_num)

sw_error_t fal_bm_port_dynamic_thresh_get(a_uint32_t dev_id, fal_port_t port, fal_bm_dynamic_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_port_dynamic_thresh_get, dev_id, port, cfg)

sw_error_t fal_port_bm_ctrl_get(a_uint32_t dev_id, fal_port_t port, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_bm_ctrl_get, dev_id, port, enable)

sw_error_t fal_bm_bufgroup_buffer_set(a_uint32_t dev_id, a_uint8_t group, a_uint16_t buff_num)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_bufgroup_buffer_set, dev_id, group, buff_num)

sw_error_t fal_port_bufgroup_map_set(a_uint32_t dev_id, fal_port_t port, a_uint8_t group)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_bufgroup_map_set, dev_id, port, group)

sw_error_t fal_bm_port_reserved_buffer_set(a_uint32_t dev_id, fal_port_t port, a_uint16_t prealloc_buff, a_uint16_t react_buff)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_port_reserved_buffer_set, dev_id, port, prealloc_buff, react_buff)

sw_error_t fal_bm_port_dynamic_thresh_set(a_uint32_t dev_id, fal_port_t port, fal_bm_dynamic_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_port_dynamic_thresh_set, dev_id, port, cfg)

sw_error_t fal_port_bm_ctrl_set(a_uint32_t dev_id, fal_port_t port, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_bm_ctrl_set, dev_id, port, enable)

sw_error_t fal_bm_port_counter_get(a_uint32_t dev_id, fal_port_t port, fal_bm_port_counter_t *counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(bm_port_counter_get, dev_id, port, counter)

#ifndef IN_BM_MINI
sw_error_t fal_bm_port_static_thresh_get(a_uint32_t dev_id, fal_port_t port, fal_bm_static_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_HSL_EXPORT(bm_port_static_thresh_get, port_static_thresh_get, dev_id, port, cfg)

sw_error_t fal_bm_port_static_thresh_set(a_uint32_t dev_id, fal_port_t port, fal_bm_static_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_HSL_EXPORT(bm_port_static_thresh_set, port_static_thresh_set, dev_id, port, cfg)
#endif

