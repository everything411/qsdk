// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm IPQ5210
 *
 * (C) Copyright 2024 Linaro Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/types.h>
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dm/device-internal.h>
#include <asm/arch/dt-bindings/clock/qcom,ipq5210-gcc.h>
#include <asm/arch/dt-bindings/reset/qcom,ipq5210-gcc.h>
#include "clock-qcom.h"

#define	GCC_QUPV3_UART1_CMD_RCGR		0x5004
#define	GCC_QUPV3_UART2_CMD_RCGR		0x3018
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_QUPV3_SPI0_CMD_RCGR			0x3018

#define GCC_USB0_MOCK_UTMI_DIV_CDIVR		(0x2C040)
#define GCC_USB0_MOCK_UTMI_CMD_RCGR		(0x2C02C)
#define GCC_USB0_AUX_CMD_RCGR			(0x2C018)
#define GCC_USB0_MASTER_CMD_RCGR		(0x2C004)
#define GCC_QPIC_CMD_RCGR			(0x32020)
#define GCC_QPIC_CBCR				(0x32028)
#define GCC_QPIC_IO_MACRO_CMD_RCGR		(0x32004)
#define GCC_QPIC_IO_MACRO_CBCR			(0x3200C)
#define GCC_QPIC_AHB_CBCR			(0x32010)

#define GCC_USB0_MOCK_UTMI_CBCR			0x2C050
#define GCC_USB0_MASTER_CBCR			0x2C044
#define GCC_USB0_AUX_CBCR			0x2C04C
#define GCC_USB0_PIPE_CBCR			0x2C054
#define GCC_USB0_SLEEP_CBCR			0x2C058
#define GCC_USB0_PHY_CFG_AHB_CBCR		0x2C05C

#define CFG_CLK_SRC_GPLL4_OUT_AUX		(1 << 8)

#define IO_MACRO_CLK_400_MHZ				(400000000)
#define IO_MACRO_CLK_320_MHZ				(320000000)
#define IO_MACRO_CLK_266_MHZ				(266000000)
#define IO_MACRO_CLK_228_MHZ				(228000000)
#define IO_MACRO_CLK_200_MHZ				(200000000)
#define IO_MACRO_CLK_100_MHZ				(100000000)
#define IO_MACRO_CLK_50_MHZ				(50000000)
#define IO_MACRO_CLK_24_MHZ				(24000000)

#define GCC_QUPV3_I2C0_CMD_RCGR			(0x02018)
#define GCC_QUPV3_I2C1_CMD_RCGR			(0x02034)

#define GCC_PCNOC_BFDCD_CMD_RCGR		(0x31004)
#define GCC_SYSTEM_NOC_BFDCD_CMD_RCGR		(0x2E004)
#define GCC_QUPV3_AHB_MST_CBCR			(0x01014)
#define GCC_QUPV3_AHB_SLV_CBCR			(0x0102C)
#define GCC_QUPV3_2X_CORE_CMD_RCGR		(0x0100C)
#define GCC_QUPV3_2X_CORE_CBCR			(0x01020)
#define GCC_QUPV3_CORE_CBCR			(0x01018)
#define GCC_QUPV3_SLEEP_CBCR			(0x01028)
#define PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN		(2 << 8)

/* PCIE clock control registers */
#define GCC_PCIE_AUX_CMD_RCGR				0x28004
#define GCC_PCIE0_AXI_M_CMD_RCGR 			0x28018
#define GCC_PCIE1_AXI_M_CMD_RCGR 			0x29018
#define GCC_PCIE0_AXI_S_CMD_RCGR			0x28020
#define GCC_PCIE1_AXI_S_CMD_RCGR			0x29020
#define GCC_PCIE0_RCHNG_CMD_RCGR			0x28028
#define GCC_PCIE1_RCHNG_CMD_RCGR			0x29028
#define PCIE_GPLL0_OUT_AUX				(2 << 8)
#define PCIE_GPLL4_OUT_MAIN				(2 << 8)
#define PCIE_GPLL0_OUT_MAIN				(1 << 8)

/* Clock rate constants */
#define CLK_1_25_MHZ				(1250000UL)
#define CLK_2_5_MHZ				(2500000UL)
#define CLK_12_5_MHZ				(12500000UL)
#define CLK_24_MHZ				(24000000UL)
#define CLK_25_MHZ				(25000000UL)
#define CLK_78_125_MHZ				(78125000UL)
#define CLK_50_MHZ				(50000000UL)
#define CLK_125_MHZ				(125000000UL)
#define CLK_156_25_MHZ				(156250000UL)
#define CLK_312_5_MHZ				(312500000UL)

/* Core NSS Clocks */
#define NSS_CC_NSS_CSR_CBCR			(0x00714)
#define NSS_CC_NSSNOC_NSS_CSR_CBCR		(0x00718)

#define NSS_CC_PPE_SWITCH_IPE_CBCR		(0x00424)
#define NSS_CC_PPE_SWITCH_BTQ_CBCR		(0x0042C)
#define NSS_CC_PPE_SWITCH_CBCR			(0x00434)
#define NSS_CC_PPE_SWITCH_CFG_CBCR		(0x0043C)
#define NSS_CC_PPE_EDMA_CBCR			(0x00440)
#define NSS_CC_PPE_EDMA_CFG_CBCR		(0x00448)

/* NOC PPE Clocks */
#define NSS_CC_NSSNOC_PPE_CBCR			(0x004A4)
#define NSS_CC_NSSNOC_PPE_CFG_CBCR		(0x004A8)

#define NSS_CC_PORT1_MAC_CBCR			(0x0044C)
#define NSS_CC_PORT2_MAC_CBCR			(0x00454)
#define NSS_CC_PORT3_MAC_CBCR			(0x0045C)
#define NSS_CC_PORT4_MAC_CBCR			(0x00464)
#define NSS_CC_PORT5_MAC_CBCR			(0x0046C)
#define NSS_CC_PORT6_MAC_CBCR			(0x00474)

#define NSS_CC_PON_CBCR				(0x0047C)

#define NSS_CC_PORT1_RX_CBCR			(0x00548)
#define NSS_CC_PORT1_TX_CBCR			(0x00550)
#define NSS_CC_PORT2_RX_CBCR			(0x00558)
#define NSS_CC_PORT2_TX_CBCR			(0x00560)
#define NSS_CC_PORT3_RX_CBCR			(0x00568)
#define NSS_CC_PORT3_TX_CBCR			(0x00570)
#define NSS_CC_PORT4_RX_CBCR			(0x00578)
#define NSS_CC_PORT4_TX_CBCR			(0x00580)
#define NSS_CC_PORT5_RX_CBCR			(0x00588)
#define NSS_CC_PORT5_TX_CBCR			(0x00590)
#define NSS_CC_PORT6_RX_CBCR			(0x00598)
#define NSS_CC_PORT6_TX_CBCR			(0x005A0)

/* PTP Reference Clocks */
#define NSS_CC_XGMAC0_PTP_REF_CBCR		(0x00488)
#define NSS_CC_XGMAC1_PTP_REF_CBCR		(0x0048C)
#define NSS_CC_XGMAC2_PTP_REF_CBCR		(0x00490)

/* Debug Clock */
#define NSS_CC_DEBUG_CBCR			(0x00750)

/* UNIPHY Port Clocks */
#define NSS_CC_UNIPHY_PORT1_RX_CBCR		(0x005E0)
#define NSS_CC_UNIPHY_PORT1_TX_CBCR		(0x005E4)
#define NSS_CC_UNIPHY_PORT2_RX_CBCR		(0x005E8)
#define NSS_CC_UNIPHY_PORT2_TX_CBCR		(0x005EC)
#define NSS_CC_UNIPHY_PORT3_RX_CBCR		(0x005F0)
#define NSS_CC_UNIPHY_PORT3_TX_CBCR		(0x005F4)
#define NSS_CC_UNIPHY_PORT4_RX_CBCR		(0x005F8)
#define NSS_CC_UNIPHY_PORT4_TX_CBCR		(0x005FC)
#define NSS_CC_UNIPHY_PORT5_RX_CBCR		(0x00600)
#define NSS_CC_UNIPHY_PORT5_TX_CBCR		(0x00604)
#define NSS_CC_UNIPHY_PORT6_RX_CBCR		(0x00608)
#define NSS_CC_UNIPHY_PORT6_TX_CBCR		(0x0060C)

/* GCC Clock Registers */
#define GCC_IM_SLEEP_CBCR			(0x34020)
#define GCC_CMN_AHB_CBCR			(0x3A004)
#define GCC_CMN_SYS_CBCR			(0x3A008)
#define GCC_NSSCC_CBCR				(0x17034)
#define GCC_NSSNOC_NSSCC_CBCR			(0x17030)
#define GCC_NSSNOC_SNOC_CBCR			(0x17028)
#define GCC_NSSNOC_SNOC_1_CBCR			(0x1707C)
#define GCC_UNIPHY0_AHB_CBCR			(0x1704C)
#define GCC_UNIPHY1_AHB_CBCR			(0x1705C)
#define GCC_UNIPHY2_AHB_CBCR			(0x1706C)
#define GCC_UNIPHY0_SYS_CBCR			(0x17048)
#define GCC_UNIPHY1_SYS_CBCR			(0x17058)
#define GCC_UNIPHY2_SYS_CBCR			(0x17068)

#define GCC_APSS_DBG_CBCR			(0x2402C)
#define GCC_APSS_TS_CBCR			(0x24030)
#define GCC_SNOC_QOSGEN_EXTREF_CBCR		(0x2E020)
#define GCC_SYS_NOC_AT_CBCR			(0x2E038)
#define GCC_SNOC_XO_DCD_CBCR			(0x2E060)
#define GCC_SNOC_TS_CBCR			(0x2E068)
#define GCC_PCNOC_AT_CBCR			(0x31024)
#define GCC_PCNOC_TS_CBCR			(0x3102C)
#define GCC_CNOC_QOSGEN_EXTREF_CBCR		(0x310B0)

#define GCC_APSS_AXI_CMD_RCGR			(0x24004)
#define GCC_APSS_AHB_CMD_RCGR			(0x2400C)
#define GCC_QDSS_TSCTR_CMD_RCGR			(0x2D01C)

#define GCC_QDSS_AT_CMD_RCGR			(0x2D004)
#define APCS_ALIAS0_CMD_RCGR			(0x00050)
#define GCC_NSSNOC_SNOC_CMD_RCGR		(0x2E008)
#define GCC_UNIPHY_SYS_CMD_RCGR			(0x17094)
/* Ethernet related clocks */
#define GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR	(0x17004)
#define NSS_CC_EIP_BFDCD_CMD_RCGR		(0x006A8)
#define NSS_CC_PPE_CMD_RCGR			(0x003EC)
#define NSS_CC_PPE_CFG_RCGR			(0x003F0)
#define NSS_CC_CFG_CMD_RCGR			(0x0070C)
#define NSS_CC_CFG_CFG_RCGR			(0x00710)

#define NSS_CC_PORT1_RX_CMD_RCGR		(0x004B4)
#define NSS_CC_PORT1_RX_CFG_RCGR		(0x004B8)
#define NSS_CC_PORT1_TX_CMD_RCGR		(0x004C0)
#define NSS_CC_PORT1_TX_CFG_RCGR		(0x004C4)
#define NSS_CC_PORT2_RX_CMD_RCGR		(0x004CC)
#define NSS_CC_PORT2_RX_CFG_RCGR		(0x004D0)
#define NSS_CC_PORT2_TX_CMD_RCGR		(0x004D8)
#define NSS_CC_PORT2_TX_CFG_RCGR		(0x004DC)
#define NSS_CC_PORT3_RX_CMD_RCGR		(0x004E4)
#define NSS_CC_PORT3_RX_CFG_RCGR		(0x004E8)
#define NSS_CC_PORT3_TX_CMD_RCGR		(0x004F0)
#define NSS_CC_PORT3_TX_CFG_RCGR		(0x004F4)
#define NSS_CC_PORT4_RX_CMD_RCGR		(0x004FC)
#define NSS_CC_PORT4_RX_CFG_RCGR		(0x00500)
#define NSS_CC_PORT4_TX_CMD_RCGR		(0x00508)
#define NSS_CC_PORT4_TX_CFG_RCGR		(0x0050C)
#define NSS_CC_PORT5_RX_CMD_RCGR		(0x00514)
#define NSS_CC_PORT5_RX_CFG_RCGR		(0x00518)
#define NSS_CC_PORT5_TX_CMD_RCGR		(0x00520)
#define NSS_CC_PORT5_TX_CFG_RCGR		(0x00524)
#define NSS_CC_PORT6_RX_CMD_RCGR		(0x0052C)
#define NSS_CC_PORT6_RX_CFG_RCGR		(0x00530)
#define NSS_CC_PORT6_TX_CMD_RCGR		(0x00538)
#define NSS_CC_PORT6_TX_CFG_RCGR		(0x0053C)

/* UNIPHY Port DIV4 registers for XGMII2GMII bridge */
#define NSS_CC_UNIPHY_PORT1_RX_DIV4_DIV_CDIVR	(0x00624)
#define NSS_CC_UNIPHY_PORT1_TX_DIV4_DIV_CDIVR	(0x00628)
#define NSS_CC_UNIPHY_PORT2_RX_DIV4_DIV_CDIVR	(0x0062C)
#define NSS_CC_UNIPHY_PORT2_TX_DIV4_DIV_CDIVR	(0x00630)
#define NSS_CC_UNIPHY_PORT3_RX_DIV4_DIV_CDIVR	(0x00634)
#define NSS_CC_UNIPHY_PORT3_TX_DIV4_DIV_CDIVR	(0x00638)
#define NSS_CC_UNIPHY_PORT4_RX_DIV4_DIV_CDIVR	(0x0063C)
#define NSS_CC_UNIPHY_PORT4_TX_DIV4_DIV_CDIVR	(0x00640)

/* UNIPHY Port DIV4 Clock Branch Control Registers */
#define NSS_CC_UNIPHY_PORT1_RX_DIV4_CBCR	(0x00644)
#define NSS_CC_UNIPHY_PORT1_TX_DIV4_CBCR	(0x00648)
#define NSS_CC_UNIPHY_PORT2_RX_DIV4_CBCR	(0x0064C)
#define NSS_CC_UNIPHY_PORT2_TX_DIV4_CBCR	(0x00650)
#define NSS_CC_UNIPHY_PORT3_RX_DIV4_CBCR	(0x00654)
#define NSS_CC_UNIPHY_PORT3_TX_DIV4_CBCR	(0x00658)
#define NSS_CC_UNIPHY_PORT4_RX_DIV4_CBCR	(0x0065C)
#define NSS_CC_UNIPHY_PORT4_TX_DIV4_CBCR	(0x00660)

#define GCC_NSSNOC_MEMNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN	BIT(8)
#define GCC_QDSS_AT_SRC_SEL_GPLL4_OUT_MAIN		BIT(8)
#define GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN	(2 << 8)

#define GCC_QUPV3_2X_CORE_SRC_SEL_GPLL0_OUT_MAIN	BIT(8)
#define GCC_APSS_AXI_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_APSS_AHB_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define APCS_ALIAS0_SRC_SEL_APCPLL_OUT_EARLY		(5 << 8)
#define GCC_QDSS_TSCTR_SRC_SEL_GPLL4_OUT_MAIN		BIT(8)

/* Clock source selections */
#define NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_375M		(6 << 8)
#define NSS_CC_PPE_SRC_SEL_GCC_GPLL0_OUT_AUX		(2 << 8)
#define NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK	(3 << 8)
#define NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK	(4 << 8)
#define NSS_CC_PORT4_SRC_SEL_UNIPHY1_NSS_TX_CLK		(2 << 8)
#define NSS_CC_PORT5_SRC_SEL_UNIPHY0_NSS_TX_CLK		(2 << 8)
#define NSS_CC_PORT4_RX_SRC_SEL_RX_GCC			(1 << 8)
#define NSS_CC_PORT4_TX_SRC_SEL_TX_GCC			(5 << 8)
#define NSS_CC_PORT5_RX_SRC_SEL_RX_GCC			(1 << 8)
#define NSS_CC_PORT5_TX_SRC_SEL_TX_GCC			(5 << 8)
#define CMN_PLL_NSS_CLK_429M				(6 << 8)
#define PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN		(2 << 8)
#define NSSNOC_MEMNOC_BFDCD_SRC_SEL_NSS_CMN_CLK		BIT(8)

/* Reset Control Registers */
#define NSS_CC_PPE_BCR_REG			(0x003E8)
#define GCC_UNIPHY0_BCR_REG			(0x17044)
#define GCC_UNIPHY1_BCR_REG			(0x17054)
#define GCC_UNIPHY2_BCR_REG			(0x17064)

int msm_set_parent(struct clk *clk, struct clk *parent)
{
	if (!clk || !parent || !clk->dev || !parent->dev)
		return 0;

	clk->dev->parent = parent->dev;
	dev_set_uclass_priv(parent->dev, parent);

	return 0;
}

ulong msm_get_rate(struct clk *clk)
{
	switch (clk->id) {
	case GCC_USB0_MOCK_UTMI_CLK:
		/* USB MOCK_UTMI clocks are configured to 24MHz from CXO */
		clk->rate = CLK_24_MHZ;
		break;
	}

	return (ulong)clk->rate;
}

static int calc_div_for_nss_port_clk(struct clk *clk, ulong rate,
				     int *div, int *cdiv, int *div4)
{
	int pclk_rate = clk_get_parent_rate(clk);

	/* Default DIV4 value is 0 (divide by 1, since register value N divides by N+1) */
	*div4 = 0;

	if (pclk_rate == CLK_312_5_MHZ) {
		switch (rate) {
		case CLK_2_5_MHZ:
			*div = 24;
			*cdiv = 9;
			break;
		case CLK_12_5_MHZ:
			*div = 9;
			*cdiv = 4;
			break;
		case CLK_25_MHZ:
			*div = 24;
			break;
		case CLK_78_125_MHZ:
			*div = 7;
			break;
		case CLK_125_MHZ:
			*div = 4;
			break;
		case CLK_156_25_MHZ:
			*div = 3;
			break;
		case CLK_312_5_MHZ:
			*div = 1;
			*div4 = 3;
			break;
		default:
			return -EINVAL;
		}
	} else {
		/* 125MHz and no uniphy parent clk with maximum 312MHz*/
		switch (rate) {
		case CLK_2_5_MHZ:
			*div = 9;
			*cdiv = 9;
			break;
		case CLK_25_MHZ:
			*div = 9;
			break;
		case CLK_125_MHZ:
			*div = 1;
			break;
		case CLK_312_5_MHZ:
			*div = 1;
			break;
		default:
			return -EINVAL;
		}
	}

	return 0;
}

/*
 * clk->data encoding set by the network driver before calling clk_set_rate():
 *
 *   bits [7:0]  = mac_speed  (0=10M, 1=100M, 2=1G, 3=10G, 4=2.5G, 5=5G)
 *   bits [15:8] = gmac_type  (0=GMAC, 1=XGMAC)
 *
 * Both values are packed as: data = (gmac_type << 8) | mac_speed
 *
 * DIV4 (XGMII2GMII bridge) is supported for: 10M, 100M, 1G, 2.5G
 * DIV4 is NOT used for: 5G, 10G
 */
#define MAC_SPEED_10M		0
#define MAC_SPEED_100M		1
#define MAC_SPEED_1G		2
#define MAC_SPEED_10G		3
#define MAC_SPEED_2_5G		4
#define MAC_SPEED_5G		5

#define GMAC_TYPE_GMAC		0
#define GMAC_TYPE_XGMAC		1

/* Extract mac_speed and gmac_type from packed clk->data */
#define CLK_DATA_MAC_SPEED(data)	((int)((data) & 0xFF))
#define CLK_DATA_GMAC_TYPE(data)	((int)(((data) >> 8) & 0xFF))

/*
 * Returns true if the port needs the DIV4 XGMII2GMII bridge divider.
 *
 * Ports 1-4 operate in two modes:
 *   GMAC  (gmac_type=0): uses GMII interface -> DIV4 bridge IS required
 *   XGMAC (gmac_type=1): uses XGMII interface directly -> DIV4 NOT required
 */
static inline bool nss_port_clk_needs_div4(int gmac_type)
{
	return gmac_type == GMAC_TYPE_GMAC;
}

static ulong ipq5210_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret, src, div = 0, cdiv = 0, div4 = 0;
	int gmac_type = CLK_DATA_GMAC_TYPE(clk->data); /* MAC type: 0=GMAC, 1=XGMAC */

	switch (clk->id) {
	case GCC_QUPV3_I2C0_CLK:
		/* Default: 32MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_I2C0_CMD_RCGR,
				     1, 1, 25, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_QUPV3_I2C1_CLK:
		/* Default: 32MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_I2C1_CMD_RCGR,
				     1, 1, 25, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_QUPV3_UART1_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_UART1_CMD_RCGR,
				     0x19, 0, 0, CFG_CLK_SRC_CXO, 16);
		break;
	case GCC_QUPV3_UART2_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_UART2_CMD_RCGR,
				     0x19, 0, 0, CFG_CLK_SRC_CXO, 16);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC1_APPS_CMD_RCGR,
				     5, 0, 0, CFG_CLK_SRC_GPLL2, 16);
		break;
	case GCC_QUPV3_SPI0_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_QUPV3_SPI0_CMD_RCGR,
				     31, 0, 0, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_QUPV3_2X_CORE_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_QUPV3_2X_CORE_CMD_RCGR, 0,
				     7, 0,
				     GCC_QUPV3_2X_CORE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_PCNOC_BFDCD_CMD_RCGR, 0,
				    15, 0,
				    PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_SYSTEM_NOC_BFDCD_CMD_RCGR, 0,
				    8, 0,
				    SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_USB0_MASTER_CLK:
		/* Default: 200MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_MASTER_CMD_RCGR, 7,
					0, 0, CFG_CLK_SRC_GPLL0, 8);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		/* Default: 24MHz */
		writel(0, priv->base + GCC_USB0_MOCK_UTMI_DIV_CDIVR);
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_MOCK_UTMI_CMD_RCGR,
				1, 0, 0, CFG_CLK_SRC_CXO, 8);
		break;
	case GCC_USB0_AUX_CLK:
		/* Default: 24MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_AUX_CMD_RCGR, 1,
					0, 0, CFG_CLK_SRC_CXO, 8);
		break;
	case GCC_QPIC_CLK:
		/* GCC_QPIC_CLK: 100 MHz  */
		clk_rcg_set_rate_v2(priv->base, GCC_QPIC_CMD_RCGR,
				    0, 0xF, 0, CFG_CLK_SRC_GPLL0);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		src = CFG_CLK_SRC_GPLL0;
		switch (rate) {
		case IO_MACRO_CLK_24_MHZ:
			src = CFG_CLK_SRC_CXO;
			div = 0;
			break;
		case IO_MACRO_CLK_50_MHZ:
			div = 31;
			break;
		case IO_MACRO_CLK_100_MHZ:
			div = 15;
			break;
		case IO_MACRO_CLK_200_MHZ:
			div = 7;
			break;
		case IO_MACRO_CLK_228_MHZ:
			div = 6;
			break;
		case IO_MACRO_CLK_266_MHZ:
			div = 5;
			break;
		case IO_MACRO_CLK_320_MHZ:
			div = 4;
			break;
		case IO_MACRO_CLK_400_MHZ:
			div = 3;
			break;
		default:
			return -EINVAL;
		}
		clk_rcg_set_rate_v2(priv->base, GCC_QPIC_IO_MACRO_CMD_RCGR,
				    0, div, 0, src);
		break;
	case GCC_PCIE0_AUX_CLK:
		fallthrough;
	case GCC_PCIE1_AUX_CLK:
		/* GCC_PCIE_AUX_CLK: 20 MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_PCIE_AUX_CMD_RCGR,
					0x1F, 2, 5, PCIE_GPLL0_OUT_AUX, 16);
		break;
	case GCC_PCIE0_AXI_M_CLK:
		/* GCC_PCIE0_AXI_M_CLK: 240 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE0_AXI_M_CMD_RCGR,
					5, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_M_CLK:
		/* GCC_PCIE1_AXI_M_CLK: 266.67 MHz */
		clk_rcg_set_rate_v2(priv->base, GCC_PCIE1_AXI_M_CMD_RCGR,
					0, 8, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_AXI_S_CLK:
		/* GCC_PCIE0_AXI_S_CLK: 240 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE0_AXI_S_CMD_RCGR,
					5, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_S_CLK:
		/* GCC_PCIE1_AXI_S_CLK: 240 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE1_AXI_S_CMD_RCGR,
					5, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_RCHNG_CLK:
		/* GCC_PCIE0_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE0_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE1_RCHNG_CLK:
		/* GCC_PCIE1_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate(priv->base, GCC_PCIE1_RCHNG_CMD_RCGR,
					8, PCIE_GPLL0_OUT_MAIN);
		break;
	case NSS_CC_PPE_SWITCH_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PPE_CMD_RCGR,
				0, 1, 0, CMN_PLL_NSS_CLK_429M); /* CMN_PLL_NSS_CLK_429M source */
		break;
	case NSS_CC_CFG_CLK:
		/* NSS_CC_PON: 429 MHz)*/
		/* Rate: 429 MHz from CMN_PLL_NSS_CLK_429M */
		clk_rcg_set_rate(priv->base, NSS_CC_CFG_CMD_RCGR, 0x8,
				 (2 << 8)); /* 100M source */
		break;
	case GCC_NSSNOC_NSSCC_CLK:
		/* NSS_CC_PON: 429 MHz)*/
		/* Rate: 429 MHz from CMN_PLL_NSS_CLK_429M */
		clk_rcg_set_rate(priv->base, GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR,
				 1, (1 << 8)); /* 429M source */
		break;
	case NSS_CC_EIP_BFDCD_CLK:
		/* NSS_CC_PON: 429 MHz)*/
		/* Rate: 429 MHz from CMN_PLL_NSS_CLK_429M */
		clk_rcg_set_rate(priv->base, NSS_CC_EIP_BFDCD_CMD_RCGR,
				 1, CMN_PLL_NSS_CLK_429M); /* 429M source */
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT1_RX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT1_RX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT1_RX_CMD_RCGR,
				    NSS_CC_PORT1_RX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT1_TX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT1_TX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT1_TX_CMD_RCGR,
				    NSS_CC_PORT1_TX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT2_RX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT2_RX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT2_RX_CMD_RCGR,
				    NSS_CC_PORT2_RX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT2_TX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT2_TX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT2_TX_CMD_RCGR,
				    NSS_CC_PORT2_TX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT3_RX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT3_RX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT3_RX_CMD_RCGR,
				    NSS_CC_PORT3_RX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT3_TX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT3_TX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT3_TX_CMD_RCGR,
				    NSS_CC_PORT3_TX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT4_RX_CLK: {
		long parent_rate = clk_get_parent_rate(clk);

		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;

		if (parent_rate > 0) {
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT4_RX_CMD_RCGR,
					    NSS_CC_PORT4_RX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
			writel(div4, priv->base + NSS_CC_UNIPHY_PORT4_RX_DIV4_DIV_CDIVR);
			writel(0x1, priv->base + NSS_CC_UNIPHY_PORT4_RX_DIV4_CBCR);
		} else {
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT4_RX_CMD_RCGR,
					    NSS_CC_PORT4_RX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT4_RX_SRC_SEL_RX_GCC);
			writel(0, priv->base + NSS_CC_UNIPHY_PORT4_RX_DIV4_DIV_CDIVR);
			writel(0x1, priv->base + NSS_CC_UNIPHY_PORT4_RX_DIV4_CBCR);
		}
		break;
	}
	case NSS_CC_PORT4_TX_CLK: {
		long parent_rate = clk_get_parent_rate(clk);

		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;

		if (parent_rate > 0) {
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT4_TX_CMD_RCGR,
					    NSS_CC_PORT4_TX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
			writel(div4, priv->base + NSS_CC_UNIPHY_PORT4_TX_DIV4_DIV_CDIVR);
			writel(0x1, priv->base + NSS_CC_UNIPHY_PORT4_TX_DIV4_CBCR);
		} else {
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT4_TX_CMD_RCGR,
					    NSS_CC_PORT4_TX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT4_TX_SRC_SEL_TX_GCC);
			writel(0, priv->base + NSS_CC_UNIPHY_PORT4_TX_DIV4_DIV_CDIVR);
			writel(0x1, priv->base + NSS_CC_UNIPHY_PORT4_TX_DIV4_CBCR);
		}
		break;
	}
	case NSS_CC_PORT4_UNIPHY1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT4_RX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT4_RX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT4_RX_CMD_RCGR,
				    NSS_CC_PORT4_RX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT4_SRC_SEL_UNIPHY1_NSS_TX_CLK);
		break;
	case NSS_CC_PORT4_UNIPHY1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* DIV4 (XGMII2GMII bridge): GMAC mode needs it, XGMAC mode does not */
		if (!nss_port_clk_needs_div4(gmac_type))
			div4 = 0;
		writel(div4, priv->base + NSS_CC_UNIPHY_PORT4_TX_DIV4_DIV_CDIVR);
		writel(0x1, priv->base + NSS_CC_UNIPHY_PORT4_TX_DIV4_CBCR);
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT4_TX_CMD_RCGR,
				    NSS_CC_PORT4_TX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT4_SRC_SEL_UNIPHY1_NSS_TX_CLK);
		break;
	case NSS_CC_PORT5_RX_CLK: {
		long parent_rate = clk_get_parent_rate(clk);

		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;

		/* Port 5 doesn't need DIV4 configuration, regardless of parent */
		if (parent_rate > 0) {
			/* External PHY/switch case: sourced from this port's UNIPHY */
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT5_RX_CMD_RCGR,
					    NSS_CC_PORT5_RX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		} else {
			/* Internal EPHY case: sourced from the fixed EPHY raw clock */
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT5_RX_CMD_RCGR,
					    NSS_CC_PORT5_RX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT5_RX_SRC_SEL_RX_GCC);
		}
		break;
	}
	case NSS_CC_PORT5_TX_CLK: {
		long parent_rate = clk_get_parent_rate(clk);

		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;

		/* Port 5 doesn't need DIV4 configuration, regardless of parent */
		if (parent_rate > 0) {
			/* External PHY/switch case: sourced from this port's UNIPHY */
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT5_TX_CMD_RCGR,
					    NSS_CC_PORT5_TX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		} else {
			/* Internal EPHY case: sourced from the fixed EPHY raw clock */
			clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT5_TX_CMD_RCGR,
					    NSS_CC_PORT5_TX_CMD_RCGR + 0x8, div, cdiv,
					    NSS_CC_PORT5_TX_SRC_SEL_TX_GCC);
		}
		break;
	}
	case NSS_CC_PORT5_UNIPHY0_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* Port 5 doesn't need DIV4 configuration */
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT5_RX_CMD_RCGR,
				    NSS_CC_PORT5_RX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT5_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT5_UNIPHY0_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* Port 5 doesn't need DIV4 configuration */
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT5_TX_CMD_RCGR,
				    NSS_CC_PORT5_TX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT5_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT6_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* Port 6 doesn't need DIV4 configuration */
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT6_RX_CMD_RCGR,
				    NSS_CC_PORT6_RX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT6_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv, &div4);
		if (ret < 0)
			return ret;
		/* Port 6 doesn't need DIV4 configuration */
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT6_TX_CMD_RCGR,
				    NSS_CC_PORT6_TX_CMD_RCGR + 0x8, div, cdiv,
				    NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_UNIPHY_PORT1_RX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT1_TX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT2_RX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT2_TX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT3_RX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT3_TX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT4_RX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT4_TX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT5_RX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT5_TX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT6_RX_CLK:
		fallthrough;
	case NSS_CC_UNIPHY_PORT6_TX_CLK:
		fallthrough;
	case UNIPHY0_NSS_RX_CLK:
		fallthrough;
	case UNIPHY0_NSS_TX_CLK:
		fallthrough;
	case UNIPHY1_NSS_RX_CLK:
		fallthrough;
	case UNIPHY1_NSS_TX_CLK:
		fallthrough;
	case UNIPHY2_NSS_RX_CLK:
		fallthrough;
	case UNIPHY2_NSS_TX_CLK:
		if (rate == CLK_125_MHZ) {
			clk->rate = CLK_125_MHZ;
		} else if (rate == CLK_312_5_MHZ) {
			clk->rate = CLK_312_5_MHZ;
		} else {
			ret = -EINVAL;
		}
		break;
	case GCC_APSS_AXI_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_APSS_AXI_CMD_RCGR, 0,
				     1, 0,
				     GCC_APSS_AXI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_APSS_AHB_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_APSS_AHB_CMD_RCGR, 0,
				     15, 0,
				     GCC_APSS_AHB_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QDSS_TSCTR_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_TSCTR_CMD_RCGR, 0,
				     3, 0,
				     GCC_QDSS_TSCTR_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_QDSS_AT_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_AT_CMD_RCGR, 0,
				     9, 0,
				     GCC_QDSS_AT_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case APCS_ALIAS0_CLK:
		clk_rcg_set_rate_v2(priv->base, APCS_ALIAS0_CMD_RCGR, 0,
				    1, 0,
				    APCS_ALIAS0_SRC_SEL_APCPLL_OUT_EARLY);
		break;

	default:
		return -EINVAL;
	}

	return rate;
}

static const struct gate_clk ipq5210_clks[] = {
	GATE_CLK(GCC_QUPV3_UART1_CLK,		0x05020, 0x00000001),
	GATE_CLK(GCC_QUPV3_UART2_CLK,		0x0302C, 0x00000001),
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x3303C, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	GATE_CLK(GCC_QUPV3_SPI0_CLK,		0x0302C, 0x00000001),
	GATE_CLK(GCC_USB0_MOCK_UTMI_CLK,	0x2C050, 0x00000001),
	GATE_CLK(GCC_USB0_MASTER_CLK,		0x2C044, 0x00000001),
	GATE_CLK(GCC_USB0_AUX_CLK,		0x2C04C, 0x00000001),
	GATE_CLK(GCC_USB0_PIPE_CLK,		0x2C054, 0x00000001),
	GATE_CLK(GCC_USB0_SLEEP_CLK,		0x2C058, 0x00000001),
	GATE_CLK(GCC_USB0_PHY_CFG_AHB_CLK,	0x2C05C, 0x00000001),
	GATE_CLK(GCC_QPIC_CLK,			0x32028, 0x00000001),
	GATE_CLK(GCC_QPIC_AHB_CLK,		0x32010, 0x00000001),
	GATE_CLK(GCC_QPIC_IO_MACRO_CLK,		0x3200C, 0x00000001),
	GATE_CLK(GCC_QUPV3_I2C0_CLK,		0x0202C,  0x00000001),
	GATE_CLK(GCC_QUPV3_I2C1_CLK,		0x02048,  0x00000001),
	GATE_CLK(GCC_PCIE0_AHB_CLK,		0x28030,  0x00000001),
	GATE_CLK(GCC_PCIE0_AUX_CLK,		0x28070,  0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_M_CLK,		0x28038,  0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_S_BRIDGE_CLK,	0x28048,  0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_S_CLK,		0x28040,  0x00000001),
	GATE_CLK(GCC_PCIE0_PIPE_CLK,		0x28068,  0x00000001),
	GATE_CLK(GCC_PCIE1_AHB_CLK,		0x29030,  0x00000001),
	GATE_CLK(GCC_PCIE1_AUX_CLK,		0x29074,  0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_M_CLK,		0x29038,  0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_S_BRIDGE_CLK,	0x29048,  0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_S_CLK,		0x29040,  0x00000001),
	GATE_CLK(GCC_PCIE1_PIPE_CLK,		0x29068,  0x00000001),
	GATE_CLK(GCC_CNOC_PCIE0_1LANE_S_CLK,	0x31088,  0x00000001),
	GATE_CLK(GCC_CNOC_PCIE1_2LANE_S_CLK,	0x3108C,  0x00000001),
	GATE_CLK(GCC_SNOC_PCIE0_AXI_M_CLK,	0x2E04C,  0x00000001),
	GATE_CLK(GCC_SNOC_PCIE1_AXI_M_CLK,      0x2E050,  0x00000001),

	GATE_CLK(GCC_IM_SLEEP_CLK,		0x34020, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_AHB_CLK,	0x3A004, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_APU_CLK,	0x3A00C, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_SYS_CLK,	0x3A008, 0x00000001),
	GATE_CLK(GCC_UNIPHY0_AHB_CLK,		0x1704C, 0x00000001),
	GATE_CLK(GCC_UNIPHY0_SYS_CLK,		0x17048, 0x00000001),
	GATE_CLK(GCC_UNIPHY1_AHB_CLK,		0x1705C, 0x00000001),
	GATE_CLK(GCC_UNIPHY1_SYS_CLK,		0x17058, 0x00000001),
	GATE_CLK(GCC_UNIPHY2_AHB_CLK,		0x1706C, 0x00000001),
	GATE_CLK(GCC_UNIPHY2_SYS_CLK,		0x17068, 0x00000001),
	GATE_CLK(GCC_NSSNOC_NSSCC_CLK,		0x17030, 0x00000001),
	GATE_CLK(GCC_NSSCC_CLK,			0x17034, 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_1_CLK,		0x1707C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_CLK,		0x17028, 0x00000001),
	/* Core NSS Clocks */
	GATE_CLK(NSS_CC_NSS_CSR_CLK,		0x00714, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_NSS_CSR_CLK,	0x00718, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_IPE_CLK,	0x00424, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_BTQ_CLK,	0x0042C, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CLK,		0x00434, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CFG_CLK,	0x0043C, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CLK,		0x00440, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CFG_CLK,	0x00448, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CLK,		0x004A4, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CFG_CLK,	0x004A8, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_MAC_CLK,		0x0044C, 0x00000001),
	GATE_CLK(NSS_CC_PORT2_MAC_CLK,		0x00454, 0x00000001),
	GATE_CLK(NSS_CC_PORT3_MAC_CLK,		0x0045C, 0x00000001),
	GATE_CLK(NSS_CC_PORT4_MAC_CLK,		0x00464, 0x00000001),
	GATE_CLK(NSS_CC_PORT5_MAC_CLK,		0x0046C, 0x00000001),
	GATE_CLK(NSS_CC_PORT6_MAC_CLK,		0x00474, 0x00000001),
	GATE_CLK(NSS_CC_PON_CLK,		0x0047C, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_RX_CLK,		0x00548, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_TX_CLK,		0x00550, 0x00000001),
	GATE_CLK(NSS_CC_PORT2_RX_CLK,		0x00558, 0x00000001),
	GATE_CLK(NSS_CC_PORT2_TX_CLK,		0x00560, 0x00000001),
	GATE_CLK(NSS_CC_PORT3_RX_CLK,		0x00568, 0x00000001),
	GATE_CLK(NSS_CC_PORT3_TX_CLK,		0x00570, 0x00000001),
	GATE_CLK(NSS_CC_PORT4_RX_CLK,		0x00578, 0x00000001),
	GATE_CLK(NSS_CC_PORT4_TX_CLK,		0x00580, 0x00000001),
	GATE_CLK(NSS_CC_PORT5_RX_CLK,		0x00588, 0x00000001),
	GATE_CLK(NSS_CC_PORT5_TX_CLK,		0x00590, 0x00000001),
	GATE_CLK(NSS_CC_PORT6_RX_CLK,		0x00598, 0x00000001),
	GATE_CLK(NSS_CC_PORT6_TX_CLK,		0x005A0, 0x00000001),
	GATE_CLK(NSS_CC_XGMAC0_PTP_REF_CLK,	0x00488, 0x00000001),
	GATE_CLK(NSS_CC_XGMAC1_PTP_REF_CLK,	0x0048C, 0x00000001),
	GATE_CLK(NSS_CC_XGMAC2_PTP_REF_CLK,	0x00490, 0x00000001),
	GATE_CLK(NSS_CC_DEBUG_CLK,		0x00750, 0x00000001),
	GATE_CLK(NSS_CC_CE_APB_CLK,		0x00610, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_RX_CLK,	0x005E0, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_TX_CLK,	0x005E4, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_RX_DIV4_CLK,	0x00644, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_TX_DIV4_CLK,	0x00648, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_RX_CLK,	0x005E8, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_TX_CLK,	0x005EC, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_RX_DIV4_CLK,	0x0064C, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_TX_DIV4_CLK,	0x00650, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_RX_CLK,	0x005F0, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_TX_CLK,	0x005F4, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_RX_DIV4_CLK,	0x00654, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_TX_DIV4_CLK,	0x00658, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_RX_CLK,	0x005F8, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_TX_CLK,	0x005FC, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_RX_DIV4_CLK,	0x0065C, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_TX_DIV4_CLK,	0x00660, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT5_RX_CLK,	0x00600, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT5_TX_CLK,	0x00604, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT6_RX_CLK,	0x00608, 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT6_TX_CLK,	0x0060C, 0x00000001),
	GATE_CLK(GCC_MDIO_AHB_CLK,		0x17060, 0x00000001),
	GATE_CLK(GCC_QUPV3_AHB_MST_CLK,		0x0B004, 0x00004000),
	GATE_CLK(GCC_QUPV3_AHB_SLV_CLK,		0x0B004, 0x00000010),
	GATE_CLK(GCC_QUPV3_CORE_CLK,		0x0B004, 0x00008000),
	GATE_CLK(GCC_QUPV3_2X_CORE_CLK,		0x0B004, 0x00010000),
	GATE_CLK(GCC_QUPV3_SLEEP_CLK,		0x0B004, 0x00000020),
	GATE_CLK(GCC_APCS_CLOCK_BRANCH_ENA_VOTE,0x0B004, 0x00002000),
	GATE_CLK(GCC_MDIO_AHB_CLK,		0x17040, 0x00000001),
	GATE_CLK(GCC_MDIO_GEPHY_AHB_CLK,	0x17098, 0x00000001),

	GATE_CLK(GCC_APSS_DBG_CLK,		0x2402C, 0x00000001),
	GATE_CLK(GCC_APSS_TS_CLK,		0x24030, 0x00000001),
	GATE_CLK(GCC_SNOC_QOSGEN_EXTREF_CLK,	0x2E020, 0x00000001),
	GATE_CLK(GCC_SYS_NOC_AT_CLK,		0x2E038, 0x00000001),
	GATE_CLK(GCC_SNOC_XO_DCD_CLK,		0x2E060, 0x00000001),
	GATE_CLK(GCC_SNOC_TS_CLK,		0x2E068, 0x00000001),
	GATE_CLK(GCC_PCNOC_AT_CLK,		0x31024, 0x00000001),
	GATE_CLK(GCC_PCNOC_TS_CLK,		0x3102C, 0x00000001),
	GATE_CLK(GCC_CNOC_QOSGEN_EXTREF_CLK,	0x310B0, 0x00000001),

	GATE_CLK(NSS_CC_CFG_CLK,		0x00714, 0x00000001),
	GATE_CLK(NSS_CC_EIP_BFDCD_CLK,		0x006BC, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_EIP_CLK,		0x006C4, 0x00000001),
	GATE_CLK(GCC_GEPHY_SYS_CLK,		0x2A004, 0x00000001),
	GATE_CLK(GCC_NSSNOC_MEMNOC_CLK,		0x17024, 0x00000001),
	/* IPQ52xx internal EPHY clocks */
	GATE_CLK(NSS_CC_EPHY_RX_CLK,		0x00618, 0x00000001),
	GATE_CLK(NSS_CC_EPHY_TX_CLK,		0x0061C, 0x00000001),
};

static int ipq5210_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq5210_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq5210_gcc_resets[] = {
	[GCC_SDCC_BCR]			= {0x33000, 0},
	[GCC_QPIC_BCR]			= {0x32000, 0},
	[GCC_USB0_PHY_BCR]		= {0x2C06C, 0},
	[GCC_USB3PHY_0_PHY_BCR]		= {0x2C070, 0},
	[GCC_QUSB2_0_PHY_BCR]		= {0x2C068, 0},
	[GCC_USB_BCR]			= {0x2C000, 0},
	[GCC_PCIE0_PHY_BCR]		= {0x28060, 0},
	[GCC_PCIE0PHY_PHY_BCR]		= {0x2805c, 0},
	[GCC_PCIE1_PHY_BCR]		= {0x29060, 0},
	[GCC_PCIE1PHY_PHY_BCR]		= {0x2905c, 0},
	[GCC_PCIE1_PIPE_ARES]		= {0x29068, 2},
	[GCC_PCIE1_CORE_STICKY_RESET]	= {0x29058, 1},
	[GCC_PCIE1_AXI_M_ARES]		= {0x29038, 2},
	[GCC_PCIE1_AXI_S_ARES]		= {0x29040, 2},
	[GCC_PCIE1_AXI_M_STICKY_RESET]	= {0x29058, 4},
	[GCC_PCIE1_AXI_S_STICKY_RESET]	= {0x29058, 2},
	[GCC_PCIE1_AHB_ARES]		= {0x29030, 2},
	[GCC_PCIE1_AUX_ARES]		= {0x29074, 2},
	[GCC_PCIE0_PIPE_ARES]		= {0x28068, 2},
	[GCC_PCIE0_CORE_STICKY_RESET]	= {0x28058, 1},
	[GCC_PCIE0_AXI_M_ARES]		= {0x28038, 2},
	[GCC_PCIE0_AXI_S_ARES]		= {0x28040, 2},
	[GCC_PCIE0_AXI_M_STICKY_RESET]	= {0x28058, 4},
	[GCC_PCIE0_AXI_S_STICKY_RESET]	= {0x28058, 2},
	[GCC_PCIE0_AHB_ARES]		= {0x28030, 2},
	[GCC_PCIE0_AUX_ARES]		= {0x28070, 2},
	[GCC_UNIPHY0_BCR]		= {0x17044, 0},
	[GCC_UNIPHY1_BCR]		= {0x17054, 0},
	[GCC_UNIPHY2_BCR]		= {0x17064, 0},
	[GCC_NSS_BCR]			= {0x17000, 0},
	[NSS_CC_PPE_BCR]		= {0x003E8, 0},
	[GCC_UNIPHY0_AHB_ARES]		= {0x1704C, 2},
	[GCC_UNIPHY1_AHB_ARES]		= {0x1705C, 2},
	[GCC_UNIPHY2_AHB_ARES]		= {0x1706C, 2},
	[GCC_UNIPHY0_SYS_ARES]		= {0x17048, 2},
	[GCC_UNIPHY1_SYS_ARES]		= {0x17058, 2},
	[GCC_UNIPHY2_SYS_ARES]		= {0x17068, 2},
	[GCC_UNIPHY0_XPCS_ARES]		= {0x17050, 2},
	[GCC_UNIPHY1_XPCS_ARES]		= {0x17060, 2},
	[GCC_UNIPHY2_XPCS_ARES]		= {0x17070, 2},
	[NSS_CC_UNIPHY_PORT1_RX_CLK_ARES] = {0x005E0, 2},
	[NSS_CC_UNIPHY_PORT1_TX_CLK_ARES] = {0x005E4, 2},
	[NSS_CC_UNIPHY_PORT1_RX_CLK_DIV4_ARES] = {0x00644, 2},
	[NSS_CC_UNIPHY_PORT1_TX_CLK_DIV4_ARES] = {0x00648, 2},
	[NSS_CC_UNIPHY_PORT2_RX_CLK_ARES] = {0x005E8, 2},
	[NSS_CC_UNIPHY_PORT2_TX_CLK_ARES] = {0x005EC, 2},
	[NSS_CC_UNIPHY_PORT2_RX_CLK_DIV4_ARES] = {0x0064C, 2},
	[NSS_CC_UNIPHY_PORT2_TX_CLK_DIV4_ARES] = {0x00650, 2},
	[NSS_CC_UNIPHY_PORT3_RX_CLK_ARES] = {0x005F0, 2},
	[NSS_CC_UNIPHY_PORT3_TX_CLK_ARES] = {0x005F4, 2},
	[NSS_CC_UNIPHY_PORT3_RX_CLK_DIV4_ARES] = {0x00654, 2},
	[NSS_CC_UNIPHY_PORT3_TX_CLK_DIV4_ARES] = {0x00658, 2},
	[NSS_CC_UNIPHY_PORT4_RX_CLK_ARES] = {0x005F8, 2},
	[NSS_CC_UNIPHY_PORT4_TX_CLK_ARES] = {0x005FC, 2},
	[NSS_CC_UNIPHY_PORT4_RX_CLK_DIV4_ARES] = {0x0065C, 2},
	[NSS_CC_UNIPHY_PORT4_TX_CLK_DIV4_ARES] = {0x00660, 2},
	[NSS_CC_UNIPHY_PORT5_RX_CLK_ARES] = {0x00600, 2},
	[NSS_CC_UNIPHY_PORT5_TX_CLK_ARES] = {0x00604, 2},
	[NSS_CC_UNIPHY_PORT6_RX_CLK_ARES] = {0x00608, 2},
	[NSS_CC_UNIPHY_PORT6_TX_CLK_ARES] = {0x0060C, 2},
	[NSS_CC_PORT1_RX_CLK_ARES]	= {0x00548, 2},
	[NSS_CC_PORT1_TX_CLK_ARES]	= {0x00550, 2},
	[NSS_CC_PORT2_RX_CLK_ARES]	= {0x00558, 2},
	[NSS_CC_PORT2_TX_CLK_ARES]	= {0x00560, 2},
	[NSS_CC_PORT3_RX_CLK_ARES]	= {0x00568, 2},
	[NSS_CC_PORT3_TX_CLK_ARES]	= {0x00570, 2},
	[NSS_CC_PORT4_RX_CLK_ARES]	= {0x00578, 2},
	[NSS_CC_PORT4_TX_CLK_ARES]	= {0x00580, 2},
	[NSS_CC_PORT5_RX_CLK_ARES]	= {0x00588, 2},
	[NSS_CC_PORT5_TX_CLK_ARES]	= {0x00590, 2},
	[NSS_CC_PORT6_RX_CLK_ARES]	= {0x00598, 2},
	[NSS_CC_PORT6_TX_CLK_ARES]	= {0x005A0, 2},
	[NSS_CC_PORT1_MAC_CLK_ARES]	= {0x0044C, 2},
	[NSS_CC_PORT2_MAC_CLK_ARES]	= {0x00454, 2},
	[NSS_CC_PORT3_MAC_CLK_ARES]	= {0x0045C, 2},
	[NSS_CC_PORT4_MAC_CLK_ARES]	= {0x00464, 2},
	[NSS_CC_PORT5_MAC_CLK_ARES]	= {0x0046C, 2},
	[NSS_CC_PORT6_MAC_CLK_ARES]	= {0x00474, 2},
	[GCC_NSS_PARTIAL_RESET]		= {0x17008, 0},
	/* IPQ52xx internal EPHY resets (bit 2 of CBCR) */
	[NSS_CC_EPHY_RX_CLK_ARES]	= {0x00618, 2},
	[NSS_CC_EPHY_TX_CLK_ARES]	= {0x0061C, 2},
	[GCC_GEPHY_SYS_CLK_ARES]	= {0x2A004, 2},
	/* NSS_CC_PPE_EDMA_CBCR bit 2 (CLK_ARES): resets EDMA core */
	[NSS_CC_PPE_EDMA_CLK_ARES]	= {0x00440, 2},
	/* NSS_CC_PPE_EDMA_CFG_CBCR bit 2 (CLK_ARES): resets EDMA CFG core */
	[NSS_CC_PPE_EDMA_CFG_CLK_ARES]	= {0x00448, 2},
};

static struct msm_clk_data ipq5210_gcc_data = {
	.resets = ipq5210_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5210_gcc_resets),
	.clks = ipq5210_clks,
	.num_clks = ARRAY_SIZE(ipq5210_clks),
	.enable = ipq5210_enable,
	.set_rate = ipq5210_set_rate,
};

static struct msm_clk_data ipq5210_apss_data = {
	.resets = ipq5210_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq5210_gcc_resets),
	.clks = ipq5210_clks,
	.num_clks = ARRAY_SIZE(ipq5210_clks),
	.enable = ipq5210_enable,
	.set_rate = ipq5210_set_rate,
};

static const struct udevice_id gcc_ipq5210_of_match[] = {
	{
		.compatible = "qcom,ipq5210-gcc",
		.data = (ulong)&ipq5210_gcc_data,
	},
	{ }
};

static const struct udevice_id apss_ipq5210_of_match[] = {
	{
		.compatible = "qcom,ipq5210-apss",
		.data = (ulong)&ipq5210_apss_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq5210) = {
	.name		= "gcc_ipq5210",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq5210_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};

U_BOOT_DRIVER(apss_ipq5210) = {
	.name		= "apss_ipq5210",
	.id		= UCLASS_NOP,
	.of_match	= apss_ipq5210_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
