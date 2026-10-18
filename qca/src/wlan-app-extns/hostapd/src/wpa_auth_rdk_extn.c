// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifdef RDK_ONEWIFI

#include "utils/includes.h"
#include "utils/common.h"
#include "ap/hostapd.h"
#include "ap/ap_drv_ops.h"

void hostapd_wpa_auth_get_sta_auth_type(void *ctx, const u8 *addr,
					const u8 *ies, size_t ies_len, int frame_type)
{
	struct hostapd_data *hapd = ctx;

	if (!hapd) {
		wpa_printf(MSG_ERROR, "%s hapd is NULL", __func__);
		return;
	}
	hostapd_drv_get_sta_auth_type(hapd, addr, ies, ies_len, frame_type);
}

#endif /* RDK_ONEWIFI */
