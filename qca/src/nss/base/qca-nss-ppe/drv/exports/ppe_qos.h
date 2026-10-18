/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_qos.h
 *	NSS PPE RFS definitions.
 */

#ifndef _PPE_QOS_H_
#define _PPE_QOS_H_

#include <linux/if.h>

#define PPE_QOS_MAX_NAME_LENGTH 64

/*
 * ppe_qos_ret
 *	PPE QoS return status types.
 */
typedef enum ppe_qos_ret {
	PPE_QOS_SUCCESS = 0,	/**< Success. */
	PPE_QOS_CLASS_NON_LEAF,	/**< Class is not a leaf node. */
	PPE_QOS_INVALID_HANDLE_ID,	/**< Class ID is not present in Qos heirarchy. */
	PPE_QOS_INVALID_DEV,	/**< Interface not valid. */
	PPE_QOS_FAIL,		/**< Failure. */
	PPE_QOS_CREATE_SHAPER_FAIL,	/**< Shaper creation failure.. */
	PPE_QOS_DELETE_SHAPER_FAIL,	/**< shaper delete failure. */
	PPE_QOS_CREATE_INTERFACE_QUEUES_FAIL,	/**< Interface's queue creation faliure. */
	PPE_QOS_FLUSH_INTERFACE_QUEUES_FAIL,	/**< Interface's queue deletion faliure. */
	PPE_QOS_SET_INTERFACE_SHAPER_FAIL,	/**< Interface's shaper configuration faliure. */
	PPE_QOS_MAP_PQ_TO_TCONT_FAIL,	/**< Priority queue to Tcont mapping faliure. */
	PPE_QOS_TCONT_STATS_GET_FAIL,	/**< Tcont stats fetch failure. */
	PPE_QOS_RESET_TCONT_CREDIT_FAIL,	/**< Tcont credit reset failure. */
	PPE_QOS_SET_QUEUE_TM_FAIL,	/**< Queue's traffic management configuration faliure. */
	PPE_QOS_SET_QUEUE_LIMIT_FAIL,	/**< Queue's thresholds configuration faliure. */
	PPE_QOS_SET_INTERFACE_QUEUE_CTRL_FAIL,	/**< Interface queue control configuration failure. */
	PPE_QOS_SET_UCAST_PRIO_MAP_FAIL,	/**< Unicast priority map configuration failure. */
} ppe_qos_ret_t;

/*
 * ppe_qos_queue_ctrl_mode
 *	PPE QoS queue control mode types.
 */
typedef enum ppe_qos_queue_ctrl_mode {
	PPE_QOS_QUEUE_CTRL_MODE_ENQUEUE = 0,	/**< Enqueue control. */
	PPE_QOS_QUEUE_CTRL_MODE_DEQUEUE = 1,	/**< Dequeue control. */
} ppe_qos_queue_ctrl_mode_t;

/*
 * ppe_qos_interface_type
 *	PPE QoS interface types.
 * TODO: Extend the support for virtual ports
 */
typedef enum ppe_qos_interface_type {
	PPE_QOS_INTERFACE_TYPE_PHYSICAL_VP = 0,	/**< Physical or VP interface. */
	PPE_QOS_INTERFACE_TYPE_TCONT,	/**< Tcont interface. */
} ppe_qos_interface_type_t;

/**
 * ppe_qos_queue_type_t - Queue type enumeration
 */
typedef enum ppe_qos_queue_type {
	PPE_QOS_QUEUE_TYPE_UCAST = 0,	/**< Unicast queue type. */
	PPE_QOS_QUEUE_TYPE_MCAST,	/**< Multicast queue type. */
	PPE_QOS_QUEUE_TYPE_MAX		/**< Maximum queue types. */
} ppe_qos_queue_type_t;

/*
 * ppe_qos_interface
 *	PPE QoS interface information.
 * TODO: Extend the support for virtual ports
 */
 struct ppe_qos_interface {
	union {
		char dev[IFNAMSIZ];			/** Device name */
		uint32_t tcont_id;			/** Tcont ID */
	} interface;					/** interface info. */
	ppe_qos_interface_type_t type;		/** interface type */
};

/*
 * ppe_qos_req
 *	PPE QoS request information.
 */
struct ppe_qos_req {
	uint8_t int_pri;	/**< INT PRI value of PPE Queue. */
	uint8_t port_id;	/**< PPE port ID. */
	uint32_t port_type;		/** Port type, 0: Tcont, 1: UNI .*/
	uint16_t ucast_qid;	/**< Unicast queue number of PPE. */
	uint32_t handle_id;	/**< Qdisc handle ID/Class ID corresponding to the PPE Queue. */
	char dev[IFNAMSIZ];	/**< Netdevice. */
};

/*
 * ppe_qos_shaper_info
 *	PPE QoS shaper profile information.
 */
struct ppe_qos_shaper_info {
	char name[PPE_QOS_MAX_NAME_LENGTH];	/** Shaper name. */
	uint32_t cir;	/** Committed Information Rate. */
	uint32_t eir;	/** Exceed Information Rate. */
	uint32_t cbs;	/** Committed burst size. */
	uint32_t ebs;	/** Exceed Burst size. */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * ppe_qos_queue_limit_info
 *	PPE QoS queue limit configuration.
 */
struct ppe_qos_queue_limit_info {
	struct ppe_qos_interface if_data;		/** interface data */
	uint32_t queue_id;	/** Queue number. */
	uint32_t ceiling;	/** Queue ceiling */
	bool color_en;		/** Color enable */
	bool wred_en;		/** WRED enable */
	uint32_t green_min_off;		/** Green minimum offset */
	uint32_t yellow_max_off;	/** Yellow maximum offset */
	uint32_t yellow_min_off;	/** Yellow minimum offset */
	uint32_t red_max_off;		/** Red maximum offset */
	uint32_t red_min_off;		/** Red minimum offset */
	uint32_t green_resume_off;	/** Green resume offset */
	uint32_t yellow_resume_off;	/** Yellow resume offset */
	uint32_t red_resume_off;	/** Red resume offset */
	ppe_qos_queue_type_t queue_type;	/** Queue type (unicast/multicast) */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * ppe_qos_queue_tm_info
 *	PPE QoS queue traffic management information.
 */
struct ppe_qos_queue_tm_info {
	struct ppe_qos_interface if_data;		/** interface data */
	uint32_t queue_id;	/** Queue number. */
	uint32_t priority;	/** Priority of the queue. */
	uint32_t weight;	/** Weight assigned to the queue. */
	ppe_qos_queue_type_t queue_type;	/** Queue type (unicast/multicast) */
	enum ppe_qos_ret ret;		/** Return status */
};

#ifdef NSS_PPE_PON_SUPPORT
/*
 * ppe_qos_tcont_stats_info
 *	PPE QoS Tcont statistics information.
 */
struct ppe_qos_tcont_stats_info {
	uint32_t tcont_id;	/** T-cont ID. */
	uint32_t credit;	/** T-cont credit. */
	uint64_t pend_bytes;	/** T-cont pending bytes. */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * ppe_qos_pq_to_tcont_info
 *	PPE QoS priority queue to tcont mapping information.
 */
struct ppe_qos_pq_to_tcont_info {
	uint32_t queue_id;	/** Queue ID. */
	uint32_t tcont_id;	/** T-cont ID. */
	enum ppe_qos_ret ret;		/** Return status */
};
#endif

/*
 * ppe_qos_interface_queues_info
 *	PPE QoS T-cont/UNI queues information.
 */
struct ppe_qos_interface_queues_info {
	struct ppe_qos_interface if_data;		/** interface data */
	uint32_t num_queues;	/** Number of queues. */
	ppe_qos_queue_type_t queue_type;	/** Queue type (unicast/multicast) */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * ppe_qos_interface_shaper_info
 *	PPE QoS T-cont/UNI shaper information.
 */
struct ppe_qos_interface_shaper_info {
	struct ppe_qos_interface if_data;		/** interface data */
	char shaper_name[PPE_QOS_MAX_NAME_LENGTH];	/** Shaper name. */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * ppe_qos_queue_ctrl_state
 *	PPE QoS queue control state types.
 */
typedef enum ppe_qos_queue_ctrl_state {
	PPE_QOS_QUEUE_CTRL_STATE_DISABLE = 0,	/**< Disable state. */
	PPE_QOS_QUEUE_CTRL_STATE_ENABLE = 1,	/**< Enable state. */
	PPE_QOS_QUEUE_CTRL_STATE_DROP = 2,	/**< Drop state (dequeue only). */
} ppe_qos_queue_ctrl_state_t;

/*
 * ppe_qos_interface_queue_ctrl_info
 *	PPE QoS interface queue control information.
 */
struct ppe_qos_interface_queue_ctrl_info {
	struct ppe_qos_interface if_data;		/** interface data */
	ppe_qos_queue_ctrl_mode_t mode;			/** enqueue or dequeue */
	ppe_qos_queue_ctrl_state_t state;		/** enable, disable, or drop */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * Maximum number of priorities supported by PPE hardware
 */
#define PPE_DRV_MAX_PRIORITY 16

/*
 * ppe_qos_ucast_prio_map_info
 *	PPE QoS unicast priority map information.
 */
struct ppe_qos_ucast_prio_map_info {
	struct ppe_qos_interface if_data;    /** Interface data */
	uint8_t prio_map[PPE_DRV_MAX_PRIORITY]; /** Priority map values */
	enum ppe_qos_ret ret;		/** Return status */
};

/*
 * ppe_qos_mcast_prio_map_info
 *	PPE QoS multicast priority map information.
 */
struct ppe_qos_mcast_prio_map_info {
	struct ppe_qos_interface if_data;    /** Interface data */
	uint8_t prio_map[PPE_DRV_MAX_PRIORITY]; /** Priority map values (0-3) */
	enum ppe_qos_ret ret;		/** Return status */
};

/**
 * ppe_qos_get_int_pri_func
 *	Function to fetch PPE QoS internal priority for a Qdisc/leaf class.
 *
 *
 * @param[in] req         PPE QoS request's information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_get_int_pri_func(struct ppe_qos_req *req);

/**
 * ppe_qos_delete_shaper
 *	Function to delete shaper profile.
 *
 *
 * @param[in] info         PPE QoS shaper profile information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_delete_shaper(struct ppe_qos_shaper_info *info);

/**
 * ppe_qos_create_shaper
 *	Function to create shaper profile.
 *
 *
 * @param[in] info         PPE QoS shaper profile information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_create_shaper(struct ppe_qos_shaper_info *info);

/**
 * ppe_qos_set_queue_limit
 *	Function to set queue limit and thresholds for a given queue.
 *
 *
 * @param[in] info         PPE QoS queue's traffic management information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_set_queue_limit(struct ppe_qos_queue_limit_info *info);

/**
 * ppe_qos_set_queue_tm
 *	Function to set QoS traffic management for a given interface's queue.
 *
 *
 * @param[in] info         PPE QoS queue's traffic management information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_set_queue_tm(struct ppe_qos_queue_tm_info *info);

#ifdef NSS_PPE_PON_SUPPORT
/**
 * ppe_qos_get_tcont_stats
 *	Function to get statistics for a given Tcont ID.
 *
 *
 * @param[in] info         PPE QoS T-cont statistics information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_get_tcont_stats(struct ppe_qos_tcont_stats_info *info);

/**
 * ppe_qos_reset_tcont_credit
 *	Function to reset credit of a given Tcont ID.
 *
 *
 * @param[in] info         PPE QoS T-cont information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_reset_tcont_credit(struct ppe_qos_tcont_stats_info *info);

/**
 * ppe_qos_map_pq_to_tcont
 *	Function to map priority queue to a given Tcont ID.
 *
 *
 * @param[in] info         PPE QoS priority queue mapping information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_map_pq_to_tcont(struct ppe_qos_pq_to_tcont_info *info);
#endif

/**
 * ppe_qos_set_interface_shaper
 *	Function to set shaper at interface.
 *
 *
 * @param[in] info         PPE QoS interface's shaper information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_set_interface_shaper(struct ppe_qos_interface_shaper_info *info);

/**
 * ppe_qos_flush_interface_queues
 *	Function to flush and delete number of queues for a given t-cont/UNI.
 *
 *
 * @param[in] info         PPE QoS interface information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_flush_interface_queues(struct ppe_qos_interface_queues_info *info);

/**
 * ppe_qos_create_interface_queues
 *	Function to create number of queues for a given t-cont/UNI.
 *
 *
 * @param[in] info         PPE QoS interface information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_create_interface_queues(struct ppe_qos_interface_queues_info *info);

/**
 * ppe_qos_set_interface_queue_ctrl
 *	Function to enable/disable enqueue or dequeue for all queues on an interface.
 *
 *
 * @param[in] info         PPE QoS interface queue control information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_set_interface_queue_ctrl(struct ppe_qos_interface_queue_ctrl_info *info);

/**
 * ppe_qos_set_ucast_prio_map
 *	Function to set unicast priority map for a given profile ID.
 *
 *
 * @param[in] info         PPE QoS unicast priority map information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_set_ucast_prio_map(struct ppe_qos_ucast_prio_map_info *info);

/**
 * ppe_qos_set_mcast_prio_map
 *	Function to set multicast priority map for a given port.
 *
 *
 * @param[in] info         PPE QoS multicast priority map information.
 * @return
 * PPE QoS request's return status.
 */
ppe_qos_ret_t ppe_qos_set_mcast_prio_map(struct ppe_qos_mcast_prio_map_info *info);

#endif /* _PPE_QOS_H_ */
