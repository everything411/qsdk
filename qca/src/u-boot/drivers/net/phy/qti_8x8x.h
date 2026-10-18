// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2022, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QTI_8X8X_H_
#define _QTI_8X8X_H_

#include <linux/compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/*QTI 8X8X Core Registers */
#define QTI_8X8X_UNIPHY_CFG					0xC90F014
#define QTI_8X8X_EPHY_CFG					0xC90F018
#define QTI_8X8X_GEPHY0_TX_CBCR					0xC800058
#define QTI_8X8X_SRDS0_SYS_CBCR					0xC8001A8
#define QTI_8X8X_SRDS1_SYS_CBCR					0xC8001AC
#define QTI_8X8X_EPHY0_SYS_CBCR					0xC8001B0
#define QTI_8X8X_EPHY1_SYS_CBCR					0xC8001B4
#define QTI_8X8X_EPHY2_SYS_CBCR					0xC8001B8
#define QTI_8X8X_EPHY3_SYS_CBCR					0xC8001BC
#define QTI_8X8X_GCC_GEPHY_MISC					0xC800304
#define QTI_8X8X_QFPROM_RAW_PTE_ROW2_MSB			0xC900014
#define QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW4_LSB		0xC900048
#define QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW6_MSB		0xC90005C
#define QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW7_LSB		0xC900060
#define QTI_8X8X_QFPROM_RAW_CALIBRATION_ROW8_LSB		0xC900068
#define QTI_8X8X_PHY_ADDR_LENGTH				5
#define QTI_8X8X_PHY_ADDR_NUM					4
#define QTI_8X8X_UNIPHY_ADDR_NUM				3
#define QTI_8X8X_MII_HIGH_ADDR_PREFIX				0x18
#define QTI_8X8X_MII_LOW_ADDR_PREFIX				0x10
#define QTI_8X8X_PHY_DEBUG_PORT_ADDR				0x1d
#define QTI_8X8X_PHY_DEBUG_PORT_DATA				0x1e
#define QTI_8X8X_PHY_LDO_EFUSE_REG				0x180
#define QTI_8X8X_PHY_ICC_EFUSE_REG				0x280

/*MII register*/
#define QTI_8X8X_PHY_FIFO_CONTROL				0x19

/*MII register field*/
#define QTI_8X8X_PHY_FIFO_RESET					0x3

/*MMD1 register*/
#define QTI_8X8X_PHY_MMD1_NUM					0x1

/*MMD3 register*/
#define QTI_8X8X_PHY_MMD3_NUM					0x3
#define QTI_8X8X_PHY_MMD3_ADDR_8023AZ_EEE_2500M_CAPABILITY	0x15
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL3			0x8074
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL4			0x8075
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL5			0x8076
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL6			0x8077
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL7			0x8078
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL9			0x807a
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL13			0x807e
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL14			0x807f

/*MMD3 register field*/
#define QTI_8X8X_PHY_EEE_CAPABILITY_2500M			0x1
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL3_VAL			0xc040
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL4_VAL			0xa060
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL5_VAL			0xc040
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL6_VAL			0xa060
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL7_VAL			0xc050
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL9_VAL			0xc060
#define QTI_8X8X_PHY_MMD3_CDT_THRESH_CTRL13_VAL			0xb060
#define QTI_8X8X_PHY_MMD3_NEAR_ECHO_THRESH_VAL			0x1eb0

/*MMD7 register*/
#define QTI_8X8X_PHY_MMD7_NUM					0x7
#define QTI_8X8X_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL		0x3e
#define QTI_8X8X_PHY_MMD7_ADDR_8023AZ_EEE_2500M_PARTNER		0x3f
#define QTI_8X8X_PHY_MMD7_IPG_10_11_ENABLE			0x901d

/*MMD7 register field*/
#define QTI_8X8X_PHY_8023AZ_EEE_2500BT				0x1
#define QTI_8X8X_PHY_MMD7_IPG_10_EN				0
#define QTI_8X8X_PHY_MMD7_IPG_11_EN				0x1

/*DEBUG port analog register*/
#define QTI_8X8X_PHY_DEBUG_ANA_INTERFACE_CLK_SEL		0x8b80
#define QTI_8X8X_DEBUG_PORT_ADDRESS				29
#define QTI_8X8X_DEBUG_PORT_DATA				30

#define QTI_8X8X_PHY_CONTROL					0
#define QTI_8X8X_AUTONEG_ADVERT					4
#define QTI_8X8X_LINK_PARTNER_ABILITY				5
#define QTI_8X8X_1000BASET_CONTROL				9
#define QTI_8X8X_1000BASET_STATUS				10
#define QTI_8X8X_PHY_SPEC_STATUS				17
#define QTI_8X8X_CTRL_AUTONEGOTIATION_ENABLE			0x1000
#define QTI_8X8X_CTRL_SOFTWARE_RESET				0x8000


#define QTI_8X8X_STATUS_LINK_PASS				0x0400
#define PHY_INVALID_DATA					0xffff

#define QTI_8X8X_PHY_MMD7_AUTONEGOTIATION_CONTROL		0x20
#define QTI_8X8X_PHY_MMD7_LP_2500M_ABILITY			0x21

  /* Auto-Negotiation Advertisement register. offset:4 */
#define QTI_8X8X_ADVERTISE_SELECTOR_FIELD			0x0001

  /* 10T   Half Duplex Capable */
#define QTI_8X8X_ADVERTISE_10HALF				0x0020

  /* 10T   Full Duplex Capable */
#define QTI_8X8X_ADVERTISE_10FULL				0x0040

  /* 100TX Half Duplex Capable */
#define QTI_8X8X_ADVERTISE_100HALF				0x0080

  /* 100TX Full Duplex Capable */
#define QTI_8X8X_ADVERTISE_100FULL				0x0100

  /* 100T4 Capable */
#define QTI_8X8X_ADVERTISE_100T4				0x0200

  /* Pause operation desired */
#define QTI_8X8X_ADVERTISE_PAUSE				0x0400

  /* Asymmetric Pause Direction bit */
#define QTI_8X8X_ADVERTISE_ASYM_PAUSE				0x0800

  /* Remote Fault detected */
#define QTI_8X8X_ADVERTISE_REMOTE_FAULT				0x2000

  /* 1000TX Half Duplex Capable */
#define QTI_8X8X_ADVERTISE_1000HALF				0x0100

  /* 1000TX Full Duplex Capable */
#define QTI_8X8X_ADVERTISE_1000FULL				0x0200

  /* 2500TX Full Duplex Capable */
#define QTI_8X8X_ADVERTISE_2500FULL				0x80

#define QTI_8X8X_ADVERTISE_ALL \
    (QTI_8X8X_ADVERTISE_10HALF | QTI_8X8X_ADVERTISE_10FULL | \
     QTI_8X8X_ADVERTISE_100HALF | QTI_8X8X_ADVERTISE_100FULL | \
     QTI_8X8X_ADVERTISE_1000FULL)

#define QTI_8X8X_ADVERTISE_MEGA_ALL \
    (QTI_8X8X_ADVERTISE_10HALF | QTI_8X8X_ADVERTISE_10FULL | \
     QTI_8X8X_ADVERTISE_100HALF | QTI_8X8X_ADVERTISE_100FULL | \
     QTI_8X8X_ADVERTISE_PAUSE | QTI_8X8X_ADVERTISE_ASYM_PAUSE)

/* FDX =1, half duplex =0 */
#define QTI_8X8X_CTRL_FULL_DUPLEX				0x0100

  /* Restart auto negotiation */
#define QTI_8X8X_CTRL_RESTART_AUTONEGOTIATION			0x0200

  /* 1=Duplex 0=Half Duplex */
#define QTI_8X8X_STATUS_FULL_DUPLEX				0x2000
#define QTI_8X8X_PHY_RX_FLOWCTRL_STATUS				0x4
#define QTI_8X8X_PHY_TX_FLOWCTRL_STATUS				0x8

  /* Speed, bits 9:7 */
#define QTI_8X8X_STATUS_SPEED_MASK				0x380


  /* 000=10Mbs */
#define QTI_8X8X_STATUS_SPEED_10MBS				0x0000

  /* 001=100Mbs */
#define QTI_8X8X_STATUS_SPEED_100MBS				0x80

  /* 010=1000Mbs */
#define QTI_8X8X_STATUS_SPEED_1000MBS				0x100

  /* 100=2500Mbs */
#define QTI_8X8X_STATUS_SPEED_2500MBS				0x200


#define QTI_8X8X_MII_ADDR_C45					(1<<30)
#define QTI_8X8X_REG_C45_ADDRESS(dev_type, reg_num) (QTI_8X8X_MII_ADDR_C45 | \
			((dev_type & 0x1f) << 16) | (reg_num & 0xffff))

//phy autoneg adv
#define FAL_PHY_ADV_10T_HD					0x01
#define FAL_PHY_ADV_10T_FD					0x02
#define FAL_PHY_ADV_100TX_HD					0x04
#define FAL_PHY_ADV_100TX_FD					0x08
#define FAL_PHY_ADV_1000T_FD					0x200
#define FAL_PHY_ADV_1000BX_HD					0x400
#define FAL_PHY_ADV_1000BX_FD					0x800
#define FAL_PHY_ADV_2500T_FD					0x1000
#define FAL_PHY_ADV_5000T_FD					0x2000
#define FAL_PHY_ADV_10000T_FD					0x4000
#define FAL_PHY_ADV_10G_R_FD					0x8000

#define FAL_DEFAULT_MAX_FRAME_SIZE				0x5ee

#define FAL_PHY_ADV_PAUSE					0x10
#define FAL_PHY_ADV_ASY_PAUSE					0x20

/* Chip information */
#define QCA_VER_QTI_8X8X						0x17
#define CHIP_QTI_8X8X						0x13

/* Port Status Register */
#define PORT_STATUS
#define PORT_STATUS_OFFSET			0x007c
#define PORT_STATUS_E_LENGTH			4
#define PORT_STATUS_E_OFFSET			0x0004
#define PORT_STATUS_NR_E			7

#define DUPLEX_MODE
#define PORT_STATUS_DUPLEX_MODE_BOFFSET         6
#define PORT_STATUS_DUPLEX_MODE_BLEN            1
#define PORT_STATUS_DUPLEX_MODE_FLAG            HSL_RW

#define SPEED_MODE
#define PORT_STATUS_SPEED_MODE_BOFFSET          0
#define PORT_STATUS_SPEED_MODE_BLEN             2
#define PORT_STATUS_SPEED_MODE_FLAG             HSL_RW

#define LINK_EN
#define PORT_STATUS_LINK_EN_BOFFSET             9
#define PORT_STATUS_LINK_EN_BLEN                1
#define PORT_STATUS_LINK_EN_FLAG                HSL_RW

#define RXMAC_EN
#define PORT_STATUS_RXMAC_EN_BOFFSET            3
#define PORT_STATUS_RXMAC_EN_BLEN               1
#define PORT_STATUS_RXMAC_EN_FLAG               HSL_RW

#define TXMAC_EN
#define PORT_STATUS_TXMAC_EN_BOFFSET            2
#define PORT_STATUS_TXMAC_EN_BLEN               1
#define PORT_STATUS_TXMAC_EN_FLAG               HSL_RW

#define RX_FLOW_EN
#define PORT_STATUS_RX_FLOW_EN_BOFFSET          5
#define PORT_STATUS_RX_FLOW_EN_BLEN             1
#define PORT_STATUS_RX_FLOW_EN_FLAG             HSL_RW

#define TX_FLOW_EN
#define PORT_STATUS_TX_FLOW_EN_BOFFSET          4
#define PORT_STATUS_TX_FLOW_EN_BLEN             1
#define PORT_STATUS_TX_FLOW_EN_FLAG             HSL_RW

#define TX_HALF_FLOW_EN
#define PORT_STATUS_TX_HALF_FLOW_EN_BOFFSET     7
#define PORT_STATUS_TX_HALF_FLOW_EN_BLEN        1
#define PORT_STATUS_TX_HALF_FLOW_EN_FLAG        HSL_RW

#define QTI_8X8X_PORT_SPEED_10M			0
#define QTI_8X8X_PORT_SPEED_100M		1
#define QTI_8X8X_PORT_SPEED_1000M		2
#define QTI_8X8X_PORT_SPEED_2500M		QTI_8X8X_PORT_SPEED_1000M
#define QTI_8X8X_PORT_HALF_DUPLEX		0
#define QTI_8X8X_PORT_FULL_DUPLEX		1

/* Header Ctl Register */
#define HEADER_CTL
#define HEADER_CTL_OFFSET			0x0098
#define HEADER_CTL_E_LENGTH			4
#define HEADER_CTL_E_OFFSET			0x0004
#define HEADER_CTL_NR_E				1

#define TYPE_LEN
#define HEADER_CTL_TYPE_LEN_BOFFSET		16
#define HEADER_CTL_TYPE_LEN_BLEN		1
#define HEADER_CTL_TYPE_LEN_FLAG		HSL_RW

#define TYPE_VAL
#define HEADER_CTL_TYPE_VAL_BOFFSET		0
#define HEADER_CTL_TYPE_VAL_BLEN		16
#define HEADER_CTL_TYPE_VAL_FLAG		HSL_RW


/* Port Header Ctl Register */
#define PORT_HDR_CTL
#define PORT_HDR_CTL_OFFSET			0x009c
#define PORT_HDR_CTL_E_LENGTH			4
#define PORT_HDR_CTL_E_OFFSET			0x0004
#define PORT_HDR_CTL_NR_E			7

#define RXHDR_MODE
#define PORT_HDR_CTL_RXHDR_MODE_BOFFSET         2
#define PORT_HDR_CTL_RXHDR_MODE_BLEN            2
#define PORT_HDR_CTL_RXHDR_MODE_FLAG            HSL_RW

#define TXHDR_MODE
#define PORT_HDR_CTL_TXHDR_MODE_BOFFSET         0
#define PORT_HDR_CTL_TXHDR_MODE_BLEN            2
#define PORT_HDR_CTL_TXHDR_MODE_FLAG            HSL_RW

#define QTI_8X8X_HEADER_TYPE_VAL		0xaaaa

/* Global Forward Control1 Register */
#define FORWARD_CTL1
#define FORWARD_CTL1_OFFSET			0x0624
#define FORWARD_CTL1_E_LENGTH			4
#define FORWARD_CTL1_E_OFFSET			0
#define FORWARD_CTL1_NR_E			1

#define BC_FLOOD_DP
#define FORWARD_CTL1_BC_FLOOD_DP_BOFFSET	16
#define FORWARD_CTL1_BC_FLOOD_DP_BLEN		7
#define FORWARD_CTL1_BC_FLOOD_DP_FLAG		HSL_RW

//#######
#define ISISC_PHY_MODE_PHY_ID			4
#define ISISC_LPI_PORT1_OFFSET			4
#define ISISC_LPI_BIT_STEP			2

/* ISIS Mask Control Register */
#define MASK_CTL
#define MASK_CTL_ID				0
#define MASK_CTL_OFFSET				0x0000
#define MASK_CTL_E_LENGTH			4
#define MASK_CTL_E_OFFSET			0
#define MASK_CTL_NR_E				1

#define DEVICE_ID
#define MASK_CTL_DEVICE_ID_BOFFSET		8
#define MASK_CTL_DEVICE_ID_BLEN			8
#define MASK_CTL_DEVICE_ID_FLAG			HSL_RO

#define REV_ID
#define MASK_CTL_REV_ID_BOFFSET			0
#define MASK_CTL_REV_ID_BLEN			8
#define MASK_CTL_REV_ID_FLAG			HSL_RO

/* EEE control Register */
#define EEE_CTL
#define EEE_CTL_OFFSET				0x0100
#define EEE_CTL_E_LENGTH			4
#define EEE_CTL_E_OFFSET			0
#define EEE_CTL_NR_E				1

/* Port Status Register */
#define PORT_STATUS
#define PORT_STATUS_OFFSET			0x007c
#define PORT_STATUS_E_LENGTH			4
#define PORT_STATUS_E_OFFSET			0x0004
#define PORT_STATUS_NR_E			7

#define FLOW_LINK_EN
#define PORT_STATUS_FLOW_LINK_EN_BOFFSET        12
#define PORT_STATUS_FLOW_LINK_EN_BLEN           1
#define PORT_STATUS_FLOW_LINK_EN_FLAG           HSL_RW

#define DUPLEX_MODE
#define PORT_STATUS_DUPLEX_MODE_BOFFSET         6
#define PORT_STATUS_DUPLEX_MODE_BLEN            1
#define PORT_STATUS_DUPLEX_MODE_FLAG            HSL_RW

#define SPEED_MODE
#define PORT_STATUS_SPEED_MODE_BOFFSET          0
#define PORT_STATUS_SPEED_MODE_BLEN             2
#define PORT_STATUS_SPEED_MODE_FLAG             HSL_RW

#define LEAVE_EN_OFFSET				2
#define JOIN_EN_OFFSET				1
#define IGMP_MLD_EN_OFFSET			0

/* Port HOL CTL0 Register */
#define PORT_HOL_CTL0
#define PORT_HOL_CTL0_OFFSET			0x0970
#define PORT_HOL_CTL0_E_LENGTH			4
#define PORT_HOL_CTL0_E_OFFSET			0x0008
#define PORT_HOL_CTL0_NR_E			7

#define PORT_DESC_NR
#define PORT_HOL_CTL0_PORT_DESC_NR_BOFFSET	24
#define PORT_HOL_CTL0_PORT_DESC_NR_BLEN         8
#define PORT_HOL_CTL0_PORT_DESC_NR_FLAG         HSL_RW

/* Port HOL CTL1 Register */
#define PORT_HOL_CTL1
#define PORT_HOL_CTL1_OFFSET			0x0974
#define PORT_HOL_CTL1_E_LENGTH			4
#define PORT_HOL_CTL1_E_OFFSET			0x0008
#define PORT_HOL_CTL1_NR_E			7

#define PORT_RED_EN
#define PORT_HOL_CTL1_PORT_RED_EN_BOFFSET       8
#define PORT_HOL_CTL1_PORT_RED_EN_BLEN          1
#define PORT_HOL_CTL1_PORT_RED_EN_FLAG          HSL_RW

#define PORT_IN_DESC_EN
#define PORT_HOL_CTL1_PORT_IN_DESC_EN_BOFFSET   0
#define PORT_HOL_CTL1_PORT_IN_DESC_EN_BLEN      6
#define PORT_HOL_CTL1_PORT_IN_DESC_EN_FLAG      HSL_RW

#define ISISC_QOS_PORT_RX_BUFFER_MAX		504
#define ISISC_QOS_PORT_TX_BUFFER_MAX		2040
#define ISISC_QOS_HOL_STEP			8
#define ISISC_QOS_HOL_MOD			3

/****************************************************************************
 *
 *  1) PinCtrl/TLMM Register Definition
 *
 ****************************************************************************/
/* TLMM_GPIO_CFGn */
#define TLMM_GPIO_CFGN
#define TLMM_GPIO_CFGN_OFFSET			0xC400000
#define TLMM_GPIO_CFGN_E_LENGTH			4
#define TLMM_GPIO_CFGN_E_OFFSET			0x1000
#define TLMM_GPIO_CFGN_NR_E			80

#define GPIO_HIHYS_EN
#define TLMM_GPIO_CFGN_GPIO_HIHYS_EN_BOFFSET	10
#define TLMM_GPIO_CFGN_GPIO_HIHYS_EN_BLEN       1
#define TLMM_GPIO_CFGN_GPIO_HIHYS_EN_FLAG       HSL_RW

#define GPIO_OEA
#define TLMM_GPIO_CFGN_GPIO_OEA_BOFFSET		9
#define TLMM_GPIO_CFGN_GPIO_OEA_BLEN            1
#define TLMM_GPIO_CFGN_GPIO_OEA_FLAG            HSL_RW

#define DRV_STRENGTH
#define TLMM_GPIO_CFGN_DRV_STRENGTH_BOFFSET     6
#define TLMM_GPIO_CFGN_DRV_STRENGTH_BLEN        3
#define TLMM_GPIO_CFGN_DRV_STRENGTH_FLAG        HSL_RW

typedef enum {
	ADC_RISING = 0,
	ADC_FALLING = 0xf0,
}
qti_8x8x_adc_edge_t;

typedef enum {
	FAL_NO_HEADER_EN = 0,
	FAL_ONLY_MANAGE_FRAME_EN,
	FAL_ALL_TYPE_FRAME_EN
} port_header_mode_t;

struct port_phy_status
{
	u32 link_status;
	u32 speed;
	u32 duplex;
	bool tx_flowctrl;
	bool rx_flowctrl;
};


enum QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH {
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_2_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_4_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_6_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_8_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_10_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_12_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_14_MA,
    QTI_8X8X_TLMM_GPIO_CFGN_DRV_STRENGTH_16_MA,
};

#define FUNC_SEL
#define TLMM_GPIO_CFGN_FUNC_SEL_BOFFSET             2
#define TLMM_GPIO_CFGN_FUNC_SEL_BLEN                4
#define TLMM_GPIO_CFGN_FUNC_SEL_FLAG                HSL_RW

#define GPIO_PULL
#define TLMM_GPIO_CFGN_GPIO_PULL_BOFFSET            0
#define TLMM_GPIO_CFGN_GPIO_PULL_BLEN               2
#define TLMM_GPIO_CFGN_GPIO_PULL_FLAG               HSL_RW

enum QTI_8X8X_QTI_8X8X_PIN_CONFIG_PARAM {
    QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_DISABLE,   //Disables all pull
    QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_DOWN,
    QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_BUS_HOLD,  //Weak Keepers
    QTI_8X8X_TLMM_GPIO_CFGN_GPIO_PULL_UP,
};


/* TLMM_GPIO_IN_OUTn */
#define TLMM_GPIO_IN_OUTN
#define TLMM_GPIO_IN_OUTN_OFFSET		     0xC400004
#define TLMM_GPIO_IN_OUTN_E_LENGTH		     4
#define TLMM_GPIO_IN_OUTN_E_OFFSET		     0x1000
#define TLMM_GPIO_IN_OUTN_NR_E			     80

#define GPIO_OUTE
#define TLMM_GPIO_IN_OUTN_GPIO_OUTE_BOFFSET          1
#define TLMM_GPIO_IN_OUTN_GPIO_OUTE_BLEN             1
#define TLMM_GPIO_IN_OUTN_GPIO_OUTE_FLAG             HSL_RW

#define GPIO_IN
#define TLMM_GPIO_IN_OUTN_GPIO_IN_BOFFSET           0
#define TLMM_GPIO_IN_OUTN_GPIO_IN_BLEN              1
#define TLMM_GPIO_IN_OUTN_GPIO_IN_FLAG              HSL_R

/* TLMM_CLK_GATE_EN */
#define TLMM_CLK_GATE_EN
#define TLMM_CLK_GATE_EN_OFFSET			    0xC500000
#define TLMM_CLK_GATE_EN_E_LENGTH		    4
#define TLMM_CLK_GATE_EN_E_OFFSET		    0
#define TLMM_CLK_GATE_EN_NR_E			    1

#define AHB_HCLK_EN
#define TLMM_CLK_GATE_EN_AHB_HCLK_EN_BOFFSET        2
#define TLMM_CLK_GATE_EN_AHB_HCLK_EN_BLEN           1
#define TLMM_CLK_GATE_EN_AHB_HCLK_EN_FLAG           HSL_RW

#define SUMMARY_INTR_EN
#define TLMM_CLK_GATE_EN_SUMMARY_INTR_EN_BOFFSET    1
#define TLMM_CLK_GATE_EN_SUMMARY_INTR_EN_BLEN       1
#define TLMM_CLK_GATE_EN_SUMMARY_INTR_EN_FLAG       HSL_RW

#define CRIF_READ_EN
#define TLMM_CLK_GATE_EN_CRIF_READ_EN_BOFFSET       0
#define TLMM_CLK_GATE_EN_CRIF_READ_EN_BLEN          1
#define TLMM_CLK_GATE_EN_CRIF_READ_EN_FLAG          HSL_RW

/* TLMM_HW_REVISION_NUMBER */
#define TLMM_HW_REVISION_NUMBER
#define TLMM_HW_REVISION_NUMBER_OFFSET		    0xC510010
#define TLMM_HW_REVISION_NUMBER_E_LENGTH            4
#define TLMM_HW_REVISION_NUMBER_E_OFFSET            0
#define TLMM_HW_REVISION_NUMBER_NR_E                1

#define VERSION_ID
#define TLMM_HW_REVISION_NUMBER_VERSION_ID_BOFFSET  28
#define TLMM_HW_REVISION_NUMBER_VERSION_ID_BLEN     4
#define TLMM_HW_REVISION_NUMBER_VERSION_ID_FLAG     HSL_R

#define PARTNUM
#define TLMM_HW_REVISION_NUMBER_PARTNUM_BOFFSET     12
#define TLMM_HW_REVISION_NUMBER_PARTNUM_BLEN        16
#define TLMM_HW_REVISION_NUMBER_PARTNUM_FLAG        HSL_R

#define MFG_ID
#define TLMM_HW_REVISION_NUMBER_MFG_ID_BOFFSET      1
#define TLMM_HW_REVISION_NUMBER_MFG_ID_BLEN         11
#define TLMM_HW_REVISION_NUMBER_MFG_ID_FLAG         HSL_R

#define START_BIT
#define TLMM_HW_REVISION_NUMBER_START_BIT_BOFFSET   0
#define TLMM_HW_REVISION_NUMBER_START_BIT_BLEN      1
#define TLMM_HW_REVISION_NUMBER_START_BIT_FLAG      HSL_R


/****************************************************************************
 *
 *  2) PINs Functions Selection  GPIO_CFG[5:2] (FUNC_SEL)
 *
 ****************************************************************************/
/*GPIO*/
#define QTI_8X8X_PIN_FUNC_GPIO0  0
#define QTI_8X8X_PIN_FUNC_GPIO1  0
#define QTI_8X8X_PIN_FUNC_GPIO2  0
#define QTI_8X8X_PIN_FUNC_GPIO3  0
#define QTI_8X8X_PIN_FUNC_GPIO4  0
#define QTI_8X8X_PIN_FUNC_GPIO5  0
#define QTI_8X8X_PIN_FUNC_GPIO6  0
#define QTI_8X8X_PIN_FUNC_GPIO7  0
#define QTI_8X8X_PIN_FUNC_GPIO8  0
#define QTI_8X8X_PIN_FUNC_GPIO9  0
#define QTI_8X8X_PIN_FUNC_GPIO10 0
#define QTI_8X8X_PIN_FUNC_GPIO11 0
#define QTI_8X8X_PIN_FUNC_GPIO12 0
#define QTI_8X8X_PIN_FUNC_GPIO13 0
#define QTI_8X8X_PIN_FUNC_GPIO14 0
#define QTI_8X8X_PIN_FUNC_GPIO15 0
#define QTI_8X8X_PIN_FUNC_GPIO16 0
#define QTI_8X8X_PIN_FUNC_GPIO17 0
#define QTI_8X8X_PIN_FUNC_GPIO18 0
#define QTI_8X8X_PIN_FUNC_GPIO19 0
#define QTI_8X8X_PIN_FUNC_GPIO20 0
#define QTI_8X8X_PIN_FUNC_GPIO21 0

/*MINIMUM CONCURRENCY SET FUNCTION*/
#define QTI_8X8X_PIN_FUNC_INTN_WOL         1
#define QTI_8X8X_PIN_FUNC_INTN             1
#define QTI_8X8X_PIN_FUNC_P0_LED_0         1
#define QTI_8X8X_PIN_FUNC_P1_LED_0         1
#define QTI_8X8X_PIN_FUNC_P2_LED_0         1
#define QTI_8X8X_PIN_FUNC_P3_LED_0         1
#define QTI_8X8X_PIN_FUNC_PPS_IN           1
#define QTI_8X8X_PIN_FUNC_TOD_IN           1
#define QTI_8X8X_PIN_FUNC_RTC_REFCLK_IN    1
#define QTI_8X8X_PIN_FUNC_P0_PPS_OUT       1
#define QTI_8X8X_PIN_FUNC_P1_PPS_OUT       1
#define QTI_8X8X_PIN_FUNC_P2_PPS_OUT       1
#define QTI_8X8X_PIN_FUNC_P3_PPS_OUT       1
#define QTI_8X8X_PIN_FUNC_P0_TOD_OUT       1
#define QTI_8X8X_PIN_FUNC_P0_CLK125_TDI    1
#define QTI_8X8X_PIN_FUNC_P0_SYNC_CLKO_PTP 1
#define QTI_8X8X_PIN_FUNC_P0_LED_1         1
#define QTI_8X8X_PIN_FUNC_P1_LED_1         1
#define QTI_8X8X_PIN_FUNC_P2_LED_1         1
#define QTI_8X8X_PIN_FUNC_P3_LED_1         1
#define QTI_8X8X_PIN_FUNC_MDC_M            1
#define QTI_8X8X_PIN_FUNC_MDO_M            1

/*ALT FUNCTION K*/
#define QTI_8X8X_PIN_FUNC_EVENT_TRG_I        2
#define QTI_8X8X_PIN_FUNC_P0_EVENT_TRG_O     2
#define QTI_8X8X_PIN_FUNC_P1_EVENT_TRG_O     2
#define QTI_8X8X_PIN_FUNC_P2_EVENT_TRG_O     2
#define QTI_8X8X_PIN_FUNC_P3_EVENT_TRG_O     2
#define QTI_8X8X_PIN_FUNC_P1_TOD_OUT         2
#define QTI_8X8X_PIN_FUNC_P1_CLK125_TDI      2
#define QTI_8X8X_PIN_FUNC_P1_SYNC_CLKO_PTP   2
#define QTI_8X8X_PIN_FUNC_P0_INTN_WOL        2
#define QTI_8X8X_PIN_FUNC_P1_INTN_WOL        2
#define QTI_8X8X_PIN_FUNC_P2_INTN_WOL        2
#define QTI_8X8X_PIN_FUNC_P3_INTN_WOL        2

/*ALT FUNCTION L*/
#define QTI_8X8X_PIN_FUNC_P2_TOD_OUT         3
#define QTI_8X8X_PIN_FUNC_P2_CLK125_TDI      3
#define QTI_8X8X_PIN_FUNC_P2_SYNC_CLKO_PTP   3

/*ALT FUNCTION M*/
#define QTI_8X8X_PIN_FUNC_P3_TOD_OUT         4
#define QTI_8X8X_PIN_FUNC_P3_CLK125_TDI      4
#define QTI_8X8X_PIN_FUNC_P3_SYNC_CLKO_PTP   4

/*ALT FUNCTION N*/
#define QTI_8X8X_PIN_FUNC_P0_LED_2           3
#define QTI_8X8X_PIN_FUNC_P1_LED_2           2
#define QTI_8X8X_PIN_FUNC_P2_LED_2           2
#define QTI_8X8X_PIN_FUNC_P3_LED_2           3

/*ALT FUNCTION O*/


/*ALT FUNCTION DEBUG BUS OUT*/
#define QTI_8X8X_PIN_FUNC_DBG_OUT_CLK        2
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT0       2
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT1       2
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT12      2
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT13      2
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT2       3
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT3       4
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT4       3
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT5       3
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT6       3
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT7       5
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT8       5
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT9       5
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT10      3
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT11      3
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT14      2
#define QTI_8X8X_PIN_FUNC_DBG_BUS_OUT15      2


/****************************************************************************
 *
 *  2) PINs Functions Selection  GPIO_CFG[5:2] (FUNC_SEL)
 *
 ****************************************************************************/
struct qti_8x8x_pinctrl_setting_mux {
	u32 pin;
	u32 func;
};

struct qti_8x8x_pinctrl_setting_configs {
	u32 pin;
	u32 num_configs;
	u64 *configs;
};

enum qti_8x8x_pin_config_param {
	QTI_8X8X_PIN_CONFIG_BIAS_BUS_HOLD,
	QTI_8X8X_PIN_CONFIG_BIAS_DISABLE,
	QTI_8X8X_PIN_CONFIG_BIAS_HIGH_IMPEDANCE,
	QTI_8X8X_PIN_CONFIG_BIAS_PULL_DOWN,
	QTI_8X8X_PIN_CONFIG_BIAS_PULL_PIN_DEFAULT,
	QTI_8X8X_PIN_CONFIG_BIAS_PULL_UP,
	QTI_8X8X_PIN_CONFIG_DRIVE_OPEN_DRAIN,
	QTI_8X8X_PIN_CONFIG_DRIVE_OPEN_SOURCE,
	QTI_8X8X_PIN_CONFIG_DRIVE_PUSH_PULL,
	QTI_8X8X_PIN_CONFIG_DRIVE_STRENGTH,
	QTI_8X8X_PIN_CONFIG_DRIVE_STRENGTH_UA,
	QTI_8X8X_PIN_CONFIG_INPUT_DEBOUNCE,
	QTI_8X8X_PIN_CONFIG_INPUT_ENABLE,
	QTI_8X8X_PIN_CONFIG_INPUT_SCHMITT,
	QTI_8X8X_PIN_CONFIG_INPUT_SCHMITT_ENABLE,
	QTI_8X8X_PIN_CONFIG_LOW_POWER_MODE,
	QTI_8X8X_PIN_CONFIG_OUTPUT_ENABLE,
	QTI_8X8X_PIN_CONFIG_OUTPUT,
	QTI_8X8X_PIN_CONFIG_POWER_SOURCE,
	QTI_8X8X_PIN_CONFIG_SLEEP_HARDWARE_STATE,
	QTI_8X8X_PIN_CONFIG_SLEW_RATE,
	QTI_8X8X_PIN_CONFIG_SKEW_DELAY,
	QTI_8X8X_PIN_CONFIG_PERSIST_STATE,
	QTI_8X8X_PIN_CONFIG_END = 0x7F,
	QTI_8X8X_PIN_CONFIG_MAX = 0xFF,
};

enum qti_8x8x_pinctrl_map_type {
	QTI_8X8X_PIN_MAP_TYPE_INVALID,
	QTI_8X8X_PIN_MAP_TYPE_DUMMY_STATE,
	QTI_8X8X_PIN_MAP_TYPE_MUX_GROUP,
	QTI_8X8X_PIN_MAP_TYPE_CONFIGS_PIN,
	QTI_8X8X_PIN_MAP_TYPE_CONFIGS_GROUP,
};

struct qti_8x8x_pinctrl_setting {
	enum qti_8x8x_pinctrl_map_type type;
	union {
		struct qti_8x8x_pinctrl_setting_mux mux;
		struct qti_8x8x_pinctrl_setting_configs configs;
	} data;
};

#define QTI_8X8X_PIN_SETTING_MUX(pin_id, function)		\
	{							\
		.type = QTI_8X8X_PIN_MAP_TYPE_MUX_GROUP,		\
		.data.mux = {					\
			.pin = pin_id,				\
			.func = function			\
		},						\
	}

#define QTI_8X8X_PIN_SETTING_CONFIG(pin_id, cfgs)		\
	{							\
		.type = QTI_8X8X_PIN_MAP_TYPE_CONFIGS_PIN,	\
		.data.configs = {				\
			.pin = pin_id,				\
			.configs = cfgs,			\
			.num_configs = ARRAY_SIZE(cfgs)		\
		},						\
	}

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#define QTI_8X8X_SWITCH_CORE_CLK		"switch_clk"
#define QTI_8X8X_APB_BRIDGE_CLK			"apb_clk"

#define QTI_8X8X_MAC0_TX_CLK			"m0_tx_clk"
#define QTI_8X8X_MAC0_TX_UNIPHY1_CLK		"m0_tx_srds1_clk"

#define QTI_8X8X_MAC0_RX_CLK			"m0_rx_clk"
#define QTI_8X8X_MAC0_RX_UNIPHY1_CLK		"m0_rx_srds1_clk"

#define QTI_8X8X_MAC1_TX_CLK			"m1_tx_clk"
#define QTI_8X8X_MAC1_GEPHY0_TX_CLK		"m1_gp0_tx_clk"
#define QTI_8X8X_MAC1_UNIPHY1_CH0_RX_CLK	"m1_srds1_ch0_rx_clk"
#define QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_RX_CLK	"m1_srds1_ch0_xgmii_rx_clk"

#define QTI_8X8X_MAC1_RX_CLK			"m1_rx_clk"
#define QTI_8X8X_MAC1_GEPHY0_RX_CLK		"m1_gp0_rx_clk"
#define QTI_8X8X_MAC1_UNIPHY1_CH0_TX_CLK	"m1_srds1_ch0_tx_clk"
#define QTI_8X8X_MAC1_UNIPHY1_CH0_XGMII_TX_CLK	"m1_srds1_ch0_xgmii_tx_clk"

#define QTI_8X8X_MAC2_TX_CLK			"m2_tx_clk"
#define QTI_8X8X_MAC2_GEPHY1_TX_CLK		"m2_gp1_tx_clk"
#define QTI_8X8X_MAC2_UNIPHY1_CH1_RX_CLK	"m2_srds1_ch1_rx_clk"
#define QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_RX_CLK	"m2_srds1_ch1_xgmii_rx_clk"

#define QTI_8X8X_MAC2_RX_CLK			"m2_rx_clk"
#define QTI_8X8X_MAC2_GEPHY1_RX_CLK		"m2_gp1_rx_clk"
#define QTI_8X8X_MAC2_UNIPHY1_CH1_TX_CLK	"m2_srds1_ch1_tx_clk"
#define QTI_8X8X_MAC2_UNIPHY1_CH1_XGMII_TX_CLK	"m2_srds1_ch1_xgmii_tx_clk"

#define QTI_8X8X_MAC3_TX_CLK			"m3_tx_clk"
#define QTI_8X8X_MAC3_GEPHY2_TX_CLK		"m3_gp2_tx_clk"
#define QTI_8X8X_MAC3_UNIPHY1_CH2_RX_CLK	"m3_srds1_ch2_rx_clk"
#define QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_RX_CLK	"m3_srds1_ch2_xgmii_rx_clk"

#define QTI_8X8X_MAC3_RX_CLK			"m3_rx_clk"
#define QTI_8X8X_MAC3_GEPHY2_RX_CLK		"m3_gp2_rx_clk"
#define QTI_8X8X_MAC3_UNIPHY1_CH2_TX_CLK	"m3_srds1_ch2_tx_clk"
#define QTI_8X8X_MAC3_UNIPHY1_CH2_XGMII_TX_CLK	"m3_srds1_ch2_xgmii_tx_clk"

#define QTI_8X8X_MAC4_TX_CLK			"m4_tx_clk"
#define QTI_8X8X_MAC4_GEPHY3_TX_CLK		"m4_gp3_tx_clk"
#define QTI_8X8X_MAC4_UNIPHY1_CH3_RX_CLK	"m4_srds1_ch3_rx_clk"
#define QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_RX_CLK	"m4_srds1_ch3_xgmii_rx_clk"

#define QTI_8X8X_MAC4_RX_CLK			"m4_rx_clk"
#define QTI_8X8X_MAC4_GEPHY3_RX_CLK		"m4_gp3_rx_clk"
#define QTI_8X8X_MAC4_UNIPHY1_CH3_TX_CLK	"m4_srds1_ch3_tx_clk"
#define QTI_8X8X_MAC4_UNIPHY1_CH3_XGMII_TX_CLK	"m4_srds1_ch3_xgmii_tx_clk"

#define QTI_8X8X_MAC5_TX_CLK			"m5_tx_clk"
#define QTI_8X8X_MAC5_TX_UNIPHY0_CLK		"m5_tx_srds0_clk"
#define QTI_8X8X_MAC5_TX_SRDS0_CLK_SRC		"m5_tx_srds0_clk_src"

#define QTI_8X8X_MAC5_RX_CLK			"m5_rx_clk"
#define QTI_8X8X_MAC5_RX_UNIPHY0_CLK		"m5_rx_srds0_clk"
#define QTI_8X8X_MAC5_RX_SRDS0_CLK_SRC		"m5_rx_srds0_clk_src"

#define QTI_8X8X_SEC_CTRL_CLK			"sec_ctrl_clk"
#define QTI_8X8X_SEC_CTRL_SENSE_CLK		"sec_ctrl_sense_clk"

#define QTI_8X8X_SRDS0_SYS_CLK			"srds0_sys_clk"
#define QTI_8X8X_SRDS1_SYS_CLK			"srds1_sys_clk"
#define QTI_8X8X_GEPHY0_SYS_CLK			"gp0_sys_clk"
#define QTI_8X8X_GEPHY1_SYS_CLK			"gp1_sys_clk"
#define QTI_8X8X_GEPHY2_SYS_CLK			"gp2_sys_clk"
#define QTI_8X8X_GEPHY3_SYS_CLK			"gp3_sys_clk"

#define QTI_8X8X_AHB_CLK			"ahb_clk"
#define QTI_8X8X_SEC_CTRL_AHB_CLK		"sec_ctrl_ahb_clk"
#define QTI_8X8X_TLMM_CLK			"tlmm_clk"
#define QTI_8X8X_TLMM_AHB_CLK			"tlmm_ahb_clk"
#define QTI_8X8X_CNOC_AHB_CLK			"cnoc_ahb_clk"
#define QTI_8X8X_MDIO_AHB_CLK			"mdio_ahb_clk"
#define QTI_8X8X_MDIO_MASTER_AHB_CLK		"mdio_master_ahb_clk"

#define QTI_8X8X_GLOBAL_RST			"global_rst"
#define QTI_8X8X_UNIPHY_XPCS_RST		"xpcs_rst"
#define QTI_8X8X_GEPHY_DSP_HW_RST		"dsp_hw_rst"
#define QTI_8X8X_GEPHY_P3_MDC_SW_RST		"p3_mdc_sw_rst"
#define QTI_8X8X_GEPHY_P2_MDC_SW_RST		"p2_mdc_sw_rst"
#define QTI_8X8X_GEPHY_P1_MDC_SW_RST		"p1_mdc_sw_rst"
#define QTI_8X8X_GEPHY_P0_MDC_SW_RST		"p0_mdc_sw_rst"



typedef enum {
	QTI_8X8X_P_XO,
	QTI_8X8X_P_UNIPHY0_RX,
	QTI_8X8X_P_UNIPHY0_TX,
	QTI_8X8X_P_UNIPHY1_RX,
	QTI_8X8X_P_UNIPHY1_TX,
	QTI_8X8X_P_UNIPHY1_RX312P5M,
	QTI_8X8X_P_UNIPHY1_TX312P5M,
	QTI_8X8X_P_MAX,
} qti_8x8x_clk_parent_t;

struct qti_8x8x_clk_data {
	unsigned long rate;
	unsigned int rcg_val;
	unsigned int cdiv_val;
	unsigned int cbc_val;
};

struct qti_8x8x_parent_data {
	unsigned long prate;		/* RCG input clock rate */
	qti_8x8x_clk_parent_t parent;	/* RCG parent clock id */
	int cfg;			/* RCG clock src value */
};

struct clk_lookup {
	unsigned int rcg;
	unsigned int cdiv;
	unsigned int cbc;
	unsigned int rst_bit;
	const char *clk_name;
	const unsigned long *support_rate;
	unsigned int num_rate;
	const struct qti_8x8x_parent_data *pdata;
	unsigned int num_parent;
};

#define CLK_LOOKUP(_rcg, _cdiv, _cbc, _rst_bit, _clk_name,		\
		_rate, _num_rate, _pdata, _num_parent)			\
{									\
	.rcg = _rcg,							\
	.cdiv = _cdiv,							\
	.cbc = _cbc,							\
	.rst_bit = _rst_bit,						\
	.clk_name = _clk_name,						\
	.support_rate = _rate,						\
	.num_rate = _num_rate,						\
	.pdata = _pdata,						\
	.num_parent = _num_parent,					\
}

#define QTI_8X8X_CLK_TYPE_EPHY			BIT(0)
#define QTI_8X8X_CLK_TYPE_UNIPHY		BIT(1)
#define QTI_8X8X_CLK_TYPE_MAC			BIT(2)

#define UQXGMII_SPEED_2500M_CLK			312500000
#define UQXGMII_SPEED_1000M_CLK			125000000
#define UQXGMII_SPEED_100M_CLK			25000000
#define UQXGMII_SPEED_10M_CLK			2500000
#define UQXGMII_XPCS_SPEED_2500M_CLK		78125000
#define QTI_8X8X_AHB_CLK_RATE_104P17M		104160000
#define QTI_8X8X_SYS_CLK_RATE_25M		25000000
#define QTI_8X8X_XO_CLK_RATE_50M		50000000

#define QTI_8X8X_CLK_BASE_REG			0x0c800000
#define QTI_8X8X_CLK_MUX_SEL			0x300
#define QTI_8X8X_UNIPHY0_MUX_SEL_MASK		BITS_MASK(0, 2)
#define QTI_8X8X_UNIPHY0_SEL_MAC5		0x3
#define QTI_8X8X_UNIPHY0_SEL_MAC4		0

#define RCGR_CMD_ROOT_OFF		BIT(31)
#define RCGR_CMD_UPDATE			BIT(0)
#define RCGR_SRC_SEL			BITS_MASK(8, 3)
#define RCGR_SRC_SEL_SHIFT		8
#define RCGR_HDIV			BITS_MASK(0, 5)
#define RCGR_HDIV_SHIFT			0
#define RCGR_DIV_BYPASS			0
#define RCGR_DIV_MAX			0x1f
#define CDIVR_DIVIDER_10		9	/* CDIVR divided by N + 1 */
#define CDIVR_DIVIDER			BITS_MASK(0, 4)
#define CDIVR_DIVIDER_SHIFT		0
#define CBCR_CLK_OFF			BIT(31)
#define CBCR_CLK_RESET			BIT(2)
#define CBCR_CLK_ENABLE			BIT(0)


/* work mode */
#define WORK_MODE
#define WORK_MODE_ID                                    0
#define WORK_MODE_OFFSET                                0xC90F030
#define WORK_MODE_E_LENGTH                              4
#define WORK_MODE_E_OFFSET                              0
#define WORK_MODE_NR_E                                  1

/* port5 sel */
#define WORK_MODE_PORT5_SEL
#define WORK_MODE_PORT5_SEL_BOFFSET                     5
#define WORK_MODE_PORT5_SEL_BLEN                        1
#define WORK_MODE_PORT5_SEL_FLAG                        HSL_RW

/* phy3 sel1 */
#define WORK_MODE_PHY3_SEL1
#define WORK_MODE_PHY3_SEL1_BOFFSET                     4
#define WORK_MODE_PHY3_SEL1_BLEN                        1
#define WORK_MODE_PHY3_SEL1_FLAG                        HSL_RW

/* phy3 sel0 */
#define WORK_MODE_PHY3_SEL0
#define WORK_MODE_PHY3_SEL0_BOFFSET                     3
#define WORK_MODE_PHY3_SEL0_BLEN                        1
#define WORK_MODE_PHY3_SEL0_FLAG                        HSL_RW

/* phy2 sel */
#define WORK_MODE_PHY2_SEL
#define WORK_MODE_PHY2_SEL_BOFFSET                      2
#define WORK_MODE_PHY2_SEL_BLEN                         1
#define WORK_MODE_PHY2_SEL_FLAG                         HSL_RW

/* phy1 sel */
#define WORK_MODE_PHY1_SEL
#define WORK_MODE_PHY1_SEL_BOFFSET                      1
#define WORK_MODE_PHY1_SEL_BLEN                         1
#define WORK_MODE_PHY1_SEL_FLAG                         HSL_RW

/* phy0 sel */
#define WORK_MODE_PHY0_SEL
#define WORK_MODE_PHY0_SEL_BOFFSET                      0
#define WORK_MODE_PHY0_SEL_BLEN                         1
#define WORK_MODE_PHY0_SEL_FLAG                         HSL_RW

#define QTI_8X8X_WORK_MODE_MASK \
	(BITSM(WORK_MODE_PHY0_SEL_BOFFSET, WORK_MODE_PORT5_SEL_BOFFSET + 1))

typedef enum {
	QTI_8X8X_SWITCH_MODE =
		(BIT(WORK_MODE_PHY3_SEL1_BOFFSET)),
	QTI_8X8X_SWITCH_BYPASS_PORT5_MODE =
		(BIT(WORK_MODE_PORT5_SEL_BOFFSET)),
	QTI_8X8X_PHY_UQXGMII_MODE =
		(BIT(WORK_MODE_PORT5_SEL_BOFFSET) |
		 BIT(WORK_MODE_PHY3_SEL0_BOFFSET) |
		 BIT(WORK_MODE_PHY2_SEL_BOFFSET) |
		 BIT(WORK_MODE_PHY1_SEL_BOFFSET) |
		 BIT(WORK_MODE_PHY0_SEL_BOFFSET)),
	QTI_8X8X_PHY_SGMII_UQXGMII_MODE =
		(BIT(WORK_MODE_PORT5_SEL_BOFFSET) |
		 BIT(WORK_MODE_PHY2_SEL_BOFFSET) |
		 BIT(WORK_MODE_PHY1_SEL_BOFFSET) |
		 BIT(WORK_MODE_PHY0_SEL_BOFFSET)),
	QTI_8X8X_WORK_MODE_MAX,
} qti_8x8x_work_mode_t;


#define EPHY_CFG_OFFSET				0xC90F018
#define EPHY_CFG_EPHY0_ADDR_BOFFSET		0
#define EPHY_CFG_EPHY1_ADDR_BOFFSET		5
#define EPHY_CFG_EPHY2_ADDR_BOFFSET		10
#define EPHY_CFG_EPHY3_ADDR_BOFFSET		15

#define SERDES_CFG_OFFSET			0xC90F014
#define SERDES_CFG_S0_ADDR_BOFFSET		0
#define SERDES_CFG_S1_ADDR_BOFFSET		5
#define SERDES_CFG_S1_XPCS_ADDR_BOFFSET		10

#define QTI_8X8X_UNIPHY_SGMII_0                 0
#define QTI_8X8X_UNIPHY_SGMII_1			1
#define QTI_8X8X_UNIPHY_XPCS                    2

/*UNIPHY MII registers*/
#define QTI_8X8X_UNIPHY_PLL_POWER_ON_AND_RESET          0

/*UNIPHY MII register field*/
#define QTI_8X8X_UNIPHY_ANA_SOFT_RESET                  0
#define QTI_8X8X_UNIPHY_ANA_SOFT_RELEASE                0x40

/*UNIPHY MMD*/
#define QTI_8X8X_UNIPHY_MMD1                            0x1
#define QTI_8X8X_UNIPHY_MMD3				0x3
#define QTI_8X8X_UNIPHY_MMD26                           0x1a
#define QTI_8X8X_UNIPHY_MMD27                           0x1b
#define QTI_8X8X_UNIPHY_MMD28				0x1c
#define QTI_8X8X_UNIPHY_MMD31                           0x1f

/*UNIPHY MMD1 registers*/
#define QTI_8X8X_UNIPHY_MMD1_CDA_CONTROL1               0x20
#define QTI_8X8X_UNIPHY_MMD1_CALIBRATION4               0x78
#define QTI_8X8X_UNIPHY_MMD1_BYPASS_TUNING_IPG          0x189
#define QTI_8X8X_UNIPHY_MMD1_MODE_CTRL                  0x11b
#define QTI_8X8X_UNIPHY_MMD1_CHANNEL0_CFG               0x120
#define QTI_8X8X_UNIPHY_MMD1_GMII_DATAPASS_SEL          0x180
#define QTI_8X8X_UNIPHY_MMD1_USXGMII_RESET              0x18c

/*UNIPHY MMD1 register field*/
#define QTI_8X8X_UNIPHY_MMD1_BYPASS_TUNING_IPG_EN       0x0fff
#define QTI_8X8X_UNIPHY_MMD1_XPCS_MODE                  0x1000
#define QTI_8X8X_UNIPHY_MMD1_SGMII_MODE                 0x400
#define QTI_8X8X_UNIPHY_MMD1_SGMII_PLUS_MODE            0x800
#define QTI_8X8X_UNIPHY_MMD1_1000BASE_X                 0x0
#define QTI_8X8X_UNIPHY_MMD1_SGMII_PHY_MODE             0x10
#define QTI_8X8X_UNIPHY_MMD1_SGMII_MAC_MODE             0x20
#define QTI_8X8X_UNIPHY_MMD1_SGMII_MODE_CTRL_MASK       0x1f70
#define QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_MASK       0xe
#define QTI_8X8X_UNIPHY_MMD1_CH0_AUTONEG_ENABLE         0x0
#define QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_ENABLE           0x8
#define QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_1G         0x4
#define QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_100M       0x2
#define QTI_8X8X_UNIPHY_MMD1_CH0_FORCE_SPEED_10M        0x0
#define QTI_8X8X_UNIPHY_MMD1_DATAPASS_MASK              0x1
#define QTI_8X8X_UNIPHY_MMD1_DATAPASS_USXGMII           0x1
#define QTI_8X8X_UNIPHY_MMD1_DATAPASS_SGMII             0x0
#define QTI_8X8X_UNIPHY_MMD1_CALIBRATION_DONE           0x80
#define QTI_8X8X_UNIPHY_MMD1_SGMII_FUNC_RESET           0x10
#define QTI_8X8X_UNIPHY_MMD1_SGMII_ADPT_RESET           0x800
#define QTI_8X8X_UNIPHY_MMD1_SSCG_ENABLE                0x8

/*UNIPHY MMD3 registers*/
#define QTI_8X8X_UNIPHY_MMD3_PCS_CTRL2			0x7
#define QTI_8X8X_UNIPHY_MMD3_AN_LP_BASE_ABL2            0x14
#define QTI_8X8X_UNIPHY_MMD3_10GBASE_R_PCS_STATUS1      0x20
#define QTI_8X8X_UNIPHY_MMD3_DIG_CTRL1                  0x8000
#define QTI_8X8X_UNIPHY_MMD3_EEE_MODE_CTRL              0x8006
#define QTI_8X8X_UNIPHY_MMD3_VR_RPCS_TPC		0x8007
#define QTI_8X8X_UNIPHY_MMD3_EEE_TX_TIMER               0x8008
#define QTI_8X8X_UNIPHY_MMD3_EEE_RX_TIMER               0x8009
#define QTI_8X8X_UNIPHY_MMD3_MII_AM_INTERVAL            0x800a
#define QTI_8X8X_UNIPHY_MMD3_EEE_MODE_CTRL1             0x800b

/*UNIPHY MMD3 register field*/
#define QTI_8X8X_UNIPHY_MMD3_PCS_TYPE_10GBASE_R         0
#define QTI_8X8X_UNIPHY_MMD3_10GBASE_R_UP               0x1000
#define QTI_8X8X_UNIPHY_MMD3_USXGMII_EN                 0x200
#define QTI_8X8X_UNIPHY_MMD3_QXGMII_EN                  0x1400
#define QTI_8X8X_UNIPHY_MMD3_MII_AM_INTERVAL_VAL        0x6018
#define QTI_8X8X_UNIPHY_MMD3_XPCS_SOFT_RESET            0x8000
#define QTI_8X8X_UNIPHY_MMD3_XPCS_EEE_CAP               0x40
#define QTI_8X8X_UNIPHY_MMD3_EEE_RES_REGS               0x100
#define QTI_8X8X_UNIPHY_MMD3_EEE_SIGN_BIT_REGS          0x40
#define QTI_8X8X_UNIPHY_MMD3_EEE_EN                     0x3
#define QTI_8X8X_UNIPHY_MMD3_EEE_TSL_REGS               0xa
#define QTI_8X8X_UNIPHY_MMD3_EEE_TLU_REGS               0xc0
#define QTI_8X8X_UNIPHY_MMD3_EEE_TWL_REGS               0x1600
#define QTI_8X8X_UNIPHY_MMD3_EEE_100US_REG_REGS         0xc8
#define QTI_8X8X_UNIPHY_MMD3_EEE_RWR_REG_REGS           0x1c00
#define QTI_8X8X_UNIPHY_MMD3_EEE_TRANS_LPI_MODE         0x1
#define QTI_8X8X_UNIPHY_MMD3_EEE_TRANS_RX_LPI_MODE      0x100
#define QTI_8X8X_UNIPHY_MMD3_USXG_FIFO_RESET            0x400

/*UNIPHY MMD26 27 28 31 registers*/
#define QTI_8X8X_UNIPHY_MMD_MII_CTRL                    0
#define QTI_8X8X_UNIPHY_MMD_MII_DIG_CTRL                0x8000
#define QTI_8X8X_UNIPHY_MMD_MII_AN_INT_MSK              0x8001
#define QTI_8X8X_UNIPHY_MMD_MII_ERR_SEL                 0x8002
#define QTI_8X8X_UNIPHY_MMD_MII_XAUI_MODE_CTRL          0x8004

/*UNIPHY MMD26 27 28 31 register field*/
#define QTI_8X8X_UNIPHY_MMD_AN_COMPLETE_INT             0x1
#define QTI_8X8X_UNIPHY_MMD_MII_4BITS_CTRL		0x0
#define QTI_8X8X_UNIPHY_MMD_TX_CONFIG_CTRL              0x8
#define QTI_8X8X_UNIPHY_MMD_MII_AN_ENABLE               0x1000
#define QTI_8X8X_UNIPHY_MMD_MII_AN_RESTART              0x200
#define QTI_8X8X_UNIPHY_MMD_MII_AN_COMPLETE_INT         0x1
#define QTI_8X8X_UNIPHY_MMD_USXG_FIFO_RESET             0x20
#define QTI_8X8X_UNIPHY_MMD_XPC_SPEED_MASK		0x2060
#define QTI_8X8X_UNIPHY_MMD_XPC_SPEED_2500              0x20
#define QTI_8X8X_UNIPHY_MMD_XPC_SPEED_1000              0x40
#define QTI_8X8X_UNIPHY_MMD_XPC_SPEED_100               0x2000
#define QTI_8X8X_UNIPHY_MMD_XPC_SPEED_10                0
#define QTI_8X8X_UNIPHY_MMD_TX_IPG_CHECK_DISABLE        0x1

#define UNIPHY_CLK_RATE_25M				25000000
#define UNIPHY_CLK_RATE_50M				50000000
#define UNIPHY_CLK_RATE_125M				125000000
#define UNIPHY_CLK_RATE_312M				312500000
#define UNIPHY_DEFAULT_RATE				UNIPHY_CLK_RATE_125M

/** Bit manipulation macros */
#ifndef BITSM
#define BITSM(_s, _n)			(((1UL << (_n)) - 1) << _s)
#endif
#define BITS_MASK(_s, _n)		(((1UL << (_n)) - 1) << _s)

#define SW_BIT_MASK_U32(nr)		(~(0xFFFFFFFF << (nr)))

#define SW_FIELD_MASK_U32(offset, len) \
	((SW_BIT_MASK_U32(len) << (offset)))

#define SW_FIELD_MASK_NOT_U32(offset,len) \
	(~(SW_BIT_MASK_U32(len) << (offset)))

#define SW_FIELD_2_REG(field_val, bit_offset) \
	(field_val << (bit_offset) )

#define SW_REG_2_FIELD(reg_val, bit_offset, field_len) \
	(((reg_val) >> (bit_offset)) & ((1 << (field_len)) - 1))

#define SW_FIELD_GET_BY_REG_U32(reg_value, field_value, \
		bit_offset, field_len)\
	do { \
		(field_value) = \
		(((reg_value) >> (bit_offset)) & SW_BIT_MASK_U32( \
			field_len)); \
	} while (0)

#define SW_REG_SET_BY_FIELD_U32(reg_value, field_value, bit_offset, \
		field_len)\
	do { \
		(reg_value) = \
		(((reg_value) & SW_FIELD_MASK_NOT_U32( \
		 (bit_offset),(field_len))) \
		 | (((field_value) & SW_BIT_MASK_U32( \
		 field_len)) << (bit_offset)));\
	} while (0)

#define SW_GET_FIELD_BY_REG(reg, field, field_value, reg_value) \
	SW_FIELD_GET_BY_REG_U32(reg_value, field_value, \
			reg##_##field##_BOFFSET, \
			reg##_##field##_BLEN)

#define SW_SET_REG_BY_FIELD(reg, field, field_value, reg_value) \
	SW_REG_SET_BY_FIELD_U32(reg_value, field_value, \
			reg##_##field##_BOFFSET, \
			reg##_##field##_BLEN)

#define QTI_8X8X_REG_ENTRY_GET(phydev, reg, index, value) \
	*((u32 *) value) = qti_8x8x_mii_read(phydev, \
			reg##_OFFSET + ((u32)index) * reg##_E_OFFSET);

#define QTI_8X8X_REG_ENTRY_SET(phydev, reg, index, value) \
	qti_8x8x_mii_write(phydev, reg##_OFFSET + ((u32)index) * \
			reg##_E_OFFSET, *((u32 *) value));

#define QTI_8X8X_REG_FIELD_GET(phydev, reg, index, field, value) \
	do { \
		qti_8x8x_reg_field_get(phydev, reg##_OFFSET + ((u32)index) * \
			       	reg##_E_OFFSET,\
				reg##_##field##_BOFFSET, \
				reg##_##field##_BLEN, (u8*)value);\
	} while (0);

#define QTI_8X8X_REG_FIELD_SET(phydev, reg, index, field, value) \
	do { \
		qti_8x8x_reg_field_set(phydev, reg##_OFFSET + ((u32)index) * \
			       	reg##_E_OFFSET,\
				reg##_##field##_BOFFSET, \
				reg##_##field##_BLEN, (u8*)value);\
	} while (0);

#define for_each_clear_bit(bit, addr, size) \
        for ((bit) = find_first_zero_bit((addr), (size));       \
                (bit) < (size);                                 \
                (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

/* same as for_each_clear_bit() but use bit as value to start with */
#define for_each_clear_bit_from(bit, addr, size) \
        for ((bit) = find_next_zero_bit((addr), (size), (bit)); \
                (bit) < (size);                                 \
                (bit) = find_next_zero_bit((addr), (size), (bit) + 1))

enum qti_8x8x_ports {
	DEV_8X8X_PORT_0,
	DEV_8X8X_PORT_1,
	DEV_8X8X_PORT_2,
	DEV_8X8X_PORT_3,
	DEV_8X8X_PORT_4,
	DEV_8X8X_PORT_5,
	DEV_8X8X_MAX_PORTS,
};
typedef enum {
	QTI_8X8X_UNIPHY_MAC = QTI_8X8X_UNIPHY_MMD1_SGMII_MAC_MODE,
	QTI_8X8X_UNIPHY_PHY = QTI_8X8X_UNIPHY_MMD1_SGMII_PHY_MODE,
	QTI_8X8X_UNIPHY_SGMII = QTI_8X8X_UNIPHY_MMD1_SGMII_MODE,
	QTI_8X8X_UNIPHY_SGMII_PLUS = QTI_8X8X_UNIPHY_MMD1_SGMII_PLUS_MODE,
	QTI_8X8X_UNIPHY_UQXGMII = QTI_8X8X_UNIPHY_MMD1_XPCS_MODE,
} qti_8x8x_uniphy_mode_t;

typedef enum {
	QTI_8X8X_INTERFACE_CLOCK_MAC_MODE = 0,
	QTI_8X8X_INTERFACE_CLOCK_PHY_MODE = 1,
} qti_8x8x_clock_mode_t;

typedef enum {
	QTI_8X8X_MAC_MODE_RGMII = 0,
	QTI_8X8X_MAC_MODE_GMII,
	QTI_8X8X_MAC_MODE_MII,
	QTI_8X8X_MAC_MODE_SGMII,
	QTI_8X8X_MAC_MODE_FIBER,
	QTI_8X8X_MAC_MODE_RMII,
	QTI_8X8X_MAC_MODE_SGMII_PLUS,
	QTI_8X8X_MAC_MODE_DEFAULT,
	QTI_8X8X_MAC_MODE_MAX = 0xFF,
} qti_8x8x_mac_mode_t;

typedef struct {
	qti_8x8x_mac_mode_t mac_mode;
	qti_8x8x_clock_mode_t clock_mode;
	bool auto_neg;
	u32 speed;
	bool prbs_enable;
	bool rem_phy_lpbk;
} mac_config_t;

struct qti_8x8x_port_info {
	const char * label;
	unsigned int addr;
	phy_interface_t mode;
	unsigned int speed;
	unsigned int duplex;
};

struct qti_8x8x_switch_info {
	struct qti_8x8x_port_info * port;
	unsigned int cpu_bmp;
	unsigned int lan_bmp;
	unsigned int wan_bmp;
};

void qti_8x8x_pre_init(struct phy_device *phydev);

#endif                          /* _QTI_8X8X_H_ */
