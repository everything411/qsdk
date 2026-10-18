// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "nss-switch.h"

/*
 * Hardware Configuration
 * - 24 TX Descriptor Rings (0-23)
 * - 20 TX Completion Rings (0-19)
 * - 20 RX Fill Rings (0-19)
 * - 24 RX Descriptor Rings (0-23)
 * - Mixed stride: 4KB for TX, 256B for RX (memory efficient)
 * - DDRQ, GRO, TSO control, pass-through features
 * - 40-port flow control
 * - 11 interrupt types
 */

/* Hardware configuration constants */
#define TX_DESC_RINGS_COUNT		24
#define TX_CMPL_RINGS_COUNT		20
#define RX_FILL_RINGS_COUNT		20
#define RX_DESC_RINGS_COUNT		24
#define TX_RING_STRIDE			0x1000	/* 4KB */
#define RX_RING_STRIDE			0x100	/* 256B */
#define INTERRUPT_TYPES_COUNT		11
#define PORT_COUNT			8
#define ETH_PORT_START			1
#define ETH_PORT_END			6
#define CPU_PORT			0
#define LOOPBACK_PORT			7

/* Buffer size constants */
#define RX_BUFFER_SIZE			2048
#define TX_BUFFER_SIZE			2048
#define RXFILL_DESC_SIZE		16
#define RXDESC_DESC_SIZE		32
#define TXDESC_DESC_SIZE		32
#define TXDESC_SEC_DESC_SIZE		32
#define TXCMPL_DESC_SIZE		16

/* EDMA configuration constants */
#define EDMA_SW_VERSION			EDMA_SW_VER_3_ID
#define MAX_TXCMPL_RINGS		20
#define MAX_TXDESC_RINGS		24
#define MAX_RXDESC_RINGS		24
#define MAX_RXFILL_RINGS		20

/* PPE buffer management constants */
#define PPE_GROUP_BUF_SIZE		2550
#define PPE_AC_GROUP_TOTAL_BUF		3000

/*
 * CMN PLL Register Definitions
 */
#ifndef CMN_BLK_ADDR
#define CMN_BLK_ADDR			0x9b000
#endif

/* Register offsets from CMN_BLK_ADDR */
#define CMN_PLL_POWER_ON_AND_RESET	0x780  /* CMN_BLK_ADDR + 0x780 */
#define CMN_PLL_REFCLK_CONFIG		0x784  /* CMN_BLK_ADDR + 0x784 */
#define CMN_PLL_LOCKED			0x64   /* CMN_BLK_ADDR + 0x64 (Lock status) */
/* CMN_PLL_REFCLK_CONFIG (0x784) bit definitions */
#define CMN_PLL_REFCLK_INDEX_MASK	0x0F   /* Bits 3:0 */
#define CMN_PLL_REFCLK_INDEX_48MHZ	0x07   /* Index 7 for 48 MHz*/

/* CMN_PLL_POWER_ON_AND_RESET (0x780) bit definitions */
#define CMN_ANA_EN_SW_RSTN		BIT(6) /* Bit 6: Software reset (active high) */
#define CMN_PLL_CLKS_LOCKED		BIT(8) /* Lock status bit */

/* Clock enable bits in CMN_PLL_POWER_ON_AND_RESET */
#define CLK_ENABLE_MASK_PARTIAL		0xBF   /* Enable most clocks (bit 6 = 0) */
#define CLK_ENABLE_MASK_FULL		0xFF   /* Enable all clocks (bit 6 = 1) */
/*
 * Hardware configuration structure
 */
static struct edma_hw_cfg ipq5210_hw_cfg = {
	/* Global register configuration */
	.global = {
		.mas_ctrl = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.port_ctrl = {
			.offset			= 0x04,
			.mask			= 0x221F9,	/* Includes DDRQ fields */
			.shift			= 0,
		},
		.rxdesc2fill_map_0 = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,	/* 7-bit fields */
			.shift			= 0,
		},
		.rxdesc2fill_map_1 = {
			.offset			= 0x18,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.rxdesc2fill_map_2 = {
			.offset			= 0x1C,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.dmar_ctrl = {
			.offset			= 0x48,
			.mask			= 0x1DDF0,	/* Expanded fields */
			.shift			= 0,
		},
		.misc_int_stat = {
			.offset			= 0x5C,
			.mask			= 0x7FF,	/* 11 interrupt types */
			.shift			= 0,
		},
		.misc_int_mask = {
			.offset			= 0x60,
			.mask			= 0x7FF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_0 = {
			.offset			= 0x8C,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_1 = {
			.offset			= 0x90,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_2 = {
			.offset			= 0x94,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.txdesc2cmpl_map_3 = {
			.offset			= 0x98,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
	},

	/* TX Descriptor Ring Configuration - 24 rings */
	.txdesc = {
		.base_offset			= 0x1000,
		.ring_increment			= TX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x18,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x1C,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x0C,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ctrl = {
			.offset			= 0x10,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x10,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0x28,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0x2c,
			.mask			= 0x0,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x30,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x34,
			.mask			= 0x0,
			.shift			= 0,
		},

		/* TXDESC-specific masks - v3 descriptor */
		.tx_en				= 0x1,
		.buf_hi_add_mask		= 0xFF,
		.data_offset_mask		= 0xFFF,
		.data_offset_shift		= 0,
		.data_length_mask		= 0xFFFF,	/* v3: 16-bit (bits 0-15) */
		.data_length_shift		= 0,
	},

	/* TX Completion Ring Configuration - 20 rings */
	.txcmpl = {
		.base_offset			= 0x79000,
		.ring_increment			= TX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x1C,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x04,
			.mask			= 0x0,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x1c,
			.mask			= 0x0,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x0C,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ctrl = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x14,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0x20000,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0x20004,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x2000c,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x34,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},

		/* TXCMPL-specific masks */
		.ring_int_status_mask		= 0x3,
	},

	/* RX Fill Ring Configuration - 20 rings */
	.rxfill = {
		.base_offset			= 0x29000,
		.ring_increment			= RX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x28,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x1c,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x10,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.buf_size = {
			.offset			= 0x10,
			.mask			= 0xFFFF0000,
			.shift			= 16,
		},
		.ctrl = {
			.offset			= 0x10,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x1C,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0xd000,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0xd004,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x30,
			.mask			= 0xffffffff,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x14,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},

		/* RXFILL-specific masks */
		.buf_hi_add_mask		= 0xFF,
		.ring_int_status_mask		= 0x1,
	},

	/* RX Descriptor Ring Configuration - 24 rings */
	.rxdesc = {
		.base_offset			= 0x4A000,
		.ring_increment			= RX_RING_STRIDE,

		.base_addr = {
			.offset			= 0x00,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr_high = {
			.offset			= 0x2C,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.base_addr2 = {
			.offset			= 0x28,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.base_addr2_high = {
			.offset			= 0x30,
			.mask			= 0xFF,
			.shift			= 0,
		},
		.prod_idx = {
			.offset			= 0x04,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.cons_idx = {
			.offset			= 0x08,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.ring_size = {
			.offset			= 0x0c,
			.mask			= 0xFFFF,
			.shift			= 0,
		},
		.pl_offset = {
			.offset			= 0x10,
			.mask			= 0xFF800000,
			.shift			= 23,
		},
		.ctrl = {
			.offset			= 0x18,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},
		.ring_en = {
			.offset			= 0x18,
			.mask			= 0x1,
			.shift			= 0,
		},
		.int_stat = {
			.offset			= 0x18000,
			.mask			= 0x3,
			.shift			= 0,
		},
		.int_mask = {
			.offset			= 0x18004,
			.mask			= 0x0,
			.shift			= 0,
		},
		.int_ctrl = {
			.offset			= 0x1800c,
			.mask			= 0x0,
			.shift			= 0,
		},
		.fc_thre = {
			.offset			= 0x10,
			.mask			= 0xFFFFFFFF,
			.shift			= 0,
		},

		/* RXDESC-specific masks */
		.rx_en				= 0x1,
		.srcinfo_type_mask		= 0xF000,
		.pkt_size_mask			= 0xFFFF,	/* v3: 16-bit field (bits 0-15) */
		.pkt_size_shift			= 0,
		.ring_int_status_mask		= 0x3,
		.portnum_bits			= 0x0FFF,
	},

	/* QID2RID configuration */
	.qid2rid_base_offset		= 0xB9000,
	.qid2rid_increment		= 0x4,

	/* Interrupt control configuration */
	.int_ctrl_base_offset		= 0x62000,
	.int_ctrl_ring_increment	= 0x1000,
	.int_ctrl_reg_offset		= 0xc,

	/* Hardware-specific parameters */
	.ring_dma_mask			= 0xFFFFFFFF,
	.rx_ring_size			= IS_ENABLED(CONFIG_ETH_LOW_MEM) ? 32 : 128,
	.tx_ring_size			= IS_ENABLED(CONFIG_ETH_LOW_MEM) ? 32 : 128,
	.rx_buff_size			= RX_BUFFER_SIZE,
	.tx_buff_size			= TX_BUFFER_SIZE,
	.rxfill_desc_size		= RXFILL_DESC_SIZE,
	.rxdesc_desc_size		= RXDESC_DESC_SIZE,
	.txdesc_desc_size		= TXDESC_DESC_SIZE,
	.txdesc_sec_desc_size		= TXDESC_SEC_DESC_SIZE,
	.txcmpl_desc_size		= TXCMPL_DESC_SIZE,

	/* Interrupt masks */
	.rxfill_int_mask		= 0x1,
	.rxdesc_int_mask		= 0x1,
	.txcmpl_int_mask		= 0x1,
	.misc_intr_mask			= 0x7FF,
	.rx_payload_offset		= 0x0,

	/* Common masks used across multiple rings */
	.tx_int_mask			= 0x3,
	.rx_ne_int_en			= 0x2,
	.tx_ne_int_en			= 0x2,

	/* Destination port configuration */
	.dst_port_type			= 2,
	.dst_port_type_shift		= 28,
	.dst_port_type_mask		= 0xf0000000,
	.dst_port_id_shift		= 16,
	.dst_port_id_mask		= 0x0fff0000,

	/* Register initialization values */
	.port_ctrl_init_val		= 0x6,	/* EDMA v3: PAD_MODE(0x2) | EDMA_EN(0x4) */

	/* DMAR_CTRL configurable values - EDMA v3 expanded limits */
	.dmar_txdata_outstanding_num	= 31,	/* 6-bit field (max 63) */
	.dmar_txdesc_outstanding_num	= 7,	/* 4-bit field (max 15) */
	.dmar_rxfill_outstanding_num	= 7,	/* 4-bit field (max 15) */

	/* DMAR_CTRL bit field configuration - EDMA v3 */
	.dmar_txdata_mask		= 0x3f,		/* 6 bits (0-63) */
	.dmar_txdata_shift		= 4,		/* Bits [9:4] */
	.dmar_txdesc_mask		= 0xf,		/* 4 bits (0-15) */
	.dmar_txdesc_shift		= 10,		/* Bits [13:10] */
	.dmar_rxfill_mask		= 0xf,		/* 4 bits (0-15) */
	.dmar_rxfill_shift		= 14,		/* Bits [17:14] */
};

/*
 * Port Configuration
 * Includes all PHY types from reference designs:
 *
 * - SFP10G_PHY_TYPE: 10G SFP with 10GBASE_R mode
 * - QCA81xx_PHY_TYPE: QCA81xx PHY with USXGMII mode
 * - QCA8081_PHY_TYPE: QCA8081 PHY with SGMII0_RGMII4 mode
 * - QCA8337_SWITCH_TYPE: QCA8337 switch with SGMII0_RGMII4 mode
 * - QCA8033_PHY_TYPE: QCA8033 PHY with SGMII0_RGMII4 mode
 * - QCA8075_PHY_TYPE: QCA8075 PHY with PSGMII mode
 *
 * Each configuration includes:
 * - .id: PHY type identifier
 * - .clk_rate[]: Clock rates for different speeds {10, 100, 1000, 10000, 2500, 5000}
 * - .mac_mode[]: MAC modes for different speeds
 * - .modes[]: Port wrapper modes for different speeds
 */
static struct ipq_eth_port_config ipq5210_port_config[] = {
	{
		SFP10G_PHY_TYPE,
		{
			CLK_312_5_MHZ,			/* 10M */
			CLK_312_5_MHZ,			/* 100M */
			CLK_312_5_MHZ,			/* 1000M */
			CLK_312_5_MHZ,			/* 10000M */
			CLK_312_5_MHZ,			/* 2500M */
			CLK_312_5_MHZ			/* 5000M */
		},
		{
			XGMAC,				/* 10M */
			XGMAC,				/* 100M */
			XGMAC,				/* 1000M */
			XGMAC,				/* 10000M */
			XGMAC,				/* 2500M */
			XGMAC				/* 5000M */
		},
		{
			PORT_WRAPPER_10GBASE_R,		/* 10M */
			PORT_WRAPPER_10GBASE_R,		/* 100M */
			PORT_WRAPPER_10GBASE_R,		/* 1000M */
			PORT_WRAPPER_10GBASE_R,		/* 10000M */
			PORT_WRAPPER_10GBASE_R,		/* 2500M */
			PORT_WRAPPER_10GBASE_R		/* 5000M */
		},
	},
	/* QCA81xx PHY Type */
	{
		QCA81xx_PHY_TYPE,
		{
			-1,				/* 10M - not supported */
			CLK_12_5_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			CLK_312_5_MHZ,			/* 10000M */
			CLK_78_125_MHZ,			/* 2500M */
			CLK_156_25_MHZ			/* 5000M */
		},
		{
			-1,				/* 10M - not supported */
			XGMAC,				/* 100M */
			XGMAC,				/* 1000M */
			XGMAC,				/* 10000M */
			XGMAC,				/* 2500M */
			XGMAC				/* 5000M */
		},
		{
			-1,				/* 10M - not supported */
			PORT_WRAPPER_USXGMII,		/* 100M */
			PORT_WRAPPER_USXGMII,		/* 1000M */
			PORT_WRAPPER_USXGMII,		/* 10000M */
			PORT_WRAPPER_USXGMII,		/* 2500M */
			PORT_WRAPPER_USXGMII		/* 5000M */
		},
	},
	/* QCA8081 PHY Type */
	{
		QCA8081_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			-1,				/* 10000M - not supported */
			CLK_312_5_MHZ			/* 2500M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC,				/* 1000M */
			-1,				/* 10000M - not supported */
			XGMAC				/* 2500M */
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,	/* 10M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 100M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 1000M */
			-1,				/* 10000M - not supported */
			PORT_WRAPPER_SGMII_PLUS		/* 2500M */
		},
	},
	/* QCA8337 Switch Type */
	{
		QCA8337_SWITCH_TYPE,
		{
			CLK_125_MHZ,			/* 10M */
			CLK_125_MHZ,			/* 100M */
			CLK_125_MHZ			/* 1000M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC				/* 1000M */
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,	/* 10M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 100M */
			PORT_WRAPPER_SGMII0_RGMII4	/* 1000M */
		},
	},
	/* QCA8033 PHY Type */
	{
		QCA8033_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			-1,				/* 10000M - not supported */
			-1				/* 2500M - not supported */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC,				/* 1000M */
			-1,				/* 10000M - not supported */
			-1				/* 2500M - not supported */
		},
		{
			PORT_WRAPPER_SGMII0_RGMII4,	/* 10M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 100M */
			PORT_WRAPPER_SGMII0_RGMII4,	/* 1000M */
			-1,				/* 10000M - not supported */
			-1				/* 2500M - not supported */
		},
	},
	/* QCA8075 PHY Type */
	{
		QCA8075_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ			/* 1000M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC				/* 1000M */
		},
		{
			PORT_WRAPPER_PSGMII,		/* 10M */
			PORT_WRAPPER_PSGMII,		/* 100M */
			PORT_WRAPPER_PSGMII		/* 1000M */
		},
	},
	/* QCE1204 PHY Type */
	{
		QCE1204_PHY_TYPE,
		{
			CLK_2_5_MHZ,
			CLK_25_MHZ,
			CLK_125_MHZ,
			-1,
			CLK_312_5_MHZ,
		},
		{
			GMAC,
			GMAC,
			GMAC,
			GMAC,
			GMAC
		},
		{
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
			PORT_WRAPPER_UQXGMII,
		},
	},
	{
		QCE2204_SWITCH_TYPE,
		{
			CLK_312_5_MHZ,		/* 10M  */
			CLK_312_5_MHZ,		/* 100M */
			CLK_312_5_MHZ,		/* 1G   */
			CLK_312_5_MHZ,		/* 10G  */
			CLK_312_5_MHZ,		/* 2.5G */
			CLK_312_5_MHZ		/* 5G   */
		},
		{
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC,
			XGMAC
		},
		{
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R,
			PORT_WRAPPER_10GBASE_R
		},
	},
	/* Internal PHY Type */
	{
		INTERNAL_PHY_TYPE,
		{
			CLK_2_5_MHZ,			/* 10M */
			CLK_25_MHZ,			/* 100M */
			CLK_125_MHZ,			/* 1000M */
			-1,				/* 10000M - not supported */
			CLK_312_5_MHZ			/* 2500M */
		},
		{
			GMAC,				/* 10M */
			GMAC,				/* 100M */
			GMAC,				/* 1000M */
			-1,				/* 10000M - not supported */
			GMAC				/* 2500M */
		},
		{
			PORT_WRAPPER_NA,		/* 10M - NA mode (no uniphy) */
			PORT_WRAPPER_NA,		/* 100M - NA mode (no uniphy) */
			PORT_WRAPPER_NA,		/* 1000M - NA mode (no uniphy) */
			-1,				/* 10000M - not supported */
			PORT_WRAPPER_NA			/* 2500M - NA mode (no uniphy) */
		},
	},
	/* Unused PHY Type - terminator */
	{
		UNUSED_PHY_TYPE,
	},
};

/*
 * VSI configuration array - CONFIG_ETH_MAX_MAC = 6
 */
u32 nb_vsi_config[CONFIG_ETH_MAX_MAC] = {
	0x03, 0x05, 0x09, 0x11, 0x21, 0x41
};

/*
 * PPE Table Address Configuration
 *
 * Example usage for VSI table:
 * Address = IPE_L2_BASE + VSI_TBL_OFFSET + (vsi_id * entry_size)
 * Address = 0x540000 + 0x3800 + (vsi_id * 0x10)
 */
static struct ppe_table_addr_config ipq5210_ppe_table_addrs = {
	.vsi_tbl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x3800,	/* VSI_TBL_OFFSET */
		.increment		= 0x10,		/* VSI_TBL_INC (16 bytes per entry) */
	},
	.vp_port_tbl = {
		.base_addr		= 0x200000,	/* PPE_IPE_L3_BASE_ADDR */
		.offset			= 0x4000,	/* VP_PORT_TBL_OFFSET */
		.increment		= 0x10,		/* VP_PORT_TBL_INC (16 bytes per entry) */
	},
	.port_fc_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x1000,	/* PORT_FC_CFG_OFFSET */
		.increment		= 0x10,		/* PORT_FC_CFG_INC (16 bytes per entry) */
	},
	.mru_mtu_ctrl_tbl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x5000,	/* MRU_MTU_CTRL_TBL_OFFSET */
		.increment		= 0x10,		/* MRU_MTU_CTRL_TBL_INC (16 bytes) */
	},
	.mc_mtu_ctrl_tbl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0xa00,	/* MC_MTU_CTRL_TBL_OFFSET */
		.increment		= 0x4,		/* MC_MTU_CTRL_TBL_INC (4 bytes) */
	},
	.port_bridge_ctrl = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x300,	/* PORT_BRIDGE_CTRL_OFFSET */
		.increment		= 0x4,		/* PORT_BRIDGE_CTRL_INC (4 bytes) */
	},
	.cst_state = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x100,	/* CST_STATE_ADDRESS */
		.increment		= 0x4,		/* CST_STATE_INC (4 bytes per entry) */
	},
	.ipo_action = {
		.base_addr		= 0x540000,	/* IPE_L2_CSR */
		.offset			= 0x8000,	/* IPO_ACTION_OFFSET */
		.increment		= 0x20,		/* IPO_ACTION_INC (32 bytes per entry) */
	},
	.port_eg_vlan_tbl = {
		.base_addr		= 0x600000,	/* NSS_PTX_CSR_BASE_ADDR */
		.offset			= 0x40,		/* PORT_EG_VLAN_TBL_OFFSET */
		.increment		= 0x4,		/* PORT_EG_VLAN_TBL_INC (16 bytes) */
	},
	.eg_bridge_config = {
		.base_addr		= 0x600000,	/* NSS_PTX_CSR_BASE_ADDR */
		.offset			= 0x84,		/* EG_BRIDGE_CONFIG_ADDRESS */
		.increment		= 0x4,		/* Single 32-bit register */
	},
	.ipo_rule_reg = {
		.base_addr		= 0x0b0000,	/* IPO_CSR_BASE_ADDR */
		.offset			= 0x0,		/* IPO_RULE_REG_OFFSET */
		.increment		= 0x10,		/* IPO_RULE_REG_INC (16 bytes per entry) */
	},
	.ipo_mask_reg = {
		.base_addr		= 0x0b0000,	/* IPO_CSR_BASE_ADDR */
		.offset			= 0x2000,	/* IPO_MASK_REG_OFFSET */
		.increment		= 0x10,		/* IPO_MASK_REG_INC (16 bytes per entry) */
	},
	.tl_port_vp_tbl = {
		.base_addr		= 0x200000,	/* PPE_IPE_L3_BASE_ADDR */
		.offset			= 0x2000,	/* TL_PORT_VP_TBL_OFFSET */
		.increment		= 0x10,		/* TL_PORT_VP_TBL_INC (16 bytes) */
	},
	.ac_uni_queue_cfg_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x4e000,	/* AC_UNI_QUEUE_CFG_TBL_OFFSET */
		.increment		= 0x20,		/* AC_UNI_QUEUE_CFG_TBL_INC (32 bytes) */
	},
	.ac_mul_queue_cfg_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x50000,	/* AC_MUL_QUEUE_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* AC_MUL_QUEUE_CFG_TBL_INC (16 bytes) */
	},
	.ac_grp_cfg_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x51000,	/* AC_GRP_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* AC_GRP_CFG_TBL_INC (16 bytes) */
	},
	.ucast_queue_map_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x10000,	/* UCAST_QUEUE_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* UCAST_QUEUE_MAP_TBL_INC (16 bytes) */
	},
	.ucast_priority_map_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x30000,	/* UCAST_PRIORITY_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* UCAST_PRIORITY_MAP_TBL_INC (16 bytes) */
	},
	.l0_flow_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x2000,	/* L0_FLOW_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L0_FLOW_MAP_TBL_INC (16 bytes) */
	},
	.l0_flow_port_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x8000,	/* L0_FLOW_PORT_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L0_FLOW_PORT_MAP_TBL_INC (16 bytes) */
	},
	.l1_flow_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x40000,	/* L1_FLOW_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L1_FLOW_MAP_TBL_INC (16 bytes) */
	},
	.l1_flow_port_map_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x46000,	/* L1_FLOW_PORT_MAP_TBL_OFFSET */
		.increment		= 0x10,		/* L1_FLOW_PORT_MAP_TBL_INC (16 bytes) */
	},
	.psch_tdm_cfg_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x7a000,	/* PSCH_TDM_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* PSCH_TDM_CFG_TBL_INC (16 bytes) */
	},
	.port_bufgrp_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x460,	/* PORT_BUFGRP_CFG_OFFSET */
		.increment		= 0x4,		/* PORT_BUFGRP_CFG_INC (4 bytes) */
	},
	.port_shp_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x240,	/* PORT_SHP_CFG_OFFSET */
		.increment		= 0x4,		/* PORT_SHP_CFG_INC (4 bytes per entry) */
	},
	.port_cnt_cfg = {
		.base_addr		= 0x800000,	/* NSS_BM_CSR_BASE_ADDR */
		.offset			= 0x100,	/* PORT_CNT_CFG_OFFSET */
		.increment		= 0x4,		/* PORT_CNT_CFG_INC (4 bytes per entry) */
	},
	.l0_comp_cfg_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x28000,	/* L0_COMP_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* L0_COMP_CFG_TBL_INC (16 bytes) */
	},
	.l1_comp_cfg_tbl = {
		.base_addr		= 0x400000,	/* PPE_TRAFFIC_MANAGER_BASE_ADDR */
		.offset			= 0x6a000,	/* L1_COMP_CFG_TBL_OFFSET */
		.increment		= 0x10,		/* L1_COMP_CFG_TBL_INC (16 bytes) */
	},
	.ac_mcast_queue_en_tbl = {
		.base_addr		= 0xa00000,	/* PPE_QUEUE_MANAGER_BASE_ADDR */
		.offset			= 0x4a000,	/* AC_MCAST_QUEUE_EN_TBL_OFFSET */
		.increment		= 0x10,		/* AC_MCAST_QUEUE_EN_TBL_INC (16 bytes) */
	},
	.l2_global_conf = {
		.base_addr		= 0x540000,	/* IPE_L2_BASE_ADDR */
		.offset			= 0x38,		/* L2_GLOBAL_CONF_OFFSET */
		.increment		= 0x4,		/* Single 32-bit register */
	},
};

/*
 * Port Configuration
 * 8 ports (0-7):
 * - Port 0: CPU port
 * - Ports 1-6: Ethernet ports
 * - Port 7: Loopback/EIP port
 */
static struct ppe_port_config ipq5210_port_cfg = {
	.port_cpu		= CPU_PORT,
	.port_eth_start		= ETH_PORT_START,
	.port_eth_end		= ETH_PORT_END,
	.port_loopback		= LOOPBACK_PORT,
	.num_ports		= PORT_COUNT,
	.reserved		= 0,
};

/*
 * IPQ5210 port-to-XGMAC-ID mapping.
 *
 * IPQ5210 has only 3 XGMACs shared across 6 ports:
 *   Port 1 -> XGMAC0 (gmacid = 0)
 *   Port 5 -> XGMAC1 (gmacid = 1)
 *   Port 6 -> XGMAC2 (gmacid = 2)
 * All other ports have no XGMAC; returns (u32)-1 for those.
 */
static u32 ipq5210_port_to_gmacid(u32 portid)
{
	switch (portid) {
	case 1: return 0;
	case 5: return 1;
	case 6: return 2;
	default: return (u32)-1;
	}
}

/*
 * EDMA Configuration
 * Architecture with 24 TX rings, 20 RX fill rings
 */
struct edma_config ipq_edma_config = {
	.sw_version		= EDMA_SW_VERSION,
	.txdesc_ring_start	= 0,
	.txdesc_rings		= 1,
	.txdesc_ring_end	= 1,
	.txcmpl_ring_start	= 0,
	.txcmpl_rings		= 1,
	.txcmpl_ring_end	= 1,
	.rxfill_ring_start	= 0,
	.rxfill_rings		= 1,
	.rxfill_ring_end	= 1,
	.rxdesc_ring_start	= 0,
	.rxdesc_rings		= 1,
	.rxdesc_ring_end	= 1,
	.tx_map			= 4,
	.rx_map			= 3,
	.max_txcmpl_rings	= MAX_TXCMPL_RINGS,
	.max_txdesc_rings	= MAX_TXDESC_RINGS,
	.max_rxdesc_rings	= MAX_RXDESC_RINGS,
	.max_rxfill_rings	= MAX_RXFILL_RINGS,
	.iports			= 9,
	.ports			= 6,
	.start_ports		= 1,
	.vsi			= 0x1FF,
	.ipo_action		= 6,
	.tdm_ctrl_val		= 0x80000010,
	.hw_cfg			= &ipq5210_hw_cfg,
	.port_to_gmacid		= ipq5210_port_to_gmacid,
};

/*
 * TDM Configurations
 * Multiple TDM configurations for different board variants
 * Board selects config via device tree tdm_mode property (array index)
 */
static struct ipq_tdm_config ipq5210_tdm_config[] = {
	{
		.val = {
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0),	/* 0x20 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  0, 0, 0),	/* 0x30 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 0, 0),	/* 0x24 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  4, 0, 0),	/* 0x34 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 0, 0),	/* 0x21 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  1, 0, 0),	/* 0x31 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0),	/* 0x25 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  5, 0, 0),	/* 0x35 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 0, 0),	/* 0x22 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  2, 0, 0),	/* 0x32 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 0, 0),	/* 0x26 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  6, 0, 0),	/* 0x36 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 0, 0),	/* 0x23 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  3, 0, 0),	/* 0x33 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 0, 0),	/* 0x27 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS,  7, 0, 0),	/* 0x37 */
		},
		.depth		= 16,
	},
	{
		.val = {
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 2),	/* 0x00 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 2),	/* 0x01 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),	/* 0x02 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),	/* 0x03 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),	/* 0x04 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),	/* 0x05 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 1, 7),	/* 0x06 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 1, 7),	/* 0x07 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 1, 0),	/* 0x08 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 1, 0),	/* 0x09 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 1, 7),	/* 0x0a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 1, 7),	/* 0x0b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),	/* 0x0c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),	/* 0x0d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),	/* 0x0e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),	/* 0x0f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 3),	/* 0x10 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 3),	/* 0x11 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 2),	/* 0x12 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 2),	/* 0x13 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 6),	/* 0x14 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 6),	/* 0x15 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0x16 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0x17 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 0, 0),	/* 0x18 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 0, 0),	/* 0x19 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),	/* 0x1a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),	/* 0x1b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 3),	/* 0x1c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 3),	/* 0x1d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 6),	/* 0x1e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 6),	/* 0x1f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 4),	/* 0x20 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 4),	/* 0x21 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 0),	/* 0x22 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 0),	/* 0x23 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0x24 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0x25 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 0, 0),	/* 0x26 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 0, 0),	/* 0x27 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 4),	/* 0x28 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 4),	/* 0x29 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 5),	/* 0x2a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 5),	/* 0x2b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),	/* 0x2c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),	/* 0x2d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 6),	/* 0x2e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 6),	/* 0x2f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 0),	/* 0x30 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 0),	/* 0x31 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0x32 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0x33 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 0, 0),	/* 0x34 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 0, 0),	/* 0x35 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 2),	/* 0x36 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 2),	/* 0x37 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),	/* 0x38 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),	/* 0x39 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 6),	/* 0x3a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 6),	/* 0x3b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),	/* 0x3c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),	/* 0x3d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 1, 6),	/* 0x3e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 1, 6),	/* 0x3f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 1, 7),	/* 0x40 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 1, 7),	/* 0x41 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 6),	/* 0x42 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 6),	/* 0x43 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 1, 7),	/* 0x44 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 1, 7),	/* 0x45 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),	/* 0x46 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),	/* 0x47 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 2),	/* 0x48 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 2),	/* 0x49 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 4),	/* 0x4a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 4),	/* 0x4b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 3),	/* 0x4c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 3),	/* 0x4d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 6),	/* 0x4e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 6),	/* 0x4f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0x50 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0x51 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 0, 0),	/* 0x52 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 0, 0),	/* 0x53 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 5),	/* 0x54 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 5),	/* 0x55 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 4),	/* 0x56 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 4),	/* 0x57 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),	/* 0x58 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),	/* 0x59 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 6),	/* 0x5a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 6),	/* 0x5b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 0),	/* 0x5c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 0),	/* 0x5d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0x5e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0x5f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),	/* 0x60 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),	/* 0x61 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 5),	/* 0x62 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 5),	/* 0x63 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 3),	/* 0x64 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 3),	/* 0x65 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),	/* 0x66 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),	/* 0x67 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 6),	/* 0x68 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 6),	/* 0x69 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),	/* 0x6a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),	/* 0x6b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),	/* 0x6c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),	/* 0x6d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0x6e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0x6f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 6),	/* 0x70 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 6),	/* 0x71 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 2),	/* 0x72 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 2),	/* 0x73 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 6),	/* 0x74 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 6),	/* 0x75 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 1, 7),	/* 0x76 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 1, 7),	/* 0x77 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 1, 0),	/* 0x78 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 1, 0),	/* 0x79 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 1, 7),	/* 0x7a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 1, 7),	/* 0x7b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 0),	/* 0x7c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 0),	/* 0x7d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 0, 0),	/* 0x7e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 0, 0),	/* 0x7f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 5),	/* 0x80 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 5),	/* 0x81 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 3),	/* 0x82 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 3),	/* 0x83 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),	/* 0x84 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),	/* 0x85 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 4),	/* 0x86 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 4),	/* 0x87 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 6),	/* 0x88 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 6),	/* 0x89 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),	/* 0x8a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),	/* 0x8b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 5),	/* 0x8c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 5),	/* 0x8d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),	/* 0x8e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),	/* 0x8f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 5),	/* 0x90 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 5),	/* 0x91 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 6),	/* 0x92 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 6),	/* 0x93 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),	/* 0x94 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),	/* 0x95 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 6),	/* 0x96 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 6),	/* 0x97 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),	/* 0x98 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),	/* 0x99 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 5),	/* 0x9a */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 5),	/* 0x9b */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 1),	/* 0x9c */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 1),	/* 0x9d */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 5),	/* 0x9e */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 5),	/* 0x9f */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 6),	/* 0xa0 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 6),	/* 0xa1 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 5),	/* 0xa2 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 5),	/* 0xa3 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 1, 7),	/* 0xa4 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 1, 7),	/* 0xa5 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 6, 1, 0),	/* 0xa6 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 6, 1, 0),	/* 0xa7 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 1, 7),	/* 0xa8 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 1, 7),	/* 0xa9 */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 1, 1),	/* 0xaa */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 1, 1),	/* 0xab */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 7, 1, 5),	/* 0xac */
			TDM(A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 7, 1, 5),	/* 0xad */
		},
		.depth		= 174,
	},

	/* Add more configurations here as needed for different board variants */
};

/*
 * Scheduler configuration table
 * Indexed by ppe->tdm_mode (from DTS):
 *   [0] = ppe_port_scheduler0_tbl (default, tm_tick_mode=0)
 *   [1] = ppe_port_scheduler1_tbl (10G ports on 1/5/6, tm_tick_mode=1)
 */
static struct ipq_sch_config ipq5210_ppe_port_scheduler0_tbl[] = {
	{
		.val = {
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /*  0 */
			PSCH(0x0, 0xf, 0x6, 0x0, 0x0),   /*  1 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /*  2 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /*  3 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /*  4 */
			PSCH(0x0, 0x6, 0x7, 0x0, 0x0),   /*  5 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /*  6 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /*  7 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /*  8 */
			PSCH(0x0, 0x6, 0x2, 0x0, 0x0),   /*  9 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 10 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 11 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 12 */
			PSCH(0x0, 0x2, 0x5, 0x0, 0x0),   /* 13 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 14 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /* 15 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 16 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 17 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 18 */
			PSCH(0x0, 0x6, 0x3, 0x0, 0x0),   /* 19 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 20 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 21 */
			PSCH(0x0, 0xf, 0x0, 0x0, 0x0),   /* 22 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 23 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 24 */
			PSCH(0x0, 0x3, 0x6, 0x0, 0x0),   /* 25 */
			PSCH(0x0, 0xf, 0x0, 0x0, 0x0),   /* 26 */
			PSCH(0x0, 0x6, 0x7, 0x0, 0x0),   /* 27 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 28 */
			PSCH(0x0, 0x6, 0x5, 0x0, 0x0),   /* 29 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 30 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 31 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 32 */
			PSCH(0x0, 0x5, 0x0, 0x0, 0x0),   /* 33 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 34 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 35 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 36 */
			PSCH(0x0, 0x5, 0x4, 0x0, 0x0),   /* 37 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 38 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 39 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 40 */
			PSCH(0x0, 0x4, 0x5, 0x0, 0x0),   /* 41 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 42 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 43 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 44 */
			PSCH(0x0, 0x2, 0x1, 0x0, 0x0),   /* 45 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 46 */
			PSCH(0x0, 0x6, 0x5, 0x0, 0x0),   /* 47 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 48 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 49 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 50 */
			PSCH(0x0, 0x3, 0x0, 0x0, 0x0),   /* 51 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 52 */
			PSCH(0x0, 0x6, 0x5, 0x0, 0x0),   /* 53 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 54 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 55 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 56 */
			PSCH(0x0, 0x5, 0x0, 0x0, 0x0),   /* 57 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 58 */
			PSCH(0x0, 0x6, 0x2, 0x0, 0x0),   /* 59 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 60 */
			PSCH(0x0, 0x5, 0x3, 0x0, 0x0),   /* 61 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 62 */
			PSCH(0x0, 0x6, 0x4, 0x0, 0x0),   /* 63 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 64 */
			PSCH(0x0, 0x4, 0x5, 0x0, 0x0),   /* 65 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 66 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /* 67 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 68 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 69 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 70 */
			PSCH(0x0, 0x5, 0x2, 0x0, 0x0),   /* 71 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 72 */
			PSCH(0x0, 0x6, 0x3, 0x0, 0x0),   /* 73 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 74 */
			PSCH(0x0, 0x3, 0x4, 0x0, 0x0),   /* 75 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 76 */
			PSCH(0x0, 0x2, 0x5, 0x0, 0x0),   /* 77 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 78 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 79 */
			PSCH(0x0, 0x5, 0x7, 0x0, 0x0),   /* 80 */
			PSCH(0x0, 0x5, 0x7, 0x0, 0x0),   /* 81 */
			PSCH(0x0, 0x6, 0x7, 0x0, 0x0),   /* 82 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 83 */
			PSCH(0x0, 0x1, 0x7, 0x0, 0x0),   /* 84 */
			PSCH(0x0, 0x4, 0x6, 0x0, 0x0),   /* 85 */
		},
		.depth		= 86,
	},
	{
		.val = {
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /*  0 */
			PSCH(0x0, 0xf, 0x6, 0x0, 0x0),   /*  1 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /*  2 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /*  3 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /*  4 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /*  5 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /*  6 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /*  7 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /*  8 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /*  9 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 10 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 11 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 12 */
			PSCH(0x0, 0x2, 0x5, 0x0, 0x0),   /* 13 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 14 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /* 15 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 16 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 17 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 18 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /* 19 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 20 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 21 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 22 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 23 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 24 */
			PSCH(0x0, 0x3, 0x6, 0x0, 0x0),   /* 25 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 26 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /* 27 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 28 */
			PSCH(0x0, 0x6, 0x5, 0x0, 0x0),   /* 29 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 30 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 31 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 32 */
			PSCH(0x0, 0x5, 0x1, 0x0, 0x0),   /* 33 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 34 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 35 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 36 */
			PSCH(0x0, 0x5, 0x1, 0x0, 0x0),   /* 37 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 38 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 39 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 40 */
			PSCH(0x0, 0x4, 0x5, 0x0, 0x0),   /* 41 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 42 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 43 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 44 */
			PSCH(0x0, 0x2, 0x1, 0x0, 0x0),   /* 45 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 46 */
			PSCH(0x0, 0x6, 0x5, 0x0, 0x0),   /* 47 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 48 */
			PSCH(0x0, 0x5, 0x6, 0x0, 0x0),   /* 49 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 50 */
			PSCH(0x0, 0x3, 0x1, 0x0, 0x0),   /* 51 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 52 */
			PSCH(0x0, 0x6, 0x5, 0x0, 0x0),   /* 53 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 54 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 55 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 56 */
			PSCH(0x0, 0x5, 0x1, 0x0, 0x0),   /* 57 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 58 */
			PSCH(0x0, 0x6, 0x0, 0x0, 0x0),   /* 59 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 60 */
			PSCH(0x0, 0x5, 0x7, 0x0, 0x0),   /* 61 */
			PSCH(0x0, 0xf, 0x0, 0x0, 0x0),   /* 62 */
			PSCH(0x0, 0x6, 0x4, 0x0, 0x0),   /* 63 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 64 */
			PSCH(0x0, 0x4, 0x5, 0x0, 0x0),   /* 65 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 66 */
			PSCH(0x0, 0x6, 0x1, 0x0, 0x0),   /* 67 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 68 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 69 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 70 */
			PSCH(0x0, 0x5, 0x0, 0x0, 0x0),   /* 71 */
			PSCH(0x0, 0xf, 0x7, 0x0, 0x0),   /* 72 */
			PSCH(0x0, 0x6, 0x7, 0x0, 0x0),   /* 73 */
			PSCH(0x0, 0xf, 0x0, 0x0, 0x0),   /* 74 */
			PSCH(0x0, 0x3, 0x4, 0x0, 0x0),   /* 75 */
			PSCH(0x0, 0x0, 0x7, 0x0, 0x0),   /* 76 */
			PSCH(0x0, 0x2, 0x5, 0x0, 0x0),   /* 77 */
			PSCH(0x0, 0x7, 0x0, 0x0, 0x0),   /* 78 */
			PSCH(0x0, 0x1, 0x6, 0x0, 0x0),   /* 79 */
			PSCH(0x0, 0x5, 0x7, 0x0, 0x0),   /* 80 */
			PSCH(0x0, 0x5, 0x1, 0x0, 0x0),   /* 81 */
			PSCH(0x0, 0x6, 0x7, 0x0, 0x0),   /* 82 */
			PSCH(0x0, 0x1, 0x4, 0x0, 0x0),   /* 83 */
			PSCH(0x0, 0x1, 0x5, 0x0, 0x0),   /* 84 */
			PSCH(0x0, 0x4, 0x6, 0x0, 0x0),   /* 85 */
		},
		.depth		= 86,
	},
};

/*
 * TDM Address Configuration
 * Shared across all TDM configurations - same for all boards
 */
static struct ipq_tdm_addr_config ipq5210_tdm_addr_config = {
	.tdm_addr = {
		.base_addr		= 0x0,		/* Relative to PPE base */
		.offset			= 0xc000,	/* TDM offset from PPE base */
		.increment		= 0x4,		/* 4 bytes per TDM entry */
	},
	.tdm_ctrl_offset		= 0xb000,	/* TDM control register offset */
};

/* Global exports required by nss-switch.c */
struct ipq_eth_port_config	*port_config		= ipq5210_port_config;
struct ppe_table_addr_config	*ppe_table_addrs	= &ipq5210_ppe_table_addrs;
struct ppe_port_config		*ppe_port_cfg		= &ipq5210_port_cfg;

/* Export pointers - nss-switch.c will index tdm_config based on tdm_mode from DTS */
struct ipq_tdm_config		*tdm_config		= ipq5210_tdm_config;
struct ipq_tdm_addr_config	*tdm_addr_config	= &ipq5210_tdm_addr_config;
struct ipq_sch_config		*sch_config		= ipq5210_ppe_port_scheduler0_tbl;

static struct ipq_eth_sku ipq5210_uniphy[CONFIG_ETH_MAX_UNIPHY] = {
	{
		.reg	= 0xA4030,
		.bit	= 15,
	},
	{
		.reg	= 0xA4030,
		.bit	= 16,
	},
	{
		.reg	= 0xA4030,
		.bit	= 17,
	},
};

struct ipq_eth_sku *ipq_uniphy = ipq5210_uniphy;

/*
 * Common clock configuration function - dummy implementation
 */
void ipq_config_cmn_clock(void)
{
	unsigned int reg_val;

	/* Skip initialization if CMN PLL clocks are already fully enabled and locked */
	reg_val = readl(CMN_BLK_ADDR + CMN_PLL_POWER_ON_AND_RESET);
	if (reg_val == CLK_ENABLE_MASK_FULL) {
		reg_val = readl(CMN_BLK_ADDR + CMN_PLL_LOCKED);
		if (reg_val & CMN_PLL_CLKS_LOCKED)
			return;
	}

	/*
	 * Step 1: Configure reference clock to 48 MHz
	 * uses ref_48mhz_clk (derived from xo_clk) as reference (from DTS)
	 * This is identical to IPQ9574
	 */
	reg_val = readl(CMN_BLK_ADDR + CMN_PLL_REFCLK_CONFIG);
	reg_val = (reg_val & ~CMN_PLL_REFCLK_INDEX_MASK) |
			CMN_PLL_REFCLK_INDEX_48MHZ;
	writel(reg_val, CMN_BLK_ADDR + CMN_PLL_REFCLK_CONFIG);

	/*
	 * Step 2: Assert reset (set bit 6)
	 * Setting bit 6 actually asserts reset
	 */
	reg_val = readl(CMN_BLK_ADDR + CMN_PLL_POWER_ON_AND_RESET);
	reg_val = reg_val | CMN_ANA_EN_SW_RSTN;
	writel(reg_val, CMN_BLK_ADDR + CMN_PLL_POWER_ON_AND_RESET);
	mdelay(1);

	/*
	 * Step 3: Deassert reset (clear bit 6)
	 * Clearing bit 6 releases the PLL from reset
	 */
	reg_val = reg_val & (~CMN_ANA_EN_SW_RSTN);
	writel(reg_val, CMN_BLK_ADDR + CMN_PLL_POWER_ON_AND_RESET);
	mdelay(1);

	/*
	 * Step 4: Enable clocks progressively
	 * First enable most clocks (0xbf = bit 6 clear)
	 */
	writel(CLK_ENABLE_MASK_PARTIAL,
		CMN_BLK_ADDR + CMN_PLL_POWER_ON_AND_RESET);
	mdelay(1);

	/*
	 * Step 5: Enable all clocks (0xff = all bits set)
	 */
	writel(CLK_ENABLE_MASK_FULL, CMN_BLK_ADDR + CMN_PLL_POWER_ON_AND_RESET);
	mdelay(1);

	/*
	 * Wait for PLL lock
	 */
	{
		int timeout = 1000;
		while (timeout-- > 0) {
			reg_val = readl(CMN_BLK_ADDR + CMN_PLL_LOCKED);
			if (reg_val & CMN_PLL_CLKS_LOCKED) {
				break;
			}
			udelay(10);
		}

		if (timeout <= 0) {
			printf("Warning: CMN PLL lock timeout\n");
		}
	}
}

/**
 * ipq_get_group_buf() - Get the buffer group size for the current chip
 *
 * Returns the configured buffer group size value for PPE buffer management.
 * This value is chip-specific:
 *
 * Return: Buffer group size value
 */
u16 ipq_get_group_buf(void)
{
	return PPE_GROUP_BUF_SIZE;
}

/**
 * ipq_get_ac_group_total_buf() - Get the AC group total buffer size
 *
 * Returns the configured total buffer value for AC group configuration.
 * This value is chip-specific:
 *
 * Return: Total buffer value for AC group
 */
u16 ipq_get_ac_group_total_buf(void)
{
	return PPE_AC_GROUP_TOTAL_BUF;
}

enum csr_version uniphy_get_csr_version(void)
{
	return CSR_VERSION_V1;
}

enum reset_version uniphy_get_reset_version(void)
{
	return RESET_VERSION_V2;
}
