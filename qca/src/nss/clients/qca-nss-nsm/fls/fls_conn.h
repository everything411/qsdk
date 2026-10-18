/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __FLS_CONN_H
#define __FLS_CONN_H

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include "fls_stats.h"
#include <sfe_api.h>
#include "fls_sensor_manager.h"
#include "fls_def_sensor.h"

#define FLS_CONN_HASH_SHIFT 12
#define FLS_CONN_HASH_SIZE (1 << FLS_CONN_HASH_SHIFT)
#define FLS_CONN_HASH_MASK (FLS_CONN_HASH_SIZE - 1)

/*
 * This is inline with SFE max connections for LM profile.
 * Needs to be revisited if we update SFE max connections.
 */
#ifdef FLS_MEM_PROFILE_512
#define FLS_CONN_MAX 4096
#else
#define FLS_CONN_MAX 8192
#endif

enum fls_conn_direction {
	FLS_CONN_DIRECTION_ORIG,
	FLS_CONN_DIRECTION_RET
};

struct fls_conn_stats {
	struct fls_def_sensor_data isd;
};

struct fls_conn_cmn {
	struct fls_def_sensor_timers *timers;
	struct fls_conn *orig;
	struct fls_conn *reply;
};

struct fls_conn {
	uint8_t dir;
	uint8_t ip_version;
	uint8_t protocol;
	uint8_t traffic_class;
	uint32_t src_ip[4];
	uint16_t src_port;
	uint32_t dest_ip[4];
	uint16_t dest_port;
	uint32_t hash;
	uint32_t flags;
	bool externalrule;
	struct fls_conn *reverse;
	struct fls_conn *hash_next;
	struct fls_conn *hash_prev;
	struct fls_conn *all_next;
	struct fls_conn *all_prev;
	struct fls_conn_stats stats;
	struct fls_conn_cmn *cmn;
	ktime_t last_ts;		/* last packet arrival */
	atomic_t fls_conn_counters[FLS_CONN_COUNTERS_MAX];
	atomic_t fls_conn_exception_counters[FLS_CONN_EXCEPTION_COUNTERS_MAX];
};

struct fls_conn_tracker {
	spinlock_t lock;		/* Synchronization lock. */
	struct fls_conn **hash;		/* dynamic hash table */
	struct fls_conn *all_connections_head;		/* active list head */
	struct fls_conn *all_connections_tail;		/* active list tail */
	uint32_t max_connections;		/* soft cap */
	uint32_t num_connections;		/* active count */

	struct fls_sensor_manager fsm;
	atomic_t fls_gbl_counters[FLS_GBL_COUNTERS_MAX];
	atomic_t fls_gbl_exception_counters[FLS_GBL_EXCEPTION_COUNTERS_MAX];
};

extern struct fls_conn_tracker fct;
extern s64 fls_conn_timeout;

extern uint8_t fls_conn_stats_update(void *connection, struct sk_buff *skb);
extern struct fls_conn *fls_conn_lookup(uint8_t ip_version,
											uint8_t protocol,
											uint32_t *src_ip,
											uint16_t src_port,
											uint32_t *dest_ip,
											uint16_t dest_port);
void fls_conn_delete(void *conn);

struct fls_conn *fls_conn_create_flow(uint8_t ip_version,
											uint8_t protocol,
											uint32_t *src_ip,
											uint16_t src_port,
											uint32_t *dest_ip,
											uint16_t dest_port);
struct fls_conn *fls_conn_create_bidiflow(uint8_t ip_version,
										uint8_t protocol,
										uint32_t *orig_src_ip,
										uint16_t orig_src_port,
										uint32_t *orig_dest_ip,
										uint16_t orig_dest_port,
										bool isexternal, ktime_t last_ts);
extern void fls_conn_create(uint8_t ip_version,
										uint8_t protocol,
										uint32_t *orig_src_ip,
										uint16_t orig_src_port,
										uint32_t *orig_dest_ip,
										uint16_t orig_dest_port,
										void **orig_conn,
										void **repl_conn);
int fls_conn_tracker_init(void);
void fls_conn_flush(void);
void fls_conn_tracker_exit(void);
#endif
