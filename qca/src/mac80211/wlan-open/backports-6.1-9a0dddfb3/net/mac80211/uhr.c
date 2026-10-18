// SPDX-License-Identifier: GPL-2.0-only
/*
 * UHR handling
 *
 * Copyright(c) 2025-2026 Intel Corporation
 */

#include "ieee80211_i.h"
#include "smd.h"

void
ieee80211_uhr_cap_ie_to_sta_uhr_cap(struct ieee80211_sub_if_data *sdata,
				    struct ieee80211_supported_band *sband,
				    const struct ieee80211_uhr_cap_elem *uhr_cap,
				    u8 uhr_cap_len,
				    struct link_sta_info *link_sta)
{
	struct ieee80211_sta_uhr_cap *sta_uhr_cap = &link_sta->pub->uhr_cap;

	memset(sta_uhr_cap, 0, sizeof(*sta_uhr_cap));

	if (!uhr_cap || !ieee80211_get_uhr_iftype_cap_vif(sband, &sdata->vif))
		return;

	sta_uhr_cap->has_uhr = true;

	sta_uhr_cap->mac = uhr_cap->fixed.mac;
	sta_uhr_cap->phy = uhr_cap->fixed.phy;
}

void
ieee80211_uhr_npca_elem_to_sta_uhr_npca_info(struct ieee80211_sub_if_data *sdata,
					   struct ieee80211_supported_band *sband,
					   const struct ieee80211_uhr_operation *uhr_oper,
					   struct link_sta_info *link_sta)
{
	struct ieee80211_sta_uhr_npca_info *npca_info = &link_sta->pub->npca_info;
	const struct ieee80211_sta_uhr_cap *uhr_cap;
	const struct ieee80211_uhr_npca_info *npca;

	memset(npca_info, 0, sizeof(*npca_info));
	link_sta->pub->npca_primary_channel = 0;
	link_sta->pub->npca_puncture_bitmap = 0;

	uhr_cap = ieee80211_get_uhr_iftype_cap_vif(sband, &sdata->vif);
	if (!uhr_cap)
		return;

	if (!(uhr_cap->mac.mac_cap[0] & IEEE80211_UHR_MAC_CAP0_NPCA_SUPP))
		return;

	if (!uhr_oper)
		return;

	npca = ieee80211_uhr_npca_info(uhr_oper);
	if (!npca)
		return;

	npca_info->npca_enabled = true;
	npca_info->npca_min_dur_threshold =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_MIN_DUR_THRESH);
	npca_info->npca_switch_delay =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_SWITCH_DELAY);
	npca_info->npca_switch_back_delay =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_SWITCH_BACK_DELAY);
	npca_info->npca_initial_qsrc =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_INIT_QSRC);
	npca_info->npca_moplen =
		le32_get_bits(npca->params, IEEE80211_UHR_NPCA_PARAMS_MOPLEN);

	link_sta->pub->npca_primary_channel =
		le32_get_bits(npca->params,
			      IEEE80211_UHR_NPCA_PARAMS_PRIMARY_CHAN);
	if (npca->params &
	    cpu_to_le32(IEEE80211_UHR_NPCA_PARAMS_DIS_SUBCH_BMAP_PRES))
		link_sta->pub->npca_puncture_bitmap =
			le16_to_cpu(npca->dis_subch_bmap[0]);
}

VISIBLE_IF_MAC80211_KUNIT u8 *
ieee80211_add_uhr_link_reconf_elem(struct sk_buff *skb,
				   struct ieee80211_sub_if_data *sdata,
				   const u8 *target_addr,
				   struct ieee80211_mgd_assoc_data *add_links_data,
				   u16 removed_links,
				   u8 type)
{
	struct ieee80211_local *local = sdata->local;
	struct ieee80211_multi_link_elem *ml_elem;
	struct ieee80211_mle_basic_common_info *common;
	enum nl80211_iftype iftype = ieee80211_vif_type_p2p(&sdata->vif);
	unsigned int link_id;
	__le16 eml_capa = 0, mld_capa_ops = 0;
	u8 common_size, var_common_size;
	u8 *ml_elem_len;
	u16 capab = 0;
	__le16 ext_mld_capa_ops = add_links_data ? add_links_data->ext_mld_capa_ops : 0;

	/* Add the ML reconfiguration element */
	skb_put_u8(skb, WLAN_EID_EXTENSION);
	ml_elem_len = skb_put(skb, 1);
	skb_put_u8(skb, WLAN_EID_EXT_EHT_MULTI_LINK);
	ml_elem = skb_put(skb, sizeof(*ml_elem));

	/* Control Field:
	 * Type = Reconfiguration
	 * Bit 0 = Sender (Non-AP) MLD MAC Present
	 * Bit 4 = Target AP MLD MAC Present (MANDATORY for ST Prep)
	 */
	ml_elem->control =
		cpu_to_le16(IEEE80211_ML_CONTROL_TYPE_RECONF |
			    IEEE80211_MLC_RECONF_PRES_MLD_MAC_ADDR |
			    IEEE80211_MLC_RECONF_PRES_TARGET_AP_MLD_MAC_ADDR);

	/* Common Info Fixed Part: Length (1) + Sender MLD MAC (6) */
	common_size = sizeof(*common);

	/* Variable Common Info Calculation */
	var_common_size = 0;

	/* 1. Capabilities (if adding links) */
	if (add_links_data) {
		const struct wiphy_iftype_ext_capab *ift_ext_capa =
			cfg80211_get_iftype_ext_capa(local->hw.wiphy, iftype);

		if (ift_ext_capa) {
			eml_capa = cpu_to_le16(ift_ext_capa->eml_capabilities);
			mld_capa_ops =
				cpu_to_le16(ift_ext_capa->mld_capa_and_ops);
		}

		/* MLD capabilities and operation */
		var_common_size += 2;

		/* EML capabilities */
		if (eml_capa & cpu_to_le16((IEEE80211_EML_CAP_EMLSR_SUPP |
					    IEEE80211_EML_CAP_EMLMR_SUPPORT)))
			var_common_size += 2;
	}

	if (ext_mld_capa_ops)
		var_common_size += 2;

	/* 2. Target AP MLD MAC (Bit 4) - Always present for ST Prep */
	var_common_size += ETH_ALEN;

	/* Add the Common Info */
	common = skb_put(skb, common_size);

	/* Common Info Length includes the length field itself */
	common->len = common_size + var_common_size;

	/* Bit 0: Sender (Non-AP) MLD MAC Address */
	memcpy(common->mld_mac_addr, sdata->vif.addr, ETH_ALEN);

	/* Append Variable Common Info */
	/* A. Capabilities */
	if (add_links_data) {
		if (eml_capa &
		    cpu_to_le16((IEEE80211_EML_CAP_EMLSR_SUPP |
				 IEEE80211_EML_CAP_EMLMR_SUPPORT))) {
			ml_elem->control |=
				cpu_to_le16(IEEE80211_MLC_RECONF_PRES_EML_CAPA);
			skb_put_data(skb, &eml_capa, sizeof(eml_capa));
		}

		ml_elem->control |=
			cpu_to_le16(IEEE80211_MLC_RECONF_PRES_MLD_CAPA_OP);

		skb_put_data(skb, &mld_capa_ops, sizeof(mld_capa_ops));
	}

	if (ext_mld_capa_ops) {
		ml_elem->control |=
			cpu_to_le16(IEEE80211_MLC_RECONF_PRES_EXT_MLD_CAPA_OP);
		skb_put_data(skb, &ext_mld_capa_ops, sizeof(ext_mld_capa_ops));
	}

	/* B. Bit 4: Target AP MLD MAC Address (Must be AFTER capabilities) */
	skb_put_data(skb, target_addr, ETH_ALEN);

	/* Capability setup for Per-STA loop */
	if (sdata->u.mgd.flags & IEEE80211_STA_ENABLE_RRM)
		capab |= WLAN_CAPABILITY_RADIO_MEASURE;

	/* Add the per station profile */
	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		u8 *subelem_len = NULL;
		u16 ctrl;
		const u8 *addr;

		/* Skip links that are not changing */
		if (!(removed_links & BIT(link_id)) &&
		    (!add_links_data || !add_links_data->link[link_id].bss))
			continue;

		ctrl = link_id |
		       IEEE80211_MLE_STA_RECONF_CONTROL_STA_MAC_ADDR_PRESENT;

		if (removed_links & BIT(link_id)) {
			struct ieee80211_bss_conf *conf =
				sdata_dereference(sdata->vif.link_conf[link_id],
						  sdata);
			if (!conf)
				continue;

			addr = conf->addr;
			ctrl |= u16_encode_bits(
				IEEE80211_MLE_STA_RECONF_CONTROL_OPERATION_TYPE_DEL_LINK,
				IEEE80211_MLE_STA_RECONF_CONTROL_OPERATION_TYPE);
		} else {
			addr = add_links_data->link[link_id].addr;
			ctrl |= IEEE80211_MLE_STA_RECONF_CONTROL_COMPLETE_PROFILE |
				u16_encode_bits(
				IEEE80211_MLE_STA_RECONF_CONTROL_OPERATION_TYPE_ADD_LINK,
				IEEE80211_MLE_STA_RECONF_CONTROL_OPERATION_TYPE);
		}

		skb_put_u8(skb, IEEE80211_MLE_SUBELEM_PER_STA_PROFILE);
		subelem_len = skb_put(skb, 1);

		/* Control Field (2 bytes) */
		put_unaligned_le16(ctrl, skb_put(skb, sizeof(ctrl)));

		/* STA Info Length (1 byte) - matches ieee80211_build_ml_reconf_req */
		skb_put_u8(skb, 1 + ETH_ALEN); /* STA Info Length */

		/* MAC Address (6 bytes) */
		skb_put_data(skb, addr, ETH_ALEN);

		if (!(removed_links & BIT(link_id))) {
			u16 link_present_elems[PRESENT_ELEMS_MAX] = {};
			size_t extra_used;
			void *capab_pos;
			u8 qos_info;

			capab_pos = skb_put(skb, 2);

			extra_used =
				ieee80211_add_link_elems(sdata, skb, &capab, NULL,
						 add_links_data->link[link_id].elems,
						 add_links_data->link[link_id].elems_len,
						 link_id, NULL,
						 link_present_elems,
						 add_links_data);

			if (add_links_data->link[link_id].elems)
				skb_put_data(skb,
					     add_links_data->link[link_id].elems +
					     extra_used,
					     add_links_data->link[link_id].elems_len -
					     extra_used);

			if (sdata->u.mgd.flags & IEEE80211_STA_UAPSD_ENABLED) {
				qos_info = sdata->u.mgd.uapsd_queues;
				qos_info |= (sdata->u.mgd.uapsd_max_sp_len <<
					     IEEE80211_WMM_IE_STA_QOSINFO_SP_SHIFT);
			} else {
				qos_info = 0;
			}

			ieee80211_add_wmm_info_ie(skb_put(skb, 9), qos_info);
			put_unaligned_le16(capab, capab_pos);
		}

		/* Update Per-STA Profile Length */
		ieee80211_fragment_element(skb, subelem_len,
					   IEEE80211_MLE_SUBELEM_FRAGMENT);
	}

	/* Update Multi-Link Element Length */
	ieee80211_fragment_element(skb, ml_elem_len, WLAN_EID_FRAGMENT);

	return skb_tail_pointer(skb);
}
EXPORT_SYMBOL_IF_MAC80211_KUNIT(ieee80211_add_uhr_link_reconf_elem);

VISIBLE_IF_MAC80211_KUNIT u8 *
ieee80211_add_uhr_link_reconf_smd_bss_trans_elem(struct sk_buff *skb,
					 struct ieee80211_sub_if_data *sdata,
					 struct cfg80211_smd_prepare_req *req_params)
{
	struct ieee80211_local *local = sdata->local;
	u8 *elem_len;
	u8 *pos;

	/* Element ID: Extension */
	skb_put_u8(skb, WLAN_EID_EXTENSION);
	elem_len = skb_put(skb, 1);
	/* Element ID Extension: SMD BSS Transition Parameters */
	skb_put_u8(skb, WLAN_EID_EXT_BSS_SMD_TRANS_PARAMS); /* TBD by IEEE */

	if (req_params->type == 1) {
		/*
		 * ST Execution ST Info field (IEEE 802.11bn Figure 9-aa47):
		 *   Octet 1: Reserved
		 *   Octet 2: Presence Bitmap (bit 0 = DL TID Bitmap Present)
		 *   Octet 3: DL TID Bitmap (present if Presence Bitmap bit 0 = 1)
		 */
		u8 exec_presence_bitmap = 0;

		/* Presence Bitmap: bit 0 = DL TID Bitmap Present (Figure 9-aa48) */
		if (req_params->dl_tid_bitmap)
			exec_presence_bitmap |= BIT(0);
		skb_put_u8(skb, exec_presence_bitmap);

		/* DL TID Bitmap (conditional on Presence Bitmap bit 0) */
		if (req_params->dl_tid_bitmap)
			skb_put_u8(skb, req_params->dl_tid_bitmap);
	} else {
		/*
		 * ST Preparation ST Info field:
		 *   Octet 1: Common Info (DL/UL SN not transferred flags)
		 *   Octet 2-3: Listen Interval
		 *   Octet 4: Presence Bitmap (bit 0 = SCS List present)
		 *   Variable: SCS List (if present)
		 */
		u8 st_control = 0;
		u16 li;

		/* Common Info: DL/UL SN not transferred flags */
		if (req_params->request_dl_sn_not_transferred)
			st_control |= BIT(0);
		if (req_params->request_ul_sn_not_transferred)
			st_control |= BIT(1);
		skb_put_u8(skb, st_control);

		/* Listen Interval */
		li = local->hw.conf.listen_interval;
		pos = skb_put(skb, 2);
		put_unaligned_le16(li, pos);

		/* SCS List */
		if (req_params->scs_list_len > 0) {
			skb_put_u8(skb, req_params->scs_list_len);
			skb_put_data(skb, req_params->scs_list, req_params->scs_list_len);
		}
	}

	/* Fill in element length */
	*elem_len = skb_tail_pointer(skb) - elem_len - 1;

	return skb_tail_pointer(skb);
}
EXPORT_SYMBOL_IF_MAC80211_KUNIT(ieee80211_add_uhr_link_reconf_smd_bss_trans_elem);

VISIBLE_IF_MAC80211_KUNIT u8
*ieee80211_add_uhr_link_reconf_dh_nonce_elems(struct sk_buff *skb,
					      struct cfg80211_smd_prepare_req *req_params)
{
	if (req_params->dh_public_key && req_params->dh_public_key_len) {
		skb_put_u8(skb, WLAN_EID_EXTENSION);
		skb_put_u8(skb, 1 + req_params->dh_public_key_len);
		skb_put_u8(skb, 32);
		skb_put_data(skb, req_params->dh_public_key,
			     req_params->dh_public_key_len);
	}

	if (req_params->snonce && req_params->snonce_len) {
		skb_put_u8(skb, WLAN_EID_EXTENSION);
		skb_put_u8(skb, 1 + req_params->snonce_len);
		skb_put_u8(skb, WLAN_EID_EXT_FILS_NONCE);
		skb_put_data(skb, req_params->snonce, req_params->snonce_len);
	}

	return skb_tail_pointer(skb);
}
EXPORT_SYMBOL_IF_MAC80211_KUNIT(ieee80211_add_uhr_link_reconf_dh_nonce_elems);


VISIBLE_IF_MAC80211_KUNIT size_t
ieee80211_uhr_link_reconf_frame_calc_len(struct ieee80211_sub_if_data *sdata,
					 struct ieee80211_mgd_assoc_data *add_links_data,
					 struct cfg80211_smd_prepare_req *req_params)
{
	struct ieee80211_local *local = sdata->local;
	enum nl80211_iftype iftype = ieee80211_vif_type_p2p(&sdata->vif);
	size_t size;
	unsigned int link_id;

	/* Base frame size with UHR action frame */
	size = local->hw.extra_tx_headroom +
	       offsetofend(struct ieee80211_mgmt, u.action.u.uhr_link_reconf_req);

	/* ML element (control field); matches ieee80211_build_ml_reconf_req() style */
	size += sizeof(struct ieee80211_multi_link_elem);

	/* Common info fixed part */
	size += sizeof(struct ieee80211_mle_basic_common_info);

	/* Variable common info fields, aligned with ieee80211_build_ml_reconf_req() */
	if (add_links_data) {
		const struct wiphy_iftype_ext_capab *ift_ext_capa =
			cfg80211_get_iftype_ext_capa(local->hw.wiphy,
						     ieee80211_vif_type_p2p(&sdata->vif));
		__le16 eml_capa = 0;

		if (ift_ext_capa)
			eml_capa = cpu_to_le16(ift_ext_capa->eml_capabilities);

		/* MLD capabilities and operation */
		size += 2;

		/* EML capabilities */
		if (eml_capa & cpu_to_le16((IEEE80211_EML_CAP_EMLSR_SUPP |
					    IEEE80211_EML_CAP_EMLMR_SUPPORT)))
			size += 2;
	}

	if (req_params->ml_reconf.ext_mld_capa_ops)
		size += 2;

	/* Target AP MLD MAC (Bit 4) - Always present for ST Prep */
	size += ETH_ALEN;

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		struct cfg80211_bss *cbss;
		size_t elems_len;

		if (req_params->ml_reconf.rem_links & BIT(link_id)) {
			/* ML reconf req: +sizeof(profile)+ETH_ALEN (no explicit +2) */
			size += sizeof(struct ieee80211_mle_per_sta_profile) + ETH_ALEN;
			continue;
		}

		if (!add_links_data || !add_links_data->link[link_id].bss)
			continue;

		cbss = add_links_data->link[link_id].bss;
		elems_len = add_links_data->link[link_id].elems_len;

		/* Per-STA Profile: subelement header + profile + STA MAC */
		size += 2 + sizeof(struct ieee80211_mle_per_sta_profile) + ETH_ALEN;

		/* WMM info IE */
		size += 9;

		/* Common elements sizing (includes elems_len) */
		size += ieee80211_link_common_elems_size(sdata, iftype, cbss, elems_len);
	}

	/* SMD BSS Transition Parameters element: only present for Type=0 or Type=1 */
	if (req_params->type <= 1) {
		size += 3; /* EID(1)+len(1)+ext_id(1) */
		if (req_params->type == 1) {
			size += 1;
			if (req_params->dl_tid_bitmap)
				size += 1;
		} else {
			/* type == 0: ST Preparation */
			size += 3;
			if (req_params->scs_list_len > 0)
				size += 1 + req_params->scs_list_len;
		}
	}

	/* DH/Nonce elements: only for ST Preparation (add_links_data != NULL) */
	if (add_links_data) {
		if (req_params->dh_public_key && req_params->dh_public_key_len)
			size += 3 + req_params->dh_public_key_len;
		if (req_params->snonce && req_params->snonce_len)
			size += 3 + req_params->snonce_len;
	}
	/*
	 * Slack for ieee80211_fragment_element() overhead etc.
	 */
	size += 128;

	return size;
}
EXPORT_SYMBOL_IF_MAC80211_KUNIT(ieee80211_uhr_link_reconf_frame_calc_len);

/* CORRECTED: Main ST Prep request builder using assoc_data */
struct sk_buff *
ieee80211_build_uhr_link_reconf_req(struct ieee80211_sub_if_data *sdata,
				    const u8 *target_addr,
				    struct cfg80211_smd_prepare_req *req_params,
				    struct ieee80211_mgd_assoc_data *assoc_data)
{
	struct ieee80211_if_managed *ifmgd;
	struct ieee80211_local *local;
	struct ieee80211_mgmt *mgmt;
	struct sk_buff *skb;
	size_t frame_len;
	int target_slot;

	if (!target_addr || !req_params)
		return NULL;

	local = sdata->local;
	ifmgd = &sdata->u.mgd;

	lockdep_assert_wiphy(local->hw.wiphy);

	if (!ifmgd->associated)
		return NULL;

	target_slot = ieee80211_smd_find_target_by_addr(sdata, target_addr);
	if (target_slot < 0) {
		sdata_info(sdata, "smd: ST Prep: target not found in prep array\n");
		return NULL;
	}

	frame_len = ieee80211_uhr_link_reconf_frame_calc_len(sdata, assoc_data,
							     req_params);

	skb = alloc_skb(frame_len, GFP_KERNEL);
	if (!skb)
		return NULL;

	skb_reserve(skb, local->hw.extra_tx_headroom);

	mgmt = skb_put_zero(skb,
			    offsetofend(struct ieee80211_mgmt,
					u.action.u.uhr_link_reconf_req));

	mgmt->frame_control = cpu_to_le16(IEEE80211_FTYPE_MGMT |
					IEEE80211_STYPE_ACTION);

	const u8 *ap_addr = sdata->vif.cfg.ap_addr; /* default: serving AP */

	if (!assoc_data) {
		/* ST Execution: determine recipient from exec_path */
		int i;

		for (i = 0; i < ifmgd->max_prepared_targets; i++) {
			if (ifmgd->prep_targets[i].valid &&
			    ether_addr_equal(ifmgd->prep_targets[i].target_mld_addr,
					     target_addr)) {
				if (ifmgd->prep_targets[i].exec_path == 1)
					ap_addr = target_addr;
				break;
			}
		}
	}

	memcpy(mgmt->da, ap_addr, ETH_ALEN);
	memcpy(mgmt->sa, sdata->vif.addr, ETH_ALEN);
	memcpy(mgmt->bssid, ap_addr, ETH_ALEN);

	mgmt->u.action.category = WLAN_CATEGORY_PROTECTED_UHR;

	mgmt->u.action.u.uhr_link_reconf_req.action_code =
		WLAN_PROTECTED_UHR_ACTION_LINK_RECONFIG_REQ;

	mgmt->u.action.u.uhr_link_reconf_req.dialog_token =
		sdata->u.mgd.prep_targets[target_slot].dialog_token;

	mgmt->u.action.u.uhr_link_reconf_req.type =
		assoc_data ? IEEE80211_UHR_LINK_RECONF_TYPE_ST_PREP
			   : IEEE80211_UHR_LINK_RECONF_TYPE_ST_EXEC;

	sdata_dbg(sdata, "smd: st frame info %pM slot=%d token=%u\n",
		  sdata->vif.addr, target_slot,
		  mgmt->u.action.u.uhr_link_reconf_req.dialog_token);

	ieee80211_add_uhr_link_reconf_elem(skb, sdata, target_addr,
					   assoc_data,
					   0, /* removed_links = 0 for SMD */
					   assoc_data ?
					   IEEE80211_UHR_LINK_RECONF_TYPE_ST_PREP :
					   IEEE80211_UHR_LINK_RECONF_TYPE_ST_EXEC);

	ieee80211_add_uhr_link_reconf_smd_bss_trans_elem(skb, sdata, req_params);

	if (assoc_data)
		ieee80211_add_uhr_link_reconf_dh_nonce_elems(skb, req_params);

	if (assoc_data)
		assoc_data->is_smd_prep = true;

	return skb;
}

int ieee80211_tx_smd_uhr_link_reconf(struct ieee80211_sub_if_data *sdata,
				     const u8 *target_addr,
				     struct cfg80211_smd_prepare_req *req_params,
				     struct ieee80211_mgd_assoc_data *assoc_data)
{
	struct ieee80211_local *local = sdata->local;
	struct sk_buff *skb;
	int link_id = -1;

	lockdep_assert_wiphy(local->hw.wiphy);

	skb = ieee80211_build_uhr_link_reconf_req(sdata, target_addr,
						  req_params, assoc_data);
	if (!skb)
		return -ENOMEM;

	/* Request TX status to enable frame transmission tracking */
	if (ieee80211_hw_check(&local->hw, REPORTS_TX_ACK_STATUS)) {
		u64 cookie;
		int ret;

		IEEE80211_SKB_CB(skb)->flags |= IEEE80211_TX_CTL_REQ_TX_STATUS |
						IEEE80211_TX_INTFL_NL80211_FRAME_TX;

		/* Set skb->dev so that attached ACK SKB has correct device reference */
		skb->dev = sdata->dev;

		ret = ieee80211_attach_ack_skb(local, skb, &cookie, GFP_ATOMIC);
		if (ret) {
			kfree_skb(skb);
			return ret;
		}
	}

	if (req_params->tx_link_id >= 0 &&
	    (sdata->vif.active_links & BIT(req_params->tx_link_id)))
		link_id = (int)req_params->tx_link_id;

	ieee80211_tx_skb_tid(sdata, skb, 7, link_id);

	if (req_params->type == 0) {
		struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
		int active = 0, i;

		for (i = 0; i < ifmgd->max_prepared_targets; i++)
			if (ifmgd->prep_targets[i].valid)
				active++;
		sdata_info(sdata, "ST: send prep request to %pM (prep %d/%d)\n",
			   target_addr, active, ifmgd->max_prepared_targets);
	} else {
		struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
		struct ieee80211_smd_prep_target *tgt = NULL;
		int i;

		for (i = 0; i < ifmgd->max_prepared_targets; i++) {
			if (ifmgd->prep_targets[i].valid &&
			    ether_addr_equal(ifmgd->prep_targets[i].target_mld_addr,
					     target_addr)) {
				tgt = &ifmgd->prep_targets[i];
				break;
			}
		}
		if (tgt && tgt->exec_path == 1)
			sdata_info(sdata, "ST: send exec request to %pM via TAP\n",
				   target_addr);
		else
			sdata_info(sdata, "ST: send exec request to %pM via SAP\n",
				   target_addr);
	}

	return 0;
}
EXPORT_SYMBOL(ieee80211_tx_smd_uhr_link_reconf);


static void ieee80211_process_smd_prep_resp(struct ieee80211_sub_if_data *sdata,
					    struct ieee80211_mgmt *mgmt,
					    size_t len)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	struct ieee80211_mgd_assoc_data *assoc_data = NULL;
	struct ieee80211_smd_prep_target *target = NULL;
	struct cfg80211_uhr_reconfig_done done = {0};
	struct ieee80211_elems_parse_params parse_params = {
		.from_ap = true,
		.link_id = -1,
	};
	struct ieee802_11_elems *elems = NULL;
	u8 *pos;
	u8 *ie_start = NULL;
	size_t ie_len = 0;
	size_t orig_len = len;
	int i, target_slot = -1;
	int tap_link_id;
	u16 status_code = WLAN_STATUS_SUCCESS;
	unsigned int link_id;
	bool success = false;
	u16 transitioning_links;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!ieee80211_sdata_running(sdata))
		return;

	sdata_dbg(sdata, "smd: processing prep response\n");

	if (mgmt->u.action.u.uhr_link_reconf_resp.status_code) {
		u16 sc = le16_to_cpu(mgmt->u.action.u.uhr_link_reconf_resp.status_code);

		sdata_err(sdata, "smd: prep response status code invalid %u\n", sc);
		return;
	}

	pos = mgmt->u.action.u.uhr_link_reconf_resp.variable;
	len -= offsetofend(typeof(*mgmt), u.action.u.uhr_link_reconf_resp);

	/* Find matching target first */
	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		if (ifmgd->prep_targets[i].valid &&
		    ifmgd->prep_targets[i].dialog_token ==
		    mgmt->u.action.u.uhr_link_reconf_resp.dialog_token) {
			target = &ifmgd->prep_targets[i];
			target_slot = i;
			break;
		}
	}

	if (!target) {
		sdata_err(sdata, "smd: no target for token=%d\n",
			  mgmt->u.action.u.uhr_link_reconf_resp.dialog_token);
		return;
	}

	assoc_data = target->assoc_data;
	if (!assoc_data || !assoc_data->is_smd_prep) {
		sdata_info(sdata, "smd: no preparation in progress for target\n");
		goto out;
	}

	/* Drop AP retransmissions of a Prep Response we already processed. */
	if (target->target_sta) {
		sdata_dbg(sdata,
			  "smd: duplicate ST Prep Resp from %pM (token=%u), dropping\n",
			  mgmt->sa,
			  mgmt->u.action.u.uhr_link_reconf_resp.dialog_token);
		return;
	}

	if (len < mgmt->u.action.u.uhr_link_reconf_resp.count * 3) {
		sdata_info(sdata,
			   "smd: unexpected len=%zu, count=%u\n",
			   len, mgmt->u.action.u.uhr_link_reconf_resp.count);
		goto out_fail;
	}

	target->prepared_links_mask = 0;
	for (i = 0; i < mgmt->u.action.u.uhr_link_reconf_resp.count; i++) {
		u16 status = get_unaligned_le16(pos + 1);

		if (i == 0)
			status_code = status;

		link_id = *pos;

		if (status == WLAN_STATUS_SUCCESS) {
			target->prepared_links_mask |= BIT(link_id);
			success = true;
		} else {
			sdata_info(sdata, "smd: link %d rejected status=%d %pM\n",
				   link_id, status, target->target_mld_addr);
		}
		pos += 3;
		len -= 3;
	}

	if (!success) {
		sdata_err(sdata, "smd: all links rejected\n");
		goto out_fail;
	}

	ie_start = pos;
	ie_len = (u8 *)mgmt + orig_len - pos;

	parse_params.mode = IEEE80211_CONN_MODE_UHR;
	parse_params.start = ie_start;
	parse_params.len = ie_len;
	elems = ieee802_11_parse_elems(ie_start, ie_len,
				       IEEE80211_FTYPE_MGMT |
				       IEEE80211_STYPE_ACTION,
				       NULL);
	if (!elems) {
		sdata_err(sdata, "smd: failed to parse response IEs\n");
		goto out_fail;
	}
	if (elems->ml_basic && elems->ml_basic_len >= 3) {
		int ret;

		ret = ieee80211_smd_parse_ml_persta(sdata,
						    (const u8 *)elems->ml_basic,
						    elems->ml_basic_len,
						    target);
		if (ret < 0)
			sdata_err(sdata, "smd: ml element parse failed\n");
	}

	for (tap_link_id = 0; tap_link_id < IEEE80211_MLD_MAX_NUM_LINKS; tap_link_id++) {
		struct cfg80211_bss *fresh_bss;
		struct cfg80211_bss *old_bss = target->assoc_data->link[tap_link_id].bss;
		const u8 *tap_bssid = target->assoc_data->link[tap_link_id].addr;

		if (!old_bss)
			continue;
		if (is_zero_ether_addr(tap_bssid))
			continue;

		/* Refresh the BSS entry but constrain the lookup to the channel
		 * the AP was operating on at PREP-request time.  Without this,
		 * cfg80211_get_bss() may return a stale entry from a different
		 * channel if the AP changed bands between the last scan and the
		 * PREP response, producing a wrong band-to-SAP-slot remap.
		 */
		fresh_bss = cfg80211_get_bss(sdata->local->hw.wiphy,
					     old_bss->channel, tap_bssid,
					     NULL, 0,
					     IEEE80211_BSS_TYPE_ANY,
					     IEEE80211_PRIVACY_ANY);
		if (!fresh_bss)
			continue;

		cfg80211_put_bss(sdata->local->hw.wiphy, old_bss);
		target->assoc_data->link[tap_link_id].bss = fresh_bss;
	}

	if (ieee80211_smd_build_link_id_remap(sdata, target)) {
		sdata_info(sdata, "smd: link id remap failed\n");
		goto out_free_elems;
	}

	while (pos + 2 <= (u8 *)mgmt + orig_len) {
		u8 id = pos[0];
		u8 elen = pos[1];
		u8 *data = pos + 2;


		if (pos + 2 + elen > (u8 *)mgmt + orig_len)
			break;

		if (id == WLAN_EID_EXTENSION && elen >= 1) {
			switch (data[0]) { /* Extension ID */
			case WLAN_EID_EXT_BSS_SMD_TRANS_PARAMS:
				/* SMD BSS Transition Parameters */
				if (elen >= 5) {
					int ret;

					ret = ieee80211_smd_parse_trans_params(sdata,
									       data + 1,
									       elen - 1,
									       target);
					if (ret < 0) {
						sdata_info(sdata,
							   "smd: trans params parse failed\n");
						goto out_free_elems;
					}
				}
				break;

			case WLAN_EID_EXT_DH_PARAMETER:
				/* Diffie-Hellman Parameter (for PTK derivation) */
				if (elen >= 3) { /* Extension ID + at least 1 byte data */
					u8 *dh_data = data + 1; /* Skip extension ID */
					size_t dh_len = elen - 1;

					if (dh_len <= sizeof(target->dh_resp)) {
						memcpy(target->dh_resp, dh_data, dh_len);
						target->dh_resp_len = dh_len;
						target->st_prep_flags |=
							ST_PREP_FLAG_GOT_DH;
					}
				}
				break;

			case WLAN_EID_EXT_FILS_NONCE:
				/* Nonce (for PTK derivation) */
				if (elen == WLAN_NONCE_LEN + 1) { /* EID + nonce */
					u8 *nonce_data = data + 1; /* Skip extension ID */

					memcpy(target->target_anonce,
					       nonce_data, WLAN_NONCE_LEN);
					target->st_prep_flags |= ST_PREP_FLAG_GOT_NONCE;
				}
				break;

			case 54: /* OCI element (EID ext 54) — OCV channel info */
				/* Passed to userspace via raw frame; no validation */
				break;

			case 88: /* MSCS Descriptor element (EID ext 88) */
				/* Passed to userspace via raw frame */
				break;
			default:
				break;
			}
		}

		pos += 2 + elen;
	}

	ether_addr_copy(done.target_mld_addr, target->target_mld_addr);
	done.status_code = status_code;  /* Overall status from first link */

	memcpy(done.anonce, target->target_anonce, sizeof(done.anonce));
	memcpy(done.target_dh_public_key, target->dh_resp,
	       min(sizeof(done.target_dh_public_key), target->dh_resp_len));

	done.timeout_value_tu = assoc_data->smd_timeout;
	done.buf = (const u8 *)mgmt;
	done.len = orig_len;
	done.prepared_links = target->prepared_links_mask;

	for_each_set_bit(i, (unsigned long *)&target->prepared_links_mask,
			 IEEE80211_MLD_MAX_NUM_LINKS) {
		done.links[i].link_id = i;
		done.links[i].bss = target->assoc_data->link[i].bss;
		done.links[i].addr = target->assoc_data->link[i].addr;
	}

	target->elems = elems;

	if (ieee80211_smd_compute_prep_bitmaps(sdata, target, assoc_data))
		goto out_fail;

	transitioning_links = target->transitioning_links;

	char acc[16] = "", rej[16] = "";

	if (target->prepared_links_mask)
		scnprintf(acc, sizeof(acc), "accepted=0x%x ",
			  target->prepared_links_mask);
	if (target->rejected_links_mask)
		scnprintf(rej, sizeof(rej), "rejected=0x%x ",
			  target->rejected_links_mask);
	sdata_info(sdata, "ST: preparing to %pM (%s%sprimary=%d remap=%s)\n",
		   target->target_mld_addr, acc, rej,
		   target->primary_link_id,
		   target->link_id_remap ? "yes (diff-links)" : "no (same-links)");

	if (ieee80211_smd_prep_setup(sdata, target, mgmt, ie_start, ie_len)) {
		sdata_err(sdata, "smd: prep_setup failed\n");
		goto out_fail;
	}

	if (target->is_preferred_target) {
		if (ieee80211_smd_prep_activate(sdata, target)) {
			sdata_err(sdata, "smd: prep_activate failed\n");
			goto out_fail;
		}
	}

	done.status_code = WLAN_STATUS_SUCCESS;
	done.transitioning_links = transitioning_links;
	if (!target->exec_path && target->is_preferred_target && transitioning_links)
		done.link_transition_state = NL80211_SMD_LINK_STATE_PARTIAL;
	else
		done.link_transition_state = NL80211_SMD_LINK_STATE_PENDING;
	cfg80211_uhr_reconfig_resp_done(sdata->dev, &done);

	/* PREP response arrived — cancel the PREP timeout before arming
	 * the EXEC timeout. Now that prep and exec use separate work fields
	 * there is no shared-timer corruption risk, but we still cancel the
	 * prep timeout here since the prep phase is done.
	 */
	if (target->prep_timeout_started) {
		wiphy_delayed_work_cancel(sdata->local->hw.wiphy,
					  &target->prep_timeout_work);
		target->prep_timeout_started = false;
	}

	if (!target->exec_path && assoc_data->smd_timeout)
		ieee80211_smd_start_exec_timeout(sdata, target, assoc_data->smd_timeout);

	sdata_info(sdata, "ST: prep complete - %pM (transition=0x%x)\n",
		   target->target_mld_addr, transitioning_links);

	status_code = WLAN_STATUS_SUCCESS;
	set_bit(SDATA_STATE_SMD_BSS_TRANSITION, &sdata->state);
	goto out;

out_free_elems:
	kfree(elems);
out_fail:
	sdata_info(sdata, "ST: aborted — %pM (phase=prep)\n",
		   target->target_mld_addr);
	status_code = WLAN_STATUS_UNSPECIFIED_FAILURE;

out:
	if (status_code != WLAN_STATUS_SUCCESS)
		ieee80211_smd_prep_reset_target(sdata, target, status_code, 0, false);
}

static void ieee80211_process_smd_exec_resp(struct ieee80211_sub_if_data *sdata,
					    struct ieee80211_mgmt *mgmt, size_t len)
{
	struct ieee80211_if_managed *ifmgd = &sdata->u.mgd;
	struct ieee80211_smd_prep_target *target = NULL;
	struct cfg80211_uhr_reconfig_done done = {0};
	u16 status_code = WLAN_STATUS_SUCCESS;
	u16 accepted_links = 0;
	bool found_smd_trans_params = false;
	const struct element *elem;
	u8 group_key_data_len = 0;
	struct sta_info *cur_sta;
	u32 dl_drain_time_tu = 0;
	size_t orig_len = len;
	size_t ie_len;
	u8 *ie_start;
	u8 *pos;
	int i;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (!ieee80211_sdata_running(sdata))
		return;

	sdata_dbg(sdata, "smd: processing exec response\n");

	pos = mgmt->u.action.u.uhr_link_reconf_resp.variable;
	len -= offsetofend(typeof(*mgmt), u.action.u.uhr_link_reconf_resp);

	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		if (ifmgd->prep_targets && ifmgd->prep_targets[i].valid &&
		    ifmgd->prep_targets[i].dialog_token ==
		    mgmt->u.action.u.uhr_link_reconf_resp.dialog_token) {
			target = &ifmgd->prep_targets[i];
			break;
		}
	}

	if (!target) {
		sdata_err(sdata, "smd: exec no target for token=%d\n",
			  mgmt->u.action.u.uhr_link_reconf_resp.dialog_token);
		return;
	}

	/*
	 * Drop AP retransmissions during the async DL-drain window;
	 * re-entering execute_transition() corrupts the transition FSM.
	 */
	if (!target->exec_path && target->execution_in_progress) {
		sdata_dbg(sdata,
			  "smd: duplicate ST Exec Resp from %pM (token=%u), dropping\n",
			  mgmt->sa,
			  mgmt->u.action.u.uhr_link_reconf_resp.dialog_token);
		return;
	}

	cur_sta = sta_info_get(sdata, sdata->vif.cfg.ap_addr);
	if (cur_sta)
		ether_addr_copy(target->current_sta_addr, cur_sta->sta.addr);
	else
		eth_zero_addr(target->current_sta_addr);

	if (mgmt->u.action.u.uhr_link_reconf_resp.status_code) {
		u16 sc = le16_to_cpu(mgmt->u.action.u.uhr_link_reconf_resp.status_code);

		sdata_err(sdata, "smd: exec response status code invalid %u\n", sc);
		return;
	}

	for (i = 0; i < mgmt->u.action.u.uhr_link_reconf_resp.count && len >= 3; i++) {
		u8 link_id = *pos;
		u16 status = get_unaligned_le16(pos + 1);

		if (i == 0)
			status_code = status;

		if (status != WLAN_STATUS_SUCCESS) {
			sdata_info(sdata, "smd: exec link %u rejected with status %u\n",
				   link_id, status);
			status_code = status;
		} else {
			accepted_links |= BIT(link_id);
		}

		pos += 3;
		len -= 3;
	}

	if (status_code != WLAN_STATUS_SUCCESS) {
		sdata_info(sdata, "smd: exec rejected with status %u\n",
			   status_code);
		ieee80211_smd_prep_reset_target(sdata, target, status_code, 1, false);
		return;
	}

	ieee80211_vif_cfg_change_notify(sdata, BSS_CHANGED_MLD_VALID_LINKS);

	if (accepted_links && len >= 1 && pos[0] != WLAN_EID_EXTENSION) {
		group_key_data_len = *pos++;
		len--;

		if (len < group_key_data_len) {
			sdata_err(sdata,
				  "smd: exec group key data truncated (%u > %zu)\n",
				  group_key_data_len, len);
			ieee80211_smd_prep_reset_target(sdata, target,
						WLAN_STATUS_UNSPECIFIED_FAILURE, 1,
						false);
			return;
		}


		pos += group_key_data_len;
		len -= group_key_data_len;
	}
	ie_start = pos;
	ie_len = len;

	for_each_element(elem, ie_start, ie_len) {
		const u8 *data = elem->data;
		int data_len = elem->datalen;

		if (elem->id == WLAN_EID_FRAGMENT)
			continue;

		if (elem->id == WLAN_EID_EXTENSION && data_len >= 1) {
			u8 ext_id = data[0];

			switch (ext_id) {
			case WLAN_EID_EXT_BSS_SMD_TRANS_PARAMS:
				if (!ieee80211_smd_parse_exec_trans_params(
						sdata, data + 1,
						data_len - 1,
						&dl_drain_time_tu)) {
					found_smd_trans_params = true;
				} else {
					sdata_info(sdata, "smd: exec trans_params parse failed\n");
				}
				break;
			case WLAN_EID_EXT_KEY_DELIVERY:
				break;
			case 54:
				/* OCI element - operating channel validation */
				break;
			default:
				break;
			}
		}
	}

	if (!found_smd_trans_params)
		sdata_info(sdata, "smd: exec no trans_params, default dl_drain\n");

	sdata_info(sdata, "smd: exec dl_drain=%u TU\n", dl_drain_time_tu);

	if (dl_drain_time_tu > 0 && WARN_ON(target->exec_path))
		sdata_info(sdata, "ST: exec complete SAP draining DL (%u TU)\n",
			   dl_drain_time_tu);
	else
		sdata_info(sdata, "smd: exec complete, no drain (exec_path=%u)\n",
			   target->exec_path);

	done.status_code = WLAN_STATUS_SUCCESS;
	done.buf = (const u8 *)mgmt;
	done.len = orig_len;
	done.transitioning_links = target->transitioning_links;
	ether_addr_copy(done.target_mld_addr, target->target_mld_addr);

	for (i = 0; i < ifmgd->max_prepared_targets; i++) {
		struct ieee80211_smd_prep_target *t =
			&ifmgd->prep_targets[i];

		if (t->valid && t != target)
			ieee80211_smd_prep_reset_target(sdata, t,
							WLAN_STATUS_REQUEST_DECLINED,
							0, false);
	}

	if (target->exec_path) {
		ieee80211_smd_prep_complete_target(sdata, target);
		done.link_transition_state = NL80211_SMD_LINK_STATE_COMPLETE;
		cfg80211_uhr_reconfig_resp_done(sdata->dev, &done);
		return;
	}

	if (ieee80211_smd_execute_transition(sdata, target, dl_drain_time_tu)) {
		sdata_info(sdata, "ST: aborted — %pM (phase=exec)\n",
			   target->target_mld_addr);
		ieee80211_smd_prep_reset_target(sdata, target,
						WLAN_STATUS_UNSPECIFIED_FAILURE, 1,
						false);
		return;
	}

	done.link_transition_state = NL80211_SMD_LINK_STATE_DL_DRAIN;
	cfg80211_uhr_reconfig_resp_done(sdata->dev, &done);
}

void ieee80211_process_uhr_reconf_resp(struct ieee80211_sub_if_data *sdata,
				       struct ieee80211_mgmt *mgmt, size_t len)
{
	u8 type;

	lockdep_assert_wiphy(sdata->local->hw.wiphy);

	if (len < offsetofend(typeof(*mgmt), u.action.u.uhr_link_reconf_resp))
		return;

	type = mgmt->u.action.u.uhr_link_reconf_resp.type;

	switch (type) {
	case IEEE80211_UHR_LINK_RECONF_TYPE_ST_PREP:
		ieee80211_process_smd_prep_resp(sdata, mgmt, len);
		break;
	case IEEE80211_UHR_LINK_RECONF_TYPE_ST_EXEC:
		ieee80211_process_smd_exec_resp(sdata, mgmt, len);
		break;
	default:
		break;
	}
}
