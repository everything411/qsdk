/*
 **************************************************************************
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

/*
 * netfn_flowmgr_sfe_stats.c
 *	Netfn flow manager sfe stats file
 */

#include <linux/types.h>
#include <linux/etherdevice.h>
#include <sfe_api.h>
#include <netfn_flowmgr.h>
#include <flowmgr/netfn_flowmgr_priv.h>
#include "netfn_flowmgr_sfe.h"
#include "netfn_flowmgr_sfe_stats.h"

#define NETFN_FLOWMGR_SFE_IPV4_STATS_SYNC_PERIOD msecs_to_jiffies(ipv4_stats_sync_period)
#define NETFN_FLOWMGR_SFE_IPV6_STATS_SYNC_PERIOD msecs_to_jiffies(ipv6_stats_sync_period)

struct delayed_work netfn_flowmgr_sfe_ipv4_work;
struct sfe_ipv4_msg *netfn_flowmgr_sfe_ipv4_sync_req_msg;
struct workqueue_struct *netfn_flowmgr_sfe_ipv4_workqueue;

struct delayed_work netfn_flowmgr_sfe_ipv6_work;
struct sfe_ipv6_msg *netfn_flowmgr_sfe_ipv6_sync_req_msg;
struct workqueue_struct *netfn_flowmgr_sfe_ipv6_workqueue;

/*
 * netfn_flowmgr_sfe_ipv4_process_one_conn_sync_msg()
 *	Process one connection sync message.
 */
void netfn_flowmgr_sfe_ipv4_process_one_conn_sync_msg(struct sfe_ipv4_conn_sync *sync)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_conn_stats conn_stats;
	netfn_flowmgr_ipv4_stats_callback_t sync_cb;
	void *sync_data;
	struct netfn_flowmgr_debug_stats *stats;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];

	spin_lock(&f->lock);
	sync_cb = f->stats_sync_v4_cb;
	sync_data = f->stats_sync_v4_data;
	spin_unlock(&f->lock);

	if (!sync_cb) {
		netfn_flowmgr_warn("No callback registered for stats sync\n");
		netfn_flowmgr_stats_inc(&stats->v4_stats_sync_callback_not_registered);
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


	switch(sync->reason) {
		case SFE_RULE_SYNC_REASON_EVICT:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_EVICT;
			break;
		case SFE_RULE_SYNC_REASON_FLUSH:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_FLUSH;
			break;
		case SFE_RULE_SYNC_REASON_DESTROY:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_DESTROY;
			break;
		default:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_STATS;
	}

	/*
	 * Call user registered callback
	 */
	sync_cb(sync_data, &conn_stats);
}

/*
 * netfn_flowmgr_sfe_ipv4_stats_sync_callback()
 *	callback function called from sfe to sync single flow stats
 */
void netfn_flowmgr_sfe_ipv4_stats_sync_callback(void *app_data, struct sfe_ipv4_msg *nim)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_debug_stats *stats;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];

	/*
	 * Only respond to sync messages
	 */
	if (nim->cm.type != SFE_RX_CONN_STATS_SYNC_MSG) {
		netfn_flowmgr_warn("Ignoring nim: %px - not sync: %d", nim, nim->cm.type);
		netfn_flowmgr_stats_inc(&stats->v4_stats_sync_invalid_msg_type);
		return;
	}
	netfn_flowmgr_sfe_ipv4_process_one_conn_sync_msg(&nim->msg.conn_stats);
}

/*
 * netfn_flowmgr_sfe_ipv4_stats_sync_req_work()
 *	Worker function called to periodically request sfe stats sync
 */
void netfn_flowmgr_sfe_ipv4_stats_sync_req_work(struct work_struct *work)
{
	/*
	 * Prepare sfe_ipv4_msg with CONN_STATS_SYNC_MANY request
	 */
	int retry = 3;
	sfe_tx_status_t sfe_tx_status;

	netfn_flowmgr_sfe_ipv4_sync_req_msg->msg.conn_stats_many.count = 0;
	while (retry) {
		sfe_tx_status = sfe_ipv4_tx(NULL, netfn_flowmgr_sfe_ipv4_sync_req_msg);
		if (sfe_tx_status == SFE_TX_SUCCESS) {
			return;
		}
	}

	netfn_flowmgr_warn("Rescheduling sync stats work\n");
	queue_delayed_work(netfn_flowmgr_sfe_ipv4_workqueue, &netfn_flowmgr_sfe_ipv4_work, NETFN_FLOWMGR_SFE_IPV4_STATS_SYNC_PERIOD);
	return;
}

/*
 * netfn_flowmgr_sfe_ipv4_sync_many_callback()
 *	callback function called from sfe to sync numerous flow's stats
 */
void netfn_flowmgr_sfe_ipv4_connection_sync_many_callback(void *app_data, struct sfe_ipv4_msg *nim)
{
	struct sfe_ipv4_conn_sync_many_msg *nicsm = &nim->msg.conn_stats_many;
	int i;

	if (nim->cm.response == SFE_CMN_RESPONSE_ACK) {
		for (i = 0; i < nicsm->count; i++) {
			netfn_flowmgr_sfe_ipv4_process_one_conn_sync_msg(&nicsm->conn_sync[i]);
		}
		netfn_flowmgr_sfe_ipv4_sync_req_msg->msg.conn_stats_many.index = nicsm->next;
	} else {
		netfn_flowmgr_warn("IPv4 conn stats request failed, restarting\n");
		netfn_flowmgr_sfe_ipv4_sync_req_msg->msg.conn_stats_many.index = 0;
	}
	queue_delayed_work(netfn_flowmgr_sfe_ipv4_workqueue, &netfn_flowmgr_sfe_ipv4_work, 0);
}

/*
 * netfn_flowmgr_sfe_v4_get_stats()
 *	Get SFE IPv4 stats for a single connection
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_v4_get_stats(struct netfn_flowmgr_flow_conn_stats *conn_stats)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct sfe_ipv4_single_conn_stats sfe_stats;
	netfn_tuple_type_t tuple_type;
	bool sfe_status;
	struct netfn_flowmgr_debug_stats *stats;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];

	tuple_type = conn_stats->tuple.tuple_type;

	/*
	 * Check tuple type
	 */
	if (!(tuple_type == NETFN_TUPLE_3TUPLE) && !(tuple_type == NETFN_TUPLE_4TUPLE) && !(tuple_type == NETFN_TUPLE_5TUPLE)) {
		netfn_flowmgr_warn("Unsupported tupple type in SFE acceleration mode, tuple_type = %d\n", tuple_type);
		netfn_flowmgr_stats_inc(&stats->v4_get_stats_tuple_type_unsupported);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_UNSUPPORTED_TUPLE_TYPE, 0);
	}

	sfe_stats.tuple.flow_ident = 0;
	sfe_stats.tuple.return_ident = 0;
	if (tuple_type == NETFN_TUPLE_3TUPLE) {
		sfe_stats.tuple.protocol = conn_stats->tuple.tuples.tuple_3.protocol;
		sfe_stats.tuple.flow_ip = conn_stats->tuple.tuples.tuple_3.src_ip.ip4.s_addr;
		sfe_stats.tuple.return_ip = conn_stats->tuple.tuples.tuple_3.dest_ip.ip4.s_addr;
	} else if (tuple_type == NETFN_TUPLE_4TUPLE) {
		sfe_stats.tuple.protocol = conn_stats->tuple.tuples.tuple_4.protocol;
		sfe_stats.tuple.flow_ip = conn_stats->tuple.tuples.tuple_4.src_ip.ip4.s_addr;
		sfe_stats.tuple.return_ip = conn_stats->tuple.tuples.tuple_4.dest_ip.ip4.s_addr;
		if (conn_stats->tuple.tuples.tuple_4.ident_type == NETFN_4TUPLE_VALID_SRC_PORT) {
			sfe_stats.tuple.flow_ident = conn_stats->tuple.tuples.tuple_4.l4_ident;
		} else {
			sfe_stats.tuple.return_ident = conn_stats->tuple.tuples.tuple_4.l4_ident;
		}
	} else if (tuple_type == NETFN_TUPLE_5TUPLE) {
                sfe_stats.tuple.protocol = conn_stats->tuple.tuples.tuple_5.protocol;
                sfe_stats.tuple.flow_ip = conn_stats->tuple.tuples.tuple_5.src_ip.ip4.s_addr;
                sfe_stats.tuple.return_ip = conn_stats->tuple.tuples.tuple_5.dest_ip.ip4.s_addr;
                sfe_stats.tuple.flow_ident = conn_stats->tuple.tuples.tuple_5.l4_src_ident;
                sfe_stats.tuple.return_ident = conn_stats->tuple.tuples.tuple_5.l4_dest_ident;
        }
	sfe_stats.org_dev = conn_stats->org_netdev;
	sfe_stats.reply_dev = conn_stats->reply_netdev;

	sfe_status = sfe_ipv4_get_single_conn_stats(&sfe_stats);
	if (!sfe_status) {
		netfn_flowmgr_warn("SFE v4 get sfe_stats failed for below tuples:\n"
					"protocol = %u\n"
					"flow_ip = %pI4\n"
					"return_ip = %pI4\n"
					"flow_ident = %u\n"
					"return_ident = %u\n",
					sfe_stats.tuple.protocol,
					&sfe_stats.tuple.flow_ip,
					&sfe_stats.tuple.return_ip,
					sfe_stats.tuple.flow_ident,
					sfe_stats.tuple.return_ident);
		netfn_flowmgr_stats_inc(&stats->v4_get_stats_fail);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_GET_SFE_STATS_FAILED, sfe_status);
	}

	/*
	 * Copy sfe stats to the conn stats structure to update the user stats.
	 */
	conn_stats->conn_stats.ip_version = ETH_P_IP;
	conn_stats->conn_stats.protocol = sfe_stats.tuple.protocol;

	/*
	 * original direction stats
	 */
	conn_stats->conn_stats.org_src_ip[0] = sfe_stats.tuple.flow_ip;
	conn_stats->conn_stats.org_dest_ip[0] = sfe_stats.tuple.return_ip;
	conn_stats->conn_stats.org_src_ident = sfe_stats.tuple.flow_ident;
	conn_stats->conn_stats.org_dest_ident = sfe_stats.tuple.return_ident;
	conn_stats->conn_stats.org_tx_pkt_count = sfe_stats.tx_packet_count;
	conn_stats->conn_stats.org_rx_pkt_count = sfe_stats.rx_packet_count;
	conn_stats->conn_stats.org_tx_byte_count = sfe_stats.tx_byte_count;
	conn_stats->conn_stats.org_rx_byte_count = sfe_stats.rx_byte_count;

	/*
	 * reply direction stats
	 */
	conn_stats->conn_stats.reply_src_ip[0] = sfe_stats.tuple.return_ip;
	conn_stats->conn_stats.reply_dest_ip[0] = sfe_stats.tuple.flow_ip;
	conn_stats->conn_stats.reply_src_ident = sfe_stats.tuple.return_ident;
	conn_stats->conn_stats.reply_dest_ident = sfe_stats.tuple.flow_ident;
	conn_stats->conn_stats.reply_tx_pkt_count = sfe_stats.rx_packet_count;
	conn_stats->conn_stats.reply_rx_pkt_count = sfe_stats.tx_packet_count;
	conn_stats->conn_stats.reply_tx_byte_count = sfe_stats.rx_byte_count;
	conn_stats->conn_stats.reply_rx_byte_count = sfe_stats.tx_byte_count;

	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_SUCCESS, sfe_status);
}

/*
 * netfn_flowmgr_sfe_ipv4_stats_init()
 *	SFE IPv4 stats initialization.
 */
bool netfn_flowmgr_sfe_ipv4_stats_init(void)
{
	/*
	 * Create sync request messages
	 */
	netfn_flowmgr_sfe_ipv4_sync_req_msg = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!netfn_flowmgr_sfe_ipv4_sync_req_msg) {
		return false;
	}

	sfe_ipv4_msg_init(netfn_flowmgr_sfe_ipv4_sync_req_msg, SFE_SPECIAL_INTERFACE_IPV4,
		SFE_TX_CONN_STATS_SYNC_MANY_MSG,
		sizeof(struct sfe_ipv4_conn_sync_many_msg),
		NULL,
		NULL);

	netfn_flowmgr_sfe_ipv4_sync_req_msg->msg.conn_stats_many.index = 0;
	netfn_flowmgr_sfe_ipv4_sync_req_msg->msg.conn_stats_many.size = PAGE_SIZE;

	/*
	 * Create Workqueues
	 */
	netfn_flowmgr_sfe_ipv4_workqueue = create_singlethread_workqueue("netfn_flowmgr_sfe_ipv4_workqueue");
	if(!netfn_flowmgr_sfe_ipv4_workqueue) {
		destroy_workqueue(netfn_flowmgr_sfe_ipv4_workqueue);
		return false;
	}
	INIT_DELAYED_WORK(&netfn_flowmgr_sfe_ipv4_work, netfn_flowmgr_sfe_ipv4_stats_sync_req_work);
	queue_delayed_work(netfn_flowmgr_sfe_ipv4_workqueue, &netfn_flowmgr_sfe_ipv4_work, NETFN_FLOWMGR_SFE_IPV4_STATS_SYNC_PERIOD);

	/*
	 * Create Register Stats Callbacks
	 */
	sfe_ipv4_notify_register(netfn_flowmgr_sfe_ipv4_stats_sync_callback, netfn_flowmgr_sfe_ipv4_connection_sync_many_callback, NULL);

	return true;
}

/*
 * netfn_flowmgr_sfe_ipv4_stats_deinit()
 *	SFE IPv4 stats exit.
 */
void netfn_flowmgr_sfe_ipv4_stats_deinit(void)
{
	sfe_ipv4_notify_unregister();

	/*
	 * Cancel the conn sync req work and destroy workqueues
	 */
	cancel_delayed_work_sync(&netfn_flowmgr_sfe_ipv4_work);
	destroy_workqueue(netfn_flowmgr_sfe_ipv4_workqueue);
	kfree(netfn_flowmgr_sfe_ipv4_stats_sync_req_work);
}

/*
 * netfn_flowmgr_sfe_ipv6_process_one_conn_sync_msg()
 * 	fill and return filled stats struct
 */
void netfn_flowmgr_sfe_ipv6_process_one_conn_sync_msg(struct sfe_ipv6_conn_sync *sync)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_conn_stats conn_stats;
	netfn_flowmgr_ipv6_stats_callback_t sync_cb;
	void *sync_data;
	struct netfn_flowmgr_debug_stats *stats;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];

	spin_lock(&f->lock);
	sync_cb = f->stats_sync_v6_cb;
	sync_data = f->stats_sync_v6_data;
	spin_unlock(&f->lock);

	if (!sync_cb) {
		netfn_flowmgr_warn("No callback registered for stats sync\n");
		netfn_flowmgr_stats_inc(&stats->v6_stats_sync_callback_not_registered);
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
	memcpy(conn_stats.org_src_ip, sync->flow_ip, sizeof(sync->flow_ip));
	memcpy(conn_stats.org_dest_ip, sync->return_ip, sizeof(sync->return_ip));
	conn_stats.org_src_ident = sync->flow_ident;
	conn_stats.org_dest_ident = sync->return_ident;
	conn_stats.org_tx_pkt_count = sync->flow_tx_packet_count;
	conn_stats.org_rx_pkt_count = sync->flow_rx_packet_count;
	conn_stats.org_tx_byte_count = sync->flow_tx_byte_count;
	conn_stats.org_rx_byte_count = sync->flow_rx_byte_count;

	/*
	 * reply direction stats
	 */
	memcpy(conn_stats.reply_src_ip, sync->return_ip, sizeof(sync->return_ip));
	memcpy(conn_stats.reply_dest_ip, sync->flow_ip, sizeof(sync->flow_ip));
	conn_stats.reply_src_ident = sync->return_ident;
	conn_stats.reply_dest_ident = sync->flow_ident;
	conn_stats.reply_tx_pkt_count = sync->return_tx_packet_count;
	conn_stats.reply_rx_pkt_count = sync->return_rx_packet_count;
	conn_stats.reply_tx_byte_count = sync->return_tx_byte_count;
	conn_stats.reply_rx_byte_count = sync->return_rx_byte_count;
	switch(sync->reason) {
		case SFE_RULE_SYNC_REASON_EVICT:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_EVICT;
			break;
		case SFE_RULE_SYNC_REASON_FLUSH:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_FLUSH;
			break;
		case SFE_RULE_SYNC_REASON_DESTROY:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_DESTROY;
			break;
		default:
			conn_stats.reason = NETFN_FLOWMGR_STATS_SYNC_REASON_STATS;
	}
	/*
	 * Call user registered callback
	 */
	sync_cb(sync_data, &conn_stats);
}

/*
 * netfn_flowmgr_sfe_ipv6_stats_sync_callback()
 *	callback function called from sfe to sync single flow stats
 */
void netfn_flowmgr_sfe_ipv6_stats_sync_callback(void *app_data, struct sfe_ipv6_msg *nim)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct netfn_flowmgr_debug_stats *stats;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];

	/*
	 * Only respond to sync messages
	 */
	if (nim->cm.type != SFE_RX_CONN_STATS_SYNC_MSG) {
		netfn_flowmgr_warn("Ignoring nim: %px - not sync: %d", nim, nim->cm.type);
		netfn_flowmgr_stats_inc(&stats->v6_stats_sync_invalid_msg_type);
		return;
	}
	netfn_flowmgr_sfe_ipv6_process_one_conn_sync_msg(&nim->msg.conn_stats);
}

/*
 * netfn_flowmgr_sfe_ipv6_sync_many_callback()
 *	callback function called from sfe to sync numerous flow's stats
 */
void netfn_flowmgr_sfe_ipv6_connection_sync_many_callback(void *app_data, struct sfe_ipv6_msg *nim)
{
	struct sfe_ipv6_conn_sync_many_msg *nicsm = &nim->msg.conn_stats_many;
	int i;

	if (nim->cm.response == SFE_CMN_RESPONSE_ACK) {
		for (i = 0; i < nicsm->count; i++) {
			netfn_flowmgr_sfe_ipv6_process_one_conn_sync_msg(&nicsm->conn_sync[i]);
		}
		netfn_flowmgr_sfe_ipv6_sync_req_msg->msg.conn_stats_many.index = nicsm->next;
	} else {
		netfn_flowmgr_warn("IPv6 conn stats request failed, restarting\n");
		netfn_flowmgr_sfe_ipv6_sync_req_msg->msg.conn_stats_many.index = 0;
	}
	queue_delayed_work(netfn_flowmgr_sfe_ipv6_workqueue, &netfn_flowmgr_sfe_ipv6_work, 0);
}

/*
 * netfn_flowmgr_sfe_ipv6_stats_sync_req_work()
 *	Worker function called to periodically request sfe stats sync
 */
void netfn_flowmgr_sfe_ipv6_stats_sync_req_work(struct work_struct *work)
{
	/*
	 * Prepare sfe_ipv6_msg with CONN_STATS_SYNC_MANY request
	 */
	int retry = 3;
	sfe_tx_status_t sfe_tx_status;

	netfn_flowmgr_sfe_ipv6_sync_req_msg->msg.conn_stats_many.count = 0;
	while (retry) {
		sfe_tx_status = sfe_ipv6_tx(NULL, netfn_flowmgr_sfe_ipv6_sync_req_msg);
		if (sfe_tx_status == SFE_TX_SUCCESS) {
			return;
		}
	}

	netfn_flowmgr_warn("Rescheduling sync stats work\n");
	queue_delayed_work(netfn_flowmgr_sfe_ipv6_workqueue, &netfn_flowmgr_sfe_ipv6_work, NETFN_FLOWMGR_SFE_IPV6_STATS_SYNC_PERIOD);
	return;
}

/*
 * netfn_flowmgr_sfe_v6_get_stats()
 *	Get SFE IPv6 stats for a single connection
 */
netfn_flowmgr_ret_t netfn_flowmgr_sfe_v6_get_stats(struct netfn_flowmgr_flow_conn_stats *conn_stats)
{
	struct netfn_flowmgr *f = &netfn_flowmgr_gbl;
	struct sfe_ipv6_single_conn_stats sfe_stats;
	netfn_tuple_type_t tuple_type;
	bool sfe_status;
	struct netfn_flowmgr_debug_stats *stats;

	stats = &f->stats[NETFN_FLOWMGR_AE_TYPE_SFE];

	tuple_type = conn_stats->tuple.tuple_type;

	/*
	 * Check tuple type
	 */
	if (!(tuple_type == NETFN_TUPLE_3TUPLE) && !(tuple_type == NETFN_TUPLE_4TUPLE) && !(tuple_type == NETFN_TUPLE_5TUPLE)) {
		netfn_flowmgr_warn("Unsupported tupple type in SFE acceleration mode, tuple_type = %d\n", tuple_type);
		netfn_flowmgr_stats_inc(&stats->v6_get_stats_tuple_type_unsupported);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_UNSUPPORTED_TUPLE_TYPE, 0);
	}

	sfe_stats.tuple.flow_ident = 0;
	sfe_stats.tuple.return_ident = 0;
	if (tuple_type == NETFN_TUPLE_3TUPLE) {
		sfe_stats.tuple.protocol = conn_stats->tuple.tuples.tuple_3.protocol;
		memcpy(sfe_stats.tuple.flow_ip, conn_stats->tuple.tuples.tuple_3.src_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
		memcpy(sfe_stats.tuple.return_ip, conn_stats->tuple.tuples.tuple_3.dest_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
		sfe_stats.tuple.flow_ident = 0;
		sfe_stats.tuple.return_ident = 0;
	} else if (tuple_type == NETFN_TUPLE_4TUPLE) {
		sfe_stats.tuple.protocol = conn_stats->tuple.tuples.tuple_4.protocol;
		memcpy(sfe_stats.tuple.flow_ip, conn_stats->tuple.tuples.tuple_4.src_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
		memcpy(sfe_stats.tuple.return_ip, conn_stats->tuple.tuples.tuple_4.dest_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
		if (conn_stats->tuple.tuples.tuple_4.ident_type == NETFN_4TUPLE_VALID_SRC_PORT) {
			sfe_stats.tuple.flow_ident = conn_stats->tuple.tuples.tuple_4.l4_ident;
		} else {
			sfe_stats.tuple.return_ident = conn_stats->tuple.tuples.tuple_4.l4_ident;
		}
	} else if (tuple_type == NETFN_TUPLE_5TUPLE) {
                sfe_stats.tuple.protocol = conn_stats->tuple.tuples.tuple_5.protocol;
		memcpy(sfe_stats.tuple.flow_ip, conn_stats->tuple.tuples.tuple_5.src_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
		memcpy(sfe_stats.tuple.return_ip, conn_stats->tuple.tuples.tuple_5.dest_ip.ip6.s6_addr32, sizeof(uint32_t) * 4);
                sfe_stats.tuple.flow_ident = conn_stats->tuple.tuples.tuple_5.l4_src_ident;
                sfe_stats.tuple.return_ident = conn_stats->tuple.tuples.tuple_5.l4_dest_ident;
        }
	sfe_stats.org_dev = conn_stats->org_netdev;
	sfe_stats.reply_dev = conn_stats->reply_netdev;

	sfe_status = sfe_ipv6_get_single_conn_stats(&sfe_stats);
	if (!sfe_status) {
		netfn_flowmgr_warn("SFE v6 get sfe_stats failed for below tuples:\n"
					"protocol = %u\n"
					"flow_ip = %pI6\n"
					"return_ip = %pI6\n"
					"flow_ident = %u\n"
					"return_ident = %u\n",
					sfe_stats.tuple.protocol,
					&sfe_stats.tuple.flow_ip,
					&sfe_stats.tuple.return_ip,
					sfe_stats.tuple.flow_ident,
					sfe_stats.tuple.return_ident);
		netfn_flowmgr_stats_inc(&stats->v6_get_stats_fail);
		return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_GET_SFE_STATS_FAILED, sfe_status);
	}

	/*
	 * Copy sfe stats to the conn stats structure to update the user stats.
	 */
	conn_stats->conn_stats.ip_version = ETH_P_IP;
	conn_stats->conn_stats.protocol = sfe_stats.tuple.protocol;

	/*
	 * original direction stats
	 */
	memcpy(conn_stats->conn_stats.org_src_ip, sfe_stats.tuple.flow_ip, sizeof(uint32_t) * 4);
	memcpy(conn_stats->conn_stats.org_dest_ip, sfe_stats.tuple.return_ip, sizeof(uint32_t) * 4);
	conn_stats->conn_stats.org_src_ident = sfe_stats.tuple.flow_ident;
	conn_stats->conn_stats.org_dest_ident = sfe_stats.tuple.return_ident;
	conn_stats->conn_stats.org_tx_pkt_count = sfe_stats.tx_packet_count;
	conn_stats->conn_stats.org_rx_pkt_count = sfe_stats.rx_packet_count;
	conn_stats->conn_stats.org_tx_byte_count = sfe_stats.tx_byte_count;
	conn_stats->conn_stats.org_rx_byte_count = sfe_stats.rx_byte_count;

	/*
	 * reply direction stats
	 */
	memcpy(conn_stats->conn_stats.reply_src_ip, sfe_stats.tuple.return_ip, sizeof(uint32_t) * 4);
	memcpy(conn_stats->conn_stats.reply_dest_ip, sfe_stats.tuple.flow_ip, sizeof(uint32_t) * 4);
	conn_stats->conn_stats.reply_src_ident = sfe_stats.tuple.return_ident;
	conn_stats->conn_stats.reply_dest_ident = sfe_stats.tuple.flow_ident;
	conn_stats->conn_stats.reply_tx_pkt_count = sfe_stats.rx_packet_count;
	conn_stats->conn_stats.reply_rx_pkt_count = sfe_stats.tx_packet_count;
	conn_stats->conn_stats.reply_tx_byte_count = sfe_stats.rx_byte_count;
	conn_stats->conn_stats.reply_rx_byte_count = sfe_stats.tx_byte_count;

	return NETFN_FLOWMGR_SET_NETFN_STATUS_CODE(NETFN_FLOWMGR_RET_SUCCESS, sfe_status);
}

/*
 * netfn_flowmgr_sfe_ipv6_stats_init()
 *	SFE IPv6 stats initialization.
 */
bool netfn_flowmgr_sfe_ipv6_stats_init(void)
{
	/*
	 * Create sync request messages
	 */
	netfn_flowmgr_sfe_ipv6_sync_req_msg = kzalloc(PAGE_SIZE, GFP_KERNEL);
	if (!netfn_flowmgr_sfe_ipv6_sync_req_msg) {
		return false;
	}

	sfe_ipv6_msg_init(netfn_flowmgr_sfe_ipv6_sync_req_msg, SFE_SPECIAL_INTERFACE_IPV4,
		SFE_TX_CONN_STATS_SYNC_MANY_MSG,
		sizeof(struct sfe_ipv6_conn_sync_many_msg),
		NULL,
		NULL);

	netfn_flowmgr_sfe_ipv6_sync_req_msg->msg.conn_stats_many.index = 0;
	netfn_flowmgr_sfe_ipv6_sync_req_msg->msg.conn_stats_many.size = PAGE_SIZE;

	/*
	 * Create Workqueues
	 */
	netfn_flowmgr_sfe_ipv6_workqueue = create_singlethread_workqueue("netfn_flowmgr_sfe_ipv6_workqueue");
	if(!netfn_flowmgr_sfe_ipv6_workqueue) {
		destroy_workqueue(netfn_flowmgr_sfe_ipv6_workqueue);
		return false;
	}
	INIT_DELAYED_WORK(&netfn_flowmgr_sfe_ipv6_work, netfn_flowmgr_sfe_ipv6_stats_sync_req_work);
	queue_delayed_work(netfn_flowmgr_sfe_ipv6_workqueue, &netfn_flowmgr_sfe_ipv6_work, NETFN_FLOWMGR_SFE_IPV6_STATS_SYNC_PERIOD);

	/*
	 * Create Register Stats Callbacks
	 */
	sfe_ipv6_notify_register(netfn_flowmgr_sfe_ipv6_stats_sync_callback, netfn_flowmgr_sfe_ipv6_connection_sync_many_callback, NULL);

	return true;
}

/*
 * netfn_flowmgr_sfe_ipv6_stats_deinit()
 *	SFE IPv6 stats exit.
 */
void netfn_flowmgr_sfe_ipv6_stats_deinit(void)
{
	sfe_ipv6_notify_unregister();

	/*
	 * Cancel the conn sync req work and destroy workqueues
	 */
	cancel_delayed_work_sync(&netfn_flowmgr_sfe_ipv6_work);
	destroy_workqueue(netfn_flowmgr_sfe_ipv6_workqueue);
	kfree(netfn_flowmgr_sfe_ipv6_stats_sync_req_work);
}
