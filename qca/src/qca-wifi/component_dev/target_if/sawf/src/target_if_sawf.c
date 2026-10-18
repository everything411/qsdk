/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <qdf_status.h>
#include <qdf_mem.h>
#include <wlan_tgt_def_config.h>
#include <target_type.h>
#include <target_if.h>
#include <wlan_lmac_if_def.h>
#include <init_deinit_lmac.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wmi_unified_param.h>
#include <wmi_unified_ap_params.h>
#include <wmi_unified_ap_api.h>

#include <wlan_sawf.h>

QDF_STATUS
target_if_sawf_svc_create_send(struct wlan_objmgr_pdev *pdev, void *svc_params)
{
	wmi_unified_t wmi_handle;
	struct wlan_sawf_svc_class_params *svc;
	struct wmi_sawf_params param;
	QDF_STATUS status;

	svc = (struct wlan_sawf_svc_class_params *)svc_params;

	wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!wmi_handle) {
		qdf_err("Invalid wmi handle");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&param, sizeof(param));

	param.svc_id = svc->svc_id;
	param.min_thruput_rate = svc->min_thruput_rate;
	param.max_thruput_rate = svc->max_thruput_rate;
	param.burst_size = svc->burst_size;
	param.service_interval = svc->service_interval;
	param.delay_bound = svc->delay_bound;
	param.msdu_ttl = svc->msdu_ttl;
	param.priority = svc->priority;
	param.tid = svc->tid;
	param.msdu_rate_loss = svc->msdu_rate_loss;
	param.disabled_modes = svc->disabled_modes;

	status = wmi_sawf_create_send(wmi_handle, &param);

	/* After WMI call, the params are updated with
	 * the FW provided details, so we need to update
	 * the same information in our database.
	 */
	svc->svc_id = param.svc_id;
	svc->min_thruput_rate = param.min_thruput_rate;
	svc->max_thruput_rate = param.max_thruput_rate;
	svc->burst_size = param.burst_size;
	svc->service_interval = param.service_interval;
	svc->delay_bound = param.delay_bound;
	svc->msdu_ttl = param.msdu_ttl;
	svc->priority = param.priority;
	svc->tid = param.tid;
	svc->msdu_rate_loss = param.msdu_rate_loss;

	return status;
}

QDF_STATUS
target_if_sawf_svc_disable_send(struct wlan_objmgr_pdev *pdev, void *svc_params)
{
	wmi_unified_t wmi_handle;
	struct wlan_sawf_svc_class_params *svc;
	QDF_STATUS status;

	svc = (struct wlan_sawf_svc_class_params *)svc_params;

	wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!wmi_handle) {
		qdf_err("Invalid wmi handle");
		return QDF_STATUS_E_FAILURE;
	}

	status = wmi_sawf_disable_send(wmi_handle, svc->svc_id);

	return status;
}

QDF_STATUS
target_if_sawf_ul_svc_update_params(struct wlan_objmgr_pdev *pdev,
				    uint8_t vdev_id, uint8_t *peer_mac,
				    uint8_t ac, uint8_t add_or_sub,
				    void *svc_params)
{
	wmi_unified_t wmi_handle;
	struct wlan_sawf_svc_class_params *svc;
	struct wmi_peer_latency_config_params param;
	QDF_STATUS status;

	svc = (struct wlan_sawf_svc_class_params *)svc_params;

	wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!wmi_handle) {
		qdf_err("Invalid wmi handle");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&param, sizeof(param));

	param.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	param.num_peer = 1;
	param.latency_info[0].ul_enable = 1;

	param.latency_info[0].latency_tid = svc->tid;
	param.latency_info[0].service_interval = svc->ul_service_interval;
	param.latency_info[0].burst_size = svc->ul_burst_size;
	param.latency_info[0].min_throughput = svc->ul_min_tput;
	param.latency_info[0].max_latency = svc->ul_max_latency;
	param.latency_info[0].add_or_sub = add_or_sub;
	param.latency_info[0].ac = ac;
	param.latency_info[0].sawf_ul_param = 1;

	qdf_mem_copy(param.latency_info[0].peer_mac, peer_mac,
		     QDF_MAC_ADDR_SIZE);

	qdf_info("UL SAWF: mac " QDF_MAC_ADDR_FMT
		 " tid %u ac %u svc_intvl %u burst_size %u add_sub %u",
		 QDF_MAC_ADDR_REF(peer_mac), svc->tid, ac,
		 svc->ul_service_interval, svc->ul_burst_size, add_or_sub);

	status = wmi_unified_config_peer_latency_info_cmd_send(wmi_handle,
							       &param);
	return status;
}

QDF_STATUS
target_if_sawf_update_ul_params(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,
				uint8_t *peer_mac, uint8_t tid, uint8_t ac,
				uint32_t service_interval, uint32_t burst_size,
				uint32_t min_tput, uint32_t max_latency,
				uint8_t add_or_sub)
{
	wmi_unified_t wmi_handle;
	struct wmi_peer_latency_config_params param;
	QDF_STATUS status;

	wmi_handle = lmac_get_pdev_wmi_handle(pdev);
	if (!wmi_handle) {
		qdf_err("Invalid wmi handle");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&param, sizeof(param));

	param.pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	param.num_peer = 1;
	param.latency_info[0].ul_enable = 1;

	param.latency_info[0].latency_tid = tid;
	param.latency_info[0].service_interval = service_interval;
	param.latency_info[0].burst_size = burst_size;
	param.latency_info[0].min_throughput = min_tput;
	param.latency_info[0].max_latency = max_latency;
	param.latency_info[0].add_or_sub = add_or_sub;
	param.latency_info[0].ac = ac;
	param.latency_info[0].sawf_ul_param = 1;

	qdf_mem_copy(param.latency_info[0].peer_mac, peer_mac,
		     QDF_MAC_ADDR_SIZE);

	qdf_info("UL SAWF: mac " QDF_MAC_ADDR_FMT
		 " tid %u ac %u svc_intvl %u burst_size %u add_sub %u",
		 QDF_MAC_ADDR_REF(peer_mac), tid, ac,
		 service_interval, burst_size, add_or_sub);

	status = wmi_unified_config_peer_latency_info_cmd_send(wmi_handle,
							       &param);
	return status;
}
