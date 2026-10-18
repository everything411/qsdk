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

#ifndef _NSS_PHY_C45_COMMON_H_
#define _NSS_PHY_C45_COMMON_H_

#ifdef __cplusplus
extern "C" {
#endif				/* __cplusplus */
#include "nss_phy_common.h"

#define NSS_PHY_INVALID_REG		0xffff
/*MMD registers*/
#define NSS_PHY_MMD1_PMA_CONTROL		0x0
#define NSS_PHY_MMD1_PMA_TTYPE		0x7
#define NSS_PHY_MMD3_8023AZ_EEE_CAPABILITY1		0x15
#define NSS_PHY_MMD3_10G_FRAME_CHECK_CTRL		0xa110
#define NSS_PHY_MMD3_10G_EGRESS_COUNTER_HIGH		0xa146
#define NSS_PHY_MMD3_10G_EGRESS_COUNTER_MIDDLE		0xa115
#define NSS_PHY_MMD3_10G_EGRESS_COUNTER_LOW		0xa114
#define NSS_PHY_MMD3_10G_EGRESS_ERROR_COUNTER		0xa116
#define NSS_PHY_MMD3_10G_INGRESS_COUNTER_HIGH		0xa145
#define NSS_PHY_MMD3_10G_INGRESS_COUNTER_MIDDLE		0xa119
#define NSS_PHY_MMD3_10G_INGRESS_COUNTER_LOW		0xa118
#define NSS_PHY_MMD3_10G_INGRESS_ERROR_COUNTER		0xa11a
#define NSS_PHY_MMD7_AN_CONTROL		0x0
#define NSS_PHY_MMD7_8023AZ_EEE_CTRL1		0x3e
#define NSS_PHY_MMD7_8023AZ_EEE_PARTNER1		0x3f
#define NSS_PHY_MMD7_LED0_CTRL		0x8078
#define NSS_PHY_MMD7_LED1_CTRL		0x8074
#define NSS_PHY_MMD7_LED2_CTRL		0x8076
#define NSS_PHY_MMD7_LED0_FORCE_CTRL		0x8079
#define NSS_PHY_MMD7_LED1_FORCE_CTRL		0x8075
#define NSS_PHY_MMD7_LED2_FORCE_CTRL		0x8077
#define NSS_PHY_MMD7_LED_10G_CTRL		0x807a

/*MMD registers field*/
#define NSS_PHY_MMD1_PMA_SPEED_MASK		0x207c
#define NSS_PHY_MMD1_PMA_CONTROL_10000M		0x2040
#define NSS_PHY_MMD1_PMA_CONTROL_5000M		0x205c
#define NSS_PHY_MMD1_PMA_CONTROL_2500M		0x2058
#define NSS_PHY_MMD1_PMA_CONTROL_1000M		0x40
#define NSS_PHY_MMD1_PMA_CONTROL_100M		0x2000
#define NSS_PHY_MMD1_PMA_CONTROL_10M		0x0
#define NSS_PHY_MMD1_PMA_TYPE_MASK		0x3f
#define NSS_PHY_MMD1_PMA_TYPE_10000M		0x9
#define NSS_PHY_MMD1_PMA_TYPE_5000M		0x31
#define NSS_PHY_MMD1_PMA_TYPE_2500M		0x30
#define NSS_PHY_MMD1_PMA_TYPE_1000M		0xc
#define NSS_PHY_MMD1_PMA_TYPE_100M		0xe
#define NSS_PHY_MMD1_PMA_TYPE_10M		0xf

#define NSS_PHY_MMD3_EEE_CAPABILITY_2500M		0x0001
#define NSS_PHY_MMD3_EEE_CAPABILITY_5000M		0x0002
#define NSS_PHY_MMD3_10G_FRAME_CHECK_EN		0x80

#define NSS_PHY_MMD7_EEE_MASK1		0x0003
#define NSS_PHY_MMD7_EEE_ADV_2500M		0x0001
#define NSS_PHY_MMD7_EEE_ADV_5000M		0x0002
#define NSS_PHY_MMD7_EEE_PARTNER_ADV_2500M		0x0001
#define NSS_PHY_MMD7_EEE_PARTNER_ADV_5000M		0x0002
#define NSS_PHY_MMD7_LINK_2500M_LIGHT_EN		0x8000
#define NSS_PHY_MMD7_10G_SRC0_OFFSET		0x7
#define NSS_PHY_MMD7_5G_SRC0_OFFSET		0x1

#define NSS_PHY_MMD7_LINK_5000M_LIGHT_EN		0x2
#define NSS_PHY_MMD7_LINK_10000M_LIGHT_EN		0x80

#define NSS_PHY_MMD31_INTR_FAST_LINK_DOWN		0x8000
#define NSS_PHY_MMD31_INTR_SEC_ENA		0x2000
#define NSS_PHY_MMD31_INTR_FAST_LINK_DOWN_100M		0x200
#define NSS_PHY_MMD31_INTR_FAST_LINK_DOWN_1000M	0x240
#define NSS_PHY_MMD31_INTR_FAST_LINK_RETRAIN_START		0x0100
#define NSS_PHY_MMD31_INTR_FAST_LINK_RETRAIN_END		0x0080
#define NSS_PHY_MMD31_INTR_DOWNSHIF		0x0020
#define NSS_PHY_MMD31_INTR_10MS_PTP		0x0010
#define NSS_PHY_MMD31_INTR_RX_PTP		0x0008
#define NSS_PHY_MMD31_INTR_TX_PTP		0x0004

int nss_phy_c45_common_eee_adv_set(struct nss_phy_device *nss_phydev,
	u32 adv);
int nss_phy_c45_common_eee_adv_get(struct nss_phy_device *nss_phydev,
	u32 *adv);
int nss_phy_c45_common_eee_partner_adv_get(struct nss_phy_device *nss_phydev,
	u32 *adv);
int nss_phy_c45_common_eee_cap_get(struct nss_phy_device *nss_phydev,
	u32 *cap);
int nss_phy_c45_common_eee_status_get(struct nss_phy_device *nss_phydev,
	u32 *status);
int nss_phy_c45_common_8023az_set(struct nss_phy_device *nss_phydev,
	u32 enable);
int nss_phy_c45_common_8023az_get(struct nss_phy_device *nss_phydev,
	u32 *enable);
int nss_phy_c45_common_autoneg_set(struct nss_phy_device *nss_phydev,
	u32 enable);
int nss_phy_c45_common_force_speed_set(struct nss_phy_device *nss_phydev);
int nss_phy_c45_common_pma_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable);
int nss_phy_c45_common_pma_local_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable);
int nss_phy_c45_common_pcs_local_loopback_set(struct nss_phy_device *nss_phydev,
	u32 enable);
int nss_phy_c45_common_pcs_local_loopback_get(struct nss_phy_device *nss_phydev,
	u32 *enable);
int nss_phy_c45_common_fifo_reset(struct nss_phy_device *nss_phydev,
	u32 enable);
int nss_phy_c45_common_autoneg_restart(struct nss_phy_device *nss_phydev);
int nss_phy_c45_common_cdt_start(struct nss_phy_device *nss_phydev);
int nss_phy_c45_common_mdix_mode_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_mode mode);
int nss_phy_c45_common_mdix_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_mode *mode);
int nss_phy_c45_common_mdix_status_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_mdix_status *mode);
int nss_phy_c45_common_stats_status_set(struct nss_phy_device *nss_phydev,
	u32 enable);
int nss_phy_c45_common_stats_status_get(struct nss_phy_device *nss_phydev,
	u32 *enable);
int nss_phy_c45_common_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_stats_info *cnt_info);
u16 nss_phy_c45_common_intr_to_reg(struct nss_phy_device *nss_phydev,
	u32 mask);
u32 nss_phy_c45_common_intr_from_reg(struct nss_phy_device *nss_phydev,
	u16 phy_data);
int
nss_phy_c45_common_intr_mask_set(struct nss_phy_device *nss_phydev,
	u32 intr_mask);
int
nss_phy_c45_common_intr_mask_get(struct nss_phy_device *nss_phydev,
	u32 *intr_mask);
int
nss_phy_c45_common_intr_status_get(struct nss_phy_device *nss_phydev,
	u32 *intr_status);
int nss_phy_c45_common_led_force_set
	(struct nss_phy_device *nss_phydev, u32 source_id, u32 enable,
	u32 force_mode);
int nss_phy_c45_common_led_force_get
	(struct nss_phy_device *nss_phydev, u32 source_id, u32 *enable,
	u32 *force_mode);
int nss_phy_2500m_led_ctrl_source_set
	(struct nss_phy_device *nss_phydev, u32 source_id,
	struct nss_phy_led_pattern_ctrl *pattern);
int nss_phy_2500m_led_ctrl_source_get
	(struct nss_phy_device *nss_phydev, u32 source_id,
	struct nss_phy_led_pattern_ctrl *pattern);
int nss_phy_c45_common_led_ctrl_source_set
	(struct nss_phy_device *nss_phydev, u32 source_id,
	struct nss_phy_led_pattern_ctrl *pattern);
int nss_phy_c45_common_led_ctrl_source_get
	(struct nss_phy_device *nss_phydev, u32 source_id,
	struct nss_phy_led_pattern_ctrl *pattern);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _NSS_PHY_C45_COMMON_H_ */
