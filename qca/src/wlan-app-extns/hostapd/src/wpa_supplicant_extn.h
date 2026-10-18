/* SPDX-License-Identifier: BSD-3-Clause */
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#ifndef WPA_SUPPLICANT_EXTN_H
#define WPA_SUPPLICANT_EXTN_H

int wpa_drv_send_uplink_csa(struct wpa_supplicant *wpa_s, int freq,
			    u8 cs_count, u8 ch_seg_0, u8 ch_seg_1,
			    u8 new_ch_width, const u8 *nol_ie,
			    size_t nol_ie_len, int cac_abort);
int wpa_drv_send_action_extn(struct wpa_supplicant *wpa_s, unsigned int freq,
			unsigned int wait, const u8 *dst, const u8 *src,
			const u8 *bssid, const u8 *data, size_t data_len,
			int no_cck, int link_id);
int wpa_drv_send_rrm_action_extn(struct wpa_supplicant *wpa_s,
				 const u8 *data, size_t data_len, int no_cck);
int wpa_ctrl_set_wds_ie_extn(struct wpa_supplicant *wpa_s, int val);
int wpa_ctrl_get_wds_ie_extn(struct wpa_supplicant *wpa_s,
			     char *reply, int reply_size);
int wpa_ctrl_set_allow_3addr_mc_extn(struct wpa_supplicant *wpa_s, int val);
bool wpas_uplink_csa_link_available(struct wpa_supplicant *wpa_s,
				    unsigned int freq);
#endif /* WPA_SUPPLICANT_EXTN_H */
