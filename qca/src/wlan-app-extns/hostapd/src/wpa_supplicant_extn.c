// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#include "includes.h"
#include "common.h"
#include "config.h"
#include "../wpa_supplicant/wpa_supplicant_i.h"
#include "../wpa_supplicant/driver_i.h"
#include "../wpa_supplicant/ctrl_iface.h"
#include "common/version.h"
#include "common/wpa_ctrl.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "../wpa_supplicant/wps_supplicant.h"
#include "../wpa_supplicant/ap.h"
#include "../wpa_supplicant/scan.h"
#include "../wpa_supplicant/bss.h"
#include "wpa_supplicant_extn.h"
#include "common/defs.h"
#include "cmn.h"
#include "../wpa_supplicant/config.h"
#include "ap/hostapd.h"
#include "ap/dfs.h"
#include "reg_extn.h"
#include "dfs_extn.h"
#include "ucode_extn.h"
#include "qcn_ie_extn.h"

/*
 * IEEE 802.11 action frame/IE constants used by wpa_drv_send_uplink_csa().
 *
 * References:
 * - IEEE 802.11-2020, Spectrum Management action frames
 * - IEEE 802.11-2020, Channel Switch Announcement element
 * - IEEE 802.11-2020, Wide Bandwidth Channel Switch element
 */
#define UPLINK_CSA_ACTION_CHANNEL_SWITCH_ANNOUNCEMENT 4

/*
 * CSA action frame (category + action + CSA IE):
 *   1 (Category: Spectrum Management) +
 *   1 (Action: Channel Switch Announcement) +
 *   1 (Element ID: Channel Switch) +
 *   1 (Length) +
 *   3 (CSA element body)
 */
#define UPLINK_CSA_MIN_FRAME_LEN 7

/*
 * Wide Bandwidth Channel Switch element length is 3 bytes:
 *   New Channel Width, New Channel Center Frequency Segment 0,
 *   New Channel Center Frequency Segment 1.
 */
#define UPLINK_CSA_WIDE_BW_IE_BODY_LEN 3
#define UPLINK_CSA_WIDE_BW_IE_TOTAL_LEN (2 + UPLINK_CSA_WIDE_BW_IE_BODY_LEN)

/* Channel Switch Announcement element body length is 3 bytes. */
#define UPLINK_CSA_IE_BODY_LEN 3

/*
 * Channel Switch Mode:
 * 1 = stop transmitting until the scheduled channel switch (CSA).
 */
#define UPLINK_CSA_MODE_STOP_TX 1

#define UPLINK_CSA_DUMP_ACTION_FRAME 0


/**
 * wpa_ctrl_set_wds_ie_extn - Set wds_ie for the current network profile
 *
 * Handles the "WDS_IE <0|1>" ctrl_iface command.  Sets ssid->wds_ie for
 * the currently selected network and updates wpa_s->wds_ie_ap to 0 so
 * that the next association will re-negotiate WDS capability.
 *
 * @wpa_s:      wpa_supplicant instance
 * @buf:        Command argument string ("0" or "1")
 * @reply:      Output buffer for the response
 * @reply_size: Size of @reply
 *
 * Returns number of bytes written to @reply on success, -1 on error.
 */
int wpa_ctrl_set_wds_ie_extn(struct wpa_supplicant *wpa_s, int val)
{
	struct wpa_ssid *ssid;
	int count = 0;

	if (!wpa_s)
		return -1;

	if (val < 0 || val > 1) {
		wpa_printf(MSG_ERROR,
			   "WDS IE: WDS_IE: invalid value %d (expected 0 or 1)",
			   val);
		return -1;
	}

	/* Apply to every configured network profile so the setting is
	 * effective regardless of whether the STA is currently associated. */
	if (wpa_s->conf) {
		for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
			ssid->wds_ie = val;
			count++;
			wpa_printf(MSG_DEBUG,
				   "WDS IE: WDS_IE set to %d for network id=%d",
				   val, ssid->id);
		}
	}

	if (count == 0)
		wpa_printf(MSG_DEBUG,
			   "WDS IE: WDS_IE: no configured networks found");
	else
		wpa_printf(MSG_INFO,
			   "WDS IE: WDS_IE set to %d across %d network profile(s)",
			   val, count);

	/* Reset the AP-side WDS IE flag so the next association
	 * re-negotiates WDS capability. */
	wpa_s->wds_ie_ap = 0;

	return 0;
}

/**
 * wpa_ctrl_get_wds_ie_extn - Get wds_ie state for the current network
 *
 * Handles the "GET_WDS_IE" ctrl_iface command.  Returns the current
 * wds_ie value for the selected network and the AP-side WDS IE flag.
 *
 * @wpa_s:      wpa_supplicant instance
 * @reply:      Output buffer for the response
 * @reply_size: Size of @reply
 *
 * Returns number of bytes written to @reply on success, -1 on error.
 */
int wpa_ctrl_get_wds_ie_extn(struct wpa_supplicant *wpa_s,
			     char *reply, int reply_size)
{
	int ssid_wds_ie = 0, res;

	if (!wpa_s)
		return -1;

	if (wpa_s->current_ssid)
		ssid_wds_ie = wpa_s->current_ssid->wds_ie;

	res = os_snprintf(reply, reply_size,
			  "wds_ie=%d\nwds_ie_ap=%d\n",
			  ssid_wds_ie, wpa_s->wds_ie_ap);
	if (os_snprintf_error(reply_size, res))
		return -1;
	return res;
}

/**
 * wpa_ctrl_set_allow_3addr_mc_extn - Set INTF_ALLOW_3ADDR_MC via vendor command
 *
 * Handles the "INTF_ALLOW_3ADDR_MC <0|1>" ctrl_iface command.  Sends
 * QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION with
 * QCA_WLAN_VENDOR_ATTR_CONFIG_INTF_ALLOW_3ADDR_MC attribute to the driver.
 *
 * @wpa_s:      wpa_supplicant instance
 * @buf:        Command argument string ("0" or "1")
 * @reply:      Output buffer for the response
 * @reply_size: Size of @reply
 *
 * Returns number of bytes written to @reply on success, -1 on error.
 */
int wpa_ctrl_set_allow_3addr_mc_extn(struct wpa_supplicant *wpa_s, int val)
{
	if (!wpa_s)
		return -1;

	if (val < 0 || val > 1) {
		wpa_printf(MSG_ERROR,
			   "INTF_ALLOW_3ADDR_MC: invalid value %d"
			   " (expected 0 or 1)", val);
		return -1;
	}

	if (nl80211_set_allow_3addr_mc_extn(wpa_s->drv_priv, (u8)val)) {
		wpa_printf(MSG_ERROR,
			   "ALLOW_3ADDR_MC: driver command failed");
		return -1;
	}

	wpa_s->conf->conf_extn.allow_3addr_mc = val;
	wpa_printf(MSG_INFO, "ALLOW_3ADDR_MC: set to %d", val);

	return 0;
}

bool wpas_ap_link_address_extn(struct wpa_supplicant *wpa_s, const u8 *addr)
{
	int i;

	if (!wpa_s->valid_links)
		return false;

	for_each_link(wpa_s->valid_links, i) {
		if (ether_addr_equal(wpa_s->links[i].bssid, addr))
			return true;
	}

	return false;
}


int wpa_drv_send_action_extn(struct wpa_supplicant *wpa_s, unsigned int freq,
			unsigned int wait, const u8 *dst, const u8 *src,
			const u8 *bssid, const u8 *data, size_t data_len,
			int no_cck, int link_id)
{
	if (!wpa_s->driver->send_action)
		return -1;

	if (data_len > 0 && data[0] != WLAN_ACTION_PUBLIC) {
		if (wpas_ap_link_address_extn(wpa_s, dst))
			dst = wpa_s->ap_mld_addr;

		if (wpas_ap_link_address_extn(wpa_s, bssid))
			bssid = wpa_s->ap_mld_addr;
	}

	return wpa_s->driver->send_action(wpa_s->drv_priv, freq, wait, dst, src,
					  bssid, data, data_len, no_cck, link_id);
}

static s8 wpas_extn_get_hw_idx_by_freq(struct wpa_supplicant *wpa_s,
					int partner_freq)
{
	struct hostapd_multi_hw_info *hw_info;
	int i;

	if (!wpa_s)
		return -1;

	for (i = 0; i < wpa_s->num_multi_hws; i++) {
		hw_info = &wpa_s->multi_hw_info[i];
		if (partner_freq >= hw_info->start_freq &&
		    partner_freq <= hw_info->end_freq)
			return i;
	}

	return -1;
}

static int wpas_rrm_get_request_link(struct wpa_supplicant *wpa_s)
{
	int i;
	s8 target_hw_idx;

	if (!wpa_s->valid_links || !wpa_s->rrm.req_freq)
		return -1;

	if (wpa_s->valid_links) {
		target_hw_idx = wpas_extn_get_hw_idx_by_freq(wpa_s,
							     wpa_s->rrm.req_freq);
		for_each_link(wpa_s->valid_links, i) {
			if (wpa_s->links[i].disabled)
				continue;
			if (target_hw_idx == wpas_extn_get_hw_idx_by_freq(wpa_s,
									  wpa_s->links[i].freq))
				return i;
		}
	}

	return -1;
}


int wpa_drv_send_rrm_action_extn(struct wpa_supplicant *wpa_s,
				 const u8 *data, size_t data_len, int no_cck)
{
	const u8 *bssid = wpa_s->bssid;
	unsigned int freq = wpa_s->assoc_freq;
	int link_id;

	link_id = wpas_rrm_get_request_link(wpa_s);
	if (link_id >= 0 && link_id < MAX_NUM_MLD_LINKS &&
	    (wpa_s->valid_links & BIT(link_id)) &&
	    !wpa_s->links[link_id].disabled &&
	    wpa_s->links[link_id].freq) {
		freq = wpa_s->links[link_id].freq;
		bssid = wpa_s->links[link_id].bssid;
	}

	wpa_printf(MSG_DEBUG,
		   "RRM: Send Radio Measurement report req_freq=%u freq=%u link_id=%d dst="
		   MACSTR " src=" MACSTR,
		   wpa_s->rrm.req_freq, freq, link_id, MAC2STR(bssid),
		   MAC2STR(wpa_s->own_addr));

	return wpa_drv_send_action_extn(wpa_s, freq, 0, bssid, wpa_s->own_addr,
					bssid, data, data_len, no_cck, link_id);
}

static bool wpas_uplink_csa_freq_band_match(struct wpa_supplicant *wpa_s,
					    unsigned int ref_freq,
					    unsigned int link_freq)
{
	s8 target_hw_idx;
	s8 link_hw_idx;

	if (!ref_freq || !link_freq)
		return false;

	if (is_24ghz_freq(ref_freq))
		return is_24ghz_freq(link_freq);

	if (is_5ghz_freq(ref_freq)) {
		if (!is_5ghz_freq(link_freq))
			return false;

		if (!wpa_s)
			return true;

		target_hw_idx = wpas_extn_get_hw_idx_by_freq(wpa_s, ref_freq);
		link_hw_idx = wpas_extn_get_hw_idx_by_freq(wpa_s, link_freq);
		wpa_printf(MSG_DEBUG,
			   "uplink_csa: 5GHz hw_idx check ref_freq=%u link_freq=%u target_hw_idx=%d link_hw_idx=%d",
			   ref_freq, link_freq, target_hw_idx, link_hw_idx);

		return target_hw_idx == link_hw_idx;
	}

	if (is_6ghz_freq(ref_freq))
		return is_6ghz_freq(link_freq);

	return false;
}

static int wpas_uplink_csa_get_tx_link(struct wpa_supplicant *wpa_s,
				       unsigned int new_freq,
				       unsigned int *tx_freq, int *tx_link_id)
{
	int i;

	*tx_freq = 0;
	*tx_link_id = -1;

	if (!new_freq)
		return -1;

	if (!wpa_s->valid_links) {
		if (wpas_uplink_csa_freq_band_match(wpa_s, new_freq,
						    wpa_s->assoc_freq)) {
			*tx_freq = wpa_s->assoc_freq;
			return 0;
		}
		return -1;
	}

	/* Pick a live link operating in the same band as the new CSA frequency. */
	for_each_link(wpa_s->valid_links, i) {
		if (wpa_s->links[i].disabled || !wpa_s->links[i].freq)
			continue;
		if (!wpas_uplink_csa_freq_band_match(wpa_s, new_freq,
						     wpa_s->links[i].freq))
			continue;
		*tx_freq = wpa_s->links[i].freq;
		*tx_link_id = i;
		return 0;
	}

	return -1;
}

bool wpas_uplink_csa_link_available(struct wpa_supplicant *wpa_s,
				    unsigned int freq)
{
	int i;

	if (!wpa_s || !freq)
		return false;

	if (wpa_s->valid_links) {
		for_each_link(wpa_s->valid_links, i) {
			if (wpa_s->links[i].disabled || !wpa_s->links[i].bss)
				continue;
			wpa_printf(MSG_DEBUG,
				   "uplink_csa: validating link=%d link_freq=%u target_freq=%u bss=%p",
				   i, wpa_s->links[i].freq, freq, wpa_s->links[i].bss);

			if (wpas_uplink_csa_freq_band_match(wpa_s, freq,
							    wpa_s->links[i].freq)) {
				wpa_printf(MSG_DEBUG,
					   "uplink_csa: link=%d available for target_freq=%u",
					   i, freq);
				return true;
			}
		}

		wpa_printf(MSG_DEBUG,
			   "uplink_csa: no MLO link available for target_freq=%u",
			   freq);
		return false;
	}

	if (!wpa_s->current_bss)
		return false;

	wpa_printf(MSG_DEBUG,
		   "uplink_csa: validating current_bss freq=%u target_freq=%u",
		   wpa_s->current_bss->freq, freq);

	return wpas_uplink_csa_freq_band_match(wpa_s, freq,
					       wpa_s->current_bss->freq);
}

static void wpas_uplink_csa_update_wb_ie_cfs(u8 primary_chan, u8 new_ch_width,
					     u8 *ch_seg_0, u8 *ch_seg_1)
{
	if (new_ch_width != CONF_OPER_CHWIDTH_160MHZ)
		return;

	/*
	 * Encode CCFS0/CCFS1 for 160 MHz per IEEE Std 802.11-2024,
	 * Table 9-316, matching hostapd_eid_wb_channel_switch():
	 * - CCFS1 is the 160 MHz center frequency segment index.
	 * - CCFS0 is the 80 MHz segment containing the primary channel.
	 */
	*ch_seg_1 = *ch_seg_0;
	if (primary_chan < *ch_seg_0)
		*ch_seg_0 -= 8;
	else
		*ch_seg_0 += 8;
}

#ifdef UPLINK_CSA_DUMP_ACTION_FRAME
static void wpas_uplink_csa_dump_action_frame(struct wpabuf *buf)
{
	const u8 *pos, *end;

	wpa_hexdump_buf(MSG_INFO, "uplink_csa: Action frame payload", buf);

	/* Walk IEs (after Category+Action) and print summary */
	pos = wpabuf_head_u8(buf) + 2;
	end = wpabuf_head_u8(buf) + wpabuf_len(buf);
	while (end - pos >= 2) {
		u8 eid = pos[0];
		u8 elen = pos[1];
		const u8 *edata = pos + 2;
		size_t ie_len = 2 + elen;

		if ((size_t) (end - pos) < ie_len) {
			wpa_printf(MSG_INFO,
				   "uplink_csa: IE parse truncated eid=%u elen=%u rem=%zu",
				   eid, elen, (size_t) (end - pos));
			break;
		}

		if (eid == WLAN_EID_CHANNEL_SWITCH && elen >= 3) {
			wpa_printf(MSG_INFO,
				   "uplink_csa: CSA IE: mode=%u new_chan=%u count=%u",
				   edata[0], edata[1], edata[2]);
		} else if (eid == WLAN_EID_WIDE_BW_CHSWITCH && elen >= 3) {
			wpa_printf(MSG_INFO,
				   "uplink_csa: WideBW IE: width=%u seg0=%u seg1=%u",
				   edata[0], edata[1], edata[2]);
		} else if (eid == WLAN_EID_EXTENSION && elen >= 3 &&
			   edata[0] == WLAN_EID_EXT_MLO_LINK_INFO) {
			wpa_printf(MSG_INFO,
				   "uplink_csa: ML Info IE: link_id_bitmap=0x%04x",
				   WPA_GET_LE16(edata + 1));
		} else if (eid == WLAN_EID_VENDOR_SPECIFIC && elen >= 4) {
			wpa_printf(MSG_INFO,
				   "uplink_csa: Vendor IE: OUI=%02x:%02x:%02x type=%u elen=%u",
				   edata[0], edata[1], edata[2], edata[3], elen);
		} else {
			wpa_printf(MSG_INFO,
				   "uplink_csa: IE parsed: eid=%u elen=%u",
				   eid, elen);
			wpa_hexdump(MSG_INFO, "uplink_csa: IE raw", pos, ie_len);
		}

		pos += ie_len;
	}
}
#endif

int wpa_drv_send_uplink_csa(struct wpa_supplicant *wpa_s, int freq,
			    u8 cs_count, u8 ch_seg_0, u8 ch_seg_1,
			    u8 new_ch_width, const u8 *nol_ie,
			    size_t nol_ie_len, int cac_abort)
{
	struct wpabuf *buf = NULL;
	u8 chan;
	int res;
	unsigned int tx_freq;
	int tx_link_id;
	bool is_wb_ie_present = false;
	size_t total_len;
	u8 width = 0;
	bool ml_info_present = false;

	if (wpa_s->wpa_state != WPA_COMPLETED)
		return -1;

	if ((new_ch_width < CONF_OPER_CHWIDTH_USE_HT) ||
	    (new_ch_width > CONF_OPER_CHWIDTH_160MHZ)) {
		wpa_printf(MSG_DEBUG, "Invalid new channel width %u", new_ch_width);
		return -1;
	}

	/*
	 * Wide Bandwidth Channel Switch element channel width encoding per
	 * IEEE Std 802.11-2024, Table 9-316 (VHT Operation Information):
	 * 0 = 40 MHz, 1 = 80 MHz / 160 MHz / 80+80 MHz.
	 *
	 * Both 80 MHz and 160 MHz use width=1. The receiver distinguishes
	 * them by checking whether cf1 (ch_seg_1 / CCFS1) is non-zero:
	 * non-zero cf1 indicates 160 MHz, zero cf1 indicates 80 MHz.
	 */
	if (new_ch_width == CONF_OPER_CHWIDTH_160MHZ ||
	    new_ch_width == CONF_OPER_CHWIDTH_80MHZ)
		width = 1;

	/* Here, "freq" refers to the new channel frequency selected by the repeater
	 * upon radar detection. This frequency is conveyed to the root AP via an
	 * uplink CSA frame, enabling the root AP to switch to the new channel.
	 */
	ieee80211_freq_to_chan(freq, &chan);
	if (wpas_uplink_csa_get_tx_link(wpa_s, freq, &tx_freq, &tx_link_id) < 0) {
		wpa_printf(MSG_ERROR,
			   "Drop uplink CSA: no valid tx link found for new freq %d",
			   freq);
		return -1;
	}

	wpas_uplink_csa_update_wb_ie_cfs(chan, new_ch_width,
					 &ch_seg_0, &ch_seg_1);

	wpa_printf(MSG_DEBUG,
		   "freq %u chan %u cs_count %u ch_seg_0 %u ch_seg_1 %u new_ch_width %u assoc_freq %u tx_freq %u tx_link_id %d",
		   freq, chan, cs_count, ch_seg_0, ch_seg_1, new_ch_width,
		   wpa_s->assoc_freq, tx_freq, tx_link_id);

	total_len = UPLINK_CSA_MIN_FRAME_LEN;
	if (chan == ch_seg_0) {
		wpa_printf(MSG_DEBUG, "20MHz, ignore adding wide band ie");
	} else {
		wpa_printf(MSG_DEBUG, "Add wide band ie");
		total_len += UPLINK_CSA_WIDE_BW_IE_TOTAL_LEN;
		is_wb_ie_present = true;
	}

	if (cac_abort && wpa_s->valid_links)
		total_len += 5; /* ML Info IE length */

	if (nol_ie && nol_ie_len > 0) {
		total_len += nol_ie_len;
		wpa_printf(MSG_INFO, "Adding NOL IE to uplink CSA, len=%zu",
			   nol_ie_len);
	}

	buf = wpabuf_alloc(total_len);

	if (!buf) {
		wpa_printf(MSG_DEBUG, "Memory allocation failed");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_SPECTRUM_MGMT);
	wpabuf_put_u8(buf, UPLINK_CSA_ACTION_CHANNEL_SWITCH_ANNOUNCEMENT);
	wpabuf_put_u8(buf, WLAN_EID_CHANNEL_SWITCH);
	wpabuf_put_u8(buf, UPLINK_CSA_IE_BODY_LEN);
	wpabuf_put_u8(buf, UPLINK_CSA_MODE_STOP_TX);
	wpabuf_put_u8(buf, chan);
	wpabuf_put_u8(buf, cs_count);
	if (is_wb_ie_present) {
		wpabuf_put_u8(buf, WLAN_EID_WIDE_BW_CHSWITCH);
		wpabuf_put_u8(buf, UPLINK_CSA_WIDE_BW_IE_BODY_LEN);
		wpabuf_put_u8(buf, width);
		wpabuf_put_u8(buf, ch_seg_0);
		wpabuf_put_u8(buf, ch_seg_1);
	}

	if (nol_ie && nol_ie_len > 0) {
		wpabuf_put_data(buf, nol_ie, nol_ie_len);
		wpa_hexdump(MSG_INFO, "Uplink CSA NOL IE", nol_ie, nol_ie_len);
	}

	if (cac_abort && wpa_s->valid_links) {
		unsigned int pending_ch_switch_freq = 0;
		u16 link_id_bitmap = 0;
		s8 target_hw_idx = wpas_extn_get_hw_idx_by_freq(wpa_s, freq);
		int link_5g = -1;
		int i;
		bool has_non_5g_partner = false;

		for_each_link(wpa_s->valid_links, i) {
			s8 link_hw_idx;

			if (wpa_s->links[i].disabled || !wpa_s->links[i].freq)
				continue;
			if (is_5ghz_freq(wpa_s->links[i].freq)) {
				link_hw_idx = wpas_extn_get_hw_idx_by_freq(wpa_s,
									   wpa_s->links[i].freq);
				wpa_printf(MSG_DEBUG,
					   "uplink_csa: cac_abort hw_idx check target_hw_idx=%d link=%d link_hw_idx=%d",
					   target_hw_idx, i, link_hw_idx);
				if (target_hw_idx == link_hw_idx) {
					wpa_printf(MSG_DEBUG,
						   "uplink_csa: cac_abort selected 5GHz link=%d freq=%u",
						   i, wpa_s->links[i].freq);
					link_5g = i;
				}
			} else {
				has_non_5g_partner = true;
			}
		}

		if (has_non_5g_partner && link_5g >= 0) {
			/* Encoding ML Info IE to the action frame */
			link_id_bitmap = BIT(link_5g);
			ml_info_present = true;
			wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
			wpabuf_put_u8(buf, 3); /* ext_id (1) + bitmap (2) */
			wpabuf_put_u8(buf, WLAN_EID_EXT_MLO_LINK_INFO);
			wpabuf_put_le16(buf, link_id_bitmap);
		}

		if (tx_link_id >= 0 && tx_link_id < MAX_NUM_MLD_LINKS)
			pending_ch_switch_freq =
				wpa_s->links[tx_link_id].pending_ch_switch_freq;

		if (pending_ch_switch_freq != 0)
			tx_freq = pending_ch_switch_freq;
	}

#ifdef UPLINK_CSA_DUMP_ACTION_FRAME
	wpas_uplink_csa_dump_action_frame(buf);
#endif
	res = wpa_drv_send_action_extn(wpa_s, tx_freq, 0, wpa_s->bssid,
				       wpa_s->own_addr, wpa_s->bssid,
				       wpabuf_head(buf), wpabuf_len(buf), 0,
				       ml_info_present ? -1 : tx_link_id);
	if (res < 0)
		wpa_printf(MSG_ERROR,
			   "Failed to send uplink CSA action frame");
	wpabuf_free(buf);
	return res;
}

static u32 wpa_bss_get_mcst_ie_extn(const u8 *ies, size_t ies_len)
{
	const struct element *elem;

	for_each_element_extid(elem, WLAN_EID_EXT_MAX_CHANNEL_SWITCH_TIME,
			       ies, ies_len) {
		wpa_printf(MSG_DEBUG, "MLD: MCST IE candidate datalen=%u",
			   elem->datalen);
		if (elem->datalen >= 4)
			return WPA_GET_LE24(&elem->data[1]);
	}

	return 0;
}


void wpa_bss_update_mld_link_mcst_extn(struct wpa_bss *bss, u8 link_id,
				       const u8 *ies, size_t ies_len)
{
	u32 mcst;

	if (!bss || link_id >= MAX_NUM_MLD_LINKS) {
		wpa_printf(MSG_DEBUG, "MLD: skip MCST cache bss=%p link_id=%u",
			   bss, link_id);
		return;
	}

	mcst = wpa_bss_get_mcst_ie_extn(ies, ies_len);
	if (!mcst)
		return;

	bss->mld_links[link_id].mcst = mcst;
	os_get_reltime(&bss->mld_links[link_id].mcst_update_time);
	wpa_printf(MSG_INFO, "MLD: link_id=%u MCST=%u TU cached",
		   link_id, mcst);
}


u32 wpa_bss_get_mld_link_mcst_extn(struct wpa_bss *bss, u8 link_id)
{
	struct os_reltime now, age;
	u64 elapsed_tu;
	u32 mcst;

	if (!bss || link_id >= MAX_NUM_MLD_LINKS)
		return 0;

	mcst = bss->mld_links[link_id].mcst;
	if (!mcst)
		return 0;

	if (os_get_reltime(&now) < 0)
		return mcst;

	os_reltime_sub(&now, &bss->mld_links[link_id].mcst_update_time, &age);
	elapsed_tu = (u64) age.sec * 1000000 + age.usec;
	elapsed_tu /= 1024;

	if (elapsed_tu >= mcst) {
		wpa_printf(MSG_INFO, "MLD: MCST expired link_id=%u original=%u "
			   "elapsed=%llu TU",
			   link_id, mcst, (unsigned long long) elapsed_tu);
		return 0;
	}

	wpa_printf(MSG_DEBUG, "MLD: MCST residual link_id=%u original=%u "
		   "elapsed=%llu residual=%llu TU",
		   link_id, mcst, (unsigned long long) elapsed_tu,
		   (unsigned long long) (mcst - elapsed_tu));
	return mcst - elapsed_tu;
}


void wpa_bss_parse_basic_mle_per_sta_mcst_extn(struct wpa_bss *bss,
					       struct wpabuf *mlbuf,
					       size_t common_info_len)
{
	const u8 *pos, *end;
	size_t rem_len;

	pos = wpabuf_head_u8(mlbuf) + sizeof(struct ieee80211_eht_ml) +
		common_info_len;
	end = wpabuf_head_u8(mlbuf) + wpabuf_len(mlbuf);
	rem_len = end - pos;

	while (rem_len > 2) {
		size_t sub_elem_len;
		int num_frag_subelems;

		num_frag_subelems =
			ieee802_11_defrag_mle_subelem(mlbuf, pos,
						      &sub_elem_len);
		if (num_frag_subelems < 0) {
			wpa_printf(MSG_DEBUG, "MLD: ML probe response MCST "
				   "Per-STA defrag failed");
			break;
		}

		rem_len -= num_frag_subelems * 2;
		if (2 + sub_elem_len > rem_len) {
			wpa_printf(MSG_DEBUG,
				   "MLD: ML probe response MCST Per-STA "
				   "invalid subelem_len=%zu rem_len=%zu",
				   sub_elem_len, rem_len);
			break;
		}

		if (*pos == MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE &&
		    sub_elem_len >= BASIC_MLE_STA_CTRL_LEN + 1) {
			const u8 *body = pos + 2;
			const u8 *sub_end = pos + 2 + sub_elem_len;
			u16 ctrl = WPA_GET_LE16(body);
			u8 link_id = ctrl & BASIC_MLE_STA_CTRL_LINK_ID_MASK;
			size_t sta_info_len;

			body += BASIC_MLE_STA_CTRL_LEN;
			if (link_id < MAX_NUM_MLD_LINKS &&
			    ctrl & BASIC_MLE_STA_CTRL_COMPLETE_PROFILE &&
			    body < sub_end) {
				sta_info_len = *body;
				wpa_printf(MSG_INFO,
					   "MLD: ML probe response MCST Per-STA link_id=%u "
					   "ctrl=0x%x sta_info_len=%zu profile_len=%zu",
					   link_id, ctrl, sta_info_len,
					   (size_t) (sub_end - body));
				if (sta_info_len <= (size_t) (sub_end - body))
					wpa_bss_update_mld_link_mcst_extn(
						bss, link_id, body + sta_info_len,
						sub_end - (body + sta_info_len));
			}
		}

		pos += 2 + sub_elem_len;
		rem_len -= 2 + sub_elem_len;
	}
}

int wpa_supplicant_event_extn(struct wpa_supplicant *wpa_s,
			      enum wpa_event_type event,
			      union wpa_event_data *data)
{
	if (!wpa_s || !data)
		return -EINVAL;

	switch (event) {
	case EVENT_HW_BLOCKED_CHANS_NOTIFY:
		wpas_event_hw_blocklist_notify_extn(
			wpa_s, &data->event_data_extn.hw_blocklist_info);
		break;
	case EVENT_AFC_POWER_UPDATE_COMPLETE_NOTIFY:
	case EVENT_AFC_PAYLOAD_RESET:
		wpa_printf(MSG_DEBUG, "Received AFC Event. Waiting for AFC regd");
		wpa_s->wpas_extn.is_waiting_for_afc_regd = true;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void wpas_iface_init_extn(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s)
		return;

	wpa_s->wpas_extn.check_hw_blocklist = true;
}

/*
 * wpas_country_change_init_active_alpha2_extn - Prime active_alpha2 from
 * the driver's current regulatory domain.
 *
 * Called once the driver is attached (wpas_init_driver() has succeeded).
 * Without this, active_alpha2 starts as "" and the first
 * EVENT_CHANNEL_LIST_CHANGED / REGDOM_TYPE_COUNTRY notification is always
 * treated as a change in wpas_country_change_check_current_bss(), even when
 * the STA is already connected under the same country that is being
 * reported. Priming it here makes the first comparison meaningful.
 */
void wpas_country_change_init_active_alpha2_extn(struct wpa_supplicant *wpa_s)
{
	char alpha2[3] = { 0 };

	if (!wpa_s || !wpa_s->driver || !wpa_s->drv_priv ||
	    !wpa_s->driver->get_country)
		return;

	if (wpa_s->driver->get_country(wpa_s->drv_priv, alpha2) < 0)
		return;

	os_strlcpy(wpa_s->wpas_extn.active_alpha2, alpha2,
		   sizeof(wpa_s->wpas_extn.active_alpha2));
}

void wpas_iface_deinit_extn(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s)
		return;

	hostapd_free_hw_blocklist_info_extn(wpa_s->wpas_extn.hw_blocklist_info,
					    wpa_s->wpas_extn.num_hw_blocklist);
	wpa_s->wpas_extn.hw_blocklist_info = NULL;
	wpa_s->wpas_extn.num_hw_blocklist = 0;
	wpa_s->wpas_extn.check_hw_blocklist = false;
	wpa_s->wpas_extn.is_waiting_for_afc_regd = false;
}

static struct hostapd_multi_hw_info *
wpas_get_current_hw_info_extn(struct wpa_supplicant *wpa_s, int freq)
{
	u8 i;

	if (!wpa_s || !freq || !wpa_s->multi_hw_info || !wpa_s->num_multi_hws)
		return NULL;

	for (i = 0; i < wpa_s->num_multi_hws; i++) {
		struct hostapd_multi_hw_info *hw_info = &wpa_s->multi_hw_info[i];

		if (hw_info->start_freq <= freq && hw_info->end_freq >= freq)
			return hw_info;
	}

	return NULL;
}

void wpas_query_hw_blocklist_extn(struct wpa_supplicant *wpa_s)
{
	struct hostapd_multi_hw_info *hw_info;
	int radio_idx = -1;
	int query_freq = 0;
	int ret;

	if (!wpa_s || !wpa_s->support_6ghz)
		return;

	if (!wpa_s->driver || !wpa_s->drv_priv ||
	    !wpa_s->driver->is_6ghz_hw_blocked_chans_supported ||
	    !wpa_s->driver->fetch_hw_blocked_chans)
		return;

	if (!wpa_s->driver->is_6ghz_hw_blocked_chans_supported(wpa_s->drv_priv))
		return;

	/* For MLO connections, query HWBL for each unique radio across all
	 * active links. For SLO, fall back to assoc_freq/current_bss. */
	if (wpa_s->valid_links) {
		u32 queried_mask = 0;
		int i;

		for_each_link(wpa_s->valid_links, i) {
			unsigned int freq = wpa_s->links[i].freq;

			if (!freq)
				continue;
			/* HWBL is 6 GHz only — skip 2.4/5 GHz links */
			if (!is_6ghz_freq(freq))
				continue;
			hw_info = wpas_get_current_hw_info_extn(wpa_s, freq);
			if (!hw_info)
				continue;
			if (queried_mask & BIT(hw_info->hw_idx))
				continue;
			queried_mask |= BIT(hw_info->hw_idx);

			wpa_printf(MSG_DEBUG,
				   "Query HW blocklist for ML link=%d freq=%d hw_idx=%u",
				   i, freq, hw_info->hw_idx);
			ret = wpa_s->driver->fetch_hw_blocked_chans(
				wpa_s->drv_priv, hw_info->hw_idx);
			if (ret)
				wpa_printf(MSG_DEBUG,
					   "wpas: Failed to fetch HW blocklist (ifname=%s link=%d radio_idx=%u ret=%d)",
					   wpa_s->ifname, i, hw_info->hw_idx,
					   ret);
		}
		return;
	}

	if (wpa_s->assoc_freq) {
		query_freq = wpa_s->assoc_freq;
		wpa_printf(MSG_DEBUG,
			   "Query HW blocklist for assoc_freq=%d",
			   wpa_s->assoc_freq);
	} else if (wpa_s->current_bss) {
		query_freq = wpa_s->current_bss->freq;
		wpa_printf(MSG_DEBUG,
			   "Query HW blocklist for current_bss freq=%d",
			   wpa_s->current_bss->freq);
	} else {
		wpa_printf(MSG_DEBUG,
			   "No assoc_freq or current_bss, query HW blocklist for all radios");
	}

	hw_info = wpas_get_current_hw_info_extn(wpa_s, query_freq);
	if (query_freq && hw_info)
		radio_idx = hw_info->hw_idx;

	ret = wpa_s->driver->fetch_hw_blocked_chans(wpa_s->drv_priv,
						    radio_idx);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "wpas: Failed to fetch HW blocklist channels (ifname=%s radio_idx=%d ret=%d)",
			   wpa_s->ifname, radio_idx, ret);
	}
}

void wpas_event_hw_blocklist_notify_extn(
	struct wpa_supplicant *wpa_s,
	const struct hostapd_hw_blocklist_info *hw_blocklist_info)
{
	struct wpa_supplicant *w;

	if (!wpa_s || !hw_blocklist_info)
		return;

	wpa_printf(MSG_DEBUG, "Received HW blocklist update event for hw_idx=%u modes=%u ifaces %p",
		   hw_blocklist_info->hw_idx, hw_blocklist_info->num_pwr_modes, wpa_s->global->ifaces);

	if (!wpa_s->global->ifaces) {
		wpa_printf(MSG_ERROR,
			   "wpas: No ifaces available to store HW BL for hw_idx=%u, store in wpa_s context",
			   hw_blocklist_info->hw_idx);
		if (hw_blocklist_update_list_extn(&wpa_s->wpas_extn.hw_blocklist_info,
						  &wpa_s->wpas_extn.num_hw_blocklist,
						  hw_blocklist_info)) {
			wpa_printf(MSG_ERROR,
				   "wpas: Failed to store HW blocklist info for hw_idx=%u",
				   hw_blocklist_info->hw_idx);
			return;
		}

		wpa_printf(MSG_DEBUG,
			   "wpas: Stored HW blocklist info for ifname=%s hw_idx=%u modes=%u total_hw=%u",
			   wpa_s->ifname, hw_blocklist_info->hw_idx, hw_blocklist_info->num_pwr_modes,
			   wpa_s->wpas_extn.num_hw_blocklist);
		return;
	}

	/* multi_hw_info is wiphy-wide: all wpa_s instances share the same
	 * physical radios. Store the blocklist in every interface so that
	 * whichever wpa_s later performs 6 GHz BSS filtering finds the data. */
	for (w = wpa_s->global->ifaces; w; w = w->next) {
		int ret;

		ret = hw_blocklist_update_list_extn(&w->wpas_extn.hw_blocklist_info,
						    &w->wpas_extn.num_hw_blocklist,
						    hw_blocklist_info);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "wpas: Failed to store HW blocklist info for ifname=%s hw_idx=%u ret=%d",
				   w->ifname, hw_blocklist_info->hw_idx, ret);
			continue;
		}

		wpa_printf(MSG_DEBUG,
			   "wpas: Stored HW blocklist info for ifname=%s hw_idx=%u modes=%u total_hw=%u",
			   w->ifname, hw_blocklist_info->hw_idx,
			   hw_blocklist_info->num_pwr_modes,
			   w->wpas_extn.num_hw_blocklist);
	}
}

static u8 wpas_get_6ghz_bss_pwr_mode_extn(const struct wpa_bss *bss)
{
	const u8 *ie;
	struct ieee80211_he_operation *heop;
	struct ieee80211_he_6ghz_oper_info *he_oper_6g;
	u8 pos = 9;
	u8 he_reg_info;

	ie = get_ie_ext(bss->ies, bss->ie_len, WLAN_EID_EXT_HE_OPERATION);
	if (!ie || ie[1] < 6)
		return NL80211_REG_NUM_POWER_MODES;

	heop = (struct ieee80211_he_operation *)(&ie[3]);
	if (!(heop->he_oper_params & HE_OPERATION_6GHZ_OPER_INFO))
		return NL80211_REG_NUM_POWER_MODES;

	if (heop->he_oper_params & HE_OPERATION_VHT_OPER_INFO)
		pos += 3;
	if (heop->he_oper_params & HE_OPERATION_COHOSTED_BSS)
		pos += 1;

	he_oper_6g = (struct ieee80211_he_6ghz_oper_info *)(ie + pos);
	he_reg_info = (he_oper_6g->control &
		       HE_6GHZ_OPER_INFO_CTRL_REG_INFO_MASK) >>
		       HE_6GHZ_OPER_INFO_CTRL_REG_INFO_SHIFT;

	if (he_reg_info == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR_SP)
		he_reg_info = HE_REG_INFO_6GHZ_AP_TYPE_SP;

	return he_reg_info;
}

bool wpas_is_6ghz_hwbl_link_ok_extn(struct wpa_supplicant *wpa_s,
				     const struct wpa_bss *bss)
{
	struct wpa_supplicant_extn *wpas_extn;
	u16 center_freq, bw;
	u8 pwr_mode;

	if (!wpa_s || !bss || !is_6ghz_freq(bss->freq))
		return true;

	wpas_extn = &wpa_s->wpas_extn;
	if (!wpas_extn->check_hw_blocklist ||
	    !wpas_extn->hw_blocklist_info ||
	    !wpas_extn->num_hw_blocklist)
		return true;

	pwr_mode = wpas_get_6ghz_bss_pwr_mode_extn(bss);
	if (pwr_mode >= NL80211_REG_NUM_POWER_MODES)
		return true;

	bw = channel_width_to_int(bss->max_cw);
	if (!bw)
		bw = 20;

	/*
	 * Compute center frequency. Use ccfs0 from the Operation IE when
	 * available (center_freq1_idx != 0). For 320 MHz, ccfs0 unambiguously
	 * identifies the segment. When center_freq1_idx is zero (IE not
	 * parsed), fall back to hostapd_get_bonded_chan_center_freq() which
	 * uses the bonded channel table; for 320 MHz it returns the first
	 * matching segment's center — good enough for a conservative check.
	 */
	if (bss->center_freq1_idx == 2)
		center_freq = 5935;
	else if (bss->center_freq1_idx)
		center_freq = 5950 + bss->center_freq1_idx * 5;
	else
		center_freq = hostapd_get_bonded_chan_center_freq(
			(u16)bss->freq, bw, 0, 0);

	if (hw_features_is_channel_in_hw_blocklist_for_hw_idx_extn(
		    wpas_extn->hw_blocklist_info,
		    wpas_extn->num_hw_blocklist,
		    NL80211_WIPHY_RADIO_ID_MAX,
		    (u16)bss->freq, center_freq, bw,
		    pwr_mode, bss->punc_bitmap)) {
		wpa_printf(MSG_DEBUG,
			   "HWBL: 6 GHz BSS freq=%d bw=%u center=%u pwr=%u blocked",
			   bss->freq, bw, center_freq, pwr_mode);
		return false;
	}

	return true;
}

static size_t wpas_qcn_buflen_add_vht_mcs10_11_attr(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s || !wpa_s->conf || !wpa_s->conf->conf_extn.vht_mcs_10_11_supp ||
	    !(wpa_s->hw_capab & BIT(CAPAB_VHT)))
		return 0;

	return QCN_ATTRIB_HDR_LEN + QCN_VHT_MCS10_11_SUPP_ATTRIB_LEN;
}

static size_t wpas_qcn_buflen_add_he_400ns_sgi_attr(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s || !wpa_s->conf || !wpa_s->conf->conf_extn.he_400ns_sgi_supp ||
	    !(wpa_s->hw_capab & BIT(CAPAB_HE)))
		return 0;

	return QCN_ATTRIB_HDR_LEN + QCN_HE_400NS_SGI_SUPP_ATTRIB_LEN;
}

static size_t wpas_qcn_buflen_add_he_2xltf_160_attr(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s || !wpa_s->conf || !wpa_s->conf->conf_extn.he_2xltf_160_80p80_supp ||
	    !(wpa_s->hw_capab & BIT(CAPAB_HE)))
		return 0;

	return QCN_ATTRIB_HDR_LEN + QCN_HE_2XLTF_160_80P80_SUPP_ATTRIB_LEN;
}

static size_t wpas_qcn_buflen_add_he_mcs_12_13_attr(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->conf->conf_extn.he_mcs_12_13_enabled ||
	    !(wpa_s->hw_capab & BIT(CAPAB_HE)))
		return 0;

	return QCN_ATTRIB_HDR_LEN + QCN_HE_MCS_12_13_SUPP_ATTRIB_LEN;
}

size_t wpas_modify_buflen_for_qcn_ie_extn(struct wpa_supplicant *wpa_s)
{
	size_t attr_len = 0;

	attr_len += wpas_qcn_buflen_add_he_mcs_12_13_attr(wpa_s);
	attr_len += wpas_qcn_buflen_add_vht_mcs10_11_attr(wpa_s);
	attr_len += wpas_qcn_buflen_add_he_400ns_sgi_attr(wpa_s);
	attr_len += wpas_qcn_buflen_add_he_2xltf_160_attr(wpa_s);
	if (attr_len)
		attr_len += QCN_IE_HDR_LEN;

	return attr_len;
}

u8 * wpas_eid_qcn_vendor_ie_extn(struct wpa_supplicant *wpa_s, u8 *eid)
{
	struct wpa_supplicant_extn *wpas_extn;
	u8 *len_ptr = NULL;
	u8 *pos = eid;

	if (!eid)
		return eid;

	if (!wpa_s || !wpa_s->conf)
		return eid;

	wpas_extn = &wpa_s->wpas_extn;
	pos = qcn_ie_begin(pos, &len_ptr);
	pos = qcn_eid_add_he_mcs_12_13_attr(wpas_extn->he_mcs_12_13_radio_cap,
					   wpa_s->conf->conf_extn.he_mcs_12_13_enabled &&
					   !!(wpa_s->hw_capab & BIT(CAPAB_HE)),
					   pos);
	pos = qcn_eid_add_vht_mcs10_11_attr(wpa_s->conf->conf_extn.vht_mcs_10_11_supp &&
					    !!(wpa_s->hw_capab & BIT(CAPAB_VHT)), pos);
	pos = qcn_eid_add_he_400ns_sgi_attr(wpa_s->conf->conf_extn.he_400ns_sgi_supp &&
					    !!(wpa_s->hw_capab & BIT(CAPAB_HE)), pos);
	pos = qcn_eid_add_he_2xltf_160_attr(wpa_s->conf->conf_extn.he_2xltf_160_80p80_supp &&
					    !!(wpa_s->hw_capab & BIT(CAPAB_HE)), pos);

	if (pos == eid + QCN_IE_HDR_LEN)
		return eid;

	qcn_ie_end(len_ptr, pos);
	return pos;
}

#ifdef CONFIG_SME
void wpas_add_qcn_ie_probe_req_extn(struct wpa_supplicant *wpa_s,
				    struct wpabuf **extra_ie)
{
	size_t ie_len;
	u8 *eid, *eid_end;

	ie_len = wpas_modify_buflen_for_qcn_ie_extn(wpa_s);
	if (!ie_len)
		return;

	if (wpa_s->drv_max_probe_req_ie_len &&
	    wpabuf_len(*extra_ie) + ie_len > wpa_s->drv_max_probe_req_ie_len)
		return;

	if (wpabuf_resize(extra_ie, ie_len) != 0)
		return;

	if (wpa_s->sme.freq &&
	    wpas_set_he_mcs_12_13_cap_extn(wpa_s, wpa_s->sme.freq))
		return;

	eid = wpabuf_put(*extra_ie, 0);
	eid_end = wpas_eid_qcn_vendor_ie_extn(wpa_s, eid);
	wpabuf_put(*extra_ie, eid_end - eid);
}

void wpas_add_qcn_ie_assoc_req_extn(struct wpa_supplicant *wpa_s)
{
	size_t ie_len;
	u8 *eid, *eid_end;

	ie_len = wpas_modify_buflen_for_qcn_ie_extn(wpa_s);
	if (!ie_len)
		return;

	if (wpa_s->sme.assoc_req_ie_len + ie_len >
	    sizeof(wpa_s->sme.assoc_req_ie))
		return;

	if (wpas_set_he_mcs_12_13_cap_extn(wpa_s, wpa_s->current_bss->freq))
		return;

	eid = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
	eid_end = wpas_eid_qcn_vendor_ie_extn(wpa_s, eid);
	wpa_s->sme.assoc_req_ie_len += eid_end - eid;
}
#endif /* CONFIG_SME */

static u32 get_mcst_from_bss_extn(struct wpa_bss *bss, int link_id)
{
	u32 mcst;

	if (!bss)
		return 0;

	mcst = wpa_bss_get_mld_link_mcst_extn(bss, link_id);
	if (!mcst)
		return 0;

	wpa_printf(MSG_INFO, "%s: link_id=%d bssid=" MACSTR " mcst=%u",
		   __func__, link_id, MAC2STR(bss->bssid), mcst);

	return mcst;
}

u32 wpas_ucode_get_link_mcst_extn(struct wpa_supplicant *wpa_s, int link_id)
{
	u32 mcst;

	if (!wpa_s || link_id < 0 || link_id >= MAX_NUM_MLD_LINKS)
		return 0;

	mcst = get_mcst_from_bss_extn(wpa_s->links[link_id].bss, link_id);
	if (mcst)
		return mcst;

	return get_mcst_from_bss_extn(wpa_s->current_bss, link_id);
}


bool wpas_ucode_freq_range_is_dfs(int center_freq, int bandwidth)
{
	int start_freq, end_freq, subchan_freq;

	if (bandwidth <= 20)
		return ieee80211_is_dfs(center_freq, NULL, 0);

	start_freq = center_freq - bandwidth / 2 + 10;
	end_freq = center_freq + bandwidth / 2 - 10;

	for (subchan_freq = start_freq; subchan_freq <= end_freq;
	     subchan_freq += 20) {
		if (ieee80211_is_dfs(subchan_freq, NULL, 0))
			return true;
	}

	return false;
}

bool wpas_ucode_is_dfs_chandef(int freq, enum chan_width ch_width,
			       int cf1, int cf2)
{
	int bw;

	if (ieee80211_is_dfs(freq, NULL, 0))
		return true;

	bw = channel_width_to_int(ch_width);
	if (bw <= 20)
		return false;

	if (ch_width == CHAN_WIDTH_80P80) {
		if (cf1 > 0 && wpas_ucode_freq_range_is_dfs(cf1, 80))
			return true;
		return cf2 > 0 && wpas_ucode_freq_range_is_dfs(cf2, 80);
	}

	if (bw == 320 && cf2 > 0) {
		if (cf1 > 0 && wpas_ucode_freq_range_is_dfs(cf1, 160))
			return true;
		return wpas_ucode_freq_range_is_dfs(cf2, 80);
	}

	if (bw == 160 && cf2 > 0)
		return wpas_ucode_freq_range_is_dfs(cf2, 160);

	if (cf1 > 0)
		return wpas_ucode_freq_range_is_dfs(cf1, bw);

	if (freq > 0)
		return wpas_ucode_freq_range_is_dfs(freq, bw);

	return false;
}

#ifdef UCODE_SUPPORT
static int wpa_drv_notify_rcsa(struct wpa_supplicant *wpa_s, int freq,
			       u32 chan, u8 cs_count, u32 switch_mode,
			       u8 *opt_ie, size_t opt_ie_len)
{
	struct wpabuf *buf = NULL;
	s8 res;
	s8 target_hw_idx;
	s8 link_id = -1;
	u8 i = 0;
	u32 tx_freq = wpa_s->assoc_freq;
	size_t total_len = RCSA_MIN_FRAME_LEN;

	if (wpa_s->wpa_state != WPA_COMPLETED && !wpa_s->sta_dfs_en)
		return -1;

	wpa_printf(MSG_DEBUG, "rcsa: freq %u chan %u cs_count %u",
		   freq, chan, cs_count);

	if (opt_ie && opt_ie_len > 0)
		total_len += opt_ie_len;

	buf = wpabuf_alloc(total_len);
	if (!buf) {
		wpa_printf(MSG_DEBUG, "rcsa: Memory allocation failed");
		return -1;
	}

	wpabuf_put_u8(buf, WLAN_ACTION_VENDOR_SPECIFIC);
	wpabuf_put_be24(buf, OUI_QCOM);
	wpabuf_put_u8(buf, WLAN_EID_CHANNEL_SWITCH);
	wpabuf_put_u8(buf, IEEE80211_CSA_IE_MIN_LEN);
	wpabuf_put_u8(buf, switch_mode);
	wpabuf_put_u8(buf, 36);
	wpabuf_put_u8(buf, cs_count);

	if (opt_ie && opt_ie_len > 0)
		wpabuf_put_data(buf, opt_ie, opt_ie_len);

	/*
	 * hapd and wpa link ids in a repeater may not map to same
	 * band. Derive the link_id to be used for tx of rcs in wpa side.
	 * consider case for splitphy as well.
	 *
	 * if ml_info ie is present, it means, tx should be link agnistic.
	 * So, no need to derive link_id and tx freq
	 */
	if (wpa_s->valid_links) {
		target_hw_idx = wpa_get_hw_idx_by_freq(wpa_s, freq);
		for_each_link(wpa_s->valid_links, i) {
			if (is_5ghz_freq(wpa_s->links[i].freq) &&
			    (target_hw_idx == wpa_get_hw_idx_by_freq(wpa_s, wpa_s->links[i].freq))) {
				link_id = i;
				tx_freq = wpa_s->links[i].freq;
				break;
			}
		}
	}

	if (!wpa_s->sta_dfs_en) {
		/*
		 * link bitmap in ml info ie is populated by repeater hostapd.
		 * This can't be mapped to root hostapd, so replace bitmap with
		 * wpa_supplicant link bitmap(linkids of repeater wpa and root
		 * hostapd are same).
		 */
		if (optional_ml_info_ie_access(opt_ie, opt_ie_len, &link_id, 1))
			link_id = -1;
	} else {
		tx_freq = freq;
	}
	wpa_printf(MSG_DEBUG, "rcsa: sending on %d link freq %u",
		   link_id, tx_freq);
	wpa_hexdump_buf(MSG_INFO, "rcsa: Action frame payload", buf);
	res = wpa_drv_send_action_extn(wpa_s, tx_freq, 0,
				       wpa_s->bssid,
				       wpa_s->own_addr, wpa_s->bssid,
				       wpabuf_head(buf), wpabuf_len(buf),
				       0, link_id);
	if (res < 0)
		wpa_printf(MSG_ERROR, "rcsa: Failed to send action frame");
	wpabuf_free(buf);

	return res;
}

struct uc_value *uc_wpas_notify_rcsa_extn(struct uc_vm *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = uc_fn_thisval("wpas.iface");
	uc_value_t *info = uc_fn_arg(0);
	u32 freq = 0;
	u32 chan = 0;
	u32 cs_count = 0;
	u32 switch_mode = 0;
	u64 intval;
	uc_value_t *opt_ie_hex;
	const char *hex;
	u8 *opt_ie = NULL;
	size_t opt_ie_len = 0;

	if (!wpa_s || ucv_type(info) != UC_OBJECT)
		return NULL;

	if ((intval = ucv_int64_get(ucv_object_get(info, "csa_count",
						   NULL))) && !errno)
		cs_count = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "frequency",
						   NULL))) && !errno)
		freq = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "channel",
						   NULL))) && !errno)
		chan = intval;
	if ((intval = ucv_int64_get(ucv_object_get(info, "switch_mode", NULL)))
	    && !errno)
		switch_mode = intval;

	opt_ie_hex = ucv_object_get(info, "optional_ie_hex", NULL);
	if (opt_ie_hex && ucv_type(opt_ie_hex) == UC_STRING) {
		hex = ucv_string_get(opt_ie_hex);
		if (hex && *hex) {
			opt_ie_len = os_strlen(hex) / 2;
			opt_ie = os_malloc(opt_ie_len);
			if (!opt_ie)
				return NULL;
			if (hexstr2bin(hex, opt_ie, opt_ie_len)) {
				os_free(opt_ie);
				return NULL;
			}
		}
	}

	wpa_drv_notify_rcsa(wpa_s, freq, chan, cs_count, switch_mode,
			    opt_ie, opt_ie_len);

	os_free(opt_ie);

	return ucv_boolean_new(1);
}

uc_value_t *uc_wpas_notify_uplink_csa_extn(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = uc_fn_thisval("wpas.iface");
	uc_value_t *info = uc_fn_arg(0);
	u32 freq = 0, chan = 0, cs_count = 0;
	u64 intval;
	u32 new_ch_width = 0, ch_seg_0 = 0, ch_seg_1 = 0;
	/* 17 bytes: EID(1)+LEN(1)+OUI(3)+Type(1)+Count(1)+Entry(10) */
	u8 nol_ie_buf[DFS_NOL_IE_FIXED_HDR_LEN + DFS_NOL_IE_ENTRY_LEN];
	size_t nol_ie_len = 0;
	uc_value_t *nol_channel;
	int cac_abort = 0;

	if (!wpa_s || ucv_type(info) != UC_OBJECT)
		return NULL;

	if ((intval = ucv_int64_get(ucv_object_get(info, "csa_count", NULL))) && !errno)
		cs_count = intval;

	if ((intval = ucv_int64_get(ucv_object_get(info, "frequency", NULL))) && !errno)
		freq = intval;

	if ((intval = ucv_int64_get(ucv_object_get(info, "channel", NULL))) && !errno)
		chan = intval;

	if ((intval = ucv_int64_get(ucv_object_get(info, "new_ch_width", NULL))) && !errno)
		new_ch_width = intval;

	if ((intval = ucv_int64_get(ucv_object_get(info, "ch_seg_0", NULL))) && !errno)
		ch_seg_0 = intval;

	if ((intval = ucv_int64_get(ucv_object_get(info, "ch_seg_1", NULL))) && !errno)
		ch_seg_1 = intval;

	if ((intval = ucv_int64_get(ucv_object_get(info, "cac_abort", NULL))) && !errno)
		cac_abort = intval;

	wpa_printf(MSG_INFO,
		   "%s freq=%d chan=%d csa_count=%d new_ch_width=%u ch_seg_0=%u ch_seg_1=%u cac_abort=%d",
		   __func__, freq, chan, cs_count, new_ch_width, ch_seg_0,
		   ch_seg_1, cac_abort);

	if (!wpas_uplink_csa_link_available(wpa_s, freq)) {
		wpa_printf(MSG_WARNING,
			   "%s: BH link is not available for freq=%u, skipping uplink CSA",
			   __func__, freq);
		return ucv_boolean_new(0);
	}

	nol_channel = ucv_object_get(info, "nol_channel", NULL);
	if (nol_channel && ucv_type(nol_channel) == UC_OBJECT) {
		dfs_nol_ie_info nol_info;
		int64_t val;
		int encoded_len;

		os_memset(&nol_info, 0, sizeof(nol_info));

		val = ucv_int64_get(ucv_object_get(nol_channel, "freq", NULL));
		nol_info.freq = (u32)val;

		val = ucv_int64_get(ucv_object_get(nol_channel, "bandwidth", NULL));
		nol_info.bandwidth = (u32)val;

		val = ucv_int64_get(ucv_object_get(nol_channel, "bitmap", NULL));
		nol_info.subchan_bitmap = (u16)val;

		encoded_len = dfs_encode_nol_ie(&nol_info, nol_ie_buf,
						sizeof(nol_ie_buf));
		if (encoded_len > 0) {
			nol_ie_len = (size_t)encoded_len;
			wpa_printf(MSG_INFO,
				   "%s: Encoded NOL entry freq=%u bw=%u bitmap=0x%04x len=%zu",
				   __func__, nol_info.freq, nol_info.bandwidth,
				   nol_info.subchan_bitmap, nol_ie_len);
		} else {
			wpa_printf(MSG_WARNING,
				   "%s: Failed to encode NOL IE", __func__);
		}
	}

	wpa_drv_send_uplink_csa(wpa_s, freq, cs_count, ch_seg_0, ch_seg_1,
				new_ch_width,
				nol_ie_len > 0 ? nol_ie_buf : NULL,
				nol_ie_len, cac_abort);

	return ucv_boolean_new(1);
}

uc_value_t *uc_wpas_iface_reconnect_extn(uc_vm_t *vm, size_t nargs)
{
	struct wpa_supplicant *wpa_s = uc_fn_thisval("wpas.iface");

	if (!wpa_s)
		return NULL;
	wpa_printf(MSG_INFO, "%s disconnect and reconnect backhaul station", __func__);
	wpas_request_disconnection(wpa_s);
	wpas_request_connection(wpa_s);
	return ucv_boolean_new(1);
}

#endif /* UCODE_SUPPORT */

static void wpa_rcsa_get_local_ml_info(struct wpa_supplicant *wpa_s,
					  bool *include_ml_ie,
					  u16 *link_id_bitmap, int *link_id)
{
	u16 i;
	*include_ml_ie = false;
	*link_id_bitmap = 0;

	if (!wpa_s)
		return;

#ifdef CONFIG_IEEE80211BE
	if (!wpa_s->valid_links)
		return;

	for_each_link(wpa_s->valid_links, i) {
		int freq = wpa_s->links[i].freq;

		if (!freq)
			continue;

		if (freq == wpa_s->assoc_freq)
			*link_id = i;

		if (is_5ghz_freq(freq)) {
			if (wpa_s->wpa_state == WPA_STACACING ||
			    wpa_s->links[i].pending_ch_switch_freq) {
				*include_ml_ie = true;
				*link_id_bitmap = BIT(i);
				wpa_printf(MSG_DEBUG,
					   "rcsa: selecting 5G link %u (freq=%d) for ML info IE",
					   i, freq);
			} else {
				*link_id = i;
				 wpa_printf(MSG_DEBUG,
					    "rcsa: 5G link %u (freq=%d) operational, sending RCSA on 5G",
					     i, freq);
			}
			break;
		}
	}
#endif /* CONFIG_IEEE80211BE */
}

static int wpa_rcsa_prepare_nol_ie(const struct dfs_event *radar,
				   u8 *nol_ie_buf,
				   size_t nol_ie_buf_len)
{
	dfs_nol_ie_info nol_info;

	if (!radar)
		return -1;

	if (dfs_prepare_nol_ie_bitmap(NULL, radar->freq,
				      convert_to_oper_chan_width(radar->chan_width),
				      radar->cf1, radar->cf2,
				      radar->radar_bitmap, &nol_info)) {
		wpa_printf(MSG_DEBUG, "rcsa: failed to prepare NOL IE bitmap");
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "rcsa: STA NOL IE freq=%u bw=%u bitmap=0x%04x",
		   nol_info.freq, nol_info.bandwidth, nol_info.subchan_bitmap);

	return dfs_encode_nol_ie(&nol_info, nol_ie_buf, nol_ie_buf_len);
}

static size_t wpa_rcsa_build_opt_ies(struct wpa_supplicant *wpa_s,
				     const u8 *nol_ie, size_t nol_ie_len,
				     u8 *opt_ie, size_t opt_ie_buf_len,
				     int *link_id)
{
	bool include_ml_ie;
	u16 link_id_bitmap;

	wpa_rcsa_get_local_ml_info(wpa_s, &include_ml_ie,
				   &link_id_bitmap, link_id);

	return hostapd_build_rcsa_optional_ies(
		nol_ie_len > 0 ? nol_ie : NULL,
		nol_ie_len > 0 ? nol_ie_len : 0,
		include_ml_ie, link_id_bitmap,
		opt_ie, opt_ie_buf_len);
}

void wpa_rcsa_handle_radar(struct wpa_supplicant *wpa_s,
			   const struct dfs_event *radar)
{
	u8 nol_ie_buf[RCSA_MAX_OPTIONAL_IE_LEN];
	u8 opt_ie[RCSA_MAX_OPTIONAL_IE_LEN];
	int nol_ie_len;
	size_t opt_ie_len;
	u8 chan;
	int link_id = -1;
	unsigned int tx_freq;

	nol_ie_len = wpa_rcsa_prepare_nol_ie(radar,
					     nol_ie_buf,
					     sizeof(nol_ie_buf));
	if (nol_ie_len < 0) {
		if (radar->radar_bitmap)
			return;

		nol_ie_len = 0;
		wpa_printf(MSG_INFO,
			   "rcsa: full-BW radar detected, sending RCSA without NOL IE");
	}

	opt_ie_len = wpa_rcsa_build_opt_ies(wpa_s,
					    nol_ie_buf,
					    (size_t) nol_ie_len,
					    opt_ie,
					    sizeof(opt_ie), &link_id);

	if (link_id >= 0 && link_id < MAX_NUM_MLD_LINKS) {
		if (wpa_s->links[link_id].pending_ch_switch_freq)
			tx_freq = wpa_s->links[link_id].pending_ch_switch_freq;
		else
			tx_freq = wpa_s->links[link_id].freq;
	} else {
		tx_freq = wpa_s->assoc_freq;
	}
	ieee80211_freq_to_chan(tx_freq, &chan);
	wpa_printf(MSG_INFO,
		   "rcsa: radar detected freq %d [%d], sending RCSA Txfreq=%d  bw=%u"
		   "bitmap=0x%04x opt_len=%zu wpa_s->assoc_freq %d chan %d",
		   radar->freq, link_id, tx_freq, radar->chan_width,
		   radar->radar_bitmap, opt_ie_len,wpa_s->assoc_freq, chan);

	/* Currently only one RCSA sent. TODO sending RCSA for 5 TBTT */
#ifdef UCODE_SUPPORT
	wpa_drv_notify_rcsa(wpa_s,
			    tx_freq,
			    chan,
			    HOSTAPD_RCSA_TX_COUNT,
			    HOSTAPD_RCSA_SWITCH_MODE,
			    opt_ie_len ? opt_ie : NULL,
			    opt_ie_len);
#endif
}

/*
 * wpas_country_change_check_current_bss - Disconnect STA on country change.
 *
 * Called from wpa_supplicant_update_channel_list() after hw.modes has been
 * refreshed with the new country's channel flags.  Disconnects immediately
 * when the regulatory country has actually changed so the STA can reconnect
 * under the new domain.  Beacon-hint and other soft regulatory events are
 * ignored — only real country changes trigger a disconnect.
 */
void wpas_country_change_check_current_bss(struct wpa_supplicant *wpa_s,
					   struct channel_list_changed *info)
{
	bool is_afc_regd = false;

	if (!info)
		return;

	/* Only act on real COUNTRY events with an alpha2. */
	if (info->type != REGDOM_TYPE_COUNTRY || !info->alpha2[0] ||
	    info->initiator != REGDOM_SET_BY_DRIVER)
		return;

	if (wpa_s->wpas_extn.is_waiting_for_afc_regd) {
		is_afc_regd = true;
		wpa_s->wpas_extn.is_waiting_for_afc_regd = false;
	}

	/* Only disconnect if the alpha2 actually changed from last time.
	 * The kernel can fire REGDOM_TYPE_COUNTRY multiple times for the
	 * same country (e.g. wiphy-specific events after a global set). */
	if (!os_strncmp(wpa_s->wpas_extn.active_alpha2, info->alpha2, 2) &&
	    is_afc_regd)
		return;

	os_strlcpy(wpa_s->wpas_extn.active_alpha2, info->alpha2,
		   sizeof(wpa_s->wpas_extn.active_alpha2));

	/* Disconnect regardless of wpa_state: even mid-scan or mid-auth the
	 * STA may be targeting a channel now invalid in the new country.
	 * wpa_supplicant_deauthenticate() handles all states cleanly. */
	if (wpa_s->wpa_state == WPA_INACTIVE || wpa_s->wpa_state == WPA_INTERFACE_DISABLED)
		return;

	wpa_msg(wpa_s, MSG_INFO,
		"REGDOM: country changed to %s, disconnecting STA on %s",
		info->alpha2, wpa_s->ifname);

	wpa_supplicant_deauthenticate(wpa_s, WLAN_REASON_DEAUTH_LEAVING);

	wpa_supplicant_req_scan(wpa_s, 0, 100000);
}
