/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_vlan.h
 *	NSS Netlink VLAN definitions
 */
#ifndef __NSS_PPENL_VLAN_H
#define __NSS_PPENL_VLAN_H

bool nss_ppenl_vlan_init(void);
bool nss_ppenl_vlan_exit(void);

#if defined(CONFIG_NSS_PPENL_VLAN)
#define NSS_PPENL_VLAN_INIT nss_ppenl_vlan_init
#define NSS_PPENL_VLAN_EXIT nss_ppenl_vlan_exit
#else
#define NSS_PPENL_VLAN_INIT 0
#define NSS_PPENL_VLAN_EXIT 0
#endif /* !CONFIG_NSS_PPENL_VLAN */

#endif /* __NSS_PPENL_VLAN_H */

