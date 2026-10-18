/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "adpt.h"
#include "adpt_appe_servcode.h"

sw_error_t
adpt_appe_servcode_tl_config_set(a_uint32_t dev_id, a_uint32_t servcode_index,
					fal_servcode_config_t *entry)
{
	union tl_service_tbl_u tl_service_tbl = {0};

	SW_RTN_ON_ERROR(appe_tl_service_tbl_get(dev_id, servcode_index, &tl_service_tbl));

	tl_service_tbl.bf.bypass_bitmap = entry->bypass_bitmap[3];

	SW_RTN_ON_ERROR(appe_tl_service_tbl_set(dev_id, servcode_index, &tl_service_tbl));
	return SW_OK;
}

sw_error_t
adpt_appe_servcode_tl_config_get(a_uint32_t dev_id, a_uint32_t servcode_index,
					fal_servcode_config_t *entry)
{
	union tl_service_tbl_u tl_service_tbl = {0};

	SW_RTN_ON_ERROR(appe_tl_service_tbl_get(dev_id, servcode_index, &tl_service_tbl));

	entry->bypass_bitmap[3] = tl_service_tbl.bf.bypass_bitmap;
	return SW_OK;
}

/**
 * @}
 */
