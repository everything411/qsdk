/*
 **************************************************************************
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * @file netfn_sk_offload_priv.h
 * Netfn Sock Offload Module header file.
 */
#ifndef __NETFN_SK_OFFLOAD_PRIV_H
#define __NETFN_SK_OFFLOAD_PRIV_H

#include <net/sock.h>

#include <netfn_types.h>
#include <ppe_vp_public.h>

#include <netfn_sk_offload.h>
#include "netfn_sk_offload_db.h"
#include "netfn_sk_offload_rx.h"
#include "netfn_sk_offload_stats.h"

#define NETFN_SK_OFFLOAD_MAX_CONNS 128

#define NETFN_SOCK_OFFLOAD_RFS_CORES 7

extern int rx_core_mask;

extern struct netfn_sk_offload g_sk_offload;

/*
 * netfn_sk_offload
 *	 Netfn sock offload global context.
 */
struct netfn_sk_offload {
	struct net_device *vp_dev;	/* VP netdev */
	struct netfn_sk_offload_stats __percpu *stats;
				/* Global Per CPU stats */
	struct dentry *dentry;		/* Debugfs entry */
	spinlock_t lock;		/* Global lock */

	/*
	 * TODO: Optimization: Maintain per CPU DBs for efficient access.
	 */
	struct netfn_sk_offload_db_entry db[NETFN_SK_OFFLOAD_MAX_CONNS];
					/* DB Array for ctx to flow_idx mapping */
	DECLARE_BITMAP(sk_bitmap, NETFN_SK_OFFLOAD_MAX_CONNS);
					/*Bit map to maintain ID for given ctx */
	atomic_t num_conns;		/* Total connections */
	ppe_vp_num_t vp_num;		/* VP number associated with sk offload instance */
};

/*
 * netfn_sk_offload_ctx
 *	Netfn sock offload per connection context.
 */
struct netfn_sk_offload_ctx {
	struct sock *sk;			/* Socket associated with Connection */
	struct netfn_sk_offload_stats_ctx __percpu *stats;
						/* Context specific stats */
	struct netfn_tuple tuple;		/* Context tuple */
	void (*gro_rcv)(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_cb_info *info);
						/* SKB-based GRO receive */
#ifdef NETFN_SK_OFFLOAD_HW_GRO
	void (*gro_rcv_pages)(struct netfn_sk_offload_ctx *ctx, struct ppe_vp_xdp_cb_info *info);
						/* XDP-based page pool GRO receive */
#endif
	struct netfn_sk_offload_rx_state gro_state;	/* GRO state */
	uint16_t flow_idx;			/* Flow Index */
	uint16_t db_idx : __builtin_ctz(NETFN_SK_OFFLOAD_MAX_CONNS);
						/* DB Index */
	/*
	 * TODO: Add CPU ID.
	 */

	struct napi_notifier_info napi_info;	/* NAPI completion info */
	uint8_t iphdr_sz;               /* IP header size */
	__be16 etype;			/* Ethernet type */
	struct net_device *dev;		/* Egress net device */

};


#endif /*__NETFN_SK_OFFLOAD_PRIV_H */
