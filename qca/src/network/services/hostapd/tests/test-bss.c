/*
 * WPA BSS parsing - test program
 * Copyright (C) 2023 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include <assert.h>

#include "utils/includes.h"

#include "utils/common.h"
#include "wpa_supplicant_i.h"
#include "bss.h"

#define ASSERT_CMP_INT(a, cmp, b) { \
	ssize_t __a = (a); ssize_t __b = (b);		\
		if (!(__a cmp __b)) {			\
			wpa_printf(MSG_ERROR,		\
				   "Assertion failed: %ld %s %ld (%s %s %s)", \
				   __a, #cmp, __b, #a, #cmp, #b);	\
			abort();			\
		}					\
	}

void test_parse_basic_ml(struct wpa_supplicant *wpa_s, u8 mld_id,
			 int mbssid_idx)
{
	u8 params_link = RNR_BSS_PARAM_SAME_SSID;
	u8 params_bss = RNR_BSS_PARAM_SAME_SSID |
		((mbssid_idx >= 0) ? RNR_BSS_PARAM_MULTIPLE_BSSID : 0) |
		((mbssid_idx == 0) ? RNR_BSS_PARAM_TRANSMITTED_BSSID : 0);
	const u8 rnr_ie[] = {
		/* RNR */
		WLAN_EID_REDUCED_NEIGHBOR_REPORT, 40,
		0x00, 0x10, 0x51, 0x01, 0xff, 0x00, 0x11, 0x22,
		0x33, 0x44, 0x01, 0x68, 0x05, 0x2d, 0xa6, params_bss,
		0xfe, mld_id, 0x10, 0x00, 0x00, 0x10, 0x51, 0x06,
		0xff, 0x00, 0x11, 0x22, 0x33, 0x44, 0x02, 0x68,
		0x05, 0x2d, 0xa6, params_link, 0xfe, mld_id, 0x11, 0x00,
	};
	const u8 ml_ie[] = {
		/* basic ML */
		WLAN_EID_EXTENSION, 1 + 15, WLAN_EID_EXT_MULTI_LINK,
		0xb0, 0x01, 0x0d, 0x02, 0x00, 0x00, 0x00, 0x07,
		0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
	};
	const u8 ml_ie_mld_id[] = {
		/* basic ML */
		WLAN_EID_EXTENSION, 1 + 16, WLAN_EID_EXT_MULTI_LINK,
		0xb0, 0x03, 0x0e, 0x02, 0x00, 0x00, 0x00, 0x07,
		0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, mld_id
	};
	const u8 mbssid_idx_ie[] = {
		WLAN_EID_MULTIPLE_BSSID_INDEX, 1, mbssid_idx,
	};
	struct {
		struct wpa_bss bss;
		u8 ies[sizeof(rnr_ie) + sizeof(ml_ie_mld_id) +
		       sizeof(mbssid_idx_ie)];
	} bss;
	u16 missing_links = 0;
	u8 ret;

	memcpy(bss.bss.ies, rnr_ie, sizeof(rnr_ie));
	bss.bss.ie_len = sizeof(rnr_ie);

	if (mld_id == (mbssid_idx < 0 ? 0 : mbssid_idx)) {
		memcpy(bss.bss.ies + bss.bss.ie_len, ml_ie, sizeof(ml_ie));
		bss.bss.ie_len += sizeof(ml_ie);
	} else {
		memcpy(bss.bss.ies + bss.bss.ie_len, ml_ie_mld_id,
		       sizeof(ml_ie_mld_id));
		bss.bss.ie_len += sizeof(ml_ie_mld_id);
	}

	if (mbssid_idx > 0) {
		memcpy(bss.bss.ies + bss.bss.ie_len, mbssid_idx_ie,
		       sizeof(mbssid_idx_ie));
		bss.bss.ie_len += sizeof(mbssid_idx_ie);
	}

	wpa_bss_parse_basic_ml_element(wpa_s, &bss.bss);
	ret = wpa_bss_get_usable_links(wpa_s, &bss.bss, NULL, &missing_links);

	ASSERT_CMP_INT(ret, ==, 1);
	ASSERT_CMP_INT(bss.bss.valid_links, ==, 3);
	ASSERT_CMP_INT(missing_links, ==, 0x0002);
	ASSERT_CMP_INT(bss.bss.mld_bss_non_transmitted, ==, mbssid_idx > 0);
}

#define RUN_TEST(func, ...) do {			\
		func(wpa_s, __VA_ARGS__);		\
		printf("\nok " #func " " #__VA_ARGS__ "\n\n");		\
	} while (false)

int main(void)
{
	struct wpa_interface iface = {
		.ifname = "dummy",
	};
	struct wpa_global *global;
	struct wpa_params params = {
		.wpa_debug_level = MSG_DEBUG,
	};
	struct wpa_supplicant *wpa_s;

	global = wpa_supplicant_init(&params);

	wpa_s = wpa_supplicant_add_iface(global, &iface, NULL);
	assert(wpa_s);

	RUN_TEST(test_parse_basic_ml, 0, -1);
	RUN_TEST(test_parse_basic_ml, 1, -1);
	RUN_TEST(test_parse_basic_ml, 0, 0);
	RUN_TEST(test_parse_basic_ml, 1, 0);
	RUN_TEST(test_parse_basic_ml, 0, 1);
	RUN_TEST(test_parse_basic_ml, 1, 1);
	RUN_TEST(test_parse_basic_ml, 2, 0);
	RUN_TEST(test_parse_basic_ml, 2, 1);

	return 0;
}
