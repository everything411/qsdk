/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef MLD_CTRL_H
#define MLD_CTRL_H

/*
 * Hostapd/wpa_supplicant control socket helpers
 *
 * Open and attach to ctrl_if sockets for hostapd and wpa_supplicant.
 * The returned ctrl handle is ready for recv requests and has a valid fd.
 */
#include "mld_types.h"

/*
 * hostapd_ctrl_open_attach - Open hostapd ctrl socket for given AP ifname
 * and attach for async events
 */
struct wpa_ctrl *hostapd_ctrl_open_attach(const char *ifname);

/*
 * wpa_supp_ctrl_open_attach - Open wpa_supplicant ctrl socket for given
 * STA ifname and attach for events
 */
struct wpa_ctrl *wpa_supp_ctrl_open_attach(const char *ifname);

#endif /* MLD_CTRL_H */
