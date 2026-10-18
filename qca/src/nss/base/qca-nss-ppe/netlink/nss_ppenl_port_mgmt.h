/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_port_mgmt.h
 *      NSS Netlink PORT_MGMT definitions
 */
#ifndef __NSS_PPENL_PORT_MGMT_H
#define __NSS_PPENL_PORT_MGMT_H

bool nss_ppenl_port_mgmt_init(void);
bool nss_ppenl_port_mgmt_exit(void);

#if defined(CONFIG_NSS_PPENL_PORT_MGMT)
#define NSS_PPENL_PORT_MGMT_INIT nss_ppenl_port_mgmt_init
#define NSS_PPENL_PORT_MGMT_EXIT nss_ppenl_port_mgmt_exit
#else
#define NSS_PPENL_PORT_MGMT_INIT 0
#define NSS_PPENL_PORT_MGMT_EXIT 0
#endif /* !CONFIG_NSS_PPENL_PORT_MGMT */

#endif /* __NSS_PPENL_PORT_MGMT_H */
