/*
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <command.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/bitfield.h>
#include <linux/ethtool.h>
#include <phy.h>
#include <miiphy.h>
#include <i2c.h>

#include <linux/compat.h>
#include <linux/ethtool.h>

#ifndef SPEED_5000
#define SPEED_5000 5000
#endif

#define QCA_81XX_PHY					0x004dd1c0
#define TO_QCA81XX_PHY_SOC_ADDR(addr, reg)		((BIT(30) | (reg)) | ((addr) << 24))

/*qcom SFP PHY device addr is 0x1d and PHY addr is 0x4c*/
#define TO_QCOM_SFP_PHY_ADDR(phy_id)		((phy_id) + 0x2f)

/* below two registers are used to access PHY */
/* DEBUG registers indirectly */
#define QCA81XX_DEBUG_ADDR				0x1d
#define QCA81XX_DEBUG_DATA				0x1e
#define QCA81XX_DEBUG_ANA_EDAC_CAP0_CTRL		0x4d80
#define QCA81XX_DEBUG_ANA_EDAC_CAP0_VAL			0x2528
#define QCA81XX_DEBUG_ANA_EDAC_CAP1_CTRL		0x4e80
#define QCA81XX_DEBUG_ANA_EDAC_CAP1_VAL			0x2825
#define QCA81XX_DEBUG_ANA_AFE_DAC8_DP			0x2f80
#define QCA81XX_DEBUG_ANA_EDAC_RES_VAL0			0x5868
#define QCA81XX_DEBUG_ANA_AFE_DAC9_DP			0x3080
#define QCA81XX_DEBUG_ANA_EDAC_RES_VAL1			0x6858
#define QCA81XX_DEBUG_ANA_TX_POWER_CTRL			0xbd80
#define QCA81XX_DEBUG_ANA_TX_POWER_MASK			GENMASK(12, 0)
#define QCA81XX_DEBUG_ANA_TX_POWER_REDUCED_1P3		0xdc3
#define QCA81XX_DEBUG_ANA_OPEN_RAMPING_CRTL0		0x9180
#define QCA81XX_DEBUG_ANA_OPEN_RAMPING_EN		BIT(7)

#define PHY_INVALID_DATA				0xffff
#define QCAPHY_SPEC_STATUS                              17
#define QCA81XX_SPEC_CONTROL				0x10
#define QCA81XX_AUTO_SOFT_RESET_EN			0x8

#define QCAPHY_STATUS_LINK_PASS                         0x0400
#define QCAPHY_STATUS_SPEED_MASK			0x380
#define QCAPHY_STATUS_SPEED_10000MBS                    0x180
#define QCAPHY_STATUS_SPEED_5000MBS                     0x280
#define QCAPHY_STATUS_SPEED_2500MBS                     0x200
#define QCAPHY_STATUS_SPEED_1000MBS                     0x100
#define QCAPHY_STATUS_SPEED_100MBS                      0x80
#define QCAPHY_STATUS_SPEED_10MBS                       0x0
#define QCAPHY_STATUS_FULL_DUPLEX                       0x2000

/*analog register*/
#define QCA81XX_ANA_DEBUG_AFE_DAC8_DP			0x2f80
#define QCA81XX_ANA_DEBUG_AFE_DAC9_DP			0x3080

/*PHY MMD3 register*/
#define QCA81XX_PHY_MMD3_10G_FRAME_CHECK_CTRL           0xa110
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL2               0x8073
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL3               0x8074
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL4               0x8075
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL5               0x8076
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL6               0x8077
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL7               0x8078
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL9               0x807a
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL13              0x807e
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL14              0x807f

/*PHY MMD3 register fields*/
#define QCA81XX_PHY_MMD3_10G_FRAME_CHECK_EN             0x80
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL2_VAL           0xb03f
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL3_VAL           0xc040
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL4_VAL           0xa060
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL5_VAL           0xc040
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL6_VAL           0xa060
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL7_VAL           0xae50
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL9_VAL           0xc060
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL13_VAL          0xb060
#define QCA81XX_PHY_MMD3_CDT_THRESH_CTRL14_VAL          0xb8b0

/*PHY MMD31 register*/
#define QCA81XX_PHY_FIFO_CONTROL                        0x19
#define QCA81XX_PHY_FIFO_RESET                          0x3

/*UNIPHY MII registers*/
#define QCA81XX_PCS_PLL_POWER_ON_AND_RESET		0
#define QCA81XX_PCS_MII_DIG_CTRL			0x8000

/*UNIPHY MMD1 registers*/
#define QCA81XX_PCS_MMD1_MODE_CTRL			0x11b
#define QCA81XX_PCS_MMD1_CDA_CONTROL1			0x20

/*UNIPHY MMD3 registers*/
#define QCA81XX_PCS_MMD3_PCS_CTRL2			0x7
#define QCA81XX_PCS_MMD3_DIG_CTRL1			0x8000
#define QCA81XX_PCS_MMD3_EEE_MODE_CTRL		0x8006
#define QCA81XX_PCS_MMD3_EEE_TX_TIMER			0x8008
#define QCA81XX_PCS_MMD3_EEE_RX_TIMER			0x8009
#define QCA81XX_PCS_MMD3_EEE_MODE_CTRL1		0x800b
#define QCA81XX_PCS_MMD3_AN_LP_BASE_ABL2		0x14

/*MMD7 (AN) registers*/
#define QCA81XX_AN_SPEED_CTRL			0x20
#define QCA81XX_AN_SPEED_CTRL_1G		0x63

/*UNIPHY MMD31 register*/
#define QCA81XX_PCS_MMD31_MII_CTRL			0
#define QCA81XX_PCS_MMD31_MII_DIG_CTRL		0x8000
#define QCA81XX_PCS_MMD31_MII_AN_INT_MSK		0x8001
#define QCA81XX_PCS_MMD31_MII_ERR_SEL			0x8002

/*UNIPHY MII register field*/
#define QCA81XX_PCS_ANA_SOFT_RESET_MASK		0x40
#define QCA81XX_PCS_ANA_SOFT_RESET			0
#define QCA81XX_PCS_ANA_SOFT_RELEASE			0x40
#define QCA81XX_PCS_MMD3_USXG_FIFO_RESET		0x400

/*UNIPHY MMD1 registers field*/
#define QCA81XX_PCS_MMD1_MODE_MASK			0x1f00
#define QCA81XX_PCS_MMD1_XPCS_MODE			0x1000
#define QCA81XX_PCS_MMD1_CALIBRATION_DONE		0x80
#define QCA81XX_PCS_MMD1_SSCG_ENABLE			0x8
#define QCA81XX_PCS_MMD1_CALIBRATION4			0x78
#define QCA81XX_MMD1_MSE_THRESHOLD_CTRL			0x8020
#define QCA81XX_MMD1_MSE_THRESHOLD_MASK			GENMASK(10, 0)
#define QCA81XX_MMD1_MSE_THRESHOLD_VAL			0x419
#define QCA81XX_MMD1_10G_VGA_GAIN_CTRL			0x8022
#define QCA81XX_MMD1_10G_VGA_GAIN_VAL			0x5dd9

/*UNIPHY MMD3 register field*/
#define QCA81XX_PCS_MMD3_PCS_TYPE_MASK		0xf
#define QCA81XX_PCS_MMD3_PCS_TYPE_10GBASE_R		0
#define QCA81XX_PCS_MMD3_10GBASE_R_PCS_STATUS1	0x20
#define QCA81XX_PCS_MMD3_10GBASE_R_UP			0x1000
#define QCA81XX_PCS_MMD3_USXGMII_EN			0x200
#define QCA81XX_PCS_MMD3_XPCS_SOFT_RESET		0x8000
#define QCA81XX_PCS_MMD3_XPCS_EEE_CAP			0x40
#define QCA81XX_PCS_MMD3_EEE_RES_REGS			0x100
#define QCA81XX_PCS_MMD3_EEE_SIGN_BIT_REGS		0x40
#define QCA81XX_PCS_MMD3_EEE_EN			0x3
#define QCA81XX_PCS_MMD3_EEE_TSL_REGS			0xa
#define QCA81XX_PCS_MMD3_EEE_TLU_REGS			0xc0
#define QCA81XX_PCS_MMD3_EEE_TWL_REGS			0x1600
#define QCA81XX_PCS_MMD3_EEE_100US_REG_REGS		0xc8
#define QCA81XX_PCS_MMD3_EEE_RWR_REG_REGS		0x1c00
#define QCA81XX_PCS_MMD3_EEE_TRANS_LPI_MODE		0x1
#define QCA81XX_PCS_MMD3_EEE_TRANS_RX_LPI_MODE	0x100
#define QCA81XX_MMD3_CABLE_SKEW				0xa102
#define QCA81XX_MMD3_CABLE_TX_SKEW_MASK			GENMASK(7, 0)
#define QCA81XX_MMD3_OPEN_RAMPING_CTRL1			0xa048
#define QCA81XX_MMD3_OPEN_RAMPING_EN_MASK1		0x801
#define QCA81XX_MMD3_OPEN_RAMPING_CTRL2			0xa01f
#define QCA81XX_MMD3_OPEN_RAMPING_EN_MASK2		BIT(7)
#define QCA81XX_MMD3_PB0_TRAIN_DURATION_CTRL		0xa017
#define QCA81XX_MMD3_PB0_DURATION_MASK			GENMASK(5, 4)
#define QCA81XX_MMD3_PB0_DURATION_VAL			0x10

/*UNIPHY MMD31 register field*/
#define QCA81XX_PCS_MMD31_AN_COMPLETE_INT		0x1
#define QCA81XX_PCS_MMD31_MII_4BITS_CTRL		0x0
#define QCA81XX_PCS_MMD31_TX_CONFIG_CTRL		0x8
#define QCA81XX_PCS_MMD31_MII_AN_ENABLE		0x1000
#define QCA81XX_PCS_MMD31_PHY_MODE_CTRL_EN		0x1
#define QCA81XX_PCS_MMD31_MII_AN_COMPLETE_INT		0x1
#define QCA81XX_PCS_MMD31_AN_RESTART			0x200
#define QCA81XX_PCS_MMD31_XPCS_SPEED_MASK		0x2060
#define QCA81XX_PCS_MMD31_XPCS_SPEED_10000		0x2040
#define QCA81XX_PCS_MMD31_XPCS_SPEED_5000		0x2020
#define QCA81XX_PCS_MMD31_XPCS_SPEED_2500		0x20
#define QCA81XX_PCS_MMD31_XPCS_SPEED_1000		0x40
#define QCA81XX_PCS_MMD31_XPCS_SPEED_100		0x2000

/*SOC GCC registers*/
#define GCC_E2S_TX_CMD_RCGR				0x800000
#define GCC_E2S_TX_CFG_RCGR				0x800004
#define GCC_E2S_TX_DIV_CDIVR				0x800008
#define GCC_E2S_SRDS_CH0_RX_CBCR			0x800010
#define GCC_E2S_GEPHY_TX_CBCR				0x800014
#define GCC_E2S_RX_CMD_RCGR				0x800018
#define GCC_E2S_RX_CFG_RCGR				0x80001c
#define GCC_E2S_RX_DIV_CDIVR				0x800020
#define GCC_E2S_SRDS_CH0_TX_CBCR			0x800028
#define GCC_E2S_GEPHY_RX_CBCR				0x80002c
#define GCC_AHB_CMD_RCGR				0x80003c
#define GCC_AHB_CFG_RCGR				0x800040
#define GCC_SRDS_SYS_CBCR				0x80007c
#define GCC_GEPHY_SYS_CBCR				0x800080
#define GCC_SEC_CTRL_CMD_RCGR				0x800088
#define GCC_SEC_CTRL_CFG_RCGR				0x80008c
#define GCC_SERDES_CTL					0x80030C

/*SOC GCC registers field*/
#define GCC_CLK_ENABLE					0x1
#define GCC_CLK_ARES					0x4
#define XPCS_PWR_ARES					0x1
#define GCC_E2S_SRC_MASK				GENMASK(10, 8)
#define GCC_E2S_SRC0_REF_50MCLK				0
#define GCC_E2S_SRC1_EPHY_TXCLK				1
#define GCC_E2S_SRC2_EPHY_RXCLK				2
#define GCC_E2S_SRC3_SRDS_TXCLK				3
#define GCC_E2S_SRC4_SRDS_RXCLK				4

#define SRC_DIV_MASK					GENMASK(4, 0)
#define CLK_DIV_MASK					GENMASK(3, 0)
#define CLK_CMD_UPDATE					BIT(0)

/*SOC SEC_TCSR registers*/
#define EPHY_CFG					0x90F018
#define EPHY_LDO_CTRL					BIT(20)

/*SOC TLMM registers*/
#define TLMM_BASE		0x400000
#define TLMM_GPIO_OFFSET	0x1000
#define TO_TLMM_CFG_REG(pin)	(TLMM_BASE + TLMM_GPIO_OFFSET * pin)
#define TLMM_FUNC_MASK		GENMASK(5, 2)

enum {
	GPIO0_WOL_INT = 0,
	GPIO1_PHY_INT,
	GPIO2_LED0,
	GPIO3_LED1,
	GPIO4_LED3,
	GPIO5_PPS_IN = 5,
	GPIO6_TOD_IN = 6,
	GPIO7_REFCLK_IN = 7,
	GPIO10_PPS_OUT = 10,
	GPIO11_TOD_OUT = 11,
	GPIO12_CLK125_TDI = 12,
	GPIO_MAX
};

enum qca81xx_phy_ext_addr {
	QCA81XX_PCS_ADDR_OFFSET = 1,
	QCA81XX_SOC_ADDR_OFFSET = 2,
};

struct qca_81xx_device {
	struct udevice *i2c_bus;
	int bus_phandle;
};

static int qca81xx_phy_debug_write(struct phy_device *phydev,
				   unsigned int reg, u16 val)
{
	int ret;

	ret = phy_write(phydev, MDIO_MMD_VEND2, QCA81XX_DEBUG_ADDR, reg);
	if (ret < 0)
		return ret;

	ret = phy_write(phydev, MDIO_MMD_VEND2,	QCA81XX_DEBUG_DATA, val);
	return ret;
}

int qca81xx_phy_debug_modify(struct phy_device *phydev,
			     int reg, u16 clear, u16 set)
{
	int ret;

	ret = phy_write(phydev, MDIO_MMD_VEND2, QCA81XX_DEBUG_ADDR, reg);
	if (ret < 0)
		return ret;

	ret = phy_modify(phydev, MDIO_MMD_VEND2, QCA81XX_DEBUG_DATA,
			 clear, set);
	return ret;
}


static void qca81xx_split_addr(u32 regaddr, u16 *reg_low, u16 *reg_mid,
			       u16 *reg_high)
{
	*reg_low = (regaddr & 0xc) << 1;
	*reg_mid = regaddr >> 4 & 0xffff;
	*reg_high = ((regaddr >> 20 & 0xf) << 1) | BIT(0);
}

static u32 qca81xx_soc_i2c_read(struct phy_device *phydev, u32 reg)
{
	struct qca_81xx_device *dev = phydev->priv;
	struct dm_i2c_ops *ops = i2c_get_ops(dev->i2c_bus);
	struct i2c_msg msgs[2];
	int bus_addr, ret, addr;
	u8 data[4] = { 0 };
	u8 tx[5] = {0xa0, (reg >> 24) & 0xff, (reg >> 16) & 0xff,
		(reg >> 8) & 0xff, reg & 0xff};

	addr = FIELD_GET(GENMASK(28, 24), reg);
	bus_addr = TO_QCOM_SFP_PHY_ADDR(addr);
	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(tx);
	msgs[0].buf = tx;
	msgs[1].addr = bus_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = sizeof(data);
	msgs[1].buf = data;

	ret =  ops->xfer(dev->i2c_bus, msgs, ARRAY_SIZE(msgs));
	if (ret)
		return ret;

	return ((data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]);
}

static u32 qca81xx_soc_mdio_read(struct phy_device *phydev, u32 reg)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	struct phy_device local_phydev;

	qca81xx_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = FIELD_GET(GENMASK(28, 24), reg);

	/*write ahb address bit4~bit23*/
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(100);
	/*write ahb address bit0~bit3 and read low 16bit data*/
	lo = phy_read(&local_phydev, MDIO_DEVAD_NONE, reg_low);
	/*write ahb address bit0~bit3 and read high 16 bit data*/
	hi = phy_read(&local_phydev, MDIO_DEVAD_NONE, (reg_low + 4));

	return (hi << 16) | lo;
}

static u32 qca81xx_soc_read(struct phy_device *phydev, u32 reg)
{
	struct qca_81xx_device *dev = phydev->priv;

	if (dev->i2c_bus)
		return qca81xx_soc_i2c_read(phydev, reg);
	else
		return qca81xx_soc_mdio_read(phydev, reg);
}

static void qca81xx_soc_i2c_write(struct phy_device *phydev, u32 reg, u32 val)
{
	struct qca_81xx_device *dev = phydev->priv;
	struct dm_i2c_ops *ops = i2c_get_ops(dev->i2c_bus);
	struct i2c_msg msgs[2];
	int bus_addr, addr;
	u8 tx[5] = {0x20, (reg >> 24) & 0xff, (reg >> 16) & 0xff,
		(reg >> 8) & 0xff, reg & 0xff};
	u8 tx1[5] = {0, (val >> 24) & 0xff, (val >> 16) & 0xff,
		(val >> 8) & 0xff, val & 0xff};

	addr = FIELD_GET(GENMASK(28, 24), reg);
	bus_addr = TO_QCOM_SFP_PHY_ADDR(addr);
	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = sizeof(tx);
	msgs[0].buf = tx;
	msgs[1].addr = bus_addr;
	msgs[1].flags = 0;
	msgs[1].len = sizeof(tx1);
	msgs[1].buf = tx1;

	ops->xfer(dev->i2c_bus, msgs, ARRAY_SIZE(msgs));
}

static void qca81xx_soc_mdio_write(struct phy_device *phydev, u32 reg, u32 val)
{
	u16 reg_low, reg_mid, reg_high;
	u16 lo, hi;
	struct phy_device local_phydev;

	qca81xx_split_addr(reg, &reg_low, &reg_mid, &reg_high);

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = FIELD_GET(GENMASK(28, 24), reg);

	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	/*write ahb address bit4~bit23*/
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_high & 0x1f, reg_mid);
	udelay(100);
	/*write ahb address bit0~bit3 and write low 16 bit data*/
	phy_write(&local_phydev, MDIO_DEVAD_NONE, reg_low, lo);
	/*write ahb address bit0~bit3 and write high 16 bit data*/
	phy_write(&local_phydev, MDIO_DEVAD_NONE, (reg_low + 4), hi);
}

static void qca81xx_soc_write(struct phy_device *phydev, u32 reg, u32 val)
{
	struct qca_81xx_device *dev = phydev->priv;

	if (dev->i2c_bus)
		return qca81xx_soc_i2c_write(phydev, reg, val);
	else
		return qca81xx_soc_mdio_write(phydev, reg, val);
}

static int qca81xx_soc_modify(struct phy_device *phydev, u32 reg,
			      u32 mask, u32 set)
{
	u32 val;

	val = qca81xx_soc_read(phydev, TO_QCA81XX_PHY_SOC_ADDR((phydev->addr +
			QCA81XX_SOC_ADDR_OFFSET), reg));
	val = (val & ~mask) | set;
	qca81xx_soc_write(phydev, TO_QCA81XX_PHY_SOC_ADDR((phydev->addr +
			QCA81XX_SOC_ADDR_OFFSET), reg), val);

	/*debug log*/
	debug("soc phy_addr:0x%x, reg:0x%x, reg_value:0x%x\n", phydev->addr +
			QCA81XX_SOC_ADDR_OFFSET,
			reg, qca81xx_soc_read(phydev, TO_QCA81XX_PHY_SOC_ADDR((phydev->addr +
					      QCA81XX_SOC_ADDR_OFFSET), reg)));
	return 0;
}

static int qca81xx_pcs_read_mmd(struct phy_device *phydev, int devad,
				u32 regnum)
{
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phydev->addr + QCA81XX_PCS_ADDR_OFFSET;

	return phy_read(&local_phydev, devad, regnum);
}

static int qca81xx_pcs_write_mmd(struct phy_device *phydev, int devad,
		u32 regnum, u16 set)
{
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phydev->addr + QCA81XX_PCS_ADDR_OFFSET;

	return phy_write(&local_phydev, devad, regnum, set);
}

static int qca81xx_pcs_modify_mmd(struct phy_device *phydev, int devad,
				  u32 regnum, u16 mask, u16 set)
{
	int new, ret;
	struct phy_device local_phydev;

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));
	local_phydev.addr = phydev->addr + QCA81XX_PCS_ADDR_OFFSET;

	ret = phy_read(&local_phydev, devad, regnum);
	new = (ret & ~mask) | set;
	return phy_write(&local_phydev, devad, regnum, new);
}

static void qca81xx_pcs_print_reg(struct phy_device *phydev, int devad,
				  u32 regnum)
{
	u16 phy_data = qca81xx_pcs_read_mmd(phydev, devad, regnum);

	debug("uniphy_addr:0x%x, mmd_num:%d, mmd_reg:0x%x, reg_value:0x%x\n",
	      phydev->addr + QCA81XX_PCS_ADDR_OFFSET,
			devad, regnum, phy_data);
}

static int qca81xx_pcs_txclk_en_set(struct phy_device *phydev, bool enable)
{
	return qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_TX_CBCR,
			GCC_CLK_ENABLE,	enable ? GCC_CLK_ENABLE : 0);
}

static int qca81xx_pcs_rxclk_en_set(struct phy_device *phydev, bool enable)
{
	return qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_RX_CBCR,
			GCC_CLK_ENABLE,	enable ? GCC_CLK_ENABLE : 0);
}

static int qca81xx_pcs_clk_en_set(struct phy_device *phydev, bool enable)
{
	int ret;

	ret = qca81xx_pcs_txclk_en_set(phydev, enable);
	if (ret < 0)
		return ret;

	return qca81xx_pcs_rxclk_en_set(phydev, enable);
}

static int qca81xx_pcs_clk_reset_update(struct phy_device *phydev, bool assert)
{
	int ret;

	ret = qca81xx_soc_modify(phydev, GCC_E2S_SRDS_CH0_RX_CBCR, GCC_CLK_ARES,
				 assert ? GCC_CLK_ARES : 0);
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

static int qca81xx_pcs_sysclk_en_set(struct phy_device *phydev, bool enable)
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

static int qca81xx_xpcs_clk_reset_update(struct phy_device *phydev, bool enable)
{
	return qca81xx_soc_modify(phydev, GCC_SERDES_CTL, XPCS_PWR_ARES,
			enable ? XPCS_PWR_ARES : 0);
}

static int qca81xx_phy_clk_en_set(struct phy_device *phydev, bool enable)
{
	int ret;

	ret = qca81xx_soc_modify(phydev, GCC_E2S_GEPHY_TX_CBCR, GCC_CLK_ENABLE,
				 enable ? GCC_CLK_ENABLE : 0);
	if (ret < 0)
		return ret;

	return qca81xx_soc_modify(phydev, GCC_E2S_GEPHY_RX_CBCR, GCC_CLK_ENABLE,
			enable ? GCC_CLK_ENABLE : 0);
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

static int qca81xx_phy_clk_reset_update(struct phy_device *phydev, bool assert)
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

static int qca81xx_phy_sysclk_reset_update(struct phy_device *phydev,
					   bool assert)
{
	return qca81xx_soc_modify(phydev, GCC_GEPHY_SYS_CBCR, GCC_CLK_ARES,
			assert ? GCC_CLK_ARES : 0);
}

static int qca81xx_phy_sysclk_reset(struct phy_device *phydev)
{
	int ret;

	ret = qca81xx_phy_sysclk_reset_update(phydev, true);
	if (ret < 0)
		return ret;
	mdelay(1);

	return qca81xx_phy_sysclk_reset_update(phydev, false);
}

static int qca81xx_phy_speed_clk_set(struct phy_device *phydev)
{
	int ret, div0 = 0, div1 = 0;

	switch (phydev->speed) {
	case SPEED_100:
		/*312.5 divided by 2.5*5**/
		div0 = 4;
		div1 = 4;
		break;
	case SPEED_1000:
		/*312.5 divided by 2.5*1**/
		div0 = 4;
		div1 = 0;
		break;
	case SPEED_2500:
		/*312.5 divided by 1*4**/
		div0 = 1;
		div1 = 3;
		break;
	case SPEED_5000:
		/*312.5 divided by 1*2**/
		div0 = 1;
		div1 = 1;
		break;
	case SPEED_10000:
		/*312.5 divided by 1*1*/
		div0 = 1;
		div1 = 0;
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
				 CLK_CMD_UPDATE,	CLK_CMD_UPDATE);
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

	return qca81xx_soc_modify(phydev, GCC_E2S_RX_CMD_RCGR, CLK_CMD_UPDATE,
			CLK_CMD_UPDATE);
}

static int qca_81xx_phy_fifo_reset(struct phy_device *phydev, bool enable)
{
	return qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
			QCA81XX_PHY_FIFO_CONTROL, QCA81XX_PHY_FIFO_RESET,
			(enable ? 0 : QCA81XX_PHY_FIFO_RESET));
}

static int qca_81xx_phy_speed_fixup(struct phy_device *phydev)
{
	int count, ret = 0;
	u16 phy_data = 0;
	bool port_clk_en = false;

	debug("wait autoneg complete interrupt and clear it\n");
	count = 0;
	ret = -ETIMEDOUT;
	do {
		phy_data = qca81xx_pcs_read_mmd(phydev, MDIO_MMD_VEND2,
						QCA81XX_PCS_MMD31_MII_ERR_SEL);
		if (phy_data == PHY_INVALID_DATA ||
		    phy_data & QCA81XX_PCS_MMD31_MII_AN_COMPLETE_INT) {
			ret = 0;
			break;
		}

		mdelay(1);
		count++;
	} while (count < 100);

	if (ret < 0) {
		printf("autoneg complete timeout!\n");
		return ret;
	}

	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
				     QCA81XX_PCS_MMD31_MII_ERR_SEL,
			QCA81XX_PCS_MMD31_MII_AN_COMPLETE_INT, 0);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
			      QCA81XX_PCS_MMD31_MII_ERR_SEL);
	mdelay(10);

	if (phydev->link) {
		debug("set gmii,xgmii clock to uniphy and ethphy\n");
		ret = qca81xx_phy_speed_clk_set(phydev);
		if (ret < 0)
			return ret;

		/*avoid garbe data transmit out, need to assert ephy tx clock*/
		qca81xx_phy_txclk_reset_update(phydev, true);
		port_clk_en = true;
	}

	debug("GMII/XGMII clock and ETHPHY GMII clock enable/disable\n");
	ret = qca81xx_pcs_clk_en_set(phydev, port_clk_en);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_clk_en_set(phydev, port_clk_en);
	if (ret < 0)
		return ret;
	mdelay(1);

	debug("UNIPHY GMII/XGMII interface and ETHPHY GMII interface reset and release\n");
	ret = qca81xx_pcs_clk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_clk_reset(phydev);
	if (ret < 0)
		return ret;

	debug("do function adptreset\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MII_DIG_CTRL,
			QCA81XX_PCS_MMD3_USXG_FIFO_RESET,
			QCA81XX_PCS_MMD3_USXG_FIFO_RESET);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
			      QCA81XX_PCS_MMD31_MII_ERR_SEL);

	/*do ethphy function reset*/
	debug("do ethphy function reset\n");
	ret = qca_81xx_phy_fifo_reset(phydev, true);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
			      QCA81XX_PHY_FIFO_CONTROL);
	mdelay(1);

	if (phydev->link) {
		ret = qca_81xx_phy_fifo_reset(phydev, false);
		if (ret)
			goto fail;
		qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
				      QCA81XX_PHY_FIFO_CONTROL);
		mdelay(1);
	}

fail:
	if (ret)
		printf("%s %d failed ret: %d\n", __func__, __LINE__, ret);
	return ret;
}

static int qca81xx_pcs_eee_enable(struct phy_device *phydev)
{
	int ret = 0;

	/*Configure the EEE related timer*/
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_EEE_MODE_CTRL, 0x0f40,
			QCA81XX_PCS_MMD3_EEE_RES_REGS |
			QCA81XX_PCS_MMD3_EEE_SIGN_BIT_REGS);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_EEE_MODE_CTRL);

	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_EEE_TX_TIMER, 0x1fff,
			QCA81XX_PCS_MMD3_EEE_TSL_REGS |
			QCA81XX_PCS_MMD3_EEE_TLU_REGS |
			QCA81XX_PCS_MMD3_EEE_TWL_REGS);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_EEE_TX_TIMER);

	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_EEE_RX_TIMER, 0x1fff,
			QCA81XX_PCS_MMD3_EEE_100US_REG_REGS |
			QCA81XX_PCS_MMD3_EEE_RWR_REG_REGS);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_EEE_RX_TIMER);

	/*enable TRN_LPI*/
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_EEE_MODE_CTRL1, 0x101,
			QCA81XX_PCS_MMD3_EEE_TRANS_LPI_MODE |
			QCA81XX_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_EEE_MODE_CTRL1);

	/*enable TX/RX LPI pattern*/
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_EEE_MODE_CTRL, 0x3,
			QCA81XX_PCS_MMD3_EEE_EN);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_EEE_MODE_CTRL);

fail:
	if (ret)
		printf("%s %d failed ret: %d\n", __func__, __LINE__, ret);
	return ret;
}

static int qca81xx_phy_gcc_pre_init(struct phy_device *phydev)
{
	int ret;

	/*gephy system reset and release*/
	ret = qca81xx_phy_sysclk_reset(phydev);
	if (ret < 0)
		return ret;
	/*enable efuse loading into analog circuit*/
	ret = qca81xx_soc_modify(phydev, EPHY_CFG, EPHY_LDO_CTRL, 0);
	mdelay(1);

	/* Set AHB clk to 50MHz */
	ret = qca81xx_soc_modify(phydev, GCC_AHB_CFG_RCGR,
			0xFFFFFFFF, 0);
	if (ret < 0)
		return ret;

	ret = qca81xx_soc_modify(phydev, GCC_AHB_CMD_RCGR,
			0x3, 0x3);
	if (ret < 0)
		return ret;

	return ret;
}

static int qca81xx_phy_gcc_post_init(struct phy_device *phydev)
{
	int ret;

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

static int qca81xx_phy_soft_reset(struct phy_device *phydev)
{
	int ret;

	/* enable auto soft reset when power on */
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
				     QCA81XX_SPEC_CONTROL,
			QCA81XX_AUTO_SOFT_RESET_EN,
			QCA81XX_AUTO_SOFT_RESET_EN);
	if (ret < 0)
		return ret;

	phy_set_bits_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1, MDIO_CTRL1_LPOWER);
	mdelay(10);
	phy_clear_bits_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1, MDIO_CTRL1_LPOWER);
	mdelay(1);

	/* disable auto soft reset when power on */
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
				     QCA81XX_SPEC_CONTROL,
			QCA81XX_AUTO_SOFT_RESET_EN, 0);

	return ret;
}

static int qca_81xx_phy_usxgmii_init(struct phy_device *phydev)
{
	u16 count, phy_data = 0;
	int ret = 0;

	debug("disable uniphy GMII/XGMII clock and ethphy GMII/XGMII clock\n");
	ret = qca81xx_phy_clk_en_set(phydev, false);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_clk_en_set(phydev, false);
	if (ret < 0)
		return ret;

	debug("enable uniphy system clock\n");
	ret = qca81xx_pcs_sysclk_en_set(phydev, true);
	if (ret < 0)
		return ret;

	debug("reset and release uniphy_phy_sys_cbcr_clk\n");
	ret = qca81xx_pcs_sysclk_reset(phydev);
	if (ret < 0)
		return ret;

	debug("reset xpcs\n");
	ret = qca81xx_xpcs_clk_reset_update(phydev, true);
	if (ret < 0)
		return ret;

	debug("uniphy usxgmii Mode configuration\n");
	debug("select xpcs mode\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCA81XX_PCS_MMD1_MODE_CTRL,
			QCA81XX_PCS_MMD1_MODE_MASK,
			QCA81XX_PCS_MMD1_XPCS_MODE);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PMAPMD,
			      QCA81XX_PCS_MMD1_MODE_CTRL);

	debug("reset and release uniphy GMII/XGMII and ethphy GMII/XGMII\n");
	ret = qca81xx_pcs_clk_reset(phydev);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_clk_reset(phydev);
	if (ret < 0)
		return ret;

	debug("ana sw reset and release\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_DEVAD_NONE,
				     QCA81XX_PCS_PLL_POWER_ON_AND_RESET,
			QCA81XX_PCS_ANA_SOFT_RESET_MASK,
			QCA81XX_PCS_ANA_SOFT_RESET);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_DEVAD_NONE,
			      QCA81XX_PCS_PLL_POWER_ON_AND_RESET);
	mdelay(1);
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_DEVAD_NONE,
				     QCA81XX_PCS_PLL_POWER_ON_AND_RESET,
			QCA81XX_PCS_ANA_SOFT_RESET_MASK,
			QCA81XX_PCS_ANA_SOFT_RELEASE);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_DEVAD_NONE,
			      QCA81XX_PCS_PLL_POWER_ON_AND_RESET);

	count = 0;
	ret = -ETIMEDOUT;
	debug("Wait calibration done\n");
	do {
		phy_data = qca81xx_pcs_read_mmd(phydev, MDIO_MMD_PMAPMD,
						QCA81XX_PCS_MMD1_CALIBRATION4);
		if (phy_data == PHY_INVALID_DATA ||
		    phy_data & QCA81XX_PCS_MMD1_CALIBRATION_DONE) {
			ret = 0;
			break;
		}

		mdelay(1);
		count++;
	} while (count < 100);

	if (ret < 0)
		printf("uniphy calibration timed out!\n");
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PMAPMD,
			      QCA81XX_PCS_MMD1_CALIBRATION4);

	debug("enable uniphy sscg(pread Spectrum Clock Generator)\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
				     QCA81XX_PCS_MMD1_CDA_CONTROL1,
			QCA81XX_PCS_MMD1_SSCG_ENABLE,
			QCA81XX_PCS_MMD1_SSCG_ENABLE);
	if (ret)
		goto fail;
	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PMAPMD,
			      QCA81XX_PCS_MMD1_CDA_CONTROL1);

	debug("Enable uniphy_phy mix_phy_tx_clk\n");
	ret = qca81xx_pcs_txclk_en_set(phydev, true);
	if (ret < 0)
		return ret;

	debug("release XPCS\n");
	ret = qca81xx_xpcs_clk_reset_update(phydev, false);
	if (ret < 0)
		return ret;

	debug("ethphy software reset\n");
	ret = qca81xx_phy_soft_reset(phydev);
	if (ret)
		goto fail;

	debug("Set BaseR mode\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_PCS_CTRL2,
			QCA81XX_PCS_MMD3_PCS_TYPE_MASK,
			QCA81XX_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_PCS_CTRL2);

	count = 0;
	ret = -ETIMEDOUT;
	debug("wait 10G base_r link up\n");
	do {
		phy_data = qca81xx_pcs_read_mmd(phydev, MDIO_MMD_PCS,
						QCA81XX_PCS_MMD3_10GBASE_R_PCS_STATUS1);
		if (phy_data == PHY_INVALID_DATA ||
		    phy_data & QCA81XX_PCS_MMD3_10GBASE_R_UP) {
			ret = 0;
			break;
		}

		mdelay(1);
		count++;
	} while (count < 500);

	if (ret < 0)
		printf("10G base_r link up timed out\n");

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_10GBASE_R_PCS_STATUS1);

	debug("enable UQSXGMII mode\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_DIG_CTRL1,
			QCA81XX_PCS_MMD3_USXGMII_EN,
			QCA81XX_PCS_MMD3_USXGMII_EN);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_DIG_CTRL1);

	debug("xpcs software reset\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
				     QCA81XX_PCS_MMD3_DIG_CTRL1,
			QCA81XX_PCS_MMD3_XPCS_SOFT_RESET,
			QCA81XX_PCS_MMD3_XPCS_SOFT_RESET);
	if (ret)
		goto fail;

	count = 0;
	ret = -ETIMEDOUT;
	do {
		phy_data = qca81xx_pcs_read_mmd(phydev, MDIO_MMD_PCS,
						QCA81XX_PCS_MMD3_DIG_CTRL1);
		if (phy_data == PHY_INVALID_DATA ||
		    !(phy_data & QCA81XX_PCS_MMD3_XPCS_SOFT_RESET)) {
			ret = 0;
			break;
		}

		mdelay(1);
		count++;
	} while (count < 100);

	if (ret < 0)
		printf("xpcs software reset timeout\n");

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_PCS,
			      QCA81XX_PCS_MMD3_DIG_CTRL1);

	debug("enable auto-neg complete interrupt,Mii using mii-4bits,configure as PHY mode\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
				     QCA81XX_PCS_MMD31_MII_AN_INT_MSK, 0x109,
			QCA81XX_PCS_MMD31_AN_COMPLETE_INT |
			QCA81XX_PCS_MMD31_MII_4BITS_CTRL |
			QCA81XX_PCS_MMD31_TX_CONFIG_CTRL);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
			      QCA81XX_PCS_MMD31_MII_AN_INT_MSK);

	debug("Enable SGMII PHY Mode control\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
				     QCA81XX_PCS_MMD31_MII_DIG_CTRL, BIT(0),
			QCA81XX_PCS_MMD31_PHY_MODE_CTRL_EN);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
			      QCA81XX_PCS_MMD31_MII_DIG_CTRL);

	debug("enable autoneg ability\n");
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_VEND2,
				     QCA81XX_PCS_MMD31_MII_CTRL,
			QCA81XX_PCS_MMD31_MII_AN_ENABLE,
			QCA81XX_PCS_MMD31_MII_AN_ENABLE);
	if (ret)
		goto fail;

	qca81xx_pcs_print_reg(phydev, MDIO_MMD_VEND2,
			      QCA81XX_PCS_MMD31_MII_CTRL);

	debug("enable EEE for xpcs\n");
	ret = qca81xx_pcs_eee_enable(phydev);

fail:
	if (ret)
		printf("%s %d failed ret: %d\n", __func__, __LINE__, ret);
	return ret;
}

static int qca_81xx_phy_cdt_thresh_init(struct phy_device *phydev)
{
	int ret = 0;

	ret = qca81xx_phy_debug_write(phydev,
				      QCA81XX_ANA_DEBUG_AFE_DAC8_DP, 0);
	if (ret < 0)
		goto fail;

	ret = qca81xx_phy_debug_write(phydev,
				      QCA81XX_ANA_DEBUG_AFE_DAC9_DP, 0);
	if (ret < 0)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL2,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL2_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL3,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL3_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL4,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL4_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL5,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL5_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL6,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL6_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL7,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL7_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL9,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL9_VAL);
	if (ret)
		goto fail;

	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL13,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL13_VAL);
	if (ret)
		goto fail;

	/* for asic read mmd3 0x808b and got the value, and
	 * program the value+1 to 0x807f threshold
	 */
	ret = phy_write(phydev, MDIO_MMD_PCS,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL14,
			QCA81XX_PHY_MMD3_CDT_THRESH_CTRL14_VAL);
	if (ret)
		goto fail;

fail:
	if (ret)
		printf("%s %d failed ret: %d\n", __func__, __LINE__, ret);
	return ret;
}

static int qca_81xx_probe(struct phy_device *phydev)
{
	struct qca_81xx_device *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	memset(dev, 0, sizeof(*dev));

	phydev->priv = dev;
	return 0;
}

static int qca81xx_phy_ana_capacitance_update(struct phy_device *phydev)
{
	int ret = 0;

	/* update tx power value */
	ret = qca81xx_phy_debug_modify(phydev, QCA81XX_DEBUG_ANA_TX_POWER_CTRL,
		QCA81XX_DEBUG_ANA_TX_POWER_MASK,
		QCA81XX_DEBUG_ANA_TX_POWER_REDUCED_1P3);
	if (ret < 0)
		return ret;
	/* update edac value */
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC8_DP,
		QCA81XX_DEBUG_ANA_EDAC_RES_VAL0);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_debug_write(phydev, QCA81XX_DEBUG_ANA_AFE_DAC9_DP,
		QCA81XX_DEBUG_ANA_EDAC_RES_VAL1);
	if (ret < 0)
		return ret;
	/* update analog capacitance value */
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
		QCA81XX_DEBUG_ANA_OPEN_RAMPING_EN,
		QCA81XX_DEBUG_ANA_OPEN_RAMPING_EN);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_OPEN_RAMPING_CTRL1,
		QCA81XX_MMD3_OPEN_RAMPING_EN_MASK1, 0x1);
	if (ret < 0)
		return ret;
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS,
		QCA81XX_MMD3_OPEN_RAMPING_CTRL2,
		QCA81XX_MMD3_OPEN_RAMPING_EN_MASK2, 0);
	if (ret < 0)
		return ret;
	/* update MSE threshold */
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_MMD1_MSE_THRESHOLD_CTRL, QCA81XX_MMD1_MSE_THRESHOLD_MASK,
		QCA81XX_MMD1_MSE_THRESHOLD_VAL);
	if (ret < 0)
		return ret;
	/* update VGA gain to improve 10G long cable high temperature performance */
	ret = qca81xx_pcs_write_mmd(phydev, MDIO_MMD_PMAPMD,
		QCA81XX_MMD1_10G_VGA_GAIN_CTRL, QCA81XX_MMD1_10G_VGA_GAIN_VAL);
	if (ret < 0)
		return ret;
	/* adjust the cable tx skew to improve 10G link performance */
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_CABLE_SKEW,
		QCA81XX_MMD3_CABLE_TX_SKEW_MASK, 0x30);
	if (ret < 0)
		return ret;
	/* adjust the PBO duration */
	ret = qca81xx_pcs_modify_mmd(phydev, MDIO_MMD_PCS, QCA81XX_MMD3_PB0_TRAIN_DURATION_CTRL,
		QCA81XX_MMD3_PB0_DURATION_MASK, QCA81XX_MMD3_PB0_DURATION_VAL);
	if (ret < 0)
		return ret;
	ret = qca81xx_phy_soft_reset(phydev);

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


static int qca_81xx_config(struct phy_device *phydev)
{
	struct qca_81xx_device *dev = phydev->priv;
	int ret = 0;

	dev->bus_phandle = ofnode_read_u32_default(phydev->node, "i2c-bus", 0);
	if (dev->bus_phandle) {
		ret = uclass_get_device_by_phandle_id(UCLASS_I2C,
						      dev->bus_phandle, &dev->i2c_bus);
		if (ret) {
			printf("%s: failed to get i2c bus, err: %d\n",
			       __func__, ret);
			return ret;
		}
	}

	ret = qca81xx_phy_gcc_pre_init(phydev);
	if (ret < 0)
		goto fail;

	ret = qca_81xx_phy_usxgmii_init(phydev);
	if (ret)
		goto fail;

	ret = qca81xx_phy_gcc_post_init(phydev);
	if (ret < 0)
		goto fail;

	ret = qca81xx_tlmm_init(phydev);
	if (ret < 0)
		goto fail;


	ret = qca_81xx_phy_cdt_thresh_init(phydev);
	if (ret)
		goto fail;

	ret = qca81xx_phy_ana_capacitance_update(phydev);

	/* Force 1G speed if qcom,force-speed is set in DTS */
	if (ofnode_read_bool(phydev->node, "qcom,force-speed") &&
	    ofnode_read_u32_default(phydev->node, "max-speed", -1) == SPEED_1000) {
		ret = phy_write(phydev, MDIO_MMD_AN, QCA81XX_AN_SPEED_CTRL,
				QCA81XX_AN_SPEED_CTRL_1G);
		if (ret)
			goto fail;
	}

fail:
	if (ret)
		printf("%s %d failed ret: %d\n", __func__, __LINE__, ret);
	return ret;
}

static int qca_81xx_startup(struct phy_device *phydev)
{
	u16 phy_data, link, speed;

	phy_data = phy_read(phydev, MDIO_MMD_VEND2, QCAPHY_SPEC_STATUS);
	if (phy_data & QCAPHY_STATUS_LINK_PASS)
		link = 1;
	else
		link = 0;

	switch (phy_data & QCAPHY_STATUS_SPEED_MASK) {
	case QCAPHY_STATUS_SPEED_10000MBS:
		speed = SPEED_10000;
		break;
	case QCAPHY_STATUS_SPEED_5000MBS:
		speed = SPEED_5000;
		break;
	case QCAPHY_STATUS_SPEED_2500MBS:
		speed = SPEED_2500;
		break;
	case QCAPHY_STATUS_SPEED_1000MBS:
		speed = SPEED_1000;
		break;
	case QCAPHY_STATUS_SPEED_100MBS:
		speed = SPEED_100;
		break;
	case QCAPHY_STATUS_SPEED_10MBS:
		speed = SPEED_10;
		break;
	default:
		return -EINVAL;
	}

	if (phy_data & QCAPHY_STATUS_FULL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	if (phydev->link != link || phydev->speed != speed) {
		phydev->link = link;
		phydev->speed = speed;
		return qca_81xx_phy_speed_fixup(phydev);
	}

	return 0;
}

U_BOOT_PHY_DRIVER(qca_81xx_driver) = {
	.name = "QCA 81XX PHY Driver",
	.uid = QCA_81XX_PHY,
	.mask = 0xfffffff0,
	.features = PHY_10G_FEATURES,
	.probe = &qca_81xx_probe,
	.config = &qca_81xx_config,
	.startup = &qca_81xx_startup,
	.shutdown = &genphy_shutdown,
};
