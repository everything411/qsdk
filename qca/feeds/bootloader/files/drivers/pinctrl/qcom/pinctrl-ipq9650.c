// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm IPQ9650 pinctrl
 *
 * Copyright (c) 2019 Sartura Ltd.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 */

#include <dm.h>

#include "pinctrl-qcom.h"

#define MAX_PIN_NAME_LEN 32
static char pin_name[MAX_PIN_NAME_LEN] __section(".data");

enum ipq9650_functions {
	msm_mux_gpio,
	msm_mux_sdc_data,
	msm_mux_qspi_data,
	msm_mux_sdc_cmd,
	msm_mux_qspi_cs,
	msm_mux_sdc_clk,
	msm_mux_qspi_clk,
	msm_mux_qup_se0_l0,
	msm_mux_qup_se0_l1,
	msm_mux_qup_se0_l2,
	msm_mux_qup_se0_l3,
	msm_mux_qup_se1_l0,
	msm_mux_qup_se1_l1,
	msm_mux_qup_se1_l2,
	msm_mux_qup_se1_l3,
	msm_mux_qup_se4_l0,
	msm_mux_qup_se4_l1,
	msm_mux_qup_se4_l2,
	msm_mux_qup_se4_l3,
	msm_mux_qup_se5_l0,
	msm_mux_qup_se5_l1,
	msm_mux_qup_se5_l2,
	msm_mux_qup_se5_l3,
	msm_mux_qup_se6_l2,
	msm_mux_qup_se6_l3,
	msm_mux_i2c0_scl,
	msm_mux_i2c0_sda,
	msm_mux_i2c1_scl,
	msm_mux_i2c1_sda,
	msm_mux_core_voltage_0,
	msm_mux_core_voltage_1,
	msm_mux_core_voltage_2,
	msm_mux_core_voltage_3,
	msm_mux_core_voltage_4,
	msm_mux_mdc_slv,
	msm_mux_mdio_slv,
	msm_mux_mdc_mst,
	msm_mux_mdio_mst,
	msm_mux_pcie0_clk_req_n,
	msm_mux_pcie0_wake,
	msm_mux_pcie1_clk_req_n,
	msm_mux_pcie1_wake,
	msm_mux_pcie2_clk_req_n,
	msm_mux_pcie2_wake,
	msm_mux_pcie3_clk_req_n,
	msm_mux_pcie3_wake,
	msm_mux_pcie4_clk_req_n,
	msm_mux_pcie4_wake,
	msm_mux_audio_pri_d0,
	msm_mux_audio_pri_d1,
	msm_mux_audio_pri_fsync,
	msm_mux_audio_pri_pclk,
	msm_mux_resout,
	msm_mux_tsn,
	msm_mux_NA,
};

#define MSM_PIN_FUNCTION(fname)				\
	[msm_mux_##fname] = {#fname, msm_mux_##fname}

static const struct pinctrl_function msm_pinctrl_functions[] = {
	MSM_PIN_FUNCTION(gpio),
	MSM_PIN_FUNCTION(sdc_data),
	MSM_PIN_FUNCTION(qspi_data),
	MSM_PIN_FUNCTION(sdc_cmd),
	MSM_PIN_FUNCTION(qspi_cs),
	MSM_PIN_FUNCTION(sdc_clk),
	MSM_PIN_FUNCTION(qspi_clk),
	MSM_PIN_FUNCTION(qup_se0_l0),
	MSM_PIN_FUNCTION(qup_se0_l1),
	MSM_PIN_FUNCTION(qup_se0_l2),
	MSM_PIN_FUNCTION(qup_se0_l3),
	MSM_PIN_FUNCTION(qup_se1_l0),
	MSM_PIN_FUNCTION(qup_se1_l1),
	MSM_PIN_FUNCTION(qup_se1_l2),
	MSM_PIN_FUNCTION(qup_se1_l3),
	MSM_PIN_FUNCTION(qup_se4_l0),
	MSM_PIN_FUNCTION(qup_se4_l1),
	MSM_PIN_FUNCTION(qup_se4_l2),
	MSM_PIN_FUNCTION(qup_se4_l3),
	MSM_PIN_FUNCTION(qup_se5_l0),
	MSM_PIN_FUNCTION(qup_se5_l1),
	MSM_PIN_FUNCTION(qup_se5_l2),
	MSM_PIN_FUNCTION(qup_se5_l3),
	MSM_PIN_FUNCTION(qup_se6_l2),
	MSM_PIN_FUNCTION(qup_se6_l3),
	MSM_PIN_FUNCTION(i2c0_scl),
	MSM_PIN_FUNCTION(i2c0_sda),
	MSM_PIN_FUNCTION(i2c1_scl),
	MSM_PIN_FUNCTION(i2c1_sda),
	MSM_PIN_FUNCTION(core_voltage_0),
	MSM_PIN_FUNCTION(core_voltage_1),
	MSM_PIN_FUNCTION(core_voltage_2),
	MSM_PIN_FUNCTION(core_voltage_3),
	MSM_PIN_FUNCTION(core_voltage_4),
	MSM_PIN_FUNCTION(mdc_slv),
	MSM_PIN_FUNCTION(mdio_slv),
	MSM_PIN_FUNCTION(mdc_mst),
	MSM_PIN_FUNCTION(mdio_mst),
	MSM_PIN_FUNCTION(pcie0_clk_req_n),
	MSM_PIN_FUNCTION(pcie0_wake),
	MSM_PIN_FUNCTION(pcie1_clk_req_n),
	MSM_PIN_FUNCTION(pcie1_wake),
	MSM_PIN_FUNCTION(pcie2_clk_req_n),
	MSM_PIN_FUNCTION(pcie2_wake),
	MSM_PIN_FUNCTION(pcie3_clk_req_n),
	MSM_PIN_FUNCTION(pcie3_wake),
	MSM_PIN_FUNCTION(pcie4_clk_req_n),
	MSM_PIN_FUNCTION(pcie4_wake),
	MSM_PIN_FUNCTION(audio_pri_d0),
	MSM_PIN_FUNCTION(audio_pri_d1),
	MSM_PIN_FUNCTION(audio_pri_fsync),
	MSM_PIN_FUNCTION(audio_pri_pclk),
	MSM_PIN_FUNCTION(resout),
	MSM_PIN_FUNCTION(tsn),
};

typedef unsigned int msm_pin_function[10];

#define PINGROUP(id, f1, f2, f3, f4, f5, f6, f7, f8, f9) \
	[id] = {        msm_mux_gpio, /* gpio mode */	\
			msm_mux_##f1,			\
			msm_mux_##f2,			\
			msm_mux_##f3,			\
			msm_mux_##f4,			\
			msm_mux_##f5,			\
			msm_mux_##f6,			\
			msm_mux_##f7,			\
			msm_mux_##f8,			\
			msm_mux_##f9,			\
	}

static const msm_pin_function ipq9650_pin_functions[] = {
	PINGROUP(0, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(1, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(2, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(3, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(4, sdc_cmd, qspi_cs, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(5, sdc_clk, qspi_clk, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(6, qup_se0_l2, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(7, qup_se0_l3, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(8, qup_se0_l0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(9, qup_se0_l1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(10, qup_se1_l1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(11, qup_se1_l0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(12, qup_se1_l3, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(13, qup_se1_l2, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(14, qup_se4_l1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(15, qup_se4_l0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(16, core_voltage_0, i2c1_scl, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(17, core_voltage_1, i2c1_sda, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(18, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(19, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(20, mdc_slv, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(21, mdio_slv, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(22, mdc_mst, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(23, mdio_mst, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(24, pcie0_clk_req_n, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(25, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(26, pcie0_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(27, pcie1_clk_req_n, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(28, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(29, pcie1_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(30, pcie4_clk_req_n, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(31, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(32, pcie4_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(33, core_voltage_2, i2c0_scl, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(34, core_voltage_3, i2c0_sda, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(35, core_voltage_4, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(36, audio_pri_d0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(37, audio_pri_d1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(38, audio_pri_fsync, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(39, audio_pri_pclk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(40, pcie3_clk_req_n, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(41, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(42, pcie3_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(43, qup_se4_l3, qup_se6_l3, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(44, qup_se4_l2, qup_se6_l2, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(45, qup_se5_l2, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(46, qup_se5_l3, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(47, qup_se5_l0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(48, qup_se5_l1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(49, resout, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(50, tsn, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(51, pcie2_clk_req_n, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(52, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(53, pcie2_wake, NA, NA, NA, NA, NA, NA, NA, NA),
};

static const char *ipq9650_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *ipq9650_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	snprintf(pin_name, MAX_PIN_NAME_LEN, "gpio%u", selector);
	return pin_name;
}

static unsigned int ipq9650_get_function_mux(unsigned int pin,
					     unsigned int selector)
{
	unsigned int i;
	const msm_pin_function *func = ipq9650_pin_functions + pin;

	for (i = 0; i < 10; i++)
		if ((*func)[i] == selector)
			return i;

	pr_err("Can't find requested function for pin %u pin\n", pin);
	return -EINVAL;
}

static const struct msm_pinctrl_data ipq9650_data = {
	.pin_data = {
		.pin_count = 54,
		.special_pins_start = 54, /* There are no special pins */
	},
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = ipq9650_get_function_name,
	.get_function_mux = ipq9650_get_function_mux,
	.get_pin_name = ipq9650_get_pin_name,
};

static const struct udevice_id msm_pinctrl_ids[] = {
	{ .compatible = "qcom,ipq9650-tlmm", .data = (ulong)&ipq9650_data },
	{ /* Sentinal */ }
};

U_BOOT_DRIVER(pinctrl_ipq9650) = {
	.name		= "pinctrl_ipq9650",
	.id		= UCLASS_NOP,
	.of_match	= msm_pinctrl_ids,
	.ops		= &msm_pinctrl_ops,
	.bind		= msm_pinctrl_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
