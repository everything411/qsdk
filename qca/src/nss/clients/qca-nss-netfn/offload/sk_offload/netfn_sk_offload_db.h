/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * @file netfn_sk_offload_db.h
 * Netfn Sock Offload DB Management.
 */

/*
 * netfn_sk_offload_db_entry
 *      DB entry to store the mapping between flow index and connection
 */
struct netfn_sk_offload_db_entry {
	struct netfn_sk_offload_ctx __rcu *ctx;	/* Connection Context */
        uint16_t flow_idx;			/* Flow index */
};

/*
 * APIs to manage the flow entry DB
 */
bool netfn_sk_offload_db_add(struct netfn_sk_offload_ctx *ctx, uint16_t flow_idx);
void netfn_sk_offload_db_del(struct netfn_sk_offload_ctx *ctx);
struct netfn_sk_offload_ctx *netfn_sk_offload_db_get_ctx(uint16_t flow_idx);
