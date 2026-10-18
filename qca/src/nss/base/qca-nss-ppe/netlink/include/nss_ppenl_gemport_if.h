/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_gemport_if.h
 *	NSS PPE Netlink GEM PORT
 */
#ifndef __NSS_PPENL_GEMPORT_IF_H
#define __NSS_PPENL_GEMPORT_IF_H

#include <ppe_gemport.h>

/**
 * GEM PORT Configure Family
 */
#define NSS_PPENL_GEM_PORT_FAMILY "nss_ppenl_gem"

/**
 * @brief GEM PORT rule
 */
struct nss_ppenl_gem_port_rule {
	struct nss_ppenl_cmn cm;				/**< common message header */
	struct ppe_gem_port_rule rule;				/**< GEM PORT rule message */
};

/*
 * @brief Message types
 */
enum nss_ppe_gem_port_message_types {
	NSS_PPE_GEM_PORT_CREATE_RULE_MSG,		/**< GEM PORT rule create message */
	NSS_PPE_GEM_PORT_DELETE_RULE_MSG,		/**< GEM PORT rule delete message */
	NSS_PPE_GEM_PORT_FLUSH_RULE_MSG,		/**< GEM PORT rule flush message */
	NSS_PPE_GEM_PORT_MAX_MSG_TYPES,			/**< Maximum message type */
};

/**
 * @brief NETLINK GEM PORT message init
 *
 * @param rule[IN] NSS NETLINK GEM PORT rule
 * @param type[IN] GEM PORT  message type
 */
static inline void nss_ppenl_gem_port_rule_init(struct nss_ppenl_gem_port_rule *rule, enum nss_ppe_gem_port_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_gem_port_rule), type);
}

#endif /* __NSS_PPENL_GEMPORT_IF_H */
