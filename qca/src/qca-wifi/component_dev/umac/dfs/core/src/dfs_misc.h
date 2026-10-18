/*
 * Copyright (c) 2013, 2016-2020 The Linux Foundation.  All rights reserved.
 * Copyright (c) 2005-2006 Atheros Communications, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
e* copyright notice and this permission notice appear in all copies.
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
 * DOC: This file has header files of dfs_misc.c
 *
 */

#include "dfs.h"
#include "wlan_dfs_ucfg_api.h"
#include "wlan_lmac_if_def.h"
#ifdef CONFIG_HOST_FIND_CHAN
#include <wlan_reg_channel_api.h>
#endif

#ifdef QCA_SUPPORT_DFS_CHAN_POSTNOL
/**
 * dfs_set_postnol_freq() - DFS API to set postNOL frequency.
 * @dfs: Pointer to wlan_dfs object.
 * @postnol_freq: PostNOL frequency value configured by the user.
 */
void dfs_set_postnol_freq(struct wlan_dfs *dfs, qdf_freq_t postnol_freq);

/**
 * dfs_set_postnol_mode() - DFS API to set postNOL mode.
 * @dfs: Pointer to wlan_dfs object.
 * @postnol_mode: PostNOL frequency value configured by the user.
 */
void dfs_set_postnol_mode(struct wlan_dfs *dfs, uint16_t postnol_mode);

/**
 * dfs_set_postnol_cfreq2() - DFS API to set postNOL secondary center frequency.
 * @dfs: Pointer to wlan_dfs object.
 * @postnol_cfreq2: PostNOL secondary center frequency value configured by the
 * user.
 */
void dfs_set_postnol_cfreq2(struct wlan_dfs *dfs, qdf_freq_t postnol_cfreq2);

/**
 * dfs_get_postnol_freq() - DFS API to get postNOL frequency.
 * @dfs: Pointer to wlan_dfs object.
 * @postnol_freq: PostNOL frequency value configured by the user.
 */
void dfs_get_postnol_freq(struct wlan_dfs *dfs, qdf_freq_t *postnol_freq);

/**
 * dfs_get_postnol_mode() - DFS API to get postNOL mode.
 * @dfs: Pointer to wlan_dfs object.
 * @postnol_mode: PostNOL frequency value configured by the user.
 */
void dfs_get_postnol_mode(struct wlan_dfs *dfs, uint8_t *postnol_mode);

/**
 * dfs_get_postnol_cfreq2() - DFS API to get postNOL secondary center frequency.
 * @dfs: Pointer to wlan_dfs object.
 * @postnol_cfreq2: PostNOL secondary center frequency value configured by the
 * user.
 */
void dfs_get_postnol_cfreq2(struct wlan_dfs *dfs, qdf_freq_t *postnol_cfreq2);
#else
static inline void
dfs_set_postnol_freq(struct wlan_dfs *dfs, qdf_freq_t postnol_freq)
{
}

static inline void
dfs_set_postnol_mode(struct wlan_dfs *dfs, uint16_t postnol_mode)
{
}

static inline void
dfs_set_postnol_cfreq2(struct wlan_dfs *dfs, qdf_freq_t postnol_cfreq2)
{
}

static inline void
dfs_get_postnol_freq(struct wlan_dfs *dfs, qdf_freq_t *postnol_freq)
{
	*postnol_freq = 0;
}

static inline void
dfs_get_postnol_mode(struct wlan_dfs *dfs, uint8_t *postnol_mode)
{
	*postnol_mode = CH_WIDTH_INVALID;
}

static inline void
dfs_get_postnol_cfreq2(struct wlan_dfs *dfs, qdf_freq_t *postnol_cfreq2)
{
	*postnol_cfreq2 = 0;
}
#endif /* QCA_SUPPORT_DFS_CHAN_POSTNOL */

#ifdef QCA_DFS_BW_EXPAND
/**
 * dfs_set_bw_expand_channel() - DFS API to set user frequency and user
 *                               configured phymode.
 * @dfs: Pointer to wlan_dfs object.
 * @user_freq: frequency value configured by the user.
 * @user_mode: Phymode value configured by the user.
 *
 * Return: Nothing.
 */
void dfs_set_bw_expand_channel(struct wlan_dfs *dfs,
			       qdf_freq_t user_freq,
			       enum wlan_phymode user_mode);

/**
 * dfs_set_bw_expand() - Set or unset BW Expansion feature.
 * @dfs: Pointer to wlan_dfs structure.
 * @bw_expand: - Configure BW Expansion feature.
 *
 * Return: Nothing.
 */
void dfs_set_bw_expand(struct wlan_dfs *dfs,
		       bool bw_expand);

/**
 * dfs_get_bw_expand() - Get the value of BW Expansion feature.
 * @dfs: Pointer to wlan_dfs structure.
 * @bw_expand: - Read and store the value of BW Expansion feature.
 *
 * Return: Nothing.
 */
void dfs_get_bw_expand(struct wlan_dfs *dfs,
		       bool *bw_expand);
#endif /* QCA_DFS_BW_EXPAND */

#ifdef QCA_DFS_BW_PUNCTURE
/**
 * dfs_set_dfs_puncture() - Set or unset DFS puncturing feature.
 * @dfs: Pointer to wlan_dfs structure.
 * @is_dfs_punc_en: - Configure DFS puncturing feature.
 *
 * Return: Nothing.
 */
void dfs_set_dfs_puncture(struct wlan_dfs *dfs,
			  bool is_dfs_punc_en);

/**
 * dfs_get_dfs_puncture() - Get the value of DFS puncturing feature.
 * @dfs: Pointer to wlan_dfs structure.
 * @is_dfs_punc_en: - Read and store the value of DFS puncturing feature.
 *
 * Return: Nothing.
 */
void dfs_get_dfs_puncture(struct wlan_dfs *dfs,
			  bool *is_dfs_punc_en);
#endif /* QCA_DFS_BW_PUNCTURE */

#ifdef WLAN_DISP_CHAN_INFO
/**
 * dfs_get_cac_nol_time() - Get CAC and NOL NOL time related values
 * @dfs: Pointer to wlan_dfs
 * @index: Channel index
 * @rem_cac_time: Pointer to remaining cac time field
 * @cac_comp_time: Pointer to cac completion time field
 * @rem_nol_time: Pointer to remaining nol time field
 * @freq: Frequency in MHz
 *
 * Return: None
 */
void dfs_get_cac_nol_time(struct wlan_dfs *dfs, int8_t index,
			  uint32_t *rem_cac_time,
			  uint64_t *cac_comp_time, uint32_t *rem_nol_time,
			  qdf_freq_t freq);

#endif

#ifdef WLAN_FEATURE_11BE
/*
 * dfs_is_11be_supported() - Check if 11BE supported chip
 *
 * @dfs: Pointer to dfs structure.
 * return: True if the 11BE supported chip.
 */
bool dfs_is_11be_supported(struct wlan_dfs *dfs);
#else
static inline
bool dfs_is_11be_supported(struct wlan_dfs *dfs)
{
	return false;
}
#endif

