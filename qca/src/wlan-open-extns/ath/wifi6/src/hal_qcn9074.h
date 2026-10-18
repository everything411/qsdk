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

extern const struct hal_ops hal_qcn9074_ops;


static inline u32 ath12k_wifi6_hal_get_rx_desc_size_qcn9074(void)
{
	return sizeof(struct hal_rx_desc_qcn9074);
}

static inline
u32 ath12k_wifi6_hal_rx_desc_get_mpdu_start_tag_qcn9074(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9074.mpdu_start_tag,
			HAL_TLV_HDR_TAG);
}

static inline
u8 ath12k_wifi6_dp_mon_rx_decap_format_get(struct hal_rx_desc *desc)
{
    return le32_get_bits(desc->u.qcn9074.msdu_start.info2,
                 RX_MSDU_START_INFO2_DECAP_FORMAT);
}

static inline
bool ath12k_wifi6_dp_rx_desc_is_first_msdu(struct hal_rx_desc *desc)
{
	return !!le16_get_bits(desc->u.qcn9074.msdu_end.info4,
			RX_MSDU_END_INFO4_FIRST_MSDU);
}

static inline
uint32_t ath12k_wifi6_hal_rx_desc_get_mpdu_ppdu_id(
		struct hal_reo_entrance_ring *buff)
{
	return le32_get_bits(buff->info2, HAL_REO_ENTR_RING_INFO2_PHY_PPDU_ID);
}

static inline
uint32_t ath12k_wifi6_dp_rx_desc_mpdu_fcs_err_get(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9074.attention.info1,
			RX_ATTENTION_INFO1_FCS_ERR);
}

static inline
uint32_t ath12k_wifi6_dp_rx_desc_mpdu_len_err_get(struct hal_rx_desc *desc)
{
	return le32_get_bits(desc->u.qcn9074.attention.info1,
			RX_ATTENTION_INFO1_MPDU_LEN_ERR);
}

static inline
u8 *ath12k_wifi6_hal_rx_desc_get_msdu_payload_qcn9074(struct hal_rx_desc *desc)
{
	return &desc->u.qcn9074.msdu_payload[0];
}
