/*
 *******************************************************************************
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *******************************************************************************
 */

#include <linux/module.h>
#include <netfn_types.h>

#ifndef __NETFN_FLOW_COOKIE_EXPORT_H
#define __NETFN_FLOW_COOKIE_EXPORT_H

struct netfn_flow_cookie_stats;

/*
 * netfn_flow_cookie_db
 *	Database structure.
 */
struct netfn_flow_cookie_db {
	struct kref ref;		                        	/* Reference count */
	uint32_t max_bits;						/* Max bits for DB hash */

	size_t max_size;						/* Max entries for database */
	atomic_t active_cnt;						/* No. of active entries */
	struct kmem_cache *node_cache;					/* Memory Cache for DB Entries */

	struct dentry *dentry;  	  	                        /* Debug fs entry */
	struct netfn_flow_cookie_stats __percpu *stats;			/* DB specific Per CPU Stats */
	int dump_dev_major_id;						/* Flow Dump File Major ID */
	unsigned long *bitmap;						/* Bit Map */

	spinlock_t lock;						/* DB specific Spin Lock */
	struct hlist_head db_table[] __attribute((aligned(L1_CACHE_BYTES)));
};

/**
 * netfn_flow_cookie structure.
 *	Consists of Flow ID, Sawf Handle and Flow Mark.
 */
struct netfn_flow_cookie {
	uint32_t valid_flag;					/**< Flag tracking cookie params */
	uint32_t flow_id;					/**< Flow ID */
	uint32_t flow_mark;					/**< Flow Mark */
	uint32_t scs_sdwf_hdl;					/**< SCS_SDWF_Handle */
};

/**
 * netfn_flow_cookie_db_alloc
 *	Following API is used to initialize the
 *	Flow DB basing on the no.of entries as
 *	passed from the DB init caller.
 *
 * @param[in] num_entries	32 bit Hash Table size
 *
 * @return
 * netfn_flow_cookie_db handle instantiated.
 *
 */
struct netfn_flow_cookie_db *netfn_flow_cookie_db_alloc(uint32_t num_entries);

/**
 * netfn_flow_cookie_db_ref
 *	Following API is used to increase the
 *	references of the netfn_flow_cookie_db instance provided
 *	to ensure that the DB is protected from parallel accessing.
 *
 * @param[in] db         netfn_flow_cookie_db handle to be referenced
 *
 * @return
 * netfn_flow_cookie_db handle
 *
 */
struct netfn_flow_cookie_db *netfn_flow_cookie_db_ref(struct netfn_flow_cookie_db *db);

/**
 * netfn_flow_cookie_db_deref
 *	Following API is used to free the
 *	Flow DB instance which has been instantiated post it's usage.
 *
 * @param[in] db         netfn_flow_cookie_db handle to be released
 *
 * @return
 * bool
 *
 */
extern bool netfn_flow_cookie_db_deref(struct netfn_flow_cookie_db *db);

/*
 * netfn_flow_cookie_db_add
 *	Following API is used to add the
 *	appropriate DB entry corresponding to the
 *	5 tuple info of the flow into the Hash Table.
 *
 * @param[in] db	netfn_flow_cookie_db handle
 * @param[in] t		Pointer to 5 tuple info
 * @param[in] cookie	Pointer to flow cookie info
 *
 * @return
 * Zero when successful, otherwise error code
 *
 */
extern int netfn_flow_cookie_db_add(struct netfn_flow_cookie_db *db, struct netfn_tuple *t, struct netfn_flow_cookie *cookie);

/**
 * netfn_flow_cookie_db_del
 *	Following API is used to delete the
 *	appropriate DB entry corresponding to the
 *	5 tuple info of the flow from the Hash Table.
 *
 * @param[in] db	netfn_flow_cookie_db handle
 * @param[in] t		Pointer to 5 tuple info
 *
 * @return
 * Zero when successful, otherwise error code
 *
 */
extern int netfn_flow_cookie_db_del(struct netfn_flow_cookie_db *db, struct netfn_tuple *t);

/**
 * netfn_flow_cookie_db_lookup
 *	Following API is used to fetch the entire
 *	Flow Cookie corresponding to the
 *	5 tuple info of the flow from the Hash Table.
 *	Caller needs to invoke this function
 *	using rcu_read_lock()/rcu_read_unlock()
 *	along with holding the DB reference to
 *	ensure that the cookie pointer remains protected
 *	throughout the access period.
 *
 * @param[in] db	netfn_flow_cookie_db handle
 * @param[in] t         Pointer to 5 tuple info
 *
 * @return
 * cookie    Pointer to flow cookie info
 *
 */
struct netfn_flow_cookie *netfn_flow_cookie_db_lookup(struct netfn_flow_cookie_db *db, struct netfn_tuple *t);

#endif		/* __NETFN_FLOW_COOKIE_EXPORT_H */
