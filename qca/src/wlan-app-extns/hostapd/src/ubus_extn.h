/* SPDX-License-Identifier: BSD-3-Clause */
/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
*/

#ifndef UBUS_EXTN_H
#define UBUS_EXTN_H

#if defined(UBUS_SUPPORT) && defined(HOSTAPD)
bool hostapd_ubus_is_bhsta_configured(struct hostapd_iface *iface);
#else
static inline bool hostapd_ubus_is_bhsta_configured(struct hostapd_iface *iface)
{
	return false;
}
#endif /* UBUS_SUPPORT && HOSTAPD */
#endif /* UBUS_EXTN_H */
