/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <command.h>
#include <asm/io.h>
#include <phy.h>
#include <miiphy.h>
/*
 * Phy Specific status fields offset:17
 * 1=Speed & Duplex resolved
 * 1=Duplex 0=Half Duplex
 * Speed, bits 14:15
 *	00=10Mbs
 *	01=100Mbs
 *	10=1000Mbs
 */
#define QTI_8033_PHY_V1				0x004DD074
#define QTI_8033_PHY_SPEC_STATUS		17
#define QTI_8033_STATUS_LINK_PASS		0x0400
#define QTI_8033_STATUS_FULL_DUPLEX		0x2000
#define QTI_8033_STATUS_SPEED_MASK		0xC000
#define QTI_8033_STATUS_SPEED_1000MBS		0x8000
#define QTI_8033_STATUS_SPEED_100MBS		0x4000
#define QTI_8033_STATUS_SPEED_10MBS		0x0000
static int qti_8033_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x5);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x2d47);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0xb);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0xbc40);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1d, 0x0);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1e, 0x82ee);
	return 0;
}

static int qti_8033_startup(struct phy_device *phydev)
{
	u16 phy_data = phy_read(phydev, MDIO_DEVAD_NONE, QTI_8033_PHY_SPEC_STATUS);

	if (phy_data & QTI_8033_STATUS_LINK_PASS)
		phydev->link = 1;
	else
		phydev->link = 0;
	if (phy_data & QTI_8033_STATUS_FULL_DUPLEX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;
	switch (phy_data & QTI_8033_STATUS_SPEED_MASK) {
	case QTI_8033_STATUS_SPEED_1000MBS:
		phydev->speed = SPEED_1000;
		break;
	case QTI_8033_STATUS_SPEED_100MBS:
		phydev->speed = SPEED_100;
		break;
	case QTI_8033_STATUS_SPEED_10MBS:
		phydev->speed = SPEED_10;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

int qti_8033_probe(struct phy_device *phydev)
{
	phydev->flags = PHY_FLAG_BROKEN_RESET;

	return 0;
}

U_BOOT_PHY_DRIVER(qti_8033_driver) = {
	.name = "QTI 8033 PHY Driver",
	.uid = QTI_8033_PHY_V1,
	.mask = 0xfffffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &qti_8033_config,
	.startup = &qti_8033_startup,
	.shutdown = &genphy_shutdown,
	.probe  = &qti_8033_probe,
};
