/* Copyright (c) 2015, 2017, 2020, The Linux Foundation. All rights reserved.
 *
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
#include <clk.h>
#include <clk-uclass.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <generic-phy.h>
#include <malloc.h>
#include <reset.h>
#include <asm/io.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/delay.h>

#define PIPE_CLK_DELAY_MIN_US			5000
#define PIPE_CLK_DELAY_MAX_US			5100
#define CDR_CTRL_REG_1				0x80
#define CDR_CTRL_REG_2				0x84
#define CDR_CTRL_REG_3				0x88
#define CDR_CTRL_REG_4				0x8C
#define CDR_CTRL_REG_5				0x90
#define CDR_CTRL_REG_6				0x94
#define CDR_CTRL_REG_7				0x98
#define SSCG_CTRL_REG_1				0x9c
#define SSCG_CTRL_REG_2				0xa0
#define SSCG_CTRL_REG_3				0xa4
#define SSCG_CTRL_REG_4				0xa8
#define SSCG_CTRL_REG_5				0xac
#define SSCG_CTRL_REG_6				0xb0
#define PCS_INTERNAL_CONTROL_2			0x2d8

#define PHY_MODE_FIXED				0x1

/* IPQ5332 specific registers */
#define PHY_CFG_PLLCFG				0x220
#define PHY_CFG_EIOS_DTCT_REG			0x3E4
#define PHY_CFG_GEN3_ALIGN_HOLDOFF_TIME		0x3E8

/* PCIe mode control register values */
#define TCSR_2LANE_MODE				0x0
#define TCSR_2PORT_MODE				0x1

enum qca_uni_pcie_phy_type {
	PHY_TYPE_PCIE,
	PHY_TYPE_PCIE_GEN2,
	PHY_TYPE_PCIE_GEN3,
};

struct qca_uni_pcie_phy {
	struct phy phy;
	struct udevice *dev;
	unsigned int phy_type;
	struct clk *pipe_clk;
	struct clk *lane_m_clk;
	struct clk *lane_s_clk;
	struct clk *phy_ahb_clk;
	struct reset_ctl *res_phy;
	struct reset_ctl *res_phy_phy;
	struct reset_ctl res_phy_ahb;
	struct reset_ctl_bulk rsts;
	struct clk_bulk clks;
	u32 is_phy_gen3;
	u32 mode;
	u32 is_x2;
	void __iomem *reg_base;
        struct regmap *phy_mux_map;
        u32 phy_mux_reg;
	bool phy_ahb_shared_reset;
};

static int qca_uni_pcie_phy_power_off(struct phy *x)
{
	int ret;
	struct qca_uni_pcie_phy *phy = dev_get_priv(x->dev);

	ret = reset_assert_bulk(&phy->rsts);
	if (ret) {
		dev_err(x->dev, "failed to assert resets (%d)\n", ret);
		return ret;
	}

	udelay(500);

	return 0;
}

static void qca_uni_pcie_phy_init(struct qca_uni_pcie_phy *phy)
{
	int loop = 0;
	void __iomem *reg = phy->reg_base;

	while (loop < 2) {
		reg += (loop * 0x800);
		if (phy->is_phy_gen3) {
			writel(0x30, reg + PHY_CFG_PLLCFG);
			writel(0x53EF, reg + PHY_CFG_EIOS_DTCT_REG);
			writel(0xCF, reg + PHY_CFG_GEN3_ALIGN_HOLDOFF_TIME);
		} else {
			/*set frequency initial value*/
			writel(0x1cb9, reg + SSCG_CTRL_REG_4);
			writel(0x023a, reg + SSCG_CTRL_REG_5);
			/*set spectrum spread count*/
			writel(0xd360, reg + SSCG_CTRL_REG_3);
			if (phy->mode == PHY_MODE_FIXED) {
				/*set fstep*/
				writel(0x0, reg + SSCG_CTRL_REG_1);
				writel(0x0, reg + SSCG_CTRL_REG_2);
			} else {
				/*set fstep*/
				writel(0x1, reg + SSCG_CTRL_REG_1);
				writel(0xeb, reg + SSCG_CTRL_REG_2);
				/*set FLOOP initial value*/
				writel(0x3f9, reg + CDR_CTRL_REG_4);
				writel(0x1c9, reg + CDR_CTRL_REG_5);
				/*set upper boundary level*/
				writel(0x419, reg + CDR_CTRL_REG_2);
				/*set fixed offset*/
				writel(0x200, reg + CDR_CTRL_REG_1);
				writel(0xf101, reg + PCS_INTERNAL_CONTROL_2);
			}
		}

		if (phy->is_x2)
			loop += 1;
		else
			break;
	}
}

static int qca_uni_pcie_phy_power_on(struct phy *x)
{
	struct qca_uni_pcie_phy *phy = dev_get_priv(x->dev);
	int ret;

	ret = reset_assert_bulk(&phy->rsts);
	if (ret) {
		dev_err(x->dev, "failed to assert resets (%d)\n", ret);
		return ret;
	}

	udelay(500);

	ret = reset_deassert_bulk(&phy->rsts);
	if (ret) {
		dev_err(x->dev, "failed to deassert resets (%d)\n", ret);
		return ret;
	}

	udelay(500);

	ret = clk_enable_bulk(&phy->clks);
	if (ret) {
		dev_err(x->dev, "failed to enable clocks (%d)\n", ret);
		return ret;
	}

	udelay(500);

	qca_uni_pcie_phy_init(phy);

	return 0;
}

static int  phy_mux_sel(struct qca_uni_pcie_phy *phy, unsigned int mode)
{
	phys_addr_t lane_reg = 0;

	dev_read_u32(phy->dev, "qti,phy-mux-regs", (unsigned int *)&lane_reg);

	if (lane_reg)
		writel(mode, lane_reg);

	return 0;
}

static int qca_uni_pcie_get_resources(struct qca_uni_pcie_phy *phy)
{
	int ret;
	const char *name;

	phy->reg_base = (void __iomem *)dev_read_addr(phy->dev);
	if (IS_ERR(phy->reg_base))
		return PTR_ERR(phy->reg_base);

	phy->phy_ahb_shared_reset = dev_read_bool(phy->dev,
						"phy-ahb-shared-reset");

	phy->is_x2 = dev_read_u32_default(phy->dev, "x2", 0);

	ret = reset_get_bulk(phy->dev, &phy->rsts);
	if (ret) {
		dev_err(phy->dev, "failed to get resets (%d)\n", ret);
		return ret;
	}

	ret = clk_get_bulk(phy->dev, &phy->clks);
	if (ret) {
		dev_err(phy->dev, "failed to get clocks (%d)\n", ret);
		return ret;
	}

	if(phy->phy_ahb_shared_reset)
		if (reset_get_by_name(phy->dev, "phy_ahb", &phy->res_phy_ahb))
			dev_warn(phy->dev, "failed to get nocsr reset\n");

	name = dev_read_string(phy->dev, "phy-type");
	if (name) {
		if (!strcmp(name, "gen3")) {
			phy->phy_type = PHY_TYPE_PCIE_GEN3;
			phy->is_phy_gen3 = 1;
		} else if (!strcmp(name, "gen2"))
			phy->phy_type = PHY_TYPE_PCIE_GEN2;
		else if (!strcmp(name, "gen1"))
			phy->phy_type = PHY_TYPE_PCIE;
	} else {
		dev_err(phy->dev, "%s, unknown gen type\n", __func__);
		return -EINVAL;
	}

	phy->mode = dev_read_u32_default(phy->dev, "mode-fixed", 2);

	if (dev_read_bool(phy->dev, "qti,multiplexed-phy"))
		phy_mux_sel(phy, TCSR_2PORT_MODE);
	else
		phy_mux_sel(phy, TCSR_2LANE_MODE);

	return 0;
}

static const struct udevice_id qca_uni_pcie_id_table[] = {
	{ .compatible = "qca,uni-pcie-phy", .data = (ulong)PHY_TYPE_PCIE},
	{ .compatible = "qca,uni-pcie-phy-gen2",
		.data = (ulong)PHY_TYPE_PCIE_GEN2},
	{ .compatible = "qca,uni-pcie-phy-gen3",
		.data = (ulong)PHY_TYPE_PCIE_GEN3},
	{ /* Sentinel */ }
};

static const struct phy_ops pcie_ops = {
	.power_on	= qca_uni_pcie_phy_power_on,
	.power_off	= qca_uni_pcie_phy_power_off,
};

static int qca_uni_pcie_probe(struct udevice *dev)
{
	struct qca_uni_pcie_phy  *phy = dev_get_priv(dev);;
	int ret;

	phy->dev = dev;

	ret = qca_uni_pcie_get_resources(phy);
	if (ret < 0) {
		dev_err(dev, "failed to get resources: %d\n", ret);
		return ret;
	}

	return 0;
}

U_BOOT_DRIVER(qcom_uni_pcie) = {
	.name		= "qcom-uni-pcie-phy",
	.id		= UCLASS_PHY,
	.of_match	= qca_uni_pcie_id_table,
	.ops		= &pcie_ops,
	.probe		= qca_uni_pcie_probe,
	.priv_auto	= sizeof(struct qca_uni_pcie_phy),
};
