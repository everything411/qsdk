/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_acl_if.h
 *	NSS PPE Netlink ACL
 */
#ifndef __NSS_PPENL_ACL_IF_H
#define __NSS_PPENL_ACL_IF_H

#include <ppe_acl.h>

/**
 * ACL Configure Family
 */
#define NSS_PPENL_ACL_FAMILY "nss_ppenl_acl"

/*
 * TODO: Restrict export of this file to kernel space.
 */

/**
 * @brief ACL rule
 */
struct nss_ppenl_acl_rule {
	struct nss_ppenl_cmn cm;				/**< common message header */
	struct ppe_acl_rule rule;				/**< ACL rule message */
};

/*
 * @brief Message types
 */
enum nss_ppe_acl_message_types {
	NSS_PPE_ACL_CREATE_RULE_MSG,		/**< ACL rule create message */
	NSS_PPE_ACL_DESTROY_RULE_MSG,		/**< ACL rule destroy message */
	NSS_PPE_ACL_FLUSH_RULE_MSG,		/**< ACL rule flush message */
	NSS_PPE_ACL_UPDATE_PRI_RULE_MSG,	/**< ACL rule update priority rule message */
	NSS_PPE_ACL_MAX_MSG_TYPES,		/**< Maximum message type */
};

/**
 * @brief NETLINK ACL message init
 *
 * @param rule[IN] NSS NETLINK ACL rule
 * @param type[IN] ACL message type
 */
static inline void nss_ppenl_acl_rule_init(struct nss_ppenl_acl_rule *rule, enum nss_ppe_acl_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_acl_rule), type);
}

#endif /* __NSS_PPENL_ACL_IF_H */
