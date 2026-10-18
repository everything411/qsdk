// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Sartura Ltd.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 */

#include "pinctrl-snapdragon.h"
#include <common.h>

#define MAX_PIN_NAME_LEN		32

static char pin_name[MAX_PIN_NAME_LEN];

static const struct pinctrl_function msm_pinctrl_functions[] = {
	{"gpio", 0},
	{"uart0_rfr", 1},
	{"uart0_cts", 1},
	{"uart0_rx", 1},
	{"uart0_tx", 1},
	{"uart1_rx", 1},
	{"uart1_tx", 1},
	{"i2c0_scl", 1},
	{"i2c0_sda", 1},
	{"i2c1_scl", 2},
	{"i2c1_sda", 2},
	{"i2c2_scl", 1},
	{"i2c2_sda", 1},
	{"spi0_miso", 1},
	{"spi0_mosi", 1},
	{"spi0_sclk", 1},
	{"spi0_cs0", 1},
	{"spi1_miso_0", 1},
	{"spi1_mosi_0", 1},
	{"spi1_sclk_0", 1},
	{"spi1_cs0_0", 1},
	{"spi1_miso_1", 3},
	{"spi1_mosi_1", 3},
	{"spi1_sclk_1", 3},
	{"spi1_cs0_1", 3},
	{"spi1_cs1", 2},
	{"spi1_cs2", 2},
	{"spi1_cs3", 2},
	{"qspi_clk", 2},
	{"qspi_cs_n", 2},
	{"qspi_data", 2},
	{"sdc_clk", 1},
	{"sdc_cmd", 1},
	{"sdc_data", 1},
	{"mdc", 1},
	{"mdio", 1},
};

static const char *ipq5424_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *ipq5424_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	snprintf(pin_name, MAX_PIN_NAME_LEN, "GPIO_%u", selector);
	return pin_name;
}

static unsigned int ipq5424_get_function_mux(unsigned int selector)
{
	return msm_pinctrl_functions[selector].val;
}

struct msm_pinctrl_data pinctrl_data = {
	.pin_count = 49,
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = ipq5424_get_function_name,
	.get_function_mux = ipq5424_get_function_mux,
	.get_pin_name = ipq5424_get_pin_name,
};
