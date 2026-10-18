/*
 * hostapd / IEEE 802.11bn UHR
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef UHR_IAP_H
#define UHR_IAP_H

/* IAP message types */
#define UHR_IAP_MSG_ST_PREP_REQUEST  1
#define UHR_IAP_MSG_ST_PREP_RESPONSE 2

/* ST Execute IAP message types (NEW in V25) */
#define UHR_IAP_MSG_ST_EXEC_REQUEST  3
#define UHR_IAP_MSG_ST_EXEC_RESPONSE 4

/* ST Roam Cleanup - notify non-exec TAPs to free prepped STA state */
#define UHR_IAP_MSG_ST_ROAM_CLEANUP  5

#define UHR_IAP_MSG_ST_PREP_CTX      9  /* Current -> Target: deferred prep SMD context */

/* IAP MTU threshold: split smd_ctx into a separate message when
 * sizeof(iap_frame) + ota_frame_len + smd_ctx_len would exceed this. */
#define UHR_IAP_MTU_THRESHOLD     1400

#define UHR_IAP_MAX_FRAME_LEN 1500

/* ST Roam Cleanup - notify non-exec TAPs to free prepped STA state */
#define UHR_IAP_MSG_ST_ROAM_CLEANUP  5

/* ST Execute via Target AP-MLD IAP message types */
#define UHR_IAP_MSG_ST_EXEC_VIA_TGT_DONE  6  /* Target -> Current: transition complete */
#define UHR_IAP_MSG_ST_CTX_REQUEST        7  /* Target -> Current: fetch SMD context */
#define UHR_IAP_MSG_ST_CTX_RESPONSE       8  /* Current -> Target: deliver SMD context */

#define MAX_IE_LEN 60

/**
 * struct uhr_iap_security_ctx - Security context for ST preparation
 *
 * Includes PTK components (KCK, KEK, TK) for rekeying support
 */
struct uhr_iap_security_ctx {
       /* PMK and PMKID */
       u8 pmk_len;
       u8 pmk[PMK_LEN_MAX];
       u8 pmkid[PMKID_LEN];

       /* PTK Components for rekeying */
       u8 kck_len;
       u8 kck[WPA_KCK_MAX_LEN];
       u8 kek_len;
       u8 kek[WPA_KEK_MAX_LEN];
       u8 tk_len;
       u8 tk[WPA_TK_MAX_LEN];

       /* Cipher suite information */
       u8 akm[4];
       u8 cipher[4];

       u8 wpa_ie[MAX_IE_LEN];
       u8 rsnxe[MAX_IE_LEN];
       u8 wpa_ie_len;
       u8 rsnxe_len;

} __attribute__((packed));

/**
 * struct uhr_iap_frame - Unified IAP frame structure
 */
struct uhr_iap_frame {
       u8 msg_type;
       u8 iap_transaction_id;
       u64 sequence_number;

       u8 current_ap_mld_addr[ETH_ALEN];
       u8 target_ap_mld_addr[ETH_ALEN];
       u8 sta_addr[ETH_ALEN];

       u8 current_link_id;
       u8 flags;
       u8 status_code;

       struct uhr_iap_security_ctx sec_ctx;

       __le16 frame_len;
	__le16 smd_ctx_len;
	u8  frame_ctx_data[];
} __attribute__((packed));

/* Flags for uhr_iap_frame */
#define UHR_IAP_FLAG_HAS_SEC_CTX      0x01
#define UHR_IAP_FLAG_HAS_DYNAMIC_CTX  0x02
#define UHR_IAP_FLAG_STA_4ADDR        0x04

/* IAP status codes */
#define UHR_IAP_STATUS_SUCCESS           0
#define UHR_IAP_STATUS_FAILURE           1
#define UHR_IAP_STATUS_INVALID_PARAMS    2
#define UHR_IAP_STATUS_NO_RESOURCES      3
#define UHR_IAP_STATUS_TIMEOUT           4

/* IAP Protocol Functions */
int uhr_iap_send_st_prep_req(struct hostapd_data *hapd,
			 const u8 *target_ap_mld_addr,
			 struct sta_info *sta,
			 const u8 *frame, size_t frame_len);

int uhr_iap_send_st_prep_resp(struct hostapd_data *hapd,
			  const u8 *current_ap_mld_addr,
			  const u8 *sta_addr,
			  u8 iap_transaction_id,
			  u64 sequence_number,
			  u8 status_code, u8 current_link_id,
			  const u8 *frame, size_t frame_len);

int uhr_iap_send_st_exec_req(struct hostapd_data *hapd,
                                struct sta_info *sta,
                                const u8 *target_ap_mld_addr, const u8 *frame, size_t frame_len);

int uhr_iap_send_st_exec_resp(struct hostapd_data *hapd,
                              const u8 *current_ap_mld_addr,
                              const u8 *sta_addr,
                              u8 iap_transaction_id,
                              u64 sequence_number,
                              u8 status_code,
			      u8 current_link_id,
                              const u8 *frame, size_t frame_len);

int uhr_iap_send_st_roam_cleanup(struct hostapd_data *hapd,
				  const u8 *target_ap_mld_addr,
				  const u8 *sta_mld_addr);


void uhr_iap_rx(struct hostapd_data *hapd, const u8 *src_addr, const u8 *dst_addr,
		u16 msg_type, const u8 *data, size_t data_len);

int uhr_iap_send_st_prep_ctx(struct hostapd_data *hapd,
			      const u8 *target_ap_mld_addr,
			      const u8 *sta_addr,
			      const struct sta_smd_ctx_info *smd_ctx,
			      size_t smd_ctx_len);

/* ST Execute via Target AP-MLD IAP send functions */
int uhr_iap_send_st_ctx_request(struct hostapd_data *hapd,
				const u8 *current_ap_mld_addr,
				const u8 *sta_addr,
				u8 iap_transaction_id);

int uhr_iap_send_st_ctx_response(struct hostapd_data *hapd,
				 const u8 *target_ap_mld_addr,
				 const u8 *sta_addr,
				 u8 iap_transaction_id,
				 u8 status,
				 const struct sta_smd_ctx_info *smd_ctx);

int uhr_iap_send_st_exec_via_tgt_done(struct hostapd_data *hapd,
				      const u8 *current_ap_mld_addr,
				      const u8 *sta_addr,
				      u8 iap_transaction_id);

#endif /* UHR_IAP_H */
