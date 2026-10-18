/*
 * hostapd / IEEE 802.11bn UHR — Generic Inter-AP Protocol via IEEE 1905.1
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This module provides the generic IEEE 1905.1 Inter-AP Protocol (IAP)
 * infrastructure for UHR / Wi-Fi 8.  It is designed to be extensible:
 * new protocol families are registered as additional message type
 * handlers in eth_p_1905_msg_rx() without modifying the underlying
 * 1905 transport layer (eth_p_1905.c).
 */

#ifndef ETH_P_1905_IAP_H
#define ETH_P_1905_IAP_H

#include "utils/common.h"
#include "utils/wpabuf.h"

/* Forward declaration — avoids pulling in uhr_iap.h and its dependencies */
struct uhr_iap_frame;

/* -------------------------------------------------------------------------
 * Wi-Fi 8 TLV type — shared by all Wi-Fi 8 / UHR TLVs
 * ------------------------------------------------------------------------- */
#define WIFI8_TLV_TYPE   0xC2

/* -------------------------------------------------------------------------
 * Wi-Fi 8 TLV subtype values (2-octet field at start of TLV value)
 * ------------------------------------------------------------------------- */
#define WIFI8_TLV_SUBTYPE_RECONFIG_FRAME    0x0006  /* Reconfiguration Frame TLV */
#define WIFI8_TLV_SUBTYPE_CLIENT_IDENTIFIER 0x0007  /* Client Identifier TLV     */
#define WIFI8_TLV_SUBTYPE_CLIENT_SEC_CTX    0x0009  /* Client Security Context TLV */
#define WIFI8_TLV_SUBTYPE_DATAPATH_CTX      0x000A  /* Data Path Context TLV     */
#define WIFI8_TLV_SUBTYPE_ROAM_CLEANUP      0x000B  /* Roam Cleanup TLV          */
#define WIFI8_TLV_SUBTYPE_VENDOR_CTX        0x000C  /* Vendor Context TLV        */

/* -------------------------------------------------------------------------
 * Reconfiguration Frame TLV — frame_type field values
 * ------------------------------------------------------------------------- */
#define WIFI8_RECONFIG_FRAME_TYPE_REQUEST   0x00  /* UHR Reconfiguration Request  */
#define WIFI8_RECONFIG_FRAME_TYPE_RESPONSE  0x01  /* UHR Reconfiguration Response */

/* -------------------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------------------- */
struct hostapd_data;
struct sta_info;

/* =========================================================================
 * TLV encode API
 *
 * Each function builds the Wi-Fi 8 TLV payload for one SMD IAP message and
 * returns an allocated wpabuf.  The caller is responsible for freeing the
 * buffer with wpabuf_free() and for transmitting it via eth_p_1905_send().
 *
 * These are the primary integration points for uhr_iap.c: the existing
 * uhr_iap_send_st_*() functions call these helpers to obtain the 1905 TLV
 * payload, then send it via the 1905 transport.
 * ========================================================================= */

/**
 * eth_p_1905_iap_encode_prep_req - Encode ST Preparation Request TLV payload
 *
 * Serving AP → Target AP.  TLVs included based on @iap->flags:
 *   Reconfig Frame TLV        (if iap->frame_len > 0)
 *   Client Security Ctx TLV   (if UHR_IAP_FLAG_HAS_SEC_CTX)
 *   Datapath Ctx TLV          (if UHR_IAP_FLAG_HAS_DYNAMIC_CTX)
 *
 * @hapd: hostapd instance (for mld_link_id)
 * @iap:  IAP frame carrying the data to encode; iap->target_ap_mld_addr is
 *        used as the destination MLD address in each TLV.
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_prep_req(struct hostapd_data *hapd,
					      const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_prep_resp - Encode ST Preparation Response TLV payload
 *
 * Target AP → Serving AP.  TLVs included:
 *   Reconfig Frame TLV  (if iap->frame_len > 0)
 *
 * @iap: IAP frame; iap->current_ap_mld_addr is the destination.
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_prep_resp(const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_exec_req - Encode ST Execute Request TLV payload
 *
 * Serving AP → Target AP.  Same TLV selection logic as encode_prep_req.
 *
 * @hapd: hostapd instance (for mld_link_id)
 * @iap:  IAP frame; iap->target_ap_mld_addr is the destination.
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_exec_req(struct hostapd_data *hapd,
					      const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_exec_resp - Encode ST Execute Response TLV payload
 *
 * Role-dependent encoding based on iap->flags:
 *
 * Serving AP → Target AP  (UHR_IAP_FLAG_HAS_SEC_CTX | UHR_IAP_FLAG_HAS_DYNAMIC_CTX set):
 *   Client Identifier TLV + Client Security Ctx TLV + Datapath Ctx TLV
 *
 * Target AP → Serving AP  (flags = 0):
 *   Reconfig Frame TLV only
 *
 * @hapd: hostapd instance (for smd_identifier, used on Serving AP path)
 * @iap:  IAP frame; iap->current_ap_mld_addr is the destination.
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_exec_resp(struct hostapd_data *hapd,
					       const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_prep_ctx - Encode ST Preparation Context TLV payload
 *
 * Serving AP → Target AP (deferred SMD context after MTU split).
 * TLVs included:
 *   Datapath Ctx TLV  (always — iap->flags must have UHR_IAP_FLAG_HAS_DYNAMIC_CTX)
 *
 * @iap: IAP frame with UHR_IAP_FLAG_HAS_DYNAMIC_CTX set and frame_len = 0;
 *       iap->target_ap_mld_addr is the destination.
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_prep_ctx(const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_roam_cleanup - Encode ST Roam Cleanup TLV payload
 *
 * Serving AP → non-exec Target APs.  TLVs included:
 *   Roam Cleanup TLV  (always)
 *
 * @iap: IAP frame; iap->target_ap_mld_addr is the destination,
 *       iap->sta_addr is the STA MLD address to clean up.
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_roam_cleanup(const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_ctx_req - Encode ST Context Request TLV payload
 *
 * Target AP → Current AP: request the STA's SMD context.
 * TLVs included: Client Identifier TLV (carries sta_addr).
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_ctx_req(const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_ctx_resp - Encode ST Context Response TLV payload
 *
 * Current AP → Target AP: deliver the STA's SMD context.
 * TLVs included: Client Identifier TLV (always) + Datapath Ctx TLV +
 * Vendor Ctx TLV (when context is present).
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_ctx_resp(const struct uhr_iap_frame *iap);

/**
 * eth_p_1905_iap_encode_exec_via_tgt_done - Encode ST Exec-via-Target Done TLV payload
 *
 * Target AP → Current AP: notify that the via-target transition is complete.
 * TLVs included: Client Identifier TLV (carries sta_addr).
 *
 * Returns allocated wpabuf on success, NULL on failure.
 */
struct wpabuf *eth_p_1905_iap_encode_exec_via_tgt_done(const struct uhr_iap_frame *iap);

/* =========================================================================
 * TLV decode API
 *
 * eth_p_1905_msg_rx() decodes the incoming 1905 TLV stream and returns an
 * allocated uhr_iap_frame.  The caller (uhr_iap_rx()) is responsible for
 * dispatching to the appropriate southbound handler and freeing the frame.
 * ========================================================================= */

/**
 * eth_p_1905_msg_rx - Decode a received SMD IAP 1905 CMDU
 *
 * Called from uhr_iap_rx() to decode a received 1905 CMDU.  Validates the
 * destination MLD address and dispatches to the appropriate static decoder
 * based on @msg_type.  Returns the decoded uhr_iap_frame to the caller;
 * does NOT invoke any southbound handlers.
 *
 * Returns allocated uhr_iap_frame on success, NULL on failure.
 */
struct uhr_iap_frame *eth_p_1905_msg_rx(struct hostapd_data *hapd,
					 const u8 *src_addr,
					 const u8 *dst_addr,
					 u16 msg_type,
					 const u8 *data,
					 size_t data_len);

#endif /* ETH_P_1905_IAP_H */