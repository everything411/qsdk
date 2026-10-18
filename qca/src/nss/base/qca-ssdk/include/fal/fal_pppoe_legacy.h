/*
 * Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_pppoe_legacy FAL_PPPOE_LEGACY
 * @{
 */
#ifndef _FAL_PPPOE_LEGACY_H_
#define _FAL_PPPOE_LEGACY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

sw_error_t
fal_pppoe_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);

sw_error_t
fal_pppoe_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);

sw_error_t
fal_pppoe_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_pppoe_status_get(a_uint32_t dev_id, a_bool_t * enable);

sw_error_t
fal_pppoe_session_add(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t strip_hdr);

sw_error_t
fal_pppoe_session_del(a_uint32_t dev_id, a_uint32_t session_id);

sw_error_t
fal_pppoe_session_get(a_uint32_t dev_id, a_uint32_t session_id, a_bool_t * strip_hdr);

sw_error_t
fal_pppoe_session_id_set(a_uint32_t dev_id, a_uint32_t index,
                         a_uint32_t id);

sw_error_t
fal_pppoe_session_id_get(a_uint32_t dev_id, a_uint32_t index,
                         a_uint32_t * id);

sw_error_t
fal_rtd_pppoe_en_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
fal_rtd_pppoe_en_get(a_uint32_t dev_id, a_bool_t * enable);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_PPPOE_LEGACY_H_ */
/**
 * @}
 */
