/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022, 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @defgroup fal_policer FAL_POLICER
 * @{
 */
#include "sw.h"
#include "fal_policer.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_acl_policer_counter_get(a_uint32_t dev_id, a_uint32_t index, fal_policer_counter_t *counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_policer_counter_get, dev_id, index, counter)

sw_error_t fal_port_policer_counter_get(a_uint32_t dev_id, fal_port_t port_id, fal_policer_counter_t *counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_policer_counter_get, dev_id, port_id, counter)

sw_error_t fal_port_policer_entry_get(a_uint32_t dev_id, fal_port_t port_id, fal_policer_config_t *policer, fal_policer_action_t *action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_policer_entry_get, dev_id, port_id, policer, action)

sw_error_t fal_acl_policer_entry_get(a_uint32_t dev_id, a_uint32_t index, fal_policer_config_t *policer, fal_policer_action_t *action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_policer_entry_get, dev_id, index, policer, action)

sw_error_t fal_port_policer_entry_set(a_uint32_t dev_id, fal_port_t port_id, fal_policer_config_t *policer, fal_policer_action_t *action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_policer_entry_set, dev_id, port_id, policer, action)

sw_error_t fal_acl_policer_entry_set(a_uint32_t dev_id, a_uint32_t index, fal_policer_config_t *policer, fal_policer_action_t *action)
    DEFINE_FAL_FUNC_ADPT_EXPORT(acl_policer_entry_set, dev_id, index, policer, action)

sw_error_t fal_policer_timeslot_get(a_uint32_t dev_id, a_uint32_t *timeslot)
    DEFINE_FAL_FUNC_ADPT(policer_time_slot_get, dev_id, timeslot)
    EXPORT_SYMBOL(fal_policer_timeslot_get);

sw_error_t fal_policer_bypass_en_get(a_uint32_t dev_id, fal_policer_frame_type_t frame_type, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_bypass_en_get, dev_id, frame_type, enable)

sw_error_t fal_policer_timeslot_set(a_uint32_t dev_id, a_uint32_t timeslot)
    DEFINE_FAL_FUNC_ADPT(policer_time_slot_set, dev_id, timeslot)
    EXPORT_SYMBOL(fal_policer_timeslot_set);

sw_error_t fal_policer_bypass_en_set(a_uint32_t dev_id, fal_policer_frame_type_t frame_type, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_bypass_en_set, dev_id, frame_type, enable)

sw_error_t fal_port_policer_compensation_byte_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *length)
    DEFINE_FAL_FUNC_ADPT(port_compensation_byte_get, dev_id, port_id, length)
    EXPORT_SYMBOL(fal_port_policer_compensation_byte_get);

sw_error_t fal_port_policer_compensation_byte_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t length)
    DEFINE_FAL_FUNC_ADPT(port_compensation_byte_set, dev_id, port_id, length)
    EXPORT_SYMBOL(fal_port_policer_compensation_byte_set);

sw_error_t fal_policer_ctrl_get(a_uint32_t dev_id, fal_policer_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_ctrl_get, dev_id, ctrl)

sw_error_t fal_policer_ctrl_set(a_uint32_t dev_id, fal_policer_ctrl_t *ctrl)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_ctrl_set, dev_id, ctrl)

#ifndef IN_POLICER_MINI
sw_error_t fal_policer_global_counter_get(a_uint32_t dev_id, fal_policer_global_counter_t *counter)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_global_counter_get, dev_id, counter)

sw_error_t fal_policer_priority_remap_get(a_uint32_t dev_id, fal_policer_priority_t *priority, fal_policer_remap_t *remap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_priority_remap_get, dev_id, priority, remap)

sw_error_t fal_policer_priority_remap_set(a_uint32_t dev_id, fal_policer_priority_t *priority, fal_policer_remap_t *remap)
    DEFINE_FAL_FUNC_ADPT_EXPORT(policer_priority_remap_set, dev_id, priority, remap)
#endif

