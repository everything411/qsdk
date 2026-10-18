/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "../../dp_mon.h"
#include "dp.h"
#include "hal_rx.h"

extern const struct ath12k_dp_arch_mon_ops ath12k_wifi6_dp_arch_mon_quad_ring_ops;
extern const struct ath12k_dp_arch_mon_ops ath12k_wifi6_dp_arch_mon_dual_ring_ops;

static inline
void ath12k_wifi6_dp_mon_ops_register(struct ath12k_dp *dp)
{
	struct ath12k_dp_mon *dp_mon = dp->dp_mon;

	dp_mon->mon_ops = &ath12k_wifi6_dp_arch_mon_quad_ring_ops;
}

static inline
struct ath12k_dp_rx_desc_pool *ath12k_wifi6_dp_rx_get_mon_desc_pool(struct ath12k_dp *dp)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);

	if (!ath12k_dp_mon_rxdma1_enable(dp))
		return &dp_wifi6->rx_desc_buf;
	else
		return &dp_wifi6->rx_desc_mon;
}

static inline
int ath12k_dp_rx_mon_process_ring(struct ath12k_dp *dp, int mac_id,
				  struct napi_struct *napi, int budget)
{
	struct ath12k_pdev_dp *dp_pdev;
	const struct ath12k_dp_arch_mon_ops *mon_ops;
	u8 pdev_id = ath12k_hw_mac_id_to_pdev_id(dp->hw_params, mac_id);
	int num_buffs_reaped = 0;

	mon_ops = ath12k_dp_mon_ops_get(dp);
	rcu_read_lock();

	dp_pdev = ath12k_dp_to_dp_pdev(dp, pdev_id);
	if (unlikely(!dp_pdev || !dp_pdev->dp_mon_pdev)) {
		rcu_read_unlock();
		return 0;
	}

	if (mon_ops && mon_ops->mon_rx_srng_process) {
		num_buffs_reaped =
			mon_ops->mon_rx_srng_process(dp_pdev, mac_id,
							napi, &budget);
	}

	rcu_read_unlock();

	return num_buffs_reaped;
}

#define ATH12K_DP_RX_DESC_COOKIE_INDEX    GENMASK(17, 0)
#define ATH12K_DP_RX_DESC_COOKIE_POOL_ID  GENMASK(22, 18)

static inline
struct ath12k_dp_rx_desc_pool *
ath12k_wifi6_dp_get_rx_desc_pool(struct ath12k_dp *dp, int mac_id)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);

	if (unlikely(!dp_wifi6))
		return NULL;

	if (unlikely(mac_id < 0 || mac_id >= MAX_RXDMA_PER_PDEV))
		return NULL;

	return &dp_wifi6->rx_desc_status[mac_id];
}

static inline
struct ath12k_dp_wifi6_rx_desc_info *
ath12k_wifi6_dp_get_rx_status_desc(struct ath12k_dp *dp, u32 cookie)
{
	struct ath12k_dp_wifi6 *dp_wifi6 = ath12k_get_dp_wifi6(dp);
	struct ath12k_dp_rx_desc_pool *rx_desc_pool;
	u32 index, pool_id;

	if (unlikely(!dp_wifi6))
		return NULL;

	pool_id = u32_get_bits(cookie, ATH12K_DP_RX_DESC_COOKIE_POOL_ID);
	index   = u32_get_bits(cookie, ATH12K_DP_RX_DESC_COOKIE_INDEX);

	if (unlikely(pool_id >= MAX_RXDMA_PER_PDEV))
		return NULL;

	rx_desc_pool = &dp_wifi6->rx_desc_status[pool_id];

	if (unlikely(index >= rx_desc_pool->pool_size))
		return NULL;

	if (unlikely(!rx_desc_pool->mon_desc_pool))
		return NULL;

	return &rx_desc_pool->mon_desc_pool[index];
}

static inline
u32 ath12k_wifi6_dp_rxdma_get_mon_dst_ring(struct ath12k_pdev_dp *dp_pdev, int mac_id)
{
	struct ath12k_dp *dp = dp_pdev->dp;

	if (unlikely(mac_id < 0 || mac_id >= MAX_RXDMA_PER_PDEV))
		return 0;

	if (ath12k_dp_mon_rxdma1_enable(dp))
		return dp_pdev->dp_mon_pdev->rxdma_mon_dst_ring[mac_id].ring_id;
	else
		return dp->rxdma_err_dst_ring[mac_id].ring_id;
}

static inline
int ath12k_wifi6_dp_rx_mon_alloc_parent_buf(struct sk_buff **head_msdu)
{
	u8 align = 4;
	unsigned long offset;

	*head_msdu = dev_alloc_skb(ATH12K_DP_MON_MAX_RADIO_TAP_HDR + align);

	if (!(*head_msdu))
		return -ENOMEM;

	skb_reserve(*head_msdu, ATH12K_DP_MON_MAX_RADIO_TAP_HDR);
	offset = ALIGN((unsigned long)(*head_msdu)->data, align) -
				(unsigned long)(*head_msdu)->data;
	/* Reserve extra bytes to align skb->data */
	skb_reserve(*head_msdu, offset);

	/* Set *head_msdu->next as NULL as all msdus are mapped via nr frags */
	(*head_msdu)->next = NULL;

	return 0;
}

static inline
void *ath12k_wifi6_dp_rx_cookie_to_mon_link_desc(struct ath12k_pdev_dp *dp_pdev,
						 struct hal_rx_buf_info *buf_info,
						 struct ath12k_mon_data *pmon)
{
	void *link_desc_va = NULL;
	struct ath12k_dp *dp = dp_pdev->dp;
	u32 desc_bank = 0;

	desc_bank = u32_get_bits(buf_info->sw_cookie, DP_LINK_DESC_BANK_MASK);
	if (unlikely(desc_bank >= DP_LINK_DESC_BANKS_MAX))
		return NULL;

	if (ath12k_dp_mon_rxdma1_enable(dp)) {
		link_desc_va =
			pmon->link_desc_banks[desc_bank].vaddr +
			(buf_info->paddr - pmon->link_desc_banks[desc_bank].paddr);
	} else {
		link_desc_va =
			dp->link_desc_banks[desc_bank].vaddr +
			(buf_info->paddr - dp->link_desc_banks[desc_bank].paddr);
	}

	return link_desc_va;
}

static inline
struct dp_srng* ath12k_dp_rxdma_get_mon_buf_ring(struct ath12k_dp *dp)
{
	if (!ath12k_dp_mon_rxdma1_enable(dp)) {
		struct dp_rxdma_ring *rx_ring = &dp->rx_refill_buf_ring;
		return &rx_ring->refill_buf_ring;
	} else {
		struct dp_rxdma_mon_ring *rx_ring = &dp->dp_mon->rxdma_mon_buf_ring;
		return &rx_ring->refill_buf_ring;
	}
}

static inline
u32 ath12k_wifi6_dp_get_rx_mon_refill_ring_size(struct ath12k_base *ab)
{
	return ab->hal.hal_params->rx_mon_refill_ring_size;
}

int ath12k_wifi6_dp_mon_rx_srng_setup(struct ath12k_dp *dp);
void ath12k_wifi6_dp_mon_rx_srng_cleanup(struct ath12k_dp *dp);
int ath12k_wifi6_dp_mon_rx_ring_init(struct ath12k_dp *dp);
void ath12k_wifi6_dp_mon_rx_ring_deinit(struct ath12k_dp *dp);
int ath12k_wifi6_dp_mon_rx_htt_srng_setup(struct ath12k_dp *dp);
int ath12k_wifi6_dp_mon_pdev_rx_srng_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
void ath12k_wifi6_dp_mon_pdev_rx_srng_cleanup(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi6_dp_mon_pdev_rx_htt_srng_setup(struct ath12k_pdev_dp *dp_pdev, u32 mac_id);
int ath12k_wifi6_mon_setup_mon_link_desc(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi6_mon_cleanup_mon_link_desc(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi6_dp_mon_rx_quad_ring_process(struct ath12k_pdev_dp *pdev_dp,
					     int mac_id, struct napi_struct *napi,
					     int *budget);
void ath12k_wifi6_dp_mon_rx_monitor_mode_set(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi6_dp_mon_rx_monitor_mode_reset(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi6_dp_mon_rx_update_ring_filter(struct ath12k_pdev_dp *pdev_dp);
void ath12k_wifi6_dp_mon_rx_mon_mode_config_filter(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi6_dp_mon_rx_mon_mode_reset_filter(struct ath12k_pdev_dp *dp_pdev);
/* HTT FP_PHY_ERR_BUF_SRC: bits [14:13] of info2 */
#define ATH12K_WIFI6_FP_PHY_ERR_BUF_SRC_SW2RXDMA	2
#define ATH12K_WIFI6_FP_PHY_ERR_BUF_SRC_NONE		3

/* HTT FP_PHY_ERR_BUF_DEST: bits [16:15] of info2 */
#define ATH12K_WIFI6_FP_PHY_ERR_BUF_DEST_RXDMA2SW	2
#define ATH12K_WIFI6_FP_PHY_ERR_BUF_DEST_RELEASING	0

void ath12k_wifi6_dp_mon_rx_undecoded_metadata_config_filter(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi6_dp_mon_rx_undecoded_metadata_reset_filter(struct ath12k_pdev_dp *dp_pdev);
void ath12k_wifi6_dp_mon_rx_htt_phy_err_filter_cfg(void *ptr,
						    struct htt_rx_ring_tlv_filter *tlv_filter);

