/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_DP_ARCH_H__
#define __NSS_DP_ARCH_H__

#define NSS_DP_HAL_MAX_PORTS		5
#define NSS_DP_MAX_PORTS		NSS_DP_HAL_MAX_PORTS
#define NSS_DP_HAL_CPU_NUM		4
#define NSS_DP_HAL_START_IFNUM		1
#define NSS_DP_PREHEADER_SIZE		32

/*
 * Max FC Groups (VQs) supported for this SOC
 */
#define NSS_DP_HW_MAX_FC_GRP		8

/*
 * Maximum supported GSO segments
 */
#define NSS_DP_HAL_GSO_MAX_SEGS		GSO_MAX_SEGS

/*
 * Number of TX/RX queue supported
 */
#define NSS_DP_QUEUE_NUM		4

/*
 * TX/RX NAPI budget
 */
#define NSS_DP_HAL_RX_NAPI_BUDGET	32
#define NSS_DP_HAL_TX_NAPI_BUDGET	32
#define NSS_DP_HAL_RXFILL_NAPI_BUDGET	0

/**
 * nss_dp_hal_gmac_stats
 *	The per-GMAC statistics structure.
 */
struct nss_dp_hal_gmac_stats {
};

/**
 * nss_dp_hal_nsm_sawf_sc_stats
 *	Per-service code stats to be send to NSM.
 */
struct nss_dp_hal_nsm_sawf_sc_stats {
};

extern int edma_init(void);
extern void edma_cleanup(bool is_dp_override);
extern struct nss_dp_data_plane_ops nss_dp_edma_ops;
extern bool nss_dp_hal_nsm_sawf_sc_stats_read(struct nss_dp_hal_nsm_sawf_sc_stats *nsm_stats, uint8_t service_class);

#endif /* __NSS_DP_ARCH_H__ */
