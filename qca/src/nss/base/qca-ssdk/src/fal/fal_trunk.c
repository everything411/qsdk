/*
 * Copyright (c) 2012, 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_trunk FAL_TRUNK
 * @{
 */
#include "sw.h"
#include "fal_trunk.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_trunk_group_set(a_uint32_t dev_id, a_uint32_t trunk_id, a_bool_t enable, fal_pbmp_t member)
    DEFINE_FAL_FUNC_EXPORT(trunk_group_set, dev_id, trunk_id, enable, member)

sw_error_t fal_trunk_group_get(a_uint32_t dev_id, a_uint32_t trunk_id, a_bool_t * enable, fal_pbmp_t * member)
    DEFINE_FAL_FUNC_EXPORT(trunk_group_get, dev_id, trunk_id, enable, member)

sw_error_t fal_trunk_hash_mode_set(a_uint32_t dev_id, a_uint32_t hash_mode)
    DEFINE_FAL_FUNC_EXPORT(trunk_hash_mode_set, dev_id, hash_mode)

sw_error_t fal_trunk_hash_mode_get(a_uint32_t dev_id, a_uint32_t * hash_mode)
    DEFINE_FAL_FUNC_EXPORT(trunk_hash_mode_get, dev_id, hash_mode)

sw_error_t fal_trunk_failover_status_get(a_uint32_t dev_id, a_bool_t * failover)
    DEFINE_FAL_FUNC_ADPT(trunk_fail_over_en_get, dev_id, failover)
    EXPORT_SYMBOL(fal_trunk_failover_status_get);

sw_error_t fal_trunk_failover_enable(a_uint32_t dev_id, a_bool_t failover)
    DEFINE_FAL_FUNC_ADPT(trunk_fail_over_en_set, dev_id, failover)
    EXPORT_SYMBOL(fal_trunk_failover_enable);

