/*
 * hostapd / Neighboring APs DB
 * Copyright(c) 2013 - 2016 Intel Mobile Communications GmbH.
 * Copyright(c) 2011 - 2016 Intel Corporation. All rights reserved.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/crc32.h"
#include "hostapd.h"
#include "ieee802_11.h"
#include "neighbor_db.h"
#include "ap_drv_ops.h"
#include "beacon.h"
#include "utils/eloop.h"

static void hostapd_neighbor_clear_entry(struct hostapd_neighbor_entry *nr);
static struct hostapd_neighbor_entry *hostapd_neighbor_add(struct hostapd_data *hapd);
static void hostapd_neighbor_free(struct hostapd_neighbor_entry *nr);

struct hostapd_neighbor_entry *
hostapd_neighbor_get(struct hostapd_data *hapd, const u8 *bssid,
		     const struct wpa_ssid_value *ssid)
{
	struct hostapd_neighbor_entry *nr;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		if (ether_addr_equal(bssid, nr->bssid) &&
		    (!ssid ||
		     (ssid->ssid_len == nr->ssid.ssid_len &&
		      os_memcmp(ssid->ssid, nr->ssid.ssid,
				ssid->ssid_len) == 0)))
			return nr;
	}
	return NULL;
}

/**
 * hostapd_neighbor_set_mld - Add/update neighbor entry with MLD info
 * @hapd: hostapd data
 * @bssid: BSSID of the neighbor
 * @ssid: SSID of the neighbor
 * @nr: Neighbor Report element
 * @mld_addr: MLD MAC address
 * @smd_id: SMD ID
 * @lci: LCI subelement or NULL
 * @civic: Civic location subelement or NULL
 * @stationary: Stationary flag
 * @bss_parameters: BSS parameters
 * Returns: 0 on success, -1 on failure
 */
int hostapd_neighbor_set_mld(struct hostapd_data *hapd, const u8 *bssid,
		const struct wpa_ssid_value *ssid,
		const struct wpabuf *nr,
		const u8 *mld_addr,
		const u8 *smd_id,
		const struct wpabuf *lci,
		const struct wpabuf *civic,
		int stationary,
		u8 bss_parameters)
{
	struct hostapd_neighbor_entry *entry;

	wpa_printf(MSG_DEBUG,
		   "NeighborDB set_mld: req bssid=" MACSTR " mld=" MACSTR
		   " has_smd=%u smd=" MACSTR " nr_len=%u",
		   MAC2STR(bssid),
		   MAC2STR(mld_addr ? mld_addr : (const u8 *) "\0\0\0\0\0\0"),
		   smd_id && !is_zero_ether_addr(smd_id),
		   MAC2STR((smd_id && !is_zero_ether_addr(smd_id)) ?
			   smd_id : (const u8 *) "\0\0\0\0\0\0"),
		   nr ? (unsigned int) wpabuf_len(nr) : 0);

	/* Try to find existing entry by BSSID */
	entry = hostapd_neighbor_get(hapd, bssid, ssid);
	if (!entry) {
		entry = hostapd_neighbor_add(hapd);
		if(!entry)
			return -1;
	} else
		hostapd_neighbor_clear_entry(entry);

	wpa_printf(MSG_DEBUG,
		   "NeighborDB set_mld: using %s entry for bssid=" MACSTR,
		   hostapd_neighbor_get(hapd, bssid, ssid) ? "existing" : "new",
		   MAC2STR(bssid));

	/* Set basic fields */
	os_memcpy(entry->bssid, bssid, ETH_ALEN);
	os_memcpy(&entry->ssid, ssid, sizeof(entry->ssid));
	entry->short_ssid = ieee80211_crc32(ssid->ssid, ssid->ssid_len);

	/* Set MLD fields */
	if (mld_addr)
		os_memcpy(entry->mld_addr, mld_addr, ETH_ALEN);
	else
		os_memset(entry->mld_addr, 0, ETH_ALEN);

	if (smd_id && !is_zero_ether_addr(smd_id))
		os_memcpy(entry->smd_id, smd_id, ETH_ALEN);
	else
		os_memset(entry->smd_id, 0, ETH_ALEN);

	/* Set neighbor report */
	entry->nr = wpabuf_dup(nr);
	if (!entry->nr)
		goto fail;

	/* Set optional LCI */
	if (lci && wpabuf_len(lci)) {
		entry->lci = wpabuf_dup(lci);
		if (!entry->lci || os_get_time(&entry->lci_date))
			goto fail;
	}

	/* Set optional civic location */
	if (civic && wpabuf_len(civic)) {
		entry->civic = wpabuf_dup(civic);
		if (!entry->civic)
			goto fail;
	}

	entry->stationary = stationary;
	entry->bss_parameters = bss_parameters;

	wpa_printf(MSG_DEBUG,
			"NeighborDB: Added/updated entry BSSID=" MACSTR
			" MLD=" MACSTR " has_smd=%u SMD=" MACSTR,
			MAC2STR(entry->bssid), MAC2STR(entry->mld_addr),
			!is_zero_ether_addr(entry->smd_id), MAC2STR(entry->smd_id));

	return 0;

fail:
	hostapd_neighbor_remove(hapd, bssid, ssid);
	return -1;
}

void hostapd_free_neighbor_db_nonself_scoped(struct hostapd_data *hapd,
					     u8 has_smd,
					     u8 has_mld,
					     u8 has_bssid,
					     const u8 *addr)
{
	struct hostapd_neighbor_entry *nr, *prev;
	int count = 0;

	if (!hapd)
		return;

	if (has_smd) {
		if (!addr || is_zero_ether_addr(addr)) {
			wpa_printf(MSG_WARNING,
				   "NeighborDB: invalid SMD for non-self clear");
			return;
		}

		dl_list_for_each_safe(nr, prev, &hapd->nr_db,
				      struct hostapd_neighbor_entry, list) {
			if (nr->self_entry)
				continue;
			if (is_zero_ether_addr(nr->smd_id))
				continue;
			if (!ether_addr_equal(nr->smd_id, addr))
				continue;
			hostapd_neighbor_free(nr);
			count++;
		}

		wpa_printf(MSG_INFO,
			   "NeighborDB: Cleared %d non-self entries by SMD=" MACSTR,
			   count, MAC2STR(addr));
		return;
	}

	if (has_mld) {
		if (!addr || is_zero_ether_addr(addr)) {
			wpa_printf(MSG_WARNING,
				   "NeighborDB: invalid MLD for non-self clear");
			return;
		}

		dl_list_for_each_safe(nr, prev, &hapd->nr_db,
				      struct hostapd_neighbor_entry, list) {
			if (nr->self_entry)
				continue;
			if (is_zero_ether_addr(nr->mld_addr))
				continue;
			if (!ether_addr_equal(nr->mld_addr, addr))
				continue;
			hostapd_neighbor_free(nr);
			count++;
		}

		wpa_printf(MSG_INFO,
			   "NeighborDB: Cleared %d non-self entries by MLD=" MACSTR,
			   count, MAC2STR(addr));
		return;
	}

	if (has_bssid) {
		if (!addr || is_zero_ether_addr(addr)) {
			wpa_printf(MSG_WARNING,
				   "NeighborDB: invalid BSSID for non-self clear");
			return;
		}

		dl_list_for_each_safe(nr, prev, &hapd->nr_db,
				      struct hostapd_neighbor_entry, list) {
			if (nr->self_entry)
				continue;
			if (!ether_addr_equal(nr->bssid, addr))
				continue;
			hostapd_neighbor_free(nr);
			count++;
		}

		wpa_printf(MSG_INFO,
			   "NeighborDB: Cleared %d non-self entries by BSSID=" MACSTR,
			   count, MAC2STR(addr));
		return;
	}

	dl_list_for_each_safe(nr, prev, &hapd->nr_db,
			      struct hostapd_neighbor_entry, list) {
		if (nr->self_entry)
			continue;
		hostapd_neighbor_free(nr);
		count++;
	}

	wpa_printf(MSG_INFO,
		   "NeighborDB: Cleared %d non-self entries (global)", count);
}

/**
 * hostapd_neighbor_get_all_by_smd_id - Get all neighbor entries by SMD ID
 * @hapd: hostapd data
 * @smd_id: SMD ID (6 bytes)
 * @entries: Array to store pointers to matching entries
 * @max_entries: Maximum number of entries the array can hold
 * Returns: Number of entries found (may be more than max_entries)
 */
int hostapd_neighbor_get_all_by_smd_id(struct hostapd_data *hapd,
		const u8 *smd_id,
		struct hostapd_neighbor_entry **entries,
		size_t max_entries)
{
	struct hostapd_neighbor_entry *nr;
	int count = 0;

	if (!smd_id || !entries)
		return 0;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry, list) {
		if (!is_zero_ether_addr(nr->smd_id) &&
				ether_addr_equal(smd_id, nr->smd_id)) {
			wpa_printf(MSG_DEBUG,
					"NeighborDB by_smd: match bssid=" MACSTR " mld=" MACSTR,
					MAC2STR(nr->bssid), MAC2STR(nr->mld_addr));
			if (count < (int)max_entries)
				entries[count] = nr;
			count++;
		}
	}

	wpa_printf(MSG_DEBUG,
			"NeighborDB: Found %d entries with SMD_ID=" MACSTR,
			count, MAC2STR(smd_id));

	return count;
}

/**
 * hostapd_neighbor_get_all_by_mld_addr - Get all neighbor entries by MLD address
 * @hapd: hostapd data
 * @mld_addr: MLD MAC address (6 bytes)
 * @entries: Array to store pointers to matching entries
 * @max_entries: Maximum number of entries the array can hold
 * Returns: Number of entries found (may be more than max_entries)
 */
int hostapd_neighbor_get_all_by_mld_addr(struct hostapd_data *hapd,
		const u8 *mld_addr,
		struct hostapd_neighbor_entry **entries,
		size_t max_entries)
{
	struct hostapd_neighbor_entry *nr;
	int count = 0;

	if (!mld_addr || !entries)
		return 0;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry, list) {
		if (!is_zero_ether_addr(nr->mld_addr) &&
				ether_addr_equal(mld_addr, nr->mld_addr)) {
			wpa_printf(MSG_DEBUG,
					"NeighborDB by_mld: match bssid=" MACSTR " has_smd=%u smd=" MACSTR,
					MAC2STR(nr->bssid),
					!is_zero_ether_addr(nr->smd_id),
					MAC2STR(nr->smd_id));
			if (count < (int)max_entries)
				entries[count] = nr;
			count++;
		}
	}

	wpa_printf(MSG_DEBUG,
			"NeighborDB: Found %d entries with MLD_ADDR=" MACSTR,
			count, MAC2STR(mld_addr));

	return count;
}

/**
 * hostapd_neighbor_count - Count total entries in neighbor database
 * @hapd: hostapd data
 * Returns: Number of entries in the database
 */
int hostapd_neighbor_count(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr;
	int count = 0;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry, list) {
		count++;
	}

	return count;
}

int hostapd_neighbor_show(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	struct hostapd_neighbor_entry *nr;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		int ret;
		char nrie[2 * 255 + 1];
		char lci[2 * 255 + 1];
		char civic[2 * 255 + 1];
		char ssid[SSID_MAX_LEN * 2 + 1];
		char smd[sizeof(" smd_id=xx:xx:xx:xx:xx:xx")];

		ssid[0] = '\0';
		wpa_snprintf_hex(ssid, sizeof(ssid), nr->ssid.ssid,
				 nr->ssid.ssid_len);

		nrie[0] = '\0';
		if (nr->nr)
			wpa_snprintf_hex(nrie, sizeof(nrie),
					 wpabuf_head(nr->nr),
					 wpabuf_len(nr->nr));

		lci[0] = '\0';
		if (nr->lci)
			wpa_snprintf_hex(lci, sizeof(lci),
					 wpabuf_head(nr->lci),
					 wpabuf_len(nr->lci));

		civic[0] = '\0';
		if (nr->civic)
			wpa_snprintf_hex(civic, sizeof(civic),
					 wpabuf_head(nr->civic),
					 wpabuf_len(nr->civic));

		smd[0] = '\0';
		if (!is_zero_ether_addr(nr->smd_id))
			os_snprintf(smd, sizeof(smd), " smd_id=" MACSTR,
				    MAC2STR(nr->smd_id));

		ret = os_snprintf(pos, end - pos, MACSTR
				  " ssid=%s%s%s%s%s%s%s%s%s\n",
				  MAC2STR(nr->bssid),
				  ssid,
				  smd,
				  nr->nr ? " nr=" : "", nrie,
				  nr->lci ? " lci=" : "", lci,
				  nr->civic ? " civic=" : "", civic,
				  nr->stationary ? " stat" : "");
		if (os_snprintf_error(end - pos, ret))
			break;
		pos += ret;
	}

	return pos - buf;
}


static void hostapd_neighbor_clear_entry(struct hostapd_neighbor_entry *nr)
{
	wpabuf_free(nr->nr);
	nr->nr = NULL;
	wpabuf_free(nr->lci);
	nr->lci = NULL;
	wpabuf_free(nr->civic);
	nr->civic = NULL;
	os_memset(nr->bssid, 0, sizeof(nr->bssid));
	os_memset(&nr->ssid, 0, sizeof(nr->ssid));
	os_memset(nr->mld_addr, 0, sizeof(nr->mld_addr));
	os_memset(nr->smd_id, 0, sizeof(nr->smd_id));
	nr->self_entry = 0;
	os_memset(&nr->lci_date, 0, sizeof(nr->lci_date));
	nr->stationary = 0;
	nr->short_ssid = 0;
	nr->bss_parameters = 0;
}


static struct hostapd_neighbor_entry *
hostapd_neighbor_add(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr;

	nr = os_zalloc(sizeof(struct hostapd_neighbor_entry));
	if (!nr)
		return NULL;

	dl_list_add(&hapd->nr_db, &nr->list);

	return nr;
}


int hostapd_neighbor_set(struct hostapd_data *hapd, const u8 *bssid,
			 const struct wpa_ssid_value *ssid,
			 const struct wpabuf *nr, const struct wpabuf *lci,
			 const struct wpabuf *civic, int stationary,
			 u8 bss_parameters)
{
	struct hostapd_neighbor_entry *entry;

	entry = hostapd_neighbor_get(hapd, bssid, ssid);
	if (!entry)
		entry = hostapd_neighbor_add(hapd);
	if (!entry)
		return -1;

	hostapd_neighbor_clear_entry(entry);

	os_memcpy(entry->bssid, bssid, ETH_ALEN);
	os_memcpy(&entry->ssid, ssid, sizeof(entry->ssid));
	entry->short_ssid = ieee80211_crc32(ssid->ssid, ssid->ssid_len);

	entry->nr = wpabuf_dup(nr);
	if (!entry->nr)
		goto fail;

	if (lci && wpabuf_len(lci)) {
		entry->lci = wpabuf_dup(lci);
		if (!entry->lci || os_get_time(&entry->lci_date))
			goto fail;
	}

	if (civic && wpabuf_len(civic)) {
		entry->civic = wpabuf_dup(civic);
		if (!entry->civic)
			goto fail;
	}

	entry->stationary = stationary;
	entry->bss_parameters = bss_parameters;

	return 0;

fail:
	hostapd_neighbor_remove(hapd, bssid, ssid);
	return -1;
}


static void hostapd_neighbor_free(struct hostapd_neighbor_entry *nr)
{
	hostapd_neighbor_clear_entry(nr);
	dl_list_del(&nr->list);
	os_free(nr);
}

int hostapd_prepare_neighbor_buf(struct hostapd_data *hapd,
				 const u8 *bssid, struct wpabuf *nrbuf)
{
	struct hostapd_neighbor_entry *nr;

	nr = hostapd_neighbor_get(hapd, bssid, NULL);
	if (!nr)
		return -1;

	if (wpabuf_tailroom(nrbuf) < wpabuf_len(nr->nr)) {
		wpa_printf(MSG_ERROR,
			   "Invalid buf size for Neighbor Report\n");
		return -1;
	}

	wpabuf_put_buf(nrbuf, nr->nr);
	return 0;
}

int hostapd_neighbor_remove(struct hostapd_data *hapd, const u8 *bssid,
			    const struct wpa_ssid_value *ssid)
{
	struct hostapd_neighbor_entry *nr;

	nr = hostapd_neighbor_get(hapd, bssid, ssid);
	if (!nr)
		return -1;

	hostapd_neighbor_free(nr);

	return 0;
}


void hostapd_free_neighbor_db(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr, *prev;

	dl_list_for_each_safe(nr, prev, &hapd->nr_db,
			      struct hostapd_neighbor_entry, list) {
		hostapd_neighbor_free(nr);
	}
}

#ifdef NEED_AP_MLME
static enum nr_chan_width
hostapd_get_nr_chan_width(struct hostapd_data *hapd,
			  int ht, int vht, int he,
			  enum oper_chan_width oper_chwidth,
			  int secondary_channel)
{
	if (!ht && !vht && !he)
		return NR_CHAN_WIDTH_20;
	if (!secondary_channel)
		return NR_CHAN_WIDTH_20;
	if ((!vht && !he) || oper_chwidth == CONF_OPER_CHWIDTH_USE_HT)
		return NR_CHAN_WIDTH_40;
	if (oper_chwidth == CONF_OPER_CHWIDTH_80MHZ)
		return NR_CHAN_WIDTH_80;
	if (oper_chwidth == CONF_OPER_CHWIDTH_160MHZ)
		return NR_CHAN_WIDTH_160;
	if (oper_chwidth == CONF_OPER_CHWIDTH_80P80MHZ)
		return NR_CHAN_WIDTH_80P80;
	return NR_CHAN_WIDTH_20;
}
#endif /* NEED_AP_MLME */

static void hostapd_neighbor_add_op_capab_subelements(struct hostapd_data *hapd,
						      struct wpabuf *nr,
						      int ht, int vht, int he,
						      bool eht)
{
	u8 buf[512];
	u8 *pos, *end;
	size_t len;

	if (ht) {
		pos = buf;
		end = hostapd_eid_ht_capabilities(hapd, pos);
		len = end - pos;
		if (len > 2) { /* EID + Len */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_HT_CAPAB);
			wpabuf_put_u8(nr, len - 2);
			wpabuf_put_data(nr, pos + 2, len - 2);
		}

		pos = buf;
		end = hostapd_eid_ht_operation(hapd, pos);
		len = end - pos;
		if (len > 2) { /* EID + Len */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_HT_OPER);
			wpabuf_put_u8(nr, len - 2);
			wpabuf_put_data(nr, pos + 2, len - 2);
		}
	}

	if (vht) {
		pos = buf;
		end = hostapd_eid_vht_capabilities(hapd, pos, 0);
		len = end - pos;
		if (len > 2) { /* EID + Len */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_VHT_CAPAB);
			wpabuf_put_u8(nr, len - 2);
			wpabuf_put_data(nr, pos + 2, len - 2);
		}

		pos = buf;
		end = hostapd_eid_vht_operation(hapd, pos);
		len = end - pos;
		if (len > 2) { /* EID + Len */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_VHT_OPER);
			wpabuf_put_u8(nr, len - 2);
			wpabuf_put_data(nr, pos + 2, len - 2);
		}
	}

	if (he) {
		pos = buf;
		end = hostapd_eid_he_capab(hapd, pos, IEEE80211_MODE_AP);
		len = end - pos;
		if (len > 3) { /* EID + Len + ExtID */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_HE_CAPAB);
			wpabuf_put_u8(nr, len - 3);
			wpabuf_put_data(nr, pos + 3, len - 3);
		}

		pos = buf;
		end = hostapd_eid_he_operation(hapd, pos);
		len = end - pos;
		if (len > 3) { /* EID + Len + ExtID */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_HE_OPER);
			wpabuf_put_u8(nr, len - 3);
			wpabuf_put_data(nr, pos + 3, len - 3);
		}
	}

	if (eht) {
		pos = buf;
		end = hostapd_eid_eht_capab(hapd, pos, IEEE80211_MODE_AP);
		len = end - pos;
		if (len > 3) { /* EID + Len + ExtID */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_EHT_CAPAB);
			wpabuf_put_u8(nr, len - 3);
			wpabuf_put_data(nr, pos + 3, len - 3);
		}

		pos = buf;
		end = hostapd_eid_eht_operation(hapd, pos);
		len = end - pos;
		if (len > 3) { /* EID + Len + ExtID */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_EHT_OPER);
			wpabuf_put_u8(nr, len - 3);
			wpabuf_put_data(nr, pos + 3, len - 3);
		}
	}
}

#ifdef CONFIG_IEEE80211BN
/**
 * hostapd_neighbor_add_11bn_subelements - Add 11BN mandatory subelements
 * @hapd: hostapd data
 * @nr: Neighbor report buffer
 *
 * Adds mandatory neighbor report subelements as required by IEEE 802.11bn
 */
static void hostapd_neighbor_add_11bn_subelements(struct hostapd_data *hapd,
						  struct wpabuf *nr)
{
	u8 buf[256];
	u8 *pos, *end;
	size_t len;

	if (!hapd->iconf->ieee80211bn)
		return;

	/* BSS Load */
	if (hapd->conf->bss_load_update_period) {
		wpabuf_put_u8(nr, WNM_NEIGHBOR_BSS_LOAD);
		wpabuf_put_u8(nr, 5);
		wpabuf_put_le16(nr, hapd->num_sta);
		wpabuf_put_u8(nr, hapd->iface->channel_utilization);
		wpabuf_put_le16(nr, 0); /* Available Admission Capacity */
	}

	/* UHR Operation */
	pos = buf;
	end = hostapd_eid_uhr_operation(hapd, pos, false);
	len = end - pos;
	if (len > 3) { /* EID + Len + ExtID */
		wpabuf_put_u8(nr, WNM_NEIGHBOR_UHR_OPER);
		wpabuf_put_u8(nr, len - 3);
		wpabuf_put_data(nr, pos + 3, len - 3);
	}

	/* UHR Capabilities */
	pos = buf;
	end = hostapd_eid_uhr_capab(hapd, pos, IEEE80211_MODE_AP);
	len = end - pos;
	if (len > 3) { /* EID + Len + ExtID */
		wpabuf_put_u8(nr, WNM_NEIGHBOR_UHR_CAPAB);
		wpabuf_put_u8(nr, len - 3);
		wpabuf_put_data(nr, pos + 3, len - 3);
	}

	/* Supported Rates */
	pos = buf;
	end = hostapd_eid_supp_rates(hapd, pos);
	len = end - pos;
	if (len > 2) { /* EID + Len */
		wpabuf_put_u8(nr, WNM_NEIGHBOR_SUPP_RATES);
		wpabuf_put_u8(nr, len - 2);
		wpabuf_put_data(nr, pos + 2, len - 2);
	}

	/* SMD Information */
	if (hapd->conf->smd.enabled) {
		pos = buf;
		end = hostapd_eid_smd_ie(hapd, pos);
		len = end - pos;
		if (len > 3) { /* EID + Len + ExtID */
			wpabuf_put_u8(nr, WNM_NEIGHBOR_SMD_INFO);
			wpabuf_put_u8(nr, len - 3);
			wpabuf_put_data(nr, pos + 3, len - 3);
		}
	}

	/* Basic Multi-Link (MLD only) */
	if (hostapd_is_multiple_link_mld(hapd)) {
		wpabuf_put_u8(nr, WNM_NEIGHBOR_MULTI_LINK);
		wpabuf_put_u8(nr, 9);
		wpabuf_put_le16(nr, MULTI_LINK_CONTROL_TYPE_BASIC);
		wpabuf_put_u8(nr, 6); /* Common Info Length */
		wpabuf_put_data(nr, hapd->mld->mld_addr, ETH_ALEN);
	}
}
#endif /* CONFIG_IEEE80211BN */

/*
 * hostapd_neighbor_set_own_report_for - Build an authoritative NR entry from
 * src's BSSID, frequency, and capabilities and insert it into dest's NR DB.
 *
 * When dest == src this is the normal "own report" path.  When dest != src
 * the entry describes a co-located radio (e.g. 5 GHz inserted into the 2.4 GHz
 * NR DB) so that STAs can discover and roam to co-located BSSes.
 */
void hostapd_neighbor_set_own_report_for(struct hostapd_data *dest,
					 struct hostapd_data *src)
{
#ifdef NEED_AP_MLME
	u16 capab = hostapd_own_capab_info(src);
	int ht = hostapd_is_ht_enabled(src);
	int vht = hostapd_is_vht_enabled(src);
	int he = hostapd_is_he_enabled(src);
	bool eht = he && hostapd_is_eht_enabled(src);
	struct wpa_ssid_value ssid;
	const u8 *own_mld_addr = NULL;
	const u8 *own_smd_id = NULL;
	u8 channel, op_class;
	u8 center_freq1_idx = 0, center_freq2_idx = 0;
	enum oper_chan_width oper_chwidth;
	enum nr_chan_width width;
	int secondary_channel;
	u32 bssid_info;
	struct wpabuf *nr;
	struct hostapd_neighbor_entry *own_entry;

	/*
	 * The dest radio must advertise RRM Neighbor Report capability;
	 * it owns the NR DB that will be sent to STAs.
	 */
	if (!(dest->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT))
		return;

	/*
	 * For cross-radio entries (dest != src): skip hidden BSSes and BSSes
	 * with a different SSID.  A STA associated to dest's SSID should only
	 * be steered to a co-located BSS advertising the same SSID.
	 * Use ignore_broadcast_ssid rather than raw ssid_len so that mode-2
	 * (zero-length SSID in beacon, responds to directed probes) is also
	 * excluded correctly.
	 */
	if (dest != src) {
		if (src->conf->ignore_broadcast_ssid)
			return;
		if (src->conf->ssid.ssid_len != dest->conf->ssid.ssid_len ||
		    os_memcmp(src->conf->ssid.ssid, dest->conf->ssid.ssid,
			      dest->conf->ssid.ssid_len) != 0)
			return;
	}

	bssid_info = 3; /* AP is reachable */
	bssid_info |= NEI_REP_BSSID_INFO_SECURITY; /* "same as the AP" */
	bssid_info |= NEI_REP_BSSID_INFO_KEY_SCOPE; /* "same as the AP" */

	if (capab & WLAN_CAPABILITY_SPECTRUM_MGMT)
		bssid_info |= NEI_REP_BSSID_INFO_SPECTRUM_MGMT;

	bssid_info |= NEI_REP_BSSID_INFO_RM; /* RRM is supported */

	if (src->conf->wmm_enabled) {
		bssid_info |= NEI_REP_BSSID_INFO_QOS;

		if (src->conf->wmm_uapsd &&
		    (src->iface->drv_flags & WPA_DRIVER_FLAGS_AP_UAPSD))
			bssid_info |= NEI_REP_BSSID_INFO_APSD;
	}

	if (ht) {
		bssid_info |= NEI_REP_BSSID_INFO_HT;
		if (vht)
			bssid_info |= NEI_REP_BSSID_INFO_VHT;
	}

	if (he)
		bssid_info |= NEI_REP_BSSID_INFO_HE;
	if (eht)
		bssid_info |= NEI_REP_BSSID_INFO_EHT;

	/* Signal to the STA that this entry describes a co-located AP */
	if (dest != src)
		bssid_info |= NEI_REP_BSSID_INFO_COLOCATED_AP;

#ifdef CONFIG_IEEE80211BN
	/* This AP is SMD-enabled, so it is by definition in the same SMD as
	 * itself. Set SAME_SMD in the own neighbor report entry sent to STAs. */
	if (src->conf->smd.enabled)
		bssid_info |= NEI_REP_BSSID_INFO_SAME_SMD;

	/* Set UHR bit if this is a UHR AP */
	if (src->iconf->ieee80211bn)
		bssid_info |= NEI_REP_BSSID_INFO_UHR;
#endif /* CONFIG_IEEE80211BN */
	/* TODO: Set NEI_REP_BSSID_INFO_MOBILITY_DOMAIN if MDE is set */

	hostapd_get_oper_chan_info_of_bss(src, &oper_chwidth,
					  &center_freq1_idx, &center_freq2_idx);
	secondary_channel = src->iconf->secondary_channel;

	if (center_freq1_idx == src->iconf->channel &&
	    oper_chwidth == CONF_OPER_CHWIDTH_USE_HT)
		secondary_channel = 0;

	if (ieee80211_freq_to_channel_ext(src->iface->freq,
					  secondary_channel,
					  oper_chwidth,
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return;
	width = hostapd_get_nr_chan_width(src, ht, vht, he, oper_chwidth,
					  secondary_channel);

	if (width != NR_CHAN_WIDTH_80P80)
		center_freq2_idx = 0;

	if (!vht && ht) {
		center_freq1_idx = 0;
		ieee80211_freq_to_chan(src->iface->freq +
				       10 * secondary_channel,
				       &center_freq1_idx);
	}

	ssid.ssid_len = src->conf->ssid.ssid_len;
	os_memcpy(ssid.ssid, src->conf->ssid.ssid, ssid.ssid_len);

	if (hostapd_is_multiple_link_mld(src) && src->mld)
		own_mld_addr = src->mld->mld_addr;

#ifdef CONFIG_IEEE80211BN
	if (src->conf->smd.enabled) {
		own_smd_id = src->conf->smd.smd_identifier;
	}
#endif /* CONFIG_IEEE80211BN */

	/*
	 * Neighbor Report element size = BSSID + BSSID info + op_class + chan +
	 * phy type + wide bandwidth channel subelement.
	 */
	nr = wpabuf_alloc(ETH_ALEN + 4 + 1 + 1 + 1 + 5 + 512);
	if (!nr)
		return;

	wpabuf_put_data(nr, src->own_addr, ETH_ALEN);
	wpabuf_put_le32(nr, bssid_info);
	wpabuf_put_u8(nr, op_class);
	wpabuf_put_u8(nr, channel);
	wpabuf_put_u8(nr, ieee80211_get_phy_type(src->iface->freq, ht, vht, eht));

	/*
	 * Wide Bandwidth Channel subelement may be needed to allow the
	 * receiving STA to send packets to the AP. See IEEE Std 802.11-2024,
	 * Figure 9-423 (Wide Bandwidth Channel subelement format).
	 */
	wpabuf_put_u8(nr, WNM_NEIGHBOR_WIDE_BW_CHAN);
	wpabuf_put_u8(nr, 3);
	wpabuf_put_u8(nr, width);
	wpabuf_put_u8(nr, center_freq1_idx);
	wpabuf_put_u8(nr, center_freq2_idx);

	hostapd_neighbor_add_op_capab_subelements(src, nr, ht, vht, he, eht);
#ifdef CONFIG_IEEE80211BN
	hostapd_neighbor_add_11bn_subelements(src, nr);
#endif /* CONFIG_IEEE80211BN */

#ifdef CONFIG_IEEE80211BE
	/*
	 * For a co-located MLD AP, append a Basic Multi-Link subelement so
	 * the STA can perform ML setup when roaming to this BSS.
	 * Use hostapd_is_multiple_link_mld() rather than conf->mld_ap directly
	 * so the QSDK repurpose guard is respected.
	 */
	if (dest != src && hostapd_is_multiple_link_mld(src)) {
		wpabuf_put_u8(nr, WNM_NEIGHBOR_MULTI_LINK);
		wpabuf_put_u8(nr, 9);
		wpabuf_put_le16(nr, MULTI_LINK_CONTROL_TYPE_BASIC);
		wpabuf_put_u8(nr, 6); /* Common Info Length */
		wpabuf_put_data(nr, src->mld->mld_addr, ETH_ALEN);
	}
#endif /* CONFIG_IEEE80211BE */

	if (hostapd_neighbor_set_mld(dest, src->own_addr, &ssid, nr,
                                 own_mld_addr, own_smd_id,
				 src->iconf->lci, src->iconf->civic,
				 src->iconf->stationary_ap, 0) < 0)
		wpa_printf(MSG_DEBUG,
			   "NR: failed to set own report for " MACSTR " in %s DB",
			   MAC2STR(src->own_addr), dest->conf->iface);
        own_entry = hostapd_neighbor_get(dest, dest->own_addr, &ssid);
	if (own_entry)
		own_entry->self_entry = 1;
	wpabuf_free(nr);
#endif /* NEED_AP_MLME */
}


void hostapd_neighbor_set_own_report(struct hostapd_data *hapd)
{
	hostapd_neighbor_set_own_report_for(hapd, hapd);
}

int hostapd_add_candidate_own(struct hostapd_data *hapd, int pref,
			      u8 *links, u8 num_links,
			      u8 *nei_rep, size_t nei_rep_len)
{
	u8 *nei_pos = nei_rep;
#ifdef NEED_AP_MLME
	u16 capab = hostapd_own_capab_info(hapd);
	bool ht = hostapd_is_ht_enabled(hapd);
	bool vht = hostapd_is_vht_enabled(hapd);
	bool he = hostapd_is_he_enabled(hapd);
	bool eht = he && hostapd_is_eht_enabled(hapd);
	struct wpa_ssid_value ssid;
	u8 channel, op_class;
	u8 center_freq1_idx = 0, center_freq2_idx = 0;
	enum oper_chan_width oper_chwidth;
	enum nr_chan_width width;
	int secondary_channel;
	u32 bssid_info;

	bssid_info = NEI_REP_BSSID_INFO_AP_REACHABLE; /* AP is reachable */
	bssid_info |= NEI_REP_BSSID_INFO_SECURITY; /* "same as the AP" */
	bssid_info |= NEI_REP_BSSID_INFO_KEY_SCOPE; /* "same as the AP" */

	if (capab & WLAN_CAPABILITY_SPECTRUM_MGMT)
		bssid_info |= NEI_REP_BSSID_INFO_SPECTRUM_MGMT;

	bssid_info |= NEI_REP_BSSID_INFO_RM; /* RRM is supported */

	if (hapd->conf->wmm_enabled) {
		bssid_info |= NEI_REP_BSSID_INFO_QOS;

		if (hapd->conf->wmm_uapsd &&
		    (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_UAPSD))
			bssid_info |= NEI_REP_BSSID_INFO_APSD;
	}

	if (ht) {
		bssid_info |= NEI_REP_BSSID_INFO_HT |
			NEI_REP_BSSID_INFO_DELAYED_BA;

		/* VHT bit added in IEEE P802.11-REVmc/D4.3 */
		if (vht)
			bssid_info |= NEI_REP_BSSID_INFO_VHT;
	}

	if (he)
		bssid_info |= NEI_REP_BSSID_INFO_HE;
	if (eht)
		bssid_info |= NEI_REP_BSSID_INFO_EHT;
	/* TODO: Set NEI_REP_BSSID_INFO_MOBILITY_DOMAIN if MDE is set */

	hostapd_get_oper_chan_info_of_bss(hapd, &oper_chwidth,
					  &center_freq1_idx, &center_freq2_idx);
	secondary_channel = hapd->iconf->secondary_channel;

	if (center_freq1_idx == hapd->iconf->channel &&
	    oper_chwidth == CONF_OPER_CHWIDTH_USE_HT)
		secondary_channel = 0;

	if (ieee80211_freq_to_channel_ext(hapd->iface->freq,
					  secondary_channel,
					  oper_chwidth,
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return 0;
	width = hostapd_get_nr_chan_width(hapd, ht, vht, he, oper_chwidth,
					  secondary_channel);

	if (width != NR_CHAN_WIDTH_80P80)
		center_freq2_idx = 0;

	if (!vht && ht) {
		center_freq1_idx = 0;
		ieee80211_freq_to_chan(hapd->iface->freq +
				       10 * secondary_channel,
				       &center_freq1_idx);
	}

	ssid.ssid_len = hapd->conf->ssid.ssid_len;
	os_memcpy(ssid.ssid, hapd->conf->ssid.ssid, ssid.ssid_len);

	*nei_pos++ = WLAN_EID_NEIGHBOR_REPORT;
	nei_pos++; /* length to be filled in */

	os_memcpy(nei_pos, hapd->own_addr, ETH_ALEN);
	nei_pos += ETH_ALEN;

	WPA_PUT_LE32(nei_pos, bssid_info);
	nei_pos += 4;

	*nei_pos++ = op_class;
	*nei_pos++ = channel;
	*nei_pos++ = ieee80211_get_phy_type(hapd->iface->freq, ht, vht, eht);

	/* Candidate preference subelement */
	*nei_pos++ = WNM_NEIGHBOR_BSS_TRANSITION_CANDIDATE;
	*nei_pos++ = 1;
	*nei_pos++ = pref;

	/*
	 * Wide Bandwidth Channel subelement may be needed to allow the
	 * receiving STA to send packets to the AP. See IEEE P802.11-REVmc/D5.0
	 * Figure 9-301.
	 */
	*nei_pos++ = WNM_NEIGHBOR_WIDE_BW_CHAN;
	*nei_pos++ = 3;
	*nei_pos++ = width;
	*nei_pos++ = center_freq1_idx;
	*nei_pos++ = center_freq2_idx;

#ifdef CONFIG_IEEE80211BE
#ifdef CONFIG_QCN_EXTN
	/* Skip Basic multi-link subelement if BSS is repurposed */
	if (!hostapd_is_repurpose_disabled_11be_extn(hapd->conf)) {
#endif /* CONFIG_QCN_EXTN */
	/* Basic multi-link subelement */
	if (hapd->conf->mld_ap) {
		int len;

		len = hostapd_wnm_add_multi_link_sub_elem(hapd, links,
							  num_links,
							  nei_pos,
							  nei_pos - nei_rep);
		if (len < 0)
			return -1;

		nei_pos += len;
	}
#ifdef CONFIG_QCN_EXTN
	}
#endif /* CONFIG_QCN_EXTN */
#endif /* CONFIG_IEEE80211BE */

	nei_rep[1] = nei_pos - nei_rep - 2;
#endif /* NEED_AP_MLME */
	return nei_pos - nei_rep;
}

static struct hostapd_neighbor_entry *
hostapd_neighbor_db_get_diff(struct hostapd_data *hapd, const u8 *bssid)
{
	struct hostapd_neighbor_entry *nr;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			list) {
		if (!ether_addr_equal(bssid, nr->bssid))
			continue;

		if (nr->short_ssid != hapd->conf->ssid.short_ssid)
			return nr;

#ifdef CONFIG_IEEE80211BN
		if (os_memcmp(nr->smd_id, hapd->conf->smd.smd_identifier,
					ETH_ALEN) != 0)
			return nr;
#endif
	}
	return NULL;
}


int hostapd_neighbor_sync_own_report(struct hostapd_data *hapd)
{
	struct hostapd_neighbor_entry *nr;

	nr = hostapd_neighbor_db_get_diff(hapd, hapd->own_addr);
	if (!nr)
		return -1;

	/* Clear old entry due to SSID change */
	hostapd_neighbor_free(nr);

	hostapd_neighbor_set_own_report(hapd);

	return 0;
}

#ifdef NEED_AP_MLME
static u32 hostapd_get_band(struct hostapd_iface *iface)
{
	int freq = iface->freq;
	enum hostapd_hw_mode mode = iface->current_mode ?
				    iface->current_mode->mode :
				    iface->conf->hw_mode;

	if (mode == HOSTAPD_MODE_IEEE80211B ||
	    mode == HOSTAPD_MODE_IEEE80211G)
		return WPA_SETBAND_2G;
	else if (mode == HOSTAPD_MODE_IEEE80211A) {
		if (is_6ghz_freq(freq))
			return WPA_SETBAND_6G;
		else
			return WPA_SETBAND_5G;
	}

	return WPA_SETBAND_AUTO;
}

static u32 hostapd_get_nr_bssid_info(struct wpa_scan_res *bss,
				     struct ieee802_11_elems *elems)
{
	u32 info = 0;

	/* AP reachability unknown */
	info = NEI_REP_BSSID_INFO_AP_UNKNOWN_REACH;

	/*
	 * Leave the security and key scope bits unset to indicate that the
	 * security information is not available.
	 */

	if (bss->caps & WLAN_CAPABILITY_SPECTRUM_MGMT)
		info |= NEI_REP_BSSID_INFO_SPECTRUM_MGMT;
	if (bss->caps & WLAN_CAPABILITY_QOS)
		info |= NEI_REP_BSSID_INFO_QOS;
	if (bss->caps & WLAN_CAPABILITY_APSD)
		info |= NEI_REP_BSSID_INFO_APSD;
	if (bss->caps & WLAN_CAPABILITY_RADIO_MEASUREMENT)
		info |= NEI_REP_BSSID_INFO_RM;
	if (bss->caps & WLAN_CAPABILITY_DELAYED_BLOCK_ACK)
		info |= NEI_REP_BSSID_INFO_DELAYED_BA;
	if (bss->caps & WLAN_CAPABILITY_IMM_BLOCK_ACK)
		info |= NEI_REP_BSSID_INFO_IMM_BA;

	/* TODO: Mobility domain flag */

	if (elems->ht_capabilities) {
		info |= NEI_REP_BSSID_INFO_HT;
		if (elems->vht_capabilities)
			info |= NEI_REP_BSSID_INFO_VHT;
	}
	if (elems->he_capabilities)
		info |= NEI_REP_BSSID_INFO_HE;
	if (elems->eht_capabilities)
		info |= NEI_REP_BSSID_INFO_EHT;

	return info;
}

static void get_channel_width_and_center_freqs(struct ieee802_11_elems *elems,
					       enum nr_chan_width *width,
					       u8 *center_freq1_idx,
					       u8 *center_freq2_idx)
{
	*width = NR_CHAN_WIDTH_20;
	*center_freq1_idx = 0;
	*center_freq2_idx = 0;

	if (elems->eht_operation) {
		const struct ieee80211_eht_operation *eht_oper =
		(const struct ieee80211_eht_operation *) elems->eht_operation;

		if (eht_oper->oper_params & EHT_OPER_INFO_PRESENT) {
			*width = eht_oper->oper_info.control & 0x7;
			*center_freq1_idx = eht_oper->oper_info.ccfs0;
			*center_freq2_idx = eht_oper->oper_info.ccfs1;
			return;
		}
	}

	if (elems->he_operation) {
		const struct ieee80211_he_operation *he_oper =
		(const struct ieee80211_he_operation *) elems->he_operation;
		int offset = 0;

		if (he_oper->he_oper_params & HE_OPERATION_VHT_OPER_INFO)
			offset = 3;
		if (he_oper->he_oper_params & HE_OPERATION_COHOSTED_BSS)
			offset += 1;
		if (he_oper->he_oper_params & HE_OPERATION_6GHZ_OPER_INFO) {
			const struct ieee80211_he_6ghz_oper_info *oper_info =
			    (const struct ieee80211_he_6ghz_oper_info *)
			    (elems->he_operation +
			     sizeof(struct ieee80211_he_operation) + offset);

			*width = oper_info->control &
			    HE_6GHZ_OPER_INFO_CTRL_CHAN_WIDTH_MASK;
			*center_freq1_idx =
			    oper_info->chan_center_freq_seg0;
			*center_freq2_idx =
			    oper_info->chan_center_freq_seg1;
			return;
		}
	}

	if (elems->vht_operation) {
		const struct ieee80211_vht_operation *vht_oper =
		(const struct ieee80211_vht_operation *) elems->vht_operation;

		switch (vht_oper->vht_op_info_chwidth) {
		case CHANWIDTH_80MHZ:
			*width = NR_CHAN_WIDTH_80;
			break;
		case CHANWIDTH_160MHZ:
			*width = NR_CHAN_WIDTH_160;
			break;
		case CHANWIDTH_80P80MHZ:
			*width = NR_CHAN_WIDTH_80P80;
			break;
		default:
			*width = NR_CHAN_WIDTH_20;
			break;
		}

		*center_freq1_idx = vht_oper->vht_op_info_chan_center_freq_seg0_idx;
		*center_freq2_idx = vht_oper->vht_op_info_chan_center_freq_seg1_idx;
	}
}

static int
hostapd_neighbor_set_scan_report(struct hostapd_data *hapd,
				 const struct wpa_ssid_value *ssid,
				 int freq,
				 struct wpa_scan_results *scan_res)
{
	struct wpa_ssid_value bss_ssid = *ssid;
	int i;

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *bss = scan_res->res[i];
		struct ieee802_11_elems elems;
		struct ieee80211_ht_operation *ht_oper = NULL;
		struct ieee80211_vht_operation *vht_oper = NULL;
		enum oper_chan_width vht_width = CONF_OPER_CHWIDTH_USE_HT;
		enum phy_type phy_type;
		enum nr_chan_width width;
		u8 center_freq1_idx = 0, center_freq2_idx = 0;
		struct wpabuf *nr;
		int ht = 0, vht = 0, eht = 0;
		int sec_chan = 0;
		u8 op_class, chan;
		u32 info;

		/* Check for SSID match if supplied, otherwise fill with all */
		if (ieee802_11_parse_elems((u8 *) (bss + 1), bss->ie_len,
					   &elems, 0) == ParseFailed ||
		    freq != bss->freq ||
		    (bss_ssid.ssid_len &&
		     (bss_ssid.ssid_len != elems.ssid_len ||
		      os_memcmp(bss_ssid.ssid, elems.ssid, bss_ssid.ssid_len))))
			continue;

		if (!bss_ssid.ssid_len && elems.ssid_len) {
			os_memcpy(bss_ssid.ssid, elems.ssid, elems.ssid_len);
			bss_ssid.ssid_len = elems.ssid_len;
		}

		if (elems.ht_operation) {
			ht_oper = (struct ieee80211_ht_operation *) (elems.ht_operation);
			ht = 1;

			if (ht_oper->ht_param &
			    HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE)
				sec_chan = 1;
			else if (ht_oper->ht_param &
				 HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW)
				sec_chan = -1;
		}

		if (elems.vht_operation) {
			vht_oper = (struct ieee80211_vht_operation *) (elems.vht_operation);
			vht = 1;

			if (vht_oper->vht_op_info_chwidth == CHANWIDTH_80MHZ ||
			    vht_oper->vht_op_info_chwidth == CHANWIDTH_160MHZ ||
			    vht_oper->vht_op_info_chwidth == CHANWIDTH_80P80MHZ)
				vht_width = vht_oper->vht_op_info_chwidth;
		}

		/* Get channel and opmode */
		if (ieee80211_freq_to_channel_ext(bss->freq, sec_chan,
						  vht_width, &op_class,
						  &chan) == NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_DEBUG,
				   "NR: Cannot determine opclass and channel");
			continue;
		}

		eht = hostapd_is_eht_enabled(hapd);
		/* Get phy type */
		phy_type = ieee80211_get_phy_type(bss->freq, ht, vht, eht);
		if (phy_type == PHY_TYPE_UNSPECIFIED) {
			wpa_printf(MSG_DEBUG,
				   "NR: Cannot determine BSS phy type");
			continue;
		}

		/* Get BSSID info */
		info = hostapd_get_nr_bssid_info(bss, &elems);

		/* Get WB channel width and freq idx */
		get_channel_width_and_center_freqs(&elems, &width,
						   &center_freq1_idx,
						   &center_freq2_idx);

		wpa_printf(MSG_DEBUG, "Neighboring BSS: " MACSTR
			   " info=08%x opclass=%d chan=%d", MAC2STR(bss->bssid),
			   info, op_class, chan);

		/*
		 * Neighbor Report element size = BSSID + BSSID info +
		 * op_class + chan + phy type +
		 * wide bandwidth channel subelement.
		 */
		if (center_freq1_idx || center_freq2_idx)
			nr = wpabuf_alloc(ETH_ALEN + 4 + 1 + 1 + 1 + 5);
		else
			nr = wpabuf_alloc(ETH_ALEN + 4 + 1 + 1 + 1);
		if (!nr)
			return -1;

		wpabuf_put_data(nr, bss->bssid, ETH_ALEN);
		wpabuf_put_le32(nr, info);
		wpabuf_put_u8(nr, op_class);
		wpabuf_put_u8(nr, chan);
		wpabuf_put_u8(nr, phy_type);

		/*
		 * Wide Bandwidth Channel subelement may be needed to allow the
		 * receiving STA to send packets to the AP.
		 * See IEEE P802.11-REVmc/D5.0
		 * Figure 9-301.
		 */
		if (center_freq1_idx || center_freq2_idx) {
			wpabuf_put_u8(nr, WNM_NEIGHBOR_WIDE_BW_CHAN);
			wpabuf_put_u8(nr, 3);
			wpabuf_put_u8(nr, width);
			wpabuf_put_u8(nr, center_freq1_idx);
			wpabuf_put_u8(nr, center_freq2_idx);
		}

		hostapd_neighbor_set(hapd, bss->bssid, &bss_ssid, nr,
				     NULL, NULL, 0, 0);

		wpabuf_free(nr);
	}

	return 0;
}
#endif /* NEED_AP_MLME */


#ifdef NEED_AP_MLME
/*
 * hostapd_oce_update_channel_info - Scan cached driver results and update
 * OCE Capability Indication bits in hapd:
 *   non_oce_ap_present: set if any AP on the operating channel lacks
 *                           the OCE Capability Indication attribute (BIT(6))
 *   ap_11b_present:     set if any 2.4 GHz AP on the operating channel
 *                           lacks the ERP element (BIT(4))
 *
 * Called from hostapd_neighbor_set_ifaces_scan_report() after the neighbor
 * DB is rebuilt from scan results.
 */
static void hostapd_oce_update_channel_info(struct hostapd_data *hapd)
{
	struct wpa_scan_results *scan_res;
	int i;

	hapd->non_oce_ap_present = 0;
	hapd->ap_11b_present = 0;

	scan_res = hostapd_driver_get_scan_results(hapd);
	if (!scan_res) {
		wpa_printf(MSG_DEBUG, "OCE channel info: no scan results");
		return;
	}

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *bss = scan_res->res[i];
		struct ieee802_11_elems elems;

		/* Only consider APs on our operating channel */
		if (bss->freq != hapd->iface->freq)
			continue;

		/* Skip our own BSS */
		if (ether_addr_equal(bss->bssid, hapd->own_addr))
			continue;

		if (ieee802_11_parse_elems((u8 *) (bss + 1), bss->ie_len,
					   &elems, 0) == ParseFailed)
			continue;

		/* 11b-only per OCE spec: 2.4 GHz AP without ERP element */
		if (is_24ghz_freq(bss->freq) && !elems.erp_info)
			hapd->ap_11b_present = 1;

		/* Non-OCE: no MBO-OCE IE with OCE Capability Indication */
		if (!elems.mbo || !ieee80211_is_oce_capable(elems.mbo,
							    elems.mbo_len))
			hapd->non_oce_ap_present = 1;
	}

	wpa_scan_results_free(scan_res);

	wpa_printf(MSG_DEBUG,
		   "OCE channel info: non_oce_ap_present=%d 11b_ap_present=%d",
		   hapd->non_oce_ap_present, hapd->ap_11b_present);
}
#endif /* NEED_AP_MLME */

int hostapd_neighbor_set_ifaces_scan_report(struct hostapd_data *hapd,
					    const struct wpa_ssid_value *ssid,
					    u32 bands)
{
#ifdef NEED_AP_MLME
	struct hapd_interfaces *interfaces = hapd->iface->interfaces;
	struct wpa_scan_results *scan_res;
	int ret;
	int i;

	/* Clear the old NR entries */
	hostapd_free_neighbor_db(hapd);

	/*
	 * Get list of neighboring BSSes (from scan) and add to the
	 * neighbor database
	 */

	/* Get own radio scan report and set the neighbor databse */
	scan_res = hostapd_driver_get_scan_results(hapd);
	if (scan_res == NULL)
		wpa_printf(MSG_DEBUG, "No scan result found");
	else {
		ret = hostapd_neighbor_set_scan_report(hapd,
						       ssid,
						       hapd->iface->freq,
						       scan_res);
		wpa_scan_results_free(scan_res);
		if (ret)
			return -1;
	}

	/* OCE 4.3.1/4.3.2: update channel info bits from driver scan cache */
	if (OCE_AP_ENABLED(hapd))
		hostapd_oce_update_channel_info(hapd);

	/* Restore the AP's own neighbor report entry (cleared above) */
	hostapd_neighbor_set_own_report(hapd);

	if (!bands)
		return 0;

	/* Iterate over other radio interfaces and get the scan results */
	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];
		int j;

		if (!iface)
			continue;

		if (iface == hapd->iface)
			continue;

		if ((hostapd_get_band(iface) & bands) == 0)
			continue;

		scan_res = hostapd_driver_get_scan_results(iface->bss[0]);
		if (scan_res == NULL)
			wpa_printf(MSG_DEBUG, "No scan result found");
		else {
			ret = hostapd_neighbor_set_scan_report(hapd,
							       ssid,
							       iface->freq,
							       scan_res);
			wpa_scan_results_free(scan_res);
			if (ret)
				return -1;
		}

		/*
		 * Scan results never include the scanning radio's own BSSIDs.
		 * Inject an authoritative NR entry for every BSS on this
		 * other-band interface into hapd's NR DB so that co-located
		 * radios are visible to STAs as roam candidates
		 * (e.g. 2.4/5/6 GHz in a 3-link MLO deployment).
		 */
		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *other_hapd = iface->bss[j];

			if (other_hapd && other_hapd->started)
				hostapd_neighbor_set_own_report_for(hapd,
								    other_hapd);
		}
	}

#endif /* NEED_AP_MLME */
	return 0;
}


#ifdef NEED_AP_MLME
/*
 * OCE 4.3.1/4.3.2: Periodic channel survey update
 *
 * Every OCE_SURVEY_INTERVAL seconds the AP triggers a scan on the
 * operating channel, then on EVENT_SCAN_RESULTS reads the fresh driver
 * pushes updated OCE AP Channel Report IEs into beacons for all
 * co-located BSSes.  Co-located BSS info is read from iface->interfaces
 * at IE build time; no scan is needed.
 *
 * Timer context is struct hostapd_iface so a single timer covers all
 * BSSes on the radio.
 */
#define OCE_SURVEY_INTERVAL      3600  /* 1hr - operating channel refresh */

void hostapd_oce_survey_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_iface *iface = eloop_ctx;
	size_t i, j;

	/*
	 * AP-mode driver scans do not return results via the nl80211 BSS
	 * cache (confirmed on ath12k).  Rebuild beacons directly: the OCE
	 * AP Channel Report IE reads co-located BSS info from iface->interfaces
	 * at build time, so no scan is needed for co-located BSS discovery.
	 */
	for (i = 0; i < iface->interfaces->count; i++) {
		struct hostapd_iface *other = iface->interfaces->iface[i];

		if (!other || !other->num_bss)
			continue;
		for (j = 0; j < other->num_bss; j++) {
			struct hostapd_data *h = other->bss[j];

			if (h && h->started && OCE_AP_ENABLED(h))
				ieee802_11_set_beacon(h);
		}
	}

	wpa_printf(MSG_DEBUG, "OCE survey: beacon update complete on %s",
		   iface->bss[0]->conf->iface);

	eloop_register_timeout(OCE_SURVEY_INTERVAL, 0,
			       hostapd_oce_survey_timer, iface, NULL);
}


void hostapd_oce_survey_timer_start(struct hostapd_iface *iface)
{
	size_t i;
	int any_oce = 0;

	if (!iface->num_bss)
		return;

#ifdef CONFIG_IEEE80211BE
	/* Skip until a clean per-link model is available. */
	if (iface->bss[0]->conf->mld_ap)
		return;
#endif /* CONFIG_IEEE80211BE */

	/* Start the timer if any BSS on this radio has OCE enabled */
	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i] && OCE_AP_ENABLED(iface->bss[i])) {
			any_oce = 1;
			break;
		}
	}
	if (!any_oce)
		return;

	/*
	 * On single-wiphy devices all BSSes share phy0 (both bands).
	 * Only start the OCE timer on the FIRST registered iface to avoid
	 * concurrent scan requests on the same phy from multiple timers.
	 */
	if (iface->interfaces && iface->interfaces->count > 1 &&
	    iface->interfaces->iface[0] != iface) {
		wpa_printf(MSG_DEBUG,
			   "OCE survey: skipping timer on %s (not primary iface)",
			   iface->bss[0]->conf->iface);
		return;
	}

	wpa_printf(MSG_DEBUG,
		   "OCE survey: scheduling on primary iface %s",
		   iface->bss[0]->conf->iface);

	eloop_cancel_timeout(hostapd_oce_survey_timer, iface, NULL);
	/* Fire at 120s initially for CTT AP discovery */
	eloop_register_timeout(120, 0,
			       hostapd_oce_survey_timer, iface, NULL);
}


void hostapd_oce_survey_timer_cancel(struct hostapd_iface *iface)
{
	eloop_cancel_timeout(hostapd_oce_survey_timer, iface, NULL);
}
#endif /* NEED_AP_MLME */
