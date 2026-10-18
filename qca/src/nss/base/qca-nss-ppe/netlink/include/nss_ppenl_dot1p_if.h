/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_dot1p_if.h
 *	NSS PPE Netlink DOT1P
 */
#ifndef __NSS_PPENL_DOT1P_IF_H
#define __NSS_PPENL_DOT1P_IF_H

#include <ppe_dot1p.h>

/**
 * DOT1P Configure Family
 */
#define NSS_PPENL_DOT1P_FAMILY "nss_ppenl_dot1p"

/*
 * TODO: Restrict export of this file to kernel space.
 */

/**
 * @brief DOT1P rule
 */
struct nss_ppenl_dot1p_rule {
	struct nss_ppenl_cmn cm;				/**< common message header */
	struct ppe_dot1p_rule rule;				/**< DOT1P rule message */
	struct ppe_dot1p_policer_rule policer_rule;		/**< DOT1P policer rule message */
};

/*
 * @brief Message types
 */
enum nss_ppe_dot1p_message_types {
	NSS_PPE_DOT1P_CREATE_RULE_MSG,			/**< DOT1P rule create message */
	NSS_PPE_DOT1P_DELETE_RULE_MSG,			/**< DOT1P rule delete message */
	NSS_PPE_DOT1P_FLUSH_RULE_MSG,			/**< DOT1P rule flush message */
	NSS_PPE_DOT1P_CREATE_DEF_RULE_MSG,		/**< DOT1P rule create default message */
	NSS_PPE_DOT1P_PAUSE_RULE_MSG,			/**< DOT1P rule pause message */
	NSS_PPE_DOT1P_RESUME_RULE_MSG,			/**< DOT1P rule resume message */
	NSS_PPE_DOT1P_GET_STATE_RULE_MSG,		/**< DOT1P rule get state message */
	NSS_PPE_DOT1P_POLICER_CREATE_RULE_MSG,		/**< DOT1P policer rule create message */
	NSS_PPE_DOT1P_POLICER_DELETE_RULE_MSG,		/**< DOT1P policer rule delete message */
	NSS_PPE_DOT1P_POLICER_FLUSH_RULE_MSG,		/**< DOT1P policer rule flush message */
	NSS_PPE_DOT1P_MAX_MSG_TYPES,			/**< Maximum message type */
};

/**
 * @brief NETLINK DOT1P message init
 *
 * @param rule[IN] NSS NETLINK DOT1P rule
 * @param type[IN] DOT1P  message type
 */
static inline void nss_ppenl_dot1p_rule_init(struct nss_ppenl_dot1p_rule *rule, enum nss_ppe_dot1p_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_dot1p_rule), type);
}

#endif /* __NSS_PPENL_DOT1P_IF_H */
