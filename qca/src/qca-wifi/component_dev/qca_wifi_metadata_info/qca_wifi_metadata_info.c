/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "qca_wifi_metadata_info_if.h"
#include "qdf_module.h"

#define DP_SAWF_META_DATA_INVALID 0xffffffff

uint32_t qca_wifi_get_metadata_info(
		struct qca_wifi_metadata_info *wifi_info)
{
	uint32_t sawf_key = 0;
	uint32_t mlo_key = 0;
	uint32_t wifi_key = 0;

	if (wifi_info->is_mlo_param_valid) {
		mlo_key = qca_mlo_get_mark_metadata(
						&wifi_info->mlo_param);
		wifi_key |= mlo_key;
	}

	if (wifi_info->is_sawf_param_valid) {
		struct qca_mlo_metadata_param mlo_param = {0};

		/* Even through the request is only for sawf info
		 * fill the mlo metadata as well
		 */
		if (!wifi_info->is_mlo_param_valid) {
			mlo_param.in_dest_dev = wifi_info->sawf_param.netdev;
			mlo_param.in_dest_mac = wifi_info->sawf_param.peer_mac;

			mlo_key = qca_mlo_get_mark_metadata(&mlo_param);
			wifi_key |= mlo_key;
		}

		sawf_key = qca_sawf_get_mark_metadata(
						&wifi_info->sawf_param);
		if (sawf_key != DP_SAWF_META_DATA_INVALID)
			wifi_key |= sawf_key;
	}

	return wifi_key;
}

qdf_export_symbol(qca_wifi_get_metadata_info);
