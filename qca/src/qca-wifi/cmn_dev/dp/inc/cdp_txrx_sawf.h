/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _CDP_TXRX_SAWF_H_
#define _CDP_TXRX_SAWF_H_

#include <cdp_txrx_cmn_struct.h>
#include <cdp_txrx_cmn.h>

enum cdp_sawf_peer_msduq_event {
	SAWF_PEER_MSDUQ_ADD_EVENT,
	SAWF_PEER_MSDUQ_DELETE_EVENT,
	SAWF_PEER_MSDUQ_UPDATE_EVENT,
};

struct cdp_sawf_peer_msduq_event_intf {
	enum cdp_sawf_peer_msduq_event event_type;
	uint16_t hw_link_id;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE];
	uint8_t vdev_id;
	uint8_t queue_id;
	uint8_t svc_id;
	uint8_t tid;
	uint8_t ac;
	uint8_t priority;
	uint8_t type;
	uint32_t mark_metadata;
	uint32_t service_interval;
	uint32_t min_throughput;
	uint32_t burst_size;
	uint32_t delay_bound;
};

static inline QDF_STATUS
cdp_sawf_peer_svcid_map(ol_txrx_soc_handle soc,
			uint8_t *mac, uint8_t svc_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_def_queues_map_req) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_def_queues_map_req(soc, mac, svc_id);
}

static inline QDF_STATUS
cdp_sawf_peer_unmap(ol_txrx_soc_handle soc,
		    uint8_t *mac, uint8_t svc_id)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_def_queues_unmap_req) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_def_queues_unmap_req(soc, mac, svc_id);
}

static inline QDF_STATUS
cdp_sawf_peer_get_map_conf(ol_txrx_soc_handle soc,
			   uint8_t *mac)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_def_queues_get_map_report) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_def_queues_get_map_report(soc, mac);
}

static inline QDF_STATUS
cdp_sawf_peer_get_msduq_info(ol_txrx_soc_handle soc, char *mac, uint8_t svc_id,
			     uint8_t debug_level)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_get_peer_msduq_info) {
		return QDF_STATUS_E_FAILURE;
	}

	return soc->ops->sawf_ops->sawf_get_peer_msduq_info(soc, mac, svc_id,
							    debug_level);
}

#ifdef CONFIG_SAWF
/**
 * cdp_get_peer_sawf_delay_stats() - Call to get SAWF delay stats
 * @soc: soc handle
 * @svc_id: service class ID
 * @mac: peer mac address
 * @data: opaque pointer
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_get_peer_sawf_delay_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			      uint8_t *mac, void *data)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_get_peer_sawf_delay_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_get_peer_sawf_delay_stats(soc, svc_id,
								  mac, data);
}

/**
 * cdp_get_peer_sawf_tx_stats() - Call to get SAWF Tx stats
 * @soc: soc handle
 * @svc_id: service class ID
 * @mac: peer mac address
 * @data: opaque pointer
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_get_peer_sawf_tx_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			   uint8_t *mac, void *data)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_get_peer_sawf_tx_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_get_peer_sawf_tx_stats(soc, svc_id,
							       mac, data);
}

/**
 * cdp_get_peer_sawf_msduq_svc_params() - Call to get Peer MSDUQ SVC Info
 * @soc: soc handle
 * @mac: peer mac address
 * @data: opaque pointer
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_get_peer_sawf_msduq_svc_params(ol_txrx_soc_handle soc, uint8_t *mac,
				   void *data)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_get_peer_sawf_msduq_svc_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_get_peer_sawf_msduq_svc_params(soc,
								       mac,
								       data);
}

/**
 * cdp_sawf_mpdu_stats_req() - Call to subscribe to MPDU stats TLV
 * @soc: soc handle
 * @enable: 1: enable 0: disable
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_sawf_mpdu_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_mpdu_stats_req)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->sawf_mpdu_stats_req(soc, enable);
}

/**
 * cdp_sawf_mpdu_details_stats_req - Call to subscribe to MPDU details stats TLV
 * @soc: soc handle
 * @enable: 1: enable 0: disable
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_sawf_mpdu_details_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_mpdu_details_stats_req)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->sawf_mpdu_details_stats_req(soc, enable);
}

/**
 * cdp_sawf_set_mov_avg_params - Set moving average pararms
 * @soc: SOC handle
 * @num_pkt: No of packets per window to calucalte moving average
 * @num_win: No of windows to calucalte moving average
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_set_mov_avg_params(ol_txrx_soc_handle soc,
			    uint32_t num_pkt,
			    uint32_t num_win)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_sawf_set_mov_avg_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_sawf_set_mov_avg_params(num_pkt,
								num_win);
}

/**
 * cdp_sawf_set_sla_params - Set SLA pararms
 * @soc: SOC handle
 * @num_pkt: No of packets to detect SLA breach
 * @time_secs: Time ins secs to detect breach
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_set_sla_params(ol_txrx_soc_handle soc,
			uint32_t num_pkt,
			uint32_t time_secs)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_sawf_set_sla_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_sawf_set_sla_params(num_pkt,
							    time_secs);
}

/**
 * cdp_sawf_init_telemtery_params() - Initialize telemetry pararms
 * @soc: SOC handle
 *
 * Return: none
 */
static inline QDF_STATUS
cdp_sawf_init_telemtery_params(ol_txrx_soc_handle soc)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_sawf_init_telemtery_params)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_sawf_init_telemtery_params();
}

static inline QDF_STATUS
cdp_get_throughput_stats(ol_txrx_soc_handle soc, void *arg,
			 uint64_t *in_bytes, uint64_t *in_cnt,
			 uint64_t *tx_bytes, uint64_t *tx_cnt,
			 uint8_t tid, uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_throughput_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_throughput_stats(
			arg, in_bytes, in_cnt, tx_bytes,
			tx_cnt, tid, msduq);
}

static inline QDF_STATUS
cdp_get_msduq_tx_stats(ol_txrx_soc_handle soc, void *arg,
		       void *msduq_tx_stats,
		       uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_msduq_tx_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_msduq_tx_stats(
			arg, msduq_tx_stats, msduq);
}

static inline QDF_STATUS
cdp_get_mpdu_stats(ol_txrx_soc_handle soc, void *arg,
		   uint64_t *svc_int_pass, uint64_t *svc_int_fail,
		   uint64_t *burst_pass, uint64_t *burst_fail,
		   uint8_t tid, uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_mpdu_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_mpdu_stats(
			arg, svc_int_pass, svc_int_fail, burst_pass,
			burst_fail, tid, msduq);
}

static inline QDF_STATUS
cdp_get_drop_stats(ol_txrx_soc_handle soc, void *arg,
		   uint64_t *pass, uint64_t *drop,
		   uint64_t *drop_ttl,
		   uint8_t tid, uint8_t msduq)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->telemetry_get_drop_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->telemetry_get_drop_stats(
			arg, pass, drop, drop_ttl, tid, msduq);
}

/**
 * cdp_sawf_peer_config_ul - Config uplink QoS parameters
 * @soc: SOC handle
 * @mac_addr: MAC address
 * @tid: TID
 * @service_interval: Service Interval
 * @burst_size: Burst Size
 * @min_tput: Min throughput
 * @max_latency: Max latency
 * @add_or_sub: Add or Sub parameters
 * @peer_id: peer id
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_peer_config_ul(ol_txrx_soc_handle soc, uint8_t *mac_addr, uint8_t tid,
			uint32_t service_interval, uint32_t burst_size,
			uint32_t min_tput, uint32_t max_latency,
			uint8_t add_or_sub, uint16_t peer_id)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->peer_config_ul) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	return soc->ops->sawf_ops->peer_config_ul(soc, mac_addr, tid,
						  service_interval, burst_size,
						  min_tput, max_latency,
						  add_or_sub, peer_id);
}

/**
 * cdp_sawf_peer_flow_count - Peer flow count in SAWF
 * @soc: SOC handle
 * @mac_addr: MAC address
 * @svc_id: Service Class ID
 * @direction: Indication of forward or reverse service class match
 * @start_or_stop: Indication of start or stop
 * @peer_mac: Peer MAC address
 * @peer_id: peer id
 * @flow_count: flow count
 *
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_sawf_peer_flow_count(ol_txrx_soc_handle soc, uint8_t *mac_addr,
			 uint8_t svc_id, uint8_t direction,
			 uint8_t start_or_stop, uint8_t *peer_mac,
			 uint16_t peer_id, uint16_t flow_count)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_peer_flow_count) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	return soc->ops->sawf_ops->sawf_peer_flow_count
		(soc, mac_addr, svc_id, direction, start_or_stop, peer_mac,
		 peer_id, flow_count);
}

/**
 * cdp_swaf_peer_sla_configuration() - Check if sla is configured for a peer
 * @soc: SOC handle
 * @mac_addr: peer mac address
 * @sla_mask: pointer to SLA mask
 * Return: QDF_STATUS
 */
static inline QDF_STATUS
cdp_swaf_peer_sla_configuration(ol_txrx_soc_handle soc, uint8_t *mac_addr,
				uint16_t *sla_mask)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->swaf_peer_sla_configuration) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_INVAL;
	}

	return soc->ops->sawf_ops->swaf_peer_sla_configuration(soc, mac_addr,
							       sla_mask);
}

/**
 * cdp_get_peer_sawf_admctrl_stats() - Call to get Peer SAWF AdmCtrl stats
 * @soc: soc handle
 * @mac: peer mac address
 * @data: opaque pointer
 * @peer_type: peer type
 *
 * return: status Success/Failure
 */
static inline QDF_STATUS
cdp_get_peer_sawf_admctrl_stats(ol_txrx_soc_handle soc, uint8_t *mac,
				void *data, enum cdp_peer_type peer_type)
{
	if (!soc || !soc->ops) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return QDF_STATUS_E_FAILURE;
	}

	if (!soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->txrx_get_peer_sawf_admctrl_stats)
		return QDF_STATUS_E_FAILURE;

	return soc->ops->sawf_ops->txrx_get_peer_sawf_admctrl_stats(soc, mac,
								    data,
								    peer_type);
}
#else
static inline QDF_STATUS
cdp_sawf_mpdu_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_sawf_mpdu_details_stats_req(ol_txrx_soc_handle soc, uint8_t enable)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_get_peer_sawf_delay_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			      uint8_t *mac, void *data)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_get_peer_sawf_tx_stats(ol_txrx_soc_handle soc, uint32_t svc_id,
			   uint8_t *mac, void *data)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_swaf_peer_sla_configuration(ol_txrx_soc_handle soc, uint8_t *mac_addr,
				uint16_t *sla_mask)
{
	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
cdp_get_peer_sawf_admctrl_stats(ol_txrx_soc_handle soc, uint8_t *mac,
				void *data, enum cdp_peer_type peer_type)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

#ifdef WLAN_FEATURE_11BE_MLO_3_LINK_TX
static inline
uint16_t cdp_sawf_get_peer_msduq(ol_txrx_soc_handle soc,
				 uint8_t vdev_id, uint8_t *dest_mac,
				 uint32_t dscp_pcp, bool pcp)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->get_peer_msduq) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	return soc->ops->sawf_ops->get_peer_msduq
		(soc, vdev_id, dest_mac, dscp_pcp, pcp);
}

static inline QDF_STATUS
cdp_sawf_3_link_peer_flow_count(ol_txrx_soc_handle soc, uint8_t *mac_addr,
				uint16_t peer_id, uint32_t mark_metadata)
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_3_link_peer_flow_count) {
		dp_cdp_debug("Invalid Instance");
		QDF_BUG(0);
		return false;
	}

	return soc->ops->sawf_ops->sawf_3_link_peer_flow_count
		(soc, mac_addr, peer_id, mark_metadata);
}

static inline QDF_STATUS
cdp_sawf_3_link_peer_set_tid_weight(ol_txrx_soc_handle soc,
				    uint8_t *mac_addr,
				    uint16_t peer_id,
				    uint8_t tid_weight[])
{
	if (!soc || !soc->ops || !soc->ops->sawf_ops ||
	    !soc->ops->sawf_ops->sawf_3_link_peer_set_tid_weight) {
		dp_cdp_debug("Invalid Instance");
		return false;
	}

	return soc->ops->sawf_ops->sawf_3_link_peer_set_tid_weight
		(soc, mac_addr, peer_id, tid_weight);
}
#endif
#endif /* _CDP_TXRX_SAWF_H_ */
