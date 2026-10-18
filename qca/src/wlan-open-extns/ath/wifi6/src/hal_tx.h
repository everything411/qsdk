/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_TX_H
#define ATH12K_HAL_TX_H

#include "../../mac.h"
#include "hal_desc.h"

extern u8 ath12k_default_dscp_tid_map[DSCP_TID_MAP_TBL_ENTRY_SIZE];

/* TODO: Check if the actual desc macros can be used instead */
#define HAL_TX_STATUS_FLAGS_FIRST_MSDU		BIT(0)
#define HAL_TX_STATUS_FLAGS_LAST_MSDU		BIT(1)
#define HAL_TX_STATUS_FLAGS_MSDU_IN_AMSDU	BIT(2)
#define HAL_TX_STATUS_FLAGS_RATE_STATS_VALID	BIT(3)
#define HAL_TX_STATUS_FLAGS_RATE_LDPC		BIT(4)
#define HAL_TX_STATUS_FLAGS_RATE_STBC		BIT(5)
#define HAL_TX_STATUS_FLAGS_OFDMA		BIT(6)
/* STA mode will have MCAST_PKT_CTRL instead of DSCP_TID_MAP bitfield */
#define HAL_TX_BANK_CONFIG_DSCP_TIP_MAP_ID	GENMASK(22, 17)

#define HAL_TX_STATUS_DESC_LEN		sizeof(struct hal_wbm_release_ring)
#endif
