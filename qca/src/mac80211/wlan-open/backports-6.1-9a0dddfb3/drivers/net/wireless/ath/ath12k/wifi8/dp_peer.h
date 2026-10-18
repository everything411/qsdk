/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_PEER_WIFI8_H
#define ATH12K_PEER_WIFI8_H

#include "../dp_cmn.h"
#include "hw.h"
#include "dp_tx_flow_info.h"

#define ATH12K_SMD_MGMT_TID	15

struct peer_assoc_flowq_params;
struct ath12k_dp_peer_ext_ctx {
	struct ath12k_dp_tx_flow_info tx_flow_info;
	u16 ast_index;
	u16 ast_hash;
};

static inline struct ath12k_dp_tx_flow_info *
ath12k_dp_get_tx_flow_info_from_peer(struct ath12k_dp_peer *dp_peer)
{
	if (!dp_peer->peer_ext_ctx)
		return NULL;

	return &dp_peer->peer_ext_ctx->tx_flow_info;
}

int ath12k_wifi8_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif);
void ath12k_wifi8_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id,
				 struct ieee80211_vif *vif);
int ath12k_wifi8_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr,
			       struct ath12k_dp_peer_create_params *params);
void ath12k_wifi8_dp_link_peer_assign_id(struct ath12k_dp *dp, struct ath12k *ar,
					 struct ath12k_dp_link_peer *peer);
void ath12k_wifi8_dp_link_peer_unassign_id(struct ath12k_dp *dp, struct ath12k *ar,
					   struct ath12k_dp_link_peer *peer);
void ath12k_dp_peer_cleanup_indication(struct ath12k_dp *dp,
				       u16 peer_id,
				       u8 hw_link_id);
void ath12k_wifi8_dp_link_peer_assoc(struct ath12k_dp_hw *dp_hw, struct ath12k_dp *dp,
				     u8 *addr, u32 hw_link_id, struct ieee80211_vif *vif);
int ath12k_wifi8_get_mgmt_flowq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
				u8 *addr, struct peer_assoc_flowq_params *flowq_params,
				struct ieee80211_vif *vif);
int ath12k_wifi8_get_holq(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			  u8 *addr, struct peer_assoc_holq_params *holq_params,
			  struct ieee80211_vif *vif);
int ath12k_wifi8_dp_get_peer_init_status(struct ath12k_dp *dp,
					 struct ath12k_dp_hw *dp_hw,
					 u8 *addr);
void ath12k_wifi8_dp_peer_release(struct kref *ref);
void ath12k_wifi8_dp_vif_update_4addr(struct ath12k_dp_hw *dp_hw,
				      struct ath12k_dp_vif *dp_vif,
				      struct ieee80211_sta *sta);
int ath12k_dp_peer_fetch_smd_tx_ctx(struct ath12k_base *ab,
				    struct ath12k_dp_peer *dp_peer,
				    u32 tx_tid_bitmap,
				    u16 *tx_tid_ba_win_size,
				    void (*cb)(struct ath12k_dp *dp, void *ctx,
					       u8 *addr, u8 tid));
void ath12k_wifi8_dp_assoc_link_update(struct ath12k_dp *dp,
				       struct ath12k_hw *ah,
				       struct ieee80211_sta *sta,
				       struct ieee80211_vif *vif);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
bool ath12k_wifi8_dp_peer_ast_param_get(struct ath12k_hw *ah, struct ieee80211_vif *vif,
					u16 *ast_info, u16 *hw_peer_id, u8 *addr);
#endif
int ath12k_wifi8_peer_tx_tid_update_for_smd(struct ath12k_base *ab,
					    struct ath12k_dp_hw *dp_hw,
					    struct ieee80211_vif *vif,
					    const u8 *peer_addr,
					    struct ath12k_tx_smd_ctx_per_tid *tx_tid_ctx);
int ath12k_wifi8_dp_smd_prep_transfer_ext_ctx(
	struct ath12k_dp *dp,
	struct ath12k_dp_peer *current_dp_peer,
	const u8 *target_mld_addr,
	u16 transitioning_links);
void ath12k_wifi8_dp_smd_abort_prep(struct ath12k_dp *dp, struct ieee80211_vif *vif);
void ath12k_wifi8_dp_smd_exec_activate_links(
	struct ath12k_dp *dp,
	struct ath12k_dp_hw *dp_hw,
	struct ath12k_dp_vif *dp_vif,
	const u8 *addr,
	u16 active_links);
int ath12k_wifi8_peer_tx_tid_sn_reset(struct ath12k_base *ab,
				      struct ath12k_dp_hw *dp_hw,
				      struct ieee80211_vif *vif,
				      const u8 *peer_addr);
#endif
