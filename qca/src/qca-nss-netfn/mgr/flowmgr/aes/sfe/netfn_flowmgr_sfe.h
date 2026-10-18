/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_sfe.h
 *	Netfn flow manager sfe header file
 */

/*
 * netfn_flowmgr_sfe_init
 *	API for initialization.
 */
bool netfn_flowmgr_sfe_init(void);

/*
 * netfn_flowmgr_sfe_deinit
 *	API for exit
 */
void netfn_flowmgr_sfe_deinit(void);

/*
 * netfn_flowmgr_sfe_validate_vlan_info
 *	API used to validate vlan parameters
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_validate_vlan_info(struct netfn_flowmgr_create_rule *original, struct netfn_flowmgr_create_rule *reply);
