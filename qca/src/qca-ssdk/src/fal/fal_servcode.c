/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022,2024 Qualcomm Innovation Center, Inc. All rights reserved.
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


/**
 * @defgroup fal_ctrlpkt FAL_SERVCODE
 * @{
 */
#include "sw.h"
#include "fal_servcode.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_servcode_config_set(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_config_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(servcode_config_set, dev_id, servcode_index, entry)

sw_error_t fal_servcode_config_get(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_config_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(servcode_config_get, dev_id, servcode_index, entry)

sw_error_t fal_servcode_loopcheck_en(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(servcode_loopcheck_en, dev_id, enable)

sw_error_t fal_servcode_loopcheck_status_get(a_uint32_t dev_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(servcode_loopcheck_status_get, dev_id, enable)

sw_error_t fal_port_servcode_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t servcode_index)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_servcode_set, dev_id, port_id, servcode_index)

sw_error_t fal_port_servcode_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *servcode_index)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_servcode_get, dev_id, port_id, servcode_index)

sw_error_t fal_servcode_athtag_set(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_athtag_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(servcode_athtag_set, dev_id, servcode_index, entry)

sw_error_t fal_servcode_athtag_get(a_uint32_t dev_id, a_uint32_t servcode_index, fal_servcode_athtag_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(servcode_athtag_get, dev_id, servcode_index, entry)

