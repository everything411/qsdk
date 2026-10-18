// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../debug.h"
#include "../../hif.h"
#include "hal.h"
#include "hal_rx.h"

void ath12k_wifi6_hal_rx_reo_ent_buf_paddr_get(void *rx_desc,
					       struct hal_rx_buf_info *buf_info,
					       u32 *msdu_cnt)
{
	struct hal_reo_entrance_ring *reo_ent_ring =
		(struct hal_reo_entrance_ring *)rx_desc;
	struct ath12k_buffer_addr *buf_addr_info;
	struct rx_mpdu_desc *rx_mpdu_desc_info_details;

	rx_mpdu_desc_info_details =
			(struct rx_mpdu_desc *)&reo_ent_ring->rx_mpdu_info;

	*msdu_cnt = le32_get_bits(rx_mpdu_desc_info_details->info0,
				  RX_MPDU_DESC_INFO0_MSDU_COUNT);

	buf_addr_info = (struct ath12k_buffer_addr *)&reo_ent_ring->buf_addr_info;

	buf_info->paddr = (((u64)le32_get_bits(buf_addr_info->info1,
					       WIFI6_BUFFER_ADDR_INFO1_ADDR)) << 32) |
					le32_get_bits(buf_addr_info->info0,
						      WIFI6_BUFFER_ADDR_INFO0_ADDR);

	buf_info->sw_cookie = le32_get_bits(buf_addr_info->info1,
					    WIFI6_BUFFER_ADDR_INFO1_SW_COOKIE);
	buf_info->rbm = le32_get_bits(buf_addr_info->info1,
				      WIFI6_BUFFER_ADDR_INFO1_RET_BUF_MGR);
}

void
ath12k_wifi6_hal_rx_msdu_link_desc_set(struct ath12k_base *ab,
				       struct hal_wbm_release_ring *desc,
				       struct ath12k_buffer_addr *buf_addr_info,
				       enum hal_wbm_rel_bm_act action)
{
	desc->buf_addr_info = *buf_addr_info;
	desc->info0 |= le32_encode_bits(HAL_WBM_REL_SRC_MODULE_SW,
					HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE) |
		    le32_encode_bits(action, HAL_WBM_RELEASE_INFO0_BM_ACTION) |
		    le32_encode_bits(HAL_WBM_REL_DESC_TYPE_MSDU_LINK,
				     HAL_WBM_RELEASE_INFO0_DESC_TYPE);
}

void ath12k_wifi6_hal_rx_msdu_list_get(void *desc,
				       void *list,
				       u16 *num_msdus)
{
	struct hal_rx_msdu_link *link_desc = (struct hal_rx_msdu_link *)desc;
	struct hal_rx_msdu_list *msdu_list = (struct hal_rx_msdu_list *)list;
	struct hal_rx_msdu_details *msdu_details = NULL;
	struct rx_msdu_desc *msdu_desc_info = NULL;
	u32 last = 0, first = 0;
	u8 tmp = 0;
	int i;

	last = u32_encode_bits(last, RX_MSDU_DESC_INFO0_LAST_MSDU_IN_MPDU);
	first = u32_encode_bits(first, RX_MSDU_DESC_INFO0_FIRST_MSDU_IN_MPDU);
	msdu_details = &link_desc->msdu_link[0];

	for (i = 0; i < HAL_RX_NUM_MSDU_DESC; i++) {
		/* num_msdus received in mpdu descriptor may be incorrect
		 * sometimes due to HW issue. Check msdu buffer address also
		 */
		if (!i && le32_get_bits(msdu_details[i].buf_addr_info.info0,
					WIFI6_BUFFER_ADDR_INFO0_ADDR) == 0)
			break;
		if (le32_get_bits(msdu_details[i].buf_addr_info.info0,
				  WIFI6_BUFFER_ADDR_INFO0_ADDR) == 0) {
			/* set the last msdu bit in the prev msdu_desc_info */
			msdu_desc_info = &msdu_details[i - 1].rx_msdu_info;
			msdu_desc_info->info0 |= cpu_to_le32(last);
			break;
		}
		msdu_desc_info = &msdu_details[i].rx_msdu_info;

		/* set first MSDU bit or the last MSDU bit */
		if (!i)
			msdu_desc_info->info0 |= cpu_to_le32(first);
		else if (i == (HAL_RX_NUM_MSDU_DESC - 1))
			msdu_desc_info->info0 |= cpu_to_le32(last);

		msdu_list->msdu_info[i].msdu_flags = le32_to_cpu(msdu_desc_info->info0);
		msdu_list->msdu_info[i].msdu_len =
			 HAL_RX_MSDU_PKT_LENGTH_GET(msdu_desc_info->info0);
		msdu_list->sw_cookie[i] =
			le32_get_bits(msdu_details[i].buf_addr_info.info1,
				      WIFI6_BUFFER_ADDR_INFO1_SW_COOKIE);
		tmp = le32_get_bits(msdu_details[i].buf_addr_info.info1,
				    WIFI6_BUFFER_ADDR_INFO1_RET_BUF_MGR);
		msdu_list->paddr[i] =
			((u64)(le32_get_bits(msdu_details[i].buf_addr_info.info1,
					     WIFI6_BUFFER_ADDR_INFO1_ADDR)) << 32) |
			le32_get_bits(msdu_details[i].buf_addr_info.info0,
				      WIFI6_BUFFER_ADDR_INFO0_ADDR);
		msdu_list->rbm[i] = tmp;
	}
	*num_msdus = i;
}

void
ath12k_wifi6_dp_mon_rx_priv_info_set(u8 *buf, u8 *priv_data, u8 len)
{
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)buf;

	len = (len > HAL_RX_DESC_PADDING0_BYTES) ?
		  HAL_RX_DESC_PADDING0_BYTES : len;
	memcpy(rx_desc->u.qcn9074.rx_padding0, priv_data, len);
}

void
ath12k_wifi6_dp_mon_rx_priv_info_get(u8 *buf, u8 *priv_data, u8 len)
{
	struct hal_rx_desc *rx_desc = (struct hal_rx_desc *)buf;

	len = (len > HAL_RX_DESC_PADDING0_BYTES) ?
		HAL_RX_DESC_PADDING0_BYTES : len;
	memcpy(priv_data, rx_desc->u.qcn9074.rx_padding0, len);
}

void ath12k_wifi6_hal_rx_buf_addr_info_set(struct ath12k_buffer_addr *b_info,
					   dma_addr_t paddr, u32 cookie,
					   u8 manager)
{
	struct ath12k_buffer_addr *binfo = (struct ath12k_buffer_addr *)b_info;
	u32 paddr_lo, paddr_hi;

	paddr_lo = lower_32_bits(paddr);
	paddr_hi = upper_32_bits(paddr);

	binfo->info0 = le32_encode_bits(paddr_lo, WIFI6_BUFFER_ADDR_INFO0_ADDR);
	binfo->info1 = le32_encode_bits(paddr_hi, WIFI6_BUFFER_ADDR_INFO1_ADDR) |
		le32_encode_bits(manager, WIFI6_BUFFER_ADDR_INFO1_RET_BUF_MGR) |
		le32_encode_bits(cookie, WIFI6_BUFFER_ADDR_INFO1_SW_COOKIE);
}

void ath12k_wifi6_hal_rx_buf_addr_info_get(struct ath12k_buffer_addr *b_info,
					   dma_addr_t *paddr, u32 *cookie,
					   u8 *rbm)
{
	struct ath12k_buffer_addr *binfo = (struct ath12k_buffer_addr *)b_info;

	*paddr = (((u64)le32_get_bits(binfo->info1,
				      WIFI6_BUFFER_ADDR_INFO1_ADDR)) << 32) |
				      le32_get_bits(binfo->info0, WIFI6_BUFFER_ADDR_INFO0_ADDR);
	*cookie = le32_get_bits(binfo->info1, WIFI6_BUFFER_ADDR_INFO1_SW_COOKIE);
	*rbm = le32_get_bits(binfo->info1, WIFI6_BUFFER_ADDR_INFO1_RET_BUF_MGR);
}
