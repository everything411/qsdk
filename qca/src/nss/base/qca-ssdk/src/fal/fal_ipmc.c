/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw.h"
#include "fal_ipmc.h"
#include "adpt.h"

sw_error_t fal_ipmc_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_status_set, dev_id, enable)

sw_error_t fal_ipmc_status_get(a_uint32_t dev_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_status_get, dev_id, enable)

sw_error_t fal_ipmc_global_cfg_set(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_global_cfg_set, dev_id, cfg)

sw_error_t fal_ipmc_global_cfg_get(a_uint32_t dev_id, fal_ipmc_global_cfg_t *cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_global_cfg_get, dev_id, cfg)

sw_error_t fal_ipmc_entry_add(a_uint32_t dev_id, fal_ipmc_op_mode_t add_mode, fal_ipmc_entry_t *ipmc_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_entry_add, dev_id, add_mode, ipmc_entry)

sw_error_t fal_ipmc_entry_del(a_uint32_t dev_id, fal_ipmc_op_mode_t del_mode, fal_ipmc_entry_t *ipmc_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_entry_del, dev_id, del_mode, ipmc_entry)

sw_error_t fal_ipmc_entry_get(a_uint32_t dev_id, fal_ipmc_op_mode_t get_mode, fal_ipmc_entry_t *ipmc_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_entry_get, dev_id, get_mode, ipmc_entry)

sw_error_t fal_ipmc_entry_getnext(a_uint32_t dev_id, fal_ipmc_next_mode_t next_mode, fal_ipmc_entry_t *ipmc_entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_entry_getnext, dev_id, next_mode, ipmc_entry)

sw_error_t fal_ipmc_entry_flush(a_uint32_t dev_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_entry_flush, dev_id)

sw_error_t fal_ipmc_ucast_fwd_set(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_ucast_fwd_set, dev_id, ucast_fwd)

sw_error_t fal_ipmc_ucast_fwd_get(a_uint32_t dev_id, fal_ipmc_ucast_fwd_t *ucast_fwd)
    DEFINE_FAL_FUNC_ADPT_EXPORT(ipmc_ucast_fwd_get, dev_id, ucast_fwd)
