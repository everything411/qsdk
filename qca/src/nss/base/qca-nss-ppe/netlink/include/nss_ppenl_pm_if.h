/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_pm_if.h
 *      NSS PPE Netlink PM
 */
#ifndef __NSS_PPENL_PM_IF_H
#define __NSS_PPENL_PM_IF_H

#include <ppe_pm.h>

/**
 * PM Configure Family
 */
#define NSS_PPENL_PM_FAMILY "nss_ppenl_pm"

/* This file is intended to define the interface for Netlink communication
 * with user space. Therefore, the structures and definitions within it
 * are by design exposed for user-space consumption via Netlink.
 */

/**
 * @brief VLAN rule
 */
struct nss_ppenl_pm_info {
	struct nss_ppenl_cmn cm;			/**< common message header */
	struct ppe_pm_counter counter_info;		/**< PM counter table match fields */
	struct ppe_pm_counter_gen counter_gen;		/**< PM counter generation table fields */
};

/*
 * @brief Message types
 */
enum nss_ppe_pm_message_types {
	NSS_PPE_PM_COUNTER_GET_MSG,			/**< PM counter stats get message */
	NSS_PPE_PM_COUNTER_GEN_CREATE_RULE_MSG,		/**< PM counter generation rule add message */
	NSS_PPE_PM_COUNTER_GEN_DESTROY_RULE_MSG,	/**< PM counter generation rule del message */
	NSS_PPE_PM_MAX_MSG_TYPES,			/**< Maximum message type */
};

/**
 * @brief NETLINK PM  message init
 *
 * @param rule[IN] NSS NETLINK PM config
 * @param type[IN] PM message type
 */
static inline void nss_ppenl_rule_pm_init(struct nss_ppenl_pm_info *pm_info, enum nss_ppe_pm_message_types type)
{
	nss_ppenl_cmn_set_ver(&pm_info->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&pm_info->cm, sizeof(struct nss_ppenl_pm_info), type);
}
#endif /* __NSS_PPENL_PM_IF_H */
