/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _MAPC_EXTN_H_
#define _MAPC_EXTN_H_

#ifndef CPTCFG_QCN_EXTN

static inline void
ath12k_mapc_cache_update_extn(struct ath12k_link_sta *arsta,
			      struct ath12k *ar,
			      const struct ath12k_wmi_peer_mapc_params_arg *arg) {}

#else

void ath12k_mapc_cache_update_extn(struct ath12k_link_sta *arsta,
				   struct ath12k *ar,
				   const struct ath12k_wmi_peer_mapc_params_arg *arg);

#endif /* CPTCFG_QCN_EXTN */
#endif /* _MAPC_EXTN_H_ */
