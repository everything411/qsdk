/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "nss_phy.h"
#include "qca8084_pinctrl.h"

/****************************************************************************
 *
 * 1) PINs default Setting
 *
 ****************************************************************************/
#ifdef IN_PINCTRL_DEF_CONFIG
static a_ulong pin_configs[] = {
	NSS_PHY_PIN_CONFIG_OUTPUT_ENABLE,
	NSS_PHY_PIN_CONFIG_BIAS_PULL_DOWN,
};
#endif

static struct nss_phy_pinctrl_setting qca8084_pin_settings[] = {
	/*PINs default MUX Setting*/
	NSS_PHY_PIN_SETTING_MUX(0,  QCA8084_PIN_FUNC_INTN_WOL),
	NSS_PHY_PIN_SETTING_MUX(1,  QCA8084_PIN_FUNC_INTN),
	NSS_PHY_PIN_SETTING_MUX(10, QCA8084_PIN_FUNC_P1_PPS_OUT),
	NSS_PHY_PIN_SETTING_MUX(11, QCA8084_PIN_FUNC_P2_PPS_OUT),
	NSS_PHY_PIN_SETTING_MUX(12, QCA8084_PIN_FUNC_P3_PPS_OUT),
	NSS_PHY_PIN_SETTING_MUX(13, QCA8084_PIN_FUNC_P0_TOD_OUT),
	NSS_PHY_PIN_SETTING_MUX(14, QCA8084_PIN_FUNC_P0_CLK125_TDI),
	NSS_PHY_PIN_SETTING_MUX(15, QCA8084_PIN_FUNC_P0_SYNC_CLKO_PTP),
	NSS_PHY_PIN_SETTING_MUX(20, QCA8084_PIN_FUNC_MDC_M),
	NSS_PHY_PIN_SETTING_MUX(21, QCA8084_PIN_FUNC_MDO_M),

	/*PINs default Config Setting*/
#ifdef IN_PINCTRL_DEF_CONFIG
	NSS_PHY_PIN_SETTING_MUX(0,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(1,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(2,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(3,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(4,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(5,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(6,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(7,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(8,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(9,  pin_configs),
	NSS_PHY_PIN_SETTING_MUX(10, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(11, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(12, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(13, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(14, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(15, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(16, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(17, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(18, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(19, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(20, pin_configs),
	NSS_PHY_PIN_SETTING_MUX(21, pin_configs),
#endif
};

/****************************************************************************
 *
 * 2) PINs Operations
 *
 ****************************************************************************/
int qca8084_gpio_set_bit(struct nss_phy_device *nss_phydev,
	u32 pin, u32 value)
{
	return nss_phy_modify_soc(nss_phydev, TO_GPIIO_IN_OUT_REG(pin),
		GPIO_OUT_VAL, value & GPIO_OUT_VAL);
}

int qca8084_gpio_get_bit(struct nss_phy_device *nss_phydev, u32 pin,
	u32  *data)
{
	*data = (nss_phy_read_soc(nss_phydev, TO_GPIIO_IN_OUT_REG(pin)) &
		GPIO_IN_VAL);

	return 0;
}

int qca8084_gpio_pin_mux_set(struct nss_phy_device *nss_phydev, u32 pin,
	u32 func)
{
	return nss_phy_modify_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin),
		TLMM_GPIO_CFGN_FUNC_SEL, func << 2);
}

int qca8084_gpio_pin_cfg_set_bias(struct nss_phy_device *nss_phydev, u32 pin,
	enum pin_config_param bias)
{
	u32 data = 0;

	switch (bias) {
	case NSS_PHY_PIN_CONFIG_BIAS_DISABLE:
		data = PULL_DISABLE;
		break;
	case NSS_PHY_PIN_CONFIG_BIAS_PULL_DOWN:
		data = PULL_DOWN;
		break;
	case NSS_PHY_PIN_CONFIG_BIAS_BUS_HOLD:
		data = PULL_BUS_HOLD;
		break;
	case NSS_PHY_PIN_CONFIG_BIAS_PULL_UP:
		data = PULL_UP;
		break;
	default:
		nss_phy_err(nss_phydev, "[%s] doesn't support bias:%d\n",
			__func__, bias);
		return -NSS_PHY_EOPNOTSUPP;
	}

	return nss_phy_modify_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin),
		TLMM_GPIO_CFGN_GPIO_PULL, data);
}

int qca8084_gpio_pin_cfg_get_bias(struct nss_phy_device *nss_phydev, u32 pin,
	enum pin_config_param *bias)
{
	u32 data = 0;

	data = nss_phy_read_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin));
	switch (data & TLMM_GPIO_CFGN_GPIO_PULL) {
	case PULL_DISABLE:
		*bias = NSS_PHY_PIN_CONFIG_BIAS_DISABLE;
		break;
	case PULL_DOWN:
		*bias = NSS_PHY_PIN_CONFIG_BIAS_PULL_DOWN;
		break;
	case PULL_BUS_HOLD:
		*bias = NSS_PHY_PIN_CONFIG_BIAS_BUS_HOLD;
		break;
	case PULL_UP:
		*bias = NSS_PHY_PIN_CONFIG_BIAS_PULL_UP;
		break;
	default:
		nss_phy_err(nss_phydev, "[%s] doesn't support bias:%d\n",
			__func__, *bias);
		return -NSS_PHY_EINVAL;
	}

	return 0;
}

int qca8084_gpio_pin_cfg_set_drvs(struct nss_phy_device *nss_phydev,
	u32 pin, u32 drvs)
{
	if ((drvs < DRV_STRENGTH_2_MA) || (drvs > DRV_STRENGTH_16_MA)) {
		nss_phy_err(nss_phydev, "[%s] doesn't support drvs:%d\n",
			__func__, drvs);
		return -NSS_PHY_EINVAL;
	}

	return nss_phy_modify_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin),
		TLMM_GPIO_CFGN_DRV_STRENGTH, drvs << 6);
}

int qca8084_gpio_pin_cfg_get_drvs(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *drvs)
{
	*drvs = (nss_phy_read_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin)) &
		TLMM_GPIO_CFGN_DRV_STRENGTH >> 6);

	return 0;
}

int qca8084_gpio_pin_cfg_set_hihys(struct nss_phy_device *nss_phydev,
	u32 pin, u32 hihys_en)
{
	return nss_phy_modify_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin),
		TLMM_GPIO_CFGN_GPIO_HIHYS_EN, hihys_en << 10);
}
int qca8084_gpio_pin_cfg_get_hihys(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *hihys_en)
{
	*hihys_en = (nss_phy_read_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin)) &
		TLMM_GPIO_CFGN_GPIO_HIHYS_EN >> 10);

	return 0;
}

int qca8084_gpio_pin_cfg_set_oe(struct nss_phy_device *nss_phydev,
	u32 pin, u32 oe)
{
	return nss_phy_modify_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin),
		TLMM_GPIO_CFGN_GPIO_OE, oe << 9);
}

int qca8084_gpio_pin_cfg_get_oe(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *oe)
{
	*oe = (nss_phy_read_soc(nss_phydev, TO_GPIIO_CFGN_REG(pin)) &
		TLMM_GPIO_CFGN_GPIO_OE >> 9);

	return 0;
}

static int qca8084_gpio_pin_cfg_set(struct nss_phy_device *nss_phydev,
	u32 pin, ulong *configs, u32 num_configs)
{
	int ret = 0;
	enum nss_phy_pin_config_param param;
	u32 i, arg;

	for (i = 0; i < num_configs; i++) {
		param = nss_phy_pinconf_to_config_param(configs[i]);
		arg = nss_phy_pinconf_to_config_argument(configs[i]);

		switch (param) {
		case NSS_PHY_PIN_CONFIG_BIAS_BUS_HOLD:
		case NSS_PHY_PIN_CONFIG_BIAS_DISABLE:
		case NSS_PHY_PIN_CONFIG_BIAS_PULL_DOWN:
		case NSS_PHY_PIN_CONFIG_BIAS_PULL_UP:
			ret = qca8084_gpio_pin_cfg_set_bias(nss_phydev, pin,
				param);
			if (ret < 0)
				return ret;
			break;
		case NSS_PHY_PIN_CONFIG_DRIVE_STRENGTH:
			ret = qca8084_gpio_pin_cfg_set_drvs(nss_phydev, pin,
				arg);
			if (ret < 0)
				return ret;
			break;

		case NSS_PHY_PIN_CONFIG_OUTPUT:
			ret = qca8084_gpio_pin_cfg_set_oe(nss_phydev, pin,
				true);
			if (ret < 0)
				return ret;
			ret = qca8084_gpio_set_bit(nss_phydev, pin, arg);
			if (ret < 0)
				return ret;
			break;

		case NSS_PHY_PIN_CONFIG_INPUT_ENABLE:
			ret = qca8084_gpio_pin_cfg_set_oe(nss_phydev, pin,
				false);
			if (ret < 0)
				return ret;
			break;

		case NSS_PHY_PIN_CONFIG_OUTPUT_ENABLE:
			ret = qca8084_gpio_pin_cfg_set_oe(nss_phydev, pin,
				true);
			if (ret < 0)
				return ret;
			break;

		default:
			nss_phy_err(nss_phydev, "%s doesn't support:%d\n",
				__func__, param);
			return -NSS_PHY_EOPNOTSUPP;
		}
	}

	return 0;
}

/****************************************************************************
 *
 * 3) PINs Init
 *
 ****************************************************************************/
int qca8084_pinctrl_clk_gate_set(struct nss_phy_device *nss_phydev,
	u32 gate_en)
{
	int ret = 0;

	ret = nss_phy_modify_soc(nss_phydev, TLMM_CLK_GATE_EN,
		TLMM_CLK_GATE_EN_AHB_HCLK_EN,
		gate_en << 2);
	if (ret < 0)
		return ret;
	ret = nss_phy_modify_soc(nss_phydev, TLMM_CLK_GATE_EN,
		TLMM_CLK_GATE_EN_SUMMARY_INTR_EN,
		gate_en << 1);
	if (ret < 0)
		return ret;
	ret = nss_phy_modify_soc(nss_phydev, TLMM_CLK_GATE_EN,
		TLMM_CLK_GATE_EN_CRIF_READ_EN, gate_en);

	return ret;
}

static int qca8084_pinctrl_rev_check(struct nss_phy_device *nss_phydev)
{
	u32 data = 0, version_id = 0, mfg_id = 0, start_bit = 0;

	data = nss_phy_read_soc(nss_phydev, TLMM_HW_REVISION_NUMBER);
	version_id = (data & TLMM_HW_REVISION_NUMBER_VERSION_ID_BOFFSET) >> 28;
	mfg_id = (data & TLMM_HW_REVISION_NUMBER_MFG_ID_BOFFSET) >> 1;
	start_bit = data & TLMM_HW_REVISION_NUMBER_START;
	if (!((version_id == 0x0) && (mfg_id == 0x70) && (start_bit == 0x1))) {
		nss_phy_err(nss_phydev, " Pinctrl Version Check Fail\n");
		return -NSS_PHY_EINVAL;
	}

	return 0;
}

static int qca8084_pinctrl_hw_init(struct nss_phy_device *nss_phydev)
{
	int ret = 0;

	ret = qca8084_pinctrl_clk_gate_set(nss_phydev, true);
	if (ret < 0)
		return ret;
	ret = qca8084_pinctrl_rev_check(nss_phydev);

	return ret;
}

static int qca8084_pinctrl_setting_init(struct nss_phy_device *nss_phydev,
	const struct nss_phy_pinctrl_setting *pin_settings,
	u32 num_setting)
{
	int ret = 0;
	u32 i;

	for (i = 0; i < num_setting; i++) {
		const struct nss_phy_pinctrl_setting *setting =
			&pin_settings[i];

		if (setting->type == NSS_PHY_PIN_MAP_TYPE_MUX_GROUP) {
			ret = qca8084_gpio_pin_mux_set(nss_phydev,
				setting->data.mux.pin,
				setting->data.mux.func);
		} else if (setting->type == NSS_PHY_PIN_MAP_TYPE_CONFIGS_PIN) {
			ret = qca8084_gpio_pin_cfg_set(nss_phydev,
				setting->data.configs.pin,
				setting->data.configs.configs,
				setting->data.configs.num_configs);
		}
	}

	return ret;
}

int qca8084_pinctrl_init(struct nss_phy_device *nss_phydev)
{
	qca8084_pinctrl_hw_init(nss_phydev);
	qca8084_pinctrl_setting_init(nss_phydev, qca8084_pin_settings,
		ARRAY_SIZE(qca8084_pin_settings));

	return 0;
}
