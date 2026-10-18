/*
 * hostapd_mqtt.c - hostapd-specific MQTT command/event handling
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/mqtt_eloop.h"
#include "utils/mqtt_feature_map.h"
#include "hostapd.h"
#include "hostapd_mqtt.h"
#include "beacon.h"
#include "neighbor_db.h"
#ifdef CONFIG_IEEE80211BN
#include "eth_p_1905.h"
#endif /* CONFIG_IEEE80211BN */

#define EZHIF_STATUS_OK            0
#define EZHIF_STATUS_INVALID_PARAM 1
#define EZHIF_STATUS_NOT_FOUND     2
#define EZHIF_STATUS_NO_RESOURCE   3
#define EZHIF_STATUS_INTERNAL      4

static struct mqtt_eloop_ctx *hostapd_mqtt_ctx(const struct hostapd_data *hapd)
{
	if (!hapd || !hapd->iface || !hapd->iface->interfaces)
		return NULL;
	return hapd->iface->interfaces->mqtt_ctx;
}

static void mqtt_publish_simple_status(struct hostapd_data *hapd,
				       uint16_t evt_id,
				       uint16_t tlv_status,
				       u8 status)
{
	struct mqtt_tlv_message *resp;
	char topic[64];
	u8 out[128];
	int len;

	resp = mqtt_tlv_message_alloc(evt_id);
	if (!resp)
		return;

	mqtt_tlv_add_u8(resp, tlv_status, status);
	mqtt_build_transmit_topic(MQTT_FEATURE_SMD, topic, sizeof(topic));
	len = mqtt_tlv_serialize(resp, out, sizeof(out));
	if (len > 0) {
		wpa_printf(MSG_INFO,
			   "MQTT: publish status evt=0x%04x status=%u topic=%s len=%d",
			   evt_id, status, topic, len);
		mqtt_eloop_publish(hostapd_mqtt_ctx(hapd), topic, out, len, 0, false);
	}
	mqtt_tlv_message_free(resp);
}

#ifdef CONFIG_IEEE80211BN
static int hostapd_mqtt_add_smd_partner(struct hostapd_data *hapd,
					       const u8 *mld_addr)
{
	struct smd_partner_entry *partner;

	if (!hapd || !hapd->conf || !mld_addr || is_zero_ether_addr(mld_addr))
		return -1;

	for (partner = hapd->conf->smd_partners; partner; partner = partner->next) {
		if (ether_addr_equal(partner->mac_addr, mld_addr))
			return 0;
	}

	partner = os_zalloc(sizeof(*partner));
	if (!partner)
		return -1;

	os_memcpy(partner->mac_addr, mld_addr, ETH_ALEN);
	partner->next = hapd->conf->smd_partners;
	hapd->conf->smd_partners = partner;

	/* MQTT dynamic peers have no key material and are plain-text peers. */
	if (hapd->eth_p_1905_ctx &&
	    eth_p_1905_add_peer(hapd->eth_p_1905_ctx, mld_addr, NULL, false) < 0) {
		wpa_printf(MSG_WARNING,
			   "MQTT: Failed to add runtime plain smd_partner " MACSTR,
			   MAC2STR(mld_addr));
	}

	wpa_printf(MSG_INFO, "MQTT: Added plain smd_partner " MACSTR,
		   MAC2STR(mld_addr));
	return 0;
}
#endif /* CONFIG_IEEE80211BN */

static void hostapd_mqtt_handle_sys_ping(struct hapd_interfaces *interfaces)
{
	struct mqtt_tlv_message *resp;
	char topic[64];
	uint8_t buf[256];
	uint16_t num_ifaces = 0;
	size_t i;
	int len;

	for (i = 0; i < interfaces->count; i++)
		num_ifaces += (uint16_t)interfaces->iface[i]->num_bss;
	wpa_printf(MSG_INFO, "MQTT: CMD_SYS_PING received num_ifaces=%u", num_ifaces);

	resp = mqtt_tlv_message_alloc(EVT_ID_SYS_HOSTAPD_STARTED);
	if (!resp)
		return;

	mqtt_tlv_add_string(resp, TLV_SYS_HOSTAPD_STARTED_VERSION, "hostapd-2.10");
	mqtt_tlv_add_u16(resp, TLV_SYS_HOSTAPD_STARTED_NUM_IFACES, num_ifaces);
	mqtt_build_transmit_topic(MQTT_FEATURE_SYS, topic, sizeof(topic));
	len = mqtt_tlv_serialize(resp, buf, sizeof(buf));
	if (len > 0) {
		wpa_printf(MSG_INFO,
			   "MQTT: publish EVT_SYS_HOSTAPD_STARTED topic=%s len=%d",
			   topic, len);
		mqtt_eloop_publish(interfaces->mqtt_ctx, topic, buf, len, 0, false);
	}
	mqtt_tlv_message_free(resp);
}

static void hostapd_mqtt_handle_neighbor_db_set(struct hostapd_data *hapd,
						struct mqtt_tlv_message *msg)
{
	u8 ap_alid[ETH_ALEN], smd_id[ETH_ALEN], tuple_mld_addr[ETH_ALEN];
	u8 tuple_bssid[ETH_ALEN];
	u8 has_smd_id = 0, tuple_has_mld_addr = 0, status = EZHIF_STATUS_OK;
	u16 tuple_nre_len = 0;
	u16 num_nre = 0;
	u16 bare;
	int parse_state;
	int tuple_count = 0;
	int payload_seen = 0;
	int apply_count = 0;
	struct wpabuf *nr_buf = NULL;
	struct wpa_ssid_value ssid;
	struct mqtt_tlv_entry *e;

	enum {
		NR_SET_EXPECT_HAS_MLD = 0,
		NR_SET_EXPECT_MLD_ADDR,
		NR_SET_EXPECT_BSSID,
		NR_SET_EXPECT_NRE_LEN,
		NR_SET_EXPECT_NRE
	};

	wpa_printf(MSG_INFO, "MQTT: CMD_NEIGHBOR_DB_SET received");

	if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_SET_AP_ALID, ap_alid) < 0 ||
	    mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_SET_HAS_SMD_ID, &has_smd_id) < 0 ||
	    mqtt_tlv_get_u16(msg, TLV_NEIGHBOR_DB_SET_NUM_NRE, &num_nre) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto out;
	}

	if (has_smd_id) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_SET_SMD_ID, smd_id) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
	} else {
		os_memset(smd_id, 0, ETH_ALEN);
	}
	if (num_nre == 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto out;
	}
	wpa_printf(MSG_INFO,
		   "NeighborDB SET: ap_alid=" MACSTR " has_smd=%u smd_id=" MACSTR
		   " num_nre=%u",
		   MAC2STR(ap_alid), has_smd_id, MAC2STR(smd_id), num_nre);

	os_memset(&ssid, 0, sizeof(ssid));

	parse_state = NR_SET_EXPECT_HAS_MLD;
	tuple_has_mld_addr = 0;
	os_memset(tuple_mld_addr, 0, ETH_ALEN);
	os_memset(tuple_bssid, 0, ETH_ALEN);
	tuple_nre_len = 0;
	dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list) {
		bare = MQTT_TLV_DECODE_TYPE(e->type);
		if (MQTT_TLV_IS_GLOBAL(bare))
			continue;

		if (bare == TLV_NEIGHBOR_DB_SET_AP_ALID ||
		    bare == TLV_NEIGHBOR_DB_SET_HAS_SMD_ID ||
		    bare == TLV_NEIGHBOR_DB_SET_SMD_ID ||
		    bare == TLV_NEIGHBOR_DB_SET_NUM_NRE) {
			if (payload_seen) {
				status = EZHIF_STATUS_INVALID_PARAM;
				goto out;
			}
			continue;
		}

		payload_seen = 1;
		if (parse_state == NR_SET_EXPECT_HAS_MLD) {
			if (bare != TLV_NEIGHBOR_DB_SET_HAS_MLD_ADDR || e->length != 1) {
				status = EZHIF_STATUS_INVALID_PARAM;
				goto out;
			}
			tuple_has_mld_addr = e->value[0] ? 1 : 0;
			parse_state = tuple_has_mld_addr ? NR_SET_EXPECT_MLD_ADDR :
							   NR_SET_EXPECT_BSSID;
			continue;
		}

		if (parse_state == NR_SET_EXPECT_MLD_ADDR) {
			if (bare != TLV_NEIGHBOR_DB_SET_MLD_ADDR ||
			    e->length != ETH_ALEN) {
				status = EZHIF_STATUS_INVALID_PARAM;
				goto out;
			}
			os_memcpy(tuple_mld_addr, e->value, ETH_ALEN);
			parse_state = NR_SET_EXPECT_BSSID;
			continue;
		}

		if (parse_state == NR_SET_EXPECT_BSSID) {
			if (bare != TLV_NEIGHBOR_DB_SET_BSSID || e->length != ETH_ALEN) {
				status = EZHIF_STATUS_INVALID_PARAM;
				goto out;
			}
			os_memcpy(tuple_bssid, e->value, ETH_ALEN);
			parse_state = NR_SET_EXPECT_NRE_LEN;
			continue;
		}

		if (parse_state == NR_SET_EXPECT_NRE_LEN) {
			if (bare != TLV_NEIGHBOR_DB_SET_NRE_LEN || e->length != 2) {
				status = EZHIF_STATUS_INVALID_PARAM;
				goto out;
			}
			tuple_nre_len = mqtt_get_be16(e->value);
			if (tuple_nre_len < 13 || tuple_nre_len > 255) {
				status = EZHIF_STATUS_INVALID_PARAM;
				goto out;
			}
			parse_state = NR_SET_EXPECT_NRE;
			continue;
		}

		if (bare != TLV_NEIGHBOR_DB_SET_NRE ||
		    e->length != tuple_nre_len ||
		    os_memcmp(e->value, tuple_bssid, ETH_ALEN) != 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}

		tuple_count++;
		wpa_printf(MSG_INFO,
			   "NeighborDB SET: validated tuple[%d] bssid=" MACSTR
			   " has_mld=%u mld=" MACSTR " nre_len=%u",
			   tuple_count - 1, MAC2STR(tuple_bssid), tuple_has_mld_addr,
			   MAC2STR(tuple_mld_addr), tuple_nre_len);
		parse_state = NR_SET_EXPECT_HAS_MLD;
	}

	if (!payload_seen || parse_state != NR_SET_EXPECT_HAS_MLD || tuple_count == 0 ||
	    tuple_count != num_nre) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto out;
	}

	parse_state = NR_SET_EXPECT_HAS_MLD;
	tuple_has_mld_addr = 0;
	os_memset(tuple_mld_addr, 0, ETH_ALEN);
	os_memset(tuple_bssid, 0, ETH_ALEN);
	tuple_nre_len = 0;
	dl_list_for_each(e, &msg->tlvs, struct mqtt_tlv_entry, list) {
		bare = MQTT_TLV_DECODE_TYPE(e->type);

		if (MQTT_TLV_IS_GLOBAL(bare))
			continue;
		if (bare == TLV_NEIGHBOR_DB_SET_AP_ALID ||
		    bare == TLV_NEIGHBOR_DB_SET_HAS_SMD_ID ||
		    bare == TLV_NEIGHBOR_DB_SET_SMD_ID ||
		    bare == TLV_NEIGHBOR_DB_SET_NUM_NRE)
			continue;

		if (parse_state == NR_SET_EXPECT_HAS_MLD) {
			tuple_has_mld_addr = e->value[0] ? 1 : 0;
			parse_state = tuple_has_mld_addr ? NR_SET_EXPECT_MLD_ADDR :
							   NR_SET_EXPECT_BSSID;
			continue;
		}
		if (parse_state == NR_SET_EXPECT_MLD_ADDR) {
			os_memcpy(tuple_mld_addr, e->value, ETH_ALEN);
			parse_state = NR_SET_EXPECT_BSSID;
			continue;
		}
		if (parse_state == NR_SET_EXPECT_BSSID) {
			os_memcpy(tuple_bssid, e->value, ETH_ALEN);
			parse_state = NR_SET_EXPECT_NRE_LEN;
			continue;
		}
		if (parse_state == NR_SET_EXPECT_NRE_LEN) {
			tuple_nre_len = mqtt_get_be16(e->value);
			parse_state = NR_SET_EXPECT_NRE;
			continue;
		}

		nr_buf = wpabuf_alloc_copy(e->value, tuple_nre_len);
		if (!nr_buf) {
			status = EZHIF_STATUS_NO_RESOURCE;
			goto out;
		}

		if (hostapd_neighbor_set_mld(hapd, tuple_bssid, &ssid, nr_buf,
					     tuple_has_mld_addr ? tuple_mld_addr : NULL,
					     has_smd_id ? smd_id : NULL,
					     NULL, NULL, 0, 0) < 0) {
			status = EZHIF_STATUS_INTERNAL;
			goto out;
		}
		wpa_printf(MSG_INFO,
			   "NeighborDB SET: applied tuple[%d] bssid=" MACSTR
			   " has_mld=%u mld=" MACSTR " nre_len=%u",
			   apply_count, MAC2STR(tuple_bssid), tuple_has_mld_addr,
			   MAC2STR(tuple_mld_addr), tuple_nre_len);

#ifdef CONFIG_IEEE80211BN
		if (tuple_has_mld_addr) {
			bool same_smd = false;

			if (hapd->conf->smd.enabled &&
			    has_smd_id &&
			    !is_zero_ether_addr(smd_id) &&
			    !is_zero_ether_addr(hapd->conf->smd.smd_identifier) &&
			    ether_addr_equal(smd_id, hapd->conf->smd.smd_identifier))
				same_smd = true;

			if (same_smd) {
				if (hostapd_mqtt_add_smd_partner(hapd,
								       tuple_mld_addr) < 0) {
					status = EZHIF_STATUS_NO_RESOURCE;
					goto out;
				}
			} else {
				wpa_printf(MSG_INFO,
					   "MQTT: Skip smd_partner add for MLD " MACSTR
					   " (incoming_smd=" MACSTR
					   " own_smd=" MACSTR " has_smd=%u)",
					   MAC2STR(tuple_mld_addr), MAC2STR(smd_id),
					   MAC2STR(hapd->conf->smd.smd_identifier),
					   has_smd_id);
			}
		}
#endif /* CONFIG_IEEE80211BN */

		wpabuf_free(nr_buf);
		nr_buf = NULL;
		apply_count++;
		tuple_has_mld_addr = 0;
		os_memset(tuple_mld_addr, 0, ETH_ALEN);
		parse_state = NR_SET_EXPECT_HAS_MLD;
	}

	if (apply_count > 0)
		ieee802_11_set_beacon(hapd);

out:
	wpa_printf(MSG_INFO,
		   "NeighborDB SET: complete status=%u applied=%d validated=%d",
		   status, apply_count, tuple_count);
	mqtt_publish_simple_status(hapd, EVT_ID_NEIGHBOR_DB_SET_RESP,
				   TLV_NEIGHBOR_DB_SET_RESP_STATUS, status);
	wpabuf_free(nr_buf);
}

static int
hostapd_mqtt_collect_by_smd_id(struct hapd_interfaces *interfaces,
		const u8 *smd_id, u8 get_global_entries,
		struct hostapd_neighbor_entry **out_entries,
		size_t out_max)
{
	struct hostapd_neighbor_entry *tmp_entries[256];
	int total_filtered = 0;
	size_t out_count = 0;
	size_t i;

	if (!interfaces || !smd_id || !out_entries)
		return 0;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];
		size_t j;

		if (!iface || !iface->bss)
			continue;

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *bss_hapd = iface->bss[j];
			int raw_count;
			int k;

			if (!bss_hapd)
				continue;

			os_memset(tmp_entries, 0, sizeof(tmp_entries));
			raw_count = hostapd_neighbor_get_all_by_smd_id(
				bss_hapd, smd_id, tmp_entries,
				ARRAY_SIZE(tmp_entries));

			for (k = 0; k < raw_count &&
			     k < (int) ARRAY_SIZE(tmp_entries); k++) {
				struct hostapd_neighbor_entry *nr = tmp_entries[k];

				if (!nr)
					continue;
				if (get_global_entries) {
					if (nr->self_entry) {
						continue;
					}
				} else if (!nr->self_entry) {
					continue;
				}

				if (out_count < out_max)
					out_entries[out_count++] = nr;
				total_filtered++;
			}
		}
	}
	return total_filtered;
}

static void hostapd_mqtt_handle_neighbor_db_get(struct hapd_interfaces *interfaces,
						struct hostapd_data *hapd,
						struct mqtt_tlv_message *msg)
{
	u8 ap_alid[ETH_ALEN], smd_id[ETH_ALEN], bssid[ETH_ALEN], mld_addr[ETH_ALEN];
	u8 has_smd_id = 0, get_global_entries = 0;
	u8 has_mld_addr = 0, has_bssid = 0, status = EZHIF_STATUS_OK;
	struct mqtt_tlv_message *resp;
	struct hostapd_neighbor_entry *nr_single = NULL;
	struct hostapd_neighbor_entry *nr_array[256];
	int nr_count = 0;
	char topic[64];
	u8 out[8192];
	int len;
	int i;

	wpa_printf(MSG_INFO, "MQTT: CMD_NEIGHBOR_DB_GET received");
	os_memset(nr_array, 0, sizeof(nr_array));

	if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_GET_AP_ALID, ap_alid) < 0 ||
	    mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_GET_HAS_SMD_ID, &has_smd_id) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto build_resp;
	}

	if (has_smd_id) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_GET_SMD_ID, smd_id) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto build_resp;
		}
		if (mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_GET_GLOBAL_ENTRIES,
				    &get_global_entries) < 0)
			get_global_entries = 0;
	} else {
		os_memset(smd_id, 0, ETH_ALEN);
		get_global_entries = 0;
	}

	if (mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_GET_HAS_MLD_ADDR, &has_mld_addr) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto build_resp;
	}
	if (has_mld_addr) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_GET_MLD_ADDR, mld_addr) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto build_resp;
		}
	} else {
		os_memset(mld_addr, 0, ETH_ALEN);
	}

	if (mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_GET_HAS_BSSID, &has_bssid) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto build_resp;
	}
	if (has_bssid) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_GET_BSSID, bssid) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto build_resp;
		}
	} else {
		os_memset(bssid, 0, ETH_ALEN);
	}
	wpa_printf(MSG_INFO,
		   "NeighborDB GET: ap_alid=" MACSTR " has_smd=%u smd_id=" MACSTR
		   " get_global=%u has_mld=%u mld=" MACSTR " has_bssid=%u bssid=" MACSTR,
		   MAC2STR(ap_alid), has_smd_id, MAC2STR(smd_id), get_global_entries,
		   has_mld_addr, MAC2STR(mld_addr), has_bssid, MAC2STR(bssid));

	if (has_smd_id) {
		if (is_zero_ether_addr(smd_id)) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto build_resp;
		}
		nr_count = hostapd_mqtt_collect_by_smd_id(interfaces, smd_id,
                                                           get_global_entries,
                                                           nr_array,
                                                           ARRAY_SIZE(nr_array));
		wpa_printf(MSG_INFO, "NeighborDB GET: scope=SMD result_count=%d", nr_count);
	} else if (has_mld_addr) {
		if (is_zero_ether_addr(mld_addr)) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto build_resp;
		}
		nr_count = hostapd_neighbor_get_all_by_mld_addr(hapd, mld_addr,
								nr_array,
								ARRAY_SIZE(nr_array));
		wpa_printf(MSG_INFO, "NeighborDB GET: scope=MLD result_count=%d", nr_count);
	} else if (has_bssid) {
		if (is_zero_ether_addr(bssid)) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto build_resp;
		}
		nr_single = hostapd_neighbor_get(hapd, bssid, NULL);
		if (nr_single) {
			nr_array[0] = nr_single;
			nr_count = 1;
		}
		wpa_printf(MSG_INFO, "NeighborDB GET: scope=BSSID result_count=%d", nr_count);
	} else {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto build_resp;
	}

build_resp:
	resp = mqtt_tlv_message_alloc(EVT_ID_NEIGHBOR_DB_GET_RESP);
	if (!resp)
		return;

	if (status != EZHIF_STATUS_OK) {
		mqtt_tlv_add_u8(resp, TLV_NEIGHBOR_DB_GET_RESP_STATUS, status);
		mqtt_tlv_add_u16(resp, TLV_NEIGHBOR_DB_GET_RESP_NRE_COUNT, 0);
	} else if (nr_count == 0) {
		mqtt_tlv_add_u8(resp, TLV_NEIGHBOR_DB_GET_RESP_STATUS, EZHIF_STATUS_NOT_FOUND);
		mqtt_tlv_add_u16(resp, TLV_NEIGHBOR_DB_GET_RESP_NRE_COUNT, 0);
	} else {
		u16 actual_count = nr_count > (int)ARRAY_SIZE(nr_array) ?
				   (u16)ARRAY_SIZE(nr_array) : (u16)nr_count;
		mqtt_tlv_add_u8(resp, TLV_NEIGHBOR_DB_GET_RESP_STATUS, EZHIF_STATUS_OK);
		mqtt_tlv_add_u16(resp, TLV_NEIGHBOR_DB_GET_RESP_NRE_COUNT, actual_count);

		for (i = 0; i < (int)actual_count; i++) {
			struct hostapd_neighbor_entry *nr = nr_array[i];
			struct mqtt_tlv_entry *container_entry;
			u16 nre_len;

			if (!nr || !nr->nr)
				continue;
			nre_len = (u16) wpabuf_len(nr->nr);

			container_entry = mqtt_tlv_add_container(resp,
								 TLV_NEIGHBOR_DB_GET_RESP_NRE_ENTRY);
			if (!container_entry)
				continue;
			mqtt_tlv_container_add_u16(container_entry,
						   TLV_NEIGHBOR_DB_GET_RESP_ENTRY_NRE_LEN,
						   nre_len);
			mqtt_tlv_container_add_binary(container_entry,
						      TLV_NEIGHBOR_DB_GET_RESP_ENTRY_NRE,
						      wpabuf_head_u8(nr->nr), nre_len);
		}
	}

	mqtt_build_transmit_topic(MQTT_FEATURE_SMD, topic, sizeof(topic));
	len = mqtt_tlv_serialize(resp, out, sizeof(out));
	if (len > 0) {
		wpa_printf(MSG_INFO,
			   "NeighborDB GET: publish resp status=%u count=%d topic=%s len=%d",
			   status == EZHIF_STATUS_OK ?
			   (nr_count > 0 ? EZHIF_STATUS_OK : EZHIF_STATUS_NOT_FOUND) : status,
			   nr_count, topic, len);
		mqtt_eloop_publish(hostapd_mqtt_ctx(hapd), topic, out, len, 0, false);
	}
	mqtt_tlv_message_free(resp);
}

static void hostapd_mqtt_handle_neighbor_db_clear(struct hostapd_data *hapd,
						  struct mqtt_tlv_message *msg)
{
	u8 ap_alid[ETH_ALEN], smd_id[ETH_ALEN], mld_addr[ETH_ALEN], bssid[ETH_ALEN];
	u8 has_smd_id = 0, has_mld_addr = 0, has_bssid = 0;
	u8 status = EZHIF_STATUS_OK;
	const u8 *scope_addr = NULL;

	wpa_printf(MSG_INFO, "MQTT: CMD_NEIGHBOR_DB_CLEAR received");
	if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_CLEAR_AP_ALID, ap_alid) < 0 ||
	    mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_CLEAR_HAS_SMD_ID, &has_smd_id) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto out;
	}

	if (has_smd_id) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_CLEAR_SMD_ID, smd_id) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
	} else {
		os_memset(smd_id, 0, ETH_ALEN);
	}

	if (mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_CLEAR_HAS_MLD_ADDR, &has_mld_addr) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto out;
	}
	if (has_mld_addr) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_CLEAR_MLD_ADDR, mld_addr) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
	} else {
		os_memset(mld_addr, 0, ETH_ALEN);
	}

	if (mqtt_tlv_get_u8(msg, TLV_NEIGHBOR_DB_CLEAR_HAS_BSSID, &has_bssid) < 0) {
		status = EZHIF_STATUS_INVALID_PARAM;
		goto out;
	}
	if (has_bssid) {
		if (mqtt_tlv_get_mac(msg, TLV_NEIGHBOR_DB_CLEAR_BSSID, bssid) < 0) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
	} else {
		os_memset(bssid, 0, ETH_ALEN);
	}
	wpa_printf(MSG_INFO,
		   "NeighborDB CLEAR: ap_alid=" MACSTR " has_smd=%u smd_id=" MACSTR
		   " has_mld=%u mld=" MACSTR " has_bssid=%u bssid=" MACSTR,
		   MAC2STR(ap_alid), has_smd_id, MAC2STR(smd_id),
		   has_mld_addr, MAC2STR(mld_addr), has_bssid, MAC2STR(bssid));

	if (has_smd_id) {
		if (is_zero_ether_addr(smd_id)) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
		scope_addr = smd_id;
	} else if (has_mld_addr) {
		if (is_zero_ether_addr(mld_addr)) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
		scope_addr = mld_addr;
	} else if (has_bssid) {
		if (is_zero_ether_addr(bssid)) {
			status = EZHIF_STATUS_INVALID_PARAM;
			goto out;
		}
		scope_addr = bssid;
	}
	hostapd_free_neighbor_db_nonself_scoped(hapd, has_smd_id, has_mld_addr,
						has_bssid, scope_addr);

	ieee802_11_set_beacon(hapd);

out:
	wpa_printf(MSG_INFO, "NeighborDB CLEAR: complete status=%u", status);
	mqtt_publish_simple_status(hapd, EVT_ID_NEIGHBOR_DB_CLEAR_RESP,
				   TLV_NEIGHBOR_DB_CLEAR_RESP_STATUS, status);
}

static void hostapd_mqtt_sys_cmd(struct hapd_interfaces *interfaces,
				 uint16_t msg_type,
				 struct mqtt_tlv_message *msg)
{
	(void)msg;
	wpa_printf(MSG_INFO, "MQTT: SYS dispatch msg_type=0x%04x", msg_type);
	switch (msg_type) {
	case CMD_ID_SYS_PING:
		hostapd_mqtt_handle_sys_ping(interfaces);
		break;
	default:
		break;
	}
}

static void hostapd_mqtt_smd_cmd(struct hapd_interfaces *interfaces,
				 struct hostapd_data *hapd,
				 uint16_t msg_type,
				 struct mqtt_tlv_message *msg)
{
	wpa_printf(MSG_INFO, "MQTT: SMD dispatch msg_type=0x%04x", msg_type);
	switch (msg_type) {
	case CMD_ID_NEIGHBOR_DB_SET:
		hostapd_mqtt_handle_neighbor_db_set(hapd, msg);
		break;
	case CMD_ID_NEIGHBOR_DB_GET:
		hostapd_mqtt_handle_neighbor_db_get(interfaces, hapd, msg);
		break;
	case CMD_ID_NEIGHBOR_DB_CLEAR:
		hostapd_mqtt_handle_neighbor_db_clear(hapd, msg);
		break;
	default:
		break;
	}
}

static void hostapd_mqtt_msg_cb(const char *topic, const void *payload, int payloadlen,
				void *userdata)
{
	struct hapd_interfaces *interfaces = userdata;
	struct hostapd_data *hapd;
	struct mqtt_tlv_message *msg;
	const char *feature;
	uint16_t msg_type;

	if (!payload || payloadlen < 6) {
		wpa_printf(MSG_ERROR,
			   "MQTT RX: drop short payload topic=%s len=%d",
			   topic ? topic : "<null>", payloadlen);
		return;
	}

	if (!interfaces || !interfaces->count || !interfaces->iface[0] ||
	    !interfaces->iface[0]->num_bss || !interfaces->iface[0]->bss[0])
		return;
	hapd = interfaces->iface[0]->bss[0];

	feature = mqtt_feature_from_topic(topic);
	if (!feature) {
		wpa_printf(MSG_INFO, "MQTT RX: drop unknown topic=%s", topic);
		return;
	}

	msg = mqtt_tlv_deserialize((const uint8_t *)payload, (size_t)payloadlen);
	if (!msg) {
		wpa_printf(MSG_INFO, "MQTT RX: TLV deserialize failed topic=%s", topic);
		return;
	}

	msg_type = mqtt_tlv_msg_type(msg);
	wpa_printf(MSG_INFO,
		   "MQTT RX: topic=%s feature=%s msg_type=0x%04x len=%d",
		   topic, feature, msg_type, payloadlen);
	if (mqtt_feature_validate_cmd(feature, msg_type) < 0) {
		wpa_printf(MSG_INFO,
			   "MQTT RX: feature validation failed feature=%s msg_type=0x%04x",
			   feature, msg_type);
		goto done;
	}
	if (mqtt_msg_validate_if_policy(msg_type, msg) < 0) {
		wpa_printf(MSG_INFO,
			   "MQTT RX: policy validation failed msg_type=0x%04x",
			   msg_type);
		goto done;
	}

	switch (MQTT_MSG_FEATURE(msg_type)) {
	case MQTT_FEAT_SYS:
		hostapd_mqtt_sys_cmd(interfaces, msg_type, msg);
		break;
	case MQTT_FEAT_SMD:
		hostapd_mqtt_smd_cmd(interfaces, hapd, msg_type, msg);
		break;
	default:
		wpa_printf(MSG_INFO,
			   "MQTT RX: no dispatch handler for msg_type=0x%04x feature_nibble=%u",
			   msg_type, MQTT_MSG_FEATURE(msg_type));
		break;
	}

done:
	mqtt_tlv_message_free(msg);
}

static void hostapd_mqtt_state_cb(bool connected, void *userdata)
{
	struct hapd_interfaces *interfaces = userdata;
	struct mqtt_tlv_message *msg;
	char topic[64];
	uint8_t buf[256];
	uint16_t num_ifaces = 0;
	size_t i;
	int len;

	if (!interfaces)
		return;
	if (!connected) {
		wpa_printf(MSG_INFO, "MQTT: broker disconnected");
		return;
	}

	for (i = 0; i < interfaces->count; i++)
		num_ifaces += (uint16_t)interfaces->iface[i]->num_bss;

	msg = mqtt_tlv_message_alloc(EVT_ID_SYS_HOSTAPD_STARTED);
	if (!msg)
		return;
	mqtt_tlv_add_string(msg, TLV_SYS_HOSTAPD_STARTED_VERSION, "hostapd-2.10");
	mqtt_tlv_add_u16(msg, TLV_SYS_HOSTAPD_STARTED_NUM_IFACES, num_ifaces);
	mqtt_build_transmit_topic(MQTT_FEATURE_SYS, topic, sizeof(topic));
	len = mqtt_tlv_serialize(msg, buf, sizeof(buf));
	if (len > 0) {
		wpa_printf(MSG_INFO,
			   "MQTT: connected, publish EVT_SYS_HOSTAPD_STARTED topic=%s len=%d num_ifaces=%u",
			   topic, len, num_ifaces);
		mqtt_eloop_publish(interfaces->mqtt_ctx, topic, buf, len, 0, false);
	}
	mqtt_tlv_message_free(msg);
}

void hostapd_mqtt_init(struct hostapd_iface *iface)
{
	char client_id[64];
	const char *host;
	int port;

	if (!iface || !iface->interfaces || !iface->conf)
		return;

	if (!iface->conf->mqtt_enabled || iface->interfaces->mqtt_ctx)
		return;

	host = iface->conf->mqtt_broker_host ?
	       iface->conf->mqtt_broker_host : "localhost";
	port = iface->conf->mqtt_broker_port ?
	       iface->conf->mqtt_broker_port : 1883;

	os_snprintf(client_id, sizeof(client_id), "hostapd-%d", getpid());
	iface->interfaces->mqtt_ctx = mqtt_eloop_init(
		host, port, client_id, 60,
		hostapd_mqtt_msg_cb,
		hostapd_mqtt_state_cb,
		iface->interfaces);

	if (!iface->interfaces->mqtt_ctx) {
		wpa_printf(MSG_INFO,
			   "MQTT: init failed broker=%s:%d client_id=%s",
			   host, port, client_id);
		return;
	}
	wpa_printf(MSG_INFO,
		   "MQTT: init success broker=%s:%d client_id=%s",
		   host, port, client_id);

	mqtt_eloop_subscribe(iface->interfaces->mqtt_ctx,
			     MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SMD, 1);
	mqtt_eloop_subscribe(iface->interfaces->mqtt_ctx,
			     MQTT_TOPIC_RECEIVE "/" MQTT_FEATURE_SYS, 1);
	wpa_printf(MSG_INFO,
		   "MQTT: subscribed topics=%s/%s and %s/%s",
		   MQTT_TOPIC_RECEIVE, MQTT_FEATURE_SMD,
		   MQTT_TOPIC_RECEIVE, MQTT_FEATURE_SYS);
	mqtt_eloop_connect(iface->interfaces->mqtt_ctx);
	wpa_printf(MSG_INFO, "MQTT: connect requested");
}

void hostapd_mqtt_deinit(struct hapd_interfaces *interfaces)
{
	if (!interfaces || !interfaces->mqtt_ctx)
		return;
	wpa_printf(MSG_INFO, "MQTT: deinit");
	mqtt_eloop_deinit(interfaces->mqtt_ctx);
	interfaces->mqtt_ctx = NULL;
}
