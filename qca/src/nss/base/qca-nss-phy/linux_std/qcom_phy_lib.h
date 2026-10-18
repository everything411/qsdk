/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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
#include <linux/phy.h>

#define QCA8084_PHY_ID				0x004dd180

#define QCOM_PHY_PCS_PLL_POWER_ON_AND_RESET	0
#define QCOM_PHY_PCS_PLL_LOOP_CONTROL		6
#define QCOM_PHY_PCS_ANA_SOFT_RESET		0
#define QCOM_PHY_PCS_ANA_SOFT_RELEASE		0x40
#define QCOM_PHY_PCS_PLL_CML2CMS_IBSEL		0x30
#define QCOM_PHY_PCS_MMD1_CDA_CONTROL1		0x20
#define QCOM_PHY_PCS_MMD1_CALIBRATION4		 0x78
#define QCOM_PHY_PCS_MMD1_BYPASS_TUNING_IPG	0x189
#define QCOM_PHY_PCS_MMD1_MODE_CTRL		0x11b
#define QCOM_PHY_PCS_MMD1_CHANNEL0_CFG		0x120
#define QCOM_PHY_PCS_MMD1_GMII_DATAPASS_SEL	0x180
#define QCOM_PHY_PCS_MMD1_USXGMII_RESET		0x18c
#define QCOM_PHY_PCS_MMD1_BYPASS_TUNING_IPG_EN	0x0fff
#define QCOM_PHY_PCS_MMD1_XPCS_MODE		0x1000
#define QCOM_PHY_PCS_MMD1_SGMII_MODE		0x400
#define QCOM_PHY_PCS_MMD1_SGMII_PLUS_MODE	0x800
#define QCOM_PHY_PCS_MMD1_1000BASE_X		0x0
#define QCOM_PHY_PCS_MMD1_SGMII_PHY_MODE	0x10
#define QCOM_PHY_PCS_MMD1_SGMII_MAC_MODE	0x20
#define QCOM_PHY_PCS_MMD1_SGMII_MODE_CTRL_MASK	0x1f70
#define QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_MASK	0xe
#define QCOM_PHY_PCS_MMD1_CH0_AUTONEG_ENABLE	0x0
#define QCOM_PHY_PCS_MMD1_CH0_FORCE_ENABLE	0x8
#define QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_1G	0x4
#define QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_100M	0x2
#define QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_10M	0x0
#define QCOM_PHY_PCS_MMD1_DATAPASS_MASK		0x1
#define QCOM_PHY_PCS_MMD1_DATAPASS_USXGMII	0x1
#define QCOM_PHY_PCS_MMD1_DATAPASS_SGMII	0x0
#define QCOM_PHY_PCS_MMD1_CALIBRATION_DONE	0x80
#define QCOM_PHY_PCS_MMD1_SGMII_FUNC_RESET	0x10
#define QCOM_PHY_PCS_MMD1_SGMII_ADPT_RESET	0x800
#define QCOM_PHY_PCS_MMD1_SSCG_ENABLE		0x8
#define QCOM_PHY_PCS_MMD3_PCS_CTRL2		0x7
#define QCOM_PHY_PCS_MMD3_AN_LP_BASE_ABL2	0x14
#define QCOM_PHY_PCS_MMD3_10GBASE_R_PCS_STATUS1	0x20
#define QCOM_PHY_PCS_MMD3_DIG_CTRL1		0x8000
#define QCOM_PHY_PCS_MMD3_EEE_MODE_CTRL		0x8006
#define QCOM_PHY_PCS_MMD3_VR_RPCS_TPC		0x8007
#define QCOM_PHY_PCS_MMD3_EEE_TX_TIMER		0x8008
#define QCOM_PHY_PCS_MMD3_EEE_RX_TIMER		0x8009
#define QCOM_PHY_PCS_MMD3_MII_AM_INTERVAL	0x800a
#define QCOM_PHY_PCS_MMD3_EEE_MODE_CTRL1	0x800b
#define QCOM_PHY_PCS_MMD3_PCS_TYPE_10GBASE_R	0
#define QCOM_PHY_PCS_MMD3_10GBASE_R_UP		0x1000
#define QCOM_PHY_PCS_MMD3_USXGMII_EN		0x200
#define QCOM_PHY_PCS_MMD3_QXGMII_EN		0x1400
#define QCOM_PHY_PCS_MMD3_MII_AM_INTERVAL_VAL	0x6018
#define QCOM_PHY_PCS_MMD3_XPCS_SOFT_RESET	0x8000
#define QCOM_PHY_PCS_MMD3_XPCS_EEE_CAP		0x40
#define QCOM_PHY_PCS_MMD3_EEE_RES_REGS		0x100
#define QCOM_PHY_PCS_MMD3_EEE_SIGN_BIT_REGS	0x40
#define QCOM_PHY_PCS_MMD3_EEE_EN		0x3
#define QCOM_PHY_PCS_MMD3_EEE_TSL_REGS		0xa
#define QCOM_PHY_PCS_MMD3_EEE_TLU_REGS		0xc0
#define QCOM_PHY_PCS_MMD3_EEE_TWL_REGS		0x1600
#define QCOM_PHY_PCS_MMD3_EEE_100US_REG_REGS	0xc8
#define QCOM_PHY_PCS_MMD3_EEE_RWR_REG_REGS	0x1c00
#define QCOM_PHY_PCS_MMD3_EEE_TRANS_LPI_MODE	0x1
#define QCOM_PHY_PCS_MMD3_EEE_TRANS_RX_LPI_MODE	0x100
#define QCOM_PHY_PCS_MMD3_USXG_FIFO_RESET	0x400
#define QCOM_PHY_PCS_MMD_MII_CTRL		0
#define QCOM_PHY_PCS_MMD_MII_DIG_CTRL		0x8000
#define QCOM_PHY_PCS_MMD_MII_AN_INT_MSK		0x8001
#define QCOM_PHY_PCS_MMD_MII_ERR_SEL		0x8002
#define QCOM_PHY_PCS_MMD_MII_XAUI_MODE_CTRL	0x8004
#define QCOM_PHY_PCS_MMD_AN_COMPLETE_INT	0x1
#define QCOM_PHY_PCS_MMD_MII_4BITS_CTRL		0x0
#define QCOM_PHY_PCS_MMD_TX_CONFIG_CTRL		0x8
#define QCOM_PHY_PCS_MMD_MII_AN_ENABLE		0x1000
#define QCOM_PHY_PCS_MMD_MII_AN_RESTART		0x200
#define QCOM_PHY_PCS_MMD_MII_AN_COMPLETE_INT	0x1
#define QCOM_PHY_PCS_MMD_USXG_FIFO_RESET	0x20
#define QCOM_PHY_PCS_MMD_XPC_SPEED_MASK		0x2060
#define QCOM_PHY_PCS_MMD_XPC_SPEED_2500		0x20
#define QCOM_PHY_PCS_MMD_XPC_SPEED_1000		0x40
#define QCOM_PHY_PCS_MMD_XPC_SPEED_100		0x2000
#define QCOM_PHY_PCS_MMD_XPC_SPEED_10		0
#define QCOM_PHY_PCS_MMD_TX_IPG_CHECK_DISABLE	0x1
#define QCOM_PHY_PCS_MMD_PHY_MODE_CTRL_EN	0x1

enum qcom_phy_pcs_addr_offset {
	PCS0_ADDR_OFFSET = 4,
	PCS1_ADDR_OFFSET = 5,
	XPCS_ADDR_OFFSET = 6,
};

enum qcom_phy_pcs_clock_mode {
	CLOCK_MAC_MODE = 0,
	CLOCK_PHY_MODE = 1,
};

struct qcom_phy_pcs_cfg {
	u32 addr_offset;
	u32 type;
	u32 clock_mode;
	bool auto_neg;
	u32 force_speed;
};

enum qcom_phy_pcs_mode {
	PCS_MAC = QCOM_PHY_PCS_MMD1_SGMII_MAC_MODE,
	PCS_PHY = QCOM_PHY_PCS_MMD1_SGMII_PHY_MODE,
	PCS_SGMII = QCOM_PHY_PCS_MMD1_SGMII_MODE,
	PCS_2500X = QCOM_PHY_PCS_MMD1_SGMII_PLUS_MODE,
	PCS_QUSGMII = QCOM_PHY_PCS_MMD1_XPCS_MODE,
};

int qcom_phy_pcs_read_mmd(struct phy_device *phydev, int addr_offset,
	int devad, u32 regnum);

int qcom_phy_pcs_write_mmd(struct phy_device *phydev, int addr_offset,
	int devad, u32 regnum, u16 val);

int qcom_phy_pcs_modify_mmd(struct phy_device *phydev, int addr_offset,
	int devad, u32 regnum, u16 mask, u16 set);

int qcom_phy_xpcs_8023az_enable(struct phy_device *phydev,
	u32 addr_offset);

int qcom_phy_xpcs_qusgmii_function_reset(struct phy_device *phydev,
	u32 addr_offset, u32 channel);

int qcom_phy_xpcs_autoneg_restart(struct phy_device *phydev,
	u32 addr_offset, u32 channel);

int qcom_phy_pcs_usxgmii_reset(struct phy_device *phydev,
	u32 addr_offset, u32 channel);

int qcom_phy_pcs_sgmii_function_reset(struct phy_device *phydev,
	u32 addr_offset);

int qcom_phy_pcs_ipg_tune_reset(struct phy_device *phydev,
	u32 addr_offset);

int qcom_phy_sgmii_interface_fix_up(struct phy_device *phydev);

int qcom_phy_pcs_interface_set(struct phy_device *phydev,
	struct qcom_phy_pcs_cfg config);
int qcom_phy_pcs_speed_clock_set(struct phy_device *phydev,
	u32 channel, u32 speed);
bool qcom_phy_pcs_mode_check(struct phy_device *phydev,
	u32 offset, u32 pcs_mode);
