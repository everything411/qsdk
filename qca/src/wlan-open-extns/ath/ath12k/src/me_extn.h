/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __ATH12K_ME_EXTN_H
#define __ATH12K_ME_EXTN_H

#include "me.h"
#include "dp.h"
#include "core.h"

#ifndef CPTCFG_QCN_EXTN

int ath12k_me_info_print_extn(struct ath12k_me_db *me_db,
                        struct ath12k_dp_vif *dp_vif)
{
	return 0;
}

#else

int ath12k_me_info_print_extn(struct ath12k_me_db *me_db,
                        struct ath12k_dp_vif *dp_vif);

#endif /* CPTCFG_QCN_EXTN */
#endif /* __ATH12K_ME_EXTN_H */
