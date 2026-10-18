/*
 * hostapd / IEEE 802.11 Management: Beacon and Probe Request/Response
 * Copyright (c) 2002-2004, Instant802 Networks, Inc.
 * Copyright (c) 2005-2006, Devicescape Software, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef BEACON_H
#define BEACON_H

struct ieee80211_mgmt;

struct probe_resp_params {
	const struct ieee80211_mgmt *req;
	bool is_p2p;
	bool force_bcast_resp_oce_non6ghz;

	/* Generated IEs will be included inside an ML element */
	struct hostapd_data *mld_ap;
	struct mld_info *mld_info;

	struct ieee80211_mgmt *resp;
	size_t resp_len;
	u8 *csa_pos;
	u8 *ecsa_pos;
	const u8 *known_bss;
	u8 known_bss_len;

#ifdef CONFIG_IEEE80211AX
	u8 *cca_pos;
#endif /* CONFIG_IEEE80211AX */

	bool is_ml_probe;
};

int ieee802_11_build_nontx_bss_probe_params(struct hostapd_data *hapd,
					    struct probe_resp_params *ntx_probe_params);
void handle_probe_req(struct hostapd_data *hapd,
		      const struct ieee80211_mgmt *mgmt, size_t len,
		      const struct hostapd_frame_info *fi);
void ieee802_11_set_beacon_per_bss_only(struct hostapd_data *hapd);
int ieee802_11_set_beacon(struct hostapd_data *hapd);
int ieee802_11_set_beacons(struct hostapd_iface *iface);
int ieee802_11_update_beacons(struct hostapd_iface *iface);
void ieee802_11_update_beacon_mbssid(struct hostapd_data *hapd);
int ieee802_11_build_ap_params(struct hostapd_data *hapd,
			       struct wpa_driver_ap_params *params);
void ieee802_11_free_ap_params(struct wpa_driver_ap_params *params);
void sta_track_add(struct hostapd_iface *iface, const u8 *addr, int ssi_signal);
void sta_track_del(struct hostapd_sta_info *info);
void sta_track_expire(struct hostapd_iface *iface, int force);
struct hostapd_data *
sta_track_seen_on(struct hostapd_iface *iface, const u8 *addr,
		  const char *ifname);
void sta_track_claim_taxonomy_info(struct hostapd_iface *iface, const u8 *addr,
				   struct wpabuf **probe_ie_taxonomy);

const u8 * hostapd_wpa_ie(struct hostapd_data *hapd, u8 eid);

/**
 * hostapd_eid_add_max_cs_time - Add Max Channel Switch Time element.
 * @eid: Buffer position where the element should be written.
 * @switch_time: Max channel switch time in TU.
 *
 * Return: Updated buffer position after the element.
 */
u8 * hostapd_eid_add_max_cs_time(u8 *eid, u32 switch_time);

u8 * hostapd_unsol_bcast_probe_resp(struct hostapd_data *hapd,
				    struct unsol_bcast_probe_resp *ubpr);

int ieee802_11_build_nontx_bss_params(struct hostapd_data *hapd,
				      struct wpa_driver_ap_params *params);
u8 ieee802_11_erp_info(struct hostapd_data *hapd);

#endif /* BEACON_H */
