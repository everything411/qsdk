/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef __HOSTAPD_IF_ELOOP_
#define __HOSTAPD_IF_ELOOP_
void hostapd_if_eloop_inbound_handlers(struct hostapd_external_app_object *);
int hostapd_if_eloop_init(enum hostapd_if_eloop_type type);
#endif

