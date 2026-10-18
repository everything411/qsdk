/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
 */

#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/ctype.h>
#include "qca81xx.h"

#define QCA81XX_TEMP_WARN_DEFAULT_MDEG	120000	/* 120C */
#define QCA81XX_TEMP_CRIT_DEFAULT_MDEG	140000	/* 140C */
#define QCA81XX_TEMP_MIN_MDEG		40000	/* 40C: reject implausibly low thresholds */
#define QCA81XX_TEMP_MAX_VALID_MDEG	200000	/* 200C: sensor reading is unreliable above this */

#define QFPROM_RAW_CALIBRATION_ROW6_MSB	0x290005C
#define TSENSOR_BASE_CODE_120C		GENMASK(19, 10)
#define TSENSOR_BASE_CODE_30C		GENMASK(9, 0)
#define TSENSOR_OFFSET_0C_0		GENMASK(23, 20)
#define QFPROM_RAW_CALIBRATION_ROW7_LSB	0x900060
#define TSENSOR_OFFSET_0C_1		GENMASK(3, 0)
#define TSENSOR_OFFSET_0C_2		GENMASK(7, 4)
#define TSENSOR_CAL_RESULT_MASK		GENMASK(22, 20)
#define TSENSOR_CAL_RESULT_DONE		0x3
#define VDD4BLOW_EN			0x902038
#define POWER_DOWN			BIT(2)
#define POWER_EN			BIT(3)
#define QFPROM_BLOW_TIMER		0x90203C
#define BELOW_TIMER_MASK		GENMASK(11, 0)
#define BELOW_TIMER_10US		0x190

#define TSENS_0_CONVERSION		0x920060
#define CZERO_MASK			GENMASK(9, 0)
#define CZERO_DEFAULT			0x161
#define SLOPE_MASK			GENMASK(22, 10)
#define SLOPE_DEFAULT			0xcdb
#define SHIFT_MASK			GENMASK(24, 23)
#define SHIFT_10BITS			3
#define TSENS_CTRL			0x920004
#define TSENS_GLOBAL_EN			BIT(0)
#define TSENS_SW_RST			BIT(1)
#define SENSOR0_EN			BIT(3)
#define SENSOR1_EN			BIT(4)
#define SENSOR2_EN			BIT(5)
#define SENSORS_EN			(SENSOR0_EN | SENSOR1_EN | SENSOR2_EN)

#define TSENS_0_STATUS			0x9300A0
#define LAST_TEMP_MASK			GENMASK(11, 0)

static umode_t qca81xx_hwmon_is_visible(const void *data,
	enum hwmon_sensor_types type, u32 attr, int channel)
{
	if (type != hwmon_temp)
		return 0;

	if (attr == hwmon_temp_input)
		return 0444;

	if (attr == hwmon_temp_max || attr == hwmon_temp_crit)
		return 0644;

	return 0;
}

static long qca81xx_hwmon_temp_read(struct phy_device *phydev, int sensor_id)
{
	u32 phy_data = 0, slope = 0;
	long code = 0, czero = 0, temp = 0;

	if (sensor_id >= QCA81XX_SENSORS_NUM)
		return -EOPNOTSUPP;
	phy_lock_mdio_bus(phydev);
	__qca81xx_soc_modify(phydev, TSENS_CTRL,
		TSENS_GLOBAL_EN | SENSORS_EN | TSENS_SW_RST,
		TSENS_GLOBAL_EN | SENSORS_EN | TSENS_SW_RST);
	__qca81xx_soc_modify(phydev, TSENS_CTRL, TSENS_SW_RST, 0);
	usleep_range(1000, 2000);	/* wait for TSENS to stabilize after SW reset */
	phy_data = __qca81xx_soc_read(phydev, TSENS_0_STATUS + 4 * sensor_id);
	code = phy_data & LAST_TEMP_MASK;
	phy_data = __qca81xx_soc_read(phydev, TSENS_0_CONVERSION + 4 * sensor_id);
	czero = phy_data & CZERO_MASK;
	slope = (phy_data & SLOPE_MASK) >> 10;
	/* slope = 921600/diff_90c encodes 0.1°C per ADC code × 1024;
	 * multiply by 100 to convert to milli-Celsius as hwmon requires. */
	temp = (code - czero) * slope >> 10;
	__qca81xx_soc_modify(phydev, TSENS_CTRL, TSENS_GLOBAL_EN | SENSORS_EN, 0);
	phy_unlock_mdio_bus(phydev);

	return temp * 100;
}

static int qca81xx_hwmon_read(struct device *dev, enum hwmon_sensor_types type,
	u32 attr, int sensor_id, long *value)
{
	struct phy_device *phydev = dev_get_drvdata(dev);
	struct qca81xx_private *priv;

	if (!phydev)
		return -EINVAL;
	priv = phydev->priv;
	if (!priv)
		return -EINVAL;
	if (type != hwmon_temp)
		return -EOPNOTSUPP;
	if (sensor_id >= QCA81XX_SENSORS_NUM)
		return -EOPNOTSUPP;

	switch (attr) {
	case hwmon_temp_input:
		*value = qca81xx_hwmon_temp_read(phydev, sensor_id);
		break;
	case hwmon_temp_max:
		*value = priv->temp_warn_mdeg[sensor_id];
		break;
	case hwmon_temp_crit:
		*value = priv->temp_crit_mdeg[sensor_id];
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int qca81xx_hwmon_write(struct device *dev, enum hwmon_sensor_types type,
	u32 attr, int sensor_id, long value)
{
	struct phy_device *phydev = dev_get_drvdata(dev);
	struct qca81xx_private *priv;

	if (!phydev)
		return -EINVAL;
	priv = phydev->priv;
	if (!priv)
		return -EINVAL;
	if (type != hwmon_temp)
		return -EOPNOTSUPP;
	if (sensor_id >= QCA81XX_SENSORS_NUM)
		return -EOPNOTSUPP;

	switch (attr) {
	case hwmon_temp_max:
		/* Enforce crit - warn >= HYSTERESIS so the recovery band always
		 * clears the warn threshold, preventing oscillation near warn.
		 */
		if (value < QCA81XX_TEMP_MIN_MDEG ||
		    value > priv->temp_crit_mdeg[sensor_id] - QCA81XX_THERMAL_HYSTERESIS_MDEG)
			return -EINVAL;
		priv->temp_warn_mdeg[sensor_id] = value;
		break;
	case hwmon_temp_crit:
		if (value < QCA81XX_TEMP_MIN_MDEG ||
		    value < priv->temp_warn_mdeg[sensor_id] + QCA81XX_THERMAL_HYSTERESIS_MDEG)
			return -EINVAL;
		priv->temp_crit_mdeg[sensor_id] = value;
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static const struct hwmon_ops qca81xx_hwmon_ops = {
	.is_visible = qca81xx_hwmon_is_visible,
	.read = qca81xx_hwmon_read,
	.write = qca81xx_hwmon_write,
};

static u32 qca81xx_hwmon_chip_config[] = {
	HWMON_C_REGISTER_TZ,
	0,
};

static const struct hwmon_channel_info qca81xx_hwmon_chip = {
	.type = hwmon_chip,
	.config = qca81xx_hwmon_chip_config,
};

static u32 qca81xx_hwmon_temp_config[] = {
	HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_CRIT,
	HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_CRIT,
	HWMON_T_INPUT | HWMON_T_MAX | HWMON_T_CRIT,
	0,
};

static const struct hwmon_channel_info qca81xx_hwmon_temp = {
	.type = hwmon_temp,
	.config = qca81xx_hwmon_temp_config,
};

static const struct hwmon_channel_info * qca81xx_hwmon_info[] = {
	&qca81xx_hwmon_chip,
	&qca81xx_hwmon_temp,
	NULL,
};

static const struct hwmon_chip_info qca81xx_hwmon_chip_info = {
	.ops = &qca81xx_hwmon_ops,
	.info = qca81xx_hwmon_info,
};

int qca81xx_hwmon_hw_init(struct phy_device *phydev)
{
	u32 phy_data0, phy_data1, ts0_conv_ctrl, ts1_conv_ctrl, ts2_conv_ctrl;

	/* configure the ready time of qfproom as 10us */
	qca81xx_soc_modify(phydev, QFPROM_BLOW_TIMER,
		BELOW_TIMER_MASK, BELOW_TIMER_10US);
	/* enable power */
	qca81xx_soc_modify(phydev, VDD4BLOW_EN,
		POWER_DOWN | POWER_EN, POWER_EN);
	mdelay(1);
	phy_data0 = qca81xx_soc_read(phydev, QFPROM_RAW_CALIBRATION_ROW6_MSB);
	phy_data1 = qca81xx_soc_read(phydev, QFPROM_RAW_CALIBRATION_ROW7_LSB);
	if ((phy_data1 & TSENSOR_CAL_RESULT_MASK) >> 20
		== TSENSOR_CAL_RESULT_DONE) {
		u32 base_code_30c, ts0_0c_code_offset,
			ts1_0c_code_offset, ts2_0c_code_offset, base_code_diff_90c,
			ts0_0c_code, ts1_0c_code, ts2_0c_code, slope;

		base_code_30c = phy_data0 & TSENSOR_BASE_CODE_30C;
		/* the 921600 is temp diff of 90c, and the 246 is the code diff of 90c */
		base_code_diff_90c = 246;
		slope = 921600/base_code_diff_90c;
		ts0_0c_code_offset = (phy_data0 & TSENSOR_OFFSET_0C_0) >> 20;
		ts1_0c_code_offset = phy_data1 & TSENSOR_OFFSET_0C_1;
		ts2_0c_code_offset = (phy_data1 & TSENSOR_OFFSET_0C_2) >> 4;
		ts0_0c_code = base_code_30c - base_code_diff_90c/3 + ts0_0c_code_offset;
		ts1_0c_code = base_code_30c - base_code_diff_90c/3 + ts1_0c_code_offset;
		ts2_0c_code = base_code_30c - base_code_diff_90c/3 + ts2_0c_code_offset;

		ts0_conv_ctrl = ts0_0c_code | (slope << 10) | (SHIFT_10BITS << 23);
		ts1_conv_ctrl = ts1_0c_code | (slope << 10) | (SHIFT_10BITS << 23);
		ts2_conv_ctrl = ts2_0c_code | (slope << 10) | (SHIFT_10BITS << 23);
	}else {
		ts0_conv_ctrl = CZERO_DEFAULT | (SLOPE_DEFAULT << 10) | (SHIFT_10BITS << 23);
		ts1_conv_ctrl = ts0_conv_ctrl;
		ts2_conv_ctrl = ts0_conv_ctrl;
	}
	qca81xx_soc_modify(phydev, TSENS_0_CONVERSION,
		CZERO_MASK | SLOPE_MASK | SHIFT_MASK, ts0_conv_ctrl);
	qca81xx_soc_modify(phydev, TSENS_0_CONVERSION + 4,
		CZERO_MASK | SLOPE_MASK | SHIFT_MASK, ts1_conv_ctrl);
	qca81xx_soc_modify(phydev, TSENS_0_CONVERSION + 8,
		CZERO_MASK | SLOPE_MASK | SHIFT_MASK, ts2_conv_ctrl);
	qca81xx_soc_modify(phydev, VDD4BLOW_EN,
		POWER_DOWN | POWER_EN, POWER_DOWN);

	return 0;
}

int qca81xx_hwmon_probe(struct phy_device *phydev)
{
	struct device *dev = &phydev->mdio.dev;
	char *hwmon_name;
	int i = 0, j = 0;
	struct qca81xx_private *priv = phydev->priv;

	hwmon_name = devm_kstrdup(dev, dev_name(dev), GFP_KERNEL);
	if (!hwmon_name)
		return -ENOMEM;

	for (i = j = 0; hwmon_name[i]; i++) {
		if (isalnum(hwmon_name[i])) {
			if (i != j)
				hwmon_name[j] = hwmon_name[i];
			j++;
		}
	}
	hwmon_name[j] = '\0';

	/* Initialize thresholds before hwmon registration so thermal_check()
	 * never sees zero-valued thresholds even if registration fails.
	 */
	for (i = 0; i < QCA81XX_SENSORS_NUM; i++) {
		priv->temp_warn_mdeg[i] = QCA81XX_TEMP_WARN_DEFAULT_MDEG;
		priv->temp_crit_mdeg[i] = QCA81XX_TEMP_CRIT_DEFAULT_MDEG;
	}
	priv->temp_shutdown_latched = false;

	priv->hwmon_dev = devm_hwmon_device_register_with_info(dev, hwmon_name,
		phydev, &qca81xx_hwmon_chip_info, NULL);
	if (IS_ERR(priv->hwmon_dev))
		return PTR_ERR(priv->hwmon_dev);

	return 0;
}

/*
 * qca81xx_thermal_power_down_locked() - lock-free thermal-shutdown core.
 *
 * qca81xx_phy_suspend() only takes the MDIO bus lock (phy_lock_mdio_bus()),
 * not phydev->lock, so it is safe to call it here from
 * qca81xx_thermal_check(), which itself runs from read_status() with
 * phydev->lock already held.
 *
 * Limitation: temp_shutdown_latched is a software-only flag and is NOT
 * synchronized with the standard phylib suspend/resume state
 * (phydev->state == PHY_HALTED).  If the system calls phy_suspend() or
 * phy_resume() independently (e.g. via ethtool, system sleep), the latch
 * state may become inconsistent.  This is a known limitation of the
 * best-effort software thermal path.
 */
static void qca81xx_thermal_power_down_locked(struct phy_device *phydev)
{
	struct qca81xx_private *priv = phydev->priv;

	if (priv->temp_shutdown_latched)
		return;

	priv->temp_shutdown_latched = true;
	if (qca81xx_phy_suspend(phydev)) {
		phydev_err(phydev, "thermal: suspend failed, shutdown not latched\n");
		priv->temp_shutdown_latched = false;
	}
}

static void qca81xx_thermal_power_up_locked(struct phy_device *phydev)
{
	struct qca81xx_private *priv = phydev->priv;

	if (!priv->temp_shutdown_latched)
		return;

	priv->temp_shutdown_latched = false;
	if (qca81xx_phy_resume(phydev)) {
		phydev_err(phydev, "thermal: resume failed, shutdown remains latched\n");
		priv->temp_shutdown_latched = true;
	}
}

/*
 * qca81xx_thermal_check() - best-effort SW over-temperature poll, called from
 * the tail of qca81xx_phy_read_status(). Never fails/propagates an error to
 * its caller: internal temp-read errors are logged and that sensor is
 * skipped for this cycle.
 */
void qca81xx_thermal_check(struct phy_device *phydev)
{
	struct qca81xx_private *priv = phydev->priv;
	bool any_crit = false, all_recovered = true;
	int i;

	if (!priv)
		return;

	for (i = 0; i < QCA81XX_SENSORS_NUM; i++) {
		long temp = qca81xx_hwmon_temp_read(phydev, i);

		if (temp < 0 || temp > QCA81XX_TEMP_MAX_VALID_MDEG)
			continue;

		if (temp >= priv->temp_crit_mdeg[i]) {
			dev_err_ratelimited(&phydev->mdio.dev,
				"thermal: sensor %d critical temp %ld mC >= %ld mC, shutting down\n",
				i, temp, priv->temp_crit_mdeg[i]);
			any_crit = true;
		} else if (temp >= priv->temp_warn_mdeg[i]) {
			dev_warn_ratelimited(&phydev->mdio.dev,
				"thermal: sensor %d warning temp %ld mC >= %ld mC\n",
				i, temp, priv->temp_warn_mdeg[i]);
		}

		/* Recovery is measured from crit, not warn, to avoid hysteresis
		 * asymmetry when warn and crit are far apart.  Strict `>` (not `>=`)
		 * is intentional: a temperature exactly at crit - HYSTERESIS is not
		 * yet considered recovered, matching the inclusive `>=` used for
		 * shutdown and providing a small additional margin against noise.
		 */
		if (temp > (priv->temp_crit_mdeg[i] - QCA81XX_THERMAL_HYSTERESIS_MDEG))
			all_recovered = false;
	}

	if (!priv->temp_shutdown_latched && any_crit) {
		qca81xx_thermal_power_down_locked(phydev);
	} else if (priv->temp_shutdown_latched && all_recovered) {
		phydev_warn(phydev, "thermal: all sensors recovered, powering up\n");
		qca81xx_thermal_power_up_locked(phydev);
	}
}
