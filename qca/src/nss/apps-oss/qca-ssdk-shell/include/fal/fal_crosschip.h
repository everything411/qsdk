/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _FAL_CROSSCHIP_H_
#define _FAL_CROSSCHIP_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"

typedef enum {
	FAL_CROSSCHIP_BP_DISABLE = 0,
	FAL_CROSSCHIP_BP_EDMA = 1,
	FAL_CROSSCHIP_BP_QUEUE = 2,
} fal_crosschip_bp_mode_t;

sw_error_t
fal_crosschip_bp_mode_set(a_uint32_t dev_id, fal_crosschip_bp_mode_t mode);

sw_error_t
fal_crosschip_bp_mode_get(a_uint32_t dev_id, fal_crosschip_bp_mode_t *mode);

sw_error_t
fal_crosschip_bp_en_set(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t enable);

sw_error_t
fal_crosschip_bp_en_get(a_uint32_t dev_id, a_uint32_t queue_id, a_bool_t *enable);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_CROSSCHIP_H_ */
/**
 * @}
 */

