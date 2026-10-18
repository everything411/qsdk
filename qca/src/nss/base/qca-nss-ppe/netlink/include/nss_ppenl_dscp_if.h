/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_dscp_if.h
 *	NSS PPE Netlink DSCP
 */
#ifndef __NSS_PPENL_DSCP_IF_H
#define __NSS_PPENL_DSCP_IF_H

#include <ppe_dscp.h>

/*
 * DSCP Configure Family
 */
#define NSS_PPENL_DSCP_FAMILY "nss_ppenl_dscp"

/*
 * TODO: Restrict export of this file to kernel space.
 */

/**
 * @brief DSCP rule
 */
struct nss_ppenl_dscp_rule {
	struct nss_ppenl_cmn cm;			/**< common message header */
	struct ppe_dscp_rule rule;			/**< DSCP_P_bit Table fields */
};

/*
 * @brief Message types
 */
enum nss_ppe_dscp_message_types {
	NSS_PPE_DSCP_CONFIG_RULE_MSG,			/**< DSCP rule configure message */
	NSS_PPE_DSCP_MAX_MSG_TYPES,			/**< Maximum message type */
};

/**
 * @brief NETLINK DSCP  message init
 *
 * @param rule[IN] NSS NETLINK DSCP config
 * @param type[IN] DSCP message type
 */
static inline void nss_ppenl_dscp_rule_init(struct nss_ppenl_dscp_rule *rule, enum nss_ppe_dscp_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_dscp_rule), type);
}
#endif /* __NSS_PPENL_DSCP_IF_H */

