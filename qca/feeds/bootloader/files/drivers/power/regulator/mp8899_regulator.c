// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * MP8899 I2C regulator driver
 *
 * Responsibilities:
 *   - I2C register read/write
 *   - uV -> VOUT selector conversion
 *   - set/get voltage
 *   - enable/disable buck if register is provided
 *
 * This driver does NOT:
 *   - read CPR fuse
 *   - calculate OLV
 *   - apply threshold policy
 *   - know nominal/turbo mode
 */

#include <dm.h>
#include <dm/lists.h>
#include <errno.h>
#include <i2c.h>
#include <log.h>
#include <power/regulator.h>
#include <vsprintf.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/types.h>

#define MP8899_DEFAULT_MIN_UV		400000
#define MP8899_DEFAULT_MAX_UV		2047500
#define MP8899_DEFAULT_STEP_UV		500

#define MP8899_BUCK1_CTL3		0x02
#define MP8899_BUCK1_CTL4		0x03
#define MP8899_BUCK1_CTL5		0x04
#define MP8899_BUCK1_CTL6		0x05
#define MP8899_BUCK_CTL_OFFSET		0x06
#define MP8899_SYSTEM4			0x21
#define MP8899_DEFAULT_BUCK_ID		1
#define MP8899_MAX_BUCKS		4

#define MP8899_VOUT_SELECT_MASK		BIT(6)
#define MP8899_VREF_HIGH_MASK		GENMASK(3, 0)
#define MP8899_GO_BIT_MASK		BIT(7)
#define MP8899_HW_SEL_MASK		GENMASK(11, 0)
#define MP8899_VENDOR_ID_MASK		GENMASK(7, 4)
#define MP8899_VENDOR_ID_VALUE		0x8
#define MP8899_GO_BIT_TIMEOUT_US	10000
#define MP8899_GO_BIT_POLL_US		10

struct mp8899_plat {
	const char *name;

	u32 min_uv;
	u32 max_uv;
	u32 step_uv;
	u32 buck_id;

	bool enable_supported;
	u32 enable_reg;
	u32 enable_mask;
};

static struct udevice *mp8899_i2c_dev(struct udevice *dev)
{
	if (dev->parent && device_is_compatible(dev->parent, "mps,mp8899"))
		return dev->parent;

	return dev;
}

static int mp8899_reg_read(struct udevice *dev, uint reg)
{
	return dm_i2c_reg_read(mp8899_i2c_dev(dev), reg);
}

static int mp8899_reg_write(struct udevice *dev, uint reg, uint val)
{
	return dm_i2c_reg_write(mp8899_i2c_dev(dev), reg, val);
}

static int mp8899_update_bits(struct udevice *dev,
			      uint reg, u32 mask, u32 val)
{
	int old;
	u32 new;

	old = mp8899_reg_read(dev, reg);
	if (old < 0)
		return old;

	new = (old & ~mask) | (val & mask);

	return mp8899_reg_write(dev, reg, new);
}

static u32 mp8899_buck_reg(struct mp8899_plat *plat, u32 buck1_reg)
{
	return buck1_reg + (plat->buck_id - 1) * MP8899_BUCK_CTL_OFFSET;
}

static int mp8899_uv_to_sel(struct mp8899_plat *plat, u32 uv)
{
	if (uv < plat->min_uv || uv > plat->max_uv)
		return -EINVAL;

	if (!plat->step_uv)
		return -EINVAL;

	/*
	 * Round up so programmed voltage is not below requested OLV.
	 */
	return DIV_ROUND_UP(uv, plat->step_uv);
}

static int mp8899_sel_to_uv(struct mp8899_plat *plat, int sel)
{
	return sel * plat->step_uv;
}

static int mp8899_wait_go_bit_clear(struct udevice *dev,
				    struct mp8899_plat *plat)
{
	int timeout = MP8899_GO_BIT_TIMEOUT_US;
	uint reg = mp8899_buck_reg(plat, MP8899_BUCK1_CTL6);
	int val;

	while (timeout > 0) {
		val = mp8899_reg_read(dev, reg);
		if (val < 0)
			return val;

		if (!(val & MP8899_GO_BIT_MASK))
			return 0;

		udelay(MP8899_GO_BIT_POLL_US);
		timeout -= MP8899_GO_BIT_POLL_US;
	}

	return -ETIMEDOUT;
}

static int mp8899_set_value(struct udevice *dev, int uv)
{
	struct mp8899_plat *plat = dev_get_plat(dev);
	u8 regs[3];
	int sel;
	int ret;

	if (uv < 0)
		return -EINVAL;

	sel = mp8899_uv_to_sel(plat, (u32)uv);
	if (sel < 0) {
		pr_err("%s: voltage %duV out of range [%u, %u]\n",
		       dev->name, uv, plat->min_uv, plat->max_uv);
		return sel;
	}

	if (sel & ~MP8899_HW_SEL_MASK)
		return -EINVAL;

	regs[0] = (sel >> 8) & MP8899_VREF_HIGH_MASK;
	regs[1] = sel & 0xff;
	regs[2] = MP8899_GO_BIT_MASK;

	ret = dm_i2c_write(mp8899_i2c_dev(dev),
			   mp8899_buck_reg(plat, MP8899_BUCK1_CTL4),
			   regs, sizeof(regs));
	if (ret)
		return ret;

	ret = mp8899_wait_go_bit_clear(dev, plat);
	if (ret)
		return ret;

	debug("%s: buck%u set voltage=%duV selector=0x%x\n",
	       dev->name, plat->buck_id, uv, sel);

	return 0;
}

static int mp8899_get_value(struct udevice *dev)
{
	struct mp8899_plat *plat = dev_get_plat(dev);
	int high;
	int low;
	int sel;

	high = mp8899_reg_read(dev, mp8899_buck_reg(plat, MP8899_BUCK1_CTL4));
	if (high < 0)
		return high;

	low = mp8899_reg_read(dev, mp8899_buck_reg(plat, MP8899_BUCK1_CTL5));
	if (low < 0)
		return low;

	sel = ((high & MP8899_VREF_HIGH_MASK) << 8) | low;

	return mp8899_sel_to_uv(plat, sel);
}

static int mp8899_set_enable(struct udevice *dev, bool enable)
{
	struct mp8899_plat *plat = dev_get_plat(dev);
	u32 val;

	if (!plat->enable_supported)
		return 0;

	val = enable ? plat->enable_mask : 0;

	return mp8899_update_bits(dev, plat->enable_reg,
				  plat->enable_mask, val);
}

static int mp8899_get_enable(struct udevice *dev)
{
	struct mp8899_plat *plat = dev_get_plat(dev);
	int val;

	if (!plat->enable_supported)
		return 1;

	val = mp8899_reg_read(dev, plat->enable_reg);
	if (val < 0)
		return val;

	return !!(val & plat->enable_mask);
}

static const struct dm_regulator_ops mp8899_regulator_ops = {
	.set_value	= mp8899_set_value,
	.get_value	= mp8899_get_value,
	.set_enable	= mp8899_set_enable,
	.get_enable	= mp8899_get_enable,
};

static int mp8899_identify_device(struct udevice *dev);

static int mp8899_of_to_plat(struct udevice *dev)
{
	struct mp8899_plat *plat = dev_get_plat(dev);
	struct dm_regulator_uclass_plat *uc_pdata;
	const char *node_name;
	u32 arr[2];
	long buck_id;
	int ret;

	uc_pdata = dev_get_uclass_plat(dev);

	plat->name = dev_read_string(dev, "regulator-name");
	if (!plat->name)
		plat->name = dev->name;

	if (uc_pdata)
		uc_pdata->name = plat->name;

	ret = dev_read_u32(dev, "qcom,buck-id", &plat->buck_id);
	if (ret) {
		node_name = dev_read_name(dev);
		buck_id = trailing_strtol(node_name);
		if (buck_id < 0)
			buck_id = MP8899_DEFAULT_BUCK_ID;

		plat->buck_id = buck_id;
	}

	if (plat->buck_id < 1 || plat->buck_id > MP8899_MAX_BUCKS)
		return -EINVAL;

	if (dev_read_u32(dev, "regulator-min-microvolt", &plat->min_uv))
		plat->min_uv = MP8899_DEFAULT_MIN_UV;

	if (dev_read_u32(dev, "regulator-max-microvolt", &plat->max_uv))
		plat->max_uv = MP8899_DEFAULT_MAX_UV;

	plat->step_uv = MP8899_DEFAULT_STEP_UV;

	if (!plat->step_uv || plat->min_uv > plat->max_uv)
		return -EINVAL;

	/*
	 * Optional:
	 * qcom,enable-reg = <reg mask>
	 */
	ret = dev_read_u32_array(dev, "qcom,enable-reg", arr, 2);
	if (!ret) {
		plat->enable_supported = true;
		plat->enable_reg = arr[0];
		plat->enable_mask = arr[1];
	}

	return 0;
}

static int mp8899_probe(struct udevice *dev)
{
	struct mp8899_plat *plat = dev_get_plat(dev);
	int val;
	int ret;

	if (!dev->parent || !device_is_compatible(dev->parent, "mps,mp8899")) {
		ret = mp8899_identify_device(dev);
		if (ret)
			return ret;
	}

	val = mp8899_reg_read(dev, mp8899_buck_reg(plat, MP8899_BUCK1_CTL3));
	if (val < 0)
		return val;

	if (val & MP8899_VOUT_SELECT_MASK)
		plat->step_uv = 1000;

	return 0;
}

static int mp8899_identify_device(struct udevice *dev)
{
	int vendor_id;

	vendor_id = dm_i2c_reg_read(dev, MP8899_SYSTEM4);
	if (vendor_id < 0)
		return vendor_id;

	vendor_id = (vendor_id & MP8899_VENDOR_ID_MASK) >> 4;
	if (vendor_id != MP8899_VENDOR_ID_VALUE) {
		pr_err("%s: invalid vendor ID 0x%x\n", dev->name, vendor_id);
		return -ENODEV;
	}

	return 0;
}

static int mp8899_bind(struct udevice *dev)
{
	ofnode regulators_node;
	ofnode node;
	int children = 0;
	int ret;

	regulators_node = dev_read_subnode(dev, "regulators");
	if (!ofnode_valid(regulators_node))
		return 0;

	ofnode_for_each_subnode(node, regulators_node) {
		if (!ofnode_is_enabled(node))
			continue;

		ret = device_bind_driver_to_node(dev, "mp8899_regulator",
						 ofnode_get_name(node),
						 node, NULL);
		if (ret)
			return ret;

		children++;
	}

	if (!children)
		debug("%s: no regulator children found\n", dev->name);

	return 0;
}

static int mp8899_parent_probe(struct udevice *dev)
{
	return mp8899_identify_device(dev);
}

static const struct udevice_id mp8899_ids[] = {
	{ .compatible = "qcom,mp8899-regulator" },
	{ .compatible = "mps,mp8899-regulator" },
	{ }
};

static const struct udevice_id mp8899_parent_ids[] = {
	{ .compatible = "mps,mp8899" },
	{ }
};

U_BOOT_DRIVER(mp8899_pmic) = {
	.name		= "mp8899_pmic",
	.id		= UCLASS_NOP,
	.of_match	= mp8899_parent_ids,
	.bind		= mp8899_bind,
	.probe		= mp8899_parent_probe,
};

U_BOOT_DRIVER(mp8899_regulator) = {
	.name		= "mp8899_regulator",
	.id		= UCLASS_REGULATOR,
	.of_match	= mp8899_ids,
	.ops		= &mp8899_regulator_ops,
	.of_to_plat	= mp8899_of_to_plat,
	.probe		= mp8899_probe,
	.plat_auto	= sizeof(struct mp8899_plat),
};
