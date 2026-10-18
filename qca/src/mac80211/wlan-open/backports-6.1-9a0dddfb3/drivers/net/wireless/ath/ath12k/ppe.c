// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "core.h"
#include "hal.h"
#include "dp.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs_sta.h"
#include "hw.h"
#include "peer.h"
#include <linux/dma-mapping.h>
#include <linux/cacheflush.h>
#include "hif.h"
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
#include "ppe.h"
#endif
#include "dp.h"
#include "fse.h"
#include "ppe_public.h"
#include "dp_stats.h"
#include "ath12k_notif.h"

extern bool ath12k_fse_enable;
atomic_t ath12k_num_ppeds_nodes;
EXPORT_SYMBOL(ath12k_num_ppeds_nodes);


struct ath12k_base *ds_node_map[PPE_DS_MAX_NODE];
EXPORT_SYMBOL(ds_node_map);
struct nss_plugins_ops *nss_plugin_ops_ptr;

struct nss_plugins_ops *ath12k_get_registered_nss_plugin_ops(void)
{
	if (!nss_plugin_ops_ptr) {
		ath12k_err(NULL, "NSS plugin ops not registered with ath12k\n");
		return NULL;
	}
	return nss_plugin_ops_ptr;
}

extern struct sk_buff *
ath12k_dp_ppeds_tx_release_desc(struct ath12k_dp *dp,
				struct ath12k_ppeds_tx_desc_info *tx_desc);


void *ath12k_dp_get_ppe_ds_ctxt(struct ath12k_base *ab)
{
	if (!ab)
		return NULL;

	if (ab->dp->ppe.nss_plugin_ops)
		return ab->dp->ppe.nss_plugin_ops->ds_inst_get_ctx(ab->dp->ppe.ds_node_id);

	return NULL;
}

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_dp_peer_ppeds_route_setup(struct ath12k *ar, struct ath12k_link_vif *arvif,
				      struct ath12k_link_sta *arsta)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_link_vif *primary_link_arvif;
	struct ath12k_vif *ahvif = arvif->ahvif;
	u32 service_code = PPE_DRV_SC_SPF_BYPASS;
	bool ppe_routing_enable = true;
	bool use_ppe = true;
	struct ath12k_sta *ahsta = arsta->ahsta;
	u32 priority_valid = 0, src_info = ahsta->ppe_vp_num;
	struct ieee80211_sta *sta;

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR ||
	    (ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
	    arvif->vdev_subtype == WMI_VDEV_SUBTYPE_MESH_11S))
		return;

	if (ahvif->dp_vif.ppe_vp_num == -1) {
		ath12k_dbg(ab, ATH12K_DBG_PPE, "Invalid ppe vp number\n");
		return;
	}

	/* If FSE is enabled, then let flow rule take decision of routing the
	 * packet to DS or host.
	 */
	if (ab->hw_params->support_fse && ath12k_fse_enable)
		use_ppe = false;

	sta = container_of((void *)ahsta, struct ieee80211_sta, drv_priv);

	/* When SLO STA is associated to AP link vif which does not have DS rings,
	 * do not enable DS.
	 */
	/* Check is for handling IPQ5322 radio, Can we add IPQ5322 target specific check here */
	if (!sta->mlo && !test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	/* If STA is MLO capable but primary link does not support DS,
	 * disable DS routing on RX.
	 */
	if (sta->mlo) {
		primary_link_arvif = arvif->ahvif->link[ahsta->assoc_link_id];

		if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED,
			      &primary_link_arvif->ar->ab->dev_flags)) {
			ath12k_dbg(ab, ATH12K_DBG_PPE,
				   "Primary link %d does not support DS "
				   "Disabling DS routing on RX for peer %pM\n",
				   ahsta->assoc_link_id, arsta->addr);
			return;
		}
	}

	ath12k_wmi_config_peer_ppeds_routing(ar, arsta->addr, arvif->vdev_id,
					     service_code, priority_valid,
					     src_info, ppe_routing_enable,
					     use_ppe);
}
#endif

void ath12k_dp_ppeds_tx_set_ppe_vp_entry(struct ath12k_base *ab,
					 struct ath12k_dp_ppe_vp_profile *ppe_vp_profile,
					 u32 ppe_vp_idx, u32 vdev_id,
					 u32 bank_id, u32 lmac_id)
{
	ab->hal.hal_ops->hal_tx_set_ppe_vp_entry(ab, ppe_vp_profile, ppe_vp_idx,
						 vdev_id, bank_id, lmac_id);
}
EXPORT_SYMBOL(ath12k_dp_ppeds_tx_set_ppe_vp_entry);


void ath12k_dp_ppeds_setup_vp_entry(struct ath12k_base *ab,
				    struct ath12k *ar,
				    struct ath12k_link_vif *arvif,
				    struct ath12k_dp_ppe_vp_profile *ppe_vp_profile)
{
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	u8 lmac_id;
	u8 bank_id;

	/* In splitphy DS case, when mlo vaps created on different radios,
	 * the vp profile must be shared between these vaps with wildcard pmac_id,
	 * as they share the same vp number. We need to create a separate bank
	 * corresponding the shared vp. Also, this bank should have vdev id check
	 * disabled, so that the FW can get the ast from any of the lmacs without
	 * throwing vdev id mismatch error
	 */
	ppe_ops->ath12k_dp_ppeds_get_bank_lmac_id(ab, ar, arvif,
						ppe_vp_profile, &bank_id, &lmac_id);

	ath12k_dp_ppeds_tx_set_ppe_vp_entry(ab, ppe_vp_profile,
					    ppe_vp_profile->ppe_vp_num_idx,
					    arvif->vdev_id, bank_id, lmac_id);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"PPEDS vp_num:%d srch_idx_reg_num:%d int_pri:%d to_fw:%d\n",
			ppe_vp_profile->vp_num, ppe_vp_profile->search_idx_reg_num,
			ppe_vp_profile->use_ppe_int_pri, ppe_vp_profile->to_fw);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"drop_prec:%d bank_id:%d lmac_id:%d vdev_id:%d ppe_vp_idx:%d\n",
			ppe_vp_profile->drop_prec_enable, bank_id, lmac_id,
			arvif->vdev_id, ppe_vp_profile->ppe_vp_num_idx);
}

void ath12k_dp_ppeds_update_vp_entry(struct ath12k *ar,
				     struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	int ppe_vp_profile_idx;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) ||
	    arvif->ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	ppe_vp_profile_idx = arvif->ppe_vp_profile_idx;
	vp_profile = ppe_ops->ath12k_dp_ppeds_get_vp_profile_from_idx(ab,
							ppe_vp_profile_idx);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE, "vp profile not present for arvif\n");
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	ath12k_dp_ppeds_setup_vp_entry(ab, arvif->ar, arvif, vp_profile);
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

int ath12k_ppeds_attach_link_apvlan_vif(struct ath12k_link_vif *arvif, int vp_num,
					struct ath12k_vlan_iface *vlan_iface, int link_id)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_ppe_vp_profile *vp_profile = NULL;
	int ppe_vp_profile_idx, ppe_vp_tbl_idx = -1;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	int ppe_vp_search_tbl_idx = -1;
	int vdev_id = arvif->vdev_id;
	int ret;
	enum nl80211_iftype vif_type;
	u32 bank_config;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	if (!wdev)
		return -EOPNOTSUPP;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return 0;

	/*Allocate a ppe vp profile for a vap */
	spin_lock(&ppe->ppe_vp_tbl_lock);
	if (!ppe_ops->ath12k_dp_ppeds_alloc_ppe_vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "Profile alloc not registered:%s link %d ",
			   wdev->netdev->name, arvif->link_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return -EINVAL;
	}

	ppe_vp_profile_idx =
			ppe_ops->ath12k_dp_ppeds_alloc_ppe_vp_profile(ab,
				&vp_profile, vp_num);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "flows for %s link %d will use SFE RFS flow distribution",
			   wdev->netdev->name, arvif->link_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}

	if (vp_profile->ref_count == 1) {
		if (!ppe_ops->ath12k_dp_ppeds_alloc_vp_tbl_entry) {
			ath12k_err(ab, "Alloc Not registered:vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}
		ppe_vp_tbl_idx =
			ppe_ops->ath12k_dp_ppeds_alloc_vp_tbl_entry(ab,
					ppe_vp_profile_idx);
		if (ppe_vp_tbl_idx < 0) {
			ath12k_err(ab, "Alloc failed PPE VP idx for vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}

		if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
			if (!ppe_ops->ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry) {
				ath12k_err(ab, "Handle not present vdev_id:%d",
					   vdev_id);
				ret = -ENOSR;
				goto dealloc_vp_profile;
			}
			ppe_vp_search_tbl_idx =
				ppe_ops->ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry(ab,
							ppe_vp_profile_idx);
			if (ppe_vp_search_tbl_idx < 0) {
				ath12k_err(ab, "Alloc failed vp srch tbl en vdev_id:%d",
					   vdev_id);
				ret = -ENOSR;
				goto dealloc_vp_profile;
			}
			vp_profile->search_idx_reg_num = ppe_vp_search_tbl_idx;
		}

		vp_profile->vp_num = vp_num;
		vp_profile->ppe_vp_num_idx = ppe_vp_tbl_idx;
		vp_profile->to_fw = 0;
		vp_profile->use_ppe_int_pri = 0;
		vp_profile->drop_prec_enable = 0;
		vp_profile->arvif = arvif;
		vp_profile->entry_valid = true;

		vlan_iface->ppe_vp_profile_idx[link_id] = ppe_vp_profile_idx;
	} else {
		vlan_iface->ppe_vp_profile_idx[link_id] = ppe_vp_profile_idx;

		bank_config = ath12k_dp_arch_tx_get_vdev_bank_config(ath12k_ab_to_dp(ab),
								     ahvif,
								     arvif->link_id,
								     true);
		arvif->splitphy_ds_bank_id =
			ath12k_dp_tx_get_bank_profile(ath12k_ab_to_dp(ab), bank_config);

		ath12k_ppeds_update_splitphy_bank_id(ab, arvif);
	}

	ath12k_dp_ppeds_setup_vp_entry(ab, ar, arvif, vp_profile);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"PPEDS vp profile setup success soc:%d node_id:%d vdev_id %d\n",
			ab->dp->ppe.ppeds_soc_idx, ab->dp->ppe.ds_node_id, vdev_id);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"vpnum:%d ppe_vp_idx:%d ppe_vp_tbl_idx:%d to_fw %d int_pri %d\n",
			vp_num, ppe_vp_profile_idx, ppe_vp_tbl_idx, vp_profile->to_fw,
			vp_profile->use_ppe_int_pri);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"prec_en %d search_idx_reg_num %d\n",
			vp_profile->drop_prec_enable, vp_profile->search_idx_reg_num);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return 0;

dealloc_vp_profile:
	vif_type = arvif->ahvif->vif->type;
	if (!ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile) {
		ath12k_err(ab, "Failed to dealloc vp profile:vdev_id:%d",
				vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return ret;
	}
	ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab,
			ppe_vp_profile_idx, vif_type);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return ret;
}

void ath12k_ppe_ds_attach_vlan_vif_link(struct ath12k_vlan_iface *vlan_iface,
					int ppe_vp_num)
{
	struct ieee80211_vif *vif = vlan_iface->parent_vif;
	struct ath12k_vif *ap_ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *ap_arvif;
	struct ath12k *ar;
	int link_id, ret;
	unsigned long links_map;

	if (vlan_iface->attach_link_done)
		return;

	links_map = ap_ahvif->links_map;

	rcu_read_lock();
	for_each_set_bit(link_id, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		ap_arvif = rcu_dereference(ap_ahvif->link[link_id]);

		if (!ap_arvif || !ap_arvif->is_created)
			continue;

		ar = ap_arvif->ar;
		ret = ath12k_ppeds_attach_link_apvlan_vif(ap_arvif, ppe_vp_num, vlan_iface,
							  link_id);
		if (ret)
			ath12k_info(ar->ab, "Unable to attach ppe ds node for arvif %d\n",
				    ret);
	}
	rcu_read_unlock();
	vlan_iface->attach_link_done = true;
}

void ath12k_ppeds_update_splitphy_bank_id(struct ath12k_base *ab,
					  struct ath12k_link_vif *arvif)
{
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_link_vif *iter_arvif;
	unsigned long links_map;
	int link_idx;

	links_map = ahvif->links_map;
	for_each_set_bit(link_idx, &links_map, IEEE80211_MLD_MAX_NUM_LINKS) {
		iter_arvif = ahvif->link[link_idx];
		int splitphy_ds_bank_id = DP_INVALID_BANK_ID;

		/* if parnter vif is not from same soc continue */
		if (!iter_arvif || iter_arvif == arvif ||
		    !iter_arvif->is_created || ab != iter_arvif->ar->ab)
			continue;

		splitphy_ds_bank_id = iter_arvif->splitphy_ds_bank_id;
		/**
		 * If the link already has a splitphy_ds_bank_id
		 * then decrement the refcount for that bank_id
		 * before updating the link with a new bank_id
		 */
		if (splitphy_ds_bank_id != DP_INVALID_BANK_ID)
			ath12k_dp_tx_put_bank_profile(ath12k_ab_to_dp(ab),
						      splitphy_ds_bank_id);

		iter_arvif->splitphy_ds_bank_id = arvif->splitphy_ds_bank_id;
		ath12k_dp_increment_bank_num_users(ath12k_ab_to_dp(ab),
						   arvif->splitphy_ds_bank_id);
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "splitphy_ds_bank_id %d for vp_num %d vdev_id %d\n",
			   arvif->splitphy_ds_bank_id,
			   ahvif->dp_vif.ppe_vp_num, iter_arvif->vdev_id);
	}
}

int ath12k_ppeds_attach_link_vif(struct ath12k_link_vif *arvif, int vp_num,
				 int *link_ppe_vp_profile_idx,
				 struct ieee80211_vif *vif)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(arvif->ahvif->vif);
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_dp_ppe_vp_profile *vp_profile = NULL;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	struct ieee80211_sta *sta;
	struct ath12k_sta *ahsta;
	int ppe_vp_profile_idx, ppe_vp_tbl_idx = -1;
	int ppe_vp_search_tbl_idx = -1;
	int vdev_id = arvif->vdev_id;
	int ret;
	enum nl80211_iftype vif_type;
	u32 bank_config;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	if (!wdev)
		return -EOPNOTSUPP;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return 0;

	if (vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return 0;

	if (ahvif->vif->type != NL80211_IFTYPE_AP && ahvif->vif->type != NL80211_IFTYPE_STATION) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "DS is not supported for vap type %d\n", ahvif->vif->type);
		return 0;
	}

	/*Allocate a ppe vp profile for a vap */
	spin_lock(&ppe->ppe_vp_tbl_lock);
	if (!ppe_ops->ath12k_dp_ppeds_alloc_ppe_vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "Alloc handle not present  %s link %d",
			   wdev->netdev->name, arvif->link_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}
	ppe_vp_profile_idx =
		ppe_ops->ath12k_dp_ppeds_alloc_ppe_vp_profile(ab,
				&vp_profile, vp_num);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "flows for %s link %d will use SFE RFS flow distribution",
			   wdev->netdev->name, arvif->link_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return 0;
	}

	if (vp_profile->ref_count == 1) {
		if (!ppe_ops->ath12k_dp_ppeds_alloc_vp_tbl_entry) {
			ath12k_err(ab, "Alloc failed vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}
		ppe_vp_tbl_idx =
			ppe_ops->ath12k_dp_ppeds_alloc_vp_tbl_entry(ab,
					ppe_vp_profile_idx);
		if (ppe_vp_tbl_idx < 0) {
			ath12k_err(ab, "Failed to allocate PPE VP idx for vdev_id:%d", vdev_id);
			ret = -ENOSR;
			goto dealloc_vp_profile;
		}

		if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
			if (!ppe_ops->ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry) {
				ath12k_err(ab,
					   "Failed entry alloc - vdev_id:%d",
					   vdev_id);
				ret = -ENOSR;
				goto dealloc_vp_profile;
			}
			ppe_vp_search_tbl_idx =
				ppe_ops->ath12k_dp_ppeds_alloc_vp_search_idx_tbl_entry(ab,
						ppe_vp_profile_idx);
			if (ppe_vp_search_tbl_idx < 0) {
				ath12k_err(ab,
					   "Failed to allocate PPE VP search table idx for vdev_id:%d", vdev_id);
				ret = -ENOSR;
				goto dealloc_vp_profile;
			}
			vp_profile->search_idx_reg_num = ppe_vp_search_tbl_idx;
		}

		vp_profile->vp_num = vp_num;
		vp_profile->ppe_vp_num_idx = ppe_vp_tbl_idx;
		vp_profile->to_fw = 0;
		vp_profile->use_ppe_int_pri = 0;
		vp_profile->drop_prec_enable = 0;
		vp_profile->arvif = arvif;
		vp_profile->entry_valid = true;

		*link_ppe_vp_profile_idx = ppe_vp_profile_idx;
	} else {
		u8 link_id;

		if (arvif->ahvif->links_map &&
		    arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
			struct ath12k_link_vif *arvif;
			struct ath12k_base *prim_ab;
			struct ath12k_dp_ppe_vp_profile *prim_vp_profile;

			rcu_read_lock();
			sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
			if (!sta) {
				ath12k_warn(ab, "failed to find station entry for %pM",
					    vif->cfg.ap_addr);
				spin_unlock(&ppe->ppe_vp_tbl_lock);
				rcu_read_unlock();
				return -ENOENT;
			}

			ahsta = ath12k_sta_to_ahsta(sta);
			link_id = ahsta->deflink.link_id;
			if (!sta->mlo)
				link_id = ahsta->deflink.link_id;
			else
				link_id = ahsta->assoc_link_id;
			arvif = rcu_dereference(ahvif->link[link_id]);

			if (!arvif) {
				ath12k_warn(ab, "arvif not found for station:%pM",
					    sta->addr);
				spin_unlock(&ppe->ppe_vp_tbl_lock);
				rcu_read_unlock();
				return -EINVAL;
			}

			prim_ab = arvif->ar->ab;
			prim_vp_profile =
				ppe_ops->ath12k_dp_ppeds_get_vp_profile_from_idx(prim_ab,
							arvif->ppe_vp_profile_idx);
			if (!prim_vp_profile) {
				ath12k_warn(ab, "Invalid PPE VP profile idx:%d",
						arvif->ppe_vp_profile_idx);
				spin_unlock(&ppe->ppe_vp_tbl_lock);
				rcu_read_unlock();
				return 0;
			}

			vp_profile->search_idx_reg_num =
					prim_vp_profile->search_idx_reg_num;
			rcu_read_unlock();
		}

		*link_ppe_vp_profile_idx = ppe_vp_profile_idx;
		bank_config = ath12k_dp_arch_tx_get_vdev_bank_config(ath12k_ab_to_dp(ab),
								     ahvif,
								     arvif->link_id,
								     true);
		arvif->splitphy_ds_bank_id =
			ath12k_dp_tx_get_bank_profile(ath12k_ab_to_dp(ab), bank_config);
		ath12k_ppeds_update_splitphy_bank_id(ab, arvif);

	}

	ath12k_dp_ppeds_setup_vp_entry(ab, ar, arvif, vp_profile);

	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"PPEDS vp profile setup success soc:%d node_id:%d vdev_id %d\n",
			ab->dp->ppe.ppeds_soc_idx, ab->dp->ppe.ds_node_id, vdev_id);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"vpnum:%d ppe_vp_idx:%d ppe_vp_tbl_idx:%d to_fw %d int_pri %d\n",
			vp_num, ppe_vp_profile_idx, ppe_vp_tbl_idx, vp_profile->to_fw,
			vp_profile->use_ppe_int_pri);
	ath12k_dbg(ab, ATH12K_DBG_PPE,
			"prec_en %d search_idx_reg_num %d\n",
			vp_profile->drop_prec_enable, vp_profile->search_idx_reg_num);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return 0;

dealloc_vp_profile:
	vif_type = arvif->ahvif->vif->type;
	if (!ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile) {
		ath12k_err(ab, "Failed to dealloc vp profile:vdev_id:%d",
				vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return ret;
	}
	ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab,
			ppe_vp_profile_idx, vif_type);
	spin_unlock(&ppe->ppe_vp_tbl_lock);

	return ret;
}

void ath12k_dp_tx_ppeds_cfg_astidx_cache_mapping(struct ath12k_base *ab,
						 struct ath12k_link_vif *arvif,
						 bool peer_map)
{
	u32 ppeds_idx_map_val = 0;
	int ppe_vp_profile_idx = arvif->ppe_vp_profile_idx;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	u8 link_id = arvif->link_id;
	struct ath12k_dp_link_vif *dp_link_vif = &arvif->ahvif->dp_vif.dp_link_vif[link_id];
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags) ||
	    !arvif->primary_sta_link)
		return;

	if (arvif->ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	vp_profile = ppe_ops->ath12k_dp_ppeds_get_vp_profile_from_idx(ab,
							ppe_vp_profile_idx);
	if (!vp_profile) {
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	if (!vp_profile->is_configured) {
		ath12k_err(ab, "Invalid PPE VP profile for vdev_id:%d",
			   arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}
	if (arvif->ahvif->vif->type == NL80211_IFTYPE_STATION) {
		if (peer_map) {
			ppeds_idx_map_val |=
				u32_encode_bits(dp_link_vif->ast_idx, HAL_TX_PPEDS_CFG_SEARCH_IDX) |
				u32_encode_bits(dp_link_vif->ast_hash, HAL_TX_PPEDS_CFG_CACHE_SET);
		}
		ath12k_hal_ppeds_cfg_ast_override_map_reg(ab, vp_profile->search_idx_reg_num,
							  ppeds_idx_map_val);
	}
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

void ath12k_ppeds_detach_link_apvlan_vif(struct ath12k_link_vif *arvif,
					 struct ath12k_vlan_iface *vlan_iface,
					 int link_id)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	int ppe_vp_profile_idx = vlan_iface->ppe_vp_profile_idx[link_id];
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	enum nl80211_iftype vif_type;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ahvif->dp_vif.ppe_vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	vp_profile = ppe_ops->ath12k_dp_ppeds_get_vp_profile_from_idx(ab,
							ppe_vp_profile_idx);
	if (!vp_profile) {
		ath12k_err(ab, "Invalid PPE VP idx:%d for vdev_id:%d",
				ppe_vp_profile_idx, arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	if (!vp_profile->is_configured) {
		ath12k_err(ab, "Invalid PPE VP profile for vdev_id:%d",
			   arvif->vdev_id);
		vlan_iface->ppe_vp_profile_idx[link_id] = ATH12K_INVALID_VP_PROFILE_IDX;
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	vif_type = arvif->ahvif->vif->type;
	if (!ppe->ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile) {
		ath12k_err(ab, "Failed to dealloc vp profile");
		vlan_iface->ppe_vp_profile_idx[link_id] = ATH12K_INVALID_VP_PROFILE_IDX;
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}
	ppe->ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab,
			ppe_vp_profile_idx, vif_type);
	vlan_iface->ppe_vp_profile_idx[link_id] = ATH12K_INVALID_VP_PROFILE_IDX;
	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS vdev detach success vpnum %d  ppe_vp_profile_idx %d\n",
	       vp_profile->vp_num, ppe_vp_profile_idx);
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

void ath12k_ppeds_detach_link_vif(struct ath12k_link_vif *arvif, int ppe_vp_profile_idx)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_dp_ppe_vp_profile *vp_profile;
	struct ath12k_ppe *ppe = &ab->dp->ppe;
	enum nl80211_iftype vif_type;
	struct ath12k_ppeds_arch_ops *ppe_ops = ppe->ppe_ops;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ab->dev_flags))
		return;

	if (ahvif->dp_vif.ppe_vp_num <= 0 || ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_DS)
		return;

	spin_lock(&ppe->ppe_vp_tbl_lock);
	vp_profile = ppe_ops->ath12k_dp_ppeds_get_vp_profile_from_idx(ab,
							ppe_vp_profile_idx);
	if (!vp_profile) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "No PPE VP profile found for vdev_id:%d", arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	if (!vp_profile->is_configured) {
		ath12k_dbg(ab, ATH12K_DBG_PPE,
			   "No PPE VP profile found for vdev_id:%d", arvif->vdev_id);
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}

	vif_type = arvif->ahvif->vif->type;
	if (!ppe->ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile) {
		ath12k_err(ab, "Failed to dealloc vp profile");
		spin_unlock(&ppe->ppe_vp_tbl_lock);
		return;
	}
	ppe->ppe_ops->ath12k_dp_ppeds_dealloc_ppe_vp_profile(ab,
			ppe_vp_profile_idx, vif_type);
	ath12k_dbg(ab, ATH12K_DBG_PPE, "PPEDS vdev detach success vpnum %d  ppe_vp_profile_idx %d\n",
		   vp_profile->vp_num, ppe_vp_profile_idx);
	spin_unlock(&ppe->ppe_vp_tbl_lock);
}

void ath12k_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
				      u8 rbm_id, int ring_type)
{
	ab->hal.hal_ops->hal_tx_config_rbm_mapping(ab, ring_num, rbm_id,
						      ring_type);
}
EXPORT_SYMBOL(ath12k_hal_tx_config_rbm_mapping);

int ath12k_dp_srng_init_idx(struct ath12k_base *ab, struct dp_srng *ring,
			    enum hal_ring_type type, int ring_num,
			    int mac_id,
			    int num_entries, u32 restore_idx)
{
	struct hal_srng_params params = { 0 };
	bool cached = false;
	int ret;
	int vector = 0;

	params.ring_base_vaddr = ring->vaddr;
	params.ring_base_paddr = ring->paddr;
	params.num_entries = num_entries;
	ath12k_dp_srng_msi_setup(ab, &params, type, ring_num + mac_id);

	if (ab->hw_params->ds_support && ab->hif.bus == ATH12K_BUS_AHB &&
	    !ath12k_dp_umac_reset_in_progress(ab))
		ath12k_hif_ppeds_register_interrupts(ab, type, vector, ring_num);

	switch (type) {
	case HAL_REO_DST:
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	case HAL_REO2PPE:
#endif
			params.intr_batch_cntr_thres_entries =
				HAL_SRNG_INT_BATCH_THRESHOLD_RX;
			params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_RX;
			break;
	case HAL_WBM2SW_RELEASE:
	case HAL_TX_COMPLETION:
		if (ab->hw_params->hw_ops->dp_srng_is_tx_comp_ring(ring_num)) {
			params.intr_batch_cntr_thres_entries =
				HAL_SRNG_INT_BATCH_THRESHOLD_TX;
			params.intr_timer_thres_us =
				HAL_SRNG_INT_TIMER_THRESHOLD_TX;
			break;
		}
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_OTHER;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_OTHER;
		break;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	case HAL_PPE2TCL:
		params.intr_batch_cntr_thres_entries =
			HAL_SRNG_INT_BATCH_THRESHOLD_PPE2TCL;
		params.intr_timer_thres_us = HAL_SRNG_INT_TIMER_THRESHOLD_PPE2TCL;
		break;
#endif
	default:
		ath12k_warn(ab, "Not a valid ring type in dp :%d\n", type);
		return -EINVAL;
	}
	if (cached) {
		params.flags |= HAL_SRNG_FLAGS_CACHED;
		ring->cached = 1;
	}

	ret = ath12k_hal_srng_setup_idx(ab, type, ring_num, mac_id, &params,
					restore_idx);
	if (ret < 0) {
		ath12k_warn(ab, "failed to setup srng: %d ring_id %d\n",
			    ret, ring_num);
		return ret;
	}

	ring->ring_id = ret;

	return 0;
}
EXPORT_SYMBOL(ath12k_dp_srng_init_idx);

static int ath12k_ppe_ds_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
				       enum hal_ring_type type, int ring_num,
				       int num_entries)
{
	int entry_sz = ath12k_hal_srng_get_entrysize(ab, type);
	int max_entries = ath12k_hal_srng_get_max_entries(ab, type);
	bool cached = false;

	if (max_entries < 0 || entry_sz < 0)
		return -EINVAL;

	if (num_entries > max_entries)
		num_entries = max_entries;

#ifndef CONFIG_IO_COHERENCY
	if (ab->hw_params->alloc_cacheable_memory) {
		/* Allocate the reo dst and tx completion rings from cacheable memory */
		switch (type) {
		case HAL_REO_DST:
		case HAL_WBM2SW_RELEASE:
			cached = true;
			break;
		default:
			cached = false;
		}
	}
#else
	cached = true;
#endif
	return ath12k_dp_srng_alloc_aligned(ab, ring, num_entries, entry_sz,
					   cached);
}

int ath12k_ppeds_dp_srng_alloc(struct ath12k_base *ab, struct dp_srng *ring,
			       enum hal_ring_type type, int ring_num,
			       int num_entries)
{
	int ret;

	ret = ath12k_ppe_ds_dp_srng_alloc(ab, ring, type, ring_num, num_entries);
	if (ret != 0)
		ath12k_warn(ab, "Failed to allocate dp srng ring.\n");

	return ret;
}
EXPORT_SYMBOL(ath12k_ppeds_dp_srng_alloc);

int ath12k_ppeds_dp_srng_init(struct ath12k_base *ab, struct dp_srng *ring,
			      enum hal_ring_type type, int ring_num,
			      int mac_id, int num_entries,
			      u32 restore_idx)
{
	int ret;

	ret = ath12k_dp_srng_init_idx(ab, ring, type, ring_num, mac_id,
				      num_entries, restore_idx);
	if (ret != 0)
		ath12k_warn(ab, "Failed to initialize dp srng ring.\n");

	return 0;
}
EXPORT_SYMBOL(ath12k_ppeds_dp_srng_init);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
int ath12k_ppe_rfs_get_core_mask(void)
{
	return ATH12K_PPE_DEFAULT_CORE_MASK;
}

/* User is expected to flush ecm entries before changing core mask */
int ath12k_change_core_mask_for_ppe_rfs(struct ath12k_base *ab,
					struct ath12k_vif *ahvif,
					int core_mask)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	int ret;

	if (!wdev)
		return -ENODEV;

	if (ahvif->dp_vif.ppe_vp_num <= 0 ||
	    ahvif->dp_vif.ppe_vp_type != PPE_VP_USER_TYPE_PASSIVE) {
		ath12k_warn(ab, "invalid vp for dev %s\n", wdev->netdev->name);
		return -EINVAL;
	}

	if (core_mask < 0) {
		ath12k_warn(ab, "Invalid core_mask for PPE RFS\n");
		return -EINVAL;
	}

	if (core_mask == ahvif->dp_vif.ppe_core_mask)
		return 0;

	ret = ath12k_vif_get_vp_num(ahvif, wdev->netdev);
	if (ret) {
		ath12k_warn(ab, "error in enabling ppe vp for netdev %s\n",
			    wdev->netdev->name);
		return ret;
	}

	return 0;
}

static bool ath12k_stats_update_ppe_vp(struct net_device *dev, ppe_vp_hw_stats_t *vp_stats)
{
	struct pcpu_sw_netstats *tstats = this_cpu_ptr(netdev_tstats(dev));
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ath12k_vif *ahvif;
	struct ath12k_dp_vif *dp_vif;
	struct ieee80211_sta *sta;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_hw *ah;

	if (dev->reg_state != NETREG_REGISTERED)
		return false;
	/*
	 * PPE VP RX stats are not accounted by the normal ath12k RX path.
	 * Sync RX counters into ath12k datapath stats per interface type -
	 * at peer-level for WDS STA/AP_VLAN, MLD VIF-level otherwise.
	 */
	wdev = ath12k_get_wdev_from_netdev(dev);
	if (!wdev)
		goto sync_stats;

	rcu_read_lock();
	/*
	 * For WDS STA interfaces, get the actual VLAN VIF to lookup the peer
	 * and attribute the RX stats to it.
	 */
	if (wdev->iftype == NL80211_IFTYPE_AP_VLAN) {
		sta = wdev_to_ieee80211_vlan_sta(wdev);
		if (!sta)
			goto update_vif;

		/* Fetch the VLAN VIF (not the parent AP VIF) for peer lookup */
		vif = wdev_to_ieee80211_vif_vlan(wdev, true);
		if (!vif)
			goto update_vif;

		ahvif = ath12k_vif_to_ahvif(vif);
		if (!ahvif)
			goto update_vif;

		ah = ahvif->ah;
		spin_lock_bh(&ah->dp_hw.peer_hash_lock);
		dp_peer = ath12k_dp_peer_find_by_addr(&ah->dp_hw, sta->addr, vif);
		if (dp_peer) {
			if (dp_peer->assoc_hw_link_id < ATH12K_DP_PEER_MAX_MLO_LINKS)
				DP_PEER_STATS_PKT_LEN(dp_peer, rx, DP_REO_PPEDS_RING_IDX,
						      sent_to_stack,
						      dp_peer->assoc_hw_link_id,
						      vp_stats->rx_pkt_cnt,
						      vp_stats->rx_byte_cnt);
			spin_unlock_bh(&ah->dp_hw.peer_hash_lock);
			goto sync_stats;
		} else  {
			spin_unlock_bh(&ah->dp_hw.peer_hash_lock);
			ath12k_dbg(NULL, ATH12K_DBG_PPE,
				   "dp_peer not found for addr=%pM\n",
				   sta->addr);
		}
	}
update_vif:
	/*
	 * Attribute RX stats at MLD VIF level for non-WDS DS interfaces,
	 * or as a fallback when peer lookup fails for a WDS STA.
	 * Fetch the parent MLD VIF for VIF-level stats attribution.
	 */
	vif = wdev_to_ieee80211_vif_vlan(wdev, false);

	if (!vif)
		goto sync_stats;

	ahvif = ath12k_vif_to_ahvif(vif);
	if (!ahvif)
		goto sync_stats;

	dp_vif = &ahvif->dp_vif;
	if (!dp_vif) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE, "MLD VIF also not found.\n");
		goto sync_stats;
	}
	DP_RX_STATS_INC_PKT(dp_vif, ppeds_rx, vp_stats->rx_pkt_cnt,
			 vp_stats->rx_byte_cnt, DP_REO_PPEDS_RING_IDX);

sync_stats:
	rcu_read_unlock();
	/*
	 * If TX/RX offload accounting is active, mac80211 pulls stats via
	 * get_netstats(). Skip direct sw_netstats updates to avoid double counting.
	 */
	if (vif && vif->offload_flags & IEEE80211_OFFLOAD_TXRX_STATS)
		return true;
	/* Update the netdev statistics */
	u64_stats_update_begin(&tstats->syncp);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	tstats->tx_packets += vp_stats->tx_pkt_cnt;
	tstats->tx_bytes += vp_stats->tx_byte_cnt;
	tstats->rx_packets += vp_stats->rx_pkt_cnt;
	tstats->rx_bytes += vp_stats->rx_byte_cnt;
#else
	u64_stats_add(&tstats->tx_packets, vp_stats->tx_pkt_cnt);
	u64_stats_add(&tstats->tx_bytes, vp_stats->tx_byte_cnt);
	u64_stats_add(&tstats->rx_packets, vp_stats->rx_pkt_cnt);
	u64_stats_add(&tstats->rx_bytes, vp_stats->rx_byte_cnt);
#endif
	u64_stats_update_end(&tstats->syncp);

	return true;
}
#endif

void ath12k_vif_free_vp(struct ath12k_vif *ahvif, struct net_device *dev)
{
	/* ahvif->dp_vif.ppe_vp_num gets reset to 0 during memset in mac80211 for vif->drv_priv */
	if (ahvif->dp_vif.ppe_vp_num == ATH12K_INVALID_PPE_VP_NUM || ahvif->dp_vif.ppe_vp_num == 0)
		return;

	ppe_vp_free(ahvif->dp_vif.ppe_vp_num);

	ath12k_dbg(NULL, ATH12K_DBG_PPE, "Destroyed PPE VP port no:%d for dev:%s vdev type %d\n",
	       ahvif->dp_vif.ppe_vp_num, dev->name,
	       ahvif->vdev_type);
	ahvif->dp_vif.ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM;
}
EXPORT_SYMBOL(ath12k_vif_free_vp);

int ath12k_vif_update_vp_config(struct ath12k_vif *ahvif, int ppe_vp_type)

{
	struct ppe_vp_ui vpui;
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	int ret;
	uint8_t update_flags = 0;
	struct nss_plugins_ops *plugin_ops = ath12k_get_registered_nss_plugin_ops();

	if ((ahvif->dp_vif.ppe_vp_num == ATH12K_INVALID_PPE_VP_NUM) || !wdev)
		return -EINVAL;

	if (!plugin_ops)
		return -EINVAL;

	memset(&vpui, 0, sizeof(struct ppe_vp_ui));
	vpui.usr_type = ppe_vp_type;
	vpui.core_mask = ahvif->dp_vif.ppe_core_mask;
	update_flags |= PPE_VP_UPDATE_FLAG_VP_CORE_MASK;
	update_flags |= PPE_VP_UPDATE_FLAG_VP_USR_TYPE;
	vpui.update_flags = update_flags;
	vpui.stats_cb = ath12k_stats_update_ppe_vp;
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Direct Switching */
	switch (ppe_vp_type) {
	case PPE_VP_USER_TYPE_PASSIVE:
		vpui.core_mask = ath12k_ppe_rfs_get_core_mask();
		break;
	case PPE_VP_USER_TYPE_DS:
	case PPE_VP_USER_TYPE_ACTIVE:
		vpui.core_mask = ATH12K_PPE_DEFAULT_CORE_MASK;
		break;
	}
#endif

	ret = plugin_ops->vp_cfg_update(wdev->netdev, &vpui);

	if (ret) {
		ath12k_err(NULL, "failed to update ppe vp config type %d err %d\n",
			   ppe_vp_type, ret);
		goto exit;
	}

	ahvif->dp_vif.ppe_vp_type = ppe_vp_type;

	ath12k_dbg(NULL, ATH12K_DBG_PPE,
		   "Updated PPE VP port no %d for dev %s type %d\n",
		   ahvif->dp_vif.ppe_vp_num, wdev->netdev->name, ahvif->dp_vif.ppe_vp_type);
exit:
	return ret;
}

int ath12k_vif_set_mtu(struct ath12k_vif *ahvif, int mtu)
{
	struct wireless_dev *wdev = ieee80211_vif_to_wdev(ahvif->vif);
	int ppe_vp_num = ahvif->dp_vif.ppe_vp_num;

	if (!wdev)
		return -ENODEV;

	if (ppe_vp_mtu_set(ppe_vp_num, mtu) != PPE_VP_STATUS_SUCCESS) {
		pr_err("\ndev:%px, dev->name:%s mtu %d vp num = %d set failed ",
			wdev->netdev, wdev->netdev->name, mtu, ppe_vp_num);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_vif_set_mtu);


static void
ath12k_dp_rx_ppeds_fse_update_flow_info(struct ath12k_base *ab,
					struct rx_flow_info *flow_info,
					struct ath12k_fse_update_event *event,
					struct ppe_drv_fse_rule_info *ppe_flow_info,
					int operation)
{
	struct hal_flow_tuple_info *tuple_info = &flow_info->flow_tuple_info;
	struct ppe_drv_fse_tuple *ppe_tuple = &ppe_flow_info->tuple;

	ath12k_dbg(ab, ATH12K_DBG_DP_FST, "%s S_IP:%x:%x:%x:%x,sPort:%u,D_IP:%x:%x:%x:%x,dPort:%u,Proto:%d,flags:%d",
		   fse_state_to_string(operation),
		   ppe_tuple->src_ip[0], ppe_tuple->src_ip[1],
		   ppe_tuple->src_ip[2], ppe_tuple->src_ip[3],
		   ppe_tuple->src_port,
		   ppe_tuple->dest_ip[0], ppe_tuple->dest_ip[1],
		   ppe_tuple->dest_ip[2], ppe_tuple->dest_ip[3],
		   ppe_tuple->dest_port,
		   ppe_tuple->protocol,
		   ppe_flow_info->flags);

	tuple_info->src_port = ppe_tuple->src_port;
	tuple_info->dest_port = ppe_tuple->dest_port;
	tuple_info->l4_protocol = ppe_tuple->protocol;
	flow_info->fse_metadata =
		u32_replace_bits(flow_info->fse_metadata, ppe_flow_info->vp_num,
				 ATH12K_DP_RX_FSE_FL_VP_NUM_MASK);
	flow_info->fse_metadata =
		u32_replace_bits(flow_info->fse_metadata, ppe_flow_info->macid,
				 ATH12K_DP_RX_FSE_FL_EGRESS_MACID_MASK);

	if (ppe_flow_info->flags & PPE_DRV_FSE_IPV4) {
		flow_info->is_addr_ipv4 = 1;
		tuple_info->src_ip_31_0 = ntohl(ppe_tuple->src_ip[0]);
		tuple_info->dest_ip_31_0 = ntohl(ppe_tuple->dest_ip[0]);
	} else if (ppe_flow_info->flags & PPE_DRV_FSE_IPV6) {
		tuple_info->src_ip_31_0 = ntohl(ppe_tuple->src_ip[3]);
		tuple_info->src_ip_63_32 = ntohl(ppe_tuple->src_ip[2]);
		tuple_info->src_ip_95_64 = ntohl(ppe_tuple->src_ip[1]);
		tuple_info->src_ip_127_96 = ntohl(ppe_tuple->src_ip[0]);

		tuple_info->dest_ip_31_0 = ntohl(ppe_tuple->dest_ip[3]);
		tuple_info->dest_ip_63_32 = ntohl(ppe_tuple->dest_ip[2]);
		tuple_info->dest_ip_95_64 = ntohl(ppe_tuple->dest_ip[1]);
		tuple_info->dest_ip_127_96 = ntohl(ppe_tuple->dest_ip[0]);
	}

	if (ppe_flow_info->flags & PPE_DRV_FSE_DS)
		flow_info->use_ppe = 1;

	/* Populate event from flow_info (already in host order) */
	if (event) {
		event->src_port = tuple_info->src_port;
		event->dest_port = tuple_info->dest_port;
		event->protocol = tuple_info->l4_protocol;

		if (flow_info->is_addr_ipv4) {
			event->version = 4;
			event->src_ip[0] = tuple_info->src_ip_31_0;
			event->dest_ip[0] = tuple_info->dest_ip_31_0;
			/* Clear unused array elements */
			event->src_ip[1] = event->src_ip[2] = event->src_ip[3] = 0;
			event->dest_ip[1] = event->dest_ip[2] = event->dest_ip[3] = 0;
		} else {
			event->version = 6;
			event->src_ip[0] = tuple_info->src_ip_127_96;
			event->src_ip[1] = tuple_info->src_ip_95_64;
			event->src_ip[2] = tuple_info->src_ip_63_32;
			event->src_ip[3] = tuple_info->src_ip_31_0;
			event->dest_ip[0] = tuple_info->dest_ip_127_96;
			event->dest_ip[1] = tuple_info->dest_ip_95_64;
			event->dest_ip[2] = tuple_info->dest_ip_63_32;
			event->dest_ip[3] = tuple_info->dest_ip_31_0;
		}
	}
}

bool
ath12k_dp_rx_ppeds_fse_add_flow_entry(struct ppe_drv_fse_rule_info *ppe_flow_info)
{
	struct rx_flow_info flow_info = { 0 };
	struct ath12k_fse_update_event ev = {};
	struct wireless_dev *wdev;
	struct ieee80211_vif *vif;
	struct ath12k_base *ab = NULL;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	struct net_device *dev = ppe_flow_info->dev;
	unsigned long links;
	u8 link_id;
	int ret;

	if (!ath12k_fse_enable)
		return false;

	if (!dev)
		return false;

	wdev = dev->ieee80211_ptr;

	vif = wdev_to_ieee80211_vif_vlan(wdev, false);
	if (!vif) {
		pr_warn("FSE flow rule addition failed vif = NULL\n");
		return false;
	}

	ahvif = ath12k_vif_to_ahvif(vif);

	ah = ahvif->ah;
	if (!ah) {
		pr_warn("FSE flow rule addition failed ah = NULL \n");
		return false;
	}

	rcu_read_lock();

	links = ahvif->links_map;

	for_each_set_bit(link_id, &links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = rcu_dereference(ahvif->link[link_id]);
		if (!arvif)
			continue;

		ar = arvif->ar;
		if (!ar)
			continue;
		ab = ar->ab;
		if (ab)
			break;
	}

	rcu_read_unlock();

	/* TODO: protect ag->ab[] by spin lock */
	/* NOTE: ag->ab[0] can be any arbitirary ab but first ab is used to cover non-MLO */
	if (!ab || ab->is_bypassed) {
		pr_warn("FSE flow rule addition failed ab = NULL or ab is bypassed\n");
		return false;
	}

	/* Populate both flow_info and event in one call (no redundant ntohl) */
	ath12k_dp_rx_ppeds_fse_update_flow_info(ab, &flow_info, &ev, ppe_flow_info,
						FSE_RULE_ADD);

	ret = ath12k_dp_rx_flow_add_entry(ab, &flow_info);

	/* Fire FSE_UPDATE notification for PPE flow addition */
	if (!ret && ath12k_fse_update_notif_has_listeners()) {
		ev.op = ATH12K_FSE_OP_ADD;
		ath12k_fse_update_notif_call_chain(&ev);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_ppeds_fse_add_flow_entry);

bool
ath12k_dp_rx_ppeds_fse_del_flow_entry(struct ppe_drv_fse_rule_info *ppe_flow_info)
{
	struct rx_flow_info flow_info = { 0 };
	struct ath12k_fse_update_event ev = {};
	struct wireless_dev *wdev;
	struct ath12k_hw *ah;
	struct ieee80211_hw *hw = NULL;
	struct ath12k_base *ab = NULL;
	struct net_device *dev = ppe_flow_info->dev;
	int ret;

	if (!ath12k_fse_enable)
		return false;

	if (!dev)
		return false;

	wdev = dev->ieee80211_ptr;

	hw = wiphy_to_ieee80211_hw(wdev->wiphy);

	if (!hw) {
		pr_warn("failed to find the ieee80211_hw for netdev %p\n", dev);
		return false;
	}

	ah = hw->priv;
	if (!ah) {
		pr_warn("FSE flow rule deletion failed ah = NULL \n");
		return false;
	}

	/* TODO: protect ah->radio[0].ab  by spin lock */
	/* NOTE: ah->radio[0].ab can be any arbitirary ab but first ab is used to cover non-MLO */
	ab = ah->radio[0].ab;
	if (!ab) {
		pr_warn("FSE flow rule deletion failed ab = NULL \n");
		return false;
	}

	if ((test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) ||
	     test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) &&
	    !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags))

		return false;

	/* Populate both flow_info and event in one call (no redundant ntohl) */
	ath12k_dp_rx_ppeds_fse_update_flow_info(ab, &flow_info, &ev, ppe_flow_info,
						FSE_RULE_DELETE);

	ret = ath12k_dp_rx_flow_delete_entry(ab, &flow_info);

	/* Fire FSE_UPDATE notification for PPE flow deletion */
	if (!ret && ath12k_fse_update_notif_has_listeners()) {
		ev.op = ATH12K_FSE_OP_DELETE;
		ath12k_fse_update_notif_call_chain(&ev);
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_dp_rx_ppeds_fse_del_flow_entry);

void ath12k_dp_ppeds_service_enable_disable(struct ath12k_base *ab,
					    bool enable)
{
	struct ath12k_mlo_dp_umac_reset *mlo_umac_reset = &ab->ag->mlo_umac_reset;
	struct ath12k_dp *dp = ab->dp;

	if (enable) {
		ab->dp->ppe.task_id = atomic_inc_return(&mlo_umac_reset->task_id);
		atomic_set(&mlo_umac_reset->task_id, ab->dp->ppe.task_id);
	}

	if (dp->ppe.nss_plugin_ops)
		dp->ppe.nss_plugin_ops->service_status_update(dp->ppe.ds_node_id,
							      enable);
}
EXPORT_SYMBOL(ath12k_dp_ppeds_service_enable_disable);

int ath12k_vif_get_vp_num(struct ath12k_vif *ahvif, struct net_device *dev)
{
	int ppe_vp_num = ATH12K_INVALID_PPE_VP_NUM;
	struct nss_plugins_ops *plugin_ops = ath12k_get_registered_nss_plugin_ops();

	if (dev->ieee80211_ptr &&
	    dev->ieee80211_ptr->iftype == NL80211_IFTYPE_MONITOR)
		return 0;

	if (!plugin_ops)
		return -EINVAL;

	ppe_vp_num = plugin_ops->get_vp_num(dev);

	if (ppe_vp_num <= 0) {
		ath12k_dbg(NULL, ATH12K_DBG_PPE,
			   "Error in getting VP num for netdev %s err %d\n",
			   dev->name, ppe_vp_num);
		return -ENOSR;
	}

	ahvif->dp_vif.ppe_vp_num = ppe_vp_num;

	ath12k_dbg(NULL, ATH12K_DBG_PPE,
			"PPE VP device '%s' VP num:%d assigned by ath client\n",
			dev->name, ahvif->dp_vif.ppe_vp_num);
	return 0;
}
EXPORT_SYMBOL(ath12k_vif_get_vp_num);

int ath12k_nss_plugin_register_ops(struct ath12k_base *ab)
{
	struct ath12k_dp *dp = ab->dp;

	dp->ppe.nss_plugin_ops = qca_nss_wifi_plugins_get_ops();
	nss_plugin_ops_ptr = dp->ppe.nss_plugin_ops;
	if (!dp->ppe.nss_plugin_ops) {
		ath12k_err(ab, " error in registering nss plugin ops\n");
		return -EINVAL;
	}

	ath12k_info(ab, "NSS plugin ops registered successfully\n");

	return 0;
}
EXPORT_SYMBOL(ath12k_nss_plugin_register_ops);

void ath12k_nss_plugin_unregister_ops(struct ath12k_base *ab)
{
	ab->dp->ppe.nss_plugin_ops = NULL;
}
EXPORT_SYMBOL(ath12k_nss_plugin_unregister_ops);

void ath12k_ppe_ds_notify_freq_range(struct ath12k *ar, u32 freq_low, u32 freq_high)
{
	struct ath12k_dp *dp = ath12k_ab_to_dp(ar->ab);
	struct ppe_ds_wlan_node_freq node_freq;

	if (!test_bit(ATH12K_FLAG_PPE_DS_ENABLED, &ar->ab->dev_flags))
		return;

	if (!dp->ppe.nss_plugin_ops || !dp->ppe.nss_plugin_ops->get_ds_node_freq)
		return;

	node_freq = (struct ppe_ds_wlan_node_freq) {
	.low_freq = freq_low,
	.high_freq = freq_high,
	};

	dp->ppe.nss_plugin_ops->get_ds_node_freq(dp->ppe.ds_node_id, &node_freq);
}
EXPORT_SYMBOL(ath12k_ppe_ds_notify_freq_range);
