/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_CFG_TX_UDP_ST_H__
#define __EDMA_CFG_TX_UDP_ST_H__

#define EDMA_TX_UDP_ST_RING_SIZE 60
#define EDMA_TX_UDP_ST_OUT_OF_RANGE_IDX (EDMA_TX_UDP_ST_RING_SIZE + 1)
#define NSS_DP_UDP_ST_TX_RING_ID	19	/* TX ring reserved for UDP-ST HW offload */

struct edma_gbl_ctx;

/**
 * UDP-ST EDMA context
 */
struct edma_udp_st_ctx {
	struct edma_txdesc_ring *tx_ring;
	struct edma_txcmpl_ring *tx_cmpl_ring;
	uint16_t vp_port;
	bool initialized;
	spinlock_t lock;
	struct sk_buff *loop_skb;
};

int32_t edma_cfg_tx_udp_st_ring_alloc(struct edma_gbl_ctx *egc);
void edma_cfg_tx_udp_st_mapping(struct edma_gbl_ctx *egc);
void edma_cfg_tx_udp_st_ring_cleanup(struct edma_gbl_ctx *egc);
void edma_cfg_tx_udp_st_ring(struct edma_gbl_ctx *egc);
void edma_cfg_tx_udp_st_ring_enable(struct edma_gbl_ctx *egc);
void edma_cfg_tx_udp_st_ring_disable(struct edma_gbl_ctx *egc);
int nss_dp_udp_st_init(void);
void nss_dp_udp_st_deinit(void);
void nss_dp_udp_st_reset_indices(void);
int nss_dp_udp_st_xmit(struct nss_dp_udp_st_xmit_info *xmit_info);

#endif	/* __EDMA_CFG_TX_UDP_ST_H__ */
