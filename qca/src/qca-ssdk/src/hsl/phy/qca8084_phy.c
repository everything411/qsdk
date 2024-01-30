/*
 * Copyright (c) 2021-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "sw.h"
#include "hsl_phy.h"
#include "ssdk_plat.h"
#include "ssdk_mht_pinctrl.h"
#include "qca808x_phy.h"
#include "qca8084_phy.h"
#include "mht_sec_ctrl.h"
#include "mht_interface_ctrl.h"
#include "ssdk_mht_clk.h"

static a_uint32_t
qca8084_phy_icc[SSDK_PHYSICAL_PORT4+1];

static sw_error_t
qca8084_phy_icc_init(a_uint32_t dev_id, a_uint32_t phy_addr)
{
	sw_error_t rv = SW_OK;
	a_uint32_t icc_value = 0, mht_port_id = 0;

	rv = qca_mht_port_id_get(dev_id, phy_addr, &mht_port_id);
	SW_RTN_ON_ERROR(rv);
	rv = qca_mht_ethphy_icc_efuse_get(dev_id, mht_port_id, &icc_value);
	SW_RTN_ON_ERROR(rv);

	qca8084_phy_icc[mht_port_id] = icc_value;
	SSDK_DEBUG("dev_id %d, mht_port_id %d icc value is 0x%x\n",
		dev_id, mht_port_id, icc_value);

	return SW_OK;
}

static sw_error_t
qca8084_phy_icc_fix_up(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_speed_t speed)
{
	sw_error_t rv = SW_OK;
	a_uint32_t icc_value = 0, mht_port_id = 0;

	rv = qca_mht_port_id_get(dev_id, phy_addr, &mht_port_id);
	SW_RTN_ON_ERROR(rv);
	/*retrim icc value for link up 100M, and set orginal icc value for link down
		and other speeds*/
	if(speed == FAL_SPEED_100)
	{
		if(qca8084_phy_icc[mht_port_id] < (0x1f-3))
			icc_value = qca8084_phy_icc[mht_port_id]+3;
		else
			icc_value = 0x1f;
	}
	else
	{
		icc_value = qca8084_phy_icc[mht_port_id];
	}
	SSDK_DEBUG("dev_id:%d mht_port_id:%d icc value is 0x%x\n", dev_id, mht_port_id, icc_value);
	rv = qca808x_phy_modify_debug(dev_id, phy_addr, QCA8084_PHY_DEBUG_ANA_ICC,
		QCA8084_PHY_DEBUG_ANA_ICC_MASK, icc_value);
	aos_mdelay(10);

	return rv;
}

sw_error_t
qca8084_phy_ipg_config(a_uint32_t dev_id, a_uint32_t phy_id,
	fal_port_speed_t speed)
{
	sw_error_t rv = SW_OK;
	a_uint16_t phy_data = 0;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA808X_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_IPG_10_11_ENABLE);
	PHY_RTN_ON_READ_ERROR(phy_data);

	phy_data &= ~QCA8084_PHY_MMD7_IPG_11_EN;
	/*If speed is 1G, enable 11 ipg tuning*/
	SSDK_DEBUG("if speed is 1G, enable 11 ipg tuning\n");
	if(speed == FAL_SPEED_1000)
		phy_data |= QCA8084_PHY_MMD7_IPG_11_EN;

	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA808X_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_IPG_10_11_ENABLE, phy_data);

	return rv;
}

static sw_error_t
qca8084_phy_sgmii_mode_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	fal_port_interface_mode_t interface_mode)
{
	sw_error_t rv = SW_OK;
	a_uint32_t phy_addr_tmp = 0;
	fal_mac_config_t config = {0};

	if(interface_mode == PHY_SGMII_BASET)
		config.mac_mode = FAL_MAC_MODE_SGMII;
	else if(interface_mode == PORT_SGMII_PLUS)
		config.mac_mode = FAL_MAC_MODE_SGMII_PLUS;
	else
		return SW_NOT_SUPPORTED;
	config.config.sgmii.clock_mode = FAL_INTERFACE_CLOCK_PHY_MODE;
	config.config.sgmii.auto_neg = A_TRUE;

	rv = qca_mht_ephy_addr_get(dev_id, SSDK_PHYSICAL_PORT4, &phy_addr_tmp);
	SW_RTN_ON_ERROR (rv);
	if(phy_addr_tmp != phy_addr)
	{
		SSDK_ERROR("phy_addr:0x%x is not matched with port4 phy addr:0x%x\n",
			phy_addr, phy_addr_tmp);
		return SW_NOT_SUPPORTED;
	}

	rv = mht_interface_sgmii_mode_set(dev_id, MHT_UNIPHY_SGMII_0,
		SSDK_PHYSICAL_PORT4, &config);

	return rv;
}

static sw_error_t
qca8084_phy_interface_get_mode_by_speed(a_uint32_t dev_id, a_uint32_t phy_id,
	fal_port_speed_t speed, fal_port_interface_mode_t *interface_mode_status)
{
	a_uint32_t mht_port_id = 0;
	sw_error_t rv = SW_OK;

	rv = qca_mht_port_id_get(dev_id, phy_id, &mht_port_id);
	SW_RTN_ON_ERROR(rv);

	*interface_mode_status = PORT_INTERFACE_MODE_MAX;

	if(mht_port_id >= SSDK_PHYSICAL_PORT1 && mht_port_id <= SSDK_PHYSICAL_PORT3)
	{
		/*if uniphy1 is UQXGMII mode, then port interface mode is PORT_UQXGMII,
			if not, then port1~port3 is switch port and port interface mode is
			PORT_INTERFACE_MODE_MAX*/
		if(mht_uniphy_mode_check(dev_id, MHT_UNIPHY_SGMII_1, MHT_UNIPHY_UQXGMII))
		{
			*interface_mode_status = PORT_UQXGMII;
		}
	}
	else
	{
		/*if uniphy0 is mac mode, then there are two case: 1. uniphy0 is not
			used(sgmii and mac mode in default), and if uniphy1 is uqxgmii,
			then port4 interface mode is PORT_UQXGMII, 2. uniphy0 is used as
			sgmii or sgmii+, means uniphy0 is used for switch mode, the the
			port4 interface mode is PORT_INTERFACE_MODE_MAX*/
		if(mht_uniphy_mode_check(dev_id, MHT_UNIPHY_SGMII_0, MHT_UNIPHY_MAC)
			&& mht_uniphy_mode_check(dev_id, MHT_UNIPHY_SGMII_1,
			MHT_UNIPHY_UQXGMII))
		{
			*interface_mode_status = PORT_UQXGMII;
		}
		/*if uniphy0 is phy mode, then uniphy0 connect to port4, so the current
			interface mode depend on current speed, sgmii+ for 2.5G and sgmii
			for 1G/100M/10M*/
		if(mht_uniphy_mode_check(dev_id, MHT_UNIPHY_SGMII_0, MHT_UNIPHY_PHY))
		{
			if(speed == FAL_SPEED_2500)
				*interface_mode_status = PORT_SGMII_PLUS;
			else
				*interface_mode_status = PHY_SGMII_BASET;
		}
	}

	return SW_OK;
}

sw_error_t
qca8084_phy_interface_get_mode_status(a_uint32_t dev_id, a_uint32_t phy_id,
	fal_port_interface_mode_t *interface_mode_status)
{
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	*interface_mode_status =
		phy_info->port_mode[qca_ssdk_phy_addr_to_port (dev_id, phy_id)];
	SSDK_DEBUG("dev_id :0x%x, phy_id:0x%x, interface mode: 0x%x\n",
		dev_id, phy_id, *interface_mode_status);

	return SW_OK;
}

sw_error_t
qca8084_phy_fifo_reset(a_uint32_t dev_id, a_uint32_t phy_addr, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_uint16_t phy_data = 0;

	phy_data = qca808x_phy_reg_read (dev_id, phy_addr, QCA8084_PHY_FIFO_CONTROL);
	PHY_RTN_ON_READ_ERROR(phy_data);

	if(enable)
		phy_data &= ~QCA8084_PHY_FIFO_RESET;
	else
		phy_data |= QCA8084_PHY_FIFO_RESET;

	rv = qca808x_phy_reg_write(dev_id, phy_addr, QCA8084_PHY_FIFO_CONTROL,
		phy_data);

	return rv;
}

static sw_error_t
_qca8084_phy_interface_mode_fixup(a_uint32_t dev_id, a_uint32_t phy_id,
	fal_port_speed_t speed)
{
	sw_error_t rv = SW_OK;
	fal_port_interface_mode_t mode_new = 0;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);
	a_uint32_t port_id = qca_ssdk_phy_addr_to_port(dev_id, phy_id);

	rv = qca8084_phy_interface_get_mode_by_speed(dev_id, phy_id, speed, &mode_new);
	SW_RTN_ON_ERROR (rv);
	SSDK_DEBUG("dev_id :0x%x, phy_id:0x%x, interface mode: 0x%x\n", dev_id, phy_id,
		mode_new);

	/*only SGMII and SGMII+ may need to change interface mode*/
	if(mode_new != PHY_SGMII_BASET && mode_new != PORT_SGMII_PLUS)
		return SW_OK;

	if(phy_info->port_mode[port_id] != mode_new)
	{
		SSDK_DEBUG("interface mode change from mode 0x%x to mode 0x%x\n",
			phy_info->port_mode[port_id], mode_new);
		rv = qca8084_phy_sgmii_mode_set(dev_id, phy_id, mode_new);
		SW_RTN_ON_ERROR (rv);
		phy_info->port_mode[port_id] = mode_new;
	}

	return SW_OK;
}

static sw_error_t
qca8084_phy_pll_disable(a_uint32_t dev_id, a_uint32_t phy_addr)
{
	sw_error_t rv = SW_OK;

	rv = qca808x_phy_debug_write( dev_id, phy_addr,
		QCA8084_PHY_CONTROL_DEBUG_REGISTER0,
		QCA8084_PHY_CONTROL_DEBUG_REGISTER0_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_debug_write( dev_id, phy_addr,
		QCA8084_PHY_AFE25_CMN_6_MII, QCA8084_PHY_AFE25_CMN_6_MII_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_debug_write( dev_id, phy_addr,
		QCA8084_PHY_AFE25_CMN_9_MII, QCA8084_PHY_AFE25_CMN_9_MII_VAL);

	return rv;
}

sw_error_t
qca8084_phy_interface_set_mode(a_uint32_t dev_id, a_uint32_t phy_id,
		fal_port_interface_mode_t interface_mode)
{
	sw_error_t rv = SW_OK;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);
	a_uint32_t port_id  = 0, mht_port_id = 0, phy_addr = 0;

	switch (interface_mode) {
		case PORT_UQXGMII:
			SSDK_INFO("configure manhattan phy as PORT_UQXGMII\n");
			if(qca_mht_sku_check(dev_id, MHT_SKU_8082))
			{
				/*for qca8082, mht port 1, 2 is disabled, so need power down and
					disable below PLL to save power*/
				for(mht_port_id = SSDK_PHYSICAL_PORT1;
					mht_port_id <= SSDK_PHYSICAL_PORT2; mht_port_id++)
				{
					rv = qca_mht_ephy_addr_get(dev_id, mht_port_id, &phy_addr);
					SW_RTN_ON_ERROR(rv);
					rv = qca808x_phy_poweroff(dev_id, phy_addr);
					SW_RTN_ON_ERROR(rv);
					rv = qca8084_phy_pll_disable(dev_id, phy_addr);
					SW_RTN_ON_ERROR(rv);
				}
			}
			rv = qca_mht_mem_ctrl_set(dev_id, MHT_MEM_CTRL_DVS_PHY_MODE,
				MHT_MEM_ACC_0_PHY_MODE);
			SW_RTN_ON_ERROR (rv);
			/*the work mode is PORT_UQXGMII in default*/
			rv = mht_interface_uqxgmii_mode_set(dev_id);
			SW_RTN_ON_ERROR (rv);
			/*init clock for PORT_UQXGMII*/
			ssdk_mht_gcc_clock_init(dev_id, MHT_PHY_UQXGMII_MODE, 0);

			/*init pinctrl for phy mode*/
			rv = ssdk_mht_pinctrl_init(dev_id);

			/*init port mode*/
			for(port_id = SSDK_PHYSICAL_PORT1; port_id <= SSDK_PHYSICAL_PORT4;
				port_id++)
			{
				phy_info->port_mode[port_id] = PORT_UQXGMII;
			}
			break;
		case PHY_SGMII_BASET:
		case PORT_SGMII_PLUS:
			if(!qca_mht_sku_uniphy_enabled(dev_id, MHT_UNIPHY_SGMII_0))
			{
				SSDK_ERROR("MHT uniphy 0 is not enabled on the sku\n");
				return SW_NOT_SUPPORTED;
			}
			if(interface_mode ==
				phy_info->port_mode[qca_ssdk_phy_addr_to_port(dev_id, phy_id)])
				return SW_OK;
			/*need to configure work mode as MHT_PHY_SGMII_USXGMII_MODE*/
			rv = qca_mht_work_mode_set(dev_id, MHT_PHY_SGMII_UQXGMII_MODE);
			SW_RTN_ON_ERROR (rv);
			rv = qca8084_phy_sgmii_mode_set(dev_id, phy_id, interface_mode);
			SW_RTN_ON_ERROR (rv);
			/*port4 software reset*/
			SSDK_DEBUG(" ethphy3 software reset\n");
			rv = qca808x_phy_reset(dev_id, phy_id);
			SW_RTN_ON_ERROR (rv);
			phy_info->port_mode[qca_ssdk_phy_addr_to_port(dev_id, phy_id)] =
				interface_mode;
			break;
		default:
			rv = SW_NOT_SUPPORTED;
			SSDK_WARN("Unsupport interface mode: %d\n", interface_mode);
			break;
	}

	return rv;
}

sw_error_t
qca8084_phy_set_8023az(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t enable)
{
	a_uint16_t phy_data;
	sw_error_t rv = SW_OK;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL);
	PHY_RTN_ON_READ_ERROR(phy_data);

	if (enable == A_TRUE) {
		phy_data |= QCA8084_PHY_8023AZ_EEE_2500BT;
	} else {
		phy_data &= ~QCA8084_PHY_8023AZ_EEE_2500BT;
	}

	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL, phy_data);

	return rv;
}

sw_error_t
qca8084_phy_get_8023az(a_uint32_t dev_id, a_uint32_t phy_id, a_bool_t * enable)
{
	a_uint16_t phy_data;

	*enable = A_FALSE;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL);
	PHY_RTN_ON_READ_ERROR(phy_data);

	if (phy_data & QCA8084_PHY_8023AZ_EEE_2500BT) {
		*enable = A_TRUE;
	}

	return SW_OK;
}

sw_error_t
qca8084_phy_set_eee_adv(a_uint32_t dev_id, a_uint32_t phy_id,
	a_uint32_t adv)
{
	a_uint16_t phy_data = 0;
	sw_error_t rv = SW_OK;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL);
	PHY_RTN_ON_READ_ERROR(phy_data);
	phy_data &= ~(QCA8084_PHY_8023AZ_EEE_2500BT);

	if (adv & FAL_PHY_EEE_2500BASE_T) {
		phy_data |= QCA8084_PHY_8023AZ_EEE_2500BT;
	}

	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL, phy_data);

	return rv;
}

sw_error_t
qca8084_phy_get_eee_adv(a_uint32_t dev_id, a_uint32_t phy_id,
	a_uint32_t *adv)
{
	a_uint16_t phy_data = 0;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_CTRL);
	PHY_RTN_ON_READ_ERROR(phy_data);

	if (phy_data & QCA8084_PHY_8023AZ_EEE_2500BT) {
		*adv |= FAL_PHY_EEE_2500BASE_T;
	}

	return SW_OK;
}

sw_error_t
qca8084_phy_get_eee_partner_adv(a_uint32_t dev_id, a_uint32_t phy_id,
	a_uint32_t *adv)
{
	a_uint16_t phy_data = 0;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA8084_PHY_MMD7_NUM,
		QCA8084_PHY_MMD7_ADDR_8023AZ_EEE_2500M_PARTNER);
	PHY_RTN_ON_READ_ERROR(phy_data);

	if (phy_data & QCA8084_PHY_8023AZ_EEE_2500BT) {
		*adv |= FAL_PHY_EEE_2500BASE_T;
	}

	return SW_OK;
}

sw_error_t
qca8084_phy_get_eee_cap(a_uint32_t dev_id, a_uint32_t phy_id,
	a_uint32_t *cap)
{
	a_uint16_t phy_data = 0;

	phy_data = qca808x_phy_mmd_read(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_ADDR_8023AZ_EEE_2500M_CAPABILITY);

	if (phy_data & QCA8084_PHY_EEE_CAPABILITY_2500M) {
		*cap |= FAL_PHY_EEE_2500BASE_T;
	}

	return SW_OK;
}

sw_error_t
qca8084_phy_get_eee_status(a_uint32_t dev_id, a_uint32_t phy_id,
	a_uint32_t *status)
{
	a_uint32_t adv = 0, lp_adv = 0;
	sw_error_t rv = SW_OK;

	rv = qca8084_phy_get_eee_adv(dev_id, phy_id, &adv);
	SW_RTN_ON_ERROR(rv);

	rv = qca8084_phy_get_eee_partner_adv(dev_id, phy_id, &lp_adv);
	SW_RTN_ON_ERROR(rv);

	*status |= (adv & lp_adv);

	return SW_OK;
}

static sw_error_t
qca8084_phy_cdt_thresh_init(a_uint32_t dev_id, a_uint32_t phy_id)
{
	sw_error_t rv = SW_OK;

	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL3,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL3_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL4,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL4_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL5,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL5_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL6,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL6_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL7,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL7_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL9,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL9_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL13,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL13_VAL);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_mmd_write(dev_id, phy_id, QCA8084_PHY_MMD3_NUM,
		QCA8084_PHY_MMD3_CDT_THRESH_CTRL14,
		QCA8084_PHY_MMD3_NEAR_ECHO_THRESH_VAL);

	return rv;
}

static sw_error_t
qca8084_phy_function_reset(a_uint32_t dev_id, a_uint32_t phy_id,
	hsl_phy_function_reset_t phy_reset_type)
{
	sw_error_t rv = SW_OK;

	rv = qca8084_phy_fifo_reset(dev_id, phy_id, A_TRUE);
	SW_RTN_ON_ERROR(rv);

	aos_mdelay(50);

	rv = qca8084_phy_fifo_reset(dev_id, phy_id, A_FALSE);

	return rv;
}

static sw_error_t
_qca8084_phy_uqxgmii_speed_fixup(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_uint32_t link, fal_port_speed_t new_speed)
{
	sw_error_t rv = SW_OK;
	a_uint32_t mht_port_id = 0;
	a_bool_t port_clock_en = A_FALSE;

	rv = qca_mht_port_id_get(dev_id, phy_addr, &mht_port_id);
	SW_RTN_ON_ERROR(rv);

	/*Restart the auto-neg of uniphy*/
	SSDK_DEBUG("Restart the auto-neg of uniphy\n");
	rv = mht_uniphy_xpcs_autoneg_restart(dev_id, mht_port_id);
	SW_RTN_ON_ERROR(rv);
	/*set gmii+ clock to uniphy1 and ethphy*/
	SSDK_DEBUG("set gmii,xgmii clock to uniphy and gmii to ethphy\n");
	rv = mht_port_speed_clock_set(dev_id, mht_port_id, new_speed);
	SW_RTN_ON_ERROR(rv);
	/*set xpcs speed*/
	SSDK_DEBUG("set xpcs speed\n");
	rv = mht_uniphy_xpcs_speed_set(dev_id, mht_port_id, new_speed);
	SW_RTN_ON_ERROR(rv);

	/*GMII/XGMII clock and ETHPHY GMII clock enable/disable*/
	SSDK_DEBUG("GMII/XGMII clock and ETHPHY GMII clock enable/disable\n");
	if(link)
		port_clock_en = A_TRUE;
	rv = ssdk_mht_port_clk_en_set(dev_id, mht_port_id,
		MHT_CLK_TYPE_UNIPHY|MHT_CLK_TYPE_EPHY, port_clock_en);
	SW_RTN_ON_ERROR(rv);
	/*delay 100ms after enable/disable clock*/
	SSDK_DEBUG("delay 100ms after enable/disable clock\n");
	aos_mdelay(100);
	/*GMII/XGMII interface and ETHPHY GMII interface reset and release*/
	SSDK_DEBUG("UNIPHY GMII/XGMII interface and ETHPHY GMII interface reset and release\n");
	rv = ssdk_mht_port_clk_reset(dev_id, mht_port_id, MHT_CLK_TYPE_UNIPHY|MHT_CLK_TYPE_EPHY);
	SW_RTN_ON_ERROR(rv);
	/*ipg_tune and xgmii2gmii reset for uniphy and ETHPHY, function reset*/
	SSDK_DEBUG("ipg_tune and xgmii2gmii reset for uniphy and ETHPHY, function reset\n");
	rv = mht_uniphy_uqxgmii_function_reset(dev_id, mht_port_id);
	SW_RTN_ON_ERROR(rv);
	/*do ethphy function reset*/
	SSDK_DEBUG("do ethphy function reset\n");
	if(link)
	{
		rv = qca8084_phy_function_reset(dev_id, phy_addr, PHY_FIFO_RESET);
		SW_RTN_ON_ERROR(rv);
	}
	else
	{
		rv = qca8084_phy_fifo_reset(dev_id, phy_addr, A_TRUE);
		SW_RTN_ON_ERROR(rv);
	}
	/*change IPG from 10 to 11 for 1G speed*/
	rv = qca8084_phy_ipg_config(dev_id, phy_addr, new_speed);

	return rv;
}

static sw_error_t
_qca8084_phy_sgmii_speed_fixup (a_uint32_t dev_id, a_uint32_t phy_addr,
	a_uint32_t link, fal_port_speed_t new_speed)
{
	sw_error_t rv = SW_OK;

	/*disable ethphy3 and uniphy0 clock*/
	SSDK_DEBUG("disable ethphy3 and uniphy0 clock\n");
	rv = ssdk_mht_port_clk_en_set(dev_id, SSDK_PHYSICAL_PORT4,
		MHT_CLK_TYPE_EPHY, A_FALSE);
	SW_RTN_ON_ERROR(rv);
	rv = ssdk_mht_port_clk_en_set(dev_id, SSDK_PHYSICAL_PORT5,
		MHT_CLK_TYPE_UNIPHY, A_FALSE);
	SW_RTN_ON_ERROR(rv);
	/*set gmii clock for ethphy3 and uniphy0*/
	SSDK_DEBUG("set speed clock for eth3 and uniphy0\n");
	rv = mht_port_speed_clock_set(dev_id, SSDK_PHYSICAL_PORT4, new_speed);
	SW_RTN_ON_ERROR(rv);
	/*uniphy and ethphy gmii clock enable/disable*/
	SSDK_DEBUG("uniphy and ethphy GMII clock enable/disable\n");
	if(link)
	{
		SSDK_DEBUG("enable ethphy3 and uniphy0 clock\n");
		rv = ssdk_mht_port_clk_en_set(dev_id, SSDK_PHYSICAL_PORT4,
			MHT_CLK_TYPE_EPHY, A_TRUE);
		SW_RTN_ON_ERROR(rv);
		rv = ssdk_mht_port_clk_en_set(dev_id, SSDK_PHYSICAL_PORT5,
			MHT_CLK_TYPE_UNIPHY, A_TRUE);
		SW_RTN_ON_ERROR(rv);
	}
	/*uniphy and ethphy gmii reset and release*/
	SSDK_DEBUG("uniphy and ethphy GMII interface reset and release\n");
	rv = ssdk_mht_port_clk_reset(dev_id, SSDK_PHYSICAL_PORT4,
		MHT_CLK_TYPE_EPHY);
	SW_RTN_ON_ERROR(rv);
	rv = ssdk_mht_port_clk_reset(dev_id, SSDK_PHYSICAL_PORT5,
		MHT_CLK_TYPE_UNIPHY);
	SW_RTN_ON_ERROR(rv);
	/*uniphy and ethphy ipg_tune reset, function reset*/
	SSDK_DEBUG("uniphy and ethphy ipg_tune reset, function reset\n");
	rv = mht_uniphy_sgmii_function_reset(dev_id, MHT_UNIPHY_SGMII_0);
	SW_RTN_ON_ERROR(rv);
	/*do ethphy function reset*/
	SSDK_DEBUG("do ethphy function reset\n");
	rv = qca8084_phy_function_reset(dev_id, phy_addr, PHY_FIFO_RESET);

	return rv;
}

static sw_error_t
_qca8084_phy_speed_fixup (a_uint32_t dev_id, a_uint32_t phy_addr,
	a_uint32_t link, fal_port_interface_mode_t mode, fal_port_speed_t new_speed)
{
	sw_error_t rv = SW_OK;

	if(mode == PORT_SGMII_PLUS || mode == PHY_SGMII_BASET)
	{
		rv = _qca8084_phy_sgmii_speed_fixup(dev_id, phy_addr, link,
			new_speed);
		SW_RTN_ON_ERROR(rv);
	}
	else if(mode == PORT_UQXGMII)
	{
		rv = _qca8084_phy_uqxgmii_speed_fixup(dev_id, phy_addr, link,
			new_speed);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

sw_error_t
qca8084_phy_speed_fixup(a_uint32_t dev_id, a_uint32_t phy_addr,
	struct port_phy_status *phy_status)
{
	a_uint32_t port_id = 0;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	_qca8084_phy_interface_mode_fixup(dev_id, phy_addr, phy_status->speed);

	port_id = qca_ssdk_phy_addr_to_port(dev_id, phy_addr);
	if(phy_info->port_link_status[port_id] != phy_status->link_status)
	{
		SSDK_DEBUG("port %d link status is from %s to %s, speed is:%d\n",
			port_id,
			phy_info->port_link_status[port_id] ? "link up" :"link down",
			phy_status->link_status ? "link up" : "link down",
			phy_status->speed);
		qca8084_phy_icc_fix_up(dev_id, phy_addr, phy_status->speed);
		_qca8084_phy_speed_fixup(dev_id, phy_addr, phy_status->link_status,
			phy_info->port_mode[port_id], phy_status->speed);
		phy_info->port_link_status[port_id] = phy_status->link_status;
	}

	return SW_OK;
}

static sw_error_t
qca8084_phy_adc_edge_set(a_uint32_t dev_id, a_uint32_t phy_addr,
	qca8084_adc_edge_t adc_edge)
{
	sw_error_t rv = SW_OK;

	if(adc_edge != ADC_RISING && adc_edge != ADC_FALLING)
		return SW_NOT_SUPPORTED;

	rv = qca808x_phy_modify_debug(dev_id, phy_addr,
		QCA8084_PHY_DEBUG_ANA_INTERFACE_CLK_SEL, BITS(4,4), adc_edge);
	SW_RTN_ON_ERROR(rv);
	rv = qca808x_phy_reset(dev_id, phy_addr);

	return rv;
}

sw_error_t
qca8084_phy_hw_init(a_uint32_t dev_id, a_uint32_t phy_addr)
{
	sw_error_t rv = SW_OK;

	/*adjust CDT threshold*/
	rv = qca8084_phy_cdt_thresh_init(dev_id, phy_addr);
	SW_RTN_ON_ERROR(rv);
	/*invert ADC clock edge as falling edge to fix link issue*/
	rv = qca8084_phy_adc_edge_set(dev_id, phy_addr, ADC_FALLING);
	SW_RTN_ON_ERROR(rv);
	/*configure signal energy detect threshold to fix link issue
		for some chips*/
	rv = qca808x_phy_mmd_write(dev_id, phy_addr, QCA8084_PHY_MMD1_NUM,
		QCA8084_PHY_MMD1_MSE_THRESH_DEBUG_12,
		QCA8084_PHY_MMD1_MSE_THRESH_ENERGY_DETECT);
	SW_RTN_ON_ERROR(rv);
	/*init icc value*/
	rv = qca8084_phy_icc_init(dev_id, phy_addr);

	return rv;
}

sw_error_t
qca8084_phy_fixup_ability(a_uint32_t dev_id, a_uint32_t phy_addr,
	a_uint32_t *ability)
{
	a_uint32_t mht_port_id = 0;
	a_uint16_t phy_data = 0;
	a_bool_t half_support = A_TRUE;
	sw_error_t rv = SW_OK;

	/*In PHY mode, half duplex is not supported because the USXG doesn't support
		half duplex because of large latency*/
	if(mht_uniphy_mode_check(dev_id, MHT_UNIPHY_SGMII_1, MHT_UNIPHY_UQXGMII))
		half_support = A_FALSE;
	/*In bypass mode, half duplex is not supported for mht port 4 because of the
		new clock structure, PHY3 has extra two FIFOs which lead to large latency*/
	if(mht_uniphy_mode_check(dev_id, MHT_UNIPHY_SGMII_0, MHT_UNIPHY_PHY))
	{
		qca_mht_port_id_get(dev_id, phy_addr, &mht_port_id);
		if(mht_port_id == SSDK_PHYSICAL_PORT4)
			half_support = A_FALSE;
	}

	if(!half_support)
	{
		*ability &= ~(FAL_PHY_ADV_10T_HD | FAL_PHY_ADV_100TX_HD);
		phy_data = qca808x_phy_reg_read(dev_id, phy_addr, QCA808X_AUTONEG_ADVERT);
		if(phy_data & (QCA808X_ADVERTISE_10HALF | QCA808X_ADVERTISE_100HALF))
		{
			phy_data &= ~(QCA808X_ADVERTISE_10HALF | QCA808X_ADVERTISE_100HALF);
			rv = qca808x_phy_reg_write(dev_id, phy_addr, QCA808X_AUTONEG_ADVERT,
				phy_data);
			SW_RTN_ON_ERROR(rv);
		}
	}

	return SW_OK;
}
