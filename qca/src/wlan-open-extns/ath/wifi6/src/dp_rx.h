/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_RX_WIFI6_H
#define ATH12K_DP_RX_WIFI6_H

#include "../../dp_rx.h"
#include "../../dp_mon.h"
#include "dp.h"
#include "hal.h"

#define DP_BA_WIN_SZ_MAX_WIFI6 256

#define ATH12K_WIFI6_RX_DESC_COOKIE_POOL_ID_SHIFT	18
#define ATH12K_RX_DESC_COOKIE_INDEX_SHIFT 		0
#define ATH12K_RX_DESC_COOKIE_INDEX_MASK 		0x3ffff /* 18 bits */

#define ATH12K_DP_RX_DESC_COOKIE_INDEX_GET(val) \
	(((val) & ATH12K_RX_DESC_COOKIE_INDEX_MASK) >> \
			ATH12K_RX_DESC_COOKIE_INDEX_SHIFT)

int ath12k_wifi6_dp_rx_process_wbm_err(struct ath12k_dp *dp,
				       struct napi_struct *napi, int budget);
int ath12k_wifi6_dp_rx_process_err(struct ath12k_dp *dp, struct napi_struct *napi,
				   int budget);
int ath12k_wifi6_dp_rx_process(struct ath12k_dp *dp, int mac_id,
			       struct napi_struct *napi,
			       int budget);
int ath12k_wifi6_dp_rx_link_desc_return(struct ath12k_dp *dp,
					struct ath12k_buffer_addr *buf_addr_info,
					enum hal_wbm_rel_bm_act action);
void ath12k_wifi6_dp_rx_process_reo_status(struct ath12k_dp *dp);
int ath12k_wifi6_dp_rxdma_ring_sel_config_qcn9074(struct ath12k_base *ab);
int ath12k_wifi6_dp_rxdma_ring_sel_config_qcn9160(struct ath12k_base *ab);

static inline
u8 ath12k_wifi6_dp_rx_get_rbm_id(struct ath12k_dp *dp)
{
	struct ath12k_base *ab = dp->ab;

	if (!ab->hw_params->rxdma1_enable)
		return HAL_RX_BUF_RBM_SW3_BM;
	else
		return HAL_RX_BUF_RBM_SW1_BM;
}

static inline void
ath12k_wifi6_dp_rx_desc_reset(struct ath12k_dp_wifi6_rx_desc_info *desc)
{
	/* calculate the offset to skip reset of list, cookie and pool id */
	size_t offset = sizeof(desc->list) + sizeof(desc->cookie) + sizeof(desc->pool_id);
	memset((u8 *)desc + offset, 0, sizeof(*desc) - offset);
	desc->unmapped = 1;
}

static inline
void *ath12k_wifi6_dp_get_rx_desc(struct ath12k_dp *dp, u32 cookie)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u16 index = ATH12K_DP_RX_DESC_COOKIE_INDEX_GET(cookie);

	if (ath12k_dp_mon_rxdma1_enable(dp))
		rx_desc_pool = &dp_wifi6->rx_desc_mon;
	else
		rx_desc_pool = &dp_wifi6->rx_desc_buf;

	if (unlikely(index >= rx_desc_pool->pool_size))
		return NULL;

	return &rx_desc_pool->mon_desc_pool[index];
}

static inline
u32 ath12k_wifi6_dp_get_rx_refill_ring_size(struct ath12k_base *ab)
{
	return ab->hal.hal_params->rx_refill_ring_size;
}

static inline
u32 ath12k_wifi6_dp_get_rx_mon_err_dst_ring_size(struct ath12k_base *ab)
{
	return ab->hal.hal_params->rx_mon_err_dst_ring_size;
}

void ath12k_wifi6_dp_rx_ring_free(struct ath12k_base *ab);
void ath12k_wifi6_dp_rx_buf_free(struct ath12k_dp *dp);
int ath12k_wifi6_dp_rx_ring_alloc(struct ath12k_base *ab);
int ath12k_wifi6_dp_rx_ring_init(struct ath12k_base *ab);
int ath12k_wifi6_dp_rxdma_buf_setup(struct ath12k_base *ab);
int ath12k_wifi6_dp_rx_desc_pool_alloc(struct ath12k_dp *dp,
				       struct ath12k_dp_rx_desc_pool *rx_desc_pool,
				       u16 ring_size);
int ath12k_wifi6_rx_desc_pool_init(struct ath12k_dp *dp,
				   struct ath12k_dp_rx_desc_pool *rx_desc_pool,
				   int mac_id, u16 ring_size);
size_t ath12k_wifi6_dp_get_req_entries_from_buf_ring(struct ath12k_dp *dp,
						     struct dp_srng *rx_ring,
						     struct list_head *list,
						     struct ath12k_dp_rx_desc_pool *rx_desc_pool);
size_t ath12k_wifi6_dp_list_cut_nodes(struct list_head *list,
				      struct list_head *head, size_t count);
int ath12k_wifi6_dp_buf_replenish(struct ath12k_dp *dp,
				  struct dp_srng *buf_ring,
				  struct list_head *used_list,
				  int req_entries,
				  struct ath12k_dp_rx_desc_pool *rx_desc_pool);
#endif
