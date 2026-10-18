/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * DOC: contains ipa component main function definitions
 */

#include "wlan_ipa_main.h"
#include "wlan_ipa_core.h"
#include "wlan_ipa_tgt_api.h"
#include "cfg_ucfg_api.h"
#include "wlan_ipa_obj_mgmt_api.h"

static struct wlan_ipa_config *g_ipa_config;
static bool g_ipa_hw_support;
static bool g_ipa_pld_enable = true;
static bool g_ipa_shared_smmu_enable;
static bool g_ipa_cap_offload = true;

void ipa_set_cap_offload(bool flag)
{
	g_ipa_cap_offload = flag;
}

void ipa_set_pld_enable(bool flag)
{
	g_ipa_pld_enable = flag;
}

bool ipa_get_pld_enable(void)
{
	return (g_ipa_pld_enable && g_ipa_cap_offload);
}

void ipa_set_shared_smmu_enable(bool flag)
{
	g_ipa_shared_smmu_enable = flag;
}

bool ipa_get_shared_smmu_enable(void)
{
	return g_ipa_shared_smmu_enable;
}

bool ipa_check_hw_present(void)
{
	/* Check if ipa hw is enabled */
	if (qdf_ipa_uc_reg_rdyCB(NULL) != -EPERM) {
		g_ipa_hw_support = true;
		return true;
	} else {
		return false;
	}
}

QDF_STATUS ipa_config_mem_alloc(void)
{
	struct wlan_ipa_config *ipa_cfg;

	if (g_ipa_config)
		return QDF_STATUS_SUCCESS;

	ipa_cfg = qdf_mem_malloc(sizeof(*ipa_cfg));
	if (!ipa_cfg)
		return QDF_STATUS_E_NOMEM;

	g_ipa_config = ipa_cfg;

	return QDF_STATUS_SUCCESS;
}

void ipa_config_mem_free(void)
{
	if (!g_instances_added) {
		if (!g_ipa_config) {
			ipa_err("IPA config already freed");
			return;
		}

		qdf_mem_free(g_ipa_config);
		g_ipa_config = NULL;
	}
}

bool ipa_is_hw_support(void)
{
	return g_ipa_hw_support;
}

bool ipa_config_is_enabled(void)
{
	return g_ipa_config ? wlan_ipa_is_enabled(g_ipa_config) : 0;
}

bool ipa_config_is_uc_enabled(void)
{
	return g_ipa_config ? wlan_ipa_uc_is_enabled(g_ipa_config) : 0;
}

bool ipa_config_is_opt_wifi_dp_enabled(void)
{
	return g_ipa_config ? wlan_ipa_is_opt_wifi_dp_enabled(g_ipa_config) : 0;
}

bool ipa_config_is_vlan_enabled(void)
{
	if (!ipa_config_is_enabled())
		return false;

	return g_ipa_config ? g_ipa_config->ipa_vlan_support : 0;
}

bool ipa_config_is_two_tx_pipes_enabled(void)
{
	return g_ipa_config ? (ipa_config_is_enabled() ?
		wlan_ipa_is_two_tx_pipes_enabled(g_ipa_config) : 0) :
		0;
}

QDF_STATUS ipa_obj_setup(struct wlan_ipa_priv *ipa_ctx)
{
	return wlan_ipa_setup(ipa_ctx, g_ipa_config);
}

QDF_STATUS ipa_obj_cleanup(struct wlan_ipa_priv *ipa_ctx)
{
	return wlan_ipa_cleanup(ipa_ctx);
}

QDF_STATUS ipa_send_uc_offload_enable_disable(struct wlan_objmgr_psoc *psoc,
					      struct ipa_uc_offload_control_params *req)
{
	return tgt_ipa_uc_offload_enable_disable(psoc, req);
}

QDF_STATUS
ipa_send_intrabss_enable_disable(struct wlan_objmgr_psoc *psoc,
				 struct ipa_intrabss_control_params *req)
{
	return tgt_ipa_intrabss_enable_disable(psoc, req);
}

void ipa_set_dp_handle(struct wlan_objmgr_psoc *psoc, void *dp_soc)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	ipa_obj->dp_soc = dp_soc;
}

QDF_STATUS ipa_rm_set_perf_level(struct wlan_objmgr_pdev *pdev,
				 uint64_t tx_packets, uint64_t rx_packets)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_set_perf_level(ipa_obj, tx_packets, rx_packets);
}

void ipa_uc_info(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_info(ipa_obj);
}

void ipa_uc_stat(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_stat(ipa_obj);
}

#ifdef IPA_OPT_WIFI_DP_CTRL
void ipa_set_opt_dp_ctrl_flt(struct wlan_objmgr_pdev *pdev,
			     struct ipa_wdi_opt_dpath_flt_add_cb_params *flt)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_wdi_opt_dpath_ctrl_flt_add_cb(ipa_obj, flt);
}

void ipa_set_opt_dp_ctrl_flt_rm(struct wlan_objmgr_pdev *pdev,
				struct ipa_wdi_opt_dpath_flt_rem_cb_params *flt)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_wdi_opt_dpath_ctrl_flt_rem_cb_wrapper(ipa_obj, flt);
}
#else
void ipa_set_opt_dp_ctrl_flt(struct wlan_objmgr_pdev *pdev,
			     struct ipa_wdi_opt_dpath_flt_add_cb_params *flt)
{
}

void ipa_set_opt_dp_ctrl_flt_rm(struct wlan_objmgr_pdev *pdev,
				struct ipa_wdi_opt_dpath_flt_rem_cb_params *flt)
{
}
#endif

void ipa_uc_rt_debug_host_dump(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_rt_debug_host_dump(ipa_obj);
}

void ipa_dump_info(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_dump_info(ipa_obj);
}

void ipa_uc_stat_request(struct wlan_objmgr_pdev *pdev, uint8_t reason)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_stat_request(ipa_obj, reason);
}

void ipa_uc_stat_query(struct wlan_objmgr_pdev *pdev,
		       uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_stat_query(ipa_obj, ipa_tx_diff, ipa_rx_diff);
}

void ipa_reg_sap_xmit_cb(struct wlan_objmgr_pdev *pdev, wlan_ipa_softap_xmit cb)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;
	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_sap_xmit_cb(ipa_obj, cb);
}

void ipa_reg_send_to_nw_cb(struct wlan_objmgr_pdev *pdev,
			   wlan_ipa_send_to_nw cb)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_send_to_nw_cb(ipa_obj, cb);
}

#if defined(QCA_CONFIG_RPS) && !defined(MDM_PLATFORM)
void ipa_reg_rps_enable_cb(struct wlan_objmgr_pdev *pdev,
			   wlan_ipa_rps_enable cb)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_rps_enable_cb(ipa_obj, cb);
}
#endif

void ipa_reg_is_driver_unloading_cb(struct wlan_objmgr_pdev *pdev,
				    wlan_ipa_driver_unloading cb)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_reg_is_driver_unloading_cb(ipa_obj, cb);
}

void ipa_set_mcc_mode(struct wlan_objmgr_pdev *pdev, bool mcc_mode)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_set_mcc_mode(ipa_obj, mcc_mode);
}

void ipa_set_dfs_cac_tx(struct wlan_objmgr_pdev *pdev, bool tx_block)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_set_dfs_cac_tx(ipa_obj, tx_block);
}

void ipa_set_ap_ibss_fwd(struct wlan_objmgr_pdev *pdev, uint8_t session_id,
			 bool intra_bss)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_set_ap_ibss_fwd(ipa_obj, session_id, intra_bss);
}

void ipa_uc_force_pipe_shutdown(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		ipa_debug("objmgr pdev is null!");
		return;
	}

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	psoc = wlan_pdev_get_psoc(pdev);

	if (!psoc) {
		ipa_debug("objmgr psoc is null!");
		return;
	}

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_uc_disable_pipes(ipa_obj, true);
}

void ipa_flush(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return;
	}

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_flush(ipa_obj);
}

QDF_STATUS ipa_suspend(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_suspend(ipa_obj);
}

QDF_STATUS ipa_resume(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_resume(ipa_obj);
}

QDF_STATUS ipa_uc_ol_init(struct wlan_objmgr_psoc *psoc,
			  qdf_device_t osdev)
{
	struct wlan_ipa_priv *ipa_obj;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_uc_ol_init(ipa_obj, osdev);
}

bool ipa_is_tx_pending(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);

	return wlan_ipa_is_tx_pending(ipa_obj);
}

QDF_STATUS ipa_uc_ol_deinit(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
	QDF_STATUS status;

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	ipa_init_deinit_lock();

	if (!ipa_cb_is_ready()) {
		ipa_debug("ipa is not ready");
		status = QDF_STATUS_SUCCESS;
		goto out;
	}

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		status = QDF_STATUS_E_FAILURE;
		goto out;
	}

	if (!(ipa_obj->handle_initialized)) {
		ipa_debug("IPA is already deinit for hdl:%d", ipa_obj->hdl);
		status = QDF_STATUS_SUCCESS;
		goto out;
	}

	status = wlan_ipa_uc_ol_deinit(ipa_obj);
	ipa_obj_cleanup(ipa_obj);

	if (g_instances_added)
		g_instances_added--;

	if (!g_instances_added)
		ipa_disable_register_cb();

out:
	ipa_init_deinit_unlock();
	return status;
}

QDF_STATUS ipa_send_mcc_scc_msg(struct wlan_objmgr_pdev *pdev,
				bool mcc_mode)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug("ipa is disabled");
		return QDF_STATUS_SUCCESS;
	}

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_send_mcc_scc_msg(ipa_obj, mcc_mode);
}

QDF_STATUS ipa_wlan_evt(struct wlan_objmgr_pdev *pdev, qdf_netdev_t net_dev,
			uint8_t device_mode, uint8_t session_id,
			enum wlan_ipa_wlan_event ipa_event_type,
			const uint8_t *mac_addr, bool is_2g_iface)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_wlan_evt(net_dev, device_mode, session_id,
				 ipa_event_type, mac_addr, is_2g_iface,
				 ipa_obj);
}

int ipa_uc_smmu_map(bool map, uint32_t num_buf, qdf_mem_info_t *buf_arr)
{
	return wlan_ipa_uc_smmu_map(map, num_buf, buf_arr);
}

bool ipa_is_fw_wdi_activated(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled()) {
		ipa_debug_rl("ipa is disabled");
		return false;
	}

	if (!ipa_cb_is_ready()) {
		ipa_debug("ipa is not ready");
		return false;
	}

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err_rl("IPA object is NULL");
		return false;
	}

	return wlan_ipa_is_fw_wdi_activated(ipa_obj);
}

void ipa_uc_cleanup_sta(struct wlan_objmgr_pdev *pdev,
			qdf_netdev_t net_dev, uint8_t session_id)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_cb_is_ready()) {
		ipa_debug("ipa is not ready");
		return;
	}

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_cleanup_sta(ipa_obj, net_dev, session_id);
}

QDF_STATUS ipa_uc_disconnect_ap(struct wlan_objmgr_pdev *pdev,
				qdf_netdev_t net_dev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_cb_is_ready())
		return QDF_STATUS_SUCCESS;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return wlan_ipa_uc_disconnect_ap(ipa_obj, net_dev);
}

void ipa_cleanup_dev_iface(struct wlan_objmgr_pdev *pdev,
			   qdf_netdev_t net_dev, uint8_t session_id)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_cleanup_dev_iface(ipa_obj, net_dev, session_id);
}

void ipa_uc_shutdown_opt_dp_ctrl_cleanup(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_shutdown_opt_dp_ctrl_cleanup(ipa_obj);
}

void ipa_uc_ssr_cleanup(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_uc_ssr_cleanup(ipa_obj);
}

void ipa_fw_rejuvenate_send_msg(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc;

	if (!pdev) {
		ipa_debug("objmgr pdev is null!");
		return;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		ipa_debug("objmgr psoc is null!");
		return;
	}

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	return wlan_ipa_fw_rejuvenate_send_msg(ipa_obj);
}

void ipa_component_config_update(struct wlan_objmgr_psoc *psoc)
{
	QDF_STATUS status;

	status = ipa_config_mem_alloc();
	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to alloc g_ipa_config");
		return;
	}

	if (g_ipa_pld_enable && g_ipa_cap_offload) {
		g_ipa_config->ipa_config = get_ipa_config(psoc);
		ipa_debug("IPA ini configuration: 0x%x",
			  g_ipa_config->ipa_config);
	} else {
		g_ipa_config->ipa_config = 0;
		ipa_info("IPA disabled from platform driver");
	}

	g_ipa_config->desc_size =
		cfg_get(psoc, CFG_DP_IPA_DESC_SIZE);
	g_ipa_config->txbuf_count =
		qdf_rounddown_pow_of_two(cfg_get(psoc,
						 CFG_DP_IPA_UC_TX_BUF_COUNT));
	g_ipa_config->ipa_bw_high =
		cfg_get(psoc, CFG_DP_IPA_HIGH_BANDWIDTH_MBPS);
	g_ipa_config->ipa_bw_medium =
		cfg_get(psoc, CFG_DP_IPA_MEDIUM_BANDWIDTH_MBPS);
	g_ipa_config->ipa_bw_low =
		cfg_get(psoc, CFG_DP_IPA_LOW_BANDWIDTH_MBPS);
	g_ipa_config->bus_bw_high =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_HIGH_THRESHOLD);
	g_ipa_config->bus_bw_medium =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_MEDIUM_THRESHOLD);
	g_ipa_config->bus_bw_low =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_LOW_THRESHOLD);
	g_ipa_config->ipa_force_voting =
		cfg_get(psoc, CFG_DP_IPA_ENABLE_FORCE_VOTING);
	g_ipa_config->ipa_wds =
		cfg_get(psoc, CFG_DP_IPA_WDS_STATUS);
	g_ipa_config->ipa_vlan_support =
		cfg_get(psoc, CFG_DP_IPA_ENABLE_VLAN_SUPPORT);
}

void ipa_component_config_free(void)
{
	ipa_info("Free the IPA config memory");
	ipa_config_mem_free();
}

uint32_t ipa_get_tx_buf_count(void)
{
	return g_ipa_config ? g_ipa_config->txbuf_count : 0;
}

void ipa_update_tx_stats(struct wlan_objmgr_pdev *pdev, uint64_t sta_tx,
			 uint64_t ap_tx)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_update_tx_stats(ipa_obj, sta_tx, ap_tx);
}

void ipa_flush_pending_vdev_events(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled())
		return;

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_flush_pending_vdev_events(ipa_obj, vdev_id);
}

bool ipa_is_wds_enabled(void)
{
	return g_ipa_config ? g_ipa_config->ipa_wds : 0;
}

QDF_STATUS ipa_get_alt_pipe(struct wlan_objmgr_pdev *pdev,
			    uint8_t vdev_id,
			    bool *alt_pipe)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled())
		return QDF_STATUS_E_INVAL;

	if (!ipa_cb_is_ready())
		return QDF_STATUS_E_INVAL;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_INVAL;
	}

	return wlan_ipa_get_alt_pipe(ipa_obj, vdev_id, alt_pipe);
}

bool ipa_set_perf_level_bw_enabled(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled())
		return false;

	if (!ipa_cb_is_ready())
		return false;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return false;
	}

	return wlan_ipa_set_perf_level_bw_enabled(ipa_obj);
}

void ipa_set_perf_level_bw(struct wlan_objmgr_pdev *pdev,
			   enum wlan_ipa_bw_level lvl)
{
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

	if (!ipa_config_is_enabled())
		return;

	if (!ipa_cb_is_ready())
		return;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return;
	}

	wlan_ipa_set_perf_level_bw(ipa_obj, lvl);
}

#if defined(IPA_OFFLOAD) && defined(QCA_IPA_LL_TX_FLOW_CONTROL)
/**
 * ipa_event_wq() - Queue WLAN IPA event for later processing
 * @psoc: psoc handle
 * @peer_mac_addr: peer mac address
 * @vdev: vdev object
 * @wlan_event: wlan event
 *
 * Return: None
 */
void ipa_event_wq(struct wlan_objmgr_psoc *psoc, uint8_t *peer_mac_addr,
		  struct wlan_objmgr_vdev *vdev,
		  enum wlan_ipa_wlan_event wlan_event)
{
	struct wlan_ipa_priv *ipa_obj =
		wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_IPA);
	struct wlan_ipa_evt_wq_args *ipa_ctx = NULL;
	struct wlan_objmgr_pdev *pdev = psoc->soc_objmgr.wlan_pdev_list[0];
	QDF_STATUS ret;

	if (!ipa_obj) {
		qdf_err("IPA_object is NULL !!");
		return;
	}

	ipa_ctx = qdf_mem_malloc(sizeof(struct wlan_ipa_evt_wq_args));
	if (!ipa_ctx) {
		qdf_err("Memory alloc failed for IPA_CTX !!");
		return;
	}

	qdf_spin_lock_bh(&ipa_obj->ipa_evt_wq->list_lock);
	qdf_mem_copy(ipa_ctx->mac_addr, peer_mac_addr, QDF_MAC_ADDR_SIZE);
	ipa_ctx->vdev = vdev;
	ret = wlan_objmgr_vdev_try_get_ref(ipa_ctx->vdev, WLAN_IPA_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_mem_free(ipa_ctx);
		return;
	}

	ipa_ctx->pdev_obj = pdev;
	ipa_ctx->net_dev = vdev->vdev_nif.osdev->wdev->netdev;
	ipa_ctx->ch_freq = vdev->vdev_mlme.bss_chan->ch_freq;
	ipa_ctx->device_mode = wlan_vdev_mlme_get_opmode(vdev);
	ipa_ctx->vdev_id = wlan_vdev_get_id(vdev);
	ipa_ctx->event = wlan_event;

	TAILQ_INSERT_TAIL(&ipa_obj->ipa_evt_wq->list, ipa_ctx, list_elem);

	qdf_spin_unlock_bh(&ipa_obj->ipa_evt_wq->list_lock);

	qdf_queue_work(0, ipa_obj->ipa_evt_wq->work_queue, &ipa_obj->ipa_evt_wq->work);
}

static
void wlan_ipa_obj_ipa_evt_wq_handler(void *ctx)
{
	struct wlan_ipa_evt_wq_args *ipa_ctx, *ipa_ctx_next;
	struct wlan_ipa_priv *ipa_obj = (struct wlan_ipa_priv *)ctx;

	TAILQ_HEAD(, wlan_ipa_evt_wq_args) ipa_ctx_list;

	TAILQ_INIT(&ipa_ctx_list);

	qdf_spin_lock_bh(&ipa_obj->ipa_evt_wq->list_lock);

	TAILQ_CONCAT(&ipa_ctx_list, &ipa_obj->ipa_evt_wq->list, list_elem);
	qdf_spin_unlock_bh(&ipa_obj->ipa_evt_wq->list_lock);

	TAILQ_FOREACH_SAFE(ipa_ctx, &ipa_ctx_list, list_elem, ipa_ctx_next) {
		TAILQ_REMOVE(&ipa_ctx_list, ipa_ctx, list_elem);
		if (!ipa_ctx->pdev_obj) {
			wlan_objmgr_vdev_release_ref(ipa_ctx->vdev, WLAN_IPA_ID);
			qdf_mem_free(ipa_ctx);
			continue;
		}
		ipa_wlan_evt(ipa_ctx->pdev_obj, ipa_ctx->net_dev,
			     ipa_ctx->device_mode, ipa_ctx->vdev_id,
			     ipa_ctx->event, ipa_ctx->mac_addr,
			     ipa_ctx->ch_freq);

		/* Clean Interface for STA/AP disconnect event */
		if ((ipa_ctx->event == WLAN_IPA_AP_DISCONNECT) ||
		    (ipa_ctx->event == WLAN_IPA_STA_DISCONNECT))
			ipa_flush_pending_vdev_events(ipa_ctx->pdev_obj,
						      ipa_ctx->vdev_id);

		if (ipa_ctx->event == WLAN_IPA_AP_DISCONNECT)
			ipa_cleanup_dev_iface(ipa_ctx->pdev_obj,
					      ipa_ctx->net_dev,
					      ipa_ctx->vdev_id);

		wlan_objmgr_vdev_release_ref(ipa_ctx->vdev, WLAN_IPA_ID);
		qdf_mem_free(ipa_ctx);
	}
}

/**
 * wlan_psoc_ipa_evt_wq_attach() - Create WQ to handle IPA event
 * @psoc: psoc handle
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_psoc_ipa_evt_wq_attach(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_ipa_priv *ipa_obj =
		wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_IPA);

	if (!ipa_obj)
		return QDF_STATUS_E_FAILURE;

	ipa_obj->ipa_evt_wq = qdf_mem_malloc(sizeof(struct wlan_ipa_evt_wq));

	if (!ipa_obj->ipa_evt_wq)
		return QDF_STATUS_E_FAILURE;

	TAILQ_INIT(&ipa_obj->ipa_evt_wq->list);

	qdf_create_work(0, &ipa_obj->ipa_evt_wq->work,
			wlan_ipa_obj_ipa_evt_wq_handler, ipa_obj);

	ipa_obj->ipa_evt_wq->work_queue =
		qdf_alloc_unbound_workqueue("wlan_ipa_evt_work_queue");

	if (!ipa_obj->ipa_evt_wq->work_queue)
		goto fail;

	qdf_spinlock_create(&ipa_obj->ipa_evt_wq->list_lock);
	return QDF_STATUS_SUCCESS;

fail:
	qdf_flush_work(&ipa_obj->ipa_evt_wq->work);
	qdf_disable_work(&ipa_obj->ipa_evt_wq->work);
	qdf_mem_free(ipa_obj->ipa_evt_wq);
	ipa_obj->ipa_evt_wq = NULL;
	return QDF_STATUS_E_FAILURE;
}

qdf_export_symbol(wlan_psoc_ipa_evt_wq_attach);

/**
 * wlan_psoc_ipa_evt_wq_detach() - Detach WQ which handle IPA event
 * @psoc: psoc handle
 *
 * Return: None
 */
void wlan_psoc_ipa_evt_wq_detach(struct wlan_objmgr_psoc *psoc)
{
	struct wlan_ipa_evt_wq_args *ctx, *ctx_next;
	struct wlan_ipa_priv *ipa_obj =
		wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_IPA);

	if (!ipa_obj || !(ipa_obj->ipa_evt_wq &&
			  ipa_obj->ipa_evt_wq->work_queue))
		return;

	qdf_flush_workqueue(0, ipa_obj->ipa_evt_wq->work_queue);
	qdf_destroy_workqueue(0, ipa_obj->ipa_evt_wq->work_queue);
	qdf_flush_work(&ipa_obj->ipa_evt_wq->work);
	qdf_disable_work(&ipa_obj->ipa_evt_wq->work);
	qdf_spin_lock_bh(&ipa_obj->ipa_evt_wq->list_lock);

	TAILQ_FOREACH_SAFE(ctx, &ipa_obj->ipa_evt_wq->list, list_elem,
			   ctx_next) {
		TAILQ_REMOVE(&ipa_obj->ipa_evt_wq->list, ctx, list_elem);
		wlan_objmgr_vdev_release_ref(ctx->vdev, WLAN_IPA_ID);
		qdf_mem_free(ctx);
	}

	qdf_spin_unlock_bh(&ipa_obj->ipa_evt_wq->list_lock);
	qdf_spinlock_destroy(&ipa_obj->ipa_evt_wq->list_lock);
	qdf_mem_free(ipa_obj->ipa_evt_wq);
	ipa_obj->ipa_evt_wq = NULL;
}

qdf_export_symbol(wlan_psoc_ipa_evt_wq_detach);
#endif
