// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/wpa_ctrl.h"
#include "ap/hostapd.h"
#include "cmn.h"
#include "ap/beacon.h"
#ifdef CONFIG_IEEE80211BE
#include "common/hw_features_common.h"
#endif /* CONFIG_IEEE80211BE */

int
hostapd_config_check_bss_repurpose_mode_extn(const struct hostapd_config *conf,
					     const struct hostapd_bss_config *bss)
{
#ifdef CONFIG_IEEE80211BE
	if (!bss->bss_extn.repurpose_mode)
		return 0;

	/* Disallow repurpose_mode on non-MLD BSS */
	if (!bss->mld_ap) {
		wpa_printf(MSG_ERROR,
			   "repurpose_mode only allowed on link BSS of AP MLD");
		return -1;
	}

	/* Disallow when 11be/11ax are disabled by config */
	if (!conf->ieee80211be || bss->disable_11be ||
	    !conf->ieee80211ax || bss->disable_11ax) {
		wpa_printf(MSG_ERROR,
			   "repurpose_mode not allowed when 11be/11ax disabled");
		return -1;
	}

	/* 11AX can't be disabled on 6 GHz through repurpose */
	if (is_6ghz_op_class(conf->op_class) &&
	    hostapd_is_repurpose_disabled_11ax_extn(bss)) {
		wpa_printf(MSG_ERROR,
			   "repurpose: Can't disable 11AX on 6GHz BSS");
		return -1;
	}

	/* 11AC requires ieee80211ac to be enabled */
	if (bss->bss_extn.repurpose_mode == REPURPOSE_11AC &&
	    (!conf->ieee80211ac || bss->disable_11ac)) {
		wpa_printf(MSG_ERROR,
			   "repurpose: REPURPOSE_11AC requires ieee80211ac enabled");
		return -1;
	}
#endif /* CONFIG_IEEE80211BE */
	return 0;
}

/**
 * hostapd_is_repurpose_disabled_11ax_extn - Checks if repurpose has disabled
 * 11AX mode on the given BSS or not.
 * @bss: BSS config of the BSS.
 *
 * This helper function checks if repurpose feature had disabled 11ax capability
 * from the BSS or not. If no repurpose_mode configured then it returns false.
 * This does not really check if BSS has ieee80211ax=1 and disable_11ax=0.
 * So, use it along with appropriate mld_ap or ieee80211ax or disable_11ax check
 * from caller when it returns false.
 *
 * Return: True if BSS repurposed to mode lesser than 11ax, false otherwise.
 */
bool
hostapd_is_repurpose_disabled_11ax_extn(const struct hostapd_bss_config *bss)
{
	if (bss &&
	    hostapd_is_valid_repurpose_mode_extn(bss->bss_extn.repurpose_mode) &&
	    bss->bss_extn.repurpose_mode < REPURPOSE_11AX)
		return true;

	return false;
}

/**
 * hostapd_is_repurpose_disabled_11be_extn - Checks if repurpose has disabled
 * 11BE mode on the given BSS or not.
 * @bss: BSS config of the BSS.
 *
 * This helper function checks if repurpose feature had disabled 11be capability
 * from the BSS or not. If no repurpose_mode configured then it returns false.
 * This does not really check if BSS has ieee80211be=1 and disable_11be=0.
 * So, use it along with appropriate mld_ap or ieee80211be or disable_11be check
 * from caller when it return false.
 *
 * Return: True if the BSS repurposed to mode lesser than 11be, false otherwise.
 */
bool
hostapd_is_repurpose_disabled_11be_extn(const struct hostapd_bss_config *bss)
{
	if (bss &&
	    hostapd_is_valid_repurpose_mode_extn(bss->bss_extn.repurpose_mode) &&
	    bss->bss_extn.repurpose_mode < REPURPOSE_11BE)
		return true;

	return false;
}

#ifdef CONFIG_IEEE80211BE
int wpa_driver_nl80211_vendor_cmd_notify_link_repurpose(void *priv, u8 link_id);
#endif /* CONFIG_IEEE80211BE */

int
hostapd_drv_notify_link_repurpose_extn(struct hostapd_data *hapd, u8 link_id)
{
#ifdef CONFIG_IEEE80211BE
	if (!hapd || !hapd->driver || !hapd->drv_priv)
		return 0;

	return wpa_driver_nl80211_vendor_cmd_notify_link_repurpose(hapd->drv_priv,
								   link_id);
#else
	return 0;
#endif /* CONFIG_IEEE80211BE */
}

u8 hostapd_get_repurposed_links_bitmap_extn(struct hostapd_data *hapd,
					    u16 *repurposed_links)
{
	struct hostapd_data *lhapd;
	u8 num_repurposed_links = 0;

	if (repurposed_links)
		*repurposed_links = 0;

	for_each_mld_link_include_repurposed(lhapd, hapd) {
		if (hostapd_is_repurpose_disabled_11be_extn(lhapd->conf)) {
			if (repurposed_links)
				*repurposed_links |= BIT(lhapd->mld_link_id);
			num_repurposed_links++;
		}
	}

	return num_repurposed_links;
}

int
hostapd_link_remove_repurposed_bss_extn(struct hostapd_data *hapd,
					u32 removal_type)
{
	struct hostapd_data *tx_hapd = NULL;
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_iface **tmp;
	struct hapd_interfaces *interfaces = iface->interfaces;
	unsigned int i;
	bool is_6g = false;

	if (removal_type == HAPD_LINK_DISABLE) {
		hostapd_disable_bss(hapd, 0, AP_EVENT_DISABLED);
		return 0;
	}

	/* check this to verify if the removed BSS is 6 GHz,
	 * otherwise updating only the partner beacon,
	 * irrespective of the band is suffice.
	 */
	is_6g = is_6ghz_op_class(iface->conf->op_class);

	if (iface->num_bss == 1) {
		for (i = 0; i < interfaces->count; i++) {
			if (interfaces->iface[i] == iface) {
				hostapd_interface_deinit_free(iface);
				iface = NULL;
				os_remove_in_array(interfaces->iface,
						   interfaces->count,
						   sizeof(struct hostapd_iface *),
						   i);
				interfaces->count--;
				tmp = os_realloc_array(interfaces->iface,
						       interfaces->count,
						       sizeof(struct hostapd_iface *));
				if (!tmp)
					return -1;
				interfaces->iface = tmp;
				break;
			}
		}
	} else {
		for (i = 0; i < iface->conf->num_bss; i++) {
			if (iface->bss[i] == hapd)
				break;
		}

		/* Shouldn't happen */
		if (i >= iface->conf->num_bss) {
			wpa_printf(MSG_ERROR, "Wrong hapd is provided\n");
			return -1;
		}

		/* Store tx_hapd to update MBSSID beacon as hapd will be
		 * freed by hostapd_remove_bss() */
		tx_hapd = hostapd_mbssid_get_tx_bss(hapd);
		if (tx_hapd == hapd)
			tx_hapd = NULL;

		hostapd_remove_bss(iface, i);

		if (tx_hapd)
			ieee802_11_update_beacon_mbssid(tx_hapd);
	}

	/* For 6G, OOB advertisement also needs to be refreshed in all
	 * enabled lower bands, hence invoke other iface beacon refresh
	 * to update all the entries properly
	 */
	if (is_6g) {
		if (!iface && interfaces->count > 0)
			hostapd_refresh_all_iface_beacons(interfaces->iface[0]);
		else
			hostapd_refresh_other_iface_beacons(iface);
	}

	return 0;
}

static const char *
hostapd_repurpose_mode_str_extn(
	enum repurpose_mode mode)
{
	switch (mode) {
	case REPURPOSE_11AC:
		return "11ac";
	case REPURPOSE_11AX:
		return "11ax";
	case REPURPOSE_11BE:
		return "11be";
	default:
		return "invalid";
	}
}

int
hostapd_validate_mbssid_group_repurpose_mode_extn(struct hostapd_data *hapd)
{
	struct hostapd_data *bss;

	if (!hapd->conf || !hapd->iconf || !hapd->iface)
		return -1;

	if (hapd->iconf->mbssid == MBSSID_DISABLED)
		return 0;

	if (hapd->iconf->mbssid == MULTI_MBSSID_GROUP_ENABLED &&
	    hapd->mbssid_group) {
		dl_list_for_each(bss, &hapd->mbssid_group->bss_list,
				 struct hostapd_data, mbssid_bss) {
			if (bss && bss->conf && bss != hapd)
				goto compare;
		}
	} else {
		size_t i;

		for (i = 0; i < hapd->iface->num_bss; i++) {
			bss = hapd->iface->bss[i];
			if (bss && bss->conf && bss != hapd)
				goto compare;
		}
	}

	return 0;

compare:
	if (hapd->conf->bss_extn.repurpose_mode !=
	    bss->conf->bss_extn.repurpose_mode) {
		wpa_printf(MSG_ERROR,
			   "Repurpose mode mismatch between %s (%s) and %s (%s)",
			   hapd->conf->iface,
			   hostapd_repurpose_mode_str_extn(hapd->conf->bss_extn.repurpose_mode),
			   bss->conf->iface,
			   hostapd_repurpose_mode_str_extn(bss->conf->bss_extn.repurpose_mode));
		return -1;
	}

	return 0;
}

struct hostapd_data *
hostapd_get_non_repurposed_link_of_mld_extn(struct hostapd_data *hapd)
{
	struct hostapd_data *link_hapd;

	/* Use for_each_mld_link as it loops only non-repurposed links */
	for_each_mld_link(link_hapd, hapd) {
		return link_hapd;
	}

	return NULL;
}


bool hostapd_config_check_repurpose_width_extn(struct hostapd_config *conf)
{
	u16 oper_width;
	enum oper_chan_width oper_chwidth;

	if (!conf->ieee80211be)
		return true;

	if (is_6ghz_op_class(conf->op_class))
		oper_chwidth = op_class_to_ch_width(conf->op_class);
	else
		oper_chwidth = conf->eht_oper_chwidth;

	oper_width =
		hostapd_get_width_from_oper_chwidth_extn(oper_chwidth,
							 conf->secondary_channel);

	if (conf->conf_extn.user_repurpose_he_width) {
		conf->conf_extn.repurpose_he_width =
			MIN(conf->conf_extn.user_repurpose_he_width, oper_width);
	} else {
		if (oper_width == 320)
			conf->conf_extn.repurpose_he_width = 160;
		else
			conf->conf_extn.repurpose_he_width = oper_width;
		wpa_printf(MSG_DEBUG,
			   "Repurpose: internally derived repurpose HE width = %d",
			   conf->conf_extn.repurpose_he_width);
	}

	if (conf->conf_extn.user_repurpose_vht_width) {
		conf->conf_extn.repurpose_vht_width =
			MIN(conf->conf_extn.user_repurpose_vht_width,
			    conf->conf_extn.repurpose_he_width);
	} else {
		conf->conf_extn.repurpose_vht_width = conf->conf_extn.repurpose_he_width;
		wpa_printf(MSG_DEBUG,
			   "Repurpose: internally derived repurpose vht width = %d",
			   conf->conf_extn.repurpose_vht_width);
	}

	return true;
}


void hostapd_set_repurpose_oper_chwidth_extn(struct hostapd_config *conf,
					     enum oper_chan_width oper_chwidth)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_config_extn *conf_extn = &conf->conf_extn;
	u16 oper_width;

	if (!conf->ieee80211be)
		return;

	/* 320 mhz not supported on lower mode, downgrade chwidth to 160MHz */
	if (oper_chwidth == CONF_OPER_CHWIDTH_320MHZ)
		oper_chwidth = CONF_OPER_CHWIDTH_160MHZ;

	oper_width = hostapd_get_width_from_oper_chwidth_extn(
				oper_chwidth,
				conf->secondary_channel);

	if (!conf_extn->user_repurpose_he_width) {
		wpa_printf(MSG_DEBUG,
			   "Repurpose: User not configured repurpose_he_width, set it now to %d",
			   oper_width);
		conf_extn->repurpose_he_width = oper_width;
	} else {
		conf_extn->repurpose_he_width =
			MIN(conf_extn->user_repurpose_he_width, oper_width);
		wpa_printf(MSG_DEBUG,
			   "Repurpose: repurpose_he_width set to %d (user %d, oper %d)",
			   conf_extn->repurpose_he_width,
			   conf_extn->user_repurpose_he_width, oper_width);
	}

	if (!conf_extn->user_repurpose_vht_width) {
		wpa_printf(MSG_DEBUG,
			   "Repurpose: User not configured repurpose_vht_width, set it now to %d",
			   oper_width);
		conf_extn->repurpose_vht_width = oper_width;
	} else {
		conf_extn->repurpose_vht_width =
			MIN(conf_extn->user_repurpose_vht_width,
			    conf_extn->repurpose_he_width);
		wpa_printf(MSG_DEBUG,
			   "Repurpose: repurpose_vht_width set to %d (user %d, he %d)",
			   conf_extn->repurpose_vht_width,
			   conf_extn->user_repurpose_vht_width,
			   conf_extn->repurpose_he_width);
	}

	wpa_printf(MSG_DEBUG,
		   "Repurpose: repurpose he width = %d vht width = %d",
		   conf_extn->repurpose_he_width,
		   conf_extn->repurpose_vht_width);
#endif /* CONFIG_IEEE80211BE */
}

static void
repurpose_reduce_contig_bw_extn(u8 pri, enum oper_chan_width *width,
				u8 *seg0, u8 *seg1)
{
	switch (*width) {
	case CONF_OPER_CHWIDTH_320MHZ:
		*width = CONF_OPER_CHWIDTH_160MHZ;
		if (pri < *seg0)
			*seg0 -= 16;
		else
			*seg0 += 16;
		*seg1 = 0;
		break;
	case CONF_OPER_CHWIDTH_160MHZ:
		*width = CONF_OPER_CHWIDTH_80MHZ;
		if (pri < *seg0)
			*seg0 -= 8;
		else
			*seg0 += 8;
		*seg1 = 0;
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		*width = CONF_OPER_CHWIDTH_USE_HT;
		if (pri < *seg0)
			*seg0 -= 4;
		else
			*seg0 += 4;
		*seg1 = 0;
		break;
	default:
		break;
	}
}

/**
 * hostapd_oper_info_of_repurposed_bss_helper_extn - Apply repurpose width cap
 * @hapd: BSS context whose repurpose configuration is used
 * @primary_channel: Primary operating channel to derive the legacy info for
 * @secondary_channel: Secondary channel offset of the operating channel
 * @oper_chwidth: Maximum advertisable legacy operating channel width
 * @seg0: Center frequency segment 0 index for the operating channel
 * @seg1: Center frequency segment 1 index for the operating channel
 *
 * Apply the repurpose bandwidth cap to the caller supplied legacy operating
 * channel information. The caller is expected to pass already-adjusted values
 * when puncturing or 320 MHz to 160 MHz downgrade handling is needed.
 */
static void
hostapd_oper_info_of_repurposed_bss_helper_extn(struct hostapd_data *hapd,
						u8 primary_channel,
						int secondary_channel,
						enum oper_chan_width *oper_chwidth,
						u8 *seg0,
						u8 *seg1)
{
	u16 oper_width;
	u16 repurpose_width;

	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;

	if (*seg0 == 0)
		*seg0 = primary_channel;

	/* If chan width is 20/40, then check if seg0 passed is same as pri
	 * channel. If so, the operating bandwidth is 20. Skip deriving based
	 * on secondary channel.
	 */
	if (*oper_chwidth == CONF_OPER_CHWIDTH_USE_HT &&
	    *seg0 == primary_channel)
		oper_width = 20;
	else
		oper_width = hostapd_get_width_from_oper_chwidth_extn(
				      *oper_chwidth,
				      secondary_channel);

	if (hostapd_is_repurpose_disabled_11ax_extn(hapd->conf))
		repurpose_width =
			hapd->iconf->conf_extn.repurpose_vht_width;
	else
		repurpose_width =
			hapd->iconf->conf_extn.repurpose_he_width;

	if (repurpose_width >= oper_width ||
	    !repurpose_width)
		return;

	while (oper_width > repurpose_width) {
		if (*oper_chwidth == CONF_OPER_CHWIDTH_USE_HT) {
			/* 20 MHz */
			*seg1 = 0;
			*seg0 = primary_channel;
			break;
		}
		repurpose_reduce_contig_bw_extn(primary_channel,
						oper_chwidth,
						seg0, seg1);
		oper_width = hostapd_get_width_from_oper_chwidth_extn(
					*oper_chwidth,
					secondary_channel);
	}
}



/* hostapd_get_oper_info_of_repurposed_bss_extn derives the channel operation
 * information that can be advertised in management frames of repurposed BSS
 * by considering the repurpose_he_width or repurpose_vht_width configured on
 * the interface based on the repurpose mode configured on the BSS.
 *
 * Callers must pass the current maximum-advertisable legacy operating channel
 * information for the BSS. In particular, if the radio is punctured or the
 * BSS is EHT-disabled on a 320 MHz interface, width/seg0/seg1 must already be
 * adjusted for those constraints before this function is called.
 *
 * This function only applies the additional repurpose bandwidth cap. It does
 * not recompute puncture-derived operating information. Hence, callers must
 * also ensure to call this only when BSS is repurposed.
 */
void hostapd_get_oper_info_of_repurposed_bss_extn(struct hostapd_data *hapd,
						  enum oper_chan_width *oper_chwidth,
						  u8 *seg0,
						  u8 *seg1)
{
	hostapd_oper_info_of_repurposed_bss_helper_extn(hapd,
							hapd->iconf->channel,
							hapd->iconf->secondary_channel,
							oper_chwidth,
							seg0,
							seg1);
}


/**
 * hostapd_get_csa_info_of_repurposed_bss_extn - Derive repurposed CSA info
 * @hapd: BSS context whose repurpose configuration is used
 * @primary_channel: Primary channel of the CSA target channel definition
 * @secondary_channel: Secondary channel offset of the CSA target channel
 * @oper_chwidth: Maximum advertisable legacy operating channel width for CSA
 * @seg0: Center frequency segment 0 index for the CSA target channel
 * @seg1: Center frequency segment 1 index for the CSA target channel
 *
 * Apply the repurpose bandwidth cap to caller supplied CSA target channel
 * information so that ECSA/CSA operating class derivation uses the target BSS
 * bandwidth instead of the interface bandwidth.
 */
void hostapd_get_csa_info_of_repurposed_bss_extn(struct hostapd_data *hapd,
						 u8 primary_channel,
						 int secondary_channel,
						 enum oper_chan_width *oper_chwidth,
						 u8 *seg0,
						 u8 *seg1)
{
	/* Repurposed CSA to be derived only if user explicitly configured
	 * a repurpose bandwidth in interface.
	 */
	if (hostapd_is_repurpose_disabled_11ax_extn(hapd->conf) &&
	    !hapd->iconf->conf_extn.user_repurpose_vht_width)
		return;

	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf) &&
	    !hapd->iconf->conf_extn.user_repurpose_he_width)
		return;

	hostapd_oper_info_of_repurposed_bss_helper_extn(hapd,
							primary_channel,
							secondary_channel,
							oper_chwidth,
							seg0,
							seg1);
}

void
hostapd_repurpose_update_ht_capabilities_extn(struct hostapd_data *hapd,
					      struct ieee80211_ht_capabilities *cap)
{
	/* Clear HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET if repurposed BSS has
	 * disabled 40MHz
	 */
	if (cap->ht_capabilities_info & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET) {
		if (hostapd_is_repurpose_disabled_11ax_extn(hapd->conf)) {
			if (hapd->iconf->conf_extn.repurpose_vht_width == 20)
				cap->ht_capabilities_info &=
					~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		} else if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
			if (hapd->iconf->conf_extn.repurpose_he_width == 20)
				cap->ht_capabilities_info &=
					~HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET;
		}
	}
}


u8 hostapd_get_repurpose_width_extn(struct hostapd_data *hapd)
{
	if (!hapd->iconf)
		return 0;

	/* return repurpose width as per the repurpose mode. If BSS is not
	 * repurposed, return operating channel width of the radio.
	 */
	if (hostapd_is_repurpose_disabled_11ax_extn(hapd->conf))
		return hapd->iconf->conf_extn.repurpose_vht_width;

	if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return hapd->iconf->conf_extn.repurpose_he_width;

	return hostapd_get_width_from_oper_chwidth_extn
			(hostapd_get_oper_chwidth(hapd->iconf),
			 hapd->iconf->secondary_channel);
}


void
hostapd_repurpose_get_vht_legacy_chan_info_extn(struct hostapd_data *hapd,
						enum oper_chan_width *chwidth,
						u8 *seg0,
						u8 *seg1)
{
#ifdef CONFIG_IEEE80211BE
	u16 punct_bitmap = hostapd_get_punct_bitmap(hapd);
#endif /* CONFIG_IEEE80211BE */

	*chwidth = hapd->iconf->vht_oper_chwidth;
	*seg0 = hapd->iconf->vht_oper_centr_freq_seg0_idx;
	*seg1 = hapd->iconf->vht_oper_centr_freq_seg1_idx;

#ifdef CONFIG_IEEE80211BE
	if (punct_bitmap) {
		hostapd_get_oper_center_freq_seg_extn(hapd->iconf,
						      seg0,
						      seg1,
						      chwidth);
		punct_update_legacy_bw(punct_bitmap,
				       hapd->iconf->channel,
				       chwidth,
				       seg0,
				       seg1);
	}
#endif /* CONFIG_IEEE80211BE */
}


bool
hostapd_repurpose_update_ht_operation_mode_extn(struct hostapd_data *hapd,
						le32 vht_capabilities_info,
						struct ieee80211_ht_operation *oper)
{
	enum oper_chan_width chwidth;
	u8 seg0, seg1;

	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return false;

	if (!(vht_capabilities_info & VHT_CAP_EXTENDED_NSS_BW_SUPPORT))
		return false;

	hostapd_repurpose_get_vht_legacy_chan_info_extn(hapd, &chwidth,
							&seg0, &seg1);
	hostapd_get_oper_info_of_repurposed_bss_extn(hapd, &chwidth,
						     &seg0, &seg1);
	if (chwidth == CHANWIDTH_160MHZ)
		oper->operation_mode = host_to_le16(seg0);

	return true;
}

void
hostapd_repurpose_update_vht_capabilities_extn(struct hostapd_data *hapd,
					       u8 *chwidth,
					       struct ieee80211_vht_capabilities *cap)
{
	enum oper_chan_width repurpose_chwidth;
	u8 seg0, seg1;

	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf))
		return;

	hostapd_repurpose_get_vht_legacy_chan_info_extn(hapd, &repurpose_chwidth,
							&seg0, &seg1);
	hostapd_get_oper_info_of_repurposed_bss_extn(hapd, &repurpose_chwidth,
						     &seg0, &seg1);
	*chwidth = repurpose_chwidth;
	if (*chwidth != CHANWIDTH_160MHZ &&
	    *chwidth != CHANWIDTH_80P80MHZ) {
		cap->vht_capabilities_info &=
			~(host_to_le32(VHT_CAP_SUPP_CHAN_WIDTH_MASK));
		cap->vht_capabilities_info &=
			~(host_to_le32(VHT_CAP_SHORT_GI_160));
	}
}
