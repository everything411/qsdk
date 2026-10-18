/*
 * DFS - Dynamic Frequency Selection
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2013-2017, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
#ifndef DFS_H
#define DFS_H

/* Wait duration between radar detection and channel switch*/
#define HAPD_DFS_RADAR_CH_SWITCH_WAIT_DUR 500000
#define HAPD_AGILE_CAC_RESTART_DELAY_SECS 3

/* CSA count for uplink CSA notification to wpa_supplicant */
#define HAPD_DFS_UPLINK_CSA_COUNT 5

/* DFS_RANDOM_CH_FLAG bits passed to dfs_get_valid_channel() */
#define DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH   0x00000001 /* exclude current operating channel */
#define DFS_RANDOM_CH_FLAG_NO_DFS_CH        0x00000002 /* exclude DFS channels */

/*identify freq using channel number*/
#define BASE_FREQ_5G 5160
#define BASE_CHAN_5G 32
#define GET_FREQ_CHAN_5G(chan) (BASE_FREQ_5G + ((chan - BASE_CHAN_5G) * 5))

bool hostapd_is_freq_in_current_hw_info(struct hostapd_iface *iface, int freq);

int hostapd_handle_dfs(struct hostapd_iface *iface);

/**
 * hostapd_dfs_complete_cac - Handle DFS CAC completion
 * @iface: Pointer to hostapd interface
 * @success: CAC completion status
 * @freq: Frequency
 * @ht_enabled: HT enabled flag
 * @chan_offset: Channel offset
 * @chan_width: Channel width
 * @cf1: Center frequency 1
 * @cf2: Center frequency 2
 * @unpunc_bitmap: Bitmap identifying radar-affected subchannels on which CAC has completed.
 *     This bitmap is used to track which subchannels are no longer in DFS state and
 *     need to be unpunctured.
 * @is_background: Background CAC flag
 * @chan_width_device: Device channel width
 * @cf_device: Device center frequency
 *
 * Completion handling for the following CAC types:
 *
 * 1) Home channel CAC
 *    (a) Regular CAC running over the entire bandwidth of the channel.
 *        This generally occurs before starting any transmission on the home channel.
 *    (b) Puncture / Un-puncture CAC.
 *        This typically runs on one or two 20 MHz subchannels while the
 *        remaining subchannels continue normal transmission and reception.
 *
 * 2) Agile CAC / Pre-CAC
 *    CAC is performed on an Agile channel while the home channel is either
 *    undergoing CAC or in ISM (In-Service Monitoring).
 *
 * In the case of Puncture/Un-puncture CAC, the @unpunc_bitmap indicates
 * the subchannels on which CAC has been completed.
 */
int hostapd_dfs_complete_cac(struct hostapd_iface *iface, int success, int freq,
			     int ht_enabled, int chan_offset, int chan_width,
			     int cf1, int cf2, u16 unpunc_bitmap,
			     bool is_background, int chan_width_device,
			     int cf_device);
int hostapd_dfs_pre_cac_expired(struct hostapd_iface *iface, int freq,
				int ht_enabled, int chan_offset, int chan_width,
				int cf1, int cf2,
				int chan_width_device, int cf_device);
int hostapd_dfs_radar_detected(struct hostapd_iface *iface, int freq,
			       int ht_enabled,
			       int chan_offset, int chan_width,
			       int cf1, int cf2, u16 radar_bitmap,
			       int chan_width_device, int cf_device);
int hostapd_dfs_nop_finished(struct hostapd_iface *iface, int freq,
			     int ht_enabled,
			     int chan_offset, int chan_width, int cf1, int cf2,
			     int chan_width_device, int cf_device);
int hostapd_is_dfs_required(struct hostapd_iface *iface);
int hostapd_is_dfs_chan_available(struct hostapd_iface *iface);
int hostapd_dfs_start_channel_switch(struct hostapd_iface *iface);
int hostapd_dfs_start_cac(struct hostapd_iface *iface, int freq,
			  int ht_enabled, int chan_offset, int chan_width,
			  int cf1, int cf2, bool is_background,
			  int chan_width_device, int cf_device);
int hostapd_handle_dfs_offload(struct hostapd_iface *iface);
int hostapd_is_dfs_overlap(struct hostapd_iface *iface, enum chan_width width,
			   int center_freq);
void hostapd_dfs_radar_handling_timeout(void *eloop_data, void *user_data);
void hostapd_start_device_cac_background(struct hostapd_iface *iface);
int hostapd_start_background_cac(struct hostapd_iface *iface);
bool dfs_use_radar_background(struct hostapd_iface *iface);
int hostapd_start_rcac_on_channel(struct hostapd_iface *iface, int chan, int bw_mhz);
int hostapd_dfs_agile_cac_switch(struct hostapd_iface *iface);
void hostapd_restart_agile_cac_after_ch_switch(struct hostapd_iface *iface);
void hostapd_schedule_agile_cac_restart(struct hostapd_iface *iface);
void hostapd_cancel_agile_cac_restart(struct hostapd_iface *iface);
void hostapd_abort_background_cac(struct hostapd_iface *iface);
enum oper_chan_width convert_to_oper_chan_width(int chan_width);

int set_dfs_state_freq(struct hostapd_iface *iface, int freq, u32 state);
bool hostapd_is_cac_required(struct hostapd_iface *iface);
bool hostapd_dfs_csa_target_has_unavailable_channel(struct hostapd_iface *iface,
						    struct hostapd_freq_params *freq_params,
						    enum chan_width width);
void hostapd_dfs_start_background_cac_deferred(struct hostapd_iface *iface);
int hostapd_dfs_count_precac_channels(struct hostapd_iface *iface);
int hostapd_dfs_start_precac(struct hostapd_iface *iface);
int hostapd_dfs_precac_restart_after_radar(struct hostapd_iface *iface,
					   int radar_freq);

/**
 * hostapd_get_remaining_cac_tu - Get remaining CAC time in TU
 * @iface: Pointer to hostapd interface undergoing CAC
 * Returns: Remaining CAC time in TU, or 0 if CAC is not running / complete
 */
u32 hostapd_get_remaining_cac_tu(struct hostapd_iface *iface);

/**
 * dfs_find_bw_reduced_channel - Try to reduce bandwidth on same channel
 * @iface: Pointer to interface data
 * @secondary_channel: Pointer to secondary channel offset (output)
 * @oper_centr_freq_seg0_idx: Pointer to center freq seg0 (output)
 * @oper_centr_freq_seg1_idx: Pointer to center freq seg1 (output)
 * Returns: Channel data pointer on success, NULL on failure
 */

struct hostapd_channel_data *
dfs_find_bw_reduced_channel(struct hostapd_iface *iface,
			   int *secondary_channel,
			   u8 *oper_centr_freq_seg0_idx,
			   u8 *oper_centr_freq_seg1_idx);

int hostapd_set_dfs_cac_time(struct hostapd_iface *iface);
#ifdef CONFIG_QCN_EXTN
int dfs_check_chans_radar(struct hostapd_iface *iface,
			  int start_chan_idx, int n_chans);
int dfs_check_chans_available(struct hostapd_iface *iface,
			      int start_chan_idx, int n_chans);
#endif /* CONFIG_QCN_EXTN */
bool hostapd_dfs_intercac_boot(struct hostapd_iface *iface);
int hostapd_dfs_intercac_defer_non_radar_switch(struct hostapd_iface *iface,
						struct csa_settings *settings);
#endif /* DFS_H */
