/*
 * Copyright (c) 2013-2018, 2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <osdep.h>
#include <wmi.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_p2p_api.h>

/**
 * send_set_p2pgo_noa_req_cmd_tlv() - send p2p go noa request to fw
 * @wmi_handle: wmi handle
 * @noa: p2p power save parameters
 *
 * Return: QDF status
 */
static QDF_STATUS send_set_p2pgo_noa_req_cmd_tlv(wmi_unified_t wmi_handle,
						 struct p2p_ps_params *noa)
{
	wmi_p2p_set_noa_cmd_fixed_param *cmd;
	wmi_p2p_noa_descriptor *noa_discriptor;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint16_t len;
	QDF_STATUS status;
	uint32_t duration;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE + sizeof(*noa_discriptor);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = (wmi_p2p_set_noa_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_p2p_set_noa_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN
			       (wmi_p2p_set_noa_cmd_fixed_param));
	duration = (noa->count == 1) ? noa->single_noa_duration : noa->duration;
	cmd->vdev_id = noa->session_id;
	cmd->enable = (duration) ? true : false;
	cmd->num_noa = 1;

	WMITLV_SET_HDR((buf_ptr + sizeof(wmi_p2p_set_noa_cmd_fixed_param)),
		       WMITLV_TAG_ARRAY_STRUC, sizeof(wmi_p2p_noa_descriptor));
	noa_discriptor = (wmi_p2p_noa_descriptor *)(buf_ptr +
						    sizeof
						    (wmi_p2p_set_noa_cmd_fixed_param)
						     + WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(&noa_discriptor->tlv_header,
		       WMITLV_TAG_STRUC_wmi_p2p_noa_descriptor,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_noa_descriptor));
	noa_discriptor->type_count = noa->count;
	noa_discriptor->duration = duration;
	noa_discriptor->interval = noa->interval;
	noa_discriptor->start_time = noa->start;

	wmi_debug("SET P2P GO NOA:vdev_id:%d count:%d duration:%d interval:%d start:%d",
		  cmd->vdev_id, noa->count, noa_discriptor->duration,
		  noa->interval, noa->start);
	wmi_mtrace(WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, len,
				      WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("Failed to send WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID");
		wmi_buf_free(buf);
	}

end:
	return status;
}

/**
 * send_set_p2pgo_oppps_req_cmd_tlv() - send p2p go opp power save request to fw
 * @wmi_handle: wmi handle
 * @oppps: p2p opp power save parameters
 *
 * Return: QDF status
 */
static QDF_STATUS send_set_p2pgo_oppps_req_cmd_tlv(wmi_unified_t wmi_handle,
						   struct p2p_ps_params *oppps)
{
	wmi_p2p_set_oppps_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS status;

	buf = wmi_buf_alloc(wmi_handle, sizeof(*cmd));
	if (!buf) {
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}

	cmd = (wmi_p2p_set_oppps_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_p2p_set_oppps_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
					wmi_p2p_set_oppps_cmd_fixed_param));
	cmd->vdev_id = oppps->session_id;
	if (oppps->ctwindow)
		WMI_UNIFIED_OPPPS_ATTR_ENABLED_SET(cmd);

	WMI_UNIFIED_OPPPS_ATTR_CTWIN_SET(cmd, oppps->ctwindow);
	wmi_debug("SET P2P GO OPPPS:vdev_id:%d ctwindow:%d",
		 cmd->vdev_id, oppps->ctwindow);
	wmi_mtrace(WMI_P2P_SET_OPPPS_PARAM_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle, buf, sizeof(*cmd),
				      WMI_P2P_SET_OPPPS_PARAM_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("Failed to send WMI_P2P_SET_OPPPS_PARAM_CMDID");
		wmi_buf_free(buf);
	}

end:
	return status;
}

/**
 * extract_p2p_noa_ev_param_tlv() - extract p2p noa information from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @param: Pointer to hold p2p noa info
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_p2p_noa_ev_param_tlv(
	wmi_unified_t wmi_handle, void *evt_buf,
	struct p2p_noa_info *param)
{
	WMI_P2P_NOA_EVENTID_param_tlvs *param_tlvs;
	wmi_p2p_noa_event_fixed_param *fixed_param;
	uint8_t i;
	wmi_p2p_noa_info *wmi_noa_info;
	uint8_t *buf_ptr;
	uint32_t descriptors;

	param_tlvs = (WMI_P2P_NOA_EVENTID_param_tlvs *)evt_buf;
	if (!param_tlvs) {
		wmi_err("Invalid P2P NoA event buffer");
		return QDF_STATUS_E_INVAL;
	}

	if (!param) {
		wmi_err("noa information param is null");
		return QDF_STATUS_E_INVAL;
	}

	fixed_param = param_tlvs->fixed_param;
	buf_ptr = (uint8_t *) fixed_param;
	buf_ptr += sizeof(wmi_p2p_noa_event_fixed_param);
	wmi_noa_info = (wmi_p2p_noa_info *) (buf_ptr);

	if (!WMI_UNIFIED_NOA_ATTR_IS_MODIFIED(wmi_noa_info)) {
		wmi_err("noa attr is not modified");
		return QDF_STATUS_E_INVAL;
	}

	param->vdev_id = fixed_param->vdev_id;
	param->index =
		(uint8_t)WMI_UNIFIED_NOA_ATTR_INDEX_GET(wmi_noa_info);
	param->opps_ps =
		(uint8_t)WMI_UNIFIED_NOA_ATTR_OPP_PS_GET(wmi_noa_info);
	param->ct_window =
		(uint8_t)WMI_UNIFIED_NOA_ATTR_CTWIN_GET(wmi_noa_info);
	descriptors = WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET(wmi_noa_info);
	param->num_desc = (uint8_t)descriptors;
	if (param->num_desc > WMI_P2P_MAX_NOA_DESCRIPTORS) {
		wmi_err("Invalid num desc: %d", param->num_desc);
		return QDF_STATUS_E_INVAL;
	}

	wmi_debug("index %u, opps_ps %u, ct_window %u, num_descriptors = %u",
		 param->index, param->opps_ps, param->ct_window,
		 param->num_desc);
	for (i = 0; i < param->num_desc; i++) {
		param->noa_desc[i].type_count =
			(uint8_t)wmi_noa_info->noa_descriptors[i].
			type_count;
		param->noa_desc[i].duration =
			wmi_noa_info->noa_descriptors[i].duration;
		param->noa_desc[i].interval =
			wmi_noa_info->noa_descriptors[i].interval;
		param->noa_desc[i].start_time =
			wmi_noa_info->noa_descriptors[i].start_time;
		wmi_debug("NoA descriptor[%d] type_count %u, duration %u, interval %u, start_time = %u",
			 i, param->noa_desc[i].type_count,
			param->noa_desc[i].duration,
			param->noa_desc[i].interval,
			param->noa_desc[i].start_time);
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS extract_mac_addr_rx_filter_evt_param_tlv(
	wmi_unified_t wmi_handle, void *evt_buf,
	struct p2p_set_mac_filter_evt *param)
{
	WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_STATUS_EVENTID_param_tlvs *param_buf;
	wmi_vdev_add_mac_addr_to_rx_filter_status_event_fixed_param *event;

	param_buf =
		(WMI_VDEV_ADD_MAC_ADDR_TO_RX_FILTER_STATUS_EVENTID_param_tlvs *)
		evt_buf;
	if (!param_buf) {
		wmi_err("Invalid action frame filter mac event");
		return QDF_STATUS_E_INVAL;
	}
	event = param_buf->fixed_param;
	if (!event) {
		wmi_err("Invalid fixed param");
		return QDF_STATUS_E_INVAL;
	}
	param->vdev_id = event->vdev_id;
	param->status = event->status;

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
send_p2p_ap_assist_dfs_group_params_cmd_tlv(wmi_unified_t wmi_handle,
					    struct p2p_ap_assist_dfs_group_params *params)
{
	wmi_p2p_go_dfs_ap_config_fixed_param *cmd;
	uint32_t len;
	bool is_valid_bssid = false, is_valid_non_tx_bssid = false;
	wmi_buf_t wmi_buf;
	QDF_STATUS status;
	uint8_t *buf;
	wmi_mac_addr *mac_buf;

	if (!wmi_handle) {
		wmi_err("WMA context is invalid!");
		return QDF_STATUS_E_INVAL;
	}

	len = sizeof(*cmd) + (WMI_TLV_HDR_SIZE * 2);

	if (!qdf_is_macaddr_zero(&params->bssid)) {
		is_valid_bssid = true;
		len += sizeof(wmi_mac_addr);
	}

	if (!qdf_is_macaddr_zero(&params->non_tx_bssid)) {
		is_valid_non_tx_bssid = true;
		len += sizeof(wmi_mac_addr);
	}

	wmi_buf = wmi_buf_alloc(wmi_handle, len);
	if (!wmi_buf) {
		wmi_err("Failed allocate wmi buffer");
		return QDF_STATUS_E_NOMEM;
	}

	buf = wmi_buf_data(wmi_buf);
	cmd = (wmi_p2p_go_dfs_ap_config_fixed_param *)buf;

	buf += sizeof(*cmd);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_p2p_go_dfs_ap_config_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_go_dfs_ap_config_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->set = is_valid_bssid;

	WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       (is_valid_bssid * sizeof(wmi_mac_addr)));

	if (is_valid_bssid) {
		mac_buf = (wmi_mac_addr *)(buf + WMI_TLV_HDR_SIZE);
		WMI_CHAR_ARRAY_TO_MAC_ADDR(&params->bssid.bytes[0], mac_buf);
	}

	buf += WMI_TLV_HDR_SIZE + (is_valid_bssid * sizeof(wmi_mac_addr));

	WMITLV_SET_HDR(buf, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       (is_valid_non_tx_bssid * sizeof(wmi_mac_addr)));

	if (is_valid_non_tx_bssid) {
		mac_buf = (wmi_mac_addr *)(buf + WMI_TLV_HDR_SIZE);
		WMI_CHAR_ARRAY_TO_MAC_ADDR(&params->non_tx_bssid.bytes[0],
					   mac_buf);
	}

	status = wmi_unified_cmd_send(wmi_handle, wmi_buf, len,
				      WMI_P2P_GO_DFS_AP_CONFIG_CMDID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wmi_err("Failed to send P2P DFS assisted AP params");
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
extract_p2p_ap_assist_dfs_group_bmiss_param_tlv(wmi_unified_t wmi_handle,
						void *ev_buf, uint8_t *data)
{
	WMI_P2P_CLI_DFS_AP_BMISS_DETECTED_EVENTID_param_tlvs *buf;
	wmi_p2p_cli_dfs_ap_bmiss_fixed_param *event;

	buf = (WMI_P2P_CLI_DFS_AP_BMISS_DETECTED_EVENTID_param_tlvs *)ev_buf;
	if (!buf) {
		wmi_err("Invalid event data");
		return QDF_STATUS_E_INVAL;
	}
	event = buf->fixed_param;
	if (!event) {
		wmi_err("Invalid fixed param");
		return QDF_STATUS_E_INVAL;
	}

	*data = event->vdev_id;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * send_p2p_lo_start_cmd_tlv() - send p2p lo start request to fw
 * @wmi_handle: wmi handle
 * @param: p2p listen offload start parameters
 *
 * Return: QDF status
 */
static QDF_STATUS send_p2p_lo_start_cmd_tlv(wmi_unified_t wmi_handle,
					    struct p2p_lo_start *param)
{
	wmi_buf_t buf;
	wmi_p2p_lo_start_cmd_fixed_param *cmd;
	int32_t len = sizeof(*cmd);
	uint8_t *buf_ptr;
	QDF_STATUS status;
	int device_types_len_aligned;
	int probe_resp_len_aligned;

	if (!param) {
		wmi_err("lo start param is null");
		return QDF_STATUS_E_INVAL;
	}

	wmi_debug("vdev_id: %d", param->vdev_id);

	device_types_len_aligned =
		qdf_roundup(param->dev_types_len,
			    sizeof(uint32_t));
	probe_resp_len_aligned =
		qdf_roundup(param->probe_resp_len,
			    sizeof(uint32_t));

	len += 2 * WMI_TLV_HDR_SIZE + device_types_len_aligned +
			probe_resp_len_aligned;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		return QDF_STATUS_E_NOMEM;
	}

	cmd = (wmi_p2p_lo_start_cmd_fixed_param *)wmi_buf_data(buf);
	buf_ptr = (uint8_t *) wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		 WMITLV_TAG_STRUC_wmi_p2p_lo_start_cmd_fixed_param,
		 WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_lo_start_cmd_fixed_param));

	cmd->vdev_id = param->vdev_id;
	cmd->ctl_flags = param->ctl_flags;
	cmd->channel = param->freq;
	cmd->period = param->period;
	cmd->interval = param->interval;
	cmd->count = param->count;
	cmd->device_types_len = param->dev_types_len;
	cmd->prob_resp_len = param->probe_resp_len;

	buf_ptr += sizeof(wmi_p2p_lo_start_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       device_types_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	qdf_mem_copy(buf_ptr, param->device_types,
		     param->dev_types_len);

	buf_ptr += device_types_len_aligned;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       probe_resp_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	qdf_mem_copy(buf_ptr, param->probe_resp_tmplt,
		     param->probe_resp_len);

	wmi_debug("Sending WMI_P2P_LO_START command, channel=%d, period=%d, interval=%d, count=%d",
		 cmd->channel, cmd->period, cmd->interval, cmd->count);

	wmi_mtrace(WMI_P2P_LISTEN_OFFLOAD_START_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle,
				      buf, len,
				      WMI_P2P_LISTEN_OFFLOAD_START_CMDID);
	if (status != QDF_STATUS_SUCCESS) {
		wmi_err("Failed to send p2p lo start: %d", status);
		wmi_buf_free(buf);
		return status;
	}

	wmi_debug("Successfully sent WMI_P2P_LO_START");

	return QDF_STATUS_SUCCESS;
}

/**
 * send_p2p_lo_stop_cmd_tlv() - send p2p lo stop request to fw
 * @wmi_handle: wmi handle
 * @vdev_id: vdev identifier
 *
 * Return: QDF status
 */
static QDF_STATUS send_p2p_lo_stop_cmd_tlv(wmi_unified_t wmi_handle,
					   uint8_t vdev_id)
{
	wmi_buf_t buf;
	wmi_p2p_lo_stop_cmd_fixed_param *cmd;
	int32_t len;
	QDF_STATUS status;

	wmi_debug("vdev_id: %d", vdev_id);

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		return QDF_STATUS_E_NOMEM;
	}
	cmd = (wmi_p2p_lo_stop_cmd_fixed_param *)wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_p2p_lo_stop_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_lo_stop_cmd_fixed_param));

	cmd->vdev_id = vdev_id;

	wmi_debug("Sending WMI_P2P_LO_STOP command");

	wmi_mtrace(WMI_P2P_LISTEN_OFFLOAD_STOP_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle,
				      buf, len,
				      WMI_P2P_LISTEN_OFFLOAD_STOP_CMDID);
	if (status != QDF_STATUS_SUCCESS) {
		wmi_err("Failed to send p2p lo stop: %d", status);
		wmi_buf_free(buf);
		return status;
	}

	wmi_debug("Successfully sent WMI_P2P_LO_STOP");

	return QDF_STATUS_SUCCESS;
}

/**
 * extract_p2p_lo_stop_ev_param_tlv() - extract p2p lo stop
 * information from event
 * @wmi_handle: wmi handle
 * @evt_buf: pointer to event buffer
 * @param: Pointer to hold p2p lo stop event information
 *
 * Return: QDF_STATUS_SUCCESS for success or error code
 */
static QDF_STATUS extract_p2p_lo_stop_ev_param_tlv(
	wmi_unified_t wmi_handle, void *evt_buf,
	struct p2p_lo_event *param)
{
	WMI_P2P_LISTEN_OFFLOAD_STOPPED_EVENTID_param_tlvs *param_tlvs;
	wmi_p2p_lo_stopped_event_fixed_param *lo_param;

	param_tlvs = (WMI_P2P_LISTEN_OFFLOAD_STOPPED_EVENTID_param_tlvs *)
					evt_buf;
	if (!param_tlvs) {
		wmi_err("Invalid P2P lo stop event buffer");
		return QDF_STATUS_E_INVAL;
	}

	if (!param) {
		wmi_err("lo stop event param is null");
		return QDF_STATUS_E_INVAL;
	}

	lo_param = param_tlvs->fixed_param;
	param->vdev_id = lo_param->vdev_id;
	param->reason_code = lo_param->reason;
	wmi_debug("vdev_id:%d, reason:%d",
		 param->vdev_id, param->reason_code);

	return QDF_STATUS_SUCCESS;
}

void wmi_p2p_listen_offload_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_p2p_lo_start_cmd = send_p2p_lo_start_cmd_tlv;
	ops->send_p2p_lo_stop_cmd = send_p2p_lo_stop_cmd_tlv;
	ops->extract_p2p_lo_stop_ev_param =
			extract_p2p_lo_stop_ev_param_tlv;
}
#endif /* FEATURE_P2P_LISTEN_OFFLOAD */

#ifdef FEATURE_WLAN_SUPPORT_USD
/**
 * wmi_p2p_op_type_convert_p2p_enum_to_wmi_enum() - this API converts
 * P2P_USD_OP_TYPE_XX to WMI_USD_MODE_XX
 *
 * @p2p_op_type: P2P USD OP type
 * @wmi_mode: pointer to WMI USD mode
 *
 * Return: QDF status
 */
static QDF_STATUS
wmi_p2p_op_type_convert_p2p_enum_to_wmi_enum(enum p2p_usd_op_type p2p_op_type,
					     WMI_USD_MODE *wmi_mode)
{
	switch (p2p_op_type) {
	case P2P_USD_OP_TYPE_FLUSH:
		*wmi_mode = WMI_USD_MODE_FLUSH;
		break;
	case P2P_USD_OP_TYPE_PUBLISH:
		*wmi_mode = WMI_USD_MODE_PUBLISH;
		break;
	case P2P_USD_OP_TYPE_SUBSCRIBE:
		*wmi_mode = WMI_USD_MODE_SUBSCRIBE;
		break;
	case P2P_USD_OP_TYPE_UPDATE_PUBLISH:
		*wmi_mode = WMI_USD_MODE_UPDATE_PUBLISH;
		break;
	case P2P_USD_OP_TYPE_CANCEL_PUBLISH:
		*wmi_mode = WMI_USD_MODE_CANCEL_PUBLISH;
		break;
	case P2P_USD_OP_TYPE_CANCEL_SUBSCRIBE:
		*wmi_mode = WMI_USD_MODE_CANCEL_SUBSCRIBE;
		break;
	default:
		wmi_err("invalid OP type %d", p2p_op_type);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wmi_p2p_service_protocol_type_convert_p2p_enum_to_wmi_enum() - This API
 * converts P2P_USD_SERVICE_PROTOCOL_TYPE_XX to WMI_USD_SERVICE_PROTOCOL_TYPE_XX
 *
 * @p2p_service_type: P2P USD service protocol type
 * @wmi_service_type: pointer to WMI USD service protocol type
 *
 * Return: QDF status
 */
static QDF_STATUS
wmi_p2p_service_protocol_type_convert_p2p_enum_to_wmi_enum(
			enum p2p_usd_service_protocol_type p2p_service_type,
			WMI_USD_SERVICE_PROTOCOL_TYPE *wmi_service_type)
{
	switch (p2p_service_type) {
	case P2P_USD_SERVICE_PROTOCOL_TYPE_BONJOUR:
		*wmi_service_type = WMI_USD_SERVICE_PROTOCOL_TYPE_BONJOUR;
		break;
	case P2P_USD_SERVICE_PROTOCOL_TYPE_GENERIC:
		*wmi_service_type = WMI_USD_SERVICE_PROTOCOL_TYPE_GENERIC;
		break;
	case P2P_USD_SERVICE_PROTOCOL_TYPE_CSA_MATTER:
		*wmi_service_type = WMI_USD_SERVICE_PROTOCOL_TYPE_CSA_MATTER;
		break;
	default:
		wmi_err("invalid service protocol type %d", p2p_service_type);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * send_p2p_usd_req_cmd_tlv() - send P2P USD command to target
 * @wmi_handle: WMI handle
 * @param: P2P USD attributes parameters
 *
 * Return: QDF status
 */
static QDF_STATUS
send_p2p_usd_req_cmd_tlv(wmi_unified_t wmi_handle,
			 struct p2p_usd_attr_params *param)
{
	wmi_buf_t buf;
	wmi_usd_service_cmd_fixed_param *cmd;
	int32_t len = sizeof(*cmd);
	uint8_t *buf_ptr;
	QDF_STATUS status;
	uint32_t num_chan_len = 0;
	uint32_t sdf_len = 0;
	uint32_t sdf_len_aligned = 0;
	uint32_t num_channel_aligned = 0;
	uint32_t ssi_len = 0;
	uint32_t ssi_len_aligned = 0;
	WMI_USD_MODE usd_mode;

	if (!param) {
		wmi_err("lo start param is null");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_p2p_op_type_convert_p2p_enum_to_wmi_enum(param->op_type,
							      &usd_mode);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	if (usd_mode == WMI_USD_MODE_PUBLISH ||
	    usd_mode == WMI_USD_MODE_SUBSCRIBE) {
		num_chan_len = param->freq_config.freq_list.len;
		num_channel_aligned = qdf_roundup(num_chan_len,
						  sizeof(uint32_t));

		ssi_len = param->ssi.len;
		ssi_len_aligned = qdf_roundup(ssi_len, sizeof(uint32_t));

		sdf_len = param->frame.len;
		sdf_len_aligned = qdf_roundup(sdf_len, sizeof(uint32_t));

		len += 3 * WMI_TLV_HDR_SIZE + sdf_len_aligned +
					ssi_len_aligned + num_channel_aligned;
	} else if (usd_mode == WMI_USD_MODE_UPDATE_PUBLISH) {
		ssi_len = param->ssi.len;
		ssi_len_aligned = qdf_roundup(ssi_len, sizeof(uint32_t));
		len += WMI_TLV_HDR_SIZE + ssi_len_aligned;
	}

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_usd_service_cmd_fixed_param *)wmi_buf_data(buf);
	buf_ptr = (uint8_t *)wmi_buf_data(buf);

	qdf_mem_zero(cmd, sizeof(*cmd));

	wmi_debug("vdev: %d len %d, num_chan_len %d, num_channel_aligned %d sdf_len_aligned %d",
		  param->vdev_id, len, num_chan_len, num_channel_aligned,
		  sdf_len_aligned);

	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_usd_service_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_usd_service_cmd_fixed_param));

	cmd->vdev_id = param->vdev_id;
	cmd->usd_mode = usd_mode;
	cmd->instance_id = param->instance_id;

	if (usd_mode == WMI_USD_MODE_PUBLISH ||
	    usd_mode == WMI_USD_MODE_SUBSCRIBE) {
		status =
		wmi_p2p_service_protocol_type_convert_p2p_enum_to_wmi_enum(
					param->service_info.protocol_type,
					&cmd->protocol_type);
		if (QDF_IS_STATUS_ERROR(status)) {
			wmi_buf_free(buf);
			return status;
		}

		cmd->time_to_live = param->ttl;
		cmd->element_container_attr_data_len = sdf_len;
		cmd->service_specific_info_len = ssi_len;
		cmd->default_freq = param->freq_config.default_freq;

		WMI_CHAR_ARRAY_TO_MAC_ADDR(param->p2p_mac_addr.bytes,
					   &cmd->device_mac_addr);

		WMI_CHAR_ARRAY_TO_GENERIC_HASH(param->service_info.service_id,
					       &cmd->service_id);

		buf_ptr += sizeof(*cmd);
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
			       sdf_len_aligned);
		buf_ptr += WMI_TLV_HDR_SIZE;
		qdf_mem_copy(buf_ptr, param->frame.data, sdf_len);

		buf_ptr += sdf_len_aligned;
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			       num_channel_aligned);
		buf_ptr += WMI_TLV_HDR_SIZE;
		qdf_mem_copy(buf_ptr, param->freq_config.freq_list.freq,
			     num_chan_len);

		buf_ptr += num_channel_aligned;
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ssi_len_aligned);
		buf_ptr += WMI_TLV_HDR_SIZE;
		qdf_mem_copy(buf_ptr, param->ssi.data, ssi_len);
	} else if (usd_mode == WMI_USD_MODE_UPDATE_PUBLISH) {
		buf_ptr += sizeof(*cmd);
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ssi_len_aligned);
		buf_ptr += WMI_TLV_HDR_SIZE;
		qdf_mem_copy(buf_ptr, param->ssi.data, ssi_len);
	}

	wmi_debug("Sending USD req: vdev %d, usd_mode %d, ttl %d, freq %d sdf_len %d",
		  cmd->vdev_id, cmd->usd_mode, cmd->time_to_live,
		  cmd->default_freq, cmd->element_container_attr_data_len);

	wmi_mtrace(WMI_USD_SERVICE_CMDID, cmd->vdev_id, 0);
	status = wmi_unified_cmd_send(wmi_handle,
				      buf, len,
				      WMI_USD_SERVICE_CMDID);

	if (QDF_IS_STATUS_ERROR(status))
		wmi_buf_free(buf);

	return status;
}

/**
 * wmi_p2p_attach_usd_tlv() - attach USD tlv to P2P ops
 * @ops: pointer to WMI ops structure
 *
 * Return: none
 */
static void wmi_p2p_attach_usd_tlv(struct wmi_ops *ops)
{
	ops->send_p2p_usd_req_cmd = send_p2p_usd_req_cmd_tlv;
}
#else
static inline void wmi_p2p_attach_usd_tlv(struct wmi_ops *ops)
{
}
#endif /* FEATURE_WLAN_SUPPORT_USD */

void wmi_p2p_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->send_set_p2pgo_oppps_req_cmd = send_set_p2pgo_oppps_req_cmd_tlv;
	ops->send_set_p2pgo_noa_req_cmd = send_set_p2pgo_noa_req_cmd_tlv;
	ops->extract_p2p_noa_ev_param = extract_p2p_noa_ev_param_tlv;
	ops->extract_mac_addr_rx_filter_evt_param =
				extract_mac_addr_rx_filter_evt_param_tlv;
	ops->send_p2p_ap_assist_dfs_group_params =
				send_p2p_ap_assist_dfs_group_params_cmd_tlv;
	ops->extract_p2p_ap_assist_dfs_group_bmiss =
				extract_p2p_ap_assist_dfs_group_bmiss_param_tlv;
	wmi_p2p_attach_usd_tlv(ops);
	wmi_p2p_listen_offload_attach_tlv(wmi_handle);
}
