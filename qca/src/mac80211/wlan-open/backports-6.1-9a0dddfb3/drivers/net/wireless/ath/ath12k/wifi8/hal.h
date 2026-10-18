/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef ATH12K_HAL_WIFI8_H
#define ATH12K_HAL_WIFI8_H

#include "../hal.h"
#include "hal_desc.h"
#include "hal_tx.h"
#include "hal_rx.h"
#include "hal_rx_desc.h"
#include "umac_reset.h"
#include "../dp_tx.h"

extern const struct ath12k_hw_regs qcn9625_regs;

extern const struct ath12k_hw_hal_params ath12k_wifi8_hw_hal_params_qcn9625;

/* Peer Metadata classification */
/* Version 0 */
#define RX_MPDU_DESC_META_DATA_V0_PEER_ID_WIFI8		GENMASK(11, 0)
/* Version 1 */
#define RX_MPDU_DESC_META_DATA_V1_PEER_ID_WIFI8		GENMASK(11, 0)
/* Version 1A */
#define RX_MPDU_DESC_META_DATA_V1A_PEER_ID_WIFI8	GENMASK(11, 0)
/* Version 1B */
#define RX_MPDU_DESC_META_DATA_V1B_PEER_ID_WIFI8	GENMASK(11, 0)

#define PCIE_WINDOW_REG_ADDRESS			0x3278
#define WINDOW_VALUE_MASK			GENMASK(25, 19)
#define WINDOW_STATIC_MASK			GENMASK(31, 7)
#define WINDOW_DYNAMIC_MASK			GENMASK(6, 0)
#define CE_WINDOW_SHIFT				7
#define UMAC_WINDOW_SHIFT			14

/* Total size of the LUT is based on 2K peers, each having reference
 * for 20 tids (17 for DP, 2 for mgmt, and 1 shared between DP and mgmt),
 * note each entry is of type ath12k_reo_queue_ref
 * hence total size is 2048 * 20 * 8 = 327680
 */
#define HAL_WIFI8_REOQ_LUT_SIZE       327680
#define HAL_WIFI8_NUM_TIDS     20

#define HAL_WIFI8_DP_RX_ERR_RDI 6

#define HAL_DSCP_TID_MAP_TBL_NUM_ENTRIES_MAX    128
#define HAL_DSCP_TID_TBL_SIZE                   24

#define HAL_MGMT_UCAST_TID HAL_NON_QOS_TID
#define HAL_MGMT_BCAST_TID 18
#define HAL_MGMT_SENSING_TID 19

/*
 * PPE_VP_CONFIG_TABLE DETAILS
 */
#define HAL_TX_PPE_VP_CFG_WILDCARD_LMAC_ID 3
#define HAL_TX_PPE_VP_CFG_ENTRY_VALID           BIT(0)
#define HAL_TX_PPE_VP_CFG_LMAC_ID               GENMASK(3, 1)
#define HAL_TX_PPE_VP_CFG_BANK_ID               GENMASK(11, 4)
#define HAL_TX_PPE_VP_CFG_VDEV_ID               GENMASK(19, 12)
#define HAL_TX_PPE_VP_CFG_SRCH_IDX_REG_NUM      GENMASK(25, 20)
#define HAL_TX_PPE_VP_CFG_USE_PPE_INT_PRI       BIT(26)
#define HAL_TX_PPE_VP_CFG_TO_FW                 BIT(27)
#define HAL_TX_PPE_VP_CFG_DROP_PREC_EN          BIT(28)

struct ath12k_hal_wifi8 {
	const struct ath12k_hal_reset_rings *reset_rings;
	struct ath12k_cumac_hw_reset_timestamps ssr_ts;
	struct ath12k_cumac_hw_reset_errors reset_errors;
};

static inline struct ath12k_hal_wifi8 *ath12k_get_hal_wifi8(struct ath12k_hal *hal)
{
	return (struct ath12k_hal_wifi8 *)hal->arch_data;
}

enum rdi_based_destination_ring_selection {
	DESTINATION_RING_CTRL_SW0,
	DESTINATION_RING_CTRL_SW1,
	DESTINATION_RING_CTRL_SW2,
	DESTINATION_RING_CTRL_SW3,
	DESTINATION_RING_CTRL_SW4,
	DESTINATION_RING_CTRL_RELEASE,
	DESTINATION_RING_CTRL_FW,
	DESTINATION_RING_CTRL_SW5,
	DESTINATION_RING_CTRL_SW6,
	DESTINATION_RING_CTRL_SW7,
	DESTINATION_RING_CTRL_SW8,
	DESTINATION_RING_CTRL_SW9,
	DESTINATION_RING_CTRL_PPE,
	DESTINATION_RING_CTRL_PPE1,
	DESTINATION_RING_CTRL_PPE2,
	DESTINATION_RING_CTRL_FW_MGMT,
	DESTINATION_RING_CTRL_SW10,
	DESTINATION_RING_CTRL_SW11,
};

enum rdi_based_source_ring_selection {
	SOURCE_RING_CTRL_SFE,
	SOURCE_RING_CTRL_FW1,
	SOURCE_RING_CTRL_PPE,
	SOURCE_RING_CTRL_UNUSED,
	SOURCE_RING_CTRL_MGMT,
	SOURCE_RING_CTRL_FW2,
#ifdef CPTCFG_EXT_IPA_OFFLOAD
	SOURCE_RING_CTRL_IPA,
#endif
};

/* RDI value assigned to IPA ring (reo2sw5) — must match
 * ATH12K_DP_WIFI8_REO2SW5_RDI in wlan-open-extns/ath/wifi8/src/ipa/dp_ipa.h
 */
#ifdef CPTCFG_EXT_IPA_OFFLOAD
#define HAL_WIFI8_IPA_REO_RDI		5
#endif

/* calculate the register address from bar0 of shadow register x */
#define HAL_SHADOW_BASE_ADDR			0x000008fc
#define HAL_SHADOW_NUM_REGS			40
#define HAL_HP_OFFSET_IN_REG_START		1
#define HAL_OFFSET_FROM_HP_TO_TP		4

#define HAL_SHADOW_REG(x) (HAL_SHADOW_BASE_ADDR + (4 * (x)))
#define HAL_REO_QDESC_MAX_PEERID               8191

#define HAL_LINK_DESC_SIZE			(32 << 3)
#define HAL_NUM_MPDUS_PER_LINK_DESC		12
#define HAL_NUM_TX_MSDUS_PER_LINK_DESC		11
#define HAL_NUM_RX_MSDUS_PER_LINK_DESC		11
#define HAL_NUM_MPDU_LINKS_PER_QUEUE_DESC	30

/* UMAC Relative address */
#define HAL_SEQ_WCSS_UMAC_OFFSET		0xf00000
#define HAL_SEQ_WCSS_UMAC_REO_REG		0xf22000
#define HAL_SEQ_WCSS_UMAC_TCL_REG		0xf13000
#define HAL_SEQ_WCSS_UMAC_TQM_REG		0xf08000
#define HAL_SEQ_WCSS_UMAC_WBM_REG		0xf04000
#define HAL_SEQ_WCSS_UMAC_SAM_REG		0xf28000
#define HAL_SEQ_WCSS_UMAC_MXI_REG		HAL_SEQ_WCSS_UMAC_OFFSET
#define HAL_SEQ_WCSS_UMAC_UMCMN_REG		0xf1e000

/* DMAC Relative address */
#define HAL_SEQ_WCSS_DMAC_MXI_REG		0xe00000
#define HAL_SEQ_WCSS_DMAC_DMCMN_REG		0xe03000
#define HAL_SEQ_WCSS_DMAC_RXDMA_REG		0xe42000
#define HAL_SEQ_WCSS_DMAC_RXOLE_REG		0xe1e000

/* PMAC Relative address */
#define HAL_SEQ_WCSS_PMAC_PMCMN_REG		{0xf49000, 0xf89000}
#define HAL_SEQ_WCSS_PMAC_MXI_REG		{0xf43000, 0xf83000}
#define HAL_SEQ_WCSS_PMAC_HWMLO_REG		{0xf65000, 0xfa5000}
#define HAL_SEQ_WCSS_PMAC_RXPCU_REG		{0xf4c000, 0xf8c000}
#define HAL_SEQ_WCSS_PMAC_HWSCH_REG		{0xf59000, 0xf99000}

#define HAL_SEQ_WCSS_MAC_WSIB_REG		0x10c0000
#define HAL_SEQ_WCSS_WFSS_CC_REG		0x10e4000

#define HAL_SEQ_WCSS_UMAC_CE0_SRC_REG	0x1B80000
#define HAL_SEQ_WCSS_UMAC_CE0_DST_REG	0x1B81000
#define HAL_SEQ_WCSS_UMAC_CE1_SRC_REG	0x1B82000
#define HAL_SEQ_WCSS_UMAC_CE1_DST_REG	0x1B83000

#define HAL_CE_WFSS_CE_REG_BASE                 0x1B80000
#define HAL_DP_REG_WINDOW_OFFSET                0x00180000

/*
 * TCL register offsets for PCP-TID map and TID map precedence.
 * These are relative to HAL_SEQ_WCSS_UMAC_TCL_REG.
 */
#define HAL_TCL_R0_PCP_TID_MAP_OFFSET		0x137C
#define HAL_TCL_R0_TID_MAP_PRTY_OFFSET		0x13CC

#define HAL_TCL_R0_PCP_TID_MAP_ADDR \
	(HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_R0_PCP_TID_MAP_OFFSET)
#define HAL_TCL_R0_PCP_TID_MAP_PRTY_OFFSET \
	(HAL_SEQ_WCSS_UMAC_TCL_REG + HAL_TCL_R0_TID_MAP_PRTY_OFFSET)

/* wifi8 TID map precedence register field masks
 * Register: UMAC_TCL_R0_TID_PCP_DSCP_PRIORITY (offset 0x13CC from TCL base)
 *   Bit  0     : VAL  - TID priority order (0=DSCP>PCP, 1=PCP>DSCP)
 *   Bits 3:1   : TID_DEF - Default TID when no TID is valid for an MSDU
 */
#define HAL_TCL_TID_MAP_PRTY_VAL_MASK		GENMASK(0, 0)
#define HAL_TCL_TID_MAP_PRTY_TID_DEF_MASK	GENMASK(3, 1)

/* PCP-TID map register bit-field masks (3 bits per PCP value, PCP0 at [2:0]) */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_0		GENMASK(2, 0)	/* 0x7      */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_1		GENMASK(5, 3)	/* 0x38     */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_2		GENMASK(8, 6)	/* 0x1c0    */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_3		GENMASK(11, 9)	/* 0xe00    */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_4		GENMASK(14, 12)	/* 0x7000   */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_5		GENMASK(17, 15)	/* 0x38000  */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_6		GENMASK(20, 18)	/* 0x1c0000 */
#define HAL_TCL_R0_PCP_TID_MAP_PCP_7		GENMASK(23, 21)	/* 0xe00000 */

#define HAL_TCL_SW_CONFIG_BANK_ADDR		0x00F130E0

/* To set mcast pkt ctrl vlaues */
#define HAL_TCL_R0_VDEV_MCAST_PACKET_CTRL_MAP_n_ADDR(reg_idx) \
	(0x00F132E4 + (0x4 * (reg_idx)))
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_REG_ID(vdev_id) ((vdev_id) >> 0x3)
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_INDEX_IN_REG(vdev_id) ((vdev_id) & 0x7)
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_MASK 0x7
#define HAL_TCL_VDEV_MCAST_PACKET_CTRL_SHIFT 0x3

/*TODO: Revisit Default value */
#define HAL_TCL_SW_CONFIG_BANK_DEFAULT_VAL	0x3843c
#define HAL_TCL_SW_CONFIG_BANK_DEFAULT		0x00F132E0

#define HAL_TCL_LINK_ID_TO_CHIP_ID_MAP		0x9c

/* SW2TCL(x) R0 ring configuration address */
#define HAL_TCL1_RING_CMN_CTRL_REG		0x30
#define HAL_TCL1_RING_DSCP_TID_MAP		0x77C
#define HAL_TCL1_RING_HALT_STAT_REG		0x34
#define HAL_TCL1_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl1_ring_base_lsb)
#define HAL_TCL1_RING_BASE_MSB(hal) \
	((hal)->regs->hal_tcl1_ring_base_msb)
#define HAL_TCL1_RING_ID(hal)			((hal)->regs->hal_tcl1_ring_id)
#define HAL_TCL1_RING_MISC(hal) \
	((hal)->regs->hal_tcl1_ring_misc)
#define HAL_TCL1_RING_TP_ADDR_LSB(hal) \
	((hal)->regs->hal_tcl1_ring_tp_addr_lsb)
#define HAL_TCL1_RING_TP_ADDR_MSB(hal) \
	((hal)->regs->hal_tcl1_ring_tp_addr_msb)
#define HAL_TCL1_RING_CONSUMER_INT_SETUP_IX0(hal) \
	((hal)->regs->hal_tcl1_ring_consumer_int_setup_ix0)
#define HAL_TCL1_RING_CONSUMER_INT_SETUP_IX1(hal) \
	((hal)->regs->hal_tcl1_ring_consumer_int_setup_ix1)
#define HAL_TCL1_RING_MSI1_BASE_LSB(hal) \
	((hal)->regs->hal_tcl1_ring_msi1_base_lsb)
#define HAL_TCL1_RING_MSI1_BASE_MSB(hal) \
	((hal)->regs->hal_tcl1_ring_msi1_base_msb)
#define HAL_TCL1_RING_MSI1_DATA(hal) \
	((hal)->regs->hal_tcl1_ring_msi1_data)
#define HAL_TCL2_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl2_ring_base_lsb)
#define HAL_TCL_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl_ring_base_lsb)

#define HAL_TCL1_RING_MSI1_BASE_LSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MSI1_BASE_LSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_MSI1_BASE_MSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MSI1_BASE_MSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_MSI1_DATA_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MSI1_DATA(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_BASE_MSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_BASE_MSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_ID_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_ID(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_CONSUMER_INT_SETUP_IX0(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX1_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_CONSUMER_INT_SETUP_IX1(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_TP_ADDR_LSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_TP_ADDR_LSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_TP_ADDR_MSB_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_TP_ADDR_MSB(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })
#define HAL_TCL1_RING_MISC_OFFSET(hal) ({ typeof(hal) _hal = (hal); \
	(HAL_TCL1_RING_MISC(_hal) - HAL_TCL1_RING_BASE_LSB(_hal)); })

/* SW2TCL(x) R2 ring pointers (head/tail) address */
#define HAL_TCL1_RING_HP			0x4000
#define HAL_TCL1_RING_TP			0x4004
#define HAL_TCL2_RING_HP			0x4008

#define HAL_TCL1_RING_TP_OFFSET \
		(HAL_TCL1_RING_TP - HAL_TCL1_RING_HP)

/*TCL CMD ring address */
#define HAL_TCL_CMD_RING_BASE_LSB		0x00001944
#define HAL_TCL_CMD_RING_HP			0x00004038
/*TQM CMD ring address */
#define HAL_TQM_HOST_CMD_RING_BASE_LSB		0x0000028C
#define HAL_TQM_HOST_CMD_RING_HP		0x00003028

/*TQM STATUS ring address */
#define HAL_TQM_HOST_STATUS_RING_BASE_LSB	0x000005D4
#define HAL_TQM_HOST_STATUS_RING_HP		0x00003060

/* TCL STATUS ring address */
#define HAL_TCL_STATUS_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl_status_ring_base_lsb)
#define HAL_TCL_STATUS_RING_HP			0x4070

/* TCL2SW1_EXCEPTION_RING */
#define HAL_TX_EXCEPTION_RING_BASE_LSB		0x1dec
#define HAL_TX_EXCEPTION_RING_HP		0x4080

/* SW2WBM_ASE_CMD_RING address */
#define HAL_SW2WBM_ASE_CMD_RING_BASE_LSB	0x1b48
#define HAL_SW2WBM_ASE_CMD_RING_HP		0x3178

/* TQM2SW_TELEMETRY1 RING*/
#define HAL_PEER_TX_TELEMETRY_RING_BASE_LSB	0xd54
#define HAL_PEER_TX_TELEMETRY_RING_HP		0x30e0

/* REO2SW_TELEMETRY1 RING*/
#define HAL_PEER_RX_TELEMETRY_RING_BASE_LSB	0x171c
#define HAL_PEER_RX_TELEMETRY_RING_HP		0x3168

/* PPE2TCL1 Ring address */
#define HAL_TCL_PPE2TCL1_RING_BASE_LSB(hal) \
	((hal)->regs->hal_tcl_ppe2tcl_ring_base_lsb)
#define HAL_TCL_PPE2TCL1_RING_HP		0x4040

/* PPE2TCL2 Ring address */
#define HAL_TCL_PPE2TCL2_RING_BASE_LSB		0x1a50
#define HAL_TCL_PPE2TCL2_RING_HP		0x4048

/* PPE2TCL3 Ring address */
#define HAL_TCL_PPE2TCL3_RING_BASE_LSB		0x1ae4
#define HAL_TCL_PPE2TCL3_RING_HP		0x4050

/*TQM2SW Ring address */
#define HAL_TQM2SW0_RELEASE_RING_HP		0x3078
#define HAL_TQM2SW1_RELEASE_RING_HP		0x3080
#define HAL_TQM2SW5_RELEASE_RING_HP		0x30A0
#define HAL_TQM2SW6_RELEASE_RING_HP		0x30A8

/* SAM cmd Ring address*/
#define HAL_SAM_HOST_CMD_RING_BASE_LSB		0x174
#define HAL_SAM_HOST_CMD_RING_HP		0x1010

/* SAM cmd Ring address*/
#define HAL_SAM_HOST_STATUS_RING_BASE_LSB	0x2DC
#define HAL_SAM_HOST_STATUS_RING_HP		0x1028

/* SAM command parameters. */
#define MAX_NUM_SAM_MSDU_QUEUES_SUPPORTED	4096
#define MAX_NUM_SAM_MPDU_QUEUES_SUPPORTED	1024

#define MAX_NUM_SAM_RESERVED_MSDU_QUEUES	128
#define MAX_NUM_SAM_RESERVED_MPDU_QUEUES	64

#define HAL_SAM_INVALID_MSDUQ_ID		0x1FFF
#define HAL_SAM_INVALID_MPDUQ_ID		0x7FF

/* SAM supports queue IDs from a single slice range (slice size: 256)
 * due to v1 hardware limitations.
 * TODO: Extend support to the full queue range with v2 hardware.
 */
#define HAL_SAM_QUEUE_SLICE_START_IDX		256
#define HAL_SAM_QUEUE_SLICE_END_IDX		511

#define HAL_SAM_R0_CONFIG			0
#define HAL_SAM_ENABLE				BIT(0)
#define HAL_SAM_IDLE_MASK			BIT(5)
#define HAL_SAM_IDLE_VALUE			1

#define HAL_TCL_PPE_INDEX_MAPPING_OFFSET 0x670
#define HAL_TCL_PPE_INDEX_MAPPING_SLOT_SIZE 0x4
#define HAL_TCL_PPE_INDEX_MAPPING_TABLE_n_ADDR(base, n) ((base) + \
			HAL_TCL_PPE_INDEX_MAPPING_OFFSET + \
			(HAL_TCL_PPE_INDEX_MAPPING_SLOT_SIZE * (n)))

/* ASE configuration details */
#define HAL_HW_AST_ENTRY_ALIGN          8
#define HAL_HW_AST_ENTRY_SIZE           32

#define HAL_TCL_ASE_GST_BASE_ADDR_LOW	0xF150B4
#define HAL_TCL_ASE_GST_BASE_ADDR_HIGH	0xF150B8
#define HAL_TCL_ASE_GST_SIZE		0xF150BC
#define HAL_TCL_ASE_SEARCH_CTRL		0xF150C0
#define HAL_TCL_ASE_HASH_KEY_31_0	0xF143A8
#define HAL_TCL_ASE_HASH_KEY_63_32	0xF143AC
#define HAL_TCL_ASE_HASH_KEY_64		0xF143B0

#define HAL_TCL_ASE_GST_BASE_ADDR_LOW_MASK			GENMASK(31, 0)
#define HAL_TCL_ASE_GST_BASE_ADDR_HIGH_MASK			GENMASK(7, 0)
#define HAL_TCL_ASE_GST_SIZE_MASK				GENMASK(19, 0)
#define HAL_TCL_ASE_SEARCH_CTRL_MAX_SEARCH			GENMASK(7, 0)
#define HAL_TCL_ASE_SEARCH_CTRL_CACHE_DISABLE			BIT(9)
#define HAL_TCL_ASE_SEARCH_CTRL_CACHE_FAILURES_ENABLE		BIT(10)

#define HAL_TCL_ASE_PEER_FETCH_CACHE_CTRL		0xF130A4
#define HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH		BIT(1)
#define HAL_TCL_ASE_PEER_FETCH_CACHE_FLUSH_STATUS	BIT(2)
/* WBM PPE Release Ring address */
#define HAL_TQM_PPE_RELEASE_RING_BASE_LSB(hal) \
	((hal)->regs->hal_ppe_rel_ring_base)
#define HAL_TQM_PPE_RELEASE_RING_HP	0xb88

#define HAL_TCL_ENTRANCE_FROM_PPE_RING_SIZE (8 * sizeof(u32))

/* TODO: CORE DP: cahnge REO error ring assignment as required */
#define HAL_REO_DEST_REL_ERR_RING_NUM 0

/* REO2SW(x) R0 ring configuration address */
#define HAL_REO1_GEN_ENABLE			0x00000000
#define HAL_REO1_MISC_CTRL_ADDR(hal) \
	((hal)->regs->hal_reo1_misc_ctrl_addr)
#define HAL_REO1_DEST_RING_CTRL_AP_IX_0		0x1be0
#define HAL_REO1_DEST_RING_CTRL_AP_IX_1		0x1be4
#define HAL_REO1_DEST_RING_CTRL_AP_IX_2		0x1be8
#define HAL_REO1_DEST_RING_CTRL_AP_IX_3		0x1bec
#define HAL_REO1_DEST_RING_CTRL_AP_IX_4		0x1bf0
#define HAL_REO1_DEST_RING_CTRL_AP_IX_5		0x1bf4

#define HAL_REO_ERROR_DEST_MAPPING_AP_IX_0	0x1c28
#define HAL_REO_ERROR_DEST_MAPPING_AP_IX_1	0x1c2c
#define HAL_REO_ERROR_DEST_MAPPING_AP_IX_2	0x1c30

#define HAL_REO_IX_FIELD_WIDTH 5
#define HAL_REO_ERROR_CODE_RBM_OVERRIDE		0x1bd4
#define HAL_REO_RXDMA_ERROR_CODE_RBM_OVERRIDE	0x1c58

#define HAL_REO_RDI_CTRL_SEL_WITH_TID				0x00000048

#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_0	0x1bf8
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_1	0x1bfc
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_2	0x1c00
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_3	0x1c04
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_4	0x1c08
#define HAL_REO1_DEST_RING_CTRL_AP_TID_BASED_IX_5	0x1c0c

#define HAL_REO_ERROR_DEST_MAPPING_AP_TID_BASED_IX_0	0x1c34
#define HAL_REO_ERROR_DEST_MAPPING_AP_TID_BASED_IX_1	0x1c38
#define HAL_REO_ERROR_DEST_MAPPING_AP_TID_BASED_IX_2	0x1c3c

#define HAL_REO_RBM_DEST_RING_CTRL_AP_IX_0	0x1c40
#define HAL_REO_RBM_DEST_RING_CTRL_AP_IX_1	0x1c44
#define HAL_REO_RBM_DEST_RING_CTRL_AP_IX_2	0x1c48
#define HAL_REO_MISC_CTL_AP			0x1c4c
#define HAL_REO_MISC_CTL_BAR_DEST_RING_AP	GENMASK(9, 5)

#define HAL_REO_RXDMA_ERROR_CODE_REORDER	0x1c50
#define HAL_REO_RXDMA_ERROR_CODE_REO_DELINK	0x1c54
#define HAL_REO_ERROR_CODE_REO_DELINK		0x1bd0
#define HAL_BAR_REO_ERROR_CODE_DELINK		0x1df4

/* ROAMING_DEST_CFG: per-RD-ring select ROAMING1 when roaming is enabled.
 * Bits [18:0] correspond to REO2FW_MGMT..REO2SW0 destination rings
 */
#define HAL_REO1_ROAMING_DEST_CFG			0x1de4
#define HAL_REO1_ROAMING_DEST_CFG_MASK			GENMASK(18, 0)

/* DESCRIPTOR_TYPE_ROAMING: descriptor type placed in REO2SW_ROAMING1 ring
 * 0 = REO_DESTINATION_RING descriptor
 */
#define HAL_REO1_DESCRIPTOR_TYPE_ROAMING		0x1e04
#define HAL_REO1_DESC_TYPE_PPE_FOR_SW_ROAMING1		BIT(0)

#define HAL_REO1_MISC_CFG_1				0x190c
#define HAL_REO1_MISC_CFG_1_REO_ERR_DELINK_ENABLE	BIT(3)
#define HAL_REO1_MISC_CFG_1_RXDMA_ERR_DELINK_ENABLE	BIT(4)
#define HAL_REO1_MISC_CFG_1_REO_MSDU_FETCH_OPTIMIZE	BIT(1)
#define HAL_REO1_MISC_CFG_1_REO_MSDU_LINK_SHARING_EN	BIT(10)
#define HAL_REO1_MISC_CFG_1_STOP_DELINKING_DURING_CORRUPTION	BIT(11)

#define HAL_REO1_COOKIE_CONV_EN_RING			0x1910
#define HAL_REO1_COOKIE_CONV_REO2SW0_EN			BIT(0)
#define HAL_REO1_COOKIE_CONV_REO2SW8_EN			BIT(8)
#define HAL_REO1_COOKIE_CONV_REO2SW9_EN			BIT(9)
#define HAL_REO1_COOKIE_CONV_REO2SW11_EN		BIT(20)
#define HAL_REO1_COOKIE_CONV_REO2SW_ROAMING1_EN		BIT(21)

#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX0		0x1c84
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW0_ERR	GENMASK(4, 0)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW1_ERR	GENMASK(9, 5)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW2_ERR	GENMASK(14, 10)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW3_ERR	GENMASK(19, 15)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW4_ERR	GENMASK(24, 20)

#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX1		0x1c88
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW5_ERR	GENMASK(4, 0)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW6_ERR	GENMASK(9, 5)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW8_ERR	GENMASK(19, 15)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW9_ERR	GENMASK(24, 20)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2PPE_ERR	GENMASK(29, 25)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW6_ERR	GENMASK(9, 5)

#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_IX2		0x1c8c
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2PPE1_ERR	GENMASK(4, 0)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2PPE2_ERR	GENMASK(9, 5)
#define HAL_REO1_ERROR_RING_CFG_FOR_DEST_REO2SW11_ERR	GENMASK(24, 20)

#define HAL_REO1_FRAG_MAP_IX_0				0x1dd8
#define HAL_REO1_FRAG_MAP_REO2SW0			GENMASK(4, 0)
#define HAL_REO1_FRAG_MAP_REO2SW1			GENMASK(9, 5)
#define HAL_REO1_FRAG_MAP_REO2SW2			GENMASK(14, 10)
#define HAL_REO1_FRAG_MAP_REO2SW3			GENMASK(19, 15)
#define HAL_REO1_FRAG_MAP_REO2SW4			GENMASK(24, 20)

#define HAL_REO1_FRAG_MAP_IX_1				0x1ddc
#define HAL_REO1_FRAG_MAP_REO2SW5			GENMASK(4, 0)
#define HAL_REO1_FRAG_MAP_REO2SW8			GENMASK(19, 15)
#define HAL_REO1_FRAG_MAP_REO2SW9			GENMASK(24, 20)
#define HAL_REO1_FRAG_MAP_REO2PPE			GENMASK(29, 25)

#define HAL_REO1_FRAG_MAP_IX_2				0x1de0
#define HAL_REO1_FRAG_MAP_REO2PPE1			GENMASK(4, 0)
#define HAL_REO1_FRAG_MAP_REO2PPE2			GENMASK(9, 5)
#define HAL_REO1_FRAG_MAP_REO2SW11			GENMASK(24, 20)

#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX0		0x1de8
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW0_ERR	GENMASK(4, 0)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW1_ERR	GENMASK(9, 5)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW2_ERR	GENMASK(14, 10)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW3_ERR	GENMASK(19, 15)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW4_ERR	GENMASK(24, 20)

#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX1		0x1dec
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW5_ERR	GENMASK(4, 0)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW8_ERR	GENMASK(19, 15)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW9_ERR	GENMASK(24, 20)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2PPE_ERR	GENMASK(29, 25)

#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_IX2		0x1df0
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2PPE1_ERR	GENMASK(4, 0)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2PPE2_ERR	GENMASK(9, 5)
#define HAL_REO1_FRAG_ERROR_RING_CFG_FOR_DEST_REO2SW11_ERR	GENMASK(24, 20)

#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX0		0x1df8
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW0_ERR	GENMASK(4, 0)
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW1_ERR	GENMASK(9, 5)
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW2_ERR	GENMASK(14, 10)
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW3_ERR	GENMASK(19, 15)
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW4_ERR	GENMASK(24, 20)

#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX1		0x1dfc
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2SW5_ERR	GENMASK(4, 0)
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2PPE_ERR	GENMASK(29, 25)

#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_IX2		0x1e00
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2PPE1_ERR	GENMASK(4, 0)
#define HAL_REO1_BAR_ERROR_RING_CFG_FOR_DEST_REO2PPE2_ERR	GENMASK(9, 5)

#define HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX0		0x1c5c
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_0			GENMASK(4, 0)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_1			GENMASK(9, 5)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_2			GENMASK(14, 10)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_3			GENMASK(19, 15)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_4			GENMASK(24, 20)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_5			GENMASK(29, 25)

#define HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX1		0x1c60
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_6			GENMASK(4, 0)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_7			GENMASK(9, 5)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_8			GENMASK(14, 10)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_9			GENMASK(19, 15)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_10		GENMASK(24, 20)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_11		GENMASK(29, 25)

#define HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX2		0x1c64
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_12		GENMASK(4, 0)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_13		GENMASK(9, 5)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_14		GENMASK(14, 10)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_15		GENMASK(19, 15)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_16		GENMASK(24, 20)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_17		GENMASK(29, 25)

#define HAL_REO1_RXDMA_ERROR_DESTINATION_MAPPING_AP_IX3		0x1c68
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_18		GENMASK(4, 0)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_19		GENMASK(9, 5)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_20		GENMASK(14, 10)
#define HAL_REO1_RXDMA_ERROR_DESTINATION_RING_OTHER		GENMASK(19, 15)

#define HAL_REO1_MISC_CFG_2				0x1c7c
#define HAL_REO1_MISC_CFG_2_BAR_REO_ERR_DELINK_ENABLE	BIT(12)

#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_0	0x1c6c
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_1	0x1c70
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_2	0x1c74
#define HAL_REO_RXDMA_ERROR_DEST_MAPPING_AP_TID_BASED_IX_3	0x1c78
#define HAL_REO_MISC_CFG_BN_2			0x1c7c
#define HAL_REO_PPE_DEST_OVERRIDE		0x1c80
#define HAL_REO_FW_MGMT_ROUTING_CFG		0x1c90
#define HAL_REO_RX_SDWF_CFG			0x1c94
#define HAL_REO_BACKPRESSURE_DROP_EN		0x1c98

/* REO PPE destination override field definitions */
#define HAL_RE02PPE_OVERRIDE_RING		GENMASK(4, 0)
#define HAL_RE02PPE1_OVERRIDE_RING		GENMASK(9, 5)
#define HAL_RE02PPE2_OVERRIDE_RING		GENMASK(14, 10)

/* HAL_REO_PPE_DEST_OVERRIDE_VAL
 *
 * REO2PPE  -> ring 1
 * REO2PPE1 -> ring 2
 * REO2PPE2 -> ring 3
 */
#define HAL_REO_PPE_DEST_OVERRIDE_VAL				\
	(u32_encode_bits(HAL_SRNG_RING_ID_REO2SW1, HAL_RE02PPE_OVERRIDE_RING) | \
	 u32_encode_bits(HAL_SRNG_RING_ID_REO2SW2, HAL_RE02PPE1_OVERRIDE_RING) | \
	 u32_encode_bits(HAL_SRNG_RING_ID_REO2SW3, HAL_RE02PPE2_OVERRIDE_RING))

/* Bit definitions for HAL_REO_BACKPRESSURE_DROP_EN
 * (UMAC_REO_R0_BACKPRESSURE_DROP_EN, full addr 0xF23C98)
 * Setting a bit enables backpressure-drop for the corresponding ring:
 * REO will route packets to the buffer-release ring instead of stalling.
 */
#define HAL_REO_BP_DROP_REO2SW0		BIT(0)
#define HAL_REO_BP_DROP_REO2SW1		BIT(1)
#define HAL_REO_BP_DROP_REO2SW2		BIT(2)
#define HAL_REO_BP_DROP_REO2SW3		BIT(3)
#define HAL_REO_BP_DROP_REO2SW4		BIT(4)
#define HAL_REO_BP_DROP_REO2SW5		BIT(5)
#define HAL_REO_BP_DROP_REO2SW6		BIT(6)
#define HAL_REO_BP_DROP_REO2SW7		BIT(7)
#define HAL_REO_BP_DROP_REO2SW8		BIT(8)
#define HAL_REO_BP_DROP_REO2SW9		BIT(9)
#define HAL_REO_BP_DROP_REO2SW10	BIT(10)
#define HAL_REO_BP_DROP_REO2SW11	BIT(11)
#define HAL_REO_BP_DROP_REO2FW		BIT(12)
#define HAL_REO_BP_DROP_REO2FW_MGMT	BIT(13)
#define HAL_REO_BP_DROP_REO2PPE		BIT(14)
#define HAL_REO_BP_DROP_REO2PPE1	BIT(15)
#define HAL_REO_BP_DROP_REO2PPE2	BIT(16)

/* Convenience masks */
/* All REO2SW rings (SW0..SW11) — enable backpressure drop for all SW rings
 * except flush(REO2SW0) and mgmt(REO2SW8, REO2SW9 and REO2SW11) rings.
 */
#define HAL_REO_BP_DROP_REO2SW_ALL	(HAL_REO_BP_DROP_REO2SW1  | \
					 HAL_REO_BP_DROP_REO2SW2  | \
					 HAL_REO_BP_DROP_REO2SW3  | \
					 HAL_REO_BP_DROP_REO2SW4  | \
					 HAL_REO_BP_DROP_REO2SW5  | \
					 HAL_REO_BP_DROP_REO2SW6  | \
					 HAL_REO_BP_DROP_REO2SW7  | \
					 HAL_REO_BP_DROP_REO2SW10)
/* All REO2PPE rings */
#define HAL_REO_BP_DROP_REO2PPE_ALL	(HAL_REO_BP_DROP_REO2PPE  | \
					 HAL_REO_BP_DROP_REO2PPE1 | \
					 HAL_REO_BP_DROP_REO2PPE2)

#define HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_0	0x1c9c
/*
 * Buffer-release ring IDs used in BACKPRESSURE_BUFFER_RELEASE_RING_SELECT:
 *   HAL_REO_BP_REL_RING_SW0  = 0  → SW0 buffer pool (used by REO2SW / SFE path)
 *   HAL_REO_BP_REL_RING_DS   = 2  → Dedicated DS buffer pool (used by REO2PPE
 *                                    when dp_ppe2wbm_use_dedicated_pool is set)
 *
 * REO2SW rings always use SW0 (ring 0).
 * REO2PPE rings use SW0 (ring 0) by default, or DS (ring 2) when the
 * dedicated PPE-to-WBM buffer pool is enabled.
 */
#define HAL_REO_BP_REL_RING_SW0		0  /* SW0 pool — REO2SW / SFE path */
#define HAL_REO_BP_REL_RING_DS		2  /* DS pool  — REO2PPE dedicated pool */

/*
 * Bit definitions for HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_0
 * (UMAC_REO_R0_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_0, addr 0xF23C9C)
 * Each 2-bit field selects which buffer-release ring receives the
 * backpressure-dropped packets for the corresponding REO destination ring.
 *
 * Policy:
 *   REO2SW rings  → always SW0 (HAL_REO_BP_REL_RING_SW0 = 0)
 *   REO2PPE rings → SW0 by default; DS pool (HAL_REO_BP_REL_RING_DS = 2)
 *                   when dp_ppe2wbm_use_dedicated_pool is enabled
 */
#define HAL_REO_BP_REL_SEL_REO2SW0	GENMASK(1, 0)
#define HAL_REO_BP_REL_SEL_REO2SW1	GENMASK(3, 2)
#define HAL_REO_BP_REL_SEL_REO2SW2	GENMASK(5, 4)
#define HAL_REO_BP_REL_SEL_REO2SW3	GENMASK(7, 6)
#define HAL_REO_BP_REL_SEL_REO2SW4	GENMASK(9, 8)
#define HAL_REO_BP_REL_SEL_REO2SW5	GENMASK(11, 10)
#define HAL_REO_BP_REL_SEL_REO2SW6	GENMASK(13, 12)
#define HAL_REO_BP_REL_SEL_REO2SW7	GENMASK(15, 14)
#define HAL_REO_BP_REL_SEL_REO2SW8	GENMASK(17, 16)
#define HAL_REO_BP_REL_SEL_REO2SW9	GENMASK(19, 18)
#define HAL_REO_BP_REL_SEL_REO2SW10	GENMASK(21, 20)
#define HAL_REO_BP_REL_SEL_REO2SW11	GENMASK(23, 22)
#define HAL_REO_BP_REL_SEL_REO2FW	GENMASK(25, 24)
#define HAL_REO_BP_REL_SEL_REO2FW_MGMT	GENMASK(27, 26)
#define HAL_REO_BP_REL_SEL_REO2PPE	GENMASK(29, 28)
#define HAL_REO_BP_REL_SEL_REO2PPE1	GENMASK(31, 30)

/*
 * HAL_REO_BP_REL_SEL_IX0_VAL(ppe_ring) — IX_0 register value
 *
 * REO2SW rings (SW0..SW11): always use HAL_REO_BP_REL_RING_SW0 (= 0)
 * REO2PPE rings (PPE, PPE1): use @ppe_ring, which is:
 *   - HAL_REO_BP_REL_RING_SW0 (0) when dp_ppe2wbm_use_dedicated_pool = false
 *   - HAL_REO_BP_REL_RING_DS  (2) when dp_ppe2wbm_use_dedicated_pool = true
 */
#define HAL_REO_BP_REL_SEL_IX0_VAL(ppe_ring)				\
	(u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW0)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW1)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW2)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW3)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW4)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW5)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW6)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW7)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW8)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW9)  | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW10) | \
	 u32_encode_bits(HAL_REO_BP_REL_RING_SW0, HAL_REO_BP_REL_SEL_REO2SW11) | \
	 u32_encode_bits((ppe_ring),              HAL_REO_BP_REL_SEL_REO2PPE)  | \
	 u32_encode_bits((ppe_ring),              HAL_REO_BP_REL_SEL_REO2PPE1))

#define HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_1	0x1ca0
/*
 * Bit definitions for HAL_REO_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_1
 * (UMAC_REO_R0_BACKPRESSURE_BUFFER_RELEASE_RING_SELECT_IX_1, addr 0xF23CA0)
 * Only REO2PPE2 is present in this register.
 */
#define HAL_REO_BP_REL_SEL_REO2PPE2	GENMASK(1, 0)

/*
 * HAL_REO_BP_REL_SEL_IX1_VAL(ppe_ring) — IX_1 register value
 *
 * REO2PPE2: use @ppe_ring (same policy as IX_0 PPE rings above)
 */
#define HAL_REO_BP_REL_SEL_IX1_VAL(ppe_ring)				\
	u32_encode_bits((ppe_ring), HAL_REO_BP_REL_SEL_REO2PPE2)
#define HAL_REO_AGING_FLUSH_LOW_LATENCY_OPTION	\
	HWIO_REO_R0_AGING_FLUSH_LOW_LATENCY_OPTION_OFFS

#define HAL_REO1_QDESC_ADDR(hal)        ((hal)->regs->hal_reo1_qdesc_addr)
#define HAL_REO1_QDESC_MAX_PEERID(hal)  ((hal)->regs->hal_reo1_qdesc_max_peerid)
#define HAL_REO1_SW_COOKIE_CFG0(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg0)
#define HAL_REO1_SW_COOKIE_CFG1(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg1)

#define REO2PPE_DST_RING_MAP 11
#define REO_DEST_CTRL_IX_0_RING6_MAP_MASK 0xF
#define REO_DEST_CTRL_IX_0_RING6_MAP_SHFT 24

/* REO2PPE cookie conversion enable bits */
#define HAL_REO2PPE_COOKIE_CONV_EN_RING		BIT(11)
#define HAL_REO2PPE1_COOKIE_CONV_EN_RING	BIT(12)
#define HAL_REO2PPE2_COOKIE_CONV_EN_RING	BIT(13)

#define HAL_REO1_QDESC_ADDR(hal)                ((hal)->regs->hal_reo1_qdesc_addr)
#define HAL_REO1_QDESC_MAX_PEERID(hal)  ((hal)->regs->hal_reo1_qdesc_max_peerid)
#define HAL_REO1_SW_COOKIE_CFG0(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg0)
#define HAL_REO1_SW_COOKIE_CFG1(hal)	((hal)->regs->hal_reo1_sw_cookie_cfg1)
#define HAL_REO1_QDESC_LUT_BASE0(hal)	((hal)->regs->hal_reo1_qdesc_lut_base0)
#define HAL_REO1_QDESC_LUT_BASE1(hal)	((hal)->regs->hal_reo1_qdesc_lut_base1)
#define HAL_REO1_RING_BASE_LSB(hal)	((hal)->regs->hal_reo1_ring_base_lsb)
#define HAL_REO1_RING_BASE_MSB(hal)	((hal)->regs->hal_reo1_ring_base_msb)
#define HAL_REO1_RING_ID(hal)		((hal)->regs->hal_reo1_ring_id)
#define HAL_REO1_RING_MISC(hal)		((hal)->regs->hal_reo1_ring_misc)
#define HAL_REO1_RING_HP_ADDR_LSB(hal)	((hal)->regs->hal_reo1_ring_hp_addr_lsb)
#define HAL_REO1_RING_HP_ADDR_MSB(hal)	((hal)->regs->hal_reo1_ring_hp_addr_msb)
#define HAL_REO1_RING_PRODUCER_INT_SETUP(hal) \
	((hal)->regs->hal_reo1_ring_producer_int_setup)
#define HAL_REO1_RING_MSI1_BASE_LSB(hal)	\
	((hal)->regs->hal_reo1_ring_msi1_base_lsb)
#define HAL_REO1_RING_MSI1_BASE_MSB(hal)	\
	((hal)->regs->hal_reo1_ring_msi1_base_msb)
#define HAL_REO1_RING_MSI1_DATA(hal)	((hal)->regs->hal_reo1_ring_msi1_data)
#define HAL_REO2_RING_BASE_LSB(hal)	((hal)->regs->hal_reo2_ring_base)
#define HAL_REO1_AGING_THRESH_IX_0(hal)	((hal)->regs->hal_reo1_aging_thres_ix0)
#define HAL_REO1_AGING_THRESH_IX_1(hal)	((hal)->regs->hal_reo1_aging_thres_ix1)
#define HAL_REO1_AGING_THRESH_IX_2(hal)	((hal)->regs->hal_reo1_aging_thres_ix2)
#define HAL_REO1_AGING_THRESH_IX_3(hal)	((hal)->regs->hal_reo1_aging_thres_ix3)

#define HAL_REO1_RING_MISC_OFFSET \
		(HAL_REO1_RING_MISC(hal) - HAL_REO1_RING_BASE_LSB(hal))

#define HAL_REO1_REO2PPE_DST_VAL		0x2000
#define HAL_REO1_REO2PPE_DST_INFO		0x00001af0
#define HAL_REO_COPY_PPE_INFO_FROM_MSDU_VAL	BIT(2)
#define HAL_REO_PPE_JUMBO_FRAME_SUPPORT_EN	BIT(1)
#define HAL_REO_PPE_DEST_OVERRIDE_EN		BIT(0)

#define HAL_WIFI8_HASH_ROUTING_RING_SW0 0
#define HAL_WIFI8_HASH_ROUTING_RING_SW1 1
#define HAL_WIFI8_HASH_ROUTING_RING_SW2 2
#define HAL_WIFI8_HASH_ROUTING_RING_SW3 3
#define HAL_WIFI8_HASH_ROUTING_RING_SW4 4
#define HAL_WIFI8_HASH_ROUTING_RING_REL 5
#define HAL_WIFI8_HASH_ROUTING_RING_FW  6
#define HAL_WIFI8_HASH_ROUTING_RING_SW5 7
#define HAL_WIFI8_HASH_ROUTING_RING_SW6 8
#define HAL_WIFI8_HASH_ROUTING_RING_SW7 9
#define HAL_WIFI8_HASH_ROUTING_RING_SW8 10
#define HAL_WIFI8_HASH_ROUTING_RING_SW9 11
#define HAL_WIFI8_HASH_ROUTING_RING_SW10 16

/* REO2SW(x) R2 ring pointers (head/tail) address */
/* REO2SW(x) R2 ring pointers (head/tail) address */
#define HAL_REO1_RING_HP                       0x3078
#define HAL_REO1_RING_TP                       0x307c
#define HAL_REO2_RING_HP                       0x3080

#define HAL_REO1_RING_TP_OFFSET			(HAL_REO1_RING_TP - HAL_REO1_RING_HP)

/* REO2SW0 ring configuration address */
#define HAL_REO_SW0_RING_BASE_LSB(hal) \
	((hal)->regs->hal_reo2_sw0_ring_base)

/* REO2SW0 R2 ring pointer (head/tail) address */
#define HAL_REO_SW0_RING_HP			0x30d0

/* REO CMD R0 address */
#define HAL_REO_CMD_RING_BASE_LSB(hal) \
	((hal)->regs->hal_reo_cmd_ring_base)

/* REO CMD R2 address */
#define HAL_REO_CMD_HP				0x3020

/* FSE CMD Cache ring R0 address */
#define HAL_FSE_CMD_RING_BASE_LSB		0x1bc0

/* FSE CMD Cache ring R2 address */
#define HAL_FSE_CMD_HP				0x3180

/* REO CMD R0 address - priority cmd ring for SMD*/
#define HAL_REO_CMD1_RING_BASE_LSB		0x7d0

/* REO CMD R2 address - priority cmd ring for SMD*/
#define HAL_REO_CMD1_HP				0x3070

/* SW2REO R0 address */
#define	HAL_SW2REO_RING_BASE_LSB(hal) \
	((hal)->regs->hal_sw2reo_ring_base)
#define HAL_SW2REO1_RING_BASE_LSB(hal) \
	((hal)->regs->hal_sw2reo1_ring_base)

/* SW2REO R2 address */
#define HAL_SW2REO_RING_HP			0x3028
#define HAL_SW2REO1_RING_HP			0x3030

/* CE ring R0 address */
#define HAL_CE_SRC_RING_BASE_LSB	0x0
#define HAL_CE_DST_RING_BASE_LSB	0x0
#define HAL_CE_DST_STATUS_RING_BASE_LSB		0x58
#define HAL_CE_DST_RING_CTRL			0xb0

/* CE ring R2 address */
#define HAL_CE_DST_RING_HP		0x400
#define HAL_CE_DST_STATUS_RING_HP	0x408

/* REO status address */
#define HAL_REO_STATUS_RING_BASE_LSB(hal) \
	((hal)->regs->hal_reo_status_ring_base)
#define HAL_REO_STATUS_HP			0x3128

/* REO2PPE address */
#define HAL_REO2PPE_RING_BASE_LSB(hal) \
		((hal)->regs->hal_reo2ppe_ring_base)
#define HAL_REO2PPE_HP				0x30d8

/* WBM Idle R0 address */
#define HAL_WBM_IDLE_LINK_RING_BASE_LSB(hal) \
	((hal)->regs->hal_wbm_idle_ring_base_lsb)
#define HAL_WBM_IDLE_LINK_RING_MISC_ADDR(hal) \
	((hal)->regs->hal_wbm_idle_ring_misc_addr)
#define HAL_WBM_R0_IDLE_LIST_CONTROL_ADDR(hal) \
	((hal)->regs->hal_wbm_r0_idle_list_cntl_addr)
#define HAL_WBM_R0_IDLE_LIST_SIZE_ADDR(hal) \
	((hal)->regs->hal_wbm_r0_idle_list_size_addr)
#define HAL_WBM_SCATTERED_RING_BASE_LSB(hal) \
	((hal)->regs->hal_wbm_scattered_ring_base_lsb)
#define HAL_WBM_SCATTERED_RING_BASE_MSB(hal) \
	((hal)->regs->hal_wbm_scattered_ring_base_msb)
#define HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX0(hal) \
	((hal)->regs->hal_wbm_scattered_desc_head_info_ix0)
#define HAL_WBM_SCATTERED_DESC_PTR_HEAD_INFO_IX1(hal) \
	((hal)->regs->hal_wbm_scattered_desc_head_info_ix1)
#define HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX0(hal) \
	((hal)->regs->hal_wbm_scattered_desc_tail_info_ix0)
#define HAL_WBM_SCATTERED_DESC_PTR_TAIL_INFO_IX1(hal) \
	((hal)->regs->hal_wbm_scattered_desc_tail_info_ix1)
#define HAL_WBM_SCATTERED_DESC_PTR_HP_ADDR(hal) \
	((hal)->regs->hal_wbm_scattered_desc_ptr_hp_addr)

/* WBM Idle R2 address */
#define HAL_WBM_IDLE_LINK_RING_HP		0x3040

/* SW2WBM R0 release address */
#define HAL_WBM_SW_RELEASE_RING_BASE_LSB	0x24c
#define HAL_WBM_FW_RELEASE_RING_BASE_LSB	0x2c4

/* SW2WBM R2 release address */
#define HAL_WBM_SW_RELEASE_RING_HP		0x3010
#define HAL_WBM_FW_RELEASE_RING_HP		0x3018

/* TQM2SW R0 release address */
#define HAL_TQM2SW0_RELEASE_RING_BASE_LSB	0x73c
#define HAL_TQM2SW1_RELEASE_RING_BASE_LSB	0x7b4
#define HAL_TQM2SW5_RELEASE_RING_BASE_LSB	0x994
#define HAL_TQM2SW6_RELEASE_RING_BASE_LSB	0xA0C

/* REO2SW6 ring */
#define HAL_REO2SW6_RING_BASE_LSB	0xaa0
#define HAL_REO2SW6_RING_HP		0x30a0

/* TQM2PPE release ring address */
#define HAL_TQM2PPE_RELEASE_RING_BASE_LSB	0xB74
#define HAL_TQM2PPE_RELEASE_RING_HP		0x30C0
#define HAL_TQM2PPE_RELEASE_RING_MISC		0xB84
#define HAL_TQM2PPE_RELEASE_RING_MISC_RING_ID_DISABLE	BIT(0)
#define HAL_TQM2PPE_RELEASE_RING_MISC_LOOPCNT_DISABLE	BIT(1)

/* REO2SW8 ring */
#define HAL_REO2SW8_RING_BASE_LSB	0xb90
#define HAL_REO2SW8_RING_HP		0x30b0

/* REO2SW9 ring */
#define HAL_REO2SW9_RING_BASE_LSB	0xc08
#define HAL_REO2SW9_RING_HP		0x30b8

/* REO2SW11 ring */
#define HAL_REO2SW11_RING_BASE_LSB	0xcf8
#define HAL_REO2SW11_RING_HP		0x30c8

/* REO2SW_ROAMING1 */
#define HAL_REO2SW_ROAMING1_RING_BASE_LSB	0x180C
#define HAL_REO2SW_ROAMING1_RING_HP		0x3178

/* SW2WBM MGMT buf ring */
#define HAL_WBM_SW2WBM_MGMT_BUFF_RELEASE1_RING_BASE_LSB	0x1f08
#define HAL_WBM_SW2WBM_MGMT_BUFF_RELEASE1_RING_HP	0x31b8

/* WBM idle MGMT buf pool */
#define HAL_WBM_SW_IDLE_MGMT_BUF_RING_LSB	0x2190
#define HAL_WBM_SW_IDLE_MGMT_BUF_RING_HP	0x31e8

/* TQM cookie config address and mask */
#define HAL_TQM_TX_COMPLETION_MISC_CFG	0x1310
#define HAL_TQM_TX_COMPLETION_MISC_CFG_RELEASE_PATH_EN	0x1
#define HAL_TQM_TX_COMPLETION_MISC_CFG_ERR_PATH_EN	0x2
#define HAL_TQM_TX_COMPLETION_MISC_CFG_CONV_IND_EN	0x8

#define HAL_TQM_SW_COOKIE_CFG0		0x1314
#define HAL_TQM_SW_COOKIE_CFG0_CMEM_LUT_BASE_ADDR_31_0	0xffffffff

#define HAL_TQM_SW_COOKIE_CFG1		0x1318
#define HAL_TQM_SW_COOKIE_CFG1_CMEM_BASE_ADDR_MSB	0xff
#define HAL_TQM_SW_COOKIE_CFG1_COOKIE_PPT_MSB		0x1f00
#define HAL_TQM_SW_COOKIE_CFG1_COOKIE_SPT_MSB		0x3e000
#define HAL_TQM_SW_COOKIE_CFG1_ALIGN	0x40000

#define HAL_TQM_SW_COOKIE_CONVERT_CFG	0x1220
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW0_EN	0x2
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW1_EN	0x4
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW2_EN	0x8
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW3_EN	0x10
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW4_EN	0x20
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW5_EN	0x40
#define HAL_TQM_SW_COOKIE_CONV_CFG_TQM2SW6_EN	0x80
#define HAL_TQM_SW_COOKIE_CONV_CFG_GLOBAL_EN	0x100

#define HAL_TQM_SW_COOKIE_CONVERT_CFG2  0x1224
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW7_EN	BIT(0x0)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW8_EN	BIT(0x1)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2PPE_EN	BIT(0x2)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW_ROAMING0_EN	BIT(0x3)
#define HAL_TQM_SW_COOKIE_CONV_CFG2_TQM2SW_ROAMING1_EN	BIT(0x4)

/* SW2WBM buf ring */
#define HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_BASE_LSB     0x00001c38
#define HAL_WBM_SW2WBM_BUFF_RELEASE2_RING_BASE_LSB     0x00001cb0
#define HAL_WBM_SW2WBM_BUFF_RELEASE1_RING_HP           0x00003188
#define HAL_WBM_SW2WBM_BUFF_RELEASE2_RING_HP           0x00003190

/* WBM idle buf pool */
#define HAL_WBM_SW_IDLE_BUF_RING_LSB                   0x000020e8
#define HAL_WBM_SW_IDLE_BUF_RING_HP                    0x000031d8

/* PPE2WBM buf ring */
#define HAL_WBM_PPE2WBM_BUFF_RELEASE1_RING_BASE_LSB	0x00001914
#define HAL_WBM_PPE2WBM_BUFF_RELEASE2_RING_BASE_LSB	0x000019A8
#define HAL_WBM_PPE2WBM_BUFF_RELEASE1_RING_HP		0x00003158
#define HAL_WBM_PPE2WBM_BUFF_RELEASE2_RING_HP		0x00003160

/* PPE2WBM idle buf pool */
#define HAL_PPE2WBM_SW_IDLE_BUF_RING_LSB		0x0000213C
#define HAL_PPE2WBM_SW_IDLE_BUF_RING_HP			0x000031E0

/* TCL ring field mask and offset */
#define HAL_TCL1_RING_BASE_MSB_RING_SIZE	0xfffff00
#define HAL_TCL1_RING_BASE_MSB_RING_BASE_ADDR_MSB	0xff
#define HAL_TCL1_RING_ID_ENTRY_SIZE	0xff
#define HAL_TCL1_RING_MISC_MSI_RING_ID_DISABLE		0x1
#define HAL_TCL1_RING_MISC_MSI_LOOPCNT_DISABLE		0x2
#define HAL_TCL1_RING_MISC_MSI_SWAP	0x8
#define HAL_TCL1_RING_MISC_HOST_FW_SWAP		0x10
#define HAL_TCL1_RING_MISC_DATA_TLV_SWAP	0x20
#define HAL_TCL1_RING_MISC_SRNG_ENABLE	0x40
#define HAL_TCL1_RING_MISC_TRANSACTION_TYPE		0x400000
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_INTR_TMR_THOLD	0xffff0000
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX0_BATCH_COUNTER_THOLD	0x7fff
#define HAL_TCL1_RING_CONSR_INT_SETUP_IX1_LOW_THOLD		0xffff
#define HAL_TCL1_RING_MSI1_BASE_MSB_MSI1_ENABLE		0x100
#define HAL_TCL1_RING_MSI1_BASE_MSB_ADDR		0xff
#define HAL_TCL1_RING_CMN_CTRL_DSCP_TID_MAP_PROG_EN		0x80000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP		GENMASK(31, 0)
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP0		0x7
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP1		0x38
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP2		0x1c0
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP3		0xe00
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP4		0x7000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP5		0x38000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP6		0x1c0000
#define HAL_TCL1_RING_FIELD_DSCP_TID_MAP7		0xe00000

#define HAL_TCL1_CMN_CONFIG_CLFY_INVALID_PPE_DESC	BIT(26)

/* TODO: CORE DP TX needs any update from Host */
#define HAL_TCL1_CMN_CONFIG	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x38)
#define HAL_TCL1_CMN_CONFIG_PPE	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x44)
#define HAL_TCL1_CMN_CONFIG1_PPE	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x48)
#define HAL_TCL1_RBM_MAPPING0	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0xd8)
#define HAL_TCL1_RBM_MAPPING1	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0xdc)
#define HAL_TCL1_LINK_ID_TO_CHIP_ID_MAP	(HAL_SEQ_WCSS_UMAC_TCL_REG + 0x9c)

#define HAL_TCL1_DISABLE_SKIP_HDR_FETCH 0x2

/* REO ring field mask and offset */
#define HAL_REO1_RING_BASE_MSB_RING_SIZE		0xfffff00
#define HAL_REO1_RING_BASE_MSB_RING_BASE_ADDR_MSB	0xff
#define HAL_REO1_RING_ID_RING_ID	0xff00
#define HAL_REO1_RING_ID_ENTRY_SIZE	0xff
#define HAL_REO1_RING_MISC_MSI_SWAP	0x8
#define HAL_REO1_RING_MISC_HOST_FW_SWAP		0x10
#define HAL_REO1_RING_MISC_DATA_TLV_SWAP	0x20
#define HAL_REO1_RING_MISC_SRNG_ENABLE		0x40
#define HAL_REO1_RING_MISC_RING_ID_DISABLE	0x1
#define HAL_REO1_RING_MISC_LOOPCNT_DISABLE	0x2
#define HAL_REO1_RING_PRDR_INT_SETUP_INTR_TMR_THOLD	0xffff0000
#define HAL_REO1_RING_PRDR_INT_SETUP_BATCH_COUNTER_THOLD	0x7fff
#define HAL_REO1_RING_MSI1_BASE_MSB_MSI1_ENABLE		0x100
#define HAL_REO1_RING_MSI1_BASE_MSB_ADDR		0xff
#define HAL_REO1_MISC_CTL_SPARE_CTRL_DST_RING		BIT(5)

#define HAL_REO1_MISC_CTL_NULL_BUFFER_ERROR_DST_RING	0xf8000
#define HAL_REO1_MISC_CTL_SOFT_REORDER_DST_RING		0x7c00
#define HAL_REO1_MISC_CTL_BAR_DST_RING		0x3e0
#define HAL_REO1_MISC_CTL_FRAG_DST_RING		0x1f

#define HAL_REO1_GEN_ENABLE_AGING_LIST_ENABLE		0x4
#define HAL_REO1_GEN_ENABLE_AGING_FLUSH_ENABLE		0x8
#define HAL_REO1_SW_COOKIE_CFG_CMEM_BASE_ADDR_MSB	0xff
#define HAL_REO1_SW_COOKIE_CFG_COOKIE_PPT_MSB		0x1f00
#define HAL_REO1_SW_COOKIE_CFG_COOKIE_SPT_MSB		0x3e000
#define HAL_REO1_SW_COOKIE_CFG_ALIGN	0x40000
#define HAL_REO1_SW_COOKIE_CFG_ENABLE		0x80000
#define HAL_REO1_SW_COOKIE_CFG_GLOBAL_ENABLE	0x100000
#define HAL_REO_QDESC_ADDR_READ_LUT_ENABLE		0x80
#define HAL_REO_QDESC_ADDR_READ_CLEAR_QDESC_ARRAY	0x40
#define HAL_REO_QLUT_REG_BASE_ADDR			GENMASK(39, 8)

/* CE ring bit field mask and shift */
#define HAL_CE_DST_R0_DEST_CTRL_MAX_LEN			GENMASK(15, 0)

#define HAL_ADDR_LSB_REG_MASK				0xffffffff

#define HAL_ADDR_MSB_REG_SHIFT				32

/* WBM ring bit field mask and shift */
#define HAL_WBM_LINK_DESC_IDLE_LIST_MODE	0x1
#define HAL_WBM_SCATTER_BUFFER_SIZE		0x7fc
#define HAL_WBM_SCATTER_RING_SIZE_OF_IDLE_LINK_DESC_LIST	0xffff0000
#define HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_39_32		0xff
#define HAL_WBM_SCATTERED_DESC_MSB_BASE_ADDR_MATCH_TAG		0xffffff00

#define HAL_WBM_SCATTERED_DESC_HEAD_P_OFFSET_IX1		0x1fff00
#define HAL_WBM_SCATTERED_DESC_TAIL_P_OFFSET_IX1	0x1fff00

#define HAL_WBM_IDLE_LINK_RING_MISC_SRNG_ENABLE		BIT(6)
#define HAL_WBM_IDLE_LINK_RING_MISC_RIND_ID_DISABLE	BIT(0)

#define BASE_ADDR_MATCH_TAG_VAL 0x5

#define HAL_REO_REO2SW1_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_REO_REO2SW0_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_REO_SW2REO_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_REO_CMD_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_REO_STATUS_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_FSE_CMD_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_REO2PPE_RING_BASE_MSB_RING_SIZE		0xffffffff
#define HAL_PPE2TCL_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_SW2TCL1_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_SW2TCL1_CMD_RING_BASE_MSB_RING_SIZE		0x000fffff
#define HAL_TCL_STATUS_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_TX_EXCEPTION_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_TQM_HOST_CMD_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_TQM_HOST_STATUS_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_CE_SRC_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_CE_DST_RING_BASE_MSB_RING_SIZE		0x0000ffff
#define HAL_CE_DST_STATUS_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_WBM_IDLE_LINK_RING_BASE_MSB_RING_SIZE	0x000fffff
#define HAL_SW2WBM_RELEASE_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_TQM2SW_RELEASE_RING_BASE_MSB_RING_SIZE	0x000fffff
#define HAL_RXDMA_RING_MAX_SIZE				0x000fffff
#define HAL_RXDMA_RING_MAX_SIZE_BE			0x000fffff
#define HAL_WBM2PPE_RELEASE_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_SW2WBM_ASE_CMD_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_SW2WBM_ASE_STATUS_RING_BASE_MSB_RING_SIZE	0x0000ffff
#define HAL_WBM_IDLE_BUF_RING_MAX_SIZE                 0x000fffff
#define HAL_WBM_BUF_RING_MAX_SIZE                      0x000fffff
#define HAL_SAM_HOST_CMD_RING_BASE_MSB_RING_SIZE	0x000fffff
#define HAL_SAM_HOST_STATUS_RING_BASE_MSB_RING_SIZE	0x000fffff
#define HAL_REO_REO2SW6_RING_BASE_MSB_RING_SIZE		0x000fffff

#define HAL_TELEMETRY_RING_MAX_SIZE			0x000fffff
#define HAL_TQM2PPE_RELEASE_RING_BASE_MSB_RING_SIZE	0x0000ffff

#define HAL_WBM2SW_REL_ERR_RING_NUM 5
#define HAL_WBM2SW_PPEDS_TX_CMPLN_MAP_ID 11

#define HAL_RX_MAX_BA_WINDOW	256

#define MIN_VI_REORDER_TIMEOUT_MS 100
#define MAX_VI_REORDER_TIMEOUT_MS 600
#define HAL_DEFAULT_BE_BK_VI_REO_TIMEOUT_USEC	(100 * 1000)
#define HAL_DEFAULT_VO_REO_TIMEOUT_USEC		(40 * 1000)

#define HAL_SRNG_TLV_HDR_TAG		GENMASK(9, 0)
#define HAL_SRNG_TLV_HDR_LEN		GENMASK(25, 10)

#define HAL_SRNG_DESC_LOOP_CNT		0xf0000000

/* Should be matching with HAL_REO_UPD_RX_QUEUE_INFO0_UPD_* fields */
#define HAL_REO_CMD_UPD0_RX_QUEUE_NUM		BIT(8)
#define HAL_REO_CMD_UPD0_VLD			BIT(9)
#define HAL_REO_CMD_UPD0_ALDC			BIT(10)
#define HAL_REO_CMD_UPD0_DIS_DUP_DETECTION	BIT(11)
#define HAL_REO_CMD_UPD0_SOFT_REORDER_EN	BIT(12)
#define HAL_REO_CMD_UPD0_AC			BIT(13)
#define HAL_REO_CMD_UPD0_BAR			BIT(14)
#define HAL_REO_CMD_UPD0_RETRY			BIT(15)
#define HAL_REO_CMD_UPD0_CHECK_2K_MODE		BIT(16)
#define HAL_REO_CMD_UPD0_OOR_MODE		BIT(17)
#define HAL_REO_CMD_UPD0_BA_WINDOW_SIZE		BIT(18)
#define HAL_REO_CMD_UPD0_PN_CHECK		BIT(19)
#define HAL_REO_CMD_UPD0_EVEN_PN		BIT(20)
#define HAL_REO_CMD_UPD0_UNEVEN_PN		BIT(21)
#define HAL_REO_CMD_UPD0_PN_HANDLE_ENABLE	BIT(22)
#define HAL_REO_CMD_UPD0_PN_SIZE		BIT(23)
#define HAL_REO_CMD_UPD0_IGNORE_AMPDU_FLG	BIT(24)
#define HAL_REO_CMD_UPD0_SVLD			BIT(25)
#define HAL_REO_CMD_UPD0_SSN			BIT(26)
#define HAL_REO_CMD_UPD0_SEQ_2K_ERR		BIT(27)
#define HAL_REO_CMD_UPD0_PN_ERR			BIT(28)
#define HAL_REO_CMD_UPD0_PN_VALID		BIT(29)
#define HAL_REO_CMD_UPD0_PN			BIT(30)

/* Should be matching with HAL_REO_UPD_RX_QUEUE_INFO1_* fields */
#define HAL_REO_CMD_UPD1_VLD			BIT(16)
#define HAL_REO_CMD_UPD1_ALDC			GENMASK(18, 17)
#define HAL_REO_CMD_UPD1_DIS_DUP_DETECTION	BIT(19)
#define HAL_REO_CMD_UPD1_SOFT_REORDER_EN	BIT(20)
#define HAL_REO_CMD_UPD1_LL_TIMER_ID		GENMASK(24, 21)
#define HAL_REO_CMD_UPD1_BAR			BIT(25)
#define HAL_REO_CMD_UPD1_RETRY			BIT(26)
#define HAL_REO_CMD_UPD1_CHECK_2K_MODE		BIT(27)
#define HAL_REO_CMD_UPD1_OOR_MODE		BIT(28)
#define HAL_REO_CMD_UPD1_PN_CHECK		BIT(29)
#define HAL_REO_CMD_UPD1_EVEN_PN		BIT(30)
#define HAL_REO_CMD_UPD1_UNEVEN_PN		BIT(31)

/* Should be matching with HAL_REO_UPD_RX_QUEUE_INFO2_* fields */
#define HAL_REO_CMD_UPD2_PN_HANDLE_ENABLE	BIT(0)
#define HAL_REO_CMD_UPD2_IGNORE_AMPDU_FLG	BIT(1)
#define HAL_REO_CMD_UPD2_SVLD			BIT(14)
#define HAL_REO_CMD_UPD2_SSN			GENMASK(26, 15)
#define HAL_REO_CMD_UPD2_SEQ_2K_ERR		BIT(27)
#define HAL_REO_CMD_UPD2_PN_ERR			BIT(28)
#define HAL_REO_CMD_UPD2_PN_VALID		BIT(29)
#define HAL_REO_CMD_UPD2_FLUSH_FROM_CACHE	BIT(30)

#define HAL_PMAC_PMCMN_MCMN_MAC_IDLE		0x288
#define HAL_WMAC_GXI_SM_STATES_IX_0		0x104
#define HAL_GXI_WDOG_WARN_STATUS		0x138
#define HAL_MXI_CMD_FIFO_DBG_STS		0x188
#define HAL_DMAC_DMCMN_DMAC_IDLE		{0x3020, 0x6020}
#define HAL_DMAC_DMCMN_DMAC_IDLE_COMMON		0xb4

#define HAL_DMAC_RXDMA_GLOBAL_RER_CMN		0x1808
#define HAL_DMAC_M0_RXDMA_ENTRANCE_FITLER	0x3800
#define HAL_DMAC_M1_RXDMA_ENTRANCE_FITLER	0x5800
#define HAL_PMAC_HWMLO_WAR_OPTIONS		0xBA4
#define HAL_MAC_WSIB_CFG			0
#define HAL_MAC_WSIB_IDLE_STATUS		0x1020
#define HAL_MAC_WSIB_MASTER_SYNC		0x9C
#define HAL_WFSS_WCSS_MLO_WSI_CBCR		0x410

#define HAL_WBM_GENERAL_ENABLE			0

#define HAL_MAC_IDLE_CHECK_TIMEOUT_USEC		10000
#define HAL_MAC_IDLE_CHECK_DELAY_USEC		10
#define HAL_TXRX_IDLE_CHECK_MASK		GENMASK(24, 23)
#define HAL_TXRX_IDLE_CHECK_VALUE		0x3

#define HAL_MXI_SM_STATES_IDLE_VALUE		0x211
#define HAL_MXI_SM_STATES_IDLE_MASK		GENMASK(11, 0)

#define HAL_GXI_WDOG_WARN_STATUS_VALUE		0
#define HAL_GXI_WDOG_WARN_STATUS_MASK		GENMASK(15, 0)

#define HAL_DMAC_CMN_IDLE_VALUE			0x3FF
#define HAL_DMAC_CMN_IDLE_MASK			GENMASK(9, 0)

#define WR_CMD_FIFO_WR_MASK			GENMASK(29, 24)
#define WR_CMD_FIFO_RD_MASK			GENMASK(21, 16)
#define RD_CMD_FIFO_WR_MASK			GENMASK(13, 8)
#define RD_CMD_FIFO_RD_MASK			GENMASK(5, 0)

#define HAL_RXDMA_PREFETCH_MASK_ENABLE		BIT(0)
#define HAL_RXDMA_ENT_FITLER_GLOBAL_ENABLE	BIT(1)
#define HAL_HALT_DOORBELL_ACTIVITY_MASK		BIT(12)
#define HAL_WSIB_PAUSE_MASK			BIT(7)

#define HAL_WSIB_IDLE_CHECK_TIMEOUT_USEC	16
#define HAL_WSIB_IDLE_CHECK_DELAY_USEC		1
#define HAL_WSIB_IDLE_MASK			GENMASK(3, 0)
#define HAL_WSIB_IDLE_VALUE			HAL_WSIB_IDLE_MASK
#define HAL_WSI_RESET_MASK			BIT(2)

#define HAL_TCL_RING_HALT_MASK			GENMASK(14, 3)
#define HAL_TCL_RING_HALT_VALUE			GENMASK(11, 0)
#define HAL_TCL_RING_UNHALT_VALUE		0

#define TCL2TQM_RING_STATUS			0x1C18
#define TCL2FW_EXCEPTION_RING_STATUS		0x1D80
#define TCL2FW_STATUS_RING_STATUS		0x1C90
#define TCL_NUM_AVAIL_WORDS_MASK		GENMASK(31, 16)

#define TCL_IDLE_MASK				BIT(15)
#define TCL_IDLE_VALUE				1
#define HAL_TCL_RING_HALT_STAT_REG		0x34
#define HAL_TCL_RING_HALT_STAT_MASK		GENMASK(11, 0)

#define HAL_TQM_R0_CONTROL			0
#define HAL_TQM_BLOCK_PREFETCH_MASK		BIT(3)
#define HAL_TQM_BLOCK_ENABLE_MASK		BIT(0)

#define HAL_TQM_PREFETCH_COUNT_MASK			GENMASK(23, 16)
#define HAL_TQM_PREFETCH_COUNT_VALUE			0
#define TCL2TQM_RING_CONSUMER_PREFETCH_STATUS		0x78
#define SW_CMD_RING_CONSUMER_PREFETCH_STATUS		0x168
#define SW_CMD1_RING_CONSUMER_PREFETCH_STATUS		0x1E0
#define TQM2TQM_IN1_RING_CONSUMER_PREFETCH_STATUS	0xFE8
#define TQM2TQM_IN2_RING_CONSUMER_PREFETCH_STATUS	0x107C
#define TQM2TQM_IN3_RING_CONSUMER_PREFETCH_STATUS	0x13C4
#define TQM2TQM_IN4_RING_CONSUMER_PREFETCH_STATUS	0x1458
#define FW2TQM_RING_CONSUMER_PREFETCH_STATUS		0xF0

#define HAL_TQM_SM_STATES_MASK				GENMASK(31, 0)
#define HAL_TQM_SM_STATES_VALUE				0
#define HAL_TQM_SM_STATES_IX0				0xED8
#define HAL_TQM_SM_STATES_IX1				0xEDC
#define HAL_TQM_SM_STATES_IX2				0xEE0
#define HAL_TQM_SM_STATES_IX3				0xEE4
#define HAL_TQM_SM_STATES_IX4				0x1828
#define HAL_TQM_SM_STATES_IX5				0x182C
#define HAL_TQM_SM_STATES_IX6				0x1830
#define HAL_TQM_SM_STATES_IX7				0x1834
#define HAL_TQM_SM_STATES_IX8				0x1838
#define HAL_TQM_SM_STATES_IX9				0x183C
#define HAL_TQM_SM_STATES_IX10				0x1840
#define HAL_TQM_SM_STATES_IX11				0x1844
#define HAL_TQM_SM_STATES_IX12				0x1848
#define HAL_TQM_SM_STATES_IX13				0x184C
#define HAL_TQM_BANK_SM_STATES_IX0			0x20A0
#define HAL_TQM_BANK_SM_STATES_IX1			0x20A4
#define HAL_TQM_BANK_SM_STATES_IX2			0x20A8
#define HAL_TQM_BANK_SM_STATES_IX3			0x20AC
#define HAL_TQM_BANK_SM_STATES_IX4			0x2124
#define HAL_TQM_BANK_SM_STATES_IX5			0x2128
#define HAL_TQM_BANK_SM_STATES_IX6			0x212C
#define HAL_TQM_BANK_SM_STATES_IX7			0x2130
#define HAL_TQM_BANK_SM_STATES_IX8			0x2134
#define HAL_TQM_BANK_SM_STATES_IX9			0x2138
#define HAL_TQM_BANK_SM_STATES_IX10			0x213C
#define HAL_TQM_BANK_SM_STATES_IX11			0x2140
#define HAL_TQM_BANK_SM_STATES_IX12			0x2144
#define HAL_TQM_BANK_SM_STATES_IX13			0x2148

#define HAL_WBM_ENABLE_MASK				GENMASK(4, 2)
#define HAL_WBM_ENABLE_VALUE				7
#define HAL_WBM_DISABLE_VALUE				0

#define HAL_REO_ENABLE_MASK				BIT(0)

#define HAL_INTERVAL_OF_FETCH_POINTER_MASK		GENMASK(6, 4)
#define HAL_SRNG_IDLE_MASK				BIT(7)
#define HAL_SRNG_IDLE_VALUE				1

#define HAL_SRNG_SM_STATE1_MASK				GENMASK(11, 8)
#define HAL_SRNG_SM_STATE2_MASK				GENMASK(13, 12)
#define HAL_SRNG_SM_STATE3_MASK				GENMASK(8, 7)
#define HAL_SRNG_SM_STATE_VALUE				0

#define HAL_UMAC_UMRCM_SOFTRESET			0xF1E008
#define HAL_UMAC_UMRCM_SOFTRESET_VALUE			0x18F0
#define HAL_UMAC_UMRCM_SOFTRESET_DELAY			5

#define HAL_UMAC_TQM_ENABLE_DELAY			10

#define HAL_REO_SM_ALL_IDLE				0x207C
#define HAL_REO_SM_ALL_IDLE_VALUE			1
#define HAL_REO_SM_ALL_IDLE_MASK			BIT(0)
#define HAL_REO_IDLE_STATES_IX0				0x20D8
#define HAL_REO_IDLE_STATES_IX1				0x20DC
#define HAL_REO_IDLE_STATES_VALUE			0
#define HAL_REO_IDLE_STATES_MASK			GENMASK(31, 0)

#define HAL_PMAC_RXPCU_SIFS_RESP_CTRL			0x2DC
#define HAL_PMAC_RXPCU_UL_TRIGGER_EN_MASK		BIT(3)

#define HAL_PMAC_HWSCH_FLUSH_TLV_MSG_CFG	0x1660
#define HAL_TX_FLUSH_REASON_HALT_TX		0x24
#define HAL_TX_FLUSH_REASON_HALT_RX		0x21
#define HAL_FLUSH_TLV_FLUSH_CODE		0x6
#define HAL_TX_FLUSH_UCODE_MSG_SHIFT		8
#define HAL_FLUSH_TLV_MSG			((HAL_TX_FLUSH_REASON_HALT_TX << \
						 HAL_TX_FLUSH_UCODE_MSG_SHIFT) | \
						 HAL_FLUSH_TLV_FLUSH_CODE)

#define HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_0		0x76C
#define HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_1		0x770
#define HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_0_VALUE	0xffffffff
#define HAL_PMAC_HWSCH_SEND_FLUSH_PAUSE_BITMAP_IX_1_VALUE	0xf

#define HAL_PMAC_HWSCH_FLUSH_STATUS		0x774
#define HAL_PMAC_HWSCH_SEND_FLUSH		0x768
#define HAL_PMAC_HWSCH_FLUSH_CMD		0xa0000000
#define HAL_PMAC_HWSCH_FLUSH_STATUS_MASK	BIT(0)
#define HAL_PMAC_HWSCH_FLUSH_STATUS_VALUE	HAL_PMAC_HWSCH_FLUSH_STATUS_MASK

#define HAL_PMAC_HWSCH_CMD_MGR_GLB_CTRL_IX_0	0x10d4
#define HAL_PMAC_HWSCH_HALT_ALL_SCH_CMD_RINGS	BIT(2)

#define HAL_UMAC_UMCMN_GLOBAL_CFG		0x1a0
#define HAL_UMAC_UMCMN_GLOBAL_CFG_HOLD		BIT(0)
#define HAL_UMCMN_R0_IDLE_SIGNAL		0xD8

#define HAL_PMAC_HWSCH_R0_MTU_FOR_HMAC_CONTROLS_IX_0 0x718
#define HAL_PMAC_PMCMN_MAC_PCU_DIAG_SW			0x1220
#define HAL_PMAC_PMCMN_MAC_PCU_DIAG_SW_HALT_RX_MASK	BIT(1)

#define HAL_DMAC_RXOLE_CLKGATE_DISABLE			0x14
#define HAL_DMAC_RXOLE_CLKGATE_DISABLE_SFM_WRITE_MASK	BIT(7)

#define HAL_PMAC_RXPCU_RX_ERR_INJECTION_CFG			0x6D8
#define HAL_PMAC_RXPCU_RX_ERR_INJECTION_CFG_AMPI_ERR_SEL_MASK	BIT(0)

#define HAL_PMAC_RXPCU_MACRX_ABORT_REQUEST_CTRL			0x698
#define HAL_PMAC_RXPCU_MACRX_ABORT_REQUEST_CTRL_SEND_MASK	BIT(8)

#define HAL_PMAC_RXPCU_RX_FLUSH_CTRL			0x52C
#define HAL_PMAC_RXPCU_RX_FLUSH_CTRL_RX_FLUSH_REQ	BIT(16)
#define HAL_PMAC_RXPCU_RX_FLUSH_CTRL_MIN_DURATION	GENMASK(15, 0)
#define HAL_PMAC_RXPCU_RX_FLUSH_RX_FLUSH_ACK_MASK	BIT(17)
#define HAL_PMAC_RXPCU_RX_FLUSH_RX_FLUSH_ACK_VALUE	1
#define HAL_RX_FLUSH_TIMEOUT_USEC			4000
#define HAL_RX_FLUSH_DELAY_USEC				20

#define HAL_DMAC_DMCMN_ISR_S24				{0x3034, 0x6034}
#define HAL_DMAC_DMCMN_ISR_S24_MASK			BIT(0)
#define HAL_DMAC_DMCMN_ISR_S24_VALUE			1

#define HAL_DMAC_DMCMN_ISR_WLAN_RX_OK			{0x3074, 0x6074}
#define HAL_RING_PREFETCH_TIMER_ENABLE		3
#define HAL_RING_PREFETCH_TIMER_DISABLE		0
#define HAL_PMAC_RX_FLUSH_MIN_DURATION		200
#define HAL_WSI_RESET_DELAY_USEC		100


/* TQM miscelllaneous configuration 1 register address */
#define HAL_TQM_MISC_CFG_1			0xeec
#define HAL_TQM_MISC_CFG_1_SORT_BIT_MASK	BIT(20)

#define HAL_TQM_MAX_SORTED_FLOW			16
#define HAL_TQM_MAX_SORTED_FLOW_ALL_SVC		64

#define HAL_TQM_R0_SORTING_REG			0x00F09620
#define HAL_TQM_R0_SORTING_LATCH		BIT(0)

#define HAL_TQM_R0_ACTIVE_MSDU_CNT		0x00F095D0
#define HAL_TQM_R0_SC0_ACTIVE_MSDU_CNT		0x00F095D4
#define HAL_TQM_R0_SC1_ACTIVE_MSDU_CNT		0x00F095D8
#define HAL_TQM_R0_SC2_ACTIVE_MSDU_CNT		0x00F095DC
#define HAL_TQM_R0_SC3_ACTIVE_MSDU_CNT		0x00F095E0

/* SC0 */
#define HAL_TQM_R0_SC0_SORTED_FLOW_0_LOW		0x00F09624
#define HAL_TQM_R0_SC_SORTED_FLOW_LOW_VALID		BIT(0)
#define HAL_TQM_R0_SC_SORTED_FLOW_LOW_FLOW_NUM		GENMASK(24, 1)

#define HAL_TQM_R0_SC0_SORTED_FLOW_0_HIGH		0x00F09628
#define HAL_TQM_R0_SC_SORTED_FLOW_HIGH_MSDU_CNT		GENMASK(15, 0)

#define HAL_TQM_R0_SC0_SORTED_FLOW_1_LOW		0x00F0962C
#define HAL_TQM_R0_SC0_SORTED_FLOW_1_HIGH		0x00F09630

#define HAL_TQM_R0_SC0_SORTED_FLOW_2_LOW		0x00F09634
#define HAL_TQM_R0_SC0_SORTED_FLOW_2_HIGH		0x00F09638

#define HAL_TQM_R0_SC0_SORTED_FLOW_3_LOW		0x00F0963C
#define HAL_TQM_R0_SC0_SORTED_FLOW_3_HIGH		0x00F09640

#define HAL_TQM_R0_SC0_SORTED_FLOW_4_LOW		0x00F09644
#define HAL_TQM_R0_SC0_SORTED_FLOW_4_HIGH		0x00F09648

#define HAL_TQM_R0_SC0_SORTED_FLOW_5_LOW		0x00F0964C
#define HAL_TQM_R0_SC0_SORTED_FLOW_5_HIGH		0x00F09650

#define HAL_TQM_R0_SC0_SORTED_FLOW_6_LOW		0x00F09654
#define HAL_TQM_R0_SC0_SORTED_FLOW_6_HIGH		0x00F09658

#define HAL_TQM_R0_SC0_SORTED_FLOW_7_LOW		0x00F0965C
#define HAL_TQM_R0_SC0_SORTED_FLOW_7_HIGH		0x00F09660

#define HAL_TQM_R0_SC0_SORTED_FLOW_8_LOW		0x00F09664
#define HAL_TQM_R0_SC0_SORTED_FLOW_8_HIGH		0x00F09668

#define HAL_TQM_R0_SC0_SORTED_FLOW_9_LOW		0x00F0966C
#define HAL_TQM_R0_SC0_SORTED_FLOW_9_HIGH		0x00F09670

#define HAL_TQM_R0_SC0_SORTED_FLOW_10_LOW		0x00F09674
#define HAL_TQM_R0_SC0_SORTED_FLOW_10_HIGH		0x00F09678

#define HAL_TQM_R0_SC0_SORTED_FLOW_11_LOW		0x00F0967C
#define HAL_TQM_R0_SC0_SORTED_FLOW_11_HIGH		0x00F09680

#define HAL_TQM_R0_SC0_SORTED_FLOW_12_LOW		0x00F09684
#define HAL_TQM_R0_SC0_SORTED_FLOW_12_HIGH		0x00F09688

#define HAL_TQM_R0_SC0_SORTED_FLOW_13_LOW		0x00F0968C
#define HAL_TQM_R0_SC0_SORTED_FLOW_13_HIGH		0x00F09690

#define HAL_TQM_R0_SC0_SORTED_FLOW_14_LOW		0x00F09694
#define HAL_TQM_R0_SC0_SORTED_FLOW_14_HIGH		0x00F09698

#define HAL_TQM_R0_SC0_SORTED_FLOW_15_LOW		0x00F0969C
#define HAL_TQM_R0_SC0_SORTED_FLOW_15_HIGH		0x00F096A0

/* SC1 */
#define HAL_TQM_R0_SC1_SORTED_FLOW_0_LOW		0x00F096A4
#define HAL_TQM_R0_SC1_SORTED_FLOW_0_HIGH		0x00F096A8

#define HAL_TQM_R0_SC1_SORTED_FLOW_1_LOW		0x00F096AC
#define HAL_TQM_R0_SC1_SORTED_FLOW_1_HIGH		0x00F096B0

#define HAL_TQM_R0_SC1_SORTED_FLOW_2_LOW		0x00F096B4
#define HAL_TQM_R0_SC1_SORTED_FLOW_2_HIGH		0x00F096B8

#define HAL_TQM_R0_SC1_SORTED_FLOW_3_LOW		0x00F096BC
#define HAL_TQM_R0_SC1_SORTED_FLOW_3_HIGH		0x00F096C0

#define HAL_TQM_R0_SC1_SORTED_FLOW_4_LOW		0x00F096C4
#define HAL_TQM_R0_SC1_SORTED_FLOW_4_HIGH		0x00F096C8

#define HAL_TQM_R0_SC1_SORTED_FLOW_5_LOW		0x00F096CC
#define HAL_TQM_R0_SC1_SORTED_FLOW_5_HIGH		0x00F096D0

#define HAL_TQM_R0_SC1_SORTED_FLOW_6_LOW		0x00F096D4
#define HAL_TQM_R0_SC1_SORTED_FLOW_6_HIGH		0x00F096D8

#define HAL_TQM_R0_SC1_SORTED_FLOW_7_LOW		0x00F096DC
#define HAL_TQM_R0_SC1_SORTED_FLOW_7_HIGH		0x00F096E0

#define HAL_TQM_R0_SC1_SORTED_FLOW_8_LOW		0x00F096E4
#define HAL_TQM_R0_SC1_SORTED_FLOW_8_HIGH		0x00F096E8

#define HAL_TQM_R0_SC1_SORTED_FLOW_9_LOW		0x00F096EC
#define HAL_TQM_R0_SC1_SORTED_FLOW_9_HIGH		0x00F096F0

#define HAL_TQM_R0_SC1_SORTED_FLOW_10_LOW		0x00F096F4
#define HAL_TQM_R0_SC1_SORTED_FLOW_10_HIGH		0x00F096F8

#define HAL_TQM_R0_SC1_SORTED_FLOW_11_LOW		0x00F096FC
#define HAL_TQM_R0_SC1_SORTED_FLOW_11_HIGH		0x00F09700

#define HAL_TQM_R0_SC1_SORTED_FLOW_12_LOW		0x00F09704
#define HAL_TQM_R0_SC1_SORTED_FLOW_12_HIGH		0x00F09708

#define HAL_TQM_R0_SC1_SORTED_FLOW_13_LOW		0x00F0970C
#define HAL_TQM_R0_SC1_SORTED_FLOW_13_HIGH		0x00F09710

#define HAL_TQM_R0_SC1_SORTED_FLOW_14_LOW		0x00F09714
#define HAL_TQM_R0_SC1_SORTED_FLOW_14_HIGH		0x00F09718

#define HAL_TQM_R0_SC1_SORTED_FLOW_15_LOW		0x00F0971C
#define HAL_TQM_R0_SC1_SORTED_FLOW_15_HIGH		0x00F09720

/* SC2 */
#define HAL_TQM_R0_SC2_SORTED_FLOW_0_LOW		0x00F09724
#define HAL_TQM_R0_SC2_SORTED_FLOW_0_HIGH		0x00F09728

#define HAL_TQM_R0_SC2_SORTED_FLOW_1_LOW		0x00F0972C
#define HAL_TQM_R0_SC2_SORTED_FLOW_1_HIGH		0x00F09730

#define HAL_TQM_R0_SC2_SORTED_FLOW_2_LOW		0x00F09734
#define HAL_TQM_R0_SC2_SORTED_FLOW_2_HIGH		0x00F09738

#define HAL_TQM_R0_SC2_SORTED_FLOW_3_LOW		0x00F0973C
#define HAL_TQM_R0_SC2_SORTED_FLOW_3_HIGH		0x00F09740

#define HAL_TQM_R0_SC2_SORTED_FLOW_4_LOW		0x00F09744
#define HAL_TQM_R0_SC2_SORTED_FLOW_4_HIGH		0x00F09748

#define HAL_TQM_R0_SC2_SORTED_FLOW_5_LOW		0x00F0974C
#define HAL_TQM_R0_SC2_SORTED_FLOW_5_HIGH		0x00F09750

#define HAL_TQM_R0_SC2_SORTED_FLOW_6_LOW		0x00F09754
#define HAL_TQM_R0_SC2_SORTED_FLOW_6_HIGH		0x00F09758

#define HAL_TQM_R0_SC2_SORTED_FLOW_7_LOW		0x00F0975C
#define HAL_TQM_R0_SC2_SORTED_FLOW_7_HIGH		0x00F09760

#define HAL_TQM_R0_SC2_SORTED_FLOW_8_LOW		0x00F09764
#define HAL_TQM_R0_SC2_SORTED_FLOW_8_HIGH		0x00F09768

#define HAL_TQM_R0_SC2_SORTED_FLOW_9_LOW		0x00F0976C
#define HAL_TQM_R0_SC2_SORTED_FLOW_9_HIGH		0x00F09770

#define HAL_TQM_R0_SC2_SORTED_FLOW_10_LOW		0x00F09774
#define HAL_TQM_R0_SC2_SORTED_FLOW_10_HIGH		0x00F09778

#define HAL_TQM_R0_SC2_SORTED_FLOW_11_LOW		0x00F0977C
#define HAL_TQM_R0_SC2_SORTED_FLOW_11_HIGH		0x00F09780

#define HAL_TQM_R0_SC2_SORTED_FLOW_12_LOW		0x00F09784
#define HAL_TQM_R0_SC2_SORTED_FLOW_12_HIGH		0x00F09788

#define HAL_TQM_R0_SC2_SORTED_FLOW_13_LOW		0x00F0978C
#define HAL_TQM_R0_SC2_SORTED_FLOW_13_HIGH		0x00F09790

#define HAL_TQM_R0_SC2_SORTED_FLOW_14_LOW		0x00F09794
#define HAL_TQM_R0_SC2_SORTED_FLOW_14_HIGH		0x00F09798

#define HAL_TQM_R0_SC2_SORTED_FLOW_15_LOW		0x00F0979C
#define HAL_TQM_R0_SC2_SORTED_FLOW_15_HIGH		0x00F097A0

/* SC3 */
#define HAL_TQM_R0_SC3_SORTED_FLOW_0_LOW		0x00F097A4
#define HAL_TQM_R0_SC3_SORTED_FLOW_0_HIGH		0x00F097A8

#define HAL_TQM_R0_SC3_SORTED_FLOW_1_LOW		0x00F097AC
#define HAL_TQM_R0_SC3_SORTED_FLOW_1_HIGH		0x00F097B0

#define HAL_TQM_R0_SC3_SORTED_FLOW_2_LOW		0x00F097B4
#define HAL_TQM_R0_SC3_SORTED_FLOW_2_HIGH		0x00F097B8

#define HAL_TQM_R0_SC3_SORTED_FLOW_3_LOW		0x00F097BC
#define HAL_TQM_R0_SC3_SORTED_FLOW_3_HIGH		0x00F097C0

#define HAL_TQM_R0_SC3_SORTED_FLOW_4_LOW		0x00F097C4
#define HAL_TQM_R0_SC3_SORTED_FLOW_4_HIGH		0x00F097C8

#define HAL_TQM_R0_SC3_SORTED_FLOW_5_LOW		0x00F097CC
#define HAL_TQM_R0_SC3_SORTED_FLOW_5_HIGH		0x00F097D0

#define HAL_TQM_R0_SC3_SORTED_FLOW_6_LOW		0x00F097D4
#define HAL_TQM_R0_SC3_SORTED_FLOW_6_HIGH		0x00F097D8

#define HAL_TQM_R0_SC3_SORTED_FLOW_7_LOW		0x00F097DC
#define HAL_TQM_R0_SC3_SORTED_FLOW_7_HIGH		0x00F097E0

#define HAL_TQM_R0_SC3_SORTED_FLOW_8_LOW		0x00F097E4
#define HAL_TQM_R0_SC3_SORTED_FLOW_8_HIGH		0x00F097E8

#define HAL_TQM_R0_SC3_SORTED_FLOW_9_LOW		0x00F097EC
#define HAL_TQM_R0_SC3_SORTED_FLOW_9_HIGH		0x00F097F0

#define HAL_TQM_R0_SC3_SORTED_FLOW_10_LOW		0x00F097F4
#define HAL_TQM_R0_SC3_SORTED_FLOW_10_HIGH		0x00F097F8

#define HAL_TQM_R0_SC3_SORTED_FLOW_11_LOW		0x00F097FC
#define HAL_TQM_R0_SC3_SORTED_FLOW_11_HIGH		0x00F09800

#define HAL_TQM_R0_SC3_SORTED_FLOW_12_LOW		0x00F09804
#define HAL_TQM_R0_SC3_SORTED_FLOW_12_HIGH		0x00F09808

#define HAL_TQM_R0_SC3_SORTED_FLOW_13_LOW		0x00F0980C
#define HAL_TQM_R0_SC3_SORTED_FLOW_13_HIGH		0x00F09810

#define HAL_TQM_R0_SC3_SORTED_FLOW_14_LOW		0x00F09814
#define HAL_TQM_R0_SC3_SORTED_FLOW_14_HIGH		0x00F09818

#define HAL_TQM_R0_SC3_SORTED_FLOW_15_LOW		0x00F0981C
#define HAL_TQM_R0_SC3_SORTED_FLOW_15_HIGH		0x00F09820

#define HAL_UMAC_WBM_LOW_WATERMARK_SHIFT 16
#define HAL_UMAC_WBM_LOW_WATERMARK_DIFF 32
#define HAL_UMAC_WBM_MAX_WATERMARK_CFG_REGS 5


#define HAL_UMAC_WBM_WATERMARK_DISABLE	BIT(13)
#define HAL_UMAC_WBM_MISC_CTRL	0x0040

#define HAL_UMAC_WBM_BUFF_DESC_RING_CFG1 \
	{0x2300, 0x2304, 0x2308, 0x230C, 0x2310}

#define HAL_UMAC_WBM_BUFF_DESC_RING_CFG2 \
	{0x2314, 0x2318, 0x231C, 0x2320, 0x2324}

#define HAL_UMAC_WBM_BUFF_DESC_RING_CFG3 \
	{0x2328, 0x232C, 0x2330, 0x2334, 0x2338}

enum ath12k_hal_reset_ring_type {
	/* UMAC2UMAC SRNGs */
	HAL_TCL_GSE_CMD_RING,
	HAL_TCL_GSE_STATUS_RING,
	HAL_TCL2FW_OUTPUT_RING,
	HAL_TQM_HIPRI_CMD_RING,
	HAL_TQM_HIPRI1_CMD_RING,
	HAL_TQM_LOWPRI_CMD_RING,
	HAL_FW2TQM_ENT_RING,
	HAL_TQM_HIPRI_STATUS_RING,
	HAL_TQM_HIPRI1_STATUS_RING,
	HAL_TQM_LOWPRI_STATUS_RING,
	HAL_TQM2FW_REL_BUF_RING,
	HAL_FW2TQM_REL_RING,
	HAL_WBM_ERR_REL_RING,
	HAL_REO2FW_DEST_RING,
	HAL_REO2FW_MGMT_RING,
	HAL_TQM_ERR_REL_RING,
	HAL_TCL_TCL2TQM_ENT_RING,
	HAL_TQM_TCL2TQM_ENT_RING,
	HAL_WBM_WBM2TQM_DESC_RING,
	HAL_TQM_WBM2TQM_DESC_RING,
	HAL_TQM_TQM2WBM_REL_RING,
	HAL_WBM_TQM2WBM_REL_RING,
	HAL_WBM_WBM2REO_DESC_RING,
	HAL_REO_WBM2REO_DESC_RING,
	HAL_REO_REO2WBM_REL_RING,
	HAL_WBM_REO2WBM_REL_RING,
	HAL_FW2WBM_REL_RING,
	HAL_WBM_IDLE_DESC_RING,

	/* UMAC2UMAC MLO SRNGs */
	HAL_TQM2TQM_IN1_MLO_IPC_RING,
	HAL_TQM2TQM_IN2_MLO_IPC_RING,
	HAL_TQM2TQM_IN3_MLO_IPC_RING,
	HAL_TQM2TQM_IN4_MLO_IPC_RING,
	HAL_REO2REO_OUT1_MLO_IPC_RING,
	HAL_REO2REO_OUT2_MLO_IPC_RING,
	HAL_REO2REO_OUT3_MLO_IPC_RING,
	HAL_REO2REO_OUT4_MLO_IPC_RING,
	HAL_TQM2TQM_OUT1_MLO_IPC_RING,
	HAL_TQM2TQM_OUT2_MLO_IPC_RING,
	HAL_TQM2TQM_OUT3_MLO_IPC_RING,
	HAL_TQM2TQM_OUT4_MLO_IPC_RING,
	HAL_TCL2TCL_MLO_OUT1_IPC_RING,
	HAL_TCL2TCL_MLO_OUT2_IPC_RING,
	HAL_TCL2TCL_MLO_OUT3_IPC_RING,
	HAL_TCL2TCL_MLO_OUT4_IPC_RING,

	/* DMAC2UMAC MLO SRNGs */
	HAL_RXDMA2REO_MLO0_IPC_RING,
	HAL_RXDMA2REO_MLO1_IPC_RING,
	HAL_RXDMA2REO_MLO2_IPC_RING,
	HAL_RXDMA2REO_MLO3_IPC_RING,
	HAL_RXDMA2REO_MLO4_IPC_RING,
	HAL_WBM2RXDMA_0_BUF_RING_IPC_RING,
	HAL_WBM2RXDMA_1_BUF_RING_IPC_RING,
	HAL_WBM2RXDMA_2_BUF_RING_IPC_RING,
	HAL_WBM2RXDMA_LINK_RING_IPC_RING,
	HAL_RXDMA2REO_MLO_IPC_RING,
	HAL_WBM2RXDMA_LINK_MLO1_IPC_RING,
	HAL_WBM2RXDMA_LINK_MLO2_IPC_RING,
	HAL_WBM2RXDMA_LINK_MLO3_IPC_RING,
	HAL_WBM2RXDMA_LINK_MLO4_IPC_RING,
	HAL_WBM2RXDMA_LINK_MLO5_IPC_RING,
	HAL_WBM2RXDMA_SW0_BUFF_MLO1_IPC_RING,
	HAL_WBM2RXDMA_SW0_BUFF_MLO2_IPC_RING,
	HAL_WBM2RXDMA_SW0_BUFF_MLO3_IPC_RING,
	HAL_WBM2RXDMA_SW0_BUFF_MLO4_IPC_RING,
	HAL_WBM2RXDMA_SW0_BUFF_MLO5_IPC_RING,
	HAL_WBM2RXDMA_SW1_BUFF_MLO1_IPC_RING,
	HAL_WBM2RXDMA_SW1_BUFF_MLO2_IPC_RING,
	HAL_WBM2RXDMA_SW1_BUFF_MLO3_IPC_RING,
	HAL_WBM2RXDMA_SW1_BUFF_MLO4_IPC_RING,
	HAL_WBM2RXDMA_SW1_BUFF_MLO5_IPC_RING,
	HAL_WBM2RXDMA_PPE_BUFF_MLO1_IPC_RING,
	HAL_WBM2RXDMA_PPE_BUFF_MLO2_IPC_RING,
	HAL_WBM2RXDMA_PPE_BUFF_MLO3_IPC_RING,
	HAL_WBM2RXDMA_PPE_BUFF_MLO4_IPC_RING,
	HAL_WBM2RXDMA_PPE_BUFF_MLO5_IPC_RING,
	HAL_WBM2RXOLE_ASE_CMD_MLO1_IPC_RING,
	HAL_WBM2RXOLE_ASE_CMD_MLO2_IPC_RING,
	HAL_WBM2RXOLE_ASE_CMD_MLO3_IPC_RING,
	HAL_WBM2RXOLE_ASE_CMD_MLO4_IPC_RING,
	HAL_WBM2RXOLE_ASE_CMD_MLO5_IPC_RING,
	HAL_WBM2RXOLE_FSE_CMD_MLO1_IPC_RING,
	HAL_WBM2RXOLE_FSE_CMD_MLO2_IPC_RING,
	HAL_WBM2RXOLE_FSE_CMD_MLO3_IPC_RING,
	HAL_WBM2RXOLE_FSE_CMD_MLO4_IPC_RING,
	HAL_WBM2RXOLE_FSE_CMD_MLO5_IPC_RING,
	HAL_WBM2RXOLE_ASE_CMD_RING_IPC_RING,
	HAL_WBM2RXOLE_FSE_CMD_RING_IPC_RING,
	HAL_RESET_RING_TYPE_MAX,
};

struct ath12k_hal_reset_rings {
	u32 consumer_prefetch_timer;
	u32 srng_misc_reg;
	u32 hp;
	u32 consumer_producer_mlo;
	u32 mlo_doorbell_press;
};

/**
 * enum hal_wifi8_rx_buf_return_buf_manager - manager for returned rx buffers
 *
 * @HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST: Buffer returned to WBM idle buffer list
 * @HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST: Descriptor returned to WBM idle
 *	descriptor list, where the device 0 WBM is chosen in case of a multi-device config
 * @HAL_RX_BUF_RBM_FW_CHIP1_BM: Buffer returned to chip 1 FW
 * @HAL_RX_BUF_RBM_FW_CHIP2_BM: Buffer returned to chip 2 FW
 * @HAL_RX_BUF_RBM_FW_CHIP0_BM: Buffer returned to chip 0 FW
 * @HAL_RX_BUF_RBM_SW0_BM: Buffer returned to SW ring 0
 * @HAL_RX_BUF_RBM_SW1_BM: Buffer returned to SW ring 1
 * @HAL_RX_BUF_RBM_SW2_BM: Buffer returned to SW ring 2
 * @HAL_RX_BUF_RBM_SW3_BM: Buffer returned to SW ring 3
 * @HAL_RX_BUF_RBM_SW4_BM: Buffer returned to SW ring 4
 * @HAL_RX_BUF_RBM_SW5_BM: Buffer returned to SW ring 5
 * @HAL_RX_BUF_RBM_SW6_BM: Buffer returned to SW ring 6
 * @HAL_RX_BUF_RBM_FW_CHIP3_BM: Buffer returned to chip 3 FW
 * @HAL_RX_BUF_RBM_SW7_BM: Buffer returned to SW ring 7
 * @HAL_RX_BUF_RBM_FW_CHIP4_BM: Buffer returned to chip 4 FW
 *
 * Description:
 *	Consumer: WBM
 *	Producer: SW/FW
 *
 *	In case of 'NULL' pointer, this field is set to 0.
 *
 *	Indicates to which buffer manager the buffer, MSDU_EXTENSION descriptor,
 *	or link descriptor being pointed to shall be returned after the frame
 *	has been processed. It is used by WBM for routing purposes.
 *
 *	<legal 0-14>
 */

enum hal_wifi8_rx_buf_return_buf_manager {
	HAL_RX_BUF_RBM_WBM_IDLE_BUF_LIST,
	HAL_RX_BUF_RBM_WBM_DEV0_IDLE_DESC_LIST,
	HAL_RX_BUF_RBM_FW_CHIP1_BM,
	HAL_RX_BUF_RBM_FW_CHIP2_BM,
	HAL_RX_BUF_RBM_FW_CHIP0_BM,
	HAL_RX_BUF_RBM_SW0_BM,
	HAL_RX_BUF_RBM_SW1_BM,
	HAL_RX_BUF_RBM_SW2_BM,
	HAL_RX_BUF_RBM_SW3_BM,
	HAL_RX_BUF_RBM_SW4_BM,
	HAL_RX_BUF_RBM_SW5_BM,
	HAL_RX_BUF_RBM_SW6_BM,
	HAL_RX_BUF_RBM_FW_CHIP3_BM,
	HAL_RX_BUF_RBM_SW7_BM,
	HAL_RX_BUF_RBM_FW_CHIP4_BM,
};

enum ath12k_hal_tx_pkt_ctrl_config {
	HAL_TX_PACKET_CONTROL_CONFIG_DEFAULT,
	HAL_TX_PACKET_CONTROL_CONFIG_TO_FW_EXCEPTION,
	HAL_TX_PACKET_CONTROL_CONFIG_DROP_PKT,
	HAL_TX_PACKET_CONTROL_CONFIG_PKT_TO_TQM,
	HAL_TX_PACKET_CONTROL_CONFIG_MEC_NOTIFY_TX,
	HAL_TX_PACKET_CONTROL_CONFIG_MEC_NOTIFY_TX_EXCEPTION,
};

/* Add any other errors here and return them in
 * ath12k_hal_rx_desc_get_err().
 */

struct hal_wbm_idle_scatter_list {
	dma_addr_t paddr;
	struct hal_wbm_link_desc *vaddr;
};

/* TODO: confirm src_info relacement from HW DESC */
struct rx_mpdu_desc_info {
	union {
		struct {
			u32 msdu_count				:  8,
			    fragment_flag			:  1,
			    mpdu_retry_bit			:  1,
			    ampdu_flag				:  1,
			    bar_frame				:  1,
			    pn_fields_contain_valid_info	:  1,
			    raw_mpdu				:  1,
			    more_fragment_flag			:  1,
			    vdev_id                             :  8,
			    reserved				:  2,
			    flow_idx_invalid                    :  1,
			    flow_idx_timeout                    :  1,
			    mpdu_qos_control_valid		:  1,
			    tid					:  4;
		};
		u32 info0;
	};
	union {
		u32 peer_meta_data;
		struct {
			u32 peer_id		: 16,
			flow_metadata		: 16;
		} flow_info;
	};
	union {
		struct {
			u32 mgmt_pkt				:  1,
			    rxdma_push_reason			:  2,
			    rxdma_error_code			:  5,
			    reo_dest_buffer_type		:  1,
			    release_source_module		:  3,
			    msdu_link_desc_index		:  4,
			    ll_pkt				:  1,
			    high_priority_pkt			:  2,
			    src_link_id				:  3,
			    reo_push_reason			:  2,
			    reo_error_code			:  5,
			    groupcast_mpdu			:  1;
		};
		u32 info1;
	};
};

struct rx_msdu_desc_info {
	u32 first_msdu		        :  1,
	    last_msdu			:  1,
	    msdu_continuation           :  1,
	    msdu_length                 : 14,
	    msdu_drop                   :  1,
	    sa_is_valid                 :  1,
	    da_is_valid                 :  1,
	    da_is_mcbc                  :  1,
	    l3_header_padding_msb       :  1,
	    tcp_udp_chksum_fail         :  1,
	    ip_chksum_fail              :  1,
	    fr_ds                       :  1,
	    to_ds                       :  1,
	    intra_bss                   :  1,
	    dest_chip_id                :  2,
	    ra_is_mcbc			:  1,
	    reserved			:  2;
};

struct rx_tlv_info_1 {
	u32 freq;
	u32 decap			: 2,
	    rate_mcs			: 4,
	    nss				: 7,
	    sgi				: 2,
	    is_decrypted		: 1,
	    mesh_ctrl_present		: 1,
	    pkt_type			: 4,
	    bw				: 3,
	    encrypt_info_valid		: 1,
	    mpdu_frame_control_valid	: 1,
	    encrypted			: 1;
};

struct hal_rx_spd_data {
	union {
		u64 info4;
		u8 *vaddr;
		struct ath12k_buffer_addr buf_addr;
	};
	union {
		u64 info3;
		struct sk_buff *msdu;
	};

	struct rx_mpdu_desc_info rx_mpdu_info;

	union {
		u32 info2;
		struct rx_msdu_desc_info rx_msdu_info;
	};

	union {
		u32 info0;
		struct {
			u32 cookie_conversion_status	:  1,
			    reo_delink_error		:  1,
			    sw_buffer_cookie		:  20,
			    reserved_6a			:  4,
			    c_tdma_lut_ptr		:  6;
		};
	};

	union {
		u32 info1;
		struct {
			u32 phy_lmac_latency		:  8,
			    umac_latency		:  8,
			    sw_exception		:  1,
			    backpressure_drop		:  1,
			    flow_idx_valid		:  1,
			    rx_sawf_msdu_dropped	:  1,
			    ring_id			:  8,
			    looping_count		:  4;
		};
	};

	struct rx_tlv_info_1 tlv_info;

	union {
		u8 flags;
		struct {
			u8 first_sg_frame		: 1,
			    last_sg_frame		: 1,
			    is_frag			: 1,
			    cce_match                   : 1,
			    rsvd0			: 4;
		};
	};

	u8	reserved_0;
	u16	cce_metadata;
	u32	reserved_1;

	u64	reserved_2;
} __packed;

static_assert(sizeof(struct hal_rx_spd_data) == 64,
	      "size of struct hal_rx_spd_data is not 64 bytes!");

#define HAL_AST_ENTRY_INFO0_MAC_ADDR_31_0		GENMASK(31, 0)
#define HAL_AST_ENTRY_INFO1_MAC_ADDR_47_32		GENMASK(15, 0)
#define HAL_AST_ENTRY_INFO1_MEC				BIT(17)
#define HAL_AST_ENTRY_INFO1_LINK_ID			GENMASK(21, 19)
#define HAL_AST_ENTRY_INFO1_ENTRY_VALID			BIT(22)
#define HAL_AST_ENTRY_INFO1_USE_ADDRX_SEARCH		BIT(27)
#define HAL_AST_ENTRY_INFO1_NUM_WHO_CLASSIFY_INFO	GENMASK(31, 30)
#define HAL_AST_ENTRY_INFO2_WHO_CLASSIFY_INFO_31_0	GENMASK(31, 0)
#define HAL_AST_ENTRY_INFO3_WHO_CLASSIFY_INFO_39_32	GENMASK(7, 0)
#define HAL_AST_ENTRY_INFO5_SW_PEER_ID			GENMASK(31, 16)
#define HAL_AST_ENTRY_INFO6_VDEV_ID			GENMASK(7, 0)

struct hal_ast_entry {
	__le32 info0;
	__le32 info1;
	__le32 info2;
	__le32 info3;
	__le32 info4;
	__le32 info5;
	__le32 info6;
	__le32 info7;
} __packed;

struct ath12k_hal_ast_param {
	u32 skid_len:8,
	    ast_cache_en:1,
	    ast_cache_failure_en:1,
	    reserved:22;
	u32 ase_hash_key1;
	u32 ase_hash_key2;
	u32 ase_hash_key3;
	dma_addr_t paddr;
	u16 num_ast_entries;
};

#define HAL_TQM_FAIL_DROP_MAP(map, reason, category) \
	do { \
		u32 __shift = 2 * (reason % 10); \
		(map) |= (((category) & 0x3) << __shift); \
	} while (0)

/**
 * enum tqm_release_reason_map - 2-bit value for TQM release reasons
 * @HAL_TQM_RELEASE_REASON_FAILED: Failed packets
 * @HAL_TQM_RELEASE_REASON_DROP1: Dropped due to AQM (Active Queue Management)
 * @HAL_TQM_RELEASE_REASON_DROP2: Dropped due to congestion
 * @HAL_TQM_RELEASE_REASON_IGNORE: Ignored packets
 */
enum tqm_release_reason_category {
	HAL_TQM_RELEASE_REASON_FAILED = 0,
	HAL_TQM_RELEASE_REASON_DROP1  = 1,
	HAL_TQM_RELEASE_REASON_DROP2  = 2,
	HAL_TQM_RELEASE_REASON_IGNORE = 3
};

enum tasc_band_index {
	HAL_TASC_BAND_0 = 0,
	HAL_TASC_BAND_1 = 1,
	HAL_TASC_BAND_2 = 2,
	HAL_TASC_BAND_3 = 3,
	HAL_TASC_BAND_4 = 4,
	HAL_TASC_BAND_MAX,
};

enum tqm_release_reason {
	HAL_TQM_RR_FRAME_ACKED = 0,
	HAL_TQM_RR_REM_CMD_REM,
	HAL_TQM_RR_REM_CMD_TX,
	HAL_TQM_RR_REM_CMD_NOTX,
	HAL_TQM_RR_REM_CMD_AGED,
	HAL_TQM_FW_REASON1,
	HAL_TQM_FW_REASON2,
	HAL_TQM_FW_REASON3,
	HAL_TQM_RR_REM_CMD_DISABLE_QUEUE,
	HAL_TQM_RR_REM_CMD_TILL_NONMATCHING,
	HAL_TQM_RR_DROP_THRESHOLD,
	HAL_TQM_RR_LINK_DESC_UNAVAILABLE,
	HAL_TQM_RR_DROP_OR_INVALID_MSDU,
	HAL_TQM_RR_MULTICAST_DROP,
	HAL_TQM_RR_VDEV_MISMATCH_DROP,
	HAL_TQM_RR_GEN_CMD_USED_TREE_EXT,
	HAL_TQM_RR_TCL_DROP_FROM_PEER_CCE_OR_FLOW_TABLE,
	HAL_TQM_RR_TCL_MULTICAST_REINJECT_FOR_VDEV,
	HAL_TQM_RR_TCL_MEC_SEARCH_FAIL_FOR_VDEV,
	HAL_TQM_RR_TCL_ASE_SEARCH_FAIL,
	HAL_TQM_RR_TCL_SMD_ROAMING_DROP,
	HAL_TQM_RR_TCL_STRIP_VLAN_TCI_MISMATCH_DROP,
	HAL_TQM_RR_TCL_MEC_KEEP_ALIVE_FOR_VDEV,
	HAL_TQM_RR_TCL_RESERVED_DROP_REASON1,
	HAL_TQM_RR_TCL_RESERVED_DROP_REASON2,
	HAL_TQM_RR_TQM_REM_MSDU_SMD_ROAMING,
	HAL_TQM_RR_TQM_REM_MPDU_SMD_ROAMING,
	HAL_TQM_RR_TQM_RESERVED_DROP_REASON3,
	HAL_TQM_RR_TQM_RESERVED_DROP_REASON4
};

#define RX_DROP_MAP(map, reason, category) \
	do { \
		u32 __shift = 2 * reason; \
		(map) |= (((category) & 0x3) << __shift); \
	} while (0)

enum rx_drop_reason {
	HAL_RX_NO_DROP,
	HAL_RX_BACKPRESSURE_DROP,
	HAL_RX_MSDU_DROP,
	HAL_RX_MSDU_BACKPRESSURE_DROP,
	HAL_RX_SDWF_DROP,
	HAL_RX_SDWF_BACKPRESSURE_DROP,
	HAL_RX_SDWF_MSDU_DROP,
	HAL_RX_SDWF_MSDU_BACKPRESSURE_DROP
};

enum rx_drop_reason_category {
	HAL_RX_DROPPED_REASON_1,
	HAL_RX_DROPPED_REASON_2,
	HAL_RX_IGNORE_MAP
};

/* Register address to configure telemetry stats */
#define HAL_TELEMETRY_PEER_CLK_CYCLE_ADDR		0xF10588
#define HAL_TELEMETRY_PEER_CLK_CYCLE_DEFAULT_VAL	2097152

#define HAL_TX_TELEMETRY_GLOBAL_CTRL_ADDR		0xF10584
#define HAL_TX_TELEMETRY_GLOBAL_CTRL_PEER_STATS_ENABLE	BIT(0)

#define HAL_TX_PEER_STATS_WINDOW_SIZE_CFG_ADDR		0xF10000
#define HAL_TX_PEER_STATS_WINDOW_SIZE_CFG		GENMASK(11, 0)

#define HAL_TX_NUM_OF_VALID_PEER_CFG_ADDR		0xF10004
#define HAL_TX_NUM_OF_VALID_UNICAST_PEER		GENMASK(10, 0)
#define HAL_TX_NUM_OF_VALID_DL_GCAST_PEER		GENMASK(17, 11)
#define HAL_TX_NUM_OF_VALID_UL_GCAST_PEER		GENMASK(21, 18)

#define HAL_TQM_RELEASE_REASON_MAP_1_ADDR		0xF10008
#define HAL_TQM_RELEASE_REASON_MAP_1_FOR_0_9		GENMASK(29, 0)

#define HAL_TQM_RELEASE_REASON_MAP_2_ADDR		0xF1000C
#define HAL_TQM_RELEASE_REASON_MAP_2_FOR_10_19		GENMASK(29, 0)

#define HAL_TX_PEER_STATS_CONFIG_CTRL_ADDR		0xF10014
#define HAL_TX_PEER_TELEMETRY_STATS_ID			GENMASK(10, 0)
#define HAL_TX_PEER_STATS_ENABLE_TELEMETRY_STATS_ID	BIT(11)
#define HAL_TX_PEER_STATS_CLEAR_TELEMETRY_STATS_ID	BIT(12)

#define HAL_TX_PEER_STATS_CFG0_ADDR			0xF10018
#define HAL_TX_PEER_STATS_BAND_INDEX_0			GENMASK(10, 0)
#define HAL_TX_PEER_STATS_BAND_INDEX_1			GENMASK(21, 11)

#define HAL_TX_PEER_STATS_CFG1_ADDR			0xF1001C
#define HAL_TX_PEER_STATS_BAND_INDEX_2			GENMASK(10, 0)
#define HAL_TX_PEER_STATS_BAND_INDEX_3			GENMASK(21, 11)

#define HAL_TX_PEER_STATS_CFG2_ADDR			0xF10020
#define HAL_TX_PEER_STATS_BAND_INDEX_4			GENMASK(10, 0)

#define HAL_TX_PEER_STATS_BAND_INDEX_MSB		BIT(31)

#define HAL_TX_PEER_STATS_NUM_TRANSMISSIONS_CFG_ADDR	0xF10580

#define HAL_TX_UL_GCAST_PEER_ID_REMAP0			0xF10460
#define HAL_TX_UL_GCAST_PEER_ID_REMAP1			0xF10464
#define HAL_TX_UL_GCAST_PEER_ID_REMAP2			0xF10468
#define HAL_TX_UL_GCAST_PEER_ID_REMAP3			0xF1046C

#define HAL_TX_UL_PEER_ID_REMAP1			GENMASK(10, 0)
#define HAL_TX_UL_PEER_ID_REMAP2			GENMASK(21, 11)

#define HAL_TX_UL_GCAST_PEER_ID_1616			1616
#define HAL_TX_UL_GCAST_PEER_ID_1617			1617
#define HAL_TX_UL_GCAST_PEER_ID_1618			1618
#define HAL_TX_UL_GCAST_PEER_ID_1619			1619
#define HAL_TX_UL_GCAST_PEER_ID_1620			1620
#define HAL_TX_UL_GCAST_PEER_ID_1621			1621
#define HAL_TX_UL_GCAST_PEER_ID_1622			1622
#define HAL_TX_UL_GCAST_PEER_ID_1623			1623

#define HAL_RX_TELEMETRY_GLOBAL_CTRL_ADDR		0xF2645C
#define HAL_RX_TELEMETRY_GLOBAL_CTRL_PEER_STATS_ENABLE	BIT(0)

#define HAL_RX_PEER_STATS_WINDOW_SIZE_CFG_ADDR		0xF26000
#define HAL_RX_PEER_STATS_WINDOW_SIZE_CFG		GENMASK(11, 0)

#define HAL_RX_NUM_OF_VALID_PEER_CFG_ADDR		0xF26004
#define HAL_RX_NUM_OF_VALID_UNICAST_PEER		GENMASK(10, 0)
#define HAL_RX_NUM_OF_VALID_GCAST_PEER			GENMASK(17, 11)

#define HAL_RX_DROP_REASON_MAP_ADDR			0xF26458

#define HAL_RX_PEER_STATS_CONFIG_CTRL_ADDR		0xF26008
#define HAL_RX_PEER_TELEMETRY_STATS_ID		GENMASK(10, 0)
#define HAL_RX_PEER_STATS_ENABLE_STATS_ID	BIT(11)
#define HAL_RX_PEER_STATS_CLEAR_STATS_ID	BIT(12)

#define HAL_RX_PEER_STATS_CFG0_ADDR			0xF2600C
#define HAL_RX_PEER_STATS_BAND_INDEX_0			GENMASK(10, 0)
#define HAL_RX_PEER_STATS_BAND_INDEX_1			GENMASK(21, 11)

#define HAL_RX_PEER_STATS_CFG1_ADDR			0xF26010
#define HAL_RX_PEER_STATS_BAND_INDEX_2			GENMASK(10, 0)
#define HAL_RX_PEER_STATS_BAND_INDEX_3			GENMASK(21, 11)

#define HAL_RX_PEER_STATS_CFG2_ADDR			0xF26014
#define HAL_RX_PEER_STATS_BAND_INDEX_4			GENMASK(10, 0)

#define HAL_RX_PEER_STATS_BAND_INDEX_MSB		BIT(31)

void ath12k_wifi8_hal_ce_dst_setup(struct ath12k_base *ab,
				   struct hal_srng *srng, int ring_num);
void ath12k_wifi8_hal_srng_dst_hw_init(struct ath12k_base *ab,
				       struct hal_srng *srng, u32 restore_idx);
void ath12k_wifi8_hal_srng_src_hw_init(struct ath12k_base *ab,
				       struct hal_srng *srng, u32 restore_idx);
void ath12k_wifi8_hal_set_umac_srng_ptr_addr(struct ath12k_base *ab,
					     struct hal_srng *srng,
					     enum hal_ring_type type, int ring_num);
int ath12k_wifi8_hal_srng_update_shadow_config(struct ath12k_base *ab,
					       enum hal_ring_type ring_type,
					       int ring_num);
int ath12k_wifi8_hal_srng_get_ring_id(struct ath12k_hal *hal,
				      enum hal_ring_type type,
				      int ring_num, int mac_id);
u32 ath12k_wifi8_hal_ce_get_desc_size(enum hal_ce_desc type);
void ath12k_wifi8_hal_cc_config(struct ath12k_base *ab);
u8
ath12k_wifi8_hal_get_idle_link_rbm(struct ath12k_hal *hal, u8 device_id);
void ath12k_wifi8_get_tlv_tag_params(__le32 tl, uint16_t *tag, uint32_t *id,
				     uint16_t *length);
void ath12k_wifi8_hal_get_hw_hptp(struct ath12k_base *ab, enum hal_ring_type type,
				  struct hal_srng *srng, uint32_t *hp, uint32_t *tp);
void ath12k_wifi8_hal_ce_src_set_desc(struct hal_ce_srng_src_desc *desc,
				      dma_addr_t paddr,
				      u32 len, u32 id, u8 byte_swap_data);
void ath12k_wifi8_hal_ce_dst_set_desc(struct hal_ce_srng_dest_desc *desc,
				      dma_addr_t paddr);
void
ath12k_wifi8_hal_set_link_desc_addr(struct hal_wbm_link_desc *desc,
				    u32 cookie, dma_addr_t paddr,
				    u8 rbm);
u32
ath12k_wifi8_hal_ce_dst_status_get_length(struct hal_ce_srng_dst_status_desc *desc);
void
ath12k_wifi8_hal_setup_link_idle_list(struct ath12k_base *ab,
				      struct hal_wbm_idle_scatter_list *sbuf,
				      u32 nsbufs, u32 tot_link_desc,
				      u32 end_offset);
void ath12k_wifi8_hal_reoq_lut_addr_read_enable(struct ath12k_base *ab);
void ath12k_wifi8_hal_reoq_lut_set_max_peerid(struct ath12k_base *ab);
void ath12k_wifi8_hal_write_reoq_lut_addr(struct ath12k_base *ab,
					  dma_addr_t paddr);
void ath12k_wifi8_hal_write_ml_reoq_lut_addr(struct ath12k_base *ab,
					     dma_addr_t paddr);
void ath12k_wifi8_hal_reo_init_cmd_ring(struct ath12k_base *ab,
					struct hal_srng *srng);
void ath12k_wifi8_hal_reo_hw_setup(struct ath12k_base *ab);
void
ath12k_wifi8_hal_rx_msdu_link_info_get(struct hal_rx_msdu_link *link,
				       u32 *num_msdus, u32 *msdu_cookies,
				       enum hal_wifi8_rx_buf_return_buf_manager *rbm);
u32 ath12k_wifi8_hal_reo_qdesc_size(u32 ba_window_size, u8 tid);
void ath12k_wifi8_hal_reo_qdesc_setup(struct hal_rx_reo_queue *qdesc,
				      int tid, u32 ba_window_size,
				      u32 start_seq, enum hal_pn_type type,
				      u16 stats_id);
u32 ath12k_hal_srng_get_cmd_size(struct ath12k_base *ab, enum hal_tlv_tag_be type);
void *ath12k_hal_srng_src_get_next_entry_by_cmd_size(struct ath12k_base *ab,
						     struct hal_srng *srng,
						     enum hal_tlv_tag_be type);
void ath12k_wifi8_hal_srng_hw_disable(struct ath12k_base *ab,
				      struct hal_srng *srng);
void ath12k_wifi8_hal_reset_rx_reo_tid_q(void *vaddr,
					 u32 ba_window_size, u8 tid);
void ath12k_wifi8_hal_tx_configure_cmn_reg(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_configure_bank_register_default(struct ath12k_base *ab);
bool ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_get(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_set(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_reset(struct ath12k_base *ab);
bool ath12k_wifi8_hal_tx_ppe2tcl_ring_halt_done(struct ath12k_base *ab);
void ath12k_wifi8_hal_tx_config_rbm_mapping(struct ath12k_base *ab, u8 ring_num,
					    u8 rbm_id, int ring_type);
void ath12k_wifi8_hal_tx_set_ppe_vp_entry(struct ath12k_base *ab,
					  struct ath12k_dp_ppe_vp_profile *ppe_vp_profile,
					  u32 ppe_vp_idx, u32 vdev_id,
					  u32 bank_id, u32 lmac_id);
void ath12k_wifi8_hal_reo_config_reo2ppe_dest_info(struct ath12k_base *ab);

static __always_inline void
ath12k_wifi8_hal_tx_completion_process(struct hal_tqm2sw_completion_ring *desc,
				       struct ath12k_dp_tx_comp_status *tx_comp_status)
{
	u64 desc_va = 0;

	tx_comp_status->buf_rel_source =
		FIELD_GET(HAL_TQM2SW_COMPLETION_RING_INFO0_RELEASE_SOURCE_MODULE,
			  desc->info0);
	tx_comp_status->u.tqm_status =
		       le32_get_bits(desc->info0,
				     HAL_TQM2SW_COMPLETION_RING_INFO0_TQM_RELEASE_REASON);
	tx_comp_status->tx_desc = NULL;

	if (likely(HAL_TQM2SW_COMPLETION_RING_INFO3_COOKIE_CONVERSION_STATUS &
		   desc->info3)) {
		desc_va = ((u64)le32_to_cpu(desc->buf_addr_info.info1) << 32 |
			   le32_to_cpu(desc->buf_addr_info.info0));
		tx_comp_status->tx_desc = (struct ath12k_ppeds_tx_desc_info *)
						((unsigned long)desc_va);
	} else {
		tx_comp_status->desc_id = u32_get_bits(desc->buf_addr_info.info1,
						       BUFFER_ADDR_INFO1_SW_COOKIE);
	}
}

void ath12k_wifi8_hal_hw_ase_init(struct ath12k_base *ab,
				  struct ath12k_hal_ast_param *ast_param);

void ath12k_wifi8_hal_vdev_mcast_ctrl_set(struct ath12k_base *ab, u32 vdev_id,
					  u8 mcast_ctrl_val);
int ath12k_wifi8_hal_get_rdi_source_cfg(struct ath12k_base *ab, int source);
void ath12k_wifi8_hal_txpt_classify_info_flush(struct ath12k_base *ab);
void ath12k_wifi8_hal_ppeds_tx_configure_skip_hdr_fetch(struct ath12k_base *ab);
void ath12k_wifi8_hal_srng_idx_update_addr(struct ath12k_base *ab, struct hal_srng *srng,
		void __iomem *hp_vaddr, dma_addr_t hp_paddr, void __iomem *tp_vaddr,
		dma_addr_t tp_paddr);
void ath12k_wifi8_hal_srng_hw_enable(struct ath12k_base *ab,
		struct hal_srng *srng);
void ath12k_wifi8_hal_ppeds_reo2ppe_cc_config(struct ath12k_base *ab);
bool ath12k_wifi8_hal_ppeds_cfg_ast(struct ath12k_base *ab,
		u32 ppe_vp_num,
		u32 ppeds_idx_map_val);

void ath12k_wifi8_hal_tasc_peer_tx_cfg(struct ath12k_base *ab, bool enable);
void ath12k_wifi8_hal_tasc_peer_clk_cycle_config(struct ath12k_base *ab);
void ath12k_wifi8_hal_tasc_peer_tx_max_peer(struct ath12k_base *ab,
					    u16 ucast, u8 gcast_dl,
					    u8 gcast_ul);
void ath12k_wifi8_hal_tasc_peer_tx_window(struct ath12k_base *ab,
					  u16 time);
void ath12k_wifi8_hal_tasc_peer_tx_fail_drop_default(struct ath12k_base *ab);
void ath12k_wifi8_hal_tasc_peer_tx_fail_drop_map(struct ath12k_base *ab,
						 u32 map1, u32 map2);
void ath12k_wifi8_hal_num_transmission_map_default(struct ath12k_base *ab);
void ath12k_wifi8_hal_num_transmission_map_(struct ath12k_base *ab, u32 map);
void ath12k_wifi8_hal_tasc_peer_tx_gcast_id_map(struct ath12k_base *ab,
						u16 mlo_peer_stats_id,
						u16 gcast_peer_stats_id);
void ath12k_wifi8_hal_tasc_peer_tx_set_id(struct ath12k_base *ab,
					  u16 stats_id);
void ath12k_wifi8_hal_tasc_peer_tx_band(struct ath12k_base *ab,
					u16 band_idx[HAL_TASC_BAND_MAX],
					bool enable);
void ath12k_wifi8_hal_tasc_peer_rx_cfg(struct ath12k_base *ab, bool enable);
void ath12k_wifi8_hal_tasc_peer_rx_max_peer(struct ath12k_base *ab,
					    u16 ucast, u8 gcast);
void ath12k_wifi8_hal_tasc_peer_rx_window(struct ath12k_base *ab,
					  u16 time);
void ath12k_wifi8_hal_tasc_peer_rx_fail_drop_default(struct ath12k_base *ab);
void ath12k_wifi8_hal_tasc_peer_rx_fail_drop_map(struct ath12k_base *ab,
						 u32 map);
void ath12k_wifi8_hal_tasc_peer_rx_set_id(struct ath12k_base *ab,
					  u16 stats_id);
void ath12k_wifi8_hal_tasc_peer_rx_band(struct ath12k_base *ab,
					u16 band_idx[HAL_TASC_BAND_MAX],
					bool enable);
void ath12k_wifi8_hal_tasc_reset_peer_tx(struct ath12k_base *ab, u16 stats_id);
void ath12k_wifi8_hal_tasc_reset_peer_rx(struct ath12k_base *ab, u16 stats_id);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
void ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
						     u32 ppeds_idx_map_val);
#else
static inline
void ath12k_wifi8_hal_ppeds_cfg_ast_override_map_reg(struct ath12k_base *ab, u8 idx,
						     u32 ppeds_idx_map_val)
{
}
#endif
static inline
void *ath12k_hal_srng_src_begin_get_next_entry_nolock_fast(struct hal_srng *srng)
{
	void *desc;
	u32 next_hp;

	/* TODO: Using % is expensive, but we have to do this since size of some
	 * SRNG rings is not power of 2 (due to descriptor sizes). Need to see
	 * if separate function is defined for rings having power of 2 ring size
	 * (TCL2SW, REO2SW, SW2RXDMA and CE rings) so that we can avoid the
	 * overhead of % by using mask (with &).
	 */
	next_hp = (srng->u.src_ring.hp + srng->entry_size) % srng->ring_size;

	if (next_hp == srng->u.src_ring.cached_tp) {
		srng->u.src_ring.cached_tp = *(volatile u32 *)srng->u.src_ring.tp_addr;
		if (next_hp == srng->u.src_ring.cached_tp)
			return NULL;
	}

	desc = srng->ring_base_vaddr + srng->u.src_ring.hp;
	srng->u.src_ring.hp = next_hp;

	/* TODO: Reap functionality is not used by all rings. If particular
	 * ring does not use reap functionality, we need not update reap_hp
	 * with next_hp pointer. Need to make sure a separate function is used
	 * before doing any optimization by removing below code updating
	 * reap_hp.
	 */
	srng->u.src_ring.reap_hp = next_hp;

	return desc;
}

static inline bool ath12k_wifi8_hal_is_reo_nonqos_mgmt_tid(u8 tid)
{
	return (tid == HAL_NON_QOS_TID ||
		tid == HAL_MGMT_BCAST_TID ||
		tid == HAL_MGMT_SENSING_TID);
}

#endif
