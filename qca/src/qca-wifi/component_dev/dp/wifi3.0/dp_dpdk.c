/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <dp_dpdk.h>
#include <hal_api.h>
#include "dp_types.h"
#include "dp_rings.h"
#include "dp_internal.h"
#include "dp_peer.h"

#ifdef WLAN_SUPPORT_DPDK
static const char *tcl_ring_name[] = {
	"tcl_data_ring1",
	"tcl_data_ring2",
	"tcl_data_ring3",
	"tcl_data_ring4",
	"tcl_data_ring5",
};

static const char *tcl_comp_ring_name[] = {
	"tcl_comp_ring1",
	"tcl_comp_ring2",
	"tcl_comp_ring3",
	"tcl_comp_ring4",
	"tcl_comp_ring5",
};

static const char *reo_dest_ring_name[] = {
	"reo_dest_ring1",
	"reo_dest_ring2",
	"reo_dest_ring3",
	"reo_dest_ring4",
	"reo_dest_ring5",
	"reo_dest_ring6",
	"reo_dest_ring7",
	"reo_dest_ring8",
};

uint8_t dp_dpdk_get_ring_info(struct cdp_soc_t *soc_hdl,
			      qdf_uio_info_t *uio_info)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	struct hal_srng_params srng_params;
	uint8_t idx = 1, i;

	/* WBM Desc Release Ring */
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    soc->wbm_desc_rel_ring.hal_srng,
			    &srng_params);
	uio_info->mem[idx].name = "wbm_desc_rel_ring";
	uio_info->mem[idx].addr = (unsigned long)srng_params.ring_base_paddr;
	uio_info->mem[idx].size =
			(srng_params.num_entries * srng_params.entry_size) << 2;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* WBM Idle Link Ring */
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    soc->wbm_idle_link_ring.hal_srng,
			    &srng_params);
	uio_info->mem[idx].name = "wbm_idle_link_ring";
	uio_info->mem[idx].addr = (unsigned long)srng_params.ring_base_paddr;
	uio_info->mem[idx].size =
			(srng_params.num_entries * srng_params.entry_size) << 2;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* TCL Data Rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
				    soc->tcl_data_ring[i].hal_srng,
				    &srng_params);
		uio_info->mem[idx].name = tcl_ring_name[i];
		uio_info->mem[idx].addr =
			(unsigned long)srng_params.ring_base_paddr;
		uio_info->mem[idx].size =
			(srng_params.num_entries * srng_params.entry_size) << 2;
		uio_info->mem[idx].memtype = UIO_MEM_PHYS;
		idx++;
	}

	/* TCL Completion Rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
				    soc->tx_comp_ring[i].hal_srng,
				    &srng_params);
		uio_info->mem[idx].name = tcl_comp_ring_name[i];
		uio_info->mem[idx].addr =
			(unsigned long)srng_params.ring_base_paddr;
		uio_info->mem[idx].size =
			(srng_params.num_entries * srng_params.entry_size) << 2;
		uio_info->mem[idx].memtype = UIO_MEM_PHYS;
		idx++;
	}

	/* Reo Dest Rings */
	for (i = 0; i < soc->num_reo_dest_rings; i++) {
		hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
				    soc->reo_dest_ring[i].hal_srng,
				    &srng_params);
		uio_info->mem[idx].name = reo_dest_ring_name[i];
		uio_info->mem[idx].addr =
			(unsigned long)srng_params.ring_base_paddr;
		uio_info->mem[idx].size =
			(srng_params.num_entries * srng_params.entry_size) << 2;
		uio_info->mem[idx].memtype = UIO_MEM_PHYS;
		idx++;
	}

	/* RXDMA Refill Ring */
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    soc->rx_refill_buf_ring[0].hal_srng,
			    &srng_params);
	uio_info->mem[idx].name = "rxdma_buf_ring";
	uio_info->mem[idx].addr = (unsigned long)srng_params.ring_base_paddr;
	uio_info->mem[idx].size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* REO Exception Ring */
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    soc->reo_exception_ring.hal_srng,
			    &srng_params);
	uio_info->mem[idx].name = "reo_exception_ring";
	uio_info->mem[idx].addr =
		(unsigned long)srng_params.ring_base_paddr;
	uio_info->mem[idx].size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* RX Release Ring */
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    soc->rx_rel_ring.hal_srng,
			    &srng_params);
	uio_info->mem[idx].name = "rx_release_ring";
	uio_info->mem[idx].addr = (unsigned long)srng_params.ring_base_paddr;
	uio_info->mem[idx].size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* Reo Reinject Ring */
	hal_get_srng_params(hal_soc_to_hal_soc_handle(hal_soc),
			    soc->reo_reinject_ring.hal_srng,
			    &srng_params);
	uio_info->mem[idx].name = "reo_reinject_ring";
	uio_info->mem[idx].addr =
		(unsigned long)srng_params.ring_base_paddr;
	uio_info->mem[idx].size =
		(srng_params.num_entries * srng_params.entry_size) << 2;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* Shadow Write Pointer for LMAC Ring */
	uio_info->mem[idx].name = "lmac_shadow_wrptr";
	uio_info->mem[idx].addr =
		(unsigned long)hal_soc->shadow_wrptr_mem_paddr;
	uio_info->mem[idx].size =
		sizeof(*(hal_soc->shadow_wrptr_mem_vaddr)) * HAL_MAX_LMAC_RINGS;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;
	idx++;

	/* Shadow Write Pointer for LMAC Ring */
	uio_info->mem[idx].name = "lmac_shadow_rdptr";
	uio_info->mem[idx].addr =
		(unsigned long)hal_soc->shadow_rdptr_mem_paddr;
	uio_info->mem[idx].size =
		sizeof(*(hal_soc->shadow_rdptr_mem_vaddr)) * HAL_SRNG_ID_MAX;
	uio_info->mem[idx].memtype = UIO_MEM_PHYS;

	return idx;
}

int dp_cfgmgr_get_soc_info(struct cdp_soc_t *soc_hdl, uint8_t soc_id,
			   struct dpdk_wlan_soc_info_event *ev_buf)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct hal_soc *hal_soc = (struct hal_soc *)soc->hal_soc;
	struct dp_pdev *pdev = NULL;
	uint8_t i, vdev_count = 0, idx = 0;
	unsigned long dev_base_paddr;
	struct dpdk_wlan_pdev_info *data;

	ev_buf->soc_id = soc_id;
	ev_buf->pdev_count =  soc->pdev_count;
	dev_base_paddr =
		(unsigned long)((struct hif_softc *)(hal_soc->hif_handle))->mem_pa;

	for (i = 0; i < MAX_PDEV_CNT; i++) {
		pdev = soc->pdev_list[i];
		if (!pdev)
			continue;
		data = &ev_buf->pdev_info[idx];
		data->pdev_id = pdev->pdev_id;
		data->lmac_id = pdev->lmac_id;
		data->vdev_count = pdev->vdev_count;
		data->tx_descs_max = pdev->tx_descs_max;
		data->rx_decap_mode = pdev->rx_decap_mode;
		data->num_tx_allowed = pdev->num_tx_allowed;
		data->num_reg_tx_allowed = pdev->num_reg_tx_allowed;
		data->num_tx_spl_allowed = pdev->num_tx_spl_allowed;
		vdev_count += pdev->vdev_count;
		idx++;
		if (idx > soc->pdev_count) {
			dp_err("invalid pdev data");
			return -EINVAL;
		}
	}

	ev_buf->vdev_count = vdev_count;
	ev_buf->max_peers = soc->max_peers;
	ev_buf->max_peer_id = soc->max_peer_id;
	ev_buf->num_tcl_data_rings = soc->num_tcl_data_rings;
	ev_buf->num_reo_dest_rings = soc->num_reo_dest_rings;
	ev_buf->total_link_descs = soc->total_link_descs;
	ev_buf->num_tx_allowed = soc->num_tx_allowed;
	ev_buf->num_reg_tx_allowed = soc->num_reg_tx_allowed;
	ev_buf->num_tx_spl_allowed = soc->num_tx_spl_allowed;

	/* TCL Data Rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		ev_buf->tcl_data_rings[i].hp_addr_offset =
			(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->tcl_data_ring[i].hal_srng) - dev_base_paddr);
		ev_buf->tcl_data_rings[i].tp_addr_offset =
			(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->tcl_data_ring[i].hal_srng) - dev_base_paddr);
	}

	/* TCL Completion Rings */
	for (i = 0; i < soc->num_tcl_data_rings; i++) {
		ev_buf->tx_comp_ring[i].hp_addr_offset =
			(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->tx_comp_ring[i].hal_srng) - dev_base_paddr);
		ev_buf->tx_comp_ring[i].tp_addr_offset =
			(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->tx_comp_ring[i].hal_srng) - dev_base_paddr);
	}

	/* Reo Dest Rings */
	for (i = 0; i < soc->num_reo_dest_rings; i++) {
		ev_buf->reo_dest_ring[i].hp_addr_offset =
			(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->reo_dest_ring[i].hal_srng) - dev_base_paddr);
		ev_buf->reo_dest_ring[i].tp_addr_offset =
			(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->reo_dest_ring[i].hal_srng) - dev_base_paddr);
	}

	/* RXDMA Refill Ring */
	ev_buf->rx_refill_buf_ring.hp_addr_offset =
		(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->rx_refill_buf_ring[0].hal_srng) -
			   hal_soc->shadow_wrptr_mem_paddr);
	ev_buf->rx_refill_buf_ring.tp_addr_offset =
		(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->rx_refill_buf_ring[0].hal_srng) -
			   hal_soc->shadow_rdptr_mem_paddr);

	/* REO Exception Ring */
	ev_buf->reo_exception_ring.hp_addr_offset =
		(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->reo_exception_ring.hal_srng) - dev_base_paddr);
	ev_buf->reo_exception_ring.tp_addr_offset =
		(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->reo_exception_ring.hal_srng) - dev_base_paddr);

	/* RX Release Ring */
	ev_buf->rx_rel_ring.hp_addr_offset =
		(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->rx_rel_ring.hal_srng) - dev_base_paddr);
	ev_buf->rx_rel_ring.tp_addr_offset =
		(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->rx_rel_ring.hal_srng) - dev_base_paddr);

	/* Reo Reinject Ring */
	ev_buf->reo_reinject_ring.hp_addr_offset =
		(uint32_t)(hal_srng_get_hp_addr(hal_soc, soc->reo_reinject_ring.hal_srng) - dev_base_paddr);
	ev_buf->reo_reinject_ring.tp_addr_offset =
		(uint32_t)(hal_srng_get_tp_addr(hal_soc, soc->reo_reinject_ring.hal_srng) - dev_base_paddr);

	return 0;
}

int dp_cfgmgr_get_vdev_info(struct cdp_soc_t *soc_hdl, uint8_t soc_id,
			    struct dpdk_wlan_vdev_info_event *ev_buf)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_pdev *pdev = NULL;
	struct dp_vdev *vdev = NULL;
	struct dpdk_wlan_vdev_info *data;
	uint8_t i = 0, idx = 0;

	ev_buf->soc_id = soc_id;
	for (i = 0; i < MAX_PDEV_CNT; i++) {
		pdev = soc->pdev_list[i];
		if (!pdev)
			continue;

		TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
			data = &ev_buf->vdev_info[idx];
			data->vdev_id = vdev->vdev_id;
			data->pdev_id = pdev->pdev_id;
			data->lmac_id = vdev->lmac_id;
			data->bank_id = vdev->bank_id;
			data->opmode = vdev->opmode;
			qdf_mem_copy(data->mac_addr, vdev->mac_addr.raw,
				     QDF_MAC_ADDR_SIZE);
			data->mesh_vdev = vdev->mesh_vdev;
			data->num_peers = vdev->num_peers;
			idx++;
		}
	}
	ev_buf->vdev_count = idx;

	return 0;
}

static void
dp_cfgmgr_peer_info(struct dp_soc *soc, struct dp_peer *peer, void *arg)
{
	struct dp_dpdk_wlan_peer_info *peer_info = (struct dp_dpdk_wlan_peer_info *)arg;
	struct dpdk_wlan_peer_info *ev_buf = &peer_info->ev_buf[peer_info->idx];

	if (!peer)
		return;

	if (IS_MLO_DP_MLD_PEER(peer))
		return;

	ev_buf->peer_id = peer->peer_id;
	ev_buf->vdev_id = peer->vdev->vdev_id;
	qdf_mem_copy(ev_buf->mac_addr, peer->mac_addr.raw,
		     QDF_MAC_ADDR_SIZE);
	ev_buf->ast_idx = peer->ast_idx;
	ev_buf->ast_hash = peer->ast_hash;
	peer_info->idx++;
}

int dp_cfgmgr_get_peer_info(struct cdp_soc_t *soc_hdl, uint8_t soc_id,
			    struct dpdk_wlan_peer_info *ev_buf)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_dpdk_wlan_peer_info peer_info = {0};

	peer_info.ev_buf = ev_buf;
	dp_soc_iterate_peer(soc, dp_cfgmgr_peer_info, &peer_info, DP_MOD_ID_CDP);
	return 0;
}

int dp_cfgmgr_get_vdev_create_evt_info(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				       struct dpdk_wlan_vdev_create_info *ev_buf)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_vdev *vdev = dp_vdev_get_ref_by_id(soc, vdev_id,
						     DP_MOD_ID_CDP);

	if (!vdev) {
		dp_cdp_err("%pK: Invalid vdev id %d", soc, vdev_id);
		return 0;
	}

	ev_buf->vdev_id = vdev->vdev_id;
	ev_buf->pdev_id = vdev->pdev->pdev_id;
	ev_buf->lmac_id = vdev->lmac_id;
	ev_buf->bank_id = vdev->bank_id;
	ev_buf->opmode = vdev->opmode;
	qdf_mem_copy(ev_buf->mac_addr, vdev->mac_addr.raw,
		     QDF_MAC_ADDR_SIZE);
	ev_buf->mesh_vdev = vdev->mesh_vdev;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_CDP);
	return 0;
}

int dp_cfgmgr_get_peer_create_evt_info(struct cdp_soc_t *soc_hdl, uint16_t peer_id,
				       struct dpdk_wlan_peer_create_info *ev_buf)
{
	struct dp_soc *soc = (struct dp_soc *)soc_hdl;
	struct dp_peer *peer;

	peer = dp_peer_get_ref_by_id((struct dp_soc *)soc,
				     (uint16_t)peer_id,
				     DP_MOD_ID_CDP);

	if (!peer)
		return 0;

	ev_buf->peer_id = peer->peer_id;
	ev_buf->vdev_id = peer->vdev->vdev_id;
	qdf_mem_copy(ev_buf->mac_addr, peer->mac_addr.raw,
		     QDF_MAC_ADDR_SIZE);
	ev_buf->ast_idx = peer->ast_idx;
	ev_buf->ast_hash = peer->ast_hash;

	dp_peer_unref_delete(peer, DP_MOD_ID_CDP);
	return 0;
}
#endif
