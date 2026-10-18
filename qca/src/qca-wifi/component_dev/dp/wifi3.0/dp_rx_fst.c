/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifdef WLAN_SUPPORT_RX_FLOW_TAG
#include "dp_types.h"
#include "qdf_mem.h"
#include "qdf_nbuf.h"
#include "cfg_dp.h"
#include "wlan_cfg.h"
#include "dp_types.h"
#include "hal_rx_flow.h"
#include "dp_htt.h"
#include "dp_rx_tag.h"
#include "cdp_txrx_cmn_struct.h"
#if defined(WLAN_SUPPORT_PPEDS) || (QCA_PPE_VP)
#include <ppe_drv.h>
#include <ppe_drv_sc.h>
#endif

#define RING_MAP_MAX 0xF

#if defined(WLAN_SUPPORT_PPEDS) || (QCA_PPE_VP)
/**
 * dp_rx_ppe_fse_register() - function to register fse add and delete
 *				with PPE
 *
 * Return: void
 */
void dp_rx_ppe_fse_register(void)
{
	struct ppe_drv_fse_ops ppe_fse_ops;

	ppe_fse_ops.create_fse_rule = dp_rx_ppe_add_flow_entry;
	ppe_fse_ops.destroy_fse_rule = dp_rx_ppe_del_flow_entry;

	ppe_drv_fse_ops_register(&ppe_fse_ops);
}

/**
 * dp_rx_ppe_fse_unregister() - function to unregister fse add and delete
 *				with PPE
 *
 * Return: void
 */
void dp_rx_ppe_fse_unregister(void)
{
	ppe_drv_fse_ops_unregister();
}

/**
 * dp_rx_update_ppe_fse_fields() - API to update PPE related fields
 * @hal_rx_flow: hal level flow info
 * @cdp_rx_flow_info: flow info from PPE
 *
 * Return: void
 */
void dp_rx_update_ppe_fse_fields(struct hal_rx_flow *flow,
				 struct cdp_rx_flow_info *rx_flow_info)
{
	flow->use_ppe_ds = rx_flow_info->use_ppe_ds;
	flow->service_code = PPE_DRV_SC_SPF_BYPASS;
}
#else
/**
 * dp_rx_ppe_fse_register() - function to register fse add and delete
 *				with PPE
 *
 * Return: void
 */
void dp_rx_ppe_fse_register(void)
{
}

/**
 * dp_rx_ppe_fse_unregister() - function to unregister fse add and delete
 *				with PPE
 *
 * Return: void
 */
void dp_rx_ppe_fse_unregister(void)
{
}

/**
 * dp_rx_update_ppe_fse_fields() - API to update PPE related fields
 * @hal_rx_flow: hal level flow info
 * @cdp_rx_flow_info: flow info from PPE
 *
 * Return: void
 */
void dp_rx_update_ppe_fse_fields(struct hal_rx_flow *flow,
				 struct cdp_rx_flow_info *rx_flow_info)
{
}
#endif

/**
 * In Hawkeye, a hardware bug disallows SW to only clear a single flow entry
 * when added/deleted by upper layer. Workaround is to clear entire cache,
 * which can have a performance impact. Flow additions/deletions
 * are bundled together over 100ms to save HW cycles if upper layer
 * adds/deletes multiple flows together. Use a longer timeout during setup
 * stage since no flows are anticipated at this time.
 */
#define HW_RX_FSE_CACHE_INVALIDATE_BUNDLE_PERIOD_MS (100)
#define HW_RX_FSE_CACHE_INVALIDATE_DELAYED_FST_SETUP_MS (5000)

/**
 * dp_rx_flow_write_entry_metadata() - Update fse metadata
 * @pdev: DP pdev instance
 * @fse_metadata: FSE metadata
 * @fse: fse entry
 *
 * Return: Success when flow is added, no-memory or already exists on error
 */
QDF_STATUS
dp_rx_flow_write_entry_metadata(struct dp_pdev *pdev, uint32_t fse_metadata,
				struct dp_rx_fse *fse)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_rx_fst *fst;
	struct hal_rx_flow flow = { 0 };

	flow.fse_metadata = fse_metadata;
	fst = pdev->rx_fst;

	if (hal_rx_flow_write_fse_metadata(soc->hal_soc, fst->hal_rx_fst,
					   fse->flow_id, &flow))
		return QDF_STATUS_SUCCESS;

	return QDF_STATUS_E_INVAL;
}

/**
 * dp_rx_flow_get_fse() - Obtain flow search entry from flow hash
 * @fst: Rx FST Handle
 * @flow_hash: Computed hash value of flow
 *
 * Return: Handle to flow search table entry
 */
static inline struct dp_rx_fse *
dp_rx_flow_get_fse(struct dp_rx_fst *fst, uint32_t flow_hash)
{
	struct dp_rx_fse *fse;
	uint32_t idx = hal_rx_get_hal_hash(fst->hal_rx_fst, flow_hash);

	fse = (struct dp_rx_fse *)((uint8_t *)fst->base + (idx *
					sizeof(struct dp_rx_fse)));

	return fse;
}

/**
 * dp_rx_flow_dump_flow_entry() - Print flow search entry from 5-tuple
 * @fst: Rx FST Handle
 * @flow_info: Flow 5-tuple
 *
 * Return: None
 */
void dp_rx_flow_dump_flow_entry(struct dp_rx_fst *fst,
				struct cdp_rx_flow_info *flow_info)
{
	dp_info("Dest IP address %x:%x:%x:%x",
		flow_info->flow_tuple_info.dest_ip_127_96,
		flow_info->flow_tuple_info.dest_ip_95_64,
		flow_info->flow_tuple_info.dest_ip_63_32,
		flow_info->flow_tuple_info.dest_ip_31_0);
	dp_info("Source IP address %x:%x:%x:%x",
		flow_info->flow_tuple_info.src_ip_127_96,
		flow_info->flow_tuple_info.src_ip_95_64,
		flow_info->flow_tuple_info.src_ip_63_32,
		flow_info->flow_tuple_info.src_ip_31_0);
	dp_info("Dest port %u, Src Port %u, Protocol %u",
		flow_info->flow_tuple_info.dest_port,
		flow_info->flow_tuple_info.src_port,
		flow_info->flow_tuple_info.l4_protocol);
}

/**
 * dp_rx_flow_dump_hal_fse_entries() - Print flow search entries though HAL
 * @soc_hdl: CDP SoC Handle
 * @pdev_id: pdev_id
 *
 * Return: Success if request is accepted else failure
 */
QDF_STATUS
dp_rx_flow_dump_hal_fse_entries(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct wlan_cfg_dp_soc_ctxt *cfg;
	struct dp_soc *soc = cdp_soc_t_to_dp_soc(soc_hdl);
	struct dp_rx_fst *fst;

	if (qdf_unlikely(!soc))
		return QDF_STATUS_E_FAILURE;

	cfg = soc->wlan_cfg_ctx;

	if (qdf_unlikely(!wlan_cfg_is_rx_flow_tag_enabled(cfg))) {
		qdf_err("RX Flow tag feature disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	fst = soc->pdev_list[pdev_id]->rx_fst;
	qdf_spin_lock_bh(&fst->fst_lock);
	hal_rx_dump_fse_table(fst->hal_rx_fst);
	qdf_spin_unlock_bh(&fst->fst_lock);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_flow_compute_flow_hash() - Print flow search entry from 5-tuple
 * @fst: Rx FST Handle
 * @rx_flow_info: DP Rx Flow 5-tuple programmed by upper layer
 * @flow: HAL (HW) flow entry
 *
 * Return: Computed Toeplitz hash
 */
uint32_t dp_rx_flow_compute_flow_hash(struct dp_rx_fst *fst,
				      struct cdp_rx_flow_info *rx_flow_info,
				      struct hal_rx_flow *flow)
{
	flow->tuple_info.dest_ip_127_96 =
			rx_flow_info->flow_tuple_info.dest_ip_127_96;
	flow->tuple_info.dest_ip_95_64 =
			rx_flow_info->flow_tuple_info.dest_ip_95_64;
	flow->tuple_info.dest_ip_63_32 =
			rx_flow_info->flow_tuple_info.dest_ip_63_32;
	flow->tuple_info.dest_ip_31_0 =
			rx_flow_info->flow_tuple_info.dest_ip_31_0;
	flow->tuple_info.src_ip_127_96 =
			rx_flow_info->flow_tuple_info.src_ip_127_96;
	flow->tuple_info.src_ip_95_64 =
			rx_flow_info->flow_tuple_info.src_ip_95_64;
	flow->tuple_info.src_ip_63_32 =
			rx_flow_info->flow_tuple_info.src_ip_63_32;
	flow->tuple_info.src_ip_31_0 =
			rx_flow_info->flow_tuple_info.src_ip_31_0;
	flow->tuple_info.dest_port =
			rx_flow_info->flow_tuple_info.dest_port;
	flow->tuple_info.src_port =
			rx_flow_info->flow_tuple_info.src_port;
	flow->tuple_info.l4_protocol =
			rx_flow_info->flow_tuple_info.l4_protocol;

	return hal_flow_toeplitz_hash(fst->hal_rx_fst, flow);
}

/**
 * dp_rx_flow_alloc_entry() - Create DP and HAL flow entries in FST
 * @pdev: Pdev handle
 * @fst: Rx FST Handle
 * @rx_flow_info: DP Rx Flow 5-tuple to be added to DP FST
 * @flow: HAL (HW) flow entry that is created
 * @fse: fse entry
 *
 * Return: status - -1 : failure, 0 : add success, 1 : update success
 */
static
int dp_rx_flow_alloc_entry(struct dp_pdev *pdev, struct dp_rx_fst *fst,
			   struct cdp_rx_flow_info *rx_flow_info,
			   struct hal_rx_flow *flow, struct dp_rx_fse **fse)
{
	struct dp_soc *soc = pdev->soc;
	uint32_t flow_hash;
	uint32_t flow_idx;
	QDF_STATUS status;
	bool update_svc_id = false;
	int ret = -1;

	*fse = NULL;
	flow_hash = dp_rx_flow_compute_flow_hash(fst, rx_flow_info, flow);

	if (DP_RX_FLOW_INVALID_SVC_ID != rx_flow_info->svc_id)
		update_svc_id = true;

	status = hal_rx_insert_flow_entry(soc->hal_soc,
					  fst->hal_rx_fst,
					  flow_hash,
					  &rx_flow_info->flow_tuple_info,
					  &flow_idx);
	if (status != QDF_STATUS_SUCCESS) {
		if (status == QDF_STATUS_E_EXISTS) {
			*fse = dp_rx_flow_get_fse(fst, flow_idx);
			if (update_svc_id && (*fse)->is_valid) {
				uint32_t meta;

				meta = DP_RX_FSE_FLOW_UPDATE_TID(DP_RX_FSE_FLOW_MATCH_SFE, rx_flow_info->tid);
				meta = DP_RX_FSE_FLOW_UPDATE_EVT_REQ(meta, 1);
				dp_rx_flow_write_entry_metadata(pdev, meta,
								*fse);

				ret = 1;
				goto update_n_send_wdi;
			}
		}

		dp_err("Add entry failed with status %d for tuple with hash %u",
		       status, flow_hash);

		return ret;
	}

	*fse = dp_rx_flow_get_fse(fst, flow_idx);
	(*fse)->is_ipv4_addr_entry = rx_flow_info->is_addr_ipv4;
	(*fse)->flow_hash = flow_hash;
	(*fse)->flow_id = flow_idx;
	(*fse)->stats.msdu_count = 0;
	(*fse)->is_valid = true;

	ret = 0;

update_n_send_wdi:

	if (rx_flow_info->dest_mac)
		qdf_mem_copy(&((*fse)->dest_mac.raw[0]), rx_flow_info->dest_mac,
			     QDF_MAC_ADDR_SIZE);

	(*fse)->svc_id = rx_flow_info->svc_id;
	(*fse)->tid = rx_flow_info->tid;

	if (update_svc_id) {
		struct fse_info_cookie cookie = {0};

		/* send wdi event */
		qdf_mem_copy(&cookie.tuple_info, &rx_flow_info->flow_tuple_info,
			     sizeof(struct cdp_rx_flow_tuple_info));

		cookie.svc_id = rx_flow_info->svc_id;
		cookie.tid = rx_flow_info->tid;
		cookie.dest_mac = rx_flow_info->dest_mac;

		dp_wdi_event_handler(WDI_EVENT_FSE_UPDATE, soc, &cookie,
				     HTT_INVALID_PEER, dp_rx_fse_event_add,
				     pdev->pdev_id);
	}

	return ret;
}

/**
 * dp_rx_flow_find_entry_by_tuple() - Find the DP FSE matching a given 5-tuple
 * @fst: Rx FST Handle
 * @rx_flow_info: DP Rx Flow 5-tuple
 * @flow: Pointer to the  HAL (HW) flow entry
 *
 * Return: Pointer to the DP FSE entry
 */
struct dp_rx_fse *
dp_rx_flow_find_entry_by_tuple(hal_soc_handle_t hal_soc_hdl,
			       struct dp_rx_fst *fst,
			       struct cdp_rx_flow_info *rx_flow_info,
			       struct hal_rx_flow *flow)
{
	uint32_t flow_hash;
	uint32_t flow_idx;
	QDF_STATUS status;

	flow_hash = dp_rx_flow_compute_flow_hash(fst, rx_flow_info, flow);

	status = hal_rx_find_flow_from_tuple(hal_soc_hdl,
					     fst->hal_rx_fst,
					     flow_hash,
					     &rx_flow_info->flow_tuple_info,
					     &flow_idx);

	if (status != QDF_STATUS_SUCCESS) {
		dp_err("Could not find tuple with hash %u", flow_hash);
		dp_rx_flow_dump_flow_entry(fst, rx_flow_info);
		return NULL;
	}
	return dp_rx_flow_get_fse(fst, flow_idx);
}

/**
 * dp_rx_flow_find_entry_by_flowid() - Find DP FSE matching a given flow index
 * @soc: soc handle
 * @fst: Rx FST Handle
 * @flow_id: Flow index of the requested flow
 *
 * Return: Pointer to the DP FSE entry
 */
struct dp_rx_fse *
dp_rx_flow_find_entry_by_flowid(struct dp_soc *soc, struct dp_rx_fst *fst,
				uint32_t flow_id)
{
	struct dp_rx_fse *fse = NULL;

	fse = dp_rx_flow_get_fse(fst, flow_id);
	if (!fse->is_valid)
		return NULL;

	dp_info("flow_idx= %d, flow_addr = %pK", flow_id, fse);
	if (dp_assert_always_internal_stat((fse->flow_id ==
			hal_rx_get_hal_hash(fst->hal_rx_fst, flow_id)),
			soc, rx.err.invalid_fse_flow_id)) {
		return NULL;
	}

	return fse;
}

/**
 * dp_rx_flow_send_htt_operation_cmd() - Send HTT FSE command to FW for flow
 *					 addition/removal
 * @pdev: Pdev instance
 * @op: Add/delete operation
 * @info: DP Flow parameters of the flow added/deleted
 *
 * Return: Success on sending HTT command to FW, error on failure
 */
QDF_STATUS dp_rx_flow_send_htt_operation_cmd(struct dp_pdev *pdev,
					     enum dp_htt_flow_fst_operation op,
					     struct cdp_rx_flow_info *info)
{
	struct dp_htt_rx_flow_fst_operation fst_op;
	struct wlan_cfg_dp_soc_ctxt *cfg = pdev->soc->wlan_cfg_ctx;

	qdf_mem_set(&fst_op, sizeof(struct dp_htt_rx_flow_fst_operation), 0);

	if (qdf_unlikely(wlan_cfg_is_rx_flow_search_table_per_pdev(cfg))) {
		/* Firmware pdev ID starts from 1 */
		fst_op.pdev_id = DP_SW2HW_MACID(pdev->pdev_id);
	} else {
		fst_op.pdev_id = 0;
	}

	fst_op.op_code = op;
	fst_op.rx_flow = info;

	return dp_htt_rx_flow_fse_operation(pdev, &fst_op);
}

/**
 * dp_rx_get_ring_map() - API to fetch ring id mask
 * @cfg: dp soc configuration object
 *
 * Return: configured ring map
 */
#if defined(WLAN_FEATURE_11BE_MLO) && defined(WLAN_MLO_MULTI_CHIP)
static inline uint8_t
dp_rx_get_ring_map(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	return wlan_cfg_mlo_rx_ring_map_get(cfg);
}
#else
static inline uint8_t
dp_rx_get_ring_map(struct wlan_cfg_dp_soc_ctxt *cfg)
{
	/* Always default to 3 ring configuration*/
	return 0x7;
}
#endif

/**
 * dp_rx_get_next_ring_id() - traverse through ring id mask
 * @last_ring_id: ring id used for last flow
 * @cfg: dp soc configuration object
 *
 * Return: ring id with which flow should be attached
 */
static uint8_t
dp_rx_get_next_ring_id(uint8_t last_ring_id, struct wlan_cfg_dp_soc_ctxt *cfg)
{
	uint8_t ring_id_mask = dp_rx_get_ring_map(cfg);
	if (ring_id_mask == 0 || ring_id_mask > RING_MAP_MAX) {
		ring_id_mask = RING_MAP_MAX;
	}

	if (ring_id_mask >> last_ring_id)
		ring_id_mask >>= last_ring_id;
	else
		last_ring_id = 0;

	last_ring_id += qdf_ffs(ring_id_mask);

	return last_ring_id;
}

/**
 * dp_rx_set_rr_reo_indication() - API to update reo_destination_indication
 * @hal_rx_flow: hal level flow info
 * @ring_id: last indicated ring id
 * @cfg: dp soc configuration object
 *
 * Return: true on valid indication
 */
static inline bool
dp_rx_set_rr_reo_indication(struct hal_rx_flow *flow, uint8_t *ring_id,
			    struct wlan_cfg_dp_soc_ctxt *cfg)
{
	/**
	 * If Round Robin flow to core distribution is enabled,
	 * select reo_destination_indication in a round robin
	 * fashion for all the incoming flows.
	 */
	if (!wlan_cfg_is_rx_rr_enabled(cfg)) {
		return false;
	}

	flow->reo_destination_indication = *ring_id;
	*ring_id = dp_rx_get_next_ring_id(*ring_id, cfg);
	return true;
}

/**
 * dp_rx_flow_invalidate_fse_entry() - invalidate fse entry
 * @pdev: pdev handle
 * @fse: fse entry
 * @rx_flow_info: Flow tuple info
 * @delete_entry: flag to indicate if delete is needed if invalidate fails
 *
 * Return: Status
 */
QDF_STATUS
dp_rx_flow_invalidate_fse_entry(struct dp_pdev *pdev, struct dp_rx_fse *fse,
				struct cdp_rx_flow_info *rx_flow_info,
				bool delete_entry)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_rx_fst *fst = pdev->rx_fst;
	QDF_STATUS status =  QDF_STATUS_SUCCESS;
	struct dp_soc *partner_soc;
	int chip_id;

	if (soc->is_rx_fse_full_cache_invalidate_war_enabled) {
		qdf_atomic_set(&fst->is_cache_update_pending, 1);
	} else {
		struct wlan_cfg_dp_soc_ctxt *cfg = pdev->soc->wlan_cfg_ctx;

		for (chip_id = 0; chip_id < WLAN_MAX_MLO_CHIPS; chip_id++) {
			if (chip_id >= soc->arch_ops.dp_soc_get_num_soc(soc))
				return QDF_STATUS_SUCCESS;

			if (soc->arch_ops.dp_get_soc_by_chip_id)
				partner_soc =
				soc->arch_ops.dp_get_soc_by_chip_id(soc,
								   chip_id);
			else
				partner_soc = soc;

			if (!partner_soc)
				continue;

			/**
			 * unlike LI, BE SOC has only one DMAC so there is
			 * no concept of FST per pdev, also there is only one
			 * single FST table maintained across SOCs which are
			 * part of one ML group.
			 */
			if (qdf_likely(
			      !wlan_cfg_is_rx_flow_search_table_per_pdev(cfg)))
				pdev = partner_soc->pdev_list[0];

			/**
			 * Send HTT cache invalidation command to firmware to
			 * reflect the added flow
			 */
			status = dp_rx_flow_send_htt_operation_cmd(
					pdev,
					DP_HTT_FST_CACHE_INVALIDATE_ENTRY,
					rx_flow_info);

			if (QDF_STATUS_SUCCESS != status) {
				dp_err("Send cache invalidate entry to fw failed: %u",
				       status);
				dp_rx_flow_dump_flow_entry(fst, rx_flow_info);

				if (delete_entry) {
					/* Free DP FSE and HAL FSE */
					hal_rx_flow_delete_entry(
							soc->hal_soc,
							fst->hal_rx_fst,
							fse->hal_rx_fse);
					fse->is_valid = false;
				}
			}
		}
	}

	return status;
}

/**
 * dp_rx_flow_add_entry() - Add a flow entry to flow search table
 * @pdev: DP pdev instance
 * @rx_flow_info: DP flow paramaters
 *
 * Return: Success when flow is added, no-memory or already exists on error
 */
QDF_STATUS dp_rx_flow_add_entry(struct dp_pdev *pdev,
				struct cdp_rx_flow_info *rx_flow_info)
{
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fse *fse = NULL;
	struct dp_soc *soc = pdev->soc;
	struct dp_rx_fst *fst;
	struct cdp_rx_flow_tuple_info *flow_tuple_info =
						&rx_flow_info->flow_tuple_info;
	struct wlan_cfg_dp_soc_ctxt *cfg = pdev->soc->wlan_cfg_ctx;
	int ret;

	fst = pdev->rx_fst;

	/* Initialize unused bits in IPv6 address for IPv4 address */
	if (rx_flow_info->is_addr_ipv4) {
		flow_tuple_info->dest_ip_63_32 = 0;
		flow_tuple_info->dest_ip_95_64 = 0;
		flow_tuple_info->dest_ip_127_96 =
			HAL_IP_DA_SA_PREFIX_IPV4_COMPATIBLE_IPV6;

		flow_tuple_info->src_ip_63_32 = 0;
		flow_tuple_info->src_ip_95_64 = 0;
		flow_tuple_info->src_ip_127_96 =
			HAL_IP_DA_SA_PREFIX_IPV4_COMPATIBLE_IPV6;
	}

	qdf_spin_lock_bh(&fst->fst_lock);

	/* Allocate entry in DP FST */
	ret = dp_rx_flow_alloc_entry(pdev, fst, rx_flow_info, &flow, &fse);
	if (ret == -1) {
		dp_err("RX FSE alloc failed");
		dp_rx_flow_dump_flow_entry(fst, rx_flow_info);
		qdf_spin_unlock_bh(&fst->fst_lock);
		return QDF_STATUS_E_NOMEM;
	} else if (ret == 1) {
		qdf_spin_unlock_bh(&fst->fst_lock);
		return dp_rx_flow_invalidate_fse_entry(pdev, fse, rx_flow_info,
						       true);
	} else {
	}

	dp_info("flow_addr = %pK, flow_id = %u, valid = %d, v4 = %d\n",
		fse, fse->flow_id, fse->is_valid, fse->is_ipv4_addr_entry);

	if (rx_flow_info->drop) {
		flow.drop = 1;
	} else if (rx_flow_info->ring_id > 0 &&
		rx_flow_info->ring_id < HAL_REO_DEST_IND_DROP_RING_ID) {
		flow.reo_destination_indication = rx_flow_info->ring_id;
	} else {
		/* Initialize other parameters for HW flow & populate HW FSE
		 * entry
		 */
		flow.reo_destination_indication = (fse->flow_hash &
				HAL_REO_DEST_IND_HASH_MASK);

		/**
		 * Reo destination of each flow is mapped to match the same used
		 * by RX Hash algorithm. If RX Hash is disabled, then the REO
		 * destination below is directly got from pdev, rather than
		 * using dp_peer_setup_get_reo_hash since we do not have vdev
		 * handle here.
		 */
		if (wlan_cfg_is_rx_hash_enabled(soc->wlan_cfg_ctx)) {
			flow.reo_destination_indication |=
				HAL_REO_DEST_IND_START_OFFSET;
		} else {
			flow.reo_destination_indication = pdev->reo_dest;
		}

		dp_rx_set_rr_reo_indication(&flow, &(fst->ring_id), cfg);
	}

	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;
	flow.fse_metadata =
		DP_RX_FSE_FLOW_UPDATE_DROP_BIT(rx_flow_info->fse_metadata,
					       rx_flow_info->drop);
	dp_rx_update_ppe_fse_fields(&flow, rx_flow_info);
	fse->hal_rx_fse = hal_rx_flow_setup_fse(soc->hal_soc, fst->hal_rx_fst,
						fse->flow_id, &flow);
	if (qdf_unlikely(!fse->hal_rx_fse)) {
		dp_err("Unable to alloc FSE entry");
		dp_rx_flow_dump_flow_entry(fst, rx_flow_info);
		/* Free up the FSE entry as returning failure */
		fse->is_valid = false;
		qdf_spin_unlock_bh(&fst->fst_lock);
		return QDF_STATUS_E_EXISTS;
	}

	/* Increment number of valid entries in table */
	fst->num_entries++;
	fst->fse_flow_create_cnt++;

	if (rx_flow_info->is_addr_ipv4)
		fst->ipv4_fse_cnt++;
	else
		fst->ipv6_fse_cnt++;

	dp_info("FST num_entries = %d, reo_dest_ind = %d, reo_dest_hand = %u",
		fst->num_entries, flow.reo_destination_indication,
		flow.reo_destination_handler);

	qdf_spin_unlock_bh(&fst->fst_lock);
	return dp_rx_flow_invalidate_fse_entry(pdev, fse, rx_flow_info, true);
}

/**
 * dp_rx_flow_delete_entry() - Delete a flow entry from flow search table
 * @pdev: pdev handle
 * @rx_flow_info: DP flow parameters
 *
 * Return: Success when flow is deleted, error on failure
 */
QDF_STATUS dp_rx_flow_delete_entry(struct dp_pdev *pdev,
				   struct cdp_rx_flow_info *rx_flow_info)
{
	struct hal_rx_flow flow = { 0 };
	struct dp_rx_fse *fse;
	struct dp_soc *soc = pdev->soc;
	struct dp_rx_fst *fst;
	QDF_STATUS status;

	fst = pdev->rx_fst;

	qdf_spin_lock_bh(&fst->fst_lock);
	/* Find the given flow entry DP FST */
	fse = dp_rx_flow_find_entry_by_tuple(soc->hal_soc, fst,
					     rx_flow_info, &flow);

	if (!fse || (fse->is_valid == false)) {
		dp_err("RX flow delete entry failed");
		dp_rx_flow_dump_flow_entry(fst, rx_flow_info);
		qdf_spin_unlock_bh(&fst->fst_lock);
		return QDF_STATUS_E_INVAL;
	}

	/* Delete the FSE in HW FST */
	status = hal_rx_flow_delete_entry(soc->hal_soc, fst->hal_rx_fst,
					  fse->hal_rx_fse);
	if (dp_assert_always_internal_stat((status == QDF_STATUS_SUCCESS),
				soc, rx.err.hw_fst_del_failed)) {
		qdf_spin_unlock_bh(&fst->fst_lock);
		dp_err("RX HW flow delete entry failed");
		return status;
	}

	if (fse->svc_id != DP_RX_FLOW_INVALID_SVC_ID) {
		/* send WDI event */
		struct fse_info_cookie cookie = {0};

		/* send wdi event */
		qdf_mem_copy(&cookie.tuple_info, &rx_flow_info->flow_tuple_info,
			     sizeof(struct cdp_rx_flow_tuple_info));

		cookie.svc_id = fse->svc_id;
		cookie.tid = fse->tid;
		cookie.dest_mac = &fse->dest_mac.raw[0];

		dp_wdi_event_handler(WDI_EVENT_FSE_UPDATE, soc, &cookie,
				     HTT_INVALID_PEER, dp_rx_fse_event_del,
				     pdev->pdev_id);
	}

	/* Free the FSE in DP FST */
	fse->is_valid = false;
	fse->mismatch = false;
	fse->svc_id = 0;
	fse->tid = 0;
	qdf_mem_zero(&fse->dest_mac.raw[0], QDF_MAC_ADDR_SIZE);

	/* Decrement number of valid entries in table */
	fst->num_entries--;
	fst->fse_flow_del_cnt++;

	if (rx_flow_info->is_addr_ipv4)
		fst->ipv4_fse_cnt--;
	else
		fst->ipv6_fse_cnt--;

	qdf_spin_unlock_bh(&fst->fst_lock);
	return dp_rx_flow_invalidate_fse_entry(pdev, fse, rx_flow_info, false);
}

/* dp_rx_flow_update_fse_stats() - Update a flow's statistics
 * @pdev: pdev handle
 * @flow_id: flow index (truncated hash) in the Rx FST
 *
 * Return: Success when flow statistcs is updated, error on failure
 */
QDF_STATUS dp_rx_flow_update_fse_stats(struct dp_pdev *pdev, uint32_t flow_id)
{
	struct dp_rx_fse *fse;

	fse = dp_rx_flow_find_entry_by_flowid(pdev->soc, pdev->rx_fst, flow_id);

	if (NULL == fse) {
		dp_err("Flow not found, flow ID %u", flow_id);
		return QDF_STATUS_E_NOENT;
	}

	fse->stats.msdu_count += 1;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_flow_get_fse_stats() - Fetch a flow's stats based on DP flow parameter
 * @pdev: pdev handle
 * @rx_flow_info: Pointer to the DP flow struct of the requested flow
 * @stats: Matching flow's stats returned to caller
 *
 * Return: Success when flow statistcs is updated, error on failure
 */
QDF_STATUS dp_rx_flow_get_fse_stats(struct dp_pdev *pdev,
				    struct cdp_rx_flow_info *rx_flow_info,
				    struct cdp_flow_stats *stats)
{
	struct dp_soc *soc = pdev->soc;
	struct dp_rx_fst *fst;
	struct dp_rx_fse *fse;
	struct hal_rx_flow flow;

	fst = pdev->rx_fst;

	/* Find the given flow entry DP FST */
	fse = dp_rx_flow_find_entry_by_tuple(soc->hal_soc, fst,
					     rx_flow_info, &flow);
	if (!fse) {
		dp_err("RX flow entry search failed");
		dp_rx_flow_dump_flow_entry(fst, rx_flow_info);
		return QDF_STATUS_E_INVAL;
	}

	stats->msdu_count = fse->stats.msdu_count;
	stats->mon_msdu_count = fse->stats.mon_msdu_count;
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_flow_cache_invalidate_timer_handler() - Timer handler used for bundling
 * flows before invalidating entire cache
 * @ctx: Pdev handle
 *
 * Return: None
 */
void dp_rx_flow_cache_invalidate_timer_handler(void *ctx)
{
	struct dp_pdev *pdev = (struct dp_pdev *)ctx;
	struct dp_rx_fst *fst;
	bool is_update_pending;
	QDF_STATUS status;

	fst = pdev->rx_fst;

	if (dp_assert_always_internal_stat(fst,
			pdev->soc, rx.err.cache_invl_fail_no_fst)) {
		dp_err("cache invalidate failed");
		return;
	}

	is_update_pending = qdf_atomic_read(&fst->is_cache_update_pending);
	qdf_atomic_set(&fst->is_cache_update_pending, 0);

	if (is_update_pending) {
		/* Send full cache invalidate command to firmware */
		status = dp_rx_flow_send_htt_operation_cmd(
				pdev,
				DP_HTT_FST_CACHE_INVALIDATE_FULL,
				NULL);

		if (QDF_STATUS_SUCCESS != status)
			dp_err("Send full cache inv to fw failed: %u", status);
	}

	qdf_timer_start(&fst->cache_invalidate_timer,
			HW_RX_FSE_CACHE_INVALIDATE_BUNDLE_PERIOD_MS);
}

/**
 * dp_rx_fst_attach() - Initialize Rx FST and setup necessary parameters
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: Handle to flow search table entry
 */
QDF_STATUS dp_rx_fst_attach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *fst;
	uint8_t *hash_key;
	struct hal_rx_flow flow;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;
	bool is_rx_flow_search_table_per_pdev =
		wlan_cfg_is_rx_flow_search_table_per_pdev(cfg);

	if (qdf_unlikely(!wlan_cfg_is_rx_flow_tag_enabled(cfg))) {
		dp_err("RX Flow tag feature disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (!wlan_psoc_nif_fw_ext_cap_get(
				(struct wlan_objmgr_psoc *)soc->ctrl_psoc,
				WLAN_SOC_CEXT_RX_FSE_SUPPORT)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "rx fse disabled in FW\n");
		wlan_cfg_set_rx_flow_tag_enabled(cfg, false);
		return QDF_STATUS_E_NOSUPPORT;
	}
	/**
	 * Func. is called for every pdev. If FST is per SOC, then return
	 * if it was already called once.
	 */
	if (!is_rx_flow_search_table_per_pdev && soc->rx_fst) {
		pdev->rx_fst = soc->rx_fst;
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST for SoC is already initialized");
		return QDF_STATUS_SUCCESS;
	}

	/**
	 * Func. is called for this pdev already. This is an error.
	 * Return failure
	 */
	if (is_rx_flow_search_table_per_pdev && pdev->rx_fst) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST for PDEV %u is already initialized",
			  pdev->pdev_id);
		return QDF_STATUS_E_EXISTS;
	}

	fst = qdf_mem_malloc(sizeof(struct dp_rx_fst));
	if (!fst) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "RX FST allocation failed\n");
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_set(fst, sizeof(struct dp_rx_fst), 0);

	if (!dp_rx_set_rr_reo_indication(&flow, &(fst->ring_id), cfg)) {
		fst->ring_id = 1;
	}

	fst->max_skid_length = wlan_cfg_rx_fst_get_max_search(cfg);
	fst->max_entries = wlan_cfg_get_rx_flow_search_table_size(cfg);
	hash_key = wlan_cfg_rx_fst_get_hash_key(cfg);

	if (!(fst->max_entries &&
	      (!(fst->max_entries & (fst->max_entries - 1))))) {
		uint32_t next_power_of_2 = fst->max_entries - 1;

		next_power_of_2 |= (next_power_of_2 >> 1);
		next_power_of_2 |= (next_power_of_2 >> 2);
		next_power_of_2 |= (next_power_of_2 >> 4);
		next_power_of_2 |= (next_power_of_2 >> 8);
		next_power_of_2 |= (next_power_of_2 >> 16);
		next_power_of_2++;
		if (next_power_of_2 > WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_MAX)
			next_power_of_2 =
				 WLAN_CFG_RX_FLOW_SEARCH_TABLE_SIZE_MAX;
		dp_info("Num entries in cfg is not a ^2:%u, using next ^2:%u",
			fst->max_entries, next_power_of_2);
		fst->max_entries = next_power_of_2;
	}
	fst->hash_mask = fst->max_entries - 1;
	fst->num_entries = 0;
	fst->fse_flow_create_cnt = 0;
	fst->fse_flow_del_cnt = 0;

	fst->base = (uint8_t *) qdf_mem_malloc(sizeof(struct dp_rx_fse) *
					       fst->max_entries);

	if (!fst->base) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Rx fst->base allocation failed, #entries:%d\n",
			  fst->max_entries);

		qdf_mem_free(fst);
		return QDF_STATUS_E_NOMEM;
	}

	qdf_mem_set((uint8_t *)fst->base,
		    (sizeof(struct dp_rx_fse) * fst->max_entries), 0);

	fst->hal_rx_fst = hal_rx_fst_attach(
				soc->hal_soc,
				soc->osdev,
				&fst->hal_rx_fst_base_paddr,
				fst->max_entries,
				fst->max_skid_length,
				hash_key, 0);

	if (qdf_unlikely(!fst->hal_rx_fst)) {
		QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
			  "Rx Hal fst allocation failed, #entries:%d\n",
			  fst->max_entries);
		qdf_mem_free(fst->base);
		qdf_mem_free(fst);
		return QDF_STATUS_E_NOMEM;
	}
	if (!is_rx_flow_search_table_per_pdev)
		soc->rx_fst = fst;

	pdev->rx_fst = fst;

	if (soc->is_rx_fse_full_cache_invalidate_war_enabled) {
		QDF_STATUS status;

		status = qdf_timer_init(
				soc->osdev,
				&fst->cache_invalidate_timer,
				dp_rx_flow_cache_invalidate_timer_handler,
				(void *)pdev,
				QDF_TIMER_TYPE_SW);

		if (dp_assert_always_internal(status == QDF_STATUS_SUCCESS)) {
			dp_err("Cache invalidate timer start failed");
		} else {
			/* Start the timer */
			qdf_timer_start(&fst->cache_invalidate_timer,
					HW_RX_FSE_CACHE_INVALIDATE_DELAYED_FST_SETUP_MS);

			qdf_atomic_set(&fst->is_cache_update_pending, false);
		}
	}

	dp_rx_ppe_fse_register();
	qdf_spinlock_create(&fst->fst_lock);

	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO,
		  "Rx FST attach successful, #entries:%d\n",
		  fst->max_entries);
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_fst_detach() - De-initialize Rx FST
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: None
 */
void dp_rx_fst_detach(struct dp_soc *soc, struct dp_pdev *pdev)
{
	struct dp_rx_fst *dp_fst;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;

	if (qdf_unlikely(wlan_cfg_is_rx_flow_search_table_per_pdev(cfg))) {
		dp_fst = pdev->rx_fst;
		pdev->rx_fst = NULL;
	} else {
		dp_fst = soc->rx_fst;
		soc->rx_fst = NULL;
	}

	if (soc->arch_ops.dp_set_rx_fst)
		soc->arch_ops.dp_set_rx_fst(NULL);
	dp_rx_ppe_fse_unregister();
	if (qdf_likely(dp_fst)) {
		hal_rx_fst_detach(soc->hal_soc, dp_fst->hal_rx_fst,
				  soc->osdev, 0);
		if (soc->is_rx_fse_full_cache_invalidate_war_enabled) {
			qdf_timer_sync_cancel(&dp_fst->cache_invalidate_timer);
			qdf_timer_stop(&dp_fst->cache_invalidate_timer);
			qdf_timer_free(&dp_fst->cache_invalidate_timer);
		}
		qdf_mem_free(dp_fst->base);
		qdf_mem_free(dp_fst);
	}

	qdf_spinlock_destroy(&dp_fst->fst_lock);

	QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_DEBUG,
		  "Rx FST detached for pdev %u\n", pdev->pdev_id);
}

/**
 * dp_rx_flow_send_fst_fw_setup() - Program FST parameters in FW/HW post-attach
 * @soc: SoC handle
 * @pdev: Pdev handle
 *
 * Return: Success when fst parameters are programmed in FW, error otherwise
 */
QDF_STATUS dp_rx_flow_send_fst_fw_setup(struct dp_soc *soc,
					struct dp_pdev *pdev)
{
	struct dp_htt_rx_flow_fst_setup fst_setup;
	struct dp_rx_fst *fst;
	QDF_STATUS status;
	struct wlan_cfg_dp_soc_ctxt *cfg = soc->wlan_cfg_ctx;

	if (qdf_unlikely(!wlan_cfg_is_rx_flow_tag_enabled(cfg)))
		return QDF_STATUS_SUCCESS;

	qdf_mem_set(&fst_setup, sizeof(struct dp_htt_rx_flow_fst_setup), 0);

	if (qdf_unlikely(wlan_cfg_is_rx_flow_search_table_per_pdev(cfg))) {
		/* Firmware pdev ID starts from 1 */
		fst_setup.pdev_id = DP_SW2HW_MACID(pdev->pdev_id);
		fst = pdev->rx_fst;
	} else {
		fst_setup.pdev_id = 0;
		fst = soc->rx_fst;
	}
	fst_setup.max_entries = fst->max_entries;
	fst_setup.max_search = fst->max_skid_length;
	fst_setup.base_addr_lo = (uint32_t)fst->hal_rx_fst_base_paddr;
	fst_setup.base_addr_hi =
		(uint32_t)((uint64_t)fst->hal_rx_fst_base_paddr >> 32);
	fst_setup.ip_da_sa_prefix =
		HAL_FST_IP_DA_SA_PFX_TYPE_IPV4_COMPATIBLE_IPV6;
	fst_setup.hash_key =  wlan_cfg_rx_fst_get_hash_key(cfg);
	fst_setup.hash_key_len = HAL_FST_HASH_KEY_SIZE_BYTES;

	status = dp_htt_rx_flow_fst_setup(pdev, &fst_setup);
	if (status == QDF_STATUS_SUCCESS) {
		fst->fse_setup_done = true;
		return status;
	}
	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
		  "Failed to send Rx FSE Setup pdev%d status %d\n",
		  pdev->pdev_id, status);
	/* Free all the memory allocations and data structures */
	dp_rx_fst_detach(pdev->soc, pdev);
	return status;
}

/** dp_mon_rx_update_rx_flow_tag_stats() - Update a mon flow's statistics
 * @pdev: pdev handle
 * @flow_id: flow index (truncated hash) in the Rx FST
 *
 * Return: Success when flow statistcs is updated, error on failure
 */
#ifdef QCA_TEST_MON_PF_TAGS_STATS
QDF_STATUS dp_mon_rx_update_rx_flow_tag_stats(struct dp_pdev *pdev,
					      uint32_t flow_id)
{
	struct dp_rx_fse *fse;

	fse = dp_rx_flow_find_entry_by_flowid(pdev->soc, pdev->rx_fst, flow_id);

	if (!fse) {
		dp_err("Flow not found, flow ID %u", flow_id);
		return QDF_STATUS_E_NOENT;
	}

	fse->stats.mon_msdu_count += 1;
	return QDF_STATUS_SUCCESS;
}
#else
QDF_STATUS dp_mon_rx_update_rx_flow_tag_stats(struct dp_pdev *pdev,
					      uint32_t flow_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif
qdf_export_symbol(dp_mon_rx_update_rx_flow_tag_stats);
#endif /* WLAN_SUPPORT_RX_FLOW_TAG */
