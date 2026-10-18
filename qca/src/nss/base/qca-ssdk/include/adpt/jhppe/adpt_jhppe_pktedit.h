/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_JHPPE_PKTEDIT_
#define _ADPT_JHPPE_PKTEDIT_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_pktedit_en_set(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t enable);
sw_error_t
adpt_jhppe_pktedit_en_get(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t *enable);
sw_error_t
adpt_jhppe_rxdesc_cpucode_en_set(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t enable);
sw_error_t
adpt_jhppe_rxdesc_cpucode_en_get(a_uint32_t dev_id, a_uint32_t cpucode, a_bool_t *enable);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
