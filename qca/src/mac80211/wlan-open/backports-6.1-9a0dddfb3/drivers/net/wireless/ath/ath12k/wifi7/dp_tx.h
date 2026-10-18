/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_WIFI7_H
#define ATH12K_DP_TX_WIFI7_H

#include "../dp.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/mesh_util.h"
#endif /* CPTCFG_QCN_EXTN */

#define DP_TX_SFE_BUFFER_SIZE		256

struct ath12k_dp_skb_ctrl;
struct ath12k_dp_tx_msdu_info;
struct ath12k_dp_link_vif;

int ath12k_wifi7_dp_tx_completion_handler(struct ath12k_dp *dp, int ring_id,
					  int budget);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
/* Ring id to account stats for TX completions in PPE DS */
#define ATH12K_DP_PPEDS_RING_ID DP_TCL_PPEDS_RING_IDX
#endif
u32 ath12k_wifi7_dp_tx_get_vdev_bank_config(struct ath12k_base *ab,
					    struct ath12k_vif *ahvif,
					    u8 link_id,
					    bool force_vdev_id_check_disable);
int ath12k_wifi7_sdwf_reinject_handler(struct ath12k_pdev_dp *dp_pdev,
				       struct ath12k_link_vif *arvif,
				       struct sk_buff *skb,
				       struct ath12k_link_sta *arsta,
				       struct ath12k_dp_peer *dp_peer);
int ath12k_wifi7_dp_tx_ring_alloc(struct ath12k_base *ab);
int ath12k_wifi7_dp_tx_ring_init(struct ath12k_base *ab);
int ath12k_wifi7_dp_tx_ring_setup(struct ath12k_base *ab);
void ath12k_wifi7_dp_tx_ring_cleanup(struct ath12k_base *ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_wifi7_ppeds_tx_completion_handler(struct ath12k_base *ab,
					     int budget);
#else
static inline int
ath12k_wifi7_ppeds_tx_completion_handler(struct ath12k_base *ab, int budget)
{
	return 0;
}
#endif
void ath12k_wifi7_mcbc_handler(struct ath12k_dp_vif *dp_vif,
			       u8 link_id,
			       struct ath12k_link_sta *arsta,
			       struct sk_buff *skb,
			       bool is_eth,
			       bool gsn_valid,
			       bool is_sta,
			       struct ieee80211_vif *vlan_vif,
			       struct ath12k_dp_skb_ctrl *skb_ctrl,
			       struct ieee80211_tx_info *info,
			       u32 qos_nw_delay,
			       bool htt_mesh, struct ieee80211_sta *sta);

void ath12k_wifi7_ucast_handler(struct ath12k_dp_vif *dp_vif,
				u8 link_id,
				struct ath12k_link_sta *arsta,
				struct sk_buff *skb,
				struct ath12k_dp_skb_ctrl *skb_ctrl,
				u32 qos_nw_delay, bool htt_mesh,
				struct ath12k_dp_peer *dp_peer);

enum ath12k_dp_tx_enq_error
ath12k_wifi7_dp_tx_mcast_send(struct ath12k_pdev_dp *dp_pdev,
			      struct ath12k_vif *ahvif,
			      struct ath12k_dp_link_vif *dp_link_vif,
			      u8 ring_id, struct ath12k_dp_tx_msdu_info *msdu_info,
			      bool gsn_valid, u16 gsn,
			      struct sk_buff *skb, struct ath12k_link_sta *arsta,
			      struct ath12k_dp_skb_ctrl *skb_ctrl, bool htt_mesh);
void ath12k_wifi7_dp_tx_set_ast(struct ath12k_dp_peer *dp_peer,
				struct ath12k_dp_tx_msdu_info *msdu_info,
				u8 hw_link_id);


#endif
