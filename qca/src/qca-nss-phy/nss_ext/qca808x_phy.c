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
#include "qca808x_phy.h"
#include "nss_phy_common.h"
#include "nss_phy_c45_common.h"
#include "qca8084_pinctrl.h"

static int qca8081_phy_fifo_reset(struct nss_phy_device *nss_phydev)
{
	int ret;

	ret = nss_phy_pcs_modify_mmd(nss_phydev,
		QCA808X_SERDES_ADDR_OFFSET,
		NSS_PHY_MMD1_NUM, QCA808X_PHY_MMD1_FIFO_REST_REG,
		QCA808X_PHY_MMD1_FIFO_RESET,
		QCA808X_PHY_MMD1_FIFO_RESET);
	if (ret < 0)
		return ret;
	nss_phy_mdelay(50);
	ret = nss_phy_pcs_modify_mmd(nss_phydev,
		QCA808X_SERDES_ADDR_OFFSET,
		NSS_PHY_MMD1_NUM, QCA808X_PHY_MMD1_FIFO_REST_REG,
		QCA808X_PHY_MMD1_FIFO_RESET,
		QCA808X_PHY_MMD1_FIFO_RELEASE);

	return ret;
}

static int qca808x_phy_function_reset(struct nss_phy_device *nss_phydev,
	enum nss_phy_reset reset_type)
{
	int ret = 0;

	switch (reset_type) {
	case FIFO_RESET:
		if (nss_phy_id_check(nss_phydev, QCA8081_PHY,
			QCA_PHY_EXACT_MASK))
			ret = qca8081_phy_fifo_reset(nss_phydev);
		else
			ret = nss_phy_common_function_reset(nss_phydev,
				FIFO_RESET);
		break;
	case SOFT_RESET:
		ret = nss_phy_common_function_reset(nss_phydev,
			SOFT_RESET);
		break;
	default:
		break;
	}

	return ret;
}

static int qca808x_phy_eee_adv_set(struct nss_phy_device *nss_phydev, u32 adv)
{
	u16 phy_data = 0;
	int ret;

	ret = nss_phy_common_eee_adv_set(nss_phydev, adv);
	if (ret < 0)
		return ret;

	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		if (adv & EEE_2500BASE_T)
			phy_data |= NSS_PHY_MMD7_EEE_ADV_2500M;
		ret = nss_phy_modify_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_8023AZ_EEE_CTRL1,
			NSS_PHY_MMD7_EEE_ADV_2500M,
			phy_data);
		if (ret < 0)
			return ret;
	}

	return nss_phy_common_autoneg_restart(nss_phydev);
}

static int qca808x_phy_eee_adv_get(struct nss_phy_device *nss_phydev, u32 *adv)
{
	u16 phy_data = 0;
	int ret = 0;

	*adv = 0;
	ret = nss_phy_common_eee_adv_get(nss_phydev, adv);
	if (ret < 0)
		return ret;

	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_8023AZ_EEE_CTRL1);
		if (phy_data & NSS_PHY_MMD7_EEE_ADV_2500M)
			*adv |= EEE_2500BASE_T;
	}

	return 0;
}

static int qca808x_phy_eee_partner_adv_get(struct nss_phy_device *nss_phydev,
	u32 *adv)
{
	u16 phy_data = 0;
	int ret = 0;

	*adv = 0;
	ret = nss_phy_common_eee_partner_adv_get(nss_phydev, adv);
	if (ret < 0)
		return ret;

	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD7_NUM,
			NSS_PHY_MMD7_8023AZ_EEE_PARTNER1);
		if (phy_data & NSS_PHY_MMD7_EEE_PARTNER_ADV_2500M)
			*adv |= EEE_2500BASE_T;
	}

	return 0;
}

static int qca808x_phy_eee_cap_get(struct nss_phy_device *nss_phydev,
	u32 *cap)
{
	u16 phy_data = 0;
	int ret = 0;

	*cap = 0;
	ret = nss_phy_common_eee_cap_get(nss_phydev, cap);
	if (ret < 0)
		return ret;

	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		phy_data = nss_phy_read_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
			NSS_PHY_MMD3_8023AZ_EEE_CAPABILITY1);
		if (phy_data & NSS_PHY_MMD3_EEE_CAPABILITY_2500M)
			*cap |= EEE_2500BASE_T;
	}

	return 0;
}

static int qca808x_phy_eee_status_get(struct nss_phy_device *nss_phydev,
	u32 *status)
{
	u32 adv = 0, lp_adv = 0;
	int ret = 0;

	ret = qca808x_phy_eee_adv_get(nss_phydev, &adv);
	if (ret < 0)
		return ret;
	ret = qca808x_phy_eee_partner_adv_get(nss_phydev, &lp_adv);
	if (ret < 0)
		return ret;

	*status = (adv & lp_adv);

	return 0;
}

static int qca808x_phy_8023az_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u32 eee_adv = 0;

	if (enable) {
		eee_adv = GE_EEE;
		if (nss_phy_id_check(nss_phydev, QCA8084_PHY,
			QCA_PHY_EXACT_MASK))
			eee_adv |= EEE_2500BASE_T;
	}

	return qca808x_phy_eee_adv_set(nss_phydev, eee_adv);
}

static int qca808x_phy_8023az_get(struct nss_phy_device *nss_phydev,
	u32 *enable)
{
	u32 eee_adv = 0, eee_cap = 0;
	int ret = 0;

	ret = qca808x_phy_eee_adv_get(nss_phydev, &eee_adv);
	if (ret < 0)
		return ret;
	ret = qca808x_phy_eee_cap_get(nss_phydev, &eee_cap);
	if (ret < 0)
		return ret;

	if (eee_adv == eee_cap)
		*enable = !NSS_PHY_FALSE;
	else
		*enable = NSS_PHY_FALSE;

	return 0;
}

static int
qca808x_phy_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable)
{
	u16 phy_data = 0, mask = 0;
	u32 autoneg = NSS_PHY_FALSE;
	int ret = 0;

	if (enable) {
		nss_phy_c45_common_force_speed_set(nss_phydev);
		phy_data |= NSS_PHY_LOCAL_LOOPBACK_EN | NSS_PHY_FULL_DUPLEX;
	} else {
		phy_data |= NSS_PHY_COMMON_CTRL;
		autoneg = !NSS_PHY_FALSE;
	}
	mask = NSS_PHY_AUTONEG_EN | NSS_PHY_LOCAL_LOOPBACK_EN |
		NSS_PHY_FULL_DUPLEX;

	ret = nss_phy_modify(nss_phydev, NSS_PHY_CONTROL, mask, phy_data);
	if (ret < 0)
		return ret;

	return nss_phy_common_autoneg_set(nss_phydev, autoneg);
}

#define QCA8084_LED_FUNC(lend_func, phy_index, source_id)		\
{		\
	if (phy_index == 0)		\
		lend_func = \
			QCA8084_PIN_FUNC_P0_LED_##source_id;		\
	else if (phy_index == 1)		\
		lend_func =		\
			QCA8084_PIN_FUNC_P1_LED_##source_id;		\
	else if (phy_index == 2)		\
		lend_func =		\
			QCA8084_PIN_FUNC_P2_LED_##source_id;		\
	else		\
		lend_func =		\
			QCA8084_PIN_FUNC_P3_LED_##source_id;		\
}

static int qca8084_phy_led_ctrl_source_pin_cfg
	(struct nss_phy_device *nss_phydev, u32 source_id)
{
	int ret = 0;
	u32 phy_index = 0, led_start_pin = 0, led_pin = 0, led_func = 0;

	phy_index = (nss_phy_addr_get(nss_phydev) -
		nss_phy_share_addr_get(nss_phydev));
	if (source_id == NSS_PHY_LED_SOURCE0) {
		led_start_pin = 2;
		QCA8084_LED_FUNC(led_func, phy_index, 0)
	} else if (source_id == NSS_PHY_LED_SOURCE1) {
		led_start_pin = 16;
		QCA8084_LED_FUNC(led_func, phy_index, 1)
	} else {
		led_start_pin = 6;
		QCA8084_LED_FUNC(led_func, phy_index, 2)
	}
	led_pin = led_start_pin + phy_index;
	nss_phy_dbg(nss_phydev, "phy_index:%d, led_pin:%d, led_func:%d",
		phy_index, led_pin, led_func);
	ret = qca8084_gpio_pin_mux_set(nss_phydev, led_pin, led_func);

	return ret;
}

static int
qca808x_phy_led_ctrl_source_set(struct nss_phy_device *nss_phydev,
	u32 source_id, struct nss_phy_led_pattern_ctrl *pattern)
{
	int ret = 0;

	ret = nss_phy_2500m_led_ctrl_source_set(nss_phydev, source_id,
		pattern);
	if (ret < 0)
		return ret;
	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		ret = qca8084_phy_led_ctrl_source_pin_cfg(nss_phydev,
			source_id);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int qca808x_phy_pll_on(struct nss_phy_device *nss_phydev)
{
	int ret;

	if (!nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK))
		return -NSS_PHY_EOPNOTSUPP;

	ret = nss_phy_modify_debug(nss_phydev,
		QCA8084_PHY_DEBUG_CONTROL_REGISTER0,
		QCA8084_PHY_DEBUG_1588_P2_EN,
		QCA8084_PHY_DEBUG_1588_P2_EN);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_debug(nss_phydev,
		QCA8084_PHY_DEBUG_AFE25_CMN_6_MII,
		QCA8084_PHY_DEBUG_AFE25_PLL_EN,
		QCA8084_PHY_DEBUG_AFE25_PLL_EN);

	nss_phy_mdelay(20);

	return ret;
}

static int qca808x_phy_pll_off(struct nss_phy_device *nss_phydev)
{
	int ret;

	if (!nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK))
		return -NSS_PHY_EOPNOTSUPP;

	ret = nss_phy_modify_debug(nss_phydev,
		QCA8084_PHY_DEBUG_CONTROL_REGISTER0,
		QCA8084_PHY_DEBUG_1588_P2_EN, 0);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_debug(nss_phydev,
		QCA8084_PHY_DEBUG_AFE25_CMN_6_MII,
		QCA8084_PHY_DEBUG_AFE25_PLL_EN, 0);

	return ret;
}

static int
qca8084_phy_cdt_thresh_init(struct nss_phy_device *nss_phydev)
{
	int ret;

	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL3,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL3_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL4,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL4_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL5,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL5_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL6,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL6_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL7,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL7_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL9,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL9_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL13,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL13_VAL);
	if (ret < 0)
		return ret;
	ret = nss_phy_write_mmd(nss_phydev, NSS_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL14,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL14_VAL);

	return ret;
}

int qca8084_phy_fixup(struct nss_phy_device *nss_phydev)
{
	int ret = 0;

	ret = qca8084_pinctrl_init(nss_phydev);
	if (ret < 0)
		return ret;
	ret = qca8084_phy_cdt_thresh_init(nss_phydev);
	if (ret < 0)
		return ret;

	return 0;
}

static int qca808x_phy_cdt(struct nss_phy_device *nss_phydev, u32 mdi_pair,
	enum nss_phy_cable_status *cable_status, u32 *cable_len)
{
	int ret;

	ret = nss_phy_common_cdt(nss_phydev, mdi_pair, cable_status, cable_len);
	if (ret < 0)
		return ret;
	/* CDT status open and short are reversed for QCA8084 PHY at */
	/* analog level */
	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		if (*cable_status == CABLE_OPENED)
			*cable_status = CABLE_SHORT;
		else if (*cable_status == CABLE_SHORT)
			*cable_status = CABLE_OPENED;
	}

	return 0;
}

static int qca8084_phy_intr_enable(struct nss_phy_device *nss_phydev,
	u32 intr_bmp)
{
	int ret;
	u32 phy_index = 0, data0 = 0, data1 = 0, mask0 = 0, mask1 = 0;

	phy_index =
	nss_phy_addr_get(nss_phydev) - nss_phy_share_addr_get(nss_phydev);
	mask0 = NSS_BIT(QCA8084_SOC_GLOBAL_INTR_ENABLE_PHY0_BOFFSET
	- phy_index);
	mask1 = NSS_BIT(phy_index);
	if (intr_bmp) {
		data0 |= mask0;
		if (intr_bmp & INTR_WOL)
			data1 |= mask1;
	}

	ret = nss_phy_modify_soc(nss_phydev, QCA8084_SOC_GLOBAL_INTR_ENABLE,
		mask0, data0);
	if (ret < 0)
		return ret;

	ret = nss_phy_modify_soc(nss_phydev, QCA8084_SOC_WOL_INTR_ENABLE,
		mask1, data1);

	return ret;
}

static int qca808x_phy_intr_mask_set(struct nss_phy_device *nss_phydev,
	u32 intr_mask)
{
	u16 phy_data = 0;
	int ret = 0;

	phy_data = nss_phy_c45_common_intr_to_reg(nss_phydev, intr_mask);

	if (intr_mask & INTR_FAST_LINK_DOWN_10M)
		phy_data |= QCA808X_PHY_INTR_FAST_LINK_DOWN_10M;
	if (intr_mask & INTR_SG_LINK_FAIL)
		phy_data |= QCA808X_PHY_INTR_SG_LINK_FAIL;
	if (intr_mask & INTR_SG_LINK_SUCCESS)
		phy_data |= QCA808X_PHY_INTR_SG_LINK_SUCCESS;

	ret = nss_phy_write(nss_phydev, NSS_PHY_INTR_MASK,
		phy_data);
	if (ret < 0)
		return ret;
	if (nss_phy_id_check(nss_phydev, QCA8084_PHY, QCA_PHY_EXACT_MASK)) {
		ret = qca8084_phy_intr_enable(nss_phydev, intr_mask);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int qca808x_phy_intr_mask_get(struct nss_phy_device *nss_phydev,
	u32 *intr_mask)

{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_INTR_MASK);

	*intr_mask = nss_phy_c45_common_intr_from_reg(nss_phydev,
		phy_data & (~QCA808X_PHY_INTR_MASK));

	if (phy_data & QCA808X_PHY_INTR_FAST_LINK_DOWN_10M)
		*intr_mask |= INTR_FAST_LINK_DOWN_10M;
	if (phy_data & QCA808X_PHY_INTR_SG_LINK_FAIL)
		*intr_mask |= INTR_SG_LINK_FAIL;
	if (phy_data & QCA808X_PHY_INTR_SG_LINK_SUCCESS)
		*intr_mask |= INTR_SG_LINK_SUCCESS;

	return 0;
}

static int qca808x_phy_intr_status_get(struct nss_phy_device *nss_phydev,
	u32 *intr_status)
{
	u16 phy_data = 0;

	phy_data = nss_phy_read(nss_phydev, NSS_PHY_INTR_STATUS);

	*intr_status = nss_phy_c45_common_intr_from_reg(nss_phydev,
		phy_data & (~QCA808X_PHY_INTR_MASK));

	if (phy_data & QCA808X_PHY_INTR_FAST_LINK_DOWN_10M)
		*intr_status |= INTR_FAST_LINK_DOWN_10M;
	if (phy_data & QCA808X_PHY_INTR_SG_LINK_FAIL)
		*intr_status |= INTR_SG_LINK_FAIL;
	if (phy_data & QCA808X_PHY_INTR_SG_LINK_SUCCESS)
		*intr_status |= INTR_SG_LINK_SUCCESS;

	return 0;
}

int qca808x_phy_ops_init(struct nss_phy_ops *ops)
{
	ops->hibernation_set = nss_phy_common_hibernation_set;
	ops->hibernation_get = nss_phy_common_hibernation_get;
	ops->function_reset = qca808x_phy_function_reset;
	ops->eee_adv_set = qca808x_phy_eee_adv_set;
	ops->eee_adv_get = qca808x_phy_eee_adv_get;
	ops->eee_partner_adv_get = qca808x_phy_eee_partner_adv_get;
	ops->eee_cap_get = qca808x_phy_eee_cap_get;
	ops->eee_status_get = qca808x_phy_eee_status_get;
	ops->ieee_8023az_set = qca808x_phy_8023az_set;
	ops->ieee_8023az_get = qca808x_phy_8023az_get;
	ops->local_loopback_set = qca808x_phy_local_loopback_set;
	ops->local_loopback_get = nss_phy_common_local_loopback_get;
	ops->remote_loopback_set = nss_phy_common_remote_loopback_set;
	ops->remote_loopback_get = nss_phy_common_remote_loopback_get;
	ops->led_ctrl_source_set = qca808x_phy_led_ctrl_source_set;
	ops->led_ctrl_source_get = nss_phy_2500m_led_ctrl_source_get;
	ops->pll_on = qca808x_phy_pll_on;
	ops->pll_off = qca808x_phy_pll_off;
	ops->cdt = qca808x_phy_cdt;
	ops->wol_set = nss_phy_common_wol_set;
	ops->wol_get = nss_phy_common_wol_get;
	ops->magic_frame_set = nss_phy_common_magic_frame_set;
	ops->magic_frame_get = nss_phy_common_magic_frame_get;
	ops->mdix_set = nss_phy_common_mdix_set;
	ops->mdix_get = nss_phy_common_mdix_get;
	ops->mdix_status_get = nss_phy_common_mdix_status_get;
	ops->stats_status_set = nss_phy_common_stats_status_set;
	ops->stats_status_get = nss_phy_common_stats_status_get;
	ops->stats_get = nss_phy_common_stats_get;
	ops->intr_mask_set = qca808x_phy_intr_mask_set;
	ops->intr_mask_get = qca808x_phy_intr_mask_get;
	ops->intr_status_get = qca808x_phy_intr_status_get;

	return 0;
}
