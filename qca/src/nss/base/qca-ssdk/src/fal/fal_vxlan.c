/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/**
 * @defgroup fal_vxlan FAL_VXLAN
 * @{
 */
#include "sw.h"
#include "fal_vxlan.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_vxlan_entry_add(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(vxlan_entry_add, dev_id, type, entry)

sw_error_t fal_vxlan_entry_del(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(vxlan_entry_del, dev_id, type, entry)

#ifndef IN_VXLAN_MINI
sw_error_t fal_vxlan_entry_getfirst(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(vxlan_entry_getfirst, dev_id, type, entry)

sw_error_t fal_vxlan_entry_getnext(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(vxlan_entry_getnext, dev_id, type, entry)

sw_error_t fal_vxlan_gpe_proto_cfg_set(a_uint32_t dev_id, fal_vxlan_gpe_proto_cfg_t * proto_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(vxlan_gpe_proto_cfg_set, dev_id, proto_cfg)

sw_error_t fal_vxlan_gpe_proto_cfg_get(a_uint32_t dev_id, fal_vxlan_gpe_proto_cfg_t * proto_cfg)
    DEFINE_FAL_FUNC_ADPT_EXPORT(vxlan_gpe_proto_cfg_get, dev_id, proto_cfg)
#endif
