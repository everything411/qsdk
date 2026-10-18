/*
 * Copyright (c) 2012, 2017, 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
* @defgroup fal_gen FAL_MISC
* @{
*/
#include "sw.h"
#include "fal_misc.h"
#include "hsl_api.h"
#include "adpt.h"
#include "hsl_phy.h"

sw_error_t fal_intr_port_link_mask_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t intr_mask_flag)
    DEFINE_FAL_PORT_PHY_FUNC(intr_mask_set, dev_id, port_id, intr_mask_flag)

sw_error_t fal_intr_port_link_mask_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask_flag)
    DEFINE_FAL_PORT_PHY_FUNC(intr_mask_get, dev_id, port_id, intr_mask_flag)

sw_error_t fal_intr_port_link_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask_flag)
    DEFINE_FAL_PORT_PHY_FUNC(intr_status_get, dev_id, port_id, intr_mask_flag)

/**
* @}
*/
