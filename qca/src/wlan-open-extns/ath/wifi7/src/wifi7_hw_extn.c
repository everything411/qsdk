/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/of.h>
#include "../../core.h"
#include "../../ce.h"
#include "../../hw.h"
#include "../../debug.h"
#include "../../qcn_extns/ath12k_cmn_extn.h"
#include "../../mac.h"

static void ath12k_nol_regdom_change_locked_extn(struct ieee80211_hw *hw)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k *ar;
	int i;

	/*
	 * On split-phy devices, one ieee80211_hw can own multiple
	 * 5 GHz radios with non-overlapping frequency ranges, each with its own
	 * radio_idx and its own NOL dump file. Iterate all radios so every NOL file
	 * is restored.
	 */
	for_each_ar(ah, ar, i)
		ath12k_core_nol_extn_regdom_change_radio_locked(ar);
}

struct ieee80211_240mhz_vendor_oper_extn*
ath12k_get_240_mhz_cap_extn(struct ieee80211_vif *vif,
			    struct ieee80211_link_sta *link_sta)
{
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct sta_240mhz_info *sta;

	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	list_for_each_entry(sta, &ahvif->ath12k_vif_extn.peer_240mhz_list_extn, list) {
		if (sta && !memcmp(sta->mac_addr, link_sta->addr, ETH_ALEN)) {
			struct ieee80211_240mhz_vendor_oper_extn *cap;

			cap = kmemdup(&sta->params_240MHz_extn,
				      sizeof(sta->params_240MHz_extn),
				      GFP_ATOMIC);
			spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
			return cap;
		}
	}

	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);

	return NULL;
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
int ath12k_mac_op_mesh_peer_update_caps_extn(struct ieee80211_hw *hw,
					     struct ieee80211_vif *vif,
					     struct ieee80211_sta *sta,
					     struct ieee80211_link_sta *link_sta)
{
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_sta *arsta;
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	struct ath12k_wmi_peer_assoc_arg *peer_arg;
	int ret = 0;

	lockdep_assert_wiphy(hw->wiphy);

	arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_sta->link_id]);
	if (!arvif) {
		ath12k_err(NULL, "failed to get arvif for link %u\n", link_sta->link_id);
		return -EFAULT;
	}

	arsta = wiphy_dereference(hw->wiphy, ahsta->link[link_sta->link_id]);
	if (!arsta) {
		ath12k_warn(arvif->ar->ab, "failed to get arsta for link %u\n",
			    link_sta->link_id);
		return -EFAULT;
	}

	ar = arvif->ar;

	peer_arg = kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return -ENOMEM;

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mac sta update caps for %pM on vdev %d\n",
		   sta->addr, arvif->vdev_id);

	/* Prepare peer assoc with updated capabilities
	 * reassoc = true indicates this is an update
	 */
	ath12k_peer_assoc_prepare(ar, arvif, arsta,
				  peer_arg, true, link_sta);

	/* Update mesh peer phymode and nss from the prepared peer assoc args */
	sta->sta_extn.peer_info.mesh_peer_phymode = peer_arg->peer_phymode;
	sta->sta_extn.peer_info.mesh_peer_nss = peer_arg->peer_nss;

	/* Set is_assoc = false to indicate this is a capability
	 * update for an existing peer, not a new association
	 */
	peer_arg->is_assoc = false;

	ret = ath12k_wmi_send_peer_assoc_cmd(ar, peer_arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send peer assoc for %pM vdev %d: %d\n",
			    sta->addr, arvif->vdev_id, ret);
		goto out;
	}

	if (!wait_for_completion_timeout(&ar->peer_assoc_done, 1 * HZ)) {
		ath12k_warn(ar->ab, "peer assoc timeout for %pM vdev %d\n",
			    sta->addr, arvif->vdev_id);
	}

out:
    kfree(peer_arg);
    return ret;
}
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

static const struct ieee80211_ops_extn ath12k_ops_wifi7_extn = {
	.get_240mhz_cap_extn = ath12k_get_240_mhz_cap_extn,
	.validate_6ghz_chandef_extn = ath12k_validate_6ghz_chandef_extn,
	.nol_regdom_change_locked_extn = ath12k_nol_regdom_change_locked_extn,
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	.mesh_peer_update_caps_extn = ath12k_mac_op_mesh_peer_update_caps_extn,
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */
};

void ath12k_wifi7_hw_init_extn(struct ath12k_base *ab)
{
	ab->ath12k_ops_extn = &ath12k_ops_wifi7_extn;
}
