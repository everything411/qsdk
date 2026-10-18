/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


/*
 * nss_ppenl_mcast.h
 *	NSS Netlink multicast definitions
 */
#ifndef __NSS_PPENL_MCAST_H
#define __NSS_PPENL_MCAST_H

bool nss_ppenl_mcast_init(void);
bool nss_ppenl_mcast_exit(void);

/*
 * Userspace should define CONFIG_NSS_PPENL_MCAST
 */
#if defined(CONFIG_NSS_PPENL_MCAST)
#define NSS_PPENL_MCAST_INIT nss_ppenl_mcast_init
#define NSS_PPENL_MCAST_EXIT nss_ppenl_mcast_exit
#else
#define NSS_PPENL_MCAST_INIT 0
#define NSS_PPENL_MCAST_EXIT 0
#endif /* !CONFIG_NSS_PPENL_MCAST */

#endif /* __NSS_PPENL_MCAST_H */
