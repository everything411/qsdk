/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _ADPT_APPE_PORTVLAN_H_
#define _ADPT_APPE_PORTVLAN_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_appe_port_vlan_vpgroup_set(a_uint32_t dev_id, a_uint32_t vport_id,
		fal_port_vlan_direction_t direction, a_uint32_t vpgroup_id);
sw_error_t
adpt_appe_port_vlan_vpgroup_get(a_uint32_t dev_id, a_uint32_t vport_id,
		fal_port_vlan_direction_t direction, a_uint32_t *vpgroup_id);
#ifndef IN_PORTVLAN_MINI
sw_error_t
adpt_appe_port_egress_vlan_filter_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter);
sw_error_t
adpt_appe_port_egress_vlan_filter_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_egress_vlan_filter_t *filter);
#endif

sw_error_t
adpt_appe_port_isol_group_get(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp);
sw_error_t
adpt_appe_port_isol_group_set(a_uint32_t dev_id,
		a_uint8_t isol_group_id, a_uint64_t *isol_group_bmp);
sw_error_t
adpt_appe_port_isol_ctrl_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);
sw_error_t
adpt_appe_port_isol_ctrl_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_port_isol_ctrl_t *isol_ctrl);
sw_error_t
adpt_appe_portvlan_vpmember_get(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t * mem_port_map);
sw_error_t
adpt_appe_portvlan_vpmember_update(a_uint32_t dev_id, fal_port_t port_id, fal_pbmp_t mem_port_map);
sw_error_t
adpt_appe_portvlan_vpmember_add(a_uint32_t dev_id, fal_port_t port_id, fal_port_t mem_port_id);
sw_error_t
adpt_appe_portvlan_vpmember_del(a_uint32_t dev_id, fal_port_t port_id, fal_port_t mem_port_id);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif
