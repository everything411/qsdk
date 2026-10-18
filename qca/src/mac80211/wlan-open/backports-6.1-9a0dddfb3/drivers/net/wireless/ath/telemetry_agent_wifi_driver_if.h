/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__
#define __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__

#define TELEMETRY_MAX_MCS (24 + 1)

#define NUM_TID 8
#define NUM_AC 4
#define EHTCAP_TXRX_MCS_NSS_IDX_MAX 4
#define MU_USERS 37
#define MUMIMO_USERS 8
#define SAWF_NUM_QUEUES (NUM_TID * 2)
#define RSSI_MIN_VALID -100

#define SET_PEER_FLAG(_val, _attr) \
	((_val) |= (1 << (PEER_FLAGS_##_attr##_BIT)))
#define CLEAR_PEER_FLAG(_val, _attr) \
	((_val) &= ~(1 << (PEER_FLAGS_##_attr##_BIT)))
#define IS_PEER_FLAG_SET(_val, _attr) \
	(((_val) >> PEER_FLAGS_##_attr##_BIT) & 0x1)

struct agent_psoc_obj {
	void *psoc_back_pointer;
	u8 psoc_id;
};

struct agent_pdev_obj {
	void *pdev_back_pointer;
	void *psoc_back_pointer;
	u8 psoc_id;
	u8 pdev_id;
};

struct agent_peer_obj {
	void *peer_back_pointer;
	void *pdev_back_pointer;
	void *psoc_back_pointer;
	u8 psoc_id;
	u8 pdev_id;
	u8 peer_mac_addr[6];
	u16 peer_id;
	u8 hw_link_id;
};

enum agent_notification_event {
	AGENT_NOTIFY_EVENT_INIT,
	AGENT_NOTIFY_EVENT_DEINIT,
};

enum agent_params {
	AGENT_INVALID_PARAM,
	AGENT_SET_DEBUG_LEVEL,
};

enum agent_t2lm_direction {
	T2LM_INVALID_DIRECTION = 0,
	T2LM_DOWNLINK_DIRECTION = 1,
	T2LM_UPLINK_DIRECTION = 2,
	T2LM_BIDI_DIRECTION = 3,
	T2LM_MAX_VALID_DIRECTION = T2LM_BIDI_DIRECTION,
};

/* Bit definitions for peer_flags */
#define PEER_FLAGS_BTM_CAP_BIT 0        /* Peer BTM capability */

struct t2lm_of_tids {
	enum agent_t2lm_direction direction; /* 0-DL, 1-UL, 2-BiDi */
	u8 default_link_mapping;
	/* Bit0 for link0, bit1 for link1 and so on */
	u16 t2lm_provisioned_links[NUM_TID];
};

struct eht_peer_caps {
	u32 tx_mcs_nss_map[EHTCAP_TXRX_MCS_NSS_IDX_MAX];
	u32 rx_mcs_nss_map[EHTCAP_TXRX_MCS_NSS_IDX_MAX];
};

struct agent_msduq_info_iface_obj {
	u8 is_used;
	u8 svc_id;
	u8 svc_type;
	u8 svc_tid;
	u8 svc_ac;
	u8 priority;
	u32 service_interval;
	u32 burst_size;
	u32 min_throughput;
	u32 delay_bound;
	u32 mark_metadata;
};

struct agent_peer_iface_init_obj {
	u8 peer_mld_mac[6];
	u8 peer_link_mac[6];
	u8 ap_mld_addr[6];
	u8 is_assoc_link;
	int ifindex;
	u8 vdev_id;
	u8 bw;
	u16 freq;
	struct t2lm_of_tids t2lm_info[T2LM_MAX_VALID_DIRECTION]; /* T2LM mapping */
	struct eht_peer_caps caps;          /* Peer capabilities */
	u8 ieee_link_id;
	u16 disabled_link_bitmap;
	u16 peer_flags;
	u8 phymode;
	struct agent_msduq_info_iface_obj msduq_info[SAWF_NUM_QUEUES];
};

struct agent_pdev_iface_init_obj {
	/* This info is stroed in telemetry pdev object,
	 * so this can be ignored for now
	 */
	u16 link_id;
	u8 soc_id;
	u8 band;
};

struct agent_psoc_iface_init_obj {
	u8 soc_id;
	u16 max_peers;
	u16 num_peers;

};

struct agent_peer_iface_stats_obj {
	u8 peer_mld_mac[6];
	u8 peer_link_mac[6];
	u8 airtime_consumption[NUM_AC];
	u16 tx_airtime_consumption[NUM_AC];
	u32 tx_mpdu_retried;
	u32 tx_mpdu_total;
	u32 rx_mpdu_retried;
	u32 rx_mpdu_total;
	u8 rssi;
	u16 sla_mask; /* Uses telemetry_sawf_param for bitmask */
};

struct agent_link_iface_stats_obj {
	u16 link_id;
	u8 soc_id;
	u8 available_airtime[NUM_AC];
	u32 congestion[NUM_AC];
	u32 tx_mpdu_failed[NUM_AC];
	u32 tx_mpdu_total[NUM_AC];
	u8 link_airtime[NUM_AC];
	u8 freetime;
	u8 obss_airtime;
	u32 traffic_condition[NUM_AC];
	u32 error_margin[NUM_AC];
	u32 num_dl_asymmetric_clients[NUM_AC];
	u32 num_ul_asymmetric_clients[NUM_AC];
	u8 dl_payload_ratio[NUM_AC];
	u8 ul_payload_ratio[NUM_AC];
	u32 avg_chan_latency[NUM_AC];
	bool is_mon_enabled;
	u16 freq;
};

struct emesh_peer_iface_stats_obj {
	u8 peer_link_mac[6];
	u16 tx_airtime_consumption[NUM_AC];
};

struct emesh_link_iface_stats_obj {
	u8 link_mac[6];
	u8 link_idle_airtime;
};

struct deter_peer_iface_stats_obj {
/*
 * uint8_t peer_link_mac[6];
 * struct deter_peer_iface_stats deter[NUM_TID];
 * uint8_t vdev_id;
 */
};

struct deter_link_iface_stats_obj {
/*
 * uint8_t link_mac[6];
 * uint8_t hw_link_id;
 * uint64_t dl_ofdma_usr[MU_USERS];
 * uint64_t ul_ofdma_usr[MU_USERS];
 * uint64_t dl_mimo_usr[MUMIMO_USERS];
 * uint64_t ul_mimo_usr[MUMIMO_USERS];
 * uint64_t dl_mode_cnt[TX_DL_MAX];
 * uint64_t ul_mode_cnt[TX_UL_MAX];
 * uint64_t rx_su_cnt;
 * uint32_t ch_access_delay[NUM_AC];
 * struct deter_link_chan_util_stats ch_util;
 * struct deter_link_ul_trigger_status ts[TX_UL_MAX];
 */
};

struct erp_link_iface_stats_obj {
	u64 tx_data_msdu_cnt;
	u64 rx_data_msdu_cnt;
	u64 total_tx_data_bytes;
	u64 total_rx_data_bytes;
	u8 sta_vap_exist;
	u64 time_since_last_assoc;
};

struct admctrl_link_iface_stats_obj {
	u16 link_id;
	u8 freetime;
	u8 tx_link_airtime[NUM_AC];
};

struct admctrl_msduq_iface_stats_obj {
	u64 tx_success_num;
};

struct admctrl_peer_iface_stats_obj {
	u8 peer_link_mac[6];
	u8 peer_mld_mac[6];
	u8 is_assoc_link;
	u8 tx_airtime_consumption[NUM_AC];
	u64 tx_success_num;
	u64 mld_tx_success_num;
	u64 avg_tx_rate;
	struct admctrl_msduq_iface_stats_obj msduq_stats[SAWF_NUM_QUEUES];
};

enum telemetry_packet_type {
	TELEMETRY_DOT11_A = 0,
	TELEMETRY_DOT11_B = 1,
	TELEMETRY_DOT11_N = 2,
	TELEMETRY_DOT11_AC = 3,
	TELEMETRY_DOT11_AX = 4,
	TELEMETRY_DOT11_BA = 5,
	TELEMETRY_DOT11_BE = 6,
	TELEMETRY_DOT11_AZ = 7,
	TELEMETRY_DOT11_N_GF = 8,
	TELEMETRY_DOT11_BN = 9,	/* UHR */
	TELEMETRY_DOT11_MAX,
};

struct telemetry_pkt_type {
	u32 mcs_count[TELEMETRY_MAX_MCS];
};

struct telemetry_msduq_tx_stats {
	u32 tx_failed;
	u32 retry_count;
	u32 total_retries_count;
	struct telemetry_pkt_type packet_type[TELEMETRY_DOT11_MAX];
};

struct agent_peer_tx_ext_stats {
	u32 avg_ack_rssi;
	u32 tx_failed;
	u32 retries;
	u32 total_retries;
	u64 tx_cnt;
	u64 tx_bytes;
	struct telemetry_pkt_type packet_type[TELEMETRY_DOT11_MAX];
};

enum telemetry_threshold_type {
	THRESHOLD_RSSI_MIN = 0,
	THRESHOLD_RSSI_MAX = 1,
	THRESHOLD_ACKRSSI_MIN = 2,
	THRESHOLD_ACKRSSI_MAX = 3,
	THRESHOLD_TXRATE_MIN = 4,
	THRESHOLD_TXRATE_MAX = 5,
	THRESHOLD_RXRATE_MIN = 6,
	THRESHOLD_RXRATE_MAX = 7,
	THRESHOLD_MAX,
};

enum hysteresis_type {
	HYSTERESIS_TYPE_RSSI = 0,
	HYSTERESIS_TYPE_RATE = 1,
};

/* Path type for datapath source */
enum telemetry_path_type {
	PATH_TYPE_RX = 0,     /* RX path: RSSI + RX Rate */
	PATH_TYPE_TX = 1,     /* TX path: ACK RSSI + TX Rate */
};

struct telemetry_agent_ops {
	int  (*agent_psoc_create_handler)(void *arg, struct agent_psoc_obj *psoc_obj);
	int  (*agent_psoc_destroy_handler)(void *arg, struct agent_psoc_obj *psoc_obj);
	int  (*agent_pdev_create_handler)(void *arg, struct agent_pdev_obj *pdev_obj);
	int  (*agent_pdev_destroy_handler)(void *arg, struct agent_pdev_obj *pdev_obj);
	int  (*agent_peer_create_handler)(void *arg, struct agent_peer_obj *peer_obj);
	int  (*agent_peer_destroy_handler)(void *arg, struct agent_peer_obj *peer_obj);
	int  (*agent_set_param)(int command, int value);
	int  (*agent_get_param)(int command);
	void (*agent_notify_app_event)(enum agent_notification_event, int service_id,
				       u64 service_data);
	void (*agent_notify_host_event)(enum agent_notification_event event,
					int service_id,
					u8 category);
	void (*agent_notify_emesh_event)(enum agent_notification_event);
	void (*agent_dynamic_app_init_deinit_notify)(enum agent_notification_event,
						     int service_id,
						     u64 service_data,
						     bool is_container_app);
	int (*agent_get_psoc_info)(void *obj, struct agent_psoc_iface_init_obj *stats);
	int (*agent_get_pdev_info)(void *obj, struct agent_pdev_iface_init_obj *stats);
	int (*agent_get_peer_info)(void *obj, struct agent_peer_iface_init_obj *stats);
	int (*agent_get_pdev_stats)(void *obj, struct agent_link_iface_stats_obj *stats);
	int (*agent_get_peer_stats)(int obj_id, void *parent, u8 hw_link_id,
				    struct agent_peer_iface_stats_obj *stats);
	int (*agent_get_emesh_pdev_stats)(void *obj,
					  struct emesh_link_iface_stats_obj *stats);
	int (*agent_get_emesh_peer_stats)(void *obj,
					  struct emesh_peer_iface_stats_obj *stats);
	int (*agent_get_deter_pdev_stats)(void *obj,
					  struct deter_link_iface_stats_obj *stats);
	int (*agent_get_deter_peer_stats)(void *obj,
					  struct deter_peer_iface_stats_obj *stats);
	int (*agent_get_erp_pdev_stats)(void *obj,
					struct erp_link_iface_stats_obj *stats);
	int (*agent_get_admctrl_pdev_stats)(void *obj,
					    struct admctrl_link_iface_stats_obj *stats);
	int (*agent_get_admctrl_peer_stats)(void *obj,
					    struct admctrl_peer_iface_stats_obj *stats);
	int (*agent_get_peer_tx_stats)(void *obj, void *stats);
	int (*agent_peer_sla_stats_threshold)(u8 *peer_mac,
					      u32 packet_error_rate,
					      u32 retries_threshold,
					      u32 mcs_min_threshold,
					      u32 mcs_max_threshold,
					      u32 min_thruput_rate,
					      u32 max_thruput_rate);
	int (*agent_get_pext_stats_enabled_flag)(void *obj, void *flag);
	int (*agent_pull_tx_peer_stats)(u8 *peer_mac,
					u32 *min_tput,
					u32 *max_tput,
					u32 *avg_tput,
					u32 *per,
					u32 *retries_pct);

	/* SAWF ops */
	void *(*sawf_alloc_peer)(void *sawf_ctx, void *sawf_stats_ctx,
				 u8 *mac_addr,
				 u8 svc_id, u8 hostq_id);
	void (*sawf_free_peer)(void *telemetry_sawf_ctx);
	void (*sawf_peer_stats_reset)(void *telemetry_ctx);
	int (*sawf_updt_queue_info)(void *telemetry_sawf_ctx,
				    u8 svc_id,
				    u8 tid, u8 msduq_idx);
	int (*sawf_update_msduq_info)(void *telemetry_sawf_ctx,
				      u8 hostq_id, u8 tid,
				      u8 msduq_idx, u8 svc_id);
	int (*sawf_clear_msduq_info)(void *telemetry_sawf_ctx,
				     u8 hostq_id);
	int (*sawf_updt_delay_mvng)(u32 num_win, u32 num_pkt);
	int (*sawf_updt_sla_params)(u32 num_pkt, u32 time_sec);
	int (*sawf_set_sla_cfg)(u8 svc_id, u8 min_thruput_rate,
				u8 max_thruput_rate,
				u8 burst_size,
				u8 service_interval,
				u8 delay_bound, u8 msdu_ttl,
				u8 msdu_rate_loss);
	int (*sawf_set_sla_config)(u8 svc_id, u8 min_thruput_rate,
				   u8 max_thruput_rate,
				   u8 burst_size,
				   u8 service_interval,
				   u8 delay_bound, u8 msdu_ttl,
				   u8 msdu_rate_loss,
				   u8 packet_error_rate,
				   u8 mcs_min_threshold,
				   u8 mcs_max_threshold,
				   u8 retries_threshold);
	int (*sawf_set_svclass_cfg)(bool enable, u8 svclass_id,
				    u32 min_thruput_rate,
				    u32 max_thruput_rate,
				    u32 burst_size,
				    u32 service_interval,
				    u32 delay_bound,
				    u32 msdu_ttl,
				    u32 msdu_rate_loss);
	int (*sawf_set_sla_dtct_cfg)(u8 detect_type,
				     u8 min_thruput_rate,
				     u8 max_thruput_rate,
				     u8 burst_size,
				     u8 service_interval,
				     u8 delay_bound,
				     u8 msdu_ttl,
				     u8 msdu_rate_loss);
	int (*sawf_set_sla_detect_config)(u8 detect_type,
					  u8 min_thruput_rate,
					  u8 max_thruput_rate,
					  u8 burst_size,
					  u8 service_interval,
					  u8 delay_bound,
					  u8 msdu_ttl,
					  u8 msdu_rate_loss,
					  u8 packet_error_rate,
					  u8 mcs_min_threshold,
					  u8 mcs_max_threshold,
					  u8 retries_threshold);
	int (*sawf_push_delay)(void *telemetry_sawf_ctx, u8 tid,
			       u8 queue, u64 pass,
			       u64 fail);
	int (*sawf_push_delay_mvng)(void *telemetry_ctx, u8 tid,
				    u8 queue, u64 nwdelay_avg,
				    u64 swdelay_avg,
				    u64 hwdelay_avg);
	int (*sawf_push_msdu_drop)(void *telemetry_sawf_ctx, u8 tid,
				   u8 queue, u64 pass,
				   u64 fail_drop, u64 fail_ttl);
	int (*sawf_pull_rate)(void *telemetry_sawf_ctx, u8 tid,
			      u8 queue, u32 *egress_rate,
			      u32 *ingress_rate);
	int (*sawf_pull_tx_rate)(void *telemetry_sawf_ctx, u8 tid,
				 u8 queue,
				 u32 *min_tput, u32 *max_tput,
				 u32 *avg_tput, u32 *per,
				 u32 *retries_pct);
	int (*sawf_pull_mov_avg)(void *telemetry_sawf_ctx, u8 tid,
				 u8 queue, u32 *nwdelay_avg,
				 u32 *swdelay_avg, u32 *hwdelay_avg);
	int (*sawf_reset_peer_stats)(u8 *mac_addr);
	int (*sawf_get_tput_stats)(void *soc, void *arg, u64 *in_bytes,
				   u64 *in_cnt, u64 *tx_bytes,
				   u64 *tx_cnt, u8 tid,
				   u8 msduq);
	int (*sawf_get_mpdu_stats)(void *soc, void *arg, u64 *svc_int_pass,
				   u64 *svc_int_fail, u64 *burst_pass,
				   u64 *burst_fail, u8 tid,
				   u8 msduq);
	int (*sawf_get_drop_stats)(void *soc, void *arg, u64 *pass,
				   u64 *drop, u64 *drop_ttl,
				   u8 tid, u8 msduq);
	void (*sawf_notify_breach)(u8 *mac_addr, u8 svc_id,
				   u8 param, bool set_clear, u8 tid,
				   u8 queue_id);
	int (*sawf_get_msduq_tx_stats)(void *soc, void *arg,
				       void *msduq_tx_stats,
				       u8 msduq);

	/* RSSI/Rate breach detection & notification ops */
	int (*agent_set_rssi_rate_threshold)(u8 threshold_type, u32 value);
	int (*agent_print_rssi_rate_thresholds)(void);
	int (*agent_set_rssi_rate_breach_mask)(u8 mask);
	int (*agent_get_rssi_rate_threshold)(u8 threshold_type);
	u8 (*agent_get_rssi_rate_breach_mask)(void);
	int (*agent_update_rssi_rate_breach)(u8 soc_id,
					     u8 pdev_id,
					     u16 peer_id,
					     u8 *peer_mac,
					     u8 path_type,
					     s32 rssi_value,
					     u32 rate_value);
	void (*agent_notify_rssi_rate_breach)(u8 *peer_mac,
					      u8 breach_type,
					      u32 threshold_value,
					      u32 detected_value,
					      bool set_clear);
	int (*agent_set_hysteresis)(u8 type, u32 value);
	int (*agent_get_hysteresis)(u8 type, u64 *hyst_value);
};

void wlan_cfg80211_t2lm_app_reply_generic_response(void *gen_data,
						   u8 category,
						   u8 service_id);
#endif /* __TELEMETRY_AGENT_WIFI_DRIVER_IF_H__ */
