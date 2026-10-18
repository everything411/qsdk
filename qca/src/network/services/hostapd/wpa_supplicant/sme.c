/*
 * wpa_supplicant - SME
 * Copyright (c) 2009-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "utils/eloop.h"
#include "utils/ext_password.h"
#include "common/dragonfly.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/ocv.h"
#include "common/ptksa_cache.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "common/wpa_common.h"
#include "common/sae.h"
#include "common/dpp.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/pmksa_cache.h"
#include "rsn_supp/wpa_ie.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"
#include "p2p_supplicant.h"
#include "notify.h"
#include "bss.h"
#include "bssid_ignore.h"
#include "scan.h"
#include "sme.h"
#include "hs20_supplicant.h"
#ifdef CONFIG_QCN_EXTN
#include "../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */

#define SME_AUTH_TIMEOUT 5
#define SME_ASSOC_TIMEOUT 5
#define CIP_CAPAB_LEN 4

#ifdef CONFIG_QCN_EXTN
/* Pre-connect timeout for Independent Repeater flow */
#define SME_PRE_CONNECT_TIMEOUT 8
#endif

#ifdef CONFIG_ENC_ASSOC
static const int dot11RSNAConfigPMKLifetime = 43200;
#endif /* CONFIG_ENC_ASSOC */

static void sme_auth_timer(void *eloop_ctx, void *timeout_ctx);
static void sme_assoc_timer(void *eloop_ctx, void *timeout_ctx);
static void sme_obss_scan_timeout(void *eloop_ctx, void *timeout_ctx);
static void sme_stop_sa_query(struct wpa_supplicant *wpa_s);
static struct wpabuf *cip_build_assoc_req(u8 cip_element_id_ext, u8 padding_delay);


#ifdef CONFIG_SAE

static int index_within_array(const int *array, int idx)
{
	int i;
	for (i = 0; i < idx; i++) {
		if (array[i] <= 0)
			return 0;
	}
	return 1;
}


static int sme_set_sae_group(struct wpa_supplicant *wpa_s, bool external)
{
	int *groups = wpa_s->conf->sae_groups;
	int default_groups[] = { 19, 20, 21, 0 };

	if (!groups || groups[0] <= 0)
		groups = default_groups;

	/* Configuration may have changed, so validate current index */
	if (!index_within_array(groups, wpa_s->sme.sae_group_index))
		return -1;

	for (;;) {
		int group = groups[wpa_s->sme.sae_group_index];
		if (group <= 0)
			break;
		if (!int_array_includes(wpa_s->sme.sae_rejected_groups,
					group) &&
		    sae_set_group(&wpa_s->sme.sae, group) == 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Selected SAE group %d",
				wpa_s->sme.sae.group);
			wpa_s->sme.sae.akmp = external ?
				wpa_s->sme.ext_auth_key_mgmt : wpa_s->key_mgmt;
			return 0;
		}
		wpa_s->sme.sae_group_index++;
	}

	return -1;
}


static struct wpabuf * sme_auth_build_sae_commit(struct wpa_supplicant *wpa_s,
						 struct wpa_ssid *ssid,
						 const u8 *bssid,
						 const u8 *mld_addr,
						 int external,
						 int reuse, int *ret_use_pt,
						 bool *ret_use_pk)
{
	struct wpabuf *buf;
	size_t len;
	char *password = NULL;
	struct wpa_bss *bss;
	int use_pt = 0;
	bool use_pk = false;
	u8 rsnxe_capa = 0;
	int key_mgmt = external ? wpa_s->sme.ext_auth_key_mgmt :
		wpa_s->key_mgmt;
	const u8 *addr = mld_addr ? mld_addr : bssid;
	enum sae_pwe sae_pwe;
	const u8 *password_id = (const u8 *) ssid->sae_password_id;
	size_t password_id_len = ssid->sae_password_id ?
		os_strlen(ssid->sae_password_id) : 0;

	if (ret_use_pt)
		*ret_use_pt = 0;
	if (ret_use_pk)
		*ret_use_pk = false;

#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->sae_commit_override) {
		wpa_printf(MSG_DEBUG, "SAE: TESTING - commit override");
		buf = wpabuf_alloc(4 + wpabuf_len(wpa_s->sae_commit_override));
		if (!buf)
			goto fail;
		if (!external) {
			wpabuf_put_le16(buf, 1); /* Transaction seq# */
			wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);
		}
		wpabuf_put_buf(buf, wpa_s->sae_commit_override);
		return buf;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (ssid->sae_password) {
		password = os_strdup(ssid->sae_password);
		if (!password) {
			wpa_dbg(wpa_s, MSG_INFO,
				"SAE: Failed to allocate password");
			goto fail;
		}
	}
	if (!password && ssid->passphrase) {
		password = os_strdup(ssid->passphrase);
		if (!password) {
			wpa_dbg(wpa_s, MSG_INFO,
				"SAE: Failed to allocate password");
			goto fail;
		}
	}
	if (!password && ssid->ext_psk) {
		struct wpabuf *pw = ext_password_get(wpa_s->ext_pw,
						     ssid->ext_psk);

		if (!pw) {
			wpa_msg(wpa_s, MSG_INFO,
				"SAE: No password found from external storage");
			goto fail;
		}

		password = os_malloc(wpabuf_len(pw) + 1);
		if (!password) {
			wpa_dbg(wpa_s, MSG_INFO,
				"SAE: Failed to allocate password");
			goto fail;
		}
		os_memcpy(password, wpabuf_head(pw), wpabuf_len(pw));
		password[wpabuf_len(pw)] = '\0';
		ext_password_free(pw);
	}
	if (!password) {
		wpa_printf(MSG_DEBUG, "SAE: No password available");
		goto fail;
	}

	if (reuse && wpa_s->sme.sae.tmp &&
	    ether_addr_equal(addr, wpa_s->sme.sae.tmp->bssid)) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Reuse previously generated PWE on a retry with the same AP");
		use_pt = wpa_s->sme.sae.h2e;
		use_pk = wpa_s->sme.sae.pk;
		goto reuse_data;
	}
	if (sme_set_sae_group(wpa_s, external) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to select group");
		goto fail;
	}

	bss = wpa_bss_get_bssid_latest(wpa_s, bssid);
	if (!bss) {
		wpa_printf(MSG_DEBUG,
			   "SAE: BSS not available, update scan result to get BSS");
		wpa_supplicant_update_scan_results(wpa_s, bssid);
		bss = wpa_bss_get_bssid_latest(wpa_s, bssid);
	}
	if (bss) {
		const u8 *rsnxe;

		rsnxe = wpa_bss_get_rsnxe(wpa_s, bss, ssid, false);
		if (rsnxe && rsnxe[0] == WLAN_EID_VENDOR_SPECIFIC &&
		    rsnxe[1] >= 1 + 4)
			rsnxe_capa = rsnxe[2 + 4];
		else if (rsnxe && rsnxe[1] >= 1)
			rsnxe_capa = rsnxe[2];

		/*
		 * Security Profile element preference (802.11bn D1.4, 37.32):
		 * The Extended RSN Capabilities in the Security Profile element
		 * take precedence over the legacy RSNX/RSNXO element.  Merge
		 * the security profile's RSNX capabilities into rsnxe_capa so
		 * that SAE H2E / SAE-PK checks below use the security profile
		 * values.
		 */
		if (wpas_security_profile_active(wpa_s)) {
			const u8 *sp_ie_rsnx;
			size_t sp_rsnx_len;
			const u8 *sp = wpa_bss_get_ie_ext(
				bss, WLAN_EID_EXT_SECURITY_PROFILE);

			sp_ie_rsnx = security_profile_ie_get_rsnx(
				sp, &sp_rsnx_len);
			if (sp_ie_rsnx && sp_rsnx_len >= 1)
				rsnxe_capa |= sp_ie_rsnx[0];
		}
	}

	sae_pwe = wpas_get_ssid_sae_pwe(wpa_s, ssid);

	if (ssid->sae_password_id &&
	    sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		use_pt = 1;
	if (wpa_key_mgmt_sae_ext_key(key_mgmt) &&
	    sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		use_pt = 1;
	if (bss && is_6ghz_freq(bss->freq) &&
	    sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		use_pt = 1;
#ifdef CONFIG_SAE_PK
	if ((rsnxe_capa & BIT(WLAN_RSNX_CAPAB_SAE_PK)) &&
	    ssid->sae_pk != SAE_PK_MODE_DISABLED &&
	    ((ssid->sae_password &&
	      sae_pk_valid_password(ssid->sae_password)) ||
	     (!ssid->sae_password && ssid->passphrase &&
	      sae_pk_valid_password(ssid->passphrase)))) {
		use_pt = 1;
		use_pk = true;
	}

	if (ssid->sae_pk == SAE_PK_MODE_ONLY && !use_pk) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Cannot use PK with the selected AP");
		goto fail;
	}
#endif /* CONFIG_SAE_PK */

	if (use_pt || sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
	    sae_pwe == SAE_PWE_BOTH) {
		use_pt = !!(rsnxe_capa & BIT(WLAN_RSNX_CAPAB_SAE_H2E));

		if ((sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
		     ssid->sae_password_id ||
		     wpa_key_mgmt_sae_ext_key(key_mgmt)) &&
		    sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK &&
		    !use_pt) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Cannot use H2E with the selected AP");
			goto fail;
		}
	}

	if (use_pt && !ssid->pt)
		wpa_s_setup_sae_pt(wpa_s, ssid, true);
	if (use_pt &&
	    sae_prepare_commit_pt(&wpa_s->sme.sae, ssid->pt,
				  wpa_s->own_addr, addr,
				  wpa_s->sme.sae_rejected_groups, NULL) < 0)
		goto fail;
	if (!use_pt &&
	    sae_prepare_commit(wpa_s->own_addr, addr,
			       (u8 *) password, os_strlen(password),
			       &wpa_s->sme.sae) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Could not pick PWE");
		goto fail;
	}
	if (wpa_s->sme.sae.tmp) {
		os_memcpy(wpa_s->sme.sae.tmp->bssid, addr, ETH_ALEN);
		if (use_pt && use_pk)
			wpa_s->sme.sae.pk = 1;
#ifdef CONFIG_SAE_PK
		os_memcpy(wpa_s->sme.sae.tmp->own_addr, wpa_s->own_addr,
			  ETH_ALEN);
		os_memcpy(wpa_s->sme.sae.tmp->peer_addr, addr, ETH_ALEN);
		sae_pk_set_password(&wpa_s->sme.sae, password);
#endif /* CONFIG_SAE_PK */
	}

reuse_data:
	len = wpa_s->sme.sae_token ? 3 + wpabuf_len(wpa_s->sme.sae_token) : 0;
	if (ssid->sae_password_id)
		len += 4 + os_strlen(ssid->sae_password_id);
	buf = wpabuf_alloc(4 + SAE_COMMIT_MAX_LEN + len);
	if (buf == NULL)
		goto fail;
	if (!external) {
		wpabuf_put_le16(buf, 1); /* Transaction seq# */
		if (use_pk)
			wpabuf_put_le16(buf, WLAN_STATUS_SAE_PK);
		else if (use_pt)
			wpabuf_put_le16(buf, WLAN_STATUS_SAE_HASH_TO_ELEMENT);
		else
			wpabuf_put_le16(buf,WLAN_STATUS_SUCCESS);
	}

	if (use_pt && ssid->pt && ssid->pt->password_id) {
		password_id = wpabuf_head(ssid->pt->password_id);
		password_id_len = wpabuf_len(ssid->pt->password_id);
	}
	if (sae_write_commit(&wpa_s->sme.sae, buf, wpa_s->sme.sae_token,
			     password_id, password_id_len) < 0) {
		wpabuf_free(buf);
		goto fail;
	}
	if (ret_use_pt)
		*ret_use_pt = use_pt;
	if (ret_use_pk)
		*ret_use_pk = use_pk;

	str_clear_free(password);
	return buf;

fail:
	str_clear_free(password);
	return NULL;
}


static struct wpabuf * sme_auth_build_sae_confirm(struct wpa_supplicant *wpa_s,
						  int external)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(4 + SAE_CONFIRM_MAX_LEN);
	if (buf == NULL)
		return NULL;

	if (!external) {
		wpabuf_put_le16(buf, 2); /* Transaction seq# */
		wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);
	}
	sae_write_confirm(&wpa_s->sme.sae, buf);

	return buf;
}

#endif /* CONFIG_SAE */


/**
 * sme_auth_handle_rrm - Handle RRM aspects of current authentication attempt
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: Pointer to the bss which is the target of authentication attempt
 */
static void sme_auth_handle_rrm(struct wpa_supplicant *wpa_s,
				struct wpa_bss *bss)
{
	const u8 rrm_ie_len = 5;
	u8 *pos;
	const u8 *rrm_ie;

	wpa_s->rrm.rrm_used = 0;

	wpa_printf(MSG_DEBUG,
		   "RRM: Determining whether RRM can be used - device support: 0x%x",
		   wpa_s->drv_rrm_flags);

	rrm_ie = wpa_bss_get_ie(bss, WLAN_EID_RRM_ENABLED_CAPABILITIES);
	if (!rrm_ie || !(bss->caps & IEEE80211_CAP_RRM)) {
		wpa_printf(MSG_DEBUG, "RRM: No RRM in network");
		return;
	}

	if (!((wpa_s->drv_rrm_flags &
	       WPA_DRIVER_FLAGS_DS_PARAM_SET_IE_IN_PROBES) &&
	      (wpa_s->drv_rrm_flags & WPA_DRIVER_FLAGS_QUIET)) &&
	    !(wpa_s->drv_rrm_flags & WPA_DRIVER_FLAGS_SUPPORT_RRM)) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Insufficient RRM support in driver - do not use RRM");
		return;
	}

	if (sizeof(wpa_s->sme.assoc_req_ie) <
	    wpa_s->sme.assoc_req_ie_len + rrm_ie_len + 2) {
		wpa_printf(MSG_INFO,
			   "RRM: Unable to use RRM, no room for RRM IE");
		return;
	}

	wpa_printf(MSG_DEBUG, "RRM: Adding RRM IE to Association Request");
	pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
	os_memset(pos, 0, 2 + rrm_ie_len);
	*pos++ = WLAN_EID_RRM_ENABLED_CAPABILITIES;
	*pos++ = rrm_ie_len;

	/* Set supported capabilities flags */
	if (wpa_s->drv_rrm_flags & WPA_DRIVER_FLAGS_TX_POWER_INSERTION)
		*pos |= WLAN_RRM_CAPS_LINK_MEASUREMENT;

	*pos |= WLAN_RRM_CAPS_BEACON_REPORT_PASSIVE |
		WLAN_RRM_CAPS_BEACON_REPORT_ACTIVE |
		WLAN_RRM_CAPS_BEACON_REPORT_TABLE;

	*pos |= WLAN_RRM_CAPS_CHANNEL_LOAD;
	pos[1] |= WLAN_RRM_CAPS_NOISE_HISTOGRAM;

	if (wpa_s->lci)
		pos[1] |= WLAN_RRM_CAPS_LCI_MEASUREMENT;

	wpa_s->sme.assoc_req_ie_len += rrm_ie_len + 2;
	wpa_s->rrm.rrm_used = 1;
}


static void wpas_ml_handle_removed_links(struct wpa_supplicant *wpa_s,
					 struct wpa_bss *bss)
{
	u16 removed_links = wpa_bss_parse_reconf_ml_element(wpa_s, bss);

	wpa_s->valid_links &= ~removed_links;
}


#ifdef CONFIG_TESTING_OPTIONS
static struct wpa_bss * wpas_ml_connect_pref(struct wpa_supplicant *wpa_s,
					     struct wpa_bss *bss,
					     struct wpa_ssid *ssid)
{
	unsigned int low, high, i;

	wpa_printf(MSG_DEBUG,
		   "MLD: valid_links=%d, band_pref=%u, bssid_pref=" MACSTR,
		   wpa_s->valid_links,
		   wpa_s->conf->mld_connect_band_pref,
		   MAC2STR(wpa_s->conf->mld_connect_bssid_pref));

	/* Check if there are more than one link */
	if (!(wpa_s->valid_links & (wpa_s->valid_links - 1)))
		return bss;

	if (!is_zero_ether_addr(wpa_s->conf->mld_connect_bssid_pref)) {
		for_each_link(wpa_s->valid_links, i) {
			if (wpa_s->mlo_assoc_link_id == i)
				continue;

			if (ether_addr_equal(
				    wpa_s->links[i].bssid,
				    wpa_s->conf->mld_connect_bssid_pref))
				goto found;
		}
	}

	if (wpa_s->conf->mld_connect_band_pref == MLD_CONNECT_BAND_PREF_AUTO)
		return bss;

	switch (wpa_s->conf->mld_connect_band_pref) {
	case MLD_CONNECT_BAND_PREF_2GHZ:
		low = 2412;
		high = 2472;
		break;
	case MLD_CONNECT_BAND_PREF_5GHZ:
		low = 5180;
		high = 5985;
		break;
	case MLD_CONNECT_BAND_PREF_6GHZ:
		low = 5955;
		high = 7125;
		break;
	default:
		return bss;
	}

	for_each_link(wpa_s->valid_links, i) {
		if (wpa_s->mlo_assoc_link_id == i)
			continue;

		if (wpa_s->links[i].freq >= low && wpa_s->links[i].freq <= high)
			goto found;
	}

found:
	if (i == MAX_NUM_MLD_LINKS) {
		wpa_printf(MSG_DEBUG, "MLD: No match for connect/band pref");
		return bss;
	}

	wpa_printf(MSG_DEBUG,
		   "MLD: Change BSS for connect: " MACSTR " -> " MACSTR,
		   MAC2STR(wpa_s->links[wpa_s->mlo_assoc_link_id].bssid),
		   MAC2STR(wpa_s->links[i].bssid));

	/* Get the BSS entry and do the switch */
	if (ssid && ssid->ssid_len)
		bss = wpa_bss_get(wpa_s, wpa_s->links[i].bssid, ssid->ssid,
				  ssid->ssid_len);
	else
		bss = wpa_bss_get_bssid(wpa_s, wpa_s->links[i].bssid);
	wpa_s->mlo_assoc_link_id = i;

	return bss;
}
#endif /* CONFIG_TESTING_OPTIONS */


static int wpas_sme_ml_auth(struct wpa_supplicant *wpa_s,
			    union wpa_event_data *data,
			    int ie_offset)
{
	struct ieee802_11_elems elems;
	const u8 *mld_addr;
	u16 status_code = data->auth.status_code;

	if (!wpa_s->valid_links)
		return 0;

	if (ieee802_11_parse_elems(data->auth.ies + ie_offset,
				   data->auth.ies_len - ie_offset,
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MLD: Failed parsing elements");
		return -1;
	}

	if (!elems.basic_mle || !elems.basic_mle_len) {
		wpa_printf(MSG_DEBUG, "MLD: No ML element in authentication");
		if (status_code == WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ ||
		    status_code == WLAN_STATUS_SUCCESS ||
		    status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		    status_code == WLAN_STATUS_SAE_PK)
			return -1;
		/* Accept missing Multi-Link element in failed authentication
		 * cases. */
		return 0;
	}

	mld_addr = get_basic_mle_mld_addr(elems.basic_mle, elems.basic_mle_len);
	if (!mld_addr)
		return -1;

	wpa_printf(MSG_DEBUG, "MLD: mld_address=" MACSTR, MAC2STR(mld_addr));

	if (!ether_addr_equal(wpa_s->ap_mld_addr, mld_addr)) {
		wpa_printf(MSG_DEBUG, "MLD: Unexpected MLD address (expected "
			   MACSTR ")", MAC2STR(wpa_s->ap_mld_addr));
		return -1;
	}

	return 0;
}


static void wpas_sme_set_mlo_links(struct wpa_supplicant *wpa_s,
				   struct wpa_bss *bss, struct wpa_ssid *ssid,
				   u16 *missing_links)
{
	u16 usable_links;
	u8 i;

	wpas_reset_mlo_info(wpa_s);

	if (!(wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_MLO))
		return;

	usable_links = wpa_bss_get_usable_links(wpa_s, bss, ssid, missing_links);
	if (!usable_links)
		return;

	os_memcpy(wpa_s->ap_mld_addr, bss->mld_addr, ETH_ALEN);
	wpa_s->valid_links = 0;
	wpa_s->mlo_assoc_link_id = bss->mld_link_id;

	for_each_link(usable_links, i) {
		const u8 *bssid = bss->mld_links[i].bssid;

		wpa_s->valid_links |= BIT(i);
		os_memcpy(wpa_s->links[i].bssid, bssid, ETH_ALEN);
		wpa_s->links[i].freq = bss->mld_links[i].freq;
		wpa_s->links[i].disabled = bss->mld_links[i].disabled;
		wpabuf_free(wpa_s->links[i].ies);
		wpa_s->links[i].ies = NULL;
#ifdef CONFIG_TESTING_OPTIONS
		if (wpa_s->link_ies[i])
			wpa_s->links[i].ies = wpabuf_dup(wpa_s->link_ies[i]);
#endif /* CONFIG_TESTING_OPTIONS */

		if (bss->mld_link_id == i)
			wpa_s->links[i].bss = bss;
		else if (ssid && ssid->ssid_len)
			wpa_s->links[i].bss = wpa_bss_get(wpa_s, bssid,
							  ssid->ssid,
							  ssid->ssid_len);
		else
			wpa_s->links[i].bss = wpa_bss_get_bssid(wpa_s, bssid);
	}

	wpa_printf(MSG_DEBUG,
		   "MLO-DBG: set MLO links for (re)assoc: assoc_link_id=%d requested valid_links=0x%x usable_links=0x%x missing_links=0x%x",
		   wpa_s->mlo_assoc_link_id, wpa_s->valid_links, usable_links,
		   missing_links ? *missing_links : 0);
}


static void sme_add_assoc_req_ie(struct wpa_supplicant *wpa_s,
				 const struct wpabuf *buf)
{
	size_t len;
	u8 *pos, *end;

	if (!buf)
		return;

	pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
	end = wpa_s->sme.assoc_req_ie + sizeof(wpa_s->sme.assoc_req_ie);
	if (pos >= end)
		return;

	len = wpabuf_len(buf);
	if (len < (size_t) (end - pos)) {
		os_memcpy(pos, wpabuf_head(buf), len);
		wpa_s->sme.assoc_req_ie_len += len;
	}
}


#ifdef CONFIG_ENC_ASSOC

static bool wpas_eppke_ap_capable(struct wpa_supplicant *wpa_s,
				  struct wpa_bss *bss)
{
	const u8 *ap_rsnxe;

	if (!(wpa_s->drv_flags2 &
	      WPA_DRIVER_FLAGS2_ASSOCIATION_FRAME_ENCRYPTION)) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: Driver does not support association frame encryption");
		return false;
	}

	ap_rsnxe = wpa_bss_get_rsnxe(wpa_s, bss, NULL, false);

	if (!ieee802_11_rsnx_capab(ap_rsnxe, WLAN_RSNX_CAPAB_KEK_IN_PASN)) {
		wpa_printf(MSG_DEBUG, "EPPKE: AP does not support KEK_IN_PASN");
		return false;
	}

	if (!ieee802_11_rsnx_capab(ap_rsnxe,
				   WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION)) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: AP does not support association frame encryption");
		return false;
	}

	return true;
}


static int wpas_eppke_initialize(struct wpa_supplicant *wpa_s,
				 struct wpa_bss *bss,
				 struct wpa_ssid *ssid)
{
	struct pasn_data *pasn;
	const u8 *ap_rsne, *ap_rsnxe;
	u8 ap_rsne_len, ap_rsnxe_len;
	u32 capab = 0;
	int group;
	bool derive_kdk;
	u8 rsne[80];
	size_t rsne_len = sizeof(rsne);
	int len;

	pasn = &wpa_s->pasn;

	group = wpas_pasn_get_group(wpa_s, ssid, pasn);
	if (!group) {
		wpa_printf(MSG_DEBUG, "EPPKE: No suitable group available");
		return -1;
	}

	/* Reset PASN data before initialization to clear state from any
	 * previous attempt, e.g., on group rejection retries.
	 */
	wpa_pasn_reset(pasn);

	/* As per IEEE P802.11-REVmf/D2.1, 12.13.5, when SAE is wrapped within
	 * PASN authentication, both shall use the same finite cyclic group.
	 * SAE-level group rejection validation is not required.
	 */
	if (sae_set_group(&wpa_s->sme.sae, group) < 0) {
		wpa_printf(MSG_INFO, "EPPKE: Failed to set SAE group %u",
			   group);
		return -1;
	}

	ap_rsne = wpa_bss_get_rsne(wpa_s, bss, NULL, false);
	if (!ap_rsne) {
		wpa_printf(MSG_DEBUG, "EPPKE: Can't connect without AP RSNE");
		return -1;
	}

	ap_rsnxe = wpa_bss_get_rsnxe(wpa_s, bss, NULL, false);

	ap_rsne_len = *(ap_rsne + 1) + 2;
	ap_rsnxe_len = ap_rsnxe ? *(ap_rsnxe + 1) + 2 : 0;
	if (ap_rsne && ap_rsne_len) {
		wpabuf_free(pasn->beacon_rsne_rsnxe);
		pasn->beacon_rsne_rsnxe = wpabuf_alloc(ap_rsne_len +
						       ap_rsnxe_len);
		if (!pasn->beacon_rsne_rsnxe) {
			wpa_printf(MSG_INFO,
				   "EPPKE: Failed storing AP's RSNE/RSNXE");
			return -1;
		}

		wpabuf_put_data(pasn->beacon_rsne_rsnxe, ap_rsne, ap_rsne_len);
		if (ap_rsnxe && ap_rsnxe_len)
			wpabuf_put_data(pasn->beacon_rsne_rsnxe,
					ap_rsnxe, ap_rsnxe_len);
	}

	if (!ieee802_11_rsnx_capab(ap_rsnxe, WLAN_RSNX_CAPAB_KEK_IN_PASN)) {
		wpa_printf(MSG_DEBUG, "EPPKE: AP does not support KEK_IN_PASN");
		goto fail;
	}

	if (!ieee802_11_rsnx_capab(ap_rsnxe,
				   WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION)) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: AP does not support association frame encryption");
		goto fail;
	}

	if (!(wpa_s->drv_flags2 &
	      WPA_DRIVER_FLAGS2_ASSOCIATION_FRAME_ENCRYPTION)) {
		wpa_printf(MSG_INFO,
			   "EPPKE: Cannot use EPPKE without support for association frame encryption");
		goto fail;
	}

	capab |= BIT(WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION);
	capab |= BIT(WLAN_RSNX_CAPAB_KEK_IN_PASN);
#ifdef CONFIG_PMKSA_PRIVACY
	if (ssid->pmksa_privacy)
		capab |= BIT(WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY);
#endif /* CONFIG_PMKSA_PRIVACY */
	pasn->derive_kek = true;

	if (0) {
#ifdef CONFIG_SAE
	} else if (wpa_key_mgmt_sae_ext_key(ssid->key_mgmt)) {
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
		if (!ieee802_11_rsnx_capab(ap_rsnxe, WLAN_RSNX_CAPAB_SAE_H2E)) {
			wpa_printf(MSG_DEBUG,
				   "EPPKE: AP does not support SAE H2E");
			goto fail;
		}
		if (pasn->pt)
			sae_deinit_pt(pasn->pt);
		pasn_set_pt(pasn, wpas_pasn_sae_derive_pt_for_eppke(ssid,
								    group));
		if (!pasn->pt) {
			wpa_printf(MSG_DEBUG, "EPPKE: Failed to derive PT");
			goto fail;
		}
		pasn->sae.state = SAE_NOTHING;
		pasn->sae.send_confirm = 0;
#endif /* CONFIG_SAE */
	} else {
		wpa_msg(wpa_s, MSG_INFO,
			"EPPKE: Suitable base AKM not enabled in local configuration");
		goto fail;
	}

	derive_kdk = (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA) &&
		ieee802_11_rsnx_capab(ap_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF);
#ifdef CONFIG_TESTING_OPTIONS
	if (!derive_kdk)
		derive_kdk = wpa_s->conf->force_kdk_derivation;
#endif /* CONFIG_TESTING_OPTIONS */
	if (derive_kdk)
		pasn_enable_kdk_derivation(pasn);
	else
		pasn_disable_kdk_derivation(pasn);

	wpa_printf(MSG_DEBUG, "PASN: kdk_len=%zu", pasn->kdk_len);

	if ((wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA) &&
	    ieee802_11_rsnx_capab(ap_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF))
		pasn->secure_ltf = true;
	else
		pasn->secure_ltf = false;

#ifdef CONFIG_TESTING_OPTIONS
	pasn->corrupt_mic = wpa_s->conf->pasn_corrupt_mic;
#endif /* CONFIG_TESTING_OPTIONS */

	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_RTT_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_STA) {
		/*
		 * URNM_MFPR_X20 is a subset of URNM_MFPR which excludes 20 MHz
		 * bandwidth from mandating protected Management frames. Set
		 * URNM_MFPR only when URNM_MFPR_X20 is not set.
		 */
		if (wpa_s->disable_urnm_mfpr) {
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR, 0);
		} else {
			capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR);
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR, 1);
		}
		if (wpa_s->urnm_mfpr_x20) {
			capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR_X20);
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR_X20,
					 1);
		} else {
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR_X20,
					 0);
		}
	}
	if ((wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SPP_AMSDU) &&
	    ieee802_11_rsnx_capab(ap_rsnxe, WLAN_RSNX_CAPAB_SPP_A_MSDU))
		capab |= BIT(WLAN_RSNX_CAPAB_SPP_A_MSDU);

	pasn_set_rsnxe_caps(pasn, capab);
	pasn_set_initiator_pmksa(pasn, wpa_sm_get_pmksa_cache(wpa_s->wpa));

	if (pasn->ecdh)
		crypto_ecdh_deinit(pasn->ecdh);
	pasn->ecdh = crypto_ecdh_init(group);
	if (!pasn->ecdh) {
		wpa_printf(MSG_INFO, "EPPKE: Failed to init ECDH");
		goto fail;
	}

	len = wpa_gen_wpa_ie(wpa_s->wpa, rsne, rsne_len);
	if (len < 0) {
		wpa_printf(MSG_INFO, "EPPKE: Failed to generate RSNE");
		goto fail;
	}
	pasn_set_rsne(pasn, rsne);
	wpa_hexdump(MSG_DEBUG, "EPPKE: Set own RSNE default",
		    pasn->rsn_ie, pasn->rsn_ie_len);
	pasn->akmp = wpa_s->key_mgmt;
	pasn->cipher = wpa_s->pairwise_cipher;
	pasn->group = group;
	pasn->freq = bss->freq;
	pasn->auth_alg = WLAN_AUTH_EPPKE;

	os_memcpy(pasn->own_addr, wpa_s->own_addr, ETH_ALEN);
	if ((wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_MLO) &&
	    wpa_s->valid_links) {
		pasn->is_ml_peer = true;
		os_memcpy(pasn->peer_addr, wpa_s->ap_mld_addr, ETH_ALEN);
	} else {
		os_memcpy(pasn->peer_addr, bss->bssid, ETH_ALEN);
	}
	os_memcpy(pasn->bssid, bss->bssid, ETH_ALEN);

	wpa_printf(MSG_DEBUG,
		   "PASN: Init: " MACSTR " akmp=0x%x, cipher=0x%x, group=%u",
		   MAC2STR(pasn->peer_addr), pasn->akmp,
		   pasn->cipher, pasn->group);

	return 0;

fail:
	wpa_pasn_reset(pasn);
	return -1;
}

#endif /* CONFIG_ENC_ASSOC */


static void sme_send_authentication(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss, struct wpa_ssid *ssid,
				    int start)
{
	struct wpa_driver_auth_params params;
	struct wpa_ssid *old_ssid;
#ifdef CONFIG_IEEE80211R
	const u8 *ie;
#endif /* CONFIG_IEEE80211R */
#if defined(CONFIG_IEEE80211R) || defined(CONFIG_FILS)
	const u8 *md = NULL;
#endif /* CONFIG_IEEE80211R || CONFIG_FILS */
	int bssid_changed;
	struct wpabuf *resp = NULL;
	u8 ext_capab[18];
	int ext_capab_len;
	int skip_auth;
	u8 *wpa_ie;
	size_t wpa_ie_len;
#ifdef CONFIG_MBO
	const u8 *mbo_ie;
#endif /* CONFIG_MBO */
	int omit_rsnxe = 0;
	u16 missing_links = 0;
	u8 *auth_ie_buf = NULL;

	if (bss == NULL) {
		wpa_msg(wpa_s, MSG_ERROR, "SME: No scan result available for "
			"the network");
		wpas_connect_work_done(wpa_s);
		return;
	}

	os_memset(&params, 0, sizeof(params));

	/*
	 * Reset the ML neighbor retry counter whenever this is a genuinely
	 * new connection attempt (different target BSSID than the one the
	 * retry counter is currently tracking).
	 */
	if (!ether_addr_equal(bss->bssid, wpa_s->ml_neigh_retry_bssid)) {
		wpa_s->ml_neigh_retries = 0;
		os_memcpy(wpa_s->ml_neigh_retry_bssid, bss->bssid, ETH_ALEN);
	}

	wpas_sme_set_mlo_links(wpa_s, bss, ssid, &missing_links);

	if (missing_links && wpa_s->ml_neigh_retries <= 5) {
		wpa_printf(MSG_DEBUG,
			   "MLD: Neighbor not found in scan, current neigh scan retry count %u", wpa_s->ml_neigh_retries);
		wpas_connect_work_done(wpa_s);
		/* Tear down auth process and start scan again */
		wpa_supplicant_cancel_scan(wpa_s);
		wpa_supplicant_cancel_sched_scan(wpa_s);
		wpa_s->ml_neigh_retries++;
		/*
		 * Set reassociate so that after the scan completes,
		 * wpas_select_network_from_last_scan() enters the association
		 * branch and calls wpa_supplicant_connect_ml_missing(), which
		 * sets up a directed ML probe (ml_probe_bssid / ml_probe_links)
		 * targeting only the missing link IDs.  A plain wildcard scan
		 * without this flag bypasses the ML probe path entirely.
		 */
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
		return;
	}

	if (wpa_s->valid_links) {
		wpa_printf(MSG_DEBUG, "MLD: In authentication");

#ifdef CONFIG_TESTING_OPTIONS
		bss = wpas_ml_connect_pref(wpa_s, bss, ssid);

		if (wpa_s->conf->mld_force_single_link) {
			wpa_printf(MSG_DEBUG, "MLD: Force single link");
			wpa_s->valid_links = BIT(wpa_s->mlo_assoc_link_id);
		}
#endif /* CONFIG_TESTING_OPTIONS */
		params.mld = true;
		params.mld_link_id = wpa_s->mlo_assoc_link_id;
		params.ap_mld_addr = wpa_s->ap_mld_addr;
		wpas_ml_handle_removed_links(wpa_s, bss);
	}

	skip_auth = wpa_s->conf->reassoc_same_bss_optim &&
		wpa_s->reassoc_same_bss;

	if (!skip_auth && wpa_s->smd_capable) {
		struct rsn_pmksa_cache_entry *pmksa = pmksa_cache_get_current(wpa_s->wpa);
		if (pmksa && pmksa->smd_enabled &&
		    os_memcmp(pmksa->aa, bss->bssid, ETH_ALEN) == 0) {
			wpa_printf(MSG_DEBUG, "SME: Using OPEN Auth for SMD PMKSA Cache reuse");
			params.auth_alg = WPA_AUTH_ALG_OPEN;
		}
	}

	wpa_s->current_bss = bss;

	wpa_s->reassociate = 0;

	params.freq = bss->freq;
	params.bssid = bss->bssid;
	params.ssid = bss->ssid;
	params.ssid_len = bss->ssid_len;
	params.p2p = ssid->p2p_group;
	params.sta_max_bw = ssid->sta_max_bw;

	if (wpa_s->sme.ssid_len != params.ssid_len ||
	    os_memcmp(wpa_s->sme.ssid, params.ssid, params.ssid_len) != 0)
		wpa_s->sme.prev_bssid_set = 0;

	wpa_s->sme.freq = params.freq;
	os_memcpy(wpa_s->sme.ssid, params.ssid, params.ssid_len);
	wpa_s->sme.ssid_len = params.ssid_len;

	params.auth_alg = WPA_AUTH_ALG_OPEN;
#ifdef IEEE8021X_EAPOL
	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if (ssid->leap) {
			if (ssid->non_leap == 0)
				params.auth_alg = WPA_AUTH_ALG_LEAP;
			else
				params.auth_alg |= WPA_AUTH_ALG_LEAP;
		}
	}
#endif /* IEEE8021X_EAPOL */
	wpa_dbg(wpa_s, MSG_DEBUG, "Automatic auth_alg selection: 0x%x",
		params.auth_alg);
	if (ssid->auth_alg) {
		params.auth_alg = ssid->auth_alg;
		wpa_dbg(wpa_s, MSG_DEBUG, "Overriding auth_alg selection: "
			"0x%x", params.auth_alg);
	}
#ifdef CONFIG_SAE
	wpa_s->sme.sae_pmksa_caching = 0;
	if (wpa_key_mgmt_sae(ssid->key_mgmt)) {
		const u8 *rsn;
		struct wpa_ie_data ied;

		rsn = wpa_bss_get_rsne(wpa_s, bss, ssid, false);
		if (!rsn) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SAE enabled, but target BSS does not advertise RSN");
		} else if (wpa_parse_wpa_ie(rsn, 2 + rsn[1], &ied)) {
			wpa_printf(MSG_DEBUG, "SME: Failed parsing RSNE data");
			return;
#ifdef CONFIG_DPP
		} else if ((ssid->key_mgmt & WPA_KEY_MGMT_DPP) &&
			   (ied.key_mgmt & WPA_KEY_MGMT_DPP)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Prefer DPP over SAE when both are enabled");
#endif /* CONFIG_DPP */
#ifdef CONFIG_ENC_ASSOC
		} else if ((wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_EPPKE) &&
			   (ssid->key_mgmt & WPA_KEY_MGMT_EPPKE) &&
			   (ied.key_mgmt & WPA_KEY_MGMT_EPPKE) &&
			   wpa_key_mgmt_sae_ext_key(ssid->key_mgmt) &&
			   wpa_key_mgmt_sae_ext_key(ied.key_mgmt) &&
			   !wpas_is_sae_avoided(wpa_s, ssid, &ied) &&
			   wpas_eppke_ap_capable(wpa_s, bss)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"Prefer EPPKE over SAE when both are enabled");
			params.auth_alg = WPA_AUTH_ALG_EPPKE;
#endif /* CONFIG_ENC_ASSOC */
		} else if (wpa_key_mgmt_sae(ied.key_mgmt)) {
			if (wpas_is_sae_avoided(wpa_s, ssid, &ied)) {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"SAE enabled, but disallowing SAE auth_alg without PMF");
			} else {
				wpa_dbg(wpa_s, MSG_DEBUG, "Using SAE auth_alg");
				params.auth_alg = WPA_AUTH_ALG_SAE;
			}
		} else {
			/*
			 * Security Profile element preference
			 * (802.11bn D1.4, 37.32): If the AP's Security Profile
			 * element implies SAE-EXT-KEY (profiles 1, 2, 9, 10),
			 * use SAE auth even when the legacy RSNE only lists
			 * WPA-PSK or another non-SAE AKM.  The SP override in
			 * BSS selection already augmented ie.key_mgmt, but
			 * auth algorithm selection is a separate decision here.
			 */
			if (wpas_security_profile_active(wpa_s)) {
				const u8 *sp = wpa_bss_get_ie_ext(
					bss, WLAN_EID_EXT_SECURITY_PROFILE);
				int sp_km = security_profile_ie_get_key_mgmt(
					sp, ssid->key_mgmt);

				if (sp_km && wpa_key_mgmt_sae(sp_km) &&
				    !wpas_is_sae_avoided(wpa_s, ssid, &ied)) {
					wpa_dbg(wpa_s, MSG_DEBUG,
						"Using SAE auth_alg via Security Profile IE override");
					params.auth_alg = WPA_AUTH_ALG_SAE;
				} else {
					wpa_dbg(wpa_s, MSG_DEBUG,
						"SAE enabled, but target BSS does not advertise SAE AKM for RSN");
				}
			} else {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"SAE enabled, but target BSS does not advertise SAE AKM for RSN");
			}
		}
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_WEP
	{
		int i;

		for (i = 0; i < NUM_WEP_KEYS; i++) {
			if (ssid->wep_key_len[i])
				params.wep_key[i] = ssid->wep_key[i];
			params.wep_key_len[i] = ssid->wep_key_len[i];
		}
		params.wep_tx_keyidx = ssid->wep_tx_keyidx;
	}
#endif /* CONFIG_WEP */

	if ((wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE) ||
	     wpa_bss_get_rsne(wpa_s, bss, ssid, false)) &&
	    wpa_key_mgmt_wpa(ssid->key_mgmt)) {
		int try_opportunistic;
		const u8 *cache_id = NULL;

		try_opportunistic = (ssid->proactive_key_caching < 0 ?
				     wpa_s->conf->okc :
				     ssid->proactive_key_caching) &&
			(ssid->proto & WPA_PROTO_RSN);
#ifdef CONFIG_FILS
		if (wpa_key_mgmt_fils(ssid->key_mgmt))
			cache_id = wpa_bss_get_fils_cache_id(bss);
#endif /* CONFIG_FILS */
		if (pmksa_cache_set_current(wpa_s->wpa, NULL,
					    params.mld ? params.ap_mld_addr :
					    bss->bssid,
					    wpa_s->current_ssid,
					    try_opportunistic, cache_id,
					    0, false) == 0)
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol);
		wpa_s->sme.assoc_req_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		if (wpa_supplicant_set_suites(wpa_s, bss, ssid,
					      wpa_s->sme.assoc_req_ie,
					      &wpa_s->sme.assoc_req_ie_len,
					      false)) {
			wpa_msg(wpa_s, MSG_WARNING, "SME: Failed to set WPA "
				"key management and encryption suites");
			wpas_connect_work_done(wpa_s);
			return;
		}
	} else if ((ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) &&
		   wpa_key_mgmt_wpa_ieee8021x(ssid->key_mgmt)) {
		/*
		 * Both WPA and non-WPA IEEE 802.1X enabled in configuration -
		 * use non-WPA since the scan results did not indicate that the
		 * AP is using WPA or WPA2.
		 */
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_s->sme.assoc_req_ie_len = 0;
	} else if (wpa_key_mgmt_wpa_any(ssid->key_mgmt)) {
		wpa_s->sme.assoc_req_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		if (wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					      wpa_s->sme.assoc_req_ie,
					      &wpa_s->sme.assoc_req_ie_len,
					      false)) {
			wpa_msg(wpa_s, MSG_WARNING, "SME: Failed to set WPA "
				"key management and encryption suites (no "
				"scan results)");
			wpas_connect_work_done(wpa_s);
			return;
		}
#ifdef CONFIG_WPS
	} else if (ssid->key_mgmt & WPA_KEY_MGMT_WPS) {
		struct wpabuf *wps_ie;

		wpa_s->sme.assoc_req_ie_len = 0;
		wps_ie = wps_build_assoc_req_ie(wpas_wps_get_req_type(ssid));
		sme_add_assoc_req_ie(wpa_s, wps_ie);
		wpabuf_free(wps_ie);
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
#endif /* CONFIG_WPS */
	} else {
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_s->sme.assoc_req_ie_len = 0;
	}

	/* In case the WPA vendor IE is used, it should be placed after all the
	 * non-vendor IEs, as the lower layer expects the IEs to be ordered as
	 * defined in the standard. Store the WPA IE so it can later be
	 * inserted at the correct location.
	 */
	wpa_ie = NULL;
	wpa_ie_len = 0;
	if (wpa_s->wpa_proto == WPA_PROTO_WPA) {
		wpa_ie = os_memdup(wpa_s->sme.assoc_req_ie,
				   wpa_s->sme.assoc_req_ie_len);
		omit_rsnxe = 1;
		if (wpa_ie) {
			wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Storing WPA IE");

			wpa_ie_len = wpa_s->sme.assoc_req_ie_len;
			wpa_s->sme.assoc_req_ie_len = 0;
		} else {
			wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed copy WPA IE");
			wpas_connect_work_done(wpa_s);
			return;
		}
	}

#ifdef CONFIG_IEEE80211R
	ie = wpa_bss_get_ie(bss, WLAN_EID_MOBILITY_DOMAIN);
	if (ie && ie[1] >= MOBILITY_DOMAIN_ID_LEN)
		md = ie + 2;
	wpa_sm_set_ft_params(wpa_s->wpa, ie, ie ? 2 + ie[1] : 0);
	if (md && (!wpa_key_mgmt_ft(ssid->key_mgmt) ||
		   !wpa_key_mgmt_ft(wpa_s->key_mgmt)))
		md = NULL;
	if (md) {
		/* Prepare for the next transition */
		wpa_ft_prepare_auth_request(wpa_s->wpa, ie);
	}

	if (md) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: FT mobility domain %02x%02x",
			md[0], md[1]);

		if (wpa_s->sme.assoc_req_ie_len + 5 <
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			struct rsn_mdie *mdie;
			u8 *pos = wpa_s->sme.assoc_req_ie +
				wpa_s->sme.assoc_req_ie_len;
			*pos++ = WLAN_EID_MOBILITY_DOMAIN;
			*pos++ = sizeof(*mdie);
			mdie = (struct rsn_mdie *) pos;
			os_memcpy(mdie->mobility_domain, md,
				  MOBILITY_DOMAIN_ID_LEN);
			mdie->ft_capab = md[MOBILITY_DOMAIN_ID_LEN];
			wpa_s->sme.assoc_req_ie_len += 5;
		}

		if (wpa_s->sme.prev_bssid_set && wpa_s->sme.ft_used &&
		    os_memcmp(md, wpa_s->sme.mobility_domain, 2) == 0 &&
		    wpa_sm_has_ft_keys(wpa_s->wpa, md)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Trying to use FT "
				"over-the-air");
			omit_rsnxe |= !wpa_bss_get_rsnxe(wpa_s, bss, ssid,
							 false);
			params.auth_alg = WPA_AUTH_ALG_FT;
			params.ie = wpa_s->sme.ft_ies;
			params.ie_len = wpa_s->sme.ft_ies_len;
		}
	}
#endif /* CONFIG_IEEE80211R */

	wpa_s->sme.mfp = wpas_get_ssid_pmf(wpa_s, ssid);
	if (wpa_s->sme.mfp != NO_MGMT_FRAME_PROTECTION) {
		const u8 *rsn = wpa_bss_get_rsne(wpa_s, bss, ssid, false);
		struct wpa_ie_data _ie;
		if (rsn && wpa_parse_wpa_ie(rsn, 2 + rsn[1], &_ie) == 0 &&
		    _ie.capabilities &
		    (WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Selected AP supports "
				"MFP: require MFP");
			wpa_s->sme.mfp = MGMT_FRAME_PROTECTION_REQUIRED;
		}
	}

	wpa_s->sme.spp_amsdu = wpa_sm_uses_spp_amsdu(wpa_s->wpa);

#ifdef CONFIG_P2P
	if (wpa_s->global->p2p) {
		u8 *pos;
		size_t len;
		int res;
		pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
		len = sizeof(wpa_s->sme.assoc_req_ie) -
			wpa_s->sme.assoc_req_ie_len;
		res = wpas_p2p_assoc_req_ie(wpa_s, bss, pos, len,
					    ssid->p2p_group);
		if (res >= 0)
			wpa_s->sme.assoc_req_ie_len += res;
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_FST
	sme_add_assoc_req_ie(wpa_s, wpa_s->fst_ies);
#endif /* CONFIG_FST */

	sme_auth_handle_rrm(wpa_s, bss);

#ifndef CONFIG_NO_RRM
	wpa_s->sme.assoc_req_ie_len += wpas_supp_op_class_ie(
		wpa_s, ssid, bss,
		wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
		sizeof(wpa_s->sme.assoc_req_ie) - wpa_s->sme.assoc_req_ie_len);
#endif /* CONFIG_NO_RRM */

	if (params.p2p)
		wpa_drv_get_ext_capa(wpa_s, WPA_IF_P2P_CLIENT);
	else
		wpa_drv_get_ext_capa(wpa_s, WPA_IF_STATION);

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab,
					     sizeof(ext_capab), bss);
	if (ext_capab_len > 0) {
		u8 *pos = wpa_s->sme.assoc_req_ie;
		if (wpa_s->sme.assoc_req_ie_len > 0 && pos[0] == WLAN_EID_RSN)
			pos += 2 + pos[1];
		os_memmove(pos + ext_capab_len, pos,
			   wpa_s->sme.assoc_req_ie_len -
			   (pos - wpa_s->sme.assoc_req_ie));
		wpa_s->sme.assoc_req_ie_len += ext_capab_len;
		os_memcpy(pos, ext_capab, ext_capab_len);
	}

	if (ssid->max_idle && wpa_s->sme.assoc_req_ie_len + 5 <=
	    sizeof(wpa_s->sme.assoc_req_ie)) {
		u8 *pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;

		*pos++ = WLAN_EID_BSS_MAX_IDLE_PERIOD;
		*pos++ = 3;
		WPA_PUT_LE16(pos, ssid->max_idle);
		pos += 2;
		*pos = 0; /* Idle Options */
		wpa_s->sme.assoc_req_ie_len += 5;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->rsnxe_override_assoc) {
		wpa_printf(MSG_DEBUG, "TESTING: RSNXE AssocReq override");
		sme_add_assoc_req_ie(wpa_s, wpa_s->rsnxe_override_assoc);
	} else
#endif /* CONFIG_TESTING_OPTIONS */
	if (wpa_s->rsnxe_len > 0 &&
	    wpa_s->rsnxe_len <=
	    sizeof(wpa_s->sme.assoc_req_ie) - wpa_s->sme.assoc_req_ie_len &&
	    !omit_rsnxe) {
		os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			  wpa_s->rsnxe, wpa_s->rsnxe_len);
		wpa_s->sme.assoc_req_ie_len += wpa_s->rsnxe_len;
	}

	if (wpa_s->sme.mfp != NO_MGMT_FRAME_PROTECTION &&
	    wpa_bss_ext_capab(bss, WLAN_EXT_CAPAB_KNOWN_STA_IDENTIFICATION)) {
		struct wpabuf *e;

		e = wpa_sm_known_sta_identification(
			wpa_s->wpa,
			params.mld ? params.ap_mld_addr : bss->bssid,
			bss->tsf);
		if (e) {
			wpa_printf(MSG_DEBUG,
				   "SME: Add Known STA Identification element");
			sme_add_assoc_req_ie(wpa_s, e);
			wpabuf_free(e);
		}
	}

#ifdef CONFIG_HS20
	if (is_hs20_network(wpa_s, ssid, bss)) {
		struct wpabuf *hs20;

		hs20 = wpabuf_alloc(20 + MAX_ROAMING_CONS_OI_LEN);
		if (hs20) {
			int pps_mo_id = hs20_get_pps_mo_id(wpa_s, ssid);

			wpas_hs20_add_indication(hs20, pps_mo_id,
						 get_hs20_version(bss));
			wpas_hs20_add_roam_cons_sel(hs20, ssid);
			sme_add_assoc_req_ie(wpa_s, hs20);
			wpabuf_free(hs20);
		}
	}
#endif /* CONFIG_HS20 */

	if (wpa_ie) {
		size_t len;

		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Reinsert WPA IE");

		len = sizeof(wpa_s->sme.assoc_req_ie) -
			wpa_s->sme.assoc_req_ie_len;

		if (len > wpa_ie_len) {
			os_memcpy(wpa_s->sme.assoc_req_ie +
				  wpa_s->sme.assoc_req_ie_len,
				  wpa_ie, wpa_ie_len);
			wpa_s->sme.assoc_req_ie_len += wpa_ie_len;
		} else {
			wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Failed to add WPA IE");
		}

		os_free(wpa_ie);
	}

	sme_add_assoc_req_ie(wpa_s, wpa_s->vendor_elem[VENDOR_ELEM_ASSOC_REQ]);

#ifdef CONFIG_MBO
	mbo_ie = wpa_bss_get_vendor_ie(bss, MBO_IE_VENDOR_TYPE);
	if (!wpa_s->disable_mbo_oce && mbo_ie) {
		int len;

		len = wpas_mbo_ie(wpa_s, wpa_s->sme.assoc_req_ie +
				  wpa_s->sme.assoc_req_ie_len,
				  sizeof(wpa_s->sme.assoc_req_ie) -
				  wpa_s->sme.assoc_req_ie_len,
				  !!mbo_attr_from_mbo_ie(mbo_ie,
							 OCE_ATTR_ID_CAPA_IND));
		if (len >= 0)
			wpa_s->sme.assoc_req_ie_len += len;
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_ENC_ASSOC
	if (!skip_auth && params.auth_alg == WPA_AUTH_ALG_EPPKE) {
		if (start) {
			/* Initialize EPPKE just for the first Authentication
			 * frame */
			if (wpas_eppke_initialize(wpa_s, bss, ssid) < 0) {
				wpas_connection_failed(wpa_s, bss->bssid, NULL);
				return;
			}

			resp = wpas_pasn_build_auth_1(&wpa_s->pasn, NULL,
						      false, 0);
		} else {
			resp = wpas_pasn_build_auth_3(&wpa_s->pasn, 0);
		}
		if (!resp) {
			wpas_connection_failed(wpa_s, bss->bssid, NULL);
			return;
		}
		params.auth_data = wpabuf_head(resp);
		params.auth_data_len = wpabuf_len(resp);
	}
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_SAE
	if (!skip_auth && params.auth_alg == WPA_AUTH_ALG_SAE &&
	    pmksa_cache_set_current(wpa_s->wpa, NULL,
				    params.mld ? params.ap_mld_addr :
				    bss->bssid,
				    ssid, 0,
				    NULL,
				    wpa_key_mgmt_sae(wpa_s->key_mgmt) ?
				    wpa_s->key_mgmt :
				    (int) WPA_KEY_MGMT_SAE, false) == 0) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"PMKSA cache entry found - try to use PMKSA caching instead of new SAE authentication");
		wpa_sm_set_pmk_from_pmksa(wpa_s->wpa);
		params.auth_alg = WPA_AUTH_ALG_OPEN;
		wpa_s->sme.sae_pmksa_caching = 1;
	}

	if (!skip_auth && params.auth_alg == WPA_AUTH_ALG_SAE) {
		if (start)
			resp = sme_auth_build_sae_commit(wpa_s, ssid,
							 bss->bssid,
							 params.mld ?
							 params.ap_mld_addr :
							 NULL, 0,
							 start == 2, NULL,
							 NULL);
		else
			resp = sme_auth_build_sae_confirm(wpa_s, 0);
		if (resp == NULL) {
			wpas_connection_failed(wpa_s, bss->bssid, NULL);
			return;
		}
		params.auth_data = wpabuf_head(resp);
		params.auth_data_len = wpabuf_len(resp);
		wpa_s->sme.sae.state = start ? SAE_COMMITTED : SAE_CONFIRMED;
	}
#endif /* CONFIG_SAE */

	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memcpy(wpa_s->pending_bssid, bss->bssid, ETH_ALEN);
	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);

	old_ssid = wpa_s->current_ssid;
	wpa_s->current_ssid = ssid;
	wpa_supplicant_rsn_supp_set_config(wpa_s, wpa_s->current_ssid);
	wpa_sm_set_ssid(wpa_s->wpa, bss->ssid, bss->ssid_len);
	wpa_supplicant_initiate_eapol(wpa_s);

#ifdef CONFIG_FILS
	/* TODO: FILS operations can in some cases be done between different
	 * network_ctx (i.e., same credentials can be used with multiple
	 * networks). */
	if (params.auth_alg == WPA_AUTH_ALG_OPEN &&
	    wpa_key_mgmt_fils(ssid->key_mgmt)) {
		const u8 *indic;
		u16 fils_info;
		const u8 *realm, *username, *rrk;
		size_t realm_len, username_len, rrk_len;
		u16 next_seq_num;

		/*
		 * Check FILS Indication element (FILS Information field) bits
		 * indicating supported authentication algorithms against local
		 * configuration (ssid->fils_dh_group). Try to use FILS
		 * authentication only if the AP supports the combination in the
		 * network profile. */
		indic = wpa_bss_get_ie(bss, WLAN_EID_FILS_INDICATION);
		if (!indic || indic[1] < 2) {
			wpa_printf(MSG_DEBUG, "SME: " MACSTR
				   " does not include FILS Indication element - cannot use FILS authentication with it",
				   MAC2STR(bss->bssid));
			goto no_fils;
		}

		fils_info = WPA_GET_LE16(indic + 2);
		if (ssid->fils_dh_group == 0 && !(fils_info & BIT(9))) {
			wpa_printf(MSG_DEBUG, "SME: " MACSTR
				   " does not support FILS SK without PFS - cannot use FILS authentication with it",
				   MAC2STR(bss->bssid));
			goto no_fils;
		}
		if (ssid->fils_dh_group != 0 && !(fils_info & BIT(10))) {
			wpa_printf(MSG_DEBUG, "SME: " MACSTR
				   " does not support FILS SK with PFS - cannot use FILS authentication with it",
				   MAC2STR(bss->bssid));
			goto no_fils;
		}

		if (wpa_s->last_con_fail_realm &&
		    eapol_sm_get_erp_info(wpa_s->eapol, &ssid->eap,
					  &username, &username_len,
					  &realm, &realm_len, &next_seq_num,
					  &rrk, &rrk_len) == 0 &&
		    realm && realm_len == wpa_s->last_con_fail_realm_len &&
		    os_memcmp(realm, wpa_s->last_con_fail_realm,
			      realm_len) == 0) {
			wpa_printf(MSG_DEBUG,
				   "SME: FILS authentication for this realm failed last time - try to regenerate ERP key hierarchy");
			goto no_fils;
		}

		if (pmksa_cache_set_current(wpa_s->wpa, NULL,
					    params.mld ? params.ap_mld_addr :
					    bss->bssid,
					    ssid, 0,
					    wpa_bss_get_fils_cache_id(bss),
					    0, false) == 0)
			wpa_printf(MSG_DEBUG,
				   "SME: Try to use FILS with PMKSA caching");
		resp = fils_build_auth(wpa_s->wpa, ssid->fils_dh_group, md);
		if (resp) {
			int auth_alg;

			if (ssid->fils_dh_group)
				wpa_printf(MSG_DEBUG,
					   "SME: Try to use FILS SK authentication with PFS (DH Group %u)",
					   ssid->fils_dh_group);
			else
				wpa_printf(MSG_DEBUG,
					   "SME: Try to use FILS SK authentication without PFS");
			auth_alg = ssid->fils_dh_group ?
				WPA_AUTH_ALG_FILS_SK_PFS : WPA_AUTH_ALG_FILS;
			params.auth_alg = auth_alg;
			params.auth_data = wpabuf_head(resp);
			params.auth_data_len = wpabuf_len(resp);
			wpa_s->sme.auth_alg = auth_alg;
		}
	}
no_fils:
#endif /* CONFIG_FILS */

	if (wpa_s->smd_capable) {
		u8 auth_ies[512];
		size_t auth_ies_len = 0;
		u8 *smd_ie;
		size_t smd_ie_len;
		u8 ptk_mode = 0;
		u8 capabilities = bss->smd_capabilities ? bss->smd_capabilities : 0x01;
		u16 timeout = bss->smd_timeout ? bss->smd_timeout : 1000;

		if (ssid->smd_ptk_mode == 1)
			ptk_mode = 1;

		params.smd.enabled = wpa_s->smd_capable;
		os_memcpy(params.smd.smd_identifier,
			  bss->smd_identifier, ETH_ALEN);
		params.smd.smd_timeout = timeout;
		params.smd.caps.max_prep_target_apmlds = 0;
		params.smd.caps.smd_type = ssid->smd_ptk_mode;
		params.smd.caps.ptk_mode = ptk_mode;

		if (params.ie && params.ie_len > 0) {
			if (params.ie_len <= sizeof(auth_ies)) {
				os_memcpy(auth_ies, params.ie, params.ie_len);
				auth_ies_len = params.ie_len;
			} else {
				wpa_printf(MSG_WARNING, "SME: Existing auth IEs too large");
				goto skip_smd_ie;
			}
		}

		smd_ie = wpas_build_smd_ie(bss->smd_identifier, ptk_mode, capabilities,
					  timeout, &smd_ie_len);
		if (smd_ie) {
			if (auth_ies_len + smd_ie_len < sizeof(auth_ies)) {
				os_memcpy(auth_ies + auth_ies_len, smd_ie,
					  smd_ie_len);
				auth_ies_len += smd_ie_len;

				params.ie = auth_ies;
				params.ie_len = auth_ies_len;

				wpa_printf(MSG_DEBUG,
					   "SME: Added SMD IE to authentication frame");
			} else {
				wpa_printf(MSG_WARNING, "SME: Auth IEs buffer tpp small for SMD IE");
			}

			os_free(smd_ie);
		}

	skip_smd_ie:;
	}

	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpa_supplicant_cancel_scan(wpa_s);

	wpa_msg(wpa_s, MSG_INFO, "SME: Trying to authenticate with " MACSTR
		" (SSID='%s' freq=%d MHz)", MAC2STR(params.bssid),
		wpa_ssid_txt(params.ssid, params.ssid_len), params.freq);

	eapol_sm_notify_portValid(wpa_s->eapol, false);
	wpa_clear_keys(wpa_s, bss->bssid);
	wpa_supplicant_set_state(wpa_s, WPA_AUTHENTICATING);
	if (old_ssid != wpa_s->current_ssid)
		wpas_notify_network_changed(wpa_s);

#ifdef CONFIG_HS20
	hs20_configure_frame_filters(wpa_s);
#endif /* CONFIG_HS20 */

#ifdef CONFIG_P2P
	/*
	 * If multi-channel concurrency is not supported, check for any
	 * frequency conflict. In case of any frequency conflict, remove the
	 * least prioritized connection.
	 */
	if (wpa_s->num_multichan_concurrent < 2) {
		int freq, num;
		num = get_shared_radio_freqs(wpa_s, &freq, 1, false);
		if (num > 0 && freq > 0 && freq != params.freq) {
			wpa_printf(MSG_DEBUG,
				   "Conflicting frequency found (%d != %d)",
				   freq, params.freq);
			if (wpas_p2p_handle_frequency_conflicts(wpa_s,
								params.freq,
								ssid) < 0) {
				wpas_connection_failed(wpa_s, bss->bssid, NULL);
				wpa_supplicant_mark_disassoc(wpa_s);
				wpabuf_free(resp);
				wpas_connect_work_done(wpa_s);
				return;
			}
		}
	}
#endif /* CONFIG_P2P */

	if (skip_auth) {
		wpa_msg(wpa_s, MSG_DEBUG,
			"SME: Skip authentication step on reassoc-to-same-BSS");
		wpabuf_free(resp);
		sme_associate(wpa_s, ssid->mode, bss->bssid, WLAN_AUTH_OPEN);
		return;
	}


	/*
	 * Security Profile element — auth frame inclusion (802.11bn D1.4,
	 * 37.32, 9.4.2.364).
	 *
	 * When the Security Profile element was selected and built in
	 * wpa_supplicant_set_suites(), include it in the Authentication frame
	 * IEs (params.ie / params.ie_len → NL80211_ATTR_IE) for auth
	 * algorithms that use it:
	 *
	 *   WPA_AUTH_ALG_EPPKE  — EPPKE (AKM 29) authentication
	 *   WPA_AUTH_ALG_802_1X — 802.1X-in-auth-frames (37.32, Table 9-bb14)
	 */
	if (wpa_s->security_profile_ie_len > 0 && start &&
	    (params.auth_alg == WPA_AUTH_ALG_EPPKE ||
	     params.auth_alg == WPA_AUTH_ALG_802_1X)) {
		if (params.ie && params.ie_len > 0) {
			size_t combined_len = params.ie_len +
				wpa_s->security_profile_ie_len;

			auth_ie_buf = os_malloc(combined_len);
			if (!auth_ie_buf) {
				wpa_msg(wpa_s, MSG_ERROR,
					"SME: Failed to allocate buffer for auth frame IEs");
				wpas_connection_failed(wpa_s, bss->bssid, NULL);
				wpa_supplicant_mark_disassoc(wpa_s);
				wpabuf_free(resp);
				wpas_connect_work_done(wpa_s);
				return;
			}
			os_memcpy(auth_ie_buf, params.ie, params.ie_len);
			os_memcpy(auth_ie_buf + params.ie_len,
				  wpa_s->security_profile_ie,
				  wpa_s->security_profile_ie_len);
			params.ie = auth_ie_buf;
			params.ie_len = combined_len;
		} else {
			params.ie = wpa_s->security_profile_ie;
			params.ie_len = wpa_s->security_profile_ie_len;
		}
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: Including Security Profile element in auth frame (alg=0x%x profile=%d)",
			params.auth_alg,
			wpa_s->selected_security_profile_num);
	}

	wpa_s->sme.auth_alg = params.auth_alg;
	if (wpa_drv_authenticate(wpa_s, &params) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "SME: Authentication request to the "
			"driver failed");
		wpas_connection_failed(wpa_s, bss->bssid, NULL);
		wpa_supplicant_mark_disassoc(wpa_s);
		wpabuf_free(resp);
		os_free(auth_ie_buf);
		wpas_connect_work_done(wpa_s);
		return;
	}

	eloop_register_timeout(SME_AUTH_TIMEOUT, 0, sme_auth_timer, wpa_s,
			       NULL);

	/*
	 * Association will be started based on the authentication event from
	 * the driver.
	 */

	wpabuf_free(resp);
	os_free(auth_ie_buf);
}


static void sme_auth_start_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_connect_work *cwork = work->ctx;
	struct wpa_supplicant *wpa_s = work->wpa_s;

	wpa_s->roam_in_progress = false;
#ifdef CONFIG_WNM
	wpa_s->bss_trans_mgmt_in_progress = false;
#endif /* CONFIG_WNM */

	if (deinit) {
		if (work->started)
			wpa_s->connect_work = NULL;

#ifdef CONFIG_QCN_EXTN
		wpa_s->cache_cwork = NULL;
#endif /* CONFIG_QCN_EXTN */
		wpas_connect_work_free(cwork);
		return;
	}

	wpa_s->connect_work = work;

	if (cwork->bss_removed ||
	    !wpas_valid_bss_ssid(wpa_s, cwork->bss, cwork->ssid) ||
	    wpas_network_disabled(wpa_s, cwork->ssid)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: BSS/SSID entry for authentication not valid anymore - drop connection attempt");
		wpas_connect_work_done(wpa_s);
		return;
	}

	/* Starting new connection, so clear the possibly used WPA IE from the
	 * previous association. */
	wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);
	wpa_sm_set_assoc_rsnxe(wpa_s->wpa, NULL, 0);
	wpa_s->rsnxe_len = 0;

	sme_send_authentication(wpa_s, cwork->bss, cwork->ssid, 1);
	wpas_notify_auth_changed(wpa_s);
}

#ifdef CONFIG_QCN_EXTN
/**
 * sme_schedule_auth_radio_work - Schedule SME authentication radio work
 * @wpa_s: Pointer to wpa_supplicant interface
 * @cwork: Cached connect work describing target BSS and SSID
 *
 * Cancel any pending pre-connect timeout, schedule a new sme-connect
 * radio work item for the cached BSS, and clear the pre_connect_cnt
 * counter used by repeater pre-connection flows.
 */
void sme_schedule_auth_radio_work(struct wpa_supplicant *wpa_s,
				  struct wpa_connect_work *cwork)
{
	eloop_cancel_timeout(sme_pre_connect_timer_extn, wpa_s, NULL);

	if (!cwork || !cwork->bss) {
		wpa_msg(wpa_s, MSG_ERROR, "SME: schedule_auth: NULL cwork/bss; abort\n");
		return;
	}
	if (radio_add_work(wpa_s, cwork->bss->freq, "sme-connect", 1, sme_auth_start_cb, cwork) < 0) {
		wpa_msg(wpa_s, MSG_ERROR, "SME: radio_add_work failed; free\n");
		wpas_connect_work_free(cwork);
	}

#ifdef CONFIG_QCN_EXTN
	wpa_s->cache_cwork = NULL;
#endif /* CONFIG_QCN_EXTN */
	wpa_s->pre_connect_cnt = 0;
}
#endif

void sme_authenticate(struct wpa_supplicant *wpa_s,
		      struct wpa_bss *bss, struct wpa_ssid *ssid)
{
	struct wpa_connect_work *cwork;

	if (bss == NULL || ssid == NULL)
		return;
	if (wpa_s->connect_work) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Reject sme_authenticate() call since connect_work exist");
		return;
	}

	if (wpa_s->roam_in_progress) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: Reject sme_authenticate() in favor of explicit roam request");
		return;
	}
#ifdef CONFIG_WNM
	if (wpa_s->bss_trans_mgmt_in_progress) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: Reject sme_authenticate() in favor of BSS transition management request");
		return;
	}
#endif /* CONFIG_WNM */
	if (radio_work_pending(wpa_s, "sme-connect")) {
		/*
		 * The previous sme-connect work might no longer be valid due to
		 * the fact that the BSS list was updated. In addition, it makes
		 * sense to adhere to the 'newer' decision.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: Remove previous pending sme-connect");
		radio_remove_works(wpa_s, "sme-connect", 0);
	}

	wpas_abort_ongoing_scan(wpa_s);

	cwork = os_zalloc(sizeof(*cwork));
	if (cwork == NULL)
		return;
	cwork->bss = bss;
	cwork->ssid = ssid;
	cwork->sme = 1;

#ifdef CONFIG_SAE
	wpa_s->sme.sae.state = SAE_NOTHING;
	wpa_s->sme.sae.send_confirm = 0;
	wpa_s->sme.sae_group_index = 0;
#endif /* CONFIG_SAE */

#ifdef CONFIG_QCN_EXTN
	if (wpa_s->conf->ind_rptr) {
		/*
		 * In ind_rptr mode the rptr_mgr socket drives the auth flow
		 * externally, bypassing wpas_select_network_from_last_scan()
		 * and therefore wpa_supplicant_connect_ml_missing().  Check
		 * for missing MLD partner links here, before notifying the
		 * rptr_mgr, so the directed ML probe path is taken instead of
		 * entering PRE_CONNECT with an incomplete link set.
		 */
		if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_MLO) {
			u16 ind_missing_links = 0;

			wpa_bss_get_usable_links(wpa_s, bss, ssid,
						 &ind_missing_links);
			if (ind_missing_links &&
			    wpa_s->ml_neigh_retries <= 5) {
				wpa_printf(MSG_DEBUG,
					   "MLD: ind_rptr: missing links 0x%04x before PRE_CONNECT, retry %u",
					   ind_missing_links,
					   wpa_s->ml_neigh_retries);
				wpa_s->ml_neigh_retries++;
				wpa_s->reassociate = 1;
				wpas_connect_work_free(cwork);
				wpa_supplicant_cancel_scan(wpa_s);
				wpa_supplicant_cancel_sched_scan(wpa_s);
				wpa_supplicant_req_scan(wpa_s, 0, 0);
				return;
			}
		}

		if (wpa_s->conf->rptr_mgr_comm_mode == RPTR_MGR_MODE_COMM_SOCK) {
			wpa_supp_pre_connect_state_handle_extn(wpa_s, bss);
		}
		wpa_s->cache_cwork = cwork;
		/*
		 * Snapshot the target frequencies now: bss->freq/mld_links are
		 * live values in the scan cache and can be rewritten by a
		 * background scan while this connection attempt is still
		 * pending in PRE_CONNECT. For an MLD BSS, one channel-switch-
		 * result event per valid link is expected (matching the
		 * per-link pre_connect_cnt increments above), so snapshot
		 * every valid link's frequency, not just bss->freq.
		 */
		wpa_s->pre_connect_target_freq_count = 0;
		if (!is_zero_ether_addr(bss->mld_addr)) {
			u8 link_id;

			for_each_link(bss->valid_links, link_id) {
				if (bss->mld_links[link_id].freq == 0)
					continue;
				wpa_s->pre_connect_target_freqs[
					wpa_s->pre_connect_target_freq_count++] =
					bss->mld_links[link_id].freq;
			}
		}
		if (!wpa_s->pre_connect_target_freq_count)
			wpa_s->pre_connect_target_freqs[
				wpa_s->pre_connect_target_freq_count++] = bss->freq;
		wpa_supplicant_set_state(wpa_s, WPA_PRE_CONNECT);

		eloop_register_timeout(SME_PRE_CONNECT_TIMEOUT, 0, sme_pre_connect_timer_extn,
				       wpa_s, NULL);
	} else {
#endif
		if (radio_add_work(wpa_s, bss->freq, "sme-connect", 1,
				   sme_auth_start_cb, cwork) < 0)
			wpas_connect_work_free(cwork);
#ifdef CONFIG_QCN_EXTN
	}
#endif
}


#ifdef CONFIG_SAE

#define WPA_AUTH_FRAME_ML_IE_LEN	(6 + ETH_ALEN)

static void wpa_auth_ml_ie(struct wpabuf *buf, const u8 *mld_addr)
{

	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 4 + ETH_ALEN);
	wpabuf_put_u8(buf, WLAN_EID_EXT_MULTI_LINK);

	/* Basic Multi-Link element Control field */
	wpabuf_put_u8(buf, 0x0);
	wpabuf_put_u8(buf, 0x0);

	/* Common Info */
	wpabuf_put_u8(buf, 0x7); /* length = Length field + MLD MAC address */
	wpabuf_put_data(buf, mld_addr, ETH_ALEN);
}


static int sme_external_auth_build_buf(struct wpabuf *buf,
				       struct wpabuf *params,
				       const u8 *sa, const u8 *da,
				       u16 auth_transaction, u16 seq_num,
				       u16 status_code, const u8 *mld_addr)
{
	struct ieee80211_mgmt *resp;

	resp = wpabuf_put(buf, offsetof(struct ieee80211_mgmt,
					u.auth.variable));

	resp->frame_control = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
					   (WLAN_FC_STYPE_AUTH << 4));
	os_memcpy(resp->da, da, ETH_ALEN);
	os_memcpy(resp->sa, sa, ETH_ALEN);
	os_memcpy(resp->bssid, da, ETH_ALEN);
	resp->u.auth.auth_alg = host_to_le16(WLAN_AUTH_SAE);
	resp->seq_ctrl = host_to_le16(seq_num << 4);
	resp->u.auth.auth_transaction = host_to_le16(auth_transaction);
	resp->u.auth.status_code = host_to_le16(status_code);
	if (params)
		wpabuf_put_buf(buf, params);

	if (mld_addr)
		wpa_auth_ml_ie(buf, mld_addr);

	return 0;
}


static int sme_external_auth_send_sae_commit(struct wpa_supplicant *wpa_s,
					     const u8 *bssid,
					     struct wpa_ssid *ssid)
{
	struct wpabuf *resp, *buf;
	int use_pt;
	bool use_pk;
	u16 status;

	resp = sme_auth_build_sae_commit(wpa_s, ssid, bssid,
					 wpa_s->sme.ext_ml_auth ?
					 wpa_s->sme.ext_auth_ap_mld_addr : NULL,
					 1, 0, &use_pt, &use_pk);
	if (!resp) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to build SAE commit");
		return -1;
	}

	wpa_s->sme.sae.state = SAE_COMMITTED;
	buf = wpabuf_alloc(4 + SAE_COMMIT_MAX_LEN + wpabuf_len(resp) +
			   (wpa_s->sme.ext_ml_auth ? WPA_AUTH_FRAME_ML_IE_LEN :
			    0));
	if (!buf) {
		wpabuf_free(resp);
		return -1;
	}

	wpa_s->sme.seq_num++;
	if (use_pk)
		status = WLAN_STATUS_SAE_PK;
	else if (use_pt)
		status = WLAN_STATUS_SAE_HASH_TO_ELEMENT;
	else
		status = WLAN_STATUS_SUCCESS;
	sme_external_auth_build_buf(buf, resp, wpa_s->own_addr,
				    wpa_s->sme.ext_ml_auth ?
				    wpa_s->sme.ext_auth_ap_mld_addr : bssid, 1,
				    wpa_s->sme.seq_num, status,
				    wpa_s->sme.ext_ml_auth ?
				    wpa_s->own_addr : NULL);
	wpa_drv_send_mlme(wpa_s, wpabuf_head(buf), wpabuf_len(buf), 1, 0, 0);
	wpabuf_free(resp);
	wpabuf_free(buf);

	return 0;
}


void sme_send_external_auth_status(struct wpa_supplicant *wpa_s, u16 status)
{
	struct external_auth params;

	wpa_s->sme.ext_auth_wpa_ssid = NULL;
	os_memset(&params, 0, sizeof(params));
	params.status = status;
	params.ssid = wpa_s->sme.ext_auth_ssid;
	params.ssid_len = wpa_s->sme.ext_auth_ssid_len;
	params.bssid = wpa_s->sme.ext_auth_bssid;
	if (wpa_s->conf->sae_pmkid_in_assoc && status == WLAN_STATUS_SUCCESS)
		params.pmkid = wpa_s->sme.sae.pmkid;
	wpa_drv_send_external_auth_status(wpa_s, &params);
}


static int sme_handle_external_auth_start(struct wpa_supplicant *wpa_s,
					  union wpa_event_data *data)
{
	struct wpa_ssid *ssid;
	size_t ssid_str_len = data->external_auth.ssid_len;
	const u8 *ssid_str = data->external_auth.ssid;

	wpa_s->sme.ext_auth_wpa_ssid = NULL;
	/* Get the SSID conf from the ssid string obtained */
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!wpas_network_disabled(wpa_s, ssid) &&
		    ssid_str_len == ssid->ssid_len &&
		    os_memcmp(ssid_str, ssid->ssid, ssid_str_len) == 0 &&
		    wpa_key_mgmt_sae(ssid->key_mgmt)) {
			/* Make sure PT is derived */
			wpa_s_setup_sae_pt(wpa_s, ssid, false);
			wpa_s->sme.ext_auth_wpa_ssid = ssid;
			break;
		}
	}
	if (!ssid ||
	    sme_external_auth_send_sae_commit(wpa_s, data->external_auth.bssid,
					      ssid) < 0)
		return -1;

	return 0;
}


static void sme_external_auth_send_sae_confirm(struct wpa_supplicant *wpa_s,
					       const u8 *da)
{
	struct wpabuf *resp, *buf;

	resp = sme_auth_build_sae_confirm(wpa_s, 1);
	if (!resp) {
		wpa_printf(MSG_DEBUG, "SAE: Confirm message buf alloc failure");
		return;
	}

	wpa_s->sme.sae.state = SAE_CONFIRMED;
	buf = wpabuf_alloc(4 + SAE_CONFIRM_MAX_LEN + wpabuf_len(resp) +
			   (wpa_s->sme.ext_ml_auth ? WPA_AUTH_FRAME_ML_IE_LEN :
			    0));
	if (!buf) {
		wpa_printf(MSG_DEBUG, "SAE: Auth Confirm buf alloc failure");
		wpabuf_free(resp);
		return;
	}
	wpa_s->sme.seq_num++;
	sme_external_auth_build_buf(buf, resp, wpa_s->own_addr,
				    da, 2, wpa_s->sme.seq_num,
				    WLAN_STATUS_SUCCESS,
				    wpa_s->sme.ext_ml_auth ?
				    wpa_s->own_addr : NULL);

	wpa_drv_send_mlme(wpa_s, wpabuf_head(buf), wpabuf_len(buf), 1, 0, 0);
	wpabuf_free(resp);
	wpabuf_free(buf);
}


static bool is_sae_key_mgmt_suite(struct wpa_supplicant *wpa_s, u32 suite)
{
	/* suite is supposed to be the selector value in host byte order with
	 * the OUI in three most significant octets. However, the initial
	 * implementation swapped that byte order and did not work with drivers
	 * that followed the expected byte order. Keep a workaround here to
	 * match that initial implementation so that already deployed use cases
	 * remain functional. */
	if (RSN_SELECTOR_GET(&suite) == RSN_AUTH_KEY_MGMT_SAE) {
		/* This will be true in following cases
		 * 1. Old drivers which follow the initial implementation send
		 *    RSN_AUTH_KEY_MGMT_SAE with swapped byte order for both SAE
		 *    and FT-SAE connections.
		 * 2. The driver sending RSN_AUTH_KEY_MGMT_SAE in host byte
		 *    order but kernel swapped that byte order to follow initial
		 *    implementation.
		 * In these cases, update the AKM as WPA_KEY_MGMT_SAE. */
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_SAE;
		return true;
	}

	if (suite == RSN_AUTH_KEY_MGMT_SAE)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_SAE;
	else if (suite == RSN_AUTH_KEY_MGMT_FT_SAE)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_FT_SAE;
	else if (suite == RSN_AUTH_KEY_MGMT_SAE_EXT_KEY)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_SAE_EXT_KEY;
	else if (suite == RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_FT_SAE_EXT_KEY;
	else
		return false;

	return true;
}


#ifdef CONFIG_ENC_ASSOC

static bool is_eppke_auth_key_mgmt_suite(struct wpa_supplicant *wpa_s,
					 u32 suite)
{
	/* Supported AKM suites for EPPKE */
	if (suite == RSN_AUTH_KEY_MGMT_SAE_EXT_KEY)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_SAE_EXT_KEY;
	else if (suite == RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_FT_SAE_EXT_KEY;
	else if (suite == RSN_AUTH_KEY_MGMT_EPPKE)
		wpa_s->sme.ext_auth_key_mgmt = WPA_KEY_MGMT_EPPKE;
	else
		return false;

	return true;
}


static int sme_handle_eppke_external_auth_start(struct wpa_supplicant *wpa_s,
						union wpa_event_data *data)
{
	struct wpa_ssid *ssid;
	size_t ssid_str_len = data->external_auth.ssid_len;
	const u8 *ssid_str = data->external_auth.ssid;
	u8 peer_addr[ETH_ALEN];
	bool is_ml_peer;
	int group;

	wpa_s->sme.ext_auth_wpa_ssid = NULL;
	/* Find the network profil based on the selected SSID */
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!wpas_network_disabled(wpa_s, ssid) &&
		    wpa_key_mgmt_eppke(ssid->key_mgmt) &&
		    ssid_str_len == ssid->ssid_len &&
		    os_memcmp(ssid_str, ssid->ssid, ssid_str_len) == 0) {
			wpa_s->sme.ext_auth_wpa_ssid = ssid;
			break;
		}
	}
	if (!ssid) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: No matching network block found");
		return -1;
	}

	if (data->external_auth.mld_addr) {
		is_ml_peer = true;
		os_memcpy(peer_addr, data->external_auth.mld_addr, ETH_ALEN);
	} else {
		is_ml_peer = false;
		os_memcpy(peer_addr, data->external_auth.bssid, ETH_ALEN);
	}

	group = wpas_pasn_get_group(wpa_s, ssid, NULL);
	if (!group) {
		wpa_printf(MSG_DEBUG, "EPPKE: Failed to select PASN group");
		return -1;
	}

	/* As per IEEE P802.11-REVmf/D2.1, 12.13.5, when SAE is wrapped within
	 * PASN authentication, both shall use the same finite cyclic group.
	 */
	if (sae_set_group(&wpa_s->sme.sae, group) < 0) {
		wpa_printf(MSG_INFO, "EPPKE: Failed to set SAE group %u",
			   group);
		return -1;
	}

	return wpas_pasn_auth_start(wpa_s, wpa_s->own_addr, peer_addr,
				    rsn_key_mgmt_to_wpa_akm(
					    data->external_auth.key_mgmt_suite),
				    data->external_auth.pairwise_cipher,
				    group, ssid->id, NULL, 0,
				    data->external_auth.auth_alg,
				    data->external_auth.group_cipher,
				    data->external_auth.group_mgmt_cipher,
				    data->external_auth.rsn_capab,
				    data->external_auth.rsnxe_data,
				    is_ml_peer);
}

#endif /* CONFIG_ENC_ASSOC */


void sme_external_auth_trigger(struct wpa_supplicant *wpa_s,
			       union wpa_event_data *data)
{
	switch (data->external_auth.auth_alg) {
#ifdef CONFIG_ENC_ASSOC
	case WLAN_AUTH_EPPKE:
		if (!is_eppke_auth_key_mgmt_suite(
			    wpa_s, data->external_auth.key_mgmt_suite))
			return;
		break;
#endif /* CONFIG_ENC_ASSOC */
	default:
#ifdef CONFIG_SAE
		if (!is_sae_key_mgmt_suite(wpa_s,
					   data->external_auth.key_mgmt_suite))
			return;
		break;
#else /* CONFIG_SAE */
		return;
#endif /* CONFIG_SAE */
	}

	if (data->external_auth.action == EXT_AUTH_START) {
		if (!data->external_auth.bssid || !data->external_auth.ssid)
			return;
		os_memcpy(wpa_s->sme.ext_auth_bssid, data->external_auth.bssid,
			  ETH_ALEN);
		os_memcpy(wpa_s->sme.ext_auth_ssid, data->external_auth.ssid,
			  data->external_auth.ssid_len);
		wpa_s->sme.ext_auth_ssid_len = data->external_auth.ssid_len;
		if (data->external_auth.mld_addr) {
			wpa_s->sme.ext_ml_auth = true;
			os_memcpy(wpa_s->sme.ext_auth_ap_mld_addr,
				  data->external_auth.mld_addr, ETH_ALEN);
		} else {
			wpa_s->sme.ext_ml_auth = false;
		}
#ifdef CONFIG_ENC_ASSOC
		if (data->external_auth.auth_alg == WLAN_AUTH_EPPKE) {
			if (sme_handle_eppke_external_auth_start(wpa_s, data) <
			    0)
				sme_send_external_auth_status(
					wpa_s, WLAN_STATUS_UNSPECIFIED_FAILURE);
			return;
		}
#endif /* CONFIG_ENC_ASSOC */
		wpa_s->sme.seq_num = 0;
		wpa_s->sme.sae.state = SAE_NOTHING;
		wpa_s->sme.sae.send_confirm = 0;
		wpa_s->sme.sae_group_index = 0;
		if (sme_handle_external_auth_start(wpa_s, data) < 0)
			sme_send_external_auth_status(wpa_s,
					      WLAN_STATUS_UNSPECIFIED_FAILURE);
	} else if (data->external_auth.action == EXT_AUTH_ABORT) {
		/* Report failure to driver for the wrong trigger */
		sme_send_external_auth_status(wpa_s,
					      WLAN_STATUS_UNSPECIFIED_FAILURE);
	}
}


static int sme_sae_is_group_enabled(struct wpa_supplicant *wpa_s, int group)
{
	int *groups = wpa_s->conf->sae_groups;
	int default_groups[] = { 19, 20, 21, 0 };
	int i;

	if (!groups)
		groups = default_groups;

	for (i = 0; groups[i] > 0; i++) {
		if (groups[i] == group)
			return 1;
	}

	return 0;
}


static int sme_check_sae_rejected_groups(struct wpa_supplicant *wpa_s,
					 const struct wpabuf *groups)
{
	size_t i, count, len;
	const u8 *pos;

	if (!groups)
		return 0;

	pos = wpabuf_head(groups);
	len = wpabuf_len(groups);
	if (len & 1) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Invalid length of the Rejected Groups element payload: %zu",
			   len);
		return 1;
	}
	count = len / 2;
	for (i = 0; i < count; i++) {
		int enabled;
		u16 group;

		group = WPA_GET_LE16(pos);
		pos += 2;
		enabled = sme_sae_is_group_enabled(wpa_s, group);
		wpa_printf(MSG_DEBUG, "SAE: Rejected group %u is %s",
			   group, enabled ? "enabled" : "disabled");
		if (enabled)
			return 1;
	}

	return 0;
}


static int sme_external_ml_auth(struct wpa_supplicant *wpa_s,
				const u8 *data, size_t len, int ie_offset,
				u16 status_code)
{
	struct ieee802_11_elems elems;
	const u8 *mld_addr;

	if (ieee802_11_parse_elems(data + ie_offset, len - ie_offset,
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MLD: Failed parsing elements");
		return -1;
	}

	if (!elems.basic_mle || !elems.basic_mle_len) {
		wpa_printf(MSG_DEBUG, "MLD: No ML element in authentication");
		if (status_code == WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ ||
		    status_code == WLAN_STATUS_SUCCESS ||
		    status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		    status_code == WLAN_STATUS_SAE_PK)
			return -1;
		/* Accept missing Multi-Link element in failed authentication
		 * cases. */
		return 0;
	}

	mld_addr = get_basic_mle_mld_addr(elems.basic_mle, elems.basic_mle_len);
	if (!mld_addr) {
		wpa_printf(MSG_DEBUG, "MLD: No MLD address in ML element");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "MLD: mld_address=" MACSTR, MAC2STR(mld_addr));

	if (!ether_addr_equal(wpa_s->sme.ext_auth_ap_mld_addr, mld_addr)) {
		wpa_printf(MSG_DEBUG, "MLD: Unexpected MLD address (expected "
			   MACSTR ")",
			   MAC2STR(wpa_s->sme.ext_auth_ap_mld_addr));
		return -1;
	}

	return 0;
}


static int sme_sae_auth(struct wpa_supplicant *wpa_s, u16 auth_transaction,
			u16 status_code, const u8 *data, size_t len,
			int external, const u8 *sa, int *ie_offset)
{
	int *groups;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: SAE authentication transaction %u "
		"status code %u", auth_transaction, status_code);

	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT &&
	    status_code == WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ &&
	    wpa_s->sme.sae.state == SAE_COMMITTED &&
	    ((external && wpa_s->sme.ext_auth_wpa_ssid) ||
	     (!external && wpa_s->current_bss && wpa_s->current_ssid))) {
		int default_groups[] = { 19, 20, 21, 0 };
		u16 group;
		const u8 *token_pos;
		size_t token_len;
		int h2e = 0;

		groups = wpa_s->conf->sae_groups;
		if (!groups || groups[0] <= 0)
			groups = default_groups;

		wpa_hexdump(MSG_DEBUG, "SME: SAE anti-clogging token request",
			    data, len);
		if (len < sizeof(le16)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SME: Too short SAE anti-clogging token request");
			return -1;
		}
		group = WPA_GET_LE16(data);
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: SAE anti-clogging token requested (group %u)",
			group);
		if (sae_group_allowed(&wpa_s->sme.sae, groups, group) !=
		    WLAN_STATUS_SUCCESS) {
			wpa_dbg(wpa_s, MSG_ERROR,
				"SME: SAE group %u of anti-clogging request is invalid",
				group);
			return -1;
		}
		wpabuf_free(wpa_s->sme.sae_token);
		token_pos = data + sizeof(le16);
		token_len = len - sizeof(le16);
		h2e = wpa_s->sme.sae.h2e;
		if (h2e) {
			u8 id, elen, extid;

			if (token_len < 3) {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"SME: Too short SAE anti-clogging token container");
				return -1;
			}
			id = *token_pos++;
			elen = *token_pos++;
			extid = *token_pos++;
			if (id != WLAN_EID_EXTENSION ||
			    elen == 0 || elen > token_len - 2 ||
			    extid != WLAN_EID_EXT_ANTI_CLOGGING_TOKEN) {
				wpa_dbg(wpa_s, MSG_DEBUG,
					"SME: Invalid SAE anti-clogging token container header");
				return -1;
			}
			token_len = elen - 1;
#ifdef CONFIG_IEEE80211BE
		} else if ((wpa_s->valid_links ||
			    (external && wpa_s->sme.ext_ml_auth)) &&
			   token_len > 12 &&
			   token_pos[token_len - 12] == WLAN_EID_EXTENSION &&
			   token_pos[token_len - 11] == 10 &&
			   token_pos[token_len - 10] ==
			   WLAN_EID_EXT_MULTI_LINK) {
			/* IEEE P802.11be requires H2E to be used whenever SAE
			 * is used for ML association. However, some early
			 * Wi-Fi 7 APs enable MLO without H2E. Recognize this
			 * special case based on the fixed length Basic
			 * Multi-Link element being at the end of the data that
			 * would contain the unknown variable length
			 * Anti-Clogging Token field. The Basic Multi-Link
			 * element in Authentication frames include the MLD MAC
			 * addreess in the Common Info field and all subfields
			 * of the Presence Bitmap subfield of the Multi-Link
			 * Control field of the element zero and consequently,
			 * has a fixed length of 12 octets. */
			wpa_printf(MSG_DEBUG,
				   "SME: Detected Basic Multi-Link element at the end of Anti-Clogging Token field");
			token_len -= 12;
#endif /* CONFIG_IEEE80211BE */
		}

		*ie_offset = token_pos + token_len - data;

		wpa_s->sme.sae_token = wpabuf_alloc_copy(token_pos, token_len);
		if (!wpa_s->sme.sae_token) {
			wpa_dbg(wpa_s, MSG_ERROR,
				"SME: Failed to allocate SAE token");
			return -1;
		}

		wpa_hexdump_buf(MSG_DEBUG, "SME: Requested anti-clogging token",
				wpa_s->sme.sae_token);
		if (!external) {
			sme_send_authentication(wpa_s, wpa_s->current_bss,
						wpa_s->current_ssid, 2);
		} else {
			if (wpa_s->sme.ext_ml_auth &&
			    sme_external_ml_auth(wpa_s, data, len, *ie_offset,
						 status_code))
				return -1;

			sme_external_auth_send_sae_commit(
				wpa_s, wpa_s->sme.ext_auth_bssid,
				wpa_s->sme.ext_auth_wpa_ssid);
		}
		return 0;
	}

	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT &&
	    status_code == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED &&
	    wpa_s->sme.sae.state == SAE_COMMITTED &&
	    ((external && wpa_s->sme.ext_auth_wpa_ssid) ||
	     (!external && wpa_s->current_bss && wpa_s->current_ssid))) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: SAE group not supported");
		int_array_add_unique(&wpa_s->sme.sae_rejected_groups,
				     wpa_s->sme.sae.group);
		wpa_s->sme.sae_group_index++;
		if (sme_set_sae_group(wpa_s, external) < 0)
			return -1; /* no other groups enabled */
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Try next enabled SAE group");
		if (!external) {
			sme_send_authentication(wpa_s, wpa_s->current_bss,
						wpa_s->current_ssid, 1);
		} else {
			if (wpa_s->sme.ext_ml_auth &&
			    sme_external_ml_auth(wpa_s, data, len, *ie_offset,
						 status_code))
				return -1;

			sme_external_auth_send_sae_commit(
				wpa_s, wpa_s->sme.ext_auth_bssid,
				wpa_s->sme.ext_auth_wpa_ssid);
		}
		return 0;
	}

	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT &&
	    status_code == WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER) {
		const u8 *bssid = sa ? sa : wpa_s->pending_bssid;
		struct wpa_ssid *ssid = wpa_s->current_ssid;

		if (ssid && ssid->alt_sae_password_ids &&
		    ssid->alt_sae_passwords_ids_used) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Remove alternative password identifier (idx=%u) due to rejection",
				   ssid->alt_sae_passwords_ids_idx);
			wpabuf_array_remove(ssid->alt_sae_password_ids,
					    ssid->alt_sae_passwords_ids_idx);

#ifndef CONFIG_NO_CONFIG_WRITE
			if (wpa_s->conf->update_config &&
			    wpa_config_write(wpa_s->confname, wpa_s->conf))
				wpa_printf(MSG_DEBUG,
					   "SAE: Failed to update configuration");
#endif /* CONFIG_NO_CONFIG_WRITE */
		}

		wpa_msg(wpa_s, MSG_INFO,
			WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER MACSTR,
			MAC2STR(bssid));
		return -1;
	}

	if (status_code != WLAN_STATUS_SUCCESS &&
	    status_code != WLAN_STATUS_SAE_HASH_TO_ELEMENT &&
	    status_code != WLAN_STATUS_SAE_PK) {
		const u8 *bssid = sa ? sa : wpa_s->pending_bssid;

		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_AUTH_REJECT MACSTR
			" auth_type=%u auth_transaction=%u status_code=%u",
			MAC2STR(bssid), WLAN_AUTH_SAE,
			auth_transaction, status_code);
		return -2;
	}

	if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_COMMIT) {
		u16 res;

		groups = wpa_s->conf->sae_groups;

		wpa_dbg(wpa_s, MSG_DEBUG, "SME SAE commit");
		if ((external && !wpa_s->sme.ext_auth_wpa_ssid) ||
		    (!external &&
		     (!wpa_s->current_bss || !wpa_s->current_ssid)))
			return -1;
		if (wpa_s->sme.sae.state != SAE_COMMITTED) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Ignore commit message while waiting for confirm");
			return 0;
		}
		if (wpa_s->sme.sae.h2e && status_code == WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Unexpected use of status code 0 in SAE commit when H2E was expected");
			return -1;
		}
		if ((!wpa_s->sme.sae.h2e || wpa_s->sme.sae.pk) &&
		    status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Unexpected use of status code for H2E in SAE commit when H2E was not expected");
			return -1;
		}
		if (!wpa_s->sme.sae.pk &&
		    status_code == WLAN_STATUS_SAE_PK) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Unexpected use of status code for PK in SAE commit when PK was not expected");
			return -1;
		}

		if (groups && groups[0] <= 0)
			groups = NULL;
		res = sae_parse_commit(&wpa_s->sme.sae, data, len, NULL, NULL,
				       groups, status_code ==
				       WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
				       status_code == WLAN_STATUS_SAE_PK,
				       ie_offset);
		if (res == SAE_SILENTLY_DISCARD) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Drop commit message due to reflection attack");
			return 0;
		}
		if (res != WLAN_STATUS_SUCCESS)
			return -1;

		if (wpa_s->sme.sae.tmp &&
		    sme_check_sae_rejected_groups(
			    wpa_s,
			    wpa_s->sme.sae.tmp->peer_rejected_groups))
			return -1;

		if (sae_process_commit(&wpa_s->sme.sae) < 0) {
			wpa_printf(MSG_DEBUG, "SAE: Failed to process peer "
				   "commit");
			return -1;
		}

		wpabuf_free(wpa_s->sme.sae_token);
		wpa_s->sme.sae_token = NULL;
		if (!external) {
			sme_send_authentication(wpa_s, wpa_s->current_bss,
						wpa_s->current_ssid, 0);
		} else {
			if (wpa_s->sme.ext_ml_auth &&
			    sme_external_ml_auth(wpa_s, data, len, *ie_offset,
						 status_code))
				return -1;

			sme_external_auth_send_sae_confirm(wpa_s, sa);
		}
		return 0;
	} else if (auth_transaction == WLAN_AUTH_TR_SEQ_SAE_CONFIRM) {
		if (status_code != WLAN_STATUS_SUCCESS)
			return -1;
		wpa_dbg(wpa_s, MSG_DEBUG, "SME SAE confirm");
		if (wpa_s->sme.sae.state != SAE_CONFIRMED)
			return -1;
		if (sae_check_confirm(&wpa_s->sme.sae, data, len,
				      ie_offset) < 0)
			return -1;
		if (external && wpa_s->sme.ext_ml_auth &&
		    sme_external_ml_auth(wpa_s, data, len, *ie_offset,
					 status_code))
			return -1;

		wpa_s->sme.sae.state = SAE_ACCEPTED;
		sae_clear_temp_data(&wpa_s->sme.sae);
		wpa_s_clear_sae_rejected(wpa_s);

		if (external) {
			const u8 *connected_addr = wpa_s->valid_links ?
				wpa_s->ap_mld_addr : wpa_s->bssid;
			const u8 *src = wpa_s->sme.ext_ml_auth ?
				wpa_s->sme.ext_auth_ap_mld_addr :
				wpa_s->sme.ext_auth_bssid;

			wpa_s->ext_auth_to_same_bss =
				wpa_s->wpa_state > WPA_ASSOCIATED &&
				ether_addr_equal(src, connected_addr);

			/* Report success to driver */
			sme_send_external_auth_status(wpa_s,
					WLAN_STATUS_SUCCESS);
		}

		return 1;
	}

	return -1;
}


static int sme_sae_set_pmk(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	wpa_printf(MSG_DEBUG,
		   "SME: SAE completed - setting PMK for 4-way handshake");

	if (wpa_s->current_bss && wpa_s->smd_capable) {
		wpa_printf(MSG_DEBUG, "SMD : Setting SAE PMK with SMD Context");
		wpa_sm_set_smd_params(wpa_s->wpa, wpa_s->smd_id,
				      wpa_s->smd_ptk_mode,
				      wpa_s->smd_me_initial_ap_mld_addr);
	}
	wpa_sm_set_pmk(wpa_s->wpa, wpa_s->sme.sae.pmk, wpa_s->sme.sae.pmk_len,
		       wpa_s->sme.sae.pmkid, bssid);
	if (wpa_s->conf->sae_pmkid_in_assoc) {
		/* Update the own RSNE contents now that we have set the PMK
		 * and added a PMKSA cache entry based on the successfully
		 * completed SAE exchange. In practice, this will add the PMKID
		 * into RSNE. */
		if (wpa_s->sme.assoc_req_ie_len + 2 + PMKID_LEN >
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_msg(wpa_s, MSG_WARNING,
				"RSN: Not enough room for inserting own PMKID into RSNE");
			return -1;
		}
		if (wpa_insert_pmkid(wpa_s->sme.assoc_req_ie,
				     &wpa_s->sme.assoc_req_ie_len,
				     wpa_s->sme.sae.pmkid, true) < 0)
			return -1;
		wpa_hexdump(MSG_DEBUG,
			    "SME: Updated Association Request IEs",
			    wpa_s->sme.assoc_req_ie,
			    wpa_s->sme.assoc_req_ie_len);
	}

	return 0;
}


#ifdef CONFIG_ENC_ASSOC
static void sme_handle_eppke_unknown_password_id(struct wpa_supplicant *wpa_s,
						 bool external)
{
	struct wpa_ssid *ssid;
	const u8 *bssid;

	ssid = external ? wpa_s->sme.ext_auth_wpa_ssid : wpa_s->current_ssid;
	bssid = external ? wpa_s->sme.ext_auth_bssid : wpa_s->pending_bssid;

	if (ssid && ssid->alt_sae_password_ids &&
	    ssid->alt_sae_passwords_ids_used) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: Remove alternative password identifier (idx=%u) due to rejection",
			   ssid->alt_sae_passwords_ids_idx);
		wpabuf_array_remove(ssid->alt_sae_password_ids,
				    ssid->alt_sae_passwords_ids_idx);

#ifndef CONFIG_NO_CONFIG_WRITE
		if (wpa_s->conf->update_config &&
		    wpa_config_write(wpa_s->confname, wpa_s->conf))
			wpa_printf(MSG_DEBUG,
				   "EPPKE: Failed to update configuration");
#endif /* CONFIG_NO_CONFIG_WRITE */
	}

	wpa_msg(wpa_s, MSG_INFO,
		WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER MACSTR,
		MAC2STR(bssid));

	if (external) {
		sme_send_external_auth_status(wpa_s,
					      WLAN_STATUS_UNSPECIFIED_FAILURE);
	} else {
		wpas_connection_failed(wpa_s, bssid, NULL);
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	}
}
#endif /* CONFIG_ENC_ASSOC */


void sme_external_auth_mgmt_rx(struct wpa_supplicant *wpa_s,
			       const u8 *auth_frame, size_t len)
{
	const struct ieee80211_mgmt *header;
	size_t auth_length;

	header = (const struct ieee80211_mgmt *) auth_frame;
	auth_length = IEEE80211_HDRLEN + sizeof(header->u.auth);

	if (len < auth_length) {
		/* Notify failure to the driver */
		sme_send_external_auth_status(wpa_s,
					      WLAN_STATUS_UNSPECIFIED_FAILURE);
		return;
	}

	if (le_to_host16(header->u.auth.auth_alg) == WLAN_AUTH_SAE) {
		int res;
		int ie_offset = 0;

		res = sme_sae_auth(
			wpa_s, le_to_host16(header->u.auth.auth_transaction),
			le_to_host16(header->u.auth.status_code),
			header->u.auth.variable,
			len - auth_length, 1, header->sa, &ie_offset);
		if (res < 0) {
			/* Notify failure to the driver */
			sme_send_external_auth_status(
				wpa_s,
				res == -2 ?
				le_to_host16(header->u.auth.status_code) :
				WLAN_STATUS_UNSPECIFIED_FAILURE);
			return;
		}
		if (res != 1)
			return;

		if (sme_sae_set_pmk(wpa_s,
				    wpa_s->sme.ext_ml_auth ?
				    wpa_s->sme.ext_auth_ap_mld_addr :
				    wpa_s->sme.ext_auth_bssid) < 0)
			return;
#ifdef CONFIG_ENC_ASSOC
	} else if (le_to_host16(header->u.auth.auth_alg) == WLAN_AUTH_EPPKE) {
		int res;
		u16 status_code = le_to_host16(header->u.auth.status_code);

		/*
		 * Handle unknown password identifier rejection for EPPKE:
		 * remove the rejected alternative password identifier and
		 * report the event.
		 */
		if (status_code == WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER) {
			sme_handle_eppke_unknown_password_id(wpa_s, true);
			return;
		}

		res = wpas_pasn_auth_rx(wpa_s, header, len);
		if (res < 0) {
			/* Notify failure to the driver */
			sme_send_external_auth_status(
				wpa_s,
				res == -2 ?
				status_code :
				WLAN_STATUS_UNSPECIFIED_FAILURE);
			return;
		}
#endif /* CONFIG_ENC_ASSOC */
	}
}

#endif /* CONFIG_SAE */


void sme_event_auth(struct wpa_supplicant *wpa_s, union wpa_event_data *data)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	int ie_offset = 0;
	u8 reject_bssid[ETH_ALEN];

	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Ignore authentication event "
			"when network is not selected");
		return;
	}

	if (wpa_s->wpa_state != WPA_AUTHENTICATING) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Ignore authentication event "
			"when not in authenticating state");
		return;
	}

	if (!ether_addr_equal(wpa_s->pending_bssid, data->auth.peer) &&
	    !(wpa_s->valid_links &&
	      ether_addr_equal(wpa_s->ap_mld_addr, data->auth.peer))) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Ignore authentication with "
			"unexpected peer " MACSTR,
			MAC2STR(data->auth.peer));
		return;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Authentication response: peer=" MACSTR
		" auth_type=%d auth_transaction=%d status_code=%d",
		MAC2STR(data->auth.peer), data->auth.auth_type,
		data->auth.auth_transaction, data->auth.status_code);
	wpa_hexdump(MSG_MSGDUMP, "SME: Authentication response IEs",
		    data->auth.ies, data->auth.ies_len);

	eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);

#ifdef CONFIG_ENC_ASSOC
	if (data->auth.auth_type == WLAN_AUTH_EPPKE) {
		struct pasn_data *pasn = &wpa_s->pasn;
		struct wpa_pasn_params_data pasn_params;
		int res;
		enum wpa_alg alg;
		struct ptksa_cache_entry *entry;

		/*
		 * Handle unknown password identifier rejection for EPPKE:
		 * remove the rejected alternative password identifier and
		 * report the event.
		 */
		if (data->auth.status_code ==
		    WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER) {
			sme_handle_eppke_unknown_password_id(wpa_s, false);
			return;
		}

		res = wpas_parse_pasn_frame(pasn, data->auth.auth_type,
					    data->auth.auth_transaction,
					    data->auth.status_code,
					    data->auth.ies, data->auth.ies_len,
					    &pasn_params);
		if (res < 0) {
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
					       NULL);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			return;
		}

		/*
		 * TODO: Add support for comeback if AP rejected group
		 * negotiation temporarily.
		 */

		/*
		 * AP rejected the proposed group; retry with the
		 * next valid group.
		 */
		if (res == 2) {
			struct wpa_bss *bss = wpa_s->current_bss;

			if (!bss) {
				bss = wpa_bss_get_bssid_latest(
					wpa_s, wpa_s->pending_bssid);
				if (!bss) {
					wpas_connection_failed(
						wpa_s, wpa_s->pending_bssid,
						NULL);
					wpa_supplicant_mark_disassoc(wpa_s);
					return;
				}
			}

			wpa_printf(MSG_DEBUG,
				   "EPPKE: Group %u rejected by AP - retrying with next group",
				   pasn->group);
			wpas_connect_work_done(wpa_s);
			wpa_supplicant_mark_disassoc(wpa_s);
			wpa_supplicant_connect(wpa_s, bss, ssid);
			return;
		}

		ptksa_cache_add(wpa_s->ptksa, pasn->own_addr, pasn->peer_addr,
				pasn_get_cipher(pasn),
				dot11RSNAConfigPMKLifetime,
				pasn_get_ptk(pasn), NULL, NULL,
				pasn_get_akmp(pasn), pasn->auth_alg);

		if (pasn->pmksa_entry)
			wpa_sm_set_cur_pmksa(wpa_s->wpa, pasn->pmksa_entry);
		wpa_sm_set_pmk(wpa_s->wpa, pasn->pmk, pasn->pmk_len,
			       pasn->sae.pmkid, NULL);

		sme_send_authentication(wpa_s, wpa_s->current_bss,
					wpa_s->current_ssid, 0);

		if (wpa_s->conf->sae_pmkid_in_assoc) {
			if (wpa_s->sme.assoc_req_ie_len + 2 + PMKID_LEN >
			    sizeof(wpa_s->sme.assoc_req_ie)) {
				wpa_msg(wpa_s, MSG_WARNING,
					"RSN: Not enough room for inserting own PMKID into RSNE");
				return;
			}
			if (wpa_insert_pmkid(wpa_s->sme.assoc_req_ie,
					     &wpa_s->sme.assoc_req_ie_len,
					     pasn->sae.pmkid, true) < 0)
				return;
		}

		alg = wpa_cipher_to_alg(pasn_get_cipher(pasn));
		entry = ptksa_cache_get(wpa_s->ptksa, pasn->peer_addr,
					pasn_get_cipher(pasn));
		if (!entry) {
			wpa_printf(MSG_INFO,
				   "EPPKE: No PTKSA found to configure TK");
			return;
		}

		wpa_drv_set_key(wpa_s, -1, alg, pasn->peer_addr, 0, 1,
				NULL, 0, entry->ptk.tk, entry->ptk.tk_len,
				KEY_FLAG_PAIRWISE_RX_TX);
	}
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_SAE
	if (data->auth.auth_type == WLAN_AUTH_SAE) {
		const u8 *addr = wpa_s->pending_bssid;
		int res;

		res = sme_sae_auth(wpa_s, data->auth.auth_transaction,
				   data->auth.status_code, data->auth.ies,
				   data->auth.ies_len, 0, data->auth.peer,
				   &ie_offset);
		if (res < 0) {
			if (data->auth.auth_transaction ==
			    WLAN_AUTH_TR_SEQ_SAE_CONFIRM &&
			    data->auth.status_code ==
			    WLAN_STATUS_CHALLENGE_FAIL)
				wpas_notify_sae_password_mismatch(wpa_s);
			/*
			 * Reset to DISCONNECTED and clear pending_bssid before
			 * reporting the failure. wpas_connection_failed() may
			 * re-enter wpa_supplicant_connect() via fast-associate;
			 * if the state still shows AUTHENTICATING with the same
			 * pending BSSID, that path takes the "already trying to
			 * connect" branch and suppresses the recovery scan,
			 * leaving the STA idle with nothing scheduled. Preserve
			 * the rejected BSSID so it is still added to the ignore
			 * list.
			 */
			os_memcpy(reject_bssid, wpa_s->pending_bssid, ETH_ALEN);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
			wpas_connection_failed(wpa_s, reject_bssid, NULL);

			if (wpa_s->sme.sae_rejected_groups &&
			    ssid->disabled_until.sec) {
				wpa_printf(MSG_DEBUG,
					   "SME: Clear SAE state with rejected groups due to continuous failures");
				wpa_s_clear_sae_rejected(wpa_s);
			}
		}
		if (res != 1)
			return;

		if (wpa_s->valid_links)
			addr = wpa_s->ap_mld_addr;

		if (sme_sae_set_pmk(wpa_s, addr) < 0)
			return;
	}
#endif /* CONFIG_SAE */

	if (data->auth.status_code != WLAN_STATUS_SUCCESS) {
		char *ie_txt = NULL;

		if (data->auth.ies && data->auth.ies_len) {
			size_t buflen = 2 * data->auth.ies_len + 1;
			ie_txt = os_malloc(buflen);
			if (ie_txt) {
				wpa_snprintf_hex(ie_txt, buflen, data->auth.ies,
						 data->auth.ies_len);
			}
		}
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_AUTH_REJECT MACSTR
			" auth_type=%u auth_transaction=%u status_code=%u%s%s",
			MAC2STR(data->auth.peer), data->auth.auth_type,
			data->auth.auth_transaction, data->auth.status_code,
			ie_txt ? " ie=" : "",
			ie_txt ? ie_txt : "");
		os_free(ie_txt);

#ifdef CONFIG_FILS
		if (wpa_s->sme.auth_alg == WPA_AUTH_ALG_FILS ||
		    wpa_s->sme.auth_alg == WPA_AUTH_ALG_FILS_SK_PFS)
			fils_connection_failure(wpa_s);
#endif /* CONFIG_FILS */

		if (data->auth.status_code !=
		    WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG ||
		    wpa_s->sme.auth_alg == data->auth.auth_type ||
		    wpa_s->current_ssid->auth_alg == WPA_AUTH_ALG_LEAP) {
			os_memcpy(reject_bssid, wpa_s->pending_bssid, ETH_ALEN);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
			wpas_connection_failed(wpa_s, reject_bssid, NULL);
			return;
		}

		wpas_connect_work_done(wpa_s);

		switch (data->auth.auth_type) {
		case WLAN_AUTH_OPEN:
			wpa_s->current_ssid->auth_alg = WPA_AUTH_ALG_SHARED;

			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Trying SHARED auth");
			wpa_supplicant_associate(wpa_s, wpa_s->current_bss,
						 wpa_s->current_ssid);
			return;

		case WLAN_AUTH_SHARED_KEY:
			wpa_s->current_ssid->auth_alg = WPA_AUTH_ALG_LEAP;

			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Trying LEAP auth");
			wpa_supplicant_associate(wpa_s, wpa_s->current_bss,
						 wpa_s->current_ssid);
			return;

		default:
			return;
		}
	}

#ifdef CONFIG_IEEE80211R
	if (data->auth.auth_type == WLAN_AUTH_FT) {
		const u8 *ric_ies = NULL;
		size_t ric_ies_len = 0;

		if (wpa_s->ric_ies) {
			ric_ies = wpabuf_head(wpa_s->ric_ies);
			ric_ies_len = wpabuf_len(wpa_s->ric_ies);
		}
		if (wpa_ft_process_response(wpa_s->wpa, data->auth.ies,
					    data->auth.ies_len, 0,
					    data->auth.peer,
					    ric_ies, ric_ies_len) < 0) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SME: FT Authentication response processing failed");
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid="
				MACSTR
				" reason=%d locally_generated=1",
				MAC2STR(wpa_s->pending_bssid),
				WLAN_REASON_DEAUTH_LEAVING);
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
					       NULL);
			wpa_supplicant_mark_disassoc(wpa_s);
			return;
		}
	}
#endif /* CONFIG_IEEE80211R */

#ifdef CONFIG_FILS
	if (data->auth.auth_type == WLAN_AUTH_FILS_SK ||
	    data->auth.auth_type == WLAN_AUTH_FILS_SK_PFS) {
		u16 expect_auth_type;

		expect_auth_type = wpa_s->sme.auth_alg ==
			WPA_AUTH_ALG_FILS_SK_PFS ? WLAN_AUTH_FILS_SK_PFS :
			WLAN_AUTH_FILS_SK;
		if (data->auth.auth_type != expect_auth_type) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SME: FILS Authentication response used different auth alg (%u; expected %u)",
				data->auth.auth_type, expect_auth_type);
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid="
				MACSTR
				" reason=%d locally_generated=1",
				MAC2STR(wpa_s->pending_bssid),
				WLAN_REASON_DEAUTH_LEAVING);
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
					       NULL);
			wpa_supplicant_mark_disassoc(wpa_s);
			return;
		}

		if (fils_process_auth(wpa_s->wpa, wpa_s->pending_bssid,
				      data->auth.ies, data->auth.ies_len) < 0) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SME: FILS Authentication response processing failed");
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid="
				MACSTR
				" reason=%d locally_generated=1",
				MAC2STR(wpa_s->pending_bssid),
				WLAN_REASON_DEAUTH_LEAVING);
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
					       NULL);
			wpa_supplicant_mark_disassoc(wpa_s);
			return;
		}
	}
#endif /* CONFIG_FILS */

	/* TODO: Support additional auth_type values as well */
	if ((data->auth.auth_type == WLAN_AUTH_OPEN ||
	     data->auth.auth_type == WLAN_AUTH_SAE) &&
	    wpas_sme_ml_auth(wpa_s, data, ie_offset) < 0) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"MLD: Failed to parse ML Authentication frame");
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid=" MACSTR
			" reason=%d locally_generated=1",
			MAC2STR(wpa_s->pending_bssid),
			WLAN_REASON_DEAUTH_LEAVING);
		wpas_connection_failed(wpa_s, wpa_s->pending_bssid, NULL);
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
		wpa_printf(MSG_DEBUG,
			   "MLD: Authentication - clearing MLD state");
		wpas_reset_mlo_info(wpa_s);
		return;
	}

	if (data->auth.ies && data->auth.ies_len > 0) {
		struct ieee802_11_elems elems;

		if (ieee802_11_parse_elems(data->auth.ies + ie_offset,
					   data->auth.ies_len - ie_offset,
					   & elems, 0) != ParseFailed) {
			if (elems.smd && elems.smd_len >= 10) {
				u8 smd_identifier[6];
				u8 ptk_mode, capabilities;
				u16 timeout;

				if (wpas_parse_smd_ie(elems.smd, elems.smd_len,
						      smd_identifier, &ptk_mode,
						      &capabilities, &timeout) == 0) {
					wpa_printf(MSG_DEBUG, "SME: Parsed SMD IE from auth response: "
						   "SMD_ID=" MACSTR " PTK_mode=%u capabilities=0x%02x timeout=%u",
						   MAC2STR(smd_identifier), ptk_mode, capabilities, timeout);

					if (wpa_s->current_bss) {
						os_memcpy(wpa_s->current_bss->smd_identifier, smd_identifier, 6);
						wpa_s->current_bss->smd_capabilities = capabilities;
						wpa_s->current_bss->smd_ptk_mode = ptk_mode;
						wpa_s->current_bss->smd_timeout = timeout;
					}
				} else {
					wpa_printf(MSG_WARNING, "SME: Failed to parse SMD IE from auth response");
				}
			}
		}
	}

	sme_associate(wpa_s, ssid->mode, data->auth.peer,
		      data->auth.auth_type);
}


#ifdef CONFIG_IEEE80211R
static void remove_ie(u8 *buf, size_t *len, u8 eid)
{
	u8 *pos, *next, *end;

	pos = (u8 *) get_ie(buf, *len, eid);
	if (pos) {
		next = pos + 2 + pos[1];
		end = buf + *len;
		*len -= 2 + pos[1];
		os_memmove(pos, next, end - next);
	}
}
#endif /* CONFIG_IEEE80211R */


void sme_associate(struct wpa_supplicant *wpa_s, enum wpas_mode mode,
		   const u8 *bssid, u16 auth_type)
{
	struct wpa_driver_associate_params params;
	struct ieee802_11_elems elems;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
#ifdef CONFIG_FILS
	u8 nonces[2 * NONCE_LEN];
#endif /* CONFIG_FILS */
#ifdef CONFIG_HT_OVERRIDES
	struct ieee80211_ht_capabilities htcaps;
	struct ieee80211_ht_capabilities htcaps_mask;
#endif /* CONFIG_HT_OVERRIDES */
#ifdef CONFIG_VHT_OVERRIDES
	struct ieee80211_vht_capabilities vhtcaps;
	struct ieee80211_vht_capabilities vhtcaps_mask;
#endif /* CONFIG_VHT_OVERRIDES */
#ifdef CONFIG_PMKSA_PRIVACY
	const u8 *ap_rsnxe;
#endif /* CONFIG_PMKSA_PRIVACY */

	os_memset(&params, 0, sizeof(params));

	/* Save auth type, in case we need to retry after comeback timer. */
	wpa_s->sme.assoc_auth_type = auth_type;

#ifdef CONFIG_FILS
	if (auth_type == WLAN_AUTH_FILS_SK ||
	    auth_type == WLAN_AUTH_FILS_SK_PFS) {
		struct wpabuf *buf;
		const u8 *snonce, *anonce;
		const unsigned int max_hlp = 20;
		struct wpabuf *hlp[max_hlp];
		unsigned int i, num_hlp = 0;
		struct fils_hlp_req *req;

		dl_list_for_each(req, &wpa_s->fils_hlp_req, struct fils_hlp_req,
				 list) {
			hlp[num_hlp] = wpabuf_alloc(2 * ETH_ALEN + 6 +
					      wpabuf_len(req->pkt));
			if (!hlp[num_hlp])
				break;
			wpabuf_put_data(hlp[num_hlp], req->dst, ETH_ALEN);
			wpabuf_put_data(hlp[num_hlp], wpa_s->own_addr,
					ETH_ALEN);
			wpabuf_put_data(hlp[num_hlp],
					"\xaa\xaa\x03\x00\x00\x00", 6);
			wpabuf_put_buf(hlp[num_hlp], req->pkt);
			num_hlp++;
			if (num_hlp >= max_hlp)
				break;
		}

		buf = fils_build_assoc_req(wpa_s->wpa, &params.fils_kek,
					   &params.fils_kek_len, &snonce,
					   &anonce,
					   (const struct wpabuf **) hlp,
					   num_hlp);
		for (i = 0; i < num_hlp; i++)
			wpabuf_free(hlp[i]);
		if (!buf)
			return;
		wpa_hexdump(MSG_DEBUG, "FILS: assoc_req before FILS elements",
			    wpa_s->sme.assoc_req_ie,
			    wpa_s->sme.assoc_req_ie_len);
#ifdef CONFIG_IEEE80211R
		if (wpa_key_mgmt_ft(wpa_s->key_mgmt)) {
			/* Remove RSNE and MDE to allow them to be overridden
			 * with FILS+FT specific values from
			 * fils_build_assoc_req(). */
			remove_ie(wpa_s->sme.assoc_req_ie,
				  &wpa_s->sme.assoc_req_ie_len,
				  WLAN_EID_RSN);
			wpa_hexdump(MSG_DEBUG,
				    "FILS: assoc_req after RSNE removal",
				    wpa_s->sme.assoc_req_ie,
				    wpa_s->sme.assoc_req_ie_len);
			remove_ie(wpa_s->sme.assoc_req_ie,
				  &wpa_s->sme.assoc_req_ie_len,
				  WLAN_EID_MOBILITY_DOMAIN);
			wpa_hexdump(MSG_DEBUG,
				    "FILS: assoc_req after MDE removal",
				    wpa_s->sme.assoc_req_ie,
				    wpa_s->sme.assoc_req_ie_len);
		}
#endif /* CONFIG_IEEE80211R */
		/* TODO: Make wpa_s->sme.assoc_req_ie use dynamic allocation */
		if (wpa_s->sme.assoc_req_ie_len + wpabuf_len(buf) >
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_printf(MSG_ERROR,
				   "FILS: Not enough buffer room for own AssocReq elements");
			wpabuf_free(buf);
			return;
		}
		os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			  wpabuf_head(buf), wpabuf_len(buf));
		wpa_s->sme.assoc_req_ie_len += wpabuf_len(buf);
		wpabuf_free(buf);
		wpa_hexdump(MSG_DEBUG, "FILS: assoc_req after FILS elements",
			    wpa_s->sme.assoc_req_ie,
			    wpa_s->sme.assoc_req_ie_len);

		os_memcpy(nonces, snonce, NONCE_LEN);
		os_memcpy(nonces + NONCE_LEN, anonce, NONCE_LEN);
		params.fils_nonces = nonces;
		params.fils_nonces_len = sizeof(nonces);
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
#ifdef CONFIG_TESTING_OPTIONS
	if (get_ie_ext(wpa_s->sme.assoc_req_ie, wpa_s->sme.assoc_req_ie_len,
		       WLAN_EID_EXT_OWE_DH_PARAM)) {
		wpa_printf(MSG_INFO, "TESTING: Override OWE DH element");
	} else
#endif /* CONFIG_TESTING_OPTIONS */
	if (auth_type == WLAN_AUTH_OPEN &&
	    wpa_s->key_mgmt == WPA_KEY_MGMT_OWE) {
		struct wpabuf *owe_ie;
		u16 group;

		if (ssid && ssid->owe_group) {
			group = ssid->owe_group;
		} else if (wpa_s->assoc_status_code ==
			   WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED) {
			if (wpa_s->last_owe_group == 19)
				group = 20;
			else if (wpa_s->last_owe_group == 20)
				group = 21;
			else
				group = OWE_DH_GROUP;
		} else {
			group = OWE_DH_GROUP;
		}

		wpa_s->last_owe_group = group;
		wpa_printf(MSG_DEBUG, "OWE: Try to use group %u", group);
		owe_ie = owe_build_assoc_req(wpa_s->wpa, group);
		if (!owe_ie) {
			wpa_printf(MSG_ERROR,
				   "OWE: Failed to build IE for Association Request frame");
			return;
		}
		if (wpa_s->sme.assoc_req_ie_len + wpabuf_len(owe_ie) >
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_printf(MSG_ERROR,
				   "OWE: Not enough buffer room for own Association Request frame elements");
			wpabuf_free(owe_ie);
			return;
		}
		os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			  wpabuf_head(owe_ie), wpabuf_len(owe_ie));
		wpa_s->sme.assoc_req_ie_len += wpabuf_len(owe_ie);
		wpabuf_free(owe_ie);
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
	if (DPP_VERSION > 1 && wpa_s->key_mgmt == WPA_KEY_MGMT_DPP && ssid &&
	    ssid->dpp_netaccesskey && ssid->dpp_pfs != 2 &&
	    !ssid->dpp_pfs_fallback) {
		struct rsn_pmksa_cache_entry *pmksa;

		pmksa = pmksa_cache_get_current(wpa_s->wpa);
		if (!pmksa || !pmksa->dpp_pfs)
			goto pfs_fail;

		dpp_pfs_free(wpa_s->dpp_pfs);
		wpa_s->dpp_pfs = dpp_pfs_init(ssid->dpp_netaccesskey,
					      ssid->dpp_netaccesskey_len);
		if (!wpa_s->dpp_pfs) {
			wpa_printf(MSG_DEBUG, "DPP: Could not initialize PFS");
			/* Try to continue without PFS */
			goto pfs_fail;
		}
		if (wpa_s->sme.assoc_req_ie_len +
		    wpabuf_len(wpa_s->dpp_pfs->ie) >
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_printf(MSG_ERROR,
				   "DPP: Not enough buffer room for own Association Request frame elements");
			dpp_pfs_free(wpa_s->dpp_pfs);
			wpa_s->dpp_pfs = NULL;
			goto pfs_fail;
		}
		os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			  wpabuf_head(wpa_s->dpp_pfs->ie),
			  wpabuf_len(wpa_s->dpp_pfs->ie));
		wpa_s->sme.assoc_req_ie_len += wpabuf_len(wpa_s->dpp_pfs->ie);
	}
pfs_fail:
#endif /* CONFIG_DPP2 */

#ifndef CONFIG_NO_ROBUST_AV
	wpa_s->mscs_setup_done = false;
	if (wpa_bss_ext_capab(wpa_s->current_bss, WLAN_EXT_CAPAB_MSCS) &&
	    wpa_s->robust_av.valid_config) {
		struct wpabuf *mscs_ie;
		size_t mscs_ie_len, buf_len, *wpa_ie_len, max_ie_len;

		buf_len = 3 +	/* MSCS descriptor IE header */
			  1 +	/* Request type */
			  2 +	/* User priority control */
			  4 +	/* Stream timeout */
			  3 +	/* TCLAS Mask IE header */
			  wpa_s->robust_av.frame_classifier_len;
		mscs_ie = wpabuf_alloc(buf_len);
		if (!mscs_ie) {
			wpa_printf(MSG_INFO,
				   "MSCS: Failed to allocate MSCS IE");
			goto mscs_fail;
		}

		wpa_ie_len = &wpa_s->sme.assoc_req_ie_len;
		max_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		wpas_populate_mscs_descriptor_ie(&wpa_s->robust_av, mscs_ie);
		if ((*wpa_ie_len + wpabuf_len(mscs_ie)) <= max_ie_len) {
			wpa_hexdump_buf(MSG_MSGDUMP, "MSCS IE", mscs_ie);
			mscs_ie_len = wpabuf_len(mscs_ie);
			os_memcpy(wpa_s->sme.assoc_req_ie + *wpa_ie_len,
				  wpabuf_head(mscs_ie), mscs_ie_len);
			*wpa_ie_len += mscs_ie_len;
		}

		wpabuf_free(mscs_ie);
	}
mscs_fail:
#endif /* CONFIG_NO_ROBUST_AV */

	wpa_s->sme.assoc_req_ie_len =
		wpas_populate_wfa_capa(wpa_s, wpa_s->current_bss,
				       wpa_s->sme.assoc_req_ie,
				       wpa_s->sme.assoc_req_ie_len,
				       sizeof(wpa_s->sme.assoc_req_ie));

	if (ssid && ssid->multi_ap_backhaul_sta) {
		size_t multi_ap_ie_len;
		struct multi_ap_params multi_ap = { 0 };

		multi_ap.capability = MULTI_AP_BACKHAUL_STA;
		multi_ap.profile = ssid->multi_ap_profile;

		multi_ap_ie_len = add_multi_ap_ie(
			wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			sizeof(wpa_s->sme.assoc_req_ie) -
			wpa_s->sme.assoc_req_ie_len,
			&multi_ap);
		if (multi_ap_ie_len == 0) {
			wpa_printf(MSG_ERROR,
				   "Multi-AP: Failed to build Multi-AP IE");
			return;
		}
		wpa_s->sme.assoc_req_ie_len += multi_ap_ie_len;
	}

#ifdef CONFIG_QCN_EXTN
	/* WDS vendor IE: add to assoc request if wds_ie is enabled */
	if (ssid && ssid->wds_ie) {
		u8 *pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
		size_t avail = sizeof(wpa_s->sme.assoc_req_ie) -
			       wpa_s->sme.assoc_req_ie_len;
		u8 *end = wds_ie_populate_assoc_req_extn(wpa_s, ssid,
							 pos, avail);

		wpa_s->sme.assoc_req_ie_len += end - pos;
	}
#endif /* CONFIG_QCN_EXTN */

	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_RSN_OVERRIDE_SUPPORT,
			 wpas_rsn_overriding(wpa_s, ssid));
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_RSN_OVERRIDE,
			 RSN_OVERRIDE_NOT_USED);
	if (wpas_rsn_overriding(wpa_s, ssid) &&
	    wpas_ap_supports_rsn_overriding(wpa_s, wpa_s->current_bss) &&
	    wpa_s->sme.assoc_req_ie_len + 2 + 4 <=
	    sizeof(wpa_s->sme.assoc_req_ie)) {
		u8 *pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
		const u8 *ie;
		enum rsn_selection_variant variant = RSN_SELECTION_RSNE;

		wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_RSN_OVERRIDE,
				 RSN_OVERRIDE_RSNE);
		ie = wpa_bss_get_rsne(wpa_s, wpa_s->current_bss, ssid,
				      wpa_s->valid_links);
		if (ie && ie[0] == WLAN_EID_VENDOR_SPECIFIC && ie[1] >= 4) {
			u32 type;

			type = WPA_GET_BE32(&ie[2]);
			if (type == RSNE_OVERRIDE_IE_VENDOR_TYPE) {
				variant = RSN_SELECTION_RSNE_OVERRIDE;
				wpa_sm_set_param(wpa_s->wpa,
						 WPA_PARAM_RSN_OVERRIDE,
						 RSN_OVERRIDE_RSNE_OVERRIDE);
			} else if (type == RSNE_OVERRIDE_2_IE_VENDOR_TYPE) {
				variant = RSN_SELECTION_RSNE_OVERRIDE_2;
				wpa_sm_set_param(wpa_s->wpa,
						 WPA_PARAM_RSN_OVERRIDE,
						 RSN_OVERRIDE_RSNE_OVERRIDE_2);
			}
		}

		/* Indicate which RSNE variant was used */
		*pos++ = WLAN_EID_VENDOR_SPECIFIC;
		*pos++ = 4 + 1;
		WPA_PUT_BE32(pos, RSN_SELECTION_IE_VENDOR_TYPE);
		pos += 4;
		*pos = variant;
		wpa_s->sme.assoc_req_ie_len += 2 + 4 + 1;
	}

	if ((ssid->control_frame_protection &&
	    (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_CIGTK) &&
	    (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_CIP_PADDING_SUPPORT) &&
	    (ssid->cip_padding_delay > 0))) {

		struct wpabuf *cip_ie = cip_build_assoc_req(
					WLAN_EID_EXT_CIP_CAPAB,
					ssid->cip_padding_delay);

		if (!cip_ie) {
			wpa_printf(MSG_ERROR, "CIP: Failed to build IE");
			return;
		}

		if (wpa_s->sme.assoc_req_ie_len + wpabuf_len(cip_ie) >
		     sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_printf(MSG_ERROR, "CIP: Not enough buffer room");
			wpabuf_free(cip_ie);
			return;
		}

		os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			  wpabuf_head(cip_ie), wpabuf_len(cip_ie));
		wpa_s->sme.assoc_req_ie_len += wpabuf_len(cip_ie);
		wpabuf_free(cip_ie);
	}

	/*
	 * Security Profile element — assoc frame inclusion (37.32):
	 * Include unconditionally when a profile has been selected (unlike the
	 * auth frame, inclusion here does not depend on RSNE presence).
	 * The element was pre-built in wpa_supplicant_set_suites() from the
	 * same wpa_sm state as the RSNE and RSNXE, guaranteeing consistency.
	 */
	if (wpa_s->security_profile_ie_len > 0 &&
	    wpa_s->security_profile_ie_len <=
	    sizeof(wpa_s->sme.assoc_req_ie) - wpa_s->sme.assoc_req_ie_len) {
		os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
			  wpa_s->security_profile_ie,
			  wpa_s->security_profile_ie_len);
		wpa_s->sme.assoc_req_ie_len += wpa_s->security_profile_ie_len;
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: Appended Security Profile element to assoc frame IEs (profile=%d)",
			wpa_s->selected_security_profile_num);
	}

#ifdef CONFIG_QCN_EXTN
	wpas_add_qcn_ie_assoc_req_extn(wpa_s);
#endif /* CONFIG_QCN_EXTN */
	if (wpa_s->current_bss && wpa_s->smd_capable) {
		u8 *smd_ie;
		size_t smd_ie_len;
		u8 ptk_mode = 0;
		u8 capabilities = wpa_s->current_bss->smd_capabilities ?
			wpa_s->current_bss->smd_capabilities : 0x01;
		u16 timeout = wpa_s->current_bss->smd_timeout ?
			wpa_s->current_bss->smd_timeout : 1000;

		if (ssid && ssid->smd_ptk_mode == 1)
			ptk_mode = 1;
		params.smd.enabled = wpa_s->smd_capable;
		os_memcpy(params.smd.smd_identifier,
			  wpa_s->current_bss->smd_identifier, ETH_ALEN);
		params.smd.smd_timeout = timeout;
		params.smd.caps.max_prep_target_apmlds = 0;
		params.smd.caps.smd_type = wpa_s->current_bss->smd_type;
		params.smd.caps.ptk_mode = ptk_mode;
		smd_ie = wpas_build_smd_ie(wpa_s->current_bss->smd_identifier,
					   ptk_mode, capabilities, timeout, &smd_ie_len);
		if (smd_ie) {
			if (wpa_s->sme.assoc_req_ie_len + smd_ie_len <=
			    sizeof(wpa_s->sme.assoc_req_ie)) {
				os_memcpy(wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len,
					  smd_ie, smd_ie_len);
				wpa_s->sme.assoc_req_ie_len += smd_ie_len;
				wpa_printf(MSG_DEBUG, "SME: Added SMD IE to association request (PTK Mode %u)",
					   ptk_mode);
			} else {
				wpa_printf(MSG_WARNING, "SME: Not enough buffer space for SMD IE in association request");
			}

			os_free(smd_ie);
		}
	}
#ifdef CONFIG_PMKSA_PRIVACY
	ap_rsnxe = wpa_bss_get_rsnxe(wpa_s, wpa_s->current_bss,
				     NULL, wpa_s->valid_links);
	if (ssid->pmksa_privacy &&
	    (wpa_s->drv_flags2 &
	     WPA_DRIVER_FLAGS2_ASSOCIATION_FRAME_ENCRYPTION) &&
	    ieee802_11_rsnx_capab(ap_rsnxe,
				  WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY)) {
		size_t len = 3 + NONCE_LEN;
		u8 *pos;

		if (wpa_s->sme.assoc_req_ie_len + len >
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_printf(MSG_INFO,
			"PMKID privacy: AssocReq IE buffer too small for SNonce");
			return;
		}

		if (os_get_random(wpa_s->pmkid_snonce, NONCE_LEN) < 0) {
			wpa_printf(MSG_INFO,
				   "PMKID privacy: Failed to generate SNonce");
			return;
		}

		pos = &wpa_s->sme.assoc_req_ie[wpa_s->sme.assoc_req_ie_len];
		*pos++ = WLAN_EID_EXTENSION; /* EID Extension */
		*pos++ = 1 + NONCE_LEN; /* Length */
		*pos++ = WLAN_EID_EXT_NONCE; /* Element ID */
		os_memcpy(pos, wpa_s->pmkid_snonce, NONCE_LEN);
		wpa_s->sme.assoc_req_ie_len += len;
		wpa_hexdump(MSG_DEBUG, "PMKID privacy: SNonce in Assoc Request",
			    wpa_s->pmkid_snonce, NONCE_LEN);
		wpa_s->pmkid_snonce_set = true;
	}
#endif /* CONFIG_PMKSA_PRIVACY */


	params.bssid = bssid;
	params.ssid = wpa_s->sme.ssid;
	params.ssid_len = wpa_s->sme.ssid_len;
	params.freq.freq = wpa_s->sme.freq;
	params.bg_scan_period = ssid ? ssid->bg_scan_period : -1;
	params.sta_max_bw = ssid ? ssid->sta_max_bw : 0;
	params.wpa_ie = wpa_s->sme.assoc_req_ie_len ?
		wpa_s->sme.assoc_req_ie : NULL;
	params.wpa_ie_len = wpa_s->sme.assoc_req_ie_len;
	wpa_hexdump(MSG_DEBUG, "SME: Association Request IEs",
		    params.wpa_ie, params.wpa_ie_len);
	params.pairwise_suite = wpa_s->pairwise_cipher;
	params.group_suite = wpa_s->group_cipher;
	params.mgmt_group_suite = wpa_s->mgmt_group_cipher;
	params.key_mgmt_suite = wpa_s->key_mgmt;
	params.wpa_proto = wpa_s->wpa_proto;
#ifdef CONFIG_HT_OVERRIDES
	os_memset(&htcaps, 0, sizeof(htcaps));
	os_memset(&htcaps_mask, 0, sizeof(htcaps_mask));
	params.htcaps = (u8 *) &htcaps;
	params.htcaps_mask = (u8 *) &htcaps_mask;
	wpa_supplicant_apply_ht_overrides(wpa_s, ssid, &params);
#endif /* CONFIG_HT_OVERRIDES */
#ifdef CONFIG_VHT_OVERRIDES
	os_memset(&vhtcaps, 0, sizeof(vhtcaps));
	os_memset(&vhtcaps_mask, 0, sizeof(vhtcaps_mask));
	params.vhtcaps = &vhtcaps;
	params.vhtcaps_mask = &vhtcaps_mask;
	wpa_supplicant_apply_vht_overrides(wpa_s, ssid, &params);
#endif /* CONFIG_VHT_OVERRIDES */
#ifdef CONFIG_HE_OVERRIDES
	wpa_supplicant_apply_he_overrides(wpa_s, ssid, &params);
#endif /* CONFIG_HE_OVERRIDES */
	wpa_supplicant_apply_eht_overrides(wpa_s, ssid, &params);
#ifdef CONFIG_IEEE80211R
	if (auth_type == WLAN_AUTH_FT && wpa_s->sme.ft_ies &&
	    get_ie(wpa_s->sme.ft_ies, wpa_s->sme.ft_ies_len,
		   WLAN_EID_RIC_DATA)) {
		/* There seems to be a pretty inconvenient bug in the Linux
		 * kernel IE splitting functionality when RIC is used. For now,
		 * skip correct behavior in IE construction here (i.e., drop the
		 * additional non-FT-specific IEs) to avoid kernel issues. This
		 * is fine since RIC is used only for testing purposes in the
		 * current implementation. */
		wpa_printf(MSG_INFO,
			   "SME: Linux kernel workaround - do not try to include additional IEs with RIC");
		params.wpa_ie = wpa_s->sme.ft_ies;
		params.wpa_ie_len = wpa_s->sme.ft_ies_len;
	} else if (auth_type == WLAN_AUTH_FT && wpa_s->sme.ft_ies) {
		const u8 *rm_en, *pos, *end;
		size_t rm_en_len = 0;
		u8 *rm_en_dup = NULL, *wpos;

		/* Remove RSNE, MDE, FTE to allow them to be overridden with
		 * FT specific values */
		remove_ie(wpa_s->sme.assoc_req_ie,
			  &wpa_s->sme.assoc_req_ie_len,
			  WLAN_EID_RSN);
		remove_ie(wpa_s->sme.assoc_req_ie,
			  &wpa_s->sme.assoc_req_ie_len,
			  WLAN_EID_MOBILITY_DOMAIN);
		remove_ie(wpa_s->sme.assoc_req_ie,
			  &wpa_s->sme.assoc_req_ie_len,
			  WLAN_EID_FAST_BSS_TRANSITION);
		rm_en = get_ie(wpa_s->sme.assoc_req_ie,
			       wpa_s->sme.assoc_req_ie_len,
			       WLAN_EID_RRM_ENABLED_CAPABILITIES);
		if (rm_en) {
			/* Need to remove RM Enabled Capabilities element as
			 * well temporarily, so that it can be placed between
			 * RSNE and MDE. */
			rm_en_len = 2 + rm_en[1];
			rm_en_dup = os_memdup(rm_en, rm_en_len);
			remove_ie(wpa_s->sme.assoc_req_ie,
				  &wpa_s->sme.assoc_req_ie_len,
				  WLAN_EID_RRM_ENABLED_CAPABILITIES);
		}
		wpa_hexdump(MSG_DEBUG,
			    "SME: Association Request IEs after FT IE removal",
			    wpa_s->sme.assoc_req_ie,
			    wpa_s->sme.assoc_req_ie_len);
		if (wpa_s->sme.assoc_req_ie_len + wpa_s->sme.ft_ies_len +
		    rm_en_len > sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_printf(MSG_ERROR,
				   "SME: Not enough buffer room for FT IEs in Association Request frame");
			os_free(rm_en_dup);
			return;
		}

		os_memmove(wpa_s->sme.assoc_req_ie + wpa_s->sme.ft_ies_len +
			   rm_en_len,
			   wpa_s->sme.assoc_req_ie,
			   wpa_s->sme.assoc_req_ie_len);
		pos = wpa_s->sme.ft_ies;
		end = pos + wpa_s->sme.ft_ies_len;
		wpos = wpa_s->sme.assoc_req_ie;
		if (*pos == WLAN_EID_RSN) {
			os_memcpy(wpos, pos, 2 + pos[1]);
			wpos += 2 + pos[1];
			pos += 2 + pos[1];
		}
		if (rm_en_dup) {
			os_memcpy(wpos, rm_en_dup, rm_en_len);
			wpos += rm_en_len;
			os_free(rm_en_dup);
		}
		os_memcpy(wpos, pos, end - pos);
		wpa_s->sme.assoc_req_ie_len += wpa_s->sme.ft_ies_len +
			rm_en_len;
		params.wpa_ie = wpa_s->sme.assoc_req_ie;
		params.wpa_ie_len = wpa_s->sme.assoc_req_ie_len;
		wpa_hexdump(MSG_DEBUG,
			    "SME: Association Request IEs after FT override",
			    params.wpa_ie, params.wpa_ie_len);
	}
#endif /* CONFIG_IEEE80211R */
	params.mode = mode;
	params.mgmt_frame_protection = wpa_s->sme.mfp;
	params.spp_amsdu = wpa_s->sme.spp_amsdu;
	params.rrm_used = wpa_s->rrm.rrm_used;
	if (wpa_s->sme.prev_bssid_set)
		params.prev_bssid = wpa_s->sme.prev_bssid;

	wpa_msg(wpa_s, MSG_INFO, "Trying to associate with " MACSTR
		" (SSID='%s' freq=%d MHz)", MAC2STR(params.bssid),
		params.ssid ? wpa_ssid_txt(params.ssid, params.ssid_len) : "",
		params.freq.freq);

	if ((ssid->control_frame_protection
	    && (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_CIGTK)))
		params.control_frame_protection = ssid->control_frame_protection;

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATING);

	if (params.wpa_ie == NULL ||
	    ieee802_11_parse_elems(params.wpa_ie, params.wpa_ie_len, &elems, 0)
	    < 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Could not parse own IEs?!");
		os_memset(&elems, 0, sizeof(elems));
	}
	if (elems.rsn_ie) {
		params.wpa_proto = WPA_PROTO_RSN;
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.rsn_ie - 2,
					elems.rsn_ie_len + 2);
	} else if (elems.wpa_ie) {
		params.wpa_proto = WPA_PROTO_WPA;
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.wpa_ie - 2,
					elems.wpa_ie_len + 2);
	} else
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);
	if (elems.rsnxe)
		wpa_sm_set_assoc_rsnxe(wpa_s->wpa, elems.rsnxe - 2,
				       elems.rsnxe_len + 2);
	else
		wpa_sm_set_assoc_rsnxe(wpa_s->wpa, NULL, 0);
	if (ssid && ssid->p2p_group)
		params.p2p = 1;

	if (wpa_s->p2pdev->set_sta_uapsd)
		params.uapsd = wpa_s->p2pdev->sta_uapsd;
	else
		params.uapsd = -1;

	if (wpa_s->valid_links) {
		unsigned int i;

		wpa_printf(MSG_DEBUG,
			   "MLD: In association. assoc_link_id=%u, valid_links=0x%x",
			   wpa_s->mlo_assoc_link_id, wpa_s->valid_links);

		params.mld_params.mld_addr = wpa_s->ap_mld_addr;
		params.mld_params.valid_links = wpa_s->valid_links;
		params.mld_params.assoc_link_id = wpa_s->mlo_assoc_link_id;
		for_each_link(wpa_s->valid_links, i) {
			params.mld_params.mld_links[i].bssid =
				wpa_s->links[i].bssid;
			params.mld_params.mld_links[i].freq =
				wpa_s->links[i].freq;
			params.mld_params.mld_links[i].disabled =
				wpa_s->links[i].disabled;
			if (wpa_s->links[i].ies) {
				params.mld_params.mld_links[i].ies =
					wpabuf_head(wpa_s->links[i].ies);
				params.mld_params.mld_links[i].ies_len =
					wpabuf_len(wpa_s->links[i].ies);
			}

			wpa_printf(MSG_DEBUG,
				   "MLD: id=%u, freq=%d, disabled=%u, ies_len=%zu, "
				   MACSTR,
				   i, wpa_s->links[i].freq,
				   wpa_s->links[i].disabled,
				   params.mld_params.mld_links[i].ies_len,
				   MAC2STR(wpa_s->links[i].bssid));
		}
	}

	if (wpa_drv_associate(wpa_s, &params) < 0) {
		unsigned int n_failed_links = 0;
		int i;

		wpa_msg(wpa_s, MSG_INFO, "SME: Association request to the "
			"driver failed");

		/* Prepare list of failed links for error report */
		for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
			if (!(wpa_s->valid_links & BIT(i)) ||
			    wpa_s->mlo_assoc_link_id == i ||
			    !params.mld_params.mld_links[i].error)
				continue;

			wpa_bssid_ignore_add(wpa_s, wpa_s->links[i].bssid);
			n_failed_links++;
		}

		if (n_failed_links) {
			/* Deauth and connect (possibly to the same AP MLD) */
			wpa_drv_deauthenticate(wpa_s, wpa_s->ap_mld_addr,
					       WLAN_REASON_DEAUTH_LEAVING);
			wpas_connect_work_done(wpa_s);
			wpa_supplicant_mark_disassoc(wpa_s);
			wpas_request_connection(wpa_s);
		} else {
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
					       NULL);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);

#ifdef CONFIG_ENC_ASSOC
			/* Clear configured keys and PTKSA */
			if (wpa_s->ptksa &&
			    ptksa_cache_get(wpa_s->ptksa, wpa_s->bssid,
					    WPA_CIPHER_NONE)) {
				wpa_clear_keys(wpa_s, wpa_s->bssid);
				ptksa_cache_flush(wpa_s->ptksa, wpa_s->bssid,
						  WPA_CIPHER_NONE);
			}
#endif /* CONFIG_ENC_ASSOC */

			os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
		}
		return;
	}

	eloop_register_timeout(SME_ASSOC_TIMEOUT, 0, sme_assoc_timer, wpa_s,
			       NULL);

#ifdef CONFIG_TESTING_OPTIONS
	wpabuf_free(wpa_s->last_assoc_req_wpa_ie);
	wpa_s->last_assoc_req_wpa_ie = NULL;
	if (params.wpa_ie)
		wpa_s->last_assoc_req_wpa_ie =
			wpabuf_alloc_copy(params.wpa_ie, params.wpa_ie_len);
#endif /* CONFIG_TESTING_OPTIONS */
}


int sme_update_ft_ies(struct wpa_supplicant *wpa_s, const u8 *md,
		      const u8 *ies, size_t ies_len)
{
	if (md == NULL || ies == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Remove mobility domain");
		os_free(wpa_s->sme.ft_ies);
		wpa_s->sme.ft_ies = NULL;
		wpa_s->sme.ft_ies_len = 0;
		wpa_s->sme.ft_used = 0;
		return 0;
	}

	os_memcpy(wpa_s->sme.mobility_domain, md, MOBILITY_DOMAIN_ID_LEN);
	wpa_hexdump(MSG_DEBUG, "SME: FT IEs", ies, ies_len);
	os_free(wpa_s->sme.ft_ies);
	wpa_s->sme.ft_ies = os_memdup(ies, ies_len);
	if (wpa_s->sme.ft_ies == NULL)
		return -1;
	wpa_s->sme.ft_ies_len = ies_len;
	return 0;
}

static struct wpabuf *cip_build_assoc_req(u8 cip_element_id_ext, u8 padding_delay)
{
	struct wpabuf *ie;

	ie = wpabuf_alloc(CIP_CAPAB_LEN);

	if (!ie)
		return NULL;
	wpabuf_put_u8(ie, WLAN_EID_EXTENSION);
	wpabuf_put_u8(ie, 1 + 1);
	wpabuf_put_u8(ie, cip_element_id_ext);
	wpabuf_put_u8(ie, padding_delay);

	return ie;
}

static void sme_deauth(struct wpa_supplicant *wpa_s, const u8 **link_bssids)
{
	int bssid_changed;
	u8 failed_bssid[ETH_ALEN];
	const u8 *bssid;

	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);

	if (wpa_s->valid_links)
		bssid = wpa_s->ap_mld_addr;
	else
		bssid = wpa_s->pending_bssid;

	if (wpa_drv_deauthenticate(wpa_s, bssid,
				   WLAN_REASON_DEAUTH_LEAVING) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "SME: Deauth request to the driver "
			"failed");
	}
	wpa_s->sme.prev_bssid_set = 0;

	/*
	 * Save the failed BSSID and clear association state before calling
	 * wpas_connection_failed(), since that function may synchronously
	 * retry association via wpa_supplicant_fast_associate(). If
	 * wpa_state/pending_bssid are still left at their pre-failure values
	 * during that retry, wpa_supplicant_connect()'s re-entrancy guard
	 * mistakes the retry for an already-in-progress connection to the
	 * same AP and silently drops it, leaving the STA disconnected until
	 * some later unrelated trigger (e.g. a new WPS_PBC) kicks it again.
	 */
	os_memcpy(failed_bssid, wpa_s->pending_bssid, ETH_ALEN);
	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);

#ifdef CONFIG_ENC_ASSOC
	/* Clear configured keys and PTKSA */
	if (wpa_s->ptksa &&
	    ptksa_cache_get(wpa_s->ptksa, wpa_s->bssid, WPA_CIPHER_NONE)) {
		wpa_clear_keys(wpa_s, wpa_s->bssid);
		ptksa_cache_flush(wpa_s->ptksa, wpa_s->bssid, WPA_CIPHER_NONE);
	}
#endif /* CONFIG_ENC_ASSOC */

	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);

	wpas_connection_failed(wpa_s, failed_bssid, link_bssids);
}


static void sme_assoc_comeback_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (!wpa_s->current_bss || !wpa_s->current_ssid) {
		wpa_msg(wpa_s, MSG_DEBUG,
			"SME: Comeback timeout expired; SSID/BSSID cleared; ignoring");
		return;
	}

	wpa_msg(wpa_s, MSG_DEBUG,
		"SME: Comeback timeout expired; retry associating with "
		MACSTR "; mode=%d auth_type=%u",
		MAC2STR(wpa_s->current_bss->bssid),
		wpa_s->current_ssid->mode,
		wpa_s->sme.assoc_auth_type);

	/* Authentication state was completed already; just try association
	 * again. */
	sme_associate(wpa_s, wpa_s->current_ssid->mode,
		      wpa_s->current_bss->bssid,
		      wpa_s->sme.assoc_auth_type);
}


static bool sme_try_assoc_comeback(struct wpa_supplicant *wpa_s,
				   union wpa_event_data *data)
{
	struct ieee802_11_elems elems;
	u32 timeout_interval;
	unsigned long comeback_usec;
	u8 type = WLAN_TIMEOUT_ASSOC_COMEBACK;

#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->test_assoc_comeback_type != -1)
		type = wpa_s->test_assoc_comeback_type;
#endif /* CONFIG_TESTING_OPTIONS */

	if (ieee802_11_parse_elems(data->assoc_reject.resp_ies,
				   data->assoc_reject.resp_ies_len,
				   &elems, 0) == ParseFailed) {
		wpa_msg(wpa_s, MSG_INFO,
			"SME: Temporary assoc reject: failed to parse (Re)Association Response frame elements");
		return false;
	}

	if (!elems.timeout_int) {
		wpa_msg(wpa_s, MSG_INFO,
			"SME: Temporary assoc reject: missing timeout interval IE");
		return false;
	}

	if (elems.timeout_int[0] != type) {
		wpa_msg(wpa_s, MSG_INFO,
			"SME: Temporary assoc reject: missing association comeback time");
		return false;
	}

	timeout_interval = WPA_GET_LE32(&elems.timeout_int[1]);
	if (timeout_interval > 60000) {
		/* This is unprotected information and there is no point in
		 * getting stuck waiting for very long duration based on it */
		wpa_msg(wpa_s, MSG_DEBUG,
			"SME: Ignore overly long association comeback interval: %u TUs",
			timeout_interval);
		return false;
	}
	wpa_msg(wpa_s, MSG_DEBUG, "SME: Association comeback interval: %u TUs",
		timeout_interval);

	comeback_usec = timeout_interval * 1024;
	eloop_register_timeout(comeback_usec / 1000000, comeback_usec % 1000000,
			       sme_assoc_comeback_timer, wpa_s, NULL);
	return true;
}


void sme_event_assoc_reject(struct wpa_supplicant *wpa_s,
			    union wpa_event_data *data,
			    const u8 **link_bssids)
{
	const u8 *bssid;

	if (wpa_s->valid_links)
		bssid = wpa_s->ap_mld_addr;
	else
		bssid = wpa_s->pending_bssid;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Association with " MACSTR " failed: "
		"status code %d", MAC2STR(wpa_s->pending_bssid),
		data->assoc_reject.status_code);

	eloop_cancel_timeout(sme_assoc_timer, wpa_s, NULL);
	eloop_cancel_timeout(sme_assoc_comeback_timer, wpa_s, NULL);

	/* Authentication phase has been completed at this point. Check whether
	 * the AP rejected association temporarily due to still holding a
	 * security associationis with us (MFP). If so, we must wait for the
	 * AP's association comeback timeout period before associating again. */
	if (data->assoc_reject.status_code ==
	    WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY) {
		wpa_msg(wpa_s, MSG_DEBUG,
			"SME: Temporary association reject from BSS " MACSTR,
			MAC2STR(bssid));
		if (sme_try_assoc_comeback(wpa_s, data)) {
			/* Break out early; comeback error is not a failure. */
			return;
		}
	}

#ifdef CONFIG_SAE
	if (wpa_s->sme.sae_pmksa_caching && wpa_s->current_ssid &&
	    wpa_key_mgmt_sae(wpa_s->current_ssid->key_mgmt)) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"PMKSA caching attempt rejected - drop PMKSA cache entry and fall back to SAE authentication");
		wpa_sm_aborted_cached(wpa_s->wpa);
		wpa_sm_pmksa_cache_flush(wpa_s->wpa, wpa_s->current_ssid);
		if (wpa_s->current_bss) {
			struct wpa_bss *bss = wpa_s->current_bss;
			struct wpa_ssid *ssid = wpa_s->current_ssid;

			wpa_drv_deauthenticate(wpa_s, bssid,
					       WLAN_REASON_DEAUTH_LEAVING);
			wpas_connect_work_done(wpa_s);
			wpa_supplicant_mark_disassoc(wpa_s);
			wpa_supplicant_connect(wpa_s, bss, ssid);
			return;
		}
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_DPP
	if (wpa_s->current_ssid &&
	    wpa_s->current_ssid->key_mgmt == WPA_KEY_MGMT_DPP &&
	    !data->assoc_reject.timed_out &&
	    data->assoc_reject.status_code == WLAN_STATUS_INVALID_PMKID) {
		struct rsn_pmksa_cache_entry *pmksa;

		pmksa = pmksa_cache_get_current(wpa_s->wpa);
		if (pmksa) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"DPP: Drop PMKSA cache entry for the BSS due to invalid PMKID report");
			wpa_sm_pmksa_cache_remove(wpa_s->wpa, pmksa);
		}
		wpa_sm_aborted_cached(wpa_s->wpa);
		if (wpa_s->current_bss) {
			struct wpa_bss *bss = wpa_s->current_bss;
			struct wpa_ssid *ssid = wpa_s->current_ssid;

			wpa_dbg(wpa_s, MSG_DEBUG,
				"DPP: Try network introduction again");
			wpas_connect_work_done(wpa_s);
			wpa_supplicant_mark_disassoc(wpa_s);
			wpa_supplicant_connect(wpa_s, bss, ssid);
			return;
		}
	}
#endif /* CONFIG_DPP */

	/*
	 * For now, unconditionally terminate the previous authentication. In
	 * theory, this should not be needed, but mac80211 gets quite confused
	 * if the authentication is left pending.. Some roaming cases might
	 * benefit from using the previous authentication, so this could be
	 * optimized in the future.
	 */
	sme_deauth(wpa_s, link_bssids);
}


static bool sme_other_mld_ap_candidate_available(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	size_t i;

	if (!ssid)
		return false;

	for (i = 0; i < wpa_s->last_scan_res_used; i++) {
		struct wpa_bss *bss = wpa_s->last_scan_res[i];

		if (is_zero_ether_addr(bss->mld_addr))
			continue;
		if (ether_addr_equal(bss->mld_addr, wpa_s->ap_mld_addr))
			continue;

		/* Only a candidate if it matches the network we are
		 * trying to connect to. */
		if (ssid->ssid_len == 0 ||
		    bss->ssid_len != ssid->ssid_len ||
		    os_memcmp(bss->ssid, ssid->ssid, ssid->ssid_len) != 0)
			continue;

		return true;
	}

	return false;
}


static void sme_collect_mld_link_bssids(struct wpa_supplicant *wpa_s,
					const u8 **link_bssids, int *n)
{
	int i;

	*n = 0;
	if (!wpa_s->valid_links)
		return;

	/*
	 * Only worth ignore-listing every link of this MLD AP if there is
	 * another MLD AP candidate to fall back to; otherwise this AP is
	 * the only option and cycling through its links keeps it eligible
	 * sooner.
	 */
	if (!sme_other_mld_ap_candidate_available(wpa_s))
		return;

	for_each_link(wpa_s->valid_links, i) {
		if (!ether_addr_equal(wpa_s->links[i].bssid,
				      wpa_s->pending_bssid))
			link_bssids[(*n)++] = wpa_s->links[i].bssid;
	}
	link_bssids[*n] = NULL;
}


void sme_event_auth_timed_out(struct wpa_supplicant *wpa_s,
			      union wpa_event_data *data)
{
	const u8 *link_bssids[MAX_NUM_MLD_LINKS + 1];
	int n;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Authentication timed out");
	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	sme_collect_mld_link_bssids(wpa_s, link_bssids, &n);
	wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
			       n ? link_bssids : NULL);
	wpa_supplicant_mark_disassoc(wpa_s);
}


void sme_event_assoc_timed_out(struct wpa_supplicant *wpa_s,
			       union wpa_event_data *data)
{
	const u8 *link_bssids[MAX_NUM_MLD_LINKS + 1];
	int n;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Association timed out");
	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	sme_collect_mld_link_bssids(wpa_s, link_bssids, &n);
	wpas_connection_failed(wpa_s, wpa_s->pending_bssid,
			       n ? link_bssids : NULL);
	wpa_supplicant_mark_disassoc(wpa_s);
}


void sme_event_disassoc(struct wpa_supplicant *wpa_s,
			struct disassoc_info *info)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Disassociation event received");
	if (wpa_s->sme.prev_bssid_set) {
		/*
		 * cfg80211/mac80211 can get into somewhat confused state if
		 * the AP only disassociates us and leaves us in authenticated
		 * state. For now, force the state to be cleared to avoid
		 * confusing errors if we try to associate with the AP again.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Deauthenticate to clear "
			"driver state");
		wpa_drv_deauthenticate(wpa_s, wpa_s->sme.prev_bssid,
				       WLAN_REASON_DEAUTH_LEAVING);
	}
}


static void sme_auth_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	if (wpa_s->wpa_state == WPA_AUTHENTICATING) {
		wpa_msg(wpa_s, MSG_DEBUG, "SME: Authentication timeout");
		sme_deauth(wpa_s, NULL);
	}
}


static void sme_assoc_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	if (wpa_s->wpa_state == WPA_ASSOCIATING) {
		wpa_msg(wpa_s, MSG_DEBUG, "SME: Association timeout");
		sme_deauth(wpa_s, NULL);
	}
}


void sme_state_changed(struct wpa_supplicant *wpa_s)
{
	/* Make sure timers are cleaned up appropriately. */
	if (wpa_s->wpa_state != WPA_ASSOCIATING) {
		eloop_cancel_timeout(sme_assoc_timer, wpa_s, NULL);
		eloop_cancel_timeout(sme_assoc_comeback_timer, wpa_s, NULL);
	}
	if (wpa_s->wpa_state != WPA_AUTHENTICATING)
		eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);
}


void sme_clear_on_disassoc(struct wpa_supplicant *wpa_s)
{
	wpa_s->sme.prev_bssid_set = 0;
#ifdef CONFIG_SAE
	wpabuf_free(wpa_s->sme.sae_token);
	wpa_s->sme.sae_token = NULL;
	sae_clear_data(&wpa_s->sme.sae);
#endif /* CONFIG_SAE */
#ifdef CONFIG_IEEE80211R
	if (wpa_s->sme.ft_ies || wpa_s->sme.ft_used)
		sme_update_ft_ies(wpa_s, NULL, NULL, 0);
#endif /* CONFIG_IEEE80211R */
	sme_stop_sa_query(wpa_s);
}


void sme_deinit(struct wpa_supplicant *wpa_s)
{
	sme_clear_on_disassoc(wpa_s);
#ifdef CONFIG_SAE
	os_free(wpa_s->sme.sae_rejected_groups);
	wpa_s->sme.sae_rejected_groups = NULL;
#endif /* CONFIG_SAE */

	eloop_cancel_timeout(sme_assoc_timer, wpa_s, NULL);
	eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);
	eloop_cancel_timeout(sme_obss_scan_timeout, wpa_s, NULL);
	eloop_cancel_timeout(sme_assoc_comeback_timer, wpa_s, NULL);
#ifdef CONFIG_QCN_EXTN
	eloop_cancel_timeout(sme_pre_connect_timer_extn, wpa_s, NULL);
	wpa_s->pre_connect_cnt = 0;
	wpa_s->cache_cwork = NULL;
#endif
}


static void sme_send_2040_bss_coex(struct wpa_supplicant *wpa_s,
				   const u8 *chan_list, u8 num_channels,
				   u8 num_intol)
{
	struct ieee80211_2040_bss_coex_ie *bc_ie;
	struct ieee80211_2040_intol_chan_report *ic_report;
	struct wpabuf *buf;

	wpa_printf(MSG_DEBUG, "SME: Send 20/40 BSS Coexistence to " MACSTR
		   " (num_channels=%u num_intol=%u)",
		   MAC2STR(wpa_s->bssid), num_channels, num_intol);
	wpa_hexdump(MSG_DEBUG, "SME: 20/40 BSS Intolerant Channels",
		    chan_list, num_channels);

	buf = wpabuf_alloc(2 + /* action.category + action_code */
			   sizeof(struct ieee80211_2040_bss_coex_ie) +
			   sizeof(struct ieee80211_2040_intol_chan_report) +
			   num_channels);
	if (buf == NULL)
		return;

	wpabuf_put_u8(buf, WLAN_ACTION_PUBLIC);
	wpabuf_put_u8(buf, WLAN_PA_20_40_BSS_COEX);

	bc_ie = wpabuf_put(buf, sizeof(*bc_ie));
	bc_ie->element_id = WLAN_EID_20_40_BSS_COEXISTENCE;
	bc_ie->length = 1;
	if (num_intol)
		bc_ie->coex_param |= WLAN_20_40_BSS_COEX_20MHZ_WIDTH_REQ;

	if (num_channels > 0) {
		ic_report = wpabuf_put(buf, sizeof(*ic_report));
		ic_report->element_id = WLAN_EID_20_40_BSS_INTOLERANT;
		ic_report->length = num_channels + 1;
		ic_report->op_class = 0;
		os_memcpy(wpabuf_put(buf, num_channels), chan_list,
			  num_channels);
	}

	if (wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				wpa_s->own_addr, wpa_s->bssid,
				wpabuf_head(buf), wpabuf_len(buf), 0) < 0) {
		wpa_msg(wpa_s, MSG_INFO,
			"SME: Failed to send 20/40 BSS Coexistence frame");
	}

	wpabuf_free(buf);
}


int sme_proc_obss_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;
	const u8 *ie;
	u16 ht_cap;
	u8 chan_list[P2P_MAX_CHANNELS], channel;
	u8 num_channels = 0, num_intol = 0, i;

	if (!wpa_s->sme.sched_obss_scan)
		return 0;

	wpa_s->sme.sched_obss_scan = 0;
	if (!wpa_s->current_bss || wpa_s->wpa_state != WPA_COMPLETED)
		return 1;

	/*
	 * Check whether AP uses regulatory triplet or channel triplet in
	 * country info. Right now the operating class of the BSS channel
	 * width trigger event is "unknown" (IEEE Std 802.11-2012 10.15.12),
	 * based on the assumption that operating class triplet is not used in
	 * beacon frame. If the First Channel Number/Operating Extension
	 * Identifier octet has a positive integer value of 201 or greater,
	 * then its operating class triplet.
	 *
	 * TODO: If Supported Operating Classes element is present in beacon
	 * frame, have to lookup operating class in Annex E and fill them in
	 * 2040 coex frame.
	 */
	ie = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_COUNTRY);
	if (ie && (ie[1] >= 6) && (ie[5] >= 201))
		return 1;

	os_memset(chan_list, 0, sizeof(chan_list));

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		/* Skip other band bss */
		enum hostapd_hw_mode mode;
		mode = ieee80211_freq_to_chan(bss->freq, &channel);
		if (mode != HOSTAPD_MODE_IEEE80211G &&
		    mode != HOSTAPD_MODE_IEEE80211B)
			continue;

		ie = wpa_bss_get_ie(bss, WLAN_EID_HT_CAP);
		ht_cap = (ie && (ie[1] == 26)) ? WPA_GET_LE16(ie + 2) : 0;
		wpa_printf(MSG_DEBUG, "SME OBSS scan BSS " MACSTR
			   " freq=%u chan=%u ht_cap=0x%x",
			   MAC2STR(bss->bssid), bss->freq, channel, ht_cap);

		if (!ht_cap || (ht_cap & HT_CAP_INFO_40MHZ_INTOLERANT)) {
			if (ht_cap & HT_CAP_INFO_40MHZ_INTOLERANT)
				num_intol++;

			/* Check whether the channel is already considered */
			for (i = 0; i < num_channels; i++) {
				if (channel == chan_list[i])
					break;
			}
			if (i != num_channels)
				continue;

			chan_list[num_channels++] = channel;
		}
	}

	sme_send_2040_bss_coex(wpa_s, chan_list, num_channels, num_intol);
	return 1;
}


static void wpa_obss_scan_freqs_list(struct wpa_supplicant *wpa_s,
				     struct wpa_driver_scan_params *params)
{
	/* Include only affected channels */
	struct hostapd_hw_modes *mode;
	int count, i;
	int start, end;

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes,
			HOSTAPD_MODE_IEEE80211G, false);
	if (mode == NULL) {
		/* No channels supported in this band - use empty list */
		params->freqs = os_zalloc(sizeof(int));
		return;
	}

	if (wpa_s->sme.ht_sec_chan == HT_SEC_CHAN_UNKNOWN &&
	    wpa_s->current_bss) {
		const u8 *ie;

		ie = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_HT_OPERATION);
		if (ie && ie[1] >= 2) {
			u8 o;

			o = ie[3] & HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK;
			if (o == HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE)
				wpa_s->sme.ht_sec_chan = HT_SEC_CHAN_ABOVE;
			else if (o == HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW)
				wpa_s->sme.ht_sec_chan = HT_SEC_CHAN_BELOW;
		}
	}

	start = wpa_s->assoc_freq - 10;
	end = wpa_s->assoc_freq + 10;
	switch (wpa_s->sme.ht_sec_chan) {
	case HT_SEC_CHAN_UNKNOWN:
		/* HT40+ possible on channels 1..9 */
		if (wpa_s->assoc_freq <= 2452)
			start -= 20;
		/* HT40- possible on channels 5-13 */
		if (wpa_s->assoc_freq >= 2432)
			end += 20;
		break;
	case HT_SEC_CHAN_ABOVE:
		end += 20;
		break;
	case HT_SEC_CHAN_BELOW:
		start -= 20;
		break;
	}
	wpa_printf(MSG_DEBUG,
		   "OBSS: assoc_freq %d possible affected range %d-%d",
		   wpa_s->assoc_freq, start, end);

	params->freqs = os_calloc(mode->num_channels + 1, sizeof(int));
	if (params->freqs == NULL)
		return;
	for (count = 0, i = 0; i < mode->num_channels; i++) {
		int freq;

		if (mode->channels[i].flag & HOSTAPD_CHAN_DISABLED)
			continue;
		freq = mode->channels[i].freq;
		if (freq - 10 >= end || freq + 10 <= start)
			continue; /* not affected */
		params->freqs[count++] = freq;
	}
}


static void sme_obss_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_driver_scan_params params;

	if (!wpa_s->current_bss) {
		wpa_printf(MSG_DEBUG, "SME OBSS: Ignore scan request");
		return;
	}

	os_memset(&params, 0, sizeof(params));
	wpa_obss_scan_freqs_list(wpa_s, &params);
	params.low_priority = 1;
	wpa_printf(MSG_DEBUG, "SME OBSS: Request an OBSS scan");

	if (wpa_supplicant_trigger_scan(wpa_s, &params, true, false))
		wpa_printf(MSG_DEBUG, "SME OBSS: Failed to trigger scan");
	else
		wpa_s->sme.sched_obss_scan = 1;
	os_free(params.freqs);

	eloop_register_timeout(wpa_s->sme.obss_scan_int, 0,
			       sme_obss_scan_timeout, wpa_s, NULL);
}


void sme_sched_obss_scan(struct wpa_supplicant *wpa_s, int enable)
{
	const u8 *ie;
	struct wpa_bss *bss = wpa_s->current_bss;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	struct hostapd_hw_modes *hw_mode = NULL;
	int i;

	eloop_cancel_timeout(sme_obss_scan_timeout, wpa_s, NULL);
	wpa_s->sme.sched_obss_scan = 0;
	wpa_s->sme.ht_sec_chan = HT_SEC_CHAN_UNKNOWN;
	if (!enable)
		return;

	/*
	 * Schedule OBSS scan if driver is using station SME in wpa_supplicant
	 * or it expects OBSS scan to be performed by wpa_supplicant.
	 */
	if (!((wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME) ||
	      (wpa_s->drv_flags & WPA_DRIVER_FLAGS_OBSS_SCAN)) ||
	    ssid == NULL || ssid->mode != WPAS_MODE_INFRA)
		return;

#ifdef CONFIG_HT_OVERRIDES
	/* No need for OBSS scan if HT40 is explicitly disabled */
	if (ssid->disable_ht40)
		return;
#endif /* CONFIG_HT_OVERRIDES */

	if (!wpa_s->hw.modes)
		return;

	/* only HT caps in 11g mode are relevant */
	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		hw_mode = &wpa_s->hw.modes[i];
		if (hw_mode->mode == HOSTAPD_MODE_IEEE80211G)
			break;
	}

	/* Driver does not support HT40 for 11g or doesn't have 11g. */
	if (i == wpa_s->hw.num_modes || !hw_mode ||
	    !(hw_mode->ht_capab & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
		return;

	if (bss == NULL || bss->freq < 2400 || bss->freq > 2500)
		return; /* Not associated on 2.4 GHz band */

	/* Check whether AP supports HT40 */
	ie = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_HT_CAP);
	if (!ie || ie[1] < 2 ||
	    !(WPA_GET_LE16(ie + 2) & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
		return; /* AP does not support HT40 */

	ie = wpa_bss_get_ie(wpa_s->current_bss,
			    WLAN_EID_OVERLAPPING_BSS_SCAN_PARAMS);
	if (!ie || ie[1] < 14)
		return; /* AP does not request OBSS scans */

	wpa_s->sme.obss_scan_int = WPA_GET_LE16(ie + 6);
	if (wpa_s->sme.obss_scan_int < 10) {
		wpa_printf(MSG_DEBUG, "SME: Invalid OBSS Scan Interval %u "
			   "replaced with the minimum 10 sec",
			   wpa_s->sme.obss_scan_int);
		wpa_s->sme.obss_scan_int = 10;
	}
	wpa_printf(MSG_DEBUG, "SME: OBSS Scan Interval %u sec",
		   wpa_s->sme.obss_scan_int);
	eloop_register_timeout(wpa_s->sme.obss_scan_int, 0,
			       sme_obss_scan_timeout, wpa_s, NULL);
}


static const unsigned int sa_query_max_timeout = 1000;
static const unsigned int sa_query_retry_timeout = 201;
static const unsigned int sa_query_ch_switch_max_delay = 5000; /* in usec */

static int sme_check_sa_query_timeout(struct wpa_supplicant *wpa_s)
{
	u32 tu;
	struct os_reltime now, passed;
	os_get_reltime(&now);
	os_reltime_sub(&now, &wpa_s->sme.sa_query_start, &passed);
	tu = (passed.sec * 1000000 + passed.usec) / 1024;
	if (sa_query_max_timeout < tu) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: SA Query timed out");
		sme_stop_sa_query(wpa_s);
		wpa_supplicant_deauthenticate(
			wpa_s, WLAN_REASON_PREV_AUTH_NOT_VALID);
		return 1;
	}

	return 0;
}


static void sme_send_sa_query_req(struct wpa_supplicant *wpa_s,
				  const u8 *trans_id)
{
	u8 req[2 + WLAN_SA_QUERY_TR_ID_LEN + OCV_OCI_EXTENDED_LEN];
	u8 req_len = 2 + WLAN_SA_QUERY_TR_ID_LEN;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Sending SA Query Request to "
		MACSTR, MAC2STR(wpa_s->bssid));
	wpa_hexdump(MSG_DEBUG, "SME: SA Query Transaction ID",
		    trans_id, WLAN_SA_QUERY_TR_ID_LEN);
	req[0] = WLAN_ACTION_SA_QUERY;
	req[1] = WLAN_SA_QUERY_REQUEST;
	os_memcpy(req + 2, trans_id, WLAN_SA_QUERY_TR_ID_LEN);

#ifdef CONFIG_OCV
	if (wpa_sm_ocv_enabled(wpa_s->wpa)) {
		struct wpa_channel_info ci;

		if (wpa_drv_channel_info(wpa_s, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info for OCI element in SA Query Request frame");
			return;
		}

#ifdef CONFIG_TESTING_OPTIONS
		if (wpa_s->oci_freq_override_saquery_req) {
			wpa_printf(MSG_INFO,
				   "TEST: Override SA Query Request OCI frequency %d -> %d MHz",
				   ci.frequency,
				   wpa_s->oci_freq_override_saquery_req);
			ci.frequency = wpa_s->oci_freq_override_saquery_req;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		if (ocv_insert_extended_oci(&ci, req + req_len) < 0)
			return;

		req_len += OCV_OCI_EXTENDED_LEN;
	}
#endif /* CONFIG_OCV */

	if (wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				wpa_s->own_addr, wpa_s->bssid,
				req, req_len, 0) < 0)
		wpa_msg(wpa_s, MSG_INFO, "SME: Failed to send SA Query "
			"Request");
}


static void sme_sa_query_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	unsigned int timeout, sec, usec;
	u8 *trans_id, *nbuf;

	if (wpa_s->sme.sa_query_count > 0 &&
	    sme_check_sa_query_timeout(wpa_s))
		return;

	nbuf = os_realloc_array(wpa_s->sme.sa_query_trans_id,
				wpa_s->sme.sa_query_count + 1,
				WLAN_SA_QUERY_TR_ID_LEN);
	if (nbuf == NULL) {
		sme_stop_sa_query(wpa_s);
		return;
	}
	if (wpa_s->sme.sa_query_count == 0) {
		/* Starting a new SA Query procedure */
		os_get_reltime(&wpa_s->sme.sa_query_start);
	}
	trans_id = nbuf + wpa_s->sme.sa_query_count * WLAN_SA_QUERY_TR_ID_LEN;
	wpa_s->sme.sa_query_trans_id = nbuf;
	wpa_s->sme.sa_query_count++;

	if (os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0) {
		wpa_printf(MSG_DEBUG, "Could not generate SA Query ID");
		sme_stop_sa_query(wpa_s);
		return;
	}

	timeout = sa_query_retry_timeout;
	sec = ((timeout / 1000) * 1024) / 1000;
	usec = (timeout % 1000) * 1024;
	eloop_register_timeout(sec, usec, sme_sa_query_timer, wpa_s, NULL);

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Association SA Query attempt %d",
		wpa_s->sme.sa_query_count);

	sme_send_sa_query_req(wpa_s, trans_id);
}


static void sme_start_sa_query(struct wpa_supplicant *wpa_s)
{
	sme_sa_query_timer(wpa_s, NULL);
}


static void sme_stop_sa_query(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->sme.sa_query_trans_id)
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Stop SA Query");
	eloop_cancel_timeout(sme_sa_query_timer, wpa_s, NULL);
	os_free(wpa_s->sme.sa_query_trans_id);
	wpa_s->sme.sa_query_trans_id = NULL;
	wpa_s->sme.sa_query_count = 0;
}


void sme_event_unprot_disconnect(struct wpa_supplicant *wpa_s, const u8 *sa,
				 const u8 *da, u16 reason_code)
{
	struct wpa_ssid *ssid;
	struct os_reltime now;

	if (wpa_s->wpa_state != WPA_COMPLETED)
		return;
	ssid = wpa_s->current_ssid;
	if (wpas_get_ssid_pmf(wpa_s, ssid) == NO_MGMT_FRAME_PROTECTION)
		return;
	if (!ether_addr_equal(sa, wpa_s->bssid))
		return;
	if (reason_code != WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA &&
	    reason_code != WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA)
		return;
	if (wpa_s->sme.sa_query_count > 0)
		return;
#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->disable_sa_query)
		return;
#endif /* CONFIG_TESTING_OPTIONS */

	os_get_reltime(&now);
	if (wpa_s->sme.last_unprot_disconnect.sec &&
	    !os_reltime_expired(&now, &wpa_s->sme.last_unprot_disconnect, 10))
		return; /* limit SA Query procedure frequency */
	wpa_s->sme.last_unprot_disconnect = now;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Unprotected disconnect dropped - "
		"possible AP/STA state mismatch - trigger SA Query");
	sme_start_sa_query(wpa_s);
}


void sme_event_ch_switch(struct wpa_supplicant *wpa_s)
{
	unsigned int usec;
	u32 _rand;

	if (wpa_s->wpa_state != WPA_COMPLETED ||
	    !wpa_sm_ocv_enabled(wpa_s->wpa))
		return;

	wpa_dbg(wpa_s, MSG_DEBUG,
		"SME: Channel switch completed - trigger new SA Query to verify new operating channel");
	sme_stop_sa_query(wpa_s);

	if (os_get_random((u8 *) &_rand, sizeof(_rand)) < 0)
		_rand = os_random();
	usec = _rand % (sa_query_ch_switch_max_delay + 1);
	eloop_register_timeout(0, usec, sme_sa_query_timer, wpa_s, NULL);
}


static void sme_process_sa_query_request(struct wpa_supplicant *wpa_s,
					 const u8 *sa, const u8 *data,
					 size_t len, int freq)
{
	u8 resp[2 + WLAN_SA_QUERY_TR_ID_LEN + OCV_OCI_EXTENDED_LEN];
	u8 resp_len = 2 + WLAN_SA_QUERY_TR_ID_LEN;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Sending SA Query Response to "
		MACSTR, MAC2STR(wpa_s->bssid));

	resp[0] = WLAN_ACTION_SA_QUERY;
	resp[1] = WLAN_SA_QUERY_RESPONSE;
	os_memcpy(resp + 2, data + 1, WLAN_SA_QUERY_TR_ID_LEN);

#ifdef CONFIG_OCV
	if (wpa_sm_ocv_enabled(wpa_s->wpa)) {
		struct wpa_channel_info ci;

		if (wpa_drv_channel_info(wpa_s, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info for OCI element in SA Query Response frame");
			return;
		}

#ifdef CONFIG_TESTING_OPTIONS
		if (wpa_s->oci_freq_override_saquery_resp) {
			wpa_printf(MSG_INFO,
				   "TEST: Override SA Query Response OCI frequency %d -> %d MHz",
				   ci.frequency,
				   wpa_s->oci_freq_override_saquery_resp);
			ci.frequency = wpa_s->oci_freq_override_saquery_resp;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		if (ocv_insert_extended_oci(&ci, resp + resp_len) < 0)
			return;

		resp_len += OCV_OCI_EXTENDED_LEN;
	}
#endif /* CONFIG_OCV */

	if (wpa_drv_send_action(wpa_s, freq, 0, wpa_s->bssid,
				wpa_s->own_addr, wpa_s->bssid,
				resp, resp_len, 0) < 0)
		wpa_msg(wpa_s, MSG_INFO,
			"SME: Failed to send SA Query Response");
}


static void sme_process_sa_query_response(struct wpa_supplicant *wpa_s,
					  const u8 *sa, const u8 *data,
					  size_t len)
{
	int i;

	if (!wpa_s->sme.sa_query_trans_id)
		return;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Received SA Query response from "
		MACSTR " (trans_id %02x%02x)", MAC2STR(sa), data[1], data[2]);

	if (!ether_addr_equal(sa, wpa_s->bssid))
		return;

	for (i = 0; i < wpa_s->sme.sa_query_count; i++) {
		if (os_memcmp(wpa_s->sme.sa_query_trans_id +
			      i * WLAN_SA_QUERY_TR_ID_LEN,
			      data + 1, WLAN_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= wpa_s->sme.sa_query_count) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: No matching SA Query "
			"transaction identifier found");
		return;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Reply to pending SA Query received "
		"from " MACSTR, MAC2STR(sa));
	sme_stop_sa_query(wpa_s);
}


void sme_sa_query_rx(struct wpa_supplicant *wpa_s, const u8 *da, const u8 *sa,
		     const u8 *data, size_t len, int freq)
{
	if (len < 1 + WLAN_SA_QUERY_TR_ID_LEN)
		return;
	if (is_multicast_ether_addr(da)) {
		wpa_printf(MSG_DEBUG,
			   "IEEE 802.11: Ignore group-addressed SA Query frame (A1=" MACSTR " A2=" MACSTR ")",
			   MAC2STR(da), MAC2STR(sa));
		return;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Received SA Query frame from "
		MACSTR " (trans_id %02x%02x)", MAC2STR(sa), data[1], data[2]);

#ifdef CONFIG_OCV
	if (wpa_sm_ocv_enabled(wpa_s->wpa)) {
		struct ieee802_11_elems elems;
		struct wpa_channel_info ci;

		if (ieee802_11_parse_elems(data + 1 + WLAN_SA_QUERY_TR_ID_LEN,
					   len - 1 - WLAN_SA_QUERY_TR_ID_LEN,
					   &elems, 1) == ParseFailed) {
			wpa_printf(MSG_DEBUG,
				   "SA Query: Failed to parse elements");
			return;
		}

		if (wpa_drv_channel_info(wpa_s, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info to validate received OCI in SA Query Action frame");
			return;
		}

		if (ocv_verify_tx_params(elems.oci, elems.oci_len, &ci,
					 channel_width_to_int(ci.chanwidth),
					 ci.seg1_idx) != OCI_SUCCESS) {
			wpa_msg(wpa_s, MSG_INFO, OCV_FAILURE "addr=" MACSTR
				" frame=saquery%s error=%s",
				MAC2STR(sa), data[0] == WLAN_SA_QUERY_REQUEST ?
				"req" : "resp", ocv_errorstr);
			return;
		}
	}
#endif /* CONFIG_OCV */

	if (data[0] == WLAN_SA_QUERY_REQUEST)
		sme_process_sa_query_request(wpa_s, sa, data, len, freq);
	else if (data[0] == WLAN_SA_QUERY_RESPONSE)
		sme_process_sa_query_response(wpa_s, sa, data, len);
}
