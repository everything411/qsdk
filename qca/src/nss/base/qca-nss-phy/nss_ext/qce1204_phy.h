/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


#ifndef _QCE1204_PHY_H_
#define _QCE1204_PHY_H_

#ifdef __cplusplus
extern "C"
{
#endif				/* __cplusplus */
#define QCE1204_PHY_MMD7_CDT_WITH_AUTONEG	0x200
#define QCE1204_PHY_ALL_PAIRS_NORMAL		0x1111
#define QCE1204_PHY_MMD3_CLD_CTRL		0x8002
#define QCE1204_PHY_CLD_FORCE_EN		0x100
#define QCE1204_PHY_MMD3_CLD_RESULT		0x808c
#define QCE1204_PHY_CLD_CABLE_LENGTH		0xff

/* PPM offset clock frequency divisors for QCE1204 (Huntington) and IPQ52XX (Hermosa) */
#define QCE1204_PPM_DIV_1G		(512ULL << 18)
#define QCE1204_PPM_DIV_2_5G		(640ULL << 15)

#define QCE1204_GPIO_FUNC_SEL			0x3c
#define QCE1204_GPIO_DRV			0x1c0	/* drive strength = 16 mA */
#define QCE1204_GPIO_LED_MODE			0x800
#define QCE1204_GPIO_PULL			0x3

#define QCE1204_TLMM_BASE			0x6400000
#define QCE1204_TLMM_ADDR_FLAG			BIT(30)	/* address-space flag consumed by sw_read/sw_write */
#define TO_GPIO_REG(gpio_num)		\
	((QCE1204_TLMM_BASE + (gpio_num) * 0x1000) | QCE1204_TLMM_ADDR_FLAG)

/* GPIOs for LED */
#define QCE1204_GPIO1				1
#define QCE1204_GPIO2				2
#define QCE1204_GPIO3				3
#define QCE1204_GPIO4				4

#define QCE1204_GPIO_FUNC_P0_LED_0		1
#define QCE1204_GPIO_FUNC_P1_LED_0		1
#define QCE1204_GPIO_FUNC_P2_LED_0		1
#define QCE1204_GPIO_FUNC_P3_LED_0		1

#define QCE1204_GPIO5				5
#define QCE1204_GPIO6				6
#define QCE1204_GPIO7				7
#define QCE1204_GPIO8				8

#define QCE1204_GPIO_FUNC_P0_LED_2		2
#define QCE1204_GPIO_FUNC_P1_LED_2		2
#define QCE1204_GPIO_FUNC_P2_LED_2		3
#define QCE1204_GPIO_FUNC_P3_LED_2		3

#define QCE1204_GPIO9				9
#define QCE1204_GPIO15				15
#define QCE1204_GPIO16				16
#define QCE1204_GPIO17				17

#define QCE1204_GPIO_FUNC_P0_LED_1		3
#define QCE1204_GPIO_FUNC_P1_LED_1		1
#define QCE1204_GPIO_FUNC_P2_LED_1		1
#define QCE1204_GPIO_FUNC_P3_LED_1		1

int qce1204_phy_ops_init(struct nss_phy_ops *ops);
#ifdef __cplusplus
}
#endif				/* __cplusplus */
#endif				/* _QCE1204_PHY_H_ */
