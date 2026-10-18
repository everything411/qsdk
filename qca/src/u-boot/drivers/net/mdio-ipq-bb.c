/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <linux/bitops.h>
#include <linux/iopoll.h>
#include <asm-generic/gpio.h>
#include <miiphy.h>
#include <phy.h>

#define MAX_MDIO_BB_DEV		(2)

struct ipq_bb_mdio_gpios {
	struct gpio_desc mdc;
	struct gpio_desc mdio;
};

static int dev_idx = 0;

static int ipq_bb_mdio_active(struct bb_miiphy_bus *bus)
{
	struct ipq_bb_mdio_gpios *priv = bus->priv;
	dm_gpio_clrset_flags(&priv->mdio,
			GPIOD_MASK_DIR,	GPIOD_IS_OUT);
	dm_gpio_set_value(&priv->mdio, 1);
	return 0;
}

static int ipq_bb_mdio_tristate(struct bb_miiphy_bus *bus)
{
	struct ipq_bb_mdio_gpios *priv = bus->priv;
	dm_gpio_clrset_flags(&priv->mdio,
			GPIOD_MASK_DIR, GPIOD_IS_IN);
	dm_gpio_set_value(&priv->mdio, 0);
	return 0;
}

static int ipq_bb_set_mdio(struct bb_miiphy_bus *bus, int v)
{
	struct ipq_bb_mdio_gpios *priv = bus->priv;
	dm_gpio_set_value(&priv->mdio, v);
	return 0;
}

static int ipq_bb_get_mdio(struct bb_miiphy_bus *bus, int *v)
{
	struct ipq_bb_mdio_gpios *priv  = bus->priv;
	*v = dm_gpio_get_value(&priv->mdio);
	return 0;
}

static int ipq_bb_set_mdc(struct bb_miiphy_bus *bus, int v)
{
	struct ipq_bb_mdio_gpios *priv = bus->priv;
	dm_gpio_set_value(&priv->mdc, v);
	return 0;
}

static int ipq_bb_delay(struct bb_miiphy_bus *bus)
{
	udelay(1);
	return 0;
}

struct bb_miiphy_bus bb_miiphy_buses[MAX_MDIO_BB_DEV] = {
	{
		.name = "ipq_mdio_bb0",
		.mdio_active = ipq_bb_mdio_active,
		.mdio_tristate = ipq_bb_mdio_tristate,
		.set_mdio = ipq_bb_set_mdio,
		.get_mdio = ipq_bb_get_mdio,
		.set_mdc = ipq_bb_set_mdc,
		.delay = ipq_bb_delay,
	}, {
		.name = "ipq_mdio_bb1",
		.mdio_active = ipq_bb_mdio_active,
		.mdio_tristate = ipq_bb_mdio_tristate,
		.set_mdio = ipq_bb_set_mdio,
		.get_mdio = ipq_bb_get_mdio,
		.set_mdc = ipq_bb_set_mdc,
		.delay = ipq_bb_delay,
	},
};

int bb_miiphy_buses_num = MAX_MDIO_BB_DEV;

int ipq_bb_mdio_read(struct udevice *dev, int addr, int devad, int reg)
{
	struct mdio_perdev_priv *pdata = dev_get_uclass_priv(dev);
	return bb_miiphy_read(pdata->mii_bus, addr, devad, reg);
}

int ipq_bb_mdio_write(struct udevice *dev, int addr, int devad, int reg,
		u16 val)
{
	struct mdio_perdev_priv *pdata = dev_get_uclass_priv(dev);
	return bb_miiphy_write(pdata->mii_bus, addr, devad, reg, val);
}

static const struct mdio_ops ipq_bb_mdio_ops = {
	.read = ipq_bb_mdio_read,
	.write = ipq_bb_mdio_write,
};

static int ipq_bb_mdio_probe(struct udevice *dev)
{
	struct ipq_bb_mdio_gpios *priv = dev_get_priv(dev);
	int ret = 0;

	if (dev_idx >= MAX_MDIO_BB_DEV) {
		ret = -ENODEV;
		return ret;
	}

	ret = gpio_request_by_name(dev, "mdc-gpio", 0, &priv->mdc,
			GPIOD_IS_OUT);
	if (ret)
		return ret;

	ret = gpio_request_by_name(dev, "mdio-gpio", 0, &priv->mdio,
			GPIOD_IS_OUT);
	if (ret)
		return ret;

	bb_miiphy_buses[dev_idx].priv = priv;
	snprintf(bb_miiphy_buses[dev_idx].name,
			sizeof(bb_miiphy_buses[dev_idx].name),
			"ipq_mdio_bb%d", dev_idx);
	device_set_name(dev, bb_miiphy_buses[dev_idx].name);

	dev_idx++;
	return 0;
}

static const struct udevice_id ipq_bb_mdio_ids[] = {
	{ .compatible = "qti,ipq-bb-mdio", },
	{ }
};

U_BOOT_DRIVER(ipq_bb_mdio) = {
	.name           = "ipq_bb_mdio",
	.id             = UCLASS_MDIO,
	.of_match       = ipq_bb_mdio_ids,
	.probe          = ipq_bb_mdio_probe,
	.ops            = &ipq_bb_mdio_ops,
	.priv_auto	= sizeof(struct ipq_bb_mdio_gpios),
};
