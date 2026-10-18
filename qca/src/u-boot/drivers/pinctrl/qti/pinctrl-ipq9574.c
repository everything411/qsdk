// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Sartura Ltd.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 */

#include "pinctrl-snapdragon.h"
#include <common.h>

#define MAX_PIN_NAME_LEN 32

static char pin_name[MAX_PIN_NAME_LEN];

static const struct pinctrl_function msm_pinctrl_functions[] = {
	{"gpio", 0},
	{"blsp0_uart0", 1},
	{"blsp1_uart2", 2},
	{"blsp0_spi", 1},
	{"blsp1_i2c_scl", 2},
	{"blsp1_i2c_sda", 2},
	{"blsp2_i2c_scl", 2},
	{"blsp2_i2c_sda", 2},
	{"blsp3_i2c_scl", 2},
	{"blsp3_i2c_sda", 2},
	{"blsp4_i2c_scl", 2},
	{"blsp4_i2c_sda", 2},
	{"blsp5_i2c_scl", 1},
	{"blsp5_i2c_sda", 1},
	{"sdc", 1},
	{"qspi", 2},
	{"mdio", 1},
	{"mdc", 1},
};

static const char *ipq9574_get_function_name(struct udevice *dev,
					     unsigned int selector)
{
	return msm_pinctrl_functions[selector].name;
}

static const char *ipq9574_get_pin_name(struct udevice *dev,
					unsigned int selector)
{
	snprintf(pin_name, MAX_PIN_NAME_LEN, "GPIO_%u", selector);
	return pin_name;
}

static unsigned int ipq9574_get_function_mux(unsigned int selector)
{
	return msm_pinctrl_functions[selector].val;
}

struct msm_pinctrl_data pinctrl_data = {
	.pin_count = 64,
	.functions_count = ARRAY_SIZE(msm_pinctrl_functions),
	.get_function_name = ipq9574_get_function_name,
	.get_function_mux = ipq9574_get_function_mux,
	.get_pin_name = ipq9574_get_pin_name,
};
