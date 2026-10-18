/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HOSTAPD_IF_COMMON_H
#define HOSTAPD_IF_COMMON_H

/*
 * Common types and enums shared between hostapd core and external plugins
 */

struct action_field {
	union {
		uint8_t action;   /* non-vendor action code */
		uint8_t oui[3];   /* vendor-specific OUI (for categories 126/127) */
	} match;
	bool is_wild_card;     /* true = wildcard (catch-all within category) */
};

enum hostapd_if_eloop_type {
	HOSTAPD_IF_ELOOP_ROUTING,
	HOSTAPD_IF_ELOOP_DIRECT_CALL
};

enum hostapd_if_disconnect_type {
	HOSTAPD_IF_DISCONNECT_FROM_STA = 0,
	HOSTAPD_IF_DISCONNECT_TO_STA = 1
};

enum hostapd_if_sa_query_status {
	HOSTAPD_IF_SAQUERY_STA_VALID = 0,
	HOSTAPD_IF_SAQUERY_STA_INVALID = 1
};

#endif /* HOSTAPD_IF_COMMON_H */
