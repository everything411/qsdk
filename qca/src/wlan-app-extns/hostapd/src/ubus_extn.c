// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#include "utils/includes.h"
#include <math.h>

#include "utils/common.h"
#include "utils/list.h"
#include "common/ieee802_11_defs.h"
#include "common/hw_features_common.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ap_drv_ops.h"
#include "ap/hw_features.h"
#include "ap/acs.h"
#include "ap/sta_info.h"
#include "ap/ubus.h"
#include <libubus.h>
#include <libubox/blobmsg.h>
#include "ubus_extn.h"
#include "cmn.h"

enum {
	STATUS_STA_STATE,
	__STATUS_MAX,
};

static const struct blobmsg_policy status_policy[__STATUS_MAX] = {
	[STATUS_STA_STATE] = { .name = "state", .type = BLOBMSG_TYPE_STRING },
};

static void phy_status_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
	struct blob_attr *tb[__STATUS_MAX];
	int *result = (int *)req->priv;

	if (!msg) {
		*result = false;
		return;
	}

	blobmsg_parse(status_policy, __STATUS_MAX, tb, blob_data(msg), blob_len(msg));

	if (tb[STATUS_STA_STATE]) {
		char *state = blobmsg_get_string(tb[STATUS_STA_STATE]);

		if (!state || !strlen(state)) {
			*result = 0;
			return;
		}

		wpa_printf(MSG_INFO, "state of station is %s", state);

		if (!strcmp(state, "COMPLETED"))
			*result = 1;
		else
			*result = 0;
	}
}

bool hostapd_ubus_is_bhsta_configured(struct hostapd_iface *iface)
{
	u32 id;
	struct ubus_context *ctx = NULL;
	struct blob_buf *b = NULL;
	int ret = -1;
	int status = 0;
	int hw_idx = 0;
	struct hostapd_data *hapd = iface->bss[0];
	const char *phy = hostapd_drv_get_radio_name(hapd);

	if (!phy)
		return false;

	ctx = ubus_ap_fetch_context_extn();
	if (!ctx)
		return false;

	if (iface->current_hw_info)
		hw_idx = iface->current_hw_info->hw_idx;

	ret = ubus_lookup_id(ctx, "wpa_supplicant", &id);
	if (ret) {
		wpa_printf(MSG_INFO, "ubus look up failed %d", ret);
		return false;
	}

	b = ubus_ap_fetch_bbuf_extn();
	if (!b)
		return false;

	blob_buf_init(b, 0);
	blobmsg_add_string(b, "phy", phy);
	blobmsg_add_u32(b, "radio", hw_idx);

	ret = ubus_invoke(ctx, id, "phy_status", b->head, phy_status_cb, &status, 3000);
	if (ret) {
		wpa_printf(MSG_DEBUG, "station not configured %d", ret);
		return false;
	}

	wpa_printf(MSG_INFO, "received status is %d", status);
	return status ? true : false;
}


