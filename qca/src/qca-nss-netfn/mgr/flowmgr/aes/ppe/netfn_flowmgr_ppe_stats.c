/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_ppe_stats.c
 *	Netfn flow manager ppe stats file
 */

#include <flowmgr/netfn_flowmgr_priv.h>
#include "netfn_flowmgr_ppe_ipv6.h"
#include <ppe_drv.h>
#include <ppe_drv_v4.h>
#include <ppe_drv_v6.h>
#include <ppe_acl.h>

/*
 * Declarations related to IPv4 stats sync
 */
#define NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_PERIOD msecs_to_jiffies(ipv4_stats_sync_period)
                                /* Default stats sync happens every 60ms, such that max of 2K conn are synced in 1sec; (1000ms / 60ms) * 128 = ~2K */
#define NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_COUNT 128     /* Sync max of 128 connections from ppe */

/*
 * Declaration related to IPv6 stats sync
 */
#define NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_PERIOD msecs_to_jiffies(ipv6_stats_sync_period)
                                /* Default stats sync happens every 60ms, such that max of 2K conn are synced in 1sec; (1000ms / 60ms) * 128 = ~2K */
#define NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_COUNT 128     /* Sync max of 128 connections from ppe */

/*
 * Workqueue for IPv4 connection sync
 */
static struct workqueue_struct *netfn_flowmgr_ppe_ipv4_workqueue;
static struct delayed_work netfn_flowmgr_ppe_ipv4_work;
static struct ppe_drv_v4_conn_sync_many netfn_flowmgr_ppe_ipv4_stats_sync_req_msg;

/*
 * Workqueue for IPv6 connection sync
 */
static struct workqueue_struct *netfn_flowmgr_ppe_ipv6_workqueue;
static struct delayed_work netfn_flowmgr_ppe_ipv6_work;
static struct ppe_drv_v6_conn_sync_many netfn_flowmgr_ppe_ipv6_stats_sync_req_msg;

/*
 * netf_flowmgr_ipv4_process_one_conn_sync_msg()
 *	Process one connection sync message.
 */
static inline void netfn_flowmgr_ppe_ipv4_process_one_conn_sync_msg(struct ppe_drv_v4_conn_sync *sync)
{
	struct netfn_flowmgr_conn_stats conn_stats;
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ipv4_stats_callback_t sync_cb;
	void *sync_data;

	sync_cb = f->stats_sync_v4_cb;
	sync_data = f->stats_sync_v4_data;

	if (sync_cb == NULL) {
		netfn_flowmgr_warn("No callback registered for stats sync\n");
		return;
	}

	/*
	 * Copy the sync data to the conn stats structure to update the user stats.
	 */
	conn_stats.ip_version = ETH_P_IP;
	conn_stats.protocol = sync->protocol;

	/*
	 * original direction stats
	 */
	conn_stats.org_src_ip[0] = sync->flow_ip;
	conn_stats.org_dest_ip[0] = sync->return_ip;
	conn_stats.org_src_ident = sync->flow_ident;
	conn_stats.org_dest_ident = sync->return_ident;
	conn_stats.org_tx_pkt_count = sync->flow_tx_packet_count;
	conn_stats.org_rx_pkt_count = sync->flow_rx_packet_count;
	conn_stats.org_tx_byte_count = sync->flow_tx_byte_count;
	conn_stats.org_rx_byte_count = sync->flow_rx_byte_count;

	/*
	 * reply direction stats
	 */
	conn_stats.reply_src_ip[0] = sync->return_ip;
	conn_stats.reply_dest_ip[0] = sync->flow_ip;
	conn_stats.reply_src_ident = sync->return_ident;
	conn_stats.reply_dest_ident = sync->flow_ident;
	conn_stats.reply_tx_pkt_count = sync->return_tx_packet_count;
	conn_stats.reply_rx_pkt_count = sync->return_rx_packet_count;
	conn_stats.reply_tx_byte_count = sync->return_tx_byte_count;
	conn_stats.reply_rx_byte_count = sync->return_rx_byte_count;

	/*
	 * Call user registered callback
	 */
	sync_cb(sync_data, &conn_stats);
}

/*
 * netfn_flowmgr_ppe_ipv4_stats_callback()
 *	Callback handler from the PPE.
 */
static void netfn_flowmgr_ppe_ipv4_stats_callback(void *app_data, struct ppe_drv_v4_conn_sync *conn_sync)
{
	netfn_flowmgr_ppe_ipv4_process_one_conn_sync_msg(conn_sync);
}

/*
 * netfn_flowmgr_ppe_ipv4_stats_sync_req_work()
 *	Schedule delayed work to process connection stats and request next sync
 */
static void netfn_flowmgr_ppe_ipv4_stats_sync_req_work(struct work_struct *work)
{
	/*
	 * Prepare ppe_ipv4_msg with CONN_STATS_SYNC_MANY request
	 */
	int i;

	memset(netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.conn_sync, 0, sizeof(struct ppe_drv_v4_conn_sync) * NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_COUNT);

	netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.count = 0;
	ppe_drv_v4_conn_sync_many(&netfn_flowmgr_ppe_ipv4_stats_sync_req_msg, NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_COUNT);

	for (i = 0; i < netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.count; i++) {
		netfn_flowmgr_ppe_ipv4_process_one_conn_sync_msg(&netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.conn_sync[i]);
	}

	netfn_flowmgr_warn("Rescheduling sync stats work\n");
	queue_delayed_work(netfn_flowmgr_ppe_ipv4_workqueue, &netfn_flowmgr_ppe_ipv4_work, NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_PERIOD);
}

/*
 * netfn_flowmgr_ppe_ipv4_stats_sync_workqueue_deinit
 *	Deinitialize the workqueue for ipv4 stats sync
 */
static void netfn_flowmgr_ppe_ipv4_stats_sync_workqueue_deinit(void)
{
	/*
	 * Cancel the stats sync req work and destroy workqueue
	 */
	cancel_delayed_work_sync(&netfn_flowmgr_ppe_ipv4_work);
	destroy_workqueue(netfn_flowmgr_ppe_ipv4_workqueue);
	vfree(netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.conn_sync);
}

/*
 * netfn_flowmgr_ppe_ipv4_stats_sync_workqueue_init
 *	Initialize the workqueue for ipv4 stats sync
 */
static bool netfn_flowmgr_ppe_ipv4_stats_sync_workqueue_init(void)
{
	netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.conn_sync = vzalloc(sizeof(struct ppe_drv_v4_conn_sync) * NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_COUNT);
	if (!netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.conn_sync) {
		netfn_flowmgr_warn("Memory allocation failed for ppe_drv_v4_conn_sync_many\n");
		return false;
	}

	netfn_flowmgr_ppe_ipv4_workqueue = create_singlethread_workqueue("netfn_flowmgr_ppe_ipv4_workqueue");
	if (!netfn_flowmgr_ppe_ipv4_workqueue) {
		vfree(netfn_flowmgr_ppe_ipv4_stats_sync_req_msg.conn_sync);
		return false;
	}

	INIT_DELAYED_WORK(&netfn_flowmgr_ppe_ipv4_work, netfn_flowmgr_ppe_ipv4_stats_sync_req_work);
	queue_delayed_work(netfn_flowmgr_ppe_ipv4_workqueue, &netfn_flowmgr_ppe_ipv4_work, NETFN_FLOWMGR_PPE_IPV4_STATS_SYNC_PERIOD);
	return true;
}


/*
 * netfn_flowmgr_ppe_ipv4_stats_deinit()
 *	PPE IPv4 stats de-initialization.
 */
void netfn_flowmgr_ppe_ipv4_stats_deinit(void)
{
	netfn_flowmgr_warn("PPE IPv4 stats deinit, unregister IPv4 callback with PPE\n");
	/*
	 * Unregister stats callback with PPE driver
	 */
	ppe_drv_v4_stats_callback_unregister();

	/*
	 * Clean up the stats sync queue/work
	 */
	netfn_flowmgr_ppe_ipv4_stats_sync_workqueue_deinit();
}

/*
 * netfn_flowmgr_ppe_ipv4_stats_init()
 *	PPE IPv4 stats initialization.
 */
bool netfn_flowmgr_ppe_ipv4_stats_init(void)
{
	netfn_flowmgr_warn("PPE IPv4 stats init, register IPv4 callback with PPE\n");
	/*
	 * Register a stats callback with PPE driver for any exception in PPE for a IPv4 flow.
	 */
	if (!ppe_drv_v4_stats_callback_register(netfn_flowmgr_ppe_ipv4_stats_callback, NULL)) {
		netfn_flowmgr_warn("Failed to register IPv4 stats callback\n");
		return false;
	}

	/*
	 * Initialize the workqueue for stats sync.
	 */
	if (!netfn_flowmgr_ppe_ipv4_stats_sync_workqueue_init()) {
		netfn_flowmgr_warn("Failed to create workqueue for IPv4 stats sync\n");
		return false;
	}
	return true;
}

/*
 * netf_flowmgr_ipv6_process_one_conn_sync_msg()
 *	Process one connection sync message.
 */
static inline void netfn_flowmgr_ppe_ipv6_process_one_conn_sync_msg(struct ppe_drv_v6_conn_sync *sync)
{
	struct netfn_flowmgr_conn_stats conn_stats;
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	netfn_flowmgr_ipv6_stats_callback_t sync_cb;
	void *sync_data;

	sync_cb = f->stats_sync_v6_cb;
	sync_data = f->stats_sync_v6_data;

	if (sync_cb == NULL) {
		netfn_flowmgr_warn("No callback registered for IPv6 stats sync\n");
		return;
	}

	/*
	 * Copy the sync data to the conn stats structure to update the user stats.
	 */
	conn_stats.ip_version = ETH_P_IPV6;
	conn_stats.protocol = sync->protocol;

	/*
	 * original direction stats
	 */
	memcpy(conn_stats.org_src_ip, sync->flow_ip, sizeof(uint32_t) * 4);
	memcpy(conn_stats.org_dest_ip, sync->return_ip, sizeof(uint32_t) * 4);
	conn_stats.org_src_ident = sync->flow_ident;
	conn_stats.org_dest_ident = sync->return_ident;
	conn_stats.org_tx_pkt_count = sync->flow_tx_packet_count;
	conn_stats.org_rx_pkt_count = sync->flow_rx_packet_count;
	conn_stats.org_tx_byte_count = sync->flow_tx_byte_count;
	conn_stats.org_rx_byte_count = sync->flow_rx_byte_count;

	/*
	 * reply direction stats
	 */
	memcpy(conn_stats.reply_src_ip, sync->return_ip, sizeof(uint32_t) * 4);
	memcpy(conn_stats.reply_dest_ip, sync->flow_ip, sizeof(uint32_t) * 4);
	conn_stats.reply_src_ident = sync->return_ident;
	conn_stats.reply_dest_ident = sync->flow_ident;
	conn_stats.reply_tx_pkt_count = sync->return_tx_packet_count;
	conn_stats.reply_rx_pkt_count = sync->return_rx_packet_count;
	conn_stats.reply_tx_byte_count = sync->return_tx_byte_count;
	conn_stats.reply_rx_byte_count = sync->return_rx_byte_count;

	/*
	 * Call user registered callback
	 */
	sync_cb(sync_data, &conn_stats);
}

/*
 * netfn_flowmgr_ppe_ipv6_stats_callback()
 *	Callback handler from PPE.
 */
static void netfn_flowmgr_ppe_ipv6_stats_callback(void *app_data, struct ppe_drv_v6_conn_sync *conn_sync)
{
	netfn_flowmgr_ppe_ipv6_process_one_conn_sync_msg(conn_sync);
}

/*
 * netfn_flowmgr_ppe_ipv6_stats_sync_req_work()
 *	Schedule delayed work to process connection stats and request next sync
 */
static void netfn_flowmgr_ppe_ipv6_stats_sync_req_work(struct work_struct *work)
{
	/*
	 * Prepare ppe_ipv6_msg with CONN_STATS_SYNC_MANY request
	 */
	int i;

	memset(netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.conn_sync, 0, sizeof(struct ppe_drv_v6_conn_sync) * NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_COUNT);

	netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.count = 0;
	ppe_drv_v6_conn_sync_many(&netfn_flowmgr_ppe_ipv6_stats_sync_req_msg, NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_COUNT);

	for (i = 0; i < netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.count; i++) {
		netfn_flowmgr_ppe_ipv6_process_one_conn_sync_msg(&netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.conn_sync[i]);
	}

	netfn_flowmgr_warn("Rescheduling sync stats work\n");
	queue_delayed_work(netfn_flowmgr_ppe_ipv6_workqueue, &netfn_flowmgr_ppe_ipv6_work, NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_PERIOD);
}

/*
 * netfn_flowmgr_ppe_ipv6_stats_sync_workqueue_deinit
 *	Deinitialize the workqueue for ipv6 stats sync
 */
static void netfn_flowmgr_ppe_ipv6_stats_sync_workqueue_deinit(void)
{
	/*
	 * Cancel the stats sync req work and destroy workqueue
	 */
	cancel_delayed_work_sync(&netfn_flowmgr_ppe_ipv6_work);
	destroy_workqueue(netfn_flowmgr_ppe_ipv6_workqueue);
	vfree(netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.conn_sync);
}

/*
 * netfn_flowmgr_ppe_ipv6_stats_sync_workqueue_init
 *	Initialize the workqueue for ipv6 stats sync
 */
static bool netfn_flowmgr_ppe_ipv6_stats_sync_workqueue_init(void)
{
	netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.conn_sync = vzalloc(sizeof(struct ppe_drv_v6_conn_sync) * NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_COUNT);
	if (!netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.conn_sync) {
		netfn_flowmgr_warn("Memory allocation failed for ppe_drv_v6_conn_sync_many\n");
		return false;
	}

	netfn_flowmgr_ppe_ipv6_workqueue = create_singlethread_workqueue("netfn_flowmgr_ppe_ipv6_workqueue");
	if (!netfn_flowmgr_ppe_ipv6_workqueue) {
		vfree(netfn_flowmgr_ppe_ipv6_stats_sync_req_msg.conn_sync);
		return false;
	}

	INIT_DELAYED_WORK(&netfn_flowmgr_ppe_ipv6_work, netfn_flowmgr_ppe_ipv6_stats_sync_req_work);
	queue_delayed_work(netfn_flowmgr_ppe_ipv6_workqueue, &netfn_flowmgr_ppe_ipv6_work, NETFN_FLOWMGR_PPE_IPV6_STATS_SYNC_PERIOD);

	return true;
}

/*
 * netfn_flowmgr_ppe_ipv6_stats_deinit()
 *      PPE IPv6 stats de-initialization.
 */
void netfn_flowmgr_ppe_ipv6_stats_deinit(void)
{
	netfn_flowmgr_warn("PPE IPv6 stats deinit, unregister IPv6 callback with PPE\n");
	/*
	 * Unregister stats callback with PPE driver
	 */
	ppe_drv_v6_stats_callback_unregister();

	/*
	 * Clean up the stats sync queue/work
	 */
	netfn_flowmgr_ppe_ipv6_stats_sync_workqueue_deinit();
}

/*
 * netfn_flowmgr_ppe_ipv6_stats_init()
 *	PPE IPv6 stats initialization.
 */
bool netfn_flowmgr_ppe_ipv6_stats_init(void)
{
	netfn_flowmgr_warn("PPE IPv6 stats init, register IPv6 callback with PPE\n");
	/*
	 * Register a stats callback with PPE driver for any exception in PPE for a IPv6 flow.
	 */
	if (!ppe_drv_v6_stats_callback_register(netfn_flowmgr_ppe_ipv6_stats_callback, NULL)) {
		netfn_flowmgr_warn("Failed to register IPv6 stats callback\n");
		return false;
	}

	/*
	 * Initialize the workqueue for stats sync.
	 */
	if (!netfn_flowmgr_ppe_ipv6_stats_sync_workqueue_init()) {
		netfn_flowmgr_warn("Failed to create workqueue for IPv6 stats sync\n");
		return false;
	}
	return true;
}
