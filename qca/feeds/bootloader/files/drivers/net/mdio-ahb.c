/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */


#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <phy.h>
#include <clk.h>
#include <linux/bitops.h>
#include <linux/sizes.h>

#define MDIO_AHB_PHY_ADDR		29
#define MDIO_AHB_BUS_NAME		"MDIO-AHB-BUS"

/* MMD to AHB address mapping offsets */
#define AHB_PHY_MMD_REGION_SIZE		0x40000  /* 256KB per MMD region */
#define AHB_PHY_MMD1_BASE		0x00000  /* MMD1 (PMAPMD) base */
#define AHB_PHY_MMD3_BASE		0x40000  /* MMD3 (PCS) base */
#define AHB_PHY_MMD7_BASE		0x80000  /* MMD7 (AN) base */
#define AHB_PHY_MMD31_BASE		0xC0000  /* MMD31 (Vendor) base */
#define AHB_PHY_REG_ALIGNMENT_SHIFT	2        /* 32-bit alignment (4 bytes) */

struct mdio_ahb_priv {
	void __iomem *base;
	size_t size;
};

/*
 * mmd_to_ahb_addr_convert() - Convert MMD device address to AHB offset
 */
static int mmd_to_ahb_addr_convert(struct mdio_ahb_priv *priv, int phy_addr,
				   int devad, u32 reg, u32 *ahb_addr)
{
	u32 ahb_base_addr = 0;
	u32 offset = 0;

	if (phy_addr != MDIO_AHB_PHY_ADDR)
		return -EOPNOTSUPP;

	switch (devad) {
	case MDIO_DEVAD_NONE:
	case MDIO_MMD_PMAPMD:
		ahb_base_addr = AHB_PHY_MMD1_BASE;
		break;
	case MDIO_MMD_PCS:
		ahb_base_addr = AHB_PHY_MMD3_BASE;
		break;
	case MDIO_MMD_AN:
		ahb_base_addr = AHB_PHY_MMD7_BASE;
		break;
	case MDIO_MMD_VEND2:
		ahb_base_addr = AHB_PHY_MMD31_BASE;
		break;
	default:
		return -EOPNOTSUPP;
	}

	/* Calculate register offset with 32-bit alignment */
	offset = (reg & 0xFFFF) << AHB_PHY_REG_ALIGNMENT_SHIFT;

	/* Bounds checking */
	if (offset >= priv->size ||
	    ahb_base_addr >= priv->size ||
	    priv->size - ahb_base_addr < offset ||
	    priv->size - ahb_base_addr - offset < sizeof(u32))
		return -EINVAL;

	*ahb_addr = ahb_base_addr + offset;

	return 0;
}

static int mdio_ahb_read(struct udevice *dev, int addr, int devad, int reg)
{
	struct mdio_ahb_priv *priv = dev_get_priv(dev);
	u32 ahb_addr;
	u32 val;
	int ret;

	if (!priv->base)
		return -EINVAL;

	ret = mmd_to_ahb_addr_convert(priv, addr, devad, reg, &ahb_addr);

	if (ret == -EOPNOTSUPP)
		return 0xFFFF;

	if (ret < 0)
		return ret;

	/* Read 32-bit value from AHB-mapped register */
	val = readl(priv->base + ahb_addr);

	/* MDIO registers are 16-bit, return the lower 16 bits */
	return val & 0xFFFF;
}

static int mdio_ahb_write(struct udevice *dev, int addr, int devad,
			  int reg, u16 val)
{
	struct mdio_ahb_priv *priv = dev_get_priv(dev);
	u32 ahb_addr;
	int ret;

	if (!priv->base)
		return -EINVAL;

	ret = mmd_to_ahb_addr_convert(priv, addr, devad, reg, &ahb_addr);

	if (ret == -EOPNOTSUPP)
		return 0;

	if (ret < 0)
		return ret;

	/* Write 16-bit value to a 32-bit AHB-mapped register */
	writel(val, priv->base + ahb_addr);

	return 0;
}

static const struct mdio_ops mdio_ahb_ops = {
	.read = mdio_ahb_read,
	.write = mdio_ahb_write,
};

static int mdio_ahb_bind(struct udevice *dev)
{
	if (ofnode_valid(dev_ofnode(dev)))
		device_set_name(dev, ofnode_get_name(dev_ofnode(dev)));

	return 0;
}

static int ipq52xx_phy_sys_clk_enable(struct udevice *dev)
{
	struct clk_bulk clks;
	int ret = -1;


	ret = clk_get_bulk(dev, &clks);
	if (ret) {
		printf("Failed to get clocks: %d\n", ret);
		return ret;
	}

	/* Enable all clocks at once */
	ret = clk_enable_bulk(&clks);
	if (ret) {
		printf("Failed to enable clocks: %d\n", ret);
		clk_release_bulk(&clks);
		return ret;
	}

	return 0;
}

static int mdio_ahb_probe(struct udevice *dev)
{
	struct mdio_ahb_priv *priv = dev_get_priv(dev);
	fdt_addr_t addr;
	fdt_size_t size;

	addr = dev_read_addr_size(dev, &size);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;
	priv->base = (void __iomem *)(uintptr_t)addr;
	priv->size = size;

	/* will check  to enable it in clock driver */
	ipq52xx_phy_sys_clk_enable(dev);

	debug("%s: Probed MDIO AHB bus at %p, size 0x%lx\n",
	      dev->name, priv->base, (unsigned long)size);

	return 0;
}

static const struct udevice_id mdio_ahb_ids[] = {
	{ .compatible = "qcom,mdio-ahb-ipq52xx" },
	{ }
};

U_BOOT_DRIVER(mdio_ahb) = {
	.name		= "mdio_ahb",
	.id		= UCLASS_MDIO,
	.of_match	= mdio_ahb_ids,
	.bind		= mdio_ahb_bind,
	.probe		= mdio_ahb_probe,
	.ops		= &mdio_ahb_ops,
	.priv_auto	= sizeof(struct mdio_ahb_priv),
};
