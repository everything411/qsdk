/*
 * Copyright (c) 2012, 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_trunk_legacy FAL_TRUNK_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_trunk_legacy.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_trunk_manipulate_sa_set(a_uint32_t dev_id, fal_mac_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(trunk_manipulate_sa_set, dev_id, addr)

sw_error_t fal_trunk_manipulate_sa_get(a_uint32_t dev_id, fal_mac_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(trunk_manipulate_sa_get, dev_id, addr)

/**
 * @}
 */
