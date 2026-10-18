/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPECFG_QOS_H
#define __PPECFG_QOS_H

#define PPECFG_QOS_HDR_VERSION 4
#define PPECFG_QOS_MCAST_QUEUE_CLASS_MAX 4
#define PPECFG_QOS_MAX_PRIORITY 16
/*
 * PPECFG QOS commands
 */
enum ppecfg_qos_cmd {
	PPECFG_QOS_GET_INT_PRI,	/* GET INTRERNAL PRIORITY */
	PPECFG_QOS_CREATE_SHAPER,	/* Create shaper profile */
	PPECFG_QOS_DELETE_SHAPER,	/* Delete shaper profile */
	PPECFG_QOS_CREATE_INTERFACE_QUEUES,	/* Create interface queues */
	PPECFG_QOS_FLUSH_INTERFACE_QUEUES,	/* Delete interface queues */
	PPECFG_QOS_SET_INTERFACE_SHAPER,	/* Configure interface shaper */
#ifdef NSS_PPE_PON_PORT_FEATURE
	PPECFG_QOS_MAP_PQ_TO_TCONT, 	/* Map Priority Queue to T-cont */
	PPECFG_QOS_GET_TCONT_STATS, 	/* Get Tcont stats */
	PPECFG_QOS_RESET_TCONT_CREDIT, 	/* Reset Tcont credit */
#endif
	PPECFG_QOS_SET_QUEUE_TM, 	/* Set Queue traffic managemment */
	PPECFG_QOS_SET_QUEUE_LIMIT, 	/* Set Queue limit*/
	PPECFG_QOS_SET_INTERFACE_QUEUE_CTRL, 	/* Set interface queue control */
	PPECFG_QOS_SET_UCAST_PRIO_MAP,    /* Set unicast priority map */
	PPECFG_QOS_SET_MCAST_PRIO_MAP,    /* Set multicast priority map */
	PPECFG_QOS_CMD_MAX /* max attribute */
};

/*
 * PPECFG QOS GET INT_PRI
 */
enum ppecfg_qos_get_int_pri {
	PPECFG_QOS_DEV,	/*dev */
	PPECFG_QOS_HANDLE_ID,	/* class id or handle id*/
	PPECFG_QOS_MAX	/* max attribute */
};

/*
 * PPECFG QOS SHAPER
 */
enum ppecfg_qos_shaper {
	PPECFG_QOS_SHAPER_NAME,	/* Shaper name. */
	PPECFG_QOS_SHAPER_DELETE_MAX,	/* Shaper delete max attribute */
	PPECFG_QOS_SHAPER_CIR = PPECFG_QOS_SHAPER_DELETE_MAX,	/* Committed Information Rate. */
	PPECFG_QOS_SHAPER_EIR,	/* Exceed Information Rate. */
	PPECFG_QOS_SHAPER_CBS,	/* Committed burst size. */
	PPECFG_QOS_SHAPER_EBS,	/* Exceed Burst size. */
	PPECFG_QOS_SHAPER_MAX	/* max attribute */
};

/*
 * PPECFG QOS INTERFACE DEVICE
 */
enum ppecfg_qos_interface_dev {
	PPECFG_QOS_INTERFACE_DEV_NAME,		/* interface device name. */
	PPECFG_QOS_INTERFACE_DEV_MAX	/* max attribute */
};

/*
 * PPECFG QOS TCONT
 */
enum ppecfg_qos_tcont {
	PPECFG_QOS_TCONT_ID,		/* T-cont ID. */
	PPECFG_QOS_TCONT_MAX	/* max attribute */
};

/*
 * PPECFG QOS INTERFACE QUEUES
 */
enum ppecfg_qos_interface_queues {
	PPECFG_QOS_INTERFACE_TYPE_PHYSICAL_VP,		/* interface type physical. */
	PPECFG_QOS_INTERFACE_TYPE_TCONT,		/* interface type T-cont. */
	PPECFG_QOS_INTERFACE_FLUSH_MAX,	/* Flush max attribute */
	PPECFG_QOS_INTERFACE_NUM_QUEUES = PPECFG_QOS_INTERFACE_FLUSH_MAX,	/** Number of queues. */
	PPECFG_QOS_INTERFACE_QUEUE_TYPE,	/** Queue type (ucast/mcast). */
	PPECFG_QOS_INTERFACE_MAX	/* max attribute */
};

/*
 * PPECFG QOS INTERFACE SHAPER
 */
enum ppecfg_qos_shaper_interface {
	PPECFG_QOS_SHAPER_INTERFACE_TYPE_PHYSICAL,		/* interface type physical. */
	PPECFG_QOS_SHAPER_INTERFACE_TYPE_TCONT,		/* interface type Tcont. */
	PPECFG_QOS_SHAPER_INTERFACE_SHAPER_NAME,	/* shaper name */
	PPECFG_QOS_SHAPER_INTERFACE_MAX	/* max attribute */
};

/*
 * PPECFG QOS TCONT STATS
 */
enum ppecfg_qos_tcont_stats {
	PPECFG_QOS_TCONT_STATS_TCONT_ID,	/* Tcont ID */
	PPECFG_QOS_TCONT_STATS_MAX	/* max attribute */
};

/*
 * PPECFG QOS PQ MAP
 */
enum ppecfg_qos_pq_map {
	PPECFG_QOS_PQ_MAP_QUEUE_ID,	/* queue ID */
	PPECFG_QOS_PQ_MAP_TCONT_ID,	/* tcont ID */
	PPECFG_QOS_PQ_MAP_MAX	/* max attribute */
};

/*
 * PPECFG QOS QUEUE TM
 */
enum ppecfg_qos_queue_tm {
	PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_PHYSICAL,	/* interface type physical. */
	PPECFG_QOS_QUEUE_TM_INTERFACE_TYPE_TCONT,	/* interface type Tcont. */
	PPECFG_QOS_QUEUE_TM_QUEUE_TYPE,	/** Queue type (ucast/mcast). */
	PPECFG_QOS_QUEUE_TM_ID,	/* queue ID */
	PPECFG_QOS_QUEUE_TM_PRIORITY,	/* queue priority */
	PPECFG_QOS_QUEUE_TM_WEIGHT,	/* queue weight */
	PPECFG_QOS_QUEUE_TM_MAX	/* max attribute */
};

/*
 * PPECFG QOS QUEUE LIMIT
 */
enum ppecfg_qos_queue_limit {
	PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_PHYSICAL,	/* interface type physical */
	PPECFG_QOS_QUEUE_LIMIT_INTERFACE_TYPE_TCONT,	/* interface type Tcont */
	PPECFG_QOS_QUEUE_LIMIT_QUEUE_TYPE,	/** Queue type (ucast/mcast). */
	PPECFG_QOS_QUEUE_LIMIT_ID,	/* queue ID */
	PPECFG_QOS_QUEUE_LIMIT_CEILING,	/* queue ceiling */
	PPECFG_QOS_QUEUE_LIMIT_COLOR_EN,	/* queue color enable */
	PPECFG_QOS_QUEUE_LIMIT_WRED_EN,	/* queue WRED enable */
	PPECFG_QOS_QUEUE_LIMIT_GREEN_MIN_OFF,	/* queue green minimum offset */
	PPECFG_QOS_QUEUE_LIMIT_YELLOW_MAX_OFF,	/* queue yellow maximum offset */
	PPECFG_QOS_QUEUE_LIMIT_YELLOW_MIN_OFF,	/* queue yellow minimum offset */
	PPECFG_QOS_QUEUE_LIMIT_RED_MAX_OFF,	/* queue red maximum offset */
	PPECFG_QOS_QUEUE_LIMIT_RED_MIN_OFF,	/* queue red minimum offset */
	PPECFG_QOS_QUEUE_LIMIT_GREEN_RESUME_OFF,	/* queue green resume offset */
	PPECFG_QOS_QUEUE_LIMIT_YELLOW_RESUME_OFF,	/* queue yellow resume offset */
	PPECFG_QOS_QUEUE_LIMIT_RED_RESUME_OFF,	/* queue red resume offset */
	PPECFG_QOS_QUEUE_LIMIT_MAX	/* max attribute */
};

/*
 * PPECFG QOS INTERFACE QUEUE CONTROL
 */
enum ppecfg_qos_interface_queue_ctrl {
	PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_PHYSICAL,	/* interface type physical */
	PPECFG_QOS_QUEUE_CTRL_INTERFACE_TYPE_TCONT,	/* interface type Tcont */
	PPECFG_QOS_QUEUE_CTRL_MODE,	/* mode: enqueue or dequeue */
	PPECFG_QOS_QUEUE_CTRL_STATE,	/* state: enable or disable */
	PPECFG_QOS_QUEUE_CTRL_MAX	/* max attribute */
};

/*
 * PPECFG QOS UNICAST PRIORITY MAP
 */
enum ppecfg_qos_ucast_prio_map {
	PPECFG_QOS_UCAST_PRIO_MAP_DEV_NAME,    /* Device name */
	PPECFG_QOS_UCAST_PRIO_MAP_PRIO_MAP,      /* Priority map values */
	PPECFG_QOS_UCAST_PRIO_MAP_MAX            /* Max attribute */
};

/*
 * PPECFG QOS MULTICAST PRIORITY MAP
 */
enum ppecfg_qos_mcast_prio_map {
	PPECFG_QOS_MCAST_PRIO_MAP_DEV_NAME,    /* Device name */
	PPECFG_QOS_MCAST_PRIO_MAP_PRIO_MAP,    /* Priority map values */
	PPECFG_QOS_MCAST_PRIO_MAP_MAX          /* Max attribute */
};

#endif /* __PPECFG_QOS_H*/
