/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ESP_H
#define ESP_H

#define ESP_DEFAULT_PPDU_DURATION       100

int hostapd_drv_set_esp_param_extn(struct hostapd_data *hapd, const char *param,
				   const int val);
int hostapd_drv_get_esp_params_extn(struct hostapd_data *hapd, int *airtime,
				    int *ppdu_dur, int *ba_window, int *enable);
int nl80211_parse_esp_params_extn(struct i802_bss *bss,
				  struct nlattr *esp_params_attr, u8 link_id);
void hostapd_update_esp_params_extn(struct hostapd_data *hapd,
				    union wpa_event_data *data);

#endif /* ESP_H */
