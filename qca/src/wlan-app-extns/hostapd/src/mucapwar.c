// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/list.h"
#include "ap/hostapd.h"
#include "ap/sta_info.h"
#include "ap/ap_drv_ops.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "cmn.h"

#ifdef CONFIG_IEEE80211AC

void hostapd_mu_cap_war_state_init_extn(struct hostapd_data *hapd)
{
	struct hostapd_data_extn *h_ext;

	h_ext = &hapd->hapd_extn;

	h_ext->mu_cap_war = false;
	h_ext->mu_cap_war_override = false;
	dl_list_init(&h_ext->mu_cap_war_sta_list);
}


static bool hostapd_mu_cap_war_sta_in_list_extn(struct hostapd_data *hapd, const u8 *addr)
{
	struct hostapd_data_extn *h_ext;
	struct hostapd_mu_cap_war_sta_entry_extn *e;

	h_ext = &hapd->hapd_extn;

	dl_list_for_each(e, &h_ext->mu_cap_war_sta_list,
			 struct hostapd_mu_cap_war_sta_entry_extn, list) {
		if (os_memcmp(e->addr, addr, ETH_ALEN) == 0)
			return true;
	}

	return false;
}


void hostapd_mu_cap_war_client_cap_extn(struct hostapd_data *hapd,
				      struct sta_info *sta)
{
	struct sta_info_extn *s_ext = &sta->sta_extn;
	bool in_db;
	u32 vht_cap;

	s_ext->mu_cap_war_mu_capable = 0;
	s_ext->mu_cap_war_mu_force_join = 0;
	s_ext->mu_cap_war_su_join = 0;

	in_db = hostapd_mu_cap_war_sta_in_list_extn(hapd, sta->addr);
	vht_cap = le_to_host32(sta->vht_capabilities->vht_capabilities_info);

	if (vht_cap & VHT_CAP_MU_BEAMFORMER_CAPABLE) {
		if (!in_db)
			s_ext->mu_cap_war_mu_capable = 1;
		else
			s_ext->mu_cap_war_mu_force_join = 1;
	} else if (in_db) {
		s_ext->mu_cap_war_su_join = 1;
	}
}


static void hostapd_mu_cap_war_vht_mucap_reset(struct hostapd_data *hapd,
					       struct ieee80211_vht_capabilities *cap)
{
	u32 c;

	c = le_to_host32(cap->vht_capabilities_info);
	c &= ~(VHT_CAP_MU_BEAMFORMER_CAPABLE |
			VHT_CAP_MU_BEAMFORMEE_CAPABLE);

	cap->vht_capabilities_info = host_to_le32(c);
}


void hostapd_mu_cap_war_sta_list_flush_extn(struct hostapd_data *hapd)
{
	struct hostapd_data_extn *h_ext;
	struct hostapd_mu_cap_war_sta_entry_extn *e, *tmp;

	h_ext = &hapd->hapd_extn;

	dl_list_for_each_safe(e, tmp, &h_ext->mu_cap_war_sta_list,
			      struct hostapd_mu_cap_war_sta_entry_extn, list) {
		dl_list_del(&e->list);
		os_free(e);
	}
}


void hostapd_mu_cap_war_expire_queries(struct hostapd_data *hapd)
{
	struct hostapd_data_extn *h_ext = &hapd->hapd_extn;
	struct hostapd_mu_cap_war_sta_entry_extn *e, *tmp;
	struct os_reltime now, diff;

	os_get_reltime(&now);

	dl_list_for_each_safe(e, tmp, &h_ext->mu_cap_war_sta_list,
			      struct hostapd_mu_cap_war_sta_entry_extn, list) {
		os_reltime_sub(&now, &e->last_probe_time, &diff);
		if (diff.sec > MU_CAP_WAR_DB_ENTRY_TIMEOUT_SEC) {
			wpa_printf(MSG_DEBUG,
				   "MU_CAP_WAR: removing aged out STA " MACSTR
				   " from DB (last probe %ld sec ago)",
				   MAC2STR(e->addr), diff.sec);

			dl_list_del(&e->list);
			os_free(e);
		}
	}
}


void hostapd_mu_cap_war_update_db_extn(struct hostapd_data *hapd, const u8 *addr,
				       u8 *vht_cap_offset)
{
	struct hostapd_data_extn *h_ext;
	struct hostapd_mu_cap_war_sta_entry_extn *e;

	h_ext = &hapd->hapd_extn;

	if (!vht_cap_offset || vht_cap_offset[0] != WLAN_EID_VHT_CAP)
		return;

	dl_list_for_each(e, &h_ext->mu_cap_war_sta_list,
			 struct hostapd_mu_cap_war_sta_entry_extn, list) {
		if (os_memcmp(e->addr, addr, ETH_ALEN) == 0)
			goto update;
	}

	e = os_zalloc(sizeof(*e));
	if (!e)
		return;

	os_memcpy(e->addr, addr, ETH_ALEN);
	dl_list_add(&h_ext->mu_cap_war_sta_list, &e->list);

update:
	os_get_reltime(&e->last_probe_time);
	hostapd_mu_cap_war_vht_mucap_reset(hapd,
					   (struct ieee80211_vht_capabilities *)(vht_cap_offset + 2));
	wpa_printf(MSG_DEBUG,
		   "MU_CAP_WAR: Hacked VHT MU bits for STA" MACSTR "",
		   MAC2STR(addr));
}


static int hostapd_mu_cap_war_count_mu_clients_extn(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	int count = 0;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		struct sta_info_extn *s_ext = &sta->sta_extn;

		if (!(sta->flags & WLAN_STA_ASSOC))
			continue;

		if (s_ext->mu_cap_war_mu_force_join ||
		    s_ext->mu_cap_war_mu_capable)
			count++;
	}

	return count;
}


void hostapd_mu_cap_war_kickout_timer_extn(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta;
	int mu_cnt;

	mu_cnt = hostapd_mu_cap_war_count_mu_clients_extn(hapd);

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		struct sta_info_extn *s_ext = &sta->sta_extn;
		bool in_db;

		in_db = hostapd_mu_cap_war_sta_in_list_extn(hapd, sta->addr);
		if (!in_db)
			continue;

		if (mu_cnt > 0 && s_ext->mu_cap_war_su_join) {
			wpa_printf(MSG_DEBUG,
				   "MU_CAP_WAR: Deauth STA " MACSTR
				   " (joined via hacked probe; MU context present)",
				   MAC2STR(sta->addr));

			/* Disconnect STA to come back with MU capable */
			hostapd_drv_sta_deauth(hapd, sta->addr,
					       WLAN_REASON_PREV_AUTH_NOT_VALID);
			continue;
		}

		if (mu_cnt == 1 && s_ext->mu_cap_war_mu_force_join &&
		    !s_ext->mu_cap_war_su_join) {
			wpa_printf(MSG_DEBUG,
				   "MU_CAP_WAR: Deauth STA " MACSTR
				   " (joined via normal probe; no other MU clients)",
				   MAC2STR(sta->addr));
			hostapd_drv_sta_deauth(hapd, sta->addr,
					       WLAN_REASON_PREV_AUTH_NOT_VALID);
		}
	}
}


void hostapd_mu_cap_war_mu_state_changed_extn(struct hostapd_data *hapd)
{
	struct hostapd_data_extn *h_ext;
	int mu_cnt;

	h_ext = &hapd->hapd_extn;
	mu_cnt = hostapd_mu_cap_war_count_mu_clients_extn(hapd);
	h_ext->mu_cap_war_override = (mu_cnt >= 2);

	eloop_cancel_timeout(hostapd_mu_cap_war_kickout_timer_extn, hapd, NULL);
	eloop_register_timeout(15, 0, hostapd_mu_cap_war_kickout_timer_extn, hapd, NULL);
}

#endif /* CONFIG_IEEE80211AC */
