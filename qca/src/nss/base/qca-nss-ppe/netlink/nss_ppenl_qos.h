/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_qos.h
 *	NSS Netlink Qos definitions
 */
#ifndef __NSS_PPENL_QOS_H
#define __NSS_PPENL_QOS_H

bool nss_ppenl_qos_init(void);
bool nss_ppenl_qos_exit(void);

/*
 * Userspace should define CONFIG_NSS_PPENL_QOS
 */
#if defined(CONFIG_NSS_PPENL_QOS)
#define NSS_PPENL_QOS_INIT nss_ppenl_qos_init
#define NSS_PPENL_QOS_EXIT nss_ppenl_qos_exit
#else
#define NSS_PPENL_QOS_INIT 0
#define NSS_PPENL_QOS_EXIT 0
#endif /* !CONFIG_NSS_PPENL_QOS */

#endif /* __NSS_PPENL_QOS_H */
