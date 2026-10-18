/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI7_UMAC_RESET_H
#define ATH12K_WIFI7_UMAC_RESET_H

#include "../core.h"
#include "../umac_reset.h"

/* WiFi7-specific UMAC reset wrapper function declarations */
void ath12k_wifi7_umac_reset_handle_pre_reset_wrapper(struct ath12k_base *ab);
void ath12k_wifi7_umac_reset_handle_post_reset_start_wrapper(struct ath12k_base *ab);
void ath12k_wifi7_umac_reset_handle_post_reset_complete_wrapper(struct ath12k_base *ab);

#endif /* ATH12K_WIFI7_UMAC_RESET_H */
