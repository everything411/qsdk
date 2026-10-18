/*
 * Copyright (c) 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2020,2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef _WLAN_RNR_H_
#define _WLAN_RNR_H_
#include <qdf_atomic.h>
#include <wlan_objmgr_pdev_obj.h>
#include <ieee80211_var.h>
#include <ol_if_athvar.h>

extern int ol_num_global_soc;

#define MAX_6GHZ_SOCS  3
#define MAX_6GHZ_LINKS MAX_6GHZ_SOCS

typedef QDF_STATUS (*wlan_rnr_handler)(void *arg1, void *arg2);

struct rnr_info_pdev {
	struct wlan_objmgr_pdev *pdev_6ghz;
};

struct rnr_info_soc {
	struct ol_ath_soc_softc *soc;
	struct rnr_info_pdev pdev_6ghz_ctx[WLAN_UMAC_MAX_PDEVS];
};

/**
 * struct rnr_global_info - Global context for RNR
 * @init_done:              Init done flag is set after initialised
 * @vdev_lower_band_cnt:    5ghz/2ghz vdev count
 * @vdev_6ghz_band_cnt:     6ghz vdev count
 * @pdev_6ghz_ctx:          6Ghz pdev context
 */
struct rnr_global_info {
#define EMA_AP_MAX_GROUPS 8
	bool init_done;
	qdf_atomic_t vdev_lower_band_cnt;
	qdf_atomic_t vdev_6ghz_band_cnt;
	/* This needs a revisit at a later point in time
	 * where it may hvae to be moved to to pdev level
	 */
	uint32_t rnr_mbss_idx_map[EMA_AP_MAX_GROUPS];
	struct rnr_info_soc soc_info[MAX_6GHZ_LINKS];
};

/**
 * wlan_rnr_lower_band_vdev_inc - Atomic increment of
 *				  global lower band vdev counter
 *
 * API to increment global lower band vdev counter
 *
 * Return:void
 */
void wlan_rnr_lower_band_vdev_inc(void);

/**
 * wlan_rnr_lower_band_vdev_dec - Atomic decrement of
 *				  global lower band vdev counter
 *
 * API to decrement global lower band vdev counter
 *
 * Return:void
 */
void wlan_rnr_lower_band_vdev_dec(void);

/**
 * wlan_rnr_6ghz_vdev_inc - Atomic increment of
 *			    6ghz vdev counter
 *
 * API to increment of 6Ghz vdev counter
 *
 * Return:void
 */
void wlan_rnr_6ghz_vdev_inc(void);

/**
 * wlan_rnr_6ghz_vdev_dec - Atomic decrement of
 *			    6ghz vdev counter
 *
 * API to decrement of 6Ghz vdev counter
 *
 * Return:void
 */
void wlan_rnr_6ghz_vdev_dec(void);

/**
 * wlan_rnr_register_soc - Register soc to RNR-module
 *
 * API to register soc to RNR-module
 *
 * Return: bool
 */
bool wlan_rnr_register_soc(struct ol_ath_soc_softc *soc);

/**
 * wlan_rnr_unregister_soc - Unregister soc from RNR-module
 *
 * API to unregister soc from RNR-module
 *
 * Return: bool
 */
bool wlan_rnr_unregister_soc(struct ol_ath_soc_softc *soc);

/**
 * wlan_rnr_unregister_soc - RNR-module-iterator to iterate
 *			       per-soc per-6ghz-pdev
 *
 * API to run specific 'handler' per-soc per-6gh-pdev in RNR-module
 *
 * Return: void
 */
void wlan_rnr_6ghz_iterator(wlan_rnr_handler handler, void *arg1, void *arg2);

/**
 * wlan_global_6ghz_pdev_get - Retrieve 6Ghz pdev pointer from speicified soc
 *
 * API to get 6Ghz pdev pointer
 *
 * Return: struct wlan_objmgr_pdev
 */
struct
wlan_objmgr_pdev *wlan_global_6ghz_pdev_get(uint8_t soc_id, uint8_t pdev_id);

/**
 * wlan_global_6ghz_pdev_set - Store 6Ghz pdev specific to a soc in
 *			       global context
 *
 * API to save 6Ghz pdev in global context for
 * faster access
 *
 * Return:void
 */
void wlan_global_6ghz_pdev_set(uint8_t soc_id, uint8_t pdev_id);

/**
 * wlan_global_6ghz_pdev_destroy - Delete 6Ghz pdev specific to a soc from
 *				   global context
 *
 * API to delete 6Ghz pdev in global context for
 * faster access
 *
 * Return:void
 */
void wlan_global_6ghz_pdev_destroy(uint8_t soc_id, uint8_t pdev_id);

/**
 * wlan_lower_band_ap_cnt_get - Get lower band AP count
 *
 * API to get lower band vdev from global context for
 * faster access
 *
 * Return: int32_t
 */
int32_t wlan_lower_band_ap_cnt_get(void);

/**
 * wlan_6ghz_band_ap_cnt_get - Get 6GHz band AP count
 *
 * API to get 6GHz band vdev from global context for
 * faster access
 *
 * Return: int32_t
 */
int32_t wlan_6ghz_band_ap_cnt_get(void);

/**
 * wlan_rnr_init_cnt - Initialize counters for
 *			6Ghz vdev and lower band vdev
 *
 * API to initialize atomic counters used for 6Ghz vdev
 * and lower band vdev
 *
 * Return: void
 */
void wlan_rnr_init_cnt(void);

/**
 * wlan_rnr_deinit_cnt - De-Initialize rnr counters context
 *                       for 6Ghz vdev and lower band vdev
 *
 * API to de-initialize init_done flag.
 *
 * Return: void
 */
void wlan_rnr_deinit_cnt(void);

/**
 * wlan_rnr_set_bss_idx - Set bit corresponding to bss index
 *
 * API to set bss index bitmap for adding Non Tx APs
 * not included in Mbss IE in the RNR IE
 *
 * Return: void
 */
void wlan_rnr_set_bss_idx(uint32_t bss_idx, uint8_t group_id);

/**
 * wlan_rnr_get_bss_idx - Get bit corresponding to bss index
 *
 * API to Get bss index bitmap for adding Non Tx APs
 * not included in Mbss IE in the RNR IE
 *
 * Return: void
 */
uint32_t wlan_rnr_get_bss_idx(uint8_t group_id);

/**
 * wlan_rnr_clear_bss_idx - Clear bits corresponding to bss index map
 *
 * API to clear bss index bitmap for adding Non Tx APs
 * not included in Mbss IE in the RNR IE
 *
 * Return: void
 */
void wlan_rnr_clear_bss_idx(uint8_t group_id);

#endif /* End of _WLAN_RNR_H_ */
