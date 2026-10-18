/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
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
 * @file wlan_reg_channel_api.c
 * @brief contains regulatory channel access functions
 */

#include <qdf_status.h>
#include <qdf_types.h>
#include <qdf_module.h>
#include <wlan_cmn.h>
#include <wlan_reg_channel_api.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <reg_priv_objs.h>
#include <reg_services_common.h>
#include "../../core/reg_channel.h"
#include <wlan_reg_services_api.h>
#include <reg_build_chan_list.h>

#ifdef CONFIG_HOST_FIND_CHAN
bool wlan_reg_is_phymode_chwidth_allowed(struct wlan_objmgr_pdev *pdev,
					 enum reg_phymode phy_in,
					 enum phy_ch_width ch_width,
					 qdf_freq_t primary_freq,
					 enum supported_6g_pwr_types
					 in_6g_pwr_mode,
					 uint16_t input_puncture_bitmap)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return false;
	}

	return reg_is_phymode_chwidth_allowed(pdev_priv_obj, phy_in, ch_width,
					      primary_freq,
					      in_6g_pwr_mode,
					      input_puncture_bitmap);
}

enum phy_ch_width
wlan_reg_get_max_channel_width(struct wlan_objmgr_pdev *pdev,
			  qdf_freq_t freq,
			  enum phy_ch_width ch_width,
			  enum supported_6g_pwr_types in_6g_pwr_mode,
			  uint16_t input_puncture_bitmap)
{
	return reg_get_max_channel_width(pdev,
					 freq,
					 ch_width,
					 in_6g_pwr_mode,
					 input_puncture_bitmap);
}

QDF_STATUS wlan_reg_get_max_phymode_and_chwidth(struct wlan_objmgr_pdev *pdev,
						enum reg_phymode *phy_in,
						enum phy_ch_width *ch_width)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	*phy_in = pdev_priv_obj->max_phymode;
	*ch_width = pdev_priv_obj->max_chwidth;

	return QDF_STATUS_SUCCESS;
}

void wlan_reg_get_txpow_ant_gain(struct wlan_objmgr_pdev *pdev,
				 qdf_freq_t freq,
				 uint32_t *txpower,
				 uint8_t *ant_gain,
				 struct regulatory_channel *reg_chan_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}
		if (reg_chan_list->center_freq == freq) {
			*txpower = reg_chan_list->tx_power;
			*ant_gain = reg_chan_list->ant_gain;
		}
}

static bool reg_get_chan_flags_freq1(struct regulatory_channel *reg_chan_list,
				     qdf_freq_t freq1,
				     uint16_t *sec_flags,
				     uint64_t *pri_flags)
{
	if (reg_chan_list->center_freq == freq1) {
		if (reg_chan_list->chan_flags &
		    REGULATORY_CHAN_RADAR) {
			*sec_flags |= WLAN_CHAN_DFS;
			*pri_flags |= WLAN_CHAN_PASSIVE;
			*sec_flags |= WLAN_CHAN_DISALLOW_ADHOC;
		} else if (reg_chan_list->chan_flags &
			   REGULATORY_CHAN_NO_IR) {
				/* For 2Ghz passive channels. */
				*pri_flags |= WLAN_CHAN_PASSIVE;
		}
		return true;
	}

	return false;
}

static bool reg_get_chan_flags_freq2(struct regulatory_channel *reg_chan_list,
				     qdf_freq_t freq2,
				     uint16_t *sec_flags,
				     uint64_t *pri_flags)
{
	if (freq2) {
		if (reg_chan_list->center_freq == freq2 &&
		    reg_chan_list->chan_flags &
		    REGULATORY_CHAN_RADAR) {
			*sec_flags |= WLAN_CHAN_DFS_CFREQ2;
			*sec_flags |= WLAN_CHAN_DISALLOW_ADHOC;
			return true;
		}
	}

	return false;
}

void wlan_reg_get_chan_flags(struct wlan_objmgr_pdev *pdev,
			     qdf_freq_t freq1,
			     qdf_freq_t freq2,
			     uint16_t *sec_flags,
			     uint64_t *pri_flags,
			     enum supported_6g_pwr_types in_6g_pwr_mode,
			     struct regulatory_channel *reg_chan_list)
{
	struct wlan_regulatory_pdev_priv_obj *pdev_priv_obj;
	struct regulatory_channel reg_chan_list_obj = {0};
	struct regulatory_channel *reg_freq2_chan_list = &reg_chan_list_obj;

	pdev_priv_obj = reg_get_pdev_obj(pdev);

	if (!IS_VALID_PDEV_REG_OBJ(pdev_priv_obj)) {
		reg_err("reg pdev priv obj is NULL");
		return;
	}

	reg_get_chan_flags_freq1(reg_chan_list,
				 freq1,
				 sec_flags,
				 pri_flags);

	if (WLAN_REG_IS_6GHZ_PSC_CHAN_FREQ(freq1))
		*sec_flags |= WLAN_CHAN_PSC;

	if (!freq2)
		return;

	*reg_freq2_chan_list = wlan_reg_get_reg_chan_list_based_on_freq(pdev,
									freq2,
									in_6g_pwr_mode);
	reg_get_chan_flags_freq2(reg_freq2_chan_list,
				 freq2,
				 sec_flags,
				 pri_flags);
}

void wlan_reg_set_chan_blocked(struct wlan_objmgr_pdev *pdev,
			       qdf_freq_t freq)
{
	reg_set_chan_blocked(pdev, freq);
}

bool wlan_reg_is_chan_blocked(struct wlan_objmgr_pdev *pdev,
			      qdf_freq_t freq)
{
	return reg_is_chan_blocked(pdev, freq);
}

void wlan_reg_clear_allchan_blocked(struct wlan_objmgr_pdev *pdev)
{
	 reg_clear_allchan_blocked(pdev);
}

void wlan_reg_set_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				 enum ht40_intol ht40intol_flags)
{
	reg_set_chan_ht40intol(pdev, freq, ht40intol_flags);
}

void wlan_reg_clear_chan_ht40intol(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq,
				   enum ht40_intol ht40intol_flags)
{
	reg_clear_chan_ht40intol(pdev, freq, ht40intol_flags);
}

bool wlan_reg_is_chan_ht40intol(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq,
				enum ht40_intol ht40intol_flags)
{
	return reg_is_chan_ht40intol(pdev, freq, ht40intol_flags);
}

void wlan_reg_clear_allchan_ht40intol(struct wlan_objmgr_pdev *pdev)
{
	 reg_clear_allchan_ht40intol(pdev);
}

bool wlan_reg_is_band_present(struct wlan_objmgr_pdev *pdev,
			      enum reg_wifi_band reg_band)
{
	return reg_is_band_present(pdev, reg_band);
}

qdf_export_symbol(wlan_reg_is_band_present);

#endif /* CONFIG_HOST_FIND_CHAN */

bool wlan_reg_is_nol_for_freq(struct wlan_objmgr_pdev *pdev, qdf_freq_t freq)
{
	   return reg_is_nol_for_freq(pdev, freq);
}

bool wlan_reg_is_nol_hist_for_freq(struct wlan_objmgr_pdev *pdev,
				   qdf_freq_t freq)
{
	return reg_is_nol_hist_for_freq(pdev, freq);
}

QDF_STATUS wlan_reg_get_ap_chan_list(struct wlan_objmgr_pdev *pdev,
				     struct regulatory_channel *chan_list,
				     bool get_cur_chan_list,
				     enum reg_6g_ap_type ap_pwr_type)
{
	return reg_get_ap_chan_list(pdev, chan_list, get_cur_chan_list,
					ap_pwr_type);
}
qdf_export_symbol(wlan_reg_get_ap_chan_list);

bool wlan_reg_is_freq_width_dfs(struct wlan_objmgr_pdev *pdev,
				qdf_freq_t freq,
				enum phy_ch_width ch_width)
{
	return reg_is_freq_width_dfs(pdev, freq, ch_width);
}

void wlan_reg_get_channel_params(struct wlan_objmgr_pdev *pdev,
				 qdf_freq_t freq,
				 qdf_freq_t sec_ch_2g_freq,
				 struct ch_params *ch_params,
				 enum supported_6g_pwr_types in_6g_pwr_mode)
{
	reg_get_channel_params(pdev, freq, sec_ch_2g_freq, ch_params,
			       in_6g_pwr_mode);
}

uint16_t wlan_reg_get_wmodes_and_max_chwidth(struct wlan_objmgr_pdev *pdev,
					     uint64_t *mode_select,
					     bool include_nol_chan)
{
    return reg_get_wmodes_and_max_chwidth(pdev, mode_select, include_nol_chan);
}

QDF_STATUS
wlan_reg_get_client_power_for_rep_ap(struct wlan_objmgr_pdev *pdev,
				     enum reg_6g_ap_type ap_pwr_type,
				     enum reg_6g_client_type client_type,
				     qdf_freq_t chan_freq,
				     bool *is_psd, uint16_t *reg_eirp,
				     uint16_t *reg_psd)
{
	return reg_get_client_power_for_rep_ap(pdev, ap_pwr_type, client_type,
					       chan_freq, is_psd, reg_eirp,
					       reg_psd);
}

#ifdef CONFIG_AFC_SUPPORT
QDF_STATUS
wlan_reg_get_client_psd_for_ap(struct wlan_objmgr_pdev *pdev,
			       enum reg_6g_ap_type ap_pwr_type,
			       enum reg_6g_client_type client_type,
			       qdf_freq_t chan_freq,
			       uint16_t *reg_psd)
{
	return reg_get_client_psd_for_ap(pdev, ap_pwr_type, client_type,
					 chan_freq, reg_psd);
}
#endif

struct regulatory_channel
wlan_reg_get_reg_chan_list_based_on_freq(struct wlan_objmgr_pdev *pdev,
					 qdf_freq_t freq,
					 enum supported_6g_pwr_types
					 in_6g_pwr_mode)
{
	return reg_get_reg_chan_list_based_on_freq(pdev,
						   freq,
						   in_6g_pwr_mode);
}

QDF_STATUS
wlan_reg_get_first_valid_freq(struct wlan_objmgr_pdev *pdev,
			      enum supported_6g_pwr_types
			      in_6g_pwr_mode,
			      qdf_freq_t *first_valid_freq,
			      int bw,
			      int sec_40_offset,
			      enum channel_enum start_chan,
			      enum channel_enum end_chan)
{
	return reg_get_first_valid_freq(pdev, in_6g_pwr_mode, first_valid_freq,
					bw, sec_40_offset, start_chan, end_chan);
}

bool wlan_reg_is_6g_domain_jp(struct wlan_objmgr_pdev *pdev)
{
	return reg_is_6g_domain_jp(pdev);
}

#ifdef CONFIG_BAND_6GHZ
QDF_STATUS
wlan_reg_get_max_reg_eirp_from_list(struct wlan_objmgr_pdev *pdev,
				    enum reg_6g_ap_type ap_pwr_type,
				    bool is_client_power_needed,
				    enum reg_6g_client_type client_type,
				    struct channel_power *chan_eirp_list,
				    uint8_t num_6g_chans)
{
	return reg_get_max_reg_eirp_from_list(pdev, ap_pwr_type,
					      is_client_power_needed,
					      client_type, chan_eirp_list,
					      num_6g_chans);
}
#endif

QDF_STATUS
wlan_quick_reg_set_ap_pwr_and_update_chan_list(struct wlan_objmgr_pdev *pdev,
					       enum reg_6g_ap_type ap_pwr_type)
{
	return reg_quick_set_ap_pwr_and_update_chan_list(pdev, ap_pwr_type);
}

bool
wlan_reg_is_freq_txable(struct wlan_objmgr_pdev *pdev,
			qdf_freq_t freq,
			enum supported_6g_pwr_types in_6ghz_pwr_mode)
{
	return reg_is_freq_txable(pdev, freq, in_6ghz_pwr_mode);
}

qdf_export_symbol(wlan_reg_is_freq_txable);
