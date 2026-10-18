/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HOSTAPD_RPTR_EXTN_H
#define HOSTAPD_RPTR_EXTN_H

#include "cmn.h"

bool uc_hostapd_compare_channel_params_extn(struct hostapd_config *conf,
					    struct hostapd_freq_params freq_params,
					    int freq);
bool hostapd_radio_has_ap_bss_extn(struct hostapd_iface *iface);
void hostapd_beacon_set_skip_cac_extn(struct hostapd_iface *iface,
				      struct hostapd_freq_params *freq_params);
#endif /* HOSTAPD_RPTR_EXTN_H */
