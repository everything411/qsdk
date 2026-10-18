/*
 * hostapd - Driver operations
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/hw_features_common.h"
#include "wps/wps.h"
#include "p2p/p2p.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "ap_config.h"
#include "p2p_hostapd.h"
#include "hs20.h"
#include "wpa_auth.h"
#include "hw_features.h"
#include "ap_drv_ops.h"
#ifdef CONFIG_QCN_EXTN
#include "../../qcn_extns/cmn.h"
#endif /* CONFIG_QCN_EXTN */
#include "hostapd_if/hostapd_if.h"

#ifdef CONFIG_IEEE80211BE
#include "common/qca-vendor.h"
#endif
#include "dscp_policy.h"

#ifdef CONFIG_IEEE80211AX
#include "robust_av.h"
#endif

u32 hostapd_sta_flags_to_drv(u32 flags, u32 flags_ext)
{
	int res = 0;
	if (flags & WLAN_STA_AUTHORIZED)
		res |= WPA_STA_AUTHORIZED;
	if (flags & WLAN_STA_WMM)
		res |= WPA_STA_WMM;
	if (flags & WLAN_STA_SHORT_PREAMBLE)
		res |= WPA_STA_SHORT_PREAMBLE;
	if (flags & WLAN_STA_MFP)
		res |= WPA_STA_MFP;
	if (flags & WLAN_STA_AUTH)
		res |= WPA_STA_AUTHENTICATED;
	if (flags & WLAN_STA_ASSOC)
		res |= WPA_STA_ASSOCIATED;
	if (flags & WLAN_STA_SPP_AMSDU)
		res |= WPA_STA_SPP_AMSDU;
	if (flags & WLAN_STA_FT_AUTH)
		res |= WPA_STA_FT_AUTH;
	if (flags_ext & WLAN_STA_SMD)
		res |= WPA_STA_SMD;
	if (flags & WLAN_STA_CFP)
		res |= WPA_STA_CFP;

	return res;
}


static int add_buf(struct wpabuf **dst, const struct wpabuf *src)
{
	if (!src)
		return 0;
	if (wpabuf_resize(dst, wpabuf_len(src)) != 0)
		return -1;
	wpabuf_put_buf(*dst, src);
	return 0;
}


static int add_buf_data(struct wpabuf **dst, const u8 *data, size_t len)
{
	if (!data || !len)
		return 0;
	if (wpabuf_resize(dst, len) != 0)
		return -1;
	wpabuf_put_data(*dst, data, len);
	return 0;
}


int hostapd_build_ap_extra_ies(struct hostapd_data *hapd,
			       struct wpabuf **beacon_ret,
			       struct wpabuf **proberesp_ret,
			       struct wpabuf **assocresp_ret)
{
	struct wpabuf *beacon = NULL, *proberesp = NULL, *assocresp = NULL;
	u8 buf[216], *pos;
	size_t i;

	*beacon_ret = *proberesp_ret = *assocresp_ret = NULL;

#ifdef NEED_AP_MLME
	pos = buf;
	pos = hostapd_eid_rm_enabled_capab(hapd, pos, sizeof(buf));
	if (add_buf_data(&assocresp, buf, pos - buf) < 0 ||
	    add_buf_data(&proberesp, buf, pos - buf) < 0)
		goto fail;
#endif /* NEED_AP_MLME */

	pos = buf;
	pos = hostapd_eid_time_adv(hapd, pos);
	if (add_buf_data(&beacon, buf, pos - buf) < 0)
		goto fail;
	pos = hostapd_eid_time_zone(hapd, pos);
	if (add_buf_data(&proberesp, buf, pos - buf) < 0)
		goto fail;

	pos = buf;
	pos = hostapd_eid_ext_capab(hapd, pos, false);
	if (add_buf_data(&assocresp, buf, pos - buf) < 0)
		goto fail;
	pos = hostapd_eid_interworking(hapd, pos);
	pos = hostapd_eid_adv_proto(hapd, pos);
	pos = hostapd_eid_roaming_consortium(hapd, pos);
	if (add_buf_data(&beacon, buf, pos - buf) < 0 ||
	    add_buf_data(&proberesp, buf, pos - buf) < 0)
		goto fail;

#ifdef CONFIG_FST
	if (add_buf(&beacon, hapd->iface->fst_ies) < 0 ||
	    add_buf(&proberesp, hapd->iface->fst_ies) < 0 ||
	    add_buf(&assocresp, hapd->iface->fst_ies) < 0)
		goto fail;
#endif /* CONFIG_FST */

#ifdef CONFIG_FILS
	pos = hostapd_eid_fils_indic(hapd, buf, 0);
	if (add_buf_data(&beacon, buf, pos - buf) < 0 ||
	    add_buf_data(&proberesp, buf, pos - buf) < 0)
		goto fail;
#endif /* CONFIG_FILS */

	pos = hostapd_add_wfa_cap_ie(hapd, NULL, buf);
	if (pos != buf) {
		if (add_buf_data(&beacon, buf, pos - buf) < 0 ||
		    add_buf_data(&proberesp, buf, pos - buf) < 0 ||
		    add_buf_data(&assocresp, buf, pos - buf) < 0)
			goto fail;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (!hapd->conf->rsn_override_omit_rsnxe) {
		pos = hostapd_eid_rsnxe(hapd, buf, sizeof(buf),
					hapd->conf->rsnxe_capab_mask);
		if (add_buf_data(&assocresp, buf, pos - buf) < 0)
			goto fail;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (add_buf(&beacon, hapd->wps_beacon_ie) < 0 ||
	    add_buf(&proberesp, hapd->wps_probe_resp_ie) < 0)
		goto fail;

#ifdef CONFIG_P2P
	if (add_buf(&beacon, hapd->p2p_beacon_ie) < 0 ||
	    add_buf(&proberesp, hapd->p2p_probe_resp_ie) < 0)
		goto fail;
#endif /* CONFIG_P2P */

#ifdef CONFIG_P2P_MANAGER
	if (hapd->conf->p2p & P2P_MANAGE) {
		if (wpabuf_resize(&beacon, 100) == 0) {
			u8 *start, *p;
			start = wpabuf_put(beacon, 0);
			p = hostapd_eid_p2p_manage(hapd, start);
			wpabuf_put(beacon, p - start);
		}

		if (wpabuf_resize(&proberesp, 100) == 0) {
			u8 *start, *p;
			start = wpabuf_put(proberesp, 0);
			p = hostapd_eid_p2p_manage(hapd, start);
			wpabuf_put(proberesp, p - start);
		}
	}
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_WPS
	if (hapd->conf->wps_state) {
		struct wpabuf *a = wps_build_assoc_resp_ie();
		if (add_buf(&assocresp, a) < 0) {
			wpabuf_free(a);
			goto fail;
		}
		wpabuf_free(a);
	}
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P_MANAGER
	if (hapd->conf->p2p & P2P_MANAGE) {
		if (wpabuf_resize(&assocresp, 100) == 0) {
			u8 *start, *p;
			start = wpabuf_put(assocresp, 0);
			p = hostapd_eid_p2p_manage(hapd, start);
			wpabuf_put(assocresp, p - start);
		}
	}
#endif /* CONFIG_P2P_MANAGER */

#ifdef CONFIG_WIFI_DISPLAY
	if (hapd->p2p_group) {
		struct wpabuf *a;
		a = p2p_group_assoc_resp_ie(hapd->p2p_group, P2P_SC_SUCCESS);
		if (add_buf(&assocresp, a) < 0) {
			wpabuf_free(a);
			goto fail;
		}
		wpabuf_free(a);
	}
#endif /* CONFIG_WIFI_DISPLAY */

#ifdef CONFIG_HS20
	pos = hostapd_eid_hs20_indication(hapd, buf);
	if (add_buf_data(&beacon, buf, pos - buf) < 0 ||
	    add_buf_data(&proberesp, buf, pos - buf) < 0)
		goto fail;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_MBO
	if (hapd->conf->mbo_enabled ||
	    OCE_STA_CFON_ENABLED(hapd) || OCE_AP_ENABLED(hapd)) {
		pos = hostapd_eid_mbo(hapd, buf, sizeof(buf));
		if (add_buf_data(&beacon, buf, pos - buf) < 0 ||
		    add_buf_data(&proberesp, buf, pos - buf) < 0 ||
		    add_buf_data(&assocresp, buf, pos - buf) < 0)
			goto fail;
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_OWE
	pos = hostapd_eid_owe_trans(hapd, buf, sizeof(buf));
	if (add_buf_data(&beacon, buf, pos - buf) < 0 ||
	    add_buf_data(&proberesp, buf, pos - buf) < 0)
		goto fail;
#endif /* CONFIG_OWE */

	/* Use plugin vendor elements if set, otherwise use conf vendor elements */
	if (hapd->plugin_vendor_elements) {
		if (add_buf(&beacon, hapd->plugin_vendor_elements) < 0 ||
		    add_buf(&proberesp, hapd->plugin_vendor_elements) < 0)
			goto fail;
	} else {
		for (i = 0; i < hapd->conf->vendor_elements_count; i++) {
			if (add_buf(&beacon, hapd->conf->vendor_elements[i]) < 0 ||
			    add_buf(&proberesp, hapd->conf->vendor_elements[i]) < 0)
				goto fail;
		}
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (add_buf(&proberesp, hapd->conf->presp_elements) < 0)
		goto fail;
#endif /* CONFIG_TESTING_OPTIONS */
	if (add_buf(&assocresp, hapd->conf->assocresp_elements) < 0)
		goto fail;

	*beacon_ret = beacon;
	*proberesp_ret = proberesp;
	*assocresp_ret = assocresp;

	return 0;

fail:
	wpabuf_free(beacon);
	wpabuf_free(proberesp);
	wpabuf_free(assocresp);
	return -1;
}


void hostapd_free_ap_extra_ies(struct hostapd_data *hapd,
			       struct wpabuf *beacon,
			       struct wpabuf *proberesp,
			       struct wpabuf *assocresp)
{
	wpabuf_free(beacon);
	wpabuf_free(proberesp);
	wpabuf_free(assocresp);
}


int hostapd_reset_ap_wps_ie(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->set_ap_wps_ie == NULL)
		return 0;

	return hapd->driver->set_ap_wps_ie(hapd->drv_priv, NULL, NULL, NULL);
}


int hostapd_set_ap_wps_ie(struct hostapd_data *hapd)
{
	struct wpabuf *beacon, *proberesp, *assocresp;
	int ret;

	if (hapd->driver == NULL || hapd->driver->set_ap_wps_ie == NULL)
		return 0;

	if (hostapd_build_ap_extra_ies(hapd, &beacon, &proberesp, &assocresp) <
	    0)
		return -1;

	ret = hapd->driver->set_ap_wps_ie(hapd->drv_priv, beacon, proberesp,
					  assocresp);

	hostapd_free_ap_extra_ies(hapd, beacon, proberesp, assocresp);

	return ret;
}


bool hostapd_sta_is_link_sta(struct hostapd_data *hapd,
			     struct sta_info *sta)
{
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    sta->mld_assoc_link_id != hapd->mld_link_id)
		return true;
#endif /* CONFIG_IEEE80211BE */

	return false;
}

static
int __hostapd_set_authorized(struct hostapd_data *hapd,
			     struct sta_info *sta, int authorized);
int hostapd_set_authorized(struct hostapd_data *hapd,
			   struct sta_info *sta, int authorized)
{
	int ret;
	/*
	 * The WPA_STA_AUTHORIZED flag is relevant only for the MLD station and
	 * not to the link stations (as the authorization is done between the
	 * MLD peers). Thus, do not propagate the change to the driver for the
	 * link stations.
	 */
	if (hostapd_sta_is_link_sta(hapd, sta)) {
		wpa_printf(MSG_DEBUG,
			   "%s: Do not update link station flags (" MACSTR ")",
			   __func__, MAC2STR(sta->addr));
		return 0;
	}

	ret = __hostapd_set_authorized(hapd, sta, authorized);
#ifdef CONFIG_HOSTAPD_IF
	if (!ret)
		hostapd_if_event_authorize_completion(hapd, sta->addr,
						      authorized);
#endif

	return ret;
}

static
int __hostapd_set_authorized(struct hostapd_data *hapd,
			     struct sta_info *sta, int authorized)
{

	if (authorized) {
		return hostapd_sta_set_flags(hapd, sta->addr,
					     hostapd_sta_flags_to_drv(
						     sta->flags, sta->flags_ext),
					     WPA_STA_AUTHORIZED, ~0);
	}

	return hostapd_sta_set_flags(hapd, sta->addr,
				     hostapd_sta_flags_to_drv(sta->flags, sta->flags_ext),
				     0, ~WPA_STA_AUTHORIZED);
}


int hostapd_set_sta_flags(struct hostapd_data *hapd, struct sta_info *sta)
{
	int set_flags, total_flags, flags_and, flags_or;
	total_flags = hostapd_sta_flags_to_drv(sta->flags, sta->flags_ext);
	set_flags = WPA_STA_SHORT_PREAMBLE | WPA_STA_WMM | WPA_STA_MFP |
		WPA_STA_AUTHORIZED | WPA_STA_CFP | WPA_STA_SMD;

#ifdef CONFIG_IEEE80211BN
	if (sta->smd_info.smd_sta)
		set_flags |= WPA_STA_ASSOCIATED;
#endif /* CONFIG_IEEE80211BN */
	/*
	 * All the station flags other than WPA_STA_SHORT_PREAMBLE are relevant
	 * only for the MLD station and not to the link stations (as these flags
	 * are related to the MLD state and not the link state). As for the
	 * WPA_STA_SHORT_PREAMBLE, since the station is an EHT station, it must
	 * support short preamble. Thus, do not propagate the change to the
	 * driver for the link stations.
	 */
	if (hostapd_sta_is_link_sta(hapd, sta)) {
		wpa_printf(MSG_DEBUG,
			   "%s: Do not update link station flags (" MACSTR ")",
			   __func__, MAC2STR(sta->addr));
		return 0;
	}

	flags_or = total_flags & set_flags;
	flags_and = total_flags | ~set_flags;
	return hostapd_sta_set_flags(hapd, sta->addr, total_flags,
				     flags_or, flags_and);
}


int hostapd_set_drv_ieee8021x(struct hostapd_data *hapd, const char *ifname,
			      int enabled)
{
	struct wpa_bss_params params;
	os_memset(&params, 0, sizeof(params));
	params.ifname = ifname;
	params.enabled = enabled;
	if (enabled) {
		params.wpa = hapd->conf->wpa;
		params.ieee802_1x = hapd->conf->ieee802_1x;
		params.wpa_group = hapd->conf->wpa_group;
		if ((hapd->conf->wpa & (WPA_PROTO_WPA | WPA_PROTO_RSN)) ==
		    (WPA_PROTO_WPA | WPA_PROTO_RSN))
			params.wpa_pairwise = hapd->conf->wpa_pairwise |
				hapd->conf->rsn_pairwise;
		else if (hapd->conf->wpa & WPA_PROTO_RSN)
			params.wpa_pairwise = hapd->conf->rsn_pairwise;
		else if (hapd->conf->wpa & WPA_PROTO_WPA)
			params.wpa_pairwise = hapd->conf->wpa_pairwise;
		params.wpa_key_mgmt = hapd->conf->wpa_key_mgmt;
		params.rsn_preauth = hapd->conf->rsn_preauth;
		params.ieee80211w = hapd->conf->ieee80211w;
	}
	return hostapd_set_ieee8021x(hapd, &params);
}


int hostapd_vlan_if_add(struct hostapd_data *hapd, const char *ifname)
{
	char force_ifname[IFNAMSIZ];
	u8 if_addr[ETH_ALEN];
	const u8 *addr = hapd->own_addr;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

	return hostapd_if_add(hapd, WPA_IF_AP_VLAN, ifname, addr,
			      NULL, NULL, force_ifname, if_addr, NULL, 0,
			      hapd->conf->ppe_vp_type);
}


int hostapd_vlan_if_remove(struct hostapd_data *hapd, const char *ifname)
{
	return hostapd_if_remove(hapd, WPA_IF_AP_VLAN, ifname);
}


int hostapd_set_wds_sta(struct hostapd_data *hapd, char *ifname_wds,
			const u8 *addr, int aid, int val)
{
	const char *bridge = NULL;

	if (hapd->driver == NULL || hapd->driver->set_wds_sta == NULL)
		return -1;
	if (hapd->conf->wds_bridge[0])
		bridge = hapd->conf->wds_bridge;
	return hapd->driver->set_wds_sta(hapd->drv_priv, addr, aid, val,
					 bridge, ifname_wds);
}


int hostapd_add_sta_node(struct hostapd_data *hapd, const u8 *addr,
			 u16 auth_alg, bool is_ml)
{
	if (hapd->driver == NULL || hapd->driver->add_sta_node == NULL)
		return -EOPNOTSUPP;
	return hapd->driver->add_sta_node(hapd->drv_priv, addr, auth_alg, is_ml);
}


#ifdef CONFIG_IEEE80211BE
int hostapd_drv_mark_ppe_vp_type(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->mark_ppe_vp_type == NULL)
		return -1;

	return hapd->driver->mark_ppe_vp_type(hapd->drv_priv,
			OUI_QCA,
			QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
			NULL, 0, 0, NULL, hapd->conf->iface,
			hapd->conf->ppe_vp_type, true);
}
#endif


#ifdef CONFIG_IEEE80211AX
int hostapd_drv_rule_config_notify(struct hostapd_data *hapd, const u8 *mac,
				   const struct qm_req_desc_data *qm_desc,
				   enum qos_mgmt_type qm_type)
{
	if (hapd->driver == NULL || !hapd->driver->rule_config_notify)
		return -1;

	return hapd->driver->rule_config_notify(hapd->drv_priv,
			OUI_QCA,
			QCA_NL80211_VENDOR_SUBCMD_SCS_RULE_CONFIG,
			NULL, 0, 0, NULL, mac, hapd->conf->iface, qm_desc,
			qm_type);
}
#endif /* CONFIG_IEEE80211AX */

int hostapd_drv_notify_iface_state(struct hostapd_data *hapd, u32 bss_mode)
{
	int link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	if (hapd->driver == NULL || hapd->driver->notify_iface_state == NULL)
		return -1;

	return hapd->driver->notify_iface_state(
		hapd->drv_priv, OUI_QCA,
		QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
		QCA_WLAN_VENDOR_WIFI_PARAM_INTERFACE_EN_DIS_MODE,
		bss_mode, link_id, hapd->conf->iface);
}


int hostapd_sta_auth(struct hostapd_data *hapd, const u8 *addr,
		     u16 seq, u16 status, const u8 *ie, size_t len)
{
	struct wpa_driver_sta_auth_params params;
#ifdef CONFIG_FILS
	struct sta_info *sta;
#endif /* CONFIG_FILS */

	if (hapd->driver == NULL || hapd->driver->sta_auth == NULL)
		return 0;

	os_memset(&params, 0, sizeof(params));

#ifdef CONFIG_FILS
	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for sta_auth processing",
			   MAC2STR(addr));
		return 0;
	}

	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		params.fils_auth = 1;
		wpa_auth_get_fils_aead_params(sta->wpa_sm, params.fils_anonce,
					      params.fils_snonce,
					      params.fils_kek,
					      &params.fils_kek_len);
	}
#endif /* CONFIG_FILS */

	params.own_addr = hapd->own_addr;
	params.addr = addr;
	params.seq = seq;
	params.status = status;
	params.ie = ie;
	params.len = len;

	return hapd->driver->sta_auth(hapd->drv_priv, &params);
}


int hostapd_sta_assoc(struct hostapd_data *hapd, const u8 *addr,
		      int reassoc, u16 status, const u8 *ie, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->sta_assoc == NULL)
		return 0;
	return hapd->driver->sta_assoc(hapd->drv_priv, hapd->own_addr, addr,
				       reassoc, status, ie, len);
}

int hostapd_smd_roam(struct hostapd_data *hapd,
		     struct sta_info *sta,
		     u32 role,
		     u32 type,
		     bool dl_sn_not_transferred,
		     bool ul_sn_not_transferred,
		     u32 dl_drain_time)
{
	struct hostapd_smd_roam_params params;

	params.role = role;
	params.dl_sn_not_transferred = dl_sn_not_transferred;
	params.ul_sn_not_transferred = ul_sn_not_transferred;
	params.dl_drain_time = dl_drain_time;
	params.type = type;
	params.n_macs = 1;
	memcpy(params.mac[0], sta->mld_info.common_info.mld_addr, ETH_ALEN);

	return hapd->driver->smd_roam(hapd->drv_priv, &params);
}

int hostapd_sta_add(struct hostapd_data *hapd,
		    const u8 *addr,
		    u16 aid,
		    u16 capability,
		    const u8 *supp_rates,
		    size_t supp_rates_len,
		    u16 listen_interval,
		    const struct ieee80211_ht_capabilities *ht_capab,
		    const struct ieee80211_vht_capabilities *vht_capab,
		    const struct ieee80211_he_capabilities *he_capab,
		    size_t he_capab_len,
		    const struct ieee80211_eht_capabilities *eht_capab,
		    size_t eht_capab_len,
		    const struct ieee80211_uhr_capabilities *uhr_capab,
		    size_t uhr_capab_len,
#ifdef CONFIG_QCN_EXTN
		    struct sta_info_extn *sta_extn,
#endif
		    bool smd_sta, bool dl_data_fwd, const u8 *smd_mac_addr,
		    bool smd_roam_sta,
		    const struct ieee80211_he_6ghz_band_cap *he_6ghz_capab,
		    u32 flags, u8 qosinfo, u8 vht_opmode, int supp_p2p_ps,
		    int set, const u8 *link_addr, bool mld_link_sta,
		    u16 eml_cap, int type, u8 control_mic_pad, bool epp_sta)
{
	struct hostapd_sta_add_params params, *tmp = NULL;
	struct hostapd_data *assoc_hapd;
	struct sta_info *sta, *assoc_sta;

	if (hapd->driver == NULL)
		return 0;
	if (hapd->driver->sta_add == NULL)
		return 0;

	os_memset(&params, 0, sizeof(params));
	params.addr = addr;
	params.aid = aid;
	params.capability = capability;
	params.supp_rates = supp_rates;
	params.supp_rates_len = supp_rates_len;
	params.listen_interval = listen_interval;
	params.ht_capabilities = ht_capab;
	params.vht_capabilities = vht_capab;
	params.he_capab = he_capab;
	params.he_capab_len = he_capab_len;
	params.eht_capab = eht_capab;
	params.eht_capab_len = eht_capab_len;
	params.uhr_capab = uhr_capab;
	params.uhr_capab_len = uhr_capab_len;
	params.he_6ghz_capab = he_6ghz_capab;
	params.vht_opmode_enabled = !!(flags & WLAN_STA_VHT_OPMODE_ENABLED);
	params.vht_opmode = vht_opmode;
	params.flags = hostapd_sta_flags_to_drv(flags, 0);
#ifdef CONFIG_IEEE80211BN
	sta = ap_get_sta(hapd, addr);
	if (sta && sta->is_mapc_peer) {
		params.flags |= WPA_STA_MAPC_PEER;
		wpa_printf(MSG_DEBUG, "hostapd_add_sta: WPA_STA_MAPC_PEER set for " MACSTR,
			   MAC2STR(addr));
	}
#endif /* CONFIG_IEEE80211BN */
	params.qosinfo = qosinfo;
	params.support_p2p_ps = supp_p2p_ps;
	params.set = set;
	params.mld_link_id = -1;
	params.control_mic_pad = control_mic_pad;

#ifdef CONFIG_ENC_ASSOC
	params.epp_sta = epp_sta;
#endif /* CONFIG_ENC_ASSOC */

#ifdef CONFIG_QCN_EXTN
	hostapd_copy_sta_add_params_extn(&params.params_extn, sta_extn);
#endif /* CONFIG_QCN_EXTN */

#ifdef CONFIG_IEEE80211BE
	/*
	 * An AP MLD needs to always specify to what link the station needs
	 * to be added.
	 */
	if (hapd->conf->mld_ap) {
		params.mld_link_id = hapd->mld_link_id;
		params.mld_link_addr = link_addr;
		params.mld_link_sta = mld_link_sta;
		/* Copy EML capabilities of ML STA */
		if (link_addr)
			params.eml_cap = eml_cap;
	}

	params.smd_sta = smd_sta;
	params.dl_data_fwd = dl_data_fwd;
	params.smd_mac_addr = smd_mac_addr;
	params.smd_roam_sta = smd_roam_sta;

	if (type == LINK_PARSE_RECONF) {
		sta = ap_get_sta(hapd, addr);
		if (!sta) {
			wpa_printf(MSG_ERROR, "No link sta for addr = " MACSTR,
				   MAC2STR(addr));
			return -1;
		}
		assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
		if (!assoc_sta) {
			wpa_printf(MSG_ERROR, "No assoc sta found");
			return -1;
		}

		tmp = os_zalloc(sizeof(*tmp));
		if (!tmp) {
			wpa_printf(MSG_ERROR, "Failed to allocate reconf_sta_add_params");
			return -1;
		}

		os_memcpy(tmp, &params, sizeof(params));

		if (params.ht_capabilities) {
			tmp->ht_capabilities = os_memdup(params.ht_capabilities,
							 sizeof(*params.ht_capabilities));
			if (!tmp->ht_capabilities)
				goto fail;
		}
		if (params.vht_capabilities) {
			tmp->vht_capabilities = os_memdup(params.vht_capabilities,
							  sizeof(*params.vht_capabilities));
			if (!tmp->vht_capabilities)
				goto fail;
		}
		if (params.he_capab && params.he_capab_len) {
			tmp->he_capab = os_memdup(params.he_capab, params.he_capab_len);
			if (!tmp->he_capab)
				goto fail;
		}
		if (params.he_6ghz_capab) {
			tmp->he_6ghz_capab = os_memdup(params.he_6ghz_capab,
						       sizeof(*params.he_6ghz_capab));
			if (!tmp->he_6ghz_capab)
				goto fail;
		}
		if (params.eht_capab && params.eht_capab_len) {
			tmp->eht_capab = os_memdup(params.eht_capab, params.eht_capab_len);
			if (!tmp->eht_capab)
				goto fail;
		}

		assoc_sta->recfg_sta_add_params[hapd->mld_link_id] = tmp;
		return 0;
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->sta_add(hapd->drv_priv, &params);

fail:
	hostapd_free_reconf_sta_add_params(tmp);
	return -1;
}


int hostapd_add_tspec(struct hostapd_data *hapd, const u8 *addr,
		      u8 *tspec_ie, size_t tspec_ielen)
{
	if (hapd->driver == NULL || hapd->driver->add_tspec == NULL)
		return 0;
	return hapd->driver->add_tspec(hapd->drv_priv, addr, tspec_ie,
				       tspec_ielen);
}


int hostapd_set_privacy(struct hostapd_data *hapd, int enabled)
{
	if (hapd->driver == NULL || hapd->driver->set_privacy == NULL)
		return 0;
	return hapd->driver->set_privacy(hapd->drv_priv, enabled);
}


int hostapd_set_generic_elem(struct hostapd_data *hapd, const u8 *elem,
			     size_t elem_len)
{
	if (hapd->driver == NULL || hapd->driver->set_generic_elem == NULL)
		return 0;
	return hapd->driver->set_generic_elem(hapd->drv_priv, elem, elem_len);
}


int hostapd_get_ssid(struct hostapd_data *hapd, u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->hapd_get_ssid == NULL)
		return 0;
	return hapd->driver->hapd_get_ssid(hapd->drv_priv, buf, len);
}


int hostapd_set_ssid(struct hostapd_data *hapd, const u8 *buf, size_t len)
{
	if (hapd->driver == NULL || hapd->driver->hapd_set_ssid == NULL)
		return 0;
	return hapd->driver->hapd_set_ssid(hapd->drv_priv, buf, len);
}


int hostapd_if_add(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		   const char *ifname, const u8 *addr, void *bss_ctx,
		   void **drv_priv, char *force_ifname, u8 *if_addr,
		   const char *bridge, int use_existing, int ppe_vp_type)
{
	if (hapd->driver == NULL || hapd->driver->if_add == NULL)
		return -1;
	return hapd->driver->if_add(hapd->drv_priv, type, ifname, addr,
				    bss_ctx, drv_priv, force_ifname, if_addr,
				    bridge, use_existing, 1, ppe_vp_type);
}


#ifdef CONFIG_IEEE80211BE
int hostapd_if_link_remove(struct hostapd_data *hapd,
			   enum wpa_driver_if_type type,
			   const char *ifname, u8 link_id)
{
	if (!hapd->driver || !hapd->drv_priv || !hapd->driver->link_remove)
		return -1;

	if (!hapd->driver->link_remove(hapd->drv_priv, type, ifname,
					 hapd->mld_link_id)) {
		hapd->drv_priv = NULL;
		return 0;
	}

	return -1;
}
#endif /* CONFIG_IEEE80211BE */


int hostapd_if_remove(struct hostapd_data *hapd, enum wpa_driver_if_type type,
		      const char *ifname)
{
	if (hapd->driver == NULL || hapd->drv_priv == NULL ||
	    hapd->driver->if_remove == NULL)
		return -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && type == WPA_IF_AP_BSS)
		return hostapd_if_link_remove(hapd, type, ifname,
					      hapd->mld_link_id);
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->if_remove(hapd->drv_priv, type, ifname);
}


#ifdef CONFIG_IEEE80211BE
int hostapd_drv_ml_reconfig_link_remove(struct hostapd_data *hapd,
					enum wpa_driver_if_type type,
					const struct driver_reconfig_link_removal_params *params)
{
	if (hapd->driver == NULL || hapd->drv_priv == NULL ||
	    hapd->driver->ml_reconfig_link_remove == NULL)
		return -1;

	return hapd->driver->ml_reconfig_link_remove(hapd->drv_priv, type, params);
}
#endif /* CONFIG_IEEE80211BE */


int hostapd_set_ieee8021x(struct hostapd_data *hapd,
			  struct wpa_bss_params *params)
{
	if (hapd->driver == NULL || hapd->driver->set_ieee8021x == NULL)
		return 0;
	return hapd->driver->set_ieee8021x(hapd->drv_priv, params);
}


int hostapd_get_seqnum(const char *ifname, struct hostapd_data *hapd,
		       const u8 *addr, int idx, int link_id, u8 *seq, int get_cigtk_seq_num)
{
	if (hapd->driver == NULL || hapd->driver->get_seqnum == NULL)
		return 0;
	return hapd->driver->get_seqnum(ifname, hapd->drv_priv, addr, idx,
					link_id, seq, get_cigtk_seq_num);
}


int hostapd_flush(struct hostapd_data *hapd)
{
	int link_id = -1;

	if (hapd->driver == NULL || hapd->driver->flush == NULL ||
	    hapd->drv_priv == NULL)
		return 0;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->flush(hapd->drv_priv, link_id);
}


int hostapd_set_freq(struct hostapd_data *hapd, enum hostapd_hw_mode mode,
		     int freq, int channel, int edmg, u8 edmg_channel,
		     int ht_enabled, int vht_enabled,
		     int he_enabled, bool eht_enabled, bool uhr_enabled,
		     int sec_channel_offset, int oper_chwidth,
		     int center_segment0, int center_segment1,
#ifdef CONFIG_QCN_EXTN
		     bool skip_cac_rep,
#endif
		     int bandwidth_device, int center_freq_device)
{
	struct hostapd_freq_params data;
	struct hostapd_hw_modes *cmode = hapd->iface->current_mode;

	if (hostapd_set_freq_params(&data, mode, freq, channel, edmg,
				    edmg_channel, ht_enabled,
				    vht_enabled, he_enabled, eht_enabled,
				    uhr_enabled, sec_channel_offset, oper_chwidth,
				    center_segment0, center_segment1,
				    cmode ? cmode->vht_capab : 0,
				    cmode ?
				    &cmode->he_capab[IEEE80211_MODE_AP] : NULL,
				    cmode ?
				    &cmode->eht_capab[IEEE80211_MODE_AP] :
				    NULL,
				    cmode ?
				    &cmode->uhr_capab[IEEE80211_MODE_AP] :
				    NULL,
				    hostapd_get_punct_bitmap(hapd),
				    hapd->iconf->he_6ghz_reg_pwr_type,
#ifdef CONFIG_IEEE80211BN
				    hostapd_hw_get_freq(hapd, hapd->iconf->npca_primary_channel),
				    hapd->iconf->npca_punct_bitmap,
#else
				    0, 0,
#endif /* CONFIG_IEEE80211BN */
				    bandwidth_device, center_freq_device))
		return -1;

#ifdef CONFIG_QCN_EXTN
	if (hostapd_validate_hw_blocklist_for_freq_params_extn(
		    hapd->iface, &data, data.he_6ghz_reg_pwr_type,
		    "set_freq"))
		return -1;

	update_chan_params(hapd, data.center_freq1, data.center_freq2, hostapd_get_chan_width_from_oper_chan_width(hapd->iconf));
#endif

	if (hapd->driver == NULL)
		return 0;
	if (hapd->driver->set_freq == NULL)
		return 0;

	data.link_id = -1;

#ifdef CONFIG_QCN_EXTN
	if (skip_cac_rep)
		data.skip_cac = 1;
#endif
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap) {
		data.link_id = hapd->mld_link_id;
		wpa_printf(MSG_DEBUG,
			   "hostapd_set_freq: link_id=%d", data.link_id);
	}
#endif /* CONFIG_IEEE80211BE */

	/*
	 * Only the first AP interface triggers the monitor update to avoid
	 * redundant calls.
	 */
	if (hapd == hapd->iface->bss[0])
		if (hostapd_update_monitor_channel(hapd, &data))
			return -1;

	return hapd->driver->set_freq(hapd->drv_priv, &data);
}

int hostapd_set_rts(struct hostapd_data *hapd, int rts)
{
	if (hapd->driver == NULL || hapd->driver->set_rts == NULL)
		return 0;
	return hapd->driver->set_rts(hapd->drv_priv, rts);
}


int hostapd_set_frag(struct hostapd_data *hapd, int frag)
{
	if (hapd->driver == NULL || hapd->driver->set_frag == NULL)
		return 0;
	return hapd->driver->set_frag(hapd->drv_priv, frag);
}


int hostapd_sta_set_flags(struct hostapd_data *hapd, u8 *addr,
			  int total_flags, int flags_or, int flags_and)
{
	if (!hapd->driver || !hapd->drv_priv || !hapd->driver->sta_set_flags)
		return 0;
	return hapd->driver->sta_set_flags(hapd->drv_priv, addr, total_flags,
					   flags_or, flags_and);
}


#ifdef CONFIG_IEEE80211BN
int hostapd_sta_set_mapc_params(struct hostapd_data *hapd, const u8 *addr,
				const struct mapc_parameters *params)
{
	if (!hapd->driver || !hapd->driver->sta_set_mapc_params)
		return 0;
	return hapd->driver->sta_set_mapc_params(hapd->drv_priv, addr, params);
}
#endif /* CONFIG_IEEE80211BN */


int hostapd_sta_set_airtime_weight(struct hostapd_data *hapd, const u8 *addr,
				   unsigned int weight)
{
	if (!hapd->driver || !hapd->driver->sta_set_airtime_weight)
		return 0;
	return hapd->driver->sta_set_airtime_weight(hapd->drv_priv, addr,
						    weight);
}


int hostapd_set_country(struct hostapd_data *hapd, const char *country)
{
	if (hapd->driver == NULL ||
	    hapd->driver->set_country == NULL)
		return 0;
	return hapd->driver->set_country(hapd->drv_priv, country);
}


int hostapd_set_tx_queue_params(struct hostapd_data *hapd, int queue, int aifs,
				int cw_min, int cw_max, int burst_time,
				int acm, int noack)
{
	int link_id = -1;

	if (hapd->driver == NULL || hapd->driver->set_tx_queue_params == NULL)
		return 0;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->set_tx_queue_params(hapd->drv_priv, queue, aifs,
						 cw_min, cw_max, burst_time,
						 acm, noack, link_id);
}


struct hostapd_hw_modes *
hostapd_get_hw_feature_data(struct hostapd_data *hapd, u16 *num_modes,
			    u16 *flags, u8 *dfs_domain)
{
	if (!hapd->driver || !hapd->driver->get_hw_feature_data ||
	    !hapd->drv_priv)
		return NULL;
	return hapd->driver->get_hw_feature_data(hapd->drv_priv, num_modes,
						 flags, dfs_domain,
						 hapd->iconf->he_6ghz_reg_pwr_type);
}


int hostapd_driver_commit(struct hostapd_data *hapd)
{
	if (hapd->driver == NULL || hapd->driver->commit == NULL)
		return 0;
	return hapd->driver->commit(hapd->drv_priv);
}


int hostapd_drv_none(struct hostapd_data *hapd)
{
	return hapd->driver && os_strcmp(hapd->driver->name, "none") == 0;
}


bool hostapd_drv_nl80211(struct hostapd_data *hapd)
{
	return hapd->driver && os_strcmp(hapd->driver->name, "nl80211") == 0;
}


int hostapd_driver_scan(struct hostapd_data *hapd,
			struct wpa_driver_scan_params *params)
{
	params->link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		params->link_id = hapd->mld_link_id;
	if (!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_PARALLEL_HW_SCAN) &&
	    !hapd->iface->scan_cb && hapd->conf->mld_ap &&
	    hapd->iface->interfaces) {
		/* Other links may be waiting for scan results */
		unsigned int i;

		for (i = 0; i < hapd->iface->interfaces->count; i++) {
			struct hostapd_iface *h_iface =
				hapd->iface->interfaces->iface[i];
			struct hostapd_data *h_hapd;

			if (!h_iface || h_iface == hapd->iface ||
			    h_iface->num_bss == 0)
				continue;

			h_hapd = h_iface->bss[0];

			if (hostapd_is_ml_partner(hapd, h_hapd) &&
			    (h_hapd->iface->state == HAPD_IFACE_ACS ||
			     h_hapd->iface->state == HAPD_IFACE_HT_SCAN) &&
			    h_hapd->iface->scan_cb) {
				wpa_printf(MSG_INFO,
					   "ACS in progress in a partner link %d- try to scan later", hapd->mld_link_id);
				return -EBUSY;
			}
		}
	}
#endif /* CONFIG_IEEE80211BE */

	if (hapd->driver && hapd->driver->scan2)
		return hapd->driver->scan2(hapd->drv_priv, params);
	return -1;
}


struct wpa_scan_results * hostapd_driver_get_scan_results(
	struct hostapd_data *hapd)
{
	if (hapd->driver && hapd->driver->get_scan_results)
		return hapd->driver->get_scan_results(hapd->drv_priv, NULL);
	if (hapd->driver && hapd->driver->get_scan_results2)
		return hapd->driver->get_scan_results2(hapd->drv_priv);
	return NULL;
}


int hostapd_driver_set_noa(struct hostapd_data *hapd, u8 count, int start,
			   int duration)
{
	if (hapd->driver && hapd->driver->set_noa)
		return hapd->driver->set_noa(hapd->drv_priv, count, start,
					     duration);
	return -1;
}


int hostapd_drv_set_key(const char *ifname, struct hostapd_data *hapd,
			enum wpa_alg alg, const u8 *addr,
			int key_idx, int vlan_id, int set_tx,
			const u8 *seq, size_t seq_len,
			const u8 *key, size_t key_len, enum key_flag key_flag)
{
	struct wpa_driver_set_key_params params;

	if (hapd->driver == NULL || hapd->driver->set_key == NULL)
		return 0;

	os_memset(&params, 0, sizeof(params));
	params.ifname = ifname;
	params.alg = alg;
	params.addr = addr;
	params.key_idx = key_idx;
	params.set_tx = set_tx;
	params.seq = seq;
	params.seq_len = seq_len;
	params.key = key;
	params.key_len = key_len;
	params.vlan_id = vlan_id;
	params.key_flag = key_flag;
	params.link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && !(key_flag & KEY_FLAG_PAIRWISE))
		params.link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->set_key(hapd->drv_priv, &params);
}


int hostapd_drv_send_mlme(struct hostapd_data *hapd,
			  const void *msg, size_t len, int noack,
			  const u16 *csa_offs, size_t csa_offs_len,
			  int no_encrypt, u16 rate, u8 rate_type)
{
	int link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->driver || !hapd->driver->send_mlme || !hapd->drv_priv)
		return 0;
	return hapd->driver->send_mlme(hapd->drv_priv, msg, len, noack, 0, rate, rate_type,
				       csa_offs, csa_offs_len, no_encrypt, 0,
				       link_id);
}


int hostapd_drv_sta_deauth(struct hostapd_data *hapd,
			   const u8 *addr, int reason)
{
	int link_id = -1;
	const u8 *own_addr = hapd->own_addr;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap) {
		struct sta_info *sta = ap_get_sta(hapd, addr);

		link_id = hapd->mld_link_id;
		if (ap_sta_is_mld(hapd, sta))
			own_addr = hapd->mld->mld_addr;
	}
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->driver || !hapd->driver->sta_deauth || !hapd->drv_priv)
		return 0;
	return hapd->driver->sta_deauth(hapd->drv_priv, own_addr, addr,
					reason, link_id);
}
int hostapd_drv_sta_disassoc(struct hostapd_data *hapd,
			     const u8 *addr, int reason)
{
	const u8 *own_addr = hapd->own_addr;
	int link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap) {
		struct sta_info *sta = ap_get_sta(hapd, addr);

		if (ap_sta_is_mld(hapd, sta)) {
			own_addr = hapd->mld->mld_addr;
			link_id = hapd->mld_link_id;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->driver || !hapd->driver->sta_disassoc || !hapd->drv_priv)
		return 0;
	return hapd->driver->sta_disassoc(hapd->drv_priv, own_addr, addr,
					  reason, link_id);
}


int hostapd_drv_wnm_oper(struct hostapd_data *hapd, enum wnm_oper oper,
			 const u8 *peer, u8 *buf, u16 *buf_len)
{
	if (hapd->driver == NULL || hapd->driver->wnm_oper == NULL)
		return -1;
	return hapd->driver->wnm_oper(hapd->drv_priv, oper, peer, buf,
				      buf_len);
}


#ifdef CONFIG_IEEE80211BE
static bool hostapd_is_action_frame_link_agnostic(const u8 *data, size_t len)
{
	u8 category = data[0];
	u8 sub_category = data[1];

	/* As per IEEE Std 802.11be-2024, 35.3.14 (MLD individually addressed
	 * Management frame delivery), between an AP MLD and a non-AP MLD, the
	 * following individually addressed MMPDUs shall be intended for an MLD.
	 */
	switch (category) {
	case WLAN_ACTION_BLOCK_ACK:
	case WLAN_ACTION_FT:
	case WLAN_ACTION_SA_QUERY:
	case WLAN_ACTION_WNM:
		switch (sub_category) {
		case WNM_BSS_TRANS_MGMT_REQ:
		case WNM_BSS_TRANS_MGMT_RESP:
		case WNM_SLEEP_MODE_REQ:
		case WNM_SLEEP_MODE_RESP:
			return true;
		default:
			return false;
		}
	case WLAN_ACTION_ROBUST_AV_STREAMING:
		switch (sub_category) {
		case ROBUST_AV_SCS_REQ:
		case ROBUST_AV_SCS_RESP:
		case ROBUST_AV_MSCS_REQ:
		case ROBUST_AV_MSCS_RESP:
			return true;
		default:
			return false;
		}
	case WLAN_ACTION_VENDOR_SPECIFIC_PROTECTED:
		if (len < 6)
			return false;

		if (WPA_GET_BE32(&data[1]) == QM_ACTION_VENDOR_TYPE &&
		    data[5] == QM_DSCP_POLICY_REQ)
			return true;
		return false;

	/* TODO: Handle EHT/EPCS related action frames once the support is
	 * added. */
	default:
		return false;
	}
}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211BN
static bool mapc_action_frame(u8 action_code)
{
	switch (action_code) {
	case WLAN_PA_MAPC_DISCOVERY_REQ:
	case WLAN_PA_MAPC_DISCOVERY_RESP:
	case WLAN_PA_MAPC_NEGOTIATION_REQ:
	case WLAN_PA_MAPC_NEGOTIATION_RESP:
		return true;
	default:
		return false;
	}
}
#endif /* CONFIG_IEEE80211BN */


static int hapd_drv_send_action(struct hostapd_data *hapd, unsigned int freq,
				unsigned int wait, const u8 *dst,
				const u8 *data, size_t len, bool addr3_ap,
				const u8 *forced_a3)
{
	const u8 *own_addr = hapd->own_addr;
	const u8 *bssid;
	const u8 wildcard_bssid[ETH_ALEN] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	struct sta_info *sta;
	int link_id = -1;

	if (!hapd->driver || !hapd->driver->send_action || !hapd->drv_priv)
		return 0;
	bssid = hapd->own_addr;
	if (forced_a3) {
		bssid = forced_a3;
	} else if (!addr3_ap && !is_multicast_ether_addr(dst) &&
		   len > 0 && data[0] == WLAN_ACTION_PUBLIC
#ifdef CONFIG_IEEE80211BN
		   && !mapc_action_frame(data[1])
#endif
		   ) {
		/*
		 * Public Action frames to a STA that is not a member of the BSS
		 * shall use wildcard BSSID value.
		 */
		sta = ap_get_sta(hapd, dst);
		if (!sta || !(sta->flags & WLAN_STA_ASSOC))
			bssid = wildcard_bssid;
	} else if (!addr3_ap && is_broadcast_ether_addr(dst) &&
		   len > 0 && data[0] == WLAN_ACTION_PUBLIC
#ifdef CONFIG_IEEE80211BN
		   && !mapc_action_frame(data[1])
#endif
		   ) {
		/*
		 * The only current use case of Public Action frames with
		 * broadcast destination address is DPP PKEX. That case is
		 * directing all devices and not just the STAs within the BSS,
		 * so have to use the wildcard BSSID value.
		 */
		bssid = wildcard_bssid;
#ifdef CONFIG_IEEE80211BE
	} else if (hapd->conf->mld_ap) {
		sta = ap_get_sta(hapd, dst);

		if (ap_sta_is_mld(hapd, sta)) {
			if (hapd->mld) {
				own_addr = hapd->mld->mld_addr;
				bssid = own_addr;
			}
		}

#ifdef CONFIG_QCN_EXTN
		/* If the link BSS is repurposed to lower modes, populate
		 * link id as the frame can't be link agnostic.
		 */
		if (hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
			link_id = hapd->mld_link_id;
		} else {
#endif /* CONFIG_QCN_EXTN */

		if (!hostapd_is_action_frame_link_agnostic(data, len))
			link_id = hapd->mld_link_id;
#ifdef CONFIG_QCN_EXTN
		}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */
	}

	return hapd->driver->send_action(hapd->drv_priv, freq, wait, dst,
					 own_addr, bssid, data, len, 0,
					 link_id);
}


int hostapd_drv_send_action(struct hostapd_data *hapd, unsigned int freq,
			    unsigned int wait, const u8 *dst, const u8 *data,
			    size_t len)
{
	return hapd_drv_send_action(hapd, freq, wait, dst, data, len, false,
				    NULL);
}

int hostapd_drv_notify_radar(struct hostapd_data *hapd,
			     struct hostapd_freq_params *freq,
			     u16 radar_bitmap)
{
	if (!hapd || !hapd->driver || !hapd->driver->notify_radar ||
	    !hapd->drv_priv)
		return -1;

	return hapd->driver->notify_radar(hapd->drv_priv, freq, radar_bitmap);
}

int hostapd_drv_send_action_addr3_ap(struct hostapd_data *hapd,
				     unsigned int freq,
				     unsigned int wait, const u8 *dst,
				     const u8 *data, size_t len)
{
	return hapd_drv_send_action(hapd, freq, wait, dst, data, len, true,
				    NULL);
}


int hostapd_drv_send_action_forced_addr3(struct hostapd_data *hapd,
					 unsigned int freq,
					 unsigned int wait, const u8 *dst,
					 const u8 *a3,
					 const u8 *data, size_t len)
{
	return hapd_drv_send_action(hapd, freq, wait, dst, data, len, false,
				    a3);
}


int hostapd_stop_background_cac(struct hostapd_data *hapd)
{
	int radio_idx;
	int link_id = -1;

	if (!hapd->driver || !hapd->driver->stop_background_cac ||
	    !hapd->drv_priv)
		return -1;

	radio_idx = (hapd->iface && hapd->iface->current_hw_info) ?
		(int)hapd->iface->current_hw_info->hw_idx : -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf && hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->stop_background_cac(hapd->drv_priv, radio_idx,
					      link_id);
}


int hostapd_start_dfs_cac(struct hostapd_iface *iface,
			  enum hostapd_hw_mode mode, int freq,
			  int channel, int ht_enabled, int vht_enabled,
			  int he_enabled, bool eht_enabled, bool uhr_enabled,
			  int sec_channel_offset, int oper_chwidth,
			  int center_segment0, int center_segment1,
			  bool radar_background,
			  int bandwidth_device, int center_freq_device)
{
	struct hostapd_data *hapd = NULL;
	struct hostapd_freq_params data;
	int res, radio_idx;
	struct hostapd_hw_modes *cmode = iface->current_mode;
	size_t b;
#ifdef CONFIG_QCN_EXTN
	bool is_dfs = false;
	bool mcst_allows_skip_cac = false;
	enum chan_width chanwidth;
#endif

	/* Use the first started, non-disabled BSS as the CAC initiator.
	 * If no BSS is started yet (e.g. enable_bss on a DFS channel before
	 * setup completes), fall back to the first BSS pending re-enable
	 * (reenable != REENABLE_REUSE_LINK) so we use the BSS being enabled
	 * rather than a user-disabled bss[0] whose vdev the firmware would
	 * bring up as AP after CAC completes. */
	for (b = 0; b < iface->num_bss; b++) {
		if (iface->bss[b]->started && !iface->bss[b]->disabled) {
			hapd = iface->bss[b];
			break;
		}
	}
	if (!hapd) {
		for (b = 0; b < iface->num_bss; b++) {
			if (iface->bss[b]->reenable != REENABLE_REUSE_LINK) {
				hapd = iface->bss[b];
				break;
			}
		}
	}
	if (!hapd)
		hapd = iface->bss[0];

	if (!hapd->driver || !hapd->driver->start_dfs_cac || !cmode)
		return 0;

	if (!iface->conf->ieee80211h) {
		wpa_printf(MSG_ERROR, "Can't start DFS CAC, DFS functionality "
			   "is not enabled");
		return -1;
	}

	if (hostapd_set_freq_params(&data, mode, freq, channel, 0, 0,
				    ht_enabled,
				    vht_enabled, he_enabled, eht_enabled,
				    uhr_enabled, sec_channel_offset,
				    oper_chwidth, center_segment0,
				    center_segment1,
				    cmode->vht_capab,
				    &cmode->he_capab[IEEE80211_MODE_AP],
				    &cmode->eht_capab[IEEE80211_MODE_AP],
				    &cmode->uhr_capab[IEEE80211_MODE_AP],
				    radar_background ? 0 :
				    (hostapd_get_punct_bitmap(hapd) |
				    iface->radar_bit_pattern),
				    hapd->iconf->he_6ghz_reg_pwr_type,
				    0, 0,
				    bandwidth_device, center_freq_device)) {
		wpa_printf(MSG_ERROR, "Can't set freq params");
		return -1;
	}
	data.radar_background = radar_background;

#ifdef CONFIG_QCN_EXTN
	data.mcst = iface->mcst;
	chanwidth = hostapd_oper_chwidth_to_chanwidth_extn(oper_chwidth,
							   sec_channel_offset);
	is_dfs = ieee80211_is_dfs(freq, NULL, 0);
	if (!is_dfs && channel_width_to_int(chanwidth) == 160)
		is_dfs = ieee80211_is_dfs(freq + 80, NULL, 0);

	mcst_allows_skip_cac =
		hostapd_mcst_allows_skip_cac_extn(iface->mcst,
						  iface->conf->beacon_int, is_dfs);

	if (iface->conf->conf_extn.ind_rptr) {
		data.skip_cac = ((iface->iface_extn.csa_bitmap ||
				 iface->iface_extn.dfs_available_from_sta) &&
				 iface->conf->conf_extn.skip_cac);
		if (iface->mcst && iface->conf->conf_extn.skip_cac &&
		    !mcst_allows_skip_cac)
			data.skip_cac = 0;
		else if (!data.skip_cac && mcst_allows_skip_cac) {
			/* Skip CAC for DFS based on MCST - PreCAC case */
			data.skip_cac = 1;
		}

		/* CSwOpts 0x4: AP must perform CAC on STA-triggered bring-up.
		 * Override any skip_cac set above; the config-file skip_cac=1 may
		 * have been restored by a config reload after the runtime CSwOpts
		 * command cleared it to 0. */
		if (IS_CSH_CAC_APUP_BYSTA_ENABLED(iface->conf->conf_extn.cswopts))
			data.skip_cac = 0;

		wpa_printf(MSG_INFO, "Ind Rptr: skip_cac = %d csa_bitmap = %d"
			   " dfs_available_from_sta = %d conf_extn.skip_cac = %d mcst = %u",
			   data.skip_cac, iface->iface_extn.csa_bitmap,
			   iface->iface_extn.dfs_available_from_sta,
			   iface->conf->conf_extn.skip_cac,
			   iface->mcst);
	} else {
		data.skip_cac = (iface->cac_type != HAPD_CAC_COMPLETE_AFTER_CSA) &&
				iface->iface_extn.dfs_available_from_sta &&
				 iface->conf->conf_extn.skip_cac;
		if (iface->mcst && iface->conf->conf_extn.skip_cac &&
		    !mcst_allows_skip_cac)
			data.skip_cac = 0;
		else if (!data.skip_cac && mcst_allows_skip_cac) {
			/* Skip CAC for DFS based on MCST - PreCAC case */
			data.skip_cac = 1;
		}

		/* CSwOpts 0x4: AP must perform CAC on STA-triggered bring-up.
		 * Override any skip_cac set above; the config-file skip_cac=1 may
		 * have been restored by a config reload after the runtime CSwOpts
		 * command cleared it to 0. */
		if (IS_CSH_CAC_APUP_BYSTA_ENABLED(iface->conf->conf_extn.cswopts))
			data.skip_cac = 0;

		wpa_printf(MSG_INFO, "Dep Rptr: skip_cac = %d cac_type = %d"
			   " conf_extn.skip_cac = %d mcst = %u", data.skip_cac,
			   iface->cac_type, iface->conf->conf_extn.skip_cac, iface->mcst);
	}
#endif

	data.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		data.link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	if (!radar_background && hostapd_update_monitor_channel(hapd, &data))
		return -1;

	radio_idx = (radar_background && iface->current_hw_info) ?
		(int)iface->current_hw_info->hw_idx : -1;
	res = hapd->driver->start_dfs_cac(hapd->drv_priv, &data, radio_idx);
	if (!res) {
		if (radar_background) {
			iface->radar_background.cac_started = 1;
			os_get_reltime(&iface->radar_background.dfs_cac_start);
		} else {
			iface->cac_started = 1;
			os_get_reltime(&iface->dfs_cac_start);
		}
	}

	return res;
}


int hostapd_drv_set_qos_map(struct hostapd_data *hapd,
			    const u8 *qos_map_set, u8 qos_map_set_len)
{
	if (!hapd->driver || !hapd->driver->set_qos_map || !hapd->drv_priv ||
	    !(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_QOS_MAPPING))
		return 0;
	return hapd->driver->set_qos_map(hapd->drv_priv, qos_map_set,
					 qos_map_set_len);
}


void hostapd_get_hw_mode_any_channels(struct hostapd_data *hapd,
				      struct hostapd_hw_modes *mode,
				      int acs_ch_list_all, bool allow_disabled,
				      int **freq_list)
{
	int i;
	bool is_no_ir = false;
	bool allow_6g_acs = is_6ghz_op_class(hapd->iconf->op_class) &&
		hostapd_config_check_bss_6g(hapd->conf) &&
		(hapd->iface->conf->ieee80211ax ||
		 hapd->iface->conf->ieee80211be);

	for (i = 0; i < mode->num_channels; i++) {
		struct hostapd_channel_data *chan = &mode->channels[i];

		if (!acs_ch_list_all &&
		    (hapd->iface->conf->acs_freq_list.num &&
		     !freq_range_list_includes(
			     &hapd->iface->conf->acs_freq_list,
			     chan->freq)))
			continue;
		if (!acs_ch_list_all &&
		    (!hapd->iface->conf->acs_freq_list_present &&
		     hapd->iface->conf->acs_ch_list.num &&
		     !freq_range_list_includes(
			     &hapd->iface->conf->acs_ch_list,
			     chan->chan)))
			continue;
		if (is_6ghz_freq(chan->freq) &&
		    ((hapd->iface->conf->acs_exclude_6ghz_non_psc &&
		      !is_6ghz_psc_frequency(chan->freq)) ||
		     !allow_6g_acs))
			continue;
		if ((!(chan->flag & HOSTAPD_CHAN_DISABLED) || allow_disabled) &&
		    !(hapd->iface->conf->acs_exclude_dfs &&
		      (chan->flag & HOSTAPD_CHAN_RADAR)) &&
		    !(chan->max_tx_power < hapd->iface->conf->min_tx_power))
			int_array_add_unique(freq_list, chan->freq);
		else if ((chan->flag & HOSTAPD_CHAN_NO_IR) &&
			 is_6ghz_freq(chan->freq))
			is_no_ir = true;
	}

	hapd->iface->is_no_ir = is_no_ir;
}


void hostapd_get_ext_capa(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];

	if (!hapd->driver || !hapd->driver->get_ext_capab)
		return;

	hapd->driver->get_ext_capab(hapd->drv_priv, WPA_IF_AP_BSS,
				    &iface->extended_capa,
				    &iface->extended_capa_mask,
				    &iface->extended_capa_len);
}


void hostapd_get_mld_capa(struct hostapd_iface *iface)
{
	struct hostapd_data *hapd = iface->bss[0];

	if (!hapd->driver || !hapd->driver->get_mld_capab)
		return;

	hapd->driver->get_mld_capab(hapd->drv_priv, WPA_IF_AP_BSS,
				    &iface->mld_eml_capa,
				    &iface->mld_mld_capa,
				    &iface->mld_ext_mld_capa);
}


/**
 * hostapd_drv_do_acs - Start automatic channel selection
 * @hapd: BSS data for the device initiating ACS
 * Returns: 0 on success, -1 on failure, 1 on failure due to NO_IR (AFC)
 */
int hostapd_drv_do_acs(struct hostapd_data *hapd)
{
	struct drv_acs_params params;
	int ret, i, acs_ch_list_all = 0;
	struct hostapd_hw_modes *mode;
	int *freq_list = NULL;
	enum hostapd_hw_mode selected_mode;

	if (hapd->driver == NULL || hapd->driver->do_acs == NULL)
		return 0;

	os_memset(&params, 0, sizeof(params));
	params.hw_mode = hapd->iface->conf->hw_mode;
	params.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		params.link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	/*
	 * If no chanlist config parameter is provided, include all enabled
	 * channels of the selected hw_mode.
	 */
	if (hapd->iface->conf->acs_freq_list_present)
		acs_ch_list_all = !hapd->iface->conf->acs_freq_list.num;
	else
		acs_ch_list_all = !hapd->iface->conf->acs_ch_list.num;

	if (hapd->iface->current_mode)
		selected_mode = hapd->iface->current_mode->mode;
	else
		selected_mode = HOSTAPD_MODE_IEEE80211ANY;

	for (i = 0; i < hapd->iface->num_hw_features; i++) {
		mode = &hapd->iface->hw_features[i];
		if (selected_mode != HOSTAPD_MODE_IEEE80211ANY &&
		    selected_mode != mode->mode)
			continue;
		hostapd_get_hw_mode_any_channels(hapd, mode, acs_ch_list_all,
						 false, &freq_list);
	}

	if (!freq_list && hapd->iface->is_no_ir) {
		wpa_printf(MSG_ERROR,
			   "NO_IR: Interface freq_list is empty. Failing do_acs.");
		return 1;
	}

	params.freq_list = freq_list;
	params.edmg_enabled = hapd->iface->conf->enable_edmg;

	params.ht_enabled = !!(hapd->iface->conf->ieee80211n);
	params.ht40_enabled = !!(hapd->iface->conf->ht_capab &
				 HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET);
	params.vht_enabled = !!(hapd->iface->conf->ieee80211ac);
	params.eht_enabled = !!(hapd->iface->conf->ieee80211be);
	params.ch_width = 20;
	if (hapd->iface->conf->ieee80211n && params.ht40_enabled)
		params.ch_width = 40;

	/* Note: VHT20 is defined by combination of ht_capab & oper_chwidth
	 */
	if ((hapd->iface->conf->ieee80211be ||
	     hapd->iface->conf->ieee80211ax ||
	     hapd->iface->conf->ieee80211ac) &&
	    params.ht40_enabled) {
		enum oper_chan_width oper_chwidth;

		oper_chwidth = hostapd_get_oper_chwidth(hapd->iface->conf);
		if (oper_chwidth == CONF_OPER_CHWIDTH_80MHZ)
			params.ch_width = 80;
		else if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ ||
			 oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ)
			params.ch_width = 160;
		else if (oper_chwidth == CONF_OPER_CHWIDTH_320MHZ)
			params.ch_width = 320;
	}

	if (hapd->iface->conf->op_class)
		params.ch_width = op_class_to_bandwidth(
			hapd->iface->conf->op_class);
	ret = hapd->driver->do_acs(hapd->drv_priv, &params);
	os_free(freq_list);

	return ret;
}
int hostapd_drv_update_dh_ie(struct hostapd_data *hapd, const u8 *peer,
			     u16 reason_code, const u8 *ie, size_t ielen)
{
	if (!hapd->driver || !hapd->driver->update_dh_ie || !hapd->drv_priv)
		return 0;
	return hapd->driver->update_dh_ie(hapd->drv_priv, peer, reason_code,
					  ie, ielen);
}


int hostapd_drv_dpp_listen(struct hostapd_data *hapd, bool enable)
{
	if (!hapd->driver || !hapd->driver->dpp_listen || !hapd->drv_priv)
		return 0;
	return hapd->driver->dpp_listen(hapd->drv_priv, enable);
}
#ifdef CONFIG_PASN
int hostapd_drv_set_secure_ranging_ctx(struct hostapd_data *hapd,
				       const u8 *own_addr, const u8 *peer_addr,
				       u32 cipher, u8 tk_len, const u8 *tk,
				       u8 ltf_keyseed_len,
				       const u8 *ltf_keyseed, u32 action)
{
	struct secure_ranging_params params;

	if (!hapd->driver || !hapd->driver->set_secure_ranging_ctx)
		return 0;

	os_memset(&params, 0, sizeof(params));
	params.own_addr = own_addr;
	params.peer_addr = peer_addr;
	params.cipher = cipher;
	params.tk_len = tk_len;
	params.tk = tk;
	params.ltf_keyseed_len = ltf_keyseed_len;
	params.ltf_keyseed = ltf_keyseed;
	params.action = action;
	params.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		params.link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->set_secure_ranging_ctx(hapd->drv_priv, &params);
}
#endif /* CONFIG_PASN */


struct hostapd_multi_hw_info *
hostapd_get_multi_hw_info(struct hostapd_data *hapd,
			  unsigned int *num_multi_hws)
{
	if (!hapd->driver || !hapd->driver->get_multi_hw_info)
		return NULL;

	return hapd->driver->get_multi_hw_info(hapd->drv_priv, num_multi_hws);
}


int hostapd_drv_add_pmkid(struct hostapd_data *hapd,
			  struct wpa_pmkid_params *params)
{
	if (!hapd->driver || !hapd->driver->add_pmkid || !hapd->drv_priv)
		return 0;
	return hapd->driver->add_pmkid(hapd->drv_priv, params);
}


int hostapd_add_pmkid(struct hostapd_data *hapd, const u8 *bssid, const u8 *pmk,
		      size_t pmk_len, const u8 *pmkid, int akmp)
{
	struct wpa_pmkid_params params;

	os_memset(&params, 0, sizeof(params));
	params.bssid = bssid;
	params.pmkid = pmkid;
	params.pmk = pmk;
	params.pmk_len = pmk_len;

	return hostapd_drv_add_pmkid(hapd, &params);
}


static int hostapd_drv_remove_pmkid(struct hostapd_data *hapd,
				    struct wpa_pmkid_params *params)
{
	if (!hapd->driver || !hapd->driver->remove_pmkid || !hapd->drv_priv)
		return 0;
	return hapd->driver->remove_pmkid(hapd->drv_priv, params);
}


int hostapd_remove_pmkid(struct hostapd_data *hapd, const u8 *sta_addr,
			 const u8 *pmkid)
{
	struct wpa_pmkid_params params;

	os_memset(&params, 0, sizeof(params));
	params.bssid = sta_addr;
	params.pmkid = pmkid;

	return hostapd_drv_remove_pmkid(hapd, &params);
}

#ifdef CONFIG_IEEE80211BE
int hostapd_drv_set_advertised_ttlm_params(struct hostapd_data *hapd,
					   const struct drv_adv_ttlm_params *up_ttlm_params,
					   const struct drv_adv_ttlm_params *est_ttlm_params,
					   bool send_default_mapping)
{
	if (!hapd->driver || !hapd->drv_priv ||
	    !hapd->driver->set_advertised_ttlm_params)
		return 0;

	return hapd->driver->set_advertised_ttlm_params(hapd->drv_priv,
							up_ttlm_params,
							est_ttlm_params,
							send_default_mapping);
}
#endif /* CONFIG_IEEE80211BE */


int hostapd_drv_set_qos(struct hostapd_data *hapd, struct qm_req_data *qm_req,
			struct qm_resp_data *qm_resp)
{
	if (!hapd->driver || !hapd->drv_priv)
		return -1;

	return hapd->driver->set_qos(hapd->drv_priv, qm_req, qm_resp);
}

#ifdef CONFIG_IEEE80211BN
int hostapd_drv_critical_update(struct hostapd_data *hapd, u8 link_id,
				u32 cu_type, const u8 *elems, size_t elems_len)
{
	if (!hapd->driver || !hapd->drv_priv ||
	    !hapd->driver->critical_update)
		return -EOPNOTSUPP;

	return hapd->driver->critical_update(hapd->drv_priv, link_id,
					     cu_type, elems, elems_len);
}
#endif /* CONFIG_IEEE80211BN */

int hostapd_drv_set_smd_ctx(struct hostapd_data *hapd, struct sta_info *sta,
			    const struct sta_smd_ctx_info *ctx)
{
	if (!hapd->driver || !hapd->drv_priv || !hapd->driver->set_smd_ctx)
		return 0;

	if (!sta || !ctx)
		return -1;

	return hapd->driver->set_smd_ctx(hapd->drv_priv, sta->addr, ctx);
}

int hostapd_drv_get_smd_ctx(struct hostapd_data *hapd, const u8 *sta_addr,
			    u8 type, u8 valid_ctx_bitmap, u8 dl_tid_bitmap,
			    u8 ul_tid_bitmap, struct sta_smd_ctx_info **out_ctx)
{
	if (!hapd->driver || !hapd->drv_priv || !hapd->driver->get_smd_ctx)
		return -1;

	if (!sta_addr)
		return -1;

	return hapd->driver->get_smd_ctx(hapd->drv_priv, sta_addr, type,
					 valid_ctx_bitmap, dl_tid_bitmap,
					 ul_tid_bitmap, out_ctx);
}

#ifdef CONFIG_QCN_EXTN
int hostapd_drv_set_muedca_mode(struct hostapd_data *hapd, int mode, int radio_idx)
{
	if (!hapd->driver || !hapd->driver->set_muedca_mode)
		return -1;

	return hapd->driver->set_muedca_mode(hapd->drv_priv, mode, radio_idx);
}
#endif /* CONFIG_QCN_EXTN */

int hostapd_drv_set_rtt_responder_role(struct hostapd_data *hapd, int role)
{
	int link_id = -1;

	if (!hapd->driver || !hapd->driver->notify_iface_state)
		return -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	return hapd->driver->notify_iface_state(
		hapd->drv_priv, OUI_QCA,
		QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION,
		QCA_WLAN_VENDOR_WIFI_PARAM_RTT_RESPONDER_ROLE,
		(u32)role, link_id, hapd->conf->iface);
}
