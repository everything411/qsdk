/* SPDX-License-Identifier: GPL-2.0 */
/*
 * MDIO I2C bridge
 *
 * Copyright (C) 2015 Russell King
 */
#ifndef MDIO_I2C_H
#define MDIO_I2C_H

struct device;
struct i2c_adapter;
struct mii_bus;

enum mdio_i2c_proto {
	MDIO_I2C_NONE,
	MDIO_I2C_MARVELL_C22,
	MDIO_I2C_C45,
	MDIO_I2C_ROLLBALL,
	MIDO_I2C_QCOM,
};

struct qcom_mdio_i2c_data {
	void __iomem	*membase[2];
	void __iomem *eth_ldo_rdy[3];
	int clk_div;
	bool force_c22;
	struct gpio_descs *reset_gpios;
	void (*preinit)(struct mii_bus *bus);
	u32 (*sw_read)(struct mii_bus *bus, u32 reg);
	void (*sw_write)(struct mii_bus *bus, u32 reg, u32 val);
	void *clk[5];
	struct i2c_adapter *i2c;
};

struct mii_bus *mdio_i2c_alloc(struct device *parent, struct i2c_adapter *i2c,
			       enum mdio_i2c_proto protocol);

#endif
