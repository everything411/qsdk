/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#ifndef _ADPT_JHPPE_QOS_
#define _ADPT_JHPPE_QOS_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_qos_port_pcp_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_qos_pcp_cfg_t *pcp_cfg);
sw_error_t
adpt_jhppe_qos_port_pcp_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_qos_pcp_cfg_t *pcp_cfg);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
