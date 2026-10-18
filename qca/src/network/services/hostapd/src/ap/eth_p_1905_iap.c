/*
 * hostapd / IEEE 802.11bn UHR — Generic Inter-AP Protocol via IEEE 1905.1
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This module provides the generic IEEE 1905.1 Inter-AP Protocol (IAP)
 * infrastructure for UHR / Wi-Fi 8.  The SMD messages replace the
 * proprietary blob transport in uhr_iap.c/.h with standard IEEE
 * 802.11bn Wi-Fi 8 TLVs.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/wpabuf.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "hostapd.h"
#include "sta_info.h"
#include "wpa_auth.h"
#include "wpa_auth_i.h"
#include "uhr_utils.h"
#include "eth_p_1905.h"
#include "eth_p_1905_iap.h"

/* -------------------------------------------------------------------------
 * Internal constants
 * ------------------------------------------------------------------------- */

/* Wi-Fi 8 TLV outer header: type(1) + length(2) */
#define WIFI8_TLV_HDR_LEN      3
/* Wi-Fi 8 TLV subtype field size */
#define WIFI8_TLV_SUBTYPE_LEN  2
/* Per-TLV fixed overhead: type(1) + length(2) + subtype(2) + dst_mld(6) + dst_bssid(6) = 17 bytes */
#define WIFI8_TLV_OVERHEAD     (WIFI8_TLV_HDR_LEN + WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN)
/* PN length in Data Path Context TLV (128 bits = 16 bytes) */
#define IAP_1905_PN_LEN        16
/* Maximum frame body length */
#define IAP_1905_MAX_FRAME_LEN 1500

/* Zero-filled constants (zero-initialized by static storage class, C99 §6.7.9) */
static const u8 zero_addr[ETH_ALEN];      /* zero MAC — used when BSSID not known */
static const u8 zero_pn_buf[IAP_1905_PN_LEN]; /* zero PN — used for PN padding    */

/**
 * encode_reconfig_frame_tlv - Encode Reconfiguration Frame TLV (subtype 0x0006)
 *
 * Wire layout of value field:
 *   subtype(2) | dst_mld_addr(6) | dst_bssid(6) | frame_type(1) |
 *   frame_length(2) | frame_body(frame_len)
 *
 * Note: dst_bssid is the destination link address (BSSID).  The existing
 *   uhr_iap layer routes by MLD address only and does not carry a per-link
 *   BSSID.  Pass NULL to encode zeros; callers should supply the correct
 *   link address once it is available.
 *
 * Returns: 0 on success, -1 on failure (buffer too small).
 */
static int encode_reconfig_frame_tlv(struct wpabuf *buf,
				     const u8 *dst_mld_addr,
				     const u8 *dst_bssid,
				     u8 frame_type,
				     u8 link_id,
				     const u8 *frame, u16 frame_len)
{
	/* value: subtype(2) + dst_mld(6) + dst_bssid(6) + frame_type(1) +
	 *        link_id(1) + frame_length(2) + frame_body */
	u16 val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
		      1 + 1 + 2 + frame_len;

	if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
		return -1;

	wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
	wpabuf_put_be16(buf, val_len);
	wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_RECONFIG_FRAME);
	wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
	/* dst_bssid: use caller-supplied value or zeros if not known */
	wpabuf_put_data(buf, dst_bssid ? dst_bssid : zero_addr, ETH_ALEN);
	wpabuf_put_u8(buf, frame_type);
	wpabuf_put_u8(buf, link_id);
	wpabuf_put_be16(buf, frame_len);
	if (frame && frame_len > 0)
		wpabuf_put_data(buf, frame, frame_len);

	return 0;
}

/**
 * encode_client_identifier_tlv - Encode Client Identifier TLV (subtype 0x0007)
 *
 * Wire layout of value field:
 *   subtype(2) | dst_mld_addr(6) | dst_bssid(6) | client_mld_addr(6) |
 *   ap_smd_addr(6) | ap_primary_link_id(1)
 *
 * Note: dst_bssid is the destination link address.  Pass NULL to encode zeros.
 *
 * Returns: 0 on success, -1 on failure (buffer too small).
 */
static int encode_client_identifier_tlv(struct wpabuf *buf,
					 const u8 *dst_mld_addr,
					 const u8 *dst_bssid,
					 const u8 *client_mld_addr,
					 const u8 *ap_smd_addr,
					 u8 ap_primary_link_id)
{
	u16 val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
		      ETH_ALEN + ETH_ALEN + 1;

	if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
		return -1;

	wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
	wpabuf_put_be16(buf, val_len);
	wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_CLIENT_IDENTIFIER);
	wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
	/* dst_bssid: use caller-supplied value or zeros if not known */
	wpabuf_put_data(buf, dst_bssid ? dst_bssid : zero_addr, ETH_ALEN);
	wpabuf_put_data(buf, client_mld_addr, ETH_ALEN);
	wpabuf_put_data(buf, ap_smd_addr, ETH_ALEN);
	wpabuf_put_u8(buf, ap_primary_link_id);

	return 0;
}

/**
 * encode_client_sec_ctx_tlv - Encode Client Security Context TLV (subtype 0x0009)
 *
 * Wire layout of value field (ptk_mode = 0, num_links = 1):
 *   subtype(2) | dst_mld_addr(6) | dst_bssid(6) | akm(4) | cipher(4) |
 *   ptk_mode(1) | num_links(1) |
 *   [link_id(1) | pmkid(16) | pmk_len(2) | pmk(var) |
 *    kck_len(2) | kck(var) | kek_len(2) | kek(var) | tk_len(2) | tk(var)] |
 *   rsn_ie_len(1) | rsn_ie(var) | rsnxe_len(1) | rsnxe(var)
 *
 * Note: dst_bssid is the destination link address.  Pass NULL to encode zeros.
 *
 * ptk_mode = 0 (per-SMD PTK): transfers PMK + PTK components (KCK/KEK/TK).
 * TODO: ptk_mode = 1 (per-AP PTK, transfers SMD KDK instead of PMK) is not
 *       yet implemented.
 *
 * num_links = 1: MLD security keys are common for all links; per-link keying
 * material is not required.  The spec allows per-link keys but in practice
 * the PTK is derived at MLD level and shared across all affiliated links.
 *
 * Returns: 0 on success, -1 on failure (buffer too small).
 */
static int encode_client_sec_ctx_tlv(struct wpabuf *buf,
				     const u8 *dst_mld_addr,
				     const u8 *dst_bssid,
				     u8 link_id,
				     const struct uhr_iap_security_ctx *sec_ctx)
{
	u16 per_link_len = 1 + PMKID_LEN +
			   2 + sec_ctx->pmk_len +
			   2 + sec_ctx->kck_len +
			   2 + sec_ctx->kek_len +
			   2 + sec_ctx->tk_len;
	u16 val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
		      4 + 4 + 1 + 1 +
		      per_link_len +
		      1 + sec_ctx->wpa_ie_len +
		      1 + sec_ctx->rsnxe_len;

	if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
		return -1;

	wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
	wpabuf_put_be16(buf, val_len);
	wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_CLIENT_SEC_CTX);
	wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
	/* dst_bssid: use caller-supplied value or zeros if not known */
	wpabuf_put_data(buf, dst_bssid ? dst_bssid : zero_addr, ETH_ALEN);
	wpabuf_put_data(buf, sec_ctx->akm, 4);
	wpabuf_put_data(buf, sec_ctx->cipher, 4);
	/* ptk_mode = 0: per-SMD PTK — transfers PMK + PTK components.
	 * TODO: ptk_mode = 1 (per-AP PTK / SMD KDK) not yet implemented. */
	wpabuf_put_u8(buf, 0);
	/* num_links = 1: MLD security keys are common for all links;
	 * per-link keying is not required. */
	wpabuf_put_u8(buf, 1);

	/* Per-link fields (single link) */
	wpabuf_put_u8(buf, link_id);
	wpabuf_put_data(buf, sec_ctx->pmkid, PMKID_LEN);
	wpabuf_put_be16(buf, sec_ctx->pmk_len);
	wpabuf_put_data(buf, sec_ctx->pmk, sec_ctx->pmk_len);
	wpabuf_put_be16(buf, sec_ctx->kck_len);
	wpabuf_put_data(buf, sec_ctx->kck, sec_ctx->kck_len);
	wpabuf_put_be16(buf, sec_ctx->kek_len);
	wpabuf_put_data(buf, sec_ctx->kek, sec_ctx->kek_len);
	wpabuf_put_be16(buf, sec_ctx->tk_len);
	wpabuf_put_data(buf, sec_ctx->tk, sec_ctx->tk_len);

	/* RSN IE and RSNXE (after per-link block) */
	wpabuf_put_u8(buf, sec_ctx->wpa_ie_len);
	if (sec_ctx->wpa_ie_len > 0)
		wpabuf_put_data(buf, sec_ctx->wpa_ie, sec_ctx->wpa_ie_len);
	wpabuf_put_u8(buf, sec_ctx->rsnxe_len);
	if (sec_ctx->rsnxe_len > 0)
		wpabuf_put_data(buf, sec_ctx->rsnxe, sec_ctx->rsnxe_len);

	return 0;
}

/**
 * encode_datapath_ctx_tlv - Encode Data Path Context TLV (subtype 0x000A)
 *
 * Wire layout of value field:
 *   subtype(2) | dst_mld_addr(6) | dst_bssid(6) |
 *   valid_ctx_bmap(1) | pn_len(1) |
 *   tx_valid_tid_bmap(1) | tx_num_tids(1) |
 *   [tx_ba_timeout(2) | tx_ba_params(2) | tx_ext_ba_params(2) | tx_sn(2)] x tx_num_tids |
 *   tx_pn(16) |
 *   rx_valid_tid_bmap(1) | rx_num_tids(1) |
 *   [rx_ba_timeout(2) | rx_ba_params(2) | rx_ext_ba_params(2) | rx_sn(2) | rx_pn(16)] x rx_num_tids
 *
 * ba_params encoding (matches 802.11 ADDBA BA Parameter Set field, 2 octets):
 *   bit 0    : amsdu_supported
 *   bit 1    : ba_policy (0=delayed, 1=immediate)
 *   bits 6-15: buffer_size
 *
 * ext_ba_params encoding (2 octets, big-endian):
 *   bit 0    : ext_no_frag
 *   bits 1-2 : extfrag_level
 *   bits 3-12: ext_buffer_size
 *
 * Note: dst_bssid is the destination link address.  Pass NULL to encode zeros.
 *
 * Returns: 0 on success, -1 on failure (buffer too small).
 */
static int encode_datapath_ctx_tlv(struct wpabuf *buf,
				   const u8 *dst_mld_addr,
				   const u8 *dst_bssid,
				   const struct sta_smd_ctx_info *smd_ctx,
				   size_t smd_ctx_len)
{
	int i;
	u8 tx_num_tids = 0, rx_num_tids = 0;
	u16 val_len;
	u8 pn_len;

	if (!smd_ctx || smd_ctx_len == 0) {
		/* Encode empty datapath context */
		val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
			  1 + 1 +                    /* valid_ctx_bmap + pn_len */
			  1 + 1 + IAP_1905_PN_LEN +  /* tx: valid_tid_bmap + num_tids + pn */
			  1 + 1;                     /* rx: valid_tid_bmap + num_tids */
		if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
			return -1;
		wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
		wpabuf_put_be16(buf, val_len);
		wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_DATAPATH_CTX);
		wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
		wpabuf_put_data(buf, dst_bssid ? dst_bssid : zero_addr, ETH_ALEN);
		wpabuf_put_u8(buf, 0);                              /* valid_ctx_bmap = 0 */
		wpabuf_put_u8(buf, 0);                              /* pn_len = 0 */
		wpabuf_put_u8(buf, 0);                              /* tx_valid_tid_bmap = 0 */
		wpabuf_put_u8(buf, 0);                              /* tx_num_tids = 0 */
		wpabuf_put_data(buf, zero_pn_buf, IAP_1905_PN_LEN); /* tx_pn */
		wpabuf_put_u8(buf, 0);                              /* rx_valid_tid_bmap = 0 */
		wpabuf_put_u8(buf, 0);                              /* rx_num_tids = 0 */
		return 0;
	}

	pn_len = smd_ctx->pn_len;
	if (pn_len > IAP_1905_PN_LEN)
		pn_len = IAP_1905_PN_LEN;

	/* Count valid TIDs */
	for (i = 0; i < SMD_NUM_TIDS; i++) {
		if (smd_ctx->dl.valid_tid_bmap & BIT(i))
			tx_num_tids++;
		if (smd_ctx->ul.valid_tid_bmap & BIT(i))
			rx_num_tids++;
	}

	/*
	 * Value length:
	 * subtype(2) + dst_mld(6) + dst_bssid(6) +
	 * valid_ctx_bmap(1) + pn_len(1) +
	 * tx_valid_tid_bmap(1) + tx_num_tids(1) +
	 *   [ba_timeout(2)+ba_params(2)+ext_ba_params(2)+sn(2)] x tx_num_tids +
	 * tx_pn(16) +
	 * rx_valid_tid_bmap(1) + rx_num_tids(1) +
	 *   [ba_timeout(2)+ba_params(2)+ext_ba_params(2)+sn(2)+pn(16)] x rx_num_tids
	 */
	val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
		  1 + 1 +                                             /* valid_ctx_bmap + pn_len */
		  1 + 1 + (u16)(8 * tx_num_tids) + IAP_1905_PN_LEN + /* tx */
		  1 + 1 + (u16)(24 * rx_num_tids);                   /* rx */

	if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
		return -1;

	wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
	wpabuf_put_be16(buf, val_len);
	wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_DATAPATH_CTX);
	wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
	/* dst_bssid: use caller-supplied value or zeros if not known */
	wpabuf_put_data(buf, dst_bssid ? dst_bssid : zero_addr, ETH_ALEN);
	wpabuf_put_u8(buf, smd_ctx->valid_ctx_bmap);
	wpabuf_put_u8(buf, pn_len);

	/* TX context */
	wpabuf_put_u8(buf, smd_ctx->dl.valid_tid_bmap);
	wpabuf_put_u8(buf, tx_num_tids);
	for (i = 0; i < SMD_NUM_TIDS; i++) {
		u16 ba_params, ext_ba_params;

		if (!(smd_ctx->dl.valid_tid_bmap & BIT(i)))
			continue;
		wpabuf_put_be16(buf, smd_ctx->dl.ba[i].timeout);
		ba_params = (u16)(smd_ctx->dl.ba[i].amsdu_supported & 0x1) |
			    (u16)((smd_ctx->dl.ba[i].ba_policy & 0x1) << 1) |
			    (u16)((smd_ctx->dl.ba[i].buffer_size & 0x3FF) << 6);
		wpabuf_put_be16(buf, ba_params);
		ext_ba_params = (u16)(smd_ctx->dl.ba[i].ext_no_frag & 0x1) |
				(u16)((smd_ctx->dl.ba[i].extfrag_level & 0x3) << 1) |
				(u16)((smd_ctx->dl.ba[i].ext_buffer_size & 0x3FF) << 3);
		wpabuf_put_be16(buf, ext_ba_params);
		wpabuf_put_be16(buf, smd_ctx->dl.sn[i]);
	}
	/* txPN: common for all TIDs.  tx.pn[] is already IAP_1905_PN_LEN bytes;
	 * encode it directly so decode is symmetric. */
	wpabuf_put_data(buf, smd_ctx->dl.pn, IAP_1905_PN_LEN);

	/* RX context */
	wpabuf_put_u8(buf, smd_ctx->ul.valid_tid_bmap);
	wpabuf_put_u8(buf, rx_num_tids);
	for (i = 0; i < SMD_NUM_TIDS; i++) {
		u16 ba_params, ext_ba_params;

		if (!(smd_ctx->ul.valid_tid_bmap & BIT(i)))
			continue;
		wpabuf_put_be16(buf, smd_ctx->ul.ba[i].timeout);
		ba_params = (u16)(smd_ctx->ul.ba[i].amsdu_supported & 0x1) |
			    (u16)((smd_ctx->ul.ba[i].ba_policy & 0x1) << 1) |
			    (u16)((smd_ctx->ul.ba[i].buffer_size & 0x3FF) << 6);
		wpabuf_put_be16(buf, ba_params);
		ext_ba_params = (u16)(smd_ctx->ul.ba[i].ext_no_frag & 0x1) |
				(u16)((smd_ctx->ul.ba[i].extfrag_level & 0x3) << 1) |
				(u16)((smd_ctx->ul.ba[i].ext_buffer_size & 0x3FF) << 3);
		wpabuf_put_be16(buf, ext_ba_params);
		wpabuf_put_be16(buf, smd_ctx->ul.sn[i]);
		/* rxPN per TID: rx.pn[i] is IAP_1905_PN_LEN bytes; encode directly. */
		wpabuf_put_data(buf, (const u8 *)smd_ctx->ul.pn[i], IAP_1905_PN_LEN);
	}

	return 0;
}

/* -------------------------------------------------------------------------
 * Internal: TLV decode helpers
 * ------------------------------------------------------------------------- */

static int decode_reconfig_frame_tlv(const u8 *val, u16 val_len,
				     u8 *out_frame_type,
				     u8 *out_link_id,
				     const u8 **out_frame,
				     u16 *out_frame_len)
{
	/* min_len = subtype(2) + dst_mld_addr(6) + dst_bssid(6) +
	 *           frame_type(1) + link_id(1) + frame_length(2) = 18 bytes */
	const size_t min_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN + 1 + 1 + 2;
	u16 flen;

	if (val_len < min_len)
		return -1;

	val += WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN;
	*out_frame_type = *val++;
	*out_link_id    = *val++;
	flen = WPA_GET_BE16(val);
	val += 2;

	if (val_len < min_len + flen)
		return -1;

	*out_frame_len = flen;
	*out_frame = (flen > 0) ? val : NULL;
	return 0;
}

static int decode_client_identifier_tlv(const u8 *val, u16 val_len,
					 u8 *out_client_mld,
					 u8 *out_ap_smd_addr,
					 u8 *out_link_id)
{
	const size_t min_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
			       ETH_ALEN + ETH_ALEN + 1;

	if (val_len < min_len)
		return -1;

	val += WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN;
	os_memcpy(out_client_mld, val, ETH_ALEN);
	val += ETH_ALEN;
	os_memcpy(out_ap_smd_addr, val, ETH_ALEN);
	val += ETH_ALEN;
	*out_link_id = *val;
	return 0;
}

static int decode_client_sec_ctx_tlv(const u8 *val, u16 val_len,
				     struct uhr_iap_security_ctx *out_sec_ctx)
{
	const size_t min_hdr = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
			       4 + 4 + 1 + 1;
	const u8 *p = val;
	const u8 *end = val + val_len;
	u8 ptk_mode, num_links, i;
	u16 len16;

	if (val_len < min_hdr)
		return -1;

	os_memset(out_sec_ctx, 0, sizeof(*out_sec_ctx));
	p += WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN;

	os_memcpy(out_sec_ctx->akm, p, 4);
	p += 4;
	os_memcpy(out_sec_ctx->cipher, p, 4);
	p += 4;
	ptk_mode  = *p++;
	num_links = *p++;

	for (i = 0; i < num_links && p < end; i++) {
		u16 pmk_len, kck_len, kek_len, tk_len;

		if (p + 1 + PMKID_LEN + 2 > end)
			return -1;
		p++; /* link_id */
		os_memcpy(out_sec_ctx->pmkid, p, PMKID_LEN);
		p += PMKID_LEN;

		if (ptk_mode == 0) {
			if (p + 2 > end) return -1;
			pmk_len = WPA_GET_BE16(p);
			p += 2;
			if (p + pmk_len > end) return -1;
			if (pmk_len <= PMK_LEN_MAX) {
				out_sec_ctx->pmk_len = pmk_len;
				os_memcpy(out_sec_ctx->pmk, p, pmk_len);
			}
			p += pmk_len;
		} else {
			if (p + 2 > end) return -1;
			len16 = WPA_GET_BE16(p);
			p += 2;
			if (p + len16 > end) return -1;
			p += len16;
		}

		if (p + 2 > end) return -1;
		kck_len = WPA_GET_BE16(p);
		p += 2;
		if (p + kck_len > end) return -1;
		if (kck_len <= WPA_KCK_MAX_LEN) {
			out_sec_ctx->kck_len = kck_len;
			os_memcpy(out_sec_ctx->kck, p, kck_len);
		}
		p += kck_len;

		if (p + 2 > end) return -1;
		kek_len = WPA_GET_BE16(p);
		p += 2;
		if (p + kek_len > end) return -1;
		if (kek_len <= WPA_KEK_MAX_LEN) {
			out_sec_ctx->kek_len = kek_len;
			os_memcpy(out_sec_ctx->kek, p, kek_len);
		}
		p += kek_len;

		if (p + 2 > end) return -1;
		tk_len = WPA_GET_BE16(p);
		p += 2;
		if (p + tk_len > end) return -1;
		if (tk_len <= WPA_TK_MAX_LEN) {
			out_sec_ctx->tk_len = tk_len;
			os_memcpy(out_sec_ctx->tk, p, tk_len);
		}
		p += tk_len;
	}

	if (p + 1 > end) return 0;
	out_sec_ctx->wpa_ie_len = *p++;
	if (out_sec_ctx->wpa_ie_len > 0) {
		if (p + out_sec_ctx->wpa_ie_len > end) return -1;
		os_memcpy(out_sec_ctx->wpa_ie, p, out_sec_ctx->wpa_ie_len);
		p += out_sec_ctx->wpa_ie_len;
	}

	if (p + 1 > end) return 0;
	out_sec_ctx->rsnxe_len = *p++;
	if (out_sec_ctx->rsnxe_len > 0) {
		if (p + out_sec_ctx->rsnxe_len > end) return -1;
		os_memcpy(out_sec_ctx->rsnxe, p, out_sec_ctx->rsnxe_len);
	}

	return 0;
}

static int decode_datapath_ctx_tlv(const u8 *val, u16 val_len,
				   u16 msg_type,
				   struct sta_smd_ctx_info *out_smd_ctx)
{
	/*
	 * min_len: subtype(2) + dst_mld(6) + dst_bssid(6) +
	 *          valid_ctx_bmap(1) + pn_len(1) +
	 *          tx_valid_tid_bmap(1) + tx_num_tids(1) + tx_pn(16) +
	 *          rx_valid_tid_bmap(1) + rx_num_tids(1)
	 */
	const size_t min_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN +
			       1 + 1 +
			       1 + 1 + IAP_1905_PN_LEN +
			       1 + 1;
	const u8 *p = val;
	const u8 *end = val + val_len;
	u8 tx_valid_tid_bmap, tx_num_tids;
	u8 rx_valid_tid_bmap, rx_num_tids;
	int tid_idx, i;

	if (val_len < min_len)
		return -1;

	os_memset(out_smd_ctx, 0, sizeof(*out_smd_ctx));

	p += WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN;

	if (p + 1 > end) return -1;
	out_smd_ctx->valid_ctx_bmap = *p++;

	if (p + 1 > end) return -1;
	out_smd_ctx->pn_len = *p++;

	if (p + 1 > end) return -1;
	tx_valid_tid_bmap = *p++;
	out_smd_ctx->dl.valid_tid_bmap = tx_valid_tid_bmap;

	if (p + 1 > end) return -1;
	tx_num_tids = *p++;

	tid_idx = 0;
	for (i = 0; i < tx_num_tids && p + 8 <= end; i++) {
		u16 ba_timeout, ba_params, ext_ba_params, sn;

		/* Advance to the next set bit in tx_valid_tid_bmap */
		while (tid_idx < SMD_NUM_TIDS &&
		       !(tx_valid_tid_bmap & BIT(tid_idx)))
			tid_idx++;
		if (tid_idx >= SMD_NUM_TIDS)
			break;

		ba_timeout    = WPA_GET_BE16(p);
		p += 2;
		ba_params     = WPA_GET_BE16(p);
		p += 2;
		ext_ba_params = WPA_GET_BE16(p);
		p += 2;
		sn            = WPA_GET_BE16(p);
		p += 2;

		out_smd_ctx->dl.ba[tid_idx].timeout         = ba_timeout;
		out_smd_ctx->dl.ba[tid_idx].amsdu_supported  = ba_params & 0x1;
		out_smd_ctx->dl.ba[tid_idx].ba_policy        = (ba_params >> 1) & 0x1;
		out_smd_ctx->dl.ba[tid_idx].buffer_size      = (ba_params >> 6) & 0x3FF;
		out_smd_ctx->dl.ba[tid_idx].ext_no_frag      = ext_ba_params & 0x1;
		out_smd_ctx->dl.ba[tid_idx].extfrag_level    = (ext_ba_params >> 1) & 0x3;
		out_smd_ctx->dl.ba[tid_idx].ext_buffer_size  = (ext_ba_params >> 3) & 0x3FF;
		out_smd_ctx->dl.sn[tid_idx] = sn;
		tid_idx++;
	}

	if (p + IAP_1905_PN_LEN > end) return -1;
	os_memcpy(out_smd_ctx->dl.pn, p, IAP_1905_PN_LEN);
	p += IAP_1905_PN_LEN;

	if (p + 1 > end) return -1;
	rx_valid_tid_bmap = *p++;
	out_smd_ctx->ul.valid_tid_bmap = rx_valid_tid_bmap;

	if (p + 1 > end) return -1;
	rx_num_tids = *p++;

	tid_idx = 0;
	for (i = 0; i < rx_num_tids && p + 24 <= end; i++) {
		u16 ba_timeout, ba_params, ext_ba_params, sn;

		/* Advance to the next set bit in rx_valid_tid_bmap */
		while (tid_idx < SMD_NUM_TIDS &&
		       !(rx_valid_tid_bmap & BIT(tid_idx)))
			tid_idx++;
		if (tid_idx >= SMD_NUM_TIDS)
			break;

		ba_timeout    = WPA_GET_BE16(p);
		p += 2;
		ba_params     = WPA_GET_BE16(p);
		p += 2;
		ext_ba_params = WPA_GET_BE16(p);
		p += 2;
		sn            = WPA_GET_BE16(p);
		p += 2;

		out_smd_ctx->ul.ba[tid_idx].timeout         = ba_timeout;
		out_smd_ctx->ul.ba[tid_idx].amsdu_supported  = ba_params & 0x1;
		out_smd_ctx->ul.ba[tid_idx].ba_policy        = (ba_params >> 1) & 0x1;
		out_smd_ctx->ul.ba[tid_idx].buffer_size      = (ba_params >> 6) & 0x3FF;
		out_smd_ctx->ul.ba[tid_idx].ext_no_frag      = ext_ba_params & 0x1;
		out_smd_ctx->ul.ba[tid_idx].extfrag_level    = (ext_ba_params >> 1) & 0x3;
		out_smd_ctx->ul.ba[tid_idx].ext_buffer_size  = (ext_ba_params >> 3) & 0x3FF;
		out_smd_ctx->ul.sn[tid_idx] = sn;
		os_memcpy((u8 *)out_smd_ctx->ul.pn[tid_idx], p, IAP_1905_PN_LEN);
		p += IAP_1905_PN_LEN;

		tid_idx++;
	}

	/*
	 * st_type is not encoded in the datapath_ctx_tlv; infer it from the
	 * 1905 message type: EXEC messages → CFG80211_ST_PREP_EXEC (1),
	 * PREP messages → CFG80211_ST_TYPE_PREP (0).
	 */
	out_smd_ctx->st_type = (msg_type == ETH_P_1905_SMD_ST_EXEC_REQ_MSG ||
				msg_type == ETH_P_1905_SMD_ST_EXEC_REP_MSG) ? 1 : 0;

	return 0;
}

/**
 * encode_roam_cleanup_tlv - Encode Roam Cleanup TLV (subtype 0x000B)
 *
 * Wire layout of value field:
 *   subtype(2) | dst_mld_addr(6) | dst_bssid(6) | sta_mld_addr(6)
 */
static int encode_roam_cleanup_tlv(struct wpabuf *buf,
				   const u8 *dst_mld_addr,
				   const u8 *sta_mld_addr)
{
	u16 val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN + ETH_ALEN;

	if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
		return -1;

	wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
	wpabuf_put_be16(buf, val_len);
	wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_ROAM_CLEANUP);
	wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
	wpabuf_put_data(buf, zero_addr, ETH_ALEN);  /* dst_bssid not applicable */
	wpabuf_put_data(buf, sta_mld_addr, ETH_ALEN);

	return 0;
}

/**
 * encode_vendor_ctx_tlv - Encode Vendor Context TLV (subtype 0x000C)
 *
 * Wire layout of value field:
 *   subtype(2) | dst_mld_addr(6) | dst_bssid(6) | vendor_ctx_data(var)
 *
 * Carries opaque vendor-specific context that is not part of the standard
 * Datapath Context TLV.  The receiver may process or silently ignore this
 * TLV without affecting standard interoperability.
 *
 * @dst_bssid: destination link address.  Pass NULL to encode zeros.
 */
static int encode_vendor_ctx_tlv(struct wpabuf *buf,
				  const u8 *dst_mld_addr,
				  const u8 *dst_bssid,
				  const u8 *vendor_ctx,
				  u16 vendor_ctx_len)
{
	u16 val_len = WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN + vendor_ctx_len;

	if (wpabuf_tailroom(buf) < WIFI8_TLV_HDR_LEN + val_len)
		return -1;

	wpabuf_put_u8(buf, WIFI8_TLV_TYPE);
	wpabuf_put_be16(buf, val_len);
	wpabuf_put_be16(buf, WIFI8_TLV_SUBTYPE_VENDOR_CTX);
	wpabuf_put_data(buf, dst_mld_addr, ETH_ALEN);
	/* dst_bssid: use caller-supplied value or zeros if not known */
	wpabuf_put_data(buf, dst_bssid ? dst_bssid : zero_addr, ETH_ALEN);
	wpabuf_put_data(buf, vendor_ctx, vendor_ctx_len);

	return 0;
}

/* -------------------------------------------------------------------------
 * Internal: payload length helper
 * ------------------------------------------------------------------------- */

/**
 * eth_p_1905_payload_len - Calculate the upper-bound 1905 payload length.
 *
 * Computes the buffer size needed to encode the TLVs for an IAP message,
 * based on the flags and known data sizes in @iap:
 *   - Reconfig Frame TLV:      WIFI8_TLV_OVERHEAD + frame_type(1) +
 *                              frame_len_field(2) + frame_data
 *   - Client Security Ctx TLV: WIFI8_TLV_OVERHEAD +
 *                              sizeof(uhr_iap_security_ctx) [upper bound]
 *   - Datapath Ctx TLV:        WIFI8_TLV_OVERHEAD + smd_ctx_len
 *
 * Does NOT include the Client Identifier TLV (exec_resp serving AP path);
 * that caller adds WIFI8_TLV_OVERHEAD + 2 * ETH_ALEN + 1 separately.
 */
static size_t eth_p_1905_payload_len(const struct uhr_iap_frame *iap)
{
	u16 frame_len = le_to_host16(iap->frame_len);
	size_t len = 0;

	/* Reconfig Frame TLV: frame_type(1) + link_id(1) + frame_len_field(2) + frame_data */
	if (frame_len > 0)
		len += WIFI8_TLV_OVERHEAD + 1 + 1 + 2 + frame_len;

	/* Client Security Ctx TLV: sizeof(uhr_iap_security_ctx) as upper bound */
	if (iap->flags & UHR_IAP_FLAG_HAS_SEC_CTX)
		len += WIFI8_TLV_OVERHEAD + sizeof(struct uhr_iap_security_ctx);

	/* Datapath Ctx TLV: smd_ctx_len bytes of SMD context data */
	if (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX)
		len += WIFI8_TLV_OVERHEAD + iap->smd_ctx_len;

	return len;
}

/* -------------------------------------------------------------------------
 * Public: TLV encode functions
 *
 * Each function reads iap->flags to determine which TLVs to include,
 * uses iap->sec_ctx for the security context (UHR_IAP_FLAG_HAS_SEC_CTX),
 * and iap->frame_ctx_data for the frame body and smd_ctx blob.
 * The destination MLD address is taken from iap->target_ap_mld_addr or
 * iap->current_ap_mld_addr depending on the message direction.
 * ------------------------------------------------------------------------- */

struct wpabuf *eth_p_1905_iap_encode_prep_req(struct hostapd_data *hapd,
					      const struct uhr_iap_frame *iap)
{
	u16 frame_len;
	const u8 *frame;
	const struct sta_smd_ctx_info *smd_ctx = NULL;
	size_t smd_ctx_len = 0;
	struct wpabuf *buf;

	if (!hapd || !iap)
		return NULL;

	frame_len = le_to_host16(iap->frame_len);
	frame = (frame_len > 0) ? iap->frame_ctx_data : NULL;

	if (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) {
		smd_ctx = (const struct sta_smd_ctx_info *)
			  (iap->frame_ctx_data + frame_len);
		smd_ctx_len = iap->smd_ctx_len;
	}

	buf = wpabuf_alloc(eth_p_1905_payload_len(iap));
	if (!buf)
		return NULL;

	if ((frame_len > 0 &&
	     encode_reconfig_frame_tlv(buf, iap->target_ap_mld_addr, NULL,
				       WIFI8_RECONFIG_FRAME_TYPE_REQUEST,
				       iap->current_link_id,
				       frame, frame_len) < 0) ||
	    ((iap->flags & UHR_IAP_FLAG_HAS_SEC_CTX) &&
	     encode_client_sec_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				       hapd->mld_link_id, &iap->sec_ctx) < 0) ||
	    ((iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) &&
	     encode_datapath_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				     smd_ctx, smd_ctx_len) < 0)) {
		wpabuf_free(buf);
		return NULL;
	}

	/* Vendor Context TLV: separate TLV appended after Datapath Ctx TLV */
	if ((iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) &&
	    smd_ctx && smd_ctx->vendor_ctx_len > 0 &&
	    encode_vendor_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				  smd_ctx->vendor_ctx,
				  (u16)smd_ctx->vendor_ctx_len) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_prep_resp(const struct uhr_iap_frame *iap)
{
	u16 frame_len;
	const u8 *frame;
	struct wpabuf *buf;

	if (!iap)
		return NULL;

	frame_len = le_to_host16(iap->frame_len);
	frame = (frame_len > 0) ? iap->frame_ctx_data : NULL;

	buf = wpabuf_alloc(eth_p_1905_payload_len(iap));
	if (!buf)
		return NULL;

	if (encode_reconfig_frame_tlv(buf, iap->current_ap_mld_addr, NULL,
				      WIFI8_RECONFIG_FRAME_TYPE_RESPONSE,
				      iap->current_link_id,
				      frame, frame_len) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_exec_req(struct hostapd_data *hapd,
					      const struct uhr_iap_frame *iap)
{
	/* Same TLV selection logic as encode_prep_req */
	u16 frame_len;
	const u8 *frame;
	const struct sta_smd_ctx_info *smd_ctx = NULL;
	size_t smd_ctx_len = 0;
	struct wpabuf *buf;

	if (!hapd || !iap)
		return NULL;

	frame_len = le_to_host16(iap->frame_len);
	frame = (frame_len > 0) ? iap->frame_ctx_data : NULL;

	if (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) {
		smd_ctx = (const struct sta_smd_ctx_info *)
			  (iap->frame_ctx_data + frame_len);
		smd_ctx_len = iap->smd_ctx_len;
	}

	buf = wpabuf_alloc(eth_p_1905_payload_len(iap));
	if (!buf)
		return NULL;

	if ((frame_len > 0 &&
	     encode_reconfig_frame_tlv(buf, iap->target_ap_mld_addr, NULL,
				       WIFI8_RECONFIG_FRAME_TYPE_REQUEST,
				       iap->current_link_id,
				       frame, frame_len) < 0) ||
	    ((iap->flags & UHR_IAP_FLAG_HAS_SEC_CTX) &&
	     encode_client_sec_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				       hapd->mld_link_id, &iap->sec_ctx) < 0) ||
	    ((iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) &&
	     encode_datapath_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				     smd_ctx, smd_ctx_len) < 0)) {
		wpabuf_free(buf);
		return NULL;
	}

	/* Vendor Context TLV: separate TLV appended after Datapath Ctx TLV */
	if ((iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) &&
	    smd_ctx && smd_ctx->vendor_ctx_len > 0 &&
	    encode_vendor_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				  smd_ctx->vendor_ctx,
				  (u16)smd_ctx->vendor_ctx_len) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_exec_resp(struct hostapd_data *hapd,
					       const struct uhr_iap_frame *iap)
{
	u16 frame_len;
	struct wpabuf *buf;

	if (!hapd || !iap)
		return NULL;

	frame_len = le_to_host16(iap->frame_len);

	/*
	 * Role detection for SMD_ST_EXEC_REP_MSG:
	 *
	 * Serving AP → Target AP  (both sec ctx and datapath ctx present):
	 *   Client Identifier TLV + Client Security Ctx TLV + Datapath Ctx TLV
	 *
	 * Target AP → Serving AP  (neither ctx present):
	 *   Reconfig Frame TLV only
	 */
	if ((iap->flags & UHR_IAP_FLAG_HAS_SEC_CTX) &&
	    (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX)) {
		/* Serving AP role: Client Identifier TLV is fixed-size:
		 * WIFI8_TLV_OVERHEAD + client_mld(6) + ap_smd(6) + link_id(1) */
		const struct sta_smd_ctx_info *smd_ctx =
			(const struct sta_smd_ctx_info *)
			(iap->frame_ctx_data + frame_len);
		size_t smd_ctx_len = iap->smd_ctx_len;
		size_t alloc_len = eth_p_1905_payload_len(iap) +
				   WIFI8_TLV_OVERHEAD + ETH_ALEN + ETH_ALEN + 1;

		buf = wpabuf_alloc(alloc_len);
		if (!buf)
			return NULL;

		if (encode_client_identifier_tlv(buf,
						  iap->current_ap_mld_addr,
						  NULL,
						  iap->sta_addr,
						  hapd->conf->smd.smd_identifier,
						  iap->current_link_id) < 0 ||
		    encode_client_sec_ctx_tlv(buf, iap->current_ap_mld_addr,
					      NULL, iap->current_link_id,
					      &iap->sec_ctx) < 0 ||
		    encode_datapath_ctx_tlv(buf, iap->current_ap_mld_addr,
					    NULL, smd_ctx, smd_ctx_len) < 0) {
			wpabuf_free(buf);
			return NULL;
		}

		/* Vendor Context TLV: separate TLV appended after Datapath Ctx TLV */
		if (smd_ctx->vendor_ctx_len > 0 &&
		    encode_vendor_ctx_tlv(buf, iap->current_ap_mld_addr, NULL,
					  smd_ctx->vendor_ctx,
					  (u16)smd_ctx->vendor_ctx_len) < 0) {
			wpabuf_free(buf);
			return NULL;
		}
	} else {
		/* Target AP role */
		const u8 *frame = (frame_len > 0) ? iap->frame_ctx_data : NULL;

		buf = wpabuf_alloc(eth_p_1905_payload_len(iap));
		if (!buf)
			return NULL;

		if (encode_reconfig_frame_tlv(buf, iap->current_ap_mld_addr,
					      NULL,
					      WIFI8_RECONFIG_FRAME_TYPE_RESPONSE,
					      iap->current_link_id,
					      frame, frame_len) < 0) {
			wpabuf_free(buf);
			return NULL;
		}
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_prep_ctx(const struct uhr_iap_frame *iap)
{
	const struct sta_smd_ctx_info *smd_ctx;
	size_t smd_ctx_len;
	struct wpabuf *buf;

	if (!iap || !(iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX))
		return NULL;

	/* frame_len is 0 for PREP_CTX; smd_ctx starts at frame_ctx_data[0] */
	smd_ctx = (const struct sta_smd_ctx_info *) iap->frame_ctx_data;
	smd_ctx_len = le_to_host16(iap->smd_ctx_len);

	buf = wpabuf_alloc(eth_p_1905_payload_len(iap));
	if (!buf)
		return NULL;

	if (encode_datapath_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				    smd_ctx, smd_ctx_len) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	/* Vendor Context TLV: separate TLV appended after Datapath Ctx TLV */
	if (smd_ctx->vendor_ctx_len > 0 &&
	    encode_vendor_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
				  smd_ctx->vendor_ctx,
				  (u16)smd_ctx->vendor_ctx_len) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_roam_cleanup(const struct uhr_iap_frame *iap)
{
	struct wpabuf *buf;
	/* Fixed size: HDR(3) + subtype(2) + dst_mld(6) + dst_bssid(6) + sta_mld(6) */
	size_t alloc_len = WIFI8_TLV_HDR_LEN + WIFI8_TLV_SUBTYPE_LEN +
			   ETH_ALEN + ETH_ALEN + ETH_ALEN;

	if (!iap)
		return NULL;

	buf = wpabuf_alloc(alloc_len);
	if (!buf)
		return NULL;

	if (encode_roam_cleanup_tlv(buf, iap->target_ap_mld_addr,
				    iap->sta_addr) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

/* Size of one Client Identifier TLV (type + length + value):
 * subtype(2) + dst_mld(6) + dst_bssid(6) + client_mld(6) + ap_smd(6) + link_id(1) = 27 bytes value
 * total = WIFI8_TLV_HDR_LEN(3) + 27 = 30 bytes */
#define CLIENT_ID_TLV_SIZE  (WIFI8_TLV_HDR_LEN + WIFI8_TLV_SUBTYPE_LEN + \
                             4 * ETH_ALEN + 1)

struct wpabuf *eth_p_1905_iap_encode_ctx_req(const struct uhr_iap_frame *iap)
{
	struct wpabuf *buf;

	if (!iap)
		return NULL;

	buf = wpabuf_alloc(CLIENT_ID_TLV_SIZE);
	if (!buf)
		return NULL;

	/* Client Identifier TLV: dst=current_ap, sta=iap->sta_addr */
	if (encode_client_identifier_tlv(buf, iap->current_ap_mld_addr, NULL,
					 iap->sta_addr, iap->target_ap_mld_addr,
					 0) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_ctx_resp(const struct uhr_iap_frame *iap)
{
	const struct sta_smd_ctx_info *smd_ctx = NULL;
	size_t smd_ctx_len = 0;
	struct wpabuf *buf;

	if (!iap)
		return NULL;

	if (iap->flags & UHR_IAP_FLAG_HAS_DYNAMIC_CTX) {
		smd_ctx = (const struct sta_smd_ctx_info *) iap->frame_ctx_data;
		smd_ctx_len = iap->smd_ctx_len;
	}

	buf = wpabuf_alloc(CLIENT_ID_TLV_SIZE + eth_p_1905_payload_len(iap));
	if (!buf)
		return NULL;

	/* Client Identifier TLV: dst=target_ap, sta=iap->sta_addr */
	if (encode_client_identifier_tlv(buf, iap->target_ap_mld_addr, NULL,
					 iap->sta_addr, iap->current_ap_mld_addr,
					 0) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	if (smd_ctx && smd_ctx_len > 0) {
		if (encode_datapath_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
					    smd_ctx, smd_ctx_len) < 0) {
			wpabuf_free(buf);
			return NULL;
		}
		if (smd_ctx->vendor_ctx_len > 0 &&
		    encode_vendor_ctx_tlv(buf, iap->target_ap_mld_addr, NULL,
					  smd_ctx->vendor_ctx,
					  (u16)smd_ctx->vendor_ctx_len) < 0) {
			wpabuf_free(buf);
			return NULL;
		}
	}

	return buf;
}

struct wpabuf *eth_p_1905_iap_encode_exec_via_tgt_done(const struct uhr_iap_frame *iap)
{
	struct wpabuf *buf;

	if (!iap)
		return NULL;

	buf = wpabuf_alloc(CLIENT_ID_TLV_SIZE);
	if (!buf)
		return NULL;

	/* Client Identifier TLV: dst=current_ap, sta=iap->sta_addr */
	if (encode_client_identifier_tlv(buf, iap->current_ap_mld_addr, NULL,
					 iap->sta_addr, iap->target_ap_mld_addr,
					 0) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}

/* -------------------------------------------------------------------------
 * SMD-specific 1905 message decode handler
 *
 * Parses the Wi-Fi 8 TLV stream for the four SMD ST Prep/Exec messages and
 * reconstructs a uhr_iap_frame.  Returns the allocated frame to the caller
 * (uhr_iap_rx via eth_p_1905_msg_rx); does NOT call any southbound handlers.
 * The caller is responsible for dispatch and for freeing the frame.
 * ------------------------------------------------------------------------- */

static struct uhr_iap_frame *decode_smd_msg(struct hostapd_data *hapd,
					 const u8 *src_addr,
					 const u8 *dst_addr,
					 u16 msg_type,
					 const u8 *data,
					 size_t data_len)
{
	const u8 *p = data;
	const u8 *end = data + data_len;
	u8 tlv_type;
	u16 tlv_len, tlv_subtype;
	const u8 *tlv_val;

	/* Decoded TLV data */
	const u8 *reconfig_frame = NULL;
	u16 reconfig_frame_len = 0;
	u8 reconfig_frame_type = 0;
	bool has_reconfig_frame = false;
	u8 reconfig_link_id = 0;

	u8 client_mld_addr[ETH_ALEN];
	u8 ap_smd_addr[ETH_ALEN];
	u8 client_link_id = 0;
	bool has_client_id = false;

	struct uhr_iap_security_ctx sec_ctx;
	bool has_sec_ctx = false;

	struct sta_smd_ctx_info smd_ctx_decoded;
	bool has_datapath_ctx = false;

	os_memset(&smd_ctx_decoded, 0, sizeof(smd_ctx_decoded));

	const u8 *vendor_ctx_data = NULL;
	u16 vendor_ctx_len = 0;

	u8 sta_mld_addr[ETH_ALEN];
	bool has_roam_cleanup = false;

	/* Reconstructed IAP frame */
	struct uhr_iap_frame *iap = NULL;
	size_t iap_buf_size;
	u8 *pos;

	/* ----------------------------------------------------------------
	 * Parse Wi-Fi 8 TLV stream
	 * ---------------------------------------------------------------- */
	while (p + WIFI8_TLV_HDR_LEN <= end) {
		tlv_type = *p++;
		tlv_len  = WPA_GET_BE16(p);
		p += 2;

		if (p + tlv_len > end) {
			wpa_printf(MSG_ERROR,
				   "1905 SMD IAP: TLV length overrun");
			return NULL;
		}

		tlv_val = p;
		p += tlv_len;

		/* All Wi-Fi 8 TLVs share type 0xC2 */
		if (tlv_type != WIFI8_TLV_TYPE)
			continue;
		if (tlv_len < WIFI8_TLV_SUBTYPE_LEN)
			continue;

		tlv_subtype = WPA_GET_BE16(tlv_val);

		switch (tlv_subtype) {
		case WIFI8_TLV_SUBTYPE_RECONFIG_FRAME:
			if (decode_reconfig_frame_tlv(tlv_val, tlv_len,
						      &reconfig_frame_type,
						      &reconfig_link_id,
						      &reconfig_frame,
						      &reconfig_frame_len) == 0) {
				has_reconfig_frame = true;
				client_link_id = reconfig_link_id;
			}
			break;

		case WIFI8_TLV_SUBTYPE_CLIENT_IDENTIFIER:
			if (decode_client_identifier_tlv(tlv_val, tlv_len,
							  client_mld_addr,
							  ap_smd_addr,
							  &client_link_id) == 0)
				has_client_id = true;
			break;

		case WIFI8_TLV_SUBTYPE_CLIENT_SEC_CTX:
			if (decode_client_sec_ctx_tlv(tlv_val, tlv_len,
						      &sec_ctx) == 0)
				has_sec_ctx = true;
			break;

		case WIFI8_TLV_SUBTYPE_DATAPATH_CTX:
			if (decode_datapath_ctx_tlv(tlv_val, tlv_len,
						    msg_type,
						    &smd_ctx_decoded) == 0)
				has_datapath_ctx = true;
			break;

		case WIFI8_TLV_SUBTYPE_ROAM_CLEANUP:
			if (tlv_len >= WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN + ETH_ALEN) {
				os_memcpy(sta_mld_addr,
					  tlv_val + WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN,
					  ETH_ALEN);
				has_roam_cleanup = true;
			}
			break;

		case WIFI8_TLV_SUBTYPE_VENDOR_CTX:
			/* Value: subtype(2) + dst_mld(6) + dst_bssid(6) + vendor_ctx_data(var) */
			if (tlv_len > WIFI8_TLV_SUBTYPE_LEN + ETH_ALEN + ETH_ALEN) {
				vendor_ctx_data = tlv_val + WIFI8_TLV_SUBTYPE_LEN +
						  ETH_ALEN + ETH_ALEN;
				vendor_ctx_len  = tlv_len - WIFI8_TLV_SUBTYPE_LEN -
						  ETH_ALEN - ETH_ALEN;
			}
			break;

		default:
			wpa_printf(MSG_DEBUG,
				   "1905 SMD IAP: Unknown Wi-Fi 8 TLV subtype 0x%04x",
				   tlv_subtype);
			break;
		}
	}

	/* ----------------------------------------------------------------
	 * Reconstruct uhr_iap_frame and dispatch to southbound handler.
	 *
	 * Layout of frame_ctx_data[]:
	 *   [0 .. frame_len-1]                 : 802.11 frame body
	 *   [frame_len .. frame_len+smd_ctx-1] : sta_smd_ctx_info (if present)
	 * ---------------------------------------------------------------- */
	size_t ctx_len = 0;

	if (has_datapath_ctx || vendor_ctx_len > 0)
		ctx_len += sizeof(smd_ctx_decoded);
	if (vendor_ctx_len > 0)
		ctx_len += vendor_ctx_len;

	iap_buf_size = sizeof(*iap) + reconfig_frame_len + ctx_len;

	iap = os_zalloc(iap_buf_size);
	if (!iap) {
		wpa_printf(MSG_ERROR,
			   "1905 SMD IAP: Failed to allocate IAP frame");
		return NULL;
	}

	/* Common header fields
	 *
	 * Determine sender role from TLV content rather than message type,
	 * because EXEC_REQ and EXEC_RESP can flow in either direction:
	 *
	 *   SAP sends: SecCtx present, ClientId present, RoamCleanup present,
	 *              or DatapathCtx-only (PREP_CTX — no ReconfFrame).
	 *   TAP sends: ReconfFrame only (PREP_RESP, EXEC_REQ from TAP,
	 *              EXEC_RESP from TAP).
	 *
	 * src_addr and dst_addr are already MLD addresses because
	 * eth_p_1905_send() uses hapd->mld->mld_addr as the Ethernet SA.
	 */
	bool sender_is_sap = has_sec_ctx || has_datapath_ctx || has_client_id || has_roam_cleanup;

	if (sender_is_sap) {
		os_memcpy(iap->current_ap_mld_addr, src_addr, ETH_ALEN);
		os_memcpy(iap->target_ap_mld_addr,  dst_addr, ETH_ALEN);
	} else {
		/* Sender is TAP */
		os_memcpy(iap->target_ap_mld_addr,  src_addr, ETH_ALEN);
		os_memcpy(iap->current_ap_mld_addr, dst_addr, ETH_ALEN);
	}

	/* CTX_REQ and EXEC_VIA_TGT_DONE are sent by the Target AP but carry a
	 * Client Identifier TLV (which would set sender_is_sap=true above).
	 * Override the assignment so target_ap_mld_addr always reflects the
	 * actual sender. */
	if (msg_type == ETH_P_1905_SMD_ST_CTX_REQ_MSG ||
	    msg_type == ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG) {
		os_memcpy(iap->target_ap_mld_addr,  src_addr, ETH_ALEN);
		os_memcpy(iap->current_ap_mld_addr, dst_addr, ETH_ALEN);
	}

	iap->iap_transaction_id = 0;
	iap->sequence_number    = 0;
	iap->status_code        = 0;

	/*
	 * sta_addr: from Client Identifier TLV if present, otherwise
	 * extract from the 802.11 MAC header inside the Reconfig Frame TLV.
	 */
	if (has_client_id) {
		os_memcpy(iap->sta_addr, client_mld_addr, ETH_ALEN);
		iap->current_link_id = client_link_id;
	} else if (has_reconfig_frame && reconfig_frame &&
		   reconfig_frame_len >= IEEE80211_HDRLEN) {
		/*
		 * REQUEST (reassoc request):  SA = addr2 (offset 10) = STA MAC
		 * RESPONSE (reassoc response): DA = addr1 (offset 4)  = STA MAC
		 */
		int sta_offset = (reconfig_frame_type ==
				  WIFI8_RECONFIG_FRAME_TYPE_RESPONSE) ? 4 : 10;
		os_memcpy(iap->sta_addr, reconfig_frame + sta_offset, ETH_ALEN);
		/* client_link_id was populated from the Reconfig Frame TLV */
		iap->current_link_id = client_link_id;
	} else {
		iap->current_link_id = hapd->mld_link_id;
	}

	/* Security context */
	if (has_sec_ctx) {
		iap->flags |= UHR_IAP_FLAG_HAS_SEC_CTX;
		os_memcpy(&iap->sec_ctx, &sec_ctx, sizeof(sec_ctx));
	}

	/* Frame body */
	iap->frame_len = htole16(reconfig_frame_len);
	pos = iap->frame_ctx_data;
	if (reconfig_frame && reconfig_frame_len > 0) {
		os_memcpy(pos, reconfig_frame, reconfig_frame_len);
		pos += reconfig_frame_len;
	}

	/* Datapath context (smd_ctx) placed after frame body.
	 * vendor_ctx[] is a flexible array member — copying the struct followed
	 * by the vendor bytes produces the correct in-memory layout.
	 *
	 * The struct header is written whenever datapath ctx OR vendor ctx is
	 * present (e.g. Prep messages carry vendor ctx but no datapath ctx).
	 * A zeroed struct is written in the vendor-ctx-only case so the upper
	 * layer can always cast frame_ctx_data+frame_len to sta_smd_ctx_info *. */
	if (has_datapath_ctx || vendor_ctx_len > 0) {
		iap->flags |= UHR_IAP_FLAG_HAS_DYNAMIC_CTX;
		smd_ctx_decoded.vendor_ctx_len = vendor_ctx_len;
		iap->smd_ctx_len = sizeof(smd_ctx_decoded) + vendor_ctx_len;
		os_memcpy(pos, &smd_ctx_decoded, sizeof(smd_ctx_decoded));
		pos += sizeof(smd_ctx_decoded);
	}

	/* Vendor ctx data written independently after the struct */
	if (vendor_ctx_len > 0 && vendor_ctx_data)
		os_memcpy(pos, vendor_ctx_data, vendor_ctx_len);

	/* Set msg_type in the reconstructed frame */
	switch (msg_type) {
	case ETH_P_1905_SMD_ST_PREP_REQ_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_PREP_REQUEST;
		break;
	case ETH_P_1905_SMD_ST_PREP_REP_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_PREP_RESPONSE;
		break;
	case ETH_P_1905_SMD_ST_EXEC_REQ_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_EXEC_REQUEST;
		break;
	case ETH_P_1905_SMD_ST_EXEC_REP_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_EXEC_RESPONSE;
		break;
	case ETH_P_1905_SMD_ST_PREP_CTX_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_PREP_CTX;
		break;
	case ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_ROAM_CLEANUP;
		if (has_roam_cleanup)
			os_memcpy(iap->sta_addr, sta_mld_addr, ETH_ALEN);
		break;
	case ETH_P_1905_SMD_ST_CTX_REQ_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_CTX_REQUEST;
		break;
	case ETH_P_1905_SMD_ST_CTX_REP_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_CTX_RESPONSE;
		iap->status_code = has_datapath_ctx ? UHR_IAP_STATUS_SUCCESS
						    : UHR_IAP_STATUS_FAILURE;
		break;
	case ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG:
		iap->msg_type = UHR_IAP_MSG_ST_EXEC_VIA_TGT_DONE;
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "1905 SMD IAP: Unexpected msg_type 0x%04x", msg_type);
		os_free(iap);
		return NULL;
	}

	/* Return the reconstructed frame — caller dispatches and frees */
	return iap;
}

/* -------------------------------------------------------------------------
 * Public: 1905 message receive dispatcher
 *
 * Called from uhr_iap_rx() to decode a received 1905 CMDU.  Validates the
 * destination MLD address and dispatches to the appropriate static decoder
 * based on @msg_type.  Returns the decoded uhr_iap_frame to the caller;
 * does NOT invoke any southbound handlers.
 * ------------------------------------------------------------------------- */

struct uhr_iap_frame *eth_p_1905_msg_rx(struct hostapd_data *hapd,
					 const u8 *src_addr,
					 const u8 *dst_addr,
					 u16 msg_type,
					 const u8 *data,
					 size_t data_len)
{
	if (os_memcmp(dst_addr, hapd->mld->mld_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "1905 IAP: Frame not for this MLD, ignoring");
		return NULL;
	}

	wpa_printf(MSG_DEBUG,
		   "1905 IAP: RX msg_type=0x%04x from " MACSTR " len=%zu",
		   msg_type, MAC2STR(src_addr), data_len);

	switch (msg_type) {
	/* SMD ST Preparation / Execution messages */
	case ETH_P_1905_SMD_ST_PREP_REQ_MSG:
	case ETH_P_1905_SMD_ST_PREP_REP_MSG:
	case ETH_P_1905_SMD_ST_EXEC_REQ_MSG:
	case ETH_P_1905_SMD_ST_EXEC_REP_MSG:
	case ETH_P_1905_SMD_ST_PREP_CTX_MSG:
	case ETH_P_1905_SMD_ST_ROAM_CLEANUP_MSG:
	/* SMD ST context / via-target completion messages */
	case ETH_P_1905_SMD_ST_CTX_REQ_MSG:
	case ETH_P_1905_SMD_ST_CTX_REP_MSG:
	case ETH_P_1905_SMD_ST_EXEC_VIA_TGT_DONE_MSG:
		return decode_smd_msg(hapd, src_addr, dst_addr, msg_type,
				      data, data_len);

	/* Future IAP protocol families go here */

	default:
		wpa_printf(MSG_DEBUG,
			   "1905 IAP: No handler for msg_type=0x%04x",
			   msg_type);
		return NULL;
	}
}
