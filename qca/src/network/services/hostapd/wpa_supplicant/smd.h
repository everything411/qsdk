/*
 * SMD (Seamless Mobility Domain) - Non-AP STA interface
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SMD_H
#define SMD_H

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/list.h"
#include "common/wpa_common.h"

struct wpa_supplicant;
struct wpa_bss;
struct wpa_ssid;

#define SMD_DOMAIN_ID_LEN       6
#define SMD_MAX_LINKS		MAX_NUM_MLD_LINKS

#define SMD_ME_TIMEOUT		5
#define SMD_PTK_MODE_PER_DOMAIN 0
#define SMD_PTK_MODE_PER_AP     1

struct smd_neighbor_target {
	u32 short_ssid;
	u32 freq;
	u8 op_class;
	u8 tbtt_offset;
	u8 bssid[ETH_ALEN];
	u8 ap_mld_id;
	bool same_smd;
};

struct smd_group_member {
	struct dl_list list;
	u8 bssid[ETH_ALEN];
	u8 ap_mld_addr[ETH_ALEN];

	bool ptk_mode;
	bool smd_type;
	u8 max_targets;
	u16 timeout;
	bool dl_forwarding;

	struct os_reltime last_seen;
};

struct smd_group {
	struct dl_list list;
	u8 smd_id[ETH_ALEN];

	bool ptk_mode;
	bool ptk_mode_valid;
	bool smd_type;
	bool smd_type_valid;

	u16 min_timeout;
	u8 min_max_targets;
	bool dl_forwarding;

	struct dl_list members;
	size_t member_count;
	struct os_reltime last_update;
};

enum smd_discovery_completeness {
	SMD_DISC_NONE = 0,
	SMD_DISC_RNR_ONLY = 1,
	SMD_DISC_PARTIAL = 2,
	SMD_DISC_FULL = 3,
};

enum smd_state {
	SMD_STATE_DISABLED = 0,
	SMD_STATE_IDLE = 1,
	SMD_STATE_DISCOVERING,
	SMD_STATE_ASSOCIATING,
	SMD_STATE_ASSOCIATED,
	SMD_STATE_TRANSITIONING,
	SMD_STATE_FAILED,
};

enum smd_prep_state {
	SMD_TARGET_NONE = 0,
	SMD_TARGET_PREP_PENDING,
	SMD_TARGET_PREPARED,
	SMD_TARGET_EXEC_PENDING,
	SMD_TARGET_DRAINING,
	SMD_TARGET_TRANSITIONED,
	SMD_TARGET_FAILED,
};

/**
 * struct smd_link_info - Per-link information from ML reconfiguration element
 *
 */
struct smd_link_info {
	bool valid;
	u8 link_id;
	u8 link_addr[ETH_ALEN];
	u16 aid;
};

/**
 * struct smd_ba_info - Block Ack Agreement info per TID
 *
 */
struct smd_ba_info {
	bool valid;
	u16 block_ack_param_set;
	u16 addba_ext_param_set;		/* Starting sequence number */
};

/**
 * struct wpa_smd_prepared_target - Context for a prepared SMD target
 * @list: List head for wpa_s->smd_targets
 * @bssid: Target BSSID (Link ID/MAC)
 * @dialog_token: Unique token for the preparation dialog
 * @state: Current state of preparation (PREPARING, PREPARED, FAILED)
 */
struct wpa_smd_prepared_target {
	struct dl_list list;
	enum smd_prep_state state;

	u8 bssid[ETH_ALEN];
	u8 target_mld_addr[ETH_ALEN];
	u8 serving_bssid[ETH_ALEN];
	/* REQ-PREP-RESP-011: AID assigned by target */
	u16 aid;
	u8 dialog_token;

	bool req_dl_sn_not_transferred;
	bool req_ul_sn_not_transferred;
	/* Listen Interval */
	u16 listen_interval;

	u8 req_scs_ids[16];
	u8 num_req_scs;

	/* REQ-PREP-REQ-013/015: SN transfer Request flags */
	int no_dl_sn;
	int no_ul_sn;

	/* Link configuration */
	u16 prepared_links;
	u16 transitioning_links;
	u8 primary_link_id;
	struct smd_link_info links[SMD_MAX_LINKS];

	/* REQ-PREP-REQ-020: SNonce */
	u8 snonce[WPA_NONCE_LEN];
	/* REQ-PREP-RESP-025: ANonce */
	u8 anonce[WPA_NONCE_LEN];
	struct wpa_ptk ptk;	/* Derived PTK for target (Mode 1 Only)*/
	bool ptk_set;		/* PTK has been derived (Mode 1) or reuse flag (Mode 0)*/

	/* Security - DH Key exchange (Per-AP MLD PTK Mode 1 only)
	 *
	 * Lifecycle: dh_ctx created in Tx Path and used in Rx path
	 * to compute shared secret, freed after PTK derivation.
	 *
	 * dh_pubkey: DH public key sent in ST Preparation Request.
	 * peer_dh_pubkey: Target AP's DH public key from ST Preparation Response
	 */
	/* PTK Mode 1 Context */
	struct crypto_ecdh *dh_ctx;

	u8 *dh_pubkey;
	size_t dh_pubkey_len;
	u16 dh_group;		/* DH group used in ST Prep Req; validated on Resp */
	u8* peer_dh_pubkey;
	size_t peer_dh_pubkey_len;

	struct {
		bool set;
		u8 igtk;
	};

	/* REQ-PREP-RESP-034: Execution timeout */
	u32 execution_timeout;	/* Timeout in TUs */
	u8 dl_drain_time;	/* DL drain time from EXEC Response */

	struct smd_ba_info dl_ba[8];
	struct smd_ba_info ul_ba[8];

	/* REQ-PREP-RESP-020 to REQ-PREP-RESP-021: SCS List */
#define MAX_SCS_IDS 16
	u8 num_accepted_scs;
	u8 accepted_scs_ids[MAX_SCS_IDS];

	/* Bitmask of links with success status */
	u16 accepted_links;

	u8 exec_req_dl_tid_bitmpa;

	/*
	 * force_diff_tx - mirrors the FORCE_DIFF_TX flag from SMD_PREPARE.
	 * Stored here so wpas_smd_request_execute() can honour it when ST
	 * Execution is triggered automatically after PREP completes.
	 * 1 = send ST Exec on a non-assoc link; 0 = use assoc link (default).
	 */
	u8 force_diff_tx;

	/* exec_path for auto-execute: 0=via serving AP, 1=via target AP */
	u8 auto_exec_path;
	/* 1 = this is the ROAM ST preferred target; triggers
	 * eloop ST Execute work on PREP response */
	int is_preferred_target;

	u16 dl_drain_duration;
	bool dl_drain_duration_valid;

	u8 latest_ul_sn_tid_bitmap;
	u16 latest_ul_sn[8];

	/* Exec Resp: Group Key Data
	 *
	 * GKD is isntalled on transitioning links during EXEC, but primary
	 * link GTK mist be instaleld at COMPLETE when primary switches.
	 * Store a copy of GKD here so COMPLETE handler can use it.
	 */
	u8 *gkd;
	size_t gkd_len;

	struct os_reltime prep_time;

	/* Status Code from response */
	u16 status_code;

	u8 *prep_resp_frame;
	size_t prep_resp_frame_len;
	u8 *exec_resp_frame;
	size_t exec_resp_frame_len;

	/* PTK install tracking: true once PTK has been installed for partner
	 * (transitioning) links.  Set during PREP when state is PARTIAL;
	 * deferred to EXEC when state was PENDING at PREP time.
	 */
	bool partner_ptk_installed;
	/* GTK installed for transitioning (partner) links at exec response time.
	 * Allows early PM=0 dynamic context before primary link transitions. */
	bool partner_gtk_installed;

	/*
	 * transition_complete_pending: set when NL80211_CMD_SMD_TRANSITION_DONE
	 * (COMPLETE) arrives before the ST Execution Response has been processed.
	 * This happens in exec_path=1 (SLO) where the kernel fires the DONE event
	 * ahead of delivering the EXEC Response frame.  When set,
	 * smd_handle_transition_complete() is deferred and called from
	 * smd_handle_execute_response() once the response has been parsed.
	 */
	bool transition_complete_pending;
};

int smd_enabled(struct wpa_supplicant *wpa_s);
int smd_set_domain_id(struct wpa_supplicant *wpa_s, const u8 *domain_id);
const u8 *smd_get_domain_id(struct wpa_supplicant *wpa_s);

enum smd_state smd_get_state(struct wpa_supplicant *wpa_s);
const char *smd_state_txt(enum smd_state state);
void smd_set_state(struct wpa_supplicant *wpa_s, enum smd_state new_state);

int smd_verify_uhr_capability(struct wpa_supplicant *wpa_s, struct wpa_bss *bss);

int smd_needs_initial_association(struct wpa_supplicant *wpa_s, struct wpa_bss *bss);
int smd_establish_smd_me_association(struct wpa_supplicant *wpa_s, struct wpa_bss *bss,
				     struct wpa_ssid *ssid);
int smd_needs_bss_transition(struct wpa_supplicant *wpa_s, struct wpa_bss *bss);
int smd_prepare_bss_transition(struct wpa_supplicant *wpa_s, struct wpa_bss *bss);
int smd_should_suppress_connect(struct wpa_supplicant *wpa_s,
				struct wpa_bss *selected);

int smd_parse_rnr_for_neighbors(struct wpa_supplicant *wpa_s,
				struct wpa_bss *bss,
				struct smd_neighbor_target **targets,
				size_t *num_targets);

int smd_trigger_neighbor_discovery(struct wpa_supplicant *wpa_s,
				   struct smd_neighbor_target *targets,
				   size_t num_targets);

int smd_process_discovery_results(struct wpa_supplicant *wpa_s,
				  struct wpa_bss *bss);

void smd_neighbor_discovery_flow(struct wpa_supplicant *wpa_s,
				 struct wpa_bss *reporting_bss);

void smd_groups_init(struct wpa_supplicant *wpa_s);
void smd_groups_deinit(struct wpa_supplicant *wpa_s);
void smd_targets_deinit(struct wpa_supplicant *wpa_s);
struct smd_group *smd_group_find(struct wpa_supplicant *wpa_s,
				 const u8 *smd_id);
struct smd_group *smd_group_create(struct wpa_supplicant *wpa_s,
				   const u8 *smd_id);
int smd_group_add_member(struct wpa_supplicant *wpa_s,
			 struct smd_group *group,
			 struct wpa_bss *bss);
void smd_group_remove_bss(struct wpa_supplicant *wpa_s,
			  const u8 *bssid);

int smd_ctrl_iface_execute(struct wpa_supplicant *wpa_s, char *cmd,
			   char *buf, size_t buflen);

int wpas_smd_request_execute(struct wpa_supplicant *wpa_s,
			     const u8 *bssid,
			     u8 exec_path,
			     u8 dl_tid_bitmap);

const u8 *smd_get_current_domain(struct wpa_supplicant *wpa_s);

bool smd_detect_domain_transition(struct wpa_supplicant *wpa_s,
				  const u8 *target_smd_id);

int smd_validate_domain_transition(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *target_bss,
				   const u8 *target_smd_id);

void smd_begin_domain_transition(struct wpa_supplicant *wpa_s,
				 const u8 *target_smd_id);

void smd_complete_domain_transition(struct wpa_supplicant *wpa_s,
				    bool success);

struct neighbor_report;

int smd_btm_filter_candidate(struct wpa_supplicant *wpa_s,
			     struct wpa_bss *bss,
			     struct neighbor_report *neighbor);

void smd_btm_enhance_preference(struct wpa_supplicant *wpa_s,
				struct neighbor_report *neighbor,
				struct wpa_bss *bss);


int smd_process_rrm_neighbor_report(struct wpa_supplicant *wpa_s,
				    const u8 *bssid,
				    const u8 *subelems,
				    size_t subelems_len);

int smd_validate_ie_format(const u8 *ie, size_t ie_len);

int smd_validate_capabilities_reserved(u32 capabilities);

int smd_validate_security_policy(struct wpa_supplicant *wpa_s,
				 struct wpa_bss *bss);

int smd_ctrl_iface_domains(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);

int smd_ctrl_iface_groups(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);

int smd_ctrl_iface_status(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);

bool smd_is_bss_fresh(struct wpa_bss *bss);

int smd_validate_group_freshness(struct wpa_supplicant *wpa_s,
				 struct smd_group *group);

void smd_trigger_freshness_scan(struct wpa_supplicant *wpa_s,
				struct smd_group *group);

enum smd_discovery_completeness smd_get_discovery_completeness(struct wpa_bss *bss);

int smd_score_candidate_quality(struct wpa_bss *bss);

void smd_upgrade_rnr_to_full(struct wpa_supplicant *wpa_s, struct wpa_bss *bss);

struct wpa_bss *smd_select_best_candidate(struct wpa_supplicant *wpa_s,
					  struct smd_group *group);

/* UHR Reconfig Response handling */
void wpas_uhr_reconfig_resp(struct wpa_supplicant *wpa_s,
			    const struct uhr_reconfig_resp *resp);
void wpas_uhr_smd_handle_transition_status(struct wpa_supplicant *wpa_s,
					   const struct st_transition *info);

/**
 * wpas_smd_request_prepare - Request preparation for a target
 * @wpa_s: Pointer to wpa_supplicant
 * @bssid: Target BSSID
 * @no_dl_sn: Request DL SN not transferred
 * @no_ul_sn: Request UL SN not transferred
 * @scs_ids: Comma-separated list of SCS IDs (optional)
 * Returns: 0 on success, -1 on failure
 */
int wpas_smd_request_prepare(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     int no_dl_sn, int no_ul_sn, const char *scs_ids);

int smd_ctrl_iface_prepare(struct wpa_supplicant *wpa_s, char *cmd,
			   char *buf, size_t buflen);

int smd_ctrl_iface_list_prepared(struct wpa_supplicant *wpa_s, char *buf, size_t buflen);

int smd_ctrl_iface_cancel_prepare(struct wpa_supplicant *wpa_s, char *cmd,
				  char *buf, size_t buflen);

int wpas_smd_bss_transition(struct wpa_supplicant *wpa_s, const u8 *bssid,
			   u8 exec_path, char *buf, size_t buflen);

/**
 * wpas_smd_roam - Notify the driver of an SMD roam phase from supplicant.
 *
 * Thin wrapper around wpa_driver_ops::smd_roam that takes a supplicant
 * context and a peer MLD address.  Used to inform the kernel/firmware of
 * per-phase SMD roam progress (mirrors hostapd_smd_roam() on the AP side).
 */
int wpas_smd_roam(struct wpa_supplicant *wpa_s, const u8 *peer_mld_addr,
		  u32 role, u32 type,
		  bool dl_sn_not_transferred, bool ul_sn_not_transferred,
		  u32 dl_drain_time);
#endif /* SMD_H */
