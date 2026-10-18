/*
 * SMD (Seamless Mobility Domain) Implementation
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/crc32.h"
#include "utils/eloop.h"
#include "common/wpa_common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/wpa_i.h"
#include "crypto/crypto.h"
#include "crypto/sha256.h"
#include "utils/wpabuf.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "bss.h"
#include "smd.h"
#include "wnm_sta.h"
#include "scan.h"
#include "driver_i.h"
#include "notify.h"


/* Forward declarations for ST Preparation Response handling */
struct smd_bss_transition_work {
	u8 bssid[ETH_ALEN];
	bool is_exec;		/* false = ST Preparation, true = ST Execution */

	/* ST Preparation parameters */
	int no_dl_sn;
	int no_ul_sn;
	u8 exec_path;		/* stored for preferred-target marking */
	bool has_reconfig;
	struct wpa_mlo_reconfig_info reconfig_info;

	/* ST Execution parameters */
	u8 dl_tid_bitmap;
};

static void smd_handle_prepare_response(struct wpa_supplicant *wpa_s,
					const u8 *frame, size_t frame_len,
					u16 status_code,
					enum nl80211_smd_link_transition_state
					transition_state);
static void smd_st_roam_execute_work(void *eloop_ctx, void *timeout_ctx);
static void smd_handle_execute_response(struct wpa_supplicant *wpa_s,
					const u8 *frame, size_t frame_len,
					u16 status_code,
					enum nl80211_smd_link_transition_state
					transition_state);
void smd_handle_uhr_reconfig_response(struct wpa_supplicant *wpa_s,
				      u8 type,
				      const u8 *frame, size_t frame_len,
				      u16 status_code,
				      enum nl80211_smd_link_transition_state 
				      transition_state);

int smd_install_target_ptk(struct wpa_supplicant *wpa_s,
			   struct wpa_smd_prepared_target *target);

static int smd_start_execution_timeout(struct wpa_supplicant *wpa_s,
				       struct wpa_smd_prepared_target *target);
static void smd_cancel_execution_timeout(struct wpa_supplicant *wpa_s,
					 struct wpa_smd_prepared_target *target);
static void smd_cancel_drain_watchdog(struct wpa_supplicant *wpa_s,
				      struct wpa_smd_prepared_target *target);
static int smd_start_drain_watchdog(struct wpa_supplicant *wpa_s,
				    struct wpa_smd_prepared_target *target);

int smd_enabled(struct wpa_supplicant *wpa_s)
{
	return wpa_s && wpa_s->smd_capable;
}

int smd_set_domain_id(struct wpa_supplicant *wpa_s, const u8 *domain_id)
{
	if (!wpa_s || !domain_id)
		return -1;

	os_memcpy(wpa_s->smd_id, domain_id, SMD_DOMAIN_ID_LEN);
	wpa_printf(MSG_DEBUG, "SMD: Set domain ID " MACSTR, MAC2STR(domain_id));

	return 0;
}

const u8 *smd_get_domain_id(struct wpa_supplicant *wpa_s)
{
	if (!smd_enabled(wpa_s))
		return NULL;

	return wpa_s->smd_id;
}

const char *smd_state_txt(enum smd_state state)
{
	switch (state) {
	case SMD_STATE_DISABLED:
		return "DISABLED";
	case SMD_STATE_IDLE:
		return "IDLE";
	case SMD_STATE_DISCOVERING:
		return "DISCOVERING";
	case SMD_STATE_ASSOCIATING:
		return "ASSOCIATING";
	case SMD_STATE_ASSOCIATED:
		return "ASSOCIATED";
	case SMD_STATE_TRANSITIONING:
		return "TRANSITIONING";
	default:
		return "UNKNOWN";
	}
}

enum smd_state smd_get_state(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s)
		return SMD_STATE_DISABLED;

	return wpa_s->smd_state;
}

void smd_set_state(struct wpa_supplicant *wpa_s, enum smd_state new_state)
{
	enum smd_state old_state;

	if (!wpa_s)
		return;

	old_state = wpa_s->smd_state;

	if (old_state == new_state) {
		wpa_printf(MSG_DEBUG, "SMD: State unchanged: %s",
			   smd_state_txt(new_state));
		return;
	}

	wpa_printf(MSG_INFO, "SMD: State Transition: %s -> %s",
		   smd_state_txt(old_state), smd_state_txt(new_state));

	wpa_s->smd_state = new_state;
}

const u8 *smd_get_current_domain(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s || !wpa_s->smd_me_associated) {
		wpa_printf(MSG_WARNING, "SMD-ME Association not yet done, un-expected");
		return NULL;
	}

	return wpa_s->smd_id;
}

bool smd_detect_domain_transition(struct wpa_supplicant *wpa_s,
				  const u8 *target_smd_id)
{
	if (!wpa_s || !target_smd_id)
		return false;

	if (!wpa_s->smd_me_associated)
		return false;

	if (os_memcmp(wpa_s->smd_id, target_smd_id, ETH_ALEN) != 0) {
		wpa_printf(MSG_INFO, "SMD: Domain transition detected - current="
			    MACSTR " target=" MACSTR,
			   MAC2STR(wpa_s->smd_id), MAC2STR(target_smd_id));

		return true;
	}

	return false;
}

int smd_validate_domain_transition(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *target_bss,
				   const u8 *target_smd_id)
{
	if (!wpa_s || !target_bss || !target_smd_id) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameter for domain transition validation");
		return -1;
	}

	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_ERROR, "SMD: Cannot validate transition - SMD not enabled");
		return -1;
	}

	wpa_printf(MSG_INFO, "SMD: Validating domain transition from " MACSTR
		   " to " MACSTR, MAC2STR(wpa_s->smd_id), MAC2STR(target_smd_id));

	if (smd_validate_security_policy(wpa_s, target_bss) < 0) {
		wpa_printf(MSG_ERROR, "SMD: Target domain security policy validation failed");
		return -1;
	}

	if (smd_verify_uhr_capability(wpa_s, target_bss) < 0) {
		wpa_printf(MSG_ERROR, "SMD: Target domain UHR capability validation failed");
		return -1;
	}

	struct smd_group *target_group = smd_group_find(wpa_s, target_smd_id);

	if (target_group) {
		wpa_printf(MSG_DEBUG, "SMD: Target domain " MACSTR " is known (%zu) members",
			   MAC2STR(target_smd_id), target_group->member_count);

		if (target_group->ptk_mode_valid) {
			wpa_printf(MSG_DEBUG, "SMD: Target domain PTK mode: %d (Per-%s PTK)",
				   target_group->ptk_mode,
				   target_group->ptk_mode ? "AP MLD" : "SMD");
		}
	} else {
		wpa_printf(MSG_DEBUG, "SMD: Target domain " MACSTR " is new -will be discovered later",
			   MAC2STR(target_smd_id));
	}

	wpa_printf(MSG_INFO, "SMD: Domain transition validation passed - can transition to domain "
		    MACSTR,
		   MAC2STR(target_smd_id));

	return 0;
}

void smd_begin_domain_transition(struct wpa_supplicant *wpa_s,
				 const u8 *target_smd_id)
{
	if (!wpa_s || !target_smd_id)
		return;

	if (wpa_s->smd_me_associated) {
		os_memcpy(wpa_s->smd_previous_id, wpa_s->smd_id, ETH_ALEN);
		wpa_s->smd_in_transition = 1;

		wpa_printf(MSG_INFO, "SMD: Beginning domain transition - previous="
			    MACSTR " target=" MACSTR,
			   MAC2STR(wpa_s->smd_previous_id), MAC2STR(target_smd_id));
	} else {
		os_memset(wpa_s->smd_previous_id, 0, ETH_ALEN);
		wpa_s->smd_in_transition = 0;

		wpa_printf(MSG_INFO, "SMD: Initial domain association - target="
			    MACSTR, MAC2STR(target_smd_id));
	}
}

void smd_complete_domain_transition(struct wpa_supplicant *wpa_s, bool success)
{
	if (!wpa_s)
		return;

	if (!wpa_s->smd_in_transition) {
		wpa_printf(MSG_DEBUG, "SMD: No domain transition in progress");
		return;
	}

	if (success) {
		wpa_printf(MSG_INFO, "SMD: Domain transition completed successfully - previous="
			    MACSTR " new=" MACSTR,
			   MAC2STR(wpa_s->smd_previous_id), MAC2STR(wpa_s->smd_id));
	} else {
		/* Transition Failed - revert to previous domain if possible */
		if (!is_zero_ether_addr(wpa_s->smd_previous_id)) {
			wpa_printf(MSG_WARNING, "SMD: Domain transition FAILED - reverting to previous domain "
				    MACSTR,
				   MAC2STR(wpa_s->smd_previous_id));
			os_memcpy(wpa_s->smd_id, wpa_s->smd_previous_id, ETH_ALEN);
		} else {
			wpa_printf(MSG_WARNING, "SMD: Domain transition FAILED - no previous domain to revert to");
			wpa_s->smd_me_associated = 0;
		}
	}

	/* Note: SMD_KDK is managed as part of the current PTK - no separate cleanup needed */

	wpa_s->smd_in_transition = 0;
	os_memset(wpa_s->smd_previous_id, 0, ETH_ALEN);
}

int smd_verify_uhr_capability(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	if (!wpa_s || !bss) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for UHR capability verification");
		return -1;
	}

	wpa_printf(MSG_INFO, "SMD: UHR Validation -TBD");

	return 0;
}

int smd_needs_initial_association(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	if (!smd_enabled(wpa_s) || !bss || !bss->smd_capable) {
		wpa_printf(MSG_ERROR, "SMD: SMD-ME assoc may not be required for this BSS");
		return 0;
	}

	if (wpa_s->smd_me_associated &&
	    os_memcmp(wpa_s->smd_id, bss->smd_identifier, ETH_ALEN) == 0) {
		wpa_printf(MSG_DEBUG, "SMD: Already associated with SMD-ME " MACSTR,
			   MAC2STR(bss->smd_identifier));
		return 0;
	}

	if (wpa_s->smd_me_associated &&
	    os_memcmp(wpa_s->smd_id, bss->smd_identifier, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "SMD: Transitioning from SMD-ME " MACSTR
			   " to " MACSTR " -  need new association",
			   MAC2STR(wpa_s->smd_id), MAC2STR(bss->smd_identifier));
		return 1;
	}

	wpa_printf(MSG_DEBUG, "SMD: Initial association needed with SMD-ME " MACSTR,
		   MAC2STR(bss->smd_identifier));
	return 1;
}

int smd_establish_smd_me_association(struct wpa_supplicant *wpa_s,
				     struct wpa_bss *bss,
				     struct wpa_ssid *ssid)
{
	const u8 *ap_mld_addr;
	bool is_domain_transition = false;

	if (!smd_enabled(wpa_s) || !bss || !bss->smd_capable || !ssid) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for SMD-ME Association");
		return -1;
	}

	is_domain_transition = smd_detect_domain_transition(wpa_s, bss->smd_identifier);
	if (is_domain_transition) {
		if (smd_validate_domain_transition(wpa_s, bss,
						   bss->smd_identifier) < 0) {
			wpa_printf(MSG_ERROR, "SMD: Domain transition validation failed");
			return -1;
		}

		smd_begin_domain_transition(wpa_s, bss->smd_identifier);
	}

	smd_set_state(wpa_s, SMD_STATE_ASSOCIATING);

	wpa_printf(MSG_INFO, "SMD: Establising association with SMD-ME " MACSTR
		   " via initial AP MLD " MACSTR "%s",
		   MAC2STR(bss->smd_identifier), MAC2STR(bss->bssid),
		   is_domain_transition ? "(inter-domain transition)" : "");

	if (smd_verify_uhr_capability(wpa_s, bss) < 0) {
		wpa_printf(MSG_ERROR, "SMD: UHR capability verification failed - cannot establish SMD-ME Assocaition");
		smd_set_state(wpa_s, SMD_STATE_IDLE);

		if (is_domain_transition) {
			smd_complete_domain_transition(wpa_s, false);
		}
		return -1;
	}

	os_memcpy(wpa_s->smd_id, bss->smd_identifier, ETH_ALEN);

	ap_mld_addr = bss->bssid;
	os_memcpy(wpa_s->smd_me_initial_ap_mld_addr, ap_mld_addr, ETH_ALEN);

	wpa_printf(MSG_DEBUG, "SMD: Set initial AP MLD address to " MACSTR
		   " for Per-SMD PTK derivation context",
		   MAC2STR(wpa_s->smd_me_initial_ap_mld_addr));

	return 0;
}

int smd_needs_bss_transition(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	if (!smd_enabled(wpa_s) || !bss || !bss->smd_capable) {
		return 0;
	}

	if (!wpa_s->smd_me_associated) {
		return 0;
	}

	if (os_memcmp(wpa_s->smd_id, bss->smd_identifier, ETH_ALEN) != 0) {
		return 0;
	}

	wpa_printf(MSG_DEBUG, "SMD: BSS transition needed within SMD " MACSTR
		   " to AP " MACSTR,
		   MAC2STR(wpa_s->smd_id), MAC2STR(bss->bssid));
	return 1;
}

/*
 * SMD Neigbor Discovery Implementation
 */

int smd_parse_rnr_for_neighbors(struct wpa_supplicant *wpa_s,
				struct wpa_bss *bss,
				struct smd_neighbor_target **targets,
				size_t *num_targets)
{
	struct smd_neighbor_target *target_list = NULL;
	size_t target_count = 0;
	const u8 *pos, *end;
	const u8 *rnr_ie;

	if (!wpa_s || !bss || !targets || !num_targets) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for RNR parsing");
		return -1;
	}

	*targets = NULL;
	*num_targets = 0;

	rnr_ie = wpa_bss_get_ie(bss, WLAN_EID_REDUCED_NEIGHBOR_REPORT);
	if (!rnr_ie) {
		wpa_printf(MSG_DEBUG, "SMD: No RNR element in BSS " MACSTR,
			   MAC2STR(bss->bssid));
		return 0;
	}

	if (rnr_ie[1] < RNR_HEADER_LEN) {
		wpa_printf(MSG_DEBUG, "SMD: RNR Element too short: %u",
			   rnr_ie[1]);
		return 0;
	}

	pos = rnr_ie + 2;
	end = rnr_ie + 2 + rnr_ie[1];

	wpa_printf(MSG_DEBUG, "SMD: Parsing RNR from BSS " MACSTR
		   " (length=%u)", MAC2STR(bss->bssid), rnr_ie[1]);

	while (pos + RNR_TBTT_HEADER_LEN <= end) {
		u8 op_class = pos[0];
		u8 channel = pos[1];
		u8 tbtt_info_hdr = pos[2];
		u8 tbtt_info_len = pos[3];
		u8 tbtt_info_count = RNR_TBTT_INFO_COUNT_VAL(tbtt_info_hdr);
		u8 i;

		pos += RNR_TBTT_HEADER_LEN;

		wpa_printf(MSG_DEBUG, "SMD: RNR TBTT Set: op_class=%u channel=%u count=%u len=%u",
			   op_class, channel,
			   tbtt_info_count, tbtt_info_len);

		for (i = 0; i <= tbtt_info_count && pos + tbtt_info_len <= end; i++) {
			u8 neighbor_tbtt_offset;
			u32 short_ssid;
			u8 bss_params;
			struct smd_neighbor_target *new_list;

			if (tbtt_info_len < 6) {
				wpa_printf(MSG_DEBUG, "SMD: TBTT info too short: %u",
					   tbtt_info_len);
				pos += tbtt_info_len;
				continue;
			}

			neighbor_tbtt_offset = pos[0];
			short_ssid = WPA_GET_LE32(&pos[1]);
			bss_params = pos[5];

			if (!(bss_params & RNR_BSS_PARAM_MEMBER_OF_SMD)) {
				wpa_printf(MSG_DEBUG, "SMD: TBTT entry %u: "
					"bit 7 not set, skipping", i);
				pos +=  tbtt_info_len;
				continue;
			}

			wpa_printf(MSG_DEBUG, "SMD: Found SMD neighbor: "
				   "Short SSID=0x%08x op_class=%u channel=%u tbtt_offset=%u",
				   short_ssid, op_class, channel,
				   neighbor_tbtt_offset);

			new_list = os_realloc_array(target_list, target_count + 1,
						sizeof(struct smd_neighbor_target));
			if (!new_list) {
				wpa_printf(MSG_ERROR, "SMD: Failed to allocate "
					"target array");
				os_free(target_list);
				return -1;
			}

			target_list = new_list;
			os_memset(&target_list[target_count], 0,
				  sizeof(struct smd_neighbor_target));
			target_list[target_count].short_ssid = short_ssid;
			target_list[target_count].op_class = op_class;
			target_list[target_count].tbtt_offset = neighbor_tbtt_offset;
			target_list[target_count].same_smd = true;

			target_list[target_count].freq =
				ieee80211_chan_to_freq(NULL, op_class, channel);
			if (target_list[target_count].freq == 0) {
				wpa_printf(MSG_WARNING, "SMD: Could not convert channel %u op_class %u to frequency",
					   channel, op_class);
			}

			if (tbtt_info_len >= 11) {
				os_memcpy(target_list[target_count].bssid,
					  &pos[6], ETH_ALEN);
				wpa_printf(MSG_DEBUG, "SMD: BSSID=" MACSTR,
					   MAC2STR(target_list[target_count].bssid));
			}

			if (tbtt_info_len >= 16) {
				/* MLD Parameters at offset 12-13 (2 octets):
				* Bits 0-7: AP MLD ID */
				target_list[target_count].ap_mld_id = pos[12];
				wpa_printf(MSG_DEBUG, "SMD: AP MLD ID=%u",
					   target_list[target_count].ap_mld_id);
			}

			target_count++;
			pos += tbtt_info_len;
		}
	}

	if (target_count > 0) {
		wpa_printf(MSG_INFO, "SMD: Parsed %zu SMD neighbor(s) from RNR of "
			    MACSTR, target_count, MAC2STR(bss->bssid));
		*targets = target_list;
		*num_targets = target_count;
	} else {
		wpa_printf(MSG_DEBUG, "SMD: No SMD neigbhors found in RNR");
		os_free(target_list);
	}

	return 0;
}

int smd_trigger_neighbor_discovery(struct wpa_supplicant *wpa_s,
				   struct smd_neighbor_target *targets,
				   size_t num_targets)
{
	size_t i;

	if (!wpa_s || !targets || num_targets == 0) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for neighbor discovery trigger");
		return -1;
	}

	if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED) {
		wpa_printf(MSG_ERROR, "SMD: Cannot trigger discovery - interface disabled");
		return -1;
	}

	wpa_printf(MSG_INFO, "SMD: Triggerring discovery for %zu neighbor(s)",
		   num_targets);

	for (i = 0; i < num_targets; i++) {
		if (!is_zero_ether_addr(targets[i].bssid)) {
			wpa_printf(MSG_DEBUG, "SMD: Target %zu: Short SSID: 0x%0x8x freq=%u op_class=%u BSSID="
				     MACSTR,
				    i, targets[i].short_ssid, targets[i].freq,
				    targets[i].op_class, MAC2STR(targets[i].bssid));
		} else {
			wpa_printf(MSG_DEBUG, "SMD: Target %zu: Short SSID: 0x%0x8x freq=%u op_class=%u",
				   i, targets[i].short_ssid, targets[i].freq,
				    targets[i].op_class);
		}

		if (targets[i].ap_mld_id != 0) {
			wpa_printf(MSG_DEBUG, "SMD:  AP MLD ID=%u",
				   targets[i].ap_mld_id);
		}
	}

	if (!wpa_s->driver->trigger_smd_discovery) {
		wpa_printf(MSG_ERROR, "SMD: Driver does not support trigger_smd_discovery operation");
		return -1;
	}

	struct wpa_driver_smd_neighbor *driver_neighbors;

	driver_neighbors = os_calloc(num_targets,
				     sizeof(struct wpa_driver_smd_neighbor));
	if (!driver_neighbors) {
		wpa_printf(MSG_ERROR, "SMD: Failed to allocate driver neighbor array");
		return -1;
	}

	for (i = 0; i < num_targets; i++) {
		driver_neighbors[i].short_ssid = targets[i].short_ssid;
		driver_neighbors[i].freq = targets[i].freq;
		driver_neighbors[i].op_class = targets[i].op_class;
		os_memcpy(driver_neighbors[i].bssid, targets[i].bssid, ETH_ALEN);
		driver_neighbors[i].ap_mld_id = targets[i].ap_mld_id;
	}

	int ret = wpa_s->driver->trigger_smd_discovery(wpa_s->drv_priv,
						       driver_neighbors,
						       num_targets);
	os_free(driver_neighbors);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD: Driver failed to trigger discovery: %d", ret);
		return -1;
	}

	wpa_printf(MSG_INFO, "SMD: Discovery trigger successfully for %zu neighbors",
		   num_targets);

	return 0;
}

static u32 smd_calculate_short_ssid(const u8 *ssid, size_t ssid_len)
{
	if (!ssid || ssid_len == 0)
		return 0;

	return ieee80211_crc32(ssid, ssid_len);
}

static struct wpa_bss *smd_find_bss_by_ap_mld_id(struct wpa_supplicant *wpa_s,
						 u8 ap_mld_id)
{
	struct wpa_bss *bss;

	if (!wpa_s)
		return NULL;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bss->rnr_smd_inference_present &&
		    bss->rnr_ap_mld_id == ap_mld_id) {
			return bss;
		}
	}

	return NULL;
}

static struct wpa_bss *smd_find_bss_by_short_ssid(struct wpa_supplicant *wpa_s,
						  u32 short_ssid)
{
	struct wpa_bss *bss;
	u32 bss_short_ssid;

	if (!wpa_s || short_ssid == 0)
		return NULL;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		bss_short_ssid = smd_calculate_short_ssid(bss->ssid, bss->ssid_len);
		if (bss_short_ssid == short_ssid) {
			return bss;
		}
	}

	return NULL;
}

static int smd_method1_direct_same_smd(struct wpa_supplicant *wpa_s,
				       struct wpa_bss *bss)
{
	if (!wpa_s || !bss || !bss->smd_capable)
		return -1;

	if (wpa_s->smd_me_associated) {
		if (os_memcmp(bss->smd_identifier, wpa_s->smd_id, ETH_ALEN) == 0) {
			wpa_printf(MSG_INFO, "SMD: (Direct Same SMD) - BSS "
				    MACSTR " matches current SMD domain " MACSTR,
				   MAC2STR(bss->bssid), MAC2STR(wpa_s->smd_id));
			return 1;
		}
	}

	if (bss->rnr_smd_inference_present && bss->rnr_same_smd_bit) {
		wpa_printf(MSG_INFO, "SMD: (Direct Same SMD) - BSS "
			    MACSTR " has RNR Same SMD bit set",
			   MAC2STR(bss->bssid));
		return 1;
	}

	return 0;
}

static int smd_method2_ap_mld_id_correlation(struct wpa_supplicant *wpa_s,
					     struct wpa_bss *bss)
{
	struct wpa_bss *collocated_bss;

	if (!wpa_s || !bss)
		return -1;

	if (!bss->rnr_smd_inference_present || bss->rnr_ap_mld_id == 0)
		return 0;

	collocated_bss = smd_find_bss_by_ap_mld_id(wpa_s, bss->rnr_ap_mld_id);
	if (!collocated_bss) {
		wpa_printf(MSG_DEBUG, "SMD: (AP MLD ID) - No collocated BSS found with AP MLD ID %u",
			   bss->rnr_ap_mld_id);
		return 0;
	}

	if (collocated_bss->smd_capable) {
		wpa_printf(MSG_INFO, "SMD: (AP MLD ID Correlation) - BSS "
			    MACSTR " coorelated with collocated " MACSTR
			   " via AP MLD ID %u, inferred SMD ID=" MACSTR,
			   MAC2STR(bss->bssid), MAC2STR(collocated_bss->bssid),
			   bss->rnr_ap_mld_id, MAC2STR(collocated_bss->smd_identifier));

		os_memcpy(bss->smd_identifier, collocated_bss->smd_identifier, ETH_ALEN);
		bss->smd_capable = true;

		return 1;
	}

	return 0;
}

static int smd_method3_short_ssid_determination(struct wpa_supplicant *wpa_s,
						struct wpa_bss *bss)
{
	struct wpa_bss *matching_bss;
	u32 bss_short_ssid;

	if (!wpa_s || !bss)
		return -1;

	if (!bss->rnr_smd_inference_present || bss->rnr_short_ssid == 0)
		return 0;

	bss_short_ssid = smd_calculate_short_ssid(bss->ssid, bss->ssid_len);

	matching_bss = smd_find_bss_by_short_ssid(wpa_s, bss_short_ssid);
	if (!matching_bss) {
		wpa_printf(MSG_DEBUG, "SMD:  (Short SSID) - No BSS found with short SSID 0x%08x",
			   bss_short_ssid);
		return 0;
	}

	if (matching_bss->smd_capable) {
		wpa_printf(MSG_INFO, "SMD: Method 3 (Short SSID Determination) - BSS "
			    MACSTR " matched with " MACSTR
			   " via short SSID 0x%08x, inferred SMD ID=" MACSTR,
			   MAC2STR(bss->bssid), MAC2STR(matching_bss->bssid),
			   bss_short_ssid, MAC2STR(matching_bss->smd_identifier));

		os_memcpy(bss->smd_identifier, matching_bss->smd_identifier, ETH_ALEN);
		bss->smd_capable = true;

		return 1;
	}

	return 0;
}

int smd_process_discovery_results(struct wpa_supplicant *wpa_s,
				  struct wpa_bss *bss)
{
	int method_result = 0;

	if (!wpa_s || !bss) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for discovery results processing");
		return -1;
	}

	if (!bss->smd_capable) {
		wpa_printf(MSG_DEBUG, "SMD: BSS " MACSTR " not SMD-capable, skipping topology processing",
			   MAC2STR(bss->bssid));
		return 0;
	}

	wpa_printf(MSG_DEBUG, "SMD: Processing discovery for " MACSTR
		   " (SMD ID= " MACSTR ", source:%s)",
		   MAC2STR(bss->bssid), MAC2STR(bss->smd_identifier),
		   bss->smd_from_beacon ? "beacon" : "probe response");

	method_result = smd_method1_direct_same_smd(wpa_s, bss);
	if (method_result > 0) {
		wpa_printf(MSG_INFO, "SMD: Topology built via Method 1 (Direct Same SMD) for BSS "
			    MACSTR, MAC2STR(bss->bssid));
		goto topology_complete;
	}

	method_result = smd_method2_ap_mld_id_correlation(wpa_s, bss);
	if (method_result > 0) {
		wpa_printf(MSG_INFO, "SMD: Topology built via Method 2 (AP MLD ID Correlation) for BSS "
			    MACSTR, MAC2STR(bss->bssid));
		goto topology_complete;
	}

	method_result = smd_method3_short_ssid_determination(wpa_s, bss);
	if (method_result > 0) {
		wpa_printf(MSG_INFO, "SMD: Topology built via Methos 3 (Short SSID Determination) for BSS "
			    MACSTR, MAC2STR(bss->bssid));
		goto topology_complete;
	}

	wpa_printf(MSG_DEBUG, "SMD: No topology inference method applicable for BSS "
		    MACSTR " - direct SMD IE information user",
		   MAC2STR(bss->bssid));

topology_complete:
	wpa_printf(MSG_INFO, "SMD: Successfully discovered neighbor BSS "
		    MACSTR " SMD ID=" MACSTR,
		   MAC2STR(bss->bssid), MAC2STR(bss->smd_identifier));

	struct smd_group *group;

	group = smd_group_find(wpa_s, bss->smd_identifier);
	if (!group) {
		group = smd_group_create(wpa_s, bss->smd_identifier);
		if (!group) {
			wpa_printf(MSG_ERROR, "SMD: Failed to create group for domain "
				    MACSTR, MAC2STR(bss->smd_identifier));
			return -1;
		}
	}

	if (smd_group_add_member(wpa_s, group, bss) < 0) {
		wpa_printf(MSG_ERROR, "SMD: Failed to add " MACSTR " to group - PTK/Type consistency violation",
			   MAC2STR(bss->bssid));
		return -1;
	}

	wpa_printf(MSG_INFO, "SMD: Discovery processing complete for " MACSTR
		   " - Added to group " MACSTR " (%zu members)",
		   MAC2STR(bss->bssid), MAC2STR(group->smd_id),
		   group->member_count);

	return 0;
}

void smd_neighbor_discovery_flow(struct wpa_supplicant *wpa_s,
				 struct wpa_bss *reporting_bss)
{
	struct smd_neighbor_target *targets = NULL;
	size_t num_targets = 0;
	int ret;

	if (!wpa_s || !reporting_bss) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for neighbor discovery flow");
		return;
	}

	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_DEBUG, "SMD: Not enabled, skipping neighbor discovery");
		return;
	}

	wpa_printf(MSG_DEBUG, "SMD: Starting neighbor discovery flow for reporting BSS "
		    MACSTR, MAC2STR(reporting_bss->bssid));
	smd_set_state(wpa_s, SMD_STATE_DISCOVERING);

	ret = smd_parse_rnr_for_neighbors(wpa_s, reporting_bss,
					  &targets, &num_targets);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD: RNR parsing failed");
		return;
	}

	if (num_targets == 0) {
		wpa_printf(MSG_DEBUG, "SMD: No SMD neighbors in RNR -discovery flow complete");
		return;
	}

	ret = smd_trigger_neighbor_discovery(wpa_s, targets, num_targets);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD: Discovery trigger failed");
		os_free(targets);
		return;
	}

	wpa_printf(MSG_INFO, "SMD: Discovery initiated for %zu neighbor(s)",
		   num_targets);
	os_free(targets);

	wpa_printf(MSG_DEBUG, "SMD: Discovery waiting for scan results");
}

/*
 * SMD Group Managements
 */
void smd_groups_init(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameter for groups init");
		return;
	}

	dl_list_init(&wpa_s->smd_groups);
	wpa_printf(MSG_DEBUG, "SMD: Group Management initialized");
}

void smd_groups_deinit(struct wpa_supplicant *wpa_s)
{
	struct smd_group *group, *tmp_group;
	struct smd_group_member *member, *tmp_member;

	if (!wpa_s)
		return;

	dl_list_for_each_safe(group, tmp_group, &wpa_s->smd_groups,
			      struct smd_group, list) {
		dl_list_for_each_safe(member, tmp_member, &group->members,
				      struct smd_group_member, list) {
			dl_list_del(&member->list);
			os_free(member);
		}
		dl_list_del(&group->list);
		os_free(group);
	}

	wpa_printf(MSG_DEBUG, "SMD: Group management deinitialized");
}

struct smd_group *smd_group_find(struct wpa_supplicant *wpa_s, const u8 *smd_id)
{
	struct smd_group *group;

	if (!wpa_s || !smd_id)
		return NULL;

	dl_list_for_each(group, &wpa_s->smd_groups, struct smd_group, list) {
		if (os_memcmp(group->smd_id, smd_id, ETH_ALEN) == 0) {
			return group;
		}
	}

	return NULL;
}

struct smd_group *smd_group_create(struct wpa_supplicant *wpa_s, const u8 *smd_id)
{
	struct smd_group *group;

	if (!wpa_s || !smd_id) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for group creation");
		return NULL;
	}

	group = os_zalloc(sizeof(*group));
	if (!group) {
		wpa_printf(MSG_ERROR, "SMD: Failed to allocate group structure");
		return NULL;
	}

	os_memcpy(group->smd_id, smd_id, ETH_ALEN);
	dl_list_init(&group->members);
	group->member_count = 0;
	group->ptk_mode_valid = false;
	group->smd_type_valid = false;
	os_get_reltime(&group->last_update);

	dl_list_add(&wpa_s->smd_groups, &group->list);

	wpa_printf(MSG_INFO, "SMD: Created new group for domain " MACSTR,
		   MAC2STR(smd_id));

	return group;
}

bool smd_group_validate_ptk_consistency(struct smd_group *group,
					bool new_ptk_mode,
					const u8 *new_bssid)
{
	if (!group)
		return false;

	if (!group->ptk_mode_valid) {
		group->ptk_mode = new_ptk_mode;
		group->ptk_mode_valid = true;
		wpa_printf(MSG_DEBUG, "SMD: Group " MACSTR " PTK mode set to %d (Per-%s PTK)",
			   MAC2STR(group->smd_id), new_ptk_mode,
			   new_ptk_mode ? "AP MLD" : "SMD");
		return true;
	}

	if (group->ptk_mode != new_ptk_mode) {
		wpa_printf(MSG_ERROR, "SMD: PTK Mode inconsistenct detected");
		wpa_printf(MSG_ERROR, "SMD:   Group " MACSTR " requires PTK mode=%d (Per-%s PTK)",
			   MAC2STR(group->smd_id), group->ptk_mode,
			   group->ptk_mode ? "AP MLD" : "SMD");
		wpa_printf(MSG_ERROR, "SMD:   AP " MACSTR " advertises PTK mode=%d (Per-%s PTK)",
			   MAC2STR(new_bssid), new_ptk_mode,
			   new_ptk_mode ? "AP MLD" : "SMD");
		wpa_printf(MSG_ERROR, "SMD:   Rejecting AP - cannot mix PTK modes within same SMD domain");
		return false;
	}

	return true;
}

bool smd_group_validate_smd_type(struct smd_group *group,
				 bool new_smd_type,
				 const u8 *new_bssid)
{
	if (!group)
		return false;

	if (!group->smd_type_valid) {
		group->smd_type = new_smd_type;
		group->smd_type_valid = true;
		wpa_printf(MSG_DEBUG, "SMD: Group " MACSTR " SMD Type set to %d",
			   MAC2STR(group->smd_id), new_smd_type);
		return true;
	}

	if (group->smd_type != new_smd_type) {
		wpa_printf(MSG_ERROR, "SMD: SMD Type inconsistency detected");
		wpa_printf(MSG_ERROR, "SMD:  Group " MACSTR " requires SMD Type=%d",
			   MAC2STR(group->smd_id), group->smd_type);
		wpa_printf(MSG_ERROR, "SMD:  AP " MACSTR " advertises SMD Type=%d",
			   MAC2STR(new_bssid), new_smd_type);
		wpa_printf(MSG_ERROR, "SMD:  Rejecting AP - SMD Type must be consistent within domain");
		return false;
	}

	return true;
}

bool smd_group_validate_timeout_consitency(struct smd_group *group,
					   u16 new_timeout,
					   const u8 *new_bssid)
{
	if (!group)
		return false;

	if (group->member_count == 0) {
		group->min_timeout = new_timeout;
		wpa_printf(MSG_DEBUG,
			   "SMD: Group " MACSTR " timeout set to %u TUs",
			   MAC2STR(group->smd_id), new_timeout);
		return true;
	}

	if (group->min_timeout != new_timeout) {
		wpa_printf(MSG_ERROR,
			   "SMD: Timeout inconsitency detected");
		wpa_printf(MSG_ERROR,
			   "SMD:   Group " MACSTR " requires timeout=%u TUs",
			   MAC2STR(group->smd_id), group->min_timeout);
		wpa_printf(MSG_ERROR,
			   "SMD:   AP " MACSTR " advertises timeout=%u TUs",
			   MAC2STR(new_bssid), new_timeout);
		wpa_printf(MSG_ERROR,
			   "SMD:   Rejecting AP - timeout must be consistent across all AP MLDs in SMD");
		return false;
	}

	return true;
}

int smd_group_add_member(struct wpa_supplicant *wpa_s, struct smd_group *group,
			 struct wpa_bss *bss)
{
	struct smd_group_member *member;

	if (!wpa_s || !group || !bss) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for add_member");
		return -1;
	}

	if (!bss->smd_capable) {
		wpa_printf(MSG_ERROR, "SMD: Cannot add non-SMD capabe AP " MACSTR,
			   MAC2STR(bss->bssid));
		return -1;
	}
	dl_list_for_each(member, &group->members, struct smd_group_member, list) {
		if (os_memcmp(member->bssid, bss->bssid, ETH_ALEN) == 0) {
			member->ptk_mode = bss->smd_ptk_mode;
			member->smd_type = bss->smd_type;
			member->max_targets = bss->smd_max_targets;
			member->timeout = bss->smd_timeout;
			member->dl_forwarding = bss->smd_dl_forwarding;

			os_get_reltime(&member->last_seen);

			wpa_printf(MSG_DEBUG, "SMD: Updated member " MACSTR
				   " in group " MACSTR,
				   MAC2STR(bss->bssid), MAC2STR(group->smd_id));
			os_get_reltime(&group->last_update);
			return 0;
		}
	}

	member = os_zalloc(sizeof(*member));
	if (!member) {
		wpa_printf(MSG_ERROR, "SMD: Failed to allocate group member");
		return -1;
	}

	os_memcpy(member->bssid, bss->bssid, ETH_ALEN);
	os_memcpy(member->ap_mld_addr, bss->mld_addr, ETH_ALEN);
	member->ptk_mode = bss->smd_ptk_mode;
	member->smd_type = bss->smd_type;
	member->max_targets = bss->smd_max_targets;
	member->timeout = bss->smd_timeout;
	member->dl_forwarding = bss->smd_dl_forwarding;

	os_get_reltime(&member->last_seen);

	dl_list_add(&group->members, &member->list);
	group->member_count++;

	wpa_printf(MSG_INFO, "SMD: Added member " MACSTR " to group " MACSTR
		   " (count=%zu, PTK mode=%d, SMD type=%d)",
		   MAC2STR(bss->bssid), MAC2STR(group->smd_id),
		   group->member_count, member->ptk_mode, member->smd_type);

	os_get_reltime(&group->last_update);

	return 0;
}

/*
 * SMD Security changes
 */
int smd_validate_ie_format(const u8 *ie, size_t ie_len)
{
	if (!ie) {
		wpa_printf(MSG_ERROR, "SMD: Null IE buffer in format validation");
		return -1;
	}

	if (ie_len < 12) {
		wpa_printf(MSG_WARNING, "SMD: IE format validation failed - length too short (expected >= 12, got %zu)",
			   ie_len);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SMD: IE format validation passed - length=%zu octets",
		   ie_len);

	return 0;
}

int smd_validate_security_policy(struct wpa_supplicant *wpa_s,
				 struct wpa_bss *bss)
{
	const u8 *rsn_ie, *wpa_ie;

	if (!wpa_s || !bss) {
		wpa_printf(MSG_ERROR, "SMD: Invalid params for security policy validation");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SMD: Validating security policy for BSS " MACSTR,
		   MAC2STR(bss->bssid));

	rsn_ie = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	wpa_ie = wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);

	if (!rsn_ie && !wpa_ie) {
		wpa_printf(MSG_ERROR, "SMD: Security policy violation - AP "
			    MACSTR " has no WPA/WPA2/WPA2 encryption",
			   MAC2STR(bss->bssid));
		return -1;
	}

	if (!rsn_ie && wpa_ie) {
		wpa_printf(MSG_WARNING, "SMD: AP " MACSTR " uses WPA (not WPA2/WPA3) - accepting but WPA2/WPA3 preferred for SMD",
			   MAC2STR(bss->bssid));
	}

	if (rsn_ie) {
		wpa_printf(MSG_DEBUG, "SMD: RSN/WPA2/WPA3 encryption present");
	}

	return 0;
}


int smd_btm_filter_candidate(struct wpa_supplicant *wpa_s,
			     struct wpa_bss *bss,
			     struct neighbor_report *neighbor)
{
	if (!wpa_s || !bss || !neighbor)
		return -1;

	/* TBD */
	return 0;
}

void smd_btm_enhance_preference(struct wpa_supplicant *wpa_s,
				struct neighbor_report *neighbor,
				struct wpa_bss *bss)
{
	if (!wpa_s || !neighbor || !bss)
		return;

	/* TBD */
}

int smd_ctrl_iface_domains(struct wpa_supplicant *wpa_s, char *buf,
			   size_t buflen)
{
	struct smd_group *group;
	char *pos, *end;

	int ret, count = 0;

	if (!wpa_s || !buf)
		return -1;

	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_DEBUG, "SMD: Not enabled - no domains");
		return -1;
	}

	pos = buf;
	end = buf + buflen;

	dl_list_for_each(group, &wpa_s->smd_groups, struct smd_group, list) {
		count++;
	}

	ret = os_snprintf(pos, end - pos, "domain_count=%d\n", count);
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	count = 0;
	dl_list_for_each(group, &wpa_s->smd_groups, struct smd_group, list) {
		ret = os_snprintf(pos, end - pos, "domain[%d]=" MACSTR "\n",
				  count, MAC2STR(group->smd_id));
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
		count++;
	}

	return pos - buf;
}

int smd_ctrl_iface_groups(struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	struct smd_group_member *member;
	struct smd_group *group;
	char *pos, *end;
	int ret, group_idx = 0, member_idx;

	if (!wpa_s || !buf)
		return -1;

	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_DEBUG, "SMD: Not enabled - no groups");
		return -1;
	}

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "group_count=%d\n",
			  dl_list_len(&wpa_s->smd_groups));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;

	pos += ret;

	dl_list_for_each(group, &wpa_s->smd_groups, struct smd_group, list) {
		ret = os_snprintf(pos, end - pos,
				  "group[%d].smd_id=" MACSTR "\n"
				  "group[%d].member_count=%zu\n"
				  "group[%d].ptk_mode=%d\n"
				  "group[%d].ptk_mode_valid=%d\n"
				  "group[%d].smd_type=%d\n"
				  "group[%d].smd_type_valid=%d\n"
				  "group[%d].timeout=%u\n"
				  "group[%d].max_targets=%u\n"
				  "group[%d].dl_forwarding=%d\n",
				  group_idx, MAC2STR(group->smd_id),
				  group_idx, group->member_count,
				  group_idx, group->ptk_mode,
				  group_idx, group->ptk_mode_valid,
				  group_idx, group->smd_type,
				  group_idx, group->smd_type_valid,
				  group_idx, group->min_timeout,
				  group_idx, group->min_max_targets,
				  group_idx, group->dl_forwarding);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		member_idx = 0;
		dl_list_for_each(member, &group->members, struct smd_group_member, list) {
			ret = os_snprintf(pos, end - pos,
					  "group[%d].member[%d].bssid=" MACSTR "\n"
					   "group[%d].member[%d].ap_mld_addr=" MACSTR "\n"
					   "group[%d].member[%d].ptk_mode=%d\n"
					   "group[%d].member[%d].smd_type=%d\n"
					   "group[%d].member[%d].timeout=%u\n"
					   "group[%d].member[%d].max_targets=%u\n"
					   "group[%d].member[%d].dl_forwarding=%d\n",
					   group_idx, member_idx, MAC2STR(member->bssid),
					   group_idx, member_idx, MAC2STR(member->ap_mld_addr),
					   group_idx, member_idx, member->ptk_mode,
					   group_idx, member_idx, member->smd_type,
					   group_idx, member_idx, member->timeout,
					   group_idx, member_idx, member->max_targets,
					   group_idx, member_idx, member->dl_forwarding);
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;

			member_idx++;
		}

		group_idx++;
	}

	return pos - buf;
}

int smd_ctrl_iface_status(struct wpa_supplicant *wpa_s,
			  char *buf, size_t buflen)
{
	char *pos, *end;
	int ret;

	if (!wpa_s || !buf)
		return -1;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos,
			  "smd_enabled=%d\n"
			  "smd_state=%s\n"
			  "smd_me_associated=%d\n",
			  smd_enabled(wpa_s),
			  smd_state_txt(smd_get_state(wpa_s)),
			  wpa_s->smd_me_associated);
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;

	pos += ret;

	if (wpa_s->smd_me_associated) {
		ret = os_snprintf(pos, end - pos,
				  "smd_id=" MACSTR "\n"
				  "smd_ptk_mode=%d\n"
				  "smd_in_transition=%d\n",
				  MAC2STR(wpa_s->smd_id),
				  wpa_s->smd_ptk_mode,
				  wpa_s->smd_in_transition);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		if (wpa_s->smd_in_transition && !is_zero_ether_addr(wpa_s->smd_previous_id)) {
			ret = os_snprintf(pos, end - pos,
					  "smd_previous_id=" MACSTR "\n",
					  MAC2STR(wpa_s->smd_previous_id));
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}
	}

	ret = os_snprintf(pos, end - pos, "discovered_domains=%d\n",
			  dl_list_len(&wpa_s->smd_groups));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	return pos - buf;
}

bool smd_is_bss_fresh(struct wpa_bss *bss)
{
	struct os_reltime now, age;
	unsigned int max_age_sec;
	const char *discovery_type;

	if (!bss || !bss->smd_capable)
		return false;

	os_get_reltime(&now);
	os_reltime_sub(&now, &bss->last_update, &age);

	if (bss->rnr_smd_inference_present) {
		max_age_sec = 60;
		discovery_type = "RNR-only";
	} else if (bss->smd_from_beacon) {
		max_age_sec = 300;
		discovery_type = "Beacon";
	} else {
		max_age_sec = 180;
		discovery_type = "Active";
	}

	if (age.sec > max_age_sec) {
		wpa_printf(MSG_DEBUG,
			   "SMD: BSS " MACSTR " STALE (age: %u sec, max: %u sec, type: %s)",
			   MAC2STR(bss->bssid), (unsigned int)age.sec,
			   max_age_sec, discovery_type);
		return false;
	}

	if (is_zero_ether_addr(bss->smd_identifier)) {
		wpa_printf(MSG_DEBUG, "SMD: BSS " MACSTR " has invalid SMD ID",
			   MAC2STR(bss->bssid));
		return false;
	}

	wpa_printf(MSG_DEBUG, "SMD: BSS " MACSTR " Fresh (age: %u sec, type: %s)",
		   MAC2STR(bss->bssid), (unsigned int)age.sec, discovery_type);

	return true;
}

int smd_validate_group_freshness(struct wpa_supplicant *wpa_s,
				 struct smd_group *group)
{
	struct smd_group_member *member;
	struct wpa_bss *bss;
	int fresh_count = 0;
	int stale_count = 0;

	if (!wpa_s || !group)
		return -1;

	dl_list_for_each(member, &group->members, struct smd_group_member, list) {
		bss = wpa_bss_get_bssid(wpa_s, member->bssid);
		if (!bss) {
			wpa_printf(MSG_DEBUG,
				   "SMD: BSS " MACSTR "no longer in table - skipping",
				   MAC2STR(member->bssid));
			stale_count++;
			continue;
		}

		if (smd_is_bss_fresh(bss)) {
			fresh_count++;
		} else {
			stale_count++;
		}
	}

	wpa_printf(MSG_DEBUG, "SMD: Group " MACSTR " freshness check - Fresh: %d, Stale: %d (total %zu)",
		   MAC2STR(group->smd_id), fresh_count, stale_count,
		   group->member_count);

	if (fresh_count == 0 && group->member_count > 0) {
		wpa_printf(MSG_WARNING,
			   "SMD: Group " MACSTR " has no fresh candidates - re-scan recommended before target selection",
			   MAC2STR(group->smd_id));
	}

	return fresh_count;
}

void smd_trigger_freshness_scan(struct wpa_supplicant *wpa_s,
				struct smd_group *group)
{
	if (!wpa_s || !group)
		return;

	wpa_printf(MSG_INFO,
		   "SMD: Triggerign freshness scan for group " MACSTR,
		   MAC2STR(group->smd_id));

	wpa_supplicant_req_scan(wpa_s, 0, 0);
}

enum smd_discovery_completeness smd_get_discovery_completeness(struct wpa_bss *bss)
{
	if (!bss || !bss->smd_capable)
		return SMD_DISC_NONE;

	if (bss->rnr_smd_inference_present)
		return SMD_DISC_RNR_ONLY;

	return SMD_DISC_NONE;
}

static void wpas_smd_free_prepared_target(struct wpa_smd_prepared_target *target)
{
	if (!target)
		return;

	dl_list_del(&target->list);

	if (target->dh_ctx) {
		crypto_ecdh_deinit(target->dh_ctx);
		target->dh_ctx = NULL;
	}
	os_free(target->dh_pubkey);
	os_free(target->peer_dh_pubkey);
	os_free(target);
}

static struct wpa_smd_prepared_target *
wpas_smd_get_prepared_target(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_smd_prepared_target *target;

	dl_list_for_each(target, &wpa_s->smd_targets,
			 struct wpa_smd_prepared_target, list) {
		if (os_memcmp(target->bssid, bssid, ETH_ALEN) == 0)
			return target;
		if (!is_zero_ether_addr(target->target_mld_addr) &&
		    os_memcmp(target->target_mld_addr, bssid, ETH_ALEN) == 0)
			return target;
		if (!is_zero_ether_addr(target->serving_bssid) &&
		    os_memcmp(target->serving_bssid, bssid, ETH_ALEN) == 0 &&
		    (target->state == SMD_TARGET_PREP_PENDING ||
		     target->state == SMD_TARGET_EXEC_PENDING))
			return target;
	}

	return NULL;
}

struct wpa_smd_prepared_target *
wpas_smd_get_prepared_target_v2(struct wpa_supplicant *wpa_s)
{
	struct wpa_smd_prepared_target *target;

	dl_list_for_each(target, &wpa_s->smd_targets,
			 struct wpa_smd_prepared_target, list) {
		if (target->state == SMD_TARGET_PREPARED)
			return target;
	}

	return NULL;
}

/**
 * wpas_smd_pick_exec_link - Select TX link for ST Execution
 * @wpa_s: wpa_supplicant context
 *
 * Returns the link ID of the first active non-assoc link from valid_links,
 * or mlo_assoc_link_id when no other link is available (single-link MLO or
 * all non-assoc links dormant).
 * Returns -1 for non-MLO connections (let mac80211 decide).
 */
static s8 wpas_smd_pick_exec_link(struct wpa_supplicant *wpa_s)
{
	u16 non_assoc_links;
	u8 link_id;

	if (!wpa_s->valid_links) {
		/* Non-MLO connection: no link selection needed */
		return -1;
	}

	/* Exclude the assoc link from candidates */
	non_assoc_links = wpa_s->valid_links & ~BIT(wpa_s->mlo_assoc_link_id);
	if (!non_assoc_links) {
		wpa_printf(MSG_ERROR,
			   "SMD: ST Exec TX: no non-assoc link available "
			   "(valid_links=0x%04x assoc_link=%u), "
			   "falling back to assoc link",
			   wpa_s->valid_links, wpa_s->mlo_assoc_link_id);
		return (s8) wpa_s->mlo_assoc_link_id;
	}

	/* Pick the lowest-numbered non-assoc active link */
	for (link_id = 0; link_id < MAX_NUM_MLD_LINKS; link_id++) {
		if (non_assoc_links & BIT(link_id)) {
			wpa_printf(MSG_ERROR,
				   "SMD: ST Exec TX: selected link_id=%u "
				   "(assoc_link=%u valid_links=0x%04x)",
				   link_id, wpa_s->mlo_assoc_link_id,
				   wpa_s->valid_links);
			return (s8) link_id;
		}
	}

	/* Unreachable, but be safe */
	return (s8) wpa_s->mlo_assoc_link_id;
}

int wpas_smd_request_prepare(struct wpa_supplicant *wpa_s, const u8 *bssid,
			     int no_dl_sn, int no_ul_sn, const char *scs_ids)
{
	struct wpa_smd_prepared_target *target;
	struct wpa_driver_uhr_reconfig_params params;
	int ret = -1;
	size_t prepared_count = 0;
	struct wpa_bss *target_bss;
	u8 target_mld_addr[ETH_ALEN];

	if (!wpa_s || !bssid) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for SMD preparation");
		return -1;
	}

	/* Step 1: Validate state (must be associated, not already preparing) */
	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_ERROR, "SMD: Cannot prepare - SMD not enabled");
		return -1;
	}

	if (!wpa_s->smd_me_associated) {
		wpa_printf(MSG_ERROR, "SMD: Cannot prepare - not associated with SMD-ME");
		return -1;
	}

	/* Step 2: Check max prepared targets limit (REQ-PREP-004) */
	dl_list_for_each(target, &wpa_s->smd_targets,
			 struct wpa_smd_prepared_target, list) {
		if (target->state == SMD_TARGET_PREP_PENDING || target->state == SMD_TARGET_PREPARED)
			prepared_count++;
	}

	/* Assume max limit of 8 prepared targets (can be made configurable) */
	if (prepared_count >= 8) {
		wpa_printf(MSG_ERROR, "SMD: Cannot prepare - max prepared targets limit (%zu) reached",
			   prepared_count);
		return -1;
	}

	/* Step 3: Validate target BSS is SMD-capable and in known SMD group */
	target_bss = wpa_bss_get_bssid(wpa_s, bssid);
	if (!target_bss) {
		wpa_printf(MSG_ERROR, "SMD: Cannot prepare - target BSS " MACSTR " not found",
			   MAC2STR(bssid));
		return -1;
	}

	if (!target_bss->smd_capable) {
		wpa_printf(MSG_ERROR, "SMD: Cannot prepare - target BSS " MACSTR " is not SMD-capable",
			   MAC2STR(bssid));
		return -1;
	}

	/* Verify target BSS is part of a known SMD group */
	struct smd_group *target_group = smd_group_find(wpa_s, target_bss->smd_identifier);

	if (!target_group) {
		wpa_printf(MSG_ERROR, "SMD: Cannot prepare - target BSS " MACSTR " SMD domain " MACSTR
			   " is not in any known SMD group. Discovery may be needed first.",
			   MAC2STR(bssid), MAC2STR(target_bss->smd_identifier));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SMD: Target BSS " MACSTR " validated - SMD domain " MACSTR
		   " found in group with %zu members",
		   MAC2STR(bssid), MAC2STR(target_bss->smd_identifier), target_group->member_count);

	/* Step 4: Determine context transfer preferences (REQ-PREP-REQ-013/015) */
	/* Use BSS-specific preferences from the validated target */
	wpa_printf(MSG_DEBUG, "SMD: Using context transfer preferences for target BSS " MACSTR,
		   MAC2STR(bssid));

	/* Step 5: Build request parameters */
	target = wpas_smd_get_prepared_target(wpa_s, bssid);
	if (target) {
		wpa_printf(MSG_DEBUG, "SMD: Updating existing prep target " MACSTR, MAC2STR(bssid));
		target->state = SMD_TARGET_PREP_PENDING;
	} else {
		target = os_zalloc(sizeof(*target));
		if (!target)
			return -1;
		os_memcpy(target->bssid, bssid, ETH_ALEN);
		dl_list_add(&wpa_s->smd_targets, &target->list);
		target->state = SMD_TARGET_PREP_PENDING;
	}

	target->no_dl_sn = no_dl_sn;
	target->no_ul_sn = no_ul_sn;
	if (!is_zero_ether_addr(wpa_s->bssid))
		os_memcpy(target->serving_bssid, wpa_s->bssid, ETH_ALEN);

	os_memset(&params, 0, sizeof(params));

        /* Derive target MLD address from target BSS - copy to local static array */
        if (!is_zero_ether_addr(target_bss->mld_addr)) {
                os_memcpy(target_mld_addr, target_bss->mld_addr, ETH_ALEN);
        } else {
                /* Fallback to BSSID if MLD address not available */
                os_memcpy(target_mld_addr, target_bss->bssid, ETH_ALEN);
        }

	wpa_printf(MSG_DEBUG, "SMD: target MLD address: " MACSTR, MAC2STR(target_mld_addr));
        os_memcpy(target->target_mld_addr, target_mld_addr, ETH_ALEN);

	params.bssid = target->bssid;
	params.target_mld_addr = target_mld_addr;
	params.no_dl_sn = no_dl_sn;
	params.no_ul_sn = no_ul_sn;
	params.is_preferred_target = 1;
	params.scs_ids = scs_ids;
	params.ssid = target_bss->ssid;
	params.ssid_len = target_bss->ssid_len;
	params.ptk_mode = -1;
	params.reconfig_info = NULL; /* Will be set by caller if link management needed */

	/* Kernel (driver/nl80211) now generates all protocol fields:
	 * - dialog token
	 * - ECDH keys
	 * - snonce/nonce
	 */

	/* Step 5: Call driver: wpa_drv_uhr_reconfig_req() */
	ret = wpa_drv_uhr_reconfig_req(wpa_s, &params);

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD: Failed to send SMD Prepare request");
		target->state = SMD_TARGET_FAILED;
		wpas_smd_free_prepared_target(target);
		return -1;
	}

	/* Step 6: Transition to SMD_PREPARING state */
	smd_set_state(wpa_s, SMD_STATE_TRANSITIONING);

	wpa_printf(MSG_INFO, "SMD: Preparation started for " MACSTR " (prepared targets: %zu/8)",
		   MAC2STR(bssid), prepared_count + 1);
	return 0;
}

/**
 * wpas_uhr_link_reconfig_common_params - Validate state and populate shared
 * UHR Link Reconfiguration Request parameters common to both ST Prep and
 * ST Exec requests.
 *
 * Performs:
 *   - SMD enabled / associated checks
 *   - Max prepared targets limit check (Prep only — caller passes check flag)
 *   - Target BSS lookup and SMD capability validation
 *   - SMD group membership check
 *   - Link management info (add_links BSSID/freq, delete_links) population
 *   - Target entry alloc/update, serving BSSID copy
 *   - Base wpa_driver_uhr_reconfig_params initialisation (target MLD addr,
 *     SSID, SN flags, exec_path, tx_link_id, smd params)
 *
 * Returns 0 on success and fills @target_out and @params_out.
 * Returns -1 on any validation failure without modifying driver state.
 */
static int
wpas_uhr_link_reconfig_common_params(struct wpa_supplicant *wpa_s,
				     const u8 *bssid,
				     int no_dl_sn, int no_ul_sn,
				     struct wpa_mlo_reconfig_info *reconfig_info,
				     int is_exec,
				     int is_preferred_target,
				     struct wpa_smd_prepared_target **target_out,
				     struct wpa_driver_uhr_reconfig_params *params)
{
	struct wpa_smd_prepared_target *target;
	struct wpa_bss *target_bss;
	struct smd_group *target_group;
	size_t prepared_count = 0;
	static u8 target_mld_addr[ETH_ALEN];

	if (!wpa_s || !bssid)
		return -1;

	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_ERROR, "SMD: Cannot send UHR reconfig - SMD not enabled");
		return -1;
	}

	if (!wpa_s->smd_me_associated) {
		wpa_printf(MSG_ERROR, "SMD: Cannot send UHR reconfig - not associated with SMD-ME");
		return -1;
	}

	/* Max prepared targets check applies to ST Prep only */
	if (!is_exec) {
		dl_list_for_each(target, &wpa_s->smd_targets,
				 struct wpa_smd_prepared_target, list) {
			if (target->state == SMD_TARGET_PREP_PENDING ||
			    target->state == SMD_TARGET_PREPARED)
				prepared_count++;
		}
		if (prepared_count >= 8) {
			wpa_printf(MSG_ERROR,
				   "SMD: Cannot prepare - max prepared targets limit (%zu) reached",
				   prepared_count);
			return -1;
		}
	}

	target_bss = wpa_bss_get_bssid(wpa_s, bssid);
	if (!target_bss) {
		wpa_printf(MSG_ERROR, "SMD: Target BSS " MACSTR " not found",
			   MAC2STR(bssid));
		return -1;
	}

	if (!target_bss->smd_capable) {
		wpa_printf(MSG_ERROR, "SMD: Target BSS " MACSTR " is not SMD-capable",
			   MAC2STR(bssid));
		return -1;
	}

	target_group = smd_group_find(wpa_s, target_bss->smd_identifier);
	if (!target_group) {
		wpa_printf(MSG_ERROR,
			   "SMD: Target BSS " MACSTR " SMD domain " MACSTR
			   " not in any known SMD group; discovery may be needed",
			   MAC2STR(bssid), MAC2STR(target_bss->smd_identifier));
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "SMD: Target BSS " MACSTR " validated - SMD domain " MACSTR
		   " found in group with %zu members",
		   MAC2STR(bssid), MAC2STR(target_bss->smd_identifier),
		   target_group->member_count);

	/* Populate add_links BSSID + freq from scan cache */
	if (reconfig_info && reconfig_info->add_links) {
		u8 link_id;

		for_each_link(reconfig_info->add_links, link_id) {
			if (!(target_bss->valid_links & BIT(link_id))) {
				wpa_printf(MSG_ERROR,
					   "SMD: Link %u not found in target BSS scan",
					   link_id);
				return -1;
			}
			os_memcpy(reconfig_info->add_link_bssid[link_id],
				  target_bss->mld_links[link_id].bssid,
				  ETH_ALEN);
			reconfig_info->add_link_freq[link_id] =
				target_bss->mld_links[link_id].freq;
		}
	}

	/* Validate delete_links against currently active links */
	if (reconfig_info && reconfig_info->delete_links) {
		u8 link_id;

		for_each_link(reconfig_info->delete_links, link_id) {
			if (!(wpa_s->valid_links & BIT(link_id))) {
				wpa_printf(MSG_ERROR,
					   "SMD: Link %u not active, cannot delete",
					   link_id);
				return -1;
			}
		}
	}

	/* Alloc or update the prepared target entry */
	target = wpas_smd_get_prepared_target(wpa_s, bssid);
	if (target) {
		wpa_printf(MSG_DEBUG, "SMD: Updating existing target " MACSTR,
			   MAC2STR(bssid));
		if (!is_exec)
			target->state = SMD_TARGET_PREP_PENDING;
	} else {
		target = os_zalloc(sizeof(*target));
		if (!target)
			return -1;
		os_memcpy(target->bssid, bssid, ETH_ALEN);
		dl_list_add(&wpa_s->smd_targets, &target->list);
		target->state = SMD_TARGET_PREP_PENDING;
	}

	target->no_dl_sn = no_dl_sn;
	target->no_ul_sn = no_ul_sn;
	target->is_preferred_target = is_preferred_target;
	if (!is_zero_ether_addr(wpa_s->bssid))
		os_memcpy(target->serving_bssid, wpa_s->bssid, ETH_ALEN);

	/* Base params */
	os_memset(params, 0, sizeof(*params));
	params->bssid = target->bssid;
	params->is_preferred_target = target->is_preferred_target;
	params->exec_path = wpa_s->smd_st_exec_path;
	params->no_dl_sn = no_dl_sn;
	params->no_ul_sn = no_ul_sn;
	params->reconfig_info = reconfig_info;
	params->ptk_mode = wpa_s->smd_ptk_mode;

	if (!is_zero_ether_addr(target_bss->mld_addr)) {
		os_memcpy(target_mld_addr, target_bss->mld_addr, ETH_ALEN);
		params->target_mld_addr = target_mld_addr;
		params->ssid = target_bss->ssid;
		params->ssid_len = target_bss->ssid_len;
	} else {
		os_memcpy(target_mld_addr, target_bss->bssid, ETH_ALEN);
		params->target_mld_addr = target_mld_addr;
	}

	params->smd.enabled = wpa_s->smd_capable;
	os_memcpy(params->smd.smd_identifier, target_bss->smd_identifier, ETH_ALEN);
	params->smd.smd_timeout = target_bss->smd_timeout;
	params->smd.caps.max_prep_target_apmlds = 0;
	params->smd.caps.smd_type = target_bss->smd_type;
	params->smd.caps.ptk_mode = target_bss->smd_ptk_mode;

	*target_out = target;
	return 0;
}

/**
 * wpas_uhr_link_reconfig_prep_request - Send a UHR Link Reconfiguration
 * Request frame with Type=0 (ST Preparation Request).
 *
 * Builds the ST Prep Request after common parameter validation, generates
 * SNonce and DH public key for Per-AP MLD PTK mode, and calls the driver.
 */
int wpas_uhr_link_reconfig_prep_request(struct wpa_supplicant *wpa_s,
					const u8 *bssid,
					int no_dl_sn, int no_ul_sn,
					const char *scs_ids,
					struct wpa_mlo_reconfig_info *reconfig_info,
					u8 **per_link_ie_buf,
					size_t *per_link_ie_len,
					size_t max_links,
					int is_preferred_target)
{
	struct wpa_smd_prepared_target *target;
	struct wpa_driver_uhr_reconfig_params params;
	int ret;

	if (wpas_uhr_link_reconfig_common_params(wpa_s, bssid, no_dl_sn,
						 no_ul_sn, reconfig_info,
						 0 /* prep */,
						 is_preferred_target,
						 &target, &params) < 0)
		return -1;

	params.type = 0;
	params.scs_ids = scs_ids;
	params.per_link_ie_buf = per_link_ie_buf;
	params.per_link_ie_len = per_link_ie_len;
	params.max_links = max_links;
	params.tx_link_id = (s8) wpa_s->mlo_assoc_link_id;

	/* Generate SNonce + DH public key for Per-AP MLD PTK mode */
	os_memset(target->snonce, 0, sizeof(target->snonce));
	if (wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_AP) {
		if (os_get_random(target->snonce, WPA_NONCE_LEN) < 0) {
			wpa_printf(MSG_ERROR, "SMD: Failed to generate SNonce");
		} else {
			params.snonce = target->snonce;
			params.snonce_len = WPA_NONCE_LEN;
		}

		int dh_group = 0;

		if (wpa_s->current_ssid && wpa_s->current_ssid->fils_dh_group)
			dh_group = wpa_s->current_ssid->fils_dh_group;
		if (dh_group == 0)
			dh_group = 19;

		target->dh_ctx = crypto_ecdh_init(dh_group);
		if (!target->dh_ctx) {
			wpa_printf(MSG_ERROR,
				   "SMD: ECDH init failed (group=%d)", dh_group);
			return -1;
		}
		target->dh_group = dh_group;

		struct wpabuf *pub = crypto_ecdh_get_pubkey(target->dh_ctx, 1);

		if (pub) {
			os_free(target->dh_pubkey);
			target->dh_pubkey_len = wpabuf_len(pub);
			target->dh_pubkey = os_memdup(wpabuf_head(pub),
						      target->dh_pubkey_len);
			if (target->dh_pubkey) {
				params.dh_pubkey = target->dh_pubkey;
				params.dh_pubkey_len = target->dh_pubkey_len;
			}
		} else {
			wpa_printf(MSG_ERROR, "SMD: Failed to get DH public key");
		}
	}

	wpa_printf(MSG_DEBUG,
		   "SMD: Sending ST Preparation Request (type=0) to " MACSTR,
		   MAC2STR(bssid));
	wpa_printf(MSG_INFO,
		   "SMD: Params: enabled=%d ptk_mode=%d identifier=" MACSTR,
		   params.smd.enabled, params.smd.caps.ptk_mode,
		   MAC2STR(params.smd.smd_identifier));

	ret = wpa_drv_uhr_reconfig_req(wpa_s, &params);

	if (per_link_ie_buf) {
		size_t i;

		for (i = 0; i < max_links; i++) {
			if (per_link_ie_buf[i]) {
				os_free(per_link_ie_buf[i]);
				per_link_ie_buf[i] = NULL;
				per_link_ie_len[i] = 0;
			}
		}
	}

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD: ST Preparation Request failed");
		target->state = SMD_TARGET_FAILED;
		wpas_smd_free_prepared_target(target);
		return -1;
	}

	smd_set_state(wpa_s, SMD_STATE_TRANSITIONING);
	wpa_printf(MSG_INFO,
		   "SMD: ST Preparation Request sent to " MACSTR, MAC2STR(bssid));
	return 0;
}

/**
 * wpas_uhr_link_reconfig_exec_request - Send a UHR Link Reconfiguration
 * Request frame with Type=1 (ST Execution Request).
 *
 * Builds the ST Exec Request after common parameter validation.
 * SNonce and DH are not included in the Exec Request.
 */
int wpas_uhr_link_reconfig_exec_request(struct wpa_supplicant *wpa_s,
					const u8 *bssid,
					int no_dl_sn, int no_ul_sn,
					struct wpa_mlo_reconfig_info *reconfig_info,
					int is_preferred_target)
{
	struct wpa_smd_prepared_target *target;
	struct wpa_driver_uhr_reconfig_params params;
	int ret;

	if (wpas_uhr_link_reconfig_common_params(wpa_s, bssid, no_dl_sn,
						 no_ul_sn, reconfig_info,
						 1 /* exec */,
						 is_preferred_target,
						 &target, &params) < 0)
		return -1;

	params.type = 1;
	params.snonce = NULL;
	params.snonce_len = 0;
	params.dh_pubkey = NULL;
	params.dh_pubkey_len = 0;
	params.scs_ids = NULL;
	params.per_link_ie_buf = NULL;
	params.per_link_ie_len = NULL;
	params.max_links = 0;

	if (reconfig_info) {
		params.exec_path = reconfig_info->exec_path;
		params.dl_tid_bitmap = reconfig_info->dl_tid_bitmap;
	}

	if (reconfig_info && reconfig_info->force_diff_tx)
		params.tx_link_id = wpas_smd_pick_exec_link(wpa_s);
	else
		params.tx_link_id = (s8) wpa_s->mlo_assoc_link_id;

	if (!params.target_mld_addr) {
		wpa_printf(MSG_ERROR,
			   "SMD: Target MLD address required for ST Execution");
		wpas_smd_free_prepared_target(target);
		return -1;
	}

	target->state = SMD_TARGET_EXEC_PENDING;

	wpa_printf(MSG_DEBUG,
		   "SMD: Sending ST Execution Request (type=1) to " MACSTR
		   " exec_path=%u dl_tid=0x%02x",
		   MAC2STR(bssid), params.exec_path, params.dl_tid_bitmap);
	wpa_printf(MSG_INFO,
		   "SMD: Params: enabled=%d ptk_mode=%d identifier=" MACSTR,
		   params.smd.enabled, params.smd.caps.ptk_mode,
		   MAC2STR(params.smd.smd_identifier));

	ret = wpa_drv_uhr_reconfig_req(wpa_s, &params);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "SMD: ST Execution Request failed");
		target->state = SMD_TARGET_FAILED;
		wpas_smd_free_prepared_target(target);
		return -1;
	}

	wpa_printf(MSG_INFO,
		   "SMD: ST Execution Request sent to " MACSTR, MAC2STR(bssid));
	return 0;
}

int smd_ctrl_iface_prepare(struct wpa_supplicant *wpa_s, char *cmd,
			   char *buf, size_t buflen)
{
	/* SMD_PREPARE <bssid> [NO_DL_SN] [NO_UL_SN] [SCS=<list>] [ADD_LINKS=<bitmask>] [DEL_LINKS=<bitmask>] */
	u8 bssid[ETH_ALEN];
	char *pos, *tmp;
	int no_dl_sn = 0;
	int no_ul_sn = 0;
	const char *scs_ids = NULL;
	struct wpa_mlo_reconfig_info reconfig_info;
	struct wpa_mlo_reconfig_info *reconfig_ptr = NULL;
	struct {
		u8 *buf[MAX_NUM_MLD_LINKS];
		size_t len[MAX_NUM_MLD_LINKS];
	} per_link_ie = { {0}, {0} };

	os_memset(&reconfig_info, 0, sizeof(reconfig_info));

	if (hwaddr_aton(cmd, bssid)) {
		return -1;
	}

	pos = os_strchr(cmd, ' ');
	while (pos) {
		while (*pos == ' ')
			pos++;
		if (*pos == '\0')
			break;

		tmp = os_strchr(pos, ' ');
		if (tmp)
			*tmp++ = '\0';

		if (os_strcasecmp(pos, "NO_DL_SN") == 0)
			no_dl_sn = 1;
		else if (os_strcasecmp(pos, "NO_UL_SN") == 0)
			no_ul_sn = 1;
		else if (os_strncasecmp(pos, "SCS=", 4) == 0)
			scs_ids = pos + 4;
		else if (os_strncasecmp(pos, "ADD_LINKS=", 10) == 0) {
			reconfig_info.add_links = (u16)strtoul(pos + 10, NULL, 0);
			reconfig_ptr = &reconfig_info;
			wpa_printf(MSG_DEBUG, "SMD: Add links bitmask: 0x%x", reconfig_info.add_links);
		} else if (os_strncasecmp(pos, "DEL_LINKS=", 10) == 0) {
			reconfig_info.delete_links = (u16)strtoul(pos + 10, NULL, 0);
			reconfig_ptr = &reconfig_info;
			wpa_printf(MSG_DEBUG, "SMD: Delete links bitmask: 0x%x", reconfig_info.delete_links);
		} else if (os_strncasecmp(pos, "LINK_IE_", 8) == 0) {
			/* Support: LINK_IE_<n>=hexblob */
			int link_id = atoi(pos + 8);
			char *eq = os_strchr(pos, '=');

			if (eq && link_id >= 0 && link_id < MAX_NUM_MLD_LINKS) {
				eq++;
				size_t hexlen = os_strlen(eq) / 2;
				u8 *buf = os_malloc(hexlen);

				if (buf && hexstr2bin(eq, buf, hexlen) == 0) {
					per_link_ie.buf[link_id] = buf;
					per_link_ie.len[link_id] = hexlen;
				} else {
					os_free(buf);
				}
			}
		} else if (os_strcasecmp(pos, "FORCE_DIFF_TX") == 0) {
			/*
			 * FORCE_DIFF_TX: boolean flag requesting that ST Prep
			 * and ST Exec be sent on different MLO links.
			 *   ST Prep -> mlo_assoc_link_id
			 *   ST Exec -> first non-assoc active link
			 *              (fallback: assoc link if none available)
			 *
			 * Example:
			 *   SMD_PREPARE aa:bb:cc:dd:ee:ff FORCE_DIFF_TX
			 */
			reconfig_info.force_diff_tx = 1;
			reconfig_ptr = &reconfig_info;
			wpa_printf(MSG_ERROR,
				   "SMD: Prep force_diff_tx set, use non-assoc link");
		}
		pos = tmp;
	}

	/* Send ST Preparation Request */
	if (wpas_uhr_link_reconfig_prep_request(wpa_s, bssid,
						no_dl_sn, no_ul_sn,
						scs_ids, reconfig_ptr,
						per_link_ie.buf, per_link_ie.len,
						MAX_NUM_MLD_LINKS, 0) < 0)
		return -1;

	/*
	 * Store force_diff_tx in the prepared target so that the auto-exec
	 * path (smd_st_roam_execute_work -> wpas_smd_request_execute) can
	 * honour it when ST Execution is triggered after PREP response.
	 */
	if (reconfig_info.force_diff_tx) {
		struct wpa_smd_prepared_target *target =
			wpas_smd_get_prepared_target(wpa_s, bssid);
		if (target) {
			target->force_diff_tx = 1;
			wpa_printf(MSG_DEBUG,
				   "SMD: stored force_diff_tx=1 for target "
				   MACSTR, MAC2STR(bssid));
		}
	}

	os_memcpy(buf, "OK\n", 3);

	return 3;
}

int smd_ctrl_iface_list_prepared(struct wpa_supplicant *wpa_s, char *buf, size_t buflen)
{
	struct wpa_smd_prepared_target *target;
	int ret;
	size_t len = 0;

	dl_list_for_each(target, &wpa_s->smd_targets,
			 struct wpa_smd_prepared_target, list) {
		ret = os_snprintf(buf + len, buflen - len,
				  MACSTR " state=%d token=%u aid=%u\n",
				  MAC2STR(target->bssid), target->state,
				  target->dialog_token, target->aid);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	return len;
}

int smd_ctrl_iface_cancel_prepare(struct wpa_supplicant *wpa_s, char *cmd,
				  char *buf, size_t buflen)
{
	/* SMD_CANCEL_PREPARE <bssid> */
	u8 bssid[ETH_ALEN];
	struct wpa_smd_prepared_target *target;

	if (hwaddr_aton(cmd, bssid)) {
		return -1;
	}

	target = wpas_smd_get_prepared_target(wpa_s, bssid);
	if (!target) {
		return -1;
	}

	wpas_smd_free_prepared_target(target);
	return 0;
}

int smd_ctrl_iface_execute(struct wpa_supplicant *wpa_s, char *cmd,
			   char *buf, size_t buflen)
{
	/* SMD_EXECUTE <bssid> [EXEC_PATH=<0|1>] [DL_TID_BITMAP=<bitmap>] */
	u8 bssid[ETH_ALEN];
	char *pos, *tmp;
	u8 exec_path = 0; /* Default: via current AP */
	u8 dl_tid_bitmap = 0xFF; /* Default: all TIDs */

	if (hwaddr_aton(cmd, bssid)) {
		return -1;
	}

	pos = os_strchr(cmd, ' ');
	while (pos) {
		while (*pos == ' ')
			pos++;
		if (*pos == '\0')
			break;

		tmp = os_strchr(pos, ' ');
		if (tmp)
			*tmp++ = '\0';

		if (os_strncasecmp(pos, "EXEC_PATH=", 10) == 0) {
			exec_path = (u8)strtoul(pos + 10, NULL, 0);
		} else if (os_strncasecmp(pos, "DL_TID_BITMAP=", 14) == 0) {
			dl_tid_bitmap = (u8)strtoul(pos + 14, NULL, 0);
		}
		pos = tmp;
	}

	/* Call ST Execution function */
	if (wpas_smd_request_execute(wpa_s, bssid, exec_path, dl_tid_bitmap) < 0)
		return -1;

	os_memcpy(buf, "OK\n", 3);
	return 3;
}

static void smd_bss_transition_work_cb(struct wpa_radio_work *work, int deinit)
{
	struct smd_bss_transition_work *ctx = work->ctx;
	struct wpa_smd_prepared_target *target = NULL;
	int no_dl_sn, no_ul_sn, max_links, preferred;
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_mlo_reconfig_info reconfig_info;
	struct wpa_mlo_reconfig_info *reconfig_ptr;
	int ret;

	if (deinit) {
		if (ctx) {
			if (!ctx->is_exec)
				wpa_s->roam_in_progress = false;
			os_free(ctx);
		}
		return;
	}

	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpa_supplicant_cancel_scan(wpa_s);

	if (ctx->is_exec) {
		target = wpas_smd_get_prepared_target(wpa_s, ctx->bssid);
		if (!target) {
			wpa_printf(MSG_ERROR,
				   "SMD: smd-bss-transition work: EXEC target "
				   MACSTR " not in PREPARED or EXEC Pending or invalid state",
				   MAC2STR(ctx->bssid));
			goto done;
		}
		/*
		 * SMD_EXECUTE path: begin transition tracking here — the same
		 * state that ROAM ST sets before auto-executing.
		 * is_preferred_target is set inside wpas_uhr_link_reconfig_exec_request.
		 */
		smd_set_state(wpa_s, SMD_STATE_TRANSITIONING);
		os_get_reltime(&wpa_s->roam_start);
		wpa_s->roam_in_progress = 1;
		os_memcpy(wpa_s->pending_bssid, ctx->bssid, ETH_ALEN);

		os_memset(&reconfig_info, 0, sizeof(reconfig_info));
		reconfig_info.is_execution_request = 1;
		reconfig_info.exec_path = ctx->exec_path;
		reconfig_info.dl_tid_bitmap = ctx->dl_tid_bitmap;
		reconfig_ptr = &reconfig_info;
		no_dl_sn = target->no_dl_sn;
		no_ul_sn = target->no_ul_sn;
		max_links = 0;
		preferred = 1;
		wpa_printf(MSG_DEBUG,
			   "SMD: smd-bss-transition work: sending ST EXEC to "
			   MACSTR " (path=%u dl_tid=0x%02x)",
			   MAC2STR(ctx->bssid), ctx->exec_path,
			   ctx->dl_tid_bitmap);
	} else {
		reconfig_ptr = ctx->has_reconfig ? &ctx->reconfig_info : NULL;
		no_dl_sn = ctx->no_dl_sn;
		no_ul_sn = ctx->no_ul_sn;
		max_links = MAX_NUM_MLD_LINKS;
		preferred = 1;
		wpa_printf(MSG_DEBUG,
			   "SMD: smd-bss-transition work: sending ST PREP to "
			   MACSTR, MAC2STR(ctx->bssid));
	}

	if (ctx->is_exec && ctx->exec_path &&
	    target->state == SMD_TARGET_PREPARED &&
	    target->state != SMD_TARGET_EXEC_PENDING) {
		/*
		 * exec_path=1 (direct to target AP): first drive all links to
		 * the TAP via wpas_smd_roam(role=3) — the kernel/driver
		 * transitions every link locally and synchronously (mirrors
		 * the auto-roam path in smd_handle_prepare_response()).  Only
		 * once the STA is actually present on the TAP links can the
		 * ST Execution Request frame be sent to it, so the OTA send
		 * below must be chained after wpas_smd_roam() succeeds rather
		 * than substituted for it.
		 */
		ret = wpas_smd_roam(wpa_s, target->target_mld_addr,
				    3, ctx->exec_path, true, true, 0);
		if (ret)
			wpa_printf(MSG_DEBUG,
				   "SMD: wpas_smd_roam failed for " MACSTR,
				   MAC2STR(target->target_mld_addr));
		else {
			target = wpas_smd_get_prepared_target(wpa_s, ctx->bssid);
			if (target) {
				target->auto_exec_path = ctx->exec_path;
				target->state = SMD_TARGET_EXEC_PENDING;
			}
			wpa_printf(MSG_DEBUG,
				   "SMD: wpas_smd_roam notified driver for , exec path: %d"
				   MACSTR, MAC2STR(target->target_mld_addr),
				   target->auto_exec_path);
		}
	} else if (ctx->is_exec) {
		ret = wpas_uhr_link_reconfig_exec_request(wpa_s, ctx->bssid,
							  no_dl_sn, no_ul_sn,
							  reconfig_ptr, preferred);
	} else {
		ret = wpas_uhr_link_reconfig_prep_request(wpa_s, ctx->bssid,
							  no_dl_sn, no_ul_sn,
							  NULL, reconfig_ptr,
							  NULL, NULL,
							  max_links, preferred);
	}

	if (ret < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD: smd-bss-transition work: %s failed",
			   ctx->is_exec ? "EXEC" : "PREP");
		/* Roll back transition tracking on failure */
		wpa_s->roam_in_progress = false;
		smd_set_state(wpa_s, SMD_STATE_ASSOCIATED);
		os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
		if (ctx->is_exec && target) {
			target->is_preferred_target = 0;
			target->state = SMD_TARGET_PREPARED; /* Keep prepared for retry */
		}
	} else if (!ctx->is_exec) {
		/* PREP succeeded: auto_exec_path stored for smd_st_roam_execute_work.
		 * is_preferred_target was already set inside wpas_uhr_link_reconfig_prep_request. */
		target = wpas_smd_get_prepared_target(wpa_s, ctx->bssid);
		if (target)
			target->auto_exec_path = ctx->exec_path;
	}

done:
	radio_work_done(work);
	os_free(ctx);
}


int wpas_smd_bss_transition(struct wpa_supplicant *wpa_s, const u8 *bssid,
			   u8 exec_path, char *buf, size_t buflen)
{
	struct smd_bss_transition_work *ctx;

	if (!wpa_s || !bssid) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for ROAM ST");
		return -1;
	}

	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpa_supplicant_cancel_scan(wpa_s);
	wpas_abort_ongoing_scan(wpa_s);

	radio_remove_works(wpa_s, "smd-bss-transition", 0);

	wpa_s->roam_in_progress = true;

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "SMD: Failed to allocate smd-bss-transition context");
		wpa_s->roam_in_progress = false;
		return -1;
	}

	os_memcpy(ctx->bssid, bssid, ETH_ALEN);
	ctx->is_exec = false;
	ctx->exec_path = exec_path;
	ctx->no_dl_sn = 1;
	ctx->no_ul_sn = 1;

	/*
	 * add_links must use TAP link IDs from the scan cache, not the SAP
	 * valid_links bitmask.  The per-STA profile Link ID field in the
	 * ST Prep Request carries target AP link IDs (IEEE 802.11bn §37.15.6.2
	 * #12144).  Using wpa_s->valid_links (SAP link IDs) builds the wrong
	 * per-STA profiles for any diff-links map scenario.
	 */
	{
		struct wpa_bss *target_bss = wpa_bss_get_bssid(wpa_s, bssid);

		if (target_bss && target_bss->valid_links) {
			os_memset(&ctx->reconfig_info, 0, sizeof(ctx->reconfig_info));
			ctx->reconfig_info.add_links = target_bss->valid_links;
			ctx->has_reconfig = true;
			wpa_printf(MSG_DEBUG,
				   "SMD: ROAM ST - ADD_LINKS=0x%x from target BSS scan (TAP link IDs)",
				   ctx->reconfig_info.add_links);
		}
		/* else: target is SLO or not yet in scan cache — single-link path */
	}

	wpa_printf(MSG_INFO,
		   "SMD: ROAM ST - target=" MACSTR
		   " exec_path=%u NO_DL_SN=%d NO_UL_SN=%d ADD_LINKS=0x%x",
		   MAC2STR(bssid), exec_path, ctx->no_dl_sn, ctx->no_ul_sn,
		   ctx->has_reconfig ? ctx->reconfig_info.add_links : 0);

	if (radio_add_work(wpa_s, 0, "smd-bss-transition", 1,
			   smd_bss_transition_work_cb, ctx) < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD: Failed to queue smd-bss-transition work");
		wpa_s->roam_in_progress = false;
		os_free(ctx);
		return -1;
	}

	/*
	 * Queue PREP requests for other SMD-member APs as fallback targets.
	 * The specified bssid is already marked preferred (is_preferred_target=1).
	 * Neighbor preps use is_preferred_target=0 — their PREP responses
	 * allocate resources without triggering auto-execute.  If the preferred
	 * target's PREP fails, wpa_supplicant can retry execute against a
	 * prepared neighbor.  The kernel cancels all neighbor preps automatically
	 * when the preferred target's EXEC response succeeds (§37.15.5.2 #7477).
	 */
	{
		struct wpa_bss *target_bss = wpa_bss_get_bssid(wpa_s, bssid);
		struct smd_group *group = target_bss ?
			smd_group_find(wpa_s, target_bss->smd_identifier) : NULL;

		if (group && group->member_count > 1) {
			struct smd_group_member *member;
			int count = 1; /* preferred already queued */
			int max = group->min_max_targets > 0 ?
				  group->min_max_targets : 1;

			dl_list_for_each(member, &group->members,
					 struct smd_group_member, list) {
				struct smd_bss_transition_work *nctx;

				if (count >= max)
					break;

				/* Skip the preferred target */
				if (os_memcmp(member->bssid, bssid,
					      ETH_ALEN) == 0 ||
				    os_memcmp(member->ap_mld_addr, bssid,
					      ETH_ALEN) == 0)
					continue;

				nctx = os_zalloc(sizeof(*nctx));
				if (!nctx)
					break;

				os_memcpy(nctx->bssid, member->bssid,
					  ETH_ALEN);
				nctx->is_exec = false;
				nctx->exec_path = exec_path;
				nctx->no_dl_sn = 1;
				nctx->no_ul_sn = 1;
				/* is_preferred_target stays 0 — no auto-exec */

				if (radio_add_work(wpa_s, 0,
						   "smd-bss-transition", 0,
						   smd_bss_transition_work_cb,
						   nctx) < 0) {
					os_free(nctx);
					break;
				}

				wpa_printf(MSG_DEBUG,
					   "SMD: ROAM ST - queued neighbor prep "
					   MACSTR " (%d/%d)",
					   MAC2STR(member->bssid),
					   count, max - 1);
				count++;
			}
		}
	}

	wpa_printf(MSG_DEBUG,
		   "SMD: ROAM ST - smd-bss-transition work queued, "
		   "waiting for radio");

	if (buf && buflen >= 3)
		os_memcpy(buf, "OK\n", 3);
	return 3;
}

void smd_targets_deinit(struct wpa_supplicant *wpa_s)
{
	struct wpa_smd_prepared_target *target, *tmp_target;

	if (!wpa_s)
		return;

	/* Cancel any pending ST roam execute work */
	eloop_cancel_timeout(smd_st_roam_execute_work, wpa_s, NULL);

	if (wpa_s->radio)
		radio_remove_works(wpa_s, "smd-bss-transition", 0);

	dl_list_for_each_safe(target, tmp_target, &wpa_s->smd_targets,
			      struct wpa_smd_prepared_target, list) {
		wpas_smd_free_prepared_target(target);
	}

	wpa_printf(MSG_DEBUG, "SMD: Prepared targets deinitialized");
}

/**
 * smd_handle_uhr_reconfig_response - Handle UHR Link Reconfiguration Response
 * @wpa_s: Pointer to wpa_supplicant data
 * @type: Response type (0 = Preparation, 1 = Execution)
 * @frame: Raw 802.11 frame
 * @frame_len: Frame length
 * @status_code: Status code from frame
 *
 * This function dispatches to the appropriate handler based on type.
 */
void smd_handle_uhr_reconfig_response(struct wpa_supplicant *wpa_s,
				      u8 type,
				      const u8 *frame, size_t frame_len,
				      u16 status_code,
				      enum nl80211_smd_link_transition_state 
				      transition_state)
{
	wpa_printf(MSG_DEBUG, "SMD: UHR Reconfig Response type=%u status=%u",
		   type, status_code);

	if (type == 0) {
		/* Type 0: ST Preparation Response */
		smd_handle_prepare_response(wpa_s, frame, frame_len, status_code,
					    transition_state);
	} else if (type == 1) {
		/* Type 1: ST Execution Response */
		smd_handle_execute_response(wpa_s, frame, frame_len, status_code,
					    transition_state);
	} else {
		wpa_printf(MSG_ERROR, "SMD: Unknown UHR Reconfig type: %u", type);
	}
}

/**
 * wpas_uhr_reconfig_resp - Handle UHR Link Reconfiguration Response
 * @wpa_s: wpa_supplicant structure
 * @resp: UHR reconfig response data
 *
 * This function handles ST Preparation Response (type=0) and ST Execution
 * Response (type=1) frames received from the kernel.
 */
void wpas_uhr_reconfig_resp(struct wpa_supplicant *wpa_s,
			    const struct uhr_reconfig_resp *resp)
{
	if (!wpa_s || !resp) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for UHR reconfig response");
		return;
	}

	/* Call the main dispatcher */
	smd_handle_uhr_reconfig_response(wpa_s, resp->type, resp->frame, 
					 resp->frame_len, resp->status_code,
					 resp->link_transition_state);
}

static void
smd_handle_transition_complete(struct wpa_supplicant *wpa_s,
			       struct wpa_smd_prepared_target *target)
{
	struct wpa_sm *sm = wpa_s->wpa;
	int i;

	/* Cancel the DL drain watchdog — the driver confirmed transition. */
	smd_cancel_drain_watchdog(wpa_s, target);

	wpa_printf(MSG_INFO,
		   "SMD: Transition complete for target " MACSTR "ptk set: %d auto exec path: %d",
		   MAC2STR(target->target_mld_addr), target->ptk_set,
		   target->auto_exec_path);

	/* Primary link PTK installation: when there are no transitioning
	 * links (single-link or all-primary case), PTK was not installed
	 * during PREP phase. Install it now that the primary link has
	 * switched to the target AP (EXEC Phase C7: Install Target Keys).
	 * This applies to both Mode 0 (reused TK) and Mode 1 (derived TK).
	 */
	/* Install PTK unless already done: exec_path=1 pre-installs it before
	 * the Exec Request, and MLO partner links install it at PREP time.
	 * IEEE 802.11bn D1.4 §37.15.7/8 requires "strictly once". */
	os_memcpy(wpa_s->bssid, target->target_mld_addr, ETH_ALEN);
	os_memcpy(wpa_s->ap_mld_addr, target->target_mld_addr, ETH_ALEN);

	wpa_printf(MSG_DEBUG,
		   "SMD: key-install: updated BSSID/AP_MLD to " MACSTR
		   " before PTK install",
		   MAC2STR(wpa_s->bssid));

	/* Query MLO info from driver BEFORE PTK/GTK install.
	 * drv->sta_mlo_info.ap_mld_addr must reflect the NEW AP so nl80211
	 * set_key (link_id=-1) can find the correct STA peer. This also
	 * ensures wpa_driver_nl80211_set_supp_port() uses the correct address.
	 */
	{
		struct driver_sta_mlo_info mlo;

		if (!wpas_drv_get_sta_mlo_info(wpa_s, &mlo)) {
			wpa_s->valid_links = mlo.valid_links;
			wpa_s->mlo_assoc_link_id = mlo.assoc_link_id;

			for (i = 0; i < SMD_MAX_LINKS; i++) {
				if (!(mlo.valid_links & BIT(i)))
					continue;

				os_memcpy(wpa_s->links[i].addr,
					  mlo.links[i].addr, ETH_ALEN);
				os_memcpy(wpa_s->links[i].bssid,
					  mlo.links[i].bssid, ETH_ALEN);
				wpa_s->links[i].freq = mlo.links[i].freq;

				/* Update link BSS from scan cache */
				wpa_s->links[i].bss =
					wpa_bss_get_bssid(wpa_s, mlo.links[i].bssid);

				wpa_printf(MSG_DEBUG,
					   "SMD: Updated MLO info valid_links=0x%04x assoc_link=%u addr:"
					 MACSTR " bssid:" MACSTR,
					wpa_s->valid_links,
					wpa_s->mlo_assoc_link_id,
					MAC2STR(wpa_s->links[i].addr),
					MAC2STR(wpa_s->links[i].bssid));
			}

			wpa_printf(MSG_DEBUG,
				   "SMD: Updated MLO info valid_links=0x%04x assoc_link=%u",
				   wpa_s->valid_links,
				   wpa_s->mlo_assoc_link_id);
		} else {
			wpa_printf(MSG_WARNING,
				   "SMD: Failed to get MLO info from driver");
		}
	}

	if (target->ptk_set) {
		wpa_printf(MSG_DEBUG,
			   "SMD: Primary link: PTK fallback install at COMPLETE for "
			   MACSTR " (mac80211 install may have already succeeded)",
			   MAC2STR(target->target_mld_addr));
		if (smd_install_target_ptk(wpa_s, target) < 0) {
			wpa_printf(MSG_DEBUG,
				   "SMD: PTK install at COMPLETE skipped/failed for "
				   MACSTR " (likely already installed by mac80211)",
				   MAC2STR(target->target_mld_addr));
			/* Non-fatal: mac80211 already installed it before T2 */
		}
	}
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);

	wpa_printf(MSG_DEBUG, "SMD: Updated BSSID/AP_MLD to " MACSTR,
		   MAC2STR(wpa_s->bssid));

	wpa_s->current_bss = wpa_bss_get_bssid(wpa_s, target->target_mld_addr);
	if (!wpa_s->current_bss) {
		wpa_printf(MSG_DEBUG,
			   "SMD: Target BSS not in scan cache (non-fatal)");
	}



	if (wpa_s->mlo_assoc_link_id < SMD_MAX_LINKS) {
		wpa_s->assoc_freq = wpa_s->links[wpa_s->mlo_assoc_link_id].freq;
		wpa_printf(MSG_DEBUG,
			   "SMD: Updated assoc_freq=%u", wpa_s->assoc_freq);
	}

	if (wpa_s->roam_start.sec || wpa_s->roam_start.usec) {
		os_reltime_age(&wpa_s->roam_start, &wpa_s->roam_time);
		wpa_printf(MSG_INFO, "SMD: Roam time %u.%06u seconds",
			   (unsigned int)wpa_s->roam_time.sec,
			   (unsigned int)wpa_s->roam_time.usec);
		wpas_notify_roam_time(wpa_s);
	}

	wpa_s->roam_start.sec = 0;
	wpa_s->roam_start.usec = 0;
	wpa_s->roam_in_progress = 0;

	/* Authorize the 802.1X port for the new AP immediately after PTK
	 * install, before GTK.  Mirrors wpa_supplicant_process_3_of_4(). */
	if (target->ptk_set)
		wpa_sm_smd_notify_ptk_installed(sm, target->target_mld_addr);

	/*
	 * Explicitly authorize the new AP in the driver.  The EAPOL SM's
	 * suppPortStatus is already Authorized from the previous connection,
	 * so portValid=true inside key_neg_complete() is a no-op and port_cb
	 * (wpa_supplicant_port_cb -> wpa_drv_set_supp_port) is never called.
	 * Call it directly here after drv->sta_mlo_info has been refreshed so
	 * the driver uses the correct (new) AP MLD address.
	 */
	wpa_drv_set_supp_port(wpa_s, 1);

	wpa_printf(MSG_DEBUG, "UHR: SMD: "
		   "state=%d is roam=%d ST execute with - target=" MACSTR,
		   target->state, wpa_s->roam_in_progress,
		   MAC2STR(target->target_mld_addr));
	if (target->auto_exec_path &&
	    target->state == SMD_TARGET_EXEC_PENDING) {
		/*
		 * ROAM ST path: auto-execute is imminent, begin transition
		 * tracking now so roam latency is measured correctly.
		 */
		target->transition_complete_pending = true;
		smd_set_state(wpa_s, SMD_STATE_TRANSITIONING);
		os_get_reltime(&wpa_s->roam_start);
		wpa_s->roam_in_progress = 1;
		os_memcpy(wpa_s->pending_bssid, target->target_mld_addr,
			  ETH_ALEN);

		wpa_printf(MSG_DEBUG, "UHR: SMD:"
			   " state=%d is roam=%d ST execute with - target=" MACSTR,
			   target->state, wpa_s->roam_in_progress,
			   MAC2STR(target->target_mld_addr));

		eloop_cancel_timeout(smd_st_roam_execute_work, wpa_s, NULL);
		eloop_register_timeout(0, 0, smd_st_roam_execute_work,
				       wpa_s, NULL);
		return;
	}

	if (target->gkd && target->gkd_len > 0) {
		u16 gtk_links = target->partner_gtk_installed
			? BIT(target->primary_link_id) : wpa_s->valid_links;

		wpa_printf(MSG_DEBUG,
			   "SMD: Installing GTK on links=0x%04x (primary=%u partner_gtk=%d)",
			   gtk_links, target->primary_link_id,
			   target->partner_gtk_installed);

		for_each_link(gtk_links, i) {
			os_memset(&sm->mlo.links[i].gtk, 0,
				  sizeof(sm->mlo.links[i].gtk));
			os_memset(&sm->mlo.links[i].gtk_wnm_sleep, 0,
				  sizeof(sm->mlo.links[i].gtk_wnm_sleep));
		}

		if (wpa_sm_install_mlo_group_keys(sm, target->gkd,
						  target->gkd_len,
						  gtk_links) < 0) {
			wpa_printf(MSG_ERROR,
				   "SMD: Failed to install GTK for links=0x%04x",
				   gtk_links);
			target->state = SMD_TARGET_FAILED;
			return;
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "SMD: No GKD stored, skipping primary link GTK");
	}

	/* Finalize: enable RX+TX pairwise protection and set WPA_COMPLETED. */
	wpa_sm_notify_smd_transition_complete(sm, target->target_mld_addr);

	target->state = SMD_TARGET_TRANSITIONED;
	smd_set_state(wpa_s, SMD_STATE_ASSOCIATED);

	wpa_msg(wpa_s, MSG_INFO,
		"SMD-TRANSITION-COMPLETE " MACSTR,
		MAC2STR(target->target_mld_addr));

	wpas_smd_free_prepared_target(target);

	wpa_printf(MSG_INFO, "SMD: BSS transition completed successfully");
}

static void
smd_handle_transition_abort(struct wpa_supplicant *wpa_s,
			    struct wpa_smd_prepared_target *target,
			    u16 status_code)
{
	wpa_printf(MSG_WARNING,
		   "SMD: Transition ABORT for target " MACSTR
		   " status=%u state=%d",
		   MAC2STR(target->target_mld_addr), status_code,
		   target->state);

	smd_cancel_execution_timeout(wpa_s, target);

	wpa_s->roam_in_progress = 0;
	wpa_s->roam_start.sec = 0;
	wpa_s->roam_start.usec = 0;
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);

	wpa_printf(MSG_DEBUG, "SMD: Cleared roaming state after abort");

	wpa_msg(wpa_s, MSG_INFO,
		"SMD-TRANSITION-ABORT " MACSTR " status=%u",
		MAC2STR(target->target_mld_addr), status_code);

	target->state = SMD_TARGET_FAILED;
	wpas_smd_free_prepared_target(target);

	/* Revert to ASSOCIATED - Still connected to current AP */
	smd_set_state(wpa_s, SMD_STATE_ASSOCIATED);

	wpa_printf(MSG_INFO,
		   "SMD: Reverted to ASSOCIATED after abort");
}

void smd_handle_transition_status(struct wpa_supplicant *wpa_s,
				  const struct st_transition *info)
{
	struct wpa_smd_prepared_target *target;

#define SMD_TRANSITION_COMPLETE 2
#define SMD_TRANSITION_ABORT	3

	if (!smd_enabled(wpa_s)) {
		wpa_printf(MSG_DEBUG,
			   "SMD: Transition status received but SMD disabled");
		return;
	}

	target = wpas_smd_get_prepared_target(wpa_s, info->target_mld_addr);
	if (!target) {
		wpa_printf(MSG_WARNING,
			   "SMD: Transition status for unknown target " MACSTR,
			   MAC2STR(info->target_mld_addr));
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "SMD: Transition status type=%u status=%u target=" MACSTR
		   " state=%d",
		   info->type, info->status_code,
		   MAC2STR(info->target_mld_addr), target->state);

	switch (info->type) {
	case SMD_TRANSITION_COMPLETE:
		/*
		 * Guard against the race where SMD-EXEC-FAILED already set
		 * target->state = SMD_TARGET_FAILED (e.g. due to group key
		 * install failure before the DL drain FW event arrives) and
		 * the kernel still delivers SMD_TRANSITION_DONE.  Calling
		 * smd_handle_transition_complete on a FAILED target causes a
		 * crash due to freed/invalid state.
		 */
		if (target->state == SMD_TARGET_FAILED) {
			wpa_printf(MSG_WARNING,
				   "SMD: TRANSITION_DONE for failed target "
				   MACSTR " — skipping complete handler",
				   MAC2STR(info->target_mld_addr));
			break;
		}
		/*
		 * exec_path=1 race: the kernel fires NL80211_CMD_SMD_TRANSITION_DONE
		 * before delivering the ST Execution Response frame.  If the EXEC
		 * response has not been processed yet (state is still EXEC_PENDING),
		 * defer the complete handler; smd_handle_execute_response() will call
		 * it once the response has been parsed.
		 */
		if (target->state == SMD_TARGET_EXEC_PENDING &&
		    !target->auto_exec_path) {
			wpa_printf(MSG_DEBUG,
				   "SMD: TRANSITION_DONE arrived before EXEC "
				   "response for " MACSTR " — deferring",
				   MAC2STR(info->target_mld_addr));
			target->transition_complete_pending = true;
			break;
		}

		smd_handle_transition_complete(wpa_s, target);
		break;
	case SMD_TRANSITION_ABORT:
		smd_handle_transition_abort(wpa_s, target,
					    info->status_code);
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "SMD: Unexpected transition type %u", info->type);
		break;
	}
}

/**
 * wpas_uhr_reconfig_resp - Handle UHR Link Reconfiguration Response
 * @wpa_s: wpa_supplicant structure
 * @resp: UHR reconfig response data
 *
 * This function handles ST Preparation Response (type=0) and ST Execution
 * Response (type=1) frames received from the kernel.
 */
void wpas_uhr_smd_handle_transition_status(struct wpa_supplicant *wpa_s,
					   const struct st_transition *info)
{
	if (!wpa_s || !info) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for UHR smd transition status");
		return;
	}

	/* Call the main dispatcher */
	smd_handle_transition_status(wpa_s, info);
}

static int smd_parse_mle(struct wpa_supplicant *wpa_s,
			 const u8 *mle_data, size_t mle_len,
			 struct wpa_smd_prepared_target *target)
{
	struct wpabuf *mlbuf;
	const u8 *pos, *end;
	u8 common_info_len;
	u16 ml_control;
	size_t rem_len;

	if (!mle_data || mle_len < 3 || !target)
		return -1;

	target->accepted_links = 0;
	mlbuf = wpabuf_alloc_copy(mle_data, mle_len);
	if (!mlbuf)
		return -1;

	pos = wpabuf_head(mlbuf);
	end = pos + wpabuf_len(mlbuf);

	ml_control = WPA_GET_LE16(pos);
	pos += 2;

	/* Per IEEE 802.11bn D1.4 §37.15.6.2, the MLE in ST Prep frames is a
	 * Reconfiguration Multi-Link element (type 2).  Accept Basic ML (type 0)
	 * as well for interoperability with implementations that follow an
	 * earlier draft. */
	if ((ml_control & MULTI_LINK_CONTROL_TYPE_MASK) !=
	    MULTI_LINK_CONTROL_TYPE_RECONF &&
	    (ml_control & MULTI_LINK_CONTROL_TYPE_MASK) !=
	    MULTI_LINK_CONTROL_TYPE_BASIC) {
		wpa_printf(MSG_ERROR,
			   "SMD: Unexpected MLE type %u in ST Prep Response "
			   "(expected Reconf=2 or Basic=0)",
			   ml_control & MULTI_LINK_CONTROL_TYPE_MASK);
		wpabuf_free(mlbuf);
		return -1;
	}
	wpa_printf(MSG_DEBUG, "SMD: MLE type %u in ST Prep Response",
		   ml_control & MULTI_LINK_CONTROL_TYPE_MASK);

	if (pos >= end) {
		wpabuf_free(mlbuf);
		return -1;
	}

	common_info_len = *pos;
	if (common_info_len < ETH_ALEN ||
	    (size_t)(end - pos) < 1 + common_info_len) {
		wpabuf_free(mlbuf);
		return -1;
	}

	os_memcpy(target->target_mld_addr, pos + 1, ETH_ALEN);
	wpa_printf(MSG_DEBUG,
		   "UHR: SMD: MLE Target AP MLD addr=" MACSTR,
		   MAC2STR(target->target_mld_addr));

	/* Skip remaining common Info fields to reach per-STA profiles */
	pos += common_info_len;
	rem_len = end - pos;

	while (rem_len > 2) {
		u8 sub_id;
		size_t subelem_defrag_len;
		int num_frag_subelems;
		u16 sta_control;
		u8 link_id;

		sub_id = pos[0];

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: sub elem id [%d]\n", sub_id);

		num_frag_subelems =
			ieee802_11_defrag_mle_subelem(mlbuf, pos,
						      &subelem_defrag_len);
		if (num_frag_subelems < 0) {
			wpa_printf(MSG_DEBUG,
				   "UHR: SMD: Failed to defrag MLE subelem");
			break;
		}

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: sub elem id [%d] num_frag_subelems: %d\n",
			   sub_id, num_frag_subelems);
		rem_len -= (size_t)num_frag_subelems * 2;

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: sub elem id [%d] num_frag_subelems: %d rem_len: %zu\n",
			   sub_id, num_frag_subelems, rem_len);
		if (rem_len < 2 + subelem_defrag_len) {
			wpa_printf(MSG_DEBUG,
				   "UHR: SMD: seems truncated");
			break;
		}

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: sub elem id [%d] num_frag_subelems: %d post rem_len: %zu\n",
			   sub_id, num_frag_subelems, rem_len);

		if (sub_id != MULTI_LINK_SUB_ELEM_ID_PER_STA_PROFILE) {
			pos += 2 + subelem_defrag_len;
			rem_len -= 2 + subelem_defrag_len;
			continue;
		}

		if (subelem_defrag_len < 2) {
			pos += 2 + subelem_defrag_len;
			rem_len -= 2 + subelem_defrag_len;
			continue;
		}

		sta_control = WPA_GET_LE16(pos + 2);
		link_id = sta_control & 0x0F;

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: sta control: 0x%02x link_id: %d\n",
			   sta_control, link_id);
		if (link_id >= SMD_MAX_LINKS) {
			wpa_printf(MSG_WARNING,
				   "SMD: Invalid link_id %u in MLE", link_id);
			pos += 2 + subelem_defrag_len;
			rem_len -= 2 + subelem_defrag_len;
			continue;
		}

		target->links[link_id].valid = true;
		target->links[link_id].link_id = link_id;
		target->prepared_links |= BIT(link_id);

		if (wpa_s->valid_links && (wpa_s->valid_links & (wpa_s->valid_links - 1)))
			target->accepted_links |= BIT(link_id);
		/* Extract link MAC Addr from STA Info */
		if (subelem_defrag_len >= 2 + 1 + ETH_ALEN) {
			u8 sta_info_len = pos[4];

			if (sta_info_len >= ETH_ALEN &&
			    (sta_control & BIT(5))) {
				/* Bit 5: STA MAC Address Present */
				os_memcpy(target->links[link_id].link_addr,
					  pos + 5, ETH_ALEN);
				wpa_printf(MSG_DEBUG,
					   "UHR: SMD: MLE Link %u addr=" MACSTR,
					   link_id,
					   MAC2STR(target->links[link_id].link_addr));
			}
		}

		pos += 2 + subelem_defrag_len;
		rem_len -= 2 + subelem_defrag_len;
	}

	wpabuf_free(mlbuf);

	wpa_printf(MSG_DEBUG,
		   "UHR: SMD: MLE Parsed - prepared link=0x%04x accepted links=0x%04x",
		   target->prepared_links,
		   target->accepted_links);

	return 0;
}

static int smd_parse_ba_info(const u8 *pos, const u8 *end,
			     struct smd_ba_info *ba)
{
	const u8 *start = pos;
	u8 tid_bitmap;
	int tid;

	if (pos >= end)
		return -1;

	tid_bitmap = *pos++;

	for (tid = 0; tid < 8; tid++) {
		if (!(tid_bitmap & BIT(tid)))
			continue;
		if (end - pos < 3)
			return -1;
		ba[tid].valid = true;
		ba[tid].block_ack_param_set = WPA_GET_LE16(pos);
		ba[tid].addba_ext_param_set = pos[2];
		pos += 3;

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: BA Info TID %d: ba_param=0x%04x addba_ext=0x%02x",
			   tid, ba[tid].block_ack_param_set,
			   ba[tid].addba_ext_param_set);
	}

	return pos - start;
}

static int smd_parse_latest_ul_sn(const u8 *data, size_t len,
				  u8 *tid_bitmap_out, u16 *latest_ul_sn)
{
	u8 tid_bitmap;
	int tid, count = 0;
	size_t bits_needed, bytes_needed;
	unsigned int bit_offset;

	if (len < 1)
		return -1;

	tid_bitmap = data[0];
	*tid_bitmap_out = tid_bitmap;
	data++;
	len--;

	for (tid = 0; tid < 8; tid++) {
		if (tid_bitmap & BIT(tid))
			count++;
	}

	bits_needed = (size_t)count * 12;
	bytes_needed = (bits_needed + 7) / 8;

	if (len < bytes_needed)
		return -1;

	/* Extract 12-bit packed SNs
	 *
	 * 12-bit values are packed contiguously in LE byte order, alternating
	 * between byte-aligned (bit_offset % 8 == 0) and niblle aligned
	 * (bit_offset % 8) == 4. Handle both cases uniformly:
	 *
	 * byte-aligned: SN = LE16[byte_idx] & 0xFFF
	 * niblle-aligned: SN = (LE16[byte_idx] >> 4) & 0xFFF
	 *
	 */
	bit_offset = 0;
	for (tid = 0; tid < 8; tid++) {
		unsigned int byte_idx;
		u16 sn;

		if (!(tid_bitmap & BIT(tid)))
			continue;

		byte_idx = bit_offset / 8;
		sn = (WPA_GET_LE16(data + byte_idx) >> (bit_offset % 8)) &
		     0xFFF;
		latest_ul_sn[tid] = sn;

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: Latest UL SN TID %d: %u",
			   tid, sn);
		bit_offset += 12;
	}

	return 0;
}

/**
 * smd_parse_bss_transition - Parse SMD BSS Transition Parameters
 * @wpa_s: Pointer to wpa_supplicant data
 * @target: Target structure
 * @ie: SMD BSS Transition IE
 * Returns: 0 on success, -1 on failure
 */
static int smd_parse_bss_transition(struct wpa_smd_prepared_target *target,
				    const u8 *ie)
{
	const u8 *pos, *end;
	size_t ie_len;
	u8 presence;
	int ret;

	if (!target || !ie)
		return -1;

	ie_len = ie[1];
	if (ie_len < 1 + 1)
		return -1;

	pos = ie + 3;
	end = ie + 2 + ie_len;

	presence = *pos++;

	if (presence & BIT(0)) {
		target->aid = WPA_GET_LE16(pos);
		pos += 2;
		wpa_printf(MSG_DEBUG, "UHR: SMD Prep Resp AID=%u", target->aid);
	}

	if (presence & BIT(1)) {
		ret = smd_parse_ba_info(pos, end, target->dl_ba);
		if (ret < 0)
			return -1;
		pos += ret;
	}

	if (presence & BIT(2)) {
		ret = smd_parse_ba_info(pos, end, target->ul_ba);
		if (ret < 0)
			return -1;
		pos += ret;
	}

	if (presence & BIT(3)) {
		u8 num_scs;

		if (pos >= end)
			return -1;
		num_scs = *pos++;
		if ((size_t)(end - pos) < num_scs)
			return -1;
		if (num_scs > sizeof(target->accepted_scs_ids))
			num_scs = sizeof(target->accepted_scs_ids);
		os_memcpy(target->accepted_scs_ids, pos, num_scs);
		target->num_accepted_scs = num_scs;
		pos += num_scs;

		wpa_printf(MSG_DEBUG,
			   "SMD: Prep resp accepted SCS count%u", num_scs);
	}

	return 0;
}

static int smd_parse_bss_transition_exec_resp(struct wpa_smd_prepared_target *target,
					      const u8 *ie)
{
	const u8 *pos, *end;
	u8 presence;
	size_t ie_len;

	if (!target || !ie)
		return -1;

	ie_len = ie[1];
	if (ie_len < 1 + 1)
		return -1;

	pos = ie + 3;
	end = ie + 2 + ie_len;

	presence = *pos++;

	if (presence & BIT(0)) {
		if (end - pos < 2)
			return -1;

		target->dl_drain_duration = WPA_GET_LE16(pos);
		target->dl_drain_duration_valid = true;
		pos += 2;
		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: Exec resp DL drain duration=%u TUs",
			   target->dl_drain_duration);
	}

	if (presence & BIT(1)) {
		if (smd_parse_latest_ul_sn(pos, end - pos,
					   &target->latest_ul_sn_tid_bitmap,
					   target->latest_ul_sn) < 0)
			return -1;
	}

	return 0;
}

/* Maximum MIC length: SHA-384 → MMM = 24 bytes; SHA-256 → MMM = 16 bytes. */
#define SMD_MIC_MAX_LEN 24

/*
 * smd_mic_len - MIC length for Per-AP MLD PTK mode (D1.4 §37.15.4.2)
 *
 * MMM = hash_output_len / 2, same rule as PASN (§12.2.11) which D1.4 references.
 * Defined locally to avoid requiring CONFIG_PASN for pasn_mic_len().
 */
static inline size_t smd_mic_len(enum rsn_hash_alg alg)
{
	switch (alg) {
	case RSN_HASH_SHA384:
		return 24; /* SHA-384 output (48 B) / 2 */
	case RSN_HASH_SHA256:
	default:
		return 16; /* SHA-256 output (32 B) / 2 */
	}
}

/**
 * smd_compute_ptk_mic - Compute SMD BSS Transition MIC
 *
 * IEEE 802.11bn D1.4 §37.15.4.2:
 *   MIC = first_MMM_octets(HMAC-HASH(PTK-KCK,
 *             AA || SPA || ANonce || SNonce [|| MLE]))
 *
 *   AA     = target AP MLD address
 *   SPA    = non-AP MLD address
 *   ANonce = nonce from non-AP MLD (our target->snonce — spec naming is
 *            the reverse of the 4-way handshake convention)
 *   SNonce = nonce from target AP MLD (our target->anonce)
 *   MLE    = full ML element bytes including EID+Length+ext_id+body
 *            (ST Prep Response only; omit for ST Exec Request)
 *   MMM    = pasn_mic_len(ptk->hash_alg) — half the HASH output length
 *
 * Uses scatter-gather HMAC to avoid a large contiguous allocation.
 * The MLE is passed as two segments (3-byte reconstructed header + body)
 * so callers supply the raw body pointer from ieee802_11_parse_elems.
 */
static int smd_compute_ptk_mic(const struct wpa_ptk *ptk,
				const u8 *aa, const u8 *spa,
				const u8 *snonce_sta, const u8 *anonce_ap,
				const u8 *mle_body, size_t mle_body_len,
				u8 *mic_out, size_t *mic_len_out)
{
	u8 mle_hdr[3];
	const u8 *addr[6];
	size_t len[6];
	size_t num_elem = 0;
	u8 hash[SHA384_MAC_LEN];
	size_t mic_len;

	if (!ptk || !aa || !spa || !snonce_sta || !anonce_ap || !mic_out)
		return -1;

	addr[num_elem] = aa;         len[num_elem] = ETH_ALEN;       num_elem++;
	addr[num_elem] = spa;        len[num_elem] = ETH_ALEN;       num_elem++;
	addr[num_elem] = snonce_sta; len[num_elem] = WPA_NONCE_LEN;  num_elem++;
	addr[num_elem] = anonce_ap;  len[num_elem] = WPA_NONCE_LEN;  num_elem++;

	if (mle_body && mle_body_len > 0) {
		/* Reconstruct [EID=255][Length=1+body_len][ext_id=107] header
		 * so the HMAC covers the complete ML element TLV as transmitted. */
		mle_hdr[0] = WLAN_EID_EXTENSION;
		mle_hdr[1] = (u8)(1 + mle_body_len);
		mle_hdr[2] = WLAN_EID_EXT_MULTI_LINK;
		addr[num_elem] = mle_hdr; len[num_elem] = sizeof(mle_hdr); num_elem++;
		addr[num_elem] = mle_body; len[num_elem] = mle_body_len;   num_elem++;
	}

	switch (ptk->hash_alg) {
	case RSN_HASH_SHA384:
		if (hmac_sha384_vector(ptk->kck, ptk->kck_len,
				       num_elem, addr, len, hash) < 0)
			return -1;
		break;
	case RSN_HASH_SHA256:
	default:
		if (hmac_sha256_vector(ptk->kck, ptk->kck_len,
				       num_elem, addr, len, hash) < 0)
			return -1;
		break;
	}

	mic_len = smd_mic_len(ptk->hash_alg);
	os_memcpy(mic_out, hash, mic_len);
	*mic_len_out = mic_len;

	forced_memzero(hash, sizeof(hash));
	return 0;
}


/**
 * smd_derive_target_ptk - Derive PTK for target AP MLD (Per-AP MLD PTK mode)
 * @wpa_s: Pointer to wpa_supplicant data
 * @target: Target preparation information
 * Returns: 0 on success, -1 on failure
 *
 * Per IEEE 802.11bn Section 37.16.5.3:
 *
 *
 * Per-SMD PTK Mode (Mode 0):
 *	The SAME PTK derived during initial SMD-ME 4-way handshake
 *	is reused across all APs in the same SMD domain. Mo new derivation
 *	needed.
 *
 * Per-AP PTK Mode (Mode 1):
 *	A fresh PTK is derived per target AP using DH shared secret.
 *	Tx Path creates: target->dh_ctx (ECDH context), target->snonce
 *	PREP response provides: ANonce, peer DH public key.
 *	DH shared secret z = crypto_ecdh_set_peerkey(dh_ctx, peer_pubkey)
 *	AA = Target MLD MAC Addr
 *	PTK = Derives from ptk formula
 *
 * Implements REQ-SEC-PTK-AP-004 from IEEE 802.11bn specification:
 * PTK = KDF(SMD_KDK, "Pairwise key expansion",
 *           Min(AA, SPA) || Max(AA, SPA) ||
 *           Min(ANonce, SNonce) || Max(ANonce, SNonce) ||
 *           DHss)
 *
 * @peer_dh_pubkey: Peer DH public key (Mode 1 only, from DH Parameter IE,
 *		    raw public key bytes without group ID prefix)
 * @peer_dh_pubkey_len: Length of peer DH public key
 */
static int smd_derive_target_ptk(struct wpa_supplicant *wpa_s,
				 struct wpa_smd_prepared_target *target,
				 const u8 *peer_dh_pubkey,
				 size_t peer_dh_pubkey_len)
{
	struct wpa_sm *sm = wpa_s->wpa;
	int ret;

	if (!sm || !target) {
		wpa_printf(MSG_ERROR, "SMD: Missing WPA SM context for PTK derivation");
		return -1;
	}

	if (wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_DOMAIN) {
		/* Per-SMD PTK Mode (Mode 0)
		 * Resuse the existing PTK from the initial SMD-ME Association
		 * The PTK was derived during the 4-way handshake with:
		 *	AA = initial SMD-ME AP MLD Address
		 *	SPA = own MLD address
		 *	SMD_ID = domain identifier
		 * Same PTK is valid for all APs in the domain
		 *
		 * Copy the TK portion for installation.
		 * No New SNonce, no deriveration, no DH
		 */
		if (!sm->ptk_set) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: No current PTK for Per-SMD PTK mode");
			return -1;
		}

		os_memcpy(&target->ptk, &sm->ptk, sizeof(struct wpa_ptk));
		target->ptk_set = true;

		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: Per-SMD PTK: reusing existing PTK (tk_len=%zu) for target "
			    MACSTR,
			   sm->ptk.tk_len,
			   MAC2STR(target->target_mld_addr));
		return 0;
	}

	/* Per-AP MLD PTK Mode (Mode 1)
	 * Derive a fresh PTK using DH shared secret.
	 */
	if (!sm->pmk_len) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: No PMK available for Mode 1 PTK derivation");
		return -1;
	}

	if (!target->dh_ctx) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: No DH Context for Per-AP MLD PTK mode (was it created in ST Prep Req?)");
		return -1;
	}

	if (!peer_dh_pubkey || peer_dh_pubkey_len == 0) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: No Peer DH public key from Prep Response");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "UHR: SMD: Mode 1 SNonce (From ST Prep Req)",
		    target->snonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "UHR: SMD: Mode 1 ANonce (From ST Prep Resp)",
		    target->anonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "UHR: SMD: Mode 1 Peer DH pubkey",
		    peer_dh_pubkey, peer_dh_pubkey_len);

	/* Compute DH shared secret */
	struct wpabuf *z;

	z = crypto_ecdh_set_peerkey(target->dh_ctx, 1,
				    peer_dh_pubkey,
				    peer_dh_pubkey_len);
	if (!z) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: DH shared secret computation failed");
		return -1;
	}

	wpa_hexdump_buf_key(MSG_DEBUG, "UHR: SMD: Mode 1 DH shared secret z", z);

	ret = wpa_pmk_to_ptk(sm->smd_kdk, sm->smd_kdk_len,
			     "Pairwise key expansion",
			     sm->own_addr, target->target_mld_addr,
			     target->snonce, target->anonce,
			     &target->ptk, sm->key_mgmt,
			     sm->pairwise_cipher,
			     wpabuf_head(z), wpabuf_len(z),
			     0, sm->smd_id);

	wpabuf_clear_free(z);

	crypto_ecdh_deinit(target->dh_ctx);
	target->dh_ctx = NULL;

	if (ret) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: Mode 1 PTK derivation failed");
		return -1;
	}

	target->ptk_set = true;

	wpa_printf(MSG_DEBUG,
		   "UHR: SMD: Mode 1 PTK derived for target " MACSTR
		   " (tk_len=%zu)",
		   MAC2STR(target->target_mld_addr), target->ptk.tk_len);

	return 0;
}

int smd_install_target_ptk(struct wpa_supplicant *wpa_s,
			   struct wpa_smd_prepared_target *target)
{
	struct wpa_sm *sm = wpa_s->wpa;
	enum wpa_alg alg;

	if (!sm || !target || !target->ptk_set)
		return -1;

	/* Determine algorithm from pairwise ciper */
	switch (sm->pairwise_cipher) {
	case WPA_CIPHER_CCMP:
		alg = WPA_ALG_CCMP;
		break;
	case WPA_CIPHER_GCMP:
		alg = WPA_ALG_GCMP;
		break;
	case WPA_CIPHER_CCMP_256:
		alg = WPA_ALG_CCMP_256;
		break;
	case WPA_CIPHER_GCMP_256:
		alg = WPA_ALG_GCMP_256;
		break;
	default:
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: Unsupported pairwise cipher %d",
			   sm->pairwise_cipher);
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR: SMD: Installing PTK for target " MACSTR
		   " (alg=%d, tk_len=%zu)",
		   MAC2STR(target->target_mld_addr), alg, target->ptk.tk_len);

	if (wpa_sm_set_key(sm, -1, alg,
			   target->target_mld_addr,
			   0, 1, NULL, 0,
			   target->ptk.tk, target->ptk.tk_len,
			   KEY_FLAG_PAIRWISE_RX_TX) < 0) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: Failed to install PTK for " MACSTR,
			   MAC2STR(target->target_mld_addr));
		return -1;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR: SMD: PTK installed for target " MACSTR,
		   MAC2STR(target->target_mld_addr));

	return 0;
}

/**
 * smd_st_roam_execute_work - Eloop work: send ST Execute for ROAM ST preferred target
 * @eloop_ctx: wpa_supplicant context
 * @timeout_ctx: unused
 *
 * Scheduled by smd_handle_prepare_response() when the ST Preparation Response
 * is received for the preferred roam target (set by ROAM ST command).
 * Decouples the Execute request from the Prep-Response handler so the
 * response frame processing completes cleanly before the Execute is sent.
 */
static void smd_st_roam_execute_work(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_smd_prepared_target *target;

	/*
	 * pending_bssid was set to the ROAM ST target address just before this
	 * work was scheduled.  Use it directly rather than scanning for any
	 * preferred target — the ROAM command is for a specific AP.
	 */
	target = wpas_smd_get_prepared_target(wpa_s, wpa_s->pending_bssid);
	if (!target) {
		wpa_printf(MSG_WARNING,
			   "SMD: ST roam execute work: " MACSTR
			   " not in PREPARED/EXEC Pending state or target is invalid",
			   MAC2STR(wpa_s->pending_bssid));
		return;
	}

	wpa_printf(MSG_INFO,
		   "SMD: ST roam execute work: sending ST Execute to "
		   MACSTR " (exec_path=%u)",
		   MAC2STR(target->bssid), target->auto_exec_path);
	if (wpas_smd_request_execute(wpa_s, target->bssid,
				     target->auto_exec_path, 0xFF) < 0)
		wpa_printf(MSG_ERROR,
			   "SMD: ST roam execute work: failed to send ST Execute");
}

/**
 * wpas_smd_roam - Issue NL80211_CMD_SMD_ROAM from supplicant context.
 * @wpa_s: supplicant instance
 * @peer_mld_addr: MLD address of the AP that is the subject of this
 *                 notification (SAP for pre-transition, TAP for
 *                 post-transition, depending on @role/@type)
 * @role: SMD role encoding expected by the driver
 *        (matches ath12k's SMD_ROAM_CONFIG_ROLE_*)
 * @type: driver-defined phase/type (matches ath12k's SMD_ROAM_CONFIG_TYPE_*)
 * @dl_sn_not_transferred: true if DL sequence numbers were NOT transferred
 * @ul_sn_not_transferred: true if UL sequence numbers were NOT transferred
 * @dl_drain_time: DL drain duration in TU, 0 if unused for this phase
 *
 * Thin wpa_supplicant-side counterpart of hostapd_smd_roam().  Builds a
 * hostapd_smd_roam_params (the struct is shared, declared in driver.h) and
 * dispatches to the shared nl80211 driver op registered by
 * wpa_driver_nl80211_ops.smd_roam.
 *
 * Returns 0 on success, -1 if the driver has no smd_roam op or the
 * command failed.
 */
int wpas_smd_roam(struct wpa_supplicant *wpa_s, const u8 *peer_mld_addr,
		  u32 role, u32 type,
		  bool dl_sn_not_transferred, bool ul_sn_not_transferred,
		  u32 dl_drain_time)
{
	struct hostapd_smd_roam_params params;

	if (!wpa_s || !peer_mld_addr || !wpa_s->driver ||
	    !wpa_s->driver->smd_roam)
		return -1;

	os_memset(&params, 0, sizeof(params));
	params.role = 3; //STA specific
	params.type = type;
	params.dl_sn_not_transferred = dl_sn_not_transferred;
	params.ul_sn_not_transferred = ul_sn_not_transferred;
	params.dl_drain_time = dl_drain_time;
	params.n_macs = 1;
	os_memcpy(params.mac[0], peer_mld_addr, ETH_ALEN);

	return wpa_s->driver->smd_roam(wpa_s->drv_priv, &params);
}

static void smd_handle_prepare_response(struct wpa_supplicant *wpa_s,
					const u8 *frame, size_t frame_len,
					u16 status_code,
					enum nl80211_smd_link_transition_state
					transition_state)
{
	u8 target_mld_addr[ETH_ALEN];
	int have_mld_addr = 0;
	u8 common_info_len;
	struct wpa_smd_prepared_target *target;
	const struct ieee80211_mgmt *mgmt;
	struct ieee802_11_elems elems;
	const u8 *smd_bss_trans;
	struct wpabuf *mlbuf;
	const u8 *ie, *ie_end;
	size_t remaining;
	const u8 *mle_for_mic = NULL;
	size_t mle_for_mic_len = 0;
	const u8 *pos;
	u8 count;

	wpa_printf(MSG_DEBUG, "SMD: Processing ST Preparation Response (UHR Link Reconfig)");

	if (frame_len < 24) {
		wpa_printf(MSG_ERROR, "SMD: Frame too short (%zu bytes)", frame_len);
		return;
	}

	mgmt = (const struct ieee80211_mgmt *)frame;

	wpa_hexdump(MSG_DEBUG, "UHR: SMD Prep Response Frame: ",
		    frame, frame_len);
	/* Parse IEs first to extract MLD address from ML element */
	/*
	 * D1.4 fixed field layout (offsets from frame[0]):
	 *   [24]    Category
	 *   [25]    Action
	 *   [26]    Dialog Token
	 *   [27]    Type (0=ST Prep, 1=ST Exec)
	 *   [28-29] Status Code  ← D1.4: new 2-octet fixed field
	 *   [30]    Count        ← D1.2 had Count at [28]
	 *   [31+]   Status Duples (Count × 3 bytes)
	 *   [31 + Count*3 +]  IEs
	 *
	 * Minimum to read Count: 24 + Cat(1)+Action(1)+Token(1)+Type(1)+
	 * StatusCode(2)+Count(1) = 31.  (D1.2 was 29.)
	 */
	if (frame_len < 31) {
		wpa_printf(MSG_ERROR,
			   "SMD: Frame too short (%zu bytes) for D1.4 action frame",
			   frame_len);
		return;
	}

	count = frame[30];           /* D1.4: Count shifted +2 vs D1.2 */
	pos = frame + 31;            /* D1.4: status list starts at 31 */
	remaining = frame_len - 31;

	if (remaining < (size_t)(count * 3)) {
		wpa_printf(MSG_ERROR,
			   "SMD: Frame too short for %u Status Duples",
			count);
		return;
	}

	/* Skip status duples to get to IEs */
	pos += count * 3;
	remaining -= count * 3;

	ie = pos;
	ie_end = frame + frame_len;

	wpa_hexdump(MSG_DEBUG, "UHR: SMD Prep Response Frame IE: ",
		    ie, ie_end - ie);
	if (ieee802_11_parse_elems(ie, ie_end - ie, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_ERROR,
			   "SMD: Failed to parse Information Elements");
	}

	wpa_printf(MSG_DEBUG, "SMD: elems: basic mle: %pX len: %zu",
		   elems.basic_mle, elems.basic_mle_len);
	/* Extract TGT MLD address from ML Basic element */
	if (elems.basic_mle && elems.basic_mle_len >= 9) {
		common_info_len = elems.basic_mle[2];
		if (common_info_len >= ETH_ALEN &&
		    elems.basic_mle_len >= 3 + ETH_ALEN) {
			os_memcpy(target_mld_addr, elems.basic_mle + 3, ETH_ALEN);
			have_mld_addr = 1;
			wpa_printf(MSG_DEBUG, "SMD: Extracted MLD address from ML element: " MACSTR,
				   MAC2STR(target_mld_addr));
		}
	}

	/* Try to get target using MLD address first, then fall back to SA */
	target = wpas_smd_get_prepared_target(wpa_s, mgmt->sa);
	if (!target && have_mld_addr) {
		wpa_printf(MSG_DEBUG,
			   "SMD: SA lookup failed, trying MLD address");
		target = wpas_smd_get_prepared_target(wpa_s, target_mld_addr);
	}

	if (!target) {
		wpa_printf(MSG_ERROR, "UHR: SMD: ST Preparation response from " MACSTR
			   " but no matching target in Tx path",
			   MAC2STR(mgmt->sa));
		return;
	}

	if (status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_WARNING, "UHR: SMD: ST Preparation rejected by " MACSTR
			   " status=%u", MAC2STR(mgmt->sa), status_code);
		if (target) {
			target->state = SMD_TARGET_FAILED;
		}
		smd_set_state(wpa_s, SMD_STATE_IDLE);
		return;
	}

	if (target->state != SMD_TARGET_PREP_PENDING) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: Prep Response in unexpected state %d(expected SMD_PREP_PREARING)",
			   target->state);
		return;
	}

	target->state = SMD_TARGET_PREP_PENDING;

	/* Accept both Reconfiguration ML (type 2, per D1.4 §37.15.6.2) and
	 * Basic ML (type 0) for interoperability.  Store a pointer to
	 * whichever is present; this is also used for the MIC computation. */
	{
		const u8 *mle_ptr;
		size_t mle_len;

		if (elems.reconf_mle && elems.reconf_mle_len >= 2) {
			mle_ptr = elems.reconf_mle;
			mle_len = elems.reconf_mle_len;
		} else if (elems.basic_mle && elems.basic_mle_len >= 2) {
			mle_ptr = elems.basic_mle;
			mle_len = elems.basic_mle_len;
		} else {
			wpa_printf(MSG_ERROR,
				   "SMD: Multi-link element missing or malformed "
				   "in ST Prep Response");
			target->state = SMD_TARGET_FAILED;
			return;
		}

		mlbuf = ieee802_11_defrag(mle_ptr, mle_len, true);
		if (!mlbuf) {
			wpa_printf(MSG_ERROR,
				   "SMD: ML element defragmentation failed");
			target->state = SMD_TARGET_FAILED;
			return;
		}
		wpa_hexdump(MSG_DEBUG, "SMD: ST Prep Resp MLE",
			    wpabuf_head(mlbuf), wpabuf_len(mlbuf));

		/* Save body pointer for MIC computation — valid for the
		 * lifetime of this function (frame buffer is in-scope). */
		mle_for_mic = mle_ptr;
		mle_for_mic_len = mle_len;
	}

	if (smd_parse_mle(wpa_s, wpabuf_head(mlbuf), wpabuf_len(mlbuf),
			  target) < 0) {
		wpa_printf(MSG_ERROR, "UHR: SMD: Failed to parse Multi-Link Element");
		wpabuf_free(mlbuf);
		target->state = SMD_TARGET_FAILED;
		return;
	}
	wpabuf_free(mlbuf);

	/* Initialize transitioning links and primary link based on current state */
	if (wpa_s->valid_links && (wpa_s->valid_links & (wpa_s->valid_links - 1))) {
		/* MLO: Multiple links active. Primary stays, others transition. */
		target->primary_link_id = wpa_s->mlo_assoc_link_id;
		target->transitioning_links = target->prepared_links & ~BIT(target->primary_link_id);
	} else {
		/* SLO: Single link active. No transitioning links during PREP. */
		target->primary_link_id = wpa_s->valid_links ? wpa_s->mlo_assoc_link_id : 0;
		target->transitioning_links = 0;
	}

	/* Extract ANonce from Nonce element (IEEE 802.11bn 9.4.2.188)
	 *
	 * The Nonce element uses WLAN_EID_EXT_FILS_NONCE (ext ID)
	 * FILS uses 16-byte nonces; SMD uses WPA_NONCE_LEN (32) bytes.
	 * ieee802_11_parse_elems() accepts elements >= FILS_NONCE_LEN (16).
	 * For Per-AP MLD PTK mode the ANonce is mandatory — abort if absent.
	 */
	if (elems.nonce && elems.nonce_len >= WPA_NONCE_LEN) {
		os_memcpy(target->anonce, elems.nonce, WPA_NONCE_LEN);
		wpa_hexdump(MSG_DEBUG, "SMD: ANonce from ST Prep Response",
			    target->anonce, WPA_NONCE_LEN);
	} else if (wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_AP) {
		wpa_printf(MSG_ERROR,
			   "SMD: Per-AP mode: ANonce missing or too short "
			   "(%u bytes, need %u) — aborting ST Prep",
			   elems.nonce ? elems.nonce_len : 0,
			   WPA_NONCE_LEN);
		target->state = SMD_TARGET_FAILED;
		return;
	}

	/* Extract peer DH public key from Diffie-Hellman Parameter element
	 * (IEEE 802.11bn D1.4 §9.4.2.312, ext ID 32).
	 *
	 * Format (elems.owe_dh points past the ext_id byte):
	 *   [0..1] = Finite Cyclic Group (LE16, e.g. 19 for NIST P-256)
	 *   [2..]  = Public Key
	 *
	 * Mandatory for Per-AP MLD PTK mode; group must match what we sent.
	 */
	if (wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_AP) {
		u16 resp_group;

		if (!elems.owe_dh || elems.owe_dh_len < 2 + 1) {
			wpa_printf(MSG_ERROR,
				   "SMD: Per-AP mode: DH Parameter element "
				   "missing or too short in ST Prep Response");
			target->state = SMD_TARGET_FAILED;
			return;
		}
		resp_group = WPA_GET_LE16(elems.owe_dh);
		if (resp_group != target->dh_group) {
			wpa_printf(MSG_ERROR,
				   "SMD: DH group mismatch in ST Prep Response: "
				   "sent %u, received %u",
				   target->dh_group, resp_group);
			target->state = SMD_TARGET_FAILED;
			return;
		}
		wpa_printf(MSG_DEBUG,
			   "SMD: DH Parameter element: group=%u pubkey_len=%zu",
			   resp_group, (size_t)(elems.owe_dh_len - 2));
	}

	/* SMD BSS Transition IE Ext ID*/
	smd_bss_trans = get_ie_ext(ie, ie_end - ie, 155);
	if (smd_bss_trans) {
		if (smd_parse_bss_transition(target, smd_bss_trans) < 0) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: Failed to parse SMD BSS Transition Parameters");
			target->state = SMD_TARGET_FAILED;
			return;
		}
	} else {
		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: No SMD BSS Transition Parameters present");
	}

	if (smd_derive_target_ptk(wpa_s, target,
				  elems.owe_dh ? elems.owe_dh + 2 : NULL,
				  elems.owe_dh ? elems.owe_dh_len - 2 : 0) < 0) {
		wpa_printf(MSG_ERROR, "SMD: PTK derivation failed");
		target->state = SMD_TARGET_FAILED;
		return;
	}

	/* Verify the MIC element from the ST Prep Response (D1.4 §37.15.4.2).
	 * Mandatory for Per-AP MLD PTK mode — a missing or invalid MIC means
	 * the DH exchange is unauthenticated and the derived PTK must not be
	 * installed.  SNonce (ours, sent in Prep Req) maps to spec "ANonce";
	 * ANonce (AP's, received in Prep Resp) maps to spec "SNonce". */
	if (wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_AP) {
		struct wpa_sm *sm = wpa_s->wpa;
		const u8 *mic_ie;
		size_t expected_mic_len;
		u8 computed_mic[SMD_MIC_MAX_LEN];
		size_t computed_mic_len;

		mic_ie = get_ie_ext(ie, ie_end - ie, WLAN_EID_EXT_MIC);
		if (!mic_ie) {
			wpa_printf(MSG_ERROR,
				   "SMD: MIC element missing in ST Prep Response "
				   "— aborting Per-AP PTK mode transition");
			forced_memzero(&target->ptk, sizeof(target->ptk));
			target->ptk_set = false;
			target->state = SMD_TARGET_FAILED;
			return;
		}

		/* mic_ie[0]=EID(255) mic_ie[1]=len mic_ie[2]=ext_id(9)
		 * mic_ie[3..] = MIC bytes */
		expected_mic_len = smd_mic_len(target->ptk.hash_alg);
		if (mic_ie[1] < (u8)(1 + expected_mic_len)) {
			wpa_printf(MSG_ERROR,
				   "SMD: MIC element too short (%u bytes, "
				   "need %zu)", mic_ie[1], 1 + expected_mic_len);
			forced_memzero(&target->ptk, sizeof(target->ptk));
			target->ptk_set = false;
			target->state = SMD_TARGET_FAILED;
			return;
		}

		if (smd_compute_ptk_mic(&target->ptk,
					target->target_mld_addr, sm->own_addr,
					target->snonce, target->anonce,
					mle_for_mic, mle_for_mic_len,
					computed_mic, &computed_mic_len) < 0) {
			wpa_printf(MSG_ERROR, "SMD: MIC computation failed");
			forced_memzero(&target->ptk, sizeof(target->ptk));
			target->ptk_set = false;
			target->state = SMD_TARGET_FAILED;
			return;
		}

		if (os_memcmp_const(computed_mic, mic_ie + 3,
				    computed_mic_len) != 0) {
			wpa_printf(MSG_ERROR,
				   "SMD: ST Prep Response MIC mismatch — "
				   "aborting transition");
			forced_memzero(&target->ptk, sizeof(target->ptk));
			target->ptk_set = false;
			target->state = SMD_TARGET_FAILED;
			return;
		}
		wpa_printf(MSG_DEBUG,
			   "SMD: ST Prep Response MIC verified OK (%zu bytes)",
			   computed_mic_len);
	}

	/* PTK installation for partner links: only when links are actually at
	 * the Target AP.  PARTIAL means prep_activate ran and VDEV/PEER state
	 * exists at the driver for those links.  PENDING means this is a
	 * non-preferred target or SLO/exec_path=0 — defer to EXEC.
	 */
	if (transition_state == NL80211_SMD_LINK_STATE_PARTIAL) {
		if (smd_install_target_ptk(wpa_s, target) < 0) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: PTK install for partner links failed");
			target->state = SMD_TARGET_FAILED;
			return;
		}
		target->partner_ptk_installed = true;
	} else {
		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: PTK derived but not installed yet "
			   "(state=%u, will install during EXEC phase)",
			   transition_state);
	}

	if (target->execution_timeout > 0) {
		if (smd_start_execution_timeout(wpa_s, target) < 0) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: Failed to start execution timeout timer");
			target->state = SMD_TARGET_FAILED;
			return;
		}
		wpa_printf(MSG_DEBUG,
			   "UHR: SMD: Execution timeout timer started (%u TUs)",
			   target->execution_timeout);
	} else {
		/* Use group default timeout */
		struct smd_group *group = smd_group_find(wpa_s, wpa_s->smd_id);

		if (group && group->min_timeout > 0) {
			target->execution_timeout = group->min_timeout;
			smd_start_execution_timeout(wpa_s, target);
		}
	}

	target->state = SMD_TARGET_PREPARED;
	os_get_reltime(&target->prep_time);

	wpa_printf(MSG_INFO, "UHR: SMD: ST Preparation successful - target=" MACSTR
		   " AID=%u links=0x%04x timeout=%u TUs ",
		   MAC2STR(target->target_mld_addr), target->aid,
		   target->prepared_links, target->execution_timeout);

	wpa_msg(wpa_s, MSG_INFO, "SMD-PREP-COMPLETE " MACSTR
		" AID=%u LINKS=0x%04x",
		MAC2STR(target->target_mld_addr), target->aid,
		target->prepared_links);

	if (!target->is_preferred_target)
		return;

	/*
	 * ROAM ST path (both exec_path=0 via SAP and exec_path=1 via TAP):
	 * auto-execute is imminent, begin transition tracking now so roam
	 * latency is measured correctly.  smd_st_roam_execute_work() calls
	 * wpas_smd_request_execute(), which queues smd_bss_transition_work_cb()
	 * — that callback drives the driver roam notification (wpas_smd_roam())
	 * for exec_path=1 AND sends the ST Execution Request frame for both
	 * exec paths.  Do not short-circuit to wpas_smd_roam() directly here:
	 * that would notify the driver but never enqueue the OTA frame.
	 */
	smd_set_state(wpa_s, SMD_STATE_TRANSITIONING);
	os_get_reltime(&wpa_s->roam_start);
	wpa_s->roam_in_progress = 1;
	os_memcpy(wpa_s->pending_bssid, target->target_mld_addr,
		  ETH_ALEN);

	wpa_printf(MSG_DEBUG, "UHR: SMD: ST Preparation - target=" MACSTR
		   " state=%d is roam=%d ST execute with - target=" MACSTR,
		   MAC2STR(target->target_mld_addr), target->state,
		   wpa_s->roam_in_progress,
		   MAC2STR(target->target_mld_addr));

	eloop_cancel_timeout(smd_st_roam_execute_work, wpa_s, NULL);
	eloop_register_timeout(0, 0, smd_st_roam_execute_work,
			       wpa_s, NULL);
	/*
	 * Non-preferred (SMD_PREPARE path): stay in ASSOCIATED state.
	 * Transition tracking begins when the user calls SMD_EXECUTE.
	 */
}

/**
 * wpas_smd_request_execute - Send ST Execution Request
 * @wpa_s: wpa_supplicant context
 * @bssid: Target BSSID (Link ID/MAC)
 * @exec_path: 0 = via current AP, 1 = via target AP
 * @dl_tid_bitmap: DL TID bitmap for traffic draining (0xFF = all TIDs)
 * Returns: 0 on success, -1 on failure
 *
 * This function implements Phase 5 of the ST Preparation process by sending
 * an ST Execution Request to initiate the actual seamless transition.
 *
 * According to IEEE 802.11bn, ST Execution Request contains:
 * - Category: 21 (Protected UHR)
 * - Action: 0 (Link Reconfiguration Request)
 * - Type: 1 (ST Execution)
 * - Reconfiguration Multi-Link Element (identifies sender & target)
 * - SMD BSS Transition Parameters (DL TID bitmap for traffic draining)
 * - Optional: OCI, DH, Nonce elements
 */
int wpas_smd_request_execute(struct wpa_supplicant *wpa_s,
			     const u8 *bssid,
			     u8 exec_path,
			     u8 dl_tid_bitmap)
{
	struct wpa_smd_prepared_target *target;
	struct smd_bss_transition_work *ctx;

	if (!wpa_s || !bssid) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for ST Execution Request");
		return -1;
	}

	/* Validate that target is prepared */
	target = wpas_smd_get_prepared_target(wpa_s, bssid);
	if (!target) {
		wpa_printf(MSG_ERROR,
			   "SMD: Cannot execute - target " MACSTR
			   " not found in prepared list",
			   MAC2STR(bssid));
		return -1;
	}

	wpa_printf(MSG_INFO,
		   "SMD: ST Execution queued for " MACSTR
		   " (path=%d dl_tid_bitmap=0x%02x)",
		   MAC2STR(bssid), exec_path, dl_tid_bitmap);

	ctx = os_zalloc(sizeof(*ctx));
	if (!ctx) {
		wpa_printf(MSG_ERROR,
			   "SMD: Failed to allocate smd-bss-transition "
			   "context for EXEC");
		return -1;
	}

	os_memcpy(ctx->bssid, bssid, ETH_ALEN);
	ctx->is_exec = true;
	ctx->exec_path = exec_path;
	target->auto_exec_path = exec_path;
	ctx->dl_tid_bitmap = dl_tid_bitmap;

	if (radio_add_work(wpa_s, 0, "smd-bss-transition", 1,
			   smd_bss_transition_work_cb, ctx) < 0) {
		wpa_printf(MSG_ERROR,
			   "SMD: Failed to queue smd-bss-transition work "
			   "for EXEC");
		os_free(ctx);
		return -1;
	}

	return 0;
}

static struct wpabuf *
smd_parse_key_delivery_element(const u8 **pos_p, size_t *remaining_p,
			       const u8 *ie_end)
{
	const u8 *pos = *pos_p;
	size_t remaining = *remaining_p;
	struct ieee802_11_elems elems;
	struct wpabuf *defrag, *result;
	size_t gkd_len;

	/* Spec-compliant path: use the standard IE parsing infrastructure.
	 * ieee802_11_parse_elems() calls ieee802_11_fragments_length() for
	 * WLAN_EID_EXT_KEY_DELIVERY, so elems.key_delivery_len reflects the
	 * full defragmented length including any Fragment elements. */
	if (ieee802_11_parse_elems(pos, ie_end - pos, &elems, 0) !=
	    ParseFailed && elems.key_delivery) {
		defrag = ieee802_11_defrag(elems.key_delivery,
					   elems.key_delivery_len,
					   true);
		if (!defrag) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: Failed to defragment Key Delivery element");
			return NULL;
		}
		if (wpabuf_len(defrag) <= WPA_KEY_RSC_LEN) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: Key Delivery element too short after defrag (%zu bytes)",
				   wpabuf_len(defrag));
			wpabuf_free(defrag);
			return NULL;
		}

		/* Strip RSC (8 bytes, SHALL be 0 per §37.15.6/37.15.7).
		 * Per-link PNs are carried inside the individual MLO KDEs. */
		result = wpabuf_alloc_copy(
			(const u8 *)wpabuf_head(defrag) + WPA_KEY_RSC_LEN,
			wpabuf_len(defrag) - WPA_KEY_RSC_LEN);
		wpabuf_free(defrag);
		return result;
	}

	/* Legacy fallback: [Length:1][KDE_blob] at fixed offset */
	if (remaining < 1 || pos[0] == WLAN_EID_EXTENSION) {
		wpa_printf(MSG_WARNING,
			   "UHR: SMD: No Key Delivery element or legacy GKD found");
		return NULL;
	}

	wpa_printf(MSG_DEBUG,
		   "UHR: SMD: No Key Delivery IE; trying legacy [Length:1][KDE_blob] format");
	gkd_len = pos[0];

	if (remaining - 1 < gkd_len) {
		wpa_printf(MSG_ERROR,
			   "UHR: SMD: Legacy GKD blob truncated (%zu > %zu)",
			   gkd_len, remaining - 1);
		return NULL;
	}

	result = wpabuf_alloc_copy(pos + 1, gkd_len);
	if (!result)
		return NULL;

	/* Advance past [Length:1][KDE_blob] so caller's ie = *pos_p
	 * points to the actual IE section. */
	*pos_p = pos + 1 + gkd_len;
	*remaining_p = remaining - 1 - gkd_len;
	return result;
}

/**
 * smd_handle_execute_response - Handle ST Execution Response (Type 1)
 */
static void smd_handle_execute_response(struct wpa_supplicant *wpa_s,
					const u8 *frame, size_t frame_len,
					u16 status_code,
					enum nl80211_smd_link_transition_state
					transition_state)
{
	struct wpa_smd_prepared_target *target;
	const struct ieee80211_mgmt *mgmt;
	const u8 *ie, *ie_end;

	wpa_printf(MSG_DEBUG, "UHR: SMD: Processing ST Execution Response");

	/* 24-byte MAC header + 5-byte action header + 1-byte count = 30 min;
	 * use 29 as the threshold (count field at [28], duples follow). */
	if (frame_len < 29) {
		wpa_printf(MSG_ERROR,
			   "SMD: Exec Response too short (%zu bytes)", frame_len);
		return;
	}

	mgmt = (const struct ieee80211_mgmt *)frame;

	wpa_hexdump(MSG_DEBUG, "SMD: ST Exec Response frame", frame, frame_len);

	/* Handle rejection */
	if (status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_ERROR,
			   "SMD: ST Exec rejected (status=%u)", status_code);

		target = wpas_smd_get_prepared_target(wpa_s, mgmt->sa);
		if (target) {
			smd_cancel_execution_timeout(wpa_s, target);
			/* For exec_path=1 the PTK was pre-installed at the driver
			 * before TX.  Remove it now — a failed transition must
			 * not leave a live key for the target AP in the table. */
			if (target->partner_ptk_installed &&
			    wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_AP) {
				struct wpa_sm *sm = wpa_s->wpa;

				wpa_printf(MSG_DEBUG,
					   "SMD: Removing pre-installed PTK for "
					   MACSTR " after exec rejection",
					   MAC2STR(target->target_mld_addr));
				wpa_sm_set_key(sm, -1, WPA_ALG_NONE,
					       target->target_mld_addr,
					       0, 0, NULL, 0, NULL, 0,
					       KEY_FLAG_PAIRWISE);
			}
			wpas_smd_free_prepared_target(target);
		}

		wpa_msg(wpa_s, MSG_INFO, "SMD-EXEC-REJECTED " MACSTR
			" status=%u", MAC2STR(mgmt->sa), status_code);
		smd_set_state(wpa_s, SMD_STATE_ASSOCIATED);
		return;
	}

	/* Look up by the frame source address; fall back to the MLD address
	 * extracted from the Exec Response ML element if present. */
	target = wpas_smd_get_prepared_target(wpa_s, mgmt->sa);
	if (!target) {
		wpa_printf(MSG_ERROR,
			   "SMD: Exec Response from unknown target " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}

	smd_cancel_execution_timeout(wpa_s, target);

	target->state = SMD_TARGET_EXEC_PENDING;
	target->exec_resp_frame = os_memdup(frame, frame_len);
	if (target->exec_resp_frame)
		target->exec_resp_frame_len = frame_len;

	/* UHR Link Reconfiguration Response frame:
	 *
	 * 29 bytes fixed header (Refer prepare handle)
	 * Count * 3 bytes status duples
	 * For type=1 : Group Key Data [length(1 byte) + KDE_blob(length)]
	 * Variable: Information Elements
	 */
	{
		const u8 *pos, *gkd_start = NULL;
		size_t remaining, gkd_len = 0;
		u8 count;
		int i;
		struct wpabuf *kde_buf = NULL;

		if (frame_len < 31) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: Exec frame too short for header");
			target->state = SMD_TARGET_FAILED;
			return;
		}

		count = frame[30];
		pos = frame + 31;
		remaining = frame_len - 31;

		if (remaining < (size_t)(count * 3)) {
			wpa_printf(MSG_ERROR,
				   "UHR: SMD: Exec frame too short for %u Status duples",
				   count);
			target->state = SMD_TARGET_FAILED;
			return;
		}

		target->accepted_links = 0;
		for (i = 0; i < count; i++) {
			u8 duple_link_id = pos[0];
			u16 duple_status = WPA_GET_LE16(pos + 1);

			wpa_printf(MSG_DEBUG,
				   "UHR: SMD: Exec status duple: link_id=%u status=%d",
				duple_link_id, duple_status);

			if (duple_link_id < MAX_NUM_MLD_LINKS &&
			    duple_status == WLAN_STATUS_SUCCESS)
				target->accepted_links |= BIT(duple_link_id);

			pos += 3;
			remaining -= 3;
		}

		/* Reconcile links with accpeted status duples */
		{
			u16 rejected_prepared =
				target->prepared_links & ~target->accepted_links;
			u16 rejected_transitioning =
				target->transitioning_links & ~target->accepted_links;
			if (rejected_prepared) {
				wpa_printf(MSG_WARNING,
					   "UHR: SMD: some prepared links rejected "
					   "prepared=0x%04x accepted=0x%04x "
					   "rejected=0x%04x",
					   target->prepared_links,
					   target->accepted_links,
					   rejected_prepared);
			}

			if (rejected_transitioning) {
				wpa_printf(MSG_WARNING,
					   "UHR: SMD: Transitioning links reduced: "
					   "0x%04x -> 0x%04x",
					   target->transitioning_links,
					   target->transitioning_links &
					   target->accepted_links);
				target->transitioning_links &= target->accepted_links;
			}
		}

		ie = pos;
		ie_end = frame + frame_len;

		if (target->accepted_links > 0) {
			kde_buf = smd_parse_key_delivery_element(
				&pos, &remaining, ie_end);
			ie = pos;
			if (!kde_buf) {
				wpa_printf(MSG_ERROR,
					   "UHR: SMD: Failed to install group keys for transitioning links 0x%04x",
					   target->accepted_links);
				target->state = SMD_TARGET_FAILED;
				wpa_msg(wpa_s, MSG_INFO,
					"SMD-EXEC-FAILED " MACSTR
					" reason=no_group_key_data",
					MAC2STR(target->target_mld_addr));
				return;
			}
		}

		/* Install group keys from Key Delivery element */
		if (kde_buf) {
			gkd_start = wpabuf_head(kde_buf);
			gkd_len   = wpabuf_len(kde_buf);

			wpa_hexdump_key(MSG_DEBUG,
					"UHR: SMD: Group Key Data (KDE list)",
					gkd_start, gkd_len);

			/* Store KDE blob; all GTKs (transitioning + primary)
			 * are installed together in smd_handle_transition_complete
			 * once the primary link has switched to the target AP. */
			target->gkd = os_memdup(gkd_start, gkd_len);
			if (target->gkd)
				target->gkd_len = gkd_len;
			else
				wpa_printf(MSG_WARNING,
					   "UHR: SMD: Failed to store GKD (non-fatal)");
			wpabuf_free(kde_buf);
		}
	}

	{
		const u8 *smd_bss_trans;

		smd_bss_trans = get_ie_ext(ie, ie_end - ie,
					   155);
		if (smd_bss_trans &&
		    smd_parse_bss_transition_exec_resp(target,
						       smd_bss_trans) < 0) {
			wpa_printf(MSG_WARNING,
				   "UHR: SMD: Failed to parse Exec BSS Transition Params (non-fatal)");
		}
	}

	if (target->dl_drain_duration_valid && target->dl_drain_duration > 0 &&
	    !target->auto_exec_path) {
		/*
		 * Install PTK for partner links if deferred from PREP
		 * (PENDING state at PREP time: non-preferred target or SLO).
		 * At DL_DRAIN, partner links are at the TAP and PTK is safe.
		 */
		if (!target->partner_ptk_installed &&
		    target->transitioning_links &&
		    (transition_state == NL80211_SMD_LINK_STATE_DL_DRAIN ||
		     transition_state == NL80211_SMD_LINK_STATE_COMPLETE)) {
			if (smd_install_target_ptk(wpa_s, target) == 0)
				target->partner_ptk_installed = true;
			else
				wpa_printf(MSG_ERROR,
					   "UHR: SMD: PTK install for partner "
					   "links failed at EXEC");
		}

		if (!target->partner_gtk_installed &&
		    target->partner_ptk_installed &&
		    target->transitioning_links &&
		    target->gkd && target->gkd_len > 0) {
			struct wpa_sm *sm = wpa_s->wpa;
			int i;

			for_each_link(target->transitioning_links, i)
				os_memset(&sm->mlo.links[i].gtk, 0,
					  sizeof(sm->mlo.links[i].gtk));
			if (wpa_sm_install_mlo_group_keys(
				    sm, target->gkd, target->gkd_len,
				    target->transitioning_links) == 0) {
				target->partner_gtk_installed = true;
				wpa_printf(MSG_INFO,
					   "UHR: SMD: Partner GTK installed "
					   "links=0x%04x (early PM=0)",
					   target->transitioning_links);
			} else {
				wpa_printf(MSG_ERROR,
					   "UHR: SMD: Partner GTK install "
					   "failed links=0x%04x",
					   target->transitioning_links);
			}
		}
		target->state = SMD_TARGET_DRAINING;
		/* Start watchdog: if the driver never sends TRANSITION_COMPLETE
		 * after the drain period, fall back to ASSOCIATED state. */
		smd_start_drain_watchdog(wpa_s, target);
		wpa_printf(MSG_INFO,
			   "UHR: SMD: ST Execution successful - target="
			   MACSTR " transitioning_links=0x%04xentering DL drain (%u TUs) ul_sn_bitmap=0x%02x",
			   MAC2STR(target->target_mld_addr),
			   target->transitioning_links,
			   target->dl_drain_duration,
			   target->latest_ul_sn_tid_bitmap);
	} else {
		/*
		 * No DL drain (exec_path=1 or transition_done_in_prep): all
		 * links are at the TAP.  Install PTK for partner links if not
		 * yet done (deferred from PREP).
		 */
		if (!target->partner_ptk_installed &&
		    target->transitioning_links &&
		    transition_state == NL80211_SMD_LINK_STATE_COMPLETE) {
			if (smd_install_target_ptk(wpa_s, target) == 0)
				target->partner_ptk_installed = true;
			else
				wpa_printf(MSG_ERROR,
					   "UHR: SMD: PTK install for partner "
					   "links failed at EXEC (no-drain)");
		}
		target->state = SMD_TARGET_TRANSITIONED;
		wpa_printf(MSG_INFO,
			   "UHR: SMD: ST Execution successful - target="
			   MACSTR " transitioning_links=0x%04x no DL drain, transitioned ul_sn_bitmap=0x%02x",
			   MAC2STR(target->target_mld_addr),
			   target->transitioning_links,
			   target->latest_ul_sn_tid_bitmap);
	}

	/*
	 * exec_path=1 deferred completion: if NL80211_CMD_SMD_TRANSITION_DONE
	 * arrived before this EXEC response was processed, fire the complete
	 * handler now that all EXEC response state has been installed.
	 */
	if (target->transition_complete_pending) {
		wpa_printf(MSG_DEBUG,
			   "SMD: Firing deferred TRANSITION_DONE for " MACSTR,
			   MAC2STR(target->target_mld_addr));
		target->transition_complete_pending = false;
		smd_handle_transition_complete(wpa_s, target);
		return;
	}

	/* Notify control interface and upper layers */
	wpa_msg(wpa_s, MSG_INFO, "SMD-EXEC-COMPLETE " MACSTR
		" TRANS_LINKS=0x%04x STATE=%s",
		MAC2STR(target->target_mld_addr),
		target->transitioning_links,
		target->state == SMD_TARGET_DRAINING ?
		"DRAINING" : "TRANSITIONED");
}

static void smd_execution_timeout_handler(void *eloop_data, void *user_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_data;
	struct wpa_smd_prepared_target *target = user_ctx;

	wpa_printf(MSG_WARNING,
		   "SMD: ST Exec timeout for " MACSTR
		   " — deleting derived PTK and context (D1.4 §37.15.9)",
		   MAC2STR(target->target_mld_addr));

	smd_set_state(wpa_s, SMD_STATE_ASSOCIATED);
	wpa_msg(wpa_s, MSG_INFO, "SMD-EXEC-TIMEOUT " MACSTR,
		MAC2STR(target->target_mld_addr));

	/* D1.4 §37.15.9: delete the derived PTK and ST Preparation context. */
	wpas_smd_free_prepared_target(target);
}

/**
 * smd_start_execution_timeout - Start execution timeout timer
 * @wpa_s: Pointer to wpa_supplicant data
 * @target: Target preparation information
 * Returns: 0 on success, -1 on failure
 *
 * Implements REQ-PREP-RESP-034 to REQ-PREP-RESP-037
 */
static int smd_start_execution_timeout(struct wpa_supplicant *wpa_s,
				       struct wpa_smd_prepared_target *target)
{
	unsigned int timeout_ms;

	if (!wpa_s || !target) {
		wpa_printf(MSG_ERROR, "SMD: Invalid parameters for execution timeout");
		return -1;
	}

	if (target->execution_timeout == 0) {
		wpa_printf(MSG_DEBUG, "SMD: No execution timeout specified for target " MACSTR,
			   MAC2STR(target->bssid));
		return 0;
	}

	/* 1 TU = 1.024 ms ~ 1 ms */
	timeout_ms = target->execution_timeout;

	eloop_cancel_timeout(smd_execution_timeout_handler, wpa_s, target);
	eloop_register_timeout(timeout_ms / 1000,
			       (timeout_ms % 1000) * 1000,
			       smd_execution_timeout_handler,
			       wpa_s, target);

	wpa_printf(MSG_DEBUG,
		   "SMD: Execution timeout timer (%u TUs) for target " MACSTR,
		   target->execution_timeout, MAC2STR(target->bssid));

	return 0;
}

static void smd_cancel_execution_timeout(struct wpa_supplicant *wpa_s,
					 struct wpa_smd_prepared_target *target)
{
	eloop_cancel_timeout(smd_execution_timeout_handler, wpa_s, target);
}


/* DL drain watchdog — fires if the driver never sends SMD_TRANSITION_COMPLETE
 * after the non-AP MLD enters SMD_TARGET_DRAINING state.  Prevents the STA
 * from being stuck in SMD_STATE_TRANSITIONING indefinitely (D1.4 §37.15.7). */
static void smd_drain_watchdog_handler(void *eloop_data, void *user_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_data;
	struct wpa_smd_prepared_target *target = user_ctx;

	wpa_printf(MSG_WARNING,
		   "SMD: DL drain watchdog fired for " MACSTR
		   " — driver did not send TRANSITION_COMPLETE; reverting",
		   MAC2STR(target->target_mld_addr));

	/* SUCCESS exec response was received but the firmware stalled during
	 * DL drain.  We cannot conclude the STA is at the target AP (D1.4
	 * §37.15.7: the link move is only complete when the driver signals
	 * TRANSITION_COMPLETE).  Undo the side-effects of entering the
	 * transition without committing any target-AP state. */

	/* 1. Remove any Pre-AP PTK pre-installed for exec_path=1.
	 *    The transition is aborted — this key must not remain live. */
	if (target->partner_ptk_installed &&
	    wpa_s->smd_ptk_mode == SMD_PTK_MODE_PER_AP) {
		struct wpa_sm *sm = wpa_s->wpa;

		wpa_printf(MSG_DEBUG,
			   "SMD: Drain watchdog: removing pre-installed PTK "
			   "for " MACSTR, MAC2STR(target->target_mld_addr));
		wpa_sm_set_key(sm, -1, WPA_ALG_NONE,
			       target->target_mld_addr,
			       0, 0, NULL, 0, NULL, 0,
			       KEY_FLAG_PAIRWISE);
	}

	/* 2. Clear in-transition wpa_supplicant flags. */
	wpa_s->roam_in_progress = false;
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);

	smd_set_state(wpa_s, SMD_STATE_ASSOCIATED);
	wpa_msg(wpa_s, MSG_INFO, "SMD-DRAIN-TIMEOUT " MACSTR,
		MAC2STR(target->target_mld_addr));
	wpas_smd_free_prepared_target(target);
}

static int smd_start_drain_watchdog(struct wpa_supplicant *wpa_s,
				    struct wpa_smd_prepared_target *target)
{
	unsigned int timeout_sec;
	unsigned int timeout_us;
	unsigned int drain_us;

	if (!wpa_s || !target || !target->dl_drain_duration_valid ||
	    target->dl_drain_duration == 0)
		return -1;

	drain_us = (unsigned int)target->dl_drain_duration * 1024;
	timeout_sec = drain_us / 1000000 + SMD_ME_TIMEOUT;
	timeout_us  = drain_us % 1000000;

	eloop_cancel_timeout(smd_drain_watchdog_handler, wpa_s, target);
	return eloop_register_timeout(timeout_sec, timeout_us,
				      smd_drain_watchdog_handler,
				      wpa_s, target);
}

static void smd_cancel_drain_watchdog(struct wpa_supplicant *wpa_s,
				      struct wpa_smd_prepared_target *target)
{
	eloop_cancel_timeout(smd_drain_watchdog_handler, wpa_s, target);
}


/**
 * smd_should_suppress_connect - Check whether SMD state forbids a new association
 * @wpa_s: Pointer to wpa_supplicant data
 * @selected: BSS candidate chosen by the BSS selection algorithm
 * Returns: 1 if the connection attempt should be suppressed, 0 otherwise
 *
 * Two SMD-specific scenarios require suppression:
 *
 * 1. TRANSITIONING — an ST Prep/Exec is in progress.  Injecting a regular
 *    association now would race against and corrupt the ST state machine.
 *    The transition has its own completion path.
 *
 * 2. ASSOCIATED — after a successful ST, wpa_s->bssid is set to the target
 *    AP MLD MAC address, while scan results expose per-link BSSIDs.  The
 *    standard ether_addr_equal(selected->bssid, wpa_s->bssid) check in
 *    wpa_supplicant_connect() would then mismatch and trigger a full
 *    re-association, destroying the SMD session.  Suppress when:
 *      a) the reassociation was not explicitly requested (wpa_s->reassociate
 *         must be 0 — explicit requests are always honoured), AND
 *      b) the selected BSS advertises the same SMD Identifier as our current
 *         domain, AND
 *      c) its BSSID is a known per-link BSSID of the current AP MLD.
 */
int smd_should_suppress_connect(struct wpa_supplicant *wpa_s,
				struct wpa_bss *selected)
{
	enum smd_state ss;
	int i;

	if (!wpa_s->smd_capable)
		return 0;

	ss = smd_get_state(wpa_s);

	if (ss == SMD_STATE_TRANSITIONING) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SMD: blocking connect request during active BSS transition");
		return 1;
	}

	if (ss != SMD_STATE_ASSOCIATED || !wpa_s->valid_links)
		return 0;

	/* An explicit reassociation request always takes precedence. */
	if (wpa_s->reassociate)
		return 0;

	/*
	 * SMD Identifier cross-check: the selected BSS must advertise the
	 * same SMD domain as our current association.  A zero smd_id on
	 * either side falls through to normal re-association logic.
	 */
	if (is_zero_ether_addr(wpa_s->smd_id) ||
	    is_zero_ether_addr(selected->smd_identifier) ||
	    os_memcmp(wpa_s->smd_id, selected->smd_identifier,
		      ETH_ALEN) != 0)
		return 0;

	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		if (!(wpa_s->valid_links & BIT(i)))
			continue;
		if (ether_addr_equal(selected->bssid, wpa_s->links[i].bssid)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SMD: selected " MACSTR
				" is link-%d BSSID of current SMD AP " MACSTR
				" (SMD-ID " MACSTR ") -- skip re-association",
				MAC2STR(selected->bssid), i,
				MAC2STR(wpa_s->bssid),
				MAC2STR(wpa_s->smd_id));
			return 1;
		}
	}

	return 0;
}
