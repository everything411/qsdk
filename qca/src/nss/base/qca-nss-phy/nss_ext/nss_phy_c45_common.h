/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
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
#define NSS_PHY_MMD7_TX_CTRL		0x9022
#define NSS_PHY_MMD7_AUTONEG_STATUS		0x8001

/* PMA monitor control — MMD3 */
#define NSS_PHY_MMD3_PHY_MISC_CTRL0		0xa010
#define NSS_PHY_MMD3_PHY_PMA_MONITOR_EN0	BIT(2)
#define NSS_PHY_MMD3_PHY_MISC_CTRL1		0xa02f
#define NSS_PHY_MMD3_PHY_PMA_MONITOR_EN1	BIT(13)
#define NSS_PHY_MMD3_PHY_PMA_MONITOR_STATUS	0xa014
#define NSS_PHY_MMD3_PHY_PMA_MONITOR_MASK	GENMASK(6, 0)
#define NSS_PHY_MMD3_PHY_PMA_MONITOR_EN	0x40

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
#define NSS_PHY_MMD7_TX_ZERO		0x1
#define NSS_PHY_MMD7_AUTONEG_STATUS_MASK		0xf
#define NSS_PHY_MMD7_AUTONEG_INIT	0
#define NSS_PHY_MMD7_AUTONEG_TRANS_DIS	1
#define NSS_PHY_MMD7_AUTONEG_ADV_DETECT	2
#define NSS_PHY_MMD7_AUTONEG_ACK_DETECT		3
#define NSS_PHY_MMD7_AUTONEG_ACK_COMPLETE	4
#define NSS_PHY_MMD7_AUTONEG_FLP_LINK_GOOD_CHECK	5
#define NSS_PHY_MMD7_AUTONEG_FLP_LINK_GOOD	6
#define NSS_PHY_MMD7_AUTONEG_LINK_STATUS_CHECK	7
#define NSS_PHY_MMD7_AUTONEG_PARALLEL_DETECT_FAULT	8
#define NSS_PHY_MMD7_AUTONEG_NEXT_PAGE_WAIT	9

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

/* Fast Retrain (IEEE + Cisco proprietary) MMD registers.
 * "_REG" is only appended when the address macro would otherwise
 * collide with a same-named bit-field macro below (FR_TRIGGER,
 * FR_ENABLE_BYPASS).
 */
#define NSS_PHY_MMD7_FR_ADV		0x20
#define NSS_PHY_MMD7_LP_FR_ABILITY		0x21
#define NSS_PHY_MMD7_FR_THP_BYPASS_ADV		0x40
#define NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_41		0x41
#define NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_42		0x42
#define NSS_PHY_MMD3_CFR_CTRL		0xa038
#define NSS_PHY_MMD1_FR_CONTROL		0x93
#define NSS_PHY_MMD1_8201_FR_STATUS		0x8201
#define NSS_PHY_MMD3_FR_TRIGGER_REG		0xa012
#define NSS_PHY_MMD7_FR_BYPASS_25		0x9011
#define NSS_PHY_MMD7_FR_BYPASS_10_5		0x9013
#define NSS_PHY_MMD3_FR_ENABLE_BYPASS_REG		0xa03b

/* Fast Retrain MMD registers field */
#define NSS_PHY_MMD7_FR_ADV_10G		0x0002
#define NSS_PHY_MMD7_FR_ADV_2500M		0x0020
#define NSS_PHY_MMD7_FR_ADV_5000M		0x0040
#define NSS_PHY_MMD7_LP_FR_ABILITY_10G		0x0002
#define NSS_PHY_MMD7_LP_FR_ABILITY_2500M		0x0008
#define NSS_PHY_MMD7_LP_FR_ABILITY_5000M		0x0010
#define NSS_PHY_MMD7_FR_BYPASS_2500M		0x0100
#define NSS_PHY_MMD7_FR_BYPASS_10G		0x0002
#define NSS_PHY_MMD7_FR_BYPASS_5000M		0x0001
#define NSS_PHY_MMD7_FR_THP_BYPASS_ADV_2500M		0x0008
#define NSS_PHY_MMD7_FR_THP_BYPASS_ADV_5000M		0x0004
#define NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_2500M		0x0008
#define NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_5000M		0x0004
#define NSS_PHY_MMD7_LP_FR_THP_BYPASS_ABILITY_10G		0x0001
#define NSS_PHY_MMD3_CFR_ADV		0x0008
#define NSS_PHY_MMD3_CFR_THP_BYPASS_ADV		0x0004
#define NSS_PHY_MMD3_CFR_EXTEND_WAIT_ADV		0x0002
#define NSS_PHY_MMD3_CFR_DISABLE_TIMER_ADV		0x0001
#define NSS_PHY_MMD3_LP_CFR_ABILITY		0x0080
#define NSS_PHY_MMD3_LP_CFR_THP_BYPASS_ABILITY		0x0040
#define NSS_PHY_MMD3_LP_CFR_EXTEND_WAIT_ABILITY		0x0020
#define NSS_PHY_MMD3_LP_CFR_DISABLE_TIMER_ABILITY		0x0010
#define NSS_PHY_MMD3_FR_ENABLE_BYPASS		0x0040
#define NSS_PHY_MMD1_FR_ENABLE		0x0001
#define NSS_PHY_MMD1_FR_TX_CNT_MASK		0x07c0
#define NSS_PHY_MMD1_FR_TX_CNT_SHIFT		6
#define NSS_PHY_MMD1_FR_RX_CNT_MASK		0xf800
#define NSS_PHY_MMD1_FR_RX_CNT_SHIFT		11
/* 5-bit counters saturate at 31; reading 31 indicates possible overflow */
#define NSS_PHY_MMD1_FR_CNT_MAX		0x1f
#define NSS_PHY_MMD1_8201_FR_FAIL		0x0001
#define NSS_PHY_MMD1_8201_FR_SUCCESS		0x0002
#define NSS_PHY_MMD1_8201_FR_START		0x0008
#define NSS_PHY_MMD3_FR_TRIGGER		0x0002

/* PCS Status register (MMD3.0x20).
 * The bits below are only valid at NBase-T speeds (2.5G and above).
 */
#define NSS_PHY_MMD3_PCS_STATUS			0x20
#define NSS_PHY_MMD3_PCS_STATUS_LOCKED		0x1000	/* bit 12: PCS locked */
#define NSS_PHY_MMD3_PCS_STATUS_BLK_LOCK	0x0001	/* bit 0: block lock */

/* Link training completion bits in MMD7.0x8001 (vendor autoneg status register,
 * NSS_PHY_MMD7_AUTONEG_STATUS).  Only valid at 2.5G and above.
 * These bits are returned as the training_complete u32 by link_training_completion_get().
 */
#define NSS_PHY_MMD7_LINK_OK_10G		0x0400	/* bit 10: 10G training done */
#define NSS_PHY_MMD7_LINK_OK_5G			0x0200	/* bit 9:  5G training done */
#define NSS_PHY_MMD7_LINK_OK_2P5G		0x0100	/* bit 8:  2.5G training done */
#define NSS_PHY_MMD7_LINK_OK_MASK		0x0700	/* bits[10:8]: all training bits */

/* Bitmap values for training_complete (returned by link_training_completion_get(), mapped from
 * MMD7_reg_8001 bits[10:8] to a compact 3-bit field).
 */
#define NSS_PHY_TRAINING_COMPLETE_10G		BIT(2)
#define NSS_PHY_TRAINING_COMPLETE_5G		BIT(1)
#define NSS_PHY_TRAINING_COMPLETE_2P5G		BIT(0)

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
int nss_phy_c45_common_soft_reset(struct nss_phy_device *nss_phydev);
int nss_phy_c45_common_mdix_set(struct nss_phy_device *nss_phydev,
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
int nss_phy_c45_function_reset(struct nss_phy_device *nss_phydev,
	enum nss_phy_reset reset_type);
void nss_phy_c45_common_fr_state_init(struct nss_phy_device *nss_phydev);
int nss_phy_c45_common_fr_cfg_set(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_cfg *cfg);
int nss_phy_c45_common_fr_cfg_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_cfg *cfg);
int nss_phy_c45_common_fr_status_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_fr_status *status);
int nss_phy_c45_common_fr_cnt_poll(struct nss_phy_device *nss_phydev,
	bool link_up_transition);
int nss_phy_c45_common_fr_trigger(struct nss_phy_device *nss_phydev);
int nss_phy_c45_common_pcs_status_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_pcs_status *status);
int nss_phy_c45_common_link_training_completion_get(struct nss_phy_device *nss_phydev,
	u32 *training_complete);

/* MSE MMD registers (2.5G / 5G / 10G path, accessed via MMD3 = PCS) */
/* NSS_PHY_MMD3_MSE_2_5G_5G_10G_EN_REG == NSS_PHY_MMD3_PHY_MISC_CTRL0 (0xa010) */
#define NSS_PHY_MMD3_MSE_2_5G_5G_10G_EN	BIT(0)
#define NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH0	0xa005
#define NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH1	0xa008
#define NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH2	0xa00b
#define NSS_PHY_MMD3_MSE_2_5G_5G_10G_CH3	0xa00e

int nss_phy_c45_common_mse_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_mse *mse);

/* Master/slave control — MMD7 reg 0x20 (AN 10GBT Control), used by
 * Laguna, Huntington, and Hermosa. NAPA (QCA808X) uses MII reg 9 instead.
 */
#define NSS_PHY_AN_MS_CTRL_MASK		(BIT(15) | BIT(14) | BIT(13))
#define NSS_PHY_AN_MS_FORCE_EN		BIT(15)
#define NSS_PHY_AN_MS_MASTER_VAL	BIT(14)
#define NSS_PHY_AN_MS_PREFER_MASTER	BIT(13)

/* MMD7 0x21 bit 14: 1 = local PHY resolved as Master, 0 = local PHY resolved as Slave. */
#define NSS_PHY_AN_MS_RESOLUTION_MASTER	BIT(14)

/* 1G PPM offset (debug-port indirect access).
 * NSS_PHY_DEBUG_MSE_100M_1G_EN (BIT(0) of NSS_PHY_DEBUG_CONTROL_REGISTER0) doubles
 * as the PPM enable; the same bit enables both MSE and PPM capture.
 */
#define NSS_PHY_DEBUG_PPM_OFFSET	0x25

/* 2.5G / 5G / 10G PPM offset register (MMD3 / PCS).
 * NSS_PHY_MMD3_PHY_MISC_CTRL0 BIT(2) doubles as the PPM-database enable;
 * NSS_PHY_MMD3_PHY_MISC_CTRL1 BIT(13) doubles as the PPM capture trigger
 * (hardware self-clears after latching the offset).
 */
#define NSS_PHY_MMD3_PPM_OFFSET		0xa06f

/*
 * nss_phy_c45_common_clk_ppm_offset_get - Read the PHY clock frequency offset
 *
 * WARNING: this function has an intrusive side-effect — it temporarily forces
 * the PHY into slave role via ms_set (which internally restarts autoneg) to
 * obtain a stable readback.  A live link will be disrupted for up to ~15 s.
 * Do not call this from a context where brief link loss is unacceptable.
 *
 * Pass 0 for any divisor to mark that speed as unsupported.
 */
int nss_phy_c45_common_link_status_get(struct nss_phy_device *nss_phydev, bool is_c45);
int nss_phy_c45_common_clk_ppm_offset_get(struct nss_phy_device *nss_phydev,
	int *ppm, u64 div_1g, u64 div_2_5g, u64 div_5g_10g);

int nss_phy_c45_common_ms_set(struct nss_phy_device *nss_phydev,
	enum nss_phy_ms_mode mode);
int nss_phy_c45_common_ms_get(struct nss_phy_device *nss_phydev,
	enum nss_phy_ms_mode *mode);
int nss_phy_c45_common_ms_status_get(struct nss_phy_device *nss_phydev);
/* LDPC per-iteration bucket registers (MMD3) */
#define NSS_PHY_MMD3_LDPC_ITER0_A		0xa0ed
#define NSS_PHY_MMD3_LDPC_ITER0_B		0xa0ee
#define NSS_PHY_MMD3_LDPC_ITER0_C		0xa0ef
#define NSS_PHY_MMD3_LDPC_ITER0_D		0xa0f0
#define NSS_PHY_MMD3_LDPC_ITER1_A		0xa0d0
#define NSS_PHY_MMD3_LDPC_ITER1_B		0xa0d1
#define NSS_PHY_MMD3_LDPC_ITER1_C		0xa0d2
#define NSS_PHY_MMD3_LDPC_ITER1_D		0xa0d3
#define NSS_PHY_MMD3_LDPC_ITER2_A		0xa0d4
#define NSS_PHY_MMD3_LDPC_ITER2_B		0xa0d5
#define NSS_PHY_MMD3_LDPC_ITER2_C		0xa0d6
#define NSS_PHY_MMD3_LDPC_ITER2_D		0xa0d7
#define NSS_PHY_MMD3_LDPC_ITER3_A		0xa0d8
#define NSS_PHY_MMD3_LDPC_ITER3_B		0xa0d9
#define NSS_PHY_MMD3_LDPC_ITER3_C		0xa0da
#define NSS_PHY_MMD3_LDPC_ITER3_D		0xa0db
#define NSS_PHY_MMD3_LDPC_ITER4_A		0xa0dc
#define NSS_PHY_MMD3_LDPC_ITER4_B		0xa0dd
#define NSS_PHY_MMD3_LDPC_ITER4_C		0xa0de
#define NSS_PHY_MMD3_LDPC_ITER4_D		0xa0df
#define NSS_PHY_MMD3_LDPC_ITER_ERR		0xa064

int nss_phy_c45_common_ldpc_stats_get(struct nss_phy_device *nss_phydev,
	struct nss_phy_ldpc_stats *stats);

#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _NSS_PHY_C45_COMMON_H_ */
