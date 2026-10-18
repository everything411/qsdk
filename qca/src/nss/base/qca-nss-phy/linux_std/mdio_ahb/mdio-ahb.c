/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/module.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/phy.h>
#include <linux/clk.h>
#include "mdio-ahb.h"

/* Private data for the MDIO-AHB bus */
struct mdio_ahb_priv {
	void __iomem *base;
	size_t size;
	struct device *dev;
};

/* MMD to AHB address mapping offsets based on hardware specification
 * Each MMD region is 256KB (0x40000 bytes) apart in the address space
 * Registers within each region are 32-bit aligned (4 bytes per register)
 */
#define AHB_PHY_MMD_REGION_SIZE			0x40000  /* 256KB per MMD region */
#define AHB_PHY_MMD1_BASE			0x00000  /* MMD1 (PMAPMD) base */
#define AHB_PHY_MMD3_BASE			0x40000  /* MMD3 (PCS) base */
#define AHB_PHY_MMD7_BASE			0x80000  /* MMD7 (AN) base */
#define AHB_PHY_MMD31_BASE			0xC0000  /* MMD31 (Vendor) base */
#define AHB_PHY_REG_ALIGNMENT_SHIFT		2        /* 32-bit alignment (4 bytes) */

/**
 * mmd_to_ahb_addr_convert() - Convert MMD device address to AHB offset
 * @bus: MDIO bus
 * @phy_addr: PHY address
 * @devad: MDIO MMD device address
 * @reg: Register offset within the MMD
 * @ahb_addr: Output parameter for the calculated AHB address
 *
 * This function maps MDIO MMD device addresses to their corresponding
 * AHB base addresses according to the hardware specification.
 *
 * Return: 0 on success, -EOPNOTSUPP for unsupported operations, -EINVAL for out of bounds
 */
static int mmd_to_ahb_addr_convert(struct mii_bus *bus, int phy_addr, int devad, u32 reg, u32 *ahb_addr)
{
	struct mdio_ahb_priv *priv = NULL;
	u32 ahb_base_addr = 0, offset = 0;

	if (!bus || !bus->priv)
		return -EINVAL;

	priv = bus->priv;

	/* Only support the specific PHY address for built-in PHY */
	if (phy_addr != MDIO_AHB_PHY_ADDR)
		return -EOPNOTSUPP;

	switch (devad) {
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
		/* Unsupported MMD device */
		return -EOPNOTSUPP;
	}

	/* Calculate register offset with 32-bit alignment */
	offset = (reg & 0xFFFF) << AHB_PHY_REG_ALIGNMENT_SHIFT;

	/* Bounds checking: ensure no overflow and access is within mapped region */
	if (offset >= priv->size ||
	    ahb_base_addr >= priv->size ||
	    priv->size - ahb_base_addr < offset ||
	    priv->size - ahb_base_addr - offset < sizeof(u32))
		return -EINVAL;

	/* Calculate final AHB address */
	*ahb_addr = ahb_base_addr + offset;

	return 0;
}

/**
 * mdio_ahb_read_c45() - Read from PHY register via AHB interface
 * @bus: MDIO bus
 * @phy_addr: PHY address
 * @devad: MDIO MMD device address
 * @reg: Register offset
 *
 * Return: Register value on success, negative error code on failure
 */
static int mdio_ahb_read_c45(struct mii_bus *bus, int phy_addr, int devad, int reg)
{
	struct mdio_ahb_priv *priv;
	u32 ahb_addr;
	u32 val;
	int ret;

	if (!bus || !bus->priv)
		return -EINVAL;

	priv = bus->priv;

	if (!priv->base)
		return -EINVAL;

	ret = mmd_to_ahb_addr_convert(bus, phy_addr, devad, reg, &ahb_addr);
	/* Return 0xFFFF for unsupported PHY addresses/MMD devices to indicate no device present */
	if (ret == -EOPNOTSUPP)
		return 0xFFFF;
	/* Return error for other failures (e.g., out of bounds access) */
	if (ret < 0)
		return ret;

	/* Read 32-bit value from AHB-mapped register */
	val = readl(priv->base + ahb_addr);

	/* MDIO registers are 16-bit, return the lower 16 bits */
	return val & 0xFFFF;
}

/**
 * mdio_ahb_write_c45() - Write to PHY register via AHB interface
 * @bus: MDIO bus
 * @phy_addr: PHY address
 * @devad: MDIO MMD device address
 * @reg: Register offset
 * @val: Value to write
 *
 * Return: 0 on success, negative error code on failure
 */
static int mdio_ahb_write_c45(struct mii_bus *bus, int phy_addr, int devad, int reg, u16 val)
{
	struct mdio_ahb_priv *priv;
	u32 ahb_addr;
	int ret;

	if (!bus || !bus->priv)
		return -EINVAL;

	priv = bus->priv;

	if (!priv->base)
		return -EINVAL;

	ret = mmd_to_ahb_addr_convert(bus, phy_addr, devad, reg, &ahb_addr);
	/* If the return value is -EOPNOTSUPP, do not treat it as an error.
	 * Return success (0) to avoid breaking normal kernel flow.
	 */
	if (ret == -EOPNOTSUPP)
		return 0;
	/* Return error for other failures (e.g., out of bounds access) */
	if (ret < 0)
		return ret;

	/* Write 16-bit value to a 32-bit AHB-mapped register */
	writel(val, priv->base + ahb_addr);

	return 0;
}

/**
 * mdio_ahb_iounmap() - Unmap the AHB register space
 * @bus: The MDIO-AHB bus to clean up
 *
 * This function unmaps the I/O memory region associated with the MDIO-AHB bus.
 */
static void mdio_ahb_iounmap(struct mii_bus *bus)
{
	struct mdio_ahb_priv *priv;

	if (!bus || !bus->priv)
		return;

	priv = bus->priv;
	if (priv->base) {
		iounmap(priv->base);
		priv->base = NULL;
	}
}

/**
 * mdio_ahb_bus_register() - Allocate and register an MDIO-AHB bus
 * @base: Physical base address of the AHB PHY register space
 * @size: Size of the AHB PHY register space
 * @parent: Parent device for the MDIO bus
 *
 * This function allocates, initializes and registers an MDIO bus that
 * translates MDIO C45 operations to AHB memory-mapped register accesses.
 *
 * Return: Pointer to registered mii_bus on success, ERR_PTR on failure
 */
static struct mii_bus *mdio_ahb_bus_register(phys_addr_t base, size_t size, struct device *parent)
{
	struct mii_bus *bus;
	struct mdio_ahb_priv *priv;
	int ret;

	if (!size || !base)
		return ERR_PTR(-EINVAL);

	/* Allocate MDIO bus with private data */
	bus = mdiobus_alloc_size(sizeof(*priv));
	if (!bus)
		return ERR_PTR(-ENOMEM);

	priv = bus->priv;

	/* Map the AHB register space */
	priv->base = ioremap(base, size);
	if (!priv->base) {
		ret = -ENOMEM;
		goto err_free_bus;
	}

	priv->size = size;
	priv->dev = parent;

	/* Configure MDIO bus */
	bus->name = MDIO_AHB_BUS_NAME;
	bus->read_c45 = mdio_ahb_read_c45;
	bus->write_c45 = mdio_ahb_write_c45;
	bus->parent = parent;

	snprintf(bus->id, MII_BUS_ID_SIZE, MDIO_AHB_BUS_NAME);

	/* Register MDIO bus */
	ret = mdiobus_register(bus);
	if (ret) {
		goto err_iounmap;
	}

	return bus;

err_iounmap:
	mdio_ahb_iounmap(bus);
err_free_bus:
	mdiobus_free(bus);
	return ERR_PTR(ret);
}

/* Driver private data for platform device */
struct mdio_ahb_driver_data {
	struct mii_bus *bus;
	struct clk *mdio_gephy_ahb_clk;
	struct clk *gephy_sys_clk;
};

/* MDIO GEPHY AHB clock rate: 100MHz as per hardware specification */
#define MDIO_GEPHY_AHB_CLK_RATE			100000000

/**
 * mdio_ahb_clk_get - Get a single optional clock resource
 * @pdev: Platform device
 * @clk_ptr: Output pointer to store the clock handle
 * @name: Clock name as defined in DTS clock-names
 *
 * Wraps devm_clk_get_optional(): returns NULL if the clock is absent
 * in DTS (not an error), or an ERR_PTR on a real failure.
 *
 * Return: 0 on success, negative error code on failure
 */
static int mdio_ahb_clk_get(struct platform_device *pdev,
			    struct clk **clk_ptr, const char *name)
{
	*clk_ptr = devm_clk_get_optional(&pdev->dev, name);
	if (IS_ERR(*clk_ptr)) {
		dev_err(&pdev->dev, "Failed to get %s: %ld\n",
			name, PTR_ERR(*clk_ptr));
		return PTR_ERR(*clk_ptr);
	}
	if (*clk_ptr)
		dev_dbg(&pdev->dev, "Got clock: %s\n", name);
	return 0;
}

/**
 * mdio_ahb_clock_deinit() - Deinit all clocks for the MDIO-AHB bus
 * @drv_data: Driver private data
 */
static void mdio_ahb_clock_deinit(struct mdio_ahb_driver_data *drv_data)
{
	if (drv_data->gephy_sys_clk)
		clk_disable_unprepare(drv_data->gephy_sys_clk);
	if (drv_data->mdio_gephy_ahb_clk)
		clk_disable_unprepare(drv_data->mdio_gephy_ahb_clk);
}

/**
 * mdio_ahb_clock_init() - Init all clocks for the MDIO-AHB bus
 * @pdev: Platform device
 * @drv_data: Driver private data to store clock handles
 *
 * This function retrieves and enables the clocks required for MDIO-AHB operation.
 * The clock names "mdio_gephy_ahb_clk" and "gephy_sys_clk" must be defined in
 * the device tree clock-names property for the qcom,mdio-ahb-ipq52xx node.
 *
 * Example DTS configuration:
 *   mdio_ahb: mdio@... {
 *       compatible = "qcom,mdio-ahb-ipq52xx";
 *       clocks = <&gcc GCC_MDIO_GEPHY_AHB_CLK>, <&gcc GCC_GEPHY_SYS_CLK>;
 *       clock-names = "mdio_gephy_ahb_clk", "gephy_sys_clk";
 *       ...
 *   };
 *
 * Return: 0 on success, negative error code on failure
 */
static int mdio_ahb_clock_init(struct platform_device *pdev,
			       struct mdio_ahb_driver_data *drv_data)
{
	int ret;

	ret = mdio_ahb_clk_get(pdev, &drv_data->mdio_gephy_ahb_clk,
				"mdio_gephy_ahb_clk");
	if (ret)
		return ret;

	if (drv_data->mdio_gephy_ahb_clk) {
		ret = clk_set_rate(drv_data->mdio_gephy_ahb_clk, MDIO_GEPHY_AHB_CLK_RATE);
		if (ret) {
			dev_err(&pdev->dev, "Failed to set mdio_gephy_ahb_clk as 100M: %d\n", ret);
			goto err_out;
		}
		ret = clk_prepare_enable(drv_data->mdio_gephy_ahb_clk);
		if (ret) {
			dev_err(&pdev->dev, "Failed to enable mdio_gephy_ahb_clk: %d\n", ret);
			goto err_disable_ahb_clk;
		}
	}

	ret = mdio_ahb_clk_get(pdev, &drv_data->gephy_sys_clk,
				"gephy_sys_clk");
	if (ret)
		goto err_disable_ahb_clk;
	if (drv_data->gephy_sys_clk) {
		/*
		 * Note: gephy_sys_clk rate is not explicitly configured here.
		 * It is expected to be set to the correct frequency by the GCC
		 * clock driver based on hardware requirements. Verify that the
		 * default rate matches the PHY SYS clock specification.
		 */
		ret = clk_prepare_enable(drv_data->gephy_sys_clk);
		if (ret) {
			dev_err(&pdev->dev, "Failed to enable gephy_sys_clk: %d\n", ret);
			goto err_disable_ahb_clk;
		}
	}

	return 0;

err_disable_ahb_clk:
	if (drv_data->mdio_gephy_ahb_clk)
		clk_disable_unprepare(drv_data->mdio_gephy_ahb_clk);
err_out:
	return ret;
}

/**
 * mdio_ahb_probe() - Platform driver probe function
 * @pdev: Platform device
 *
 * This function is called when the platform device is registered.
 * It allocates and registers the MDIO-AHB bus.
 *
 * Return: 0 on success, negative error code on failure
 */
static int mdio_ahb_probe(struct platform_device *pdev)
{
	struct mdio_ahb_driver_data *drv_data;
	struct mii_bus *bus;
	struct resource *res;
	phys_addr_t base_addr;
	resource_size_t reg_size;
	int ret;

	/* Allocate driver private data */
	drv_data = devm_kzalloc(&pdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;
	/* Init all clocks of mdio ahb */
	ret = mdio_ahb_clock_init(pdev, drv_data);
	if (ret) {
		dev_err(&pdev->dev, "Failed to initialize MDIO AHB clock: %d\n", ret);
		return ret;
	}

	/* Get memory resource from device tree */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "Failed to get memory resource\n");
		ret = -ENODEV;
		goto err_clock_deinit;
	}

	base_addr = res->start;
	reg_size = resource_size(res);

	dev_info(&pdev->dev, "Probing MDIO-AHB bus at 0x%llx, size 0x%llx\n",
		 (unsigned long long)base_addr, (unsigned long long)reg_size);

	/* Allocate and register MDIO-AHB bus */
	bus = mdio_ahb_bus_register(base_addr, reg_size, &pdev->dev);
	if (IS_ERR(bus)) {
		dev_err(&pdev->dev, "Failed to register MDIO-AHB bus: %ld\n", PTR_ERR(bus));
		ret = PTR_ERR(bus);
		goto err_clock_deinit;
	}

	drv_data->bus = bus;

	/* Store driver data in platform device */
	platform_set_drvdata(pdev, drv_data);

	dev_info(&pdev->dev, "MDIO-AHB bus registered successfully\n");

	return 0;

err_clock_deinit:
	mdio_ahb_clock_deinit(drv_data);
	return ret;
}

/**
 * mdio_ahb_remove() - Platform driver remove function
 * @pdev: Platform device
 *
 * This function is called when the platform device is removed.
 * It unregisters and frees the MDIO-AHB bus.
 *
 * Return: 0 on success
 */
static int mdio_ahb_remove(struct platform_device *pdev)
{
	struct mdio_ahb_driver_data *drv_data = platform_get_drvdata(pdev);
	struct mii_bus *bus;

	if (!drv_data)
		return 0;

	bus = drv_data->bus;

	/*
	 * Unregister and free the bus.
	 * The null check for 'bus' is defensive programming: if probe failed
	 * after clock init but before bus registration, bus will be NULL.
	 * In that case, we still need to call mdio_ahb_clock_deinit() below.
	 */
	if (bus) {
		mdiobus_unregister(bus);
		mdio_ahb_iounmap(bus);
		mdiobus_free(bus);
	}

	mdio_ahb_clock_deinit(drv_data);

	dev_info(&pdev->dev, "MDIO-AHB bus removed\n");

	return 0;
}

/* Device tree match table */
static const struct of_device_id mdio_ahb_of_match[] = {
	{ .compatible = "qcom,mdio-ahb-ipq52xx" },
	{ }
};
MODULE_DEVICE_TABLE(of, mdio_ahb_of_match);

/* Platform driver structure */
static struct platform_driver mdio_ahb_driver = {
	.probe = mdio_ahb_probe,
	.remove = mdio_ahb_remove,
	.driver = {
		.name = "mdio-ahb",
		.of_match_table = mdio_ahb_of_match,
	},
};

module_platform_driver(mdio_ahb_driver);

MODULE_DESCRIPTION("mdio ahb bus driver");
MODULE_LICENSE("Dual BSD/GPL");
