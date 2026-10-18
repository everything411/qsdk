/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_DP_HAL_INFO_H__
#define __NSS_DP_HAL_INFO_H__

#include <edma.h>

/*
 * nss_dp_hal_info
 *	Data plane specific information wrapper
 */
struct nss_dp_hal_info {
	struct edma_txdesc_ring *txr_map[NR_CPUS][EDMA_MAX_TX_RINGS_PER_CORE];
				/* Per CPU Tx descriptor ring map */
#ifdef NSS_DP_MHT_SW_PORT_MAP
	struct edma_txdesc_ring *txr_sw_port_map[NSS_DP_HAL_SW_MAX_TX_PORT][NR_CPUS];
				/* Per CPU Software ports Tx descriptor ring map */
#endif
	struct edma_pcpu_stats pcpu_stats;
				/* Per CPU netdev statistics */
};

#endif	/* __NSS_DP_HAL_INFO_H__ */
