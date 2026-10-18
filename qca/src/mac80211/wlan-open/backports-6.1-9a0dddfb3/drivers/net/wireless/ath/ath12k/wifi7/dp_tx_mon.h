/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_DP_TX_MON1_H
#define ATH12K_DP_TX_MON1_H

#include <linux/workqueue.h>
#include <linux/interrupt.h>

struct workqueue_struct;

/**
 * enum ath12k_dp_tx_ext_mon_peer_filter - TX ext-monitor peer-filter mode
 *
 * Selects how TX frames are peer-scoped in the ext-monitor capture path.
 * The active mode is derived from the (fp_enabled, fpmo_enabled) bits of
 * the current TX ext-monitor configuration, or overridden directly when
 * special packet capture (ATH12K_EXT_MON_PKT_CAP) is requested.
 *
 * @ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER: peer scoping is delegated to the
 *   HW/FW peer-filter table (fpmo only).  The host still checks selfgen
 *   and leaked frames through its own peer_list.
 * @ATH12K_DP_TX_EXT_MON_ALL_PEER_FILTER: blanket capture; every peer is
 *   accepted and only the frame-type subtype filter (fp) is applied.
 * @ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER: peer scoping is enforced entirely
 *   in software via peer_list (fp and fpmo both set).
 *   A matching peer passes unconditionally (return 0);
 *   non-matching peers proceed to the fp type check.
 * @ATH12K_DP_TX_EXT_MON_SPC_PEER_FILTER: special packet capture mode
 *   (monitor_flags == ATH12K_EXT_MON_PKT_CAP).  Peer scoping stays in the
 *   host filter path via peer_list; HW peer-filter programming is skipped.
 *   A matching peer passes unconditionally (return 0); a non-matching peer
 *   is passed to the frame-type filter (fp).
 * @ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER_MAX: sentinel / array-size guard.
 */
enum ath12k_dp_tx_ext_mon_peer_filter {
	ATH12K_DP_TX_EXT_MON_HW_PEER_FILTER =	1,
	ATH12K_DP_TX_EXT_MON_ALL_PEER_FILTER,
	ATH12K_DP_TX_EXT_MON_SW_PEER_FILTER,
	ATH12K_DP_TX_EXT_MON_SPC_PEER_FILTER,
	ATH12K_DP_TX_EXT_MON_PEER_FILTER_MAX,
};

u8 ath12k_wifi7_get_ext_mon_peer_filter_mode_locked(struct ath12k_pdev_dp *dp_pdev);
int ath12k_wifi7_dp_ext_mon_filter(struct ath12k_pdev_dp *dp_pdev, struct sk_buff *mpdu);
int ath12k_wifi7_dp_mon_tx_config_filter(struct ath12k_pdev_dp *dp_pdev,
					 bool enable);
int
ath12k_wifi7_dp_ext_mon_add_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					 struct ath12k_dp_ext_mon_tx_peer_params
					 *peer_param);
int
ath12k_wifi7_dp_ext_mon_remove_wmi_tx_peers(struct ath12k_pdev_dp *dp_pdev,
					    struct ath12k_dp_ext_mon_tx_peer_params
					    *peer_param);
u32
ath12k_wifi7_dp_mon_tx_get_spc_bitmap(struct ath12k_base *ab);
#endif
