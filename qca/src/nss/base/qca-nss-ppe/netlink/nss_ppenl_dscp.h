/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_dscp.h
 *	NSS Netlink DSCP definitions
 */
#ifndef __NSS_PPENL_DSCP_H
#define __NSS_PPENL_DSCP_H

bool nss_ppenl_dscp_init(void);
bool nss_ppenl_dscp_exit(void);

#if defined(CONFIG_NSS_PPENL_DSCP)
#define NSS_PPENL_DSCP_INIT nss_ppenl_dscp_init
#define NSS_PPENL_DSCP_EXIT nss_ppenl_dscp_exit
#else
#define NSS_PPENL_DSCP_INIT 0
#define NSS_PPENL_DSCP_EXIT 0
#endif /* !CONFIG_NSS_PPENL_DSCP */

#endif /* __NSS_PPENL_DSCP_H */

