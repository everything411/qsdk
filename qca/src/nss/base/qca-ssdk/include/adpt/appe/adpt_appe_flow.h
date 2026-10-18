/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_APPE_FLOW_
#define _ADPT_APPE_FLOW_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_appe_flow_copy_escape_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
adpt_appe_flow_copy_escape_get(a_uint32_t dev_id, a_bool_t *enable);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
