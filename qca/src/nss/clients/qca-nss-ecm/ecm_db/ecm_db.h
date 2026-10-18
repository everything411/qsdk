/*
 **************************************************************************
 * Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * API's
 */
#ifndef ECM_DB_H_
#define ECM_DB_H_

#include "ecm_db_connection.h"
#include "ecm_db_mapping.h"
#include "ecm_db_host.h"
#include "ecm_db_node.h"
#include "ecm_db_iface.h"
#include "ecm_db_listener.h"
#include "ecm_db_multicast.h"
#include "ecm_db_timer.h"

extern spinlock_t ecm_db_lock;

/*
 * Management thread control
 */
extern bool ecm_db_terminate_pending;	/* When true the user has requested termination */

/*
 * Random seed used during hash calculations
 */
extern uint32_t ecm_db_jhash_rnd __read_mostly;

#ifdef ECM_DB_PER_CLIENT_ROUTED_STATS_ENABLE
int ecm_db_per_client_routed_stats_state_write(struct ecm_state_file_instance *sfi,
					       uint64_t from_data_routed, uint64_t to_data_routed,
					       uint64_t from_packet_routed, uint64_t to_packet_routed);
#endif

int ecm_db_adv_stats_state_write(struct ecm_state_file_instance *sfi,uint64_t from_data_total, uint64_t to_data_total,
				uint64_t from_packet_total, uint64_t to_packet_total, uint64_t from_data_total_dropped,
				uint64_t to_data_total_dropped, uint64_t from_packet_total_dropped, uint64_t to_packet_total_dropped);

int ecm_db_init(struct dentry *dentry);
void ecm_db_exit(void);
#endif /* ECM_DB_H_ */
