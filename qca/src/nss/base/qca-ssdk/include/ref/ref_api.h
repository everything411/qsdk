/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _REF_API_H_
#define _REF_API_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */
#ifdef ISISC
#if defined(IN_VLAN) && !defined(IN_VLAN_MINI)
#define REF_VLAN_API \
    SW_API_DEF(SW_API_LAN_WAN_CFG_SET, qca_lan_wan_cfg_set), \
    SW_API_DEF(SW_API_LAN_WAN_CFG_GET, qca_lan_wan_cfg_get),

#define REF_VLAN_API_PARAM \
    SW_API_DESC(SW_API_LAN_WAN_CFG_SET) \
    SW_API_DESC(SW_API_LAN_WAN_CFG_GET)

#else
#define REF_VLAN_API
#define REF_VLAN_API_PARAM
#endif
#else
#define REF_VLAN_API
#define REF_VLAN_API_PARAM
#endif

#define SSDK_REF_API \
    REF_VLAN_API

#define SSDK_REF_PARAM  \
    REF_VLAN_API_PARAM

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _REF_API_H_ */
