/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_MIB_H_
#define _ADPT_JHPPE_MIB_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_lpbk_mib_cpukeep_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
adpt_jhppe_lpbk_mib_status_set(a_uint32_t dev_id, a_bool_t enable);

sw_error_t
adpt_jhppe_lpbk_mib_flush_counters(a_uint32_t dev_id,
	fal_port_t port_id);

sw_error_t
adpt_jhppe_lpbk_get_mib_info(a_uint32_t dev_id, fal_port_t port_id,
	fal_mib_info_t * mib_info );

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
