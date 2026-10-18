/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: reg_channel.h
 * This file provides prototypes of the channel list APIs in addition to
 * predefined macros.
 */

#ifndef __REG_CHANNEL_H_
#define __REG_CHANNEL_H_

#include <reg_db.h>
#include <wlan_reg_channel_api.h>

#define NEXT_20_CH_OFFSET 20
#define SP_AP_AND_CLIENT_POWER_DIFF_IN_DBM 6

#ifdef CONFIG_HOST_FIND_CHAN

/**
 * reg_is_phymode_chwidth_allowed() - Check if requested phymode is allowed
 * @pdev_priv_obj: Pointer to regulatory pdev private object.
 * @phy_in: phymode that the user requested.
 * @ch_width: Channel width that the user requested.
 * @primary_freq: Input primary frequency.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @input_puncture_bitmap: Input puncture bitmap
 *
 * Return: true if phymode is allowed, else false.
 */
bool reg_is_phymode_chwidth_allowed(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		enum reg_phymode phy_in,
		enum phy_ch_width ch_width,
		qdf_freq_t primary_freq,
		enum supported_6g_pwr_types in_6g_pwr_mode,
		uint16_t input_puncture_bitmap);

/**
 * reg_get_max_channel_width() - Get the maximum channel width supported
 * given a frequency and a global maximum channel width.
 * @pdev: Pointer to PDEV object.
 * @freq: Input frequency.
 * @g_max_width: Global maximum channel width.
 * @input_puncture_bitmap: Input puncture bitmap
 *
 * Return: Maximum channel width of type phy_ch_width.
 */
enum phy_ch_width
reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum phy_ch_width g_max_width,
			  enum supported_6g_pwr_types in_6g_pwr_mode,
			  uint16_t input_puncture_bitmap);

/**
 * reg_set_chan_blocked() - Set is_chan_hop_blocked to true for a frequency
 * in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 *
 * Return: void.
 */
void reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_is_chan_blocked() - Check if is_chan_hop_blocked to true for a
 * frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 *
 * Return: true if is_chan_hop_blocked is true for the input frequency, else
 * false.
 */
bool reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * reg_is_chan_blocked() - Clear is_chan_hop_blocked for channel in the
 * current chan list.
 * @pdev: Pointer to pdev.
 *
 * Return: void.
 */
void reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev);

/**
 * reg_set_chan_ht40intol() - Set ht40intol_flags to the value for a
 * frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: ht40intol_flags to be set.
 *
 * Return: void.
 */
void reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			    enum ht40_intol ht40intol_flags);

/**
 * reg_clear_chan_ht40intol() - Clear the ht40intol_flags from the
 * regulatory channel corresponding to the frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: ht40intol_flags to be cleared.
 *
 * Return: void.
 */
void reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			      enum ht40_intol ht40intol_flags);

/**
 * reg_is_chan_ht40intol() - Check if the ht40intol flag is set to the
 * given enum for a frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: The ht40intol flag (plus/minus) to check.
 *
 * Return: true if is_chan_htintol is set to given value for the input
 * frequency, else false.
 */
bool reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			   enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_clear_allchan_ht40intol() - Clear ht40intol_flags for all channels
 * in the current chan list.
 * @pdev: Pointer to pdev.
 *
 * Return: void.
 */
void reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev);

/*
 * reg_is_band_present() - Check if input band channels are present
 * in the regulatory current channel list.
 * @pdev: pdev pointer.
 * @reg_band: regulatory band.
 *
 */
bool reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			 enum reg_wifi_band reg_band);
#else
static inline bool reg_is_phymode_chwidth_allowed(
		struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj,
		enum reg_phymode phy_in,
		enum phy_ch_width ch_width,
		qdf_freq_t primary_freq,
		uint16_t input_puncture_bitmap)
{
	return false;
}

static inline enum phy_ch_width
reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum phy_ch_width g_max_width,
			  enum supported_6g_pwr_types in_6g_pwr_mode,
			  uint16_t input_puncture_bitmap);
{
	return CH_WIDTH_INVALID;
}

static inline
void reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
}

static inline
bool reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return false;
}

static inline void reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev)
{
}

static inline void
reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
		       enum ht40_intol ht40intol_flags)
{
}

	static inline void
reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			    enum ht40_intol ht40intol_flags)
{
}

static inline bool
reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
		      enum ht40_intol ht40intol_flags)
{
	return false;
}

static inline void
reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev)
{
}

static inline bool
reg_is_band_present(struct wlan_objmgr_pdev *pdev, enum reg_wifi_band reg_band)
{
	return false;
}
#endif /* CONFIG_HOST_FIND_CHAN */

/**
 * reg_is_nol_for_freq () - Checks the channel is a nol channel or not
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 *
 * Return: true if nol channel else false.
 */
bool reg_is_nol_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * reg_is_nol_hist_for_freq () - Checks the channel is a nol hist channel or not
 * @pdev: pdev ptr
 * @freq: Channel center frequency
 *
 * Return: true if nol channel else false.
 */
bool reg_is_nol_hist_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * reg_get_ap_chan_list() - Get the AP master channel list
 * @pdev     : Pointer to pdev
 * @chan_list: Pointer to the channel list
 * @get_cur_chan_list: Flag to pull the current channel list
 * @ap_pwr_type: Type of AP power for 6GHz
 *
 * NOTE: If get_cur_chan_list is true, then ap_pwr_type is ignored.
 *
 * Return:
 * QDF_STATUS_SUCCESS: Success
 * QDF_STATUS_E_INVAL: Failed to get channel list
 */
QDF_STATUS reg_get_ap_chan_list(struct wlan_objmgr_pdev *pdev,
				struct regulatory_channel *chan_list,
				bool get_cur_chan_list,
				enum reg_6g_ap_type ap_pwr_type);

/**
 * reg_is_freq_width_dfs()- Check if a channel is DFS, given the channel
 * frequency and width combination.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @ch_width: Channel Width.
 *
 * Return: True if frequency + width has DFS subchannels, else false.
 */
bool reg_is_freq_width_dfs(struct wlan_objmgr_pdev *pdev,
			   qdf_freq_t freq,
			   enum phy_ch_width ch_width);

/**
 * reg_get_channel_params () - Sets channel parameteres for given
 * bandwidth
 * @pdev: Pointer to pdev
 * @freq: Channel center frequency.
 * @sec_ch_2g_freq: Secondary 2G channel frequency
 * @ch_params: pointer to the channel parameters.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: None
 */
void reg_get_channel_params(struct wlan_objmgr_pdev *pdev,
			    qdf_freq_t freq,
			    qdf_freq_t sec_ch_2g_freq,
			    struct ch_params *ch_params,
			    enum supported_6g_pwr_types in_6g_pwr_mode);

/**
 * reg_get_wmodes_and_max_chwidth() - Filter out the wireless modes
 * that are not supported by the available regulatory channels. Also,
 * return the maxbw supported by regulatory.
 * @pdev: Pointer to pdev.
 * @mode_select: Wireless modes to be filtered.
 * @include_nol_chan: boolean to indicate whether NOL channels are to be
 * considered as available channels.
 *
 * Return: Max reg bw
 */
uint16_t reg_get_wmodes_and_max_chwidth(struct wlan_objmgr_pdev *pdev,
					uint64_t *mode_select,
					bool include_nol_chan);

/**
 * reg_get_client_power_for_rep_ap() - Get the client power for the repeater AP
 * @pdev: Pointer to pdev.
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @chan_freq: Channel frequency
 * @is_psd: Pointer to is_psd
 * @reg_eirp: Pointer to EIRP power
 * @reg_psd: Pointer to PSD
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
reg_get_client_power_for_rep_ap(struct wlan_objmgr_pdev *pdev,
				enum reg_6g_ap_type ap_pwr_type,
				enum reg_6g_client_type client_type,
				qdf_freq_t chan_freq,
				bool *is_psd, uint16_t *reg_eirp,
				uint16_t *reg_psd);

#ifdef CONFIG_AFC_SUPPORT
/**
 * reg_get_client_psd_for_ap() - Get the client PSD for AP
 * @pdev: Pointer to pdev.
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @chan_freq: Channel frequency
 * @reg_psd: Pointer to PSD
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS reg_get_client_psd_for_ap(struct wlan_objmgr_pdev *pdev,
				     enum reg_6g_ap_type ap_pwr_type,
				     enum reg_6g_client_type client_type,
				     qdf_freq_t chan_freq,
				     uint16_t *reg_psd);
#endif

/**
 * reg_is_6g_domain_jp() - Check if current 6 GHz regdomain is a JP domain
 * or not.
 * @pdev: Pointer to pdev.
 *
 * Return: True if 6 GHz regdomain is a JP domain, else false.
 */
bool reg_is_6g_domain_jp(struct wlan_objmgr_pdev *pdev);

/**
 * reg_get_reg_chan_list_based_on_freq() - Chan list returned  based on freq
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: regulatory_channel.
 */
struct regulatory_channel
reg_get_reg_chan_list_based_on_freq(struct wlan_objmgr_pdev *pdev,
				    qdf_freq_t freq,
				    enum supported_6g_pwr_types
				    in_6g_pwr_mode);

/**
 * reg_get_first_valid_freq() - Get the first valid freq between the given
 * channel indexes based on cur chan list.
 * @pdev: Pointer to pdev.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @first_valid_freq: channel center frequency.
 * @bw: Bandwidth.
 * @sec_40_offset: 40 MHz channel's secondary offset
 * @start_chan: Input channel start index
 * @end_chan: Input channel end index
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
reg_get_first_valid_freq(struct wlan_objmgr_pdev *pdev,
			 enum supported_6g_pwr_types
			 in_6g_pwr_mode,
			 qdf_freq_t *first_valid_freq,
			 int bw, int sec_40_offset,
			 enum channel_enum start_chan,
			 enum channel_enum end_chan);

#ifdef CONFIG_BAND_6GHZ
/**
 * reg_get_max_reg_eirp_from_list() - Fill the input chan_eirp_list with
 * max_reg_eirp and channel information based on the input AP and client type.
 * @pdev: Pointer to pdev
 * @ap_pwr_type - 6G AP power type
 * @is_client_power_needed - Flag to indicate if client power is needed
 * @client_type - 6G client type
 * @chan_eirp_list - Pointer to chan_eirp_list
 * @num_6g_chans - Number of 6G channels
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS reg_get_max_reg_eirp_from_list(struct wlan_objmgr_pdev *pdev,
					  enum reg_6g_ap_type ap_pwr_type,
					  bool is_client_power_needed,
					  enum reg_6g_client_type client_type,
					  struct channel_power *chan_eirp_list,
					  uint8_t num_6g_chans);
#endif

/**
 * reg_quick_set_ap_pwr_and_update_chan_list() - Set the AP power mode and
 * recompute the current channel list.
 *
 * @pdev: Pointer to pdev.
 * @ap_pwr_type: The AP power type to update to.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
reg_quick_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					  enum reg_6g_ap_type ap_pwr_type);

/**
 * reg_is_freq_txable() - Check if the given frequency is tx-able.
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @in_6ghz_pwr_mode: Input AP power type
 *
 * Return: True if the frequency is tx-able, else false.
 */
bool
reg_is_freq_txable(struct wlan_objmgr_pdev *pdev,
		   qdf_freq_t freq,
		   enum supported_6g_pwr_types in_6ghz_pwr_mode);
#endif /* __REG_CHANNEL_H_ */
