/*
 * wpa_supplicant / ubus support
 * Copyright (c) 2018, Daniel Golle <daniel@makrotopia.org>
 * Copyright (c) 2013, Felix Fietkau <nbd@nbd.name>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
#ifndef __WPAS_UBUS_H
#define __WPAS_UBUS_H

struct wpa_supplicant;
struct wpa_global;
struct wps_credential;

#ifdef UBUS_SUPPORT
#include <libubus.h>

struct wpas_ubus_bss {
	struct ubus_object obj;
};

void wpas_ubus_add_bss(struct wpa_supplicant *wpa_s);
void wpas_ubus_free_bss(struct wpa_supplicant *wpa_s);
int wpas_ubus_has_hostapd_iface_same_radio(struct wpa_supplicant *wpa_s);

#ifdef CONFIG_WPS
void wpas_ubus_notify(struct wpa_supplicant *wpa_s, const struct wps_credential *cred);
#endif

#else
struct wpas_ubus_bss {};

static inline void wpas_ubus_add_bss(struct wpa_supplicant *wpa_s)
{
}

static inline void wpas_ubus_free_bss(struct wpa_supplicant *wpa_s)
{
}

static inline int wpas_ubus_has_hostapd_iface_same_radio(struct wpa_supplicant *wpa_s)
{
	return -1;
}

static inline void wpas_ubus_notify(struct wpa_supplicant *wpa_s, const struct wps_credential *cred)
{
}

static inline void wpas_ubus_add(struct wpa_global *global)
{
}

static inline void wpas_ubus_free(struct wpa_global *global)
{
}
#endif

#endif
