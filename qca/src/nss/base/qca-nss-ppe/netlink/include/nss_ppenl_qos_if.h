/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
 #include <ppe_qos.h>

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
	union {
		struct nss_ppenl_qos_config config;	/* PPE QoS config. */
		struct ppe_qos_shaper_info shaper_info; /* PPE QoS shaper information */
		struct ppe_qos_interface_queues_info if_info;	/* PPE QoS interface's queues information */
		struct ppe_qos_interface_shaper_info if_shaper_info;	/* PPE QoS interface's shaper information */
#if defined(NSS_PPE_PON_SUPPORT)
		struct ppe_qos_tcont_stats_info stats_info;	/* PPE QoS Tcont stats information */
		struct ppe_qos_pq_to_tcont_info pq_info;	/* PPE QoS priority queue mapping information */
#endif
		struct ppe_qos_queue_tm_info tm_info;	/* PPE QoS traffic management information */
		struct ppe_qos_queue_limit_info limit_info;	/* PPE QoS limit and threshold information */
		struct ppe_qos_interface_queue_ctrl_info queue_ctrl_info;	/* PPE QoS queue control information */
		struct ppe_qos_ucast_prio_map_info ucast_prio_map_info; /* PPE QoS unicast priority map information */
		struct ppe_qos_mcast_prio_map_info mcast_prio_map_info; /* PPE QoS multicast priority map information */
	} msg;
};

/*
 * @brief Message types.
 */
enum nss_ppe_qos_message_types {
	NSS_PPE_QOS_GET_INT_PRI,	/* QoS request create message. */
	NSS_PPE_QOS_CREATE_SHAPER,	/* QoS create shaper profile. */
	NSS_PPE_QOS_DELETE_SHAPER,	/* QoS delete shaper profile. */
	NSS_PPE_QOS_CREATE_INTERFACE_QUEUES,	/* QoS create interface queues. */
	NSS_PPE_QOS_FLUSH_INTERFACE_QUEUES,	/* QoS delete interface queues. */
	NSS_PPE_QOS_SET_INTERFACE_SHAPER,	/* QoS configure interface shaper. */
	NSS_PPE_QOS_GET_TCONT_STATS,	/* QoS get Tcont stats */
	NSS_PPE_QOS_RESET_TCONT_CREDIT,	/* QoS reset Tcont statistics */
	NSS_PPE_QOS_MAP_PQ_TO_TCONT,	/* QoS priority queue to Tcont mapping message. */
	NSS_PPE_QOS_SET_QUEUE_TM,	/* QoS set queue's traffic management message. */
	NSS_PPE_QOS_SET_QUEUE_LIMIT,	/* QoS set queue's limit and threshold message. */
	NSS_PPE_QOS_SET_INTERFACE_QUEUE_CTRL,	/* QoS set interface queue control message. */
	NSS_PPE_QOS_SET_UCAST_PRIO_MAP,    /* QoS set unicast priority map */
	NSS_PPE_QOS_SET_MCAST_PRIO_MAP,    /* QoS set multicast priority map */
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
