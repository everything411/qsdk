/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */

#ifndef _ADPT_JHPPE_UNIPHY_H_
#define _ADPT_JHPPE_UNIPHY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "sw.h"

sw_error_t
adpt_jhppe_uniphy_rxeq_status_check(a_uint32_t dev_id, a_uint32_t uniphy_index);

sw_error_t
adpt_jhppe_uniphy_calibrate(a_uint32_t dev_id, a_uint32_t uniphy_index);

sw_error_t
adpt_jhppe_uniphy_25g_r_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index);

sw_error_t
adpt_jhppe_uniphy_pma_init_setting(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t uniphy_mode, a_uint32_t dfe_mode, a_bool_t is_long);

sw_error_t
adpt_jhppe_uniphy_pma_dfe_sw_tune(a_uint32_t dev_id, a_uint32_t uniphy_index);

sw_error_t
adpt_jhppe_uniphy_fec_set(a_uint32_t dev_id, a_uint32_t uniphy_index, a_uint32_t active_fec);

sw_error_t
adpt_jhppe_uniphy_25gr_status_check(a_uint32_t dev_id, a_uint32_t uniphy_index, a_bool_t *status);

sw_error_t
adpt_jhppe_uniphy_xlgpcs_soft_reset(a_uint32_t dev_id, a_uint32_t uniphy_index);
#ifdef __cplusplus
}
#endif

#endif /* _ADPT_JHPPE_UNIPHY_H_ */
