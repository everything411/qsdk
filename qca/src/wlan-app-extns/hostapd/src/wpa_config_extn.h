/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef WPA_CONFIG_EXTN_H
#define WPA_CONFIG_EXTN_H

#ifdef CONFIG_QCN_EXTN
#define BOOL_KEY(key, field) key, wpa_global_config_parse_bool, \
	wpa_config_get_bool, OFFSET(field), NULL, NULL

/*
 * WPA_GLOBAL_FIELDS_EXTN - extension entries injected into global_fields[]
 * in wpa_supplicant/config.c.
 */
#define WPA_GLOBAL_FIELDS_EXTN \
	{ BOOL_KEY("he_mcs_12_13_enabled", conf_extn.he_mcs_12_13_enabled), 0 },
#else
#define WPA_GLOBAL_FIELDS_EXTN
#endif /* CONFIG_QCN_EXTN */

#endif /* WPA_CONFIG_EXTN_H */
