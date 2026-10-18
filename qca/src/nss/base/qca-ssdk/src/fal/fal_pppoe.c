/*
 * Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_pppoe FAL_PPPOE
 * @{
 */
#include "sw.h"
#include "fal_pppoe.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_pppoe_session_table_add(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
    DEFINE_FAL_FUNC_EXPORT(pppoe_session_table_add, dev_id, session_tbl)

sw_error_t fal_pppoe_session_table_del(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
    DEFINE_FAL_FUNC_EXPORT(pppoe_session_table_del, dev_id, session_tbl)

sw_error_t fal_pppoe_session_table_get(a_uint32_t dev_id, fal_pppoe_session_t * session_tbl)
    DEFINE_FAL_FUNC_EXPORT(pppoe_session_table_get, dev_id, session_tbl)

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

