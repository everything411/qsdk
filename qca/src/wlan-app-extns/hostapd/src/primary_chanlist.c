// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/hw_features.h"
#include "common/hw_features_common.h"
#include "dcs.h"
#include "cmn.h"

/**
 * hostapd_set_primary_chanlist - Set the primary channel list
 * @iface:    Pointer to hostapd interface
 * @chan_str: Space-separated channel numbers; empty string clears the list
 *
 * Parses @chan_str and converts each channel number to its centre frequency
 * (MHz) using hw_get_channel_chan() against the hardware channel table.
 * Channels not present in the hardware table are rejected.  An empty
 * argument string clears the list.
 *
 * Return: 0 on success, -1 on parse error, invalid channel, or if the
 *         hardware channel table is not yet available.
 */
int hostapd_set_primary_chanlist(struct hostapd_data *hapd, const char *chan_str)
{
	struct hostapd_config_extn *conf_extn;
	struct hostapd_iface *iface;
	const char *p;
	char *endp;
	int n = 0, freq;
	long ch;

	if (!hapd)
		return -1;

        iface = hapd->iface;
        if (!hapd->iface)
		return -1;

	if (!iface->current_mode) {
		wpa_printf(MSG_ERROR,
			   "PRIMARY_CHAN: interface not ready,"
			   " cannot validate channels");
		return -1;
	}

	conf_extn = &iface->conf->conf_extn;

	/* Reset the existing list before repopulating */
	conf_extn->num_primary_freq = 0;
	os_memset(conf_extn->primary_freq_list, 0,
		  sizeof(conf_extn->primary_freq_list));

	p = chan_str;
	while (*p) {
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\0')
			break;

		ch = strtol(p, &endp, 10);
		if (endp == p || ch <= 0 || ch > 255) {
			wpa_printf(MSG_ERROR,
				   "PRIMARY_CHAN: invalid channel number");
			return -1;
		}

		/*
		 * Use hw_get_freq() to look up the frequency from the hardware
		 * channel table.  This avoids duplicating the channel-to-
		 * frequency conversion formulas already present in
		 * hw_features_common.c and ensures only channels supported by
		 * the current regulatory domain are accepted.
		 */
		freq = hw_get_freq(iface->current_mode, (int)ch);
		if (freq <= 0) {
			wpa_printf(MSG_ERROR,
				   "PRIMARY_CHAN: channel %ld not found in"
				   " hw channel table", ch);
			return -1;
		}

		if (n >= MAX_NUM_CHANNELS) {
			wpa_printf(MSG_ERROR,
				   "PRIMARY_CHAN: too many channels (max %d)",
				   MAX_NUM_CHANNELS);
			return -1;
		}
		conf_extn->primary_freq_list[n++] = (u16)freq;
		p = endp;
	}

	conf_extn->num_primary_freq = (u8)n;
	hostapd_update_primary_chanlist_flags(hapd);

	if (n > 0)
		wpa_printf(MSG_INFO,
			   "PRIMARY_CHAN: configured %d channel(s)", n);
	else
		wpa_printf(MSG_INFO, "PRIMARY_CHAN: list cleared");

	return 0;
}

/**
 * hostapd_get_primary_chanlist - Return the primary channel list as a string
 * @iface:  Pointer to hostapd interface
 * @buf:    Caller-provided output buffer
 * @buflen: Size of @buf in bytes
 *
 * Converts the stored frequencies back to channel numbers using
 * hw_mode_get_channel() and writes them as a space-separated list followed
 * by a newline.  Returns "DISABLED\n" when no list is configured.
 *
 * Return: Number of bytes written (excluding NUL terminator) on success,
 *         or -1 on error.
 */
int hostapd_get_primary_chanlist( struct hostapd_iface *iface,
				 char *buf, size_t buflen)
{
	struct hostapd_config_extn *conf_extn;
	size_t pos = 0;
	int i, ret, chan;

	if (!iface || !iface->conf || !buf || buflen == 0)
		return -1;

	conf_extn = &iface->conf->conf_extn;

	if (conf_extn->num_primary_freq == 0) {
		ret = os_snprintf(buf, buflen, "DISABLED\n");
		if (os_snprintf_error(buflen, ret))
			return -1;
		return ret;
	}

	for (i = 0; i < conf_extn->num_primary_freq; i++) {
		chan = 0;
		/*
		 * Use hw_mode_get_channel() to convert the stored frequency
		 * back to a channel number for display.
		 */
		if (iface->current_mode)
			hw_mode_get_channel(iface->current_mode,
					    conf_extn->primary_freq_list[i],
					    &chan);

		ret = os_snprintf(buf + pos, buflen - pos,
				  "%s%d",
				  (i == 0) ? "" : " ",
				  chan);
		if (os_snprintf_error(buflen - pos, ret))
			return -1;
		pos += ret;
	}

	ret = os_snprintf(buf + pos, buflen - pos, "\n");
	if (os_snprintf_error(buflen - pos, ret))
		return -1;
	pos += ret;

	return (int)pos;
}

bool chan_pri_allowed_extn(const struct hostapd_channel_data *chan)
{
      return !chan->extn.is_non_primary;
}

int hostapd_is_chan_in_primary_list(struct hostapd_iface *iface, u16 freq)
{
	struct hostapd_config_extn *conf_extn;
	int i;

	if (!iface || !iface->conf)
		return -1;

	conf_extn = &iface->conf->conf_extn;

	if (!conf_extn->num_primary_freq) {
		return 1;
	}

	for (i = 0; i < conf_extn->num_primary_freq; i++) {
		if (conf_extn->primary_freq_list[i] == freq)
			return 1;
	}

	return 0;
}

void
hostapd_update_primary_chanlist_flags(struct hostapd_data *hapd)
{
	struct hostapd_config_extn *conf_extn;
	struct hostapd_hw_modes *mode;
	struct hostapd_iface *iface;
	int i, j;
	int acs_ret;
	int k;

	if (!hapd)
		return;

	iface = hapd->iface;
	if (!iface || !iface->conf)
		return;

	conf_extn = &iface->conf->conf_extn;

	for (k = 0; k < iface->num_hw_features; k++) {
		mode = &iface->hw_features[k];
		if (hostapd_hw_skip_mode(iface, mode))
			continue;

		for (i = 0; i < mode->num_channels; i++) {
			if (conf_extn->num_primary_freq) {
				mode->channels[i].extn.is_non_primary = true;

				for (j = 0; j < conf_extn->num_primary_freq; j++) {
					if (mode->channels[i].freq ==
							conf_extn->primary_freq_list[j]) {
						mode->channels[i].extn.is_non_primary = false;
						break;
					}
				}
			} else {
				mode->channels[i].extn.is_non_primary = false;
			}
		}
	}
	if (conf_extn->num_primary_freq) {
		if (!hostapd_is_chan_in_primary_list(iface, (u16)iface->freq)) {
			wpa_printf(MSG_ERROR,
				   "PRIMARY_CHAN: current channel not in primary freq list");

			acs_ret = hostapd_trigger_dynamic_acs(hapd,
							      CHANNEL_CHANGE_CSA);
			if (acs_ret < 0) {
			 wpa_printf(MSG_INFO,"Error ACS is not triggered(%d)",acs_ret);
			}
		} else {
			wpa_printf(MSG_INFO,
				   "PRIMARY_CHAN: primary channel list updated");
		}
	}
}
