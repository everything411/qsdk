/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 * Copyright (c) 2008-2012, Jouni Malinen <j@w1.fi>
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include "utils/common.h"
#include "utils/crc32.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "crypto/sha1.h"
#include "wps/wps_defs.h"
#include "p2p/p2p.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "wpa_auth.h"
#include "wmm.h"
#include "ap_config.h"
#include "sta_info.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "hs20.h"
#include "dfs.h"
#include "hw_features.h"
#include "taxonomy.h"
#include "ieee802_11_auth.h"
#include "dscp_policy.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_HOSTAPD_IF
#include "hostapd_if/hostapd_if.h"
#endif
#include <assert.h>

#define HOSTAPD_TPC_REPORT_IE_LEN			2
#define HOSTAPD_TPC_REPORT_LINK_MARGIN_UNKNOWN		0
#define PROBE_DELAY_DEFAULT_MAX_STA			10

#ifdef CONFIG_IEEE80211AX
#include "robust_av.h"
#endif

#ifdef NEED_AP_MLME
static int hostapd_insert_mbssid_ie(u8 *mbssid_offset, u8 *mbssid_pos, u8 *pos);

static u8 * hostapd_eid_bss_load(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	if (len < 2 + 5)
		return eid;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->bss_load_test_set) {
		*eid++ = WLAN_EID_BSS_LOAD;
		*eid++ = 5;
		os_memcpy(eid, hapd->conf->bss_load_test, 5);
		eid += 5;
		return eid;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (hapd->conf->bss_load_update_period) {
		*eid++ = WLAN_EID_BSS_LOAD;
		*eid++ = 5;
		WPA_PUT_LE16(eid, hapd->num_sta);
		eid += 2;
		*eid++ = hapd->iface->channel_utilization;
		WPA_PUT_LE16(eid, 0); /* no available admission capabity */
		eid += 2;
	}
	return eid;
}


u8 ieee802_11_erp_info(struct hostapd_data *hapd)
{
	u8 erp = 0;

	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return 0;

	if (hapd->iface->olbc)
		erp |= ERP_INFO_USE_PROTECTION;
	if (hapd->iface->num_sta_non_erp > 0) {
		erp |= ERP_INFO_NON_ERP_PRESENT |
			ERP_INFO_USE_PROTECTION;
	}
	if (hapd->iface->num_sta_no_short_preamble > 0 ||
	    hapd->iconf->preamble == LONG_PREAMBLE)
		erp |= ERP_INFO_BARKER_PREAMBLE_MODE;

	return erp;
}


static u8 * hostapd_eid_ds_params(struct hostapd_data *hapd, u8 *eid)
{
	enum hostapd_hw_mode hw_mode = hapd->iconf->hw_mode;

	if (hw_mode != HOSTAPD_MODE_IEEE80211G &&
	    hw_mode != HOSTAPD_MODE_IEEE80211B)
		return eid;

	*eid++ = WLAN_EID_DS_PARAMS;
	*eid++ = 1;
	*eid++ = hapd->iconf->channel;
	return eid;
}


static u8 * hostapd_eid_erp_info(struct hostapd_data *hapd, u8 *eid)
{
	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211G)
		return eid;

	/* Set NonERP_present and use_protection bits if there
	 * are any associated NonERP stations. */
	/* TODO: use_protection bit can be set to zero even if
	 * there are NonERP stations present. This optimization
	 * might be useful if NonERP stations are "quiet".
	 * See 802.11g/D6 E-1 for recommended practice.
	 * In addition, Non ERP present might be set, if AP detects Non ERP
	 * operation on other APs. */

	/* Add ERP Information element */
	*eid++ = WLAN_EID_ERP_INFO;
	*eid++ = 1;
	*eid++ = ieee802_11_erp_info(hapd);

	return eid;
}


static u8 * hostapd_eid_pwr_constraint(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 local_pwr_constraint = 0;
	int dfs;

	if (hapd->iface->current_mode == NULL ||
	    hapd->iface->current_mode->mode != HOSTAPD_MODE_IEEE80211A)
		return eid;

	/* Let host drivers add this IE if DFS support is offloaded */
	if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD)
		return eid;

	/*
	 * There is no DFS support and power constraint was not directly
	 * requested by config option.
	 */
	if (!hapd->iconf->ieee80211h &&
	    hapd->iconf->local_pwr_constraint == -1)
		return eid;

	/* Check if DFS is required by regulatory. */
	dfs = hostapd_is_dfs_required(hapd->iface);
	if (dfs < 0) {
		wpa_printf(MSG_WARNING, "Failed to check if DFS is required; ret=%d",
			   dfs);
		dfs = 0;
	}

	if (dfs == 0 && hapd->iconf->local_pwr_constraint == -1)
		return eid;

	/*
	 * ieee80211h (DFS) is enabled so Power Constraint element shall
	 * be added when running on DFS channel whenever local_pwr_constraint
	 * is configured or not. In order to meet regulations when TPC is not
	 * implemented using a transmit power that is below the legal maximum
	 * (including any mitigation factor) should help. In this case,
	 * indicate 3 dB below maximum allowed transmit power.
	 */
	if (hapd->iconf->local_pwr_constraint == -1)
		local_pwr_constraint = 3;

	/*
	 * A STA that is not an AP shall use a transmit power less than or
	 * equal to the local maximum transmit power level for the channel.
	 * The local maximum transmit power can be calculated from the formula:
	 * local max TX pwr = max TX pwr - local pwr constraint
	 * Where max TX pwr is maximum transmit power level specified for
	 * channel in Country element and local pwr constraint is specified
	 * for channel in this Power Constraint element.
	 */

	/* Element ID */
	*pos++ = WLAN_EID_PWR_CONSTRAINT;
	/* Length */
	*pos++ = 1;
	/* Local Power Constraint */
	if (local_pwr_constraint)
		*pos++ = local_pwr_constraint;
	else
		*pos++ = hapd->iconf->local_pwr_constraint;

	return pos;
}

static bool hostapd_tpc_report_allowed(struct hostapd_data *hapd)
{
	size_t i;

	if (hapd->iconf->ieee80211h)
		return true;

	for (i = 0; i < RRM_CAPABILITIES_IE_LEN; i++) {
		if (hapd->conf->radio_measurements[i])
			return true;
	}

	return false;
}

static size_t hostapd_tpc_report_len(struct hostapd_data *hapd)
{
	if (!hapd->tpc_eirp_valid || !hostapd_tpc_report_allowed(hapd))
		return 0;
	return sizeof(struct tpc_report);
}

static u8 * hostapd_eid_tpc_report(struct hostapd_data *hapd, u8 *eid)
{
	if (!hapd->tpc_eirp_valid || !hostapd_tpc_report_allowed(hapd))
		return eid;

	*eid++ = WLAN_EID_TPC_REPORT;
	*eid++ = HOSTAPD_TPC_REPORT_IE_LEN;
	*eid++ = (u8) (s8) hapd->tpc_eirp_dbm;
	*eid++ = HOSTAPD_TPC_REPORT_LINK_MARGIN_UNKNOWN;

	return eid;
}


static u8 * hostapd_eid_country_add(struct hostapd_data *hapd, u8 *pos,
				    u8 *end, int chan_spacing,
				    struct hostapd_channel_data *start,
				    struct hostapd_channel_data *prev)
{
	if (end - pos < 3)
		return pos;

	/* first channel number */
	*pos++ = start->chan;
	/* number of channels */
	*pos++ = (prev->chan - start->chan) / chan_spacing + 1;
	/* maximum transmit power level */
	if (!is_6ghz_op_class(hapd->iconf->op_class))
		*pos++ = start->max_tx_power;
	else
		*pos++ = 0; /* Reserved when operating on the 6 GHz band */

	return pos;
}


static u8 * hostapd_fill_subband_triplets(struct hostapd_data *hapd, u8 *pos,
					    u8 *end)
{
	int i;
	struct hostapd_hw_modes *mode;
	struct hostapd_channel_data *start, *prev;
	int chan_spacing = 1;

	mode = hapd->iface->current_mode;
	if (mode->mode == HOSTAPD_MODE_IEEE80211A)
		chan_spacing = 4;

	start = prev = NULL;
	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];
		if (chan->flag & HOSTAPD_CHAN_DISABLED)
			continue;
		if (start && prev &&
		    prev->chan + chan_spacing == chan->chan &&
		    start->max_tx_power == chan->max_tx_power) {
			prev = chan;
			continue; /* can use same entry */
		}

		if (start && prev)
			pos = hostapd_eid_country_add(hapd, pos, end,
						      chan_spacing,
						      start, prev);

		/* Start new group */
		start = prev = chan;
	}

	if (start) {
		pos = hostapd_eid_country_add(hapd, pos, end, chan_spacing,
					      start, prev);
	}

	return pos;
}


u8 * hostapd_eid_country(struct hostapd_data *hapd, u8 *eid,
			 int max_len)
{
	u8 *pos = eid;
	u8 *end = eid + max_len;
	u8 op_class;
#ifdef CONFIG_QCN_EXTN
	u8 opclass_tbl_idx;
#endif /* CONFIG_QCN_EXTN */
	bool force_global;

	if (!hapd->iconf->ieee80211d || max_len < 6 ||
	    hapd->iface->current_mode == NULL)
		return eid;

	op_class = hostapd_get_oper_class_of_bss(hapd);

	*pos++ = WLAN_EID_COUNTRY;
	pos++; /* length will be set later */
	os_memcpy(pos, hapd->iconf->country, 3); /* e.g., 'US ' */
	pos += 3;

	/* Force the third octet of the country string to indicate
	 * Global Operating Class (Table E-4) */
	force_global = true;
#ifdef CONFIG_QCN_EXTN
	opclass_tbl_idx = hapd->iconf->conf_extn.opclass_tbl_idx;
	if (opclass_tbl_idx != OPCLS_TAB_IDX_NONE)
		eid[4] = opclass_tbl_idx;

	force_global = false;
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_MBO
	/* Wi-Fi Agile Muiltiband AP is required to use a global operating
	 * class. */
	if (hapd->conf->mbo_enabled)
		force_global = true;
#endif /* CONFIG_MBO */

	if (force_global) {
		/* Force the third octet of the country string to indicate
		 * Global Operating Class (Table E-4) */
		eid[4] = 0x04;
	}

	if (is_6ghz_op_class(op_class)) {
		/* Operating Triplet field */
		/* Operating Extension Identifier (>= 201 to indicate this is
		 * not a Subband Triplet field) */
		*pos++ = 201;
		/* Operating Class */
		*pos++ = op_class;
		/* Coverage Class */
		*pos++ = 0;
		/* Subband Triplets are required only for the 20 MHz case */
		if (op_class == 131 || op_class == 136)
			pos = hostapd_fill_subband_triplets(hapd, pos, end);
	} else {
		pos = hostapd_fill_subband_triplets(hapd, pos, end);
	}

	if ((pos - eid) & 1) {
		if (end - pos < 1)
			return eid;
		*pos++ = 0; /* pad for 16-bit alignment */
	}

	eid[1] = (pos - eid) - 2;

	return pos;
}


const u8 * hostapd_wpa_ie(struct hostapd_data *hapd, u8 eid)
{
	const u8 *ies;
	size_t ies_len;

	ies = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ies_len);
	if (!ies)
		return NULL;

	return get_ie(ies, ies_len, eid);
}


static const u8 * hostapd_vendor_wpa_ie(struct hostapd_data *hapd,
					u32 vendor_type)
{
	const u8 *ies;
	size_t ies_len;

	ies = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ies_len);
	if (!ies)
		return NULL;

	return get_vendor_ie(ies, ies_len, vendor_type);
}


static u8 * hostapd_get_rsne(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_wpa_ie(hapd, WLAN_EID_RSN);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_mde(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_wpa_ie(hapd, WLAN_EID_MOBILITY_DOMAIN);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_rsnxe(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->no_beacon_rsnxe) {
		wpa_printf(MSG_INFO, "TESTING: Do not add RSNXE into Beacon");
		return pos;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	ie = hostapd_wpa_ie(hapd, WLAN_EID_RSNX);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_wpa_ie(struct hostapd_data *hapd, u8 *pos, size_t len)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, WPA_IE_VENDOR_TYPE);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_rsne_override(struct hostapd_data *hapd, u8 *pos,
				      size_t len)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, RSNE_OVERRIDE_IE_VENDOR_TYPE);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_rsne_override_2(struct hostapd_data *hapd, u8 *pos,
					size_t len)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, RSNE_OVERRIDE_2_IE_VENDOR_TYPE);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static u8 * hostapd_get_rsnxe_override(struct hostapd_data *hapd, u8 *pos,
				       size_t len)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, RSNXE_OVERRIDE_IE_VENDOR_TYPE);
	if (!ie || 2U + ie[1] > len)
		return pos;

	os_memcpy(pos, ie, 2 + ie[1]);
	return pos + 2 + ie[1];
}


static size_t hostapd_get_rsne_override_len(struct hostapd_data *hapd)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, RSNE_OVERRIDE_IE_VENDOR_TYPE);
	if (!ie)
		return 0;
	return 2 + ie[1];
}


static size_t hostapd_get_rsne_override_2_len(struct hostapd_data *hapd)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, RSNE_OVERRIDE_2_IE_VENDOR_TYPE);
	if (!ie)
		return 0;
	return 2 + ie[1];
}


static size_t hostapd_get_rsnxe_override_len(struct hostapd_data *hapd)
{
	const u8 *ie;

	ie = hostapd_vendor_wpa_ie(hapd, RSNXE_OVERRIDE_IE_VENDOR_TYPE);
	if (!ie)
		return 0;
	return 2 + ie[1];
}


static u8 * hostapd_eid_csa(struct hostapd_data *hapd, u8 *eid)
{
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iface->cs_oper_class && hapd->iconf->ecsa_ie_only)
		return eid;
#endif /* CONFIG_TESTING_OPTIONS */

	if (!hapd->cs_freq_params.channel)
		return eid;

	*eid++ = WLAN_EID_CHANNEL_SWITCH;
	*eid++ = 3;
	*eid++ = hapd->cs_block_tx;
	*eid++ = hapd->cs_freq_params.channel;
	*eid++ = hapd->cs_count;

	return eid;
}


static u8 * hostapd_eid_ecsa(struct hostapd_data *hapd, u8 *eid)
{
	u8 oper_class = hapd->iface->cs_oper_class;

#ifdef CONFIG_QCN_EXTN
	if (hapd->conf->bss_extn.ecsa_opclass)
		oper_class = hapd->conf->bss_extn.ecsa_opclass;
#endif /* CONFIG_QCN_EXTN */

	if (!hapd->cs_freq_params.channel || !oper_class) {
		return eid;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iconf->csa_ie_only)
		return eid;
#endif /* CONFIG_TESTING_OPTIONS */

	*eid++ = WLAN_EID_EXT_CHANSWITCH_ANN;
	*eid++ = 4;
	*eid++ = hapd->cs_block_tx;
	*eid++ = oper_class;
	*eid++ = hapd->cs_freq_params.channel;
	*eid++ = hapd->cs_count;

	return eid;
}

u8 * hostapd_eid_add_max_cs_time(u8 *eid, u32 switch_time)
{
	*eid++ = WLAN_EID_EXTENSION;
	*eid++ = 4;
	*eid++ = WLAN_EID_EXT_MAX_CHANNEL_SWITCH_TIME;
	WPA_PUT_LE24(eid, switch_time);
	eid += 3;

	return eid;
}

static u8 * hostapd_eid_max_cs_time(struct hostapd_data *hapd, u8 *eid)
{
#ifdef CONFIG_IEEE80211BE
	u32 switch_time;

	/* Add Max Channel Switch Time element only if this AP is affiliated
	 * with an AP MLD and channel switch is in process. */
	if (!hapd->conf->mld_ap || !hapd->cs_freq_params.channel)
		goto check_bootup_cac;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return eid;
#endif /* CONFIG_QCN_EXTN */

	/* Switch time is basically time between CSA count 1 and CSA count
	 * 0 (1 beacon interval) + time for interface restart + time to
	 * send a Beacon frame in the new channel (1 beacon interval).
	 *
	 * Use driver-provided timing if available, otherwise fall back
	 * to hardcoded values
	 */
	if (hapd->iface->cs_time > 0) {
		/* Convert from milliseconds to TU (1 TU = 1024 microseconds) */
		switch_time = USEC_TO_TU(hapd->iface->cs_time * 1000);
		wpa_printf(MSG_DEBUG,
			   "Using driver-provided channel switch time: %u ms (%u TU)",
			   hapd->iface->cs_time, switch_time);
	} else {
		/* Fallback to previous hardcoded behavior (assume 1 second) */
		switch_time = USEC_TO_TU(250 * 1000) + 2 * hapd->iconf->beacon_int;
		wpa_printf(MSG_DEBUG, "Using fallback channel switch time: %u TU",
			   switch_time);
	}

	return hostapd_eid_add_max_cs_time(eid, switch_time);

check_bootup_cac:
#ifdef CONFIG_QCN_EXTN
	/*
	 * Bootup CAC: add MCST IE directly in the 5 GHz beacon so that
	 * clients on the 5 GHz channel can see the remaining CAC time.
	 */
	if (hapd->conf->mld_ap && hapd->iface->bootup_cac_in_progress) {
		switch_time = hostapd_get_remaining_cac_tu(hapd->iface);
		if (switch_time)
			eid = hostapd_eid_add_max_cs_time(eid, switch_time);
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	return eid;
}


static u8 * hostapd_eid_supported_op_classes(struct hostapd_data *hapd, u8 *eid)
{
	enum oper_chan_width chwidth;
	u8 op_class, channel;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA) ||
	    !hapd->iface->freq)
		return eid;

	chwidth = hostapd_get_oper_chan_width_of_bss(hapd);
	if (ieee80211_freq_to_channel_ext(hapd->iface->freq,
					  hapd->iconf->secondary_channel,
					  chwidth,
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return eid;

	*eid++ = WLAN_EID_SUPPORTED_OPERATING_CLASSES;
	*eid++ = 2;

#ifdef CONFIG_QCN_EXTN
	hostapd_modify_supported_op_class_for_240mhz_extn(hapd->iface->freq,
							  chwidth, &op_class);
#endif /* CONFIG_QCN_EXTN */

	/* Current Operating Class */
	*eid++ = op_class;

	/* TODO: Advertise all the supported operating classes */
	*eid++ = 0;

	return eid;
}

#ifdef CONFIG_IEEE80211BN
/**
 * hostapd_eid_smd_ie - Add SMD Information Element
 * @hapd: BSS data
 * @eid: Pointer to the current position in the buffer
 * Returns: Pointer to the next position in the buffer
 */
u8 * hostapd_eid_smd_ie(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	u8 smd_cap_byte;

	if (!hapd->conf->smd.enabled)
		return eid;

	if (!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SMD))
		return eid;

	*pos++ = WLAN_EID_EXTENSION;
	*pos++ = SMD_IE_LEN - 2; /* Ext ID + SMD ID + Capabilities + Timeout */
	*pos++ = WLAN_EID_EXT_SMD; /* Element ID Extension */

	/* SMD Identifier (6 octets) - 48-bit MAC address */
	os_memcpy(pos, hapd->conf->smd.smd_identifier, ETH_ALEN);
	pos += ETH_ALEN;

	/* SMD Capabilities (1 octet)
	 * B0: DL Data Forwarding
	 * B1-B3: Max Number Of Prepared Target AP MLDs
	 * B4: SMD Type
	 * B5: PTK Mode
	 * B6: Neighboring AP Probing Support
	 * B7: Reserved
	 */
	smd_cap_byte = 0;
	if (hapd->conf->smd.caps.dl_data_fwd)
		smd_cap_byte |= BIT(0);
	smd_cap_byte |= (hapd->conf->smd.caps.max_prep_target_apmlds & 0x07) << 1;
	if (hapd->conf->smd.caps.smd_type)
		smd_cap_byte |= BIT(4);
	if (hapd->conf->smd.caps.ptk_mode)
		smd_cap_byte |= BIT(5);
	*pos++ = smd_cap_byte;

	/* Timeout Value (1 octet) - in units of 64 TUs */
	*pos++ = (u8)hapd->conf->smd.smd_prep_timeout;

	return pos;
}

static size_t hostapd_smd_ie_len(struct hostapd_data *hapd)
{
	if (!hapd->conf->smd.enabled)
		return 0;

	if (!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SMD))
		return 0;

	return SMD_IE_LEN;
}
#endif /* CONFIG_IEEE80211BN */

static int
ieee802_11_build_ap_params_mbssid(struct hostapd_data *hapd,
				  struct wpa_driver_ap_params *params)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_data *tx_bss;
	size_t len, rnr_len = 0;
	u8 elem_count = 0, *elem = NULL, **elem_offset = NULL, *end;
	u8 rnr_elem_count = 0, *rnr_elem = NULL, **rnr_elem_offset = NULL;
	u32 elemid_modified_bmap = 0;
	bool is_len_calc_failed = false;

	if (!iface->mbssid_max_interfaces ||
	    iface->num_bss > iface->mbssid_max_interfaces ||
	    (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED &&
	     !iface->ema_max_periodicity))
		goto fail;

#ifdef RDK_ONEWIFI
       tx_bss = hostapd_drv_mbssid_get_tx_bss(hapd);
       len = hostapd_drv_eid_mbssid_len(tx_bss, WLAN_FC_STYPE_BEACON, &elem_count,
                                    NULL, 0, &rnr_len);
#else

	tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	len = hostapd_eid_mbssid_len(tx_bss, WLAN_FC_STYPE_BEACON, &elem_count,
				     NULL, 0, &rnr_len, true, params,
				     &is_len_calc_failed);
#endif /* RDK_ONEWIFI */
	if (is_len_calc_failed)
		goto fail;

	if (iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED && !elem_count)
		return 0;

	if (!len || (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED &&
		     elem_count > iface->ema_max_periodicity))
		goto fail;

	elem = os_zalloc(len);
	if (!elem)
		goto fail;

	elem_offset = os_zalloc(elem_count * sizeof(u8 *));
	if (!elem_offset)
		goto fail;

	if (rnr_len) {
		rnr_elem = os_zalloc(rnr_len);
		if (!rnr_elem)
			goto fail;

		rnr_elem_offset = os_calloc(elem_count + 1, sizeof(u8 *));
		if (!rnr_elem_offset)
			goto fail;
	}
#ifdef RDK_ONEWIFI
       end = hostapd_drv_eid_mbssid(tx_bss, elem, elem + len, WLAN_FC_STYPE_BEACON,
                                elem_count, elem_offset, NULL, 0, rnr_elem,
                                &rnr_elem_count, rnr_elem_offset, rnr_len);
#else

	end = hostapd_eid_mbssid(tx_bss, elem, elem + len, WLAN_FC_STYPE_BEACON,
				 elem_count, elem_offset, NULL, 0, rnr_elem,
				 &rnr_elem_count, rnr_elem_offset, rnr_len,
				 &elemid_modified_bmap, true, params);
#endif /* RDK_ONEWIFI */

	params->mbssid.mbssid_tx_iface = tx_bss->conf->iface;
#ifdef RDK_ONEWIFI
	params->mbssid.mbssid_index = hostapd_drv_mbssid_get_bss_index(hapd);
#else
	params->mbssid.mbssid_index = hostapd_mbssid_get_bss_index(hapd);
#endif /* RDK_ONEWIFI */
	params->mbssid.mbssid_elem = elem;
	params->mbssid.mbssid_elem_len = end - elem;
	params->mbssid.mbssid_elem_count = elem_count;
	params->mbssid.mbssid_elem_offset = elem_offset;
	params->mbssid.rnr_elem = rnr_elem;
	params->mbssid.rnr_elem_len = rnr_len;
	params->mbssid.rnr_elem_count = rnr_elem_count;
	params->mbssid.rnr_elem_offset = rnr_elem_offset;
	params->elemid_modified_bmap |= elemid_modified_bmap;
	if (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED)
		params->mbssid.ema = true;

	params->mbssid.mbssid_tx_iface_linkid = -1;
#ifdef CONFIG_IEEE80211BE
	if (tx_bss->conf->mld_ap)
		params->mbssid.mbssid_tx_iface_linkid = tx_bss->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return 0;

fail:
	os_free(rnr_elem);
	os_free(rnr_elem_offset);
	os_free(elem_offset);
	os_free(elem);
	wpa_printf(MSG_ERROR, "MBSSID: Configuration failed");
	return -1;
}


static u8 * hostapd_eid_mbssid_config(struct hostapd_data *hapd, u8 *eid,
				      u8 mbssid_elem_count)
{
	struct hostapd_iface *iface = hapd->iface;

	if (iface->conf->mbssid == ENHANCED_MBSSID_ENABLED) {
		*eid++ = WLAN_EID_EXTENSION;
		*eid++ = 3;
		*eid++ = WLAN_EID_EXT_MULTIPLE_BSSID_CONFIGURATION;
		*eid++ = iface->num_bss;
		*eid++ = mbssid_elem_count;
	}

	return eid;
}


static size_t he_elem_len(struct hostapd_data *hapd)
{
	size_t len = 0;

#ifdef CONFIG_IEEE80211AX
	if (!hostapd_is_he_enabled(hapd))
		return len;

	len += 3 + sizeof(struct ieee80211_he_capabilities) +
		3 + sizeof(struct ieee80211_he_operation) +
		3 + sizeof(struct ieee80211_he_mu_edca_parameter_set) +
		3 + sizeof(struct ieee80211_spatial_reuse);
	if (is_6ghz_op_class(hapd->iconf->op_class)) {
		len += sizeof(struct ieee80211_he_6ghz_oper_info) +
			3 + sizeof(struct ieee80211_he_6ghz_band_cap);
		len += 3 + MAX_PSD_TPE_POWER_COUNT +
		       1 + MAX_PSD_TPE_EXT_POWER_COUNT;
		/* An additional Transmit Power Envelope element for
		 * subordinate client */
		if (he_reg_is_indoor(hapd->iconf->he_6ghz_reg_pwr_type))
			len += 3 + MAX_PSD_TPE_POWER_COUNT +
			       1 + MAX_PSD_TPE_EXT_POWER_COUNT;

		/* An additional Transmit Power Envelope element for
		 * default client with unit interpretation of regulatory
		 * client EIRP */
		if (hapd->iconf->reg_def_cli_eirp != -1 &&
		    he_reg_is_sp(hapd->iconf->he_6ghz_reg_pwr_type))
			len += 3 + MAX_TPE_EIRP_NUM_POWER_SUPPORTED +
			       1 + MAX_EIRP_TPE_POWER_EXT_COUNT;
	}
#endif /* CONFIG_IEEE80211AX */

	return len;
}

size_t hostapd_eid_country_len(struct hostapd_data *hapd)
{
	struct hostapd_hw_modes *mode;
	size_t len;
	int i, enabled_channels = 0;

	if (!hapd->iconf->ieee80211d || hapd->iface->current_mode == NULL)
		return 0;

	mode = hapd->iface->current_mode;

	/* Element ID (1) + Length (1) + Country String (3) = 5 */
	len = 5;

	/* Add Operating Triplet for 6 GHz: Extension ID + Op Class + Coverage */
	if (is_6ghz_op_class(hapd->iconf->op_class))
		len += 3;

	/* Count the enabled (non-disabled) channels */
	for (i = 0; i < mode->num_channels; i++) {
		if (!(mode->channels[i].flag & HOSTAPD_CHAN_DISABLED))
			enabled_channels++;
	}

	/* Estimate subband triplets: 3 bytes per channel group */
	/* Worst case: each enabled channel is a separate group */
	len += enabled_channels * 3;

	/* Padding for alignment */
	if (len & 1)
		len += 1;
	return len;
}

static void hostapd_free_probe_resp_params(struct probe_resp_params *params)
{
#ifdef CONFIG_IEEE80211BE
	if (!params)
		return;

	os_free(params->mld_info);
	params->mld_info = NULL;
#endif /* CONFIG_IEEE80211BE */
}


static size_t hostapd_probe_resp_elems_len(struct hostapd_data *hapd,
					   struct probe_resp_params *params)
{
	size_t buflen = 0;
	u8 include_ext_cap = 0;
	u8 param_ext_cap = 0;

	hapd = hostapd_mbssid_get_tx_bss(hapd);

#ifdef CONFIG_WPS
	if (hapd->wps_probe_resp_ie)
		buflen += wpabuf_len(hapd->wps_probe_resp_ie);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	if (hapd->p2p_probe_resp_ie)
		buflen += wpabuf_len(hapd->p2p_probe_resp_ie);
#endif /* CONFIG_P2P */
#ifdef CONFIG_FST
	if (hapd->iface->fst_ies)
		buflen += wpabuf_len(hapd->iface->fst_ies);
#endif /* CONFIG_FST */

	/* Use plugin vendor elements if set, otherwise use conf vendor elements */
	if (hapd->plugin_vendor_elements)
		buflen += wpabuf_len(hapd->plugin_vendor_elements);
	else if (hapd->conf->vendor_elements_count)
		buflen += hapd->conf->vendor_elements_len;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->presp_elements)
		buflen += wpabuf_len(hapd->conf->presp_elements);
#endif /* CONFIG_TESTING_OPTIONS */
	if (hapd->conf->vendor_vht) {
		buflen += 5 + 2 + sizeof(struct ieee80211_vht_capabilities) +
			2 + sizeof(struct ieee80211_vht_operation);
	}

	buflen += he_elem_len(hapd);

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {

		buflen += hostapd_eid_eht_capab_len(hapd, IEEE80211_MODE_AP);
		buflen += 3 + sizeof(struct ieee80211_eht_operation);
		if (hapd->iconf->punct_bitmap)
			buflen += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;

		if (hapd->conf->enable_aal)
			include_ext_cap = BIT(BASIC_MULTI_LINK_CTRL_EXT_EN);
		if (hapd->conf->single_link_emlsr &&
		    (hapd->iface->mld_ext_mld_capa &
		     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
			include_ext_cap |=
				BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);

#ifdef CONFIG_QCN_EXTN
		if (params->mld_ap && params->mld_ap->conf->mld_ap &&
		    !hostapd_is_repurpose_disabled_11be_extn(params->mld_ap->conf)) {
#else
		if (params->mld_ap && params->mld_ap->conf->mld_ap) {
#endif /* CONFIG_QCN_EXTN */
			/* Check for non-Tx BSS conf */
			if (params->mld_ap->conf->enable_aal)
				param_ext_cap = BIT(BASIC_MULTI_LINK_CTRL_EXT_EN);
			if (params->mld_ap->conf->single_link_emlsr &&
			    (params->mld_ap->iface->mld_ext_mld_capa &
			     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
				param_ext_cap |=
					BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);

			buflen += hostapd_eid_eht_ml_beacon_len(
				params->mld_ap, params->mld_info,
				!!params->mld_ap, param_ext_cap,
				true, true,
				!!(params->mld_ap->conf->skip_uhr_extn & SKIP_UHR_EXTN_PROBE_RESP));

			if (hapd->conf->mld_ap)
				buflen += hostapd_eid_eht_ml_beacon_len(
					hapd, NULL, false, include_ext_cap,
					true, true,
					!!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_PROBE_RESP));

			/* For Max Channel Switch Time element during channel
			 * switch */
			buflen += 6;
		} else if (hapd->conf->mld_ap) {
			buflen += hostapd_eid_eht_ml_beacon_len(
				hapd, params->mld_info, false, include_ext_cap,
				true, true,
				!!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_PROBE_RESP));

			/* For Max Channel Switch Time element during channel
			 * switch */
			buflen += 6;
		}
		/* ML reconfigure feature */
		if (hapd->conf->mld_ap)
			buflen += hostapd_eid_eht_ml_reconfig_len(hapd);

		/* TTLM IE */
		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present)
			buflen += hostapd_get_ttlm_elem_len(
				&hapd->mld->ttlm_ctx.established_ttlm.ttlm);

		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm.mapping_switch_time_present)
			buflen += hostapd_get_ttlm_elem_len(
				&hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm);

	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		buflen += 3 + sizeof(struct ieee80211_uhr_capabilities);
		buflen += 3 + sizeof(struct ieee80211_uhr_operation);
		if (hapd->iconf->npca_enable) {
			buflen += IEEE80211_UHR_NPCA_OPER_BASE_SIZE;
			if (hapd->iconf->npca_punct_bitmap)
				buflen += IEEE80211_UHR_NPCA_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;
		}
		buflen += hostapd_eid_uhr_params_update_len(hapd, false, false);
		buflen += hostapd_eid_tx_power_indication_len();
	}
	buflen += hostapd_smd_ie_len(hapd);
#endif /* CONFIG_IEEE80211BN */

	buflen += hostapd_eid_rnr_len(hapd, WLAN_FC_STYPE_PROBE_RESP, true);
	buflen += hostapd_mbo_ie_len(hapd);
#ifdef CONFIG_MBO
	if (OCE_AP_ENABLED(hapd) && hapd->conf->oce_ess_report_enabled)
		buflen += 4; /* ESS Report element: EID(1)+Len(1)+EID_EXT(1)+ESS_Info(1) */
#endif /* CONFIG_MBO */
	buflen += hostapd_eid_owe_trans_len(hapd);
	buflen += hostapd_eid_dpp_cc_len(hapd);
	buflen += hostapd_get_rsne_override_len(hapd);
	buflen += hostapd_get_rsne_override_2_len(hapd);
	buflen += hostapd_get_rsnxe_override_len(hapd);
	buflen += hostapd_wfa_cap_ie_len(hapd, NULL);
	buflen += hostapd_tpc_report_len(hapd);
	buflen += hostapd_security_profile_ie_len(hapd);
#ifdef CONFIG_QCN_EXTN
	buflen += hostapd_modify_buflen_for_qcn_ie_extn(hapd);
	/* WDS vendor IE */
	buflen += hostapd_wds_ie_len_extn(hapd);
#endif /* CONFIG_QCN_EXTN */
	/* Estimated Service Parameters (ESP) IE */
#ifdef CONFIG_QCN_EXTN
	buflen += hostapd_esp_ie_len_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

	return buflen;
}

int ieee802_11_build_nontx_bss_probe_params(struct hostapd_data *hapd,
					    struct probe_resp_params *nontx_probe_params)
{
	u8 *pos, *epos;
	size_t buflen;

#define MBSSID_NONTX_PROBE_RESP_LEN 600
	buflen = MBSSID_NONTX_PROBE_RESP_LEN;

#ifdef CONFIG_WPS
	if (hapd->wps_probe_resp_ie)
		buflen += wpabuf_len(hapd->wps_probe_resp_ie);
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if (hapd->p2p_probe_resp_ie)
		buflen += wpabuf_len(hapd->p2p_probe_resp_ie);
#endif /* CONFIG_P2P */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		/* TTLM IE */
		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present)
			buflen += hostapd_get_ttlm_elem_len(&hapd->mld->ttlm_ctx.established_ttlm.ttlm);
		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm.mapping_switch_time_present)
			buflen += hostapd_get_ttlm_elem_len(&hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm);
	}

#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies)
		buflen += wpabuf_len(hapd->iface->fst_ies);
#endif /* CONFIG_FST */

	buflen += hostapd_mbo_ie_len(hapd);
#ifdef CONFIG_MBO
	if (OCE_AP_ENABLED(hapd) && hapd->conf->oce_ess_report_enabled)
		buflen += 4; /* ESS Report element: EID(1)+Len(1)+EID_EXT(1)+ESS_Info(1) */
#endif /* CONFIG_MBO */
	buflen += hostapd_eid_owe_trans_len(hapd);
	buflen += hostapd_eid_dpp_cc_len(hapd);
	buflen += hostapd_get_rsne_override_len(hapd);
	buflen += hostapd_get_rsne_override_2_len(hapd);
	buflen += hostapd_get_rsnxe_override_len(hapd);
	buflen += hostapd_wfa_cap_ie_len(hapd, NULL);
#ifdef CONFIG_QCN_EXTN
	buflen += hostapd_esp_ie_len_extn(hapd);
	buflen += hostapd_modify_buflen_for_qcn_ie_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd))
		buflen += hostapd_eid_uhr_params_update_len(hapd, false, false);
#endif /* CONFIG_IEEE80211BE */

	buflen += hostapd_security_profile_ie_len(hapd);

	nontx_probe_params->resp = os_zalloc(buflen);
	if (!nontx_probe_params->resp) {
		nontx_probe_params->resp_len = 0;
		return -1;
	}

	pos = nontx_probe_params->resp->u.probe_resp.variable;
	epos = (u8 *)nontx_probe_params->resp + buflen;

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* Power Constraint */
	pos = hostapd_eid_pwr_constraint(hapd, pos);

	/* Extended supported rates */
	pos = hostapd_eid_ext_supp_rates(hapd, pos);

	/* RSN, BSS Load, RRM capabilities, MDE */
	pos = hostapd_get_rsne(hapd, pos, epos - pos);
	pos = hostapd_eid_bss_load(hapd, pos, epos - pos);
	pos = hostapd_eid_rm_enabled_capab(hapd, pos, epos - pos);
	pos = hostapd_get_mde(hapd, pos, epos - pos);

	/* Extended capabilities (gate for MBSSID and known BSS list) */
	pos = hostapd_eid_ext_capab(hapd, pos,
				    hapd->iconf->mbssid >= MBSSID_ENABLED &&
				    !nontx_probe_params->known_bss_len);

	/* Time Advertisement & Time Zone */
	pos = hostapd_eid_time_adv(hapd, pos);
	pos = hostapd_eid_time_zone(hapd, pos);

	/* Interworking/ANQP */
	pos = hostapd_eid_interworking(hapd, pos);
	pos = hostapd_eid_adv_proto(hapd, pos);
	pos = hostapd_eid_roaming_consortium(hapd, pos);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(pos, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		pos += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

	/* FILS indication */
	pos = hostapd_eid_fils_indic(hapd, pos, 0);

	/* RSNXE */
	pos = hostapd_get_rsnxe(hapd, pos, epos - pos);

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd))
		pos = hostapd_eid_he_mu_edca_parameter_set(hapd, pos, false);
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present)
			pos = hostapd_add_ttlm_info_elem(pos,
							 &hapd->mld->ttlm_ctx.established_ttlm.ttlm,
							 hapd);

		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm.mapping_switch_time_present)
			pos = hostapd_add_ttlm_info_elem(pos,
							 &hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm,
							 hapd);
	}
#endif /* CONFIG_IEEE80211BE */

	/* WPA (legacy) */
	pos = hostapd_get_wpa_ie(hapd, pos, epos - pos);

	/* WFA capability IE */
	pos = hostapd_add_wfa_cap_ie(hapd, NULL, pos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->wps_probe_resp_ie),
			  wpabuf_len(hapd->wps_probe_resp_ie));
		pos += wpabuf_len(hapd->wps_probe_resp_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) &&
	    nontx_probe_params->is_p2p &&
	    hapd->p2p_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->p2p_probe_resp_ie),
			  wpabuf_len(hapd->p2p_probe_resp_ie));
		pos += wpabuf_len(hapd->p2p_probe_resp_ie);
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		pos = hostapd_eid_p2p_manage(hapd, pos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	pos = hostapd_eid_hs20_indication(hapd, pos);
#endif /* CONFIG_HS20 */

	/* MBO, OWE transition, DPP channel config */
#ifdef CONFIG_MBO
	pos = hostapd_eid_ess_report(hapd, pos, epos - pos);
#endif /* CONFIG_MBO */
#ifdef CONFIG_MBO
	pos = hostapd_eid_ap_channel_report(hapd, pos, epos - pos);
#endif /* CONFIG_MBO */
	pos = hostapd_eid_mbo(hapd, pos, epos - pos);
	pos = hostapd_eid_owe_trans(hapd, pos, epos - pos);
	pos = hostapd_eid_dpp_cc(hapd, pos, epos - pos);

	/* RSN overrides */
	pos = hostapd_get_rsne_override(hapd, pos, epos - pos);
	pos = hostapd_get_rsne_override_2(hapd, pos, epos - pos);
	pos = hostapd_get_rsnxe_override(hapd, pos, epos - pos);
#ifdef CONFIG_QCN_EXTN
	pos = hostapd_eid_qcn_vendor_ie_extn(hapd, pos, IEEE80211_MODE_AP);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd))
		pos = hostapd_eid_uhr_params_update(hapd, pos, false, false);
#endif /* CONFIG_IEEE80211BN */

	pos = hostapd_eid_security_profile(hapd, pos);

	/* Final length */
	nontx_probe_params->resp_len = pos - (u8 *) nontx_probe_params->resp;
	if (nontx_probe_params->resp_len > buflen) {
		wpa_printf(MSG_ERROR,
			   "Non-Tx Probe params build failed %s: built size exceeds allocation"
			   "(alloc=%zu used=%zu)", hapd->conf->iface, buflen,
			   nontx_probe_params->resp_len);
		assert(0);
	}
	return 0;
}


static u8 * hostapd_probe_resp_fill_elems(struct hostapd_data *hapd,
					  struct probe_resp_params *params,
					  u8 *pos, size_t len)
{
	struct hostapd_data *hapd_probed = params->mld_ap ? params->mld_ap :
		hapd;
	u8 *csa_pos;
	u8 *epos;
	u8 *mbssid_pos, *mbssid_offset;
	u8 ext_cap = 0;
	u8 p_ext_cap = 0;
	size_t i, mbssid_len;
	bool bcast_prb_resp = false;
	bool is_len_calc_failed = false;

	hapd = hostapd_mbssid_get_tx_bss(hapd);
	epos = (u8 *) params->resp + len;

	*pos++ = WLAN_EID_SSID;

	if (hapd->conf->ignore_broadcast_ssid && hapd != hapd_probed) {
		if (hapd->conf->ignore_broadcast_ssid == 2) {
			*pos++ = hapd->conf->ssid.ssid_len;
			os_memset(pos, 0, hapd->conf->ssid.ssid_len);
			pos += hapd->conf->ssid.ssid_len;
		} else {
			*pos++ = 0; /* empty SSID */
		}
	} else {
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memcpy(pos, hapd->conf->ssid.ssid,
				hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	}

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	pos = hostapd_eid_country(hapd, pos, epos - pos);

	/* Power Constraint element */
	pos = hostapd_eid_pwr_constraint(hapd, pos);
	pos = hostapd_eid_tpc_report(hapd, pos);

	/* CSA element */
	csa_pos = hostapd_eid_csa(hapd, pos);
	if (csa_pos != pos)
		params->csa_pos = csa_pos - 1;
	else
		params->csa_pos = NULL;
	pos = csa_pos;

	/* ERP Information element */
	pos = hostapd_eid_erp_info(hapd, pos);

	/* Extended supported rates */
	pos = hostapd_eid_ext_supp_rates(hapd, pos);

	pos = hostapd_get_rsne(hapd, pos, epos - pos);
	pos = hostapd_eid_bss_load(hapd, pos, epos - pos);

	/* RMSL value would be sent in broadcast Probe response case */
	if (is_broadcast_ether_addr(params->resp->da))
		bcast_prb_resp = true;

	/* store the mbssid_ie offset to insert it later */
	mbssid_offset = pos;

	pos = hostapd_eid_rm_enabled_capab(hapd, pos, epos - pos);
	pos = hostapd_get_mde(hapd, pos, epos - pos);

	/* eCSA element */
	csa_pos = hostapd_eid_ecsa(hapd, pos);
	if (csa_pos != pos)
		params->ecsa_pos = csa_pos - 1;
	else
		params->ecsa_pos = NULL;
	pos = csa_pos;

	pos = hostapd_eid_supported_op_classes(hapd, pos);
	pos = hostapd_eid_ht_capabilities(hapd, pos);
	pos = hostapd_eid_ht_operation(hapd, pos);

	/* Probe Response frames always include all non-TX profiles except
	 * when a list of known BSSes is included in the Probe Request frame. */
	pos = hostapd_eid_ext_capab(hapd, pos,
				    hapd->iconf->mbssid >= MBSSID_ENABLED &&
				    !params->known_bss_len);

	pos = hostapd_eid_time_adv(hapd, pos);
	pos = hostapd_eid_time_zone(hapd, pos);

	pos = hostapd_eid_interworking(hapd, pos);
	pos = hostapd_eid_adv_proto(hapd, pos);
	pos = hostapd_eid_roaming_consortium(hapd, pos);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(pos, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		pos += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

#ifdef CONFIG_IEEE80211AC
	if (hostapd_is_vht_enabled(hapd) &&
	    !is_6ghz_op_class(hapd->iconf->op_class)) {
		pos = hostapd_eid_vht_capabilities(hapd, pos, 0);

		pos = hostapd_eid_vht_operation(hapd, pos);
		pos = hostapd_eid_txpower_envelope(hapd, pos);
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd) &&
	    is_6ghz_op_class(hapd->iconf->op_class))
		pos = hostapd_eid_txpower_envelope(hapd, pos);
#endif /* CONFIG_IEEE80211AX */

	pos = hostapd_eid_chsw_wrapper(hapd, pos);

	pos = hostapd_eid_rnr(hapd, pos, WLAN_FC_STYPE_PROBE_RESP, true);
	pos = hostapd_eid_fils_indic(hapd, pos, 0);

	/* Max Channel Switch Time element */
	pos = hostapd_eid_max_cs_time(hapd, pos);

	pos = hostapd_get_rsnxe(hapd, pos, epos - pos);

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd)) {
		u8 *cca_pos;

		pos = hostapd_eid_he_capab(hapd, pos, IEEE80211_MODE_AP);
		pos = hostapd_eid_he_operation(hapd, pos);

		/* BSS Color Change Announcement element */
		cca_pos = hostapd_eid_cca(hapd, pos);
		if (cca_pos != pos)
			params->cca_pos = cca_pos - 2;
		else
			params->cca_pos = NULL;
		pos = cca_pos;

		pos = hostapd_eid_spatial_reuse(hapd, pos);
		pos = hostapd_eid_he_mu_edca_parameter_set(hapd, pos, false);
		pos = hostapd_eid_he_6ghz_band_cap(hapd, pos);
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		if (hapd->conf->enable_aal) {
			ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EN);

			/* RMSL value would be sent in broadcast Probe response case */
			if (bcast_prb_resp)
				ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_RMSL_INFO_EN);
		}
		if (hapd->conf->single_link_emlsr &&
		    (hapd->iface->mld_ext_mld_capa &
		     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
			ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);

#ifdef CONFIG_QCN_EXTN
		if (params->mld_ap && params->mld_ap->conf->mld_ap &&
		    !hostapd_is_repurpose_disabled_11be_extn(params->mld_ap->conf)) {
#else
		if (params->mld_ap && params->mld_ap->conf->mld_ap) {
#endif /* CONFIG_QCN_EXTN */
			/* Check for non-Tx BSS conf */
			if (params->mld_ap->conf->enable_aal) {
				p_ext_cap = BIT(BASIC_MULTI_LINK_CTRL_EXT_EN);

				/* RMSL value would be sent in broadcast Probe response */
				if (bcast_prb_resp)
					p_ext_cap |=
					BIT(BASIC_MULTI_LINK_CTRL_EXT_RMSL_INFO_EN);
			}
			if (params->mld_ap->conf->single_link_emlsr &&
			    (params->mld_ap->iface->mld_ext_mld_capa &
			     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
				ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);

			pos = hostapd_eid_eht_ml_beacon(
				params->mld_ap, params->mld_info,
				pos, !!params->mld_ap, p_ext_cap,
				true, true,
				!!(params->mld_ap->conf->skip_uhr_extn & SKIP_UHR_EXTN_PROBE_RESP));

			if (hapd->conf->mld_ap)
				pos = hostapd_eid_eht_ml_beacon(
					hapd, NULL, pos, false, ext_cap,
					true, true,
					!!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_PROBE_RESP));

		} else if (hapd->conf->mld_ap) {
			pos = hostapd_eid_eht_ml_beacon(hapd,
							params->mld_info,
							pos, false, ext_cap,
							true, true,
							!!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_PROBE_RESP));
		}
		/* ML reconfigure feature */
		if (hapd->conf->mld_ap)
			pos = hostapd_eid_eht_reconf_ml(hapd, pos);

		pos = hostapd_eid_eht_capab(hapd, pos, IEEE80211_MODE_AP);
		pos = hostapd_eid_eht_operation(hapd, pos);

		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.established_ttlm.ttlm.expected_duration_present)
			pos = hostapd_add_ttlm_info_elem(pos,
							 &hapd->mld->ttlm_ctx.established_ttlm.ttlm,
							 hapd);

		if (hapd->mld &&
		    hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm.mapping_switch_time_present)
			pos = hostapd_add_ttlm_info_elem(pos,
							 &hapd->mld->ttlm_ctx.upcoming_ttlm.ttlm,
							 hapd);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		pos = hostapd_eid_uhr_capab(hapd, pos, IEEE80211_MODE_AP);
		pos = hostapd_eid_uhr_operation(hapd, pos, false);
		pos = hostapd_eid_uhr_params_update(hapd, pos, false, false);
		pos = hostapd_eid_tx_power_indication(hapd, pos);
	}
#endif /* CONFIG_IEEE80211BN */
	pos = hostapd_eid_security_profile(hapd, pos);
#ifdef CONFIG_IEEE80211AC
	if (hapd->conf->vendor_vht)
		pos = hostapd_eid_vendor_vht(hapd, pos);
#endif /* CONFIG_IEEE80211AC */

	/* WPA */
	pos = hostapd_get_wpa_ie(hapd, pos, epos - pos);

	/* Wi-Fi Alliance WMM */
	pos = hostapd_eid_wmm(hapd, pos, false);

	pos = hostapd_add_wfa_cap_ie(hapd, NULL, pos);

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->wps_probe_resp_ie),
			  wpabuf_len(hapd->wps_probe_resp_ie));
		pos += wpabuf_len(hapd->wps_probe_resp_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && params->is_p2p &&
	    hapd->p2p_probe_resp_ie) {
		os_memcpy(pos, wpabuf_head(hapd->p2p_probe_resp_ie),
			  wpabuf_len(hapd->p2p_probe_resp_ie));
		pos += wpabuf_len(hapd->p2p_probe_resp_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		pos = hostapd_eid_p2p_manage(hapd, pos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	pos = hostapd_eid_hs20_indication(hapd, pos);
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MBO
	pos = hostapd_eid_ess_report(hapd, pos, epos - pos);
#endif /* CONFIG_MBO */
#ifdef CONFIG_MBO
	pos = hostapd_eid_ap_channel_report(hapd, pos, epos - pos);
#endif /* CONFIG_MBO */
	pos = hostapd_eid_mbo(hapd, pos, epos - pos);
	pos = hostapd_eid_owe_trans(hapd, pos, epos - pos);
	pos = hostapd_eid_dpp_cc(hapd, pos, epos - pos);

	pos = hostapd_get_rsne_override(hapd, pos, epos - pos);
	pos = hostapd_get_rsne_override_2(hapd, pos, epos - pos);
	pos = hostapd_get_rsnxe_override(hapd, pos, epos - pos);

#ifdef CONFIG_QCN_EXTN
	pos = hostapd_eid_qcn_vendor_ie_extn(hapd, pos, IEEE80211_MODE_AP);
	/* WDS vendor IE in probe response */
	pos = hostapd_eid_wds_ie_extn(hapd, pos, epos - pos);
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BN
	/* SMD Information element */
	pos = hostapd_eid_smd_ie(hapd, pos);
#endif /* CONFIG_IEEE80211BN */

	/* Add Estimated Service Parameters (ESP) IE in Probe Response when enabled */
#ifdef CONFIG_QCN_EXTN
	pos = hostapd_eid_esp_extn(hapd, pos, epos - pos);
#endif /* CONFIG_QCN_EXTN */

	/* Use plugin vendor elements if set, otherwise use conf vendor elements */
	if (hapd->plugin_vendor_elements) {
		os_memcpy(pos, wpabuf_head(hapd->plugin_vendor_elements),
			  wpabuf_len(hapd->plugin_vendor_elements));
		pos += wpabuf_len(hapd->plugin_vendor_elements);
	} else {
		for (i = 0; i < hapd->conf->vendor_elements_count; i++) {
			struct wpabuf *entry = hapd->conf->vendor_elements[i];

			os_memcpy(pos, wpabuf_head(entry), wpabuf_len(entry));
			pos += wpabuf_len(entry);
		}
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->presp_elements) {
		os_memcpy(pos, wpabuf_head(hapd->conf->presp_elements),
			  wpabuf_len(hapd->conf->presp_elements));
		pos += wpabuf_len(hapd->conf->presp_elements);
	}
#endif /* CONFIG_TESTING_OPTIONS */

	params->resp_len = pos - (u8 *) params->resp;
	mbssid_len = hostapd_eid_mbssid_len(hapd_probed, WLAN_FC_STYPE_PROBE_RESP,
					    NULL,
					    params->known_bss,
					    params->known_bss_len, NULL,
					    bcast_prb_resp, params,
					    &is_len_calc_failed);
	if (is_len_calc_failed) {
		wpa_printf(MSG_ERROR,
			   "Probe response: Failed to calculate the MBSSID elements length %s",
			   hapd_probed->conf->iface);
		return NULL;
	}

	if (mbssid_len) {
		u8 *orig_pos = pos;
		size_t used_len = params->resp_len;
		size_t total_capacity = epos - (u8 *) params->resp;

		if (total_capacity - used_len < mbssid_len) {
			struct ieee80211_mgmt *new_resp;
			size_t new_capacity = total_capacity +
					      (mbssid_len - (total_capacity - used_len));

			new_resp = os_realloc(params->resp, new_capacity);
			if (!new_resp) {
				wpa_printf(MSG_ERROR,
				"Failed to allocate memory required to add MBSSID element");
				return NULL;
			}

			params->resp = new_resp;
			pos = (u8 *) params->resp + used_len;
			epos = (u8 *) params->resp + new_capacity;
			mbssid_offset = pos - ((size_t) (orig_pos - mbssid_offset));
			mbssid_pos = pos;
		} else {
			mbssid_pos = pos;
		}
#ifdef RDK_ONEWIFI
       pos = hostapd_drv_eid_mbssid(hapd, pos, epos, WLAN_FC_STYPE_PROBE_RESP, 0,
                                NULL, NULL, 0,
                                NULL, NULL, NULL, 0);
#else
		pos = hostapd_eid_mbssid(hapd_probed, pos, epos,
					 WLAN_FC_STYPE_PROBE_RESP, 0,
					 NULL, params->known_bss, params->known_bss_len,
					 NULL, NULL, NULL, 0, NULL, bcast_prb_resp, params);
#endif /* RDK_ONEWIFI */
		/* Insert mbssid-ie at specified location */
		if (hostapd_insert_mbssid_ie(mbssid_offset, mbssid_pos, pos)) {
			wpa_printf(MSG_ERROR, "Failed to insert MBSSID-IE");
			return NULL;
		}
	}

	return pos;
}

static int hostapd_insert_mbssid_ie(u8 *mbssid_offset, u8 *mbssid_pos, u8 *pos)
{
	u8 *mbssid_tmp;
	size_t mbssid_len, suffix_len;

	if (!mbssid_offset || !pos || !mbssid_pos) {
		wpa_printf(MSG_ERROR, "mbssid_offset/pos/mbssid_pos is NULL");
		return -1;
	}

	mbssid_len = (size_t) (pos - mbssid_pos);

	if (!mbssid_len) {
		wpa_printf(MSG_ERROR, "mbssid_len is zero");
		return -1;
	}

	/* exclude appended MBSSID */
	suffix_len = (size_t) (mbssid_pos - mbssid_offset);

	if (!suffix_len) {
		wpa_printf(MSG_ERROR, "suffix_len is zero");
		return -1;
	}

	/* Copy appended MBSSID to a temporary buffer */
	mbssid_tmp = os_zalloc(mbssid_len);
	if (!mbssid_tmp) {
		wpa_printf(MSG_ERROR, "Failed to allocate memory");
		return -1;
	}

	os_memcpy(mbssid_tmp, mbssid_pos, mbssid_len);

	/* Create space at mbssid_ie by shifting suffix */
	os_memmove(mbssid_offset + mbssid_len, mbssid_offset, suffix_len);

	/* Copy MBSSID into the gap */
	os_memcpy(mbssid_offset, mbssid_tmp, mbssid_len);
	os_free(mbssid_tmp);

	return 0;
}


static void hostapd_gen_probe_resp(struct hostapd_data *hapd,
				   struct probe_resp_params *params)
{
	struct hostapd_data *hapd_probed = params->mld_ap? params->mld_ap : hapd;
	u8 *pos;
	size_t buflen;

#ifdef RDK_ONEWIFI
       hapd = hostapd_drv_mbssid_get_tx_bss(hapd);
#else
	hapd = hostapd_mbssid_get_tx_bss(hapd);
#endif /* RDK_ONEWIFI */

#define MAX_PROBERESP_LEN 768
	buflen = MAX_PROBERESP_LEN;
	buflen += hostapd_probe_resp_elems_len(hapd_probed, params);
	params->resp = os_zalloc(buflen);
	if (!params->resp) {
		params->resp_len = 0;
		return;
	}

	params->resp->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						   WLAN_FC_STYPE_PROBE_RESP);
	/* Unicast the response to all requests on bands other than 6 GHz. For
	 * the 6 GHz, unicast is used only if the actual SSID is not included in
	 * the Beacon frames. Otherwise, broadcast response is used per IEEE
	 * Std 802.11ax-2021, 26.17.2.3.2. Broadcast address is also used for
	 * the Probe Response frame template for the unsolicited (i.e., not as
	 * a response to a specific request) case. */
	/* Wi-Fi Optimized Connectivity Specification v2.0, Section 3.6:
	 * use broadcast Probe Response for OCE-capable STAs on non-6 GHz. */
	if (params->req && params->force_bcast_resp_oce_non6ghz)
		os_memset(params->resp->da, 0xff, ETH_ALEN);
	else if (params->req && (!is_6ghz_op_class(hapd->iconf->op_class) ||
				 hapd_probed->conf->ignore_broadcast_ssid))
		os_memcpy(params->resp->da, params->req->sa, ETH_ALEN);
	else
		os_memset(params->resp->da, 0xff, ETH_ALEN);
	os_memcpy(params->resp->sa, hapd->own_addr, ETH_ALEN);

	os_memcpy(params->resp->bssid, hapd->own_addr, ETH_ALEN);
	params->resp->u.probe_resp.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	params->resp->u.probe_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd));

	params->resp->u.probe_resp.capab_info |=
		host_to_le16(hostapd_critical_update_capab(hapd));

	pos = hostapd_probe_resp_fill_elems(hapd_probed, params,
					    params->resp->u.probe_resp.variable,
					    buflen);

	if (!pos) {
		wpa_printf(MSG_ERROR,
			   "Probe response: Failed to fill probe response elements for %s",
			   hapd->conf->iface);
		goto fail;
	}

	params->resp_len = pos - (u8 *) params->resp;
	wpa_printf(MSG_DEBUG,
		   "Probe response:%s allocated buffer size :%zu actual frame size:%zu max allowed frame size:%zu",
		   hapd->conf->iface, buflen, params->resp_len, hapd->iface->max_mgmt_frm_sz);

	if (hapd->iface->max_mgmt_frm_sz && (params->resp_len > hapd->iface->max_mgmt_frm_sz)) {
		wpa_printf(MSG_ERROR, "probe response size limit (%zu) exceeded for %s: max allowed size(%zu)",
			   params->resp_len, hapd->conf->iface, hapd->iface->max_mgmt_frm_sz);
		goto fail;
	}

	return;
fail:
	os_free(params->resp);
	params->resp = NULL;
	params->resp_len = 0;
	return;
}


#ifdef CONFIG_IEEE80211BE
static void hostapd_fill_probe_resp_ml_params(struct hostapd_data *hapd,
					      struct probe_resp_params *params,
					      const struct ieee80211_mgmt *mgmt,
					      int mld_id, u16 links)
{
	struct hostapd_data *link;

	params->mld_ap = NULL;
	params->mld_info = os_zalloc(sizeof(*params->mld_info));
	if (!params->mld_info)
		return;

	wpa_printf(MSG_DEBUG,
		   "MLD: Got ML probe request with AP MLD ID %d for links %04x",
		   mld_id, links);

	/*
	 * Set mld_ap if the ML probe request explicitly requested a specific
	 * AP MLD ID.
	 */
	if (mld_id > 0) {
		if (hapd == hostapd_mbssid_get_tx_bss(hapd)) {
			hapd = hostapd_get_mbssid_bss_by_idx(hapd, mld_id);
			if (!hapd) {
				wpa_printf(MSG_INFO,
					   "Ignore Probe Request from " MACSTR
					   " since no matched non-TX BSS found for MBSSID Index %d",
					   MAC2STR(mgmt->sa), mld_id);
				goto fail;
			}
		}
		params->mld_ap = hapd;
	} else {
		/* If ML probe request was sent to a non-TX BSSID without AP MLD ID,
		 * include the TX ML IEs in the ML probe response body.
		*/
		if (hapd != hostapd_mbssid_get_tx_bss(hapd))
			params->mld_ap = hapd;
	}

	for_each_mld_link(link, hapd) {
		struct mld_link_info *link_info;
		u8 mld_link_id = link->mld_link_id;

		/* Never duplicate main Probe Response frame body */
		if (link == hapd)
			continue;

		/* Only include requested links */
		if (!(BIT(mld_link_id) & links))
			continue;

		link_info = &params->mld_info->links[mld_link_id];
		os_memcpy(link_info, &hapd->partner_links[mld_link_id],
			  sizeof(hapd->partner_links[mld_link_id]));

		wpa_printf(MSG_DEBUG,
			   "MLD: ML probe response includes link STA info for %d: %u bytes",
			   mld_link_id, link_info->resp_sta_profile_len);
	}

	if (mld_id > 0 && !params->mld_ap) {
		wpa_printf(MSG_DEBUG,
			   "MLD: No nontransmitted BSSID for MLD ID %d",
			   mld_id);
		goto fail;
	}

	return;

fail:
	hostapd_free_probe_resp_params(params);
	params->mld_ap = NULL;
	params->mld_info = NULL;
}
#endif /* CONFIG_IEEE80211BE */


enum ssid_match_result {
	NO_SSID_MATCH,
	EXACT_SSID_MATCH,
	WILDCARD_SSID_MATCH,
	CO_LOCATED_SSID_MATCH,
};

static enum ssid_match_result ssid_match(struct hostapd_data *hapd,
					 const u8 *ssid, size_t ssid_len,
					 const u8 *ssid_list,
					 size_t ssid_list_len,
					 const u8 *short_ssid_list,
					 size_t short_ssid_list_len)
{
	const u8 *pos, *end;
	struct hostapd_iface *iface = hapd->iface;
	int wildcard = 0;
	size_t i, j;

	if (ssid_len == 0)
		wildcard = 1;
	if (ssid_len == hapd->conf->ssid.ssid_len &&
	    os_memcmp(ssid, hapd->conf->ssid.ssid, ssid_len) == 0)
		return EXACT_SSID_MATCH;

	if (ssid_list) {
		pos = ssid_list;
		end = ssid_list + ssid_list_len;
		while (end - pos >= 2) {
			if (2 + pos[1] > end - pos)
				break;
			if (pos[1] == 0)
				wildcard = 1;
			if (pos[1] == hapd->conf->ssid.ssid_len &&
			    os_memcmp(pos + 2, hapd->conf->ssid.ssid,
				      pos[1]) == 0)
				return EXACT_SSID_MATCH;
			pos += 2 + pos[1];
		}
	}

	if (short_ssid_list) {
		pos = short_ssid_list;
		end = short_ssid_list + short_ssid_list_len;
		while (end - pos >= 4) {
			if (hapd->conf->ssid.short_ssid == WPA_GET_LE32(pos))
				return EXACT_SSID_MATCH;
			pos += 4;
		}
	}

	if (wildcard)
		return WILDCARD_SSID_MATCH;

	if (!iface->interfaces || iface->interfaces->count <= 1 ||
	    is_6ghz_op_class(hapd->iconf->op_class))
		return NO_SSID_MATCH;

	for (i = 0; i < iface->interfaces->count; i++) {
		struct hostapd_iface *colocated;

		colocated = iface->interfaces->iface[i];

		if (colocated == iface ||
		    !is_6ghz_op_class(colocated->conf->op_class))
			continue;

		for (j = 0; j < colocated->num_bss; j++) {
			struct hostapd_bss_config *conf;

			conf = colocated->bss[j]->conf;
			if (ssid_len == conf->ssid.ssid_len &&
			    os_memcmp(ssid, conf->ssid.ssid, ssid_len) == 0)
				return CO_LOCATED_SSID_MATCH;
		}
	}

	return NO_SSID_MATCH;
}


void sta_track_expire(struct hostapd_iface *iface, int force)
{
	struct os_reltime now;
	struct hostapd_sta_info *info;

	if (!iface->num_sta_seen)
		return;

	os_get_reltime(&now);
	while ((info = dl_list_first(&iface->sta_seen, struct hostapd_sta_info,
				     list))) {
		if (!force &&
		    !os_reltime_expired(&now, &info->last_seen,
					iface->conf->track_sta_max_age))
			break;
		force = 0;

		wpa_printf(MSG_MSGDUMP, "%s: Expire STA tracking entry for "
			   MACSTR, iface->bss[0]->conf->iface,
			   MAC2STR(info->addr));
		dl_list_del(&info->list);
		iface->num_sta_seen--;
		sta_track_del(info);
	}
}


#ifdef CONFIG_MBO
static bool oce_probe_req_is_suppressed(struct hostapd_data *hapd,
					const u8 *mbo, size_t mbo_len)
{
	const u8 *pos;
	size_t len;

	if (!mbo || mbo_len < 4)
		return false;

	/* Skip WFA OUI (3) + OUI type (1) */
	pos = mbo + 4;
	len = mbo_len - 4;

	while (len >= 2) {
		u8 attr_id = pos[0];
		u8 attr_len = pos[1];
		const u8 *attr;

		pos += 2;
		len -= 2;

		if (attr_len > len) {
			wpa_printf(MSG_DEBUG,
				   "OCE: malformed Probe Suppression attribute: attr_len=%u > remaining len=%zu",
				   attr_len, len);
			break;
		}

		attr = pos;

		if (attr_id == OCE_ATTR_ID_PROBE_SUPPRESSION_BSSIDS) {
			size_t i;

			/* Length=0 is an explicit wildcard: suppress for all BSSIDs */
			if (attr_len == 0)
				return true;

			for (i = 0; i + ETH_ALEN <= attr_len; i += ETH_ALEN) {
				const u8 *bssid = attr + i;

				/* Wildcard */
				if (is_broadcast_ether_addr(bssid))
					return true;

				if (os_memcmp(bssid, hapd->own_addr, ETH_ALEN) == 0)
					return true;
			}
		} else if (attr_id == OCE_ATTR_ID_PROBE_SUPPRESSION_SSIDS) {
			size_t i;
			u32 ap_short_ssid = hapd->conf->ssid.short_ssid;

			for (i = 0; i + sizeof(u32) <= attr_len; i += sizeof(u32)) {
				u32 short_ssid = WPA_GET_LE32(attr + i);

				/* Wildcard */
				if (short_ssid == 0xffffffff)
					return true;

				if (short_ssid == ap_short_ssid)
					return true;
			}
		}

		pos += attr_len;
		len -= attr_len;
	}

	return false;
}
#endif /* CONFIG_MBO */


static struct hostapd_sta_info * sta_track_get(struct hostapd_iface *iface,
					       const u8 *addr)
{
	struct hostapd_sta_info *info;

	dl_list_for_each(info, &iface->sta_seen, struct hostapd_sta_info, list)
		if (ether_addr_equal(addr, info->addr))
			return info;

	return NULL;
}


void sta_track_add(struct hostapd_iface *iface, const u8 *addr, int ssi_signal)
{
	struct hostapd_sta_info *info;

	info = sta_track_get(iface, addr);
	if (info) {
		/* Move the most recent entry to the end of the list */
		dl_list_del(&info->list);
		dl_list_add_tail(&iface->sta_seen, &info->list);
		os_get_reltime(&info->last_seen);
		info->ssi_signal = ssi_signal;
		return;
	}

	/* Add a new entry */
	info = os_zalloc(sizeof(*info));
	if (info == NULL)
		return;
	os_memcpy(info->addr, addr, ETH_ALEN);
	os_get_reltime(&info->last_seen);
	info->ssi_signal = ssi_signal;

	if (iface->num_sta_seen >= iface->conf->track_sta_max_num) {
		/* Expire oldest entry to make room for a new one */
		sta_track_expire(iface, 1);
	}

	wpa_printf(MSG_MSGDUMP, "%s: Add STA tracking entry for "
		   MACSTR, iface->bss[0]->conf->iface, MAC2STR(addr));
	dl_list_add_tail(&iface->sta_seen, &info->list);
	iface->num_sta_seen++;
}


struct hostapd_data *
sta_track_seen_on(struct hostapd_iface *iface, const u8 *addr,
		  const char *ifname)
{
	struct hapd_interfaces *interfaces = iface->interfaces;
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_data *hapd = NULL;

		iface = interfaces->iface[i];
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				break;
			hapd = NULL;
		}

		if (hapd && sta_track_get(iface, addr))
			return hapd;
	}

	return NULL;
}


#ifdef CONFIG_TAXONOMY
void sta_track_claim_taxonomy_info(struct hostapd_iface *iface, const u8 *addr,
				   struct wpabuf **probe_ie_taxonomy)
{
	struct hostapd_sta_info *info;

	info = sta_track_get(iface, addr);
	if (!info)
		return;

	wpabuf_free(*probe_ie_taxonomy);
	*probe_ie_taxonomy = info->probe_ie_taxonomy;
	info->probe_ie_taxonomy = NULL;
}
#endif /* CONFIG_TAXONOMY */


#ifdef CONFIG_IEEE80211BE
static bool parse_ml_probe_req(const struct ieee80211_eht_ml *ml, size_t ml_len,
			       int *mld_id, u16 *links)
{
	u16 ml_control;
	const struct element *sub;
	const u8 *pos;
	size_t len;

	*mld_id = -1;
	*links = 0xffff;

	if (ml_len < sizeof(struct ieee80211_eht_ml))
		return false;

	ml_control = le_to_host16(ml->ml_control);
	if ((ml_control & MULTI_LINK_CONTROL_TYPE_MASK) !=
	    MULTI_LINK_CONTROL_TYPE_PROBE_REQ) {
		wpa_printf(MSG_DEBUG, "MLD: Not an ML probe req");
		return false;
	}

	if (sizeof(struct ieee80211_eht_ml) + 1 > ml_len) {
		wpa_printf(MSG_DEBUG, "MLD: ML probe req too short");
		return false;
	}

	pos = ml->variable;
	len = pos[0];
	if (len < 1 || sizeof(struct ieee80211_eht_ml) + len > ml_len) {
		wpa_printf(MSG_DEBUG,
			   "MLD: ML probe request with invalid length");
		return false;
	}

	if (ml_control & EHT_ML_PRES_BM_PROBE_REQ_AP_MLD_ID) {
		if (len < 2) {
			wpa_printf(MSG_DEBUG,
				   "MLD: ML probe req too short for MLD ID");
			return false;
		}

		*mld_id = pos[1];
	}
	pos += len;

	/* Parse subelements (if there are any) */
	len = ml_len - len - sizeof(struct ieee80211_eht_ml);
	for_each_element_id(sub, 0, pos, len) {
		const struct ieee80211_eht_per_sta_profile *sta;
		u16 sta_control;

		if (*links == 0xffff)
			*links = 0;

		if (sub->datalen <
		    sizeof(struct ieee80211_eht_per_sta_profile)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: ML probe req %d too short for sta profile",
				   sub->datalen);
			return false;
		}

		sta = (struct ieee80211_eht_per_sta_profile *) sub->data;

		/*
		 * Extract the link ID, do not return whether a complete or
		 * partial profile was requested.
		 */
		sta_control = le_to_host16(sta->sta_control);
		*links |= BIT(sta_control & BASIC_MLE_STA_CTRL_LINK_ID_MASK);
	}

	if (!for_each_element_completed(sub, pos, len)) {
		wpa_printf(MSG_DEBUG,
			   "MLD: ML probe req sub-elements parsing error");
		return false;
	}

	return true;
}
#endif /* CONFIG_IEEE80211BE */


static bool probe_rssi_check_suppression(struct hostapd_data *hapd, const u8 *sa,
				int ssi_signal)
{
	struct hostapd_sta_info *info;
	struct sta_info *sta;
	struct os_reltime now, diff;
	int elapsed_sec;

	/*
	 * Ensure the tracking table has capacity for the probe-delay
	 * feature even when track_sta_max_num was not explicitly set.
	 */
	if (!hapd->iconf->track_sta_max_num)
		hapd->iconf->track_sta_max_num = PROBE_DELAY_DEFAULT_MAX_STA;

	wpa_printf(MSG_DEBUG,
		   "Probe delay evaluating " MACSTR
		   ": signal=%d thresh=%d W=%d s N=%d",
		   MAC2STR(sa), ssi_signal,
		   hapd->iconf->rssi_ignore_probe_request,
		   hapd->iconf->rssi_probe_delay_time_window,
		   hapd->iconf->rssi_probe_delay_req_count);

	sta = ap_get_sta(hapd, sa);
	if (sta && (sta->flags & WLAN_STA_ASSOC)) {
		info = sta_track_get(hapd->iface, sa);
		wpa_printf(MSG_DEBUG,
			   "Probe delay: STA already associated, allowing");
		if (info) {
			info->probe_first_low_rssi_seen.sec = 0;
			info->probe_first_low_rssi_seen.usec = 0;
			info->probe_delay_count = 0;
		}
		return false;
	}

	/*
	 * sta_track_add() is called here (not in handle_probe_req()) so
	 * that suppressed probes are still recorded in the tracking list.
	 */
	sta_track_add(hapd->iface, sa, ssi_signal);

	info = sta_track_get(hapd->iface, sa);
	if (!info)
		return false;

	os_get_reltime(&now);

	if (info->probe_first_low_rssi_seen.sec == 0 &&
	    info->probe_first_low_rssi_seen.usec == 0) {
		info->probe_first_low_rssi_seen = now;
		info->probe_delay_count = 1;
		wpa_printf(MSG_DEBUG,
			   "Probe delay: suppressing probe from "
			   MACSTR " (new entry, count=1)", MAC2STR(sa));
		return true;
	}

	os_reltime_sub(&now, &info->probe_first_low_rssi_seen, &diff);
	elapsed_sec = (int)diff.sec;

	if (elapsed_sec > hapd->iconf->rssi_probe_delay_time_window) {
		info->probe_first_low_rssi_seen = now;
		info->probe_delay_count = 1;
		wpa_printf(MSG_DEBUG,
			   "Probe delay: suppressing probe from " MACSTR
			   " (window expired, elapsed=%d s > W=%d s, reset count=1)",
			   MAC2STR(sa), elapsed_sec,
			   hapd->iconf->rssi_probe_delay_time_window);
		return true;
	}

	info->probe_delay_count++;

	if (info->probe_delay_count > hapd->iconf->rssi_probe_delay_req_count) {
		wpa_printf(MSG_DEBUG,
			   "Probe delay: allowing probe from " MACSTR
			   " (count=%d > N=%d, elapsed=%d s <= W=%d s)",
			   MAC2STR(sa), info->probe_delay_count,
			   hapd->iconf->rssi_probe_delay_req_count,
			   elapsed_sec,
			   hapd->iconf->rssi_probe_delay_time_window);
		return false;
	}

	wpa_printf(MSG_DEBUG,
		   "Probe delay: suppressing probe from " MACSTR
		   " (count=%d <= N=%d, elapsed=%d s <= W=%d s)",
		   MAC2STR(sa), info->probe_delay_count,
		   hapd->iconf->rssi_probe_delay_req_count,
		   elapsed_sec,
		   hapd->iconf->rssi_probe_delay_time_window);
	return true;
}

void handle_probe_req(struct hostapd_data *hapd,
		      const struct ieee80211_mgmt *mgmt, size_t len,
		      const struct hostapd_frame_info *fi)
{
	enum hostapd_hw_mode hw_mode = hapd->iface->current_mode->mode;
	struct ieee802_11_elems elems;
	const u8 *ie;
	size_t ie_len;
	size_t i;
	int noack;
	int ssi_signal;
	enum ssid_match_result res;
	int ret;
	u16 csa_offs[2];
	size_t csa_offs_len;
	struct radius_sta rad_info;
	struct probe_resp_params params;
	char *hex = NULL;
	bool skip_acl = false;
	u8 rate_type = 0;
	u16 rate = 0;
#ifdef CONFIG_MBO
	u32 bitrate;
#endif
#ifdef CONFIG_IEEE80211BE
	int mld_id;
	u16 links;
#endif /* CONFIG_IEEE80211BE */
	struct hostapd_ubus_request req = {
		.type = HOSTAPD_UBUS_PROBE_REQ,
		.mgmt_frame = mgmt,
		.ssi_signal = fi ? fi->ssi_signal : 0,
		.elems = &elems,
	};

	ssi_signal = fi ? fi->ssi_signal : 0;
#ifdef CONFIG_MBO
	bitrate = fi ? fi->datarate : 0;
#endif

	if (hapd->iconf->rssi_ignore_probe_request && ssi_signal &&
	    ssi_signal < hapd->iconf->rssi_ignore_probe_request) {
		if ((hapd->iconf->rssi_probe_delay_time_window) &&
		    (hapd->iconf->rssi_probe_delay_req_count)) {
			if (probe_rssi_check_suppression(hapd, mgmt->sa, ssi_signal))
				return;
		} else {
			return;
		}
	}

	if (len < IEEE80211_HDRLEN)
		return;
	ie = ((const u8 *) mgmt) + IEEE80211_HDRLEN;
	if (hapd->iconf->track_sta_max_num)
		sta_track_add(hapd->iface, mgmt->sa, ssi_signal);
	ie_len = len - IEEE80211_HDRLEN;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap)
		skip_acl = true;
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
	if (!skip_acl) {
		ret = hostapd_allowed_address(hapd, mgmt->sa, (const u8 *) mgmt, len,
				      &rad_info, 1);
		if (ret == HOSTAPD_ACL_REJECT) {
			wpa_msg(hapd->msg_ctx, MSG_DEBUG,
				"Ignore Probe Request frame from " MACSTR
				" due to ACL reject ", MAC2STR(mgmt->sa));
			return;
		}
	}

	for (i = 0; hapd->probereq_cb && i < hapd->num_probereq_cb; i++)
		if (hapd->probereq_cb[i].cb(hapd->probereq_cb[i].ctx,
					    mgmt->sa, mgmt->da, mgmt->bssid,
					    ie, ie_len, ssi_signal) > 0)
			return;

	if (!hapd->conf->send_probe_response)
		return;

	if (ieee802_11_parse_elems(ie, ie_len, &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "Could not parse ProbeReq from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}

	if ((!elems.ssid || !elems.supp_rates)) {
		wpa_printf(MSG_DEBUG, "STA " MACSTR " sent probe request "
			   "without SSID or supported rates element",
			   MAC2STR(mgmt->sa));
		return;
	}

	/*
	 * No need to reply if the Probe Request frame was sent on an adjacent
	 * channel. IEEE Std 802.11-2012 describes this as a requirement for an
	 * AP with dot11RadioMeasurementActivated set to true, but strictly
	 * speaking does not allow such ignoring of Probe Request frames if
	 * dot11RadioMeasurementActivated is false. Anyway, this can help reduce
	 * number of unnecessary Probe Response frames for cases where the STA
	 * is less likely to see them (Probe Request frame sent on a
	 * neighboring, but partially overlapping, channel).
	 */
	if (elems.ds_params && 0 &&
	    hapd->iface->current_mode &&
	    (hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G ||
	     hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211B) &&
	    hapd->iconf->channel != elems.ds_params[0]) {
		wpa_printf(MSG_DEBUG,
			   "Ignore Probe Request due to DS Params mismatch: chan=%u != ds.chan=%u",
			   hapd->iconf->channel, elems.ds_params[0]);
		return;
	}

#ifdef CONFIG_P2P
	if (hapd->p2p && hapd->p2p_group && elems.wps_ie) {
		struct wpabuf *wps;
		wps = ieee802_11_vendor_ie_concat(ie, ie_len, WPS_DEV_OUI_WFA);
		if (wps && !p2p_group_match_dev_type(hapd->p2p_group, wps)) {
			wpa_printf(MSG_MSGDUMP, "P2P: Ignore Probe Request "
				   "due to mismatch with Requested Device "
				   "Type");
			wpabuf_free(wps);
			return;
		}
		wpabuf_free(wps);
	}

	if (hapd->p2p && hapd->p2p_group && elems.p2p) {
		struct wpabuf *p2p;
		p2p = ieee802_11_vendor_ie_concat(ie, ie_len, P2P_IE_VENDOR_TYPE);
		if (p2p && !p2p_group_match_dev_id(hapd->p2p_group, p2p)) {
			wpa_printf(MSG_MSGDUMP, "P2P: Ignore Probe Request "
				   "due to mismatch with Device ID");
			wpabuf_free(p2p);
			return;
		}
		wpabuf_free(p2p);
	}
#endif /* CONFIG_P2P */

	if (hapd->conf->ignore_broadcast_ssid && elems.ssid_len == 0 &&
	    elems.ssid_list_len == 0 && elems.short_ssid_list_len == 0) {
		wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR " for "
			   "broadcast SSID ignored", MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	    elems.ssid_len == P2P_WILDCARD_SSID_LEN &&
	    os_memcmp(elems.ssid, P2P_WILDCARD_SSID,
		      P2P_WILDCARD_SSID_LEN) == 0) {
		/* Process P2P Wildcard SSID like Wildcard SSID */
		elems.ssid_len = 0;
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_TAXONOMY
	{
		struct sta_info *sta;
		struct hostapd_sta_info *info;

		if ((sta = ap_get_sta(hapd, mgmt->sa)) != NULL) {
			taxonomy_sta_info_probe_req(hapd, sta, ie, ie_len);
		} else if ((info = sta_track_get(hapd->iface,
						 mgmt->sa)) != NULL) {
			taxonomy_hostapd_sta_info_probe_req(hapd, info,
							    ie, ie_len);
		}
	}
#endif /* CONFIG_TAXONOMY */

	res = ssid_match(hapd, elems.ssid, elems.ssid_len,
			 elems.ssid_list, elems.ssid_list_len,
			 elems.short_ssid_list, elems.short_ssid_list_len);
	/* Check the SSID of Non Transmitting BSS if it is not
	 * a broadcast probe request frame
	 */
	if (res == NO_SSID_MATCH && hapd->iconf->mbssid &&
	    !(mgmt->da[0] & 0x01 || mgmt->bssid[0] & 0x01)) {
		if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
			struct hostapd_data *bss;
			struct hostapd_multi_mbssid_group *group = hapd->mbssid_group;

			if (group) {
				dl_list_for_each(bss, &group->bss_list,
						 struct hostapd_data, mbssid_bss) {
					if (bss == hapd)
						continue;
					res = ssid_match(bss, elems.ssid,
							 elems.ssid_len, elems.ssid_list,
							 elems.ssid_list_len,
							 elems.short_ssid_list,
							 elems.short_ssid_list_len);
					if (res != NO_SSID_MATCH)
						break;
				}
			}
		} else {
			for (i = 0; i < hapd->iface->num_bss; i++) {
				if (hapd->iface->bss[i] == hapd)
					continue;
				res = ssid_match(hapd->iface->bss[i], elems.ssid,
						 elems.ssid_len, elems.ssid_list,
						 elems.ssid_list_len,
						 elems.short_ssid_list,
						 elems.short_ssid_list_len);
				if (res != NO_SSID_MATCH)
					break;
			}
		}
	}

	if (res == NO_SSID_MATCH) {
		if (!(mgmt->da[0] & 0x01)) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for foreign SSID '%s' (DA " MACSTR ")%s",
				   MAC2STR(mgmt->sa),
				   wpa_ssid_txt(elems.ssid, elems.ssid_len),
				   MAC2STR(mgmt->da),
				   elems.ssid_list ? " (SSID list)" : "");
		}
		return;
	}

	if (hapd->conf->ignore_broadcast_ssid && res == WILDCARD_SSID_MATCH) {
		wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR " for "
			   "broadcast SSID ignored", MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_INTERWORKING
	if (hapd->conf->interworking &&
	    elems.interworking && elems.interworking_len >= 1) {
		u8 ant = elems.interworking[0] & 0x0f;
		if (ant != INTERWORKING_ANT_WILDCARD &&
		    ant != hapd->conf->access_network_type) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for mismatching ANT %u ignored",
				   MAC2STR(mgmt->sa), ant);
			return;
		}
	}

	if (hapd->conf->interworking && elems.interworking &&
	    (elems.interworking_len == 7 || elems.interworking_len == 9)) {
		const u8 *hessid;
		if (elems.interworking_len == 7)
			hessid = elems.interworking + 1;
		else
			hessid = elems.interworking + 1 + 2;
		if (!is_broadcast_ether_addr(hessid) &&
		    !ether_addr_equal(hessid, hapd->conf->hessid)) {
			wpa_printf(MSG_MSGDUMP, "Probe Request from " MACSTR
				   " for mismatching HESSID " MACSTR
				   " ignored",
				   MAC2STR(mgmt->sa), MAC2STR(hessid));
			return;
		}
	}
#endif /* CONFIG_INTERWORKING */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	    supp_rates_11b_only(&elems)) {
		/* Indicates support for 11b rates only */
		wpa_printf(MSG_EXCESSIVE, "P2P: Ignore Probe Request from "
			   MACSTR " with only 802.11b rates",
			   MAC2STR(mgmt->sa));
		return;
	}
#endif /* CONFIG_P2P */

	if (hostapd_ubus_handle_event(hapd, &req)) {
		wpa_printf(MSG_DEBUG, "Probe request for " MACSTR " rejected by ubus handler.\n",
		       MAC2STR(mgmt->sa));
		return;
	}

	/* TODO: verify that supp_rates contains at least one matching rate
	 * with AP configuration */

	if (hapd->conf->no_probe_resp_if_seen_on &&
	    is_multicast_ether_addr(mgmt->da) &&
	    is_multicast_ether_addr(mgmt->bssid) &&
	    sta_track_seen_on(hapd->iface, mgmt->sa,
			      hapd->conf->no_probe_resp_if_seen_on)) {
		wpa_printf(MSG_MSGDUMP, "%s: Ignore Probe Request from " MACSTR
			   " since STA has been seen on %s",
			   hapd->conf->iface, MAC2STR(mgmt->sa),
			   hapd->conf->no_probe_resp_if_seen_on);
		return;
	}

	if (hapd->conf->no_probe_resp_if_max_sta &&
	    is_multicast_ether_addr(mgmt->da) &&
	    is_multicast_ether_addr(mgmt->bssid) &&
	    hostapd_check_max_sta(hapd) &&
	    !ap_get_sta(hapd, mgmt->sa)) {
		wpa_printf(MSG_MSGDUMP, "%s: Ignore Probe Request from " MACSTR
			   " since no room for additional STA",
			   hapd->conf->iface, MAC2STR(mgmt->sa));
		return;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iconf->ignore_probe_probability > 0.0 &&
	    drand48() < hapd->iconf->ignore_probe_probability) {
		wpa_printf(MSG_INFO,
			   "TESTING: ignoring probe request from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Do not send Probe Response frame from a non-transmitting multiple
	 * BSSID profile unless the Probe Request frame is directed at that
	 * particular BSS. */
	if (hapd != hostapd_mbssid_get_tx_bss(hapd) && res != EXACT_SSID_MATCH)
		return;

	if (hapd->conf->notify_mgmt_frames) {
		size_t hex_len;

		hex_len = len * 2 + 1;
		hex = os_malloc(hex_len);
		if (hex)
			wpa_snprintf_hex(hex, hex_len, (const u8 *) mgmt, len);
	}

	if (fi) {
		int snr = 0;
		if (fi->channel && hapd->iface->lowest_nf) {
			snr = ssi_signal - hapd->iface->lowest_nf;
			wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO, RX_PROBE_REQUEST "sa=" MACSTR
				     " signal=%d channel=%u snr=%d rate=%u%s%s",
				     MAC2STR(mgmt->sa), ssi_signal, fi->channel,
				     snr, fi->datarate ? fi->datarate : 0,
				     hex ? " buf=" : "", hex ? hex : "");
		} else if (fi->channel) {
			wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO, RX_PROBE_REQUEST "sa=" MACSTR
				     " signal=%d channel=%u rate=%u%s%s",
				     MAC2STR(mgmt->sa), ssi_signal, fi->channel,
				     fi->datarate ? fi->datarate : 0,
				     hex ? " buf=" : "", hex ? hex : "");
		} else {
			wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO, RX_PROBE_REQUEST "sa=" MACSTR
				     " signal=%d rate=%u%s%s",
				     MAC2STR(mgmt->sa), ssi_signal,
				     fi->datarate ? fi->datarate : 0,
				     hex ? " buf=" : "", hex ? hex : "");
		}
	} else {
		wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO, RX_PROBE_REQUEST "sa=" MACSTR
			     " signal=%d%s%s",
			     MAC2STR(mgmt->sa), ssi_signal,
			     hex ? " buf=" : "", hex ? hex : "");
	}

	os_free(hex);

	os_memset(&params, 0, sizeof(params));

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && elems.probe_req_mle &&
	    parse_ml_probe_req((struct ieee80211_eht_ml *) elems.probe_req_mle,
			       elems.probe_req_mle_len, &mld_id, &links)) {
		params.is_ml_probe = true;
		hostapd_fill_probe_resp_ml_params(hapd, &params, mgmt,
						  mld_id, links);
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	params.req = mgmt;
	params.is_p2p = !!elems.p2p;

	params.known_bss = elems.mbssid_known_bss;
	params.known_bss_len = elems.mbssid_known_bss_len;

#ifdef CONFIG_MBO
	if ((hapd->conf->oce & OCE_AP) &&
	    is_broadcast_ether_addr(mgmt->da) &&
	    ieee80211_is_oce_capable(elems.mbo, elems.mbo_len)) {
		/*
		 * OCE spec v2.0 section 3.5.1: if MaxChannelTime >= beacon interval,
		 * the STA can hear the next beacon — no probe response needed.
		 * MaxChannelTime is FILS Request Parameters element byte[1].
		 */
		if (oce_probe_req_is_suppressed(hapd, elems.mbo, elems.mbo_len) ||
		    (elems.fils_req_params && elems.fils_req_params_len >= 2 &&
		    elems.fils_req_params[1] != 255 &&
		    (u16) elems.fils_req_params[1] >= hapd->iconf->beacon_int))
			return;

		/* Wi-Fi Optimized Connectivity Specification v2.0, Section 3.6:
		 * respond with a broadcast Probe Response on non-6 GHz bands. */
		if (!is_6ghz_op_class(hapd->iconf->op_class))
			params.force_bcast_resp_oce_non6ghz = true;
	}
#endif /* CONFIG_MBO */

	hostapd_gen_probe_resp(hapd, &params);

	hostapd_free_probe_resp_params(&params);

	if (!params.resp)
		return;

#ifdef CONFIG_MBO
	/* TODO: Use OCE_AP_ENABLED() */
	if ((hapd->conf->oce & OCE_AP) &&
	    (hw_mode == HOSTAPD_MODE_IEEE80211G ||
	     hw_mode == HOSTAPD_MODE_IEEE80211B) &&
	    (is_broadcast_ether_addr(params.resp->da) ||
	     (ieee80211_is_oce_capable(elems.mbo, elems.mbo_len)))) {
		if (!is_broadcast_ether_addr(params.resp->da) &&
		    (bitrate && bitrate < BITRATE_5_5_MBPS)) {
			rate = bitrate;
			rate_type = RATE_LEGACY;
		} else if (hapd->conf->probe_resp_rate_type ||
			   (hapd->conf->probe_resp_rate >= BITRATE_5_5_MBPS)) {
			rate = hapd->conf->probe_resp_rate;
			rate_type = hapd->conf->probe_resp_rate_type;
		} else {
			rate = BITRATE_5_5_MBPS;
			rate_type = RATE_LEGACY;
		}
	} else
#endif /* CONFIG_MBO */
	if (hapd->conf->probe_resp_rate_type ||
	    hapd->conf->probe_resp_rate) {
		rate = hapd->conf->probe_resp_rate;
		rate_type = hapd->conf->probe_resp_rate_type;
	}

	/*
	 * If this is a broadcast probe request, apply no ack policy to avoid
	 * excessive retries.
	 */
	noack = !!(res == WILDCARD_SSID_MATCH &&
		   is_broadcast_ether_addr(mgmt->da));

	csa_offs_len = 0;
	if (hapd->csa_in_progress) {
		if (params.csa_pos)
			csa_offs[csa_offs_len++] =
				params.csa_pos - (u8 *) params.resp;

		if (params.ecsa_pos)
			csa_offs[csa_offs_len++] =
				params.ecsa_pos - (u8 *) params.resp;
	}

#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211AC)
	if (is_mu_cap_war_active(hapd) && elems.is_mu_cap_war_vendor &&
	    is_sta_elems_vht_only(&elems)) {
		u8 *vht_cap_ie = NULL;
		u8 *pos, *end, *ies;
		size_t ies_len, fixed_hdr;

		fixed_hdr = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);

		if (params.resp_len <= fixed_hdr)
			goto skip_mu_cap_war;

		ies_len = params.resp_len - fixed_hdr;
		ies = params.resp->u.probe_resp.variable;
		pos = (u8 *) ies;
		end = pos + ies_len;

		while (end - pos >= 2) {
			u8 eid = pos[0];
			u8 elen = pos[1];

			if (pos + 2 + elen > end)
				break;

			if (eid == WLAN_EID_VHT_CAP) {
				vht_cap_ie = pos;
				break;
			}

			pos += 2 + elen;
		}

		if (vht_cap_ie)
			hostapd_mu_cap_war_update_db_extn(hapd, mgmt->sa,
							  vht_cap_ie);
skip_mu_cap_war:
	}
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211AC */

#ifdef RDK_ONEWIFI
       hapd = hostapd_drv_mbssid_get_tx_bss(hapd);
#endif /* RDK_ONEWIFI */

	ret = hostapd_drv_send_mlme(hostapd_mbssid_get_tx_bss(hapd),
				    params.resp, params.resp_len, noack,
				    csa_offs_len ? csa_offs : NULL,
				    csa_offs_len, 0, rate, rate_type);

	if (ret < 0)
		wpa_printf(MSG_INFO, "handle_probe_req: send failed");

	os_free(params.resp);

	wpa_printf(MSG_EXCESSIVE, "STA " MACSTR " sent probe request for %s "
		   "SSID", MAC2STR(mgmt->sa),
		   elems.ssid_len == 0 ? "broadcast" : "our");
}


static u8 * hostapd_probe_resp_offloads(struct hostapd_data *hapd,
					size_t *resp_len)
{
	struct probe_resp_params params;

	/* check probe response offloading caps and print warnings */
	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_PROBE_RESP_OFFLOAD))
		return NULL;

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_probe_resp_ie &&
	    (!(hapd->iface->probe_resp_offloads &
	       (WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS |
		WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS2))))
		wpa_printf(MSG_WARNING, "Device is trying to offload WPS "
			   "Probe Response while not supporting this");
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_probe_resp_ie &&
	    !(hapd->iface->probe_resp_offloads &
	      WPA_DRIVER_PROBE_RESP_OFFLOAD_P2P))
		wpa_printf(MSG_WARNING, "Device is trying to offload P2P "
			   "Probe Response while not supporting this");
#endif  /* CONFIG_P2P */

	if (hapd->conf->interworking &&
	    !(hapd->iface->probe_resp_offloads &
	      WPA_DRIVER_PROBE_RESP_OFFLOAD_INTERWORKING))
		wpa_printf(MSG_WARNING, "Device is trying to offload "
			   "Interworking Probe Response while not supporting "
			   "this");

	/* Generate a Probe Response template for the non-P2P case */
	os_memset(&params, 0, sizeof(params));
	params.req = NULL;
	params.is_p2p = false;
	params.known_bss = NULL;
	params.known_bss_len = 0;
	params.mld_ap = NULL;
	params.mld_info = NULL;

	hostapd_gen_probe_resp(hapd, &params);
	*resp_len = params.resp_len;
	if (!params.resp)
		return NULL;

	/* TODO: Avoid passing these through struct hostapd_data */
	if (params.csa_pos)
		hapd->cs_c_off_proberesp = params.csa_pos - (u8 *) params.resp;
	if (params.ecsa_pos)
		hapd->cs_c_off_ecsa_proberesp = params.ecsa_pos -
			(u8 *) params.resp;
#ifdef CONFIG_IEEE80211AX
	if (params.cca_pos)
		hapd->cca_c_off_proberesp = params.cca_pos - (u8 *) params.resp;
#endif /* CONFIG_IEEE80211AX */

	return (u8 *) params.resp;
}

#endif /* NEED_AP_MLME */


#ifdef CONFIG_IEEE80211AX
/* Unsolicited broadcast Probe Response(UBPR) transmission, 6 GHz only */
u8 * hostapd_unsol_bcast_probe_resp(struct hostapd_data *hapd,
				    struct unsol_bcast_probe_resp *ubpr)
{
	struct probe_resp_params probe_params;

	/* Do not enable UBPR in 6GHz AP if colocated with lower band APs */
	hapd->conf->ubpr_state = FILS_UBPR_USER_DISABLED;

	if (!is_6ghz_op_class(hapd->iconf->op_class))
		return NULL;

	if (hapd->conf->unsol_bcast_probe_resp_interval &&
	    hapd->conf->force_disable_in_band_discovery &&
	    (get_colocation_mode(hapd) == COLOCATED_6GHZ)) {
		hapd->conf->ubpr_state = FILS_UBPR_FORCE_DISABLED;
		return NULL;
	}

	ubpr->unsol_bcast_probe_resp_interval =
		hapd->conf->unsol_bcast_probe_resp_interval;

	if (ubpr->unsol_bcast_probe_resp_interval)
		hapd->conf->ubpr_state = FILS_UBPR_ENABLED;

	os_memset(&probe_params, 0, sizeof(probe_params));
	probe_params.req = NULL;
	probe_params.is_p2p = false;
	probe_params.known_bss = NULL;
	probe_params.known_bss_len = 0;
	probe_params.mld_ap = NULL;
	probe_params.mld_info = NULL;

	hostapd_gen_probe_resp(hapd, &probe_params);
	if (!probe_params.resp)
		return NULL;
	ubpr->unsol_bcast_probe_resp_tmpl_len = probe_params.resp_len;
	return (u8 *) probe_params.resp;
}
#endif /* CONFIG_IEEE80211AX */


void sta_track_del(struct hostapd_sta_info *info)
{
#ifdef CONFIG_TAXONOMY
	wpabuf_free(info->probe_ie_taxonomy);
	info->probe_ie_taxonomy = NULL;
#endif /* CONFIG_TAXONOMY */
	os_free(info);
}


#ifdef CONFIG_FILS

static u16 hostapd_gen_fils_discovery_phy_index(struct hostapd_data *hapd)
{
#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd))
		return FD_CAP_PHY_INDEX_EHT;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd))
		return FD_CAP_PHY_INDEX_HE;
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211AC
	if (hostapd_is_vht_enabled(hapd))
		return FD_CAP_PHY_INDEX_VHT;
#endif /* CONFIG_IEEE80211AC */

	if (hostapd_is_ht_enabled(hapd))
		return FD_CAP_PHY_INDEX_HT;

	return 0;
}


static u16 hostapd_gen_fils_discovery_nss(struct hostapd_hw_modes *mode,
					  u16 phy_index, u8 he_mcs_nss_size)
{
	u16 nss = 0;

	if (!mode)
		return 0;

	if (phy_index == FD_CAP_PHY_INDEX_HE) {
		const u8 *he_mcs = mode->he_capab[IEEE80211_MODE_AP].mcs;
		int i;
		u16 mcs[6];

		os_memset(mcs, 0xff, 6 * sizeof(u16));

		if (he_mcs_nss_size == 4) {
			mcs[0] = WPA_GET_LE16(&he_mcs[0]);
			mcs[1] = WPA_GET_LE16(&he_mcs[2]);
		}

		if (he_mcs_nss_size == 8) {
			mcs[2] = WPA_GET_LE16(&he_mcs[4]);
			mcs[3] = WPA_GET_LE16(&he_mcs[6]);
		}

		if (he_mcs_nss_size == 12) {
			mcs[4] = WPA_GET_LE16(&he_mcs[8]);
			mcs[5] = WPA_GET_LE16(&he_mcs[10]);
		}

		for (i = 0; i < HE_NSS_MAX_STREAMS; i++) {
			u16 nss_mask = 0x3 << (i * 2);

			/*
			 * If Tx and/or Rx indicate support for a given NSS,
			 * count it towards the maximum NSS.
			 */
			if (he_mcs_nss_size == 4 &&
			    (((mcs[0] & nss_mask) != nss_mask) ||
			     ((mcs[1] & nss_mask) != nss_mask))) {
				nss++;
				continue;
			}

			if (he_mcs_nss_size == 8 &&
			    (((mcs[2] & nss_mask) != nss_mask) ||
			     ((mcs[3] & nss_mask) != nss_mask))) {
				nss++;
				continue;
			}

			if (he_mcs_nss_size == 12 &&
			    (((mcs[4] & nss_mask) != nss_mask) ||
			     ((mcs[5] & nss_mask) != nss_mask))) {
				nss++;
				continue;
			}
		}
	} else if (phy_index == FD_CAP_PHY_INDEX_EHT) {
		u8 rx_nss, tx_nss, max_nss = 0, i;
		u8 *mcs = mode->eht_capab[IEEE80211_MODE_AP].mcs;

		/*
		 * The Supported EHT-MCS And NSS Set field for the AP contains
		 * one to three EHT-MCS Map fields based on the supported
		 * bandwidth. Check the first byte (max NSS for Rx/Tx that
		 * supports EHT-MCS 0-9) for each bandwidth (<= 80,
		 * 160, 320) to find the maximum NSS. This assumes that
		 * the lowest MCS rates support the largest number of spatial
		 * streams. If values are different between Tx, Rx or the
		 * bandwidths, choose the highest value.
		 */
		for (i = 0; i < 3; i++) {
			rx_nss = mcs[3 * i] & 0x0F;
			if (rx_nss > max_nss)
				max_nss = rx_nss;

			tx_nss = (mcs[3 * i] & 0xF0) >> 4;
			if (tx_nss > max_nss)
				max_nss = tx_nss;
		}

		nss = max_nss;
	}

	if (nss > 4)
		return FD_CAP_NSS_5_8 << FD_CAP_NSS_SHIFT;
	if (nss)
		return (nss - 1) << FD_CAP_NSS_SHIFT;

	return 0;
}


static u16 hostapd_fils_discovery_cap(struct hostapd_data *hapd)
{
	u16 cap_info, phy_index;
	u8 chwidth = FD_CAP_BSS_CHWIDTH_20, he_mcs_nss_size = 4;
	struct hostapd_hw_modes *mode = hapd->iface->current_mode;

	cap_info = FD_CAP_ESS;
	if (hapd->conf->wpa)
		cap_info |= FD_CAP_PRIVACY;

	if (is_6ghz_op_class(hapd->iconf->op_class)) {
		switch (hapd->iconf->op_class) {
		case 137:
			chwidth = FD_CAP_BSS_CHWIDTH_320;
			break;
		case 135:
			he_mcs_nss_size += 4;
			/* fallthrough */
		case 134:
			he_mcs_nss_size += 4;
			chwidth = FD_CAP_BSS_CHWIDTH_160_80_80;
			break;
		case 133:
			chwidth = FD_CAP_BSS_CHWIDTH_80;
			break;
		case 132:
			chwidth = FD_CAP_BSS_CHWIDTH_40;
			break;
		}
	} else {
		switch (hostapd_get_oper_chwidth(hapd->iconf)) {
		case CONF_OPER_CHWIDTH_80P80MHZ:
			he_mcs_nss_size += 4;
			/* fallthrough */
		case CONF_OPER_CHWIDTH_160MHZ:
			he_mcs_nss_size += 4;
			chwidth = FD_CAP_BSS_CHWIDTH_160_80_80;
			break;
		case CONF_OPER_CHWIDTH_80MHZ:
			chwidth = FD_CAP_BSS_CHWIDTH_80;
			break;
		case CONF_OPER_CHWIDTH_USE_HT:
			if (hapd->iconf->secondary_channel)
				chwidth = FD_CAP_BSS_CHWIDTH_40;
			else
				chwidth = FD_CAP_BSS_CHWIDTH_20;
			break;
		default:
			break;
		}
	}

	phy_index = hostapd_gen_fils_discovery_phy_index(hapd);
	cap_info |= phy_index << FD_CAP_PHY_INDEX_SHIFT;
	cap_info |= chwidth << FD_CAP_BSS_CHWIDTH_SHIFT;
	cap_info |= hostapd_gen_fils_discovery_nss(mode, phy_index,
						   he_mcs_nss_size);

	if (hapd->iconf->mbssid != MBSSID_DISABLED)
		cap_info |= FD_CAP_MULTI_BSSID_PRESENT;

	return cap_info;
}


static u8 * hostapd_gen_fils_discovery(struct hostapd_data *hapd, size_t *len)
{
	struct ieee80211_mgmt *head;
	const u8 *mobility_domain;
	u8 *pos, *length_pos, buf[200];
	u16 ctl = 0;
	u8 fd_rsn_info[5];
	size_t total_len, buf_len;

	total_len = 24 + 2 + 12;

	/* FILS Discovery Frame Control */
	ctl = (sizeof(hapd->conf->ssid.short_ssid) - 1) |
		FD_FRAME_CTL_SHORT_SSID_PRESENT |
		FD_FRAME_CTL_LENGTH_PRESENT |
		FD_FRAME_CTL_CAP_PRESENT;
	total_len += 4 + 1 + 2;

	/* Fill primary channel information for 6 GHz channels with over 20 MHz
	 * bandwidth, if the primary channel is not a PSC */
	if (is_6ghz_op_class(hapd->iconf->op_class) &&
	    !is_6ghz_psc_frequency(ieee80211_chan_to_freq(
					   NULL, hapd->iconf->op_class,
					   hapd->iconf->channel)) &&
	    op_class_to_bandwidth(hapd->iconf->op_class) > 20) {
		ctl |= FD_FRAME_CTL_PRI_CHAN_PRESENT;
		total_len += 2;
	}

	/* Check for optional subfields and calculate length */
	if (wpa_auth_write_fd_rsn_info(hapd->wpa_auth, fd_rsn_info)) {
		ctl |= FD_FRAME_CTL_RSN_INFO_PRESENT;
		total_len += sizeof(fd_rsn_info);
	}

	mobility_domain = hostapd_wpa_ie(hapd, WLAN_EID_MOBILITY_DOMAIN);
	if (mobility_domain) {
		ctl |= FD_FRAME_CTL_MD_PRESENT;
		total_len += 3;
	}

	total_len += hostapd_eid_rnr_len(hapd, WLAN_FC_STYPE_ACTION, true);

	pos = hostapd_eid_fils_indic(hapd, buf, 0);
	buf_len = pos - buf;
	total_len += buf_len;

#ifdef CONFIG_IEEE80211AX
        /* Transmit Power Envelope element(s) */
        if (is_6ghz_op_class(hapd->iconf->op_class)) {
                total_len += 3 + MAX_PSD_TPE_POWER_COUNT +
                             1 + MAX_PSD_TPE_EXT_POWER_COUNT;
                if (hapd->iconf->he_6ghz_reg_pwr_type == HE_REG_INFO_6GHZ_AP_TYPE_INDOOR)
                        total_len += 3 + MAX_PSD_TPE_POWER_COUNT +
                                     1 + MAX_PSD_TPE_EXT_POWER_COUNT;
		if (hapd->iconf->he_6ghz_reg_pwr_type == HE_REG_INFO_6GHZ_AP_TYPE_SP) {
			total_len += 3 + MAX_TPE_EIRP_NUM_POWER_SUPPORTED +
				     1 + MAX_EIRP_TPE_POWER_EXT_COUNT;
		}

        }
#endif /* CONFIG_IEEE80211AX */

	/* he_elem_len() may return too large a value for FD frame, but that is
	 * fine here since this is used as the maximum length of the buffer. */
	total_len += he_elem_len(hapd);

	head = os_zalloc(total_len);
	if (!head)
		return NULL;

	head->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_ACTION);
	os_memset(head->da, 0xff, ETH_ALEN);
	os_memcpy(head->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(head->bssid, hapd->own_addr, ETH_ALEN);

	head->u.action.category = WLAN_ACTION_PUBLIC;
	head->u.action.u.public_action.action = WLAN_PA_FILS_DISCOVERY;

	pos = &head->u.action.u.public_action.variable[0];

	/* FILS Discovery Information field */

	/* FILS Discovery Frame Control */
	WPA_PUT_LE16(pos, ctl);
	pos += 2;

	/* Hardware or low-level driver will fill in the Timestamp value */
	pos += 8;

	/* Beacon Interval */
	WPA_PUT_LE16(pos, hapd->iconf->beacon_int);
	pos += 2;

	/* Short SSID */
	WPA_PUT_LE32(pos, hapd->conf->ssid.short_ssid);
	pos += sizeof(hapd->conf->ssid.short_ssid);

	/* Store position of FILS discovery information element Length field */
	length_pos = pos++;

	/* FD Capability */
	WPA_PUT_LE16(pos, hostapd_fils_discovery_cap(hapd));
	pos += 2;

	/* Operating Class and Primary Channel - if a 6 GHz chan is non PSC */
	if (ctl & FD_FRAME_CTL_PRI_CHAN_PRESENT) {
		*pos++ = hapd->iconf->op_class;
		*pos++ = hapd->iconf->channel;
	}

	/* AP Configuration Sequence Number - not present */

	/* Access Network Options - not present */

	/* FD RSN Information */
	if (ctl & FD_FRAME_CTL_RSN_INFO_PRESENT) {
		os_memcpy(pos, fd_rsn_info, sizeof(fd_rsn_info));
		pos += sizeof(fd_rsn_info);
	}

	/* Channel Center Frequency Segment 1 - not present */

	/* Mobility Domain */
	if (ctl & FD_FRAME_CTL_MD_PRESENT) {
		os_memcpy(pos, &mobility_domain[2], 3);
		pos += 3;
	}

	/* Fill in the Length field value */
	*length_pos = pos - (length_pos + 1);

	pos = hostapd_eid_rnr(hapd, pos, WLAN_FC_STYPE_ACTION, true);

	/* FILS Indication element */
	if (buf_len) {
		os_memcpy(pos, buf, buf_len);
		pos += buf_len;
	}

	if (is_6ghz_op_class(hapd->iconf->op_class))
		pos = hostapd_eid_txpower_envelope(hapd, pos);

	*len = pos - (u8 *) head;
	wpa_hexdump(MSG_DEBUG, "FILS Discovery frame template",
		    head, pos - (u8 *) head);
	return (u8 *) head;
}


/* Configure FILS Discovery frame transmission parameters */
static u8 * hostapd_fils_discovery(struct hostapd_data *hapd,
				   struct wpa_driver_ap_params *params)
{
	/*
	 * Fils discovery for 6GHz AP can be enabled even if its colocated
	 * with lower band APs if user sets force_disable_in_band_discovery
	 * to zero.
	 */

	if (is_6ghz_op_class(hapd->iconf->op_class) &&
	    hapd->conf->force_disable_in_band_discovery &&
	    get_colocation_mode(hapd) == COLOCATED_6GHZ &&
	    hapd->conf->fils_discovery_max_int) {
		hapd->conf->fils_state = FILS_UBPR_FORCE_DISABLED;
		return NULL;
	}

	params->fd_max_int = hapd->conf->fils_discovery_max_int;
	if (is_6ghz_op_class(hapd->iconf->op_class) &&
	    params->fd_max_int > FD_MAX_INTERVAL_6GHZ)
		params->fd_max_int = FD_MAX_INTERVAL_6GHZ;

	params->fd_min_int = hapd->conf->fils_discovery_min_int;
	if (params->fd_min_int > params->fd_max_int)
		params->fd_min_int = params->fd_max_int;

	if (params->fd_max_int) {
		hapd->conf->fils_state = FILS_UBPR_ENABLED;
		return hostapd_gen_fils_discovery(hapd,
						  &params->fd_frame_tmpl_len);
	}
	hapd->conf->fils_state = FILS_UBPR_USER_DISABLED;

	return NULL;
}

#endif /* CONFIG_FILS */

u8 *hostapd_add_traffic_ind_elem(struct hostapd_data *hapd, u8 *eid)
{
#ifdef CONFIG_IEEE80211BE
	struct ml_traffic_indication_elem *ml_tim;

	ml_tim = (struct ml_traffic_indication_elem *)eid;
	ml_tim->elem_id = WLAN_EID_EXTENSION;
	ml_tim->elem_id_extn = WLAN_EID_EXT_MULTI_LINK_TRAFFIC_INDICATION;
	ml_tim->elem_len = sizeof(*ml_tim) - sizeof(struct elem_header);
	ml_tim->ml_traffic_ind_control = 0;
	ml_tim->per_link_traffic_ind_list[0] = 0;
	eid += sizeof(*ml_tim);

#endif
	return eid;
}

int ieee802_11_build_nontx_bss_params(struct hostapd_data *hapd,
				      struct wpa_driver_ap_params *params)
{
	u8 *tail;
#ifdef NEED_AP_MLME
	u8 *tailpos, *tailend, *startpos;
	u16 elemid_modified = 0;
#endif /* NEED_AP_MLME */
	size_t tail_len = 0;

#ifdef NEED_AP_MLME
#define MBSSID_NON_TX_BEACON_TAIL_SIZE 600
	tail_len = MBSSID_NON_TX_BEACON_TAIL_SIZE;

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie)
		tail_len += wpabuf_len(hapd->wps_beacon_ie);
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if (hapd->p2p_beacon_ie)
		tail_len += wpabuf_len(hapd->p2p_beacon_ie);
#endif /* CONFIG_P2P */

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies)
		tail_len += wpabuf_len(hapd->iface->fst_ies);
#endif /* CONFIG_FST */

	tail_len += hostapd_mbo_ie_len(hapd);
#ifdef CONFIG_MBO
	if (OCE_AP_ENABLED(hapd) && hapd->conf->oce_ess_report_enabled)
		tail_len += 4; /* ESS Report element: EID(1)+Len(1)+EID_EXT(1)+ESS_Info(1) */
#endif /* CONFIG_MBO */
	tail_len += hostapd_eid_owe_trans_len(hapd);
	tail_len += hostapd_eid_dpp_cc_len(hapd);
	tail_len += hostapd_get_rsne_override_len(hapd);
	tail_len += hostapd_get_rsne_override_2_len(hapd);
	tail_len += hostapd_get_rsnxe_override_len(hapd);
	tail_len += hostapd_wfa_cap_ie_len(hapd, NULL);
	tail_len += hostapd_tpc_report_len(hapd);
#ifdef CONFIG_QCN_EXTN
	tail_len += hostapd_modify_buflen_for_qcn_ie_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BN
	tail_len += hostapd_smd_ie_len(hapd);
#endif /* CONFIG_IEEE80211BN */

	tail_len += hostapd_security_profile_ie_len(hapd);

	tailpos = tail = os_malloc(tail_len);
	if (tail == NULL) {
		wpa_printf(MSG_ERROR,
			   "Failed to allocate beacon tail for non-TX BSS");
		return -1;
	}

	tailend = tail + tail_len;

	tailpos = hostapd_eid_supp_rates(hapd, tailpos);

	/* Power Constraint element */
	tailpos = hostapd_eid_pwr_constraint(hapd, tailpos);
	tailpos = hostapd_eid_tpc_report(hapd, tailpos);

	/* Extended supported rates */
	tailpos = hostapd_eid_ext_supp_rates(hapd, tailpos);

	tailpos = hostapd_get_rsne(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_bss_load(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_rm_enabled_capab(hapd, tailpos,
			tailend - tailpos);
	tailpos = hostapd_get_mde(hapd, tailpos, tailend - tailpos);

	tailpos = hostapd_eid_ext_capab(hapd, tailpos, false);

	/*
	 * TODO: Time Advertisement element should only be included in some
	 * DTIM Beacon frames.
	 */
	tailpos = hostapd_eid_time_adv(hapd, tailpos);

	tailpos = hostapd_eid_interworking(hapd, tailpos);
	tailpos = hostapd_eid_adv_proto(hapd, tailpos);
	tailpos = hostapd_eid_roaming_consortium(hapd, tailpos);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(tailpos, wpabuf_head(hapd->iface->fst_ies),
				wpabuf_len(hapd->iface->fst_ies));
		tailpos += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

	tailpos = hostapd_eid_fils_indic(hapd, tailpos, 0);

	tailpos = hostapd_get_rsnxe(hapd, tailpos, tailend - tailpos);
#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd)) {
		startpos = tailpos;
		tailpos = hostapd_eid_he_mu_edca_parameter_set(hapd, tailpos, false);
#ifdef CONFIG_IEEE80211BE
		hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
				tailpos-startpos, ELEMID_CU_PARAM_MU_EDCA);
#endif
	}
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap)
		tailpos = hostapd_add_traffic_ind_elem(hapd, tailpos);
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	/* WPA */
	tailpos = hostapd_get_wpa_ie(hapd, tailpos, tailend - tailpos);

	tailpos = hostapd_add_wfa_cap_ie(hapd, NULL, tailpos);
#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->wps_beacon_ie),
				wpabuf_len(hapd->wps_beacon_ie));
		tailpos += wpabuf_len(hapd->wps_beacon_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->p2p_beacon_ie),
				wpabuf_len(hapd->p2p_beacon_ie));
		tailpos += wpabuf_len(hapd->p2p_beacon_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
			P2P_MANAGE)
		tailpos = hostapd_eid_p2p_manage(hapd, tailpos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	tailpos = hostapd_eid_hs20_indication(hapd, tailpos);
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MBO
	tailpos = hostapd_eid_ess_report(hapd, tailpos,
					 tail + tail_len - tailpos);
#endif /* CONFIG_MBO */
#ifdef CONFIG_MBO
	tailpos = hostapd_eid_ap_channel_report(hapd, tailpos,
			  tail + tail_len - tailpos);
#endif /* CONFIG_MBO */
	tailpos = hostapd_eid_mbo(hapd, tailpos, tail + tail_len - tailpos);
	tailpos = hostapd_eid_owe_trans(hapd, tailpos,
			tail + tail_len - tailpos);
	tailpos = hostapd_eid_dpp_cc(hapd, tailpos, tail + tail_len - tailpos);

	tailpos = hostapd_get_rsne_override(hapd, tailpos,
			tail + tail_len - tailpos);
	tailpos = hostapd_get_rsne_override_2(hapd, tailpos,
			tail + tail_len - tailpos);
	tailpos = hostapd_get_rsnxe_override(hapd, tailpos,
			tail + tail_len - tailpos);
#ifdef CONFIG_QCN_EXTN
	tailpos = hostapd_eid_qcn_vendor_ie_extn(hapd, tailpos, IEEE80211_MODE_AP);
#endif /* CONFIG_QCN_EXTN */

	tailpos = hostapd_eid_security_profile(hapd, tailpos);

	tail_len = tailpos > tail ? tailpos - tail : 0;
#endif /* NEED_AP_MLME */

	params->tail = tail;
	params->tail_len = tail_len;
	return 0;
}


int ieee802_11_build_ap_params(struct hostapd_data *hapd,
			       struct wpa_driver_ap_params *params)
{
	struct ieee80211_mgmt *head = NULL;
	u8 *tail = NULL;
	size_t head_len = 0, tail_len = 0;
	u8 *resp = NULL;
	size_t i, resp_len = 0;
#ifdef NEED_AP_MLME
	u16 capab_info;
	u8 *pos, *tailpos, *tailend, *csa_pos;
	bool complete = false;
	u8 *startpos;
	u8 *extcap_elem, *mbssid_cfg_elem;
	u8 *complete_nontx_prof_list = NULL;
	u8 *mbssid_cfg_periodicity = NULL;
	u16 elemid_modified = 0;
	struct hostapd_data *tx_bss;
	u8 ext_cap = 0;
#endif /* NEED_AP_MLME */

	os_memset(params, 0, sizeof(*params));

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED &&
	    !hapd->mbssid_group) {
		wpa_printf(MSG_ERROR,
			   "MBSSID: mbssid_group is null for %s op_class:%u chan:%u num_bss:%zu, started:%u beacon_set_done:%d, csa_in_progress:%d ",
			   hapd->conf->iface, hapd->iconf->op_class, hapd->iconf->channel,
			   hapd->iface->num_bss, hapd->started, hapd->beacon_set_done,
			   hapd->csa_in_progress);
		return -1;
	}

	tx_bss = hostapd_mbssid_get_tx_bss(hapd);

#ifdef NEED_AP_MLME
#define BEACON_HEAD_BUF_SIZE 256
#define BEACON_TAIL_BUF_SIZE 1500
	head = os_zalloc(BEACON_HEAD_BUF_SIZE);
	tail_len = BEACON_TAIL_BUF_SIZE;
#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie)
		tail_len += wpabuf_len(hapd->wps_beacon_ie);
#endif /* CONFIG_WPS */
#ifdef CONFIG_P2P
	if (hapd->p2p_beacon_ie)
		tail_len += wpabuf_len(hapd->p2p_beacon_ie);
#endif /* CONFIG_P2P */
#ifdef CONFIG_FST
	if (hapd->iface->fst_ies)
		tail_len += wpabuf_len(hapd->iface->fst_ies);
#endif /* CONFIG_FST */

	/* Use plugin vendor elements if set, otherwise use conf vendor elements */
	if (hapd->plugin_vendor_elements)
		tail_len += wpabuf_len(hapd->plugin_vendor_elements);
	else if (hapd->conf->vendor_elements_count)
		tail_len += hapd->conf->vendor_elements_len;

#ifdef CONFIG_IEEE80211AC
	if (hapd->conf->vendor_vht) {
		tail_len += 5 + 2 + sizeof(struct ieee80211_vht_capabilities) +
			2 + sizeof(struct ieee80211_vht_operation);
	}
#endif /* CONFIG_IEEE80211AC */

	tail_len += he_elem_len(hapd);

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		tail_len += hostapd_eid_eht_capab_len(hapd, IEEE80211_MODE_AP);
		tail_len += 3 + sizeof(struct ieee80211_eht_operation);

		/* Add space in beacon tail for Channel Usage element */
		tail_len += hostapd_eid_channel_usage_len(hapd);

		if (hapd->iconf->punct_bitmap)
			tail_len += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;

		/*
		 * TODO: Multi-Link element has variable length and can be
		 * long based on the common info and number of per
		 * station profiles. For now use 256.
		 */
		if (hapd->conf->mld_ap) {
			tail_len += 256;

			/* for Max Channel Switch Time element during channel
			 * switch */
			tail_len += 6;
		}
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		tail_len += (3 + sizeof(struct ieee80211_uhr_operation));
		tail_len += hostapd_eid_tx_power_indication_len();
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef RDK_ONEWIFI
       if (hapd->iconf->mbssid == MBSSID_ENABLED)
               tail_len += 5; /* Multiple BSSID Configuration element */
#endif /* RDK_ONEWIFI */
	tail_len += hostapd_security_profile_ie_len(hapd);

	if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
	    hapd == hostapd_mbssid_get_tx_bss(hapd))
		tail_len += 5; /* Multiple BSSID Configuration element */
	tail_len += hostapd_eid_rnr_len(hapd, WLAN_FC_STYPE_BEACON, true);
	tail_len += hostapd_mbo_ie_len(hapd);
#ifdef CONFIG_MBO
	if (OCE_AP_ENABLED(hapd) && hapd->conf->oce_ess_report_enabled)
		tail_len += 4; /* ESS Report element: EID(1)+Len(1)+EID_EXT(1)+ESS_Info(1) */
#endif /* CONFIG_MBO */
	tail_len += hostapd_eid_owe_trans_len(hapd);
#ifdef CONFIG_MBO
	if (OCE_AP_ENABLED(hapd))
		tail_len += 20; /* AP Channel Report IEs (EID 51) */
#endif /* CONFIG_MBO */
	tail_len += hostapd_eid_dpp_cc_len(hapd);
	tail_len += hostapd_get_rsne_override_len(hapd);
	tail_len += hostapd_get_rsne_override_2_len(hapd);
	tail_len += hostapd_get_rsnxe_override_len(hapd);
	tail_len += hostapd_wfa_cap_ie_len(hapd, NULL);
	tail_len += hostapd_tpc_report_len(hapd);
#ifdef CONFIG_QCN_EXTN
	tail_len += hostapd_modify_buflen_for_qcn_ie_extn(hapd);
	/* WDS vendor IE */
	tail_len += hostapd_wds_ie_len_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

	tailpos = tail = os_malloc(tail_len);
	if (head == NULL || tail == NULL) {
		wpa_printf(MSG_ERROR, "Failed to set beacon data");
		os_free(head);
		os_free(tail);
		return -1;
	}
	tailend = tail + tail_len;

	head->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					   WLAN_FC_STYPE_BEACON);
	head->duration = host_to_le16(0);
	os_memset(head->da, 0xff, ETH_ALEN);

	os_memcpy(head->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(head->bssid, hapd->own_addr, ETH_ALEN);
	head->u.beacon.beacon_int =
		host_to_le16(hapd->iconf->beacon_int);

	/* hardware or low-level driver will setup seq_ctrl and timestamp */
	capab_info = hostapd_own_capab_info(hapd);
	head->u.beacon.capab_info = host_to_le16(capab_info);
	pos = &head->u.beacon.variable[0];

	/* SSID */
	*pos++ = WLAN_EID_SSID;
	if (hapd->conf->ignore_broadcast_ssid) {
		if (hapd->conf->ignore_broadcast_ssid == 2) {
			/* clear the data, but keep the correct length of the SSID */
			*pos++ = hapd->conf->ssid.ssid_len;
			os_memset(pos, 0, hapd->conf->ssid.ssid_len);
			pos += hapd->conf->ssid.ssid_len;
		} else {
			*pos++ = 0; /* empty SSID */
		}
	} else {
		*pos++ = hapd->conf->ssid.ssid_len;
		os_memcpy(pos, hapd->conf->ssid.ssid,
			  hapd->conf->ssid.ssid_len);
		pos += hapd->conf->ssid.ssid_len;
	}

	/* Supported rates */
	pos = hostapd_eid_supp_rates(hapd, pos);

	/* DS Params */
	pos = hostapd_eid_ds_params(hapd, pos);

	head_len = pos - (u8 *) head;

	tailpos = hostapd_eid_country(hapd, tailpos, tailend - tailpos);

	/* Power Constraint element */
	tailpos = hostapd_eid_pwr_constraint(hapd, tailpos);
	tailpos = hostapd_eid_tpc_report(hapd, tailpos);

	/* CSA IE */
	csa_pos = hostapd_eid_csa(hapd, tailpos);
	if (csa_pos != tailpos)
		hapd->cs_c_off_beacon = csa_pos - tail - 1;
	tailpos = csa_pos;

	/* ERP Information element */
	tailpos = hostapd_eid_erp_info(hapd, tailpos);

	/* Extended supported rates */
	tailpos = hostapd_eid_ext_supp_rates(hapd, tailpos);

	tailpos = hostapd_get_rsne(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_bss_load(hapd, tailpos, tailend - tailpos);
	tailpos = hostapd_eid_rm_enabled_capab(hapd, tailpos,
					       tailend - tailpos);
	tailpos = hostapd_get_mde(hapd, tailpos, tailend - tailpos);

	/* eCSA IE */
	csa_pos = hostapd_eid_ecsa(hapd, tailpos);
	if (csa_pos != tailpos)
		hapd->cs_c_off_ecsa_beacon = csa_pos - tail - 1;
	tailpos = csa_pos;

	tailpos = hostapd_eid_supported_op_classes(hapd, tailpos);
	tailpos = hostapd_eid_ht_capabilities(hapd, tailpos);
	startpos = tailpos;
	tailpos = hostapd_eid_ht_operation(hapd, tailpos);

#ifdef CONFIG_IEEE80211BE
	if (hapd == tx_bss)
		hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
					   tailpos-startpos, ELEMID_CU_PARAM_HTOP);
#endif

	/* Store the bit offset of the Extended Capabilities Complete
	 * Non-Tx Profile List. Update this offset after building the
	 * MBSSID IE when elem_count > 1 in Enhanced Multi-BSSID enabled
	 * case.
	 */
	extcap_elem = tailpos;
	tailpos = hostapd_eid_ext_capab(hapd, tailpos,
					hapd->iconf->mbssid != MBSSID_DISABLED);

	if ((hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED) &&
	    (tailpos - extcap_elem)) {
		if (extcap_elem + 1 < tailpos) {
			/* Ensure element length covers byte index 10 */
			if ((extcap_elem[1] > 10) && (extcap_elem + 2 + 10) < tailpos)
				complete_nontx_prof_list = extcap_elem + 2 + 10;
		}
	}

	/*
	 * TODO: Time Advertisement element should only be included in some
	 * DTIM Beacon frames.
	 */
	tailpos = hostapd_eid_time_adv(hapd, tailpos);

	tailpos = hostapd_eid_interworking(hapd, tailpos);
	tailpos = hostapd_eid_adv_proto(hapd, tailpos);
	tailpos = hostapd_eid_roaming_consortium(hapd, tailpos);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(tailpos, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		tailpos += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

	tailpos = hostapd_eid_security_profile(hapd, tailpos);
#ifdef CONFIG_IEEE80211AC
	if (hostapd_is_vht_enabled(hapd) &&
	    !is_6ghz_op_class(hapd->iconf->op_class)) {
		tailpos = hostapd_eid_vht_capabilities(hapd, tailpos, 0);
		startpos = tailpos;
		tailpos = hostapd_eid_vht_operation(hapd, tailpos);
#ifdef CONFIG_IEEE80211BE
		if (hapd == tx_bss)
			hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
						   tailpos-startpos, ELEMID_CU_PARAM_VHTOP);
#endif
		tailpos = hostapd_eid_txpower_envelope(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd) &&
	    is_6ghz_op_class(hapd->iconf->op_class))
		tailpos = hostapd_eid_txpower_envelope(hapd, tailpos);
#endif /* CONFIG_IEEE80211AX */

	tailpos = hostapd_eid_chsw_wrapper(hapd, tailpos);

	tailpos = hostapd_eid_rnr(hapd, tailpos, WLAN_FC_STYPE_BEACON, true);
	tailpos = hostapd_eid_fils_indic(hapd, tailpos, 0);

	/* Max Channel Switch Time element */
	tailpos = hostapd_eid_max_cs_time(hapd, tailpos);

	tailpos = hostapd_get_rsnxe(hapd, tailpos, tailend - tailpos);


	/* Store the bit offset of the MBSSID Configurations Full Set
	 * Rx Periodicity  Update this offset after building the
	 * MBSSID IE in Enhanced Multi-BSSID enabled  case.
	 */
	mbssid_cfg_elem = tailpos;
#ifdef RDK_ONEWIFI
       tailpos = hostapd_drv_mbssid_config(hapd, tailpos);
#else
	tailpos = hostapd_eid_mbssid_config(hapd, tailpos, 0);
#endif /* RDK_ONEWIFI */

	if (tailpos - mbssid_cfg_elem) {
		if ((mbssid_cfg_elem + 1 < tailpos) &&
		    (mbssid_cfg_elem + mbssid_cfg_elem[1] + 2 < tailpos)
		    && (mbssid_cfg_elem[1] >= 3))
			mbssid_cfg_periodicity = mbssid_cfg_elem + 2 +
						 mbssid_cfg_elem[1] - 1;
	}

#ifdef CONFIG_IEEE80211AX
	if (hostapd_is_he_enabled(hapd)) {
		u8 *cca_pos;

		tailpos = hostapd_eid_he_capab(hapd, tailpos,
					       IEEE80211_MODE_AP);
		startpos = tailpos;
		tailpos = hostapd_eid_he_operation(hapd, tailpos);

#ifdef CONFIG_IEEE80211BE
		if (hapd == tx_bss)
			hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
						   tailpos-startpos, ELEMID_CU_PARAM_EXT_HEOP);
#endif
		/* BSS Color Change Announcement element */
		cca_pos = hostapd_eid_cca(hapd, tailpos);
		if (cca_pos != tailpos)
			hapd->cca_c_off_beacon = cca_pos - tail - 2;
		tailpos = cca_pos;

		startpos = tailpos;
		tailpos = hostapd_eid_spatial_reuse(hapd, tailpos);
#ifdef CONFIG_IEEE80211BE
		if (hapd == tx_bss)
			hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
						   tailpos-startpos, ELEMID_CU_PARAM_SPATIAL_REUSE);
#endif
		startpos = tailpos;
		tailpos = hostapd_eid_he_mu_edca_parameter_set(hapd, tailpos, false);
#ifdef CONFIG_IEEE80211BE
		if (hapd == tx_bss)
			hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
						   tailpos-startpos, ELEMID_CU_PARAM_MU_EDCA);
#endif
		tailpos = hostapd_eid_he_6ghz_band_cap(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if (hostapd_is_eht_enabled(hapd)) {
		if (hapd->conf->mld_ap) {
			startpos = tailpos;

			if (hapd->conf->enable_aal)
				ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_RMSL_INFO_EN);
			if (hapd->conf->single_link_emlsr &&
			    (hapd->iface->mld_ext_mld_capa &
			     BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK)))
				ext_cap |= BIT(BASIC_MULTI_LINK_CTRL_EXT_EMLSR_ONE_LINK);

			tailpos = hostapd_eid_eht_ml_beacon(hapd, NULL,
							    tailpos, false,
							    ext_cap, true, true,
							    !!(hapd->conf->skip_uhr_extn & SKIP_UHR_EXTN_BCN));
			hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
						   tailpos-startpos, ELEMID_CU_PARAM_EXT_ML);
		}

		tailpos = hostapd_eid_eht_capab(hapd, tailpos,
						IEEE80211_MODE_AP);
		startpos = tailpos;
		tailpos = hostapd_eid_eht_operation(hapd, tailpos);
		tailpos = hostapd_eid_channel_usage(hapd, tailpos, tailend - tailpos);
		if (hapd == tx_bss)
			hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
						   tailpos-startpos, ELEMID_CU_PARAM_EXT_EHTOP);

		if (hapd->conf->mld_ap)
			tailpos = hostapd_add_traffic_ind_elem(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
	if (hostapd_is_uhr_enabled(hapd)) {
		u8 *uhr_cap;

		tailpos = hostapd_eid_uhr_operation(hapd, tailpos, true);

		params->uhr_cap = os_zalloc(3 + IEEE80211_UHR_CAP_MAX_SIZE);
		if (!params->uhr_cap) {
			wpa_printf(MSG_ERROR, "Failed to allocate for UHR capabilities");
			os_free(head);
			os_free(tail);
			return -1;
		}

		uhr_cap = hostapd_eid_uhr_capab(hapd, params->uhr_cap,
					       IEEE80211_MODE_AP);
		/* check that it was filled */
		if (uhr_cap == params->uhr_cap) {
			os_free(params->uhr_cap);
			params->uhr_cap = NULL;
		}
		tailpos = hostapd_eid_tx_power_indication(hapd, tailpos);
	}
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_IEEE80211AC
	if (hapd->conf->vendor_vht)
		tailpos = hostapd_eid_vendor_vht(hapd, tailpos);
#endif /* CONFIG_IEEE80211AC */

	/* WPA */
	tailpos = hostapd_get_wpa_ie(hapd, tailpos, tailend - tailpos);

	/* Wi-Fi Alliance WMM */
	startpos = tailpos;
	tailpos = hostapd_eid_wmm(hapd, tailpos, false);
#ifdef CONFIG_IEEE80211BE
	if (hapd == tx_bss)
		hostapd_eid_update_cu_info(hapd, &elemid_modified, startpos,
					   tailpos-startpos, ELEMID_CU_PARAM_WMM);
#endif

	tailpos = hostapd_add_wfa_cap_ie(hapd, NULL, tailpos);
#ifdef CONFIG_WPS
	if (hapd->conf->wps_state && hapd->wps_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->wps_beacon_ie),
			  wpabuf_len(hapd->wps_beacon_ie));
		tailpos += wpabuf_len(hapd->wps_beacon_ie);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	if ((hapd->conf->p2p & P2P_ENABLED) && hapd->p2p_beacon_ie) {
		os_memcpy(tailpos, wpabuf_head(hapd->p2p_beacon_ie),
			  wpabuf_len(hapd->p2p_beacon_ie));
		tailpos += wpabuf_len(hapd->p2p_beacon_ie);
	}
#endif /* CONFIG_P2P */
#ifdef CONFIG_P2P_MANAGER
	if ((hapd->conf->p2p & (P2P_MANAGE | P2P_ENABLED | P2P_GROUP_OWNER)) ==
	    P2P_MANAGE)
		tailpos = hostapd_eid_p2p_manage(hapd, tailpos);
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_HS20
	tailpos = hostapd_eid_hs20_indication(hapd, tailpos);
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MBO
	tailpos = hostapd_eid_ess_report(hapd, tailpos,
					 tail + tail_len - tailpos);
#endif /* CONFIG_MBO */
#ifdef CONFIG_MBO
	tailpos = hostapd_eid_ap_channel_report(hapd, tailpos,
				  tail + tail_len - tailpos);
#endif /* CONFIG_MBO */
	tailpos = hostapd_eid_mbo(hapd, tailpos, tail + tail_len - tailpos);
	tailpos = hostapd_eid_owe_trans(hapd, tailpos,
					tail + tail_len - tailpos);
	tailpos = hostapd_eid_dpp_cc(hapd, tailpos, tail + tail_len - tailpos);

	tailpos = hostapd_get_rsne_override(hapd, tailpos,
					    tail + tail_len - tailpos);
	tailpos = hostapd_get_rsne_override_2(hapd, tailpos,
					      tail + tail_len - tailpos);
	tailpos = hostapd_get_rsnxe_override(hapd, tailpos,
					     tail + tail_len - tailpos);
#ifdef CONFIG_QCN_EXTN
	tailpos = hostapd_eid_qcn_vendor_ie_extn(hapd, tailpos, IEEE80211_MODE_AP);
	/* WDS vendor IE in beacon */
	tailpos = hostapd_eid_wds_ie_extn(hapd, tailpos,
					  tail + tail_len - tailpos);
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BN
	/* SMD Information element */
	tailpos = hostapd_eid_smd_ie(hapd, tailpos);
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_QCN_EXTN
	tailpos = hostapd_eid_esp_extn(hapd, tailpos,
				       tail + tail_len - tailpos);
#endif /* CONFIG_QCN_EXTN */

	/* Use plugin vendor elements if set, otherwise use conf vendor elements */
	if (hapd->plugin_vendor_elements) {
		os_memcpy(tailpos, wpabuf_head(hapd->plugin_vendor_elements),
			  wpabuf_len(hapd->plugin_vendor_elements));
		tailpos += wpabuf_len(hapd->plugin_vendor_elements);
	} else {
		for (i = 0; i < hapd->conf->vendor_elements_count; i++) {
			struct wpabuf *entry = hapd->conf->vendor_elements[i];

			os_memcpy(tailpos, wpabuf_head(entry), wpabuf_len(entry));
			tailpos += wpabuf_len(entry);
		}
	}

	tail_len = tailpos > tail ? tailpos - tail : 0;

	resp = hostapd_probe_resp_offloads(hapd, &resp_len);
#endif /* NEED_AP_MLME */

	/* If key management offload is enabled, configure PSK to the driver. */
	if (wpa_key_mgmt_wpa_psk_no_sae(hapd->conf->wpa_key_mgmt) &&
	    (hapd->iface->drv_flags2 &
	     WPA_DRIVER_FLAGS2_4WAY_HANDSHAKE_AP_PSK)) {
		if (hapd->conf->ssid.wpa_psk && hapd->conf->ssid.wpa_psk_set) {
			os_memcpy(params->psk, hapd->conf->ssid.wpa_psk->psk,
				  PMK_LEN);
			params->psk_len = PMK_LEN;
		} else if (hapd->conf->ssid.wpa_passphrase &&
			   pbkdf2_sha1(hapd->conf->ssid.wpa_passphrase,
				       hapd->conf->ssid.ssid,
				       hapd->conf->ssid.ssid_len, 4096,
				       params->psk, PMK_LEN) == 0) {
			params->psk_len = PMK_LEN;
		}
	}

#ifdef CONFIG_SAE
	/* If SAE offload is enabled, provide password to lower layer for
	 * SAE authentication and PMK generation.
	 */
	if (wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt |
			     hapd->conf->rsn_override_key_mgmt |
			     hapd->conf->rsn_override_key_mgmt_2) &&
	    (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SAE_OFFLOAD_AP)) {
		if (hostapd_sae_pk_in_use(hapd->conf)) {
			wpa_printf(MSG_ERROR,
				   "SAE PK not supported with SAE offload");
			return -1;
		}

		if (hostapd_sae_pw_id_in_use(hapd->conf)) {
			wpa_printf(MSG_ERROR,
				   "SAE Password Identifiers not supported with SAE offload");
			return -1;
		}

		params->sae_password = sae_get_password(hapd, NULL, NULL, 0,
							NULL, NULL, NULL);
		if (!params->sae_password) {
			wpa_printf(MSG_ERROR, "SAE password not configured for offload");
			return -1;
		}
	}
#endif /* CONFIG_SAE */

	params->head = (u8 *) head;
	params->head_len = head_len;
	params->tail = tail;
	params->tail_len = tail_len;
	params->proberesp = resp;
	params->proberesp_len = resp_len;
	params->dtim_period = hapd->conf->dtim_period;
	params->beacon_int = hapd->iconf->beacon_int;
	params->basic_rates = hapd->basic_rates;
	params->beacon_rate = hapd->conf->beacon_rate;
	params->rate_type = hapd->conf->rate_type;
	params->ssid = hapd->conf->ssid.ssid;
	params->ssid_len = hapd->conf->ssid.ssid_len;
	if ((hapd->conf->wpa & (WPA_PROTO_WPA | WPA_PROTO_RSN)) ==
	    (WPA_PROTO_WPA | WPA_PROTO_RSN))
		params->pairwise_ciphers = hapd->conf->wpa_pairwise |
			hapd->conf->rsn_pairwise;
	else if (hapd->conf->wpa & WPA_PROTO_RSN)
		params->pairwise_ciphers = hapd->conf->rsn_pairwise;
	else if (hapd->conf->wpa & WPA_PROTO_WPA)
		params->pairwise_ciphers = hapd->conf->wpa_pairwise;
	params->group_cipher = hapd->conf->wpa_group;
	params->key_mgmt_suites = hapd->conf->wpa_key_mgmt |
		hapd->conf->rsn_override_key_mgmt |
		hapd->conf->rsn_override_key_mgmt_2;
	params->auth_algs = hapd->conf->auth_algs;
	params->wpa_version = hapd->conf->wpa;
	params->privacy = hapd->conf->wpa;
#ifdef CONFIG_WEP
	params->privacy |= hapd->conf->ssid.wep.keys_set ||
		(hapd->conf->ieee802_1x &&
		 (hapd->conf->default_wep_key_len ||
		  hapd->conf->individual_wep_key_len));
#endif /* CONFIG_WEP */
	switch (hapd->conf->ignore_broadcast_ssid) {
	case 0:
		params->hide_ssid = NO_SSID_HIDING;
		break;
	case 1:
		params->hide_ssid = HIDDEN_SSID_ZERO_LEN;
		break;
	case 2:
		params->hide_ssid = HIDDEN_SSID_ZERO_CONTENTS;
		break;
	}
	params->isolate = hapd->conf->isolate;
#ifdef NEED_AP_MLME
	params->cts_protect = !!(ieee802_11_erp_info(hapd) &
				ERP_INFO_USE_PROTECTION);
	params->preamble = hapd->iface->num_sta_no_short_preamble == 0 &&
		hapd->iconf->preamble == SHORT_PREAMBLE;
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		params->short_slot_time =
			hapd->iface->num_sta_no_short_slot_time > 0 ? 0 : 1;
	else
		params->short_slot_time = -1;
	if (!hostapd_is_ht_enabled(hapd))
		params->ht_opmode = -1;
	else
		params->ht_opmode = hapd->iface->ht_op_mode;
#endif /* NEED_AP_MLME */
	params->interworking = hapd->conf->interworking;
	if (hapd->conf->interworking &&
	    !is_zero_ether_addr(hapd->conf->hessid))
		params->hessid = hapd->conf->hessid;
	params->access_network_type = hapd->conf->access_network_type;
	params->ap_max_inactivity = hapd->conf->ap_max_inactivity;
#ifdef CONFIG_P2P
	params->p2p_go_ctwindow = hapd->iconf->p2p_go_ctwindow;
#endif /* CONFIG_P2P */
#ifdef CONFIG_HS20
	params->disable_dgaf = hapd->conf->disable_dgaf;
#endif /* CONFIG_HS20 */
	params->multicast_to_unicast = hapd->conf->multicast_to_unicast;
	params->pbss = hapd->conf->pbss;
	params->dynamic_vlan =
		hapd->conf->ssid.dynamic_vlan != DYNAMIC_VLAN_DISABLED;

	if (hapd->conf->ftm_responder) {
		if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_FTM_RESPONDER) {
			params->ftm_responder = 1;
			params->lci = hapd->iface->conf->lci;
			params->civic = hapd->iface->conf->civic;
		} else {
			wpa_printf(MSG_WARNING,
				   "Not configuring FTM responder as the driver doesn't advertise support for it");
		}
	}

	params->beacon_tx_mode = hapd->conf->beacon_tx_mode;

	if (hapd->iconf->mbssid &&
	    (hapd->iconf->mbssid != MULTI_MBSSID_GROUP_ENABLED ||
	     hapd->mbssid_group)) {
		if (ieee802_11_build_ap_params_mbssid(hapd, params)) {
			ieee802_11_free_ap_params(params);
			wpa_printf(MSG_ERROR,
				   "MBSSID: Failed to set beacon data");
			return -1;
		}
		complete = hapd->iconf->mbssid == MBSSID_ENABLED ||
			   hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED ||
			   (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
			    params->mbssid.mbssid_elem_count == 1);
	}

	/*
	 * Update the these specific bits in extended capablility and
	 * MBSSID configuration elements after constructing entire MBSSID
	 * element.
	 */
	if ((hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED) &&
	    complete_nontx_prof_list && !complete)
		/* Bit 80 - Complete List of NonTxBSSID Profiles */
		*complete_nontx_prof_list &= ~0x01;

	if (mbssid_cfg_periodicity)
		*mbssid_cfg_periodicity = params->mbssid.mbssid_elem_count;

#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && elemid_modified)
		params->elemid_modified_bmap |= BIT(hostapd_mbssid_get_bss_index(tx_bss));
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap) {
		params->mld_ap = true;
		params->mld_link_id = hapd->mld_link_id;
	}
#endif /* CONFIG_IEEE80211BE */

	params->rssi_reject_assoc_rssi = hapd->conf->rssi_reject_assoc_rssi ?
		hapd->conf->rssi_reject_assoc_rssi : hapd->iconf->rssi_reject_assoc_rssi;
	params->rssi_deauth_grace_samples = hapd->conf->rssi_deauth_grace_samples ?
		hapd->conf->rssi_deauth_grace_samples : hapd->iconf->rssi_deauth_grace_samples;

	return 0;
}


void ieee802_11_free_ap_params(struct wpa_driver_ap_params *params)
{
	os_free(params->tail);
	params->tail = NULL;
	os_free(params->head);
	params->head = NULL;
	os_free(params->proberesp);
	params->proberesp = NULL;
	os_free(params->mbssid.mbssid_elem);
	params->mbssid.mbssid_elem = NULL;
	os_free(params->mbssid.mbssid_elem_offset);
	params->mbssid.mbssid_elem_offset = NULL;
	os_free(params->mbssid.rnr_elem);
	params->mbssid.rnr_elem = NULL;
	os_free(params->mbssid.rnr_elem_offset);
	params->mbssid.rnr_elem_offset = NULL;
#ifdef CONFIG_FILS
	os_free(params->fd_frame_tmpl);
	params->fd_frame_tmpl = NULL;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211AX
	os_free(params->ubpr.unsol_bcast_probe_resp_tmpl);
	params->ubpr.unsol_bcast_probe_resp_tmpl = NULL;
#endif /* CONFIG_IEEE80211AX */
	os_free(params->allowed_freqs);
	params->allowed_freqs = NULL;
#ifdef CONFIG_IEEE80211BN
	os_free(params->uhr_cap);
	params->uhr_cap = NULL;
#endif /* CONFIG_IEEE80211BN */
}

#ifdef CONFIG_IEEE80211BE
static bool ieee802_11_is_link_beacon_set(struct hostapd_data *hapd)
{
	if (hapd->reenable_beacon)
		return false;

	return hostapd_drv_read_link_set_beacon(hapd, hapd->mld_link_id);
}

static inline int
ieee802_11_configure_ttlm_on_non_tx(struct hostapd_data *hapd)
{
	if (hapd->iface->conf->mbssid == MULTI_MBSSID_GROUP_ENABLED)
		return hostapd_offload_set_adv_ttlm_multi_mbssid(hapd);

	return hostapd_offload_set_adv_ttlm_mbssid_enhanced(hapd);
}
#endif

static void hostapd_apply_rtt_responder_role(struct hostapd_data *hapd)
{
	int ret;

	if (hapd->rtt_role_fw_sent)
		return;

	ret = hostapd_drv_set_rtt_responder_role(hapd,
						 hapd->conf->rtt_responder_role);
	if (ret)
		wpa_printf(MSG_DEBUG,
			   "Failed to set RTT responder role: %d", ret);
}

static int __ieee802_11_set_beacon(struct hostapd_data *hapd)
{
	struct wpa_driver_ap_params params;
	struct hostapd_freq_params freq;
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_config *iconf = iface->conf;
	struct hostapd_hw_modes *cmode = iface->current_mode;
	struct wpabuf *beacon, *proberesp, *assocresp;
	bool twt_he_responder = false;
	size_t bcn_len;
	int res, ret = -1, j;
#ifdef CONFIG_DRIVER_NL80211_QCA
	int i;
	struct hostapd_hw_modes *mode;
#endif /* CONFIG_DRIVER_NL80211_QCA */
#ifdef CONFIG_IEEE80211BE
	bool beacon_set = false;
	u8 num_repurposed_links = 0;
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->drv_priv) {
		wpa_printf(MSG_ERROR, "Interface is disabled");
		return -1;
	}

	/* If CAC is followed by DFS CSA, update the partner-link RNR entry with
	 * the new 5 GHz channel. Only allow beacon templates for partner links.
	 */
	if (iface->cac_type == HAPD_CAC_COMPLETE_AFTER_CSA &&
	    iface->state == HAPD_IFACE_DFS) {
		wpa_printf(MSG_DEBUG, "Skipping beacon update while CSA CAC is in progress");
		return 0;
	}

	if (hapd->csa_in_progress) {
		wpa_printf(MSG_ERROR, "Cannot set beacons during CSA period");
		return -1;
	}

#ifdef CONFIG_IEEE80211AX
	if (hapd->cca_in_progress) {
		wpa_printf(MSG_ERROR,
			   "Cannot set beacons during CCA period");
		return -1;
	}
#endif /* CONFIG_IEEE80211AX */

	hapd->beacon_set_done = 1;

	ieee802_11_update_beacon_mbssid(hapd);

	hapd->iface->rnr_psd = hostapd_get_20mhz_psd_for_rnr(hapd);

#ifdef CONFIG_QCN_EXTN
	/* RNR memeber ess colocated indication in bss param */
	hapd->iconf->rnr_colocated_ess = hostapd_rnr_colocated_ess_indication_extn(hapd);
#endif /* CONFIG_QCN_EXTN */

	if (ieee802_11_build_ap_params(hapd, &params) < 0)
		return -1;

	if (hostapd_build_ap_extra_ies(hapd, &beacon, &proberesp, &assocresp) <
	    0)
		goto fail1;

	bcn_len = params.head_len + params.tail_len + wpabuf_len(beacon) +
		  params.mbssid.mbssid_elem_len;

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED) {
		if (bcn_len > hapd->iface->multi_mbssid.max_beacon_size) {
			if (params.mbssid.mbssid_elem_count > 1) {
				wpa_printf(MSG_ERROR,
					   "Reduce MBSSID group size (%d) to accommodate within beacon size limit of %u bytes. Current beacon length is %zu",
					   hapd->iconf->group_size,
					   iface->multi_mbssid.max_beacon_size,
					   bcn_len);
			}
			goto fail2;
		}
		if (hapd->iface->multi_mbssid.num_mbssid_groups >
		    hapd->iface->multi_mbssid.mbssid_max_ngroups) {
			wpa_printf(MSG_ERROR,
				   "Created Multi MBSSID groups(%zu) exceeded max allowed groups(%d)\n",
				   hapd->iface->multi_mbssid.num_mbssid_groups,
				   hapd->iface->multi_mbssid.mbssid_max_ngroups);
			goto fail2;
		}
	} else if (hapd->iface->max_mgmt_frm_sz && (bcn_len > hapd->iface->max_mgmt_frm_sz)) {
		wpa_printf(MSG_ERROR, "Beacon size limit (%zu) exceeded for %s: max allowed size(%zu)",
			   bcn_len, hapd->conf->iface, hapd->iface->max_mgmt_frm_sz);
		goto fail2;
	}

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		beacon_set = ieee802_11_is_link_beacon_set(hapd);
#endif

#if defined(CONFIG_QCN_EXTN) && defined(CONFIG_IEEE80211BE)
	if (hapd->conf->mld_ap)
		num_repurposed_links =
			hostapd_get_repurposed_links_bitmap_extn(hapd, NULL);
#endif /* CONFIG_QCN_EXTN && CONFIG_IEEE80211BE */

	params.beacon_ies = beacon;
	params.proberesp_ies = proberesp;
	params.assocresp_ies = assocresp;
	params.reenable = hapd->reenable_beacon;
#ifdef CONFIG_IEEE80211AX
	params.he_spr_ctrl = hapd->iface->conf->spr.sr_control;
	params.he_spr_non_srg_obss_pd_max_offset =
		hapd->iface->conf->spr.non_srg_obss_pd_max_offset;
	params.he_spr_srg_obss_pd_min_offset =
		hapd->iface->conf->spr.srg_obss_pd_min_offset;
	params.he_spr_srg_obss_pd_max_offset =
		hapd->iface->conf->spr.srg_obss_pd_max_offset;
	os_memcpy(params.he_spr_bss_color_bitmap,
		  hapd->iface->conf->spr.srg_bss_color_bitmap, 8);
	os_memcpy(params.he_spr_partial_bssid_bitmap,
		  hapd->iface->conf->spr.srg_partial_bssid_bitmap, 8);
	params.he_bss_color_disabled =
		hapd->iface->conf->he_op.he_bss_color_disabled;
	params.he_bss_color_partial =
		hapd->iface->conf->he_op.he_bss_color_partial;
	params.he_bss_color = hapd->iface->conf->he_op.he_bss_color;
	params.he_bss_color_collision_detection =
		hapd->iface->conf->he_op.he_bss_color_collision_detection;
	twt_he_responder = hostapd_get_he_twt_responder(hapd,
							IEEE80211_MODE_AP);
	params.ubpr.unsol_bcast_probe_resp_tmpl =
		hostapd_unsol_bcast_probe_resp(hapd, &params.ubpr);
#endif /* CONFIG_IEEE80211AX */
	params.twt_responder =
		twt_he_responder || hostapd_get_ht_vht_twt_responder(hapd);
	hapd->reenable_beacon = 0;
#ifdef CONFIG_SAE
	params.sae_pwe = hapd->conf->sae_pwe;
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	params.fd_frame_tmpl = hostapd_fils_discovery(hapd, &params);
#endif /* CONFIG_FILS */

#ifdef CONFIG_IEEE80211BE
	params.punct_bitmap = iconf->punct_bitmap;
#endif /* CONFIG_IEEE80211BE */
	params.disable_cu = hapd->disable_cu;
	hapd->disable_cu = 0;

	if (hapd->conf->is_cmn_param) {
		/* Currently hostapd framework supports to send only one parameter */
		params.num_cmn_params = 1;

		for (j = 0; j < params.num_cmn_params; j++) {
			params.multi_bss_params[j].cmn_param_id = hapd->conf->cmn_param_id;
			params.multi_bss_params[j].cmn_param_val[0] = hapd->conf->cmn_param_val[0];
			if (hapd->conf->cmn_param_val[1] >= 0) {
				params.multi_bss_params[j].cmn_param_val[1] =
								hapd->conf->cmn_param_val[1];
			}
		}
	}

#ifdef CONFIG_IEEE80211BN
	/* Populate SMD parameters from hapd->conf into params structure */
	if (hapd->conf->smd.enabled) {
		params.smd.enabled = hapd->conf->smd.enabled;
		os_memcpy(params.smd.smd_identifier,
			  hapd->conf->smd.smd_identifier, ETH_ALEN);
		params.smd.smd_timeout = hapd->conf->smd.smd_prep_timeout;
		params.smd.caps.dl_data_fwd = hapd->conf->smd.caps.dl_data_fwd;
		params.smd.caps.max_prep_target_apmlds =
			hapd->conf->smd.caps.max_prep_target_apmlds;
		params.smd.caps.smd_type = hapd->conf->smd.caps.smd_type;
		params.smd.caps.ptk_mode = hapd->conf->smd.caps.ptk_mode;
	}
#endif /* CONFIG_IEEE80211BN */

	if (cmode &&
	    hostapd_set_freq_params(&freq, iconf->hw_mode, iface->freq,
				    iconf->channel, iconf->enable_edmg,
				    iconf->edmg_channel, iconf->ieee80211n,
				    iconf->ieee80211ac, iconf->ieee80211ax,
				    iconf->ieee80211be,
				    iconf->ieee80211bn,
				    iconf->secondary_channel,
				    hostapd_get_oper_chwidth(iconf),
				    hostapd_get_oper_centr_freq_seg0_idx(iconf),
				    hostapd_get_oper_centr_freq_seg1_idx(iconf),
				    cmode->vht_capab,
				    &cmode->he_capab[IEEE80211_MODE_AP],
				    &cmode->eht_capab[IEEE80211_MODE_AP],
				    &cmode->uhr_capab[IEEE80211_MODE_AP],
				    hostapd_get_punct_bitmap(hapd),
				    iconf->he_6ghz_reg_pwr_type,
#ifdef CONFIG_IEEE80211BN
				    hostapd_hw_get_freq(hapd,
					iconf->npca_primary_channel),
				    iconf->npca_punct_bitmap,
#else
				    0, 0,
#endif /* CONFIG_IEEE80211BN */
				    iconf->bandwidth_device,
				    iconf->center_freq_device) == 0) {
		freq.link_id = -1;
#ifdef CONFIG_IEEE80211BE
		if (hapd->conf->mld_ap)
			freq.link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
		params.freq = &freq;
#ifdef CONFIG_QCN_EXTN
		hostapd_beacon_set_skip_cac_extn(iface, params.freq);
		hostapd_ignorecac_update_freq_params_extn(iface,
							   params.freq);
		if (iface->mcst && iface->cs_time) {
			/* Pass adjusted cs_time to START_AP. For residual CAC,
			 * keep skip_cac cleared so kernel runs remaining CAC.
			 */
			params.freq->mcst = IEEE80211_MS_TO_TU(iface->cs_time);
			if (!hostapd_mcst_allows_skip_cac_extn(
				    iface->mcst, iface->conf->beacon_int,
				    params.freq->skip_cac))
				params.freq->skip_cac = 0;
		}
#endif
	}

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if ((hapd->conf->mld_ap) && (hapd->conf->enable_aal)) {
		if ((hapd->mld->num_links - num_repurposed_links) >=
		    hapd->conf->ml_max_rec_links)
			params.ml_max_rec_links = hapd->conf->ml_max_rec_links;
		else
			params.ml_max_rec_links = (hapd->mld->num_links -
						   num_repurposed_links);

		if (params.ml_max_rec_links == ML_IE_RSVD_MAX_REC_LINKS)
			params.ml_max_rec_links = ML_IE_NO_MAX_REC_LINKS;
	} else {
		params.ml_max_rec_links = ML_IE_MAX_REC_LINKS_INVAL;
	}
#ifdef CONFIG_QCN_EXTN
	} else {
		params.ml_max_rec_links = ML_IE_MAX_REC_LINKS_INVAL;
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	params.allowed_freqs = NULL;
#ifdef CONFIG_DRIVER_NL80211_QCA
	for (i = 0; i < hapd->iface->num_hw_features; i++) {
		mode = &hapd->iface->hw_features[i];

		if (iconf->hw_mode != HOSTAPD_MODE_IEEE80211ANY &&
		    iconf->hw_mode != mode->mode)
			continue;

		hostapd_get_hw_mode_any_channels(hapd, mode,
						 !(iconf->acs_freq_list.num ||
						   iconf->acs_ch_list.num),
						 true, &params.allowed_freqs);
	}
#endif /* CONFIG_DRIVER_NL80211_QCA */

#ifdef CONFIG_IEEE80211BE
	/* If hapd has advertised ttlm established or under advertisement,
	 * include ttlm attributes in params of start_ap despite of 2g/5g/6g
	 * tx/non-tx bss.
	 *
	 * If the bss is 6GHz non-tx bss and its tx-bss has some non-default
	 * TTLM being advertised send default TTLM mapping to driver as part of
	 * ap_settings of non-tx bss.
	 *
	 * If hapd is 6GHz tx bss,
	 *    - send set_ttlm nl command to driver across already started non-tx
	 * bss's belonging to tx bss's mbssid group if any. If non-tx bss does
	 * not have advertised ttlm, send default mapping to driver.
	 *    - skip sending set_ttlm nl command on non-started non-tx bss's as
	 * vdev not created yet. When start ap of the non-started non-tx bss
	 * trigggered, send mapping through start ap params
	 */
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	if (hapd->conf->mld_ap && hapd->conf->ttlm_enable && !beacon_set &&
	    hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_TTLM_BEACON_OFFLOAD) {
		struct hostapd_data *tx_bss = hostapd_mbssid_get_tx_bss(hapd);
		struct mlo_ttlm_ie *est_ttlm =
			&hapd->mld->ttlm_ctx.established_ttlm;
		struct mlo_ttlm_ie *up_ttlm =
			&hapd->mld->ttlm_ctx.upcoming_ttlm;
		struct wpa_driver_ap_ttlm_params *ttlm_params =
			&params.ttlm_params;

		/* bss has its own establised ttlm, include same in
		 * params.ttlm_params
		 */
		if (est_ttlm->ttlm.expected_duration_present ||
		    up_ttlm->ttlm.mapping_switch_time_present) {
			if (hostapd_fill_ttlm_params(&up_ttlm->ttlm,
						     &est_ttlm->ttlm,
						     &ttlm_params->up_ttlm,
						     &ttlm_params->est_ttlm)) {
				wpa_printf(MSG_DEBUG,
					   "TTLM: Failed to fill setap params");
				os_memset(ttlm_params, 0, sizeof(*ttlm_params));
				goto set_ap;
			}

			ttlm_params->send_default_mapping = false;

			/* if this bss is 6g tx-bss, send ttlm config on already
			 * started non-tx bss if any
			 */
			if (is_6ghz_freq(hapd->iface->freq) &&
			    tx_bss == hapd &&
			    ieee802_11_configure_ttlm_on_non_tx(hapd)) {
				wpa_printf(MSG_DEBUG,
					   "TTLM: fail to set param on non-tx");
			}
		} else if (is_6ghz_freq(hapd->iface->freq) &&
			   hapd != tx_bss &&
			   tx_bss->mld) {
			/* 6g non-tx bss without ttlm advertisement, needs
			 * default mapping to be sent to driver when its tx-bss
			 * has non-default ttlm being advertised
			 */
			struct mlo_ttlm_ie *tx_est_ttlm =
				&tx_bss->mld->ttlm_ctx.established_ttlm;
			struct mlo_ttlm_ie *tx_up_ttlm =
				&tx_bss->mld->ttlm_ctx.upcoming_ttlm;

			if (!tx_est_ttlm->ttlm.expected_duration_present &&
			    !tx_up_ttlm->ttlm.mapping_switch_time_present)
				goto set_ap;

			/* default value of established ttlm will be
			 * default mapping
			 */
			if (hostapd_fill_ttlm_params(&up_ttlm->ttlm,
						     &est_ttlm->ttlm,
						     &ttlm_params->up_ttlm,
						     &ttlm_params->est_ttlm)) {
				wpa_printf(MSG_DEBUG,
					   "TTLM: Fail to fill params");
				os_memset(ttlm_params, 0,
					  sizeof(*ttlm_params));
				goto set_ap;
			}
			ttlm_params->send_default_mapping = true;
		}
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif
#ifdef CONFIG_IEEE80211BN
	/* Send dps_assist to driver if HW supports the feature */
	if (cmode && cmode->uhr_capab[IEEE80211_MODE_AP].uhr_supported &&
	    (cmode->uhr_capab[IEEE80211_MODE_AP].mac_cap[0] & UHR_MACCAP_DPS_ASSIST))
		params.dps_assist = hapd->conf->dps_assist;
#endif
	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_CIGTK &&
	    hapd->conf->control_frame_prot)
		params.is_cfp_enabled = true;

set_ap:
	res = hostapd_drv_set_ap(hapd, &params);
	if (res)
		wpa_printf(MSG_ERROR,
			   "%s: Failed to set beacon parameters (ret=%d)",
			   hapd->conf->iface, res);
	else {
		hostapd_apply_rtt_responder_role(hapd);
		ret = 0;
	}
fail2:
	hostapd_free_ap_extra_ies(hapd, beacon, proberesp, assocresp);
fail1:
	ieee802_11_free_ap_params(&params);
	return ret;
}


void ieee802_11_set_beacon_per_bss_only(struct hostapd_data *hapd)
{
	if (!hapd->started && !hapd->beacon_set_done) {
		wpa_printf(MSG_ERROR, "BSS not started hapd:%s", hapd->conf->iface);
		return;
	}

	__ieee802_11_set_beacon(hapd);
}


#ifdef CONFIG_IEEE80211BE

static int hostapd_get_probe_resp_tmpl(struct hostapd_data *hapd,
				       struct probe_resp_params *params,
				       bool is_ml_sta_info)
{
	os_memset(params, 0, sizeof(*params));
	hostapd_gen_probe_resp(hapd, params);
	if (!params->resp)
		return -1;

	/* The caller takes care of freeing params->resp. */
	return 0;
}


static bool is_restricted_eid_in_sta_profile(u8 eid, bool tx_vap)
{
	switch (eid) {
	case WLAN_EID_TIM:
	case WLAN_EID_BSS_MAX_IDLE_PERIOD:
	case WLAN_EID_MULTIPLE_BSSID:
	case WLAN_EID_REDUCED_NEIGHBOR_REPORT:
	case WLAN_EID_NEIGHBOR_REPORT:
		return true;
	case WLAN_EID_SSID:
		/* SSID is not restricted for non-transmitted BSSID */
		return tx_vap;
	default:
		return false;
	}
}


static bool is_restricted_ext_eid_in_sta_profile(u8 ext_id)
{
	switch (ext_id) {
	case WLAN_EID_EXT_MULTI_LINK:
		return true;
	default:
		return false;
	}
}


/* Create the link STA profiles based on inheritance from the reporting
 * profile.
 *
 * NOTE: The same function is used for length calculation as well as filling
 * data in the given buffer. This avoids risk of not updating the length
 * function but filling function or vice versa.
 */
static size_t hostapd_add_sta_profile(struct ieee80211_mgmt *link_fdata,
				      size_t link_data_len,
				      struct ieee80211_mgmt *own_fdata,
				      size_t own_data_len,
				      u8 *sta_profile, bool tx_vap)
{
	const struct element *link_elem;
	size_t sta_profile_len = 0;
	const u8 *link_elem_data;
	u8 link_ele_len;
	u8 *link_data;
	const struct element *own_elem;
	u8 link_eid, own_eid, own_ele_len;
	const u8 *own_elem_data;
	u8 *own_data;
	bool is_ext;
	bool ie_found;
	u8 non_inherit_ele_ext_list[256] = { 0 };
	u8 non_inherit_ele_ext_list_len = 0;
	u8 non_inherit_ele_list[256] = { 0 };
	u8 non_inherit_ele_list_len = 0;
	u8 num_link_elem_vendor_ies = 0, num_own_elem_vendor_ies = 0;
	bool add_vendor_ies = false, is_identical_vendor_ies = true;
	/* The bitmap of parsed EIDs. There are 256 EIDs and ext EIDs, so 32
	 * bytes to store the bitmaps. */
	u8 parsed_eid_bmap[32] = { 0 }, parsed_ext_eid_bmap[32] = { 0 };
	/* extra len used in the logic includes the element id and len */
	u8 extra_len = 2;

	/* Include len for capab info */
	sta_profile_len += sizeof(le16);
	if (sta_profile) {
		os_memcpy(sta_profile, &link_fdata->u.probe_resp.capab_info,
			  sizeof(le16));
		sta_profile += sizeof(le16);
	}

	own_data = own_fdata->u.probe_resp.variable;
	link_data = link_fdata->u.probe_resp.variable;

	/* The below logic takes the reporting BSS data and reported BSS data
	 * and performs intersection to build the STA profile of the reported
	 * BSS. Certain elements are not added to the STA profile as
	 * recommended in standard. Matching element information in the
	 * reporting BSS profile are ignored in the STA profile. Remaining
	 * elements pertaining to the STA profile are appended at the end. */
	for_each_element(own_elem, own_data, own_data_len) {
		is_ext = false;
		ie_found = false;

		/* Pick one of own elements and get its EID and length */
		own_elem_data = own_elem->data;
		own_ele_len = own_elem->datalen;

		if (own_elem->id == WLAN_EID_EXTENSION) {
			is_ext = true;
			own_eid = *(own_elem_data);
			if (is_restricted_ext_eid_in_sta_profile(own_eid))
				continue;
		} else {
			own_eid = own_elem->id;
			if (is_restricted_eid_in_sta_profile(own_eid, tx_vap))
				continue;
		}

		for_each_element(link_elem, link_data, link_data_len) {
			/* If the element type mismatches, do not consider
			 * this link element for comparison. */
			if ((link_elem->id == WLAN_EID_EXTENSION &&
			     !is_ext) ||
			    (is_ext && link_elem->id != WLAN_EID_EXTENSION))
				continue;

			/* Comparison can be done so get the link element and
			 * its EID and length. */
			link_elem_data = link_elem->data;
			link_ele_len = link_elem->datalen;

			if (link_elem->id == WLAN_EID_EXTENSION)
				link_eid = *(link_elem_data);
			else
				link_eid = link_elem->id;

			/* Ignore if EID does not match */
			if (own_eid != link_eid)
				continue;

			ie_found = true;

			/* Ignore if the contents is identical. */
			if (own_ele_len == link_ele_len &&
			    os_memcmp(own_elem->data, link_elem->data,
				      own_ele_len) == 0) {
				if (own_eid == WLAN_EID_VENDOR_SPECIFIC) {
					is_identical_vendor_ies = true;
					num_own_elem_vendor_ies++;
				}

				/* Update the parsed EIDs bitmap */
				if (is_ext)
					parsed_ext_eid_bmap[own_eid / 8] |=
						BIT(own_eid % 8);
				else
					parsed_eid_bmap[own_eid / 8] |=
						BIT(own_eid % 8);
				break;
			}

			/* No need to include this non-matching Vendor Specific
			 * element explicitly at this point. */
			if (own_eid == WLAN_EID_VENDOR_SPECIFIC) {
				is_identical_vendor_ies = false;
				continue;
			}

			/* This element is present in the reported profile
			 * as well as present in the reporting profile.
			 * However, there is a mismatch in the contents and
			 * hence, include this in the per STA profile. */
			sta_profile_len += link_ele_len + extra_len;
			if (sta_profile) {
				os_memcpy(sta_profile,
					  link_elem->data - extra_len,
					  link_ele_len + extra_len);
				sta_profile += link_ele_len + extra_len;
			}

			/* Update the parsed EIDs bitmap */
			if (is_ext)
				parsed_ext_eid_bmap[own_eid / 8] |=
					BIT(own_eid % 8);
			else
				parsed_eid_bmap[own_eid / 8] |=
					BIT(own_eid % 8);
			break;
		}

		/* We found at least one Vendor Specific element in reporting
		 * link which is not same (or present) in the reported link. We
		 * need to include all Vendor Specific elements from the
		 * reported link. */
		if (!is_identical_vendor_ies)
			add_vendor_ies = true;

		/* This is a unique element in the reporting profile which is
		 * not present in the reported profile. Update the
		 * non-inheritance list. */
		if (!ie_found) {
			u8 idx;

			if (is_ext) {
				idx = non_inherit_ele_ext_list_len++;
				non_inherit_ele_ext_list[idx] = own_eid;
			} else {
				idx = non_inherit_ele_list_len++;
				non_inherit_ele_list[idx] = own_eid;
			}
		}
	}

	/* Parse the remaining elements in the reported profile */
	for_each_element(link_elem, link_data, link_data_len) {
		link_elem_data = link_elem->data;
		link_ele_len = link_elem->datalen;

		/* No need to check this Vendor Specific element at this point.
		 * Just take the count and continue. */
		if (link_elem->id == WLAN_EID_VENDOR_SPECIFIC) {
			num_link_elem_vendor_ies++;
			continue;
		}

		if (link_elem->id == WLAN_EID_EXTENSION) {
			link_eid = *(link_elem_data);

			if ((parsed_ext_eid_bmap[link_eid / 8] &
			     BIT(link_eid % 8)) ||
			    is_restricted_ext_eid_in_sta_profile(link_eid))
				continue;
		} else {
			link_eid = link_elem->id;

			if ((parsed_eid_bmap[link_eid / 8] &
			     BIT(link_eid % 8)) ||
			    is_restricted_eid_in_sta_profile(link_eid, tx_vap))
				continue;
		}

		sta_profile_len += link_ele_len + extra_len;
		if (sta_profile) {
			os_memcpy(sta_profile, link_elem_data - extra_len,
				  link_ele_len + extra_len);
			sta_profile += link_ele_len + extra_len;
		}
	}

	/* Handle Vendor Specific elements
	 * Add all the Vendor Specific elements of the reported link if
	 *  a. There is at least one non-matching Vendor Specific element, or
	 *  b. The number of Vendor Specific elements in reporting and reported
	 *     link is not same. */
	if (add_vendor_ies ||
	    num_own_elem_vendor_ies != num_link_elem_vendor_ies) {
		for_each_element(link_elem, link_data, link_data_len) {
			link_elem_data = link_elem->data;
			link_ele_len = link_elem->datalen;

			if (link_elem->id != WLAN_EID_VENDOR_SPECIFIC)
				continue;

			sta_profile_len += link_ele_len + extra_len;
			if (sta_profile) {
				os_memcpy(sta_profile,
					  link_elem_data - extra_len,
					  link_ele_len + extra_len);
				sta_profile += link_ele_len + extra_len;
			}
		}
	}

	/* Handle non-inheritance
	 * Non-Inheritance element:
	 *      Element ID Ext: 1 octet
	 *	Length: 1 octet
	 *	Ext tag number: 1 octet
	 *	Length of Elements ID list: 1 octet
	 *	Elements ID list: variable
	 *      Length of Elements ID Extension list: 1 octet
	 *	Elements ID extensions list: variable
	 */
	if (non_inherit_ele_list_len || non_inherit_ele_ext_list_len)
		sta_profile_len += 3 + 2 + non_inherit_ele_list_len +
			non_inherit_ele_ext_list_len;

	if (sta_profile &&
	    (non_inherit_ele_list_len || non_inherit_ele_ext_list_len)) {
		*sta_profile++ = WLAN_EID_EXTENSION;
		*sta_profile++ = non_inherit_ele_list_len +
			non_inherit_ele_ext_list_len + 3;
		*sta_profile++ = WLAN_EID_EXT_NON_INHERITANCE;
		*sta_profile++ = non_inherit_ele_list_len;
		os_memcpy(sta_profile, non_inherit_ele_list,
			  non_inherit_ele_list_len);
		sta_profile += non_inherit_ele_list_len;
		*sta_profile++ = non_inherit_ele_ext_list_len;
		os_memcpy(sta_profile, non_inherit_ele_ext_list,
			  non_inherit_ele_ext_list_len);
		sta_profile += non_inherit_ele_ext_list_len;
	}

	return sta_profile_len;
}


static u8 * hostapd_gen_sta_profile(struct ieee80211_mgmt *link_data,
				    size_t link_data_len,
				    struct ieee80211_mgmt *own_data,
				    size_t own_data_len,
				    size_t *sta_profile_len, bool tx_vap)
{
	u8 *sta_profile;

	/* Get the length first */
	*sta_profile_len = hostapd_add_sta_profile(link_data, link_data_len,
						   own_data, own_data_len,
						   NULL, tx_vap);
	if (!(*sta_profile_len) || *sta_profile_len > EHT_ML_MAX_STA_PROF_LEN)
		return NULL;

	sta_profile = os_zalloc(*sta_profile_len);
	if (!sta_profile)
		return NULL;

	/* Now fill in the data */
	hostapd_add_sta_profile(link_data, link_data_len, own_data,
				own_data_len, sta_profile, tx_vap);

	/* The caller takes care of freeing the returned sta_profile */
	return sta_profile;
}


void hostapd_gen_per_sta_profiles(struct hostapd_data *hapd)
{
	bool tx_vap;
	size_t link_data_len, sta_profile_len;
	size_t own_data_len, fixed;
	struct probe_resp_params link_params;
	struct probe_resp_params own_params;
	struct ieee80211_mgmt *link_data;
	struct ieee80211_mgmt *own_data;
	struct mld_link_info *link_info;
	struct hostapd_data *link_bss;
	u8 link_id, *sta_profile;

	if (!hapd->conf->mld_ap || !hapd->started)
		return;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;
#endif /* CONFIG_QCN_EXTN */

	wpa_printf(MSG_DEBUG, "MLD: Generating per STA profiles for MLD %s",
		   hapd->conf->iface);

	wpa_printf(MSG_DEBUG, "MLD: Reporting link %d", hapd->mld_link_id);

	/* Generate a Probe Response template for self */
	if (hostapd_get_probe_resp_tmpl(hapd, &own_params, false)) {
		wpa_printf(MSG_ERROR,
			   "MLD: Error in building per STA profiles");
		return;
	}

	own_data = own_params.resp;
	own_data_len = own_params.resp_len;

	/* Consider the length of the variable fields */
	fixed = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	if (own_data_len < fixed)
		goto fail;
	own_data_len -= fixed;

	/* tx_vap becomes true only if this hapd is the TX BSS */
	tx_vap = (hapd == hostapd_mbssid_get_tx_bss(hapd));

	for_each_mld_link(link_bss, hapd) {
		if (link_bss == hapd || !link_bss->started)
			continue;

		link_id = link_bss->mld_link_id;
		if (link_id >= MAX_NUM_MLD_LINKS)
			continue;

		sta_profile = NULL;
		sta_profile_len = 0;

		/* Generate a Probe Response frame template for partner link */
		if (hostapd_get_probe_resp_tmpl(link_bss, &link_params, true)) {
			wpa_printf(MSG_ERROR,
				   "MLD: Could not get link STA probe response template for link %d",
				   link_id);
			continue;
		}

		link_data = link_params.resp;
		link_data_len = link_params.resp_len;

		/* Consider length of the variable fields */
		fixed = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
		if (link_data_len < fixed)
			continue;
		link_data_len -= fixed;

		sta_profile = hostapd_gen_sta_profile(link_data, link_data_len,
						      own_data, own_data_len,
						      &sta_profile_len, tx_vap);
		if (!sta_profile) {
			wpa_printf(MSG_ERROR,
				   "MLD: Could not generate link STA profile for link %d",
				   link_id);
			continue;
		}

		link_info = &hapd->partner_links[link_id];
		link_info->valid = true;

		os_free(link_info->resp_sta_profile);
		link_info->resp_sta_profile_len = sta_profile_len;

		link_info->resp_sta_profile = os_memdup(sta_profile,
							sta_profile_len);
		if (!link_info->resp_sta_profile)
			link_info->resp_sta_profile_len = 0;

		os_memcpy(link_info->local_addr, link_bss->own_addr, ETH_ALEN);

		wpa_printf(MSG_DEBUG,
			   "MLD: Reported link STA info for %d: %u bytes",
			   link_id, link_info->resp_sta_profile_len);

		os_free(sta_profile);
		os_free(link_params.resp);
	}

fail:
	os_free(own_params.resp);
}

#endif /* CONFIG_IEEE80211BE */


void ieee802_11_update_beacon_mbssid(struct hostapd_data *hapd)
{
	struct hostapd_data *tx_hapd;
	int ret;

	if (!hapd || !hapd->iconf || hapd->iconf->mbssid == MBSSID_DISABLED)
		return;

	tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
	if (!tx_hapd || tx_hapd == hapd || !tx_hapd->started ||
	    !tx_hapd->beacon_set_done || tx_hapd->csa_in_progress ||
	    tx_hapd->cca_in_progress)
		return;

	ret = __ieee802_11_set_beacon(tx_hapd);
	if (ret)
		wpa_printf(MSG_WARNING,
			   "failed to update beacon for transmitted profile %s",
			   tx_hapd->conf->iface);
}


int ieee802_11_set_beacon(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	int ret;
	size_t i, j;
	bool is_6g, hapd_mld = false;
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *link_bss;
#endif /* CONFIG_IEEE80211BE */

	if (hapd->started || hapd->is_update_beacon) {
		ret = __ieee802_11_set_beacon(hapd);
		if (ret != 0)
			return ret;
	}

	if (!iface->interfaces || iface->interfaces->count <= 1)
		return 0;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
#endif /* CONFIG_QCN_EXTN */
	hapd_mld = hapd->conf->mld_ap;
#endif /* CONFIG_IEEE80211BE */

	/* Update Beacon frames in case of 6 GHz colocation or AP MLD */
	is_6g = is_6ghz_op_class(iface->conf->op_class);
	for (j = 0; j < iface->interfaces->count; j++) {
		struct hostapd_iface *other;
		bool other_iface_6g;

		other = iface->interfaces->iface[j];
		if (other == iface || !other || !other->conf)
			continue;

		other_iface_6g = is_6ghz_op_class(other->conf->op_class);

		if (is_6g == other_iface_6g && !hapd_mld)
			continue;

		for (i = 0; i < other->num_bss; i++) {
#ifdef CONFIG_IEEE80211BE
			if (is_6g == other_iface_6g &&
			    !(hapd_mld && other->bss[i]->conf->mld_ap))
				continue;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BE
			if (!is_6g &&
			    (!hostapd_is_ml_partner(hapd, other->bss[i])
#ifdef CONFIG_QCN_EXTN
			     || hostapd_is_repurpose_disabled_11be_extn(other->bss[i]->conf)
#endif /* CONFIG_QCN_EXTN */
			    ))
				continue;

#endif /* CONFIG_IEEE80211BE */
			if (other->bss[i] && other->bss[i]->started &&
			    other->bss[i]->beacon_set_done)
				__ieee802_11_set_beacon(other->bss[i]);
		}
	}

#ifdef CONFIG_IEEE80211BE
	if (!hapd_mld)
		return 0;

	/* Generate per STA profiles for each affiliated APs */
	for_each_mld_link(link_bss, hapd)
		hostapd_gen_per_sta_profiles(link_bss);
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


int ieee802_11_set_beacons(struct hostapd_iface *iface)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->started &&
		    ieee802_11_set_beacon(iface->bss[i]) < 0)
			ret = -1;
	}

	return ret;
}


/* only update beacons if started */
int ieee802_11_update_beacons(struct hostapd_iface *iface)
{
	size_t i;
	int ret = 0;

	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->beacon_set_done && iface->bss[i]->started &&
		    ieee802_11_set_beacon(iface->bss[i]) < 0)
			ret = -1;
	}

	return ret;
}

#endif /* CONFIG_NATIVE_WINDOWS */
