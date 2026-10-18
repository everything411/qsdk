/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * SMD (Seamless Mobility Domain) BSS Transition MLME state machine
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef MAC80211_SMD_H
#define MAC80211_SMD_H

/* Forward declarations — callers already include ieee80211_i.h */
struct ieee80211_sub_if_data;
struct ieee80211_link_data;
struct ieee80211_smd_prep_target;
struct ieee80211_mgd_assoc_data;
struct ieee80211_vif;
struct ieee80211_mgmt;
struct wiphy;
struct wiphy_work;
struct cfg80211_smd_prepare_req;
struct element;

void ieee80211_smd_prep_timeout_work(struct wiphy *wiphy,
				     struct wiphy_work *work);
void ieee80211_smd_prep_init(struct ieee80211_sub_if_data *sdata);
void ieee80211_smd_prep_deinit(struct ieee80211_sub_if_data *sdata);
void ieee80211_smd_cancel_all_targets(struct ieee80211_sub_if_data *sdata);
void ieee80211_smd_dl_drain_work(struct wiphy *wiphy,
				 struct wiphy_work *work);

int ieee80211_smd_execute_transition(struct ieee80211_sub_if_data *sdata,
				     struct ieee80211_smd_prep_target *target,
				     u16 dl_drain_time_tu);
void ieee80211_smd_prep_complete_target(struct ieee80211_sub_if_data *sdata,
					struct ieee80211_smd_prep_target *target);
int ieee80211_smd_prep_setup(struct ieee80211_sub_if_data *sdata,
			     struct ieee80211_smd_prep_target *target,
			     struct ieee80211_mgmt *mgmt,
			     u8 *ie_start, size_t ie_len);
void ieee80211_smd_start_exec_timeout(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_smd_prep_target *target,
				      u16 timeout_tu);
void __ieee80211_smd_dl_drain_complete(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_smd_prep_target *target,
				       bool defer_complete);
int ieee80211_smd_parse_trans_params(struct ieee80211_sub_if_data *sdata,
				     const u8 *data, size_t len,
				     struct ieee80211_smd_prep_target *target);
int ieee80211_smd_parse_exec_trans_params(struct ieee80211_sub_if_data *sdata,
					  const u8 *data, size_t len,
					  u32 *dl_drain_time);
int ieee80211_smd_parse_ml_persta(struct ieee80211_sub_if_data *sdata,
				  const u8 *data, size_t len,
				  struct ieee80211_smd_prep_target *target);

int ieee80211_smd_build_link_id_remap(struct ieee80211_sub_if_data *sdata,
				      struct ieee80211_smd_prep_target *target);
int ieee80211_smd_compute_prep_bitmaps(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_smd_prep_target *target,
				       struct ieee80211_mgd_assoc_data *assoc_data);
struct ieee80211_smd_target_link;
struct sta_info;

/* Functions defined in smd.c, called from mlme.c */
bool ieee80211_smd_move_sta_state(struct ieee80211_sub_if_data *sdata,
					 const u8 *ap_addr,
					 enum ieee80211_sta_state new_state);
void ieee80211_smd_stop_old_link(struct ieee80211_vif *vif,
				  struct ieee80211_link_data *old_link,
				  unsigned int link_id);
void ieee80211_smd_free_old_links(struct ieee80211_sub_if_data *sdata,
				  struct ieee80211_smd_prep_target *target,
				  bool remap_keys);
void ieee80211_smd_rollback_link_assign(struct ieee80211_sub_if_data *sdata,
					struct ieee80211_smd_prep_target *target);
void ieee80211_smd_free_target_links(struct ieee80211_smd_prep_target *target);
int ieee80211_smd_find_sap_lid_for_band(struct ieee80211_sub_if_data *sdata,
					       enum nl80211_band band);
struct ieee80211_smd_target_link *
ieee80211_smd_alloc_target_link(struct ieee80211_sub_if_data *sdata,
				struct ieee80211_smd_prep_target *target,
				unsigned int link_id);

/* MLME functions defined in mlme.c, called from smd.c */
int ieee80211_smd_assoc_success(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_smd_prep_target *target,
				       unsigned int tap_link_id,
				       unsigned int sap_link_id);
void ieee80211_smd_assoc_success_finalize(struct ieee80211_sub_if_data *sdata,
					     struct ieee80211_smd_prep_target *target,
					     const u8 *current_sta_addr,
					     bool defer_complete);

#endif /* MAC80211_SMD_H */
