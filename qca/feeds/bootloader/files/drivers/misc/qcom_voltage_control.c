// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * Qualcomm fuse based voltage policy driver
 *
 * Responsibilities:
 *   - Read CPR/open-loop voltage fuse fields
 *   - Decode signed fuse ticks into OLV/uV
 *   - Handle nominal/turbo mode selection
 *   - Apply threshold policy for discrete GPIO regulators
 *   - Decide final target voltage in uV
 *   - Call regulator_set_value()
 *
 * This driver does NOT:
 *   - directly program GPIOs
 *   - directly program PMIC/I2C registers
 *
 * Backend drivers:
 *   - qcom_gpio_regulator.c: voltage -> GPIO state
 *   - mp8899_regulator.c: voltage -> I2C VOUT selector/register
 */

#include <dm.h>
#include <dm/ofnode.h>
#include <dm/read.h>
#include <dm/uclass.h>
#include <errno.h>
#include <log.h>
#include <asm/io.h>
#include <power/regulator.h>
#include <linux/bitops.h>
#include <linux/types.h>
#include <qcom_voltage_control.h>

#define QVC_MAX_RAILS			4

#define QVC_MODE_MASK_NOMINAL		BIT(0)
#define QVC_MODE_MASK_TURBO		BIT(1)

enum qvc_backend {
	QVC_BACKEND_GPIO = 0,
	QVC_BACKEND_PMIC,
};

struct qvc_fuse_desc {
	phys_addr_t addr;
	u32 shift;
	u32 mask;
	bool valid;
	phys_addr_t addr2;
	u32 shift2;
	u32 mask2;
	u32 value_shift2;
};

struct qvc_mode_desc {
	struct qvc_fuse_desc fuse;
	u32 vref_uv;
	bool valid;
};

/*
 * qcom_voltage_policy_state format:
 *
 * mode_mask:
 *   bit0 = nominal
 *   bit1 = turbo
 *
 * max_uv:
 *   0xffffffff means open-ended.
 *
 * This table exists only in qcom_voltage_control because threshold policy
 * belongs here, not in the GPIO regulator backend.
 */
struct qvc_rail_desc {
	const char *rail_name;
	u32 supported_modes;
	enum qcom_voltage_mode default_mode;

	struct qvc_mode_desc nominal;
	struct qvc_mode_desc turbo;

	u32 default_uv;
	u32 sign_bit;
	u32 step_uv;

	const struct qcom_voltage_policy_state *policy;
	int npolicy;
};

struct qvc_target_desc {
	const struct qvc_rail_desc *rails;
	int nrails;
};

struct qvc_rail {
	const struct qvc_rail_desc *desc;

	enum qvc_backend backend;
	enum qcom_voltage_mode configured_mode;

	ofnode regulator_node;
	const char *regulator_name;
	struct udevice *regulator;
};

struct qvc_plat {
	struct qvc_rail rails[QVC_MAX_RAILS];
	int nrails;
};

static const struct qcom_voltage_policy_state ipq5210_soc_cx_policy[] = {
	{ QVC_MODE_MASK_NOMINAL, 850000, 850000, 850000 },
	{ QVC_MODE_MASK_NOMINAL, 862500, 920000, 920000 },
};

static const struct qcom_voltage_policy_state ipq9650_soc_cx_policy[] = {
	{ QVC_MODE_MASK_NOMINAL, 750000, 750000, 750000 },
	{ QVC_MODE_MASK_NOMINAL, 755000, 820000, 820000 },
};

static const struct qcom_voltage_policy_state ipq9650_ddr_cx_policy[] = {
	{ QVC_MODE_MASK_NOMINAL, 750000, 815000, 815000 },
	{ QVC_MODE_MASK_TURBO, 815000, 960000, 960000 },
};

static const struct qvc_rail_desc ipq5210_rails[] = {
	{
		.rail_name = "soc_cx",
		.supported_modes = QVC_MODE_MASK_NOMINAL,
		.default_mode = QCOM_VOLTAGE_MODE_NOMINAL,
		.nominal = {
			.fuse = { 0x000A03F0, 15, 0x3f, true },
			.vref_uv = 850000,
			.valid = true,
		},
		.default_uv = 920000,
		.sign_bit = 5,
		.step_uv = 12500,
		.policy = ipq5210_soc_cx_policy,
		.npolicy = ARRAY_SIZE(ipq5210_soc_cx_policy),
	},
};

static const struct qvc_rail_desc ipq9650_rails[] = {
	{
		.rail_name = "soc_cx",
		.supported_modes = QVC_MODE_MASK_NOMINAL,
		.default_mode = QCOM_VOLTAGE_MODE_NOMINAL,
		.nominal = {
			.fuse = { 0x000A03F8, 15, 0x7f, true },
			.vref_uv = 820000,
			.valid = true,
		},
		.default_uv = 820000,
		.sign_bit = 6,
		.step_uv = 5000,
		.policy = ipq9650_soc_cx_policy,
		.npolicy = ARRAY_SIZE(ipq9650_soc_cx_policy),
	},
	{
		.rail_name = "ddr_cx",
		.supported_modes = QVC_MODE_MASK_NOMINAL | QVC_MODE_MASK_TURBO,
		.default_mode = QCOM_VOLTAGE_MODE_TURBO,
		.nominal = {
			.fuse = {
				.addr = 0x000A0410,
				.shift = 30,
				.mask = 0x3,
				.valid = true,
				.addr2 = 0x000A0414,
				.shift2 = 0,
				.mask2 = 0x1f,
				.value_shift2 = 2,
			},
			.vref_uv = 815000,
			.valid = true,
		},
		.turbo = {
			.fuse = { 0x000A0410, 23, 0x7f, true },
			.vref_uv = 960000,
			.valid = true,
		},
		.default_uv = 960000,
		.sign_bit = 6,
		.step_uv = 5000,
		.policy = ipq9650_ddr_cx_policy,
		.npolicy = ARRAY_SIZE(ipq9650_ddr_cx_policy),
	},
};

static const struct qvc_target_desc ipq5210_desc = {
	.rails = ipq5210_rails,
	.nrails = ARRAY_SIZE(ipq5210_rails),
};

static const struct qvc_target_desc ipq9650_desc = {
	.rails = ipq9650_rails,
	.nrails = ARRAY_SIZE(ipq9650_rails),
};

static u32 qvc_mode_to_mask(enum qcom_voltage_mode mode)
{
	if (mode == QCOM_VOLTAGE_MODE_TURBO)
		return QVC_MODE_MASK_TURBO;

	return QVC_MODE_MASK_NOMINAL;
}

static const char *qvc_mode_name(enum qcom_voltage_mode mode)
{
	return mode == QCOM_VOLTAGE_MODE_TURBO ? "turbo" : "nominal";
}

static const char *qvc_backend_name(enum qvc_backend backend)
{
	return backend == QVC_BACKEND_PMIC ? "pmic" : "gpio";
}

static int qvc_parse_mode(const char *mode, enum qcom_voltage_mode *qmode)
{
	if (!mode || !qmode)
		return -EINVAL;

	if (!strcmp(mode, "nominal")) {
		*qmode = QCOM_VOLTAGE_MODE_NOMINAL;
		return 0;
	}

	if (!strcmp(mode, "turbo")) {
		*qmode = QCOM_VOLTAGE_MODE_TURBO;
		return 0;
	}

	return -EINVAL;
}

static const struct qvc_mode_desc *
qvc_get_mode_desc(const struct qvc_rail_desc *desc,
		  enum qcom_voltage_mode mode)
{
	if (mode == QCOM_VOLTAGE_MODE_TURBO)
		return &desc->turbo;

	return &desc->nominal;
}

static const struct qvc_rail_desc *
qvc_find_rail_desc(const struct qvc_target_desc *target, const char *rail_name)
{
	int i;

	if (!rail_name)
		return NULL;

	for (i = 0; i < target->nrails; i++) {
		if (!strcmp(target->rails[i].rail_name, rail_name))
			return &target->rails[i];
	}

	return NULL;
}

static int qvc_read_fuse(const struct qvc_fuse_desc *fuse, u32 *fuse_val)
{
	u32 reg;
	u32 val;

	if (!fuse || !fuse_val || !fuse->valid)
		return -EINVAL;

	reg = readl((void __iomem *)fuse->addr);
	val = (reg >> fuse->shift) & fuse->mask;

	if (fuse->addr2) {
		reg = readl((void __iomem *)fuse->addr2);
		val |= ((reg >> fuse->shift2) & fuse->mask2) <<
		       fuse->value_shift2;
	}

	*fuse_val = val;

	return 0;
}

/*
 * Generic CPR target-voltage decode:
 *
 * if sign bit = 1:
 *   OLV = Vref - steps * step_uv
 * else:
 *   OLV = Vref + steps * step_uv
 */
int qcom_voltage_control_calc_olv_uv(u32 fuse_val, u32 vref_uv,
				     u32 sign_bit, u32 step_uv,
				     int *olv_uv)
{
	u32 step_mask;
	u32 steps;
	u32 delta_uv;
	bool sign;

	if (!olv_uv || !step_uv)
		return -EINVAL;

	if (sign_bit >= 31)
		return -EINVAL;

	step_mask = sign_bit ? GENMASK(sign_bit - 1, 0) : 0;
	steps = fuse_val & step_mask;
	sign = !!(fuse_val & BIT(sign_bit));
	delta_uv = steps * step_uv;

	if (sign) {
		if (delta_uv > vref_uv)
			return -ERANGE;
		*olv_uv = vref_uv - delta_uv;
	} else {
		*olv_uv = vref_uv + delta_uv;
	}

	return 0;
}

int qcom_voltage_control_pick_policy(const struct qcom_voltage_policy_state *policy,
				     int npolicy, u32 mode_mask,
				     u32 olv_uv, u32 *target_uv)
{
	u32 boundary_target = 0;
	bool boundary_match = false;
	int i;

	if (!policy || !target_uv || npolicy <= 0)
		return -EINVAL;

	for (i = 0; i < npolicy; i++) {
		const struct qcom_voltage_policy_state *st = &policy[i];

		if (!(st->mode_mask & mode_mask))
			continue;

		if (olv_uv < st->min_uv)
			continue;

		if (st->max_uv != 0xffffffff && olv_uv > st->max_uv)
			continue;

		if (st->max_uv != 0xffffffff && olv_uv == st->max_uv) {
			/*
			 * Ranges are normally [min, max), so boundary values
			 * may be matched by the next row. Keep this as a
			 * fallback for the final ceiling voltage.
			 */
			boundary_target = st->target_uv;
			boundary_match = true;
			continue;
		}

		*target_uv = st->target_uv;
		return 0;
	}

	if (boundary_match) {
		*target_uv = boundary_target;
		return 0;
	}

	return -ERANGE;
}

static int qvc_decode_olv_uv(const struct qvc_rail_desc *desc,
			     u32 fuse_val, u32 vref_uv,
			     int *out_olv_uv)
{
	if (!desc || !out_olv_uv)
		return -EINVAL;

	return qcom_voltage_control_calc_olv_uv(fuse_val, vref_uv,
						desc->sign_bit, desc->step_uv,
						out_olv_uv);
}

static int qvc_find_regulator_by_name(const char *name, struct udevice **rdevp)
{
	struct udevice *rdev;
	struct uclass *uc;
	int ret;

	if (!name || !rdevp)
		return -EINVAL;

	ret = uclass_get(UCLASS_REGULATOR, &uc);
	if (ret)
		return ret;

	uclass_foreach_dev_probe(UCLASS_REGULATOR, rdev) {
		struct dm_regulator_uclass_plat *uc_pdata;

		uc_pdata = dev_get_uclass_plat(rdev);

		if (uc_pdata && uc_pdata->name &&
		    !strcmp(uc_pdata->name, name)) {
			*rdevp = rdev;
			return 0;
		}

		if (!strcmp(rdev->name, name)) {
			*rdevp = rdev;
			return 0;
		}
	}

	return -ENODEV;
}

static int qvc_resolve_regulator(struct qvc_rail *rail)
{
	int ret;

	if (rail->regulator)
		return 0;

	if (ofnode_valid(rail->regulator_node)) {
		ret = uclass_get_device_by_ofnode(UCLASS_REGULATOR,
						  rail->regulator_node,
						  &rail->regulator);
		if (!ret)
			return 0;
	}

	ret = qvc_find_regulator_by_name(rail->regulator_name,
					 &rail->regulator);
	if (ret) {
		pr_err("%s: regulator not found ret=%d\n",
		       rail->desc->rail_name, ret);
		return ret;
	}

	return 0;
}

static int qvc_get_target_voltage(struct qvc_rail *rail,
				  enum qcom_voltage_mode mode,
				  int olv_uv, int *target_uv)
{
	const struct qvc_rail_desc *desc;
	u32 target;
	int ret;

	if (!rail || !target_uv)
		return -EINVAL;

	desc = rail->desc;

	if (rail->backend == QVC_BACKEND_PMIC) {
		/*
		 * PMIC case:
		 * final voltage is the exact computed OLV or default voltage.
		 */
		*target_uv = olv_uv;
		return 0;
	}

	/*
	 * GPIO regulator case:
	 * GPIO rails are discrete-voltage rails. Threshold policy converts
	 * computed OLV or default voltage into a supported discrete voltage.
	 */
	ret = qcom_voltage_control_pick_policy(desc->policy, desc->npolicy,
					       qvc_mode_to_mask(mode),
					       olv_uv, &target);
	if (ret) {
		pr_err("%s: no policy state for mode=%s olv=%duV\n",
		       desc->rail_name, qvc_mode_name(mode), olv_uv);
		return ret;
	}

	*target_uv = target;

	return 0;
}

static int qvc_set_rail_mode(struct qvc_rail *rail,
			     enum qcom_voltage_mode mode)
{
	const struct qvc_rail_desc *desc;
	const struct qvc_mode_desc *mode_desc;
	bool default_used = false;
	u32 fuse_val;
	int olv_uv;
	int target_uv;
	int ret;

	if (!rail || !rail->desc)
		return -EINVAL;

	desc = rail->desc;

	if (!(desc->supported_modes & qvc_mode_to_mask(mode))) {
		pr_err("%s: unsupported mode=%s\n",
		       desc->rail_name, qvc_mode_name(mode));
		return -EINVAL;
	}

	mode_desc = qvc_get_mode_desc(desc, mode);
	if (!mode_desc->valid)
		return -EINVAL;

	ret = qvc_resolve_regulator(rail);
	if (ret)
		return ret;

	ret = qvc_read_fuse(&mode_desc->fuse, &fuse_val);
	if (ret) {
		pr_err("%s: fuse read failed mode=%s ret=%d\n",
		       desc->rail_name, qvc_mode_name(mode), ret);
		return ret;
	}

	if (!fuse_val) {
		if (!desc->default_uv) {
			pr_err("%s: zero fuse but no default voltage\n",
			       desc->rail_name);
			return -EINVAL;
		}

		olv_uv = desc->default_uv;
		default_used = true;
	} else {
		ret = qvc_decode_olv_uv(desc, fuse_val, mode_desc->vref_uv,
					&olv_uv);
		if (ret) {
			pr_err("%s: OLV decode failed ret=%d\n",
			       desc->rail_name, ret);
			return ret;
		}
	}

	ret = qvc_get_target_voltage(rail, mode, olv_uv, &target_uv);
	if (ret)
		return ret;

	ret = regulator_set_value(rail->regulator, target_uv);
	if (ret) {
		pr_err("%s: regulator_set_value(%duV) failed ret=%d\n",
		       desc->rail_name, target_uv, ret);
		return ret;
	}

	ret = regulator_set_enable(rail->regulator, true);
	if (ret) {
		pr_err("%s: regulator enable failed ret=%d\n",
		       desc->rail_name, ret);
		return ret;
	}

	printf("%s: backend=%s mode=%s fuse=0x%x vref=%uuV olv=%duV target=%duV%s\n",
	       desc->rail_name, qvc_backend_name(rail->backend),
	       qvc_mode_name(mode), fuse_val, mode_desc->vref_uv, olv_uv,
	       target_uv, default_used ? " default" : "");

	return 0;
}

int qcom_voltage_control_set_configured_mode(const char *dev_name)
{
	struct qvc_plat *plat;
	struct udevice *dev;
	int ret;
	int i;

	ret = uclass_get_device_by_name(UCLASS_MISC, dev_name, &dev);
	if (ret) {
		pr_err("voltage-control device '%s' not found ret=%d\n",
		       dev_name, ret);
		return ret;
	}

	plat = dev_get_plat(dev);
	if (!plat || !plat->nrails)
		return -EINVAL;

	for (i = 0; i < plat->nrails; i++) {
		ret = qvc_set_rail_mode(&plat->rails[i],
					plat->rails[i].configured_mode);
		if (ret)
			return ret;
	}

	return 0;
}

static int qvc_parse_backend(ofnode node, struct qvc_rail *rail)
{
	const char *backend;

	backend = ofnode_read_string(node, "qcom,backend");
	if (!backend)
		return -EINVAL;

	if (!strcmp(backend, "gpio")) {
		rail->backend = QVC_BACKEND_GPIO;
		return 0;
	}

	if (!strcmp(backend, "pmic")) {
		rail->backend = QVC_BACKEND_PMIC;
		return 0;
	}

	return -EINVAL;
}

static int qvc_parse_regulator(ofnode node, struct qvc_rail *rail)
{
	struct ofnode_phandle_args args;
	int ret;

	ret = ofnode_parse_phandle_with_args(node, "regulator", NULL, 0, 0,
					     &args);
	if (!ret) {
		rail->regulator_node = args.node;
		return 0;
	}

	rail->regulator_name = ofnode_read_string(node, "qcom,regulator-name");
	if (rail->regulator_name)
		return 0;

	return ret;
}

static int qvc_parse_rail_node(const struct qvc_target_desc *target,
			       ofnode node, struct qvc_rail *rail)
{
	const struct qvc_rail_desc *desc;
	const char *rail_name;
	const char *mode;
	int ret;

	rail_name = ofnode_read_string(node, "qcom,rail-name");
	desc = qvc_find_rail_desc(target, rail_name);
	if (!desc)
		return -EINVAL;

	rail->desc = desc;
	rail->regulator_node = ofnode_null();

	ret = qvc_parse_backend(node, rail);
	if (ret)
		return ret;

	mode = ofnode_read_string(node, "qcom,mode");
	if (mode) {
		ret = qvc_parse_mode(mode, &rail->configured_mode);
		if (ret)
			return ret;
	} else {
		rail->configured_mode = desc->default_mode;
	}

	if (!(desc->supported_modes & qvc_mode_to_mask(rail->configured_mode)))
		return -EINVAL;

	ret = qvc_parse_regulator(node, rail);
	if (ret)
		return ret;

	return 0;
}

static int qvc_of_to_plat(struct udevice *dev)
{
	const struct qvc_target_desc *target;
	struct qvc_plat *plat = dev_get_plat(dev);
	ofnode child;
	int ret;

	target = (const struct qvc_target_desc *)dev_get_driver_data(dev);
	if (!target)
		return -EINVAL;

	ofnode_for_each_subnode(child, dev_ofnode(dev)) {
		if (!ofnode_is_enabled(child))
			continue;

		if (plat->nrails >= QVC_MAX_RAILS)
			return -EINVAL;

		ret = qvc_parse_rail_node(target, child,
					  &plat->rails[plat->nrails]);
		if (ret) {
			pr_err("%s: failed to parse rail node '%s' ret=%d\n",
			       dev->name, ofnode_get_name(child), ret);
			return ret;
		}

		plat->nrails++;
	}

	if (!plat->nrails)
		return -ENODEV;

	return 0;
}

static int qvc_probe(struct udevice *dev)
{
	struct qvc_plat *plat = dev_get_plat(dev);
	int i;

	for (i = 0; i < plat->nrails; i++) {
		struct qvc_rail *rail = &plat->rails[i];

		pr_debug("%s: rail=%s backend=%s mode=%s\n",
			 dev->name, rail->desc->rail_name,
			 qvc_backend_name(rail->backend),
			 qvc_mode_name(rail->configured_mode));
	}

	return 0;
}

static const struct udevice_id qvc_ids[] = {
	{
		.compatible = "qcom,ipq5210-fuse-voltage-control",
		.data = (ulong)&ipq5210_desc,
	},
	{
		.compatible = "qcom,ipq9650-fuse-voltage-control",
		.data = (ulong)&ipq9650_desc,
	},
	{ }
};

U_BOOT_DRIVER(qcom_fuse_voltage_control) = {
	.name		= "qcom_fuse_voltage_control",
	.id		= UCLASS_MISC,
	.of_match	= qvc_ids,
	.of_to_plat	= qvc_of_to_plat,
	.probe		= qvc_probe,
	.plat_auto	= sizeof(struct qvc_plat),
};
