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

#include <osdep.h>
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wmi_unified_wifi_radar_api.h"

static QDF_STATUS
extract_wifi_radar_cal_status_param_tlv
		(wmi_unified_t wmi_handle,
		 void *evt_buf,
		 struct wmi_wifi_radar_cal_status_param *param)
{
	WMI_PDEV_WIFI_RADAR_CAL_COMPLETION_STATUS_EVENTID_param_tlvs *ev_buf;
	wmi_pdev_wifi_radar_cal_completion_status_event_param *ev_param;

	ev_buf = (WMI_PDEV_WIFI_RADAR_CAL_COMPLETION_STATUS_EVENTID_param_tlvs *)evt_buf;
	if (!ev_buf) {
		wmi_err("Invalid wifi radar cal status event buffer");
		return QDF_STATUS_E_INVAL;
	}

	ev_param = ev_buf->cal_completion_status_event_param;

	param->pdev_id = wmi_handle->ops->convert_pdev_id_target_to_host
				(wmi_handle, ev_param->pdev_id);
	param->wifi_radar_pkt_bw = ev_param->wifi_radar_pkt_bw;
	param->channel_bw = ev_param->channel_bw;
	param->band_center_freq = ev_param->band_center_freq;
	param->num_ltf_tx = ev_param->num_ltf_tx;
	param->num_skip_ltf_rx = ev_param->num_skip_ltf_rx;
	param->num_ltf_accumulation = ev_param->num_ltf_accumulation;
	qdf_mem_copy(param->per_chain_cal_status,
		     ev_param->per_chain_cal_status,
		     QDF_MIN(sizeof(param->per_chain_cal_status),
			     sizeof(ev_param->per_chain_cal_status)));
	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wifi_radar_send_command_tlv(wmi_unified_t wmi_handle,
			    struct wmi_wifi_radar_command_params *param)
{
	wmi_pdev_enable_wifi_radar_cmd_fixed_param *cmd;
	struct wmi_ops *ops = wmi_handle->ops;
	wmi_buf_t buf;
	int len = sizeof(*cmd);
	int ret;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("%s:wmi_buf_alloc failed", __func__);
		return QDF_STATUS_E_NOMEM;
	}

	cmd = (wmi_pdev_enable_wifi_radar_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_enable_wifi_radar_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		(wmi_pdev_enable_wifi_radar_cmd_fixed_param));
	cmd->pdev_id = ops->convert_host_pdev_id_to_target(wmi_handle,
							   param->pdev_id);
	cmd->tx_chain_mask = param->tx_chainmask;
	cmd->rx_chain_mask = param->rx_chainmask;
	cmd->num_ltf_tx = param->num_ltf_tx;
	cmd->num_skip_ltf_rx = param->num_skip_ltf_rx;
	cmd->num_ltf_accumulation = param->num_ltf_accumulation;
	cmd->bw = param->bandwidth;
	cmd->capture_interval_ms = param->periodicity;
	cmd->capture_calibrate = param->cmd_type;

	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_PDEV_ENABLE_WIFI_RADAR_CMDID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wmi_err("Failed to send WMI_PDEV_ENABLE_WIFI_RADAR_CMDID");
		wmi_buf_free(buf);
	}

	return ret;
}

void wmi_wifi_radar_attach_tlv(wmi_unified_t wmi_handle)
{
	struct wmi_ops *ops = wmi_handle->ops;

	ops->extract_wifi_radar_cal_status_param =
		extract_wifi_radar_cal_status_param_tlv;
	ops->wifi_radar_send_command =
		wifi_radar_send_command_tlv;
}
