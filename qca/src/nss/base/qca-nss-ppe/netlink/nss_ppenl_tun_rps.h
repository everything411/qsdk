/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_tun_rps.h
 *	NSS Netlink Tunnel RPS definitions
 */
#ifndef __NSS_PPENL_TUN_RPS_H
#define __NSS_PPENL_TUN_RPS_H

bool nss_ppenl_tun_rps_init(void);
bool nss_ppenl_tun_rps_exit(void);

#if defined(CONFIG_NSS_PPENL_TUN_RPS)
#define NSS_PPENL_TUN_RPS_INIT nss_ppenl_tun_rps_init
#define NSS_PPENL_TUN_RPS_EXIT nss_ppenl_tun_rps_exit
#else
#define NSS_PPENL_TUN_RPS_INIT 0
#define NSS_PPENL_TUN_RPS_EXIT 0
#endif /* !CONFIG_NSS_PPENL_TUN_RPS */

#endif /* __NSS_PPENL_TUN_RPS_H */
