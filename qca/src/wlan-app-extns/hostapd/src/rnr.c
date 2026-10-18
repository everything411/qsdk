// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <math.h>
#include "utils/includes.h"
#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "ap/hostapd.h"
#include "cmn.h"
#include "rnr.h"
#include "ap/ieee802_11.h"

bool hostapd_skip_rnr_6ghz_colocated_extn(struct hostapd_data *hapd, u32 type)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;
	u8 frame_mask;

	frame_mask = conf_extn->rnr_6ghz_colocated_enable;

	if (!frame_mask)
		return false;

	switch (type) {
	case WLAN_FC_STYPE_BEACON:
		return !(frame_mask & WLAN_RNR_IN_BCN);
	case WLAN_FC_STYPE_PROBE_RESP:
		return !(frame_mask & WLAN_RNR_IN_PRB);
	case WLAN_FC_STYPE_ACTION:
		return !(frame_mask & WLAN_RNR_IN_FILS);
	default:
		return false;
	}
}

bool hostapd_rnr_6ghz_override_extn(struct hostapd_data *hapd)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;

	if (!is_6ghz_freq(hapd->iface->freq))
		return true;

	return conf_extn->rnr_6ghz_override;
}


bool hostapd_rnr_colocated_ess_indication_extn(struct hostapd_data *hapd)
{
	struct hostapd_config_extn *conf_extn = &hapd->iconf->conf_extn;

	/* Member of ESS with 2.4/5 GHz colocated AP of RNR BSS param */
	if (conf_extn->rnr_ess_colocated_en &&
		(get_colocation_mode(hapd) == COLOCATED_6GHZ)) {
		return true;

	}
	return false;
}
