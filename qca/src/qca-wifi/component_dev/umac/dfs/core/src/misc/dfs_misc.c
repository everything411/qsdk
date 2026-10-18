/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2002-2006, Atheros Communications Inc.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: This file contains the miscellanous files
 *
 */

#include "dfs.h"
#include "../dfs_misc.h"
#include "dfs_zero_cac.h"
#include "wlan_dfs_mlme_api.h"
#include "ieee80211_mlme_dfs_interface.h"
#include "wlan_dfs_init_deinit_api.h"
#include "_ieee80211.h"
#include "ieee80211_var.h"
#include "ieee80211_mlme_dfs_dispatcher.h"
#include "ieee80211_channel.h"
#include "wlan_mlme_if.h"
#include "dfs_postnol_ucfg.h"
#include "wlan_dfs_lmac_api.h"
#include "dfs_process_radar_found_ind.h"
#include "wlan_dfs_utils_api.h"

#if defined(QCA_SUPPORT_DFS_CHAN_POSTNOL)
void dfs_postnol_attach(struct wlan_dfs *dfs)
{
	dfs->dfs_chan_postnol_mode = CH_WIDTH_INVALID;
}
#endif

#ifdef QCA_SUPPORT_DFS_CHAN_POSTNOL
void dfs_set_postnol_freq(struct wlan_dfs *dfs, qdf_freq_t postnol_freq)
{
	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
		 "dfs_chan_postnol_freq configured as %d", postnol_freq);

	dfs->dfs_chan_postnol_freq = postnol_freq;
}

#ifdef WLAN_FEATURE_11BE
static inline void dfs_set_postnol_mode_as_320(struct wlan_dfs *dfs)
{
	dfs->dfs_chan_postnol_mode = CH_WIDTH_320MHZ;
}
#else
static inline void dfs_set_postnol_mode_as_320(struct wlan_dfs *dfs)
{
	dfs->dfs_chan_postnol_mode = CH_WIDTH_INVALID;
}
#endif

void dfs_set_postnol_mode(struct wlan_dfs *dfs, uint16_t postnol_mode)
{
	if (dfs->dfs_chan_postnol_cfreq2) {
		dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
			 "postNOL cfreq2 has been set,reset it to change mode");
		return;
	}

	switch (postnol_mode) {
	case DFS_CHWIDTH_20_VAL:
		dfs->dfs_chan_postnol_mode = CH_WIDTH_20MHZ;
		break;
	case DFS_CHWIDTH_40_VAL:
		dfs->dfs_chan_postnol_mode = CH_WIDTH_40MHZ;
		break;
	case DFS_CHWIDTH_80_VAL:
		dfs->dfs_chan_postnol_mode = CH_WIDTH_80MHZ;
		break;
	case DFS_CHWIDTH_160_VAL:
		dfs->dfs_chan_postnol_mode = CH_WIDTH_160MHZ;
		break;
	case DFS_CHWIDTH_240_VAL:
	case DFS_CHWIDTH_320_VAL:
		dfs_set_postnol_mode_as_320(dfs);
		break;
	default:
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,
			"Invalid postNOL mode configured %d",
			postnol_mode);
		return;
	}

	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
		 "DFS postnol mode configured as %d",
		 dfs->dfs_chan_postnol_mode);
}

void dfs_set_postnol_cfreq2(struct wlan_dfs *dfs, qdf_freq_t postnol_cfreq2)
{
	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
		 "dfs_chan_postnol_cfreq2 configured as %d", postnol_cfreq2);

	dfs->dfs_chan_postnol_cfreq2 = postnol_cfreq2;

	if (postnol_cfreq2) {
		dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
			 "postNOL cfreq2 is set, changing mode to 80P80");
		dfs->dfs_chan_postnol_mode = CH_WIDTH_80P80MHZ;
	}
}

void dfs_get_postnol_freq(struct wlan_dfs *dfs, qdf_freq_t *postnol_freq)
{
	*postnol_freq = dfs->dfs_chan_postnol_freq;
}

void dfs_get_postnol_mode(struct wlan_dfs *dfs, uint8_t *postnol_mode)
{
	*postnol_mode = dfs->dfs_chan_postnol_mode;
}

void dfs_get_postnol_cfreq2(struct wlan_dfs *dfs, qdf_freq_t *postnol_cfreq2)
{
	*postnol_cfreq2 = dfs->dfs_chan_postnol_cfreq2;
}
#endif

#ifdef QCA_SUPPORT_DFS_CHAN_POSTNOL
#ifdef WLAN_FEATURE_11BE
static enum wlan_phymode dfs_get_11be_phymode(struct wlan_dfs *dfs)
{
	switch (dfs->dfs_chan_postnol_mode) {
	case CH_WIDTH_20MHZ:
		return WLAN_PHYMODE_11BEA_EHT20;
	case CH_WIDTH_40MHZ:
		return WLAN_PHYMODE_11BEA_EHT40;
	case CH_WIDTH_80MHZ:
		return WLAN_PHYMODE_11BEA_EHT80;
	case CH_WIDTH_160MHZ:
		return WLAN_PHYMODE_11BEA_EHT160;
	case CH_WIDTH_320MHZ:
		return WLAN_PHYMODE_11BEA_EHT320;
	default:
		return WLAN_PHYMODE_MAX;
	}
}
#else
static inline enum wlan_phymode dfs_get_11be_phymode(struct wlan_dfs *dfs)
{
	return WLAN_PHYMODE_MAX;
}
#endif

bool dfs_switch_to_postnol_chan_if_nol_expired(struct wlan_dfs *dfs)
{
	struct dfs_channel chan;
	struct dfs_channel *curchan = dfs->dfs_curchan;
	bool is_curchan_11ac = false, is_curchan_11axa = false;
	bool is_curchan_11be = false;
	enum wlan_phymode postnol_phymode;

	if (!dfs->dfs_chan_postnol_freq)
		return false;

	if (WLAN_IS_CHAN_11AC(curchan))
		is_curchan_11ac = true;
	else if (WLAN_IS_CHAN_11AXA(curchan))
		is_curchan_11axa = true;
	else if (WLAN_IS_CHAN_11BE(curchan))
		is_curchan_11be = true;

	if (is_curchan_11be) {
		postnol_phymode = dfs_get_11be_phymode(dfs);

		if (postnol_phymode == WLAN_PHYMODE_MAX) {
			dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,
				"Invalid postNOL mode set.");
			return false;
		}
	} else {
		switch (dfs->dfs_chan_postnol_mode) {
		case CH_WIDTH_20MHZ:
			if (is_curchan_11ac)
				postnol_phymode = WLAN_PHYMODE_11AC_VHT20;
			else if (is_curchan_11axa)
				postnol_phymode = WLAN_PHYMODE_11AXA_HE20;
			else
				return false;
			break;
		case CH_WIDTH_40MHZ:
			if (is_curchan_11ac)
				postnol_phymode = WLAN_PHYMODE_11AC_VHT40;
			else if (is_curchan_11axa)
				postnol_phymode = WLAN_PHYMODE_11AXA_HE40;
			else
				return false;
			break;
		case CH_WIDTH_80MHZ:
			if (is_curchan_11ac)
				postnol_phymode = WLAN_PHYMODE_11AC_VHT80;
			else if (is_curchan_11axa)
				postnol_phymode = WLAN_PHYMODE_11AXA_HE80;
			else
				return false;
			break;
		case CH_WIDTH_160MHZ:
			if (is_curchan_11ac)
				postnol_phymode = WLAN_PHYMODE_11AC_VHT160;
			else if (is_curchan_11axa)
				postnol_phymode = WLAN_PHYMODE_11AXA_HE160;
			else
				return false;
			break;
		case CH_WIDTH_80P80MHZ:
			if (is_curchan_11ac)
				postnol_phymode = WLAN_PHYMODE_11AC_VHT80_80;
			else if (is_curchan_11axa)
				postnol_phymode = WLAN_PHYMODE_11AXA_HE80_80;
			else
				return false;
			break;
		default:
			dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,
				"Invalid postNOL mode set.");
			return false;
		}
	}

	qdf_mem_zero(&chan, sizeof(struct dfs_channel));
	if (QDF_STATUS_SUCCESS !=
		dfs_mlme_find_dot11_chan_for_freq(
			dfs->dfs_pdev_obj,
			dfs->dfs_chan_postnol_freq,
			dfs->dfs_chan_postnol_cfreq2,
			postnol_phymode,
			&chan.dfs_ch_freq,
			&chan.dfs_ch_flags,
			&chan.dfs_ch_flagext,
			&chan.dfs_ch_ieee,
			&chan.dfs_ch_vhtop_ch_freq_seg1,
			&chan.dfs_ch_vhtop_ch_freq_seg2,
			&chan.dfs_ch_mhz_freq_seg1,
			&chan.dfs_ch_mhz_freq_seg2)) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,
			"Channel %d not found for mode %d and cfreq2 %d",
			dfs->dfs_chan_postnol_freq,
			postnol_phymode,
			dfs->dfs_chan_postnol_cfreq2);
		return false;
	}
	if (WLAN_IS_CHAN_RADAR(dfs, &chan))
		return false;

	if (global_dfs_to_mlme.mlme_postnol_chan_switch)
		global_dfs_to_mlme.mlme_postnol_chan_switch(
				dfs->dfs_pdev_obj,
				dfs->dfs_chan_postnol_freq,
				dfs->dfs_chan_postnol_cfreq2,
				postnol_phymode);
	return true;
}
#endif

#ifdef QCA_HW_MODE_SWITCH
bool dfs_is_hw_mode_switch_in_progress(struct wlan_dfs *dfs)
{
	return lmac_dfs_is_hw_mode_switch_in_progress(dfs->dfs_pdev_obj);
}


void dfs_complete_deferred_tasks(struct wlan_dfs *dfs)
{
	if (dfs->dfs_defer_params.is_radar_detected) {
		/* Handle radar event that was deferred and free the temporary
		 * storage of the radar event parameters.
		 */
		dfs_process_radar_ind(dfs, dfs->dfs_defer_params.radar_params);
		qdf_mem_free(dfs->dfs_defer_params.radar_params);
		dfs->dfs_defer_params.is_radar_detected = false;
	} else if (dfs->dfs_defer_params.is_cac_completed) {
		/* Handle CAC completion event that was deferred for HW mode
		 * switch.
		 */
		dfs_process_cac_completion(dfs);
		dfs->dfs_defer_params.is_cac_completed = false;
	}
}

void dfs_init_tmp_psoc_nol(struct wlan_dfs *dfs, uint8_t num_radios)
{
	struct dfs_soc_priv_obj *dfs_soc_obj = dfs->dfs_soc_obj;

	if (WLAN_UMAC_MAX_PDEVS < num_radios) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,
			"num_radios (%u) exceeds limit", num_radios);
		return;
	}

	/* Allocate the temporary psoc NOL copy structure for the number
	 * of radios provided.
	 */
	dfs_soc_obj->dfs_psoc_nolinfo =
		qdf_mem_malloc(sizeof(struct dfsreq_nolinfo) * num_radios);
}

void dfs_deinit_tmp_psoc_nol(struct wlan_dfs *dfs)
{
	struct dfs_soc_priv_obj *dfs_soc_obj = dfs->dfs_soc_obj;

	if (!dfs_soc_obj->dfs_psoc_nolinfo)
		return;

	qdf_mem_free(dfs_soc_obj->dfs_psoc_nolinfo);
	dfs_soc_obj->dfs_psoc_nolinfo = NULL;
}

void dfs_save_dfs_nol_in_psoc(struct wlan_dfs *dfs,
			      uint8_t pdev_id)
{
	struct dfs_soc_priv_obj *dfs_soc_obj = dfs->dfs_soc_obj;
	struct dfsreq_nolinfo tmp_nolinfo, *nolinfo;
	uint32_t i, num_chans = 0;

	if (!dfs->dfs_nol_count)
		return;

	if (!dfs_soc_obj->dfs_psoc_nolinfo)
		return;

	nolinfo = &dfs_soc_obj->dfs_psoc_nolinfo[pdev_id];
	/* Fetch the NOL entries for the DFS object. */
	dfs_getnol(dfs, &tmp_nolinfo);

	/* nolinfo might already have some data. Do not overwrite it */
	num_chans = nolinfo->dfs_ch_nchans;
	for (i = 0; i < tmp_nolinfo.dfs_ch_nchans; i++) {
		/* Figure out the completed duration of each NOL. */
		uint32_t nol_completed_ms = qdf_do_div(
			qdf_get_monotonic_boottime() -
			tmp_nolinfo.dfs_nol[i].nol_start_us, 1000);

		nolinfo->dfs_nol[num_chans] = tmp_nolinfo.dfs_nol[i];
		/* Remember the remaining NOL time in the timeout
		 * variable.
		 */
		nolinfo->dfs_nol[num_chans++].nol_timeout_ms -=
			nol_completed_ms;
	}

	nolinfo->dfs_ch_nchans = num_chans;
}

void dfs_reinit_nol_from_psoc_copy(struct wlan_dfs *dfs,
				   uint8_t pdev_id,
				   uint16_t low_5ghz_freq,
				   uint16_t high_5ghz_freq)
{
	struct dfs_soc_priv_obj *dfs_soc_obj = dfs->dfs_soc_obj;
	struct dfsreq_nolinfo *nol, req_nol;
	uint8_t i, j = 0;

	if (!dfs_soc_obj->dfs_psoc_nolinfo)
		return;

	if (!dfs_soc_obj->dfs_psoc_nolinfo[pdev_id].dfs_ch_nchans)
		return;

	nol = &dfs_soc_obj->dfs_psoc_nolinfo[pdev_id];

	for (i = 0; i < nol->dfs_ch_nchans; i++) {
		uint16_t tmp_freq = nol->dfs_nol[i].nol_freq;

		/* Add to nol only if within the tgt pdev's frequency range. */
		if ((low_5ghz_freq < tmp_freq) && (high_5ghz_freq > tmp_freq)) {
			/* The NOL timeout value in each entry points to the
			 * remaining time of the NOL. This is to indicate that
			 * the NOL entries are paused and are not left to
			 * continue.
			 * While adding these NOL, update the start ticks to
			 * current time to avoid losing entries which might
			 * have timed out during the pause and resume mechanism.
			 */
			nol->dfs_nol[i].nol_start_us =
				qdf_get_monotonic_boottime();
			req_nol.dfs_nol[j++] = nol->dfs_nol[i];
		}
	}
	dfs_set_nol(dfs, req_nol.dfs_nol, j);
}
#endif

#ifdef CONFIG_HOST_FIND_CHAN
bool wlan_is_chan_radar(struct wlan_dfs *dfs, struct dfs_channel *chan)
{
	qdf_freq_t sub_freq_list[MAX_20MHZ_SUBCHANS];
	uint8_t n_subchans, i;

	if (!chan || !WLAN_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(chan))
		return false;

	n_subchans = dfs_get_bonding_channel_without_seg_info_for_freq(
				chan,
				sub_freq_list);

	for (i = 0; i < n_subchans; i++) {
		if (wlan_reg_is_nol_for_freq(dfs->dfs_pdev_obj,
					     sub_freq_list[i]))
			return true;
	}

	return false;
}

bool wlan_is_chan_history_radar(struct wlan_dfs *dfs, struct dfs_channel *chan)
{
	qdf_freq_t sub_freq_list[MAX_20MHZ_SUBCHANS];
	uint8_t n_subchans, i;

	if (!chan || !WLAN_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(chan))
		return false;

	n_subchans = dfs_get_bonding_channel_without_seg_info_for_freq(
				chan,
				sub_freq_list);

	for (i = 0; i < n_subchans; i++) {
		if (wlan_reg_is_nol_hist_for_freq(dfs->dfs_pdev_obj,
						  sub_freq_list[i]))
			return true;
	}

	return false;
}
#endif /* CONFIG_HOST_FIND_CHAN */

#if defined(WLAN_DISP_CHAN_INFO)
void dfs_deliver_cac_state_events(struct wlan_dfs *dfs)
{
	struct dfs_channel *chan;

	chan = dfs->dfs_curchan;
	dfs_send_dfs_events_for_chan(dfs, chan, WLAN_EV_CAC_STARTED);
	dfs_update_cac_elements(dfs, NULL, 0, chan, WLAN_EV_CAC_STARTED);

	/* The current channel has started CAC, so the CAC_DONE state of the
	 * previous channel has to reset. So deliver the CAC_RESET event on
	 * all the sub-channels of previous dfs channel. If the previous
	 * channel was non-dfs channel the CAC_RESET event need not be
	 * delivered. Since in the case of ETSI domains retain the CAC_DONE
	 * state CAC_RESET event need not be delivered for this case too.
	 */
	if (!WLAN_IS_PRIMARY_OR_SECONDARY_CHAN_DFS(dfs->dfs_prevchan) ||
	    utils_get_dfsdomain(dfs->dfs_pdev_obj) == DFS_ETSI_DOMAIN)
		return;

	chan = dfs->dfs_prevchan;

	/**
	 * Do not change the state of NOL infected channels to
	 * "CAC Required" within the NOL duration.
	 */
	if (WLAN_IS_CHAN_RADAR(dfs, chan))
		return;

	dfs_update_cac_elements(dfs, NULL, 0, dfs->dfs_prevchan, WLAN_EV_CAC_RESET);
	dfs_send_dfs_events_for_chan(dfs, chan, WLAN_EV_CAC_RESET);
}
#endif

#ifdef QCA_DFS_BW_EXPAND
void dfs_set_bw_expand_channel(struct wlan_dfs *dfs,
			       qdf_freq_t user_freq,
			       enum wlan_phymode user_mode)
{
	dfs->dfs_bw_expand_target_freq = user_freq;
	dfs->dfs_bw_expand_des_mode = user_mode;
}

void dfs_set_bw_expand(struct wlan_dfs *dfs,
		      bool bw_expand)
{
	dfs->dfs_use_bw_expand = bw_expand;
	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS, "BW Expand Feature is %s ",
		 (bw_expand) ? "set" : "disabled");
}

void dfs_get_bw_expand(struct wlan_dfs *dfs,
		      bool *bw_expand)
{
	(*bw_expand) = dfs->dfs_use_bw_expand;
}
#endif /* QCA_DFS_BW_EXPAND */

#ifdef QCA_DFS_BW_PUNCTURE
void dfs_set_dfs_puncture(struct wlan_dfs *dfs,
			  bool is_dfs_punc_en)
{
	dfs->dfs_use_puncture = is_dfs_punc_en;
	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS, "DFS Puncturing Feature is %s ",
		 (is_dfs_punc_en) ? "enabled" : "disabled");
}

void dfs_get_dfs_puncture(struct wlan_dfs *dfs,
			  bool *is_dfs_punc_en)
{
	*is_dfs_punc_en = dfs->dfs_use_puncture;
}
#endif /* QCA_DFS_BW_PUNCTURE */

#if defined(WLAN_DISP_CHAN_INFO)
/**
 * dfs_clear_cac_elems() - Clear the dfs_cacelem data structure.
 * @dfs_cac_cur_elem: Pointer to struct dfs_cacelem
 *
 * Return: None
 */
static void
dfs_clear_cac_elems(struct dfs_cacelem *dfs_cac_cur_elem)
{
	qdf_mem_zero(dfs_cac_cur_elem, sizeof(struct dfs_cacelem));
}

/**
 * dfs_fill_cac_comp_params() - Store the timestamp of CAC completion.
 * This is called immediately after CAC is completed for the element.
 * @dfs_cac_cur_elem: Pointer to struct dfs_cacelem
 *
 * Return: None
 */
static void
dfs_fill_cac_comp_params(struct dfs_cacelem *dfs_cac_cur_elem)
{
	dfs_cac_cur_elem->cac_completed_time =  qdf_get_monotonic_boottime();
}

/**
 * dfs_fill_cac_start_params() - Fill CAC start params in struct dfs_cacelem.
 * This is called when CAC is just started.
 * @dfs_cac_elem: Pointer to struct dfs_cacelem
 *
 * Return: None
 */
static void
dfs_fill_cac_start_params(struct dfs_cacelem *dfs_cac_elem)
{
	dfs_cac_elem->cac_start_us = qdf_get_monotonic_boottime();
	dfs_cac_elem->cac_completed_time = 0;
}

/**
 * dfs_update_cac_elem() - Fill dfs_cacelem data structure based on the
 * input CAC events.
 * @dfs: Pointer to struct wlan_dfs
 * @dfs_cac_cur_elem: Pointer to struct dfs_cacelem
 * @dfs_ev: DFS event
 *
 * Return: None
 */
static void
dfs_update_cac_elem(struct wlan_dfs *dfs, struct dfs_cacelem *dfs_cac_cur_elem,
		    enum WLAN_DFS_EVENTS dfs_ev)
{
	switch (dfs_ev) {
	case WLAN_EV_CAC_STARTED:
		dfs_fill_cac_start_params(dfs_cac_cur_elem);
		break;
	case WLAN_EV_CAC_COMPLETED:
		dfs_fill_cac_comp_params(dfs_cac_cur_elem);
		break;
	case WLAN_EV_NOL_STARTED:
	case WLAN_EV_CAC_RESET:
		dfs_clear_cac_elems(dfs_cac_cur_elem);
		break;
	default:
		dfs_debug(dfs, WLAN_DEBUG_DFS, "unknown event %u received", dfs_ev);
		break;
	}

	dfs_debug(dfs, WLAN_DEBUG_DFS, "event: %u", dfs_ev);
}

QDF_STATUS
dfs_update_cac_elements(struct wlan_dfs *dfs, uint16_t *freq_list,
			uint8_t num_chan, struct dfs_channel *dfs_chan,
			enum WLAN_DFS_EVENTS dfs_ev)
{
	qdf_freq_t cac_subchan_freq[MAX_20MHZ_SUBCHANS];
	qdf_freq_t *sub_chan_freq;
	uint8_t num_cac_subchan = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct dfs_cacelem *dfs_cac_cur_elem;
	uint8_t i;
	int8_t index;

	if (dfs_chan) {
		num_cac_subchan = dfs_find_dfs_sub_channels_for_freq(dfs,
								     dfs_chan,
								     cac_subchan_freq);
		sub_chan_freq = cac_subchan_freq;
	} else if (freq_list) {
		sub_chan_freq = freq_list;
		num_cac_subchan = num_chan;
	}

	if (!num_cac_subchan) {
		dfs_debug(dfs, WLAN_DEBUG_DFS, "number of cac subchans is 0");
		return QDF_STATUS_E_INVAL;
	}
	for (i = 0; i < num_cac_subchan; i++) {
		if (!wlan_reg_is_dfs_for_freq(dfs->dfs_pdev_obj,
					      sub_chan_freq[i])) {
			dfs_debug(dfs, WLAN_DEBUG_DFS, "sub chan freq: %u is non_dfs",
				  sub_chan_freq[i]);
			continue;
		}
		utils_dfs_convert_freq_to_index(sub_chan_freq[i], &index);
		if (index == INVALID_INDEX) {
			dfs_debug(dfs, WLAN_DEBUG_DFS, "Index for freq: %u is invalid",
				  sub_chan_freq[i]);
			return QDF_STATUS_E_INVAL;
		}
		dfs_cac_cur_elem = &dfs->dfs_cacelems[index];
		dfs_update_cac_elem(dfs, dfs_cac_cur_elem, dfs_ev);
	}
	return status;
}

/**
 * dfs_compute_rem_time() - Given the start time (in us) and timeout (in ms),
 * compute the remaining time.
 * @start_time_us: Start time in us
 * @timeout_ms: Timeout in ms
 *
 * Return: Remaining time in seconds
 */
static uint32_t
dfs_compute_rem_time(uint64_t start_time_us, uint32_t timeout_ms)
{
	uint32_t diff_ms;

	diff_ms = qdf_do_div(qdf_get_monotonic_boottime() - start_time_us, 1000);
	if (timeout_ms > diff_ms)
		diff_ms = timeout_ms - diff_ms;
	else
		diff_ms = 0;
	/* Convert to seconds */
	return qdf_do_div(diff_ms, 1000);
}

/**
 * dfs_get_rem_nol_time() - If the input freq is an NOL freq, find out
 * the remaining NOL time. If the input freq is not in NOL, set remaining
 * NOL timeout as 0.
 *
 * @dfs: Pointer to struct wlan_dfs
 * @freq: Frequency in MHz
 *
 * Return: Remaining NOL time in seconds.
 */
static uint32_t
dfs_get_rem_nol_time(struct wlan_dfs *dfs, qdf_freq_t freq)
{
	struct dfs_nolelem *nol = dfs->dfs_nol;

	while (nol) {
		if (nol->nol_freq == freq)
			break;
		nol = nol->nol_next;
	}

	if (nol)
		return dfs_compute_rem_time(nol->nol_start_us,
					    nol->nol_timeout_ms);
	return 0;
}

/**
 * dfs_get_rem_cac_and_nol_time() - Get remaining CAC and NOL time.
 * @dfs: Pointer to struct wlan_dfs
 * @dfs_elem: Pointer to struct dfs_cacelem
 * @dfs_state: DFS state
 * @rem_cac_time: Pointer to remaining cac time
 * @rem_nol_time: Pointer to remaining NOL time
 * @freq: Frequency in MHz
 *
 * Return: None
 */
#define TIME_IN_MS 1000
static void
dfs_get_rem_cac_and_nol_time(struct wlan_dfs *dfs,
			     struct dfs_cacelem *dfs_elem,
			     enum channel_dfs_state dfs_state,
			     uint32_t *rem_cac_time,
			     uint32_t *rem_nol_time,
			     qdf_freq_t freq)
{
	struct dfs_channel *chan = dfs->dfs_curchan;
	uint16_t cac_timeout;

	*rem_nol_time = 0;
	*rem_cac_time = 0;

	switch (dfs_state) {
	case CH_DFS_S_CAC_STARTED:
		cac_timeout = dfs_mlme_get_cac_timeout_for_freq(dfs->dfs_pdev_obj,
							  chan->dfs_ch_freq,
							  chan->dfs_ch_mhz_freq_seg2,
							  chan->dfs_ch_flags);
		*rem_cac_time = dfs_compute_rem_time(dfs_elem->cac_start_us,
						     cac_timeout * TIME_IN_MS);
		break;
	case CH_DFS_S_CAC_COMPLETED:
		/* rem_cac_time was already set to 0 before entering switch */
		break;
	case CH_DFS_S_NOL:
		*rem_nol_time = dfs_get_rem_nol_time(dfs, freq);
		break;
	default:
		dfs_debug(dfs, WLAN_DEBUG_DFS, "remaining cac/nol time is NA for state: %u",
			  dfs_state);
		break;
	}
}

void dfs_get_cac_nol_time(struct wlan_dfs *dfs, int8_t index,
			  uint32_t *rem_cac_time,
			  uint64_t *cac_comp_time, uint32_t *rem_nol_time,
			  qdf_freq_t freq)
{
	struct dfs_cacelem *dfs_cacelems = &dfs->dfs_cacelems[index];
	enum channel_dfs_state dfs_state = dfs->dfs_channel_state_array[index];

	dfs_get_rem_cac_and_nol_time(dfs, dfs_cacelems, dfs_state,
				     rem_cac_time, rem_nol_time, freq);

	*cac_comp_time = dfs_cacelems->cac_completed_time;
}

#endif
