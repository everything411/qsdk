// SPDX-License-Identifier: BSD-3-Clause
/*
 * Clock drivers for QTI IPQ9574
 *
 * (C) Copyright 2022 Sumit Garg <sumit.garg@linaro.org>
 *
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include <dt-bindings/clock/gcc-ipq9574.h>


/*UNIPHY status register*/
#define QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB     0xA4024

#define UNIPHY_0_DISABLE_BIT                    23
#define UNIPHY_1_DISABLE_BIT                    24
#define UNIPHY_2_DISABLE_BIT                    25

/* GPLL0 clock control registers */
#define GPLL0_STATUS_ACTIVE BIT(31)

static struct vote_clk gcc_blsp1_ahb_clk = {
	.cbcr_reg = BLSP1_AHB_CBCR,
	.ena_vote = APCS_CLOCK_BRANCH_ENA_VOTE,
	.vote_bit = BIT(4) | BIT(2) | BIT(1),
};

static const struct bcr_regs sdc_regs = {
	.cfg_rcgr = SDCC1_APPS_CFG_RCGR,
	.cmd_rcgr = SDCC1_APPS_CMD_RCGR,
	.M = SDCC1_APPS_M,
	.N = SDCC1_APPS_N,
	.D = SDCC1_APPS_D,
};

static const struct bcr_regs uart0_regs = {
	.cfg_rcgr = BLSP1_UART_APPS_CFG_RCGR(0),
	.cmd_rcgr = BLSP1_UART_APPS_CMD_RCGR(0),
	.M = BLSP1_UART_APPS_M(0),
	.N = BLSP1_UART_APPS_N(0),
	.D = BLSP1_UART_APPS_D(0),
};

static const struct bcr_regs uart1_regs = {
	.cfg_rcgr = BLSP1_UART_APPS_CFG_RCGR(1),
	.cmd_rcgr = BLSP1_UART_APPS_CMD_RCGR(1),
	.M = BLSP1_UART_APPS_M(1),
	.N = BLSP1_UART_APPS_N(1),
	.D = BLSP1_UART_APPS_D(1),
};

static const struct bcr_regs uart2_regs = {
	.cfg_rcgr = BLSP1_UART_APPS_CFG_RCGR(2),
	.cmd_rcgr = BLSP1_UART_APPS_CMD_RCGR(2),
	.M = BLSP1_UART_APPS_M(2),
	.N = BLSP1_UART_APPS_N(2),
	.D = BLSP1_UART_APPS_D(2),
};

static const struct bcr_regs uart3_regs = {
	.cfg_rcgr = BLSP1_UART_APPS_CFG_RCGR(3),
	.cmd_rcgr = BLSP1_UART_APPS_CMD_RCGR(3),
	.M = BLSP1_UART_APPS_M(3),
	.N = BLSP1_UART_APPS_N(3),
	.D = BLSP1_UART_APPS_D(3),
};

static const struct bcr_regs uart4_regs = {
	.cfg_rcgr = BLSP1_UART_APPS_CFG_RCGR(4),
	.cmd_rcgr = BLSP1_UART_APPS_CMD_RCGR(4),
	.M = BLSP1_UART_APPS_M(4),
	.N = BLSP1_UART_APPS_N(4),
	.D = BLSP1_UART_APPS_D(4),
};

static const struct bcr_regs uart5_regs = {
	.cfg_rcgr = BLSP1_UART_APPS_CFG_RCGR(5),
	.cmd_rcgr = BLSP1_UART_APPS_CMD_RCGR(5),
	.M = BLSP1_UART_APPS_M(5),
	.N = BLSP1_UART_APPS_N(5),
	.D = BLSP1_UART_APPS_D(5),
};

static const struct bcr_regs qup1_spi_regs = {
	.cfg_rcgr = BLSP1_QUP_SPI_APPS_CFG_RCGR(0),
	.cmd_rcgr = BLSP1_QUP_SPI_APPS_CMD_RCGR(0),
	.M = BLSP1_QUP_SPI_APPS_M(0),
	.N = BLSP1_QUP_SPI_APPS_N(0),
	.D = BLSP1_QUP_SPI_APPS_D(0),
};

static const struct bcr_regs qup2_spi_regs = {
	.cfg_rcgr = BLSP1_QUP_SPI_APPS_CFG_RCGR(1),
	.cmd_rcgr = BLSP1_QUP_SPI_APPS_CMD_RCGR(1),
	.M = BLSP1_QUP_SPI_APPS_M(1),
	.N = BLSP1_QUP_SPI_APPS_N(1),
	.D = BLSP1_QUP_SPI_APPS_D(1),
};

static const struct bcr_regs qup3_spi_regs = {
	.cfg_rcgr = BLSP1_QUP_SPI_APPS_CFG_RCGR(2),
	.cmd_rcgr = BLSP1_QUP_SPI_APPS_CMD_RCGR(2),
	.M = BLSP1_QUP_SPI_APPS_M(2),
	.N = BLSP1_QUP_SPI_APPS_N(2),
	.D = BLSP1_QUP_SPI_APPS_D(2),
};

static const struct bcr_regs qup4_spi_regs = {
	.cfg_rcgr = BLSP1_QUP_SPI_APPS_CFG_RCGR(3),
	.cmd_rcgr = BLSP1_QUP_SPI_APPS_CMD_RCGR(3),
	.M = BLSP1_QUP_SPI_APPS_M(3),
	.N = BLSP1_QUP_SPI_APPS_N(3),
	.D = BLSP1_QUP_SPI_APPS_D(3),
};

static const struct bcr_regs qup5_spi_regs = {
	.cfg_rcgr = BLSP1_QUP_SPI_APPS_CFG_RCGR(4),
	.cmd_rcgr = BLSP1_QUP_SPI_APPS_CMD_RCGR(4),
	.M = BLSP1_QUP_SPI_APPS_M(4),
	.N = BLSP1_QUP_SPI_APPS_N(4),
	.D = BLSP1_QUP_SPI_APPS_D(4),
};

static const struct bcr_regs qup6_spi_regs = {
	.cfg_rcgr = BLSP1_QUP_SPI_APPS_CFG_RCGR(5),
	.cmd_rcgr = BLSP1_QUP_SPI_APPS_CMD_RCGR(5),
	.M = BLSP1_QUP_SPI_APPS_M(5),
	.N = BLSP1_QUP_SPI_APPS_N(5),
	.D = BLSP1_QUP_SPI_APPS_D(5),
};

static const struct bcr_regs pci_aux_regs = {
	.cfg_rcgr = GCC_PCIE_AUX_CFG_RCGR,
	.cmd_rcgr = GCC_PCIE_AUX_CMD_RCGR,
	.M = GCC_PCIE_AUX_M,
	.N = GCC_PCIE_AUX_N,
	.D = GCC_PCIE_AUX_D,
};

static const struct bcr_regs qup0_i2c_regs = {
	.cfg_rcgr = BLSP1_QUP_I2C_APPS_CFG_RCGR(0),
	.cmd_rcgr = BLSP1_QUP_I2C_APPS_CMD_RCGR(0),
};

static const struct bcr_regs qup1_i2c_regs = {
	.cfg_rcgr = BLSP1_QUP_I2C_APPS_CFG_RCGR(1),
	.cmd_rcgr = BLSP1_QUP_I2C_APPS_CMD_RCGR(1),
};

static const struct bcr_regs qup2_i2c_regs = {
	.cfg_rcgr = BLSP1_QUP_I2C_APPS_CFG_RCGR(2),
	.cmd_rcgr = BLSP1_QUP_I2C_APPS_CMD_RCGR(2),
};

static const struct bcr_regs qup3_i2c_regs = {
	.cfg_rcgr = BLSP1_QUP_I2C_APPS_CFG_RCGR(3),
	.cmd_rcgr = BLSP1_QUP_I2C_APPS_CMD_RCGR(3),
};

static const struct bcr_regs qup4_i2c_regs = {
	.cfg_rcgr = BLSP1_QUP_I2C_APPS_CFG_RCGR(4),
	.cmd_rcgr = BLSP1_QUP_I2C_APPS_CMD_RCGR(4),
};

static const struct bcr_regs qup5_i2c_regs = {
	.cfg_rcgr = BLSP1_QUP_I2C_APPS_CFG_RCGR(5),
	.cmd_rcgr = BLSP1_QUP_I2C_APPS_CMD_RCGR(5),
};

static const struct bcr_regs_v2 gcc_nssnoc_memnoc_bfdcd_regs = {
	.cfg_rcgr = GCC_NSSNOC_MEMNOC_BFDCD_CFG_RCGR,
	.cmd_rcgr = GCC_NSSNOC_MEMNOC_BFDCD_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_qdss_at_regs = {
	.cfg_rcgr = GCC_QDSS_AT_CFG_RCGR,
	.cmd_rcgr = GCC_QDSS_AT_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_uniphy_sys_regs = {
	.cfg_rcgr = GCC_UNIPHY_SYS_CFG_RCGR,
	.cmd_rcgr = GCC_UNIPHY_SYS_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_pcnoc_bfdcd_regs = {
	.cfg_rcgr = GCC_PCNOC_BFDCD_CFG_RCGR,
	.cmd_rcgr = GCC_PCNOC_BFDCD_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_system_noc_bfdcd_regs = {
	.cfg_rcgr = GCC_SYSTEM_NOC_BFDCD_CFG_RCGR,
	.cmd_rcgr = GCC_SYSTEM_NOC_BFDCD_CMD_RCGR,
};

static const struct bcr_regs_v2 nss_cc_cfg_regs = {
	.cfg_rcgr = NSS_CC_CFG_CFG_RCGR,
	.cmd_rcgr = NSS_CC_CFG_CMD_RCGR,
};

static const struct bcr_regs_v2 nss_cc_ppe_regs = {
	.cfg_rcgr = NSS_CC_PPE_CFG_RCGR,
	.cmd_rcgr = NSS_CC_PPE_CMD_RCGR,
};

static const struct bcr_regs_v2 gcc_qpic_io_macro_regs = {
	.cfg_rcgr = GCC_QPIC_IO_MACRO_CFG_RCGR,
	.cmd_rcgr = GCC_QPIC_IO_MACRO_CMD_RCGR,
};

static const struct bcr_regs gcc_pcie0_axi_m_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_M_CFG_RCGR(0),
	.cmd_rcgr = GCC_PCIE_AXI_M_CMD_RCGR(0),
};

static const struct bcr_regs gcc_pcie1_axi_m_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_M_CFG_RCGR(1),
	.cmd_rcgr = GCC_PCIE_AXI_M_CMD_RCGR(1),
};

static const struct bcr_regs gcc_pcie2_axi_m_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_M_CFG_RCGR(2),
	.cmd_rcgr = GCC_PCIE_AXI_M_CMD_RCGR(2),
};

static const struct bcr_regs gcc_pcie3_axi_m_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_M_CFG_RCGR(3),
	.cmd_rcgr = GCC_PCIE_AXI_M_CMD_RCGR(3),
};

static const struct bcr_regs gcc_pcie0_axi_s_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_S_CFG_RCGR(0),
	.cmd_rcgr = GCC_PCIE_AXI_S_CMD_RCGR(0),
};

static const struct bcr_regs gcc_pcie1_axi_s_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_S_CFG_RCGR(1),
	.cmd_rcgr = GCC_PCIE_AXI_S_CMD_RCGR(1),
};

static const struct bcr_regs gcc_pcie2_axi_s_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_S_CFG_RCGR(2),
	.cmd_rcgr = GCC_PCIE_AXI_S_CMD_RCGR(2),
};

static const struct bcr_regs gcc_pcie3_axi_s_regs = {
	.cfg_rcgr = GCC_PCIE_AXI_S_CFG_RCGR(3),
	.cmd_rcgr = GCC_PCIE_AXI_S_CMD_RCGR(3),
};

static const struct bcr_regs gcc_pcie0_rchng_regs = {
	.cfg_rcgr = GCC_PCIE_RCHNG_CFG_RCGR(0),
	.cmd_rcgr = GCC_PCIE_RCHNG_CMD_RCGR(0),
};

static const struct bcr_regs gcc_pcie1_rchng_regs = {
	.cfg_rcgr = GCC_PCIE_RCHNG_CFG_RCGR(1),
	.cmd_rcgr = GCC_PCIE_RCHNG_CMD_RCGR(1),
};

static const struct bcr_regs gcc_pcie2_rchng_regs = {
	.cfg_rcgr = GCC_PCIE_RCHNG_CFG_RCGR(2),
	.cmd_rcgr = GCC_PCIE_RCHNG_CMD_RCGR(2),
};

static const struct bcr_regs gcc_pcie3_rchng_regs = {
	.cfg_rcgr = GCC_PCIE_RCHNG_CFG_RCGR(3),
	.cmd_rcgr = GCC_PCIE_RCHNG_CMD_RCGR(3),
};

static const struct bcr_regs_v2 nss_cc_port1_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT_RX_CFG_RCGR(1),
	.cmd_rcgr = NSS_CC_PORT_RX_CMD_RCGR(1),
	.div_cdivr = NSS_CC_PORT_RX_DIV_CDIVR(1),
};

static const struct bcr_regs_v2 nss_cc_port1_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT_TX_CFG_RCGR(1),
	.cmd_rcgr = NSS_CC_PORT_TX_CMD_RCGR(1),
	.div_cdivr = NSS_CC_PORT_TX_DIV_CDIVR(1),
};

static const struct bcr_regs_v2 nss_cc_port2_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT_RX_CFG_RCGR(2),
	.cmd_rcgr = NSS_CC_PORT_RX_CMD_RCGR(2),
	.div_cdivr = NSS_CC_PORT_RX_DIV_CDIVR(2),
};

static const struct bcr_regs_v2 nss_cc_port2_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT_TX_CFG_RCGR(2),
	.cmd_rcgr = NSS_CC_PORT_TX_CMD_RCGR(2),
	.div_cdivr = NSS_CC_PORT_TX_DIV_CDIVR(2),
};

static const struct bcr_regs_v2 nss_cc_port3_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT_RX_CFG_RCGR(3),
	.cmd_rcgr = NSS_CC_PORT_RX_CMD_RCGR(3),
	.div_cdivr = NSS_CC_PORT_RX_DIV_CDIVR(3),
};

static const struct bcr_regs_v2 nss_cc_port3_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT_TX_CFG_RCGR(3),
	.cmd_rcgr = NSS_CC_PORT_TX_CMD_RCGR(3),
	.div_cdivr = NSS_CC_PORT_TX_DIV_CDIVR(3),
};

static const struct bcr_regs_v2 nss_cc_port4_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT_RX_CFG_RCGR(4),
	.cmd_rcgr = NSS_CC_PORT_RX_CMD_RCGR(4),
	.div_cdivr = NSS_CC_PORT_RX_DIV_CDIVR(4),
};

static const struct bcr_regs_v2 nss_cc_port4_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT_TX_CFG_RCGR(4),
	.cmd_rcgr = NSS_CC_PORT_TX_CMD_RCGR(4),
	.div_cdivr = NSS_CC_PORT_TX_DIV_CDIVR(4),
};

static const struct bcr_regs_v2 nss_cc_port5_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT_RX_CFG_RCGR(5),
	.cmd_rcgr = NSS_CC_PORT_RX_CMD_RCGR(5),
	.div_cdivr = NSS_CC_PORT_RX_DIV_CDIVR(5),
};

static const struct bcr_regs_v2 nss_cc_port5_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT_TX_CFG_RCGR(5),
	.cmd_rcgr = NSS_CC_PORT_TX_CMD_RCGR(5),
	.div_cdivr = NSS_CC_PORT_TX_DIV_CDIVR(5),
};

static const struct bcr_regs_v2 nss_cc_port6_rx_regs = {
	.cfg_rcgr = NSS_CC_PORT_RX_CFG_RCGR(6),
	.cmd_rcgr = NSS_CC_PORT_RX_CMD_RCGR(6),
	.div_cdivr = NSS_CC_PORT_RX_DIV_CDIVR(6),
};

static const struct bcr_regs_v2 nss_cc_port6_tx_regs = {
	.cfg_rcgr = NSS_CC_PORT_TX_CFG_RCGR(6),
	.cmd_rcgr = NSS_CC_PORT_TX_CMD_RCGR(6),
	.div_cdivr = NSS_CC_PORT_TX_DIV_CDIVR(6),
};

static const struct bcr_regs usb0_mock_utmi_regs = {
	.cfg_rcgr = GCC_USB0_MOCK_UTMI_CFG_RCGR,
	.cmd_rcgr = GCC_USB0_MOCK_UTMI_CMD_RCGR,
	.M = GCC_USB0_MOCK_UTMI_M,
	.N = GCC_USB0_MOCK_UTMI_N,
	.D = GCC_USB0_MOCK_UTMI_D,
};

static const struct bcr_regs usb0_aux_regs = {
	.cfg_rcgr = GCC_USB0_AUX_CFG_RCGR,
	.cmd_rcgr = GCC_USB0_AUX_CMD_RCGR,
	.M = GCC_USB0_AUX_M,
	.N = GCC_USB0_AUX_N,
	.D = GCC_USB0_AUX_D,
};

static const struct bcr_regs usb0_master_regs = {
	.cfg_rcgr = GCC_USB0_MASTER_CFG_RCGR,
	.cmd_rcgr = GCC_USB0_MASTER_CMD_RCGR,
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
	switch (clk->id) {

	case GCC_BLSP1_QUP0_I2C_APPS_CLK:
	case GCC_BLSP1_QUP1_I2C_APPS_CLK:
	case GCC_BLSP1_QUP2_I2C_APPS_CLK:
	case GCC_BLSP1_QUP3_I2C_APPS_CLK:
	case GCC_BLSP1_QUP4_I2C_APPS_CLK:
	case GCC_BLSP1_QUP5_I2C_APPS_CLK:
		clk->rate = CLK_50_MHZ;
		break;
	}

	return (ulong)clk->rate;
}

ulong msm_set_rate(struct clk *clk, ulong rate)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);
	int ret = 0, src, div = 0, cdiv = 0;
	struct clk *pclk = NULL;

	switch (clk->id) {

	case GCC_BLSP1_UART0_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, &uart0_regs, 0, 36, 15625,
				     SDCC1_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_UART1_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, &uart1_regs, 0, 36, 15625,
				     SDCC1_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_UART2_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, &uart2_regs, 0, 36, 15625,
				     SDCC1_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_UART3_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, &uart3_regs, 0, 36, 15625,
				     SDCC1_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_UART4_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, &uart4_regs, 0, 36, 15625,
				     SDCC1_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_UART5_APPS_CLK:
		/* UART: 115200 */
		clk_rcg_set_rate_mnd(priv->base, &uart5_regs, 0, 36, 15625,
				     SDCC1_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SDCC1_APPS_CLK:
		/* SDCC1: 200MHz */
		clk_rcg_set_rate_mnd(priv->base, &sdc_regs, 6, 0, 0,
				     SDCC1_SRC_SEL_GPLL2_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP1_SPI_APPS_CLK:
		/* QUP1 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &qup1_spi_regs, 16, 0, 0,
				     BLSP1_QUP_SPI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP2_SPI_APPS_CLK:
		/* QUP2 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &qup2_spi_regs, 16, 0, 0,
				     BLSP1_QUP_SPI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP3_SPI_APPS_CLK:
		/* QUP3 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &qup3_spi_regs, 16, 0, 0,
				     BLSP1_QUP_SPI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP4_SPI_APPS_CLK:
		/* QUP4 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &qup4_spi_regs, 16, 0, 0,
				     BLSP1_QUP_SPI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP5_SPI_APPS_CLK:
		/* QUP5 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &qup5_spi_regs, 16, 0, 0,
				     BLSP1_QUP_SPI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP6_SPI_APPS_CLK:
		/* QUP6 SPI APPS CLK: 50MHz */
		clk_rcg_set_rate_mnd(priv->base, &qup6_spi_regs, 16, 0, 0,
				     BLSP1_QUP_SPI_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_UNIPHY_SYS_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_uniphy_sys_regs,
			1, 0, 0);
		break;
	case GCC_PCNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_pcnoc_bfdcd_regs,
			15, 0, GCC_PCNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_SYSTEM_NOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_system_noc_bfdcd_regs,
			6, 0, GCC_SYSTEM_NOC_BFDCD_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_NSSNOC_MEMNOC_BFDCD_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_nssnoc_memnoc_bfdcd_regs,
			2, 0, GCC_NSSNOC_MEMNOC_BFDCD_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QDSS_AT_CLK:
		clk_rcg_set_rate_v2(priv->base, &gcc_qdss_at_regs,
			9, 0, GCC_QDSS_AT_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		src = GCC_QPIC_IO_MACRO_SRC_SEL_GPLL0_OUT_MAIN;
		cdiv = 0;
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
	case GCC_BLSP1_QUP0_I2C_APPS_CLK:
		/* QUP0 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, &qup0_i2c_regs,
				BLSP1_QUP_I2C_50M_DIV_VAL,
				BLSP1_QUP_I2C_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP1_I2C_APPS_CLK:
		/* QUP1 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, &qup1_i2c_regs,
				BLSP1_QUP_I2C_50M_DIV_VAL,
				BLSP1_QUP_I2C_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP2_I2C_APPS_CLK:
		/* QUP2 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, &qup2_i2c_regs,
				BLSP1_QUP_I2C_50M_DIV_VAL,
				BLSP1_QUP_I2C_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP3_I2C_APPS_CLK:
		/* QUP3 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, &qup3_i2c_regs,
				BLSP1_QUP_I2C_50M_DIV_VAL,
				BLSP1_QUP_I2C_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP4_I2C_APPS_CLK:
		/* QUP4 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, &qup4_i2c_regs,
				BLSP1_QUP_I2C_50M_DIV_VAL,
				BLSP1_QUP_I2C_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_BLSP1_QUP5_I2C_APPS_CLK:
		/* QUP5 I2C APPS CLK: 50MHz */
		clk_rcg_set_rate(priv->base, &qup5_i2c_regs,
				BLSP1_QUP_I2C_50M_DIV_VAL,
				BLSP1_QUP_I2C_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE_AUX_CLK:
		/* PCIE AUX CLK: 20MHZ */
		clk_rcg_set_rate_mnd(priv->base, &pci_aux_regs, 10, 1, 4,
				     PCIE_SRC_SEL_UNSUSED_GND);
		break;
	case GCC_PCIE0_AXI_M_CLK:
		/* PCIE0_AXI_M_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie0_axi_m_regs,
			9, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_M_CLK:
		/* PCIE1_AXI_M_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie1_axi_m_regs,
			9, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE2_AXI_M_CLK:
		/* PCIE2_AXI_M_CLK: 342MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie2_axi_m_regs,
			6, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE3_AXI_M_CLK:
		/* PCIE3_AXI_M_CLK: 342MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie3_axi_m_regs,
			6, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_AXI_S_CLK:
		/* PCIE0_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie0_axi_s_regs,
			9, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE1_AXI_S_CLK:
		/* PCIE1_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie1_axi_s_regs,
			9, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE2_AXI_S_CLK:
		/* PCIE2_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie2_axi_s_regs,
			9, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE3_AXI_S_CLK:
		/* PCIE3_AXI_S_CLK: 240MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie3_axi_s_regs,
			9, PCIE_SRC_SEL_GPLL4_OUT_MAIN);
		break;
	case GCC_PCIE0_RCHNG_CLK:
		/* PCIE0_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie0_rchng_regs,
			15, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE1_RCHNG_CLK:
		/* PCIE1_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie1_rchng_regs,
			15, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE2_RCHNG_CLK:
		/* PCIE2_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie2_rchng_regs,
			15, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_PCIE3_RCHNG_CLK:
		/* PCIE3_RCHNG_CLK: 100MHZ */
		clk_rcg_set_rate(priv->base, &gcc_pcie3_rchng_regs,
			15, PCIE_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_USB0_MASTER_CLK:
		clk_rcg_set_rate(priv->base, &usb0_master_regs,
				4, USB0_MASTER_SRC_SEL_GPLL0_OUT_MAIN);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		clk_rcg_set_rate_mnd(priv->base, &usb0_mock_utmi_regs, 0, 0, 0,
				USB0_MOCK_UTMI_SRC_SEL_XO);
		break;
	case GCC_USB0_AUX_CLK:
		clk_rcg_set_rate_mnd(priv->base, &usb0_aux_regs, 0, 0, 0,
				USB0_AUX_SRC_SEL_XO);
		break;

	/*
	 * NSS controlled clock
	 */
	case NSS_CC_CFG_CLK:
		clk_rcg_set_rate_v2(priv->base, &nss_cc_cfg_regs,
			15, 0, NSS_CC_CFG_SRC_SEL_GCC_GPLL0_OUT_AUX);
		break;
	case NSS_CC_PPE_CLK:
		clk_rcg_set_rate_v2(priv->base, &nss_cc_ppe_regs,
			1, 0, NSS_CC_PPE_SRC_SEL_BIAS_PLL_UBI_NC_CLK);
		break;
	case NSS_CC_PORT1_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port1_rx_regs,
				div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT1_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port1_tx_regs,
				div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT2_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port2_rx_regs,
				div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT2_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port2_tx_regs,
				div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT3_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port3_rx_regs,
				div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT3_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port3_tx_regs,
				div, cdiv,
				NSS_CC_PORT1_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK);
		break;
	case NSS_CC_PORT4_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port4_rx_regs,
				div, cdiv,
				NSS_CC_PORT1_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK);
		break;
	case NSS_CC_PORT4_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port4_tx_regs,
				div, cdiv,
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

		if (pclk->id == UNIPHY0_NSS_RX_CLK)
			src = NSS_CC_PORT5_RX_SRC_SEL_UNIPHY0_NSS_RX_CLK;
		else if (pclk->id == UNIPHY1_NSS_RX_CLK)
			src = NSS_CC_PORT5_RX_SRC_SEL_UNIPHY1_NSS_RX_CLK;
		else {
			ret = -EINVAL;
			break;
		}
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port5_rx_regs,
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

		if (pclk->id == UNIPHY0_NSS_TX_CLK)
			src = NSS_CC_PORT5_TX_SRC_SEL_UNIPHY0_NSS_TX_CLK;
		else if (pclk->id == UNIPHY1_NSS_TX_CLK)
			src = NSS_CC_PORT5_TX_SRC_SEL_UNIPHY1_NSS_TX_CLK;
		else {
			ret = -EINVAL;
			break;
		}
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port5_tx_regs,
				div, cdiv, src);
		break;
	case NSS_CC_PORT6_RX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port6_rx_regs,
				div, cdiv,
				NSS_CC_PORT6_RX_SRC_SEL_UNIPHY2_NSS_RX_CLK);
		break;
	case NSS_CC_PORT6_TX_CLK:
		ret = calc_div_for_nss_port_clk(clk, rate, &div, &cdiv);
		if (ret < 0)
			return ret;
		clk_rcg_set_rate_v2(priv->base, &nss_cc_port6_tx_regs,
				div, cdiv,
				NSS_CC_PORT6_TX_SRC_SEL_UNIPHY2_NSS_TX_CLK);
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
	default:
		ret = -EINVAL;
	}

	return ret;
}

int msm_enable(struct clk *clk)
{
	struct msm_clk_priv *priv = dev_get_priv(clk->dev);

	switch (clk->id) {
	case GCC_BLSP1_AHB_CLK:
		clk_enable_vote_clk(priv->base, &gcc_blsp1_ahb_clk);
		break;
	case GCC_BLSP1_QUP1_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(0));
		break;
	case GCC_BLSP1_QUP2_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(1));
		break;
	case GCC_BLSP1_QUP3_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(2));
		break;
	case GCC_BLSP1_QUP4_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(3));
		break;
	case GCC_BLSP1_QUP5_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(4));
		break;
	case GCC_BLSP1_QUP6_SPI_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_SPI_APPS_CBCR(5));
		break;
	case GCC_SDCC1_APPS_CLK:
		clk_enable_cbc(priv->base + SDCC1_APPS_CBCR);
		break;
	case GCC_MEM_NOC_NSSNOC_CLK:
		clk_enable_cbc(priv->base + GCC_MEM_NOC_NSSNOC_CBCR);
		break;
	case GCC_NSSCFG_CLK:
		clk_enable_cbc(priv->base + GCC_NSSCFG_CBCR);
		break;
	case GCC_NSSNOC_ATB_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_ATB_CBCR);
		break;
	case GCC_NSSNOC_MEM_NOC_1_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_MEM_NOC_1_CBCR);
		break;
	case GCC_NSSNOC_MEMNOC_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_MEMNOC_CBCR);
		break;
	case GCC_NSSNOC_QOSGEN_REF_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_QOSGEN_REF_CBCR);
		break;
	case GCC_NSSNOC_TIMEOUT_REF_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_TIMEOUT_REF_CBCR);
		break;
	case GCC_CMN_12GPLL_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_CMN_12GPLL_AHB_CBCR);
		break;
	case GCC_CMN_12GPLL_SYS_CLK:
		clk_enable_cbc(priv->base + GCC_CMN_12GPLL_SYS_CBCR);
		break;
	case GCC_UNIPHY0_SYS_CLK:
		if(readl(QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB) &
				BIT(UNIPHY_0_DISABLE_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY_SYS_CBCR(0));
		break;
	case GCC_UNIPHY0_AHB_CLK:
		if(readl(QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB) &
				BIT(UNIPHY_0_DISABLE_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY_AHB_CBCR(0));
		break;
	case GCC_UNIPHY1_SYS_CLK:
		if(readl(QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB) &
				BIT(UNIPHY_1_DISABLE_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY_SYS_CBCR(1));
		break;
	case GCC_UNIPHY1_AHB_CLK:
		if(readl(QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB) &
				BIT(UNIPHY_1_DISABLE_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY_AHB_CBCR(1));
		break;
	case GCC_UNIPHY2_SYS_CLK:
		if(readl(QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB) &
				BIT(UNIPHY_2_DISABLE_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY_SYS_CBCR(2));
		break;
	case GCC_UNIPHY2_AHB_CLK:
		if(readl(QFPROM_CORR_FEATURE_CONFIG_ROW2_MSB) &
				BIT(UNIPHY_2_DISABLE_BIT))
			break;
		clk_enable_cbc(priv->base + GCC_UNIPHY_AHB_CBCR(2));
		break;
	case GCC_MDIO_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_MDIO_AHB_CBCR);
		break;
	case GCC_NSSNOC_SNOC_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_SNOC_CBCR);
		break;
	case GCC_NSSNOC_SNOC_1_CLK:
		clk_enable_cbc(priv->base + GCC_NSSNOC_SNOC_1_CBCR);
		break;
	case GCC_MEM_NOC_SNOC_AXI_CLK:
		clk_enable_cbc(priv->base + GCC_MEM_NOC_SNOC_AXI_CBCR);
		break;
	case GCC_QPIC_IO_MACRO_CLK:
		clk_enable_cbc(priv->base + GCC_QPIC_IO_MACRO_CBCR);
		break;
	case GCC_SDCC1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_SDCC1_AHB_CBCR);
		break;
	case GCC_BLSP1_QUP0_I2C_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_I2C_APPS_CBCR(0));
		break;
	case GCC_BLSP1_QUP1_I2C_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_I2C_APPS_CBCR(1));
		break;
	case GCC_BLSP1_QUP2_I2C_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_I2C_APPS_CBCR(2));
		break;
	case GCC_BLSP1_QUP3_I2C_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_I2C_APPS_CBCR(3));
		break;
	case GCC_BLSP1_QUP4_I2C_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_I2C_APPS_CBCR(4));
		break;
	case GCC_BLSP1_QUP5_I2C_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_QUP_I2C_APPS_CBCR(5));
		break;
	case GCC_PCIE0_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AUX_CBCR(0));
		break;
	case GCC_PCIE1_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AUX_CBCR(1));
		break;
	case GCC_PCIE2_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AUX_CBCR(2));
		break;
	case GCC_PCIE3_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AUX_CBCR(3));
		break;
	case GCC_PCIE0_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AHB_CBCR(0));
		break;
	case GCC_PCIE1_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AHB_CBCR(1));
		break;
	case GCC_PCIE2_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AHB_CBCR(2));
		break;
	case GCC_PCIE3_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AHB_CBCR(3));
		break;
	case GCC_PCIE0_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_M_CBCR(0));
		break;
	case GCC_PCIE1_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_M_CBCR(1));
		break;
	case GCC_PCIE2_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_M_CBCR(2));
		break;
	case GCC_PCIE3_AXI_M_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_M_CBCR(3));
		break;
	case GCC_PCIE0_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_CBCR(0));
		break;
	case GCC_PCIE1_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_CBCR(1));
		break;
	case GCC_PCIE2_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_CBCR(2));
		break;
	case GCC_PCIE3_AXI_S_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_CBCR(3));
		break;
	case GCC_PCIE0_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_BRIDGE_CBCR(0));
		break;
	case GCC_PCIE1_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_BRIDGE_CBCR(1));
		break;
	case GCC_PCIE2_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_BRIDGE_CBCR(2));
		break;
	case GCC_PCIE3_AXI_S_BRIDGE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_AXI_S_BRIDGE_CBCR(3));
		break;
	case GCC_PCIE0_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_PIPE_CBCR(0));
		break;
	case GCC_PCIE1_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_PIPE_CBCR(1));
		break;
	case GCC_PCIE2_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_PIPE_CBCR(2));
		break;
	case GCC_PCIE3_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_PCIE_PIPE_CBCR(3));
		break;
	case GCC_SNOC_PCIE0_1LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_SNOC_PCIE0_1LANE_S_CBCR);
		break;
	case GCC_SNOC_PCIE1_1LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_SNOC_PCIE1_1LANE_S_CBCR);
		break;
	case GCC_SNOC_PCIE2_2LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_SNOC_PCIE2_2LANE_S_CBCR);
		break;
	case GCC_SNOC_PCIE3_2LANE_S_CLK:
		clk_enable_cbc(priv->base + GCC_SNOC_PCIE3_2LANE_S_CBCR);
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
	case GCC_USB0_MASTER_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_MASTER_CBCR);
		break;
	case GCC_USB0_MOCK_UTMI_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_MOCK_UTMI_CBCR);
		break;
	case GCC_USB0_AUX_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_AUX_CBCR);
		break;
	case GCC_USB0_PIPE_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_PIPE_CBCR);
		break;
	case GCC_USB0_SLEEP_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_SLEEP_CBCR);
		break;
	case GCC_SNOC_USB_CLK:
		clk_enable_cbc(priv->base + GCC_SNOC_USB_CBCR);
		break;
	case GCC_ANOC_USB_AXI_CLK:
		clk_enable_cbc(priv->base + GCC_ANOC_USB_AXI_CBCR);
		break;
	case GCC_USB0_PHY_CFG_AHB_CLK:
		clk_enable_cbc(priv->base + GCC_USB0_PHY_CFG_AHB_CBCR);
		break;
	case GCC_BLSP1_UART0_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_UART_APPS_CBCR(0));
		break;
	case GCC_BLSP1_UART1_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_UART_APPS_CBCR(1));
		break;
	case GCC_BLSP1_UART2_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_UART_APPS_CBCR(2));
		break;
	case GCC_BLSP1_UART3_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_UART_APPS_CBCR(3));
		break;
	case GCC_BLSP1_UART4_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_UART_APPS_CBCR(4));
		break;
	case GCC_BLSP1_UART5_APPS_CLK:
		clk_enable_cbc(priv->base + BLSP1_UART_APPS_CBCR(5));
		break;
	/*
	 * NSS controlled clock
	 */
	case NSS_CC_NSS_CSR_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSS_CSR_CBCR);
		break;
	case NSS_CC_NSSNOC_NSS_CSR_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_NSS_CSR_CBCR);
		break;
	case NSS_CC_PORT1_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_MAC_CBCR(1));
		break;
	case NSS_CC_PORT2_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_MAC_CBCR(2));
		break;
	case NSS_CC_PORT3_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_MAC_CBCR(3));
		break;
	case NSS_CC_PORT4_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_MAC_CBCR(4));
		break;
	case NSS_CC_PORT5_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_MAC_CBCR(5));
		break;
	case NSS_CC_PORT6_MAC_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_MAC_CBCR(6));
		break;
	case NSS_CC_PPE_SWITCH_IPE_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_IPE_CBCR);
		break;
	case NSS_CC_PPE_SWITCH_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_CBCR);
		break;
	case NSS_CC_PPE_SWITCH_CFG_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_CFG_CBCR);
		break;
	case NSS_CC_PPE_EDMA_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_EDMA_CBCR);
		break;
	case NSS_CC_PPE_EDMA_CFG_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_EDMA_CFG_CBCR);
		break;
	case NSS_CC_CRYPTO_PPE_CLK:
		clk_enable_cbc(priv->base + NSS_CC_CRYPTO_PPE_CBCR);
		break;
	case NSS_CC_NSSNOC_PPE_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_PPE_CBCR);
		break;
	case NSS_CC_NSSNOC_PPE_CFG_CLK:
		clk_enable_cbc(priv->base + NSS_CC_NSSNOC_PPE_CFG_CBCR);
		break;
	case NSS_CC_PPE_SWITCH_BTQ_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PPE_SWITCH_BTQ_CBCR);
		break;
	case NSS_CC_PORT1_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_RX_CBCR(1));
		break;
	case NSS_CC_PORT1_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_TX_CBCR(1));
		break;
	case NSS_CC_PORT2_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_RX_CBCR(2));
		break;
	case NSS_CC_PORT2_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_TX_CBCR(2));
		break;
	case NSS_CC_PORT3_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_RX_CBCR(3));
		break;
	case NSS_CC_PORT3_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_TX_CBCR(3));
		break;
	case NSS_CC_PORT4_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_RX_CBCR(4));
		break;
	case NSS_CC_PORT4_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_TX_CBCR(4));
		break;
	case NSS_CC_PORT5_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_RX_CBCR(5));
		break;
	case NSS_CC_PORT5_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_TX_CBCR(5));
		break;
	case NSS_CC_PORT6_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_RX_CBCR(6));
		break;
	case NSS_CC_PORT6_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_PORT_TX_CBCR(6));
		break;
	case NSS_CC_UNIPHY_PORT1_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_RX_CBCR(1));
		break;
	case NSS_CC_UNIPHY_PORT1_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_TX_CBCR(1));
		break;
	case NSS_CC_UNIPHY_PORT2_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_RX_CBCR(2));
		break;
	case NSS_CC_UNIPHY_PORT2_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_TX_CBCR(2));
		break;
	case NSS_CC_UNIPHY_PORT3_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_RX_CBCR(3));
		break;
	case NSS_CC_UNIPHY_PORT3_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_TX_CBCR(3));
		break;
	case NSS_CC_UNIPHY_PORT4_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_RX_CBCR(4));
		break;
	case NSS_CC_UNIPHY_PORT4_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_TX_CBCR(4));
		break;
	case NSS_CC_UNIPHY_PORT5_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_RX_CBCR(5));
		break;
	case NSS_CC_UNIPHY_PORT5_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_TX_CBCR(5));
		break;
	case NSS_CC_UNIPHY_PORT6_RX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_RX_CBCR(6));
		break;
	case NSS_CC_UNIPHY_PORT6_TX_CLK:
		clk_enable_cbc(priv->base + NSS_CC_UNIPHY_PORT_TX_CBCR(6));
		break;

	case UNIPHY0_NSS_RX_CLK:
	case UNIPHY0_NSS_TX_CLK:
	case UNIPHY1_NSS_RX_CLK:
	case UNIPHY1_NSS_TX_CLK:
	case UNIPHY2_NSS_RX_CLK:
	case UNIPHY2_NSS_TX_CLK:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}
