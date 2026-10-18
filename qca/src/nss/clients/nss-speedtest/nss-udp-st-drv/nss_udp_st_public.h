/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file nss_udp_st_public.h
 *	NSS UDP ST Public definitions.
 */

#ifndef _NSS_UDP_ST_PUBLIC_H_
#define _NSS_UDP_ST_PUBLIC_H_

/**
 * @addtogroup nss_udp_st_public_subsystem
 * @{
 */

#include "nss_udp_st_drv.h"
#include "nss_udp_st_tx.h"
#include "nss_udp_st_rx.h"
#include "nss_udp_st_ip.h"
#ifdef NSS_UDP_ST_DRV_HW_OFFLOAD_ENABLE
#include "nss_udp_st_ppe.h"
#endif

#define NSS_UDP_ST_RULE_ID_VALID_TAG    0xDD
#define NSS_UDP_ST_RULE_ID_MASK         0xFF
#define NSS_UDP_ST_EXCEPTION_DPORT      0
#define NSS_UDP_ST_PPPOE_OVERHEAD	8
#define NSS_UDP_ST_VLAN_OVERHEAD	4

#define udp_st_err(s, ...) pr_err("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define udp_st_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define udp_st_trace(s, ...) pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define udp_st_debug(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)

/** @} */ /* end_addtogroup nss_udp_st_public_subsystem */

#endif /*_NSS_UDP_ST_PUBLIC_H_*/
