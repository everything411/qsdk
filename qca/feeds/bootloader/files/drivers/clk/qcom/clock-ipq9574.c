// SPDX-License-Identifier: GPL-2.0
/*
 * Clock drivers for Qualcomm ipq9574
 *
 * (C) Copyright 2024 Linaro Ltd.
 * Copyright (c) 2025, Qualcomm Innovation Center,Inc.All rights reserved.
 */

#include <linux/types.h>
#include <clk-uclass.h>
#include <dm.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <dm/device-internal.h>
#include <dt-bindings/clock/qcom,ipq9574-gcc.h>
#include <asm/arch/dt-bindings/clock/qcom,ipq9574-gcc.h>
#include <dt-bindings/reset/qcom,ipq9574-gcc.h>
#include <asm/arch/dt-bindings/reset/qcom,ipq9574-gcc.h>

#include "clock-qcom.h"

#define APCS_CLOCK_BRANCH_ENA_VOTE		0x0B004
#define GCC_BLSP1_AHB_CBCR			0x1004
#define	GCC_BLSP1_UART1_APPS_CMD_RCGR		0x202C
#define	GCC_BLSP1_UART2_APPS_CMD_RCGR		0x302C
#define	GCC_BLSP1_UART3_APPS_CMD_RCGR		0x402C
#define	GCC_BLSP1_UART4_APPS_CMD_RCGR		0x502C
#define	GCC_BLSP1_UART5_APPS_CMD_RCGR		0x602C
#define	GCC_BLSP1_UART6_APPS_CMD_RCGR		0x702C
#define GCC_BLSP1_UART1_APPS_CBCR		0x2054
#define GCC_BLSP1_UART2_APPS_CBCR		0x3054
#define GCC_BLSP1_UART3_APPS_CBCR		0x4054
#define GCC_BLSP1_UART4_APPS_CBCR		0x5054
#define GCC_BLSP1_UART5_APPS_CBCR		0x6054
#define GCC_BLSP1_UART6_APPS_CBCR		0x7054
#define GCC_SDCC1_APPS_CMD_RCGR			0x33004
#define GCC_SDCC1_APPS_CBCR			0x3302C
#define GCC_SDCC1_AHB_CBCR			0x33034
#define GCC_SDCC1_ICE_CORE_CMD_RCGR		0x33018
#define GCC_SDCC1_ICE_CORE_CBCR			0x33030

/* BLSP QUP SPI clock register */
#define BLSP1_QUP1_SPI_BCR		0x02000

#define BLSP1_QUP_SPI_BCR(id)		((id < 1) ? \
					(BLSP1_QUP1_SPI_BCR) : \
					(BLSP1_QUP1_SPI_BCR + (0x1000 * id)))

#define BLSP1_QUP_SPI_APPS_CMD_RCGR(id)	(BLSP1_QUP_SPI_BCR(id) + 0x04)
#define BLSP1_QUP_SPI_APPS_CFG_RCGR(id)	(BLSP1_QUP_SPI_BCR(id) + 0x08)
#define BLSP1_QUP_SPI_APPS_M(id)	(BLSP1_QUP_SPI_BCR(id) + 0x0c)
#define BLSP1_QUP_SPI_APPS_N(id)	(BLSP1_QUP_SPI_BCR(id) + 0x10)
#define BLSP1_QUP_SPI_APPS_D(id)	(BLSP1_QUP_SPI_BCR(id) + 0x14)
#define BLSP1_QUP_SPI_APPS_CBCR(id)	(BLSP1_QUP_SPI_BCR(id) + 0x20)

#define GCC_UNIPHY0_SYS_CBCR				(0x17048)
#define GCC_UNIPHY0_AHB_CBCR				(0x1704C)
#define GCC_UNIPHY_SYS_CBCR(id)		((id < 1) ? \
					(GCC_UNIPHY0_SYS_CBCR) : \
					(GCC_UNIPHY0_SYS_CBCR + (0x10 * id)))
#define GCC_UNIPHY_AHB_CBCR(id)		((id < 1) ? \
					(GCC_UNIPHY0_AHB_CBCR) : \
					(GCC_UNIPHY0_AHB_CBCR + (0x10 * id)))

#define NSS_CC_PORT1_MAC_CBCR		(0x2824C)
#define NSS_CC_PORT_MAC_CBCR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_MAC_CBCR) : \
					(NSS_CC_PORT1_MAC_CBCR + (0x4 * (id - 1))))

#define NSS_CC_PORT1_RX_CBCR				(0x281A0)
#define NSS_CC_PORT1_TX_CBCR				(0x281A4)
#define NSS_CC_PORT_RX_CBCR(id)		((id <= 1) ? \
					(NSS_CC_PORT1_RX_CBCR) : \
					(NSS_CC_PORT1_RX_CBCR + (0x8 * (id - 1))))
#define NSS_CC_PORT_TX_CBCR(id)		((id <= 1) ? \
					(NSS_CC_PORT1_TX_CBCR) : \
					(NSS_CC_PORT1_TX_CBCR + (0x8 * (id - 1))))

#define NSS_CC_UNIPHY_PORT1_RX_CBCR			(0x28904)
#define NSS_CC_UNIPHY_PORT1_TX_CBCR			(0x28908)
#define NSS_CC_UNIPHY_PORT_RX_CBCR(id)	((id <= 1) ? \
					(NSS_CC_UNIPHY_PORT1_RX_CBCR) : \
					(NSS_CC_UNIPHY_PORT1_RX_CBCR +\
					(0x8 * (id - 1))))
#define NSS_CC_UNIPHY_PORT_TX_CBCR(id)	((id <= 1) ? \
					(NSS_CC_UNIPHY_PORT1_TX_CBCR) : \
					(NSS_CC_UNIPHY_PORT1_TX_CBCR +\
					(0x8 * (id - 1))))

#define NSS_CC_CFG_CMD_RCGR				(0x28104)
#define NSS_CC_PORT1_RX_CMD_RCGR			(0x28110)
#define NSS_CC_PORT1_TX_CMD_RCGR			(0x2811C)
#define NSS_CC_PORT_RX_CMD_RCGR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_RX_CMD_RCGR) : \
					(NSS_CC_PORT1_RX_CMD_RCGR +\
					 (0x18 * (id - 1))))
#define NSS_CC_PORT_TX_CMD_RCGR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_TX_CMD_RCGR) : \
					(NSS_CC_PORT1_TX_CMD_RCGR +\
					 (0x18 * (id - 1))))

#define NSS_CC_PORT1_RX_DIV_CDIVR			(0x28118)
#define NSS_CC_PORT_RX_DIV_CDIVR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_RX_DIV_CDIVR) : \
					(NSS_CC_PORT1_RX_DIV_CDIVR +\
					(0x18 * (id - 1))))

#define NSS_CC_PORT1_TX_DIV_CDIVR			(0x28124)
#define NSS_CC_PORT_TX_DIV_CDIVR(id)	((id <= 1) ? \
					(NSS_CC_PORT1_TX_DIV_CDIVR) : \
					(NSS_CC_PORT1_TX_DIV_CDIVR +\
					(0x18 * (id - 1))))

#define NSS_CC_PPE_CMD_RCGR				(0x28204)

#define NSS_CC_CFG_SRC_SEL_GCC_GPLL0_OUT_AUX		(2 << 8)
#define NSS_CC_PPE_SRC_SEL_BIAS_PLL_UBI_NC_CLK		BIT(8)
#define NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK	(2 << 8)
#define NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK	(3 << 8)
#define NSS_CC_PORT5_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK	(2 << 8)
#define NSS_CC_PORT5_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK	(3 << 8)
#define NSS_CC_PORT5_RX_SRC_SEL_UNIPHY1_NSS_RX_CLK	(4 << 8)
#define NSS_CC_PORT5_TX_SRC_SEL_UNIPHY1_NSS_TX_CLK	(5 << 8)
#define NSS_CC_PORT6_RX_SRC_SEL_UNIPHY2_NSS_RX_CLK	(2 << 8)
#define NSS_CC_PORT6_TX_SRC_SEL_UNIPHY2_NSS_TX_CLK	(3 << 8)

#define CLK_1_25_MHZ			(1250000UL)
#define CLK_2_5_MHZ			(2500000UL)
#define CLK_12_5_MHZ			(12500000UL)
#define CLK_25_MHZ			(25000000UL)
#define CLK_78_125_MHZ			(78125000UL)
#define CLK_50_MHZ			(50000000UL)
#define CLK_125_MHZ			(125000000UL)
#define CLK_156_25_MHZ			(156250000UL)
#define CLK_312_5_MHZ			(312500000UL)

#define GCC_NSSNOC_MEMNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN	BIT(8)
#define GCC_QDSS_AT_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN		BIT(8)
#define GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN	(2 << 8)
#define PCIE_SRC_SEL_UNSUSED_GND			(1 << 8)
#define PCIE_SRC_SEL_GPLL4_OUT_MAIN			(2 << 8)
#define PCIE_SRC_SEL_GPLL0_OUT_MAIN			(1 << 8)

#define GCC_UNIPHY_SYS_CMD_RCGR				(0x17090)
#define GCC_PCNOC_BFDCD_CMD_RCGR			(0x31004)
#define GCC_SYSTEM_NOC_BFDCD_CMD_RCGR			(0x2E004)
#define GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR		(0x17004)
#define GCC_QDSS_AT_CMD_RCGR				(0x2D004)
/* BLSP QUP I2C clock register */
#define BLSP1_QUP0_I2C_BCR		0x02000

#define BLSP1_QUP_I2C_BCR(id)		((id < 1) ? \
					(BLSP1_QUP0_I2C_BCR) : \
					(BLSP1_QUP0_I2C_BCR + (0x1000 * id)))

#define BLSP1_QUP_I2C_APPS_CMD_RCGR(id)	(BLSP1_QUP_I2C_BCR(id) + 0x18)
#define BLSP1_QUP_I2C_APPS_CFG_RCGR(id)	(BLSP1_QUP_I2C_BCR(id) + 0x1C)
#define BLSP1_QUP_I2C_APPS_CBCR(id)	(BLSP1_QUP_I2C_BCR(id) + 0x24)

#define BLSP1_QUP_I2C_50M_DIV_VAL	(0x1F << 0)

#define GCC_PCIE_BASE					0x28000
#define GCC_PCIE_OFFSET(_id, _off)	(GCC_PCIE_BASE + (_id * 0x1000) + _off)

#define GCC_PCIE_AUX_CMD_RCGR			(GCC_PCIE_BASE + 0x4)

#define GCC_PCIE_AXI_M_CMD_RCGR(id)		GCC_PCIE_OFFSET(id, 0x18)
#define GCC_PCIE_AXI_S_CMD_RCGR(id)		GCC_PCIE_OFFSET(id, 0x20)
#define GCC_PCIE_RCHNG_CMD_RCGR(id)		GCC_PCIE_OFFSET(id, 0x28)

#define GCC_QPIC_IO_MACRO_CMD_RCGR		(0x32004)
#define IO_MACRO_CLK_400_MHZ			(400000000)
#define IO_MACRO_CLK_320_MHZ			(320000000)
#define IO_MACRO_CLK_266_MHZ			(266000000)
#define IO_MACRO_CLK_228_MHZ			(228000000)
#define IO_MACRO_CLK_200_MHZ			(200000000)
#define IO_MACRO_CLK_100_MHZ			(100000000)
#define IO_MACRO_CLK_24_MHZ			(24000000)


static bool fdone;

#define GCC_USB0_MOCK_UTMI_CMD_RCGR	(0x2C02C)
#define GCC_USB0_AUX_CMD_RCGR		(0x2C018)
#define GCC_USB0_MASTER_CMD_RCGR	(0x2C004)

static int calc_div_for_nss_port_clk(struct clk *clk, ulong rate,
				     int *div, int *cdiv)
{
	int pclk_rate = clk_get_parent_rate(clk);

	if (pclk_rate == CLK_125_MHZ) {
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
		default:
			return -EINVAL;
		}
	} else if (pclk_rate == CLK_312_5_MHZ) {
		switch (rate) {
		case CLK_1_25_MHZ:
			*div = 19;
			*cdiv = 24;
			break;
		case CLK_12_5_MHZ:
			*div = 9;
			*cdiv = 4;
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
			break;
		default:
			return -EINVAL;
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

int msm_set_parent(struct clk *clk, struct clk *parent)
{
	assert(clk);
	assert(parent);
	clk->dev->parent = parent->dev;
	dev_set_uclass_priv(parent->dev, parent);
	return 0;
}

ulong msm_get_rate(struct clk *clk)
{
	switch (clk->id) {
	case GCC_BLSP1_QUP2_I2C_APPS_CLK:
	case GCC_BLSP1_QUP3_I2C_APPS_CLK:
	case GCC_BLSP1_QUP4_I2C_APPS_CLK:
	case GCC_BLSP1_QUP5_I2C_APPS_CLK:
		clk->rate = CLK_50_MHZ;
		break;
	}

	return (ulong)clk->rate;
}

static ulong ipq9574_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret, src, div = 0, cdiv = 0;
	struct clk *pclk = NULL;

	switch (clk->id) {
	case GCC_BLSP1_UART1_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART1_APPS_CMD_RCGR,
				     0, 36, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_UART2_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART2_APPS_CMD_RCGR,
				     0, 36, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_UART3_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART3_APPS_CMD_RCGR,
				     0, 36, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_UART4_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART4_APPS_CMD_RCGR,
				     0, 36, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_UART5_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART5_APPS_CMD_RCGR,
				     0, 36, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_UART6_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, GCC_BLSP1_UART6_APPS_CMD_RCGR,
				     0, 36, 15625, CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC1_APPS_CMD_RCGR,
				     23, 0, 0, CFG_CLK_SRC_GPLL2, 16);
		break;
	case GCC_SDCC1_ICE_CORE_CLK:
		/* ICE Core Clock: 300 MHz */
		clk_rcg_set_rate_mnd(priv->base, GCC_SDCC1_ICE_CORE_CMD_RCGR,
				     7, 0, 0, CFG_CLK_SRC_GPLL2, 8);
		break;
	case GCC_BLSP1_QUP1_SPI_APPS_CLK:
		/* QUP1 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(0), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP2_SPI_APPS_CLK:
		/* QUP2 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(1), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP3_SPI_APPS_CLK:
		/* QUP3 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(2), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP4_SPI_APPS_CLK:
		/* QUP4 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(3), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP5_SPI_APPS_CLK:
		/* QUP5 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(4), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP6_SPI_APPS_CLK:
		/* QUP6 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base,
				     BLSP1_QUP_SPI_APPS_CMD_RCGR(5), 16, 0, 0,
				     CFG_CLK_SRC_GPLL0, 16);
		break;
	case GCC_BLSP1_QUP2_I2C_APPS_CLK:
		/* QUP1 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, BLSP1_QUP_I2C_APPS_CMD_RCGR(1),
				 BLSP1_QUP_I2C_50M_DIV_VAL,
				 CFG_CLK_SRC_GPLL0);
		break;
	case GCC_BLSP1_QUP3_I2C_APPS_CLK:
		/* QUP2 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, BLSP1_QUP_I2C_APPS_CMD_RCGR(2),
				 BLSP1_QUP_I2C_50M_DIV_VAL,
				 CFG_CLK_SRC_GPLL0);
		break;
	case GCC_BLSP1_QUP4_I2C_APPS_CLK:
		/* QUP3 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, BLSP1_QUP_I2C_APPS_CMD_RCGR(3),
				 BLSP1_QUP_I2C_50M_DIV_VAL,
				 CFG_CLK_SRC_GPLL0);
		break;
	case GCC_BLSP1_QUP5_I2C_APPS_CLK:
		/* QUP4 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, BLSP1_QUP_I2C_APPS_CMD_RCGR(4),
				 BLSP1_QUP_I2C_50M_DIV_VAL,
				 CFG_CLK_SRC_GPLL0);
		break;
	case GCC_UNIPHY_SYS_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_UNIPHY_SYS_CMD_RCGR,
				    0, 1, 0, 0);
		break;
	case GCC_PCNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_PCNOC_BFDCD_CMD_RCGR,
				    0, 15, 0,
				    GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_SYSTEM_NOC_BFDCD_CMD_RCGR,
				    0, 6, 0,
				    GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_NSSNOC_MEMNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR,
				    0, 2, 0,
				    GCC_NSSNOC_MEMNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QDSS_AT_CLK:
		clk_rcg_set_rate_v2(priv->base, GCC_QDSS_AT_CMD_RCGR,
				    0, 9, 0,
				    GCC_QDSS_AT_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE0_AUX_CLK:
		fallthrough;
	case GCC_PCIE1_AUX_CLK:
		fallthrough;
	case GCC_PCIE2_AUX_CLK:
		fallthrough;
	case GCC_PCIE3_AUX_CLK:
		/* PCIE AUX CLK: 20MHZ */
		if (fdone)
			break;
		clk_rcg_set_rate_mnd(priv->base, GCC_PCIE_AUX_CMD_RCGR, 10, 1,
				     4, PCIE_SRC_SEL_UNSUSED_GND, 16);
		fdone =  true;
		break;
	case GCC_PCIE0_AXI_M_CLK:
		/* PCIE0_AXI_M_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_M_CMD_RCGR(0),
				 5, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_M_CLK:
		/* PCIE1_AXI_M_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_M_CMD_RCGR(1),
				 5, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE2_AXI_M_CLK:
		/* PCIE2_AXI_M_CLK: 342MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_M_CMD_RCGR(2),
				 6, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE3_AXI_M_CLK:
		/* PCIE3_AXI_M_CLK: 342MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_M_CMD_RCGR(3),
				 6, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_AXI_S_CLK:
		/* PCIE0_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_S_CMD_RCGR(0),
				 5, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_S_CLK:
		/* PCIE1_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_S_CMD_RCGR(1),
				 5, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE2_AXI_S_CLK:
		/* PCIE2_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_S_CMD_RCGR(2),
				 5, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE3_AXI_S_CLK:
		/* PCIE3_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_AXI_S_CMD_RCGR(3),
				 5, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_RCHNG_CLK:
		/* PCIE0_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_RCHNG_CMD_RCGR(0),
				 8, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE1_RCHNG_CLK:
		/* PCIE1_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_RCHNG_CMD_RCGR(1),
				 8, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE2_RCHNG_CLK:
		/* PCIE2_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_RCHNG_CMD_RCGR(2),
				 8, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE3_RCHNG_CLK:
		/* PCIE3_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, GCC_PCIE_RCHNG_CMD_RCGR(3),
				 8, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	/*
	 * NSS controlled clock
	 */
	case NSS_CC_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_CFG_CMD_RCGR,
				    0, 15, 0,
				    NSS_CC_CFG_SRC_SEL_GCC_GPLL0_OUT_AUX);
		break;
	case NSS_CC_PPE_CLK:
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PPE_CMD_RCGR,
				    0, 1, 0,
				    NSS_CC_PPE_SRC_SEL_BIAS_PLL_UBI_NC_CLK);
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(1),
				    NSS_CC_PORT_RX_DIV_CDIVR(1), div, cdiv,
				    NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(1),
				    NSS_CC_PORT_TX_DIV_CDIVR(1), div, cdiv,
				    NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(2),
				    NSS_CC_PORT_RX_DIV_CDIVR(2), div, cdiv,
				    NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(2),
				    NSS_CC_PORT_TX_DIV_CDIVR(2), div, cdiv,
				    NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(3),
				    NSS_CC_PORT_RX_DIV_CDIVR(3), div, cdiv,
				    NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(3),
				    NSS_CC_PORT_TX_DIV_CDIVR(3), div, cdiv,
				    NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT4_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(4),
				    NSS_CC_PORT_RX_DIV_CDIVR(4), div, cdiv,
				    NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT4_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(4),
				    NSS_CC_PORT_TX_DIV_CDIVR(4), div, cdiv,
				    NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT5_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;

		pclk = clk_get_parent(clk);
		if (!pclk) {
			ret = -ENODEV;
			break;
		}

		if (pclk->id == UNIPHY0_NSS_RX_CLK) {
			src = NSS_CC_PORT5_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK;
		} else if (pclk->id == UNIPHY1_NSS_RX_CLK) {
			src = NSS_CC_PORT5_RX_SRC_SEL_UNIPHY1_NSS_RX_CLK;
		} else {
			ret = -EINVAL;
			break;
		}
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(5),
				    NSS_CC_PORT_RX_DIV_CDIVR(5),
				    div, cdiv, src);
		break;
	case NSS_CC_PORT5_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;

		pclk = clk_get_parent(clk);
		if (!pclk) {
			ret = -ENODEV;
			break;
		}

		if (pclk->id == UNIPHY0_NSS_TX_CLK) {
			src = NSS_CC_PORT5_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK;
		} else if (pclk->id == UNIPHY1_NSS_TX_CLK) {
			src = NSS_CC_PORT5_TX_SRC_SEL_UNIPHY1_NSS_TX_CLK;
		} else {
			ret = -EINVAL;
			break;
		}
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(5),
				    NSS_CC_PORT_TX_DIV_CDIVR(5),
				    div, cdiv, src);
		break;
	case NSS_CC_PORT6_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_RX_CMD_RCGR(6),
				    NSS_CC_PORT_RX_DIV_CDIVR(6), div, cdiv,
				    NSS_CC_PORT6_RX_SRC_SEL_UNIPHY2_NSS_RX_CLK);
		break;
	case NSS_CC_PORT6_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, NSS_CC_PORT_TX_CMD_RCGR(6),
				    NSS_CC_PORT_TX_DIV_CDIVR(6), div, cdiv,
				    NSS_CC_PORT6_TX_SRC_SEL_UNIPHY2_NSS_TX_CLK);
		break;
	case GCC_USB0_MASTER_CLK:
		clk_rcg_set_rate(priv->base, GCC_USB0_MASTER_CMD_RCGR,
				 4, CFG_CLK_SRC_GPLL0);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_MOCK_UTMI_CMD_RCGR, 0, 0, 0,
				     CFG_CLK_SRC_CXO, 8);
		break;
	case GCC_USB0_AUX_CLK:
		clk_rcg_set_rate_mnd(priv->base, GCC_USB0_AUX_CMD_RCGR, 0, 0, 0,
				     CFG_CLK_SRC_CXO, 8);
		break;
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
		if (rate == CLK_125_MHZ)
			clk->rate = CLK_125_MHZ;
		else if (rate == CLK_312_5_MHZ)
			clk->rate = CLK_312_5_MHZ;
		else
			ret = -EINVAL;
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		src = CFG_CLK_SRC_GPLL0;
		switch (rate) {
		case IO_MACRO_CLK_24_MHZ:
			src = CFG_CLK_SRC_CXO;
			div = 0;
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
	default:
		return -EINVAL;
	}
	return rate;
}

static const struct gate_clk ipq9574_clks[] = {
	/*UART*/
	GATE_CLK(GCC_BLSP1_UART1_APPS_CLK,	0x02040, 0x00000001),
	GATE_CLK(GCC_BLSP1_UART2_APPS_CLK,	0x03040, 0x00000001),
	GATE_CLK(GCC_BLSP1_UART3_APPS_CLK,	0x04054, 0x00000001),
	GATE_CLK(GCC_BLSP1_UART4_APPS_CLK,	0x05040, 0x00000001),
	GATE_CLK(GCC_BLSP1_UART5_APPS_CLK,	0x06040, 0x00000001),
	GATE_CLK(GCC_BLSP1_UART6_APPS_CLK,	0x07040, 0x00000001),
	GATE_CLK(GCC_BLSP1_AHB_CLK,		0x01004, 0x00000001),
	/*MMC*/
	GATE_CLK(GCC_SDCC1_AHB_CLK,		0x33034, 0x00000001),
	GATE_CLK(GCC_SDCC1_APPS_CLK,		0x3302C, 0x00000001),
	/*ETHERNET*/
	GATE_CLK(GCC_MDIO_AHB_CLK,		0x17040, 0x00000001),
	GATE_CLK(GCC_MEM_NOC_NSSNOC_CLK,	0x19014, 0x00000001),
	GATE_CLK(GCC_NSSCFG_CLK,		0x1702C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_ATB_CLK,		0x17014, 0x00000001),
	GATE_CLK(GCC_NSSNOC_MEM_NOC_1_CLK,	0x17084, 0x00000001),
	GATE_CLK(GCC_NSSNOC_MEMNOC_CLK,		0x17024, 0x00000001),
	GATE_CLK(GCC_NSSNOC_QOSGEN_REF_CLK,	0x1701C, 0x00000001),
	GATE_CLK(GCC_NSSNOC_TIMEOUT_REF_CLK,	0x17020, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_AHB_CLK,	0x3A004, 0x00000001),
	GATE_CLK(GCC_CMN_12GPLL_SYS_CLK,	0x3A008, 0x00000001),
	GATE_CLK(GCC_UNIPHY0_SYS_CLK,		GCC_UNIPHY_SYS_CBCR(0), 0x00000001),
	GATE_CLK(GCC_UNIPHY0_AHB_CLK,		GCC_UNIPHY_AHB_CBCR(0), 0x00000001),
	GATE_CLK(GCC_UNIPHY1_SYS_CLK,		GCC_UNIPHY_SYS_CBCR(1), 0x00000001),
	GATE_CLK(GCC_UNIPHY1_AHB_CLK,		GCC_UNIPHY_AHB_CBCR(1), 0x00000001),
	GATE_CLK(GCC_UNIPHY2_SYS_CLK,		GCC_UNIPHY_SYS_CBCR(2), 0x00000001),
	GATE_CLK(GCC_UNIPHY2_AHB_CLK,		GCC_UNIPHY_AHB_CBCR(2), 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_CLK,		0x17028, 0x00000001),
	GATE_CLK(GCC_NSSNOC_SNOC_1_CLK,		0x1707C, 0x00000001),
	GATE_CLK(GCC_MEM_NOC_SNOC_AXI_CLK,	0x19018, 0x00000001),
	GATE_CLK(NSS_CC_NSS_CSR_CLK,		0x281D0, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_NSS_CSR_CLK,	0x281D4, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_PORT2_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_PORT3_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_PORT4_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_PORT5_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_PORT6_MAC_CLK,		NSS_CC_PORT_MAC_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_IPE_CLK,	0x2822C, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CLK,		0x28230, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_CFG_CLK,	0x28234, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CLK,		0x28238, 0x00000001),
	GATE_CLK(NSS_CC_PPE_EDMA_CFG_CLK,	0x2823C, 0x00000001),
	GATE_CLK(NSS_CC_CRYPTO_PPE_CLK,		0x28240, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CLK,		0x28244, 0x00000001),
	GATE_CLK(NSS_CC_NSSNOC_PPE_CFG_CLK,	0x28248, 0x00000001),
	GATE_CLK(NSS_CC_PPE_SWITCH_BTQ_CLK,	0x2827C, 0x00000001),
	GATE_CLK(NSS_CC_PORT1_RX_CLK,		NSS_CC_PORT_RX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_PORT1_TX_CLK,		NSS_CC_PORT_TX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_PORT2_RX_CLK,		NSS_CC_PORT_RX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_PORT2_TX_CLK,		NSS_CC_PORT_TX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_PORT3_RX_CLK,		NSS_CC_PORT_RX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_PORT3_TX_CLK,		NSS_CC_PORT_TX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_PORT4_RX_CLK,		NSS_CC_PORT_RX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_PORT4_TX_CLK,		NSS_CC_PORT_TX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_PORT5_RX_CLK,		NSS_CC_PORT_RX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_PORT5_TX_CLK,		NSS_CC_PORT_TX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_PORT6_RX_CLK,		NSS_CC_PORT_RX_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_PORT6_TX_CLK,		NSS_CC_PORT_TX_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT1_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(1), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT2_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(2), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT3_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(3), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT4_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(4), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT5_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT5_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(5), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT6_RX_CLK,	NSS_CC_UNIPHY_PORT_RX_CBCR(6), 0x00000001),
	GATE_CLK(NSS_CC_UNIPHY_PORT6_TX_CLK,	NSS_CC_UNIPHY_PORT_TX_CBCR(6), 0x00000001),
	GATE_CLK(GCC_BLSP1_QUP2_I2C_APPS_CLK,	BLSP1_QUP_I2C_APPS_CBCR(1), 0x00000001),
	GATE_CLK(GCC_BLSP1_QUP3_I2C_APPS_CLK,	BLSP1_QUP_I2C_APPS_CBCR(2), 0x00000001),
	GATE_CLK(GCC_BLSP1_QUP4_I2C_APPS_CLK,	BLSP1_QUP_I2C_APPS_CBCR(3), 0x00000001),
	GATE_CLK(GCC_BLSP1_QUP5_I2C_APPS_CLK,	BLSP1_QUP_I2C_APPS_CBCR(4), 0x00000001),
	GATE_CLK(GCC_PCIE0_AUX_CLK,		0x28034, 0x00000001),
	GATE_CLK(GCC_PCIE1_AUX_CLK,		0x29034, 0x00000001),
	GATE_CLK(GCC_PCIE2_AUX_CLK,		0x2A034, 0x00000001),
	GATE_CLK(GCC_PCIE3_AUX_CLK,		0x2B034, 0x00000001),
	GATE_CLK(GCC_PCIE0_AHB_CLK,		0x28030, 0x00000001),
	GATE_CLK(GCC_PCIE1_AHB_CLK,		0x29030, 0x00000001),
	GATE_CLK(GCC_PCIE2_AHB_CLK,		0x2A030, 0x00000001),
	GATE_CLK(GCC_PCIE3_AHB_CLK,		0x2B030, 0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_M_CLK,		0x28038, 0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_M_CLK,		0x29038, 0x00000001),
	GATE_CLK(GCC_PCIE2_AXI_M_CLK,		0x2A038, 0x00000001),
	GATE_CLK(GCC_PCIE3_AXI_M_CLK,		0x2B038, 0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_S_CLK,		0x2803C, 0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_S_CLK,		0x2903C, 0x00000001),
	GATE_CLK(GCC_PCIE2_AXI_S_CLK,		0x2A03C, 0x00000001),
	GATE_CLK(GCC_PCIE3_AXI_S_CLK,		0x2B03C, 0x00000001),
	GATE_CLK(GCC_PCIE0_AXI_S_BRIDGE_CLK,	0x28040, 0x00000001),
	GATE_CLK(GCC_PCIE1_AXI_S_BRIDGE_CLK,	0x29040, 0x00000001),
	GATE_CLK(GCC_PCIE2_AXI_S_BRIDGE_CLK,	0x2A040, 0x00000001),
	GATE_CLK(GCC_PCIE3_AXI_S_BRIDGE_CLK,	0x2B040, 0x00000001),
	GATE_CLK(GCC_PCIE0_PIPE_CLK,		0x28044, 0x00000001),
	GATE_CLK(GCC_PCIE1_PIPE_CLK,		0x29044, 0x00000001),
	GATE_CLK(GCC_PCIE2_PIPE_CLK,		0x2A044, 0x00000001),
	GATE_CLK(GCC_PCIE3_PIPE_CLK,		0x2B044, 0x00000001),
	GATE_CLK(GCC_SNOC_PCIE0_1LANE_S_CLK,	0x2E048, 0x00000001),
	GATE_CLK(GCC_SNOC_PCIE1_1LANE_S_CLK,	0x2E04C, 0x00000001),
	GATE_CLK(GCC_SNOC_PCIE2_2LANE_S_CLK,	0x2E050, 0x00000001),
	GATE_CLK(GCC_SNOC_PCIE3_2LANE_S_CLK,	0x2E054, 0x00000001),
	GATE_CLK(GCC_ANOC_PCIE0_1LANE_M_CLK,	0x2E07C, 0x00000001),
	GATE_CLK(GCC_ANOC_PCIE1_1LANE_M_CLK,	0x2E08C, 0x00000001),
	GATE_CLK(GCC_ANOC_PCIE2_2LANE_M_CLK,	0x2E080, 0x00000001),
	GATE_CLK(GCC_ANOC_PCIE3_2LANE_M_CLK,	0x2E090, 0x00000001),
	GATE_CLK(GCC_QPIC_IO_MACRO_CLK,		0x3200C, 0x00000001),
	GATE_CLK(GCC_USB0_MASTER_CLK,		0x2C044, 0x00000001),
	GATE_CLK(GCC_USB0_MOCK_UTMI_CLK,	0x2C04C, 0x00000001),
	GATE_CLK(GCC_USB0_AUX_CLK,		0x2C048, 0x00000001),
	GATE_CLK(GCC_USB0_PIPE_CLK,		0x2C054, 0x00000001),
	GATE_CLK(GCC_USB0_SLEEP_CLK,		0x2C058, 0x00000001),
	GATE_CLK(GCC_USB0_PHY_CFG_AHB_CLK,	0x2C05C, 0x00000001),
	GATE_CLK(GCC_SNOC_USB_CLK,		0x2E058, 0x00000001),
	GATE_CLK(GCC_ANOC_USB_AXI_CLK,		0x2E084, 0x00000001),
};

static int ipq9574_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	if (priv->data->num_clks <= clk->id) {
		debug("%s: unknown clk id %lu\n", __func__, clk->id);
		return 0;
	}

	debug("%s: clk %s\n", __func__, ipq9574_clks[clk->id].name);

	qcom_gate_clk_en(priv, clk->id);

	return 0;
}

static const struct qcom_reset_map ipq9574_gcc_resets[] = {
	[GCC_SDCC_BCR]				= {0x33000, 0},
	[GCC_UNIPHY0_SOFT_RESET]		= {0x17050, 0},
	[GCC_UNIPHY1_SOFT_RESET]		= {0x17060, 0},
	[GCC_UNIPHY2_SOFT_RESET]		= {0x17070, 0},
	[GCC_UNIPHY0_XPCS_RESET]		= {0x17050, 2},
	[GCC_UNIPHY1_XPCS_RESET]		= {0x17060, 2},
	[GCC_UNIPHY2_XPCS_RESET]		= {0x17070, 2},
	[NSS_CC_PPE_CFG_RESET]			= {0x28A08, 17},
	[NSS_CC_PPE_EDMA_RESET]			= {0x28A08, 16},
	[NSS_CC_PORT1_MAC_RESET]		= {0x28A08, 11},
	[NSS_CC_PORT2_MAC_RESET]		= {0x28A08, 10},
	[NSS_CC_PORT3_MAC_RESET]		= {0x28A08, 9},
	[NSS_CC_PORT4_MAC_RESET]		= {0x28A08, 8},
	[NSS_CC_PORT5_MAC_RESET]		= {0x28A08, 7},
	[NSS_CC_PORT6_MAC_RESET]		= {0x28A08, 6},
	[NSS_CC_UNIPHY_PORT1_RX_RESET]		= {0x28A24, 23},
	[NSS_CC_UNIPHY_PORT1_TX_RESET]		= {0x28A24, 22},
	[NSS_CC_UNIPHY_PORT2_RX_RESET]		= {0x28A24, 21},
	[NSS_CC_UNIPHY_PORT2_TX_RESET]		= {0x28A24, 20},
	[NSS_CC_UNIPHY_PORT3_RX_RESET]		= {0x28A24, 19},
	[NSS_CC_UNIPHY_PORT3_TX_RESET]		= {0x28A24, 18},
	[NSS_CC_UNIPHY_PORT4_RX_RESET]		= {0x28A24, 17},
	[NSS_CC_UNIPHY_PORT4_TX_RESET]		= {0x28A24, 16},
	[NSS_CC_UNIPHY_PORT5_RX_RESET]		= {0x28A24, 15},
	[NSS_CC_UNIPHY_PORT5_TX_RESET]		= {0x28A24, 14},
	[NSS_CC_UNIPHY_PORT6_RX_RESET]		= {0x28A24, 13},
	[NSS_CC_UNIPHY_PORT6_TX_RESET]		= {0x28A24, 12},
	[NSS_CC_PORT1_RX_RESET]			= {0x28A24, 11},
	[NSS_CC_PORT1_TX_RESET]			= {0x28A24, 10},
	[NSS_CC_PORT2_RX_RESET]			= {0x28A24, 9},
	[NSS_CC_PORT2_TX_RESET]			= {0x28A24, 8},
	[NSS_CC_PORT3_RX_RESET]			= {0x28A24, 7},
	[NSS_CC_PORT3_TX_RESET]			= {0x28A24, 6},
	[NSS_CC_PORT4_RX_RESET]			= {0x28A24, 5},
	[NSS_CC_PORT4_TX_RESET]			= {0x28A24, 4},
	[NSS_CC_PORT5_RX_RESET]			= {0x28A24, 3},
	[NSS_CC_PORT5_TX_RESET]			= {0x28A24, 2},
	[NSS_CC_PORT6_RX_RESET]			= {0x28A24, 1},
	[NSS_CC_PORT6_TX_RESET]			= {0x28A24, 0},
	[GCC_PCIE0PHY_PHY_BCR]			= {0x2805c, 0},
	[GCC_PCIE0_AHB_ARES]			= {0x28058, 7},
	[GCC_PCIE0_AUX_ARES]			= {0x28058, 6},
	[GCC_PCIE0_AXI_M_ARES]			= {0x28058, 5},
	[GCC_PCIE0_AXI_M_STICKY_ARES]		= {0x28058, 4},
	[GCC_PCIE0_AXI_S_ARES]			= {0x28058, 3},
	[GCC_PCIE0_AXI_S_STICKY_ARES]		= {0x28058, 2},
	[GCC_PCIE0_CORE_STICKY_ARES]		= {0x28058, 1},
	[GCC_PCIE0_PIPE_ARES]			= {0x28058, 0},
	[GCC_PCIE1_AHB_ARES]			= {0x29058, 7},
	[GCC_PCIE1_AUX_ARES]			= {0x29058, 6},
	[GCC_PCIE1_AXI_M_ARES]			= {0x29058, 5},
	[GCC_PCIE1_AXI_M_STICKY_ARES]		= {0x29058, 4},
	[GCC_PCIE1_AXI_S_ARES]			= {0x29058, 3},
	[GCC_PCIE1_AXI_S_STICKY_ARES]		= {0x29058, 2},
	[GCC_PCIE1_CORE_STICKY_ARES]		= {0x29058, 1},
	[GCC_PCIE1_PIPE_ARES]			= {0x29058, 0},
	[GCC_PCIE2_AHB_ARES]			= {0x2a058, 7},
	[GCC_PCIE2_AUX_ARES]			= {0x2a058, 6},
	[GCC_PCIE2_AXI_M_ARES]			= {0x2a058, 5},
	[GCC_PCIE2_AXI_M_STICKY_ARES]		= {0x2a058, 4},
	[GCC_PCIE2_AXI_S_ARES]			= {0x2a058, 3},
	[GCC_PCIE2_AXI_S_STICKY_ARES]		= {0x2a058, 2},
	[GCC_PCIE2_CORE_STICKY_ARES]		= {0x2a058, 1},
	[GCC_PCIE2_PIPE_ARES]			= {0x2a058, 0},
	[GCC_PCIE3_AHB_ARES]			= {0x2b058, 7},
	[GCC_PCIE3_AUX_ARES]			= {0x2b058, 6},
	[GCC_PCIE3_AXI_M_ARES]			= {0x2b058, 5},
	[GCC_PCIE3_AXI_M_STICKY_ARES]		= {0x2b058, 4},
	[GCC_PCIE3_AXI_S_ARES]			= {0x2b058, 3},
	[GCC_PCIE3_AXI_S_STICKY_ARES]		= {0x2b058, 2},
	[GCC_PCIE3_CORE_STICKY_ARES]		= {0x2b058, 1},
	[GCC_PCIE3_PIPE_ARES]			= {0x2b058, 0},
	[GCC_PCIE0_BCR]				= {0x28000, 0},
	[GCC_PCIE0_LINK_DOWN_BCR]		= {0x28054, 0},
	[GCC_PCIE0_PHY_BCR]			= {0x28060, 0},
	[GCC_PCIE1_BCR]				= {0x29000, 0},
	[GCC_PCIE1_LINK_DOWN_BCR]		= {0x29054, 0},
	[GCC_PCIE1_PHY_BCR]			= {0x29060, 0},
	[GCC_PCIE1PHY_PHY_BCR]			= {0x2905c, 0},
	[GCC_PCIE2_BCR]				= {0x2a000, 0},
	[GCC_PCIE2_LINK_DOWN_BCR]		= {0x2a054, 0},
	[GCC_PCIE2_PHY_BCR]			= {0x2a060, 0},
	[GCC_PCIE2PHY_PHY_BCR]			= {0x2a05c, 0},
	[GCC_PCIE3_BCR]				= {0x2b000, 0},
	[GCC_PCIE3_LINK_DOWN_BCR]		= {0x2b054, 0},
	[GCC_PCIE3PHY_PHY_BCR]			= {0x2b05c, 0},
	[GCC_PCIE3_PHY_BCR]			= {0x2b060, 0},
	[GCC_USB_BCR]				= {0x2C000, 0},
	[GCC_QUSB2_0_PHY_BCR]			= {0x2C068, 0},
	[GCC_USB0_PHY_BCR]			= {0x2C06C, 0},
	[GCC_USB3PHY_0_PHY_BCR]			= {0x2C070, 0},
};

static struct msm_clk_data ipq9574_gcc_data = {
	.resets = ipq9574_gcc_resets,
	.num_resets = ARRAY_SIZE(ipq9574_gcc_resets),
	.clks = ipq9574_clks,
	.num_clks = ARRAY_SIZE(ipq9574_clks),
	.enable = ipq9574_enable,
	.set_rate = ipq9574_set_rate,
};

static const struct udevice_id gcc_ipq9574_of_match[] = {
	{
		.compatible = "qcom,ipq9574-gcc",
		.data = (ulong)&ipq9574_gcc_data,
	},
	{ }
};

U_BOOT_DRIVER(gcc_ipq9574) = {
	.name		= "gcc_ipq9574",
	.id		= UCLASS_NOP,
	.of_match	= gcc_ipq9574_of_match,
	.bind		= qcom_cc_bind,
	.flags		= DM_FLAG_PRE_RELOC | DM_FLAG_DEFAULT_PD_CTRL_OFF,
};
