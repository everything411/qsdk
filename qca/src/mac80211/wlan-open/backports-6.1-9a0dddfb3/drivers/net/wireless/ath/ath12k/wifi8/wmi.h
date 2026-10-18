/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WMI_WIFI8_H
#define ATH12K_WMI_WIFI8_H

void ath12k_wifi8_wmi_init_qcn9625(struct ath12k_base *ab,
				   struct ath12k_wmi_resource_config_arg *config);

void ath12k_wifi8_cu_notify(struct ath12k *ar, struct ath12k_link_vif *arvif);

#endif
