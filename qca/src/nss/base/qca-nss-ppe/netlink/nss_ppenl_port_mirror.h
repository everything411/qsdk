/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_port_mirror.h
 *	NSS Netlink port-mirror handler declarations.
 */
#ifndef __NSS_PPENL_PORT_MIRROR_H
#define __NSS_PPENL_PORT_MIRROR_H

bool nss_ppenl_port_mirror_init(void);
bool nss_ppenl_port_mirror_exit(void);

#if defined(CONFIG_NSS_PPENL_PORT_MIRROR)
#define NSS_PPENL_PORT_MIRROR_INIT	nss_ppenl_port_mirror_init
#define NSS_PPENL_PORT_MIRROR_EXIT	nss_ppenl_port_mirror_exit
#else
#define NSS_PPENL_PORT_MIRROR_INIT	0
#define NSS_PPENL_PORT_MIRROR_EXIT	0
#endif /* !CONFIG_NSS_PPENL_PORT_MIRROR */

#endif /* __NSS_PPENL_PORT_MIRROR_H */
