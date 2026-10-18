/* SPDX-License-Identifier: BSD-3-Clause */
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#ifndef UCODE_EXTN_H
#define UCODE_EXTN_H
#include "dfs_extn.h"

#ifndef CONFIG_QCN_EXTN
static inline
void hostapd_ucode_trigger_bhsta_disconnect(struct hostapd_iface *hapd)
{
}
static inline
bool wpas_ucode_freq_range_is_dfs(int center_freq, int bandwidth)
{
	return false;
}
static inline
bool wpas_ucode_is_dfs_chandef(int freq, enum chan_width ch_width,
			       int cf1, int cf2)
{
	return false;
}
static inline
void hostapd_ucode_notify_uplink_csa(struct hostapd_iface *hapd, int event, u8 channel,
				     int freq, int csa_count, u8 new_ch_width,
				     u8 ch_seg_0, u8 ch_seg_1,
				     const dfs_nol_ie_info *nol_info)
{
}
#else
void hostapd_ucode_trigger_bhsta_disconnect(struct hostapd_iface *hapd);
bool wpas_ucode_freq_range_is_dfs(int center_freq, int bandwidth);
bool wpas_ucode_is_dfs_chandef(int freq, enum chan_width ch_width,
			       int cf1, int cf2);
void hostapd_ucode_notify_uplink_csa(struct hostapd_iface *hapd, int event, u8 channel,
				     int freq, int csa_count, u8 new_ch_width,
				     u8 ch_seg_0, u8 ch_seg_1,
				     const dfs_nol_ie_info *nol_info);
#endif /* CONFIG_QCN_EXTN */
#endif /* UCODE_EXTN_H */
