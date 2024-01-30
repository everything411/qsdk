/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __PPE_DRV_STATS_H
#define __PPE_DRV_STATS_H

#define PPE_STATS_NODE_NAME		"PPE"

/*
 * Max service code number.
 * TODO: update it with enum definition
 */
#define PPE_DRV_SC_CNT_MAX 256

/*
 * ppe_drv_conn_type
 *	connection type
 */
enum ppe_drv_conn_type {
	PPE_DRV_CONN_TYPE_FLOW,		/* inner flow or TCP/UDP flow*/
	PPE_DRV_CONN_TYPE_TUNNEL,	/* Tunnel flow */
	PPE_DRV_CONN_TYPE_MAX,
};

/*
 * ppe_drv_gen_stats
 *	Message structure for ppe general stats
 */
struct ppe_drv_gen_stats {
	atomic64_t v4_l3_flows;			/* No of v4 routed flows */
	atomic64_t v4_l2_flows;			/* No of v4 bridge flows */
	atomic64_t v4_vp_wifi_flows;		/* No of v4 VP Wi-Fi flows */
	atomic64_t v4_ds_flows;			/* No of v4 Direct Switch flows */
	atomic64_t v4_host_add_fail;			/* v4 host table add failed */

	atomic64_t v4_flush_req;			/* No of v4 flush requests */
	atomic64_t v4_flush_fail;			/* No of v4 flush requests fail */
	atomic64_t v4_flush_conn_not_found;		/* No of v4 connection not found during flush. */
	atomic64_t v4_flush_skip_conn_rfs;		/* No of rfs v4 connection not found during flush. */

	atomic64_t v6_l3_flows;			/* No of v6 routed flows */
	atomic64_t v6_l2_flows;			/* No of v6 bridge flows */
	atomic64_t v6_vp_wifi_flows;		/* No of v6 VP Wi-Fi flows */
	atomic64_t v6_ds_flows;			/* No of v6 Direct Switch flows */
	atomic64_t v6_host_add_fail;		/* v6 host table add failed */
	atomic64_t v6_create_fail_bridge_nat;		/* No of v6 create failure due to NAT with bridge flow */

	atomic64_t v6_flush_req;			/* No of v6 flush requests */
	atomic64_t v6_flush_fail;			/* No of v6 flush requests fail */
	atomic64_t v6_flush_conn_not_found;		/* No of v6 connection not found during flush. */
	atomic64_t v6_flush_skip_conn_rfs;		/* No of v6 connection not found during flush. */

	atomic64_t fail_vp_full;		/* Create req fail due to VP table full */
	atomic64_t fail_pp_full;		/* Create req fail due to physical port table full */
	atomic64_t fail_nh_full;		/* Create req fail due to nexthop table full */
	atomic64_t fail_flow_full;		/* Create req fail due to flow table full */
	atomic64_t fail_host_full;		/* Create req fail due to host table full */
	atomic64_t fail_pubip_full;		/* Create req fail due to pub-ip table full */
	atomic64_t fail_dev_port_map;		/* Create req fail due to PPE port not mapped to net-device */
	atomic64_t fail_l3_if_full;		/* Create req fail due to L3_IF table full */
	atomic64_t fail_vsi_full;		/* Create req fail due to VSI table full */
	atomic64_t fail_vsi_reuse;		/* Create req fail due to VSI reuse */
	atomic64_t fail_pppoe_full;		/* Create req fail due to PPPoE table full */
	atomic64_t fail_rw_fifo_full;		/* Create req fail due to rw fifo full */
	atomic64_t fail_flow_command;		/* Create req fail due to PPE flow command failure */
	atomic64_t fail_unknown_proto;		/* Create req fail due to unknown protocol */
	atomic64_t fail_query_unknown_proto;	/* Query fail due to unknown protocol */
	atomic64_t fail_ppe_unresponsive;	/* Create req fail due to PPE not responding */
	atomic64_t ce_opaque_invalid;		/* Request fail due to invalid opaque in CE */
	atomic64_t fail_fqg_full;		/* Create req fail due to flow qos group full */
	atomic64_t fail_ingress_vlan_add;	/* Ingress vlan translation addition failed */
	atomic64_t fail_egress_vlan_add;	/* Egress vlan translation addition failed */
	atomic64_t fail_my_mac_full;		/* Create req fail due to MAC table full */
	atomic64_t fail_ingress_untag_vlan_add;	/* Ingress untag vlan translation addition failed */
	atomic64_t fail_ingress_untag_vlan_del;	/* Ingress untag vlan translation deletion failed */
};

/*
 * ppe_drv_comm_stats
 *	common stats for flow and tunnel.
 */
struct ppe_drv_comm_stats {
	atomic64_t v4_create_req;		/* No of v4 create requests */
	atomic64_t v4_create_fail;		/* No of v4 create failure */
	atomic64_t v4_destroy_req;		/* No of v4 delete requests */
	atomic64_t v4_destroy_fail;		/* No of v4 delete failure */
	atomic64_t v4_create_fail_bridge_nat;		/* No of v4 create failure due to NAT with bridge flow */
	atomic64_t v4_create_fail_snat_dnat;		/* No of v4 create failure due to both SNAT and DNAT is requested */
	atomic64_t v4_destroy_conn_not_found;	/* No of v4 delete failure due to connection not found */
	atomic64_t v4_create_fail_mem;			/* No of v4 create failure due to OOM */
	atomic64_t v4_create_fail_conn;			/* No of v4 create failure due to invalid parameters */
	atomic64_t v4_create_fail_collision;		/* No of v4 create failure due to connection already exist */
	atomic64_t v4_unknown_interface;		/* No of v4 create failure due to invalid IF */
	atomic64_t v4_create_fail_invalid_rx_if;	/* No of v4 create failure due to invalid Rx IF */
	atomic64_t v4_create_fail_invalid_tx_if;	/* No of v4 create failure due to invalid Tx IF */
	atomic64_t v4_create_fail_invalid_rx_port;	/* No of v4 create failure due to invalid Rx Port */
	atomic64_t v4_create_fail_invalid_tx_port;	/* No of v4 create failure due to invalid Tx Port */
	atomic64_t v4_create_fail_if_hierarchy;		/* No of v4 create failure due to interface hierarchy walk fail */

	atomic64_t v4_create_fail_vlan_filter;		/* No of v4 create failure due to interface not in bridge */
	atomic64_t v4_create_fail_bridge_noexist;	/* No of v4 create failure due to bridge interface not created */

	atomic64_t v4_create_rfs_req;		/* No of v4 RFS create requests */
	atomic64_t v4_create_rfs_fail;		/* No of v4 RFS create failure */
	atomic64_t v4_destroy_rfs_req;		/* No of v4 RFS delete requests */
	atomic64_t v4_destroy_rfs_fail;		/* No of v4 RFS delete failure */
	atomic64_t v4_destroy_rfs_conn_not_found;	/* No of v4 RFS delete failure due to connection not found */
	atomic64_t v4_create_rfs_fail_mem;			/* No of v4 RFS create failure due to OOM */
	atomic64_t v4_create_rfs_fail_conn;			/* No of v4 RFS create failure due to invalid parameters */
	atomic64_t v4_create_rfs_fail_collision;		/* No of v4 RFS create failure due to connection already exist */
	atomic64_t v4_unknown_rfs_interface;		/* No of v4 RFS create failure due to invalid IF */
	atomic64_t v4_create_rfs_fail_invalid_rx_if;	/* No of v4 RFS create failure due to invalid Rx IF */
	atomic64_t v4_create_rfs_fail_invalid_tx_if;	/* No of v4 RFS create failure due to invalid Tx IF */
	atomic64_t v4_create_rfs_fail_invalid_rx_port;	/* No of v4 RFS create failure due to invalid Rx Port */
	atomic64_t v4_create_rfs_fail_invalid_tx_port;	/* No of v4 RFS create failure due to invalid Tx Port */
	atomic64_t v4_create_rfs_noedit_flow;		/* No of v4 request for non edit rfs mode */

	atomic64_t v4_create_fse_success;		/* No of v4 FSE rule create failure */
	atomic64_t v4_create_fse_fail;		/* No of v4 FSE rule create failure */
	atomic64_t v4_destroy_fse_success;		/* No of v4 FSE rule destroy failure */
	atomic64_t v4_destroy_fse_fail;		/* No of v4 FSE rule destroy failure */
	atomic64_t v4_create_offload_disabled;		/* No of v4 request where offload is disabled */

	atomic64_t v6_create_req;		/* No of v6 create requests */
	atomic64_t v6_create_fail;		/* No of v6 create failure */
	atomic64_t v6_destroy_req;		/* No of v6 delete requests */
	atomic64_t v6_destroy_fail;		/* No of v6 delete failure */
	atomic64_t v6_destroy_conn_not_found;	/* No of v6 delete failure due to connection not found */
	atomic64_t v6_create_fail_mem;			/* No of v6 create failure due to OOM */
	atomic64_t v6_create_fail_conn;			/* No of v6 create failure due to invalid parameters */
	atomic64_t v6_create_fail_collision;		/* No of v6 create failure due to connection already exist */
	atomic64_t v6_create_fail_invalid_rx_if;	/* No of v6 create failure due to invalid Rx IF */
	atomic64_t v6_create_fail_invalid_tx_if;	/* No of v6 create failure due to invalid Tx IF */
	atomic64_t v6_create_fail_invalid_rx_port;	/* No of v6 create failure due to invalid Rx Port */
	atomic64_t v6_create_fail_invalid_tx_port;	/* No of v6 create failure due to invalid Tx Port */
	atomic64_t v6_create_fail_if_hierarchy;		/* No of v6 create failure due to interface hierarchy walk fail */

	atomic64_t v6_create_fail_vlan_filter;		/* No of v6 create failure due to interface not in bridge */
	atomic64_t v6_create_fail_bridge_noexist;	/* No of v6 create failure due to bridge interface not created */

	atomic64_t v6_create_rfs_req;		/* No of v6 RFS create requests */
	atomic64_t v6_create_rfs_fail;		/* No of v6 RFS create failure */
	atomic64_t v6_destroy_rfs_req;		/* No of v6 RFS delete requests */
	atomic64_t v6_destroy_rfs_fail;		/* No of v6 RFS delete failure */
	atomic64_t v6_destroy_rfs_conn_not_found;	/* No of v6 RFS delete failure due to connection not found */
	atomic64_t v6_create_rfs_fail_mem;			/* No of v6 RFS create failure due to OOM */
	atomic64_t v6_create_rfs_fail_conn;			/* No of v6 RFS create failure due to invalid parameters */
	atomic64_t v6_create_rfs_fail_collision;		/* No of v6 RFS create failure due to connection already exist */
	atomic64_t v6_unknown_rfs_interface;		/* No of v6 RFS create failure due to invalid IF */
	atomic64_t v6_create_rfs_fail_invalid_rx_if;	/* No of v6 RFS create failure due to invalid Rx IF */
	atomic64_t v6_create_rfs_fail_invalid_tx_if;	/* No of v6 RFS create failure due to invalid Tx IF */
	atomic64_t v6_create_rfs_fail_invalid_rx_port;	/* No of v6 RFS create failure due to invalid Rx Port */
	atomic64_t v6_create_rfs_fail_invalid_tx_port;	/* No of v6 RFS create failure due to invalid Tx Port */
	atomic64_t v6_create_rfs_noedit_flow;		/* No of v6 request for non edit rfs mode */

	atomic64_t v6_create_fse_success;		/* No of v6 FSE rule create failure */
	atomic64_t v6_create_fse_fail;		/* No of v6 FSE rule create failure */
	atomic64_t v6_destroy_fse_success;		/* No of v6 FSE rule destroy failure */
	atomic64_t v6_destroy_fse_fail;		/* No of v6 FSE rule destroy failure */
	atomic64_t v6_create_offload_disabled;		/* No of v6 request where offload is disabled */
};

/*
 * ppe_drv_stats_sc
 *	Message structure for per service code stats.
 */
struct ppe_drv_stats_sc {
	atomic64_t sc_cb_unregister;		/* Per service-code counter for callback not registered */
	atomic64_t sc_cb_packet_consumed;	/* Per service-code coutner for successful packet consumption in callback */
	atomic64_t sc_cb_packet_processed;	/* Per service-code counter for packet processed in callback */
	atomic64_t sc_vp_cb_unregister;		/* Per service-code counter for vp callback not registered */
	atomic64_t sc_vp_cb_packet_consumed;	/* Per service-code coutner for vp successful packet consumption in callback */
	atomic64_t sc_vp_cb_packet_processed;	/* Per service-code counter for vp packet processed in callback */
};

/*
 * ppe_drv_stats_sawf_sc
 *	Message structure for per service class stats.
 */
struct ppe_drv_stats_sawf_sc {
	atomic64_t rx_packets;		/* Per service-class counter for packets recieved on ethernet port */
	atomic64_t rx_bytes;		/* Per service-class counter for bytes recieved on ethernet port */
	atomic64_t flow_count;		/* Per service-class counter for total number of flows present */
};

/*
 * ppe_drv_stats
 *	Message structure for ppe stats
 */
struct ppe_drv_stats {
	struct ppe_drv_gen_stats gen_stats;				/* General connection stats */
	struct ppe_drv_comm_stats comm_stats[PPE_DRV_CONN_TYPE_MAX];	/* common stats for flow and tunnel */
	struct ppe_drv_stats_sc	sc_stats[PPE_DRV_SC_CNT_MAX];		/* Per service-code stats */
	struct ppe_drv_stats_sawf_sc sawf_sc_stats[PPE_DRV_SAWF_SC_MAX];	/* Per service-class stats */
};

/*
 * ppe_drv_stats_dec()
 *	Decrement stats counter.
 */
static inline void ppe_drv_stats_dec(atomic64_t *stat)
{
	atomic64_dec(stat);
}

/*
 * ppe_drv_stats_inc()
 *	Increment stats counter.
 */
static inline void ppe_drv_stats_inc(atomic64_t *stat)
{
	atomic64_inc(stat);
}

int ppe_drv_stats_debugfs_init(void);
void ppe_drv_stats_debugfs_exit(void);

#endif
