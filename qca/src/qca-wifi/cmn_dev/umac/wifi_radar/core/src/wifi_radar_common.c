/*
 * Copyright (c) 2021,2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <wifi_radar_defs_i.h>
#include <qdf_types.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_streamfs.h>
#include <target_if.h>
#include <target_if_direct_buf_rx_api.h>
#include <wlan_osif_priv.h>
#include <cfg_ucfg_api.h>
#include <wifi_radar_cfg.h>

/**
 * wlan_wifi_radar_is_ini_enabled() - Check if wifi_radar ini is enabled
 * @pdev: the physical device object
 *
 * Return: true if wifi_radar is enabled, else false
 */
static bool
wlan_wifi_radar_is_ini_enabled(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	uint8_t wr_enable_bitmap;

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		wifi_radar_err("psoc is null");
		return false;
	}

	wr_enable_bitmap = cfg_get(psoc, CFG_WIFI_RADAR_ENABLE);

	if (wr_enable_bitmap & (1 << wlan_objmgr_pdev_get_pdev_id(pdev)))
		return true;

	wifi_radar_info("wifi radar is disabled for pdev[%d]",
			wlan_objmgr_pdev_get_pdev_id(pdev));
	return false;
}

QDF_STATUS
wlan_wifi_radar_psoc_obj_create_handler(
struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct psoc_wifi_radar *wifi_radar_sc = NULL;

	wifi_radar_sc =
	(struct psoc_wifi_radar *)qdf_mem_malloc(sizeof(*wifi_radar_sc));
	if (!wifi_radar_sc) {
		wifi_radar_err("Failed to allocate wifi_radar_ctx object\n");
		return QDF_STATUS_E_NOMEM;
	}

	wifi_radar_sc->psoc_obj = psoc;

	wlan_objmgr_psoc_component_obj_attach(psoc, WLAN_UMAC_COMP_WIFI_RADAR,
					      (void *)wifi_radar_sc,
					      QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_wifi_radar_psoc_obj_destroy_handler(
struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct psoc_wifi_radar *wifi_radar_sc = NULL;

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj(
				    psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (wifi_radar_sc) {
		wlan_objmgr_psoc_component_obj_detach(
		psoc, WLAN_UMAC_COMP_WIFI_RADAR, (void *)wifi_radar_sc);
		qdf_mem_free(wifi_radar_sc);
	}

	return QDF_STATUS_SUCCESS;
}

#define MAX_ENTRIES 100

static uint32_t
wlan_wifi_radar_get_dbr_num_entries(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_psoc_host_dbr_ring_caps *dbr_ring_cap;
	uint8_t num_dbr_ring_caps, cap_idx, pdev_id;
	struct target_psoc_info *tgt_psoc_info;
	uint32_t num_entries = MAX_ENTRIES;

	if (!pdev) {
		wifi_radar_err("Invalid pdev");
		return num_entries;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		wifi_radar_err("psoc is null");
		return num_entries;
	}

	tgt_psoc_info = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_psoc_info) {
		wifi_radar_err("target_psoc_info is null");
		return num_entries;
	}

	num_dbr_ring_caps = target_psoc_get_num_dbr_ring_caps(tgt_psoc_info);
	dbr_ring_cap = target_psoc_get_dbr_ring_caps(tgt_psoc_info);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	for (cap_idx = 0; cap_idx < num_dbr_ring_caps; cap_idx++) {
		if (dbr_ring_cap[cap_idx].pdev_id == pdev_id &&
		    dbr_ring_cap[cap_idx].mod_id == DBR_MODULE_WIFI_RADAR)
			num_entries = dbr_ring_cap[cap_idx].ring_elems_min;
	}

	num_entries = QDF_MIN(num_entries, MAX_ENTRIES);

	return num_entries;
}

QDF_STATUS
wlan_wifi_radar_pdev_obj_create_handler(
struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct pdev_wifi_radar *pa = NULL;
	int idx;

	if (!pdev) {
		wifi_radar_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_wifi_radar_is_ini_enabled(pdev)) {
		wlan_pdev_nif_feat_ext_cap_clear
			(pdev, WLAN_PDEV_FEXT_WIFI_RADAR_ENABLE);
		return QDF_STATUS_E_NOSUPPORT;
	}

	wlan_pdev_nif_feat_ext_cap_set(pdev, WLAN_PDEV_FEXT_WIFI_RADAR_ENABLE);

	pa = (struct pdev_wifi_radar *)
		 qdf_mem_malloc(sizeof(struct pdev_wifi_radar));
	if (!pa) {
		wifi_radar_err("Failed to allocate pdev object\n");
		return QDF_STATUS_E_NOMEM;
	}
	pa->pdev_obj = pdev;
	pa->header_entries = wlan_wifi_radar_get_dbr_num_entries(pdev);
	if (!pa->header_entries) {
		wifi_radar_err("header entries 0");
		qdf_mem_free(pa);
		return QDF_STATUS_E_INVAL;
	}
	pa->header = (struct wifi_radar_header **)qdf_mem_malloc
			(pa->header_entries *
			 sizeof(struct wifi_radar_header *));
	if (!pa->header) {
		wifi_radar_err("Failed to allocate header");
		qdf_mem_free(pa);
		return QDF_STATUS_E_NOMEM;
	}

	for (idx = 0; idx < pa->header_entries; idx++)
		pa->header[idx] = (struct wifi_radar_header *)qdf_mem_malloc
					(sizeof(struct wifi_radar_header));

	wlan_objmgr_pdev_component_obj_attach(pdev, WLAN_UMAC_COMP_WIFI_RADAR,
					      (void *)pa, QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_wifi_radar_pdev_obj_destroy_handler(
struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct pdev_wifi_radar *pa = NULL;
	uint32_t idx;

	if (!pdev) {
		wifi_radar_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pa =
	wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_WIFI_RADAR);
	if (pa) {
		wlan_objmgr_pdev_component_obj_detach(
		pdev, WLAN_UMAC_COMP_WIFI_RADAR, (void *)pa);
		if (pa->header_entries) {
			for (idx = 0; idx < pa->header_entries; idx++)
				qdf_mem_free(pa->header[idx]);
			qdf_mem_free(pa->header);
		}
		qdf_mem_free(pa);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_wifi_radar_peer_obj_create_handler(
struct wlan_objmgr_peer *peer, void *arg)
{
	struct peer_wifi_radar *pe = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!peer) {
		wifi_radar_err("PEER is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (vdev)
		pdev = wlan_vdev_get_pdev(vdev);

	if (!pdev) {
		wifi_radar_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_info("WiFi Radar is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	pe = (struct peer_wifi_radar *)
	qdf_mem_malloc(sizeof(struct peer_wifi_radar));
	if (!pe) {
		wifi_radar_err("Failed to allocate peer_wifi_radar object\n");
		return QDF_STATUS_E_FAILURE;
	}

	pe->peer_obj = peer;

	wlan_objmgr_peer_component_obj_attach(peer, WLAN_UMAC_COMP_WIFI_RADAR,
					      (void *)pe, QDF_STATUS_SUCCESS);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_wifi_radar_peer_obj_destroy_handler(
struct wlan_objmgr_peer *peer, void *arg)
{
	struct peer_wifi_radar *pe = NULL;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (!peer) {
		wifi_radar_err("PEER is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_peer_get_vdev(peer);
	if (vdev)
		pdev = wlan_vdev_get_pdev(vdev);

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_info("WiFi Radar is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	pe =
	wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_WIFI_RADAR);
	if (pe) {
		wlan_objmgr_peer_component_obj_detach(
		peer, WLAN_UMAC_COMP_WIFI_RADAR, (void *)pe);
		qdf_mem_free(pe);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wifi_radar_get_dev_name() - Get net device name from pdev
 *  @pdev: objmgr pdev
 *
 *  Return: netdev name
 */
static char *wifi_radar_get_dev_name(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_osif_priv *pdev_ospriv;
	struct qdf_net_if *nif;

	pdev_ospriv = wlan_pdev_get_ospriv(pdev);
	if (!pdev_ospriv) {
		wifi_radar_err("pdev_ospriv is NULL\n");
		return NULL;
	}

	nif = pdev_ospriv->nif;
	if (!nif) {
		wifi_radar_err("pdev nif is NULL\n");
		return NULL;
	}

	return  qdf_net_if_get_devname(nif);
}

QDF_STATUS wifi_radar_streamfs_init(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_wifi_radar *pa = NULL;
	char *devname;
	char folder[32];

	if (!pdev) {
		wifi_radar_err("PDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_info("WiFi Radar is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(
		pdev, WLAN_UMAC_COMP_WIFI_RADAR);

	if (!pa) {
		wifi_radar_err("pdev_wifi_radar is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (!pa->is_wifi_radar_capable) {
		wifi_radar_err("WiFi Radar is not supported");
		return QDF_STATUS_E_FAILURE;
	}

	devname = wifi_radar_get_dev_name(pdev);
	if (!devname) {
		wifi_radar_err("devname is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	snprintf(folder, sizeof(folder), "wifi-radar%s", devname);

	pa->dir_ptr = qdf_streamfs_create_dir((const char *)folder, NULL);

	if (!pa->dir_ptr) {
		wifi_radar_err("Directory create failed");
		return QDF_STATUS_E_FAILURE;
	}

	pa->chan_ptr = qdf_streamfs_open("wifi_radar_dump", pa->dir_ptr,
					 pa->subbuf_size,
					 pa->num_subbufs, NULL);

	if (!pa->chan_ptr) {
		wifi_radar_err("Chan create failed");
		qdf_streamfs_remove_dir_recursive(pa->dir_ptr);
		pa->dir_ptr = NULL;
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_radar_streamfs_remove(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_wifi_radar *pa = NULL;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_info("WiFi Radar is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(
		 pdev, WLAN_UMAC_COMP_WIFI_RADAR);
	if (pa) {
		if (pa->chan_ptr) {
			qdf_streamfs_close(pa->chan_ptr);
			pa->chan_ptr = NULL;
		}

		if (pa->dir_ptr) {
			qdf_streamfs_remove_dir_recursive(pa->dir_ptr);
			pa->dir_ptr = NULL;
		}

	} else {
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_radar_streamfs_write(
struct pdev_wifi_radar *pa, const void *write_data,
size_t write_len)
{
	if (pa->chan_ptr) {
	/* write to channel buffer */
		qdf_streamfs_write(pa->chan_ptr, (const void *)write_data,
				   write_len);
	} else {
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wifi_radar_streamfs_flush(struct pdev_wifi_radar *pa)
{
	if (pa->chan_ptr) {
	/* Flush the data write to channel buffer */
		qdf_streamfs_flush(pa->chan_ptr);
	} else {
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}
