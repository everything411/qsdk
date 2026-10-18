/*
 * Copyright (c) 2012, 2016-2018, 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_port_vlan_legacy FAL_PORT_VLAN_LEGACY
 * @{
 */
#ifndef _FAL_PORT_VLAN_LEGACY_H_
#define _FAL_PORT_VLAN_LEGACY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

sw_error_t fal_port_vlan_propagation_set(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_propagation_mode_t mode);

sw_error_t fal_port_egvlanmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1q_egmode_t port_egvlanmode);

sw_error_t fal_port_egvlanmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1q_egmode_t * pport_egvlanmode);

sw_error_t fal_port_1qmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1qmode_t port_1qmode);

sw_error_t fal_port_1qmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1qmode_t * pport_1qmode);

sw_error_t fal_port_default_svid_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vid);

sw_error_t fal_port_default_svid_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vid);

sw_error_t fal_port_default_cvid_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vid);

sw_error_t fal_port_default_cvid_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vid);

sw_error_t fal_port_tls_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

sw_error_t fal_nestvlan_tpid_set(a_uint32_t dev_id, a_uint32_t tpid);

sw_error_t fal_nestvlan_tpid_get(a_uint32_t dev_id, a_uint32_t * tpid);

sw_error_t fal_port_force_default_vid_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

sw_error_t fal_port_force_default_vid_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

sw_error_t fal_port_force_portvlan_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

sw_error_t fal_port_force_portvlan_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

sw_error_t fal_port_tls_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

sw_error_t fal_port_pri_propagation_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

sw_error_t fal_port_pri_propagation_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

sw_error_t fal_port_vlan_propagation_get(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_propagation_mode_t * mode);

sw_error_t fal_port_mac_vlan_xlt_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);

sw_error_t fal_port_mac_vlan_xlt_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);

sw_error_t fal_netisolate_set(a_uint32_t dev_id, a_uint32_t enable);

sw_error_t fal_netisolate_get(a_uint32_t dev_id, a_uint32_t * enable);

sw_error_t fal_eg_trans_filter_bypass_en_set(a_uint32_t dev_id, a_uint32_t enable);

sw_error_t fal_eg_trans_filter_bypass_en_get(a_uint32_t dev_id, a_uint32_t* enable);

sw_error_t fal_port_vrf_id_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vrf_id);

sw_error_t fal_port_vrf_id_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vrf_id);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_PORT_VLAN_LEGACY_H_ */
/**
 * @}
 */
