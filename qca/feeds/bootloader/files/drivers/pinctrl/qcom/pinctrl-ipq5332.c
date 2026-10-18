// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm IPQ5332 pinctrl
 *
 * Copyright (c) 2019 Sartura Ltd.
 * Copyright (c) 2025, Qualcomm Innovation Center,Inc.All rights reserved.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 */

#include <dm.h>

#include "pinctrl-qcom.h"

#define MAX_PIN_NAME_LEN 32
static char pin_name[MAX_PIN_NAME_LEN] __section(".data");

enum ipq5332_functions {
	msm_mux_blsp0_i2c,
	msm_mux_blsp0_spi,
	msm_mux_blsp0_uart0,
	msm_mux_blsp0_uart1,
	msm_mux_blsp1_i2c0,
	msm_mux_blsp1_i2c1,
	msm_mux_blsp1_spi0,
	msm_mux_blsp1_spi1,
	msm_mux_blsp1_uart0,
	msm_mux_blsp1_uart1,
	msm_mux_blsp1_uart2,
	msm_mux_blsp2_i2c0,
	msm_mux_blsp2_i2c1,
	msm_mux_blsp2_spi,
	msm_mux_blsp2_spi0,
	msm_mux_blsp2_spi1,
	msm_mux_gpio,
	msm_mux_mdc0,
	msm_mux_mdc1,
	msm_mux_mdio0,
	msm_mux_mdio1,
	msm_mux_pcie0_clk,
	msm_mux_pcie0_wake,
	msm_mux_pcie1_clk,
	msm_mux_pcie1_wake,
	msm_mux_pcie2_clk,
	msm_mux_pcie2_wake,
	msm_mux_qspi_data,
	msm_mux_qspi_clk,
	msm_mux_qspi_cs,
	msm_mux_sdc_data,
	msm_mux_sdc_clk,
	msm_mux_sdc_cmd,
	msm_mux_NA,
};

#define MSM_PIN_FUNCTION(fname)				\
	[msm_mux_##fname] = {#fname, msm_mux_##fname}

static const struct pinctrl_function msm_pinctrl_functions[] = {
	MSM_PIN_FUNCTION(blsp0_i2c),
	MSM_PIN_FUNCTION(blsp0_spi),
	MSM_PIN_FUNCTION(blsp0_uart0),
	MSM_PIN_FUNCTION(blsp0_uart1),
	MSM_PIN_FUNCTION(blsp1_i2c0),
	MSM_PIN_FUNCTION(blsp1_i2c1),
	MSM_PIN_FUNCTION(blsp1_spi0),
	MSM_PIN_FUNCTION(blsp1_spi1),
	MSM_PIN_FUNCTION(blsp1_uart0),
	MSM_PIN_FUNCTION(blsp1_uart1),
	MSM_PIN_FUNCTION(blsp1_uart2),
	MSM_PIN_FUNCTION(blsp2_i2c0),
	MSM_PIN_FUNCTION(blsp2_i2c1),
	MSM_PIN_FUNCTION(blsp2_spi),
	MSM_PIN_FUNCTION(blsp2_spi0),
	MSM_PIN_FUNCTION(blsp2_spi1),
	MSM_PIN_FUNCTION(gpio),
	MSM_PIN_FUNCTION(mdc0),
	MSM_PIN_FUNCTION(mdc1),
	MSM_PIN_FUNCTION(mdio0),
	MSM_PIN_FUNCTION(mdio1),
	MSM_PIN_FUNCTION(pcie0_clk),
	MSM_PIN_FUNCTION(pcie0_wake),
	MSM_PIN_FUNCTION(pcie1_clk),
	MSM_PIN_FUNCTION(pcie1_wake),
	MSM_PIN_FUNCTION(pcie2_clk),
	MSM_PIN_FUNCTION(pcie2_wake),
	MSM_PIN_FUNCTION(qspi_data),
	MSM_PIN_FUNCTION(qspi_clk),
	MSM_PIN_FUNCTION(qspi_cs),
	MSM_PIN_FUNCTION(sdc_data),
	MSM_PIN_FUNCTION(sdc_clk),
	MSM_PIN_FUNCTION(sdc_cmd),
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

static const msm_pin_function ipq5332_pin_functions[] = {
	PINGROUP(0, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(1, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(2, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(3, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(4, NA, NA, NA, NA, NA, NA, NA, NA,	 NA),
	PINGROUP(5, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(6, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(7, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(8, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(9, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(10, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(11, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(12, sdc_cmd, qspi_cs, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(13, sdc_clk, qspi_clk, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(14, blsp0_spi, blsp1_uart0, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(15, blsp0_spi, blsp1_uart0, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(16, blsp0_spi, blsp0_i2c, blsp1_uart0, NA, NA, NA, NA, NA, NA),
	PINGROUP(17, blsp0_spi, blsp0_i2c, blsp1_uart0, NA, NA, NA, NA, NA, NA),
	PINGROUP(18, blsp0_uart0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(19, blsp0_uart0, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(20, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(21, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(22, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(23, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(24, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(25, mdc0, blsp1_uart1, blsp1_spi1, NA, NA, NA, NA, NA, NA),
	PINGROUP(26, mdio0, blsp1_uart1, blsp1_spi1, NA, NA, NA, NA, NA, NA),
	PINGROUP(27, mdc1, blsp0_uart1, blsp1_uart1, blsp1_spi1, NA, NA, NA, NA, NA),
	PINGROUP(28, mdio1, blsp0_uart1, blsp1_uart1, blsp1_spi1, NA, NA, NA, NA, NA),
	PINGROUP(29, NA, blsp1_spi0, blsp1_i2c0, NA, NA, NA, NA, NA, NA),
	PINGROUP(30, NA, blsp1_spi0, blsp1_i2c0, NA, NA, NA, NA, NA, NA),
	PINGROUP(31, NA, blsp1_spi0, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(32, NA, blsp1_spi0, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(33, NA, blsp1_uart2, blsp2_i2c1, blsp2_spi0, NA, NA, NA, NA, NA),
	PINGROUP(34, NA, blsp1_uart2, blsp2_i2c1, blsp2_spi0, NA, NA, NA, NA, NA),
	PINGROUP(35, NA, blsp1_uart2, NA, NA, blsp2_spi0,  NA, NA, NA, NA),
	PINGROUP(36, NA, blsp1_uart2, NA, NA, blsp2_spi0, NA, NA, NA, NA),
	PINGROUP(37, pcie0_clk, blsp2_spi, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(38, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(39, pcie0_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(40, NA, blsp1_i2c1, blsp2_spi1, NA, NA, NA, NA, NA, NA),
	PINGROUP(41, NA, blsp1_i2c1, blsp2_spi1, NA, NA, NA, NA, NA, NA),
	PINGROUP(42, NA, blsp2_spi1, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(43, pcie2_clk, NA, NA, blsp2_i2c0, NA, NA, NA, NA, NA),
	PINGROUP(44, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(45, pcie2_wake, NA, NA, blsp2_i2c0, NA, NA, NA, NA, NA),
	PINGROUP(46, pcie1_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(47, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(48, pcie1_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(49, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(50, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(51, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(52, NA, blsp2_spi1, NA, NA, NA, NA, NA, NA, NA),
};

static const char *ipq5332_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *ipq5332_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	snprintf(pin_name, MAX_PIN_NAME_LEN, "gpio%u", selector);
	return pin_name;
}

static unsigned int ipq5332_get_function_mux(unsigned int pin,
					     unsigned int selector)
{
	unsigned int i;
	const msm_pin_function *func = ipq5332_pin_functions + pin;

	for (i = 0; i < 15; i++)
		if ((*func)[i] == selector)
			return i;

	pr_err("Can't find requested function for pin %u pin\n", pin);
	return -EINVAL;
}

static const struct msm_pinctrl_data ipq5332_data = {
	.pin_data = {
		.pin_count = 53,
		.special_pins_start = 53, /* There are no special pins */
	},
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = ipq5332_get_function_name,
	.get_function_mux = ipq5332_get_function_mux,
	.get_pin_name = ipq5332_get_pin_name,
};

static const struct udevice_id msm_pinctrl_ids[] = {
	{ .compatible = "qcom,ipq5332-tlmm", .data = (ulong)&ipq5332_data },
	{ /* Sentinal */ }
};

U_BOOT_DRIVER(pinctrl_ipq5332) = {
	.name		= "pinctrl_ipq5332",
	.id		= UCLASS_NOP,
	.of_match	= msm_pinctrl_ids,
	.ops		= &msm_pinctrl_ops,
	.bind		= msm_pinctrl_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
