/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_vlan_if.h
 *	NSS PPE Netlink VLAN
 */
#ifndef __NSS_PPENL_VLAN_IF_H
#define __NSS_PPENL_VLAN_IF_H

#include <ppe_vlan.h>

/**
 * VLAN Configure Family
 */
#define NSS_PPENL_VLAN_FAMILY "nss_ppenl_vlan"

/*
 * TODO: Restrict export of this file to kernel space.
 */

/**
 * @brief VLAN rule
 */
struct nss_ppenl_vlan_rule {
	struct nss_ppenl_cmn cm;				/**< common message header */
	struct ppe_vlan_rule rule;				/**< VLAN rule  message */
};

/*
 * @brief Message types
 */
enum nss_ppe_vlan_message_types {
	NSS_PPE_VLAN_CREATE_RULE_MSG,			/**< VLAN rule create message */
	NSS_PPE_VLAN_DESTROY_RULE_MSG,			/**< VLAN rule destroy message */
	NSS_PPE_VLAN_FLUSH_RULE_MSG,			/**< VLAN rule flush message */
	NSS_PPE_VLAN_MAX_MSG_TYPES,			/**< Maximum message type */
};

/**
 * @brief NETLINK VLAN  message init
 *
 * @param rule[IN] NSS NETLINK VLAN config
 * @param type[IN] VLAN message type
 */
static inline void nss_ppenl_vlan_rule_init(struct nss_ppenl_vlan_rule *rule, enum nss_ppe_vlan_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_vlan_rule), type);
}
#endif /* __NSS_PPENL_VLAN_IF_H */

