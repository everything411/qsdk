/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/ieee80211.h>
#include <linux/etherdevice.h>
#include "../../hal.h"
#include "hal_tx.h"
#include "hal_rx.h"
#include "./hal_desc.h"

extern const struct hal_ops hal_qcn9160_ops;

static inline u32 ath12k_wifi6_hal_get_rx_desc_size_qcn9160(void)
{
	return sizeof(struct hal_rx_desc_qcn9160);
}

static inline
u32 ath12k_wifi6_hal_rx_desc_get_mpdu_start_tag_qcn9160(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9160.mpdu_start_tag,
			HAL_TLV_HDR_TAG);
}

static inline
u8 *ath12k_wifi6_hal_rx_desc_get_msdu_payload_qcn9160(struct hal_rx_desc *desc)
{
	return &desc->u.qcn9160.msdu_payload[0];
}
