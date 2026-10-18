/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_CFG_TX_LOOPBACK_H__
#define __EDMA_CFG_TX_LOOPBACK_H__

void edma_cfg_tx_loopback_rings(struct edma_gbl_ctx *egc);
int32_t edma_cfg_tx_loopback_rings_alloc(struct edma_gbl_ctx *egc);
void edma_cfg_tx_loopback_rings_cleanup(struct edma_gbl_ctx *egc);
void edma_cfg_tx_loopback_mapping(struct edma_gbl_ctx *egc);
void edma_cfg_tx_loopback_rings_enable(struct edma_gbl_ctx *egc);
void edma_cfg_tx_loopback_rings_disable(struct edma_gbl_ctx *egc);
#endif	/* __EDMA_CFG_TX_LOOPBACK_H__ */
