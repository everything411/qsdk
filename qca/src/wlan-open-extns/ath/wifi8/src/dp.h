/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_WIFI8_EXTNS_DP_H__
#define __ATH12K_WIFI8_EXTNS_DP_H__

#include "../../core.h"

#ifdef CPTCFG_EXT_IPA_OFFLOAD
#include "ipa/dp_ipa.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD */

struct ath12k_dp_wifi8_extn {
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	struct ath12k_dp_wifi8_ipa *ipa;
#endif /* CPTCFG_EXT_IPA_OFFLOAD */
};

#endif /* __ATH12K_WIFI8_EXTNS_DP_H__ */
