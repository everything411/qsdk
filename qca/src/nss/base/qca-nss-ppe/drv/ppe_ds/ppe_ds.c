/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>

#include "ppe_vp_public.h"
#include "ppe_ds.h"
#include "ppe_ds_stats.h"
#include <ppe_drv.h>

#define IDX_MGMT_PERIOD max_t(u64, 10000, NSEC_PER_SEC / idx_mgmt_freq)

/*
 * Callback structure to call into plugin module
 */
static struct ppe_ds_wlan_ops wlan_ops_cbs;

/*
 * ppe_ds_wlan_plugins_cb_register()
 * 	Callback for ppeds plugin registration
 */
int ppe_ds_wlan_plugins_cb_register(struct ppe_ds_wlan_ops *wlan_ops)
{

	if (!wlan_ops) {
		return -1;
	}

	memcpy(&wlan_ops_cbs, wlan_ops, sizeof(struct ppe_ds_wlan_ops));
	return 0;
}
EXPORT_SYMBOL(ppe_ds_wlan_plugins_cb_register);

/*
 * ppe_ds_wlan_plugins_cb_unregister()
 * 	Callback for ppeds plugin unregistration
 */
void ppe_ds_wlan_plugins_cb_unregister(void)
{
	memset(&wlan_ops_cbs, 0, sizeof(struct ppe_ds_wlan_ops));
}
EXPORT_SYMBOL(ppe_ds_wlan_plugins_cb_unregister);

/*
 * ppe_ds_ppe2tcl_rx()
 *	PPE-DS PPE2TCL Rx processing API
 */
static void ppe_ds_ppe2tcl_rx(nss_dp_ppeds_handle_t *edma_handle, uint16_t hw_prod_idx)
{
	struct ppe_ds *node = nss_dp_ppeds_priv(edma_handle);

	return node->wlan_ops->set_tcl_prod_idx(node->node_cfg_idx, hw_prod_idx);
}

/*
 * ppe_ds_ppe2tcl_fill()
 *	PPE-DS WLAN Tx descriptor and buffer fill API
 */
static uint32_t ppe_ds_ppe2tcl_fill(nss_dp_ppeds_handle_t *edma_handle, uint32_t num_buff_req, uint32_t buff_size,
		uint32_t headroom)
{
	struct ppe_ds *node = nss_dp_ppeds_priv(edma_handle);

	struct ppe_ds_wlan_txdesc_elem *tx_desc_arr = (struct ppe_ds_wlan_txdesc_elem *)nss_dp_ppeds_get_rx_fill_arr(edma_handle);

	return node->wlan_ops->get_tx_desc_many(node->node_cfg_idx, tx_desc_arr, num_buff_req, buff_size, headroom);
}

/*
 * ppe_ds_reo2ppe_tx_cmpl()
 *	PPE-DS WLAN Rx descriptor and buffer fill API
 */
static void ppe_ds_reo2ppe_tx_cmpl(nss_dp_ppeds_handle_t *edma_handle, uint16_t count)
{
	struct ppe_ds *node = nss_dp_ppeds_priv(edma_handle);

	struct ppe_ds_wlan_rxdesc_elem *rx_desc_arr = (struct ppe_ds_wlan_rxdesc_elem *)nss_dp_ppeds_get_tx_cmpl_arr(edma_handle);

	return node->wlan_ops->release_rx_desc(node->node_cfg_idx, rx_desc_arr, count);
}

/*
 * ppe_ds_ppe2tcl_rel()
 *	PPE-DS WLAN Tx descriptor and buffer release API
 */
static void ppe_ds_ppe2tcl_rel(nss_dp_ppeds_handle_t *edma_handle, uint64_t rx_opaque)
{
	struct ppe_ds *node = nss_dp_ppeds_priv(edma_handle);
	uint32_t cookie = (uint32_t)(rx_opaque) & 0x000fffff;

	return node->wlan_ops->release_tx_desc_single(node->node_cfg_idx, cookie);
}

/*
 * ppe_ds_notify_napi_done()
 *	PPE-DS ring NAPI done notification to WLAN.
 */
static void ppe_ds_notify_napi_done(nss_dp_ppeds_handle_t *edma_handle)
{
	struct ppe_ds *node = nss_dp_ppeds_priv(edma_handle);

	node->wlan_ops->notify_napi_done(node->node_cfg_idx);
}

/*
 * ppe_ds_timer()
 *	PPE-DS timer callback
 */
static enum hrtimer_restart ppe_ds_timer(struct hrtimer *hrtimer)
{
	uint32_t cons_idx, prod_idx, move;
	struct ppe_ds *node = container_of(hrtimer,  struct ppe_ds, timer);
	nss_dp_ppeds_handle_t *edma_handle = node->edma_handle;
	struct nss_dp_ppeds_wifi7_handle *wifi7_hdl = &edma_handle->wifi7_hdl;
	uint32_t ppe2tcl_ring_size = wifi7_hdl->ppe2tcl_num_desc;
	uint32_t reo2ppe_size = wifi7_hdl->reo2ppe_num_desc;
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &edma_handle->wifi8_hdl;

	if (edma_handle->wifi_arch_mode == PPE_DS_WIFI_ARCH_MODE_WIFI8) {
		/*
		 * TODO: Need to handle for multiring.
		 */
		ppe2tcl_ring_size = wifi8_hdl->data_ring.ring_info.ppe2tcl_num_desc[0];
		reo2ppe_size = wifi8_hdl->data_ring.ring_info.reo2ppe_num_desc[0];
	} else {
		ppe2tcl_ring_size = wifi7_hdl->ppe2tcl_num_desc;
		reo2ppe_size = wifi7_hdl->reo2ppe_num_desc;
	}

	/*
	 * Move Prod Idx
	 */
	prod_idx = dp_ops->get_rx_prod_idx(edma_handle);
	cons_idx = node->wlan_ops->get_tcl_cons_idx(node->node_cfg_idx);
	move = (prod_idx - cons_idx  + ppe2tcl_ring_size) &
		(ppe2tcl_ring_size - 1);
	if (move > 0) {
		node->wlan_ops->set_tcl_prod_idx(node->node_cfg_idx, prod_idx);
	}

	/*
	 * Move Cons Index
	 */
	dp_ops->set_rx_cons_idx(edma_handle, cons_idx);

	/*
	 * Move producer index for UL
	 */
	prod_idx = node->wlan_ops->get_reo_prod_idx(node->node_cfg_idx);
	cons_idx = dp_ops->get_tx_cons_idx(edma_handle);
	move = (prod_idx - cons_idx  + reo2ppe_size) & (reo2ppe_size - 1);
	if (move > 0) {
		dp_ops->set_tx_prod_idx(edma_handle, prod_idx);
	}

	/*
	 * Move consumer index for UL
	 */
	if (cons_idx != node->last_reo2ppe_cons_idx) {
		node->wlan_ops->set_reo_cons_idx(node->node_cfg_idx, cons_idx);
		node->last_reo2ppe_cons_idx = cons_idx;
	}

	hrtimer_forward(hrtimer, ktime_get(), ns_to_ktime(IDX_MGMT_PERIOD));
	return HRTIMER_RESTART;
}

/*
 * ppe_ds_ppe2tcl_idx_stats_set()
 * 	Populate statistics for timestamps
 * 	associated with PPE-to-TCL interrupts
 */
static void ppe_ds_ppe2tcl_ts_stats_set(int wintr, int node_idx)
{
	int cnt;
	u_int64_t cur_ts, ts_bt_intr;

	cur_ts = ktime_to_us(ktime_get_real());
	ts_bt_intr = cur_ts - atomic64_read(&prev_wlan_intr_ts[node_idx].ppe2tcl.prev_intr_ts);
	atomic64_set(&prev_wlan_intr_ts[node_idx].ppe2tcl.prev_intr_ts, cur_ts);

	for (cnt = 0; cnt < PPE_DS_TS_HYS_MAX; cnt++)
		if (ts_bt_intr >= ppe_ds_ts_hys_range.ppe_ds_ts_hys_range[cnt].start &&
				ts_bt_intr < ppe_ds_ts_hys_range.ppe_ds_ts_hys_range[cnt].end) {
			atomic64_inc(&ppe_ds_ts_hyst[node_idx].ppe2tcl.ts_bt_intr[cnt]);
			atomic64_add(wintr, &ppe_ds_ts_hyst[node_idx].ppe2tcl.waste_intr[cnt]);
			break;
		}
}

/*
 * ppe_ds_ppe2tcl_move_idx_stats_set()
 * 	Populate statistics on the number of
 * 	valid PPE-to-TCL ring entry buffers.
 */
static void ppe_ds_ppe2tcl_move_idx_stats_set(int move, int node_idx)
{
	int cnt;

	for (cnt = 0; cnt < PPE_DS_VLD_RING_ENTR_HYS_MAX; cnt++)
		if (move >= ppe_ds_idx_hys_range.vld_ring_entr_range[cnt].start &&
				move < ppe_ds_idx_hys_range.vld_ring_entr_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].ppe2tcl.vld_ring_entr[cnt]);
			break;
		}
}

/*
 * ppe_ds_ppe2tcl_cached_idx_stats_set()
 * 	Populate statistics for cached index
 * 	movements related to PPE-to-TCL interrupts.
 */
static void ppe_ds_ppe2tcl_cached_idx_stats_set(int prod_move, int cons_move, int node_idx)
{
	int cnt;

	for (cnt = 0; cnt < PPE_DS_IDX_MV_HYS_MAX; cnt++)
		if (cons_move >= ppe_ds_idx_hys_range.idx_mv_range[cnt].start &&
				cons_move < ppe_ds_idx_hys_range.idx_mv_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].ppe2tcl.cons_idx_mv[cnt]);
			break;
		}

	for (cnt = 0; cnt < PPE_DS_IDX_MV_HYS_MAX; cnt++)
		if (prod_move >= ppe_ds_idx_hys_range.idx_mv_range[cnt].start &&
				prod_move < ppe_ds_idx_hys_range.idx_mv_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].ppe2tcl.prod_idx_mv[cnt]);
			break;
		}
}

/*
 * ppe_ds_reo2ppe_idx_stats_set()
 * 	Populate statistics for timestamps
 * 	associated with REO-to-PPE interrupts.
 */
static void ppe_ds_reo2ppe_ts_stats_set(int wintr, int node_idx)
{
	int cnt;

	u_int64_t cur_ts, ts_bt_intr;

	cur_ts = ktime_to_us(ktime_get_real());
	ts_bt_intr = cur_ts - atomic64_read(&prev_wlan_intr_ts[node_idx].reo2ppe.prev_intr_ts);
	atomic64_set(&prev_wlan_intr_ts[node_idx].reo2ppe.prev_intr_ts, cur_ts);

	for (cnt = 0; cnt < PPE_DS_TS_HYS_MAX; cnt++)
		if (ts_bt_intr >= ppe_ds_ts_hys_range.ppe_ds_ts_hys_range[cnt].start &&
				ts_bt_intr < ppe_ds_ts_hys_range.ppe_ds_ts_hys_range[cnt].end) {
			atomic64_inc(&ppe_ds_ts_hyst[node_idx].reo2ppe.ts_bt_intr[cnt]);
			atomic64_add(wintr, &ppe_ds_ts_hyst[node_idx].reo2ppe.waste_intr[cnt]);
			break;
		}
}

/*
 * ppe_ds_reo2ppe_move_idx_stats_set()
 * 	Populate statistics on the number
 * 	of valid REO-to-PPE ring entry buffers
 */
static void ppe_ds_reo2ppe_move_idx_stats_set(int move, int node_idx)
{
	int cnt;

	for (cnt = 0; cnt < PPE_DS_VLD_RING_ENTR_HYS_MAX; cnt++)
		if (move >= ppe_ds_idx_hys_range.vld_ring_entr_range[cnt].start &&
				move < ppe_ds_idx_hys_range.vld_ring_entr_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].reo2ppe.vld_ring_entr[cnt]);
			break;
		}
}

/*
 * ppe_ds_reo2ppe_cached_idx_stats_set()
 * 	Populate statistics for cached index
 * 	movements related to REO-to-PPE interrupts.
 */
static void ppe_ds_reo2ppe_cached_idx_stats_set(int cons_move, int prod_move, int node_idx)
{
	int cnt;

	for (cnt = 0; cnt < PPE_DS_IDX_MV_HYS_MAX; cnt++)
		if (cons_move >= ppe_ds_idx_hys_range.idx_mv_range[cnt].start &&
				cons_move < ppe_ds_idx_hys_range.idx_mv_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].reo2ppe.cons_idx_mv[cnt]);
			break;
		}
	for (cnt = 0; cnt < PPE_DS_IDX_MV_HYS_MAX; cnt++)
		if (prod_move >= ppe_ds_idx_hys_range.idx_mv_range[cnt].start &&
				prod_move < ppe_ds_idx_hys_range.idx_mv_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].reo2ppe.prod_idx_mv[cnt]);
			break;
		}

}

/*
 * ppe_ds_edma_cached_idx_stats_set()
 * 	Populate statistics for cached index
 *      movements related to EDMA interrupts.
 */
static void ppe_ds_edma_cached_idx_stats_set(int prod_move, int node_idx)
{
	int cnt;

	for (cnt = 0; cnt < PPE_DS_IDX_MV_HYS_MAX; cnt++)
		if (prod_move >= ppe_ds_idx_hys_range.idx_mv_range[cnt].start &&
				prod_move < ppe_ds_idx_hys_range.idx_mv_range[cnt].end) {
			atomic_inc(&ppe_ds_idx_hyst[node_idx].edma_prod_mv[cnt]);
			break;
		}
}

/*
 * ppe_ds_ppe2tcl_wlan_handle_intr()
 *	PPE-DS PPE2TCL IRQ Tx processing API
 *
 * This is an interrupt handler for PPE2TCL ring.
 * Gets trggered periodically at configured time.
 *
 */
int ppe_ds_ppe2tcl_wlan_handle_intr(void *ctxt)
{
	uint32_t cons_idx, prod_idx, prev_cons_idx, prev_prod_idx;
	uint64_t cons_move = 0, prod_move = 0, move = 0;
	struct ppe_ds *node = (struct ppe_ds *)ctxt;
	nss_dp_ppeds_handle_t *edma_handle = node->edma_handle;
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;
	uint32_t wintr = 0;
	uint32_t ppe2tcl_ring_size;
	struct nss_dp_ppeds_wifi7_handle *wifi7_hdl = &edma_handle->wifi7_hdl;
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &edma_handle->wifi8_hdl;

	if (!node->en_process_irq) {
		if (node->umac_reset_inprogress) {
			node->wlan_ops->enable_tx_consume_intr(node->node_cfg_idx,
					false);
		}
		return 0;
	}

	/*
	 * Get prod and cons idx
	 */
	prod_idx = dp_ops->get_rx_prod_idx(edma_handle);
	cons_idx = node->wlan_ops->get_tcl_cons_idx(node->node_cfg_idx);

	/*
	 * Get cached prod and cons idx
	 */
	prev_cons_idx = node->last_ppe2tcl_cons_idx;
	prev_prod_idx = node->last_ppe2tcl_prod_idx;
	atomic64_inc(&ppe_ds_node_stats[node->node_cfg_idx].wlan_ppe2tcl_intr_cnt);

	if (edma_handle->wifi_arch_mode == PPE_DS_WIFI_ARCH_MODE_WIFI8) {
		ppe2tcl_ring_size = wifi8_hdl->data_ring.ring_info.ppe2tcl_num_desc[0];
	} else {
		ppe2tcl_ring_size = wifi7_hdl->ppe2tcl_num_desc;
	}

	/*
	 * Move Consumer Index
	 */
	if (likely(prev_cons_idx != cons_idx)) {
		dp_ops->set_rx_cons_idx(edma_handle, cons_idx);
		cons_move = (cons_idx - prev_cons_idx + ppe2tcl_ring_size) & (ppe2tcl_ring_size - 1);
		atomic64_add(cons_move, &ppe_ds_node_stats[node->node_cfg_idx].tx_pkts);
		node->last_ppe2tcl_cons_idx = cons_idx;
	}

	if (likely(prev_prod_idx != prod_idx)) {
		prod_move = (prod_idx - prev_prod_idx  + ppe2tcl_ring_size) & (ppe2tcl_ring_size - 1);
		node->last_ppe2tcl_prod_idx = prod_idx;
	}


	if (unlikely(prod_idx == cons_idx)) {
		/* Disable the wlan interrupt */
		node->wlan_ops->enable_tx_consume_intr(node->node_cfg_idx, false);
		/* Enable the edma interrupt */
		dp_ops->enable_rx_reap_intr(edma_handle);
		atomic64_inc(&ppe_ds_node_stats[node->node_cfg_idx].prod_n_con_same);
	} else {
		/*
		 * Move Producer Idx
		 */
		node->wlan_ops->set_tcl_prod_idx(node->node_cfg_idx, prod_idx);

		if (likely(prev_cons_idx != cons_idx)) {
			move = (prod_idx - cons_idx  + ppe2tcl_ring_size) &
				(ppe2tcl_ring_size - 1);
		}

		if (unlikely(prev_prod_idx == prod_idx)) {
			atomic64_inc(&ppe_ds_node_stats[node->node_cfg_idx].ppe2tcl_still_prod);
		}

		if (unlikely(prev_cons_idx == cons_idx)) {
			atomic64_inc(&ppe_ds_node_stats[node->node_cfg_idx].ppe2tcl_still_cons);
			wintr = 1;
		}

		if (unlikely(enable_ring_hyst_stats)) {
			/* Hysteresis for the timestamp of interrupt. */
			ppe_ds_ppe2tcl_ts_stats_set(wintr, node->node_cfg_idx);
			/* Hysteresis for the index movement of interrupt. */
			ppe_ds_ppe2tcl_move_idx_stats_set(move, node->node_cfg_idx);
		}
	}

	if (unlikely(enable_ring_hyst_stats)) {
		/* Hysteresis for the prod and cons index movement of interrupt. */
		ppe_ds_ppe2tcl_cached_idx_stats_set(cons_move, prod_move, node->node_cfg_idx);
	}

	return 0;
}
EXPORT_SYMBOL(ppe_ds_ppe2tcl_wlan_handle_intr);

/*
 * ppe_ds_reo2ppe_wlan_handle_intr()
 *	PPE-DS REO2PPE IRQ Rx processing API
 *
 * This is an interrupt handler for REO2PPE ring.
 * Gets trggered periodically at configured time,
 * whenever the head pointer moves.
 *
 */
int ppe_ds_reo2ppe_wlan_handle_intr(void *ctxt)
{
	uint32_t cons_idx, prod_idx, prev_cons_idx, prev_prod_idx, count;
	uint64_t move = 0, cons_move = 0, prod_move = 0;
	struct ppe_ds *node = (struct ppe_ds *)ctxt;
	nss_dp_ppeds_handle_t *edma_handle = node->edma_handle;
	uint32_t reo2ppe_size;
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;
	uint32_t wintr = 0;
	struct nss_dp_ppeds_wifi7_handle *wifi7_hdl = &edma_handle->wifi7_hdl;
	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &edma_handle->wifi8_hdl;

	if (!node->en_process_irq) {
		return 0;
	}

	atomic64_inc(&ppe_ds_node_stats[node->node_cfg_idx].wlan_reo2ppe_intr_cnt);

	/*
	 * Get prod and cons idx
	 */
	prod_idx = node->wlan_ops->get_reo_prod_idx(node->node_cfg_idx);
	cons_idx = dp_ops->get_tx_cons_idx(edma_handle);

	/*
	 * Get cached prod and cons idx.
	 */
	prev_cons_idx = node->last_reo2ppe_cons_idx;
	prev_prod_idx = node->last_reo2ppe_prod_idx;

	if (edma_handle->wifi_arch_mode == PPE_DS_WIFI_ARCH_MODE_WIFI8) {
		/*
		 * TODO: Need to handle for multiring.
		 */
		reo2ppe_size = wifi8_hdl->data_ring.ring_info.reo2ppe_num_desc[0];
	} else {
		reo2ppe_size = wifi7_hdl->reo2ppe_num_desc;
	}

	/*
	 * Move producer index for UL
	 */
	move = (prod_idx - cons_idx  + reo2ppe_size) & (reo2ppe_size - 1);
	if (move > 0) {
		dp_ops->set_tx_prod_idx(edma_handle, prod_idx);
		atomic64_add(move, &ppe_ds_node_stats[node->node_cfg_idx].rx_pkts);
	}

	/*
	 * Count how many packets are consumed by PPE.
	 * If it is same as prev consumer index then nothing will be added into the stats.
	 */
	count = (cons_idx - node->last_reo2ppe_cons_idx + reo2ppe_size) & (reo2ppe_size - 1);

	/*
	 * Move consumer index for UL
	 */
	if (cons_idx != node->last_reo2ppe_cons_idx) {
		node->wlan_ops->set_reo_cons_idx(node->node_cfg_idx, cons_idx);
		cons_move = (cons_idx - prev_cons_idx  + reo2ppe_size) & (reo2ppe_size - 1);
		node->last_reo2ppe_cons_idx = cons_idx;
	}

	atomic64_add(count, &ppe_ds_node_stats[node->node_cfg_idx].rx_pkts);

	if (prod_idx != prev_prod_idx) {
		prod_move = (prod_idx - prev_prod_idx  + reo2ppe_size) & (reo2ppe_size - 1);
		node->last_reo2ppe_prod_idx = prod_idx;
	}

	if (unlikely(prev_cons_idx == cons_idx)) {
		atomic64_inc(&ppe_ds_node_stats[node->node_cfg_idx].reo2ppe_still_cons);
		wintr = 1;
	}

	if (unlikely(enable_ring_hyst_stats)) {
		/* Hysteresis for the timestamp of interrupt. */
		ppe_ds_reo2ppe_ts_stats_set(wintr, node->node_cfg_idx);
		/* Hysteresis for the index movement of interrupt. */
		ppe_ds_reo2ppe_move_idx_stats_set(move, node->node_cfg_idx);
		/* Hysteresis for the cons index movement of interrupt. */
		ppe_ds_reo2ppe_cached_idx_stats_set(cons_move, prod_move, node->node_cfg_idx);
	}

	return 0;
}
EXPORT_SYMBOL(ppe_ds_reo2ppe_wlan_handle_intr);

static void ppe_ds_enable_wlan_intr(nss_dp_ppeds_handle_t *edma_handle,
		bool enable)
{
	struct ppe_ds *node = nss_dp_ppeds_priv(edma_handle);
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;
	uint32_t prod_idx;
	uint32_t ppe2tcl_ring_size;
	struct nss_dp_ppeds_wifi7_handle *wifi7_hdl = &edma_handle->wifi7_hdl;
	uint32_t prev_prod_idx, prod_move = 0;

	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &edma_handle->wifi8_hdl;
	prod_idx = dp_ops->get_rx_prod_idx(edma_handle);
	prev_prod_idx = node->last_edma_rx_prod_idx;

	if (edma_handle->wifi_arch_mode == PPE_DS_WIFI_ARCH_MODE_WIFI8) {
		ppe2tcl_ring_size = wifi8_hdl->data_ring.ring_info.ppe2tcl_num_desc[0];
	} else {
		ppe2tcl_ring_size = wifi7_hdl->ppe2tcl_num_desc;
	}

	/*
	 * Update tcl producer index before enabling wlan interrupt.
	 */
	node->wlan_ops->set_tcl_prod_idx(node->node_cfg_idx, prod_idx);
	node->wlan_ops->enable_tx_consume_intr(node->node_cfg_idx, true);

	if (likely(prev_prod_idx != prod_idx)) {
		prod_move = (prod_idx - prev_prod_idx  + ppe2tcl_ring_size) & (ppe2tcl_ring_size - 1);
		node->last_edma_rx_prod_idx = prod_idx;
	}

	if (unlikely(enable_ring_hyst_stats)) {
		/* Hysteresis for the prod index movement of interrupt. */
		ppe_ds_edma_cached_idx_stats_set(prod_move, node->node_cfg_idx);
	}
}

/*
 * ppe_ds_wlan_get_intr_ctxt
 * 	PPE-DS get wlan context
 */
void *ppe_ds_wlan_get_intr_ctxt(struct ppe_ds *node)
{
	return (void *)node;
}
EXPORT_SYMBOL(ppe_ds_wlan_get_intr_ctxt);

/*
 * ppe_ds_wlan_service_status_update()
 *	Umac reset service update.
 */
void ppe_ds_wlan_service_status_update(struct ppe_ds *node, bool enable)
{
	nss_dp_ppeds_handle_t *edma_handle = node->edma_handle;
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;

	if (!dp_ops || !dp_ops->service_status_update) {
		ppe_ds_err("NULL service status update API\n");
		return;
	}

	dp_ops->service_status_update(edma_handle, enable);
}
EXPORT_SYMBOL(ppe_ds_wlan_service_status_update);

/*
 * ppe_ds_wlan_get_rxfill_ring_info()
 *	Get RxFill ring filled-count information.
 */
void ppe_ds_wlan_get_rxfill_ring_info(struct ppe_ds *node,
				struct ppe_ds_wlan_rxfill_ring_info *info)
{
	nss_dp_ppeds_handle_t *edma_handle;
	struct nss_dp_ppeds_ops *dp_ops;
	struct nss_dp_ppeds_rxfill_ring_info dp_info = {};

	if (!node) {
		ppe_ds_err("NULL PPE-DS node in get_rxfill_ring_info\n");
		return;
	}

	edma_handle = node->edma_handle;
	dp_ops = node->dp_ops;

	if (!dp_ops || !dp_ops->get_rxfill_ring_info) {
		ppe_ds_err("NULL get_rxfill_ring_info API\n");
		return;
	}

	dp_info.arch_mode = info->arch_mode;
	dp_ops->get_rxfill_ring_info(edma_handle, &dp_info);

	switch (dp_info.arch_mode) {
	case EDMA_PPEDS_WIFI_ARCH_MODE_WIFI7:
		info->wifi7.prim_prod_idx	= dp_info.wifi7.prim_prod_idx;
		info->wifi7.prim_cons_idx	= dp_info.wifi7.prim_cons_idx;
		info->wifi7.prim_active_cnt	= dp_info.wifi7.prim_active_cnt;
		break;
	case EDMA_PPEDS_WIFI_ARCH_MODE_WIFI8:
		info->wifi8.prim_prod_idx	= dp_info.wifi8.prim_prod_idx;
		info->wifi8.prim_cons_idx	= dp_info.wifi8.prim_cons_idx;
		info->wifi8.prim_active_cnt	= dp_info.wifi8.prim_active_cnt;
		info->wifi8.secd_prod_idx	= dp_info.wifi8.secd_prod_idx;
		info->wifi8.secd_cons_idx	= dp_info.wifi8.secd_cons_idx;
		info->wifi8.secd_active_cnt	= dp_info.wifi8.secd_active_cnt;
		info->wifi8.hw_buff_mgmt_en	= dp_info.wifi8.hw_buff_mgmt_en;
		break;
	default:
		ppe_ds_err("Unknown arch_mode %u in get_rxfill_ring_info\n", dp_info.arch_mode);
		break;
	}
}
EXPORT_SYMBOL(ppe_ds_wlan_get_rxfill_ring_info);

/*
 * ppe_ds_get_cur_prod_cons_ring_idx_mode_wifi7()
 *	Get the current EDMA producer and consumer ring indices
 */
static void ppe_ds_get_cur_prod_cons_ring_idx_mode_wifi7(ppe_ds_wlan_handle_t *wlan_handle, struct ppe_ds_wlan_arch_reg_info *reg_info)
{
	struct ppe_ds *node = container_of(wlan_handle, struct ppe_ds, wlan_handle);
	nss_dp_ppeds_handle_t *edma_handle = node->edma_handle;
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;

	reg_info->wifi7_cfg.reo2ppe_start_idx = dp_ops->get_tx_cons_idx(edma_handle);
	dp_ops->set_tx_prod_idx(edma_handle, reg_info->wifi7_cfg.reo2ppe_start_idx);
	reg_info->wifi7_cfg.ppe2tcl_start_idx = dp_ops->get_rx_prod_idx(edma_handle);
	dp_ops->set_rx_cons_idx(edma_handle, reg_info->wifi7_cfg.ppe2tcl_start_idx);

	dp_ops->set_rxfill_prod_idx(edma_handle, dp_ops->get_rxfill_cons_idx(edma_handle));

	reg_info->wifi7_cfg.ppe_ds_int_mode_enabled = !polling_for_idx_update;

	ppe_ds_info("%px: PPE-DS get current EDMA ring indices API call successful", node);
	return;
}

/*
 * ppe_ds_wlan_rx()
 *	PPE-DS REO2PPE Rx processing API
 */
void ppe_ds_wlan_rx(ppe_ds_wlan_handle_t *wlan_handle, uint16_t reo_prod_idx)
{
	struct ppe_ds *node = container_of(wlan_handle, struct ppe_ds, wlan_handle);
	struct ppe_ds_node_config *node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);
	nss_dp_ppeds_handle_t *edma_handle = node->edma_handle;
	struct nss_dp_ppeds_ops *dp_ops = node->dp_ops;

	read_lock_bh(&node_cfg->lock);
	if(node_cfg->node_state != PPE_DS_NODE_STATE_START_DONE) {
		ppe_ds_err("Invalid node state: %d, PPE-DS REO2PPE processing API failed\n",
				node_cfg->node_state);
		read_unlock_bh(&node_cfg->lock);
		return;
	}
	read_unlock_bh(&node_cfg->lock);

	if (!dp_ops || !dp_ops->set_tx_prod_idx) {
		ppe_ds_err("NULL EDMA operation in REO2PPE processing API\n");
		return;
	}

	dp_ops->set_tx_prod_idx(edma_handle, reo_prod_idx);
}
EXPORT_SYMBOL(ppe_ds_wlan_rx);

/*
 * ppe_ds_wlan_vp_free()
 *	PPE-DS WLAN VP free API
 */
ppe_vp_status_t ppe_ds_wlan_vp_free(ppe_ds_wlan_handle_t *wlan_handle, ppe_vp_num_t vp_num)
{
	return ppe_vp_free(vp_num);
}
EXPORT_SYMBOL(ppe_ds_wlan_vp_free);

/*
 * ppe_ds_wlan_vp_alloc()
 *	PPE-DS WLAN VP alloc API
 */
ppe_vp_num_t ppe_ds_wlan_vp_alloc(ppe_ds_wlan_handle_t *wlan_handle, struct net_device *dev, struct ppe_vp_ai *vpai)
{
	uint32_t ppe_queue_start;
	struct ppe_ds *node;
	struct ppe_ds_node_config *node_cfg;
	nss_dp_ppeds_handle_t *edma_handle;
	struct nss_dp_ppeds_ops *dp_ops;

	if ((!wlan_handle) && (vpai->net_dev_flags != PPE_VP_NET_DEV_FLAG_IS_MLD)) {
		ppe_ds_err("wlan_handle is NULL for non MLD VP\n");
		return PPE_VP_STATUS_FAILURE;
	}

	if (vpai->net_dev_flags == PPE_VP_NET_DEV_FLAG_IS_MLD) {
		ppe_ds_info("MLD VP so skipping node to queue mapping\n");
		goto vp_alloc;
	}

	node = container_of(wlan_handle, struct ppe_ds, wlan_handle);
	dp_ops = node->dp_ops;
	if (!dp_ops || !dp_ops->get_queues) {
		ppe_ds_err("NULL EDMA operation, in PPE-DS VP alloc API\n");
		return PPE_VP_STATUS_FAILURE;
	}

	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);
	edma_handle = node->edma_handle;

	read_lock_bh(&node_cfg->lock);
	if(node_cfg->node_state != PPE_DS_NODE_STATE_START_DONE) {
		ppe_ds_err("Invalid node state: %d, PPE-DS vp alloc failed\n",
				node_cfg->node_state);
		read_unlock_bh(&node_cfg->lock);
		return PPE_VP_STATUS_FAILURE;
	}
	read_unlock_bh(&node_cfg->lock);

	dp_ops->get_queues(edma_handle, &ppe_queue_start);
	ppe_ds_info("%px: PPE-DS node mapped start queue-id: %d", node, ppe_queue_start);

	vpai->queue_num = ppe_queue_start;
vp_alloc:
	vpai->xmit_port = PPE_DRV_PORT_CPU;

	return ppe_vp_alloc(dev, vpai);
}
EXPORT_SYMBOL(ppe_ds_wlan_vp_alloc);

/*
 * ppe_ds_wlan_get_node_id()
 *	PPE-DS WLAN get node id API
 *	This API is called only in case of MLO
 */
uint32_t ppe_ds_wlan_get_node_id(ppe_ds_wlan_handle_t *wlan_handle)
{
	struct ppe_ds *node;
	struct ppe_ds_node_config *node_cfg;

	if (!wlan_handle) {
		ppe_ds_err("wlan_handle is NULL\n");
		return PPE_VP_DS_INVALID_NODE_ID;
	}

	node = container_of(wlan_handle, struct ppe_ds, wlan_handle);
	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);

	read_lock_bh(&node_cfg->lock);
	if(node_cfg->node_state != PPE_DS_NODE_STATE_START_DONE) {
		ppe_ds_err("Invalid node state: %d, PPE-DS wlan get node id failed\n",
				node_cfg->node_state);
		read_unlock_bh(&node_cfg->lock);
		return PPE_VP_DS_INVALID_NODE_ID;
	}

	read_unlock_bh(&node_cfg->lock);

	ppe_ds_info("node id of the wlan_handle %d\n", node->node_cfg_idx);

	return node->node_cfg_idx;
}
EXPORT_SYMBOL(ppe_ds_wlan_get_node_id);

/*
 * ppe_ds_wlan_inst_register_arch_mode_wifi7()
 *	PPE-DS WLAN instance registration API
 *
 * TODO: Currently the undone of works done in ppe_ds_wlan_inst_alloc and
 * ppe_ds_wlan_inst_register APIs is being happening in a single API,
 * (ppe_ds_wlan_inst_free). Try out to see if undone can be separately divided
 * properly.
 */
bool ppe_ds_wlan_inst_register_arch_mode_wifi7(struct ppe_ds *node, struct ppe_ds_wlan_arch_reg_info *reg_info)
{
	static unsigned int ppeds_node_iter_cnt;
	ppe_ds_node_state_t priv_node_state;
	struct ppe_ds_node_config *node_cfg;
	nss_dp_ppeds_handle_t *edma_handle;
	struct nss_dp_ppeds_wifi7_handle *wifi7_hdl;
	struct nss_dp_ppeds_ops *dp_ops;
	ppe_ds_wlan_handle_t *wlan_handle;
	unsigned int cpu;
	bool ret;
	uint32_t rxfill_min, rxfill_def;

	wlan_handle = &node->wlan_handle;
	if (!wlan_handle) {
		ppe_ds_err("wlan_handle is NULL\n");
		return false;
	}

	dp_ops = node->dp_ops;

	/*
	 * Validate EDMA operations which are part of data path also before
	 * enabling the HR timer callback
	 */
	if (!dp_ops || !dp_ops->reg || !dp_ops->set_rx_cons_idx ||
			!dp_ops->set_tx_prod_idx || !dp_ops->get_tx_cons_idx ||
			!dp_ops->get_rx_prod_idx) {
		ppe_ds_err("NULL EDMA operation in PPE-DS registration API\n");
		return false;
	}

	edma_handle = node->edma_handle;
	wifi7_hdl = &edma_handle->wifi7_hdl;
	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);

	write_lock_bh(&node_cfg->lock);
	priv_node_state = node_cfg->node_state;
	if ((priv_node_state != PPE_DS_NODE_STATE_ALLOC) &&
			(priv_node_state != PPE_DS_NODE_STATE_STOP_DONE)) {
		ppe_ds_err("Invalid node state: %d, PPE-DS registration failed\n",
				node_cfg->node_state);
		write_unlock_bh(&node_cfg->lock);
		return false;
	}
	node_cfg->node_state = PPE_DS_NODE_STATE_REG_IN_PROG;
	write_unlock_bh(&node_cfg->lock);

	/*
	 * During wifi up/down we could come here, get the current
	 * EDMA producer and consumer indices.
	 */
	if (priv_node_state == PPE_DS_NODE_STATE_STOP_DONE) {
		ppe_ds_get_cur_prod_cons_ring_idx_mode_wifi7(wlan_handle, reg_info);

		write_lock_bh(&node_cfg->lock);
		node_cfg->node_state = PPE_DS_NODE_STATE_REG_DONE;
		write_unlock_bh(&node_cfg->lock);

		/* Enable the edma interrupt */
		if (!polling_for_idx_update) {
			dp_ops->enable_rx_reap_intr(edma_handle);
		}

		return true;
	}

	if (polling_for_idx_update) {
		/*
		 * Currently assuming the below PPE-DS node to SoC mapping:
		 * 1st PPE-DS node is used by 2G SoC
		 * 2nd PPE-DS node is used by 6g SoC
		 * 3rd PPE-DS node is used by 5g high SoC
		 * 4th PPE-DS node is used by 5g low SoC
		 */
		ppeds_node_iter_cnt++;
		if (ppeds_node_iter_cnt > PPE_DS_MAX_NODE) {
			ppeds_node_iter_cnt = 1;
		}

		if (ppeds_node_iter_cnt == 1) {
			cpu = cpu_mask_2g;
		} else if (ppeds_node_iter_cnt == 2) {
			cpu = cpu_mask_6g;
		} else if (ppeds_node_iter_cnt == 3) {
			cpu = cpu_mask_5g_hi;
		} else if (ppeds_node_iter_cnt == 4) {
			cpu = cpu_mask_5g_lo;
		} else {
			ppe_ds_err("Invalid PPE-DS iteration count: %d\n",
					ppeds_node_iter_cnt);
			return false;
		}

		/*
		 * Setup dummy netdev for all the NAPIs associated with this node
		 */
		init_dummy_netdev(&node->napi_ndev);

		/*
		 * Init high res timer.
		 */
		hrtimer_init_and_bind(&node->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL, cpu);
		node->timer.function = ppe_ds_timer;
		ppe_ds_info("For PPE-DS node iteration count: %d, cpu mask is 0x%x\n",
				ppeds_node_iter_cnt, cpu);
	}

	wifi7_hdl->ppe2tcl_ba = reg_info->wifi7_cfg.ppe2tcl_ba;
	wifi7_hdl->reo2ppe_ba = reg_info->wifi7_cfg.reo2ppe_ba;
	wifi7_hdl->ppe2tcl_num_desc = reg_info->wifi7_cfg.ppe2tcl_num_desc;
	wifi7_hdl->reo2ppe_num_desc = reg_info->wifi7_cfg.reo2ppe_num_desc;
	wifi7_hdl->polling_for_idx_update = polling_for_idx_update;

	if (ppe_ds_active_mem_profile == PPE_DS_MEM_PROFILE_DYNAMIC_LOW) {
		rxfill_min = PPE_DS_RXFILL_NUM_DESC_MIN_LOW;
		rxfill_def = PPE_DS_RXFILL_NUM_DESC_DEF_LOW;
	} else {
		rxfill_min = PPE_DS_RXFILL_NUM_DESC_MIN;
		rxfill_def = PPE_DS_RXFILL_NUM_DESC_DEF;
	}

	node_cfg->freq.low_freq = reg_info->freq.low_freq;
	node_cfg->freq.high_freq = reg_info->freq.high_freq;

	if ((ppe2tcl_rxfill_num_desc < ppe_ds_rxfill_num_desc_min()) ||
			(ppe2tcl_rxfill_num_desc > PPE_DS_RXFILL_NUM_DESC_MAX)) {
		wifi7_hdl->ppe2tcl_rxfill_num_desc = rxfill_def;
	} else {
		wifi7_hdl->ppe2tcl_rxfill_num_desc = ppe2tcl_rxfill_num_desc;
	}

	if ((rxfill_budget < PPE_DS_RXFILL_BUDGET_MIN) ||
			(rxfill_budget > PPE_DS_RXFILL_BUDGET_MAX)) {
		wifi7_hdl->eth_rxfill_budget = PPE_DS_RXFILL_BUDGET_DEF;
	} else {
		wifi7_hdl->eth_rxfill_budget = rxfill_budget;
	}

	if ((reo2ppe_txcmpl_num_desc < PPE_DS_TXCMPL_NUM_DESC_MIN) ||
			(reo2ppe_txcmpl_num_desc > PPE_DS_TXCMPL_NUM_DESC_MAX)) {
		wifi7_hdl->reo2ppe_txcmpl_num_desc = PPE_DS_TXCMPL_NUM_DESC_DEF;
	} else {
		wifi7_hdl->reo2ppe_txcmpl_num_desc = reo2ppe_txcmpl_num_desc;
	}

	if (rxfill_low_threshold >= reg_info->wifi7_cfg.ppe2tcl_num_desc) {
		wifi7_hdl->eth_rxfill_low_thr =
			reg_info->wifi7_cfg.ppe2tcl_num_desc >> PPE_DS_RXFILL_LOW_THRES_DIVISOR;
	} else {
		wifi7_hdl->eth_rxfill_low_thr = rxfill_low_threshold;
	}

	if ((txcmpl_budget < PPE_DS_TXCMPL_MIN_BUDGET) ||
			(txcmpl_budget > wifi7_hdl->reo2ppe_num_desc)) {
		wifi7_hdl->eth_txcomp_budget = PPE_DS_TXCMPL_DEF_BUDGET;
	} else {
		wifi7_hdl->eth_txcomp_budget = txcmpl_budget;
	}

	if ((txcmpl_chunk_of_reap < PPE_DS_TXCMPL_MIN_BUDGET) ||
			(txcmpl_chunk_of_reap > wifi7_hdl->reo2ppe_num_desc)) {
		wifi7_hdl->eth_txcomp_chnk_of_reap = PPE_DS_TXCMPL_DEF_CHNK_OF_REAP;
	} else {
		wifi7_hdl->eth_txcomp_chnk_of_reap = txcmpl_chunk_of_reap;
	}

	ppe_ds_info(" ppe2tcl num desc: %d, reo2ppe num desc: %d, txcmpl budget: %d"
			" rxfill low threshold value: %d txcmp_chnk_of_reap:%d\n",
			wifi7_hdl->ppe2tcl_num_desc,
			wifi7_hdl->reo2ppe_num_desc,
			wifi7_hdl->eth_txcomp_budget,
			wifi7_hdl->eth_rxfill_low_thr,
			wifi7_hdl->eth_txcomp_chnk_of_reap);

	ret = dp_ops->reg(edma_handle);

	ppe_ds_get_cur_prod_cons_ring_idx_mode_wifi7(wlan_handle, reg_info);

	write_lock_bh(&node_cfg->lock);
	node_cfg->node_state = PPE_DS_NODE_STATE_REG_DONE;
	write_unlock_bh(&node_cfg->lock);

	/* Enable the edma interrupt */
	if (!polling_for_idx_update) {
		dp_ops->enable_rx_reap_intr(edma_handle);
	}

	ppe_ds_info("%px: PPE-DS register successful", node);
	return ret;
}
EXPORT_SYMBOL(ppe_ds_wlan_inst_register_arch_mode_wifi7);

/*
 * ppe_ds_print_wlan_arch_reg_info()
 *	Print contents of ppe_ds_wlan_arch_reg_info structure
 */
static void ppe_ds_print_wlan_arch_reg_info(struct ppe_ds_wlan_arch_reg_info *reg_info)
{
	struct ppe_ds_wlan_reg_data_ring_cfg *reg_data_ring;
	struct ppe_ds_wlan_reg_data_ring_hptp_cfg *txrx_ring_hptp_cfg;
	struct ppe_ds_wlan_reg_hbm_ring_cfg *reg_hw_ring;
	struct ppe_ds_wlan_reg_hbm_ring_hptp_cfg *reg_hw_tx_rx;
	int i;

	reg_data_ring = &reg_info->wifi8_cfg.data_ring.ring_info;
	txrx_ring_hptp_cfg = &reg_info->wifi8_cfg.data_ring.txrx_info;
	reg_hw_ring = &reg_info->wifi8_cfg.hw_buf_mgmt.ring_info;
	reg_hw_tx_rx = &reg_info->wifi8_cfg.hw_buf_mgmt.txrx_info;

	ppe_ds_info("=== ppe_ds_wlan_arch_reg_info contents ===\n");
	ppe_ds_info("data_ring_auto_index_en: %d\n", reg_info->wifi8_cfg.data_ring_auto_index_en);
	ppe_ds_info("hw_buff_mgmt_en: %d\n", reg_info->wifi8_cfg.hw_buff_mgmt_en);

	ppe_ds_info("Data Ring Info:\n");
	ppe_ds_info("  num_reo2ppe: %d\n", reg_data_ring->num_reo2ppe);
	ppe_ds_info("  num_ppe2tcl: %d\n", reg_data_ring->num_ppe2tcl);

	for (i = 0; i < PPE_DS_MAX_RING_PER_NODE; i++) {
		ppe_ds_info("  Ring[%d]: ppe2tcl_ba=0x%pad reo2ppe_ba=0x%pad\n",
			i, &reg_data_ring->ppe2tcl_ba[i], &reg_data_ring->reo2ppe_ba[i]);
		ppe_ds_info("  Ring[%d]: ppe2tcl_num_desc=%d reo2ppe_num_desc=%d\n",
			i, reg_data_ring->ppe2tcl_num_desc[i], reg_data_ring->reo2ppe_num_desc[i]);
	}

	ppe_ds_info("Data Ring TX/RX Info:\n");
	for (i = 0; i < PPE_DS_MAX_RING_PER_NODE; i++) {
		ppe_ds_info("  Ring[%d]: wlan_ppe2tcl_hp vaddr=%px paddr=%pad\n",
			i, txrx_ring_hptp_cfg->wlan_ppe2tcl_hp_addr[i].vaddr,
			&txrx_ring_hptp_cfg->wlan_ppe2tcl_hp_addr[i].paddr);
		ppe_ds_info("  Ring[%d]: wlan_reo2ppe_tp vaddr=%px paddr=%pad\n",
			i, txrx_ring_hptp_cfg->wlan_reo2ppe_tp_addr[i].vaddr,
			&txrx_ring_hptp_cfg->wlan_reo2ppe_tp_addr[i].paddr);
		ppe_ds_info("  Ring[%d]: edma_txdesc_prod vaddr=%px paddr=%pad\n",
			i, txrx_ring_hptp_cfg->edma_txdesc_prod_addr[i].vaddr,
			&txrx_ring_hptp_cfg->edma_txdesc_prod_addr[i].paddr);
		ppe_ds_info("  Ring[%d]: edma_rxdesc_cons vaddr=%px paddr=%pad\n",
			i, txrx_ring_hptp_cfg->edma_rxdesc_cons_addr[i].vaddr,
			&txrx_ring_hptp_cfg->edma_rxdesc_cons_addr[i].paddr);
	}

	ppe_ds_info("HW Buffer Management Ring Info:\n");
	ppe_ds_info("  tqm2ppe_ba: 0x%pad\n", &reg_hw_ring->tqm2ppe_ba);
	ppe_ds_info("  ppe2wbm_ba: 0x%pad\n", &reg_hw_ring->ppe2wbm_ba);
	ppe_ds_info("  tqm2ppe_num_desc: %d\n", reg_hw_ring->tqm2ppe_num_desc);
	ppe_ds_info("  ppe2wbm_num_desc: %d\n", reg_hw_ring->ppe2wbm_num_desc);

	ppe_ds_info("HW Buffer Management TX/RX Info:\n");
	ppe_ds_info("  edma_txcmpl_cons vaddr=%px paddr=%pad\n",
		reg_hw_tx_rx->edma_txcmpl_cons_addr.vaddr,
		&reg_hw_tx_rx->edma_txcmpl_cons_addr.paddr);
	ppe_ds_info("  edma_rxfill_prod vaddr=%px paddr=%pad\n",
		reg_hw_tx_rx->edma_rxfill_prod_addr.vaddr,
		&reg_hw_tx_rx->edma_rxfill_prod_addr.paddr);
	ppe_ds_info("  wlan_ppe2wbm_hp vaddr=%px paddr=%pad\n",
		reg_hw_tx_rx->wlan_ppe2wbm_hp_addr.vaddr,
		&reg_hw_tx_rx->wlan_ppe2wbm_hp_addr.paddr);
	ppe_ds_info("  wlan_tqm2ppe_tp vaddr=%px paddr=%pad\n",
		reg_hw_tx_rx->wlan_tqm2ppe_tp_addr.vaddr,
		&reg_hw_tx_rx->wlan_tqm2ppe_tp_addr.paddr);
	ppe_ds_info("=== End of ppe_ds_wlan_arch_reg_info ===\n");
}

/*
 * ppe_ds_wlan_inst_register_arch_mode_wifi8()
 *	PPE-DS WLAN instance registration API for wifi8 mode
 */
bool ppe_ds_wlan_inst_register_arch_mode_wifi8(struct ppe_ds *node, struct ppe_ds_wlan_arch_reg_info *reg_info)
{
	ppe_ds_node_state_t priv_node_state;
	struct ppe_ds_node_config *node_cfg;
	nss_dp_ppeds_handle_t *edma_handle;
	struct nss_dp_ppeds_ops *dp_ops;
	ppe_ds_wlan_handle_t *wlan_handle;
	struct nss_dp_ppeds_wifi8_handle *edma_wifi8;
	struct nss_dp_ppeds_wlan_reg_data_ring_cfg *edma_data_ring_info;
	struct nss_dp_ppeds_wlan_reg_data_ring_hptp_cfg *edma_data_tx_rx;
	struct nss_dp_ppeds_wlan_reg_hbm_ring_cfg *edma_hw_ring;
	struct nss_dp_ppeds_wlan_reg_hbm_ring_hptp_cfg *hbm_ring_hptp_cfg;
	struct ppe_ds_wlan_reg_data_ring_cfg *reg_data_ring;
	struct ppe_ds_wlan_reg_data_ring_hptp_cfg *txrx_ring_hptp_cfg;
	struct ppe_ds_wlan_reg_hbm_ring_cfg *reg_hw_ring;
	struct ppe_ds_wlan_reg_hbm_ring_hptp_cfg *reg_hw_tx_rx;
	bool ret;
	int i;
	uint32_t rxfill_min, rxfill_def;

	wlan_handle = &node->wlan_handle;
	if (!wlan_handle) {
		ppe_ds_err("wlan_handle is NULL\n");
		return false;
	}

	dp_ops = node->dp_ops;

	/*
	 * Validate EDMA operations which are part of data path also before
	 * enabling the HR timer callback
	 */
	if (!dp_ops || !dp_ops->reg || !dp_ops->set_rx_cons_idx ||
			!dp_ops->set_tx_prod_idx || !dp_ops->get_tx_cons_idx ||
			!dp_ops->get_rx_prod_idx) {
		ppe_ds_err("NULL EDMA operation in PPE-DS registration API\n");
		return false;
	}

	edma_handle = node->edma_handle;
	edma_wifi8 = &edma_handle->wifi8_hdl;
	edma_data_ring_info = &edma_wifi8->data_ring.ring_info;
	edma_data_tx_rx = &edma_wifi8->data_ring.txrx_info;
	edma_hw_ring = &edma_wifi8->hw_buf_mgmt.ring_info;
	hbm_ring_hptp_cfg = &edma_wifi8->hw_buf_mgmt.txrx_info;
	reg_data_ring = &reg_info->wifi8_cfg.data_ring.ring_info;
	txrx_ring_hptp_cfg = &reg_info->wifi8_cfg.data_ring.txrx_info;
	reg_hw_ring = &reg_info->wifi8_cfg.hw_buf_mgmt.ring_info;
	reg_hw_tx_rx = &reg_info->wifi8_cfg.hw_buf_mgmt.txrx_info;

	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);

	write_lock_bh(&node_cfg->lock);
	priv_node_state = node_cfg->node_state;
	if ((priv_node_state != PPE_DS_NODE_STATE_ALLOC) &&
			(priv_node_state != PPE_DS_NODE_STATE_STOP_DONE)) {
		ppe_ds_err("Invalid node state: %d, PPE-DS registration failed\n",
				node_cfg->node_state);
		write_unlock_bh(&node_cfg->lock);
		return false;
	}
	node_cfg->node_state = PPE_DS_NODE_STATE_REG_IN_PROG;
	write_unlock_bh(&node_cfg->lock);

	/*
	 * During wifi up/down we could come here, get the current
	 * EDMA producer and consumer indices.
	 */
	if (priv_node_state == PPE_DS_NODE_STATE_STOP_DONE &&
				(!node->umac_reset_inprogress)) {
		write_lock_bh(&node_cfg->lock);
		node_cfg->node_state = PPE_DS_NODE_STATE_REG_DONE;
		write_unlock_bh(&node_cfg->lock);

		/* Enable the edma interrupt */
		if (!edma_wifi8->data_ring_auto_index_en) {
			dp_ops->enable_rx_reap_intr(edma_handle);
		}

		return true;
	}

	edma_wifi8->data_ring_auto_index_en = reg_info->wifi8_cfg.data_ring_auto_index_en;
	edma_wifi8->hw_buff_mgmt_en = reg_info->wifi8_cfg.hw_buff_mgmt_en;

	edma_data_ring_info->num_reo2ppe = reg_data_ring->num_reo2ppe;
	edma_data_ring_info->num_ppe2tcl = reg_data_ring->num_ppe2tcl;

	for (i = 0; i < PPE_DS_MAX_RING_PER_NODE; i++) {
		edma_data_ring_info->ppe2tcl_ba[i] = reg_data_ring->ppe2tcl_ba[i];
		edma_data_ring_info->reo2ppe_ba[i] = reg_data_ring->reo2ppe_ba[i];
		edma_data_ring_info->ppe2tcl_num_desc[i] = reg_data_ring->ppe2tcl_num_desc[i];
		edma_data_ring_info->reo2ppe_num_desc[i] = reg_data_ring->reo2ppe_num_desc[i];
	}

	/*
	 * Data ring index registers for hw auto index configuration.
	 */
	for (i = 0; i < PPE_DS_MAX_RING_PER_NODE; i++) {
		edma_data_tx_rx->wlan_ppe2tcl_hp_addr[i].vaddr = txrx_ring_hptp_cfg->wlan_ppe2tcl_hp_addr[i].vaddr;
		edma_data_tx_rx->wlan_ppe2tcl_hp_addr[i].paddr = txrx_ring_hptp_cfg->wlan_ppe2tcl_hp_addr[i].paddr;

		edma_data_tx_rx->wlan_reo2ppe_tp_addr[i].vaddr = txrx_ring_hptp_cfg->wlan_reo2ppe_tp_addr[i].vaddr;
		edma_data_tx_rx->wlan_reo2ppe_tp_addr[i].paddr = txrx_ring_hptp_cfg->wlan_reo2ppe_tp_addr[i].paddr;

		edma_data_tx_rx->edma_txdesc_prod_addr[i].vaddr = txrx_ring_hptp_cfg->edma_txdesc_prod_addr[i].vaddr;
		edma_data_tx_rx->edma_txdesc_prod_addr[i].paddr = txrx_ring_hptp_cfg->edma_txdesc_prod_addr[i].paddr;

		edma_data_tx_rx->edma_rxdesc_cons_addr[i].vaddr = txrx_ring_hptp_cfg->edma_rxdesc_cons_addr[i].vaddr;
		edma_data_tx_rx->edma_rxdesc_cons_addr[i].paddr = txrx_ring_hptp_cfg->edma_rxdesc_cons_addr[i].paddr;
	}

	/*
	 * hw buffer manager configuration.
	 */
	edma_hw_ring->tqm2ppe_ba = reg_hw_ring->tqm2ppe_ba;
	edma_hw_ring->ppe2wbm_ba = reg_hw_ring->ppe2wbm_ba;
	edma_hw_ring->tqm2ppe_num_desc = reg_hw_ring->tqm2ppe_num_desc;
	edma_hw_ring->ppe2wbm_num_desc = reg_hw_ring->ppe2wbm_num_desc;

	hbm_ring_hptp_cfg->wlan_ppe2wbm_hp_addr.vaddr = reg_hw_tx_rx->wlan_ppe2wbm_hp_addr.vaddr;
	hbm_ring_hptp_cfg->wlan_ppe2wbm_hp_addr.paddr = reg_hw_tx_rx->wlan_ppe2wbm_hp_addr.paddr;

	hbm_ring_hptp_cfg->wlan_tqm2ppe_tp_addr.vaddr = reg_hw_tx_rx->wlan_tqm2ppe_tp_addr.vaddr;
	hbm_ring_hptp_cfg->wlan_tqm2ppe_tp_addr.paddr = reg_hw_tx_rx->wlan_tqm2ppe_tp_addr.paddr;

	if (ppe_ds_active_mem_profile == PPE_DS_MEM_PROFILE_DYNAMIC_LOW) {
		rxfill_min = PPE_DS_RXFILL_NUM_DESC_MIN_LOW;
		rxfill_def = PPE_DS_RXFILL_NUM_DESC_DEF_LOW;
	} else {
		rxfill_min = PPE_DS_RXFILL_NUM_DESC_MIN;
		rxfill_def = PPE_DS_RXFILL_NUM_DESC_DEF;
	}

	if ((ppe2tcl_rxfill_num_desc < ppe_ds_rxfill_num_desc_min()) ||
			(ppe2tcl_rxfill_num_desc > PPE_DS_RXFILL_NUM_DESC_MAX)) {
		edma_wifi8->ppe2tcl_rxfill_num_desc = rxfill_def;
	} else {
		edma_wifi8->ppe2tcl_rxfill_num_desc = ppe2tcl_rxfill_num_desc;
	}

	if ((rxfill_budget < PPE_DS_RXFILL_BUDGET_MIN) ||
			(rxfill_budget > PPE_DS_RXFILL_BUDGET_MAX)) {
		edma_wifi8->eth_rxfill_budget = PPE_DS_RXFILL_BUDGET_DEF;
	} else {
		edma_wifi8->eth_rxfill_budget = rxfill_budget;
	}

	if ((reo2ppe_txcmpl_num_desc < PPE_DS_TXCMPL_NUM_DESC_MIN) ||
			(reo2ppe_txcmpl_num_desc > PPE_DS_TXCMPL_NUM_DESC_MAX)) {
		edma_wifi8->reo2ppe_txcmpl_num_desc = PPE_DS_TXCMPL_NUM_DESC_DEF;
	} else {
		edma_wifi8->reo2ppe_txcmpl_num_desc = reo2ppe_txcmpl_num_desc;
	}

	/*
	 * TODO: Need to handle for multiring.
	 */
	if (rxfill_low_threshold >= reg_data_ring->ppe2tcl_num_desc[0]) {
		edma_wifi8->eth_rxfill_low_thr =
			reg_data_ring->ppe2tcl_num_desc[0] >> PPE_DS_RXFILL_LOW_THRES_DIVISOR;
	} else {
		edma_wifi8->eth_rxfill_low_thr = rxfill_low_threshold;
	}

	/*
	 * TODO: Need to handle for multiring.
	 */
	if ((txcmpl_budget < PPE_DS_TXCMPL_MIN_BUDGET) ||
			(txcmpl_budget > edma_data_ring_info->reo2ppe_num_desc[0])) {
		edma_wifi8->eth_txcomp_budget = PPE_DS_TXCMPL_DEF_BUDGET;
	} else {
		edma_wifi8->eth_txcomp_budget = txcmpl_budget;
	}

	if ((txcmpl_chunk_of_reap < PPE_DS_TXCMPL_MIN_BUDGET) ||
			(txcmpl_chunk_of_reap > edma_data_ring_info->reo2ppe_num_desc[0])) {
		edma_wifi8->eth_txcomp_chnk_of_reap = PPE_DS_TXCMPL_DEF_CHNK_OF_REAP;
	} else {
		edma_wifi8->eth_txcomp_chnk_of_reap = txcmpl_chunk_of_reap;
	}

	ppe_ds_info("Wifi8 Reg: auto_idx:%d hw_buff:%d ppe2tcl_fill:%d reo2ppe_txcmpl:%d "
			"rxfill_low:%d txcomp_bud:%d rxfill_bud:%d txcomp_reap:%d "
			"num_reo:%d num_ppe:%d ppe2tcl_desc:%d reo2ppe_desc:%d "
			"tqm2ppe_desc:%d ppe2wbm_desc:%d\n",
			edma_wifi8->data_ring_auto_index_en,
			edma_wifi8->hw_buff_mgmt_en,
			edma_wifi8->ppe2tcl_rxfill_num_desc,
			edma_wifi8->reo2ppe_txcmpl_num_desc,
			edma_wifi8->eth_rxfill_low_thr,
			edma_wifi8->eth_txcomp_budget,
			edma_wifi8->eth_rxfill_budget,
			edma_wifi8->eth_txcomp_chnk_of_reap,
			edma_data_ring_info->num_reo2ppe,
			edma_data_ring_info->num_ppe2tcl,
			edma_data_ring_info->ppe2tcl_num_desc[0],
			edma_data_ring_info->reo2ppe_num_desc[0],
			edma_hw_ring->tqm2ppe_num_desc,
			edma_hw_ring->ppe2wbm_num_desc);

	ret = dp_ops->reg(edma_handle);
	if (!ret) {
		ppe_ds_err("%px: PPE-DS register failed", node);

		/* Clean up configured resources */
		memset(edma_data_ring_info, 0, sizeof(*edma_data_ring_info));
		memset(edma_data_tx_rx, 0, sizeof(*edma_data_tx_rx));
		memset(edma_hw_ring, 0, sizeof(*edma_hw_ring));
		memset(hbm_ring_hptp_cfg, 0, sizeof(*hbm_ring_hptp_cfg));

		write_lock_bh(&node_cfg->lock);
		node_cfg->node_state = PPE_DS_NODE_STATE_ALLOC;
		write_unlock_bh(&node_cfg->lock);
		return ret;
	}

	/*
	 * copy the EDMA producer consumer register address for data ring auto index movement
	 */
	for (i = 0; i < PPE_DS_MAX_RING_PER_NODE; i++) {
		txrx_ring_hptp_cfg->edma_txdesc_prod_addr[i].vaddr = edma_data_tx_rx->edma_txdesc_prod_addr[i].vaddr;
		txrx_ring_hptp_cfg->edma_txdesc_prod_addr[i].paddr = edma_data_tx_rx->edma_txdesc_prod_addr[i].paddr;

		txrx_ring_hptp_cfg->edma_rxdesc_cons_addr[i].vaddr = edma_data_tx_rx->edma_rxdesc_cons_addr[i].vaddr;
		txrx_ring_hptp_cfg->edma_rxdesc_cons_addr[i].paddr = edma_data_tx_rx->edma_rxdesc_cons_addr[i].paddr;
	}

	/*
	 * copy the EDMA producer consumer register address for hardware buffer manager
	 */
	reg_hw_tx_rx->edma_txcmpl_cons_addr.vaddr = hbm_ring_hptp_cfg->edma_txcmpl_cons_addr.vaddr;
	reg_hw_tx_rx->edma_txcmpl_cons_addr.paddr = hbm_ring_hptp_cfg->edma_txcmpl_cons_addr.paddr;

	reg_hw_tx_rx->edma_rxfill_prod_addr.vaddr = hbm_ring_hptp_cfg->edma_rxfill_prod_addr.vaddr;
	reg_hw_tx_rx->edma_rxfill_prod_addr.paddr = hbm_ring_hptp_cfg->edma_rxfill_prod_addr.paddr;

	node_cfg->freq.low_freq = reg_info->freq.low_freq;
	node_cfg->freq.high_freq = reg_info->freq.high_freq;

	write_lock_bh(&node_cfg->lock);
	node_cfg->node_state = PPE_DS_NODE_STATE_REG_DONE;
	write_unlock_bh(&node_cfg->lock);

	/* Enable the edma interrupt */
	if (!edma_wifi8->data_ring_auto_index_en) {
		dp_ops->enable_rx_reap_intr(edma_handle);
	}

	ppe_ds_info("%px: PPE-DS register successful", node);

	/* Print contents of ppe_ds_wlan_arch_reg_info */
	ppe_ds_print_wlan_arch_reg_info(reg_info);

	return ret;
}
EXPORT_SYMBOL(ppe_ds_wlan_inst_register_arch_mode_wifi8);

/*
 * ppe_ds_wlan_instance_stop()
 *	PPE-DS WLAN instance stop API
 */
void ppe_ds_wlan_instance_stop(struct ppe_ds *node,
		struct ppe_ds_wlan_ctx_info_handle *wlan_info_hdl)
{
	struct ppe_ds_node_config *node_cfg;
	nss_dp_ppeds_handle_t *edma_handle;
	struct nss_dp_ppeds_ops *dp_ops;
	struct nss_ppe_ds_ctx_info_handle *info_hdl =
		(struct nss_ppe_ds_ctx_info_handle *)wlan_info_hdl;

	dp_ops = node->dp_ops;
	if (!dp_ops || !dp_ops->stop) {
		ppe_ds_err("NULL EDMA operation in PPE-DS stop API\n");
		return;
	}

	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);
	edma_handle = node->edma_handle;

	node->en_process_irq = false;
	node->umac_reset_inprogress = info_hdl->umac_reset_inprogress;

	write_lock_bh(&node_cfg->lock);
	if(node_cfg->node_state != PPE_DS_NODE_STATE_START_DONE) {
		ppe_ds_err("Invalid node state: %d, PPE-DS stop API failed\n",
				node_cfg->node_state);
		write_unlock_bh(&node_cfg->lock);
		return;
	}
	node_cfg->node_state = PPE_DS_NODE_STATE_STOP_IN_PROG;
	write_unlock_bh(&node_cfg->lock);

	if (polling_for_idx_update) {
		node->timer_enabled = false;
		hrtimer_cancel(&node->timer);
	}

	/*
	 * Stop EDMA.
	 */
	dp_ops->stop(edma_handle, PPE_DS_INTR_ENABLE, info_hdl);

	write_lock_bh(&node_cfg->lock);
	node_cfg->node_state = PPE_DS_NODE_STATE_STOP_DONE;
	write_unlock_bh(&node_cfg->lock);

	ppe_ds_info("%px: PPE-DS stop successful", node);
}
EXPORT_SYMBOL(ppe_ds_wlan_instance_stop);

/*
 * ppe_ds_wlan_instance_start()
 *	PPE-DS WLAN instance start API
 */
int ppe_ds_wlan_instance_start(struct ppe_ds *node,
		struct ppe_ds_wlan_ctx_info_handle *wlan_info_hdl)
{
	int ret;
	struct ppe_ds_node_config *node_cfg;
	nss_dp_ppeds_handle_t *edma_handle;
	struct nss_dp_ppeds_ops *dp_ops;
	struct nss_ppe_ds_ctx_info_handle *info_hdl =
		(struct nss_ppe_ds_ctx_info_handle *)wlan_info_hdl;
	uint32_t ppe2tcl_rxfill_num_desc;

	dp_ops = node->dp_ops;
	if (!dp_ops || !dp_ops->refill || !dp_ops->start) {
		ppe_ds_err("NULL EDMA operation in PPE-DS start API\n");
		return -1;
	}
	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);
	edma_handle = node->edma_handle;
	struct nss_dp_ppeds_wifi7_handle *wifi7_hdl = &edma_handle->wifi7_hdl;

	struct nss_dp_ppeds_wifi8_handle *wifi8_hdl = &edma_handle->wifi8_hdl;
	write_lock_bh(&node_cfg->lock);
	if(node_cfg->node_state != PPE_DS_NODE_STATE_REG_DONE) {
		if ((node_cfg->node_state != PPE_DS_NODE_STATE_STOP_DONE) ||
				(!node->umac_reset_inprogress)) {
			ppe_ds_err("Invalid node state: %d, PPE-DS start failed\n",
					node_cfg->node_state);
			write_unlock_bh(&node_cfg->lock);
			return -1;
		}
	}
	node_cfg->node_state = PPE_DS_NODE_STATE_START_IN_PROG;
	write_unlock_bh(&node_cfg->lock);

	if (edma_handle->wifi_arch_mode == PPE_DS_WIFI_ARCH_MODE_WIFI8) {
		ppe2tcl_rxfill_num_desc = wifi8_hdl->ppe2tcl_rxfill_num_desc;
	} else {
		ppe2tcl_rxfill_num_desc = wifi7_hdl->ppe2tcl_rxfill_num_desc;
	}

	dp_ops->refill(edma_handle, ppe2tcl_rxfill_num_desc - 1);

	if (polling_for_idx_update) {
		node->timer_enabled = true;
		hrtimer_start_range_ns_on_cpu(&node->timer,
				ns_to_ktime(IDX_MGMT_PERIOD),
				0, HRTIMER_MODE_REL_PINNED);
	}

	ret = dp_ops->start(edma_handle, PPE_DS_INTR_ENABLE, info_hdl);
	if ((ret != 0) && polling_for_idx_update) {
		node->timer_enabled = false;
		hrtimer_cancel(&node->timer);
	}

	node->umac_reset_inprogress = info_hdl->umac_reset_inprogress;
	write_lock_bh(&node_cfg->lock);
	node_cfg->node_state = PPE_DS_NODE_STATE_START_DONE;
	write_unlock_bh(&node_cfg->lock);

	node->en_process_irq = true;

	ppe_ds_info("%px: PPE-DS start successful\n", node);
	return ret;
}
EXPORT_SYMBOL(ppe_ds_wlan_instance_start);

/*
 * ppe_ds_wlan_inst_free()
 *	PPE-DS WLAN instance free API
 */
void ppe_ds_wlan_inst_free(struct ppe_ds *node)
{
	struct ppe_ds_node_config *node_cfg;
	struct nss_dp_ppeds_ops *dp_ops;
	nss_dp_ppeds_handle_t *edma_handle;
	ppe_drv_ret_t ret;
	uint8_t node_id;

	dp_ops = node->dp_ops;
	if (!dp_ops || !dp_ops->free) {
		ppe_ds_err("NULL EDMA operation in PPE-DS free API\n");
		return;
	}

	node_cfg = &(ppe_ds_node_cfg[node->node_cfg_idx]);
	edma_handle = node->edma_handle;

	write_lock_bh(&node_cfg->lock);
	if(node_cfg->node_state != PPE_DS_NODE_STATE_STOP_DONE) {
		ppe_ds_err("Invalid node state: %d, PPE-DS free failed\n",
				node_cfg->node_state);
		write_unlock_bh(&node_cfg->lock);
		return;
	}
	node_cfg->node_state = PPE_DS_NODE_STATE_FREE_IN_PROG;
	write_unlock_bh(&node_cfg->lock);

	/*
	 * Capture the node_id before node free,
	 * the same could be used for enqueue vp free.
	 */
	node_id = node->node_cfg_idx;

	dp_ops->free(edma_handle);

	/*
	 * Enqueue vport release for enqueue vp allocated during inst alloc.
	 */
	ret = ppe_drv_ds_map_free(node_id);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_ds_err("PPE-DS failed unmap for node_id:%d error:%d\n", node_id, ret);
		return;
	}

	write_lock_bh(&node_cfg->lock);
	node_cfg->node_state = PPE_DS_NODE_STATE_AVAIL;
	write_unlock_bh(&node_cfg->lock);
	ppe_ds_info("PPE-DS free successful");
}
EXPORT_SYMBOL(ppe_ds_wlan_inst_free);

/*
 * edma_ops_wifi
 *	EDMA PPE-DS operation callbacks for wifi
 */
static const struct nss_dp_ppeds_cb edma_ops_wifi =
{
	.rx = ppe_ds_ppe2tcl_rx,
	.rx_fill = ppe_ds_ppe2tcl_fill,
	.rx_release = ppe_ds_ppe2tcl_rel,
	.tx_cmpl = ppe_ds_reo2ppe_tx_cmpl,
	.enable_wlan_intr = ppe_ds_enable_wlan_intr,
	.notify_napi_done = ppe_ds_notify_napi_done,
};

/*
 * ppe_ds_wlan_inst_alloc()
 * 	wlan inst alloc returning PPEDS node id
 */
struct ppe_ds *ppe_ds_wlan_inst_alloc(struct ppe_ds_wlan_ops *ops, size_t priv_size)
{
	struct ppe_ds *node;
	nss_dp_ppeds_handle_t *edma_handle = NULL;
	int size = priv_size + sizeof(struct ppe_ds);
	struct nss_dp_ppeds_ops *dp_ops = NULL;
	uint32_t i;
	uint32_t ppe_queue_start;
	uint8_t ds_node_metadata;
	ppe_drv_ret_t ret;
	uint8_t wifi_arch_mode;

	if (ops && ops->get_wlan_arch_mode) {
		wifi_arch_mode = ops->get_wlan_arch_mode();
	} else {
		wifi_arch_mode = PPE_DS_WIFI_ARCH_MODE_WIFI7;
	}

	dp_ops = nss_dp_ppeds_get_wifi_arch_mode_ops(wifi_arch_mode);
	if (!dp_ops || !dp_ops->alloc) {
		ppe_ds_err("NULL EDMA operation in PPE-DS alloc API\n");
		return NULL;
	}

	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		write_lock_bh(&ppe_ds_node_cfg[i].lock);
		if (ppe_ds_node_cfg[i].node_state != PPE_DS_NODE_STATE_AVAIL) {
			write_unlock_bh(&ppe_ds_node_cfg[i].lock);
			continue;
		}
		break;
	}

	if(i == PPE_DS_MAX_NODE) {
		ppe_ds_err("Could not get a free PPE-DS node entry\n");
		return NULL;
	}

	ppe_ds_node_cfg[i].node_state = PPE_DS_NODE_STATE_NOT_AVAIL;
	write_unlock_bh(&ppe_ds_node_cfg[i].lock);

	edma_handle = dp_ops->alloc(&edma_ops_wifi, size);
	if (!edma_handle) {
		ppe_ds_err("Failed to get edma handle. alloc size requested: %d\n", size);
		goto error_restore_state;
	}

	/*
	 * Get queue id of node.
	 */
	dp_ops->get_queues(edma_handle, &ppe_queue_start);

	node = (struct ppe_ds *)nss_dp_ppeds_priv(edma_handle);
	node->wlan_ops = ops;
	node->dp_ops = dp_ops;
	node->edma_handle = edma_handle;
	node->node_cfg_idx = i;
	node->en_process_irq = false;
	node->umac_reset_inprogress = 0;

	/*
	 * Map the enqueue vp of node with queue id.
	 *
	 * PPE-DS flow use enqueue vp for PPE2TCL ring selection.
	 * Each enqueue vp is programmed on PPE VSI table at particular index and
	 * mapped to specific PPE queue. The PPE queue is further mapped to ring.
	 */
	ds_node_metadata = node->node_cfg_idx;
	ret = ppe_drv_ds_map_node_to_queue(ds_node_metadata, ppe_queue_start);
	if (ret != PPE_DRV_RET_SUCCESS) {
		ppe_ds_err("Unable to allocate enqueue vport for node:%d error:%d", node->node_cfg_idx, ret);
		goto error_restore_state;
	}

	write_lock_bh(&ppe_ds_node_cfg[i].lock);
	ppe_ds_node_cfg[i].node_state = PPE_DS_NODE_STATE_ALLOC;
	write_unlock_bh(&ppe_ds_node_cfg[i].lock);

	ppe_ds_info("%px: PPE-DS alloc successful\n", node);

	return node;

error_restore_state:
	if (edma_handle && dp_ops && dp_ops->free) {
		dp_ops->free(edma_handle);
	}

	write_lock_bh(&ppe_ds_node_cfg[i].lock);
	ppe_ds_node_cfg[i].node_state = PPE_DS_NODE_STATE_AVAIL;
	write_unlock_bh(&ppe_ds_node_cfg[i].lock);
	return NULL;
}
EXPORT_SYMBOL(ppe_ds_wlan_inst_alloc);

/*
 * ppe_ds_get_node_id()
 * 	API to return PPEDS node id
 */
uint32_t ppe_ds_get_node_id(struct ppe_ds *node)
{
	if (node) {
		return node->node_cfg_idx;
	}

	ppe_ds_err("Failed to get node id for a NULL ppeds node");
	return -1;
};
EXPORT_SYMBOL(ppe_ds_get_node_id);

/*
 * ppe_ds_get_wlan_node_freq()
 *	Update the radio channel frequency range for a PPE-DS node.
 *
 * Called when regulatory/channel information becomes available or changes
 * after the node was initially registered (at which point chan_info may
 * have been zero).
 */
void ppe_ds_get_wlan_node_freq(int node_id, struct ppe_ds_wlan_node_freq *freq)
{
	struct ppe_ds_node_config *node_cfg;

	if (!freq) {
		ppe_ds_err("NULL freq pointer for node_id %d", node_id);
		return;
	}

	if (node_id < 0 || node_id >= PPE_DS_MAX_NODE) {
		ppe_ds_err("Invalid node_id %d for freq update", node_id);
		return;
	}

	node_cfg = &ppe_ds_node_cfg[node_id];

	write_lock_bh(&node_cfg->lock);
	node_cfg->freq = *freq;
	write_unlock_bh(&node_cfg->lock);

	ppe_ds_info("node %d freq updated: low=%u MHz high=%u MHz", node_id, freq->low_freq, freq->high_freq);
}
EXPORT_SYMBOL(ppe_ds_get_wlan_node_freq);
