// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/bitfield.h>

#include "../core.h"
#include "../ce.h"
#include "ce.h"
#include "../dp_rx.h"
#include "../dp_htt.h"

/* Target firmware's Copy Engine configuration. */
const struct ce_pipe_config ath12k_wifi8_target_ce_config_wlan_qcn9625[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.pipenum = __cpu_to_le32(0),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE1: target->host HTT + HTC control */
	{
		.pipenum = __cpu_to_le32(1),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE2: target->host WMI */
	{
		.pipenum = __cpu_to_le32(2),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE3: host->target WMI (mac0) */
	{
		.pipenum = __cpu_to_le32(3),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE4: host->target HTT */
	{
		.pipenum = __cpu_to_le32(4),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(256),
		.nbytes_max = __cpu_to_le32(256),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},

	/* CE5: target->host Pktlog */
	{
		.pipenum = __cpu_to_le32(5),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE6: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = __cpu_to_le32(6),
		.pipedir = __cpu_to_le32(PIPEDIR_INOUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(16384),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE7: host->target WMI (mac1) */
	{
		.pipenum = __cpu_to_le32(7),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE8: Reserved for target autonomous hif_memcpy */
	{
		.pipenum = __cpu_to_le32(8),
		.pipedir = __cpu_to_le32(PIPEDIR_INOUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(16384),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE9, 10 and 11: Reserved for MHI */

	/* CE12: Target CV prefetch */
	{
		.pipenum = __cpu_to_le32(12),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE13: Target CV prefetch */
	{
		.pipenum = __cpu_to_le32(13),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE14: WMI logging/CFR/Spectral/Radar */
	{
		.pipenum = __cpu_to_le32(14),
		.pipedir = __cpu_to_le32(PIPEDIR_IN),
		.nentries = __cpu_to_le32(32),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS),
		.reserved = __cpu_to_le32(0),
	},

	/* CE15: host->target MSDU/MPDUQ */
	{
		.pipenum = __cpu_to_le32(15),
		.pipedir = __cpu_to_le32(PIPEDIR_OUT),
		.nentries = __cpu_to_le32(256),
		.nbytes_max = __cpu_to_le32(2048),
		.flags = __cpu_to_le32(CE_ATTR_FLAGS | CE_ATTR_DIS_INTR),
		.reserved = __cpu_to_le32(0),
	},
};

/* Map from service/endpoint to Copy Engine.
 * This table is derived from the CE_PCI TABLE, above.
 * It is passed to the Target at startup for use by firmware.
 * Pipe direction:
 *      PIPEDIR_OUT = UL = host -> target
 *      PIPEDIR_IN = DL = target -> host
 */
const struct service_to_pipe
ath12k_wifi8_target_service_to_ce_map_wlan_qcn9625[] = {
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_VO),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_VO),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_BK),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_BK),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_BE),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_BE),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_VI),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_DATA_VI),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_CONTROL),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(3),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_CONTROL),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_RSVD_CTRL),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(0),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_RSVD_CTRL),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(1),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_TEST_RAW_STREAMS),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(0),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_TEST_RAW_STREAMS),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(1),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_HTT_DATA_MSG),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(4),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_HTT_DATA_MSG),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(1),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC1),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(7),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC1),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(2),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_PKT_LOG),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(5),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_WMI_CONTROL_DIAG),
		__cpu_to_le32(PIPEDIR_IN),
		__cpu_to_le32(14),
	},
	{
		__cpu_to_le32(ATH12K_HTC_SVC_ID_HTT_DATA4_MSG),
		__cpu_to_le32(PIPEDIR_OUT),
		__cpu_to_le32(15),
	},

	/* (Additions here) */

	{ /* must be last */
		__cpu_to_le32(0),
		__cpu_to_le32(0),
		__cpu_to_le32(0),
	},

	/* CE15: Reserved */

	/* TODO: CP: update CE maps and definitions later when available */
	/* CE16: Reserved */
	/* CE17: Reserved */
	/* CE18: Reserved */
	/* CE19: Reserved */
	/* CE20: Reserved */
	/* CE21: Reserved */
	/* CE22: Reserved */
	/* CE23: Reserved */
};

const struct ce_attr ath12k_wifi8_host_ce_config_qcn9625[] = {
	/* CE0: host->target HTC control and raw streams */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 16,
		.src_sz_max = 2048,
		.dest_nentries = 0,
	},

	/* CE1: target->host HTT + HTC control */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 512,
		.recv_cb = ath12k_htc_rx_completion_handler,
	},

	/* CE2: target->host WMI */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 128,
		.recv_cb = ath12k_htc_rx_completion_handler,
	},

	/* CE3: host->target WMI (mac0) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = ath12k_htc_tx_completion_handler,
	},

	/* CE4: host->target HTT */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 2048,
		.src_sz_max = 256,
		.dest_nentries = 0,
	},

	/* CE5: target->host pktlog */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
#if defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
		.dest_nentries = 128,
#else
		.dest_nentries = 512,
#endif
		.recv_cb = ath12k_dp_htt_htc_t2h_msg_handler,
	},

	/* CE6: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE7: host->target WMI (mac1) */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 32,
		.src_sz_max = 2048,
		.dest_nentries = 0,
		.send_cb = ath12k_htc_tx_completion_handler,
	},

	/* CE8: target autonomous hif_memcpy */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE9: MHI */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE10: MHI */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE11: MHI */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE12: CV Prefetch */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

	/* CE13: CV Prefetch */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

#if defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
	/* CE14: target->host dbg log */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 0,
		.dest_nentries = 0,
	},

#else
	/* CE14: target->host dbg log */
	{
		.flags = CE_ATTR_FLAGS,
		.src_nentries = 0,
		.src_sz_max = 2048,
		.dest_nentries = 128,
		.recv_cb = ath12k_htc_rx_completion_handler,
	},
#endif

	/* CE15: host->target MSDU/MPDUQ */
	{
		.flags = CE_ATTR_FLAGS | CE_ATTR_DIS_INTR,
		.src_nentries = 2048,
		.src_sz_max = 2048,
		.dest_nentries = 0,
	},
};
