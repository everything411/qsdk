/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_PORTCTRLH_
#define _ADPT_JHPPE_PORTCTRLH_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#define JHPPE_LOOPBACK_PORT_NUM                 0x1

sw_error_t
adpt_jhppe_switch_port_loopback_set(a_uint32_t dev_id,
	fal_port_t port_id, fal_loopback_config_t *loopback_cfg);

sw_error_t
adpt_jhppe_switch_port_loopback_get(a_uint32_t dev_id,
	fal_port_t port_id, fal_loopback_config_t *loopback_cfg);

sw_error_t
adpt_jhppe_switch_port_loopback_flowctrl_set(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t enable);

sw_error_t
adpt_jhppe_switch_port_loopback_flowctrl_get(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t *enable);

sw_error_t
adpt_jhppe_lpbk_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t *max_frame);

sw_error_t
adpt_jhppe_lpbk_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t max_frame);

sw_error_t
adpt_jhppe_switch_loopback_port_get(a_uint32_t dev_id, fal_port_t *port_id);
#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
