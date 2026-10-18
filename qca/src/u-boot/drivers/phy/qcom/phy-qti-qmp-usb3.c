// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <reset.h>
#include <clk.h>
#include <linux/delay.h>

/* Only for QMP V2 PHY - QSERDES COM registers */
#define QSERDES_COM_BG_TIMER				0x00c
#define QSERDES_COM_SSC_EN_CENTER			0x010
#define QSERDES_COM_SSC_ADJ_PER1			0x014
#define QSERDES_COM_SSC_ADJ_PER2			0x018
#define QSERDES_COM_SSC_PER1				0x01c
#define QSERDES_COM_SSC_PER2				0x020
#define QSERDES_COM_SSC_STEP_SIZE1			0x024
#define QSERDES_COM_SSC_STEP_SIZE2			0x028
#define QSERDES_COM_BIAS_EN_CLKBUFLR_EN			0x034
#define QSERDES_COM_SYS_CLK_CTRL			0x03c
#define QSERDES_COM_PLL_IVCO				0x048
#define QSERDES_COM_LOCK_CMP1_MODE0			0x04c
#define QSERDES_COM_LOCK_CMP2_MODE0			0x050
#define QSERDES_COM_LOCK_CMP3_MODE0			0x054
#define QSERDES_COM_BG_TRIM				0x070
#define QSERDES_COM_CP_CTRL_MODE0			0x078
#define QSERDES_COM_PLL_RCTRL_MODE0			0x084
#define QSERDES_COM_PLL_CCTRL_MODE0			0x090
#define QSERDES_COM_SYSCLK_EN_SEL			0x0ac
#define QSERDES_COM_LOCK_CMP_CFG			0x0cc
#define QSERDES_COM_DEC_START_MODE0			0x0d0
#define QSERDES_COM_DIV_FRAC_START1_MODE0		0x0dc
#define QSERDES_COM_DIV_FRAC_START2_MODE0		0x0e0
#define QSERDES_COM_DIV_FRAC_START3_MODE0		0x0e4
#define QSERDES_COM_INTEGLOOP_GAIN0_MODE0		0x108
#define QSERDES_COM_VCO_TUNE_MAP			0x128
#define QSERDES_COM_CLK_SELECT				0x174
#define QSERDES_COM_HSCLK_SEL				0x178
#define QSERDES_COM_CORE_CLK_EN				0x18c
#define QSERDES_COM_CMN_CONFIG				0x194
#define QSERDES_COM_SVS_MODE_CLK_SEL			0x19c

/* Only for QMP V2 PHY - TX registers */
#define QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN	0x068
#define QSERDES_TX_LANE_MODE				0x094
#define QSERDES_TX_RCV_DETECT_LVL_2			0x0ac

/* Only for QMP V2 PHY - RX registers */
#define QSERDES_RX_UCDR_SO_GAIN				0x01c
#define QSERDES_RX_UCDR_FASTLOCK_FO_GAIN		0x040
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2		0x0d8
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3		0x0dc
#define QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4		0x0e0
#define QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1		0x108
#define QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2		0x10c
#define QSERDES_RX_SIGDET_ENABLES			0x110
#define QSERDES_RX_SIGDET_CNTRL				0x114
#define QSERDES_RX_SIGDET_DEGLITCH_CNTRL		0x11c

/* Only for QMP V3 PHY - PCS registers */
#define QPHY_V3_PCS_PHY_SW_RESET			0x000
#define QPHY_V3_PCS_POWER_DOWN_CONTROL			0x004
#define QPHY_V3_PCS_START_CONTROL			0x008
#define QPHY_V3_PCS_TXDEEMPH_M6DB_V0			0x024
#define QPHY_V3_PCS_TXDEEMPH_M3P5DB_V0			0x028
#define QPHY_V3_PCS_POWER_STATE_CONFIG2			0x064
#define QPHY_V3_PCS_RCVR_DTCT_DLY_P1U2_L		0x070
#define QPHY_V3_PCS_RCVR_DTCT_DLY_P1U2_H		0x074
#define QPHY_V3_PCS_RCVR_DTCT_DLY_U3_L			0x078
#define QPHY_V3_PCS_RCVR_DTCT_DLY_U3_H			0x07c
#define QPHY_V3_PCS_LOCK_DETECT_CONFIG1			0x080
#define QPHY_V3_PCS_LOCK_DETECT_CONFIG2			0x084
#define QPHY_V3_PCS_LOCK_DETECT_CONFIG3			0x088
#define QPHY_V3_PCS_TSYNC_RSYNC_TIME			0x08c
#define QPHY_V3_PCS_PWRUP_RESET_DLY_TIME_AUXCLK		0x0a0
#define QPHY_V3_PCS_LFPS_TX_ECSTART_EQTLOCK		0x0b0
#define QPHY_V3_PCS_RXEQTRAINING_WAIT_TIME		0x0b8
#define QPHY_V3_PCS_RXEQTRAINING_RUN_TIME		0x0bc
#define QPHY_V3_PCS_FLL_CNTRL1				0x0c4
#define QPHY_V3_PCS_FLL_CNTRL2				0x0c8
#define QPHY_V3_PCS_FLL_CNT_VAL_L			0x0cc
#define QPHY_V3_PCS_FLL_CNT_VAL_H_TOL			0x0d0
#define QPHY_V3_PCS_FLL_MAN_CODE			0x0d4
#define QPHY_V3_PCS_RX_SIGDET_LVL			0x1d8


#define QMP_USB3_PHY_INIT_CFG(o, v) \
	{			\
		.reg = o,	\
		.val = v,	\
	}

struct qmp_usb3_phy_cfg_tbl {
	u32 reg;
	u32 val;
};

struct qmp_usb3_phy_cfg {
	const struct qmp_usb3_phy_cfg_tbl *serdes_cfg_tbl;
	unsigned int serdes_cfg_num;
	const struct qmp_usb3_phy_cfg_tbl *tx_cfg_tbl;
	unsigned int tx_cfg_num;
	const struct qmp_usb3_phy_cfg_tbl *rx_cfg_tbl;
	unsigned int rx_cfg_num;
	const struct qmp_usb3_phy_cfg_tbl *pcs_cfg_tbl;
	unsigned int pcs_cfg_num;
};

struct qmp_usb3_phy_priv {
	void __iomem *com_base;
	void __iomem *tx_base;
	void __iomem *rx_base;
	void __iomem *pcs_base;
	struct clk_bulk clks;
	struct reset_ctl_bulk phy_rsts;
	const struct qmp_usb3_phy_cfg *phy_cfg;
};

static const struct qmp_usb3_phy_cfg_tbl ipq9574_phy_serdes_cfg_tbl[] = {
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SYSCLK_EN_SEL,0x1a),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_BIAS_EN_CLKBUFLR_EN,0x08),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_CLK_SELECT,0x30),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_BG_TRIM,0x0f),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_UCDR_FASTLOCK_FO_GAIN,0x0b),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SVS_MODE_CLK_SEL,0x01),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_HSCLK_SEL,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_CMN_CONFIG,0x06),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_PLL_IVCO,0x0f),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SYS_CLK_CTRL,0x06),
        /* PLL and Loop filter settings */
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_DEC_START_MODE0,0x68),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_DIV_FRAC_START1_MODE0,0xAB),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_DIV_FRAC_START2_MODE0,0xAA),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_DIV_FRAC_START3_MODE0,0x02),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_CP_CTRL_MODE0,0x09),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_PLL_RCTRL_MODE0,0x16),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_PLL_CCTRL_MODE0,0x28),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_INTEGLOOP_GAIN0_MODE0,0xA0),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_LOCK_CMP1_MODE0,0xAA),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_LOCK_CMP2_MODE0,0x29),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_LOCK_CMP3_MODE0,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_CORE_CLK_EN,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_LOCK_CMP_CFG,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_VCO_TUNE_MAP,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_BG_TIMER,0x0a),
        /* SSC settings */
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_EN_CENTER,0x01),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_PER1,0x7D),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_PER2,0x01),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_ADJ_PER1,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_ADJ_PER2,0x00),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_STEP_SIZE1,0x0A),
        QMP_USB3_PHY_INIT_CFG(QSERDES_COM_SSC_STEP_SIZE2,0x05),
};

static const struct qmp_usb3_phy_cfg_tbl ipq9574_phy_tx_cfg_tbl[] = {
        QMP_USB3_PHY_INIT_CFG(QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_DRVR_EN,0x45),
        QMP_USB3_PHY_INIT_CFG(QSERDES_TX_RCV_DETECT_LVL_2,0x12),
        QMP_USB3_PHY_INIT_CFG(QSERDES_TX_LANE_MODE,0x06),
};

static const struct qmp_usb3_phy_cfg_tbl ipq9574_phy_rx_cfg_tbl[] = {
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_UCDR_SO_GAIN,0x06),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2,0x02),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3,0x6c),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3,0x4c),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4,0xb8),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL1,0x77),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2,0x80),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_SIGDET_CNTRL,0x03),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_SIGDET_DEGLITCH_CNTRL,0x16),
        QMP_USB3_PHY_INIT_CFG(QSERDES_RX_SIGDET_ENABLES,0x0c),
};

static const struct qmp_usb3_phy_cfg_tbl ipq9574_phy_pcs_cfg_tbl[] = {
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_TXDEEMPH_M6DB_V0,0x15),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_TXDEEMPH_M3P5DB_V0,0x0e),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_FLL_CNTRL2,0x83),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_FLL_CNTRL1,0x02),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_FLL_CNT_VAL_L,0x09),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_FLL_CNT_VAL_H_TOL,0xa2),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_FLL_MAN_CODE,0x85),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_LOCK_DETECT_CONFIG1,0xd1),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_LOCK_DETECT_CONFIG2,0x1f),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_LOCK_DETECT_CONFIG3,0x47),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_POWER_STATE_CONFIG2,0x1b),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RXEQTRAINING_WAIT_TIME,0x75),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RXEQTRAINING_RUN_TIME,0x13),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_LFPS_TX_ECSTART_EQTLOCK,0x86),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_PWRUP_RESET_DLY_TIME_AUXCLK,0x04),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_TSYNC_RSYNC_TIME,0x44),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RCVR_DTCT_DLY_P1U2_L,0xe7),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RCVR_DTCT_DLY_P1U2_H,0x03),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RCVR_DTCT_DLY_U3_L,0x40),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RCVR_DTCT_DLY_U3_H,0x00),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_RX_SIGDET_LVL,0x88),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_TXDEEMPH_M6DB_V0,0x17),
        QMP_USB3_PHY_INIT_CFG(QPHY_V3_PCS_TXDEEMPH_M3P5DB_V0,0x0f),
};

static const struct qmp_usb3_phy_cfg ipq9574_phy_cfgs = {
	.serdes_cfg_tbl = ipq9574_phy_serdes_cfg_tbl,
	.serdes_cfg_num = ARRAY_SIZE(ipq9574_phy_serdes_cfg_tbl),
	.tx_cfg_tbl = ipq9574_phy_tx_cfg_tbl,
	.tx_cfg_num = ARRAY_SIZE(ipq9574_phy_tx_cfg_tbl),
	.rx_cfg_tbl = ipq9574_phy_rx_cfg_tbl,
	.rx_cfg_num = ARRAY_SIZE(ipq9574_phy_rx_cfg_tbl),
	.pcs_cfg_tbl = ipq9574_phy_pcs_cfg_tbl,
	.pcs_cfg_num = ARRAY_SIZE(ipq9574_phy_pcs_cfg_tbl),
};

static int qmp_usb3_phy_do_reset(struct qmp_usb3_phy_priv *priv)
{
	int ret;

	ret = reset_assert_bulk(&priv->phy_rsts);
	if (ret)
		return ret;

	udelay(200);

	ret = reset_deassert_bulk(&priv->phy_rsts);
	if (ret)
		return ret;

	return 0;
}

static int qmp_usb3_phy_power_on(struct phy *phy)
{
	struct qmp_usb3_phy_priv *priv = dev_get_priv(phy->dev);
	const struct qmp_usb3_phy_cfg *phy_cfg = priv->phy_cfg;
	int ret, i;

	ret = qmp_usb3_phy_do_reset(priv);
	if (ret)
		return ret;

	setbits_le32(priv->pcs_base + QPHY_V3_PCS_POWER_DOWN_CONTROL, 0x1);

	/* QMP PHY Serdes COM config Sequence */
	for (i = 0; i < phy_cfg->serdes_cfg_num; i++)
		writel(phy_cfg->serdes_cfg_tbl[i].val,
				priv->com_base +
				phy_cfg->serdes_cfg_tbl[i].reg);

	/* QMP PHY RX-lane config Sequence */
	for (i = 0; i < phy_cfg->rx_cfg_num; i++)
		writel(phy_cfg->rx_cfg_tbl[i].val,
				priv->rx_base +	phy_cfg->rx_cfg_tbl[i].reg);

	/* QMP PHY TX-lane config Sequence */
	for (i = 0; i < phy_cfg->tx_cfg_num; i++)
		writel(phy_cfg->tx_cfg_tbl[i].val,
				priv->tx_base +	phy_cfg->tx_cfg_tbl[i].reg);

	/* QMP PHY PCS-lane config Sequence */
	for (i = 0; i < phy_cfg->pcs_cfg_num; i++)
		writel(phy_cfg->pcs_cfg_tbl[i].val,
				priv->pcs_base + phy_cfg->pcs_cfg_tbl[i].reg);

	writel(0x00, priv->rx_base + QSERDES_RX_SIGDET_ENABLES);
	writel(0x03, priv->pcs_base + QPHY_V3_PCS_START_CONTROL);
	writel(0x00, priv->pcs_base + QPHY_V3_PCS_PHY_SW_RESET);
	udelay(200);

	ret = clk_enable_bulk(&priv->clks);
        if (ret)
                return ret;

	return 0;
}

static int qmp_usb3_phy_power_off(struct phy *phy)
{
	struct qmp_usb3_phy_priv *priv = dev_get_priv(phy->dev);
	int ret;

	clrbits_le32(priv->pcs_base + QPHY_V3_PCS_POWER_DOWN_CONTROL, 0x1);
	udelay(10);

	ret = reset_assert_bulk(&priv->phy_rsts);
	if (ret)
		return ret;

	ret = clk_disable_bulk(&priv->clks);
        if (ret)
                return ret;

	return 0;
}

static int qmp_usb3_phy_probe(struct udevice *dev)
{
	struct qmp_usb3_phy_priv *priv = dev_get_priv(dev);
	int ret;

	priv->com_base = (void *)dev_remap_addr_name(dev, "com_base");
	if ((ulong)priv->com_base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->tx_base = (void *)dev_remap_addr_name(dev, "tx_base");
	if ((ulong)priv->tx_base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->rx_base = (void *)dev_remap_addr_name(dev, "rx_base");
	if ((ulong)priv->rx_base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->pcs_base = (void *)dev_remap_addr_name(dev, "pcs_base");
	if ((ulong)priv->pcs_base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->phy_cfg = (void *)dev_get_driver_data(dev);

	ret = clk_get_bulk(dev, &priv->clks);
	if (ret && ret != -ENOSYS && ret != -ENOENT)
		return ret;

	ret = reset_get_bulk(dev, &priv->phy_rsts);
	if (ret) {
		clk_release_bulk(&priv->clks);
		return ret;
	}

	return 0;
}

static struct phy_ops qmp_usb3_phy_ops = {
	.power_on = qmp_usb3_phy_power_on,
	.power_off = qmp_usb3_phy_power_off,
};

static const struct udevice_id qmp_usb3_phy_ids[] = {
	{
		.compatible = "qti,ipq9574-qmp-usb3-phy",
		.data	    = (long unsigned int)&ipq9574_phy_cfgs,
	},
	{ }
};

U_BOOT_DRIVER(qmp_usb3_phy) = {
	.name		= "qti_qmp_usb3_phy",
	.id		= UCLASS_PHY,
	.of_match	= qmp_usb3_phy_ids,
	.ops		= &qmp_usb3_phy_ops,
	.probe		= qmp_usb3_phy_probe,
	.priv_auto	= sizeof(struct qmp_usb3_phy_priv),
};
