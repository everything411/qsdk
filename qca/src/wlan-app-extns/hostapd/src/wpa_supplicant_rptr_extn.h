/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef WPA_SUPPLICANT_RPTR_EXTN_H
#define WPA_SUPPLICANT_RPTR_EXTN_H

#include "cmn.h"

void wpa_bss_check_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					    struct wpa_bss *bss);
bool wpas_sta_cac_5g_320mhz_update_freq_params_extn(enum chan_width width,
					     u8 cf2_idx,
					     struct hostapd_freq_params *params);
void wpas_ch_switch_5g_320mhz_vendor_ie_extn(struct wpa_supplicant *wpa_s,
					    union wpa_event_data *data);
int wpa_config_process_cswopts_extn(struct wpa_config *config, int line,
				    const char *pos);
int wpa_supplicant_ctrl_iface_set_cswopts_extn(struct wpa_supplicant *wpa_s,
					       const char *value);

#endif /* WPA_SUPPLICANT_RPTR_EXTN_H */
