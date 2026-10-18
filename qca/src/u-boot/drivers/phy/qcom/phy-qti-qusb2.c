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

#define QUSB2PHY_PLL			0x00
#define QUSB2PHY_PLL_TEST		0x04
#define QUSB2PHY_PLL_TUNE		0x08
#define QUSB2PHY_PLL_USER_CTL1		0x0C
#define QUSB2PHY_PLL_USER_CTL2		0x10
#define QUSB2PHY_PLL_PWR_CTRL		0x18
#define QUSB2PHY_PLL_AUTOPGM_CTL1	0x1C
#define QUSB2PHY_PLL_STATUS		0x38
#define QUSB2PHY_PORT_TUNE1		0x80
#define QUSB2PHY_PORT_TUNE2		0x84
#define QUSB2PHY_PORT_TUNE3		0x88
#define QUSB2PHY_PORT_TUNE4		0x8C
#define QUSB2PHY_PORT_TUNE5		0x90
#define QUSB2PHY_PORT_TEST1		0x98
#define QUSB2PHY_PORT_TEST2		0x9C
#define QUSB2PHY_PORT_POWERDOWN		0xB4
#define QUSB2PHY_INTR_CTRL		0xBC

#define QUSB2_PHY_INIT_CFG(o, v) \
	{			\
		.reg = o,	\
		.val = v,	\
	}

struct qusb2_phy_cfg_tbl {
	u32 reg;
	u32 val;
};

struct qusb2_phy_cfg {
	const struct qusb2_phy_cfg_tbl *cfg_tbl;
	unsigned int cfg_num;
};

struct qusb2_phy_priv {
	void __iomem *base;
	struct clk_bulk clks;
	struct reset_ctl_bulk phy_rsts;
	const struct qusb2_phy_cfg *phy_cfg;
};

static const struct qusb2_phy_cfg_tbl ipq9574_phy_cfg_tbl[] = {
	/* QUSB2PHY_PLL:PLL Feedback Divider Value */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL, 0x14),
	/* QUSB2PHY_PORT_TUNE1: USB Product Application Tuning Register A */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PORT_TUNE1, 0xF8),
	/* QUSB2PHY_PORT_TUNE2: USB Product Application Tuning Register B */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PORT_TUNE2, 0xB3),
	/* QUSB2PHY_PORT_TUNE3: USB Product Application Tuning Register C */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PORT_TUNE3, 0x83),
	/* QUSB2PHY_PORT_TUNE4: USB Product Application Tuning Register D */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PORT_TUNE4, 0xC0),
	/* QUSB2PHY_PORT_TEST2 */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PORT_TEST2, 0x14),
	/* QUSB2PHY_PLL_TUNE: PLL Test Configuration */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TUNE, 0x30),
	/* QUSB2PHY_PLL_USER_CTL1: PLL Control Configuration */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL1, 0x79),
	/* QUSB2PHY_PLL_USER_CTL2: PLL Control Configuration */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL2, 0x21),
	/* QUSB2PHY_PORT_TUNE5 */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PORT_TUNE5, 0x00),
	/* QUSB2PHY_PLL_PWR_CTL: PLL Manual SW Programming
	 * and Biasing Power Options */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_PWR_CTRL, 0x00),
	/* QUSB2PHY_PLL_AUTOPGM_CTL1: Auto vs. Manual PLL/Power-mode
	 * programming State Machine Control Options */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_AUTOPGM_CTL1, 0x9F),
	/* QUSB2PHY_PLL_TEST: PLL Test Configuration-Disable diff ended
	 * clock */
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TEST, 0x80),
};

static const struct qusb2_phy_cfg ipq9574_phy_cfgs = {
	.cfg_tbl = ipq9574_phy_cfg_tbl,
	.cfg_num = ARRAY_SIZE(ipq9574_phy_cfg_tbl)
};

static int qusb2_phy_do_reset(struct qusb2_phy_priv *priv)
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

static int qusb2_phy_power_on(struct phy *phy)
{
	struct qusb2_phy_priv *priv = dev_get_priv(phy->dev);
	const struct qusb2_phy_cfg *phy_cfg = priv->phy_cfg;
	int ret, i;

        ret = clk_enable_bulk(&priv->clks);
        if (ret)
                return ret;

	ret = qusb2_phy_do_reset(priv);
	if (ret)
		return ret;

	/* Disable QUSB2PHY */
	setbits_le32(priv->base + QUSB2PHY_PORT_POWERDOWN, 0x1);

	/* PHY Config Sequence */
	for (i = 0; i < phy_cfg->cfg_num; i++)
		writel(phy_cfg->cfg_tbl[i].val,
				priv->base + phy_cfg->cfg_tbl[i].reg);

	/* Enable QUSB2PHY */
	clrbits_le32(priv->base + QUSB2PHY_PORT_POWERDOWN, 0x1);
	udelay(200);

	return 0;
}

static int qusb2_phy_power_off(struct phy *phy)
{
	struct qusb2_phy_priv *priv = dev_get_priv(phy->dev);
	int ret;

	/* Disable QUSB2PHY */
	setbits_le32(priv->base + QUSB2PHY_PORT_POWERDOWN, 0x1);

	ret = reset_assert_bulk(&priv->phy_rsts);
	if (ret)
		return ret;

        ret = clk_disable_bulk(&priv->clks);
        if (ret)
                return ret;

	return 0;
}

static int qusb2_phy_probe(struct udevice *dev)
{
	struct qusb2_phy_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = (void *)dev_read_addr(dev);
	if ((ulong)priv->base == FDT_ADDR_T_NONE)
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

static struct phy_ops qusb2_phy_ops = {
	.power_on = qusb2_phy_power_on,
	.power_off = qusb2_phy_power_off,
};

static const struct udevice_id qusb2_phy_ids[] = {
	{
		.compatible = "qti,ipq9574-qusb2-phy",
		.data	    = (long unsigned int)&ipq9574_phy_cfgs,
	},
	{ }
};

U_BOOT_DRIVER(qusb2_phy) = {
	.name		= "qti_qusb2_phy",
	.id		= UCLASS_PHY,
	.of_match	= qusb2_phy_ids,
	.ops		= &qusb2_phy_ops,
	.probe		= qusb2_phy_probe,
	.priv_auto	= sizeof(struct qusb2_phy_priv),
};
