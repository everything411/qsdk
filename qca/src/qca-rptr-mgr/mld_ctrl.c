/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*
 * Control socket open/attach utilities
 *
 * Helper wrappers to open, attach, and validate ctrl_if sockets for
 * hostapd and wpa_supplicant. Return NULL on any failure.
 */
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "wpa_ctrl.h"
#include "mld_ctrl.h"

#define AP_SOCKET_PATH_FMT "/var/run/hostapd/%s"
#define WPA_SUPP_SOCKET_PATH_FMT "/var/run/wpa_supplicant/%s"

/*
 * hostapd_ctrl_open_attach - Open and attach hostapd ctrl_if
 * for interface 'ifname'
 */
struct wpa_ctrl *hostapd_ctrl_open_attach(const char *ifname)
{
	struct wpa_ctrl *ctrl = NULL;
	char path[108];

	snprintf(path, sizeof(path), AP_SOCKET_PATH_FMT, ifname);
	ctrl = wpa_ctrl_open(path);
	if (!ctrl)
		return NULL;
	if (wpa_ctrl_attach(ctrl) != 0) {
		wpa_ctrl_close(ctrl);
		return NULL;
	}
	if (wpa_ctrl_get_fd(ctrl) < 0) {
		wpa_ctrl_detach(ctrl);
		wpa_ctrl_close(ctrl);
		return NULL;
	}
	return ctrl;
}

/*
 * wpa_supp_ctrl_open_attach - Open and attach wpa_supplicant ctrl_if
 * for interface 'ifname'
 */
struct wpa_ctrl *wpa_supp_ctrl_open_attach(const char *ifname)
{
	struct wpa_ctrl *ctrl = NULL;
	char path[108];

	snprintf(path, sizeof(path), WPA_SUPP_SOCKET_PATH_FMT, ifname);
	ctrl = wpa_ctrl_open(path);
	if (!ctrl)
		return NULL;
	if (wpa_ctrl_attach(ctrl) != 0) {
		wpa_ctrl_close(ctrl);
		return NULL;
	}
	if (wpa_ctrl_get_fd(ctrl) < 0) {
		wpa_ctrl_detach(ctrl);
		wpa_ctrl_close(ctrl);
		return NULL;
	}
	return ctrl;
}
