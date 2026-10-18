/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_WIFI6_H
#define ATH12K_DP_WIFI6_H

#include "../../dp_cmn.h"
#include "hw.h"

#define HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX_WIFI6     48

struct ath12k_base;
struct ath12k_dp;

struct ath12k_dp_wifi6_rx_desc_info {
	struct list_head list;
	u32 cookie;
	u8 pool_id;
	dma_addr_t paddr;
	struct sk_buff *skb;
	u8 *vaddr;
	u32 magic;
	u8 in_use : 1,
	   unmapped : 1,
	   reserved : 6;
	/* Add new fields here - new parameters should be added at the end
	 * as the reset function (ath12k_wifi6_dp_rx_desc_reset) preserves
	 * the 'list' 'cookie' and 'pool id' field and resets everything else
	 */
};

struct ath12k_dp_rx_desc_pool {
	struct ath12k_dp_wifi6_rx_desc_info *mon_desc_pool;
	struct list_head rx_desc_free_list;
	spinlock_t rx_mon_desc_lock;
	bool dest_frag_enable;
	struct page_frag_cache rx_mon_pf_cache;
	u32 pool_size;
	u8 owner;
	u16 buf_size;
	u8 buf_alignment;
};

#define MAX_RXDMA_PER_PDEV     2

struct ath12k_dp_wifi6 {
	struct ath12k_dp *dp;
	struct ath12k_dp_rx_desc_pool rx_desc_buf;
	struct ath12k_dp_rx_desc_pool rx_desc_mon;
	struct ath12k_dp_rx_desc_pool rx_desc_status[MAX_RXDMA_PER_PDEV];
};

static inline struct ath12k_dp_wifi6 *ath12k_get_dp_wifi6(struct ath12k_dp *dp)
{
	return (struct ath12k_dp_wifi6 *)dp->arch_data;
}

static inline struct ath12k_dp *ath12k_get_dp(struct ath12k_dp_wifi6 *dp_wifi6)
{
	return dp_wifi6->dp;
}

struct ath12k_dp *ath12k_wifi6_dp_init(struct ath12k_base *ab);
void ath12k_wifi6_dp_deinit(struct ath12k_dp *dp);
int ath12k_wifi6_dp_pdev_alloc(struct ath12k_base *ab);
void ath12k_wifi6_dp_pdev_free(struct ath12k_base *ab);
int ath12k_wifi6_dp_peer_create(struct ath12k_hw *ah, u8 *addr,
				struct ath12k_dp_peer_create_params *params,
				struct ieee80211_vif *vif);
void ath12k_wifi6_dp_peer_delete(struct ath12k_dp *dp, struct ath12k_hw *ah, u8 *addr,
				 struct ieee80211_sta *sta, u8 hw_link_id,
				 struct ieee80211_vif *vif);
int ath12k_wifi6_dp_peer_assoc(struct ath12k_dp *dp, struct ath12k_dp_hw *dp_hw,
			       struct ath12k_dp_vif *dp_vif, u8 *addr,
			       struct ath12k_dp_peer_create_params *params);
#endif
