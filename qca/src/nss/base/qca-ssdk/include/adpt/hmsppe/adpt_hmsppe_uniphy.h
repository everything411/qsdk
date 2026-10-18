/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_HMSPPE_UNIPHY_H_
#define _ADPT_HMSPPE_UNIPHY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_hmsppe_uniphy_pon_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
	a_uint32_t mode);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
