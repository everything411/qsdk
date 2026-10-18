/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_DP_VP__
#define __EDMA_DP_VP__

#include "nss_dp_dev.h"

netdev_tx_t edma_dp_vp_xmit(struct nss_dp_data_plane_ctx *dpc, struct nss_dp_vp_tx_info *dptxi,
					struct sk_buff *skb);

#endif	/* __EDMA_DP_VP__ */
