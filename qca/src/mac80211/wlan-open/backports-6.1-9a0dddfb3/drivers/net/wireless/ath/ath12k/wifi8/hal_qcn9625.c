// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "hal_desc.h"
#include "hal_rx_desc.h"
#include "hal_qcn9625.h"
#include "hw.h"
#include "hal.h"
#include "../dp_cmn.h"
#include <linux/cacheflush.h>

extern void ath12k_wifi8_hal_mon_ops_init(struct ath12k_hal *hal, u8 hw_version);

static const struct hal_srng_config hw_srng_config_template[] = {
	/* TODO: max_rings can populated by querying HW capabilities */
	/* REO2SW Rings */
	[HAL_REO_EXCEPTION] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW6,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW6_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception",
	},
	[HAL_REO_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW1,
		.max_rings = 6,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst",
	},
	[HAL_REO_DST_ROAMING] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW_ROAMING1 -
				 ATH12K_DP_RX_ROAMING_RING1,
		.max_rings = ATH12K_DP_RX_ROAMING_RING1 + 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_roaming",
	},
	[HAL_REO_EXCEPTION_DS] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW7,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception_ds",
	},
	[HAL_REO_EXCEPTION_MGMT] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW8,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception_mgmt",
	},
	[HAL_REO_DST_MGMT] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW9,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_mgmt",
	},
	[HAL_REO_DST_CTDMA] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW10,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_ctdma",
	},
	[HAL_REO_DST_HIGH_PRIO] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW11,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst_high_prio",
	},
	[HAL_REO_REINJECT] = {
		.start_ring_id = HAL_SRNG_RING_ID_SW2REO,
		.max_rings = 2,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_SW2REO_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_reinject",
	},
	[HAL_REO_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO_CMD,
		.max_rings = 2,
		.entry_size = (sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_reo_get_queue_stats)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_cmd",
	},
	[HAL_REO_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_reo_get_queue_stats_status)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_status",
	},
	[HAL_REO2PPE] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2PPE,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO2PPE_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_TCL_DATA] = {
		.start_ring_id = HAL_SRNG_RING_ID_SW2TCL1,
		.max_rings = 6,
		.entry_size = sizeof(struct hal_tcl_data_cmd) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2TCL1_RING_BASE_MSB_RING_SIZE,
		.name = "Tcl_data",
	},
	[HAL_TCL_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_SW2TCL_CMD,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_tcl_gse_cmd) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2TCL1_RING_BASE_MSB_RING_SIZE,
		.name = "Tcl_cmd",
	},
	[HAL_TCL_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_TCL_STATUS,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_tcl_status_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TCL_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "Tcl_status",
	},
	/* entry_size is set to minimum value here */
	/* Get valid entry_size from ath12k_hal_srng_get_cmd_size */
	[HAL_TQM_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_TQM_HOST_CMD,
		.max_rings = 1,
		.entry_size = 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_TQM_HOST_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "Tqm_cmd",
	},
	[HAL_TQM_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_TQM_HOST_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_64_hdr) +
			sizeof(struct hal_tqm_sync_cmd_status)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TQM_HOST_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "Tqm_status",
	},
	[HAL_ASE_CMD_RING] = {
		.start_ring_id = HAL_SRNG_RING_ID_ASE_CMD_RING,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_ase_cmd) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2WBM_ASE_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "ASE_CMD",
	},
	[HAL_ASE_STATUS_RING] = {
		.start_ring_id = HAL_SRNG_RING_ID_ASE_STATUS_RING,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_ase_status) >> 2,
		.mac_type = ATH12K_HAL_SRNG_DMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_SW2WBM_ASE_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "ASE_STATUS",
		.reg_writer_en = true,
	},

	/* TCL2SW Exception Ring */
	[HAL_TX_EXCEPTION] = {
		.start_ring_id = HAL_SRNG_RING_ID_TX_EXCEPTION,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_tcl_regular_exit_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TX_EXCEPTION_RING_BASE_MSB_RING_SIZE,
		.name = "Tx_exception",
	},
	[HAL_PEER_TX_TELEMETRY] = {
		.start_ring_id = HAL_SRNG_RING_ID_PEER_TX_TELEMETRY,
		.max_rings = 1,
		.entry_size = sizeof(struct tx_peer_telemetry_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TELEMETRY_RING_MAX_SIZE,
		.name = "Peer Tx Telemetry",
	},
	[HAL_PEER_RX_TELEMETRY] = {
		.start_ring_id = HAL_SRNG_RING_ID_PEER_RX_TELEMETRY,
		.max_rings = 1,
		.entry_size = sizeof(struct rx_peer_telemetry_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TELEMETRY_RING_MAX_SIZE,
		.name = "Peer Rx Telemetry",
	},
	[HAL_CE_SRC] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_SRC,
		.max_rings = 24,
		.entry_size = sizeof(struct hal_ce_srng_src_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_SRC_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST,
		.max_rings = 24,
		.entry_size = sizeof(struct hal_ce_srng_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_DST_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_DST_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST_STATUS,
		.max_rings = 24,
		.entry_size = sizeof(struct hal_ce_srng_dst_status_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_CE_DST_STATUS_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_WBM_IDLE_LINK] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_IDLE_LINK,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_link_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE,
		.name = "WBM_hw_idle_link",
	},
	[HAL_SW2WBM_RELEASE] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_SW0_RELEASE,
		.max_rings = 2,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2WBM_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "sw2wbm_release",
	},
	/* TX completions */
	[HAL_TX_COMPLETION] = {
		.start_ring_id = HAL_SRNG_RING_ID_TQM2SW0_RELEASE,
		.max_rings = 9,
		.entry_size = sizeof(struct hal_tqm2sw_completion_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TQM2SW_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "tx_completion",
	},
	[HAL_WBM_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_BUF1,
		.max_rings = 6,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_BUF_RING_MAX_SIZE,
		.name = "WBM_buf",
	},
	[HAL_WBM_IDLE_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_IDLE_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_IDLE_BUF_RING_MAX_SIZE,
		.name = "WBM_idle_buf",
	},
	[HAL_PPE2WBM_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_PPE2WBM_BUF1,
		.max_rings = 3,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_BUF_RING_MAX_SIZE,
		.name = "WBM_PPE_buf",
	},
	[HAL_PPE2WBM_IDLE_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_PPE2WBM_IDLE_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_IDLE_BUF_RING_MAX_SIZE,
		.name = "WBM_PPE_idle_buf",
	},
	[HAL_RXDMA_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
		.max_rings = 0,
		.entry_size = 0,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_dst",
	},
	[HAL_RXDMA_MONITOR_BUF] = {
		.start_ring_id = HAL_SRNG_SW2RXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_buf_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_monitor_buf",
		.reg_writer_en = true,
	},
	[HAL_RXDMA_MONITOR_STATUS] = { 0, },
	[HAL_RXDMA_MONITOR_DESC] = { 0, },
	[HAL_RXDMA_DIR_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_RXDMA_DIR_BUF,
		.max_rings = 2,
		.entry_size = 8 >> 2, /* TODO: Define the struct */
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
	},
	[HAL_PPE2TCL] = {
		.start_ring_id = HAL_SRNG_RING_ID_PPE2TCL1,
		.max_rings = 3,
		.entry_size = sizeof(struct hal_tcl_entrance_from_ppe_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_PPE2TCL_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_PPE_RELEASE] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_PPE_RELEASE,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM2PPE_RELEASE_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_TX_MONITOR_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2TXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_buf_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.reg_writer_en = true,
	},
	[HAL_RXDMA_MONITOR_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.name = "Rxdma_monitor_dst",
		.reg_writer_en = true,
	},
	[HAL_TX_MONITOR_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_TXMON2SW0_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
		.reg_writer_en = true,
	},
	[HAL_WBM_BUF_MGMT] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_BUF_MGMT,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_BUF_RING_MAX_SIZE,
		.name = "WBM_buf_mgmt",
	},
	[HAL_WBM_IDLE_BUF_MGMT] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM_IDLE_BUF_MGMT,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_WBM_IDLE_BUF_RING_MAX_SIZE,
		.name = "WBM_idle_buf_mgmt",
	},
	[HAL_RXOLE_FSE_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_FSE_CMD,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_fse_cmd) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_FSE_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "Fse_cmd",
	},
	[HAL_SAM_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_SAM_HOST_CMD,
		.max_rings = 1,
		.entry_size = 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SAM_HOST_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "SAM_cmd"
	},
	[HAL_SAM_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_SAM_HOST_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_64_hdr) +
			       sizeof(struct hal_sam_cmd_status)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_SAM_HOST_STATUS_RING_BASE_MSB_RING_SIZE,
		.name = "SAM_status"
	},
	[HAL_REO_FLUSH] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_flush",
	},
	[HAL_TQM2PPE] = {
		.start_ring_id = HAL_SRNG_RING_ID_TQM2PPE_RELEASE,
		.max_rings = 1,
		.entry_size = sizeof(struct ath12k_buffer_addr) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TQM2PPE_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "hbm_tx_completion",
	},
	[HAL_TQM2SW_FW_COMPLETION] = {
		.start_ring_id = HAL_SRNG_RING_ID_TQM2SW5_RELEASE,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_tqm2sw_completion_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TQM2SW_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "tqm2sw_fw",
	},
};

const struct ath12k_hw_regs qcn9625_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_id = 0x1604,
	.hal_tcl1_ring_misc = 0x160c,
	.hal_tcl1_ring_tp_addr_lsb = 0x1618,
	.hal_tcl1_ring_tp_addr_msb = 0x161c,
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x162c,
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x1630,
	.hal_tcl1_ring_msi1_base_lsb = 0x1644,
	.hal_tcl1_ring_msi1_base_msb = 0x1648,
	.hal_tcl1_ring_msi1_data = 0x164c,
	.hal_tcl1_ring_base_lsb = 0x15fc,
	.hal_tcl1_ring_base_msb = 0x1600,
	.hal_tcl2_ring_base_lsb = 0x1674,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x1cfc,

	/* WBM idle link ring address */
	.hal_wbm_idle_ring_base_lsb = 0x51c,
	.hal_wbm_idle_ring_misc_addr = 0x52c,
	.hal_wbm_r0_idle_list_cntl_addr = 0x130,
	.hal_wbm_r0_idle_list_size_addr = 0x134,
	.hal_wbm_scattered_ring_base_lsb = 0x138,
	.hal_wbm_scattered_ring_base_msb = 0x13c,
	.hal_wbm_scattered_desc_head_info_ix0 = 0x140,
	.hal_wbm_scattered_desc_head_info_ix1 = 0x144,
	.hal_wbm_scattered_desc_tail_info_ix0 = 0x148,
	.hal_wbm_scattered_desc_tail_info_ix1 = 0x14c,
	.hal_wbm_scattered_desc_ptr_hp_addr = 0x150,

	/* SW2WBM release ring address */
	.hal_wbm_sw_release_ring_base_lsb = 0x24c,
	.hal_wbm_sw1_release_ring_base_lsb = 0x2c8,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x01E0C094,
	.pcie_pcs_osc_dtct_config_base = 0x01E0CE58,

	/*PCIe qrtr node id reg*/
	.pcie_pcie_local_qrtr_ins_reg = 0x1E03300,
	.pcie_pcie_local_unified_fw_board_id_reg = 0x1E03304,

	/*PCIe hot reset reg*/
	.pcie_gcc_gcc_pcie_hot_rst = 0x1E38438,

	/* REO DEST ring address */
	.hal_reo2_ring_base = 0x8c0,
	.hal_reo1_misc_ctrl_addr = 0x198c,
	.hal_reo1_sw_cookie_cfg0 = 0x6c,
	.hal_reo1_sw_cookie_cfg1 = 0x70,
	.hal_reo1_qdesc_lut_base0 = 0x74,
	.hal_reo1_qdesc_lut_base1 = 0x78,
	.hal_reo1_qdesc_addr = 0x7c,
	.hal_reo1_qdesc_max_peerid = 0x88,
	.hal_reo1_ring_base_lsb = 0x848,
	.hal_reo1_ring_base_msb = 0x84c,
	.hal_reo1_ring_id = 0x850,
	.hal_reo1_ring_misc = 0x858,
	.hal_reo1_ring_hp_addr_lsb = 0x85c,
	.hal_reo1_ring_hp_addr_msb = 0x860,
	.hal_reo1_ring_producer_int_setup =
					0x86c,
	.hal_reo1_ring_msi1_base_lsb = 0x890,
	.hal_reo1_ring_msi1_base_msb = 0x894,
	.hal_reo1_ring_msi1_data = 0x898,
	.hal_reo1_aging_thres_ix0 = 0x1918,
	.hal_reo1_aging_thres_ix1 = 0x191c,
	.hal_reo1_aging_thres_ix2 = 0x1920,
	.hal_reo1_aging_thres_ix3 = 0x1924,

	/* REO Exception ring address */
	.hal_reo2_sw0_ring_base = 0xd70,

	/* REO Reinject ring address */
	.hal_sw2reo_ring_base = 0x344,
	.hal_sw2reo1_ring_base = 0x3bc,

	/* REO cmd ring address */
	.hal_reo_cmd_ring_base = 0x2cc,

	/* REO status ring address */
	.hal_reo_status_ring_base = 0x135c,

	/* CE base address */
	.hal_umac_ce0_src_reg_base = 0x1B80000,
	.hal_umac_ce0_dest_reg_base = 0x1B81000,
	.hal_umac_ce1_src_reg_base = 0x1B82000,
	.hal_umac_ce1_dest_reg_base = 0x1B83000,
	.hal_ppe_rel_ring_base = 0xb74,
	.hal_reo2ppe_ring_base = 0xde8,
	.hal_tcl_ppe2tcl_ring_base_lsb = 0x19bc,
	/* PMM register base address */
	.hal_pmm_reg_base = 0x010D40FC,
};

const struct ath12k_hw_hal_params ath12k_wifi8_hw_hal_params_qcn9625 = {
	.rx_buf_rbm = HAL_RX_BUF_RBM_SW5_BM,
	.tqm2sw_cc_enable1 = HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW0_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW1_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW2_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW3_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW4_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW6_EN,
	.tqm2sw_cc_enable2 = HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW7_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW8_EN |
			     HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2PPE_EN,
	.link_desc_size = HAL_LINK_DESC_SIZE,
	.num_mpdus_per_link_desc = HAL_NUM_MPDUS_PER_LINK_DESC,
	.num_tx_msdus_per_link_desc = HAL_NUM_TX_MSDUS_PER_LINK_DESC,
	.num_rx_msdus_per_link_desc = HAL_NUM_RX_MSDUS_PER_LINK_DESC,
	.num_mpdu_links_per_queue_desc = HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC,
	.dscp_tid_map_tbl_max_entries = HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX,
	.num_tids = HAL_WIFI8_NUM_TIDS,
	.reoq_lut_size = HAL_WIFI8_REOQ_LUT_SIZE,
	.dp_rx_err_rdi = HAL_WIFI8_DP_RX_ERR_RDI,
	.rx_mgmt_buf_rbm = HAL_RX_BUF_RBM_SW4_BM,
};

const struct ath12k_hal_reset_rings qcn9625_reset_rings[HAL_RESET_RING_TYPE_MAX] = {
	[HAL_TCL_GSE_CMD_RING] = {
		.consumer_prefetch_timer = 0x00F1490C,
		.srng_misc_reg = 0x00F148DC,
		.hp = 0x00F17030,
	},
	[HAL_TQM_LOWPRI_CMD_RING] = {
		.consumer_prefetch_timer = 0x00F08164,
		.srng_misc_reg = 0x00F08134,
		.hp = 0x00F0B010,
	},
	[HAL_TQM_HIPRI_CMD_RING] = {
		.consumer_prefetch_timer = 0x00F081DC,
		.srng_misc_reg = 0x00F081AC,
		.hp = 0x00F0B018,
	},
	[HAL_TQM_HIPRI1_CMD_RING] = {
		.consumer_prefetch_timer = 0x00F08254,
		.srng_misc_reg = 0x00F08224,
		.hp = 0x00F0B020,
	},
	[HAL_FW2TQM_ENT_RING] = {
		.consumer_prefetch_timer = 0x00F080EC,
		.srng_misc_reg = 0x00F080BC,
		.hp = 0x00F0B008,
	},
	[HAL_TCL_TCL2TQM_ENT_RING] = {
		.srng_misc_reg = 0x00F14C1C,
		.hp = 0x00F17060,
	},
	[HAL_TQM_TCL2TQM_ENT_RING] = {
		.consumer_prefetch_timer = 0x00F08074,
		.srng_misc_reg = 0x00F08044,
		.hp = 0x00F0B000,
	},
	[HAL_WBM_WBM2TQM_DESC_RING] = {
		.srng_misc_reg = 0x00F0434C,
		.hp = 0x00F07020,
	},
	[HAL_TQM_WBM2TQM_DESC_RING] = {
		.consumer_prefetch_timer = 0x00F083BC,
		.srng_misc_reg = 0x00F0838C,
		.hp = 0x00F0B038,
	},
	[HAL_TQM_TQM2WBM_REL_RING] = {
		.srng_misc_reg = 0x00F08404,
		.hp = 0x00F0B040,
	},
	[HAL_WBM_TQM2WBM_REL_RING] = {
		.consumer_prefetch_timer = 0x00F0419C,
		.srng_misc_reg = 0x00F0416C,
		.hp = 0x00F07000,
	},
	[HAL_WBM_WBM2REO_DESC_RING] = {
		.srng_misc_reg = 0x00F05AE0,
		.hp = 0x00F07170,
	},
	[HAL_REO_WBM2REO_DESC_RING] = {
		.consumer_prefetch_timer = 0x00F22294,
		.srng_misc_reg = 0x00F22264,
		.hp = 0x00F25018,
	},
	[HAL_REO_REO2WBM_REL_RING] = {
		.srng_misc_reg = 0x00F230A4,
		.hp = 0x00F25100,
	},
	[HAL_WBM_REO2WBM_REL_RING] = {
		.consumer_prefetch_timer = 0x00F04214,
		.srng_misc_reg = 0x00F041E4,
		.hp = 0x00F07008,
	},
	[HAL_FW2WBM_REL_RING] = {
		.consumer_prefetch_timer = 0x00F04304,
		.srng_misc_reg = 0x00F042D4,
		.hp = 0x00F07018,
	},
	[HAL_FW2TQM_REL_RING] = {
		.consumer_prefetch_timer = 0x00F08E2C,
		.srng_misc_reg = 0x00F08DFC,
		.hp = 0x00F0B0E8,
	},
	[HAL_WBM_IDLE_DESC_RING] = {
		.consumer_prefetch_timer = 0x00F0455C,
		.srng_misc_reg = 0x00F0452C,
		.hp = 0x00F07040,
	},
	[HAL_TCL_GSE_STATUS_RING] = {
		.srng_misc_reg = 0x00F14C94,
		.hp = 0x00F17068,
	},
	[HAL_TCL2FW_OUTPUT_RING] = {
		.srng_misc_reg = 0x00F14D84,
		.hp = 0x00F17078,
	},
	[HAL_TQM_HIPRI_STATUS_RING] = {
		.srng_misc_reg = 0x00F084F4,
		.hp = 0x00F0B050,
	},
	[HAL_TQM_HIPRI1_STATUS_RING] = {
		.srng_misc_reg = 0x00F0856C,
		.hp = 0x00F0B058,
	},
	[HAL_TQM_LOWPRI_STATUS_RING] = {
		.srng_misc_reg = 0x00F0847C,
		.hp = 0x00F0B048,
	},
	[HAL_TQM2FW_REL_BUF_RING] = {
		.srng_misc_reg = 0x00F0865C,
		.hp = 0x00F0B068,
	},
	[HAL_WBM_ERR_REL_RING] = {
		.srng_misc_reg = 0x00F04580,
		.hp = 0x00F07048,
	},
	[HAL_REO2FW_DEST_RING] = {
		.srng_misc_reg = 0x00F22FB4,
		.hp = 0x00F250F0,
	},
	[HAL_REO2FW_MGMT_RING] = {
		.srng_misc_reg = 0x00F2302C,
		.hp = 0x00F250F8,
	},
	[HAL_TQM_ERR_REL_RING] = {
		.srng_misc_reg = 0x00F086D4,
		.hp = 0x00F0B070,
	},
	[HAL_TQM2TQM_IN1_MLO_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F08FE4,
		.srng_misc_reg = 0x00F08FB4,
		.hp = 0x00F0B0F0,
		.consumer_producer_mlo = 0x00F09018,
		.mlo_doorbell_press = 0x00F0901C,
	},
	[HAL_TQM2TQM_IN2_MLO_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F09078,
		.srng_misc_reg = 0x00F09048,
		.hp = 0x00F0B0F8,
		.consumer_producer_mlo = 0x00F090AC,
		.mlo_doorbell_press = 0x00F090B0,
	},
	[HAL_TQM2TQM_IN3_MLO_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F093C0,
		.srng_misc_reg = 0x00F09390,
		.hp = 0x00F0B110,
		.consumer_producer_mlo = 0x00F093F4,
		.mlo_doorbell_press = 0x00F093F8,
	},
	[HAL_TQM2TQM_IN4_MLO_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F09454,
		.srng_misc_reg = 0x00F09424,
		.hp = 0x00F0B118,
		.consumer_producer_mlo = 0x00F09488,
		.mlo_doorbell_press = 0x00F0948C,
	},
	[HAL_REO2REO_OUT1_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F2311C,
		.hp = 0x00F25108,
		.consumer_producer_mlo = 0x00F23180,
		.mlo_doorbell_press = 0x00F23184,
	},
	[HAL_REO2REO_OUT2_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F231B0,
		.hp = 0x00F25110,
		.consumer_producer_mlo = 0x00F23214,
		.mlo_doorbell_press = 0x00F23218,
	},
	[HAL_REO2REO_OUT3_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F23244,
		.hp = 0x00F25118,
		.consumer_producer_mlo = 0x00F232A8,
		.mlo_doorbell_press = 0x00F232AC,
	},
	[HAL_REO2REO_OUT4_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F232D8,
		.hp = 0x00F25120,
		.consumer_producer_mlo = 0x00F2333C,
		.mlo_doorbell_press = 0x00F23340,
	},
	[HAL_TQM2TQM_OUT1_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F090DC,
		.hp = 0x00F0B100,
		.consumer_producer_mlo = 0x00F09140,
		.mlo_doorbell_press = 0x00F09144,
	},
	[HAL_TQM2TQM_OUT2_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F09170,
		.hp = 0x00F0B108,
		.consumer_producer_mlo = 0x00F091D4,
		.mlo_doorbell_press = 0x00F091D8,
	},
	[HAL_TQM2TQM_OUT3_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F094B8,
		.hp = 0x00F0B120,
		.consumer_producer_mlo = 0x00F0951C,
		.mlo_doorbell_press = 0x00F09520,
	},
	[HAL_TQM2TQM_OUT4_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00F0954C,
		.hp = 0x00F0B128,
		.consumer_producer_mlo = 0x00F095B0,
		.mlo_doorbell_press = 0x00F095B4,
	},
	[HAL_TCL2TCL_MLO_OUT1_IPC_RING] = {
		.srng_misc_reg = 0x00F14E74,
		.hp = 0x00F17088,
		.consumer_producer_mlo = 0x00F14ED8,
		.mlo_doorbell_press = 0x00F14EDC,
	},
	[HAL_TCL2TCL_MLO_OUT2_IPC_RING] = {
		.srng_misc_reg = 0x00F14F08,
		.hp = 0x00F17090,
		.consumer_producer_mlo = 0x00F14F6C,
		.mlo_doorbell_press = 0x00F14F70,
	},
	[HAL_TCL2TCL_MLO_OUT3_IPC_RING] = {
		.srng_misc_reg = 0x00F14F9C,
		.hp = 0x00F17098,
		.consumer_producer_mlo = 0x00F15000,
		.mlo_doorbell_press = 0x00F15004,
	},
	[HAL_TCL2TCL_MLO_OUT4_IPC_RING] = {
		.srng_misc_reg = 0x00F15030,
		.hp = 0x00F170A0,
		.consumer_producer_mlo = 0x00F15094,
		.mlo_doorbell_press = 0x00F15098,
	},
	[HAL_RXDMA2REO_MLO0_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F220D8,
		.srng_misc_reg = 0x00F220A8,
		.hp = 0x00F25000,
		.consumer_producer_mlo = 0x00F2210C,
		.mlo_doorbell_press = 0x00F22110,
	},
	[HAL_RXDMA2REO_MLO1_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F2216C,
		.srng_misc_reg = 0x00F2213C,
		.hp = 0x00F25008,
		.consumer_producer_mlo = 0x00F221A0,
		.mlo_doorbell_press = 0x00F221A4,
	},
	[HAL_RXDMA2REO_MLO2_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F22200,
		.srng_misc_reg = 0x00F221D0,
		.hp = 0x00F25010,
		.consumer_producer_mlo = 0x00F22234,
		.mlo_doorbell_press = 0x00F22238,
	},
	[HAL_RXDMA2REO_MLO3_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F22654,
		.srng_misc_reg = 0x00F22624,
		.hp = 0x00F25058,
		.consumer_producer_mlo = 0x00F22688,
		.mlo_doorbell_press = 0x00F2268C,
	},
	[HAL_RXDMA2REO_MLO4_IPC_RING] = {
		.consumer_prefetch_timer = 0x00F226E8,
		.srng_misc_reg = 0x00F226B8,
		.hp = 0x00F25060,
		.consumer_producer_mlo = 0x00F2271C,
		.mlo_doorbell_press = 0x00F22720,
	},
	[HAL_WBM2RXDMA_0_BUF_RING_IPC_RING] = {
		.consumer_prefetch_timer = 0x00E4224C,
		.srng_misc_reg = 0x00E4221C,
		.hp = 0x00E44818,
		.consumer_producer_mlo = 0x00E42280,
		.mlo_doorbell_press = 0x00E42284,
	},
	[HAL_WBM2RXDMA_1_BUF_RING_IPC_RING] = {
		.consumer_prefetch_timer = 0x00E42314,
		.srng_misc_reg = 0x00E422E4,
		.hp = 0x00E44820,
		.consumer_producer_mlo = 0x00E42348,
		.mlo_doorbell_press = 0x00E4234C,
	},
	[HAL_WBM2RXDMA_2_BUF_RING_IPC_RING] = {
		.consumer_prefetch_timer = 0x00E423DC,
		.srng_misc_reg = 0x00E423AC,
		.hp = 0x00E44828,
		.consumer_producer_mlo = 0x00E42410,
		.mlo_doorbell_press = 0x00E42414,
	},
	[HAL_WBM2RXDMA_LINK_RING_IPC_RING] = {
		.consumer_prefetch_timer = 0x00E425FC,
		.srng_misc_reg = 0x00E425CC,
		.hp = 0x00E44840,
		.consumer_producer_mlo = 0x00E42630,
		.mlo_doorbell_press = 0x00E42634,
	},
	[HAL_RXDMA2REO_MLO_IPC_RING] = {
		.srng_misc_reg = 0x00E431F8,
		.hp = 0x00E44A20,
		.consumer_producer_mlo = 0x00E4325C,
		.mlo_doorbell_press = 0x00E43260,
	},
	[HAL_WBM2RXDMA_LINK_MLO1_IPC_RING] = {
		.srng_misc_reg = 0x00F047CC,
		.hp = 0x00F07068,
		.consumer_producer_mlo = 0x00F04830,
		.mlo_doorbell_press = 0x00F04834,
	},
	[HAL_WBM2RXDMA_LINK_MLO2_IPC_RING] = {
		.srng_misc_reg = 0x00F04860,
		.hp = 0x00F07070,
		.consumer_producer_mlo = 0x00F048C4,
		.mlo_doorbell_press = 0x00F048C8,
	},
	[HAL_WBM2RXDMA_LINK_MLO3_IPC_RING] = {
		.srng_misc_reg = 0x00F048F4,
		.hp = 0x00F07078,
		.consumer_producer_mlo = 0x00F04958,
		.mlo_doorbell_press = 0x00F0495C,
	},
	[HAL_WBM2RXDMA_LINK_MLO4_IPC_RING] = {
		.srng_misc_reg = 0x00F04988,
		.hp = 0x00F07080,
		.consumer_producer_mlo = 0x00F049EC,
		.mlo_doorbell_press = 0x00F049F0,
	},
	[HAL_WBM2RXDMA_LINK_MLO5_IPC_RING] = {
		.srng_misc_reg = 0x00F04A1C,
		.hp = 0x00F07088,
		.consumer_producer_mlo = 0x00F04A80,
		.mlo_doorbell_press = 0x00F04A84,
	},
	[HAL_WBM2RXDMA_SW0_BUFF_MLO1_IPC_RING] = {
		.srng_misc_reg = 0x00F05078,
		.hp = 0x00F070E0,
		.consumer_producer_mlo = 0x00F050DC,
		.mlo_doorbell_press = 0x00F050E0,
	},
	[HAL_WBM2RXDMA_SW0_BUFF_MLO2_IPC_RING] = {
		.srng_misc_reg = 0x00F0510C,
		.hp = 0x00F070E8,
		.consumer_producer_mlo = 0x00F05170,
		.mlo_doorbell_press = 0x00F05174,
	},
	[HAL_WBM2RXDMA_SW0_BUFF_MLO3_IPC_RING] = {
		.srng_misc_reg = 0x00F051A0,
		.hp = 0x00F070F0,
		.consumer_producer_mlo = 0x00F05204,
		.mlo_doorbell_press = 0x00F05208,
	},
	[HAL_WBM2RXDMA_SW0_BUFF_MLO4_IPC_RING] = {
		.srng_misc_reg = 0x00F05234,
		.hp = 0x00F070F8,
		.consumer_producer_mlo = 0x00F05298,
		.mlo_doorbell_press = 0x00F0529C,
	},
	[HAL_WBM2RXDMA_SW0_BUFF_MLO5_IPC_RING] = {
		.srng_misc_reg = 0x00F052C8,
		.hp = 0x00F07100,
		.consumer_producer_mlo = 0x00F0532C,
		.mlo_doorbell_press = 0x00F05330,
	},
	[HAL_WBM2RXDMA_SW1_BUFF_MLO1_IPC_RING] = {
		.srng_misc_reg = 0x00F05640,
		.hp = 0x00F07130,
		.consumer_producer_mlo = 0x00F056A4,
		.mlo_doorbell_press = 0x00F056A8,
	},
	[HAL_WBM2RXDMA_SW1_BUFF_MLO2_IPC_RING] = {
		.srng_misc_reg = 0x00F056D4,
		.hp = 0x00F07138,
		.consumer_producer_mlo = 0x00F05738,
		.mlo_doorbell_press = 0x00F0573C,
	},
	[HAL_WBM2RXDMA_SW1_BUFF_MLO3_IPC_RING] = {
		.srng_misc_reg = 0x00F05768,
		.hp = 0x00F07140,
		.consumer_producer_mlo = 0x00F057CC,
		.mlo_doorbell_press = 0x00F057D0,
	},
	[HAL_WBM2RXDMA_SW1_BUFF_MLO4_IPC_RING] = {
		.srng_misc_reg = 0x00F057FC,
		.hp = 0x00F07148,
		.consumer_producer_mlo = 0x00F05860,
		.mlo_doorbell_press = 0x00F05864,
	},
	[HAL_WBM2RXDMA_SW1_BUFF_MLO5_IPC_RING] = {
		.srng_misc_reg = 0x00F05890,
		.hp = 0x00F07150,
		.consumer_producer_mlo = 0x00F058F4,
		.mlo_doorbell_press = 0x00F058F8,
	},
	[HAL_WBM2RXDMA_PPE_BUFF_MLO1_IPC_RING] = {
		.srng_misc_reg = 0x00F0535C,
		.hp = 0x00F07108,
		.consumer_producer_mlo = 0x00F053C0,
		.mlo_doorbell_press = 0x00F053C4,
	},
	[HAL_WBM2RXDMA_PPE_BUFF_MLO2_IPC_RING] = {
		.srng_misc_reg = 0x00F053F0,
		.hp = 0x00F07110,
		.consumer_producer_mlo = 0x00F05454,
		.mlo_doorbell_press = 0x00F05458,
	},
	[HAL_WBM2RXDMA_PPE_BUFF_MLO3_IPC_RING] = {
		.srng_misc_reg = 0x00F05484,
		.hp = 0x00F07118,
		.consumer_producer_mlo = 0x00F054E8,
		.mlo_doorbell_press = 0x00F054EC,
	},
	[HAL_WBM2RXDMA_PPE_BUFF_MLO4_IPC_RING] = {
		.srng_misc_reg = 0x00F05518,
		.hp = 0x00F07120,
		.consumer_producer_mlo = 0x00F0557C,
		.mlo_doorbell_press = 0x00F05580,
	},
	[HAL_WBM2RXDMA_PPE_BUFF_MLO5_IPC_RING] = {
		.srng_misc_reg = 0x00F055AC,
		.hp = 0x00F07128,
		.consumer_producer_mlo = 0x00F05610,
		.mlo_doorbell_press = 0x00F05614,
	},
	[HAL_WBM2RXOLE_ASE_CMD_MLO1_IPC_RING] = {
		.srng_misc_reg = 0x00F04AB0,
		.hp = 0x00F07090,
		.consumer_producer_mlo = 0x00F04B14,
		.mlo_doorbell_press = 0x00F04B18,
	},
	[HAL_WBM2RXOLE_ASE_CMD_MLO2_IPC_RING] = {
		.srng_misc_reg = 0x00F04B44,
		.hp = 0x00F07098,
		.consumer_producer_mlo = 0x00F04BA8,
		.mlo_doorbell_press = 0x00F04BAC,
	},
	[HAL_WBM2RXOLE_ASE_CMD_MLO3_IPC_RING] = {
		.srng_misc_reg = 0x00F04BD8,
		.hp = 0x00F070A0,
		.consumer_producer_mlo = 0x00F04C3C,
		.mlo_doorbell_press = 0x00F04C40,
	},
	[HAL_WBM2RXOLE_ASE_CMD_MLO4_IPC_RING] = {
		.srng_misc_reg = 0x00F04C6C,
		.hp = 0x00F070A8,
		.consumer_producer_mlo = 0x00F04CD0,
		.mlo_doorbell_press = 0x00F04CD4,
	},
	[HAL_WBM2RXOLE_ASE_CMD_MLO5_IPC_RING] = {
		.srng_misc_reg = 0x00F04D00,
		.hp = 0x00F070B0,
		.consumer_producer_mlo = 0x00F04D64,
		.mlo_doorbell_press = 0x00F04D68,
	},
	[HAL_WBM2RXOLE_FSE_CMD_MLO1_IPC_RING] = {
		.srng_misc_reg = 0x00F04D94,
		.hp = 0x00F070B8,
		.consumer_producer_mlo = 0x00F04DF8,
		.mlo_doorbell_press = 0x00F04DFC,
	},
	[HAL_WBM2RXOLE_FSE_CMD_MLO2_IPC_RING] = {
		.srng_misc_reg = 0x00F04E28,
		.hp = 0x00F070C0,
		.consumer_producer_mlo = 0x00F04E8C,
		.mlo_doorbell_press = 0x00F04E90,
	},
	[HAL_WBM2RXOLE_FSE_CMD_MLO3_IPC_RING] = {
		.srng_misc_reg = 0x00F04EBC,
		.hp = 0x00F070C8,
		.consumer_producer_mlo = 0x00F04F20,
		.mlo_doorbell_press = 0x00F04F24,
	},
	[HAL_WBM2RXOLE_FSE_CMD_MLO4_IPC_RING] = {
		.srng_misc_reg = 0x00F04F50,
		.hp = 0x00F070D0,
		.consumer_producer_mlo = 0x00F04FB4,
		.mlo_doorbell_press = 0x00F04FB8,
	},
	[HAL_WBM2RXOLE_FSE_CMD_MLO5_IPC_RING] = {
		.srng_misc_reg = 0x00F04FE4,
		.hp = 0x00F070D8,
		.consumer_producer_mlo = 0x00F05048,
		.mlo_doorbell_press = 0x00F0504C,
	},
	[HAL_WBM2RXOLE_ASE_CMD_RING_IPC_RING] = {
		.consumer_prefetch_timer = 0x00E1E1B0,
		.srng_misc_reg = 0x00E1E180,
		.hp = 0x00E20000,
		.consumer_producer_mlo = 0x00E1E1E4,
		.mlo_doorbell_press = 0x00E1E1E8,
	},
	[HAL_WBM2RXOLE_FSE_CMD_RING_IPC_RING] = {
		.consumer_prefetch_timer = 0x00E1E2C4,
		.srng_misc_reg = 0x00E1E294,
		.hp = 0x00E20010,
		.consumer_producer_mlo = 0x00E1E2F8,
		.mlo_doorbell_press = 0x00E1E2FC,
	},
};

u32 ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625(struct hal_rx_desc *desc)
{
	u32 info = __le32_to_cpu(desc->u.qcn9625_compact.msdu_end.info14);
	u32 errmap = 0;

	if (info & RX_MSDU_END_INFO14_FCS_ERR)
		errmap |= HAL_RX_MPDU_ERR_FCS;

	if (info & RX_MSDU_END_INFO14_DECRYPT_ERR)
		errmap |= HAL_RX_MPDU_ERR_DECRYPT;

	if (info & RX_MSDU_END_INFO14_TKIP_MIC_ERR)
		errmap |= HAL_RX_MPDU_ERR_TKIP_MIC;

	if (info & RX_MSDU_END_INFO14_A_MSDU_ERROR)
		errmap |= HAL_RX_MPDU_ERR_AMSDU_ERR;

	if (info & RX_MSDU_END_INFO14_OVERFLOW_ERR)
		errmap |= HAL_RX_MPDU_ERR_OVERFLOW;

	if (info & RX_MSDU_END_INFO14_MSDU_LENGTH_ERR)
		errmap |= HAL_RX_MPDU_ERR_MSDU_LEN;

	if (info & RX_MSDU_END_INFO14_MPDU_LENGTH_ERR)
		errmap |= HAL_RX_MPDU_ERR_MPDU_LEN;

	return errmap;
}

void
ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625(struct hal_rx_desc *desc,
						   u8 *crypto_hdr,
						   enum hal_encrypt_type encype)
{
	unsigned int key_id;

	switch (encype) {
	case HAL_ENCRYPT_TYPE_OPEN:
		return;
	case HAL_ENCRYPT_TYPE_TKIP_NO_MIC:
	case HAL_ENCRYPT_TYPE_TKIP_MIC:
		crypto_hdr[0] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		crypto_hdr[1] = 0;
		crypto_hdr[2] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		break;
	case HAL_ENCRYPT_TYPE_CCMP_128:
	case HAL_ENCRYPT_TYPE_CCMP_256:
	case HAL_ENCRYPT_TYPE_GCMP_128:
	case HAL_ENCRYPT_TYPE_AES_GCMP_256:
		crypto_hdr[0] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		crypto_hdr[1] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2(desc->u.qcn9625_compact.mpdu_start.pn[0]);
		crypto_hdr[2] = 0;
		break;
	case HAL_ENCRYPT_TYPE_WEP_40:
	case HAL_ENCRYPT_TYPE_WEP_104:
	case HAL_ENCRYPT_TYPE_WEP_128:
	case HAL_ENCRYPT_TYPE_WAPI_GCM_SM4:
	case HAL_ENCRYPT_TYPE_WAPI:
		return;
	}
	key_id = le32_get_bits(desc->u.qcn9625_compact.mpdu_start.info4,
			       RX_MPDU_INFO_INFO4_KEY_ID_OCTET);
	crypto_hdr[3] = 0x20 | (key_id << 6);
	crypto_hdr[4] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE3(desc->u.qcn9625_compact.mpdu_start.pn[0]);
	crypto_hdr[5] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE4(desc->u.qcn9625_compact.mpdu_start.pn[0]);
	crypto_hdr[6] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE1(desc->u.qcn9625_compact.mpdu_start.pn[1]);
	crypto_hdr[7] =
		HAL_RX_MPDU_INFO_PN_GET_BYTE2(desc->u.qcn9625_compact.mpdu_start.pn[1]);
}

void
ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625(struct hal_rx_desc *desc,
					       struct ieee80211_hdr *hdr)
{
	hdr->frame_control = desc->u.qcn9625_compact.mpdu_start.frame_ctrl;
	hdr->duration_id = desc->u.qcn9625_compact.mpdu_start.duration;
	ether_addr_copy(hdr->addr1, desc->u.qcn9625_compact.mpdu_start.addr1);
	ether_addr_copy(hdr->addr2, desc->u.qcn9625_compact.mpdu_start.addr2);
	ether_addr_copy(hdr->addr3, desc->u.qcn9625_compact.mpdu_start.addr3);
	if (__le32_to_cpu(desc->u.qcn9625_compact.mpdu_start.info2) &
	    RX_MPDU_INFO_INFO2_MAC_ADDR_AD4_VALID) {
		ether_addr_copy(hdr->addr4, desc->u.qcn9625_compact.mpdu_start.addr4);
	}
	hdr->seq_ctrl = desc->u.qcn9625_compact.mpdu_start.seq_ctrl;
}

void ath12k_wifi8_hal_extract_rx_desc_data_qcn9625(struct hal_rx_desc_data *rx_desc_data,
						   struct hal_rx_desc *rx_desc,
						   struct hal_rx_desc *ldesc)
{
	rx_desc_data->msdu_done = ath12k_wifi8_hal_rx_h_msdu_done_qcn9625(ldesc);
	rx_desc_data->msdu_len = ath12k_wifi8_hal_rx_h_msdu_len_qcn9625(ldesc);
	rx_desc_data->l3_pad_bytes = ath12k_wifi8_hal_rx_h_l3pad_qcn9625(ldesc);
	rx_desc_data->is_first_msdu =
		ath12k_wifi8_hal_rx_h_first_msdu_qcn9625(ldesc);
	rx_desc_data->is_last_msdu =
		ath12k_wifi8_hal_rx_h_last_msdu_qcn9625(ldesc);
	rx_desc_data->freq = ath12k_wifi8_hal_rx_h_freq_qcn9625(rx_desc);
	rx_desc_data->snr = ath12k_wifi8_hal_rx_h_snr_qcn9625(rx_desc);
	rx_desc_data->pkt_type = ath12k_wifi8_hal_rx_h_pkt_type_qcn9625(rx_desc);
	rx_desc_data->bw = ath12k_wifi8_hal_rx_h_rx_bw_qcn9625(rx_desc);
	rx_desc_data->rate_mcs = ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625(rx_desc);
	rx_desc_data->nss = hweight8(ath12k_wifi8_hal_rx_h_nss_qcn9625(rx_desc));
	rx_desc_data->sgi = ath12k_wifi8_hal_rx_h_sgi_qcn9625(rx_desc);
	rx_desc_data->is_mcbc =
		ath12k_wifi8_hal_rx_h_is_da_mcbc_qcn9625(rx_desc);
	rx_desc_data->seq_no =
		ath12k_wifi8_hal_rx_h_seq_no_qcn9625(rx_desc);
	rx_desc_data->err_bitmap =
		ath12k_wifi8_hal_rx_h_mpdu_err_qcn9625(rx_desc);
	rx_desc_data->is_decrypted =
		ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625(rx_desc);
	rx_desc_data->decap = ath12k_wifi8_hal_rx_h_decap_type_qcn9625(rx_desc);
	rx_desc_data->ip_csum_fail =
		ath12k_wifi8_hal_rx_h_ip_cksum_fail_qcn9625(rx_desc);
	rx_desc_data->l4_csum_fail =
		ath12k_wifi8_hal_rx_h_l4_cksum_fail_qcn9625(rx_desc);
	rx_desc_data->tid = ath12k_wifi8_hal_rx_h_tid_qcn9625(rx_desc);
	rx_desc_data->mesh_ctrl_present =
		ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625(rx_desc);
	rx_desc_data->seq_ctl_valid =
			ath12k_wifi8_hal_rx_h_seq_ctrl_valid_qcn9625(rx_desc);
	rx_desc_data->fc_valid = ath12k_wifi8_hal_rx_h_fc_valid_qcn9625(rx_desc);
	rx_desc_data->is_ip_valid = ath12k_wifi8_hal_rx_h_is_ip_valid_qcn9625(rx_desc);
	rx_desc_data->is_from_ds = ath12k_wifi8_hal_rx_h_from_ds_qcn9625(rx_desc);
	rx_desc_data->is_to_ds = ath12k_wifi8_hal_rx_h_to_ds_qcn9625(rx_desc);
}

void ath12k_wifi8_hal_extract_rx_spd_data_qcn9625(struct hal_rx_spd_data *rx_info,
						  struct hal_rx_desc *rx_desc)
{
	__le32 flow_idx_info = rx_desc->u.qcn9625_compact.msdu_end.info8;

	rx_info->tlv_info.decap =
		ath12k_wifi8_hal_rx_h_decap_type_qcn9625(rx_desc);
	rx_info->tlv_info.mesh_ctrl_present =
		ath12k_wifi8_hal_rx_h_mesh_ctl_present_qcn9625(rx_desc);
	rx_info->tlv_info.freq = ath12k_wifi8_hal_rx_h_freq_qcn9625(rx_desc);
	rx_info->tlv_info.pkt_type =
		ath12k_wifi8_hal_rx_h_pkt_type_qcn9625(rx_desc);
	rx_info->tlv_info.bw = ath12k_wifi8_hal_rx_h_rx_bw_qcn9625(rx_desc);
	rx_info->tlv_info.rate_mcs =
		ath12k_wifi8_hal_rx_h_rate_mcs_qcn9625(rx_desc);
	rx_info->tlv_info.nss =
		hweight8(ath12k_wifi8_hal_rx_h_nss_qcn9625(rx_desc));
	rx_info->tlv_info.sgi = ath12k_wifi8_hal_rx_h_sgi_qcn9625(rx_desc);
	rx_info->tlv_info.is_decrypted =
		ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625(rx_desc);
	rx_info->tlv_info.encrypt_info_valid =
		ath12k_wifi8_hal_encrypt_valid_qcn9625(rx_desc);
	rx_info->tlv_info.mpdu_frame_control_valid =
		!!(le32_get_bits(rx_desc->u.qcn9625_compact.mpdu_start.info2,
				 RX_MPDU_INFO_INFO2_MPDU_FRAME_CONTROL_VALID));
	rx_info->tlv_info.encrypted =
		!!(le32_get_bits(rx_desc->u.qcn9625_compact.mpdu_start.info2,
				 RX_MPDU_INFO_INFO2_ENCRYPTED));
	rx_info->cce_metadata =
		__le16_to_cpu(rx_desc->u.qcn9625_compact.msdu_end.cce_metadata);
	rx_info->cce_match =
		le16_get_bits(rx_desc->u.qcn9625_compact.msdu_end.info8,
			      RX_MSDU_END_INFO8_CCE_MATCH);

	rx_info->rx_mpdu_info.flow_idx_timeout =
		le32_get_bits(flow_idx_info, RX_MSDU_END_INFO8_FLOW_IDX_TIMEOUT);
	rx_info->rx_mpdu_info.flow_idx_invalid =
		le32_get_bits(flow_idx_info, RX_MSDU_END_INFO8_FLOW_IDX_INVALID);
	rx_info->rx_mpdu_info.flow_info.flow_metadata =
		le16_get_bits(rx_desc->u.qcn9625_compact.msdu_end.fse_metadata,
			      ATH12K_DP_RX_FSE_FLOW_METADATA_MASK);
}

int ath12k_wifi8_hal_srng_create_config_qcn9625(struct ath12k_hal *hal)
{
	struct hal_srng_config *s;

	hal->srng_config = kmemdup(hw_srng_config_template,
				   sizeof(hw_srng_config_template),
				   GFP_KERNEL);
	if (!hal->srng_config)
		return -ENOMEM;

	s = &hal->srng_config[HAL_REO_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_HP;
	s->reg_size[0] = HAL_REO2_RING_BASE_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_REO2_RING_HP - HAL_REO1_RING_HP;

	/* REO2SW6 ring.*/
	s = &hal->srng_config[HAL_REO_EXCEPTION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW6_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW6_RING_HP;

	s = &hal->srng_config[HAL_REO_REINJECT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_HP;
	s->reg_size[0] = HAL_SW2REO1_RING_BASE_LSB(hal) - HAL_SW2REO_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_SW2REO1_RING_HP - HAL_SW2REO_RING_HP;

	s = &hal->srng_config[HAL_REO_DST_ROAMING];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW_ROAMING1_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW_ROAMING1_RING_HP;

	s = &hal->srng_config[HAL_REO_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_HP;
	s->reg_size[0]  = HAL_REO_CMD1_RING_BASE_LSB - HAL_REO_CMD_RING_BASE_LSB(hal);
	s->reg_size[1]  = HAL_REO_CMD1_HP - HAL_REO_CMD_HP;

	s = &hal->srng_config[HAL_REO_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_HP;

	s = &hal->srng_config[HAL_TQM_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_HOST_CMD_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_HOST_CMD_RING_HP;

	s = &hal->srng_config[HAL_TQM_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_HOST_STATUS_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_HOST_STATUS_RING_HP;

	s = &hal->srng_config[HAL_RXOLE_FSE_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_FSE_CMD_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_FSE_CMD_HP;

	s = &hal->srng_config[HAL_REO2PPE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2PPE_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2PPE_HP;
	s->reg_size[0] = HAL_REO2_RING_BASE_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_REO2_RING_HP - HAL_REO1_RING_HP;

	s = &hal->srng_config[HAL_TCL_DATA];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_HP;
	s->reg_size[0] = HAL_TCL2_RING_BASE_LSB(hal) - HAL_TCL1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_TCL2_RING_HP - HAL_TCL1_RING_HP;

	s = &hal->srng_config[HAL_CE_SRC];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG + HAL_CE_DST_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG + HAL_CE_DST_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG - HAL_SEQ_WCSS_UMAC_CE0_SRC_REG;
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG - HAL_SEQ_WCSS_UMAC_CE0_SRC_REG;

	s = &hal->srng_config[HAL_CE_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG + HAL_CE_DST_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG + HAL_CE_DST_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;

	s = &hal->srng_config[HAL_CE_DST_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG +
		HAL_CE_DST_STATUS_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG + HAL_CE_DST_STATUS_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG - HAL_SEQ_WCSS_UMAC_CE0_DST_REG;

	s = &hal->srng_config[HAL_WBM_IDLE_LINK];
	s->reg_start[0] =
		HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_HP;

	s = &hal->srng_config[HAL_SW2WBM_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
		HAL_WBM_SW_RELEASE_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW_RELEASE_RING_HP;
	s->reg_size[0] = HAL_WBM_FW_RELEASE_RING_BASE_LSB -
			 HAL_WBM_SW_RELEASE_RING_BASE_LSB;
	s->reg_size[1] = HAL_WBM_FW_RELEASE_RING_HP - HAL_WBM_SW_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_TX_COMPLETION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2SW0_RELEASE_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2SW0_RELEASE_RING_HP;
	s->reg_size[0] = HAL_TQM2SW1_RELEASE_RING_BASE_LSB -
			 HAL_TQM2SW0_RELEASE_RING_BASE_LSB;
	s->reg_size[1] = HAL_TQM2SW1_RELEASE_RING_HP - HAL_TQM2SW0_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_TCL_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_HP;

	/* SW2TCL CMD */
	s = &hal->srng_config[HAL_TCL_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_CMD_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_CMD_RING_HP;

	/* TCL2SW1_EXCEPTION */
	s = &hal->srng_config[HAL_TX_EXCEPTION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TX_EXCEPTION_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TX_EXCEPTION_RING_HP;

	/* SW2WBM_ASE_CMD_RING */
	s = &hal->srng_config[HAL_ASE_CMD_RING];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_SW2WBM_ASE_CMD_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_SW2WBM_ASE_CMD_RING_HP;

	s = &hal->srng_config[HAL_PEER_TX_TELEMETRY];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_PEER_TX_TELEMETRY_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_PEER_TX_TELEMETRY_RING_HP;

	s = &hal->srng_config[HAL_PEER_RX_TELEMETRY];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_PEER_RX_TELEMETRY_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_PEER_RX_TELEMETRY_RING_HP;

	/* Some LMAC rings are not accessed from the host:
	 * RXDMA_BUG, RXDMA_DST, RXDMA_MONITOR_BUF, RXDMA_MONITOR_STATUS,
	 * RXDMA_MONITOR_DST, RXDMA_MONITOR_DESC, RXDMA_DIR_BUF_SRC,
	 * RXDMA_RX_MONITOR_BUF, TX_MONITOR_BUF, TX_MONITOR_DST, SW2RXDMA
	 */
	s = &hal->srng_config[HAL_PPE2TCL];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_PPE2TCL1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_PPE2TCL1_RING_HP;

	s = &hal->srng_config[HAL_PPE_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG +
				HAL_TQM_PPE_RELEASE_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM_PPE_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_WBM_BUF];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_HP;
	s->reg_size[0] = HAL_WBM_SW2WBM_BUFF_RELEASE2_RING_BASE_LSB -
				HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_BASE_LSB;
	s->reg_size[1] = HAL_WBM_SW2WBM_BUFF_RELEASE2_RING_HP -
				HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_HP;

	s = &hal->srng_config[HAL_WBM_IDLE_BUF];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW_IDLE_BUF_RING_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW_IDLE_BUF_RING_HP;

	/* REO2SW8 MGMT Exception ring */
	s = &hal->srng_config[HAL_REO_EXCEPTION_MGMT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW8_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW8_RING_HP;

	/* REO2SW9 MGMT Destination ring */
	s = &hal->srng_config[HAL_REO_DST_MGMT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW9_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW9_RING_HP;

	/* REO2SW11 MGMT High-priority ring */
	s = &hal->srng_config[HAL_REO_DST_HIGH_PRIO];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW11_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO2SW11_RING_HP;

	/* MGMT WBM Buf ring */
	s = &hal->srng_config[HAL_WBM_BUF_MGMT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_SW2WBM_MGMT_BUFF_RELEASE1_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_SW2WBM_MGMT_BUFF_RELEASE1_RING_HP;

	/* MGMT WBM IDle Buf ring */
	s = &hal->srng_config[HAL_WBM_IDLE_BUF_MGMT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW_IDLE_MGMT_BUF_RING_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW_IDLE_MGMT_BUF_RING_HP;

	/* SAM cmd ring*/
	s = &hal->srng_config[HAL_SAM_CMD];
	s->reg_start[0] =  HAL_SEQ_WCSS_UMAC_SAM_REG + HAL_SAM_HOST_CMD_RING_BASE_LSB;
	s->reg_start[1] =  HAL_SEQ_WCSS_UMAC_SAM_REG + HAL_SAM_HOST_CMD_RING_HP;

	/* SAM status ring*/
	s = &hal->srng_config[HAL_SAM_STATUS];
	s->reg_start[0] =  HAL_SEQ_WCSS_UMAC_SAM_REG + HAL_SAM_HOST_STATUS_RING_BASE_LSB;
	s->reg_start[1] =  HAL_SEQ_WCSS_UMAC_SAM_REG + HAL_SAM_HOST_STATUS_RING_HP;

	/* PPE2WBM IDle Buf ring */
	s = &hal->srng_config[HAL_PPE2WBM_BUF];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_PPE2WBM_BUFF_RELEASE1_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				HAL_WBM_PPE2WBM_BUFF_RELEASE1_RING_HP;
	s->reg_size[0] = HAL_WBM_PPE2WBM_BUFF_RELEASE2_RING_BASE_LSB -
				HAL_WBM_PPE2WBM_BUFF_RELEASE1_RING_BASE_LSB;
	s->reg_size[1] = HAL_WBM_PPE2WBM_BUFF_RELEASE2_RING_HP -
				HAL_WBM_PPE2WBM_BUFF_RELEASE1_RING_HP;

	s = &hal->srng_config[HAL_PPE2WBM_IDLE_BUF];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_PPE2WBM_SW_IDLE_BUF_RING_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_PPE2WBM_SW_IDLE_BUF_RING_HP;

	/* Reo flush ring (REO2SW0)*/
	s = &hal->srng_config[HAL_REO_FLUSH];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_SW0_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_SW0_RING_HP;

	s = &hal->srng_config[HAL_TQM2PPE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2PPE_RELEASE_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2PPE_RELEASE_RING_HP;

	/* TQM2SW5 ring 5 for FW-owned buffer completions. */
	s = &hal->srng_config[HAL_TQM2SW_FW_COMPLETION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2SW5_RELEASE_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TQM_REG + HAL_TQM2SW5_RELEASE_RING_HP;

	return 0;
}

static const struct ath12k_hal_tcl_to_cmp_rbm_map
ath12k_wifi8_hal_tcl_to_cmp_rbm_map_qcn9625[] = {
	{
		.cmp_ring_num = 0,
		.rbm_id = HAL_RX_BUF_RBM_SW0_BM,
	},
	{
		.cmp_ring_num = 1,
		.rbm_id = HAL_RX_BUF_RBM_SW1_BM,
	},
	{
		.cmp_ring_num = 2,
		.rbm_id = HAL_RX_BUF_RBM_SW2_BM,
	},
	{
		.cmp_ring_num = 3,
		.rbm_id = HAL_RX_BUF_RBM_SW3_BM,
	},
	{
		.cmp_ring_num = 4,
		.rbm_id = HAL_RX_BUF_RBM_SW4_BM,
	}
};

#ifdef CPTCFG_EXT_IPA_OFFLOAD
static const struct ath12k_hal_rdi_mapping ath12k_wifi8_hal_rdi_mapping_qcn9625[] = {
	{ DESTINATION_RING_CTRL_SW0, SOURCE_RING_CTRL_SFE }, /* 0 - DP ERR */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 1 - DP REO1 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 2 - DP REO2 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 3 - DP REO3 */
	{ DESTINATION_RING_CTRL_SW4, SOURCE_RING_CTRL_SFE }, /* 4 - DP REO4 */
	{ DESTINATION_RING_CTRL_SW5, SOURCE_RING_CTRL_IPA }, /* 5 - IPA RING1 */
	{ DESTINATION_RING_CTRL_SW6, SOURCE_RING_CTRL_SFE }, /* 6 - DP REO6 */
	{ DESTINATION_RING_CTRL_SW7, SOURCE_RING_CTRL_SFE }, /* 7 - DP HP */
	{ DESTINATION_RING_CTRL_SW8, SOURCE_RING_CTRL_MGMT }, /* 8 - MGMT ERR */
	{ DESTINATION_RING_CTRL_SW9, SOURCE_RING_CTRL_MGMT}, /* 9 - MGMT RX */
	{ DESTINATION_RING_CTRL_SW11, SOURCE_RING_CTRL_MGMT}, /* 10 - MGMT HP */
	{ DESTINATION_RING_CTRL_PPE, SOURCE_RING_CTRL_PPE}, /* 11 - reo2ppe */
	{ DESTINATION_RING_CTRL_PPE1, SOURCE_RING_CTRL_PPE}, /* 12 - reo2ppe */
	{ DESTINATION_RING_CTRL_PPE2, SOURCE_RING_CTRL_PPE}, /* 13 - reo2ppe */
	{ 0, 0}, /* 14 - Reserved */
	{ 0, 0}, /* 15 - Reserved */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 16 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 17 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 18 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 19 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 20 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 21 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 22 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 23 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 24 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 25 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 26 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 27 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 28 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 29 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 30 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 31 */
};
#else
static const struct ath12k_hal_rdi_mapping ath12k_wifi8_hal_rdi_mapping_qcn9625[] = {
	{ DESTINATION_RING_CTRL_SW0, SOURCE_RING_CTRL_SFE }, /* 0 - DP ERR */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 1 - DP REO1 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 2 - DP REO2 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 3 - DP REO3 */
	{ DESTINATION_RING_CTRL_SW4, SOURCE_RING_CTRL_SFE }, /* 4 - DP REO4 */
	{ DESTINATION_RING_CTRL_SW5, SOURCE_RING_CTRL_SFE }, /* 5 - DP REO5 */
	{ DESTINATION_RING_CTRL_SW6, SOURCE_RING_CTRL_SFE }, /* 6 - DP REO6 */
	{ DESTINATION_RING_CTRL_SW7, SOURCE_RING_CTRL_SFE }, /* 7 - DP HP */
	{ DESTINATION_RING_CTRL_SW8, SOURCE_RING_CTRL_MGMT }, /* 8 - MGMT ERR */
	{ DESTINATION_RING_CTRL_SW9, SOURCE_RING_CTRL_MGMT}, /* 9 - MGMT RX */
	{ DESTINATION_RING_CTRL_SW11, SOURCE_RING_CTRL_MGMT}, /* 10 - MGMT HP */
	{ DESTINATION_RING_CTRL_PPE, SOURCE_RING_CTRL_PPE}, /* 11 - reo2ppe */
	{ DESTINATION_RING_CTRL_PPE1, SOURCE_RING_CTRL_PPE}, /* 12 - reo2ppe */
	{ DESTINATION_RING_CTRL_PPE2, SOURCE_RING_CTRL_PPE}, /* 13 - reo2ppe */
	{ 0, 0}, /* 14 - Reserved */
	{ 0, 0}, /* 15 - Reserved */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 16 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 17 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 18 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 19 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 20 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 21 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 22 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 23 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 24 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 25 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 26 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 27 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 28 */
	{ DESTINATION_RING_CTRL_SW2, SOURCE_RING_CTRL_SFE }, /* 29 */
	{ DESTINATION_RING_CTRL_SW3, SOURCE_RING_CTRL_SFE }, /* 30 */
	{ DESTINATION_RING_CTRL_SW1, SOURCE_RING_CTRL_SFE }, /* 31 */
};
#endif /* CPTCFG_EXT_IPA_OFFLOAD */

int ath12k_wifi8_hal_init_qcn9625(struct ath12k_hal *hal, u8 hw_version)
{
	struct ath12k_hal_wifi8 *hal_wifi8;


	switch (hw_version) {
	case ATH12K_HW_QCN9625_HW10:
	case ATH12K_HW_QCN9589_HW10:
		ath12k_wifi8_hal_init_v1_ops();
		hal->regs = &qcn9625_regs;
		hal->hal_params = &ath12k_wifi8_hw_hal_params_qcn9625;
		hal->hal_ops = &hal_qcn9625_v1_ops;
		hal->hal_desc_sz = ath12k_wifi8_hal_get_rx_desc_size_qcn9625_v1();
		hal->rx_mpdu_start_offset =
			ath12k_wifi8_hal_rx_desc_get_mpdu_start_offset_qcn9625_v1();
		hal->rx_msdu_end_offset =
			ath12k_wifi8_hal_rx_desc_get_msdu_end_offset_qcn9625_v1();
		hal->rx_mpdu_start_wmask =
				ath12k_wifi8_hal_rx_mpdu_start_wmask_get_qcn9625_v1();
		hal->rx_msdu_end_wmask =
				ath12k_wifi8_hal_rx_msdu_end_wmask_get_qcn9625_v1();
		break;
	case ATH12K_HW_QCN9625_HW20:
		hal->regs = &qcn9625_regs;
		hal->hal_params = &ath12k_wifi8_hw_hal_params_qcn9625;
		hal->hal_ops = &hal_qcn9625_ops;
		hal->hal_desc_sz = ath12k_wifi8_hal_get_rx_desc_size_qcn9625();
		hal->rx_mpdu_start_offset =
			ath12k_wifi8_hal_rx_desc_get_mpdu_start_offset_qcn9625();
		hal->rx_msdu_end_offset =
			ath12k_wifi8_hal_rx_desc_get_msdu_end_offset_qcn9625();
		hal->rx_mpdu_start_wmask =
				ath12k_wifi8_hal_rx_mpdu_start_wmask_get_qcn9625();
		hal->rx_msdu_end_wmask =
				ath12k_wifi8_hal_rx_msdu_end_wmask_get_qcn9625();
		break;
	default:
		return -EINVAL;
	}

	hal->tcl_to_cmp_rbm_map = ath12k_wifi8_hal_tcl_to_cmp_rbm_map_qcn9625;
	hal->rdi_mapping = ath12k_wifi8_hal_rdi_mapping_qcn9625;
	hal_wifi8 = kzalloc(sizeof(*hal_wifi8), GFP_KERNEL);
	if (!hal_wifi8)
		return -ENOMEM;

	hal_wifi8->reset_rings = qcn9625_reset_rings;
	hal->arch_data = hal_wifi8;

	return 0;
}

void ath12k_wifi8_hal_deinit_qcn9625(struct ath12k_hal *hal)
{
	kfree(hal->arch_data);
	hal->arch_data = NULL;
}

void ath12k_wifi8_hal_set_reg_writer_hptp_addr(struct ath12k_base *ab,
						      struct hal_srng *srng,
						      int mac_id,
						      int idx,
						      enum hal_ring_type ring_type)
{
	u32 offset = 0, address;

	switch (ring_type) {
	case HAL_RXDMA_MONITOR_BUF:
		offset = HAL_REG_WRITER_RXMON_SW2MON_BUF_RING;

		break;

	case HAL_RXDMA_MONITOR_DST:
		if (mac_id == 0)
			offset = HAL_REG_WRITER_RXMON_M0_MON2SW_DEST_RING;
		else
			offset = HAL_REG_WRITER_RXMON_M1_MON2SW_DEST_RING;
		break;

	case HAL_TX_MONITOR_BUF:
		offset = HAL_REG_WRITER_TXMON_SW2MON_BUF_RING;
		break;

	case HAL_TX_MONITOR_DST:
		if (mac_id == 0)
			offset = HAL_REG_WRITER_TXMON_M0_MON2SW_DEST_RING;
		else
			offset = HAL_REG_WRITER_TXMON_M1_MON2SW_DEST_RING;
		break;

	case HAL_ASE_STATUS_RING:
		offset = HAL_REG_WRITER_RXOLE2SW_ASE_DEST_RING;
		break;

	default:
		ath12k_err(ab, "Invalid ring_typ :%d ", ring_type);
			break;
	};

	address = HAL_REG_WRITER_VALUE_BASE_ADDR + offset + HAL_REG_WRITER_VALUE_OFFSET;

	if (srng->ring_dir == HAL_SRNG_DIR_SRC) {
		srng->u.src_ring.hp_addr = (u32 *)(uintptr_t)
			(address);

		ath12k_hif_write32(ab,
				   address,
				   0);
	} else {
		srng->u.dst_ring.tp_addr = (u32 *)(uintptr_t)
			(address);

		ath12k_hif_write32(ab,
				   address,
				   0);
	}
}

static bool ath12k_wifi8_hal_rx_reo_status(struct hal_reo_status *reo_status)
{
	return reo_status->tag == HAL_REO_GET_QUEUE_STATS_STATUS;
}

static bool ath12k_wifi8_hal_rx_reo_1k_status(struct hal_reo_status *reo_status)
{
	return reo_status->tag == HAL_REO_GET_QUEUE_1K_STATS_STATUS;
}

const struct hal_ops hal_qcn9625_ops = {
	.hal_init = ath12k_wifi8_hal_init_qcn9625,
	.hal_deinit = ath12k_wifi8_hal_deinit_qcn9625,
	.create_srng_config = ath12k_wifi8_hal_srng_create_config_qcn9625,
	.rx_desc_set_msdu_len = ath12k_wifi8_hal_rxdesc_set_msdu_len_qcn9625,
	.rx_desc_get_dot11_hdr = ath12k_wifi8_hal_rx_desc_get_dot11_hdr_qcn9625,
	.rx_desc_get_mpdu_frame_ctl =
		ath12k_wifi8_hal_rxdesc_get_mpdu_frame_ctrl_qcn9625,
	.rx_desc_get_crypto_header =
			ath12k_wifi8_hal_rx_desc_get_crypto_header_qcn9625,
	.rx_desc_copy_end_tlv = ath12k_wifi8_hal_rx_desc_end_tlv_copy_qcn9625,
	.rx_desc_get_msdu_src_link_id =
			ath12k_wifi8_hal_rx_get_msdu_src_link_qcn9625,
	.extract_rx_desc_data =
			ath12k_wifi8_hal_extract_rx_desc_data_qcn9625,
	.extract_rx_spd_data =
		ath12k_wifi8_hal_extract_rx_spd_data_qcn9625,
	.ce_dst_setup = ath12k_wifi8_hal_ce_dst_setup,
	.srng_src_hw_init = ath12k_wifi8_hal_srng_src_hw_init,
	.srng_dst_hw_init = ath12k_wifi8_hal_srng_dst_hw_init,
	.set_umac_srng_ptr_addr = ath12k_wifi8_hal_set_umac_srng_ptr_addr,
	.srng_update_shadow_config = ath12k_wifi8_hal_srng_update_shadow_config,
	.srng_get_ring_id = ath12k_wifi8_hal_srng_get_ring_id,
	.ce_get_desc_size = ath12k_wifi8_hal_ce_get_desc_size,
	.ce_src_set_desc = ath12k_wifi8_hal_ce_src_set_desc,
	.ce_dst_set_desc = ath12k_wifi8_hal_ce_dst_set_desc,
	.ce_dst_status_get_length = ath12k_wifi8_hal_ce_dst_status_get_length,
	.set_link_desc_addr = ath12k_wifi8_hal_set_link_desc_addr,
	.tx_set_dscp_tid_map = ath12k_wifi8_hal_tx_set_dscp_tid_map,
	.tx_update_dscp_tid_map = ath12k_wifi8_hal_tx_update_dscp_tid_map,
	.tx_set_pcp_tid_map = ath12k_wifi8_hal_tx_set_pcp_tid_map,
	.tx_set_tid_map_precedence = ath12k_wifi8_hal_tx_set_tid_map_precedence,
	.tx_configure_bank_register =
				ath12k_wifi8_hal_tx_configure_bank_register,
	.write_reoq_lut_addr = ath12k_wifi8_hal_write_reoq_lut_addr,
	.write_ml_reoq_lut_addr = ath12k_wifi8_hal_write_ml_reoq_lut_addr,
	.reoq_lut_addr_read_enable = ath12k_wifi8_hal_reoq_lut_addr_read_enable,
	.reoq_lut_set_max_peerid = ath12k_wifi8_hal_reoq_lut_set_max_peerid,
	.setup_link_idle_list = ath12k_wifi8_hal_setup_link_idle_list,
	.reo_qdesc_setup = ath12k_wifi8_hal_reo_qdesc_setup,
	.reo_init_cmd_ring = ath12k_wifi8_hal_reo_init_cmd_ring,
	.reo_hw_setup = ath12k_wifi8_hal_reo_hw_setup,
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	.reo_hw_setup_ipa = ath12k_wifi8_hal_reo_ring_ipa_ctrl_hash_ix0_setup,
#endif
	.cc_config = ath12k_wifi8_hal_cc_config,
	.srng_hw_disable = ath12k_wifi8_hal_srng_hw_disable,
	.reset_rx_reo_tid_q = ath12k_wifi8_hal_reset_rx_reo_tid_q,
	.get_idle_link_rbm = ath12k_wifi8_hal_get_idle_link_rbm,
	.reo_shared_qaddr_cache_clear = ath12k_wifi8_hal_reo_shared_qaddr_cache_clear,
	.hal_get_tsf2_scratch_reg = ath12k_hal_qcn9625_get_tsf2_scratch_reg,
	.hal_get_tqm_scratch_reg = ath12k_hal_qcn9625_get_tqm_scratch_reg,
	.rxdesc_get_mpdu_start_addr2 =
			ath12k_wifi8_hal_rxdesc_get_mpdu_start_addr2_qcn9625,
	.rx_h_is_decrypted = ath12k_wifi8_hal_rx_h_is_decrypted_qcn9625,
	.get_hw_hptp = ath12k_wifi8_hal_get_hw_hptp,
	.rx_h_peer_meta_data = ath12k_wifi8_hal_rx_h_peer_meta_data_qcn9625,
	.rx_desc_get_fse_info = ath12k_wifi8_hal_rx_desc_get_fse_info_qcn9625,
	.rx_get_fse_metadata = ath12k_wifi8_hal_rx_get_fse_metadata_qcn9625,
	.rx_get_cce_metadata = ath12k_wifi8_hal_rx_get_cce_metadata_qcn9625,
	.rx_get_cce_match_bit = ath12k_wifi8_hal_rx_get_cce_match_bit_qcn9625,
	.rx_get_flow_params = ath12k_wifi8_hal_rx_get_flow_params_qcn9625,
	.hal_tx_ppe2tcl_ring_halt_get = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_get,
	.hal_tx_ppe2tcl_ring_halt_set = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_set,
	.hal_tx_ppe2tcl_ring_halt_reset = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_reset,
	.hal_tx_ppe2tcl_ring_halt_done = ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_done,
	.hal_tx_config_rbm_mapping = ath12k_wifi8_hal_tx_config_rbm_mapping,
	.hal_ppeds_cfg_ast_override_map_reg =
			ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg,
	.hal_reo_config_reo2ppe_dest_info = ath12k_wifi8_hal_reo_config_reo2ppe_dest_info,
	.hal_tx_set_ppe_vp_entry = ath12k_wifi8_hal_tx_set_ppe_vp_entry,
	.hal_get_tlv_tag_params = ath12k_wifi8_get_tlv_tag_params,
	.hal_mon_ops_init = ath12k_wifi8_hal_mon_ops_init,
	.hal_srng_idx_update_addr = ath12k_wifi8_hal_srng_idx_update_addr,
	.hal_ppeds_reo2ppe_cc_config = ath12k_wifi8_hal_ppeds_reo2ppe_cc_config,
	.hal_set_reg_writer_hptp_addr = ath12k_wifi8_hal_set_reg_writer_hptp_addr,
	.hal_rx_reo_status = ath12k_wifi8_hal_rx_reo_status,
	.hal_rx_reo_1k_status = ath12k_wifi8_hal_rx_reo_1k_status,
};
