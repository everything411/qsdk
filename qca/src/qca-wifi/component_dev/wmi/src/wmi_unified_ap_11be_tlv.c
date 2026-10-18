/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
#include <wmi_unified_ap_api.h>

uint8_t *bcn_offload_quiet_add_ml_partner_links(
		uint8_t *buf_ptr,
		struct set_bcn_offload_quiet_mode_params *param)
{
	wmi_vdev_bcn_offload_ml_quiet_config_params *ml_quiet;
	struct wmi_mlo_bcn_offload_partner_links *partner_link;
	uint8_t i;

	if (param->mlo_partner.num_links > WLAN_UMAC_MLO_MAX_VDEVS) {
		wmi_err("mlo_partner.num_link(%d) are greater than supported partner links(%d)",
				param->mlo_partner.num_links, WLAN_UMAC_MLO_MAX_VDEVS);
		return buf_ptr;
	}

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       (param->mlo_partner.num_links *
			sizeof(wmi_vdev_bcn_offload_ml_quiet_config_params)));
	buf_ptr += sizeof(uint32_t);

	partner_link = &param->mlo_partner;
	ml_quiet = (wmi_vdev_bcn_offload_ml_quiet_config_params *)buf_ptr;
	for (i = 0; i < partner_link->num_links; i++) {
		WMITLV_SET_HDR(&ml_quiet->tlv_header,
			       WMITLV_TAG_STRUC_wmi_vdev_bcn_offload_ml_quiet_config_params,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_bcn_offload_ml_quiet_config_params)
			       );
		ml_quiet->vdev_id = partner_link->partner_info[i].vdev_id;
		ml_quiet->hw_link_id = partner_link->partner_info[i].hw_link_id;
		ml_quiet->beacon_interval =
			partner_link->partner_info[i].bcn_interval;
		ml_quiet->period = partner_link->partner_info[i].period;
		ml_quiet->duration = partner_link->partner_info[i].duration;
		ml_quiet->next_start = partner_link->partner_info[i].next_start;
		ml_quiet->flags = partner_link->partner_info[i].flag;
		ml_quiet++;
	}

	return buf_ptr + (param->mlo_partner.num_links *
			sizeof(wmi_vdev_bcn_offload_ml_quiet_config_params));
}

size_t quiet_mlo_params_size(struct set_bcn_offload_quiet_mode_params *params)
{
	size_t quiet_mlo_size;

	quiet_mlo_size = WMI_TLV_HDR_SIZE +
			 (sizeof(wmi_vdev_bcn_offload_ml_quiet_config_params) *
			  params->mlo_partner.num_links);

	return quiet_mlo_size;
}

QDF_STATUS
send_wmi_link_recommendation_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wlan_link_recmnd_param *param)
{
	wmi_mlo_link_recommendation_fixed_param *cmd;
	wmi_mlo_peer_recommended_links *peer_cmd;
	wmi_buf_t buf;
	void *buf_ptr;
	int32_t len = sizeof(wmi_mlo_link_recommendation_fixed_param) +
		sizeof(wmi_mlo_peer_recommended_links) + WMI_TLV_HDR_SIZE;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		wmi_err("wmi_buf_alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *)wmi_buf_data(buf);
	cmd = buf_ptr;
	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_mlo_link_recommendation_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN
		(wmi_mlo_link_recommendation_fixed_param));

	cmd->vdev_id = param->vdev_id;
	buf_ptr += sizeof(wmi_mlo_link_recommendation_fixed_param);
	WMITLV_SET_HDR(
		buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		sizeof(wmi_mlo_peer_recommended_links));
	buf_ptr += sizeof(uint32_t);
	peer_cmd = (wmi_mlo_peer_recommended_links *)buf_ptr;
	WMITLV_SET_HDR(&peer_cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_mlo_peer_recommended_links,
			WMITLV_GET_STRUCT_TLVLEN(wmi_mlo_peer_recommended_links));

	peer_cmd->assoc_id = param->assoc_id;
	peer_cmd->linkid_bitmap = (uint32_t)param->linkid_bitmap;
	buf_ptr += sizeof(wmi_mlo_peer_recommended_links);

	wmi_mtrace(WMI_MLO_LINK_RECOMMENDATION_CMDID, cmd->vdev_id, 0);
	if (wmi_unified_cmd_send(wmi_handle, buf, len,
		WMI_MLO_LINK_RECOMMENDATION_CMDID)) {
		wmi_err("Send wmi link recommendation cmd failed");
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11BE
QDF_STATUS
send_mu_on_off_cmd_tlv(wmi_unified_t wmi_handle,
		       struct wmi_host_mu_on_off_params *params)
{
	wmi_vdev_sched_mode_probe_req_fixed_param *cmd;
	wmi_buf_t buf;
	QDF_STATUS ret = QDF_STATUS_SUCCESS;
	uint32_t buf_len = 0;

	buf_len = sizeof(wmi_vdev_sched_mode_probe_req_fixed_param);

	buf = wmi_buf_alloc(wmi_handle, buf_len);
	if (!buf) {
		wmi_err("wmi buf alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	cmd = (wmi_vdev_sched_mode_probe_req_fixed_param *)wmi_buf_data(buf);

	WMITLV_SET_HDR(
		&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_sched_mode_probe_req_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_sched_mode_probe_req_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->on_duration_ms = params->mu_on_duration;
	cmd->off_duration_ms = params->mu_off_duration;
	cmd->sched_mode_to_probe = 1;

	wmi_info("vdev id: %d, mu_on: %d, mu_off: %d", params->vdev_id,
		 params->mu_on_duration, params->mu_off_duration);

	ret = wmi_unified_cmd_send(wmi_handle, buf, buf_len,
				   WMI_VDEV_SCHED_MODE_PROBE_REQ_CMDID);
	if (ret) {
		wmi_err("Failed to send MU Toggle command to FW", ret);
		wmi_buf_free(buf);
		return QDF_STATUS_E_FAILURE;
	}

	return ret;
}
#endif /* WLAN_FEATURE_11BE */
