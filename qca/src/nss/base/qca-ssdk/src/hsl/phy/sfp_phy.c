/*
 * Copyright (c) 2018, 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "sw.h"
#include "fal_port_ctrl.h"
#include "adpt.h"
#include "hsl_api.h"
#include "hsl.h"
#include "sfp_phy.h"
#include "aos_timer.h"
#include "hsl_phy.h"
#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/gpio.h>
#include "ssdk_plat.h"
#include "ssdk_phy_i2c.h"
#include "ssdk_dts.h"
#include "hppe_uniphy_reg.h"
#include "hppe_uniphy.h"
#include "hsl_port_prop.h"
#include <linux/mdio/mdio-i2c.h>
#include <linux/i2c.h>
#include <linux/phylink.h>
#include <linux/sfp.h>

static a_bool_t sfp_phy_drv_registered = A_FALSE;

static int sfp_eeprom_i2c_read(struct phy_device *phydev, u32 bus_addr,
	u8 dev_addr, void *buf, size_t len)
{
	struct mii_bus *miibus;
	struct qcom_mdio_i2c_data *bus_priv;
	struct i2c_msg msgs[2];
	size_t this_len;
	int ret;

	if (!phydev)
		return -EINVAL;
	miibus = phydev->mdio.bus;
	if (!miibus)
		return -EINVAL;

	if(!strstr(phydev->mdio.bus->id, "i2c")) {
		struct qca_phy_priv *priv = phydev->priv;
		return qca_i2c_data_get(priv->device_id, bus_addr, dev_addr, buf, len);
	}

	msgs[0].addr = bus_addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &dev_addr;
	msgs[1].addr = bus_addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = len;
	msgs[1].buf = buf;

	bus_priv = miibus->priv;
	while (len) {
		this_len = len;
		msgs[1].len = this_len;
		ret = i2c_transfer(bus_priv->i2c, msgs, ARRAY_SIZE(msgs));
		if (ret < 0)
			return ret;

		if (ret != ARRAY_SIZE(msgs))
			break;

		msgs[1].buf += this_len;
		dev_addr += this_len;
		len -= this_len;
	}

	return msgs[1].buf - (u8 *)buf;
}

static int
sfp_module_info(struct phy_device *phydev, struct ethtool_modinfo *modinfo)
{
	a_uint8_t mod_compliance = 0, addrmode = 0;
	int ret;

	ret  = sfp_eeprom_i2c_read(phydev, 0x50, 0x5e, &mod_compliance, 1);
	if (ret < 0)
		return ret;
	ret = sfp_eeprom_i2c_read(phydev, 0x50, 0x5c, &addrmode, 1);
	if (ret < 0)
		return ret;
	if ((mod_compliance != 0) && !(addrmode & BIT(2))) {
		modinfo->type = ETH_MODULE_SFF_8472;
		modinfo->eeprom_len = ETH_MODULE_SFF_8472_LEN;
	} else {
		modinfo->type = ETH_MODULE_SFF_8079;
		modinfo->eeprom_len = ETH_MODULE_SFF_8079_LEN;
	}

	return 0;
}

static int
sfp_module_eeprom(struct phy_device *phydev, struct ethtool_eeprom *ee, u8 *data)
{
	a_uint32_t f, l, len;
	int ret;

	if (ee->len == 0)
		return -EINVAL;

	/* the first byte is ee->offset, and the last byte is ee->offset + ee->len */
	f = ee->offset;
	l = ee->offset + ee->len;
	/* read the information of 0x50 */
	if (f < ETH_MODULE_SFF_8079_LEN) {
		len = (l > ETH_MODULE_SFF_8079_LEN) ? ETH_MODULE_SFF_8079_LEN : l;
		len -= f;
		ret = sfp_eeprom_i2c_read(phydev, 0x50, f, data, len);
		if (ret < 0)
			return ret;
		f += len;
		data += len;
	}
	/* read the information of 0x51 */
	if (f < ETH_MODULE_SFF_8472_LEN && l > ETH_MODULE_SFF_8079_LEN) {
		len = (l > ETH_MODULE_SFF_8472_LEN) ? ETH_MODULE_SFF_8472_LEN : l;
		len -= f;
		f -= ETH_MODULE_SFF_8079_LEN;

		ret = sfp_eeprom_i2c_read(phydev, 0x51, f, data, len);
		if (ret < 0)
			return ret;
	}

	return 0;
}

static int
sfp_phy_probe(struct phy_device *pdev)
{
	fal_port_t port;
	struct qca_phy_priv *priv = pdev->priv;

	port = qca_ssdk_phydev_to_port(priv->device_id, pdev);
	if (A_TRUE == hsl_port_feature_get(priv->device_id, port, PHY_F_SFP_SGMII)) {
		pdev->autoneg = AUTONEG_ENABLE;
	} else {
		pdev->autoneg = AUTONEG_DISABLE;
	}
	return 0;
}

static void
sfp_phy_remove(struct phy_device *pdev)
{
	return;
}

static int
sfp_phy_config_aneg(struct phy_device *pdev)
{

	return 0;
}

static int
sfp_phy_aneg_done(struct phy_device *pdev)
{

	return SFP_ANEG_DONE;
}

sw_error_t
sfp_port_status_get_from_uniphy(a_uint32_t dev_id, a_uint32_t port_id,
	struct port_phy_status *phy_status)
{
	phy_info_t *phy_info = NULL;
	union uniphy_channel0_input_output_6_u uniphy_channel0_input_output_6 = {0};
	union sr_xs_pcs_kr_sts1_u sr_xs_pcs_kr_sts1 = {0};
	a_bool_t rx_los_status = A_TRUE;
	a_uint32_t uniphy_index = 0, link_status = 0;
	sw_error_t rv = SW_OK;

	phy_status->link_status = PORT_LINK_DOWN;
	phy_status->speed = FAL_SPEED_BUTT;
	phy_status->duplex = FAL_DUPLEX_BUTT;

	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
	if(uniphy_index == SSDK_MAX_UNIPHY_INSTANCE)
		return SW_FAIL;
	phy_info = hsl_phy_info_get(dev_id);
	SW_RTN_ON_NULL(phy_info);
	if(phy_info->port_mode[port_id] == PORT_10GBASE_R)
	{
		rv = hppe_sr_xs_pcs_kr_sts1_get(dev_id, uniphy_index, &sr_xs_pcs_kr_sts1);
		SW_RTN_ON_ERROR(rv);
		link_status = sr_xs_pcs_kr_sts1.bf.plu;
	}
	else
	{
		hppe_uniphy_channel0_input_output_6_get(dev_id, uniphy_index,
			&uniphy_channel0_input_output_6);
		link_status =
			uniphy_channel0_input_output_6.bf.newaddedfromhere_ch0_link;
	}
	/*1G SFP would get link info from uniphy register directly,
	2.5G SFP and 10G SFP would get link info from rx los and uniphy*/
	if(link_status)
	{
		switch(phy_info->port_mode[port_id])
		{
			case PORT_SGMII_PLUS:
			case PORT_10GBASE_R:
				sfp_phy_rx_los_status_get(dev_id, port_id, &rx_los_status);
				SSDK_DEBUG("port %d rx_los_status is %x\n", port_id, rx_los_status);
				/*if uniphy is link up and rx los is 0, then link up,
				if uniphy is link up but rx los is 1, this is fake link*/
				if(rx_los_status)
				{
					return SW_OK;
				}
				phy_status->link_status = PORT_LINK_UP;
				phy_status->duplex = FAL_FULL_DUPLEX;
				if(phy_info->port_mode[port_id] == PORT_SGMII_PLUS)
					phy_status->speed = FAL_SPEED_2500;
				else
					phy_status->speed = FAL_SPEED_10000;
				return SW_OK;
			default:
				break;
		}
		phy_status->link_status = PORT_LINK_UP;
		switch(uniphy_channel0_input_output_6.bf.newaddedfromhere_ch0_speed_mode)
		{
			case UNIPHY_SPEED_1000M:
				phy_status->speed = FAL_SPEED_1000;
				break;
			case UNIPHY_SPEED_100M:
				phy_status->speed = FAL_SPEED_100;
				break;
			case UNIPHY_SPEED_10M:
				phy_status->speed = FAL_SPEED_10;
				break;
			default:
				phy_status->speed = FAL_SPEED_BUTT;
				break;
		}
		if(uniphy_channel0_input_output_6.bf.newaddedfromhere_ch0_duplex_mode)
		{
			phy_status->duplex = FAL_FULL_DUPLEX;
		}
		else
		{
			phy_status->duplex = FAL_HALF_DUPLEX;
		}
	}
	SSDK_DEBUG("phy_status->link_status:%d, phy_status->speed:%d, "
	"phy_status->duplex:%d\n", phy_status->link_status, phy_status->speed,
	phy_status->duplex);

	return SW_OK;
}

static int sfp_phy_read_abilities(struct phy_device *pdev)
{
	int features[] = {
		ETHTOOL_LINK_MODE_TP_BIT,
		ETHTOOL_LINK_MODE_100baseT_Full_BIT,
		ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
		ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
		ETHTOOL_LINK_MODE_2500baseT_Full_BIT,
		ETHTOOL_LINK_MODE_5000baseT_Full_BIT,
		ETHTOOL_LINK_MODE_Pause_BIT,
		ETHTOOL_LINK_MODE_Asym_Pause_BIT,
		ETHTOOL_LINK_MODE_Autoneg_BIT,
	};
	pdev->port = PORT_TP;
	linkmode_set_bit_array(features,
		ARRAY_SIZE(features),
		pdev->supported);
	linkmode_copy(pdev->advertising, pdev->supported);

	return 0;
}

static void
sfp_phy_parse_port(struct phy_device *pdev, const struct sfp_eeprom_id *sfp_id)
{
	a_uint8_t connector_type;
	a_bool_t is_fibre = A_FALSE;
	a_bool_t is_copper = A_FALSE;

	connector_type = sfp_id->base.connector;

	/* Determine if connector is fibre type */
	if (connector_type == SFF8024_CONNECTOR_SC ||
	    connector_type == SFF8024_CONNECTOR_FIBERJACK ||
	    connector_type == SFF8024_CONNECTOR_LC ||
	    connector_type == SFF8024_CONNECTOR_MT_RJ ||
	    connector_type == SFF8024_CONNECTOR_MU ||
	    connector_type == SFF8024_CONNECTOR_OPTICAL_PIGTAIL ||
	    connector_type == SFF8024_CONNECTOR_MPO_1X12 ||
	    connector_type == SFF8024_CONNECTOR_MPO_2X16) {
		is_fibre = A_TRUE;
	}

	/* Determine if connector is copper type */
	if (connector_type == SFF8024_CONNECTOR_RJ45) {
		is_copper = A_TRUE;
	}

	/* Handle unspecified connector with 1000BASE-T support */
	if (connector_type == SFF8024_CONNECTOR_UNSPEC && sfp_id->base.e1000_base_t) {
		is_copper = A_TRUE;
	}

	/* Configure PHY device based on connector type */
	if (is_fibre) {
		phylink_set(pdev->supported, FIBRE);
		pdev->port = PORT_FIBRE;
	} else if (is_copper || connector_type == SFF8024_CONNECTOR_COPPER_PIGTAIL) {
		phylink_set(pdev->supported, TP);
		pdev->port = PORT_TP;
	} else {
		/* Default to TP for other/unknown connector types */
		phylink_set(pdev->supported, TP);
		pdev->port = PORT_TP;
	}
}

static void
sfp_phy_parse_to_support(struct phy_device *pdev, const struct sfp_eeprom_id *id)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(supported_modes) = { 0, };
	a_uint32_t rate_lower_bound = 0, rate_upper_bound = 0, rate_nominal = 0;
	a_uint8_t nominal_br, ext_cc;
	a_bool_t has_passive_cable, has_active_cable;
	a_bool_t rate_valid = A_FALSE;

	sfp_phy_parse_port(pdev, id);

	nominal_br = id->base.br_nominal;

	/* Calculate bitrate boundaries */
	if (nominal_br != 0) {
		if (nominal_br != 255) {
			rate_nominal = nominal_br * 100;
			rate_lower_bound = rate_nominal - nominal_br * id->ext.br_min;
			rate_upper_bound = rate_nominal + nominal_br * id->ext.br_max;
			rate_valid = A_TRUE;
		} else {
			if (id->ext.br_max != 0) {
				rate_nominal = 250 * id->ext.br_max;
				rate_upper_bound = rate_nominal + rate_nominal * id->ext.br_min / 100;
				rate_lower_bound = rate_nominal - rate_nominal * id->ext.br_min / 100;
				rate_valid = A_TRUE;
			}
		}
		if (rate_lower_bound == rate_upper_bound && id->base.sfp_ct_passive) {
			rate_lower_bound = 0;
		}
	}

	has_passive_cable = id->base.sfp_ct_passive ? A_TRUE : A_FALSE;
	has_active_cable = id->base.sfp_ct_active ? A_TRUE : A_FALSE;

	/* Process 10G Ethernet compliance codes */
	if (id->base.e10g_base_sr) phylink_set(supported_modes, 10000baseSR_Full);
	if (id->base.e10g_base_lr) phylink_set(supported_modes, 10000baseLR_Full);
	if (id->base.e10g_base_lrm) phylink_set(supported_modes, 10000baseLRM_Full);
	if (id->base.e10g_base_er) phylink_set(supported_modes, 10000baseER_Full);

	/* Process 1G Ethernet compliance codes */
	if (id->base.e1000_base_sx || id->base.e1000_base_lx || id->base.e1000_base_cx) {
		phylink_set(supported_modes, 1000baseX_Full);
	}
	if (id->base.e1000_base_t) {
		phylink_set(supported_modes, 1000baseT_Half);
		phylink_set(supported_modes, 1000baseT_Full);
	}
	if ((id->base.e_base_px || id->base.e_base_bx10) &&
	    rate_lower_bound <= 1300 && rate_upper_bound >= 1200) {
		phylink_set(supported_modes, 1000baseX_Full);
	}

	/* Process 100M Ethernet compliance codes */
	if (id->base.e100_base_fx || id->base.e100_base_lx) {
		phylink_set(supported_modes, 100baseFX_Full);
	}
	if ((id->base.e_base_px || id->base.e_base_bx10) && rate_nominal == 100) {
		phylink_set(supported_modes, 100baseFX_Full);
	}

	/* Process cable types with bitrate analysis */
	if ((has_passive_cable || has_active_cable) && rate_valid) {
		if (rate_lower_bound <= 12000 && rate_upper_bound >= 10300) {
			phylink_set(supported_modes, 10000baseCR_Full);
		}
		if (rate_lower_bound <= 3200 && rate_upper_bound >= 3100) {
			phylink_set(supported_modes, 2500baseX_Full);
		}
		if (rate_lower_bound <= 1300 && rate_upper_bound >= 1200) {
			phylink_set(supported_modes, 1000baseX_Full);
		}
	}

	/* Process passive cable specific attributes */
	if (has_passive_cable && id->base.passive.sff8431_app_e) {
		phylink_set(supported_modes, 10000baseCR_Full);
	}

	/* Process active cable specific attributes */
	if (has_active_cable) {
		if (id->base.active.sff8431_app_e || id->base.active.sff8431_lim) {
			phylink_set(supported_modes, 10000baseCR_Full);
		}
	}

	/* Process extended compliance code */
	ext_cc = id->base.extended_cc;
	if (ext_cc == SFF8024_ECC_100G_25GAUI_C2M_AOC) {
		if (rate_lower_bound <= 28000 && rate_upper_bound >= 25000) {
			phylink_set(supported_modes, 25000baseSR_Full);
		}
	} else if (ext_cc == SFF8024_ECC_100GBASE_SR4_25GBASE_SR) {
		phylink_set(supported_modes, 100000baseSR4_Full);
		phylink_set(supported_modes, 25000baseSR_Full);
	} else if (ext_cc == SFF8024_ECC_100GBASE_LR4_25GBASE_LR ||
		ext_cc == SFF8024_ECC_100GBASE_ER4_25GBASE_ER) {
		phylink_set(supported_modes, 100000baseLR4_ER4_Full);
	} else if (ext_cc == SFF8024_ECC_100GBASE_LR4_25GBASE_LR ||
		ext_cc == SFF8024_ECC_100GBASE_ER4_25GBASE_ER) {
		phylink_set(supported_modes, 100000baseLR4_ER4_Full);
	} else if (ext_cc == SFF8024_ECC_100GBASE_CR4) {
		phylink_set(supported_modes, 100000baseCR4_Full);
		phylink_set(supported_modes, 25000baseCR_Full);
	} else if (ext_cc == SFF8024_ECC_25GBASE_CR_S ||
		ext_cc == SFF8024_ECC_25GBASE_CR_N) {
		phylink_set(supported_modes, 25000baseCR_Full);
	} else if (ext_cc == SFF8024_ECC_10GBASE_T_SFI ||
		ext_cc == SFF8024_ECC_10GBASE_T_SR) {
		phylink_set(supported_modes, 10000baseT_Full);
	} else if (ext_cc == SFF8024_ECC_5GBASE_T) {
		phylink_set(supported_modes, 5000baseT_Full);
	} else if (ext_cc == SFF8024_ECC_2_5GBASE_T) {
		phylink_set(supported_modes, 2500baseT_Full);
	}

	/* Process Fibre Channel speeds */
	if (id->base.fc_speed_100 || id->base.fc_speed_200 || id->base.fc_speed_400) {
		if (nominal_br >= 31) phylink_set(supported_modes, 2500baseX_Full);
		if (nominal_br >= 12) phylink_set(supported_modes, 1000baseX_Full);
	}

	/* Fallback bitrate-based mode detection */
	if (bitmap_empty(supported_modes, __ETHTOOL_LINK_MODE_MASK_NBITS) && rate_nominal != 0) {
		if (rate_lower_bound <= 1300 && rate_upper_bound >= 1200) {
			phylink_set(supported_modes, 1000baseX_Full);
		}
		if (rate_lower_bound <= 3200 && rate_upper_bound >= 2500) {
			phylink_set(supported_modes, 2500baseX_Full);
		}
	}

	/* Add standard link capabilities */
	phylink_set(supported_modes, Autoneg);
	phylink_set(supported_modes, Pause);
	phylink_set(supported_modes, Asym_Pause);

	/* Update PHY device capabilities */
	linkmode_or(pdev->supported, pdev->supported, supported_modes);
	linkmode_copy(pdev->advertising, pdev->supported);
}

static int
sfp_link_mode_adjust(struct phy_device *pdev)
{
	struct sfp_eeprom_id sfp_id;
	int ret;

	linkmode_zero(pdev->supported);
	linkmode_zero(pdev->advertising);
	/* Read base EEPROM data (0x00-0x5F) */
	ret = sfp_eeprom_i2c_read(pdev, 0x50, 0, &sfp_id.base, sizeof(sfp_id.base));
	if (ret < 0 || ret != sizeof(sfp_id.base))
		goto default_setting;

	/* Read extended EEPROM data (0x60-0x7F) */
	ret = sfp_eeprom_i2c_read(pdev, 0x50, sizeof(sfp_id.base), &sfp_id.ext, sizeof(sfp_id.ext));
	if (ret < 0 || ret != sizeof(sfp_id.ext))
		goto default_setting;

	sfp_phy_parse_to_support(pdev, &sfp_id);
	/* Apply the parsed capabilities to the PHY device */
	if (!linkmode_empty(pdev->supported))
		return 0;
	default_setting:
	/* apply the default setting */
	sfp_phy_read_abilities(pdev);

	return 0;
}

static int
sfp_read_status(struct phy_device *pdev)
{
	fal_port_t port;
	struct qca_phy_priv *priv = pdev->priv;
#ifdef MP
	a_uint32_t port_mode = 0, uniphy_index = 0, uniphy_mode = 0;
	adpt_api_t *p_api = NULL;
	struct port_phy_status phy_status = {0};
	phy_info_t *phy_info = NULL;
#endif

	port = qca_ssdk_phydev_to_port(priv->device_id, pdev);
	if(port == 0)
		return -ENXIO;
#ifdef MP
	phy_info = hsl_phy_info_get(priv->device_id);
	if(!phy_info)
		return -1;
	port_mode = phy_info->port_mode[port];
	sfp_phy_interface_get_mode_status(priv->device_id, port, &port_mode);
	SSDK_DEBUG("new port mode:0x%x, old port mode:%x\n", port_mode,
		phy_info->port_mode[port]);
	uniphy_index = hsl_port_to_uniphy(priv->device_id, port);
	if(port_mode != phy_info->port_mode[port])
	{
		uniphy_mode = hsl_port_mode_to_uniphy_mode(priv->device_id, port_mode);
		p_api = adpt_api_ptr_get(priv->device_id);
		if(!p_api)
			return -1;
		p_api->adpt_uniphy_mode_set(priv->device_id, uniphy_index, uniphy_mode);
		phy_info->port_mode[port] = port_mode;
		pdev->interface = hsl_port_mode_to_phydev_interface(priv->device_id, port_mode);
	}
	sfp_port_status_get_from_uniphy(priv->device_id, port, &phy_status);
	pdev->link = phy_status.link_status;
	pdev->speed = phy_status.speed;
	pdev->duplex = phy_status.duplex;
#else
	pdev->link = priv->ports[port].port_old_link;
	if(pdev->link == PORT_LINK_UP)
	{
		pdev->speed = priv->ports[port].port_old_speed;
		pdev->duplex = priv->ports[port].port_old_duplex;
	}
	else
	{
		pdev->speed = FAL_SPEED_BUTT;
		pdev->duplex = FAL_DUPLEX_BUTT;
	}
#endif
	sfp_link_mode_adjust(pdev);

	return 0;
}

static a_uint16_t
_sfp_phy_i2c_read(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t i2c_slaver,
	a_uint32_t reg_addr)
{
	sw_error_t rv = SW_OK;
	a_uint8_t rx[2] = { 0 };
	a_uint16_t reg_data = 0;

	rv = qca_i2c_data_get(dev_id, i2c_slaver, reg_addr & 0xff, rx, sizeof(rx));

	if (rv == SW_OK) {
		reg_data = (rx[0] << 8) | rx[1];
	} else {
		reg_data = 0xffff;
	}

	return reg_data;
}

static sw_error_t
sfp_phy_i2c_read(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t i2c_slaver,
	a_uint32_t reg_addr, a_uint16_t *reg_data)
{
	struct mii_bus *miibus;

	miibus = ssdk_port_miibus_get(dev_id, port_id);
	SW_RTN_ON_NULL(miibus);

	if(!strncmp(miibus->name, SFP_I2C_BUS, strlen(miibus->name)))
		*reg_data  = miibus->read(miibus, TO_MDIO_I2C_ADDR(i2c_slaver),
			reg_addr);
	else
		*reg_data = _sfp_phy_i2c_read(dev_id, port_id, i2c_slaver, reg_addr);

	return SW_OK;
}

static struct phy_driver sfp_phy_driver = {
	.flags		= PHY_IS_INTERNAL,
	.name		= "QCA SFP",
	.phy_id		= SFP_PHY,
	.phy_id_mask = SFP_PHY_MASK,
	.probe		= sfp_phy_probe,
	.remove		= sfp_phy_remove,
	.config_aneg	= sfp_phy_config_aneg,
	.aneg_done	= sfp_phy_aneg_done,
	.read_status	= sfp_read_status,
	.get_features	= sfp_phy_read_abilities,
	.module_info	= sfp_module_info,
	.module_eeprom	= sfp_module_eeprom,
	.mdiodrv.driver	= { .owner = THIS_MODULE },
};

int sfp_phy_device_setup(a_uint32_t dev_id, a_uint32_t port, a_uint32_t phy_id,
	void *priv)
{
	struct phy_device *phydev;
	a_uint32_t addr = 0;
	struct mii_bus *bus;
	int ret = 0;

	if (A_TRUE == hsl_port_phy_combo_capability_get(dev_id, port))
	{
		return 0;
	}
	/*create phy device*/
	addr = qca_ssdk_port_to_phy_addr(dev_id, port);
	bus = ssdk_phy_miibus_get(dev_id, addr);
	if(!bus)
		return SW_NOT_FOUND;
	addr = TO_PHY_ADDR(addr);
	phydev = phy_device_create(bus, addr, phy_id, false, NULL);
	if (IS_ERR(phydev) || phydev == NULL) {
		SSDK_ERROR("Failed to create phy device!\n");
		return SW_NOT_SUPPORTED;
	}
	phydev->priv = priv;
	/*register phy device*/
	ret = phy_device_register(phydev);
	if (ret) {
		phy_device_free(phydev);
		return ret;
	}

	return 0;
}

void sfp_phy_device_remove(a_uint32_t dev_id, a_uint32_t port)
{
	struct phy_device *phydev = NULL;
	a_uint32_t addr = 0;
	struct mii_bus *bus;

	if (A_TRUE == hsl_port_phy_combo_capability_get(dev_id, port))
	{
		return;
	}

	bus = ssdk_port_miibus_get(dev_id, port);
	if(!bus)
		return;

	addr = TO_PHY_ADDR(qca_ssdk_port_to_phy_addr(dev_id, port));
	if (addr < PHY_MAX_ADDR)
	if (bus->mdio_map[addr])
		phydev = to_phy_device(&bus->mdio_map[addr]->dev);
	if (phydev)
		phy_device_remove(phydev);
}

int sfp_phy_driver_register(void)
{
	int ret = 0;
	if(sfp_phy_drv_registered == A_FALSE)
	{
		ret = phy_driver_register(&sfp_phy_driver, THIS_MODULE);
		sfp_phy_drv_registered = A_TRUE;
	}
	return ret;
}

void sfp_phy_driver_unregister(void)
{
	if (sfp_phy_drv_registered == A_TRUE)
	{
		phy_driver_unregister(&sfp_phy_driver);
		sfp_phy_drv_registered = A_FALSE;
	}
}

int sfp_phy_init(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t bus_index)
{
	struct phy_device *phydev = NULL;
	struct mii_bus *bus = NULL;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	int ret;

	bus = ssdk_miibus_get(dev_id, bus_index);

	if (bus && strstr(bus->id, "i2c")) {
		hsl_phy_address_init(dev_id, port_id,
			TO_PHY_ADDR_E(FAL_SFP_PHY_ADDR, bus_index));
		/* if phydev existed, means the SFP is QCOM SFP module and */
		/* the phy device and driver are supported in qca-nss-phy */
		phydev = mdiobus_get_phy(bus, FAL_SFP_PHY_ADDR);
		if (phydev) {
			hsl_port_feature_clear(dev_id, port_id, PHY_F_SFP);
			if (phydev->is_c45)
				hsl_port_feature_set(dev_id, port_id, PHY_F_CLAUSE45);
			return 0;
		}
	}

	ret = sfp_phy_device_setup(dev_id, port_id, SFP_PHY, priv);
	if (ret < 0)
		return ret;
	ret = sfp_phy_driver_register();
	if (ret < 0)
		return ret;
	SSDK_INFO("SFP port %d init successfully\n", port_id);

	return 0;
}

void sfp_phy_exit(a_uint32_t dev_id)
{
	a_uint32_t port_id = 0;
	struct phy_device *phydev = NULL;

	for (port_id = 0; port_id < SW_MAX_NR_PORT; port_id ++) {
		if (!hsl_port_prop_check(dev_id, port_id, HSL_PP_PHY))
			continue;

		if (hsl_port_is_sfp(dev_id, port_id)) {
			hsl_port_phydev_get(dev_id, port_id, &phydev);
			if (phydev && (phydev->phy_id == SFP_PHY)) {
				sfp_phy_driver_unregister();
				sfp_phy_device_remove(dev_id, port_id);
			}
		}
	}
}

static a_uint16_t sfp_part_num[5] = {0x3330, 0x2d31, 0x3539, 0x392d, 0x3032};

a_bool_t
sfp_phy_part_number_check(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;
	a_uint16_t reg_data[5] = {0}, i = 0;

	for (i = 0; i < 5; i++) {
		rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_ADDR,
			SFP_E2PROM_PART_NUM_OFFSET + i *2, &reg_data[i]);
		if (rv != SW_OK)
			return A_FALSE;
		if (sfp_part_num[i] != reg_data[i])
			return A_FALSE;
	}
	return A_TRUE;
}

a_bool_t
sfp_phy_usxgmii_check(a_uint32_t dev_id, a_uint32_t port_id)
{
	if (sfp_phy_part_number_check(dev_id, port_id) == A_TRUE) {
		sw_error_t rv = SW_OK;
		a_uint16_t reg_data = 0;
		rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_EXTEND_ADDR,
			SFP_EXTEND_USXGMII_OFFSET, &reg_data);
		if (rv != SW_OK)
			return A_FALSE;
		if ((reg_data >> 8) & 0x1) {
			return A_TRUE;
		}
	}
	return A_FALSE;
}

a_bool_t
sfp_phy_present_status_check(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;
	a_bool_t mod_present_status = A_FALSE;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	/*if sfp_mode_present_pin is available, then need to check it*/
	if(priv && !qca_ssdk_gpio_is_invalid(dev_id, priv->ports[port_id].sfp_mod_present_pin))
	{
		rv = sfp_phy_mod_present_status_get(dev_id, port_id,
			&mod_present_status);
		if (rv != SW_OK)
			return A_FALSE;
		/*if sfp mod is not present, so no need to read SFP module with I2C*/
		if(!mod_present_status)
		{
			SSDK_DEBUG("port%d sfp mod is not present now\n", port_id);
			return A_FALSE;
		} else {
			return A_TRUE;
		}
	} else {
		/* backword compatible without present_pin case */
		return A_TRUE;
	}
}
sw_error_t sfp_phy_interface_get_mode_status(a_uint32_t dev_id,
	a_uint32_t port_id, fal_port_interface_mode_t *interface_mode_status)
{
	sw_error_t rv = SW_OK;
	a_uint16_t reg_data = 0, sfp_speed = 0;
	a_uint16_t sfp_type = 0;
	struct phy_device *phydev;

	if (sfp_phy_present_status_check(dev_id, port_id) == A_FALSE) {
		return SW_OK;
	}
	rv = hsl_port_phydev_get(dev_id, port_id, &phydev);
	SW_RTN_ON_ERROR(rv);
	rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_ADDR, SFP_SPEED_ADDR,
		&reg_data);
	SW_RTN_ON_ERROR(rv);
	sfp_speed = SFP_TO_SFP_SPEED(reg_data);
	SSDK_DEBUG("sfp_speed:%d\n", sfp_speed);

	if(sfp_speed >= SFP_SPEED_1000M &&
		sfp_speed < SFP_SPEED_2500M)
	{
		reg_data = 0;
		rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_ADDR, SFP_TYPE_ADDR,
			&reg_data);
		SW_RTN_ON_ERROR(rv);
		sfp_type = SFP_TO_SFP_TYPE(reg_data);
		if (sfp_type == SFP_TYPE_1000MBASE_T) {
			/* sfp copper module mode*/
			if (phydev->autoneg == AUTONEG_ENABLE) {
				/* sfp copper module interface is sgmii mode */
				*interface_mode_status = PHY_SGMII_BASET;
				hsl_port_feature_set(dev_id, port_id, PHY_F_SFP_SGMII);
			} else {
				/* sfp copper module interface is serdes mode */
				*interface_mode_status = PORT_SGMII_FIBER;
				hsl_port_feature_clear(dev_id, port_id, PHY_F_SFP_SGMII);
			}
		} else {
			/* sfp fiber module mode */
			*interface_mode_status = PORT_SGMII_FIBER;
			hsl_port_feature_clear(dev_id, port_id, PHY_F_SFP_SGMII);
		}
	}
	else if(sfp_speed >= SFP_SPEED_10000M &&
		sfp_speed < SFP_SPEED_25000M)
	{
		*interface_mode_status = PORT_10GBASE_R;
		if (sfp_phy_usxgmii_check(dev_id, port_id) == A_TRUE) {
			*interface_mode_status = PORT_USXGMII;
		}
	}
	else if(sfp_speed >= SFP_SPEED_2500M &&
		sfp_speed < SFP_SPEED_5000M)
	{
		struct port_phy_status sfp_status= {0};
		adpt_api_t *p_api;

		SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
		rv = p_api->adpt_port_phy_status_get(dev_id, port_id, &sfp_status);
		SW_RTN_ON_ERROR(rv);

		if (sfp_status.link_status == PORT_LINK_DOWN) {
			/*autoneg is for different speed switching on one SFP module*/
			if (phydev->autoneg == AUTONEG_ENABLE) {
				if (sfp_status.link_status == PORT_LINK_DOWN) {
					if (*interface_mode_status == PHY_SGMII_BASET) {
						*interface_mode_status = PORT_SGMII_PLUS;
					} else {
						/* sfp copper module interface is serdes mode */
						*interface_mode_status = PHY_SGMII_BASET;
						hsl_port_feature_clear(dev_id, port_id,
							PHY_F_SFP_SGMII);
					}
				}
			} else {
				*interface_mode_status = PORT_SGMII_PLUS;
			}
			return SW_OK;
		}
		if (sfp_status.link_status == PORT_LINK_UP) {
			/*solution for SFP link up case under 10G-R mode*/
			if ((*interface_mode_status != PHY_SGMII_BASET) &&
				(*interface_mode_status != PORT_SGMII_PLUS)) {
				*interface_mode_status = PORT_SGMII_PLUS;
			}
			return SW_OK;
		}
	}
	else
	{
		return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

sw_error_t
sfp_phy_phydev_adv_update(a_uint32_t dev_id, a_uint32_t phy_addr, a_uint32_t adv_mask,
	a_uint32_t adv)
{
	sw_error_t rv = SW_OK;
	a_uint32_t new_adv = 0;
	struct phy_device *phydev = NULL;

	rv = hsl_phy_phydev_get(dev_id, phy_addr, &phydev);
	SW_RTN_ON_ERROR (rv);
	rv = hsl_phy_linkmode_adv_to_adv(phydev->advertising, &new_adv);
	SW_RTN_ON_ERROR (rv);
	new_adv &= ~adv_mask;
	new_adv |= adv;

	rv = hsl_phy_phydev_autoneg_update(dev_id, phy_addr, A_TRUE, new_adv);

	return rv;
}

sw_error_t
sfp_phy_rx_los_status_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_bool_t *rx_los_status)
{
	a_uint32_t rx_los_pin = 0;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	if(!priv)
		return SW_NOT_INITIALIZED;
	rx_los_pin = priv->ports[port_id].sfp_rx_los_pin;
	if(qca_ssdk_gpio_is_invalid(dev_id, rx_los_pin))
	{
		return SW_NOT_SUPPORTED;
	}
	*rx_los_status = gpio_get_value(rx_los_pin);
	SSDK_DEBUG("port%d sfp rx_los_pin is %d, the rx_los_status is %d",
		port_id, rx_los_pin, *rx_los_status);

	return SW_OK;
}

sw_error_t
sfp_phy_tx_dis_status_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_bool_t tx_dis_status)
{
	a_uint32_t tx_dis_pin = 0;
	int ret = 0;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	if(!priv)
		return SW_NOT_INITIALIZED;
	tx_dis_pin = priv->ports[port_id].sfp_tx_dis_pin;
	if(qca_ssdk_gpio_is_invalid(dev_id, tx_dis_pin))
	{
		return SW_NOT_SUPPORTED;
	}
	ret = gpio_request(tx_dis_pin, "sfp_tx_dis_pin");
	if(ret)
	{
		SSDK_ERROR("tx_dis_pin request failed, ret:%d\n", ret);
		return SW_FAIL;
	}
	ret = gpio_direction_output(tx_dis_pin, tx_dis_status);
	gpio_free(tx_dis_pin);
	if(ret)
	{
		SSDK_ERROR("tx_dis_pin configure faied, ret:%d\n", ret);
		return SW_FAIL;
	}

	return SW_OK;
}

sw_error_t
sfp_phy_tx_dis_status_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_bool_t *tx_dis_status)
{
	a_uint32_t tx_dis_pin = 0;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	if(!priv)
		return SW_NOT_INITIALIZED;
	tx_dis_pin = priv->ports[port_id].sfp_tx_dis_pin;
	if(qca_ssdk_gpio_is_invalid(dev_id, tx_dis_pin))
	{
		return SW_NOT_SUPPORTED;
	}
	*tx_dis_status = gpio_get_value(tx_dis_pin);
	SSDK_DEBUG("port%d sfp tx_dis_pin is %d, the tx_dis_status is %d",
		port_id, tx_dis_pin, *tx_dis_status);

	return SW_OK;
}

sw_error_t
sfp_phy_mod_present_status_get(a_uint32_t dev_id, a_uint32_t port_id,
	a_bool_t *mod_present_status)
{
	a_uint32_t mod_present_pin = 0;
	a_bool_t mod_present_pin_status = A_FALSE;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	if(!priv)
		return SW_NOT_INITIALIZED;
	mod_present_pin = priv->ports[port_id].sfp_mod_present_pin;
	if(qca_ssdk_gpio_is_invalid(dev_id, mod_present_pin))
	{
		return SW_NOT_SUPPORTED;
	}
	mod_present_pin_status = gpio_get_value(mod_present_pin);
	SSDK_DEBUG("port%d sfp mod_present_pin is %d, mod_present_pin_status is %d",
		port_id, mod_present_pin, mod_present_pin_status);
	if(mod_present_pin_status)
		*mod_present_status = A_FALSE;
	else
		*mod_present_status = A_TRUE;

	return SW_OK;
}

sw_error_t
sfp_phy_medium_status_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_bool_t sfp_medium_status)
{
	a_uint32_t sfp_medium_pin = 0;
	int ret = 0;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	sfp_medium_pin = priv->ports[port_id].sfp_medium_pin;
	if(qca_ssdk_gpio_is_invalid(dev_id, sfp_medium_pin))
	{
		return SW_NOT_SUPPORTED;
	}
	ret = gpio_request(sfp_medium_pin, "sfp_medium_pin");
	if(ret)
	{
		SSDK_ERROR("sfp_medium_pin request failed, ret:%d\n", ret);
		return SW_FAIL;
	}
	if(sfp_medium_status)
		ret = gpio_direction_output(sfp_medium_pin, 1);
	else
		ret = gpio_direction_output(sfp_medium_pin, 0);
	gpio_free(sfp_medium_pin);
	if(ret)
	{
		SSDK_ERROR("sfp_medium_pin configure faied, ret:%d\n", ret);
		return SW_FAIL;
	}

	return SW_OK;
}

sw_error_t
sfp_phy_port_status_get(a_uint32_t dev_id, a_uint32_t port_id,
		struct port_phy_status *phy_status)
{
	sw_error_t rv = SW_OK;
	a_uint16_t reg_data = 0, phy_link = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	if (sfp_phy_present_status_check(dev_id, port_id) == A_FALSE) {
		return SW_OK;
	}
	if (sfp_phy_part_number_check(dev_id, port_id) == A_TRUE) {
		rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_EXTEND_ADDR,
			SFP_EXTEND_LINK_OFFSET, &reg_data);
		SW_RTN_ON_ERROR(rv);

		phy_link = (reg_data >> 8) & 0x1;
		if (phy_link) {
			a_uint16_t speed_data = 0xff, retries = 50;
			phy_status->link_status = PORT_LINK_UP;
			if (reg_data & 0x1) {
				phy_status->duplex = FAL_HALF_DUPLEX;
			} else {
				phy_status->duplex = FAL_FULL_DUPLEX;
			}
			rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_EXTEND_ADDR,
					SFP_EXTEND_SPEED_OFFSET, &reg_data);
			SW_RTN_ON_ERROR(rv);
			speed_data = (reg_data >> 0x8) & 0x7;
			while ((speed_data != SFP_PHY_SPEED_10000M) &&
				(speed_data != SFP_PHY_SPEED_5000M) &&
				(speed_data != SFP_PHY_SPEED_2500M) &&
				(speed_data != SFP_PHY_SPEED_1000M) &&
				(speed_data != SFP_PHY_SPEED_100M)) {
				aos_mdelay(10);
				if (retries-- == 0) {
					SSDK_ERROR("usxgmii sfp port speed sync failed!\n");
					break;
				}
				rv = sfp_phy_i2c_read(dev_id, port_id, SFP_E2PROM_EXTEND_ADDR,
					SFP_EXTEND_SPEED_OFFSET, &reg_data);
				SW_RTN_ON_ERROR(rv);
				speed_data = (reg_data >> 0x8) & 0x7;
			}
			switch (speed_data) {
				case SFP_PHY_SPEED_10000M:
					phy_status->speed = FAL_SPEED_10000;
					break;
				case SFP_PHY_SPEED_5000M:
					phy_status->speed = FAL_SPEED_5000;
					break;
				case SFP_PHY_SPEED_2500M:
					phy_status->speed = FAL_SPEED_2500;
					break;
				case SFP_PHY_SPEED_1000M:
					phy_status->speed = FAL_SPEED_1000;
					break;
				case SFP_PHY_SPEED_100M:
					phy_status->speed = FAL_SPEED_100;
					break;
				default:
					phy_status->speed = FAL_SPEED_BUTT;
					break;
			}
		} else {
			phy_status->link_status = PORT_LINK_DOWN;
			phy_status->speed = FAL_SPEED_BUTT;
			phy_status->duplex = FAL_DUPLEX_BUTT;
		}
	}

	return rv;
}
