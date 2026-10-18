/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "wlan_mlo_mgr_main.h"
#include "qdf_types.h"
#include "wlan_cmn.h"
#include "wlan_mlo_mgr_peer.h"
#include <wlan_mlo_mgr_ap.h>
#include <wlan_mlo_mgr_setup.h>
#include <wlan_utility.h>
#include <wlan_reg_services_api.h>
#include <wlan_mlo_mgr_sta.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_mgmt_txrx_rx_reo_utils_api.h>
/**
 * struct mlpeer_data: PSOC peers MLO data
 * @total_rssi:  sum of RSSI of all ML peers
 * @num_ml_peers: Number of ML peer's with this PSOC as TQM
 * @max_ml_peers: Max ML peers can have this PSOC as TQM
 *                (it is to distribute peers across all PSOCs)
 * @num_non_ml_peers: Non MLO peers of this PSOC
 */
struct mlpeer_data {
	int32_t total_rssi;
	uint16_t num_ml_peers;
	uint16_t max_ml_peers;
	uint16_t num_non_ml_peers;
};

/**
 * struct mlo_all_link_rssi: structure to collect TQM params for all PSOCs
 * @psoc_tqm_parms:  It collects peer data for all PSOCs
 * @num_psocs:       Number of PSOCs in the system
 * @current_psoc_id: current psoc id, it is for iterator
 * @curr_mlo_peer_id: current mlo peer id, it is for iterator
 */
struct mlo_all_link_rssi {
	struct mlpeer_data psoc_tqm_parms[WLAN_OBJMGR_MAX_DEVICES];
	uint8_t num_psocs;
	uint8_t current_psoc_id;
	uint32_t curr_mlo_peer_id;
};

/* Invalid TQM/PSOC ID */
#define ML_INVALID_PRIMARY_TQM   0xff
/* Congestion value */
#define ML_PRIMARY_TQM_CONGESTION 30
/* PTQM migration timeout value in ms */
#define ML_PRIMARY_TQM_MIGRATRION_TIMEOUT 4000
/* Link ID used for WDS Bridge*/
#define WDS_BRIDGE_VDEV_LINK_ID (WLAN_LINK_ID_INVALID - 1)

static void wlan_mlo_peer_get_rssi(struct wlan_objmgr_psoc *psoc,
				   void *obj, void *args)
{
	struct wlan_mlo_peer_context *mlo_peer_ctx;
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
	struct mlo_all_link_rssi *rssi_data = (struct mlo_all_link_rssi *)args;
	struct mlpeer_data *tqm_params = NULL;
	uint8_t index;

	mlo_peer_ctx = peer->mlo_peer_ctx;
	index = rssi_data->current_psoc_id;
	tqm_params = &rssi_data->psoc_tqm_parms[index];

	if (!wlan_peer_is_mlo(peer) && !mlo_peer_ctx) {
		if (wlan_peer_get_peer_type(peer) == WLAN_PEER_STA)
			tqm_params->num_non_ml_peers += 1;
		return;
	}

	if (!mlo_peer_ctx)
		return;

	/* If this psoc is new primary UMAC after migration,
	 * account RSSI on new link
	 */
	if (mlo_peer_ctx->migrate_primary_umac_psoc_id ==
			rssi_data->current_psoc_id) {
		tqm_params->total_rssi += mlo_peer_ctx->avg_link_rssi;
		tqm_params->num_ml_peers += 1;
		return;
	}

	/* If this psoc is not primary UMAC or if TQM migration is happening
	 * from current primary psoc, don't account RSSI
	 */
	if (mlo_peer_ctx->primary_umac_psoc_id == rssi_data->current_psoc_id &&
	    mlo_peer_ctx->migrate_primary_umac_psoc_id ==
	    ML_INVALID_PRIMARY_TQM &&
	    mlo_peer_ctx->mlo_peer_id != rssi_data->curr_mlo_peer_id) {
		tqm_params->total_rssi += mlo_peer_ctx->avg_link_rssi;
		tqm_params->num_ml_peers += 1;
	}
}

static void wlan_get_rssi_data_each_psoc(struct wlan_objmgr_psoc *psoc,
					 void *arg, uint8_t index)
{
	struct mlo_all_link_rssi *rssi_data = (struct mlo_all_link_rssi *)arg;
	struct mlpeer_data *tqm_params = NULL;

	tqm_params = &rssi_data->psoc_tqm_parms[index];

	tqm_params->total_rssi = 0;
	tqm_params->num_ml_peers = 0;
	tqm_params->num_non_ml_peers = 0;
	tqm_params->max_ml_peers = MAX_MLO_PEER;

	rssi_data->current_psoc_id = index;
	rssi_data->num_psocs++;

	wlan_objmgr_iterate_obj_list(psoc, WLAN_PEER_OP,
				     wlan_mlo_peer_get_rssi, rssi_data, 0,
				     WLAN_MLO_MGR_ID);
}

static QDF_STATUS mld_get_link_rssi(struct mlo_all_link_rssi *rssi_data)
{
	rssi_data->num_psocs = 0;

	wlan_objmgr_iterate_psoc_list(wlan_get_rssi_data_each_psoc,
				      rssi_data, WLAN_MLO_MGR_ID);

	return QDF_STATUS_SUCCESS;
}

uint8_t
wlan_mld_get_best_primary_umac_w_rssi(struct wlan_mlo_peer_context *ml_peer,
				      struct wlan_objmgr_vdev *link_vdevs[],
				      bool allow_all_links,
				      const struct mlo_all_link_rssi *rssi_data)
{
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	struct wlan_objmgr_peer *assoc_peer = NULL;
	uint8_t i;
	int32_t avg_rssi[WLAN_OBJMGR_MAX_DEVICES] = {0};
	int32_t diff_rssi[WLAN_OBJMGR_MAX_DEVICES] = {0};
	int32_t diff_low;
	bool mld_sta_links[WLAN_OBJMGR_MAX_DEVICES] = {0};
	bool mld_no_sta[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint8_t prim_link, id, prim_link_hi;
	uint8_t num_psocs;
	const struct mlpeer_data *tqm_params = NULL;
	struct wlan_channel *channel;
	enum phy_ch_width sec_hi_bw, hi_bw;
	uint8_t cong = ML_PRIMARY_TQM_CONGESTION;
	uint16_t mld_ml_sta_count[WLAN_OBJMGR_MAX_DEVICES] = {0};
	enum phy_ch_width mld_ch_width[WLAN_OBJMGR_MAX_DEVICES];
	uint8_t psoc_w_nosta;
	uint16_t ml_sta_count = 0;
	uint32_t total_cap, cap;
	uint16_t bw;
	bool group_full[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint16_t group_size[WLAN_OBJMGR_MAX_DEVICES] = {0};
	uint16_t grp_size = 0;
	uint16_t group_full_count = 0;
	struct wlan_mlo_link_peer_entry *peer_entry;

	for (i = 0; i < rssi_data->num_psocs; i++) {
		tqm_params = &rssi_data->psoc_tqm_parms[i];

		if (tqm_params->num_ml_peers)
			avg_rssi[i] = (tqm_params->total_rssi /
				       tqm_params->num_ml_peers);
	}

	/**
	 * If MLD STA associated to a set of links, choose primary UMAC
	 * from those links only
	 */
	num_psocs = 0;
	psoc_w_nosta = 0;
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++)
		mld_ch_width[i] = CH_WIDTH_INVALID;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!link_vdevs[i])
			continue;

		id = wlan_vdev_get_psoc_id(link_vdevs[i]);
		if (id >= WLAN_OBJMGR_MAX_DEVICES)
			continue;

		if (!allow_all_links && wlan_vdev_skip_pumac(link_vdevs[i])) {
			mlo_err("Skip Radio for Primary MLO umac");
			mld_sta_links[id] = false;
			continue;
		}

		tqm_params = &rssi_data->psoc_tqm_parms[id];
		mld_sta_links[id] = true;

		channel = wlan_vdev_mlme_get_bss_chan(link_vdevs[i]);
		mld_ch_width[id] = channel->ch_width;

		if ((tqm_params->num_ml_peers +
		     tqm_params->num_non_ml_peers) == 0) {
			/* If this PSOC has no stations */
			mld_no_sta[id] = true;
			psoc_w_nosta++;
		}

		mld_ml_sta_count[id] = tqm_params->num_ml_peers;
		/* Update total MLO STA count */
		ml_sta_count += tqm_params->num_ml_peers;

		num_psocs++;

		/* If no stations are associated, derive diff rssi
		 * based on psoc id {0-20, 20-40, 40 } so that
		 * stations are distributed across TQMs
		 */
		if (!avg_rssi[id]) {
			diff_rssi[id] = (id * 20);
			continue;
		}
		diff_rssi[id] = (ml_peer->avg_link_rssi >= avg_rssi[id]) ?
				(ml_peer->avg_link_rssi - avg_rssi[id]) :
				(avg_rssi[id] - ml_peer->avg_link_rssi);

	}

	prim_link = ML_INVALID_PRIMARY_TQM;

	/* If one of the PSOCs doesn't have any station select that PSOC as
	 * primary TQM. If more than one PSOC have no stations as Primary TQM
	 * the vdev with less bw needs to be selected as Primary TQM
	 */
	if (psoc_w_nosta == 1) {
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			if (mld_no_sta[i]) {
				prim_link = i;
				break;
			}
		}
	} else if (psoc_w_nosta > 1) {
		hi_bw = CH_WIDTH_INVALID;
		sec_hi_bw = CH_WIDTH_INVALID;
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			if (!mld_no_sta[i])
				continue;

			if (hi_bw == CH_WIDTH_INVALID) {
				prim_link_hi = i;
				hi_bw = mld_ch_width[i];
				continue;
			}
			/* if bw is 320MHZ mark it as highest ch width */
			if (mld_ch_width[i] == CH_WIDTH_320MHZ) {
				prim_link = prim_link_hi;
				sec_hi_bw = hi_bw;
				hi_bw = mld_ch_width[i];
				prim_link_hi = i;
			}
			/* If bw is less than or equal to 160 MHZ
			 * and chwidth is greater than than other link
			 * Mark this link as primary link
			 */
			if (mld_ch_width[i] <= CH_WIDTH_160MHZ) {
				if (hi_bw < mld_ch_width[i]) {
					/* move high bw to second high bw */
					prim_link = prim_link_hi;
					sec_hi_bw = hi_bw;

					hi_bw = mld_ch_width[i];
					prim_link_hi = i;
				} else if ((sec_hi_bw == CH_WIDTH_INVALID) ||
					   (sec_hi_bw < mld_ch_width[i])) {
					/* update sec high bw */
					sec_hi_bw = mld_ch_width[i];
					prim_link = i;
				}
			}
		}

		if (mlo_ctx && mlo_ctx->mlo_override_mlsr_ptqm) {
			/*
			 * Check if highest bandwidth is to be
			 * forced for the given peer.
			 */
			peer_entry = &ml_peer->peer_list[0];
			if (peer_entry->link_peer)
				assoc_peer =
					peer_entry->link_peer;

			if (assoc_peer &&
			    wlan_peer_mlme_flag_ext_get(assoc_peer,
				WLAN_PEER_FEXT_PRIMARY_UMAC_HI)) {
				prim_link = prim_link_hi;
			}
		}
	} else {
		total_cap = 0;
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			bw = wlan_reg_get_bw_value(mld_ch_width[i]);
			total_cap += bw * (100 - cong);
		}

		group_full_count = 0;
		for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
			if (!mld_sta_links[i])
				continue;

			bw = wlan_reg_get_bw_value(mld_ch_width[i]);
			cap = bw * (100 - cong);
			ml_sta_count++;
			grp_size = (ml_sta_count) * ((cap * 100) / total_cap);
			group_size[i] = grp_size / 100;
			if (grp_size % 100)
				group_size[i]++;

			if (group_size[i] == 0)
				group_size[i] = 1;

			if (group_size[i] <=  mld_ml_sta_count[i]) {
				group_full[i] = true;
				group_full_count++;
			}
		}

		if ((num_psocs - group_full_count) == 1) {
			for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
				if (!mld_sta_links[i])
					continue;

				if (group_full[i])
					continue;

				prim_link = i;
				break;
			}
		} else {
			diff_low = 0;
			/* find min diff, based on it, allocate primary umac */
			for (i = 0; i < WLAN_OBJMGR_MAX_DEVICES; i++) {
				if (!mld_sta_links[i])
					continue;

				/* First iteration */
				if (diff_low == 0) {
					diff_low = diff_rssi[i];
					prim_link = i;
				} else if (diff_low > diff_rssi[i]) {
					diff_low = diff_rssi[i];
					prim_link = i;
				}
			}
		}
	}

	if (prim_link != ML_INVALID_PRIMARY_TQM)
		return prim_link;

	/* If primary link id is not found, return id of 1st available link */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!link_vdevs[i])
			continue;

		if (!allow_all_links && wlan_vdev_skip_pumac(link_vdevs[i])) {
			mlo_debug("Skip Radio for Primary MLO umac");
			continue;
		}
		id = wlan_vdev_get_psoc_id(link_vdevs[i]);
		if (id >= WLAN_OBJMGR_MAX_DEVICES)
			continue;

		return wlan_vdev_get_psoc_id(link_vdevs[i]);
	}

	return ML_INVALID_PRIMARY_TQM;
}

void mlo_peer_assign_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_mlo_link_peer_entry *peer_entry)
{
	struct wlan_mlo_link_peer_entry *peer_ent_iter;
	uint8_t i;
	uint8_t primary_umac_set = 0;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
	bool is_central_primary = false;
	uint8_t bridge_umac_id = -1;
	uint8_t link_peer_psoc_id;
	struct wlan_mlo_dev_context *ml_dev = NULL;
#endif

	/* If MLD is within single SOC, then assoc link becomes
	 * primary umac
	 */
	if (ml_peer->primary_umac_psoc_id == ML_PRIMARY_UMAC_ID_INVAL) {
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
		ml_dev = ml_peer->ml_dev;

		if (!ml_dev) {
			mlo_err("ML dev ctx is NULL");
			return;
		}
		if (ml_dev->bridge_sta_ctx) {
			is_central_primary = ml_dev->bridge_sta_ctx->is_force_central_primary;
			bridge_umac_id = ml_dev->bridge_sta_ctx->bridge_umac_id;
		}
		link_peer_psoc_id = wlan_peer_get_psoc_id(peer_entry->link_peer);
		if (is_central_primary) {
			if (link_peer_psoc_id == bridge_umac_id) {
				peer_entry->is_primary = true;
				ml_peer->primary_umac_psoc_id = bridge_umac_id;
			} else {
				peer_entry->is_primary = false;
				ml_peer->primary_umac_psoc_id = bridge_umac_id;
			}

		} else {

#endif
			if (wlan_peer_mlme_is_assoc_peer(peer_entry->link_peer)) {
				peer_entry->is_primary = true;
				ml_peer->primary_umac_psoc_id =
					wlan_peer_get_psoc_id(peer_entry->link_peer);
			} else {
				peer_entry->is_primary = false;
			}

#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
		}
#endif
	} else {
		if ((wlan_peer_mlme_is_assoc_peer(peer_entry->link_peer)) &&
		    (ml_peer->max_links > 1) &&
		    (mlo_ctx->force_non_assoc_prim_umac)) {
			peer_entry->is_primary = false;
			return;
		}

		/* If this peer PSOC is not derived as Primary PSOC,
		 * mark is_primary as false
		 */
		if (wlan_peer_get_psoc_id(peer_entry->link_peer) !=
				ml_peer->primary_umac_psoc_id) {
			peer_entry->is_primary = false;
			return;
		}

		/* For single SOC, check whether is_primary is set for
		 * other partner peer, then mark is_primary false for this peer
		 */
		for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
			peer_ent_iter = &ml_peer->peer_list[i];

			if (!peer_ent_iter->link_peer)
				continue;

			/* Check for other link peers */
			if (peer_ent_iter == peer_entry)
				continue;

			if (wlan_peer_get_psoc_id(peer_ent_iter->link_peer) !=
					ml_peer->primary_umac_psoc_id)
				continue;

			if (peer_ent_iter->is_primary)
				primary_umac_set = 1;
		}

		if (primary_umac_set)
			peer_entry->is_primary = false;
		else
			peer_entry->is_primary = true;
	}
}

static int8_t wlan_vdev_derive_link_rssi(struct wlan_objmgr_vdev *vdev,
					 struct wlan_objmgr_vdev *assoc_vdev,
					 int8_t rssi)
{
	struct wlan_channel *channel, *assoc_channel;
	uint16_t ch_freq, assoc_freq;
	uint8_t tx_pow, assoc_tx_pow;
	int8_t diff_txpow;
	struct wlan_objmgr_pdev *pdev, *assoc_pdev;
	uint8_t log10_freq;
	uint8_t derived_rssi;
	int16_t ten_derived_rssi;
	int8_t ten_diff_pl = 0;

	pdev = wlan_vdev_get_pdev(vdev);
	assoc_pdev = wlan_vdev_get_pdev(assoc_vdev);

	channel = wlan_vdev_get_active_channel(vdev);
	if (channel)
		ch_freq = channel->ch_freq;
	else
		ch_freq = 1;

	assoc_channel = wlan_vdev_get_active_channel(assoc_vdev);
	if (assoc_channel)
		assoc_freq = assoc_channel->ch_freq;
	else
		assoc_freq = 1;

	/*
	 *  diff of path loss (of two links) = log10(freq1) - log10(freq2)
	 *                       (since distance is constant)
	 *  since log10 is not available, we cameup with approximate ranges
	 */
	log10_freq = (ch_freq * 10) / assoc_freq;
	if ((log10_freq >= 20) && (log10_freq < 30))
		ten_diff_pl = 4;  /* 0.4 *10 */
	else if ((log10_freq >= 11) && (log10_freq < 20))
		ten_diff_pl = 1;  /* 0.1 *10 */
	else if ((log10_freq >= 8) && (log10_freq < 11))
		ten_diff_pl = 0; /* 0 *10 */
	else if ((log10_freq >= 4) && (log10_freq < 8))
		ten_diff_pl = -1; /* -0.1 * 10 */
	else if ((log10_freq >= 1) && (log10_freq < 4))
		ten_diff_pl = -4;  /* -0.4 * 10 */

	assoc_tx_pow = wlan_reg_get_channel_reg_power_for_freq(assoc_pdev,
							       assoc_freq);
	tx_pow = wlan_reg_get_channel_reg_power_for_freq(pdev, ch_freq);

	diff_txpow = tx_pow -  assoc_tx_pow;

	ten_derived_rssi = (diff_txpow * 10) - ten_diff_pl + (rssi * 10);
	derived_rssi = ten_derived_rssi / 10;

	return derived_rssi;
}

static void mlo_peer_calculate_avg_rssi(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer,
		int8_t rssi,
		struct wlan_objmgr_vdev *assoc_vdev)
{
	int32_t total_rssi = 0;
	uint8_t num_psocs = 0;
	uint8_t i;
	struct wlan_objmgr_vdev *vdev;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev = ml_dev->wlan_vdev_list[i];
		if (!vdev)
			continue;

		num_psocs++;
		if (vdev == assoc_vdev)
			total_rssi += rssi;
		else
			total_rssi += wlan_vdev_derive_link_rssi(vdev,
								 assoc_vdev,
								 rssi);
	}

	if (!num_psocs)
		return;

	ml_peer->avg_link_rssi = total_rssi / num_psocs;
}

#ifdef WLAN_MLO_MULTI_CHIP
int8_t mlo_get_central_umac_id(
		uint8_t *psoc_ids)
{
	uint8_t prim_psoc_id = -1;
	uint8_t adjacent = 0;

	/* Some 3 link RDPs have restriction on the primary umac.
	 * Only the link that is adjacent to both the links can be
	 * a primary umac.
	 * Note: it means umac migration is also restricted.
	 */
	mlo_chip_adjacent(psoc_ids[0], psoc_ids[1], &adjacent);
	if (!adjacent) {
		prim_psoc_id = psoc_ids[2];
	} else {
		mlo_chip_adjacent(psoc_ids[0], psoc_ids[2], &adjacent);
		if (!adjacent) {
			prim_psoc_id = psoc_ids[1];
		} else {
			/* If all links are adjacent to each other,
			 * no need to restrict the primary umac.
			 * return failure the caller will handle.
			 */
			mlo_chip_adjacent(psoc_ids[1], psoc_ids[2],
					  &adjacent);
			if (!adjacent)
				prim_psoc_id = psoc_ids[0];
			else
				return prim_psoc_id;
		}
	}

	return prim_psoc_id;
}

QDF_STATUS mlo_check_topology(struct wlan_objmgr_pdev *pdev,
			      struct wlan_objmgr_vdev *vdev,
			      uint8_t aplinks)
{
	struct wlan_mlo_dev_context *ml_dev = vdev->mlo_dev_ctx;
	struct wlan_objmgr_vdev *vdev_iter = NULL;
	struct wlan_objmgr_vdev *tmp_vdev = NULL;
	uint8_t psoc_ids[WLAN_UMAC_MLO_MAX_VDEVS];
	uint8_t i, idx = 0;
	uint8_t bridge_umac;
	uint8_t adjacent = -1;
	uint8_t max_soc;
	uint8_t link_id;
	bool is_mlo_vdev;

	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	/* Do topology check for STA mode for other modes return Success */
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)
		return QDF_STATUS_SUCCESS;

	max_soc = mlo_get_total_links(pdev);

	if (max_soc != WLAN_UMAC_MLO_MAX_VDEVS) {
		/* For Devices which has no topology dependency return Success */
		return QDF_STATUS_SUCCESS;
	}

	if (!ml_dev->bridge_sta_ctx) {
		mlo_err("Bridge STA context Null");
		return QDF_STATUS_E_FAILURE;
	}

	/* Incase of 4-LINK RDP in 3-LINK NON-AP MLD mode there is
	 * restriction to have the primary umac as central in topology.
	 * Note: It also means restriction on umac migration
	 */
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		vdev_iter = vdev->mlo_dev_ctx->wlan_vdev_list[i];
		if (!vdev_iter)
			continue;
		/* Store the psoc_ids of the links */
		psoc_ids[idx] = wlan_vdev_get_psoc_id(vdev_iter);
		idx++;
	}
	/* If number of links in AP are greater or equal to STA */
	if (aplinks >= idx) {
		/* Station has 2 links enabled */
		/* Check if the primary umac and assoc links can be different*/
		if (idx == (WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY - 1)) {
			mlo_chip_adjacent(psoc_ids[0], psoc_ids[1], &adjacent);
			if (adjacent == 1) {
				mlo_info("pri umac & assoc link can be diff as chips are adj");
				return QDF_STATUS_SUCCESS;
			} else {
				mlo_info("pri umac & assoc link can be diff but need bridge");
				return QDF_STATUS_E_FAILURE;
			}
		}
		/* Check if the primary umac and assoc links are same for 3 link sta*/
		if (idx == WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY) {
			bridge_umac = mlo_get_central_umac_id(psoc_ids);

			tmp_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev,
								  bridge_umac,
								  false);

			if (!tmp_vdev)
				return QDF_STATUS_E_FAILURE;

			link_id = tmp_vdev->vdev_mlme.mlo_link_id;
			if (bridge_umac != -1) {
				if (wlan_vdev_get_psoc_id(vdev) != bridge_umac) {
					mlo_err("Central LINK %d Force central as primary umac!! ",
						bridge_umac);
					tmp_vdev->vdev_objmgr.mlo_central_vdev = true;
					ml_dev->bridge_sta_ctx->is_force_central_primary = true;
					ml_dev->bridge_sta_ctx->bridge_umac_id = bridge_umac;
					ml_dev->bridge_sta_ctx->bridge_link_id = link_id;
					wlan_objmgr_vdev_release_ref(tmp_vdev, WLAN_MLO_MGR_ID);
					return QDF_STATUS_SUCCESS;
				}
			}
		}
	} else {
		/* If # of links in AP < then link on Station check for bridge vap */
		/* Check case when AP MLD is 2 link and NON-AP MLD is 3 link capable*/
		if (idx == WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY &&
		    (aplinks == (WLAN_UMAC_MLO_MAX_PSOC_TOPOLOGY - 1))) {
			bridge_umac = mlo_get_central_umac_id(psoc_ids);
			tmp_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev,
								  bridge_umac,
								  false);

			if (!tmp_vdev)
				return QDF_STATUS_E_FAILURE;

			link_id = tmp_vdev->vdev_mlme.mlo_link_id;
			if (bridge_umac != -1) {
				if (wlan_vdev_get_psoc_id(vdev) != bridge_umac) {
					is_mlo_vdev = wlan_vdev_mlme_is_mlo_vdev(tmp_vdev);
					if (is_mlo_vdev) {
						mlo_err("Central Link %d partipating in Assoc!! ",
							bridge_umac);
					} else {
						mlo_err("Central %d not part of Assoc create bridge!!",
							bridge_umac);
						tmp_vdev->vdev_objmgr.mlo_central_vdev = true;
						ml_dev->bridge_sta_ctx->is_force_central_primary = true;
						ml_dev->bridge_sta_ctx->bridge_umac_id = bridge_umac;
						ml_dev->bridge_sta_ctx->bridge_vap_exists = true;
						ml_dev->bridge_sta_ctx->bridge_link_id = WDS_BRIDGE_VDEV_LINK_ID;
					}
				}
			}
		}
	}
	if (tmp_vdev)
		wlan_objmgr_vdev_release_ref(tmp_vdev, WLAN_MLO_MGR_ID);
	return QDF_STATUS_SUCCESS;
}

void mlo_update_partner_bridge_info(struct wlan_mlo_dev_context *ml_dev,
				    struct mlo_partner_info *partner_info)
{
	struct wlan_objmgr_vdev *bridge_vdev = NULL;
	uint8_t bridge_umac_id = -1;
	uint8_t bridge_index = partner_info->num_partner_links;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return;

	bridge_umac_id = ml_dev->bridge_sta_ctx->bridge_umac_id;
	bridge_vdev = mlo_get_link_vdev_from_psoc_id(ml_dev, bridge_umac_id, false);
	if (bridge_vdev) {
		partner_info->partner_link_info[bridge_index].link_id = bridge_vdev->vdev_mlme.mlo_link_id;
		qdf_mem_copy(&partner_info->partner_link_info[bridge_index].link_addr,
			     wlan_vdev_mlme_get_macaddr(bridge_vdev), sizeof(struct qdf_mac_addr));
		/* Account for bridge peer here */
		partner_info->num_partner_links++;
		wlan_objmgr_vdev_release_ref(bridge_vdev, WLAN_MLO_MGR_ID);
	}
}

bool mlo_is_sta_bridge_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev)
		return false;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return false;

	if (vdev->vdev_objmgr.mlo_central_vdev &&
	    ml_dev->bridge_sta_ctx->bridge_vap_exists)
		return true;

	return false;
}

qdf_export_symbol(mlo_is_sta_bridge_vdev);

bool mlo_sta_bridge_exists(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev)
		return false;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return false;

	if (ml_dev->bridge_sta_ctx->bridge_vap_exists)
		return true;

	return false;
}

qdf_export_symbol(mlo_sta_bridge_exists);

bool mlo_is_force_central_primary(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_mlo_dev_context *ml_dev = NULL;

	if (!vdev)
		return false;

	ml_dev = vdev->mlo_dev_ctx;

	if (!ml_dev || !ml_dev->bridge_sta_ctx)
		return false;

	if (ml_dev->bridge_sta_ctx->is_force_central_primary)
		return true;

	return false;
}

qdf_export_symbol(mlo_is_force_central_primary);

uint8_t mlo_get_total_links(struct wlan_objmgr_pdev *pdev)
{
	uint8_t ml_grp_id;

	ml_grp_id = wlan_get_mlo_grp_id_from_pdev(pdev);
	return mlo_setup_get_total_socs(ml_grp_id);
}

static QDF_STATUS mlo_set_3_link_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[])
{
	uint8_t psoc_ids[WLAN_UMAC_MLO_MAX_VDEVS];
	int8_t central_umac_id;
	uint8_t i;

	if (ml_peer->max_links != 3)
		return QDF_STATUS_E_FAILURE;

	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (!link_vdevs[i] && (i < ml_peer->max_links))
			return QDF_STATUS_E_FAILURE;
	}

	/* Some 3 link RDPs have restriction on the primary umac.
	 * Only the link that is adjacent to both the links can be
	 * a primary umac.
	 * Note: it means umac migration is also restricted.
	 */
	psoc_ids[0] = wlan_vdev_get_psoc_id(link_vdevs[0]);
	psoc_ids[1] = wlan_vdev_get_psoc_id(link_vdevs[1]);
	psoc_ids[2] = wlan_vdev_get_psoc_id(link_vdevs[2]);

	central_umac_id = mlo_get_central_umac_id(psoc_ids);
	if (central_umac_id != -1)
		ml_peer->primary_umac_psoc_id = central_umac_id;
	else
		return QDF_STATUS_E_FAILURE;

	mlo_peer_assign_primary_umac(ml_peer,
				     &ml_peer->peer_list[0]);

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS mlo_set_3_link_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[])
{
	return QDF_STATUS_E_FAILURE;
}

static inline
QDF_STATUS mlo_set_3_link_forced_primary_umac(
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[],
		uint8_t *psoc_id)
{
	return QDF_STATUS_E_FAILURE;
}
#endif

QDF_STATUS mlo_peer_allocate_primary_umac(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer,
		struct wlan_objmgr_vdev *link_vdevs[])
{
	struct wlan_mlo_link_peer_entry *peer_entry;
	struct wlan_objmgr_peer *assoc_peer = NULL;
	int32_t rssi;
	struct mlo_mgr_context *mlo_ctx = wlan_objmgr_get_mlo_ctx();
	uint8_t first_link_id = 0;
	bool primary_umac_set = false;
	uint8_t i, psoc_id;
	struct mlo_all_link_rssi rssi_data;

	peer_entry = &ml_peer->peer_list[0];
	assoc_peer = peer_entry->link_peer;
	if (!assoc_peer)
		return QDF_STATUS_E_FAILURE;

	/* Select assoc peer's PSOC as primary UMAC in Multi-chip solution,
	 * 1) for single link MLO connection
	 * 2) if MLD is single chip MLO
	 */
	if ((mlo_ctx->force_non_assoc_prim_umac) &&
	    (ml_peer->max_links >= 1)) {
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!link_vdevs[i])
				continue;

			if (wlan_peer_get_vdev(assoc_peer) == link_vdevs[i])
				continue;
			psoc_id = wlan_vdev_get_psoc_id(link_vdevs[i]);
			ml_peer->primary_umac_psoc_id = psoc_id;
			break;
		}

		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT
			 " primary umac soc %d ", ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if ((ml_peer->max_links == 1) ||
	    (mlo_vdevs_check_single_soc(link_vdevs, ml_peer->max_links))) {
		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d Assoc peer " QDF_MAC_ADDR_FMT
			 " primary umac soc %d ", ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if (mlo_set_3_link_primary_umac(ml_peer, link_vdevs) ==
	    QDF_STATUS_SUCCESS) {
		/* If success then the primary umac is restricted and assigned.
		 * if not, there is no restriction, so just fallthrough
		 */
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT
			 " center primary umac soc %d ",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if (mlo_ctx->mlo_is_force_primary_umac) {
		for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
			if (!link_vdevs[i])
				continue;

			psoc_id = wlan_vdev_get_psoc_id(link_vdevs[i]);
			if (!first_link_id)
				first_link_id = psoc_id;

			if (psoc_id == mlo_ctx->mlo_forced_primary_umac_id) {
				ml_peer->primary_umac_psoc_id = psoc_id;
				primary_umac_set = true;
				break;
			}
		}

		if (!primary_umac_set)
			ml_peer->primary_umac_psoc_id = first_link_id;

		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " primary umac soc %d ",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);

		return QDF_STATUS_SUCCESS;
	}

	if (mlo_set_3_link_forced_primary_umac(ml_peer, link_vdevs, &psoc_id) ==
	    QDF_STATUS_SUCCESS) {
		/* If success then the primary umac is restricted and assigned.
		 * if not, there is no restriction, so just fallthrough
		 */
		ml_peer->primary_umac_psoc_id = psoc_id;
		mlo_peer_assign_primary_umac(ml_peer, peer_entry);
		mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT
			 " forced primary umac soc %d ",
			 ml_dev->mld_id,
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
			 ml_peer->primary_umac_psoc_id);
		return QDF_STATUS_SUCCESS;
	}

	rssi = wlan_peer_get_rssi(assoc_peer);
	mlo_peer_calculate_avg_rssi(ml_dev, ml_peer, rssi,
				    wlan_peer_get_vdev(assoc_peer));

	mld_get_link_rssi(&rssi_data);
	ml_peer->primary_umac_psoc_id =
		wlan_mld_get_best_primary_umac_w_rssi(ml_peer, link_vdevs,
						      false, &rssi_data);

	mlo_peer_assign_primary_umac(ml_peer, peer_entry);

	mlo_info("MLD ID %d ML Peer " QDF_MAC_ADDR_FMT " avg RSSI %d primary umac soc %d ",
		 ml_dev->mld_id,
		 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		 ml_peer->avg_link_rssi, ml_peer->primary_umac_psoc_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlo_peer_free_primary_umac(
		struct wlan_mlo_dev_context *ml_dev,
		struct wlan_mlo_peer_context *ml_peer)
{
	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_PRIMARY_LINK_MIGRATE
void wlan_objmgr_mlo_update_primary_info(struct wlan_objmgr_peer *peer)
{
	struct wlan_mlo_peer_context *ml_peer = NULL;
	struct wlan_mlo_link_peer_entry *peer_ent_iter;
	uint8_t i;

	ml_peer = peer->mlo_peer_ctx;
	wlan_mlo_peer_wsi_link_delete(ml_peer);
	ml_peer->primary_umac_psoc_id = wlan_peer_get_psoc_id(peer);

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_ent_iter = &ml_peer->peer_list[i];

		if (!peer_ent_iter->link_peer)
			continue;

		if (peer_ent_iter->is_primary)
			peer_ent_iter->is_primary = false;

		if (peer_ent_iter->link_peer == peer)
			peer_ent_iter->is_primary = true;
	}
	wlan_mlo_peer_wsi_link_add(ml_peer);
}

qdf_export_symbol(wlan_objmgr_mlo_update_primary_info);

void mlo_mlme_ptqm_migrate_timer_cb(void *arg)
{
	struct wlan_mlo_dev_context *ml_dev = (struct wlan_mlo_dev_context *)arg;
	struct wlan_mlo_peer_context *ml_peer = NULL;
	uint16_t i = 0;

	if (!ml_dev)
		return;

	/* Check for pending bitmaps and issue disconnect */
	for (i = 0; i < MAX_MLO_PEER_ID; i++) {
		if (qdf_test_bit(i, ml_dev->mlo_peer_id_bmap)) {
			ml_peer = wlan_mlo_get_mlpeer_by_ml_peerid(ml_dev, i);
			if (ml_peer && ml_peer->primary_umac_migration_in_progress) {
				ml_peer->primary_umac_migration_in_progress = false;
				wlan_ptqm_peer_migrate_completion
					(ml_dev, ml_peer,
					 PRIMARY_LINK_PEER_MIGRATION_FAIL);
				mlo_err("Issue disconnect for ml peer with ml peer id:%d", i);
				wlan_mlo_peer_deauth_init(ml_peer,
							  NULL, 0);
			}
			qdf_clear_bit(i, ml_dev->mlo_peer_id_bmap);
		}
	}
}

/**
 * mlo_ptqm_list_peek_head() - Returns the head of linked list
 *
 * @ptqm_list: Pointer to the list of peer ptqm migrate entries
 *
 * API to retrieve the head from the list of peer ptqm migrate entries
 *
 * Return: Pointer to peer ptqm migrate entry
 */
static
struct peer_ptqm_migrate_list_entry *mlo_ptqm_list_peek_head(
					qdf_list_t *ptqm_list)
{
	struct peer_ptqm_migrate_list_entry *peer_entry;
	qdf_list_node_t *peer_node = NULL;

	if (qdf_list_peek_front(ptqm_list, &peer_node) != QDF_STATUS_SUCCESS)
		return NULL;

	peer_entry = qdf_container_of(peer_node,
				      struct peer_ptqm_migrate_list_entry,
				      node);

	return peer_entry;
}

/**
 * mlo_get_next_peer_ctx() - Return next peer ptqm entry from the list
 *
 * @peer_list:  Pointer to the list of peer ptqm migrate entries
 * @peer_cur: Pointer to the current peer ptqm entry
 *
 * API to retrieve the next node from the list of peer ptqm migrate entries
 *
 * Return: Pointer to peer ptqm migrate entry
 */
static
struct peer_ptqm_migrate_list_entry *mlo_get_next_peer_ctx(
				qdf_list_t *peer_list,
				struct peer_ptqm_migrate_list_entry *peer_cur)
{
	struct peer_ptqm_migrate_list_entry *peer_next;
	qdf_list_node_t *node = &peer_cur->node;
	qdf_list_node_t *next_node = NULL;

	/* This API is invoked with lock acquired, do not add log prints */
	if (!node)
		return NULL;

	if (qdf_list_peek_next(peer_list, node, &next_node) !=
						QDF_STATUS_SUCCESS)
		return NULL;

	peer_next = qdf_container_of(next_node,
				     struct peer_ptqm_migrate_list_entry,
				     node);
	return peer_next;
}

/**
 * wlan_mlo_send_ptqm_migrate_cmd() - API to send WMI to trigger ptqm migration
 * @vdev: objmgr vdev object
 * @list: peer list to be migrated
 * @num_peers_failed: number of peers for which wmi cmd is failed.
 * This value is expected to be used only in case failure is returned by WMI
 *
 * API to send WMI to trigger ptqm migration
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_send_ptqm_migrate_cmd(struct wlan_objmgr_vdev *vdev,
			       struct peer_migrate_ptqm_multi_entries *list,
			       uint16_t *num_peers_failed)
{
	struct wlan_lmac_if_mlo_tx_ops *mlo_tx_ops;
	struct wlan_objmgr_psoc *psoc;
	QDF_STATUS status;
	struct peer_ptqm_migrate_params param = {0};
	struct peer_ptqm_migrate_entry *peer_list = NULL;
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;
	struct wlan_mlo_dev_context *ml_dev = NULL;
	uint16_t i = 0;

	ml_dev = vdev->mlo_dev_ctx;
	if (!ml_dev)
		return QDF_STATUS_E_FAILURE;

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		mlo_err("null psoc");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_tx_ops = &psoc->soc_cb.tx_ops->mlo_ops;
	if (!mlo_tx_ops || !mlo_tx_ops->peer_ptqm_migrate_send) {
		mlo_err("mlo_tx_ops is null!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	param.vdev_id = wlan_vdev_get_id(vdev);
	param.num_peers = list->num_entries;

	param.peer_list = qdf_mem_malloc(sizeof(struct peer_ptqm_migrate_entry) *
					 list->num_entries);
	if (!param.peer_list) {
		mlo_err("Failed to allocate memory for ptqm migration command");
		return QDF_STATUS_E_FAILURE;
	}

	peer_list = param.peer_list;

	peer_entry = mlo_ptqm_list_peek_head(&list->peer_list);
	while (peer_entry) {
		peer_list[i].ml_peer_id = peer_entry->mlo_peer_id;
		peer_list[i].hw_link_id = peer_entry->new_hw_link_id;

		qdf_set_bit(peer_entry->mlo_peer_id,
			    ml_dev->mlo_peer_id_bmap);

		mlo_debug("idx:%d, ml_peer_id:%d, hw_link_id:%d",
			  i, peer_list[i].ml_peer_id,
			  peer_list[i].hw_link_id);
		i++;
		next_entry = mlo_get_next_peer_ctx(&list->peer_list,
						   peer_entry);
		peer_entry = next_entry;
	}

	status = mlo_tx_ops->peer_ptqm_migrate_send(vdev, &param);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Failed to send WMI for ptqm migration");
		*num_peers_failed = param.num_peers_failed;
		qdf_mem_free(param.peer_list);
		return QDF_STATUS_E_FAILURE;
	}

	/* Set timeout equal to peer delete timeout as requested by FW.
	 * Timeout value to be optimized later. Timeout value will be
	 * updated later based on stress testings.
	 */
	qdf_timer_mod(&ml_dev->ptqm_migrate_timer,
		      ML_PRIMARY_TQM_MIGRATRION_TIMEOUT);

	qdf_mem_free(param.peer_list);
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_mlo_get_new_ptqm_id() - API to get new ptqm ID
 * @curr_vdev: objmgr vdev object for current primary link
 * @ml_peer: ml peer object
 * @new_primary_link_id: new primary link id
 * @new_hw_link_id: hw link id for new primary TQM
 * @force_mig: allow migration to vdevs which are disabled to be pumac
 * using primary_umac_skip ini
 * @allow_reelection: Allow reelection of current primary link
 * @rssi_data: RSSI data of all the HW links
 *
 * API to get new ptqm ID
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_get_new_ptqm_id(struct wlan_objmgr_vdev *curr_vdev,
			 struct wlan_mlo_peer_context *ml_peer,
			 uint8_t new_primary_link_id,
			 uint16_t *new_hw_link_id,
			 bool force_mig, bool allow_reelection,
			 const struct mlo_all_link_rssi *rssi_data)
{
	uint8_t current_primary_link_id = WLAN_LINK_ID_INVALID;
	struct wlan_objmgr_vdev *tmp_vdev = NULL;
	struct wlan_objmgr_vdev *wlan_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_objmgr_vdev *tmp_vdev_list[WLAN_UMAC_MLO_MAX_VDEVS] = { NULL };
	struct wlan_mlo_link_peer_entry *peer_entry;
	uint8_t psoc_ids[WLAN_UMAC_MLO_MAX_VDEVS];
	struct wlan_objmgr_vdev *link_vdev = NULL;
	struct wlan_objmgr_peer *curr_peer = NULL;
	QDF_STATUS status;
	uint8_t i = 0, idx = 0, j = 0, tmp_cnt = 0;

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry || !peer_entry->link_peer)
			continue;

		if (peer_entry->is_primary) {
			curr_peer = peer_entry->link_peer;
			break;
		}
	}

	if (!curr_peer) {
		mlo_err("ML peer " QDF_MAC_ADDR_FMT " current primary link not found",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (wlan_vdev_mlme_get_opmode(curr_vdev) == QDF_SAP_MODE &&
	    wlan_peer_mlme_get_state(curr_peer) != WLAN_CONNECTED_STATE) {
		mlo_err("ML peer " QDF_MAC_ADDR_FMT " is not authorized",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	*new_hw_link_id = INVALID_HW_LINK_ID;
	current_primary_link_id =
		wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
	if (current_primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_err("ML peer " QDF_MAC_ADDR_FMT "current primary link id is invalid",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (current_primary_link_id == new_primary_link_id) {
		mlo_err("current and requested link_id are same");
		return QDF_STATUS_E_INVAL;
	}

	if (new_primary_link_id != WLAN_LINK_ID_INVALID) {
		link_vdev = mlo_get_vdev_by_link_id(curr_vdev,
						    new_primary_link_id,
						    WLAN_MLO_MGR_ID);
		if (!link_vdev) {
			mlo_err("links vdev not found for link id %d",
				new_primary_link_id);
			return QDF_STATUS_E_INVAL;
		}

		if (wlan_vdev_read_skip_pumac_cnt(link_vdev) > 0) {
			mlo_err("Selected new ptqm link not allowed for migration");
			mlo_release_vdev_ref(link_vdev);
			return QDF_STATUS_E_PERM;
		}
		mlo_release_vdev_ref(link_vdev);
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry || !peer_entry->link_peer)
			continue;

		if (wlan_peer_get_peer_type(peer_entry->link_peer) ==
					WLAN_PEER_MLO_BRIDGE)
			goto exit;

		psoc_ids[j++] = wlan_vdev_get_psoc_id(
				wlan_peer_get_vdev(peer_entry->link_peer));
	}

	/* For some 3 link RDPs, there is restriction on primary umac */
	if (j == 3) {
		if (mlo_get_central_umac_id(psoc_ids) != -1) {
			mlo_err("ML peer " QDF_MAC_ADDR_FMT "migration not supported",
				QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
			goto exit;
		}
	}

	for (i = 0; i < MAX_MLO_LINK_PEERS; i++) {
		peer_entry = &ml_peer->peer_list[i];
		if (!peer_entry || !peer_entry->link_peer)
			continue;

		if (wlan_peer_read_skip_pumac_cnt(peer_entry->link_peer) > 0) {
			mlo_debug("Peer with link %d is not allowed as primary",
				  peer_entry->link_ix);
			continue;
		}

		tmp_vdev = wlan_peer_get_vdev(peer_entry->link_peer);
		if (!tmp_vdev)
			continue;

		if (!allow_reelection && tmp_vdev == curr_vdev)
			continue;

		status = wlan_objmgr_vdev_try_get_ref(tmp_vdev,
						      WLAN_MLME_SB_ID);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("failed to get vdev ref");
			continue;
		}

		if (wlan_vdev_read_skip_pumac_cnt(tmp_vdev) > 0) {
			mlo_debug("Vdev not allowed for migration, skip this vdev");
			wlan_objmgr_vdev_release_ref(tmp_vdev,
						     WLAN_MLME_SB_ID);
			continue;
		}

		/* Store vdevs which cannot be selected as primary in a temp
		 * list. force_mig flag will be used to allow migration to vdevs
		 * which are not allowed to be selected as primary by using the
		 * primary_umac_skip ini config. This will be helpful in scenarios
		 * where if the current primary link is going down and peer ptqm
		 * needs to be migrated but the partner links ofthat mld are
		 * the user disabled links for ptqm.
		 */
		if (wlan_vdev_skip_pumac(tmp_vdev)) {
			mlo_debug("Vdev cannot be selected as primary");
			tmp_vdev_list[tmp_cnt++] = tmp_vdev;
			continue;
		}

		mlo_debug("PUMAC candidate link %d", peer_entry->link_ix);

		wlan_vdev_list[idx++] = tmp_vdev;
	}

	if (new_primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_debug("Invalid link id provided, select new link id");
		/* If there are no vdevs present in wlan_vdev_list, means none
		 * of the partner vdevs of current MLD are eligible to become
		 * primary umac. In that case if user has requested force
		 * migration and there are some vdevs present in temp_vdev_list,
		 * select new primary umac from those vdev. If no vdevs are
		 * prenset in any of the list, return failure.
		 */
		if (idx == 0) {
			if (!force_mig || tmp_cnt == 0) {
				mlo_err("No link available to be selected as primary");
				goto exit;
			} else {
				ml_peer->migrate_primary_umac_psoc_id =
					wlan_mld_get_best_primary_umac_w_rssi(
							ml_peer,
							tmp_vdev_list,
							true,
							rssi_data);
				if (ml_peer->migrate_primary_umac_psoc_id ==
						ML_PRIMARY_UMAC_ID_INVAL) {
					mlo_err("Unable to fetch new primary link id for ml peer " QDF_MAC_ADDR_FMT,
						QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
					goto exit;
				}

				for (i = 0; i < tmp_cnt; i++) {
					if (ml_peer->migrate_primary_umac_psoc_id ==
							wlan_vdev_get_psoc_id(tmp_vdev_list[i])) {
						*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
								wlan_vdev_get_pdev(tmp_vdev_list[i]));
						break;
					}
				}
			}
		} else {
			ml_peer->migrate_primary_umac_psoc_id =
				wlan_mld_get_best_primary_umac_w_rssi(
							ml_peer,
							wlan_vdev_list,
							false,
							rssi_data);
			if (ml_peer->migrate_primary_umac_psoc_id ==
					ML_PRIMARY_UMAC_ID_INVAL) {
				mlo_err("Unable to fetch new primary link id for ml peer " QDF_MAC_ADDR_FMT,
					QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
				goto exit;
			}
			for (i = 0; i < idx; i++) {
				if (ml_peer->migrate_primary_umac_psoc_id ==
						wlan_vdev_get_psoc_id(wlan_vdev_list[i])) {
					*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
							wlan_vdev_get_pdev(wlan_vdev_list[i]));
					break;
				}
			}
		}
	} else {
		/* check if provided link id is part of current ml peer links */
		for (i = 0; i < idx; i++) {
			if (new_primary_link_id == wlan_vdev_get_link_id(wlan_vdev_list[i])) {
				*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
							wlan_vdev_get_pdev(wlan_vdev_list[i]));
				ml_peer->migrate_primary_umac_psoc_id =
						wlan_vdev_get_psoc_id(wlan_vdev_list[i]);
				break;
			}
		}
		if (*new_hw_link_id == INVALID_HW_LINK_ID && force_mig) {
			for (i = 0; i < tmp_cnt; i++) {
				if (new_primary_link_id ==
				    wlan_vdev_get_link_id(tmp_vdev_list[i])) {
					*new_hw_link_id = wlan_mlo_get_pdev_hw_link_id(
								wlan_vdev_get_pdev(tmp_vdev_list[i]));
					ml_peer->migrate_primary_umac_psoc_id =
							wlan_vdev_get_psoc_id(tmp_vdev_list[i]);
					break;
				}
			}
		}
	}

	if (*new_hw_link_id == INVALID_HW_LINK_ID) {
		mlo_err("New primary link id not found for ml peer " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		goto exit;
	}

	for (i = 0; i < idx; i++)
		wlan_objmgr_vdev_release_ref(wlan_vdev_list[i],
					     WLAN_MLME_SB_ID);

	for (i = 0; i < tmp_cnt; i++)
		wlan_objmgr_vdev_release_ref(tmp_vdev_list[i],
					     WLAN_MLME_SB_ID);
	return QDF_STATUS_SUCCESS;

exit:
	ml_peer->migrate_primary_umac_psoc_id = ML_PRIMARY_UMAC_ID_INVAL;

	for (i = 0; i < idx; i++)
		wlan_objmgr_vdev_release_ref(wlan_vdev_list[i],
					     WLAN_MLME_SB_ID);

	for (i = 0; i < tmp_cnt; i++)
		wlan_objmgr_vdev_release_ref(tmp_vdev_list[i],
					     WLAN_MLME_SB_ID);
	return QDF_STATUS_E_FAILURE;
}

/**
 * wlan_mlo_free_ptqm_migrate_list() - API to free peer ptqm migration list
 * @list: peer ptqm migration list
 *
 * API to free peer ptqm migration list
 *
 * Return: void
 */
static void wlan_mlo_free_ptqm_migrate_list(
			struct peer_migrate_ptqm_multi_entries *list)
{
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;

	peer_entry = mlo_ptqm_list_peek_head(&list->peer_list);
	while (peer_entry) {
		list->num_entries--;
		next_entry = mlo_get_next_peer_ctx(&list->peer_list,
						   peer_entry);
		if (peer_entry->peer)
			wlan_objmgr_peer_release_ref(peer_entry->peer,
						     WLAN_MLME_SB_ID);
		qdf_list_remove_node(&list->peer_list, &peer_entry->node);
		qdf_mem_free(peer_entry);
		peer_entry = next_entry;
	}
	qdf_list_destroy(&list->peer_list);
}

/**
 * wlan_mlo_reset_ptqm_migrate_list() - API to reset peer ptqm migration list
 * @ml_dev: MLO dev context
 * @list: peer ptqm migration list
 * @num_peers_failed: number of peers for which wmi cmd is failed.
 *
 * API to reset peer ptqm migration list
 *
 * Return: void
 */
static void wlan_mlo_reset_ptqm_migrate_list(
			struct wlan_mlo_dev_context *ml_dev,
			struct peer_migrate_ptqm_multi_entries *list,
			uint16_t num_peers_failed)
{
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;
	uint16_t count = 0;

	if (!ml_dev)
		return;

	peer_entry = mlo_ptqm_list_peek_head(&list->peer_list);
	while (peer_entry) {
		/* Reset the flags only for entries for which wmi
		 * command trigger is failed
		 */
		if (count < list->num_entries - num_peers_failed) {
			count++;
			next_entry = mlo_get_next_peer_ctx(&list->peer_list,
							   peer_entry);
			peer_entry = next_entry;
			continue;
		}
		if (peer_entry->peer) {
			qdf_clear_bit(peer_entry->mlo_peer_id, ml_dev->mlo_peer_id_bmap);
			peer_entry->peer->mlo_peer_ctx->primary_umac_migration_in_progress = false;
			peer_entry->peer->mlo_peer_ctx->migrate_primary_umac_psoc_id =
							ML_PRIMARY_UMAC_ID_INVAL;
		}
		next_entry = mlo_get_next_peer_ctx(&list->peer_list,
						   peer_entry);
		peer_entry = next_entry;
	}
}

static void
wlan_mlo_build_ptqm_migrate_list_internal(struct wlan_objmgr_vdev *vdev,
					  void *object, void *arg,
					  bool set_active)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct peer_migrate_ptqm_multi_entries *list =
				(struct peer_migrate_ptqm_multi_entries *)arg;
	struct peer_ptqm_migrate_list_entry *peer_entry;
	struct wlan_mlo_peer_context *ml_peer;
	uint16_t new_hw_link_id = INVALID_HW_LINK_ID;
	uint8_t current_primary_link_id = WLAN_LINK_ID_INVALID;
	QDF_STATUS status;

	if (!wlan_peer_is_mlo(peer) || !peer->mlo_peer_ctx)
		return;

	ml_peer = peer->mlo_peer_ctx;

	if (ml_peer->link_peer_cnt == 1)
		return;

	if (ml_peer->primary_umac_migration_in_progress) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " primary umac migration already in progress",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	current_primary_link_id = wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
	if (current_primary_link_id == WLAN_LINK_ID_INVALID ||
	    current_primary_link_id != wlan_vdev_get_link_id(vdev)) {
		mlo_debug("peer " QDF_MAC_ADDR_FMT " not having primary on current vdev",
			  QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	status = wlan_mlo_get_new_ptqm_id(vdev, ml_peer,
					  WLAN_LINK_ID_INVALID,
					  &new_hw_link_id, true, false,
					  list->rssi_data);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " unable to get new ptqm id",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	if (set_active)
		ml_peer->primary_umac_migration_in_progress = true;

	peer_entry = (struct peer_ptqm_migrate_list_entry *)
			qdf_mem_malloc(sizeof(struct peer_ptqm_migrate_list_entry));
	if (!peer_entry) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " unable to allocate peer entry",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return;
	}

	status = wlan_objmgr_peer_try_get_ref(peer, WLAN_MLME_SB_ID);
	peer_entry->peer = peer;
	peer_entry->new_hw_link_id = new_hw_link_id;
	peer_entry->mlo_peer_id = ml_peer->mlo_peer_id;
	qdf_list_insert_back(&list->peer_list, &peer_entry->node);
	list->num_entries++;
}

/**
 * wlan_mlo_build_ptqm_migrate_list() - API to build peer ptqm migration list
 * @vdev: objmgr vdev list
 * @object: peer object
 * @arg: list pointer
 *
 * API to build peer ptqm migration list
 *
 * Return: void
 */
static void wlan_mlo_build_ptqm_migrate_list(struct wlan_objmgr_vdev *vdev,
					     void *object, void *arg)
{
	wlan_mlo_build_ptqm_migrate_list_internal(vdev, object, arg, true);
}

/**
 * wlan_mlo_trigger_link_ptqm_migration() - API to trigger ptqm migration
 * for a link
 * @vdev: objmgr vdev object
 * @rssi_data: RSSI data of all the HW links
 *
 * API to trigger ptqm migration of all peers having primary on given link
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
wlan_mlo_trigger_link_ptqm_migration(struct wlan_objmgr_vdev *vdev,
				     const struct mlo_all_link_rssi *rssi_data)
{
	struct peer_migrate_ptqm_multi_entries migrate_list = {0};
	QDF_STATUS status;
	uint16_t num_peers_failed = 0;

	qdf_list_create(&migrate_list.peer_list, MAX_MLO_PEER_ID);
	migrate_list.rssi_data = rssi_data;

	wlan_objmgr_iterate_peerobj_list(vdev,
					 wlan_mlo_build_ptqm_migrate_list,
					 &migrate_list, WLAN_MLME_NB_ID);

	/* trigger WMI */
	if (migrate_list.num_entries == 0) {
		mlo_err("No peer found");
		return QDF_STATUS_SUCCESS;
	}

	status = wlan_mlo_send_ptqm_migrate_cmd(vdev, &migrate_list,
						&num_peers_failed);
	if (QDF_IS_STATUS_ERROR(status))
		wlan_mlo_reset_ptqm_migrate_list(vdev->mlo_dev_ctx,
						 &migrate_list,
						 num_peers_failed);
	wlan_mlo_free_ptqm_migrate_list(&migrate_list);
	return status;
}

QDF_STATUS wlan_mlo_set_ptqm_migration(struct wlan_objmgr_vdev *vdev,
				       struct wlan_mlo_peer_context *ml_peer,
				       bool link_migration,
				       uint32_t link_id, bool force_mig,
				       bool allow_reelection)
{
	uint16_t new_hw_link_id = INVALID_HW_LINK_ID;
	struct peer_migrate_ptqm_multi_entries migrate_list = {0};
	struct peer_ptqm_migrate_list_entry *peer_entry;
	struct wlan_objmgr_vdev *curr_vdev = NULL;
	uint8_t current_primary_link_id = WLAN_LINK_ID_INVALID;
	uint16_t num_peers_failed = 0;
	QDF_STATUS status;
	struct mlo_all_link_rssi rssi_data = {0};
	int cur_prim_hw_link_id;
	struct wlan_objmgr_vdev *curr_pdev;

	if (!vdev) {
		mlo_err("Vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (link_migration == false && !ml_peer) {
		mlo_err("ML peer is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	rssi_data.curr_mlo_peer_id = ml_peer->mlo_peer_id;
	mld_get_link_rssi(&rssi_data);

	if (link_migration) {
		mlo_info("Trigger migration for full link");
		// trigger full link migration
		status = wlan_mlo_trigger_link_ptqm_migration(vdev, &rssi_data);
		if (QDF_IS_STATUS_ERROR(status))
			mlo_err("Failed to trigger link migration");
		return status;
	}

	if (ml_peer->link_peer_cnt == 1) {
		mlo_info("peer " QDF_MAC_ADDR_FMT " is SLO",
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_FAILURE;
	}

	if (ml_peer->primary_umac_migration_in_progress) {
		mlo_info("peer " QDF_MAC_ADDR_FMT " primary umac migration already in progress",
			 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		return QDF_STATUS_E_FAILURE;
	}

	current_primary_link_id = wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer(ml_peer);
	if (current_primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_err("Current primary link id is invalid");
		return QDF_STATUS_E_FAILURE;
	}

	curr_vdev = mlo_get_vdev_by_link_id(vdev, current_primary_link_id,
					    WLAN_MLO_MGR_ID);
	if (!curr_vdev) {
		mlo_err("Unable to get current primary vdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	curr_pdev = wlan_vdev_get_pdev(curr_vdev);
	if (!curr_pdev) {
		mlo_err("Unable to get current primary pdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	cur_prim_hw_link_id = wlan_mlo_get_pdev_hw_link_id(curr_pdev);
	if (cur_prim_hw_link_id == WLAN_LINK_ID_INVALID) {
		mlo_err("Cur pri HW link %d invalid", cur_prim_hw_link_id);
		return QDF_STATUS_E_FAILURE;
	}

	mlo_debug("Cur primary HW link ID %d", cur_prim_hw_link_id);

	status = wlan_mlo_get_new_ptqm_id(curr_vdev, ml_peer,
					  link_id, &new_hw_link_id,
					  force_mig, allow_reelection, &rssi_data);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("peer " QDF_MAC_ADDR_FMT " unable to get new ptqm id",
			QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes));
		goto exit;
	}

	mlo_debug("New primary HW link ID %d", new_hw_link_id);

	if (cur_prim_hw_link_id == new_hw_link_id) {
		mlo_debug("Same cur and new pri HW link %d", new_hw_link_id);
		status = QDF_STATUS_E_ALREADY;
		goto exit;
	}

	ml_peer->primary_umac_migration_in_progress = true;

	peer_entry = (struct peer_ptqm_migrate_list_entry *)
				qdf_mem_malloc(sizeof(struct peer_ptqm_migrate_list_entry));
	if (!peer_entry) {
		mlo_err("Failed to allocate peer entry");
		status = QDF_STATUS_E_NULL_VALUE;
		goto exit;
	}

	peer_entry->new_hw_link_id = new_hw_link_id;
	peer_entry->mlo_peer_id = ml_peer->mlo_peer_id;
	qdf_list_create(&migrate_list.peer_list, MAX_MLO_PEER_ID);
	qdf_list_insert_back(&migrate_list.peer_list, &peer_entry->node);
	migrate_list.num_entries = 1;

	//trigger WMI
	status = wlan_mlo_send_ptqm_migrate_cmd(curr_vdev, &migrate_list,
						&num_peers_failed);
	if (QDF_IS_STATUS_ERROR(status))
		wlan_mlo_reset_ptqm_migrate_list(curr_vdev->mlo_dev_ctx,
						 &migrate_list,
						 num_peers_failed);
	wlan_mlo_free_ptqm_migrate_list(&migrate_list);

exit:
	if (curr_vdev)
		mlo_release_vdev_ref(curr_vdev);

	return status;
}

/*
 * struct ptqm_migrate_peer_req: Peer request
 *
 * @module_id: Module id
 * @priority: Priority of the user module
 * @is_link_req: Indicate if it is due to a link request
 * @user_data: Opaque user data
 * @begin: Callback to be called at the beginning
 * @end: Callback to be called at the end
 * @src_link_id: Source link id
 * @dst_link_id: Destination link id
 * @force_mig: Force migration
 * @allow_reelection: Allow reelection of current primary link
 */
struct ptqm_migrate_peer_req {
	uint8_t module_id;
	uint8_t priority;
	bool is_link_req;
	void *user_data;
	void (*begin)(struct wlan_mlo_peer_context *ml_peer, void *user_data);
	void (*end)(struct wlan_mlo_peer_context *ml_peer,
		    enum primary_link_peer_migration_evenr_status status,
		    void *user_data);
	uint8_t src_link_id;
	uint8_t dst_link_id;
	bool force_mig;
	bool allow_reelection;
};

/* Max peer request node in the req_list */
#define PTQM_MIGRATION_PENDING_PEER_REQ PTQM_MIGRATION_MODULE_MAX

/*
 * struct ptqm_migrate_peer_req_node - Peer request node
 *
 * @req: Peer request
 * @node: QDF list node member
 */
struct ptqm_migrate_peer_req_node {
	struct ptqm_migrate_peer_req req;
	qdf_list_node_t node;
};

/*
 * struct ptqm_migrate_peer_req_context - Peer request context
 *
 * @req_list: Peer request list
 * @result_src_link_id: Resultant source link id
 * @result_dst_link_id: Resultant destination link id
 * @result_priority: Resultant priority
 * @force_mig: Force migration
 * @allow_reelection: Allow reelection of current primary link
 */
struct ptqm_migrate_peer_req_context {
	qdf_list_t req_list;
	uint8_t result_src_link_id;
	uint8_t result_dst_link_id;
	uint8_t result_priority;
	bool force_mig;
	bool allow_reelection;
};

/*
 * struct ptqm_migrate_link_req_context: Link request
 *
 * @vdev: VDEV object
 * @src_link_id: Source link id
 * @migration_list: Peer list
 * @rssi_data: RSSI data of all the HW links
 * @begin: Callback to be called at the beginning
 * @end: Callback to be called at the end
 * @user_data: Opaque user data
 * @link_disable: Link disable flag
 * @force_mig: Force migration
 * @ptqm_link_lock: Spin lock
 * @link_req_ctx_idx: idx of current link ptqm context ml dev context
 */
struct ptqm_migrate_link_req_context {
	struct wlan_objmgr_vdev *vdev;
	uint8_t src_link_id;
	struct peer_migrate_ptqm_multi_entries migration_list;
	struct mlo_all_link_rssi rssi_data;
	void (*begin)(struct wlan_objmgr_vdev *vdev, void *user_data);
	void (*end)(struct wlan_objmgr_vdev *vdev,
		    QDF_STATUS status, void *user_data,
		    struct ptqm_link_migration_rsp_params *rsp_params);
	void *user_data;
	bool link_disable;

	qdf_bitmap(mlo_peer_id_pending_bmap, MAX_MLO_PEER_ID);
	uint8_t status;
	uint8_t module_id;
	struct ptqm_link_migration_rsp_params rsp_params;
	bool force_mig;
	qdf_spinlock_t ptqm_link_lock;
	uint8_t link_req_ctx_idx;
};

/*
 * struct ptqm_migrate_peer_context - Peer context
 *
 * @ptqm_peer_lock: Spin lock
 * @pending_req_ctx: Pending peer request context
 * @active_req_ctx: Active peer request context
 */
struct ptqm_migrate_peer_context {
	qdf_spinlock_t ptqm_peer_lock;
	struct ptqm_migrate_peer_req_context *pending_req_ctx;
	struct ptqm_migrate_peer_req_context *active_req_ctx;
};

static QDF_STATUS
wlan_ptqm_peer_migrate_req_add_internal(struct wlan_objmgr_vdev *vdev,
					struct wlan_mlo_peer_context *ml_peer,
					struct ptqm_peer_migrate_params *params,
					bool is_link_req);

/*
 * Peer context operations
 */
static inline struct ptqm_migrate_peer_context *
wlan_ptqm_get_mlo_peer_context(struct wlan_mlo_peer_context *ml_peer)
{
	return ml_peer->peer_ptqm_migrate_ctx;
}

static inline void
wlan_ptqm_set_mlo_peer_context(struct wlan_mlo_peer_context *ml_peer,
			       struct ptqm_migrate_peer_context *ptqm_peer_ctx)
{
	ml_peer->peer_ptqm_migrate_ctx = ptqm_peer_ctx;
}

static void
wlan_ptqm_peer_req_context_free(struct ptqm_migrate_peer_req_context *req_ctx)
{
	struct ptqm_migrate_peer_req_node *t_node;
	qdf_list_node_t *node;
	QDF_STATUS status;

	status = qdf_list_peek_front(&req_ctx->req_list, &node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		t_node = qdf_container_of(node,
					  struct ptqm_migrate_peer_req_node,
					  node);

		status = qdf_list_peek_next(&req_ctx->req_list, node, &node);

		qdf_list_remove_node(&req_ctx->req_list, &t_node->node);

		qdf_mem_free(t_node);
	}

	qdf_list_destroy(&req_ctx->req_list);
	qdf_mem_free(req_ctx);
}

static void
wlan_ptqm_req_for_each(struct wlan_mlo_peer_context *ml_peer,
		       struct ptqm_migrate_peer_req_context *ctx,
		       void (*cb)(struct wlan_mlo_peer_context *ml_peer,
				  struct ptqm_migrate_peer_req_node *req_node,
				  void *cbd), void *cbd)
{
	struct ptqm_migrate_peer_req_node *req_node;
	qdf_list_node_t *node;
	QDF_STATUS status;

	status = qdf_list_peek_front(&ctx->req_list, &node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		req_node = qdf_container_of(node,
					    struct ptqm_migrate_peer_req_node,
					    node);
		if (cb)
			cb(ml_peer, req_node, cbd);
		status = qdf_list_peek_next(&ctx->req_list, node, &node);
	}
}

struct wlan_ptqm_notify_params {
	uint8_t status;
};

static void
wlan_ptqm_notify_begin_cb(struct wlan_mlo_peer_context *ml_peer,
			  struct ptqm_migrate_peer_req_node *req_node,
			  void *cbd)
{
	if (req_node && req_node->req.begin)
		req_node->req.begin(ml_peer, req_node->req.user_data);
}

static void
wlan_ptqm_notify_end_cb(struct wlan_mlo_peer_context *ml_peer,
			struct ptqm_migrate_peer_req_node *req_node,
			void *cbd)
{
	struct wlan_ptqm_notify_params *params =
		(struct wlan_ptqm_notify_params *)cbd;

	if (req_node && req_node->req.end)
		req_node->req.end(ml_peer, params->status,
				  req_node->req.user_data);
}

static inline void
wlan_ptqm_notify_begin(struct wlan_mlo_peer_context *ml_peer,
		       struct ptqm_migrate_peer_req_context *active_ctx)
{
	wlan_ptqm_req_for_each(ml_peer, active_ctx,
			       wlan_ptqm_notify_begin_cb, NULL);
}

static inline void
wlan_ptqm_notify_end(struct wlan_mlo_peer_context *ml_peer,
		     struct ptqm_migrate_peer_req_context *active_ctx,
		     uint8_t status)
{
	struct wlan_ptqm_notify_params params;

	params.status = status;

	wlan_ptqm_req_for_each(ml_peer, active_ctx,
			       wlan_ptqm_notify_end_cb, &params);
}

QDF_STATUS
wlan_ptqm_peer_migrate_ctx_alloc(struct wlan_mlo_peer_context *ml_peer)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;

	if (!ml_peer)
		return QDF_STATUS_E_NULL_VALUE;

	ptqm_peer_ctx = qdf_mem_malloc(sizeof(*ptqm_peer_ctx));
	if (!ptqm_peer_ctx) {
		mlo_err("Unable to allocate peer ptqm context");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_spinlock_create(&ptqm_peer_ctx->ptqm_peer_lock);

	wlan_ptqm_set_mlo_peer_context(ml_peer, ptqm_peer_ctx);

	return QDF_STATUS_SUCCESS;
}

void
wlan_ptqm_peer_migrate_ctx_free(struct wlan_mlo_peer_context *ml_peer)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;

	if (!ml_peer)
		return;

	ptqm_peer_ctx = wlan_ptqm_get_mlo_peer_context(ml_peer);
	if (!ptqm_peer_ctx)
		return;

	if (ptqm_peer_ctx->pending_req_ctx) {
		mlo_info("clearing pending_req_ctx for ml_peer %u",
			 ml_peer->mlo_peer_id);
		wlan_ptqm_notify_end(ml_peer, ptqm_peer_ctx->pending_req_ctx,
				     PRIMARY_LINK_PEER_MIGRATION_DELETED);
		wlan_ptqm_peer_req_context_free(ptqm_peer_ctx->pending_req_ctx);
		ptqm_peer_ctx->pending_req_ctx = NULL;
	}

	if (ptqm_peer_ctx->active_req_ctx) {
		mlo_info("clearing active_req_ctx for ml_peer %u",
			 ml_peer->mlo_peer_id);
		wlan_ptqm_notify_end(ml_peer, ptqm_peer_ctx->active_req_ctx,
				     PRIMARY_LINK_PEER_MIGRATION_DELETED);
		wlan_ptqm_peer_req_context_free(ptqm_peer_ctx->active_req_ctx);
		ptqm_peer_ctx->active_req_ctx = NULL;
	}

	qdf_spinlock_destroy(&ptqm_peer_ctx->ptqm_peer_lock);

	qdf_mem_free(ptqm_peer_ctx);
	wlan_ptqm_set_mlo_peer_context(ml_peer, NULL);
}

static void
wlan_ptqm_print_peer_req_context(struct ptqm_migrate_peer_req_context *req_ctx)
{
	struct ptqm_migrate_peer_req_node *req_node;
	struct ptqm_migrate_peer_req *req;
	qdf_list_node_t *node;
	QDF_STATUS status;
	uint8_t i = 0;

	status = qdf_list_peek_front(&req_ctx->req_list, &node);
	while (QDF_IS_STATUS_SUCCESS(status)) {
		req_node = qdf_container_of(node,
					    struct ptqm_migrate_peer_req_node,
					    node);
		req = &req_node->req;
		mlo_nofl_info("[%u: id %u prio %u is_link %u src %d dst %d]",
			      ++i, req->module_id, req->priority,
			      req->is_link_req, req->src_link_id,
			      req->dst_link_id);
		status = qdf_list_peek_next(&req_ctx->req_list, node, &node);
	}

	mlo_nofl_info("resultant: src %d dst %d prio %u",
		      req_ctx->result_src_link_id, req_ctx->result_dst_link_id,
		      req_ctx->result_priority);
}

void wlan_ptqm_print_peer_context(struct wlan_mlo_peer_context *ml_peer)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;
	struct ptqm_migrate_peer_req_context *pending_req_ctx, *active_req_ctx;

	ptqm_peer_ctx = wlan_ptqm_get_mlo_peer_context(ml_peer);

	pending_req_ctx = ptqm_peer_ctx->pending_req_ctx;
	active_req_ctx = ptqm_peer_ctx->active_req_ctx;

	qdf_nofl_info("Pending context:");
	if (pending_req_ctx)
		wlan_ptqm_print_peer_req_context(pending_req_ctx);
	qdf_nofl_info("Active context:");
	if (active_req_ctx)
		wlan_ptqm_print_peer_req_context(active_req_ctx);
}

#ifdef PTQM_LAYER_DEBUG
static uint8_t ptqm_migration_core_op;

void wlan_ptqm_migration_op_set(uint8_t op)
{
	ptqm_migration_core_op = op;
}
#endif

static void
wlan_ptqm_peer_migration_set(struct wlan_mlo_peer_context *ml_peer)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_mlo_dev_context *dev_ctx;
	struct ptqm_migrate_peer_req_context *active_req_ctx;
	uint8_t primary_link_id;
	uint8_t dst_link_id;
	bool force_mig;
	bool allow_reelection;
	QDF_STATUS status;
	int i;

	ptqm_peer_ctx = wlan_ptqm_get_mlo_peer_context(ml_peer);
	if (!ptqm_peer_ctx) {
		mlo_err("NULL ptqm_peer_ctx");
		return;
	}

	active_req_ctx = ptqm_peer_ctx->active_req_ctx;
	if (!active_req_ctx) {
		mlo_err("NULL active_req_ctx");
		return;
	}

	dst_link_id = active_req_ctx->result_dst_link_id;
	force_mig = active_req_ctx->force_mig;
	allow_reelection = active_req_ctx->allow_reelection;

	primary_link_id = wlan_mlo_peer_get_primary_peer_link_id_by_ml_peer
				(ml_peer);
	if (primary_link_id == WLAN_LINK_ID_INVALID) {
		mlo_err("Unable to get current primary link");
		goto fail;
	}

	dev_ctx = ml_peer->ml_dev;

	mlo_dev_lock_acquire(dev_ctx);
	for (i = 0; i < WLAN_UMAC_MLO_MAX_VDEVS; i++) {
		if (dev_ctx->wlan_vdev_list[i] &&
		    wlan_vdev_mlme_is_mlo_vdev(dev_ctx->wlan_vdev_list[i]) &&
		    dev_ctx->wlan_vdev_list[i]->vdev_mlme.mlo_link_id ==
		    primary_link_id) {
			if (wlan_objmgr_vdev_try_get_ref
				(dev_ctx->wlan_vdev_list[i],
				 WLAN_MLO_MGR_ID) == QDF_STATUS_SUCCESS) {
				vdev = dev_ctx->wlan_vdev_list[i];
			}
			break;
		}
	}
	mlo_dev_lock_release(dev_ctx);

	if (!vdev) {
		mlo_err("Unable to find vdev");
		goto fail;
	}

	/*
	 * Notify begin
	 */
	wlan_ptqm_notify_begin(ml_peer, active_req_ctx);

	mlo_info("[Core PTQM] ml_peer %u dst_link_id %u forcemig %d reelect %d",
		 ml_peer->mlo_peer_id, dst_link_id, force_mig,
		 allow_reelection);
	/*
	 * Post request to core framework
	 */
#ifdef PTQM_LAYER_DEBUG
	if (!ptqm_migration_core_op) {
		status = wlan_mlo_set_ptqm_migration(vdev, ml_peer, false,
						     dst_link_id, force_mig,
						     allow_reelection);
	} else {
		mlo_info("debug mode is enabled for ml_peer %u",
			 ml_peer->mlo_peer_id);
		status = QDF_STATUS_SUCCESS;
	}
#else
	status = wlan_mlo_set_ptqm_migration(vdev, ml_peer, false, dst_link_id,
					     force_mig, allow_reelection);
#endif

	if (vdev)
		mlo_release_vdev_ref(vdev);

	/*
	 * Notify end in case of failure
	 */
	if (QDF_IS_STATUS_ERROR(status)) {
		enum primary_link_peer_migration_evenr_status mig_status;

		if (status == QDF_STATUS_E_ALREADY)
			mig_status = PRIMARY_LINK_PEER_MIGRATION_NOT_REQUIRED;
		else
			mig_status = PRIMARY_LINK_PEER_MIGRATION_FAIL;

		wlan_ptqm_notify_end(ml_peer, active_req_ctx, mig_status);
		wlan_ptqm_peer_req_context_free(active_req_ctx);
		ptqm_peer_ctx->active_req_ctx = NULL;
	}

	return;
fail:
	wlan_ptqm_notify_begin(ml_peer, active_req_ctx);
	wlan_ptqm_notify_end(ml_peer, active_req_ctx,
			     PRIMARY_LINK_PEER_MIGRATION_FAIL);
	wlan_ptqm_peer_req_context_free(active_req_ctx);
	ptqm_peer_ctx->active_req_ctx = NULL;
}

static void
wlan_ptqm_schedule_peer_migrate_req(struct wlan_mlo_peer_context *ml_peer)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;

	ptqm_peer_ctx = wlan_ptqm_get_mlo_peer_context(ml_peer);
	if (!ptqm_peer_ctx) {
		mlo_debug("Null ptqm_peer_ctx");
		return;
	}

	if (!ptqm_peer_ctx->pending_req_ctx) {
		mlo_debug("Null pending_req_ctx");
		return;
	}

	if (!ptqm_peer_ctx->active_req_ctx) {
		ptqm_peer_ctx->active_req_ctx = ptqm_peer_ctx->pending_req_ctx;
		ptqm_peer_ctx->pending_req_ctx = NULL;
		/*
		 * Post ptqm peer request to the core framework
		 */
		wlan_ptqm_peer_migration_set(ml_peer);
	}
}

static void
wlan_ptqm_update_resulant_req
	(struct ptqm_migrate_peer_req_context *pending_req_ctx)
{
	struct ptqm_migrate_peer_req_node *req_node;
	qdf_list_node_t *node;
	QDF_STATUS status;
	int src_link_id, dst_link_id;
	uint8_t list_priority;
	bool force_mig = false;
	bool allow_reelection = false;

	status = qdf_list_peek_front(&pending_req_ctx->req_list, &node);

	if (status == QDF_STATUS_E_EMPTY) {
		mlo_debug("No req in the pending context");
		return;
	}

	src_link_id = HW_LINK_ID_ANY;
	dst_link_id = HW_LINK_ID_ANY;
	list_priority = -1; /* max */
	while (QDF_IS_STATUS_SUCCESS(status)) {
		req_node = qdf_container_of
			(node, struct ptqm_migrate_peer_req_node,
			 node);
		src_link_id = req_node->req.src_link_id;
		if (!force_mig)
			force_mig = req_node->req.force_mig;
		if (!allow_reelection)
			allow_reelection = req_node->req.allow_reelection;
		if (req_node->req.dst_link_id != HW_LINK_ID_ANY) {
			dst_link_id = req_node->req.dst_link_id;
			if (req_node->req.priority < list_priority)
				list_priority = req_node->req.priority;
		}
		status = qdf_list_peek_next(&pending_req_ctx->req_list, node,
					    &node);
	}

	pending_req_ctx->result_src_link_id = src_link_id;
	pending_req_ctx->result_dst_link_id = dst_link_id;
	pending_req_ctx->result_priority = list_priority;
	pending_req_ctx->force_mig = force_mig;
	pending_req_ctx->allow_reelection = allow_reelection;

	mlo_debug("src %u dst %u prio %u force_mig %d reelect %d", src_link_id,
		  dst_link_id, list_priority, force_mig, allow_reelection);
}

/*
 * Assumption:
 * req->src_link_id = [0, N]
 * req->dst_link_id = {any, [0, N]}
 */
static QDF_STATUS
wlan_ptqm_add_pending_context
	(struct ptqm_migrate_peer_req_context *pending_req_ctx,
	 struct ptqm_migrate_peer_req *req)
{
	struct ptqm_migrate_peer_req_node *req_node;

	req_node = qdf_mem_malloc(sizeof(*req_node));
	if (!req_node) {
		mlo_err("Unable to allocate req node");
		return QDF_STATUS_E_NOMEM;
	}

	req_node->req = *req;

	/*
	 * This is the case when there is a conflict between new
	 * request's hw_link_id and the resulant request's hw_link_id.
	 *
	 * We need to find a resultant list priority from the req_list
	 * and compare that priority with the new request's priority to
	 * resolve the conflict. Resultant list priority is the highest
	 * priority among nodes for which dst != any.
	 *
	 * If the new request's priority is greater than the resultant
	 * list priority than remove all the nodes with dst != any.
	 * Else, discard the new request.
	 */
	if (!qdf_list_empty(&pending_req_ctx->req_list) &&
	    pending_req_ctx->result_dst_link_id != HW_LINK_ID_ANY &&
	    req->dst_link_id != HW_LINK_ID_ANY &&
	    pending_req_ctx->result_dst_link_id != req->dst_link_id) {
		struct ptqm_migrate_peer_req_node *t_node = NULL;
		qdf_list_node_t *curr_node = NULL, *next_node = NULL;

		if (pending_req_ctx->result_priority <= req->priority) {
			mlo_err("req prio %u is lower than resultant prio %u",
				req->priority,
				pending_req_ctx->result_priority);
			qdf_mem_free(req_node);
			return QDF_STATUS_E_FAILURE;
		}

		/*
		 * remove nodes with dst_link_id != any
		 */
		qdf_list_peek_front(&pending_req_ctx->req_list, &curr_node);
		while (curr_node) {
			qdf_list_peek_next(&pending_req_ctx->req_list,
					   curr_node, &next_node);
			t_node = qdf_container_of
				(curr_node, struct ptqm_migrate_peer_req_node,
					 node);
			mlo_debug("id %u dst %u prio %u is_link_req %u",
				  t_node->req.module_id,
				  t_node->req.dst_link_id,
				  t_node->req.priority,
				  t_node->req.is_link_req);
			if (t_node->req.dst_link_id != HW_LINK_ID_ANY) {
				qdf_list_remove_node(&pending_req_ctx->req_list,
						     &t_node->node);
				qdf_mem_free(t_node);
				t_node = NULL;
			}
			curr_node = next_node;
			next_node = NULL;
		}
	}

	/*
	 * The new request can be merged with the resultant request.
	 * Add a new node in the req list and update the resultant request.
	 */
	qdf_list_insert_front(&pending_req_ctx->req_list, &req_node->node);

	wlan_ptqm_update_resulant_req(pending_req_ctx);

	return QDF_STATUS_SUCCESS;
}

/*
 * Link request operations
 */

static inline struct ptqm_migrate_link_req_context *
wlan_ptqm_get_mlo_link_context(struct wlan_mlo_dev_context *dev_ctx,
			       struct wlan_objmgr_vdev *vdev)
{
	struct ptqm_migrate_link_req_context *link_req_ctx;
	uint8_t idx;

	for (idx = 0; idx < WLAN_UMAC_MLO_MAX_VDEVS; idx++) {
		link_req_ctx = dev_ctx->link_ptqm_migrate_ctx[idx];
		if (!link_req_ctx)
			continue;

		if (vdev == link_req_ctx->vdev)
			return link_req_ctx;
	}
	return NULL;
}

static inline QDF_STATUS
wlan_ptqm_set_mlo_link_context(struct wlan_mlo_dev_context *dev_ctx,
			       struct wlan_objmgr_vdev *vdev,
			       struct ptqm_migrate_link_req_context *ctx)
{
	struct ptqm_migrate_link_req_context *link_req_ctx;
	uint8_t idx;
	uint8_t set_idx = WLAN_UMAC_MLO_MAX_VDEVS;

	for (idx = 0; idx < WLAN_UMAC_MLO_MAX_VDEVS; idx++) {
		link_req_ctx = dev_ctx->link_ptqm_migrate_ctx[idx];
		if (!link_req_ctx) {
			if (set_idx == WLAN_UMAC_MLO_MAX_VDEVS)
				set_idx = idx;
			continue;
		}
		if (link_req_ctx->vdev && link_req_ctx->vdev == vdev) {
			set_idx = idx;
			break;
		}
	}

	if (set_idx != WLAN_UMAC_MLO_MAX_VDEVS) {
		if (ctx)
			ctx->link_req_ctx_idx = set_idx;
		dev_ctx->link_ptqm_migrate_ctx[set_idx] = ctx;
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

static inline QDF_STATUS
wlan_ptqm_migrate_list_init(struct peer_migrate_ptqm_multi_entries *list,
			    struct mlo_all_link_rssi *rssi_data)
{
	qdf_mem_zero(list, sizeof(*list));
	qdf_list_create(&list->peer_list, MAX_MLO_PEER_ID);

	list->rssi_data = rssi_data;

	return QDF_STATUS_SUCCESS;
}

static void
wlan_mlo_build_ptqm_layer_migrate_list(struct wlan_objmgr_vdev *vdev,
				       void *object, void *arg)
{
	wlan_mlo_build_ptqm_migrate_list_internal(vdev, object, arg, false);
}

static inline void
wlan_ptqm_migrate_list_create(struct wlan_objmgr_vdev *vdev,
			      struct peer_migrate_ptqm_multi_entries *list)
{
	wlan_objmgr_iterate_peerobj_list(vdev,
					 wlan_mlo_build_ptqm_layer_migrate_list,
					 list, WLAN_MLO_MGR_ID);
}

static inline void
wlan_ptqm_migrate_list_destroy(struct peer_migrate_ptqm_multi_entries *list)
{
	/*
	 * free peers from the peer_list and destroy the peer_list
	 */
	list->rssi_data = NULL;
	wlan_mlo_free_ptqm_migrate_list(list);
}

static inline bool
wlan_ptqm_migrate_list_empty(struct peer_migrate_ptqm_multi_entries *list)
{
	return qdf_list_empty(&list->peer_list);
}

static inline uint16_t
wlan_ptqm_migrate_list_peer_count(struct peer_migrate_ptqm_multi_entries *list)
{
	return list->num_entries;
}

static void
wlan_ptqm_notify_link_req_begin
	(struct ptqm_migrate_link_req_context *ptqm_link_ctx)
{
	if (ptqm_link_ctx->begin)
		ptqm_link_ctx->begin(ptqm_link_ctx->vdev,
				     ptqm_link_ctx->user_data);
}

static void
wlan_ptqm_notify_link_req_end
	(struct ptqm_migrate_link_req_context *ptqm_link_ctx)
{
	if (ptqm_link_ctx->end) {
		ptqm_link_ctx->end(ptqm_link_ctx->vdev,
				   ptqm_link_ctx->rsp_params.fail_peer_count ?
				   QDF_STATUS_E_FAILURE : QDF_STATUS_SUCCESS,
				   ptqm_link_ctx->user_data,
				   &ptqm_link_ctx->rsp_params);
	}
}

static void
wlan_ptqm_peer_migration_begin_link_cb(struct wlan_mlo_peer_context *ml_peer,
				       void *user_data)
{
	struct ptqm_migrate_link_req_context *link_req_ctx;
	struct wlan_objmgr_vdev *vdev;

	link_req_ctx = (struct ptqm_migrate_link_req_context *)user_data;
	vdev = link_req_ctx->vdev;

	mlo_info("vdev %u ml peer %u", wlan_vdev_get_id(vdev),
		 ml_peer->mlo_peer_id);
}

static void
wlan_ptqm_peer_migration_end_link_cb
	(struct wlan_mlo_peer_context *ml_peer,
	 enum primary_link_peer_migration_evenr_status status,
	 void *user_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_mlo_dev_context *ml_dev;
	struct ptqm_migrate_link_req_context *link_req_ctx;
	struct ptqm_link_migration_rsp_params *rsp_params;
	uint8_t vdev_id;

	link_req_ctx = (struct ptqm_migrate_link_req_context *)user_data;

	qdf_spin_lock_bh(&link_req_ctx->ptqm_link_lock);

	vdev = link_req_ctx->vdev;

	vdev_id = wlan_vdev_get_id(vdev);

	mlo_info("vdev %u ml peer %u status %u", vdev_id, ml_peer->mlo_peer_id,
		 status);

	ml_dev = vdev->mlo_dev_ctx;

	rsp_params = &link_req_ctx->rsp_params;

	qdf_clear_bit(ml_peer->mlo_peer_id,
		      link_req_ctx->mlo_peer_id_pending_bmap);

	if (status != PRIMARY_LINK_PEER_MIGRATION_SUCCESS)
		++rsp_params->fail_peer_count;

	if (qdf_bitmap_empty(link_req_ctx->mlo_peer_id_pending_bmap,
			     MAX_MLO_PEER_ID)) {
		mlo_info("migration completed for vdev %u", vdev_id);
		wlan_ptqm_notify_link_req_end(link_req_ctx);
		wlan_ptqm_migrate_list_destroy(&link_req_ctx->migration_list);
		qdf_spin_unlock_bh(&link_req_ctx->ptqm_link_lock);
		qdf_spinlock_destroy(&link_req_ctx->ptqm_link_lock);
		wlan_ptqm_set_mlo_link_context(ml_dev, link_req_ctx->vdev,
					       NULL);
		wlan_objmgr_vdev_release_ref(link_req_ctx->vdev,
					     WLAN_MLO_MGR_ID);
		qdf_mem_free(link_req_ctx);
		return;
	}

	qdf_spin_unlock_bh(&link_req_ctx->ptqm_link_lock);
}

static void
wlan_ptqm_schedule_link_migrate_req
	(struct wlan_objmgr_vdev *vdev,
	 struct ptqm_migrate_link_req_context *link_req_ctx)
{
	struct wlan_mlo_dev_context *ml_dev;
	struct peer_migrate_ptqm_multi_entries *migration_list;
	struct peer_ptqm_migrate_list_entry *peer_entry, *next_entry;
	uint8_t vdev_id;
	QDF_STATUS status;
	uint8_t link_req_ctx_idx;

	vdev_id = wlan_vdev_get_id(vdev);

	ml_dev = vdev->mlo_dev_ctx;

	link_req_ctx_idx = link_req_ctx->link_req_ctx_idx;

	migration_list = &link_req_ctx->migration_list;

	/*
	 * Iterate through the peer list and set pending bitmask
	 * before starting peer migration
	 */
	qdf_spin_lock_bh(&link_req_ctx->ptqm_link_lock);

	peer_entry = mlo_ptqm_list_peek_head(&migration_list->peer_list);
	while (peer_entry) {
		qdf_set_bit(peer_entry->mlo_peer_id,
			    link_req_ctx->mlo_peer_id_pending_bmap);

		next_entry = mlo_get_next_peer_ctx(&migration_list->peer_list,
						   peer_entry);
		peer_entry = next_entry;
	}

	qdf_spin_unlock_bh(&link_req_ctx->ptqm_link_lock);

	peer_entry = mlo_ptqm_list_peek_head(&migration_list->peer_list);
	while (peer_entry) {
		struct ptqm_peer_migrate_params params = {0};

		params.module_id = PTQM_MIGRATION_MODULE_LINK_REQ;

		params.src_link_id = HW_LINK_ID_ANY;
		params.dst_link_id = peer_entry->new_hw_link_id;

		params.begin = wlan_ptqm_peer_migration_begin_link_cb;
		params.end = wlan_ptqm_peer_migration_end_link_cb;

		params.user_data = link_req_ctx;

		params.force_mig = link_req_ctx->force_mig;

		mlo_info("ml_peer %u dst %u", peer_entry->mlo_peer_id,
			 peer_entry->new_hw_link_id);

		status = wlan_ptqm_peer_migrate_req_add_internal
			(vdev, peer_entry->peer->mlo_peer_ctx, &params, true);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlo_err("Unable to add peer request for ml_peer %u",
				peer_entry->mlo_peer_id);
			qdf_clear_bit(peer_entry->mlo_peer_id,
				      link_req_ctx->mlo_peer_id_pending_bmap);
			link_req_ctx->rsp_params.fail_peer_count++;
			if (qdf_bitmap_empty
					(link_req_ctx->mlo_peer_id_pending_bmap,
					 MAX_MLO_PEER_ID)) {
				goto fail;
			}
		}

		if (!ml_dev->link_ptqm_migrate_ctx[link_req_ctx_idx]) {
			mlo_info("vdev_id %u link_req_ctx is NULL", vdev_id);
			return;
		}

		next_entry = mlo_get_next_peer_ctx(&migration_list->peer_list,
						   peer_entry);
		peer_entry = next_entry;
	}

	return;
fail:
	if (qdf_bitmap_empty(link_req_ctx->mlo_peer_id_pending_bmap,
			     MAX_MLO_PEER_ID)) {
		mlo_info("req completed for vdev %u", vdev_id);
		wlan_ptqm_notify_link_req_end(link_req_ctx);
		wlan_ptqm_migrate_list_destroy(&link_req_ctx->migration_list);
		qdf_spinlock_destroy(&link_req_ctx->ptqm_link_lock);
		wlan_ptqm_set_mlo_link_context(ml_dev, link_req_ctx->vdev,
					       NULL);
		wlan_objmgr_vdev_release_ref(link_req_ctx->vdev,
					     WLAN_MLO_MGR_ID);
		qdf_mem_free(link_req_ctx);
		return;
	}
}

static QDF_STATUS
wlan_ptqm_peer_migrate_req_add_internal(struct wlan_objmgr_vdev *vdev,
					struct wlan_mlo_peer_context *ml_peer,
					struct ptqm_peer_migrate_params *params,
					bool is_link_req)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;
	struct ptqm_migrate_peer_req_context *pending_req_ctx;
	struct ptqm_migrate_peer_req req = {0};
	QDF_STATUS status;
	bool new_pending_context = false;

	if (!ml_peer) {
		mlo_err("Null ml peer context");
		return QDF_STATUS_E_NULL_VALUE;
	}

	mlo_info("ML Peer " QDF_MAC_ADDR_FMT " id %u src %d dst %d f %d re %d",
		 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		 params->module_id, params->src_link_id, params->dst_link_id,
		 params->force_mig, params->allow_reelection);

	/*
	 * Create a new request
	 */
	req.module_id = params->module_id;
	req.priority = params->module_id;
	req.is_link_req = is_link_req;
	req.user_data = params->user_data;
	req.begin = params->begin;
	req.end = params->end;
	req.src_link_id = params->src_link_id;
	req.dst_link_id = params->dst_link_id;
	req.force_mig = params->force_mig;
	req.allow_reelection = params->allow_reelection;

	ptqm_peer_ctx = wlan_ptqm_get_mlo_peer_context(ml_peer);
	if (!ptqm_peer_ctx) {
		mlo_err("Invalid ptqm peer context");
		return QDF_STATUS_E_NULL_VALUE;
	}

	qdf_spin_lock_bh(&ptqm_peer_ctx->ptqm_peer_lock);

	pending_req_ctx = ptqm_peer_ctx->pending_req_ctx;
	if (!pending_req_ctx) {
		/*
		 * No req is pending, create a new pending peer request context
		 */
		pending_req_ctx = qdf_mem_malloc(sizeof(*pending_req_ctx));
		if (!pending_req_ctx) {
			qdf_spin_unlock_bh(&ptqm_peer_ctx->ptqm_peer_lock);
			mlo_err("Unable to allocate pending req context");
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_zero(pending_req_ctx, sizeof(*pending_req_ctx));
		qdf_list_create(&pending_req_ctx->req_list,
				PTQM_MIGRATION_PENDING_PEER_REQ);

		ptqm_peer_ctx->pending_req_ctx = pending_req_ctx;
		new_pending_context = true;
		mlo_debug("Allocated new pending context");
	}

	/*
	 * Add the new request to the pending context
	 */
	status = wlan_ptqm_add_pending_context(ptqm_peer_ctx->pending_req_ctx,
					       &req);
	if (QDF_IS_STATUS_ERROR(status)) {
		if (new_pending_context) {
			mlo_debug("Free pending context");
			qdf_mem_free(ptqm_peer_ctx->pending_req_ctx);
			ptqm_peer_ctx->pending_req_ctx = NULL;
		}
		qdf_spin_unlock_bh(&ptqm_peer_ctx->ptqm_peer_lock);
		mlo_err("Unable to add req to pending context");
		return status;
	}

	wlan_ptqm_schedule_peer_migrate_req(ml_peer);

	qdf_spin_unlock_bh(&ptqm_peer_ctx->ptqm_peer_lock);

	return QDF_STATUS_SUCCESS;
}

/*
 * Public APIs
 */
QDF_STATUS
wlan_ptqm_peer_migrate_req_add(struct wlan_objmgr_vdev *vdev,
			       struct wlan_mlo_peer_context *ml_peer,
			       struct ptqm_peer_migrate_params *params)
{
	return wlan_ptqm_peer_migrate_req_add_internal(vdev, ml_peer, params,
						       false);
}

void
wlan_ptqm_peer_migrate_completion(struct wlan_mlo_dev_context *ml_dev,
				  struct wlan_mlo_peer_context *ml_peer,
				  uint8_t status)
{
	struct ptqm_migrate_peer_context *ptqm_peer_ctx;
	struct ptqm_migrate_peer_req_context *active_req_ctx;

	mlo_info("ML Peer " QDF_MAC_ADDR_FMT "status : %s",
		 QDF_MAC_ADDR_REF(ml_peer->peer_mld_addr.bytes),
		 status ? "failure" : "success");

	ptqm_peer_ctx = wlan_ptqm_get_mlo_peer_context(ml_peer);
	if (!ptqm_peer_ctx) {
		mlo_err("peer ptqm context not found");
		return;
	}

	qdf_spin_lock_bh(&ptqm_peer_ctx->ptqm_peer_lock);

	active_req_ctx = ptqm_peer_ctx->active_req_ctx;
	if (!active_req_ctx) {
		qdf_spin_unlock_bh(&ptqm_peer_ctx->ptqm_peer_lock);
		mlo_err("No peer req in active ctx");
		return;
	}

	/*
	 * Notify end on the completion of migration
	 */
	wlan_ptqm_notify_end(ml_peer, active_req_ctx, status);

	wlan_ptqm_peer_req_context_free(ptqm_peer_ctx->active_req_ctx);
	ptqm_peer_ctx->active_req_ctx = NULL;

	/*
	 * Call scheduler function
	 */
	wlan_ptqm_schedule_peer_migrate_req(ml_peer);

	qdf_spin_unlock_bh(&ptqm_peer_ctx->ptqm_peer_lock);
}

qdf_export_symbol(wlan_ptqm_peer_migrate_completion);

QDF_STATUS
wlan_ptqm_link_migrate_req_add(struct wlan_objmgr_vdev *vdev,
			       struct ptqm_link_migrate_params *params)
{
	struct wlan_mlo_dev_context *dev_ctx;
	struct ptqm_migrate_link_req_context *link_req_ctx;
	struct peer_migrate_ptqm_multi_entries *migrate_list;
	uint8_t vdev_id;
	uint16_t peer_count;
	QDF_STATUS status;

	if (!vdev) {
		mlo_err("Invalid vdev");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_id = wlan_vdev_get_id(vdev);

	mlo_info("vdev_id %u module_id %u src_link_id %d force_mig %d",
		 vdev_id, params->module_id, params->src_link_id,
		 params->force_mig);

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLO_MGR_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Unable to get vdev ref");
		return status;
	}

	dev_ctx = vdev->mlo_dev_ctx;
	if (!dev_ctx) {
		mlo_err("Invalid mlo dev context");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	link_req_ctx = wlan_ptqm_get_mlo_link_context(dev_ctx, vdev);
	if (link_req_ctx) {
		mlo_err("Link request is already active");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_ALREADY;
	}

	/*
	 * Allocate link req context
	 */
	link_req_ctx = qdf_mem_malloc(sizeof(*link_req_ctx));
	if (!link_req_ctx) {
		mlo_err("Unable to allocate PTQM link context");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_zero(link_req_ctx, sizeof(*link_req_ctx));

	status = wlan_ptqm_set_mlo_link_context(dev_ctx, vdev, link_req_ctx);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Unable to set link context");
		qdf_mem_free(link_req_ctx);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return status;
	}

	link_req_ctx->vdev = vdev;
	link_req_ctx->module_id = params->module_id;
	link_req_ctx->src_link_id = params->src_link_id;
	link_req_ctx->begin = params->begin;
	link_req_ctx->end = params->end;
	link_req_ctx->user_data = params->user_data;
	link_req_ctx->link_disable = params->link_disable;
	link_req_ctx->force_mig = params->force_mig;

	qdf_spinlock_create(&link_req_ctx->ptqm_link_lock);

	mld_get_link_rssi(&link_req_ctx->rssi_data);

	migrate_list = &link_req_ctx->migration_list;

	status = wlan_ptqm_migrate_list_init(migrate_list,
					     &link_req_ctx->rssi_data);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlo_err("Unable to initialize peer migrate list");
		qdf_spinlock_destroy(&link_req_ctx->ptqm_link_lock);
		wlan_ptqm_set_mlo_link_context(dev_ctx, vdev, NULL);
		qdf_mem_free(link_req_ctx);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		return status;
	}

	/*
	 * Build peer list
	 */
	wlan_ptqm_migrate_list_create(vdev, migrate_list);

	peer_count = wlan_ptqm_migrate_list_peer_count(migrate_list);

	link_req_ctx->rsp_params.total_peer_count = peer_count;

	if (!peer_count) {
		mlo_info("No peer in the migration list");
		wlan_ptqm_notify_link_req_begin(link_req_ctx);
		wlan_ptqm_notify_link_req_end(link_req_ctx);
		wlan_ptqm_migrate_list_destroy(migrate_list);
		wlan_ptqm_set_mlo_link_context(dev_ctx, vdev, NULL);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLO_MGR_ID);
		qdf_spinlock_destroy(&link_req_ctx->ptqm_link_lock);
		qdf_mem_free(link_req_ctx);
		return QDF_STATUS_SUCCESS;
	}

	wlan_ptqm_notify_link_req_begin(link_req_ctx);

	wlan_ptqm_schedule_link_migrate_req(vdev, link_req_ctx);

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_SUPPORT_PRIMARY_LINK_MIGRATE */
