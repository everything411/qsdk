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
#ifndef _QCA_MLO_CLASSIFIER_IF_
#define _QCA_MLO_CLASSIFIER_IF_

/*
 * Wifi classifier metadata
 * ----------------------------------------------------------------------------
 * | TAG    | mlo_key_valid| sawf_valid| reserved| MLO key | peer_id | MSDUQ   |
 * |(8 bits)|   (1 bit)    |  (1 bit)  | (1 bit) | (5 bits)| (10 bit)| (6 bits)|
 * ----------------------------------------------------------------------------
 */

/**
 * struct qca_mlo_metadata_param - mlo metadata info
 * @in_dest_dev: destination dev.
 * @in_dest_mac: destination peer mac address.
 * @out_ppe_ds_node: DS node id.
 */
struct qca_mlo_metadata_param {
	struct net_device *in_dest_dev;
	uint8_t *in_dest_mac;
	uint32_t out_ppe_ds_node_id;
};

/*
 * qca_mlo_get_mark_metadata() - get mlo classifier metadata
 * @mlo_param : mlo classifier param.
 *
 * Return: Key value for per packet path.
 */

uint32_t qca_mlo_get_mark_metadata(struct qca_mlo_metadata_param *mlo_param);
#endif
