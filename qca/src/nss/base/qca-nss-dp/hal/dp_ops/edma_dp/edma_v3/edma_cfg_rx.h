/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_CFG_RX_H__
#define __EDMA_CFG_RX_H__

#define EDMA_RX_NAPI_WORK_MIN		16
#define EDMA_RX_NAPI_WORK_MAX		512
#define EDMA_RXFILL_NAPI_WORK_MIN	(NSS_DP_RX_FC_XON_DEF - NSS_DP_RX_FC_XOFF_DEF)
#define EDMA_RXFILL_NAPI_WORK_MAX	EDMA_RX_RING_SIZE
#define EDMA_RXFILL_UGT_THRESHOLD	NSS_DP_RX_FC_XON_DEF
#define EDMA_RX_PAGE_MODE_SKB_SIZE	256	/* SKB payload size used in page mode */
#define EDMA_RX_DEFAULT_QUEUE_PRI	0
#define EDMA_RX_DEFAULT_BITMAP		((1 << NR_CPUS) - 1)	/* Bitmap when using 4 cores */
#define EDMA_RX_FC_ENABLE		1	/* RX flow control default state */
#define EDMA_RX_QUEUE_TAIL_DROP_ENABLE	0	/* RX queue tail drop configuration default state */
#define EDMA_RX_FC_XOFF_THRE_MIN	0	/* Rx flow control minimum X-OFF value */
#define EDMA_RX_FC_XON_THRE_MIN		0	/* Rx flow control mininum X-ON value */
#define EDMA_RX_AC_FC_THRE_ORIG		0x190	/* Rx AC flow control original threshold */
#define EDMA_RX_AC_FC_THRE_MIN		0	/* Rx AC flow control minimum threshold */
#define EDMA_RX_AC_FC_THRE_MAX		0x7ff	/* Rx AC flow control maximum threshold.
						   AC FC threshold value is 11 bits long */

#define EDMA_RX_MITIGATION_TIMER_MIN	0	/* Rx mitigation timer's minimum value in microseconds */
#define EDMA_RX_MITIGATION_TIMER_MAX	1000	/* Rx mitigation timer's maximum value in microseconds */
#define EDMA_RX_MITIGATION_PKT_CNT_MIN	0	/* Rx mitigation packet count's minimum value */
#define EDMA_RX_MITIGATION_PKT_CNT_MAX	256	/* Rx mitigation packet count's maximum value */

#define EDMA_RXFILL_ONE_INTR_ATTEMPT_MAX	8 /* Max refill attempt in a single interrupt */
#define EDMA_RXFILL_INTR_ATTEMPT_MAX		8 /* Max refill attempt through subsequent interrupts */
#define EDMA_RXFILL_DELAY_INTR_MS		500 /* Time in milisecond for delayed interrupt */

#if defined(NSS_DP_POINT_OFFLOAD)
/* TODO: we need to close with ssdk team to close this numbers */
#define EDMA_RX_POINT_OFFLOAD_QUEUE_BASE 56
#define EDMA_RX_POINT_OFFLOAD_QUEUE_NUM 3
#endif

extern uint32_t edma_cfg_rx_fc_enable;
extern uint32_t edma_cfg_rx_queue_tail_drop_enable;
extern uint32_t edma_cfg_rx_rps_num_cores;
extern uint32_t edma_cfg_rx_sec_desc_inval;
extern uint32_t edma_cfg_rx_rps_bitmap_cores;

int edma_rx_gro_slot_vld_configure(struct edma_gbl_ctx *egc);
int edma_rx_gro_desc_count_configure(struct edma_hw_gro_ctx *gro_ctx);
int edma_rx_gro_timeout_configure(struct edma_hw_gro_ctx *gro_ctx);
int edma_rx_gro_buffer_len_configure(struct edma_hw_gro_ctx *gro_ctx);

void edma_cfg_rx_fill_ring_configure(struct edma_rxfill_ring *rxfill_ring);
void edma_cfg_rx_fill_ring_cleanup(struct edma_gbl_ctx *egc,
                                struct edma_rxfill_ring *rxfill_ring);
int edma_cfg_rx_desc_ring_setup(struct edma_rxdesc_ring *rxdesc_ring);
void edma_cfg_rx_desc_ring_cleanup(struct edma_gbl_ctx *egc,
                                struct edma_rxdesc_ring *rxdesc_ring);

void edma_cfg_rx_rings(struct edma_gbl_ctx *egc);
#if defined(NSS_DP_POINT_OFFLOAD)
void edma_cfg_rx_point_offload_mapping(struct edma_gbl_ctx *egc);
void edma_cfg_rx_point_offload_rings(struct edma_gbl_ctx *egc);
#endif
int32_t edma_cfg_rx_rings_alloc(struct edma_gbl_ctx *egc);
void edma_cfg_rx_rings_cleanup(struct edma_gbl_ctx *egc);
void edma_cfg_rx_napi_disable(struct edma_gbl_ctx *egc);
void edma_cfg_rx_napi_enable(struct edma_gbl_ctx *egc);
void edma_cfg_rx_napi_delete(struct edma_gbl_ctx *egc);
void edma_cfg_rx_napi_add(struct edma_gbl_ctx *egc, struct net_device *netdev);
void edma_cfg_rx_mapping(struct edma_gbl_ctx *egc);
void edma_cfg_rx_mcast_qid_to_core_mapping(struct edma_gbl_ctx *egc, uint8_t core_id);
void edma_cfg_rx_rings_enable(struct edma_gbl_ctx *egc);
void edma_cfg_rx_rings_disable(struct edma_gbl_ctx *egc);
void edma_cfg_rx_ring_reset(struct edma_rxdesc_ring *ring);
bool edma_cfg_rx_ring_en_mapped_queues(struct edma_gbl_ctx *egc, uint32_t queue_id, uint32_t max_queues, bool enable);
bool edma_cfg_rx_ring_enq_en_mapped_queues(uint32_t queue_id, uint32_t max_queues, bool enable);
int edma_cfg_rx_fc_enable_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos);
void edma_cfg_rx_page_mode_and_jumbo(struct edma_gbl_ctx *egc);
void edma_cfg_rx_qid_to_rid_mapping(uint32_t queue_base, uint32_t max_q, uint32_t ring_id);
void edma_cfg_rx_desc_ring_to_queue_mapping_conf(struct edma_rxdesc_ring *rxdesc_ring, uint32_t enable);
int edma_cfg_rxdesc_to_rxfill_mapping(struct edma_gbl_ctx *egc, uint32_t rxfill_ring_id, uint32_t rx_ring_id);
int edma_cfg_rx_queue_tail_drop_handler(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos);
int edma_cfg_rx_rps(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos);
int edma_cfg_rx_rps_bitmap(struct ctl_table *table, int write,
		void __user *buffer, size_t *lenp, loff_t *ppos);
void edma_cfg_rxfill_ring_reset(struct edma_rxfill_ring *ring);
#endif	/* __EDMA_CFG_RX_H__ */
