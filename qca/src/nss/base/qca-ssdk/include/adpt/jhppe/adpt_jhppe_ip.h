/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#ifndef _ADPT_JHPPE_IP_H_
#define _ADPT_JHPPE_IP_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

sw_error_t
adpt_jhppe_ip_port_vlan_as_flow_key_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_ip_vlan_as_flow_key_t *vlan_mode);
sw_error_t
adpt_jhppe_ip_port_vlan_as_flow_key_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_ip_vlan_as_flow_key_t *vlan_mode);
sw_error_t
adpt_jhppe_ip_intf_vlan_as_flow_key_set(a_uint32_t dev_id, a_uint32_t intf,
		fal_ip_vlan_as_flow_key_t *vlan_mode);
sw_error_t
adpt_jhppe_ip_intf_vlan_as_flow_key_get(a_uint32_t dev_id, a_uint32_t intf,
		fal_ip_vlan_as_flow_key_t *vlan_mode);
sw_error_t adpt_jhppe_ip_vsi_sg_cfg_get(a_uint32_t dev_id, a_uint32_t vsi,
		fal_sg_cfg_t *sg_cfg);
sw_error_t adpt_jhppe_ip_vsi_sg_cfg_set(a_uint32_t dev_id, a_uint32_t vsi,
		fal_sg_cfg_t *sg_cfg);
sw_error_t adpt_jhppe_ip_port_sg_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_sg_cfg_t *sg_cfg);
sw_error_t adpt_jhppe_ip_port_sg_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_sg_cfg_t *sg_cfg);
sw_error_t adpt_jhppe_ip_vsi_arp_sg_cfg_get(a_uint32_t dev_id, a_uint32_t vsi,
		fal_arp_sg_cfg_t *arp_sg_cfg);
sw_error_t adpt_jhppe_ip_vsi_arp_sg_cfg_set(a_uint32_t dev_id, a_uint32_t vsi,
		fal_arp_sg_cfg_t *arp_sg_cfg);
sw_error_t adpt_jhppe_ip_port_arp_sg_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_arp_sg_cfg_t *arp_sg_cfg);
sw_error_t adpt_jhppe_ip_port_arp_sg_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_arp_sg_cfg_t *arp_sg_cfg);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif
