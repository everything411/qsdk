/*
 * Copyright (c) 2012, 2015-2019, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_port_ctrl_legacy FAL_PORT_CONTROL_LEGACY
 * @{
 */
#ifndef _FAL_PORTCTRL_LEGACY_H_
#define _FAL_PORTCTRL_LEGACY_H_

#ifdef __cplusplus
extern "c" {
#endif

#include "sw.h"
#include "fal/fal_type.h"

typedef enum {
		FAL_NO_HEADER_EN = 0,
		FAL_ONLY_MANAGE_FRAME_EN,
		FAL_ALL_TYPE_FRAME_EN
} fal_port_header_mode_t;

#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_hdr_status_set(a_uint32_t dev_id, fal_port_t port_id,
				    a_bool_t enable);

sw_error_t
fal_port_hdr_status_get(a_uint32_t dev_id, fal_port_t port_id,
				    a_bool_t * enable);
#endif

sw_error_t
fal_port_rxhdr_mode_set(a_uint32_t dev_id, fal_port_t port_id,
				    fal_port_header_mode_t mode);

#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_rxhdr_mode_get(a_uint32_t dev_id, fal_port_t port_id,
				    fal_port_header_mode_t * mode);
#endif

sw_error_t
fal_port_txhdr_mode_set(a_uint32_t dev_id, fal_port_t port_id,
				    fal_port_header_mode_t mode);

#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_txhdr_mode_get(a_uint32_t dev_id, fal_port_t port_id,
				    fal_port_header_mode_t * mode);
#endif

sw_error_t
fal_header_type_set(a_uint32_t dev_id, a_bool_t enable,
				a_uint32_t type);

sw_error_t
fal_header_type_get(a_uint32_t dev_id, a_bool_t * enable,
				a_uint32_t * type);

sw_error_t
fal_port_link_forcemode_set(a_uint32_t dev_id, fal_port_t port_id,
					a_bool_t enable);

sw_error_t
fal_port_link_forcemode_get(a_uint32_t dev_id, fal_port_t port_id,
					a_bool_t * enable);

#ifndef IN_PORTCONTROL_MINI
sw_error_t
fal_port_bp_status_set(a_uint32_t dev_id, fal_port_t port_id,
				   a_bool_t enable);

sw_error_t
fal_port_bp_status_get(a_uint32_t dev_id, fal_port_t port_id,
				   a_bool_t * enable);

sw_error_t
fal_port_congestion_drop_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t queue_id, a_bool_t enable);

sw_error_t
fal_port_congestion_drop_get(a_uint32_t dev_id, fal_port_t port_id,
					 a_uint32_t queue_id,
					 a_bool_t * enable);

sw_error_t
fal_ring_union_set(a_uint32_t dev_id, a_bool_t en);

sw_error_t
fal_ring_union_get(a_uint32_t dev_id, a_bool_t *en);

sw_error_t
fal_vch_bp_status_get(a_uint32_t dev_id, a_uint32_t vch_id, a_bool_t *status);
#endif

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _FAL_PORTCTRL_LEGACY_H_ */
/**
 * @}
 */
