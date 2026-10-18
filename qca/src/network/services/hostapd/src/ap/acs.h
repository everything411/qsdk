/*
 * ACS - Automatic Channel Selection module
 * Copyright (c) 2011, Atheros Communications
 * Copyright (c) 2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef ACS_H
#define ACS_H

#ifdef CONFIG_ACS

enum bw_type {
	ACS_BW40,
	ACS_BW80,
	ACS_BW160,
	ACS_BW320_1,
	ACS_BW320_2,
};

enum hostapd_chan_status acs_init(struct hostapd_iface *iface);
void acs_cleanup(struct hostapd_iface *iface);
void acs_cleanup_mode(struct hostapd_hw_modes *mode);

#define ACS_SCAN_RETRY_MAX_COUNT	15
#define ACS_SCAN_RETRY_INTERVAL		5

#ifdef CONFIG_IEEE80211BN
void hostapd_npca_auto_select(struct hostapd_iface *iface);
#endif /* CONFIG_IEEE80211BN */

#else /* CONFIG_ACS */

static inline enum hostapd_chan_status acs_init(struct hostapd_iface *iface)
{
	wpa_printf(MSG_ERROR, "ACS was disabled on your build, rebuild hostapd with CONFIG_ACS=y or set channel");
	return HOSTAPD_CHAN_INVALID;
}

static inline void acs_cleanup(struct hostapd_iface *iface)
{
}

static inline void acs_cleanup_mode(struct hostapd_hw_modes *mode) {}

#endif /* CONFIG_ACS */

#endif /* ACS_H */
