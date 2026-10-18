/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_port_mirror_if.h
 *	NSS PPE Netlink port-mirror interface definitions.
 *
 * This header is shared between kernel and user space.  It defines the
 * Generic Netlink family name, the per-message payload structure, the
 * message-type enum, and the inline init helper used by user-space tools
 * (e.g. ppecfg) to prepare a Netlink message before sending it.
 */
#ifndef __NSS_PPENL_PORT_MIRROR_IF_H
#define __NSS_PPENL_PORT_MIRROR_IF_H

#include <linux/if.h>

/**
 * NSS_PPENL_PORT_MIRROR_FAMILY
 *	Generic Netlink family name for the port-mirror ppecfg family.
 */
#define NSS_PPENL_PORT_MIRROR_FAMILY	"nss_ppenl_pmirr"

/**
 * nss_ppe_port_mirror_dir
 *	Mirror direction for L2 VP port mirroring.
 */
enum nss_ppe_port_mirror_dir {
	NSS_PPE_PORT_MIRROR_DIR_INGRESS = 0,	/**< Ingress mirroring */
	NSS_PPE_PORT_MIRROR_DIR_EGRESS,		/**< Egress mirroring */
};

/**
 * nss_ppenl_port_mirror_cfg
 *	Per-message payload for port-mirror commands.
 *
 * @port_name:		Null-terminated name of the physical / VP net device
 * @dir:		Mirror direction (ingress or egress).
 * @mirror_en:		true to set mirroring.
 * @ret:		Return status written by the kernel handler and echoed
 * 			back to user space in the response message.
 */
struct nss_ppenl_port_mirror_cfg {
	char port_name[IFNAMSIZ];			/**< Net device name */
	uint8_t dir;					/**< nss_ppe_port_mirror_dir */
	bool mirror_en;					/**< true: call l2_vp_mirror_sc_config */
	int  ret;					/**< Return status (kernel to user) */
};

/**
 * nss_ppenl_port_mirror_rule
 *	Top-level Netlink message structure for the port-mirror family.
 */
struct nss_ppenl_port_mirror_rule {
	struct nss_ppenl_cmn cm;		/**< Common Netlink message header */
	struct nss_ppenl_port_mirror_cfg cfg;	/**< Port-mirror configuration */
};

/**
 * nss_ppe_port_mirror_message_types
 *	Netlink command identifiers for the port-mirror family.
 */
enum nss_ppe_port_mirror_message_types {
	NSS_PPE_PORT_MIRROR_CFG_MSG,		/**< Configure L2 VP mirror on a port */
	NSS_PPE_PORT_MIRROR_MAX_MSG_TYPES,	/**< Sentinel - must be last */
};

/**
 * nss_ppenl_port_mirror_rule_init()
 *	Initialise a port-mirror Netlink message (called from user space / ppecfg).
 *
 * @param rule[IN]  Pointer to the message structure to initialise.
 * @param type[IN]  Command type (one of nss_ppe_port_mirror_message_types).
 */
static inline void nss_ppenl_port_mirror_rule_init(struct nss_ppenl_port_mirror_rule *rule,
						    enum nss_ppe_port_mirror_message_types type)
{
	nss_ppenl_cmn_set_ver(&rule->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&rule->cm, sizeof(struct nss_ppenl_port_mirror_rule), type);
}

#endif /* __NSS_PPENL_PORT_MIRROR_IF_H */
