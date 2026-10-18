/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
 #include <ppe_mcast.h>

/*
 * @file nss_ppenl_mcast_if.h
 *	NSS PPE Netlink multicast
 */
#ifndef __NSS_PPENL_MCAST_IF_H
#define __NSS_PPENL_MCAST_IF_H

/*
 * Multicast Configure Family.
 */
#define NSS_PPENL_MCAST_FAMILY "nss_ppenl_mcast"

/*
 *  @brief PPE  multicast IPv4 and IPv6 address union.
 */
union nss_ppenl_mcast_ip {
	uint32_t v4;	/* IPv4 address */
	uint32_t v6[4];	/* IPv6 address */
};

/*
 *  @brief PPE multicast entry information.
 */
struct nss_ppenl_mcast_entry {
	char dev[IFNAMSIZ];		/* Device associated with multicast rule. */
	bool vlan_enabled;		/* Is VLAN enabled. */
	uint32_t vlan_id;		/* VLAN ID. */
	bool sip_enabled;		/* Is source IP of the sender enabled. */
	bool is_v4;		/* IPv4 or IPv6 entry. */
	union nss_ppenl_mcast_ip sip;		/* Source IP of the sender. */
	union nss_ppenl_mcast_ip gip;			/* Multicast group IP. */
};

/*
 * @brief QOS req.
 */
struct nss_ppenl_mcast_req {
	struct nss_ppenl_cmn cm;	/*< Common message header. */
	struct nss_ppenl_mcast_entry mc_entry;	/* PPE multicast entry config. */
	int ret;		/* Return value to userspace. */
};

/*
 * @brief Message types.
 */
enum nss_ppe_mcast_message_types {
	NSS_PPE_MCAST_CREATE_ENTRY,	/* Multicast request create entry message. */
	NSS_PPE_MCAST_DELETE_ENTRY,	/* Multicast request delete entry message. */
	NSS_PPE_MCAST_MAX_MSG_TYPES		/* Maximum message type. */
};

/**
 * @brief NETLINK multicast message init.
 *
 * @param[IN] req  NSS NETLINK multicast req.
 * @param[IN] type multicast message type.
 * @return
 * None
 */
static inline void nss_ppenl_mcast_req_init(struct nss_ppenl_mcast_req *req, enum nss_ppe_mcast_message_types type)
{
	nss_ppenl_cmn_set_ver(&req->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&req->cm, sizeof(struct nss_ppenl_mcast_req), type);
}

#endif /* __NSS_PPENL_MCAST_IF_H */
