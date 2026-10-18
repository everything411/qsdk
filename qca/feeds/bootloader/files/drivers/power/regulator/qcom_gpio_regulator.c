// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * Qualcomm generic GPIO regulator
 *
 * Responsibilities:
 *   - Support single or multiple GPIO voltage select lines
 *   - Support multiple states similar to Linux regulator-gpio
 *   - Map requested voltage in uV to GPIO selector state
 *   - Drive GPIOs
 *
 * This driver does NOT:
 *   - read CPR fuses
 *   - calculate OLV
 *   - apply threshold policy
 *   - know nominal/turbo mode
 */

#include <dm.h>
#include <errno.h>
#include <log.h>
#include <asm/gpio.h>
#include <power/regulator.h>
#include <linux/bitops.h>
#include <linux/types.h>

#define QCOM_GPIO_REG_MAX_GPIOS        8
#define QCOM_GPIO_REG_MAX_STATES    16

struct qcom_gpio_reg_state {
	u32 voltage_uv;
	u32 gpio_state;
};

struct qcom_gpio_reg_plat {
	const char *name;

	struct gpio_desc gpios[QCOM_GPIO_REG_MAX_GPIOS];
	int ngpios;

	struct qcom_gpio_reg_state states[QCOM_GPIO_REG_MAX_STATES];
	int nstates;

	bool enable_supported;
	struct gpio_desc enable_gpio;
};

static int qcom_gpio_reg_apply_state(struct qcom_gpio_reg_plat *plat,
				     u32 gpio_state)
{
	int i;
	int ret;

	for (i = 0; i < plat->ngpios; i++) {
		int val = !!(gpio_state & BIT(i));

		ret = dm_gpio_set_value(&plat->gpios[i], val);
		if (ret) {
			pr_err("%s: failed to set gpio[%d]=%d ret=%d\n",
			       plat->name, i, val, ret);
			return ret;
		}
	}

	return 0;
}

static int qcom_gpio_reg_get_gpio_state(struct qcom_gpio_reg_plat *plat,
					u32 *gpio_state)
{
	u32 state = 0;
	int i;
	int val;

	if (!gpio_state)
		return -EINVAL;

	for (i = 0; i < plat->ngpios; i++) {
		val = dm_gpio_get_value(&plat->gpios[i]);
		if (val < 0)
			return val;

		if (val)
			state |= BIT(i);
	}

	*gpio_state = state;

	return 0;
}

static struct qcom_gpio_reg_state *
qcom_gpio_reg_find_by_voltage(struct qcom_gpio_reg_plat *plat, u32 uv)
{
	int i;

	for (i = 0; i < plat->nstates; i++) {
		if (plat->states[i].voltage_uv == uv)
			return &plat->states[i];
	}

	return NULL;
}

static struct qcom_gpio_reg_state *
qcom_gpio_reg_find_by_gpio_state(struct qcom_gpio_reg_plat *plat,
				 u32 gpio_state)
{
	int i;

	for (i = 0; i < plat->nstates; i++) {
		if (plat->states[i].gpio_state == gpio_state)
			return &plat->states[i];
	}

	return NULL;
}

static int qcom_gpio_reg_set_value(struct udevice *dev, int uv)
{
	struct qcom_gpio_reg_plat *plat = dev_get_plat(dev);
	struct qcom_gpio_reg_state *state;
	int ret;

	if (uv < 0)
		return -EINVAL;

	state = qcom_gpio_reg_find_by_voltage(plat, (u32)uv);
	if (!state) {
		pr_err("%s: unsupported voltage %duV\n", plat->name, uv);
		return -EINVAL;
	}

	ret = qcom_gpio_reg_apply_state(plat, state->gpio_state);
	if (ret)
		return ret;

	printf("%s: set voltage=%duV gpio_state=0x%x\n",
	       plat->name, uv, state->gpio_state);

	return 0;
}

static int qcom_gpio_reg_get_value(struct udevice *dev)
{
	struct qcom_gpio_reg_plat *plat = dev_get_plat(dev);
	struct qcom_gpio_reg_state *state;
	u32 gpio_state;
	int ret;

	ret = qcom_gpio_reg_get_gpio_state(plat, &gpio_state);
	if (ret)
		return ret;

	state = qcom_gpio_reg_find_by_gpio_state(plat, gpio_state);
	if (!state)
		return -EINVAL;

	return state->voltage_uv;
}

static int qcom_gpio_reg_set_enable(struct udevice *dev, bool enable)
{
	struct qcom_gpio_reg_plat *plat = dev_get_plat(dev);

	if (!plat->enable_supported)
		return 0;

	return dm_gpio_set_value(&plat->enable_gpio, enable);
}

static int qcom_gpio_reg_get_enable(struct udevice *dev)
{
	struct qcom_gpio_reg_plat *plat = dev_get_plat(dev);

	if (!plat->enable_supported)
		return 1;

	return dm_gpio_get_value(&plat->enable_gpio);
}

static const struct dm_regulator_ops qcom_gpio_reg_ops = {
	.set_value    = qcom_gpio_reg_set_value,
	.get_value    = qcom_gpio_reg_get_value,
	.set_enable    = qcom_gpio_reg_set_enable,
	.get_enable    = qcom_gpio_reg_get_enable,
};

static int qcom_gpio_reg_parse_gpios(struct udevice *dev,
				     struct qcom_gpio_reg_plat *plat)
{
	int count;
	int i;
	int ret;

	count = dev_count_phandle_with_args(dev, "gpios", "#gpio-cells", 0);
	if (count <= 0)
		return -EINVAL;

	if (count > QCOM_GPIO_REG_MAX_GPIOS)
		return -EINVAL;

	plat->ngpios = count;

	for (i = 0; i < count; i++) {
		ret = gpio_request_by_name(dev, "gpios", i,
					   &plat->gpios[i],
					   GPIOD_IS_OUT);
		if (ret) {
			pr_err("%s: gpio request index=%d failed ret=%d\n",
			       dev->name, i, ret);
			return ret;
		}
	}

	return 0;
}

static int qcom_gpio_reg_parse_states(struct udevice *dev,
				      struct qcom_gpio_reg_plat *plat)
{
	u32 raw[QCOM_GPIO_REG_MAX_STATES * 2];
	int size;
	int count;
	int ret;
	int i;
	int j;

	size = dev_read_size(dev, "states");
	if (size <= 0)
		return -EINVAL;

	count = size / sizeof(u32);
	if (count % 2)
		return -EINVAL;

	plat->nstates = count / 2;
	if (plat->nstates > QCOM_GPIO_REG_MAX_STATES)
		return -EINVAL;

	ret = dev_read_u32_array(dev, "states", raw, count);
	if (ret)
		return ret;

	for (i = 0, j = 0; i < count; i += 2, j++) {
		u32 max_state = plat->ngpios >= 32 ? 0xffffffff : BIT(plat->ngpios) - 1;

		plat->states[j].voltage_uv = raw[i];
		plat->states[j].gpio_state = raw[i + 1];

		if (plat->states[j].gpio_state & ~max_state)
			return -EINVAL;
	}

	return 0;
}

static int qcom_gpio_reg_of_to_plat(struct udevice *dev)
{
	struct qcom_gpio_reg_plat *plat = dev_get_plat(dev);
	struct dm_regulator_uclass_plat *uc_pdata;
	int ret;

	uc_pdata = dev_get_uclass_plat(dev);

	plat->name = dev_read_string(dev, "regulator-name");
	if (!plat->name)
		plat->name = dev->name;

	if (uc_pdata)
		uc_pdata->name = plat->name;

	ret = qcom_gpio_reg_parse_gpios(dev, plat);
	if (ret)
		return ret;

	ret = qcom_gpio_reg_parse_states(dev, plat);
	if (ret)
		return ret;

	ret = gpio_request_by_name(dev, "enable-gpios", 0,
				   &plat->enable_gpio,
				   GPIOD_IS_OUT);
	if (!ret)
		plat->enable_supported = true;

	return 0;
}

static int qcom_gpio_reg_probe(struct udevice *dev)
{
	struct qcom_gpio_reg_plat *plat = dev_get_plat(dev);

	pr_debug("%s: ngpios=%d nstates=%d enable=%d\n",
		 plat->name, plat->ngpios, plat->nstates,
		 plat->enable_supported);

	return 0;
}

static const struct udevice_id qcom_gpio_reg_ids[] = {
	{ .compatible = "qcom,gpio-regulator" },
	{ .compatible = "regulator-gpio" },
	{ }
};

U_BOOT_DRIVER(qcom_gpio_regulator) = {
	.name        = "qcom_gpio_regulator",
	.id        = UCLASS_REGULATOR,
	.of_match    = qcom_gpio_reg_ids,
	.ops        = &qcom_gpio_reg_ops,
	.of_to_plat    = qcom_gpio_reg_of_to_plat,
	.probe        = qcom_gpio_reg_probe,
	.plat_auto    = sizeof(struct qcom_gpio_reg_plat),
};
