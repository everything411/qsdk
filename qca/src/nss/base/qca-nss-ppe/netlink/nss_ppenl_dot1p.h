/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_dot1p.h
 *	NSS Netlink DOT1P definitions
 */
#ifndef __NSS_PPENL_DOT1P_H
#define __NSS_PPENL_DOT1P_H

bool nss_ppenl_dot1p_init(void);
bool nss_ppenl_dot1p_exit(void);

#if defined(CONFIG_NSS_PPENL_DOT1P)
#define NSS_PPENL_DOT1P_INIT nss_ppenl_dot1p_init
#define NSS_PPENL_DOT1P_EXIT nss_ppenl_dot1p_exit
#else
#define NSS_PPENL_DOT1P_INIT 0
#define NSS_PPENL_DOT1P_EXIT 0
#endif /* !CONFIG_NSS_PPENL_DOT1P */

#endif /* __NSS_PPENL_DOT1P_H */
