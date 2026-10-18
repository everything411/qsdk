/*
 * DFS - Dynamic Frequency Selection
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2013-2017, Qualcomm Atheros, Inc.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "hostapd.h"
#include "beacon.h"
#include "ap_drv_ops.h"
#include "drivers/driver.h"
#include "dfs.h"
#include "crypto/crypto.h"
#include "beacon.h"
#include "eloop.h"
#include "ieee802_11.h"
#include "ubus.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#include "../../qcn_extns/dfs_extn.h"
#endif
#include "hw_features.h"

#define IEEE80211_DFS_MIN_CAC_TIME_MS  60000

enum dfs_channel_type {
	DFS_ANY_CHANNEL,
	DFS_AVAILABLE, /* non-radar or radar-available */
	DFS_NO_CAC_YET, /* radar-not-yet-available */
};

static struct hostapd_channel_data *
dfs_downgrade_bandwidth(struct hostapd_iface *iface, int *secondary_channel,
			u8 *oper_centr_freq_seg0_idx,
			u8 *oper_centr_freq_seg1_idx,
			u8 *oper_chwidth,
			enum dfs_channel_type *channel_type);

static void hostapd_dfs_update_background_chain(struct hostapd_iface *iface);
static int hostapd_dfs_compute_bgcac_chan_params(int chan, int bw_mhz,
				    enum oper_chan_width *oper_width,
				    u8 *seg0, int *sec);
static int dfs_get_precac_channel_by_state(struct hostapd_iface *iface,
					   u32 dfs_state,
					   int *channel, int *freq,
					   int *secondary_channel,
					   u8 *centr_freq_seg0_idx,
					   u8 *centr_freq_seg1_idx,
					   u8 *current_vht_oper_chwidth);

#ifndef CONFIG_QCN_EXTN
static int dfs_get_start_chan_idx(struct hostapd_iface *iface, int *seg1_start,
				  int chan_width, int channel_no, bool is_offloaded_cac);
static int hostapd_dfs_request_channel_switch(struct hostapd_iface *iface,
					      int channel, int freq,
					      int secondary_channel,
					      u8 current_vht_oper_chwidth,
					      u8 oper_centr_freq_seg0_idx,
					      u8 oper_centr_freq_seg1_idx,
					      u16 punct_bitmap);
#endif
/*
 * dfs_is_agile_cac_enabled - Check whether Agile CAC is enabled.
 *
 * This is the single gate for the entire Agile CAC flow, which covers
 * both RCAC and PreCAC sub-paths.
 *
 * bgcac_en=1 in the hostapd config selects this entire flow.  All code
 * guarded by dfs_is_agile_cac_enabled() applies to both RCAC and PreCAC.
 */
static inline bool dfs_is_agile_cac_enabled(const struct hostapd_iface *iface)
{
	return iface && iface->conf && iface->conf->bgcac_en;
}

bool dfs_use_radar_background(struct hostapd_iface *iface)
{
	return (iface->drv_flags2 & WPA_DRIVER_FLAGS2_RADAR_BACKGROUND) &&
		iface->conf->enable_background_radar
#ifdef CONFIG_QCN_EXTN
		&& iface->iface_extn.agile_capable
#endif /* CONFIG_QCN_EXTN */
	;
}

int dfs_get_subchannel_count(int bandwidth)
{
	switch (bandwidth) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		return 1;
	case CHAN_WIDTH_40:
		return 2;
	case CHAN_WIDTH_80:
	case CHAN_WIDTH_80P80:
		return 4;
	case CHAN_WIDTH_160:
		return 8;
	case CHAN_WIDTH_320:
		return 16;
	default:
		return 0;
	}
}

/**
 * dfs_get_punc_subchan() - Determine 20 MHz subchannel for puncture source
 * @iface: Pointer to hostapd interface
 * @bit: 20 MHz subchannel index within the operating bandwidth
 * @center_freq: Center frequency defining the operating channel block (MHz)
 * @bw: Channel bandwidth in MHz
 *
 * Computes the 20 MHz subchannel corresponding to the given bit position
 * within the channel block identified by @center_freq and @bw.
 *
 * The @center_freq must be provided by the caller when operating in
 * dynamic paths (e.g., CSA or radar), so that subchannel computation
 * reflects the target channel context rather than iface->conf, which may
 * still refer to the previous operating channel.
 *
 * If @center_freq is not provided, or 0, (e.g., during initial setup), it is
 * derived from iface->conf using the appropriate operating class and
 * channel definition.
 *
 * Return: Pointer to channel data for the computed 20 MHz subchannel, or
 * %NULL if unavailable.
 */
static struct hostapd_channel_data *
dfs_get_punc_subchan(struct hostapd_iface *iface,
		     int bit, u16 center_freq, int bw)
{
	int pri_freq;

	if (!iface->current_mode)
		return NULL;

	if (!center_freq) {
		u8 seg0_idx = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
		center_freq = (seg0_idx * 5) + 5000;
		bw = channel_width_to_int(
			hostapd_get_chan_width_from_oper_chan_width(iface->conf));
	}

	pri_freq = center_freq - (bw / 2) + 10 + bit * 20;
	return hw_mode_get_channel(iface->current_mode, pri_freq, NULL);
}

/**
 * dfs_update_punc_src_chan() - Update the puncture source into hostapd
 *				channel_data structure
 * @chan: Pointer to channel data
 * @new_punct_bitmap: New puncture bitmap from puncture source
 * @bit: 20 MHz subchannel bit position
 * @source: Puncture source to be set
 *
 * If the user has already punctured the channel it will be marked as USER,
 * even if future radar appears on the channel.
 *
 * Return: None.
 */
static void dfs_update_subchan_punc_src(struct hostapd_channel_data *chan,
					u16 new_punct_bitmap, int bit,
					enum dfs_chan_puncture_source source)
{
	if (new_punct_bitmap & BIT(bit)) {
		if (chan->puncture_source != DFS_CHAN_PUNC_USER)
			chan->puncture_source = source;
		return;
	}

	chan->puncture_source = DFS_CHAN_PUNC_NONE;
}

int dfs_update_puncture_source(struct hostapd_iface *iface,
			       u16 center_freq, int bandwidth,
			       u16 new_punct_bitmap,
			       enum dfs_chan_puncture_source source)
{
	struct hostapd_channel_data *chan;
	int bit;
	int subchannel_count;
	int bw;

	subchannel_count = dfs_get_subchannel_count(bandwidth);
	if (!subchannel_count) {
		wpa_printf(MSG_ERROR,
			   "DFS: puncture source update failed (bw=%d)",
			   bandwidth);
		return -1;
	}

	bw = channel_width_to_int(bandwidth);
	for (bit = 0; bit < subchannel_count; bit++) {
		chan = dfs_get_punc_subchan(iface, bit, center_freq, bw);
		if (!chan)
			continue;

		dfs_update_subchan_punc_src(chan, new_punct_bitmap,
					    bit, source);
	}

	return 0;
}

/**
 * dfs_is_puncture_bitmap_bit_source() - Check puncture source for one bit
 * @iface: Pointer to hostapd interface
 * @bit: 20 MHz subchannel bit position
 * @source: Puncture source to match
 *
 * Return: 1 if the bit maps to a channel with the requested puncture source,
 * 0 otherwise.
 */
static int
dfs_is_puncture_bitmap_bit_source(struct hostapd_iface *iface, int bit,
				  enum dfs_chan_puncture_source source)
{
	struct hostapd_channel_data *chan;

	chan = dfs_get_punc_subchan(iface, bit, 0, 0);
	if (!chan)
		return 0;

	return chan->puncture_source == source;
}

int dfs_is_puncture_bitmap_bit_user(struct hostapd_iface *iface, int bit)
{
	return dfs_is_puncture_bitmap_bit_source(iface, bit,
						 DFS_CHAN_PUNC_USER);
}

int dfs_is_puncture_bitmap_bit_radar(struct hostapd_iface *iface, int bit)
{
	return dfs_is_puncture_bitmap_bit_source(iface, bit,
						 DFS_CHAN_PUNC_RADAR);
}

u16 dfs_filter_punc_bitmap_by_src(struct hostapd_iface *iface,
				  u16 punct_bitmap,
				  enum dfs_chan_puncture_source source)
{
	u16 filtered_punct_bitmap = 0;
	int bit;

	for (bit = 0; bit < sizeof(punct_bitmap) * 8; bit++) {
		if (!(punct_bitmap & BIT(bit)))
			continue;

		if (dfs_is_puncture_bitmap_bit_source(iface, bit, source))
			filtered_punct_bitmap |= BIT(bit);
	}

	return filtered_punct_bitmap;
}

/**
 * dfs_get_dfs_punctured_bitmap() - Get radar-punctured subchannels still in radar state
 * @iface: Pointer to hostapd interface
 * @unpuncture_bitmap: Bitmap containing subchannels to be unpunctured
 *
 * Identifies which subchannels are still in radar-punctured state, excluding the
 * subchannels that are being unpunctured (specified in @unpuncture_bitmap).
 * This function filters out the channels marked for unpuncturing from the full
 * radar-punctured set to return only the subchannels that must remain punctured
 * because they are radar-affected.
 *
 * Return: Bitmap of radar-punctured subchannels that must remain punctured
 *         (excludes subchannels specified in @unpuncture_bitmap)
 */
static u16 dfs_get_dfs_punctured_bitmap(struct hostapd_iface *iface,
					u16 unpuncture_bitmap)
{
	return dfs_filter_punc_bitmap_by_src(iface, unpuncture_bitmap,
					     DFS_CHAN_PUNC_RADAR);
}

void dfs_reset_punc_bitmap_src(struct hostapd_iface *iface,
			       u16 punct_bitmap)
{
	struct hostapd_channel_data *chan;
	int bit;

	for (bit = 0; bit < sizeof(punct_bitmap) * 8; bit++) {
		if (!(punct_bitmap & BIT(bit)))
			continue;

		chan = dfs_get_punc_subchan(iface, bit, 0, 0);
		if (!chan)
			continue;

		chan->puncture_source = DFS_CHAN_PUNC_NONE;
	}
}

#ifndef CONFIG_QCN_EXTN
static
#endif
int dfs_get_used_n_chans(struct hostapd_iface *iface, int *seg1,
				int chan_width)
{
	int n_chans = 1;

	*seg1 = 0;

	if (iface->conf->ieee80211n && iface->conf->secondary_channel)
		n_chans = 2;

	if (iface->conf->ieee80211ac || iface->conf->ieee80211ax ||
	    iface->conf->ieee80211be) {
		switch (chan_width) {
		case CONF_OPER_CHWIDTH_USE_HT:
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			n_chans = 4;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
			n_chans = 8;
			break;
		case CONF_OPER_CHWIDTH_80P80MHZ:
			n_chans = 4;
			*seg1 = 4;
			break;
		default:
#ifdef CONFIG_QCN_EXTN
			hostapd_get_n_chans_and_frequency_extn(chan_width, 0,
							       &n_chans, NULL);
#endif /* CONFIG_QCN_EXTN */

			break;
		}
	}

	return n_chans;
}


/* dfs_channel_available: select new channel according to type parameter */
static int dfs_channel_available(struct hostapd_channel_data *chan,
				 enum dfs_channel_type type)
{
	if (type == DFS_NO_CAC_YET) {
		/* Select only radar channel where CAC has not been
		 * performed yet
		 */
		if ((chan->flag & HOSTAPD_CHAN_RADAR) &&
		    (chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
		     HOSTAPD_CHAN_DFS_USABLE)
			return 1;
		return 0;
	}

	/*
	 * When radar detection happens, CSA is performed. However, there's no
	 * time for CAC, so radar channels must be skipped when finding a new
	 * channel for CSA, unless they are available for immediate use.
	 */
	if (type == DFS_AVAILABLE && (chan->flag & HOSTAPD_CHAN_RADAR) &&
	    ((chan->flag & HOSTAPD_CHAN_DFS_MASK) !=
	     HOSTAPD_CHAN_DFS_AVAILABLE))
		return 0;

	if (chan->flag & HOSTAPD_CHAN_DISABLED)
		return 0;
	if ((chan->flag & HOSTAPD_CHAN_RADAR) &&
	    ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
	     HOSTAPD_CHAN_DFS_UNAVAILABLE))
		return 0;
	return 1;
}


static int dfs_is_chan_allowed(struct hostapd_channel_data *chan, int n_chans)
{
	/*
	 * The tables contain first valid channel number based on channel width.
	 * We will also choose this first channel as the control one.
	 */
	int allowed_40[] = { 36, 44, 52, 60, 100, 108, 116, 124, 132, 149, 157,
			     165, 173, 184, 192 };
	/*
	 * VHT80, valid channels based on center frequency:
	 * 42, 58, 106, 122, 138, 155, 171
	 */
	int allowed_80[] = { 36, 52, 100, 116, 132, 149, 165 };
	/*
	 * VHT160 valid channels based on center frequency:
	 * 50, 114, 163
	 */
	int allowed_160[] = { 36, 100, 149 };
	int *allowed = allowed_40;
	unsigned int i, allowed_no = 0;

	switch (n_chans) {
	case 2:
		allowed = allowed_40;
		allowed_no = ARRAY_SIZE(allowed_40);
		break;
	case 4:
		allowed = allowed_80;
		allowed_no = ARRAY_SIZE(allowed_80);
		break;
	case 8:
		allowed = allowed_160;
		allowed_no = ARRAY_SIZE(allowed_160);
		break;
	default:
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_dfs_get_allowed_channels_extn(n_chans,
							   allowed,
							   &allowed_no))
			break;
#endif /* CONFIG_QCN_EXTN */

		wpa_printf(MSG_DEBUG, "Unknown width for %d channels", n_chans);
		break;
	}

	for (i = 0; i < allowed_no; i++) {
		if (chan->chan == allowed[i])
			return 1;
	}

	return 0;
}

#ifdef RDK_ONEWIFI
struct hostapd_channel_data *
#else
static struct hostapd_channel_data *
#endif
dfs_get_chan_data(struct hostapd_hw_modes *mode, int freq, int first_chan_idx)
{
	int i;

	for (i = first_chan_idx; i < mode->num_channels; i++) {
		if (mode->channels[i].freq == freq)
			return &mode->channels[i];
	}

	return NULL;
}


static int dfs_chan_range_available(struct hostapd_hw_modes *mode,
				    int first_chan_idx, int num_chans,
				    enum dfs_channel_type type)
{
	struct hostapd_channel_data *first_chan, *chan;
	int i;
	u32 bw = num_chan_to_bw(num_chans);

	if (first_chan_idx + num_chans > mode->num_channels) {
		wpa_printf(MSG_DEBUG,
			   "DFS: some channels in range not defined");
		return 0;
	}

	first_chan = &mode->channels[first_chan_idx];

	/* hostapd DFS implementation assumes the first channel as primary.
	 * If it's not allowed to use the first channel as primary, decline the
	 * whole channel range. */
	if (!chan_pri_allowed(first_chan)) {
		wpa_printf(MSG_DEBUG, "DFS: primary channel not allowed");
		return 0;
	}

	for (i = 0; i < num_chans; i++) {
		chan = dfs_get_chan_data(mode, first_chan->freq + i * 20,
					 first_chan_idx);
		if (!chan) {
			wpa_printf(MSG_DEBUG, "DFS: no channel data for %d",
				   first_chan->freq + i * 20);
			return 0;
		}

		/* HT 40 MHz secondary channel availability checked only for
		 * primary channel */
		if (!chan_bw_allowed(chan, bw, 1, !i)) {
			wpa_printf(MSG_DEBUG, "DFS: bw now allowed for %d",
				   first_chan->freq + i * 20);
			return 0;
		}

		if (!dfs_channel_available(chan, type)) {
			wpa_printf(MSG_DEBUG, "DFS: channel not available %d",
				   first_chan->freq + i * 20);
			return 0;
		}
	}

	return 1;
}


static int is_in_chanlist(struct hostapd_iface *iface,
			  struct hostapd_channel_data *chan)
{
	if (!iface->conf->acs_ch_list.num)
		return 1;

	return freq_range_list_includes(&iface->conf->acs_ch_list, chan->chan);
}

#define HOSTAPD_DFS_UNII2_CHANNELS(chan) (chan >= 52 && chan <= 64)

static bool is_skip_unii1_dfs_switch_applicable(struct hostapd_iface *iface,
						struct hostapd_channel_data *chan)
{
	if (!iface->conf->skip_unii1_dfs_switch)
		return false;

	/* skip if the selected channel is 52/56/60/64 */
	if (HOSTAPD_DFS_UNII2_CHANNELS(iface->conf->channel) &&
	    HOSTAPD_DFS_UNII2_CHANNELS(chan->chan))
		return true;

	/* Skip below mentioned adjacent channels if one of the following
	 * conditions is true
	 * i) The primary channel of the AP is 52/56/60/64 in 80MHz mode and
	 * moves to adjacent channel 36/44/48 in 80MHz
	 * ii) The primary channel of the AP is 36/44/48/52/56/60/64 in 160MHz
	 * and moves to 36/44/48 in 80MHz mode
	 * iii) The primary channel of the AP is 52/56/60/64 in 20MHz or 40MHz mode
	 * and moves to the adjacent channels 40/44/48 in 20MHz mode or 36/40/44/48
	 * in 40MHz mode
	 */
	switch(hostapd_get_oper_chwidth(iface->conf))
	{
		case CONF_OPER_CHWIDTH_160MHZ:
			if ((iface->conf->channel >= 36) &&
			    (iface->conf->channel <= 64) &&
			    (iface->conf->channel != 40) &&
			    (chan->chan >= 36) && (chan->chan <= 48) &&
			    (chan->chan != 40))
				return true;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			if ((iface->conf->channel >= 52) &&
			    (iface->conf->channel <= 64) &&
			    (chan->chan >= 36) && (chan->chan <= 48) &&
			    (chan->chan != 40))
				return true;
			break;
		case CHANWIDTH_USE_HT:
			if ((iface->conf->channel >= 52) &&
			    (iface->conf->channel <= 64)) {
				if (iface->conf->secondary_channel) {
					if ((chan->chan >= 36) && (chan->chan <= 48))
						return true;
				} else {
					if ((chan->chan >= 40) && (chan->chan <= 48))
						return true;
				}
			}
			break;
		default:
			break;
	}

	return false;
}

/*
 * dfs_agile_cac_get_rcac_channel - Get RCAC channel parameters of the interface
 * @iface: Pointer to the hostapd interface
 * @channel: Output - primary channel number of the RCAC channel
 * @freq: Output - primary frequency (MHz) of the RCAC channel
 * @secondary_channel: Output - secondary channel offset (+1/-1/0)
 * @centr_freq_seg0_idx: Output - VHT/HE center frequency segment 0 index
 * @centr_freq_seg1_idx: Output - VHT/HE center frequency segment 1 index
 * @chwidth: Output - operating channel width (enum oper_chan_width)
 *
 * Fills in all channel parameters from iface->radar_background, which is
 * populated by hostapd_start_rcac_on_channel() /
 * hostapd_dfs_update_background_chain().
 *
 * Returns 0 on success, -1 if RCAC is not ready (not enabled, still running,
 * or no channel configured).
 */
static int dfs_agile_cac_get_rcac_channel(struct hostapd_iface *iface,
					  int *channel, int *freq,
					  int *secondary_channel,
					  u8 *centr_freq_seg0_idx,
					  u8 *centr_freq_seg1_idx,
					  u8 *chwidth)
{
	struct hostapd_channel_data *chan = hw_mode_get_channel(iface->current_mode,
								iface->radar_background.freq,
								NULL);

	if (!dfs_use_radar_background(iface)) {
		wpa_printf(MSG_DEBUG, "DFS: Failed to select an RCAC channel - background radar not enabled");
		return -1;
	}

	if (iface->radar_background.channel <= 0 || iface->radar_background.freq <= 0) {
		wpa_printf(MSG_DEBUG, "DFS: Failed to select an RCAC channel -  no RCAC channel configured");
		return -1;
	}

	if (!chan || ((chan->flag & HOSTAPD_CHAN_RADAR) &&
		(chan->flag & HOSTAPD_CHAN_DFS_MASK) != HOSTAPD_CHAN_DFS_AVAILABLE)) {
		wpa_printf(MSG_WARNING,
			   "DFS: RCAC not yet complete (background CAC still in progress) on chan %d",
			   iface->radar_background.channel);
		return -1;
	}
	wpa_printf(MSG_DEBUG,
		   "DFS: RCAC chan %d is %s - allowing seamless switch",
		   iface->radar_background.channel,
		   (chan->flag & HOSTAPD_CHAN_RADAR) ?
		   "DFS_AVAILABLE" : "non-DFS");

	*channel = iface->radar_background.channel;
	*freq = iface->radar_background.freq;
	*secondary_channel = iface->radar_background.secondary_channel;
	*centr_freq_seg0_idx = iface->radar_background.centr_freq_seg0_idx;
	*centr_freq_seg1_idx = iface->radar_background.centr_freq_seg1_idx;
	*chwidth = (iface->radar_background.channel > 0) ?
		iface->radar_background.chwidth :
		hostapd_get_oper_chwidth(iface->conf);

	return 0;
}


/*
 * hostapd_dfs_agile_cac_switch - Switch home channel to a pre-cleared DFS channel
 *
 *   dfs_agile_cac_get_rcac_channel
 *
 * This function is called either by user request (via hostapd_cli switch_to_rcac)
 * or automatically when radar is detected on the home channel.
 *
 * Returns 0 on success, -1 on failure.
 */
int hostapd_dfs_agile_cac_switch(struct hostapd_iface *iface)
{
	int freq, channel, secondary_channel;
	u8 centr_freq_seg0_idx;
	u8 centr_freq_seg1_idx;
	u8 current_vht_oper_chwidth;

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: Changing the home channel to agile-CACed channel skipped - CSA already in progress");
		return 0;
	}

	if (iface->cac_started) {
		wpa_printf(MSG_WARNING,
			   "DFS: Changing the home channel to agile-CACed channel rejected - home channel CAC in progress");
		return -1;
	}

	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI &&
	    iface->conf->bgcac_en) {
		if (dfs_get_precac_channel_by_state(iface, HOSTAPD_CHAN_DFS_AVAILABLE,
						    &channel, &freq,
						    &secondary_channel,
						    &centr_freq_seg0_idx,
						    &centr_freq_seg1_idx,
						    &current_vht_oper_chwidth) < 0) {
			wpa_printf(MSG_INFO,
				   "DFS: PreCAC has no available channel for fast switch");
			return -1;
		}
		wpa_printf(MSG_INFO,
			   "DFS: PreCAC switch to chan %d (%d MHz), seg0=%d, sec=%d",
			   channel, freq, centr_freq_seg0_idx, secondary_channel);
	} else {
		if (dfs_agile_cac_get_rcac_channel(iface, &channel, &freq,
						   &secondary_channel,
						   &centr_freq_seg0_idx,
						   &centr_freq_seg1_idx,
						   &current_vht_oper_chwidth) < 0) {
			wpa_printf(MSG_INFO,
				   "DFS: RCAC has no available channel for fast switch");
			return -1;
		}

		if (current_vht_oper_chwidth != hostapd_get_oper_chwidth(iface->conf)) {
			wpa_printf(MSG_INFO,
				   "DFS: RCAC switch with BW change: RCAC BW (%d) -> home BW (%d) (half-BW RCAC)",
				   current_vht_oper_chwidth,
				   hostapd_get_oper_chwidth(iface->conf));
		}

		wpa_printf(MSG_INFO,
			   "DFS: RCAC switch to chan %d (%d MHz), seg0=%d, sec=%d",
			   channel, freq, centr_freq_seg0_idx, secondary_channel);
	}

	iface->conf->channel = channel;
	iface->freq = freq;
	iface->conf->secondary_channel = secondary_channel;
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf, centr_freq_seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf, centr_freq_seg1_idx);
	hostapd_set_oper_chwidth(iface->conf, current_vht_oper_chwidth);

	return hostapd_dfs_request_channel_switch(iface, channel, freq,
						  secondary_channel,
						  current_vht_oper_chwidth,
						  centr_freq_seg0_idx,
						  centr_freq_seg1_idx, 0);
}

/*
 * The function assumes HT40+ operation.
 * Make sure to adjust the following variables after calling this:
 *  - hapd->secondary_channel
 *  - hapd->vht/he_oper_centr_freq_seg0_idx
 *  - hapd->vht/he_oper_centr_freq_seg1_idx
 */
static int dfs_find_channel(struct hostapd_iface *iface,
			    struct hostapd_channel_data **ret_chan,
			    int idx, enum dfs_channel_type type,
			    unsigned int flags)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int i, channel_idx = 0, n_chans, n_chans1;

	mode = iface->current_mode;
	n_chans = dfs_get_used_n_chans(iface, &n_chans1,
				       hostapd_get_oper_chwidth(iface->conf));

	wpa_printf(MSG_DEBUG, "DFS new chan checking %d channels", n_chans);
	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

#ifdef CONFIG_QCN_EXTN
		if (!chan_pri_allowed_extn(chan)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: Skipping channel %d (%d) not in primary chan list",
				   chan->freq, chan->chan);
			continue;
		}
#endif

		if (!chan_in_current_hw_info(iface->current_hw_info, chan)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: channel %d (%d) is not under current hardware index",
				   chan->freq, chan->chan);
			continue;
		}

		/* Skip HT40/VHT incompatible channels */
		if (iface->conf->ieee80211n &&
		    iface->conf->secondary_channel &&
		    (!dfs_is_chan_allowed(chan, n_chans) ||
		     !(chan->allowed_bw & HOSTAPD_CHAN_WIDTH_40P))) {
			wpa_printf(MSG_DEBUG,
				   "DFS: channel %d (%d) is incompatible",
				   chan->freq, chan->chan);
			continue;
		}

		/* Skip incompatible chandefs */
		if (!dfs_chan_range_available(mode, i, n_chans, type)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: range not available for %d (%d)",
				   chan->freq, chan->chan);
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		if (hostapd_dfs_skip_wradar_chan_extn(iface, mode, chan, i,
						      n_chans))
			continue;
#endif /* CONFIG_QCN_EXTN */

		if (!is_in_chanlist(iface, chan)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: channel %d (%d) not in chanlist",
				   chan->freq, chan->chan);
			continue;
		}

		if (is_skip_unii1_dfs_switch_applicable(iface, chan)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: skip_unii1_dfs_switch enabled, skip adjacent channel: %d (%d)",
				   chan->freq, chan->chan);
			continue;
		}

#ifdef CONFIG_QCN_EXTN
		if (dfs_chan_skip_by_flags_extn(iface, chan, flags))
			continue;
#endif

		if (chan->max_tx_power < iface->conf->min_tx_power)
			continue;

		if ((chan->flag & HOSTAPD_CHAN_INDOOR_ONLY) &&
		    iface->conf->country[2] == 0x4f)
			continue;

		if (ret_chan && idx == channel_idx) {
			wpa_printf(MSG_DEBUG, "Selected channel %d (%d)",
				   chan->freq, chan->chan);
			*ret_chan = chan;
			return idx;
		}
		wpa_printf(MSG_DEBUG, "Adding channel %d (%d)",
			   chan->freq, chan->chan);
		channel_idx++;
	}
	return channel_idx;
}


static void dfs_adjust_center_freq(struct hostapd_iface *iface,
				   struct hostapd_channel_data *chan,
				   int secondary_channel,
				   int sec_chan_idx_80p80,
				   u8 *oper_centr_freq_seg0_idx,
				   u8 *oper_centr_freq_seg1_idx)
{
	if (!iface->conf->ieee80211ac && !iface->conf->ieee80211ax &&
	    !iface->conf->ieee80211be)
		return;

	if (!chan)
		return;

	*oper_centr_freq_seg1_idx = 0;

	switch (hostapd_get_oper_chwidth(iface->conf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (secondary_channel == 1)
			*oper_centr_freq_seg0_idx = chan->chan + 2;
		else if (secondary_channel == -1)
			*oper_centr_freq_seg0_idx = chan->chan - 2;
		else
			*oper_centr_freq_seg0_idx = chan->chan;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		*oper_centr_freq_seg0_idx = chan->chan + 6;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		*oper_centr_freq_seg0_idx = chan->chan + 14;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
		*oper_centr_freq_seg0_idx = chan->chan + 6;
		*oper_centr_freq_seg1_idx = sec_chan_idx_80p80 + 6;
		break;

	default:
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_dfs_adjust_center_freq_extn(
			hostapd_get_oper_chwidth(iface->conf), chan->chan,
			oper_centr_freq_seg0_idx, oper_centr_freq_seg1_idx))
			break;
#endif /* CONFIG_QCN_EXTN */

		wpa_printf(MSG_INFO,
			   "DFS: Unsupported channel width configuration");
		*oper_centr_freq_seg0_idx = 0;
		break;
	}

	wpa_printf(MSG_DEBUG, "DFS adjusting VHT center frequency: %d, %d",
		   *oper_centr_freq_seg0_idx,
		   *oper_centr_freq_seg1_idx);
}

static int dfs_is_home_chan(struct hostapd_iface *iface,
			    struct hostapd_channel_data *chan,
			    enum oper_chan_width home_chwidth)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	int seg1_start = -1;
	int home_chan = iface->conf->channel;
	int home_start_idx, n_home_chans, k;

	if (!mode)
		return 0;

	/*
	 * dfs_precac_try_half_bw() temporarily reduces oper_chwidth before
	 * calling us, so iface->conf->oper_chwidth cannot be trusted here.
	 * The caller passes the true home bandwidth as home_chwidth.
	 *
	 * ETSI EN 301 893 §4.7.3.6: background CAC shall not use any
	 * sub-channel of the current home operating block.
	 */
	home_start_idx = dfs_get_start_chan_idx(iface, &seg1_start,
						home_chwidth,
						home_chan, false);
	if (home_start_idx < 0)
		return 0;

	n_home_chans = dfs_get_used_n_chans(iface, &seg1_start, home_chwidth);

	for (k = 0; k < n_home_chans; k++) {
		if (chan->chan == mode->channels[home_start_idx + k].chan)
			return 1;
	}

	return 0;
}

/**
 * dfs_get_precac_channel_by_state - Get first PreCAC channel with specified DFS state
 * Validates if a block is available in hardware.
 *  use cases:
 *   - HOSTAPD_CHAN_DFS_AVAILABLE: Find pre-cleared channel for fast switch after radar
 *   - HOSTAPD_CHAN_DFS_USABLE: Find next channel to start PreCAC on
 *
 * Returns: 0 on success (channel found), -1 on failure (no matching channel)
 */
static int dfs_get_precac_channel_by_state(struct hostapd_iface *iface,
					   u32 dfs_state,
					   int *channel, int *freq,
					   int *secondary_channel,
					   u8 *centr_freq_seg0_idx,
					   u8 *centr_freq_seg1_idx,
					   u8 *current_vht_oper_chwidth)
{
	struct hostapd_channel_data *chan = NULL;
	int i, n_chans, n_chans1;
	struct hostapd_hw_modes *mode = iface->current_mode;
	const char *state_str;

	if (!mode)
		return -1;

	n_chans = dfs_get_used_n_chans(iface, &n_chans1,
				       hostapd_get_oper_chwidth(iface->conf));

	/* Convert state flag to string for logging */
	if (dfs_state == HOSTAPD_CHAN_DFS_AVAILABLE)
		state_str = "AVAILABLE";
	else if (dfs_state == HOSTAPD_CHAN_DFS_USABLE)
		state_str = "USABLE";
	else
		state_str = "UNKNOWN";

	wpa_printf(MSG_INFO,
		   "PRECAC_Searching for first DFS_%s channel in current hardware",
		   state_str);

	/* Search for first channel matching the specified state */
	i = 0;
	while (i < mode->num_channels) {
		chan = &mode->channels[i];

		/* Skip non-DFS channels */
		if (!(chan->flag & HOSTAPD_CHAN_RADAR)) {
			i++;
			continue;
		}

		/* Check if channel matches desired state */
		if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) != dfs_state) {
			i++;
			continue;
		}

		/* Skip all sub-channels of the home BW block */
		if (dfs_is_home_chan(iface, chan, hostapd_get_oper_chwidth(iface->conf))) {
			i++;
			continue;
		}

		/* Skip if not in current hardware info */
		if (!hostapd_is_freq_in_current_hw_info(iface, chan->freq)) {
			wpa_printf(MSG_DEBUG,
				   "PRECAC_Skipping channel %d - not in current hardware",
				   chan->chan);
			i++;
			continue;
		}

		/* Validate full BW block starting at index i */
		if (!dfs_chan_range_available(mode, i, n_chans, DFS_AVAILABLE)) {
			wpa_printf(MSG_DEBUG,
				   "PRECAC_Skipping channel %d: BW block not fully available",
				   chan->chan);
			i += n_chans;
			continue;
		}

		/* Found a valid channel with desired state */
		*channel = chan->chan;
		*freq = chan->freq;
		*secondary_channel = iface->conf->secondary_channel;
		*current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

		/* Calculate center frequencies based on configured channel width */
		dfs_adjust_center_freq(iface, chan, *secondary_channel, 0,
				       centr_freq_seg0_idx, centr_freq_seg1_idx);

		wpa_printf(MSG_INFO,
			   "PRECAC_Found DFS_%s channel: %d (freq=%d MHz) seg0=%d",
			   state_str, *channel, *freq, *centr_freq_seg0_idx);
		return 0;
	}

	wpa_printf(MSG_INFO,
		   "PRECAC_No DFS_%s channel found in current hardware",
		   state_str);
	return -1;
}


/* Return start channel idx we will use for mode->channels[idx] */
#ifndef CONFIG_QCN_EXTN
static
#endif
int dfs_get_start_chan_idx(struct hostapd_iface *iface, int *seg1_start,
				  int chan_width, int channel_no, bool is_offloaded_cac)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int res = -1, i;
	int chan_seg1 = -1;

	*seg1_start = -1;

	/* HT40- */
	if (iface->conf->ieee80211n && iface->conf->secondary_channel == -1)
		channel_no -= 4;

	/* VHT/HE/EHT */
	if ((iface->conf->ieee80211ac || iface->conf->ieee80211ax ||
	    iface->conf->ieee80211be) && !is_offloaded_cac) {
		switch (chan_width) {
		case CONF_OPER_CHWIDTH_USE_HT:
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 6;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 14;
			break;
		case CONF_OPER_CHWIDTH_80P80MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 6;
			chan_seg1 = hostapd_get_oper_centr_freq_seg1_idx(
				iface->conf) - 6;
			break;
		case CONF_OPER_CHWIDTH_320MHZ:
			channel_no = hostapd_get_oper_centr_freq_seg0_idx(
				iface->conf) - 30;
			break;
		default:
			wpa_printf(MSG_INFO,
				   "DFS only EHT20/40/80/160/80+80/320 is supported now");
			channel_no = -1;
			break;
		}
	}

	/* Get idx */
	mode = iface->current_mode;
	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];
		if (chan->chan == channel_no) {
			res = i;
			break;
		}
	}

	if (res != -1 && chan_seg1 > -1) {
		int found = 0;

		/* Get idx for seg1 */
		mode = iface->current_mode;
		for (i = 0; i < mode->num_channels; i++) {
			chan = &mode->channels[i];
			if (chan->chan == chan_seg1) {
				*seg1_start = i;
				found = 1;
				break;
			}
		}
		if (!found)
			res = -1;
	}

	if (res == -1) {
		wpa_printf(MSG_DEBUG,
			   "DFS chan_idx seems wrong; num-ch: %d ch-no: %d conf-ch-no: %d 11n: %d sec-ch: %d vht-oper-width: %d",
			   mode->num_channels, channel_no, iface->conf->channel,
			   iface->conf->ieee80211n,
			   iface->conf->secondary_channel,
			   hostapd_get_oper_chwidth(iface->conf));

		for (i = 0; i < mode->num_channels; i++) {
			wpa_printf(MSG_DEBUG, "Available channel: %d",
				   mode->channels[i].chan);
		}
	}

	return res;
}


/* At least one channel have radar flag */
#ifndef CONFIG_QCN_EXTN
static
#endif
int dfs_check_chans_radar(struct hostapd_iface *iface,
			 int start_chan_idx, int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i, res = 0;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		if (start_chan_idx + i >= mode->num_channels)
			break;
		channel = &mode->channels[start_chan_idx + i];
		if (channel->flag & HOSTAPD_CHAN_RADAR)
			res++;
	}

	return res;
}


/* All channels available */
#ifndef CONFIG_QCN_EXTN
static
#endif
int dfs_check_chans_available(struct hostapd_iface *iface,
			     int start_chan_idx, int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		channel = &mode->channels[start_chan_idx + i];

		if (channel->flag & HOSTAPD_CHAN_DISABLED)
			break;

		if (!(channel->flag & HOSTAPD_CHAN_RADAR))
			continue;

		/* Punctured sub-channels are excluded from the operating block;
		 * their DFS state does not block the remaining channels from
		 * being considered available.
		 */
		if (channel->puncture_source != DFS_CHAN_PUNC_NONE)
			continue;

		if ((channel->flag & HOSTAPD_CHAN_DFS_MASK) !=
		    HOSTAPD_CHAN_DFS_AVAILABLE)
			break;
	}

	return i == n_chans;
}


/* At least one channel unavailable */
static int dfs_check_chans_unavailable(struct hostapd_iface *iface,
				       int start_chan_idx,
				       int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i, res = 0;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		channel = &mode->channels[start_chan_idx + i];
		if (channel->flag & HOSTAPD_CHAN_DISABLED)
			res++;
		if ((channel->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_UNAVAILABLE)
			res++;
	}

	return res;
}

#ifdef RDK_ONEWIFI
struct hostapd_channel_data *
#else
static struct hostapd_channel_data *
#endif
dfs_get_valid_channel(struct hostapd_iface *iface,
		      int *secondary_channel,
		      u8 *oper_centr_freq_seg0_idx,
		      u8 *oper_centr_freq_seg1_idx,
		      enum dfs_channel_type type)
{
	unsigned int flags = DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan = NULL;
	struct hostapd_channel_data *chan2 = NULL;
	int num_available_chandefs;
	int chan_idx, chan_idx2;
	int sec_chan_idx_80p80 = -1;
	bool is_mesh = false;
	int i;
	u32 _rand;

#ifdef CONFIG_QCN_EXTN
	flags = dfs_get_ch_flags_extn(iface->conf->conf_extn.cswopts);
#endif

#ifdef CONFIG_MESH
	is_mesh = iface->mconf;
#endif

	wpa_printf(MSG_DEBUG, "DFS: Selecting random channel");
	*secondary_channel = 0;
	*oper_centr_freq_seg0_idx = 0;
	*oper_centr_freq_seg1_idx = 0;

	if (iface->current_mode == NULL)
		return NULL;

	mode = iface->current_mode;
	if (mode->mode != HOSTAPD_MODE_IEEE80211A)
		return NULL;

	/* Get the count first */
	num_available_chandefs = dfs_find_channel(iface, NULL, 0, type, flags);
	wpa_printf(MSG_DEBUG, "DFS: num_available_chandefs=%d",
		   num_available_chandefs);
	if (num_available_chandefs == 0)
		return NULL;

	/* try to use deterministic channel in mesh, so that both sides
	 * have a chance to switch to the same channel */
	if (is_mesh) {
#ifdef CONFIG_MESH
		u64 hash[4];
		const u8 *meshid[1] = { &iface->mconf->meshid[0] };
		const size_t meshid_len = iface->mconf->meshid_len;

		sha256_vector(1, meshid, &meshid_len, (u8 *)&hash[0]);
		_rand = hash[0] + hash[1] + hash[2] + hash[3];
#endif
	} else if (os_get_random((u8 *) &_rand, sizeof(_rand)) < 0)
		return NULL;

	chan_idx = _rand % num_available_chandefs;
	wpa_printf(MSG_DEBUG, "DFS: Picked random entry from the list: %d/%d",
		   chan_idx, num_available_chandefs);
	dfs_find_channel(iface, &chan, chan_idx, type, flags);
	if (!chan) {
		wpa_printf(MSG_DEBUG, "DFS: no random channel found");
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "DFS: got random channel %d (%d)",
		   chan->freq, chan->chan);

	/* dfs_find_channel() calculations assume HT40+ */
	if (iface->conf->secondary_channel)
		*secondary_channel = 1;
	else
		*secondary_channel = 0;

	/* Get secondary channel for HT80P80 */
	if (hostapd_get_oper_chwidth(iface->conf) ==
	    CONF_OPER_CHWIDTH_80P80MHZ) {
		if (num_available_chandefs <= 1) {
			wpa_printf(MSG_ERROR,
				   "only 1 valid chan, can't support 80+80");
			return NULL;
		}

		/*
		 * Loop all channels except channel1 to find a valid channel2
		 * that is not adjacent to channel1.
		 */
		for (i = 0; i < num_available_chandefs - 1; i++) {
			/* start from chan_idx + 1, end when chan_idx - 1 */
			chan_idx2 = (chan_idx + 1 + i) % num_available_chandefs;
			dfs_find_channel(iface, &chan2, chan_idx2, type, flags);
			if (chan2 && abs(chan2->chan - chan->chan) > 12) {
				/* two channels are not adjacent */
				sec_chan_idx_80p80 = chan2->chan;
				wpa_printf(MSG_DEBUG,
					   "DFS: got second chan: %d (%d)",
					   chan2->freq, chan2->chan);
				break;
			}
		}

		/* Check if we got a valid secondary channel which is not
		 * adjacent to the first channel.
		 */
		if (sec_chan_idx_80p80 == -1) {
			wpa_printf(MSG_INFO,
				   "DFS: failed to get chan2 for 80+80");
			return NULL;
		}
	}

	dfs_adjust_center_freq(iface, chan,
			       *secondary_channel,
			       sec_chan_idx_80p80,
			       oper_centr_freq_seg0_idx,
			       oper_centr_freq_seg1_idx);

	return chan;
}

static int dfs_get_next_lower_chwidth(u8 chwidth)
{
	switch (chwidth) {
	case CONF_OPER_CHWIDTH_320MHZ:
		return CONF_OPER_CHWIDTH_160MHZ;
	case CONF_OPER_CHWIDTH_160MHZ:
		return CONF_OPER_CHWIDTH_80MHZ;
	case CONF_OPER_CHWIDTH_80P80MHZ:
		return CONF_OPER_CHWIDTH_80MHZ;
	case CONF_OPER_CHWIDTH_80MHZ:
		return CONF_OPER_CHWIDTH_USE_HT;
	default:
		return -1;
	}
}

struct hostapd_channel_data *
dfs_find_bw_reduced_channel(struct hostapd_iface *iface,
			   int *secondary_channel,
			   u8 *oper_centr_freq_seg0_idx,
			   u8 *oper_centr_freq_seg1_idx)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan = NULL;
	int i, channel;
	int seg1_start;
	u8 current_chwidth;
	u8 target_chwidth;
	int n_chans, n_chans1;
	int first_chan_idx;
	u8 saved_seg0_idx;
	u8 temp_seg0_idx, temp_seg1_idx;

	wpa_printf(MSG_DEBUG, "DFS: Trying bandwidth reduction");

	if (!iface->conf->dfs_bw_reduce_en) {
		wpa_printf(MSG_DEBUG, "DFS: BW reduction disabled");
		return NULL;
	}

	mode = iface->current_mode;
	channel = iface->conf->channel;
	current_chwidth = hostapd_get_oper_chwidth(iface->conf);

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].chan == channel) {
			chan = &mode->channels[i];
			break;
		}
	}

	if (!chan) {
		wpa_printf(MSG_ERROR, "DFS: Current channel %d not found",
			   channel);
		return NULL;
	}

	if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
	     HOSTAPD_CHAN_DFS_UNAVAILABLE) {
		wpa_printf(MSG_DEBUG,
			   "DFS: Primary channel %d in NOL, cannot reduce BW",
			   channel);
		return NULL;
	}

	if (current_chwidth == CONF_OPER_CHWIDTH_USE_HT) {
		if (iface->conf->secondary_channel) {
			*secondary_channel = 0;
			*oper_centr_freq_seg0_idx = 0;
			*oper_centr_freq_seg1_idx = 0;
			wpa_printf(MSG_INFO,
				   "DFS: BW reduction successful - Ch %d, 20 MHz",
				   channel);
			return chan;
		}
		wpa_printf(MSG_DEBUG,
			   "DFS: Already at minimum BW (20 MHz)");
		return NULL;
	}

	target_chwidth = dfs_get_next_lower_chwidth(current_chwidth);
	if (target_chwidth < 0) {
		wpa_printf(MSG_ERROR, "DFS: Unknown bandwidth");
		return NULL;
	}

	/* Save original seg0_idx — will be temporarily overwritten
	 * in the loop to compute correct first channel for each
	 * target BW anchored to our primary channel
	 */
	saved_seg0_idx = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);

	while (1) {
		n_chans = dfs_get_used_n_chans(iface, &n_chans1, target_chwidth);

		/* Temporarily set target BW and recompute seg0_idx so that
		 * dfs_get_start_chan_idx returns the correct first channel.
		 */
		temp_seg0_idx = 0;
		temp_seg1_idx = 0;
		hostapd_set_oper_chwidth(iface->conf, target_chwidth);
		dfs_adjust_center_freq(iface, chan,
				       *secondary_channel, -1,
				       &temp_seg0_idx,
				       &temp_seg1_idx);
		hostapd_set_oper_centr_freq_seg0_idx(iface->conf, temp_seg0_idx);

		first_chan_idx = dfs_get_start_chan_idx(iface, &seg1_start,
							target_chwidth,
							channel, false);

		/* Restore original BW and seg0_idx */
		hostapd_set_oper_chwidth(iface->conf, current_chwidth);
		hostapd_set_oper_centr_freq_seg0_idx(iface->conf, saved_seg0_idx);
		if (first_chan_idx < 0)
			break;

		if (dfs_chan_range_available(mode, first_chan_idx,
					     n_chans, DFS_AVAILABLE)) {
			/*
			 * Every target bandwidth above 20 MHz is built from an
			 * HT40 pair, so secondary_channel must be derived here
			 * for all of them, not just CONF_OPER_CHWIDTH_USE_HT -
			 * hostapd_set_freq_params() rejects 80/160/320 MHz
			 * configs with no secondary channel offset. But
			 * CONF_OPER_CHWIDTH_USE_HT itself covers both HT40
			 * (n_chans == 2) and plain 20 MHz (n_chans == 1), so
			 * +1/-1 is only valid when the target is actually
			 * HT40 - otherwise it must stay 0.
			 */
			if (target_chwidth == CONF_OPER_CHWIDTH_USE_HT &&
			    n_chans == 1)
				*secondary_channel = 0;
			else
				*secondary_channel =
					(channel < temp_seg0_idx) ? 1 : -1;

			hostapd_set_oper_chwidth(iface->conf, target_chwidth);
			dfs_adjust_center_freq(iface, chan,
					       *secondary_channel, -1,
						oper_centr_freq_seg0_idx,
						oper_centr_freq_seg1_idx);

			wpa_printf(MSG_INFO,
				   "DFS: BW reduction successful - Ch %d, target BW %d",
				   channel, target_chwidth);
			return chan;
		}

		target_chwidth = dfs_get_next_lower_chwidth(target_chwidth);
		if (target_chwidth < 0)
			break;
	}
	wpa_printf(MSG_DEBUG,
		   "DFS: No valid reduced BW found for channel %d", channel);
	return NULL;

}


static int dfs_set_valid_channel(struct hostapd_iface *iface, int skip_radar)
{
	struct hostapd_channel_data *channel;
	u8 cf1 = 0, cf2 = 0;
	int sec = 0;

	channel = dfs_get_valid_channel(iface, &sec, &cf1, &cf2,
					skip_radar ? DFS_AVAILABLE :
					DFS_ANY_CHANNEL);
	if (!channel) {
		wpa_printf(MSG_ERROR, "could not get valid channel");
		return -1;
	}

	iface->freq = channel->freq;
	iface->conf->channel = channel->chan;
	iface->conf->secondary_channel = sec;
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf, cf1);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf, cf2);

	return 0;
}


int set_dfs_state_freq(struct hostapd_iface *iface, int freq, u32 state)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan = NULL;
	int i;

	mode = iface->current_mode;
	if (mode == NULL)
		return 0;

	wpa_printf(MSG_DEBUG, "set_dfs_state 0x%X for %d MHz", state, freq);
	for (i = 0; i < iface->current_mode->num_channels; i++) {
		chan = &iface->current_mode->channels[i];
		if (chan->freq == freq) {
			if (chan->flag & HOSTAPD_CHAN_RADAR) {
				if (state == HOSTAPD_CHAN_DFS_AVAILABLE &&
				   !iface->conf->dfs_test_mode &&
				    (chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
				    HOSTAPD_CHAN_DFS_UNAVAILABLE) {
					wpa_printf(MSG_DEBUG,
						   "set_dfs_state: skip AVAILABLE for %d MHz - channel in NOL",
						   freq);
					return 1;
				}
				chan->flag &= ~HOSTAPD_CHAN_DFS_MASK;
				chan->flag |= state;
				return 1; /* Channel found */
			}
		}
	}
	wpa_printf(MSG_WARNING, "Can't set DFS state for freq %d MHz", freq);
	return 0;
}


#ifndef CONFIG_QCN_EXTN
static
#endif
int set_dfs_state(struct hostapd_iface *iface, int freq, int ht_enabled,
		  int chan_offset, int chan_width, int cf1,
		  int cf2, u32 state, u16 radar_bitmap)
{
	int n_chans = 1, i;
	struct hostapd_hw_modes *mode;
	int frequency = freq;
	int frequency2 = 0;
	int ret = 0;

	mode = iface->current_mode;
	if (mode == NULL)
		return 0;

	if (mode->mode != HOSTAPD_MODE_IEEE80211A) {
		wpa_printf(MSG_WARNING, "current_mode != IEEE80211A");
		return 0;
	}

	/* Seems cf1 and chan_width is enough here */
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
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_get_n_chans_and_frequency_extn(
			convert_to_oper_chan_width(chan_width),
			cf1, &n_chans, &frequency))
			break;
#endif /* CONFIG_QCN_EXTN */

		wpa_printf(MSG_INFO, "DFS chan_width %d not supported",
			   chan_width);
		break;
	}

	wpa_printf(MSG_DEBUG, "DFS freq: %dMHz, n_chans: %d", frequency,
		   n_chans);
	for (i = 0; i < n_chans; i++) {

		if (radar_bitmap && state == HOSTAPD_CHAN_DFS_UNAVAILABLE)
		{
			if (radar_bitmap & 1<<i)
				ret += set_dfs_state_freq(iface, frequency, state);
			frequency = frequency + 20;

			if (chan_width == CHAN_WIDTH_80P80 && (radar_bitmap & 1<<(i+4))) {
				ret += set_dfs_state_freq(iface, frequency2, state);
				frequency2 = frequency2 + 20;
			}
		} else {
			ret += set_dfs_state_freq(iface, frequency, state);
			frequency = frequency + 20;
			if (chan_width == CHAN_WIDTH_80P80) {
				ret += set_dfs_state_freq(iface, frequency2, state);
				frequency2 = frequency2 + 20;
			}
		}
	}

	return ret;
}

#ifdef RDK_ONEWIFI
int dfs_are_channels_overlapped(struct hostapd_iface *iface, int freq,
				       int chan_width, int cf1, int cf2)
#else
static int dfs_are_channels_overlapped(struct hostapd_iface *iface, int freq, int chan_width, int cf1, int cf2)
#endif
{
	int start_chan_idx, start_chan_idx1;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int n_chans, n_chans1, i, j, frequency = freq, radar_n_chans = 1;
	u8 radar_chan;
	int res = 0;
	int cur_chan_width = hostapd_get_oper_chwidth(iface->conf);

	/* Our configuration */
	mode = iface->current_mode;
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1, cur_chan_width,
						iface->conf->channel, false);
	n_chans = dfs_get_used_n_chans(iface, &n_chans1, cur_chan_width);

	/* Check we are on DFS channel(s) */
	if (!dfs_check_chans_radar(iface, start_chan_idx, n_chans))
		return 0;

	/* Reported via radar event */
	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		radar_n_chans = 1;
		if (frequency == 0)
			frequency = cf1;
		break;
	case CHAN_WIDTH_40:
		radar_n_chans = 2;
		frequency = cf1 - 10;
		break;
	case CHAN_WIDTH_80:
		radar_n_chans = 4;
		frequency = cf1 - 30;
		break;
	case CHAN_WIDTH_160:
		radar_n_chans = 8;
		frequency = cf1 - 70;
		break;
	default:
#ifdef CONFIG_QCN_EXTN
		if (!hostapd_get_n_chans_and_frequency_extn(
			convert_to_oper_chan_width(chan_width),
			cf1, &radar_n_chans, &frequency))
			break;
#endif /* CONFIG_QCN_EXTN */

		wpa_printf(MSG_INFO, "DFS chan_width %d not supported",
			   chan_width);
		break;
	}

	ieee80211_freq_to_chan(frequency, &radar_chan);

	for (i = 0; i < n_chans; i++) {
		chan = &mode->channels[start_chan_idx + i];
		if (!(chan->flag & HOSTAPD_CHAN_RADAR))
			continue;
		for (j = 0; j < radar_n_chans; j++) {
			wpa_printf(MSG_DEBUG, "checking our: %d, radar: %d",
				   chan->chan, radar_chan + j * 4);
			if (chan->chan == radar_chan + j * 4)
				res++;
		}
	}

	wpa_printf(MSG_DEBUG, "overlapped: %d", res);

	return res;
}


static unsigned int dfs_get_cac_time(struct hostapd_iface *iface,
				     int start_chan_idx, int n_chans)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i;
	unsigned int cac_time_ms = 0;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		if (start_chan_idx + i >= mode->num_channels)
			break;
		channel = &mode->channels[start_chan_idx + i];
		if (!(channel->flag & HOSTAPD_CHAN_RADAR))
			continue;
		if (channel->dfs_cac_ms > cac_time_ms)
			cac_time_ms = channel->dfs_cac_ms;
	}

	return cac_time_ms;
}

int hostapd_set_dfs_cac_time(struct hostapd_iface *iface)
{
	int n_chans, n_chans1, start_chan_idx, start_chan_idx1;
	int chan_width = hostapd_get_oper_chwidth(iface->conf);

	/* Get start (first) channel for current configuration */
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						chan_width,
						iface->conf->channel, false);
	if (start_chan_idx == -1)
		return -1;

	/* Get number of used channels, depend on width */
	n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);

#ifdef CONFIG_QCN_EXTN
	/* Setup CAC time */
	if (iface->mcst && iface->cs_time) {
		iface->dfs_cac_ms = iface->cs_time;
	} else {
#endif
		iface->dfs_cac_ms = dfs_get_cac_time(iface, start_chan_idx,
						     n_chans);
#ifdef CONFIG_QCN_EXTN
	}
#endif

	return 0;
}

/*
 * Main DFS handler
 * 1 - continue channel/ap setup
 * 0 - channel/ap setup will be continued after CAC
 * -1 - hit critical error
 */
int hostapd_handle_dfs(struct hostapd_iface *iface)
{
	int res, n_chans, n_chans1, start_chan_idx, start_chan_idx1;
	int skip_radar = 0;
	int chan_width = hostapd_get_oper_chwidth(iface->conf);

	if (is_6ghz_freq(iface->freq))
		return 1;

	if (!iface->current_mode) {
		/*
		 * This can happen with drivers that do not provide mode
		 * information and as such, cannot really use hostapd for DFS.
		 */
		wpa_printf(MSG_DEBUG,
			   "DFS: No current_mode information - assume no need to perform DFS operations by hostapd");
		return 1;
	}

	iface->cac_started = 0;
#ifdef CONFIG_QCN_EXTN
	iface->iface_extn.cac_abort = 0;
#endif

	do {
		/* Get start (first) channel for current configuration */
		start_chan_idx = dfs_get_start_chan_idx(iface,
							&start_chan_idx1, chan_width,
							iface->conf->channel, false);
		if (start_chan_idx == -1)
			return -1;

		/* Get number of used channels, depend on width */
		n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);

#ifdef CONFIG_QCN_EXTN
		/* Setup CAC time */
		if (iface->conf->conf_extn.skip_cac) {
			iface->dfs_cac_ms = 0;
		} else {
#endif
			iface->dfs_cac_ms = dfs_get_cac_time(iface, start_chan_idx,
							     n_chans);
#ifdef CONFIG_QCN_EXTN
		}
#endif

		/* Check if any of configured channels require DFS */
		res = dfs_check_chans_radar(iface, start_chan_idx, n_chans);
		wpa_printf(MSG_DEBUG,
			   "DFS %d channels required radar detection",
			   res);
		if (!res)
			return 1;

		/* Check if all channels are DFS available */
		res = dfs_check_chans_available(iface, start_chan_idx, n_chans);
		wpa_printf(MSG_DEBUG,
			   "DFS all channels available, (SKIP CAC): %s",
			   res ? "yes" : "no");
		if (res)
			return 1;

		/* Check if any of configured channels is unavailable */
		res = dfs_check_chans_unavailable(iface, start_chan_idx,
						  n_chans);
		wpa_printf(MSG_DEBUG, "DFS %d chans unavailable - choose other channel: %s",
			   res, res ? "yes": "no");
		if (res) {
			if (dfs_set_valid_channel(iface, skip_radar) < 0) {
				hostapd_set_state(iface, HAPD_IFACE_DFS);
				return 0;
			}
		}
	} while (res);

#ifdef CONFIG_QCN_EXTN
	if (hostapd_ignorecac_handle_dfs_extn(iface, start_chan_idx, n_chans)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: IGNORECAC=1 continue without CAC iface state=%s (%d)",
			   hostapd_state_text(iface->state), iface->state);
		return 1;
	}
#endif /* CONFIG_QCN_EXTN */
	/* Finally start CAC */
	hostapd_set_state(iface, HAPD_IFACE_DFS);
	wpa_printf(MSG_DEBUG, "DFS start CAC on %d MHz%s", iface->freq,
		   dfs_use_radar_background(iface) ? " (background)" : "");

#ifdef CONFIG_QCN_EXTN
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_START
		"freq=%d chan=%d sec_chan=%d, width=%d, seg0=%d, seg1=%d, cac_time=%ds bitmap:0x%04x",
		iface->freq,
		iface->conf->channel, iface->conf->secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		iface->dfs_cac_ms / 1000,
		iface->conf->punct_bitmap);
#else

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_START
		"freq=%d chan=%d sec_chan=%d, width=%d, seg0=%d, seg1=%d, cac_time=%ds",
		iface->freq,
		iface->conf->channel, iface->conf->secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		iface->dfs_cac_ms / 1000);
#endif

	res = hostapd_start_dfs_cac(
		iface, iface->conf->hw_mode, iface->freq, iface->conf->channel,
		iface->conf->ieee80211n, iface->conf->ieee80211ac,
		iface->conf->ieee80211ax, iface->conf->ieee80211be,
		iface->conf->ieee80211bn, iface->conf->secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		dfs_is_agile_cac_enabled(iface) ? false : dfs_use_radar_background(iface),
		iface->conf->bandwidth_device, iface->conf->center_freq_device);

	if (res) {
		wpa_printf(MSG_ERROR, "DFS start_dfs_cac() failed, %d", res);
		return -1;
	}

	if (!dfs_is_agile_cac_enabled(iface) && dfs_use_radar_background(iface)) {
		/* Cache background radar parameters. */
		iface->radar_background.channel = iface->conf->channel;
		iface->radar_background.secondary_channel =
			iface->conf->secondary_channel;
		iface->radar_background.freq = iface->freq;
		iface->radar_background.centr_freq_seg0_idx =
			hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
		iface->radar_background.centr_freq_seg1_idx =
			hostapd_get_oper_centr_freq_seg1_idx(iface->conf);

		/*
		 * Let's select a random channel according to the
		 * regulations and perform CAC on dedicated radar chain.
		 */
		res = dfs_set_valid_channel(iface, 1);
		if (res < 0)
			return res;

		iface->radar_background.temp_ch = 1;
		return 1;
	}

	return 0;
}


int hostapd_is_dfs_chan_available(struct hostapd_iface *iface)
{
	int n_chans, n_chans1, start_chan_idx, start_chan_idx1;
	int chan_width = hostapd_get_oper_chwidth(iface->conf);

	if (!iface->current_mode)
		return 0;

	/* Get the start (first) channel for current configuration */
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						chan_width,
						iface->conf->channel, false);
	if (start_chan_idx < 0)
		return 0;

	/* Get the number of used channels, depending on width */
	n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);

	/* Check if all channels are DFS available */
	return dfs_check_chans_available(iface, start_chan_idx, n_chans);
}


#ifndef CONFIG_QCN_EXTN
static
#endif
int hostapd_dfs_request_channel_switch(struct hostapd_iface *iface,
					      int channel, int freq,
					      int secondary_channel,
					      u8 current_vht_oper_chwidth,
					      u8 oper_centr_freq_seg0_idx,
					      u8 oper_centr_freq_seg1_idx,
					      u16 punct_bitmap)
{
	struct hostapd_hw_modes *cmode = iface->current_mode;
	int ieee80211_mode = IEEE80211_MODE_AP, err;
	struct csa_settings csa_settings;
	unsigned int i;
	unsigned int num_err = 0;
	u8 op_class, chan;

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_send_uplink_csa_extn(iface, channel, freq, secondary_channel,
				current_vht_oper_chwidth, oper_centr_freq_seg0_idx,
				oper_centr_freq_seg1_idx, punct_bitmap)) {
		return 0;
	}

	if (!hostapd_send_rcsa_extn(iface, channel, freq, secondary_channel,
				    current_vht_oper_chwidth,
				    oper_centr_freq_seg0_idx,
				    oper_centr_freq_seg1_idx, punct_bitmap))
		return 0;
#endif

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

	/* Setup CSA request */
	os_memset(&csa_settings, 0, sizeof(csa_settings));
	csa_settings.cs_count = 5;
	csa_settings.block_tx = 1;
	csa_settings.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (iface->bss[0]->conf->mld_ap)
		csa_settings.link_id = iface->bss[0]->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_MESH
	if (iface->mconf)
		ieee80211_mode = IEEE80211_MODE_MESH;
#endif /* CONFIG_MESH */
	err = hostapd_set_freq_params(&csa_settings.freq_params,
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
				      current_vht_oper_chwidth,
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

	hostapd_get_channel_switch_time(iface, &csa_settings.freq_params);

	/* If mesh VAP present, trigger mesh CSA prior to AP channel switch.
	 * Constraint: CSA beacon must be transmitted within ~550 ms of
	 * radar detection. Cannot wait for mesh TBTT (1000 TU). */
	hostapd_ubus_mesh_switch_channel(iface, &csa_settings);

	if (hostapd_check_reenable_bss(iface, REENABLE_HT_SCAN) ||
	    hostapd_check_reenable_bss(iface, REENABLE_CAC)) {
		/*
		 * BSSes are deferred for HT scan or CAC. Switch them
		 * alongside any started non-deferred BSSes.
		 * REENABLE_REUSE_LINK (user-disabled) BSSes are NOT
		 * switched here — their chanctx is managed via DEL_BEACON
		 * and they track the channel automatically.
		 */
		hostapd_switch_pending_bss(iface, &csa_settings,
					   REENABLE_HT_SCAN);
		hostapd_switch_pending_bss(iface, &csa_settings,
					   REENABLE_CAC);
		for (i = 0; i < iface->num_bss; i++) {
			struct hostapd_data *b = iface->bss[i];

			if (!b->started || hapd_reenable_pending(b))
				continue;
			err = hostapd_switch_channel(b, &csa_settings);
			if (err)
				num_err++;
		}
	} else {
		for (i = 0; i < iface->num_bss; i++) {
			err = hostapd_switch_channel(iface->bss[i], &csa_settings);
			if (err)
				num_err++;
		}
	}

	if (num_err == iface->num_bss) {
		wpa_printf(MSG_WARNING,
			   "DFS failed to schedule CSA (%d) - trying fallback",
			   err);
		iface->freq = freq;
		iface->conf->channel = channel;
		iface->conf->secondary_channel = secondary_channel;
		hostapd_set_oper_chwidth(iface->conf, current_vht_oper_chwidth);
		hostapd_set_oper_centr_freq_seg0_idx(iface->conf,
						     oper_centr_freq_seg0_idx);
		hostapd_set_oper_centr_freq_seg1_idx(iface->conf,
						     oper_centr_freq_seg1_idx);
		if (ieee80211_freq_to_channel_ext(freq, secondary_channel,
						  current_vht_oper_chwidth,
						  &op_class, &chan) !=
		    NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_DEBUG, "Update op_class %d->%d",
				   iface->conf->op_class, op_class);
			iface->conf->op_class = op_class;
		}

		hostapd_disable_iface(iface);
		hostapd_enable_iface(iface);

		return 0;
	}

	/* Channel configuration will be updated once CSA completes and
	 * ch_switch_notify event is received */
	wpa_printf(MSG_DEBUG, "DFS waiting channel switch event");

	return 0;
}

int hostapd_dfs_count_precac_channels(struct hostapd_iface *iface)
{
	int num_usable;

	/* dfs_find_channel with ret_chan=NULL and idx=0 returns the count of matching channels */
	num_usable = dfs_find_channel(iface, NULL, 0, DFS_NO_CAC_YET,
				      DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH);
	wpa_printf(MSG_DEBUG, "PRECAC_Found %d channels that need PreCAC", num_usable);

	return num_usable;
}

static bool
hostapd_dfs_intercac_check_pref_block(struct hostapd_iface *iface,
				      int pref_chan,
				      enum oper_chan_width chan_width,
				      bool *all_available,
				      enum dfs_channel_type type)
{
	int n_chans, n_chans1, start_chan_idx, radar_chans;
	bool range_ok;

	n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);
	if (n_chans <= 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: boot: preferred n_chans failed for width %d",
			   chan_width);
		return false;
	}

	start_chan_idx = dfs_get_start_chan_idx(iface, &n_chans1,
						chan_width,
						pref_chan, false);
	if (start_chan_idx < 0) {
		wpa_printf(MSG_DEBUG,
			   "intercac: boot: preferred ch %d width %d not usable",
			   pref_chan, chan_width);
		return false;
	}

	radar_chans = dfs_check_chans_radar(iface, start_chan_idx, n_chans);
	wpa_printf(MSG_DEBUG,
		   "intercac: boot: preferred block ch %d width %d has %d DFS channels",
		   pref_chan, chan_width, radar_chans);
	if (!radar_chans)
		return false;

	if (type == DFS_AVAILABLE) {
		if (!all_available)
			return dfs_chan_range_available(iface->current_mode,
							start_chan_idx, n_chans,
							DFS_AVAILABLE);

		*all_available = dfs_check_chans_available(iface,
							   start_chan_idx,
							   n_chans);
		if (*all_available)
			wpa_printf(MSG_INFO,
				   "intercac: preferred block ch %d width %d already DFS available",
				   pref_chan, chan_width);
	} else {
		range_ok = dfs_chan_range_available(iface->current_mode,
						    start_chan_idx, n_chans,
						    type);
		if (!range_ok) {
			wpa_printf(MSG_DEBUG,
				   "intercac: preferred block ch %d width %d not valid for type %d",
				   pref_chan, chan_width, type);
			return false;
		}

		if (all_available)
			*all_available = false;
	}

	return true;
}

static bool
hostapd_dfs_intercac_is_pref_chan(struct hostapd_hw_modes *mode,
				  int completed_chan,
				  int preferred_chan,
				  int n_chans_pref,
				  int n_chans_oper,
				  int *first_chan_idx)
{
	int i;

	*first_chan_idx = -1;
	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].chan == preferred_chan) {
			*first_chan_idx = i;
			break;
		}
	}

	if (*first_chan_idx < 0)
		return false;

	for (i = 0; i < n_chans_pref; i += n_chans_oper) {
		if (*first_chan_idx + i >= mode->num_channels)
			break;
		if (mode->channels[*first_chan_idx + i].chan == completed_chan)
			return true;
	}

	return false;
}

static struct hostapd_channel_data *
hostapd_dfs_get_preferred_precac_channel(struct hostapd_iface *iface)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;
	int idx;
	enum oper_chan_width preferred_width, rcac_width;
	int n_chans1, n_chans_pref, n_chans_oper;

	if (!iface || !iface->user_rcac_channel)
		return NULL;

	if (!iface->conf->intercac_chan)
		return NULL;

	mode = iface->current_mode;
	if (!mode)
		return NULL;

	/*
	 * Reuse user_rcac_channel as a preferred agile CAC channel selector.
	 * Despite the rcac name, this is also used for PreCAC in ETSI domain
	 * to prioritize the configured/preferred channel before random
	 * selection.
	 */
	preferred_width = iface->preferred_chan_width;
	rcac_width = hostapd_get_oper_chwidth(iface->conf);

	n_chans_pref = dfs_get_used_n_chans(iface, &n_chans1,
					    preferred_width);
	n_chans_oper = dfs_get_used_n_chans(iface, &n_chans1,
					    rcac_width);
	if (n_chans_pref <= 0 || n_chans_oper <= 0)
		return NULL;

	if (!hostapd_dfs_intercac_is_pref_chan(mode,
					       iface->user_rcac_channel,
					       iface->preferred_chan,
					       n_chans_pref,
					       n_chans_oper,
					       &idx)) {
		wpa_printf(MSG_DEBUG,
			   "PRECAC_Preferred agile CAC channel %d not in preferred block %d",
			   iface->user_rcac_channel, iface->preferred_chan);
		return NULL;
	}


	for (idx = 0; idx < mode->num_channels; idx++) {
		chan = &mode->channels[idx];

		if (chan->chan != iface->user_rcac_channel)
			continue;
		if (dfs_chan_range_available(mode, idx, n_chans_oper,
					     DFS_NO_CAC_YET)) {
			wpa_printf(MSG_INFO,
				   "PRECAC_Using preferred agile CAC channel %d",
				   iface->user_rcac_channel);
			return chan;
		}

		wpa_printf(MSG_DEBUG,
			   "PRECAC_Preferred agile CAC channel %d not eligible",
			   iface->user_rcac_channel);
		break;
	}

	return NULL;
}

static struct hostapd_channel_data *
hostapd_dfs_get_next_precac_channel(struct hostapd_iface *iface,
				    u8 *oper_centr_freq_seg0_idx,
				    u8 *oper_centr_freq_seg1_idx,
				    int *secondary_channel,
				    enum oper_chan_width home_chwidth)
{
	struct hostapd_channel_data *chan = NULL;
	int total, idx;
	int bw_mhz;
	u8 seg0_tmp = 0;
	enum oper_chan_width oper_width_tmp;

	wpa_printf(MSG_INFO, "PRECAC_Performing Precac on the DFS Channel list");


	chan = hostapd_dfs_get_preferred_precac_channel(iface);
	if (chan)
		goto found;

	total = dfs_find_channel(iface, NULL, 0, DFS_NO_CAC_YET,
				 DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH);
	if (total == 0) {
		wpa_printf(MSG_INFO, "PRECAC_No DFS_USABLE channels remain");
		return NULL;
	}

	for (idx = 0; idx < total; idx++) {
		dfs_find_channel(iface, &chan, idx, DFS_NO_CAC_YET,
				 DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH);
		if (!chan)
			continue;
		if (dfs_is_home_chan(iface, chan, home_chwidth)) {
			wpa_printf(MSG_DEBUG,
				   "PRECAC_Skipping home channel block %d",
				   chan->chan);
			chan = NULL;
			continue;
		}
		break;
	}

	if (!chan) {
		wpa_printf(MSG_INFO, "PRECAC_No eligible channel found");
		return NULL;
	}

found:
	wpa_printf(MSG_INFO,
		   "PRECAC_Checking channel %d (freq=%d MHz)",
		   chan->chan, chan->freq);

	bw_mhz = channel_width_to_int(
		     hostapd_get_chan_width_from_oper_chan_width(iface->conf));
	hostapd_dfs_compute_bgcac_chan_params(chan->chan, bw_mhz, &oper_width_tmp,
				 &seg0_tmp, secondary_channel);

	dfs_adjust_center_freq(iface, chan, *secondary_channel, 0,
			       oper_centr_freq_seg0_idx,
			       oper_centr_freq_seg1_idx);

	wpa_printf(MSG_DEBUG,
		   "PRECAC_Next channel: %d (freq=%d MHz) seg0=%d seg1=%d",
		   chan->chan, chan->freq,
		   *oper_centr_freq_seg0_idx,
		   *oper_centr_freq_seg1_idx);

	return chan;
}

static bool dfs_precac_try_half_bw(struct hostapd_iface *iface)
{
	int home_bw = channel_width_to_int(
		      hostapd_get_chan_width_from_oper_chan_width(iface->conf));
	int half_bw = home_bw / 2;
	enum oper_chan_width orig_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	int orig_secondary_channel = iface->conf->secondary_channel;
	enum oper_chan_width half_width;
	u8 seg0 = 0, seg1 = 0;
	int sec = 0;
	struct hostapd_channel_data *chan;
	int ret;

	if (half_bw < 20)
		return false;

	switch (half_bw) {
	case 20:
		half_width = CONF_OPER_CHWIDTH_USE_HT;
		break;
	case 40:
		half_width = CONF_OPER_CHWIDTH_USE_HT;
		break;
	case 80:
		half_width = CONF_OPER_CHWIDTH_80MHZ;
		break;
	case 160:
		half_width = CONF_OPER_CHWIDTH_160MHZ;
		break;
	default:
		return false;
	}

	wpa_printf(MSG_DEBUG,
		   "PRECAC_No full-BW channel found - trying half-BW (%d MHz)",
		   half_bw);

	hostapd_set_oper_chwidth(iface->conf, half_width);
	if (half_bw == 20)
		iface->conf->secondary_channel = 0;

	chan = hostapd_dfs_get_next_precac_channel(iface, &seg0, &seg1, &sec,
						   orig_oper_chwidth);
	if (!chan) {
		wpa_printf(MSG_DEBUG,
			   "PRECAC_No half-BW channel available either");
		hostapd_set_oper_chwidth(iface->conf, orig_oper_chwidth);
		iface->conf->secondary_channel = orig_secondary_channel;
		return false;
	}

	wpa_printf(MSG_INFO,
		   "PRECAC_Starting background CAC on channel %d at half-BW (%d MHz)",
		   chan->chan, half_bw);

	ret = hostapd_start_dfs_cac(iface, iface->conf->hw_mode,
				    chan->freq, chan->chan,
				    iface->conf->ieee80211n,
				    iface->conf->ieee80211ac,
				    iface->conf->ieee80211ax,
				    iface->conf->ieee80211be,
				    iface->conf->ieee80211bn,
				    sec,
				    hostapd_get_oper_chwidth(iface->conf),
				    seg0, seg1,
				    true, 0, 0);

	if (ret) {
		wpa_printf(MSG_ERROR,
			   "PRECAC_half-BW hostapd_start_dfs_cac() failed: %d", ret);
		hostapd_set_oper_chwidth(iface->conf, orig_oper_chwidth);
		iface->conf->secondary_channel = orig_secondary_channel;
		return false;
	}

	iface->radar_background.channel = chan->chan;
	iface->radar_background.freq = chan->freq;
	iface->radar_background.secondary_channel = sec;
	iface->radar_background.centr_freq_seg0_idx = seg0;
	iface->radar_background.centr_freq_seg1_idx = seg1;
	iface->radar_background.chwidth = hostapd_get_oper_chwidth(iface->conf);
	iface->radar_background.cac_started = 1;

	hostapd_set_oper_chwidth(iface->conf, orig_oper_chwidth);
	iface->conf->secondary_channel = orig_secondary_channel;

	return true;
}

int hostapd_dfs_start_precac(struct hostapd_iface *iface)
{
	struct hostapd_channel_data *chan;
	u8 seg0 = 0, seg1 = 0;
	int sec = 0;
	int ret;

	chan = hostapd_dfs_get_next_precac_channel(iface, &seg0, &seg1, &sec,
						   hostapd_get_oper_chwidth(iface->conf));
	if (!chan) {
		if (dfs_precac_try_half_bw(iface))
			return 0;
		wpa_printf(MSG_INFO,
			   "PRECAC_No eligible DFS channel found - all processed or none available");
		iface->radar_background.channel = -1;
		iface->radar_background.freq = 0;
		iface->radar_background.cac_started = 0;

		return 0;
	}

	wpa_printf(MSG_INFO,
		   "PRECAC_Starting background CAC on channel %d (freq=%d MHz)",
		   chan->chan, chan->freq);

	ret = hostapd_start_dfs_cac(iface, iface->conf->hw_mode,
				    chan->freq, chan->chan,
				    iface->conf->ieee80211n,
				    iface->conf->ieee80211ac,
				    iface->conf->ieee80211ax,
				    iface->conf->ieee80211be,
				    iface->conf->ieee80211bn,
				    sec,
				    hostapd_get_oper_chwidth(iface->conf),
				    seg0, seg1,
				    true, 0, 0);

	if (ret) {
		wpa_printf(MSG_ERROR,
			   "PRECAC_hostapd_start_dfs_cac() failed: %d", ret);
		return ret;
	}

	iface->radar_background.channel = chan->chan;
	iface->radar_background.freq = chan->freq;
	iface->radar_background.secondary_channel = sec;
	iface->radar_background.centr_freq_seg0_idx = seg0;
	iface->radar_background.centr_freq_seg1_idx = seg1;
	iface->radar_background.chwidth = hostapd_get_oper_chwidth(iface->conf);
	iface->radar_background.cac_started = 1;

	return 0;
}


/*
 * dfs_try_user_rcac_channel - Start RCAC on the user-configured channel.
 *
 * Tries home channel BW first. If that fails and home BW is 320 MHz (which
 * is not supported for RCAC), retries at half BW (160 MHz). Returns false
 * to fall back to random channel selection only if both attempts fail or the
 * channel overlaps with the home block.
 *
 * Returns true if RCAC was started successfully, false to fall back to
 * random channel selection.
 */
static bool dfs_try_user_rcac_channel(struct hostapd_iface *iface,
				      int home_bw)
{
	int home_center = (hostapd_get_oper_centr_freq_seg0_idx(iface->conf) * 5) + 5000;
	int home_start = home_center - home_bw / 2 + 10;
	int home_end = home_center + home_bw / 2 - 10;
	int user_freq = ieee80211_chan_to_freq(NULL, 81, iface->user_rcac_channel);
	bool started;

	if (user_freq > 0 && user_freq >= home_start && user_freq <= home_end) {
		wpa_printf(MSG_DEBUG,
			   "DFS: user-pinned RCAC channel %d overlaps with home block - skipping",
			   iface->user_rcac_channel);
		return false;
	}

	wpa_printf(MSG_DEBUG,
		   "DFS: trying user-configured RCAC channel %d bw=%d MHz",
		   iface->user_rcac_channel, home_bw);

	started = (hostapd_start_rcac_on_channel(iface, iface->user_rcac_channel,
						  home_bw) == 0);
	if (!started)
		wpa_printf(MSG_WARNING,
			   "DFS: failed to start user-configured RCAC on chan %d; falling back to Random Channel Selection",
			   iface->user_rcac_channel);
	return started;
}

/*
 * dfs_rcac_try_half_bw - Try RCAC at half the home bandwidth.
 * @iface: Pointer to hostapd interface
 * @home_bw: Home channel bandwidth in MHz (40/80/160/320)
 * @orig_oper_chwidth: Original channel width, restored after channel search
 *
 * Searches for a DFS_NO_CAC_YET channel (DFS_ANY_CHANNEL as fallback) at
 * home_bw / 2, excluding the home channel range and the active RCAC channel.
 * Supported half-BW values: 20, 40, 80, 160 MHz.
 *
 * Returns: true if half-BW RCAC started, false if no suitable channel found.
 */
static bool dfs_rcac_try_half_bw(struct hostapd_iface *iface,
				     int home_bw,
				     enum oper_chan_width orig_oper_chwidth)
{
	int half_bw = home_bw / 2;
	int orig_secondary_channel = iface->conf->secondary_channel;
	int home_center_freq = (hostapd_get_oper_centr_freq_seg0_idx(iface->conf) * 5) + 5000;
	int home_start = home_center_freq - (home_bw / 2) + 10;
	int home_end   = home_center_freq + (home_bw / 2) - 10;
	enum oper_chan_width half_width;
	struct hostapd_channel_data *c;
	u8 cf0 = 0, cf1 = 0;
	int sec = 0;

	wpa_printf(MSG_DEBUG,
		   "DFS: dfs_rcac_try_half_bw: home_bw=%d half_bw=%d sec_chan=%d",
		   home_bw, half_bw, iface->conf->secondary_channel);

	switch (half_bw) {
	case 20: half_width = CONF_OPER_CHWIDTH_USE_HT; break;
	case 40: half_width = CONF_OPER_CHWIDTH_USE_HT; break;
	case 80: half_width = CONF_OPER_CHWIDTH_80MHZ; break;
	case 160: half_width = CONF_OPER_CHWIDTH_160MHZ; break;
	default: return false;
	}

	hostapd_set_oper_chwidth(iface->conf, half_width);
	if (half_bw == 20)
		iface->conf->secondary_channel = 0;
	else if (half_bw == 40)
		iface->conf->secondary_channel = orig_secondary_channel;

	c = dfs_get_valid_channel(iface, &sec, &cf0, &cf1, DFS_ANY_CHANNEL);
	if (c && !(c->freq >= home_start && c->freq <= home_end) &&
	    hostapd_start_rcac_on_channel(iface, c->chan, half_bw) == 0) {
		hostapd_set_oper_chwidth(iface->conf, orig_oper_chwidth);
		iface->conf->secondary_channel = orig_secondary_channel;
		wpa_printf(MSG_DEBUG,
			   "DFS: half-BW RCAC: starting %d MHz RCAC on chan %d "
			   "(all %d MHz blocks have NOL channels)",
			   half_bw, c->chan, home_bw);
		return true;
	}

	hostapd_set_oper_chwidth(iface->conf, orig_oper_chwidth);
	iface->conf->secondary_channel = orig_secondary_channel;

	wpa_printf(MSG_WARNING,
		   "DFS: half-BW RCAC: no %d MHz channel available",
		   half_bw);
	return false;
}

/*
 * hostapd_agile_cac_update - Agile CAC channel selection and start.
 *
 * Selects an RCAC channel at home BW and starts background
 * CAC
 *
 * Channel selection priority:
 *   1. User-pinned channel (SET_RCAC_FREQ)
 *   2. Random DFS_ANY_CHANNEL at home BW (handles mixed blocks)
 *   3. Half-BW fallback
 *   4. dfs_downgrade_bandwidth() as last resort
 */
static void hostapd_agile_cac_update(struct hostapd_iface *iface)
{
	int home_bw = channel_width_to_int(
		hostapd_get_chan_width_from_oper_chan_width(iface->conf));
	int home_center = (hostapd_get_oper_centr_freq_seg0_idx(iface->conf) * 5) + 5000;
	int home_start = home_center - home_bw / 2 + 10;
	int home_end = home_center + home_bw / 2 - 10;
	struct hostapd_channel_data *c;
	int sec = 0;
	u8 cf0 = 0, cf1 = 0;
	enum oper_chan_width orig_oper_chwidth;

	if (iface->user_rcac_channel > 0 &&
	    dfs_try_user_rcac_channel(iface, home_bw))
		return;

	orig_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	c = dfs_get_valid_channel(iface, &sec, &cf0, &cf1, DFS_ANY_CHANNEL);
	if (c &&
	    !(c->freq >= home_start && c->freq <= home_end) &&
	    hostapd_start_rcac_on_channel(iface, c->chan, home_bw) == 0)
		return;

	/* Try half-BW if no full-BW block is available */
	if (dfs_rcac_try_half_bw(iface, home_bw, orig_oper_chwidth))
		return;

	wpa_printf(MSG_DEBUG, "DFS: Agile CAC: no channel available");
	iface->radar_background.channel = -1;
}


static void hostapd_dfs_update_background_chain(struct hostapd_iface *iface)
{
	int sec = 0;
	enum dfs_channel_type channel_type = DFS_NO_CAC_YET;
	struct hostapd_channel_data *channel;
	u8 oper_centr_freq_seg0_idx = 0;
	u8 oper_centr_freq_seg1_idx = 0;
	u8 current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	if (dfs_is_agile_cac_enabled(iface)) {
		hostapd_agile_cac_update(iface);
		return;
	}

	/*
	 * Allow selection of DFS channel in ETSI to comply with
	 * uniform spreading.
	 */
	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI)
		channel_type = DFS_ANY_CHANNEL;

	channel = dfs_get_valid_channel(iface, &sec, &oper_centr_freq_seg0_idx,
					&oper_centr_freq_seg1_idx,
					channel_type);
	if (!channel ||
	    channel->chan == iface->conf->channel ||
	    channel->chan == iface->radar_background.channel)
		channel = dfs_downgrade_bandwidth(iface, &sec,
						  &oper_centr_freq_seg0_idx,
						  &oper_centr_freq_seg1_idx,
						  &current_vht_oper_chwidth,
						  &channel_type);
	if (!channel ||
	    hostapd_start_dfs_cac(iface, iface->conf->hw_mode,
				  channel->freq, channel->chan,
				  iface->conf->ieee80211n,
				  iface->conf->ieee80211ac,
				  iface->conf->ieee80211ax,
				  iface->conf->ieee80211be,
				  iface->conf->ieee80211bn,
				  sec, current_vht_oper_chwidth,
				  oper_centr_freq_seg0_idx,
				  oper_centr_freq_seg1_idx, true, 0, 0)) {
		wpa_printf(MSG_ERROR, "DFS failed to start CAC offchannel");
		iface->radar_background.channel = -1;
		return;
	}

	iface->radar_background.channel = channel->chan;
	iface->radar_background.freq = channel->freq;
	iface->radar_background.secondary_channel = sec;
	iface->radar_background.centr_freq_seg0_idx = oper_centr_freq_seg0_idx;
	iface->radar_background.centr_freq_seg1_idx = oper_centr_freq_seg1_idx;

	wpa_printf(MSG_DEBUG,
		   "%s: setting background chain to chan %d (%d MHz)",
		   __func__, channel->chan, channel->freq);
}


bool
hostapd_is_freq_in_current_hw_info(struct hostapd_iface *iface, int freq)
{
	struct hostapd_channel_data *chan;

	if (!iface->current_mode)
		return false;

	chan = hw_mode_get_channel(iface->current_mode, freq, NULL);

	/* If channel data is not found for the given frequency, consider it is
	 * out of the current hardware info. */
	if (!chan)
		return false;

	return chan_in_current_hw_info(iface->current_hw_info, chan);
}


static bool
hostapd_dfs_is_background_event(struct hostapd_iface *iface, int freq)
{
	return dfs_use_radar_background(iface) &&
		iface->radar_background.channel != -1 &&
		iface->radar_background.freq == freq;
}

static void hostapd_dfs_agile_cac_restart_timeout(void *eloop_data,
						   void *user_data)
{
	struct hostapd_iface *iface = eloop_data;

	wpa_printf(MSG_DEBUG, "DFS: agile CAC restart timeout fired");
	hostapd_restart_agile_cac_after_ch_switch(iface);
}

void hostapd_schedule_agile_cac_restart(struct hostapd_iface *iface)
{
	if (!dfs_is_agile_cac_enabled(iface))
		return;
	if (!dfs_use_radar_background(iface))
		return;
	if (eloop_is_timeout_registered(hostapd_dfs_agile_cac_restart_timeout,
					iface, NULL))
		return;
	wpa_printf(MSG_DEBUG,
		   "DFS: scheduling agile CAC restart in %d sec",
		   HAPD_AGILE_CAC_RESTART_DELAY_SECS);
	eloop_register_timeout(HAPD_AGILE_CAC_RESTART_DELAY_SECS, 0,
			       hostapd_dfs_agile_cac_restart_timeout,
			       iface, NULL);
}

void hostapd_cancel_agile_cac_restart(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(hostapd_dfs_agile_cac_restart_timeout,
			     iface, NULL);
}

void hostapd_dfs_radar_handling_timeout(void *eloop_data, void *user_data)
{
	struct hostapd_iface *iface = eloop_data;

	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: radar handling timeout expired,"
			   "but CSA in progress - ignore");
		return;
	}

	wpa_printf(MSG_INFO, "Disabling interface %s since no channel"
		   " switch is initiated within radar handling timeout",
		   iface->conf->bss[0]->iface);

	hostapd_disable_iface(iface);
}

static int hostapd_dfs_testmode_set_beacon_csa(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];
	struct csa_settings csa_settings;
	int err = 0;

	/* Setup CSA request */
	os_memset(&csa_settings, 0, sizeof(csa_settings));
	csa_settings.cs_count = 5;
	csa_settings.block_tx = 1;
	csa_settings.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (iface->bss[0]->conf->mld_ap)
		csa_settings.link_id = iface->bss[0]->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	err = hostapd_set_freq_params(&csa_settings.freq_params,
				      iface->conf->hw_mode,
				      iface->freq,
				      iface->conf->channel,
				      iface->conf->enable_edmg,
				      iface->conf->edmg_channel,
				      iface->conf->ieee80211n,
				      iface->conf->ieee80211ac,
				      iface->conf->ieee80211ax,
				      iface->conf->ieee80211be,
				      iface->conf->ieee80211bn,
				      iface->conf->secondary_channel,
				      hostapd_get_oper_chwidth(iface->conf),
				      hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
				      hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
				      iface->current_mode->vht_capab,
				      &iface->current_mode->he_capab[IEEE80211_MODE_AP],
				      &iface->current_mode->eht_capab[IEEE80211_MODE_AP],
				      &iface->current_mode->uhr_capab[IEEE80211_MODE_AP],
				      hostapd_get_punct_bitmap(iface->bss[0]),
				      iface->conf->he_6ghz_reg_pwr_type,
#ifdef CONFIG_IEEE80211BN
				      hostapd_hw_get_freq(hapd,
					iface->conf->npca_primary_channel),
				      iface->conf->npca_punct_bitmap,
#else
				      0, 0,
#endif /* CONFIG_IEEE80211BN */
				      iface->conf->bandwidth_device,
				      iface->conf->center_freq_device);

	if (err) {
		wpa_printf(MSG_ERROR, "DFS failed to calculate CSA freq params");
		goto fail;
	}

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA)) {
		wpa_printf(MSG_INFO, "CSA is not supported");
		hostapd_disable_iface(iface);
		return -1;
	}

	/* Trigger mesh CSA before AP channel switch if mesh VAP present */
	hostapd_ubus_mesh_switch_channel(iface, &csa_settings);

	for (int i = 0; i < iface->num_bss; i++) {
		err = hostapd_switch_channel(iface->bss[i], &csa_settings);
		if (err) {
			wpa_printf(MSG_ERROR,
				   "CSA failed for BSS %d in dfs test mode", i);
			goto fail;
		}
	}

	wpa_printf(MSG_DEBUG, "CSA started for dfs test mode");

	return 0;

fail:
	hostapd_disable_iface(iface);
	return err;
}

static int
hostapd_dfs_start_channel_switch_background(struct hostapd_iface *iface)
{
	u8 current_vht_oper_chwidth;

	iface->conf->channel = iface->radar_background.channel;
	iface->freq = iface->radar_background.freq;
	iface->conf->secondary_channel =
		iface->radar_background.secondary_channel;
	hostapd_set_oper_centr_freq_seg0_idx(
		iface->conf, iface->radar_background.centr_freq_seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(
		iface->conf, iface->radar_background.centr_freq_seg1_idx);
	if (dfs_is_agile_cac_enabled(iface) && iface->radar_background.chwidth) {
		hostapd_set_oper_chwidth(iface->conf,
					 iface->radar_background.chwidth);
	}
	current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	if (!dfs_is_agile_cac_enabled(iface))
		hostapd_dfs_update_background_chain(iface);

	return hostapd_dfs_request_channel_switch(
		iface, iface->conf->channel, iface->freq,
		iface->conf->secondary_channel, current_vht_oper_chwidth,
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
		hostapd_get_punct_bitmap(iface->bss[0]));
}

bool hostapd_is_device_params_present(int chan_width, int cf1, int chan_width_device,
				      int cf_device)
{
	return (cf_device && chan_width_device &&
		chan_width_device != chan_width && cf_device != cf1);
}


static void hostapd_deferred_csa_dispatch(struct hostapd_iface *iface)
{
	struct hostapd_freq_params *freq_params;
	int i, err;
	u8 num_err = 0;

	freq_params = &iface->csa_settings.freq_params;

	if (!iface->num_bss || !iface->bss)
		return;

	/* Trigger mesh CSA before AP channel switch if mesh VAP present */
	hostapd_ubus_mesh_switch_channel(iface, &iface->csa_settings);

	for (i = 0; i < iface->num_bss; i++) {
		hostapd_chan_switch_config(iface->bss[i], freq_params);

		/*
		 * Skip BSSes that are not beaconing — NL80211_CMD_CHANNEL_SWITCH
		 * returns -ENOTCONN on a non-beaconing AP. BSSes that were
		 * user-disabled before the deferred CSA was queued will pick up
		 * the new channel via their own re-enable path.
		 */
		if (!iface->bss[i]->beacon_set_done) {
			wpa_printf(MSG_DEBUG,
				   "DFS deferred CSA: skip non-beaconing BSS %s",
				   iface->bss[i]->conf->iface);
			continue;
		}

		err = hostapd_switch_channel(iface->bss[i], &iface->csa_settings);
		if (err)
			num_err++;
	}

	if (num_err) {
		wpa_printf(MSG_WARNING, "Failed to schedule CSA - trying fallback");
		hostapd_switch_channel_fallback(iface, freq_params);
	}
}

/**
 * hostapd_dfs_precac_restart_after_radar - Restart PreCAC after radar detection
 * @iface: Pointer to interface data
 * @radar_freq: Frequency where radar was detected
 *
 * Called when radar is detected on a PreCAC channel or home channel.
 * This :
 * 1. Stops current PreCAC if radar was on the PreCAC channel
 * 2. Restarts PreCAC on the next available DFS_USABLE channel
 * 3. Returns: 0 on success, -1 on failure
 */
int hostapd_dfs_precac_restart_after_radar(struct hostapd_iface *iface,
					   int radar_freq)
{
	wpa_printf(MSG_INFO,
		   "PRECAC_Radar detected on freq=%d MHz - restarting PreCAC",
		   radar_freq);

	/* Check if radar was on the current PreCAC channel */
	if (iface->radar_background.cac_started &&
		iface->radar_background.freq == radar_freq) {
		wpa_printf(MSG_INFO,
			   "PRECAC_Radar on active PreCAC channel %d - stopping CAC",
			   iface->radar_background.channel);
		iface->radar_background.cac_started = 0;
		iface->radar_background.channel = -1;
		iface->radar_background.freq = 0;
	} else {
		wpa_printf(MSG_INFO,
			   "PRECAC_Radar on non-PreCAC channel (home or other) - PreCAC continues");
	}

	wpa_printf(MSG_INFO, "PRECAC_Restarting PreCAC on next available channel");
	return hostapd_dfs_start_precac(iface);
}

bool hostapd_dfs_intercac_boot(struct hostapd_iface *iface)
{
	struct hostapd_config *conf;
	enum oper_chan_width chan_width, inter_width;
	int inter_freq, inter_sec, inter_bw_mhz;
	u8 inter_seg0;
	int n_chans, n_chans1;
	bool pref_block_available;

	wpa_printf(MSG_DEBUG, "intercac: boot called");

	conf = iface->conf;
	if (!conf->intercac_chan) {
		wpa_printf(MSG_DEBUG,
			   "intercac: boot: not configured, skip");
		return false;
	}

	wpa_printf(MSG_DEBUG,
		   "intercac: boot: intercac_chan=%d chwidth=%d current_chan=%d freq=%d",
		   conf->intercac_chan, conf->intercac_chwidth,
		   conf->channel, iface->freq);

	if (!dfs_use_radar_background(iface)) {
		wpa_printf(MSG_DEBUG,
			   "intercac: boot: agile CAC not supported");
		return false;
	}

	chan_width = hostapd_get_oper_chwidth(conf);
	wpa_printf(MSG_DEBUG,
		   "intercac: boot: DFS ch %d chan_width=%d intercac_chwidth=%d",
		   conf->channel, chan_width, conf->intercac_chwidth);

	if (!hostapd_dfs_intercac_check_pref_block(iface, conf->channel,
						   chan_width,
						   &pref_block_available,
						   DFS_AVAILABLE)) {
		wpa_printf(MSG_DEBUG,
			   "intercac: boot: preferred block ch %d width %d does not need DFS, skip",
			   conf->channel, chan_width);
		return false;
	}

	if (pref_block_available)
		return false;

	n_chans = dfs_get_used_n_chans(iface, &n_chans1,
			conf->intercac_chwidth);
	if (n_chans <= 0) {
		wpa_printf(MSG_ERROR,
				"intercac: boot: n_chans failed for width %d",
				conf->intercac_chwidth);
		return false;
	}
	inter_bw_mhz = n_chans * 20;

	if (hostapd_dfs_compute_bgcac_chan_params(conf->intercac_chan,
						  inter_bw_mhz, &inter_width,
						  &inter_seg0, &inter_sec) < 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: boot: compute params failed for ch %d bw %d",
			   conf->intercac_chan, inter_bw_mhz);
		return false;
	}

	inter_freq = hostapd_hw_get_freq(iface->bss[0], conf->intercac_chan);
	if (inter_freq <= 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: boot: freq lookup failed for ch %d",
			   conf->intercac_chan);
		return false;
	}

	iface->preferred_chan = conf->channel;
	iface->preferred_chan_width = chan_width;

	wpa_printf(MSG_INFO,
		   "intercac: ACTIVATING - intermediate ch %d (%d MHz bw %d), preferred DFS ch %d (width %d), RCAC->ch %d",
		   conf->intercac_chan, inter_freq, inter_bw_mhz,
		   conf->channel, chan_width, conf->channel);

	conf->channel = conf->intercac_chan;
	iface->freq = inter_freq;
	conf->secondary_channel = inter_sec;
	hostapd_set_oper_chwidth(conf, inter_width);
	hostapd_set_oper_centr_freq_seg0_idx(conf, inter_seg0);
	hostapd_set_oper_centr_freq_seg1_idx(conf, 0);
	iface->user_rcac_channel = iface->preferred_chan;

	wpa_printf(MSG_INFO,
		   "intercac: boot done - AP on ch %d, RCAC pinned to ch %d",
		   conf->channel, iface->user_rcac_channel);
	return true;
}

static void
hostapd_dfs_intercac_step_subchan(struct hostapd_iface *iface,
				  int preferred_chan,
				  enum oper_chan_width preferred_width,
				  int completed_chan,
				  enum oper_chan_width cur_oper_width,
				  int first_chan_idx,
				  int n_chans_pref,
				  int n_chans_oper)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	int i, next_chan = 0, freq, bw_mhz;
	enum oper_chan_width oper_width;
	struct hostapd_channel_data *chan_data;
	u8 seg0;
	int sec;

	/* Walk sub-blocks to find first uncleaned one */
	for (i = 0; i < n_chans_pref; i += n_chans_oper) {
		if (first_chan_idx + i >= mode->num_channels)
			break;
		if (!dfs_chan_range_available(mode, first_chan_idx + i,
					     n_chans_oper, DFS_AVAILABLE)) {
			next_chan = mode->channels[first_chan_idx + i].chan;
			break;
		}
	}

	freq = hostapd_hw_get_freq(iface->bss[0], completed_chan);
	if (freq <= 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: step_subchan: freq lookup failed for ch %d",
			   completed_chan);
		return;
	}

	bw_mhz = n_chans_oper * 20;

	if (hostapd_dfs_compute_bgcac_chan_params(completed_chan, bw_mhz,
						  &oper_width, &seg0, &sec) < 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: step_subchan: compute params failed for ch %d",
			   completed_chan);
		return;
	}

	chan_data = hw_mode_get_channel(iface->current_mode, freq, NULL);
	if (chan_data) {
		int refined = dfs_is_chan_allowed(chan_data, 2) ? 1 : -1;

		wpa_printf(MSG_DEBUG,
			   "intercac: step_subchan: refined sec %d->%d via dfs_is_chan_allowed",
			   sec, refined);
		sec = refined;
	}

	wpa_printf(MSG_INFO,
		   "intercac: STEPPING to sub-chan %d (width %d), next RCAC ch %d, preferred ch %d (width %d) pending",
		   completed_chan, cur_oper_width, next_chan,
		   preferred_chan, preferred_width);

	/* Pin RCAC to next uncleaned sub-channel */
	iface->user_rcac_channel = next_chan;

	hostapd_dfs_request_channel_switch(iface, completed_chan, freq,
					   sec, cur_oper_width, seg0, 0, 0);
}

bool hostapd_dfs_intercac_agile_complete(struct hostapd_iface *iface,
					 int success)
{
	int preferred_chan;
	enum oper_chan_width preferred_width;
	int freq, bw_mhz;
	enum oper_chan_width oper_width;
	u8 seg0;
	int sec;
	struct hostapd_channel_data *chan_data = NULL;
	enum oper_chan_width cur_oper_width;
	int completed_chan;
	int n_chans_pref, n_chans_oper, n_chans1;
	int first_chan_idx;
	struct hostapd_hw_modes *mode;

	wpa_printf(MSG_DEBUG,
		   "intercac: agile_complete(success=%d)", success);

	preferred_chan = iface->preferred_chan;
	preferred_width = iface->preferred_chan_width;

	wpa_printf(MSG_DEBUG,
		   "intercac: agile_complete: preferred_chan=%d width=%d",
		   preferred_chan, preferred_width);

	if (!preferred_chan) {
		wpa_printf(MSG_DEBUG,
			   "intercac: agile_complete: not active");
		return false;
	}
	if (!success) {
		wpa_printf(MSG_DEBUG,
			   "intercac: agile_complete: agile CAC failed");
		return false;
	}

	cur_oper_width = hostapd_get_oper_chwidth(iface->conf);
	completed_chan = iface->radar_background.channel;
	mode = iface->current_mode;

	wpa_printf(MSG_DEBUG,
		   "intercac: agile_complete: cur_oper_width=%d completed_chan=%d",
		   cur_oper_width, completed_chan);

	n_chans_pref = dfs_get_used_n_chans(iface, &n_chans1, preferred_width);
	n_chans_oper = dfs_get_used_n_chans(iface, &n_chans1, cur_oper_width);
	bw_mhz = n_chans_pref * 20;
	if (n_chans_pref <= 0 || n_chans_oper <= 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: agile_complete: n_chans failed (pref=%d oper=%d)",
			   n_chans_pref, n_chans_oper);
		return false;
	}

	if (!hostapd_dfs_intercac_is_pref_chan(mode, completed_chan,
					       preferred_chan,
					       n_chans_pref, n_chans_oper,
					       &first_chan_idx)) {
		wpa_printf(MSG_DEBUG,
			   "intercac: agile_complete: completed ch %d not a sub-block of preferred ch %d, ignoring",
			   completed_chan, preferred_chan);
		return false;
	}

	if (!dfs_chan_range_available(mode, first_chan_idx, n_chans_pref,
				      DFS_AVAILABLE)) {
		/* Preferred block not fully available yet: intermediate step */
		hostapd_dfs_intercac_step_subchan(iface, preferred_chan,
						  preferred_width,
						  completed_chan,
						  cur_oper_width,
						  first_chan_idx,
						  n_chans_pref, n_chans_oper);
		return true;
	}

	wpa_printf(MSG_INFO,
		   "intercac: agile_complete: all sub-chans cleared, upgrading to ch %d (width %d)",
		   preferred_chan, preferred_width);


	freq = hostapd_hw_get_freq(iface->bss[0], preferred_chan);
	if (freq <= 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: agile_complete: freq lookup failed for ch %d",
			   preferred_chan);
		return false;
	}

	if (hostapd_dfs_compute_bgcac_chan_params(preferred_chan, bw_mhz,
						  &oper_width, &seg0, &sec) < 0) {
		wpa_printf(MSG_ERROR,
			   "intercac: agile_complete: compute params failed");
		return false;
	}

	chan_data = hw_mode_get_channel(iface->current_mode, freq, NULL);
	if (chan_data) {
		int refined = dfs_is_chan_allowed(chan_data, 2) ? 1 : -1;

		wpa_printf(MSG_DEBUG,
			   "intercac: agile_complete: refined sec %d->%d via dfs_is_chan_allowed",
			   sec, refined);
		sec = refined;
	}

	wpa_printf(MSG_INFO,
		   "intercac: SWITCHING to ch %d width %d",
		   preferred_chan, preferred_width);

	iface->preferred_chan = 0;
	iface->preferred_chan_width = CONF_OPER_CHWIDTH_USE_HT;
	iface->user_rcac_channel = 0;
	hostapd_dfs_request_channel_switch(iface, preferred_chan, freq,
					   sec, preferred_width, seg0, 0, 0);
	return true;
}

int hostapd_dfs_intercac_defer_non_radar_switch(struct hostapd_iface *iface,
						struct csa_settings *settings)
{
	struct hostapd_freq_params *freq;
	enum oper_chan_width width, cur_oper_width;
	int chan, n_chans, n_chans1, first_chan_idx, bw_mhz;
	int n_chans_oper, n_chans_rcac;

	if (!iface || !settings || !dfs_use_radar_background(iface) ||
	    !dfs_is_agile_cac_enabled(iface))
		return 0;

	if (!iface->conf->intercac_chan)
		return 0;

	/* Radar-triggered switches are time-critical and must not be deferred. */
	if (iface->radar_detected)
		return 0;

	freq = &settings->freq_params;
	chan = freq->channel;
	if (!chan || chan == iface->conf->channel)
		return 0;

	width = hostapd_chan_width_from_freq_params(freq);
	n_chans = dfs_get_used_n_chans(iface, &n_chans1, width);
	cur_oper_width = hostapd_get_oper_chwidth(iface->conf);
	n_chans_oper = dfs_get_used_n_chans(iface, &n_chans1,
					    cur_oper_width);
	if (n_chans <= 0 || n_chans_oper <= 0)
		return 0;
	/*
	 * Reuse the Inter CAC preferred-channel helper only to locate the
	 * first channel index of the requested target block. Since completed
	 * channel and preferred channel are both @chan and n_chans_oper equals
	 * n_chans, this is equivalent to finding the preferred block start
	 * while avoiding another open-coded channel-index lookup.
	 */
	if (!hostapd_dfs_intercac_is_pref_chan(iface->current_mode, chan, chan,
					       n_chans, n_chans,
					       &first_chan_idx))
		return 0;

	if (dfs_chan_range_available(iface->current_mode, first_chan_idx,
				     n_chans, DFS_AVAILABLE)) {
		wpa_printf(MSG_DEBUG,
			   "intercac: requested ch %d width %d already available; allow switch",
			   chan, width);
		return 0;
	}

	/*
	 * Do not switch to the requested channel now. Treat it as the new
	 * Inter CAC preferred target and let the existing agile-complete path
	 * perform step switching or final switching after CAC succeeds.
	 */
	iface->preferred_chan = chan;
	iface->preferred_chan_width = width;
	iface->user_rcac_channel = chan;

	if (iface->radar_background.cac_started &&
	    iface->radar_background.channel == chan) {
		wpa_printf(MSG_INFO,
			   "intercac: bg CAC already running on requested ch %d; keep it and defer switch",
			   chan);
		return 1;
	}

	if (iface->radar_background.cac_started ||
	    iface->radar_background.channel > 0)
		hostapd_abort_background_cac(iface);
	n_chans_rcac = n_chans_oper < n_chans ? n_chans_oper : n_chans;
	bw_mhz = n_chans_rcac * 20;

	wpa_printf(MSG_INFO,
		   "intercac: defer non-radar switch to ch %d width %d; start bg CAC with %dBW%d",
		   chan, width, bw_mhz, cur_oper_width);

	if (hostapd_start_rcac_on_channel(iface, chan, bw_mhz) == 0)
		return 1;

	wpa_printf(MSG_WARNING,
		   "intercac: failed to start bg CAC for deferred ch %d; allow original switch",
		   chan);
	iface->preferred_chan = 0;
	iface->user_rcac_channel = 0;
	iface->preferred_chan_width = CONF_OPER_CHWIDTH_USE_HT;

	return 0;
}


/**
 * hostapd_agile_complete - Handle Agile CAC (RCAC/PreCAC) completion
 * @iface: Pointer to hostapd interface data
 * @success: 1 if CAC completed successfully, 0 if aborted/failed
 * @freq: Frequency on which background CAC was performed
 *
 * Handles the completion (success or abort) of a background CAC event for
 * both RCAC and PreCAC modes
 *
 * Returns 0
 */
static int hostapd_agile_complete(struct hostapd_iface *iface, int success,
				  int freq)
{
	int precac_channel = iface->radar_background.channel;
	int precac_freq = iface->radar_background.freq;

	hostapd_dfs_intercac_agile_complete(iface, success);

	if (!success) {
		if (freq > 0 && iface->radar_background.freq > 0 &&
		    freq != iface->radar_background.freq) {
			wpa_printf(MSG_DEBUG,
				   "DFS: RCAC abort for old freq %d MHz (current RCAC freq %d MHz) - ignoring",
				   freq, iface->radar_background.freq);
			iface->radar_detected = false;
			return 0;
		}

		wpa_printf(MSG_INFO,
			   "DFS: Agile CAC failed/aborted on freq %d MHz",
			   freq);

		iface->radar_background.channel = -1;
		iface->radar_background.freq = 0;
	}


	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI) {
		if (success)
			wpa_printf(MSG_INFO,
				   "PRECAC_ CAC succeeded on chan %d (freq=%d MHz) - "
				   "moving to next channel",
				   precac_channel, freq);
		else if (freq == precac_freq && iface->radar_detected) {
			wpa_printf(MSG_INFO,
				   "PRECAC_ CAC aborted on chan %d (freq=%d MHz) - "
				   "radar detected, skipping to next channel",
				    precac_channel, freq);
		}
		else {
			wpa_printf(MSG_INFO,
				   "PRECAC_ CAC failed on chan %d (freq=%d MHz) - "
				   "channel switch in progress, will resume after switch",
				   precac_channel, freq);
			iface->radar_detected = false;
			iface->radar_background.cac_started = 0;
			return 0;
		}

		iface->radar_detected = false;
		iface->radar_background.cac_started = 0;
		return hostapd_dfs_start_precac(iface);
	}

	iface->radar_detected = false;
	iface->radar_background.cac_started = 0;
	return 0;
}

/*
 * hostapd_dfs_unpunc_cacdone_subchans - Initiate a channel switch request to
 * unpuncture the channels that have successfully completed the Puncturing CAC process.
 *
 * @iface: hostapd interface data
 * @freq: primary channel frequency
 * @channel: primary channel number
 * @secondary_channel: HT secondary channel offset
 * @chan_width: configured channel width
 * @cf1: center frequency segment 0
 * @cf2: center frequency segment 1
 * @unpuncture_bitmap: The bitmap containing channels to be unpunctured.
 *
 * Update the puncturing pattern by removing the bits corresponding to channels
 * that need to be unpunctured. Use the resulting updated puncture pattern,
 * which contains only the bits that remain in the punctured state, and send
 * this to the driver as part of the CSA.
 *
 * Reset the puncture source status for any channel that is being unpunctured.
 *
 * Return: 0 on success or a negative error code on failure
 */
static int hostapd_dfs_unpunc_cacdone_subchans(struct hostapd_iface *iface,
					       int freq, int channel,
					       int secondary_channel,
					       int chan_width, int cf1,
					       int cf2, u16 unpuncture_bitmap)
{
	u8 centr_chan1 = 0;
	u8 centr_chan2 = 0;
	enum oper_chan_width oper_chan_width;
	u16 puncture_bitmap;
	u16 radar_unpunc_bitmap;

	radar_unpunc_bitmap = dfs_get_dfs_punctured_bitmap(iface,
							   unpuncture_bitmap);
	if (!radar_unpunc_bitmap) {
		wpa_printf(MSG_DEBUG,
			   "DFS: skipping auto-unpuncture: no DFS-punctured subchannels eligible in bitmap=0x%04x",
			   unpuncture_bitmap);
		return 0;
	}

	dfs_reset_punc_bitmap_src(iface, radar_unpunc_bitmap);

	puncture_bitmap = iface->conf->punct_bitmap &
			  ~radar_unpunc_bitmap;
	iface->radar_bit_pattern = puncture_bitmap;

	oper_chan_width = convert_to_oper_chan_width(chan_width);
	if (cf1)
		ieee80211_freq_to_chan(cf1, &centr_chan1);
	if (cf2)
		ieee80211_freq_to_chan(cf2, &centr_chan2);

	if (oper_chan_width == CONF_OPER_CHWIDTH_320MHZ)
		puncture_bitmap = RIGHT80_240MHZ_PUNC | puncture_bitmap;

	wpa_printf(MSG_DEBUG,
		   "DFS: auto-unpuncture requesting CSA: chan=%d unpuncture_bitmap: 0x%04x, existing bitmap: 0x%04x, radar_unpunc_bitmap=0x%04x puncture_bitmap=0x%04x",
		   unpuncture_bitmap, iface->conf->punct_bitmap,
		   channel, radar_unpunc_bitmap, puncture_bitmap);

	return hostapd_dfs_request_channel_switch(iface, channel, freq,
						  secondary_channel,
						  oper_chan_width,
						  centr_chan1, centr_chan2,
						  puncture_bitmap);
}
int hostapd_dfs_complete_cac(struct hostapd_iface *iface, int success, int freq,
			     int ht_enabled, int chan_offset, int chan_width,
			     int cf1, int cf2, u16 unpunc_bitmap,
			     bool is_background, int chan_width_device,
			     int cf_device)
{
	struct hostapd_data *hapd = iface->bss[0];

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_COMPLETED
		"success=%d freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d radar_detected=%d"
		" chan_width_device=%d cf_device=%d cac_started=%d",
		success, freq, ht_enabled, chan_offset, chan_width, cf1, cf2,
		iface->radar_detected, chan_width_device, cf_device, iface->cac_started);

#ifdef CONFIG_QCN_EXTN
	/* Set cac_abort flag when CAC fails (success=0) */
	iface->iface_extn.cac_abort = !success;
#endif

	if (success) {
		u8 seg0;

		/* Complete iface/ap configuration */
		if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) {
			/* Complete AP configuration for the first bring up. If
			 * a radar was detected in this channel, interface setup
			 * will be handled in
			 * 1. hostapd_event_ch_switch() if switching to a
			 *    non-DFS channel
			 * 2. on next CAC complete event if switching to another
			 *    DFS channel.
			 */
			if (iface->state != HAPD_IFACE_ENABLED &&
			    !iface->radar_detected) {
				iface->cac_started = 0;
				if (hostapd_check_reenable_bss(iface,
							       REENABLE_CAC)) {
					hostapd_enable_pending_bss(
						iface, REENABLE_CAC, true);
				} else {
					hostapd_setup_interface_complete(iface, 0);
				}
			} else {
				iface->cac_started = 0;
			}
		} else {
			if (hostapd_is_device_params_present(chan_width, cf1,
							     chan_width_device, cf_device))
				set_dfs_state(iface, freq, ht_enabled, chan_offset,
					      chan_width_device, cf_device, cf2,
					      HOSTAPD_CHAN_DFS_AVAILABLE, 0);
			else
				set_dfs_state(iface, freq, ht_enabled, chan_offset,
					      chan_width, cf1, cf2,
					      HOSTAPD_CHAN_DFS_AVAILABLE, 0);

			/*
			 * Radar event from background chain for the selected
			 * channel. Perform CSA, move the main chain to the
			 * selected channel and configure the background chain
			 * to a new DFS channel.
			 */
			if (is_background || hostapd_dfs_is_background_event(iface, freq)) {
				if (dfs_is_agile_cac_enabled(iface))
					return hostapd_agile_complete(iface, success, freq);

				iface->radar_background.cac_started = 0;
				if (!iface->radar_background.temp_ch)
					return 0;

				iface->radar_background.temp_ch = 0;
				if (iface->conf->enable_background_radar)
					return hostapd_dfs_start_channel_switch_background(iface);
			}

			/*
			 * Just mark the channel available when CAC completion
			 * event is received in enabled state. CAC result could
			 * have been propagated from another radio having the
			 * same regulatory configuration. When CAC completion is
			 * received during non-HAPD_IFACE_ENABLED state, make
			 * sure the configured channel is available because this
			 * CAC completion event could have been propagated from
			 * another radio.
			 */
			if (iface->state != HAPD_IFACE_ENABLED &&
			    hostapd_is_dfs_chan_available(iface)) {
				iface->cac_started = 0;
				if (iface->cac_type == HAPD_CAC_COMPLETE_AFTER_BSS) {
#ifdef CONFIG_QCN_EXTN
					/*
					 * Only notify Rptr STA, if this CAC was directly triggered
					 * by ACS selecting a DFS channel at initial startup.
					 * acs_dfs_cac_pending is a one-shot flag: set only in
					 * hostapd_acs_completed() for DFS channel, cleared here.
					 */
					if (iface->iface_extn.acs_dfs_cac_pending) {
						iface->iface_extn.acs_dfs_cac_pending = false;
						wpa_printf(MSG_DEBUG,
						"ACS-selected DFS channel CAC "
						"completed on %d MHz — notify Rptr STA", freq);
						hostapd_ml_acs_check_and_notify(iface, true);
					}

					/*
					 * Boot-up CAC path: all BSSes were already created
					 * before CAC started.  Complete the bring-up here,
					 * before the state check below, so that
					 * iface->state == HAPD_IFACE_ENABLED after this call
					 * and the hostapd_setup_interface_complete() re-entry
					 * in the HAPD_CAC_COMPLETE_AFTER_BSS branch is
					 * naturally skipped.
					 */
					if (hostapd_bootup_cac_enabled_extn(iface)) {
						hostapd_bootup_cac_complete_extn(iface);
						goto cac_done;
					}
#endif /* CONFIG_QCN_EXTN */
					ieee80211_freq_to_chan(cf1, &seg0);
					hostapd_set_oper_centr_freq_seg0_idx(iface->conf, seg0);
					if (hostapd_check_reenable_bss(
						    iface, REENABLE_CAC)) {
						hostapd_enable_pending_bss(
							iface, REENABLE_CAC, true);
					} else {
						hostapd_setup_interface_complete(iface, 0);
					}
				} else if (iface->cac_type == HAPD_CAC_COMPLETE_AFTER_CSA) {
#ifdef CONFIG_QCN_EXTN
					hostapd_cleanup_cs_params(iface->bss[0]);
#endif
					hostapd_set_state(iface, HAPD_IFACE_ENABLED);
					wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, AP_EVENT_ENABLED);
					iface->cac_type = 0;

					for (size_t i = 0; i < iface->num_bss; i++) {
						struct hostapd_data *bss = iface->bss[i];

						if (!bss || bss->disabled || !bss->started)
							continue;

						ieee802_11_set_beacon(bss);
					}

					hostapd_start_device_cac_background(iface);
				}
			}

			if (unpunc_bitmap && iface->conf->use_ru_puncture_dfs &&
			    !iface->conf->dfs_disable_auto_unpunc) {
				int channel = iface->conf->channel;
				const int sec_offset = 1;

				return hostapd_dfs_unpunc_cacdone_subchans(iface, freq,
									   channel,
									   sec_offset,
									   chan_width,
									   cf1, cf2,
									   unpunc_bitmap);
			}
		}

#ifdef CONFIG_QCN_EXTN
		hostapd_csa_bitmap_update_extn(iface, freq);
#endif
	} else if (is_background || hostapd_dfs_is_background_event(iface, freq)) {
		if (dfs_is_agile_cac_enabled(iface))
			return hostapd_agile_complete(iface, success, freq);

		iface->radar_background.cac_started = 0;
		if (iface->conf->enable_background_radar)
			hostapd_dfs_update_background_chain(iface);
	} else {
		iface->cac_started = 0;
		iface->cac_type = 0;
#ifdef CONFIG_QCN_EXTN
		iface->bootup_cac_in_progress = 0;
#endif /* CONFIG_QCN_EXTN */
	}

#ifdef CONFIG_QCN_EXTN
cac_done:
#endif
	iface->radar_detected = false;

	if (hapd->iface->csa_pending_on_cac_abort &&
	    freq == iface->freq) {
		hostapd_deferred_csa_dispatch(hapd->iface);
		hapd->iface->csa_pending_on_cac_abort = false;
		os_memset(&hapd->iface->csa_settings, 0, sizeof(struct csa_settings));
	}

#ifdef CONFIG_QCN_EXTN
	if (!success && iface->iface_extn.country_change_post_cac_abort &&
	    freq == iface->freq)
		hostapd_country_change_post_cac_abort_extn(iface);
#endif /* CONFIG_QCN_EXTN */

	return 0;
}

int hostapd_dfs_pre_cac_expired(struct hostapd_iface *iface, int freq,
				int ht_enabled, int chan_offset, int chan_width,
				int cf1, int cf2,
				int chan_width_device, int cf_device)
{
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_PRE_CAC_EXPIRED
		"freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d"
		 "chan_width_device=%d cf_device=%d",
		freq, ht_enabled, chan_offset, chan_width, cf1, cf2,
		chan_width_device, cf_device);

	hostapd_ubus_notify_radar_detected(iface, freq, chan_width, cf1, cf2);

	/* Proceed only if DFS is not offloaded to the driver */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return 0;

	set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width,
		      cf1, cf2, HOSTAPD_CHAN_DFS_USABLE,0);

	if (dfs_is_agile_cac_enabled(iface) &&
	    iface->dfs_domain != HOSTAPD_DFS_REGION_ETSI &&
	    !iface->radar_background.cac_started &&
	    iface->radar_background.freq > 0 &&
	    iface->radar_background.freq == freq) {
		wpa_printf(MSG_DEBUG,
			   "DFS: PRE_CAC_EXPIRED for RCAC channel %d MHz - restarting RCAC",
			   freq);
		iface->radar_background.channel = -1;
		iface->radar_background.freq = 0;
		hostapd_dfs_update_background_chain(iface);
	}

	return 0;
}


static struct hostapd_channel_data *
dfs_downgrade_bandwidth(struct hostapd_iface *iface, int *secondary_channel,
			u8 *oper_centr_freq_seg0_idx,
			u8 *oper_centr_freq_seg1_idx,
			u8 *oper_chwidth,
			enum dfs_channel_type *channel_type)
{
	struct hostapd_channel_data *channel;
	int orig_secondary_channel = iface->conf->secondary_channel;
	u8 orig_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	for (;;) {
		channel = dfs_get_valid_channel(iface, secondary_channel,
						oper_centr_freq_seg0_idx,
						oper_centr_freq_seg1_idx,
						*channel_type);
		if (channel) {
			if (oper_chwidth)
				*oper_chwidth =
					hostapd_get_oper_chwidth(iface->conf);
			iface->conf->secondary_channel = orig_secondary_channel;
			hostapd_set_oper_chwidth(iface->conf,
						 orig_oper_chwidth);
			wpa_printf(MSG_DEBUG, "DFS: Selected channel: %d",
				   channel->chan);
			return channel;
		}

		if (*channel_type != DFS_ANY_CHANNEL) {
			*channel_type = DFS_ANY_CHANNEL;
		} else {
			int oper_chwidth;

			oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
			if (oper_chwidth == CONF_OPER_CHWIDTH_USE_HT) {
				/* try finding 20MHz channels if skip_unii1_dfs_switch is enabled */
				if (!iface->conf->skip_unii1_dfs_switch ||
				    !iface->conf->secondary_channel)
					break;
				iface->conf->secondary_channel = 0;
				continue;
			}
			*channel_type = DFS_AVAILABLE;
			hostapd_set_oper_chwidth(iface->conf,
						 oper_chwidth == CONF_OPER_CHWIDTH_320MHZ ?
						 CONF_OPER_CHWIDTH_160MHZ :
						 oper_chwidth - 1);
		}
	}

	iface->conf->secondary_channel = orig_secondary_channel;
	hostapd_set_oper_chwidth(iface->conf, orig_oper_chwidth);
	wpa_printf(MSG_INFO,
		   "%s: no DFS channels left, waiting for NOP to finish",
		   __func__);
	return NULL;
}


static int hostapd_dfs_start_channel_switch_cac(struct hostapd_iface *iface)
{
	struct hostapd_channel_data *channel;
	int secondary_channel;
	u8 oper_centr_freq_seg0_idx = 0;
	u8 oper_centr_freq_seg1_idx = 0;
	u8 current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);
	enum dfs_channel_type channel_type = DFS_ANY_CHANNEL;
	int err = 1;
	u8 op_class, chan;

	/* Radar detected during active CAC */
#ifndef CONFIG_QCN_EXTN
	iface->cac_started = 0;
#endif
	iface->conf->punct_bitmap = 0;
	channel = dfs_get_valid_channel(iface, &secondary_channel,
					&oper_centr_freq_seg0_idx,
					&oper_centr_freq_seg1_idx,
					channel_type);

	if (!channel) {
		channel = dfs_downgrade_bandwidth(iface, &secondary_channel,
						  &oper_centr_freq_seg0_idx,
						  &oper_centr_freq_seg1_idx,
						  &current_vht_oper_chwidth,
						  &channel_type);
		if (!channel) {
			wpa_printf(MSG_ERROR, "No valid channel available");
			return err;
		}
	}

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_send_rcsa_extn(iface, channel->chan, channel->freq, secondary_channel,
				    hostapd_get_oper_chwidth(iface->conf),
				    oper_centr_freq_seg0_idx,
				    oper_centr_freq_seg1_idx, 0)) {
		iface->cac_started = 0;
		return 0;
	}
	iface->cac_started = 0;
#endif
	wpa_printf(MSG_DEBUG, "DFS will switch to a new channel %d",
		   channel->chan);
	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NEW_CHANNEL
		"freq=%d chan=%d sec_chan=%d", channel->freq,
		channel->chan, secondary_channel);

	iface->freq = channel->freq;
	iface->conf->channel = channel->chan;
	iface->conf->secondary_channel = secondary_channel;
	hostapd_set_oper_chwidth(iface->conf, current_vht_oper_chwidth);
	hostapd_set_oper_centr_freq_seg0_idx(iface->conf,
					     oper_centr_freq_seg0_idx);
	hostapd_set_oper_centr_freq_seg1_idx(iface->conf,
					     oper_centr_freq_seg1_idx);
	if (ieee80211_freq_to_channel_ext(channel->freq, secondary_channel,
					  current_vht_oper_chwidth,
					  &op_class, &chan) !=
	    NUM_HOSTAPD_MODES) {
		wpa_printf(MSG_DEBUG, "Update op_class %d->%d",
			   iface->conf->op_class, op_class);
		iface->conf->op_class = op_class;
	}
	err = 0;


	if (hostapd_check_reenable_bss(iface, REENABLE_CAC))
		hostapd_enable_pending_bss(iface, REENABLE_CAC, true);
	else
		hostapd_setup_interface_complete(iface, err);

	return err;
}


static int
hostapd_dfs_background_start_channel_switch(struct hostapd_iface *iface,
					    int freq)
{
	if (!dfs_use_radar_background(iface))
		return -1; /* Background radar chain not supported. */

	wpa_printf(MSG_DEBUG,
		   "%s called (background CAC active: %s, CSA active: %s)",
		   __func__, iface->radar_background.cac_started ? "yes" : "no",
		   hostapd_csa_in_progress(iface) ? "yes" : "no");

	/* Check if CSA in progress */
	if (hostapd_csa_in_progress(iface))
		return 0;

	if (hostapd_dfs_is_background_event(iface, freq)) {
		/*
		 * Radar pattern is reported on the background chain.
		 * Clear the background state and select a new random channel.
		 */
		if (dfs_is_agile_cac_enabled(iface)) {
			if (iface->dfs_domain != HOSTAPD_DFS_REGION_ETSI) {
				if (iface->user_rcac_channel == iface->radar_background.channel)
					iface->user_rcac_channel = 0;

				iface->radar_background.cac_started = 0;
				iface->radar_background.channel = -1;
				iface->radar_background.freq = 0;
			} else {
				return hostapd_dfs_precac_restart_after_radar(iface, freq);
			}
		}

		hostapd_dfs_update_background_chain(iface);
		return 0;
	}

	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI &&
	    iface->conf->bgcac_en &&
	    hostapd_dfs_agile_cac_switch(iface) == 0)
		return 0;

	/*
	 * If background radar detection is supported and the radar channel
	 * monitored by the background chain is available switch to it without
	 * waiting for the CAC.
	 */
	if (iface->radar_background.channel == -1)
		return -1; /* Background radar chain not available. */

	if (iface->radar_background.cac_started) {
		/*
		 * Background channel not available yet. Perform CAC on the
		 * main chain.
		 */
		if (!dfs_is_agile_cac_enabled(iface))
			iface->radar_background.temp_ch = 1;
		return -1;
	}

	return hostapd_dfs_start_channel_switch_background(iface);
}


int hostapd_dfs_start_channel_switch(struct hostapd_iface *iface)
{
	struct hostapd_channel_data *channel;
	int secondary_channel;
	u8 oper_centr_freq_seg0_idx;
	u8 oper_centr_freq_seg1_idx;
	enum dfs_channel_type channel_type = DFS_AVAILABLE;
	u8 current_vht_oper_chwidth = hostapd_get_oper_chwidth(iface->conf);

	wpa_printf(MSG_DEBUG, "%s called (CAC active: %s, CSA active: %s)",
		   __func__, iface->cac_started ? "yes" : "no",
		   hostapd_csa_in_progress(iface) ? "yes" : "no");

	/* Check if CSA in progress */
	if (hostapd_csa_in_progress(iface))
		return 0;

	/* Check if active CAC */
	if (iface->cac_started)
		return hostapd_dfs_start_channel_switch_cac(iface);

	/*
	 * Allow selection of DFS channel in ETSI to comply with
	 * uniform spreading.
	 */
	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI)
		channel_type = DFS_ANY_CHANNEL;

	 if (iface->conf->dfs_test_mode)
		 return hostapd_dfs_testmode_set_beacon_csa(iface);

	/* Perform channel switch/CSA */
	channel = dfs_get_valid_channel(iface, &secondary_channel,
					&oper_centr_freq_seg0_idx,
					&oper_centr_freq_seg1_idx,
					channel_type);

	if (!channel) {
		/*
		 * If there is no channel to switch immediately to, check if
		 * there is another channel where we can switch even if it
		 * requires to perform a CAC first.
		 */
		channel_type = DFS_ANY_CHANNEL;
		channel = dfs_downgrade_bandwidth(iface, &secondary_channel,
						  &oper_centr_freq_seg0_idx,
						  &oper_centr_freq_seg1_idx,
						  &current_vht_oper_chwidth,
						  &channel_type);
		if (!channel) {
			/*
			 * Toggle interface state to enter DFS state
			 * until NOP is finished.
			 */
			hostapd_disable_iface(iface);
			hostapd_enable_iface(iface);
			return 0;
		}
	}

	return hostapd_dfs_request_channel_switch(iface, channel->chan,
						  channel->freq,
						  secondary_channel,
						  current_vht_oper_chwidth,
						  oper_centr_freq_seg0_idx,
						  oper_centr_freq_seg1_idx,
						  0);
}


/*convert common width(chan_width) to oper channel width*/
enum oper_chan_width convert_to_oper_chan_width(int chan_width)
{
	switch (chan_width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
	case CHAN_WIDTH_40:
		return CONF_OPER_CHWIDTH_USE_HT;
	case CHAN_WIDTH_80:
		return CONF_OPER_CHWIDTH_80MHZ;
	case CHAN_WIDTH_80P80:
		return CONF_OPER_CHWIDTH_80P80MHZ;
	case CHAN_WIDTH_160:
		return CONF_OPER_CHWIDTH_160MHZ;
	case CHAN_WIDTH_320:
		return CONF_OPER_CHWIDTH_320MHZ;
	}

	return CHAN_WIDTH_UNKNOWN;
}

static u32 hostapd_radar_bitmap_oper(int chan_width, int cf1, u16 radar_bitmap,
				     int chan_width_device, int cf_device)
{
	u16 radar_bitmap_oper;

	switch (chan_width_device) {
	case CHAN_WIDTH_40:
		if (cf1 < cf_device)
			radar_bitmap_oper = radar_bitmap & 0x1;
		else
			radar_bitmap_oper = (radar_bitmap >> 1) & 0x1;
		break;
	case CHAN_WIDTH_80:
		if (cf1 < cf_device)
			radar_bitmap_oper = radar_bitmap & 0x3;
		else
			radar_bitmap_oper = (radar_bitmap >> 2) & 0x3;
		break;
	case CHAN_WIDTH_160:
		if (cf1 < cf_device)
			radar_bitmap_oper = radar_bitmap & 0xF;
		else
			radar_bitmap_oper = (radar_bitmap >> 4) & 0xF;
		break;
	case CHAN_WIDTH_320:
		if (cf1 < cf_device)
			radar_bitmap_oper = radar_bitmap & 0xFF;
		else
			radar_bitmap_oper = (radar_bitmap >> 8) & 0xFF;
		break;
	default:
		return 0;
	}

	return radar_bitmap_oper;
}


/**
 * hostapd_dfs_get_radar_bitmap_oper() - Remap a radar bitmap onto the operating channel
 * @chan_width: Channel bandwidth reported with the radar event
 * @cf1: Center frequency reported with the radar event, in MHz
 * @radar_bitmap: Radar bitmap as reported by the driver
 * @chan_width_device: Channel bandwidth of the device
 * @cf_device: Center frequency of the device
 * @device_params_present: Set to %true if @chan_width_device/@cf_device differ
 *                          from @chan_width/@cf1
 *
 * Return: @radar_bitmap unchanged if the device channel parameters match the
 * reported ones, otherwise @radar_bitmap remapped onto the device's actual
 * subchannel bit positions.
 */
static u16 hostapd_dfs_get_radar_bitmap_oper(int chan_width, int cf1,
					     u16 radar_bitmap,
					     int chan_width_device,
					     int cf_device,
					     bool *device_params_present)
{
	*device_params_present = hostapd_is_device_params_present(chan_width,
								  cf1,
								  chan_width_device,
								  cf_device);
	if (*device_params_present)
		return hostapd_radar_bitmap_oper(chan_width, cf1, radar_bitmap,
						 chan_width_device, cf_device);

	return radar_bitmap;
}


/**
 * hostapd_dfs_radar_update_punct_bitmap() - Update puncture bitmap for a radar event
 * @iface: Pointer to hostapd interface
 * @radar_bitmap_oper: Radar bitmap remapped onto the operating channel's subchannels
 *
 * Return: %true if the radar hit a subchannel that was already punctured.
 * 	   %false if the puncture bitmap is different.
 */
static bool hostapd_dfs_radar_update_punct_bitmap(struct hostapd_iface *iface,
						   u16 radar_bitmap_oper)
{
	u16 radar_bit_pattern;

	if (!iface->conf->use_ru_puncture_dfs || !radar_bitmap_oper)
		return false;

	radar_bit_pattern = iface->radar_bit_pattern | iface->conf->punct_bitmap;
	/* Radar detected already punctured sub channel */
	if (radar_bitmap_oper && !(radar_bitmap_oper & ~radar_bit_pattern))
		return true;

	radar_bit_pattern |= radar_bitmap_oper;
	iface->conf->punct_bitmap = radar_bit_pattern;
	return false;
}


/**
 * hostapd_dfs_radar_handle_puncturing() - Try to handle a radar event by dfs puncturing
 * @iface: Pointer to hostapd interface
 * @freq: Primary channel frequency in MHz
 * @chan_width: Channel bandwidth
 * @radar_bitmap_oper: Radar bitmap
 * @cur_punct_bits: Puncture bitmap to restore on iface->conf before proceeding
 *
 * Punctures the affected subchannel(s) instead of switching to a new channel.
 *
 * Return: 0 on success and error on failure
 */
static int hostapd_dfs_radar_handle_puncturing(struct hostapd_iface *iface,
					       int freq, int chan_width,
					       u16 radar_bitmap_oper,
					       u16 cur_punct_bits)
{
	u8 oper_centr_freq_seg0_idx, oper_centr_freq_seg1_idx;

	iface->radar_bit_pattern = radar_bitmap_oper;
	iface->conf->punct_bitmap = cur_punct_bits;
	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: radar detected during CSA, deferring puncture channel switch");
		return 0;
	}

	if (iface->skip_mesh_dfs)
		return 0;

	oper_centr_freq_seg0_idx = iface->conf->vht_oper_centr_freq_seg0_idx;
	oper_centr_freq_seg1_idx = iface->conf->vht_oper_centr_freq_seg1_idx;
	chan_width = convert_to_oper_chan_width(chan_width);
#ifdef CONFIG_QCN_EXTN
	hostapd_get_oper_center_freq_seg_extn(iface->conf,
					      &oper_centr_freq_seg0_idx,
					      &oper_centr_freq_seg1_idx,
					      NULL);
#endif

	if (iface->cac_started) {
		wpa_printf(MSG_DEBUG, "radar detected during cac,"
			   "it restarted with valid puncturing bitmap :%d",
			   iface->conf->punct_bitmap | iface->radar_bit_pattern);

		iface->cac_started = 0;
#ifdef CONFIG_QCN_EXTN
		iface->iface_extn.cac_abort = 0;
#endif
		return hostapd_start_dfs_cac(iface, iface->conf->hw_mode,
					     iface->freq, iface->conf->channel,
					     iface->conf->ieee80211n,
					     iface->conf->ieee80211ac,
					     iface->conf->ieee80211ax,
					     iface->conf->ieee80211be,
					     iface->conf->ieee80211bn,
					     iface->conf->secondary_channel,
					     hostapd_get_oper_chwidth(iface->conf),
					     hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
					     hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
					     dfs_use_radar_background(iface),
					     iface->conf->bandwidth_device,
					     iface->conf->center_freq_device);
	}

	return hostapd_dfs_request_channel_switch(
		iface, iface->conf->channel, freq,
		iface->conf->secondary_channel, chan_width,
		oper_centr_freq_seg0_idx, oper_centr_freq_seg1_idx,
		hostapd_get_punct_bitmap(iface->bss[0]));
}


/**
 * hostapd_dfs_radar_reduce_bandwidth() - Try to switch to a reduced-bandwidth channel
 * @iface: Pointer to hostapd interface
 * @ret: Set to the reduced-bandwidth channel switch's return value, when found
 *
 * Looks for an available channel at a lower bandwidth than the current
 * operating bandwidth and, if found, issues a channel switch request to it.
 *
 * Return: %true  if a reduced-bandwidth channel was found and a switch was
 * 	          issued or skipped for mesh.
 * 	   %false if bandwidth reduction is disabled or no reduced-bandwidth
 * 	          channel is available.
 * 	   %ret   0 on successful channel siwtch, error on failure.
 */
static bool hostapd_dfs_radar_reduce_bandwidth(struct hostapd_iface *iface,
					       int *ret)
{
	struct hostapd_channel_data *channel;
	int secondary_channel;
	u8 oper_centr_freq_seg0_idx = 0;
	u8 oper_centr_freq_seg1_idx = 0;

	if (!iface->conf->dfs_bw_reduce_en)
		return false;

	if (iface->skip_mesh_dfs) {
		*ret = 0;
		return true;
	}

	channel = dfs_find_bw_reduced_channel(iface, &secondary_channel,
					      &oper_centr_freq_seg0_idx,
					      &oper_centr_freq_seg1_idx);
	if (!channel)
		return false;

	wpa_printf(MSG_INFO, "DFS: Radar detected, BW reduction successful - Ch %d",
		   channel->chan);
	*ret = hostapd_dfs_request_channel_switch(
		iface, channel->chan, channel->freq, secondary_channel,
		hostapd_get_oper_chwidth(iface->conf),
		oper_centr_freq_seg0_idx, oper_centr_freq_seg1_idx,
		hostapd_get_punct_bitmap(iface->bss[0]));
	return true;
}


/**
 * hostapd_dfs_radar_handle_disable_csa_dfs() - Defer radar channel switch handling
 * via a timeout
 * @iface: Pointer to hostapd interface
 *
 * Called when disable_csa_dfs is configured, so the channel switch is not
 * issued immediately from radar detection context. The AP will wait for
 * HAPD_DFS_RADAR_CH_SWITCH_WAIT_DUR duration.
 *
 * Return: %0 always.
 */
static int hostapd_dfs_radar_handle_disable_csa_dfs(struct hostapd_iface *iface)
{
	if (hostapd_csa_in_progress(iface)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: radar detected, but CSA already in progress - skip timeout");
		return 0;
	}

	if (!eloop_is_timeout_registered(hostapd_dfs_radar_handling_timeout,
					 iface, NULL))
		eloop_register_timeout(0, HAPD_DFS_RADAR_CH_SWITCH_WAIT_DUR,
				       hostapd_dfs_radar_handling_timeout,
				       iface, NULL);

	return 0;
}


int hostapd_dfs_radar_detected(struct hostapd_iface *iface, int freq,
			       int ht_enabled, int chan_offset, int chan_width,
			       int cf1, int cf2, u16 radar_bitmap,
			       int chan_width_device, int cf_device)
{
	int ret;
	u16 cur_punct_bits = iface->conf->punct_bitmap;
	u16 radar_bitmap_oper;
	bool device_params_present;
	bool is_dfs_puncture_en = iface->conf->use_ru_puncture_dfs;

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_RADAR_DETECTED
		"freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d radar_bitmap:%d"
		" chan_width_device=%d cf_device=%d cac_started=%d",
		freq, ht_enabled, chan_offset, chan_width, cf1, cf2, radar_bitmap,
		chan_width_device, cf_device, iface->cac_started);

	radar_bitmap_oper = hostapd_dfs_get_radar_bitmap_oper(chan_width, cf1,
							      radar_bitmap,
							      chan_width_device,
							      cf_device,
							      &device_params_present);
	if (is_dfs_puncture_en) {
		wpa_printf(MSG_DEBUG,
			   "DFS: Update puncture source for Radar puncture bitmap=0x%04x",
			   radar_bitmap_oper | iface->radar_bit_pattern);
		dfs_update_puncture_source(iface, cf1, chan_width,
					   radar_bitmap_oper | iface->conf->punct_bitmap,
					   DFS_CHAN_PUNC_RADAR);
	}

	iface->radar_detected = true;
	/* Proceed only if DFS is not offloaded to the driver */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return 0;

	if (!iface->conf->ieee80211h)
		return 0;

	/* mark radar frequency as invalid */
	if (device_params_present) {
		if (!set_dfs_state(iface, freq, ht_enabled, chan_offset,
				   chan_width_device, cf_device, cf2,
				   HOSTAPD_CHAN_DFS_UNAVAILABLE, radar_bitmap))
			return 0;
	} else {
		if (!set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width,
				   cf1, cf2, HOSTAPD_CHAN_DFS_UNAVAILABLE, radar_bitmap))
			return 0;
	}

	if (hostapd_dfs_radar_update_punct_bitmap(iface, radar_bitmap_oper))
		return 0;

	if (iface->conf->dfs_test_mode) {
		set_dfs_state(iface, freq, ht_enabled, chan_offset,
			      chan_width, cf1, cf2,
			      HOSTAPD_CHAN_DFS_AVAILABLE, radar_bitmap);
		return hostapd_dfs_start_channel_switch(iface);
	}

	if (!hostapd_dfs_is_background_event(iface, freq)) {
		/* Skip if reported radar event not overlapped our channels */
		if (!dfs_are_channels_overlapped(iface, freq, chan_width,
						 cf1, cf2)) {
			iface->conf->punct_bitmap = cur_punct_bits;
			return 0;
		}
	}

#ifdef CONFIG_QCN_EXTN
	hostapd_prepare_nol_ie_bmap_extn(iface, iface->conf->channel, freq,
			iface->conf->secondary_channel,
			convert_to_oper_chan_width(chan_width),
			cf1, cf2,
			hostapd_get_punct_bitmap(iface->bss[0]), radar_bitmap_oper);
#endif

	if (is_dfs_puncture_en) {
		if (hostapd_is_usable_punct_bitmap(iface))
			return hostapd_dfs_radar_handle_puncturing(iface, freq,
								   chan_width,
								   radar_bitmap_oper,
								   cur_punct_bits);

		dfs_reset_punc_bitmap_src(iface, ALL_SUBCHANS_PUNC);
	}

	/* Switch channel with random channel selection for invalid puncturing pattern */
	iface->radar_bit_pattern = 0;
	iface->conf->punct_bitmap = cur_punct_bits;

#ifdef CONFIG_QCN_EXTN
	iface->radar_bit_pattern_extn = radar_bitmap_oper;
#endif

	if (!hostapd_dfs_background_start_channel_switch(iface, freq))
		return 0;

	if (hostapd_dfs_radar_reduce_bandwidth(iface, &ret))
		return ret;

	/*
	 * radar_bitmap == 0 is reported for full-band radar events,
	 * so treat this as radar affecting the current operating
	 * bandwidth.
	 */
	if (!radar_bitmap && !iface->conf->disable_csa_dfs) {
		if (iface->skip_mesh_dfs)
			return 0;

		return hostapd_dfs_start_channel_switch(iface);
	}

	/* Radar detected on non-operating portion. No action needed. */
	if (radar_bitmap && !radar_bitmap_oper)
		return 0;

	if (iface->conf->disable_csa_dfs)
		return hostapd_dfs_radar_handle_disable_csa_dfs(iface);

	if (iface->skip_mesh_dfs)
		return 0;

	/* Radar detected while operating, switch the channel. */
	return hostapd_dfs_start_channel_switch(iface);
}

/*
 * rcac_update_background_state - Update radar_background state after RCAC start
 * @iface: Pointer to hostapd interface
 * @chan: Primary channel number
 * @freq: Primary channel frequency in MHz
 * @sec: Secondary channel direction (1=HT40+, -1=HT40-, 0=none)
 * @seg0: Center frequency segment 0 channel index
 * @seg1: Center frequency segment 1 channel index (0 for non-80P80)
 * @oper_width: Operating channel width enum
 */
static void rcac_update_background_state(struct hostapd_iface *iface,
					 int chan, int freq, int sec,
					 u8 seg0, u8 seg1,
					 enum oper_chan_width oper_width)
{
	iface->radar_background.channel = chan;
	iface->radar_background.freq = freq;
	iface->radar_background.secondary_channel = sec;
	iface->radar_background.centr_freq_seg0_idx = seg0;
	iface->radar_background.centr_freq_seg1_idx = seg1;
	iface->radar_background.chwidth = oper_width;
}

/*
 * hostapd_dfs_compute_bgcac_chan_params - Compute oper_width, seg0, sec for RCAC
 * @chan: Primary channel number
 * @bw_mhz: Bandwidth in MHz (20, 40, 80, 160)
 * @oper_width: Output: oper_chan_width enum
 * @seg0: Output: center frequency segment 0 channel index
 * @sec: Output: secondary channel direction (1=HT40+, -1=HT40-, 0=none)
 *
 * Returns 0 on success, -1 if bw_mhz is unsupported.
 */
static int hostapd_dfs_compute_bgcac_chan_params(int chan, int bw_mhz,
				    enum oper_chan_width *oper_width,
				    u8 *seg0, int *sec)
{
	int block_start;

	switch (bw_mhz) {
	case 20:
		*oper_width = CONF_OPER_CHWIDTH_USE_HT;
		*seg0 = chan;
		*sec = 0;
		break;
	case 40:
		*oper_width = CONF_OPER_CHWIDTH_USE_HT;
		/* Determine HT40+ or HT40- based on channel position in pair */
		if ((chan % 8) == 0 || (chan % 8) == 1) {
			/* Upper channel of pair (e.g. 40, 48, 56...) → HT40- */
			*seg0 = chan - 2;
			*sec = -1;
		} else {
			/* Lower channel of pair → HT40+ */
			*seg0 = chan + 2;
			*sec = 1;
		}
		break;
	case 80: {
		static const int allowed_80[] = { 36, 52, 100, 116, 132, 149, 165 };
		unsigned int k;
		bool valid = false;

		for (k = 0; k < ARRAY_SIZE(allowed_80); k++) {
			if (chan >= allowed_80[k] && chan < allowed_80[k] + 16) {
				block_start = allowed_80[k];
				valid = true;
				break;
			}
		}
		if (!valid) {
			wpa_printf(MSG_ERROR, "DFS: channel %d not in valid 80 MHz block", chan);
			return -1;
		}
		*oper_width = CONF_OPER_CHWIDTH_80MHZ;
		*seg0 = block_start + 6;
		*sec = ((chan - block_start) % 8 == 0) ? 1 : -1;
		break;
	}
	case 160: {
		static const int allowed_160[] = { 36, 100, 149 };
		unsigned int k;
		bool valid = false;

		for (k = 0; k < ARRAY_SIZE(allowed_160); k++) {
			if (chan >= allowed_160[k] && chan < allowed_160[k] + 32) {
				block_start = allowed_160[k];
				valid = true;
				break;
			}
		}
		if (!valid) {
			wpa_printf(MSG_ERROR,
				   "DFS: channel %d is not in a valid 160 MHz block",
				   chan);
			return -1;
		}
		*oper_width = CONF_OPER_CHWIDTH_160MHZ;
		*seg0 = block_start + 14;
		*sec = ((chan - block_start) % 8 == 0) ? 1 : -1;
		break;
	}
	default:
		wpa_printf(MSG_ERROR,
			   "DFS: unsupported bandwidth %d MHz for RCAC", bw_mhz);
		return -1;
	}

	return 0;
}

/*
 * rcac_block_has_dfs - Check if a channel block contains any DFS sub-channel
 * @mode: Hardware mode
 * @base_freq: Base frequency of the block
 * @bw_mhz: Bandwidth in MHz
 *
 * Returns true if at least one DFS sub-channel exists in the block.
 */
static bool rcac_block_has_dfs(struct hostapd_hw_modes *mode,
				int base_freq, int bw_mhz)
{
	int n_sub = bw_mhz / 20;
	int i;

	for (i = 0; i < n_sub; i++) {
		int f = base_freq + i * 20;
		struct hostapd_channel_data *sub = hw_mode_get_channel(mode, f, NULL);

		if (sub && (sub->flag & HOSTAPD_CHAN_RADAR))
			return true;
	}
	return false;
}
/**
 * rcac_block_has_nol - Check if any sub-channel in the block is in NOL
 * @mode: Hardware mode
 * @base_freq: Base frequency of the block (primary channel freq)
 * @bw_mhz: Bandwidth in MHz
 * @primary_chan: Primary channel number (for log message)
 *
 * Returns true if any DFS sub-channel is in NOL (DFS_UNAVAILABLE).
 */
static bool rcac_block_has_nol(struct hostapd_hw_modes *mode,
				int base_freq, int bw_mhz, int primary_chan)
{
	int n_sub = bw_mhz / 20;
	int s;

	for (s = 0; s < n_sub; s++) {
		int f = base_freq + s * 20;
		struct hostapd_channel_data *sub = hw_mode_get_channel(mode, f, NULL);

		if (!sub || (sub->flag & HOSTAPD_CHAN_DISABLED) ||
		    ((sub->flag & HOSTAPD_CHAN_RADAR) &&
		     (sub->flag & HOSTAPD_CHAN_DFS_MASK) == HOSTAPD_CHAN_DFS_UNAVAILABLE)) {
			wpa_printf(MSG_DEBUG,
				   "DFS: block %d/%d MHz has unusable sub-channel at %d MHz - rejecting block",
				   primary_chan, bw_mhz, f);
			return true;
		}
	}
	return false;
}

/**
 * hostapd_start_rcac_on_channel - Start background CAC on a specific channel
 * @iface: Pointer to hostapd interface
 * @chan: Channel number (e.g. 100 for 5500 MHz)
 * @bw_mhz: Bandwidth in MHz (20, 40, 80, 160)
 *
 * Starts background radar CAC (RCAC) on the given channel without using
 * EHT device-bandwidth parameters.  This is the correct path for non-EHT
 * APs that support the nl80211 radar-background flag.
 *
 * Returns 0 on success, -1 on failure.
 */
int hostapd_start_rcac_on_channel(struct hostapd_iface *iface, int chan,
				  int bw_mhz)
{
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *channel = NULL;
	int home_bw;
	int sec = 0;
	int block_base_freq;
	u8 seg0 = 0, seg1 = 0;
	int i;
	enum oper_chan_width oper_width;

	if (!iface || !iface->current_mode)
		return -1;

	if (!(iface->drv_flags2 & WPA_DRIVER_FLAGS2_RADAR_BACKGROUND)) {
		wpa_printf(MSG_ERROR,
			   "RCAC_DFS: driver does not support background radar");
		return -1;
	}

	if (!dfs_is_agile_cac_enabled(iface)) {
		wpa_printf(MSG_DEBUG,
			   "RCAC_DFS: QCA Agile CAC not enabled in driver; refusing RCAC start");
		return -1;
	}

	if (iface->radar_background.cac_started) {
		wpa_printf(MSG_DEBUG,
			   "RCAC_DFS: background CAC already in progress");
		return -1;
	}

	home_bw = channel_width_to_int(
		hostapd_get_chan_width_from_oper_chan_width(iface->conf));
	if (bw_mhz != home_bw && bw_mhz != home_bw / 2) {
		wpa_printf(MSG_ERROR,
			   "DFS: SET_RCAC_FREQ rejected - BW %d MHz is not compatible with home channel BW %d MHz (must be same or half)",
			   bw_mhz, home_bw);
		return -1;
	}

	mode = iface->current_mode;

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].chan == chan) {
			channel = &mode->channels[i];
			break;
		}
	}

	if (!channel || (channel->flag & HOSTAPD_CHAN_DISABLED)) {
		wpa_printf(MSG_ERROR,
			   "RCAC_DFS: channel %d not found or disabled", chan);
		return -1;
	}

	if ((channel->flag & HOSTAPD_CHAN_RADAR) &&
	    (channel->flag & HOSTAPD_CHAN_DFS_MASK) == HOSTAPD_CHAN_DFS_UNAVAILABLE) {
		wpa_printf(MSG_WARNING,
			   "RCAC_DFS: channel %d is in NOP (Non-Occupancy Period) - "
			   "falling back to RCS; will retry when NOP expires",
			   chan);
		return -1;
	}

	if (hostapd_dfs_compute_bgcac_chan_params(chan, bw_mhz, &oper_width, &seg0, &sec) < 0) {
		if (bw_mhz == 320) {
			wpa_printf(MSG_INFO,
				   "RCAC_DFS: 320 MHz not supported, retrying chan %d at 160 MHz",
				   chan);
			return hostapd_start_rcac_on_channel(iface, chan, 160);
		}
		return -1;
	}

	block_base_freq = (seg0 * 5 + 5000) - bw_mhz / 2 + 10;

	if (rcac_block_has_nol(mode, block_base_freq, bw_mhz, chan))
		return -1;

	wpa_printf(MSG_INFO,
		   "RCAC_DFS: starting background CAC on channel %d (%d MHz), bw=%d MHz, seg0=%d sec=%d",
		   chan, channel->freq, bw_mhz, seg0, sec);

	if (!(channel->flag & HOSTAPD_CHAN_RADAR) &&
	    !rcac_block_has_dfs(mode, block_base_freq, bw_mhz)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: channel %d block has no DFS sub-channels - "
			   "updating state only (no CAC needed)",
			   chan);
		goto set_state;
	}

	if ((channel->flag & HOSTAPD_CHAN_RADAR) &&
	    (channel->flag & HOSTAPD_CHAN_DFS_MASK) == HOSTAPD_CHAN_DFS_AVAILABLE) {
		wpa_printf(MSG_DEBUG,
			   "DFS: channel %d already DFS_AVAILABLE",
			   chan);
		goto set_state;
	}

	hostapd_stop_background_cac(iface->bss[0]);

	if (hostapd_start_dfs_cac(iface, iface->conf->hw_mode,
				  channel->freq, chan,
				  iface->conf->ieee80211n,
				  iface->conf->ieee80211ac,
				  iface->conf->ieee80211ax,
				  iface->conf->ieee80211be,
				  iface->conf->ieee80211bn,
				  sec, oper_width, seg0, seg1,
				  true, 0, 0)) {
		wpa_printf(MSG_ERROR,
			   "RCAC_DFS: failed to start background CAC on channel %d",
			   chan);
		iface->radar_background.cac_started = 0;
		iface->radar_background.channel = -1;
		return -1;
	}

set_state:

	rcac_update_background_state(iface, chan, channel->freq,
				     sec, seg0, seg1, oper_width);

	return 0;
}


void hostapd_start_device_cac_background(struct hostapd_iface *iface)
{
	int width, start_chan, start_chan_idx = -1, n_chans = 1, i;
	struct hostapd_channel_data *chan;
	struct hostapd_hw_modes *mode;
	bool res = false;
	u8 seg0;

	if (!iface->conf->ieee80211be ||
	    !(iface->drv_flags2 & WPA_DRIVER_FLAGS2_RADAR_BACKGROUND) ||
	    iface->radar_background.cac_started)
		return;

	width = hostapd_get_oper_chwidth(iface->conf);
	seg0 = hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
	if (!hostapd_is_device_params_present(width, seg0,
					      iface->conf->bandwidth_device,
					      iface->conf->center_freq_device))
		return;

	start_chan = seg0;

	switch (width) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (iface->conf->secondary_channel) {
			start_chan = seg0 - 2;
			n_chans = 2;
		}
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		start_chan = seg0 - 6;
		n_chans = 4;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		start_chan = seg0 - 14;
		n_chans = 8;
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		start_chan = seg0 - 30;
		n_chans = 16;
		break;
	default:
		return;
	}

	if (iface->conf->center_freq_device <
	    ieee80211_chan_to_freq(NULL, iface->conf->op_class, seg0))
		start_chan = start_chan - (4 * n_chans);
	else
		start_chan = start_chan + (4 * n_chans);

	mode = iface->current_mode;
	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];
		if (chan->chan == start_chan) {
			start_chan_idx = i;
			break;
		}
	}

	if (start_chan_idx == -1)
		return;

	for (i = 0; i < n_chans; i++) {
		chan = &mode->channels[start_chan_idx + i];

		if (!(chan->flag & HOSTAPD_CHAN_RADAR))
			continue;

		if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) == HOSTAPD_CHAN_DFS_AVAILABLE)
			continue;
		else if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) == HOSTAPD_CHAN_DFS_USABLE)
			res = true;
		else
			return;
	}

	if (res == false)
		return;

	hostapd_start_dfs_cac(iface, iface->conf->hw_mode, iface->freq,
			      iface->conf->channel, iface->conf->ieee80211n,
			      iface->conf->ieee80211ac, iface->conf->ieee80211ax,
			      iface->conf->ieee80211be,
			      iface->conf->ieee80211bn,
			      iface->conf->secondary_channel,
			      hostapd_get_oper_chwidth(iface->conf),
			      hostapd_get_oper_centr_freq_seg0_idx(iface->conf),
			      hostapd_get_oper_centr_freq_seg1_idx(iface->conf),
			      true,
			      iface->conf->bandwidth_device,
			      iface->conf->center_freq_device);

	iface->radar_background.channel = iface->conf->channel;
	iface->radar_background.secondary_channel =
		iface->conf->secondary_channel;
	iface->radar_background.freq = iface->freq;
	iface->radar_background.centr_freq_seg0_idx =
		hostapd_get_oper_centr_freq_seg0_idx(iface->conf);
	iface->radar_background.centr_freq_seg1_idx =
		hostapd_get_oper_centr_freq_seg1_idx(iface->conf);
}


/**
 * hostapd_start_background_cac - Runtime entry point to start Agile CAC.
 *
 * Called when bgcac_en is enabled at runtime (SET bgcac_en 1) or via the
 * explicit BGCAC_START command.
 *
 * Returns 0 on success (or if CAC is already running), -1 if background
 * radar is not supported or bgcac_en is not set.
 */
int hostapd_start_background_cac(struct hostapd_iface *iface)
{
	u8 op_class, channel;

	if (!iface || !iface->conf)
		return -1;

	if (!iface->conf->bgcac_en || !iface->conf->enable_background_radar) {
		wpa_printf(MSG_DEBUG,
			   "DFS: hostapd_start_background_cac: bgcac_en not set");
		return -1;
	}

	if (!dfs_use_radar_background(iface)) {
		wpa_printf(MSG_DEBUG,
			   "DFS: hostapd_start_background_cac: background radar not supported");
		return -1;
	}

	if (iface->radar_background.cac_started) {
		wpa_printf(MSG_DEBUG,
			   "DFS: hostapd_start_background_cac: already running");
		return 0;
	}

	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI) {
		wpa_printf(MSG_INFO,
					"DFS: ETSI domain, bgcac_en set - starting PreCAC");
		iface->radar_background.channel = -1;
		iface->radar_background.freq = 0;
		iface->radar_background.cac_started = 0;

		return hostapd_dfs_start_precac(iface);
	}

	if (iface->conf->rcac_freq > 0) {
		if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI) {
			wpa_printf(MSG_WARNING,
				   "DFS: rcac_freq configured but domain is ETSI - ignoring");
		} else {
			if (ieee80211_freq_to_channel_ext(iface->conf->rcac_freq,
							  0,
							  CONF_OPER_CHWIDTH_USE_HT,
							  &op_class,
							  &channel) == NUM_HOSTAPD_MODES) {
				wpa_printf(MSG_WARNING,
					   "DFS: rcac_freq=%d is invalid - ignoring",
					   iface->conf->rcac_freq);
			} else {
				wpa_printf(MSG_INFO,
					   "DFS: rcac_freq=%d configured, setting user_rcac_channel=%d",
					   iface->conf->rcac_freq, channel);
				iface->user_rcac_channel = channel;
			}
		}
	}

	wpa_printf(MSG_INFO, "DFS: starting Agile CAC (bgcac_en=1)");

	hostapd_dfs_update_background_chain(iface);

	return iface->radar_background.cac_started ? 0 : -1;
}


int hostapd_dfs_nop_finished(struct hostapd_iface *iface, int freq,
			     int ht_enabled, int chan_offset, int chan_width,
			     int cf1, int cf2,
			     int chan_width_device, int cf_device)
{
	struct hostapd_channel_data *chan;

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_NOP_FINISHED
		"freq=%d ht_enabled=%d chan_offset=%d chan_width=%d cf1=%d cf2=%d chan_width_device=%d cf_device=%d",
		freq, ht_enabled, chan_offset, chan_width, cf1, cf2, chan_width_device, cf_device);

	/* Proceed only if DFS is not offloaded to the driver */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return 0;

	if (hostapd_is_device_params_present(chan_width, cf1,
					     chan_width_device, cf_device))
		set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width_device,
			      cf_device, cf2, HOSTAPD_CHAN_DFS_USABLE, 0);
	else
		set_dfs_state(iface, freq, ht_enabled, chan_offset, chan_width,
			      cf1, cf2, HOSTAPD_CHAN_DFS_USABLE,0);

	chan = hw_mode_get_channel(iface->current_mode, freq, NULL);
	if (chan && chan->puncture_source != DFS_CHAN_PUNC_NONE) {
		wpa_printf(MSG_DEBUG,
			   "DFS: Ignore NOL expiry on %d MHz as the chan is punctured, Source = %d",
			   freq, chan->puncture_source);
		return 0;
	}

	if (iface->state == HAPD_IFACE_DFS && !iface->cac_started) {
		/* Handle cases where all channels were initially unavailable */
#ifdef CONFIG_QCN_EXTN
		if (!iface->conf->conf_extn.autorecovery_after_nol_vapdown) {
			wpa_msg(iface->bss[0]->msg_ctx, MSG_DEBUG,
				"autorecovery_after_nol_vapdown disabled, skipping DFS recovery");
			return 0;
		}
#endif
		hostapd_handle_dfs(iface);
	} else if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI &&
		   iface->conf->bgcac_en &&
		   !iface->radar_background.cac_started) {
		wpa_printf(MSG_INFO,
			   "PRECAC_NOP expired - resuming PRECAC on newly available channel");
		hostapd_dfs_start_precac(iface);
	} else if (dfs_use_radar_background(iface) &&
			iface->radar_background.channel == -1) {
		/* Reset radar background chain if disabled */
		hostapd_dfs_update_background_chain(iface);
	} else {
		hostapd_start_device_cac_background(iface);
	}

	return 0;
}


int hostapd_is_dfs_required(struct hostapd_iface *iface)
{
	int n_chans, n_chans1, start_chan_idx, start_chan_idx1, res;
	int chan_width = hostapd_get_oper_chwidth(iface->conf);

	if ((!(iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD) &&
	     !iface->conf->ieee80211h) ||
	    !iface->current_mode ||
	    iface->current_mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	/* Get start (first) channel for current configuration */
	start_chan_idx = dfs_get_start_chan_idx(iface, &start_chan_idx1,
						chan_width,
						iface->conf->channel, false);
	if (start_chan_idx == -1)
		return -1;

	/* Get number of used channels, depend on width */
	n_chans = dfs_get_used_n_chans(iface, &n_chans1, chan_width);

	/* Check if any of configured channels require DFS */
	res = dfs_check_chans_radar(iface, start_chan_idx, n_chans);
	if (res)
		return res;
	if (start_chan_idx1 >= 0 && n_chans1 > 0)
		res = dfs_check_chans_radar(iface, start_chan_idx1, n_chans1);
	return res;
}


int hostapd_dfs_start_cac(struct hostapd_iface *iface, int freq,
			  int ht_enabled, int chan_offset, int chan_width,
			  int cf1, int cf2, bool is_background,
			  int chan_width_device, int cf_device)
{
	int n_chans, n_chans1, ch_idx, ch_idx_1, dfs_cac_ms;
	int chwidth;
	u8 channel_no, cf1_ch_no;
	bool is_background_event = hostapd_dfs_is_background_event(iface, freq);

	if (is_background || is_background_event) {
		iface->radar_background.cac_started = 1;
	} else {
		/* This is called when the driver indicates that an offloaded
		 * DFS has started CAC. radar_detected might be set for previous
		 * DFS channel. Clear it for this new CAC process. */
		hostapd_set_state(iface, HAPD_IFACE_DFS);
		iface->cac_started = 1;
#ifdef CONFIG_QCN_EXTN
		iface->iface_extn.cac_abort = 0;
#endif

		/* Clear radar_detected in case it is for the previous
		 * frequency. Also remove disabled link's information in RNR
		 * element from other links. */
		iface->radar_detected = false;
		if (iface->interfaces && iface->interfaces->count > 1)
			ieee802_11_set_beacons(iface);
	}

	/* Get channel number */
	ieee80211_freq_to_chan(freq, &channel_no);

	/* Get seq1 channel number */
	ieee80211_freq_to_chan(cf1, &cf1_ch_no);

	switch (chan_width) {
	case CHAN_WIDTH_80:
		chwidth = CONF_OPER_CHWIDTH_80MHZ;
		cf1_ch_no -= 6;
		break;
	case CHAN_WIDTH_80P80:
		chwidth = CONF_OPER_CHWIDTH_80P80MHZ;
		cf1_ch_no -= 6;
		break;
	case CHAN_WIDTH_160:
		chwidth = CONF_OPER_CHWIDTH_160MHZ;
		cf1_ch_no -= 14;
		break;
	case CHAN_WIDTH_320:
		chwidth = CONF_OPER_CHWIDTH_320MHZ;
		cf1_ch_no -= 30;
		break;
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
	case CHAN_WIDTH_40:
	default:
		chwidth = CONF_OPER_CHWIDTH_USE_HT;
		break;
	}

	/* Get idx */
	ch_idx = dfs_get_start_chan_idx(iface,
					&ch_idx_1, chwidth,
					(chwidth == CHANWIDTH_USE_HT) ? channel_no :
					cf1_ch_no, true);
	if (ch_idx == -1)
		return -1;

	/* Get number of used channels, depend on width */
	n_chans = dfs_get_used_n_chans(iface, &n_chans1,
				       chwidth);
	if (n_chans == -1)
		return -1;

	dfs_cac_ms = dfs_get_cac_time(iface, ch_idx, n_chans);
	/* Set minimum cac millisecond it it's not configured for
	 * the given channel from driver.
	 */
	if (!dfs_cac_ms) {
		/* TODO: How to check CAC time for ETSI weather channels? */
		dfs_cac_ms = IEEE80211_DFS_MIN_CAC_TIME_MS;
	}

	/* Save dfs cac time to current iface configonly when DFS
	 * is offloaded and cac event is not a background event
	 */
	if (iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD &&
	    !is_background_event)
		iface->dfs_cac_ms = dfs_cac_ms;

	wpa_msg(iface->bss[0]->msg_ctx, MSG_INFO, DFS_EVENT_CAC_START
		"freq=%d chan=%d chan_offset=%d width=%d seg0=%d "
		"seg1=%d cac_time=%ds chan_width_device=%d cf_device=%d%s",
		freq, (freq - 5000) / 5, chan_offset, chan_width, cf1, cf2,
		dfs_cac_ms / 1000, chan_width_device, cf_device,
		(is_background || hostapd_dfs_is_background_event(iface, freq)) ?
		" (background)" : "");

	if (is_background || is_background_event)
		os_get_reltime(&iface->radar_background.dfs_cac_start);
	else
		os_get_reltime(&iface->dfs_cac_start);

	return 0;
}


/*
 * Main DFS handler for offloaded case.
 * 2 - continue channel/AP setup for non-DFS channel
 * 1 - continue channel/AP setup for DFS channel
 * 0 - channel/AP setup will be continued after CAC
 * -1 - hit critical error
 */
int hostapd_handle_dfs_offload(struct hostapd_iface *iface)
{
	int dfs_res;

	wpa_printf(MSG_DEBUG, "%s: iface->cac_started: %d",
		   __func__, iface->cac_started);

	/*
	 * If DFS has already been started, then we are being called from a
	 * callback to continue AP/channel setup. Reset the CAC start flag and
	 * return.
	 */
	if (iface->cac_started) {
		wpa_printf(MSG_DEBUG, "%s: iface->cac_started: %d",
			   __func__, iface->cac_started);
		iface->cac_started = 0;
		return 1;
	}

	dfs_res = hostapd_is_dfs_required(iface);
	if (dfs_res > 0) {
		wpa_printf(MSG_DEBUG,
			   "%s: freq %d MHz requires DFS for %d chans",
			   __func__, iface->freq, dfs_res);
		return 0;
	}

	wpa_printf(MSG_DEBUG,
		   "%s: freq %d MHz does not require DFS. Continue channel/AP setup",
		   __func__, iface->freq);
	return 2;
}


int hostapd_is_dfs_overlap(struct hostapd_iface *iface, enum chan_width width,
			   int center_freq)
{
	struct hostapd_channel_data *chan;
	struct hostapd_hw_modes *mode = iface->current_mode;
	int half_width;
	int res = 0;
	int i;

	if (!iface->conf->ieee80211h || !mode ||
	    mode->mode != HOSTAPD_MODE_IEEE80211A)
		return 0;

	switch (width) {
	case CHAN_WIDTH_20_NOHT:
	case CHAN_WIDTH_20:
		half_width = 10;
		break;
	case CHAN_WIDTH_40:
		half_width = 20;
		break;
	case CHAN_WIDTH_80:
	case CHAN_WIDTH_80P80:
		half_width = 40;
		break;
	case CHAN_WIDTH_160:
		half_width = 80;
		break;
	default:
#ifdef CONFIG_QCN_EXTN
		half_width = hostapd_get_dfs_half_chwidth_extn(width);
#endif /* CONFIG_QCN_EXTN */
		if (half_width)
			break;

		wpa_printf(MSG_WARNING, "DFS chanwidth %d not supported",
			   width);
		return 0;
	}

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		if (!(chan->flag & HOSTAPD_CHAN_RADAR))
			continue;

		if ((chan->flag & HOSTAPD_CHAN_DFS_MASK) ==
		    HOSTAPD_CHAN_DFS_AVAILABLE)
			continue;

		if (center_freq - chan->freq < half_width &&
		    chan->freq - center_freq < half_width)
			res++;
	}

	wpa_printf(MSG_DEBUG, "DFS CAC required: (%d, %d): in range: %s",
		   center_freq - half_width, center_freq + half_width,
		   res ? "yes" : "no");

	return res;
}

/*
 * Determine whether CAC is still required for the ACS-selected/configured
 * channel/bandwidth, using DFS helpers similar to hostapd_handle_dfs().
 *
 * Returns true only if DFS is enabled and at least one of the channels
 * in the configured bandwidth is not DFS_AVAILABLE, i.e., CAC not yet
 * completed (and not skipped via background/pre-CAC).
 */
bool hostapd_is_cac_required(struct hostapd_iface *iface)
{
	int chan_width;
	int n_chans, n_chans1, seg1;
	int start_chan_idx;
	int res;

	/* If DFS is not enabled at all, CAC is never required. */
	if (!iface->current_mode || !iface->conf)
		return false;

	if (!(iface->drv_flags & WPA_DRIVER_FLAGS_RADAR))
		return false;

	if (!iface->conf->ieee80211h)
		return false;

	chan_width = hostapd_get_oper_chwidth(iface->conf);

	/* Get number of used 20 MHz channels based on width */
	n_chans = dfs_get_used_n_chans(iface, &seg1, chan_width);

	/* Get starting channel index for current config */
	start_chan_idx = dfs_get_start_chan_idx(iface, &n_chans1, chan_width,
                                                iface->conf->channel, false);
	if (start_chan_idx < 0)
		return false;

	/*
	 * dfs_check_chans_available():
	 * - returns non-zero if ALL channels in [start_chan_idx, n_chans)
	 *   are DFS_AVAILABLE
	 * - returns 0 if some channel requires CAC.
	 */
	res = dfs_check_chans_available(iface, start_chan_idx, n_chans);
	if (res) {
		wpa_printf(MSG_DEBUG, "DFS: channels in the configured bw"
							" are DFS_AVAILABLE");
	} else {
		wpa_printf(MSG_DEBUG, "DFS: channels in the configured bw"
							" required CAC");
	}

	return !res;
}

static bool dfs_is_subchan_punctured(int chan_freq, u16 punct_bitmap,
				     int center_freq, int half_width)
{
	int start_freq;
	int chan_bit_pos;

	if (!punct_bitmap)
		return false;

	start_freq = center_freq - half_width;
	chan_bit_pos = (chan_freq - start_freq) / 20;
	if (chan_bit_pos < 0 || chan_bit_pos >= 16)
		return false;

	return !!(punct_bitmap & BIT(chan_bit_pos));
}

static bool dfs_has_unavailable_channel(struct hostapd_iface *iface,
					int start_chan_idx,
					int n_chans,
					u16 punct_bitmap,
					int center_freq,
					int half_width)
{
	struct hostapd_channel_data *channel;
	struct hostapd_hw_modes *mode;
	int i;

	mode = iface->current_mode;

	for (i = 0; i < n_chans; i++) {
		channel = &mode->channels[start_chan_idx + i];
		if (dfs_is_subchan_punctured(channel->freq, punct_bitmap,
					     center_freq, half_width))
			continue;
		if ((channel->flag & HOSTAPD_CHAN_DFS_MASK) ==
				HOSTAPD_CHAN_DFS_UNAVAILABLE)
			return true;
	}

	return false;
}

#ifdef CONFIG_QCN_EXTN
struct hostapd_channel_data *
dfs_downgrade_bandwidth_helper(struct hostapd_iface *iface, int *secondary_channel,
			       u8 *oper_centr_freq_seg0_idx,
			       u8 *oper_centr_freq_seg1_idx,
			       u8 *oper_chwidth,
			       int *channel_type)
{
	enum dfs_channel_type type = *channel_type;
	struct hostapd_channel_data *channel;

	channel = dfs_downgrade_bandwidth(iface, secondary_channel,
					  oper_centr_freq_seg0_idx,
					  oper_centr_freq_seg1_idx,
					  oper_chwidth,
					  &type);
	*channel_type = type;

	return channel;
}

struct hostapd_channel_data *
dfs_get_valid_channel_helper(struct hostapd_iface *iface,
			     int *secondary_channel,
			     u8 *oper_centr_freq_seg0_idx,
			     u8 *oper_centr_freq_seg1_idx,
			     int type)
{
	return dfs_get_valid_channel(iface, secondary_channel,
				     oper_centr_freq_seg0_idx, oper_centr_freq_seg1_idx,
				     (enum dfs_channel_type) type);
}

int hostapd_dfs_start_channel_switch_cac_helper(struct hostapd_iface *iface)
{
	return hostapd_dfs_start_channel_switch_cac(iface);
}
#endif
/**
 * hostapd_restart_agile_cac_after_ch_switch - Restart Agile CAC after home
 * channel change.
 *@iface: Pointer to hostapd interface
 *
 * Resets background radar state and clears the  user-pinned RCAC channel
 * if it matches the new home channel.  Then restarts RCAC channel selection
 * via hostapd_dfs_update_background_chain().
 * No-op if agile CAC is not enabled.
 */
void hostapd_restart_agile_cac_after_ch_switch(struct hostapd_iface *iface)
{
	if (!iface || !dfs_is_agile_cac_enabled(iface))
		return;

	iface->radar_background.cac_started = 0;
	iface->radar_background.channel = -1;
	iface->radar_background.freq = 0;

	if (iface->user_rcac_channel == iface->conf->channel) {
		wpa_printf(MSG_DEBUG,
			   "DFS: user-pinned RCAC channel %d is now home - clearing pin",
			   iface->user_rcac_channel);
		iface->user_rcac_channel = 0;
	}

	if (iface->dfs_domain == HOSTAPD_DFS_REGION_ETSI &&
	    iface->conf->bgcac_en) {
		wpa_printf(MSG_DEBUG,
			   "DFS: ETSI domain - starting PreCAC after channel switch");
		hostapd_dfs_start_precac(iface);
		return;
	}

	hostapd_dfs_update_background_chain(iface);
}


/**
 * hostapd_abort_background_cac - Abort an ongoing background CAC
 * @iface: Pointer to hostapd interface data
 *
 * Aborts the ongoing background CAC by sending NL80211_CMD_STOP_BGRADAR_DETECT
 * directly to the driver via hostapd_stop_background_cac().
 */
void hostapd_abort_background_cac(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd;

	if (!iface)
		return;

	if (!iface->radar_background.cac_started &&
			iface->radar_background.freq <= 0)
		return;

	hapd = iface->bss[0];

	wpa_printf(MSG_DEBUG,
		   "DFS: Aborting background CAC on channel %d (%d MHz)",
		   iface->radar_background.channel,
		   iface->radar_background.freq);

	if (hostapd_stop_background_cac(hapd) != 0)
		wpa_printf(MSG_WARNING,
			   "DFS: Failed to abort background CAC");

	iface->radar_background.cac_started = 0;
	iface->radar_background.channel = -1;
	iface->radar_background.freq = 0;
}


bool hostapd_dfs_csa_target_has_unavailable_channel(struct hostapd_iface *iface,
						    struct hostapd_freq_params *freq_params,
						    enum chan_width width)
{
	int start_freq;
	int n_chans;
	int half_width;
	u8 start_chan_num;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *chan;

	if (!iface)
		return true;

	mode = iface->current_mode;

	switch (width) {
	case CHAN_WIDTH_40:
		n_chans = 2;
		half_width = 20;
		start_freq = freq_params->center_freq1 - 10;
		break;
	case CHAN_WIDTH_80:
		n_chans = 4;
		half_width = 40;
		start_freq = freq_params->center_freq1 - 30;
		break;
	case CHAN_WIDTH_80P80:
		n_chans = 4;
		half_width = 40;
		start_freq = freq_params->center_freq1 - 30;
		break;
	case CHAN_WIDTH_160:
		n_chans = 8;
		half_width = 80;
		start_freq = freq_params->center_freq1 - 70;
		break;
	case CHAN_WIDTH_320:
		n_chans = 16;
		half_width = 160;
		start_freq = freq_params->center_freq1 - 150;
		break;
	default:
		n_chans = 1;
		half_width = 10;
		start_freq = freq_params->freq;
		break;
	}

	if (ieee80211_freq_to_chan(start_freq, &start_chan_num) ==
			NUM_HOSTAPD_MODES)
		return true;

	chan = hw_get_channel_chan(mode, start_chan_num, NULL);
	if (!chan)
		return true;

	if (dfs_has_unavailable_channel(iface, chan - mode->channels, n_chans,
					freq_params->punct_bitmap,
					freq_params->center_freq1 ?
					freq_params->center_freq1 : freq_params->freq,
					half_width)) {
		wpa_printf(MSG_DEBUG, "DFS: CSA target includes NOL channel(s) (pri)");
		return true;
	}

	if (width == CHAN_WIDTH_80P80 && freq_params->center_freq2) {
		start_freq = freq_params->center_freq2 - 30;
		if (ieee80211_freq_to_chan(start_freq, &start_chan_num) ==
				NUM_HOSTAPD_MODES)
			return true;

		chan = hw_get_channel_chan(mode, start_chan_num, NULL);
		if (!chan)
			return true;

		if (dfs_has_unavailable_channel(iface, chan - mode->channels,
						n_chans,
						freq_params->punct_bitmap,
						freq_params->center_freq2,
						half_width)) {
			wpa_printf(MSG_DEBUG, "DFS: CSA target includes NOL channel(s) (seg1)");
			return true;
		}
	}

	return false;
}

u32 hostapd_get_remaining_cac_tu(struct hostapd_iface *iface)
{
	struct os_reltime age;
	u32 switch_time;
	u32 left_ms;
	u32 elapsed_ms;

	os_reltime_age(&iface->dfs_cac_start, &age);
	elapsed_ms = age.sec * 1000 + age.usec / 1000;
	if (elapsed_ms < iface->dfs_cac_ms)
		left_ms = iface->dfs_cac_ms - elapsed_ms;
	else
		left_ms = 0;

	switch_time = USEC_TO_TU(left_ms * 1000);
	if (switch_time > 0xFFFFFF)
		switch_time = 0xFFFFFF;

	wpa_printf(MSG_DEBUG,
		   "MLD: MCST : freq=%d dfs_cac_ms=%u elapsed_ms=%u left_ms=%u switch_time(TU)=%u",
		   iface->freq, iface->dfs_cac_ms, elapsed_ms, left_ms,
		   switch_time);

	return switch_time;
}
