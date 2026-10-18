// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include <netlink/genl/genl.h>
#include "utils/common.h"
#include "common/qca-vendor.h"
#include "drivers/driver.h"
#include "drivers/driver_nl80211.h"
#include "common/ieee802_11_common.h"
#include "ap/hostapd.h"
#include "ap/beacon.h"
#include "common/wpa_ctrl.h"
#include "cmn.h"
#include "afc.h"
#include "common/hw_features_common.h"

typedef int num_chan_t[QCA_6G_PWR_MAX][QCA_6G_BW_MAX];
typedef int centre_freq_t[QCA_6G_PWR_MAX][QCA_6G_BW_MAX][QCA_NUM_6G_CHAN];
typedef struct hostapd_channel_data list_6g_t[QCA_6G_PWR_MAX][QCA_6G_BW_MAX][QCA_NUM_6G_CHAN];

static void hostapd_get_6g_chan_list(struct hostapd_iface *iface,
				     num_chan_t num_chan,
				     centre_freq_t centre_freq,
				     list_6g_t list_6g)
{
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct hostapd_channel_data *channel_list;
	struct hostapd_channel_data_6ghz *channels_6g_data = &mode->channels_6ghz;
	struct hostapd_channel_data **chan_6ghz  = channels_6g_data->chans_6ghz;
	static const enum nl80211_regulatory_power_modes pwr_mode_order[] = {
		NL80211_REG_AP_SP,
		NL80211_REG_AP_LPI,
		NL80211_REG_AP_VLP};
	int i, j, n_chans;
	int chan_width = CHAN_WIDTH_320, channel_width;
	int num_pp;
	int list_chan_width;

	channel_width = channel_width_to_int(chan_width);
	num_pp = hostapd_get_num_pp(channel_width);
	channel_list = os_zalloc(sizeof(struct hostapd_channel_data) *
				 (mode->num_channels * num_pp));
	if(!channel_list) {
		wpa_printf(MSG_ERROR, "channel_list memory allocation failed");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(pwr_mode_order); i++) {
		enum nl80211_regulatory_power_modes pwr_mode;
		int n_en_chans = 0;

		chan_width = CHAN_WIDTH_320;
		pwr_mode = pwr_mode_order[i];
		while (chan_width > CHAN_WIDTH_20_NOHT) {
			switch(chan_width) {
			case CHAN_WIDTH_320:
				list_chan_width = QCA_6G_BW_320;
				break;
			case CHAN_WIDTH_160:
				list_chan_width = QCA_6G_BW_160;
				break;
			case CHAN_WIDTH_80:
				list_chan_width = QCA_6G_BW_80;
				break;
			case CHAN_WIDTH_40:
				list_chan_width = QCA_6G_BW_40;
				break;
			case CHAN_WIDTH_20:
				list_chan_width = QCA_6G_BW_20;
				break;
			}

			n_chans = convert_chwidth_to_20MHz_nchans(chan_width);
			n_en_chans = find_6g_enabled_chans(iface, chan_width, &channel_list,
							   mode, chan_6ghz, n_chans, pwr_mode);
			num_chan[pwr_mode][list_chan_width] = n_en_chans;
			for(j = 0; j < n_en_chans; j++) {
				int temp_centre_freq;

				list_6g[pwr_mode][list_chan_width][j] = channel_list[j];
				get_centre_freq_6g(channel_list[j].chan, chan_width, &temp_centre_freq);
				centre_freq[pwr_mode][list_chan_width][j] = temp_centre_freq;
			}
			chan_width = get_next_max_width(chan_width);
		}
	}
	os_free(channel_list);
}

int hostapd_get_6g_chan_list_extn(struct hostapd_iface *iface,
				  char *buf, size_t buflen)
{
	int len = 0, ret;
	num_chan_t num_chan;
	centre_freq_t centre_freq;
	list_6g_t list_6g;
	int p, b, c;

	hostapd_get_6g_chan_list(iface, num_chan, centre_freq, list_6g);

	for (p = 0; p < QCA_6G_PWR_MAX; p++) {
		ret = os_snprintf(buf + len, buflen - len,
				  "power mode: %d\n",
				  p);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		ret = len;
		for (b = 0; b < QCA_6G_BW_MAX; b++) {
			ret = os_snprintf(buf + len, buflen - len,
					  "Bandwidth: %d\n",
					  b);
			if (!os_snprintf_error(buflen - len, ret))
				len += ret;
			ret = len;
			for (c = 0; c < num_chan[p][b]; c++) {
				ret = os_snprintf(buf + len, buflen - len,
						  "channel: %d cf: %d pb: %d psd: %d eirp: %d\n",
						  list_6g[p][b][c].chan,
						  centre_freq[p][b][c],
						  list_6g[p][b][c].punct_bitmap,
						  list_6g[p][b][c].psd_power,
						  list_6g[p][b][c].eirp_power);
				if (!os_snprintf_error(buflen - len, ret))
					len += ret;
				ret = len;
			}
		}
	}

	return ret;
}
