// SPDX-License-Identifier: GPL-2.0+
/*
 * Qualcomm IPQ5424 pinctrl
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

enum ipq5424_functions {
	msm_mux_gpio,
	msm_mux_i2c0_scl,
	msm_mux_i2c0_sda,
	msm_mux_i2c1_scl,
	msm_mux_i2c1_sda,
	msm_mux_i2c11,
	msm_mux_mdc_mst,
	msm_mux_mdc_slv,
	msm_mux_mdio_mst,
	msm_mux_mdio_slv,
	msm_mux_pcie0_clk,
	msm_mux_pcie0_wake,
	msm_mux_pcie1_clk,
	msm_mux_pcie1_wake,
	msm_mux_pcie2_clk,
	msm_mux_pcie2_wake,
	msm_mux_pcie3_clk,
	msm_mux_pcie3_wake,
	msm_mux_qspi_clk,
	msm_mux_qspi_cs,
	msm_mux_qspi_data,
	msm_mux_sdc_clk,
	msm_mux_sdc_cmd,
	msm_mux_sdc_data,
	msm_mux_spi0_clk,
	msm_mux_spi0_cs,
	msm_mux_spi0_miso,
	msm_mux_spi0_mosi,
	msm_mux_spi1,
	msm_mux_spi10,
	msm_mux_spi11,
	msm_mux_uart0,
	msm_mux_uart1,
	msm_mux_NA,
};

#define MSM_PIN_FUNCTION(fname)				\
	[msm_mux_##fname] = {#fname, msm_mux_##fname}

static const struct pinctrl_function msm_pinctrl_functions[] = {
	MSM_PIN_FUNCTION(gpio),
	MSM_PIN_FUNCTION(i2c0_scl),
	MSM_PIN_FUNCTION(i2c0_sda),
	MSM_PIN_FUNCTION(i2c1_scl),
	MSM_PIN_FUNCTION(i2c1_sda),
	MSM_PIN_FUNCTION(i2c11),
	MSM_PIN_FUNCTION(mdc_mst),
	MSM_PIN_FUNCTION(mdc_slv),
	MSM_PIN_FUNCTION(mdio_mst),
	MSM_PIN_FUNCTION(mdio_slv),
	MSM_PIN_FUNCTION(pcie0_clk),
	MSM_PIN_FUNCTION(pcie0_wake),
	MSM_PIN_FUNCTION(pcie1_clk),
	MSM_PIN_FUNCTION(pcie1_wake),
	MSM_PIN_FUNCTION(pcie2_clk),
	MSM_PIN_FUNCTION(pcie2_wake),
	MSM_PIN_FUNCTION(pcie3_clk),
	MSM_PIN_FUNCTION(pcie3_wake),
	MSM_PIN_FUNCTION(qspi_clk),
	MSM_PIN_FUNCTION(qspi_cs),
	MSM_PIN_FUNCTION(qspi_data),
	MSM_PIN_FUNCTION(sdc_clk),
	MSM_PIN_FUNCTION(sdc_cmd),
	MSM_PIN_FUNCTION(sdc_data),
	MSM_PIN_FUNCTION(spi0_clk),
	MSM_PIN_FUNCTION(spi0_cs),
	MSM_PIN_FUNCTION(spi0_miso),
	MSM_PIN_FUNCTION(spi0_mosi),
	MSM_PIN_FUNCTION(spi1),
	MSM_PIN_FUNCTION(spi10),
	MSM_PIN_FUNCTION(spi11),
	MSM_PIN_FUNCTION(uart0),
	MSM_PIN_FUNCTION(uart1),
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

static const msm_pin_function ipq5424_pin_functions[] = {
	PINGROUP(0, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(1, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(2, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(3, sdc_data, qspi_data, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(4, sdc_cmd, qspi_cs, NA, NA, NA, NA, NA, NA,NA),
	PINGROUP(5, sdc_clk, qspi_clk, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(6, spi0_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(7, spi0_cs, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(8, spi0_miso, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(9, spi0_mosi, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(10, uart0, NA, spi11, NA, NA, NA, NA, NA, NA),
	PINGROUP(11, uart0, NA, spi1, NA, NA, NA, NA, NA, NA),
	PINGROUP(12, uart0, NA, spi11, NA, NA, NA, NA, NA, NA),
	PINGROUP(13, uart0, NA, spi11, NA, NA, NA, NA, NA, NA),
	PINGROUP(14, i2c0_scl, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(15, i2c0_sda, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(16, NA, i2c1_scl, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(17, NA, i2c1_sda, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(18, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(19, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(20, mdc_slv, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(21, mdio_slv, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(22, mdc_mst, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(23, mdio_mst, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(24, pcie0_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(25, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(26, pcie0_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(27, pcie1_clk, i2c11, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(28, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(29, pcie1_wake, i2c11, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(30, pcie2_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(31, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(32, pcie2_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(33, pcie3_clk, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(34, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(35, pcie3_wake, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(36, NA, spi1, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(37, NA, spi1, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(38, NA, spi1, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(39, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(40, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(41, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(42, NA, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(43, uart1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(44, uart1, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(45, spi10, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(46, spi1, NA, NA, NA, NA,NA, NA, NA, NA),
	PINGROUP(47, spi10, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(48, spi10, NA, NA, NA, NA, NA, NA, NA, NA),
	PINGROUP(49, NA, NA, NA, NA, NA, NA, NA, NA, NA),
};

static const char *ipq5424_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *ipq5424_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	snprintf(pin_name, MAX_PIN_NAME_LEN, "gpio%u", selector);
	return pin_name;
}

static unsigned int ipq5424_get_function_mux(unsigned int pin,
					     unsigned int selector)
{
	unsigned int i;
	const msm_pin_function *func = ipq5424_pin_functions + pin;

	for (i = 0; i < 10; i++)
		if ((*func)[i] == selector)
			return i;

	pr_err("Can't find requested function for pin %u pin\n", pin);
	return -EINVAL;
}

static const struct msm_pinctrl_data ipq5424_data = {
	.pin_data = {
		.pin_count = 50,
		.special_pins_start = 50, /* There are no special pins */
	},
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = ipq5424_get_function_name,
	.get_function_mux = ipq5424_get_function_mux,
	.get_pin_name = ipq5424_get_pin_name,
};

static const struct udevice_id msm_pinctrl_ids[] = {
	{ .compatible = "qcom,ipq5424-tlmm", .data = (ulong)&ipq5424_data },
	{ /* Sentinal */ }
};

U_BOOT_DRIVER(pinctrl_ipq5424) = {
	.name		= "pinctrl_ipq5424",
	.id		= UCLASS_NOP,
	.of_match	= msm_pinctrl_ids,
	.ops		= &msm_pinctrl_ops,
	.bind		= msm_pinctrl_bind,
	.flags = DM_FLAG_PRE_RELOC,
};
