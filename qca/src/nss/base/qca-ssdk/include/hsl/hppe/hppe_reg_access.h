/*
 * Copyright (c) 2014-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _HPPE_REG_ACCESS_H_
#define _HPPE_REG_ACCESS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "sw.h"
#include "hsl_dev.h"

sw_error_t hppe_reg_get(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint32_t *val);
sw_error_t hppe_reg_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint32_t val);
sw_error_t hppe_reg_tbl_get(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint32_t *val, a_uint32_t num);
sw_error_t hppe_reg_tbl_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint32_t *val, a_uint32_t num);
sw_error_t hppe_uniphy_reg_get(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint32_t index, a_uint32_t *val);
sw_error_t hppe_uniphy_reg_set(a_uint32_t dev_id, a_uint32_t reg_addr, a_uint32_t index, a_uint32_t val);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _HPPE_REG_ACCESS_H_ */
