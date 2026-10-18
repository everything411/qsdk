/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_WIFI8_H
#define ATH12K_DP_TX_WIFI8_H

#include "../dp.h"
#include "hal_desc.h"
#include "hal.h"
#include "dp_tx_flow_info.h"

#define DP_TX_SFE_BUFFER_SIZE           256

#define ATH12K_DP_TX_CONGESTION_CTRL_INTERVAL_MS	100
#define ATH12K_DP_TX_CONGESTION_CTRL_2SEC_MS		(2 * MSEC_PER_SEC)
#define ATH12K_DP_TX_GET_USED_THRSHLD(_cnt_, _POOL_)	(((_cnt_) * (_POOL_) * 80) / 100)
#define ATH12K_DP_TX_DEFAULT_HARD_DROP_THRESHOLD	10240
#define ATH12K_DP_TX_SORT_FLOW_DROP_GRACE		2
#define ATH12K_DP_TX_FLOW_CONTINUOUS_DROP_THRESHOLD	5

/* FW cookie HW link ID field: bits [15:13] of the 20-bit SW cookie.
 * Encodes the hw_link_id of the chip that owns the buffer, used to
 * derive the correct FW buffer manager (RBM) in multi-chip configs.
 */
#define ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID_M	0x0000E000
#define ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID_S	13
#define ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID_GET(_var) \
	(((_var) & ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID_M) >> \
	 ATH12K_DP_TX_FW_COOKIE_HW_LINK_ID_S)

/**
 * enum ath12k_wifi8_congstn_ctrl_param_type - congestion control parameter types
 * ATH12K_CONGSTN_CTRL_USED_THRESHOLD: set used_threshold (u32)
 * ATH12K_CONGSTN_CTRL_FLOW_DROP_GRACE_PCT: set flow_drop_grace_percent (0-100)
 * ATH12K_CONGSTN_CTRL_HISTORY_ENABLE: enable(1)/disable(0) history logging
 * ATH12K_CONGSTN_CTRL_RESET_USED: Reset the max used count
 * ATH12K_CONGSTN_CTRL_PPEDS_USED_THRESHOLD: set ppeds_used_threshold (u32)
 */
enum ath12k_wifi8_congstn_ctrl_param_type {
	ATH12K_CONGSTN_CTRL_USED_THRESHOLD,
	ATH12K_CONGSTN_CTRL_FLOW_DROP_GRACE_PCT,
	ATH12K_CONGSTN_CTRL_HISTORY_ENABLE,
	ATH12K_CONGSTN_CTRL_RESET_USED,
	ATH12K_CONGSTN_CTRL_PPEDS_USED_THRESHOLD,

	/* keep last */
	ATH12K_CONGSTN_CTRL_TYPE_MAX
};

struct ath12k_dp_tx_queue {
	u16 peer_id;
	u16 cookie;
	u8 hw_link_id;
	u8 addr[ETH_ALEN];
	void (*cb)(struct ath12k_dp *dp, void *ctx,
		   u8 *addr, u8 tid);
};
struct ath12k_dp_tqm_cmd {
	struct list_head list;
	struct ath12k_dp_tx_queue data;
	enum hal_tlv_tag_be cmd_type;
	int cmd_num;
	void (*handler)(struct ath12k_dp *dp, void *ctx,
			struct hal_tqm_status *tqm_status);
};

struct ath12k_wifi8_dp_tx_flow_cost {
	u8 svc;
	u8 idx;
	u32 num;
	u32 weight;
	u32 cost;
};

struct ath12k_wifi8_flow_entry {
	union {
		u32 flow_number;
		struct ath12k_flow_metadata flow_info;
	} u;
	enum hal_tqm_service_category svc;
	u32 msdu_count;
};

struct ath12k_wifi8_flow_remove_entry {
	union {
		u32 flow_number;
		struct ath12k_flow_metadata flow_info;
	} u;
	u32 drop;
	u8 svc:2,
	   idx:4,
	   limited:1;
	u32 limit;
};

struct ath12k_wifi8_svc_sorted_flows {
	struct ath12k_wifi8_flow_entry flows[HAL_TQM_MAX_SORTED_FLOW];
	u32 total_msdu_count;
	u8 num_flows;
	u8 weight;
};

struct ath12k_wifi8_svc_remove_flows {
	struct ath12k_wifi8_flow_remove_entry flows[HAL_TQM_MAX_SORTED_FLOW_ALL_SVC];
	u8 num_flows;
};

extern bool ath12k_congestion_ctrl;
extern unsigned int ath12k_drop_algo;

int ath12k_wifi8_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id, int budget);
u32 ath12k_wifi8_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_vif *ahvif,
					    u8 link_id,
					    bool force_vdev_id_check_disable);
int ath12k_wifi8_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb,
				       struct ath12k_link_sta *arsta,
				       struct ath12k_dp_peer *dp_peer);
int ath12k_wifi8_dp_tx_ring_alloc(struct ath12k_base *ab);
int ath12k_wifi8_dp_tx_ring_init(struct ath12k_base *ab);
void ath12k_wifi8_dp_tx_ring_cleanup(struct ath12k_base *ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_wifi8_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget);
#else
static inline int
ath12k_wifi8_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	return 0;
}
#endif
int ath12k_wifi8_dp_tx_exception_handler(struct ath12k_dp *dp, int budget);

/*
 * ath12k_wifi8_dp_tx_exception_to_tx_comp() - Redirect TX exception buffer
 * to the normal TX completion ring instead of freeing it directly.
 * Sends a SW2TQM command so C-TQM routes the completion to the normal TX
 * completion ring.
 */
int ath12k_wifi8_dp_tx_exception_to_tx_comp(struct ath12k_dp *dp,
					    dma_addr_t paddr,
					    u8 rbm,
					    u32 cookie);
int ath12k_wifi8_dp_tqm2sw_fw_handler(struct ath12k_dp *dp, int budget);
int ath12k_wifi8_dp_tqm_cmd_send(struct ath12k_base *ab,
				 enum hal_tlv_tag_be type,
				 struct ath12k_hal_tqm_cmd *cmd,
				 struct ath12k_dp_tx_queue *data,
				 void (*callback_fn)(
					 struct ath12k_dp *dp,
					 void *ctx,
					 struct hal_tqm_status *tqm_status));
int ath12k_wifi8_dp_tx_process_tqm_status(struct ath12k_dp *dp, int budget);
void ath12k_dp_peer_cleanup_tqm_sync(struct ath12k_dp *dp, void *ctx,
				     struct hal_tqm_status *tqm_status);
void ath12k_wifi8_dp_tx_tqm_cmd_list_cleanup(struct ath12k_base *ab);
void ath12k_wifi8_dp_peer_tqm_sync_release(struct kref *ref);
int ath12k_wifi8_dp_tx_process_sam_status(struct ath12k_dp *dp, int budget);
enum ath12k_dp_tx_enq_error
ath12k_wifi8_dp_tx_mcast_send(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_vif *ahvif,
			      struct ath12k_dp_link_vif *dp_link_vif,
			      u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
			      bool gsn_valid, u16 gsn,
			      struct sk_buff *skb, struct ath12k_link_sta *arsta,
			      struct ath12k_dp_skb_ctrl *skb_ctrl, bool htt_mesh);
void ath12k_wifi8_dp_tx_set_ast(struct ath12k_dp_peer *dp_peer,
				struct ath12k_dp_tx_msdu_info *msdu_info,
				u8 hw_link_id);
void ath12k_wifi8_ucast_handler(struct ath12k_dp_vif *dp_vif, u8 link_id,
				struct ath12k_link_sta *arsta, struct sk_buff *skb,
				struct ath12k_dp_skb_ctrl *skb_ctrl, u32 qos_nw_delay,
				struct ath12k_vif *vlan_ahvif,
				struct ath12k_dp_peer *dp_peer);
void ath12k_wifi8_mcbc_handler(struct ath12k_dp_vif *dp_vif, u8 link_id,
			       struct ath12k_link_sta *arsta, struct sk_buff *skb,
			       bool is_eth, bool gsn_valid, bool is_sta,
			       struct ath12k_dp_skb_ctrl *skb_ctrl, u32 qos_nw_delay,
			       bool htt_mesh, struct ath12k_vif *vlan_ahvif,
			       struct ieee80211_tx_info *info,
			       struct ieee80211_sta *sta);
ssize_t ath12k_wifi8_dp_tx_dump_svc_sorted_list(struct ath12k_dp *dp, u8 ac_mask,
						char *buf, int size);
int ath12k_wifi8_dp_tx_congestion_control_init(struct ath12k_dp *dp);
void ath12k_wifi8_dp_tx_congestion_control_deinit(struct ath12k_dp *dp);
void ath12k_wifi8_dp_tx_congestion_recovery_handler(struct timer_list *t);
int ath12k_wifi8_dp_tx_update_msdu_flow(struct ath12k_dp *dp, u32 flow_number,
					u8 tid, u8 service_category,
					u16 hard_drop_threshold);
ssize_t ath12k_wifi8_dp_tx_dump_congestion_ctrl_stats(struct ath12k_dp *dp,
						      char *buf, int size);
ssize_t ath12k_wifi8_dp_tx_dump_congestion_recovery_hist(struct ath12k_dp *dp,
							 char *buf, int size);
int ath12k_wifi8_dp_tx_set_congestion_ctrl_param(struct ath12k_dp *dp,
						 u32 type, u32 value);
#endif
