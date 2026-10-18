/*
 * Copyright (c) 2012, 2016-2017, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_sec_legacy FAL_SEC_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_sec_legacy.h"
#include "hsl_api.h"
#include "adpt.h"

#ifndef IN_SEC_MINI
sw_error_t fal_sec_norm_item_set(a_uint32_t dev_id, fal_norm_item_t item, void *value)
    DEFINE_FAL_FUNC_HSL_EXPORT(sec_norm_item_set, dev_id, item, value)

sw_error_t fal_sec_norm_item_get(a_uint32_t dev_id, fal_norm_item_t item, void *value)
    DEFINE_FAL_FUNC_HSL_EXPORT(sec_norm_item_get, dev_id, item, value)
#endif

/**
 * @}
 */
