/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/

#include <common.h>
#include <command.h>
#include <asm/io.h>
#include <phy.h>
#include <miiphy.h>

#include "qti.h"

#define QTI_8081_PHY_V1				0x004DD100
#define QTI_8081_PHY_V1_1			0x004DD101

#define QTI_8081_PHY_SPEC_STATUS                 17
#define QTI_8081_STATUS_LINK_PASS                0x0400
#define QTI_8081_STATUS_FULL_DUPLEX              0x2000
#define QTI_8081_STATUS_SPEED_MASK               0x380
#define QTI_8081_STATUS_SPEED_2500MBS            0x200
#define QTI_8081_STATUS_SPEED_1000MBS            0x100
#define QTI_8081_STATUS_SPEED_100MBS             0x80
#define QTI_8081_STATUS_SPEED_10MBS              0x0000

#define QTI_8081_PHY_MMD3_NUM                    3

#define QTI_8081_PHY_MMD3_ADDR_CLD_CTRL7         0x8007
#define QTI_8081_PHY_8023AZ_AFE_CTRL_MASK        0x01f0
#define QTI_8081_PHY_8023AZ_AFE_EN               0x0090

#define QTI_8081_PHY_MMD3_AZ_TRAINING_CTRL       0x8008
#define QTI_8081_PHY_MMD3_AZ_TRAINING_VAL        0x1c32


static int qti_8081_config(struct phy_device *phydev)
{
	u16 phy_data;

	/* Enable vga when init napa to fix 8023az issue */
	phy_data = phy_read_mmd(phydev, QTI_8081_PHY_MMD3_NUM,
			QTI_8081_PHY_MMD3_ADDR_CLD_CTRL7);
	phy_data &= (~QTI_8081_PHY_8023AZ_AFE_CTRL_MASK);
	phy_data |= QTI_8081_PHY_8023AZ_AFE_EN;
	phy_write_mmd(phydev, QTI_8081_PHY_MMD3_NUM,
			QTI_8081_PHY_MMD3_ADDR_CLD_CTRL7, phy_data);

	/* Special configuration for AZ under 1G speed mode */
	phy_write_mmd(phydev, QTI_8081_PHY_MMD3_NUM,
			QTI_8081_PHY_MMD3_AZ_TRAINING_CTRL,
			QTI_8081_PHY_MMD3_AZ_TRAINING_VAL);
	return 0;
}

static int qti_8081_startup(struct phy_device *phydev)
{
	u16 phy_data;

	phy_data = phy_read(phydev, MDIO_DEVAD_NONE, QTI_8081_PHY_SPEC_STATUS);
	if (phy_data & QTI_8081_STATUS_LINK_PASS)
		phydev->link = 1;
	else
		phydev->link = 0;

	if (phy_data & QTI_8081_STATUS_FULL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	switch (phy_data & QTI_8081_STATUS_SPEED_MASK) {
	case QTI_8081_STATUS_SPEED_2500MBS:
		phydev->speed = SPEED_2500;
		break;
	case QTI_8081_STATUS_SPEED_1000MBS:
		phydev->speed = SPEED_1000;
		break;
	case QTI_8081_STATUS_SPEED_100MBS:
		phydev->speed = SPEED_100;
		break;
	case QTI_8081_STATUS_SPEED_10MBS:
		phydev->speed = SPEED_10;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct phy_driver qti_8081_driver = {
	.name = "QTI 8081 PHY Driver",
	.uid = QTI_8081_PHY_V1,
	.mask = 0xfffffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &qti_8081_config,
	.startup = &qti_8081_startup,
	.shutdown = &genphy_shutdown,
};

int phy_8081_init(void)
{
	phy_register(&qti_8081_driver);
	return 0;
}
