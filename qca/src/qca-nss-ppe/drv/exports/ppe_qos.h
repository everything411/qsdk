/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/**
 * @file ppe_qos.h
 *	NSS PPE RFS definitions.
 */

#ifndef _PPE_QOS_H_
#define _PPE_QOS_H_

#include <ppe_drv_public.h>


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
} ppe_qos_ret_t;


/*
 * ppe_qos_req
 *	PPE QoS request information.
 */
struct ppe_qos_req {
	uint8_t int_pri;	/**< INT PRI value of PPE Queue. */
	uint8_t port_id;	/**< PPE port ID. */
	uint16_t ucast_qid;	/**< Unicast queue number of PPE. */
	uint32_t handle_id;	/**< Qdisc handle ID/Class ID corresponding to the PPE Queue. */
	char dev[IFNAMSIZ];	/**< Netdevice. */
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

#endif /* _PPE_QOS_H_ */
