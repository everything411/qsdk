/*
 * Control interface for shared AP commands
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/sae.h"
#include "common/wpa_ctrl.h"
#include "common/hw_features_common.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "fst/fst_ctrl_iface.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "wpa_auth_i.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "wps_hostapd.h"
#include "p2p_hostapd.h"
#include "ctrl_iface_ap.h"
#include "ap_drv_ops.h"
#include "mbo_ap.h"
#include "taxonomy.h"
#include "wnm_ap.h"
#include "neighbor_db.h"
#include "../drivers/driver_nl80211.h"
#include "beacon.h"

static const char * hw_mode_str(enum hostapd_hw_mode mode)
{
	switch (mode) {
	case HOSTAPD_MODE_IEEE80211B:
		return "b";
	case HOSTAPD_MODE_IEEE80211G:
		return "g";
	case HOSTAPD_MODE_IEEE80211A:
		return "a";
	case HOSTAPD_MODE_IEEE80211AD:
		return "ad";
	case HOSTAPD_MODE_IEEE80211ANY:
		return "any";
	case NUM_HOSTAPD_MODES:
		return "invalid";
	}
	return "unknown";
}

#ifdef CONFIG_CTRL_IFACE_MIB

typedef enum {
	BAND_UNII_1 = 1,
	BAND_UNII_2A,
	BAND_UNII_2C,
	BAND_UNII_3,
	BAND_UNII_4,
	BAND_UNII_ALL_5G,
	BAND_UNII_ALL_6G,
	BAND_UNII_5,
} band_info_t;

static unsigned int uniibands[8] = {0};

static size_t hostapd_write_ht_mcs_bitmask(char *buf, size_t buflen,
					   size_t curr_len, const u8 *mcs_set)
{
	int ret;
	size_t len = curr_len;

	ret = os_snprintf(buf + len, buflen - len,
			  "ht_mcs_bitmask=");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	/* 77 first bits (+ 3 reserved bits) */
	len += wpa_snprintf_hex(buf + len, buflen - len, mcs_set, 10);

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (os_snprintf_error(buflen - len, ret))
		return curr_len;
	len += ret;

	return len;
}


static int hostapd_get_sta_conn_time(struct sta_info *sta,
				     struct hostap_sta_driver_data *data,
				     char *buf, size_t buflen)
{
	struct os_reltime age;
	unsigned long secs;
	int ret;

	if (sta->connected_time.sec) {
		/* Locally maintained time in AP mode */
		os_reltime_age(&sta->connected_time, &age);
		secs = (unsigned long) age.sec;
	} else if (data->flags & STA_DRV_DATA_CONN_TIME) {
		/* Time from the driver in mesh mode */
		secs = data->connected_sec;
	} else {
		return 0;
	}

	ret = os_snprintf(buf, buflen, "connected_time=%lu\n", secs);
	if (os_snprintf_error(buflen, ret))
		return 0;
	return ret;
}


u8 hostapd_tx_maxnss(struct hostapd_data *hapd, struct sta_info *sta)
{
	u8 nss = 0;
	u8 tx_nss = 1;
	u8 mcs_count;
	u16 tx_mcs_set;
	int i, j;
	const u16 *ap_mcs_set = NULL;
	const u8 *mcs_set = NULL;
	struct hostapd_config *conf = hapd->iface->conf;
	struct hostapd_hw_modes *mode = NULL;
	u8 support_check[MAXNSS_HTMODE_MAX] = {};
	u8 htmode = MAXNSS_HTMODE_UNSET;

	if (sta) {
		htmode =  (!!(sta->flags & WLAN_STA_HT)) |
			((!!(sta->flags & WLAN_STA_VHT)) << 1) |
			((!!(sta->flags & WLAN_STA_HE)) << 2) |
			((!!(sta->flags & WLAN_STA_EHT)) << 3);
		support_check[MAXNSS_HTMODE_HT_N] = !!sta->ht_capabilities;
		support_check[MAXNSS_HTMODE_VHT_AC] = !!sta->vht_capabilities;
		support_check[MAXNSS_HTMODE_EHT_BE] = !!sta->eht_capab;
		support_check[MAXNSS_HTMODE_HE_AX] = !!sta->he_capab;
	} else {
		htmode = (!!conf->ieee80211n) |
			((!!conf->ieee80211ac) << 1) |
			((!!conf->ieee80211ax) << 2) |
			((!!conf->ieee80211be) << 3);
		support_check[MAXNSS_HTMODE_HT_N] = hostapd_is_ht_enabled(hapd);
		support_check[MAXNSS_HTMODE_VHT_AC] = hostapd_is_vht_enabled(hapd);
		support_check[MAXNSS_HTMODE_EHT_BE] = hostapd_is_eht_enabled(hapd);
		support_check[MAXNSS_HTMODE_HE_AX] = hostapd_is_he_enabled(hapd);
		mode = hapd->iface->current_mode;
	}
	htmode |= htmode >> 1;
	htmode |= htmode >> 2;
	htmode |= htmode >> 4;
	htmode++;
	htmode = htmode >> 1;
	if (!(htmode < MAXNSS_HTMODE_MAX) || !support_check[htmode] || (!sta && !mode))
		return tx_nss;

	switch (htmode) {
		case MAXNSS_HTMODE_HT_N:
			/* HT does not carry separate TX NSS in supported_mcs_set; assume RX */
			mcs_set = (sta) ? sta->ht_capabilities->supported_mcs_set : mode->mcs_set;
			return (!!mcs_set[0])  + (!!mcs_set[1]) + (!!mcs_set[2]) + (!!mcs_set[3]);
		case MAXNSS_HTMODE_VHT_AC:
			tx_mcs_set = (sta) ?
				le_to_host16(sta->vht_capabilities->vht_supported_mcs_set.tx_map) :
				(mode->vht_mcs_set[4] | (mode->vht_mcs_set[5] << 8));
			for (i = VHT_RX_NSS_MAX_STREAMS - 1; i >= 0; i--) {
				if (((tx_mcs_set >> (2 * i)) & 0x03) != 0x03)
					return i + 1;
			}
			return tx_nss;
		case MAXNSS_HTMODE_EHT_BE:
			mcs_count = 1;
			mcs_set = (sta) ? sta->eht_capab->optional : mode->eht_capab[1].mcs;
			switch (conf->eht_oper_chwidth) {
				case CONF_OPER_CHWIDTH_320MHZ:
					mcs_count++;
					/* fall through */
				case CONF_OPER_CHWIDTH_80P80MHZ:
				case CONF_OPER_CHWIDTH_160MHZ:
					mcs_count++;
					break;
				default:
					break;
			}
			for (i = 0; i < mcs_count * EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS; i++) {
				nss = (mcs_set[i] & 0x000F);
				if (nss > tx_nss)
					tx_nss = nss;
			}
			return tx_nss;
		case MAXNSS_HTMODE_HE_AX:
			mcs_count = 0;
			ap_mcs_set =   (sta) ?  (u16 *) sta->he_capab->optional :
				(u16 *) mode->he_capab[1].mcs;
			switch (conf->he_oper_chwidth) {
				case CONF_OPER_CHWIDTH_80P80MHZ:
					mcs_count = 3;
					break;
				case CONF_OPER_CHWIDTH_160MHZ:
					mcs_count = 2;
					break;
				default:
					mcs_count = 1;
					break;
			}
			/* For each width, TX map follows RX map; select TX map index */
			for (i = 0; i < mcs_count; i++) {
				tx_mcs_set = WPA_GET_LE16((const u8 *)&ap_mcs_set[(i * 2) + 1]);
				for (j = HE_NSS_MAX_STREAMS - 1; j >= 0; j--) {
					if (((tx_mcs_set >> (2 * j)) & 0x03) != 0x03)
						return j + 1;
				}
			}
			/* Fallback to <= 80 MHz basic TX map */
			if (sta && sta->he_capab) {
				tx_mcs_set = le_to_host16(sta->he_capab->he_basic_supported_mcs_set.tx_map);
			} else if (mode) {
				/* mode->he_capab[AP].mcs holds basic maps */
				tx_mcs_set = WPA_GET_LE16(mode->he_capab[1].mcs);
			} else {
				return tx_nss;
			}
			for (j = HE_NSS_MAX_STREAMS - 1; j >= 0; j--) {
				if (((tx_mcs_set >> (2 * j)) & 0x03) != 0x03)
					return j + 1;
			}
			/* fall through */
		default:
			return tx_nss;
	}
	return tx_nss;
}


static int hostapd_get_sta_phy_mode(struct sta_info *sta,
				    struct hostapd_data *hapd,
				    char *buf, size_t buflen)
{
	int ret, len = 0;
	enum oper_chan_width chwidth;

	ret = os_snprintf(buf, buflen,
			  "max_STA_phymode=");
	if (os_snprintf_error(buflen, ret))
		return 0;
	len += ret;

	if (sta->flags & WLAN_STA_UHR) {
		ret = os_snprintf(buf + len, buflen - len,
				  "[11BN]");
	} else if (sta->flags & WLAN_STA_EHT) {
		ret = os_snprintf(buf + len, buflen - len,
				  "[11BE]");
	} else if (sta->flags & WLAN_STA_HE) {
		ret = os_snprintf(buf + len, buflen - len,
				  "[11AX]");
	} else if (sta->flags & WLAN_STA_VHT) {
		ret = os_snprintf(buf + len, buflen - len,
				  "[11AC]");
	} else if (sta->flags & WLAN_STA_HT) {
		ret = os_snprintf(buf + len, buflen - len,
				  "[11N]");
	} else if (!(sta->flags & WLAN_STA_NONERP)) {
		/* 11g: ERP-OFDM capable STA */
		ret = os_snprintf(buf + len, buflen - len,
				  "[11G]");
	} else {
		 /* 11b: non-ERP STA (DSSS/CCK only) */
		ret = os_snprintf(buf + len, buflen - len,
				  "[11B]");
	}
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	chwidth = hostapd_get_oper_chwidth(hapd->iconf);
	switch (chwidth) {
	case CONF_OPER_CHWIDTH_USE_HT:
		ret = os_snprintf(buf + len, buflen - len,
				  "[%s]",
				  hapd->iconf->secondary_channel ? "40" : "20");
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		ret = os_snprintf(buf + len, buflen - len,
				  "[80]");
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		ret = os_snprintf(buf + len, buflen - len,
				  "[160]");
		break;
	case CONF_OPER_CHWIDTH_320MHZ:
		ret = os_snprintf(buf + len, buflen - len,
				  "[320]");
		break;
	default:
		ret = os_snprintf(buf + len, buflen - len,
				  "[20]");
	}
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}

#endif /* CONFIG_CTRL_IFACE_MIB */

static u8 hostapd_htmaxmcs(const u8 *mcs_set)
{
	u8 rates[WLAN_SUPP_RATES_MAX];
	u8 i;
	u8 j = -1;

	for (i = 0; i < WLAN_SUPP_HT_RATES_MAX; i++) {
		if (j == WLAN_SUPP_RATES_MAX) {
			wpa_printf(MSG_INFO,
				   "HT extended rate set too large; using only %u rates",
				    j);
			break;
		}
		if (mcs_set[i / 8] & (1 << (i % 8)))
			rates[++j] = i;
	}
	if (j > -1)
		return rates[j];

	return 0;
}


static u8 hostapd_vhtmaxmcs(u16 rx_vht_mcs_map, u16 tx_vht_mcs_map)
{
	u8 rx_max_mcs, tx_max_mcs, max_mcs;

	if (rx_vht_mcs_map && tx_vht_mcs_map) {
		/* Refer to IEEE P802.11ac/D7.0 Figure 8-401bs
		 * for VHT MCS Map definition
		 */
		rx_max_mcs = rx_vht_mcs_map & 0x03;
		tx_max_mcs = tx_vht_mcs_map & 0x03;
		max_mcs = rx_max_mcs < tx_max_mcs ? rx_max_mcs : tx_max_mcs;
		if (max_mcs < 0x03)
			return 7 + max_mcs;
	}

	return 0;
}
#ifdef CONFIG_CTRL_IFACE_MIB


static unsigned int hostapd_mcs_map_max_nss(u16 mcs_map,
					    unsigned int max_streams)
{
	unsigned int i;

	for (i = max_streams; i >= 1; i--) {
		if (((mcs_map >> ((i - 1) * MCS_MAP_BITS_PER_NSS)) &
		    MCS_MAP_NSS_MASK) != MCS_MAP_NSS_MASK)
			return i;
	}

	return 0;
}


static u16 hostapd_mcs_map_intersection(u16 ap_map, u16 sta_map,
					unsigned int max_streams)
{
	u16 result = 0;
	u16 ap, sta, val;
	unsigned int i;

	for (i = 0; i < max_streams; i++) {
		ap = (ap_map >> (MCS_MAP_BITS_PER_NSS * i)) & MCS_MAP_NSS_MASK;
		sta = (sta_map >> (MCS_MAP_BITS_PER_NSS * i)) & MCS_MAP_NSS_MASK;

		if (ap == MCS_MAP_NSS_MASK || sta == MCS_MAP_NSS_MASK)
			val = MCS_MAP_NSS_MASK;
		else
			val = ap < sta ? ap : sta;

		result |= val << (MCS_MAP_BITS_PER_NSS * i);
	}

	return result;
}


static unsigned int hostapd_max_legacy_rate(struct hostapd_data *hapd,
					    struct sta_info *sta)
{
	unsigned int max_rate = 0;
	int i;

	if (sta && sta->supported_rates_len > 0) {
		for (i = 0; i < sta->supported_rates_len; i++) {
			unsigned int rate = sta->supported_rates[i] & IEEE80211_RATE_VAL_MASK;

			if (rate > max_rate)
				max_rate = rate;
		}
		return max_rate * IEEE80211_RATE_UNIT_KBPS;
	}

	if (hapd->iface && hapd->iface->current_mode &&
	    hapd->iface->current_mode->num_rates > 0)
		return hapd->iface->current_mode->rates[
			hapd->iface->current_mode->num_rates - 1] * HOSTAPD_MODE_RATE_UNIT_KBPS;

	return 0;
}


static unsigned int hostapd_ht_max_mcs_nss(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   unsigned int *nss)
{
	const u8 *ap_mcs, *sta_mcs;
	unsigned int max_mcs = 0, max_nss = 0, stream;
	int bit;
	u8 supported;

	if (!hapd->iface || !hapd->iface->current_mode || !sta ||
	    !sta->ht_capabilities)
		return 0;

	ap_mcs = hapd->iface->current_mode->mcs_set;
	sta_mcs = sta->ht_capabilities->supported_mcs_set;

	for (stream = 0; stream < 4; stream++) {
		supported = ap_mcs[stream] & sta_mcs[stream];

		for (bit = 7; bit >= 0; bit--) {
			if (supported & BIT(bit)) {
				max_mcs = stream * 8 + bit;
				max_nss = stream + 1;
				break;
			}
		}
	}

	if (nss)
		*nss = max_nss;

	return max_mcs;
}


static unsigned int hostapd_vht_max_mcs_nss(struct hostapd_data *hapd,
					    struct sta_info *sta,
					    unsigned int *nss)
{
	struct hostapd_hw_modes *mode;
	u16 ap_map, sta_map, map;
	unsigned int max_nss;

	if (!hapd->iface || !sta || !sta->vht_capabilities)
		return 0;

	mode = hapd->iface->current_mode;
	if (!mode)
		return 0;

	ap_map = WPA_GET_LE16(&mode->vht_mcs_set[4]);
	sta_map = le_to_host16(sta->vht_capabilities->vht_supported_mcs_set.rx_map);
	map = hostapd_mcs_map_intersection(ap_map, sta_map, VHT_RX_NSS_MAX_STREAMS);
	max_nss = hostapd_mcs_map_max_nss(map, VHT_RX_NSS_MAX_STREAMS);
	if (nss)
		*nss = max_nss;
	if (!max_nss)
		return 0;

	switch ((map >> ((max_nss - 1) * MCS_MAP_BITS_PER_NSS)) & MCS_MAP_NSS_MASK) {
	case 0:
		return VHT_MCS_MAP_0_7_MAX_MCS;
	case 1:
		return VHT_MCS_MAP_0_8_MAX_MCS;
	case 2:
		return VHT_MCS_MAP_0_9_MAX_MCS;
	default:
		return MCS_MAP_NOT_SUPP;
	}
}


static unsigned int hostapd_he_max_mcs_nss(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   unsigned int *nss)
{
	struct hostapd_hw_modes *mode;
	struct he_capabilities *ap_he;
	const u16 *ap_mcs;
	const u8 *sta_mcs;
	u16 map = 0;
	unsigned int max_nss = 0;
	int mcs_count, i;

	if (!hapd->iface || !sta || !sta->he_capab)
		return 0;

	mode = hapd->iface->current_mode;
	if (!mode)
		return 0;

	ap_he = &mode->he_capab[IEEE80211_MODE_AP];
	ap_mcs = (const u16 *) ap_he->mcs;
	sta_mcs = (const u8 *) &sta->he_capab->he_basic_supported_mcs_set;

	switch (hostapd_get_oper_chwidth(hapd->iconf)) {
	case CONF_OPER_CHWIDTH_80P80MHZ:
		mcs_count = HOSTAPD_HE_MCS_MAP_COUNT_80P80;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		mcs_count = HOSTAPD_HE_MCS_MAP_COUNT_160;
		break;
	default:
		mcs_count = HOSTAPD_HE_MCS_MAP_COUNT_20_40_80;
		break;
	}

	for (i = 0; i < mcs_count; i++) {
		map = hostapd_mcs_map_intersection(le_to_host16(ap_mcs[i]),
						  WPA_GET_LE16(&sta_mcs[i * sizeof(u16)]),
						  HE_NSS_MAX_STREAMS);
		max_nss = hostapd_mcs_map_max_nss(map, HE_NSS_MAX_STREAMS);
		if (max_nss)
			break;
	}

	if (nss)
		*nss = max_nss;
	if (!max_nss)
		return 0;

	switch ((map >> ((max_nss - 1) * MCS_MAP_BITS_PER_NSS)) & MCS_MAP_NSS_MASK) {
	case 0:
		return HE_MCS_MAP_0_7_MAX_MCS;
	case 1:
		return HE_MCS_MAP_0_9_MAX_MCS;
	case 2:
		return HE_MCS_MAP_0_11_MAX_MCS;
	default:
		return MCS_MAP_NOT_SUPP;
	}
}


static unsigned int hostapd_eht_max_mcs_nss(struct hostapd_data *hapd,
					    struct sta_info *sta,
					    unsigned int *nss)
{
	struct hostapd_hw_modes *mode;
	struct eht_capabilities *ap_eht;
	const u8 *ap_mcs, *sta_mcs;
	unsigned int max_nss = 0;
	int sets = HOSTAPD_EHT_MCS_NSS_SETS_20MHZ_PLUS;
	int offset;
	u8 val;

	if (!hapd->iface || !sta || !sta->eht_capab || !sta->he_capab)
		return 0;

	mode = hapd->iface->current_mode;
	if (!mode)
		return 0;

	ap_eht = &mode->eht_capab[IEEE80211_MODE_AP];
	ap_mcs = ap_eht->mcs;
	sta_mcs = sta->eht_capab->optional;

	switch (hostapd_get_oper_chwidth(hapd->iconf)) {
	case CONF_OPER_CHWIDTH_320MHZ:
		sets = HOSTAPD_EHT_MCS_NSS_SETS_320MHZ;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
		sets = HOSTAPD_EHT_MCS_NSS_SETS_160_OR_80P80;
		break;
	default:
		sets = HOSTAPD_EHT_MCS_NSS_SETS_20MHZ_PLUS;
		break;
	}

	for (offset = 0; offset < sets * EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS;
	     offset++) {
		val = ap_mcs[offset] < sta_mcs[offset] ? ap_mcs[offset] :
			sta_mcs[offset];
		if ((val & 0x0f) > max_nss)
			max_nss = val & 0x0f;
		if (((val >> 4) & 0x0f) > max_nss)
			max_nss = (val >> 4) & 0x0f;
	}

	if (nss)
		*nss = max_nss;
	if (!max_nss)
		return 0;

	for (offset = sets * EHT_PHYCAP_MCS_NSS_LEN_20MHZ_PLUS - 1; offset >= 0;
	     offset--) {
		val = ap_mcs[offset] < sta_mcs[offset] ? ap_mcs[offset] :
			sta_mcs[offset];
		if ((val >> 4) >= max_nss)
			return HOSTAPD_EHT_MAX_MCS_13;
		if ((val & 0x0f) >= max_nss)
			return HOSTAPD_EHT_MAX_MCS_11;
	}

	return HOSTAPD_EHT_MAX_MCS_9;
}


static unsigned int hostapd_max_phy_rate_kbps(struct hostapd_data *hapd,
					      struct sta_info *sta)
{
	unsigned int nss = 0, mcs, width;
	unsigned int nsd;
	unsigned int nbpsc;
	unsigned int code_num;
	unsigned int code_den;
	unsigned int tsym_tenths_us;
	unsigned long long bits_per_symbol;
	unsigned long long kbps;

	if (!hapd || !sta || !hapd->iface || !hapd->iface->current_mode)
		return 0;

	width = hostapd_get_oper_chwidth(hapd->iconf);

	if ((sta->flags & WLAN_STA_EHT) && sta->eht_capab) {
		tsym_tenths_us = HOSTAPD_PHY_RATE_TSYM_HE_EHT_TENTHS_US;

		mcs = hostapd_eht_max_mcs_nss(hapd, sta, &nss);
		if (!mcs || !nss)
			return 0;

		switch (width) {
		case CONF_OPER_CHWIDTH_320MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_320MHZ_EHT;
			break;
		case CONF_OPER_CHWIDTH_160MHZ:
		case CONF_OPER_CHWIDTH_80P80MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_160MHZ_HE_EHT;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_80MHZ_HE_EHT;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
			nsd = hapd->iconf->secondary_channel ?
				HOSTAPD_PHY_RATE_NSD_40MHZ_HE_EHT :
				HOSTAPD_PHY_RATE_NSD_20MHZ_HE_EHT;
			break;
		default:
			nsd = HOSTAPD_PHY_RATE_NSD_20MHZ_HE_EHT;
			break;
		}

		switch (mcs) {
		case 0:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_BPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 1:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 2:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 3:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 4:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 5:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_2_3_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_3;
			break;
		case 6:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 7:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		case 8:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_256QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 9:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_256QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		case 10:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_1024QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 11:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_1024QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		case 12:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_4096QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 13:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_4096QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		default:
			return 0;
		}

		bits_per_symbol = (unsigned long long) nsd * nbpsc * code_num;
		kbps = bits_per_symbol * HOSTAPD_PHY_RATE_TENTHS_US_TO_KBPS_SCALE;
		kbps /= (unsigned long long) code_den * tsym_tenths_us;
		kbps = (unsigned int) ((kbps + 50 )/ 100) * 100;

		return kbps * nss;
	}

	if ((sta->flags & WLAN_STA_HE) && sta->he_capab) {
		tsym_tenths_us = HOSTAPD_PHY_RATE_TSYM_HE_EHT_TENTHS_US;

		mcs = hostapd_he_max_mcs_nss(hapd, sta, &nss);
		if (!nss)
			return 0;

		switch (width) {
		case CONF_OPER_CHWIDTH_160MHZ:
		case CONF_OPER_CHWIDTH_80P80MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_160MHZ_HE_EHT;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_80MHZ_HE_EHT;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
			nsd = hapd->iconf->secondary_channel ?
				HOSTAPD_PHY_RATE_NSD_40MHZ_HE_EHT :
				HOSTAPD_PHY_RATE_NSD_20MHZ_HE_EHT;
			break;
		default:
			nsd = HOSTAPD_PHY_RATE_NSD_20MHZ_HE_EHT;
			break;
		}

		switch (mcs) {
		case 0:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_BPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 1:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 2:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 3:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 4:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 5:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_2_3_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_3;
			break;
		case 6:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 7:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		case 8:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_256QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 9:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_256QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		case 10:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_1024QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 11:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_1024QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		default:
			return 0;
		}

		bits_per_symbol = (unsigned long long) nsd * nbpsc * code_num * nss;
		kbps = bits_per_symbol * HOSTAPD_PHY_RATE_TENTHS_US_TO_KBPS_SCALE;
		kbps /= (unsigned long long) code_den * tsym_tenths_us;
		kbps = (unsigned int) ((kbps + 50 )/ 100) * 100;

		return kbps;
	}

	if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
		tsym_tenths_us = HOSTAPD_PHY_RATE_TSYM_HT_VHT_TENTHS_US;

		mcs = hostapd_vht_max_mcs_nss(hapd, sta, &nss);
		if (!mcs || !nss)
			return 0;

		switch (width) {
		case CONF_OPER_CHWIDTH_160MHZ:
		case CONF_OPER_CHWIDTH_80P80MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_160MHZ_VHT;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			nsd = HOSTAPD_PHY_RATE_NSD_80MHZ_VHT;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
			nsd = hapd->iconf->secondary_channel ?
				HOSTAPD_PHY_RATE_NSD_40MHZ_HT_VHT :
				HOSTAPD_PHY_RATE_NSD_20MHZ_HT_VHT;
			break;
		default:
			nsd = HOSTAPD_PHY_RATE_NSD_20MHZ_HT_VHT;
			break;
		}

		switch (mcs) {
		case 0:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_BPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 1:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 2:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 3:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 4:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 5:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_2_3_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_3;
			break;
		case 6:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 7:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		case 8:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_256QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 9:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_256QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		default:
			return 0;
		}

		bits_per_symbol = (unsigned long long) nsd * nbpsc * code_num * nss;
		kbps = bits_per_symbol * HOSTAPD_PHY_RATE_TENTHS_US_TO_KBPS_SCALE;
		kbps /= (unsigned long long) code_den * tsym_tenths_us;
		kbps = (unsigned int) ((kbps + 50 )/ 100) * 100;

		return kbps;
	}

	if ((sta->flags & WLAN_STA_HT) && sta->ht_capabilities) {
		tsym_tenths_us = HOSTAPD_PHY_RATE_TSYM_HT_VHT_TENTHS_US;

		mcs = hostapd_ht_max_mcs_nss(hapd, sta, &nss);
		if (!nss)
			return 0;

		nsd = (width == CONF_OPER_CHWIDTH_USE_HT &&
		       hapd->iconf->secondary_channel) ?
			HOSTAPD_PHY_RATE_NSD_40MHZ_HT_VHT :
			HOSTAPD_PHY_RATE_NSD_20MHZ_HT_VHT;

		switch (mcs % 8) {
		case 0:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_BPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 1:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 2:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_QPSK;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 3:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_1_2_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_2;
			break;
		case 4:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_16QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 5:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_2_3_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_3;
			break;
		case 6:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_3_4_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_4;
			break;
		case 7:
			nbpsc = HOSTAPD_PHY_RATE_NBPSC_64QAM;
			code_num = HOSTAPD_PHY_RATE_CODE_5_6_NUMERATOR;
			code_den = HOSTAPD_PHY_RATE_CODE_DENOMINATOR_6;
			break;
		default:
			return 0;
		}

		bits_per_symbol = (unsigned long long) nsd * nbpsc * code_num * nss;
		kbps = bits_per_symbol * HOSTAPD_PHY_RATE_TENTHS_US_TO_KBPS_SCALE;
		kbps /= (unsigned long long) code_den * tsym_tenths_us;
		kbps = (unsigned int) ((kbps + 50 )/ 100) * 100;

		return kbps;
	}

	return hostapd_max_legacy_rate(hapd, sta);
}


static int hostapd_get_sta_info(struct hostapd_data *hapd,
				struct sta_info *sta,
				char *buf, size_t buflen)
{
	struct hostap_sta_driver_data data;
	int ret;
	int len = 0;
	unsigned long long rx_error;
	int rx_mgmt_snr, rx_data_snr;
#ifdef CONFIG_IEEE80211BE
	bool ttlm_active = hostapd_is_ttlm_active(sta);
#endif /* CONFIG_IEEE80211BE */

	if (hostapd_drv_read_sta_data(hapd, &data, sta->addr) < 0)
		return 0;

	rx_error = (unsigned long long)data.pn_errors +
		   (unsigned long long)data.mic_errors +
		   (unsigned long long)data.decrypt_errors;
	rx_mgmt_snr = data.mgmt_signal - hapd->iface->lowest_nf;
	rx_data_snr = data.signal - hapd->iface->lowest_nf;
	ret = os_snprintf(buf, buflen, "rx_packets=%lu\ntx_packets=%lu\n"
			  "rx_bytes=%llu\ntx_bytes=%llu\ninactive_msec=%lu\n"
			  "signal=%d\ntx_failed=%lu\nrx_pn_errors=%u\n"
			  "rx_mic_errors=%u\nrx_decrypt_errors=%u\nrx_errors=%llu\n"
			  "mgmt_signal=%d\nrx_data_snr=%d\nrx_mgmt_snr=%d\n",
			  data.rx_packets, data.tx_packets,
			  data.rx_bytes, data.tx_bytes, data.inactive_msec,
			  data.signal, data.tx_retry_failed, data.pn_errors,
			  data.mic_errors, data.decrypt_errors,
			  rx_error, data.mgmt_signal, rx_data_snr, rx_mgmt_snr);
	if (os_snprintf_error(buflen, ret))
		return 0;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "max_rssi=%d\nmin_rssi=%d\nps_state=%d\n",
			  data.max_rssi, data.min_rssi,
			  data.ps_state);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	if (sta->last_rx_mgmt_rate) {
		ret = os_snprintf(buf + len, buflen - len,
				  "last_rx_mgmt_rate=%lu\n",
				  (unsigned long) (sta->last_rx_mgmt_rate / 100));
		if (os_snprintf_error(buflen - len, ret))
			return 0;
		len += ret;
	}

	if (hapd->iconf) {
		ret = os_snprintf(buf + len, buflen - len,
				  "channel=%u\nOperating_class=%u\n",
				  hapd->iconf->channel,
				  hapd->iconf->op_class);
		if (os_snprintf_error(buflen - len, ret))
			return 0;
		len += ret;
	}

	if (hapd->iface) {
		if (is_6ghz_freq(hapd->iface->freq))
			ret = os_snprintf(buf + len, buflen - len,
					  "band=6GHz\n");
		else if (is_5ghz_freq(hapd->iface->freq))
			ret = os_snprintf(buf + len, buflen - len,
					  "band=5GHz\n");
		else if (is_24ghz_freq(hapd->iface->freq))
			ret = os_snprintf(buf + len, buflen - len,
					  "band=2.4GHz\n");
		else
			ret = os_snprintf(buf + len, buflen - len,
					  "band=NA\n");
		if (os_snprintf_error(buflen - len, ret))
			return 0;
		len += ret;
	}

	ret = os_snprintf(buf + len, buflen - len,
			  "MLO=%s\nmax_tx_power=%u\nmin_tx_power=%u\n",
			  (sta->flags & WLAN_STA_EHT) ? "yes" : "no",
			  sta->max_tx_power,
			  sta->min_tx_power);
	if (os_snprintf_error(buflen - len, ret))
		return 0;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "mu_capable=%s\n",
			  (hapd->iconf->he_phy_capab.he_su_beamformee ||
			  (hapd->iconf->vht_capab & VHT_CAP_MU_BEAMFORMEE_CAPABLE)) ?
			  "yes" : "no");
	if (os_snprintf_error(buflen - len, ret))
		return 0;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "ERP=%u\n", ieee802_11_erp_info(hapd));
	if (os_snprintf_error(buflen - len, ret))
		return 0;
	len += ret;

#ifdef CONFIG_IEEE80211BE
	ret = os_snprintf(buf + len, buflen - len,
			  "ttlm_active=%s\n", ttlm_active ? "yes" : "no");
	if (os_snprintf_error(buflen - len, ret))
		return 0;
	len += ret;
#endif /* CONFIG_IEEE80211BE */

	ret = os_snprintf(buf + len, buflen - len,
			  "HT_capability=%s\nVHT_capability=%s\n",
			  hostapd_is_ht_enabled(hapd) ? "yes" : "no",
			  hostapd_is_vht_enabled(hapd) ? "yes" : "no");
	if (os_snprintf_error(buflen - len, ret))
		return 0;
	len += ret;

	len += hostapd_get_sta_phy_mode(sta, hapd, buf + len, buflen - len);

	ret = os_snprintf(buf + len, buflen - len, "\nrx_rate_info=%lu",
			  data.current_rx_rate / 100);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;
	if (data.flags & STA_DRV_DATA_RX_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " mcs %u",
				  data.rx_mcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_VHT_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtmcs %u",
				  data.rx_vhtmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_VHT_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtnss %u",
				  data.rx_vht_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_SHORT_GI) {
		ret = os_snprintf(buf + len, buflen - len, " shortGI");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_HE_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " hemcs %u",
				  data.rx_hemcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_HE_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " henss %u",
				  data.rx_he_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_HE_GI) {
		const char *gi_str;

		switch (data.rx_guard_interval) {
		case GUARD_INTERVAL_0_8:
			gi_str = "0.8";
			break;
		case GUARD_INTERVAL_1_6:
			gi_str = "1.6";
			break;
		case GUARD_INTERVAL_3_2:
			gi_str = "3.2";
			break;
		default:
			gi_str = "unknown";
			break;
		}
		ret = os_snprintf(buf + len, buflen - len, " hegi %s", gi_str);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_EHT_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " ehtmcs %u",
				  data.rx_ehtmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_EHT_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " ehtnss %u",
				  data.rx_eht_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_EHT_GI) {
		const char *gi_str;

		switch (data.rx_guard_interval) {
		case GUARD_INTERVAL_0_8:
			gi_str = "0.8";
			break;
		case GUARD_INTERVAL_1_6:
			gi_str = "1.6";
			break;
		case GUARD_INTERVAL_3_2:
			gi_str = "3.2";
			break;
		default:
			gi_str = "unknown";
			break;
		}
		ret = os_snprintf(buf + len, buflen - len, " ehtgi %s", gi_str);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

#ifdef CONFIG_DRIVER_NL80211
	char cm_buf[1024];
	size_t cm_len = sizeof(cm_buf);
	int r;
	u8 radio_idx = NL80211_WIPHY_RADIO_ID_MAX;

	/* Extract only the configured antenna masks for current radio */
	if (hapd->iface && hapd->iface->num_multi_hws && hapd->iface->current_hw_info)
		radio_idx = hapd->iface->current_hw_info->hw_idx;

	if (hapd->drv_priv) {
		r = nl80211_get_chain_mask(hapd->drv_priv, radio_idx, cm_buf, cm_len);
		if (r >= 0 && r < (int)cm_len) {
			char *p, *txp, *rxp, *endp;
			unsigned long tx = 0, rx = 0;

			cm_buf[cm_len - 1] = '\0';

			p = os_strstr(cm_buf, "Configured Antennas:");
			if (p) {
				txp = os_strstr(p, "TX ");
				rxp = os_strstr(p, "RX ");
				if (txp)
					tx = strtoul(txp + 3, &endp, 0);
				if (rxp)
					rx = strtoul(rxp + 3, &endp, 0);

				ret = os_snprintf(buf + len, buflen - len,
						  "configured_tx_chain_mask=%#lx\n", tx);
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;

				ret = os_snprintf(buf + len, buflen - len,
						  "configured_rx_chain_mask=%#lx\n", rx);
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
			} else {
				wpa_printf(MSG_DEBUG, "Chain mask info not found in expected format");
			}
		}
	}
#endif /* CONFIG_DRIVER_NL80211 */

	ret = os_snprintf(buf + len, buflen - len, "tx_rate_info=%lu",
			  data.current_tx_rate / 100);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;
	if (data.flags & STA_DRV_DATA_TX_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " mcs %u",
				  data.tx_mcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_VHT_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtmcs %u",
				  data.tx_vhtmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_VHT_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtnss %u",
				  data.tx_vht_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_SHORT_GI) {
		ret = os_snprintf(buf + len, buflen - len, " shortGI");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_HE_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " hemcs %u",
				  data.tx_hemcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_HE_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " henss %u",
				  data.tx_he_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_HE_GI) {
		const char *gi_str;

		switch (data.tx_guard_interval) {
		case GUARD_INTERVAL_0_8:
			gi_str = "0.8";
			break;
		case GUARD_INTERVAL_1_6:
			gi_str = "1.6";
			break;
		case GUARD_INTERVAL_3_2:
			gi_str = "3.2";
			break;
		default:
			gi_str = "unknown";
			break;
		}
		ret = os_snprintf(buf + len, buflen - len, " hegi %s", gi_str);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_EHT_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " ehtmcs %u",
				  data.tx_ehtmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_EHT_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " ehtnss %u",
				  data.tx_eht_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_EHT_GI) {
		const char *gi_str;

		switch (data.tx_guard_interval) {
		case GUARD_INTERVAL_0_8:
			gi_str = "0.8";
			break;
		case GUARD_INTERVAL_1_6:
			gi_str = "1.6";
			break;
		case GUARD_INTERVAL_3_2:
			gi_str = "3.2";
			break;
		default:
			gi_str = "unknown";
			break;
		}
		ret = os_snprintf(buf + len, buflen - len, " ehtgi %s", gi_str);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	ret = os_snprintf(buf + len, buflen - len, "256 QAM support=%s\n",
			  station_supports_256qam(sta) ? "yes" : "no");
	if (os_snprintf_error(buflen - len, ret))
		return 0;
	len += ret;

	if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
		ret = os_snprintf(buf + len, buflen - len,
				  "rx_vht_mcs_map=%04x\n"
				  "tx_vht_mcs_map=%04x\n",
				  le_to_host16(sta->vht_capabilities->
					       vht_supported_mcs_set.rx_map),
				  le_to_host16(sta->vht_capabilities->
					       vht_supported_mcs_set.tx_map));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	if ((sta->flags & WLAN_STA_HT) && sta->ht_capabilities) {
		len = hostapd_write_ht_mcs_bitmask(buf, buflen, len,
						   sta->ht_capabilities->
						   supported_mcs_set);
	}

	if (data.flags & STA_DRV_DATA_LAST_ACK_RSSI) {
		ret = os_snprintf(buf + len, buflen - len,
				  "last_ack_signal=%d\n", data.last_ack_rssi);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	len += hostapd_get_sta_conn_time(sta, &data, buf + len, buflen - len);

	return len;
}


static const char * timeout_next_str(int val)
{
	switch (val) {
	case STA_NULLFUNC:
		return "NULLFUNC POLL";
	case STA_DISASSOC:
		return "DISASSOC";
	case STA_DEAUTH:
		return "DEAUTH";
	case STA_REMOVE:
		return "REMOVE";
	case STA_DISASSOC_FROM_CLI:
		return "DISASSOC_FROM_CLI";
	default:
		return "?";
	}
}

unsigned int operclass_to_uniimask(unsigned int operclass)
{
	if ((operclass >= 115) && (operclass <= 117)) {
		return BAND_UNII_1;
	} else if ((operclass >= 118) && (operclass <= 120)) {
		return BAND_UNII_2A;
	} else if ((operclass >= 121) && (operclass <= 123)) {
		return BAND_UNII_2C;
	} else if (operclass == 124) {
		return BAND_UNII_3;
	} else if ((operclass >= 125) && (operclass <= 127)) {
		return BAND_UNII_4;
	} else if ((operclass >= 128) && (operclass <= 130)) {
		return BAND_UNII_ALL_5G;
	} else if ((operclass >= 131) && (operclass <= 135)) {
		return BAND_UNII_ALL_6G;
	} else if (operclass == 136) {
		return BAND_UNII_5;
	}

	return 0;
}

void check_and_add_uniibands(band_info_t uniiband)
{
	uint8_t i = 0;

	while (i < 8) {
		if (uniibands[i] == uniiband) {
			return;
		}
		else if (uniibands[i] == 0) {
			uniibands[i] = uniiband;
			return;
		}
		i++;
	}
}

#ifdef CONFIG_TAXONOMY
static int is_wpa_oui(const u8 *ie)
{
	if (ie[1] < 4)
		return 0;
	return (WPA_GET_BE32(&ie[2]) == WPA_IE_VENDOR_TYPE);
}

static int is_wmm_oui(const u8 *ie)
{
	if (ie[1] < 4)
		return 0;
	return (WPA_GET_BE32(&ie[2]) == WMM_IE_VENDOR_TYPE);
}

static int print_sta_ies_compact(const u8 *ies, size_t ies_len,
				 char *buf, size_t buflen)
{
	const u8 *pos = ies;
	size_t left = ies_len;
	int len = 0;
	int ret;

	if (!ies || ies_len == 0)
		return 0;

	ret = os_snprintf(buf + len, buflen - len, "IEs=");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	while (left >= 2) {
		u8 id = pos[0];
		u8 elen = pos[1];

		if (2 + elen > left)
			break;

		switch (id) {
		case WLAN_EID_VENDOR_SPECIFIC:
			if (is_wpa_oui(pos)) {
				ret = os_snprintf(buf + len, buflen - len,
						  "[WPA]");
				if (!os_snprintf_error(buflen - len, ret))
					len += ret;
			} else if (is_wmm_oui(pos)) {
				ret = os_snprintf(buf + len, buflen - len,
						  "[WME]");
				if (!os_snprintf_error(buflen - len, ret))
					len += ret;
			}
			break;
		case WLAN_EID_RSN:
			ret = os_snprintf(buf + len, buflen - len,
					  "[RSN]");
			if (!os_snprintf_error(buflen - len, ret))
				len += ret;
			break;
		default:
			break;
		}

		pos += 2 + elen;
		left -= 2 + elen;
	}

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	return len;
}
#endif /* CONFIG_TAXONOMY */
static int hostapd_ctrl_iface_sta_mib(struct hostapd_data *hapd,
				      struct sta_info *sta,
				      char *buf, size_t buflen)
{
	int len, res, ret, i;
	const char *keyid;
	const u8 *dpp_pkhash;

	if (!sta)
		return 0;

	len = 0;
	ret = os_snprintf(buf + len, buflen - len, MACSTR "\nflags=",
			  MAC2STR(sta->addr));
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	ret = ap_sta_flags_txt(sta->flags, buf + len, buflen - len);
	if (ret < 0)
		return len;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len, "\naid=%d\ncapability=0x%x\n"
			  "listen_interval=%d\nsupported_rates=",
			  sta->aid, sta->capability, sta->listen_interval);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	for (i = 0; i < sta->supported_rates_len; i++) {
		ret = os_snprintf(buf + len, buflen - len, "%02x%s",
				  sta->supported_rates[i],
				  i + 1 < sta->supported_rates_len ? " " : "");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	ret = os_snprintf(buf + len, buflen - len, "\ntimeout_next=%s\n",
			  timeout_next_str(sta->timeout_next));
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	if (sta->max_idle_period) {
		ret = os_snprintf(buf + len, buflen - len,
				  "max_idle_period=%d\n", sta->max_idle_period);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	ret = os_snprintf(buf + len, buflen - len, "vendor_oui=%02x:%02x:%02x\n",
			  sta->vendor_oui[0], sta->vendor_oui[1], sta->vendor_oui[2]);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	res = ieee802_11_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = wpa_get_mib_sta(sta->wpa_sm, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = ieee802_1x_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = hostapd_wps_get_mib_sta(hapd, sta->addr, buf + len,
				      buflen - len);
	if (res >= 0)
		len += res;
	res = hostapd_p2p_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

	len += hostapd_get_sta_info(hapd, sta, buf + len, buflen - len);

#ifdef CONFIG_SAE
	if (sta->sae && sta->sae->state == SAE_ACCEPTED) {
		res = os_snprintf(buf + len, buflen - len, "sae_group=%d\n",
				  sta->sae->group);
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->sae && sta->sae->tmp) {
		const u8 *pos;
		unsigned int j, count;
		struct wpabuf *groups = sta->sae->tmp->peer_rejected_groups;

		res = os_snprintf(buf + len, buflen - len,
				  "sae_rejected_groups=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;

		if (groups) {
			pos = wpabuf_head(groups);
			count = wpabuf_len(groups) / 2;
		} else {
			pos = NULL;
			count = 0;
		}
		for (j = 0; pos && j < count; j++) {
			res = os_snprintf(buf + len, buflen - len, "%s%d",
					  j == 0 ? "" : " ", WPA_GET_LE16(pos));
			if (!os_snprintf_error(buflen - len, res))
				len += res;
			pos += 2;
		}

		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_SAE */

	if (sta->vlan_id > 0) {
		res = os_snprintf(buf + len, buflen - len, "vlan_id=%d\n",
				  sta->vlan_id);
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	res = mbo_ap_get_info(sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

	if (sta->supp_op_classes &&
	    buflen - len > (unsigned) (17 + 2 * sta->supp_op_classes[0])) {
		res = os_snprintf(buf + len, buflen - len, "supp_op_classes=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					sta->supp_op_classes + 1,
					sta->supp_op_classes[0]);
		res = os_snprintf(buf + len, buflen - len, "\n");

		u8 *op_classes = sta->supp_op_classes + 1;

		while (*op_classes != 0) {
			uint8_t uniiband = operclass_to_uniimask(*op_classes);
			switch(uniiband) {
			case BAND_UNII_1:
			case BAND_UNII_2A:
			case BAND_UNII_2C:
			case BAND_UNII_3:
			case BAND_UNII_4:
			case BAND_UNII_ALL_5G:
			case BAND_UNII_ALL_6G:
			case BAND_UNII_5:
				check_and_add_uniibands(uniiband);
			default:
				break;
			}
		op_classes++;
		}

		len += os_snprintf(buf + len, buflen - len, "\nuniibands supported=");
		for (int i=0; (i < 8) && (uniibands[i] != 0); i++) {
			switch(uniibands[i]) {
			case BAND_UNII_1:
				ret = os_snprintf(buf + len, buflen - len, "UNII_1 ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_2A:
				ret = os_snprintf(buf + len, buflen - len, "UNII_2A ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_2C:
				ret = os_snprintf(buf + len, buflen - len, "UNII_2C ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_3:
				ret = os_snprintf(buf + len, buflen - len, "UNII_3 ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_4:
				ret = os_snprintf(buf + len, buflen - len, "UNII_4 ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_ALL_5G:
				ret = os_snprintf(buf + len, buflen - len, "UNII_1 UNII_2A "
						"UNII_2C UNII_3 UNII_4 ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_ALL_6G:
				ret = os_snprintf(buf + len, buflen - len, "UNII_5 UNII_6 UNII_7 UNII_8 ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			case BAND_UNII_5:
				ret = os_snprintf(buf + len, buflen - len, "UNII_5 ");
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
				break;
			}
		}

		len += os_snprintf(buf + len, buflen - len, "\n");
	}

	if (sta->power_capab) {
		ret = os_snprintf(buf + len, buflen - len,
				  "min_txpower=%d\n"
				  "max_txpower=%d\n",
				  sta->min_tx_power, sta->max_tx_power);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

#ifdef CONFIG_IEEE80211AX
	if ((sta->flags & WLAN_STA_HE) && sta->he_capab) {
		res = os_snprintf(buf + len, buflen - len, "he_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->he_capab,
					sta->he_capab_len);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BN
	if ((sta->flags & WLAN_STA_UHR) && sta->uhr_capab) {
		res = os_snprintf(buf + len, buflen - len, "uhr_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->uhr_capab,
					sta->uhr_capab_len);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_IEEE80211BE
	if ((sta->flags & WLAN_STA_EHT) && sta->eht_capab) {
		res = os_snprintf(buf + len, buflen - len, "eht_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->eht_capab,
					sta->eht_capab_len);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AC
	if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
		res = os_snprintf(buf + len, buflen - len,
				  "vht_caps_info=0x%08x\n",
				  le_to_host32(sta->vht_capabilities->
					       vht_capabilities_info));
		if (!os_snprintf_error(buflen - len, res))
			len += res;

		res = os_snprintf(buf + len, buflen - len, "vht_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->vht_capabilities,
					sizeof(*sta->vht_capabilities));
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211AC */

	if ((sta->flags & WLAN_STA_HT) && sta->ht_capabilities) {
		res = os_snprintf(buf + len, buflen - len,
				  "ht_caps_info=0x%04x\n",
				  le_to_host16(sta->ht_capabilities->
					       ht_capabilities_info));
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->ext_capability &&
	    buflen - len > (unsigned) (11 + 2 * sta->ext_capability[0])) {
		res = os_snprintf(buf + len, buflen - len, "ext_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					sta->ext_capability + 1,
					sta->ext_capability[0]);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->ifname_wds) {
		ret = os_snprintf(buf + len, buflen - len,
				  "wds_sta_ifname=%s\n", sta->ifname_wds);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	keyid = ap_sta_wpa_get_keyid(hapd, sta);
	if (keyid) {
		ret = os_snprintf(buf + len, buflen - len, "keyid=%s\n", keyid);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	dpp_pkhash = ap_sta_wpa_get_dpp_pkhash(hapd, sta);
	if (dpp_pkhash) {
		ret = os_snprintf(buf + len, buflen - len, "dpp_pkhash=");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		len += wpa_snprintf_hex(buf + len, buflen - len, dpp_pkhash,
					SHA256_MAC_LEN);
		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

#ifdef CONFIG_IEEE80211BE
	if (sta->mld_info.mld_sta) {
		u16 mld_sta_capa = sta->mld_info.common_info.mld_capa;
		u16 eml_capa = sta->mld_info.common_info.eml_capa;
		u8 max_simul_links = mld_sta_capa &
			EHT_ML_MLD_CAPA_MAX_NUM_SIM_LINKS_MASK;
		u8 emlsr_supp = eml_capa & EHT_ML_EML_CAPA_EMLSR_SUPP;
		u8 emlmr_supp = eml_capa & EHT_ML_EML_CAPA_EMLMR_SUPP;

		for (i = 0; i < MAX_NUM_MLD_LINKS; ++i) {
			if (!sta->mld_info.links[i].valid)
				continue;
			ret = os_snprintf(
				buf + len, buflen - len,
				"peer_addr[%d]=" MACSTR "\n",
				i, MAC2STR(sta->mld_info.links[i].peer_addr));
			if (!os_snprintf_error(buflen - len, ret))
				len += ret;
		}

		ret = os_snprintf(buf + len, buflen - len,
				  "max_simul_links=%d\nEMLSR_support=%s\nEMLMR_support=%s\n",
				  max_simul_links,emlsr_supp ? "yes" : "no",
				  emlmr_supp ? "yes" : "no");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
#endif /* CONFIG_IEEE80211BE */

	ret = os_snprintf(buf + len, buflen - len, "max_rx_nss=%u\n",
			  hostapd_maxnss(hapd, sta));
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	ret = os_snprintf(buf + len, buflen - len, "max_tx_nss=%u\n",
			  hostapd_tx_maxnss(hapd, sta));
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	ret = os_snprintf(buf + len, buflen - len, "maxphyrate=%u\n",
			  hostapd_max_phy_rate_kbps(hapd, sta));
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

#ifdef CONFIG_IEEE80211AC
	if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
		u8 vht_maxmcs = hostapd_vhtmaxmcs(
				le_to_host16(sta->vht_capabilities->
					vht_supported_mcs_set.rx_map),
				le_to_host16(sta->vht_capabilities->
					vht_supported_mcs_set.tx_map));
		ret = os_snprintf(buf + len, buflen - len, "max_vhtmcs=%u\n",
				vht_maxmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211N
	if ((sta->flags & (WLAN_STA_HT | WLAN_STA_VHT)) == WLAN_STA_HT &&
			sta->ht_capabilities) {
		u8 ht_maxmcs;

		ht_maxmcs = hostapd_htmaxmcs(sta->ht_capabilities->
				supported_mcs_set);
		ret = os_snprintf(buf + len, buflen - len, "max_mcs=%u\n",
				ht_maxmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
#endif /* CONFIG_IEEE80211N */

#ifdef CONFIG_IEEE80211BE
	if (sta->mld_info.mld_sta == true) {
		size_t link_type;

		link_type = sta->mld_info.links[hapd->mld_link_id].nstr_bitmap_len;
		ret = os_snprintf(buf + len, buflen-len, "link_type[%d]=%s\n",
				  hapd->mld_link_id, link_type ? "NSTR" : "STR");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
#endif /* CONFIG_IEEE80211BE*/


#ifdef CONFIG_TAXONOMY
	if (sta->assoc_ie_taxonomy) {
		const u8 *ies = wpabuf_head(sta->assoc_ie_taxonomy);
		size_t ies_len = wpabuf_len(sta->assoc_ie_taxonomy);
		int res;
		res = print_sta_ies_compact(ies, ies_len, buf + len, buflen - len);
		if (res > 0)
			len += res;
	}
#endif /* CONFIG_TAXONOMY */
	return len;
}


int hostapd_ctrl_iface_sta_first(struct hostapd_data *hapd,
				 char *buf, size_t buflen)
{
	return hostapd_ctrl_iface_sta_mib(hapd, hapd->sta_list, buf, buflen);
}


int hostapd_ctrl_iface_sta(struct hostapd_data *hapd, const char *txtaddr,
			   char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	int ret;
	const char *pos;
	struct sta_info *sta;

	if (hwaddr_aton(txtaddr, addr)) {
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return 0;
		return ret;
	}

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL)
		return -1;

	pos = os_strchr(txtaddr, ' ');
	if (pos) {
		pos++;

#ifdef HOSTAPD_DUMP_STATE
		if (os_strcmp(pos, "eapol") == 0) {
			if (sta->eapol_sm == NULL)
				return -1;
			return eapol_auth_dump_state(sta->eapol_sm, buf,
						     buflen);
		}
#endif /* HOSTAPD_DUMP_STATE */

		return -1;
	}

	ret = hostapd_ctrl_iface_sta_mib(hapd, sta, buf, buflen);
	ret += fst_ctrl_iface_mb_info(addr, buf + ret, buflen - ret);

	return ret;
}


int hostapd_ctrl_iface_sta_next(struct hostapd_data *hapd, const char *txtaddr,
				char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	int ret;

	if (hwaddr_aton(txtaddr, addr) ||
	    (sta = ap_get_sta(hapd, addr)) == NULL) {
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return 0;
		return ret;
	}

	if (!sta->next)
		return 0;

	return hostapd_ctrl_iface_sta_mib(hapd, sta->next, buf, buflen);
}

#endif

#ifdef CONFIG_P2P_MANAGER
static int p2p_manager_disconnect(struct hostapd_data *hapd, u16 stype,
				  u8 minor_reason_code, const u8 *addr)
{
	struct ieee80211_mgmt *mgmt;
	int ret;
	u8 *pos;

	mgmt = os_zalloc(sizeof(*mgmt) + 100);
	if (mgmt == NULL)
		return -1;

	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, stype);
	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "P2P: Disconnect STA " MACSTR
		" with minor reason code %u (stype=%u (%s))",
		MAC2STR(addr), minor_reason_code, stype,
		fc2str(le_to_host16(mgmt->frame_control)));

	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	if (stype == WLAN_FC_STYPE_DEAUTH) {
		mgmt->u.deauth.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = mgmt->u.deauth.variable;
	} else {
		mgmt->u.disassoc.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = mgmt->u.disassoc.variable;
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = 4 + 3 + 1;
	WPA_PUT_BE32(pos, P2P_IE_VENDOR_TYPE);
	pos += 4;

	*pos++ = P2P_ATTR_MINOR_REASON_CODE;
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	*pos++ = minor_reason_code;

	ret = hostapd_drv_send_mlme(hapd, mgmt, pos - (u8 *) mgmt, 0, NULL, 0,
				    0, 0, 0);
	os_free(mgmt);

	return ret < 0 ? -1 : 0;
}
#endif /* CONFIG_P2P_MANAGER */


int hostapd_ctrl_iface_deauthenticate(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;
	u16 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DEAUTHENTICATE %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " reason=");
	if (pos)
		reason = atoi(pos + 8);

	pos = os_strstr(txtaddr, " test=");
	if (pos) {
		struct ieee80211_mgmt mgmt;
		int encrypt;

		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DEAUTH);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.deauth.reason_code = host_to_le16(reason);
		if (hostapd_drv_send_mlme(hapd, (u8 *) &mgmt,
					  IEEE80211_HDRLEN +
					  sizeof(mgmt.u.deauth),
					  0, NULL, 0, !encrypt, 0, 0) < 0)
			return -1;
		return 0;
	}

#ifdef CONFIG_P2P_MANAGER
	pos = os_strstr(txtaddr, " p2p=");
	if (pos) {
		return p2p_manager_disconnect(hapd, WLAN_FC_STYPE_DEAUTH,
					      atoi(pos + 5), addr);
	}
#endif /* CONFIG_P2P_MANAGER */

	sta = ap_get_sta(hapd, addr);
	if (os_strstr(txtaddr, " tx=0")) {
		hostapd_drv_sta_remove(hapd, addr);
		if (sta)
			ap_free_sta(hapd, sta);
	} else {
		hostapd_drv_sta_deauth(hapd, addr, reason);
		if (sta)
			ap_sta_deauthenticate(hapd, sta, reason);
		else if (addr[0] == 0xff)
			hostapd_free_stas(hapd);
	}

	return 0;
}


int hostapd_ctrl_iface_disassociate(struct hostapd_data *hapd,
				    const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;
	u16 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DISASSOCIATE %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " reason=");
	if (pos)
		reason = atoi(pos + 8);

	pos = os_strstr(txtaddr, " test=");
	if (pos) {
		struct ieee80211_mgmt mgmt;
		int encrypt;

		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DISASSOC);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.disassoc.reason_code = host_to_le16(reason);
		if (hostapd_drv_send_mlme(hapd, (u8 *) &mgmt,
					  IEEE80211_HDRLEN +
					  sizeof(mgmt.u.deauth),
					  0, NULL, 0, !encrypt, 0, 0) < 0)
			return -1;
		return 0;
	}

#ifdef CONFIG_P2P_MANAGER
	pos = os_strstr(txtaddr, " p2p=");
	if (pos) {
		return p2p_manager_disconnect(hapd, WLAN_FC_STYPE_DISASSOC,
					      atoi(pos + 5), addr);
	}
#endif /* CONFIG_P2P_MANAGER */

	sta = ap_get_sta(hapd, addr);
	if (os_strstr(txtaddr, " tx=0")) {
		hostapd_drv_sta_remove(hapd, addr);
		if (sta)
			ap_free_sta(hapd, sta);
	} else {
		hostapd_drv_sta_disassoc(hapd, addr, reason);
		if (sta)
			ap_sta_disassociate(hapd, sta, reason);
		else if (addr[0] == 0xff)
			hostapd_free_stas(hapd);
	}

	return 0;
}


#ifdef CONFIG_TAXONOMY
int hostapd_ctrl_iface_signature(struct hostapd_data *hapd,
				 const char *txtaddr,
				 char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE SIGNATURE %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	return retrieve_sta_taxonomy(hapd, sta, buf, buflen);
}
#endif /* CONFIG_TAXONOMY */


int hostapd_ctrl_iface_poll_sta(struct hostapd_data *hapd,
				const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE POLL_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	hostapd_drv_poll_client(hapd, hapd->own_addr, addr,
				sta->flags & WLAN_STA_WMM);
	return 0;
}


int hostapd_ctrl_iface_status(struct hostapd_data *hapd, char *buf,
			      size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct hostapd_config *iconf = hapd->iconf;
	int len = 0, ret, j;
	size_t i;

	ret = os_snprintf(buf + len, buflen - len,
			  "state=%s\n"
			  "phy=%s\n"
			  "freq=%d\n"
			  "num_sta_non_erp=%d\n"
			  "num_sta_no_short_slot_time=%d\n"
			  "num_sta_no_short_preamble=%d\n"
			  "olbc=%d\n"
			  "num_sta_ht_no_gf=%d\n"
			  "num_sta_no_ht=%d\n"
			  "num_sta_ht_20_mhz=%d\n"
			  "num_sta_ht40_intolerant=%d\n"
			  "olbc_ht=%d\n"
			  "ht_op_mode=0x%x\n",
			  hostapd_state_text(iface->state),
			  iface->phy,
			  iface->freq,
			  iface->num_sta_non_erp,
			  iface->num_sta_no_short_slot_time,
			  iface->num_sta_no_short_preamble,
			  iface->olbc,
			  iface->num_sta_ht_no_gf,
			  iface->num_sta_no_ht,
			  iface->num_sta_ht_20mhz,
			  iface->num_sta_ht40_intolerant,
			  iface->olbc_ht,
			  iface->ht_op_mode);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	if (mode) {
		ret = os_snprintf(buf + len, buflen - len, "hw_mode=%s\n",
				  hw_mode_str(mode->mode));
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (iconf->country[0] && iconf->country[1]) {
		ret = os_snprintf(buf + len, buflen - len,
				  "country_code=%c%c\ncountry3=0x%X\n",
				  iconf->country[0], iconf->country[1],
				  iconf->country[2]);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (!iface->cac_started || !iface->dfs_cac_ms) {
		ret = os_snprintf(buf + len, buflen - len,
				  "cac_time_seconds=%d\n"
				  "cac_time_left_seconds=N/A\n",
				  iface->dfs_cac_ms / 1000);
	} else {
		/* CAC started and CAC time set - calculate remaining time */
		struct os_reltime now;
		long left_time;

		os_reltime_age(&iface->dfs_cac_start, &now);
		left_time = (long) iface->dfs_cac_ms / 1000 - now.sec;
		ret = os_snprintf(buf + len, buflen - len,
				  "cac_time_seconds=%u\n"
				  "cac_time_left_seconds=%lu\n",
				  iface->dfs_cac_ms / 1000,
				  left_time > 0 ? left_time : 0);
	}
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "channel=%u\n"
			  "edmg_enable=%d\n"
			  "edmg_channel=%d\n"
			  "secondary_channel=%d\n"
			  "ieee80211n=%d\n"
			  "ieee80211ac=%d\n"
			  "ieee80211ax=%d\n"
			  "ieee80211be=%d\n"
			  "ieee80211bn=%d\n"
			  "beacon_int=%u\n"
			  "dtim_period=%d\n",
			  iface->conf->channel,
			  iface->conf->enable_edmg,
			  iface->conf->edmg_channel,
			  hostapd_is_ht_enabled(hapd) ?
			  iface->conf->secondary_channel : 0,
			  hostapd_is_ht_enabled(hapd),
			  hostapd_is_vht_enabled(hapd),
			  hostapd_is_he_enabled(hapd),
			  hostapd_is_eht_enabled(hapd),
			  hostapd_is_uhr_enabled(hapd),
			  iface->conf->beacon_int,
			  hapd->conf->dtim_period);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		ret = os_snprintf(buf + len, buflen - len,
				  "eht_oper_chwidth=%d\n"
				  "eht_oper_centr_freq_seg0_idx=%d\n",
				  iface->conf->eht_oper_chwidth,
				  iface->conf->eht_oper_centr_freq_seg0_idx);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (is_6ghz_op_class(iface->conf->op_class) &&
		    hostapd_get_oper_chwidth(iface->conf) ==
		    CONF_OPER_CHWIDTH_320MHZ) {
			ret = os_snprintf(buf + len, buflen - len,
					  "eht_bw320_offset=%d\n",
					  iface->conf->eht_bw320_offset);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (hapd->iconf->punct_bitmap) {
			ret = os_snprintf(buf + len, buflen - len,
					  "punct_bitmap=0x%x\n",
					  hapd->iconf->punct_bitmap);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (is_6ghz_op_class(iface->conf->op_class)) {
			ret = os_snprintf(buf + len, buflen - len,
					  "puncture_strict_6ghz=%d\n"
					  "punc_eirp_thres_6ghz=%d\n",
					  iface->conf->puncture_strict_6ghz,
					  iface->conf->punc_eirp_thres_6ghz);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (hapd->conf->mld_ap) {
			struct hostapd_data *link_bss;

			ret = os_snprintf(buf + len, buflen - len,
					  "num_links=%d\n",
					  hapd->mld->num_links);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;

			/* Self BSS */
			ret = os_snprintf(buf + len, buflen - len,
					  "link_id=%d\n"
					  "link_addr=" MACSTR "\n",
					  hapd->mld_link_id,
					  MAC2STR(hapd->own_addr));
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;

			/* Partner BSSs */
			for_each_mld_link(link_bss, hapd) {
				if (link_bss == hapd)
					continue;

				ret = os_snprintf(buf + len, buflen - len,
						  "partner_link[%d]=" MACSTR
						  "\n",
						  link_bss->mld_link_id,
						  MAC2STR(link_bss->own_addr));
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
			}

			ret = os_snprintf(buf + len, buflen - len,
					  "ap_mld_type=%s\n",
					  (hapd->iface->mld_mld_capa &
					   EHT_ML_MLD_CAPA_AP_MLD_TYPE_IND_MASK)
					  ? "NSTR" : "STR");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd)) {
		ret = os_snprintf(buf + len, buflen - len,
				  "he_oper_chwidth=%d\n"
				  "he_oper_centr_freq_seg0_idx=%d\n"
				  "he_oper_centr_freq_seg1_idx=%d\n"
				  "he_6ghz_reg_pwr_type=%d\n",
				  iface->conf->he_oper_chwidth,
				  iface->conf->he_oper_centr_freq_seg0_idx,
				  iface->conf->he_oper_centr_freq_seg1_idx,
				  iface->conf->he_6ghz_reg_pwr_type);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (!iconf->he_op.he_bss_color_disabled &&
		    iconf->he_op.he_bss_color) {
			ret = os_snprintf(buf + len, buflen - len,
					  "he_bss_color=%d\n",
					  iconf->he_op.he_bss_color);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		ret = os_snprintf(buf + len, buflen - len,
				  "he_bss_color_collision_detection=%d\n",
				  iconf->he_op.he_bss_color_collision_detection);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		ret = os_snprintf(buf + len, buflen - len,
				  "he_bss_color_cca_count=%u\n",
				  hapd->cca_count);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		ret = os_snprintf(buf + len, buflen - len,
				  "he_bss_color_collision_ap_period=%d\n",
				  hapd->iface->conf->he_bss_color_collision_ap_period);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}
#endif /* CONFIG_IEEE80211AX */

	if (hostapd_is_vht_enabled(hapd)) {
		ret = os_snprintf(buf + len, buflen - len,
				  "vht_oper_chwidth=%d\n"
				  "vht_oper_centr_freq_seg0_idx=%d\n"
				  "vht_oper_centr_freq_seg1_idx=%d\n"
				  "vht_caps_info=%08x\n",
				  iface->conf->vht_oper_chwidth,
				  iface->conf->vht_oper_centr_freq_seg0_idx,
				  iface->conf->vht_oper_centr_freq_seg1_idx,
				  iface->conf->vht_capab);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (hostapd_is_vht_enabled(hapd) && mode) {
		u16 rxmap = WPA_GET_LE16(&mode->vht_mcs_set[0]);
		u16 txmap = WPA_GET_LE16(&mode->vht_mcs_set[4]);

		ret = os_snprintf(buf + len, buflen - len,
				  "rx_vht_mcs_map=%04x\n"
				  "tx_vht_mcs_map=%04x\n",
				  rxmap, txmap);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (mode) {
			u16 rxmap = mode->vht_mcs_set[0] |
				(mode->vht_mcs_set[1] << 8);
			u16 txmap = mode->vht_mcs_set[4] |
				(mode->vht_mcs_set[5] << 8);

			ret = os_snprintf(buf + len, buflen - len,
					"vht_max_mcs=%u\n",
					hostapd_vhtmaxmcs(rxmap, txmap));
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
	}

	if (hostapd_is_ht_enabled(hapd)) {
		ret = os_snprintf(buf + len, buflen - len,
				  "ht_caps_info=%04x\n",
				  hapd->iconf->ht_capab);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}
#ifdef CONFIG_CTRL_IFACE_MIB
	if (hostapd_is_ht_enabled(hapd) && mode) {
		len = hostapd_write_ht_mcs_bitmask(buf, buflen, len,
						   mode->mcs_set);
	}
#endif /* CONFIG_CTRL_IFACE_MIB */
	if (hapd->current_rates && hapd->num_rates) {
		ret = os_snprintf(buf + len, buflen - len, "supported_rates=");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		for (j = 0; j < hapd->num_rates; j++) {
			ret = os_snprintf(buf + len, buflen - len, "%s%02x",
					  j > 0 ? " " : "",
					  hapd->current_rates[j].rate / 5);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (mode && iface->conf->ieee80211n) {
			ret = os_snprintf(buf + len, buflen - len,
					"max_mcs=%u\n",
					hostapd_htmaxmcs(mode->mcs_set));
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
	}

	if (mode && mode->rates && mode->num_rates &&
			mode->num_rates <= WLAN_SUPP_RATES_MAX) {
		ret = os_snprintf(buf + len, buflen - len,
				"max_rate=%u\n",
				mode->rates[mode->num_rates - 1]);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	ret = os_snprintf(buf + len, buflen - len, "max_nss=%u\n",
			hostapd_maxnss(hapd, NULL));
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	for (j = 0; mode && j < mode->num_channels; j++) {
		if (mode->channels[j].freq == iface->freq) {
			ret = os_snprintf(buf + len, buflen - len,
					  "max_txpower=%u\n",
					  mode->channels[j].max_tx_power);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
			break;
		}
	}

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		ret = os_snprintf(buf + len, buflen - len,
				  "bss[%d]=%s\n"
				  "bssid[%d]=" MACSTR "\n"
				  "ssid[%d]=%s\n"
				  "num_sta[%d]=%d\n",
				  (int) i, bss->conf->iface,
				  (int) i, MAC2STR(bss->own_addr),
				  (int) i,
				  wpa_ssid_txt(bss->conf->ssid.ssid,
					       bss->conf->ssid.ssid_len),
				  (int) i, bss->num_sta);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (bss->conf->supported_rates) {
			ret = os_snprintf(buf + len, buflen - len,
					  "supported_rates[%d]=",
					  (int) i);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
			for (j = 0; bss->conf->supported_rates[j] > 0; j++) {
				ret = os_snprintf(buf + len, buflen - len, "%s%d",
						  j ? " " : "",
						  bss->conf->supported_rates[j]);
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
			}
			ret = os_snprintf(buf + len, buflen - len, "\n");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (bss->conf->basic_rates) {
			ret = os_snprintf(buf + len, buflen - len,
					  "basic_rates[%d]=",
					  (int) i);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
			for (j = 0; bss->conf->basic_rates[j] > 0; j++) {
				ret = os_snprintf(buf + len, buflen - len, "%s%d",
						  j ? " " : "",
						  bss->conf->basic_rates[j]);
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
			}
			ret = os_snprintf(buf + len, buflen - len, "\n");
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (bss->conf->rate_type != BEACON_RATE_LEGACY ||
		    bss->conf->beacon_rate) {
			const char *br_type;
			switch (bss->conf->rate_type) {
			case BEACON_RATE_HT:
				br_type = "ht-mcs";
				break;
			case BEACON_RATE_VHT:
				br_type = "vht-mcs";
				break;
			case BEACON_RATE_HE:
				br_type = "he-mcs";
				break;
			case BEACON_RATE_EHT:
				br_type = "eht-mcs";
				break;
			default:
				br_type = "legacy";
				break;
			}
			if (bss->conf->rate_type == BEACON_RATE_LEGACY) {
				ret = os_snprintf(buf + len, buflen - len,
						"beacon_rate[%d]=%u.%u Mbps\n",
						(int) i,
						bss->conf->beacon_rate / 10,
						bss->conf->beacon_rate % 10);
			} else {
				ret = os_snprintf(buf + len, buflen - len,
						"beacon_rate[%d]=%s:%u\n",
						(int) i, br_type,
						bss->conf->beacon_rate);
			}
		} else {
			ret = os_snprintf(buf + len, buflen - len,
					  "beacon_rate[%d]=auto\n",
					  (int) i);
		}
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

#ifdef CONFIG_IEEE80211BE
		if (bss->conf->mld_ap) {
			ret = os_snprintf(buf + len, buflen - len,
					  "mld_addr[%d]=" MACSTR "\n"
					  "mld_id[%d]=%d\n"
					  "mld_link_id[%d]=%d\n",
					  (int) i, MAC2STR(bss->mld->mld_addr),
					  (int) i, hostapd_get_mld_id(bss),
					  (int) i, bss->mld_link_id);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
#endif /* CONFIG_IEEE80211BE */
	}

#ifdef CONFIG_QCN_EXTN
	len = hostapd_ctrl_iface_status_extn(hapd, buf, buflen, len);
#endif /* CONFIG_QCN_EXTN */

	if (hapd->conf->chan_util_avg_period) {
		ret = os_snprintf(buf + len, buflen - len,
				  "chan_util_avg=%u\n",
				  iface->chan_util_average);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (iface->max_mgmt_frm_sz) {
		ret = os_snprintf(buf + len, buflen - len,
				  "max_mgmt_frame_size=%zu\n",
				  iface->max_mgmt_frm_sz);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	return len;
}


int hostapd_parse_freq_params(const char *pos,
			      struct hostapd_freq_params *params,
			      unsigned int freq)
{
	os_memset(params, 0, sizeof(*params));

	if (freq)
		params->freq = freq;
	else
		params->freq = atoi(pos);

	if (params->freq == 0) {
		wpa_printf(MSG_ERROR, "freq_params: invalid freq provided");
		return -1;
	}

#define SET_FREQ_PARAM(str) \
	do { \
		const char *pos2 = os_strstr(pos, " " #str "="); \
		if (pos2) { \
			pos2 += sizeof(" " #str "=") - 1; \
			params->str = atoi(pos2); \
		} \
	} while (0)

	SET_FREQ_PARAM(center_freq1);
	SET_FREQ_PARAM(center_freq2);
	SET_FREQ_PARAM(bandwidth);
	SET_FREQ_PARAM(sec_channel_offset);
	SET_FREQ_PARAM(punct_bitmap);
	SET_FREQ_PARAM(bandwidth_device);
	SET_FREQ_PARAM(center_freq_device);
#ifdef CONFIG_QCN_EXTN
	SET_FREQ_PARAM(skip_cac);
	SET_FREQ_PARAM(rptr_mgr);
#endif
	params->ht_enabled = !!os_strstr(pos, " ht");
	params->vht_enabled = !!os_strstr(pos, " vht");
	params->uhr_enabled = !!os_strstr(pos, " uhr");
	params->eht_enabled = !!os_strstr(pos, " eht") ||
		params->uhr_enabled;
	params->he_enabled = !!os_strstr(pos, " he") ||
		params->eht_enabled;
#undef SET_FREQ_PARAM

	return 0;
}


static struct hostapd_hw_modes * get_target_hw_mode(struct hostapd_iface *iface,
						    int freq)
{
	int i;
	enum hostapd_hw_mode target_mode;
	bool is_6ghz = is_6ghz_freq(freq);

	if (freq < 4000)
		target_mode = HOSTAPD_MODE_IEEE80211G;
	else if (freq > 50000)
		target_mode = HOSTAPD_MODE_IEEE80211AD;
	else
		target_mode = HOSTAPD_MODE_IEEE80211A;

	for (i = 0; i < iface->num_hw_features; i++) {
		struct hostapd_hw_modes *mode;

		mode = &iface->hw_features[i];
		if (mode->mode == target_mode && mode->is_6ghz == is_6ghz)
			return mode;
	}

	return NULL;
}


static bool
hostapd_ctrl_is_freq_in_mode(struct hostapd_hw_modes *mode,
			     struct hostapd_multi_hw_info *current_hw_info,
			     int freq)
{
	struct hostapd_channel_data *chan;
	int i;

	for (i = 0; i < mode->num_channels; i++) {
		chan = &mode->channels[i];

		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;

		if (!chan_in_current_hw_info(current_hw_info, chan))
			continue;

		if (chan->freq == freq)
			return true;
	}
	return false;
}


static int hostapd_ctrl_check_freq_params(struct hostapd_iface *iface,
					  struct hostapd_freq_params *params,
					  u16 punct_bitmap)
{
	u32 start_freq;

	if (is_6ghz_freq(params->freq)) {
		const int bw_idx[] = { 20, 40, 80, 160, 320 };
		int idx, bw;

		/* The 6 GHz band requires HE to be enabled. */
		params->he_enabled = 1;

		if (params->center_freq1) {
			if (params->freq == 5935)
				idx = (params->center_freq1 - 5925) / 5;
			else
				idx = (params->center_freq1 - 5950) / 5;

			bw = center_idx_to_bw_6ghz(idx);
			if (bw < 0 || bw >= (int) ARRAY_SIZE(bw_idx) ||
			    bw_idx[bw] != params->bandwidth)
				return -1;
		}
	} else { /* Non-6 GHz channel */
		/* An EHT STA is also an HE STA as defined in
		 * IEEE Std 802.11be-2024, 4.3.16a (Extremely high throughput
		 * (EHT) STA). */
		if (params->he_enabled || params->eht_enabled) {
			params->he_enabled = 1;
			/* An HE STA is also a VHT STA if operating in the 5 GHz
			 * band and an HE STA is also an HT STA in the 2.4 GHz
			 * band as defined in IEEE Std 802.11ax-2021, 4.3.15a.
			 * A VHT STA is an HT STA as defined in IEEE
			 * Std 802.11, 4.3.15. */
			if (IS_5GHZ(params->freq))
				params->vht_enabled = 1;

			params->ht_enabled = 1;
		}
	}

	switch (params->bandwidth) {
	case 0:
		/* bandwidth not specified: use 20 MHz by default */
		/* fall-through */
	case 20:
		if (params->center_freq1 &&
		    params->center_freq1 != params->freq)
			return -1;

		if (params->center_freq2 || params->sec_channel_offset)
			return -1;

		if (punct_bitmap)
			return -1;
		break;
	case 40:
		if (params->center_freq2 || !params->sec_channel_offset)
			return -1;

		if (punct_bitmap)
			return -1;

		if (!params->center_freq1)
			break;
		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq + 10 != params->center_freq1)
				return -1;
			break;
		case -1:
			if (params->freq - 10 != params->center_freq1)
				return -1;
			break;
		default:
			return -1;
		}
		break;
	case 80:
		if (!params->center_freq1 || !params->sec_channel_offset)
			return 1;

		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq - 10 != params->center_freq1 &&
			    params->freq + 30 != params->center_freq1)
				return 1;
			break;
		case -1:
			if (params->freq + 10 != params->center_freq1 &&
			    params->freq - 30 != params->center_freq1)
				return -1;
			break;
		default:
			return -1;
		}

		if (params->center_freq2 && punct_bitmap)
			return -1;

		/* Adjacent and overlapped are not allowed for 80+80 */
		if (params->center_freq2 &&
		    params->center_freq1 - params->center_freq2 <= 80 &&
		    params->center_freq2 - params->center_freq1 <= 80)
			return 1;
		break;
	case 160:
		if (!params->center_freq1 || params->center_freq2 ||
		    !params->sec_channel_offset)
			return -1;

		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq + 70 != params->center_freq1 &&
			    params->freq + 30 != params->center_freq1 &&
			    params->freq - 10 != params->center_freq1 &&
			    params->freq - 50 != params->center_freq1)
				return -1;
			break;
		case -1:
			if (params->freq + 50 != params->center_freq1 &&
			    params->freq + 10 != params->center_freq1 &&
			    params->freq - 30 != params->center_freq1 &&
			    params->freq - 70 != params->center_freq1)
				return -1;
			break;
		default:
			return -1;
		}
		break;
	case 320:
		if (!params->center_freq1 || params->center_freq2 ||
		    !params->sec_channel_offset)
			return -1;

		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq + 150 != params->center_freq1 &&
			    params->freq + 110 != params->center_freq1 &&
			    params->freq + 70 != params->center_freq1 &&
			    params->freq + 30 != params->center_freq1 &&
			    params->freq - 10 != params->center_freq1 &&
			    params->freq - 50 != params->center_freq1 &&
			    params->freq - 90 != params->center_freq1 &&
			    params->freq - 130 != params->center_freq1)
				return -1;
			break;
		case -1:
			if (params->freq + 130 != params->center_freq1 &&
			    params->freq + 90 != params->center_freq1 &&
			    params->freq + 50 != params->center_freq1 &&
			    params->freq + 10 != params->center_freq1 &&
			    params->freq - 30 != params->center_freq1 &&
			    params->freq - 70 != params->center_freq1 &&
			    params->freq - 110 != params->center_freq1 &&
			    params->freq - 150 != params->center_freq1)
				return -1;
			break;
		}
		break;
	default:
		return -1;
	}

	/* 5 GHz channel sanity: ensure center_freq1 implies a valid
	 * starting 20 MHz primary channel for 80/160/320 MHz.
	 * This avoids DFS deriving non-existent start channels (e.g., 92).
	 */
	if (IS_5GHZ(params->freq) &&
	    (params->bandwidth == 80 ||
	     params->bandwidth == 160 ||
	     params->bandwidth == 320) &&
	    (params->vht_enabled ||
	     params->he_enabled ||
	     params->eht_enabled)) {
		u8 seg0_idx = 0;
		int offset = 0;
		int start_ch;

		if (!params->center_freq1)
			return -1;

		if (ieee80211_freq_to_chan(params->center_freq1, &seg0_idx) ==
		    NUM_HOSTAPD_MODES)
			return -1;

		switch (params->bandwidth) {
		case 80:
			offset = 6;
			break;
		case 160:
			offset = 14;
			break;
		case 320:
			offset = 30;
			break;
		}

		start_ch = seg0_idx - offset;

		/* Validate both start and end channels against the HW channel list.
		 * Checking only start_ch misses cases where the upper half of the
		 * bandwidth block extends beyond the legal channel plan (e.g.
		 * center_freq1=5330/160 MHz: start_ch=52 is valid but end_ch=80
		 * (5400 MHz, gap between U-NII-2A and U-NII-2C, not a valid Wi-Fi
		 * channel) is absent from the HW list).
		 */
		if (iface && iface->current_mode && start_ch > 0) {
			bool found_start = false, found_end = false;
			int end_ch;
			int j;
			/* Last channel in 5 GHz 240 MHz width is channel 144 */
			if (is_5ghz_freq(params->center_freq1) &&
			    params->bandwidth == 320)
				end_ch = seg0_idx + 14;
			else
				end_ch = seg0_idx + offset;

			for (j = 0; j < iface->current_mode->num_channels; j++) {
				struct hostapd_channel_data *c =
							&iface->current_mode->channels[j];
				if (c->flag & HOSTAPD_CHAN_DISABLED)
					continue;

				if (!chan_in_current_hw_info(iface->current_hw_info, c))
					continue;

				if (c->chan == start_ch)
					found_start = true;
				if (c->chan == end_ch)
					found_end = true;
				if (found_start && found_end)
					break;
			}
			if (!found_start || !found_end) {
				wpa_printf(MSG_ERROR,
					   "chanswitch: invalid center_freq1=%d for bandwidth=%d MHz (seg0=%u start_ch=%d end_ch=%d not fully present in HW)",
					   params->center_freq1, params->bandwidth,
					   seg0_idx, start_ch, end_ch);
				return -1;
			}
		}
	}

	if (!punct_bitmap)
		return 0;

	if (!(params->uhr_enabled || params->eht_enabled)) {
		wpa_printf(MSG_ERROR,
			   "Preamble puncturing supported only in EHT and UHR");
		return -1;
	}

	if (params->freq >= 2412 && params->freq <= 2484) {
		wpa_printf(MSG_ERROR,
			   "Preamble puncturing is not supported in 2.4 GHz");
		return -1;
	}

	start_freq = params->center_freq1 - (params->bandwidth / 2);
	if (!is_punct_bitmap_valid(params->bandwidth,
				   (params->freq - start_freq) / 20,
				   punct_bitmap)) {
		wpa_printf(MSG_ERROR, "Invalid preamble puncturing bitmap");
		return -1;
	}

	return 0;
}


int hostapd_parse_csa_settings(struct hostapd_iface *iface,
			       const char *pos,
			       struct csa_settings *settings)
{
	struct hostapd_hw_modes *target_mode;
	char *end;
	int ret;

	os_memset(settings, 0, sizeof(*settings));
	settings->cs_count = strtol(pos, &end, 10);
	settings->power_mode = -1;
	if (pos == end) {
		wpa_printf(MSG_ERROR, "chanswitch: invalid cs_count provided");
		return -1;
	}

	settings->block_tx = !!os_strstr(pos, " blocktx");
	settings->handle_dfs = !!os_strstr(pos, " handle_dfs");

	ret = hostapd_parse_freq_params(end, &settings->freq_params, 0);
	if (ret < 0) {
		wpa_printf(MSG_INFO,
				"chanswitch: failed to parse frequency parameters");
		return ret;
	}

#ifdef CONFIG_IEEE80211BN
	if (settings->freq_params.uhr_enabled && !iface->conf->ieee80211bn) {
		wpa_printf(MSG_ERROR,
			   "chanswitch: UHR is not allowed when VAP is operating in EHT mode");
		return -1;
	}
#endif /* CONFIG_IEEE80211BN */

#define SET_CSA_SETTING_EXT(str) \
	do { \
		const char *pos2 = os_strstr(pos, " " #str "="); \
		if (pos2) { \
			pos2 += sizeof(" " #str "=") - 1; \
			settings->str = atoi(pos2); \
		} \
	} while (0)

	SET_CSA_SETTING_EXT(power_mode);

	if (!is_6ghz_freq(settings->freq_params.freq) &&
	    (settings->power_mode != -1)) {
		wpa_printf(MSG_ERROR,
			   "chanswitch: power mode is not supported for non- 6 GHz frequency");
		return -1;
	}

	if (settings->power_mode < -1 ||
	    settings->power_mode > HE_REG_INFO_6GHZ_AP_TYPE_VLP) {
		wpa_printf(MSG_ERROR, "chanswitch: invalid 6 GHz power_mode provided");
		return -1;
	}

	target_mode = get_target_hw_mode(iface, settings->freq_params.freq);
	if (!target_mode) {
		wpa_printf(MSG_DEBUG,
			   "chanswitch: Invalid frequency settings provided for hw mode");
		return -1;
	}

	if (iface->num_hw_features > 1 &&
	    !hostapd_ctrl_is_freq_in_mode(target_mode, iface->current_hw_info,
					  settings->freq_params.freq)) {
		wpa_printf(MSG_INFO,
			   "chanswitch: Invalid frequency settings provided for multi band phy");
		return -1;
	}

	ret = hostapd_ctrl_check_freq_params(iface, &settings->freq_params,
					     settings->freq_params.punct_bitmap);
	if (ret) {
		wpa_printf(MSG_INFO,
			   "chanswitch: invalid frequency settings provided");
		return ret;
	}

	return 0;
}


int hostapd_ctrl_iface_stop_ap(struct hostapd_data *hapd)
{
	int ret;

	ret = hostapd_drv_stop_ap(hapd);
	if (ret)
		return ret;

	ieee802_11_update_beacon_mbssid(hapd);
	return 0;
}

int hostapd_ctrl_iface_dump_tk(struct hostapd_data *hapd, const char *cmd,
			       char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	int key_idx;
	const char *pos;
	struct wpa_state_machine *sm;
	size_t tk_len = 0;
	u8 tk[WPA_TK_MAX_LEN]; /* Temporal Key (TK) */
	int ret;

	/* Verify hapd is valid */
	if (!hapd) {
		wpa_printf(MSG_ERROR, "DUMP_TK: Invalid hapd pointer");
		return -1;
	}

	/* Parse MAC address - format: <sta_addr> <key_idx> */
	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR, "DUMP_TK: Invalid STA address");
		return os_snprintf(buf, buflen, "FAIL-INVALID-MAC\n");
	}

	/* Skip past MAC address by locating the first space delimiter */
	pos = os_strchr(cmd, ' ');
	if (!pos) {
		wpa_printf(MSG_ERROR, "DUMP_TK: Missing key index");
		return os_snprintf(buf, buflen, "FAIL-MISSING-KEY-IDX\n");
	}
	pos++;
	while (*pos == ' ')
		pos++;

	if (*pos == '\0') {
		wpa_printf(MSG_ERROR, "DUMP_TK: Missing key index");
		return os_snprintf(buf, buflen, "FAIL-MISSING-KEY-IDX\n");
	}

	/* Parse key index */
	key_idx = atoi(pos);
	if (key_idx < 0 || key_idx > 3) {
		wpa_printf(MSG_ERROR, "DUMP_TK: Invalid key index %d", key_idx);
		return os_snprintf(buf, buflen, "FAIL-INVALID-KEY-IDX\n");
	}

	/* Look up station */
	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "DUMP_TK: Station " MACSTR " not found",
			   MAC2STR(addr));
		return os_snprintf(buf, buflen, "FAIL-STA-NOT-FOUND\n");
	}

#ifdef CONFIG_ENC_ASSOC
	if (sta->epp_sta) {
		/* Get key from pasn data */
		struct pasn_data *pasn;
		struct hostapd_data *assoc_hapd;
		struct sta_info *assoc_sta =
			hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);

		switch (sta->auth_alg) {
		case WLAN_AUTH_EPPKE:
			if (assoc_sta)
				pasn = assoc_sta->pasn;
			else
				pasn = sta->pasn;

			if (!pasn) {
				wpa_printf(MSG_DEBUG, "DUMP_TK: No PASN data for " MACSTR,
					   MAC2STR(addr));
				return os_snprintf(buf, buflen, "FAIL-NO-PASN-DATA\n");
			}
			tk_len = pasn->ptk.tk_len;
			os_memcpy(tk, pasn->ptk.tk, tk_len);
			break;
#ifdef CONFIG_IEEE8021X_AUTH
		case WLAN_AUTH_802_1X:
			if (assoc_sta) {
				tk_len = assoc_sta->eap_auth_data.ptk.tk_len;
				os_memcpy(tk, assoc_sta->eap_auth_data.ptk.tk, tk_len);
			} else {
				tk_len = sta->eap_auth_data.ptk.tk_len;
				os_memcpy(tk, sta->eap_auth_data.ptk.tk, tk_len);
			}
			break;
#endif /* CONFIG_IEEE8021X_AUTH */
		default:
			wpa_printf(MSG_ERROR, "Unsupported Auth algo "
				   "for an EPP station");
			return -1;
		}
	} else
#endif /* CONFIG_ENC_ASSOC */
	{
		/* Get key from WPA state machine */
		sm = sta->wpa_sm;
		if (!sm) {
			wpa_printf(MSG_DEBUG, "DUMP_TK: No WPA state machine for " MACSTR,
				   MAC2STR(addr));
			return os_snprintf(buf, buflen, "FAIL-NO-WPA-SM\n");
		}
		tk_len = sm->PTK.tk_len;
		os_memcpy(tk, sm->PTK.tk, tk_len);
	}
	/* Get TK length and verify it's installed */
	if (tk_len == 0) {
		wpa_printf(MSG_DEBUG, "DUMP_TK: No TK installed for " MACSTR,
			   MAC2STR(addr));
		return os_snprintf(buf, buflen, "FAIL-NO-TK\n");
	}
	/* Return TK in hex format with newline */
	ret = wpa_snprintf_hex(buf, buflen, tk, tk_len);
	if (ret > 0 && (size_t) ret < buflen - 1) {
		buf[ret] = '\n';
		buf[ret + 1] = '\0';
		ret++;
	}

	return ret;
}


int hostapd_ctrl_iface_pmksa_list(struct hostapd_data *hapd, char *buf,
				  size_t len)
{
	return wpa_auth_pmksa_list(hapd->wpa_auth, buf, len);
}


void hostapd_ctrl_iface_pmksa_flush(struct hostapd_data *hapd)
{
	wpa_auth_pmksa_flush(hapd->wpa_auth);
}


int hostapd_ctrl_iface_pmksa_add(struct hostapd_data *hapd, char *cmd)
{
	u8 spa[ETH_ALEN];
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	char *pos, *pos2;
	int akmp = 0, expiration = 0;
	int ret;

	/*
	 * Entry format:
	 * <STA addr> <PMKID> <PMK> <expiration in seconds> <akmp>
	 */

	if (hwaddr_aton(cmd, spa))
		return -1;

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	if (hexstr2bin(pos, pmkid, PMKID_LEN) < 0)
		return -1;

	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	pos2 = os_strchr(pos, ' ');
	if (!pos2)
		return -1;
	pmk_len = (pos2 - pos) / 2;
	if (pmk_len < PMK_LEN || pmk_len > PMK_LEN_MAX ||
	    hexstr2bin(pos, pmk, pmk_len) < 0)
		return -1;

	pos = pos2 + 1;

	if (sscanf(pos, "%d %d", &expiration, &akmp) != 2)
		return -1;

	ret = wpa_auth_pmksa_add2(hapd->wpa_auth, spa, pmk, pmk_len,
				  pmkid, expiration, akmp, NULL, false);
	if (ret)
		return ret;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		ret = wpa_auth_pmksa_add2(hapd->wpa_auth, spa, pmk, pmk_len,
					  pmkid, expiration, akmp, NULL, true);
#endif /* CONFIG_IEEE80211BE */

	return ret;
}


#ifdef CONFIG_PMKSA_CACHE_EXTERNAL
#ifdef CONFIG_MESH

int hostapd_ctrl_iface_pmksa_list_mesh(struct hostapd_data *hapd,
				       const u8 *addr, char *buf, size_t len)
{
	return wpa_auth_pmksa_list_mesh(hapd->wpa_auth, addr, buf, len);
}


void * hostapd_ctrl_iface_pmksa_create_entry(const u8 *aa, char *cmd)
{
	u8 spa[ETH_ALEN];
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN_MAX];
	char *pos;
	int expiration;

	/*
	 * Entry format:
	 * <BSSID> <PMKID> <PMK> <expiration in seconds>
	 */

	if (hwaddr_aton(cmd, spa))
		return NULL;

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return NULL;
	pos++;

	if (hexstr2bin(pos, pmkid, PMKID_LEN) < 0)
		return NULL;

	pos = os_strchr(pos, ' ');
	if (!pos)
		return NULL;
	pos++;

	if (hexstr2bin(pos, pmk, PMK_LEN) < 0)
		return NULL;

	pos = os_strchr(pos, ' ');
	if (!pos)
		return NULL;
	pos++;

	if (sscanf(pos, "%d", &expiration) != 1)
		return NULL;

	return wpa_auth_pmksa_create_entry(aa, spa, pmk, PMK_LEN,
					   WPA_KEY_MGMT_SAE, pmkid, expiration);
}

#endif /* CONFIG_MESH */
#endif /* CONFIG_PMKSA_CACHE_EXTERNAL */


#ifdef CONFIG_WNM_AP

int hostapd_ctrl_iface_disassoc_imminent(struct hostapd_data *hapd,
					 const char *cmd)
{
	u8 addr[ETH_ALEN];
	int disassoc_timer;
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;
	if (cmd[17] != ' ')
		return -1;
	disassoc_timer = atoi(cmd + 17);

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for disassociation imminent message",
			   MAC2STR(addr));
		return -1;
	}

	return wnm_send_disassoc_imminent(hapd, sta, disassoc_timer);
}


int hostapd_ctrl_iface_ess_disassoc(struct hostapd_data *hapd,
				    const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *url, *timerstr;
	int disassoc_timer;
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for ESS disassociation imminent message",
			   MAC2STR(addr));
		return -1;
	}

	timerstr = cmd + 17;
	if (*timerstr != ' ')
		return -1;
	timerstr++;
	disassoc_timer = atoi(timerstr);
	if (disassoc_timer < 0 || disassoc_timer > 65535)
		return -1;

	url = os_strchr(timerstr, ' ');
	if (url == NULL)
		return -1;
	url++;

	return wnm_send_ess_disassoc_imminent(hapd, sta, url, disassoc_timer);
}

#ifdef CONFIG_IEEE80211BE
static int hostapd_parse_candidate_partner_links(struct hostapd_data *hapd,
						 const char *pos, u8 *nei_rep,
						 size_t nei_rep_len)
{
	struct hostapd_data *partner_link;
	const char *tmp, *end;
	u8 *nei_rep_pos = nei_rep;
#ifdef CONFIG_QCN_EXTN
	u16 repurposed_links = 0;
#endif /* CONFIG_QCN_EXTN */
	int rem_nei_len = nei_rep_len;
	int len = 0;
	int pref;

#ifdef CONFIG_QCN_EXTN
	/* If BSS is repurposed link of the AP MLD, then user triggered the
	 * command on wrong link. Do not add the partner links in this case.
	 */
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
		wpa_printf(MSG_DEBUG,
			   "Skip parse candidate partners as BSS is repurposed");
		return len;
	}
	if (hapd->conf->mld_ap)
		hostapd_get_repurposed_links_bitmap_extn(hapd, &repurposed_links);
#endif /* CONFIG_QCN_EXTN */

	tmp = os_strstr(pos, " partner_link_pref=");
	if (tmp) {
		pos = tmp + 19;
		pref = atoi(pos);
		for_each_mld_link(partner_link, hapd) {
			if (hapd == partner_link)
				continue;

			len += hostapd_add_candidate_own(partner_link, pref,
							 NULL,
							 0,
							 nei_rep_pos,
							 rem_nei_len);
			nei_rep_pos = nei_rep + len;
			rem_nei_len = nei_rep_len - len;
		}
		return len;
	}

	while (pos) {
		u8 link_set[MAX_NUM_MLD_LINKS];
		u8 num_links = 0;
		int i;

		link_set[0] = 255;

		pos = os_strstr(pos, " partner_link_set=");
		if (!pos)
			break;

		/* Candidate preference */
		pos += 18;
		pref = atoi(pos);

		/* Candidate MLD link set */
		end = os_strchr(pos, ' ');
		for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
			tmp = os_strchr(pos, ',');
			if (tmp && (!end || tmp < end)) {
				pos = tmp + 1;
#ifdef CONFIG_QCN_EXTN
				/* check if atoi(pos) is the link id of
				 * repurposed link, if so, return error as we
				 * have request with invalid link
				 */
				if (BIT(atoi(pos)) & repurposed_links) {
					wpa_printf(MSG_ERROR,
						   "link set has repurposed link, fail");
					return -1;
				}
#endif /* CONFIG_QCN_EXTN */
				link_set[i] = atoi(pos);
				num_links++;
			} else {
				pos = end;
				break;
			}
		}

		if (link_set[0] == 255)
			return -1;

		for_each_mld_link(partner_link, hapd) {
			if (partner_link->mld_link_id != link_set[0])
				continue;

			len += hostapd_add_candidate_own(partner_link, pref,
							 link_set,
							 num_links,
							 nei_rep_pos,
							 rem_nei_len);
			nei_rep_pos = nei_rep + len;
			rem_nei_len = nei_rep_len - len;
		}
	}

	return len;
}

/**
 * hostapd_parse_channel_usage_settings - Parse channel usage settings
 * from given input command string
 *
 * @pos: Input string containing channel usage configuration
 * @cfg: Pointer to output configuration structure to be filled
 *
 * This function parses a space-separated string representing multiple
 * channel usage elements and populates the cfg structure with its
 * respective fields - mode, num_entry, and channel entry fields.
 *
 * Returns: 0 on success, -1 on failure
 */

int hostapd_parse_channel_usage_settings(const char *pos,
					  struct channel_usage_config *cfg)
{
	unsigned long val;
	int num_entries;
	int i, j;
	char *end;

	if (!pos) {
		wpa_printf(MSG_ERROR, "Null input string.");
		return -1;
	}

	/* Parse number of Channel Usage elements */
	val = strtoul(pos, &end, 10);
	if (pos == end) {
		wpa_printf(MSG_ERROR, "Failed to parse num_elems");
		return -1;
	}

	if (val < 0 || val > MAX_CHANNEL_USAGE_ELEMENTS) {
		wpa_printf(MSG_ERROR, "Invalid num_elems: %lu", val);
		return -1;
	}
	cfg->num_elems = (u8)val;

	if (cfg->num_elems == 0) {
		wpa_printf(MSG_INFO, "Clearing all Channel Usage elements.");
		os_memset(cfg->elems, 0, sizeof(cfg->elems));
		return 0;
	}

	pos = end;
	/* Parse each Channel Usage element */
	for (i = 0; i < cfg->num_elems; i++) {
		/* Skip spaces */
		while (*pos == ' ')
			pos++;

		if (os_strncmp(pos, "mode", 4) != 0) {
			wpa_printf(MSG_ERROR, "Missing 'mode' at element %d", i + 1);
			return -1;
		}

		pos += 4; /* Move past 'mode' */
		val = strtoul(pos, &end, 10);
		if (pos == end) {
			wpa_printf(MSG_ERROR, "Invalid mode at element %d", i + 1);
			return -1;
		}
		cfg->elems[i].mode = (u8)val;
		pos = end;

		while (*pos == ' ')
			pos++;

		if (os_strncmp(pos, "num_entry", 9) != 0) {
			wpa_printf(MSG_ERROR, "Missing 'num_entry' at element %d", i + 1);
			return -1;
		}

		pos += 9; /* Move past 'num_entry' */
		val = strtoul(pos, &end, 10);
		if (pos == end || val < 1 || val > MAX_CHANNEL_ENTRIES_PER_ELEMENT) {
			wpa_printf(MSG_ERROR, "Invalid num_entry count at element %d",
				   i + 1);
			return -1;
		}
		num_entries = (u8)val;
		cfg->elems[i].num_entries = num_entries;
		pos = end;

		wpa_printf(MSG_DEBUG, "mode: %d num_entries: %d", cfg->elems[i].mode,
			   cfg->elems[i].num_entries);

		/* Parse individual channel entries */
		for (j = 0; j < num_entries; j++) {
			while (*pos == ' ')
				pos++;
			val = strtoul(pos, &end, 10);
			if (pos == end) {
				wpa_printf(MSG_ERROR,
					   "Missing op_class for entry %d of mode %d",
				           j + 1, cfg->elems[i].mode);
				return -1;
			}

			cfg->elems[i].entries[j].op_class = (u8)val;
			pos = end;

			while (*pos == ' ')
				pos++;
			val = strtoul(pos, &end, 10);
			if (pos == end) {
				wpa_printf(MSG_ERROR,
					   "Missing channel for entry %d of mode %d",
					   j + 1, cfg->elems[i].mode);
				return -1;
			}

			cfg->elems[i].entries[j].channel = (u8)val;
			pos = end;

			wpa_printf(MSG_DEBUG, "Entry %d: op_class=%d channel=%d",
				   j, cfg->elems[i].entries[j].op_class,
			           cfg->elems[i].entries[j].channel);
		}
	}
	return 0;
}

#endif /* CONFIG_IEEE80211BE */

int hostapd_ctrl_iface_bss_tm_req(struct hostapd_data *hapd,
				  const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *pos, *end;
	int disassoc_timer = 0;
	struct sta_info *sta;
	u8 req_mode = 0, valid_int = 0x01, dialog_token = 0x01;
	u8 bss_term_dur[12];
	char *url = NULL;
	int ret;
	u8 nei_rep[1000];
	int nei_len = 0;
	u8 *nei_rep_pos = nei_rep;
	int rem_nei_len = sizeof(nei_rep);
	u8 mbo[10];
	size_t mbo_len = 0;
	void *non_pref_chan = NULL;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_DEBUG, "Invalid STA MAC address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta && hapd->mld)
		sta = ap_get_link_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for BSS TM Request message",
			   MAC2STR(addr));
		return -1;
	}

	pos = os_strstr(cmd, " disassoc_timer=");
	if (pos) {
		pos += 16;
		disassoc_timer = atoi(pos);
		if (disassoc_timer < 0 || disassoc_timer > 65535) {
			wpa_printf(MSG_DEBUG, "Invalid disassoc_timer");
			return -1;
		}
	}

	pos = os_strstr(cmd, " valid_int=");
	if (pos) {
		pos += 11;
		valid_int = atoi(pos);
	}

	pos = os_strstr(cmd, " dialog_token=");
	if (pos) {
		pos += 14;
		dialog_token = atoi(pos);
	}

	pos = os_strstr(cmd, " bss_term=");
	if (pos) {
		pos += 10;
		req_mode |= WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED;
		/* TODO: TSF learnable */
		bss_term_dur[0] = 4; /* Subelement ID */
		bss_term_dur[1] = 10; /* Length */
		bss_term_dur[2] = atoi(pos); /* TSF */
		 os_memset(&bss_term_dur[3], 0, 7);
		end = os_strchr(pos, ',');
		if (end == NULL) {
			wpa_printf(MSG_DEBUG, "Invalid bss_term data");
			return -1;
		}
		end++;
		WPA_PUT_LE16(&bss_term_dur[10], atoi(end));
	}

#ifdef CONFIG_MBO
	non_pref_chan = (void *)sta->non_pref_chan;
#endif /* CONFIG_MBO */

#ifdef CONFIG_IEEE80211BE
	nei_len = hostapd_parse_candidate_partner_links(hapd, cmd,
							nei_rep,
							sizeof(nei_rep));
	if (nei_len < 0)
		return -1;

	nei_rep_pos += nei_len;
	rem_nei_len = sizeof(nei_rep) - nei_len;
#endif /* CONFIG_IEEE80211BE */

	nei_len += ieee802_11_parse_candidate_list(cmd, non_pref_chan,
						   nei_rep_pos,
						   rem_nei_len);
	if (nei_len < 0)
		return -1;

	pos = os_strstr(cmd, " url=");
	if (pos) {
		size_t len;
		pos += 5;
		end = os_strchr(pos, ' ');
		if (end)
			len = end - pos;
		else
			len = os_strlen(pos);
		url = os_malloc(len + 1);
		if (url == NULL)
			return -1;
		os_memcpy(url, pos, len);
		url[len] = '\0';
		req_mode |= WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT;
	}

	if (os_strstr(cmd, " pref=1"))
		req_mode |= WNM_BSS_TM_REQ_PREF_CAND_LIST_INCLUDED;
	if (os_strstr(cmd, " abridged=1"))
		req_mode |= WNM_BSS_TM_REQ_ABRIDGED;
	if (os_strstr(cmd, " disassoc_imminent=1"))
		req_mode |= WNM_BSS_TM_REQ_DISASSOC_IMMINENT;
	if (os_strstr(cmd, " link_removal_imminent=1"))
		req_mode |= WNM_BSS_TM_REQ_LINK_REMOVAL_IMMINENT;

#ifdef CONFIG_MBO
	pos = os_strstr(cmd, "mbo=");
	if (pos) {
		unsigned int mbo_reason, cell_pref, reassoc_delay;
		u8 *mbo_pos = mbo;

		ret = sscanf(pos, "mbo=%u:%u:%u", &mbo_reason,
			     &reassoc_delay, &cell_pref);
		if (ret != 3) {
			wpa_printf(MSG_DEBUG,
				   "MBO requires three arguments: mbo=<reason>:<reassoc_delay>:<cell_pref>");
			ret = -1;
			goto fail;
		}

		if (mbo_reason > MBO_TRANSITION_REASON_PREMIUM_AP) {
			wpa_printf(MSG_DEBUG,
				   "Invalid MBO transition reason code %u",
				   mbo_reason);
			ret = -1;
			goto fail;
		}

		/* Valid values for Cellular preference are: 0, 1, 255 */
		if (cell_pref != 0 && cell_pref != 1 && cell_pref != 255) {
			wpa_printf(MSG_DEBUG,
				   "Invalid MBO cellular capability %u",
				   cell_pref);
			ret = -1;
			goto fail;
		}

		if (reassoc_delay > 65535 ||
		    (reassoc_delay &&
		     !(req_mode & WNM_BSS_TM_REQ_DISASSOC_IMMINENT))) {
			wpa_printf(MSG_DEBUG,
				   "MBO: Assoc retry delay is only valid in disassoc imminent mode");
			ret = -1;
			goto fail;
		}

		*mbo_pos++ = MBO_ATTR_ID_TRANSITION_REASON;
		*mbo_pos++ = 1;
		*mbo_pos++ = mbo_reason;
		*mbo_pos++ = MBO_ATTR_ID_CELL_DATA_PREF;
		*mbo_pos++ = 1;
		*mbo_pos++ = cell_pref;

		if (reassoc_delay) {
			*mbo_pos++ = MBO_ATTR_ID_ASSOC_RETRY_DELAY;
			*mbo_pos++ = 2;
			WPA_PUT_LE16(mbo_pos, reassoc_delay);
			mbo_pos += 2;
		}

		mbo_len = mbo_pos - mbo;
	}
#endif /* CONFIG_MBO */

	ret = wnm_send_bss_tm_req(hapd, sta, req_mode, disassoc_timer,
				  valid_int, bss_term_dur, dialog_token, url,
				  nei_len ? nei_rep : NULL, nei_len,
				  mbo_len ? mbo : NULL, mbo_len);
#ifdef CONFIG_MBO
fail:
#endif /* CONFIG_MBO */
	os_free(url);
	return ret;
}

#endif /* CONFIG_WNM_AP */


int hostapd_ctrl_iface_acl_del_mac(struct hostapd_bss_config *conf,
				   bool accept, const char *txtaddr)
{
	struct mac_acl_entry **exact_acl, **masked_acl;
	int *num_exact, *num_masked;
	u8 addr[ETH_ALEN];
	u8 mask[ETH_ALEN];
	const char *pos;
	bool has_mask = false;
	static const u8 exact_mask_val[ETH_ALEN] =
				{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (accept) {
		exact_acl  = &conf->accept_mac;
		num_exact  = &conf->num_accept_mac;
		masked_acl = &conf->accept_mac_masked;
		num_masked = &conf->num_accept_mac_masked;
	} else {
		exact_acl  = &conf->deny_mac;
		num_exact  = &conf->num_deny_mac;
		masked_acl = &conf->deny_mac_masked;
		num_masked = &conf->num_deny_mac_masked;
	}

	if (!(*num_exact) && !(*num_masked))
		return 0;

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	/* Check if a specific mask is provided */
	pos = os_strchr(txtaddr, ' ');
	if (pos) {
		pos++;
		while (*pos == ' ')
			pos++;
		if (hwaddr_aton(pos, mask) == 0)
			has_mask = true;
	}

	if (has_mask) {
		if (os_memcmp(mask, exact_mask_val, ETH_ALEN) == 0) {
			/* Remove specific exact entry by address */
			hostapd_remove_acl_mac(exact_acl, num_exact, addr);
		} else {
			/* Remove specific masked entry by addr+mask pair */
			hostapd_remove_acl_mac_masked_pair(masked_acl,
							   num_masked,
							   addr, mask);
		}
	} else {
		/* No mask specified: remove from both lists */
		hostapd_acl_del_entry(exact_acl, num_exact,
				      masked_acl, num_masked, addr);
	}

	return 0;
}


void hostapd_ctrl_iface_acl_clear_list(struct hostapd_bss_config *conf,
					bool accept)
{
	if (accept)
		hostapd_acl_clear(&conf->accept_mac, &conf->num_accept_mac,
				  &conf->accept_mac_masked,
				  &conf->num_accept_mac_masked);
	else
		hostapd_acl_clear(&conf->deny_mac, &conf->num_deny_mac,
				  &conf->deny_mac_masked,
				  &conf->num_deny_mac_masked);
}


int hostapd_ctrl_iface_acl_show_mac(struct hostapd_bss_config *conf,
				    bool accept,
				    char *buf, size_t buflen)
{
	struct mac_acl_entry *exact_acl, *masked_acl;
	int num_exact, num_masked;
	int i = 0, len = 0, ret = 0;

	if (accept) {
		exact_acl  = conf->accept_mac;
		num_exact  = conf->num_accept_mac;
		masked_acl = conf->accept_mac_masked;
		num_masked = conf->num_accept_mac_masked;
	} else {
		exact_acl  = conf->deny_mac;
		num_exact  = conf->num_deny_mac;
		masked_acl = conf->deny_mac_masked;
		num_masked = conf->num_deny_mac_masked;
	}

	while (i < num_exact) {
		ret = os_snprintf(buf + len, buflen - len,
				  MACSTR " VLAN_ID=%d\n",
				  MAC2STR(exact_acl[i].addr),
				  exact_acl[i].vlan_id.untagged);
		if (ret < 0 || (size_t) ret >= buflen - len)
			return len;
		i++;
		len += ret;
	}

	i = 0;
	while (i < num_masked) {
		ret = os_snprintf(buf + len, buflen - len,
				  MACSTR " mask=" MACSTR " VLAN_ID=%d\n",
				  MAC2STR(masked_acl[i].addr),
				  MAC2STR(masked_acl[i].mask),
				  masked_acl[i].vlan_id.untagged);
		if (ret < 0 || (size_t) ret >= buflen - len)
			return len;
		i++;
		len += ret;
	}

	return len;
}

int hostapd_ctrl_iface_acl_add_mac(struct hostapd_bss_config *conf,
				   bool accept, const char *cmd)
{
	struct mac_acl_entry **exact_acl, **masked_acl;
	int *num_exact, *num_masked;
	u8 addr[ETH_ALEN];
	u8 mask[ETH_ALEN];
	int ret = 0, vlanid = 0, k;
	const char *pos, *mask_pos;
	bool duplicate = false;
	static const u8 exact_mask_val[ETH_ALEN] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	if (accept) {
		exact_acl  = &conf->accept_mac;
		num_exact  = &conf->num_accept_mac;
		masked_acl = &conf->accept_mac_masked;
		num_masked = &conf->num_accept_mac_masked;
	} else {
		exact_acl  = &conf->deny_mac;
		num_exact  = &conf->num_deny_mac;
		masked_acl = &conf->deny_mac_masked;
		num_masked = &conf->num_deny_mac_masked;
	}

	if (hwaddr_aton(cmd, addr))
		return -1;

	/* Initialize mask to exact match by default */
	os_memcpy(mask, exact_mask_val, ETH_ALEN);

	/* Check for mask parameter
	 * format : "aa:bb:cc:dd:ee:ff ff:ff:ff:00:00:00 [VLAN_ID=X]"
	 */
	pos = cmd;
	/* Skip the MAC address */
	while (*pos && *pos != ' ')
		pos++;

	if (*pos == ' ') {
		pos++;
		/* Skip any spaces */
		while (*pos == ' ')
			pos++;
		/* If the next token is not VLAN_ID= and not end-of-string,
		 * treat it as a mask.  Reject malformed masks explicitly
		 * instead of silently falling back to exact-match. */
		if (*pos && os_strncmp(pos, "VLAN_ID=", 8) != 0) {
			mask_pos = pos;
			if (hwaddr_aton(mask_pos, mask) != 0) {
				wpa_printf(MSG_ERROR,
					   "ACL: Invalid mask in ADD_MAC command"
					   " — expected MAC address format");
				return -1;
			}
			/* Move pos past the mask */
			while (*pos && *pos != ' ')
				pos++;
		}
	}

	/* Look for VLAN_ID parameter */
	pos = os_strstr(cmd, "VLAN_ID=");
	if (pos)
		vlanid = atoi(pos + 8);

	/*
	 * Duplicate check: compare address AND mask together so that the same
	 * address with different masks (one exact, one masked) is allowed to
	 * coexist in the two separate lists.
	 */
	if (os_memcmp(mask, exact_mask_val, ETH_ALEN) == 0) {
		/* Exact entry: O(log n) binary search */
		if (hostapd_maclist_found(*exact_acl, *num_exact, addr, NULL))
			duplicate = true;
	} else {
		/* Masked entry: explicit addr+mask pair check — O(m) */
		for (k = 0; k < *num_masked; k++) {
			if (os_memcmp((*masked_acl)[k].addr, addr,
				      ETH_ALEN) == 0 &&
			    os_memcmp((*masked_acl)[k].mask, mask,
				      ETH_ALEN) == 0) {
				duplicate = true;
				break;
			}
		}
	}

	if (!duplicate) {
		ret = hostapd_acl_add_entry(exact_acl, num_exact,
					    masked_acl, num_masked,
					    vlanid, addr, mask);
	} else {
		wpa_printf(MSG_DEBUG,
			   "ACL: Entry " MACSTR " with mask " MACSTR " already exists",
			   MAC2STR(addr), MAC2STR(mask));
		return 0;
	}

	return ret < 0 ? -1 : 0;
}


#ifdef CONFIG_IEEE80211AX
int hostapd_ctrl_iface_set_mbssid_tx(struct hostapd_data *hapd, const char *cmd)
{
	struct hostapd_multi_mbssid_group *group;
	struct hostapd_data *tx_hapd, *bss;
	bool error = false, auto_stop = false, auto_start = false;
	u8 current_bss_index, group_size;
	char *token, *context = NULL;
	u32 mbssid_idx_disabled_bmap = 0, *mbssid_idx_bmap;
	int ret, i, j, reorder_done_index = -1;
	size_t num_bss, max_num_bss;

	if (!hapd || !hapd->iconf || !hapd->iface || !hapd->conf) {
		wpa_printf(MSG_ERROR, "Invalid BSS");
		return -1;
	}

	if (hapd->iconf->mbssid == MBSSID_DISABLED) {
		wpa_printf(MSG_INFO, "%s link %u is not part of any MBSSID group",
			   hapd->conf->iface, hapd->mld_link_id);
		return -1;
	}

	if (cmd[0] != '\0') {
		while ((token = str_token((char *) cmd, " ", &context))) {
			if (os_strncmp(token, "auto_stop", 9) == 0) {
				auto_stop = true;
			} else if (os_strncmp(token, "auto_start", 10) == 0) {
				auto_start = true;
			} else {
				wpa_printf(MSG_ERROR,
					   "Incorrect command parameter %s",
					   token);
				return -1;
			}
		}
	}

	tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	if (tx_hapd == hapd) {
		wpa_printf(MSG_INFO,
			   "%s is already the transmitted profile of MBSSID group",
			   hapd->conf->iface);
		return 0;
	}

	/* Retrive the MBSSID index bitmap to be updated after changing
	 * transmitting interface.
	 */
	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		group_size = hapd->iface->conf->group_size;
		group = hapd->mbssid_group;
		if (!group) {
			wpa_printf(MSG_ERROR,
				   "Invalid MBSSID group for the provided interface");
			return -1;
		}

		mbssid_idx_bmap = &group->mbssid_idx_bmap;
	} else {
		group_size = 1 << hostapd_max_bssid_indicator(hapd);
		mbssid_idx_bmap = &hapd->iface->mbssid_idx_bmap;
	}

	/* Store the current MBSSID index of the non-transmitted profile which
	 * is to be the new transmitting profile.
	 */
	current_bss_index = hapd->mbssid_idx;
	num_bss = hostapd_get_mbssid_max_num_bss(hapd);

	if (auto_stop) {
		/* Stop all non-transmitted profiles from the MBSSID group */
		for (i = 0; i < num_bss; i++) {
			if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
				bss = hostapd_get_multi_group_bss(group, i);
			else
				bss = hapd->iface->bss[i];

			if (!bss || !bss->conf || !bss->started || bss == tx_hapd)
				continue;

			ret = hostapd_disable_bss(bss, 0, AP_EVENT_DISABLED);
			if (ret) {
				wpa_printf(MSG_ERROR, "Failed to disable %s link %u",
					   bss->conf->iface, bss->mld_link_id);
				goto failed_to_disable;
			} else {
				mbssid_idx_disabled_bmap |= BIT(bss->mbssid_idx);
				wpa_printf(MSG_DEBUG, "Disabled %s link %u",
					   bss->conf->iface, bss->mld_link_id);
			}
		}

		/* Stop the transmitted profiles of the MBSSID group */
		if (tx_hapd->beacon_set_done) {
			ret = hostapd_disable_bss(tx_hapd, 0, AP_EVENT_DISABLED);
			if (ret) {
				wpa_printf(MSG_ERROR, "Failed to disable %s link %u",
					   tx_hapd->conf->iface, tx_hapd->mld_link_id);
				goto failed_to_disable;
			} else {
				mbssid_idx_disabled_bmap |= BIT(tx_hapd->mbssid_idx);
				wpa_printf(MSG_DEBUG, "Disabled %s link %u",
					   tx_hapd->conf->iface, tx_hapd->mld_link_id);
			}
		}
	} else if (tx_hapd->started) {
		wpa_printf(MSG_ERROR,
			   "Profiles from MBSSID group must be disabled using disable_bss command before changing transmitted profile");
		return -1;
	}

	*mbssid_idx_bmap = 0;
	/* Rotate the interface array or group BSS list such that the new
	 * transmitting profile comes to the front, stop rotation once this
	 * happens. Shift the MBSSID indices for each profile with respect to
	 * the new transmitting profile.
	 */
	for (i = 0; i < num_bss; i++) {
		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			if (reorder_done_index < 0)
				bss = hostapd_get_multi_group_bss(group, 0);
			else
				bss = hostapd_get_multi_group_bss(group,
								  i - reorder_done_index);
		} else {
			if (reorder_done_index < 0)
				bss = hapd->iface->bss[0];
			else
				bss = hapd->iface->bss[i - reorder_done_index];
		}

		if (!bss || !bss->conf)
			continue;

		if (bss->mbssid_idx < current_bss_index)
			bss->mbssid_idx += group_size;

		bss->mbssid_idx -= current_bss_index;
		*mbssid_idx_bmap |= BIT(bss->mbssid_idx);

		if (!bss->mbssid_idx) {
			reorder_done_index = i;
		} else if (reorder_done_index < 0) {
			if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
				dl_list_del(&bss->mbssid_bss);
				dl_list_add_tail(&group->bss_list, &bss->mbssid_bss);
			} else {
				for (j = 0; j < num_bss - 1; j++)
					hapd->iface->bss[j] = hapd->iface->bss[j + 1];
				hapd->iface->bss[num_bss - 1] = bss;
			}
		}
	}

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
		group->txbss = hapd;

	max_num_bss = (1 << hostapd_max_bssid_indicator(hapd));

	/* Clear previous Tx BSS(tx_hapd) AID bitmap and init AID bitmap for new Tx BSS */
	for (i = 0; i < max_num_bss; i++) {
		tx_hapd->sta_aid[0] &= ~BIT(i);
		hapd->sta_aid[0] |= BIT(i);
	}

	if (!auto_start)
		return 0;

	/* Restart all profiles with transmitting profile first */
	for (i = 0; i < num_bss; i++) {
		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			bss = hostapd_get_multi_group_bss(group, i);
		else
			bss = hapd->iface->bss[i];

		if (!bss || !bss->conf)
			continue;

		ret = hostapd_enable_bss(bss);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Failed to re-enable %s link %u after setting new tranmitting profile",
				   bss->conf->iface, bss->mld_link_id);
			error = true;
		}
	}

	if (error)
		return -1;

	return 0;

failed_to_disable:
	/* Failed to stop some profile, restart the already stopped once to
	 * return the system to original state.
	 */
	for (i = 0; i < num_bss; i++) {
		if (!(mbssid_idx_disabled_bmap & BIT(bss->mbssid_idx)))
			continue;

		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
			bss = hostapd_get_multi_group_bss(group, i);
		else
			bss = hapd->iface->bss[i];

		if (!bss || !bss->conf)
			continue;

		ret = hostapd_enable_bss(bss);
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Failed to re-enable %s link %u",
				   bss->conf->iface, bss->mld_link_id);
		}
	}

	return -1;
}
#endif /* CONFIG_IEEE80211AX */


int hostapd_disassoc_accept_mac(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	struct vlan_description vlan_id;
	bool disconnect_sta;

	if ((hapd->conf->macaddr_acl != DENY_UNLESS_ACCEPTED) &&
	    (hapd->conf->macaddr_acl != ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST))
		return 0;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
#ifdef CONFIG_IEEE80211BE
		int link_id;
		struct mld_link_info *info;
#endif /* CONFIG_IEEE80211BE */

		disconnect_sta = false;

		if (hostapd_acl_allow_wps_pbc_sta(hapd, sta->addr))
			continue;

		if (!hostapd_acl_maclist_found(hapd->conf, true,
					       sta->addr, &vlan_id) ||
		    (vlan_id.notempty &&
		     vlan_compare(&vlan_id, sta->vlan_desc)))
			disconnect_sta = true;
		else
			continue;

#ifdef CONFIG_IEEE80211BE
		for (link_id = 0; hapd->conf->mld_ap &&
				  link_id < MAX_NUM_MLD_LINKS &&
				  sta->mld_info.mld_sta; link_id++) {
			info = &sta->mld_info.links[link_id];
			if (!info->valid || link_id != hapd->mld_link_id)
				continue;
			if (!hostapd_acl_maclist_found(hapd->conf, true,
						       info->peer_addr,
						       &vlan_id) ||
			    (vlan_id.notempty &&
			     vlan_compare(&vlan_id, sta->vlan_desc))) {
				disconnect_sta = true;
			} else {
				disconnect_sta = false;
				break;
			}
		}
#endif /* CONFIG_IEEE80211BE */
		if (disconnect_sta)
			ap_sta_disconnect(hapd, sta, sta->addr,
					  WLAN_REASON_UNSPECIFIED);
	}

	return 0;
}


int hostapd_disassoc_deny_mac(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	struct vlan_description vlan_id;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
#ifdef CONFIG_IEEE80211BE
		int link_id;
		struct mld_link_info *info;
#endif /* CONFIG_IEEE80211BE */

		if (hostapd_acl_allow_wps_pbc_sta(hapd, sta->addr))
			continue;

		/*
		 * The accept list takes priority over the deny list in all
		 * modes EXCEPT ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST (mode 3),
		 * where a STA must be in the accept list AND NOT in the deny
		 * list.  This is consistent with hostapd_check_acl() which
		 * checks the accept list first for modes 0, 1, 2, and 4.
		 * Skip STAs found in the accept list so they are not
		 * disconnected solely because they also appear in the deny list.
		 */
		if (hapd->conf->macaddr_acl !=
		    ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST &&
		    hostapd_acl_maclist_found(hapd->conf, true,
					      sta->addr, NULL))
			continue;

		if (hostapd_acl_maclist_found(hapd->conf, false,
					      sta->addr, &vlan_id) &&
		    (!vlan_id.notempty ||
		     !vlan_compare(&vlan_id, sta->vlan_desc)))
			ap_sta_disconnect(hapd, sta, sta->addr,
					  WLAN_REASON_UNSPECIFIED);
#ifdef CONFIG_IEEE80211BE
		for (link_id = 0; hapd->conf->mld_ap &&
			     link_id < MAX_NUM_MLD_LINKS &&
			     sta->mld_info.mld_sta; link_id++) {
			info = &sta->mld_info.links[link_id];
			if (!info->valid || link_id != hapd->mld_link_id)
				continue;

			/* Accept list takes priority in all modes except mode 3 */
			if (hapd->conf->macaddr_acl !=
			    ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST &&
			    hostapd_acl_maclist_found(hapd->conf, true,
						      info->peer_addr, NULL))
				continue;

			if (hostapd_acl_maclist_found(hapd->conf, false,
						      info->peer_addr,
						      &vlan_id) &&
			    (!vlan_id.notempty ||
			     !vlan_compare(&vlan_id, sta->vlan_desc)))
				ap_sta_disconnect(hapd, sta, sta->addr,
						  WLAN_REASON_UNSPECIFIED);
		}
#endif /* CONFIG_IEEE80211BE */
	}

	return 0;
}
