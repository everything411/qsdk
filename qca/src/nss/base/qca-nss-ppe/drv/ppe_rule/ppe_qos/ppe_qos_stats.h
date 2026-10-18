/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __PPE_QOS_STATS_H
#define __PPE_QOS_STATS_H

#define PPE_QOS_STATS_NODE_NAME		"PPE_QOS"

/*
 * ppe_qos_stats
 *	Message structure for ppe general stats
 */
struct ppe_qos_stats {
	atomic64_t qos_create_interface_queues_fail;		/* Qos create interface queues fail */
	atomic64_t qos_create_interface_queues_success;		/* Qos create interface queues success */
	atomic64_t qos_flush_interface_queues_fail;	/* Qos flush interface queues fail */
	atomic64_t qos_flush_interface_queues_success;		/* Qos flush interface queues success */
	atomic64_t qos_set_interface_shaper_fail;		/* Qos det interface shaper fail */
	atomic64_t qos_set_interface_shaper_success;		/* Qos set interface shaper success */
	atomic64_t qos_pq_to_tcont_mapping_fail;	/* Qos priority queue mapping to Tcont fail */
	atomic64_t qos_pq_to_tcont_mapping_success;	/* Qos priority queue mapping to Tcont fail */
	atomic64_t qos_tcont_stats_get_fail;		/* QoS Tcont statistics fetch failed */
	atomic64_t qos_tcont_stats_get_success;		/* QoS Tcont statistics fetch success */
	atomic64_t qos_reset_tcont_credit_fail;		/* QoS Tcont credit reset fail */
	atomic64_t qos_reset_tcont_credit_success;	/* QoS Tcont credit reset success */
	atomic64_t qos_set_queue_tm_fail;	/* Qos set queue traffic management fail */
	atomic64_t qos_set_queue_tm_success;		/* Qos set queue traffic management success */
	atomic64_t qos_set_queue_limit_fail;			/* QoS set queue limit fail */
	atomic64_t qos_set_queue_limit_success;			/* QoS set queue limit success */
	atomic64_t qos_create_shaper_fail;		/* Qos create shaper fail */
	atomic64_t qos_create_shaper_success;		/* Qos create shaper fail */
	atomic64_t qos_delete_shaper_fail;		/* Qos delete shaper fail */
	atomic64_t qos_delete_shaper_success;		/* Qos delete shaper success */
	atomic64_t qos_set_interface_queue_ctrl_fail;	/* Qos set interface queue control fail */
	atomic64_t qos_set_interface_queue_ctrl_success;	/* Qos set interface queue control success */
	atomic64_t qos_mcast_queue_create_fail;		/* Multicast queue creation fail */
	atomic64_t qos_mcast_queue_create_success;	/* Multicast queue creation success */
	atomic64_t qos_mcast_queue_delete_fail;		/* Multicast queue deletion fail */
	atomic64_t qos_mcast_queue_delete_success;	/* Multicast queue deletion success */
	atomic64_t qos_mcast_queue_tm_set_fail;		/* Multicast queue TM set fail */
	atomic64_t qos_mcast_queue_tm_set_success;	/* Multicast queue TM set success */
	atomic64_t qos_mcast_queue_limit_set_fail;	/* Multicast queue limit set fail */
	atomic64_t qos_mcast_queue_limit_set_success;	/* Multicast queue limit set success */
	atomic64_t qos_set_ucast_prio_map_fail;		/* Unicast priority map config fail */
	atomic64_t qos_set_ucast_prio_map_success;	/* Unicast priority map config success */
	atomic64_t qos_set_mcast_prio_map_fail;		/* Multicast priority map config fail */
	atomic64_t qos_set_mcast_prio_map_success;	/* Multicast priority map config success */
};

/*
 * ppe_qos_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_qos_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_qos_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_qos_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_qos_stats_debugfs_init(struct dentry *dentry);
void ppe_qos_stats_debugfs_exit(void);

#endif
