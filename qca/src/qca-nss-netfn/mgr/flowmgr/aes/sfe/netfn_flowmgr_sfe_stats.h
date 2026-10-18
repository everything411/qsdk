/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_sfe_stats.h
 *	Netfn flow manager sfe stats header file
 */

bool netfn_flowmgr_sfe_ipv4_stats_init(void);
void netfn_flowmgr_sfe_ipv4_stats_deinit(void);
bool netfn_flowmgr_sfe_ipv6_stats_init(void);
void netfn_flowmgr_sfe_ipv6_stats_deinit(void);
netfn_flowmgr_ret_t netfn_flowmgr_sfe_v4_get_stats(struct netfn_flowmgr_flow_conn_stats *conn_stats);
netfn_flowmgr_ret_t netfn_flowmgr_sfe_v6_get_stats(struct netfn_flowmgr_flow_conn_stats *conn_stats);
