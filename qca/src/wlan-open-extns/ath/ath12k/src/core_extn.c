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
#include "../core.h"
#include "../ce.h"
#include "../hw.h"
#include "../mac.h"
#include "../debug.h"
#include "../peer.h"
#include "ath12k_cmn_extn.h"
#include "../net/mac80211/ieee80211_i.h"
#include "esp_extn.h"
#include "vendor_extn.h"
#include "qcn_extns/cbs_extn.h"
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/device.h>
#include <linux/slab.h>
#include "../wmi.h"
#include "ini.h"
#include "rropinfo.h"
#include "qcn_extns/if_meta_hdr.h"
#include "vendor_extn.h"
#include "ranging_extn.h"

struct ath12k_vif_sysfs_entry {
	struct kobject kobj;
	struct ath12k *ar;
	u8 radio_idx;
	u8 vif_id;
};

static int ath12k_mlo_set_3_link_forced_primary_umac(struct ath12k_hw *ah,
					      struct ath12k_vif *ahvif,
					      struct ath12k_sta *ahsta,
					      unsigned long valid_links,
					      int *psoc_id);
static int ath12k_get_total_stations_ab(struct ath12k_base *ab);
static int ath12k_get_total_mlo_peers_ab(struct ath12k_base *ab);

static void ath12k_mgmt_rx_inject_hidden_ssid(struct ath12k *ar,
					      struct sk_buff *skb,
					      struct ath12k_wmi_mgmt_rx_arg *rx_ev)
{
	struct ath12k_link_vif *arvif;
	struct ath12k_vif *ahvif;
	struct ieee80211_mgmt *mgmt;
	u8 *ssid_ie;
	u8 ssid[IEEE80211_MAX_SSID_LEN];
	u8 ssid_len;
	int offset;
	int ssid_offset;

	arvif = ar->scan.arvif;
	if (!arvif || !arvif->ahvif) {
		return;
	}

	mgmt = (struct ieee80211_mgmt *)skb->data;
	ahvif = arvif->ahvif;
	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	if (!ahvif->ath12k_vif_extn.ssid_len ||
	    !ahvif->ath12k_vif_extn.bssid_set ||
	    !ether_addr_equal(ahvif->ath12k_vif_extn.bssid, mgmt->bssid)) {
		spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
		return;
	}

	ssid_len = ahvif->ath12k_vif_extn.ssid_len;
	memcpy(ssid, ahvif->ath12k_vif_extn.ssid, ssid_len);
	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);

	if (ieee80211_is_beacon(mgmt->frame_control))
		offset = offsetof(struct ieee80211_mgmt, u.beacon.variable);
	else if (ieee80211_is_probe_resp(mgmt->frame_control))
		offset = offsetof(struct ieee80211_mgmt, u.probe_resp.variable);
	else {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "unsupported frame for hidden SSID injection for BSSID %pM ssid='%.*s'\n",
			   mgmt->bssid, ssid_len, ssid);
		return;
	}

	if (skb->len < offset) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "skb len %u smaller than offset %d for BSSID %pM ssid='%.*s'\n",
			   skb->len, offset, mgmt->bssid,
			   ssid_len, ssid);
		return;
	}

	ssid_ie = (u8 *)cfg80211_find_ie(WLAN_EID_SSID, skb->data + offset,
					 skb->len - offset);
	if (!ssid_ie || ssid_ie[1] != 0) {
		ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
			   "SSID IE not present or existing SSID is not hidden for BSSID %pM ssid='%.*s'\n",
			   mgmt->bssid, ssid_len, ssid);
		return;
	}

	ssid_offset = ssid_ie - skb->data;
	if (skb_tailroom(skb) < ssid_len &&
	    pskb_expand_head(skb, 0, ssid_len, GFP_ATOMIC))
		return;

	mgmt = (struct ieee80211_mgmt *)skb->data;
	ssid_ie = skb->data + ssid_offset;
	skb_put(skb, ssid_len);
	memmove(ssid_ie + 2 + ssid_len,
		ssid_ie + 2,
		skb_tail_pointer(skb) - (ssid_ie + 2 + ssid_len));

	ssid_ie[1] = ssid_len;
	memcpy(ssid_ie + 2, ssid, ssid_len);
	rx_ev->buf_len = skb->len;
	ath12k_dbg(ar->ab, ATH12K_DBG_SCAN,
		   "successful injection for BSSID %pM ssid='%.*s' buf_len %u\n",
		   mgmt->bssid, ssid_len, ssid,
		   rx_ev->buf_len);
}

void ath12k_mgmt_rx_event_extn(struct ath12k *ar, struct sk_buff *skb,
		struct ath12k_wmi_mgmt_rx_arg *rx_ev)
{
	struct ath12k_link_sta *arsta;
	struct ieee80211_hdr *hdr;
	struct ieee80211_channel *channel;
	struct cfg80211_inform_bss bss_meta = {};
	struct cfg80211_bss *cbss;
	s32 signal_dbm;

	if (!ar || !skb)
		return;

	if (skb->len < sizeof(*hdr))
		return;

	hdr = (struct ieee80211_hdr *)skb->data;
	channel = ieee80211_get_channel(ar->ah->hw->wiphy,
					rx_ev->chan_freq);

	if (ar->radio_cfg.radio_cfg_extn.scan_f_strict_passive_pch) {
		if (channel && channel->band == NL80211_BAND_5GHZ &&
		    (ieee80211_is_beacon(hdr->frame_control) ||
		     ieee80211_is_probe_resp(hdr->frame_control))) {
			ath12k_mgmt_rx_inject_hidden_ssid(ar, skb, rx_ev);
			hdr = (struct ieee80211_hdr *)skb->data;
		}
	}

	if (ar->ar_extn.cbs.cbs_enable &&
	    (ieee80211_is_beacon(hdr->frame_control) ||
	     ieee80211_is_probe_resp(hdr->frame_control) ||
	     ieee80211_is_s1g_beacon(hdr->frame_control))) {
		if (channel) {
			bss_meta.chan = channel;
			bss_meta.boottime_ns = ktime_get_boottime_ns();
			signal_dbm = (s32)rx_ev->rssi;
			bss_meta.signal = signal_dbm * 100;

			cbss = cfg80211_inform_bss_frame_data(ar->ah->hw->wiphy,
							      &bss_meta,
							      (struct ieee80211_mgmt *)hdr,
							      rx_ev->buf_len,
							      GFP_ATOMIC);
			if (cbss) {
				cfg80211_put_bss(ar->ah->hw->wiphy, cbss);
				return;
			}
		}
	}

	if (ieee80211_is_assoc_req(hdr->frame_control)) {
		spin_lock_bh(&ar->arsta_lock);
		arsta = ath12k_link_sta_find_by_addr(ar, hdr->addr2);
		if (!arsta || arsta->is_bridge_peer) {
			ath12k_warn(ar->ab, "arsta not found %pM\n",
					hdr->addr2);
		} else {
			arsta->arsta_extn.rssi_assoc = rx_ev->rssi;
		}
		spin_unlock_bh(&ar->arsta_lock);
	}
}

static void ath12k_mac_accumulate_avg_link_rssi(void *data,
		struct ieee80211_sta *sta)
{
	s32 *rssi_total_array = data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);

	if (ahsta->ahsta_extn.primary_soc_id >= 0 &&
	    ahsta->ahsta_extn.primary_soc_id < ATH12K_MAX_SOCS)
		rssi_total_array[ahsta->ahsta_extn.primary_soc_id] += ahsta->ahsta_extn.avg_link_rssi;
}

static int ath12k_get_total_mlo_peers_ab(struct ath12k_base *ab)
{
	int i;
	int total_ml_peers = 0;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev) {
			ar = pdev->ar;
			total_ml_peers += ar->num_ml_peers;
		}
	}
	return total_ml_peers;
}

static int ath12k_get_total_stations_ab(struct ath12k_base *ab)
{
	int i;
	int total_stations = 0;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev) {
			ar = pdev->ar;
			total_stations += ar->num_stations;
		}
	}

	if (total_stations) {
		/* Subtract 1 to exclude the station being added/evaluated from the count.
		 * This ensures we only count existing stations when making primary UMAC
		 * selection decisions for a new MLO association.
		 */
		return total_stations - 1;
	}

	return total_stations;
}

int ath12k_mlo_set_3_link_forced_primary_umac(struct ath12k_hw *ah,
				       struct ath12k_vif *ahvif,
				       struct ath12k_sta *ahsta,
				       unsigned long valid_links,
				       int *psoc_id)
{
	u32 target_type;
	struct ath12k *ar;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	bool found_qca5332 = false;
	bool found_qcn6432 = false;
	bool found_qcn9224 = false;
	bool found_qca5424 = false;
	u8 forced_psoc_id = 0;
	int num_links;
	u8 link_id;

	num_links = hweight32(valid_links);
	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "mlo primary selection: forced check sta %pM valid_links 0x%lx links %d\n",
			 sta->addr, valid_links, num_links);

	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;

		ar = arvif->ar;
		target_type = ar->ab->hw_rev;
		switch (target_type) {
		case ATH12K_HW_IPQ5332_HW10:
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2, "target: IPQ5332\n");
			found_qca5332 = true;
			break;

		case ATH12K_HW_QCN9274_HW10:
		case ATH12K_HW_QCN9274_HW20:
			found_qcn9224 = true;
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2, "target: QCN9274\n");
			break;

		case ATH12K_HW_QCN6432_HW10:
			found_qcn6432 = true;
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2, "target:QCN6432\n");
			forced_psoc_id = ath12k_get_ab_device_id(ar->ab);
			break;

		case ATH12K_HW_IPQ5424_HW10:
			found_qca5424 = true;
			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2, "target:IPQ5424\n");
			forced_psoc_id = ath12k_get_ab_device_id(ar->ab);
			break;

		default:
			ath12k_err(NULL,
				   "Target not valid for pumac tgt_type %d",
				   target_type);
			return -EINVAL;
		}
	}

	if (found_qca5332 && found_qcn6432 && found_qcn9224) {
		*psoc_id = forced_psoc_id;
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: forcing psoc %u as primary (3-link combo) for sta %pM\n",
				 forced_psoc_id, sta->addr);
		return 0;
	}

	/* In Marina-Waikiki platform, Marina will be chosen as
	 * the primary-umac
	 */
	if (found_qca5424) {
		*psoc_id = forced_psoc_id;
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: forcing psoc %u as primary (Marina-Waikiki) for sta %pM\n",
				 forced_psoc_id, sta->addr);
		return 0;
	}

	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "mlo primary selection: no forced primary UMAC match for sta %pM\n",
			 sta->addr);

	return  -1;
}

int ath12k_derive_link_rssi(struct ath12k_hw *ah,
			       struct ath12k_link_vif *arvif,
			       struct ath12k_link_vif *assoc_arvif, u8 rssi)
{
	/* get assoc link channel, radio , ab etcc */
	struct cfg80211_chan_def *channel = &arvif->chanctx.def;
	struct cfg80211_chan_def *assoc_channel = &assoc_arvif->chanctx.def;
	u16 ch_freq, assoc_freq;
	u8 tx_pow, assoc_tx_pow;
	s8 diff_txpow;
	u8 log10_freq;
	u8 derived_rssi;
	s16 ten_derived_rssi;
	s8 ten_diff_pl = 0;

	if (channel)
		ch_freq = channel->chan->center_freq;
	else
		ch_freq = 1;

	if (assoc_channel)
		assoc_freq = assoc_channel->chan->center_freq;
	else
		assoc_freq = 1;

	/*
	 *  diff of path loss (of two links) = log10(freq1) - log10(freq2)
	 *                       (since distance is constant)
	 *  since log10 is not available, we cameup with approximate ranges
	 */
	log10_freq = (ch_freq * 10) / assoc_freq;
	if (log10_freq >= 20 && log10_freq < 30)
		ten_diff_pl = 4;  /* 0.4 *10 */
	else if (log10_freq >= 11 && log10_freq < 20)
		ten_diff_pl = 1;  /* 0.1 *10 */
	else if (log10_freq >= 8 && log10_freq < 11)
		ten_diff_pl = 0; /* 0 *10 */
	else if (log10_freq >= 4 && log10_freq < 8)
		ten_diff_pl = -1; /* -0.1 * 10 */
	else if (log10_freq >= 1 && log10_freq < 4)
		ten_diff_pl = -4;  /* -0.4 * 10 */

	assoc_tx_pow = assoc_arvif->txpower;
	tx_pow = arvif->txpower;
	diff_txpow = tx_pow - assoc_tx_pow;

	ten_derived_rssi = (diff_txpow * 10) - ten_diff_pl + (rssi * 10);
	derived_rssi = ten_derived_rssi / 10;

	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "mlo rssi derive: ch_freq %u assoc_freq %u tx_pow %u assoc_tx_pow %u ten_diff_pl %d diff_txpow %d input_rssi %u derived_rssi %u\n",
			 ch_freq, assoc_freq, tx_pow, assoc_tx_pow,
			 ten_diff_pl, diff_txpow, rssi, derived_rssi);

	return derived_rssi;
}

static void ath12k_peer_calculate_avg_rssi(struct ath12k_hw *ah,
					struct ath12k_vif *ahvif,
					struct ath12k_sta *ahsta,
					unsigned long valid_links)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_link_vif *assoc_arvif = NULL;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	int total_rssi = 0;
	u8 num_psocs = 0;
	struct ath12k_link_sta *arsta;
	u8 link_id;
	int link_rssi;

	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;
		if (ahsta->assoc_link_id == link_id) {
			assoc_arvif = arvif;
			break;
		}
	}

	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;
		arsta = ahsta->link[link_id];
		if (!arsta)
			continue;
		num_psocs++;
		if (ahsta->assoc_link_id == link_id) {
			link_rssi = arsta->arsta_extn.rssi_assoc;
		} else if (assoc_arvif) {
			link_rssi = ath12k_derive_link_rssi(ah, arvif,
							    assoc_arvif,
							    arsta->arsta_extn.rssi_assoc);
		} else {
			link_rssi = arsta->arsta_extn.rssi_assoc;
		}

		total_rssi += link_rssi;
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo avg rssi: sta %pM link %u assoc %d rssi %d running_total %d\n",
				 sta->addr, link_id,
				 ahsta->assoc_link_id == link_id,
				 link_rssi, total_rssi);
	}

	if (!num_psocs)
		return;

	ahsta->ahsta_extn.avg_link_rssi = total_rssi / num_psocs;
	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "mlo avg rssi: sta %pM valid_links 0x%lx links %u avg %d assoc_link %u\n",
			 sta->addr, valid_links, num_psocs,
			 ahsta->ahsta_extn.avg_link_rssi, ahsta->assoc_link_id);
}

int get_linkid_from_psoc_id(struct ath12k_vif *ahvif,
			    unsigned long valid_links,
			    int soc_id,
			    u8 *pri_link_id)
{
	u8 link_id;
	struct ath12k_link_vif *arvif = NULL;
	bool is_link_found = false;

	/* Get link id from psoc_id */
	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;

		if (ath12k_get_ab_device_id(arvif->ar->ab) == soc_id) {
			*pri_link_id = link_id;
			is_link_found = true;
			break;
		}
	}

	if (is_link_found)
		return 0;

	return -1;
}

#define ML_PRIMARY_LINK_CONGESTION 30
int ath12k_get_best_primary_umac_w_rssi(struct ath12k_hw *ah,
					  struct ath12k_vif *ahvif,
					  struct ath12k_sta *ahsta,
					  unsigned long valid_links,
					  u8 *primary_link_id)
{
	struct ath12k_link_vif *arvif = NULL;
	struct ath12k_link_vif *assoc_arvif = NULL;
	struct ieee80211_sta *sta = ath12k_ahsta_to_sta(ahsta);
	u16 total_ml_sta_count = 0;
	u32 total_cap, cap;
	u16 bw;
	u8 link_id;
	u8 num_psocs = 0;
	u8 psoc_id = 0;
	u8 psoc_w_nosta = 0;
	bool plink_preferable[ATH12K_MAX_SOCS] = {0};
	bool ml_no_sta[ATH12K_MAX_SOCS] = {0};
	u16 ml_sta_count[ATH12K_MAX_SOCS] = {0};
	u16 ml_ch_width[ATH12K_MAX_SOCS];
	bool group_full[ATH12K_MAX_SOCS] = {0};
	u16 group_size[ATH12K_MAX_SOCS] = {0};
	u16 grp_size = 0;
	u16 group_full_count = 0;
	u8 cong = ML_PRIMARY_LINK_CONGESTION;
	u16 chan_width = 0;
	s32 sum_link_rssi[ATH12K_MAX_SOCS] = {0};
	s32 avg_rssi[ATH12K_MAX_SOCS] = {0};
	s32 diff_rssi[ATH12K_MAX_SOCS] = {0};
	s32 diff_low;
	int primary_psoc = -1, i;
	int hi_bw = 0;
	int sec_hi_bw = 0;
	int temp_bw = 0;
	int psoc_hi_idx = -1;
	int psoc_second_hi_idx = -1;
	int ret = -1;

	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "mlo primary selection: sta %pM avg_link_rssi %d valid_links 0x%lx\n",
			 sta->addr, ahsta->ahsta_extn.avg_link_rssi, valid_links);

	if (ath12k_mlo_set_3_link_forced_primary_umac(ah, ahvif, ahsta, valid_links,
				&primary_psoc) == 0) {
		goto exit_pri_link_selection;
	}

	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;
		if (ahsta->assoc_link_id == link_id) {
			assoc_arvif = arvif;
			break;
		}
	}

	if (!assoc_arvif)
		goto exit_pri_link_selection;

	/* Derive other Link RSSI and have average avg_link_rssi for ahsta */
	ath12k_peer_calculate_avg_rssi(ah, ahvif, ahsta, valid_links);

	/* calculate avg_link_rssi per ab(primary link soc), for all MLO STAs in ah. */
	ieee80211_iterate_stations_atomic(ah->hw,
					  ath12k_mac_accumulate_avg_link_rssi,
					  &sum_link_rssi);


	for_each_set_bit(link_id, &valid_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		arvif = ath12k_get_arvif_from_link_id(ahvif, link_id);
		if (!arvif || !arvif->ar)
			continue;

		if (!arvif->ar->ab->hw_params->is_plink_preferable) {
			ath12k_err(NULL, "Skip UMAC selection ...\n");
			psoc_id = ath12k_get_ab_device_id(arvif->ar->ab);
			if (psoc_id >= 0 && psoc_id < ATH12K_MAX_SOCS)
				plink_preferable[psoc_id] = false;
			continue;
		}

		psoc_id = ath12k_get_ab_device_id(arvif->ar->ab);
		if (psoc_id < 0 || psoc_id >= ATH12K_MAX_SOCS)
			continue;
		plink_preferable[psoc_id] = true;
		chan_width = arvif->chanctx.def.width;
		ml_ch_width[psoc_id] = ath12k_mac_get_chan_width(chan_width);
		if (!ath12k_get_total_stations_ab(arvif->ar->ab)) {
			/* If this PSOC has no stations */
			ml_no_sta[psoc_id] = true;
			psoc_w_nosta++;
		}

		ml_sta_count[psoc_id] =
			ath12k_get_total_mlo_peers_ab(arvif->ar->ab);
		total_ml_sta_count += ml_sta_count[psoc_id];
		num_psocs++;
		if (ml_sta_count[psoc_id])
			avg_rssi[psoc_id] = sum_link_rssi[psoc_id] /
					    ml_sta_count[psoc_id];

		if (!avg_rssi[psoc_id]) {
			diff_rssi[psoc_id] = psoc_id * 20;
			continue;
		}

		diff_rssi[psoc_id] = (ahsta->ahsta_extn.avg_link_rssi >= avg_rssi[psoc_id]) ?
			(ahsta->ahsta_extn.avg_link_rssi - avg_rssi[psoc_id]) :
			(avg_rssi[psoc_id] - ahsta->ahsta_extn.avg_link_rssi);
	}

	ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "%s: psoc_w_nosta: %d valid_links 0x%lx\n",
			 __func__, psoc_w_nosta, valid_links);

	for (i = 0; i < ATH12K_MAX_SOCS; i++) {

		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: psoc %d prefer %d width %u mlo_sta %u avg_rssi %d diff_rssi %d no_sta %d\n",
				 i, plink_preferable[i], ml_ch_width[i],
				 ml_sta_count[i], avg_rssi[i], diff_rssi[i],
				 ml_no_sta[i]);
	}

	primary_psoc = -1;
	if (psoc_w_nosta == 1) {
		for (i = 0; i < ATH12K_MAX_SOCS; i++) {
			if (ml_no_sta[i]) {
				primary_psoc = i;
				break;
			}
		}
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: choosing empty psoc %d as primary for sta %pM\n",
				 primary_psoc, sta->addr);
	} else if (psoc_w_nosta > 1) {
		/* If more the 2 socs have no sta then choose 2nd highest BW link */
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: choosing between psocs %d as primary for sta %pM\n",
				 psoc_w_nosta, sta->addr);
		hi_bw = 0;
		sec_hi_bw = 0;
		temp_bw = 0;
		psoc_hi_idx = -1;
		psoc_second_hi_idx = -1;
		for (i = 0; i < ATH12K_MAX_SOCS; i++) {
			if (!plink_preferable[i])
				continue;

			temp_bw = ml_ch_width[i];

			if (temp_bw > hi_bw) {
				sec_hi_bw = hi_bw;
				psoc_second_hi_idx = psoc_hi_idx;
				hi_bw = temp_bw;
				psoc_hi_idx = i;
			} else if (temp_bw < hi_bw && temp_bw > sec_hi_bw) {
				sec_hi_bw = temp_bw;
				psoc_second_hi_idx = i;
			}
		}
		primary_psoc = psoc_second_hi_idx;
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: multiple empty psocs hi_bw %d(psoc %d) second_bw %d(psoc %d) primary %d for sta %pM\n",
				 hi_bw, psoc_hi_idx, sec_hi_bw, psoc_second_hi_idx,
				 primary_psoc, sta->addr);
	} else  {
		/* no empty psoc */
		/* Capacity based grouping algorithm */
		total_cap = 0;
		for (i = 0; i < ATH12K_MAX_SOCS; i++) {
			bw = ml_ch_width[i];
			total_cap += bw * (100 - cong);
		}

		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mlo primary selection: no empty socs  capicity based algo as primary for sta %pM\n",
				sta->addr);

		group_full_count = 0;
		for (i = 0; i < ATH12K_MAX_SOCS; i++) {
			if (!plink_preferable[i])
				continue;

			bw = ml_ch_width[i];
			cap = bw * (100 - cong);
			total_ml_sta_count++;
			if (!total_cap)
				break;
			grp_size = total_ml_sta_count * ((cap * 100) / total_cap);
			group_size[i] = grp_size / 100;
			if (grp_size % 100)
				group_size[i]++;

			if (group_size[i] == 0)
				group_size[i] = 1;
			/* Mark link full if its target allocation is already reached */
			if (group_size[i] <= ml_sta_count[i]) {
				group_full[i] = true;
				group_full_count++;
			}
		}

		if ((num_psocs - group_full_count) == 1) {
			for (i = 0; i < ATH12K_MAX_SOCS; i++) {
				if (!plink_preferable[i])
					continue;
				if (group_full[i])
					continue;
				primary_psoc = i;
				break;
			}
		} else {
			diff_low = 0;

			ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				"mlo primary selection: no empty socs RSSI based algo as primary for sta %pM\n",
				sta->addr);
			/* find min diff of RSSI, based on it, allocate primary umac */
			for (i = 0; i < ATH12K_MAX_SOCS; i++) {
				if (!plink_preferable[i])
					continue;

				if (!diff_low || diff_low > diff_rssi[i]) {
					diff_low = diff_rssi[i];
					primary_psoc = i;
				}
			}
		}
	}

exit_pri_link_selection:

	if (primary_psoc >= 0) {
		if (!get_linkid_from_psoc_id(ahvif, valid_links, primary_psoc, primary_link_id)) {
			ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					 "mlo primary selection: computed primary psoc %d linkid %u for sta %pM\n",
					 primary_psoc, *primary_link_id, sta->addr);
			ret = 0;
		} else {
			ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					 "mlo primary selection: failed to map primary psoc %d to link for sta %pM\n",
					 primary_psoc, sta->addr);
		}
	} else if (primary_psoc < 0) {
		ath12k_dbg_level(NULL, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "mlo primary selection: failed to compute primary psoc for sta %pM\n",
				 sta->addr);
	}

	return ret;

}

static ssize_t macaddr_show(struct kobject *kobj, struct kobj_attribute *attr,
			    char *buf)
{
	struct ath12k_vif_sysfs_entry *entry;
	struct ath12k *ar;
	struct wiphy *wiphy;
	u8 mac_addr[ETH_ALEN];
	bool byte0 = true;
	u8 radio_idx;

	entry =	container_of(kobj, struct ath12k_vif_sysfs_entry, kobj);
	ar = entry->ar;
	wiphy = ar->ah->hw->wiphy;
	radio_idx = entry->radio_idx;

	if (ath12k_cfg_get(ar->ab, ATH12K_CFG_COHOSTED_BSS_IND_ENABLE) & (1 << ar->pdev_idx))
		byte0 = false;

	ath12k_compute_link_vif_mac(mac_addr,
				    wiphy->addresses[radio_idx].addr,
				    entry->vif_id,
				    byte0);

	return scnprintf(buf, PAGE_SIZE, "%pM\n", mac_addr);
}

static ssize_t flags_show(struct kobject *kobj, struct kobj_attribute *attr,
			  char *buf)
{
	struct ath12k_vif_sysfs_entry *entry;
	u32 used;

	entry = container_of(kobj, struct ath12k_vif_sysfs_entry, kobj);
	used = (entry->ar->vendor_mac_used_bitmap & (1 << entry->vif_id)) ? 1 : 0;

	return scnprintf(buf, PAGE_SIZE, "%u\n", used);
}

static ssize_t flags_store(struct kobject *kobj, struct kobj_attribute *attr,
			   const char *buf, size_t count)
{
	struct ath12k_vif_sysfs_entry *entry;
	struct ath12k *ar;
	unsigned long val;

	entry = container_of(kobj, struct ath12k_vif_sysfs_entry, kobj);
	ar = entry->ar;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	if (val == 1)
		ar->vendor_mac_used_bitmap |= (1 << entry->vif_id);
	else if (val == 0)
		ar->vendor_mac_used_bitmap &= ~(1 << entry->vif_id);
	else
		return -EINVAL;

	return count;
}

static struct kobj_attribute macaddr_attr = __ATTR_RO(macaddr);
static struct kobj_attribute flags_attr = __ATTR(flags, 0644, flags_show, flags_store);

static struct attribute *ath12k_vif_attrs[] = {
	&macaddr_attr.attr,
	&flags_attr.attr,
	NULL,
};

static const struct attribute_group ath12k_vif_attr_group = {
	.attrs = ath12k_vif_attrs,
};

static const struct attribute_group *ath12k_vif_default_groups[] = {
	&ath12k_vif_attr_group,
	NULL,
};

extern const struct sysfs_ops kobj_sysfs_ops;

static void ath12k_vif_kobj_release(struct kobject *kobj)
{
	struct ath12k_vif_sysfs_entry *entry =
		container_of(kobj, struct ath12k_vif_sysfs_entry, kobj);
	kfree(entry);
}

static struct kobj_type ath12k_vif_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_groups = ath12k_vif_default_groups,
	.release = ath12k_vif_kobj_release,
};

static void ath12k_sysfs_setup_vif_entry(struct ath12k *ar,
					 struct ath12k_vif_sysfs_entry *entry,
					 u8 vif_id,
					 u8 radio_idx)
{
	entry->vif_id = vif_id;
	entry->ar = ar;
	entry->radio_idx = radio_idx;
}

void ath12k_sysfs_init_extn(struct ath12k *ar)
{
	struct wiphy *wiphy = ar->ah->hw->wiphy;
	struct device *dev = wiphy_dev(wiphy);
	struct kobject *parent;
	struct ath12k_base *ab = ar->ab;
	char rname[16];
	int i, r;

	if (!dev)
		return;

	/* Place under the wiphy (class ieee80211 -> phyX) */
	parent = &dev->kobj;

	/*
	 * Create radio directory under phyX: radio<radio_idx>,
	 * determine radio_idx within ah->radio[]
	 */
	if (ab->ag->mlo_capable) {
		for (r = 0; r < ar->ah->num_radio; r++) {
			if (&ar->ah->radio[r] == ar) {
				snprintf(rname, sizeof(rname), "radio%d", r);
				break;
			}
		}
	} else {
		snprintf(rname, sizeof(rname), "radio%d", ar->pdev_idx);
	}

	ar->ar_extn.radio_kobj = kobject_create_and_add(rname, parent);
	if (!ar->ar_extn.radio_kobj)
		return;

	ar->ar_extn.vifs_kobj = kobject_create_and_add("vifs", ar->ar_extn.radio_kobj);
	if (!ar->ar_extn.vifs_kobj) {
		kobject_put(ar->ar_extn.radio_kobj);
		ar->ar_extn.radio_kobj = NULL;
		return;
	}

	for (i = 0; i <= 16; i++) {
		struct ath12k_vif_sysfs_entry *entry;
		char name[8];

		entry = kzalloc(sizeof(*entry), GFP_KERNEL);
		if (!entry)
			break;

		snprintf(name, sizeof(name), "vif%d", i);
		kobject_init(&entry->kobj, &ath12k_vif_ktype);
		if (kobject_add(&entry->kobj, ar->ar_extn.vifs_kobj, "%s", name)) {
			kobject_put(&entry->kobj);
			break;
		}
		for (r = 0; r < ar->ah->num_radio; r++) {
			if (&ar->ah->radio[r] == ar)
				break;
		}
		ath12k_sysfs_setup_vif_entry(ar, entry, i, r);
		ar->ar_extn.vif_entries[i] = entry;
	}
}

void ath12k_sysfs_cleanup_extn(struct ath12k *ar)
{
	int i;
	struct ath12k_vif_sysfs_entry *entry;

	if (ar->ar_extn.vifs_kobj) {
		for (i = 0; i <= 16; i++) {
			entry = ar->ar_extn.vif_entries[i];
			if (!entry)
				continue;
			kobject_put(&entry->kobj);
			ar->ar_extn.vif_entries[i] = NULL;
		}

		kobject_put(ar->ar_extn.vifs_kobj);
		ar->ar_extn.vifs_kobj = NULL;

		if (ar->ar_extn.radio_kobj) {
			kobject_put(ar->ar_extn.radio_kobj);
			ar->ar_extn.radio_kobj = NULL;
		}
	}
}


void ath12k_wmi_peer_migration_event_extn(struct ath12k_vif *ahvif)
{
	struct sta_240mhz_info *sta, *tmp;

	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	list_for_each_entry_safe(sta, tmp, &ahvif->ath12k_vif_extn.peer_240mhz_list_extn,
				 list) {
		list_del(&sta->list);
		kfree(sta);
	}

	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);
}

void ath12k_mac_hw_allocate_extn(struct ieee80211_hw *hw,
			  const struct ieee80211_ops_extn *ops_extn)
{
	struct ieee80211_local *local = wiphy_priv(hw->wiphy);
	local->ops_extn = ops_extn;
}

int
ath12k_add_sta_240mhz_info_extn(struct ath12k_vif *ahvif,
				u8 *mac,
				struct ieee80211_240mhz_vendor_oper_extn
				*params)
{
	struct sta_240mhz_info *sta;

	sta = kmalloc(sizeof(*sta), GFP_KERNEL);
	if (!sta)
		return -ENOMEM;

	ether_addr_copy(sta->mac_addr, mac);
	memcpy(&sta->params_240MHz_extn, params,
	       sizeof(sta->params_240MHz_extn));
	INIT_LIST_HEAD(&sta->list);
	spin_lock_bh(&ahvif->ath12k_vif_extn.data_lock);
	list_add_tail(&sta->list, &ahvif->ath12k_vif_extn.peer_240mhz_list_extn);
	spin_unlock_bh(&ahvif->ath12k_vif_extn.data_lock);

	return 0;
}

void ath12k_mac_init_arvif_extn(struct ath12k_vif *ahvif,
				struct ath12k_link_vif *arvif)
{
	/* Protects the extensions data update for an interface*/
	spin_lock_init(&ahvif->ath12k_vif_extn.data_lock);

	INIT_LIST_HEAD(&ahvif->ath12k_vif_extn.peer_240mhz_list_extn);

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
	ahvif->dp_vif.dp_extn.mhdr_len = sizeof(struct meta_hdr_s);
	ahvif->dp_vif.dp_extn.mhdr = 0;
	ahvif->dp_vif.dp_extn.mdbg = 0;
	ahvif->dp_vif.dp_extn.mesh_tx = 0;
#endif

	ath12k_gpr_arvif_init(arvif);
}

void ath12k_mac_deinit_arvif_extn(struct ath12k_link_vif *arvif)
{
	ath12k_gpr_arvif_deinit(arvif);
}

void ath12k_mac_remove_link_interface_extn(struct ath12k_link_vif *arvif)
{
	struct ath12k *ar = arvif->ar;

	if (ar->ar_extn.cbs.arvif && ar->ar_extn.cbs.arvif == arvif)
		ath12k_cbs_deinit(ar);
}

void ath12k_mgmt_stats_scan_radio_tx_reset(struct ath12k_vif *ahvif)
{
	struct ath12k_mgmt_frame_stats *stats = &ahvif->mgmt_stats;

	stats->tx_cmpl_succ_pkts = 0;
	stats->tx_cmpl_succ_bytes = 0;
	stats->tx_cmpl_retry_pkts = 0;
	stats->tx_cmpl_retry_bytes = 0;
	stats->tx_cmpl_err_pkts = 0;
	stats->tx_cmpl_err_bytes = 0;
}

void ath12k_mac_setup_radio_iface_comb_extn(
		struct ieee80211_iface_combination *comb)
{
	/* Add 320MHz support for 240MHz */
	comb->radar_detect_widths |= BIT(NL80211_CHAN_WIDTH_320);
}

int ath12k_wmi_op_rx_extn(enum wmi_tlv_event_id id, struct ath12k_base *ab,
			  struct sk_buff *skb)
{
	switch (id) {
	case WMI_HW_BLACKLIST_CHAN_EVENTID:
		ath12k_dbg(ab, ATH12K_DBG_REG, "Received Blacklist Event");
		if (ath12k_wmi_hw_blocklist_chan_event_extn(ab, skb))
			ath12k_warn(ab, "HBL extn handler failed\n");
		break;
	case WMI_ESP_ESTIMATE_EVENTID:
		ath12k_wmi_esp_estimate_event_extn(ab, skb);
		break;
	case WMI_PEER_RATECODE_LIST_EVENTID:
		ath12k_wmi_peer_ratecode_list_event_handler(ab, skb);
		break;
	case WMI_OEM_RESPONSE_EVENTID:
		ath12k_wmi_lowi_oem_response_event_extn(ab, skb);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * ath12k_mac_hw_register_extn - Extension hook for ath12k_mac_hw_register
 * @ah: ath12k_hw instance
 *
 * Conditionally sets WIPHY_FLAG_SUPPORTS_PARALLEL_HW_SCAN on the wiphy based
 * on the "parallel_hw_scan" INI key read from global.ini.  The flag is set
 * only when the INI is enabled (true); it is cleared otherwise, overriding
 * any unconditional assignment made by the core driver.
 *
 * Must be called from ath12k_mac_hw_register() after the wiphy has been
 * allocated and the INI context is available.
 */
void ath12k_mac_hw_register_extn(struct ath12k_hw *ah)
{
	struct ath12k *ar;
	struct wiphy *wiphy;

	if (!ah || !ah->hw || !ah->num_radio)
		return;

	ar = &ah->radio[0];
	wiphy = ah->hw->wiphy;

	if (ath12k_cfg_get(ar->ab, ATH12K_CFG_PARALLEL_HW_SCAN)) {
		wiphy->flags |= WIPHY_FLAG_SUPPORTS_PARALLEL_HW_SCAN;
		wiphy_ext_feature_set(wiphy,
				      NL80211_EXT_FEATURE_PARALLEL_HW_SCAN);
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "parallel hw scan enabled via INI\n");
	}
}

void ath12k_mac_setup_extn(struct ath12k *ar)
{
	ath12k_esp_init_extn(ar);
	ath12k_custom_txrx_init(ar);
	ath12k_extn_ini_debugfs_register(ar->ab);
	ar->ar_extn.cac_timeout_override_ms = NO_CAC_OVERRIDE;

	/* Initialize CBS params */
	ath12k_cbs_init(ar);

	/* Initialize WLAN interference stats cache */
	ar->ar_extn.wlan_intf.prev_valid = false;
	ar->ar_extn.wlan_intf.curr_valid = false;
	memset(&ar->ar_extn.wlan_intf.prev_stats, 0,
	       sizeof(ar->ar_extn.wlan_intf.prev_stats));
	memset(&ar->ar_extn.wlan_intf.curr_stats, 0,
	       sizeof(ar->ar_extn.wlan_intf.curr_stats));

	init_completion(&ar->ar_extn.lowi_oem_done);
}

void ath12k_mac_cleanup_unregister_extn(struct ath12k *ar)
{
	ath12k_extn_ini_debugfs_unregister(ar->ab);
	ath12k_cbs_deinit(ar);
	ath12k_esp_cleanup_extn(ar);
	ath12k_custom_txrx_deinit(ar);
}

int ath12k_wmi_tlv_scan_radio_caps_ext2(struct ath12k_base *ab, u16 tag,
					       u16 len, const void *ptr,
					       void *data)
{
	const struct wmi_scan_radio_capabilities_ext2 *cap = ptr;
	struct ath12k_pdev *pdev;
	u32 phy_id, flags, caps;
	int i;
	if (tag != WMI_TAG_SCAN_RADIO_CAPABILITIES_EXT2) {
		ath12k_err(ab,
			   "Unexpected Tag 0x%x (Expected 0x%x)\n",
			   tag, WMI_TAG_SCAN_RADIO_CAPABILITIES_EXT2);
		return -EPROTO;
	}
	phy_id = le32_to_cpu(cap->phy_id);
	flags  = le32_to_cpu(cap->flags);
	for (i = 0; i < ab->num_radios; i++) {
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "Checking fw_pdev[%d].phy_id=%u\n",
			   i, ab->fw_pdev[i].phy_id);
		if (ab->fw_pdev[i].phy_id == phy_id) {
			pdev = &ab->pdevs[i];
			caps = 0;
			if (flags & WMI_SCAN_RADIO_CAP_SCAN_RADIO)
				caps |= ATH12K_SCAN_RADIO_CAP_SUPPORTED;
			if (flags & WMI_SCAN_RADIO_CAP_DFS_ENABLED)
				caps |= ATH12K_SCAN_RADIO_CAP_DFS_ENABLED;
			if (flags & WMI_SCAN_RADIO_CAP_BLANKING_SUPPORTED)
				caps |= ATH12K_SCAN_RADIO_CAP_BLANKING;

			pdev->cap.scan_radio_caps = caps;
			ath12k_info(ab,
				    "pdev %d scan-radio caps: 0x%x (supported=%d dfs=%d blanking=%d)\n",
				    pdev->pdev_id,
				    pdev->cap.scan_radio_caps,
				    !!(caps & ATH12K_SCAN_RADIO_CAP_SUPPORTED),
				    !!(caps & ATH12K_SCAN_RADIO_CAP_DFS_ENABLED),
				    !!(caps & ATH12K_SCAN_RADIO_CAP_BLANKING));
			break;
		}
	}
	return 0;
}

int ath12k_mac_setup_vdev_create_arg_scan_radio_extn(struct ath12k_link_vif *arvif,
						     struct ath12k_wmi_vdev_create_arg *arg)
{
	struct ath12k *ar = arvif->ar;
	struct ath12k_vif *ahvif = arvif->ahvif;

	/* If the interface is marked as a special vdev, set the corresponding
	 * VDEV flag that informs firmware to treat this interface as a scan
	 * radio. Firmware will not expect a VDEV_UP for beaconing
	 */
	if (ahvif->vdev_type != WMI_VDEV_TYPE_AP || arvif != &ahvif->deflink)
		return 0;

	/* Has user requested to configure scan radio? */
	if (ahvif->vap_submode == QCA_WLAN_VENDOR_ATTR_VAP_SUBMODE_SCAN) {
		/* Reject request if firmware does not support scan radio */
		if (!ath12k_scan_radio_supported(ar->pdev)) {
			ath12k_err(ar->ab,
				   "Scan Radio not supported on PDEV %d (caps=0x%x)\n",
				   ar->pdev->pdev_id,
				   ar->pdev->cap.scan_radio_caps);
			return -EOPNOTSUPP;
		}

		arg->mbssid_flags |= VDEV_FLAGS_SCAN_MODE_VAP;
		ath12k_info(ar->ab, "vdev %d: Scan Radio ENABLED on pdev %d (caps=0x%x)\n",
			    arvif->vdev_id, ar->pdev->pdev_id,
			    ar->pdev->cap.scan_radio_caps);

	} else if (ath12k_scan_radio_supported(ar->pdev)) {
		/* Firmware supports scan radio but user didn't configure it as such.
		 * Current policy: pdev with scan radio capability should be dedicated
		 * to scan radio operation. This may change in the future if scan radio
		 * is extended to support other interface types (STA, monitor, etc.).
		 */
		ath12k_err(ar->ab,
			   "pdev %d supports scan radio (caps=0x%x) but vdev %d not configured as scan radio\n",
			   ar->pdev->pdev_id, ar->pdev->cap.scan_radio_caps,
			   arvif->vdev_id);
		return -EINVAL;
	} else {
		/* Normal AP operation: pdev does not have scan radio capability,
		 * proceed with standard VAP configuration.
		 */
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "pdev %d: Normal VAP Operation (caps=0x%x)\n",
			   ar->pdev->pdev_id,
			   ar->pdev->cap.scan_radio_caps);
	}

	return 0;
}

void ath12k_extn_reconfig_extn_params(struct ath12k *ar)
{
	ath12k_reconfig_esp_params_extn(ar);

	ath12k_smart_ant_api_recovery_handler(ar);

	ath12k_cbs_recovery_handler(ar);
}

static bool ath12k_core_nol_extn_auto_enabled(struct ath12k_base *ab)
{
	if (!ab || !ab->cfg_ctx)
		return false;

	return ath12k_cfg_get(ab, ATH12K_CFG_NOL_AUTO_ENABLE);
}

static bool ath12k_core_extn_is_5g_radio(struct ath12k *ar, u8 radio_idx)
{
	if (!ar || !ar->pdev)
		return false;

	if (!(ar->pdev->cap.supported_bands & BIT(NL80211_BAND_5GHZ))) {
		ath12k_dbg(ar->ab, ATH12K_DBG_BOOT,
			   "NOL auto-skip radio_idx=%u (non-5G radio, bands=0x%x)\n",
			   radio_idx, ar->pdev->cap.supported_bands);
		return false;
	}

	return true;
}

void ath12k_core_nol_extn_auto_dump_all_radios(struct ath12k_base *ab)
{
	int i;

	if (!ab || !ath12k_core_nol_extn_auto_enabled(ab))
		return;

	for (i = 0; i < ab->num_radios; i++) {
		struct ath12k *ar = ab->pdevs[i].ar;
		struct wiphy *wiphy;

		if (!ar || !ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
			continue;

		wiphy = ar->ah->hw->wiphy;
		wiphy_lock(wiphy);
		ath12k_core_nol_extn_auto_dump_radio_locked(ar, ar->radio_idx,
							     "stop");
		wiphy_unlock(wiphy);
	}
}

void ath12k_core_nol_extn_auto_dump_radio_locked(struct ath12k *ar, u8 radio_idx,
						  const char *reason)
{
	struct wiphy *wiphy;
	u32 dump_len = 0;
	int ret;

	if (!ar || !ar->ab || !ath12k_core_nol_extn_auto_enabled(ar->ab))
		return;

	if (!ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
		return;

	if (!ath12k_core_extn_is_5g_radio(ar, radio_idx))
		return;

	wiphy = ar->ah->hw->wiphy;
	lockdep_assert_wiphy(wiphy);

	ret = ath12k_vendor_nol_dump_to_file_extn(wiphy, radio_idx, NULL,
						  &dump_len);
	if (ret) {
		ath12k_warn(ar->ab,
			    "NOL auto-dump failed radio_idx=%u ret=%d (%s)\n",
			    radio_idx, ret, reason ? reason : "n/a");
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_BOOT,
		   "NOL auto-dump radio_idx=%u len=%u (%s)\n",
		   radio_idx, dump_len, reason ? reason : "n/a");
}

void ath12k_core_nol_extn_auto_restore_radio_locked(struct ath12k *ar, u8 radio_idx)
{
	struct wiphy *wiphy;
	u32 updated = 0;
	int ret;

	if (!ar || !ar->ab || !ath12k_core_nol_extn_auto_enabled(ar->ab))
		return;

	if (!ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
		return;

	if (!ath12k_core_extn_is_5g_radio(ar, radio_idx))
		return;

	wiphy = ar->ah->hw->wiphy;
	lockdep_assert_wiphy(wiphy);

	ret = ath12k_vendor_nol_restore_from_file_extn(wiphy, radio_idx, NULL,
						       &updated);
	if (!ret) {
		ath12k_dbg(ar->ab, ATH12K_DBG_BOOT,
			   "NOL auto-restore radio_idx=%u updated=%u\n",
			   radio_idx, updated);
		return;
	}

	if (ret == -ENOENT)
		ath12k_dbg(ar->ab, ATH12K_DBG_BOOT,
			   "NOL auto-restore skipped (no dump) radio_idx=%u\n",
			   radio_idx);
	else
		ath12k_warn(ar->ab,
			    "NOL auto-restore failed radio_idx=%u ret=%d\n",
			    radio_idx, ret);
}

void ath12k_core_nol_extn_auto_restore_all_radios(struct ath12k_base *ab)
{
	int i;

	if (!ab || !ath12k_core_nol_extn_auto_enabled(ab))
		return;

	for (i = 0; i < ab->num_radios; i++) {
		struct ath12k *ar = ab->pdevs[i].ar;
		struct wiphy *wiphy;

		if (!ar || !ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
			continue;

		wiphy = ar->ah->hw->wiphy;
		wiphy_lock(wiphy);
		ath12k_core_nol_extn_auto_restore_radio_locked(ar, ar->radio_idx);
		wiphy_unlock(wiphy);
	}
}

/**
 * ath12k_core_nol_extn_regdom_change_radio_locked() - Handle NOL on
 * user-triggered country/regdomain change
 * @ar: ath12k radio instance
 *
 * Called from ath12k_regd_update_work() on every regd update. On a
 * user-triggered country/regdomain change (regdom_set_by_user == true),
 * clears the in-memory NOL then attempts a CC-matched restore from the saved
 * dump file. On a CC mismatch (new country differs from the dump) nothing is
 * restored. On a CC match (e.g. change back to original country) the saved
 * NOL is re-applied with the remaining NOP time back-dated correctly.
 */
void ath12k_core_nol_extn_regdom_change_radio_locked(struct ath12k *ar)
{
	if (!ar || !ar->ah || !ar->ah->hw || !ar->ah->hw->wiphy)
		return;

	lockdep_assert_wiphy(ar->ah->hw->wiphy);

	ath12k_core_nol_extn_auto_restore_radio_locked(ar, ar->radio_idx);
}
EXPORT_SYMBOL(ath12k_core_nol_extn_regdom_change_radio_locked);

/**
 * ath12k_mac_dfs_violation_radar_bitmap_extn() - Build radar bitmap
 * @wiphy: wiphy owning the channel state
 * @chandef: operating channel definition
 *
 * Walk the operating channel in 20 MHz steps and mark restored NOL
 * subchannels in the returned radar bitmap.
 *
 * Return: radar bitmap for restored NOL subchannels, or 0.
 */
static u16
ath12k_mac_dfs_violation_radar_bitmap_extn(struct wiphy *wiphy,
					   const struct cfg80211_chan_def *chandef)
{
	u32 start_freq_khz, end_freq_khz, freq_khz;
	u16 radar_bitmap = 0;
	int width_mhz;
	int bit = 0;

	if (!wiphy || !cfg80211_chandef_valid(chandef) || !chandef->chan)
		return 0;

	if (cfg80211_chandef_device_present(chandef) || chandef->center_freq2)
		return 0;

	width_mhz = cfg80211_chandef_get_width(chandef);
	if (width_mhz < 0)
		return 0;

	start_freq_khz = MHZ_TO_KHZ(width_mhz <= ATH12K_CHWIDTH_20 ?
				    chandef->center_freq1 :
				    chandef->center_freq1 - width_mhz / 2 + 10);
	end_freq_khz = MHZ_TO_KHZ(width_mhz <= ATH12K_CHWIDTH_20 ?
				  chandef->center_freq1 :
				  chandef->center_freq1 + width_mhz / 2 - 10);

	for (freq_khz = start_freq_khz;
	     freq_khz <= end_freq_khz;
	     freq_khz += MHZ_TO_KHZ(ATH12K_CHWIDTH_20), bit++) {
		struct ieee80211_channel *chan;

		chan = ieee80211_get_channel_khz(wiphy, freq_khz);
		if (!chan || !(chan->flags & IEEE80211_CHAN_RADAR))
			continue;

		if (chan->dfs_state == NL80211_DFS_UNAVAILABLE)
			radar_bitmap |= BIT(bit);
	}

	return radar_bitmap;
}

void ath12k_mac_dfs_violation_recovery_extn(struct ath12k_link_vif *arvif,
					    struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	struct cfg80211_chan_def *chandef;
	int dfs_required;

	if (!ctx)
		return;

	chandef = &ctx->def;
	if (!chandef->chan)
		return;

	dfs_required = cfg80211_chandef_dfs_required(hw->wiphy, chandef,
						     arvif->ahvif->vif->type);
	if (dfs_required <= 0)
		return;

	arvif->pending_tx_vdev_up = false;

	ath12k_warn(ar->ab,
		    "dfs violation on vdev %d for freq %u, marking target unavailable\n",
		    arvif->vdev_id, chandef->chan->center_freq);

	cfg80211_radar_event(hw->wiphy, chandef, GFP_KERNEL);
}

/**
 * ath12k_mac_setnol_recovery_extn() - Trigger setnol DFS recovery
 * @arvif: AP link vif
 * @ctx: channel context
 *
 * Route setnol-triggered recovery through mac80211 radar handling so
 * CAC abort and channel move follow the normal DFS path.
 */
void ath12k_mac_setnol_recovery_extn(struct ath12k_link_vif *arvif,
				     struct ieee80211_chanctx_conf *ctx)
{
	struct ath12k *ar = arvif->ar;
	struct ieee80211_hw *hw = ath12k_ar_to_hw(ar);
	struct cfg80211_chan_def *chandef;
	u16 radar_bitmap;
	int dfs_required;

	if (!ctx)
		return;

	chandef = &ctx->def;
	if (!chandef->chan)
		return;

	dfs_required = cfg80211_chandef_dfs_required(hw->wiphy, chandef,
						     arvif->ahvif->vif->type);
	if (dfs_required <= 0)
		return;

	arvif->pending_tx_vdev_up = false;
	radar_bitmap = ath12k_mac_dfs_violation_radar_bitmap_extn(hw->wiphy,
								  chandef);

	if (radar_bitmap) {
		ieee80211_radar_detected_bitmap(hw, radar_bitmap,
						chandef->chan);
		return;
	}

	ieee80211_radar_detected(hw, ctx);
}

#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
void ath12k_peer_assoc_h_mesh_extn(struct ath12k_link_vif *arvif,
				   struct ath12k_link_sta *arsta,
				   struct ieee80211_link_sta *link_sta,
				   struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ath12k_vif *ahvif;
	struct ath12k *ar;

	if (!arvif || !arsta || !link_sta || !arg)
		return;

	ahvif = arvif->ahvif;
	ar = arvif->ar;

	/* For statically added mesh peers, keys will be programmed by user.
	 * Reset the need_ptk_4_way/need_gtk_2_way flag when sending peer assoc.
	 *
	 * Save calculated phymode and nss for peer lookup.
	 */
	if (ahvif && QCA_WLAN_VENDOR_VAP_IS_MESH_MODE(ahvif->vap_submode)) {
		arg->need_ptk_4_way = false;
		arg->need_gtk_2_way = false;
		if (link_sta->sta) {
			link_sta->sta->sta_extn.peer_info.mesh_peer_phymode = arg->peer_phymode;
			link_sta->sta->sta_extn.peer_info.mesh_peer_nss = arg->peer_nss;
		}
	}
	return;
}
#endif
#ifndef CONFIG_IO_COHERENCY
void ath12k_dp_srng_dst_invalidate_entries(struct ath12k_dp *dp,
					   struct hal_srng *srng,
					   int num_entries)
{
	u32 *first_desc, *last_desc;
	u32 last_desc_index;

	if (!(srng->flags & HAL_SRNG_FLAGS_CACHED) || !num_entries)
		return;

	first_desc = &srng->ring_base_vaddr[srng->u.dst_ring.tp];
	last_desc_index = (srng->u.dst_ring.tp +
			   (num_entries * srng->entry_size)) % srng->ring_size;
	last_desc =  &srng->ring_base_vaddr[last_desc_index];

	if (last_desc > (uint32_t *)first_desc)
		/* invalidate from tp to last_desc */
		ath12k_core_dmac_inv_range_no_dsb((void *)first_desc,
						  (void *)last_desc);
	else {
		/* This is wrap-around case.
		 * invalidate from tp to end of the ring
		 */
		ath12k_core_dmac_inv_range_no_dsb((void *)first_desc,
						  (void *)srng->ring_vaddr_end);

		/* invalidate from start of the ring to remaining entries */
		ath12k_core_dmac_inv_range_no_dsb((void *)srng->ring_base_vaddr,
						  (void *)last_desc);
	}
}

void ath12k_dsb(void)
{
	dsb(st);
}

#ifdef PLATFORM_SDX
dma_addr_t ath12k_dp_rx_buffer_map(struct ath12k_dp *dp,
				   struct ath12k_rx_desc_info *rx_sw_desc)
{
	dma_addr_t iova;

	iova = dma_map_single(dp->dev, rx_sw_desc->vaddr,
			      DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE);

	if (unlikely(dma_mapping_error(dp->dev, iova)))
		return DMA_MAPPING_ERROR;

	return iova;
}

void ath12k_dp_rx_buffer_unmap(struct ath12k_dp *dp,
			       struct ath12k_rx_desc_info *rx_sw_desc)
{
	dma_unmap_single(dp->dev, rx_sw_desc->paddr,
			 DP_RX_BUFFER_SIZE, DMA_FROM_DEVICE);
}
#else
dma_addr_t ath12k_dp_rx_buffer_map(struct ath12k_dp *dp,
				   struct ath12k_rx_desc_info *rx_sw_desc)
{
	if (unlikely(!rx_sw_desc->skb->fast_recycled))
		ath12k_core_dmac_inv_range_no_dsb(rx_sw_desc->vaddr,
						  rx_sw_desc->vaddr +
						  DP_RX_BUFFER_SIZE);

	return virt_to_phys(rx_sw_desc->vaddr);
}

void ath12k_dp_rx_buffer_unmap(struct ath12k_dp *dp,
			       struct ath12k_rx_desc_info *rx_sw_desc)
{
	ath12k_core_dmac_inv_range_no_dsb(rx_sw_desc->vaddr,
					  rx_sw_desc->vaddr +
					  DP_RX_BUFFER_SIZE);
}
#endif /* PLATFORM_SDX */
#else
void ath12k_dp_srng_dst_invalidate_entries(struct ath12k_dp *dp,
					   struct hal_srng *srng,
					   int num_entries)
{
}

void ath12k_dsb(void)
{
}

dma_addr_t ath12k_dp_rx_buffer_map(struct ath12k_dp *dp,
				   struct ath12k_rx_desc_info *rx_sw_desc)
{
	return (dma_addr_t)virt_to_phys(rx_sw_desc->vaddr);
}

void ath12k_dp_rx_buffer_unmap(struct ath12k_dp *dp,
			       struct ath12k_rx_desc_info *rx_sw_desc)
{
}
#endif

EXPORT_SYMBOL(ath12k_dp_srng_dst_invalidate_entries);
EXPORT_SYMBOL(ath12k_dsb);
EXPORT_SYMBOL(ath12k_dp_rx_buffer_map);
EXPORT_SYMBOL(ath12k_dp_rx_buffer_unmap);

#ifdef CPTCFG_MAC80211_SFE_SUPPORT
struct sk_buff *ath12k_dp_alloc_skb(int size)
{
	return netdev_alloc_skb_fast(NULL, size);
}
#else
struct sk_buff *ath12k_dp_alloc_skb(int size)
{
	return dev_alloc_skb(DP_RX_BUFFER_SIZE);
}
#endif
EXPORT_SYMBOL(ath12k_dp_alloc_skb);

#define ATH12K_RCSA_QCOM_OUI_0		0x00
#define ATH12K_RCSA_QCOM_OUI_1		0x03
#define ATH12K_RCSA_QCOM_OUI_2		0x7f
#define ATH12K_RCSA_QCOM_OUI_LEN	3
#define ATH12K_RCSA_CSA_IE_MIN_LEN	3

int ath12k_vs_action_has_ml_link_info_ie_extn(struct ath12k *ar,
					      u8 catagory,
					      const u8 *pos,
					      const u8 *end)
{
	if (catagory != WLAN_CATEGORY_VENDOR_SPECIFIC)
		return 0;

	if (pos > end)
		return 0;

	if (pos + ATH12K_RCSA_QCOM_OUI_LEN > end)
		return 0;

	ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
			 "rcsa_ml_info: vendor action oui=%02x:%02x:%02x\n",
			 pos[0], pos[1], pos[2]);

	/*
	 * RCSA is sent by supplicant as a Qualcomm vendor action frame:
	 *   Category: WLAN_CATEGORY_VENDOR_SPECIFIC
	 *   OUI     : 00:03:7f
	 *   Payload : CSA IE followed by optional IEs, including ML Link Info IE.
	 */
	if (pos[0] != ATH12K_RCSA_QCOM_OUI_0 ||
	    pos[1] != ATH12K_RCSA_QCOM_OUI_1 ||
	    pos[2] != ATH12K_RCSA_QCOM_OUI_2)
		return 0;

	pos += ATH12K_RCSA_QCOM_OUI_LEN;

	if (pos + 2 > end || pos[0] != WLAN_EID_CHANNEL_SWITCH ||
	    pos[1] < ATH12K_RCSA_CSA_IE_MIN_LEN ||
	    pos + 2 + pos[1] > end)
		return 0;

	pos += 2 + pos[1];

	return cfg80211_find_ext_elem(WLAN_EID_EXT_MLO_LINK_INFO,
				      pos, end - pos) ? 1 : 0;
}

#define ATH12K_RRM_MEASURE_TYPE_BEACON 5

bool ath12k_is_beacon_report_elem(u8 action_code, const u8 *buf, const u8 *end)
{
	if (action_code != WLAN_ACTION_RADIO_MSR_RM_REP)
		return false;

	if (buf + 5 > end)
		return false;

	if (buf[0] != WLAN_EID_MEASURE_REPORT)
		return false;

	if (buf[1] < 3)
		return false;

	return buf[4] == ATH12K_RRM_MEASURE_TYPE_BEACON;
}

/**
 * ath12k_control_beaconing_bootup_cac_check_extn - Check boot-up CAC state
 * and defer vdev_up if DFS CAC is still in progress.
 *
 * @ar:        ath12k radio instance
 * @arvif:     link vif for which vdev_up is being considered
 * @link_conf: ieee80211_bss_conf for the link
 *
 * Returns true if vdev_up should be deferred (CAC still running), false
 * otherwise.  When returning true the function also clears arvif->is_up and
 * emits a debug message.  When returning false and the CAC_RUNNING flag is
 * still set it is cleared so that get_txpower no longer returns -EAGAIN.
 */
bool ath12k_control_beaconing_bootup_cac_check_extn(struct ath12k *ar,
						     struct ath12k_link_vif *arvif,
						     struct ieee80211_bss_conf *link_conf)
{
	struct ieee80211_chanctx_conf *chanctx_conf;

	chanctx_conf = wiphy_dereference(ath12k_ar_to_hw(ar)->wiphy,
					 link_conf->chanctx_conf);

	if (chanctx_conf &&
	    cfg80211_chandef_dfs_usable(ath12k_ar_to_hw(ar)->wiphy,
					&chanctx_conf->def) &&
	    !cfg80211_chandef_dfs_available(ath12k_ar_to_hw(ar)->wiphy,
					    &chanctx_conf->def)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "vdev %d: boot-up CAC in progress, deferring vdev_up\n",
			   arvif->vdev_id);
		arvif->is_up = false;
		return true;
	}

	/*
	 * Boot-up CAC has completed; clear the CAC_RUNNING flag so that
	 * get_txpower no longer returns -EAGAIN and iw dev shows the
	 * correct txpower for this vdev.
	 */
	if (test_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags)) {
		ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
			   "vdev %d: clearing CAC_RUNNING flag post boot-up CAC\n",
			   arvif->vdev_id);
		clear_bit(ATH12K_FLAG_CAC_RUNNING, &ar->dev_flags);
	}

	return false;
}

/**
 * ath12k_get_monitor_version - Return the monitor ring version for a device
 * @ab: ath12k base device instance
 *
 * Maps the hardware revision to the monitor ring version used by the
 * firmware, mirroring the Prop driver's ol_ath_get_monitor_version():
 *
 *   Version 1: QCA Proprietary Lithium Monitor
 *   Version 2: QCA Proprietary Beryllium Monitor
 *   Version 3: UD Lithium - Quad Ring Monitor
 *   Version 4: UD Beryllium - Dual Ring Monitor
 *   Version 5: UD Boron - Dual Ring Monitor
 *
 * Return: Monitor version number, or 0 if the hw_rev is unknown.
 */
u32 ath12k_get_monitor_version(struct ath12k_base *ab)
{
	if (!ab)
		return 0;

	switch (ab->hw_rev) {
	case ATH12K_HW_WCN7850_HW20:
	case ATH12K_HW_QCN9074_HW10:
	case ATH12K_HW_QCN9160_HW10:
		return 3;
	case ATH12K_HW_QCN9274_HW10:
	case ATH12K_HW_QCN9274_HW20:
	case ATH12K_HW_IPQ5332_HW10:
	case ATH12K_HW_IPQ5424_HW10:
	case ATH12K_HW_QCN6432_HW10:
		return 4;
	case ATH12K_HW_QCN9625_HW10:
	case ATH12K_HW_QCN9625_HW20:
	case ATH12K_HW_QCN9589_HW10:
		return 5;
	default:
		ath12k_warn(ab, "unknown hw_rev %d for monitor version\n",
			    ab->hw_rev);
		return 0;
	}
}

/**
 * ath12k_peer_assoc_h_vht_mcs10_11_extn - Encode VHT MCS 10/11 (1024-QAM)
 * into the peer_assoc tx_mcs_set.
 *
 * VHT MCS 10/11 is an extension to 802.11ac negotiated via QCN vendor IE
 * attribute 0x02.  When the VAP has MCS10/11 enabled and the hardware
 * supports it, the per-SS bitmap is encoded into bits [23:16] of tx_mcs_set
 * with the validity flag in bit 24.  If neither the peer QCN IE nor the
 * nq2q fallback is active the extended bits are stripped so firmware sees a
 * standard MCS 0-9 peer.
 */
void ath12k_peer_assoc_h_vht_mcs10_11_extn(struct ath12k *ar,
					   struct ath12k_link_vif *arvif,
					   struct ath12k_link_sta *arsta,
					   struct ath12k_wmi_peer_assoc_arg *arg)
{
	u8 peer_higher_mcs;

	if (!arvif || !ar || !ar->pdev)
		return;

	if (!arvif->arvif_extn.vht_mcs10_11_supp || !ar->pdev->cap.vht_higher_mcs_supp)
		return;

	if (arsta->arsta_extn.higher_vhtmcs_supp) {
		/* Peer advertised MCS10/11 support via QCN IE:
		 * use the negotiated per-SS intersection bitmap.
		 */
		peer_higher_mcs = arsta->arsta_extn.higher_vhtmcs_supp;
	} else if (arvif->arvif_extn.vht_mcs10_11_nq2q_peer_supp) {
		/* Non-QCA peer fallback: apply self capability to all
		 * supported NSS chains.
		 */
		peer_higher_mcs = ar->pdev->cap.vht_higher_mcs_supp;
	} else {
		peer_higher_mcs = 0;
	}

	if (peer_higher_mcs) {
		/* Mask to peer's actual NSS capability: a peer with
		 * N spatial streams can only use MCS10/11 on NSS 1..N.
		 * arg->peer_nss is already clamped to the negotiated NSS.
		 */
		if (arg->peer_nss < 8)
			peer_higher_mcs &= (u8)((1 << arg->peer_nss) - 1);
		peer_higher_mcs &= ar->pdev->cap.vht_higher_mcs_supp;
		/* Set validity flag (bit 24) and per-SS bitmap [23:16] */
		arg->tx_mcs_set |= IEEE80211_DISABLE_VHT_MCS_SUPPORT_0_11;
		arg->tx_mcs_set |= ((u32)peer_higher_mcs << 16) &
				   IEEE80211_VHT_MCS_SUPPORT_0_11_MASK;
		ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "VHT MCS10/11: peer %pM tx_mcs_set 0x%08x higher_vhtmcs_supp 0x%02x\n",
				 arsta->addr, arg->tx_mcs_set,
				 arsta->arsta_extn.higher_vhtmcs_supp);
	} else {
		arg->tx_mcs_set |= IEEE80211_DISABLE_VHT_MCS_SUPPORT_0_11;
	}
}

/**
 * ath12k_peer_assoc_h_he_cap_internal_extn - Encode HE 400ns SGI and 2x LTF
 * 160/80+80 MHz internal capability into the peer_assoc arg.
 *
 * The QCA-internal HE capability bitmap is negotiated via QCN vendor IE
 * attributes 0x03 (400ns SGI, bits [1:0]) and 0x04 (2x LTF 160/80+80, bit 2).
 * Intersect with the local HW band capability so we never advertise a feature
 * the local radio does not support.
 */
void ath12k_peer_assoc_h_he_cap_internal_extn(struct ath12k *ar,
					      struct ath12k_link_sta *arsta,
					      struct ath12k_wmi_peer_assoc_arg *arg,
					      enum nl80211_band band)
{
	if (!arsta || !ar || !ar->pdev)
		return;

	arg->peer_he_cap_macinfo_internal = arsta->arsta_extn.he_cap_info_internal &
					ar->pdev->cap.band[band].he_cap_info_internal;
}

/**
 * ath12k_mac_vht_mcs10_11_ratemask_extn - Extend the per-NSS VHT ratemask
 * to include MCS10 and MCS11 (1024-QAM) bits.
 *
 * When VHT MCS10/11 is enabled for this VAP and the radio supports it,
 * extend each NSS chain's 12-bit ratemask entry to set bits 10 and 11,
 * enabling MCS10 and MCS11 in the WMI_VDEV_RATEMASK_CMDID payload.
 *
 * @arvif: link vif whose ratemask is being built
 * @arg:   ratemask WMI arg already populated with the standard MCS 0-9 bits;
 *         MCS10/11 bits are OR'd in for supported NSS chains
 */
void ath12k_mac_vht_mcs10_11_ratemask_extn(struct ath12k_link_vif *arvif,
					   struct wmi_vdev_ratemask_arg *arg)
{
	u8 higher_mcs = 0;
	u64 mcs1011_lower64 = 0, mcs1011_higher64 = 0;
	u8 nss;

	if (!arvif || !arvif->ar || !arvif->ar->pdev)
		return;

	if (!arvif->arvif_extn.vht_mcs10_11_supp ||
	    !arvif->ar->pdev->cap.vht_higher_mcs_supp)
		return;

	higher_mcs = arvif->ar->pdev->cap.vht_higher_mcs_supp;
	for (nss = 0; nss < NL80211_VHT_NSS_MAX; nss++) {
		if (!(higher_mcs & BIT(nss)))
			continue;
		/* Add MCS10 (bit 10) and MCS11 (bit 11) for this NSS */
		if (nss < 5) {
			mcs1011_lower64 |= (u64)(BIT(10) | BIT(11)) << (nss * 12);
		} else if (nss == 5) {
			mcs1011_lower64 |=
				((u64)((BIT(10) | BIT(11)) & 0xf)) << 60;
			mcs1011_higher64 |= (u64)((BIT(10) | BIT(11)) >> 4);
		} else {
			mcs1011_higher64 |= (u64)(BIT(10) | BIT(11)) <<
						(((nss - 6) * 12) + 8);
		}
	}
	arg->mask_lower32  |= lower_32_bits(mcs1011_lower64);
	arg->mask_higher32 |= upper_32_bits(mcs1011_lower64);
	arg->mask_lower32_2 |= lower_32_bits(mcs1011_higher64);
}

void *ath12k_hal_dma_alloc_noncoherent(struct device *dev, size_t size,
				       dma_addr_t *paddr,
				       enum dma_data_direction dir,
				       gfp_t flag)
{
	void *vaddr = NULL;

#if !defined(CONFIG_IO_COHERENCY) || defined(PLATFORM_SDX)
	vaddr = dma_alloc_noncoherent(dev, size, paddr, dir, flag);
#else
	vaddr = kzalloc(size, flag);
	*paddr = (dma_addr_t)virt_to_phys(vaddr);
#endif

	return vaddr;
}
EXPORT_SYMBOL(ath12k_hal_dma_alloc_noncoherent);

void *ath12k_hal_dma_alloc_coherent(struct device *dev, size_t size,
				    dma_addr_t *paddr, gfp_t flag)
{
	void *vaddr = NULL;

#if !defined(CONFIG_IO_COHERENCY) || defined(PLATFORM_SDX)
	vaddr = dma_alloc_coherent(dev, size, paddr, flag);
#else
	vaddr = kzalloc(size, flag);
	*paddr = (dma_addr_t)virt_to_phys(vaddr);
#endif

	return vaddr;
}
EXPORT_SYMBOL(ath12k_hal_dma_alloc_coherent);

void ath12k_hal_dma_free_noncoherent(struct device *dev, size_t size,
				     void *vaddr, dma_addr_t paddr,
				     enum dma_data_direction dir)
{
#if !defined(CONFIG_IO_COHERENCY) || defined(PLATFORM_SDX)
	dma_free_noncoherent(dev, size, vaddr, paddr, dir);
#else
	kfree(vaddr);
#endif
}
EXPORT_SYMBOL(ath12k_hal_dma_free_noncoherent);

void ath12k_hal_dma_free_coherent(struct device *dev, size_t size,
				  void *vaddr, dma_addr_t paddr)
{
#if !defined(CONFIG_IO_COHERENCY) || defined(PLATFORM_SDX)
	dma_free_coherent(dev, size, vaddr, paddr);
#else
	kfree(vaddr);
#endif
}
EXPORT_SYMBOL(ath12k_hal_dma_free_coherent);
