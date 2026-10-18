// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifdef RDK_ONEWIFI

#include "utils/includes.h"
#include "utils/common.h"
#include "wps/wps.h"

void wps_pin_timeout_event(struct wps_context *wps)
{
	if (wps->event_cb == NULL)
		return;

	wps->event_cb(wps->cb_ctx, WPS_EV_PIN_TIMEOUT, NULL);
}

void wps_pin_active_event(struct wps_context *wps)
{
	if (wps->event_cb == NULL)
		return;

	wps->event_cb(wps->cb_ctx, WPS_EV_PIN_ACTIVE, NULL);
}

void wps_pin_disable_event(struct wps_context *wps)
{
	if (wps->event_cb == NULL)
		return;

	wps->event_cb(wps->cb_ctx, WPS_EV_PIN_DISABLE, NULL);
}

#endif /* RDK_ONEWIFI */
