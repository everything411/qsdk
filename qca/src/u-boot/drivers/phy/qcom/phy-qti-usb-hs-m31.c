// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2014-2016, 2020, The Linux Foundation. All rights reserved.
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

#define USB_PHY_UTMI_CTRL5			0x50
#define USB_PHY_HS_PHY_CTRL_COMMON0		0x54
#define USB_PHY_HS_PHY_CTRL2			0x64
#define USB_PHY_CFG0				0x94
#define USB2PHY_PORT_POWERDOWN			0xA4
#define USB_PHY_FSEL_SEL			0xB8
#define USB2PHY_USB_PHY_M31_XCFGI_1		0xBC
#define USB2PHY_USB_PHY_M31_XCFGI_4		0xC8
#define USB2PHY_USB_PHY_M31_XCFGI_5		0xCC
#define USB2PHY_USB_PHY_M31_XCFGI_9		0xDC
#define USB2PHY_USB_PHY_M31_XCFGI_11		0xE4

#define POR_EN					BIT(1)
#define FREQ_SEL				BIT(0)
#define FSEL_VALUE				(5 << 4)
#define COMMONONN				BIT(7)
#define RETENABLEN				BIT(3)
#define USB2_SUSPEND_N_SEL			BIT(3)
#define USB2_SUSPEND_N				BIT(2)
#define USB2_UTMI_CLK_EN			BIT(1)
#define CLKCORE					BIT(1)
#define ATERESET				~BIT(0)
#define USB2_UTMI_CLK_EN			BIT(1)
#define POWER_UP				BIT(0)
#define POWER_DOWN				0
#define UTMI_PHY_OVERRIDE_EN			BIT(1)
#define XCFG_COARSE_TUNE_NUM			(2 << 0)
#define XCFG_FINE_TUNE_NUM			(1 << 3)
#define USB2_0_TX_ENABLE			BIT(2)
#define ODT_VALUE_38_02_OHM			(3 << 6)
#define HSTX_SLEW_RATE_400PS			7
#define PLL_CHARGING_PUMP_CURRENT_35UA		(3 << 3)
#define HSTX_PRE_EMPHASIS_LEVEL_0_55MA		(1)
#define HSTX_CURRENT_17_1MA_385MV		BIT(1)

struct m31usb_hsphy_priv {
	void __iomem *base;
	struct clk_bulk clks;
	struct reset_ctl phy_rst;
};

static int m31usb_hsphy_do_reset(struct m31usb_hsphy_priv *priv)
{
	int ret;

	ret = reset_assert(&priv->phy_rst);
	if (ret)
		return ret;

	udelay(10);

	ret = reset_deassert(&priv->phy_rst);
	if (ret)
		return ret;

	return 0;
}

static int m31usb_hsphy_power_on(struct phy *phy)
{
	struct m31usb_hsphy_priv *priv = dev_get_priv(phy->dev);
	int ret;

	ret = m31usb_hsphy_do_reset(priv);
	if (ret)
		return ret;

	/* Enable the PHY */
	writel(POWER_UP, priv->base + USB2PHY_PORT_POWERDOWN);
	udelay(10);

	/* Enable override ctrl */
	writel(UTMI_PHY_OVERRIDE_EN, priv->base + USB_PHY_CFG0);

	/* Enable POR*/
	writel(POR_EN, priv->base + USB_PHY_UTMI_CTRL5);
	udelay(15);
	/* Configure frequency select value*/
	writel(FREQ_SEL, priv->base + USB_PHY_FSEL_SEL);

	/* Configure refclk frequency */
	writel(COMMONONN | FSEL_VALUE | RETENABLEN,
			priv->base + USB_PHY_HS_PHY_CTRL_COMMON0);

	writel(POR_EN & ATERESET,
			priv->base + USB_PHY_UTMI_CTRL5);

	writel(USB2_SUSPEND_N_SEL | USB2_SUSPEND_N | USB2_UTMI_CLK_EN,
			priv->base + USB_PHY_HS_PHY_CTRL2);

	writel(XCFG_COARSE_TUNE_NUM | XCFG_FINE_TUNE_NUM,
			priv->base + USB2PHY_USB_PHY_M31_XCFGI_11);

	/* Adjust HSTX slew rate to 400 ps*/
	/* Adjust PLL lock Time counter for release clock to 35uA */
	/* Adjust Manual control ODT value to 38.02 Ohm */
	writel(HSTX_SLEW_RATE_400PS | PLL_CHARGING_PUMP_CURRENT_35UA |
			ODT_VALUE_38_02_OHM,
			priv->base + USB2PHY_USB_PHY_M31_XCFGI_4);

	/*
	* Enable to always turn on USB 2.0 TX driver
	* when system is in USB 2.0 HS mode
	*/
	writel(USB2_0_TX_ENABLE, priv->base + USB2PHY_USB_PHY_M31_XCFGI_1);

	/* Adjust HSTX Pre-emphasis level to 0.55mA */
	writel(HSTX_PRE_EMPHASIS_LEVEL_0_55MA,
			priv->base + USB2PHY_USB_PHY_M31_XCFGI_5);

	/*
	* Adjust HSTX Current of current-mode driver,
	* default 18.5mA * 22.5ohm = 416mV
	* 17.1mA * 22.5ohm = 385mV
	*/
	writel(HSTX_CURRENT_17_1MA_385MV,
			priv->base + USB2PHY_USB_PHY_M31_XCFGI_9);
	udelay(10);

	writel(0, priv->base + USB_PHY_UTMI_CTRL5);

	writel(USB2_SUSPEND_N | USB2_UTMI_CLK_EN,
			priv->base + USB_PHY_HS_PHY_CTRL2);
	return 0;
}

static int m31usb_hsphy_power_off(struct phy *phy)
{
	struct m31usb_hsphy_priv *priv = dev_get_priv(phy->dev);

	writel(POWER_DOWN, priv->base + USB2PHY_PORT_POWERDOWN);
	udelay(10);
	return 0;
}

static int m31usb_hsphy_probe(struct udevice *dev)
{
	struct m31usb_hsphy_priv *priv = dev_get_priv(dev);
	int ret;

	priv->base = (void *)dev_read_addr(dev);
	if ((ulong)priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = reset_get_by_name(dev, "usb2_phy_rst", &priv->phy_rst);
	if (ret)
		return ret;

	return 0;
}

static struct phy_ops m31usb_hsphy_ops = {
	.power_on = m31usb_hsphy_power_on,
	.power_off = m31usb_hsphy_power_off,
};

static const struct udevice_id m31usb_hsphy_ids[] = {
	{ .compatible = "qti,ipq5332-m31-usb-hsphy" },
	{ }
};

U_BOOT_DRIVER(m31usb_hsphy) = {
	.name		= "qca_m31usb_hsphy",
	.id		= UCLASS_PHY,
	.of_match	= m31usb_hsphy_ids,
	.ops		= &m31usb_hsphy_ops,
	.probe		= m31usb_hsphy_probe,
	.priv_auto	= sizeof(struct m31usb_hsphy_priv),
};
