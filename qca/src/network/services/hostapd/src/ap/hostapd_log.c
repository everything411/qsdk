/*
 * hostapd_log - per-BSS structured logging for hostapd
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#include "utils/includes.h"
#include "utils/common.h"
#include "utils/wpa_debug.h"
#include "hostapd.h"
#include "hostapd_log.h"


static int module_to_idx(unsigned int module)
{
	switch (module) {
	case HOSTAPD_MODULE_IEEE80211:
		return HOSTAPD_MOD_IEEE80211;
	case HOSTAPD_MODULE_IEEE8021X:
		return HOSTAPD_MOD_IEEE8021X;
	case HOSTAPD_MODULE_RADIUS:
		return HOSTAPD_MOD_RADIUS;
	case HOSTAPD_MODULE_WPA:
		return HOSTAPD_MOD_WPA;
	case HOSTAPD_MODULE_DRIVER:
		return HOSTAPD_MOD_DRIVER;
	case HOSTAPD_MODULE_MLME:
		return HOSTAPD_MOD_MLME;
	default:
		return HOSTAPD_MOD_CORE;
	}
}


void hostapd_log(struct hostapd_data *hapd, const u8 *addr,
		 unsigned int module, int level,
		 const char *fmt, ...)
{
	char buf[512];
	va_list ap;

	if (hapd) {
		int threshold = hapd->log_module_level[module_to_idx(module)];

		if (threshold >= 0 && level < threshold)
			return;

		/* Filter active: suppress non-matching peers; when not set, all pass. */
		if (hapd->log_peer_filter_set && addr &&
		    os_memcmp(hapd->log_peer_addr, addr, ETH_ALEN) != 0)
			return;
	}

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	hostapd_logger(hapd, addr, module, level, "%s", buf);
}
