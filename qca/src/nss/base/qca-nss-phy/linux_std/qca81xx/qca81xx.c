/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/ethtool_netlink.h>
#include "qca81xx.h"
#include <linux/etherdevice.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/timekeeping.h>

static void
qca81xx_priv_atomic64_inc(struct phy_device *phydev, atomic64_t *v)
{
	struct qca81xx_private *priv = phydev->priv;

	if (priv)
		atomic64_inc(v);
}

static void
qca81xx_set_init_state(struct phy_device *phydev, enum qca81xx_init_state state)
{
	struct qca81xx_private *priv = phydev->priv;

	if (priv)
		priv->init_state = state;
}

enum qca81xx_addr_offset {
	PCS_ADDR_OFFSET = 1,
	SOC_ADDR_OFFSET = 2,
};

struct qca81xx_phy_mdio_data {
	void __iomem	*membase[2];
	void __iomem *eth_ldo_rdy[3];
	int clk_div;
	bool force_c22;
	struct gpio_descs *reset_gpios;
	void (*preinit)(struct mii_bus *bus);
	u32 (*sw_read)(struct mii_bus *bus, u32 reg);
	void (*sw_write)(struct mii_bus *bus, u32 reg, u32 val);
	struct clk *clk[5];
	void *i2c;
};

#define INVALID_MMD_REG				0xffff

/* below two registers are used to access PHY */
/* DEBUG registers indirectly */
#define QCA81XX_DEBUG_ADDR			0x1d
#define QCA81XX_DEBUG_DATA			0x1e

/*PHY DEBUG registers*/
#define QCA81XX_DEBUG_ANA_PLL_JITTER0_CTRL	0x580
#define QCA81XX_DEBUG_ANA_PLL_JITTER0_VAL	0x40
#define QCA81XX_DEBUG_ANA_PLL_JITTER1_CTRL	0x2180
#define QCA81XX_DEBUG_ANA_PLL_JITTER1_VAL	0x1132
#define QCA81XX_DEBUG_ANA_PLL_JITTER2_CTRL	0x2280
#define QCA81XX_DEBUG_ANA_PLL_JITTER2_VAL	0x2136
#define QCA81XX_DEBUG_ANA_RESISTOR0_CTRL	0x2f80
#define QCA81XX_DEBUG_ANA_RESISTOR0_VAL		0x6878
#define QCA81XX_DEBUG_ANA_RESISTOR1_CTRL	0x3080
#define QCA81XX_DEBUG_ANA_RESISTOR1_VAL		0x6868
#define QCA81XX_DEBUG_ANA_EDAC_CAP0_CTRL	0x4d80
#define QCA81XX_DEBUG_ANA_EDAC_CAP0_VAL		0x2528
#define QCA81XX_DEBUG_ANA_EDAC_CAP1_CTRL	0x4e80
#define QCA81XX_DEBUG_ANA_EDAC_CAP1_VAL		0x2825
#define QCA81XX_DEBUG_ANA_AFE_DAC8_DP		0x2f80
#define QCA81XX_DEBUG_ANA_EDAC_RES_VAL0		0x5868
#define QCA81XX_DEBUG_ANA_AFE_DAC9_DP		0x3080
#define QCA81XX_DEBUG_ANA_EDAC_RES_VAL1		0x6858
#define QCA81XX_DEBUG_ANA_RX_SENS_CTRL		0xbd80
#define QCA81XX_DEBUG_ANA_RX_SENS_MASK		GENMASK(12, 0)
#define QCA81XX_DEBUG_ANA_RX_SENS_VAL		0xdc3
#define QCA81XX_DEBUG_ANA_OPEN_RAMPING_CRTL0	0x9180
#define QCA81XX_DEBUG_ANA_OPEN_RAMPING_EN	BIT(7)
#define QCA81XX_DEBUG_HIBERNATION_STATUS_REG	0xc
#define QCA81XX_DEBUG_HIBERNATION_BIT		BIT(11)
#define QCA81XX_DEBUG_SYSTEM_MODE_CTRL		0x5
#define QCA81XX_DEBUG_RETRAIN_EN_BIT		BIT(15)

/*PHY MMD1 registers*/
#define QCA81XX_MMD1_2P5G_VGA_BW_CTRL		0x8108
#define QCA81XX_MMD1_2P5G_VGA_BW_VAL		0x1b
#define QCA81XX_MMD1_FFE_COEF1_CTRL		0x801B
#define QCA81XX_MMD1_FFE_COEF1_VAL		BIT(3)
#define QCA81XX_MMD1_SNR_THRESHOLD_CTRL		0x8020
#define QCA81XX_MMD1_SNR_THRESHOLD_MASK		GENMASK(10, 0)
#define QCA81XX_MMD1_SNR_THRESHOLD_VAL		0x419
#define QCA81XX_MMD1_10G_VGA_GAIN_CTRL		0x8022
#define QCA81XX_MMD1_10G_VGA_GAIN_VAL		0x5dd9

/*PHY MMD3 registers*/
#define QCA81XX_MMD3_CDT_THRESH_CTRL2		0x8073
#define QCA81XX_MMD3_CDT_THRESH_CTRL2_VAL	0xb03f
#define QCA81XX_MMD3_CDT_THRESH_CTRL3		0x8074
#define QCA81XX_MMD3_CDT_THRESH_CTRL3_VAL	0xc040
#define QCA81XX_MMD3_CDT_THRESH_CTRL4		0x8075
#define QCA81XX_MMD3_CDT_THRESH_CTRL4_VAL	0xa060
#define QCA81XX_MMD3_CDT_THRESH_CTRL5		0x8076
#define QCA81XX_MMD3_CDT_THRESH_CTRL5_VAL	0xc040
#define QCA81XX_MMD3_CDT_THRESH_CTRL6		0x8077
#define QCA81XX_MMD3_CDT_THRESH_CTRL6_VAL	0xa060
#define QCA81XX_MMD3_CDT_THRESH_CTRL7		0x8078
#define QCA81XX_MMD3_CDT_THRESH_CTRL7_VAL	0xae50
#define QCA81XX_MMD3_CDT_THRESH_CTRL9		0x807a
#define QCA81XX_MMD3_CDT_THRESH_CTRL9_VAL	0xc060
#define QCA81XX_MMD3_CDT_THRESH_CTRL13		0x807e
#define QCA81XX_MMD3_CDT_THRESH_CTRL13_VAL	0xb060
#define QCA81XX_MMD3_CDT_THRESH_CTRL14		0x807f
#define QCA81XX_MMD3_CDT_THRESH_CTRL14_VAL	0x9cb0
#define QCA81XX_MMD3_AZ_1G_AFE_CTRL		0x8007
#define QCA81XX_MMD3_AZ_1G_AFE_CTRL_MASK	GENMASK(8, 4)
#define QCA81XX_MMD3_AZ_1G_DAC_EN		BIT(4)
#define QCA81XX_MMD3_AZ_1G_VGA_EN		BIT(5)
#define QCA81XX_MMD3_AZ_1G_ADC_EN		BIT(6)
#define QCA81XX_MMD3_AZ_1G_ECHO_EN		BIT(7)
#define QCA81XX_MMD3_AZ_1G_FULL_ECHO_EN		BIT(8)
#define QCA81XX_MMD3_NOISE_SMOOTH_CTRL_H	0xa04a
#define QCA81XX_MMD3_NOISE_AVERAGE_CNT_SEL0	BIT(2)
#define QCA81XX_MMD3_NOISE_SMOOTH_CTRL_L	0xa056
#define QCA81XX_MMD3_NOISE_AVERAGE_CNT_SEL1	BIT(7)
#define QCA81XX_MMD3_REDUCE_NOISE_CTRL		0xa016
#define QCA81XX_MMD3_REDUCE_NOISE_EN		BIT(13)
#define QCA81XX_MMD3_10G_EEE_CFG		0xa04b
#define QCA81XX_MMD3_10G_EEE_CFG_MASK		GENMASK(15, 0) /* Full register configuration */
#define QCA81XX_MMD3_10G_EEE_MST_CFG		0x931c /* Master: [15:8]=0x93, [7:0]=0x1c */
#define QCA81XX_MMD3_10G_EEE_SLV_CFG		0x351c /* Slave: [15:8]=0x35, [7:0]=0x1c */
#define QCA81XX_MMD3_ROTCLK_CTRL		0xa034
#define QCA81XX_MMD3_ROTCLK_SEL			BIT(14)
#define QCA81XX_MMD3_CDR_TRACING_CTRL		0xa101
#define QCA81XX_MMD3_CDR_TRACING_ACCEL		BIT(14)
#define QCA81XX_MMD3_FFE_COEF0_CTRL		0xa02a
#define QCA81XX_MMD3_FFE_COEF0_VAL		BIT(2)
#define QCA81XX_MMD3_FFE_COEF2_CTRL		0xa015
#define QCA81XX_MMD3_FFE_COEF2_VAL		BIT(5)
#define QCA81XX_MMD3_FFE_A2D_FIFO_DELAY_MASK	GENMASK(15, 13)
#define QCA81XX_MMD3_FFE_A2D_FIFO_DELAY_SEL	0xc000
#define QCA81XX_MMD3_PHY_MISC_CTRL0		0xa010
#define QCA81XX_MMD3_PHY_PMA_MONITOR_EN0	BIT(2)
#define QCA81XX_MMD3_PHY_MISC_CTRL1		0xa02F
#define QCA81XX_MMD3_PHY_PMA_MONITOR_EN1	BIT(13)
#define QCA81XX_MMD3_PHY_SNR_MONITOR_STATUS	0xa014
#define QCA81XX_MMD3_PHY_SNR_MONITOR_MASK	GENMASK(6, 0)
#define QCA81XX_MMD3_PHY_SNR_MONITOR_EN		0x40
#define QCA81XX_MMD3_10G_FRAME_CHECK_CTRL	0xa110
#define QCA81XX_MMD3_10G_EGRESS_COUNTER_HIGH	0xa146
#define QCA81XX_MMD3_10G_EGRESS_COUNTER_MIDDLE	0xa115
#define QCA81XX_MMD3_10G_EGRESS_COUNTER_LOW	0xa114
#define QCA81XX_MMD3_10G_EGRESS_ERROR_COUNTER	0xa116
#define QCA81XX_MMD3_10G_INGRESS_COUNTER_HIGH	0xa145
#define QCA81XX_MMD3_10G_INGRESS_COUNTER_MIDDLE	0xa119
#define QCA81XX_MMD3_10G_INGRESS_COUNTER_LOW	0xa118
#define QCA81XX_MMD3_10G_INGRESS_ERROR_COUNTER	0xa11a
#define QCA81XX_MMD3_10G_FRAME_CHECK_EN		0x80
#define QCA81XX_MMD3_CABLE_SKEW			0xa102
#define QCA81XX_MMD3_CABLE_TX_SKEW_MASK		GENMASK(7, 0)
#define QCA81XX_MMD3_OPEN_RAMPING_CTRL1		0xa048
#define QCA81XX_MMD3_OPEN_RAMPING_EN_MASK1	0x801
#define QCA81XX_MMD3_OPEN_RAMPING_CTRL2		0xa01f
#define QCA81XX_MMD3_OPEN_RAMPING_EN_MASK2	BIT(7)
#define QCA81XX_MMD3_TRAIN_DURATION_CTRL	0xa017
#define QCA81XX_MMD3_DURATION_MASK		GENMASK(5, 4)
#define QCA81XX_MMD3_DURATION_VAL		0x10
#define QCA81XX_MMD3_DELAY_DATA_MODE		0xa028
#define QCA81XX_MMD3_DELAY_DATA_MODE_MASK	GENMASK(14, 8)
#define QCA81XX_MMD3_DELAY_DATA_MODE_VAL	0x5a00
/* improve master control performance for 10G interoperability */
#define QCA81XX_MMD3_MST_CTRL			0xa04c
#define QCA81XX_MMD3_MST_VAL			BIT(14)

/*PHY MMD7 registers*/
#define QCA81XX_MMD7_COUNTER_CTRL		0x8029
#define QCA81XX_MMD7_INGRESS_COUNTER_HIGH	0x802a
#define QCA81XX_MMD7_INGRESS_COUNTER_LOW	0x802b
#define QCA81XX_MMD7_INGRESS_ERROR_COUNTER	0x802c
#define QCA81XX_MMD7_EGRESS_COUNTER_HIGH	0x802d
#define QCA81XX_MMD7_EGRESS_COUNTER_LOW		0x802e
#define QCA81XX_MMD7_EGRESS_ERROR_COUNTER	0x802f
#define QCA81XX_MMD7_FRAME_CHECK_EN		1
#define QCA81XX_MMD7_CNT_SELFCLR		2

/* LED registers and constants */
#define QCA81XX_LED_NUM				3
#define QCA81XX_MMD7_LED_GLOBAL			0x8073
#define QCA81XX_LED_BLINK_FREQ_MASK		GENMASK(5, 3)
#define QCA81XX_LED_BLINK_FREQ_2HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x0)
#define QCA81XX_LED_BLINK_FREQ_4HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x1)
#define QCA81XX_LED_BLINK_FREQ_8HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x2)
#define QCA81XX_LED_BLINK_FREQ_16HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x3)
#define QCA81XX_LED_BLINK_FREQ_32HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x4)
#define QCA81XX_LED_BLINK_FREQ_64HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x5)
#define QCA81XX_LED_BLINK_FREQ_128HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x6)
#define QCA81XX_LED_BLINK_FREQ_256HZ		FIELD_PREP(QCA81XX_LED_BLINK_FREQ_MASK, 0x7)
#define QCA81XX_LED_BLINK_DUTY_MASK		GENMASK(2, 0)
#define QCA81XX_LED_BLINK_DUTY_50_50		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x0)
#define QCA81XX_LED_BLINK_DUTY_75_25		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x1)
#define QCA81XX_LED_BLINK_DUTY_25_75		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x2)
#define QCA81XX_LED_BLINK_DUTY_33_67		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x3)
#define QCA81XX_LED_BLINK_DUTY_67_33		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x4)
#define QCA81XX_LED_BLINK_DUTY_17_83		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x5)
#define QCA81XX_LED_BLINK_DUTY_83_17		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x6)
#define QCA81XX_LED_BLINK_DUTY_8_92		FIELD_PREP(QCA81XX_LED_BLINK_DUTY_MASK, 0x7)

/* LED hw control pattern is the same for every LED */
#define QCA81XX_MMD7_LED_CTRL(index)		(0x8074 + (index * 2))
#define QCA81XX_LED_PATTERN_MASK		GENMASK(15, 0)
#define QCA81XX_LED_SPEED2500_ON		BIT(15)
/* Follow blink trigger even if duplex or speed condition doesn't match */
#define QCA81XX_LED_BLINK_CHECK_BYPASS		BIT(13)
#define QCA81XX_LED_FULL_DUPLEX_ON		BIT(12)
#define QCA81XX_LED_TX_BLINK			BIT(10)
#define QCA81XX_LED_RX_BLINK			BIT(9)
#define QCA81XX_LED_SPEED1000_ON		BIT(6)
#define QCA81XX_LED_SPEED100_ON			BIT(5)

/* LED hw force control pattern is the same for every LED */
#define QCA81XX_MMD7_LED_FORCE_CTRL(index)		(0x8075 + (index * 2))
#define QCA81XX_LED_FORCE_EN			BIT(15)
#define QCA81XX_LED_FORCE_MODE_MASK		GENMASK(14, 13)
#define QCA81XX_LED_FORCE_BLINK			FIELD_PREP(QCA81XX_LED_FORCE_MODE_MASK, 0x3)
#define QCA81XX_LED_FORCE_ON			FIELD_PREP(QCA81XX_LED_FORCE_MODE_MASK, 0x1)
#define QCA81XX_LED_FORCE_OFF			FIELD_PREP(QCA81XX_LED_FORCE_MODE_MASK, 0x0)

#define QCA81XX_MMD7_LED_POLARITY_CTRL		0x901a
#define QCA81XX_LED_ACTIVE_HIGH			BIT(6)

/*PHY MMD31 registers*/
#define QCA81XX_FIFO_CONTROL			0x19
#define QCA81XX_FIFO_RESET			0x3

#define QCA81XX_1000BASET_CONTROL		0x9
#define QCA81XX_ADVERTISE_1000FULL		0x200
#define QCA81XX_1000BASET_STATUS		0xa
#define QCA81XX_LP_ADVERTISE_1000FULL		0x800

#define QCA81XX_SMART_SPEED			0x14
#define QCA81XX_SMART_SPEED_ENABLE		0x20
#define QCA81XX_SMART_SPEED_RETRY_LIMIT		GENMASK(4, 2)
#define QCA81XX_DEFAULT_DOWNSHIFT		5
#define QCA81XX_MIN_DOWNSHIFT			2
#define QCA81XX_MAX_DOWNSHIFT			9

#define QCA81XX_SPEC_STATUS			0x11
#define QCA81XX_SS_LINK_STATUS			0x400
#define QCA81XX_INTR_DOWNSHIFT			0x20
#define QCA81XX_SS_DUPLEX_FULL			0x2000
#define QCA81XX_SS_SPEED_MASK			0x380
#define QCA81XX_SS_SPEED_10000			0x180
#define QCA81XX_SS_SPEED_5000			0x280
#define QCA81XX_SS_SPEED_2500			0x200
#define QCA81XX_SS_SPEED_1000			0x100
#define QCA81XX_SS_SPEED_100			0x80
#define QCA81XX_SS_MDIX				0x40

#define QCA81XX_INTR_MASK			0x12
#define QCA81XX_INTR_STATUS			0x13
#define QCA81XX_INTR_STATUS_DOWN		0x800
#define QCA81XX_INTR_STATUS_UP			0x400
#define QCA81XX_INTR_ENABLE_WOL			1

#define QCA81XX_SPEC_CONTROL			0x10
#define QCA81XX_AUTO_SOFT_RESET_EN		0x8
#define QCA81XX_MDI_MASK			GENMASK(6, 5)
#define QCA81XX_MDI_AUTO			0x3
#define QCA81XX_MDI_X				0x1
#define QCA81XX_MDI				0

#define QCA81XX_WOL_CTRL			0x8012
#define QCA81XX_WOL_EN				0x0020
#define QCA81XX_MAC_ADDR_0_15			0x804C
#define QCA81XX_MAC_ADDR_16_31			0x804B
#define QCA81XX_MAC_ADDR_32_47			0x804A

/*PCS MMD1 registers*/
#define QCA81XX_PCS_MMD1_MODE_CTRL		0x11b
#define QCA81XX_PCS_MMD1_MODE_MASK		0x1f00
#define QCA81XX_PCS_MMD1_XPCS_MODE		0x1000

#define QCA81XX_PCS_MMD1_CDA_CONTROL1		0x20
#define QCA81XX_PCS_MMD1_SSCG_ENABLE		0x8

#define QCA81XX_PCS_MMD1_CALIBRATION4		0x78
#define QCA81XX_PCS_MMD1_CALIBRATION_DONE	0x80

#define QCA81XX_PCS_MMD1_PLL_POWER_ON_AND_RESET	0x1e0
#define QCA81XX_PCS_MMD1_ANA_SOFT_RESET_MASK	0x40
#define QCA81XX_PCS_MMD1_ANA_SOFT_RESET		0
#define QCA81XX_PCS_MMD1_ANA_SOFT_RELEASE	0x40

/*PCS MMD3 registers*/
#define QCA81XX_PCS_MMD3_AN_LP_BASE_ABL2	0x14
#define QCA81XX_PCS_MMD3_XPCS_EEE_CAP		0x40

#define QCA81XX_PCS_MMD3_PCS_CTRL2		0x7
#define QCA81XX_PCS_MMD3_PCS_TYPE_MASK		0xf
#define QCA81XX_PCS_MMD3_PCS_TYPE_10GBASE_R	0

#define QCA81XX_PCS_MMD3_10GBASE_R_PCS_STATUS1	0x20
#define QCA81XX_PCS_MMD3_10GBASE_R_UP		0x1000

#define QCA81XX_PCS_MMD3_DIG_CTRL1		0x8000
#define QCA81XX_PCS_MMD3_USXGMII_EN		0x200
#define QCA81XX_PCS_MMD3_USXG_FIFO_RESET	0x400
#define QCA81XX_PCS_MMD3_XPCS_SOFT_RESET	0x8000

#define QCA81XX_PCS_MMD3_AN_LP_BASE_ABL2	0x14

#define QCA81XX_PCS_MMD3_EEE_MODE_CTRL		0x8006
#define QCA81XX_PCS_MMD3_EEE_RES_REGS		0x100
#define QCA81XX_PCS_MMD3_EEE_SIGN_BIT_REGS	0x40
#define QCA81XX_PCS_MMD3_EEE_EN			0x3

#define QCA81XX_PCS_MMD3_EEE_TX_TIMER		0x8008
#define QCA81XX_PCS_MMD3_EEE_TSL_REGS		0xa
#define QCA81XX_PCS_MMD3_EEE_TLU_REGS		0xc0
#define QCA81XX_PCS_MMD3_EEE_TWL_REGS		0x1600

#define QCA81XX_PCS_MMD3_EEE_MODE_CTRL1		0x800b
#define QCA81XX_PCS_MMD3_EEE_TRANS_LPI_MODE	0x1
#define QCA81XX_PCS_MMD3_EEE_TRANS_RX_LPI_MODE	0x100

#define QCA81XX_PCS_MMD3_EEE_RX_TIMER		0x8009
#define QCA81XX_PCS_MMD3_EEE_100US_REG_REGS	0xc8
#define QCA81XX_PCS_MMD3_EEE_RWR_REG_REGS	0x1c00

#define QCA81XX_PCS_MMD3_USXG_FIFO_RESET	0x400

/*PCS MMD31 register*/
#define QCA81XX_PCS_MMD31_MII_DIG_CTRL		0x8000
#define QCA81XX_PCS_MMD31_PHY_MODE_CTRL_EN	0x1

#define QCA81XX_PCS_MMD31_MII_AN_INT_MSK	0x8001
#define QCA81XX_PCS_MMD31_AN_COMPLETE_INT	0x1
#define QCA81XX_PCS_MMD31_MII_4BITS_CTRL	0
#define QCA81XX_PCS_MMD31_TX_CONFIG_CTRL	0x8

#define QCA81XX_PCS_MMD31_MII_ERR_SEL		0x8002
#define QCA81XX_PCS_MMD31_MII_XAUI_MODE_CTRL	0x8004
#define QCA81XX_PCS_MMD31_MII_CTRL		0
#define QCA81XX_PCS_MMD31_MII_AN_ENABLE		0x1000

#define QCA81XX_PCS_MMD31_MII_ERR_SEL		0x8002
#define QCA81XX_PCS_MMD31_XPCS_SPEED_MASK	0x2060
#define QCA81XX_PCS_MMD31_XPCS_SPEED_10000	0x2040
#define QCA81XX_PCS_MMD31_XPCS_SPEED_5000	0x2020
#define QCA81XX_PCS_MMD31_XPCS_SPEED_2500	0x20
#define QCA81XX_PCS_MMD31_XPCS_SPEED_1000	0x40
#define QCA81XX_PCS_MMD31_XPCS_SPEED_100	0x2000
#define QCA81XX_PCS_MMD31_AN_RESTART		0x200
#define QCA81XX_PCS_MMD31_MII_AN_COMPLETE_INT	0x1

/*SOC GCC registers*/
#define GCC_E2S_TX_CMD_RCGR			0x800000
#define GCC_E2S_TX_CFG_RCGR			0x800004
#define GCC_E2S_TX_DIV_CDIVR			0x800008
#define GCC_E2S_SRDS_CH0_RX_CBCR		0x800010
#define GCC_E2S_GEPHY_TX_CBCR			0x800014
#define GCC_E2S_RX_CMD_RCGR			0x800018
#define GCC_E2S_RX_CFG_RCGR			0x80001c
#define GCC_E2S_RX_DIV_CDIVR			0x800020
#define GCC_E2S_SRDS_CH0_TX_CBCR		0x800028
#define GCC_E2S_GEPHY_RX_CBCR			0x80002c
#define GCC_AHB_CMD_RCGR			0x80003c
#define GCC_AHB_CFG_RCGR			0x800040
#define AHB_CLK_50M				0
#define AHB_CLK_104M				0x305
#define GCC_SRDS_SYS_CBCR			0x80007c
#define GCC_GEPHY_SYS_CBCR			0x800080
#define GCC_SEC_CTRL_CMD_RCGR			0x800088
#define GCC_SEC_CTRL_CFG_RCGR			0x80008c
#define GCC_SERDES_CTL				0x80030C

#define GCC_E2S_SRC_MASK			GENMASK(10, 8)
#define GCC_E2S_SRC0_REF_50MCLK			0
#define GCC_E2S_SRC1_EPHY_TXCLK			1
#define GCC_E2S_SRC2_EPHY_RXCLK			2
#define GCC_E2S_SRC3_SRDS_TXCLK			3
#define GCC_E2S_SRC4_SRDS_RXCLK			4

#define SRC_DIV_MASK				GENMASK(4, 0)
#define CLK_DIV_MASK				GENMASK(3, 0)
#define CLK_CMD_UPDATE				BIT(0)
#define GCC_CLK_ENABLE				0x1
#define GCC_CLK_ARES				0x4
#define XPCS_PWR_ARES				0x1

/*SOC SEC_TCSR registers*/
#define EPHY_CFG				0x90F018
#define EPHY_LDO_CTRL				BIT(20)
#define GLOBAL_INTR_CTRL			0x90f008
#define PHY_INTR_EN				BIT(7)
#define WOL_INTR_CTRL				0x90f010
#define WOL_INTR_EN				BIT(0)
#define SKU_REG					0x90607C

/* Cable Diagnostic Test (CDT) registers and constants */
/* CDT control register */
#define QCA81XX_CDT				0x16
/* CDT status and control bits */
/* Time delta for length calculation */
#define QCA81XX_CDT_STATUS_DELTA_TIME_MASK	GENMASK(7, 0)
/* Start the cable test */
#define QCA81XX_CDT_ENABLE_TEST			BIT(15)
/* Use meters for length reporting */
#define QCA81XX_CDT_LENGTH_UNIT			BIT(10)
#define QCA81XX_MMD3_CDT_STATUS			0x8064
#define QCA81XX_MMD3_CDT_DIAG_PAIR_A		0x8065
#define QCA81XX_MMD3_CDT_DIAG_PAIR_B		0x8066
#define QCA81XX_MMD3_CDT_DIAG_PAIR_C		0x8067
#define QCA81XX_MMD3_CDT_DIAG_PAIR_D		0x8068
#define QCA81XX_CDT_CODE_MASK			GENMASK(3, 0)
#define QCA81XX_CDT_STATUS_STAT_TYPE		GENMASK(1, 0)
#define QCA81XX_CDT_STATUS_STAT_FAIL		FIELD_PREP_CONST(QCA81XX_CDT_STATUS_STAT_TYPE, 0)
#define QCA81XX_CDT_STATUS_STAT_NORMAL		FIELD_PREP_CONST(QCA81XX_CDT_STATUS_STAT_TYPE, 1)
#define QCA81XX_CDT_STATUS_STAT_SAME_OPEN	FIELD_PREP_CONST(QCA81XX_CDT_STATUS_STAT_TYPE, 2)
#define QCA81XX_CDT_STATUS_STAT_SAME_SHORT	FIELD_PREP_CONST(QCA81XX_CDT_STATUS_STAT_TYPE, 3)

static bool qca81xx_phy_reg_valid(struct phy_device *phydev,
	unsigned int reg)
{
	if (reg > 0xFFFF) {
		phydev_err(phydev, "Invalid debug register address: 0x%x\n",
			reg);
		return false;
	}
	return true;
}
/**
 * __qca81xx_phy_debug_write - Write to PHY debug register
 * @phydev: The PHY device structure
 * @reg: The debug register to write to (must be 0-0xFFFF)
 * @val: The value to write
 *
 * Write to the specified debug register using indirect access.
 * The register address must be within the valid range (0-0xFFFF).
 *
 * Return: 0 on success, negative error code on failure
 */
int __qca81xx_phy_debug_write(struct phy_device *phydev,
	unsigned int reg, u16 val)
{
	int ret;

	if (!qca81xx_phy_reg_valid(phydev, reg))
		return -EINVAL;

	ret = __phy_write_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_DEBUG_ADDR, reg);
	if (ret < 0)
		return ret;
	ret = __phy_write_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_DEBUG_DATA, val);

	return ret;
}

/**
 * qca81xx_phy_debug_write - Write to PHY debug register with bus locking
 * @phydev: The PHY device structure
 * @reg: The debug register to write to
 * @val: The value to write
 *
 * Write to the specified debug register using indirect access with proper
 * MDIO bus locking.
 *
 * Return: 0 on success, negative error code on failure
 */
int qca81xx_phy_debug_write(struct phy_device *phydev,
	unsigned int reg, u16 val)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = __qca81xx_phy_debug_write(phydev, reg, val);
	phy_unlock_mdio_bus(phydev);

	return ret;
}

/**
 * __qca81xx_phy_debug_read - Read from PHY debug register
 * @phydev: The PHY device structure
 * @reg: The debug register to read(must be 0-0xFFFF)
 *
 * Read from the specified debug register using indirect access.
 * The register address must be within the valid range (0-0xFFFF).
 *
 * Return: The register value on success, negative error code on failure
 */
int __qca81xx_phy_debug_read(struct phy_device *phydev,
	unsigned int reg)
{
	int ret;

	if (!qca81xx_phy_reg_valid(phydev, reg))
		return -EINVAL;

	ret = __phy_write_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_DEBUG_ADDR, reg);
	if (ret < 0)
		return ret;
	ret = __phy_read_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_DEBUG_DATA);

	return ret;
}

/**
 * qca81xx_phy_debug_read - Read from PHY debug register with bus locking
 * @phydev: The PHY device structure
 * @reg: The debug register to read
 *
 * Read from the specified debug register using indirect access with proper
 * MDIO bus locking.
 *
 * Return: The register value on success, negative error code on failure
 */
int qca81xx_phy_debug_read(struct phy_device *phydev,
	unsigned int reg)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = __qca81xx_phy_debug_read(phydev, reg);
	phy_unlock_mdio_bus(phydev);

	return ret;
}

int qca81xx_phy_debug_modify(struct phy_device *phydev,
			     unsigned int reg, u16 clear, u16 set)
{
	int ret;

	phy_lock_mdio_bus(phydev);
	ret = __phy_write_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_DEBUG_ADDR, reg);
	if (ret) {
		phy_unlock_mdio_bus(phydev);
		return ret;
	}

	ret = __phy_modify_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_DEBUG_DATA, clear, set);
	phy_unlock_mdio_bus(phydev);

	return ret;
}
EXPORT_SYMBOL_GPL(qca81xx_phy_debug_modify);

static int qca81xx_pcs_address(struct phy_device *phydev)
{
	return phydev->mdio.addr + PCS_ADDR_OFFSET;
}

static int qca81xx_pcs_read_mmd(struct phy_device *phydev,
	int devad, u32 regnum)
{
	int addr = qca81xx_pcs_address(phydev);

	return mdiobus_c45_read(phydev->mdio.bus, addr, devad, regnum);
}

static int qca81xx_pcs_modify_mmd(struct phy_device *phydev,
	int devad, u32 regnum, u16 mask, u16 set)
{
	int addr = qca81xx_pcs_address(phydev);

	return mdiobus_c45_modify(phydev->mdio.bus, addr, devad, regnum,
		mask, set);
}

static u32 qca81xx_soc_address(struct phy_device *phydev)
{
	return phydev->mdio.addr + SOC_ADDR_OFFSET;
}

static inline void qca81xx_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid,
	u16 *reg_high)
{
	*reg_low = (regaddr & 0xc) << 1;

	*reg_mid = regaddr >> 4 & 0xffff;

	*reg_high = ((regaddr >> 20 & 0xf) << 1) | BIT(0);
}

static u32 qca81xx_mii_read(struct mii_bus *bus, u32 reg)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;

	addr = FIELD_GET(GENMASK(28, 24), reg);
	qca81xx_split_addr(reg, &reg_low, &reg_mid, &reg_high);
	/*write ahb address bit4~bit23*/
	__mdiobus_write(bus, addr, reg_high & 0x1f, reg_mid);
	udelay(100);
	/*write ahb address bit0~bit3 and read low 16bit data*/
	lo = __mdiobus_read(bus, addr, reg_low);
	/*write ahb address bit0~bit3 and read high 16 bit data*/
	hi = __mdiobus_read(bus, addr, (reg_low + 4));

	return (hi << 16) | lo;
}

static void qca81xx_mii_write(struct mii_bus *bus, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	u32 addr;

	addr = FIELD_GET(GENMASK(28, 24), reg);

	qca81xx_split_addr(reg, &reg_low, &reg_mid, &reg_high);
	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	/*write ahb address bit4~bit23*/
	__mdiobus_write(bus, addr, reg_high & 0x1f, reg_mid);
	udelay(100);
	/*write ahb address bit0~bit3 and write low 16 bit data*/
	__mdiobus_write(bus, addr, reg_low, lo);
	/*write ahb address bit0~bit3 and write high 16 bit data*/
	__mdiobus_write(bus, addr, (reg_low + 4), hi);
}

u32 __qca81xx_soc_read(struct phy_device *phydev, u32 reg)
{
	u32 reg_e, val = 0xffffffff;
	int addr;
	struct qca81xx_phy_mdio_data *mdio_priv = phydev->mdio.bus->priv;

	addr = qca81xx_soc_address(phydev);
	reg_e = TO_QCA81XX_PHY_SOC_ADDR(addr, reg);

	if (strstr(phydev->mdio.bus->id, "i2c")) {
		if (mdio_priv && mdio_priv->sw_read)
			val = mdio_priv->sw_read(phydev->mdio.bus, reg_e);
	} else {
		val = qca81xx_mii_read(phydev->mdio.bus, reg_e);
	}

	return val;
}

u32 qca81xx_soc_read(struct phy_device *phydev, u32 reg)
{
	u32 val;

	phy_lock_mdio_bus(phydev);
	val = __qca81xx_soc_read(phydev, reg);
	phy_unlock_mdio_bus(phydev);

	return val;
}

int __qca81xx_soc_write(struct phy_device *phydev,
	u32 reg, u32 val)
{
	u32 reg_e;
	int addr;
	struct qca81xx_phy_mdio_data *mdio_priv = phydev->mdio.bus->priv;

	addr = qca81xx_soc_address(phydev);
	reg_e = TO_QCA81XX_PHY_SOC_ADDR(addr, reg);

	if (strstr(phydev->mdio.bus->id, "i2c")) {
		if (mdio_priv && mdio_priv->sw_write)
			mdio_priv->sw_write(phydev->mdio.bus, reg_e, val);
		else
			return -EINVAL;
	} else {
		qca81xx_mii_write(phydev->mdio.bus, reg_e, val);
	}

	return 0;
}

int __qca81xx_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set)
{
	u32 val;

	val = __qca81xx_soc_read(phydev, reg);
	val = (val & ~mask) | set;
	__qca81xx_soc_write(phydev, reg, val);

	return 0;
}

int qca81xx_soc_modify(struct phy_device *phydev, u32 reg,
	u32 mask, u32 set)
{
	phy_lock_mdio_bus(phydev);
	__qca81xx_soc_modify(phydev, reg, mask, set);
	phy_unlock_mdio_bus(phydev);

	return 0;
}
EXPORT_SYMBOL_GPL(qca81xx_soc_modify);

static int qca81xx_pcs_txclk_en_set(struct phy_device *phydev,
	bool enable)
{
	return qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_TX_CBCR,
		GCC_CLK_ENABLE, enable ? GCC_CLK_ENABLE : 0);
}

static int qca81xx_pcs_rxclk_en_set(struct phy_device *phydev,
	bool enable)
{
	return qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_RX_CBCR,
		GCC_CLK_ENABLE, enable ? GCC_CLK_ENABLE : 0);
}

static int qca81xx_pcs_clk_en_set(struct phy_device *phydev,
	bool enable)
{
	int ret;

	ret = qca81xx_pcs_txclk_en_set(phydev, enable);
	if (ret < 0)
		return ret;

	return qca81xx_pcs_rxclk_en_set(phydev, enable);
}

static int qca81xx_pcs_clk_reset_update(struct phy_device *phydev,
	bool assert)
{
	int ret;

	ret = qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_RX_CBCR,
		GCC_CLK_ARES, assert ? GCC_CLK_ARES : 0);
	if (ret < 0)
		return ret;

	return qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_TX_CBCR,
		GCC_CLK_ARES, assert ? GCC_CLK_ARES : 0);
}

static int qca81xx_pcs_clk_reset(struct phy_device *phydev)
{
	int ret;

	ret = qca81xx_pcs_clk_reset_update(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);

	return qca81xx_pcs_clk_reset_update(phydev, false);
}

static int qca81xx_pcs_sysclk_en_set(struct phy_device *phydev,
	bool enable)
{
	return qca81xx_soc_modify(phydev, GCC_SRDS_SYS_CBCR, GCC_CLK_ENABLE,
		enable ? GCC_CLK_ENABLE : 0);
}

static int qca81xx_pcs_sysclk_reset_update(struct phy_device *phydev,
	bool assert)
{
	return qca81xx_soc_modify(phydev, GCC_SRDS_SYS_CBCR, GCC_CLK_ARES,
		assert ? GCC_CLK_ARES : 0);
}

static int qca81xx_pcs_sysclk_reset(struct phy_device *phydev)
{
	int ret;

	ret = qca81xx_pcs_sysclk_reset_update(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);

	return qca81xx_pcs_sysclk_reset_update(phydev, false);
}

static int qca81xx_xpcs_clk_reset_update(struct phy_device *phydev,
	bool assert)
{
	return qca81xx_soc_modify(phydev, GCC_SERDES_CTL, XPCS_PWR_ARES,
		assert ? XPCS_PWR_ARES : 0);
}

static int qca81xx_phy_clk_en_set(struct phy_device *phydev, bool enable)
{
	int ret;

	ret = qca81xx_soc_modify(phydev, GCC_E2S_GEPHY_TX_CBCR,
		GCC_CLK_ENABLE, enable ? GCC_CLK_ENABLE : 0);
	if (ret < 0)
		return ret;

	return qca81xx_soc_modify(phydev, GCC_E2S_GEPHY_RX_CBCR,
		GCC_CLK_ENABLE, enable ? GCC_CLK_ENABLE : 0);
}

static int qca81xx_phy_txclk_reset_update(struct phy_device *phydev,
	bool assert)
{
	return qca81xx_soc_modify(phydev, GCC_E2S_GEPHY_TX_CBCR, GCC_CLK_ARES,
		assert ? GCC_CLK_ARES : 0);
}

static int qca81xx_phy_rxclk_reset_update(struct phy_device *phydev,
	bool assert)
{
	return qca81xx_soc_modify(phydev, GCC_E2S_GEPHY_RX_CBCR, GCC_CLK_ARES,
		assert ? GCC_CLK_ARES : 0);
}

static int qca81xx_phy_clk_reset_update(struct phy_device *phydev,
	bool assert)
{
	int ret;

	ret = qca81xx_phy_txclk_reset_update(phydev, assert);
	if (ret < 0)
		return ret;

	return qca81xx_phy_rxclk_reset_update(phydev, assert);
}

static int qca81xx_phy_clk_reset(struct phy_device *phydev)
{
	int ret;

	ret = qca81xx_phy_clk_reset_update(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);

	return qca81xx_phy_clk_reset_update(phydev, false);
}

static int qca81xx_phy_speed_clk_set(struct phy_device *phydev)
{
	int ret, div0 = 0, div1 = 0;

	switch (phydev->speed) {
	case SPEED_100:
		/* 312.5 divided by 2.5*5 */
		div0 = 4;
		div1 = 4;
		break;
	case SPEED_1000:
		/* 312.5 divided by 2.5*1 */
		div0 = 4;
		div1 = 0;
		break;
	case SPEED_2500:
		/* 312.5 divided by 1*4 */
		div0 = 1;
		div1 = 3;
		break;
	case SPEED_5000:
		/* 312.5 divided by 1*2 */
		div0 = 1;
		div1 = 1;
		break;
	case SPEED_10000:
		/* 312.5 divided by 1*1 */
		div0 = 1;
		div1 = 0;
		break;
	default:
		break;
	}

	ret = qca81xx_soc_modify(phydev, GCC_E2S_TX_CFG_RCGR,
		SRC_DIV_MASK, div0);
	if (ret < 0)
		return ret;
	ret = qca81xx_soc_modify(phydev, GCC_E2S_TX_DIV_CDIVR,
		CLK_DIV_MASK, div1);
	if (ret < 0)
		return ret;
	ret = qca81xx_soc_modify(phydev, GCC_E2S_TX_CMD_RCGR,
		CLK_CMD_UPDATE, CLK_CMD_UPDATE);
	if (ret < 0)
		return ret;

	ret = qca81xx_soc_modify(phydev, GCC_E2S_RX_CFG_RCGR,
		SRC_DIV_MASK, div0);
	if (ret < 0)
		return ret;
	ret = qca81xx_soc_modify(phydev, GCC_E2S_RX_DIV_CDIVR,
		CLK_DIV_MASK, div1);
	if (ret < 0)
		return ret;

	return qca81xx_soc_modify(phydev, GCC_E2S_RX_CMD_RCGR,
		CLK_CMD_UPDATE, CLK_CMD_UPDATE);
}

static int qca81xx_pcs_eee_enable(struct phy_device *phydev)
{
	int ret = 0;

	/*Configure the EEE related timer*/
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_EEE_MODE_CTRL,
		0xf40,
		QCA81XX_PCS_MMD3_EEE_RES_REGS |
		QCA81XX_PCS_MMD3_EEE_SIGN_BIT_REGS);
	if (ret < 0)
		return ret;

	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_EEE_TX_TIMER,
		0x1fff,
		QCA81XX_PCS_MMD3_EEE_TSL_REGS|
		QCA81XX_PCS_MMD3_EEE_TLU_REGS |
		QCA81XX_PCS_MMD3_EEE_TWL_REGS);
	if (ret < 0)
		return ret;

	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_EEE_RX_TIMER,
		0x1fff,
		QCA81XX_PCS_MMD3_EEE_100US_REG_REGS|
		QCA81XX_PCS_MMD3_EEE_RWR_REG_REGS);
	if (ret < 0)
		return ret;

	/*enable TRN_LPI*/
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_EEE_MODE_CTRL1,
		0x101,
		QCA81XX_PCS_MMD3_EEE_TRANS_LPI_MODE |
		QCA81XX_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);
	if (ret < 0)
		return ret;

	/*enable TX/RX LPI pattern*/
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_PCS_MMD3_EEE_MODE_CTRL, 0x3,
		QCA81XX_PCS_MMD3_EEE_EN);

	return ret;
}

static int qca81xx_phy_soft_reset(struct phy_device *phydev)
{
	int ret;

	qca81xx_priv_atomic64_inc(phydev,
		&((struct qca81xx_private *)phydev->priv)->debug_stats.soft_reset_count);

	/* enable auto soft reset when power on */
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_SPEC_CONTROL,
		QCA81XX_AUTO_SOFT_RESET_EN,
		QCA81XX_AUTO_SOFT_RESET_EN);
	if (ret < 0)
		return ret;

	genphy_c45_pma_suspend(phydev);
	mdelay(10);
	genphy_c45_pma_resume(phydev);
	mdelay(1);

	/* disable auto soft reset when power on */
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_SPEC_CONTROL,
		QCA81XX_AUTO_SOFT_RESET_EN, 0);

	return 0;
}

static int qca81xx_phy_pcs_assert(struct phy_device *phydev,
	bool assert)
{
	int ret;
	struct qca81xx_private *priv = phydev->priv;

	if (!priv)
		return -EINVAL;

	/* Skip if already in the desired state */
	if (assert == priv->pcs_assert)
		return 0;

	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_PCS_MMD1_PLL_POWER_ON_AND_RESET,
		QCA81XX_PCS_MMD1_ANA_SOFT_RESET_MASK,
		assert ? QCA81XX_PCS_MMD1_ANA_SOFT_RESET :
		QCA81XX_PCS_MMD1_ANA_SOFT_RELEASE);
	if (ret < 0)
		return ret;
	priv->pcs_assert = assert;

	return 0;
}

static int qca81xx_pcs_usxgmii_init(struct phy_device *phydev)
{
	int ret = 0;
	u16 phy_data = 0;

	ret = qca81xx_phy_clk_en_set(phydev, false);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_clk_en_set(phydev, false);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_sysclk_en_set(phydev, true);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_sysclk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_xpcs_clk_reset_update(phydev, true);
	if (ret < 0)
		return ret;
	/* optional, would settle after SOD VI, write 1 to */
	/* CSR0 MMD1_reg0x7c[3],to invert pcs txclk */
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PMAPMD, QCA81XX_PCS_MMD1_MODE_CTRL,
		QCA81XX_PCS_MMD1_MODE_MASK,
		QCA81XX_PCS_MMD1_XPCS_MODE);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_clk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_clk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_pcs_assert(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qca81xx_phy_pcs_assert(phydev, false);
	if (ret < 0)
		return ret;
	ret = read_poll_timeout(qca81xx_pcs_read_mmd,
		phy_data, (phy_data & QCA81XX_PCS_MMD1_CALIBRATION_DONE),
		1000, 100000, true, phydev, MDIO_MMD_PMAPMD,
		QCA81XX_PCS_MMD1_CALIBRATION4);
	if (ret < 0)
		phydev_warn(phydev, "PCS calibration time out!\n");
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PMAPMD, QCA81XX_PCS_MMD1_CDA_CONTROL1,
		QCA81XX_PCS_MMD1_SSCG_ENABLE,
		QCA81XX_PCS_MMD1_SSCG_ENABLE);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_txclk_en_set(phydev, true);
	if (ret < 0)
		return ret;
	ret = qca81xx_xpcs_clk_reset_update(phydev, false);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_PCS_CTRL2,
		QCA81XX_PCS_MMD3_PCS_TYPE_MASK,
		QCA81XX_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret < 0)
		return ret;
	ret = read_poll_timeout(qca81xx_pcs_read_mmd,
		phy_data, ((phy_data & QCA81XX_PCS_MMD3_10GBASE_R_UP)),
		10000, 500000, true, phydev, MDIO_MMD_PCS,
		QCA81XX_PCS_MMD3_10GBASE_R_PCS_STATUS1);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_PCS_MMD3_DIG_CTRL1,
		QCA81XX_PCS_MMD3_USXGMII_EN,
		QCA81XX_PCS_MMD3_USXGMII_EN);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_DIG_CTRL1,
		QCA81XX_PCS_MMD3_XPCS_SOFT_RESET,
		QCA81XX_PCS_MMD3_XPCS_SOFT_RESET);
	if (ret < 0)
		return ret;
	ret = read_poll_timeout(qca81xx_pcs_read_mmd, phy_data,
		(!(phy_data & QCA81XX_PCS_MMD3_XPCS_SOFT_RESET)),
		1000, 100000, true, phydev, MDIO_MMD_PCS,
		QCA81XX_PCS_MMD3_DIG_CTRL1);
	if (ret < 0) {
		phydev_err(phydev, "xpcs software reset timeout\n");
		return ret;
	}
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_VEND2, QCA81XX_PCS_MMD31_MII_AN_INT_MSK,
		0x109,
		QCA81XX_PCS_MMD31_AN_COMPLETE_INT |
		QCA81XX_PCS_MMD31_MII_4BITS_CTRL |
		QCA81XX_PCS_MMD31_TX_CONFIG_CTRL);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_VEND2, QCA81XX_PCS_MMD31_MII_DIG_CTRL, BIT(0),
		QCA81XX_PCS_MMD31_PHY_MODE_CTRL_EN);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_VEND2, QCA81XX_PCS_MMD31_MII_CTRL,
		QCA81XX_PCS_MMD31_MII_AN_ENABLE,
		QCA81XX_PCS_MMD31_MII_AN_ENABLE);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_eee_enable(phydev);

	return ret;
}

/* adjust the AFE DAC parameter and the CDT threshold for cable */
/* status precision such as open, short, normal */
static int qca81xx_phy_cdt_thresh_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL2,
		QCA81XX_MMD3_CDT_THRESH_CTRL2_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL3,
		QCA81XX_MMD3_CDT_THRESH_CTRL3_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL4,
		QCA81XX_MMD3_CDT_THRESH_CTRL4_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL5,
		QCA81XX_MMD3_CDT_THRESH_CTRL5_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL6,
		QCA81XX_MMD3_CDT_THRESH_CTRL6_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL7,
		QCA81XX_MMD3_CDT_THRESH_CTRL7_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL9,
		QCA81XX_MMD3_CDT_THRESH_CTRL9_VAL);
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL13,
		QCA81XX_MMD3_CDT_THRESH_CTRL13_VAL);
	if (ret < 0)
		return ret;
	ret = phy_write_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_CDT_THRESH_CTRL14,
		QCA81XX_MMD3_CDT_THRESH_CTRL14_VAL);

	return ret;
}

static int qca81xx_soc_ahb_clk_set(struct phy_device *phydev, u32 clk)
{
	int ret = 0;

	ret = qca81xx_soc_read(phydev, GCC_AHB_CFG_RCGR);
	if (ret < 0)
		return ret;

	/* if the current colck is expected, then nothing to do */
	if ((ret & (GCC_E2S_SRC_MASK | SRC_DIV_MASK)) == clk)
		return 0;

	ret = qca81xx_soc_modify(phydev, GCC_AHB_CFG_RCGR,
		GCC_E2S_SRC_MASK | SRC_DIV_MASK, clk);
	if (ret < 0)
		return ret;
	ret = qca81xx_soc_modify(phydev, GCC_AHB_CMD_RCGR,
		CLK_CMD_UPDATE, CLK_CMD_UPDATE);

	return ret;
}

static int qca81xx_phy_gcc_pre_init(struct phy_device *phydev)
{
	int ret;

	/* switch the ahb clock as 50M */
	ret = qca81xx_soc_ahb_clk_set(phydev, AHB_CLK_50M);
	if (ret < 0)
		return ret;
	/* enable efuse loading into analog circuit */
	ret = qca81xx_soc_modify(phydev, EPHY_CFG, EPHY_LDO_CTRL, 0);
	mdelay(10);

	return ret;
}

static int qca81xx_phy_gcc_post_init(struct phy_device *phydev)
{
	int ret;

	/* switch the ahb clock as 104M */
	ret = qca81xx_soc_ahb_clk_set(phydev, AHB_CLK_104M);
	if (ret < 0)
		return ret;
	/* security control clock switch as 25M */
	ret = qca81xx_soc_modify(phydev, GCC_SEC_CTRL_CFG_RCGR,
		GCC_E2S_SRC_MASK | SRC_DIV_MASK, 0x3);
	if (ret < 0)
		return ret;
	ret = qca81xx_soc_modify(phydev, GCC_SEC_CTRL_CMD_RCGR,
		CLK_CMD_UPDATE, CLK_CMD_UPDATE);
	if (ret < 0)
		return ret;

	/*select uphy rx, ephy tx clock source as srds_rxclk*/
	ret = qca81xx_soc_modify(phydev, GCC_E2S_TX_CFG_RCGR,
		GCC_E2S_SRC_MASK, GCC_E2S_SRC4_SRDS_RXCLK << 8);
	if (ret < 0)
		return ret;

	/*select uphy tx, ephy rx clock source as srds_txclk*/
	ret = qca81xx_soc_modify(phydev, GCC_E2S_RX_CFG_RCGR,
		GCC_E2S_SRC_MASK, GCC_E2S_SRC3_SRDS_TXCLK << 8);

	return ret;
}

/* improve the performance of link and traffic especially for 10G speed with long cable */
static int qca81xx_phy_ana_config_init(struct phy_device *phydev)
{
	/* smooth the noise */
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_NOISE_SMOOTH_CTRL_H,
		QCA81XX_MMD3_NOISE_AVERAGE_CNT_SEL0, QCA81XX_MMD3_NOISE_AVERAGE_CNT_SEL0);
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_NOISE_SMOOTH_CTRL_L,
		QCA81XX_MMD3_NOISE_AVERAGE_CNT_SEL1, QCA81XX_MMD3_NOISE_AVERAGE_CNT_SEL1);
	/* reduce noise */
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_REDUCE_NOISE_CTRL,
		QCA81XX_MMD3_REDUCE_NOISE_EN, QCA81XX_MMD3_REDUCE_NOISE_EN);
	/* reduce rotate clock */
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_ROTCLK_CTRL,
		QCA81XX_MMD3_ROTCLK_SEL, QCA81XX_MMD3_ROTCLK_SEL);
	/* enable acceleration tracing */
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_CDR_TRACING_CTRL,
		QCA81XX_MMD3_CDR_TRACING_ACCEL, 0);
	/* slow FFE(Feed-Forward Equalizer) coefficient */
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_FFE_COEF0_CTRL,
		QCA81XX_MMD3_FFE_COEF0_VAL, QCA81XX_MMD3_FFE_COEF0_VAL);
	phy_modify_mmd_changed(phydev, MDIO_MMD_PMAPMD, QCA81XX_MMD1_FFE_COEF1_CTRL,
		QCA81XX_MMD1_FFE_COEF1_VAL, QCA81XX_MMD1_FFE_COEF1_VAL);
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_FFE_COEF2_CTRL,
		QCA81XX_MMD3_FFE_COEF2_VAL | QCA81XX_MMD3_FFE_A2D_FIFO_DELAY_MASK,
		QCA81XX_MMD3_FFE_COEF2_VAL | QCA81XX_MMD3_FFE_A2D_FIFO_DELAY_SEL);
	/* optimize analog PLL jitter */
	qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_PLL_JITTER0_CTRL,
		QCA81XX_DEBUG_ANA_PLL_JITTER0_VAL);
	qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_PLL_JITTER1_CTRL,
		QCA81XX_DEBUG_ANA_PLL_JITTER1_VAL);
	qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_PLL_JITTER2_CTRL,
		QCA81XX_DEBUG_ANA_PLL_JITTER2_VAL);
	/* update analog edac resistor value */
	qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_RESISTOR0_CTRL,
		QCA81XX_DEBUG_ANA_RESISTOR0_VAL);
	qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_RESISTOR1_CTRL,
		QCA81XX_DEBUG_ANA_RESISTOR1_VAL);
	/* improve mst performance */
	phy_modify_mmd_changed(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_MST_CTRL,
		QCA81XX_MMD3_MST_VAL, QCA81XX_MMD3_MST_VAL);

	return 0;
}

static int qca81xx_sec_ctrl_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = qca81xx_soc_modify(phydev, GLOBAL_INTR_CTRL,
		PHY_INTR_EN, PHY_INTR_EN);
	if (ret < 0)
		return ret;
	ret = qca81xx_soc_modify(phydev, WOL_INTR_CTRL,
		WOL_INTR_EN, WOL_INTR_EN);

	return ret;
}

static int qca81xx_tlmm_init(struct phy_device *phydev)
{
	int ret = 0, pin_id = 0;

	/* the GPIO function bit2~5 is set 1 means the expected function */
	/* such as GPIO0 is WOL INT function and GPIO2 is LED0 function */
	for (pin_id  = GPIO0_WOL_INT; pin_id <= GPIO4_LED3; pin_id++) {
		ret = qca81xx_soc_modify(phydev, TO_TLMM_CFG_REG(pin_id),
			TLMM_FUNC_MASK, BIT(2));
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int qca81xx_phy_eee_config_init(struct phy_device *phydev)
{
	int ret;

	/* disable AFE control for 1G EEE to keep FULLECHO, ECHO, ADC, VGA */
	/* and DAC always on */
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_AZ_1G_AFE_CTRL,
		QCA81XX_MMD3_AZ_1G_AFE_CTRL_MASK, 0);
	if (ret < 0)
		return ret;
	/* some link partner would enter to data mode later than laguna when */
	/* EEE is enabled, so laguna need to match it */
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_DELAY_DATA_MODE,
		QCA81XX_MMD3_DELAY_DATA_MODE_MASK, QCA81XX_MMD3_DELAY_DATA_MODE_VAL);

	return ret;
}

static int qca81xx_phy_ana_capacitance_update(struct phy_device *phydev)
{
	int ret = 0;

	/* update rx sensitivity value */
	ret = qca81xx_phy_debug_modify(phydev, QCA81XX_DEBUG_ANA_RX_SENS_CTRL,
		QCA81XX_DEBUG_ANA_RX_SENS_MASK,
		QCA81XX_DEBUG_ANA_RX_SENS_VAL);
	if (ret < 0)
		return ret;
	/* update edac resistor value */
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC8_DP,
		QCA81XX_DEBUG_ANA_EDAC_RES_VAL0);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC9_DP,
		QCA81XX_DEBUG_ANA_EDAC_RES_VAL1);
	if (ret < 0)
		return ret;
	/* update edac capacitance value */
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_EDAC_CAP0_CTRL,
		QCA81XX_DEBUG_ANA_EDAC_CAP0_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_EDAC_CAP1_CTRL,
		QCA81XX_DEBUG_ANA_EDAC_CAP1_VAL);
	if (ret < 0)
		return ret;
	/* enable open ramping filter */
	ret = qca81xx_phy_debug_modify(phydev, QCA81XX_DEBUG_ANA_OPEN_RAMPING_CRTL0,
		QCA81XX_DEBUG_ANA_OPEN_RAMPING_EN, QCA81XX_DEBUG_ANA_OPEN_RAMPING_EN);
	if (ret < 0)
		return ret;
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_OPEN_RAMPING_CTRL1,
		QCA81XX_MMD3_OPEN_RAMPING_EN_MASK1, 0x1);
	if (ret < 0)
		return ret;
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_OPEN_RAMPING_CTRL2, QCA81XX_MMD3_OPEN_RAMPING_EN_MASK2, 0);
	if (ret < 0)
		return ret;
	/* update SNR threshold */
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_MMD1_SNR_THRESHOLD_CTRL, QCA81XX_MMD1_SNR_THRESHOLD_MASK,
		QCA81XX_MMD1_SNR_THRESHOLD_VAL);
	if (ret < 0)
		return ret;
	/* update VGA gain to improve 10G long cable high temperature performance */
	ret = phy_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_MMD1_10G_VGA_GAIN_CTRL, QCA81XX_MMD1_10G_VGA_GAIN_VAL);
	if (ret < 0)
		return ret;
	/* adjust the cable tx skew to improve 10G link performance */
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_CABLE_SKEW,
		QCA81XX_MMD3_CABLE_TX_SKEW_MASK, 0x30);
	if (ret < 0)
		return ret;
	/* adjust the training duration */
	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_TRAIN_DURATION_CTRL,
		QCA81XX_MMD3_DURATION_MASK, QCA81XX_MMD3_DURATION_VAL);

	return ret;
}

int qca81xx_phy_suspend(struct phy_device *phydev)
{
	qca81xx_priv_atomic64_inc(phydev,
		&((struct qca81xx_private *)phydev->priv)->debug_stats.suspend_count);

	return genphy_c45_pma_suspend(phydev);
}

int qca81xx_phy_resume(struct phy_device *phydev)
{
	qca81xx_priv_atomic64_inc(phydev,
		&((struct qca81xx_private *)phydev->priv)->debug_stats.resume_count);

	return genphy_c45_pma_resume(phydev);
}

static int qca81xx_phy_config_init(struct phy_device *phydev)
{
	int ret = 0;
	enum qca81xx_init_state state = QCA81XX_INIT_START;
	struct qca81xx_private *priv = phydev->priv;

	qca81xx_set_init_state(phydev, state);

	/*
	 * De-assert PCS only if pcs was asserted to make sure the SoC registers access
	 * and usxgmii init successfully
	 */
	mutex_lock(&phydev->lock);
	if (priv && priv->pcs_assert) {
		ret = qca81xx_phy_pcs_assert(phydev, false);
		if (ret < 0)
			goto err_out;
		mdelay(10);
	}

	ret = qca81xx_phy_gcc_pre_init(phydev);
	if (ret < 0) {
		state = QCA81XX_INIT_GCC_PRE_INIT_FAILURE;
		goto err_out;
	}
	ret = qca81xx_phy_ana_config_init(phydev);
	if (ret < 0) {
		state = QCA81XX_INIT_ANA_CONFIG_FAILURE;
		goto err_out;
	}

	ret = qca81xx_pcs_usxgmii_init(phydev);
	if (ret < 0) {
		state = QCA81XX_INIT_PCS_USXGMII_FAILURE;
		goto err_out;
	}
	phydev->interface = PHY_INTERFACE_MODE_USXGMII;

	ret = qca81xx_phy_gcc_post_init(phydev);
	if (ret < 0) {
		state = QCA81XX_INIT_GCC_POST_INIT_FAILURE;
		goto err_out;
	}
	qca81xx_phy_eee_config_init(phydev);
	/* update VGA gain to improve 10G long cable high */
	/* temperature performance */
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_MMD1_10G_VGA_GAIN_CTRL,
		QCA81XX_MMD1_10G_VGA_GAIN_VAL);
	/* update 2.5G VGA(Variable-Gain Amplifier)bandwidth */
	/* to improve channel anti-interference ability */
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_MMD1_2P5G_VGA_BW_CTRL,
		QCA81XX_MMD1_2P5G_VGA_BW_VAL);
	ret = qca81xx_sec_ctrl_init(phydev);
	if (ret < 0) {
		state = QCA81XX_INIT_SEC_CTRL_FAILURE;
		goto err_out;
	}
	ret = qca81xx_tlmm_init(phydev);
	if (ret < 0) {
		state = QCA81XX_INIT_TLMM_FAILURE;
		goto err_out;
	}
	qca81xx_phy_cdt_thresh_init(phydev);
#if IS_ENABLED(CONFIG_HWMON)
	qca81xx_hwmon_hw_init(phydev);
#endif
	mutex_unlock(&phydev->lock);
	/*enable phy counter check*/
	qca81xx_phy_stats_enable(phydev);

	qca81xx_phy_ana_capacitance_update(phydev);

	/* enable re-train for 1G speed to fix CRC issue with some link partner */
	ret = qca81xx_phy_debug_modify(phydev, QCA81XX_DEBUG_SYSTEM_MODE_CTRL,
		QCA81XX_DEBUG_RETRAIN_EN_BIT, QCA81XX_DEBUG_RETRAIN_EN_BIT);
	if (ret < 0)
		goto err_out_no_lock;

	mutex_lock(&phydev->lock);
	ret = qca81xx_phy_resume(phydev);
	if (ret < 0)
		goto err_out;
	mutex_unlock(&phydev->lock);
	state = QCA81XX_INIT_SUCCESS;
	goto err_out_no_lock;

err_out:
	mutex_unlock(&phydev->lock);
err_out_no_lock:
	qca81xx_set_init_state(phydev, state);
	return ret;
}

static int qca81xx_phy_get_features(struct phy_device *phydev)
{
	int ret;

	ret = genphy_c45_pma_read_abilities(phydev);
	if (ret < 0)
		return ret;

	linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		phydev->advertising);
	linkmode_clear_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT,
		phydev->supported);

	return 0;
}

int qca81xx_phy_master_slave_set(struct phy_device *phydev)
{
	u16 ctrl_val;

	if (phydev->autoneg == AUTONEG_DISABLE)
		return 0;

	switch (phydev->master_slave_set) {
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		ctrl_val = QCA81XX_MS_FORCE_EN | QCA81XX_MS_MASTER;
		break;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
		ctrl_val = QCA81XX_MS_FORCE_EN;
		break;
	case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		ctrl_val = QCA81XX_MS_PREFER_MASTER;
		break;
	case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
		ctrl_val = 0;
		break;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
	default:
		return 0;
	}

	return phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL,
				      QCA81XX_MS_CTRL_MASK, ctrl_val);
}

int qca81xx_phy_master_slave_get(struct phy_device *phydev)
{
	int ctrl, stat;

	ctrl = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL);
	if (ctrl < 0)
		return ctrl;

	if (ctrl & QCA81XX_MS_FORCE_EN) {
		if (ctrl & QCA81XX_MS_MASTER)
			phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_FORCE;
		else
			phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_FORCE;
	} else {
		if (ctrl & QCA81XX_MS_PREFER_MASTER)
			phydev->master_slave_get = MASTER_SLAVE_CFG_MASTER_PREFERRED;
		else
			phydev->master_slave_get = MASTER_SLAVE_CFG_SLAVE_PREFERRED;
	}

	stat = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_STAT);
	if (stat < 0)
		return stat;

	if (stat & MDIO_AN_10GBT_STAT_MSFLT)
		phydev->master_slave_state = MASTER_SLAVE_STATE_ERR;
	else if (phydev->link && (stat & MDIO_AN_10GBT_STAT_MS))
		phydev->master_slave_state = MASTER_SLAVE_STATE_MASTER;
	else if (phydev->link)
		phydev->master_slave_state = MASTER_SLAVE_STATE_SLAVE;
	else
		phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;

	return 0;
}

static int qca81xx_phy_mdix_ctrl_set(struct phy_device *phydev)
{
	int ret;
	u16 val;

	switch (phydev->mdix_ctrl) {
	case ETH_TP_MDI:
		val = QCA81XX_MDI;
		break;
	case ETH_TP_MDI_X:
		val = QCA81XX_MDI_X;
		break;
	case ETH_TP_MDI_AUTO:
		val = QCA81XX_MDI_AUTO;
		break;
	default:
		return 0;
	}
	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_SPEC_CONTROL);
	if (ret < 0)
		return ret;
	if (FIELD_GET(QCA81XX_MDI_MASK, ret) != val) {
		/* the bit QCA81XX_AUTO_SOFT_RESET_EN need to set with MDI bits at the */
		/* same time to make sure the mdi take effect after soft reset */
		val = (ret & (~QCA81XX_MDI_MASK)) | FIELD_PREP(QCA81XX_MDI_MASK, val) |
			QCA81XX_AUTO_SOFT_RESET_EN;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_SPEC_CONTROL, val);
		if (ret < 0)
			return ret;
		ret = qca81xx_phy_soft_reset(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int qca81xx_phy_mdix_ctrl_get(struct phy_device *phydev)
{
	int ret;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_SPEC_CONTROL);
	if (ret < 0)
		return ret;
	switch (FIELD_GET(QCA81XX_MDI_MASK, ret)) {
	case QCA81XX_MDI:
		phydev->mdix_ctrl = ETH_TP_MDI;
		break;
	case QCA81XX_MDI_X:
		phydev->mdix_ctrl = ETH_TP_MDI_X;
		break;
	case QCA81XX_MDI_AUTO:
		phydev->mdix_ctrl = ETH_TP_MDI_AUTO;
		break;
	}

	return ret;
}

static int qca81xx_phy_config_aneg(struct phy_device *phydev)
{
	bool changed = false;
	u16 reg = 0;
	int ret = 0;

	qca81xx_priv_atomic64_inc(phydev,
		&((struct qca81xx_private *)phydev->priv)->debug_stats.config_aneg_count);

	if (phydev->autoneg == AUTONEG_DISABLE)
		return genphy_c45_pma_setup_forced(phydev);

	ret = genphy_c45_an_config_aneg(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/* Clause 45 has no standardized support for 1000BaseT, */
	/* therefore use vendor registers. */
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		phydev->advertising))
		reg |= QCA81XX_ADVERTISE_1000FULL;
	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_VEND2,
		QCA81XX_1000BASET_CONTROL,
		QCA81XX_ADVERTISE_1000FULL,
		reg);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	/* configure mdix mode */
	ret = qca81xx_phy_mdix_ctrl_set(phydev);
	if (ret < 0)
		return ret;

	/* configure master/slave mode */
	ret = qca81xx_phy_master_slave_set(phydev);
	if (ret < 0)
		return ret;
	if (ret > 0)
		changed = true;

	return genphy_c45_check_and_restart_aneg(phydev, changed);
}

static int qca81xx_phy_fifo_reset(struct phy_device *phydev,
	bool enable)
{
	u16 phy_data = 0;

	qca81xx_priv_atomic64_inc(phydev,
		&((struct qca81xx_private *)phydev->priv)->debug_stats.fifo_reset_count);

	if (!enable)
		phy_data |= QCA81XX_FIFO_RESET;

	return phy_modify_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_FIFO_CONTROL,
		QCA81XX_FIFO_RESET, phy_data);
}

static int qca81xx_phy_speed_fixup(struct phy_device *phydev)
{
	int ret = 0;
	bool port_clock_en = false;
	u16 phy_data = 0;

	/* adjust 10G EEE analog setting for master and slave */
	if (phydev->link && phydev->speed == SPEED_10000) {
		phy_data = phy_read_mmd(phydev, MDIO_MMD_AN,
			MDIO_AN_10GBT_STAT);
		phy_modify_mmd_changed(phydev, MDIO_MMD_PCS,
			QCA81XX_MMD3_10G_EEE_CFG,
			QCA81XX_MMD3_10G_EEE_CFG_MASK,
			(phy_data & MDIO_AN_10GBT_STAT_MS) ?
			QCA81XX_MMD3_10G_EEE_MST_CFG : QCA81XX_MMD3_10G_EEE_SLV_CFG);
	}
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_VEND2, QCA81XX_PCS_MMD31_MII_ERR_SEL,
		QCA81XX_PCS_MMD31_MII_AN_COMPLETE_INT, 0);
	if (ret < 0)
		return ret;
	mdelay(10);
	if (phydev->link) {
		ret = qca81xx_phy_speed_clk_set(phydev);
		if (ret < 0)
			return ret;
		/*avoid garbe data transmit out, need to assert ephy tx clock*/
		qca81xx_phy_txclk_reset_update(phydev, true);
		port_clock_en = true;
	}
	ret = qca81xx_pcs_clk_en_set(phydev, port_clock_en);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_clk_en_set(phydev, port_clock_en);
	if (ret < 0)
		return ret;
	mdelay(10);

	ret = qca81xx_pcs_clk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_clk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev,
		MDIO_MMD_PCS, QCA81XX_PCS_MMD3_DIG_CTRL1,
		QCA81XX_PCS_MMD3_USXG_FIFO_RESET,
		QCA81XX_PCS_MMD3_USXG_FIFO_RESET);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_fifo_reset(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);
	if (phydev->link) {
		ret = qca81xx_phy_fifo_reset(phydev, false);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int qca81xx_phy_hibernation_get(struct phy_device *phydev, bool *enabled)
{
	int phy_data;

	phy_data = qca81xx_phy_debug_read(phydev, QCA81XX_DEBUG_HIBERNATION_STATUS_REG);
	if (phy_data < 0)
		return phy_data;

	/* bit 11 is cleared by hardware when the PHY is in hibernation (active-low) */
	*enabled = !(phy_data & QCA81XX_DEBUG_HIBERNATION_BIT);
	return 0;
}

static int qca81xx_phy_pcs_check(struct phy_device *phydev)
{
	struct qca81xx_private *priv = phydev->priv;
	bool hibernation;
	int ret;

	ret = qca81xx_phy_hibernation_get(phydev, &hibernation);
	if (ret < 0)
		return ret;

	if (hibernation == priv->hibernation)
		return 0;

	priv->hibernation = hibernation;
	return qca81xx_phy_pcs_assert(phydev, hibernation);
}

static int qca81xx_phy_read_status(struct phy_device *phydev)
{
	int ret = 0;
	unsigned old_link = 0;

	/* if loopback is enabled, no need to read PHY */
	if (phydev->loopback_enabled)
		return 0;

	qca81xx_priv_atomic64_inc(phydev,
		&((struct qca81xx_private *)phydev->priv)->debug_stats.read_status_count);

	old_link = phydev->link;

	ret = genphy_c45_read_status(phydev);
	if (ret < 0)
		return ret;
	/* Clause 45 has no standardized support for 1000BaseT, */
	/* therefore use vendor registers. */
	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = phy_read_mmd(phydev, MDIO_MMD_VEND2,
			QCA81XX_1000BASET_STATUS);
		if (ret < 0)
			return ret;
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
			 phydev->lp_advertising,
			 ret & QCA81XX_LP_ADVERTISE_1000FULL);
		/* need to get speed and duplex again with new lp adv */
		phy_resolve_aneg_linkmode(phydev);
	}
	/* Some PHY maybe have downgrade issue and */
	/* send incorrect advertise, then the link */
	/* speed will be not correct, so need to read */
	/* link speed from the vendor register directly */
	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_SPEC_STATUS);
	if (ret < 0)
		return ret;
	if (ret & QCA81XX_INTR_DOWNSHIFT) {
		switch (ret & QCA81XX_SS_SPEED_MASK) {
		case QCA81XX_SS_SPEED_10000:
			phydev->speed = SPEED_10000;
			break;
		case QCA81XX_SS_SPEED_5000:
			phydev->speed = SPEED_5000;
			break;
		case QCA81XX_SS_SPEED_2500:
			phydev->speed = SPEED_2500;
			break;
		case QCA81XX_SS_SPEED_1000:
			phydev->speed = SPEED_1000;
			break;
		case QCA81XX_SS_SPEED_100:
			phydev->speed = SPEED_100;
			break;
		default:
			phydev->speed = SPEED_UNKNOWN;
		}
		if (ret & QCA81XX_SS_DUPLEX_FULL)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_UNKNOWN;
	}
	/* get the mdix ctrl and status */
	phydev->mdix = (ret & QCA81XX_SS_MDIX) ? ETH_TP_MDI_X : ETH_TP_MDI;
	ret = qca81xx_phy_mdix_ctrl_get(phydev);
	if (ret < 0)
		return ret;

	ret = qca81xx_phy_master_slave_get(phydev);
	if (ret < 0)
		return ret;

	ret = qca81xx_phy_pcs_check(phydev);
	if (ret < 0)
		return ret;

	if (phydev->link != old_link) {
		qca81xx_phy_speed_fixup(phydev);
	}

#if IS_ENABLED(CONFIG_HWMON)
	qca81xx_thermal_check(phydev);
#endif

	return 0;
}

static int qca81xx_phy_ack_interrupt(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_INTR_STATUS);

	return (ret < 0) ? ret : 0;
}

static int qca81xx_phy_config_intr(struct phy_device *phydev)
{
	int ret = 0;
	u16 phy_data = 0;

	phy_data = phy_read_mmd(phydev, MDIO_MMD_VEND2,
		QCA81XX_INTR_MASK);

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		ret = qca81xx_phy_ack_interrupt(phydev);
		if (ret < 0)
			return ret;
		phy_data = QCA81XX_INTR_STATUS_DOWN |
			QCA81XX_INTR_STATUS_UP;
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND2,
			QCA81XX_INTR_MASK, phy_data);
	} else {
		ret = phy_write_mmd(phydev, MDIO_MMD_VEND2,
			QCA81XX_INTR_MASK, 0);
		if (ret < 0)
			return ret;
		ret = qca81xx_phy_ack_interrupt(phydev);
	}

	return ret;
}

static irqreturn_t qca81xx_phy_handle_interrupt(struct phy_device *phydev)
{
	int irq_status, int_enabled;

	irq_status = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_INTR_STATUS);
	if (irq_status < 0) {
		phy_error(phydev);
		return IRQ_NONE;
	}

	/* Read the current enabled interrupts */
	int_enabled = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_INTR_MASK);
	if (int_enabled < 0) {
		phy_error(phydev);
		return IRQ_NONE;
	}

	/* See if this was one of our enabled interrupts */
	if (!(irq_status & int_enabled))
		return IRQ_NONE;

	phy_trigger_machine(phydev);

	return IRQ_HANDLED;
}

/*
|    sku    | ptp | macsec | 10g |
|-----------|-----|--------|-----|
|  QCA8101  | yes |   no   | no  |
|  QCA8102  | yes |   yes  | no  |
|  QCA8111  | yes |   no   | yes |
|  QCA8112  | yes |   yes  | yes |
|  NO-SKU   | yes |   yes  | yes |
*/
static int qca81xx_phy_sku_probe(struct phy_device *phydev)
{
	struct qca81xx_private *priv = phydev->priv;

	priv->sku.ptp = true;
	switch(qca81xx_soc_read(phydev, SKU_REG)) {
	case QCA8101:
		priv->sku.name = "QCA8101";
		break;
	case QCA8102:
		priv->sku.name = "QCA8102";
		priv->sku.macsec = true;
		break;
	case QCA8111:
		priv->sku.name = "QCA8111";
		break;
	case QCA8112:
		priv->sku.name = "QCA8112";
		priv->sku.macsec = true;
		break;
	default:
		priv->sku.name = "NO-SKU";
		priv->sku.macsec = true;
		break;
	}
	phydev_info(phydev, "sku:%s, ptp:%s, macsec:%s\n",
		priv->sku.name,
		priv->sku.ptp ? "enabled" : "disabled",
		priv->sku.macsec ? "enabled" : "disabled"
	);

	return 0;
}

static void qca81xx_phy_shutdown (struct device *dev)
{
	struct phy_device *phydev;

	phydev = to_phy_device(dev);
	if (phydev) {
		qca81xx_phy_pcs_assert(phydev, false);
		mdelay(1);
		qca81xx_soc_ahb_clk_set(phydev, AHB_CLK_50M);
		mdelay(1);
	}
}

ssize_t qca81xx_phy_show_snr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int pair_id = 0;
	u16 snr = 0, size = 0;
	char pair_name[][10] = {"Pair A","Pair B","Pair C","Pair D"};
	struct phy_device *phydev = to_phy_device(dev);

	if (!phydev)
		return 0;

	/* enable the monitor of SNR */
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PHY_MISC_CTRL0,
		QCA81XX_MMD3_PHY_PMA_MONITOR_EN0, QCA81XX_MMD3_PHY_PMA_MONITOR_EN0);
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PHY_MISC_CTRL1,
		QCA81XX_MMD3_PHY_PMA_MONITOR_EN1, QCA81XX_MMD3_PHY_PMA_MONITOR_EN1);
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PHY_SNR_MONITOR_STATUS,
		QCA81XX_MMD3_PHY_SNR_MONITOR_MASK, QCA81XX_MMD3_PHY_SNR_MONITOR_EN);
	usleep_range(200000, 300000);
	/* read the SNR */
	for (pair_id = ETHTOOL_A_CABLE_PAIR_A; pair_id <= ETHTOOL_A_CABLE_PAIR_D;
		pair_id++) {
		snr = phy_read_mmd(phydev, MDIO_MMD_PMAPMD,
			MDIO_PMA_10GBT_SNR + pair_id);
		size += snprintf(buf + size, (ssize_t)(PAGE_SIZE - size),
			"%s SNR: 0x%x\n", pair_name[pair_id], snr);
		if (size >= PAGE_SIZE)
			break;
	}
	/* disable the monitor of SNR to save power */
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PHY_MISC_CTRL0,
		QCA81XX_MMD3_PHY_PMA_MONITOR_EN0, 0);
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PHY_MISC_CTRL1,
		QCA81XX_MMD3_PHY_PMA_MONITOR_EN1, 0);
	phy_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PHY_SNR_MONITOR_STATUS,
		QCA81XX_MMD3_PHY_SNR_MONITOR_MASK, 0);

	return size;
}

static DEVICE_ATTR(snr, 0444, qca81xx_phy_show_snr, NULL);

static int qca81xx_phy_probe(struct phy_device *phydev)
{
	struct qca81xx_private *priv;

	phydev->priv = devm_kzalloc(&phydev->mdio.dev,
			sizeof(struct qca81xx_private), GFP_KERNEL);
	if (!phydev->priv)
		return -ENOMEM;

	priv = phydev->priv;
	priv->pcs_assert = false;
	priv->hibernation = false;

	qca81xx_phy_sku_probe(phydev);
#if IS_ENABLED(CONFIG_HWMON)
	qca81xx_hwmon_probe(phydev);
#endif
	device_create_file(&phydev->mdio.dev, &dev_attr_snr);

	/* Initialize sysfs module state/statistics support */
	qca81xx_debugfs_init(phydev);
	qca81xx_phy_suspend(phydev);

	return 0;
}

static void qca81xx_phy_remove(struct phy_device *phydev)
{
	/* Clean up debugfs support */
	qca81xx_debugfs_exit(phydev);

	device_remove_file(&phydev->mdio.dev, &dev_attr_snr);
}

static int qca81xx_phy_set_wol(struct phy_device *phydev,
	struct ethtool_wolinfo *wol)
{
	int ret, irq_enabled, i;
	const unsigned int offsets[] = {
		QCA81XX_MAC_ADDR_32_47,
		QCA81XX_MAC_ADDR_16_31,
		QCA81XX_MAC_ADDR_0_15,
	};

	if (!(wol->wolopts)) {
		for (i = 0; i < 3; i++)
			phy_write_mmd(phydev, MDIO_MMD_PCS, offsets[i], 0);
		ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
			QCA81XX_WOL_CTRL, QCA81XX_WOL_EN, 0);
		if (ret)
			return ret;
		/* Disable WOL interrupt */
		ret = phy_modify_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_INTR_MASK,
			QCA81XX_INTR_ENABLE_WOL, 0);
		if (ret)
			return ret;
	} else {
		if (wol->wolopts & WAKE_MAGIC) {
			struct net_device *ndev = phydev->attached_dev;
			const u8 *mac;

			if (!ndev)
				return -ENODEV;

			mac = (const u8 *)ndev->dev_addr;

			if (!is_valid_ether_addr(mac))
				return -EINVAL;

			for (i = 0; i < 3; i++)
				phy_write_mmd(phydev, MDIO_MMD_PCS, offsets[i],
					mac[(i * 2) + 1] | (mac[(i * 2)] << 8));
			ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
				QCA81XX_WOL_CTRL, QCA81XX_WOL_EN, QCA81XX_WOL_EN);
			if (ret)
				return ret;
			/* Enable WOL interrupt */
			ret = phy_modify_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_INTR_MASK, BIT(0),
				QCA81XX_INTR_ENABLE_WOL);
			if (ret)
				return ret;
		} else {
			return -EOPNOTSUPP;
		}
	}
	/* Clear WOL status */
	ret = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_INTR_STATUS);
	if (ret < 0)
		return ret;

	/* Check if there are other interrupts except for WOL triggered when PHY is
	 * in interrupt mode, only the interrupts enabled by QCA81XX_INTR_ENABLE_WOL
	 * can be passed up to the interrupt PIN.
	 */
	irq_enabled = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_INTR_MASK);
	if (irq_enabled < 0)
		return irq_enabled;

	irq_enabled &= ~QCA81XX_INTR_ENABLE_WOL;
	if (ret & irq_enabled && !phy_polling_mode(phydev))
		phy_trigger_machine(phydev);

	return 0;
}

static void qca81xx_phy_get_wol(struct phy_device *phydev,
	struct ethtool_wolinfo *wol)
{
	int value;

	wol->supported = WAKE_MAGIC;
	wol->wolopts = 0;

	value = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA81XX_WOL_CTRL);
	if (value < 0)
		return;

	if (value & QCA81XX_WOL_EN)
		wol->wolopts |= WAKE_MAGIC;
}

static int qca81xx_phy_get_downshift(struct phy_device *phydev, u8 *d)
{
	int val;

	val = phy_read_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_SMART_SPEED);
	if (val < 0)
		return val;

	if (val & QCA81XX_SMART_SPEED_ENABLE)
		*d = FIELD_GET(QCA81XX_SMART_SPEED_RETRY_LIMIT, val) + 2;
	else
		*d = DOWNSHIFT_DEV_DISABLE;

	return 0;
}

static int qca81xx_phy_set_downshift(struct phy_device *phydev, u8 cnt)
{
	u16 mask, set;
	int ret;

	switch (cnt) {
	case DOWNSHIFT_DEV_DEFAULT_COUNT:
		cnt = QCA81XX_DEFAULT_DOWNSHIFT;
		fallthrough;
	case QCA81XX_MIN_DOWNSHIFT ... QCA81XX_MAX_DOWNSHIFT:
		/* if the register value is 0, the downshift cnt is 2 */
		set = QCA81XX_SMART_SPEED_ENABLE |
			FIELD_PREP(QCA81XX_SMART_SPEED_RETRY_LIMIT,
			cnt - QCA81XX_MIN_DOWNSHIFT);
		mask = QCA81XX_SMART_SPEED_RETRY_LIMIT;
		break;
	case DOWNSHIFT_DEV_DISABLE:
		set = 0;
		mask = QCA81XX_SMART_SPEED_ENABLE;
		break;
	default:
		return -EINVAL;
	}

	ret = phy_modify_mmd_changed(phydev, MDIO_MMD_VEND2,
		QCA81XX_SMART_SPEED, mask, set);
	/* ret > 0 means the PHY register is configured again */
	if (ret > 0)
		ret = qca81xx_phy_soft_reset(phydev);

	return ret;
}

static int qca81xx_phy_get_tunable(struct phy_device *phydev,
	struct ethtool_tunable *tuna, void *data)
{
	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		return qca81xx_phy_get_downshift(phydev, data);
	default:
		return -EOPNOTSUPP;
	}
}

static int qca81xx_phy_set_tunable(struct phy_device *phydev,
	struct ethtool_tunable *tuna, const void *data)
{
	switch (tuna->id) {
	case ETHTOOL_PHY_DOWNSHIFT:
		return qca81xx_phy_set_downshift(phydev,
			*(const u8 *)data);
	default:
		return -EOPNOTSUPP;
	}
}

struct qca81xx_phy_stat {
	const char *name;
	u8 mmd;
	u16 reg_32_47;
	u16 reg_16_31;
	u16 reg_0_15;
};

static struct qca81xx_phy_stat qca81xx_phy_gmii_stats[] = {
	{ "RxGoodFrame", MDIO_MMD_AN,
		INVALID_MMD_REG,
		QCA81XX_MMD7_INGRESS_COUNTER_HIGH,
		QCA81XX_MMD7_INGRESS_COUNTER_LOW
	},
	{ "RxBadCRC", MDIO_MMD_AN,
		INVALID_MMD_REG,
		INVALID_MMD_REG,
		QCA81XX_MMD7_INGRESS_ERROR_COUNTER
	},
	{ "TxGoodFrame", MDIO_MMD_AN,
		INVALID_MMD_REG,
		QCA81XX_MMD7_EGRESS_COUNTER_HIGH,
		QCA81XX_MMD7_EGRESS_COUNTER_LOW
	},
	{ "TxBadCRC", MDIO_MMD_AN,
		INVALID_MMD_REG,
		INVALID_MMD_REG,
		QCA81XX_MMD7_EGRESS_ERROR_COUNTER
	},
};

static struct qca81xx_phy_stat qca81xx_phy_xgmii_stats[] = {
	{ "RxGoodFrame", MDIO_MMD_PCS,
		QCA81XX_MMD3_10G_INGRESS_COUNTER_HIGH,
		QCA81XX_MMD3_10G_INGRESS_COUNTER_MIDDLE,
		QCA81XX_MMD3_10G_INGRESS_COUNTER_LOW
	},
	{ "RxBadCRC", MDIO_MMD_PCS,
		INVALID_MMD_REG,
		INVALID_MMD_REG,
		QCA81XX_MMD3_10G_INGRESS_ERROR_COUNTER
	},
	{ "TxGoodFrame", MDIO_MMD_PCS,
		QCA81XX_MMD3_10G_EGRESS_COUNTER_HIGH,
		QCA81XX_MMD3_10G_EGRESS_COUNTER_MIDDLE,
		QCA81XX_MMD3_10G_EGRESS_COUNTER_LOW
	},
	{ "TxBadCRC", MDIO_MMD_PCS,
		INVALID_MMD_REG,
		INVALID_MMD_REG,
		QCA81XX_MMD3_10G_EGRESS_ERROR_COUNTER
	},
};

/**
 * qca81xx_phy_stats_enable - Enable PHY statistics collection
 * @phydev: PHY device
 *
 * Enables frame check and counter functionality for PHY statistics.
 * This function configures both 10G and GMII frame checking and enables
 * self-clearing counters for statistics collection.
 *
 * Returns: 0 on success, negative error code on failure
 */
int qca81xx_phy_stats_enable(struct phy_device *phydev)
{
	int ret = 0;

	ret = phy_modify_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_10G_FRAME_CHECK_CTRL,
		QCA81XX_MMD3_10G_FRAME_CHECK_EN,
		QCA81XX_MMD3_10G_FRAME_CHECK_EN);
	if (ret < 0)
		return ret;
	ret = phy_modify_mmd(phydev, MDIO_MMD_AN,
		QCA81XX_MMD7_COUNTER_CTRL,
		QCA81XX_MMD7_FRAME_CHECK_EN | QCA81XX_MMD7_CNT_SELFCLR,
		QCA81XX_MMD7_FRAME_CHECK_EN | QCA81XX_MMD7_CNT_SELFCLR);

	return ret;
}

static int qca81xx_phy_xgmii_get_sset_count(struct phy_device *phydev)
{
	return ARRAY_SIZE(qca81xx_phy_xgmii_stats);
}

static int qca81xx_phy_gmii_get_sset_count(struct phy_device *phydev)
{
	return ARRAY_SIZE(qca81xx_phy_gmii_stats);
}

static int qca81xx_phy_get_sset_count(struct phy_device *phydev)
{
	if (phydev->speed >= SPEED_2500)
		return qca81xx_phy_xgmii_get_sset_count(phydev);
	else
		return qca81xx_phy_gmii_get_sset_count(phydev);
}

static void qca81xx_phy_xgmii_get_strings(struct phy_device *phydev,
	u8 *data)
{
	int i, size;

	size = qca81xx_phy_xgmii_get_sset_count(phydev);

	for (i = 0; i < size; i++)
		strscpy(data + i * ETH_GSTRING_LEN, qca81xx_phy_xgmii_stats[i].name,
			ETH_GSTRING_LEN);
}

static void qca81xx_phy_gmii_get_strings(struct phy_device *phydev,
	u8 *data)
{
	int i, size;

	size = qca81xx_phy_gmii_get_sset_count(phydev);

	for (i = 0; i < size; i++)
		strscpy(data + i * ETH_GSTRING_LEN, qca81xx_phy_gmii_stats[i].name,
			ETH_GSTRING_LEN);
}

static void qca81xx_phy_get_strings(struct phy_device *phydev,
	u8 *data)
{
	if (phydev->speed >= SPEED_2500)
		qca81xx_phy_xgmii_get_strings(phydev, data);
	else
		qca81xx_phy_gmii_get_strings(phydev, data);
}

static void qca81xx_phy_xgmii_get_stats(struct phy_device *phydev,
	struct ethtool_stats *stats, u64 *data)
{
	struct qca81xx_phy_stat phy_stat = {0};
	u16 reg_32_47 = 0, reg_16_31 = 0, reg_0_15 = 0;
	int i = 0, size = 0;

	size = qca81xx_phy_xgmii_get_sset_count(phydev);
	for (i = 0; i < size; i++) {
		reg_32_47 = reg_16_31 = reg_0_15 = 0;

		phy_stat = qca81xx_phy_xgmii_stats[i];

		if (phy_stat.reg_32_47 != INVALID_MMD_REG)
			reg_32_47 = phy_read_mmd(phydev,
			phy_stat.mmd,
			phy_stat.reg_32_47);
		if (phy_stat.reg_16_31 != INVALID_MMD_REG)
			reg_16_31 = phy_read_mmd(phydev,
			phy_stat.mmd,
			phy_stat.reg_16_31);
		if (phy_stat.reg_0_15 != INVALID_MMD_REG)
			reg_0_15 = phy_read_mmd(phydev,
			phy_stat.mmd,
			phy_stat.reg_0_15);

		data[i] = ((u64)reg_32_47 << 32) | (reg_16_31 << 16) |
			reg_0_15;
	}
}

static void qca81xx_phy_gmii_get_stats(struct phy_device *phydev,
	struct ethtool_stats *stats, u64 *data)
{
	struct qca81xx_phy_stat phy_stat = {0};
	u16 reg_16_31 = 0, reg_0_15 = 0;
	int i = 0, size = 0;

	size = qca81xx_phy_gmii_get_sset_count(phydev);
	for (i = 0; i < size; i++) {
		reg_16_31 = reg_0_15 = 0;

		phy_stat = qca81xx_phy_gmii_stats[i];

		if (phy_stat.reg_16_31 != INVALID_MMD_REG)
			reg_16_31 = phy_read_mmd(phydev,
			phy_stat.mmd,
			phy_stat.reg_16_31);
		if (phy_stat.reg_0_15 != INVALID_MMD_REG)
			reg_0_15 = phy_read_mmd(phydev,
			phy_stat.mmd,
			phy_stat.reg_0_15);

		data[i] = ((u64)reg_16_31 << 16) | reg_0_15;
	}
}

static void qca81xx_phy_get_stats(struct phy_device *phydev,
	struct ethtool_stats *stats, u64 *data)
{
	if (phydev->speed >= SPEED_2500)
		qca81xx_phy_xgmii_get_stats(phydev, stats, data);
	else
		qca81xx_phy_gmii_get_stats(phydev, stats, data);
}

static int qca81xx_phy_cable_test_completion(struct phy_device *phydev)
{
	int val, ret;

	ret = read_poll_timeout(phy_read_mmd,
		val, !(val & QCA81XX_CDT_ENABLE_TEST),
		20000, 100000, true, phydev, MDIO_MMD_VEND2,
		QCA81XX_CDT);

	return ret < 0 ? ret : 0;
}

/**
 * qca81xx_phy_cable_test_fault_valid - Check if a cable test status code indicates a valid fault
 * @status_code: The cable test status code to check
 *
 * Determines if the given status code represents a valid cable fault (open or short).
 *
 * Return: true if the status code indicates a valid fault, false otherwise
 */
static bool qca81xx_phy_cable_test_fault_valid(int status_code)
{
	switch (status_code) {
	case QCA81XX_CDT_STATUS_STAT_SAME_SHORT:
	case QCA81XX_CDT_STATUS_STAT_SAME_OPEN:
		return true;
	default:
		return false;
	}
}

/**
 * qca81xx_phy_cable_test_result_trans - Translate cable test status code to ethtool result code
 * @status_code: The cable test status code to translate
 *
 * Converts internal cable test status codes to standardized ethtool cable test result codes.
 *
 * Return: The corresponding ethtool cable test result code
 */
static int qca81xx_phy_cable_test_result_trans(int status_code)
{
	switch (status_code) {
	case QCA81XX_CDT_STATUS_STAT_NORMAL:
		return ETHTOOL_A_CABLE_RESULT_CODE_OK;
	case QCA81XX_CDT_STATUS_STAT_SAME_SHORT:
		return ETHTOOL_A_CABLE_RESULT_CODE_SAME_SHORT;
	case QCA81XX_CDT_STATUS_STAT_SAME_OPEN:
		return ETHTOOL_A_CABLE_RESULT_CODE_OPEN;
	case QCA81XX_CDT_STATUS_STAT_FAIL:
	default:
		return ETHTOOL_A_CABLE_RESULT_CODE_UNSPEC;
	}
}

static u32 qca81xx_phy_cable_test_fault_length(struct phy_device *phydev,
	u8 pair)
{
	int val;
	u32 length_reg = 0, delta_length;

	switch (pair) {
	case ETHTOOL_A_CABLE_PAIR_A:
		length_reg = QCA81XX_MMD3_CDT_DIAG_PAIR_A;
		break;
	case ETHTOOL_A_CABLE_PAIR_B:
		length_reg = QCA81XX_MMD3_CDT_DIAG_PAIR_B;
		break;
	case ETHTOOL_A_CABLE_PAIR_C:
		length_reg = QCA81XX_MMD3_CDT_DIAG_PAIR_C;
		break;
	case ETHTOOL_A_CABLE_PAIR_D:
		length_reg = QCA81XX_MMD3_CDT_DIAG_PAIR_D;
		break;
	default:
		return 0;
	}

	val = phy_read_mmd(phydev, MDIO_MMD_PCS, length_reg);
	if (val < 0) {
		phydev_err(phydev, "Failed to read length: %d\n", val);
		return 0;
	}
	/* Extract time delta value and ensure it's within valid range */
	delta_length = ((val & QCA81XX_CDT_STATUS_DELTA_TIME_MASK) * 824UL);

	return delta_length / 10;
}

static int qca81xx_phy_cable_test_get_pair_status(struct phy_device *phydev,
	u8 pair, u16 status)
{
	int result, ret;
	u16 pair_code;
	u32 length;

	if (pair > ETHTOOL_A_CABLE_PAIR_D)
		return -EINVAL;

	/* Calculate bit shift based on pair number (12, 8, 4, 0) */
	pair_code = (status & (QCA81XX_CDT_CODE_MASK << (12 - (pair * 4))))
		>> (12 - (pair * 4));
	result = qca81xx_phy_cable_test_result_trans(pair_code);
	ret = ethnl_cable_test_result(phydev, pair, result);
	if (ret < 0) {
		phydev_err(phydev, "Failed to report cable test result for pair %d: %d\n",
			pair, ret);
		return ret;
	}
	if (qca81xx_phy_cable_test_fault_valid(pair_code)) {
		length = qca81xx_phy_cable_test_fault_length(phydev, pair);
		ret = ethnl_cable_test_fault_length(phydev, pair, length);
		if (ret < 0) {
			phydev_err(phydev, "Failed to report fault length for pair %d: %d\n",
				pair, ret);
			return ret;
		}
	}

	return 0;
}

static int qca81xx_phy_afe_dac_restore(struct phy_device *phydev)
{
	int ret;
	struct qca81xx_private *priv = phydev->priv;

	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC8_DP,
		priv->afe_dac8);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC9_DP,
		priv->afe_dac9);

	return ret;
}

/**
 * qca81xx_phy_cable_test_get_status - get the result of cable test
 * @phydev: The PHY device structure
 * @finished: Pointer to boolean that will be set to true when test completes
 *
 * get the result of cable test including status(open/normal/short) and cable
 * length for open/short.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qca81xx_phy_cable_test_get_status(struct phy_device *phydev,
	bool *finished)
{
	int ret = 0, val = 0, pair_id = 0, pair_ret = 0;
	bool pairs_processed = false;

	const int all_pairs_normal = QCA81XX_CDT_STATUS_STAT_NORMAL |
		(QCA81XX_CDT_STATUS_STAT_NORMAL << 4) |
		(QCA81XX_CDT_STATUS_STAT_NORMAL << 8) |
		(QCA81XX_CDT_STATUS_STAT_NORMAL << 12);

	*finished = false;

	if (phydev->link) {
		val = all_pairs_normal;
	} else {
		ret = qca81xx_phy_cable_test_completion(phydev);
		if (ret < 0)
			goto restore;
		pair_ret = phy_read_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_CDT_STATUS);
		if (pair_ret < 0) {
			phydev_err(phydev, "Failed to get the cdt status ret: %d\n", pair_ret);
			ret = pair_ret;
			goto restore;
		}
		val = pair_ret;
	}
	for (pair_id = ETHTOOL_A_CABLE_PAIR_A; pair_id <= ETHTOOL_A_CABLE_PAIR_D;
		pair_id++) {
		pair_ret = qca81xx_phy_cable_test_get_pair_status(phydev, pair_id, val);
		if (pair_ret < 0) {
			if (ret == 0)
				ret = pair_ret;
		} else {
			pairs_processed = true;
		}
	}
	*finished = pairs_processed;
restore:
	if (phydev->link == false) {
		int restore_ret = qca81xx_phy_afe_dac_restore(phydev);
		if (restore_ret < 0) {
			if (ret == 0)
				ret = restore_ret;
		}
	}

	return ret;
}

/**
 * qca81xx_phy_cable_test_start  - Start the cable test
 * @phydev: The PHY device structure
 *
 * Initiates the appropriate settings and start cable test.
 *
 * Return: 0 on success, negative error code on failure
 */
static int qca81xx_phy_cable_test_start(struct phy_device *phydev)
{
	int ret;
	struct qca81xx_private *priv = phydev->priv;

	if (phydev->link)
		return 0;
	/* store the dac8 and dac9 value */
	ret = qca81xx_phy_debug_read(phydev, QCA81XX_DEBUG_ANA_AFE_DAC8_DP);
	if (ret < 0)
		return ret;
	priv->afe_dac8 = ret;
	ret = qca81xx_phy_debug_read(phydev, QCA81XX_DEBUG_ANA_AFE_DAC9_DP);
	if (ret < 0)
		return ret;
	priv->afe_dac9 = ret;

	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC8_DP, 0);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC9_DP, 0);
	if (ret < 0)
		return ret;
	ret = phy_modify_mmd(phydev, MDIO_MMD_VEND2, QCA81XX_CDT,
		QCA81XX_CDT_ENABLE_TEST | QCA81XX_CDT_LENGTH_UNIT,
		QCA81XX_CDT_ENABLE_TEST | QCA81XX_CDT_LENGTH_UNIT);
	if (ret < 0) {
		phydev_err(phydev, "Failed to enable cable test: %d\n", ret);
		qca81xx_phy_afe_dac_restore(phydev);
	}

	return ret;
}

static bool qca81xx_led_index_valid(struct phy_device *phydev, u8 index)
{
	if (index >= QCA81XX_LED_NUM)
		return false;

	return true;
}

static int qca81xx_led_parse_netdev(struct phy_device *phydev,
	unsigned long rules, u16 *offload_trigger)
{
	/* Parsing specific to netdev trigger */
	if (test_bit(TRIGGER_NETDEV_TX, &rules))
		*offload_trigger |= QCA81XX_LED_TX_BLINK;
	if (test_bit(TRIGGER_NETDEV_RX, &rules))
		*offload_trigger |= QCA81XX_LED_RX_BLINK;
	if (test_bit(TRIGGER_NETDEV_LINK_100, &rules))
		*offload_trigger |= QCA81XX_LED_SPEED100_ON;
	if (test_bit(TRIGGER_NETDEV_LINK_1000, &rules))
		*offload_trigger |= QCA81XX_LED_SPEED1000_ON;
	if (test_bit(TRIGGER_NETDEV_LINK_2500, &rules))
		*offload_trigger |= QCA81XX_LED_SPEED2500_ON;
	if (test_bit(TRIGGER_NETDEV_FULL_DUPLEX, &rules))
		*offload_trigger |= QCA81XX_LED_FULL_DUPLEX_ON;

	if (rules && !*offload_trigger)
		return -EOPNOTSUPP;

	/* Enable BLINK_CHECK_BYPASS by default to make the LED
	 * blink even with duplex or speed mode not enabled.
	 */
	*offload_trigger |= QCA81XX_LED_BLINK_CHECK_BYPASS;

	return 0;
}

static int qca81xx_led_hw_is_supported(struct phy_device *phydev, u8 index,
	unsigned long rules)
{
	u16 offload_trigger = 0;

	if (!qca81xx_led_index_valid(phydev, index))
		return -EINVAL;

	return qca81xx_led_parse_netdev(phydev, rules, &offload_trigger);
}

static int qca81xx_led_hw_control_set(struct phy_device *phydev, u8 index,
	unsigned long rules)
{
	u16 reg, offload_trigger = 0;
	int ret;

	if (!qca81xx_led_index_valid(phydev, index))
		return -EINVAL;
	ret = qca81xx_led_parse_netdev(phydev, rules, &offload_trigger);
	if (ret < 0)
		return ret;
	reg = QCA81XX_MMD7_LED_FORCE_CTRL(index);
	ret = phy_clear_bits_mmd(phydev, MDIO_MMD_AN, reg,
		QCA81XX_LED_FORCE_EN);
	if (ret < 0)
		return ret;

	reg = QCA81XX_MMD7_LED_CTRL(index);

	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, reg,
		QCA81XX_LED_PATTERN_MASK, offload_trigger);

	return ret;
}

static int qca81xx_led_hw_control_get(struct phy_device *phydev, u8 index,
	unsigned long *rules)
{
	u16 reg;
	int val;

	if (!qca81xx_led_index_valid(phydev, index))
		return -EINVAL;

	reg = QCA81XX_MMD7_LED_FORCE_CTRL(index);
	val = phy_read_mmd(phydev, MDIO_MMD_AN, reg);
	if (val < 0)
		return val;
	if (val & QCA81XX_LED_FORCE_EN)
		return -EINVAL;

	reg = QCA81XX_MMD7_LED_CTRL(index);

	val = phy_read_mmd(phydev, MDIO_MMD_AN, reg);
	if (val < 0)
		return val;
	if (val & QCA81XX_LED_TX_BLINK)
		set_bit(TRIGGER_NETDEV_TX, rules);
	if (val & QCA81XX_LED_RX_BLINK)
		set_bit(TRIGGER_NETDEV_RX, rules);
	if (val & QCA81XX_LED_SPEED100_ON)
		set_bit(TRIGGER_NETDEV_LINK_100, rules);
	if (val & QCA81XX_LED_SPEED1000_ON)
		set_bit(TRIGGER_NETDEV_LINK_1000, rules);
	if (val & QCA81XX_LED_SPEED2500_ON)
		set_bit(TRIGGER_NETDEV_LINK_2500, rules);
	if (val & QCA81XX_LED_FULL_DUPLEX_ON)
		set_bit(TRIGGER_NETDEV_FULL_DUPLEX, rules);

	return 0;
}

static int qca81xx_led_brightness_set(struct phy_device *phydev,
	u8 index, enum led_brightness value)
{
	u16 reg;

	if (!qca81xx_led_index_valid(phydev, index))
		return -EINVAL;

	reg = QCA81XX_MMD7_LED_FORCE_CTRL(index);
	return phy_modify_mmd(phydev, MDIO_MMD_AN, reg,
		QCA81XX_LED_FORCE_EN | QCA81XX_LED_FORCE_MODE_MASK,
		QCA81XX_LED_FORCE_EN | (value ? QCA81XX_LED_FORCE_ON :
		QCA81XX_LED_FORCE_OFF));
}

static int qca81xx_led_blink_set(struct phy_device *phydev, u8 index,
	unsigned long *delay_on, unsigned long *delay_off)
{
	int ret;
	unsigned long on = *delay_on, off = *delay_off, freq_hz = 4;
	u16 freq_field = QCA81XX_LED_BLINK_FREQ_4HZ;
	u16 duty_field = QCA81XX_LED_BLINK_DUTY_50_50;
	u16 reg;
	struct {
		unsigned long period;
		u16 freq_field;
		unsigned long freq_hz;
	} freq_table[] = {
		{ 1000/2, QCA81XX_LED_BLINK_FREQ_2HZ, 2 },
		{ 1000/4, QCA81XX_LED_BLINK_FREQ_4HZ, 4 },
		{ 1000/8, QCA81XX_LED_BLINK_FREQ_8HZ, 8 },
		{ 1000/16, QCA81XX_LED_BLINK_FREQ_16HZ, 16 },
		{ 1000/32, QCA81XX_LED_BLINK_FREQ_32HZ, 32 },
		{ 1000/64, QCA81XX_LED_BLINK_FREQ_64HZ, 64 },
		{ 1000/128, QCA81XX_LED_BLINK_FREQ_128HZ, 128 },
		{ 1000/256, QCA81XX_LED_BLINK_FREQ_256HZ, 256 },
	};
	struct {
		unsigned long threshold;
		u16 duty_field;
	} duty_table[] = {
		{ 83, QCA81XX_LED_BLINK_DUTY_83_17 },
		{ 75, QCA81XX_LED_BLINK_DUTY_75_25 },
		{ 67, QCA81XX_LED_BLINK_DUTY_67_33 },
		{ 50, QCA81XX_LED_BLINK_DUTY_50_50 },
		{ 33, QCA81XX_LED_BLINK_DUTY_33_67 },
		{ 25, QCA81XX_LED_BLINK_DUTY_25_75 },
		{ 17, QCA81XX_LED_BLINK_DUTY_17_83 },
		{ 8,  QCA81XX_LED_BLINK_DUTY_8_92 },
	};
	int i;

	if (!qca81xx_led_index_valid(phydev, index))
		return -EINVAL;
	reg = QCA81XX_MMD7_LED_FORCE_CTRL(index);
	/* if both on and off are 0, then freq is 4hz, duty is 50% */
	if (on == 0 && off == 0) {
		*delay_on = 125;
		*delay_off = 125;
	} else {
		unsigned long period = on + off;
		unsigned long duty = 0;

		/* Find appropriate frequency using lookup table */
		for (i = 0; i < ARRAY_SIZE(freq_table); i++) {
			if (period >= freq_table[i].period) {
				freq_field = freq_table[i].freq_field;
				freq_hz = freq_table[i].freq_hz;
				break;
			}
		}
		if (i == ARRAY_SIZE(freq_table))
			return -EOPNOTSUPP;

		/* calculate the duty */
		if (period > 0)
			duty = (on * 100) / period;
		else
			duty = 50;
		/* Find appropriate duty cycle */
		for (i = 0; i < ARRAY_SIZE(duty_table); i++) {
			if (duty >= duty_table[i].threshold) {
				duty_field = duty_table[i].duty_field;
				break;
			}
		}
		/* If no match, then return not support */
		if (i == ARRAY_SIZE(duty_table))
			return -EOPNOTSUPP;

		*delay_on = (1000 / freq_hz) * duty / 100;
		*delay_off = (1000 / freq_hz) - *delay_on;
	}

	ret = phy_modify_mmd(phydev, MDIO_MMD_AN, QCA81XX_MMD7_LED_GLOBAL,
		QCA81XX_LED_BLINK_FREQ_MASK | QCA81XX_LED_BLINK_DUTY_MASK,
		freq_field | duty_field);
	if (ret)
		return ret;
	return phy_modify_mmd(phydev, MDIO_MMD_AN, reg,
		QCA81XX_LED_FORCE_EN | QCA81XX_LED_FORCE_MODE_MASK,
		QCA81XX_LED_FORCE_EN | QCA81XX_LED_FORCE_BLINK);
}

static int qca81xx_led_polarity_set(struct phy_device *phydev, int index,
	unsigned long modes)
{
	bool active_low = false;
	u32 mode;

	for_each_set_bit(mode, &modes, __PHY_LED_MODES_NUM) {
		switch (mode) {
		case PHY_LED_ACTIVE_LOW:
			active_low = true;
			break;
		default:
			return -EINVAL;
		}
	}

	return phy_modify_mmd(phydev, MDIO_MMD_AN,
		QCA81XX_MMD7_LED_POLARITY_CTRL,
		QCA81XX_LED_ACTIVE_HIGH,
		active_low ? 0 : QCA81XX_LED_ACTIVE_HIGH);
}

/**
 * qca81xx_phy_show_debug_module_statistics - Show PHY API call statistics
 * @dev: The device structure
 * @attr: The device attribute structure
 * @buf: Buffer to write the statistics to
 *
 * Display statistics of all standard PHY API calls including read/write
 * operations and configuration functions.
 *
 * Return: Number of bytes written to buffer
 */
static ssize_t qca81xx_phy_show_debug_module_statistics(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct qca81xx_private *priv = phydev->priv;
	ssize_t size = 0;

	if (!priv)
		return -EINVAL;

	size += snprintf(buf + size, PAGE_SIZE - size,
		"QCA81XX PHY Debug Statistics\n");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    API Call Counters\n");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"        Read Status Calls         : %llu\n",
		atomic64_read(&priv->debug_stats.read_status_count));
	size += snprintf(buf + size, PAGE_SIZE - size,
		"        Config Aneg Calls         : %llu\n",
		atomic64_read(&priv->debug_stats.config_aneg_count));
	size += snprintf(buf + size, PAGE_SIZE - size,
		"        Soft Reset Calls          : %llu\n",
		atomic64_read(&priv->debug_stats.soft_reset_count));
	size += snprintf(buf + size, PAGE_SIZE - size,
		"        Fifo Reset Calls          : %llu\n",
		atomic64_read(&priv->debug_stats.fifo_reset_count));
	size += snprintf(buf + size, PAGE_SIZE - size,
		"        Suspend Calls             : %llu\n",
		atomic64_read(&priv->debug_stats.suspend_count));
	size += snprintf(buf + size, PAGE_SIZE - size,
		"        Resume Calls              : %llu\n",
		atomic64_read(&priv->debug_stats.resume_count));

	return size;
}

/**
 * qca81xx_phy_show_debug_module_state - Show PHY module state information
 * @dev: The device structure
 * @attr: The device attribute structure
 * @buf: Buffer to write the state information to
 *
 * Display comprehensive PHY module state including initialization status,
 * SKU information, and internal states not available through ethtool.
 *
 * Return: Number of bytes written to buffer
 */
static ssize_t qca81xx_phy_show_debug_module_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct qca81xx_private *priv = phydev->priv;
	ssize_t size = 0;

	if (!priv)
		return -EINVAL;

	switch (priv->init_state) {
	case QCA81XX_INIT_START:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Success\n", "PHY INIT START");
		break;
	case QCA81XX_INIT_GCC_PRE_INIT_FAILURE:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Failure\n", "PHY GCC PRE-INIT");
		break;
	case QCA81XX_INIT_ANA_CONFIG_FAILURE:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Failure\n", "PHY ANA CONFIG");
		break;
	case QCA81XX_INIT_PCS_USXGMII_FAILURE:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Failure\n", "PHY PCS USXGMII");
		break;
	case QCA81XX_INIT_GCC_POST_INIT_FAILURE:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Failure\n", "PHY GCC POST-INIT");
		break;
	case QCA81XX_INIT_SEC_CTRL_FAILURE:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Failure\n", "PHY SECURITY CONTROL");
		break;
	case QCA81XX_INIT_TLMM_FAILURE:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Failure\n", "PHY TLMM");
		break;
	case QCA81XX_INIT_SUCCESS:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : Success\n", "PHY INIT");
		break;
	case QCA81XX_INIT_INVALID_STATE:
	default:
		size += snprintf(buf + size, PAGE_SIZE - size,
			"%-24s : INVALID\n", "PHY INIT");
		break;
	}

	/* SKU Information (from priv, not reading registers) */
	size += snprintf(buf + size, PAGE_SIZE - size,
		"\n%s\n", "SKU Information");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "SKU", priv->sku.name);
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "PTP Support", priv->sku.ptp ? "enabled" : "disabled");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "MACSEC Support", priv->sku.macsec ? "enabled" : "disabled");

	/* PHY Device Structure Information (not available via ethtool) */
	size += snprintf(buf + size, PAGE_SIZE - size,
		"\n%s\n", "PHY Device Internal State");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "is_c45", phydev->is_c45 ? "true" : "false");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "suspended", phydev->suspended ? "true" : "false");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "pcs_assert", priv->pcs_assert ? "asserted" : "de-asserted");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "loopback_enabled", phydev->loopback_enabled ? "true" : "false");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "interrupts", phydev->interrupts ? "enabled" : "disabled");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "irq_suspended", phydev->irq_suspended ? "true" : "false");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %d\n", "state", phydev->state);
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s (%d)\n", "interface", phy_modes(phydev->interface), phydev->interface);
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : %s\n", "eee_enabled", phydev->eee_enabled ? "true" : "false");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : 0x%08x\n", "eee_broken_modes", phydev->eee_broken_modes);

	/* Internal AFE Values (from priv, not reading registers) */
	size += snprintf(buf + size, PAGE_SIZE - size,
		"\n%s\n", "AFE Configuration");
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : 0x%04x\n", "AFE DAC8", priv->afe_dac8);
	size += snprintf(buf + size, PAGE_SIZE - size,
		"    %-20s : 0x%04x\n", "AFE DAC9", priv->afe_dac9);

	return size;
}

/**
 * qca81xx_debug_module_reset_statistics - Reset debug statistics counters
 * @priv: The private data structure
 *
 * Reset all debug statistics counters to zero.
 */
static void _qca81xx_phy_debug_module_reset_statistics(struct qca81xx_private *priv)
{
	if (!priv)
		return;

	/* Reset all counters */
	atomic64_set(&priv->debug_stats.read_status_count, 0);
	atomic64_set(&priv->debug_stats.config_aneg_count, 0);
	atomic64_set(&priv->debug_stats.soft_reset_count, 0);
	atomic64_set(&priv->debug_stats.fifo_reset_count, 0);
	atomic64_set(&priv->debug_stats.suspend_count, 0);
	atomic64_set(&priv->debug_stats.resume_count, 0);
}

/**
 * qca81xx_phy_debug_module_reset_statistics - Reset debug statistics counters on write
 * @dev: The device structure
 * @attr: The device attribute structure
 * @buf: Buffer containing the input (ignored)
 * @count: Number of bytes in the buffer
 *
 * Reset all debug statistics counters to zero when this sysfs node is written to.
 *
 * Return: Number of bytes processed
 */
static ssize_t qca81xx_phy_debug_module_reset_statistics(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct qca81xx_private *priv = phydev->priv;

	if (!priv)
		return -EINVAL;

	if (count > 0 && (buf[0] == '0' || buf[0] == '\n'))
		_qca81xx_phy_debug_module_reset_statistics(priv);

	return count;
}

/* Device attribute definitions */
static DEVICE_ATTR(module_statistics, 0644, qca81xx_phy_show_debug_module_statistics, qca81xx_phy_debug_module_reset_statistics);
static DEVICE_ATTR(module_state, 0444, qca81xx_phy_show_debug_module_state, NULL);

/**
 * qca81xx_debugfs_init - Initialize debugfs support for PHY device
 * @phydev: The PHY device structure
 *
 * Create sysfs attribute files for debug statistics and module state.
 *
 * Return: 0 on success, negative error code on failure
 */
int qca81xx_debugfs_init(struct phy_device *phydev)
{
	struct qca81xx_private *priv = phydev->priv;
	int ret = 0;

	if (!priv)
		return -EINVAL;

	/* Initialize debug statistics */
	_qca81xx_phy_debug_module_reset_statistics(priv);

	/* Create sysfs attribute files */
	ret = device_create_file(&phydev->mdio.dev, &dev_attr_module_statistics);
	if (ret) {
		phydev_err(phydev, "Failed to create module_statistics sysfs file: %d\n", ret);
		return ret;
	}

	ret = device_create_file(&phydev->mdio.dev, &dev_attr_module_state);
	if (ret) {
		phydev_err(phydev, "Failed to create module_state sysfs file: %d\n", ret);
		device_remove_file(&phydev->mdio.dev, &dev_attr_module_statistics);
		return ret;
	}

	return 0;
}

/**
 * qca81xx_debugfs_exit - Clean up debugfs support for PHY device
 * @phydev: The PHY device structure
 *
 * Remove sysfs attribute files created for debug support.
 */
void qca81xx_debugfs_exit(struct phy_device *phydev)
{
	device_remove_file(&phydev->mdio.dev, &dev_attr_module_statistics);
	device_remove_file(&phydev->mdio.dev, &dev_attr_module_state);
}

/* QCE1204 PHY ID and function declarations */
#define QCE1204_PHY		0x004dd190

/**
 * qce1204_phy_probe - Probe QCE1204 PHY device
 * @phydev: PHY device structure
 *
 * Return: 0 on success, negative error code on failure
 */
int qce1204_phy_probe(struct phy_device *phydev);

/**
 * qce1204_phy_remove - Remove QCE1204 PHY device
 * @phydev: PHY device structure
 */
void qce1204_phy_remove(struct phy_device *phydev);

/**
 * qce1204_phy_config_init - Configure initial settings for QCE1204 PHY
 * @phydev: PHY device structure
 *
 * Return: 0 on success, negative error code on failure
 */
int qce1204_phy_config_init(struct phy_device *phydev);

/**
 * qce1204_phy_config_aneg - Configure auto-negotiation for QCE1204 PHY
 * @phydev: PHY device structure
 *
 * Return: 0 on success, negative error code on failure
 */
int qce1204_phy_config_aneg(struct phy_device *phydev);

/**
 * qce1204_phy_read_status - Read the status of QCE1204 PHY
 * @phydev: PHY device structure
 *
 * Return: 0 on success, negative error code on failure
 */
int qce1204_phy_read_status(struct phy_device *phydev);

/**
 * qce1204_phy_soft_reset - Perform a soft reset on QCE1204 PHY
 * @phydev: PHY device structure
 *
 * Return: 0 on success, negative error code on failure
 */
int qce1204_phy_soft_reset(struct phy_device *phydev);

/* IPQ52XX Built-in PHY ID and function declarations */
#define IPQ52XX_PHY			0x004dd120

int ipq52xx_phy_config_init(struct phy_device *phydev);
int ipq52xx_phy_probe(struct phy_device *phydev);
void ipq52xx_phy_remove(struct phy_device *phydev);
int ipq52xx_phy_read_status(struct phy_device *phydev);

static struct phy_driver qcom_phy_driver[] = {
{
	PHY_ID_MATCH_EXACT(QCA8111_PHY),
	.name = "Qualcomm QCA81xx",
	.flags = PHY_POLL_CABLE_TEST,
	.probe = qca81xx_phy_probe,
	.remove = qca81xx_phy_remove,
	.config_init = qca81xx_phy_config_init,
	.get_features = qca81xx_phy_get_features,
	.config_aneg = qca81xx_phy_config_aneg,
	.config_intr = qca81xx_phy_config_intr,
	.handle_interrupt = qca81xx_phy_handle_interrupt,
	.read_status = qca81xx_phy_read_status,
	.suspend = qca81xx_phy_suspend,
	.resume = qca81xx_phy_resume,
	.set_wol = qca81xx_phy_set_wol,
	.get_wol = qca81xx_phy_get_wol,
	.get_tunable = qca81xx_phy_get_tunable,
	.set_tunable = qca81xx_phy_set_tunable,
	.get_sset_count = qca81xx_phy_get_sset_count,
	.get_strings = qca81xx_phy_get_strings,
	.get_stats = qca81xx_phy_get_stats,
	.cable_test_start = qca81xx_phy_cable_test_start,
	.cable_test_get_status = qca81xx_phy_cable_test_get_status,
	.led_brightness_set = qca81xx_led_brightness_set,
	.led_blink_set = qca81xx_led_blink_set,
	.led_hw_is_supported = qca81xx_led_hw_is_supported,
	.led_hw_control_set = qca81xx_led_hw_control_set,
	.led_hw_control_get = qca81xx_led_hw_control_get,
	.led_polarity_set = qca81xx_led_polarity_set,
	/* to fix the reboot issue of laguna SFP */
	.mdiodrv.driver.shutdown = qca81xx_phy_shutdown,
},
{
	PHY_ID_MATCH_EXACT(QCE1204_PHY),
	.name = "Qualcomm QCE1204",
	.flags = PHY_POLL_CABLE_TEST,
	.probe = qce1204_phy_probe,
	.remove = qce1204_phy_remove,
	.config_init = qce1204_phy_config_init,
	.config_aneg = qce1204_phy_config_aneg,
	.read_status = qce1204_phy_read_status,
	.soft_reset = qce1204_phy_soft_reset,
	.suspend = genphy_c45_pma_suspend,
	.resume = genphy_c45_pma_resume,
	.config_intr = qca81xx_phy_config_intr,
	.handle_interrupt = qca81xx_phy_handle_interrupt,
	.set_wol = qca81xx_phy_set_wol,
	.get_wol = qca81xx_phy_get_wol,
	.get_tunable = qca81xx_phy_get_tunable,
	.set_tunable = qca81xx_phy_set_tunable,
	.get_sset_count = qca81xx_phy_gmii_get_sset_count,
	.get_strings = qca81xx_phy_gmii_get_strings,
	.get_stats = qca81xx_phy_gmii_get_stats,
	.led_brightness_set = qca81xx_led_brightness_set,
	.led_blink_set = qca81xx_led_blink_set,
	.led_hw_is_supported = qca81xx_led_hw_is_supported,
	.led_hw_control_set = qca81xx_led_hw_control_set,
	.led_hw_control_get = qca81xx_led_hw_control_get,
	.led_polarity_set = qca81xx_led_polarity_set,
},
{
	PHY_ID_MATCH_EXACT(IPQ52XX_PHY),
	.name = "Qualcomm IPQ52XX internal PHY",
	.flags = PHY_IS_INTERNAL | PHY_POLL_CABLE_TEST,
	.config_aneg = qce1204_phy_config_aneg,
	.read_status = ipq52xx_phy_read_status,
	.soft_reset = qce1204_phy_soft_reset,
	.suspend = genphy_c45_pma_suspend,
	.resume = genphy_c45_pma_resume,
	.config_intr = qca81xx_phy_config_intr,
	.handle_interrupt = qca81xx_phy_handle_interrupt,
	.set_wol = qca81xx_phy_set_wol,
	.get_wol = qca81xx_phy_get_wol,
	.get_tunable = qca81xx_phy_get_tunable,
	.set_tunable = qca81xx_phy_set_tunable,
	.get_sset_count = qca81xx_phy_gmii_get_sset_count,
	.get_strings = qca81xx_phy_gmii_get_strings,
	.get_stats = qca81xx_phy_gmii_get_stats,
	.led_brightness_set = qca81xx_led_brightness_set,
	.led_blink_set = qca81xx_led_blink_set,
	.led_hw_is_supported = qca81xx_led_hw_is_supported,
	.led_hw_control_set = qca81xx_led_hw_control_set,
	.led_hw_control_get = qca81xx_led_hw_control_get,
	.led_polarity_set = qca81xx_led_polarity_set,
	.config_init = ipq52xx_phy_config_init,
	.probe = ipq52xx_phy_probe,
	.remove = ipq52xx_phy_remove,
},
};
module_phy_driver(qcom_phy_driver);

static struct mdio_device_id __maybe_unused qcom_phy_tbl[] = {
	{ PHY_ID_MATCH_EXACT(QCA8111_PHY) },
	{ PHY_ID_MATCH_EXACT(QCE1204_PHY) },
	{ PHY_ID_MATCH_EXACT(IPQ52XX_PHY) },
	{ }
};
MODULE_DEVICE_TABLE(mdio, qcom_phy_tbl);

MODULE_DESCRIPTION("QCOM Linux PHY Driver");
MODULE_LICENSE("Dual BSD/GPL");
