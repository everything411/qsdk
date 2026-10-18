/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI7_PPE_H
#define ATH12K_WIFI7_PPE_H

#define HAL_TX_PPE_VP_CFG_WILDCARD_LMAC_ID 3
#define HAL_TX_PPE_VP_CFG_VP_NUM                GENMASK(7, 0)
#define HAL_TX_PPE_VP_CFG_PMAC_ID               GENMASK(9, 8)
#define HAL_TX_PPE_VP_CFG_BANK_ID               GENMASK(15, 10)
#define HAL_TX_PPE_VP_CFG_VDEV_ID               GENMASK(23, 16)
#define HAL_TX_PPE_VP_CFG_SRCH_IDX_REG_NUM      GENMASK(26, 24)
#define HAL_TX_PPE_VP_CFG_USE_PPE_INT_PRI       BIT(27)
#define HAL_TX_PPE_VP_CFG_TO_FW                 BIT(28)
#define HAL_TX_PPE_VP_CFG_DROP_PREC_EN          BIT(29)

#define HAL_TX_PPE_VP_WIFI7_ENTRIES_MAX 32

#define PPE2TCL_RING_WIFI7 0
#define REO2PPE_RING_WIFI7 0
#define PPEDS_ARCH_MODE_WIFI7 7
extern struct ppe_ds_wlan_ops_v2 ppeds_wlanops_v2;
extern struct ath12k_ppeds_arch_ops ath12k_wifi7_arch_ppeds_ops;
int ath12k_ppeds_wifi7_srng_alloc(struct ath12k_base *ab);
void ath12k_ppeds_wifi7_srng_cleanup(struct ath12k_base *ab);
#endif
