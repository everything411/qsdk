/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @defgroup fal_shaper FAL_SHAPER
 * @{
 */
#include "sw.h"
#include "fal_shaper.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_flow_shaper_set(a_uint32_t dev_id, a_uint32_t flow_id, fal_shaper_config_t * shaper) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_shaper_set, dev_id, flow_id, shaper)

sw_error_t fal_queue_shaper_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_shaper_config_t * shaper) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_shaper_get, dev_id, queue_id, shaper)

sw_error_t fal_queue_shaper_token_number_set(a_uint32_t dev_id,a_uint32_t queue_id, fal_shaper_token_number_t *token_number) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_shaper_token_number_set, dev_id, queue_id, token_number)

sw_error_t fal_flow_shaper_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot) 
    DEFINE_FAL_FUNC_ADPT(flow_shaper_time_slot_set, dev_id, timeslot)
    EXPORT_SYMBOL(fal_flow_shaper_timeslot_set);

sw_error_t fal_port_shaper_token_number_set(a_uint32_t dev_id, fal_port_t port_id, fal_shaper_token_number_t *token_number) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_shaper_token_number_set, dev_id, port_id, token_number)

sw_error_t fal_flow_shaper_token_number_set(a_uint32_t dev_id, a_uint32_t flow_id, fal_shaper_token_number_t *token_number) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_shaper_token_number_set, dev_id, flow_id, token_number)

sw_error_t fal_port_shaper_set(a_uint32_t dev_id, fal_port_t port_id, fal_shaper_config_t * shaper) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_shaper_set, dev_id, port_id, shaper)

sw_error_t fal_port_shaper_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot) 
    DEFINE_FAL_FUNC_ADPT(port_shaper_time_slot_set, dev_id, timeslot)
    EXPORT_SYMBOL(fal_port_shaper_timeslot_set);

sw_error_t fal_queue_shaper_set(a_uint32_t dev_id,a_uint32_t queue_id, fal_shaper_config_t * shaper) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_shaper_set, dev_id, queue_id, shaper)

sw_error_t fal_queue_shaper_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot) 
    DEFINE_FAL_FUNC_ADPT(queue_shaper_time_slot_set, dev_id, timeslot)
    EXPORT_SYMBOL(fal_queue_shaper_timeslot_set);

sw_error_t fal_shaper_ipg_preamble_length_set(a_uint32_t dev_id, a_uint32_t ipg_pre_length) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(shaper_ipg_preamble_length_set, dev_id, ipg_pre_length)

sw_error_t fal_queue_shaper_ctrl_set(a_uint32_t dev_id, fal_shaper_ctrl_t *queue_shaper_ctrl) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_shaper_ctrl_set, dev_id, queue_shaper_ctrl)

sw_error_t fal_flow_shaper_ctrl_set(a_uint32_t dev_id, fal_shaper_ctrl_t *flow_shaper_ctrl) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_shaper_ctrl_set, dev_id, flow_shaper_ctrl)

#ifndef IN_SHAPER_MINI
sw_error_t fal_port_shaper_get(a_uint32_t dev_id, fal_port_t port_id, fal_shaper_config_t * shaper) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_shaper_get, dev_id, port_id, shaper)

sw_error_t fal_flow_shaper_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot) 
    DEFINE_FAL_FUNC_ADPT(flow_shaper_time_slot_get, dev_id, timeslot)
    EXPORT_SYMBOL(fal_flow_shaper_timeslot_get);

sw_error_t fal_port_shaper_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot) 
    DEFINE_FAL_FUNC_ADPT(port_shaper_time_slot_get, dev_id, timeslot)
    EXPORT_SYMBOL(fal_port_shaper_timeslot_get);

sw_error_t fal_queue_shaper_token_number_get(a_uint32_t dev_id, a_uint32_t queue_id, fal_shaper_token_number_t *token_number) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_shaper_token_number_get, dev_id, queue_id, token_number)

sw_error_t fal_queue_shaper_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot) 
    DEFINE_FAL_FUNC_ADPT(queue_shaper_time_slot_get, dev_id, timeslot)
    EXPORT_SYMBOL(fal_queue_shaper_timeslot_get);

sw_error_t fal_port_shaper_token_number_get(a_uint32_t dev_id, fal_port_t port_id, fal_shaper_token_number_t *token_number) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_shaper_token_number_get, dev_id, port_id, token_number)

sw_error_t fal_flow_shaper_token_number_get(a_uint32_t dev_id, a_uint32_t flow_id, fal_shaper_token_number_t *token_number) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_shaper_token_number_get, dev_id, flow_id, token_number)

sw_error_t fal_flow_shaper_get(a_uint32_t dev_id, a_uint32_t flow_id, fal_shaper_config_t * shaper) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_shaper_get, dev_id, flow_id, shaper)

sw_error_t fal_shaper_ipg_preamble_length_get(a_uint32_t dev_id, a_uint32_t *ipg_pre_length) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(shaper_ipg_preamble_length_get, dev_id, ipg_pre_length)

sw_error_t fal_queue_shaper_ctrl_get(a_uint32_t dev_id, fal_shaper_ctrl_t *queue_shaper_ctrl) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(queue_shaper_ctrl_get, dev_id, queue_shaper_ctrl)

sw_error_t fal_flow_shaper_ctrl_get(a_uint32_t dev_id, fal_shaper_ctrl_t *flow_shaper_ctrl) 
    DEFINE_FAL_FUNC_ADPT_EXPORT(flow_shaper_ctrl_get, dev_id, flow_shaper_ctrl)
#endif

