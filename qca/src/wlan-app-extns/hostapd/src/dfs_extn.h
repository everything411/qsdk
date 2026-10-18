/* SPDX-License-Identifier: BSD-3-Clause */
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#ifndef DFS_EXTN_H
#define DFS_EXTN_H
#include "cmn.h"

/* Single NOL IE entry type */
typedef struct dfs_nol_ie_info_extn dfs_nol_ie_info;

#ifndef CONFIG_QCN_EXTN
#else

/* Channel Switch Announcement element (WLAN_EID_CHANNEL_SWITCH) fixed layout. */
#define IEEE80211_CSA_IE_MIN_LEN 3
#define IEEE80211_CSA_IE_MODE_OFFSET 2
#define IEEE80211_CSA_IE_NEW_CHANNEL_OFFSET 3
#define IEEE80211_CSA_IE_COUNT_OFFSET 4
#define IEEE80211_CSA_IE_TOTAL_LEN 5

/* NOL IE constants - aligned with RCSA design */
#define DFS_MAX_20M_SUB_CH 16
#define DFS_NOL_IE_TIMEOUT_MS 1800000  /* 30 minutes */
#define MIN_DFS_SUBCHAN_BW 20          /* 20 MHz minimum subchannel */

#define DFS_NOL_IE_SINGLE_SUBCHAN_BITMAP BIT(0)

#define DFS_NOL_IE_U32_LEN 4
#define DFS_NOL_IE_U16_LEN 2

#define DFS_NOL_IE_BITMAP_MASK(n_subchans) ((u16)((1U << (n_subchans)) - 1))

enum dfs_nol_ie_bw_mhz {
	DFS_NOL_IE_BW_20_MHZ = 20,
	DFS_NOL_IE_BW_40_MHZ = 40,
	DFS_NOL_IE_BW_80_MHZ = 80,
	DFS_NOL_IE_BW_160_MHZ = 160,
	DFS_NOL_IE_BW_320_MHZ = 320,
};

int dfs_prepare_nol_ie_bitmap(struct hostapd_iface *iface, int freq,
			      enum oper_chan_width chan_width, int cf1, int cf2,
			      u16 radar_bitmap,
			      dfs_nol_ie_info *nol_info);
int dfs_process_nol_ie_bitmap(struct hostapd_iface *iface,
			      const dfs_nol_ie_info *nol_info);
int dfs_process_nol_ie_bitmap_rcsa(struct hostapd_data *hapd,
				   const dfs_nol_ie_info *nol_info);
int dfs_decode_nol_ie(const u8 *ie, size_t ie_len,
		      dfs_nol_ie_info *nol_info);
int dfs_encode_nol_ie(const dfs_nol_ie_info *nol_info, u8 *buf,
		      size_t buf_len);
int hostapd_dfs_restart_channel_extn(struct hostapd_iface *iface);
bool hostapd_bootup_cac_start_extn(struct hostapd_iface *iface);
void hostapd_bootup_cac_complete_extn(struct hostapd_iface *iface);
bool hostapd_bss_rnr_eligible_extn(struct hostapd_data *bss);
bool hostapd_bootup_cac_enabled_extn(struct hostapd_iface *iface);
bool hostapd_is_backhaul_sta_configured(struct hostapd_iface *iface);
void hostapd_trigger_backhaul_sta_disconnect(void *eloop_data, void *user_data);
void hostapd_rcsa_trigger_channal_change(void *eloop_data, void *user_data);
void hostapd_trigger_rcsa_tx(void *eloop_data, void *user_data);
bool hostapd_rcsa_tx_bh_enabled(struct hostapd_iface *iface);
void hostapd_rcsa_handle_csa_timeout(struct hostapd_iface *iface);
void hostapd_set_rcsa_inprogress(struct hostapd_iface *iface, bool value);
bool bootup_cac_current_channel_has_nol_extn(struct hostapd_iface *iface);

/**
 * dfs_get_ch_flags_extn - Compute DFS random channel selection flags
 * @cswopts: Channel Switch Options bitmap from hostapd/wpa_supplicant config
 *
 * Always sets DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH.
 * Sets DFS_RANDOM_CH_FLAG_NO_DFS_CH when CSwOpts requests it.
 *
 * Returns the ch_flags bitmask to pass to dfs_get_valid_channel().
 */
unsigned int dfs_get_ch_flags_extn(unsigned int cswopts);

/**
 * dfs_chan_skip_by_flags_extn - Check whether a channel should be skipped
 * during random DFS channel selection
 * @iface: Pointer to hostapd_iface
 * @chan: Candidate primary channel to evaluate
 * @flags: DFS_RANDOM_CH_FLAG_* bitmask from dfs_get_ch_flags_extn()
 *
 * Evaluates the two flag-driven skip conditions introduced by the enhanced
 * random channel selection algorithm:
 *
 *   DFS_RANDOM_CH_FLAG_NO_CURR_OPE_CH: skip @chan if it is one of the 20 MHz
 *     sub-channels that make up the current operating channel.  The set of
 *     sub-channels is determined by calling dfs_get_start_chan_idx() and
 *     dfs_get_used_n_chans() and then iterating over
 *     mode->channels[start_chan_idx + i].
 *
 *   DFS_RANDOM_CH_FLAG_NO_DFS_CH: skip the candidate channel range if ANY of
 *     its sub-channels has HOSTAPD_CHAN_RADAR set.  The BW range is derived
 *     internally from iface->conf (via dfs_get_used_n_chans()), so a non-DFS
 *     primary channel (e.g. channel 36) that spans into DFS sub-channels at
 *     wide bandwidth (e.g. EHT 160 MHz) is correctly rejected.
 *
 * Returns: true if @chan should be skipped, false otherwise.
 */
bool dfs_chan_skip_by_flags_extn(struct hostapd_iface *iface,
				 struct hostapd_channel_data *chan,
				 unsigned int flags);

int dfs_nol_ie_chan_width_to_bw_mhz(enum oper_chan_width chan_width,
				    int freq, int cf1,
				    int *bandwidth_mhz);
bool optional_ml_info_ie_access(u8 *buf, size_t buf_len,
				s8 *link_id, bool set);


/*
 * RCSA Vendor Specific Action frame:
 * category(1) + Atheros OUI(3) + CSA IE [+ optional QCA NOL IE].
 */
#define RCSA_VENDOR_ACTION_HDR_LEN 4
#define RCSA_CSA_IE_HDR_LEN 2
#define RCSA_NOL_IE_INFO_LEN 4
#define RCSA_NOL_IE_TOTAL_LEN (RCSA_CSA_IE_HDR_LEN + RCSA_NOL_IE_INFO_LEN)
#define RCSA_MIN_FRAME_LEN \
	(RCSA_VENDOR_ACTION_HDR_LEN + RCSA_CSA_IE_HDR_LEN + \
	 IEEE80211_CSA_IE_MIN_LEN)
#define RCSA_MIN_DFS_SUBCHAN_BW 20
#define RCSA_MAX_20M_SUB_CH 8

#define HOSTAPD_RCSA_TX_COUNT 5
#define HOSTAPD_RCSA_SWITCH_MODE 1
#define HAPD_DFS_WAIT_FOR_RCSA_FROM_ROOT_DUR_US(bcn_intval) (HOSTAPD_RCSA_TX_COUNT * (bcn_intval) * 2)
#define HOSTAPD_DFS_BH_DISCONNECT_WAIT_TIME_US 1000
#define HOSTAPD_RCSA_INTVAL_US (100 * 1000)

int dfs_nol_ie_get_subchan_count(enum oper_chan_width chan_width,
				 int freq, int cf1,
				 int *n_subchans);
#endif /* CONFIG_QCN_EXTN */
#endif /* DFS_EXTN_H */
