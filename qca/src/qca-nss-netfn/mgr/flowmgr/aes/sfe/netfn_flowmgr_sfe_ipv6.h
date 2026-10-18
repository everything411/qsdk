/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_sfe_ipv6.h
 *	Netfn flow manager sfe ipv6 header file
 */

#include <netfn_flowmgr.h>

netfn_flowmgr_ret_t netfn_flowmgr_sfe_create_v6_rule(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply);
netfn_flowmgr_ret_t netfn_flowmgr_sfe_destroy_v6_rule(struct netfn_flowmgr_destroy_rule *original, struct netfn_flowmgr_destroy_rule *reply);
bool netfn_flowmgr_sfe_ipv6_init(void);
void netfn_flowmgr_sfe_ipv6_deinit(void);
