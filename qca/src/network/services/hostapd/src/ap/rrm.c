/*
 * hostapd / Radio Measurement (RRM)
 * Copyright(c) 2013 - 2016 Intel Mobile Communications GmbH.
 * Copyright(c) 2011 - 2016 Intel Corporation. All rights reserved.
 * Copyright (c) 2016-2017, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "hostapd.h"
#include "ap_drv_ops.h"
#include "sta_info.h"
#include "eloop.h"
#include "neighbor_db.h"
#include "rrm.h"

#define HOSTAPD_RRM_REQUEST_TIMEOUT 5

/* Forward declarations */
static void hostapd_chan_load_rep_timeout_handler(void *eloop_data,
						  void *user_ctx);
static void hostapd_handle_channel_load_report(struct hostapd_data *hapd,
					       const u8 *addr, u8 token,
					       u8 rep_mode, const u8 *pos,
					       size_t len);
static void hostapd_noise_hist_rep_timeout_handler(void *eloop_data,
						   void *user_ctx);
static void hostapd_handle_noise_histogram_report(struct hostapd_data *hapd,
						  const u8 *addr, u8 token,
						  u8 rep_mode, const u8 *pos,
						  size_t len);


static void hostapd_lci_rep_timeout_handler(void *eloop_data, void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;

	wpa_printf(MSG_DEBUG, "RRM: LCI request (token %u) timed out",
		   hapd->lci_req_token);
	hapd->lci_req_active = 0;
}


static void hostapd_handle_lci_report(struct hostapd_data *hapd,
				      const u8 *addr, u8 token,
				      u8 rep_mode, const u8 *pos,
				      size_t len)
{
	char *hex;
	size_t body_len;

	if (!hapd->lci_req_active || hapd->lci_req_token != token) {
		wpa_printf(MSG_DEBUG, "Unexpected LCI report, token %u", token);
		return;
	}

	hapd->lci_req_active = 0;
	eloop_cancel_timeout(hostapd_lci_rep_timeout_handler, hapd, NULL);

	wpa_printf(MSG_DEBUG, "LCI report token %u len %zu", token, len);

	if (rep_mode & (MEASUREMENT_REPORT_MODE_REJECT_INCAPABLE |
			MEASUREMENT_REPORT_MODE_REJECT_REFUSED)) {
		wpa_printf(MSG_DEBUG,
			   "RRM: LCI report refused/incapable from "
			   MACSTR " mode=0x%02x",
			   MAC2STR(addr), rep_mode);
		wpa_msg(hapd->msg_ctx, MSG_INFO,
			LCI_RESP_RX MACSTR " %u 0x%02x",
			MAC2STR(addr), token, rep_mode);
		return;
	}

	/*
	 * pos[0]=msr_token, pos[1]=rep_mode, pos[2]=type, pos[3..]=body.
	 * Body is a sequence of subelements; emit as hex for the application.
	 */
	if (len < 3) {
		wpa_printf(MSG_DEBUG, "RRM: LCI report too short (len=%zu)", len);
		return;
	}

	body_len = len - 3;
	hex = os_zalloc(body_len * 2 + 1);
	if (!hex)
		return;
	wpa_snprintf_hex(hex, body_len * 2 + 1, pos + 3, body_len);

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		LCI_RESP_RX MACSTR " %u 0x%02x %s",
		MAC2STR(addr), token, rep_mode, hex);

	os_free(hex);
}


static void hostapd_range_rep_timeout_handler(void *eloop_data, void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;

	wpa_printf(MSG_DEBUG, "RRM: Range request (token %u) timed out",
		   hapd->range_req_token);
	hapd->range_req_active = 0;
}

struct hostapd_bcn_report_entry *
hostapd_bcn_report_get(struct hostapd_data *hapd, const u8 *bssid, size_t len)
{
	struct hostapd_bcn_report_entry *bcn_report;
	size_t subelem_len = len - WLAN_BEACON_REPORT_FIXED_LEN;

	dl_list_for_each(bcn_report, &hapd->bcn_report_db,
			 struct hostapd_bcn_report_entry,
			 list) {
		if (ether_addr_equal(bssid, bcn_report->bssid)) {
			if (bcn_report->subelem &&
			    (bcn_report->subelem_len != subelem_len)) {
				os_free(bcn_report->subelem);
				if (subelem_len)
					bcn_report->subelem = os_zalloc(subelem_len);
			} else if (!bcn_report->subelem && subelem_len)
				bcn_report->subelem = os_zalloc(subelem_len);
			if (!bcn_report->subelem && subelem_len) {
				os_free(bcn_report);
				bcn_report = NULL;
			}
			return bcn_report;
		}
	}

	return NULL;
}

static void
hostapd_bcn_report_free(struct hostapd_bcn_report_entry *bcn_report)
{
	dl_list_del(&bcn_report->list);
	if (bcn_report->subelem)
		os_free(bcn_report->subelem);
	os_free(bcn_report);
}

int hostapd_show_rrm_bcn_report(struct hostapd_data *hapd,
				char *buf, size_t buflen)
{
	struct hostapd_bcn_report_entry *bcn_report;
	char *pos, *end;
	int ret;
	char subelem[512];

	if (!hapd || !buf || !buflen)
		return -1;

	pos = buf;
	end = buf + buflen;

	if (!dl_list_len(&hapd->bcn_report_db))
		return 0;

	dl_list_for_each(bcn_report, &hapd->bcn_report_db,
			 struct hostapd_bcn_report_entry, list) {

		os_memset(subelem, '\0', sizeof(subelem));
		if (bcn_report->subelem && bcn_report->subelem_len)
			wpa_snprintf_hex(subelem, sizeof(subelem),
					 bcn_report->subelem,
					 bcn_report->subelem_len);
		ret = os_snprintf(pos, end - pos, "bssid="MACSTR
				  " op_class=%u chan=%u start_time=%llu"
				  " dur=%hu report_info=%u rcpi=%u"
				  " rsni=%u ant_id=%u parent_tsf=%u"
				  "%s%s\n",
				  MAC2STR(bcn_report->bssid),
				  bcn_report->op_class, bcn_report->channel,
				  (long long unsigned int)bcn_report->start_time,
				  bcn_report->duration,
				  bcn_report->report_info, bcn_report->rcpi,
				  bcn_report->rsni, bcn_report->antenna_id,
				  bcn_report->parent_tsf,
				  bcn_report->subelem ? " subelem=" : "",
				  subelem);
		if (os_snprintf_error(end - pos, ret))
			break;
		pos += ret;
	}

	return pos - buf;
}

void hostapd_free_bcn_report_db(struct hostapd_data *hapd)
{
	struct hostapd_bcn_report_entry *bcn_report, *prev;

	dl_list_for_each_safe(bcn_report, prev, &hapd->bcn_report_db,
			      struct hostapd_bcn_report_entry, list)
		hostapd_bcn_report_free(bcn_report);
}


static struct hostapd_bcn_report_entry *
hostapd_bcn_report_alloc(struct hostapd_data *hapd, size_t len)
{
	struct hostapd_bcn_report_entry *bcn_report;
	size_t subelem_len = len - WLAN_BEACON_REPORT_FIXED_LEN;

	bcn_report = os_zalloc(sizeof(struct hostapd_bcn_report_entry));
	if (!bcn_report)
		return NULL;

	if (subelem_len > 0) {
		bcn_report->subelem = os_zalloc(subelem_len);
		if (!bcn_report->subelem) {
			os_free(bcn_report);
			return NULL;
		}
	}

	dl_list_add(&hapd->bcn_report_db, &bcn_report->list);

	return bcn_report;
}

static int
hostapd_bcn_report_add(struct hostapd_data *hapd,
		       struct rrm_measurement_beacon_report *bcn_report,
		       size_t len)
{
	struct hostapd_bcn_report_entry *entry;
	size_t subelem_len = len - WLAN_BEACON_REPORT_FIXED_LEN;
	char subelem[256] = {'\0'};

	if (!hapd || !bcn_report)
		return -1;

	entry = hostapd_bcn_report_get(hapd, bcn_report->bssid, len);
	if (!entry)
		entry = hostapd_bcn_report_alloc(hapd, len);
	if (!entry)
		return -1;

	os_memcpy(entry->bssid, bcn_report->bssid, ETH_ALEN);
	entry->channel = bcn_report->channel;
	entry->start_time = bcn_report->start_time;
	entry->duration = bcn_report->duration;
	entry->report_info = bcn_report->report_info;
	entry->rcpi = bcn_report->rcpi;
	entry->rsni = bcn_report->rsni;
	entry->antenna_id = bcn_report->antenna_id;
	entry->parent_tsf = bcn_report->parent_tsf;
	if ((subelem_len > 0) &&
	    entry->subelem) {
		wpa_snprintf_hex(subelem, sizeof(subelem),
				bcn_report->variable,
				subelem_len);
		os_memcpy(entry->subelem, bcn_report->variable, subelem_len);
		entry->subelem_len = subelem_len;
	}

	return 0;
}

static void hostapd_handle_range_report(struct hostapd_data *hapd, u8 token,
					const u8 *pos, size_t len)
{
	if (!hapd->range_req_active || hapd->range_req_token != token) {
		wpa_printf(MSG_DEBUG, "Unexpected range report, token %u",
			   token);
		return;
	}

	hapd->range_req_active = 0;
	eloop_cancel_timeout(hostapd_range_rep_timeout_handler, hapd, NULL);
	wpa_printf(MSG_DEBUG, "Range report token %u len %zu", token, len);
}


static void hostapd_handle_beacon_report(struct hostapd_data *hapd,
					 const u8 *addr, u8 token, u8 rep_mode,
					 const u8 *pos, size_t len)
{
	char report[2 * 255 + 1];

	wpa_printf(MSG_DEBUG, "Beacon report token %u len %zu from " MACSTR,
		   token, len, MAC2STR(addr));
	/* Skip to the beginning of the Beacon report */
	if (len < 3)
		return;
	pos += 3;
	len -= 3;
	report[0] = '\0';
	if (wpa_snprintf_hex(report, sizeof(report), pos, len) < 0)
		return;
	wpa_msg(hapd->msg_ctx, MSG_INFO, BEACON_RESP_RX MACSTR " %u %02x %s",
		MAC2STR(addr), token, rep_mode, report);
	if (len < sizeof(struct rrm_measurement_beacon_report))
		return;
	hostapd_ubus_notify_beacon_report(hapd, addr, token, rep_mode, (struct rrm_measurement_beacon_report*) pos, len);
	if (hostapd_bcn_report_add(hapd,
				   (struct rrm_measurement_beacon_report *) pos,
				   len))
		wpa_printf(MSG_DEBUG, "Failed to add Beacon report for "MACSTR,
			   MAC2STR(addr));
}


static void hostapd_handle_radio_msmt_report(struct hostapd_data *hapd,
					     const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	const u8 *pos, *ie, *end;
	u8 token, rep_mode;

	end = buf + len;
	token = mgmt->u.action.u.rrm.dialog_token;
	pos = mgmt->u.action.u.rrm.variable;

	while ((ie = get_ie(pos, end - pos, WLAN_EID_MEASURE_REPORT))) {
		if (ie[1] < 3) {
			wpa_printf(MSG_DEBUG, "Bad Measurement Report element");
			break;
		}

		rep_mode = ie[3];
		wpa_printf(MSG_DEBUG, "Measurement report mode 0x%x type %u",
			   rep_mode, ie[4]);

		switch (ie[4]) {
		case MEASURE_TYPE_LCI:
			hostapd_handle_lci_report(hapd, mgmt->sa, token,
						  rep_mode, ie + 2, ie[1]);
			break;
		case MEASURE_TYPE_FTM_RANGE:
			hostapd_handle_range_report(hapd, token, ie + 2, ie[1]);
			break;
		case MEASURE_TYPE_CHANNEL_LOAD:
			hostapd_handle_channel_load_report(hapd, mgmt->sa,
							   token, rep_mode,
							   ie + 2, ie[1]);
			break;
		case MEASURE_TYPE_NOISE_HIST:
			hostapd_handle_noise_histogram_report(hapd, mgmt->sa,
							      token, rep_mode,
							      ie + 2, ie[1]);
			break;
		case MEASURE_TYPE_BEACON:
			hostapd_handle_beacon_report(hapd, mgmt->sa, token,
						     rep_mode, ie + 2, ie[1]);
			break;
		default:
			wpa_printf(MSG_DEBUG,
				   "Measurement report type %u is not supported",
				   ie[4]);
			break;
		}

		pos = ie + ie[1] + 2;
	}
}


static u16 hostapd_parse_location_lci_req_age(const u8 *buf, size_t len)
{
	const u8 *subelem;

	/* Range Request element + Location Subject + Maximum Age subelement */
	if (len < 3 + 1 + 4)
		return 0;

	/* Subelements are arranged as IEs */
	subelem = get_ie(buf + 4, len - 4, LCI_REQ_SUBELEM_MAX_AGE);
	if (subelem && subelem[1] == 2)
		return WPA_GET_LE16(subelem + 2);

	return 0;
}


static int hostapd_check_lci_age(struct hostapd_neighbor_entry *nr, u16 max_age)
{
	struct os_time curr, diff;
	unsigned long diff_l;

	if (nr->stationary || max_age == 0xffff)
		return 1;

	if (!max_age)
		return 0;

	if (os_get_time(&curr))
		return 0;

	os_time_sub(&curr, &nr->lci_date, &diff);

	/* avoid overflow */
	if (diff.sec > 0xffff)
		return 0;

	/* LCI age is calculated in 10th of a second units. */
	diff_l = diff.sec * 10 + diff.usec / 100000;

	return max_age > diff_l;
}


static size_t hostapd_neighbor_report_len(struct wpabuf *buf,
					  struct hostapd_neighbor_entry *nr,
					  int send_lci, int send_civic)
{
	size_t len = 2 + wpabuf_len(nr->nr);

	if (send_lci && nr->lci)
		len += 2 + wpabuf_len(nr->lci);

	if (send_civic && nr->civic)
		len += 2 + wpabuf_len(nr->civic);

	return len;
}


void hostapd_send_nei_report_resp(struct hostapd_data *hapd,
				  const u8 *addr, u8 dialog_token,
				  struct wpa_ssid_value *ssid, u8 lci,
				  u8 civic, u16 lci_max_age)
{
	struct hostapd_neighbor_entry *nr;
	struct wpabuf *buf;
	u8 *msmt_token;

	/*
	 * The number and length of the Neighbor Report elements in a Neighbor
	 * Report frame is limited by the maximum allowed MMPDU size; + 3 bytes
	 * of RRM header.
	 */
	buf = wpabuf_alloc(3 + IEEE80211_MAX_MMPDU_SIZE);
	if (!buf)
		return;

	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_NEIGHBOR_REPORT_RESPONSE);
	wpabuf_put_u8(buf, dialog_token);

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		int send_lci;
		size_t len;

		if (ssid->ssid_len != nr->ssid.ssid_len ||
		    os_memcmp(ssid->ssid, nr->ssid.ssid, ssid->ssid_len) != 0)
			continue;

		send_lci = (lci != 0) && hostapd_check_lci_age(nr, lci_max_age);
		len = hostapd_neighbor_report_len(buf, nr, send_lci, civic);

		if (len - 2 > 0xff) {
			wpa_printf(MSG_DEBUG,
				   "NR entry for " MACSTR " exceeds 0xFF bytes",
				   MAC2STR(nr->bssid));
			continue;
		}

		if (len > wpabuf_tailroom(buf))
			break;

		wpabuf_put_u8(buf, WLAN_EID_NEIGHBOR_REPORT);
		wpabuf_put_u8(buf, len - 2);
		wpabuf_put_buf(buf, nr->nr);

		if (send_lci && nr->lci) {
			wpabuf_put_u8(buf, WLAN_EID_MEASURE_REPORT);
			wpabuf_put_u8(buf, wpabuf_len(nr->lci));
			/*
			 * Override measurement token - the first byte of the
			 * Measurement Report element.
			 */
			msmt_token = wpabuf_put(buf, 0);
			wpabuf_put_buf(buf, nr->lci);
			*msmt_token = lci;
		}

		if (civic && nr->civic) {
			wpabuf_put_u8(buf, WLAN_EID_MEASURE_REPORT);
			wpabuf_put_u8(buf, wpabuf_len(nr->civic));
			/*
			 * Override measurement token - the first byte of the
			 * Measurement Report element.
			 */
			msmt_token = wpabuf_put(buf, 0);
			wpabuf_put_buf(buf, nr->civic);
			*msmt_token = civic;
		}
	}

	hapd->openwrt_stats.rrm.neighbor_report_tx++;

	hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);
}


static void hostapd_handle_nei_report_req(struct hostapd_data *hapd,
					  const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	const u8 *pos, *ie, *end;
	struct wpa_ssid_value ssid = {
		.ssid_len = 0
	};
	u8 token;
	u8 lci = 0, civic = 0; /* Measurement tokens */
	u16 lci_max_age = 0;

	if (!(hapd->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT))
		return;

	end = buf + len;

	token = mgmt->u.action.u.rrm.dialog_token;
	pos = mgmt->u.action.u.rrm.variable;
	len = end - pos;

	ie = get_ie(pos, len, WLAN_EID_SSID);
	if (ie && ie[1] && ie[1] <= SSID_MAX_LEN) {
		ssid.ssid_len = ie[1];
		os_memcpy(ssid.ssid, ie + 2, ssid.ssid_len);
	} else {
		ssid.ssid_len = hapd->conf->ssid.ssid_len;
		os_memcpy(ssid.ssid, hapd->conf->ssid.ssid, ssid.ssid_len);
	}

	while ((ie = get_ie(pos, len, WLAN_EID_MEASURE_REQUEST))) {
		if (ie[1] < 3)
			break;

		wpa_printf(MSG_DEBUG,
			   "Neighbor report request, measure type %u",
			   ie[4]);

		switch (ie[4]) { /* Measurement Type */
		case MEASURE_TYPE_LCI:
			lci = ie[2]; /* Measurement Token */
			lci_max_age = hostapd_parse_location_lci_req_age(ie + 2,
									 ie[1]);
			break;
		case MEASURE_TYPE_LOCATION_CIVIC:
			civic = ie[2]; /* Measurement token */
			break;
		}

		pos = ie + ie[1] + 2;
		len = end - pos;
	}

	hostapd_send_nei_report_resp(hapd, mgmt->sa, token, &ssid, lci, civic,
				     lci_max_age);
}


static void hostapd_link_mesr_rep_timeout_handler(void *eloop_data,
						  void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;

	wpa_printf(MSG_DEBUG,
		   "RRM: Link measurement request (token %u) timed out",
		   hapd->link_measurement_req_token);
	hapd->link_mesr_req_active = 0;
}


static void hostapd_handle_link_mesr_report(struct hostapd_data *hapd,
					    const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;
	const struct rrm_link_measurement_report *report;
	const u8 *pos, *end;
	char report_msg[2 * 8 + 1];

	end = buf + len;
	pos = mgmt->u.action.u.rrm.variable;
	report = (const struct rrm_link_measurement_report *) (pos - 1);
	if (end - (const u8 *) report < (int) sizeof(*report))
		return;

	if (!hapd->link_mesr_req_active ||
	    (hapd->link_measurement_req_token != report->dialog_token)) {
		wpa_printf(MSG_INFO,
			   "Unexpected Link measurement report, token %u",
			   report->dialog_token);
		return;
	}

	hapd->link_mesr_req_active = 0;
	eloop_cancel_timeout(hostapd_link_mesr_rep_timeout_handler, hapd, NULL);

	report_msg[0] = '\0';
	if (wpa_snprintf_hex(report_msg, sizeof(report_msg),
			     pos, end - pos) < 0)
		return;

	wpa_msg(hapd->msg_ctx, MSG_INFO, LINK_MSR_RESP_RX MACSTR " %u %s",
		MAC2STR(mgmt->sa), report->dialog_token, report_msg);
}


void hostapd_handle_radio_measurement(struct hostapd_data *hapd,
				      const u8 *buf, size_t len)
{
	const struct ieee80211_mgmt *mgmt = (const struct ieee80211_mgmt *) buf;

	/*
	 * Check for enough bytes: header + (1B)Category + (1B)Action +
	 * (1B)Dialog Token.
	 */
	if (len < IEEE80211_HDRLEN + 3)
		return;

	wpa_printf(MSG_DEBUG, "Radio measurement frame, action %u from " MACSTR,
		   mgmt->u.action.u.rrm.action, MAC2STR(mgmt->sa));

	switch (mgmt->u.action.u.rrm.action) {
	case WLAN_RRM_RADIO_MEASUREMENT_REPORT:
		hostapd_handle_radio_msmt_report(hapd, buf, len);
		break;
	case WLAN_RRM_NEIGHBOR_REPORT_REQUEST:
		hostapd_handle_nei_report_req(hapd, buf, len);
		break;
	case WLAN_RRM_LINK_MEASUREMENT_REPORT:
		hostapd_ubus_handle_link_measurement(hapd, buf, len);
		hostapd_handle_link_mesr_report(hapd, buf, len);
		break;
	default:
		wpa_printf(MSG_DEBUG, "RRM action %u is not supported",
			   mgmt->u.action.u.rrm.action);
		break;
	}
}


int hostapd_send_lci_req(struct hostapd_data *hapd, const u8 *addr)
{
	struct wpabuf *buf;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	int ret;

	if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_INFO,
			   "Request LCI: Destination address is not connected");
		return -1;
	}

	if (!(sta->rrm_enabled_capa[1] & WLAN_RRM_CAPS_LCI_MEASUREMENT)) {
		wpa_printf(MSG_INFO,
			   "Request LCI: Station does not support LCI in RRM");
		return -1;
	}

	if (hapd->lci_req_active) {
		wpa_printf(MSG_DEBUG,
			   "Request LCI: LCI request is already in process, overriding");
		hapd->lci_req_active = 0;
		eloop_cancel_timeout(hostapd_lci_rep_timeout_handler, hapd,
				     NULL);
	}

	/* Measurement request (5) + Measurement element with LCI (10) */
	buf = wpabuf_alloc(5 + 10);
	if (!buf)
		return -1;

	hapd->lci_req_token++;
	/* For wraparounds - the token must be nonzero */
	if (!hapd->lci_req_token)
		hapd->lci_req_token++;

	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_RADIO_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, hapd->lci_req_token);
	wpabuf_put_le16(buf, 0); /* Number of repetitions */

	wpabuf_put_u8(buf, WLAN_EID_MEASURE_REQUEST);
	wpabuf_put_u8(buf, 3 + 1 + 4);

	wpabuf_put_u8(buf, 1); /* Measurement Token */
	/*
	 * Parallel and Enable bits are 0, Duration, Request, and Report are
	 * reserved.
	 */
	wpabuf_put_u8(buf, 0);
	wpabuf_put_u8(buf, MEASURE_TYPE_LCI);

	wpabuf_put_u8(buf, LOCATION_SUBJECT_REMOTE);

	wpabuf_put_u8(buf, LCI_REQ_SUBELEM_MAX_AGE);
	wpabuf_put_u8(buf, 2);
	wpabuf_put_le16(buf, 0xffff);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);
	if (ret)
		return ret;

	hapd->lci_req_active = 1;

	eloop_register_timeout(HOSTAPD_RRM_REQUEST_TIMEOUT, 0,
			       hostapd_lci_rep_timeout_handler, hapd, NULL);

	return 0;
}


int hostapd_send_range_req(struct hostapd_data *hapd, const u8 *addr,
			   u16 random_interval, u8 min_ap,
			   const u8 *responders, unsigned int n_responders)
{
	struct wpabuf *buf;
	struct sta_info *sta;
	u8 *len;
	unsigned int i;
	int ret;

	wpa_printf(MSG_DEBUG, "Request range: dest addr " MACSTR
		   " rand interval %u min AP %u n_responders %u", MAC2STR(addr),
		   random_interval, min_ap, n_responders);

	if (min_ap == 0 || min_ap > n_responders) {
		wpa_printf(MSG_INFO, "Request range: Wrong min AP count");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_INFO,
			   "Request range: Destination address is not connected");
		return -1;
	}

	if (!(sta->rrm_enabled_capa[4] & WLAN_RRM_CAPS_FTM_RANGE_REPORT)) {
		wpa_printf(MSG_ERROR,
			   "Request range: Destination station does not support FTM range report in RRM");
		return -1;
	}

	if (hapd->range_req_active) {
		wpa_printf(MSG_DEBUG,
			   "Request range: Range request is already in process; overriding");
		hapd->range_req_active = 0;
		eloop_cancel_timeout(hostapd_range_rep_timeout_handler, hapd,
				     NULL);
	}

	/* Action + measurement type + token + reps + EID + len = 7 */
	buf = wpabuf_alloc(7 + 255);
	if (!buf)
		return -1;

	hapd->range_req_token++;
	if (!hapd->range_req_token) /* For wraparounds */
		hapd->range_req_token++;

	/* IEEE Std 802.11-2024, 9.6.6.2 (Radio Measurement Request frame
	 * format) */
	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_RADIO_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, hapd->range_req_token); /* Dialog Token */
	wpabuf_put_le16(buf, 0); /* Number of Repetitions */

	/* IEEE Std 802.11-2024, 9.4.2.19 (Measurement Request element) */
	wpabuf_put_u8(buf, WLAN_EID_MEASURE_REQUEST);
	len = wpabuf_put(buf, 1); /* Length will be set later */

	wpabuf_put_u8(buf, 1); /* Measurement Token */
	/*
	 * Parallel and Enable bits are 0; Duration, Request, and Report are
	 * reserved.
	 */
	wpabuf_put_u8(buf, 0); /* Measurement Request Mode */
	wpabuf_put_u8(buf, MEASURE_TYPE_FTM_RANGE); /* Measurement Type */

	/* IEEE Std 802.11-2024, 9.4.2.19.19 (FTM Range request) */
	wpabuf_put_le16(buf, random_interval); /* Randomization Interval */
	wpabuf_put_u8(buf, min_ap); /* Minimum AP Count */

	/* FTM Range Subelements */

	/*
	 * Taking the neighbor report part of the range request from neighbor
	 * database instead of requesting the separate bits of data from the
	 * user.
	 */
	for (i = 0; i < n_responders; i++) {
		struct hostapd_neighbor_entry *nr;

		nr = hostapd_neighbor_get(hapd, responders + ETH_ALEN * i,
					  NULL);
		if (!nr) {
			wpa_printf(MSG_INFO, "Missing neighbor report for "
				   MACSTR, MAC2STR(responders + ETH_ALEN * i));
			wpabuf_free(buf);
			return -1;
		}

		if (wpabuf_tailroom(buf) < 2 + wpabuf_len(nr->nr)) {
			wpa_printf(MSG_ERROR, "Too long range request");
			wpabuf_free(buf);
			return -1;
		}

		wpabuf_put_u8(buf, WLAN_EID_NEIGHBOR_REPORT);
		wpabuf_put_u8(buf, wpabuf_len(nr->nr));
		wpabuf_put_buf(buf, nr->nr);
	}

	/* Action + measurement type + token + reps + EID + len = 7 */
	*len = wpabuf_len(buf) - 7;

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);
	if (ret)
		return ret;

	hapd->range_req_active = 1;

	eloop_register_timeout(HOSTAPD_RRM_REQUEST_TIMEOUT, 0,
			       hostapd_range_rep_timeout_handler, hapd, NULL);

	return 0;
}


void hostapd_clean_rrm(struct hostapd_data *hapd)
{
	hostapd_free_neighbor_db(hapd);
	eloop_cancel_timeout(hostapd_lci_rep_timeout_handler, hapd, NULL);
	hapd->lci_req_active = 0;
	eloop_cancel_timeout(hostapd_range_rep_timeout_handler, hapd, NULL);
	hapd->range_req_active = 0;
	eloop_cancel_timeout(hostapd_link_mesr_rep_timeout_handler, hapd, NULL);
	eloop_cancel_timeout(hostapd_chan_load_rep_timeout_handler, hapd, NULL);
	hapd->chan_load_req_active = 0;
	eloop_cancel_timeout(hostapd_noise_hist_rep_timeout_handler, hapd, NULL);
	hapd->noise_hist_req_active = 0;
	hostapd_free_bcn_report_db(hapd);
}


int hostapd_send_beacon_req(struct hostapd_data *hapd, const u8 *addr,
			    u8 req_mode, const struct wpabuf *req)
{
	struct wpabuf *buf;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	struct hostapd_data *link_hapd = hapd;
	int ret;
	enum beacon_report_mode mode;
	const u8 *pos;

	/* Request data:
	 * Operating Class (1), Channel Number (1), Randomization Interval (2),
	 * Measurement Duration (2), Measurement Mode (1), BSSID (6),
	 * Optional Subelements (variable)
	 */
	if (wpabuf_len(req) < 13) {
		wpa_printf(MSG_INFO, "Beacon request: Too short request data");
		return -1;
	}
	pos = wpabuf_head(req);
	mode = pos[6];

	if (!sta) {
		if (hapd->mld)
			sta = ap_get_link_sta(hapd, addr);
#ifdef CONFIG_IEEE80211BE
		/* In MLO, sta_info may be on a partner link hapd.
		 * Walk all MLD links to find the STA and track its hapd. */
		if (!sta && hapd->mld) {
			struct hostapd_data *lhapd;

			for_each_mld_link(lhapd, hapd) {
				sta = ap_get_sta(lhapd, addr);
				if (!sta)
					sta = ap_get_link_sta(lhapd, addr);
				if (sta) {
					link_hapd = lhapd;
					break;
				}
			}
		}
#endif /* CONFIG_IEEE80211BE */
		if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
			wpa_printf(MSG_INFO,
				   "Beacon request: " MACSTR " is not connected",
				   MAC2STR(addr));
			return -1;
		}
	}

	switch (mode) {
	case BEACON_REPORT_MODE_PASSIVE:
		if (!(sta->rrm_enabled_capa[0] &
		      WLAN_RRM_CAPS_BEACON_REPORT_PASSIVE)) {
			wpa_printf(MSG_INFO,
				   "Beacon request: " MACSTR
				   " does not support passive beacon report",
				   MAC2STR(addr));
			return -1;
		}
		break;
	case BEACON_REPORT_MODE_ACTIVE:
		if (!(sta->rrm_enabled_capa[0] &
		      WLAN_RRM_CAPS_BEACON_REPORT_ACTIVE)) {
			wpa_printf(MSG_INFO,
				   "Beacon request: " MACSTR
				   " does not support active beacon report",
				   MAC2STR(addr));
			return -1;
		}
		break;
	case BEACON_REPORT_MODE_TABLE:
		if (!(sta->rrm_enabled_capa[0] &
		      WLAN_RRM_CAPS_BEACON_REPORT_TABLE)) {
			wpa_printf(MSG_INFO,
				   "Beacon request: " MACSTR
				   " does not support table beacon report",
				   MAC2STR(addr));
			return -1;
		}
		break;
	default:
		wpa_printf(MSG_INFO,
			   "Beacon request: Unknown measurement mode %d", mode);
		return -1;
	}

	buf = wpabuf_alloc(5 + 2 + 3 + wpabuf_len(req));
	if (!buf)
		return -1;

	link_hapd->beacon_req_token++;
	if (!link_hapd->beacon_req_token)
		link_hapd->beacon_req_token++;

	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_RADIO_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, link_hapd->beacon_req_token);
	wpabuf_put_le16(buf, 0); /* Number of repetitions */

	/* Measurement Request element */
	wpabuf_put_u8(buf, WLAN_EID_MEASURE_REQUEST);
	wpabuf_put_u8(buf, 3 + wpabuf_len(req));
	wpabuf_put_u8(buf, 1); /* Measurement Token */
	wpabuf_put_u8(buf, req_mode); /* Measurement Request Mode */
	wpabuf_put_u8(buf, MEASURE_TYPE_BEACON); /* Measurement Type */
	wpabuf_put_buf(buf, req);

	/* flush previous Beacon reports before sending new Beacon
	 * Report Request.
	 */
	hostapd_free_bcn_report_db(link_hapd);

	ret = hostapd_drv_send_action(link_hapd, link_hapd->iface->freq, 0,
				      sta->addr, wpabuf_head(buf),
				      wpabuf_len(buf));
	wpabuf_free(buf);
	if (ret < 0)
		return ret;

	return link_hapd->beacon_req_token;
}


void hostapd_rrm_beacon_req_tx_status(struct hostapd_data *hapd,
				      const struct ieee80211_mgmt *mgmt,
				      size_t len, int ok)
{
	if (len < 24 + 3)
		return;
	wpa_msg(hapd->msg_ctx, MSG_INFO, BEACON_REQ_TX_STATUS MACSTR
		" %u ack=%d", MAC2STR(mgmt->da),
		mgmt->u.action.u.rrm.dialog_token, ok);
}


static void hostapd_noise_hist_rep_timeout_handler(void *eloop_data,
						   void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;

	wpa_printf(MSG_DEBUG,
		   "RRM: Noise Histogram request (token %u) timed out",
		   hapd->noise_hist_req_token);
	hapd->noise_hist_req_active = 0;
}


static void hostapd_handle_noise_histogram_report(struct hostapd_data *hapd,
						  const u8 *addr, u8 token,
						  u8 rep_mode, const u8 *pos,
						  size_t len)
{
	s8 anpi;
	int i;

	if (!hapd->noise_hist_req_active ||
	    hapd->noise_hist_req_token != token) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Unexpected Noise Histogram report, token %u",
			   token);
		return;
	}

	hapd->noise_hist_req_active = 0;
	eloop_cancel_timeout(hostapd_noise_hist_rep_timeout_handler, hapd,
			     NULL);

	/*
	 * Noise Histogram Report body (IEEE Std 802.11-2020, 9.4.2.22.3):
	 * pos[0]=token, pos[1]=mode, pos[2]=type, pos[3..]=report body.
	 * Report body: op_class(1) chan(1) start_time(8) duration(2)
	 * antenna_id(1) anpi(1) ipi[0..7](8) = 22 bytes;
	 * total with header = 3+22 = 25.
	 */
	if (len < 3 + 22) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Noise Histogram report too short (len=%zu)",
			   len);
		return;
	}

	if (rep_mode & (MEASUREMENT_REPORT_MODE_REJECT_INCAPABLE |
			MEASUREMENT_REPORT_MODE_REJECT_REFUSED)) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Noise Histogram report refused/incapable from "
			   MACSTR " mode=0x%02x",
			   MAC2STR(addr), rep_mode);
		return;
	}

	/*
	 * Report body offsets (pos[3] = start of body):
	 * op_class(1) + chan(1) + start_time(8) + duration(2) + antenna_id(1)
	 * = 13 bytes before anpi; ipi[0..7] follow at pos[3+14..3+21]
	 */
	anpi = (s8) pos[3 + 13];

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		NOISE_HIST_RESP_RX MACSTR " %u %d %u %u %u %u %u %u %u %u",
		MAC2STR(addr), token, (int) anpi,
		pos[3 + 14], pos[3 + 15], pos[3 + 16], pos[3 + 17],
		pos[3 + 18], pos[3 + 19], pos[3 + 20], pos[3 + 21]);

	wpa_printf(MSG_DEBUG,
		   "RRM: Noise Histogram from " MACSTR
		   " token=%u ANPI=%d dBm IPI=[%u %u %u %u %u %u %u %u]",
		   MAC2STR(addr), token, (int) anpi,
		   pos[3 + 14], pos[3 + 15], pos[3 + 16], pos[3 + 17],
		   pos[3 + 18], pos[3 + 19], pos[3 + 20], pos[3 + 21]);

	(void) i; /* suppress unused-variable warning */
}


static void hostapd_chan_load_rep_timeout_handler(void *eloop_data,
						  void *user_ctx)
{
	struct hostapd_data *hapd = eloop_data;

	wpa_printf(MSG_DEBUG,
		   "RRM: Channel Load request (token %u) timed out",
		   hapd->chan_load_req_token);
	hapd->chan_load_req_active = 0;
}


static void hostapd_handle_channel_load_report(struct hostapd_data *hapd,
					       const u8 *addr, u8 token,
					       u8 rep_mode, const u8 *pos,
					       size_t len)
{
	u8 channel_load;

	if (!hapd->chan_load_req_active ||
	    hapd->chan_load_req_token != token) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Unexpected Channel Load report, token %u",
			   token);
		return;
	}

	hapd->chan_load_req_active = 0;
	eloop_cancel_timeout(hostapd_chan_load_rep_timeout_handler, hapd, NULL);

	/*
	 * Channel Load Report body (IEEE Std 802.11-2020, 9.4.2.22.4):
	 * pos[0]=token, pos[1]=mode, pos[2]=type, pos[3..]=report body.
	 * Report body: op_class(1) chan(1) start_time(8) duration(2)
	 * channel_load(1) = 13 bytes; total with header = 3+13 = 16.
	 */
	if (len < 3 + 13) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Channel Load report too short (len=%zu)",
			   len);
		return;
	}

	if (rep_mode & (MEASUREMENT_REPORT_MODE_REJECT_INCAPABLE |
			MEASUREMENT_REPORT_MODE_REJECT_REFUSED)) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Channel Load report refused/incapable from "
			   MACSTR " mode=0x%02x",
			   MAC2STR(addr), rep_mode);
		return;
	}

	/* op_class(1) + chan(1) + start_time(8) + duration(2) = 12 bytes
	 * before channel_load in the report body (pos[3] = start of body) */
	channel_load = pos[3 + 12];

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		CHANNEL_LOAD_RESP_RX MACSTR " %u %u",
		MAC2STR(addr), token, channel_load);
}


int hostapd_send_channel_load_req(struct hostapd_data *hapd, const u8 *addr,
				  u8 op_class, u8 channel,
				  u16 random_interval, u16 duration)
{
	struct wpabuf *buf;
	struct sta_info *sta;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "RRM: Channel Load request: dest=" MACSTR
		   " op_class=%u channel=%u rand_interval=%u duration=%u",
		   MAC2STR(addr), op_class, channel, random_interval, duration);

	sta = ap_get_sta(hapd, addr);
	if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_INFO,
			   "RRM: Channel Load request: " MACSTR
			   " is not connected", MAC2STR(addr));
		return -1;
	}

	if (!(sta->rrm_enabled_capa[0] & WLAN_RRM_CAPS_CHANNEL_LOAD)) {
		wpa_printf(MSG_INFO,
			   "RRM: Channel Load request: " MACSTR
			   " does not support Channel Load measurement",
			   MAC2STR(addr));
		return -1;
	}

	if (hapd->chan_load_req_active) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Channel Load request already in progress - overriding");
		hapd->chan_load_req_active = 0;
		eloop_cancel_timeout(hostapd_chan_load_rep_timeout_handler,
				     hapd, NULL);
	}

	/*
	 * Action(1) + Action type(1) + Dialog Token(1) + Repetitions(2) +
	 * Measurement Request IE header(2) + token(1) + mode(1) + type(1) +
	 * Channel Load Request body: op_class(1) + channel(1) +
	 * rand_interval(2) + duration(2) = 16 bytes total
	 */
	buf = wpabuf_alloc(16);
	if (!buf)
		return -1;

	hapd->chan_load_req_token++;
	if (!hapd->chan_load_req_token)
		hapd->chan_load_req_token++;

	/* IEEE Std 802.11-2020, 9.6.6.2 - Radio Measurement Request frame */
	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_RADIO_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, hapd->chan_load_req_token); /* Dialog Token */
	wpabuf_put_le16(buf, 0); /* Number of Repetitions */

	/* IEEE Std 802.11-2020, 9.4.2.19 - Measurement Request element */
	wpabuf_put_u8(buf, WLAN_EID_MEASURE_REQUEST);
	wpabuf_put_u8(buf, 3 + 6); /* len: token+mode+type + body */
	wpabuf_put_u8(buf, 1); /* Measurement Token */
	wpabuf_put_u8(buf, 0); /* Measurement Request Mode */
	wpabuf_put_u8(buf, MEASURE_TYPE_CHANNEL_LOAD); /* Measurement Type */

	/* IEEE Std 802.11-2020, 9.4.2.19.4 - Channel Load request body */
	wpabuf_put_u8(buf, op_class);
	wpabuf_put_u8(buf, channel);
	wpabuf_put_le16(buf, random_interval);
	wpabuf_put_le16(buf, duration);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);
	if (ret)
		return ret;

	hapd->chan_load_req_active = 1;

	eloop_register_timeout(HOSTAPD_RRM_REQUEST_TIMEOUT, 0,
			       hostapd_chan_load_rep_timeout_handler, hapd,
			       NULL);

	return hapd->chan_load_req_token;
}


int hostapd_send_noise_histogram_req(struct hostapd_data *hapd, const u8 *addr,
				     u8 op_class, u8 channel,
				     u16 random_interval, u16 duration)
{
	struct wpabuf *buf;
	struct sta_info *sta;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "RRM: Noise Histogram request: dest=" MACSTR
		   " op_class=%u channel=%u rand_interval=%u duration=%u",
		   MAC2STR(addr), op_class, channel, random_interval, duration);

	sta = ap_get_sta(hapd, addr);
	if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_INFO,
			   "RRM: Noise Histogram request: " MACSTR
			   " is not connected", MAC2STR(addr));
		return -1;
	}

	if (!(sta->rrm_enabled_capa[1] & WLAN_RRM_CAPS_NOISE_HISTOGRAM)) {
		wpa_printf(MSG_INFO,
			   "RRM: Noise Histogram request: " MACSTR
			   " does not support Noise Histogram measurement",
			   MAC2STR(addr));
		return -1;
	}

	if (hapd->noise_hist_req_active) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Noise Histogram request already in progress - overriding");
		hapd->noise_hist_req_active = 0;
		eloop_cancel_timeout(hostapd_noise_hist_rep_timeout_handler,
				     hapd, NULL);
	}

	/*
	 * Action(1) + Action type(1) + Dialog Token(1) + Repetitions(2) +
	 * Measurement Request IE header(2) + token(1) + mode(1) + type(1) +
	 * Noise Histogram Request body: op_class(1) + channel(1) +
	 * rand_interval(2) + duration(2) = 16 bytes total
	 */
	buf = wpabuf_alloc(16);
	if (!buf)
		return -1;

	hapd->noise_hist_req_token++;
	if (!hapd->noise_hist_req_token)
		hapd->noise_hist_req_token++;

	/* IEEE Std 802.11-2020, 9.6.6.2 - Radio Measurement Request frame */
	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_RADIO_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, hapd->noise_hist_req_token); /* Dialog Token */
	wpabuf_put_le16(buf, 0); /* Number of Repetitions */

	/* IEEE Std 802.11-2020, 9.4.2.19 - Measurement Request element */
	wpabuf_put_u8(buf, WLAN_EID_MEASURE_REQUEST);
	wpabuf_put_u8(buf, 3 + 6); /* len: token+mode+type + body */
	wpabuf_put_u8(buf, hapd->noise_hist_req_token); /* Measurement Token */
	wpabuf_put_u8(buf, 0); /* Measurement Request Mode */
	wpabuf_put_u8(buf, MEASURE_TYPE_NOISE_HIST); /* Measurement Type */

	/* IEEE Std 802.11-2020, 9.4.2.19.3 - Noise Histogram request body */
	wpabuf_put_u8(buf, op_class);
	wpabuf_put_u8(buf, channel);
	wpabuf_put_le16(buf, random_interval);
	wpabuf_put_le16(buf, duration);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);
	if (ret)
		return ret;

	hapd->noise_hist_req_active = 1;

	eloop_register_timeout(HOSTAPD_RRM_REQUEST_TIMEOUT, 0,
			       hostapd_noise_hist_rep_timeout_handler, hapd,
			       NULL);

	return hapd->noise_hist_req_token;
}


int hostapd_send_link_measurement_req(struct hostapd_data *hapd, const u8 *addr)
{
	struct wpabuf *buf;
	struct sta_info *sta;
	int ret;

	wpa_printf(MSG_DEBUG, "Request Link Measurement: dest addr " MACSTR,
		   MAC2STR(addr));

	if (!(hapd->iface->drv_rrm_flags &
	      WPA_DRIVER_FLAGS_TX_POWER_INSERTION)) {
		wpa_printf(MSG_INFO,
			   "Request Link Measurement: the driver does not support TX power insertion");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta || !(sta->flags & WLAN_STA_AUTHORIZED)) {
		wpa_printf(MSG_INFO,
			   "Request Link Measurement: specied STA is not connected");
		return -1;
	}

	if (!(sta->rrm_enabled_capa[0] & WLAN_RRM_CAPS_LINK_MEASUREMENT)) {
		wpa_printf(MSG_INFO,
			   "Request Link Measurement: destination STA does not support link measurement");
		return -1;
	}

	if (hapd->link_mesr_req_active) {
		wpa_printf(MSG_DEBUG,
			   "Request Link Measurement: request already in process - overriding");
		hapd->link_mesr_req_active = 0;
		eloop_cancel_timeout(hostapd_link_mesr_rep_timeout_handler,
				     hapd, NULL);
	}

	/* Action + Action type + token + Tx Power used + Max Tx Power = 5 */
	buf = wpabuf_alloc(5);
	if (!buf)
		return -1;

	hapd->link_measurement_req_token++;
	if (!hapd->link_measurement_req_token)
		hapd->link_measurement_req_token++;

	wpabuf_put_u8(buf, WLAN_ACTION_RADIO_MEASUREMENT);
	wpabuf_put_u8(buf, WLAN_RRM_LINK_MEASUREMENT_REQUEST);
	wpabuf_put_u8(buf, hapd->link_measurement_req_token);
	/* NOTE: The driver is expected to fill the Tx Power Used and Max Tx
	 * Power */
	wpabuf_put_u8(buf, 0);
	wpabuf_put_u8(buf, 0);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);
	if (ret < 0)
		return ret;

	hapd->link_mesr_req_active = 1;

	eloop_register_timeout(HOSTAPD_RRM_REQUEST_TIMEOUT, 0,
			       hostapd_link_mesr_rep_timeout_handler, hapd,
			       NULL);

	return hapd->link_measurement_req_token;
}
