/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_CFG_TX_H__
#define __EDMA_CFG_TX_H__

#define EDMA_TX_NAPI_WORK_MIN   16
#define EDMA_TX_NAPI_WORK_MAX	512

#define EDMA_TX_MITIGATION_TIMER_MIN	0	/* Tx mitigation timer's minimum value in microseconds */
#define EDMA_TX_MITIGATION_TIMER_MAX	1000	/* Tx mitigation timer's maximum value in microseconds */
#define EDMA_TX_MITIGATION_PKT_CNT_MIN	0	/* Tx mitigation packet count's minimum value */
#define EDMA_TX_MITIGATION_PKT_CNT_MAX	256	/* Tx mitigation packet count's maximum value */

void edma_cfg_tx_rings(struct edma_gbl_ctx *egc);
int32_t edma_cfg_tx_rings_alloc(struct edma_gbl_ctx *egc);
void edma_cfg_tx_rings_cleanup(struct edma_gbl_ctx *egc);
void edma_cfg_tx_napi_enable(struct edma_gbl_ctx *egc);
void edma_cfg_tx_napi_disable(struct edma_gbl_ctx *egc);
void edma_cfg_tx_napi_delete(struct edma_gbl_ctx *egc);
void edma_cfg_tx_napi_add(struct edma_gbl_ctx *egc, struct net_device *netdev, uint32_t macid);
void edma_cfg_tx_mapping(struct edma_gbl_ctx *egc);
void edma_cfg_tx_map_tx_ring_to_txcmpl(uint32_t tx_ring_id, uint32_t txcmpl_ring_id);
void edma_cfg_tx_cmpl_mapping_fill(struct edma_gbl_ctx *egc);
void edma_cfg_tx_rings_enable(struct edma_gbl_ctx *egc);
void edma_cfg_tx_rings_disable(struct edma_gbl_ctx *egc);
void edma_cfg_tx_ring_reset(struct edma_txdesc_ring *ring);
void edma_cfg_tx_fill_per_port_tx_map(struct net_device *netdev, uint32_t macid);
void edma_cfg_txcmpl_ring_reset(struct edma_txcmpl_ring *ring);
#ifdef NSS_DP_MHT_SW_PORT_MAP
void edma_cfg_tx_set_mht_mdio_slv_pause(struct edma_gbl_ctx *egc);
void edma_cfg_tx_set_max_ports(struct edma_gbl_ctx *egc);
#endif
#if defined(NSS_DP_POINT_OFFLOAD)
void edma_cfg_tx_point_offload_mapping(struct edma_gbl_ctx *egc);
#endif
int edma_cfg_tx_cmpl_ring_setup(struct edma_txcmpl_ring *txcmpl_ring);
int edma_cfg_tx_desc_ring_setup(struct edma_txdesc_ring *txdesc_ring);

#endif	/* __EDMA_CFG_TX_H__ */
