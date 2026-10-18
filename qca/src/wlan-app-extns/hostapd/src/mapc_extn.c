// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/wpabuf.h"
#include "common/ieee802_11_defs.h"
#include "ap/sta_info.h"
#include "ap/ap_drv_ops.h"
#include "ap/hostapd.h"
#include "ap/mapc.h"
#include "common/qca-vendor.h"
#include "mapc_extn.h"

/* QCA vendor subelement wire constants — isolated to this file */
#define MAPC_VENDOR_SUBELEM_DISCOVERY_LEN   3   /* OUI(3) */
#define MAPC_VENDOR_SUBELEM_NEGO_LEN        12  /* OUI(3)+Type(1)+ver-attr(4)+apid-attr(4) */
#define MAPC_VENDOR_OUI_TYPE                0x01
#define MAPC_VENDOR_ATTR_VERSION            0x01
#define MAPC_VENDOR_ATTR_VERSION_LEN        2
#define MAPC_VENDOR_ATTR_Q2Q_APID           0x0F
#define MAPC_VENDOR_ATTR_Q2Q_APID_LEN       2
#define MAPC_VENDOR_VERSION_MAJOR           1
#define MAPC_VENDOR_VERSION_MINOR           0

void mapc_vendor_append_subelement(struct wpabuf *buf,
				   const struct sta_info *sta,
				   u8 action_code)
{
	if (!buf)
		return;

	if (action_code == WLAN_PA_MAPC_DISCOVERY_REQ ||
	    action_code == WLAN_PA_MAPC_DISCOVERY_RESP) {
		/* Discovery: announce QCA capability unconditionally.
		 * sta is NULL for broadcast requests / responses to unknown peers. */
		wpabuf_put_u8(buf, MAPC_SUBELEM_VENDOR_SPECIFIC);
		wpabuf_put_u8(buf, MAPC_VENDOR_SUBELEM_DISCOVERY_LEN);
		wpabuf_put_u8(buf, (OUI_QCA >> 16) & 0xff);
		wpabuf_put_u8(buf, (OUI_QCA >>  8) & 0xff);
		wpabuf_put_u8(buf,  OUI_QCA        & 0xff);
		wpa_printf(MSG_DEBUG, "MAPC vendor: built discovery subelement");
	} else {
		/* Negotiation: include APID only when peer is QCA-capable and
		 * a vendor AID has been allocated for them. */
		const struct mapc_vendor_peer_ctx *v;

		if (!sta)
			return;
		v = &sta->mapc_params.vendor;
		if (!v->set_vendor_apid || v->vendor_apid == 0)
			return;

		wpabuf_put_u8(buf, MAPC_SUBELEM_VENDOR_SPECIFIC);
		wpabuf_put_u8(buf, MAPC_VENDOR_SUBELEM_NEGO_LEN);
		wpabuf_put_u8(buf, (OUI_QCA >> 16) & 0xff);
		wpabuf_put_u8(buf, (OUI_QCA >>  8) & 0xff);
		wpabuf_put_u8(buf,  OUI_QCA        & 0xff);
		wpabuf_put_u8(buf, MAPC_VENDOR_OUI_TYPE);
		/* version attribute */
		wpabuf_put_u8(buf, MAPC_VENDOR_ATTR_VERSION);
		wpabuf_put_u8(buf, MAPC_VENDOR_ATTR_VERSION_LEN);
		wpabuf_put_u8(buf, MAPC_VENDOR_VERSION_MAJOR);
		wpabuf_put_u8(buf, MAPC_VENDOR_VERSION_MINOR);
		/* Q2Q APID attribute */
		wpabuf_put_u8(buf, MAPC_VENDOR_ATTR_Q2Q_APID);
		wpabuf_put_u8(buf, MAPC_VENDOR_ATTR_Q2Q_APID_LEN);
		wpabuf_put_le16(buf, v->vendor_apid);
		wpa_printf(MSG_DEBUG,
			   "MAPC vendor: built negotiation subelement"
			   " q2q_apid=0x%04x for " MACSTR,
			   v->vendor_apid, MAC2STR(sta->addr));
	}
}

void mapc_vendor_parse_subelement(const u8 *v, size_t sub_len,
				  struct sta_info *target)
{
	struct mapc_vendor_peer_ctx *vctx;
	size_t tlv_off;

	if (!v || !target)
		return;

	if (sub_len < 3) {
		wpa_printf(MSG_DEBUG,
			   "MAPC vendor: subelement too short (%zu)", sub_len);
		return;
	}

	if (v[0] != ((OUI_QCA >> 16) & 0xff) ||
	    v[1] != ((OUI_QCA >>  8) & 0xff) ||
	    v[2] != ( OUI_QCA        & 0xff)) {
		wpa_printf(MSG_DEBUG,
			   "MAPC vendor: unknown OUI %02x:%02x:%02x — skipping",
			   v[0], v[1], v[2]);
		return;
	}

	vctx = &target->mapc_params.vendor;
	vctx->set_vendor_apid = true;
	wpa_printf(MSG_DEBUG,
		   "MAPC vendor: QCA subelement — peer " MACSTR " is QCA AP",
		   MAC2STR(target->addr));

	/* Negotiation format (sub_len >= 12): OUI(3) + Type(1) + TLVs */
	if (sub_len < MAPC_VENDOR_SUBELEM_NEGO_LEN)
		return;

	tlv_off = 4; /* skip OUI(3) + Type(1) */
	while (tlv_off + 2 <= sub_len) {
		u8 attr_id  = v[tlv_off++];
		u8 attr_len = v[tlv_off++];

		if (tlv_off + attr_len > sub_len)
			break;
		if (attr_id == MAPC_VENDOR_ATTR_Q2Q_APID &&
		    attr_len == MAPC_VENDOR_ATTR_Q2Q_APID_LEN) {
			vctx->remote_assigned_vendor_apid =
				WPA_GET_LE16(&v[tlv_off]);
			wpa_printf(MSG_DEBUG,
				   "MAPC vendor: remote Q2Q APID=0x%04x"
				   " from " MACSTR,
				   vctx->remote_assigned_vendor_apid,
				   MAC2STR(target->addr));
		}
		tlv_off += attr_len;
	}
}

u16 mapc_alloc_vendor_aid(struct hostapd_data *hapd)
{
	int i, j;
	u16 aid;

	for (i = 0; i < AID_WORDS; i++) {
		if (hapd->sta_aid[i] == (u32)-1)
			continue;
		/* AID 0 (bit 0 of word 0) is the broadcast AID — always skip it */
		for (j = (i == 0 ? 1 : 0); j < 32; j++) {
			if (!(hapd->sta_aid[i] & BIT(j)))
				break;
		}
		if (j < 32)
			break;
	}
	if (i == AID_WORDS || j == 32)
		return 0;

	aid = (u16)(i * 32 + j);
	 /* AID > 2006 exceeds the 802.11 limit. Both are explicitly rejected. */
	if (aid == 0 || aid > 2006)
		return 0;

	hapd->sta_aid[i] |= BIT(j);
	wpa_printf(MSG_DEBUG, "MAPC vendor: aid=%u allocated", aid);
	return aid;
}

void mapc_release_vendor_aid(struct hostapd_data *hapd,
			     struct sta_info *sta)
{
	struct mapc_vendor_peer_ctx *vctx;
	u16 v_aid;

	if (!sta)
		return;
	vctx  = &sta->mapc_params.vendor;
	v_aid = vctx->vendor_apid;
	if (v_aid == 0)
		return;

	hapd->sta_aid[v_aid / 32] &= ~BIT(v_aid % 32);
	vctx->vendor_apid = 0;
	wpa_printf(MSG_DEBUG,
		   "MAPC vendor: aid=%u released for " MACSTR,
		   v_aid, MAC2STR(sta->addr));
}

void mapc_vendor_alloc_peer_aid(struct hostapd_data *hapd,
				struct sta_info *sta)
{
	struct mapc_vendor_peer_ctx *vctx;
	u16 v_aid;

	if (!sta)
		return;
	vctx = &sta->mapc_params.vendor;

	if (!vctx->set_vendor_apid || vctx->vendor_apid != 0)
		return;

	v_aid = mapc_alloc_vendor_aid(hapd);
	if (v_aid != 0) {
		vctx->vendor_apid = v_aid;
		wpa_printf(MSG_DEBUG,
			   "MAPC vendor: allocated aid=%u for QCA peer " MACSTR,
			   v_aid, MAC2STR(sta->addr));
	} else {
		wpa_printf(MSG_ERROR,
			   "MAPC vendor: AID pool full for " MACSTR
			   " — Q2Q disabled", MAC2STR(sta->addr));
		vctx->set_vendor_apid = false;
	}
}

int mapc_set_vendor_params(struct hostapd_data *hapd,
			   struct sta_info *sta)
{
	u16 apid_to, apid_from;
	int ret;

	if (!sta || !sta->is_mapc_peer)
		return 0;
	if (!sta->mapc_params.vendor.set_vendor_apid ||
	    !sta->mapc_params.vendor.vendor_apid        ||
	    !sta->mapc_params.vendor.remote_assigned_vendor_apid)
		return 0;

	apid_to   = sta->mapc_params.vendor.vendor_apid;
	apid_from = sta->mapc_params.vendor.remote_assigned_vendor_apid;

	wpa_printf(MSG_DEBUG,
		   "MAPC vendor: set params " MACSTR
		   " q2q_to=%u q2q_from=%u",
		   MAC2STR(sta->addr), apid_to, apid_from);

	ret = nl80211_set_mapc_vendor_params_extn(hapd, sta->addr,
						  apid_to, apid_from);
	if (ret)
		wpa_printf(MSG_ERROR,
			   "MAPC vendor: set params failed for " MACSTR
			   ": %d", MAC2STR(sta->addr), ret);
	return ret;
}

int mapc_get_peer_params(struct hostapd_data *hapd, const u8 *peer_addr,
			 char *reply, size_t reply_size)
{
	struct mapc_peer_params_result res = {};
	struct sta_info *sta;
	const char *state_str  = "unknown";
	u16  param_bitmap      = 0;
	int  cotdma_capable, agreement_cnt = 0;
	static const int ch_mhz[] = { 20, 40, 80, 160, 320 };
	int  ch_width_mhz;
	int  ret;

	/* MLO: resolve to the link hapd that owns this peer's sta_info */
	hapd = mapc_find_link_hapd(hapd, peer_addr);

	ret = nl80211_get_mapc_peer_params_extn(hapd, peer_addr, &res);
	if (ret) {
		wpa_printf(MSG_ERROR,
			   "MAPC vendor: get peer params failed for " MACSTR
			   ": %d", MAC2STR(peer_addr), ret);
		return ret;
	}

	/* Fetch hostapd-side fields not available from FW */
	cotdma_capable = !!(res.cap_bitmap & BIT(MAPC_CAPABILITY_COTDMA_SUPPORT));
	ch_width_mhz   = (res.ch_width < 5) ? ch_mhz[res.ch_width] : 0;

	sta = ap_get_sta(hapd, peer_addr);
	if (sta && sta->is_mapc_peer) {
		param_bitmap  = sta->mapc_params.mapc_parameter_bitmap;
		agreement_cnt = sta->mapc_params.agreement_cnt[MAPC_SCHEME_CO_TDMA];
		switch (sta->mapc_params.peer_state) {
		case MAPC_PEER_STATE_DISCOVERED:
			state_str = (sta->mapc_params.neg_state ==
				     MAPC_NEG_AGR_ESTABLISH_INPROGRESS)
				    ? "negotiating" : "discovered";
			break;
		case MAPC_PEER_STATE_ACTIVE:
			if (sta->mapc_params.neg_state ==
			    MAPC_NEG_AGR_UPDATE_INPROGRESS)
				state_str = "updating";
			else if (sta->mapc_params.neg_state ==
				 MAPC_NEG_AGR_TEARDOWN_INPROGRESS)
				state_str = "teardown";
			else
				state_str = "active";
			break;
		default:
			break;
		}
	}

	ret = os_snprintf(reply, reply_size,
			  "MAPC PEER: " MACSTR " (bss=%s)\n"
			  "  state:           %s\n"
			  "  peer_cap_bitmap: 0x%04x\n"
			  "  param_bitmap:    0x%04x\n\n"
			  "  [cotdma]\n"
			  "    peer_capable:  %d\n"
			  "    agreement_cnt: %d\n"
			  "    apid_to:       %u\n"
			  "    apid_from:     %u\n"
			  "    q2q_apid_to:   %u\n"
			  "    q2q_apid_from: %u\n"
			  "    profile:\n"
			  "      channel_width: %d\n"
			  "      ccfs:          %u\n"
			  "      bss_color:     %u\n"
			  "      dsb:           0x%04x\n"
			  "      rx_txop_ret:   %d\n"
			  "    TxOP Policy:\n"
			  "      primary_ac:        %u\n"
			  "      nbr_prio:          %u\n"
			  "      svc_start:         %u\n"
			  "      svc_interval:      %u\n"
			  "      svc_end:           %u\n"
			  "      latency_threshold: %u\n"
			  "      max_txop:          %u\n"
			  "      min_txop:          %u\n",
			  MAC2STR(peer_addr), hapd->conf->iface,
			  state_str,
			  res.cap_bitmap,
			  param_bitmap,
			  cotdma_capable, agreement_cnt,
			  res.apid_to, res.apid_from,
			  res.q2q_to, res.q2q_from,
			  ch_width_mhz, res.ccfs, res.bss_color,
			  res.dsb, res.rx_txop,
			  res.primary_ac, res.nbr_prio,
			  res.svc_start, res.svc_interval, res.svc_end,
			  res.latency_threshold, res.max_txop, res.min_txop);
	if (os_snprintf_error(reply_size, ret))
		return -1;

	if (res.e2e_entry_count > 0) {
		u8 i;
		int n;

		n = os_snprintf(reply + ret, reply_size - ret,
				"    E2E Config (%u entries):\n",
				res.e2e_entry_count);
		if (n > 0 && !os_snprintf_error((int)(reply_size - ret), n))
			ret += n;

		for (i = 0; i < res.e2e_entry_count && ret < (int)reply_size; i++) {
			const struct mapc_cotdma_e2e_entry *e = &res.e2e_entries[i];

			n = os_snprintf(reply + ret, reply_size - ret,
					"      [%u] qmid=%-5u mode=%s",
					i, e->qmid,
					e->config_mode ? "add" : "remove");
			if (n > 0 && !os_snprintf_error((int)(reply_size - ret), n))
				ret += n;

			if (e->bsta_mac_valid) {
				n = os_snprintf(reply + ret, reply_size - ret,
						" bsta=" MACSTR,
						MAC2STR(e->bsta_mac));
				if (n > 0 && !os_snprintf_error((int)(reply_size - ret), n))
					ret += n;
			}

			n = os_snprintf(reply + ret, reply_size - ret, "\n");
			if (n > 0 && !os_snprintf_error((int)(reply_size - ret), n))
				ret += n;
		}
	}

	return ret;
}
