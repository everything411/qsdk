/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI6_HW_H
#define ATH12K_WIFI6_HW_H

#define ATH12K_EXT_IRQ_GRP_NUM_MAX 12
#define QCN9274_QFPROM_RAW_RFA_PDET_ROW13_LSB 0x1E20338

struct ath12k_base;
int ath12k_wifi6_hw_init(struct ath12k_base *ab);

#endif /* ATH12K_WIFI6_HW_H */
