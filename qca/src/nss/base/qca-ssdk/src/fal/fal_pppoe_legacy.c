/*
 * Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_pppoe_legacy FAL_PPPOE_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_pppoe_legacy.h"
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

sw_error_t fal_pppoe_session_id_set(a_uint32_t dev_id, a_uint32_t index, a_uint32_t id)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_id_set, dev_id, index, id)

sw_error_t fal_pppoe_session_id_get(a_uint32_t dev_id, a_uint32_t index, a_uint32_t * id)
    DEFINE_FAL_FUNC_HSL_EXPORT(pppoe_session_id_get, dev_id, index, id)

sw_error_t fal_rtd_pppoe_en_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(rtd_pppoe_en_set, dev_id, enable)

sw_error_t fal_rtd_pppoe_en_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(rtd_pppoe_en_get, dev_id, enable)

/**
 * @}
 */
