/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef WPA_AUTH_RDK_EXTN_H
#define WPA_AUTH_RDK_EXTN_H

#ifdef RDK_ONEWIFI

void hostapd_wpa_auth_get_sta_auth_type(void *ctx, const u8 *addr,
					const u8 *ies, size_t ies_len, int frame_type);

#endif /* RDK_ONEWIFI */

#endif /* WPA_AUTH_RDK_EXTN_H */
