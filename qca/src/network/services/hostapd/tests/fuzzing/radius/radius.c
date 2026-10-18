/*
 * hostapd - RADIUS fuzzer
 * Copyright (c) 2025, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "radius/radius.h"
#include "../fuzzer-common.h"


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct radius_msg *msg, *sent_msg;
	struct wpabuf *eap;
	u8 buf[10];
	int untagged;
	const unsigned int num_tagged = 5;
	int tagged[num_tagged];
	char *pw;
	int keylen;

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return 0;

	sent_msg = radius_msg_new(RADIUS_CODE_ACCESS_REQUEST, 123);
	if (!sent_msg)
		return -1;
	radius_msg_finish(sent_msg, (const u8 *) "test", 4);

	msg = radius_msg_parse(data, size);
	if (msg) {
		radius_msg_dump(msg);
		radius_msg_get_attr(msg, RADIUS_ATTR_NAS_IP_ADDRESS,
				    buf, sizeof(buf));
		radius_msg_get_vlanid(msg, &untagged, num_tagged, tagged);
		eap = radius_msg_get_eap(msg);
		wpa_hexdump_buf(MSG_INFO, "EAP", eap);
		wpabuf_free(eap);
		pw = radius_msg_get_tunnel_password(msg, &keylen,
						    (const u8 *) "test", 4,
						    sent_msg, 1);
		if (pw)
			wpa_printf(MSG_INFO, "PW: %s", pw);
		os_free(pw);
		radius_msg_free(msg);
	}

	radius_msg_free(sent_msg);

	os_program_deinit();

	return 0;
}
