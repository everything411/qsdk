/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_FAMILY_H
#define __PPECFG_FAMILY_H

#include "ppecfg_acl.h"
#include "ppecfg_policer.h"
#include "ppecfg_qos.h"
#include "ppecfg_cos_map.h"
#include "ppecfg_exception.h"
#ifdef NSS_PPE_DSCP_FEATURE
#include "ppecfg_dscp.h"
#endif
#ifdef NSS_PPE_PM_FEATURE
#include "ppecfg_pm.h"
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE
#include "ppecfg_vlan.h"
#endif
#ifdef NSS_PPE_PORT_MGMT_FEATURE
#include "ppecfg_port_mgmt.h"
#endif
#ifdef NSS_PPE_DOT1P_FEATURE
#include "ppecfg_dot1p.h"
#endif
#ifdef NSS_PPE_GEMPORT_FEATURE
#include "ppecfg_gemport.h"
#endif
#ifdef NSS_PPE_PORT_MIRROR_FEATURE
#include "ppecfg_port_mirror.h"
#endif
#ifdef NSS_EDMA_DDRQ_FEATURE
#include "ppecfg_edma_ddrq.h"
#endif
#ifdef NSS_PPE_TUN_RPS_FEATURE
#include "ppecfg_tun_rps.h"
#endif
#include <nss_ppenl_base.h>

/*
 * Family match params
 */
extern struct ppecfg_param ppecfg_acl_params[PPECFG_ACL_CMD_MAX];
extern struct ppecfg_param ppecfg_policer_params[PPECFG_POLICER_CMD_MAX];
extern struct ppecfg_param ppecfg_qos_params[PPECFG_QOS_CMD_MAX];
extern struct ppecfg_param ppecfg_cos_map_params[PPECFG_COS_MAP_CMD_MAX];
extern struct ppecfg_param ppecfg_exception_params[PPECFG_EXCEPTION_PARAM_MAX];
#ifdef NSS_PPE_DSCP_FEATURE
extern struct ppecfg_param ppecfg_dscp_params[PPECFG_DSCP_CMD_MAX];
#endif
#ifdef NSS_PPE_PM_FEATURE
extern struct ppecfg_param ppecfg_pm_params[PPECFG_PM_CMD_MAX];
#endif
#ifdef NSS_PPE_EXT_VLAN_FEATURE
extern struct ppecfg_param ppecfg_vlan_params[PPECFG_VLAN_CMD_MAX];
#endif
#ifdef NSS_PPE_PORT_MGMT_FEATURE
extern struct ppecfg_param ppecfg_port_mgmt_params[PPECFG_PORT_MGMT_CMD_MAX];
#endif
#ifdef NSS_PPE_DOT1P_FEATURE
extern struct ppecfg_param ppecfg_dot1p_params[PPECFG_DOT1P_CMD_MAX];
#endif
#ifdef NSS_PPE_GEMPORT_FEATURE
extern struct ppecfg_param ppecfg_gem_port_params[PPECFG_GEM_PORT_CMD_MAX];
#endif
#ifdef NSS_PPE_PORT_MIRROR_FEATURE
extern struct ppecfg_param ppecfg_port_mirror_params[PPECFG_PORT_MIRROR_CMD_MAX];
#endif
#ifdef NSS_EDMA_DDRQ_FEATURE
extern struct ppecfg_param ppecfg_edma_ddrq_params[PPECFG_EDMA_DDRQ_CMD_MAX];
#endif
#ifdef NSS_PPE_TUN_RPS_FEATURE
extern struct ppecfg_param ppecfg_tun_rps_params[PPECFG_TUN_RPS_CMD_MAX];
#endif
#endif /* __PPECFG_FAMILY_H*/
