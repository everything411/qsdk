// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#include "includes.h"
#include "common.h"
#include "common/defs.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "common/qca-vendor.h"
#include <ap/hostapd.h>
#include <ap/beacon.h>
#include <ap/dfs.h>
#include <ap/hw_features.h>
#include "utils/wpa_debug.h"
#include "ucode_extn.h"
#include "ubus_extn.h"
#include "dfs_extn.h"
#include "utils/common.h"
#include "utils/eloop.h"
#include "qcn_ie_extn.h"
#include "cmn.h"

#include <ap/ap_drv_ops.h>
#include <drivers/driver.h>

#define IEEE80211_DFS_MIN_CAC_TIME_MS  60000
#define HAPD_DFS_WAIT_FOR_CSA_FROM_ROOT_DUR 500000

#define DFS_WEATHER_RADAR_CHANNEL(freq)  ((freq) >= 5600 && (freq) <= 5650)

/**
 * enum qca_wlan_vendor_attr_dfs_nol_info - DFS NOL information attributes
 * Used for NOL IE in uplink CSA action frames
 *
 * @QCA_WLAN_VENDOR_ATTR_DFS_NOL_FREQ: u32 attribute
 *	Center frequency in MHz of the radar-detected channel
 * @QCA_WLAN_VENDOR_ATTR_DFS_NOL_BW: u32 attribute
 *	Bandwidth of the radar-detected channel (20, 40, 80, 160, 320 MHz)
 * @QCA_WLAN_VENDOR_ATTR_DFS_NOL_BITMAP: u16 attribute
 *	Bitmap indicating affected 20MHz subchannels within the bandwidth
 *	Bit 0 = lowest 20MHz subchannel, Bit 7 = highest (for 160MHz)
 *	Example: 0x03 = bits 0,1 set = first two 20MHz channels affected
 */
enum qca_wlan_vendor_attr_dfs_nol_info {
	QCA_WLAN_VENDOR_ATTR_DFS_NOL_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_DFS_NOL_FREQ = 1,
	QCA_WLAN_VENDOR_ATTR_DFS_NOL_BW = 2,
	QCA_WLAN_VENDOR_ATTR_DFS_NOL_BITMAP = 3,

	/* keep last */
	QCA_WLAN_VENDOR_ATTR_DFS_NOL_AFTER_LAST,
	QCA_WLAN_VENDOR_ATTR_DFS_NOL_MAX =
		QCA_WLAN_VENDOR_ATTR_DFS_NOL_AFTER_LAST - 1,
};

/* QCA Vendor Element type for NOL IE in action frames */
#define QCA_VENDOR_ELEM_NOL_UPDATE 0x01

enum dfs_channel_type_extn {
	DFS_ANY_CHANNEL_EXTN,
	DFS_AVAILABLE_EXTN,	/* non-radar or radar-available */
	DFS_NO_CAC_YET_EXTN,	/* radar-not-yet-available */
};

static struct hostapd_channel_data *
hostapd_dfs_get_chan_data_extn(struct hostapd_hw_modes *mode, int freq,
			       int first_chan_idx)
{
	int i;

	for (i = first_chan_idx; i < mode->num_channels; i++) {
		if (mode->channels[i].freq == freq)
			return &mode->channels[i];
	}

	return NULL;
}

bool hostapd_dfs_skip_wradar_chan_extn(struct hostapd_iface *iface,
				       struct hostapd_hw_modes *mode,
				       struct hostapd_channel_data *chan,
				       int first_chan_idx, int n_chans)
{
	int j;

	if (!iface || !mode || !chan || !iface->iface_extn.dfs_no_wradar)
		return false;

	for (j = 0; j < n_chans; j++) {
		struct hostapd_channel_data *c;

		c = hostapd_dfs_get_chan_data_extn(mode, chan->freq + j * 20,
						   first_chan_idx);
		if (c && DFS_WEATHER_RADAR_CHANNEL(c->freq)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: dfs_no_wradar enabled, skip chandef starting at %d (%d MHz)",
				   chan->chan, chan->freq);
			return true;
		}
	}

	return false;
}

#ifdef CONFIG_QCA_LAB_TEST_FEATURES
static bool hostapd_ignorecac_enabled_extn(struct hostapd_iface *iface)
{
	return iface && iface->iface_extn.ignorecac &&
		hostapd_is_dfs_required(iface) > 0;
}

void hostapd_ignorecac_init_iface_extn(struct hostapd_iface *iface)
{
	if (!iface || !iface->conf)
		return;

	iface->iface_extn.ignorecac = iface->conf->conf_extn.ignorecac;
}

bool hostapd_ignorecac_should_skip_cac_extn(struct hostapd_iface *iface)
{
	return false;
}

void
hostapd_ignorecac_update_freq_params_extn(struct hostapd_iface *iface,
					  struct hostapd_freq_params *freq_params)
{
}

bool hostapd_ignorecac_handle_dfs_extn(struct hostapd_iface *iface,
				       int start_idx, int n_chans)
{
	int i;
	struct hostapd_hw_modes *mode;

	if (!hostapd_ignorecac_enabled_extn(iface))
		return false;

	mode = iface->current_mode;
	wpa_printf(MSG_INFO,
		   "DFS: IGNORECAC is set - skipping CAC on %d MHz",
		   iface->freq);

	if (!mode)
		return true;

	/*
	 * Mark the configured DFS channels as available so subsequent DFS
	 * checks do not immediately re-trigger CAC.
	 */
	for (i = 0; i < n_chans && start_idx + i < mode->num_channels;
	     i++) {
		set_dfs_state_freq(iface,
				   mode->channels[start_idx + i].freq,
				   HOSTAPD_CHAN_DFS_AVAILABLE);
	}

	return true;
}

void hostapd_ignorecac_switch_channel_extn(struct hostapd_data *hapd,
					   struct csa_settings *settings)
{
	struct hostapd_channel_data *chan;

	if (!hapd || !settings || !hapd->iface || !hapd->iface->conf ||
	    !hapd->iface->iface_extn.ignorecac)
		return;

	chan = hapd->iface->current_mode ?
		hw_get_channel_freq(hapd->iface->current_mode->mode,
				    settings->freq_params.freq, NULL,
				    hapd->iface->hw_features,
				    hapd->iface->num_hw_features) : NULL;
	if (chan && (chan->flag & HOSTAPD_CHAN_RADAR)) {
		settings->freq_params.skip_cac = 1;
		wpa_printf(MSG_DEBUG,
			   "CSA: IGNORECAC is set - requesting driver skip CAC for freq=%d",
			   settings->freq_params.freq);
	}
}

bool hostapd_ignorecac_chan_switch_complete_extn(struct hostapd_data *hapd,
						 u8 power_mode_6ghz,
						 int width, int width_device,
						 int is_dfs)
{
	int freq;
	int dfs_width;
	int center_freq1;
	bool device_params_present;

	if (!hapd || !hapd->iface || !hapd->iface->conf || !hapd->iconf ||
	    !hapd->iface->iface_extn.ignorecac)
		return false;

	freq = hapd->iface->freq;
	device_params_present = hapd->iconf->center_freq_device &&
		width_device &&
		width_device != width &&
		hapd->iconf->center_freq_device !=
		hapd->cs_freq_params.center_freq1;
	dfs_width = width;
	center_freq1 = hapd->cs_freq_params.center_freq1;

	wpa_printf(MSG_INFO,
		   "DFS: IGNORECAC is set - skipping CAC after CSA on freq=%d",
		   freq);

	if (device_params_present) {
		dfs_width = width_device;
		center_freq1 = hapd->iconf->center_freq_device;
	}

	set_dfs_state(hapd->iface, freq,
		      hapd->cs_freq_params.ht_enabled,
		      hapd->cs_freq_params.sec_channel_offset,
		      dfs_width,
		      center_freq1,
		      hapd->cs_freq_params.center_freq2,
		      HOSTAPD_CHAN_DFS_AVAILABLE, 0);

	hostapd_cleanup_cs_params(hapd);
	hapd->disable_cu = 1;
	ieee802_11_set_beacon(hapd);
	hostapd_start_device_cac_background(hapd->iface);
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_CSA_FINISHED
		"freq=%d dfs=%d", freq, is_dfs);

	return true;
}
#else /* CONFIG_QCA_LAB_TEST_FEATURES */
void hostapd_ignorecac_init_iface_extn(struct hostapd_iface *iface)
{
}

bool hostapd_ignorecac_should_skip_cac_extn(struct hostapd_iface *iface)
{
	return false;
}

void
hostapd_ignorecac_update_freq_params_extn(struct hostapd_iface *iface,
					  struct hostapd_freq_params *freq_params)
{
}

bool hostapd_ignorecac_handle_dfs_extn(struct hostapd_iface *iface,
				       int start_chan_idx, int n_chans)
{
	return false;
}

void hostapd_ignorecac_switch_channel_extn(struct hostapd_data *hapd,
					   struct csa_settings *settings)
{
}

bool hostapd_ignorecac_chan_switch_complete_extn(struct hostapd_data *hapd,
						 u8 power_mode_6ghz,
						 int width, int width_device,
						 int is_dfs)
{
	return false;
}
#endif /* CONFIG_QCA_LAB_TEST_FEATURES */

bool hostapd_is_ml_info_ie(const u8 *ie, size_t rem_len)
{
	if (!ie || rem_len < 2)
		return false;

	if (ie[0] != WLAN_EID_EXT_CAPAB && ie[0] != WLAN_EID_EXTENSION)
		return false;
	if (ie[2] == WLAN_EID_EXT_MLO_LINK_INFO)
		return true;

	return false;
}

u8 *add_ml_link_info_ie(u8 *buf, size_t buf_len,
			     u16 link_id_bitmap)
{
	u8 *pos = buf;

	if (!pos || buf_len < 5)
		return NULL;

	/*
	 * ML Info IE for RCSA:
	 *   Element ID     : Extension
	 *   Length         : 3
	 *   Extension ID   : Multi-Link
	 *   Link bitmap    : 2-byte LE bitmap
	 */
	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = 3;
	*pos++ = WLAN_EID_EXT_MLO_LINK_INFO;
	WPA_PUT_LE16(pos, link_id_bitmap);
	pos += 2;

	return pos;
}

unsigned int dfs_get_ch_flags_extn(unsigned int cswopts)
{
	unsigned int ch_flags = DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH;

	if (IS_CSH_NONDFS_RANDOM_ENABLED(cswopts) ||
	    IS_CSH_IGNORE_CSA_DFS_ENABLED(cswopts)) {
		wpa_printf(MSG_DEBUG, "DFS: Setting DFS_RANDOM_CH_FLAG_NO_DFS_CH flag");
		ch_flags |= DFS_RANDOM_CH_FLAG_NO_DFS_CH;
	}

	return ch_flags;
}

bool dfs_chan_skip_by_flags_extn(struct hostapd_iface *iface,
				 struct hostapd_channel_data *chan,
				 unsigned int flags)
{
	struct hostapd_hw_modes *mode;
	int n_chans, n_chans_seg1;
	int oper_chwidth, start_chan_idx, start_chan_idx1;
	int first_chan_idx;
	int i;

	if (!iface || !iface->conf || !iface->current_mode)
		return false;

	mode = iface->current_mode;
	oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	n_chans = dfs_get_used_n_chans(iface, &n_chans_seg1, oper_chwidth);

	/* Skip current operating channel and all its bonding sub-channels */
	if (flags & DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH) {
		start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						       oper_chwidth,
						       iface->conf->channel,
						       false);
		if (start_chan_idx >= 0) {
			for (i = 0; i < n_chans && start_chan_idx + i < mode->num_channels; i++) {
				if (chan->chan == mode->channels[start_chan_idx + i].chan) {
					wpa_printf(MSG_DEBUG,
						   "DFS: skipping current operating channel %d (%d)",
						   chan->freq, chan->chan);
					return true;
				}
			}
		}
	}

	/*
	 * Skip DFS/radar channels.
	 * Check every 20 MHz sub-channel in the candidate BW range.
	 * A non-DFS primary channel (e.g. ch 36) can still span into DFS
	 * sub-channels at wide BW (e.g. EHT 160 MHz), so reject the entire
	 * range if any sub-channel carries HOSTAPD_CHAN_RADAR.
	 */
	if (flags & DFS_RANDOM_CH_FLAG_NO_DFS_CH) {
		first_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						       oper_chwidth,
						       chan->chan, false);
		if (first_chan_idx >= 0) {
			for (i = 0; i < n_chans && first_chan_idx + i < mode->num_channels; i++) {
				struct hostapd_channel_data *sub_chan = &mode->channels[first_chan_idx + i];

				if (sub_chan->flag & HOSTAPD_CHAN_RADAR) {
					wpa_printf(MSG_DEBUG,
						   "DFS: skipping channel range starting at %d (%d): sub-channel %d (%d) is DFS",
						   chan->freq, chan->chan,
						   sub_chan->freq, sub_chan->chan);
					return true;
				}
			}
		}
	}

	return false;
}

int handle_action_vs_extn(struct hostapd_data *hapd,
			  struct sta_info *sta,
			  const struct ieee80211_mgmt *mgmt,
			  size_t len, unsigned int freq, bool protected)
{
	const u8 *pos = (const u8 *)mgmt + IEEE80211_HDRLEN;

	if (mgmt->u.action.category == WLAN_ACTION_VENDOR_SPECIFIC) {
		wpa_printf(MSG_DEBUG,"received vendor action");
		/* category(1) + OUI(3) must be present */
		if (len < IEEE80211_HDRLEN + 4) {
			wpa_printf(MSG_DEBUG,
				   "vendor action frame too short (%zu)",
				   len);
			return -1;
		}

		if (WPA_GET_BE24(pos + 1) != OUI_QCOM) {
			wpa_printf(MSG_DEBUG,
				   "vendor action OUI mismatch, ignoring");
			return -1;
		}


		if (hostapd_rcsa_rx_hdl(hapd, (const u8 *) mgmt, len))
			return 0;
	}

	return -1;
}

int dfs_nol_ie_chan_width_to_bw_mhz(enum oper_chan_width chan_width,
				    int freq, int cf1,
				    int *bandwidth_mhz)
{
	if (!bandwidth_mhz)
		return -1;

	switch (chan_width) {
	case CONF_OPER_CHWIDTH_USE_HT:
		/*
		 * CONF_OPER_CHWIDTH_USE_HT covers both 20 MHz and 40 MHz because
		 * convert_to_oper_chan_width() maps CHAN_WIDTH_20, CHAN_WIDTH_20_NOHT,
		 * and CHAN_WIDTH_40 all to this value.
		 *
		 * Disambiguate using cf1 (NL80211_ATTR_CENTER_FREQ1):
		 *   - 20 MHz: center_freq1 == primary freq  (cf1 == freq)
		 *   - 40 MHz: center_freq1 == primary +/- 10  (cf1 != freq)
		 *
		 * cf1 is always populated by nl80211_send_chandef() for both
		 * widths, so this check is reliable.
		 */
		*bandwidth_mhz = (cf1 != freq) ? DFS_NOL_IE_BW_40_MHZ
					       : DFS_NOL_IE_BW_20_MHZ;
		return 0;
	case CONF_OPER_CHWIDTH_40MHZ_6GHZ:
		*bandwidth_mhz = DFS_NOL_IE_BW_40_MHZ;
		return 0;
	case CONF_OPER_CHWIDTH_80MHZ:
		*bandwidth_mhz = DFS_NOL_IE_BW_80_MHZ;
		return 0;
	case CONF_OPER_CHWIDTH_160MHZ:
		*bandwidth_mhz = DFS_NOL_IE_BW_160_MHZ;
		return 0;
	case CONF_OPER_CHWIDTH_320MHZ:
		*bandwidth_mhz = DFS_NOL_IE_BW_320_MHZ;
		return 0;
	default:
		return -1;
	}
}

int dfs_nol_ie_get_subchan_count(enum oper_chan_width chan_width,
				 int freq, int cf1,
				 int *n_subchans)
{
	int bandwidth_mhz;

	if (!n_subchans)
		return -1;

	if (chan_width == CONF_OPER_CHWIDTH_320MHZ && is_5ghz_freq(freq)) {
		/*
		 * UD 5 GHz "320 MHz" operation is the 240 MHz special case and
		 * spans only 12 contiguous 20 MHz subchannels.
		 */
		*n_subchans = 12;
		return 0;
	}

	if (dfs_nol_ie_chan_width_to_bw_mhz(chan_width, freq, cf1,
					    &bandwidth_mhz))
		return -1;

	*n_subchans = bandwidth_mhz / MIN_DFS_SUBCHAN_BW;
	return 0;
}

static int dfs_nol_ie_get_base_freq(int freq, int cf1,
				    enum oper_chan_width chan_width,
				    int *base_freq)
{
	int bandwidth_mhz = 0;

	if (!base_freq)
		return -1;

	if (dfs_nol_ie_chan_width_to_bw_mhz(chan_width, freq, cf1,
					    &bandwidth_mhz))
		return -1;

	/*
	 * cf1 is the operating center frequency in MHz. Convert it to the
	 * first 20 MHz subchannel center so radar bitmap bit positions map to
	 * the actual operating subchannels instead of the primary channel.
	 */
	*base_freq = cf1 - (bandwidth_mhz / 2) + (MIN_DFS_SUBCHAN_BW / 2);
	return 0;
}

int dfs_is_uplink_csa_enabled(struct hostapd_iface *iface)
{
	if (!iface || !iface->conf)
		return 0;

	return iface->conf->conf_extn.uplink_csa;
}

bool hostapd_is_backhaul_sta_configured(struct hostapd_iface *iface)
{
	if (hostapd_ubus_is_bhsta_configured(iface))
		return true;

	return false;
}

void hostapd_trigger_backhaul_sta_disconnect(void *eloop_data, void *user_data)
{
	struct hostapd_iface *iface = eloop_data;

	if (!iface)
		return;

	wpa_printf(MSG_INFO, "CSA is not received from Root AP");
	hostapd_rcsa_handle_csa_timeout(iface);
	hostapd_ucode_trigger_bhsta_disconnect(iface);
}

bool hostapd_uplink_csa_bh_enabled(struct hostapd_iface *iface)
{
	if (dfs_is_uplink_csa_enabled(iface) &&
	    hostapd_is_bh_sta_link_connected_extn(iface))
		return true;
	else
		return false;
}

void hostapd_uplink_cancel_disconnect_timeout_extn(struct hostapd_iface *iface)
{
	if (!iface)
		return;

	if (hostapd_uplink_csa_bh_enabled(iface) ||
	    hostapd_rcsa_tx_bh_enabled(iface)) {
		wpa_printf(MSG_INFO, "chanswitch: cancel radar handling timer");
		eloop_cancel_timeout(hostapd_trigger_backhaul_sta_disconnect, iface, NULL);
		eloop_cancel_timeout(hostapd_rcsa_trigger_channal_change, iface, NULL);
		eloop_cancel_timeout(hostapd_trigger_rcsa_tx, iface, NULL);
		hostapd_set_rcsa_inprogress(iface, false);
	}
}

static void hostapd_notify_uplink_csa(struct hostapd_iface *iface, u8 channel, int freq,
				      u8 new_ch_width, u8 ch_seg_0, u8 ch_seg_1,
				      const dfs_nol_ie_info *nol_info)
{
	wpa_printf(MSG_INFO, "DFS channel uplink notifcation %d", channel);

	if (iface->num_bss && iface->bss && iface->bss[0])
		wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_UPLINK_CSA
			"freq=%d cs_count=%d chan_width=%d cf1=%d cf2=%d",
			freq, HAPD_DFS_UPLINK_CSA_COUNT, new_ch_width,
			ch_seg_0, ch_seg_1);

	if (nol_info)
		wpa_printf(MSG_INFO, "DFS: NOL entry present freq=%u bw=%u bitmap=0x%04x",
			   nol_info->freq, nol_info->bandwidth, nol_info->subchan_bitmap);
	else
		wpa_printf(MSG_INFO, "DFS: No NOL entry");

	wpa_printf(MSG_INFO, "freq=%d channel=%d cs_count=%d chan_width=%d cf1=%d cf2=%d",
		   freq, channel, HAPD_DFS_UPLINK_CSA_COUNT, new_ch_width, ch_seg_0, ch_seg_1);

	/* Notify wpa_supplicant to send uplink csa action frame */
	hostapd_ucode_notify_uplink_csa(iface, EVENT_DFS_UPLINK_CHANNEL_SELECTED, channel,
					freq, HAPD_DFS_UPLINK_CSA_COUNT,
					new_ch_width, ch_seg_0, ch_seg_1,
					nol_info);

	/* start timer for fallback mechanism, disconnect backhaul station when
	 * channel switch is not received from root AP.
	 */
	if (!eloop_is_timeout_registered(hostapd_trigger_backhaul_sta_disconnect,
					 iface, NULL))
		eloop_register_timeout(0, HAPD_DFS_WAIT_FOR_CSA_FROM_ROOT_DUR,
				       hostapd_trigger_backhaul_sta_disconnect,
				       iface, NULL);
}

int hostapd_prepare_nol_ie_bmap_extn(struct hostapd_iface *iface,
				     int channel, int freq,
				     int secondary_channel,
				     int current_vht_oper_chwidth,
				     int oper_centr_freq_seg0_idx,
				     int oper_centr_freq_seg1_idx,
				     u16 punct_bitmap,
				     u16 radar_bitmap_oper)
{

	if (!iface)
		return -EINVAL;

	/*
	 * Only free heap-allocated entries. When entries points to the embedded
	 * nol_info struct it must not be passed to os_free().
	 */
	iface->iface_extn.nol_info_valid = false;
	os_memset(&iface->iface_extn.nol_info, 0, sizeof(iface->iface_extn.nol_info));

	if (!hostapd_uplink_csa_bh_enabled(iface) &&
	    !IS_CSH_RCSA_TO_UPLINK_ENABLED(iface->conf->conf_extn.cswopts))
		return -EINVAL;

	wpa_printf(MSG_INFO,
		   "DFS: Preparing NOL IE with radar_bit_pattern=0x%04x for current channel %d freq %d chwidth %d cf0 %d cf1 %d puct 0x%x ",
		   radar_bitmap_oper, channel, freq,
		   current_vht_oper_chwidth, oper_centr_freq_seg0_idx,
		   oper_centr_freq_seg1_idx, punct_bitmap);
	if (!dfs_prepare_nol_ie_bitmap(iface, freq, current_vht_oper_chwidth,
				      oper_centr_freq_seg0_idx,
				      oper_centr_freq_seg1_idx,
				      radar_bitmap_oper, &iface->iface_extn.nol_info)) {
		iface->iface_extn.nol_info_valid = true;
		wpa_printf(MSG_INFO,
			   "DFS: NOL IE prepared - freq=%u bw=%u bitmap=0x%04x",
			   iface->iface_extn.nol_info.freq, iface->iface_extn.nol_info.bandwidth,
			   iface->iface_extn.nol_info.subchan_bitmap);
	} else {
		wpa_printf(MSG_ERROR, "DFS: Failed to prepare NOL IE bitmap");
	}

	return 0;
}

int hostapd_send_uplink_csa_extn(struct hostapd_iface *iface,
				 int channel, int freq,
				 int secondary_channel,
				 u8 current_vht_oper_chwidth,
				 u8 oper_centr_freq_seg0_idx,
				 u8 oper_centr_freq_seg1_idx,
				 u16 punct_bitmap)
{
	if (!iface)
		return -EINVAL;

	if (!hostapd_uplink_csa_bh_enabled(iface))
		return -EINVAL;

	wpa_printf(MSG_INFO, "DFS: Sending uplink CSA, NOL entry valid=%d",
		   iface->iface_extn.nol_info_valid);

	hostapd_notify_uplink_csa(iface, channel, freq,
				  current_vht_oper_chwidth,
				  oper_centr_freq_seg0_idx,
				  oper_centr_freq_seg1_idx,
				  iface->iface_extn.nol_info_valid ? &iface->iface_extn.nol_info : NULL);
	return 0;
}

/**
 * hostapd_handle_missing_nol_ie - Derive full-band NOL info when IE is absent
 * @iface: hostapd interface context
 * @nol_info: output NOL entry to populate
 *
 * When uplink CSA or RCSA is received without an explicit NOL IE, treat it as
 * full-band radar for the current operating channel and populate @nol_info
 * with the corresponding primary frequency, operating bandwidth, and complete
 * subchannel bitmap.
 */
static void
hostapd_handle_missing_nol_ie(struct hostapd_iface *iface,
			      dfs_nol_ie_info *nol_info)
{
	struct hostapd_hw_modes *mode;
	enum oper_chan_width oper_chwidth;
	int center_freq1 = 0;
	int bandwidth_mhz = 0;
	int n_subchans = 0;
	int start_chan_idx = 0;
	int seg1_start = 0;

	if (!iface || !nol_info)
		return;

	mode = iface->current_mode;
	oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	center_freq1 = iface->freq;
	if (oper_chwidth == CONF_OPER_CHWIDTH_USE_HT &&
	    iface->conf->secondary_channel)
		center_freq1 += iface->conf->secondary_channel * 10;

	if (!mode ||
	    dfs_nol_ie_chan_width_to_bw_mhz(oper_chwidth, iface->freq,
					    center_freq1, &bandwidth_mhz) ||
	    dfs_nol_ie_get_subchan_count(oper_chwidth, iface->freq,
					 center_freq1, &n_subchans) ||
	    n_subchans <= 0 || n_subchans > DFS_MAX_20M_SUB_CH) {
		wpa_printf(MSG_WARNING,
			   "uplink_csa: Failed to derive full-BW NOL fallback");
		return;
	}

	start_chan_idx = dfs_get_start_chan_idx(iface, &seg1_start, oper_chwidth,
						iface->conf->channel, false);
	if (start_chan_idx < 0 || start_chan_idx >= mode->num_channels) {
		wpa_printf(MSG_WARNING,
			   "uplink_csa: Failed to derive start channel for full-BW NOL");
		return;
	}

	nol_info->freq = mode->channels[start_chan_idx].freq;
	nol_info->bandwidth = bandwidth_mhz;
	nol_info->subchan_bitmap = DFS_NOL_IE_BITMAP_MASK(n_subchans);

	if (dfs_process_nol_ie_bitmap(iface, nol_info) == 0) {
		wpa_printf(MSG_INFO,
			   "uplink_csa: Missing NOL IE treated as full-BW radar");
	} else {
		wpa_printf(MSG_WARNING,
			   "uplink_csa: Failed to update full-BW NOL");
	}
}

/*
 * IEEE 802.11 Spectrum Management Action frame specifics.
 *
 * References:
 * - IEEE Std 802.11, "Spectrum Management" Action frames
 * - IEEE Std 802.11, "Channel Switch Announcement" element
 * - IEEE Std 802.11, "Wide Bandwidth Channel Switch" element
 */
#define IEEE80211_SPECTRUM_MGMT_ACTION_CHANNEL_SWITCH 4

/* Minimum fixed fields after the 802.11 header for Action frames: Category+Action */
#define IEEE80211_ACTION_FRAME_MIN_FIXED_FIELDS 2

/* Wide Bandwidth Channel Switch element (WLAN_EID_WIDE_BW_CHSWITCH) fixed layout. */
#define IEEE80211_WB_CSA_IE_MIN_LEN 3
#define IEEE80211_WB_CSA_IE_CH_WIDTH_OFFSET 2
#define IEEE80211_WB_CSA_IE_CF0_OFFSET 3
#define IEEE80211_WB_CSA_IE_CF1_OFFSET 4
#define IEEE80211_WB_CSA_IE_TOTAL_LEN 5

static
int hostapd_dfs_prepare_channel_switch_settings(struct hostapd_iface *iface,
						int channel, int freq,
						int secondary_channel,
						u8 oper_chwidth,
						u8 oper_centr_freq_seg0_idx,
						u8 oper_centr_freq_seg1_idx,
						u16 punct_bitmap,
						struct csa_settings *settings)
{
	struct hostapd_hw_modes *cmode = iface->current_mode;
	int ieee80211_mode = IEEE80211_MODE_AP;
	int err;

	os_memset(settings, 0, sizeof(*settings));
	settings->cs_count = 5;
	settings->block_tx = 1;
	settings->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (iface->bss[0]->conf->mld_ap)
		settings->link_id = iface->bss[0]->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_MESH
	if (iface->mconf)
		ieee80211_mode = IEEE80211_MODE_MESH;
#endif /* CONFIG_MESH */

	err = hostapd_set_freq_params(&settings->freq_params,
				      iface->conf->hw_mode,
				      freq, channel,
				      iface->conf->enable_edmg,
				      iface->conf->edmg_channel,
				      iface->conf->ieee80211n,
				      iface->conf->ieee80211ac,
				      iface->conf->ieee80211ax,
				      iface->conf->ieee80211be,
				      iface->conf->ieee80211bn,
				      secondary_channel,
				      oper_chwidth,
				      oper_centr_freq_seg0_idx,
				      oper_centr_freq_seg1_idx,
				      cmode->vht_capab,
				      &cmode->he_capab[ieee80211_mode],
				      &cmode->eht_capab[ieee80211_mode],
				      &cmode->uhr_capab[ieee80211_mode],
				      punct_bitmap | iface->radar_bit_pattern,
				      iface->conf->he_6ghz_reg_pwr_type,
				      0, 0,
				      iface->conf->bandwidth_device,
				      iface->conf->center_freq_device);
	if (err) {
		wpa_printf(MSG_ERROR,
			   "DFS failed to calculate CSA freq params");
		hostapd_disable_iface(iface);
		return err;
	}

	return 0;
}

int hostapd_dfs_abort_cac_and_request_channel_switch(struct hostapd_iface *iface,
						     int channel, int freq,
						     int secondary_channel,
						     u8 current_vht_oper_chwidth,
						     u8 oper_centr_freq_seg0_idx,
						     u8 oper_centr_freq_seg1_idx,
						     u16 punct_bitmap)
{
	struct csa_settings settings;
	u8 op_class, chan;
	int err;

	wpa_printf(MSG_DEBUG, "DFS will switch to a new channel %d", channel);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NEW_CHANNEL
		"freq=%d chan=%d sec_chan=%d", freq, channel,
		secondary_channel);

	if (ieee80211_freq_to_channel_ext(freq, secondary_channel,
				current_vht_oper_chwidth, &op_class,
				&chan) != NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG, "Update op_class %d->%d",
				iface->conf->op_class, op_class);
		iface->conf->op_class = op_class;
	}

	err = hostapd_dfs_prepare_channel_switch_settings(iface, channel,
							  freq, secondary_channel,
							  current_vht_oper_chwidth,
							  oper_centr_freq_seg0_idx,
							  oper_centr_freq_seg1_idx,
							  punct_bitmap, &settings);
	if (err)
		return err;

	return hostapd_abort_cac_for_channel_switch(iface, &settings);
}

void hostapd_handle_action_csa(struct hostapd_data *hapd,
			       const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *)buf;
	struct hostapd_data *link_hapd = NULL;
	struct hostapd_iface *iface;
	enum oper_chan_width ch_width;
	const u8 *wb_cs_ie = NULL;
	const u8 *cs_ie = NULL;
	const u8 *pos, *end;
	int freq, sec_chan;
	int link_id = -1;
	u8 cf0, cf1;
	const u8 *vendor_ie = NULL;
	dfs_nol_ie_info nol_info;

	if (!hapd || !hapd->iface || !buf)
		return;
	iface = hapd->iface;
	os_memset(&nol_info, 0, sizeof(nol_info));

	if (len < IEEE80211_HDRLEN + IEEE80211_ACTION_FRAME_MIN_FIXED_FIELDS) {
		wpa_printf(MSG_DEBUG, "invalid action frame received");
		return;
	}

	if (mgmt->u.action.u.spectrum_mgmt.action !=
	    IEEE80211_SPECTRUM_MGMT_ACTION_CHANNEL_SWITCH) {
		wpa_printf(MSG_DEBUG, "unsupported action frame received %u",
			   mgmt->u.action.u.spectrum_mgmt.action);
		return;
	}

	wpa_printf(MSG_DEBUG, "uplink_csa: action frame received");

	end = buf + len;
	pos = mgmt->u.action.u.spectrum_mgmt.variable;
	len = end - pos;

	cs_ie = get_ie(pos, len, WLAN_EID_CHANNEL_SWITCH);
	if (!cs_ie || cs_ie[1] < IEEE80211_CSA_IE_MIN_LEN)
		return;

	u8 new_chan = cs_ie[IEEE80211_CSA_IE_NEW_CHANNEL_OFFSET];

	wpa_printf(MSG_DEBUG, "uplink_csa: channel switch to %u", new_chan);
	pos = cs_ie + IEEE80211_CSA_IE_TOTAL_LEN;
	len = end - pos;

	wb_cs_ie = get_ie(pos, len, WLAN_EID_WIDE_BW_CHSWITCH);
	if (wb_cs_ie && wb_cs_ie[1] >= IEEE80211_WB_CSA_IE_MIN_LEN) {
		ch_width = wb_cs_ie[IEEE80211_WB_CSA_IE_CH_WIDTH_OFFSET];
		cf0 = wb_cs_ie[IEEE80211_WB_CSA_IE_CF0_OFFSET];
		cf1 = wb_cs_ie[IEEE80211_WB_CSA_IE_CF1_OFFSET];
		/*
		 * Per IEEE Std 802.11-2024, Table 9-316 (VHT Operation Information):
		 * width=1 is used for both 80 MHz and 160 MHz. The two are
		 * distinguished by cf1 (CCFS1): non-zero cf1 means 160 MHz, where
		 * cf0 is the 80 MHz segment center and cf1 is the 160 MHz center.
		 * Zero cf1 means plain 80 MHz.
		 */
		if (ch_width == CONF_OPER_CHWIDTH_80MHZ && cf1 != 0) {
			ch_width = CONF_OPER_CHWIDTH_160MHZ;
			wpa_printf(MSG_DEBUG,
				   "uplink_csa: wide band ie: cf0=%u cf1=%u, reclassifyed as 160 MHz (new_chan=%u chwidth=%d)",
				   cf0, cf1, new_chan, ch_width);
		} else {
			wpa_printf(MSG_DEBUG, "uplink_csa: wide band ie: cf0=%u cf1=%u chwidth=%d",
				   cf0, cf1, ch_width);
		}
	} else {
		wpa_printf(MSG_DEBUG, "uplink_csa: no wide band ie, assuming 20MHz");
		ch_width = 0;
		cf0 = new_chan;
		cf1 = 0;
	}

	/* Width 0 in the WB IE covers HT operation; use seg0 to detect HT40. */
	if (!wb_cs_ie) {
		sec_chan = 0;
	} else if (cf0 < new_chan) {
		sec_chan = -1;
	} else if (cf0 > new_chan) {
		sec_chan = 1;
	} else {
		sec_chan = 0;
	}

	if (ch_width == CONF_OPER_CHWIDTH_160MHZ) {
		/*
		 * Convert the WB IE CCFS0/CCFS1 representation back to the
		 * internal hostapd 160 MHz oper seg representation expected by
		 * hostapd_set_freq_params().
		 */
		cf0 = cf1;
		cf1 = 0;
	}

	wpa_printf(MSG_DEBUG, "uplink_csa: channel change prams: cf0 %u cf1 %u sec %d chwidth %u",
		   cf0, cf1, sec_chan, ch_width);

	if (!wb_cs_ie) {
		pos = cs_ie + IEEE80211_CSA_IE_TOTAL_LEN;
		len = end - pos;
	} else {
		pos = wb_cs_ie + IEEE80211_WB_CSA_IE_TOTAL_LEN;
		len = end - pos;
	}

	link_hapd = get_link_hapd(hapd, pos, len, &link_id);
	if (link_hapd) {
		hapd = link_hapd;
		iface = hapd->iface;
		wpa_printf(MSG_INFO,
			   "uplink_csa: using iface %s for link_id=%d",
			   hapd->conf->iface, link_id);
	}

	freq = hostapd_hw_get_freq(hapd, new_chan);
	if (freq <= 0) {
		wpa_printf(MSG_WARNING,
			   "uplink_csa: channel %u not found on iface %s, dropping CSA",
			   new_chan, hapd->conf->iface);
		return;
	}

	while (len >= 2) {
		u8 ie_id = *pos;
		u8 ie_len = *(pos + 1);

		if (ie_len > len - 2)
			break;

		if (ie_id == WLAN_EID_VENDOR_SPECIFIC && ie_len >= 4) {
			const u8 *oui = pos + 2;

			if (WPA_GET_BE24(oui) == OUI_QCA &&
			    oui[3] == QCA_VENDOR_ELEM_NOL_UPDATE) {
				vendor_ie = pos;
				wpa_printf(MSG_INFO,
					   "uplink_csa: Found NOL IE, len=%u",
					   ie_len);
				break;
			}
		}

		pos += 2 + ie_len;
		len -= 2 + ie_len;
	}

	if (vendor_ie) {
		u8 vendor_ie_len = vendor_ie[1] + 2;

		if (dfs_decode_nol_ie(vendor_ie,
				      vendor_ie_len, &nol_info) == 0) {
			wpa_printf(MSG_INFO,
				   "uplink_csa: Decoded NOL entry freq=%u bw=%u bitmap=0x%04x",
				   nol_info.freq, nol_info.bandwidth, nol_info.subchan_bitmap);

			if (dfs_process_nol_ie_bitmap(iface, &nol_info) == 0) {
				wpa_printf(MSG_INFO,
					   "uplink_csa: Successfully updated NOL from uplink CSA");
			} else {
				wpa_printf(MSG_WARNING,
					   "uplink_csa: Failed to update NOL");
			}

			/*
			 * Store the decoded NOL entry so that when this repeater
			 * forwards the uplink CSA to the root AP via
			 * hostapd_send_uplink_csa_extn(), the NOL IE is included.
			 */
			iface->iface_extn.nol_info = nol_info;
			iface->iface_extn.nol_info_valid = true;
		} else {
			wpa_printf(MSG_WARNING,
				   "uplink_csa: Failed to decode NOL IE");
			iface->iface_extn.nol_info_valid = false;
		}
	} else {
		wpa_printf(MSG_INFO,
			   "uplink_csa: No NOL IE found in received uplink CSA");
		hostapd_handle_missing_nol_ie(iface, &nol_info);
	}

	if (iface->cac_started || iface->bootup_cac_in_progress) {
		wpa_printf(MSG_DEBUG,
			   "uplink_csa: CSA on iface:%s for link_id=%d, freq:%d aborting CAC before channel switch",
			   hapd->conf->iface, link_id, freq);
		hostapd_dfs_abort_cac_and_request_channel_switch(iface, new_chan,
								 freq, sec_chan,
								 ch_width, cf0,
								 cf1, 0);
	} else {
		hostapd_dfs_request_channel_switch(iface, new_chan, freq,
						   sec_chan, ch_width, cf0,
						   cf1, 0);
	}

	/* Clear the forwarding entry after the uplink CSA has been sent */
	iface->iface_extn.nol_info_valid = false;
}

/**
 * hostapd_uplink_csa_hdl - Handle uplink CSA action frame at intermediate repeater
 * @hapd: hostapd BSS data structure
 * @buf: Raw 802.11 frame buffer
 * @len: Length of the frame buffer
 *
 * Called when an intermediate repeater (e.g. repeater 1 in a
 * repeater2 -> repeater1 -> root topology) receives an uplink CSA
 * action frame from a downstream repeater. This function:
 *   1. Forwards the complete uplink CSA frame to the root AP.
 *   2. Parses the NOL IE in the received frame and marks the affected
 *      channels as DFS_UNAVAILABLE by calling set_dfs_state() via
 *      dfs_process_nol_ie_bitmap().
 *
 * The NOL IE is decoded before forwarding so that the intermediate
 * repeater's local channel list is updated even when it is not the
 * originator of the radar detection.
 *
 * Return: true if the frame was handled, false otherwise.
 */
bool hostapd_uplink_csa_hdl(struct hostapd_data *hapd,
			    const u8 *buf, size_t len)
{
	if (!hapd || !hapd->iface || !buf)
		return false;

	if (!dfs_is_uplink_csa_enabled(hapd->iface))
		return false;

	wpa_printf(MSG_INFO,
		   "uplink_csa_extn: Received uplink CSA from downstream repeater, forwarding to root AP");

	/*
	 * hostapd_handle_action_csa() will:
	 *   1. Decode the NOL IE from the received frame.
	 *   2. Call dfs_process_nol_ie_bitmap() to mark affected channels
	 *      as DFS_UNAVAILABLE in the local channel list.
	 *   3. Store the decoded NOL entry in iface->iface_extn.nol_info.
	 *   4. Call hostapd_dfs_request_channel_switch() which invokes
	 *      hostapd_send_uplink_csa_extn() to forward the frame with
	 *      the NOL IE to the root AP.
	 */
	hostapd_handle_action_csa(hapd, buf, len);
	return true;
}

int handle_action_extn(struct hostapd_data *hapd,
		       const struct ieee80211_mgmt *mgmt, size_t len,
		       unsigned int freq)
{
	if (!hapd || !mgmt)
		return 0;

	switch (mgmt->u.action.category) {
		case WLAN_ACTION_SPECTRUM_MGMT:
			if (hostapd_uplink_csa_hdl(hapd, (const u8 *) mgmt, len))
				 return 1;
			break;
		default:
			return 0;
	}
	return 0;
}

/**
 * dfs_prepare_nol_ie_bitmap - Prepare NOL IE from radar detection
 *
 * This function creates a NOL IE entry based on the RCSA design pattern.
 * It converts radar detection information into a subchannel bitmap format.
 */
int dfs_prepare_nol_ie_bitmap(struct hostapd_iface *iface, int freq,
			      enum oper_chan_width chan_width, int cf1, int cf2,
			      u16 radar_bitmap,
			      dfs_nol_ie_info *nol_info)
{
	int base_freq = 0;
	int bandwidth_mhz;
	int n_subchans;
	u16 bitmap_mask;
	u16 radar_bitmap_oper;
	int start_subchan_idx;
	int end_subchan_idx;
	int contiguous_count;
	u16 contiguous_bitmap;

	if (!nol_info) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: Invalid parameters");
		return -1;
	}

	os_memset(nol_info, 0, sizeof(*nol_info));

	if (dfs_nol_ie_chan_width_to_bw_mhz(chan_width, freq, cf1, &bandwidth_mhz)) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: Unsupported channel width %d",
			   chan_width);
		return -1;
	}

	if (dfs_nol_ie_get_subchan_count(chan_width, freq, cf1, &n_subchans) ||
	    n_subchans <= 0 || n_subchans > DFS_MAX_20M_SUB_CH) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: Invalid subchannel count %d",
			   n_subchans);
		return -1;
	}

	if (dfs_nol_ie_get_base_freq(freq, cf1, chan_width, &base_freq)) {
		wpa_printf(MSG_ERROR,
			   "DFS NOL IE: Failed to derive base frequency");
		return -1;
	}

	bitmap_mask = DFS_NOL_IE_BITMAP_MASK(n_subchans);
	radar_bitmap_oper = radar_bitmap & bitmap_mask;

	if (!radar_bitmap_oper) {
		wpa_printf(MSG_DEBUG,
			   "DFS NOL IE: radar_bitmap is 0 after masking (0x%04x)",
			   radar_bitmap);
		return -1;
	}

	start_subchan_idx = 0;
	while (start_subchan_idx < n_subchans &&
	       !(radar_bitmap_oper & (1U << start_subchan_idx)))
		start_subchan_idx++;

	if (start_subchan_idx >= n_subchans) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: No radar-affected subchannel found");
		return -1;
	}

	/*
	 * Find contiguous radar-affected subchannels starting from
	 * start_subchan_idx. The NOL IE bitmap should represent contiguous
	 * affected subchannels only.
	 */
	end_subchan_idx = start_subchan_idx;
	while (end_subchan_idx < n_subchans &&
	       (radar_bitmap_oper & (1U << end_subchan_idx)))
		end_subchan_idx++;

	contiguous_count = end_subchan_idx - start_subchan_idx;

	/*
	 * Create contiguous bitmap starting from bit 0.
	 * Example: if subchannels 1 and 2 are affected (bits 1-2 set in
	 * radar_bitmap),
	 * the NOL IE bitmap should be 0b0011 (bits 0-1 set).
	 */
	contiguous_bitmap = (u16) ((1U << contiguous_count) - 1);

	/*
	 * RCSA design for NOL IE
	 * - bandwidth: Always 20 MHz (MIN_DFS_SUBCHAN_BW)
	 * - freq: Center frequency of the first radar-affected 20 MHz
	           subchannel
	 * - subchan_bitmap: Contiguous bitmap starting from freq (max 3 bits)
	 *
	 * Example for 80 MHz at cf1=5530, radar_bitmap=0b0110 (bits 1-2):
	 *   - Subchannels: [5500, 5520, 5540, 5560]
	 *   - Affected: 5520 (bit1), 5540 (bit2)
	 *   - NOL IE: freq=5520, bandwidth=20, bitmap=0b0011
	 */
	nol_info->freq = base_freq + (start_subchan_idx * MIN_DFS_SUBCHAN_BW);
	nol_info->bandwidth = MIN_DFS_SUBCHAN_BW;
	nol_info->subchan_bitmap = contiguous_bitmap;

	wpa_printf(MSG_DEBUG,
		   "DFS NOL IE: Input - cf1=%d bw=%d radar_bitmap=0x%04x",
		   cf1, bandwidth_mhz, radar_bitmap);

	wpa_printf(MSG_DEBUG,
		   "DFS NOL IE: Calculated - base_freq=%d primary_freq=%d n_subchans=%d",
		   base_freq, freq, n_subchans);

	wpa_printf(MSG_DEBUG,
		   "DFS NOL IE: Affected subchannels - start_idx=%d end_idx=%d count=%d",
		   start_subchan_idx, end_subchan_idx, contiguous_count);

	wpa_printf(MSG_INFO,
		   "DFS NOL IE: Prepared - freq=%u bw=%u bitmap=0x%04x",
		   nol_info->freq, nol_info->bandwidth,
		   nol_info->subchan_bitmap);

	return 0;
}

/**
 * dfs_encode_nol_ie - Encode a single NOL entry into vendor-specific IE format
 *
 * Wire format (17 bytes total):
 *   Byte  0   : EID  = WLAN_EID_VENDOR_SPECIFIC (221)
 *   Byte  1   : LEN  = 15  (OUI(3)+Type(1)+Count(1)+Entry(10))
 *   Bytes 2-4 : OUI  = 00:13:74 (QCA)
 *   Byte  5   : Type = QCA_VENDOR_ELEM_NOL_UPDATE (0x01)
 *   Byte  6   : Count = 1 (always)
 *   Bytes 7-10: freq  (u32 LE)
 *   Bytes 11-14: bw   (u32 LE)
 *   Bytes 15-16: subchan_bitmap (u16 LE)
 *
 * @nol_info: pointer to the single NOL entry to encode (must not be NULL)
 * @buf:      output buffer (must be at least DFS_NOL_IE_FIXED_HDR_LEN +
 *            DFS_NOL_IE_ENTRY_LEN = 17 bytes)
 * @buf_len:  size of @buf in bytes
 *
 * Returns total bytes written (17) on success, -1 on error.
 */
int dfs_encode_nol_ie(const dfs_nol_ie_info *nol_info, u8 *buf,
		      size_t buf_len)
{
	u8 *pos = buf;
	/* Fixed total: EID(1)+LEN(1)+OUI(3)+Type(1)+Count(1)+Entry(10) = 17 */
	const size_t required_len = DFS_NOL_IE_FIXED_HDR_LEN + DFS_NOL_IE_ENTRY_LEN;

	if (!nol_info || !buf) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: NULL parameter in encode");
		return -1;
	}

	if (buf_len < required_len) {
		wpa_printf(MSG_ERROR,
			   "DFS NOL IE: Buffer too small (%zu < %zu)",
			   buf_len, required_len);
		return -1;
	}

	/* EID */
	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	/* LEN field: everything after EID+LEN = OUI(3)+Type(1)+Count(1)+Entry(10) = 15 */
	*pos++ = (u8)(DFS_NOL_IE_FIXED_HDR_LEN - 2 + DFS_NOL_IE_ENTRY_LEN);
	/* OUI */
	WPA_PUT_BE24(pos, OUI_QCA);
	pos += 3;
	/* OUI Type */
	*pos++ = QCA_VENDOR_ELEM_NOL_UPDATE;
	/* Count: always 1 */
	*pos++ = 1;
	/* Entry */
	WPA_PUT_LE32(pos, nol_info->freq);
	pos += DFS_NOL_IE_U32_LEN;
	WPA_PUT_LE32(pos, nol_info->bandwidth);
	pos += DFS_NOL_IE_U32_LEN;
	WPA_PUT_LE16(pos, nol_info->subchan_bitmap);
	pos += DFS_NOL_IE_U16_LEN;

	wpa_printf(MSG_INFO,
		   "DFS NOL IE: Encoded - freq=%u bw=%u bitmap=0x%04x total=%zu bytes",
		   nol_info->freq, nol_info->bandwidth, nol_info->subchan_bitmap,
		   (size_t)(pos - buf));

	return (int)(pos - buf);
}

/**
 * dfs_decode_nol_ie - Decode a single NOL entry from vendor-specific IE
 *
 * Validates the wire format and extracts the first (and only expected) entry
 * into @nol_info.  The count field in the IE must be exactly 1; any other
 * value is rejected so that a malformed or multi-entry IE from an unknown
 * sender cannot silently corrupt state.
 *
 * @ie:      pointer to the start of the vendor IE (EID byte)
 * @ie_len:  total length including EID and LEN bytes
 *           (must equal DFS_NOL_IE_FIXED_HDR_LEN + DFS_NOL_IE_ENTRY_LEN = 17)
 * @nol_info: output; filled on success, zeroed on any error
 *
 * Returns 0 on success, -1 on any validation failure.
 */
int dfs_decode_nol_ie(const u8 *ie, size_t ie_len,
		      dfs_nol_ie_info *nol_info)
{
	const u8 *pos;
	u8 count;
	/* Expected total: EID(1)+LEN(1)+OUI(3)+Type(1)+Count(1)+Entry(10) = 17 */
	const size_t expected_total = DFS_NOL_IE_FIXED_HDR_LEN + DFS_NOL_IE_ENTRY_LEN;

	if (!ie || !nol_info) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: NULL parameter in decode");
		return -1;
	}

	os_memset(nol_info, 0, sizeof(*nol_info));

	if (ie_len < DFS_NOL_IE_MIN_LEN) {
		wpa_printf(MSG_ERROR,
			   "DFS NOL IE: IE too short (%zu < %u)",
			   ie_len, DFS_NOL_IE_MIN_LEN);
		return -1;
	}

	if (ie[0] != WLAN_EID_VENDOR_SPECIFIC) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: Invalid element ID %u", ie[0]);
		return -1;
	}

	/* LEN field covers everything after EID+LEN bytes */
	if ((size_t)(ie[1] + 2) != ie_len) {
		wpa_printf(MSG_ERROR,
			   "DFS NOL IE: LEN field mismatch: ie[1]+2=%u ie_len=%zu",
			   ie[1] + 2, ie_len);
		return -1;
	}

	if (ie_len != expected_total) {
		wpa_printf(MSG_ERROR,
			   "DFS NOL IE: Unexpected IE length %zu (expected %zu)",
			   ie_len, expected_total);
		return -1;
	}

	pos = ie + 2;

	if (WPA_GET_BE24(pos) != OUI_QCA) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: Invalid OUI");
		return -1;
	}
	pos += 3;

	if (*pos != QCA_VENDOR_ELEM_NOL_UPDATE) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: Invalid OUI type %u", *pos);
		return -1;
	}
	pos++;

	count = *pos++;
	if (count != 1) {
		wpa_printf(MSG_ERROR,
			   "DFS NOL IE: Expected count=1, got %u", count);
		return -1;
	}

	/* Bounds already guaranteed by the ie_len == expected_total check above */
	nol_info->freq = WPA_GET_LE32(pos);
	pos += DFS_NOL_IE_U32_LEN;
	nol_info->bandwidth = WPA_GET_LE32(pos);
	pos += DFS_NOL_IE_U32_LEN;
	nol_info->subchan_bitmap = WPA_GET_LE16(pos);

	wpa_printf(MSG_INFO,
		   "DFS NOL IE: Decoded - freq=%u bw=%u bitmap=0x%04x",
		   nol_info->freq, nol_info->bandwidth, nol_info->subchan_bitmap);

	return 0;
}

/**
 * dfs_build_notify_freq_params - Build hostapd_freq_params for radar notification
 * @hapd: BSS data
 * @freq_params: Output frequency parameters
 * @start_freq: Output base frequency of first 20 MHz subchannel
 * @n_subchans: Output number of 20 MHz subchannels in operating bandwidth
 *
 * Returns: 0 on success, -1 on failure
 */
static int
dfs_build_notify_freq_params(struct hostapd_data *hapd,
			     struct hostapd_freq_params *freq_params,
			     int *start_freq, int *n_subchans)
{
	struct hostapd_iface *iface;
	struct hostapd_config *iconf;
	struct hostapd_hw_modes *mode;
	enum oper_chan_width oper_chwidth;

	if (!hapd || !hapd->iface || !hapd->iconf || !freq_params ||
	    !start_freq || !n_subchans)
		return -1;

	iface = hapd->iface;
	iconf = hapd->iconf;
	mode = iface->current_mode;
	if (!mode)
		return -1;
	oper_chwidth = hostapd_get_oper_chwidth(iconf);

	if (hostapd_set_freq_params(freq_params, iconf->hw_mode, iface->freq,
				    iconf->channel, iconf->enable_edmg,
				    iconf->edmg_channel, iconf->ieee80211n,
				    iconf->ieee80211ac, iconf->ieee80211ax,
				    iconf->ieee80211be, iconf->ieee80211bn,
				    iconf->secondary_channel,
				    oper_chwidth,
				    hostapd_get_oper_centr_freq_seg0_idx(iconf),
				    hostapd_get_oper_centr_freq_seg1_idx(iconf),
				    mode->vht_capab,
				    &mode->he_capab[IEEE80211_MODE_AP],
				    &mode->eht_capab[IEEE80211_MODE_AP],
				    &mode->uhr_capab[IEEE80211_MODE_AP],
				    hostapd_get_punct_bitmap(hapd),
				    iconf->he_6ghz_reg_pwr_type,
#ifdef CONFIG_IEEE80211BN
				    hostapd_hw_get_freq(hapd,
							iconf->npca_primary_channel),
				    iconf->npca_punct_bitmap,
#else
				    0, 0,
#endif /* CONFIG_IEEE80211BN */
				    iconf->bandwidth_device,
				    iconf->center_freq_device))
		return -1;

	freq_params->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		freq_params->link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	if (dfs_nol_ie_get_subchan_count(oper_chwidth, iface->freq,
					 freq_params->center_freq1,
					 n_subchans))
		return -1;
	if (*n_subchans <= 0 || *n_subchans > DFS_MAX_20M_SUB_CH)
		return -1;

	if (dfs_nol_ie_get_base_freq(iface->freq, freq_params->center_freq1,
				     oper_chwidth, start_freq))
		return -1;

	return 0;
}

/**
 * dfs_map_nol_bitmap_to_oper_bitmap - Map NOL bitmap to operating channel bitmap
 * @nol_start_freq: Base frequency of the NOL IE (first 20 MHz subchannel)
 * @nol_bitmap: NOL IE subchannel bitmap (bit k = nol_start_freq + k*20 MHz)
 * @oper_start_freq: Base frequency of the operating channel
 * @oper_n_subchans: Number of 20 MHz subchannels in operating channel
 * @oper_bitmap: Output operating channel bitmap
 *
 * Returns: 0 on success, -1 if any NOL frequency falls outside operating channel
 */
static int
dfs_map_nol_bitmap_to_oper_bitmap(u32 nol_start_freq, u16 nol_bitmap,
				  int oper_start_freq, int oper_n_subchans,
				  u16 *oper_bitmap)
{
	u16 bitmap = 0;
	int bit;

	if (!oper_bitmap)
		return -1;

	for (bit = 0; bit < DFS_MAX_20M_SUB_CH; bit++) {
		int oper_bit;
		u32 chan_freq;

		if (!(nol_bitmap & BIT(bit)))
			continue;

		chan_freq = nol_start_freq + bit * MIN_DFS_SUBCHAN_BW;
		if (chan_freq < (u32) oper_start_freq)
			return -1;

		oper_bit = (chan_freq - oper_start_freq) / MIN_DFS_SUBCHAN_BW;
		if (oper_bit >= oper_n_subchans ||
		    oper_start_freq + oper_bit * MIN_DFS_SUBCHAN_BW !=
		    (int) chan_freq)
			return -1;

		bitmap |= BIT(oper_bit);
	}

	if (!bitmap)
		return -1;

	*oper_bitmap = bitmap;
	return 0;
}

/**
 * dfs_notify_nol_bitmap_to_driver - Send bitmap-based NL80211_NOTIFY_RADAR
 * @hapd: BSS data
 * @nol_info: Parsed NOL IE entry (freq, bandwidth, subchan_bitmap)
 * @notify_bitmap: Bitmap of successfully marked subchannels to notify
 *
 * Derives the current operating chandef, maps the NOL bitmap into the
 * operating channel bitmap, and sends a single NL80211_CMD_NOTIFY_RADAR.
 *
 * Returns: 0 on success, -1 on failure
 */
static int dfs_notify_nol_bitmap_to_driver(struct hostapd_data *hapd,
					   const dfs_nol_ie_info *nol_info,
					   u16 notify_bitmap)
{
	struct hostapd_freq_params freq_params;
	u16 oper_bitmap;
	int oper_start_freq;
	int oper_n_subchans;

	if (!hapd || !hapd->iface || !hapd->drv_priv || !nol_info ||
	    !notify_bitmap)
		return -1;

	if (dfs_build_notify_freq_params(hapd, &freq_params, &oper_start_freq,
					 &oper_n_subchans)) {
		wpa_printf(MSG_DEBUG,
			   "DFS NOL IE: Failed to derive notify freq params");
		return -1;
	}

	if (dfs_map_nol_bitmap_to_oper_bitmap(nol_info->freq, notify_bitmap,
					      oper_start_freq,
					      oper_n_subchans, &oper_bitmap)) {
		wpa_printf(MSG_WARNING,
			   "DFS NOL IE: Failed to map parsed bitmap=0x%04x base_freq=%u to operating bitmap",
			   notify_bitmap, nol_info->freq);
		return -1;
	}

	wpa_printf(MSG_INFO,
		   "DFS NOL IE: Notifying driver with operating bitmap=0x%04x base_freq=%d bw=%d cf1=%d",
		   oper_bitmap, oper_start_freq, freq_params.bandwidth,
		   freq_params.center_freq1);

	return hostapd_drv_notify_radar(hapd, &freq_params, oper_bitmap);
}

/**
 * dfs_notify_radar_to_driver - Notify cfg80211 driver about radar detection
 *
 * Sends NL80211_CMD_NOTIFY_RADAR to the cfg80211 driver to mark a channel
 * as radar-detected and unavailable. This ensures the kernel DFS state
 * is synchronized with hostapd when processing NOL IE from uplink CSA.
 *
 * @hapd: hostapd BSS data
 * @freq: Center frequency of the radar-detected channel in MHz
 *
 * Returns: 0 on success, -1 on failure
 */
#define DFS_CHAN_WIDTH_20MHz 20
static int dfs_notify_radar_to_driver(struct hostapd_data *hapd,
				      int freq)
{
	struct hostapd_freq_params freq_params;
	u8 channel;
	int ret;

	if (!hapd || !hapd->drv_priv) {
		wpa_printf(MSG_DEBUG,
			   "DFS: Invalid context for radar notification");
		return -1;
	}

	os_memset(&freq_params, 0, sizeof(freq_params));
	freq_params.freq = freq;
	freq_params.mode = ieee80211_freq_to_chan(freq, &channel);
	if (freq_params.mode == NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_ERROR,
			   "DFS: Invalid frequency %d MHz, cannot determine channel",
			   freq);
		return -1;
	}
	freq_params.channel = channel;
	/*
	 * Notify about this 20 MHz subchannel individually.
	 * vht_enabled=1 forces nl80211_put_freq_params() to emit
	 * NL80211_ATTR_CHANNEL_WIDTH + NL80211_ATTR_CENTER_FREQ1, which
	 * nl80211_notify_radar_detection() (NL80211_CMD_NOTIFY_RADAR) requires
	 * to build a valid cfg80211_chan_def.  Using ht_enabled alone would
	 * emit NL80211_ATTR_WIPHY_CHANNEL_TYPE instead, which is rejected.
	 */
	freq_params.bandwidth = DFS_CHAN_WIDTH_20MHz;
	freq_params.vht_enabled = 1;
	freq_params.center_freq1 = freq;
	wpa_printf(MSG_INFO,
		   "DFS: Notifying driver about radar on freq=%d MHz, bw=%d MHz",
		   freq, freq_params.bandwidth);
	ret = nl80211_notify_radar_detected_extn(hapd->drv_priv, &freq_params);
	if (ret < 0) {
		wpa_printf(MSG_WARNING,
			   "DFS: Failed to notify nl80211 about radar on %d MHz",
			   freq);
		return -1;
	}

	return 0;
}

/**
 * dfs_mark_rcsa_nol_bitmap_unavailable - Mark RCSA NOL IE subchannels as DFS unavailable
 * @iface: hostapd interface whose channel table is updated
 * @nol_info: decoded NOL IE entry (freq, bandwidth, subchan_bitmap)
 * @notify_bitmap: output bitmap of successfully marked subchannels
 *
 * Iterates over each set bit in nol_info->subchan_bitmap. Bit k represents
 * the 20 MHz subchannel at (nol_info->freq + k * 20 MHz). Each subchannel
 * is individually marked DFS_UNAVAILABLE via set_dfs_state().
 *
 * Returns: 0 if all subchannels are marked successfully, -1 on any failure
 */
static int dfs_mark_rcsa_nol_bitmap_unavailable(struct hostapd_iface *iface,
					      const dfs_nol_ie_info *nol_info,
					      u16 *notify_bitmap)
{
	u32 base_freq;
	u16 bm;
	int bw_mhz;
	int bit;
	int ret = 0;
 
	if (!iface || !nol_info) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: NULL parameter in process");
		return -1;
	}
 
	bm = nol_info->subchan_bitmap;
	if (!bm) {
		wpa_printf(MSG_WARNING,
			   "DFS NOL IE: Empty bitmap for freq %u, nothing to mark",
			   nol_info->freq);
		return -1;
	}

	bw_mhz = (int)nol_info->bandwidth;

	base_freq = nol_info->freq;
	wpa_printf(MSG_INFO,
		   "DFS NOL IE: Processing entry base_freq=%u bw=%d bitmap=0x%04x",
		   base_freq, bw_mhz, bm);
 
	/*
	 * Each bit k in bm represents a 20 MHz subchannel:
	 *   freq_k = base_freq + k * MIN_DFS_SUBCHAN_BW
	 * The NOL bitmap is carried as u16, so process the full 16-bit range.
	 */
	for (bit = 0; bit < DFS_MAX_20M_SUB_CH; bit++) {
		u32 chan_freq;
 
		if (!(bm & BIT(bit)))
			continue;
		chan_freq = base_freq + (u32)(bit * MIN_DFS_SUBCHAN_BW);

		wpa_printf(MSG_DEBUG,
			   "DFS NOL IE: Marking %u MHz as NOL (base=%u bit=%d)",
			   chan_freq, base_freq, bit);

		/*
		 * This loop processes one 20 MHz subchannel at a time, so DFS
		 * state must be updated using a 20 MHz chandef even when the
		 * original operating bandwidth was wider.
		 */
		if (!set_dfs_state(iface, chan_freq, 1, 0,
				   CHAN_WIDTH_20, chan_freq, 0,
				   HOSTAPD_CHAN_DFS_UNAVAILABLE,
				   DFS_NOL_IE_SINGLE_SUBCHAN_BITMAP)) {
			wpa_printf(MSG_WARNING,
				   "DFS NOL IE: Failed to mark %u MHz as unavailable",
				   chan_freq);
			ret = -1;
		} else {
			if (notify_bitmap)
				*notify_bitmap |= BIT(bit);
			wpa_printf(MSG_INFO,
				   "DFS NOL IE: Marked %u MHz as NOL", chan_freq);
		}
	}

	return ret;
}

/**
 * dfs_process_nol_ie_bitmap_rcsa - Process NOL IE bitmap for RCSA path
 * @hapd: BSS data
 * @nol_info: Parsed NOL IE entry (freq, bandwidth, subchan_bitmap)
 *
 * Marks the parsed 20 MHz subchannels unavailable in hostapd DFS state
 * and sends a single bitmap-based NL80211_CMD_NOTIFY_RADAR to the driver.
 * Used by the RCSA path only. Uplink CSA uses dfs_process_nol_ie_bitmap().
 *
 * Returns: 0 on success, -1 on failure
 */
int dfs_process_nol_ie_bitmap_rcsa(struct hostapd_data *hapd,
				    const dfs_nol_ie_info *nol_info)
{
	u16 notify_bitmap = 0;
	int ret;

	if (!hapd || !hapd->iface || !nol_info)
		return -1;

	ret = dfs_mark_rcsa_nol_bitmap_unavailable(hapd->iface, nol_info,
						 &notify_bitmap);

	if (!notify_bitmap)
		return ret;

	if (dfs_notify_nol_bitmap_to_driver(hapd, nol_info, notify_bitmap))
		wpa_printf(MSG_WARNING,
			   "DFS NOL IE: Failed to sync parsed NOL to driver base_freq=%u bitmap=0x%04x",
			   nol_info->freq, notify_bitmap);

	return ret;
}

/**
 * dfs_process_nol_ie_bitmap - Mark channels from NOL IE as unavailable (uplink CSA)
 *
 * Iterates over each set bit in nol_info->subchan_bitmap. Each 20 MHz
 * subchannel is individually marked DFS_UNAVAILABLE and notified to the
 * driver via a per-channel NL80211_NOTIFY_RADAR call.
 * Used by uplink CSA path only. RCSA uses dfs_process_nol_ie_bitmap_rcsa().
 *
 * Returns 0 if all set_dfs_state() calls succeed, -1 if any fail.
 */
int dfs_process_nol_ie_bitmap(struct hostapd_iface *iface,
			      const dfs_nol_ie_info *nol_info)
{
	u32 base_freq;
	u16 bm;
	int bw_mhz;
	int bit;
	int ret = 0;

	if (!iface || !nol_info) {
		wpa_printf(MSG_ERROR, "DFS NOL IE: NULL parameter in process");
		return -1;
	}

	bm = nol_info->subchan_bitmap;
	if (!bm) {
		wpa_printf(MSG_WARNING,
			   "DFS NOL IE: Empty bitmap for freq %u, nothing to mark",
			   nol_info->freq);
		return -1;
	}

	bw_mhz = (int)nol_info->bandwidth;
	base_freq = nol_info->freq;
	wpa_printf(MSG_INFO,
		   "DFS NOL IE: Processing entry base_freq=%u bw=%d bitmap=0x%04x",
		   base_freq, bw_mhz, bm);

	for (bit = 0; bit < DFS_MAX_20M_SUB_CH; bit++) {
		u32 chan_freq;

		if (!(bm & BIT(bit)))
			continue;
		chan_freq = base_freq + (u32)(bit * MIN_DFS_SUBCHAN_BW);

		wpa_printf(MSG_DEBUG,
			   "DFS NOL IE: Marking %u MHz as NOL (base=%u bit=%d)",
			   chan_freq, base_freq, bit);

		if (!set_dfs_state(iface, chan_freq, 1, 0,
				   CHAN_WIDTH_20, chan_freq, 0,
				   HOSTAPD_CHAN_DFS_UNAVAILABLE,
				   DFS_NOL_IE_SINGLE_SUBCHAN_BITMAP)) {
			wpa_printf(MSG_WARNING,
				   "DFS NOL IE: Failed to mark %u MHz as unavailable",
				   chan_freq);
			ret = -1;
		} else {
			wpa_printf(MSG_INFO,
				   "DFS NOL IE: Marked %u MHz as NOL", chan_freq);

			/*
			 * Notify cfg80211 driver about radar detection.
			 * Loop through all BSSes to find a valid one for notification.
			 */
			if (iface->bss && iface->num_bss > 0) {
				size_t i;
				bool notified = false;

				for (i = 0; i < iface->num_bss; i++) {
					if (!iface->bss[i])
						continue;

					if (dfs_notify_radar_to_driver(iface->bss[i],
								       chan_freq) == 0) {
						notified = true;
						break;
					}
				}

				if (!notified) {
					wpa_printf(MSG_WARNING,
						   "DFS NOL IE: Failed to notify driver about radar on %u MHz (no valid BSS found)",
						   chan_freq);
					/*
					 * Continue processing other channels even if
					 * notification fails.
					 * The hostapd state has been updated
					 * successfully via set_dfs_state().
					 * Kernel notification failure is logged but not
					 * fatal.
					 */
				}
			}
		}
	}

	return ret;
}

int hostapd_dfs_restart_channel_extn(struct hostapd_iface *iface)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *cmode = iface->current_mode;
	struct hostapd_freq_params freq_params;
	int secondary_channel;
	int ieee80211_mode = IEEE80211_MODE_AP;
	int err;
	u8 oper_centr_freq_seg0_idx;
	u8 oper_centr_freq_seg1_idx;
	u8 current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	int channel_type = DFS_AVAILABLE_EXTN;

	wpa_printf(MSG_DEBUG,
		   "%s called (CAC active: %s, CSA active: %s)",
		   __func__, iface->cac_started ? "yes" : "no",
		   hostapd_csa_in_progress(iface) ? "yes" : "no");

	if (iface->cac_started)
		return hostapd_dfs_start_channel_switch_cac_helper(iface);

	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI)
		channel_type = DFS_ANY_CHANNEL_EXTN;

	channel = dfs_get_valid_channel_helper(iface, &secondary_channel,
					       &oper_centr_freq_seg0_idx,
					       &oper_centr_freq_seg1_idx,
					       channel_type);

	if (!channel) {
		channel_type = DFS_ANY_CHANNEL_EXTN;
		channel = dfs_downgrade_bandwidth_helper(iface, &secondary_channel,
							 &oper_centr_freq_seg0_idx,
							 &oper_centr_freq_seg1_idx,
							 &current_vht_oper_chwidth,
							 &channel_type);
		if (!channel) {
			hostapd_disable_iface(iface);
			hostapd_enable_iface(iface);
			return 0;
		}
	}

	wpa_printf(MSG_DEBUG,
		   "DFS restarting on a fresh channel %d without CSA",
		   channel->chan);

	os_memset(&freq_params, 0, sizeof(freq_params));
	iface->radar_bit_pattern = 0;
	err = hostapd_set_freq_params(&freq_params,
				      iface->conf->hw_mode,
				      channel->freq, channel->chan,
				      iface->conf->enable_edmg,
				      iface->conf->edmg_channel,
				      iface->conf->ieee80211n,
				      iface->conf->ieee80211ac,
				      iface->conf->ieee80211ax,
				      iface->conf->ieee80211be,
				      iface->conf->ieee80211bn,
				      secondary_channel,
				      current_vht_oper_chwidth,
				      oper_centr_freq_seg0_idx,
				      oper_centr_freq_seg1_idx,
				      cmode->vht_capab,
				      &cmode->he_capab[ieee80211_mode],
				      &cmode->eht_capab[ieee80211_mode],
				      &cmode->uhr_capab[ieee80211_mode],
				      iface->radar_bit_pattern,
				      iface->conf->he_6ghz_reg_pwr_type,
				      0, 0,
				      iface->conf->bandwidth_device,
				      iface->conf->center_freq_device);
	if (err) {
		wpa_printf(MSG_ERROR,
			   "DFS failed to calculate restart freq params");
	        hostapd_disable_iface(iface);
	        return err;
	}

	hostapd_switch_channel_fallback(iface, &freq_params);

	return 0;
}

/**
 * hostapd_bootup_cac_complete_extn - Finalize boot-up CAC on an interface.
 *
 * Clears the CAC-in-progress flags, transitions the interface to ENABLED
 * state, and triggers ieee802_11_set_beacons() to bring-up the interfaces
 * after CAC is completed.
 */
void hostapd_bootup_cac_complete_extn(struct hostapd_iface *iface)
{
	iface->bootup_cac_in_progress = 0;
	hostapd_set_state(iface, HAPD_IFACE_ENABLED);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_ENABLED);

#ifdef CONFIG_MESH
	if (iface->mconf)
		return;
#endif /* CONFIG_MESH */

	if (iface->conf && iface->conf->conf_extn.repeater &&
	    !iface->conf->conf_extn.ind_rptr)
		iface->mcst = 0;

	wpa_printf(MSG_DEBUG,
		   "Boot-up CAC complete for %s: state->ENABLED, calling set_beacons for %zu BSS",
		   iface->bss[0]->conf->iface, iface->num_bss);
	ieee802_11_set_beacons(iface);
}

/**
 * hostapd_bss_rnr_eligible_extn - Extension check for RNR eligibility.
 *
 * Called from hostapd_bss_rnr_eligible() after the BSS started/beacon_set_done
 * checks have already passed.  Returns true when the BSS is in the boot-up
 * CAC window. The beacon template has been sent to the driver
 * (NL80211_CMD_START_AP) but beacon_set_done may not yet be set on partner
 * ifaces that are building their RNR before the 5 GHz cross-update fires.
 * Allowing inclusion here ensures the 5 GHz link appears in partner beacons
 * from the very first beacon transmission.
 */
bool hostapd_bss_rnr_eligible_extn(struct hostapd_data *bss)
{
	if (bss->iface && bss->iface->bootup_cac_in_progress)
		return true;
	return false;
}

/**
 * dfs_range_has_nol_extn - Check whether a channel range includes NOL
 * @mode: Hardware mode containing the channel table
 * @start_idx: Start index in @mode channel table
 * @n_chans: Number of 20 MHz channels to check
 *
 * Check whether any channel in the specified range is marked
 * DFS_UNAVAILABLE. A DFS_UNAVAILABLE channel is in NOL/NOP and must not be
 * used for CAC or AP bring-up until NOP expires.
 *
 * Return: true if any channel in the range is in NOL, false otherwise.
 */
static bool dfs_range_has_nol_extn(struct hostapd_hw_modes *mode,
				   int start_idx, int n_chans)
{
	int i;

	if (!mode || start_idx < 0 || n_chans <= 0)
		return false;

	for (i = 0; i < n_chans && start_idx + i < mode->num_channels; i++) {
		struct hostapd_channel_data *channel;

		channel = &mode->channels[start_idx + i];
		if ((channel->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_UNAVAILABLE)
			return true;
	}

	return false;
}

/**
 * bootup_cac_current_channel_has_nol_extn - Check current channel for NOL
 * @iface: Pointer to hostapd interface data
 *
 * Check whether the currently configured operating channel, including all
 * active 20 MHz subchannels and the secondary segment for 80+80 MHz, contains
 * any channel marked DFS_UNAVAILABLE. A DFS_UNAVAILABLE channel is in NOL/NOP
 * and must not be used for CAC or AP bring-up until NOP expires.
 *
 * Return: true if the current channel configuration contains a DFS unavailable
 * channel, false otherwise.
 */
bool bootup_cac_current_channel_has_nol_extn(struct hostapd_iface *iface)
{
	struct hostapd_hw_modes *mode;
	int start_chan_idx, start_chan_idx1;
	int n_chans, n_chans1;
	int chan_width;

	if (!iface || !iface->conf || !iface->current_mode)
		return false;

	mode = iface->current_mode;
	chan_width = hostapd_get_oper_chwidth(iface->conf);

	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						chan_width,
						iface->conf->channel, false);
	if (start_chan_idx < 0)
		return false;

	n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);

	return dfs_range_has_nol_extn(mode, start_chan_idx, n_chans) ||
	       dfs_range_has_nol_extn(mode, start_chan_idx1, n_chans1);
}

/**
 * hostapd_bootup_cac_enabled_extn - Check whether driver-managed boot-up CAC is enabled
 *
 * Returns true when the driver advertises
 * WPA_DRIVER_FLAGS2_IFACE_CREATE_DURING_CAC and interface teardown during
 * CAC is not requested through disable_iface_during_cac configuration.
 *
 * Return: true if driver-managed boot-up CAC is enabled, false otherwise.
 */
bool hostapd_bootup_cac_enabled_extn(struct hostapd_iface *iface)
{
	return (iface->drv_flags2 & WPA_DRIVER_FLAGS2_IFACE_CREATE_DURING_CAC) &&
	       !iface->conf->conf_extn.disable_iface_during_cac;
}

/**
 * hostapd_bootup_cac_start_extn - Start boot-up CAC if the driver and config allow it.
 *
 * When the driver advertises WPA_DRIVER_FLAGS2_IFACE_CREATE_DURING_CAC and
 * disable_iface_during_cac is not set, bypass hostapd_handle_dfs so that all
 * 5 GHz BSSes are created immediately while the mac80211 CAC timer runs.
 *
 * Returns true if the boot-up CAC path was taken (caller skips hostapd_handle_dfs),
 * false if the normal DFS path should proceed.
 */
bool hostapd_bootup_cac_start_extn(struct hostapd_iface *iface)
{
	if (!iface || !iface->conf)
		return false;

	if (iface->bootup_cac_in_progress)
		return true;

	if (!is_5ghz_freq(iface->freq))
		return false;

	if (!hostapd_bootup_cac_enabled_extn(iface))
		return false;

	if (!hostapd_is_cac_required(iface))
		return true;

	if (bootup_cac_current_channel_has_nol_extn(iface)) {
		struct hostapd_channel_data *channel;
		int secondary_channel;
		u8 oper_centr_freq_seg0_idx, oper_centr_freq_seg1_idx;
		u8 oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
		int channel_type = DFS_ANY_CHANNEL_EXTN;

		wpa_printf(MSG_DEBUG,
			   "Boot-up CAC: configured channel %d MHz is in NOL, trying bandwidth downgrade",
			   iface->freq);

		channel = dfs_downgrade_bandwidth_helper(iface, &secondary_channel,
							 &oper_centr_freq_seg0_idx,
							 &oper_centr_freq_seg1_idx,
							 &oper_chwidth,
							 &channel_type);
		if (channel) {
			wpa_printf(MSG_DEBUG,
				   "Boot-up CAC: NOL fallback channel %d MHz (chan=%d width=%d)",
				   channel->freq, channel->chan, oper_chwidth);
			iface->freq = channel->freq;
			iface->conf->channel = channel->chan;
			iface->conf->secondary_channel = secondary_channel;
			hostapd_set_oper_chwidth(iface->conf, oper_chwidth);
			hostapd_set_oper_centr_freq_seg0_idx(iface->conf,
							     oper_centr_freq_seg0_idx);
			hostapd_set_oper_centr_freq_seg1_idx(iface->conf,
							     oper_centr_freq_seg1_idx);
		} else {
			wpa_printf(MSG_DEBUG,
				   "Boot-up CAC: no fallback channel found for NOL channel %d MHz",
				   iface->freq);
			return false;
		}
	}

	if (hostapd_set_dfs_cac_time(iface))
		return false;

	wpa_printf(MSG_DEBUG,
		   "Boot-up CAC: bypassing hostapd_handle_dfs for 5 GHz DFS channel %d MHz, creating all BSS immediately",
		   iface->freq);

	hostapd_set_state(iface, HAPD_IFACE_DFS);
	iface->bootup_cac_in_progress = 1;
	os_get_reltime(&iface->dfs_cac_start);

	wpa_printf(MSG_DEBUG, "DFS start CAC on %d MHz%s", iface->freq,
		   dfs_use_radar_background(iface) ? " (background)" : "");

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_START
		"freq=%d chan=%d sec_chan=%d, width=%d, seg0=%d, seg1=%d, cac_time=%ds bitmap:0x%04x",
		iface->freq,
		iface->conf->channel, iface->conf->secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		iface->dfs_cac_ms / 1000,
		iface->conf->punct_bitmap);

	return true;
}
