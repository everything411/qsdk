/*
 * hostapd / IEEE 802.11bn UHR
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "hostapd.h"
#include "sta_info.h"
#include "wpa_auth.h"
#include "wpa_auth_i.h"
#include "uhr_utils.h"
#include "eth_p_1905.h"
#include "eth_p_1905_iap.h"
#include "uhr_neighbor_update.h"


/* Global IAP transaction ID counter */
static u8 g_iap_transaction_id = 0;

/* Global sequence number for replay protection */
static u64 g_iap_sequence_number = 0;


/**
 * uhr_extract_security_ctx - Extract security context from STA
 * @hapd: hostapd data
 * @sta: Station info
 * @sec_ctx: Output security context
 * Returns: 0 on success, -1 on error
 *
 * Extracts PMK, PMKID, and PTK components from the station's
 * WPA state machine for transfer to target AP.
 */
static int uhr_extract_security_ctx(struct hostapd_data *hapd,
				    struct sta_info *sta,
				    struct uhr_iap_security_ctx *sec_ctx)
{
	struct wpa_state_machine *sm = sta->wpa_sm;
	
	if (!sm) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: No WPA state machine for STA " MACSTR,
			   MAC2STR(sta->addr));
		return -1;
	}
	
	os_memset(sec_ctx, 0, sizeof(*sec_ctx));
	
	/* Extract PMK */
	if (sm->pmk_len > 0 && sm->pmk_len <= PMK_LEN_MAX) {
		sec_ctx->pmk_len = sm->pmk_len;
		os_memcpy(sec_ctx->pmk, sm->PMK, sm->pmk_len);
	} else {
		wpa_printf(MSG_ERROR, "SMD IAP: Invalid PMK length %u",
			   sm->pmk_len);
		return -1;
	}
	
	/* Extract PMKID */
	if (sm->pmkid_set)
		os_memcpy(sec_ctx->pmkid, sm->pmkid, PMKID_LEN);
	
	/* Extract PTK components for rekeying support */
	if (sm->PTK_valid) {
		/* KCK */
		sec_ctx->kck_len = sm->PTK.kck_len;
		if (sec_ctx->kck_len > 0 && sec_ctx->kck_len <= WPA_KCK_MAX_LEN)
			os_memcpy(sec_ctx->kck, sm->PTK.kck, sec_ctx->kck_len);

		/* KEK */
		sec_ctx->kek_len = sm->PTK.kek_len;
		if (sec_ctx->kek_len > 0 && sec_ctx->kek_len <= WPA_KEK_MAX_LEN)
			os_memcpy(sec_ctx->kek, sm->PTK.kek, sec_ctx->kek_len);

		/* TK */
		sec_ctx->tk_len = sm->PTK.tk_len;
		if (sec_ctx->tk_len > 0 && sec_ctx->tk_len <= WPA_TK_MAX_LEN)
			os_memcpy(sec_ctx->tk, sm->PTK.tk, sec_ctx->tk_len);
	}
	
	/* Extract cipher suite information */
	WPA_PUT_BE32(sec_ctx->akm, sm->wpa_key_mgmt);
	WPA_PUT_BE32(sec_ctx->cipher, sm->pairwise);

	if (sm->wpa_ie) {
		os_memcpy(sec_ctx->wpa_ie, sm->wpa_ie, sm->wpa_ie_len);
		sec_ctx->wpa_ie_len = sm->wpa_ie_len;
	}
	if (sm->rsnxe) {
		os_memcpy(sec_ctx->rsnxe, sm->rsnxe, sm->rsnxe_len);
		sec_ctx->rsnxe_len = sm->rsnxe_len;
	}


	
	wpa_printf(MSG_DEBUG,
		   "SMD IAP: Security context extraction complete");
	return 0;
}


int uhr_iap_send_st_prep_req(struct hostapd_data *hapd,
			 const u8 *target_ap_mld_addr,
			 struct sta_info *sta,
			 const u8 *frame, size_t frame_len)
{
	 struct sta_smd_ctx_info *smd_ctx;
	 struct smd_roam_ap_info *ap_info;
	 size_t iap_len = 0, smd_ctx_len = 0, orig_smd_ctx_len;
	 bool split_ctx = false;
	struct uhr_iap_frame *iap;
	u8 *buf, *pos;
	int ret;

	wpa_printf(MSG_INFO, "SMD IAP: Received UHR Link Reconfig Requst of frame length: %zu", frame_len);
	
	if (!hapd || !target_ap_mld_addr || !sta || !frame ||
	    frame_len == 0) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Invalid parameters for send_request");
		return -1;
	}
	
	/* FIX: Buffer overflow protection */
	if (frame_len > UHR_IAP_MAX_FRAME_LEN) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Frame too large (%zu > %d)",
			   frame_len, UHR_IAP_MAX_FRAME_LEN);
		return -1;
	}
	
	/* FIX: Peer validation before send */
	if (!eth_p_1905_peer_exists(hapd->eth_p_1905_ctx, target_ap_mld_addr)) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Target AP " MACSTR " not in peer list",
			   MAC2STR(target_ap_mld_addr));
		return -1;
	}

	ap_info = uhr_find_ap_in_list(sta, target_ap_mld_addr);
	if (!ap_info)
		return -1;

	smd_ctx = ap_info->smd_ctx;
	if (ap_info->smd_ctx_valid && smd_ctx)
		smd_ctx_len = sizeof(*smd_ctx) + smd_ctx->vendor_ctx_len;

	/* When the combined frame (fixed header + OTA frame + smd_ctx) would
	 * exceed UHR_IAP_MTU_THRESHOLD, omit smd_ctx here and send it in a
	 * separate UHR_IAP_MSG_ST_PREP_CTX message after this one succeeds.
	 * The security context (iap->sec_ctx) is always included unchanged. */
	orig_smd_ctx_len = smd_ctx_len;
	if (smd_ctx_len > 0 &&
	    sizeof(*iap) + frame_len + smd_ctx_len > UHR_IAP_MTU_THRESHOLD) {
		wpa_printf(MSG_DEBUG,
			   "IAP: ST PREP total %zu > threshold %d, deferring smd_ctx",
			   sizeof(*iap) + frame_len + smd_ctx_len,
			   UHR_IAP_MTU_THRESHOLD);
		split_ctx = true;
		smd_ctx_len = 0;
	}

	/* Allocate buffer for IAP frame */
	iap_len = sizeof(*iap) + frame_len + smd_ctx_len;
	buf = os_zalloc(iap_len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "SMD IAP: Failed to allocate IAP frame len = %zu", iap_len);
		return -1;
	}
	
	iap = (struct uhr_iap_frame *) buf;
	
	/* Fill IAP header - FIX: Use per-context counters */
	iap->msg_type = UHR_IAP_MSG_ST_PREP_REQUEST;
	iap->iap_transaction_id = g_iap_transaction_id++;
	iap->sequence_number = htole64(g_iap_sequence_number++);
	
	/* Fill addresses */
	os_memcpy(iap->current_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(iap->target_ap_mld_addr, target_ap_mld_addr, ETH_ALEN);
	os_memcpy(iap->sta_addr, sta->addr, ETH_ALEN);
	
	/* Fill link IDs */
	iap->current_link_id = hapd->mld_link_id; /* TODO: Get from hapd */
	
	/* Set flags and status */
	iap->flags = UHR_IAP_FLAG_HAS_SEC_CTX;
	/* Signal pre-existing 4addr intent to the target so it can create
	 * and program AP_VLAN in ST_PREP instead of waiting for reactive flow. */
	if ((sta->flags & WLAN_STA_WDS) || sta->pending_wds_enable)
		iap->flags |= UHR_IAP_FLAG_STA_4ADDR;
	iap->status_code = 0;
	
	/* Extract and fill security context */
	if (uhr_extract_security_ctx(hapd, sta, &iap->sec_ctx) < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Failed to extract security context");
		os_free(buf);
		return -1;
	}
	
	/* Copy frame buffer */
	iap->frame_len = htole16(frame_len);
	os_memcpy(iap->frame_ctx_data, frame, frame_len);

	iap->smd_ctx_len = htole16(smd_ctx_len);
	if (smd_ctx_len) {
		iap->flags |= UHR_IAP_FLAG_HAS_DYNAMIC_CTX;
		pos = iap->frame_ctx_data + frame_len;
		os_memcpy(pos, smd_ctx, smd_ctx_len);
		wpa_printf(MSG_DEBUG,
			   "SMD IAP: Including Prep SMD context (%zu bytes) for " MACSTR,
			   smd_ctx_len, MAC2STR(target_ap_mld_addr));
	}

	wpa_printf(MSG_DEBUG,
		   "SMD IAP: Sending REQUEST to " MACSTR " (txn=%u, frame_len=%zu)",
		   MAC2STR(target_ap_mld_addr), iap->iap_transaction_id,
		   frame_len);
	
	/* TLV-encode and send via IEEE 1905.1 (ETH_P_1905_SMD_ST_PREP_REQ_MSG) */
	struct wpabuf *tbuf = eth_p_1905_iap_encode_prep_req(hapd, iap);
	if (!tbuf) {
		wpa_printf(MSG_ERROR, "SMD IAP: Failed to encode ST PREP REQ TLV");
		os_free(buf);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_PREP_REQ_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, target_ap_mld_addr,
                           hapd->mld->mld_addr,
			   ETH_P_1905_SMD_ST_PREP_REQ_MSG,
			   wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);

	os_free(buf);
	
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD IAP: Failed to send REQUEST");
		return -1;
	}

	/* type-9 drop (e.g. peer gone or arrival before type-1) is safe:
	 * the ST Exec IAP unconditionally re-delivers smd_ctx as a fallback. */
	if (split_ctx) {
		if (uhr_iap_send_st_prep_ctx(hapd, target_ap_mld_addr, sta->addr,
					     smd_ctx, orig_smd_ctx_len) < 0)
			wpa_printf(MSG_WARNING,
				   "IAP: ST PREP CTX send failed for " MACSTR
				   "; exec IAP will carry ctx as fallback",
				   MAC2STR(target_ap_mld_addr));
	}

	ap_info->state = SMD_AP_STATE_ST_PREP_IAP_PENDING;
	wpa_printf(MSG_DEBUG, "SMD IAP: REQUEST sent successfully");
	return 0;
}


int uhr_iap_send_st_prep_ctx(struct hostapd_data *hapd,
			      const u8 *target_ap_mld_addr,
			      const u8 *sta_addr,
			      const struct sta_smd_ctx_info *smd_ctx,
			      size_t smd_ctx_len)
{
	struct uhr_iap_frame *iap;
	size_t iap_len;
	int ret;

	if (!hapd || !target_ap_mld_addr || !sta_addr || !smd_ctx || smd_ctx_len == 0)
		return -1;

	if (!eth_p_1905_peer_exists(hapd->eth_p_1905_ctx, target_ap_mld_addr)) {
		wpa_printf(MSG_ERROR,
			   "IAP: ST PREP CTX: Target AP " MACSTR " not in peer list",
			   MAC2STR(target_ap_mld_addr));
		return -1;
	}

	iap_len = sizeof(*iap) + smd_ctx_len;
	iap = os_zalloc(iap_len);
	if (!iap)
		return -1;

	iap->msg_type = UHR_IAP_MSG_ST_PREP_CTX;
	iap->iap_transaction_id = g_iap_transaction_id++;
	iap->sequence_number = htole64(g_iap_sequence_number++);

	os_memcpy(iap->current_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(iap->target_ap_mld_addr, target_ap_mld_addr, ETH_ALEN);
	os_memcpy(iap->sta_addr, sta_addr, ETH_ALEN);

	iap->flags = UHR_IAP_FLAG_HAS_DYNAMIC_CTX;
	iap->frame_len = 0;
	iap->smd_ctx_len = htole16((u16) smd_ctx_len);
	os_memcpy(iap->frame_ctx_data, smd_ctx, smd_ctx_len);

	wpa_printf(MSG_DEBUG,
		   "IAP: Sending ST PREP CTX to " MACSTR " (%zu bytes)",
		   MAC2STR(target_ap_mld_addr), smd_ctx_len);

	/* TLV-encode and send via IEEE 1905.1 (ETH_P_1905_SMD_ST_PREP_CTX_MSG) */
	struct wpabuf *tbuf = eth_p_1905_iap_encode_prep_ctx(iap);
	if (!tbuf) {
		wpa_printf(MSG_ERROR, "IAP: Failed to encode ST PREP CTX TLV");
		os_free(iap);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_PREP_CTX_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, target_ap_mld_addr,
			   hapd->mld->mld_addr,
			   ETH_P_1905_SMD_ST_PREP_CTX_MSG,
			   wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);
	os_free(iap);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "IAP: Failed to send ST PREP CTX");
		return -1;
	}
	return 0;
}


int uhr_iap_send_st_prep_resp(struct hostapd_data *hapd,
			  const u8 *current_ap_mld_addr,
			  const u8 *sta_addr,
			  u8 iap_transaction_id,
			  u64 sequence_number,
			  u8 status_code, u8 current_link_id,
			  const u8 *frame, size_t frame_len)
{
	struct uhr_iap_frame *iap;
	size_t iap_len;
	u8 *buf;
	int ret;
	
	if (!hapd || !current_ap_mld_addr || !sta_addr) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Invalid parameters for send_response");
		return -1;
	}
	
	/* FIX: Buffer overflow protection */
	if (frame_len > UHR_IAP_MAX_FRAME_LEN) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Frame too large (%zu > %d)",
			   frame_len, UHR_IAP_MAX_FRAME_LEN);
		return -1;
	}
	
	/* FIX: Peer validation before send */
	if (!eth_p_1905_peer_exists(hapd->eth_p_1905_ctx, current_ap_mld_addr)) {
		wpa_printf(MSG_ERROR,
			   "SMD IAP: Current AP " MACSTR " not in peer list",
			   MAC2STR(current_ap_mld_addr));
		return -1;
	}
	
	/* Allocate buffer for IAP frame */
	iap_len = sizeof(*iap) + frame_len;
	buf = os_zalloc(iap_len);
	if (!buf) {
		wpa_printf(MSG_ERROR, "SMD IAP: Failed to allocate IAP frame");
		return -1;
	}
	
	iap = (struct uhr_iap_frame *) buf;
	
	/* Fill IAP header */
	iap->msg_type = UHR_IAP_MSG_ST_PREP_RESPONSE;
	iap->iap_transaction_id = iap_transaction_id;
	iap->sequence_number = htole64(sequence_number);
	
	/* Fill addresses */
	os_memcpy(iap->current_ap_mld_addr, current_ap_mld_addr, ETH_ALEN);
	os_memcpy(iap->target_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(iap->sta_addr, sta_addr, ETH_ALEN);
	
	/* Fill link IDs */
	iap->current_link_id = current_link_id;;
	
	/* Set flags and status */
	iap->flags = 0; /* No security context in response */
	iap->status_code = status_code;
	
	/* Security context unused in response */
	os_memset(&iap->sec_ctx, 0, sizeof(iap->sec_ctx));
	
	/* Copy frame buffer if present */
	if (frame && frame_len > 0) {
		iap->frame_len = htole16(frame_len);
		os_memcpy(iap->frame_ctx_data, frame, frame_len);
	} else {
		iap->frame_len = 0;
	}
	
	wpa_printf(MSG_DEBUG,
		   "SMD IAP: Sending RESPONSE to " MACSTR " (txn=%u, status=%u, frame_len=%zu)",
		   MAC2STR(current_ap_mld_addr), iap_transaction_id,
		   status_code, frame_len);
	
	/* TLV-encode and send via IEEE 1905.1 (ETH_P_1905_SMD_ST_PREP_REP_MSG) */
	struct wpabuf *tbuf = eth_p_1905_iap_encode_prep_resp(iap);
	if (!tbuf) {
		wpa_printf(MSG_ERROR, "SMD IAP: Failed to encode ST PREP RESP TLV");
		os_free(buf);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_PREP_REP_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, current_ap_mld_addr,
                           hapd->mld->mld_addr,
			   ETH_P_1905_SMD_ST_PREP_REP_MSG,
			   wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);

	os_free(buf);
	
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD IAP: Failed to send RESPONSE");
		return -1;
	}
	
	wpa_printf(MSG_DEBUG, "SMD IAP: RESPONSE sent successfully");
	return 0;
}

int uhr_iap_send_st_exec_req(struct hostapd_data *hapd,
                                struct sta_info *sta,
                                const u8 *target_ap_mld_addr, const u8 *frame, size_t frame_len)
{
       struct smd_roam_ap_info *target_info;
	struct sta_smd_ctx_info *smd_ctx;
	size_t iap_len, smd_ctx_len = 0;
       struct uhr_iap_frame *iap;
	u8 *buf, *pos;
       int ret;

       /* Find Target AP */
       target_info = uhr_find_ap_in_list(sta, target_ap_mld_addr);
       if (!target_info) {
	       wpa_printf(MSG_ERROR, "IAP: ST EXEC REQ: Target Info NULL");
               return -1;
	}

	if (!eth_p_1905_peer_exists(hapd->eth_p_1905_ctx, target_ap_mld_addr)) {
		wpa_printf(MSG_ERROR,
			   "UHR IAP: ST EXEC REQ: Target AP " MACSTR " not in peer list",
			   MAC2STR(target_ap_mld_addr));
		return -1;
	}

	if (frame_len > UHR_IAP_MAX_FRAME_LEN) {
		wpa_printf(MSG_ERROR,
			   "UHR IAP: ST EXEC REQ: Frame too large (%zu > %d)",
			   frame_len, UHR_IAP_MAX_FRAME_LEN);
		return -1;
	}

	smd_ctx = target_info->smd_ctx;
	if (target_info->smd_ctx_valid && smd_ctx)
		smd_ctx_len = sizeof(*smd_ctx) + smd_ctx->vendor_ctx_len;

       /* Allocate IAP frame (minimal - no security context needed) */
	iap_len = sizeof(*iap) + frame_len + smd_ctx_len;
       buf = os_zalloc(iap_len);
       if (!buf) {
	       wpa_printf(MSG_ERROR, "IAP: IAP Frame alloc failed ");
               return -1;
	}

       iap = (struct uhr_iap_frame *) buf;

       /* Fill IAP header */
       iap->msg_type = UHR_IAP_MSG_ST_EXEC_REQUEST;
       iap->iap_transaction_id = g_iap_transaction_id++;
       iap->sequence_number = htole64(g_iap_sequence_number++);

       /* Fill addresses */
       os_memcpy(iap->current_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
       os_memcpy(iap->target_ap_mld_addr, target_ap_mld_addr, ETH_ALEN);
       os_memcpy(iap->sta_addr, sta->addr, ETH_ALEN);

       /* No security context needed (already sent in ST Prep) */
       iap->flags = 0;
       iap->status_code = 0;
       iap->current_link_id = hapd->mld_link_id;

       iap->frame_len = htole16(frame_len);
	os_memcpy(iap->frame_ctx_data, frame, frame_len);

	iap->smd_ctx_len = htole16(smd_ctx_len);
	if (smd_ctx_len) {
		iap->flags |= UHR_IAP_FLAG_HAS_DYNAMIC_CTX;
		pos = iap->frame_ctx_data + frame_len;
		os_memcpy(pos, smd_ctx, smd_ctx_len);
		wpa_printf(MSG_DEBUG,
			   "SMD IAP: Including Exec SMD context (%zu bytes) for " MACSTR,
			   smd_ctx_len, MAC2STR(target_ap_mld_addr));
	}

       wpa_printf(MSG_DEBUG,
                  "UHR IAP: Sending ST EXEC REQUEST to " MACSTR " (txn=%u)",
                  MAC2STR(target_ap_mld_addr), iap->iap_transaction_id);

       /* TLV-encode and send via IEEE 1905.1 (ETH_P_1905_SMD_ST_EXEC_REQ_MSG) */
       struct wpabuf *tbuf = eth_p_1905_iap_encode_exec_req(hapd, iap);
       if (!tbuf) {
               wpa_printf(MSG_ERROR, "SMD IAP: Failed to encode ST EXEC REQ TLV");
               os_free(buf);
               return -1;
       }
       wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
                  ETH_P_1905_SMD_ST_EXEC_REQ_MSG, wpabuf_len(tbuf));
       ret = eth_p_1905_send(hapd->eth_p_1905_ctx, target_ap_mld_addr,
			  hapd->mld->mld_addr,
                          ETH_P_1905_SMD_ST_EXEC_REQ_MSG,
                          wpabuf_head(tbuf), wpabuf_len(tbuf));
       wpabuf_free(tbuf);

       os_free(buf);

       if (ret < 0) {
               wpa_printf(MSG_ERROR, "UHR IAP: Failed to send ST EXEC REQUEST");
               return -1;
       }

       return 0;
}


int uhr_iap_send_st_exec_resp(struct hostapd_data *hapd,
                              const u8 *current_ap_mld_addr,
                              const u8 *sta_addr,
                              u8 iap_transaction_id,
                              u64 sequence_number,
                              u8 status_code,
			      u8 current_link_id,
                              const u8 *frame, size_t frame_len)
{
       struct uhr_iap_frame *iap;
       size_t iap_len;
       u8 *buf;
       int ret;

       if (!hapd || !current_ap_mld_addr || !sta_addr) {
               wpa_printf(MSG_ERROR,
                          "SMD IAP: Invalid parameters for send_exec_response");
               return -1;
       }

	if (!eth_p_1905_peer_exists(hapd->eth_p_1905_ctx, current_ap_mld_addr)) {
		wpa_printf(MSG_ERROR,
			   "UHR IAP: ST EXEC RESP: Current AP " MACSTR " not in peer list",
			   MAC2STR(current_ap_mld_addr));
		return -1;
	}

	if (frame_len > UHR_IAP_MAX_FRAME_LEN) {
		wpa_printf(MSG_ERROR,
			   "UHR IAP: ST EXEC RESP: Frame too large (%zu > %d)",
			   frame_len, UHR_IAP_MAX_FRAME_LEN);
		return -1;
	}

       /* Allocate buffer for IAP frame */
       iap_len = sizeof(*iap) + frame_len;
       buf = os_zalloc(iap_len);
       if (!buf)
               return -1;

       iap = (struct uhr_iap_frame *) buf;

       /* Fill IAP header */
       iap->msg_type = UHR_IAP_MSG_ST_EXEC_RESPONSE;
       iap->iap_transaction_id = iap_transaction_id;
       iap->sequence_number = htole64(sequence_number);

       /* Fill addresses */
       os_memcpy(iap->current_ap_mld_addr, current_ap_mld_addr, ETH_ALEN);
       os_memcpy(iap->target_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
       os_memcpy(iap->sta_addr, sta_addr, ETH_ALEN);
	
       /* Fill link IDs */
	iap->current_link_id = current_link_id;;

       /* Set status and frame */
       iap->flags = 0;
       iap->status_code = status_code;
       iap->frame_len = htole16(frame_len);
       if (frame && frame_len > 0)
		os_memcpy(iap->frame_ctx_data, frame, frame_len);

       wpa_printf(MSG_DEBUG,
                  "UHR IAP: Sending ST EXEC RESPONSE to " MACSTR " (txn=%u, status=%u, frame_len=%zu)",
                  MAC2STR(current_ap_mld_addr), iap_transaction_id,
                  status_code, frame_len);

       /* TLV-encode and send via IEEE 1905.1 (ETH_P_1905_SMD_ST_EXEC_REP_MSG) */
       struct wpabuf *tbuf = eth_p_1905_iap_encode_exec_resp(hapd, iap);
       if (!tbuf) {
               wpa_printf(MSG_ERROR, "SMD IAP: Failed to encode ST EXEC RESP TLV");
               os_free(buf);
               return -1;
       }
       wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
                  ETH_P_1905_SMD_ST_EXEC_REP_MSG, wpabuf_len(tbuf));
       ret = eth_p_1905_send(hapd->eth_p_1905_ctx, current_ap_mld_addr,
			  hapd->mld->mld_addr,
                          ETH_P_1905_SMD_ST_EXEC_REP_MSG,
                          wpabuf_head(tbuf), wpabuf_len(tbuf));
       wpabuf_free(tbuf);

       os_free(buf);

       if (ret < 0) {
               wpa_printf(MSG_ERROR, "UHR IAP: Failed to send ST EXEC RESPONSE");
               return -1;
       }

       wpa_printf(MSG_DEBUG, "UHR IAP: ST EXEC RESPONSE sent successfully");
       return 0;
}


int uhr_iap_send_st_roam_cleanup(struct hostapd_data *hapd,
				  const u8 *target_ap_mld_addr,
				  const u8 *sta_mld_addr)
{
	struct uhr_iap_frame *iap;
	size_t iap_len;
	u8 *buf;
	int ret;

	if (!hapd || !target_ap_mld_addr || !sta_mld_addr)
		return -1;

	if (!eth_p_1905_peer_exists(hapd->eth_p_1905_ctx, target_ap_mld_addr)) {
		wpa_printf(MSG_DEBUG,
			   "UHR IAP: ST ROAM CLEANUP: Target AP " MACSTR " not in peer list",
			   MAC2STR(target_ap_mld_addr));
		return -1;
	}

	iap_len = sizeof(*iap);
	buf = os_zalloc(iap_len);
	if (!buf)
		return -1;

	iap = (struct uhr_iap_frame *)buf;
	iap->msg_type = UHR_IAP_MSG_ST_ROAM_CLEANUP;
	iap->iap_transaction_id = g_iap_transaction_id++;
	iap->sequence_number = htole64(g_iap_sequence_number++);

	os_memcpy(iap->current_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(iap->target_ap_mld_addr, target_ap_mld_addr, ETH_ALEN);
	os_memcpy(iap->sta_addr, sta_mld_addr, ETH_ALEN);

	iap->flags = 0;
	iap->status_code = 0;
	iap->frame_len = 0;
	iap->smd_ctx_len = 0;

	wpa_printf(MSG_DEBUG,
		   "UHR IAP: Sending ST ROAM CLEANUP to " MACSTR " for STA " MACSTR,
		   MAC2STR(target_ap_mld_addr), MAC2STR(sta_mld_addr));

	/* TLV-encode and send via IEEE 1905.1 (ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG) */
	struct wpabuf *tbuf = eth_p_1905_iap_encode_roam_cleanup(iap);
	if (!tbuf) {
		wpa_printf(MSG_ERROR, "UHR IAP: Failed to encode ST ROAM CLEANUP TLV");
		os_free(buf);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, target_ap_mld_addr,
			   hapd->mld->mld_addr,
			   ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG,
			   wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);
	os_free(buf);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "UHR IAP: Failed to send ST ROAM CLEANUP");
		return -1;
	}

	return 0;
}


/**
 * uhr_iap_send_st_ctx_request - Send ST_CTX_REQUEST from Target to Current AP
 * Target AP sends this to ask the Current AP for the STA's SMD context.
 */
int uhr_iap_send_st_ctx_request(struct hostapd_data *hapd,
				const u8 *current_ap_mld_addr,
				const u8 *sta_addr,
				u8 iap_transaction_id)
{
	struct uhr_iap_frame *iap;
	size_t iap_len = sizeof(*iap);
	struct wpabuf *tbuf;
	int ret;

	iap = os_zalloc(iap_len);
	if (!iap)
		return -1;

	iap->msg_type = UHR_IAP_MSG_ST_CTX_REQUEST;
	iap->iap_transaction_id = iap_transaction_id;
	os_memcpy(iap->current_ap_mld_addr, current_ap_mld_addr, ETH_ALEN);
	os_memcpy(iap->target_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(iap->sta_addr, sta_addr, ETH_ALEN);

	tbuf = eth_p_1905_iap_encode_ctx_req(iap);
	os_free(iap);
	if (!tbuf) {
		wpa_printf(MSG_ERROR, "UHR IAP: Failed to encode ST CTX REQUEST");
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_CTX_REQ_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, current_ap_mld_addr,
			      hapd->mld->mld_addr,
			      ETH_P_1905_SMD_ST_CTX_REQ_MSG,
			      wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "UHR IAP: Failed to send ST CTX REQUEST");
		return -1;
	}
	return 0;
}


/**
 * uhr_iap_send_st_ctx_response - Send ST_CTX_RESPONSE from Current to Target AP
 * Current AP delivers the STA's SMD context to the Target AP.
 */
int uhr_iap_send_st_ctx_response(struct hostapd_data *hapd,
				 const u8 *target_ap_mld_addr,
				 const u8 *sta_addr,
				 u8 iap_transaction_id,
				 u8 status,
				 const struct sta_smd_ctx_info *smd_ctx)
{
	size_t ctx_len = smd_ctx ? sizeof(*smd_ctx) + smd_ctx->vendor_ctx_len: 0;
	size_t total = sizeof(struct uhr_iap_frame) + ctx_len;
	struct uhr_iap_frame *frame;
	struct wpabuf *tbuf;
	int ret;

	frame = os_zalloc(total);
	if (!frame)
		return -1;

	frame->msg_type = UHR_IAP_MSG_ST_CTX_RESPONSE;
	frame->iap_transaction_id = iap_transaction_id;
	os_memcpy(frame->current_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(frame->target_ap_mld_addr, target_ap_mld_addr, ETH_ALEN);
	os_memcpy(frame->sta_addr, sta_addr, ETH_ALEN);
	frame->status_code = status;

	if (smd_ctx) {
		frame->flags |= UHR_IAP_FLAG_HAS_DYNAMIC_CTX;
		frame->smd_ctx_len = (u16) ctx_len;
		os_memcpy(frame->frame_ctx_data, smd_ctx, ctx_len);
	}

	tbuf = eth_p_1905_iap_encode_ctx_resp(frame);
	os_free(frame);
	if (!tbuf) {
		wpa_printf(MSG_ERROR, "UHR IAP: Failed to encode ST CTX RESPONSE");
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_CTX_REP_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, target_ap_mld_addr,
			      hapd->mld->mld_addr,
			      ETH_P_1905_SMD_ST_CTX_REP_MSG,
			      wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "UHR IAP: Failed to send ST CTX RESPONSE");
		return -1;
	}
	return 0;
}

/**
 * uhr_iap_send_st_exec_via_tgt_done - Notify Current AP that via-target exec is done
 * Target AP sends this after successfully completing ST Execute for the STA.
 */
int uhr_iap_send_st_exec_via_tgt_done(struct hostapd_data *hapd,
				      const u8 *current_ap_mld_addr,
				      const u8 *sta_addr,
				      u8 iap_transaction_id)
{
	struct uhr_iap_frame *iap;
	size_t iap_len = sizeof(*iap);
	struct wpabuf *tbuf;
	int ret;

	iap = os_zalloc(iap_len);
	if (!iap)
		return -1;

	iap->msg_type = UHR_IAP_MSG_ST_EXEC_VIA_TGT_DONE;
	iap->iap_transaction_id = iap_transaction_id;
	os_memcpy(iap->current_ap_mld_addr, current_ap_mld_addr, ETH_ALEN);
	os_memcpy(iap->target_ap_mld_addr, hapd->mld->mld_addr, ETH_ALEN);
	os_memcpy(iap->sta_addr, sta_addr, ETH_ALEN);

	tbuf = eth_p_1905_iap_encode_exec_via_tgt_done(iap);
	os_free(iap);
	if (!tbuf) {
		wpa_printf(MSG_ERROR,
			   "UHR IAP: Failed to encode ST EXEC VIA TGT DONE");
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV ENCAP 0x%04x (%zu B)",
		   ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG, wpabuf_len(tbuf));
	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, current_ap_mld_addr,
			      hapd->mld->mld_addr,
			      ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG,
			      wpabuf_head(tbuf), wpabuf_len(tbuf));
	wpabuf_free(tbuf);

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR IAP: Failed to send ST EXEC VIA TGT DONE");
		return -1;
	}
	return 0;
}

/**
 * @hapd: hostapd data
 * @src_addr: Source MAC address
 * @dst_addr: Destination MAC address
 * @msg_type: 1905 CMDU message type (ETH_P_1905_SMD_ST_* for TLV path, 0 for legacy OUI path)
 * @data: frame payload
 * @data_len: frame payload length
 *
 * Called from both the IEEE 1905.1 transport (eth_p_1905.c) and the legacy
 * OUI transport (uhr_oui_transport.c).  SMD message types (0xFF01-0xFF06)
 * are TLV-decoded via eth_p_1905_msg_rx(); all other values take the legacy
 * raw-blob cast path.
 */
void uhr_iap_rx(struct hostapd_data *hapd, const u8 *src_addr, const u8 *dst_addr,
		u16 msg_type, const u8 *data, size_t data_len)
{
	const struct uhr_iap_frame *iap = NULL;
	struct uhr_iap_frame *decoded = NULL;
	u16 frame_len;

	if (msg_type == ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG ||
	    msg_type == ETH_P_1905_SMD_NEIGHBOR_FETCH_MSG) {
		if (!is_broadcast_ether_addr(dst_addr) &&
		    hapd->eth_p_1905_ctx &&
		    eth_p_1905_clone_peer(hapd->eth_p_1905_ctx, src_addr,
					  src_addr) < 0) {
			wpa_printf(MSG_WARNING,
				   "SMD IAP: Failed to register Neighbor sender "
				   MACSTR " in smd_partner list",
				   MAC2STR(src_addr));
		}
		smd_neighbor_update_rx(hapd, src_addr, dst_addr, data, data_len,
				       msg_type);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "SMD IAP: Received frame from " MACSTR " (len=%zu)",
		   MAC2STR(src_addr), data_len);

	if (os_memcmp(dst_addr, hapd->mld->mld_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_ERROR, "SMD IAP: Not for this interface");
		return;
	}

	decoded = eth_p_1905_msg_rx(hapd, src_addr, dst_addr,
				    msg_type, data, data_len);
	if (!decoded) {
		wpa_printf(MSG_ERROR, "SMD IAP: TLV decode failed");
		return;
	}

	iap = decoded;
	wpa_printf(MSG_DEBUG, "SMD IAP: TLV DECAP 0x%04x from " MACSTR " (%zu B)",
		   msg_type, MAC2STR(src_addr), data_len);
	
	frame_len = le_to_host16(iap->frame_len);

	/* Promote sender's MLD addr to a concrete peer entry (wildcard path). */
	if (eth_p_1905_clone_peer(hapd->eth_p_1905_ctx, src_addr,
			       iap->current_ap_mld_addr) < 0) {
		wpa_printf(MSG_WARNING,
			   "SMD IAP: Failed to register MLD addr " MACSTR " in smd_partner list",
			   MAC2STR(iap->current_ap_mld_addr));
	}

	switch (iap->msg_type) {
	case UHR_IAP_MSG_ST_PREP_REQUEST:
		wpa_printf(MSG_DEBUG,
			   "SMD IAP: Processing REQUEST (txn=%u)",
			   iap->iap_transaction_id);
		uhr_tgt_ap_handle_st_prep_req(hapd, iap, frame_len);
		break;
		
	case UHR_IAP_MSG_ST_PREP_RESPONSE:
		wpa_printf(MSG_DEBUG,
			   "SMD IAP: Processing RESPONSE (txn=%u, status=%u)",
			   iap->iap_transaction_id, iap->status_code);
		uhr_cur_ap_handle_st_prep_resp(hapd, iap, frame_len);
		break;

       case UHR_IAP_MSG_ST_EXEC_REQUEST:
               wpa_printf(MSG_DEBUG, "UHR IAP: Processing ST EXEC REQUEST (txn=%u)",
                          iap->iap_transaction_id);
               uhr_tgt_ap_handle_st_exec_req(hapd, iap, frame_len);
               break;

       case UHR_IAP_MSG_ST_EXEC_RESPONSE:
               wpa_printf(MSG_DEBUG, "UHR IAP: Processing ST EXEC RESPONSE (txn=%u)",
                          iap->iap_transaction_id);
               uhr_cur_ap_handle_st_exec_resp(hapd, iap, frame_len);
               break;

	case UHR_IAP_MSG_ST_ROAM_CLEANUP:
		wpa_printf(MSG_DEBUG, "UHR IAP: Processing ST ROAM CLEANUP (txn=%u)",
			   iap->iap_transaction_id);
		uhr_tgt_ap_handle_st_roam_cleanup(hapd, iap);
		break;

	case UHR_IAP_MSG_ST_PREP_CTX:
		wpa_printf(MSG_DEBUG,
			   "UHR IAP: Processing ST PREP CTX (txn=%u)",
			   iap->iap_transaction_id);
		uhr_tgt_ap_handle_st_prep_ctx(hapd, iap);
		break;

	case UHR_IAP_MSG_ST_EXEC_VIA_TGT_DONE:
		wpa_printf(MSG_DEBUG,
			   "UHR IAP: Processing ST EXEC VIA TGT DONE (txn=%u)",
			   iap->iap_transaction_id);
		uhr_cur_ap_handle_st_exec_via_tgt_done(hapd, iap, frame_len);
		break;

	case UHR_IAP_MSG_ST_CTX_REQUEST:
		wpa_printf(MSG_DEBUG,
			   "UHR IAP: Processing ST CTX REQUEST (txn=%u)",
			   iap->iap_transaction_id);
		uhr_cur_ap_handle_st_ctx_request(hapd, iap, frame_len);
		break;

	case UHR_IAP_MSG_ST_CTX_RESPONSE:
		wpa_printf(MSG_DEBUG,
			   "UHR IAP: Processing ST CTX RESPONSE (txn=%u)",
			   iap->iap_transaction_id);
		uhr_tgt_ap_handle_st_ctx_response(hapd, iap, frame_len);
		break;

	default:
		wpa_printf(MSG_ERROR, "SMD IAP: Unknown message type %u",
			   iap->msg_type);
		break;
	}
	os_free(decoded);  /* NULL on legacy path, allocated on TLV path */
}
