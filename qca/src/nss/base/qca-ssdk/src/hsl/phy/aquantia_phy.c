/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "sw.h"
#include "fal_port_ctrl.h"
#include "hsl_api.h"
#include "hsl.h"
#include "aquantia_phy.h"
#include "hsl_phy.h"
#include "ssdk_plat.h"

static sw_error_t
aquantia_phy_get_phy_id(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_uint32_t *phy_id)
{
	a_uint16_t org_id = 0, rev_id = 0;

	org_id= hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_AUTONEG, AQUANTIA_PHY_ID1);
	PHY_RTN_ON_READ_ERROR(org_id);
	rev_id = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_AUTONEG, AQUANTIA_PHY_ID2);
	PHY_RTN_ON_READ_ERROR(rev_id);

	*phy_id = ((org_id & 0xffff) << 16) | (rev_id & 0xffff);

	return SW_OK;
}

/******************************************************************************
*
* aquantia_phy_cdt - cable diagnostic test
*
* cable diagnostic test
*/
static inline fal_cable_status_t _phy_cdt_status_mapping(a_uint32_t pair_type, a_uint16_t status)
{
	fal_cable_status_t status_mapping = FAL_CABLE_STATUS_INVALID;

	switch(status)
	{
		case 0:
			status_mapping = FAL_CABLE_STATUS_NORMAL;
			break;
		case 1:
			if(pair_type == CABLE_PAIR_B)
				status_mapping = FAL_CABLE_STATUS_CROSSOVERA;
			else if(pair_type == CABLE_PAIR_C)
				status_mapping = FAL_CABLE_STATUS_CROSSOVERB;
			else if(pair_type == CABLE_PAIR_D)
				status_mapping = FAL_CABLE_STATUS_CROSSOVERC;
			else
				status_mapping = FAL_CABLE_STATUS_INVALID;
			break;
		case 2:
			if(pair_type == CABLE_PAIR_C)
				status_mapping = FAL_CABLE_STATUS_CROSSOVERA;
			else if(pair_type == CABLE_PAIR_D)
				status_mapping = FAL_CABLE_STATUS_CROSSOVERB;
			else
				status_mapping = FAL_CABLE_STATUS_INVALID;
			break;
		case 3:
			if(pair_type == CABLE_PAIR_D)
				status_mapping = FAL_CABLE_STATUS_CROSSOVERA;
			else
				status_mapping = FAL_CABLE_STATUS_INVALID;
			break;
		case 4:
			status_mapping = FAL_CABLE_STATUS_SHORT;
			break;
		case 5:
			status_mapping = FAL_CABLE_STATUS_LOW_MISMATCH;
			break;
		case 6:
			status_mapping = FAL_CABLE_STATUS_HIGH_MISMATCH;
			break;
		case 7:
			status_mapping = FAL_CABLE_STATUS_OPENED;
			break;
		default:
			status_mapping = FAL_CABLE_STATUS_INVALID;
			break;
	}

	return status_mapping;
}

static sw_error_t
aquantia_phy_cdt_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_cdt_t * port_cdt)
{
	a_uint16_t status = 0;
	a_uint16_t phy_data = 0;

	if (!port_cdt) {
		return SW_FAIL;
	}
	/* Get cable status */
	status = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_GLOBAL_REGISTERS,
		AQUANTIA_CABLE_DIAGNOSTIC_STATUS1);
	PHY_RTN_ON_READ_ERROR(status);
	port_cdt->pair_a_status =  (status & AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRA) >> 12
		& BITS(0, 3);
	port_cdt->pair_b_status = (status & AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRB) >> 8
		& BITS(0, 3);
	port_cdt->pair_c_status = (status & AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRC) >> 4
		& BITS(0, 3);
	port_cdt->pair_d_status = (status & AQUANTIA_CABLE_DIAGNOSTIC_STATUS_PAIRD)
		& BITS(0, 3);
	SSDK_DEBUG("status:%x, pair_a_status:%x,pair_b_status:%x,pair_c_status:%x, pair_d_status:%x\n",
		status, port_cdt->pair_a_status,port_cdt->pair_b_status,
		port_cdt->pair_c_status, port_cdt->pair_d_status);
	/* Get Cable Length value */
	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_GLOBAL_REGISTERS,
		AQUANTIA_CABLE_DIAGNOSTIC_STATUS2);
	PHY_RTN_ON_READ_ERROR(phy_data);
	port_cdt->pair_a_len = phy_data >> 8 & BITS(0, 8);

	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_GLOBAL_REGISTERS,
		AQUANTIA_CABLE_DIAGNOSTIC_STATUS4);
	PHY_RTN_ON_READ_ERROR(phy_data);
	port_cdt->pair_b_len = phy_data >> 8 & BITS(0, 8);

	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_GLOBAL_REGISTERS,
		AQUANTIA_CABLE_DIAGNOSTIC_STATUS6);
	PHY_RTN_ON_READ_ERROR(phy_data);
	port_cdt->pair_c_len = phy_data >> 8 & BITS(0, 8);

	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_GLOBAL_REGISTERS,
		AQUANTIA_CABLE_DIAGNOSTIC_STATUS8);
	PHY_RTN_ON_READ_ERROR(phy_data);
	port_cdt->pair_d_len = phy_data >> 8 & BITS(0, 8);

	return SW_OK;
}

static sw_error_t aquatia_phy_cdt_start(a_uint32_t dev_id, a_uint32_t phy_addr)
{
	a_uint16_t status = 0, phy_data = 0;
	a_uint32_t aq_phy_id = 0;
	a_uint16_t ii = 300;
	sw_error_t rv = SW_OK;

	/*select mode0 if aq107, and select mode2 if aq109*/
	rv = aquantia_phy_get_phy_id(dev_id, phy_addr, &aq_phy_id);
	PHY_RTN_ON_ERROR(rv);
	if(aq_phy_id == AQUANTIA_PHY_109 || aq_phy_id == AQUANTIA_PHY_113C_B0 ||
		aq_phy_id == AQUANTIA_PHY_113C_B1)
	{
		phy_data |= AQUANTIA_PHY_CDT_MODE2;
	}
	else
	{
		phy_data |= AQUANTIA_PHY_CDT_MODE0;
	}

	phy_data |= AQUANTIA_NORMAL_CABLE_DIAGNOSTICS;
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_GLOBAL_REGISTERS,
		 AQUANTIA_GLOBAL_CDT_CONTROL, 0x13, phy_data);
	PHY_RTN_ON_ERROR(rv);
	do {
		aos_mdelay(30);
		status  = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
			AQUANTIA_MMD_GLOBAL_REGISTERS,
			AQUANTIA_GLOBAL_GENERAL_STATUS);
		PHY_RTN_ON_READ_ERROR(status);
	}
	while ((status & AQUANTIA_CABLE_DIAGNOSTICS_STATUS) && (--ii));

	return SW_OK;
}

/******************************************************************************
*
* AQUANTIA_set_autoneg_adv - set the phy autoneg Advertisement
*
*/
static sw_error_t
aquantia_phy_set_autoneg_adv(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_uint32_t autoneg)
{
	a_uint16_t phy_data = 0;
	sw_error_t rv = SW_OK;

	if ((autoneg & FAL_PHY_ADV_10T_HD) || (autoneg & FAL_PHY_ADV_100TX_HD))
	{
		return SW_NOT_SUPPORTED;
	}

	if (autoneg & FAL_PHY_ADV_10T_FD)
		phy_data |= AQUANTIA_ADVERTISE_10FULL;
	if (autoneg & FAL_PHY_ADV_100TX_FD)
		phy_data |= AQUANTIA_ADVERTISE_100FULL;
	if (autoneg & FAL_PHY_ADV_PAUSE)
		phy_data |= AQUANTIA_ADVERTISE_PAUSE;
	if (autoneg & FAL_PHY_ADV_ASY_PAUSE)
		phy_data |= AQUANTIA_ADVERTISE_ASYM_PAUSE;
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_PHY_AN_ADV, AQUANTIA_ADVERTISE_MEGA_ALL, phy_data);
	SW_RTN_ON_ERROR(rv);

	phy_data = 0;
	if (autoneg & FAL_PHY_ADV_2500T_FD)
		phy_data |= AQUANTIA_ADVERTISE_8023BZ_2500FULL;
	if (autoneg & FAL_PHY_ADV_5000T_FD)
		phy_data |= AQUANTIA_ADVERTISE_8023BZ_5000FULL;
	if (autoneg & FAL_PHY_ADV_10000T_FD)
		phy_data |= AQUANTIA_ADVERTISE_10000FULL;
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_PHY_AN_10G_ADV, AQUANTIA_ADVERTISE_GIGA_PLUS, phy_data);
	SW_RTN_ON_ERROR(rv);

	if (autoneg & FAL_PHY_ADV_1000T_FD)
		phy_data |= AQUANTIA_ADVERTISE_1000FULL;
	if (autoneg & FAL_PHY_ADV_2500T_FD)
		phy_data |= AQUANTIA_ADVERTISE_2500FULL;
	if (autoneg & FAL_PHY_ADV_5000T_FD)
		phy_data |= AQUANTIA_ADVERTISE_5000FULL;

	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_AUTONEG_VENDOR_PROVISION1, AQUANTIA_ADVERTISE_GIGA, phy_data);
	PHY_RTN_ON_ERROR(rv);

	return hsl_phy_phydev_autoneg_update(dev_id, phy_addr, A_TRUE, autoneg);
}

sw_error_t
aquantia_phy_cdt(a_uint32_t dev_id, a_uint32_t phy_addr, a_uint32_t mdi_pair,
	fal_cable_status_t * cable_status, a_uint32_t * cable_len)
{
	fal_port_cdt_t aquantia_port_cdt = {0};
	sw_error_t rv = SW_OK;

	if (mdi_pair >= 4) {
		return SW_BAD_PARAM;
	}
	rv = aquatia_phy_cdt_start(dev_id, phy_addr);
	PHY_RTN_ON_ERROR(rv);
	rv = aquantia_phy_cdt_get(dev_id, phy_addr, &aquantia_port_cdt);
	PHY_RTN_ON_ERROR(rv);
	switch (mdi_pair)
	{
		case 0:
			*cable_status =
				 _phy_cdt_status_mapping(CABLE_PAIR_A, aquantia_port_cdt.pair_a_status);
			/* Get Cable Length value */
			*cable_len = aquantia_port_cdt.pair_a_len;
			break;
		case 1:
			*cable_status =
				 _phy_cdt_status_mapping(CABLE_PAIR_B, aquantia_port_cdt.pair_b_status);
			/* Get Cable Length value */
			*cable_len = aquantia_port_cdt.pair_b_len;
			break;
		case 2:
			*cable_status =
				_phy_cdt_status_mapping(CABLE_PAIR_C, aquantia_port_cdt.pair_c_status);
			/* Get Cable Length value */
			*cable_len = aquantia_port_cdt.pair_c_len;
			break;
		case 3:
			*cable_status =
				 _phy_cdt_status_mapping(CABLE_PAIR_D, aquantia_port_cdt.pair_d_status);
			/* Get Cable Length value */
			*cable_len = aquantia_port_cdt.pair_d_len;
			break;
		default:
			break;
	}

	return SW_OK;
}



sw_error_t
aquantia_phy_interface_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_interface_mode_t interface_mode)
{
	a_uint16_t phy_data = 0;
	sw_error_t rv = SW_OK;

	switch(interface_mode) {
	case PHY_SGMII_BASET:
		phy_data |= AQUANTIA_SERDES_MODE_SGMII;
		break;
	case PORT_USXGMII:
		phy_data |= AQUANTIA_SERDES_MODE_XFI;
		break;
	case PORT_SGMII_PLUS:
		phy_data |= AQUANTIA_SERDES_MODE_OCSGMII;
		break;
	default:
		return SW_NOT_SUPPORTED;
	}
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GLOBAL_REGISTERS, AQUANTIA_GLOBAL_SYS_CONFIG_FOR_10M,
		BITS(0, 3), phy_data);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GLOBAL_REGISTERS, AQUANTIA_GLOBAL_SYS_CONFIG_FOR_100M,
		BITS(0, 3), phy_data);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GLOBAL_REGISTERS, AQUANTIA_GLOBAL_SYS_CONFIG_FOR_1000M,
		BITS(0, 3), phy_data);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GLOBAL_REGISTERS, AQUANTIA_GLOBAL_SYS_CONFIG_FOR_2500M,
		BITS(0, 3), phy_data);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GLOBAL_REGISTERS, AQUANTIA_GLOBAL_SYS_CONFIG_FOR_5000M,
		BITS(0, 3), phy_data);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GLOBAL_REGISTERS, AQUANTIA_GLOBAL_SYS_CONFIG_FOR_10000M,
		BITS(0, 3), phy_data);

	return rv;
}

#ifndef IN_PORTCONTROL_MINI
/******************************************************************************
*
* aquantia_phy_get_status
*
* get phy status
*/
static sw_error_t
aquantia_phy_get_status(a_uint32_t dev_id, a_uint32_t phy_addr,
	struct port_phy_status *phy_status)
{
	a_uint16_t phy_data = 0;

	/*get phy link status, in order to get the  link status of real time,
	  need to read the link status two times */
	phy_data= hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_PHY_AN_STATUS);
	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_PHY_AN_STATUS);
	if (phy_data & AQUANTIA_STATUS_LINK)
		phy_status->link_status = A_TRUE;
	else
		phy_status->link_status = A_FALSE;

	/*get phy speed and duplex*/
	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_AUTONEG,
		AQUANTIA_REG_AUTONEG_VENDOR_STATUS);
	PHY_RTN_ON_READ_ERROR(phy_data);
	switch ((phy_data & AQUANTIA_STATUS_SPEED_MASK) >>1)
	{
		case AQUANTIA_STATUS_SPEED_10MBS:
			phy_status->speed = FAL_SPEED_10;
			break;
		case AQUANTIA_STATUS_SPEED_100MBS:
			phy_status->speed = FAL_SPEED_100;
			break;
		case AQUANTIA_STATUS_SPEED_1000MBS:
			phy_status->speed = FAL_SPEED_1000;
			break;
		case AQUANTIA_STATUS_SPEED_2500MBS:
			phy_status->speed = FAL_SPEED_2500;
			break;
		case AQUANTIA_STATUS_SPEED_5000MBS:
			phy_status->speed = FAL_SPEED_5000;
			break;
		case AQUANTIA_STATUS_SPEED_10000MBS:
			phy_status->speed = FAL_SPEED_10000;
			break;
		default:
			return SW_READ_ERROR;
	}
	if (phy_data & AQUANTIA_STATUS_FULL_DUPLEX)
	{
		phy_status->duplex = FAL_FULL_DUPLEX;
	}
	else
	{
		phy_status->duplex = FAL_HALF_DUPLEX;
	}
	/* get phy tx flowctrl and rx flowctrl resolution status */
	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_AUTONEG,
		AQUANTIA_RESERVED_VENDOR_STATUS1);
	PHY_RTN_ON_READ_ERROR(phy_data);
	if(phy_data & AQUANTIA_PHY_TX_FLOWCTRL_STATUS)
	{
		phy_status->tx_flowctrl = A_TRUE;
	}
	else
	{
		phy_status->tx_flowctrl = A_FALSE;
	}
	if(phy_data & AQUANTIA_PHY_RX_FLOWCTRL_STATUS)
	{
		phy_status->rx_flowctrl = A_TRUE;
	}
	else
	{
		phy_status->rx_flowctrl = A_FALSE;
	}

	return SW_OK;
}

static sw_error_t
aquantia_phy_get_speed(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_speed_t * speed)
{
	sw_error_t rv = SW_OK;
	struct port_phy_status phy_status = {0};

	rv = aquantia_phy_get_status(dev_id, phy_addr, &phy_status);
	PHY_RTN_ON_ERROR(rv);
	if (phy_status.link_status == PORT_LINK_DOWN) {
		/*the speed register(0x4007c800) is not stable when aquantia phy is down,
		 but some APIs such as aquantia_phy_set_duplex() aquantia_phy_interface_set_mode()
		 need to get the speed, so set the speed default value as 100M when link down*/
		*speed = FAL_SPEED_100;
		return SW_OK;
	}
	*speed = phy_status.speed;

	return SW_OK;
}

/******************************************************************************
*
* AQUANTIA_restart_autoneg - restart the phy autoneg
*
*/
static sw_error_t
aquantia_phy_restart_autoneg(a_uint32_t dev_id, a_uint32_t phy_addr)
{
	sw_error_t rv = SW_OK;

	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_PHY_XS_REGISTERS,
		AQUANTIA_PHY_XS_USX_TRANSMIT, AQUANTIA_PHY_USX_AUTONEG_ENABLE,
		AQUANTIA_PHY_USX_AUTONEG_ENABLE);
	PHY_RTN_ON_ERROR(rv);

	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_PHY_AN_CONTROL,
		AQUANTIA_PHY_AN_AUTONEG_EN | AQUANTIA_PHY_AN_AUTONEG_RESTART,
		AQUANTIA_PHY_AN_AUTONEG_EN | AQUANTIA_PHY_AN_AUTONEG_RESTART);
	SW_RTN_ON_ERROR(rv);

	return hsl_phy_phydev_autoneg_update(dev_id, phy_addr, A_TRUE, 0);
}
/******************************************************************************
*
* aquantia_phy_set_mdix -
*
* set phy mdix configuraiton
*/
sw_error_t
aquantia_phy_mdix_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_mdix_mode_t mode)
{
	a_uint16_t phy_data = 0;
	sw_error_t rv = SW_OK;

	switch(mode)
	{
		case PHY_MDIX_AUTO:
			phy_data |= AQUANTIA_PHY_MDIX_AUTO;
			break;
		case PHY_MDIX_MDIX:
			phy_data |= AQUANTIA_PHY_MDIX;
			break;
		case PHY_MDIX_MDI:
			phy_data |= AQUANTIA_PHY_MDI;
			break;
		default:
			return SW_BAD_PARAM;
	}
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_RESERVED_VENDOR_PROVISIONING1, BITS(0,2), phy_data);
	PHY_RTN_ON_ERROR(rv);
	return aquantia_phy_restart_autoneg(dev_id, phy_addr);
}

/******************************************************************************
*
* aquantia_phy_get_mdix
*
* get phy mdix configuration
*/
sw_error_t
aquantia_phy_mdix_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_mdix_mode_t * mode)
{
	a_uint16_t phy_data = 0;

	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_RESERVED_VENDOR_PROVISIONING1);
	PHY_RTN_ON_READ_ERROR(phy_data);
	phy_data  &= BITS(0,2);
	switch(phy_data)
	{
		case AQUANTIA_PHY_MDIX_AUTO:
			*mode = PHY_MDIX_AUTO;
			break;
		case AQUANTIA_PHY_MDIX:
			*mode = PHY_MDIX_MDIX;
			break;
		case AQUANTIA_PHY_MDI:
			*mode = PHY_MDIX_MDI;
			break;
		default:
			return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

/******************************************************************************
*
* aquantia_phy_get_mdix status
*
* get phy mdix status
*/
sw_error_t
aquantia_phy_mdix_status_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_mdix_status_t * mode)
{
	a_uint16_t phy_data = 0;

	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_RESERVED_VENDOR_STATUS1);
	PHY_RTN_ON_READ_ERROR(phy_data);
	*mode = (phy_data &  AQUANTIA_PHY_MDIX_STATUS) ? PHY_MDIX_STATUS_MDIX :
		PHY_MDIX_STATUS_MDI;

	return SW_OK;
}

sw_error_t
aquantia_phy_remote_loopback_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_bool_t enable)
{
	a_uint16_t phy_data = 0;
	fal_port_speed_t speed = FAL_SPEED_BUTT;
	sw_error_t rv = SW_OK;

	if (enable == A_TRUE)
	{
		rv = aquantia_phy_get_speed(dev_id,  phy_addr, &speed);
		PHY_RTN_ON_ERROR(rv);
		switch(speed)
		{
			case FAL_SPEED_10:
				phy_data |= AQUANTIA_10M_LOOPBACK;
				break;
			case FAL_SPEED_100:
				phy_data |= AQUANTIA_100M_LOOPBACK;
				break;
			case FAL_SPEED_1000:
				phy_data |= AQUANTIA_1000M_LOOPBACK;
				break;
			case FAL_SPEED_2500:
				phy_data |= AQUANTIA_2500M_LOOPBACK;
				break;
			case FAL_SPEED_5000:
				phy_data |= AQUANTIA_5000M_LOOPBACK;
				break;
			case FAL_SPEED_10000:
				phy_data |= AQUANTIA_10000M_LOOPBACK;
				break;
			default:
				break;
		}
		phy_data |= AQUANTIA_PHY_REMOTE_LOOPBACK;
	}
	return hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE, AQUANTIA_MMD_PHY_XS_REGISTERS,
		AQUANTIA_PHY_XS_TRANAMIT_RESERVED_VENDOR_PROVISION5,
		AQUANTIA_PHY_REMOTE_LOOPBACK | AQUANTIA_ALL_SPEED_LOOPBACK, phy_data);
}

/******************************************************************************
*
* aquantia_phy_get_remote_loopback
*
* get phy remote loopback
*/
sw_error_t
aquantia_phy_remote_loopback_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_bool_t * enable)
{
	a_uint16_t phy_data = 0;

	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PHY_XS_REGISTERS,
		AQUANTIA_PHY_XS_TRANAMIT_RESERVED_VENDOR_PROVISION5);
	PHY_RTN_ON_READ_ERROR(phy_data);
	if (phy_data & AQUANTIA_PHY_REMOTE_LOOPBACK)
	{
		*enable = A_TRUE;
	}
	else
	{
		*enable = A_FALSE;
	}

	return SW_OK;
}
/******************************************************************************
*
* aquantia_phy_set wol enable or disable
*
* set phy wol enable or disable
*/
sw_error_t
aquantia_phy_wol_set(a_uint32_t dev_id, a_uint32_t phy_addr, a_bool_t enable)
{
	a_uint16_t phy_data0 = 0, phy_data1 = 0, phy_data2 = 0;
	sw_error_t rv = SW_OK;

	if (enable == A_TRUE)
	{
		phy_data0 |= AQUANTIA_PHY_WOL_ENABLE;
		phy_data1 |= AQUANTIA_MAGIC_PACKETS_ENABLE;
		phy_data2 |= AQUANTIA_MAGIC_PACKETS_ENABLE;
	}
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_AUTONEG, AQUANTIA_RESERVED_VENDOR_PROVISIONING1,
		AQUANTIA_PHY_WOL_ENABLE, phy_data0);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS, AQUANTIA_MAGIC_ENGINE_REGISTER1,
		AQUANTIA_MAGIC_PACKETS_ENABLE, phy_data1);
	PHY_RTN_ON_ERROR(rv);
	return hsl_phy_modify_mmd(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS, AQUANTIA_MAGIC_ENGINE_REGISTER2,
		AQUANTIA_MAGIC_PACKETS_ENABLE, phy_data2);
}

/******************************************************************************
*
* aquantia_phy_get_wol status
*
* get wol status
*/
sw_error_t
aquantia_phy_wol_get(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t * enable)
{
	a_uint16_t phy_data = 0;

	*enable = A_FALSE;
	phy_data = hsl_phy_mmd_reg_read(dev_id, phy_id, A_TRUE, AQUANTIA_MMD_AUTONEG,
		AQUANTIA_RESERVED_VENDOR_PROVISIONING1);
	PHY_RTN_ON_READ_ERROR(phy_data);
	if (phy_data & AQUANTIA_PHY_WOL_ENABLE)
	{
		*enable = A_TRUE;
	}

	return SW_OK;
}

/******************************************************************************
*
* aquantia_phy_set wol frame mac address
*
* set phy wol frame mac address
*/
sw_error_t
aquantia_phy_magic_frame_set(a_uint32_t dev_id, a_uint32_t phy_id,
	fal_mac_addr_t * mac)
{
	a_uint16_t phy_data1 = 0;
	a_uint16_t phy_data2 = 0;
	a_uint16_t phy_data3 = 0;
	sw_error_t rv = SW_OK;

	phy_data1 = (mac->uc[1] << 8) | mac->uc[0];
	phy_data2 = (mac->uc[3] << 8) | mac->uc[2];
	phy_data3 = (mac->uc[5] << 8) | mac->uc[4];
	rv = hsl_phy_mmd_reg_write(dev_id, phy_id, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS, AQUANTIA_MAGIC_FRAME_MAC0, phy_data1);
	PHY_RTN_ON_ERROR(rv);
	rv = hsl_phy_mmd_reg_write(dev_id, phy_id, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS, AQUANTIA_MAGIC_FRAME_MAC1, phy_data2);
	PHY_RTN_ON_ERROR(rv);
	return hsl_phy_mmd_reg_write(dev_id, phy_id, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS, AQUANTIA_MAGIC_FRAME_MAC2, phy_data3);
}

/******************************************************************************
*
* aquantia_phy_get wol frame mac address
*
* get phy wol frame mac address
*/
sw_error_t
aquantia_phy_magic_frame_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_mac_addr_t * mac)
{
	a_uint16_t phy_data1 = 0;
	a_uint16_t phy_data2 = 0;
	a_uint16_t phy_data3 = 0;

	phy_data1 = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS,
		AQUANTIA_MAGIC_FRAME_MAC0);
	PHY_RTN_ON_READ_ERROR(phy_data1);
	phy_data2 = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS,
		AQUANTIA_MAGIC_FRAME_MAC1);
	PHY_RTN_ON_READ_ERROR(phy_data2);
	phy_data3 = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_GBE_STANDARD_REGISTERS,
		AQUANTIA_MAGIC_FRAME_MAC2);
	PHY_RTN_ON_READ_ERROR(phy_data3);
	mac->uc[0] = (phy_data1 & BITS(0, 8));
	mac->uc[1] = (phy_data1 >> 8) & BITS(0, 8);
	mac->uc[2] = (phy_data2 & BITS(0, 8));
	mac->uc[3] = (phy_data2 >> 8) & BITS(0, 8);
	mac->uc[4] = (phy_data3 & BITS(0, 8));
	mac->uc[5] = (phy_data3 >> 8) & BITS(0, 8);

	return SW_OK;
}

static sw_error_t
_aquantia_phy_line_side_counter_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_counter_info_t * counter_infor)
{
	a_uint16_t msw_counter = 0;
	a_uint16_t lsw_counter = 0;

	/*get line side tx good packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_TRANSMIT_GOOD_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_TRANSMIT_GOOD_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->TxGoodFrame = (msw_counter << 16) | lsw_counter;

	/*get line side tx bad packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_TRANSMIT_ERROR_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_TRANSMIT_ERROR_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->TxFcsErr = (msw_counter << 16) | lsw_counter;

	/*get line side rx good packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_RECEIVE_GOOD_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_RECEIVE_GOOD_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->RxGoodFrame = (msw_counter << 16) | lsw_counter;

	/*get line side rx bad packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_RECEIVE_ERROR_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_LINE_SIDE_RECEIVE_ERROR_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->RxFcsErr = (msw_counter << 16) | lsw_counter;

	return SW_OK;
}

static sw_error_t
_aquantia_phy_system_side_counter_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_counter_info_t * counter_infor)
{
	a_uint16_t msw_counter = 0;
	a_uint16_t lsw_counter = 0;

	/*get system tx good packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_TRANSMIT_GOOD_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_TRANSMIT_GOOD_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->SysTxGoodFrame = (msw_counter << 16) | lsw_counter;

	/*get system tx bad packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_TRANSMIT_ERROR_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_TRANSMIT_ERROR_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->SysTxFcsErr = (msw_counter << 16) | lsw_counter;

	/*get system rx good packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_RECEIVE_GOOD_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_RECEIVE_GOOD_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->SysRxGoodFrame = (msw_counter << 16) | lsw_counter;

	/*get system rx bad packets*/
	msw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_RECEIVE_ERROR_FRAME_COUNTER2);
	PHY_RTN_ON_READ_ERROR(msw_counter);
	lsw_counter = hsl_phy_mmd_reg_read(dev_id, phy_addr, A_TRUE,
		AQUANTIA_MMD_PCS_REGISTERS,
		AQUANTIA_SYSTEM_SIDE_RECEIVE_ERROR_FRAME_COUNTER1);
	PHY_RTN_ON_READ_ERROR(lsw_counter);
	counter_infor->SysRxFcsErr = (msw_counter << 16) | lsw_counter;

	return SW_OK;
}

/******************************************************************************
*
* aquantia_phy_show show counter statistics
*
* show counter statistics
*/
sw_error_t
aquantia_phy_stats_get(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_counter_info_t * counter_infor)
{
	sw_error_t rv = SW_OK;
	fal_port_speed_t speed;

	rv = aquantia_phy_get_speed(dev_id, phy_addr, &speed);
	PHY_RTN_ON_ERROR(rv);
	if(speed == FAL_SPEED_2500 || speed == FAL_SPEED_5000 ||
		speed == FAL_SPEED_10000)
	{
		rv = _aquantia_phy_line_side_counter_get(dev_id, phy_addr, counter_infor);
		PHY_RTN_ON_ERROR(rv);
	}
	return _aquantia_phy_system_side_counter_get(dev_id, phy_addr, counter_infor);
}
#endif
/******************************************************************************
*
* aquantia_phy_hw_register init to avoid packet loss
*
*/
sw_error_t
aquantia_phy_hw_init(a_uint32_t dev_id,  a_uint32_t phy_addr)
{
	a_uint32_t aq_phy_id = 0;
	struct phy_device *phydev = NULL;
	sw_error_t rv = SW_OK;

	/* config aq phy ACT and LINK led behavior*/
	rv = aquantia_phy_get_phy_id (dev_id, phy_addr, &aq_phy_id);
	PHY_RTN_ON_ERROR(rv);
	if(aq_phy_id == AQUANTIA_PHY_113C_B0 || aq_phy_id == AQUANTIA_PHY_113C_B1)
	{
		rv = hsl_phy_mmd_reg_write(dev_id, phy_addr, A_TRUE,
			AQUANTIA_MMD_GLOBAL_REGISTERS,
			AQUANTIA_PROVISIONING_LED0_STATUS,
			AQUANTIA_LINK_ACT_LED_DISABLE_VALUE);
		PHY_RTN_ON_ERROR(rv);
		rv = hsl_phy_mmd_reg_write(dev_id, phy_addr, A_TRUE,
			AQUANTIA_MMD_GLOBAL_REGISTERS,
			AQUANTIA_PROVISIONING_LED1_STATUS,
			AQUANTIA_LINK_ACT_LED_VALUE);
		PHY_RTN_ON_ERROR(rv);
		rv = hsl_phy_mmd_reg_write(dev_id, phy_addr, A_TRUE,
			AQUANTIA_MMD_GLOBAL_REGISTERS,
			AQUANTIA_PROVISIONING_LED2_STATUS,
			AQUANTIA_LINK_ACT_LED_DISABLE_VALUE);
		PHY_RTN_ON_ERROR(rv);
	}
	else
	{
		rv = hsl_phy_mmd_reg_write(dev_id, phy_addr, A_TRUE,
			AQUANTIA_MMD_GLOBAL_REGISTERS,
			AQUANTIA_PROVISIONING_LED0_STATUS,
			AQUANTIA_LINK_ACT_LED_VALUE);
		PHY_RTN_ON_ERROR(rv);
		rv = hsl_phy_mmd_reg_write(dev_id, phy_addr, A_TRUE,
			AQUANTIA_MMD_GLOBAL_REGISTERS,
			AQUANTIA_PROVISIONING_LED1_STATUS,
			AQUANTIA_LINK_LED_VALUE);
		PHY_RTN_ON_ERROR(rv);
	}
	/*add all ability of aq phy*/
	rv = aquantia_phy_set_autoneg_adv(dev_id, phy_addr,
		FAL_PHY_ADV_XGE_SPEED_ALL | FAL_PHY_ADV_100TX_FD |
		FAL_PHY_ADV_10T_FD | FAL_PHY_ADV_1000T_FD);
	PHY_RTN_ON_ERROR(rv);
	/*update aquantia phy supported ability*/
	rv = hsl_phy_phydev_get(dev_id, phy_addr, &phydev);
	if(rv == SW_OK) {
		rv = hsl_phy_adv_to_linkmode_adv((FAL_PHY_ADV_XGE_SPEED_ALL |
			FAL_PHY_ADV_PAUSE | FAL_PHY_ADV_ASY_PAUSE |
			FAL_PHY_ADV_100TX_FD | FAL_PHY_ADV_10T_FD |
			FAL_PHY_ADV_1000T_FD), phydev->supported);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}
