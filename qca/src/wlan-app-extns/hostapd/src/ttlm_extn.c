// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "common/ieee802_11_common.h"
#include "ap/ieee802_11.h"
#include "ap/ttlm.h"
#include "ap/sta_info.h"

#ifdef CONFIG_IEEE80211BE

static int
hostapd_ttlm_sta_p2p_tid_mapped_to_5g_only(struct hostapd_data *hapd,
					   struct sta_info *sta, void *ctx)
{
	struct ttlm_prev_negotiated_info *neg;
	int dir, tid;
	u8 link_id = *(u8 *)ctx;

	if (!ap_sta_is_mld(hapd, sta))
		return 0;

	if (!(sta->flags & WLAN_STA_AUTHORIZED))
		return 0;

	neg = &sta->mld_info.tid_map_info.ttlm_prev_negotiated_info;
	for (dir = 0; dir < TTLM_DIRECTION_MAX; dir++) {
		struct ttlm_info *info = &neg->ttlm_info[dir];

		if (info->direction == TTLM_DIRECTION_INVALID)
			continue;

		if (info->default_link_mapping)
			continue;

		for (tid = 0; tid < NUM_MAX_TIDS; tid++) {
			if (info->ieee_link_map_tid[tid] ==
			    BIT(link_id)) {
				wpa_printf(MSG_DEBUG,
					   "TTLM: STA " MACSTR " has P2P TID %d mapped to 5 GHz link_id=%u (dir=%d)",
					   MAC2STR(sta->addr), tid, link_id,
					   dir);
				return 1;
			}
		}
	}

	return 0;
}

static bool
hostapd_ttlm_mld_has_p2p_tid_mapped_to_5g_only(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss;

	for_each_mld_link(link_bss, hapd) {
		if (ap_for_each_sta(link_bss,
				    hostapd_ttlm_sta_p2p_tid_mapped_to_5g_only,
				    &hapd->mld_link_id)) {
			return true;
		}
	}

	return 0;
}

static void
hostapd_ttlm_override_p2p_ttlm_with_default_mapping(struct hostapd_data *hapd)
{
	struct hostapd_data *link_bss;
	struct mlo_ttlm_ie ttlm_conf;
	u16 ieee_link_id_mask = 0;
	u32 max_dtim = 0;
	int beacon_int;
	struct hostapd_data *hapd_6g = NULL;
	int i;

	beacon_int = hapd->iconf->beacon_int;
	if (beacon_int < 1)
		beacon_int = 100;

	if (!hostapd_ttlm_mld_has_p2p_tid_mapped_to_5g_only(hapd)) {
		wpa_printf(MSG_DEBUG,
			   "TTLM: No P2P TIDs mapped to 5 GHz only; skip advertised TTLM override");
		return;
	}

	for_each_mld_link(link_bss, hapd) {
		if (is_6ghz_freq(link_bss->iface->freq))
			hapd_6g = link_bss;

		if (link_bss->conf->dtim_period > max_dtim)
			max_dtim = link_bss->conf->dtim_period;

		ieee_link_id_mask |= BIT(link_bss->mld_link_id);
	}

	if (!max_dtim) {
		wpa_printf(MSG_ERROR,
			   "TTLM: max_dtim is 0; cannot override P2P 5 GHz-only TID mapping");
		return;
	}

	if (hapd_6g)
		hapd = hapd_6g;

	wpa_printf(MSG_INFO,
		   "TTLM: Overriding P2P 5 GHz-only TID mapping with default advertised TTLM: link_mask=0x%x max_dtim=%u",
		   ieee_link_id_mask, max_dtim);

	os_memset(&ttlm_conf, 0, sizeof(ttlm_conf));
	ttlm_conf.ttlm.direction = TTLM_DIRECTION_BIDI;
	ttlm_conf.ttlm.default_link_mapping = 1;
	ttlm_conf.ttlm.link_mapping_size = 0;
	ttlm_conf.ttlm.mapping_switch_time_present = true;
	ttlm_conf.ttlm.mapping_switch_time = max_dtim * beacon_int;
	ttlm_conf.ttlm.expected_duration_present = true;
	ttlm_conf.ttlm.expected_duration = beacon_int;
	for (i = 0; i < NUM_MAX_TIDS; i++)
		ttlm_conf.ttlm.ieee_link_map_tid[i] = ieee_link_id_mask;

	hostapd_send_advertised_ttlm(hapd, &ttlm_conf);
}

static void
hostapd_ttlm_shorten_adv_duration_for_5g_cac(struct hostapd_data *hapd)
{
	struct ttlm_context *ttlm_ctx;
	struct drv_adv_ttlm_params upcoming_params;
	struct drv_adv_ttlm_params established_params;
	int beacon_int;
	u32 orig_dur;

	ttlm_ctx = &hapd->mld->ttlm_ctx;

	beacon_int = hapd->iconf->beacon_int;
	if (beacon_int < 1)
		beacon_int = 100;

	orig_dur = ttlm_ctx->established_ttlm.ttlm.expected_duration;
	ttlm_ctx->established_ttlm.ttlm.expected_duration = beacon_int;
	ttlm_ctx->established_t2lm_ed_modified_in_case_of_cac = true;

	if (hostapd_fill_ttlm_params(&ttlm_ctx->upcoming_ttlm.ttlm,
				     &ttlm_ctx->established_ttlm.ttlm,
				     &upcoming_params,
				     &established_params)) {
		wpa_printf(MSG_ERROR,
			   "TTLM: Failed to fill TTLM params for 5 GHz CAC duration shortening on link_id=%u",
			   hapd->mld_link_id);
		ttlm_ctx->established_ttlm.ttlm.expected_duration = orig_dur;
		ttlm_ctx->established_t2lm_ed_modified_in_case_of_cac = false;
		return;
	}

	hostapd_offload_set_advertised_ttlm(hapd, &ttlm_ctx->upcoming_ttlm,
					    &upcoming_params,
					    &established_params);
}

/**
 * hostapd_ttlm_handle_5g_only_tid_map_for_cac() - Evaluate and apply default
 * TID-to-link mapping.
 * @hapd: Pointer to the hostapd BSS instance on the 5 GHz link.
 *
 * 1. Advertised TTLM with 5 GHz-only mapping: If an established Advertised TTLM
 *    has all TIDs mapped exclusively to this 5 GHz link and carries an
 *    expected_duration, the duration is shortened to one beacon interval so
 *    that the mapping expires before the CAC completes, via
 *    hostapd_ttlm_shorten_adv_duration_for_5g_cac().
 *
 * 2. P2P negotiated TTLM with 5 GHz-only TIDs: If any associated STA has a
 *    Peer-to-Peer negotiated TTLM that maps one or more TIDs exclusively to
 *    the 5 GHz link, a new Advertised TTLM is issued that restores the default
 *    mapping across all available MLD links, overriding the P2P mapping, via
 *    hostapd_ttlm_override_p2p_ttlm_with_default_mapping().
 *
 * Return: None
 */
static void hostapd_ttlm_handle_5g_only_tid_map_for_cac(struct hostapd_data *hapd)
{
	struct ttlm_context *ttlm_ctx;
	u8 link_id;

	ttlm_ctx = &hapd->mld->ttlm_ctx;
	if (ttlm_ctx->upcoming_ttlm.ttlm.mapping_switch_time_present) {
		wpa_printf(MSG_DEBUG,
			   "TTLM: Upcoming TTLM mapping switch already pending on link_id=%u",
			   hapd->mld_link_id);
		return;
	}

	link_id = hapd->mld_link_id;

	if (ttlm_ctx->established_ttlm.ttlm.expected_duration_present &&
	    ttlm_ctx->established_ttlm.ttlm.ieee_link_map_tid[0] ==
	    BIT(link_id)) {
		wpa_printf(MSG_DEBUG,
			   "TTLM: Established advertised TTLM maps TIDs to 5 GHz link_id=%u only; shortening duration",
			   link_id);
		hostapd_ttlm_shorten_adv_duration_for_5g_cac(hapd);
		return;
	}

	hostapd_ttlm_override_p2p_ttlm_with_default_mapping(hapd);
}

static int hostapd_csa_target_requires_cac(struct hostapd_data *hapd,
					   struct csa_settings *settings)
{
	int bandwidth;

	switch (settings->freq_params.bandwidth) {
	case 40:
		bandwidth = CHAN_WIDTH_40;
		break;
	case 80:
		bandwidth = settings->freq_params.center_freq2 ?
				CHAN_WIDTH_80P80 : CHAN_WIDTH_80;
		break;
	case 160:
		bandwidth = CHAN_WIDTH_160;
		break;
	case 320:
		bandwidth = CHAN_WIDTH_320;
		break;
	default:
		bandwidth = CHAN_WIDTH_20;
		break;
	}

	return hostapd_find_dfs_range_extn(hapd->iface, bandwidth,
					   &settings->freq_params);
}

void
hostapd_ttlm_restore_default_mapping_for_5g_cac(struct hostapd_data *hapd,
						struct csa_settings *settings)
{
	bool ttlm_beacon_offload;
	bool csa_requires_cac;

	if (!is_5ghz_freq(hapd->iface->freq))
		return;

	if (!hapd->conf->ttlm_enable || !hostapd_is_multiple_link_mld(hapd))
		return;

	ttlm_beacon_offload =
		hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_TTLM_BEACON_OFFLOAD;
	if (!ttlm_beacon_offload) {
		wpa_printf(MSG_DEBUG,
			   "TTLM: TTLM beacon offload not supported");
		return;
	}

	csa_requires_cac = hostapd_csa_target_requires_cac(hapd, settings);
	if (!csa_requires_cac)
		return;

	hostapd_ttlm_handle_5g_only_tid_map_for_cac(hapd);
}
#endif /* CONFIG_IEEE80211BE */
