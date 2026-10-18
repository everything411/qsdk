/*
 * Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2021,2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @defgroup fal_pppoe FAL_PPPOE
 * @{
 */
#include "sw.h"
#include "fal_pppoe.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_cmd_set, dev_id, cmd)

sw_error_t fal_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_cmd_get, dev_id, cmd)

sw_error_t fal_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_status_set, dev_id, enable)

sw_error_t fal_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_status_get, dev_id, enable)

sw_error_t fal_pppoe_session_add(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t strip_hdr)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_add, dev_id, session_id, strip_hdr)

sw_error_t fal_pppoe_session_del(a_uint32_t dev_id, a_uint32_t session_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_del, dev_id, session_id)

sw_error_t fal_pppoe_session_get(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t * strip_hdr)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_get, dev_id, session_id, strip_hdr)

sw_error_t fal_pppoe_session_table_add(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
    DEFINE_FAL_FUNC_EXPORT(pppoe_session_table_add, dev_id, session_tbl)

sw_error_t fal_pppoe_session_table_del(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
    DEFINE_FAL_FUNC_EXPORT(pppoe_session_table_del, dev_id, session_tbl)

sw_error_t fal_pppoe_session_table_get(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
    DEFINE_FAL_FUNC_EXPORT(pppoe_session_table_get, dev_id, session_tbl)

sw_error_t fal_pppoe_session_id_set(a_uint32_t dev_id, a_uint32_t index, a_uint32_t id)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_id_set, dev_id, index, id)

sw_error_t fal_pppoe_session_id_get(a_uint32_t dev_id, a_uint32_t index, a_uint32_t * id)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_id_get, dev_id, index, id)

sw_error_t fal_rtd_pppoe_en_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(rtd_pppoe_en_set, dev_id, enable)

sw_error_t fal_rtd_pppoe_en_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(rtd_pppoe_en_get, dev_id, enable)

sw_error_t fal_pppoe_l3intf_status_get(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t *enable)
    DEFINE_FAL_FUNC_ADPT(pppoe_en_get, dev_id, l3_if, enable)
    EXPORT_SYMBOL(fal_pppoe_l3intf_status_get);

sw_error_t fal_pppoe_l3intf_enable(a_uint32_t dev_id, a_uint32_t l3_if, a_uint32_t enable)
    DEFINE_FAL_FUNC_ADPT(pppoe_en_set, dev_id, l3_if, enable)
    EXPORT_SYMBOL(fal_pppoe_l3intf_enable);

sw_error_t fal_pppoe_l3_intf_set(a_uint32_t dev_id, a_uint32_t pppoe_index, fal_intf_type_t l3_type, fal_intf_id_t *pppoe_intf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pppoe_l3_intf_set, dev_id, pppoe_index, l3_type, pppoe_intf)

sw_error_t fal_pppoe_l3_intf_get(a_uint32_t dev_id, a_uint32_t pppoe_index, fal_intf_type_t l3_type, fal_intf_id_t *pppoe_intf)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pppoe_l3_intf_get, dev_id, pppoe_index, l3_type, pppoe_intf)

sw_error_t fal_pppoe_global_ctrl_set(a_uint32_t dev_id, fal_pppoe_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pppoe_global_ctrl_set, dev_id, cfg)

sw_error_t fal_pppoe_global_ctrl_get(a_uint32_t dev_id, fal_pppoe_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(pppoe_global_ctrl_get, dev_id, cfg)

