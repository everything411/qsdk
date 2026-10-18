/*
 * netfn_capwapmgr_tun.h
 *	Network function's CAPWAP manager's tunnel configuration.
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 */

#ifndef __NETFN_CAPWAPMGR_TUN_H
#define __NETFN_CAPWAPMGR_TUN_H

/*
 * Maximun number of supported tunnels.
 */
#define NETFN_CAPWAPMGR_TUNNELS_MAX 32

/*
 * Stats sync frequency from flowmanager.
 * This is set to once every second as the PPE counters are 32 bits
 * And the max throughput we expect is 1.5Mpps.
 *
 * This ensures that the stats are synced before the coutners overflow.
 */
#define NETFN_CAPWAPMGR_STATS_SYNC_FREQ 1000

/*
 * Tunnel state.
 */
#define NETFN_CAPWAPMGR_TUN_STATE_TIMER_CONFIGURED 0x0001
					/**< Timer is configured for this tunnel */
#define NETFN_CAPWAPMGR_TUN_STATE_FLOW_CONFIGURED 0x0002
					/**< Flow rule is configured for this tunnel */
#define NETFN_CAPWAPMGR_TUN_STATE_CAPWAP_CONFIGURED 0x0004
					/**< CAPWAP rule is configured for this tunnel */
#define NETFN_CAPWAPMGR_TUN_STATE_DTLS_CONFIGURED 0x0008
					/**< DTLS rule is configured for this tunnel */
#define NETFN_CAPWAPMGR_TUN_STATE_DTLS_BIND 0x0010
					/**< DTLS dev is bound to capwap dev for this tunnel */
#define NETFN_CAPWAPMGR_TUN_STATE_DEINIT_IN_PROGRESS 0x0020
					/**< Tunnel deinit in progress */
#define NETFN_CAPWAPMGR_TUN_STATE_DEINIT_DONE 0x0040
					/**< Tunnel deinit done */
#define NETFN_CAPWAPMGR_TUN_STATE_ENABLED 0x0080
					/**< Tunnel enabled */

#define NETFN_CAPWAPMGR_TUN_STATE_MAX 24
					/**< Max tunnel states */

/*
 * netfn_cawpapmgr_tun_ctx.
 *	Private tunnel context associated with the tunnel.
 */
struct netfn_capwapmgr_tun_ctx {
	struct netfn_capwapmgr *mgr;		/*< Pointer to Manager context */
	struct net_device *capwap_dev;		/*< CAPWAP net device associated with this tunnel */
	struct net_device *dtls_dev;		/*< DTLS net device assocaited with this tunnel */
	struct dentry *dentry;			/*< Stats Debugfs file associated with the tunnel */
	struct netfn_capwapmgr_tun_cfg cfg;	/*< Tunnel configuration */
	struct timer_list stats_sync;		/*< Stats sync timer associated with this tunnel */
	struct netfn_capwapmgr_flow_stats stats;
						/*< Flow manager stats */
	atomic64_t timer_resched;		/*< Flag to indicate if the stats sync timer can be re-attached */
	DECLARE_BITMAP(state, NETFN_CAPWAPMGR_TUN_STATE_MAX);
						/*< State of the tunnel */
	struct netfn_flow_cookie_db *db;	/* Store the DB handle here */
	uint32_t flow_count;			/*< Number of flow-ID configured for this tunnel */
};

/*
 * netfn_capwapmgr_tun_init_cfg
 *	Validate tunnel configuration.
 */
extern netfn_capwapmgr_ret_t netfn_capwapmgr_tun_init_cfg(struct netfn_capwapmgr_tun_cfg *cfg);

/*
 * netfn_capwapmgr_tun_init()
 * 	Initialize the tunnel.
 */
extern bool netfn_capwapmgr_tun_init(struct netfn_capwapmgr_tun_ctx *ctx, struct netfn_capwapmgr_tun_cfg *cfg);

/*
 * netfn_capwapmgr_tun_deinint()
 *	Deinitialize the tunnel
 */
extern bool netfn_capwapmgr_tun_deinit(struct netfn_capwapmgr_tun_ctx *ctx);
#endif /* __NETFN_CAPWAPMGR_TUN_H */
