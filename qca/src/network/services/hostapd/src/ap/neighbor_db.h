/*
 * hostapd / Neighboring APs DB
 * Copyright(c) 2013 - 2016 Intel Mobile Communications GmbH.
 * Copyright(c) 2011 - 2016 Intel Corporation. All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef NEIGHBOR_DB_H
#define NEIGHBOR_DB_H

struct hostapd_neighbor_entry *
hostapd_neighbor_get(struct hostapd_data *hapd, const u8 *bssid,
		     const struct wpa_ssid_value *ssid);
int hostapd_neighbor_show(struct hostapd_data *hapd, char *buf, size_t buflen);
int hostapd_neighbor_set(struct hostapd_data *hapd, const u8 *bssid,
			 const struct wpa_ssid_value *ssid,
			 const struct wpabuf *nr, const struct wpabuf *lci,
			 const struct wpabuf *civic, int stationary,
			 u8 bss_parameters);
void hostapd_neighbor_set_own_report(struct hostapd_data *hapd);
void hostapd_neighbor_set_own_report_for(struct hostapd_data *dest,
					 struct hostapd_data *src);
int hostapd_prepare_neighbor_buf(struct hostapd_data *hapd,
				 const u8 *bssid, struct wpabuf *nrbuf);
int hostapd_neighbor_sync_own_report(struct hostapd_data *hapd);
int hostapd_neighbor_remove(struct hostapd_data *hapd, const u8 *bssid,
			    const struct wpa_ssid_value *ssid);
void hostapd_free_neighbor_db(struct hostapd_data *hapd);
int hostapd_add_candidate_own(struct hostapd_data *hapd, int pref,
			      u8 *links, u8 num_links,
			      u8 *nei_rep, size_t nei_rep_len);
int hostapd_neighbor_set_ifaces_scan_report(struct hostapd_data *hapd,
					    const struct wpa_ssid_value *ssid,
					    u32 bands);

void hostapd_oce_survey_timer(void *eloop_ctx, void *timeout_ctx);
void hostapd_oce_survey_timer_start(struct hostapd_iface *iface);
void hostapd_oce_survey_timer_cancel(struct hostapd_iface *iface);
/**
 * Get all neighbor entries matching SMD_ID
 * Returns: Number of entries found
 */
int hostapd_neighbor_get_all_by_smd_id(struct hostapd_data *hapd,
		const u8 *smd_id,
		struct hostapd_neighbor_entry **entries,
		size_t max_entries);

/**
 * Get all neighbor entries matching MLD address
 * Returns: Number of entries found
 */
int hostapd_neighbor_get_all_by_mld_addr(struct hostapd_data *hapd,
		const u8 *mld_addr,
		struct hostapd_neighbor_entry **entries,
		size_t max_entries);

/**
 * Count total entries in database
 * Returns: Number of entries
 */
int hostapd_neighbor_count(struct hostapd_data *hapd);
int hostapd_neighbor_set_mld(struct hostapd_data *hapd, const u8 *bssid,
		const struct wpa_ssid_value *ssid,
		const struct wpabuf *nr,
		const u8 *mld_addr,
		const u8 *smd_id,
		const struct wpabuf *lci,
		const struct wpabuf *civic,
                             int stationary,
                             u8 bss_parameters);

void hostapd_free_neighbor_db_nonself_scoped(struct hostapd_data *hapd,
					     u8 has_smd,
					     u8 has_mld,
					     u8 has_bssid,
					     const u8 *addr);
/**
 * Count total entries in database
 * Returns: Number of entries
 */
int hostapd_neighbor_count(struct hostapd_data *hapd);
int hostapd_neighbor_set_mld(struct hostapd_data *hapd, const u8 *bssid,
		const struct wpa_ssid_value *ssid,
		const struct wpabuf *nr,
		const u8 *mld_addr,
		const u8 *smd_id,
		const struct wpabuf *lci,
		const struct wpabuf *civic,
		int stationary,
		u8 bss_parameters);
#endif /* NEIGHBOR_DB_H */
