/*
 * Airtime Fairness offload feature wrappers and apis.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.*
 */

#include <sys/un.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"
#include "ap/sta_info.h"

#include "utils/includes.h"
#include <netlink/genl/genl.h>

#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "common/wpa_ctrl.h"
#include "common/qca-vendor.h"
#include "common/qca-vendor-attr.h"
#include "../src/drivers/driver_nl80211.h"

#include "atf_offload.h"
#include "atf_offload_config.h"

#include "ap/ubus.h"
#include "ap/interference.h"

struct atf_offload *atf = NULL;

/* one second timeout for Airtime distribution */
#define ATF_ALGO_TIMEOUT 1

u8 atf_get_hw_idx(struct hostapd_iface *iface)
{
	if (iface->current_hw_info)
		return iface->current_hw_info->hw_idx;
	return 0;
}

void atf_offload_initialize_peer(struct sta_info *sta)
{
	wpa_printf(MSG_DEBUG, "ATF: initialize sta %p %p",
		   sta, &sta->atf_candidate_list);
	dl_list_init(&sta->atf_candidate_list);
}


void atf_offload_deinitialize_peer(struct sta_info *sta)
{
	wpa_printf(MSG_DEBUG, "ATF: deinitialize sta %p %p",
		   sta, &sta->atf_candidate_list);
	if (dl_list_empty(&sta->atf_candidate_list)) {
		wpa_printf(MSG_ERROR, "ATF: sta already deinitialized");
		return;
	}
	dl_list_del(&sta->atf_candidate_list);
	dl_list_init(&sta->atf_candidate_list);
}


void atf_offload_set_ssid_sched_policy(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface;
	struct atf_algo *algo;
	u8 radio_index, link_id = -1;

	if (!hapd->iface || !hapd->iface->atf_algo || !hapd->drv_priv) {
		wpa_printf(MSG_DEBUG, "ATF: Invalid hapd data %s\n", __func__);
		return;
	}

	iface = hapd->iface;
	algo = hapd->iface->atf_algo;

	if (!algo->atf_enabled) {
		wpa_printf(MSG_ERROR, "ATF: ATF is not enabled\n");
		return;
	}

	if (!hapd->conf->atf_ssid_sched)
		return;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	radio_index = atf_get_hw_idx(iface);

	if (nl80211_atf_offload_ssid_sched_policy(hapd->drv_priv, radio_index,
						    hapd->conf->atf_ssid_sched,
						    link_id))
		wpa_printf(MSG_ERROR, "ATF: Failed to set ssid scheduling policy\n");
}


void atf_offload_send_feature_params(struct hostapd_data *hapd)
{
	struct hostapd_iface *iface;
	struct atf_algo *algo;
	u8 radio_index;

	if (!hapd->iface || !hapd->iface->atf_algo || !hapd->drv_priv) {
		wpa_printf(MSG_DEBUG, "ATF: Invalid hapd data %s\n", __func__);
		return;
	}

	iface = hapd->iface;
	algo = iface->atf_algo;

	if (!iface->conf->atf_offload) {
		wpa_printf(MSG_ERROR, "ATF: ATF offload feature is not enabled\n");
		return;
	}

	radio_index = atf_get_hw_idx(iface);

	if (algo->atf_enabled) {
		if (nl80211_atf_offload_enable_disable(hapd->drv_priv, radio_index,
						       algo->atf_enabled)) {
			wpa_printf(MSG_ERROR,
				   "ATF: Failed to send commitatf, disabling commitatf\n");
			algo->atf_enabled = 0;
		}
	}

	/* Even if commitatf via UCI fails, ATF strict scheduling will still be enabled in the
	 * firmware if it has been configured through UCI. Once commitatf is successfully enabled,
	 * ATF strict scheduling configuration will be applied.
	 */
	if (algo->atfstrictsched_enabled) {
		if (nl80211_atf_offload_strict_scheduling_enable_disable(hapd->drv_priv,
									 radio_index,
									 algo->atfstrictsched_enabled)) {
			wpa_printf(MSG_ERROR,
				   "ATF: Failed to enable strict scheduling, disabling strict scheduling\n");
			algo->atfstrictsched_enabled = 0;
		}
	}
}


struct atf_peer_config *
atf_allocate_peer_config(u8 *macaddr, struct atf_algo *algo)
{
	struct atf_peer_config *peer_config;

	if (algo->num_peer_cfg >= ATF_MAX_PEER)
		return NULL;

	peer_config = os_zalloc(sizeof(*peer_config));
	if (!peer_config)
		return NULL;

	os_memcpy(peer_config->addr, macaddr, ETH_ALEN);
	dl_list_init(&peer_config->list);
	dl_list_add(&algo->peer_cfgs, &peer_config->list);
	peer_config->algo = algo;
	algo->num_peer_cfg++;

	wpa_printf(MSG_INFO, "ATF: Added peer config %d", algo->num_peer_cfg);

	return peer_config;
}


void
atf_free_peer_config(struct atf_peer_config *peer_config)
{
	struct atf_algo *algo = peer_config->algo;

	if (!algo->num_peer_cfg)
		return;

	algo->num_peer_cfg--;
	dl_list_del(&peer_config->list);
	os_free(peer_config);
}


struct atf_peer_config *
atf_find_peer_config_by_mac(u8 *mac, struct atf_algo *algo)
{
	struct atf_peer_config *peer = NULL;

	if (!algo) {
		wpa_printf(MSG_ERROR, "ATF: algo is null");
		return NULL;
	}

	if (dl_list_empty(&algo->peer_cfgs)) {
		wpa_printf(MSG_DEBUG, "ATF: peer list is empty");
		return NULL;
	}

	dl_list_for_each(peer, &algo->peer_cfgs, struct atf_peer_config, list)
	{
		if (os_memcmp(mac, peer->addr, ETH_ALEN) == 0)
			return peer;
	}

	return NULL;
}


void
atf_iterate_peer_config(struct atf_algo *algo,
                        void (*callback)(struct atf_algo *, struct atf_peer_config *))
{
	struct atf_peer_config *peer;

	if (!algo) {
		wpa_printf(MSG_ERROR, "ATF: algo is null");
		return;
	}

	dl_list_for_each(peer, &algo->peer_cfgs, struct atf_peer_config, list)
		callback(algo, peer);
}


struct atf_ssid_config *
atf_allocate_ssid_config(char *name, struct atf_algo *algo)
{
	struct atf_ssid_config *ssid_config;

	if (algo->num_ssid_cfg >= ATF_MAX_SSID)
		return NULL;

	ssid_config = os_zalloc(sizeof(struct atf_ssid_config));
	if (!ssid_config)
		return NULL;

	os_strlcpy(ssid_config->name, name, sizeof(ssid_config->name));
	dl_list_init(&ssid_config->list);
	dl_list_add(&algo->ssid_cfgs, &ssid_config->list);
	ssid_config->algo = algo;
	algo->num_ssid_cfg++;

	wpa_printf(MSG_INFO, "ATF: Added ssid config %s [%d]", ssid_config->name,
	           algo->num_ssid_cfg);

	return ssid_config;
}


void
atf_free_ssid_config(struct atf_ssid_config *ssid)
{
	struct atf_algo *algo;

	if (!ssid) {
		wpa_printf(MSG_INFO, "ATF: %s ssid_config is NULL", __func__);
		return;
	}

	algo = ssid->algo;
	if (!algo->num_ssid_cfg)
		return;

	algo->num_ssid_cfg--;
	dl_list_del(&ssid->list);
	os_free(ssid);
}


struct atf_ssid_config *
atf_find_ssid_config_by_name(char *name, struct atf_algo *algo)
{

	struct atf_ssid_config *ssid;

	if (!name || !algo) {
		wpa_printf(MSG_ERROR, "ATF: %s invalid arguments", __func__);
		return NULL;
	}

	if (dl_list_empty(&algo->ssid_cfgs))
		return NULL;

	dl_list_for_each(ssid, &algo->ssid_cfgs, struct atf_ssid_config, list)
	{
		size_t len = strlen(ssid->name);
		if (strlen(name) == len && os_strncmp(name, ssid->name, len) == 0)
			return ssid;
	}
	return NULL;
}


int
atf_add_ssid_to_group(struct atf_group *group, const char *name)
{
	if (group->num_of_ssid == WLAN_SSID_MAX)
		return -1;

	wpa_printf(MSG_INFO, "ATF: Adding %s  to group %s", name, group->name);
	os_strlcpy(group->ssidname[group->num_of_ssid], name, WLAN_SSID_MAX_LEN);
	group->num_of_ssid++;

	return 0;
}


void
atf_delete_ssid_from_group(struct atf_algo *algo, const char *name)
{
        struct atf_group *group;
        int i, j;

        dl_list_for_each(group, &algo->groups, struct atf_group, list)
        {
                for (i = 0; i < group->num_of_ssid; i++) {
			size_t len = strlen(group->ssidname[i]);
                        if (strlen(name) == len &&
			    os_strncmp(name, group->ssidname[i], len) == 0) {
                                for ( j = i; j < group->num_of_ssid - 1; j++)
                                        os_strlcpy(group->ssidname[j],
						    group->ssidname[j + 1],
						    WLAN_SSID_MAX_LEN);
                                group->num_of_ssid--;
                                break;
                        }
                }
        }
}


void
atf_clear_candidate_list(struct atf_group *group)
{
	struct sta_info *sta, *tmp;

	if (!dl_list_empty(&group->implicit_peers)) {
		dl_list_for_each_safe(sta, tmp, &group->implicit_peers, struct sta_info,
		                      atf_candidate_list)
		{
			atf_offload_deinitialize_peer(sta);
		}
	}

	if (!dl_list_empty(&group->explicit_peers)) {
		dl_list_for_each_safe(sta, tmp, &group->explicit_peers, struct sta_info,
		                      atf_candidate_list)
		{
			atf_offload_deinitialize_peer(sta);
		}
	}
}


void
atf_reset_group_values(struct atf_group *group)
{
	group->calculated_airtime = 0;
	group->total_explicit_airtime = 0;

	atf_clear_candidate_list(group);
	if (!dl_list_empty(&group->implicit_peers) ||
	    !dl_list_empty(&group->explicit_peers)) {
		wpa_printf(MSG_ERROR, "ATF: Unexpected! peer candidate list was not cleared\n");
	}
	dl_list_init(&group->implicit_peers);
	dl_list_init(&group->explicit_peers);
	group->num_impl_peers = 0;
	group->num_expl_peers = 0;
	group->num_bh_peers = 0;
}

void
atf_reset_group_ac_config(struct atf_group *group)
{
	u8 i;

	for (i = 0; i < WMM_AC_NUM; i++) {
		group->ac_airtime[i] = 0;
		group->ac_actual_airtime[i] = 0;
		group->ac_ul_airtime[i] = 0;
		group->calculated_ac_airtime[i] = 0;
	}

	group->ac_bitmap = 0;
}


struct atf_group *
atf_allocate_group(const char *name, struct atf_algo *algo)
{
	struct atf_group *group;

	if (algo->num_group_cfg >= ATF_MAX_SSID_GROUP)
		return NULL;

	group = os_zalloc(sizeof(struct atf_group));
	if (!group)
		return NULL;

	group->index = algo->num_group_cfg;
	os_strlcpy(group->name, name, sizeof(group->name));

	dl_list_init(&group->list);
	dl_list_add(&algo->groups, &group->list);
	group->algo = algo;
	algo->num_group_cfg++;

	dl_list_init(&group->implicit_peers);
	dl_list_init(&group->explicit_peers);
	atf_reset_group_ac_config(group);
	atf_reset_group_values(group);

	wpa_printf(MSG_INFO, "ATF: Added group %s [%d], no of groups %d", group->name,
	           group->index, algo->num_group_cfg);

	return group;
}


void
atf_free_group(struct atf_group *group)
{
	struct atf_algo *algo = group->algo;

	if (!algo->num_group_cfg)
		return;

	atf_reset_group_ac_config(group);
	atf_reset_group_values(group);
	algo->num_group_cfg--;
	dl_list_del(&group->list);
	os_free(group);
	group = NULL;
}


static void
atf_free_peer_configs(struct atf_algo *algo)
{
	struct atf_peer_config *peer, *tmp;

	if (dl_list_empty(&algo->peer_cfgs))
		return;

	dl_list_for_each_safe(peer, tmp, &algo->peer_cfgs, struct atf_peer_config, list)
		atf_free_peer_config(peer);
}

static void
atf_free_bh_peers(struct atf_algo *algo)
{
	struct atf_bh_peer *bh, *tmp;

	if (dl_list_empty(&algo->bh_peers))
		return;

	dl_list_for_each_safe(bh, tmp, &algo->bh_peers, struct atf_bh_peer, list) {
		dl_list_del(&bh->list);
		os_free(bh);
	}
}


static void
atf_free_ssid_configs(struct atf_algo *algo)
{
	struct atf_ssid_config *ssid_cfg, *tmp;

	if (dl_list_empty(&algo->ssid_cfgs))
		return;

	dl_list_for_each_safe(ssid_cfg, tmp, &algo->ssid_cfgs,
			      struct atf_ssid_config, list) {
		atf_free_ssid_config(ssid_cfg);
	}
}


static void
atf_free_group_configs(struct atf_algo *algo, bool skip_default)
{
	struct atf_group *group, *tmp;

	if (dl_list_empty(&algo->groups))
		return;

	dl_list_for_each_safe(group, tmp, &algo->groups, struct atf_group, list) {
		if (!skip_default ||
		    os_strncmp(group->name, "default-group", strlen("default-group")) != 0)
				atf_free_group(group);
	}
}


void
atf_free_algo_configs(struct atf_algo *algo, bool skip_default)
{
	atf_free_peer_configs(algo);

	/* Ensure all peer config is cleared by checking list is empty.
	 * if list is not empty, its corrupted.
	 */
	if (!dl_list_empty(&algo->peer_cfgs)) {
		wpa_printf(MSG_ERROR, "ATF: peer config is not cleared!\n");
	}
	algo->num_peer_cfg = 0;
	dl_list_init(&algo->peer_cfgs);

	atf_free_bh_peers(algo);
	if (!dl_list_empty(&algo->bh_peers))
		wpa_printf(MSG_ERROR, "ATF: bh_peers not cleared!\n");
	dl_list_init(&algo->bh_peers);

	atf_free_ssid_configs(algo);

	/* Ensure ssid config list is empty after the cleanup.
	 * if list is not empty, list is corrupted.
	 */
	if (!dl_list_empty(&algo->ssid_cfgs)) {
		wpa_printf(MSG_ERROR, "ATF: Unexpected! ssid config is not cleared!\n");
	}
	algo->num_ssid_cfg = 0;
	algo->user_cfg_airtime = 0;
	dl_list_init(&algo->ssid_cfgs);

	atf_free_group_configs(algo, skip_default);
	/* In case of flush table, preserve the default group */
	if (skip_default)
		return;

	/* Ensure list is empty after removing all group configs.
	 * if list is not empty, something is corrupted.
	 */
	if (!dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: Unexpected! group config is not cleared.\n");
	}

	algo->num_group_cfg = 0;
	dl_list_init(&algo->groups);
}


struct atf_group *
atf_find_group_by_name(const char *name, struct atf_algo *algo)
{
	struct atf_group *group;

	if (!name)
		return NULL;

	if (!algo) {
		wpa_printf(MSG_ERROR, "ATF: %s: algo is NULL", __func__);
		return NULL;
	}

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: %s: group list is empty", __func__);
		return NULL;
	}

	dl_list_for_each(group, &algo->groups, struct atf_group, list)
	{
		size_t len = strlen(group->name);
		if (strlen(name) == len && os_strncmp(name, group->name, len) == 0)
			return group;
	}

	return NULL;
}


void
atf_cleanup_algo(struct atf_algo *algo)
{
	if (!algo)
		return;

	atf_free_algo_configs(algo, false);
	dl_list_del(&algo->list);
	os_free(algo);
	algo = NULL;
}


struct atf_algo *
atf_allocate_algo()
{
	struct atf_algo *algo;

	algo = os_zalloc(sizeof(*algo));
	if (!algo)
		return NULL;

	wpa_printf(MSG_DEBUG, "ATF: Allocated new algo structure for atf offload\n");
	dl_list_init(&algo->list);
	dl_list_add(&atf->algo_list, &algo->list);
	algo->num_group_cfg = 0;
	dl_list_init(&algo->groups);
	algo->num_ssid_cfg = 0;
	dl_list_init(&algo->ssid_cfgs);
	algo->num_peer_cfg = 0;
	dl_list_init(&algo->peer_cfgs);
	dl_list_init(&algo->bh_peers);
	algo->init_update_done = 0;

	return algo;
}


struct atf_algo *
atf_get_algo_entry(struct hostapd_iface *iface)
{
	struct atf_algo *algo = NULL;

	algo = iface->atf_algo;
	if (!algo) {
		algo = atf_allocate_algo();
		if (!algo)
			return NULL;
	}

	return algo;
}


void
atf_join_leave_update(struct hostapd_iface *iface, struct sta_info *sta, bool is_join)
{
	if (!iface || !iface->atf_algo || !sta)
		return;

	wpa_printf(MSG_DEBUG, "ATF: sta %p is %s %p", sta,
		   is_join ? "join" : "leave" , &sta->atf_candidate_list);
	ATF_SET_STA_TO_UPDATE(sta->atf_peer);

	if (is_join && !dl_list_empty(&sta->atf_candidate_list)) {
		wpa_printf(MSG_DEBUG, "ATF: sta %p is reassociating %p", sta, &sta->atf_candidate_list);
		/* when sta reassociates, re-initialize peer so that
		 * it would avoid adding two node
		 */
		atf_offload_deinitialize_peer(sta);
	}

	/* After reboot, when first client joins it should be
	 * full update.
	 */
	if (is_join && !iface->atf_algo->init_update_done) {
		ATF_OFFLOAD_SET_FULL_UPDATE(iface->atf_algo);
		iface->atf_algo->init_update_done = 1;
	} else if (is_join) {
		ATF_OFFLOAD_SET_JOIN_UPDATE(iface->atf_algo);
	} else {
		atf_offload_deinitialize_peer(sta);
		ATF_OFFLOAD_SET_LEAVE_UPDATE(iface->atf_algo);
	}

	atf_trigger_config_timer(iface);
}

/*
 * atf_get_root_ap_addr - Fetch the Root AP link BSSID via the wpa_supplicant
 * control interface of the backhaul STA.
 *
 * For MLO connections the ap_link_addr whose frequency matches iface->freq
 * is selected from MLO_STATUS.
 * For non-MLO connections the bssid field from STATUS is used as a fallback.
 *
 * @iface: AP interface of the repeater
 * @addr: Output buffer (ETH_ALEN bytes) to store the Root AP link BSSID
 *
 * Returns 0 on success, -1 on failure.
 */
static int atf_get_root_ap_addr(struct hostapd_iface *iface, u8 *addr)
{
	char ctrl_path[128];
	char reply[512];
	size_t reply_len;
	struct wpa_ctrl *ctrl;
	char *ifname = NULL;
	char *line, *saveptr;
	int ret = -1;

#ifdef UBUS_SUPPORT
	ifname = hostapd_ubus_bhsta_ifname(iface);
#endif /* UBUS_SUPPORT */
	if (!ifname) {
		wpa_printf(MSG_DEBUG,
			   "ATF: bhsta ifname not available, skip adding root AP to sta list");
		return -1;
	}

	if (os_snprintf(ctrl_path, sizeof(ctrl_path),
			"/var/run/wpa_supplicant/%s", ifname) >=
	    (int)sizeof(ctrl_path)) {
		wpa_printf(MSG_ERROR, "ATF: bhsta ctrl path too long");
		free(ifname);
		return -1;
	}
	free(ifname);

	ctrl = wpa_ctrl_open(ctrl_path);
	if (!ctrl) {
		wpa_printf(MSG_DEBUG, "ATF: cannot open bhsta ctrl %s",
			   ctrl_path);
		return -1;
	}

	reply_len = sizeof(reply) - 1;
	if (wpa_ctrl_request(ctrl, "MLO_STATUS", 10, reply, &reply_len,
			     NULL) == 0) {
		u8 ap_link_addr[ETH_ALEN];
		int link_freq = -1;
		bool found_addr = false;

		reply[reply_len] = '\0';
		for (line = strtok_r(reply, "\n", &saveptr); line;
		     line = strtok_r(NULL, "\n", &saveptr)) {
			if (os_strncmp(line, "freq=", 5) == 0) {
				link_freq = atoi(line + 5);
			} else if (os_strncmp(line, "ap_link_addr=", 13) == 0) {
				if (hwaddr_aton(line + 13, ap_link_addr) == 0)
					found_addr = true;
			} else if (os_strncmp(line, "link_id=", 8) == 0) {
				/* start of a new link block - reset state */
				link_freq = -1;
				found_addr = false;
			}
			if (found_addr && link_freq == iface->freq) {
				os_memcpy(addr, ap_link_addr, ETH_ALEN);
				ret = 0;
				break;
			}
		}
	}

	/* For legacy non-MLD root AP, call STATUS instead */
	if (ret != 0) {
		u8 ap_bssid[ETH_ALEN];
		int sta_freq = -1;
		bool found_bssid = false;

		reply_len = sizeof(reply) - 1;
		if (wpa_ctrl_request(ctrl, "STATUS", 6, reply, &reply_len,
				     NULL) < 0) {
			wpa_printf(MSG_ERROR,
				   "ATF: STATUS request to bhsta failed");
			wpa_ctrl_close(ctrl);
			return -1;
		}
		reply[reply_len] = '\0';
		for (line = strtok_r(reply, "\n", &saveptr); line;
		     line = strtok_r(NULL, "\n", &saveptr)) {
			if (os_strncmp(line, "bssid=", 6) == 0) {
				if (hwaddr_aton(line + 6, ap_bssid) == 0)
					found_bssid = true;
			} else if (os_strncmp(line, "freq=", 5) == 0) {
				sta_freq = atoi(line + 5);
			}
			if (found_bssid && sta_freq == iface->freq) {
				os_memcpy(addr, ap_bssid, ETH_ALEN);
				ret = 0;
				break;
			}
		}
		if (ret != 0)
			wpa_printf(MSG_DEBUG,
				   "ATF: bssid not found in bhsta STATUS");
	}

	wpa_ctrl_close(ctrl);

	return ret;
}


/*
 * atf_add_bhsta_to_bh_peers - Add an atf_bh_peer record for the Root AP
 * BSSID into algo->bh_peers so that the ATF engine accounts for the upstream
 * backhaul link.  ATF-internal only - never touches hapd->sta_list.
 *
 * Only called when the backhaul STA is fully associated with the Root AP
 * (hostapd_is_backhaul_sta_conn() == true, i.e. wpa_state == COMPLETED).
 *
 * @iface: AP interface of the repeater
 */
void atf_add_bhsta_to_bh_peers(struct hostapd_iface *iface)
{
	struct atf_algo *algo;
	struct atf_bh_peer *bh;
	u8 addr[ETH_ALEN];

	if (!iface || !iface->atf_algo)
		return;

	algo = iface->atf_algo;

	if (atf_get_root_ap_addr(iface, addr) != 0) {
		wpa_printf(MSG_DEBUG, "ATF: could not get root AP BSSID");
		return;
	}

	dl_list_for_each(bh, &algo->bh_peers, struct atf_bh_peer, list) {
		if (ether_addr_equal(bh->addr, addr)) {
			wpa_printf(MSG_DEBUG,
				   "ATF: root AP " MACSTR " already in bh_peers",
				   MAC2STR(addr));
			return;
		}
	}

	bh = os_zalloc(sizeof(*bh));
	if (!bh) {
		wpa_printf(MSG_ERROR, "ATF: failed to alloc bh_peer for root AP");
		return;
	}

	os_memcpy(bh->addr, addr, ETH_ALEN);
	dl_list_add_tail(&algo->bh_peers, &bh->list);
	ATF_SET_STA_TO_UPDATE(bh->atf_peer);

	wpa_printf(MSG_INFO,
		   "ATF: added root AP " MACSTR " to bh_peers",
		   MAC2STR(addr));
}


/*
 * atf_remove_bhsta_from_bh_peers - Remove all bh_peer entries from
 * algo->bh_peers.
 *
 * @iface: AP interface of the repeater
 */
void atf_remove_bhsta_from_bh_peers(struct hostapd_iface *iface)
{
	if (!iface || !iface->atf_algo)
		return;

	atf_free_bh_peers(iface->atf_algo);
}


/*
 * atf_sync_bhsta_bh_peers - Ensure the bh_peer entry for the Root AP is
 * present when the backhaul STA is up, and absent when it is not.
 * Always flushes the existing list first so that a roam to a new root AP
 * does not leave the old BSSID in bh_peers alongside the new one.
 * Called at the start of every candidate-list build.
 */
static void atf_sync_bhsta_bh_peers(struct hostapd_iface *iface)
{
	atf_remove_bhsta_from_bh_peers(iface);
	if (hostapd_is_backhaul_sta_conn(iface))
		atf_add_bhsta_to_bh_peers(iface);
}

/*
 * atf_bh_join_leave_update - Notify ATF that the backhaul STA has connected to or
 * disconnected from the Root AP.
 *
 * Mirrors atf_join_leave_update() for regular STAs.  On connect, syncs the
 * bh_peers list and triggers a full ATF update.  On disconnect, removes the
 * Root AP from bh_peers and triggers a leave update.
 *
 * @iface:   AP interface of the repeater
 * @is_conn: true when the backhaul STA has just connected, false on disconnect
 */
void
atf_bh_join_leave_update(struct hostapd_iface *iface, bool is_conn)
{
	if (!iface || !iface->atf_algo)
		return;

	wpa_printf(MSG_DEBUG, "ATF: bhsta %s", is_conn ? "connected" : "disconnected");

	if (is_conn) {
		atf_add_bhsta_to_bh_peers(iface);
		if (!iface->atf_algo->init_update_done) {
			ATF_OFFLOAD_SET_FULL_UPDATE(iface->atf_algo);
			iface->atf_algo->init_update_done = 1;
		} else {
			ATF_OFFLOAD_SET_JOIN_UPDATE(iface->atf_algo);
		}
	} else {
		atf_remove_bhsta_from_bh_peers(iface);
		ATF_OFFLOAD_SET_LEAVE_UPDATE(iface->atf_algo);
	}

	atf_trigger_config_timer(iface);
}


void
atf_trigger_config_timer(struct hostapd_iface *iface)
{

	wpa_printf(MSG_INFO, "ATF: Trigger distribution of airtime for iface %p", iface);
	if (iface->atf_algo->atf_tasksched == 0) {
		iface->atf_algo->atf_tasksched = 1;
		atf_timer_start(iface);
	}
}


void
atf_reset_groups(struct atf_algo *algo)
{
	struct atf_group *group;

	algo->no_of_peers = 0;

	if (dl_list_empty(&algo->groups))
		return;

	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		atf_reset_group_values(group);
		if (os_strncmp(group->name, "default-group", strlen("default-group")) != 0) {
			group->is_configured = 0;
		}
	}

}


struct atf_group *
atf_is_ssid_in_default_group(const char *name, struct atf_algo *algo, bool update)
{
	int i;
	struct atf_group *group;

	if (!name || !algo) {
		return NULL;
	}

	group = atf_find_group_by_name("default-group", algo);
	if (!group) {
		wpa_printf(MSG_ERROR, "ATF: default-group is not found in %p", algo);
		return NULL;
	}

	for (i = 0; i < group->num_of_ssid; i++) {
		size_t len = strlen(group->ssidname[i]);
		if (strlen(name) == len && !os_strncmp(name, group->ssidname[i], len))
			return group;
	}

	/* SSID is not found in default-group.
	 * if update == true, then update the ssid to default-group.
	 * if update == false, return NULL.
	 */
	if (update) {
		if (atf_add_ssid_to_group(group, name)) {
			wpa_printf(MSG_ERROR, "ATF: Failed to add ssid to group");
			return NULL;
		}

		return group;
	}

	return NULL;
}


struct atf_group *
atf_find_group_if_ssid_exist(const char *name, struct atf_algo *algo)
{
	int i;
	struct atf_group *group;

	if (!algo) {
		wpa_printf(MSG_ERROR, "ATF: Algo is null");
		return NULL;
	}

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: %s: group is empty", __func__);
		return NULL;
	}

	dl_list_for_each(group, &algo->groups, struct atf_group, list)
	{
		for (i = 0; i < group->num_of_ssid; i++) {
			size_t len = strlen(group->ssidname[i]);
			if (strlen(name) == len &&
			    !os_strncmp(name, group->ssidname[i], len))
				return group;
		}
	}

	return NULL;
}


struct atf_group *
atf_find_group(struct atf_algo *algo, const char *name)
{
	size_t name_len;
	if (!algo || !name) {
		wpa_printf(MSG_ERROR, "ATF: Invalid parameters to find_group");
		return NULL;
	}

	name_len = os_strlen(name);
	if (name_len == 0 || name_len > WLAN_SSID_MAX_LEN) {
		wpa_printf(MSG_ERROR, "ATF: Invalid name length in find_group: %zu", name_len);
		return NULL;
	}

	if (algo->ssid_group_enabled) {
		wpa_printf(MSG_DEBUG, "ATF: find group with ssid name %s %p", name, algo);
		return atf_find_group_if_ssid_exist(name, algo);
	}

	/* In case of non-ssid group, group name is same as ssid */
	return atf_find_group_by_name(name, algo);
}


/**
 * atf_update_interfaces_to_group - update all the ssid to its corresponding
 * group.
 */
static int
atf_update_interfaces_to_group(struct atf_algo *algo)
{
	struct hostapd_iface *iface = algo->iface;
	struct atf_group *group = NULL;
	struct hostapd_data *bss;
	struct hostapd_ssid *ssid;
	char ssid_buf[SSID_MAX_LEN + 1];
	int i;
	int ret = 0;

	for (i = 0; i < iface->num_bss; i++) {
		bss = iface->bss[i];
		if (!bss->started)
			continue;

		ssid = &bss->conf->ssid;
		os_memset(ssid_buf, 0, sizeof(ssid_buf));
		os_memcpy(ssid_buf, ssid->ssid, ssid->ssid_len);
		ssid_buf[ssid->ssid_len] = '\0';

		group = atf_find_group(algo, ssid_buf);
		if (!group) {
			wpa_printf(MSG_DEBUG, "ATF:ssid %s is not linked with any configured group",
			           ssid_buf);

			/* check if the ssid is in default group already, if
			 * not add it to default-group
			 */
			group = atf_is_ssid_in_default_group(ssid_buf, algo, true);
			if (!group) {
				wpa_printf(MSG_DEBUG, "ATF: Not able to update  %s to default-group",
					   ssid_buf);
				ret = 1;
				continue;
			}
			bss->atf_configured = 0;
		} else {
			group->is_configured = 1;
			bss->atf_configured = 1;
		}

		/* copy the scheduling policy of ssid in case of ATF based on ssid */
		if (!algo->ssid_group_enabled)
			group->sched_policy = bss->conf->atf_ssid_sched;
	}

	return ret;
}


struct atf_group *
atf_get_peer_group(struct hostapd_data *hapd)
{
	struct atf_group *group;
	struct atf_algo *algo;
	char ssid_buf[SSID_MAX_LEN + 1];

	if (!hapd->conf->ssid.ssid_len) {
		wpa_printf(MSG_ERROR, "ATF: Unexpected! conf doesnt have ssid");
		return NULL;
	}

	if (!hapd->iface || !hapd->iface->atf_algo)
		return NULL;

	algo = hapd->iface->atf_algo;

	os_memcpy(ssid_buf, hapd->conf->ssid.ssid, hapd->conf->ssid.ssid_len);
	ssid_buf[hapd->conf->ssid.ssid_len] = '\0';

	group = atf_find_group(algo, ssid_buf);
	if (!group) {
		/* check if it is in default group */
		return atf_is_ssid_in_default_group(ssid_buf, algo, false);
	}

	return group;
}


int
atf_update_peer(struct hostapd_data *hapd, struct sta_info *sta, void *ctx)
{
	struct atf_group *group = NULL;
	struct atf_algo *algo;

	if (!sta) {
		wpa_printf(MSG_ERROR, "ATF: %s: sta is null", __func__);
		return -1;
	}

	/* consider only the authorized sta */
	if (sta && !ap_sta_is_authorized(sta))
		return 0;

	if (!hapd || !hapd->iface || !hapd->iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: unexpected, interface is not proper");
		return -1;
	}

	algo = hapd->iface->atf_algo;

	if (ATF_OFFLOAD_IS_FULL_UPDATE(algo) ||
	    (ATF_OFFLOAD_IS_JOIN_UPDATE(algo) && ATF_IS_STA_UPDATED(sta->atf_peer)))
		algo->no_of_peers++;

	wpa_printf(MSG_DEBUG, "ATF: update peer " MACSTR "peer->is_configured %d",
			MAC2STR(sta->addr), sta->atf_peer.atf_configured);

	if (!sta->atf_peer.atf_configured) {
		/* add it to implicit peer list */
		group = atf_get_peer_group(hapd);
		if (!group) {
			wpa_printf(MSG_ERROR,
			           "ATF: Unexpected! peer" MACSTR "should be associated with a group",
				   MAC2STR(sta->addr));
			return -1;
		}
		sta->atf_peer.group = group;
		dl_list_add(&group->implicit_peers, &sta->atf_candidate_list);
		group->num_impl_peers++;
	} else {
		group = sta->atf_peer.peer_cfg_ref->group;
		if (!group) {
                        wpa_printf(MSG_ERROR,
                                   "ATF: peer" MACSTR "should be associated with configured group",
                                   MAC2STR(sta->addr));

			return -1;
		}
		sta->atf_peer.group = group;
		group->num_expl_peers++;
	}

	return 0;
}


void
atf_update_peer_cfg_to_peer(struct atf_algo *algo, struct atf_peer_config *peer_cfg)
{
	struct hostapd_iface *iface = algo->iface;
	struct hostapd_data *bss;
	struct sta_info *sta = NULL;
	int i;
	struct atf_group *group = NULL;
	struct hostapd_ssid *ssid;
	char ssid_buf[SSID_MAX_LEN + 1];

	if (!peer_cfg)
		return;

	if (!peer_cfg->group) {
		wpa_printf(MSG_ERROR, "ATF: Peer should always be associated with group");
		return;
	}

	for (i = 0; i < iface->num_bss; i++) {
		bss = iface->bss[i];
		if (!bss->started || !bss->atf_configured)
			continue;

		sta = ap_get_sta(bss, peer_cfg->addr);
		if (!bss->mld) {
			if (sta && ap_sta_is_authorized(sta))
				break;
		} else {
			if (!sta) {
				/* when user configured peer config with link address */
				wpa_printf(MSG_DEBUG, "ATF: sta not found, find by link");
				sta = ap_get_link_sta(bss, peer_cfg->addr);
				if (sta && ap_sta_is_authorized(sta))
					break;
			} else if (sta->mld_info.mld_sta == true) {
				/* when mld address is same as link address, we have
				 * to ensure that its the link address as user
				 * gives only link address
				 */
				sta = ap_get_link_sta(bss, peer_cfg->addr);
				if (!sta) {
					wpa_printf(MSG_DEBUG, "ATF: Link not found");
					continue;
				}
				if (ap_sta_is_authorized(sta))
					break;
			} else {
				/* when its legacy station, we just need to check the
				 * authorized flag
				 */
				wpa_printf(MSG_DEBUG, "ATF:sta is not ml");
				if (ap_sta_is_authorized(sta))
					break;
			}
		}

	}

	if (sta == NULL) {
		wpa_printf(MSG_DEBUG,
		           "ATF: Station " MACSTR " not found "
		           "for ATF Configuration",
		           MAC2STR(peer_cfg->addr));
		return;
	}

	ssid = &bss->conf->ssid;
	os_memset(ssid_buf, 0, sizeof(ssid_buf));
	os_memcpy(ssid_buf, ssid->ssid, ssid->ssid_len);
	ssid_buf[ssid->ssid_len] = '\0';

	group = atf_find_group(algo, ssid_buf);
	if (!group)
		return;

	if (peer_cfg->group != group)
		return;

	sta->atf_peer.atf_configured = true;
	sta->atf_peer.peer_cfg_ref = peer_cfg;
	sta->atf_peer.sta = sta;
	peer_cfg->algo = algo;
	peer_cfg->calculated_for_airtime = true;

	/*update to the group candidate list*/
	dl_list_add(&peer_cfg->group->explicit_peers, &sta->atf_candidate_list);

	return;
}


static int
atf_update_bh_peer(struct atf_algo *algo, struct atf_bh_peer *bh)
{
	struct atf_group *group;

	if (ATF_OFFLOAD_IS_FULL_UPDATE(algo) ||
	    (ATF_OFFLOAD_IS_JOIN_UPDATE(algo) && ATF_IS_STA_UPDATED(bh->atf_peer)))
		algo->no_of_peers++;

	group = atf_find_group_by_name("default-group", algo);
	if (!group) {
		wpa_printf(MSG_ERROR,
			   "ATF: default-group not found for bh_peer " MACSTR,
			   MAC2STR(bh->addr));
		return -1;
	}

	bh->atf_peer.group = group;
	group->num_bh_peers++;
	return 0;
}

static int
atf_build_candidate_list(struct hostapd_iface *iface)
{
	struct atf_algo *algo;
	struct hostapd_data *hapd;
	struct atf_bh_peer *bh;
	int i;

	if (!iface || !iface->atf_algo)
		return -1;

	algo = iface->atf_algo;

	/* reset the previous group values */
	atf_reset_groups(algo);
	if (atf_update_interfaces_to_group(algo) != 0) {
		wpa_printf(MSG_ERROR, "ATF: could not map the interfaces to group");
		return -1;
	}

	if (!dl_list_empty(&algo->peer_cfgs)) {
		atf_iterate_peer_config(algo, atf_update_peer_cfg_to_peer);
	}

	for (i = 0; i < iface->num_bss; i++) {
		hapd = iface->bss[i];
		if (!hapd->started)
			continue;

		if (ap_for_each_sta(hapd, atf_update_peer, NULL))
			return -1;
	}

	dl_list_for_each(bh, &algo->bh_peers, struct atf_bh_peer, list)
		if (atf_update_bh_peer(algo, bh))
			return -1;

	return 0;
}

struct atf_peer *
atf_implicit_peer_cfg(u8 *mac, struct hostapd_data *hapd)
{
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct hostapd_data *bss;
	struct atf_group *group;
	struct sta_info *sta;
	struct atf_peer *peer_cfg;
	u8 addr[ETH_ALEN];

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return NULL;
	}

	algo = iface->atf_algo;

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: %s: group list is empty", __func__);
		return NULL;
	}

	dl_list_for_each(group, &algo->groups, struct atf_group, list)
	{
		if (dl_list_empty(&group->implicit_peers))
			return NULL;

		if (group->num_impl_peers == 0)
			return NULL;

		dl_list_for_each(sta, &group->implicit_peers,
				 struct sta_info, atf_candidate_list) {
			peer_cfg = &sta->atf_peer;
			bss = sta->atf_peer.bss;

			if (ap_sta_is_mld(bss, sta))
				memcpy(addr, sta->mld_info.links[hapd->mld_link_id].peer_addr, ETH_ALEN);
			else
				memcpy(addr, sta->addr, ETH_ALEN);

			if (ether_addr_equal(mac, addr))
				return peer_cfg;
		}
	}

	return NULL;
}


void
atf_cal_implicit_peers(struct atf_group *group, u32 per_peer_airtime)
{
	struct sta_info *sta;

	dl_list_for_each(sta, &group->implicit_peers, struct sta_info, atf_candidate_list)
		sta->atf_peer.calculated_airtime = per_peer_airtime;
}

void
atf_cal_explicit_peers(struct atf_group *group)
{
	struct sta_info *sta;
	struct atf_peer_config *cfg = NULL;

	if (dl_list_empty(&group->explicit_peers)) {
		return;
	}

	dl_list_for_each(sta, &group->explicit_peers, struct sta_info, atf_candidate_list)
	{
		cfg = sta->atf_peer.peer_cfg_ref;

		if (!cfg)
			continue;

		sta->atf_peer.calculated_airtime =
		    ((group->user_cfg_airtime * cfg->user_cfg_airtime) /
		     ATF_RADIO_DEFAULT_AIRTIME);
		group->calculated_airtime -= sta->atf_peer.calculated_airtime;
		group->total_explicit_airtime += sta->atf_peer.calculated_airtime;
	}
}

static void
atf_cal_bh_peers(struct atf_algo *algo, u32 per_peer_airtime)
{
	struct atf_bh_peer *bh;

	dl_list_for_each(bh, &algo->bh_peers, struct atf_bh_peer, list)
		bh->atf_peer.calculated_airtime = per_peer_airtime;
}

/*
 * atf_adjust_impl_airtime_for_ac - Reduce implicit-peer airtime budget by the
 * fraction consumed by AC-level ATF constraints.
 *
 * When a group has both implicit peers and per-AC airtime configured, the
 * AC weights "carve out" a portion of the group airtime for AC scheduling.
 * The remaining (non-AC) budget available to implicit peers is:
 *
 *   calculated_airtime *= (1000 - tot_ac_val) / 1000
 *
 * This is a no-op when no AC is configured (ac_bitmap == 0 or tot_ac_val == 0)
 * or when tot_ac_val would exceed the radio budget (sanity guard).
 */
static void
atf_adjust_impl_airtime_for_ac(struct atf_group *group)
{
	u32 tot_ac_val = 0;
	u8 ac;

	if (!group->num_impl_peers || !group->ac_bitmap)
		return;

	for (ac = 0; ac < WMM_AC_NUM; ac++) {
		if (ATF_AC_IS_BIT_SET(group->ac_bitmap, ac))
			tot_ac_val += group->ac_airtime[ac];
	}

	if (tot_ac_val == 0 || tot_ac_val > ATF_RADIO_DEFAULT_AIRTIME)
		return;

	group->calculated_airtime =
		((ATF_RADIO_DEFAULT_AIRTIME - tot_ac_val) *
		 group->calculated_airtime) /
		ATF_RADIO_DEFAULT_AIRTIME;
}

/*
 * atf_cal_ac_airtime_for_group - Compute per-peer per-AC airtime for a group.
 *
 * For each implicit peer in the group, derive the per-AC airtime from the
 * group AC percentage and the group calculated_airtime (residual after
 * explicit peers are accounted for).
 *
 * Formula (per configured AC):
 *   peer->calculated_ac_airtime[ac] =
 *       (group->ac_airtime[ac] * group->calculated_airtime)
 *       / ATF_RADIO_DEFAULT_AIRTIME
 *
 * Unconfigured ACs (bit not set in ac_bitmap) are left as zero, meaning no
 * per-AC ATF constraint is applied for those ACs.
 */
static void
atf_cal_ac_airtime_for_group(struct atf_group *group)
{
	struct sta_info *sta;
	u8 ac;

	if (!group->ac_bitmap)
		return;

	for (ac = 0; ac < WMM_AC_NUM; ac++) {
		if (ATF_AC_IS_BIT_SET(group->ac_bitmap, ac))
			group->calculated_ac_airtime[ac] = group->ac_airtime[ac];
		else
			group->calculated_ac_airtime[ac] = 0;
	}

	dl_list_for_each(sta, &group->implicit_peers, struct sta_info,
			 atf_candidate_list) {
		for (ac = 0; ac < WMM_AC_NUM; ac++) {
			if (ATF_AC_IS_BIT_SET(group->ac_bitmap, ac))
				sta->atf_peer.calculated_ac_airtime[ac] =
					(group->ac_airtime[ac] *
					 group->calculated_airtime) /
					ATF_RADIO_DEFAULT_AIRTIME;
			else
				sta->atf_peer.calculated_ac_airtime[ac] = 0;
		}
	}
}

int
atf_distribute_airtime(struct hostapd_iface *iface)
{
	struct atf_algo *algo = iface->atf_algo;
	struct atf_group *group, *def_group = NULL;
	u16 iface_airtime = ATF_RADIO_DEFAULT_AIRTIME;
	u32 per_peer_airtime;

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: group list is empty");
		return -1;
	}

	dl_list_for_each(group, &algo->groups, struct atf_group, list)
	{
		if (!group->is_configured)
			continue;

		/* consider user configured group first and
		 * residual airtime will be used by default-group.
		 */
		if (os_strncmp(group->name, "default-group",
			       strlen("default-group")) == 0) {
			def_group = group;
		} else {
			iface_airtime = iface_airtime - group->user_cfg_airtime;
			group->calculated_airtime = group->user_cfg_airtime;
			atf_cal_explicit_peers(group);
			atf_cal_ac_airtime_for_group(group);
			atf_adjust_impl_airtime_for_ac(group);
			atf_cal_implicit_peers(group,
				group->num_impl_peers ?
				group->calculated_airtime / group->num_impl_peers : 0);
		}
	}

	/*now calculate for the default group*/
	group = def_group;
	if (!group) {
		wpa_printf(MSG_ERROR, "ATF: default-group missing");
		return -1;
	}

	group->user_cfg_airtime = iface_airtime;
	group->calculated_airtime = iface_airtime;
	atf_cal_ac_airtime_for_group(group);
	per_peer_airtime = (group->num_impl_peers + group->num_bh_peers) ?
		group->calculated_airtime / (group->num_impl_peers + group->num_bh_peers) : 0;
	atf_cal_implicit_peers(group, per_peer_airtime);
	atf_cal_bh_peers(algo, per_peer_airtime);
	return 0;
}

int
atf_offload_build_peer_config(struct hostapd_iface *iface,
                              struct atf_peer_params *peer_param)
{
	int i;
	struct atf_peer_info *peer_info;
	u16 num_peers = 0;
	struct atf_group *group = NULL;
	struct atf_algo *algo = NULL;
	struct hostapd_data *hapd;
	struct sta_info *sta;
	struct atf_bh_peer *bh;

	if (!iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: algo is null");
		return -1;
	}
	algo = iface->atf_algo;

	if (algo->no_of_peers == 0) {
		wpa_printf(MSG_ERROR, "ATF: There is no peer details to be sent to "
		                      "driver. Skip build config.");
		return 0;
	}

	peer_info = os_zalloc(algo->no_of_peers * sizeof(struct atf_peer_info));
	if (!peer_info) {
		wpa_printf(MSG_ERROR, "ATF: could not allocate peer info");
		return -1;
	}

	for (i = 0; i < iface->num_bss; i++) {
		hapd = iface->bss[i];
		if (!hapd->started)
			continue;

		for (sta = hapd->sta_list; sta; sta = sta->next) {
			if (sta && ap_sta_is_authorized(sta)) {

				group = sta->atf_peer.group;

				if (ATF_OFFLOAD_IS_LEAVE_UPDATE(algo) ||
				    (ATF_OFFLOAD_IS_JOIN_UPDATE(algo) &&
				     !ATF_IS_STA_UPDATED(sta->atf_peer)))
					continue;

				ATF_CLEAR_STA_UPDATED(sta->atf_peer);
				if (ap_sta_is_mld(hapd, sta)) {
					wpa_printf(MSG_DEBUG, "ATF: Getting addrress from link %d " MACSTR " sta addr",
						   hapd->mld_link_id, MAC2STR(sta->mld_info.links[hapd->mld_link_id].peer_addr));
					memcpy(peer_info[num_peers].peer_macaddr, sta->mld_info.links[hapd->mld_link_id].peer_addr, 6);
				} else {
					memcpy(peer_info[num_peers].peer_macaddr, sta->addr, 6);
				}

				sta->atf_peer.bss = hapd;
				peer_info[num_peers].percentage_peer =
				    sta->atf_peer.calculated_airtime;
				peer_info[num_peers].group_index = group->index;

				if (sta->atf_peer.atf_configured)
					peer_info[num_peers].explicit_peer_flag = 1;

				wpa_printf(MSG_DEBUG, "ATF: build peer " MACSTR " airtime %d group id %d %d",
					   MAC2STR(peer_info[num_peers].peer_macaddr), sta->atf_peer.calculated_airtime, group->index,
					   peer_info[num_peers].explicit_peer_flag);
				if (group->ac_bitmap)
					wpa_printf(MSG_DEBUG,
						   "ATF: build peer " MACSTR " AC airtime BE:%u BK:%u VI:%u VO:%u",
						   MAC2STR(peer_info[num_peers].peer_macaddr),
						   sta->atf_peer.calculated_ac_airtime[WMM_AC_BE],
						   sta->atf_peer.calculated_ac_airtime[WMM_AC_BK],
						   sta->atf_peer.calculated_ac_airtime[WMM_AC_VI],
						   sta->atf_peer.calculated_ac_airtime[WMM_AC_VO]);
				num_peers++;
			}
		}
	}

	dl_list_for_each(bh, &algo->bh_peers, struct atf_bh_peer, list) {
		group = bh->atf_peer.group;
		if (!group)
			continue;

		if (ATF_OFFLOAD_IS_LEAVE_UPDATE(algo) ||
		    (ATF_OFFLOAD_IS_JOIN_UPDATE(algo) &&
		     !ATF_IS_STA_UPDATED(bh->atf_peer)))
			continue;

		if (num_peers >= algo->no_of_peers) {
			wpa_printf(MSG_ERROR, "ATF: bh_peer count exceeds no_of_peers");
			goto err_cleanup;
		}

		ATF_CLEAR_STA_UPDATED(bh->atf_peer);
		os_memcpy(peer_info[num_peers].peer_macaddr, bh->addr, ETH_ALEN);
		peer_info[num_peers].percentage_peer = bh->atf_peer.calculated_airtime;
		peer_info[num_peers].group_index = group->index;

		wpa_printf(MSG_INFO, "ATF: build bh_peer " MACSTR " airtime %d group id %d",
			   MAC2STR(bh->addr), bh->atf_peer.calculated_airtime,
			   group->index);
		num_peers++;
	}

	if (ATF_OFFLOAD_IS_FULL_UPDATE(algo) && algo->no_of_peers != num_peers) {
		wpa_printf(MSG_ERROR,
		           "ATF: Mismatched: Packed peers %d and no_of_peers %d",
		           num_peers, algo->no_of_peers);
		goto err_cleanup;
	}

	peer_param->full_update_flag = ATF_OFFLOAD_IS_FULL_UPDATE(algo);
	peer_param->num_peers = num_peers;
	peer_param->peer_info = peer_info;
	wpa_printf(MSG_INFO, "ATF: build peer with %d peers and flags %d ", num_peers, peer_param->full_update_flag);

	return 0;

err_cleanup:
	os_free(peer_info);
	peer_info = NULL;
	return -1;
}

int
atf_offload_build_group_config(struct hostapd_iface *iface,
                               struct atf_group_params *group_param)
{
	struct atf_group_param_info *group_info = NULL;
	struct atf_algo *algo;
	struct atf_group *group;
	u8 i;

	if (!iface->atf_algo)
		return -1;

	algo = iface->atf_algo;

	/* Fill Group Configurations */
	if (algo->num_group_cfg > ATF_MAX_SSID_GROUP) {
		wpa_printf(MSG_ERROR, "ATF: invalid num of Configured group %d!",
		           algo->num_group_cfg);
		return -1;
	}

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: group list is empty");
		return -1;
	}

	group_info = os_zalloc(algo->num_group_cfg * sizeof(struct atf_group_param_info));
	if (!group_info) {
		wpa_printf(MSG_ERROR, "ATF: could not  allocate group info");
		return -1;
	}

	i = 0;
	dl_list_for_each(group, &algo->groups, struct atf_group, list)
	{
		if (!group->is_configured)
			continue;

		group_info[i].group_index  = group->index;
		group_info[i].group_airtime = group->user_cfg_airtime;
		group_info[i].total_implicit_peers = group->num_impl_peers;
		group_info[i].total_explicit_peers = group->num_expl_peers;
		group_info[i].group_policy = group->sched_policy;

		/* Calculate total implicit peer units by removing total
		 * explicit peer units from Group units.
		 */
		if (group->user_cfg_airtime >= group->total_explicit_airtime)
			group_info[i].total_implicit_peer_units =
				    (group->user_cfg_airtime -
				     group->total_explicit_airtime);

		wpa_printf(MSG_INFO,
			   "ATF: Group id:%d airtime:%u  scheduling policy:%u "
			    "unconfigured peers:%d configured_peers:%d "
			    "implicit_peer_units:%d\n",
			    group_info[i].group_index, group_info[i].group_airtime,
			    group_info[i].group_policy,
			    group_info[i].total_implicit_peers,
			    group_info[i].total_explicit_peers,
			    group_info[i].total_implicit_peer_units);
		i++;
	}
	group_param->group_info = group_info;
	group_param->num_groups = i;

	return 0;
}


int
nl80211_atf_offload_stats_enable_disable(void *priv, u8 radio_index, u8 value)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	struct nlattr *data;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX, radio_index) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_ENABLED, value))
		goto fail;

	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: ATF stats enable/disable failed: %s",
			   strerror(-ret));

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}


void atf_offload_disable_atf_stats(struct hostapd_iface *iface)
{
	struct wpa_driver_nl80211_data *drv_priv;
	int ret, radio_index;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: algo is missing\n");
		return;
	}

	drv_priv = iface->bss[0]->drv_priv;
	radio_index = atf_get_hw_idx(iface);

	if (!iface->atf_algo->atf_stats_enabled)
		return;

	ret = nl80211_atf_offload_stats_enable_disable(drv_priv,
						       radio_index,
						       0);
	if (ret)
		wpa_printf(MSG_ERROR, "ATF: Failed to disable ATF stats\n");
}


int
nl80211_atf_offload_stats_timeout(void *priv, u8 radio_index, u8 value)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	struct nlattr *data;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX, radio_index) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_TIMEOUT, value))
		goto fail;

	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: ATF stats timeout setting failed: %s",
			   strerror(-ret));

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}

static int atf_stats_cb(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct nlattr *vdata[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_MAX + 1];
	struct hostapd_data *hapd = arg;
	struct hostapd_iface *iface = hapd->iface;
	struct atf_algo *algo;
	struct atf_airtime_consumption *airtime_stats;
	struct atf_peer_config *peer_config = NULL;
	struct atf_peer *implicit_peer;
	struct nlattr *peer;
	int rem;
	u8 *mac;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	algo = iface->atf_algo;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[NL80211_ATTR_VENDOR_DATA])
		return NL_SKIP;

	nla_parse_nested(vdata, QCA_WLAN_VENDOR_ATTR_MAX,
			 tb[NL80211_ATTR_VENDOR_DATA], NULL);

	// Parse radio-level stats
	if (!vdata[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS]) {
		wpa_printf(MSG_ERROR, "ATF: ATF stats not present in NL data\n");
		return NL_SKIP;
	}

	nla_parse_nested(radio_stats, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS_MAX,
			 vdata[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STATS], NULL);

	airtime_stats = &algo->radio_airtime;
	airtime_stats->tx_consumption[0].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BE_AIRTIME]);
	airtime_stats->tx_consumption[1].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_BK_AIRTIME]);
	airtime_stats->tx_consumption[2].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VI_AIRTIME]);
	airtime_stats->tx_consumption[3].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_TX_VO_AIRTIME]);
	airtime_stats->rx_consumption[0].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BE_AIRTIME]);
	airtime_stats->rx_consumption[1].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_BK_AIRTIME]);
	airtime_stats->rx_consumption[2].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VI_AIRTIME]);
	airtime_stats->rx_consumption[3].consumption =
		nla_get_u32(radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_RX_VO_AIRTIME]);

	// Parse peer-level stats
	if (!radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS]) {
		wpa_printf(MSG_ERROR, "ATF: Peer stats not present in NL data\n");
		return NL_SKIP;
	}

	nla_for_each_nested(peer, radio_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS], rem) {
		struct nlattr *peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAX + 1];
		nla_parse_nested(peer_stats, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAX,
				 peer, NULL);

		if (!peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAC])
			continue;

		mac = nla_data(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_STATS_MAC]);

		peer_config = atf_find_peer_config_by_mac(mac, algo);
		if (!peer_config)
			implicit_peer = atf_implicit_peer_cfg(mac, hapd);

		if (!peer_config && !implicit_peer)
			continue;

		airtime_stats = peer_config ? &peer_config->peer_airtime : &implicit_peer->peer_airtime;

		airtime_stats->tx_consumption[0].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BE_AIRTIME]);
		airtime_stats->tx_consumption[1].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_BK_AIRTIME]);
		airtime_stats->tx_consumption[2].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VI_AIRTIME]);
		airtime_stats->tx_consumption[3].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_TX_VO_AIRTIME]);
		airtime_stats->rx_consumption[0].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BE_AIRTIME]);
		airtime_stats->rx_consumption[1].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_BK_AIRTIME]);
		airtime_stats->rx_consumption[2].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VI_AIRTIME]);
		airtime_stats->rx_consumption[3].consumption =
			nla_get_u32(peer_stats[QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_RX_VO_AIRTIME]);
	}

	return NL_OK;
}

int nl80211_atf_offload_showatfstats(void *priv, u8 radio_index, struct hostapd_data *hapd)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data;

	msg = nl80211_bss_msg(bss, NLM_F_DUMP, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
			nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
				QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;
	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX, radio_index))
		goto fail;

	nla_nest_end(msg, data);
	return send_and_recv_resp(drv, msg, atf_stats_cb, hapd);

fail:
	nlmsg_free(msg);
	return -1;
}

int
atf_offload_build_wmm_ac_config(struct hostapd_iface *iface,
                                struct atf_group_wmm_ac_params *wmm_ac_param)
{
	struct atf_group_wmm_ac_config *wmm_ac_cfg;
	struct atf_algo *algo = iface->atf_algo;
	struct atf_group *group;
	u8 i;

	if (!algo) {
		wpa_printf(MSG_ERROR, "ATF: Missing atf algo\n");
		return -1;
	}

	if (dl_list_empty(&algo->groups)) {
		wpa_printf(MSG_ERROR, "ATF: %s group list is empty", __func__);
		return -1;
	}

	wmm_ac_cfg = os_zalloc(algo->num_group_cfg * sizeof(*wmm_ac_cfg));
	if (!wmm_ac_cfg) {
		wpa_printf(MSG_ERROR, "ATF: could not allocate wmm_ac_config");
		return -1;
	}

	wmm_ac_param->num_groups = algo->num_group_cfg;

	dl_list_for_each(group, &algo->groups, struct atf_group, list) {
		i = group->index;
		wmm_ac_cfg[i].ac_be = ATF_AC_IS_BIT_SET(group->ac_bitmap, WMM_AC_BE) ? group->ac_airtime[WMM_AC_BE] : 0;
		wmm_ac_cfg[i].ac_bk = ATF_AC_IS_BIT_SET(group->ac_bitmap, WMM_AC_BK) ? group->ac_airtime[WMM_AC_BK] : 0;
		wmm_ac_cfg[i].ac_vi = ATF_AC_IS_BIT_SET(group->ac_bitmap, WMM_AC_VI) ? group->ac_airtime[WMM_AC_VI] : 0;
		wmm_ac_cfg[i].ac_vo = ATF_AC_IS_BIT_SET(group->ac_bitmap, WMM_AC_VO) ? group->ac_airtime[WMM_AC_VO] : 0;
		wpa_printf(MSG_DEBUG,
			   "ATF: WMM AC config group %s index %u BE:%u BK:%u VI:%u VO:%u",
			   group->name, i,
			   wmm_ac_cfg[i].ac_be, wmm_ac_cfg[i].ac_bk,
			   wmm_ac_cfg[i].ac_vi, wmm_ac_cfg[i].ac_vo);
	}

	wmm_ac_param->wmm_ac_cfg = wmm_ac_cfg;
	return 0;
}


int
nl80211_atf_offload_ssid_sched_policy(void *priv, u8 radio_index, u8 value, int link_id)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data, *ssid_sched_policy_attr;
	int ret;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX, radio_index))
		goto fail;

	ssid_sched_policy_attr =
		    nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_POLICY);
	if (!ssid_sched_policy_attr)
		goto fail;

	if (link_id != -1 && link_id >= 0 && link_id < MAX_NUM_MLD_LINKS &&
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHED_LINK_ID, link_id))
		goto fail;

	if (nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_SCHEDULING, value))
		goto fail;

	nla_nest_end(msg, ssid_sched_policy_attr);
	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_DEBUG, "nl80211: ATF SSID sched policy config failed: %s",
			   strerror(-ret));

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}


int
nl80211_atf_offload_strict_scheduling_enable_disable(void *priv, u8 radio_index, u8 value)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	struct nlattr *data;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX,
		       radio_index) ||
	    nla_put_u8(msg,
		       QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_STRICT_SCHEDULING_ENABLED,
		       value))
		goto fail;

	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR, "nl80211: ATF strict scheduling enable/disable failed: %s",
			   strerror(-ret));

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}


int
nl80211_atf_offload_enable_disable(void *priv, u8 radio_index, u8 value)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	int ret;
	struct nlattr *data;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX,
		       radio_index) ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_ENABLED,
		       value))
		goto fail;

	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR, "ATF: ATF offload enable/disable failed %s",
			   strerror(-ret));

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}


int
nl80211_atf_offload_send_group_config(void *priv, u8 radio_index, struct atf_group_params *param)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data, *groups_data, *group_data;
	int ret, i;

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg)
		return -ENOBUFS;

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX,
		       radio_index))
		goto fail;

	groups_data = nla_nest_start(msg,
				     QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIG);
	if (!groups_data)
		goto fail;

	if (!param->group_info) {
		wpa_printf(MSG_ERROR, "ATF: Group info is NULL");
		goto fail;
	}

	for (i = 0; i < param->num_groups; i++) {
		group_data = nla_nest_start(msg, i);
		if (!group_data)
			goto fail;

		if (nla_put_u8(msg,
			       QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_INDEX,
			       param->group_info[i].group_index) ||
		    nla_put_u16(msg,
				QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_AIRTIME_CONFIGURED,
				param->group_info[i].group_airtime) ||
		    nla_put_u8(msg,
			       QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_POLICY,
			       param->group_info[i].group_policy) ||
		    nla_put_u16(msg,
				QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS,
				param->group_info[i].total_implicit_peers) ||
		    nla_put_u16(msg,
				QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_CONFIGURED_PEERS,
				param->group_info[i].total_explicit_peers) ||
		    nla_put_u16(msg,
				QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_SSID_GROUP_UNCONFIGURED_PEERS_AIRTIME,
				param->group_info[i].total_implicit_peer_units))
			goto fail;
		nla_nest_end(msg, group_data);
	}
	nla_nest_end(msg, groups_data);
	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR, "nl80211: ATF group config send failed: %s",
			   strerror(-ret));

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}


int
nl80211_atf_offload_send_wmm_ac_config(void *priv, u8 radio_index,
				       struct atf_group_wmm_ac_params *param)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data;
	struct build_header *build_param;
	struct atf_group_wmm_ac_info *group_info;
	int ret, i;
	u8 *ptr;
	size_t total_len;
	uint8_t *buffer;

	total_len = HDR_SIZE +
			   (param->num_groups * sizeof(struct atf_group_wmm_ac_info));
	buffer = os_zalloc(total_len);
	if (!buffer)
		return -ENOMEM;

	ptr = buffer;

	build_param = (struct build_header *)ptr;
	build_param->header = PREP(TAG_ARRAY_STRUCT, (param->num_groups *
				   sizeof(struct atf_group_wmm_ac_info)));
	ptr += sizeof(build_param->header);

	group_info = (struct atf_group_wmm_ac_info *)ptr;

	for (i = 0; i < param->num_groups; i++) {
		group_info->header = PREP(TAG_ATF_GROUP_WMM_AC_INFO,
					  sizeof(*group_info) - HDR_SIZE);
		group_info->atf_group_id = i;
		group_info->atf_units_be = param->wmm_ac_cfg[i].ac_be;
		group_info->atf_units_bk = param->wmm_ac_cfg[i].ac_bk;
		group_info->atf_units_vi = param->wmm_ac_cfg[i].ac_vi;
		group_info->atf_units_vo = param->wmm_ac_cfg[i].ac_vo;
		group_info++;
	}

	msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
	if (!msg) {
		os_free(buffer);
		return -ENOBUFS;
	}

	if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
		goto fail;

	data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
	if (!data ||
	    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX, radio_index) ||
	    nla_put(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_WMM_AC_CONFIG, total_len, buffer))
		goto fail;

	nla_nest_end(msg, data);

	ret = send_and_recv_cmd(drv, msg);
	if (ret)
		wpa_printf(MSG_ERROR, "nl80211: ATF WMM AC config send failed: %s", strerror(-ret));

	os_free(buffer);
	return ret;
fail:
	os_free(buffer);
	nlmsg_free(msg);
	return -1;
}


int
nl80211_atf_offload_send_peer_config(void *priv, u8 radio_index, struct atf_peer_params *param)
{
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	struct nl_msg *msg;
	struct nlattr *data, *peers_data, *peer_data, *peers;
	struct atf_peer_info *param_peer_info;
	int ret = 0, i;
	u16 rem_peers = param->num_peers;
	u16 num_entry = ATF_NUM_PEERS_DATA_PER_MSG;

	param_peer_info = param->peer_info;

	do {
		u16 encoded_peers = MIN(rem_peers, num_entry);

		rem_peers -= encoded_peers;

		msg = nl80211_bss_msg(bss, 0, NL80211_CMD_VENDOR);
		if (!msg)
			return -ENOBUFS;

		if (nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
		    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
				QCA_NL80211_VENDOR_SUBCMD_ATF_OFFLOAD_OPS))
			goto fail;

		data = nla_nest_start(msg, NL80211_ATTR_VENDOR_DATA);
		if (!data ||
		    nla_put_u8(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_RADIO_INDEX,
			       radio_index))
			goto fail;

		peers_data = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG);
		if (!peers_data)
			goto fail;

		if (param->full_update_flag)
			if (nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_FULL_UPDATE))
				goto fail;

		if (rem_peers)
			if (nla_put_flag(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_MORE))
				goto fail;

		peers = nla_nest_start(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIG_PAYLOAD);
		if (!peers)
			goto fail;

		for (i = 0; i < encoded_peers; i++) {
			peer_data = nla_nest_start(msg, i);
			if (!peer_data)
				goto fail;

			if (nla_put(msg, QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_MAC,
				    ETH_ALEN, param_peer_info->peer_macaddr) ||
			    nla_put_u16(msg,
					QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_AIRTIME,
					param_peer_info->percentage_peer) ||
			    nla_put_u8(msg,
				       QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_GROUP_INDEX,
				       param_peer_info->group_index))
				goto fail;

			if (param_peer_info->explicit_peer_flag)
				if (nla_put_flag(msg,
						 QCA_WLAN_VENDOR_ATTR_ATF_OFFLOAD_PEER_CONFIGURED))
					goto fail;

			nla_nest_end(msg, peer_data);
			param_peer_info++;
		}
		nla_nest_end(msg, peers);
		nla_nest_end(msg, peers_data);
		nla_nest_end(msg, data);

		ret = send_and_recv_cmd(drv, msg);
		if (ret) {
			wpa_printf(MSG_ERROR, "nl80211: ATF peer config send failed: %s",
				   strerror(-ret));
			return ret;
		}

	} while (rem_peers > 0);

	return ret;
fail:
	nlmsg_free(msg);
	return -1;
}


int
atf_send_calculated_airtime(struct hostapd_iface *iface)
{

	struct atf_peer_params atf_peer_param = {0};
	struct atf_group_params atf_group_param = {0};
	struct atf_group_wmm_ac_params atf_group_ac = {0};
	struct hostapd_data *hapd = iface->bss[0];
	int ret = -1;
	u8 radio_idx;

	if (!iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: iface does not have atf_algo assigned");
		return -1;
	}

	if (!hapd->drv_priv) {
		wpa_printf(MSG_ERROR, "ATF: Invalid hapd data\n");
		goto group_info_free;
	}

	ret = atf_offload_build_peer_config(iface, &atf_peer_param);
	if (ret != 0)
		return ret;

	/* Fill WMM AC configurations per group only in full Update mode */
	if (ATF_OFFLOAD_IS_FULL_UPDATE(iface->atf_algo)) {
		ret = atf_offload_build_wmm_ac_config(iface, &atf_group_ac);
		if (ret != 0) {
			goto peer_param_free;
		}
	}

	ret = atf_offload_build_group_config(iface, &atf_group_param);
	if (ret != 0) {
		goto group_ac_free;
	}

	radio_idx = atf_get_hw_idx(iface);

	ret = nl80211_atf_offload_send_group_config(hapd->drv_priv, radio_idx, &atf_group_param);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: sending group config failed");
		goto group_info_free;
	}

	if (ATF_OFFLOAD_IS_FULL_UPDATE(iface->atf_algo)) {
		ret = nl80211_atf_offload_send_wmm_ac_config(hapd->drv_priv,
							     radio_idx, &atf_group_ac);
		if (ret) {
			wpa_printf(MSG_ERROR, "ATF: sending group wmm ac config failed");
			goto group_info_free;
		}
	}

	ret = nl80211_atf_offload_send_peer_config(hapd->drv_priv, radio_idx, &atf_peer_param);
	if (ret) {
		wpa_printf(MSG_ERROR, "ATF: sending peer config failed");
		goto group_info_free;
	}

	ATF_OFFLOAD_SET_NO_UPDATE(iface->atf_algo);

group_info_free:
	if (atf_group_param.group_info)
		os_free(atf_group_param.group_info);

group_ac_free:
	if (atf_group_ac.wmm_ac_cfg)
		os_free(atf_group_ac.wmm_ac_cfg);

peer_param_free:
	if (atf_peer_param.peer_info)
		os_free(atf_peer_param.peer_info);

	return ret;
}

static void
atf_cfg_timeout_handler(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = (struct hostapd_iface *)eloop_ctx;
	struct atf_algo *algo;

	if (!iface || !iface->atf_algo) {
		wpa_printf(MSG_ERROR, "ATF: Something is wrong, check iface");
		return;
	}

	algo = iface->atf_algo;

	if (!algo->atf_enabled) {
		wpa_printf(MSG_ERROR, "ATF not enabled. skip distribution");
		goto out;
	}

	wpa_printf(MSG_DEBUG, "ATF: Hitting the timeout handler for iface %p", iface);

	atf_sync_bhsta_bh_peers(iface);

	if (!hostapd_iface_num_sta(iface) && dl_list_empty(&algo->bh_peers)) {
		wpa_printf(MSG_INFO,
			   "ATF: There is no peer associated in this iface, Skip distribution");
		goto out;
	}

	if (atf_build_candidate_list(iface) != 0) {
		wpa_printf(MSG_ERROR, "ATF: couldnt build the candidate list");
		goto out;
	}

	if (atf_distribute_airtime(iface) != 0) {
		wpa_printf(MSG_ERROR, "ATF: could not distribute airtime");
		goto out;
	}

	if (atf_send_calculated_airtime(iface) != 0)
		goto out;

out:
	algo->atf_tasksched = 0;
	return;
}


int
atf_timer_start(struct hostapd_iface *iface)
{
	wpa_printf(MSG_ERROR, "ATF: Trigger algo for iface %p", iface);
	eloop_register_timeout(ATF_ALGO_TIMEOUT, 0, atf_cfg_timeout_handler, iface, NULL);
	return 0;
}


void
atf_timer_stop(struct hostapd_iface *iface)
{
	wpa_printf(MSG_DEBUG, "ATF: cancelling timeout");
	if (!iface->atf_algo)
		return;

	iface->atf_algo->atf_tasksched = 0;
	eloop_cancel_timeout(atf_cfg_timeout_handler, iface, NULL);
}


void
atf_init_algo(struct hostapd_iface *iface)
{
	struct atf_algo *algo;
	struct atf_group *group;

	if (!iface->conf->atf_offload)
		return;

	algo = atf_get_algo_entry(iface);
	if (!algo) {
		wpa_printf(MSG_ERROR, "ATF: %s: atf algo entry is not found", __func__);
		return;
	}

	iface->atf_algo = algo;
	algo->iface = iface;
	algo->atf_enabled = iface->conf->commitatf;
	algo->ssid_group_enabled = iface->conf->atf_ssid_grp;
	algo->atfstrictsched_enabled = iface->conf->atf_strict_sched;

	if (iface->conf->atf_offload_config)
		if (atf_read_config(algo, iface->conf->atf_offload_config))
			wpa_printf(MSG_DEBUG, "ATF: could not read %s, expect issues",
			           iface->conf->atf_offload_config);

	wpa_printf(MSG_DEBUG, "ATF: allocating default group for %p", algo);
	group = atf_allocate_group("default-group", algo);
	if (!group) {
		wpa_printf(MSG_ERROR, "ATF: Could not allocate default group");
		return;
	}
	/* Default group is always configured */
	group->is_configured = 1;
}


void
atf_deinit_algo(struct hostapd_iface *iface)
{
	if (!iface->atf_algo)
		return;

	atf_cleanup_algo(iface->atf_algo);
}


void
atf_offload_deinit(void)
{
	struct atf_algo *algo, *tmp;

	if (!atf)
		return;

	wpa_printf(MSG_DEBUG, "%s : ATF Service stopping", __func__);

	if (dl_list_empty(&atf->algo_list))
		return;

	dl_list_for_each_safe(algo, tmp, &atf->algo_list, struct atf_algo, list)
	{
		atf_cleanup_algo(algo);
	}

	dl_list_init(&atf->algo_list);

	atf = NULL;
}


void
atf_offload_init(struct hapd_interfaces *ifaces)
{
	if (!ifaces)
		return;

	if (atf) {
		wpa_printf(MSG_INFO, "%s : ATF already enabled", __func__);
		return;
	}

	wpa_printf(MSG_DEBUG, "%s : Enabling ATF offload feature", __func__);
	atf = &ifaces->atf;
	dl_list_init(&atf->algo_list);

	return;
}
