// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock drivers for QTI IPQ5424
 *
 * (C) Copyright 2022 Sumit Garg <sumit.garg@linaro.org>
 *
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <dm/device-internal.h>
#include "clock-snapdragon.h"

#include <dt-bindings/clock/gcc-ipq5424.h>

/*UNIPHY status register*/
#define SOFTSKU_STATUS_6			0xA6264
#define SOFTSKU_STATUS_7			0xA626C
#define SOFTSKU_STATUS_8			0xA6274

#define UNIPHY_STAT_BIT				0

#define MHZ(X)	((X) * 1000000UL)
static const struct bcr_regs gcc_qupv3_uart0_regs = {
	.cfg_rcgr = GCC_QUPV3_UART0_CFG_RCGR,
	.cmd_rcgr = GCC_QUPV3_UART0_CMD_RCGR,
	.M = GCC_QUPV3_UART0_M,
	.N = GCC_QUPV3_UART0_N,
	.D = GCC_QUPV3_UART0_D,
};

static const struct bcr_regs gcc_qupv3_uart1_regs = {
	.cfg_rcgr = GCC_QUPV3_UART1_CFG_RCGR,
	.cmd_rcgr = GCC_QUPV3_UART1_CMD_RCGR,
	.M = GCC_QUPV3_UART1_M,
	.N = GCC_QUPV3_UART1_N,
	.D = GCC_QUPV3_UART1_D,
};

static const struct bcr_regs gcc_qupv3_spi0_regs = {
	.cfg_rcgr = GCC_QUPV3_SPI0_CFG_RCGR,
	.cmd_rcgr = GCC_QUPV3_SPI0_CMD_RCGR,
	.M = GCC_QUPV3_SPI0_M,
	.N = GCC_QUPV3_SPI0_N,
	.D = GCC_QUPV3_SPI0_D,
};

static const struct bcr_regs gcc_qupv3_spi1_regs = {
	.cfg_rcgr = GCC_QUPV3_SPI1_CFG_RCGR,
	.cmd_rcgr = GCC_QUPV3_SPI1_CMD_RCGR,
	.M = GCC_QUPV3_SPI1_M,
	.N = GCC_QUPV3_SPI1_N,
	.D = GCC_QUPV3_SPI1_D,
};

static const struct bcr_regs_v2 gcc_qupv3_i2c0_regs = {
	.cfg_rcgr = GCC_QUPV3_I2C0_CFG_RCGR,
	.cmd_rcgr = GCC_QUPV3_I2C0_CMD_RCGR,
	.div_cdivr = GCC_QUPV3_I2C0_DIV_CDIVR,
};

static const struct bcr_regs_v2 gcc_qupv3_i2c1_regs = {
	.cfg_rcgr = GCC_QUPV3_I2C1_CFG_RCGR,
	.cmd_rcgr = GCC_QUPV3_I2C1_CMD_RCGR,
	.div_cdivr = GCC_QUPV3_I2C1_DIV_CDIVR,
};

static const struct bcr_regs_v2 gcc_pcnoc_bfdcd_regs = {
	.cfg_rcgr = GCC_PCNOC_BFDCD_CFG_RCGR,
	.cmd_rcgr = GCC_PCNOC_BFDCD_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_system_noc_bfdcd_regs = {
	.cfg_rcgr = GCC_SYSTEM_NOC_BFDCD_CFG_RCGR,
	.cmd_rcgr = GCC_SYSTEM_NOC_BFDCD_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_nssnoc_memnoc_bfdcd_regs = {
	.cfg_rcgr = GCC_NSSNOC_MEMNOC_BFDCD_CFG_RCGR,
	.cmd_rcgr = GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR,
};

static const struct bcr_regs_v2 nss_cc_ppe_regs = {
	.cfg_rcgr = NSS_CC_PPE_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PPE_CMD_RCGR,
};

static const struct bcr_regs_v2 nss_cc_ce_regs = {
	.cfg_rcgr = NSS_CC_CE_CFG_RCGR,
	.cmd_rcgr = NSS_CC_CE_CMD_RCGR,
};

static const struct bcr_regs_v2 nss_cc_cfg_regs = {
	.cfg_rcgr = NSS_CC_CFG_CFG_RCGR,
	.cmd_rcgr = NSS_CC_CFG_CMD_RCGR,
};

static const struct bcr_regs pcie_aux_regs = {
	.cfg_rcgr = GCC_PCIE_AUX_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE_AUX_CMD_RCGR,
	.M = GCC_PCIE_AUX_M,
	.N = GCC_PCIE_AUX_N,
	.D = GCC_PCIE_AUX_D,
};

static const struct bcr_regs_v2 pcie0_axi_m_clk_regs = {
	.cfg_rcgr = GCC_PCIE0_AXI_M_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE0_AXI_M_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie1_axi_m_clk_regs = {
	.cfg_rcgr = GCC_PCIE1_AXI_M_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE1_AXI_M_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie2_axi_m_clk_regs = {
	.cfg_rcgr = GCC_PCIE2_AXI_M_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE2_AXI_M_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie3_axi_m_clk_regs = {
	.cfg_rcgr = GCC_PCIE3_AXI_M_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE3_AXI_M_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie0_axi_s_clk_regs = {
	.cfg_rcgr = GCC_PCIE0_AXI_S_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE0_AXI_S_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie1_axi_s_clk_regs = {
	.cfg_rcgr = GCC_PCIE1_AXI_S_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE1_AXI_S_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie2_axi_s_clk_regs = {
	.cfg_rcgr = GCC_PCIE2_AXI_S_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE2_AXI_S_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie3_axi_s_clk_regs = {
	.cfg_rcgr = GCC_PCIE3_AXI_S_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE3_AXI_S_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie0_rchng_clk_regs = {
	.cfg_rcgr = GCC_PCIE0_RCHNG_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE0_RCHNG_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie1_rchng_clk_regs = {
	.cfg_rcgr = GCC_PCIE1_RCHNG_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE1_RCHNG_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie2_rchng_clk_regs = {
	.cfg_rcgr = GCC_PCIE2_RCHNG_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE2_RCHNG_CMD_RCGR,
};

static const struct bcr_regs_v2 pcie3_rchng_clk_regs = {
	.cfg_rcgr = GCC_PCIE3_RCHNG_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE3_RCHNG_CMD_RCGR,
};

static const struct bcr_regs sdc_regs = {
	.cfg_rcgr = SDCC1_APPS_CFG_RCGR,
	.cmd_rcgr = SDCC1_APPS_CMD_RCGR,
	.M = SDCC1_APPS_M,
	.N = SDCC1_APPS_N,
	.D = SDCC1_APPS_D,
};

static const struct bcr_regs gcc_usb0_master_clk_regs = {
	.cfg_rcgr = GCC_USB0_MASTER_CFG_RCGR,
	.cmd_rcgr = GCC_USB0_MASTER_CMD_RCGR,
	.M = GCC_USB0_MASTER_M,
	.N = GCC_USB0_MASTER_N,
	.D = GCC_USB0_MASTER_D
};

static const struct bcr_regs gcc_usb0_aux_clk_regs = {
	.cfg_rcgr = GCC_USB0_AUX_CFG_RCGR,
	.cmd_rcgr = GCC_USB0_AUX_CMD_RCGR,
	.M = GCC_USB0_AUX_M,
	.N = GCC_USB0_AUX_N,
	.D = GCC_USB0_AUX_D,
};

static const struct bcr_regs gcc_usb0_mock_utmi_clk_regs = {
	.cfg_rcgr = GCC_USB0_MOCK_UTMI_CFG_RCGR,
	.cmd_rcgr = GCC_USB0_MOCK_UTMI_CMD_RCGR,
	.M = GCC_USB0_MOCK_UTMI_M,
	.N = GCC_USB0_MOCK_UTMI_N,
	.D = GCC_USB0_MOCK_UTMI_D,
};

static const struct bcr_regs gcc_usb1_mock_utmi_clk_regs = {
	.cfg_rcgr = GCC_USB1_MOCK_UTMI_CFG_RCGR,
	.cmd_rcgr = GCC_USB1_MOCK_UTMI_CMD_RCGR,
	.M = GCC_USB1_MOCK_UTMI_M,
	.N = GCC_USB1_MOCK_UTMI_N,
	.D = GCC_USB1_MOCK_UTMI_D,
};

static const struct bcr_regs_v2 gcc_qpic_io_macro_regs = {
	.cfg_rcgr = GCC_QPIC_IO_MACRO_CFG_RCGR,
	.cmd_rcgr = GCC_QPIC_IO_MACRO_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_qpic_regs = {
	.cfg_rcgr = GCC_QPIC_CFG_RCGR,
	.cmd_rcgr = GCC_QPIC_CMD_RCGR,
};

static const struct bcr_regs_v2 nss_cc_port1_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT1_RX_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PORT1_RX_CMD_RCGR,
	.div_cdivr = NSS_CC_PORT1_RX_DIV_CDIVR,
};

static const struct bcr_regs_v2 nss_cc_port1_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT1_TX_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PORT1_TX_CMD_RCGR,
	.div_cdivr = NSS_CC_PORT1_TX_DIV_CDIVR,
};

static const struct bcr_regs_v2 nss_cc_port2_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT2_RX_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PORT2_RX_CMD_RCGR,
	.div_cdivr = NSS_CC_PORT2_RX_DIV_CDIVR,
};

static const struct bcr_regs_v2 nss_cc_port2_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT2_TX_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PORT2_TX_CMD_RCGR,
	.div_cdivr = NSS_CC_PORT2_TX_DIV_CDIVR,
};

static const struct bcr_regs_v2 nss_cc_port3_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT3_RX_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PORT3_RX_CMD_RCGR,
	.div_cdivr = NSS_CC_PORT3_RX_DIV_CDIVR,
};

static const struct bcr_regs_v2 nss_cc_port3_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT3_TX_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PORT3_TX_CMD_RCGR,
	.div_cdivr = NSS_CC_PORT3_TX_DIV_CDIVR,
};

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
		case CLK_2_5_MHZ:
			break;
		case CLK_12_5_MHZ:
			*div = 9;
			*cdiv = 4;
			break;
		case CLK_25_MHZ:
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
	} else
		return -EINVAL;

	return 0;
}

int msm_set_parent(struct clk *clk, struct clk* parent)
{
	assert(clk);
	assert(parent);
	clk->dev->parent = parent->dev;
	dev_set_uclass_priv(parent->dev, parent);
	return 0;
}

ulong msm_get_rate(struct clk *clk)
{
	return (ulong)clk->rate;
}

ulong msm_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret, src, div = 0, cdiv = 0;

	switch (clk->id) {
	case GCC_QUPV3_SE0_CLK:
		/* Default: 1.8432MHz */
		clk_rcg_set_rate_mnd(priv->base, &gcc_qupv3_uart0_regs,
				0, 36, 15625, QUPV3_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QUPV3_SE1_CLK:
		/* Default: 1.8432MHz */
		clk_rcg_set_rate_mnd(priv->base, &gcc_qupv3_uart1_regs,
				0, 36, 15625, QUPV3_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QUPV3_SE2_CLK:
		/* Default: 64MHz */
		clk_rcg_set_rate_v2(priv->base, &gcc_qupv3_i2c0_regs, 24, 1,
				QUPV3_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QUPV3_SE3_CLK:
		/* Default: 64MHz */
		clk_rcg_set_rate_v2(priv->base, &gcc_qupv3_i2c1_regs, 24, 1,
				QUPV3_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QUPV3_SE4_CLK:
		/* Default: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &gcc_qupv3_spi0_regs, 16, 0,
				0, QUPV3_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QUPV3_SE5_CLK:
		switch(rate) {
		case MHZ(32):
			clk_rcg_set_rate_mnd(priv->base, &gcc_qupv3_spi1_regs,
				24, 0, 0, QUPV3_SRC_SEL_GPLL0_OUT_MAIN_DIV);
			break;
		default:
			/* Default: 50MHz */
			clk_rcg_set_rate_mnd(priv->base, &gcc_qupv3_spi1_regs,
				16, 0, 0, QUPV3_SRC_SEL_GPLL0_OUT_MAIN);
		}
		break;
	case GCC_USB0_MASTER_CLK:
		/* Default: 200MHz */
		clk_rcg_set_rate_mnd(priv->base, &gcc_usb0_master_clk_regs, 4,
				0, 0, USB0_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		/* Default: 60MHz */
		writel(1, priv->base + GCC_USB0_MOCK_UTMI_DIV_CDIVR);
		clk_rcg_set_rate_mnd(priv->base, &gcc_usb0_mock_utmi_clk_regs,
				10, 0, 0, USB0_SRC_SEL_GPLL4_OUT_AUX);
		break;
	case GCC_USB0_AUX_CLK:
		/* Default: 24MHz */
		clk_rcg_set_rate_mnd(priv->base, &gcc_usb0_aux_clk_regs, 1,
				0, 0, USB0_SRC_SEL_XO);
		break;
	case GCC_USB1_MOCK_UTMI_CLK:
		/* Default: 60MHz */
		writel(1, priv->base + GCC_USB1_MOCK_UTMI_DIV_CDIVR);
		clk_rcg_set_rate_mnd(priv->base, &gcc_usb1_mock_utmi_clk_regs,
				10, 0, 0, USB0_SRC_SEL_GPLL4_OUT_AUX);
		break;
	case GCC_SDCC1_APPS_CLK:
		/* SDCC1: 192 MHz */
		clk_rcg_set_rate_mnd(priv->base, &sdc_regs, 3, 0, 0,
				     SDCC1_SRC_SEL_GPLL2_OUT_MAIN);
		break;
	case GCC_PCIE_AUX_CLK:
		/* GCC_PCIE_AUX_CLK: 20 MHz */
		clk_rcg_set_rate_mnd(priv->base, &pcie_aux_regs,
					16, 2, 5, PCIE_GPLL0_OUT_AUX);
		break;
	case GCC_PCIE0_AXI_M_CLK:
		/* GCC_PCIE0_AXI_M_CLK: 240 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie0_axi_m_clk_regs,
					9, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_M_CLK:
		/* GCC_PCIE1_AXI_M_CLK: 240 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie1_axi_m_clk_regs,
					9, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE2_AXI_M_CLK:
		/* GCC_PCIE2_AXI_M_CLK: 266.67 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie2_axi_m_clk_regs,
					8, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE3_AXI_M_CLK:
		/* GCC_PCIE3_AXI_M_CLK: 266.67 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie3_axi_m_clk_regs,
					8, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_AXI_S_CLK:
		/* GCC_PCIE0_AXI_S_CLK: 240 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie0_axi_s_clk_regs,
					9, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_S_CLK:
		/* GCC_PCIE1_AXI_S_CLK: 240 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie1_axi_s_clk_regs,
					9, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE2_AXI_S_CLK:
		/* GCC_PCIE2_AXI_S_CLK: 240 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie2_axi_s_clk_regs,
					9, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE3_AXI_S_CLK:
		/* GCC_PCIE3_AXI_S_CLK: 240 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie3_axi_s_clk_regs,
					9, 0, PCIE_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_RCHNG_CLK:
		/* GCC_PCIE0_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie0_rchng_clk_regs,
					15, 0, PCIE_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE1_RCHNG_CLK:
		/* GCC_PCIE1_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie1_rchng_clk_regs,
					15, 0, PCIE_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE2_RCHNG_CLK:
		/* GCC_PCIE2_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie2_rchng_clk_regs,
					15, 0, PCIE_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE3_RCHNG_CLK:
		/* GCC_PCIE3_RCHNG_CLK: 100 MHz */
		clk_rcg_set_rate_v2(priv->base, &pcie3_rchng_clk_regs,
					15, 0, PCIE_GPLL0_OUT_MAIN);
		break;
	case GCC_PCNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_pcnoc_bfdcd_regs,
				15, 0, PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_system_noc_bfdcd_regs,
				8, 0, SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_NSSNOC_MEMNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_nssnoc_memnoc_bfdcd_regs,
				1, 0, NSSNOC_MEMNOC_BFDCD_SRC_SEL_NSS_CMN_CLK);
		break;

	/* NSS clocks */
	case NSS_CC_PPE_CLK:
		clk_rcg_set_rate_v2(priv->base, &nss_cc_ppe_regs,
				1, 0, NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_375M);
		break;
	case NSS_CC_CE_CLK:
		clk_rcg_set_rate_v2(priv->base, &nss_cc_ce_regs,
				1, 0, NSS_CC_PPE_SRC_SEL_CMN_PLL_NSS_CLK_375M);
		break;
	case NSS_CC_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, &nss_cc_cfg_regs,
				15, 0, NSS_CC_PPE_SRC_SEL_GCC_GPLL0_OUT_AUX);
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port1_rx_regs,
				div, cdiv,
				NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port1_tx_regs,
				div, cdiv,
				NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port2_rx_regs,
				div, cdiv,
				NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port2_tx_regs,
				div, cdiv,
				NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;
	case NSS_CC_PORT3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port3_rx_regs,
				div, cdiv,
				NSS_CC_PORT_RX_SRC_SEL_UNIPHY_NSS_RX_CLK);
		break;
	case NSS_CC_PORT3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port3_tx_regs,
				div, cdiv,
				NSS_CC_PORT_TX_SRC_SEL_UNIPHY_NSS_TX_CLK);
		break;

	case UNIPHY0_NSS_RX_CLK:
	case UNIPHY0_NSS_TX_CLK:
	case UNIPHY1_NSS_RX_CLK:
	case UNIPHY1_NSS_TX_CLK:
	case UNIPHY2_NSS_RX_CLK:
	case UNIPHY2_NSS_TX_CLK:
		if (rate == CLK_125_MHZ)
			clk->rate = CLK_125_MHZ;
		else if (rate == CLK_312_5_MHZ)
			clk->rate = CLK_312_5_MHZ;
		else
			ret = -EINVAL;
		break;
	case GCC_QPIC_CLK:
		/* GCC_QPIC_CLK: 100 MHz  */
		clk_rcg_set_rate_v2(priv->base, &gcc_qpic_regs,
				0xF, 0,
				GCC_QPIC_IO_MACRO_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		src = GCC_QPIC_IO_MACRO_SRC_SEL_GPLL0_OUT_MAIN;
		switch (rate) {
		case IO_MACRO_CLK_24_MHZ:
			src = GCC_QPIC_IO_MACRO_SRC_SEL_XO_CLK;
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
		clk_rcg_set_rate_v2(priv->base, &gcc_qpic_io_macro_regs,
				div, cdiv, src);
		break;
	default:
		ret = 0;
	}

	return 0;
}

int msm_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_QUPV3_SE0_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_UART0_CBCR);
		break;
	case GCC_QUPV3_SE1_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_UART1_CBCR);
		break;
	case GCC_QUPV3_SE2_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_I2C0_CBCR);
		break;
	case GCC_QUPV3_SE3_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_I2C1_CBCR);
		break;
	case GCC_QUPV3_SE4_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_SPI0_CBCR);
		break;
	case GCC_QUPV3_SE5_CLK:
		clk_enable_cbc(priv->base + GCC_QUPV3_SPI1_CBCR);
		break;
	case GCC_NSSCFG_CLK:
		clk_enable_cbc(priv->base + GCC_NSSCFG_CBCR);
		break;
	case GCC_NSSNOC_MEMNOC_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_MEMNOC_CBCR);
		break;
	case GCC_NSSNOC_MEMNOC_1_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_MEMNOC_1_CBCR);
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_APPS_CBCR);
		break;
	case GCC_SDCC1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_AHB_CBCR);
		break;
	case GCC_USB0_MASTER_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_MASTER_CBCR);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_MOCK_UTMI_CBCR);
		break;
	case GCC_USB0_SLEEP_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_SLEEP_CBCR);
		break;
	case GCC_USB0_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_AUX_CBCR);
		break;
	case GCC_USB0_PHY_CFG_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_PHY_CFG_AHB_CBCR);
		break;
	case GCC_USB1_MASTER_CLK:
		clk_enable_cbc(priv->base + GCC_USB1_MASTER_CBCR);
		break;
	case GCC_USB1_MOCK_UTMI_CLK:
		clk_enable_cbc(priv->base + GCC_USB1_MOCK_UTMI_CBCR);
		break;
	case GCC_USB1_SLEEP_CLK:
		clk_enable_cbc(priv->base + GCC_USB1_SLEEP_CBCR);
		break;
	case GCC_USB1_PHY_CFG_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_USB1_PHY_CFG_AHB_CBCR);
		break;
	case GCC_USB0_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_PIPE_CBCR);
		break;
	case GCC_CNOC_USB_CLK:
		clk_enable_cbc(priv->base + GCC_CNOC_USB_CBCR);
		break;
	case GCC_PCIE0_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE0_AHB_CBCR);
		break;
	case GCC_PCIE0_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE0_AUX_CBCR);
		break;
	case GCC_PCIE0_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE0_AXI_M_CBCR);
		break;
	case GCC_PCIE0_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE0_AXI_S_BRIDGE_CBCR);
		break;
	case GCC_PCIE0_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE0_AXI_S_CBCR);
		break;
	case GCC_PCIE0_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE0_PIPE_CBCR);
		break;
	case GCC_PCIE1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE1_AHB_CBCR);
		break;
	case GCC_PCIE1_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE1_AUX_CBCR);
		break;
	case GCC_PCIE1_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE1_AXI_M_CBCR);
		break;
	case GCC_PCIE1_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE1_AXI_S_BRIDGE_CBCR);
		break;
	case GCC_PCIE1_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE1_AXI_S_CBCR);
		break;
	case GCC_PCIE1_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE1_PIPE_CBCR);
		break;
	case GCC_PCIE2_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE2_AHB_CBCR);
		break;
	case GCC_PCIE2_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE2_AUX_CBCR);
		break;
	case GCC_PCIE2_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE2_AXI_M_CBCR);
		break;
	case GCC_PCIE2_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE2_AXI_S_BRIDGE_CBCR);
		break;
	case GCC_PCIE2_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE2_AXI_S_CBCR);
		break;
	case GCC_PCIE2_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE2_PIPE_CBCR);
		break;
	case GCC_PCIE3_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE3_AHB_CBCR);
		break;
	case GCC_PCIE3_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE3_AUX_CBCR);
		break;
	case GCC_PCIE3_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE3_AXI_M_CBCR);
		break;
	case GCC_PCIE3_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE3_AXI_S_BRIDGE_CBCR);
		break;
	case GCC_PCIE3_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE3_AXI_S_CBCR);
		break;
	case GCC_PCIE3_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE3_PIPE_CBCR);
		break;
	case GCC_CNOC_PCIE0_1LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_CNOC_PCIE0_1LANE_S_CBCR);
		break;
	case GCC_CNOC_PCIE1_1LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_CNOC_PCIE1_1LANE_S_CBCR);
		break;
	case GCC_CNOC_PCIE2_2LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_CNOC_PCIE2_2LANE_S_CBCR);
		break;
	case GCC_CNOC_PCIE3_2LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_CNOC_PCIE3_2LANE_S_CBCR);
		break;
	case GCC_ANOC_PCIE0_1LANE_M_CLK:
		clk_enable_cbc(priv->base + GCC_ANOC_PCIE0_1LANE_M_CBCR);
		break;
	case GCC_ANOC_PCIE1_1LANE_M_CLK:
		clk_enable_cbc(priv->base + GCC_ANOC_PCIE1_1LANE_M_CBCR);
		break;
	case GCC_ANOC_PCIE2_2LANE_M_CLK:
		clk_enable_cbc(priv->base + GCC_ANOC_PCIE2_2LANE_M_CBCR);
		break;
	case GCC_ANOC_PCIE3_2LANE_M_CLK:
		clk_enable_cbc(priv->base + GCC_ANOC_PCIE3_2LANE_M_CBCR);
		break;
	case GCC_IM_SLEEP_CLK:
		clk_enable_cbc(priv->base + GCC_IM_SLEEP_CBCR);
		break;
	case GCC_CMN_12GPLL_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_CMN_12GPLL_AHB_CBCR);
		break;
	case GCC_CMN_12GPLL_SYS_CLK:
		clk_enable_cbc(priv->base + GCC_CMN_12GPLL_SYS_CBCR);
		break;
	case GCC_NSSCC_CLK:
		clk_enable_cbc(priv->base + GCC_NSSCC_CBCR);
		break;
	case GCC_NSSNOC_NSSCC_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_NSSCC_CBCR);
		break;
	case GCC_NSSNOC_SNOC_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_SNOC_CBCR);
		break;
	case GCC_NSSNOC_SNOC_1_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_SNOC_1_CBCR);
		break;
	case GCC_UNIPHY0_SYS_CLK:
		if(readl(SOFTSKU_STATUS_6) & BIT(UNIPHY_STAT_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY0_SYS_CBCR);
		break;
	case GCC_UNIPHY1_SYS_CLK:
		if(readl(SOFTSKU_STATUS_7) & BIT(UNIPHY_STAT_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY1_SYS_CBCR);
		break;
	case GCC_UNIPHY2_SYS_CLK:
		if(readl(SOFTSKU_STATUS_8) & BIT(UNIPHY_STAT_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY2_SYS_CBCR);
		break;
	case GCC_UNIPHY0_AHB_CLK:
		if(readl(SOFTSKU_STATUS_6) & BIT(UNIPHY_STAT_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY0_AHB_CBCR);
		break;
	case GCC_UNIPHY1_AHB_CLK:
		if(readl(SOFTSKU_STATUS_7) & BIT(UNIPHY_STAT_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY1_AHB_CBCR);
		break;
	case GCC_UNIPHY2_AHB_CLK:
		if(readl(SOFTSKU_STATUS_8) & BIT(UNIPHY_STAT_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY2_AHB_CBCR);
		break;
	case GCC_QPIC_SLEEP_CLK:
		clk_enable_cbc(priv->base + GCC_QPIC_SLEEP_CBCR);
		break;
	case GCC_QPIC_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_QPIC_AHB_CBCR);
		break;
	case GCC_QPIC_CLK:
		clk_enable_cbc(priv->base + GCC_QPIC_CBCR);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		clk_enable_cbc(priv->base + GCC_QPIC_IO_MACRO_CBCR);
		break;
	case GCC_MDIO_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_MDIO_AHB_CBCR);
		break;

	/* NSS clocks */
	case NSS_CC_PPE_SWITCH_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_CBCR);
		break;
	case NSS_CC_PPE_EDMA_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_EDMA_CBCR);
		break;
	case NSS_CC_PPE_EDMA_CFG_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_EDMA_CFG_CBCR);
		break;
	case NSS_CC_PORT1_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT1_MAC_CBCR);
		break;
	case NSS_CC_PORT2_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT2_MAC_CBCR);
		break;
	case NSS_CC_PORT3_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT3_MAC_CBCR);
		break;
	case NSS_CC_NSSNOC_PPE_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_PPE_CBCR);
		break;
	case NSS_CC_PORT1_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT1_RX_CBCR);
		break;
	case NSS_CC_PORT1_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT1_TX_CBCR);
		break;
	case NSS_CC_PORT2_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT2_RX_CBCR);
		break;
	case NSS_CC_PORT2_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT2_TX_CBCR);
		break;
	case NSS_CC_PORT3_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT3_RX_CBCR);
		break;
	case NSS_CC_PORT3_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT3_TX_CBCR);
		break;
	case NSS_CC_UNIPHY_PORT1_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT1_RX_CBCR);
		break;
	case NSS_CC_UNIPHY_PORT1_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT1_TX_CBCR);
		break;
	case NSS_CC_UNIPHY_PORT2_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT2_RX_CBCR);
		break;
	case NSS_CC_UNIPHY_PORT2_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT2_TX_CBCR);
		break;
	case NSS_CC_UNIPHY_PORT3_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT3_RX_CBCR);
		break;
	case NSS_CC_UNIPHY_PORT3_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT3_TX_CBCR);
		break;
	case NSS_CC_CE_APB_CLK:
		clk_enable_cbc(priv->base + NSS_CC_CE_APB_CBCR);
		break;
	case NSS_CC_CE_AXI_CLK:
		clk_enable_cbc(priv->base + NSS_CC_CE_AXI_CBCR);
		break;
	case NSS_CC_NSSNOC_CE_APB_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_CE_APB_CBCR);
		break;
	case NSS_CC_NSSNOC_CE_AXI_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_CE_AXI_CBCR);
		break;
	case NSS_CC_NSS_CSR_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSS_CSR_CBCR);
		break;
	case NSS_CC_NSSNOC_NSS_CSR_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_NSS_CSR_CBCR);
		break;
	case NSS_CC_PPE_SWITCH_IPE_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_IPE_CBCR);
		break;
	case NSS_CC_NSSNOC_PPE_CFG_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_PPE_CFG_CBCR);
		break;
	case NSS_CC_PPE_SWITCH_BTQ_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_BTQ_CBCR);
		break;

	case UNIPHY0_NSS_RX_CLK:
	case UNIPHY0_NSS_TX_CLK:
	case UNIPHY1_NSS_RX_CLK:
	case UNIPHY1_NSS_TX_CLK:
	case UNIPHY2_NSS_RX_CLK:
	case UNIPHY2_NSS_TX_CLK:
		break;
	default:
	}

	return 0;
}
