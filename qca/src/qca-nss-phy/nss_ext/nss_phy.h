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

#ifndef _NSS_PHY_H_
#define _NSS_PHY_H_

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */
#include "nss_phy_lib.h"
#include "nss_phy_ptp.h"

#define QCA_PHY_EXACT_MASK		0xffffffff
#define QCA_PHY_ID		0x004dd000
#define QCA_PHY_MASK		0xfffff000
#define QCA_PHY_MATCH(phy_id)		((phy_id & QCA_PHY_MASK) == QCA_PHY_ID)
#define QCA8075_PHY		0x004dd0b1
#define QCA8072_PHY		0x004dd0b2
#define QCA807X_MASK		0xfffffff0
#define QCA8111_PHY		0x004dd1c0
#define QCA81XX_MASK		0xfffffff0
#define QCA8081_PHY		0x004dd101
#define QCA8084_PHY		0x004dd180
#define QCA808X_MASK		0xffffff00
#define QCA8337_PHY_V1		0x004DD033
#define QCA8337_PHY_V2		0x004DD034
#define QCA8337_PHY_V3		0x004DD035
#define QCA8337_PHY_V4		0x004DD036
#define QCA8337_PHY_MASK		0xfffffff0
#define QCA8030_PHY		0x004DD076
#define QCA8033_PHY		0x004DD074
#define QCA8035_PHY		0x004DD072
#define QCA803X_MASK		0xfffffff0

#define NSS_BIT(_n)		(1UL << (_n))
#define NSS_PHY_FALSE		0
#define EEE_100BASE_T		0x2
#define EEE_1000BASE_T		0x4
#define EEE_2500BASE_T		0x8
#define EEE_5000BASE_T		0x10
#define EEE_10000BASE_T		0x20
#define GE_EEE		(EEE_100BASE_T | EEE_1000BASE_T)
#define XGE_EEE		(EEE_2500BASE_T | EEE_5000BASE_T | EEE_10000BASE_T)
#define ALL_SPEED_EEE		(GE_EEE | XGE_EEE)

#define LED_SOURCE_MAX		0x3
#define LED_FULL_DUPLEX_LIGHT_EN		0
#define LED_HALF_DUPLEX_LIGHT_EN		1
#define LED_POWER_ON_LIGHT_EN		2
#define LED_LINK_1000M_LIGHT_EN		3
#define LED_LINK_100M_LIGHT_EN		4
#define LED_LINK_10M_LIGHT_EN		5
#define LED_COLLISION_BLINK_EN		6
#define LED_RX_TRAFFIC_BLINK_EN		7
#define LED_TX_TRAFFIC_BLINK_EN		8
#define LED_LINKUP_OVERRIDE_EN		9
#define LED_LINK_2500M_LIGHT_EN		10
#define LED_LINK_5000M_LIGHT_EN		11
#define LED_LINK_10000M_LIGHT_EN		12
#define LED_ACTIVE_HIGH		1
#define LED_ACTIVE_LOW		0

#define INTR_SPEED		0x1
#define INTR_DUPLEX		0x2
#define INTR_LINK_UP		0x4
#define INTR_LINK_DOWN		0x8
#define INTR_BX_FX_LINK_UP		0x10
#define INTR_BX_FX_LINK_DOWN		0x20
#define INTR_MEDIA_TYPE		0x40
#define INTR_WOL		0x80
#define INTR_POE		0x100
#define INTR_RX_PTP		0x200
#define INTR_TX_PTP		0x400
#define INTR_10MS_PTP		0x800
#define INTR_DOWNSHIF		0x1000
#define INTR_SG_LINK_SUCCESS		0x2000
#define INTR_SG_LINK_FAIL		0x4000
#define INTR_FAST_LINK_DOWN_1000M		0x8000
#define INTR_FAST_LINK_DOWN_100M		0x10000
#define INTR_FAST_LINK_DOWN_10M		0x20000
#define INTR_SEC_ENA		0x40000
#define INTR_FAST_LINK_DOWN		0x80000
#define INTR_FAST_LINK_RETRAIN_END		0x100000
#define INTR_FAST_LINK_RETRAIN_START		0x200000

enum nss_phy_reset {
	FIFO_RESET = 0,
	SERDES_RESET,
	SOFT_RESET,
};

enum nss_phy_medium {
	MEDIUM_COPPER = 0,
	MEDIUM_FIBER,
};

enum nss_phy_reg_pages {
	PAGE_FIBER = 0,
	PAGE_COPPER,
};

enum nss_phy_fiber_mode {
	FIBER_100FX = 0,
	FIBER_1000BX,
};

enum nss_phy_led_pattern {
	ALWAYS_OFF = 0,
	ALWAYS_BLINK,
	ALWAYS_ON,
	ACT_PHY_STATUS,
};

enum nss_phy_led_blink_freq {
	BLINK_2HZ = 0,
	BLINK_4HZ,
	BLINK_8HZ,
	BLINK_16HZ,
	BLINK_32HZ,
	BLINK_64HZ,
	BLINK_128HZ,
	BLINK_256HZ,
};

struct nss_phy_led_pattern_ctrl {
	enum nss_phy_led_pattern mode;
	u32 phy_status_bmap;
	enum nss_phy_led_blink_freq freq;
	u32 active_level;
};

enum nss_phy_cable_status {
	CABLE_NORMAL = 0,
	CABLE_SHORT,
	CABLE_OPENED,
	CABLE_INVALID,
};

struct nss_phy_mac {
	u8 uc[6];
};

enum nss_phy_mdix_mode {
	MODE_AUTO = 0,
	MODE_MDI,
	MODE_MDIX
};

enum nss_phy_mdix_status      {
	STATUS_MDI = 0,
	STATUS_MDIX = 1
};

struct nss_phy_stats_info {
	u64 RxGoodFrame;
	u64 RxFcsErr;
	u64 TxGoodFrame;
	u64 TxFcsErr;
	u64 SysRxGoodFrame;
	u64 SysRxFcsErr;
	u64 SysTxGoodFrame;
	u64 SysTxFcsErr;
};

enum NSS_PHY_PIN_DRV_STRENGTH {
	DRV_STRENGTH_2_MA,
	DRV_STRENGTH_4_MA,
	DRV_STRENGTH_6_MA,
	DRV_STRENGTH_8_MA,
	DRV_STRENGTH_10_MA,
	DRV_STRENGTH_12_MA,
	DRV_STRENGTH_14_MA,
	DRV_STRENGTH_16_MA,
};

enum NSS_PHY_PIN_PARAM {
	PULL_DISABLE,/*Disables all pull*/
	PULL_DOWN,
	PULL_BUS_HOLD,/*Weak Keepers*/
	PULL_UP,
};

/****************************************************************************
 *
 *  2) PINs Functions Selection  GPIO_CFG[5:2] (FUNC_SEL)
 *
 ****************************************************************************/
struct nss_phy_pinctrl_mux {
	u32 pin;
	u32 func;
};

struct nss_phy_pinctrl_configs {
	u32 pin;
	u32 num_configs;
	u_long *configs;
};

struct nss_phy_pinctrl_setting {
	enum nss_phy_pinctrl_map_type type;
	union {
		struct nss_phy_pinctrl_mux mux;
		struct nss_phy_pinctrl_configs configs;
	} data;
};

#define NSS_PHY_PIN_SETTING_MUX(pin_id, function)	\
{								\
	.type = NSS_PHY_PIN_MAP_TYPE_MUX_GROUP,	\
	.data.mux = {						\
		.pin = pin_id,					\
		.func = function				\
	},							\
}

#define NSS_PHY_PIN_SETTING_CONFIG(pin_id, cfgs)	\
{								\
	.type = NSS_PHY_PIN_MAP_TYPE_CONFIGS_PIN,	\
	.data.configs = {						\
		.pin = pin_id,					\
		.configs = cfgs,				\
		.num_configs = ARRAY_SIZE(cfgs)				\
	},							\
}

struct nss_phy_ops {
	int (*hibernation_set)(struct nss_phy_device *nss_phydev, u32 enable);
	int (*hibernation_get)(struct nss_phy_device *nss_phydev, u32 *enable);
	int (*powersave_set)(struct nss_phy_device *nss_phydev, u32 enable);
	int (*powersave_get)(struct nss_phy_device *nss_phydev, u32 *enable);
	int (*function_reset)(struct nss_phy_device *nss_phydev,
		enum nss_phy_reset reset_type);
	int (*interface_set)(struct nss_phy_device *nss_phydev,
		nss_phy_interface_t interface);
	int (*interface_get)(struct nss_phy_device *nss_phydev,
		 nss_phy_interface_t *interface);
	int (*eee_adv_set)(struct nss_phy_device *nss_phydev, u32 adv);
	int (*eee_adv_get)(struct nss_phy_device *nss_phydev, u32 *adv);
	int (*eee_partner_adv_get)(struct nss_phy_device *nss_phydev, u32 *adv);
	int (*eee_cap_get)(struct nss_phy_device *nss_phydev, u32 *cap);
	int (*eee_status_get)(struct nss_phy_device *nss_phydev, u32 *status);
	int (*ieee_8023az_set)(struct nss_phy_device *nss_phydev, u32 enable);
	int (*ieee_8023az_get)(struct nss_phy_device *nss_phydev, u32 *enable);
	int (*local_loopback_set)(struct nss_phy_device *nss_phydev,
		u32 enable);
	int (*local_loopback_get)(struct nss_phy_device *nss_phydev,
		u32 *enable);
	int (*remote_loopback_set)(struct nss_phy_device *nss_phydev,
		u32 enable);
	int (*remote_loopback_get)(struct nss_phy_device *nss_phydev,
		u32 *enable);
	int (*combo_prefer_medium_set)(struct nss_phy_device *nss_phydev,
		enum nss_phy_medium phy_medium);
	int (*combo_prefer_medium_get)(struct nss_phy_device *nss_phydev,
		enum nss_phy_medium *phy_medium);
	int (*combo_medium_status_get)(struct nss_phy_device *nss_phydev,
		enum nss_phy_medium *phy_medium);
	int (*combo_fiber_mode_set)(struct nss_phy_device *nss_phydev,
		enum nss_phy_fiber_mode fiber_mode);
	int (*combo_fiber_mode_get)(struct nss_phy_device *nss_phydev,
		enum nss_phy_fiber_mode *fiber_mode);
	int (*led_ctrl_source_set)(struct nss_phy_device *nss_phydev,
		u32 source_id, struct nss_phy_led_pattern_ctrl *pattern);
	int (*led_ctrl_source_get)(struct nss_phy_device *nss_phydev,
		u32 source_id, struct nss_phy_led_pattern_ctrl *pattern);
	int (*pll_on)(struct nss_phy_device *nss_phydev);
	int (*pll_off)(struct nss_phy_device *nss_phydev);
	int (*ldo_set)(struct nss_phy_device *nss_phydev, u32 enable);
	int (*cdt)(struct nss_phy_device *nss_phydev, u32 pair,
		enum nss_phy_cable_status *cable_status, u32 *cable_len);
	int (*wol_set)(struct nss_phy_device *nss_phydev, u32 enable);
	int (*wol_get)(struct nss_phy_device *nss_phydev, u32 *enable);
	int (*magic_frame_set)(struct nss_phy_device *nss_phydev,
		struct nss_phy_mac *mac);
	int (*magic_frame_get)(struct nss_phy_device *nss_phydev,
		struct nss_phy_mac *mac);
	int (*mdix_set)(struct nss_phy_device *nss_phydev,
		enum nss_phy_mdix_mode mode);
	int (*mdix_get)(struct nss_phy_device *nss_phydev,
		enum nss_phy_mdix_mode *mode);
	int (*mdix_status_get)(struct nss_phy_device *nss_phydev,
		enum nss_phy_mdix_status *mode);
	int (*stats_status_set)(struct nss_phy_device *nss_phydev, u32 enable);
	int (*stats_status_get)(struct nss_phy_device *nss_phydev, u32 *enable);
	int (*stats_get)(struct nss_phy_device *nss_phydev,
		struct nss_phy_stats_info *cnt);
	int (*intr_mask_set)(struct nss_phy_device *nss_phydev, u32 mask);
	int (*intr_mask_get)(struct nss_phy_device *nss_phydev, u32 *mask);
	int (*intr_status_get)(struct nss_phy_device *nss_phydev, u32 *status);
	/*below APIs can be covered by linux std driver and nss ext driver*/
	int (*speed_get)(struct nss_phy_device *nss_phydev, u32 *speed);
	int (*speed_set)(struct nss_phy_device *nss_phydev, u32 speed);
	int (*duplex_get)(struct nss_phy_device *nss_phydev, u32 *duplex);
	int (*duplex_set)(struct nss_phy_device *nss_phydev, u32 duplex);
	int (*autoneg_enable)(struct nss_phy_device *nss_phydev);
	int (*autoneg_status_get)(struct nss_phy_device *nss_phydev, u32 *status);
	int (*autoneg_restart)(struct nss_phy_device *nss_phydev);
	int (*autoadv_get)(struct nss_phy_device *nss_phydev, u32 *adv);
	int (*autoadv_set)(struct nss_phy_device *nss_phydev, u32 adv);
	int (*reset)(struct nss_phy_device *nss_phydev);
	int (*power_on)(struct nss_phy_device *nss_phydev);
	int (*power_off)(struct nss_phy_device *nss_phydev);
	int (*interface_mode_status_get)(struct nss_phy_device *nss_phydev,
		u32 *status);
	int (*phyid_get)(struct nss_phy_device *nss_phydev, u16 *org_id, u16 *rev_id);
	int (*link_status_get)(struct nss_phy_device *nss_phydev, u32 *status);
	int (*adjust_link_post)(struct nss_phy_device *nss_phydev);
	struct nss_phy_ptp_ops *ptp_ops;
};
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _NSS_PHY_H_ */
