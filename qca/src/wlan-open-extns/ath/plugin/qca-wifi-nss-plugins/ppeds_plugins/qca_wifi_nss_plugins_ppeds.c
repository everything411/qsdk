/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <qca_wifi_nss_plugins.h>
#include <qca_wifi_nss_plugins_ppeds.h>
#include <ppe_public.h>

/*
 * qca_wifi_nss_plugins_ppeds_get_batched_tx_desc
 * 	get WLAN Tx descriptors and buffers
 */
static uint32_t qca_wifi_nss_plugins_ppeds_get_batched_tx_desc(int node_id, struct ppe_ds_wlan_txdesc_elem *arr, uint32_t num_buff_req, uint32_t buff_size, uint32_t headroom)
{
	return ath12k_ppeds_get_batched_tx_desc_v2(node_id, arr, num_buff_req, buff_size, headroom);
}

/*
 * qca_wifi_nss_plugins_ppeds_release_tx_desc_single
 * 	release WLAN Tx descriptor and buffer
 */
static void qca_wifi_nss_plugins_ppeds_release_tx_desc_single(int node_id, uint32_t cookie)
{
	ath12k_ppeds_release_tx_desc_single_v2(node_id, cookie);
}

/*
 * qca_wifi_nss_plugins_ppeds_enable_srng_intr
 * 	toggle wlan interrupt
 */
static inline void qca_wifi_nss_plugins_ppeds_enable_srng_intr(int node_id, bool enable)
{
	ath12k_ppeds_enable_srng_intr_v2(node_id, enable);
}

/*
 * qca_wifi_nss_plugins_ppeds_set_tcl_prod_idx
 * 	set PPE2TCL ring's producer index
 */
static void qca_wifi_nss_plugins_ppeds_set_tcl_prod_idx(int node_id, uint16_t tcl_prod_idx)
{
	ath12k_ppeds_set_tcl_prod_idx_v2(node_id, tcl_prod_idx);
}

/*
 * qca_wifi_nss_plugins_ppeds_set_reo_cons_idx
 * 	set REO2PPE ring's consumer index
 */
static void qca_wifi_nss_plugins_ppeds_set_reo_cons_idx(int node_id, uint16_t reo_cons_idx)
{
	ath12k_ppeds_set_reo_cons_idx_v2(node_id, reo_cons_idx);
}

/*
 * qca_wifi_nss_plugins_ppeds_get_tcl_cons_idx
 * 	get PPE2TCL ring's consumer index
 */
static uint16_t qca_wifi_nss_plugins_ppeds_get_tcl_cons_idx(int node_id)
{
	return ath12k_ppeds_get_tcl_cons_idx_v2(node_id);
}

/*
 * qca_wifi_nss_plugins_ppeds_get_reo_prod_idx
 * 	get REO2PPE ring's producer index
 */
static uint16_t qca_wifi_nss_plugins_ppeds_get_reo_prod_idx(int node_id)
{
	return ath12k_ppeds_get_reo_prod_idx_v2(node_id);
}

/*
 * qca_wifi_nss_plugins_ppeds_release_rx_desc
 * 	release WLAN Rx descriptors and buffers
 */
static void qca_wifi_nss_plugins_ppeds_release_rx_desc(int node_id, struct ppe_ds_wlan_rxdesc_elem *arr, uint16_t count)
{
	ath12k_ppeds_release_rx_desc_v2(node_id, arr, count);
}

/**
 * ppeds_ops
 *  PPE-DS WLAN operations
 */
static struct ppe_ds_wlan_ops_v2 ppeds_ops = {
	.get_tx_desc_many = qca_wifi_nss_plugins_ppeds_get_batched_tx_desc,
	.release_tx_desc_single = qca_wifi_nss_plugins_ppeds_release_tx_desc_single,
	.enable_tx_consume_intr = qca_wifi_nss_plugins_ppeds_enable_srng_intr,
	.set_tcl_prod_idx  = qca_wifi_nss_plugins_ppeds_set_tcl_prod_idx,
	.set_reo_cons_idx = qca_wifi_nss_plugins_ppeds_set_reo_cons_idx,
	.get_tcl_cons_idx = qca_wifi_nss_plugins_ppeds_get_tcl_cons_idx,
	.get_reo_prod_idx = qca_wifi_nss_plugins_ppeds_get_reo_prod_idx,
	.release_rx_desc = qca_wifi_nss_plugins_ppeds_release_rx_desc,
};

/*
 * qca_wifi_nss_plugins_ppeds_cb_register
 * 	Register the cbs
 */
int qca_wifi_nss_plugins_ppeds_cb_register(void)
{
	if (ppe_ds_wlan_plugins_cb_register(&ppeds_ops)) {
		qca_wifi_nss_plugins_info("qca-wifi-nss-plugins ppeds ops cbs registration failed\n");
		return -1;
	}

	qca_wifi_nss_plugins_info("qca-wifi-nss-plugins ppeds ops cbs registered\n");
	return 0;
}

/*
 * qca_wifi_nss_plugins_ppeds_cb_unregister
 * 	Unregister the cbs
 */
void qca_wifi_nss_plugins_ppeds_cb_unregister(void)
{
	ppe_ds_wlan_plugins_cb_unregister();
	qca_wifi_nss_plugins_info("qca-wifi-nss-plugins ppeds ops cbs unregistered\n");
}

