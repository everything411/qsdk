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

#ifndef _QCA8084_PINCTRL_H_
#define _QCA8084_PINCTRL_H_

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************
 *
 *  1) PinCtrl/TLMM Register Definition
 *
 ****************************************************************************/
/* TLMM_GPIO_CFGn */
#define TLMM_GPIO_CFGN		0xC400000
#define TLMM_GPIO_CFGN_GPIO_HIHYS_EN		0x400
#define TLMM_GPIO_CFGN_GPIO_OE		0x200
#define TLMM_GPIO_CFGN_DRV_STRENGTH		0x1c0

#define TLMM_GPIO_CFGN_FUNC_SEL		0x3c
#define TLMM_GPIO_CFGN_GPIO_PULL		0x3

/* TLMM_GPIO_IN_OUTn */
#define TLMM_GPIO_IN_OUT_CTRL		0xC400004
#define GPIO_OUT_VAL		0x2
#define GPIO_IN_VAL		0x1

/* TLMM_CLK_GATE_EN */
#define TLMM_CLK_GATE_EN		0xC500000
#define TLMM_CLK_GATE_EN_AHB_HCLK_EN		0x4
#define TLMM_CLK_GATE_EN_SUMMARY_INTR_EN		0x2
#define TLMM_CLK_GATE_EN_CRIF_READ_EN		0x1

/* TLMM_HW_REVISION_NUMBER */
#define TLMM_HW_REVISION_NUMBER		0xC510010
#define TLMM_HW_REVISION_NUMBER_VERSION_ID_BOFFSET		0xf0000000
#define TLMM_HW_REVISION_NUMBER_PARTNUM		0x0ffff000
#define TLMM_HW_REVISION_NUMBER_MFG_ID_BOFFSET		0x00000ffe
#define TLMM_HW_REVISION_NUMBER_START		0x1

/****************************************************************************
 *
 *  2) PINs Functions Selection  GPIO_CFG[5:2] (FUNC_SEL)
 *
 ****************************************************************************/
/*GPIO*/
#define QCA8084_PIN_FUNC_GPIO0		0
#define QCA8084_PIN_FUNC_GPIO1		0
#define QCA8084_PIN_FUNC_GPIO2		0
#define QCA8084_PIN_FUNC_GPIO3		0
#define QCA8084_PIN_FUNC_GPIO4		0
#define QCA8084_PIN_FUNC_GPIO5		0
#define QCA8084_PIN_FUNC_GPIO6		0
#define QCA8084_PIN_FUNC_GPIO7		0
#define QCA8084_PIN_FUNC_GPIO8		0
#define QCA8084_PIN_FUNC_GPIO9		0
#define QCA8084_PIN_FUNC_GPIO10		0
#define QCA8084_PIN_FUNC_GPIO11		0
#define QCA8084_PIN_FUNC_GPIO12		0
#define QCA8084_PIN_FUNC_GPIO13		0
#define QCA8084_PIN_FUNC_GPIO14		0
#define QCA8084_PIN_FUNC_GPIO15		0
#define QCA8084_PIN_FUNC_GPIO16		0
#define QCA8084_PIN_FUNC_GPIO17		0
#define QCA8084_PIN_FUNC_GPIO18		0
#define QCA8084_PIN_FUNC_GPIO19		0
#define QCA8084_PIN_FUNC_GPIO20		0
#define QCA8084_PIN_FUNC_GPIO21		0

/*MINIMUM CONCURRENCY SET FUNCTION*/
#define QCA8084_PIN_FUNC_INTN_WOL		1
#define QCA8084_PIN_FUNC_INTN		1
#define QCA8084_PIN_FUNC_P0_LED_0		1
#define QCA8084_PIN_FUNC_P1_LED_0		1
#define QCA8084_PIN_FUNC_P2_LED_0		1
#define QCA8084_PIN_FUNC_P3_LED_0		1
#define QCA8084_PIN_FUNC_PPS_IN		1
#define QCA8084_PIN_FUNC_TOD_IN		1
#define QCA8084_PIN_FUNC_RTC_REFCLK_IN		1
#define QCA8084_PIN_FUNC_P0_PPS_OUT		1
#define QCA8084_PIN_FUNC_P1_PPS_OUT		1
#define QCA8084_PIN_FUNC_P2_PPS_OUT		1
#define QCA8084_PIN_FUNC_P3_PPS_OUT		1
#define QCA8084_PIN_FUNC_P0_TOD_OUT		1
#define QCA8084_PIN_FUNC_P0_CLK125_TDI		1
#define QCA8084_PIN_FUNC_P0_SYNC_CLKO_PTP		1
#define QCA8084_PIN_FUNC_P0_LED_1		1
#define QCA8084_PIN_FUNC_P1_LED_1		1
#define QCA8084_PIN_FUNC_P2_LED_1		1
#define QCA8084_PIN_FUNC_P3_LED_1		1
#define QCA8084_PIN_FUNC_MDC_M		1
#define QCA8084_PIN_FUNC_MDO_M		1

/*ALT FUNCTION K*/
#define QCA8084_PIN_FUNC_EVENT_TRG_I		2
#define QCA8084_PIN_FUNC_P0_EVENT_TRG_O		2
#define QCA8084_PIN_FUNC_P1_EVENT_TRG_O		2
#define QCA8084_PIN_FUNC_P2_EVENT_TRG_O		2
#define QCA8084_PIN_FUNC_P3_EVENT_TRG_O		2
#define QCA8084_PIN_FUNC_P1_TOD_OUT		2
#define QCA8084_PIN_FUNC_P1_CLK125_TDI		2
#define QCA8084_PIN_FUNC_P1_SYNC_CLKO_PTP		2
#define QCA8084_PIN_FUNC_P0_INTN_WOL		2
#define QCA8084_PIN_FUNC_P1_INTN_WOL		2
#define QCA8084_PIN_FUNC_P2_INTN_WOL		2
#define QCA8084_PIN_FUNC_P3_INTN_WOL		2

/*ALT FUNCTION L*/
#define QCA8084_PIN_FUNC_P2_TOD_OUT		3
#define QCA8084_PIN_FUNC_P2_CLK125_TDI		3
#define QCA8084_PIN_FUNC_P2_SYNC_CLKO_PTP		3

/*ALT FUNCTION M*/
#define QCA8084_PIN_FUNC_P3_TOD_OUT		4
#define QCA8084_PIN_FUNC_P3_CLK125_TDI		4
#define QCA8084_PIN_FUNC_P3_SYNC_CLKO_PTP		4

/*ALT FUNCTION N*/
#define QCA8084_PIN_FUNC_P0_LED_2		3
#define QCA8084_PIN_FUNC_P1_LED_2		2
#define QCA8084_PIN_FUNC_P2_LED_2		2
#define QCA8084_PIN_FUNC_P3_LED_2		3

/*ALT FUNCTION O*/

/*ALT FUNCTION DEBUG BUS OUT*/
#define QCA8084_PIN_FUNC_DBG_OUT_CLK		2
#define QCA8084_PIN_FUNC_DBG_BUS_OUT0		2
#define QCA8084_PIN_FUNC_DBG_BUS_OUT1		2
#define QCA8084_PIN_FUNC_DBG_BUS_OUT12		2
#define QCA8084_PIN_FUNC_DBG_BUS_OUT13		2
#define QCA8084_PIN_FUNC_DBG_BUS_OUT2		3
#define QCA8084_PIN_FUNC_DBG_BUS_OUT3		4
#define QCA8084_PIN_FUNC_DBG_BUS_OUT4		3
#define QCA8084_PIN_FUNC_DBG_BUS_OUT5		3
#define QCA8084_PIN_FUNC_DBG_BUS_OUT6		3
#define QCA8084_PIN_FUNC_DBG_BUS_OUT7		5
#define QCA8084_PIN_FUNC_DBG_BUS_OUT8		5
#define QCA8084_PIN_FUNC_DBG_BUS_OUT9		5
#define QCA8084_PIN_FUNC_DBG_BUS_OUT10		3
#define QCA8084_PIN_FUNC_DBG_BUS_OUT11		3
#define QCA8084_PIN_FUNC_DBG_BUS_OUT14		2
#define QCA8084_PIN_FUNC_DBG_BUS_OUT15		2

#define TLMM_GPIO_OFFSET		0x1000
#define TO_GPIIO_IN_OUT_REG(pin)		\
	(TLMM_GPIO_IN_OUT_CTRL + 0x1000*pin)
#define TO_GPIIO_CFGN_REG(pin)		\
	(TLMM_GPIO_CFGN + 0x1000*pin)

int qca8084_gpio_set_bit(struct nss_phy_device *nss_phydev,
	u32 pin, u32 value);
int qca8084_gpio_get_bit(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *data);
int qca8084_gpio_pin_mux_set(struct nss_phy_device *nss_phydev,
	u32 pin, u32 func);
int qca8084_gpio_pin_cfg_set_bias(struct nss_phy_device *nss_phydev,
	u32 pin, enum pin_config_param bias);
int qca8084_gpio_pin_cfg_get_bias(struct nss_phy_device *nss_phydev,
	u32 pin, enum pin_config_param *bias);
int qca8084_gpio_pin_cfg_set_drvs(struct nss_phy_device *nss_phydev,
	u32 pin, u32 drvs);
int qca8084_gpio_pin_cfg_get_drvs(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *drvs);
int qca8084_gpio_pin_cfg_set_oe(struct nss_phy_device *nss_phydev,
	u32 pin, u32 oe);
int qca8084_gpio_pin_cfg_get_oe(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *oe);
int qca8084_gpio_pin_cfg_set_hihys(struct nss_phy_device *nss_phydev,
	u32 pin, u32 hihys_en);
int qca8084_gpio_pin_cfg_get_hihys(struct nss_phy_device *nss_phydev,
	u32 pin, u32 *hihys);
int qca8084_pinctrl_init(struct nss_phy_device *nss_phydev);
#ifdef __cplusplus
}
#endif		/* __cplusplus */
#endif		/* _QCA8084_PINCTRL_H_ */
