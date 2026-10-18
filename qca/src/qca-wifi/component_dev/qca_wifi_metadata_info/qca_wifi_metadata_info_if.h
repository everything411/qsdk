/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
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
#ifndef _QCA_WIFI_CLASSIFIER_IF_
#define _QCA_WIFI_CLASSIFIER_IF_

#include "qca_sawf_if.h"
#include "qca_wifi_mlo_metadata_info_if.h"

/*
 * Wifi classifier metadata
 * ----------------------------------------------------------------------------
 * | TAG    | mlo_key_valid| sawf_valid| reserved| MLO key | peer_id | MSDUQ   |
 * |(8 bits)|   (1 bit)    |  (1 bit)  | (1 bit) | (5 bits)| (10 bit)| (6 bits)|
 * ----------------------------------------------------------------------------
 */

/**
 * struct qca_wifi_metadata_info - wifi classifier metadata
 * @mlo_param: mlo metadata info
 * @sawf_param: sawf param
 */
struct qca_wifi_metadata_info {
	uint8_t is_mlo_param_valid:1,
		is_sawf_param_valid:1,
		reserved:6;
	struct qca_mlo_metadata_param mlo_param;
	struct qca_sawf_metadata_param sawf_param;
};

/*
 * qca_wifi_get_metadata_info() - get qca wifi metadata
 * @wifi_info : wifi metadata.
 *
 * Return: updated Key value for per packet path.
 */
uint32_t qca_wifi_get_metadata_info(
		struct qca_wifi_metadata_info *wifi_info);
#endif
