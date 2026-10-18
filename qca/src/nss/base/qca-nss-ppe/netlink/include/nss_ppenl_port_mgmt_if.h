/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_port_mgmt_if.h
 *	NSS PPE Netlink PORT_MGMT
 */
#ifndef __NSS_PPENL_PORT_MGMT_IF_H
#define __NSS_PPENL_PORT_MGMT_IF_H

#include <ppe_port_mgmt.h>

/**
 * PORT_MGMT Configure Family
 */
#define NSS_PPENL_PORT_MGMT_FAMILY "nss_pmgmt"

/*
 * @brief PORT_MGMT info
 *
 * This info is used to communicate PPE PORT_MGMT configuration.
 */
struct nss_ppenl_port_mgmt_info {
	struct nss_ppenl_cmn cm;			/**< Common message header */
	struct ppe_port_mgmt_isol isol;			/**< Port Isolation information */
	struct ppe_port_mac_lrn_limit mac_lrn_limit;	/**< Port learn limit for FDB */
	struct ppe_port_mac_filter mac_filter;		/**< MAC Filter for FDB */
	struct ppe_port_omci_port omci_port;		/**< OMCI-managed port configuration */
};

/*
 * @brief Message types
 */
enum nss_ppe_port_mgmt_message_types {
	NSS_PPE_PORT_MGMT_PORT_ISOL_SET_MSG,		/**< Port isolation set message */
	NSS_PPE_PORT_MGMT_ACT_CTRL_SET_MSG,		/**< Port action control set message */
	NSS_PPE_PORT_MGMT_PORT_ISOL_DEF_MSG,		/**< Port isolation set to default */
	NSS_PPE_PORT_MGMT_MAC_LRN_LIMIT_SET_MSG,	/**< Port mac learn limit set */
	NSS_PPE_PORT_MGMT_MAC_FILTER_SET_MSG,		/**< MAC filtering set */
	NSS_PPE_PORT_MGMT_MAC_FILTER_CLR_MSG,		/**< Mac filtering clear */
	NSS_PPE_PORT_MGMT_OMCI_PORT_ADD_MSG,		/**< Add OMCI-managed port */
	NSS_PPE_PORT_MGMT_OMCI_PORT_DEL_MSG,		/**< Delete OMCI-managed port */
	NSS_PPE_PORT_MGMT_OMCI_PORT_FLUSH_MSG,		/**< Flush all OMCI-managed ports */
	NSS_PPE_PORT_MGMT_MAX_MSG_TYPES,		/**< Maximum message type */
};

/**
 * @brief NETLINK PORT_MGMT  message init
 *
 * @param rule[IN] NSS NETLINK PORT_MGMT config
 * @param type[IN] PORT_MGMT message type
 */
static inline void nss_ppenl_rule_port_mgmt_init(struct nss_ppenl_port_mgmt_info *port_mgmt_info, enum nss_ppe_port_mgmt_message_types type)
{
	nss_ppenl_cmn_set_ver(&port_mgmt_info->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&port_mgmt_info->cm, sizeof(struct nss_ppenl_port_mgmt_info), type);
}
#endif /* __NSS_PPENL_PORT_MGMT_IF_H */
