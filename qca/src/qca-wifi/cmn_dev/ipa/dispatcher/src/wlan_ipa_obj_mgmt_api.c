/*
 * Copyright (c) 2018, 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: public API related to the wlan ipa called by north bound HDD/OSIF
 */

#include "wlan_ipa_obj_mgmt_api.h"
#include "wlan_ipa_main.h"
#include "wlan_objmgr_global_obj.h"
#include <wlan_objmgr_global_obj_i.h>
#include "target_if_ipa.h"
#include "wlan_ipa_ucfg_api.h"
#include "qdf_platform.h"
#include "qdf_module.h"
#include "wlan_ipa_core.h"
#include "cfg_ucfg_api.h"

/* This is as per IPA capbility */
#define MAX_INSTANCES_SUPPORTED 2
#define IPA_CLK_ENABLE_WAIT_TIME_MS 500

uint8_t g_instances_added;
static bool g_ipa_is_ready;
qdf_mutex_t g_init_deinit_lock;

bool ipa_cb_is_ready(void)
{
	return g_ipa_is_ready;
}

void ipa_disable_register_cb(void)
{
	ipa_debug("Don't register ready cb with IPA driver");
	g_ipa_is_ready = false;
}

qdf_export_symbol(ipa_disable_register_cb);

void ipa_init_deinit_lock(void)
{
	qdf_mutex_acquire(&g_init_deinit_lock);
}

void ipa_init_deinit_unlock(void)
{
	qdf_mutex_release(&g_init_deinit_lock);
}

/**
 * ipa_psoc_obj_destroy_notification() - IPA psoc object destroy notification
 * @psoc: psoc handle
 * @arg_list: arguments list
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
ipa_psoc_obj_destroy_notification(struct wlan_objmgr_psoc *psoc,
				  void *arg_list)
{
	QDF_STATUS status;
	struct wlan_ipa_priv *ipa_obj;

	ipa_debug("ipa psoc destroyed");

	ipa_obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							WLAN_UMAC_COMP_IPA);
	if (!ipa_obj) {
		ipa_err("Failed to get ipa psoc object");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_psoc_component_obj_detach(psoc,
						       WLAN_UMAC_COMP_IPA,
						       ipa_obj);
	if (QDF_IS_STATUS_ERROR(status))
		ipa_err("Failed to detach ipa psoc object");

	qdf_mem_free(ipa_obj);

	return status;
}

/**
 * ipa_psoc_obj_create_notification() - IPA psoc object creation notification
 * @psoc: psoc handle
 * @arg_list: arguments list
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
ipa_psoc_obj_create_notification(struct wlan_objmgr_psoc *psoc,
				 void *arg_list)
{
	QDF_STATUS status;
	struct wlan_ipa_priv *ipa_obj;

	ipa_debug("ipa psoc created");

	ipa_obj = qdf_mem_malloc(sizeof(*ipa_obj));
	if (!ipa_obj)
		return QDF_STATUS_E_NOMEM;

	status = wlan_objmgr_psoc_component_obj_attach(psoc,
						       WLAN_UMAC_COMP_IPA,
						       (void *)ipa_obj,
						       QDF_STATUS_SUCCESS);
	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to attach psoc ipa component");
		qdf_mem_free(ipa_obj);
		return status;
	}

	ipa_obj->psoc = psoc;

	ipa_debug("ipa psoc attached");

	return status;
}

static void ipa_register_ready_cb(void *user_data)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc;
	qdf_device_t qdf_dev;
	qdf_ipa_wdi_capabilities_out_params_t out_param;

	if (!ipa_config_is_enabled()) {
		ipa_info("IPA config is disabled");
		return;
	}

	if (!user_data) {
		ipa_err("User_data object is NULL");
		return;
	}

	/* Validate driver state to determine ipa_obj is valid or not */
	if (qdf_is_driver_state_module_stop()) {
		ipa_err("Driver modules stop in-progress or done");
		return;
	}

	ipa_init_deinit_lock();

	/*
	 * Meanwhile acquiring lock, driver stop modules can happen in parallel,
	 * validate driver state once again to proceed with IPA init.
	 */
	if (qdf_is_driver_state_module_stop()) {
		ipa_err("Driver modules stop in-progress/done, releasing lock");
		goto out;
	}

	g_ipa_is_ready = true;
	ipa_info("IPA ready callback invoked: ipa_register_ready_cb");

	/* Make call to get num_instances supported by IPA */
	qdf_ipa_wdi_get_capabilities(&out_param);

	ipa_obj = (struct wlan_ipa_priv *)user_data;

	psoc = ipa_obj->psoc;

	if (!psoc) {
		qdf_err("Psoc is NULL");
		goto out;
	}

	if (ipa_obj->handle_initialized) {
		ipa_info("ipa_obj hdl is true for psoc_id %d",
			 psoc->soc_objmgr.psoc_id);
		goto out;
	}

	/* Update instance_id for current pdev */
	ipa_obj->instance_id = psoc->soc_objmgr.psoc_id;

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!qdf_dev) {
		ipa_err("QDF device context is NULL");
		goto out;
	}

	status = ipa_obj_setup(ipa_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		g_ipa_is_ready = false;
		ipa_err("Failed to ipa_obj_setup");
		goto out;
	}
	if (ucfg_ipa_uc_ol_init(psoc, qdf_dev)) {
		ipa_err("IPA ucfg_ipa_uc_ol_init failed");
		ipa_obj_cleanup(ipa_obj);
		g_ipa_is_ready = false;
		goto out;
	}

	ipa_obj->handle_initialized = true;
	g_instances_added++;
	ipa_info("No. of instances added for IPA is %d", g_instances_added);
out:
	ipa_init_deinit_unlock();
}

QDF_STATUS ipa_register_is_ipa_ready(struct wlan_objmgr_pdev *pdev)
{
	int ret;
	struct wlan_ipa_priv *ipa_obj;
	struct wlan_objmgr_psoc *psoc;

	if (!ipa_config_is_enabled()) {
		ipa_info("IPA config is disabled");
		return QDF_STATUS_SUCCESS;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		ipa_err("PSOC object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (ipa_obj->handle_initialized) {
		ipa_info("ipa ready cb registered for psoc_id %d",
			 psoc->soc_objmgr.psoc_id);
		return QDF_STATUS_SUCCESS;
	}

	ret = qdf_ipa_register_ipa_ready_cb(ipa_register_ready_cb,
					    (void *)ipa_obj);
	if (ret == -EEXIST) {
		ipa_info("IPA is ready, invoke callback");
		ipa_register_ready_cb((void *)ipa_obj);
	} else if (ret) {
		ipa_err("Failed to check IPA readiness %d", ret);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(ipa_register_is_ipa_ready);

QDF_STATUS ipa_init(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	ipa_info("ipa module dispatcher init");

	if (!ipa_check_hw_present()) {
		ipa_info("ipa hw not present");
		return status;
	}

	status = wlan_objmgr_register_psoc_create_handler(WLAN_UMAC_COMP_IPA,
							  ipa_psoc_obj_create_notification,
							  NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to register pdev create handler for ipa");

		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(WLAN_UMAC_COMP_IPA,
							   ipa_psoc_obj_destroy_notification,
							   NULL);

	if (QDF_IS_STATUS_ERROR(status)) {
		ipa_err("Failed to register pdev destroy handler for ipa");
		goto fail_delete_pdev;
	}

	qdf_mutex_create(&g_init_deinit_lock);

	return status;

fail_delete_pdev:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_IPA,
						   ipa_psoc_obj_create_notification,
						   NULL);

	return status;
}

QDF_STATUS ipa_deinit(void)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	ipa_info("ipa module dispatcher deinit");

	if (!ipa_is_hw_support()) {
		ipa_info("ipa hw is not present");
		return status;
	}

	qdf_mutex_destroy(&g_init_deinit_lock);

	status = wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_IPA,
							     ipa_psoc_obj_destroy_notification,
							     NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ipa_err("Failed to unregister pdev destroy handler");

	status = wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_IPA,
							    ipa_psoc_obj_create_notification,
							    NULL);
	if (QDF_IS_STATUS_ERROR(status))
		ipa_err("Failed to unregister pdev create handler");

	return status;
}

#ifdef IPA_OPT_WIFI_DP_CTRL
void ipa_tx_pkt_opt_dp_ctrl(uint8_t vdev_id,
			    qdf_nbuf_t nbuf)
{
	wlan_ipa_tx_pkt_opt_dp_ctrl(vdev_id, nbuf);
}

QDF_STATUS ipa_opt_dpath_enable_clk_req(void *soc)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)soc;
	struct wlan_ipa_priv *ipa_obj;
	QDF_STATUS status, clk_status;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL for psoc_id[%d]",
			psoc->soc_objmgr.psoc_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (ipa_obj->ipa_opt_dp_ctrl_debug) {
		ipa_debug("opt_dp_ctrl, bypassing clock for unit testing");
		return QDF_STATUS_SUCCESS;
	}

	clk_status = wlan_ipa_wdi_opt_dpath_enable_clk_req(ipa_obj);
	if (clk_status == QDF_STATUS_SUCCESS) {
		ipa_obj->ctrl_stats.clk_resp_cnt++;
		return clk_status;
	}

	status = qdf_wait_for_event_completion(&ipa_obj->ipa_opt_dp_ctrl_clk_evt,
					       IPA_CLK_ENABLE_WAIT_TIME_MS);
	return status;
}

QDF_STATUS ipa_opt_dpath_disable_clk_req(void *soc)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)soc;
	struct wlan_ipa_priv *ipa_obj;
	QDF_STATUS status;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL for psoc_id[%d]",
			psoc->soc_objmgr.psoc_id);
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_ipa_wdi_opt_dpath_disable_clk_req(ipa_obj);
	return status;
}

QDF_STATUS wlan_ipa_set_fw_cap_opt_dp_ctrl(struct wlan_objmgr_psoc *psoc,
					   bool fw_cap)
{
	struct wlan_ipa_priv *ipa_obj;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL for psoc_id[%d]",
			psoc->soc_objmgr.psoc_id);
		return QDF_STATUS_E_FAILURE;
	}

	ipa_debug("fw capability for opt_dp_ctrl, received from fw %d", fw_cap);
	ipa_obj->fw_cap_opt_dp_ctrl = fw_cap;
	return QDF_STATUS_SUCCESS;
}
#endif

qdf_ipa_wdi_hdl_t wlan_ipa_get_hdl(void *soc, uint8_t pdev_id)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)soc;
	struct wlan_ipa_priv *ipa_obj;
	qdf_ipa_wdi_hdl_t hdl;

	ipa_obj = ipa_psoc_get_priv_obj(psoc);
	if (!ipa_obj) {
		ipa_err("IPA object is NULL for psoc_id[%d]",
			psoc->soc_objmgr.psoc_id);
		return IPA_INVALID_HDL;
	}
	hdl = ipa_obj->hdl;

	return hdl;
}

qdf_export_symbol(wlan_ipa_get_hdl);

bool wlan_ipa_is_vlan_enabled(void)
{
	return ipa_config_is_vlan_enabled();
}

qdf_export_symbol(wlan_ipa_is_vlan_enabled);

bool wlan_ipa_config_is_opt_wifi_dp_enabled(void)
{
	return ipa_config_is_opt_wifi_dp_enabled();
}

qdf_export_symbol(wlan_ipa_config_is_opt_wifi_dp_enabled);

#ifdef IPA_OPT_WIFI_DP
uint32_t get_ipa_config(struct wlan_objmgr_psoc *psoc)
{
	uint32_t val = cfg_get(psoc, CFG_DP_IPA_OFFLOAD_CONFIG);

	if (val == INTRL_MODE_DISABLE) {
		val = 0;
	} else {
		if (val == IPA_OFFLOAD_CFG)
			ipa_err("Invalid IPA Config 0x%x", val);

		if (val != INTRL_MODE_RTP_STREAM_FILTER)
			val = INTRL_MODE_ENABLE;
	}
	return val;
}
#else
uint32_t get_ipa_config(struct wlan_objmgr_psoc *psoc)
{
	uint32_t val = cfg_get(psoc, CFG_DP_IPA_OFFLOAD_CONFIG);

	if (val & WLAN_IPA_OPT_WIFI_DP) {
		val &= ~WLAN_IPA_OPT_WIFI_DP;
		ipa_info("Resetting IPAConfig val to 0x%x", val);
	}
	return val;
}
#endif

qdf_export_symbol(get_ipa_config);

bool wlan_ipa_is_shared_smmu_enabled(void)
{
	return ipa_get_shared_smmu_enable();
}

qdf_export_symbol(wlan_ipa_is_shared_smmu_enabled);
