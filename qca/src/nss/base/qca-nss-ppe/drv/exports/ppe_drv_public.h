/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef _PPE_DRV_PUBLIC_H_
#define _PPE_DRV_PUBLIC_H_

/**
 * @file ppe_drv_public.h
 *	NSS PPE Public definitions.
 */

#include "ppe_drv.h"
#include "ppe_drv_acl.h"
#include "ppe_drv_br.h"
#include "ppe_drv_cc.h"
#include "ppe_drv_dp.h"
#include "ppe_drv_eip.h"
#include "ppe_drv_iface.h"
#include "ppe_drv_lag.h"
#include "ppe_drv_policer.h"
#include "ppe_drv_port.h"
#include "ppe_drv_qos.h"
#include "ppe_drv_cos_map.h"
#include "ppe_drv_sc.h"
#include "ppe_drv_v4.h"
#include "ppe_drv_v6.h"
#include "ppe_drv_vlan.h"
#include "ppe_drv_vp.h"
#include "ppe_drv_athtag.h"
#ifdef NSS_PPE_PM_COUNTER_FEATURE_SUPPORT
#include "ppe_drv_pm.h"
#endif
#ifdef NSS_PPE_DRV_PORT_MGMT_SUPPORT
#include "ppe_drv_port_mgmt.h"
#endif
#ifdef NSS_PPE_FEATURE_DOT1P
#include "ppe_drv_dot1p.h"
#endif
#ifdef NSS_PPE_FEATURE_GEMPORT
#include "ppe_drv_gemport.h"
#endif
#include "ppe_drv_veip.h"
#ifdef NSS_PPE_MCAST_FEATURE_SUPPORT
#include "ppe_drv_mcast.h"
#endif
#endif /* _PPE_DRV_PUBLIC_H_ */
