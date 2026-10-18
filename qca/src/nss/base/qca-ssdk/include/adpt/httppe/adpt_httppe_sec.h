/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_HTTPPE_SEC_H_
#define _ADPT_HTTPPE_SEC_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#ifndef IN_SEC_MINI
sw_error_t
adpt_httppe_sec_l3_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);

sw_error_t
adpt_httppe_sec_l3_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l3_excep_parser_ctrl *ctrl);
#endif

sw_error_t
adpt_httppe_sec_l4_excep_parser_ctrl_set(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

sw_error_t
adpt_httppe_sec_l4_excep_parser_ctrl_get(a_uint32_t dev_id, fal_l4_excep_parser_ctrl *ctrl);

sw_error_t
adpt_httppe_sec_l3_excep_ctrl_set(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl);

sw_error_t
adpt_httppe_sec_l3_excep_ctrl_get(a_uint32_t dev_id, a_uint32_t excep_type, fal_l3_excep_ctrl_t *ctrl);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
/**
 * @}
 */
