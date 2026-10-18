/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
 * @defgroup fal_leaky FAL_LEAKY
 * @{
 */
#include "sw.h"
#include "fal_leaky.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_uc_leaky_mode_set(a_uint32_t dev_id, fal_leaky_ctrl_mode_t ctrl_mode) 
    DEFINE_FAL_FUNC_HSL_EXPORT(uc_leaky_mode_set, dev_id, ctrl_mode)

sw_error_t fal_uc_leaky_mode_get(a_uint32_t dev_id, fal_leaky_ctrl_mode_t * ctrl_mode) 
    DEFINE_FAL_FUNC_HSL_EXPORT(uc_leaky_mode_get, dev_id, ctrl_mode)

sw_error_t fal_mc_leaky_mode_set(a_uint32_t dev_id, fal_leaky_ctrl_mode_t ctrl_mode) 
    DEFINE_FAL_FUNC_HSL_EXPORT(mc_leaky_mode_set, dev_id, ctrl_mode)

sw_error_t fal_mc_leaky_mode_get(a_uint32_t dev_id, fal_leaky_ctrl_mode_t * ctrl_mode) 
    DEFINE_FAL_FUNC_HSL_EXPORT(mc_leaky_mode_get, dev_id, ctrl_mode)

sw_error_t fal_port_arp_leaky_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(port_arp_leaky_set, dev_id, port_id, enable)

sw_error_t fal_port_arp_leaky_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(port_arp_leaky_get, dev_id, port_id, enable)

sw_error_t fal_port_uc_leaky_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(port_uc_leaky_set, dev_id, port_id, enable)

sw_error_t fal_port_uc_leaky_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(port_uc_leaky_get, dev_id, port_id, enable)

sw_error_t fal_port_mc_leaky_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(port_mc_leaky_set, dev_id, port_id, enable)

sw_error_t fal_port_mc_leaky_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable) 
    DEFINE_FAL_FUNC_HSL_EXPORT(port_mc_leaky_get, dev_id, port_id, enable)

