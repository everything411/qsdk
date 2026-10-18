// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <dm.h>
#include <generic-phy.h>
#include <linux/bitops.h>
#include <asm/io.h>
#include <reset.h>
#include <clk.h>
#include <linux/delay.h>
#include <dm/device_compat.h>

/* Register offsets */
#define USB2_PHY_USB_PHY_UTMI_CTRL0		0x3c
#define USB2_PHY_USB_PHY_UTMI_CTRL5		0x50
#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0	0x54
#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1	0x58
#define USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2	0x5c
#define USB2_PHY_USB_PHY_HS_PHY_CTRL1		0x60
#define USB2_PHY_USB_PHY_HS_PHY_CTRL2		0x64
#define USB2_PHY_USB_PHY_HS_PHY_OVERRIDE_X1	0x70
#define USB2_PHY_USB_PHY_HS_PHY_TEST0		0x80
#define USB2_PHY_USB_PHY_HS_PHY_TEST1		0x84
#define USB2_PHY_USB_PHY_CFG0			0x94
#define USB2_PHY_USB_PHY_REFCLK_CTRL		0xa0
#define USB2_PHY_USB_PHY_FSEL_SEL		0xb8

/* USB2_PHY_USB_PHY_UTMI_CTRL0 bits */
#define SLEEPM					BIT(0)

/* USB2_PHY_USB_PHY_UTMI_CTRL5 bits */
#define POR					BIT(1)
#define ATERESET				BIT(0)

/* USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0 bits */
#define FSEL_MASK				GENMASK(6, 4)
#define FSEL_24MHZ				(0x2 << 4)
#define VATESTENB_MASK				GENMASK(1, 0)
#define COMMONONN				BIT(7)

/* USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1 bits */
#define VBUSVLDEXTSEL0				BIT(4)
#define PLLBTUNE				BIT(5)

/* USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2 bits */
#define VREGBYPASS				BIT(0)

/* USB2_PHY_USB_PHY_HS_PHY_CTRL1 bits */
#define VBUSVLDEXT0				BIT(0)

/* USB2_PHY_USB_PHY_HS_PHY_CTRL2 bits */
#define USB2_SUSPEND_N				BIT(2)
#define USB2_SUSPEND_N_SEL			BIT(3)

/* USB2_PHY_USB_PHY_HS_PHY_OVERRIDE_X1 bits */
#define TXPREEMPAMPTUNE0_MASK			GENMASK(7, 6)

/* USB2_PHY_USB_PHY_HS_PHY_TEST0 bits */
#define TESTDATAIN_MASK				GENMASK(7, 0)

/* USB2_PHY_USB_PHY_TEST1 bits */
#define TESTDATAOUTSEL				BIT(4)
#define TESTCLK					BIT(6)

/* USB2_PHY_USB_PHY_CFG0 bits */
#define UTMI_PHY_CMN_CTRL_OVERRIDE_EN		BIT(1)

/* USB2_PHY_USB_PHY_REFCLK_CTRL bits */
#define REFCLK_SEL_MASK				GENMASK(1, 0)
#define REFCLK_SEL_DEFAULT			(0x2 << 0)

/* USB2_PHY_USB_PHY_FSEL_SEL bits */
#define FSEL_SEL				BIT(0)

/* Special delay values */
#define DELAY_MIN_US				10
#define DELAY_MAX_US				20

/**
 * struct phy_reg_cfg - PHY register configuration entry
 * @offset: Register offset
 * @mask: Bit mask for the field
 * @value: Value to write
 * @delay_us: Delay in microseconds after write (0 = no delay)
 */
struct phy_reg_cfg {
	u32 offset;
	u32 mask;
	u32 value;
	u32 delay_us;
};

/**
 * struct phy_init_seq - PHY initialization sequence
 * @seq: Array of register configurations
 * @num_regs: Number of register configurations
 */
struct phy_init_seq {
	const struct phy_reg_cfg *seq;
	unsigned int num_regs;
};

/**
 * struct femto_phy_cfg - SoC-specific PHY configuration
 * @por_seq: Power-on-Reset sequence
 */
struct femto_phy_cfg {
	struct phy_init_seq por_seq;
};

/**
 * struct femto_usb_hs_phy - Femto USB HS PHY attributes
 * @base: iomapped memory space for PHY registers
 * @phy_resets: PHY reset controls (bulk)
 * @clks: clock bulk data
 * @cfg: SoC-specific configuration
 */
struct femto_usb_hs_phy {
	void __iomem *base;
	struct reset_ctl_bulk phy_resets;
	struct clk_bulk clks;
	const struct femto_phy_cfg *cfg;
};

/* IPQ9650 Power-on-Reset sequence (23 steps) */
static const struct phy_reg_cfg ipq9650_por_seq[] = {
	/* Step 1: Enable software override */
	{ USB2_PHY_USB_PHY_CFG0, UTMI_PHY_CMN_CTRL_OVERRIDE_EN,
	  UTMI_PHY_CMN_CTRL_OVERRIDE_EN, 0 },

	/* Step 2: Assert POR for at least 10us */
	{ USB2_PHY_USB_PHY_UTMI_CTRL5, POR, POR, DELAY_MIN_US },

	/* Step 3: Enable FSEL software override */
	{ USB2_PHY_USB_PHY_FSEL_SEL, FSEL_SEL, FSEL_SEL, 0 },

	/* Step 4: Set FSEL for 24 MHz reference clock */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0, FSEL_MASK, FSEL_24MHZ, 0 },

	/* Step 5: Set PLL bandwidth */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1, PLLBTUNE, PLLBTUNE, 0 },

	/* Step 6: Select CLKCORE as reference clock source */
	{ USB2_PHY_USB_PHY_REFCLK_CTRL, REFCLK_SEL_MASK, REFCLK_SEL_DEFAULT, 0 },

	/* Step 7: Wordinterface - hardware controlled (no register write) */

	/* Step 8: Enable external VBUS valid select */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON1, VBUSVLDEXTSEL0, VBUSVLDEXTSEL0, 0 },

	/* Step 9: Set external VBUS valid */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL1, VBUSVLDEXT0, VBUSVLDEXT0, 0 },

	/* Step 10: Set TX preemphasis to 1X */
	{ USB2_PHY_USB_PHY_HS_PHY_OVERRIDE_X1, TXPREEMPAMPTUNE0_MASK, (0x1 << 6), 0 },

	/* Step 11: Bypass internal voltage regulator */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON2, VREGBYPASS, VREGBYPASS, 0 },

	/* Step 12: Deassert ATE reset */
	{ USB2_PHY_USB_PHY_UTMI_CTRL5, ATERESET, 0, 0 },

	/* Step 13: Clear test data output select */
	{ USB2_PHY_USB_PHY_HS_PHY_TEST1, TESTDATAOUTSEL, 0, 0 },

	/* Step 14: Clear test clock */
	{ USB2_PHY_USB_PHY_HS_PHY_TEST1, TESTCLK, 0, 0 },

	/* Step 15: Clear VATE test enable */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL_COMMON0, VATESTENB_MASK, 0, 0 },

	/* Step 16: Clear test data input */
	{ USB2_PHY_USB_PHY_HS_PHY_TEST0, TESTDATAIN_MASK, 0, 0 },

	/* Step 17: Note - outputs indeterminate during reset (no register write) */

	/* Step 18: Enable suspend override select */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL2, USB2_SUSPEND_N_SEL, USB2_SUSPEND_N_SEL, 0 },

	/* Step 19: Set suspend signal active */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL2, USB2_SUSPEND_N, USB2_SUSPEND_N, 0 },

	/* Step 20: Set sleep mode inactive */
	{ USB2_PHY_USB_PHY_UTMI_CTRL0, SLEEPM, SLEEPM, 0 },

	/* Step 21: Release POR */
	{ USB2_PHY_USB_PHY_UTMI_CTRL5, POR, 0, 0 },

	/* Step 22: Disable suspend override */
	{ USB2_PHY_USB_PHY_HS_PHY_CTRL2, USB2_SUSPEND_N_SEL, 0, 0 },

	/* Step 23: Disable software override */
	{ USB2_PHY_USB_PHY_CFG0, UTMI_PHY_CMN_CTRL_OVERRIDE_EN, 0, 0 },
};

static const struct femto_phy_cfg ipq9650_phy_cfg = {
	.por_seq = {
		.seq = ipq9650_por_seq,
		.num_regs = ARRAY_SIZE(ipq9650_por_seq),
	},
};

static inline void femto_phy_write_mask(void __iomem *base, u32 offset,
					 u32 mask, u32 val)
{
	u32 reg;

	reg = readl_relaxed(base + offset);
	reg &= ~mask;
	reg |= val & mask;
	writel_relaxed(reg, base + offset);

	/* Ensure write is completed */
	readl_relaxed(base + offset);
}

/**
 * femto_phy_apply_seq() - Apply a register configuration sequence
 * @hsphy: PHY instance
 * @seq: Initialization sequence to apply
 *
 * Return: 0 on success
 */
static int femto_phy_apply_seq(struct femto_usb_hs_phy *hsphy,
				const struct phy_init_seq *seq)
{
	unsigned int i;

	for (i = 0; i < seq->num_regs; i++) {
		const struct phy_reg_cfg *cfg = &seq->seq[i];

		femto_phy_write_mask(hsphy->base, cfg->offset,
				     cfg->mask, cfg->value);

		if (cfg->delay_us)
			udelay(cfg->delay_us + 10);
	}

	return 0;
}

/**
 * femto_usb_hs_phy_power_on() - Initialize Femto USB HS PHY
 * @phy: generic PHY
 *
 * Implements the complete Power-on-Reset (POR) sequence using
 * table-based configuration.
 *
 * Return: 0 on success, negative error code on failure
 */
static int femto_usb_hs_phy_power_on(struct phy *phy)
{
	struct femto_usb_hs_phy *hsphy = dev_get_priv(phy->dev);
	int ret;

	ret = clk_enable_bulk(&hsphy->clks);
	if (ret && ret != -ENOSYS && ret != -ENOENT) {
		dev_err(phy->dev, "failed to enable clocks, %d\n", ret);
		return ret;
	}

	ret = reset_assert_bulk(&hsphy->phy_resets);
	if (ret) {
		dev_err(phy->dev, "failed to assert reset, %d\n", ret);
		goto disable_clks;
	}

	udelay(150);

	ret = reset_deassert_bulk(&hsphy->phy_resets);
	if (ret) {
		dev_err(phy->dev, "failed to deassert reset, %d\n", ret);
		goto disable_clks;
	}

	/* Apply POR sequence from configuration table */
	ret = femto_phy_apply_seq(hsphy, &hsphy->cfg->por_seq);
	if (ret)
		goto disable_clks;

	return 0;

disable_clks:
	clk_disable_bulk(&hsphy->clks);
	return ret;
}

/**
 * femto_usb_hs_phy_power_off() - Power down Femto USB HS PHY
 * @phy: generic PHY
 *
 * Return: 0 on success
 */
static int femto_usb_hs_phy_power_off(struct phy *phy)
{
	struct femto_usb_hs_phy *hsphy = dev_get_priv(phy->dev);

	reset_assert_bulk(&hsphy->phy_resets);
	clk_disable_bulk(&hsphy->clks);

	return 0;
}

static int femto_usb_hs_phy_probe(struct udevice *dev)
{
	struct femto_usb_hs_phy *hsphy = dev_get_priv(dev);
	int ret;

	hsphy->cfg = (const struct femto_phy_cfg *)dev_get_driver_data(dev);
	if (!hsphy->cfg)
		return -EINVAL;

	hsphy->base = (void *)dev_read_addr(dev);
	if ((ulong)hsphy->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	ret = clk_get_bulk(dev, &hsphy->clks);
	if (ret && ret != -ENOSYS && ret != -ENOENT) {
		dev_err(dev, "failed to get clocks, %d\n", ret);
		return ret;
	}

	ret = reset_get_bulk(dev, &hsphy->phy_resets);
	if (ret) {
		dev_err(dev, "failed to get PHY resets, %d\n", ret);
		clk_release_bulk(&hsphy->clks);
		return ret;
	}

	return 0;
}

static struct phy_ops femto_usb_hs_phy_ops = {
	.power_on = femto_usb_hs_phy_power_on,
	.power_off = femto_usb_hs_phy_power_off,
};

static const struct udevice_id femto_usb_hs_phy_ids[] = {
	{
		.compatible = "qcom,ipq9650-usb-hs-phy",
		.data = (ulong)&ipq9650_phy_cfg,
	},
	{ }
};

U_BOOT_DRIVER(qcom_femto_usb_hs_phy) = {
	.name		= "qcom-femto-usb-hs-phy",
	.id		= UCLASS_PHY,
	.of_match	= femto_usb_hs_phy_ids,
	.ops		= &femto_usb_hs_phy_ops,
	.probe		= femto_usb_hs_phy_probe,
	.priv_auto	= sizeof(struct femto_usb_hs_phy),
};
