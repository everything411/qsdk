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
#ifndef _QCA_MLO_CLASSIFIER_
#define _QCA_MLO_CLASSIFIER_

/*
 * Wifi classifier metadata
 * ----------------------------------------------------------------------------
 * | TAG    | mlo_key_valid| sawf_valid| reserved| MLO key | peer_id | MSDUQ   |
 * |(8 bits)|   (1 bit)    |  (1 bit)  | (1 bit) | (5 bits)| (10 bit)| (6 bits)|
 * ----------------------------------------------------------------------------
 */

/**
 ** MLO metadata related information.
 **/
/* MLO TAG is same as that of SAWF tag(0xAA) with mlo mask as valid */
#define MLO_METADATA_VALID_TAG 0x155
#define MLO_METADATA_TAG_SHIFT 0x17
#define MLO_METADATA_TAG_MASK 0x1FF

#define MLO_METADATA_INVALID_DS_NODE_ID 0xff
#define MLO_METADATA_LINK_ID_SHIFT 0x10
#define MLO_METADATA_LINK_ID_MASK 0x1f

/**
 ** ECM_MLO hash metadata extraction.
 **/
#define MLO_METADATA_TAG_GET(x) (((x) >> MLO_METADATA_TAG_SHIFT) \
	& MLO_METADATA_TAG_MASK)
#define MLO_METADATA_TAG_IS_VALID(x) \
	((MLO_METADATA_TAG_GET(x) == MLO_METADATA_VALID_TAG) \
	? true : false)

#define MLO_METADATA_LINK_ID_GET(x) \
	(((x) >> MLO_METADATA_LINK_ID_SHIFT) & MLO_METADATA_LINK_ID_MASK)

#endif
