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

#include <wlan_rnr.h>
#include <qdf_module.h>
#include <qdf_status.h>
#include <qdf_types.h>
#include <wlan_objmgr_psoc_obj.h>
#include <ieee80211_objmgr_priv.h>


struct rnr_global_info g_rnr_info;
uint8_t g_soc_id_map[MAX_6GHZ_SOCS] = {GLOBAL_SOC_SIZE, GLOBAL_SOC_SIZE, GLOBAL_SOC_SIZE};

void wlan_rnr_init_cnt(void)
{
	if (g_rnr_info.init_done)
		return;

	qdf_atomic_init(&(g_rnr_info.vdev_lower_band_cnt));
	qdf_atomic_init(&(g_rnr_info.vdev_6ghz_band_cnt));
	g_rnr_info.init_done = true;
}

qdf_export_symbol(wlan_rnr_init_cnt);

void wlan_rnr_deinit_cnt(void)
{
	g_rnr_info.init_done = false;
}
qdf_export_symbol(wlan_rnr_deinit_cnt);

void wlan_rnr_lower_band_vdev_inc(void)
{
	qdf_atomic_inc(&(g_rnr_info.vdev_lower_band_cnt));
}

qdf_export_symbol(wlan_rnr_lower_band_vdev_inc);

void wlan_rnr_lower_band_vdev_dec(void)
{
	qdf_atomic_dec(&(g_rnr_info.vdev_lower_band_cnt));
}

qdf_export_symbol(wlan_rnr_lower_band_vdev_dec);

void wlan_rnr_6ghz_vdev_inc(void)
{
	qdf_atomic_inc(&(g_rnr_info.vdev_6ghz_band_cnt));
}

qdf_export_symbol(wlan_rnr_6ghz_vdev_inc);

void wlan_rnr_6ghz_vdev_dec(void)
{
	qdf_atomic_dec(&(g_rnr_info.vdev_6ghz_band_cnt));
}

qdf_export_symbol(wlan_rnr_6ghz_vdev_dec);

/* Return value MAX_6GHZ_LINKS should be treated as incorrect
 * local-id by caller.
 */
static uint8_t wlan_rnr_get_local_socid(uint8_t i_soc_id)
{
	uint8_t i = 0;

	while (i < MAX_6GHZ_LINKS) {
		if (g_soc_id_map[i] == i_soc_id)
			break;
		i++;
	}

	return i;
}

bool wlan_rnr_register_soc(struct ol_ath_soc_softc *soc)
{
	uint8_t i;

	if (soc->soc_idx > ol_num_global_soc)
		return false;

	for (i = 0; i < MAX_6GHZ_LINKS; i++) {
		if (g_soc_id_map[i] == soc->soc_idx ||
		   g_soc_id_map[i] == GLOBAL_SOC_SIZE)
			break;
	}

	if (i >= MAX_6GHZ_LINKS) {
		qdf_err("Fatal error - no slot found in rnr module"
				"  for this 6Ghz-SOC: %d", soc->soc_idx);
		return false;
	}

	if (g_soc_id_map[i] == soc->soc_idx) {
		qdf_err("soc_idx: %d is already present. local_soc_id"
				" is :%d", soc->soc_idx, i);
		return true;
	}

	if (g_soc_id_map[i] == GLOBAL_SOC_SIZE) {
		qdf_info("local id for soc_idx: %d is %d", soc->soc_idx, i);

		/* soc_id not already present in soc_id map */
		g_soc_id_map[i] = soc->soc_idx;

		g_rnr_info.soc_info[i].soc = soc;
	}

	return true;
}

qdf_export_symbol(wlan_rnr_register_soc);

bool wlan_rnr_unregister_soc(struct ol_ath_soc_softc *soc)
{
	uint8_t i;

	/* Find idx in map where the soc_id is present */
	for (i = 0; i < MAX_6GHZ_LINKS; i++) {
		if (g_soc_id_map[i] == soc->soc_idx)
			break;
	}

	if (i >= MAX_6GHZ_LINKS)
		return false;

	if (g_soc_id_map[i] != soc->soc_idx)
		return false;

	if (g_rnr_info.soc_info[i].soc != soc)
		return false;

	/* Left-shift elements starting from idx i in g_soc_id_map */
	while (i < MAX_6GHZ_LINKS - 1) {
		g_soc_id_map[i] = g_soc_id_map[i + 1];
		g_rnr_info.soc_info[i].soc = g_rnr_info.soc_info[i + 1].soc;
		i++;
	}

	if (i == MAX_6GHZ_LINKS - 1) {
		g_soc_id_map[i] = GLOBAL_SOC_SIZE;
		g_rnr_info.soc_info[i].soc = NULL;
	}

	return true;
}

qdf_export_symbol(wlan_rnr_unregister_soc);

void wlan_rnr_6ghz_iterator(wlan_rnr_handler handler, void *arg1, void *arg2)
{
	struct ieee80211com *ic = NULL;
	uint8_t i, j;

	for (i = 0; i < MAX_6GHZ_LINKS && g_rnr_info.soc_info[i].soc; i++) {
		for (j = 0; j < WLAN_UMAC_MAX_PDEVS; j++) {
			if (!(g_rnr_info.soc_info[i].pdev_6ghz_ctx[j].pdev_6ghz)) {
			    qdf_debug("pdev is null");
			    continue;
			}

			ic = wlan_pdev_get_mlme_ext_obj(
					g_rnr_info.soc_info[i].
					pdev_6ghz_ctx[j].pdev_6ghz);

			if (!ic)
				continue;

			qdf_debug("Calling rnr_handler() for pdev_id: %d",
				wlan_objmgr_pdev_get_pdev_id(ic->ic_pdev_obj));
			/* Extend to call with arg2 if required later */
			handler(ic, arg1);
		}
	}
}

qdf_export_symbol(wlan_rnr_6ghz_iterator);

void wlan_global_6ghz_pdev_set(uint8_t soc_id, uint8_t pdev_id)
{
	struct wlan_objmgr_pdev *pdev;
	struct ol_ath_soc_softc *soc;
	uint8_t l_soc_id = wlan_rnr_get_local_socid(soc_id);

	if (l_soc_id == MAX_6GHZ_LINKS) {
		qdf_err("Fatal error: Invalid l_soc_id for soc_id:%d", soc_id);
		return;
	}

	soc = g_rnr_info.soc_info[l_soc_id].soc;
	pdev = wlan_objmgr_get_pdev_by_id(
			soc->psoc_obj, pdev_id, WLAN_MLME_NB_ID);

	if (pdev) {
		g_rnr_info.soc_info[l_soc_id].pdev_6ghz_ctx[pdev_id].pdev_6ghz
		       	= pdev;
		wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
	}
}

qdf_export_symbol(wlan_global_6ghz_pdev_set);

struct
wlan_objmgr_pdev *wlan_global_6ghz_pdev_get(uint8_t soc_id, uint8_t pdev_id)
{
	uint8_t l_soc_id = wlan_rnr_get_local_socid(soc_id);

	if (l_soc_id == MAX_6GHZ_LINKS) {
		qdf_err("Fatal error: Invalid l_soc_id for soc_id:%d", soc_id);
		return NULL;
	}

	return g_rnr_info.soc_info[l_soc_id].pdev_6ghz_ctx[pdev_id].pdev_6ghz;
}

qdf_export_symbol(wlan_global_6ghz_pdev_get);

void wlan_global_6ghz_pdev_destroy(uint8_t soc_id, uint8_t pdev_id)
{
	uint8_t l_soc_id = wlan_rnr_get_local_socid(soc_id);

	if (l_soc_id == MAX_6GHZ_LINKS) {
		qdf_err("Fatal error: Invalid l_soc_id for soc_id:%d", soc_id);
		return;
	}

	g_rnr_info.soc_info[l_soc_id].pdev_6ghz_ctx[pdev_id].pdev_6ghz = NULL;
}

qdf_export_symbol(wlan_global_6ghz_pdev_destroy);

int32_t wlan_lower_band_ap_cnt_get(void)
{
	return qdf_atomic_read(&(g_rnr_info.vdev_lower_band_cnt));
}

qdf_export_symbol(wlan_lower_band_ap_cnt_get);

int32_t wlan_6ghz_band_ap_cnt_get(void)
{
	return qdf_atomic_read(&(g_rnr_info.vdev_6ghz_band_cnt));
}

qdf_export_symbol(wlan_6ghz_band_ap_cnt_get);

void wlan_rnr_set_bss_idx(uint32_t bss_idx, uint8_t group_id)
{
	g_rnr_info.rnr_mbss_idx_map[group_id] |= (1 << (bss_idx-1));
}

uint32_t wlan_rnr_get_bss_idx(uint8_t group_id)
{
	return g_rnr_info.rnr_mbss_idx_map[group_id];
}

void  wlan_rnr_clear_bss_idx(uint8_t group_id)
{
	g_rnr_info.rnr_mbss_idx_map[group_id] = 0;
}
