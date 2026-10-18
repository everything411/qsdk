/*
 * Copyright (c) 2012, 2016-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_port_vlan_legacy FAL_PORT_VLAN_LEGACY
 * @{
 */
#include "sw.h"
#include "fal_portvlan_legacy.h"
#include "hsl_api.h"
#include "adpt.h"

#include <linux/kernel.h>
#include <linux/module.h>

sw_error_t fal_port_vlan_propagation_set(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_propagation_mode_t mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vlan_propagation_set, dev_id, port_id, mode)

sw_error_t fal_port_egvlanmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1q_egmode_t port_egvlanmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_egvlanmode_set, dev_id, port_id, port_egvlanmode)

sw_error_t fal_port_egvlanmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1q_egmode_t * pport_egvlanmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_egvlanmode_get, dev_id, port_id, pport_egvlanmode)

sw_error_t fal_port_1qmode_set(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1qmode_t port_1qmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_1qmode_set, dev_id, port_id, port_1qmode)

sw_error_t fal_port_1qmode_get(a_uint32_t dev_id, fal_port_t port_id, fal_pt_1qmode_t * pport_1qmode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_1qmode_get, dev_id, port_id, pport_1qmode)

sw_error_t fal_port_default_svid_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_svid_set, dev_id, port_id, vid)

sw_error_t fal_port_default_svid_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_svid_get, dev_id, port_id, vid)

sw_error_t fal_port_default_cvid_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_cvid_set, dev_id, port_id, vid)

sw_error_t fal_port_default_cvid_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vid)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_default_cvid_get, dev_id, port_id, vid)

sw_error_t fal_port_tls_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_tls_set, dev_id, port_id, enable)

sw_error_t fal_nestvlan_tpid_set(a_uint32_t dev_id, a_uint32_t tpid)
    DEFINE_FAL_FUNC_HSL_EXPORT(nestvlan_tpid_set, dev_id, tpid)

sw_error_t fal_nestvlan_tpid_get(a_uint32_t dev_id, a_uint32_t * tpid)
    DEFINE_FAL_FUNC_HSL_EXPORT(nestvlan_tpid_get, dev_id, tpid)

sw_error_t fal_port_force_default_vid_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_default_vid_set, dev_id, port_id, enable)

sw_error_t fal_port_force_default_vid_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_default_vid_get, dev_id, port_id, enable)

sw_error_t fal_port_force_portvlan_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_portvlan_set, dev_id, port_id, enable)

sw_error_t fal_port_force_portvlan_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_force_portvlan_get, dev_id, port_id, enable)

sw_error_t fal_port_tls_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_tls_get, dev_id, port_id, enable)

sw_error_t fal_port_pri_propagation_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_pri_propagation_set, dev_id, port_id, enable)

sw_error_t fal_port_pri_propagation_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_pri_propagation_get, dev_id, port_id, enable)

sw_error_t fal_port_vlan_propagation_get(a_uint32_t dev_id, fal_port_t port_id, fal_vlan_propagation_mode_t * mode)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vlan_propagation_get, dev_id, port_id, mode)

sw_error_t fal_port_mac_vlan_xlt_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_mac_vlan_xlt_set, dev_id, port_id, enable)

sw_error_t fal_port_mac_vlan_xlt_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_mac_vlan_xlt_get, dev_id, port_id, enable)

sw_error_t fal_netisolate_set(a_uint32_t dev_id, a_uint32_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(netisolate_set, dev_id, enable)

sw_error_t fal_netisolate_get(a_uint32_t dev_id, a_uint32_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(netisolate_get, dev_id, enable)

sw_error_t fal_eg_trans_filter_bypass_en_set(a_uint32_t dev_id, a_uint32_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(eg_trans_filter_bypass_en_set, dev_id, enable)

sw_error_t fal_eg_trans_filter_bypass_en_get(a_uint32_t dev_id, a_uint32_t* enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(eg_trans_filter_bypass_en_get, dev_id, enable)

sw_error_t fal_port_vrf_id_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vrf_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vrf_id_set, dev_id, port_id, vrf_id)

sw_error_t fal_port_vrf_id_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t * vrf_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_vrf_id_get, dev_id, port_id, vrf_id)

/**
 * @}
 */
