/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_WIFI8_HW_H
#define ATH12K_WIFI8_HW_H

#include <linux/threads.h>

#define ATH12K_EXT_IRQ_GRP_NUM_MAX 18
#define QCN9625_QFPROM_RAW_FEATURE_CONFIG_ROW4_LSB 0x1F90070

#define ATH12K_TX_RING_MASK_0 0x1
#define ATH12K_TX_RING_MASK_1 0x2
#define ATH12K_TX_RING_MASK_2 0x4
#define ATH12K_TX_RING_MASK_3 0x8
#if NR_CPUS > 4
#define ATH12K_TX_RING_MASK_4 0x10
#else
#define ATH12K_TX_RING_MASK_4 0
#endif

#define ATH12K_RX_RING_MASK_0 0x1
#define ATH12K_RX_RING_MASK_1 0x2
#define ATH12K_RX_RING_MASK_2 0x4
#define ATH12K_RX_RING_MASK_3 0x8
#if NR_CPUS > 4
#define ATH12K_RX_RING_MASK_4 0x10
#else
#define ATH12K_RX_RING_MASK_4 0
#endif

#define ATH12K_RX_ERR_RING_MASK_0 0x1

#define ATH12K_RX_WBM_REL_RING_MASK_0 0x1

#define ATH12K_REO_STATUS_RING_MASK_0 0x1

#define	ATH12K_HOST2RXMON_RING_MASK_0	0x1

#define ATH12K_RX_MON_RING_MASK_0 0x1
#define ATH12K_RX_MON_RING_MASK_1 0x2
#define ATH12K_RX_MON_RING_MASK_2 0x4

#define ATH12K_TX_MON_RING_MASK_0 0x1
#define ATH12K_HOST2TX_MON_RING_MASK_0 0x1
#define ATH12K_TX_MON_RING_MASK_1 0x2
#define ATH12K_UMAC_RESET_INTR_MASK_0   0x1
#define ATH12K_UMCMN_INTR_MASK_0        0x1

#define ATH12K_TX_EXCEPTION_RING_MASK_0 0x1
#define ATH12K_TCL_STATUS_RING_MASK_0 0x1
#define ATH12K_TQM_STATUS_RING_MASK_0 0x1
#define ATH12K_ASE_STATUS_RING_MASK_0 0x1

#define ATH12K_PPE2TCL_RING_MASK_0 0x1
#define ATH12K_REO2PPE_RING_MASK_0 0x1
#define ATH12K_PPE_TQM2SW_RELEASE_RING_MASK_0 0x1

#define ATH12K_SAM_STATUS_RING_MASK_0   0x1

#define ATH12K_TX_PEER_TELEMETRY_RING_MASK 0x1
#define ATH12K_RX_PEER_TELEMETRY_RING_MASK 0x1

#define ATH12K_REO_FLUSH_RING_MASK_0   0x1

/* Dedicated interrupt group for fw_tqm2sw ring (FW-owned buffer WAR) */
#define ATH12K_FW_TQM2SW_RING_MASK_0   0x1

/*
 * Per-group CPU affinity for the wifi8 extended IRQ groups.
 *
 * Group 14 is required: the fw_tqm2sw WAR ring (TQM2SW7) fires on a
 * dedicated MSI vector (HAL_TQM_HOST_STATUS_RING=1), separate from the
 * regular tqm_status ring.  It must have its own interrupt group so that
 * ath12k_wifi8_dp_fw_tqm2sw_handler() is only invoked when that specific
 * ring has completions, rather than on every group-9 (tqm_status) fire.
 */
static const int ath12k_wifi8_ext_irq_grp_affinity[] = {
	0, /* grp0  -> cpu0 */
	1, /* grp1  -> cpu1 */
	2, /* grp2  -> cpu2 */
	3, /* grp3  -> cpu3 */
	0, /* grp4  -> cpu0 */
	1, /* grp5  -> cpu1 */
	2, /* grp6  -> cpu2 */
	3, /* grp7  -> cpu3 */
	0, /* grp8  -> cpu0 */
	1, /* grp9  -> cpu1 */
	0, /* grp10 -> cpu0 */
	1, /* grp11 -> cpu1 */
	2, /* grp12 -> cpu2 */
	0, /* grp13 -> cpu0 (roaming RX ring) */
	1, /* grp14 -> cpu1 (fw_tqm2sw WAR ring - dedicated MSI, see above) */
};

struct ath12k_base;
int ath12k_wifi8_hw_init(struct ath12k_base *ab);

struct ath12k_cp_arch_ops;
extern const struct ath12k_cp_arch_ops ath12k_wifi8_cp_ops;

#endif /* ATH12K_WIFI8_HW_H */
