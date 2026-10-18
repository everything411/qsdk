/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*
 * Qualcomm fuse-voltage control helpers
 */

#ifndef __QCOM_VOLTAGE_CONTROL_H
#define __QCOM_VOLTAGE_CONTROL_H

#include <linux/types.h>

struct udevice;

enum qcom_voltage_mode {
	QCOM_VOLTAGE_MODE_NOMINAL = 0,
	QCOM_VOLTAGE_MODE_TURBO,
};

struct qcom_voltage_policy_state {
	u32 mode_mask;
	u32 min_uv;
	u32 max_uv;
	u32 target_uv;
};

int qcom_voltage_control_set_configured_mode(const char *dev_name);

int qcom_voltage_control_calc_olv_uv(u32 fuse_val, u32 vref_uv,
				     u32 sign_bit, u32 step_uv,
				     int *olv_uv);

int qcom_voltage_control_pick_policy(const struct qcom_voltage_policy_state *policy,
				     int npolicy, u32 mode_mask,
				     u32 olv_uv, u32 *target_uv);

#endif /* __QCOM_VOLTAGE_CONTROL_H */
