/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifdef UBUS_SUPPORT
#include <libubus.h>
#undef ARRAY_SIZE
#endif
#include "includes.h"
#include "utils/common.h"
#include "utils/os.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "../wpa_supplicant/wpa_supplicant_i.h"
#include "../wpa_supplicant/sme.h"
#include "../wpa_supplicant/config.h"
#include "../wpa_supplicant/ubus.h"
#include "../wpa_supplicant/bss.h"
#include "../wpa_supplicant/scan.h"
#include "../wpa_supplicant/driver_i.h"
#include "wpa_supplicant_rptr_extn.h"
#include "240mhz.h"
#include "qcn_ie_extn.h"

#define SME_PRE_CONNECT_TIMEOUT 5

/**
 * wpa_ctrl_get_freq_list_extn - Get configured frequency list for interface
 * @wpa_s: Pointer to wpa_supplicant interface
 * @reply: Caller-provided buffer to hold space-separated frequency list
 * @reply_size: Size of @reply in bytes
 *
 * Serialize the configured frequency list into a space-separated string and
 * append a trailing newline. The list is taken from wpa_s->conf->freq_list
 * when present.
 *
 * Return: Number of bytes written to @reply (excluding terminating NUL),
 *         or 0 if there is no configured list.
 */
int wpa_ctrl_get_freq_list_extn(struct wpa_supplicant *wpa_s, char *reply, int reply_size)
{
	if (wpa_s->conf && wpa_s->conf->freq_list) {
		const int *f = wpa_s->conf->freq_list;
		char *pos = reply;
		char *end = reply + reply_size - 2;
		int n;

		pos[0] = '\0';
		for (; *f && pos < end; f++) {
			n = os_snprintf(pos, end - pos, "%s%d",
					(pos == reply) ? "" : " ", *f);
			if (os_snprintf_error(end - pos, n))
				break;
			pos += n;
		}
		if (pos == reply)
			return 0; /* No entries */
		*pos++ = '\n';
		return pos - reply;
	}
	return 0; /* No configured list */
}

/**
 * wpa_ctrl_chan_sw_finished_notify_extn - Handle channel switch completion
 * @wpa_s: Pointer to wpa_supplicant interface
 * @buf: Control interface buffer containing CHAN_SW_FINISHED_NOTIFY
 * @reply: Unused output buffer (reserved for future use)
 * @reply_size: Size of @reply in bytes (unused)
 *
 * Parse the frequency from the notification, decrement the outstanding
 * pre-connect counter, and schedule the cached SME connect work when all
 * pending operations have completed.
 *
 * Return: 0 on success or -1 if the frequency cannot be parsed.
 */
int wpa_ctrl_chan_sw_finished_notify_extn(struct wpa_supplicant *wpa_s, const char *buf, char *reply, int reply_size)
{
	const char *p;
	int freq;
	(void)reply;
	(void)reply_size;

	p = strstr(buf, "freq");
	if (!p)
		return -1;
	p += strlen("freq");
	while (*p == ' ')
		p++;

	if (*p != '=')
		p = strchr(p, '=');
	if (!p)
		return -1;

	p++;
	freq = atoi(p);
	wpa_s->pre_connect_cnt--;
	_wpa_msg(wpa_s, MSG_INFO,
		 "SME: RECVD - CHAN_SW_FINISHED_NOTIFY freq = %d pre_connect_cnt = %d",
		 freq, wpa_s->pre_connect_cnt);
	if (!wpa_s->pre_connect_cnt) {
		if (wpa_s && wpa_s->cache_cwork && wpa_s->cache_cwork->bss)
			sme_schedule_auth_radio_work(wpa_s, wpa_s->cache_cwork);
	}
	return 0;
}

/**
 * wpa_bss_check_5g_320mhz_vendor_ie_extn - Upgrade 5 GHz BSS to 320 MHz
 * @wpa_s: Pointer to wpa_supplicant interface
 * @bss: BSS entry to inspect and update
 *
 * Inspect the QCN vendor-specific IE in a 5 GHz BSS and, when a 240 MHz
 * capability element is present, upgrade the BSS bandwidth to 320 MHz and
 * update center frequency indices and puncturing bitmap accordingly.
 */
void wpa_bss_check_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					    struct wpa_bss *bss)
{
	const struct ieee80211_240mhz_vendor_oper_extn_v2 *oper_240 = NULL;
	const u8 *vendor_ie;
	const u8 *pos;
	size_t elen;
	size_t off = 0;

	if (bss->max_cw != CHAN_WIDTH_160 || !is_5ghz_freq(bss->freq))
		return;

	/* Look for QCN Vendor Specific IE for 5G 320MHz support */
	vendor_ie = wpa_bss_get_vendor_ie(bss, QCN_IE_VENDOR_TYPE);
	if (!vendor_ie) {
		wpa_dbg(wpa_s, MSG_ERROR,
			"5G 320MHz check: QCN Vendor IE not found, keeping 160MHz");
		return;
	}

	/* vendor_ie[1] is length; need at least OUI (3) + OUI Type (1) */
	if (vendor_ie[1] < 4)
		return;

	pos = vendor_ie + 2; /* Skip Element ID and Length */
	elen = vendor_ie[1];

	/* Skip OUI (3 bytes) + OUI Type (1 byte) */
	if (pos[3] != QCN_OUI_TYPE)
		return;

	pos  += 4;
	elen -= 4;

	/* Parse TLV attributes */
	while (off + 2 <= elen) {
		u8 attr_id  = pos[off];
		u8 attr_len = pos[off + 1];

		if (off + 2 + attr_len > elen) {
			wpa_msg(wpa_s, MSG_INFO,
				"5G 320MHz: QCN IE malformed (TLV truncated, id=0x%02x len=%u)",
				attr_id, attr_len);
			break;
		}

		if ((attr_id == QCN_ATTRIB_5GHZ_320MHZ_CSA &&
		     attr_len == QCN_5GHZ_320MHZ_CSA_ELEM_LEN) ||
		    (attr_id == QCN_ATTRIB_HE_240_MHZ_SUPP && !oper_240 &&
		     attr_len <= QCN_HE_240_MHZ_MAX_ELEM_LEN &&
		     attr_len >= sizeof(struct ieee80211_240mhz_vendor_oper_extn_v2))) {
			oper_240 = (const struct ieee80211_240mhz_vendor_oper_extn_v2 *)
				(pos + off + 2);
			if (attr_id == QCN_ATTRIB_5GHZ_320MHZ_CSA)
				break;
		}

		off += 2 + attr_len;
	}

	if (!oper_240)
		return;

	wpa_hexdump(MSG_DEBUG,
		    "5G 320MHz check: 240MHz vendor oper structure",
		    (const u8 *)oper_240,
		    sizeof(struct ieee80211_240mhz_vendor_oper_extn_v2));

	wpa_dbg(wpa_s, MSG_DEBUG,
		"5G 320MHz check: is5ghz240mhz=%u ccfs0=%u ccfs1=%u punct_bitmap=0x%04x",
		oper_240->is5ghz240mhz, oper_240->ccfs0,
		oper_240->ccfs1, le_to_host16(oper_240->punct_bitmap));

	/* Validate 5G 320MHz (240MHz) support */
	if (!oper_240->is5ghz240mhz)
		return;

	wpa_printf(MSG_INFO,
		   "5G 320MHz: Upgrading bandwidth from 160MHz to 320MHz");

	/* Update to 320MHz bandwidth */
	bss->max_cw = CHAN_WIDTH_320;

	/* Update center frequency indices from vendor IE */
	bss->center_freq1_idx = oper_240->ccfs0;
	bss->center_freq2_idx = oper_240->ccfs1;

	/* Update puncturing bitmap from vendor IE */
	bss->punc_bitmap = le_to_host16(oper_240->punct_bitmap);

	wpa_printf(MSG_INFO,
		   "5G 320MHz: Updated BSS " MACSTR " - "
		   "freq=%d center_freq1_idx=%u center_freq2_idx=%u "
		   "punct_bitmap=0x%04x",
		   MAC2STR(bss->bssid), bss->freq,
		   bss->center_freq1_idx, bss->center_freq2_idx,
		   bss->punc_bitmap);
}

bool wpas_sta_cac_5g_320mhz_update_freq_params_extn(enum chan_width width,
					     u8 cf2_idx,
					     struct hostapd_freq_params *params)
{
	if (width != CHAN_WIDTH_320 || !cf2_idx || !params)
		return false;

	params->center_freq1 = 5000 + 5 * cf2_idx;
	params->center_freq2 = 0;

	return true;
}

void wpas_ch_switch_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					    union wpa_event_data *data)
{
	if (!wpa_s || !data || !wpa_s->current_bss)
		return;

	if ((data->ch_switch.ch_width != CHAN_WIDTH_160 &&
	     data->ch_switch.ch_width != CHAN_WIDTH_80) ||
	    !is_5ghz_freq(data->ch_switch.freq))
		return;

	wpa_bss_check_5g_320mhz_vendor_ie_extn(wpa_s, wpa_s->current_bss);
}

/**
 * compute_sec_channel_offset_extn - Derive secondary channel offset
 * @primary_freq: Primary channel center frequency in MHz
 * @center_freq1: Center frequency of the wider channel in MHz
 * @width: Channel width enumeration (20/40/80/160/320 MHz)
 *
 * Compute the secondary channel offset value used in HT/VHT/HE/EHT
 * configuration based on the relative position of the primary channel to
 * the wider channel center.
 *
 * Return: +1 if secondary channel is above primary, -1 if below, or 0 if
 *         there is no secondary channel or the configuration is invalid.
 */
int compute_sec_channel_offset_extn(int primary_freq, int center_freq1, enum chan_width width)
{
	int offset_from_center;

	/* For 20 MHz bandwidth, there is no secondary channel */
	if (width == CHAN_WIDTH_20)
		return 0;

	/* Validate inputs */
	if (primary_freq <= 0 || center_freq1 <= 0)
		return 0;

	/* Calculate the offset between primary and center frequency */
	offset_from_center = primary_freq - center_freq1;

	switch (width) {
	case CHAN_WIDTH_40:
		if (offset_from_center == -10)
			return 1;  /* Secondary above primary */
		else if (offset_from_center == 10)
			return -1; /* Secondary below primary */
		break;

	case CHAN_WIDTH_80:
		if (offset_from_center == -30 || offset_from_center == 10)
			return 1;  /* Secondary above primary */
		else if (offset_from_center == -10 || offset_from_center == 30)
			return -1; /* Secondary below primary */
		break;

	case CHAN_WIDTH_160:
		if (offset_from_center == -70 || offset_from_center == 50 ||
		    offset_from_center == -30 || offset_from_center == 10)
			return 1;  /* Secondary above primary */
		else if (offset_from_center == 70 || offset_from_center == -50 ||
			 offset_from_center == 30 || offset_from_center == -10)
			return -1; /* Secondary below primary */
		break;

	case CHAN_WIDTH_320:
		if (offset_from_center == -150 || offset_from_center == 130 ||
		    offset_from_center == -110 || offset_from_center == 90 ||
		    offset_from_center == -70 || offset_from_center == 50 ||
		    offset_from_center == -30 || offset_from_center == 10)
			return 1;  /* Secondary above primary */
		else if (offset_from_center == 150 || offset_from_center == -130 ||
			 offset_from_center == 110 || offset_from_center == -90 ||
			 offset_from_center == 70 || offset_from_center == -50 ||
			 offset_from_center == 30 || offset_from_center == -10)
			return -1; /* Secondary below primary */
		break;

	default:
		/* Unsupported bandwidth */
		return 0;
	}

	/* Invalid configuration */
	return 0;
}
/**
 * compute_dfs_for_chanwidth_extn - Compute DFS status for channel
 * @freq: Primary channel center frequency in MHz
 * @chanwidth: Channel width enumeration (20/40/80/160/320 MHz)
 *
 * Determine whether the given primary channel (and, for 160 MHz bandwidth,
 * the secondary 80 MHz segment) falls on DFS frequencies. This is used to
 * populate DFS state in interface status for Independent Repeater flows.
 *
 * Return: true if the primary and/or secondary 80 MHz channel requires DFS,
 *         false otherwise.
 */
bool compute_dfs_for_chanwidth_extn(int freq, int chanwidth)
{
	bool is_dfs;

	is_dfs = ieee80211_is_dfs(freq, NULL, 0);
	if (!is_dfs && channel_width_to_int(chanwidth) == 160)
		is_dfs = ieee80211_is_dfs(freq + 80, NULL, 0);

	return is_dfs;
}

/**
 * wpa_get_bss_channel_oper_info_extn - Extract BSS channel operating info
 * @wpa_s: Pointer to wpa_supplicant interface
 * @bss: BSS entry whose IEs are parsed
 *
 * Parse EHT, HE and VHT operation IEs to derive maximum channel width,
 * center frequency indices and puncturing bitmap, updating the BSS fields
 * used by repeater and channel selection logic.
 */
void wpa_get_bss_channel_oper_info_extn(struct wpa_supplicant *wpa_s,
					struct wpa_bss *bss)
{
	const u8 *ie;

	/* Parse EHT Operation first:
	 * Ensures punctured channel bitmap (disabled_chan_bitmap) and EHT-specific
	 * params are captured before VHT/HE to avoid missing or being overwritten. */
	ie = get_ie_ext(bss->ies, bss->ie_len, WLAN_EID_EXT_EHT_OPERATION);
	if (ie && ie[1] >= 1) {
		const struct ieee80211_eht_operation *eht_oper =
			(const struct ieee80211_eht_operation *) (ie + 3);

		/* Clear stale puncturing bitmap. VHT IEs do not define puncturing,
		 * so if this BSS has no EHT Operation, drop any leftover bitmap. */
		if (bss->punc_bitmap > 0)
			bss->punc_bitmap = 0;

		if (eht_oper->oper_params & EHT_OPER_INFO_PRESENT) {
			switch(eht_oper->oper_info.control) {
			case EHT_OPER_CHANNEL_WIDTH_20MHZ:
				bss->max_cw = CHAN_WIDTH_20;
				break;
			case EHT_OPER_CHANNEL_WIDTH_40MHZ:
				bss->max_cw = CHAN_WIDTH_40;
				break;
			case EHT_OPER_CHANNEL_WIDTH_80MHZ:
				bss->max_cw = CHAN_WIDTH_80;
				break;
			case EHT_OPER_CHANNEL_WIDTH_160MHZ:
				bss->max_cw = CHAN_WIDTH_160;
				break;
			case EHT_OPER_CHANNEL_WIDTH_320MHZ:
				bss->max_cw = CHAN_WIDTH_320;
				break;
			default:
				bss->max_cw = CHAN_WIDTH_20;
				break;
			}
			bss->center_freq1_idx = eht_oper->oper_info.ccfs0;
			bss->center_freq2_idx = eht_oper->oper_info.ccfs1;
			if (eht_oper->oper_params & EHT_OPER_DISABLED_SUBCHAN_BITMAP_PRESENT)
				bss->punc_bitmap = eht_oper->oper_info.disabled_chan_bitmap;
			else
				bss->punc_bitmap = 0;

			wpa_dbg(wpa_s, MSG_DEBUG, "WLAN_EID_EXT_EHT_OPERATION:"
				"width = %d center_freq1_idx = %d center_freq2_idx = %d"
				"eht_oper->oper_info.disabled_chan_bitmap = %d",
				bss->max_cw, bss->center_freq1_idx, bss->center_freq2_idx,
				eht_oper->oper_info.disabled_chan_bitmap);
			return;
		}
	}

	ie = wpa_bss_get_ie(bss, WLAN_EID_VHT_OPERATION);
	if (ie && ie[1] >= 1) {
		const struct ieee80211_vht_operation *vht_oper =
			(struct ieee80211_vht_operation *) (ie + 2);

		switch (vht_oper->vht_op_info_chwidth) {
		case CHANWIDTH_80MHZ:
			bss->center_freq1_idx = vht_oper->vht_op_info_chan_center_freq_seg0_idx;
			bss->center_freq2_idx = vht_oper->vht_op_info_chan_center_freq_seg1_idx;
			if (bss->center_freq2_idx &&
			    abs(bss->center_freq2_idx - bss->center_freq1_idx) == 8)
				bss->max_cw = CHAN_WIDTH_160;
			else if (bss->center_freq2_idx)
				bss->max_cw = CHAN_WIDTH_80P80;
			else
				bss->max_cw = CHAN_WIDTH_80;
			break;
		case CHANWIDTH_160MHZ:
			bss->max_cw = CHAN_WIDTH_160;
			break;
		case CHANWIDTH_80P80MHZ:
			bss->max_cw = CHAN_WIDTH_80P80;
			break;
		default:
			/* VHT fallback width via HT Capabilities:
			 * If VHT width is inconclusive, use HT 'Supported Channel
			 * Width Set' to choose 40 MHz vs 20 MHz instead of
			 * defaulting to 20 MHz. */
			ie = wpa_bss_get_ie(bss, WLAN_EID_HT_CAP);
			if (ie && ie[1] >= 1) {
				if(WPA_GET_LE16(ie + 2) &
					HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET) {
					bss->max_cw = CHAN_WIDTH_40;
				} else {
					bss->max_cw = CHAN_WIDTH_20;
				}
			}
			break;
		}

		bss->center_freq1_idx = vht_oper->vht_op_info_chan_center_freq_seg0_idx;
		bss->center_freq2_idx = vht_oper->vht_op_info_chan_center_freq_seg1_idx;

		wpa_bss_check_5g_320mhz_vendor_ie_extn(wpa_s, bss);

		wpa_dbg(wpa_s, MSG_DEBUG, "WLAN_EID_VHT_OPERATION: width = %d"
			"center_freq1_idx = %d *center_freq2_idx = %d",
			bss->max_cw, bss->center_freq1_idx, bss->center_freq2_idx);
		return;
	}

	ie = get_ie_ext(bss->ies, bss->ie_len, WLAN_EID_EXT_HE_OPERATION);
	if (ie && ie[1] >= 1) {
		const struct ieee80211_he_operation *he_oper =
			(const struct ieee80211_he_operation *) (ie + 2);
		int offset = 0;
		u8 he_oper_chwidth;

		if (he_oper->he_oper_params & HE_OPERATION_VHT_OPER_INFO)
			offset = 3;
		if (he_oper->he_oper_params & HE_OPERATION_COHOSTED_BSS)
			offset += 1;
		if (he_oper->he_oper_params & HE_OPERATION_6GHZ_OPER_INFO) {
			const struct ieee80211_he_6ghz_oper_info *oper_info =
				(const struct ieee80211_he_6ghz_oper_info *)
				(ie + 2 + sizeof(struct ieee80211_he_operation) + 1);

			he_oper_chwidth = (oper_info->control &
					HE_6GHZ_OPER_INFO_CTRL_CHAN_WIDTH_MASK);

			switch (he_oper_chwidth) {
			case IEEE80211_6GHZ_OP_CHWIDTH_20:
				bss->max_cw = CHAN_WIDTH_20;
				break;
			case IEEE80211_6GHZ_OP_CHWIDTH_40:
				bss->max_cw = CHAN_WIDTH_40;
				break;
			case IEEE80211_6GHZ_OP_CHWIDTH_80:
				bss->max_cw = CHAN_WIDTH_80;
				break;
			case IEEE80211_6GHZ_OP_CHWIDTH_160_80_80:
				bss->max_cw = CHAN_WIDTH_160;
				break;
			default:
				bss->max_cw = CHAN_WIDTH_20;
				break;
			}
			bss->center_freq1_idx =
				oper_info->chan_center_freq_seg0;
			bss->center_freq2_idx =
				oper_info->chan_center_freq_seg1;

			/* Clear stale puncturing bitmap. HE IEs do not define puncturing,
			 * so if this BSS has no EHT Operation, drop any leftover bitmap. */
			if (bss->punc_bitmap > 0)
				bss->punc_bitmap = 0;

			wpa_dbg(wpa_s, MSG_DEBUG, "WLAN_EID_EXT_HE_OPERATION:"
				"width = %d *center_freq1_idx = %d *center_freq2_idx = %d",
				bss->max_cw, bss->center_freq1_idx, bss->center_freq2_idx);
			return;
		}
	}
	return;
}

/**
 * wpa_supp_pre_connect_state_handle_extn - Emit PRE_CONNECT notifications
 * @wpa_s: Pointer to wpa_supplicant interface
 * @bss: Target BSS entry selected for connection
 *
 * Send per-link (for MLO) or legacy channel parameters to the control
 * interface during PRE_CONNECT handling in Independent Repeater mode.
 * This allows the AP side to align channels and bandwidth before SME
 * authentication work is scheduled.
 */
void wpa_supp_pre_connect_state_handle_extn(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	u8 i, msg[2048];
	u8 op_class, channel;
	int center_freq1, center_freq2;
	s8 hw_idx;
	bool is_dfs = false;
	char links_buf[1024];
	char *pos, *end;
	bool first;

	if (wpa_s && bss) {
		if (!is_zero_ether_addr(bss->mld_addr)) {
			/*
			 * MLO Pre-connect Handling
			 *
			 * Send a single PRE-CONNECTING message per MLD, aggregating
			 * all per-link information in a comma-separated list. Each
			 * link contributes one tuple of:
			 *   hw_idx,freq,c_freq1,c_freq2,width,punc_bitmap,is_dfs
			 */
			pos = links_buf;
			end = links_buf + sizeof(links_buf) - 1;
			first = true;

			links_buf[0] = '\0';
			for_each_link(bss->valid_links, i) {
				ieee80211_freq_to_channel_ext(bss->mld_links[i].freq,
							      0, 1, &op_class, &channel);
				center_freq1 = ieee80211_chan_to_freq(NULL,
				op_class, bss->mld_links[i].center_freq1_idx);
				center_freq2 = ieee80211_chan_to_freq(NULL,
				op_class, bss->mld_links[i].center_freq2_idx);
				hw_idx = wpa_get_hw_idx_by_freq(wpa_s,
								bss->mld_links[i].freq);
				is_dfs = ieee80211_is_dfs(bss->mld_links[i].freq,
							  NULL, 0);
				wpa_printf(MSG_INFO, "MLO hw_idx = %d ifname = %s"
					   "freq = %d center_freq1 = %d center_freq2 = %d width = %d"
					   "punc_bitmap = %d is_dfs = %d", hw_idx, wpa_s->ifname,
					   bss->mld_links[i].freq, center_freq1, center_freq2,
					   bss->mld_links[i].width, bss->mld_links[i].punc_bitmap, is_dfs);

				if (!first) {
					if (pos + 1 >= end)
						break;
					*pos++ = ';';
				}
				first = false;

				/* Count one pre-connect operation per MLD */
				wpa_s->pre_connect_cnt++;

				int n = os_snprintf(pos, end - pos,
						"%d,%d,%d,%d,%d,%d,%d",
						hw_idx,
						bss->mld_links[i].freq,
						center_freq1,
						center_freq2,
						bss->mld_links[i].width,
						bss->mld_links[i].punc_bitmap,
						is_dfs);
				if (os_snprintf_error(end - pos, n))
					break;
				pos += n;
			}

			if (!first) {
				os_snprintf((char *)msg, sizeof(msg),
					"%s ifname = %s links = %s",
					WPA_EVENT_PRE_CONNECTING, wpa_s->ifname,
					links_buf);
				wpa_msg_ctrl(wpa_s, MSG_INFO, "%s", (char *)msg);
			}
		} else {
			/* Legacy Pre-connect Handling (single-link "links" entry) */
			ieee80211_freq_to_channel_ext(bss->freq, 0, 1,
						      &op_class, &channel);
			center_freq1 = ieee80211_chan_to_freq(NULL, op_class,
							      bss->center_freq1_idx);
			center_freq2 = ieee80211_chan_to_freq(NULL, op_class,
							      bss->center_freq2_idx);
			hw_idx = wpa_get_hw_idx_by_freq(wpa_s, bss->freq);
			is_dfs = ieee80211_is_dfs(bss->freq, NULL, 0);

			wpa_printf(MSG_INFO, "Legacy hw_idx = %d ifname = %s"
				   "freq = %d center_freq1 = %d center_freq2 = %d width = %d"
				   "punc_bitmap = %d is_dfs = %d", hw_idx, wpa_s->ifname,
				   bss->freq, center_freq1, center_freq2, bss->max_cw,
				   bss->punc_bitmap, is_dfs);

			os_snprintf(links_buf, sizeof(links_buf),
				    "%d,%d,%d,%d,%d,%d,%d",
				    hw_idx,
				    bss->freq,
				    center_freq1,
				    center_freq2,
				    bss->max_cw,
				    bss->punc_bitmap,
				    is_dfs);

			os_snprintf((char *)msg, sizeof(msg),
				"%s ifname = %s links = %s",
				WPA_EVENT_PRE_CONNECTING, wpa_s->ifname,
				links_buf);
			wpa_msg_ctrl(wpa_s, MSG_INFO, "%s", (char *)msg);
			wpa_s->pre_connect_cnt++;
		}
	}
}

/**
 * sme_pre_connect_timer_extn - Handle pre-connect timeout for repeater flows
 * @eloop_ctx: Eloop context, cast to struct wpa_supplicant pointer
 * @timeout_ctx: Unused timeout context (reserved for future use)
 *
 * Invoked by the eloop timeout mechanism while in WPA_PRE_CONNECT state to
 * detect when pre-connection attempts have timed out. On timeout, resets the
 * pre_connect counter and requests a fresh scan to continue repeater connect
 * logic.
 */
void sme_pre_connect_timer_extn(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->wpa_state == WPA_PRE_CONNECT) {
		wpa_msg(wpa_s, MSG_DEBUG, "SME: Pre-Connection timeout");
		wpa_s->pre_connect_cnt = 0;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
}

/**
 * wpa_bss_update_link_rnr_ap_info_extn - Update MLD link info from RNR
 * @wpa_s: Pointer to wpa_supplicant interface
 * @bss: Base BSS entry representing the MLD AP
 * @bssid_ptr: Pointer to the neighbor AP BSSID from RNR
 * @ap_info: Neighbor AP info (op_class/channel) taken from RNR element
 * @mld_params: Pointer to MLD parameter field from the RNR TBTT info
 * @link_id: Link ID index to update in the MLD link array
 *
 * Use Reduced Neighbor Report (RNR) information to update per-link
 * information for Multi-Link Device (MLD) APs in the BSS structure.
 * When a neighbor BSS entry is available, also populate channel width,
 * center frequency indices and puncturing bitmap for the link.
 */
void wpa_bss_update_link_rnr_ap_info_extn(struct wpa_supplicant *wpa_s,
					  struct wpa_bss *bss,
					  const u8 *bssid_ptr,
					  const struct ieee80211_neighbor_ap_info *ap_info,
					  const u8 *mld_params,
					  u8 link_id)
{
	struct mld_link *l;
	struct wpa_bss *neigh_bss = NULL;

	if (bss)
		neigh_bss = wpa_bss_get(wpa_s, bssid_ptr, bss->ssid, bss->ssid_len);
	if (!neigh_bss)
		neigh_bss = wpa_bss_get_bssid(wpa_s, bssid_ptr);

	bss->valid_links |= BIT(link_id);
	l = &bss->mld_links[link_id];
	os_memcpy(l->bssid, bssid_ptr, ETH_ALEN);
	l->disabled = mld_params[2] & RNR_TBTT_INFO_MLD_PARAM2_LINK_DISABLED;
	l->freq = ieee80211_chan_to_freq(NULL, ap_info->op_class, ap_info->channel);

	if (neigh_bss) {
		l->center_freq1_idx = neigh_bss->center_freq1_idx;
		l->center_freq2_idx = neigh_bss->center_freq2_idx;
		l->width = neigh_bss->max_cw;
		l->punc_bitmap = neigh_bss->punc_bitmap;
		os_get_reltime(&neigh_bss->last_update);
		wpa_printf(MSG_DEBUG, "%s: link_id=%u freq=%d cf1=%d cf2=%d width=%d punc=%d",
			   __func__, link_id, l->freq, l->center_freq1_idx, l->center_freq2_idx,
			   l->width, l->punc_bitmap);
	} else {
		wpa_printf(MSG_DEBUG, "%s: link_id=%u freq=%d (no neigh_bss, "
			   "channel info unavailable)", __func__, link_id, l->freq);
	}
}

/**
 * wpa_supplicant_start_sta_scan - Handle repeater AP ACS/CSA timeout for repeater
 * @eloop_ctx: Eloop context, cast to struct wpa_supplicant pointer
 * @timeout_ctx: Unused timeout context (reserved for future use)
 *
 * Invoked by the eloop timeout mechanism while in WPA_DISCONNECTED state to
 * detect when either repeater AP ACS attempts have timed out or repeater AP
 * CSA has timed out. On timeout, set the acs_complete flag or reset the
 * hold_scan_csa flag accordingly and requests a fresh scan to
 * continue repeater connect logic.
 */
void wpa_supplicant_start_sta_scan(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->wpa_state == WPA_DISCONNECTED) {
		if (IS_CSH_IGNORE_CSA_DFS_ENABLED(wpa_s->conf->cswopts) &&
		    wpa_s->hold_scan_csa) {
			wpa_msg(wpa_s, MSG_DEBUG, "Repeater CSA timed out for CSwOpts %s",
				convert_cswopts_to_str(CSH_OPT_IGNORE_CSA_DFS));
			wpa_s->hold_scan_csa = false;
		} else {
			wpa_msg(wpa_s, MSG_DEBUG, "Repeater ACS timeout");
			wpa_s->acs_complete = 1;
		}
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
}

/**
 * wpa_config_process_cswopts_extn - Parse and apply CSwOpts from config file
 * @config: Pointer to wpa_config structure
 * @line: Line number in the configuration file (for error messages)
 * @pos: String value to parse (decimal or hex with 0x prefix)
 *
 * Validate and store the Channel Switch Options bitmap from the configuration
 * file into wpa_config::cswopts.
 *
 * Return: 0 on success, -1 on invalid value.
 */
int wpa_config_process_cswopts_extn(struct wpa_config *config, int line,
				    const char *pos)
{
	long int val = strtol(pos, NULL, 0);

	if (!cswopts_validate(val)) {
		wpa_printf(MSG_ERROR,
			   "Line %d: Invalid CSwOpts value 0x%lx: valid mask is 0x%x",
			   line, val, CSH_OPT_VALID_MASK);
		return -1;
	}

	config->cswopts = (unsigned int)val;
	wpa_printf(MSG_INFO, "CSwOpts set to 0x%x", config->cswopts);
	if (IS_CSH_APRIORI_NEXT_CHANNEL_ENABLED(config->cswopts))
		wpa_printf(MSG_INFO,
			   "CSwOpts: Apriori next channel (0x40) - Apriori channel selection feature is not yet supported");

	return 0;
}

/** wpas_set_dfs_state_freq - Set DFS state for a single 20 MHz channel
 * @wpa_s: wpa_supplicant context
 * @freq: Channel frequency in MHz
 * @state: New DFS state (HOSTAPD_CHAN_DFS_UNAVAILABLE or HOSTAPD_CHAN_DFS_USABLE)
 *
 * Iterates over all hardware modes and sets the DFS state bits in chan->flag
 * for the channel matching @freq.  Only acts on channels that have the
 * HOSTAPD_CHAN_RADAR flag set.
 *
 * Returns 1 if the channel was found and updated, 0 otherwise.
 */
int wpas_set_dfs_state_freq(struct wpa_supplicant *wpa_s, int freq, u32 state)
{
	int i, j;

	if (!wpa_s->hw.modes || !wpa_s->hw.num_modes)
		return 0;

	for (j = 0; j < wpa_s->hw.num_modes; j++) {
		struct hostapd_hw_modes *mode = &wpa_s->hw.modes[j];

		for (i = 0; i < mode->num_channels; i++) {
			struct hostapd_channel_data *chan = &mode->channels[i];

			if (chan->freq == freq && (chan->flag & HOSTAPD_CHAN_RADAR)) {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"DFS: wpas_set_dfs_state_freq: "
					"freq=%d MHz old_dfs_state=0x%X new_dfs_state=0x%X",
					freq,
					chan->flag & HOSTAPD_CHAN_DFS_MASK,
					state);
				chan->flag &= ~HOSTAPD_CHAN_DFS_MASK;
				chan->flag |= state;
				return 1;
			}
		}
	}
	wpa_printf(MSG_WARNING,
		   "DFS: wpas_set_dfs_state_freq: cannot set state for "
		   "freq %d MHz (not a radar channel or not found)", freq);
	return 0;
}

/**
 * wpa_supplicant_ctrl_iface_set_cswopts_extn - Handle SET CSwOpts command
 * @wpa_s: Pointer to wpa_supplicant interface
 * @value: String value to parse (decimal or hex with 0x prefix)
 *
 * Validate and apply the Channel Switch Options bitmap from the SET control
 * interface command. A value of 0 clears all bits; non-zero values are OR'd
 * into the existing bitmap.
 *
 * Return: 0 on success, -1 on invalid value.
 */
int wpa_supplicant_ctrl_iface_set_cswopts_extn(struct wpa_supplicant *wpa_s,
					       const char *value)
{
	long int val = strtol(value, NULL, 0);

	if (!cswopts_validate(val)) {
		wpa_printf(MSG_ERROR,
			   "Invalid CSwOpts value 0x%lx: valid mask is 0x%x",
			   val, CSH_OPT_VALID_MASK);
		return -1;
	}

	if (val == 0)
		wpa_s->conf->cswopts = 0;
	else {
		if ((IS_CSH_RCSA_TO_UPLINK_ENABLED(*value) ||
		     IS_CSH_PROCESS_RCSA_ENABLED(*value)) &&
		    wpa_s->conf->uplink_csa) {
			wpa_printf(MSG_INFO, "RCSA and Uplink CSA cann't co-exists! disabling");
			return -1;
		}

		wpa_s->conf->cswopts |= (unsigned int)val;
	}

	wpa_printf(MSG_INFO, "Updated CSwOpts to 0x%x", wpa_s->conf->cswopts);
	if (IS_CSH_APRIORI_NEXT_CHANNEL_ENABLED(wpa_s->conf->cswopts))
		wpa_printf(MSG_INFO,
			   "CSwOpts: Apriori next channel (0x40) - Apriori channel selection feature is not yet supported");

	return 0;
}

/** wpas_set_dfs_state - Set DFS state for all sub-channels of a channel block
 * @wpa_s: wpa_supplicant context
 * @freq: Primary channel frequency in MHz
 * @ht_enabled: Whether HT is enabled (unused, kept for API symmetry)
 * @chan_offset: HT40 secondary channel offset (unused, kept for API symmetry)
 * @chan_width: Channel width (enum chan_width)
 * @cf1: Center frequency 1 in MHz
 * @cf2: Center frequency 2 in MHz (non-zero for 80P80 and 5G 240 MHz)
 * @state: New DFS state (HOSTAPD_CHAN_DFS_UNAVAILABLE or HOSTAPD_CHAN_DFS_USABLE)
 * @radar_bitmap: Bitmap of affected sub-channels (0 = all sub-channels)
 *
 * Mirrors the logic of set_dfs_state() in src/ap/dfs.c but operates on
 * wpa_s->hw.modes instead of iface->current_mode.  Used in STA-only mode
 * (no AP iface) to keep the channel table in sync with the kernel NOL state.
 *
 * For 5G 320MHz-2 (240 MHz, non-contiguous):
 *   cf1 = center of the 160 MHz segment (8 x 20 MHz sub-channels)
 *   cf2 = center of the  80 MHz segment (4 x 20 MHz sub-channels)
 *   radar_bitmap bits  0-7  -> 160 MHz segment sub-channels
 *   radar_bitmap bits  8-11 ->  80 MHz segment sub-channels
 *
 * Returns the number of channels successfully updated.
 */
int wpas_set_dfs_state(struct wpa_supplicant *wpa_s, int freq,
		       int ht_enabled, int chan_offset, int chan_width,
		       int cf1, int cf2, u32 state, u16 radar_bitmap)
{
	int n_chans = 1, i;
	int frequency = freq;
	int frequency2 = 0;
	int ret = 0;

	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		n_chans = 1;
		if (frequency == 0)
			frequency = cf1;
		break;
	case CHAN_WIDTH_40:
		n_chans = 2;
		frequency = cf1 - 10;
		break;
	case CHAN_WIDTH_80:
		n_chans = 4;
		frequency = cf1 - 30;
		break;
	case CHAN_WIDTH_80P80:
		n_chans = 4;
		frequency = cf1 - 30;
		frequency2 = cf2 - 30;
		break;
	case CHAN_WIDTH_160:
		n_chans = 8;
		frequency = cf1 - 70;
		break;
	default:
		if (!hostapd_get_n_chans_and_frequency_extn(
			hostapd_get_oper_chwidth_from_width_extn(
				channel_width_to_int(chan_width)),
			cf1, &n_chans, &frequency))
			break;

		wpa_printf(MSG_INFO,
				   "DFS: wpas_set_dfs_state: chan_width %d not supported, "
				   "treating as 20 MHz",
				   chan_width);
		n_chans = 1;
		if (frequency == 0)
			frequency = cf1;
		break;
	}

	wpa_dbg(wpa_s, MSG_DEBUG,
			"DFS: wpas_set_dfs_state: start_freq=%d MHz n_chans=%d "
			"state=0x%X radar_bitmap=0x%04X",
			frequency, n_chans, state, radar_bitmap);

	for (i = 0; i < n_chans; i++) {
		if (radar_bitmap && state == HOSTAPD_CHAN_DFS_UNAVAILABLE) {
			if (radar_bitmap & (1 << i)) {
				wpa_dbg(wpa_s, MSG_DEBUG,
						"DFS: wpas_set_dfs_state: marking "
						"freq=%d MHz UNAVAILABLE (bit %d set)",
						frequency, i);
				ret += wpas_set_dfs_state_freq(wpa_s, frequency, state);
			} else {
				wpa_dbg(wpa_s, MSG_DEBUG,
						"DFS: wpas_set_dfs_state: skipping "
						"freq=%d MHz (bit %d not set)",
						frequency, i);
			}
			frequency += 20;
			if (chan_width == CHAN_WIDTH_80P80) {
				if (radar_bitmap & (1 << (i + 4)))
					ret += wpas_set_dfs_state_freq(wpa_s, frequency2, state);
				frequency2 += 20;
			}
		} else {
			ret += wpas_set_dfs_state_freq(wpa_s, frequency, state);
			frequency += 20;
			if (chan_width == CHAN_WIDTH_80P80) {
				ret += wpas_set_dfs_state_freq(wpa_s, frequency2, state);
				frequency2 += 20;
			}
		}
	}

	return ret;
}

/**
 * wpas_dfs_radar_detected_sta_mode - Handle radar detection in STA-only mode
 * @wpa_s: wpa_supplicant context
 * @radar: DFS event data from the driver
 *
 * Called when EVENT_DFS_RADAR_DETECTED is received but no AP/mesh iface is
 * present.  Replicates the hostapd_dfs_radar_detected -> set_dfs_state ->
 * set_dfs_state_freq path directly on wpa_s->hw.modes so that the channel
 * table reflects the NOL state and subsequent BSS selection correctly avoids
 * radar-impacted channels.
 *
 * When device-level parameters differ from the operating bandwidth (e.g.,
 * EHT 320 MHz device operating at 160 MHz), the device parameters are used
 * to determine which sub-channels are affected, matching hostapd behaviour.
 */
void wpas_dfs_radar_detected_sta_mode(struct wpa_supplicant *wpa_s,
				      struct dfs_event *radar)
{
	bool use_device_params =
		radar->cf_device && radar->chan_width_device &&
		radar->chan_width_device != radar->chan_width &&
		radar->cf_device != radar->cf1;

	wpa_dbg(wpa_s, MSG_DEBUG,
			"DFS: radar detected on %d MHz (no AP iface) - "
			"updating hw channel states directly: "
			"ht_enabled=%d chan_offset=%d chan_width=%d "
			"cf1=%d cf2=%d radar_bitmap=0x%04X "
			"chan_width_device=%d cf_device=%d",
			radar->freq, radar->ht_enabled, radar->chan_offset,
			radar->chan_width, radar->cf1, radar->cf2,
			radar->radar_bitmap,
			radar->chan_width_device, radar->cf_device);

	if (use_device_params) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"DFS: using device params for NOL update: "
			"chan_width_device=%d cf_device=%d",
			radar->chan_width_device, radar->cf_device);
		wpas_set_dfs_state(wpa_s, radar->freq,
				   radar->ht_enabled, radar->chan_offset,
				   radar->chan_width_device, radar->cf_device,
				   radar->cf2,
				   HOSTAPD_CHAN_DFS_UNAVAILABLE,
				   radar->radar_bitmap);
	} else {
		wpas_set_dfs_state(wpa_s, radar->freq,
				   radar->ht_enabled, radar->chan_offset,
				   radar->chan_width, radar->cf1, radar->cf2,
				   HOSTAPD_CHAN_DFS_UNAVAILABLE,
				   radar->radar_bitmap);
	}
}

/**
 * wpas_dfs_nop_finished_sta_mode - Handle NOP expiry in STA-only mode
 * @wpa_s: wpa_supplicant context
 * @radar: DFS event data from the driver
 *
 * Called when EVENT_DFS_NOP_FINISHED is received but no AP/mesh iface is
 * present.  Replicates the hostapd_dfs_nop_finished -> set_dfs_state ->
 * set_dfs_state_freq path directly on wpa_s->hw.modes so that the channel
 * table reflects the USABLE state once the Non-Occupancy Period has expired,
 * allowing the STA to consider these channels again for future connections.
 *
 * Note: radar_bitmap is not used for NOP-finished (channels become usable
 * again unconditionally), matching hostapd behaviour.
 */
void wpas_dfs_nop_finished_sta_mode(struct wpa_supplicant *wpa_s,
				    struct dfs_event *radar)
{
	bool use_device_params =
		radar->cf_device && radar->chan_width_device &&
		radar->chan_width_device != radar->chan_width &&
		radar->cf_device != radar->cf1;

	wpa_dbg(wpa_s, MSG_DEBUG,
			"DFS: NOP finished on %d MHz (no AP iface) - "
			"updating hw channel states directly: "
			"ht_enabled=%d chan_offset=%d chan_width=%d "
			"cf1=%d cf2=%d chan_width_device=%d cf_device=%d",
			radar->freq, radar->ht_enabled, radar->chan_offset,
			radar->chan_width, radar->cf1, radar->cf2,
			radar->chan_width_device, radar->cf_device);

	if (use_device_params) {
		wpa_dbg(wpa_s, MSG_DEBUG,
				"DFS: using device params for USABLE update: "
				"chan_width_device=%d cf_device=%d",
				radar->chan_width_device, radar->cf_device);
		wpas_set_dfs_state(wpa_s, radar->freq,
				   radar->ht_enabled, radar->chan_offset,
				   radar->chan_width_device, radar->cf_device,
				   radar->cf2,
				   HOSTAPD_CHAN_DFS_USABLE, 0);
	} else {
		wpas_set_dfs_state(wpa_s, radar->freq,
				   radar->ht_enabled, radar->chan_offset,
				   radar->chan_width, radar->cf1, radar->cf2,
				   HOSTAPD_CHAN_DFS_USABLE, 0);
	}
}

/**
 * wpas_is_chan_nol_extn - Check if a frequency is in the NOL
 * @wpa_s: wpa_supplicant context
 * @freq: Frequency in MHz
 * Returns: true if the channel is marked HOSTAPD_CHAN_DFS_UNAVAILABLE,
 *          false otherwise
 *
 * Uses chan->flag DFS bits directly instead of the legacy nol_flag field.
 */
static bool wpas_is_chan_nol_extn(struct wpa_supplicant *wpa_s, int freq)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int i, j;

	if (!wpa_s->hw.modes || !wpa_s->hw.num_modes)
		return 0;

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		mode = &wpa_s->hw.modes[i];

		for (j = 0; j < mode->num_channels; j++) {
			chan = &mode->channels[j];

			if (chan->freq == freq &&
				(chan->flag & HOSTAPD_CHAN_RADAR) &&
				(chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
					HOSTAPD_CHAN_DFS_UNAVAILABLE)
				return true;
		}
	}
	return false;
}

/**
 * wpas_freq_range_uses_nol_extn - Check if any 20 MHz sub-channel in a
 *   contiguous bandwidth block is in the NOL.
 * @wpa_s: wpa_supplicant context
 * @center_freq: Center frequency of the block in MHz
 * @bandwidth: Bandwidth in MHz (20, 40, 80, 160, 320)
 * Returns: true if any channel in the range is in NOL, false otherwise
 *
 * 20 MHz channel centers within a block of bandwidth @bandwidth centered
 * at @center_freq are at:
 *   center - bw/2 + 10,  center - bw/2 + 30,  ...,  center + bw/2 - 10
 * i.e. start = center - bw/2 + 10,  end = center + bw/2 - 10.
 */
static bool wpas_freq_range_uses_nol_extn(struct wpa_supplicant *wpa_s,
					  int center_freq, int bandwidth)
{
	int start_freq, end_freq, freq;

	if (bandwidth <= 20)
		return wpas_is_chan_nol_extn(wpa_s, center_freq);

	start_freq = center_freq - (bandwidth / 2) + 10;
	end_freq = center_freq + (bandwidth / 2) - 10;

	for (freq = start_freq; freq <= end_freq; freq += 20) {
		if (wpas_is_chan_nol_extn(wpa_s, freq)) {
			wpa_printf(MSG_DEBUG,
				   "NOL: Freq %d in range [%d-%d] (bw=%d, cf=%d) is in NOL",
				   freq, start_freq, end_freq, bandwidth, center_freq);
			return true;
		}
	}

	return false;
}

/**
 * wpas_get_channel_info_extn - Extract channel info from frequency and indices
 * @freq: Primary frequency in MHz
 * @width: Channel width enum
 * @center_freq1_idx: Center frequency 1 index
 * @center_freq2_idx: Center frequency 2 index
 * @center_freq1: Output for center frequency 1 in MHz
 * @center_freq2: Output for center frequency 2 in MHz (80+80 only)
 * Returns: Bandwidth in MHz (20, 40, 80, 160, 320), or -1 on error
 *
 * Common helper function to extract bandwidth and center frequencies from
 * pre-parsed channel information. Used for both BSS and MLO link processing.
 */
static int wpas_get_channel_info_extn(int freq, enum chan_width width,
				      u8 center_freq1_idx, u8 center_freq2_idx,
				      int *center_freq1, int *center_freq2)
{
	int bw;
	u8 op_class, channel;

	if (!center_freq1 || !center_freq2)
		return -1;

	*center_freq1 = 0;
	*center_freq2 = 0;

	/* Convert channel width enum to MHz */
	bw = channel_width_to_int(width);

	/* Calculate center frequencies from indices if available */
	if (center_freq1_idx) {
		ieee80211_freq_to_channel_ext(freq, 0, 1, &op_class, &channel);
		*center_freq1 = ieee80211_chan_to_freq(NULL, op_class, center_freq1_idx);
	}

	if (center_freq2_idx) {
		ieee80211_freq_to_channel_ext(freq, 0, 1, &op_class, &channel);
		*center_freq2 = ieee80211_chan_to_freq(NULL, op_class, center_freq2_idx);
	}

	/* If no center freq calculated, use primary freq */
	if (*center_freq1 == 0)
		*center_freq1 = freq;

	return bw;
}

/**
 * wpas_check_link_nol_extn - Check if a single link uses NOL channels
 * @wpa_s: wpa_supplicant context
 * @freq: Primary channel frequency in MHz
 * @bw: Bandwidth in MHz (20, 40, 80, 160, 320)
 * @cf1: Center frequency 1 in MHz
 *       - 20/40/80/160/320 MHz contiguous: center of the full channel
 *       - 80+80 MHz: center of the first 80 MHz segment
 *       - 5G 240 MHz (320-2): center of the 160 MHz segment
 * @cf2: Center frequency 2 in MHz (0 if not applicable)
 *       - 80+80 MHz: center of the second 80 MHz segment
 *       - 160 MHz (VHT): center of the upper 80 MHz segment
 *         (cf1 is the 160 MHz center; cf2 is NOT needed for the range check)
 *       - 5G 240 MHz (320-2): center of the 80 MHz segment
 * Returns: true if link uses any NOL channel, false otherwise
 */
static bool wpas_check_link_nol_extn(struct wpa_supplicant *wpa_s,
				     int freq, int bw, int cf1, int cf2)
{
	/* Check primary channel */
	if (wpas_is_chan_nol_extn(wpa_s, freq)) {
		wpa_printf(MSG_DEBUG, "NOL: Primary freq %d is in NOL", freq);
		return true;
	}

	/* For 20 MHz, only primary matters */
	if (bw <= 20)
		return false;

	if (bw == 80 && cf2 > 0) {
		/*
		 * 80+80 MHz: two independent 80 MHz segments.
		 * cf1 = center of first segment, cf2 = center of second.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG,
				"NOL: 80+80 MHz (cf1=%d, cf2=%d)", cf1, cf2);
		if (cf1 > 0 && wpas_freq_range_uses_nol_extn(wpa_s, cf1, 80))
			return true;
		if (wpas_freq_range_uses_nol_extn(wpa_s, cf2, 80))
			return true;
	} else if (bw == 320 && cf2 > 0) {
		/*
		 * 5G 320MHz-2 (240 MHz): non-contiguous 160 MHz + 80 MHz.
		 * cf1 = center of 160 MHz segment, cf2 = center of 80 MHz segment.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG,
				"NOL: 5G 240MHz (cf1=%d 160MHz, cf2=%d 80MHz)",
				cf1, cf2);
		if (cf1 > 0 && wpas_freq_range_uses_nol_extn(wpa_s, cf1, 160))
			return true;
		if (wpas_freq_range_uses_nol_extn(wpa_s, cf2, 80))
			return true;
	} else if (bw == 160 && cf2 > 0) {
		/* 160 MHz: cf2 is the actual center frequency */
		wpa_printf(MSG_DEBUG,
				   "NOL: Checking 160 MHz MHz (cf1=%d, cf2=%d)",
				   cf1, cf2);
		if (wpas_freq_range_uses_nol_extn(wpa_s, cf2, 160))
			return true;
	} else {
		/*
		 * Standard contiguous bandwidths: 40, 80, 160, 320 MHz.
		 * cf1 is the center of the full channel.
		 *
		 * For VHT 160 MHz: cf1 = center of the 160 MHz band (seg0),
		 * cf2 = center of the upper 80 MHz segment (seg1).  cf2 is
		 * NOT needed here because cf1 already covers the full range.
		 */
		if (cf1 > 0 && wpas_freq_range_uses_nol_extn(wpa_s, cf1, bw))
			return true;
	}

	return false;
}

/**
 * wpas_bss_uses_nol_channel_extn - Check if BSS assoc link uses NOL channels
 * @wpa_s: wpa_supplicant context
 * @bss: BSS to check (Legacy, SLO, or MLO assoc link)
 * Returns: true if the assoc link uses a NOL channel, false otherwise
 *
 * Checks whether the BSS primary (assoc) link frequency is in the NOL.
 * For MLO, only the assoc link is checked here; partner link NOL handling
 * is done earlier in wpa_bss_update_scan_rnr_res() by excluding NOL partner
 * links from the connection attempt.
 */
bool wpas_bss_uses_nol_channel_extn(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss)
{
	int bw, cf1, cf2;

	if (!bss)
		return false;

	wpa_printf(MSG_DEBUG, "NOL: Checking BSS " MACSTR " (freq=%d)",
			   MAC2STR(bss->bssid), bss->freq);

	/* Only 5G channels are subject to NOL */
	if (!is_5ghz_freq(bss->freq))
		return false;

	bw = wpas_get_channel_info_extn(bss->freq, bss->max_cw,
					bss->center_freq1_idx,
					bss->center_freq2_idx, &cf1, &cf2);
	if (bw < 0) {
		wpa_printf(MSG_DEBUG, "NOL: Failed to get bandwidth info");
		return false;
	}
	return wpas_check_link_nol_extn(wpa_s, bss->freq, bw, cf1, cf2);
}

/**
 * wpas_link_uses_nol_channel_extn - Check if an arbitrary link freq/width is in NOL
 * @wpa_s: wpa_supplicant context
 * @freq: Link primary frequency in MHz
 * @width: Channel width enum
 * @cf1: Center frequency 1 in MHz
 * @cf2: Center frequency 2 in MHz (0 if not applicable)
 * Returns: true if the link uses any NOL channel, false otherwise
 *
 * Same NOL logic as wpas_bss_uses_nol_channel_extn(), but for MLO partner
 * links (bss->mld_links[]) that are not represented as a standalone
 * struct wpa_bss.
 */
bool wpas_link_uses_nol_channel_extn(struct wpa_supplicant *wpa_s, int freq,
				     enum chan_width width, int cf1, int cf2)
{
	int bw;

	if (!is_5ghz_freq(freq))
		return false;

	bw = channel_width_to_int(width);
	return wpas_check_link_nol_extn(wpa_s, freq, bw, cf1, cf2);
}

#include "wds_ie.h"

/**
 * wds_ie_assoc_req_len_extn - Return WDS IE wire length for assoc request
 *
 * @wpa_s: wpa_supplicant instance
 * @ssid:  Network profile
 *
 * Returns WDS_IE_TOTAL_LEN when ssid->wds_ie is enabled, 0 otherwise.
 */
size_t wds_ie_assoc_req_len_extn(struct wpa_supplicant *wpa_s,
				 struct wpa_ssid *ssid)
{
	if (!wpa_s || !ssid)
		return 0;

	if (!ssid->wds_ie)
		return 0;

	return WDS_IE_TOTAL_LEN;
}


/**
 * wds_ie_populate_assoc_req_extn - Append WDS IE to an association request
 *
 * Writes the WDS IE advertising WDS_IE_CAP_STA when ssid->wds_ie is enabled.
 *
 * @wpa_s:  wpa_supplicant instance
 * @ssid:   Network profile
 * @pos:    Current write position in the IE buffer
 * @avail:  Remaining bytes available at @pos
 *
 * Returns the updated write pointer.
 */
u8 *wds_ie_populate_assoc_req_extn(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid,
				   u8 *pos, size_t avail)
{
	size_t wds_ie_len;

	if (!wpa_s || !ssid || !pos)
		return pos;

	if (!ssid->wds_ie)
		return pos;

	wds_ie_len = wds_ie_build(pos, avail, WDS_IE_CAP_STA);
	if (wds_ie_len < WDS_IE_TOTAL_LEN) {
		wpa_printf(MSG_WARNING,
			   "WDS IE: STA - failed to build WDS IE for assoc req "
			   "(buffer too small, avail=%zu)", avail);
		return pos;
	}

	wpa_printf(MSG_DEBUG,
		   "WDS IE: STA - added WDS STA capability IE to assoc req");

	return pos + wds_ie_len;
}


bool wds_ie_set_params(struct wpa_supplicant *wpa_s,
		       const u8 *wds_ie, u8 elen)
{
	struct wds_ie_params params;
	bool found = false;

	if (wds_ie_parse(wds_ie, elen, &params) == 0) {
		if (params.capability & WDS_IE_CAP_AP) {
			wpa_s->wds_ie_ap = 1;
			wpa_printf(MSG_INFO,
				   "WDS IE: STA - AP advertises WDS IE capability "
				   "(cap=0x%02x ver=%u) - ",
				   params.capability, params.version);
		} else {
			wpa_printf(MSG_DEBUG,
				  "WDS IE: STA - AP WDS IE present but WDS_IE_CAP_AP "
				  "not set (cap=0x%02x)", params.capability);
		}
		return true;
	}

	return found;
}

/**
 * wds_ie_process_assoc_resp_extn - Parse WDS IE from an association response
 *                                  and enable 4-address mode on mutual WDS
 *
 * Searches the association response IEs for the WDS vendor IE.  When found
 * and the AP advertises WDS_IE_CAP_AP, sets wpa_s->wds_ie_ap = 1 and
 * enables 4-address mode via wpa_drv_set_4addr_mode() to complete the
 * mutual WDS capability negotiation on the STA side.
 *
 * Mutual WDS activation requires BOTH:
 *   - STA has wds_ie=1 configured (ssid->wds_ie)
 *   - AP advertises WDS_IE_CAP_AP in its association response
 *
 * @wpa_s:    wpa_supplicant instance
 * @ies:      IEs from the association response frame
 * @ies_len:  Length of @ies in bytes
 */
void wds_ie_process_assoc_resp_extn(struct wpa_supplicant *wpa_s,
				    const u8 *ies, size_t ies_len)
{
	const u8 *pos;
	size_t remaining;

	if (!wpa_s)
		return;

	wpa_s->wds_ie_ap = 0;

	if (wpa_s->current_ssid)
		wpa_s->conf->conf_extn.allow_3addr_mc = wpa_s->current_ssid->allow_3addr_mc;

	if (nl80211_set_allow_3addr_mc_extn(wpa_s->drv_priv,
					    (u8)wpa_s->conf->conf_extn.allow_3addr_mc))
		wpa_printf(MSG_DEBUG,
			   "ALLOW_3ADDR_MC: driver command failed");

	/* Only process if the STA profile has wds_ie enabled */
	if (!wpa_s->current_ssid || !wpa_s->current_ssid->wds_ie)
		return;

	if (!wpa_s->enabled_4addr_mode) {
		if (wpa_drv_set_4addr_mode(wpa_s, 1) == 0) {
			wpa_s->enabled_4addr_mode = 1;
			wpa_printf(MSG_INFO, "WDS IE: STA - 4-address mode enabled");
		} else {
			wpa_printf(MSG_ERROR,
				   "WDS IE: STA - failed to enable 4-address mode");
		}
	} else {
		wpa_printf(MSG_DEBUG,
			  "WDS IE: STA - 4-address mode already enabled");
	}

	if (!ies || ies_len < WDS_IE_TOTAL_LEN)
		return;

	/* Walk the IE list looking for our WDS vendor IE */
	pos = ies;
	remaining = ies_len;

	while (remaining >= 2) {
		u8 eid  = pos[0];
		u8 elen = pos[1];

		if (2u + elen > remaining)
			break;

		if (eid == WLAN_EID_VENDOR_SPECIFIC &&
		    elen >= WDS_IE_PAYLOAD_LEN) {
			/*
			 * pos + 2 points to the OUI byte.
			 * wds_ie_parse() will verify OUI and type.
			 */
			if (wds_ie_set_params(wpa_s, (pos + 2), elen))
				break;
		}

		pos += 2 + elen;
		remaining -= 2 + elen;
	}

	if (!wpa_s->wds_ie_ap) {
		wpa_printf(MSG_DEBUG,
			   "WDS IE: STA - AP did not advertise WDS capability");
	}
}
