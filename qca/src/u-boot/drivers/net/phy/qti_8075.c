// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <common.h>
#include <command.h>
#include <asm/io.h>
#include <malloc.h>
#include <phy.h>
#include <malloc.h>
#include <miiphy.h>

#include "qti.h"

#define QTI_8085_PHY_V1_0_5P                     0x004DD0B0
#define QTI_8085_PHY_V1_1_5P                     0x004DD0B1
#define QTI_8085_PHY_V1_1_2P                     0x004DD0B2

#define QTI_8085_PHY_CONTROL			0
#define QTI_8085_PHY_STATUS			1
#define QTI_8085_PHY_ID1				2
#define QTI_8085_PHY_ID2				3
#define QTI_8085_AUTONEG_ADVERT			4
#define QTI_8085_LINK_PARTNER_ABILITY		5
#define QTI_8085_AUTONEG_EXPANSION		6
#define QTI_8085_NEXT_PAGE_TRANSMIT		7
#define QTI_8085_LINK_PARTNER_NEXT_PAGE		8
#define QTI_8085_1000BASET_CONTROL		9
#define QTI_8085_1000BASET_STATUS		10
#define QTI_8085_MMD_CTRL_REG			13
#define QTI_8085_MMD_DATA_REG			14
#define QTI_8085_EXTENDED_STATUS			15
#define QTI_8085_PHY_SPEC_CONTROL		16
#define QTI_8085_PHY_SPEC_STATUS			17
#define QTI_8085_PHY_INTR_MASK			18
#define QTI_8085_PHY_INTR_STATUS			19
#define QTI_8085_PHY_CDT_CONTROL			22
#define QTI_8085_PHY_CDT_STATUS			28
#define QTI_8085_DEBUG_PORT_ADDRESS		29
#define QTI_8085_DEBUG_PORT_DATA			30
#define	COMBO_PHY_ID	4
#define PSGMII_ID	5

#define QTI_8085_DEBUG_PHY_HIBERNATION_CTRL	0xb
#define QTI_8085_DEBUG_PHY_POWER_SAVING_CTRL	0x29
#define QTI_8085_PHY_MMD7_ADDR_8023AZ_EEE_CTRL	0x3c
#define QTI_8085_PHY_MMD3_ADDR_REMOTE_LOOPBACK_CTRL	0x805a
#define QTI_8085_PHY_MMD3_WOL_MAGIC_MAC_CTRL1	0x804a
#define QTI_8085_PHY_MMD3_WOL_MAGIC_MAC_CTRL2	0x804b
#define QTI_8085_PHY_MMD3_WOL_MAGIC_MAC_CTRL3	0x804c
#define QTI_8085_PHY_MMD3_WOL_CTRL	0x8012

#define QTI_8085_PSGMII_FIFI_CTRL		0x6e
#define QTI_8085_PSGMII_CALIB_CTRL	0x27
#define QTI_8085_PSGMII_MODE_CTRL	0x6d
#define QTI_8085_PHY_PSGMII_MODE_CTRL_ADJUST_VALUE	0x220c

#define QTI_8085_PHY_MMD7_NUM		7
#define QTI_8085_PHY_MMD3_NUM		3
#define QTI_8085_PHY_MMD1_NUM		1

/* sgmii_status  Register  */
#define QTI_8085_PHY_SGMII_STATUS		0x1a
#define QTI_8085_PHY4_AUTO_SGMII_SELECT	0x40
#define QTI_8085_PHY4_AUTO_COPPER_SELECT	0x20
#define QTI_8085_PHY4_AUTO_BX1000_SELECT	0x10
#define QTI_8085_PHY4_AUTO_FX100_SELECT	0x8

#define QTI_8085_MODE_RESET_REG 			0x0
#define QTI_8085_MODE_CHANAGE_RESET 		0x0
#define QTI_8085_MODE_RESET_DEFAULT_VALUE 	0x5f
#define QTI_8085_PHY_MAX_ADDR_INC		0x4
#define QTI_8085_PHY_PSGMII_ADDR_INC 		0x5

/* Chip Configuration Register  */
#define QTI_8085_PHY_CHIP_CONFIG		0x1f

#define BT_BX_SG_REG_SELECT		BIT_15
#define BT_BX_SG_REG_SELECT_OFFSET	15
#define BT_BX_SG_REG_SELECT_LEN		1
#define QTI_8085_SG_BX_PAGES		0x0
#define QTI_8085_SG_COPPER_PAGES		0x1

#define QTI_8085_PHY_PSGMII_BASET		0x0
#define QTI_8085_PHY_PSGMII_BX1000	0x1
#define QTI_8085_PHY_PSGMII_FX100		0x2
#define QTI_8085_PHY_PSGMII_AMDET		0x3
#define QTI_8085_PHY_SGMII_BASET		0x4

#define QTI_8085_PHY4_PREFER_FIBER	0x400
#define PHY4_PREFER_COPPER		0x0
#define PHY4_PREFER_FIBER		0x1

#define QTI_8085_PHY4_FIBER_MODE_1000BX	0x100
#define AUTO_100FX_FIBER		0x0
#define AUTO_1000BX_FIBER		0x1

#define QTI_8085_PHY_MDIX			0x0020
#define QTI_8085_PHY_MDIX_AUTO		0x0060
#define QTI_8085_PHY_MDIX_STATUS		0x0040

#define MODE_CFG_QUAL			BIT_4
#define MODE_CFG_QUAL_OFFSET		4
#define MODE_CFG_QUAL_LEN		4

#define MODE_CFG			BIT_0
#define MODE_CFG_OFFSET			0
#define MODE_CFG_LEN			4

#define QTI_8085_PHY_MMD3_ADDR_8023AZ_CLD_CTRL		0x8007
#define QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL		0x804e
#define QTI_8085_PHY_MMD3_ADDR_CLD_CTRL5			0x8005
#define QTI_8085_PHY_MMD3_ADDR_CLD_CTRL3			0x8003

#define AZ_TIMER_CTRL_DEFAULT_VALUE	0x3062
#define AZ_CLD_CTRL_DEFAULT_VALUE	0x83f6
#define AZ_TIMER_CTRL_ADJUST_VALUE	0x7062
#define AZ_CLD_CTRL_ADJUST_VALUE	0x8396

  /*debug port */
#define QTI_8085_DEBUG_PORT_RGMII_MODE		18
#define QTI_8085_DEBUG_PORT_RGMII_MODE_EN		0x0008

#define QTI_8085_DEBUG_PORT_RX_DELAY		0
#define QTI_8085_DEBUG_PORT_RX_DELAY_EN		0x8000

#define QTI_8085_DEBUG_PORT_TX_DELAY		5
#define QTI_8085_DEBUG_PORT_TX_DELAY_EN		0x0100

  /* PHY Registers Field */

  /* Control Register fields  offset:0 */
  /* bits 6,13: 10=1000, 01=100, 00=10 */
#define QTI_8085_CTRL_SPEED_MSB		0x0040

  /* Collision test enable */
#define QTI_8085_CTRL_COLL_TEST_ENABLE	0x0080

  /* FDX =1, half duplex =0 */
#define QTI_8085_CTRL_FULL_DUPLEX		0x0100

  /* Restart auto negotiation */
#define QTI_8085_CTRL_RESTART_AUTONEGOTIATION	0x0200

  /* Isolate PHY from MII */
#define QTI_8085_CTRL_ISOLATE		0x0400

  /* Power down */
#define QTI_8085_CTRL_POWER_DOWN		0x0800

  /* Auto Neg Enable */
#define QTI_8085_CTRL_AUTONEGOTIATION_ENABLE	0x1000

  /* Local Loopback Enable */
#define QTI_8085_LOCAL_LOOPBACK_ENABLE		0x4000

  /* bits 6,13: 10=1000, 01=100, 00=10 */
#define QTI_8085_CTRL_SPEED_LSB			0x2000

  /* 0 = normal, 1 = loopback */
#define QTI_8085_CTRL_LOOPBACK			0x4000
#define QTI_8085_CTRL_SOFTWARE_RESET              0x8000

#define QTI_8085_CTRL_SPEED_MASK			0x2040
#define QTI_8085_CTRL_SPEED_1000			0x0040
#define QTI_8085_CTRL_SPEED_100			0x2000
#define QTI_8085_CTRL_SPEED_10			0x0000

#define QTI_8085_RESET_DONE(phy_control)                   \
    (((phy_control) & (QTI_8085_CTRL_SOFTWARE_RESET)) == 0)

  /* Status Register fields offset:1 */
  /* Extended register capabilities */
#define QTI_8085_STATUS_EXTENDED_CAPS		0x0001

  /* Link Status 1 = link */
#define QTI_8085_STATUS_LINK_STATUS_UP		0x0004

  /* Auto Neg Capable */
#define QTI_8085_STATUS_AUTONEG_CAPS		0x0008

  /* Remote Fault Detect */
#define QTI_8085_STATUS_REMOTE_FAULT		0x0010

  /* Auto Neg Complete */
#define QTI_8085_STATUS_AUTO_NEG_DONE		0x0020

  /* Preamble may be suppressed */
#define QTI_8085_STATUS_PREAMBLE_SUPPRESS		0x0040

  /* Ext. status info in Reg 0x0F */
#define QTI_8085_STATUS_EXTENDED_STATUS		0x0100

  /* 100T2 Half Duplex Capable */
#define QTI_8085_STATUS_100T2_HD_CAPS		0x0200

  /* 100T2 Full Duplex Capable */
#define QTI_8085_STATUS_100T2_FD_CAPS		0x0400

  /* 10T   Half Duplex Capable */
#define QTI_8085_STATUS_10T_HD_CAPS		0x0800

  /* 10T   Full Duplex Capable */
#define QTI_8085_STATUS_10T_FD_CAPS		0x1000

  /* 100X  Half Duplex Capable */
#define QTI_8085_STATUS_100X_HD_CAPS		0x2000

  /* 100X  Full Duplex Capable */
#define QTI_8085_STATUS_100X_FD_CAPS		0x4000

  /* 100T4 Capable */
#define QTI_8085_STATUS_100T4_CAPS		0x8000

  /* extended status register capabilities */

#define QTI_8085_STATUS_1000T_HD_CAPS		0x1000

#define QTI_8085_STATUS_1000T_FD_CAPS		0x2000

#define QTI_8085_STATUS_1000X_HD_CAPS		0x4000

#define QTI_8085_STATUS_1000X_FD_CAPS		0x8000

#define QTI_8085_AUTONEG_DONE(ip_phy_status) \
	(((ip_phy_status) & (QTI_8085_STATUS_AUTO_NEG_DONE)) ==  \
	(QTI_8085_STATUS_AUTO_NEG_DONE))

  /* PHY identifier1  offset:2 */
//Organizationally Unique Identifier bits 3:18

  /* PHY identifier2  offset:3 */
//Organizationally Unique Identifier bits 19:24

  /* Auto-Negotiation Advertisement register. offset:4 */
  /* indicates IEEE 802.3 CSMA/CD */
#define QTI_8085_ADVERTISE_SELECTOR_FIELD		0x0001

  /* 10T   Half Duplex Capable */
#define QTI_8085_ADVERTISE_10HALF			0x0020

  /* 10T   Full Duplex Capable */
#define QTI_8085_ADVERTISE_10FULL			0x0040

  /* 100TX Half Duplex Capable */
#define QTI_8085_ADVERTISE_100HALF		0x0080

  /* 100TX Full Duplex Capable */
#define QTI_8085_ADVERTISE_100FULL		0x0100

  /* 100T4 Capable */
#define QTI_8085_ADVERTISE_100T4			0x0200

  /* Pause operation desired */
#define QTI_8085_ADVERTISE_PAUSE			0x0400

  /* Asymmetric Pause Direction bit */
#define QTI_8085_ADVERTISE_ASYM_PAUSE		0x0800

  /* Remote Fault detected */
#define QTI_8085_ADVERTISE_REMOTE_FAULT		0x2000

  /* Next Page ability supported */
#define QTI_8085_ADVERTISE_NEXT_PAGE		0x8000

  /* 100TX Half Duplex Capable */
#define QTI_8085_ADVERTISE_1000HALF		0x0100

  /* 100TX Full Duplex Capable */
#define QTI_8085_ADVERTISE_1000FULL		0x0200

#define QTI_8085_ADVERTISE_ALL \
	(QTI_8085_ADVERTISE_10HALF | QTI_8085_ADVERTISE_10FULL | \
	QTI_8085_ADVERTISE_100HALF | QTI_8085_ADVERTISE_100FULL | \
	QTI_8085_ADVERTISE_1000FULL)

#define QTI_8085_ADVERTISE_MEGA_ALL \
	(QTI_8085_ADVERTISE_10HALF | QTI_8085_ADVERTISE_10FULL | \
	QTI_8085_ADVERTISE_100HALF | QTI_8085_ADVERTISE_100FULL)

#define QTI_8085_BX_ADVERTISE_1000FULL		0x0020
#define QTI_8085_BX_ADVERTISE_1000HALF		0x0040
#define QTI_8085_BX_ADVERTISE_PAUSE		0x0080
#define QTI_8085_BX_ADVERTISE_ASYM_PAUSE		0x0100

#define QTI_8085_BX_ADVERTISE_ALL \
	(QTI_8085_BX_ADVERTISE_ASYM_PAUSE | QTI_8085_BX_ADVERTISE_PAUSE | \
	QTI_8085_BX_ADVERTISE_1000HALF | QTI_8085_BX_ADVERTISE_1000FULL)

  /* Link Partner ability offset:5 */
  /* Same as advertise selector  */
#define QTI_8085_LINK_SLCT		0x001f

  /* Can do 10mbps half-duplex   */
#define QTI_8085_LINK_10BASETX_HALF_DUPLEX	0x0020

  /* Can do 10mbps full-duplex   */
#define QTI_8085_LINK_10BASETX_FULL_DUPLEX	0x0040

  /* Can do 100mbps half-duplex  */
#define QTI_8085_LINK_100BASETX_HALF_DUPLEX	0x0080

  /* Can do 100mbps full-duplex  */
#define QTI_8085_LINK_100BASETX_FULL_DUPLEX	0x0100

  /* Can do 1000mbps full-duplex  */
#define QTI_8085_LINK_1000BASETX_FULL_DUPLEX	0x0800

  /* Can do 1000mbps half-duplex  */
#define QTI_8085_LINK_1000BASETX_HALF_DUPLEX	0x0400

  /* 100BASE-T4  */
#define QTI_8085_LINK_100BASE4			0x0200

  /* PAUSE */
#define QTI_8085_LINK_PAUSE			0x0400

  /* Asymmetrical PAUSE */
#define QTI_8085_LINK_ASYPAUSE			0x0800

  /* Link partner faulted  */
#define QTI_8085_LINK_RFAULT			0x2000

  /* Link partner acked us */
#define QTI_8085_LINK_LPACK			0x4000

  /* Next page bit  */
#define QTI_8085_LINK_NPAGE			0x8000

  /* Auto-Negotiation Expansion Register offset:6 */

  /* Next Page Transmit Register offset:7 */

  /* Link partner Next Page Register offset:8 */

  /* 1000BASE-T Control Register offset:9 */
  /* Advertise 1000T HD capability */
#define QTI_8085_CTL_1000T_HD_CAPS		0x0100

  /* Advertise 1000T FD capability  */
#define QTI_8085_CTL_1000T_FD_CAPS		0x0200

  /* 1=Repeater/switch device port 0=DTE device */
#define QTI_8085_CTL_1000T_REPEATER_DTE		0x0400

  /* 1=Configure PHY as Master  0=Configure PHY as Slave */
#define QTI_8085_CTL_1000T_MS_VALUE		0x0800

  /* 1=Master/Slave manual config value  0=Automatic Master/Slave config */
#define QTI_8085_CTL_1000T_MS_ENABLE		0x1000

  /* Normal Operation */
#define QTI_8085_CTL_1000T_TEST_MODE_NORMAL	0x0000

  /* Transmit Waveform test */
#define QTI_8085_CTL_1000T_TEST_MODE_1		0x2000

  /* Master Transmit Jitter test */
#define QTI_8085_CTL_1000T_TEST_MODE_2		0x4000

  /* Slave Transmit Jitter test */
#define QTI_8085_CTL_1000T_TEST_MODE_3		0x6000

  /* Transmitter Distortion test */
#define QTI_8085_CTL_1000T_TEST_MODE_4		0x8000
#define QTI_8085_CTL_1000T_SPEED_MASK		0x0300
#define QTI_8085_CTL_1000T_DEFAULT_CAP_MASK	0x0300

  /* 1000BASE-T Status Register offset:10 */
  /* LP is 1000T HD capable */
#define QTI_8085_STATUS_1000T_LP_HD_CAPS		0x0400

  /* LP is 1000T FD capable */
#define QTI_8085_STATUS_1000T_LP_FD_CAPS		0x0800

  /* Remote receiver OK */
#define QTI_8085_STATUS_1000T_REMOTE_RX_STATUS	0x1000

  /* Local receiver OK */
#define QTI_8085_STATUS_1000T_LOCAL_RX_STATUS	0x2000

  /* 1=Local TX is Master, 0=Slave */
#define QTI_8085_STATUS_1000T_MS_CONFIG_RES	0x4000

#define QTI_8085_STATUS_1000T_MS_CONFIG_FAULT	0x8000

  /* Master/Slave config fault */
#define QTI_8085_STATUS_1000T_REMOTE_RX_STATUS_SHIFT	12
#define QTI_8085_STATUS_1000T_LOCAL_RX_STATUS_SHIFT	13

  /* Phy Specific Control Register offset:16 */
  /* 1=Polarity Reversal enabled */
#define QTI_8085_CTL_POLARITY_REVERSAL	0x0002

  /* 1=SQE Test enabled */
#define QTI_8085_CTL_SQE_TEST		0x0004
#define QTI_8085_CTL_MAC_POWERDOWN	0x0008

  /* 1=CLK125 low, 0=CLK125 toggling
     #define QTI_8085_CTL_CLK125_DISABLE	0x0010
   */
  /* MDI Crossover Mode bits 6:5 */
  /* Manual MDI configuration */
#define QTI_8085_CTL_MDI_MANUAL_MODE	0x0000

  /* Manual MDIX configuration */
#define QTI_8085_CTL_MDIX_MANUAL_MODE	0x0020

  /* 1000BASE-T: Auto crossover, 100BASE-TX/10BASE-T: MDI Mode */
#define QTI_8085_CTL_AUTO_X_1000T		0x0040

  /* Auto crossover enabled all speeds */
#define QTI_8085_CTL_AUTO_X_MODE		0x0060

  /* 1=Enable Extended 10BASE-T distance
   * (Lower 10BASE-T RX Threshold)
   * 0=Normal 10BASE-T RX Threshold */
#define QTI_8085_CTL_10BT_EXT_DIST_ENABLE	0x0080

  /* 1=5-Bit interface in 100BASE-TX
   * 0=MII interface in 100BASE-TX */
#define QTI_8085_CTL_MII_5BIT_ENABLE	0x0100

  /* 1=Scrambler disable */
#define QTI_8085_CTL_SCRAMBLER_DISABLE	0x0200

  /* 1=Force link good */
#define QTI_8085_CTL_FORCE_LINK_GOOD	0x0400

  /* 1=Assert CRS on Transmit */
#define QTI_8085_CTL_ASSERT_CRS_ON_TX	0x0800

#define QTI_8085_CTL_POLARITY_REVERSAL_SHIFT	1
#define QTI_8085_CTL_AUTO_X_MODE_SHIFT		5
#define QTI_8085_CTL_10BT_EXT_DIST_ENABLE_SHIFT	7


  /* Phy Specific status fields offset:17 */
  /* 1=Speed & Duplex resolved */
#define QTI_8085_STATUS_LINK_PASS		0x0400
#define QTI_8085_STATUS_RESOVLED		0x0800

  /* 1=Duplex 0=Half Duplex */
#define QTI_8085_STATUS_FULL_DUPLEX	0x2000

  /* Speed, bits 14:15 */
#define QTI_8085_STATUS_SPEED		0xC000
#define QTI_8085_STATUS_SPEED_MASK	0xC000

  /* 00=10Mbs */
#define QTI_8085_STATUS_SPEED_10MBS	0x0000

  /* 01=100Mbs */
#define QTI_8085_STATUS_SPEED_100MBS	0x4000

  /* 10=1000Mbs */
#define QTI_8085_STATUS_SPEED_1000MBS	0x8000
#define QTI_8085_SPEED_DUPLEX_RESOVLED(phy_status)	\
    (((phy_status) &					\
        (QTI_8085_STATUS_RESOVLED)) ==			\
        (QTI_8085_STATUS_RESOVLED))

  /*phy debug port1 register offset:29 */
  /*phy debug port2 register offset:30 */

  /*QTI_8085 interrupt flag */
#define QTI_8085_INTR_SPEED_CHANGE		0x4000
#define QTI_8085_INTR_DUPLEX_CHANGE		0x2000
#define QTI_8085_INTR_STATUS_UP_CHANGE		0x0400
#define QTI_8085_INTR_STATUS_DOWN_CHANGE		0x0800
#define QTI_8085_INTR_BX_FX_STATUS_DOWN_CHANGE	0x0100
#define QTI_8085_INTR_BX_FX_STATUS_UP_CHANGE	0x0080
#define QTI_8085_INTR_MEDIA_STATUS_CHANGE		0x1000
#define QTI_8085_INTR_WOL				0x0001
#define QTI_8085_INTR_POE				0x0002

#define QTI_8085_PHY_PSGMII_REDUCE_SERDES_TX_AMP	0x8a
#define QTI_8085_DAC_CTRL_MASK			0x380
#define QTI_8085_DAC_CTRL_INIT_CHECK		BIT(8)
#define QTI_8085_PHY_MMD7_DAC_CTRL		0x801a
#define QTI_8085_DAC_CTRL_VALUE			0x280
#define QTI_8085_PHY_MMD7_NUM			7
#define QTI_8085_PSGMII_TX_DRIVER_1_CTRL 	0xb
#define QTI_8085_PHY_MMD7_LED_1000_CTRL1		0x8076
#define QTI_8085_LED_1000_CTRL1_100_10_MASK	0x30

#define RUN_CDT				0x8000
#define CABLE_LENGTH_UNIT		0x0400
#define QTI_8085_MAX_TRIES		100
#define QTI_8085_ENABLE_HIBERNATION_VALUE		BIT(15)


/* Phy preferred medium type */
typedef enum {
        QTI_8085_PHY_MEDIUM_COPPER = 0,
        QTI_8085_PHY_MEDIUM_FIBER = 1, /**< Fiber */
} qti_8075_phy_medium_t;

/* Phy pages */
typedef enum {
        QTI_8085_PHY_SGBX_PAGES = 0, /* sgbx pages */
        QTI_8085_PHY_COPPER_PAGES = 1 /* copper pages */
} qti_8075_phy_reg_pages_t;

struct qti_8075_device {
	struct mii_dev	*bus;
};

/*
 * phy4 prfer medium
 * get phy4 prefer medum, fiber or copper;
 */
static qti_8075_phy_medium_t __phy_prefer_medium_get(
		struct phy_device *phydev)
{
	u16 phy_medium = phy_read(phydev, MDIO_DEVAD_NONE,
			QTI_8085_PHY_CHIP_CONFIG);
	return ((phy_medium & QTI_8085_PHY4_PREFER_FIBER) ?
		QTI_8085_PHY_MEDIUM_FIBER : QTI_8085_PHY_MEDIUM_COPPER);
}

/*
 *  phy4 activer medium
 *  get phy4 current active medium, fiber or copper;
 */
static qti_8075_phy_medium_t __phy_active_medium_get(
		struct phy_device *phydev)
{
	u16 phy_data = 0;
	u32 phy_mode = phy_read(phydev, MDIO_DEVAD_NONE,
			QTI_8085_PHY_CHIP_CONFIG);
	phy_mode &= 0x000f;

	if (phy_mode == QTI_8085_PHY_PSGMII_AMDET) {
		phy_data = phy_read(phydev, MDIO_DEVAD_NONE,
				QTI_8085_PHY_SGMII_STATUS);

		if ((phy_data & QTI_8085_PHY4_AUTO_COPPER_SELECT)) {
			return QTI_8085_PHY_MEDIUM_COPPER;
		} else if ((phy_data & QTI_8085_PHY4_AUTO_BX1000_SELECT)) {
			/* PHY_MEDIUM_FIBER_BX1000 */
			return QTI_8085_PHY_MEDIUM_FIBER;
		} else if ((phy_data & QTI_8085_PHY4_AUTO_FX100_SELECT)) {
			 /* PHY_MEDIUM_FIBER_FX100 */
			return QTI_8085_PHY_MEDIUM_FIBER;
		}
		/* link down */
		return __phy_prefer_medium_get(phydev);
	} else if ((phy_mode == QTI_8085_PHY_PSGMII_BASET) ||
			(phy_mode == QTI_8085_PHY_SGMII_BASET)) {
		return QTI_8085_PHY_MEDIUM_COPPER;
	} else if ((phy_mode == QTI_8085_PHY_PSGMII_BX1000) ||
			(phy_mode == QTI_8085_PHY_PSGMII_FX100)) {
		return QTI_8085_PHY_MEDIUM_FIBER;
	} else {
		return QTI_8085_PHY_MEDIUM_COPPER;
	}
}

/*
 *  phy4 copper page or fiber page select
 *  set phy4 copper or fiber page
 */

static int __phy_reg_pages_sel(struct phy_device *phydev,
		qti_8075_phy_reg_pages_t phy_reg_pages)
{
	u16 reg_pages;
	reg_pages = phy_read(phydev, MDIO_DEVAD_NONE,
			QTI_8085_PHY_CHIP_CONFIG);

	if (phy_reg_pages == QTI_8085_PHY_COPPER_PAGES) {
		reg_pages |= 0x8000;
	} else if (phy_reg_pages == QTI_8085_PHY_SGBX_PAGES) {
		reg_pages &= ~0x8000;
	} else
		return -EINVAL;

	phy_write(phydev, MDIO_DEVAD_NONE, QTI_8085_PHY_CHIP_CONFIG, reg_pages);
	return 0;
}

/*
 *  phy4 reg pages selection by active medium
 *  phy4 reg pages selection
 */
static int __phy_reg_pages_sel_by_active_medium(struct phy_device *phydev)
{
	qti_8075_phy_medium_t phy_medium;
	qti_8075_phy_reg_pages_t reg_pages;

	phy_medium = __phy_active_medium_get(phydev);
	if (phy_medium == QTI_8085_PHY_MEDIUM_FIBER) {
		reg_pages = QTI_8085_PHY_SGBX_PAGES;
	} else if (phy_medium == QTI_8085_PHY_MEDIUM_COPPER) {
		reg_pages = QTI_8085_PHY_COPPER_PAGES;
	} else {
		return -1;
	}

	return __phy_reg_pages_sel(phydev, reg_pages);
}

/*
 *  get phy4 medium is 100fx
 */
static u8 __medium_is_fiber_100fx(struct phy_device *phydev)
{
	u16 phy_data = phy_read(phydev, MDIO_DEVAD_NONE,
			QTI_8085_PHY_SGMII_STATUS);
	if (phy_data & QTI_8085_PHY4_AUTO_FX100_SELECT) {
		return 1;
	}

	/* Link down */
	if ((!(phy_data & QTI_8085_PHY4_AUTO_COPPER_SELECT)) &&
	    (!(phy_data & QTI_8085_PHY4_AUTO_BX1000_SELECT)) &&
	    (!(phy_data & QTI_8085_PHY4_AUTO_SGMII_SELECT))) {

		phy_data = phy_read(phydev, MDIO_DEVAD_NONE,
				QTI_8085_PHY_CHIP_CONFIG);
		if ((phy_data & QTI_8085_PHY4_PREFER_FIBER)
		    && (!(phy_data & QTI_8085_PHY4_FIBER_MODE_1000BX))) {
			return 1;
		}
	}
	return 0;
}

/*
 * qti_8075_restart_autoneg - restart the phy autoneg
 */
static u32 qti_8075_phy_restart_autoneg(struct phy_device *phydev)
{
	u16 phy_data;

	if (phydev->addr == COMBO_PHY_ID) {
		if (__medium_is_fiber_100fx(phydev))
			return -1;
		__phy_reg_pages_sel_by_active_medium(phydev);
	}
	phy_data = phy_read(phydev, MDIO_DEVAD_NONE, QTI_8085_PHY_CONTROL);
	phy_data |= QTI_8085_CTRL_AUTONEGOTIATION_ENABLE;
	phy_write(phydev, MDIO_DEVAD_NONE, QTI_8085_PHY_CONTROL,
			phy_data | QTI_8085_CTRL_RESTART_AUTONEGOTIATION);

	return 0;
}

/*
 * qti_8075_phy_get_8023az status
 * get 8023az status
 */
static int qti_8075_phy_get_8023az(struct phy_device *phydev, u8 *enable)
{
	u16 phy_data;
	if (phydev->addr == COMBO_PHY_ID) {
		if (QTI_8085_PHY_MEDIUM_COPPER !=
			__phy_active_medium_get(phydev))
			return -1;
	}
	*enable = 0;

	phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD7_NUM,
				QTI_8085_PHY_MMD7_ADDR_8023AZ_EEE_CTRL);

	if ((phy_data & 0x0004) && (phy_data & 0x0002))
		*enable = 1;

	return 0;
}

/*
 * qti_8075_phy_set_powersave - set power saving status
 */
static int qti_8075_phy_set_powersave(struct phy_device *phydev, u8 enable)
{
	u16 phy_data;
	u8 status = 0;

	if (phydev->addr == COMBO_PHY_ID) {
		if (QTI_8085_PHY_MEDIUM_COPPER !=
			__phy_active_medium_get(phydev))
			return -1;
	}

	if (enable) {
		qti_8075_phy_get_8023az(phydev, &status);
		if (!status) {
			phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL);
			phy_data &= ~(1 << 14);
			phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL,
				phy_data);
		}
		phy_data = phy_read_mmd(phydev,	QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL5);
		phy_data &= ~(1 << 14);
		phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL5,
				phy_data);

		phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL3);
		phy_data &= ~(1 << 15);
		phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL3, phy_data);

	} else {
		phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL);
		phy_data |= (1 << 14);
		phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL,
				phy_data);

		phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL5);
		phy_data |= (1 << 14);
		phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL5, phy_data);

		phy_data = phy_read_mmd(phydev,	QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL3);
		phy_data |= (1 << 15);
		phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_CLD_CTRL3, phy_data);

	}

	phy_write(phydev, MDIO_DEVAD_NONE, QTI_8085_PHY_CONTROL, 0x9040);
	return 0;
}

/*
 * qti_8075_phy_set_802.3az
 */
 static int qti_8075_phy_set_8023az(struct phy_device *phydev, u8 enable)
{
	u16 phy_data;

	if (phydev->addr == COMBO_PHY_ID) {
		if (QTI_8085_PHY_MEDIUM_COPPER !=
			 __phy_active_medium_get(phydev))
			return -1;
	}
	phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD7_NUM,
			QTI_8085_PHY_MMD7_ADDR_8023AZ_EEE_CTRL);
	if (enable) {
		phy_data |= 0x0006;

		phy_write_mmd(phydev, QTI_8085_PHY_MMD7_NUM,
			     QTI_8085_PHY_MMD7_ADDR_8023AZ_EEE_CTRL,
			     phy_data);
		if (phydev->drv->uid == QTI_8085_PHY_V1_0_5P) {
			/*
			 * Workaround to avoid packet loss and < 10m cable
			 * 1000M link not stable under az enable
			 */
			phy_write_mmd(phydev,
				QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL,
				AZ_TIMER_CTRL_ADJUST_VALUE);

			phy_write_mmd(phydev,
				QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_CLD_CTRL,
				AZ_CLD_CTRL_ADJUST_VALUE);
		}
	} else {
		phy_data &= ~0x0006;
		phy_write_mmd(phydev, QTI_8085_PHY_MMD7_NUM,
				QTI_8085_PHY_MMD7_ADDR_8023AZ_EEE_CTRL,
				phy_data);
		if (phydev->drv->uid == QTI_8085_PHY_V1_0_5P) {
			phy_write_mmd(phydev, QTI_8085_PHY_MMD3_NUM,
				QTI_8085_PHY_MMD3_ADDR_8023AZ_TIMER_CTRL,
				AZ_TIMER_CTRL_DEFAULT_VALUE);
		}
	}

	qti_8075_phy_restart_autoneg(phydev);
	return 0;
}

static int qti_8075_parse_status(struct phy_device *phydev)
{
	unsigned int speed;
	unsigned int mii_reg;

	if (phydev->addr == COMBO_PHY_ID)
		__phy_reg_pages_sel_by_active_medium(phydev);

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, QTI_8085_PHY_SPEC_STATUS);
	if (mii_reg & QTI_8085_STATUS_LINK_PASS)
		phydev->link = 1;
	else
		phydev->link = 0;

	if (mii_reg & QTI_8085_STATUS_FULL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	speed = mii_reg & QTI_8085_STATUS_SPEED_MASK;

	switch (speed) {
	case QTI_8085_STATUS_SPEED_1000MBS:
		phydev->speed = SPEED_1000;
		break;
	case QTI_8085_STATUS_SPEED_100MBS:
		phydev->speed = SPEED_100;
		break;
	case QTI_8085_STATUS_SPEED_10MBS:
		phydev->speed = SPEED_10;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int qti_8075_probe(struct phy_device *phydev) {
	struct qti_8075_device *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return -ENOMEM;

	memset(dev, 0, sizeof(*dev));

	phydev->priv = dev;
	dev->bus = phydev->bus;

	return 0;
}

static int qti_8075_config(struct phy_device *phydev)
{
	struct phy_device local_phydev;
	u16 phy_data;
	u32 port_id = 0;

	phy_data = phy_read_mmd(phydev, QTI_8085_PHY_MMD7_NUM,
			QTI_8085_PHY_MMD7_DAC_CTRL);
	if (!(phy_data & QTI_8085_DAC_CTRL_INIT_CHECK)) {
		debug("%s phy_addr: 0x%x config already done!! \n",
				__func__, phydev->addr);
		return 0;
	}

	memcpy(&local_phydev, phydev, sizeof(struct phy_device));

	if (phydev->drv->uid == QTI_8085_PHY_V1_0_5P) {
		local_phydev.addr = PSGMII_ID;

		phy_data = phy_read_mmd(&local_phydev, QTI_8085_PHY_MMD1_NUM,
				QTI_8085_PSGMII_FIFI_CTRL);
		phy_data &= 0xbfff;
		phy_write_mmd(&local_phydev, QTI_8085_PHY_MMD1_NUM,
			QTI_8085_PSGMII_FIFI_CTRL, phy_data);
	}

	/*
	 * Enable phy power saving function by default
	 */
	if ((phydev->drv->uid == QTI_8085_PHY_V1_0_5P) ||
	    (phydev->drv->uid == QTI_8085_PHY_V1_1_5P) ||
	    (phydev->drv->uid == QTI_8085_PHY_V1_1_2P)) {
		for (port_id = 0; port_id < 5; port_id++) {
			/*enable phy power saving function by default */
			local_phydev.addr = phydev->addr + port_id;

			qti_8075_phy_set_8023az(&local_phydev, 0x1);
			qti_8075_phy_set_powersave(&local_phydev, 0x1);

			/* set hibernate status */
			phy_write(&local_phydev, MDIO_DEVAD_NONE,
					QTI_8085_DEBUG_PORT_ADDRESS,
					QTI_8085_DEBUG_PHY_HIBERNATION_CTRL);
			phy_data = phy_read(&local_phydev, MDIO_DEVAD_NONE,
					QTI_8085_DEBUG_PORT_DATA);
			phy_data |= QTI_8085_ENABLE_HIBERNATION_VALUE;
			phy_write(&local_phydev, MDIO_DEVAD_NONE,
					QTI_8085_DEBUG_PORT_DATA, phy_data);

			/*
			 * change malibu control_dac[2:0] of
			 * MMD7 0x801A bit[9:7] from 111 to 101
			 */
			phy_data = phy_read_mmd(&local_phydev,
					QTI_8085_PHY_MMD7_NUM,
					QTI_8085_PHY_MMD7_DAC_CTRL);
			phy_data &= ~QTI_8085_DAC_CTRL_MASK;
			phy_data |= QTI_8085_DAC_CTRL_VALUE;
			phy_write_mmd(&local_phydev, QTI_8085_PHY_MMD7_NUM,
					QTI_8085_PHY_MMD7_DAC_CTRL, phy_data);

			/* add 10M and 100M link LED behavior for QFN board*/
			phy_data = phy_read_mmd(&local_phydev,
					QTI_8085_PHY_MMD7_NUM,
					QTI_8085_PHY_MMD7_LED_1000_CTRL1);
			phy_data &= ~QTI_8085_LED_1000_CTRL1_100_10_MASK;
			phy_data |= QTI_8085_LED_1000_CTRL1_100_10_MASK;
			phy_write_mmd(&local_phydev, QTI_8085_PHY_MMD7_NUM,
					QTI_8085_PHY_MMD7_LED_1000_CTRL1,
					phy_data);
		}
	}

	if ((phydev->drv->uid == QTI_8085_PHY_V1_1_2P) &&
			(phydev->addr >= 0x3)) {
		local_phydev.addr = phydev->addr - 0x3;
	}
	else {
		local_phydev.addr = phydev->addr;
	}

	/*
	 * Enable AZ transmitting ability
	 */
	local_phydev.addr += PSGMII_ID;
	phy_write_mmd(&local_phydev, QTI_8085_PHY_MMD1_NUM,
				QTI_8085_PSGMII_MODE_CTRL,
				QTI_8085_PHY_PSGMII_MODE_CTRL_ADJUST_VALUE);

	/* adjust psgmii serdes tx amp */
	phy_write(&local_phydev, MDIO_DEVAD_NONE,
				QTI_8085_PSGMII_TX_DRIVER_1_CTRL,
				QTI_8085_PHY_PSGMII_REDUCE_SERDES_TX_AMP);
	local_phydev.addr -= PSGMII_ID;

	/* to avoid psgmii module goes into hibernation,
	 * work with psgmii self test
	 * */
	local_phydev.addr += COMBO_PHY_ID;
	phy_data = phy_read_mmd(&local_phydev, QTI_8085_PHY_MMD3_NUM, 0x805a);
	phy_data &= (~(1 << 1));
	phy_write_mmd(&local_phydev, QTI_8085_PHY_MMD3_NUM, 0x805a, phy_data);
	local_phydev.addr -= COMBO_PHY_ID;

	return 0;
}

static int qti_8075_startup(struct phy_device *phydev)
{
	int ret = 0;

	ret = qti_8075_parse_status(phydev);
	if (ret)
		return ret;

	return ret;
}

static struct phy_driver qti_8075_v1_0_5P_driver = {
	.name = "QTI 8075 v1.0.5 PHY Driver",
	.uid = QTI_8085_PHY_V1_0_5P,
	.mask = 0xffffffff,
	.features = PHY_GBIT_FEATURES,
	.probe = &qti_8075_probe,
	.config = &qti_8075_config,
	.startup = &qti_8075_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver qti_8075_v1_1_5P_driver = {
	.name = "QTI 8075 v1.1.5 PHY Driver",
	.uid = QTI_8085_PHY_V1_1_5P,
	.mask = 0xffffffff,
	.features = PHY_GBIT_FEATURES,
	.probe = &qti_8075_probe,
	.config = &qti_8075_config,
	.startup = &qti_8075_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver qti_8075_v1_1_2P_driver = {
	.name = "QTI 8075 v1.1.2 PHY Driver",
	.uid = QTI_8085_PHY_V1_1_2P,
	.mask = 0xffffffff,
	.features = PHY_GBIT_FEATURES,
	.probe = &qti_8075_probe,
	.config = &qti_8075_config,
	.startup = &qti_8075_startup,
	.shutdown = &genphy_shutdown,
};

int phy_8075_init(void)
{
	phy_register(&qti_8075_v1_0_5P_driver);
	phy_register(&qti_8075_v1_1_5P_driver);
	phy_register(&qti_8075_v1_1_2P_driver);
	return 0;
}
