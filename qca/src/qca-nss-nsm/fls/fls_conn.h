/*
 **************************************************************************
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 **************************************************************************
 */

#ifndef __FLS_CONN_H
#define __FLS_CONN_H

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/if_ether.h>
#include <sfe_api.h>
#include "fls_sensor_manager.h"
#include "fls_def_sensor.h"

#define FLS_CONN_HASH_SHIFT 12
#define FLS_CONN_HASH_SIZE (1 << FLS_CONN_HASH_SHIFT)
#define FLS_CONN_HASH_MASK (FLS_CONN_HASH_SIZE - 1)
#define FLS_CONN_MAX 8192

enum fls_conn_direction {
	FLS_CONN_DIRECTION_ORIG,
	FLS_CONN_DIRECTION_RET
};

struct fls_conn_stats {
	struct fls_def_sensor_data isd;
};

struct fls_conn {
	uint8_t dir;
	uint8_t ip_version;
	uint8_t protocol;
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
	ktime_t last_ts;	/*last packet arrival*/
};

struct fls_conn_tracker {
	spinlock_t lock;	/* Synchronization lock. */
	struct fls_conn connections[FLS_CONN_MAX];
	struct fls_conn *all_connections_head;
	struct fls_conn *all_connections_tail;
	struct fls_conn *hash[FLS_CONN_HASH_SIZE];
	struct fls_conn *free_list;

	struct fls_sensor_manager fsm;
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
void fls_conn_delete_internal(void *conn);
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
void fls_conn_tracker_init(void);
void fls_conn_flush(void);
#endif
