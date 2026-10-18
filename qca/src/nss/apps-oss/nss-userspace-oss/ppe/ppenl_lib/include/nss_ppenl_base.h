/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_BASE_H__
#define __NSS_PPENL_BASE_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/socket.h>
#include <net/if.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/if_ether.h>

/* Generic Netlink header */
#include <netlink/genl/ctrl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/genl.h>

#if !defined (likely) || !defined (unlikely)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#include <nss_ppenl_cmn_if.h>
#include <nss_ppenl_acl_if.h>
#include <nss_ppenl_acl_api.h>
#include <nss_ppenl_policer_if.h>
#include <nss_ppenl_policer_api.h>
#include <nss_ppenl_qos_if.h>
#include <nss_ppenl_qos_api.h>
#include <nss_ppenl_cos_map_if.h>
#include <nss_ppenl_cos_map_api.h>
#include <nss_ppenl_exception_if.h>
#include <nss_ppenl_exception_api.h>
#include <nss_ppenl_dscp_if.h>
#include <nss_ppenl_dscp_api.h>
#include <nss_ppenl_pm_if.h>
#include <nss_ppenl_pm_api.h>
#include <nss_ppenl_port_mgmt_if.h>
#include <nss_ppenl_port_mgmt_api.h>
#include <nss_ppenl_vlan_if.h>
#include <nss_ppenl_vlan_api.h>
#include <nss_ppenl_dot1p_if.h>
#include <nss_ppenl_dot1p_api.h>
#include <nss_ppenl_gemport_if.h>
#include <nss_ppenl_gemport_api.h>
#ifdef NSS_EDMA_DDRQ_FEATURE
#include <nss_ppenl_edma_ddrq_if.h>
#include <nss_ppenl_edma_ddrq_api.h>
#endif
#ifdef NSS_PPE_MCAST_FEATURE
#include <nss_ppenl_mcast_if.h>
#include <nss_ppenl_mcast_api.h>
#endif
#ifdef NSS_PPE_PORT_MIRROR_FEATURE
#include <nss_ppenl_port_mirror_if.h>
#include <nss_ppenl_port_mirror_api.h>
#endif
#ifdef NSS_PPE_TUN_RPS_FEATURE
#include <nss_ppenl_tun_rps_if.h>
#include <nss_ppenl_tun_rps_api.h>
#endif
#endif /* __NSS_PPENL_BASE_H__ */
