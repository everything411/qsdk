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
 * DOC: wlan_reg_channel_api.h
 * This file provides prototypes of the routines needed for the
 * external components to utilize the services provided by the
 * regulatory component with respect to channel list access.
 */

#ifndef __WLAN_REG_CHANNEL_API_H
#define __WLAN_REG_CHANNEL_API_H

#include <reg_services_public_struct.h>

enum ht40_intol {
	HT40_INTOL_PLUS = 0,
	HT40_INTOL_MINUS = 1,
};

#define IS_HT40_INTOL_MINUS (_bitmap) ((_bitmap) & BIT(HT40_INTOL_MINUS))
#define IS_HT40_INTOL_PLUS (_bitmap) ((_bitmap) & BIT(HT40_INTOL_PLUS))

#ifdef CONFIG_HOST_FIND_CHAN

#define WLAN_CHAN_PASSIVE       0x0000000000100000 /* Passive channel flag */

#define WLAN_CHAN_DFS              0x0002  /* DFS set on primary segment */
#define WLAN_CHAN_DFS_CFREQ2       0x0004  /* DFS set on secondary segment */
#define WLAN_CHAN_DISALLOW_ADHOC   0x0040  /* ad-hoc is not allowed */
#define WLAN_CHAN_PSC              0x0400  /* 6GHz PSC frequency */

/**
 * wlan_reg_set_chan_blocked() - Set is_chan_hop_blocked to true for a frequency
 * in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 *
 * Return: void.
 */
void wlan_reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq);

/**
 * wlan_reg_is_chan_blocked() - Check if is_chan_hop_blocked to true for a
 * frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 *
 * Return: true if is_chan_hop_blocked is true for the input frequency, else
 * false.
 */
bool wlan_reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq);

/**
 * wlan_reg_is_chan_blocked() - Clear is_chan_hop_blocked for channel in the
 * current chan list.
 * @pdev: Pointer to pdev.
 *
 * Return: void.
 */
void wlan_reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_set_chan_ht40intol() - Set ht40intol_flags to the value for a
 * frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: ht40intol_flags to be set.
 *
 * Return: void.
 */
void wlan_reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				 enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_clear_chan_ht40intol() - Clear the ht40intol_flags from the
 * regulatory channel corresponding to the frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: ht40intol_flags to be cleared.
 *
 * Return: void.
 */
void wlan_reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_is_chan_ht40intol() - Check if the ht40intol flag is set to the
 * given enum for a frequency in the current chan list.
 * @pdev: Pointer to pdev.
 * @freq: Channel frequency in MHz.
 * @ht40intol_flags: The ht40intol flag (plus/minus) to check.
 *
 * Return: true if is_chan_htintol is set to given value for the input
 * frequency, else false.
 */
bool wlan_reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				enum ht40_intol ht40intol_flags);

/**
 * wlan_reg_clear_allchan_ht40intol() - Clear ht40intol_flags for all channels
 * in the current chan list.
 * @pdev: Pointer to pdev.
 *
 * Return: void.
 */
void wlan_reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_is_phymode_chwidth_allowed() - Check if requested phymode is allowed
 * @pdev: pdev pointer.
 * @phy_in: phymode that the user requested.
 * @ch_width: Channel width that the user requested.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @input_puncture_bitmap: Input puncture bitmap
 *
 * Return: true if phymode is allowed, else false.
 */
bool wlan_reg_is_phymode_chwidth_allowed(struct wlan_objmgr_pdev *pdev,
					 enum reg_phymode phy_in,
					 enum phy_ch_width ch_width,
					 qdf_freq_t primary_freq,
					 enum supported_6g_pwr_types
					 in_6g_pwr_mode,
					 uint16_t input_puncture_bitmap);

/**
 * wlan_reg_get_max_channel_width() - Get the maximum channel width supported
 * given a frequency and a global maximum channel width.
 * @pdev: Pointer to PDEV object.
 * @freq: Input frequency.
 * @g_max_width: Global maximum channel width.
 * @input_puncture_bitmap: Input puncture bitmap
 *
 * Return: Maximum channel width of type phy_ch_width.
 */
enum phy_ch_width
wlan_reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq,
			       enum phy_ch_width g_max_width,
			       enum supported_6g_pwr_types in_6g_pwr_mode,
			       uint16_t input_puncture_bitmap);

/**
 * wlan_reg_get_max_phymode_and_chwidth() - Find the maximum regmode and
 * channel width combo supported by the device.
 * @phy_in: Maximum reg_phymode.
 * @ch_width: Maximum channel width.
 * @primary_freq: Input primary frequency.
 *
 * Return QDF_STATUS_SUCCESS if a combination is found, else return failure.
 */
QDF_STATUS wlan_reg_get_max_phymode_and_chwidth(struct wlan_objmgr_pdev *pdev,
						enum reg_phymode *phy_in,
						enum phy_ch_width *ch_width);

/**
 * wlan_reg_get_txpow_ant_gain() - Find the tx power and antenna gain for
 * the given frequency.
 * @pdev: pdev pointer.
 * @freq: Given frequency.
 * @txpower: tx power to be filled.
 * @ant_gain: Antenna gain to be filled.
 * @reg_chan_list: regulatory channel list.
 *
 */
void wlan_reg_get_txpow_ant_gain(struct wlan_objmgr_pdev *pdev,
				 qdf_freq_t freq,
				 uint32_t *txpower,
				 uint8_t *ant_gain,
				 struct regulatory_channel *reg_chan_list);

/**
 * wlan_reg_get_chan_flags() - Find the channel flags for freq1 and freq2.
 * @pdev: pdev pointer.
 * @freq1: Frequency in primary segment.
 * @freq2: Frequency in secondary segment.
 * @sec_flags: Secondary flags to be filled.
 * @pri_flags: Primary flags to be filled.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @reg_chan_list: regulatory channel list.
 */
void wlan_reg_get_chan_flags(struct wlan_objmgr_pdev *pdev,
			     qdf_freq_t freq1,
			     qdf_freq_t freq2,
			     uint16_t *sec_flags,
			     uint64_t *pri_flags,
			     enum supported_6g_pwr_types in_6g_pwr_mode,
			     struct regulatory_channel *reg_chan_list);

/**
 * wlan_reg_is_band_present() - Check if input band channels are present
 * in the regulatory current channel list.
 * @pdev: pdev pointer.
 * @reg_band: regulatory band.
 *
 */
bool wlan_reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			      enum reg_wifi_band reg_band);

#else
static inline
void wlan_reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
}

static inline
bool wlan_reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	return false;
}

static inline void wlan_reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev)
{
}

static inline void
wlan_reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			    enum ht40_intol ht40intol_flags)
{
}

static inline void
wlan_reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq,
			      enum ht40_intol ht40intol_flags)
{
}

static inline bool
wlan_reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
			   enum ht40_intol ht40intol_flags)
{
	return false;
}

static inline void
wlan_reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev)
{
}

static inline bool
wlan_reg_is_phymode_chwidth_allowed(struct wlan_objmgr_pdev *pdev,
				    enum reg_phymode phy_in,
				    enum phy_ch_width ch_width,
				    qdf_freq_t primary_freq,
				    uint16_t input_puncture_bitmap)
{
	return false;
}

static inline enum phy_ch_width
wlan_reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum phy_ch_width g_max_width,
			  enum supported_6g_pwr_types in_6g_pwr_mode,
			  uint16_t input_puncture_bitmap);
{
	return CH_WIDTH_INVALID;
}

static inline QDF_STATUS
wlan_reg_get_max_phymode_and_chwidth(struct wlan_objmgr_pdev *pdev,
				     enum reg_phymode *phy_in,
				     enum phy_ch_width *ch_width)
{
	return QDF_STATUS_E_FAILURE;
}

static inline void
wlan_reg_get_txpow_ant_gain(struct wlan_objmgr_pdev *pdev,
			    qdf_freq_t freq,
			    uint32_t *txpower,
			    uint8_t *ant_gain,
			    struct regulatory_channel *reg_chan_list)
{
}

static inline void
wlan_reg_get_chan_flags(struct wlan_objmgr_pdev *pdev,
			qdf_freq_t freq1,
			qdf_freq_t freq2,
			uint16_t *sec_flags,
			uint64_t *pri_flags,
			enum supported_6g_pwr_types in_6g_pwr_mode,
			struct regulatory_channel *reg_chan_list)
{
}

static inline
bool wlan_reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			      enum reg_wifi_band reg_band)
{
	return false;
}
#endif /* CONFIG_HOST_FIND_CHAN */

/**
 * wlan_reg_is_nol_freq() - Checks the channel is a nol chan or not
 * @freq: Channel center frequency
 *
 * Return: true if channel is nol else false
 */
bool wlan_reg_is_nol_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq);

/**
 * wlan_reg_is_nol_hist_for_freq() - Checks the channel is a nol history channel
 * or not.
 * @freq: Channel center frequency
 *
 * Return: true if channel is nol else false
 */
bool wlan_reg_is_nol_hist_for_freq(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq);

/**
 * wlan_reg_get_ap_chan_list() - Get AP channel list
 * @pdev       : Pointer to pdev
 * @chan_list  : Pointer to channel list
 * @ap_pwr_type: Enum for AP power type (for 6GHz)
 *
 *
 * NOTE: If get_cur_chan_list is true, then ap_pwr_type is ignored
 *
 * Return
 * QDF_STATUS_SUCCESS: Successfully retrieved channel list
 * QDF_STATUS_E_INVAL: Could not get channel list
 */
QDF_STATUS wlan_reg_get_ap_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel *chan_list,
				     bool get_cur_chan_list,
				     enum reg_6g_ap_type ap_pwr_type);

/**
 * wlan_reg_is_freq_width_dfs()- Check if the channel is dfs, given the channel
 * frequency and width combination.
 * @pdev: Pointer to pdev.
 * @freq: Channel center frequency.
 * @ch_width: Channel Width.
 *
 * Return: True if frequency + width has DFS subchannels, else false.
 */
bool wlan_reg_is_freq_width_dfs(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq,
				enum phy_ch_width ch_width);

/**
 * wlan_reg_get_channel_params() - Sets channel parameteres for
 * given bandwidth
 * @pdev: The physical dev to program country code or regdomain
 * @freq: channel center frequency.
 * @sec_ch_2g_freq: Secondary channel center frequency.
 * @ch_params: pointer to the channel parameters.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: None
 */
void wlan_reg_get_channel_params(struct wlan_objmgr_pdev *pdev,
				 qdf_freq_t freq,
				 qdf_freq_t sec_ch_2g_freq,
				 struct ch_params *ch_params,
				 enum supported_6g_pwr_types in_6g_pwr_mode);

/**
 * wlan_reg_get_wmodes_and_max_chwidth() - Filter out the wireless modes
 * that are not supported by the available regulatory channels.
 * @pdev: Pointer to pdev.
 * @mode_select: Wireless modes to be filtered.
 * @include_nol_chan: boolean to indicate whether NOL channels are to be
 * considered as available channels.
 *
 * Return: Max channel width
 */
uint16_t wlan_reg_get_wmodes_and_max_chwidth(struct wlan_objmgr_pdev *pdev,
					     uint64_t *mode_select,
					     bool include_nol_chan);

/**
 * wlan_reg_get_client_power_for_rep_ap() - Get the client power for the
 * repeater AP
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
wlan_reg_get_client_power_for_rep_ap(struct wlan_objmgr_pdev *pdev,
				     enum reg_6g_ap_type ap_pwr_type,
				     enum reg_6g_client_type client_type,
				     qdf_freq_t chan_freq,
				     bool *is_psd, uint16_t *reg_eirp,
				     uint16_t *reg_psd);

#ifdef CONFIG_AFC_SUPPORT
/**
 * wlan_reg_get_client_psd_for_ap() - Get the client PSD for AP
 * @pdev: Pointer to pdev.
 * @ap_pwr_type: AP power type
 * @client_type: Client type
 * @chan_freq: Channel frequency
 * @reg_psd: Pointer to PSD
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_client_psd_for_ap(struct wlan_objmgr_pdev *pdev,
			       enum reg_6g_ap_type ap_pwr_type,
			       enum reg_6g_client_type client_type,
			       qdf_freq_t chan_freq,
			       uint16_t *reg_psd);
#else
static inline QDF_STATUS
wlan_reg_get_client_psd_for_ap(struct wlan_objmgr_pdev *pdev,
			       enum reg_6g_ap_type ap_pwr_type,
			       enum reg_6g_client_type client_type,
			       qdf_freq_t chan_freq,
			       uint16_t *reg_psd)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * reg_is_6g_domain_jp() - Check if current 6GHz regdomain is a JP domain
 * or not.
 * @pdev: Pointer to pdev.
 *
 * Return: true if 6GHz regdomain is a JP domain else false.
 */
bool wlan_reg_is_6g_domain_jp(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_reg_get_reg_chan_list_based_on_freq() - Chan list returned  based on
 * freq
 * @pdev: Pointer to pdev.
 * @freq: channel center frequency.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 *
 * Return: regulatory_channel.
 */
struct regulatory_channel
wlan_reg_get_reg_chan_list_based_on_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq,
					 enum supported_6g_pwr_types
					 in_6g_pwr_mode);

/**
 * wlan_reg_get_first_valid_freq() - Get the first valid freq
 * between the given channel indexes based on cur chan list.
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
wlan_reg_get_first_valid_freq(struct wlan_objmgr_pdev *pdev,
			      enum supported_6g_pwr_types
			      in_6g_pwr_mode,
			      qdf_freq_t *first_valid_freq,
			      int bw, int sec_40_offset,
			      enum channel_enum start_chan,
			      enum channel_enum end_chan);

/**
 * wlan_reg_get_first_valid_freq_on_power_mode() - Get the first valid freq
 * based on pwr mode.
 * @pdev: Pointer to pdev.
 * @in_6g_pwr_mode: Input 6g power mode based on which the 6g channel list
 * is determined.
 * @freq: channel center frequency.
 * @bw: Bandwidth.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_reg_get_first_valid_freq_on_power_mode(struct wlan_objmgr_pdev *pdev,
					    enum supported_6g_pwr_types in_6g_pwr_mode,
					    qdf_freq_t *first_valid_freq,
					    int bw);

#ifdef CONFIG_BAND_6GHZ
/**
 * wlan_reg_get_max_reg_eirp_from_list() - Fill the input chan_eirp_list with
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
QDF_STATUS
wlan_reg_get_max_reg_eirp_from_list(struct wlan_objmgr_pdev *pdev,
				    enum reg_6g_ap_type ap_pwr_type,
				    bool is_client_power_needed,
				    enum reg_6g_client_type client_type,
				    struct channel_power *chan_eirp_list,
				    uint8_t num_6g_chans);
#else
static inline QDF_STATUS
wlan_reg_get_max_reg_eirp_from_list(struct wlan_objmgr_pdev *pdev,
				    enum reg_6g_ap_type ap_pwr_type,
				    bool is_client_power_needed,
				    enum reg_6g_client_type client_type,
				    struct channel_power *chan_eirp_list,
				    uint8_t num_6g_chans)
{
	chan_eirp_list = NULL;
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * wlan_quick_reg_set_ap_pwr_and_update_chan_list() - Set the AP power mode and
 * recompute the current channel list based on the new AP power mode.
 *
 * @pdev: Pointer to pdev.
 * @ap_pwr_type: The AP power type to update to.
 *
 * Return: QDF_STATUS.
 */
QDF_STATUS
wlan_quick_reg_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					       enum reg_6g_ap_type ap_pwr_type);

/**
 * wlan_reg_is_freq_txable() - Check if the given frequency is tx-able.
 * @pdev: Pointer to pdev
 * @freq: Frequency in MHz
 * @in_6ghz_pwr_mode: Input AP power type
 *
 * An SP channel is tx-able if the channel is present in the AFC response.
 * In case of non-OUTDOOR mode a channel is always tx-able (Assuming it is
 * enabled by regulatory).
 *
 * Return: True if the frequency is tx-able, else false.
 */
bool
wlan_reg_is_freq_txable(struct wlan_objmgr_pdev *pdev,
			qdf_freq_t freq,
			enum supported_6g_pwr_types in_6ghz_pwr_mode);
#endif /* __WLAN_REG_CHANNEL_API_H */
