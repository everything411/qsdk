/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @defgroup fal_interface_ctrl FAL_INTERFACE_CONTROL
 * @{
 */
#include "sw.h"
#include "fal_interface_ctrl.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_interface_mac_mode_set(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_mac_mode_set, dev_id, port_id, config)

sw_error_t fal_interface_mac_mode_get(a_uint32_t dev_id, fal_port_t port_id, fal_mac_config_t * config) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_mac_mode_get, dev_id, port_id, config)

sw_error_t fal_interface_phy_mode_set(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_phy_mode_set, dev_id, phy_id, config)

sw_error_t fal_interface_phy_mode_get(a_uint32_t dev_id, a_uint32_t phy_id, fal_phy_config_t * config) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_phy_mode_get, dev_id, phy_id, config)

sw_error_t fal_interface_fx100_ctrl_set(a_uint32_t dev_id, fal_fx100_ctrl_config_t * config) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_fx100_ctrl_set, dev_id, config)

sw_error_t fal_interface_fx100_ctrl_get(a_uint32_t dev_id, fal_fx100_ctrl_config_t * config) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_fx100_ctrl_get, dev_id, config)

sw_error_t fal_interface_fx100_status_get(a_uint32_t dev_id, a_uint32_t* status) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_fx100_status_get, dev_id, status)

sw_error_t fal_interface_mac06_exch_set(a_uint32_t dev_id, a_bool_t enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_mac06_exch_set, dev_id, enable)

sw_error_t fal_interface_mac06_exch_get(a_uint32_t dev_id, a_bool_t* enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(interface_mac06_exch_get, dev_id, enable)


