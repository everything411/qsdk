/*
 * Airtime Fairness offload config parsing and cli apis
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.*
 */

#ifndef ATF_OFFLOAD_CONFIG_H
#define ATF_OFFLOAD_CONFIG_H

/*
 * Macro to scale a percentage value by 10 to avoid decimals.
 * For example: 10.5% becomes 105 (stored as integer).
 * One digit after the decimal is preserved.
 */
#define SCALE_PERCENTAGE_TO_U32(pct) ((u32)((pct) *= 10))
#define ATF_OFFLOAD_STATS_DEFAULT_TIMEOUT 30

enum atf_scheduling_policy {
	ATF_FAIR_SCHEDULING,
	ATF_STRICT_SCHEDULING,
	ATF_FAIR_WITH_UPPER_BOUND_SCHEDULING,
};

struct atf_algo;

int atf_read_config(struct atf_algo *algo, const char *fname);

int hostapd_ctrl_iface_config_atf_offload(struct hostapd_data *hapd,
					  const char *cmd, char *buf, size_t buflen);

#endif /* ATF_OFFLOAD_CONFIG_H */
