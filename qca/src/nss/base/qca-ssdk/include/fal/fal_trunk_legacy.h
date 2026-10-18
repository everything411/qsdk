/*
 * Copyright (c) 2012, 2016-2018, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_trunk_legacy FAL_TRUNK_LEGACY
 * @{
 */
#ifndef _FAL_TRUNK_LEGACY_H_
#define _FAL_TRUNK_LEGACY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

sw_error_t
fal_trunk_manipulate_sa_set(a_uint32_t dev_id, fal_mac_addr_t * addr);

sw_error_t
fal_trunk_manipulate_sa_get(a_uint32_t dev_id, fal_mac_addr_t * addr);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_TRUNK_LEGACY_H_ */

/**
 * @}
 */
