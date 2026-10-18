/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef WPS_RDK_EXTN_H
#define WPS_RDK_EXTN_H

#ifdef RDK_ONEWIFI

struct wps_context;

void wps_pin_timeout_event(struct wps_context *wps);
void wps_pin_active_event(struct wps_context *wps);
void wps_pin_disable_event(struct wps_context *wps);

#endif /* RDK_ONEWIFI */

#endif /* WPS_RDK_EXTN_H */
