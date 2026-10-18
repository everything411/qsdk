/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_HTTPPE_VSI_H_
#define _ADPT_HTTPPE_VSI_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_httppe_port_vlan_vsi_get(a_uint32_t dev_id, fal_port_t port_id,
				a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t *vsi_id);
sw_error_t
adpt_httppe_port_vlan_vsi_set(a_uint32_t dev_id, fal_port_t port_id,
				a_uint32_t stag_vid, a_uint32_t ctag_vid, a_uint32_t vsi_id);
sw_error_t
adpt_httppe_vsi_stamove_set(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_stamove_t *stamove);
sw_error_t
adpt_httppe_vsi_stamove_get(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_stamove_t *stamove);
sw_error_t
adpt_httppe_vsi_newaddr_lrn_get(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_newaddr_lrn_t *newaddr_lrn);
sw_error_t
adpt_httppe_vsi_newaddr_lrn_set(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_newaddr_lrn_t *newaddr_lrn);
sw_error_t
adpt_httppe_vsi_member_set(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_member_t *vsi_member);
sw_error_t
adpt_httppe_vsi_member_get(a_uint32_t dev_id, a_uint32_t vsi_id, fal_vsi_member_t *vsi_member);
sw_error_t
adpt_httppe_vsi_bridge_vsi_get(a_uint32_t dev_id, a_uint32_t vsi_id,
	fal_vsi_bridge_vsi_t *bridge_vsi);
sw_error_t
adpt_httppe_vsi_bridge_vsi_set(a_uint32_t dev_id, a_uint32_t vsi_id,
	fal_vsi_bridge_vsi_t *bridge_vsi);
sw_error_t
adpt_httppe_vsi_invalidvsi_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_vsi_invalidvsi_ctrl_t *invalidvsi_ctrl);
sw_error_t
adpt_httppe_vsi_invalidvsi_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_vsi_invalidvsi_ctrl_t *invalidvsi_ctrl);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
