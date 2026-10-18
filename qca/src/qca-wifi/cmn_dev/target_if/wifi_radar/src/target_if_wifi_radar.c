/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <target_if_wifi_radar.h>
#include <wlan_lmac_if_def.h>
#include <target_type.h>
#include <hif_hw_version.h>
#include <target_if.h>
#include <wlan_osif_priv.h>
#include <init_deinit_lmac.h>
#include <wlan_wifi_radar_utils_api.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wmi_unified_wifi_radar_api.h>
#include <wlan_lmac_if_def.h>
#include <target_if_direct_buf_rx_api.h>

static u_int32_t end_magic = 0xBEAFDEAD;

#ifdef DIRECT_BUF_RX_ENABLE
static bool wifi_radar_dbr_event_handler(struct wlan_objmgr_pdev *pdev,
					 struct direct_buf_rx_data *payload)
{
	uint8_t *data = NULL;
	int data_len = 0, status;
	struct wlan_lmac_if_rx_ops *rx_ops;
	struct wlan_lmac_if_wifi_radar_rx_ops *wr_rx_ops = NULL;
	struct pdev_wifi_radar *pwr;
	struct wlan_objmgr_psoc *psoc;
	uint32_t cookie = 0, i, j;
	struct wifi_radar_ucode_header uheader = {0};

	if ((!pdev) || (!payload)) {
		wifi_radar_err("pdev or payload is null");
		return true;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	if (!psoc) {
		wifi_radar_err("psoc is null");
		return true;
	}

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		wifi_radar_err("rx_ops is null");
		return true;
	}

	wr_rx_ops = &rx_ops->wifi_radar_rx_ops;

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("pdev object for Wifi radar is NULL");
		return true;
	}

	data = payload->vaddr;
	cookie = payload->cookie;

	wifi_radar_debug("<DBRCOMP><%u>:bufferaddr: 0x%pK cookie: %u\n", cookie,
			 (void *)((uintptr_t)payload->paddr), cookie);

	qdf_mem_copy(&uheader, &data[0],
		     sizeof(struct wifi_radar_ucode_header));

	data_len = uheader.ucode_header_len_32bit_dwords * 4;
	data_len += uheader.capture_len; /* length of capture */
	if (cookie >= pwr->header_entries) {
		wifi_radar_err("Invalid cookie offset");
		return true;
	}
	qdf_spin_lock_bh(&pwr->header_lock);
	pwr->header[cookie]->start_magic_num = 0xDEADBEAF;
	pwr->header[cookie]->vendorid = 0x8cfdf0;
	pwr->header[cookie]->pltform_type = WR_PLATFORM_TYPE_ARM;
	pwr->header[cookie]->metadata_version = 1;
	pwr->header[cookie]->data_version = WR_DATA_VERSION_1;
	pwr->header[cookie]->chip_type = pwr->chip_type;
	pwr->header[cookie]->metadata_len = sizeof(struct wifi_radar_header);
	pwr->header[cookie]->host_real_ts =
		qdf_ktime_to_ns(qdf_ktime_real_get());
	pwr->header[cookie]->fw_timestamp_us =
		payload->wifi_radar_meta_data.timestamp_us;
	pwr->header[cookie]->phy_mode = payload->wifi_radar_meta_data.phy_mode;
	pwr->header[cookie]->prim20_chan =
		payload->wifi_radar_meta_data.chan_mhz;
	pwr->header[cookie]->center_freq1 =
		payload->wifi_radar_meta_data.band_center_freq1;
	pwr->header[cookie]->center_freq2 =
		payload->wifi_radar_meta_data.band_center_freq2;
	pwr->header[cookie]->tx_chain_mask =
		payload->wifi_radar_meta_data.tx_chain_mask;
	pwr->header[cookie]->rx_chain_mask =
		payload->wifi_radar_meta_data.rx_chain_mask;
	pwr->header[cookie]->num_ltf_tx =
		payload->wifi_radar_meta_data.num_ltf_tx;
	pwr->header[cookie]->num_skip_ltf_rx =
		payload->wifi_radar_meta_data.num_skip_ltf_rx;
	pwr->header[cookie]->num_ltf_accumulation =
		payload->wifi_radar_meta_data.num_ltf_accumulation;
	qdf_spin_lock_bh(&pwr->cal_status_lock);
	pwr->header[cookie]->cal_num_ltf_tx = pwr->cal_num_ltf_tx;
	pwr->header[cookie]->cal_num_skip_ltf_rx = pwr->cal_num_skip_ltf_rx;
	pwr->header[cookie]->cal_num_ltf_accumulation =
		pwr->cal_num_ltf_accumulation;
	pwr->header[cookie]->cal_band_center_freq =
		pwr->band_center_freq;
	for (i = 0; i < HOST_MAX_CHAINS; i++) {
		for (j = 0; j < HOST_MAX_CHAINS; j++) {
			pwr->header[cookie]->per_chain_cal_status[i][j] =
				!!pwr->per_chain_comb_cal_status[i][j];
		}
	}
	qdf_spin_unlock_bh(&pwr->cal_status_lock);
	if (wr_rx_ops->wifi_radar_info_send)
		status = wr_rx_ops->wifi_radar_info_send(pdev,
							 pwr->header[cookie],
							 sizeof(struct
							 wifi_radar_header),
							 data,
							 data_len,
							 &end_magic, 4);
	qdf_spin_unlock_bh(&pwr->header_lock);
	return true;
}

static QDF_STATUS
target_if_register_wifi_radar_to_dbr(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_direct_buf_rx_tx_ops *dbr_tx_ops = NULL;
	struct dbr_module_config dbr_config;
	struct wlan_lmac_if_tx_ops *tx_ops;

	psoc = wlan_pdev_get_psoc(pdev);
	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		wifi_radar_err("tx_ops is NULL");
		return QDF_STATUS_SUCCESS;
	}
	dbr_tx_ops = &tx_ops->dbr_tx_ops;
	dbr_config.num_resp_per_event = DBR_NUM_RESP_PER_EVENT_WIFI_RADAR;
	dbr_config.event_timeout_in_ms = DBR_EVENT_TIMEOUT_IN_MS_WIFI_RADAR;
	if (dbr_tx_ops->direct_buf_rx_module_register) {
		return dbr_tx_ops->direct_buf_rx_module_register
			(pdev, DBR_MODULE_WIFI_RADAR, &dbr_config,
			 wifi_radar_dbr_event_handler);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
target_if_unregister_wifi_radar_to_dbr(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_direct_buf_rx_tx_ops *dbr_tx_ops = NULL;
	struct wlan_lmac_if_tx_ops *tx_ops;

	psoc = wlan_pdev_get_psoc(pdev);
	tx_ops = wlan_psoc_get_lmac_if_txops(psoc);
	if (!tx_ops) {
		wifi_radar_err("tx_ops is NULL");
		return QDF_STATUS_SUCCESS;
	}
	dbr_tx_ops = &tx_ops->dbr_tx_ops;
	if (dbr_tx_ops->direct_buf_rx_module_unregister) {
		return dbr_tx_ops->direct_buf_rx_module_unregister
			(pdev, DBR_MODULE_WIFI_RADAR);
	}

	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS
target_if_register_wifi_radar_to_dbr(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}

static QDF_STATUS
target_if_unregister_wifi_radar_to_dbr(struct wlan_objmgr_pdev *pdev)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

static QDF_STATUS
target_if_unregister_wifi_radar_cal_status_event_handler
		(struct wlan_objmgr_psoc *psoc)
{
	wmi_unified_t wmi_hdl;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	wmi_hdl = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_hdl) {
		wifi_radar_err("Unable to get wmi handle");
		return QDF_STATUS_E_NULL_VALUE;
	}
	status = wmi_unified_unregister_event
		(wmi_hdl, wmi_pdev_wifi_radar_cal_completion_status_event_id);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Failed to unregister wifi radar cal status event handler");

	return status;
}

static int
target_if_pdev_wifi_radar_cal_status_event_handler(ol_scn_t sc,
						   uint8_t *data,
						   uint32_t datalen)
{
	struct wmi_unified *wmi_handle;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	struct pdev_wifi_radar *pwr;
	QDF_STATUS retval = 0;
	struct wmi_wifi_radar_cal_status_param param = {0};
	int tx_idx, rx_idx;

	if (!sc || !data) {
		wifi_radar_err("sc or data is null");
		return -EINVAL;
	}

	psoc = target_if_get_psoc_from_scn_hdl(sc);
	if (!psoc) {
		wifi_radar_err("psoc is null");
		return -EINVAL;
	}

	retval = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_WIFI_RADAR_ID);
	if (QDF_IS_STATUS_ERROR(retval)) {
		wifi_radar_err("unable to get psoc reference");
		return -EINVAL;
	}

	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (!wmi_handle) {
		wifi_radar_err("wmi_handle is null");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	retval = wmi_extract_wifi_radar_cal_status_event_param
			(wmi_handle, data, &param);
	if (QDF_IS_STATUS_ERROR(retval)) {
		wifi_radar_err("Failed to extract wifi radar cal status tlv");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id
			(psoc, param.pdev_id, WLAN_WIFI_RADAR_ID);
	if (!pdev) {
		wifi_radar_err("pdev is null");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("pdev object for Wifi radar is NULL");
		wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
		wlan_objmgr_pdev_release_ref(pdev, WLAN_WIFI_RADAR_ID);
		return -EINVAL;
	}

	qdf_spin_lock_bh(&pwr->cal_status_lock);
	pwr->host_timestamp_of_cal_status_evt =
		qdf_ktime_to_ns(qdf_ktime_real_get());
	pwr->wifi_radar_pkt_bw = param.wifi_radar_pkt_bw;
	pwr->channel_bw = param.channel_bw;
	pwr->band_center_freq = param.band_center_freq;
	pwr->cal_num_ltf_tx = param.num_ltf_tx;
	pwr->cal_num_skip_ltf_rx = param.num_skip_ltf_rx;
	pwr->cal_num_ltf_accumulation = param.num_ltf_accumulation;
	for (tx_idx = 0; tx_idx < HOST_MAX_CHAINS; tx_idx++) {
		for (rx_idx = 0; rx_idx < HOST_MAX_CHAINS; rx_idx++) {
			pwr->per_chain_comb_cal_status[tx_idx][rx_idx] =
				(param.per_chain_cal_status[tx_idx] &
				 (1 << rx_idx));
		}
	}
	qdf_spin_unlock_bh(&pwr->cal_status_lock);

	wlan_objmgr_psoc_release_ref(psoc, WLAN_WIFI_RADAR_ID);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_WIFI_RADAR_ID);

	return 0;
}

static QDF_STATUS
target_if_register_wifi_radar_cal_status_event_handler(struct wlan_objmgr_psoc
						       *psoc)
{
	wmi_unified_t wmi_hdl;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;

	wmi_hdl = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_hdl) {
		wifi_radar_err("Unable to get wmi handle");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ret = wmi_unified_register_event_handler
		(wmi_hdl, wmi_pdev_wifi_radar_cal_completion_status_event_id,
		 target_if_pdev_wifi_radar_cal_status_event_handler,
		 WMI_RX_UMAC_CTX);

	/*
	 * Event registration is called per pdev
	 * Ignore error if event is already registered.
	 */
	if (QDF_IS_STATUS_ERROR(ret)) {
		wifi_radar_err("Failed to register wifi radar cal status event handler");
		ret = QDF_STATUS_SUCCESS;
	}

	return ret;
}

QDF_STATUS wifi_radar_deinit_pdev(struct wlan_objmgr_psoc *psoc,
				  struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pdev_wifi_radar *pwr;

	pwr = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						    WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr)
		return QDF_STATUS_E_NULL_VALUE;

	/* Add wifi radar specific unregister part here */
	status = target_if_unregister_wifi_radar_cal_status_event_handler(psoc);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Failed to unregister cal status handler");

	if (pwr->cal_status_lock_initialized) {
		qdf_spinlock_destroy(&pwr->cal_status_lock);
		pwr->cal_status_lock_initialized = false;
	}

	if (pwr->header_lock_initialized) {
		qdf_spinlock_destroy(&pwr->header_lock);
		pwr->header_lock_initialized = false;
	}

	status = target_if_unregister_wifi_radar_to_dbr(pdev);
	if (QDF_IS_STATUS_ERROR(status))
		wifi_radar_err("Failed to unregister with dbr");

	return status;
}

QDF_STATUS wifi_radar_init_pdev(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_pdev *pdev)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct pdev_wifi_radar *pwr;
	struct psoc_wifi_radar *wr_sc;
	struct target_psoc_info *tgt_psoc_info;
	struct wlan_psoc_host_wifi_radar_ltf_caps_ext2 *wr_ltf_caps;
	struct wlan_psoc_host_wifi_radar_chain_caps_ext2 *wr_chain_caps;
	uint8_t pdev_id, i, num_wr_ltf_caps, num_wr_chain_caps;

	if (!pdev) {
		wifi_radar_err("PDEV is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (!psoc) {
		wifi_radar_err("PSOC is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pwr = wlan_objmgr_pdev_get_comp_private_obj
				(pdev, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pwr) {
		wifi_radar_err("pwr is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wr_sc = wlan_objmgr_psoc_get_comp_private_obj
				(psoc, WLAN_UMAC_COMP_WIFI_RADAR);

	if (!wr_sc) {
		wifi_radar_err("wr_sc is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = target_if_register_wifi_radar_cal_status_event_handler(psoc);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_radar_err("Failed to register with wifi radar cal status handler");
		return status;
	}

	status = target_if_register_wifi_radar_to_dbr(pdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		wifi_radar_err("Failed to register with dbr");
		return status;
	}

	tgt_psoc_info = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_psoc_info) {
		wifi_radar_err("target_psoc_info is null");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pwr->is_wifi_radar_capable = 1;
	pwr->subbuf_size = STREAMFS_WIFI_RADAR_MAX_SUBBUF;
	pwr->num_subbufs = STREAMFS_WIFI_RADAR_NUM_SUBBUF;
	pwr->max_num_ltf_tx = MAX_NUM_LTF_TX;
	pwr->max_num_skip_ltf_rx = MAX_NUM_SKIP_LTF_RX;
	pwr->max_num_ltf_accumulation = MAX_NUM_LTF_ACCUMULATION;
	pwr->max_num_rx_chain = HOST_MAX_CHAINS;
	pwr->best_isolated_chain_pair_sel = DEFAULT_ISOLATED_CHAIN_PAIR_BIT;
	qdf_spinlock_create(&pwr->cal_status_lock);
	pwr->cal_status_lock_initialized = true;
	qdf_spinlock_create(&pwr->header_lock);
	pwr->header_lock_initialized = true;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	if (pdev_id < 0) {
		wifi_radar_err("pdev_id is invalid");
		return QDF_STATUS_E_INVAL;
	}

	wr_ltf_caps = tgt_psoc_info->info.wr_ltf_caps;
	wr_chain_caps = tgt_psoc_info->info.wr_chain_caps;
	if (!wr_ltf_caps || !wr_chain_caps) {
		wifi_radar_info("No wifi radar capabilities advertised");
		return QDF_STATUS_SUCCESS;
	}

	num_wr_ltf_caps = target_psoc_get_num_wr_ltf_caps(tgt_psoc_info);
	for (i = 0; i < num_wr_ltf_caps; i++) {
		if (wr_ltf_caps[i].pdev_id == pdev_id) {
			pwr->max_num_ltf_tx = wr_ltf_caps[i].ltf_max_num_tx;
			pwr->max_num_skip_ltf_rx =
					wr_ltf_caps[i].ltf_max_num_initial_skip_rx;
			pwr->max_num_ltf_accumulation =
					wr_ltf_caps[i].ltf_max_expo_num_rx;
		}
	}

	num_wr_chain_caps = target_psoc_get_num_wr_chain_caps(tgt_psoc_info);
	for (i = 0; i < num_wr_chain_caps; i++) {
		if (wr_chain_caps[i].pdev_id == pdev_id) {
			pwr->max_num_rx_chain = wr_chain_caps[i].max_num_rx_chain;
			pwr->best_isolated_chain_pair_sel =
					wr_chain_caps[i].best_isolated_chain_pair_sel;
		}
	}

	return status;
}

static QDF_STATUS
target_if_wifi_radar_init_pdev(struct wlan_objmgr_psoc *psoc,
			       struct wlan_objmgr_pdev *pdev)
{
	struct pdev_wifi_radar *pa;
	struct psoc_wifi_radar *wifi_radar_sc;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("wifi radar is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj
			(pdev, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!pa)
		return QDF_STATUS_E_FAILURE;

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj
			(psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!wifi_radar_sc)
		return QDF_STATUS_E_FAILURE;

	if (wifi_radar_sc->is_wifi_radar_capable) {
		pa->is_wifi_radar_capable = 1;
		return wifi_radar_init_pdev(psoc, pdev);
	} else {
		return QDF_STATUS_E_NOSUPPORT;
	}
}

static QDF_STATUS
target_if_wifi_radar_deinit_pdev(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_pdev *pdev)
{
	struct psoc_wifi_radar *wifi_radar_sc;

	if (wlan_wifi_radar_is_feature_disabled(pdev)) {
		wifi_radar_err("wifi radar is disabled");
		return QDF_STATUS_E_NOSUPPORT;
	}

	wifi_radar_sc = wlan_objmgr_psoc_get_comp_private_obj
			(psoc, WLAN_UMAC_COMP_WIFI_RADAR);
	if (!wifi_radar_sc)
		return QDF_STATUS_E_FAILURE;

	if (wifi_radar_sc->is_wifi_radar_capable)
		return wifi_radar_deinit_pdev(psoc, pdev);
	else
		return QDF_STATUS_E_NOSUPPORT;
}

static int
target_if_wifi_radar_capture_and_cal_command
		(struct wlan_objmgr_pdev *pdev,
		 struct wifi_radar_command_params *params)
{
	struct wmi_wifi_radar_command_params wmi_param = {0};
	struct wmi_unified *pdev_wmi_handle = NULL;
	int retv = 0;

	pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!pdev_wmi_handle) {
		wifi_radar_err("pdev wmi handle NULL");
		return -EINVAL;
	}

	wmi_param.cmd_type = params->cmd_type;
	wmi_param.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	if ((wmi_param.cmd_type == wmi_host_wifi_radar_capture_enable) ||
	    (wmi_param.cmd_type == wmi_host_wifi_radar_rx_cal)) {
		wmi_param.tx_chainmask = params->tx_chainmask;
		wmi_param.rx_chainmask = params->rx_chainmask;
		wmi_param.num_ltf_tx = params->num_ltf_tx;
		wmi_param.num_skip_ltf_rx = params->num_skip_ltf_rx;
		wmi_param.num_ltf_accumulation = params->num_ltf_accumulation;
		wmi_param.bandwidth = params->bandwidth;
	}

	if (wmi_param.cmd_type == wmi_host_wifi_radar_capture_enable) {
		wmi_param.periodicity = params->periodicity;
	}

	retv = wmi_unified_send_wifi_radar_command(pdev_wmi_handle, &wmi_param);

	if (QDF_IS_STATUS_ERROR(retv))
		wifi_radar_err("fail to send wifi radar command");

	return retv;
}

void target_if_wifi_radar_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops)
{
	tx_ops->wifi_radar_tx_ops.wifi_radar_init_pdev =
		target_if_wifi_radar_init_pdev;
	tx_ops->wifi_radar_tx_ops.wifi_radar_deinit_pdev =
		target_if_wifi_radar_deinit_pdev;
	tx_ops->wifi_radar_tx_ops.wifi_radar_capture_and_cal =
		target_if_wifi_radar_capture_and_cal_command;
}

void target_if_wifi_radar_set_support(struct wlan_objmgr_psoc *psoc,
				      uint8_t value)
{
	struct wlan_lmac_if_rx_ops *rx_ops;

	rx_ops = wlan_psoc_get_lmac_if_rxops(psoc);
	if (!rx_ops) {
		wifi_radar_err("rx_ops is NULL");
		return;
	}
	if (rx_ops->wifi_radar_rx_ops.wifi_radar_support_set)
		rx_ops->wifi_radar_rx_ops.wifi_radar_support_set(psoc, value);
}
