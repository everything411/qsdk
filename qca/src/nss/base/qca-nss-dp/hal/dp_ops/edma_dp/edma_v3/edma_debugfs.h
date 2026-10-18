/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_DEBUGFS_H__
#define __EDMA_DEBUGFS_H__

#define EDMA_STATS_BANNER_MAX_LEN	80
#define EDMA_RX_RING_STATS_NODE_NAME	"EDMA_RX"
#define EDMA_TX_RING_STATS_NODE_NAME	"EDMA_TX"
#define EDMA_MISC_STATS_NODE_NAME	"EDMA_MISC"
#define EDMA_RX_RING_PPEDS_STATS_NODE_NAME	"EDMA_RX_PPEDS"
#define EDMA_TX_RING_PPEDS_STATS_NODE_NAME	"EDMA_TX_PPEDS"

int edma_debugfs_init(void);
void edma_debugfs_exit(void);

#endif	// __EDMA_DEBUGFS_H__
