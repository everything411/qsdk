/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <asm/io.h>
#include <common.h>
#include <memalign.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <clk.h>
#include <errno.h>
#include <linux/bitops.h>
#include <linux/iopoll.h>
#include <miiphy.h>
#include <i2c.h>

struct qcom_mdio_i2c_info {
	struct udevice *bus;
	struct udevice *dev;
};

/*qcom SFP PHY device addr is 0x1d and PHY addr is 0x4c*/
#define TO_QCOM_SFP_PHY_ADDR(phy_id)		(phy_id + 0x2f)

static int qcom_mdio_i2c_read_default(struct mii_dev *mii_bus, int phy_id,
					int devad, int reg)
{
	struct qcom_mdio_i2c_info *priv = mii_bus->priv;
	struct dm_i2c_ops *ops = i2c_get_ops(priv->bus);
	struct i2c_msg msgs[2];
	u8 tx[4], data[2], addr, *p;
	int ret;

	if (devad == MDIO_DEVAD_NONE)
	{
		addr = reg;
		msgs[0].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[0].flags = 0;
		msgs[0].len = 1;
		msgs[0].buf = &addr;
		msgs[1].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = sizeof(data);
		msgs[1].buf = data;
	} else {
		p = tx;
		*p++ = 0x60 | devad;
		*p++ = reg >> 8;
		*p++ = reg;

		msgs[0].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[0].flags = 0;
		msgs[0].len = sizeof(tx);
		msgs[0].buf = tx;
		msgs[1].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[1].flags = I2C_M_RD;
		msgs[1].len = sizeof(data);
		msgs[1].buf = data;
	}

	ret =  ops->xfer(priv->bus, msgs, ARRAY_SIZE(msgs));
	if (ret)
		return ret;

	return (data[0] << 8 | data[1]);
}

static int qcom_mdio_i2c_write_default(struct mii_dev *mii_bus, int phy_id,
					int devad, int reg, u16 val)
{
	struct qcom_mdio_i2c_info *priv = mii_bus->priv;
	struct dm_i2c_ops *ops = i2c_get_ops(priv->bus);
	struct i2c_msg msgs[2];
	u8 data[3], tx[4], tx1[3], *p;
	int ret, msg_len = 0;

	if (devad == MDIO_DEVAD_NONE)
	{
		p = data;
		*p++ = reg;
		*p++ = val >> 8;
		*p++ = val;

		msgs[0].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[0].flags = 0;
		msgs[0].len = p - data;
		msgs[0].buf = data;
		msg_len = 1;
	} else {
		p = tx;
		*p++ = 0x60 | devad;
		*p++ = reg >> 8;
		*p++ = reg;

		p = tx1;
		*p++ = 0x40 | devad;
		*p++ = val >> 8;
		*p++ = val;

		msgs[0].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[0].flags = 0;
		msgs[0].len = sizeof(tx);
		msgs[0].buf = tx;
		msgs[1].addr = TO_QCOM_SFP_PHY_ADDR(phy_id);
		msgs[1].flags = 0;
		msgs[1].len = sizeof(tx1);
		msgs[1].buf = tx1;
		msg_len = 2;
	}

	ret =  ops->xfer(priv->bus, msgs, msg_len);
	return ret < 0 ? ret : 0;
}


struct mii_dev *qcom_mdio_i2c_alloc(struct udevice *i2c_bus, int phy_addr)
{
	int ret = 0;
	struct mii_dev *bus;
	struct qcom_mdio_i2c_info *priv =
		malloc_cache_aligned(sizeof(struct qcom_mdio_i2c_info));
	if (!priv) {
		printf("%s: failed to alloc priv dev \n", __func__);
		return NULL;
	}

	priv->bus = i2c_bus;
	ret = dm_i2c_probe(priv->bus, TO_QCOM_SFP_PHY_ADDR(phy_addr), 0,
			&priv->dev);
	if (ret) {
		debug("%s: failed to probe i2c device addr: 0x%x, err: %d\n",
				__func__, TO_QCOM_SFP_PHY_ADDR(phy_addr), ret);
		goto free_mem0;
	}

	bus = mdio_alloc();
	if (!bus) {
		printf("%s: failed to do mdio_alloc \n", __func__);
		goto free_mem1;
	}

	bus->read = qcom_mdio_i2c_read_default;
	bus->write = qcom_mdio_i2c_write_default;
	snprintf(bus->name, sizeof(bus->name), "qcom-mdio-i2c%d-%s",
			priv->bus->seq_, priv->dev->name);
	bus->priv = priv;

	ret = mdio_register(bus);
	if (ret) {
		printf("%s: failed to register mdio device, err: %d\n",
				__func__, ret);
		goto free_mem1;
	}

	return bus;

free_mem1:
	mdio_free(bus);
	bus = NULL;
free_mem0:
	free(priv);
	priv = NULL;
	return NULL;
}
