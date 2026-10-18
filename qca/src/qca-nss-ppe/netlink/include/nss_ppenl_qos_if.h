/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/*
 * @file nss_ppenl_qos_if.h
 *	NSS PPE Netlink QOS
 */
#ifndef __NSS_PPENL_QOS_IF_H
#define __NSS_PPENL_QOS_IF_H

/*
 * QOS Configure Family.
 */
#define NSS_PPENL_QOS_FAMILY "nss_ppenl_qos"

/*
 * @brief QOS config.
 */
struct nss_ppenl_qos_config {
	uint32_t handle_id;	/* User given qos ID. */
	char dev[IFNAMSIZ];	/* Dev name for port qos. */
	uint8_t int_pri;	/* INT_PRI value of ppe queue. */
	uint8_t port_id;	/* PPE port ID. */
	uint16_t ucast_qid;	/* Unicast queue id of ppe queue. */
	int ret;		/* Return value to userspace. */
};


/*
 * @brief QOS req.
 */
struct nss_ppenl_qos_req {
	struct nss_ppenl_cmn cm;	/*< Common message header. */
	struct nss_ppenl_qos_config config;	/* PPE QoS config. */
};

/*
 * @brief Message types.
 */
enum nss_ppe_qos_message_types {
	NSS_PPE_QOS_GET_INT_PRI,	/* QoS request create message. */
	NSS_PPE_QOS_MAX_MSG_TYPES		/* Maximum message type. */
};

/**
 * @brief NETLINK QOS message init.
 *
 * @param[IN] req  NSS NETLINK QOS req.
 * @param[IN] type QOS message type.
 * @return
 * None
 */
static inline void nss_ppenl_qos_req_init(struct nss_ppenl_qos_req *req, enum nss_ppe_qos_message_types type)
{
	nss_ppenl_cmn_set_ver(&req->cm, NSS_PPENL_VER);
	nss_ppenl_cmn_init_cmd(&req->cm, sizeof(struct nss_ppenl_qos_req), type);
}

#endif /* __NSS_PPENL_QOS_IF_H */
