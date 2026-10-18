/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_pm.h
 *	NSS Netlink PM definitions
 */
#ifndef __NSS_PPENL_PM_H
#define __NSS_PPENL_PM_H

bool nss_ppenl_pm_init(void);
bool nss_ppenl_pm_exit(void);

#if defined(CONFIG_NSS_PPENL_PM)
#define NSS_PPENL_PM_INIT nss_ppenl_pm_init
#define NSS_PPENL_PM_EXIT nss_ppenl_pm_exit
#else
#define NSS_PPENL_PM_INIT 0
#define NSS_PPENL_PM_EXIT 0
#endif /* !CONFIG_NSS_PPENL_PM */

#endif /* __NSS_PPENL_PM_H */
