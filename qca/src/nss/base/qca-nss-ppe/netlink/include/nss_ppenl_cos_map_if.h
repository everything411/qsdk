/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_cos_map_if.h
 *	NSS PPE Netlink CoS Map
 */
#ifndef __NSS_PPENL_COS_MAP_IF_H
#define __NSS_PPENL_COS_MAP_IF_H

#include <ppe_cos_map.h>
/*
 * CoS map Configure Family
 */
#define NSS_PPENL_COS_MAP_FAMILY "nss_ppenl_cos"

/*
 * @brief CoS Map rule
 */
struct nss_ppenl_cos_map_config {
	struct nss_ppenl_cmn cm;	/*< common message header */
	union {
		struct ppe_cos_map_create_info rule;	/* ppe cos mapping rule */
		struct ppe_cos_map_port_group_info config;	/* port group config */
	} msg;
};

/*
 * @brief Message types
 */
enum nss_ppe_cos_map_message_types {
	NSS_PPE_COS_MAP_CREATE_RULE_MSG,	/* CoS Map rule create message */
	NSS_PPE_COS_MAP_DESTROY_RULE_MSG,	/* CoS Map rule delete message */
	NSS_PPE_COS_MAP_FLUSH_RULE_MSG,		/* CoS Map rule flush message */
	NSS_PPE_COS_MAP_PORT_GROUP_SET,		/* CoS Map port group set message */
	NSS_PPE_COS_MAP_MAX_MSG_TYPES		/* Maximum message type */
};

/**
 * @brief NETLINK CoS Map message init
 *
 * @param rule[IN] NSS NETLINK CoS Map rule
 * @param type[IN] CoS Map message type
 */
static inline void nss_ppenl_cos_map_rule_init(struct nss_ppenl_cos_map_config *config, enum nss_ppe_cos_map_message_types type)
{
	nss_ppenl_cmn_set_ver(&config->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&config->cm, sizeof(struct nss_ppenl_cos_map_config), type);
}

#endif /* __NSS_PPENL_COS_MAP_IF_H */
