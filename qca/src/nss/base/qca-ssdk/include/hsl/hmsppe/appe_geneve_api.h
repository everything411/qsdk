/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _APPE_GENEVE_API_H_
#define _APPE_GENEVE_API_H_


sw_error_t
appe_tpr_geneve_cfg_get(
		a_uint32_t dev_id,
		union tpr_geneve_cfg_u *value);

sw_error_t
appe_tpr_geneve_cfg_set(
		a_uint32_t dev_id,
		union tpr_geneve_cfg_u *value);

sw_error_t
appe_tpr_geneve_cfg_udp_port_map_get(
		a_uint32_t dev_id,
		unsigned int *value);

sw_error_t
appe_tpr_geneve_cfg_udp_port_map_set(
		a_uint32_t dev_id,
		unsigned int value);

#endif
