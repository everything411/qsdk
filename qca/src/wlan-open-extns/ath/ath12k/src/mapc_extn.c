// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <net/mac80211.h>
#include "../core.h"
#include "../wmi.h"
#include "ath12k_cmn_extn.h"
#include "mapc_extn.h"

/*
 * ath12k_mapc_cache_update_extn - update driver-side MAPC parameter cache
 * after a successful WMI_PEER_SET_MAPC_PARAMS_CMDID call from the standard
 * NL80211_CMD_SET_STATION path (CMN + PROFILE groups only).
 * Q2Q, TXOP, and E2E fields are populated separately by their vendor handlers.
 */
void ath12k_mapc_cache_update_extn(struct ath12k_link_sta *arsta,
				   struct ath12k *ar,
				   const struct ath12k_wmi_peer_mapc_params_arg *arg)
{
	spin_lock_bh(&ar->data_lock);
	arsta->arsta_extn.mapc_cache.mapc_capability_bitmap    = arg->mapc_capability_bitmap;
	arsta->arsta_extn.mapc_cache.apid_to_neighbor_peer     = arg->apid_to_neighbor_peer;
	arsta->arsta_extn.mapc_cache.apid_from_neighbor_peer   = arg->apid_from_neighbor_peer;
	arsta->arsta_extn.mapc_cache.channel_width             = arg->channel_width;
	arsta->arsta_extn.mapc_cache.ccfs                      = arg->ccfs;
	arsta->arsta_extn.mapc_cache.disable_subchannel_bitmap = arg->disable_subchannel_bitmap;
	arsta->arsta_extn.mapc_cache.bss_color                 = arg->bss_color;
	arsta->arsta_extn.mapc_cache.rx_txop_return_support    =
		arg->rx_txop_return_support ? 1 : 0;
	arsta->arsta_extn.mapc_cache.scheme_enable_bitmap      = WMI_MAPC_SCHEME_COTDMA;
	arsta->arsta_extn.mapc_cache.param_sets_recvd++;
	spin_unlock_bh(&ar->data_lock);
}
EXPORT_SYMBOL(ath12k_mapc_cache_update_extn);
