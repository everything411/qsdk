/*
 * hostapd / IEEE 802.11bn UHR SMD Neighborhood Update
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "hostapd.h"
#include "neighbor_db.h"
#include "uhr_neighbor_update.h"
#include "eth_p_1905.h"


/*
 * Neighborhood Update TLV (payload carried in ETH_P_OUI)
 *
 * Element ID | Length | Update Type | BSSID | BSSID Info | OpClass | Channel |
 * PHY Type | Optional Subelements
 */
#define SMD_NEIGHBOR_TLV_EID 0xdd
/* Fixed body fields in nr->nr: BSSID + BSSID Info + Op Class + Channel + PHY Type */
#define NR_BODY_FIXED_LEN (ETH_ALEN + 4 + 1 + 1 + 1)
/* SMD TLV header: EID + Len + Update Type */
#define SMD_NEIGHBOR_TLV_HDR_LEN 3
#define SMD_NEIGHBOR_TLV_LEN_FIELD_SIZE 1
#define SMD_NEIGHBOR_REPORT_PAYLOAD_LEN (1 + ETH_ALEN + 4 + 1 + 1 + 1)

static const char * smd_neighbor_update_subelem_name(u8 id)
{
	switch (id) {
	case WNM_NEIGHBOR_BSS_LOAD:
		return "BSS Load";
	case WNM_NEIGHBOR_UHR_CAPAB:
		return "UHR Capabilities";
	case WNM_NEIGHBOR_UHR_OPER:
		return "UHR Operation";
	case WNM_NEIGHBOR_SMD_INFO:
		return "SMD Information";
	case WNM_NEIGHBOR_TX_POWER_INDICATION:
		return "Transmit Power Indication";
	case WNM_NEIGHBOR_HT_CAPAB:
		return "HT Capabilities";
	case WNM_NEIGHBOR_SUPP_RATES:
		return "Supported Rates";
	case WNM_NEIGHBOR_HT_OPER:
		return "HT Operation";
	case WNM_NEIGHBOR_VHT_CAPAB:
		return "VHT Capabilities";
	case WNM_NEIGHBOR_VHT_OPER:
		return "VHT Operation";
	case WNM_NEIGHBOR_HE_CAPAB:
		return "HE Capabilities";
	case WNM_NEIGHBOR_HE_OPER:
		return "HE Operation";
	case WNM_NEIGHBOR_EHT_CAPAB:
		return "EHT Capabilities";
	case WNM_NEIGHBOR_EHT_OPER:
		return "EHT Operation";
	case WNM_NEIGHBOR_MULTI_LINK:
		return "Multi-Link";
	default:
		return "Unknown";
	}
}

static void smd_neighbor_update_log_subelems(const u8 *subelems,
					     size_t subelems_len)
{
	const u8 *pos = subelems;
	size_t left = subelems_len;

	while (left >= 2) {
		u8 id = pos[0];
		u8 len = pos[1];
		const char *name = smd_neighbor_update_subelem_name(id);
		char label[64];

		pos += 2;
		left -= 2;

		if (len > left) {
			wpa_printf(MSG_WARNING,
				   "SMD Neighbor: Invalid subelement len=%u (id=%u left=%zu)",
				   len, id, left);
			break;
		}

		wpa_printf(MSG_DEBUG,
			   "SMD Neighbor: subelement id=%u (%s) len=%u",
			   id, name, len);

		switch (id) {
		case WNM_NEIGHBOR_BSS_LOAD:
			if (len >= 5) {
				u16 sta_count = WPA_GET_LE16(pos);
				u8 chan_util = pos[2];
				u16 avail_adm = WPA_GET_LE16(pos + 3);

				wpa_printf(MSG_DEBUG,
					   "SMD Neighbor: bss load sta_count=%u chan_util=%u avail_adm=%u",
					   sta_count, chan_util, avail_adm);
			}
			break;
		case WNM_NEIGHBOR_SMD_INFO:
			if (len >= ETH_ALEN) {
				wpa_printf(MSG_DEBUG,
					   "SMD Neighbor: smd identifier " MACSTR,
					   MAC2STR(pos));
			}
			break;
		case WNM_NEIGHBOR_MULTI_LINK:
			if (len >= 3) {
				u16 control = WPA_GET_LE16(pos);
				u8 common_len = pos[2];
				const u8 *common = pos + 3;
				size_t common_off = 3 + common_len;

				wpa_printf(MSG_DEBUG,
					   "SMD Neighbor: ML control=0x%04x common_len=%u",
					   control, common_len);

				if (common_off <= len) {
					if (common_len >= ETH_ALEN) {
						wpa_printf(MSG_DEBUG,
							   "SMD Neighbor: ML MLD addr " MACSTR,
							   MAC2STR(common));
					}

					if ((control & BASIC_MULTI_LINK_CTRL_PRES_LINK_ID) &&
					    common_len >= ETH_ALEN + 1) {
						u8 link_id = common[ETH_ALEN] &
							BASIC_MLE_STA_CTRL_LINK_ID_MASK;

						wpa_printf(MSG_DEBUG,
							   "SMD Neighbor: ML link_id=%u",
							   link_id);
					}
				} else {
					wpa_printf(MSG_WARNING,
						   "SMD Neighbor: ML common info truncated (len=%u remaining=%zu)",
						   common_len,
						   (size_t) (len - 3));
				}
			}
			break;
		default:
			break;
		}

		if (len) {
			os_snprintf(label, sizeof(label),
				    "SMD Neighbor: subelement data (id=%u)",
				    id);
			wpa_hexdump(MSG_DEBUG, label, pos, len);
		}
		pos += len;
		left -= len;
	}

	if (left) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: Trailing %zu bytes in subelements",
			   left);
	}
}

static bool smd_neighbor_update_validate_rx_addr(struct hostapd_data *hapd,
						 const u8 *dst_addr)
{
	return hapd && dst_addr && (is_broadcast_ether_addr(dst_addr) ||
				    ether_addr_equal(dst_addr, hapd->own_addr));
}

static const u8 *smd_neighbor_update_get_src_addr(struct hostapd_data *hapd)
{
	if (!hapd)
		return NULL;

	/* Use MLD address when available; fall back to link BSSID otherwise. */
	if (hapd->mld)
		return hapd->mld->mld_addr;

	return hapd->own_addr;
}

static struct smd_neighbor_update_entry *
smd_neighbor_update_get_entry(struct smd_neighbor_update_ctx *ctx,
			      const u8 *bssid)
{
	struct smd_neighbor_update_entry *e;

	if (!ctx || !bssid)
		return NULL;

	dl_list_for_each(e, &ctx->entries, struct smd_neighbor_update_entry, list) {
		if (ether_addr_equal(e->bssid, bssid))
			return e;
	}
	return NULL;
}

static struct smd_neighbor_update_entry *
smd_neighbor_update_add_entry(struct smd_neighbor_update_ctx *ctx,
			      const u8 *bssid)
{
	struct smd_neighbor_update_entry *e;

	if (!ctx || !bssid)
		return NULL;

	e = smd_neighbor_update_get_entry(ctx, bssid);
	if (e)
		return e;

	e = os_zalloc(sizeof(*e));
	if (!e)
		return NULL;

	os_memcpy(e->bssid, bssid, ETH_ALEN);
	os_get_time(&e->last_update);
	dl_list_add(&ctx->entries, &e->list);

	return e;
}

static void smd_neighbor_update_set_timestamp(struct smd_neighbor_update_ctx *ctx,
					      const u8 *bssid)
{
	struct smd_neighbor_update_entry *e;

	e = smd_neighbor_update_add_entry(ctx, bssid);
	if (!e)
		return;

	os_get_time(&e->last_update);
	e->pull_retry_count = 0;
}


static void smd_neighbor_update_free_entry(struct smd_neighbor_update_ctx *ctx,
					   const u8 *bssid)
{
	struct smd_neighbor_update_entry *e;

	e = smd_neighbor_update_get_entry(ctx, bssid);
	if (!e)
		return;

	dl_list_del(&e->list);
	os_free(e);
}

static int smd_neighbor_update_build_tlv(struct hostapd_data *hapd,
					 enum smd_neighbor_update_type update_type,
					 struct wpabuf **tlv)
{
	struct hostapd_neighbor_entry *nr;
	struct wpabuf *buf;
	u8 *len_pos;

	/* Ensure own Neighbor Report entry is up-to-date */
	hostapd_neighbor_set_own_report(hapd);

	nr = hostapd_neighbor_get(hapd, hapd->own_addr, NULL);
	if (!nr || !nr->nr) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: Build TLV failed. Own neighbor report is not present");
		return -1;
	}

	if (wpabuf_len(nr->nr) < NR_BODY_FIXED_LEN) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: Build TLV failed. Invalid Neighbort report length");
		return -1;
	}

	buf = wpabuf_alloc(SMD_NEIGHBOR_TLV_HDR_LEN + wpabuf_len(nr->nr));
	if (!buf) {
		wpa_printf(MSG_ERROR,
			   "SMD Neighbor: Build TLV failed. Neighbor report alloc failed");
		return -1;
	}

	wpabuf_put_u8(buf, SMD_NEIGHBOR_TLV_EID);
	len_pos = wpabuf_put(buf, 1);
	wpabuf_put_u8(buf, update_type);
	wpabuf_put_buf(buf, nr->nr);

	*len_pos = wpabuf_len(buf) - 2;
	*tlv = buf;
	return 0;
}

static struct wpabuf *
smd_neighbor_update_build_mld_tlvs(struct hostapd_data *hapd,
				    enum smd_neighbor_update_type update_type)
{
	struct wpabuf *all = NULL;
	struct hostapd_data *link_bss;
	struct wpabuf *tlv;

	if (smd_neighbor_update_build_tlv(hapd, update_type, &tlv) < 0)
		return NULL;
	all = tlv;

	for_each_mld_link(link_bss, hapd) {
		struct wpabuf *combined;

		if (link_bss == hapd || !link_bss->started)
			continue;

		if (smd_neighbor_update_build_tlv(link_bss, update_type, &tlv) < 0)
			continue;

		combined = wpabuf_concat(all, tlv);
		if (!combined) {
			wpa_printf(MSG_WARNING,
				   "SMD Neighbor: MLD TLV concat failed");
			wpabuf_free(all);
			wpabuf_free(tlv);
			return NULL;
		}
		all = combined;
	}

	return all;
}

int smd_neighbor_update_send_mld(struct hostapd_data *hapd,
				 enum smd_neighbor_update_type update_type)
{
	struct wpabuf *tlvs;
	int ret;
	const u8 *src_addr;
	const u8 bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (!hapd || !hapd->eth_p_1905_ctx) {
		wpa_printf(MSG_ERROR, "SMD Neighbor: hapd or oui ctx is NULL");
		return -1;
	}

	tlvs = smd_neighbor_update_build_mld_tlvs(hapd, update_type);
	if (!tlvs) {
		wpa_printf(MSG_WARNING, "SMD Neighbor: MLD TLV build failed");
		return -1;
	}

	src_addr = smd_neighbor_update_get_src_addr(hapd);

	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, bcast, src_addr,
			      ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG,
			      wpabuf_head(tlvs), wpabuf_len(tlvs));
	wpabuf_free(tlvs);
	return ret;
}


static int smd_neighbor_update_parse_tlv(struct smd_neighbor_update_ctx *ctx,
					const u8 *src_addr,
					const u8 *data, size_t data_len,
					size_t *consumed)
{
	struct hostapd_data *hapd = ctx->hapd;
	const u8 *pos = data;
	const u8 *end = data + data_len;
	const u8 *tlv_end;
	u8 update_type;
	const u8 *bssid;
	u32 bssid_info;
	u8 op_class, channel, phy_type;
	struct wpa_ssid_value ssid;
	struct wpabuf *nr;
	const u8 *subelems;
	size_t subelems_len;
	size_t raw_subelems_len;
#ifdef CONFIG_IEEE80211BN
	bool same_smd = false;
	const u8 *smd_info_start = NULL;
	const u8 *smd_info_end = NULL;
#endif

	if (data_len < 2) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: tlv parsing failed short frame len=%zu",
			   data_len);
		return -1;
	}

	if (*pos++ != SMD_NEIGHBOR_TLV_EID) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: tlv parsing failed unexpected eid=0x%02x",
			   pos[-1]);
		return -1;
	}
	if (pos >= end) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: tlv parsing failed, length field is NULL");
		return -1;
	}

	if (pos[0] > end - pos - 1) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: tlv parsing failed, invalid len=%u remaining=%zu",
			   pos[0], (size_t) (end - pos - 1));
		return -1;
	}
	if (pos[0] < SMD_NEIGHBOR_REPORT_PAYLOAD_LEN) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: Parsing failed, too short len=%u",
			   pos[0]);
		return -1;
	}

	tlv_end = pos + SMD_NEIGHBOR_TLV_LEN_FIELD_SIZE + pos[0];
	if (consumed)
		*consumed = (size_t)(tlv_end - data);
	pos++;
	update_type = *pos++;
	bssid = pos;
	pos += ETH_ALEN;
	bssid_info = WPA_GET_LE32(pos);
	pos += 4;
	op_class = *pos++;
	channel = *pos++;
	phy_type = *pos++;

	subelems = pos;
	raw_subelems_len = tlv_end - pos;
	subelems_len = raw_subelems_len;
	if (raw_subelems_len)
		smd_neighbor_update_log_subelems(subelems, raw_subelems_len);
	/* Parse SSID from optional subelements if present */
	os_memset(&ssid, 0, sizeof(ssid));
	while (subelems_len >= 2) {
		u8 id = subelems[0];
		u8 elen = subelems[1];

		if (2 + elen > subelems_len)
			break;
		if (id == WLAN_EID_SSID && elen <= SSID_MAX_LEN) {
			ssid.ssid_len = elen;
			os_memcpy(ssid.ssid, subelems + 2, elen);
		}
#ifdef CONFIG_IEEE80211BN
		if (id == WNM_NEIGHBOR_SMD_INFO) {
			smd_info_start = subelems;
			smd_info_end = subelems + 2 + elen;
			if (elen >= ETH_ALEN && hapd->conf->smd.enabled &&
			    os_memcmp(subelems + 2, hapd->conf->smd.smd_identifier,
				      ETH_ALEN) == 0)
				same_smd = true;
		}
#endif
		subelems += 2 + elen;
		subelems_len -= 2 + elen;
	}

	/* If SSID is not present, use our own SSID as default */
	if (!ssid.ssid_len) {
		ssid.ssid_len = hapd->conf->ssid.ssid_len;
		os_memcpy(ssid.ssid, hapd->conf->ssid.ssid, ssid.ssid_len);
	}

#ifdef CONFIG_IEEE80211BN
	/* Always recompute SAME_SMD locally rather than trusting the sender's
	 * value, since only this AP knows its own SMD identifier. */
	bssid_info &= ~NEI_REP_BSSID_INFO_SAME_SMD;
	if (same_smd)
		bssid_info |= NEI_REP_BSSID_INFO_SAME_SMD;
#endif

	if (update_type == SMD_NEIGHBOR_UPDATE_REMOVE_AP) {
		hostapd_neighbor_remove(hapd, bssid, &ssid);
		smd_neighbor_update_free_entry(ctx, bssid);

		if (hapd->conf->mld_ap && hapd->mld) {
			struct hostapd_data *link_bss;

			for_each_mld_link(link_bss, hapd) {
				if (link_bss == hapd || !link_bss->started)
					continue;
				hostapd_neighbor_remove(link_bss, bssid, &ssid);
			}
		}

		wpa_printf(MSG_DEBUG,
			   "SMD Neighbor: Removed " MACSTR " (from " MACSTR ")",
			   MAC2STR(bssid), MAC2STR(src_addr));
		return 0;
	}

	if (update_type != SMD_NEIGHBOR_UPDATE_NEW_AP &&
	    update_type != SMD_NEIGHBOR_UPDATE_MODIFY_AP) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: Unknown update type=0x%02x",
			   update_type);
		return -1;
	}

	/* Build Neighbor Report payload (no EID/len) */
	nr = wpabuf_alloc(SMD_NEIGHBOR_REPORT_PAYLOAD_LEN - 1 + (tlv_end - pos));
	if (!nr) {
		wpa_printf(MSG_ERROR,
			   "SMD Neighbor: NR alloc failed");
		return -1;
	}

	wpabuf_put_data(nr, bssid, ETH_ALEN);
	wpabuf_put_le32(nr, bssid_info);
	wpabuf_put_u8(nr, op_class);
	wpabuf_put_u8(nr, channel);
	wpabuf_put_u8(nr, phy_type);
	if (raw_subelems_len) {
#ifdef CONFIG_IEEE80211BN
		if (same_smd && smd_info_start) {
			/* Strip WNM_NEIGHBOR_SMD_INFO from the neighbor report:
			 * the STA already knows the SMD ID when SAME_SMD is set,
			 * so advertising it again is redundant. */
			wpabuf_put_data(nr, pos, smd_info_start - pos);
			wpabuf_put_data(nr, smd_info_end,
					pos + raw_subelems_len - smd_info_end);
		} else
#endif
		{
			wpabuf_put_data(nr, pos, raw_subelems_len);
		}
	}

	if (hostapd_neighbor_set(hapd, bssid, &ssid, nr, NULL, NULL, 0, 0) < 0) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: neighbor_set failed for "
			   MACSTR, MAC2STR(bssid));
		wpabuf_free(nr);
		return -1;
	}

	/* Propagate to all other started links in this MLD.
	 * hostapd_neighbor_set() dups nr internally so one nr serves all calls. */
	if (hapd->conf->mld_ap && hapd->mld) {
		struct hostapd_data *link_bss;

		for_each_mld_link(link_bss, hapd) {
			if (link_bss == hapd || !link_bss->started)
				continue;
			hostapd_neighbor_set(link_bss, bssid, &ssid,
					     nr, NULL, NULL, 0, 0);
		}
	}

	smd_neighbor_update_set_timestamp(ctx, bssid);
	wpabuf_free(nr);

	wpa_printf(MSG_DEBUG,
		   "SMD Neighbor: Updated " MACSTR " (type=0x%02x from " MACSTR ")",
		   MAC2STR(bssid), update_type, MAC2STR(src_addr));

	return (int) update_type;
}

static void smd_neighbor_update_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct smd_neighbor_update_ctx *ctx = eloop_ctx;
	struct smd_neighbor_update_entry *e, *prev;
	struct os_time now;
	struct os_time diff;

	if (!ctx)
		return;

	os_get_time(&now);

	dl_list_for_each_safe(e, prev, &ctx->entries,
			      struct smd_neighbor_update_entry, list) {
		os_time_sub(&now, &e->last_update, &diff);
		if (diff.sec < 0)
			continue;
		if ((unsigned int) diff.sec < ctx->expire_sec)
			continue;

		if (e->pull_retry_count >= ctx->pull_retry_max) {
			wpa_printf(MSG_DEBUG,
				   "SMD Neighbor: Expired " MACSTR
				   " (age=%ld sec) - removing after %u retries",
				   MAC2STR(e->bssid), diff.sec, e->pull_retry_count);
			hostapd_neighbor_remove(ctx->hapd, e->bssid, NULL);
			smd_neighbor_update_free_entry(ctx, e->bssid);
			continue;
		}

		e->pull_retry_count++;
		wpa_printf(MSG_DEBUG,
			   "SMD Neighbor: Expired " MACSTR
			   " (age=%ld sec) - pull retry %u/%u",
			   MAC2STR(e->bssid), diff.sec, e->pull_retry_count,
			   ctx->pull_retry_max);
		smd_neighbor_update_send_pull_ucast(ctx->hapd, e->bssid);
	}

	eloop_register_timeout(ctx->pull_period_sec, 0,
			       smd_neighbor_update_timer, ctx, NULL);
}


static void smd_neighbor_update_rx_frame(struct smd_neighbor_update_ctx *ctx,
					 const u8 *src_addr, const u8 *dst_addr,
					 const u8 *data, size_t data_len)
{
	struct hostapd_data *hapd;
	const u8 *pos;
	size_t remaining;
	bool got_new_ap = false;

	if (!ctx)
		return;

	hapd = ctx->hapd;
	if (!hapd)
		return;

	if (!smd_neighbor_update_validate_rx_addr(hapd, dst_addr)) {
		wpa_printf(MSG_DEBUG, "SMD Neighbor: Frame not for this BSS");
		return;
	}

	if (ether_addr_equal(src_addr, hapd->own_addr))
		return;

	/* Process all TLVs — a multi-link NEW_AP carries one per link */
	pos = data;
	remaining = data_len;
	while (remaining >= 2) {
		size_t consumed = 0;
		int ret;

		ret = smd_neighbor_update_parse_tlv(ctx, src_addr, pos,
						    remaining, &consumed);
		if (ret < 0) {
			wpa_printf(MSG_ERROR,
				   "SMD Neighbor: Failed to parse update");
			break;
		}
		if (ret == SMD_NEIGHBOR_UPDATE_NEW_AP)
			got_new_ap = true;
		if (consumed == 0 || consumed > remaining)
			break;
		pos += consumed;
		remaining -= consumed;
	}

	/*
	 * Send one multi-TLV MODIFY_AP covering all local started links so
	 * the sender learns our full MLD topology in a single frame.
	 */
	if (got_new_ap)
		smd_neighbor_update_send_mld(hapd, SMD_NEIGHBOR_UPDATE_MODIFY_AP);
}


static void smd_neighbor_fetch_rx_frame(struct smd_neighbor_update_ctx *ctx,
				  const u8 *src_addr, const u8 *dst_addr,
				  const u8 *data, size_t data_len)
{
	struct wpabuf *tlv = NULL;
	struct hostapd_data *hapd;
	const u8 *tx_src_addr;
	int ret;

	if (!ctx)
		return;

	hapd = ctx->hapd;
	if (!hapd)
		return;

	if (ctx->hapd != hapd)
		return;

	if (!smd_neighbor_update_validate_rx_addr(hapd, dst_addr))
		return;

	/* Respond with our current neighbor info */
	ret = smd_neighbor_update_build_tlv(hapd, SMD_NEIGHBOR_UPDATE_MODIFY_AP,
					    &tlv);
	if (ret < 0) {
		wpa_printf(MSG_WARNING, "SMD Neighbor: fetch response TLV build failed");
		return;
	}

	tx_src_addr = smd_neighbor_update_get_src_addr(hapd);

	eth_p_1905_send(hapd->eth_p_1905_ctx, src_addr, tx_src_addr,
			ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG,
			wpabuf_head(tlv), wpabuf_len(tlv));
	wpabuf_free(tlv);
}


void smd_neighbor_update_rx(struct hostapd_data *hapd, const u8 *src_addr,
			      const u8 *dst_addr, const u8 *data, size_t data_len,
			      u16 msg_type)
{
	struct smd_neighbor_update_ctx *ctx;

	if (!hapd || !hapd->smd_neighbor_update_ctx)
		return;

	ctx = hapd->smd_neighbor_update_ctx;

	switch (msg_type) {
	case ETH_P_1905_SMD_NEIGHBOR_FETCH_MSG:
		smd_neighbor_fetch_rx_frame(ctx, src_addr, dst_addr, data, data_len);
		break;
	case ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG:
		smd_neighbor_update_rx_frame(ctx, src_addr, dst_addr, data, data_len);
		break;
	default:
		wpa_printf(MSG_DEBUG,
			   "SMD Neighbor: unknown msg_type 0x%04x", msg_type);
		break;
	}
}

int smd_neighbor_update_send(struct hostapd_data *hapd,
			     enum smd_neighbor_update_type update_type)
{
	struct wpabuf *tlv = NULL;
	const u8 *src_addr;
	int ret;
	const u8 bcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (!hapd || !hapd->eth_p_1905_ctx) {
		wpa_printf(MSG_ERROR,
			   "SMD Neighbor: hapd or 1905 transport context is NULL");
		return -1;
	}

	ret = smd_neighbor_update_build_tlv(hapd, update_type, &tlv);
	if (ret < 0) {
		wpa_printf(MSG_WARNING,
			   "SMD Neighbor: TLV Build failed");
		return -1;
	}

	src_addr = smd_neighbor_update_get_src_addr(hapd);

	ret = eth_p_1905_send(hapd->eth_p_1905_ctx, bcast, src_addr,
			      ETH_P_1905_SMD_NEIGHBOR_UPDATE_MSG,
			      wpabuf_head(tlv), wpabuf_len(tlv));
	wpabuf_free(tlv);
	return ret;
}

int smd_neighbor_update_send_pull_ucast(struct hostapd_data *hapd,
					const u8 *dst_addr)
{
	const u8 *src_addr;
	const u8 dummy = 0;

	if (!hapd || !hapd->eth_p_1905_ctx || !hapd->smd_neighbor_update_ctx ||
	    !dst_addr)
		return -1;

	src_addr = smd_neighbor_update_get_src_addr(hapd);

	return eth_p_1905_send(hapd->eth_p_1905_ctx, dst_addr, src_addr,
			       ETH_P_1905_SMD_NEIGHBOR_FETCH_MSG, &dummy, 1);
}

void smd_neighbor_update_notify_own_report_changed(struct hostapd_data *hapd)
{
	struct hostapd_data *first_bss;

	if (!hapd)
		return;

	/* ctx lives on the first BSS; check it regardless of which link changed */
	first_bss = hostapd_mld_get_first_bss(hapd);
	if (!first_bss)
		first_bss = hapd; /* non-MLD AP */
	if (!first_bss->smd_neighbor_update_ctx)
		return;

	/* Send only this link's updated report — other links are unchanged */
	smd_neighbor_update_send(hapd, SMD_NEIGHBOR_UPDATE_MODIFY_AP);
}


void smd_neighbor_update_notify_going_down(struct hostapd_data *hapd)
{
	bool full_mld_teardown = false;

	if (!hapd || !hapd->smd_neighbor_update_ctx)
		return;

	/*
	 * Detect a full-MLD teardown: all partner ifaces will have had
	 * HAPD_IFACE_DISABLED set at the top of hostapd_interface_deinit() or
	 * hostapd_disable_iface() before the per-BSS deinit loop runs.
	 * Checking iface->state avoids relying on REENABLE_DEINIT being
	 * propagated cross-iface and works for both teardown paths.
	 *
	 * Full teardown: send one batched frame covering all links so peers
	 * remove the entire MLD entry atomically.
	 * Single-link removal: send only this link's REMOVE so peers keep the
	 * surviving links in their neighbor DB.
	 */
	if (hapd->conf->mld_ap && hapd->mld) {
		struct hostapd_data *link_bss;

		full_mld_teardown = true;
		for_each_mld_link(link_bss, hapd) {
			if (!link_bss->iface ||
			    link_bss->iface->state != HAPD_IFACE_DISABLED) {
				full_mld_teardown = false;
				break;
			}
		}
	}

	if (full_mld_teardown)
		smd_neighbor_update_send_mld(hapd,
					     SMD_NEIGHBOR_UPDATE_REMOVE_AP);
	else
		smd_neighbor_update_send(hapd, SMD_NEIGHBOR_UPDATE_REMOVE_AP);
}


int smd_neighbor_update_init(struct hostapd_data *hapd)
{
	struct smd_neighbor_update_ctx *ctx;

	if (!hapd || !hapd->eth_p_1905_ctx) {
		wpa_printf(MSG_ERROR,
			   "SMD Neighbor: hapd or 1905 transport context is NULL");
		return -1;
	}

	if (hapd->smd_neighbor_update_ctx)
		return 0;

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "SMD Neighbor: INIT alloc failed");
		return -1;
	}

	ctx->hapd = hapd;
	ctx->expire_sec = hapd->conf->smd_neighbor_expiry_time;
	ctx->pull_period_sec = hapd->conf->smd_neighbor_pull_interval;
	ctx->pull_retry_max = SMD_NEIGHBOR_PULL_RETRY_MAX;
	dl_list_init(&ctx->entries);

	hapd->smd_neighbor_update_ctx = ctx;

	eloop_register_timeout(ctx->pull_period_sec, 0,
			       smd_neighbor_update_timer, ctx, NULL);

	/*
	 * Send NEW_AP now only if every configured MLD link is already up.
	 * Otherwise the last link to start will trigger it via
	 * smd_neighbor_update_notify_link_started().
	 */
	if (!hapd->conf->mld_ap || !hapd->mld ||
	    hostapd_get_active_links(hapd) + 1 >= hapd->mld->num_links) {
		smd_neighbor_update_send_mld(hapd, SMD_NEIGHBOR_UPDATE_NEW_AP);
		ctx->new_ap_sent = true;
	}
	return 0;
}

void smd_neighbor_update_notify_link_started(struct hostapd_data *hapd)
{
	struct hostapd_data *first_bss;
	struct smd_neighbor_update_ctx *ctx;

	if (!hapd || !hapd->eth_p_1905_ctx)
		return;

	first_bss = hostapd_mld_get_first_bss(hapd);
	if (!first_bss)
		return;

	ctx = first_bss->smd_neighbor_update_ctx;
	if (!ctx) {
		/* First BSS not initialised yet; init will pick this link up */
		return;
	}

	if (!ctx->new_ap_sent) {
		/* Still waiting for all links: send once the last one is up */
		if (!first_bss->mld)
			return;
		if (hostapd_get_active_links(first_bss) + 1 <
		    first_bss->mld->num_links)
			return; /* more links still expected */

		smd_neighbor_update_send_mld(first_bss,
					     SMD_NEIGHBOR_UPDATE_NEW_AP);
		ctx->new_ap_sent = true;
	} else {
		/* Dynamic add after initial NEW_AP: announce this link alone */
		smd_neighbor_update_send(hapd, SMD_NEIGHBOR_UPDATE_NEW_AP);
	}
}

void smd_neighbor_update_deinit(struct hostapd_data *hapd)
{
	struct smd_neighbor_update_ctx *ctx;
	struct smd_neighbor_update_entry *e, *prev;

	if (!hapd)
		return;

	ctx = hapd->smd_neighbor_update_ctx;
	if (!ctx) {
		/*
		 * Affiliated MLD link: no ctx here.  For a full MLD teardown via
		 * hostapd_interface_deinit() the ctx-owner's going_down() already
		 * sent a batched REMOVE_AP covering all links, so no per-link send
		 * is needed there.
		 * For teardown via hostapd_disable_iface() (single-link removal or
		 * CSA), REENABLE_DEINIT is not propagated to partners and the
		 * batched path does not fire; each affiliated link must send its
		 * own REMOVE here.  The shared socket is still live because
		 * hostapd_free_hapd_data() guards eth_p_1905_deinit() with
		 * hostapd_mld_is_first_bss().
		 */
		if (hapd->eth_p_1905_ctx)
			smd_neighbor_update_send(hapd,
						 SMD_NEIGHBOR_UPDATE_REMOVE_AP);
		return;
	}

	/* ctx-owner: send REMOVE for this link then free ctx */
	smd_neighbor_update_notify_going_down(hapd);

	eloop_cancel_timeout(smd_neighbor_update_timer, ctx, NULL);

	dl_list_for_each_safe(e, prev, &ctx->entries,
			      struct smd_neighbor_update_entry, list) {
		dl_list_del(&e->list);
		os_free(e);
	}

	hapd->smd_neighbor_update_ctx = NULL;
	os_free(ctx);
}
