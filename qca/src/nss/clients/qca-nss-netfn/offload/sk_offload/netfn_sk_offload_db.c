/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_sk_offload_db.c
 *      Network Sock Offload Flow DB Management.
 */
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/rcupdate.h>
#include "netfn_sk_offload_priv.h"

/*
 * netfn_sk_offload_db_add
 * 	Store the connection info against the corresponding flow index
 */
bool netfn_sk_offload_db_add(struct netfn_sk_offload_ctx *ctx, uint16_t flow_idx)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_db_entry *db = so->db;
	uint8_t id;

	spin_lock_bh(&so->lock);

	id = find_first_zero_bit(so->sk_bitmap, NETFN_SK_OFFLOAD_MAX_CONNS);

	if (id >= NETFN_SK_OFFLOAD_MAX_CONNS) {
		spin_unlock_bh(&so->lock);
		return false;
	}

	WARN_ON(rcu_access_pointer(db[id].ctx));

	rcu_assign_pointer(db[id].ctx, ctx);
	db[id].flow_idx = flow_idx;
	ctx->db_idx = id;
	set_bit(id, so->sk_bitmap);

	spin_unlock_bh(&so->lock);
	return true;

}

/*
 * netfn_sk_offload_db_del
 *	Delete the connection info for the corresponding ctx
 */
void netfn_sk_offload_db_del(struct netfn_sk_offload_ctx *ctx)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_db_entry *db = so->db;
	uint8_t id;

	spin_lock_bh(&so->lock);

	id = ctx->db_idx;

	clear_bit(id, so->sk_bitmap);
	rcu_assign_pointer(db[id].ctx, NULL);
	db[id].flow_idx = 0;

	spin_unlock_bh(&so->lock);
	synchronize_rcu();

	return;
}

/*
 * netfn_sk_offload_db_get_ctx
 * 	Retrieve the ctx associated with the flow index.
 *
 * Caller should ensure to call this function under RCU read lock
 * invoking this API
 */
struct netfn_sk_offload_ctx *netfn_sk_offload_db_get_ctx(uint16_t flow_idx)
{
	struct netfn_sk_offload *so = &g_sk_offload;
	struct netfn_sk_offload_db_entry *db = so->db;
	struct netfn_sk_offload_ctx *ctx = NULL;
	uint8_t id;

	WARN_ON(!rcu_read_lock_bh_held());

	for_each_set_bit(id, so->sk_bitmap, NETFN_SK_OFFLOAD_MAX_CONNS) {
		if (db[id].flow_idx == flow_idx) {
			ctx = rcu_dereference(db[id].ctx);
			break;
		}
	}

	return ctx;
}
