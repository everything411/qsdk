/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_gemport.h
 *	NSS Netlink GEM PORT definitions
 */
#ifndef __NSS_PPENL_GEMPORT_H
#define __NSS_PPENL_GEMPORT_H

bool nss_ppenl_gem_port_init(void);
bool nss_ppenl_gem_port_exit(void);

#if defined(CONFIG_NSS_PPENL_GEM_PORT)
#define NSS_PPENL_GEM_PORT_INIT nss_ppenl_gem_port_init
#define NSS_PPENL_GEM_PORT_EXIT nss_ppenl_gem_port_exit
#else
#define NSS_PPENL_GEM_PORT_INIT 0
#define NSS_PPENL_GEM_PORT_EXIT 0
#endif /* !CONFIG_NSS_PPENL_GEM_PORT */

#endif /* __NSS_PPENL_GEMPORT_H */
