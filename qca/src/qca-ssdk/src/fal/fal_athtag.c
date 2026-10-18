/*
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
 * @defgroup fal_athtag FAL_ATHTAG
 * @{
 */
#include "sw.h"
#include "fal_athtag.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_athtag_pri_mapping_set(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_pri_mapping_set, dev_id, direction, pri_mapping)

sw_error_t fal_athtag_pri_mapping_get(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_pri_mapping_t *pri_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_pri_mapping_get, dev_id, direction, pri_mapping)

sw_error_t fal_athtag_port_mapping_set(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_port_mapping_set, dev_id, direction, port_mapping)

sw_error_t fal_athtag_port_mapping_get(a_uint32_t dev_id, fal_direction_t direction, fal_athtag_port_mapping_t *port_mapping)
    DEFINE_FAL_FUNC_ADPT_EXPORT(athtag_port_mapping_get, dev_id, direction, port_mapping)

sw_error_t fal_port_athtag_rx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_rx_set, dev_id, port_id, cfg)

sw_error_t fal_port_athtag_rx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_rx_get, dev_id, port_id, cfg)

sw_error_t fal_port_athtag_tx_set(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_tx_set, dev_id, port_id, cfg)

sw_error_t fal_port_athtag_tx_get(a_uint32_t dev_id, fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(port_athtag_tx_get, dev_id, port_id, cfg)


