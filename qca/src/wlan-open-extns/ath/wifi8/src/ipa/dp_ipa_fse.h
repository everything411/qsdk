// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_WIFI8_DP_IPA_FSE_H__
#define __ATH12K_WIFI8_DP_IPA_FSE_H__

#include <linux/ipa_fse.h>

int ath12k_dp_rx_ipa_fse_add_flow_entry(struct ipa_fse_rule *rule);
int ath12k_dp_rx_ipa_fse_del_flow_entry(struct ipa_fse_rule *rule);

#endif /* __ATH12K_WIFI8_DP_IPA_FSE_H__ */
