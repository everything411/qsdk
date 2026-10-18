/*
 * Copyright (c) 2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


#ifndef _AQUANTIA_PHY_H_
#define _AQUANTIA_PHY_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */
#define PHY_ID_AQ1202                                                    0x03a1b445
#define PHY_ID_AQ2104                                                    0x03a1b460
#define PHY_ID_AQR105                                                    0x03a1b4a2
#define PHY_ID_AQR405                                                    0x03a1b4b0
#define PHY_ID_AQR107                                                    0x03a1b4e1

#define AQUANTIA_MMD_AUTONEG                                             0x7
#define AQUANTIA_MMD_PHY_XS_REGISTERS                                    4
#define AQUANTIA_MMD_GLOBAL_REGISTERS                                    0x1E
#define AQUANTIA_MMD_GBE_STANDARD_REGISTERS                              0x1D
#define AQUANTIA_MMD_PCS_REGISTERS                                       0x3
#define AQUANTIA_MMD_PMA_REGISTERS                                       0x1

/* PHY Registers */
#define AQUANTIA_PHY_AN_CONTROL                                          0
#define AQUANTIA_PHY_ID1                                                 2
#define AQUANTIA_PHY_ID2                                                 3
#define AQUANTIA_PHY_AN_STATUS                                           0x1
#define AQUANTIA_PHY_AN_ADV                                              0x10
#define AQUANTIA_AUTONEG_VENDOR_PROVISION1                               0xC400
#define AQUANTIA_PHY_AN_10G_ADV                                          0x20
#define AQUANTIA_REG_AUTONEG_VENDOR_STATUS                               0xC800
#define AQUANTIA_AUTONEG_TRANSMIT_VENDOR_INTR_MASK                       0xD401
#define AQUANTIA_GLOBAL_INTR_STANDARD_MASK                               0xff00
#define AQUANTIA_GLOBAL_INTR_VENDOR_MASK                                 0xff01
#define AQUANTIA_PHY_XS_USX_TRANSMIT                                     0xc441
#define AQUANTIA_AUTONEG_ADVERTISEMENT_REGISTER                          0x10
#define AQUANTIA_AUTONEG_10GBASE_T_CONTROL_REGISTER                      0x20
#define AQUANTIA_GLOBAL_RESERVED_PROVISIONING6                           0xC475
#define AQUANTIA_PHY_XS_TRANAMIT_RESERVED_VENDOR_PROVISION5              0xC444
#define AQUANTIA_RESERVED_VENDOR_PROVISIONING1                           0xC410
#define AQUANTIA_RESERVED_VENDOR_STATUS1                                 0xC810
#define AQUANTIA_GLOBAL_CDT_CONTROL                                      0xC470
#define AQUANTIA_GLOBAL_GENERAL_STATUS                                   0xC831
#define AQUANTIA_NORMAL_CABLE_DIAGNOSTICS                                0x10
#define AQUANTIA_CABLE_DIAGNOSTICS_STATUS                                0x8000
/*AQUANTIA PHY  LINE SIDE PACKETS COUNTER REGISTER*/
#define AQUANTIA_LINE_SIDE_TRANSMIT_GOOD_FRAME_COUNTER2                  0xc821
#define AQUANTIA_LINE_SIDE_TRANSMIT_GOOD_FRAME_COUNTER1                  0xc820
#define AQUANTIA_LINE_SIDE_TRANSMIT_ERROR_FRAME_COUNTER2                 0xc823
#define AQUANTIA_LINE_SIDE_TRANSMIT_ERROR_FRAME_COUNTER1                 0xc822
#define AQUANTIA_LINE_SIDE_RECEIVE_GOOD_FRAME_COUNTER2                   0xe813
#define AQUANTIA_LINE_SIDE_RECEIVE_GOOD_FRAME_COUNTER1                   0xe812
#define AQUANTIA_LINE_SIDE_RECEIVE_ERROR_FRAME_COUNTER2                  0xe815
#define AQUANTIA_LINE_SIDE_RECEIVE_ERROR_FRAME_COUNTER1                  0xe814
/*AQUANTIA PHY  SYSTEM SIDE PACKETS COUNTER REGISTER*/
#define AQUANTIA_SYSTEM_SIDE_TRANSMIT_GOOD_FRAME_COUNTER2                0xc861
#define AQUANTIA_SYSTEM_SIDE_TRANSMIT_GOOD_FRAME_COUNTER1                0xc860
#define AQUANTIA_SYSTEM_SIDE_TRANSMIT_ERROR_FRAME_COUNTER2               0xc863
#define AQUANTIA_SYSTEM_SIDE_TRANSMIT_ERROR_FRAME_COUNTER1               0xc862
#define AQUANTIA_SYSTEM_SIDE_RECEIVE_GOOD_FRAME_COUNTER2                 0xe861
#define AQUANTIA_SYSTEM_SIDE_RECEIVE_GOOD_FRAME_COUNTER1                 0xe860
#define AQUANTIA_SYSTEM_SIDE_RECEIVE_ERROR_FRAME_COUNTER2                0xe863
#define AQUANTIA_SYSTEM_SIDE_RECEIVE_ERROR_FRAME_COUNTER1                0xe862
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS1                                0xC800
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRA                           0x7000
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRB                           0x0700
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRC                           0x0070
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRD                           0x0007
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS2                                0xC801
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS4                                0xC803
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS6                                0xC805
#define AQUANTIA_CABLE_DIAGNOSTIC_STATUS8                                0xC807
#define AQUANTIA_PROVISIONING_LED0_STATUS                                0xc430
#define AQUANTIA_PROVISIONING_LED1_STATUS                                0xc431
#define AQUANTIA_PROVISIONING_LED2_STATUS                                0xc432
#define AQUANTIA_LINK_ACT_LED_VALUE                                      0xc0ef
#define AQUANTIA_LINK_LED_VALUE                                          0xc0e0
#define AQUANTIA_LINK_ACT_LED_DISABLE_VALUE                              0x0
#define AQUANTIA_MAGIC_FRAME_MAC0                                        0xC339
#define AQUANTIA_MAGIC_FRAME_MAC1                                        0xC33a
#define AQUANTIA_MAGIC_FRAME_MAC2                                        0xC33b
#define AQUANTIA_MAGIC_ENGINE_REGISTER1                                  0xC355
#define AQUANTIA_MAGIC_ENGINE_REGISTER2                                  0xC356
#define AQUANTIA_GLOBAL_SYS_CONFIG_FOR_10M                               0x310
#define AQUANTIA_GLOBAL_SYS_CONFIG_FOR_100M                              0x31B
#define AQUANTIA_GLOBAL_SYS_CONFIG_FOR_1000M                             0x31C
#define AQUANTIA_GLOBAL_SYS_CONFIG_FOR_2500M                             0x31D
#define AQUANTIA_GLOBAL_SYS_CONFIG_FOR_5000M                             0x31E
#define AQUANTIA_GLOBAL_SYS_CONFIG_FOR_10000M                            0x31F
/* PHY Registers Field */
#define AQUANTIA_SERDES_MODE_XFI                                         0
#define AQUANTIA_SERDES_MODE_SGMII                                       0x3
#define AQUANTIA_SERDES_MODE_OCSGMII                                     0X4
#define AQUANTIA_MAGIC_PACKETS_ENABLE                                    0x0001
#define AQUANTIA_PHY_MDIX_CONTRO_BIT                                     3
#define AQUANTIA_PHY_MDIX_AUTO                                           0
#define AQUANTIA_PHY_MDI                                                 1
#define AQUANTIA_PHY_MDIX                                                2
#define  AQUANTIA_PHY_MDIX_STATUS                                        0x0100
#define AQUANTIA_PHY_WOL_ENABLE                                          0x0040
#define AQUANTIA_PHY_REMOTE_LOOPBACK                                     0x1800
#define AQUANTIA_INTERNAL_LOOPBACK                                       0x4800
#define AQUANTIA_10M_LOOPBACK                                            0x0006
#define AQUANTIA_100M_LOOPBACK                                           0x0001
#define AQUANTIA_1000M_LOOPBACK                                          0x0002
#define AQUANTIA_10000M_LOOPBACK                                         0x0003
#define AQUANTIA_2500M_LOOPBACK                                          0x0004
#define AQUANTIA_5000M_LOOPBACK                                          0x0005
#define AQUANTIA_PHY_RX_FLOWCTRL_STATUS                                  0x0002
#define AQUANTIA_PHY_TX_FLOWCTRL_STATUS                                  0x0001
#define AQUANTIA_ALL_SPEED_LOOPBACK (AQUANTIA_10M_LOOPBACK | AQUANTIA_100M_LOOPBACK |\
    AQUANTIA_1000M_LOOPBACK | AQUANTIA_1000M_LOOPBACK |AQUANTIA_2500M_LOOPBACK |\
    AQUANTIA_5000M_LOOPBACK | AQUANTIA_10000M_LOOPBACK)
#define AQUANTIA_STATUS_LINK                                             0x0004
#define AQUANTIA_PHY_USX_AUTONEG_ENABLE                                  0x0008
#define AQUANTIA_PHY_AN_AUTONEG_EN                                       0x1000
#define AQUANTIA_PHY_AN_AUTONEG_RESTART                                  0x200
/* 10T   Half Duplex Capable */
#define AQUANTIA_ADVERTISE_10HALF                                        0x0020
/* 10T   Full Duplex Capable */
#define AQUANTIA_ADVERTISE_10FULL                                        0x0040
/* 100TX Half Duplex Capable */
#define AQUANTIA_ADVERTISE_100HALF                                       0x0080
/* 100TX Full Duplex Capable */
#define AQUANTIA_ADVERTISE_100FULL                                       0x0100
/* Pause operation desired */
#define AQUANTIA_ADVERTISE_PAUSE                                         0x0400
/* Asymmetric Pause Direction bit */
#define AQUANTIA_ADVERTISE_ASYM_PAUSE                                    0x0800
/* Remote Fault detected */
#define AQUANTIA_ADVERTISE_REMOTE_FAULT                                  0x2000
/* Next Page ability supported */
#define AQUANTIA_ADVERTISE_NEXT_PAGE                                     0x8000
/* 1000TX Half Duplex Capable */
#define AQUANTIA_ADVERTISE_1000HALF                                      0x4000
/* 1000TX Full Duplex Capable */
#define AQUANTIA_ADVERTISE_1000FULL                                      0x8000
/* 2500TX Full Duplex Capable */
#define AQUANTIA_ADVERTISE_2500FULL                                      0x0400
#define AQUANTIA_ADVERTISE_8023BZ_2500FULL                               0x80
/* 10000TX Full Duplex Capable */
#define AQUANTIA_ADVERTISE_10000FULL                                     0x1000
/* 5000TX Full Duplex Capable */
#define AQUANTIA_ADVERTISE_5000FULL                                      0x0800
#define AQUANTIA_ADVERTISE_8023BZ_5000FULL                               0x100

#define AQUANTIA_ADVERTISE_MEGA_ALL \
    (AQUANTIA_ADVERTISE_10FULL | AQUANTIA_ADVERTISE_100FULL |\
    AQUANTIA_ADVERTISE_PAUSE | AQUANTIA_ADVERTISE_ASYM_PAUSE)

#define AQUANTIA_ADVERTISE_GIGA \
  (AQUANTIA_ADVERTISE_1000FULL | AQUANTIA_ADVERTISE_2500FULL |\
  AQUANTIA_ADVERTISE_5000FULL)

  #define AQUANTIA_ADVERTISE_GIGA_PLUS \
  (AQUANTIA_ADVERTISE_10000FULL | AQUANTIA_ADVERTISE_8023BZ_2500FULL|\
  AQUANTIA_ADVERTISE_8023BZ_5000FULL)

/* 1=Duplex 0=Half Duplex */
#define AQUANTIA_STATUS_FULL_DUPLEX                                      0x0001
/* Speed, bits 1 : 3*/
#define AQUANTIA_STATUS_SPEED                                            0xC000
#define AQUANTIA_STATUS_SPEED_MASK                                       0x000E
/* 000=10Mbs */
#define AQUANTIA_STATUS_SPEED_10MBS                                      0x0000
/* 001=100Mbs */
#define AQUANTIA_STATUS_SPEED_100MBS                                     0x0001
/* 010=1000Mbs */
#define AQUANTIA_STATUS_SPEED_1000MBS                                    0x0002
/* 011=10000Mbs */
#define AQUANTIA_STATUS_SPEED_10000MBS                                   0x0003
/* 100=2500Mbs */
#define AQUANTIA_STATUS_SPEED_2500MBS                                    0x0004
/* 101=5000Mbs */
#define AQUANTIA_STATUS_SPEED_5000MBS                                    0x0005

#define AQUANTIA_PHY_CDT_MODE0                                           0
#define AQUANTIA_PHY_CDT_MODE1                                           1
#define AQUANTIA_PHY_CDT_MODE2                                           2

sw_error_t
aquantia_phy_cdt(a_uint32_t dev_id, a_uint32_t phy_addr, a_uint32_t mdi_pair,
	fal_cable_status_t * cable_status, a_uint32_t * cable_len);
sw_error_t
aquantia_phy_interface_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_interface_mode_t interface_mode);
sw_error_t
aquantia_phy_mdix_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_mdix_mode_t mode);
sw_error_t
aquantia_phy_mdix_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_mdix_mode_t * mode);
sw_error_t
aquantia_phy_mdix_status_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_mdix_status_t * mode);
sw_error_t
aquantia_phy_remote_loopback_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_bool_t enable);
sw_error_t
aquantia_phy_remote_loopback_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_bool_t * enable);
sw_error_t
aquantia_phy_wol_set(a_uint32_t dev_id, a_uint32_t phy_addr, a_bool_t enable);
sw_error_t
aquantia_phy_wol_get(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t * enable);
sw_error_t
aquantia_phy_magic_frame_set(a_uint32_t dev_id, a_uint32_t phy_id,
	fal_mac_addr_t * mac);
sw_error_t
aquantia_phy_magic_frame_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_mac_addr_t * mac);
sw_error_t
aquantia_phy_stats_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_counter_info_t * counter_infor);
sw_error_t
aquantia_phy_hw_init(a_uint32_t dev_id,  a_uint32_t phy_addr);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _AQUANTIA_PHY_H_ */
