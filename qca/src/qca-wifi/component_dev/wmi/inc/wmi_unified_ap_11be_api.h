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

/*
 * This file contains the API definitions for 11BE WMIs
 */

#ifndef _WMI_UNIFIED_AP_11BE_API_H_
#define _WMI_UNIFIED_AP_11BE_API_H_

#ifdef WLAN_FEATURE_11BE_MLO
/**
 * bcn_offload_quiet_add_ml_partner_links - Add MLO partner links in Quiet WMI
 *                                          command
 * @buf_ptr: Pointer to Quiet IE command buffer
 * @param: Pointer to Quiet params
 *
 * Return: pointer to new offset of cmd buffer
 */
uint8_t *bcn_offload_quiet_add_ml_partner_links(
		uint8_t *buf_ptr,
		struct set_bcn_offload_quiet_mode_params *param);

/**
 * quiet_mlo_params_size() - Get ML params size for Quiet command
 * @param: Pointer to Quiet params structure
 *
 * Return: size of ML params in Quiet WMI command
 */
size_t quiet_mlo_params_size(struct set_bcn_offload_quiet_mode_params *params);

/**
 * send_wmi_link_recommendation_cmd_tlv() - send link recommendation config command to fw
 * @wmi_handle: WMI handle
 * @param: structure to get link recommendation config
 *
 * return: QDF_STATUS
 */
QDF_STATUS send_wmi_link_recommendation_cmd_tlv(
		wmi_unified_t wmi_handle,
		struct wlan_link_recmnd_param *param);
#else
static uint8_t *bcn_offload_quiet_add_ml_partner_links(
		uint8_t *buf_ptr,
		struct set_bcn_offload_quiet_mode_params *param)
{
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	return buf_ptr + WMI_TLV_HDR_SIZE;
}

static size_t quiet_mlo_params_size(
		struct set_bcn_offload_quiet_mode_params *params)
{
	return WMI_TLV_HDR_SIZE;
}
#endif /* WLAN_FEATURE_11BE_MLO */

#ifdef WLAN_FEATURE_11BE
/**
 * send_mu_on_off_cmd_tlv() - send mu on off params to FW
 * @wmi_handle: WMI handle
 * @param: structure containing mu on/off info
 *
 * return: QDF_STATUS
 */
QDF_STATUS
send_mu_on_off_cmd_tlv(wmi_unified_t wmi_handle,
		       struct wmi_host_mu_on_off_params *param);
#endif /* WLAN_FEATURE_11BE */
#endif
