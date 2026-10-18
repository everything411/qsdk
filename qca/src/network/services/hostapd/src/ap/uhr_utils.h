/*
 * hostapd / IEEE 802.11bn UHR
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef UHR_LINK_RECONFIG_H
#define UHR_LINK_RECONFIG_H

#include "common/wpa_common.h"
#include "ap/uhr_iap.h"

/* UHR Link Reconfig Type field values (NEW in V25) */
#define UHR_LINK_RECONFIG_TYPE_PREP    0  /* ST Prep */
#define UHR_LINK_RECONFIG_TYPE_EXECUTE 1  /* ST Execute */

/* DL Drain Duration default (in TU) (NEW in V25) */
/* Default: ~10 seconds = 9765 TU (1 TU = 1024 microseconds) */
#define UHR_DL_DRAIN_DURATION_TU_DEFAULT 9765

/* Maximum DL Drain Duration (in TU) (NEW in V25) */
#define UHR_DL_DRAIN_DURATION_TU_MAX 65535

/* Default Tx power indication value until the actual value is retrieved */
#define UHR_TX_PWR_IND_DEFAULT_FVAL 19

/* Extract IEs from frame
 * Frame format: MAC header + Category + Action + Dialog Token + Type + IEs
 * Skip to IEs: 24 (MAC) + 1 (Category) + 1 (Action) + 1 (Dialog) + 1 (Type) = 28 bytes
 */
#define WLAN_ST_PREP_MIN_LEN 28

enum uhr_smd_st_type {
	UHR_SMD_ST_PREP_REQ  = 0,
	UHR_SMD_ST_PREP_RESP = 1,
	UHR_SMD_ST_EXEC_REQ  = 2,
	UHR_SMD_ST_EXEC_RESP = 3,
};

#define UHR_ST_IAP_TIMEOUT_USEC 5000000

#define UHR_ST_GET_CTX_TIMEOUT_USEC 50000

/* UHR ST preparation timeout fallback when smd_timeout is not configured (5 seconds) */
#define UHR_ST_PREP_TIMEOUT_SEC 5

struct uhr_smd_bss_transition_element {
	u16 listen_interval;
	u16 dl_drain_time;
	bool dl_sn_not_transferred;
	bool ul_sn_not_transferred;
	u8 num_scs_ids;
};

/**
 * struct uhr_reconfig_mle - Parsed UHR Reconfiguration ML-IE
 */
struct uhr_reconfig_mle {
	u8 mld_mac_addr[ETH_ALEN];
	u8 target_ap_mld_addr[ETH_ALEN];
	bool has_target_ap_mld_addr;
	u8 current_link_id;
};

/* Forward declarations */
struct hostapd_data;
struct sta_info;

/* Forward declarations for handlers (defined in patches 03 and 04) */
void uhr_tgt_ap_handle_st_prep_req(struct hostapd_data *hapd,
			       const struct uhr_iap_frame *iap,
			       u16 frame_len);

void uhr_cur_ap_handle_st_prep_resp(struct hostapd_data *hapd,
				const struct uhr_iap_frame *iap,
				u16 frame_len);

/* EHT-style data structures for Per-STA Profile management */
struct uhr_link_reconf_req_list;
struct uhr_link_reconf_req_info;

/* Cleanup function (EHT naming pattern) */
void uhr_deinit_link_reconf_req(struct uhr_link_reconf_req_list **req_list_ptr);

#define MAX_NUM_MLD_LINKS 15

int uhr_cur_start_iap_msg_timer(struct sta_info *sta, const u8 *ap_mld_addr);
void uhr_cancel_iap_timeout(struct sta_info *sta, const u8 *ap_mld_addr);

/* ST Execute - Current AP Functions */
int uhr_handle_st_exec_req(struct hostapd_data *hapd,
                                 struct sta_info *sta,
				  const u8 *buf, size_t len, struct sta_smd_ctx_info *smd_ctx);
void uhr_handle_get_smd_ctx_done(struct hostapd_data *hapd,
				 const u8 *sta_addr,
				 struct sta_smd_ctx_info *ctx);

void uhr_cur_ap_handle_st_exec_resp(struct hostapd_data *hapd,
                                    const struct uhr_iap_frame *iap,
                                    u16 frame_len);

/* ST Execute - Target AP Functions */
void uhr_tgt_ap_handle_st_exec_req(struct hostapd_data *hapd,
                                const struct uhr_iap_frame *iap,
                                u16 frame_len);

/* ST Roam Cleanup - Target AP handler */
void uhr_tgt_ap_handle_st_roam_cleanup(struct hostapd_data *hapd,
					const struct uhr_iap_frame *iap);


/* ST Execute via Target AP */
int uhr_handle_st_exec_req_tgt(struct hostapd_data *hapd,
				struct sta_info *sta,
				const u8 *buf, size_t len);

/* ST Ctx - Current AP handler (receives CTX REQUEST from target AP) */
void uhr_cur_ap_handle_st_ctx_request(struct hostapd_data *hapd,
				      const struct uhr_iap_frame *iap,
				      u16 frame_len);

/* ST Ctx - Target AP handler (receives CTX RESPONSE from current AP) */
void uhr_tgt_ap_handle_st_ctx_response(struct hostapd_data *hapd,
					const struct uhr_iap_frame *iap,
					u16 frame_len);

/* Current AP handler for ST EXEC VIA TGT DONE notification */
void uhr_cur_ap_handle_st_exec_via_tgt_done(struct hostapd_data *hapd,
					     const struct uhr_iap_frame *iap,
					     u16 frame_len);


/* Current AP Functions */
int uhr_handle_st_prep_req(struct hostapd_data *hapd,
				   struct sta_info *sta,
				   const u8 *frame, size_t frame_len, struct sta_smd_ctx_info *smd_ctx);

int uhr_parse_smd_bss_trans_elem(const struct ieee802_11_elems *elems,
				 u8 type,
				 struct uhr_smd_bss_transition_element *sbte);

/* ST Prep Timer at Target AP */
void uhr_tgt_st_prep_timer_cleanup(void *eloop_ctx, void *timeout_ctx);
void uhr_tgt_start_st_prep_timer(struct hostapd_data *hapd,
                                const u8 *sta_addr);
void uhr_tgt_cancel_st_prep_timer(struct hostapd_data *hapd, const u8 *sta_addr);

/* Basic Multi-Link IE max buffer size:
 * Element ID (1) + Length (1) + Extension ID (1) + Multi-Link Control (2) +
 * Common Info Length (1) + MLD MAC Address (ETH_ALEN) + Link ID Info (1) +
 * BSS Parameters Change Count (1) + Medium Sync Delay Info (2) +
 * EML Capabilities (2) + MLD Capabilities (2) + AP MLD ID (1) +
 * Extended MLD Capabilities (2) + Enhanced Critical Update (1) +
 * Age of BSS Load (1) + Future Extensions (8)
 */
#define UHR_BMLIE_BUF_LEN  (1 + 1 + 1 + 2 + 1 + ETH_ALEN + 1 + 1 + 2 + 2 + 2 + 1 + 2 + 1 + 1 + 8)

size_t hostapd_uhr_eid_bmlie_from_rmlie(const struct wpabuf *mlbuf,
						u8 link_id,
						u8 *bmlie);
/* Function declarations */
int uhr_parse_reconfig_mle(const struct ieee802_11_elems *elems,
			   struct uhr_reconfig_mle *mle);


/* Timeout management functions */
int uhr_cur_start_st_prep_timer(struct sta_info *sta, const u8 *ap_mld_addr, u32 seconds);
void uhr_cancel_st_prep_timeout(struct sta_info *sta, const u8 *ap_mld_addr);

/* AP list management functions */
struct smd_roam_ap_info *uhr_find_ap_in_list(struct sta_info *sta, const u8 *ap_mld_addr);
int uhr_remove_ap_from_list(struct sta_info *sta, const u8 *ap_mld_addr);

/* Cleanup function for sta_info.c */
void uhr_cleanup_sta_roam_contexts(struct hostapd_data *hapd, struct sta_info *sta);

/* Target AP IAP receive handlers */
void uhr_tgt_ap_handle_st_prep_ctx(struct hostapd_data *hapd,
				    const struct uhr_iap_frame *iap);

void uhr_cur_get_ctx_timeout(void *eloop_ctx, void *timeout_ctx);

#endif /* UHR_LINK_RECONFIG_H */
