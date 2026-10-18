/*
 * Copyright (c) 2012, 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_gen FAL_MISC
 * @{
 */
#ifndef _FAL_MISC_H_
#define _FAL_MISC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"
#include "fal_misc_legacy.h"

    typedef enum
    {
        FAL_LOOP_CHECK_1MS = 0,
        FAL_LOOP_CHECK_10MS,
        FAL_LOOP_CHECK_100MS,
        FAL_LOOP_CHECK_500MS,
    } fal_loop_check_time_t;

    /* define switch interrupt type bitmap */
#define FAL_SWITCH_INTR_LINK_STATUS      0x1  /* up/down/speed/duplex status */
#define FAL_SWITCH_INTR_FDB_CHANGE       0x2  /* fdb learn, update and delete aged entry*/

    sw_error_t
    fal_intr_port_link_mask_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t intr_mask);


    sw_error_t
    fal_intr_port_link_mask_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask);


    sw_error_t
    fal_intr_port_link_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_MISC_H_ */
/**
 * @}
 */
