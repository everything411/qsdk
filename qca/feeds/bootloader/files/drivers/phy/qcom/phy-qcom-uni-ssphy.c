/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include <dm.h>
#include <generic-phy.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <reset.h>
#include <clk.h>
#include <linux/delay.h>

#define PCIE_USB_COMBO_PHY_CFG_MISC1		0x0214
#define PCIE_USB_COMBO_PHY_CFG_RX_AFE_2		0x07C4
#define PCIE_USB_COMBO_PHY_CFG_RX_DLF_DEMUX_2	0x07E8

#define TCSR_USB_PCIE_SEL_USB			0x1

#define APB_REG_UPHY_RX_RESCAL_CODE		(16 << 8)
#define APB_REG_UPHY_RX_AFE_CAP1		(7 << 4)
#define APB_REG_UPHY_RX_AFE_RES1		(6 << 0)
#define APB_REG_UPHY_RXD_BIT_WIDTH		(2 << 0)
#define APB_REG_UPHY_RX_PLOOP_GAIN		(4 << 4)
#define APB_REG_UPHY_RX_DLF_RATE		(1 << 8)
#define APB_UPHY_RX_PLOOP_EN			(1 << 12)
#define APB_REG_UPHY_RX_CDR_EN			(1 << 13)
#define APB_REG_FLOOP_GAIN			(3 << 0)

struct qti_uni_ssphy_priv {
	void __iomem *base;
	struct clk_bulk clks;
	struct reset_ctl phy_rst;
};

static int qti_uni_ssphy_do_reset(struct qti_uni_ssphy_priv *priv)
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

static int qti_uni_ssphy_power_on(struct phy *phy)
{
	struct qti_uni_ssphy_priv *priv = dev_get_priv(phy->dev);
	int ret;

	ret = qti_uni_ssphy_do_reset(priv);
	if (ret)
		return ret;

	ret = clk_enable_bulk(&priv->clks);
	if (ret)
		return ret;
	udelay(50);

	writel(APB_REG_UPHY_RX_RESCAL_CODE | APB_REG_UPHY_RX_AFE_CAP1 |
		APB_REG_UPHY_RX_AFE_RES1,
		priv->base + PCIE_USB_COMBO_PHY_CFG_RX_AFE_2);
	writel(APB_REG_UPHY_RXD_BIT_WIDTH | APB_REG_UPHY_RX_PLOOP_GAIN |
		APB_REG_UPHY_RX_DLF_RATE | APB_UPHY_RX_PLOOP_EN |
		APB_REG_UPHY_RX_CDR_EN,
		priv->base + PCIE_USB_COMBO_PHY_CFG_RX_DLF_DEMUX_2);
	writel(APB_REG_FLOOP_GAIN, priv->base + PCIE_USB_COMBO_PHY_CFG_MISC1);
	return 0;
}

static int qti_uni_ssphy_power_off(struct phy *phy)
{
	struct qti_uni_ssphy_priv *priv = dev_get_priv(phy->dev);
	int ret;

	ret = reset_assert(&priv->phy_rst);
	if (ret)
		return ret;

	return ret;
}

static int qti_uni_ssphy_probe(struct udevice *dev)
{
	struct qti_uni_ssphy_priv *priv = dev_get_priv(dev);
	int ret;
	u32 phy_mux_reg;

	priv->base = (void *)dev_read_addr(dev);
	if ((ulong)priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_bulk(dev, &priv->clks);
	if (ret && ret != -ENOSYS && ret != -ENOENT)
		return ret;

	ret = reset_get_by_name(dev, "usb3_phy_rst", &priv->phy_rst);
	if (ret) {
		clk_release_bulk(&priv->clks);
		return ret;
	}

	if (dev_read_bool(dev, "qcom,multiplexed-phy"))
	{
		if (!dev_read_u32u(dev, "qcom,phy-mux-regs", &phy_mux_reg)) {
			writel(TCSR_USB_PCIE_SEL_USB, (uintptr_t)phy_mux_reg);
			udelay(100);
		}
	}

	return 0;
}

static struct phy_ops qti_uni_ssphy_ops = {
	.power_on = qti_uni_ssphy_power_on,
	.power_off = qti_uni_ssphy_power_off,
};

static const struct udevice_id qti_uni_ssphy_ids[] = {
	{ .compatible = "qcom,ipq5332-uni-ssphy" },
	{ }
};

U_BOOT_DRIVER(qti_uni_ssphy) = {
	.name		= "qti_uni_ssphy",
	.id		= UCLASS_PHY,
	.of_match	= qti_uni_ssphy_ids,
	.ops		= &qti_uni_ssphy_ops,
	.probe		= qti_uni_ssphy_probe,
	.priv_auto	= sizeof(struct qti_uni_ssphy_priv),
};
