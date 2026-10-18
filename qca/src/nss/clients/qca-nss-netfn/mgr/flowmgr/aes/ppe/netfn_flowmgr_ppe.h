/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_ppe.h
 *	Netfn flow manager ppe header file
 */

#include <ppe_drv.h>
#include <ppe_drv_v4.h>
#include <ppe_drv_v6.h>
#include <ppe_acl.h>

#define NETFN_FLOWMGR_VLAN_NOT_CONFIGURED	0xFFF

bool netfn_flowmgr_ppe_init(void);
void netfn_flowmgr_ppe_deinit(void);
netfn_flowmgr_ret_t netfn_flowmgr_ppe_dscp_pri_add(struct netfn_flowmgr_dscp_priority *dscpinfo);
netfn_flowmgr_ret_t netfn_flowmgr_ppe_dscp_pri_del(int16_t rule_id);
void netfn_flowmgr_ppe_endianess_be_to_le(uint32_t ip_addr[], int ip_version);
netfn_flowmgr_ret_t netfn_flowmgr_ppe_validate_vlan_info(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply);
