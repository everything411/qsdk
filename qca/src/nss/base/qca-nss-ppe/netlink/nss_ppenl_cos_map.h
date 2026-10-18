/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * nss_ppenl_cos_mapping.h
 *	NSS Netlink CoS map definitions
 */
#ifndef __NSS_PPENL_COS_MAP_H
#define __NSS_PPENL_COS_MAP_H

bool nss_ppenl_cos_map_init(void);
bool nss_ppenl_cos_map_exit(void);

/*
 * Userspace should define CONFIG_NSS_PPENL_COS_MAP
 */
#if defined(CONFIG_NSS_PPENL_COS_MAP)
#define NSS_PPENL_COS_MAP_INIT nss_ppenl_cos_map_init
#define NSS_PPENL_COS_MAP_EXIT nss_ppenl_cos_map_exit
#else
#define NSS_PPENL_COS_MAP_INIT 0
#define NSS_PPENL_COS_MAP_EXIT 0
#endif /* !CONFIG_NSS_PPENL_COS_MAP */

#endif /* __NSS_PPENL_COS_MAP_H */
