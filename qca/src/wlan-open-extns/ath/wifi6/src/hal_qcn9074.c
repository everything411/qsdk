// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include "hal_desc.h"
#include "hal_rx_desc.h"
#include "hal_qcn9074.h"
#include "hw.h"
#include "hal.h"
#include "hal_tx.h"
#include <linux/cacheflush.h>

void ath12k_wifi6_hal_mon_ops_init(struct ath12k_hal *hal,
				   u8 hw_version);

static const struct hal_srng_config hw_srng_config_template[] = {
	/* TODO: max_rings can populated by querying HW capabilities */
	[HAL_REO_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW1,
		.max_rings = 4,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_dst",
	},
	[HAL_REO_EXCEPTION] = {
		/* Designating REO2SW0 ring as exception ring.
		 * Any of theREO2SW rings can be used as exception ring.
		 */
		.start_ring_id = HAL_SRNG_RING_ID_REO2SW0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_dest_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_exception",
	},
	[HAL_REO_REINJECT] = {
		.start_ring_id = HAL_SRNG_RING_ID_SW2REO,
		.max_rings = 4,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_SW2REO_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_reinject",
	},
	[HAL_REO_CMD] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO_CMD,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			sizeof(struct hal_reo_get_queue_stats)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_REO_CMD_RING_BASE_MSB_RING_SIZE,
		.name = "Reo_cmd",
	},
	[HAL_REO_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_REO_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
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
		.max_size = HAL_SW2TCL1_CMD_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_TCL_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_TCL_STATUS,
		.max_rings = 1,
		.entry_size = (sizeof(struct hal_tlv_hdr) +
			     sizeof(struct hal_tcl_status_ring)) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_TCL_STATUS_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_SRC] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_SRC,
		.max_rings = 16,
		.entry_size = sizeof(struct hal_ce_srng_src_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_SRC_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST,
		.max_rings = 16,
		.entry_size = sizeof(struct hal_ce_srng_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_CE_DST_RING_BASE_MSB_RING_SIZE,
	},
	[HAL_CE_DST_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_CE0_DST_STATUS,
		.max_rings = 16,
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
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_SW2WBM_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "sw2wbm_release",
	},
	[HAL_WBM2SW_RELEASE] = {
		.start_ring_id = HAL_SRNG_RING_ID_WBM2SW0_RELEASE,
		.max_rings = 5,
		.entry_size = sizeof(struct hal_wbm_release_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_UMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_WBM2SW_RELEASE_RING_BASE_MSB_RING_SIZE,
		.name = "wbm2sw_release",
	},
	[HAL_RXDMA_BUF] = {
		.start_ring_id = HAL_SRNG_SW2RXDMA_BUF0,
		.max_rings = 2,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_DMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
		.name = "Rxdma_buf",
	},
	[HAL_RXDMA_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_RXDMA2SW0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
		.name = "Rxdma_dst",
	},
	[HAL_RXDMA_MONITOR_BUF] = {
		.start_ring_id = HAL_SRNG_SW2RXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
		.name = "Rxdma_monitor_buf",
	},
	[HAL_RXDMA_MONITOR_STATUS] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_STATBUF,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
		.name = "Rxdma_monitor_status",
	},
	[HAL_RXDMA_MONITOR_DESC] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_DESC,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_wbm_buffer_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
		.name = "Rxdma_monitor_desc",
	},
	[HAL_RXDMA_DIR_BUF] = {
		.start_ring_id = HAL_SRNG_RING_ID_RXDMA_DIR_BUF,
		.max_rings = 2,
		.entry_size = 8 >> 2, /* TODO: Define the struct */
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_SRC,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
	},
	[HAL_PPE2TCL] = {
		.start_ring_id = HAL_SRNG_RING_ID_PPE2TCL1,
		.max_rings = 1,
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
	},
	[HAL_RXDMA_MONITOR_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_SW2RXMON_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_reo_entrance_ring) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE,
		.name = "Rxdma_monitor_dst",
	},
	[HAL_TX_MONITOR_DST] = {
		.start_ring_id = HAL_SRNG_RING_ID_WMAC1_TXMON2SW0_BUF0,
		.max_rings = 1,
		.entry_size = sizeof(struct hal_mon_dest_desc) >> 2,
		.mac_type = ATH12K_HAL_SRNG_PMAC,
		.ring_dir = HAL_SRNG_DIR_DST,
		.max_size = HAL_RXDMA_RING_MAX_SIZE_BE,
	}
};

const struct ath12k_hw_regs qcn9074_regs = {
	/* SW2TCL(x) R0 ring configuration address */
	.hal_tcl1_ring_id = 0x000004f8, //WCSS_UMAC_TCL_R0_SW2TCL1_RING_ID | 0xA444F8
	.hal_tcl1_ring_misc = 0x00000500, //WCSS_UMAC_TCL_R0_SW2TCL1_RING_MISC | 0xA44500
	.hal_tcl1_ring_tp_addr_lsb = 0x0000050c,
		/* WCSS_UMAC_TCL_R0_SW2TCL1_RING_TP_ADDR_LSB | 0xA4450C */
	.hal_tcl1_ring_tp_addr_msb = 0x00000510,
		/* WCSS_UMAC_TCL_R0_SW2TCL1_RING_TP_ADDR_MSB | 0xA44510 */
	.hal_tcl1_ring_consumer_int_setup_ix0 = 0x00000520,
		/* WCSS_UMAC_TCL_R0_SW2TCL1_RING_CONSUMER_INT_SETUP_IX0 */
		/* 0xA44520 */
	.hal_tcl1_ring_consumer_int_setup_ix1 = 0x00000524,
		/* WCSS_UMAC_TCL_R0_SW2TCL1_RING_CONSUMER_INT_SETUP_IX1 */
		/* 0xA44524 */
	.hal_tcl1_ring_msi1_base_lsb = 0x00000538, // FW address 0xA44538 confirmed ipcat
	.hal_tcl1_ring_msi1_base_msb = 0x0000053c,
		/* WCSS_UMAC_TCL_R0_SW2TCL1_RING_MSI1_BASE_MSB | 0xA4453C */
	.hal_tcl1_ring_msi1_data = 0x00000540,
		/* WCSS_UMAC_TCL_R0_SW2TCL1_RING_MSI1_DATA | 0xA44540 */
	.hal_tcl_ring_base_lsb = 0x000005f8,
		/* WCSS_UMAC_TCL_R0_SW2TCL_CREDIT_RING_BASE_LSB | 0xA445F8 */
	.hal_tcl1_ring_base_lsb = 0x000004f0, //
	.hal_tcl1_ring_base_msb = 0x000004f4,
	.hal_tcl2_ring_base_lsb = 0x00000548,

	/* TCL STATUS ring address */
	.hal_tcl_status_ring_base_lsb = 0x00000700,

	/* WBM idle link ring address */
	.hal_wbm_idle_ring_base_lsb = 0x00000874,
	.hal_wbm_idle_ring_misc_addr = 0x00000884,
	.hal_wbm_r0_idle_list_cntl_addr = 0x00000048,
	.hal_wbm_r0_idle_list_size_addr = 0x00000244,
	.hal_wbm_scattered_ring_base_lsb = 0x00000058,
	.hal_wbm_scattered_ring_base_msb = 0x0000005c,
	.hal_wbm_scattered_desc_head_info_ix0 = 0x00000068,
	.hal_wbm_scattered_desc_head_info_ix1 = 0x0000006c,
	.hal_wbm_scattered_desc_tail_info_ix0 = 0x00000078,
	.hal_wbm_scattered_desc_tail_info_ix1 = 0x0000007c,
	.hal_wbm_scattered_desc_ptr_hp_addr = 0x00000084,

	/* SW2WBM release ring address */
	.hal_wbm_sw_release_ring_base_lsb = 0x000001ec,
	.hal_wbm_sw1_release_ring_base_lsb = 0x00000244,

	/* WBM2SW release ring address */
	.hal_wbm0_release_ring_base_lsb = 0x00000924,
	.hal_wbm1_release_ring_base_lsb = 0x0000097c,

	/* PCIe base address */
	.pcie_qserdes_sysclk_en_sel = 0x01e0e0a8,
	.pcie_pcs_osc_dtct_config_base = 0x01e0f45c,

	/*PCIe hot reset reg*/
	.pcie_gcc_gcc_pcie_hot_rst = 0x1e38338,

	/*PCIe qrtr node id reg*/
	.pcie_pcie_local_qrtr_ins_reg = 0x1E03164,

	/* REO DEST ring address */
	.hal_reo2_ring_base = 0x000002f4,
	.hal_reo1_misc_ctrl_addr = 0x000002ac,
	.hal_reo1_sw_cookie_cfg0 = 0x0,
	.hal_reo1_sw_cookie_cfg1 = 0x0,
	.hal_reo1_qdesc_lut_base0 = 0x0,
	.hal_reo1_qdesc_lut_base1 = 0x0,
	.hal_reo1_qdesc_addr = 0x0,
	.hal_reo1_qdesc_max_peerid = 0x0,
	.hal_reo1_ring_base_lsb = 0x0000029c,
	.hal_reo1_ring_base_msb = 0x000002a0,
	.hal_reo1_ring_id = 0x000002a4,
	.hal_reo1_ring_misc = 0x000002ac,
	.hal_reo1_ring_hp_addr_lsb = 0x000002b0,
	.hal_reo1_ring_hp_addr_msb = 0x000002b4,
	.hal_reo1_ring_producer_int_setup = 0x000002c0,
	.hal_reo1_ring_msi1_base_lsb = 0x000002e4,
	.hal_reo1_ring_msi1_base_msb = 0x000002e8,
	.hal_reo1_ring_msi1_data = 0x000002ec,
	.hal_reo1_aging_thres_ix0 = 0x00000564,
	.hal_reo1_aging_thres_ix1 = 0x00000568,
	.hal_reo1_aging_thres_ix2 = 0x0000056c,
	.hal_reo1_aging_thres_ix3 = 0x00000570,

	/* REO Exception ring address */
	.hal_reo2_sw0_ring_base = 0x000003fc,

	/* REO Reinject ring address */
	.hal_sw2reo_ring_base = 0x000001ec,
	.hal_sw2reo1_ring_base = 0x00000244,

	/* REO cmd ring address */
	.hal_reo_cmd_ring_base = 0x00000194,

	/* REO status ring address */
	.hal_reo_status_ring_base = 0x00000504,
		/* WCSS_UMAC_REO_R0_REO_STATUS_RING_BASE_LSB | 0xA38504 */

	/* CE base address */
	.hal_umac_ce0_src_reg_base = 0x01b80000,
		/* CE_CE_0_SRC_WFSS_CE_CHANNEL_SRC_R0_SRC_RING_BASE_LSB */
		/* 0x1B80000 */
	.hal_umac_ce0_dest_reg_base = 0x01b81000,
		/* CE_CE_0_DST_WFSS_CE_CHANNEL_DST_R0_DEST_RING_BASE_LSB */
		/* 0x1B81000 */
	.hal_umac_ce1_src_reg_base = 0x01b82000,
		/* CE_CE_1_SRC_WFSS_CE_CHANNEL_SRC_R0_SRC_RING_BASE_LSB */
		/* 0x1B82000 */
	.hal_umac_ce1_dest_reg_base = 0x01b83000,
		/* CE_CE_1_DST_WFSS_CE_CHANNEL_DST_R0_DEST_RING_BASE_LSB */
		/* 0x1B83000 */

	.hal_ppe_rel_ring_base = 0x0, //WCSS_UMAC_WBM_R0_SW1_RELEASE_RING_MSI1_BASE_LSB
	.hal_reo2ppe_ring_base = 0x0, //WCSS_UMAC_REO_R0_REO2PPE_RING_BASE_LSB
	.hal_tcl_ppe2tcl_ring_base_lsb = 0x0, //WCSS_UMAC_TCL_R0_PPE2TCL1_RING_BASE_LSB

	/* PMM register base address */
	.hal_pmm_reg_base = 0x00B500FC,
};

const struct ath12k_hw_hal_params ath12k_wifi6_hw_hal_params_qcn9074 = {
	.rx_buf_rbm = HAL_RX_BUF_RBM_SW3_BM,
	.wbm2sw_cc_enable = HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW0_EN |
			    HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW1_EN |
			    HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW2_EN |
			    HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW3_EN |
			    HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW4_EN |
			    HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW5_EN |
			    HAL_WBM_SW_COOKIE_CONV_CFG_WBM2SW6_EN,
	.link_desc_size = HAL_LINK_DESC_SIZE,
	.num_mpdus_per_link_desc = HAL_NUM_MPDUS_PER_LINK_DESC,
	.num_tx_msdus_per_link_desc = HAL_NUM_TX_MSDUS_PER_LINK_DESC,
	.num_rx_msdus_per_link_desc = HAL_NUM_RX_MSDUS_PER_LINK_DESC,
	.num_mpdu_links_per_queue_desc = HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC,
	.rx_mon_err_dst_ring_size = HAL_RXDMA_ERR_DST_RING_SIZE,
	.rx_mon_refill_ring_size = HAL_RX_MON_REFILL_RING_SIZE,
	.rx_refill_ring_size = HAL_RX_REFILL_RING_SIZE,
};

static int ath12k_wifi6_hal_srng_create_config_qcn9074(struct ath12k_hal *hal)
{

	struct hal_srng_config *s;

	hal->srng_config = kmemdup(hw_srng_config_template,
				   sizeof(hw_srng_config_template),
				   GFP_KERNEL);
	if (!hal->srng_config)
		return -ENOMEM;

	s = &hal->srng_config[HAL_REO_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO1_RING_HP_QCN9074;
	s->reg_size[0] = HAL_REO2_RING_BASE_LSB(hal) - HAL_REO1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_REO2_RING_HP_QCN9074 - HAL_REO1_RING_HP_QCN9074;

	s = &hal->srng_config[HAL_REO_EXCEPTION];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_SW0_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_SW0_RING_HP_QCN9074;

	s = &hal->srng_config[HAL_REO_REINJECT];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_SW2REO_RING_HP;

	s = &hal->srng_config[HAL_REO_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_HP;

	s = &hal->srng_config[HAL_REO_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_STATUS_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_REO_REG + HAL_REO_CMD_HP_QCN9074;

	s = &hal->srng_config[HAL_TCL_DATA];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL1_RING_HP;
	s->reg_size[0] = HAL_TCL2_RING_BASE_LSB(hal) - HAL_TCL1_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_TCL2_RING_HP - HAL_TCL1_RING_HP;

	s = &hal->srng_config[HAL_TCL_CMD];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_RING_HP_QCN9074;

	s = &hal->srng_config[HAL_TCL_STATUS];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_STATUS_RING_HP_QCN9074;

	s = &hal->srng_config[HAL_CE_SRC];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(hal) + HAL_CE_DST_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(hal) + HAL_CE_DST_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG(hal) -
		HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(hal);
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_SRC_REG(hal) -
		HAL_SEQ_WCSS_UMAC_CE0_SRC_REG(hal);

	s = &hal->srng_config[HAL_CE_DST];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal) + HAL_CE_DST_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal) + HAL_CE_DST_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(hal) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal);
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(hal) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal);
	s = &hal->srng_config[HAL_CE_DST_STATUS];

	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal) +
		HAL_CE_DST_STATUS_RING_BASE_LSB;
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal) + HAL_CE_DST_STATUS_RING_HP;
	s->reg_size[0] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(hal) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal);
	s->reg_size[1] = HAL_SEQ_WCSS_UMAC_CE1_DST_REG(hal) -
		HAL_SEQ_WCSS_UMAC_CE0_DST_REG(hal);

	s = &hal->srng_config[HAL_WBM_IDLE_LINK];
		s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				      HAL_WBM_IDLE_LINK_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_IDLE_LINK_RING_HP_QCN9074;

	s = &hal->srng_config[HAL_SW2WBM_RELEASE];
		s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG +
				      HAL_WBM_SW_RELEASE_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM_SW1_RELEASE_RING_HP;

	s = &hal->srng_config[HAL_WBM2SW_RELEASE];
	s->reg_start[0] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM0_RELEASE_RING_BASE_LSB(hal);
	s->reg_start[1] = HAL_SEQ_WCSS_UMAC_WBM_REG + HAL_WBM0_RELEASE_RING_HP_QCN9074;
	s->reg_size[0] = HAL_WBM1_RELEASE_RING_BASE_LSB(hal) -
		HAL_WBM0_RELEASE_RING_BASE_LSB(hal);
	s->reg_size[1] = HAL_WBM1_RELEASE_RING_HP - HAL_WBM0_RELEASE_RING_HP_QCN9074;

	return 0;
}

static const struct ath12k_hal_tcl_to_cmp_rbm_map
ath12k_wifi6_hal_tcl_to_cmp_rbm_map_qcn9274[DP_TCL_NUM_RING_MAX] = {
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
	}
};

static int ath12k_wifi6_hal_init_qcn9074(struct ath12k_hal *hal, u8 hw_version)
{

	hal->regs = ath12k_wifi6_hw_ver_map[hw_version].hw_regs;
	hal->tcl_to_cmp_rbm_map = ath12k_wifi6_hal_tcl_to_cmp_rbm_map_qcn9274;
	hal->hal_ops = &hal_qcn9074_ops;
	hal->hal_desc_sz = ath12k_wifi6_hal_get_rx_desc_size_qcn9074();
	hal->hal_params = ath12k_wifi6_hw_ver_map[hw_version].hal_params;

	return 0;
}

const struct hal_ops hal_qcn9074_ops = {
	.hal_init = ath12k_wifi6_hal_init_qcn9074,
	.create_srng_config = ath12k_wifi6_hal_srng_create_config_qcn9074,
	.srng_src_hw_init = ath12k_wifi6_hal_srng_src_hw_init,
	.srng_dst_hw_init = ath12k_wifi6_hal_srng_dst_hw_init,
	.set_umac_srng_ptr_addr = ath12k_wifi6_hal_set_umac_srng_ptr_addr,
	.srng_update_shadow_config = ath12k_wifi6_hal_srng_update_shadow_config,
	.srng_get_ring_id = ath12k_wifi6_hal_srng_get_ring_id,
	.srng_hw_disable = ath12k_wifi6_hal_srng_hw_disable,

	.ce_dst_setup = ath12k_wifi6_hal_ce_dst_setup,
	.ce_get_desc_size = ath12k_wifi6_hal_ce_get_desc_size,
	.ce_src_set_desc = ath12k_wifi6_hal_ce_src_set_desc,
	.ce_dst_set_desc = ath12k_wifi6_hal_ce_dst_set_desc,
	.ce_dst_status_get_length = ath12k_wifi6_hal_ce_dst_status_get_length,
	.set_link_desc_addr = ath12k_wifi6_hal_set_link_desc_addr,
	.setup_link_idle_list = ath12k_wifi6_hal_setup_link_idle_list,
	.cc_config = ath12k_wifi6_hal_cc_config,
	.get_idle_link_rbm = ath12k_wifi6_hal_get_idle_link_rbm,
	.hal_mon_ops_init = ath12k_wifi6_hal_mon_ops_init,
	.get_hw_hptp = ath12k_wifi6_hal_get_hw_hptp,
	.rx_msdu_list_get = ath12k_wifi6_hal_rx_msdu_list_get,
	.rx_desc_get_mpdu_start_tag = ath12k_wifi6_hal_rx_desc_get_mpdu_start_tag_qcn9074,
};
