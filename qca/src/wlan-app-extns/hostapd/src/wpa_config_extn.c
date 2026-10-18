// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "includes.h"
#include "utils/common.h"
#include "../wpa_supplicant/config.h"
#include "cmn.h"

void
wpa_config_alloc_empty_extn(struct wpa_config *config)
{
	struct wpa_config_extn *conf_extn = &config->conf_extn;

	conf_extn->he_mcs_12_13_enabled = DEFAULT_HE_MCS_12_13_SUPPORT;
	conf_extn->vht_mcs_10_11_supp = DEFAULT_VHT_MCS_10_11_SUPPORT;
	conf_extn->he_400ns_sgi_supp = DEFAULT_HE_400NS_SGI_SUPPORT;
	conf_extn->he_2xltf_160_80p80_supp = DEFAULT_HE_2XLTF_160_80P80_SUPPORT;
}
