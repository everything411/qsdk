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
#include "wmi_unified_priv.h"
#include "wmi_unified_wifi_radar_api.h"

QDF_STATUS
wmi_extract_wifi_radar_cal_status_event_param(
		wmi_unified_t wmi_handle,
		void *evt_buf,
		struct wmi_wifi_radar_cal_status_param *param)
{
	if (wmi_handle->ops->extract_wifi_radar_cal_status_param)
		return wmi_handle->ops->extract_wifi_radar_cal_status_param
				(wmi_handle, evt_buf, param);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS
wmi_unified_send_wifi_radar_command(
		wmi_unified_t wmi_handle,
		struct wmi_wifi_radar_command_params *param)
{
	if (wmi_handle->ops->wifi_radar_send_command)
		return wmi_handle->ops->wifi_radar_send_command
				(wmi_handle, param);

	return QDF_STATUS_E_FAILURE;
}

