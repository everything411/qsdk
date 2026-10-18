/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_HTTPPE_VPORT_H_
#define _ADPT_HTTPPE_VPORT_H_

sw_error_t
adpt_httppe_vport_physical_port_id_get(a_uint32_t dev_id,
		fal_port_t vport_id, fal_port_t *phyport_id);
sw_error_t
adpt_httppe_vport_physical_port_id_set(a_uint32_t dev_id,
		fal_port_t vport_id, fal_port_t phyport_id);
sw_error_t
adpt_httppe_vport_state_check_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_vport_state_t *vp_state);
sw_error_t
adpt_httppe_vport_state_check_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_vport_state_t *vp_state);
#endif
