/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_vxlan FAL_VXLAN
 * @{
 */
#ifndef _FAL_VXLAN_H_
#define _FAL_VXLAN_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal_type.h"
#include "fal_tunnel.h"

typedef enum {
	FAL_VXLAN =0,
	FAL_VXLAN_GPE =1,
} fal_vxlan_type_t;

typedef struct {
	a_uint8_t ethernet;
	a_uint8_t ipv4;
	a_uint8_t ipv6;
} fal_vxlan_gpe_proto_cfg_t;

sw_error_t
fal_vxlan_entry_add(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry);

sw_error_t
fal_vxlan_entry_del(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry);

#ifndef IN_VXLAN_MINI
sw_error_t
fal_vxlan_entry_getfirst(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry);

sw_error_t
fal_vxlan_entry_getnext(a_uint32_t dev_id, fal_vxlan_type_t type, fal_tunnel_udp_entry_t * entry);

sw_error_t
fal_vxlan_gpe_proto_cfg_set(a_uint32_t dev_id, fal_vxlan_gpe_proto_cfg_t * proto_cfg);

sw_error_t
fal_vxlan_gpe_proto_cfg_get(a_uint32_t dev_id, fal_vxlan_gpe_proto_cfg_t * proto_cfg);
#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_VXLAN_H_ */
/**
 * @}
 */
