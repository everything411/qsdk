// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>

#include "debugfs_sta.h"
#include "core.h"
#include "peer.h"
#include "debug.h"
#include "debugfs_htt_stats.h"
#include "debugfs.h"
#include "dp_cmn.h"

enum htt_ppdu_stats_gi
ath12k_debugfs_sta_get_gi_idx(const struct rate_info *txrate)
{
	if (txrate->flags & RATE_INFO_FLAGS_EHT_MCS) {
		switch (txrate->eht_gi) {
		case NL80211_RATE_INFO_EHT_GI_0_8:
			return HTT_PPDU_STATS_SGI_0_8_US;
		case NL80211_RATE_INFO_EHT_GI_1_6:
			return HTT_PPDU_STATS_SGI_1_6_US;
		case NL80211_RATE_INFO_EHT_GI_3_2:
			return HTT_PPDU_STATS_SGI_3_2_US;
		default:
			return HTT_PPDU_STATS_SGI_0_8_US;
		}
	}

	if (txrate->flags & RATE_INFO_FLAGS_HE_MCS) {
		switch (txrate->he_gi) {
		case NL80211_RATE_INFO_HE_GI_0_8:
			return HTT_PPDU_STATS_SGI_0_8_US;
		case NL80211_RATE_INFO_HE_GI_1_6:
			return HTT_PPDU_STATS_SGI_1_6_US;
		case NL80211_RATE_INFO_HE_GI_3_2:
			return HTT_PPDU_STATS_SGI_3_2_US;
		default:
			return HTT_PPDU_STATS_SGI_0_8_US;
		}
	}

	if (txrate->flags & (RATE_INFO_FLAGS_VHT_MCS | RATE_INFO_FLAGS_MCS))
		return (txrate->flags & RATE_INFO_FLAGS_SHORT_GI) ?
			HTT_PPDU_STATS_SGI_0_4_US : HTT_PPDU_STATS_SGI_0_8_US;

	return HTT_PPDU_STATS_SGI_0_8_US;
}

int ath12k_he_ru_alloc_to_ru_loc_idx(u16 nl_ru)
{
	switch (nl_ru) {
	case NL80211_RATE_INFO_HE_RU_ALLOC_26:
		return ATH12K_EHT_RU_26;
	case NL80211_RATE_INFO_HE_RU_ALLOC_52:
		return ATH12K_EHT_RU_52;
	case NL80211_RATE_INFO_HE_RU_ALLOC_106:
		return ATH12K_EHT_RU_106;
	case NL80211_RATE_INFO_HE_RU_ALLOC_242:
		return ATH12K_EHT_RU_242;
	case NL80211_RATE_INFO_HE_RU_ALLOC_484:
		return ATH12K_EHT_RU_484;
	case NL80211_RATE_INFO_HE_RU_ALLOC_996:
		return ATH12K_EHT_RU_996;
	default:
		return -1;
	}
}

int ath12k_eht_ru_alloc_to_ru_loc_idx(u16 nl_ru)
{
	switch (nl_ru) {
	case NL80211_RATE_INFO_EHT_RU_ALLOC_26:
		return ATH12K_EHT_RU_26;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_52:
		return ATH12K_EHT_RU_52;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_52P26:
		return ATH12K_EHT_RU_52_26;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_106:
		return ATH12K_EHT_RU_106;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_106P26:
		return ATH12K_EHT_RU_106_26;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_242:
		return ATH12K_EHT_RU_242;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_484:
		return ATH12K_EHT_RU_484;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_484P242:
		return ATH12K_EHT_RU_484_242;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_996:
		return ATH12K_EHT_RU_996;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_996P484:
		return ATH12K_EHT_RU_996_484;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_996P484P242:
		return ATH12K_EHT_RU_996_484_242;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_2x996:
		return ATH12K_EHT_RU_996x2;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_2x996P484:
		return ATH12K_EHT_RU_996x2_484;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_3x996:
		return ATH12K_EHT_RU_996x3;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_3x996P484:
		return ATH12K_EHT_RU_996x3_484;
	case NL80211_RATE_INFO_EHT_RU_ALLOC_4x996:
		return ATH12K_EHT_RU_996x4;
	default:
		return -1;
	}
}

void
ath12k_debugfs_sta_update_success(struct ath12k_dp_link_peer *peer,
				  struct ath12k_per_peer_tx_stats *peer_stats)
{
	struct rate_info *txrate = &peer->txrate;
	struct ath12k_htt_tx_stats *tx_stats = peer->peer_stats.tx_stats;
	int gi, mcs, bw, nss, ppdu_type;
	u16 ru_type;
	u32 succ_bytes, succ_pkts, retry_bytes;
	u64 ampdu_pkts, ampdu_bytes;

	if (!tx_stats)
		return;

	gi = ath12k_debugfs_sta_get_gi_idx(txrate);
	mcs = txrate->mcs;
	bw = ath12k_mac_mac80211_bw_to_ath12k_bw(txrate->bw);
	nss = txrate->nss - 1;
	succ_bytes = peer_stats->succ_bytes;
	retry_bytes = peer_stats->retry_bytes;
	succ_pkts = peer_stats->succ_pkts;
	ru_type = peer_stats->ru_tones;
	ppdu_type = tx_stats->ppdu_type;

	ampdu_pkts = peer_stats->mpdu_tried; /* mpdu success + mpdu retry */
	ampdu_bytes = succ_bytes + retry_bytes;

	if (txrate->flags & RATE_INFO_FLAGS_EHT_MCS) {
		STATS_OP_FMT(SUCC).eht[0][mcs] += succ_bytes;
		STATS_OP_FMT(SUCC).eht[1][mcs] += succ_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_UHR_MCS) {
		STATS_OP_FMT(SUCC).uhr[0][mcs] += succ_bytes;
		STATS_OP_FMT(SUCC).uhr[1][mcs] += succ_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_HE_MCS) {
		STATS_OP_FMT(SUCC).he[0][mcs] += succ_bytes;
		STATS_OP_FMT(SUCC).he[1][mcs] += succ_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_VHT_MCS) {
		STATS_OP_FMT(SUCC).vht[0][mcs] += succ_bytes;
		STATS_OP_FMT(SUCC).vht[1][mcs] += succ_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_MCS) {
		STATS_OP_FMT(SUCC).ht[0][mcs] += succ_bytes;
		STATS_OP_FMT(SUCC).ht[1][mcs] += succ_pkts;
	} else {
		mcs = tx_stats->rate_idx;
		STATS_OP_FMT(SUCC).legacy[0][mcs] += succ_bytes;
		STATS_OP_FMT(SUCC).legacy[1][mcs] += succ_pkts;
	}

	STATS_OP_FMT(SUCC).bw[0][bw] += succ_bytes;
	STATS_OP_FMT(SUCC).nss[0][nss] += succ_bytes;
	STATS_OP_FMT(SUCC).gi[0][gi] += succ_bytes;

	STATS_OP_FMT(SUCC).bw[1][bw] += succ_pkts;
	STATS_OP_FMT(SUCC).nss[1][nss] += succ_pkts;
	STATS_OP_FMT(SUCC).gi[1][gi] += succ_pkts;

	/* RU location and transmit type success updates */
	if ((ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA ||
	    ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA) &&
	    (txrate->flags & RATE_INFO_FLAGS_HE_MCS ||
	    txrate->flags & RATE_INFO_FLAGS_EHT_MCS ||
	    txrate->flags & RATE_INFO_FLAGS_UHR_MCS)) {
		int ru_loc_idx;

		if (txrate->flags & RATE_INFO_FLAGS_HE_MCS)
			ru_loc_idx = ath12k_he_ru_alloc_to_ru_loc_idx(ru_type);
		else
			ru_loc_idx = ath12k_eht_ru_alloc_to_ru_loc_idx(ru_type);

		if (ru_loc_idx >= 0) {
			STATS_OP_FMT(SUCC).ru_loc[0][ru_loc_idx] += succ_bytes;
			STATS_OP_FMT(SUCC).ru_loc[1][ru_loc_idx] += succ_pkts;

			if (peer_stats->is_ampdu) {
				STATS_OP_FMT(AMPDU).ru_loc[0][ru_loc_idx] += ampdu_bytes;
				STATS_OP_FMT(AMPDU).ru_loc[1][ru_loc_idx] += ampdu_pkts;
			}
		}
	}

	if (ppdu_type < HTT_PPDU_STATS_PPDU_TYPE_MAX) {
		STATS_OP_FMT(SUCC).transmit_type[0][ppdu_type] += succ_bytes;
		STATS_OP_FMT(SUCC).transmit_type[1][ppdu_type] += succ_pkts;
		if (peer_stats->is_ampdu) {
			STATS_OP_FMT(AMPDU).transmit_type[0][ppdu_type] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).transmit_type[1][ppdu_type] += ampdu_pkts;
		}
	}

	/* AMPDU and ACK/BA failure accounting */
	if (peer_stats->is_ampdu) {
		tx_stats->ba_fails += peer_stats->ba_fails;
		if (txrate->flags & RATE_INFO_FLAGS_EHT_MCS) {
			STATS_OP_FMT(AMPDU).eht[0][mcs] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).eht[1][mcs] += ampdu_pkts;
		} else if (txrate->flags & RATE_INFO_FLAGS_UHR_MCS) {
			STATS_OP_FMT(AMPDU).uhr[0][mcs] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).uhr[1][mcs] += ampdu_pkts;
		} else if (txrate->flags & RATE_INFO_FLAGS_HE_MCS) {
			STATS_OP_FMT(AMPDU).he[0][mcs] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).he[1][mcs] += ampdu_pkts;
		} else if (txrate->flags & RATE_INFO_FLAGS_VHT_MCS) {
			STATS_OP_FMT(AMPDU).vht[0][mcs] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).vht[1][mcs] += ampdu_pkts;
		} else if (txrate->flags & RATE_INFO_FLAGS_MCS) {
			STATS_OP_FMT(AMPDU).ht[0][mcs] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).ht[1][mcs] += ampdu_pkts;
		} else {
			STATS_OP_FMT(AMPDU).vht[0][mcs] += ampdu_bytes;
			STATS_OP_FMT(AMPDU).vht[1][mcs] += ampdu_pkts;
		}
		STATS_OP_FMT(AMPDU).bw[0][bw] += ampdu_bytes;
		STATS_OP_FMT(AMPDU).nss[0][nss] += ampdu_bytes;
		STATS_OP_FMT(AMPDU).gi[0][gi] += ampdu_bytes;
		STATS_OP_FMT(AMPDU).bw[1][bw] += ampdu_pkts;
		STATS_OP_FMT(AMPDU).nss[1][nss] += ampdu_pkts;
		STATS_OP_FMT(AMPDU).gi[1][gi] += ampdu_pkts;
	} else {
		tx_stats->ack_fails += peer_stats->ba_fails;
	}
}

void
ath12k_debugfs_sta_update_retry(struct ath12k_dp_link_peer *peer,
				struct ath12k_per_peer_tx_stats *peer_stats)
{

	struct rate_info *txrate = &peer->txrate;
	struct ath12k_htt_tx_stats *tx_stats = peer->peer_stats.tx_stats;
	int gi, mcs, bw, nss, ru_type, ppdu_type;
	u32 retry_bytes, mpdu_retry_pkts;

	if (!tx_stats)
		return;

	gi = ath12k_debugfs_sta_get_gi_idx(txrate);
	mcs = txrate->mcs;
	bw = ath12k_mac_mac80211_bw_to_ath12k_bw(txrate->bw);
	nss = txrate->nss - 1;

	if (peer_stats->mpdu_tried < peer_stats->succ_mpdu_pkts)
		return;
	mpdu_retry_pkts = peer_stats->mpdu_tried - peer_stats->succ_mpdu_pkts;
	retry_bytes = peer_stats->retry_bytes;
	ru_type = peer_stats->ru_tones;
	ppdu_type = tx_stats->ppdu_type;

	if (txrate->flags & RATE_INFO_FLAGS_EHT_MCS) {
		STATS_OP_FMT(RETRY).eht[0][mcs] += retry_bytes;
		STATS_OP_FMT(RETRY).eht[1][mcs] += mpdu_retry_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_UHR_MCS) {
		STATS_OP_FMT(RETRY).uhr[0][mcs] += retry_bytes;
		STATS_OP_FMT(RETRY).uhr[1][mcs] += mpdu_retry_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_HE_MCS) {
		STATS_OP_FMT(RETRY).he[0][mcs] += retry_bytes;
		STATS_OP_FMT(RETRY).he[1][mcs] += mpdu_retry_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_VHT_MCS) {
		STATS_OP_FMT(RETRY).vht[0][mcs] += retry_bytes;
		STATS_OP_FMT(RETRY).vht[1][mcs] += mpdu_retry_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_MCS) {
		STATS_OP_FMT(RETRY).ht[0][mcs] += retry_bytes;
		STATS_OP_FMT(RETRY).ht[1][mcs] += mpdu_retry_pkts;
	} else {
		mcs = tx_stats->rate_idx;
		STATS_OP_FMT(RETRY).legacy[0][mcs] += retry_bytes;
		STATS_OP_FMT(RETRY).legacy[1][mcs] += mpdu_retry_pkts;
	}

	STATS_OP_FMT(RETRY).bw[0][bw] += retry_bytes;
	STATS_OP_FMT(RETRY).nss[0][nss] += retry_bytes;
	STATS_OP_FMT(RETRY).gi[0][gi] += retry_bytes;

	STATS_OP_FMT(RETRY).bw[1][bw] += mpdu_retry_pkts;
	STATS_OP_FMT(RETRY).nss[1][nss] += mpdu_retry_pkts;
	STATS_OP_FMT(RETRY).gi[1][gi] += mpdu_retry_pkts;

	/* RU location and transmit type retry updates */
	if ((ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA ||
	    ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA) &&
	    (txrate->flags & RATE_INFO_FLAGS_HE_MCS ||
	    txrate->flags & RATE_INFO_FLAGS_EHT_MCS ||
	    txrate->flags & RATE_INFO_FLAGS_UHR_MCS)) {
		int ru_loc_idx;

		if (txrate->flags & RATE_INFO_FLAGS_HE_MCS)
			ru_loc_idx = ath12k_he_ru_alloc_to_ru_loc_idx(ru_type);
		else
			ru_loc_idx = ath12k_eht_ru_alloc_to_ru_loc_idx(ru_type);

		if (ru_loc_idx >= 0) {
			STATS_OP_FMT(RETRY).ru_loc[0][ru_loc_idx] += retry_bytes;
			STATS_OP_FMT(RETRY).ru_loc[1][ru_loc_idx] +=
							mpdu_retry_pkts;
		}
	}

	if (ppdu_type < HTT_PPDU_STATS_PPDU_TYPE_MAX) {
		STATS_OP_FMT(RETRY).transmit_type[0][ppdu_type] += retry_bytes;
		STATS_OP_FMT(RETRY).transmit_type[1][ppdu_type] +=
							mpdu_retry_pkts;
	}
}

void
ath12k_debugfs_sta_update_failure(struct ath12k_dp_link_peer *peer,
				  u16 failed_msdu)
{
	struct rate_info *txrate = &peer->txrate;
	struct ath12k_htt_tx_stats *tx_stats = peer->peer_stats.tx_stats;
	int gi, mcs, bw, nss, ru_type;
	u32 failed_bytes = 0, failed_pkts;
	u8 ppdu_type;

	if (!tx_stats)
		return;

	gi = ath12k_debugfs_sta_get_gi_idx(txrate);
	mcs = txrate->mcs;
	bw = ath12k_mac_mac80211_bw_to_ath12k_bw(txrate->bw);
	nss = txrate->nss - 1;
	failed_pkts = failed_msdu;
	ru_type = tx_stats->ru_tones;
	ppdu_type = tx_stats->ppdu_type;

	if (txrate->flags & RATE_INFO_FLAGS_EHT_MCS) {
		STATS_OP_FMT(FAIL).eht[0][mcs] += failed_bytes;
		STATS_OP_FMT(FAIL).eht[1][mcs] += failed_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_UHR_MCS) {
		STATS_OP_FMT(FAIL).uhr[0][mcs] += failed_bytes;
		STATS_OP_FMT(FAIL).uhr[1][mcs] += failed_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_HE_MCS) {
		STATS_OP_FMT(FAIL).he[0][mcs] += failed_bytes;
		STATS_OP_FMT(FAIL).he[1][mcs] += failed_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_VHT_MCS) {
		STATS_OP_FMT(FAIL).vht[0][mcs] += failed_bytes;
		STATS_OP_FMT(FAIL).vht[1][mcs] += failed_pkts;
	} else if (txrate->flags & RATE_INFO_FLAGS_MCS) {
		STATS_OP_FMT(FAIL).ht[0][mcs] += failed_bytes;
		STATS_OP_FMT(FAIL).ht[1][mcs] += failed_pkts;
	} else {
		mcs = tx_stats->rate_idx;
		STATS_OP_FMT(FAIL).legacy[0][mcs] += failed_bytes;
		STATS_OP_FMT(FAIL).legacy[1][mcs] += failed_pkts;
	}

	STATS_OP_FMT(FAIL).bw[0][bw] += failed_bytes;
	STATS_OP_FMT(FAIL).nss[0][nss] += failed_bytes;
	STATS_OP_FMT(FAIL).gi[0][gi] += failed_bytes;

	STATS_OP_FMT(FAIL).bw[1][bw] += failed_pkts;
	STATS_OP_FMT(FAIL).nss[1][nss] += failed_pkts;
	STATS_OP_FMT(FAIL).gi[1][gi] += failed_pkts;
	/* RU location and transmit type failure updates */
	if ((ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_OFDMA ||
	    ppdu_type == HTT_PPDU_STATS_PPDU_TYPE_MU_MIMO_OFDMA) &&
	    (txrate->flags & RATE_INFO_FLAGS_HE_MCS ||
	    txrate->flags & RATE_INFO_FLAGS_EHT_MCS ||
	    txrate->flags & RATE_INFO_FLAGS_UHR_MCS)) {
		int ru_loc_idx;

		if (txrate->flags & RATE_INFO_FLAGS_HE_MCS)
			ru_loc_idx = ath12k_he_ru_alloc_to_ru_loc_idx(ru_type);
		else
			ru_loc_idx = ath12k_eht_ru_alloc_to_ru_loc_idx(ru_type);

		if (ru_loc_idx >= 0) {
			STATS_OP_FMT(FAIL).ru_loc[0][ru_loc_idx] += failed_bytes;
			STATS_OP_FMT(FAIL).ru_loc[1][ru_loc_idx] += failed_pkts;
		}
	}
	if (ppdu_type < HTT_PPDU_STATS_PPDU_TYPE_MAX) {
		STATS_OP_FMT(FAIL).transmit_type[0][ppdu_type] += failed_bytes;
		STATS_OP_FMT(FAIL).transmit_type[1][ppdu_type] += failed_pkts;
	}
}

void
ath12k_debugfs_sta_update_misc(struct ath12k_dp_link_peer *peer,
			       struct ath12k_per_peer_tx_stats *peer_stats)
{
	struct ath12k_htt_tx_stats *tx_stats = peer->peer_stats.tx_stats;

	if (!tx_stats || !peer_stats)
		return;

	tx_stats->tx_duration += peer_stats->duration;
	tx_stats->ru_start = peer_stats->ru_start;
	tx_stats->ru_tones = peer_stats->ru_tones;

	if (peer_stats->mu_grpid < MAX_MU_GROUP_ID &&
	    tx_stats->ppdu_type != HTT_PPDU_STATS_PPDU_TYPE_SU) {
		if (peer_stats->mu_grpid & (MAX_MU_GROUP_ID - 1))
			tx_stats->mu_group[peer_stats->mu_grpid] =
				(peer_stats->mu_pos + 1);
	}
}

static int
ath12k_dbg_sta_open_htt_peer_stats(struct inode *inode, struct file *file)
{
	struct ieee80211_link_sta *link_sta = inode->i_private;
	struct ieee80211_sta *sta = link_sta->sta;
	u8 link_id = link_sta->link_id;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;
	struct debug_htt_stats_req *stats_req;
	int type;
	int ret;

	mutex_lock(&ah->hw_mutex);

	if (!(BIT(link_id) & ahsta->links_map)) {
		ret = -ENOENT;
		goto err_unlock;
	}

	rcu_read_lock();
	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);

	if (!arsta || !arsta->arvif->ar) {
		ret = -ENOENT;
		rcu_read_unlock();
		goto err_unlock;
	}

	ar = arsta->arvif->ar;

	type = ar->debug.htt_stats.type;
	if ((type != ATH12K_DBG_HTT_EXT_STATS_PEER_INFO &&
	     type != ATH12K_DBG_HTT_EXT_PEER_CTRL_PATH_TXRX_STATS) ||
	     type == ATH12K_DBG_HTT_EXT_STATS_RESET) {
		ret = -EPERM;
		rcu_read_unlock();
		goto err_unlock;
	}

	rcu_read_unlock();

	stats_req = vzalloc(sizeof(*stats_req) + ATH12K_HTT_STATS_BUF_SIZE);
	if (!stats_req) {
		return -ENOMEM;
		goto err_unlock;
	}

	ar->debug.htt_stats.stats_req = stats_req;
	stats_req->type = type;
	memcpy(stats_req->peer_addr, link_sta->addr, ETH_ALEN);
	ret = ath12k_debugfs_htt_stats_req(ar);
	if (ret < 0)
		goto out;

	file->private_data = stats_req;
	ar->debug.htt_stats.type = ATH12K_DBG_HTT_EXT_STATS_RESET;
	mutex_unlock(&ah->hw_mutex);
	return 0;
out:
	vfree(stats_req);
	ar->debug.htt_stats.stats_req = NULL;
err_unlock:
	if (ar)
		ar->debug.htt_stats.type = ATH12K_DBG_HTT_EXT_STATS_RESET;
	mutex_unlock(&ah->hw_mutex);
	return ret;
}

static int
ath12k_dbg_sta_release_htt_peer_stats(struct inode *inode, struct file *file)
{
	struct ieee80211_link_sta *link_sta = inode->i_private;
	struct ieee80211_sta *sta = link_sta->sta;
	u8 link_id = link_sta->link_id;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;

	mutex_lock(&ah->hw_mutex);

	if (!(BIT(link_id) & ahsta->links_map)) {
		mutex_unlock(&ah->hw_mutex);
		return -ENOENT;
	}

	rcu_read_lock();
	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);

	if (!arsta || !arsta->arvif->ar) {
		rcu_read_unlock();
		mutex_unlock(&ah->hw_mutex);
		return -ENOENT;
	}

	ar = arsta->arvif->ar;
	rcu_read_unlock();
	vfree(file->private_data);
	ar->debug.htt_stats.stats_req = NULL;
	mutex_unlock(&ah->hw_mutex);

	return 0;
}

static ssize_t ath12k_dbg_sta_read_htt_peer_stats(struct file *file,
						  char __user *user_buf,
						  size_t count, loff_t *ppos)
{
	struct debug_htt_stats_req *stats_req = file->private_data;
	char *buf;
	u32 length = 0;

	buf = stats_req->buf;
	length = min_t(u32, stats_req->buf_len, ATH12K_HTT_STATS_BUF_SIZE);
	return simple_read_from_buffer(user_buf, count, ppos, buf, length);
}

static const struct file_operations fops_htt_peer_stats = {
	.open = ath12k_dbg_sta_open_htt_peer_stats,
	.release = ath12k_dbg_sta_release_htt_peer_stats,
	.read = ath12k_dbg_sta_read_htt_peer_stats,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t
ath12k_dbg_sta_read_qos_msduq(struct file *file, char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_dp_peer *peer;
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_msduq *msduq_map;
	struct ath12k_qos_ctx *qos_ctx;
	const int size = 2048;
	size_t len = 0;
	u16 svc_id, qos_id;
	u8 tid, q;
	int ret = -ENOENT;

	qos_ctx = ath12k_get_qos(ar->ab);
	if (!qos_ctx) {
		ath12k_err(ar->ab, "QoS Context is NULL");
		return ret;
	}

	u8 *buf __free(kfree) = kzalloc(size, GFP_ATOMIC);
	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

	spin_lock_bh(&ah->dp_hw.peer_hash_lock);
	peer = ath12k_dp_peer_find_by_addr(&ah->dp_hw, sta->addr, ahsta->ahvif->vif);
	if (!peer) {
		goto ret;
	}

	qos = peer->qos;
	if (!qos)
		goto ret;

	for (tid = 0; tid < QOS_TID_MAX; tid++) {
		for(q = 0; q < QOS_TID_MDSUQ_MAX; q++) {
			msduq_map = &qos->msduq_map[tid][q];
			if (msduq_map->reserved) {
				qos_id = msduq_map->qos_id;
				svc_id = qos_ctx->profiles[qos_id].svc_id;
				len += scnprintf(buf + len, size - len,
						 "**********************\n");
				if (svc_id) {
					len += scnprintf(buf + len, size - len,
							 "SVC ID:%u\n", svc_id);
				}
				len += scnprintf(buf + len, size - len,
						 "TID: %u\n", tid);
				len += scnprintf(buf + len, size - len,
						 "Queue: %u\n", q);
				len += scnprintf(buf + len, size - len,
						 "QoS ID: %u\n",
						 msduq_map->qos_id);
				len += scnprintf(buf + len, size - len,
						 "msduq_id: %u\n",
						 msduq_map->msduq);
				len += scnprintf(buf + len, size - len,
						 "tgt_opaque_id: 0x%x\n",
						 msduq_map->tgt_opaque_id);
			}
		}
	}
	ret = 0;
ret:
	if (!len)
		len += scnprintf(buf + len, size - len,
				 "No MSDUQ allocated\n");

	spin_unlock_bh(&ah->dp_hw.peer_hash_lock);
	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return ret;
}

static const struct file_operations fops_qos_msduq = {
	.read = ath12k_dbg_sta_read_qos_msduq,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static void ath12k_dbg_sta_reo_queue_stats_cb(struct ath12k_dp *dp, void *ctx,
					      struct hal_reo_status *status)
{
	struct ath12k_dp_rx_tid *rx_tid = ctx;
	struct ath12k_base *ab = dp->ab;
	struct hal_reo_status_queue_stats *qs = &status->u.queue_stats;

	if (status->uniform_hdr.cmd_status != HAL_REO_CMD_SUCCESS) {
		ath12k_warn(ab, "reo queue stats failed tid %u status %d\n",
			    rx_tid->tid, status->uniform_hdr.cmd_status);
		return;
	}

	ath12k_info(ab, "REO queue stats tid %u: ssn %u cur_idx %u\n",
		    rx_tid->tid, qs->ssn, qs->curr_idx);
	ath12k_info(ab, "last_rx: enqueue_ts %08x dequeue_ts %08x\n",
		    qs->last_rx_queue_ts, qs->last_rx_dequeue_ts);
	ath12k_info(ab, "count: cur_mpdu %u cur_msdu %u\n",
		    qs->curr_mpdu_cnt, qs->curr_msdu_cnt);
	ath12k_info(ab, "fwd_timeout %u fwd_bar %u dup_count %u\n",
		    qs->timeout_cnt, qs->fwd_due_to_bar_cnt, qs->dup_cnt);
	ath12k_info(ab, "frames_in_order %u bar_rcvd %u\n",
		    qs->frames_in_order_cnt, qs->bar_rx_cnt);
	ath12k_info(ab, "num_mpdus %u num_msdus %u total_bytes %u\n",
		    qs->num_mpdu_processed_cnt, qs->num_msdu_processed_cnt,
		    qs->total_num_processed_byte_cnt);
	ath12k_info(ab, "late_rcvd %u win_jump_2k %u hole_cnt %u\n",
		    qs->late_rx_mpdu_cnt, qs->num_window_2k_jump_cnt,
		    qs->reorder_hole_cnt);
}

static ssize_t ath12k_dbg_sta_write_fetch_reo_ctx(struct file *file,
					   const char __user *user_buf,
					   size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *primary_arsta;
	struct ath12k *ar;
	struct ath12k_dp_peer *dp_peer;
	struct ath12k_hal_reo_cmd cmd;
	u32 tid_bitmap;
	int ret, tid;
	bool sent = false;

	ret = kstrtou32_from_user(user_buf, count, 0, &tid_bitmap);
	if (ret)
		return ret;

	wiphy_lock(ah->hw->wiphy);
	mutex_lock(&ah->hw_mutex);

	if (ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}

	/* REO queues live on the primary link's chip (wifi7) or the
	 * CU-MAC chip (wifi8); both are reached via primary_link_id.
	 */
	primary_arsta = ahsta->link[ahsta->primary_link_id];
	if (!primary_arsta || !primary_arsta->arvif->ar) {
		ret = -ENOENT;
		goto out;
	}

	ar = primary_arsta->arvif->ar;

	if (!tid_bitmap ||
	    tid_bitmap & ~GENMASK(ar->ab->hal.hal_params->num_tids - 1, 0)) {
		ret = -EINVAL;
		goto out;
	}

	/* dp_peer is keyed by MLD address (sta->addr), not per-link address */
	spin_lock_bh(&ah->dp_hw.peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr(&ah->dp_hw, sta->addr, ahsta->ahvif->vif);
	if (!dp_peer) {
		spin_unlock_bh(&ah->dp_hw.peer_hash_lock);
		ret = -ENOENT;
		goto out;
	}

	for (tid = 0; tid < ar->ab->hal.hal_params->num_tids; tid++) {
		struct ath12k_dp_rx_tid *rx_tid;

		if (!(tid_bitmap & BIT(tid)))
			continue;

		rx_tid = &dp_peer->rx_tid[tid];

		spin_lock_bh(&rx_tid->tid_lock);
		if (!rx_tid->active || !rx_tid->paddr) {
			spin_unlock_bh(&rx_tid->tid_lock);
			continue;
		}

		memset(&cmd, 0, sizeof(cmd));
		cmd.addr_lo = lower_32_bits(rx_tid->paddr);
		cmd.addr_hi = upper_32_bits(rx_tid->paddr);
		cmd.flag = HAL_REO_CMD_FLG_NEED_STATUS;

		ret = ath12k_dp_arch_reo_cmd_send(ar->ab->dp, rx_tid,
						  sizeof(*rx_tid),
						  HAL_REO_CMD_GET_QUEUE_STATS,
						  &cmd,
						  ath12k_dbg_sta_reo_queue_stats_cb);
		spin_unlock_bh(&rx_tid->tid_lock);

		if (ret) {
			ath12k_warn(ar->ab,
				    "failed reo get_queue_stats tid %d (%d)\n",
				    tid, ret);
			break;
		}
		sent = true;
	}
	spin_unlock_bh(&ah->dp_hw.peer_hash_lock);

	if (!ret)
		ret = sent ? count : -ENOENT;
out:
	mutex_unlock(&ah->hw_mutex);
	wiphy_unlock(ah->hw->wiphy);

	return ret;
}

static const struct file_operations fops_fetch_reo_ctx = {
	.write = ath12k_dbg_sta_write_fetch_reo_ctx,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t
ath12k_dbg_sta_read_scs(struct file *file, char __user *user_buf,
			size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	struct ath12k_hw *ah = ath12k_ar_to_ah(ar);
	struct ath12k_dp_peer_qos *qos;
	struct ath12k_dp_peer *peer;
	struct ath12k_dl_scs *scs;
	u8 index, msduq;
	u16 qos_id;
	const int size = 4096;
	int ret  = -EINVAL;
	size_t len = 0;
	u8 *buf;

	buf = kzalloc(size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	spin_lock_bh(&ah->dp_hw.peer_hash_lock);

	peer = ath12k_dp_peer_find_by_addr(&ah->dp_hw, arsta->addr, ahsta->ahvif->vif);
	if (!peer)
		goto ret;

	qos = peer->qos;
	if (!qos)
		goto ret;

	for (index = 0; index < QOS_MAX_SCS_ID; index++) {
		scs = &qos->scs_map[index];
		msduq = u16_get_bits(scs->qos_id_msduq,
				     SCS_MSDUQ_MASK);
		qos_id = u16_get_bits(scs->qos_id_msduq,
				      SCS_QOS_ID_MASK);
		if (!scs->qos_id_msduq)
			continue;

		if (qos_id == QOS_ID_INVALID)
			continue;

		len += scnprintf(buf + len, size - len,
				 "**********************\n");
		len += scnprintf(buf + len, size - len,
				 "SCS ID: %u\n", index);
		len += scnprintf(buf + len, size - len,
				 "QoS ID: %u\n", qos_id);
		if (qos_id <= QOS_DL_ID_MAX) {
			len += scnprintf(buf + len, size - len,
					 "SCS Type: R3 DownLink\n");
			len += scnprintf(buf + len, size - len,
					 "MSDUQ: %u\n", msduq);
		} else if (qos_id <= QOS_UL_ID_MAX) {
			len += scnprintf(buf + len, size - len,
					 "SCS Type: R3 UpLink\n");
		} else {
			len += scnprintf(buf + len, size - len,
					 "SCS Type R2 DownLink\n");
			/* Legacy QoS profile only have TID */
			len += scnprintf(buf + len, size - len,
					 "TID: %u\n",
					 qos_id - QOS_LEGACY_DL_ID_MIN);
			continue;
		}
		len += ath12k_dbg_dump_qos_profile(ar->ab, buf + len,
						   qos_id, size - len);
	}

	ret = 0;
ret:
	spin_unlock_bh(&ah->dp_hw.peer_hash_lock);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

	kfree(buf);
	return ret;
}

static const struct file_operations fops_scs = {
	.read = ath12k_dbg_sta_read_scs,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_write_delba(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	u32 tid, initiator, reason;
	int ret;
	char buf[64] = {0};

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);
	if (ret <= 0)
		return ret;

	ret = sscanf(buf, "%u %u %u", &tid, &initiator, &reason);
	if (ret != 3)
		return -EINVAL;

	/* Valid TID values are 0 through 15 */
	if (tid > HAL_NON_QOS_TID - 1)
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON ||
	    ahsta->aggr_mode != ATH12K_DBG_AGGR_MODE_MANUAL) {
		ret = count;
		goto out;
	}

	ret = ath12k_wmi_delba_send(ar, arsta->arvif->vdev_id, sta->addr,
				    tid, initiator, reason);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send delba: vdev_id %u peer %pM tid %u initiator %u reason %u\n",
			    arsta->arvif->vdev_id, sta->addr, tid, initiator,
			    reason);
	}
	ret = count;
out:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_delba = {
	.write = ath12k_dbg_sta_write_delba,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_write_addba_resp(struct file *file,
					       const char __user *user_buf,
					       size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	u32 tid, status;
	int ret;
	char buf[64] = {0};

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);
	if (ret <= 0)
		return ret;

	ret = sscanf(buf, "%u %u", &tid, &status);
	if (ret != 2)
		return -EINVAL;

	/* Valid TID values are 0 through 15 */
	if (tid > HAL_NON_QOS_TID - 1)
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON ||
	    ahsta->aggr_mode != ATH12K_DBG_AGGR_MODE_MANUAL) {
		ret = count;
		goto out;
	}

	ret = ath12k_wmi_addba_set_resp(ar, arsta->arvif->vdev_id, sta->addr,
					tid, status);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send addba response: vdev_id %u peer %pM tid %u status%u\n",
			    arsta->arvif->vdev_id, sta->addr, tid, status);
	}
	ret = count;
out:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_addba_resp = {
	.write = ath12k_dbg_sta_write_addba_resp,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_write_addba(struct file *file,
					  const char __user *user_buf,
					  size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	u32 tid, buf_size;
	int ret;
	char buf[64] = {0};

	ret = simple_write_to_buffer(buf, sizeof(buf) - 1, ppos,
				     user_buf, count);
	if (ret <= 0)
		return ret;

	ret = sscanf(buf, "%u %u", &tid, &buf_size);
	if (ret != 2)
		return -EINVAL;

	/* Valid TID values are 0 through 15 */
	if (tid > HAL_NON_QOS_TID - 1)
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON ||
	    ahsta->aggr_mode != ATH12K_DBG_AGGR_MODE_MANUAL) {
		ret = count;
		goto out;
	}

	ret = ath12k_wmi_addba_send(ar, arsta->arvif->vdev_id, sta->addr,
				    tid, buf_size);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send addba request: vdev_id %u peer %pM tid %u buf_size %u\n",
			    arsta->arvif->vdev_id, sta->addr, tid, buf_size);
	}

	ret = count;
out:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_addba = {
	.write = ath12k_dbg_sta_write_addba,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_read_aggr_mode(struct file *file,
					     char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	char buf[64];
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf) - len,
			"aggregation mode: %s\n\n%s\n%s\n",
			(ahsta->aggr_mode == ATH12K_DBG_AGGR_MODE_AUTO) ?
			"auto" : "manual", "auto = 0", "manual = 1");
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_dbg_sta_write_aggr_mode(struct file *file,
					      const char __user *user_buf,
					      size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_sta *arsta = &ahsta->deflink;
	struct ath12k *ar = arsta->arvif->ar;
	u32 aggr_mode;
	int ret;

	if (kstrtouint_from_user(user_buf, count, 0, &aggr_mode))
		return -EINVAL;

	if (aggr_mode >= ATH12K_DBG_AGGR_MODE_MAX)
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (ar->ah->state != ATH12K_HW_STATE_ON ||
	    aggr_mode == ahsta->aggr_mode) {
		ret = count;
		goto out;
	}

	ret = ath12k_wmi_addba_clear_resp(ar, arsta->arvif->vdev_id, sta->addr);
	if (ret) {
		ath12k_warn(ar->ab, "failed to clear addba session ret: %d\n",
			    ret);
		goto out;
	}

	ahsta->aggr_mode = aggr_mode;
out:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static const struct file_operations fops_aggr_mode = {
	.read = ath12k_dbg_sta_read_aggr_mode,
	.write = ath12k_dbg_sta_write_aggr_mode,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_read_primary_link_id(struct file *file,
						   char __user *user_buf,
						   size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	char buf[32];
	int len = 0;

	mutex_lock(&ah->hw_mutex);
	len = scnprintf(buf, sizeof(buf) - len,
			"Primary_link_id: %d\n", ahsta->primary_link_id);
	mutex_unlock(&ah->hw_mutex);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t ath12k_dbg_sta_read_primary_link_info(struct file *file,
						     char __user *user_buf,
						     size_t count, loff_t *ppos)
{
	struct ieee80211_sta *sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah;
	struct ath12k_vif *ahvif;
	struct ath12k_link_vif *arvif;
	char buf[512];
	int len = 0;

	if (!ahsta || !ahsta->ahvif || !ahsta->ahvif->ah)
		return -EINVAL;

	if (ahsta->primary_link_id < 0 || !ahsta->link[ahsta->primary_link_id])
		return -EINVAL;

	ahvif = ahsta->ahvif;
	ah = ahsta->ahvif->ah;

	wiphy_lock(ah->hw->wiphy);
	mutex_lock(&ah->hw_mutex);

	arvif = ath12k_get_arvif_from_link_id(ahvif, ahsta->primary_link_id);
	if (!arvif) {
		mutex_unlock(&ah->hw_mutex);
		wiphy_unlock(ah->hw->wiphy);
		return -EINVAL;
	}

	len += scnprintf(buf + len, sizeof(buf) - len,
			"primary_link_id: %d\n", ahsta->primary_link_id);
	len += scnprintf(buf + len, sizeof(buf) - len,
			"bss_id: %pM\n", arvif->bssid);
	len += scnprintf(buf + len, sizeof(buf) - len,
			"vdev_id: %u\n", arvif->vdev_id);
	len += scnprintf(buf + len, sizeof(buf) - len,
			"pdev_id: %u\n", arvif->ar->pdev->pdev_id);

	mutex_unlock(&ah->hw_mutex);
	wiphy_unlock(ah->hw->wiphy);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_primary_link_id = {
	.read = ath12k_dbg_sta_read_primary_link_id,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static const struct file_operations fops_primary_link_info = {
	.read = ath12k_dbg_sta_read_primary_link_info,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t
ath12k_write_htt_peer_stats_reset(struct file *file,
				  const char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ieee80211_sta *sta = link_sta->sta;
	u8 link_id = link_sta->link_id;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;
	struct htt_ext_stats_cfg_params cfg_params = { 0 };
	int ret;
	u8 type;

	ret = kstrtou8_from_user(user_buf, count, 0, &type);
	if (ret)
		return ret;

	if (!type)
		return ret;

	mutex_lock(&ah->hw_mutex);

	if (!(BIT(link_id) & ahsta->links_map)) {
		mutex_unlock(&ah->hw_mutex);
		return -ENOENT;
	}

	rcu_read_lock();
	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);

	if (!arsta || !arsta->arvif->ar) {
		rcu_read_unlock();
		mutex_unlock(&ah->hw_mutex);
		return -ENOENT;
	}

	ar = arsta->arvif->ar;
	rcu_read_unlock();

	cfg_params.cfg0 = HTT_STAT_PEER_INFO_MAC_ADDR;
	cfg_params.cfg0 |= FIELD_PREP(GENMASK(15, 1),
				HTT_PEER_STATS_REQ_MODE_FLUSH_TQM);

	cfg_params.cfg1 = HTT_STAT_DEFAULT_PEER_REQ_TYPE;

	cfg_params.cfg2 |= FIELD_PREP(GENMASK(7, 0), link_sta->addr[0]);
	cfg_params.cfg2 |= FIELD_PREP(GENMASK(15, 8), link_sta->addr[1]);
	cfg_params.cfg2 |= FIELD_PREP(GENMASK(23, 16), link_sta->addr[2]);
	cfg_params.cfg2 |= FIELD_PREP(GENMASK(31, 24), link_sta->addr[3]);

	cfg_params.cfg3 |= FIELD_PREP(GENMASK(7, 0), link_sta->addr[4]);
	cfg_params.cfg3 |= FIELD_PREP(GENMASK(15, 8), link_sta->addr[5]);

	cfg_params.cfg3 |= ATH12K_HTT_PEER_STATS_RESET;

	ret = ath12k_dp_tx_htt_h2t_ext_stats_req(ar,
						 ATH12K_DBG_HTT_EXT_STATS_PEER_INFO,
						 &cfg_params,
						 0ULL);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send htt peer stats request: %d\n", ret);
		mutex_unlock(&ah->hw_mutex);
		return ret;
	}

	mutex_unlock(&ah->hw_mutex);

	ret = count;

	return ret;
}

static const struct file_operations fops_htt_peer_stats_reset = {
	.write = ath12k_write_htt_peer_stats_reset,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

#ifdef CPTCFG_ATH12K_CFR
static ssize_t ath12k_dbg_sta_write_cfr_capture(struct file *file,
						const char __user *user_buf,
						size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ieee80211_sta *sta = link_sta->sta;
	struct ath12k_sta *ahsta = (struct ath12k_sta *)sta->drv_priv;
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;
	struct wmi_peer_cfr_capture_conf_arg arg;
	u8 link_id = link_sta->link_id;
	u32 cfr_capture_enable = 0, cfr_capture_bw  = 0;
	u32 cfr_capture_method = 0, cfr_capture_period = 0;
	int ret;
	char buf[64] = {0};

	simple_write_to_buffer(buf, sizeof(buf) - 1, ppos, user_buf, count);

	wiphy_lock(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map)) {
		ret = -ENOENT;
		goto out;
	}

	arsta = ahsta->link[link_id];
	if (!arsta || !arsta->arvif->ar) {
		ret = -ENOENT;
		goto out;
	}

	ar = arsta->arvif->ar;

	if (ah->state != ATH12K_HW_STATE_ON) {
		ret = -ENETDOWN;
		goto out;
	}
	if (!ar->cfr.cfr_enabled) {
		ret = -EINVAL;
		goto out;
	}

	ret = sscanf(buf, "%u %u %u %u", &cfr_capture_enable, &cfr_capture_bw,
		     &cfr_capture_period, &cfr_capture_method);
	if ((ret < 1) || (cfr_capture_enable && ret != 4)) {
		ret = -EINVAL;
		goto out;
	}

	ret = ath12k_cfr_peer_capture_validate(ar, arsta, link_sta->bandwidth,
					       &cfr_capture_enable,
					       &cfr_capture_bw,
					       &cfr_capture_period,
					       &cfr_capture_method);
	if (ret == -EINVAL)
		goto out;

	if (ret == -EALREADY) {
		ret = count;
		goto out;
	}

	ath12k_cfr_peer_capture_fill_wmi_arg(&arg, cfr_capture_enable,
					     cfr_capture_bw,
					     cfr_capture_period,
					     cfr_capture_method);

	ret = ath12k_wmi_peer_set_cfr_capture_conf(ar, arsta->arvif->vdev_id,
						   link_sta->addr, &arg);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send cfr capture info: vdev_id %u peer %pM\n",
			    arsta->arvif->vdev_id, link_sta->addr);
		goto out;
	}
	ret = count;

	ath12k_cfr_peer_capture_update(ar, arsta, cfr_capture_enable,
				       cfr_capture_bw, cfr_capture_period,
				       cfr_capture_method);
out:
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

static ssize_t ath12k_dbg_sta_read_cfr_capture(struct file *file,
					       char __user *user_buf,
					       size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ieee80211_sta *sta = link_sta->sta;
	struct ath12k_sta *ahsta = (struct ath12k_sta *)sta->drv_priv;
	struct ath12k_link_sta *arsta;
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	u8 link_id = link_sta->link_id;
	char buf[512] = {0};
	int len = 0;

	guard(wiphy)(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map))
		return -ENOENT;

	arsta = ahsta->link[link_id];
	if (!arsta || !arsta->arvif->ar)
		return -ENOENT;

	len += scnprintf(buf + len, sizeof(buf) - len, "cfr_enabled = %d\n",
			 arsta->cfr_capture.cfr_enable);
	len += scnprintf(buf + len, sizeof(buf) - len, "bandwidth = %d\n",
			 arsta->cfr_capture.cfr_bandwidth);
	len += scnprintf(buf + len, sizeof(buf) - len, "period = %d\n",
			 arsta->cfr_capture.cfr_period);
	len += scnprintf(buf + len, sizeof(buf) - len, "cfr_method = %d\n",
			 arsta->cfr_capture.cfr_method);

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static const struct file_operations fops_peer_cfr_capture = {
	.write = ath12k_dbg_sta_write_cfr_capture,
	.read = ath12k_dbg_sta_read_cfr_capture,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};
#endif /* CPTCFG_ATH12K_CFR */

void ath12k_debugfs_sta_op_add(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta, struct dentry *dir)
{
	debugfs_create_file("aggr_mode", 0644, dir, sta, &fops_aggr_mode);
	debugfs_create_file("addba", 0200, dir, sta, &fops_addba);
	debugfs_create_file("addba_resp", 0200, dir, sta, &fops_addba_resp);
	debugfs_create_file("delba", 0200, dir, sta, &fops_delba);
	debugfs_create_file("primary_link_id", 0400, dir, sta, &fops_primary_link_id);
	debugfs_create_file("primary_link_info", 0400, dir, sta, &fops_primary_link_info);
}
EXPORT_SYMBOL(ath12k_debugfs_sta_op_add);

static ssize_t ath12k_dbg_sta_dump_tx_stats(struct file *file,
                                           char __user *user_buf,
                                           size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(link_sta->sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	u8 link_id = link_sta->link_id;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_peer_stats *peer_stats;
	struct ath12k_htt_tx_stats *tx_stats;
	struct ath12k_htt_data_stats *stats;
	static const char *str_name[ATH12K_STATS_TYPE_MAX] = {"success", "fail",
                                                             "retry", "ampdu"};
	static const char *str[ATH12K_COUNTER_TYPE_MAX] = {"bytes", "packets"};
	int len = 0, i, j, k, retval = 0;
	const int size = 2 * 4096;
	u32 wbm_rel_stats[HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX] = {0};
	char mu_group_id[MAX_MU_GROUP_LENGTH] = {0};
	u32 index;
	static const char *fields[] =
		{[HAL_WBM_REL_HTT_TX_COMP_STATUS_OK] = "Acked pkt count",
		 [HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL] = "Status ttl pkt count",
		 [HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP] = "Dropped pkt count",
		 [HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ] = "Reinj pkt count",
		 [HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT] = "Inspect pkt count",
		 [HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY] = "MEC notify pkt count"};

	wiphy_lock(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map)) {
		retval = -ENOENT;
		goto unlock;
	}

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
		retval = -ENOENT;
		goto unlock;
	}

	ar = arsta->arvif->ar;
	if (!ar) {
		retval = -ENOENT;
		goto unlock;
	}

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ah->hw->wiphy, ahsta);
	if (!dp_peer) {
		retval = -ENOENT;
		goto unlock;
	}

	if (ar->hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS) {
		retval = -ENOENT;
		goto unlock;
	}

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ar->hw_link_id);
	if (!link_peer) {
		retval = -ENOENT;
		goto unlock_rcu;
	}

	tx_stats = link_peer->peer_stats.tx_stats;
	if (!tx_stats) {
		retval = -ENOENT;
		goto unlock_rcu;
	}

	u8 *buf __free(kfree) = kzalloc(size, GFP_ATOMIC);
	if (!buf) {
		retval = -ENOENT;
		goto unlock_rcu;
	}

	peer_stats = &dp_peer->stats[ar->hw_link_id];

	for (k = 0; k < ATH12K_STATS_TYPE_MAX; k++) {
               for (j = 0; j < ATH12K_COUNTER_TYPE_MAX; j++) {
                       stats = &tx_stats->stats[k];
                       len += scnprintf(buf + len, size - len, "%s_%s\n",
                                        str_name[k],
                                        str[j]);
                       len += scnprintf(buf + len, size - len, "==========\n");
		       len += scnprintf(buf + len, size - len,
				       " EHT MCS %s\n\t",
				       str[j]);
		       for (i = 0; i < ATH12K_EHT_MCS_NUM; i++)
			       len += scnprintf(buf + len, size - len,
					     "%llu ",
					     stats->eht[j][i]);
		       len += scnprintf(buf + len, size - len, "\n");
		       len += scnprintf(buf + len, size - len,
                                        " HE MCS %s\n\t",
                                        str[j]);
		       for (i = 0; i < ATH12K_HE_MCS_NUM; i++)
			       len += scnprintf(buf + len, size - len,
                                                "%llu ",
                                                stats->he[j][i]);
                       len += scnprintf(buf + len, size - len, "\n");
                       len += scnprintf(buf + len, size - len,
                                        " VHT MCS %s\n\t",
                                        str[j]);
                       for (i = 0; i < ATH12K_VHT_MCS_NUM; i++)
                               len += scnprintf(buf + len, size - len,
                                                "%llu ",
                                                stats->vht[j][i]);
                       len += scnprintf(buf + len, size - len, "\n");
                       len += scnprintf(buf + len, size - len, " HT MCS %s\n\t",
                                        str[j]);
                       for (i = 0; i < ATH12K_HT_MCS_NUM; i++)
                               len += scnprintf(buf + len, size - len,
                                                "%llu ", stats->ht[j][i]);
                       len += scnprintf(buf + len, size - len, "\n");
                       len += scnprintf(buf + len, size - len,
                                        " BW %s (20,40,80,160,320 MHz)\n", str[j]);
                       len += scnprintf(buf + len, size - len,
                                        "\t%llu %llu %llu %llu %llu\n",
                                        stats->bw[j][0], stats->bw[j][1],
                                        stats->bw[j][2], stats->bw[j][3],
                                        stats->bw[j][4]);
                       len += scnprintf(buf + len, size - len,
                                        " NSS %s (1x1,2x2,3x3,4x4)\n", str[j]);
                       len += scnprintf(buf + len, size - len,
                                        "\t%llu %llu %llu %llu\n",
                                        stats->nss[j][0], stats->nss[j][1],
                                        stats->nss[j][2], stats->nss[j][3]);
                       len += scnprintf(buf + len, size - len,
                                        " GI %s (0.4us,0.8us,1.6us,3.2us)\n",
                                        str[j]);
                       len += scnprintf(buf + len, size - len,
                                        "\t%llu %llu %llu %llu\n",
                                        stats->gi[j][0], stats->gi[j][1],
                                        stats->gi[j][2], stats->gi[j][3]);
                       len += scnprintf(buf + len, size - len,
                                        " legacy rate %s (1,2 ... Mbps)\n  ",
                                        str[j]);
                       for (i = 0; i < ATH12K_LEGACY_NUM; i++)
                               len += scnprintf(buf + len, size - len, "%llu ",
                                                stats->legacy[j][i]);

                       len += scnprintf(buf + len, size - len, "\n ru %s:\n", str[j]);
                       len += scnprintf(buf + len, size - len,
                                        "\tru 26: %llu\n", stats->ru_loc[j][0]);
                       len += scnprintf(buf + len, size - len,
                                        "\tru 52: %llu\n", stats->ru_loc[j][1]);
                       len += scnprintf(buf + len, size - len,
                                        "\tru 106: %llu\n", stats->ru_loc[j][2]);
                       len += scnprintf(buf + len, size - len,
                                        "\tru 242: %llu\n", stats->ru_loc[j][3]);
                       len += scnprintf(buf + len, size - len,
                                        "\tru 484: %llu\n", stats->ru_loc[j][4]);
                       len += scnprintf(buf + len, size - len,
                                        "\tru 996: %llu\n", stats->ru_loc[j][5]);

                       len += scnprintf(buf + len, size - len,
                                        " ppdu type %s:\n", str[j]);
                       if (k == ATH12K_STATS_TYPE_FAIL ||
                           k == ATH12K_STATS_TYPE_RETRY) {
                               len += scnprintf(buf + len, size - len,
                                                "\tSU/MIMO: %llu\n",
                                                stats->transmit_type[j][0]);
                               len += scnprintf(buf + len, size - len,
                                                "\tOFDMA/OFDMA_MIMO: %llu\n",
                                                stats->transmit_type[j][2]);
                       } else {
                               len += scnprintf(buf + len, size - len,
                                                "\tSU: %llu\n",
                                                stats->transmit_type[j][0]);
                               len += scnprintf(buf + len, size - len,
                                                "\tMIMO: %llu\n",
                                                stats->transmit_type[j][1]);
                               len += scnprintf(buf + len, size - len,
                                                "\tOFDMA: %llu\n",
                                                stats->transmit_type[j][2]);
                               len += scnprintf(buf + len, size - len,
                                                "\tOFDMA_MIMO: %llu\n",
                                                stats->transmit_type[j][3]);
                       }
               }
       }

       len += scnprintf(buf + len, size - len, "\n");

       for (i = 0; i < MAX_MU_GROUP_ID;) {
               index = 0;
               for (j = 0; j < MAX_MU_GROUP_SHOW && i < MAX_MU_GROUP_ID; j++) {
                       index += snprintf(&mu_group_id[index],
                                         MAX_MU_GROUP_LENGTH - index,
                                         " %d",
                                         tx_stats->mu_group[i]);
                       i++;
		}
	len += scnprintf(buf + len, size - len, "User position list for GID %02d->%d: [%s]\n",
			i - MAX_MU_GROUP_SHOW, i - 1, mu_group_id);
	}

       len += scnprintf(buf + len, size - len,
			"\nLast Packet RU index [%d], Size [%d]\n",
			tx_stats->ru_start, tx_stats->ru_tones);

	len += scnprintf(buf + len, size - len,
			"\nTX duration\n %llu usecs\n",
			link_peer->tx_duration);

	len += scnprintf(buf + len, size - len,
			"BA fails\n %llu\n", tx_stats->ba_fails);

	len += scnprintf(buf + len, size - len,
			"ack fails\n %llu\n\n", tx_stats->ack_fails);

	for (j = 0; j < HAL_WBM_REL_HTT_TX_COMP_STATUS_MAX; j++)
		wbm_rel_stats[j] = peer_stats->tx_dbg.wbm_rel_reason[j];

	len += scnprintf(buf + len, size - len,
			 "WBM tx completion stats of data pkts :\n");
	for (j = 0; j <= HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY; j++) {
		len += scnprintf(buf + len, size - len,
				 "%-23s :  %d\n",
				 fields[j],
				 wbm_rel_stats[j]);
	}

unlock_rcu:
	rcu_read_unlock();

	if (len)
		retval = simple_read_from_buffer(user_buf, count, ppos, buf, len);

unlock:
	wiphy_unlock(ah->hw->wiphy);
	return retval;

}

static const struct file_operations fops_tx_stats = {
       .read = ath12k_dbg_sta_dump_tx_stats,
       .open = simple_open,
       .owner = THIS_MODULE,
       .llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_reset_tx_stats(struct file *file,
                                             const char __user *buf,
                                             size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(link_sta->sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	u8 link_id = link_sta->link_id;
	struct ath12k *ar;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer = NULL;
	struct ath12k_htt_tx_stats *tx_stats;
	bool reset;
	int ret;

	ret = kstrtobool_from_user(buf, count, &reset);
	if (ret)
		return ret;

	if (!reset)
		return -EINVAL;

	wiphy_lock(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map)) {
		ret = -ENOENT;
		goto unlock;
	}

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
		ret = -ENOENT;
		goto unlock;
	}

	ar = arsta->arvif->ar;
	if (!ar) {
		ret = -ENOENT;
		goto unlock;
	}

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ah->hw->wiphy, ahsta);
	if (!dp_peer) {
		ret = -ENOENT;
		goto unlock;
	}

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ar->hw_link_id);
	if (!link_peer) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	tx_stats = link_peer->peer_stats.tx_stats;
	if (!tx_stats) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	memset(tx_stats, 0, sizeof(*tx_stats));
	ret = count;

unlock_rcu:
	rcu_read_unlock();
unlock:
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

static const struct file_operations fops_reset_tx_stats = {
        .write = ath12k_dbg_sta_reset_tx_stats,
        .open = simple_open,
        .owner = THIS_MODULE,
        .llseek = default_llseek,
};

static ssize_t
ath12k_dbg_sta_dump_driver_rx_pkts_flow(struct file *file,
					char __user *user_buf,
					size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(link_sta->sta);
	const int size = ATH12K_STA_RX_STATS_BUF_SIZE;
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	struct ath12k *ar;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_peer_stats *peer_stats;
	u8 link_id = link_sta->link_id;
	int len = 0, i, ret = 0;
	u32 recv_from_reo = 0, sent_to_stack = 0;

	wiphy_lock(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map)) {
		ret = -ENOENT;
		goto unlock;
	}

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
		ret = -ENOENT;
		goto unlock;
	}

	ar = arsta->arvif->ar;
	if (!ar) {
		ret = -ENOENT;
		goto unlock;
	}

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ah->hw->wiphy, ahsta);
	if (!dp_peer) {
		ret = -ENOENT;
		goto unlock;
	}

	if (ar->hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS) {
		ret = -ENOENT;
		goto unlock;
	}

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ar->hw_link_id);
	if (!link_peer) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	u8 *buf __free(kfree) = kzalloc(size, GFP_ATOMIC);
	if (!buf) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	peer_stats = &dp_peer->stats[ar->hw_link_id];

	for (i = 0; i < DP_REO_DST_RING_MAX; i++)
		recv_from_reo += peer_stats->rx[i].recv_from_reo.packets;

	len += scnprintf(buf + len, size - len,
			 "Rx packets inflow from HW:%u\n", recv_from_reo);

	for (i = 0; i < DP_REO_DST_RING_MAX; i++) {
		sent_to_stack += peer_stats->rx[i].sent_to_stack.packets;
		sent_to_stack += peer_stats->rx[i].sent_to_stack_fast.packets;
	}

	len += scnprintf(buf + len, size - len,
			 "\nRx packets outflow from driver:%u\n", sent_to_stack);

	len += scnprintf(buf + len, size - len, "\n");

unlock_rcu:
	rcu_read_unlock();

	if (len)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);

unlock:
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

static const struct file_operations fops_driver_rx_pkts_flow = {
	.read = ath12k_dbg_sta_dump_driver_rx_pkts_flow,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static
u32 ath12k_dbg_sta_dump_rate_stats(u8 *buf, u32 offset, const int size,
				   bool he_rates_avail,
				   const struct ath12k_rx_peer_rate_stats *stats)
{
	static const char *legacy_rate_str[HAL_RX_MAX_NUM_LEGACY_RATES] = {
					"1 Mbps", "2 Mbps", "5.5 Mbps", "6 Mbps",
					"9 Mbps", "11 Mbps", "12 Mbps", "18 Mbps",
					"24 Mbps", "36 Mbps", "48 Mbps", "54 Mbps"};
	u8 max_bw = HAL_RX_BW_MAX, max_gi = HAL_RX_GI_MAX, max_mcs = HAL_RX_MAX_NSS;
	int mcs = 0, bw = 0, nss = 0, gi = 0, bw_num = 0;
	u32 i, len = offset, max = max_bw * max_gi * max_mcs;
	bool found;

	len += scnprintf(buf + len, size - len, "\nEHT stats:\n");
	for (i = 0; i <= HAL_RX_MAX_MCS_BE; i++)
		len += scnprintf(buf + len, size - len,
				   "MCS %d: %llu%s", i, stats->be_mcs_count[i],
				   (i + 1) % 8 ? "\t" : "\n");

	len += scnprintf(buf + len, size - len, "\nHE stats:\n");
	for (i = 0; i <= HAL_RX_MAX_MCS_HE; i++)
		len += scnprintf(buf + len, size - len,
				   "MCS %d: %llu%s", i, stats->he_mcs_count[i],
				   (i + 1) % 6 ? "\t" : "\n");

	len += scnprintf(buf + len, size - len, "\nVHT stats:\n");
	for (i = 0; i <= HAL_RX_MAX_MCS_VHT; i++)
		len += scnprintf(buf + len, size - len,
				   "MCS %d: %llu%s", i, stats->vht_mcs_count[i],
				   (i + 1) % 5 ? "\t" : "\n");

	len += scnprintf(buf + len, size - len, "\nHT stats:\n");
	for (i = 0; i <= HAL_RX_MAX_MCS_HT; i++)
		len += scnprintf(buf + len, size - len,
				   "MCS %d: %llu%s", i, stats->ht_mcs_count[i],
				   (i + 1) % 8 ? "\t" : "\n");

	len += scnprintf(buf + len, size - len, "\nLegacy stats:\n");
	for (i = 0; i < HAL_RX_MAX_NUM_LEGACY_RATES; i++)
		len += scnprintf(buf + len, size - len,
				   "%s: %llu%s", legacy_rate_str[i],
				   stats->legacy_count[i],
				   (i + 1) % 4 ? "\t" : "\n");

	len += scnprintf(buf + len, size - len, "\nNSS stats:\n");
	for (i = 0; i < HAL_RX_MAX_NSS; i++)
		len += scnprintf(buf + len, size - len,
				   "%dx%d: %llu ", i + 1, i + 1,
				   stats->nss_count[i]);

	len += scnprintf(buf + len, size - len,
			  "\n\nGI: 0.8 us %llu 0.4 us %llu 1.6 us %llu 3.2 us %llu\n",
			  stats->gi_count[0],
			  stats->gi_count[1],
			  stats->gi_count[2],
			  stats->gi_count[3]);

	len += scnprintf(buf + len, size - len,
			   "BW: 20 MHz %llu 40 MHz %llu 80 MHz %llu 160 MHz %llu 320 MHz %llu\n",
			   stats->bw_count[0],
			   stats->bw_count[1],
			   stats->bw_count[2],
			   stats->bw_count[3],
			   stats->bw_count[4]);

	for (i = 0; i < max; i++) {
		found = false;

		for (mcs = 0; mcs <= HAL_RX_MAX_MCS_HT; mcs++) {
			if (stats->rx_rate[bw][gi][nss][mcs]) {
				found = true;
				break;
			}
		}

		if (!found)
			goto skip_report;

		switch (bw) {
		case HAL_RX_BW_20MHZ:
			bw_num = 20;
			break;
		case HAL_RX_BW_40MHZ:
			bw_num = 40;
			break;
		case HAL_RX_BW_80MHZ:
			bw_num = 80;
			break;
		case HAL_RX_BW_160MHZ:
			bw_num = 160;
			break;
		case HAL_RX_BW_320MHZ:
			bw_num = 320;
			break;
		}

		len += scnprintf(buf + len, size - len, "\n%d Mhz gi %d us %dx%d : ",
				 bw_num, gi, nss + 1, nss + 1);

		for (mcs = 0; mcs <= HAL_RX_MAX_MCS_HT; mcs++) {
			if (stats->rx_rate[bw][gi][nss][mcs])
				len += scnprintf(buf + len, size - len,
						 " %d:%llu", mcs,
						 stats->rx_rate[bw][gi][nss][mcs]);
		}

skip_report:
		if (nss++ >= max_mcs - 1) {
			nss = 0;
			if (gi++ >= max_gi - 1) {
				gi = 0;
				if (bw < max_bw - 1)
					bw++;
			}
		}
	}

	len += scnprintf(buf + len, size - len, "\n");

	return len - offset;
}

static ssize_t ath12k_dbg_sta_dump_rx_stats(struct file *file,
					    char __user *user_buf,
					    size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(link_sta->sta);
	const int size = ATH12K_STA_RX_STATS_BUF_SIZE;
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_rx_peer_stats *rx_stats;
	struct ath12k_link_sta *arsta;
	struct ath12k_dp_peer *dp_peer = NULL;
	u8 link_id = link_sta->link_id;
	int len = 0, i, ret = 0;
	bool he_rates_avail;
	struct ath12k *ar;
	struct ath12k_dp_link_peer *link_peer;

	wiphy_lock(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map)) {
		ret = -ENOENT;
		goto unlock;
	}

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
		ret = -ENOENT;
		goto unlock;
	}

	ar = arsta->arvif->ar;
	if (!ar) {
		ret = -ENOENT;
		goto unlock;
	}

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ah->hw->wiphy, ahsta);
	if (!dp_peer) {
		ret = -ENOENT;
		goto unlock;
	}

	if (ar->hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS) {
		ret = -ENOENT;
		goto unlock;
	}

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, ar->hw_link_id);
	if (!link_peer) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	rx_stats = link_peer->peer_stats.rx_stats;
	if (!rx_stats) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	u8 *buf __free(kfree) = kzalloc(size, GFP_ATOMIC);
	if (!buf) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	len += scnprintf(buf + len, size - len, "RX peer stats:\n\n");
	len += scnprintf(buf + len, size - len, "Num of MSDUs: %llu\n",
			 rx_stats->num_msdu);
	len += scnprintf(buf + len, size - len, "Num of MSDUs with TCP L4: %llu\n",
			 rx_stats->tcp_msdu_count);
	len += scnprintf(buf + len, size - len, "Num of MSDUs with UDP L4: %llu\n",
			 rx_stats->udp_msdu_count);
	len += scnprintf(buf + len, size - len, "Num of other MSDUs: %llu\n",
			 rx_stats->other_msdu_count);
	len += scnprintf(buf + len, size - len, "Num of MSDUs part of AMPDU: %llu\n",
			 rx_stats->ampdu_msdu_count);
	len += scnprintf(buf + len, size - len, "Num of MSDUs not part of AMPDU: %llu\n",
			 rx_stats->non_ampdu_msdu_count);
	len += scnprintf(buf + len, size - len, "Num of MSDUs using STBC: %llu\n",
			 rx_stats->stbc_count);
	len += scnprintf(buf + len, size - len, "Num of MSDUs beamformed: %llu\n",
			 rx_stats->beamformed_count);
	len += scnprintf(buf + len, size - len, "Num of MPDUs with FCS ok: %llu\n",
			 rx_stats->num_mpdu_fcs_ok);
	len += scnprintf(buf + len, size - len, "Num of MPDUs with FCS error: %llu\n",
			 rx_stats->num_mpdu_fcs_err);

	he_rates_avail = (rx_stats->pream_cnt[HAL_RX_PREAMBLE_11AX] > 1) ? true : false;

	len += scnprintf(buf + len, size - len,
			 "preamble: 11A %llu 11B %llu 11N %llu 11AC %llu 11AX %llu 11BE %llu\n",
			 rx_stats->pream_cnt[0], rx_stats->pream_cnt[1],
			 rx_stats->pream_cnt[2], rx_stats->pream_cnt[3],
			 rx_stats->pream_cnt[4], rx_stats->pream_cnt[6]);
	len += scnprintf(buf + len, size - len,
			 "reception type: SU %llu MU_MIMO %llu MU_OFDMA %llu MU_OFDMA_MIMO %llu\n",
			 rx_stats->reception_type[0], rx_stats->reception_type[1],
			 rx_stats->reception_type[2], rx_stats->reception_type[3]);

	len += scnprintf(buf + len, size - len, "TID(0-15) Legacy TID(16):");
	for (i = 0; i <= IEEE80211_NUM_TIDS; i++)
		len += scnprintf(buf + len, size - len, "%llu ", rx_stats->tid_count[i]);

	len += scnprintf(buf + len, size - len, "\nRX Duration:%llu\n",
			 rx_stats->rx_duration);

	len += scnprintf(buf + len, size - len,
			 "\nDCM: %llu\nRU26:  %llu\nRU52:  %llu\nRU106: %llu\nRU242: %llu\nRU484: %llu\nRU996: %llu\nRU996x2: %llu\n",
			 rx_stats->dcm_count, rx_stats->ru_alloc_cnt[0],
			 rx_stats->ru_alloc_cnt[1], rx_stats->ru_alloc_cnt[2],
			 rx_stats->ru_alloc_cnt[3], rx_stats->ru_alloc_cnt[4],
			 rx_stats->ru_alloc_cnt[5], rx_stats->ru_alloc_cnt[6]);

	len += scnprintf(buf + len, size - len, "\nRX success packet stats:\n");
	len += ath12k_dbg_sta_dump_rate_stats(buf, len, size, he_rates_avail,
					      &rx_stats->pkt_stats);

	len += scnprintf(buf + len, size - len, "\n");

	len += scnprintf(buf + len, size - len, "\nRX success byte stats:\n");
	len += ath12k_dbg_sta_dump_rate_stats(buf, len, size, he_rates_avail,
					      &rx_stats->byte_stats);
unlock_rcu:
	rcu_read_unlock();
	if (len)
		ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
unlock:
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

static const struct file_operations fops_rx_stats = {
	.read = ath12k_dbg_sta_dump_rx_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};

static ssize_t ath12k_dbg_sta_reset_rx_stats(struct file *file,
					     const char __user *buf,
					     size_t count, loff_t *ppos)
{
	struct ieee80211_link_sta *link_sta = file->private_data;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(link_sta->sta);
	struct ath12k_hw *ah = ahsta->ahvif->ah;
	struct ath12k_link_sta *arsta;
	u8 link_id = link_sta->link_id;
	struct ath12k *ar;
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_link_peer *link_peer;
	struct ath12k_rx_peer_stats *rx_stats;
	bool reset;
	int ret;

	ret = kstrtobool_from_user(buf, count, &reset);
	if (ret)
		return ret;

	if (!reset)
		return -EINVAL;

	wiphy_lock(ah->hw->wiphy);

	if (!(BIT(link_id) & ahsta->links_map)) {
		ret = -ENOENT;
		goto unlock;
	}

	arsta = wiphy_dereference(ah->hw->wiphy, ahsta->link[link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar) {
		ret = -ENOENT;
		goto unlock;
	}

	ar = arsta->arvif->ar;
	if (!ar) {
		ret = -ENOENT;
		goto unlock;
	}

	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(ah->hw->wiphy, ahsta);
	if (!dp_peer) {
		ret = -ENOENT;
		goto unlock;
	}

	if (ar->hw_link_id >= ATH12K_DP_PEER_MAX_MLO_LINKS) {
		ret = -ENOENT;
		goto unlock;
	}

	rcu_read_lock();
	link_peer = ath12k_dp_link_peer_find_by_hw_link_id(dp_peer, link_id);
	if (!link_peer) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	rx_stats = link_peer->peer_stats.rx_stats;
	if (!rx_stats) {
		ret = -ENOENT;
		goto unlock_rcu;
	}

	memset(rx_stats, 0, sizeof(*rx_stats));
	ret = count;

unlock_rcu:
	rcu_read_unlock();
unlock:
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

static const struct file_operations fops_reset_rx_stats = {
	.write = ath12k_dbg_sta_reset_rx_stats,
	.open = simple_open,
	.owner = THIS_MODULE,
	.llseek = default_llseek,
};


void ath12k_debugfs_link_sta_op_add(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_link_sta *link_sta,
				    struct dentry *dir)
{
	struct ath12k *ar;

	lockdep_assert_wiphy(hw->wiphy);

	ar = ath12k_get_ar_by_vif(hw, vif, link_sta->link_id);
	if (!ar)
		return;

	if (ath12k_extd_tx_stats_enabled(&ar->dp)) {
                debugfs_create_file("tx_stats", 0400, dir, link_sta,
                                    &fops_tx_stats);
                debugfs_create_file("reset_tx_stats", 0200, dir, link_sta,
                                    &fops_reset_tx_stats);
        }

	if (ath12k_extd_rx_stats_enabled(&ar->dp)) {
		debugfs_create_file("rx_stats", 0400, dir, link_sta,
				    &fops_rx_stats);
		debugfs_create_file("reset_rx_stats", 0200, dir, link_sta,
				    &fops_reset_rx_stats);
		debugfs_create_file("driver_rx_pkts_flow", 0400, dir, link_sta,
				    &fops_driver_rx_pkts_flow);
	}

	debugfs_create_file("htt_peer_stats", 0400, dir, link_sta,
			    &fops_htt_peer_stats);

	if (test_bit(WMI_TLV_SERVICE_PER_PEER_HTT_STATS_RESET,
		     ar->ab->wmi_ab.svc_map))
		debugfs_create_file("htt_peer_stats_reset", 0600, dir, link_sta,
				    &fops_htt_peer_stats_reset);

	if (ath12k_dp_qos_msduq_stats_enabled(&ar->dp))
		debugfs_create_file("qos_msduq", 0400, dir, link_sta->sta,
				    &fops_qos_msduq);

	if (ath12k_dp_scs_stats_enabled(&ar->dp))
		debugfs_create_file("scs", 0400, dir, link_sta->sta,
				    &fops_scs);

	if (ath12k_dp_fetch_reo_stats_enabled(&ar->dp))
		debugfs_create_file("fetch_reo_ctx", 0200, dir, link_sta->sta,
				    &fops_fetch_reo_ctx);

#ifdef CPTCFG_ATH12K_CFR
	if (test_bit(WMI_TLV_SERVICE_CFR_CAPTURE_SUPPORT,
		     ar->ab->wmi_ab.svc_map))
	       debugfs_create_file("cfr_capture", 0400, dir, link_sta,
				   &fops_peer_cfr_capture);
#endif/* CPTCFG_ATH12K_CFR */

}
EXPORT_SYMBOL(ath12k_debugfs_link_sta_op_add);
