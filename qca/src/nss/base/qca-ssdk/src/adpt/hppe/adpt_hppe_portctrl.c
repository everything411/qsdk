/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "hppe_xgportctrl_reg.h"
#include "hppe_xgportctrl.h"
#include "hppe_uniphy_reg.h"
#include "hppe_uniphy.h"
#include "hppe_global_reg.h"
#include "hppe_global.h"
#include "adpt.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "hsl_phy.h"
#include "hppe_init.h"
#include "ssdk_init.h"
#include "ssdk_dts.h"
#include "ssdk_clk.h"
#include "ssdk_hppe.h"
#include "adpt_hppe.h"
#include "adpt_hppe_uniphy.h"
#if defined(JHPPE)
#include "adpt_jhppe_loopback.h"
#include "adpt_jhppe_ptp.h"
#endif
#include "sfp_phy.h"
#include "adpt_appe_portctrl.h"
#include "ref_port_ctrl.h"
#if defined(HTTPPE)
#include "adpt_httppe_portctrl.h"
#include "adpt_httppe_portvlan.h"
#endif
#if defined(JHPPE)
#include "adpt_jhppe_uniphy.h"
#endif

#define PORT4_PCS_SEL_GMII_FROM_PCS0 1
#define PORT4_PCS_SEL_RGMII 0

#define PORT5_PCS_SEL_RGMII 0
#define PORT5_PCS_SEL_GMII_FROM_PCS0 1
#define PORT5_PCS_SEL_GMII_FROM_PCS1 2
#define PORT5_GMAC_SEL_GMAC 1
#define PORT5_GMAC_SEL_XGMAC 0

#define PORT6_PCS_SEL_RGMII 0
#define PORT6_PCS_SEL_GMII_FROM_PCS2 1
#define PORT6_GMAC_SEL_GMAC 1
#define PORT6_GMAC_SEL_XGMAC 0

#define MAC_SPEED_10M 0
#define MAC_SPEED_100M 1
#define MAC_SPEED_1000M 2
#define MAC_SPEED_10000M 3
#define MAC_SPEED_2500M 4
#define MAC_SPEED_5000M 5

#if defined(JHPPE)
#define XGMAC_SPEED_XGMII_10000M	0
#define XGMAC_SPEED_XGMII_25000M	1
#define XGMAC_SPEED_XGMII_5000M		5
#define XGMAC_SPEED_XGMII_2500M		6
#define XGMAC_SPEED_GMII_2500M		2
#define XGMAC_SPEED_GMII_1000M		3
#else
#define XGMAC_USXGMII_ENABLE 1
#define XGMAC_USXGMII_CLEAR 0

#define XGMAC_SPEED_SELECT_10000M 0
#define XGMAC_SPEED_SELECT_5000M 1
#define XGMAC_SPEED_SELECT_2500M 2
#define XGMAC_SPEED_SELECT_1000M 3
#endif

#define LPI_WAKEUP_TIMER	0x20
#define LPI_SLEEP_TIMER	0x100
#define PROMISCUOUS_MODE 0x1
#define PASS_CONTROL_PACKET 0x2
#define XGMAC_PAUSE_TIME	0xffff
#define CARRIER_SENSE_SIGNAL_FROM_MAC        0x0
#define XGMAC_PWE_ENABLE	0x1
#define XGMAC_WTO_LIMIT_13K	0xb
#define XGMAC_IPG_88BIT		0x1

#define PHY_PORT_TO_BM_PORT(port)	(PPE_BM_PHY_PORT_OFFSET + port - 1)
#define GMAC_IPG_CHECK          0xc
/* the wakeup timer unit is us */
#define PORT_LPI_WAKEUP_TIMER_10000M 8
#define PORT_LPI_WAKEUP_TIMER_5000M 16
#define PORT_LPI_WAKEUP_TIMER_2500M 32
#define PORT_LPI_WAKEUP_TIMER_1000M 18
#define PORT_LPI_WAKEUP_TIMER_100M 28
/* the sleep timer unit is us */
#define PORT_LPI_SLEEP_TIMER_10000M 10000
#define PORT_LPI_SLEEP_TIMER_1000M 20000
#define PORT_LPI_SLEEP_TIMER_DEFAULT 100

/* This register is used to adjust the write timing for reserving
 * some bandwidth of the memory to read operation.
 */
#define GMAC_TX_THD    0x1

#define PPE_PHY_STATUS_LINK_BIT		7
#define PPE_PHY_STATUS_25G_LINK_BIT	1

sw_error_t
adpt_hppe_port_interface_mode_get(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t * mode)
{
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	SW_RTN_ON_NULL(phy_info);
	SW_RTN_ON_NULL(mode);

	if (adpt_ppe_type_get(dev_id) != HTTPPE_TYPE) {
		if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_EXCL_CPU))
		{
			return SW_BAD_PARAM;
		}
	}
	*mode = phy_info->port_mode[port_id];

	return SW_OK;
}

static sw_error_t
_adpt_phy_status_get_from_ppe(a_uint32_t dev_id, a_uint32_t port_id,
		struct port_phy_status *phy_status)
{
	sw_error_t rv = SW_OK;
	a_uint32_t reg_field = 0, link = 0;
	fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;

	ADPT_DEV_ID_CHECK(dev_id);

	if (hsl_port_feature_get(dev_id, port_id, PHY_F_FORCE)) {
		struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

		/*
		 * For force-link ports, track phylink state so that bringing the
		 * interface down (ndo_stop) is reflected in PPE port status.
		 */
		if (priv && port_id < SW_MAX_NR_PORT &&
		    priv->ports[port_id].phylink &&
		    !READ_ONCE(priv->ports[port_id].port_phylink_up)) {
			phy_status->link_status = PORT_LINK_DOWN;
			phy_status->speed = FAL_SPEED_BUTT;
			phy_status->duplex = FAL_DUPLEX_BUTT;
			return SW_OK;
		}

		phy_status->link_status = PORT_LINK_UP;
		phy_status->speed = hsl_port_force_speed_get(dev_id, port_id);
		phy_status->duplex = hsl_port_force_duplex_get(dev_id, port_id);
		return SW_OK;
	} else if (hsl_port_is_sfp(dev_id, port_id)) {
#if defined(IN_SFP_PHY)
		a_bool_t rx_los_status = A_TRUE;

		rv = sfp_phy_rx_los_status_get(dev_id, port_id, &rx_los_status);
		if(rv == SW_OK && rx_los_status)
		{
			phy_status->link_status = PORT_LINK_DOWN;
			phy_status->speed = FAL_SPEED_BUTT;
			phy_status->duplex = FAL_DUPLEX_BUTT;
			return SW_OK;
		}
#endif
	}

	if (port_id == SSDK_PHYSICAL_PORT5)
	{
		rv = hppe_port_phy_status_1_port5_1_phy_status_get(dev_id,
				&reg_field);
		SW_RTN_ON_ERROR(rv);
	} else if ((port_id == SSDK_PHYSICAL_PORT1) ||
		((adpt_ppe_type_get(dev_id) == CPPE_TYPE) && (port_id == SSDK_PHYSICAL_PORT4)))
	{
		rv = hppe_port_phy_status_0_port1_phy_status_get(dev_id,
				&reg_field);
		SW_RTN_ON_ERROR (rv);
	} else if (port_id == SSDK_PHYSICAL_PORT2) {
		rv = hppe_port_phy_status_0_port2_phy_status_get(dev_id,
				&reg_field);
		SW_RTN_ON_ERROR (rv);
	} else if (port_id == SSDK_PHYSICAL_PORT3) {
		rv = hppe_port_phy_status_0_port3_phy_status_get(dev_id,
				&reg_field);
		SW_RTN_ON_ERROR (rv);
	} else if (port_id == SSDK_PHYSICAL_PORT6) {
		rv = hppe_port_phy_status_1_port6_phy_status_get(dev_id,
						&reg_field);
		SW_RTN_ON_ERROR (rv);
	} else {
		return SW_NOT_SUPPORTED;
	}

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR(rv);

	if (mode == PORT_25GBASE_R)
		link = (reg_field >> PPE_PHY_STATUS_25G_LINK_BIT) & 0x1;
	else
		link = (reg_field >> PPE_PHY_STATUS_LINK_BIT) & 0x1;
	if (link)
	{
		phy_status->link_status = PORT_LINK_UP;

		if (mode == PORT_25GBASE_R) {
			phy_status->speed = FAL_SPEED_25000;
		} else {
			switch (reg_field & 0x7)
			{
				case MAC_SPEED_10M:
					phy_status->speed = FAL_SPEED_10;
					break;
				case MAC_SPEED_100M:
					phy_status->speed = FAL_SPEED_100;
					break;
				case MAC_SPEED_1000M:
					phy_status->speed = FAL_SPEED_1000;
					break;
				case MAC_SPEED_10000M:
					phy_status->speed = FAL_SPEED_10000;
					break;
				case MAC_SPEED_2500M:
					phy_status->speed = FAL_SPEED_2500;
					break;
				case MAC_SPEED_5000M:
					phy_status->speed = FAL_SPEED_5000;
					break;
				default:
					phy_status->speed = FAL_SPEED_BUTT;
					break;
			}
		}
		phy_status->duplex = FAL_FULL_DUPLEX;
	}
	else
	{
		phy_status->link_status = PORT_LINK_DOWN;
		phy_status->speed = FAL_SPEED_BUTT;
		phy_status->duplex = FAL_DUPLEX_BUTT;
	}

	return rv;
}

static sw_error_t
_adpt_hppe_port_xgmac_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t * enable)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_rx_configuration_lm_get(dev_id, port_id, (a_uint32_t*)enable);

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
static sw_error_t
_adpt_hppe_port_gmac_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t * enable)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_ctrl2_mac_loop_back_get(dev_id, port_id, (a_uint32_t*)enable);

	return rv;
}
#endif
static sw_error_t
_adpt_hppe_port_xgmac_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t enable)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_rx_configuration_lm_set(dev_id, port_id, (a_uint32_t)enable);

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
static sw_error_t
_adpt_hppe_port_gmac_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t enable)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_ctrl2_mac_loop_back_set(dev_id, port_id, (a_uint32_t)enable);

	return rv;
}
#endif
static sw_error_t
_adpt_hppe_port_jumbo_size_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t jumbo_size)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_jumbo_size_mac_jumbo_size_set(dev_id, port_id, jumbo_size);

	return rv;
}

static sw_error_t
_adpt_xgmac_port_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t *max_frame)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_rx_configuration_gpsl_get(dev_id,port_id, max_frame);

	return rv;
}

static sw_error_t
_adpt_gmac_port_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t *max_frame)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_jumbo_size_mac_jumbo_size_get(dev_id, port_id, max_frame);

	return rv;
}

static sw_error_t
adpt_hppe_port_xgmac_promiscuous_mode_set(a_uint32_t dev_id,
	a_uint32_t port_id)
{
	sw_error_t rv = 0;

	port_id = ppe_port_to_xgmac_id(dev_id, port_id);

	rv = hppe_mac_packet_filter_pr_set(dev_id, port_id, PROMISCUOUS_MODE);

	SW_RTN_ON_ERROR (rv);

	rv = hppe_mac_packet_filter_pcf_set(dev_id, port_id, PASS_CONTROL_PACKET);

	SW_RTN_ON_ERROR (rv);

	rv = hppe_mac_packet_filter_ra_set (dev_id, port_id, (a_uint32_t)A_TRUE);

	return rv;
}

static sw_error_t
_adpt_xgmac_port_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t max_frame)
{
	sw_error_t rv = SW_OK;
	a_uint32_t index = ppe_port_to_xgmac_id(dev_id, port_id);
	fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR(rv);

	if (mode == PORT_25GBASE_R) {
		/* Decrease IPG to 88bits to work around 25G XGMAC backpressured by Uniphy issue */
		rv |= hppe_mac_tx_configuration_ipg_set(dev_id, index, XGMAC_IPG_88BIT);
	}
	rv |= hppe_mac_tx_configuration_jd_set(dev_id, index, (a_uint32_t)A_TRUE);
	rv |= hppe_mac_rx_configuration_gpsl_set(dev_id, index, max_frame);
	rv |= hppe_mac_rx_configuration_wd_set(dev_id, index, (a_uint32_t)A_FALSE);
	rv |= hppe_mac_rx_configuration_gmpslce_set(dev_id, index, (a_uint32_t)A_TRUE);
	rv |= hppe_mac_watchdog_timeout_wto_set(dev_id, index, XGMAC_WTO_LIMIT_13K);
	rv |= hppe_mac_watchdog_timeout_pwe_set(dev_id, index, XGMAC_PWE_ENABLE);
	rv |= adpt_hppe_port_xgmac_promiscuous_mode_set(dev_id, port_id);

	return rv;
}

static sw_error_t
_adpt_gmac_port_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t max_frame)
{
	sw_error_t rv = SW_OK;

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv |= hppe_mac_ctrl2_maxfr_set(dev_id, port_id, max_frame);
	rv |= hppe_mac_ctrl2_crs_sel_set(dev_id, port_id, CARRIER_SENSE_SIGNAL_FROM_MAC);
	rv |= hppe_mac_dbg_ctrl_high_ipg_set(dev_id, port_id, GMAC_IPG_CHECK);
	rv |= hppe_mac_ctrl2_mac_tx_thd_set(dev_id, port_id, GMAC_TX_THD);

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
static sw_error_t
_adpt_xgmac_port_rx_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t* port_rxmac_status)
{
	sw_error_t rv = SW_OK;
	union mac_rx_configuration_u xgmac_rx_enable;

	memset(&xgmac_rx_enable, 0, sizeof(xgmac_rx_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_rx_configuration_get(dev_id, port_id,  &xgmac_rx_enable);
	if( rv != SW_OK )
		return rv;
	*port_rxmac_status = xgmac_rx_enable.bf.re;

	return rv;
}
static sw_error_t
_adpt_gmac_port_rx_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t* port_rxmac_status)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_rx_enable;

	memset(&gmac_rx_enable, 0, sizeof(gmac_rx_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_enable_get(dev_id, port_id, &gmac_rx_enable);
	if( rv != SW_OK )
		return rv;
	* port_rxmac_status = gmac_rx_enable.bf.rxmac_en;

	return rv;
}
#endif
static sw_error_t
_adpt_xgmac_port_rx_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_rx_configuration_u xgmac_rx_enable;

	memset(&xgmac_rx_enable, 0, sizeof(xgmac_rx_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv |=  hppe_mac_rx_configuration_get(dev_id, port_id,  &xgmac_rx_enable);

	xgmac_rx_enable.bf.acs = 1;
	xgmac_rx_enable.bf.cst = 1;
	if (A_TRUE == enable)
	{
		xgmac_rx_enable.bf.re = 1;
	}
	else {
		xgmac_rx_enable.bf.re = 0;
	}
	rv |= hppe_mac_rx_configuration_set(dev_id, port_id, &xgmac_rx_enable);

	return rv;
}

static sw_error_t
_adpt_gmac_port_rx_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_rx_enable;

	memset(&gmac_rx_enable, 0, sizeof(gmac_rx_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv |= hppe_mac_enable_get(dev_id, port_id, &gmac_rx_enable);
	if (A_TRUE == enable)
		gmac_rx_enable.bf.rxmac_en = 1;
	if (A_FALSE == enable)
		gmac_rx_enable.bf.rxmac_en = 0;
	rv |= hppe_mac_enable_set(dev_id, port_id, &gmac_rx_enable);

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
static sw_error_t
_adpt_xgmac_port_tx_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *port_txmac_status)
{
	sw_error_t rv = SW_OK;
	union mac_tx_configuration_u xgmac_tx_enable;

	memset(&xgmac_tx_enable, 0, sizeof(xgmac_tx_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_tx_configuration_get(dev_id, port_id,  &xgmac_tx_enable);
	if( rv != SW_OK )
		return  rv;
	*port_txmac_status = xgmac_tx_enable.bf.te;

	return SW_OK;
}

static sw_error_t
_adpt_gmac_port_tx_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *port_txmac_status)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_tx_enable;

	memset(&gmac_tx_enable, 0, sizeof(gmac_tx_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_enable_get(dev_id, port_id, &gmac_tx_enable);
	if( rv != SW_OK )
		return  rv;
	*port_txmac_status = gmac_tx_enable.bf.txmac_en;

	return SW_OK;
}
#endif
static sw_error_t
_adpt_xgmac_port_tx_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_tx_configuration_u xgmac_tx_enable;

	memset(&xgmac_tx_enable, 0, sizeof(xgmac_tx_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	 rv |=hppe_mac_tx_configuration_get(dev_id, port_id,  &xgmac_tx_enable);
	 if (A_TRUE == enable)
		 xgmac_tx_enable.bf.te = 1;
	 if (A_FALSE == enable)
		 xgmac_tx_enable.bf.te = 0;
	 rv |= hppe_mac_tx_configuration_set(dev_id, port_id, &xgmac_tx_enable);

	 return SW_OK;
}

static sw_error_t
_adpt_gmac_port_tx_status_set (a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_tx_enable;

	memset(&gmac_tx_enable, 0, sizeof(gmac_tx_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv |= hppe_mac_enable_get(dev_id, port_id, &gmac_tx_enable);
	if (A_TRUE == enable)
		gmac_tx_enable.bf.txmac_en = 1;
	if (A_FALSE == enable)
		gmac_tx_enable.bf.txmac_en = 0;
	rv |= hppe_mac_enable_set(dev_id, port_id, &gmac_tx_enable);

	return SW_OK;
}

static sw_error_t
_adpt_xgmac_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t* txfc_status)
{
	sw_error_t rv = SW_OK;
	union mac_q0_tx_flow_ctrl_u xgmac_txfc_enable;

	memset(&xgmac_txfc_enable, 0, sizeof(xgmac_txfc_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_q0_tx_flow_ctrl_get(dev_id, port_id, &xgmac_txfc_enable);
	if( rv != SW_OK )
		return rv;
	*txfc_status = xgmac_txfc_enable.bf.tfe;

	return  SW_OK;
}

static sw_error_t
_adpt_gmac_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t* txfc_status)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_txfc_enable;

	memset(&gmac_txfc_enable, 0, sizeof(gmac_txfc_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_enable_get(dev_id, port_id, &gmac_txfc_enable);
	if( rv != SW_OK )
		return rv;
	*txfc_status = gmac_txfc_enable.bf.tx_flow_en;

	return  SW_OK;
}

static sw_error_t
_adpt_xgmac_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id,  a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_q0_tx_flow_ctrl_u xgmac_txfc_enable;

	memset(&xgmac_txfc_enable, 0, sizeof(xgmac_txfc_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv |= hppe_mac_q0_tx_flow_ctrl_get(dev_id, port_id,  &xgmac_txfc_enable);
	if (A_TRUE == enable)
	{
		xgmac_txfc_enable.bf.tfe = 1;
		xgmac_txfc_enable.bf.pt = XGMAC_PAUSE_TIME;
	}
	if (A_FALSE == enable)
		xgmac_txfc_enable.bf.tfe = 0;
	 rv |= hppe_mac_q0_tx_flow_ctrl_set(dev_id, port_id,  &xgmac_txfc_enable);

	 return SW_OK;
}

static sw_error_t
_adpt_gmac_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id,  a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_txfc_enable;

	memset(&gmac_txfc_enable, 0, sizeof(gmac_txfc_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv |= hppe_mac_enable_get(dev_id, port_id, &gmac_txfc_enable);
	if (A_TRUE == enable)
		gmac_txfc_enable.bf.tx_flow_en = 1;
	if (A_FALSE == enable)
		gmac_txfc_enable.bf.tx_flow_en = 0;
	rv |= hppe_mac_enable_set(dev_id, port_id, &gmac_txfc_enable);

	return SW_OK;
}

static sw_error_t
_adpt_xgmac_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id,  a_uint32_t* rxfc_status)
{
	sw_error_t rv = SW_OK;
	union mac_rx_flow_ctrl_u xgmac_rxfc_enable;

	memset(&xgmac_rxfc_enable, 0, sizeof(xgmac_rxfc_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_rx_flow_ctrl_get(dev_id, port_id, &xgmac_rxfc_enable);
	if(rv != SW_OK)
		return rv;
	*rxfc_status = xgmac_rxfc_enable.bf.rfe;

	return  SW_OK;
}

static sw_error_t
_adpt_gmac_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t* rxfc_status)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_rxfc_enable;

	memset(&gmac_rxfc_enable, 0, sizeof(gmac_rxfc_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = hppe_mac_enable_get(dev_id, port_id, &gmac_rxfc_enable);
	if( rv != SW_OK)
		return rv;
	*rxfc_status = gmac_rxfc_enable.bf.rx_flow_en;

	return SW_OK;
}

static sw_error_t
_adpt_xgmac_port_rxfc_status_set(a_uint32_t dev_id,fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_rx_flow_ctrl_u xgmac_rxfc_enable;

	memset(&xgmac_rxfc_enable, 0, sizeof(xgmac_rxfc_enable));
	port_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv |= hppe_mac_rx_flow_ctrl_get(dev_id, port_id, &xgmac_rxfc_enable);
	if (A_TRUE == enable)
		xgmac_rxfc_enable.bf.rfe= 1;
	if (A_FALSE == enable)
		xgmac_rxfc_enable.bf.rfe = 0;
	rv |= hppe_mac_rx_flow_ctrl_set(dev_id, port_id, &xgmac_rxfc_enable);

	return SW_OK;
}

static sw_error_t
_adpt_gmac_port_rxfc_status_set(a_uint32_t dev_id,fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u gmac_rxfc_enable;

	memset(&gmac_rxfc_enable, 0, sizeof(gmac_rxfc_enable));
	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv |= hppe_mac_enable_get(dev_id, port_id, &gmac_rxfc_enable);
	if (A_TRUE == enable)
		gmac_rxfc_enable.bf.rx_flow_en = 1;
	if (A_FALSE == enable)
		gmac_rxfc_enable.bf.rx_flow_en = 0;
	rv |= hppe_mac_enable_set(dev_id, port_id, &gmac_rxfc_enable);

	return SW_OK;
}

static sw_error_t
adpt_hppe_port_xgmac_reconfig(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;
	a_uint32_t rxfc_status = 0, txfc_status = 0;
	a_uint32_t index = ppe_port_to_xgmac_id(dev_id, port_id);

	rv = adpt_hppe_port_xgmac_promiscuous_mode_set(dev_id, port_id);
	SW_RTN_ON_ERROR(rv);

	rv = hppe_mac_watchdog_timeout_wto_set(dev_id, index, XGMAC_WTO_LIMIT_13K);
	SW_RTN_ON_ERROR(rv);
	rv = hppe_mac_watchdog_timeout_pwe_set(dev_id, index, XGMAC_PWE_ENABLE);
	SW_RTN_ON_ERROR(rv);

	rv = _adpt_xgmac_port_rxfc_status_get(dev_id, port_id, &rxfc_status);
	SW_RTN_ON_ERROR(rv);
	rv = _adpt_xgmac_port_rxfc_status_set(dev_id, port_id, rxfc_status);
	SW_RTN_ON_ERROR(rv);

	rv = _adpt_xgmac_port_txfc_status_get(dev_id, port_id, &txfc_status);
	SW_RTN_ON_ERROR(rv);
	rv = _adpt_xgmac_port_txfc_status_set(dev_id, port_id, txfc_status);

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
sw_error_t
adpt_hppe_port_rxmac_status_get(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_rxmac_status = 0, port_mac_type;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		_adpt_xgmac_port_rx_status_get( dev_id, port_id, &port_rxmac_status);
	else if (port_mac_type == PORT_GMAC_TYPE)
		 _adpt_gmac_port_rx_status_get( dev_id, port_id, &port_rxmac_status);
	else
		return SW_BAD_VALUE;

	if (port_rxmac_status)
		*enable = A_TRUE;
	else
		*enable = A_FALSE;

	return rv;
}
#endif

sw_error_t
adpt_hppe_port_txmac_status_set(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t enable)
{
	a_uint32_t port_mac_type;

	ADPT_DEV_ID_CHECK(dev_id);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		_adpt_xgmac_port_tx_status_set( dev_id, port_id, enable);
	else if (port_mac_type == PORT_GMAC_TYPE)
		_adpt_gmac_port_tx_status_set( dev_id, port_id, enable);
	else
		return SW_BAD_VALUE;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_rxmac_status_set(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t enable)
{
	a_uint32_t port_mac_type;

	ADPT_DEV_ID_CHECK(dev_id);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		_adpt_xgmac_port_rx_status_set(dev_id, port_id, enable);
	else if (port_mac_type == PORT_GMAC_TYPE)
		_adpt_gmac_port_rx_status_set(dev_id, port_id, enable);
	else
		return SW_BAD_VALUE;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_link_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t * status)
{
	sw_error_t rv = 0;
	struct port_phy_status phy_status = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(status);

	if (port_id == SSDK_PHYSICAL_PORT0 || port_id == SSDK_PHYSICAL_PORT7)
	{
		*status = A_TRUE;
		return SW_OK;
	}
	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	/* for those ports without PHY device should be sfp port */
	if (A_FALSE == hsl_port_phy_connected (dev_id, port_id))
	{
		rv = _adpt_phy_status_get_from_ppe(dev_id,
			port_id, &phy_status);
		SW_RTN_ON_ERROR (rv);
		*status = (a_bool_t) phy_status.link_status;
	}
	else
	{
		HSL_PORT_PHY_API_RUN(link_status_get, dev_id, port_id, status);
		SW_RTN_ON_ERROR (rv);
	}

	return SW_OK;

}

sw_error_t
adpt_hppe_port_mru_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mru_ctrl_t *ctrl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	hppe_mru_mtu_ctrl_tbl_mru_set(dev_id, port_id, ctrl->mru_size);
	hppe_mru_mtu_ctrl_tbl_mru_cmd_set(dev_id, port_id, (a_uint32_t)ctrl->action);

	return SW_OK;
}

sw_error_t
adpt_ppe_port_mru_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mru_ctrl_t *ctrl)
{
	sw_error_t rv = 0;
	a_uint32_t port_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);
	if(ctrl->mru_size > SSDK_MAX_FRAME_SIZE_16K)
		return SW_OUT_OF_RANGE;
	port_value = FAL_PORT_ID_VALUE(port_id);
	ADPT_PPE_PORT_ID_CHECK(port_value);
	rv = adpt_hppe_port_mru_set(dev_id, port_value, ctrl);

	return rv;
}

sw_error_t
adpt_hppe_port_mtu_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_ctrl_t *ctrl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	hppe_mru_mtu_ctrl_tbl_mtu_set(dev_id, port_id, ctrl->mtu_size);
	hppe_mru_mtu_ctrl_tbl_mtu_cmd_set(dev_id, port_id, (a_uint32_t)ctrl->action);

	if ((port_id >= SSDK_PHYSICAL_PORT0) && (port_id <= SSDK_PHYSICAL_PORT7))
	{
		hppe_mc_mtu_ctrl_tbl_mtu_set(dev_id, port_id, ctrl->mtu_size);
		hppe_mc_mtu_ctrl_tbl_mtu_cmd_set(dev_id, port_id, (a_uint32_t)ctrl->action);
	}

	return SW_OK;
}

sw_error_t
adpt_ppe_port_mtu_set(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_ctrl_t *ctrl)
{
	sw_error_t rv = 0;
	a_uint32_t port_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);
	if(ctrl->mtu_size > SSDK_MAX_FRAME_SIZE_16K)
		return SW_OUT_OF_RANGE;
	port_value = FAL_PORT_ID_VALUE(port_id);
	ADPT_PPE_PORT_ID_CHECK(port_value);
	rv = adpt_hppe_port_mtu_set(dev_id, port_value, ctrl);

	return rv;
}

sw_error_t
adpt_hppe_port_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t max_frame)
{
	a_uint32_t port_mac_type = 0;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		rv |= _adpt_xgmac_port_max_frame_size_set( dev_id, port_id, max_frame);
	else if (port_mac_type == PORT_GMAC_TYPE)
	{
		/*for gmac, rxtoolong have counters when package length is longer than jumbo size and shorter than max frame size,
		   when package length is longer than max frame size, the rxbadbyte have counters.*/
		rv |= _adpt_hppe_port_jumbo_size_set(dev_id, port_id, max_frame);
		rv |= _adpt_gmac_port_max_frame_size_set( dev_id, port_id, max_frame);
	}
	else
		return SW_BAD_VALUE;

	return rv;
}

sw_error_t
adpt_ppe_port_max_frame_size_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t max_frame)
{
	a_uint32_t size = SSDK_MAX_FRAME_SIZE_12K;

#if defined(HTTPPE)
	/* HTTPPE max framesize is 16K, others are 12K */
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
		size = SSDK_MAX_FRAME_SIZE_16K;
#endif

	if (max_frame > size)
		return SW_OUT_OF_RANGE;

#ifdef JHPPE
	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_TRUE)
	{
		return adpt_jhppe_lpbk_max_frame_size_set(dev_id, port_id, max_frame);
	}
#endif
	return adpt_hppe_port_max_frame_size_set(dev_id, port_id, max_frame);
}

sw_error_t
adpt_hppe_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t rxfc_status = 0, port_mac_type;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE) {
		_adpt_xgmac_port_rxfc_status_get( dev_id, port_id, &rxfc_status);
	} else if (port_mac_type == PORT_GMAC_TYPE) {
		_adpt_gmac_port_rxfc_status_get( dev_id, port_id, &rxfc_status);
	} else if (port_mac_type == PORT_PON_MAC_TYPE) {
		*enable = A_TRUE;
		return SW_OK;
	} else {
		return SW_BAD_VALUE;
	}

	if (rxfc_status)
		*enable = A_TRUE;
	else
		*enable = A_FALSE;

	return rv;
}

sw_error_t
adpt_hppe_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id,
				     a_bool_t * enable)
{
	a_uint32_t txfc_status = 0, port_mac_type;
	adpt_api_t *p_adpt_api;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE) {
		_adpt_xgmac_port_txfc_status_get( dev_id, port_id, &txfc_status);
	} else if (port_mac_type == PORT_GMAC_TYPE) {
		_adpt_gmac_port_txfc_status_get( dev_id, port_id, &txfc_status);
	} else if (port_mac_type == PORT_PON_MAC_TYPE) {
		p_adpt_api = adpt_api_ptr_get(dev_id);
		if (p_adpt_api && p_adpt_api->adpt_port_bm_ctrl_get) {
			int bm_id = PHY_PORT_TO_BM_PORT(port_id);

			if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE)
				bm_id = port_id;

			return p_adpt_api->adpt_port_bm_ctrl_get(dev_id, bm_id, enable);
		}
	} else {
		return SW_BAD_VALUE;
	}

	if (txfc_status)
		*enable = A_TRUE;
	else
		*enable = A_FALSE;

	return SW_OK;
}

#ifndef IN_PORTCONTROL_MINI
sw_error_t
adpt_hppe_port_txmac_status_get(a_uint32_t dev_id, fal_port_t port_id,
				      a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_txmac_status = 0, port_mac_type;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		_adpt_xgmac_port_tx_status_get( dev_id, port_id, &port_txmac_status);
	else if (port_mac_type == PORT_GMAC_TYPE)
		_adpt_gmac_port_tx_status_get( dev_id, port_id, &port_txmac_status);
	else
		return SW_BAD_VALUE;

	if (port_txmac_status)
		*enable = A_TRUE;
	else
		*enable = A_FALSE;

	return rv;
}

sw_error_t
adpt_hppe_ports_link_status_get(a_uint32_t dev_id, a_uint32_t * status)
{

	sw_error_t rv = 0;
	a_uint32_t port_id;
	hsl_dev_t *pdev = NULL;
	struct port_phy_status phy_status = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(status);

	pdev = hsl_dev_ptr_get(dev_id);
	if (pdev == NULL)
		return SW_NOT_INITIALIZED;

	*status = 0x0;
	for (port_id = 0; port_id < SW_MAX_NR_PORT; port_id++)
	{
		/* for those ports without PHY device should be sfp port */
		if (A_FALSE == hsl_port_phy_connected(dev_id, port_id))
		{
			if (hsl_port_prop_check(dev_id, port_id, HSL_PP_CPU) ||
				hsl_port_prop_check(dev_id, port_id, HSL_PP_INNER))
			{
				*status |= (0x1 << port_id);
			}
			else
			{
				if(!hsl_port_prop_check(dev_id, port_id, HSL_PP_PHY))
				{
					continue;
				}
				rv = _adpt_phy_status_get_from_ppe(dev_id,
					port_id, &phy_status);
				SW_RTN_ON_ERROR (rv);

				if (phy_status.link_status == PORT_LINK_UP)
				{
					*status |= (0x1 << port_id);
				}
				else
				{
					*status &= ~(0x1 << port_id);
				}
			}
		}
		else
		{
			a_bool_t link = A_FALSE;

			HSL_PORT_PHY_API_RUN(link_status_get, dev_id, port_id, &link);
			SW_RTN_ON_ERROR(rv);
			if (A_TRUE == link)
			{
				*status |= (0x1 << port_id);
			}
			else
			{
				*status &= ~(0x1 << port_id);
			}
		}
	}
	return SW_OK;

}

sw_error_t
adpt_hppe_port_mac_loopback_set(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		rv = _adpt_hppe_port_xgmac_loopback_set( dev_id, port_id, enable);
	else if(port_mac_type == PORT_GMAC_TYPE)
		rv =  _adpt_hppe_port_gmac_loopback_set( dev_id, port_id, enable);
	else
		return SW_BAD_VALUE;

	return rv;
}
#endif

sw_error_t
adpt_hppe_port_mru_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mru_ctrl_t *ctrl)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;

	memset(&mru_mtu_ctrl_tbl, 0, sizeof(mru_mtu_ctrl_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl_tbl);

	if( rv != SW_OK )
		return rv;

	ctrl->mru_size = mru_mtu_ctrl_tbl.bf.mru;
	ctrl->action = (fal_fwd_cmd_t)mru_mtu_ctrl_tbl.bf.mru_cmd;

	return SW_OK;
}

sw_error_t
adpt_ppe_port_mru_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mru_ctrl_t *ctrl)
{
	a_uint32_t port_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	port_value = FAL_PORT_ID_VALUE(port_id);
	ADPT_PPE_PORT_ID_CHECK(port_value);	
	return adpt_hppe_port_mru_get(dev_id, port_value, ctrl);
}

sw_error_t
adpt_hppe_port_duplex_get(a_uint32_t dev_id, fal_port_t port_id,
				fal_port_duplex_t * pduplex)
{
	sw_error_t rv = 0;
	struct port_phy_status phy_status = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pduplex);

	if (port_id == SSDK_PHYSICAL_PORT0 || port_id == SSDK_PHYSICAL_PORT7)
	{
		*pduplex = FAL_FULL_DUPLEX;
		return SW_OK;
	}

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	/* for those ports without PHY device should be sfp port */
	if (A_FALSE == hsl_port_phy_connected (dev_id, port_id))
	{

		rv = _adpt_phy_status_get_from_ppe(dev_id,
			port_id, &phy_status);
		SW_RTN_ON_ERROR (rv);
		*pduplex = phy_status.duplex;

	}
	else
	{
		HSL_PORT_PHY_API_RUN(duplex_get, dev_id, port_id, pduplex);
	}

	return rv;
}

sw_error_t
_adpt_hppe_port_combo_prefer_medium_get(a_uint32_t dev_id,
					     a_uint32_t port_id,
					     fal_port_medium_t *medium)
{
	phy_info_t * phy_info = hsl_phy_info_get(dev_id);
	SW_RTN_ON_NULL(phy_info);

	if (hsl_port_is_sfp(dev_id, port_id))
	{
		*medium = PHY_MEDIUM_FIBER;
	}
	else
	{
		*medium = PHY_MEDIUM_COPPER;
	}
	return SW_OK;
}

sw_error_t
adpt_hppe_port_combo_prefer_medium_get(a_uint32_t dev_id,
						     a_uint32_t port_id,
						     fal_port_medium_t *
						     medium)
{
	sw_error_t rv = SW_FAIL;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(medium);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_PHY))
	{
		return SW_BAD_PARAM;
	}

	if (A_FALSE == hsl_port_phy_connected (dev_id, port_id) &&
		A_FALSE == hsl_port_is_sfp(dev_id, port_id))
	{
		return SW_NOT_SUPPORTED;
	}
	HSL_PORT_PHY_API_RUN(combo_prefer_medium_get, dev_id, port_id,
		(void*)medium);
	if(rv != SW_OK)
	{
		rv = _adpt_hppe_port_combo_prefer_medium_get(dev_id, port_id, medium);
	}

	return rv;

}

sw_error_t
adpt_hppe_port_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t *max_frame)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(max_frame);

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
	{
		rv = _adpt_xgmac_port_max_frame_size_get( dev_id, port_id, max_frame);
	}
	else if (port_mac_type == PORT_GMAC_TYPE)
	{
		rv = _adpt_gmac_port_max_frame_size_get( dev_id, port_id, max_frame);
	}
	else
	{
		return SW_BAD_VALUE;
	}

	return rv;
}

sw_error_t
adpt_ppe_port_max_frame_size_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t *max_frame)
{
#ifdef JHPPE
	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_TRUE)
	{
		return adpt_jhppe_lpbk_max_frame_size_get(dev_id, port_id, max_frame);
	}
#endif
	return adpt_hppe_port_max_frame_size_get(dev_id, port_id, max_frame);

}

static sw_error_t
adpt_hppe_port_flowctrl_forcemode_set(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!priv)
		return SW_FAIL;

	if ((port_id < SSDK_PHYSICAL_PORT1) || (port_id > SSDK_PHYSICAL_PORT6))
		return SW_BAD_VALUE;
	if(!hsl_port_phy_connected(dev_id, port_id) && !enable)
	{
		return SW_NOT_SUPPORTED;
	}
	priv->ports[port_id].port_tx_flowctrl_forcemode = enable;
	priv->ports[port_id].port_rx_flowctrl_forcemode = enable;

	return rv;
}

static sw_error_t
adpt_hppe_port_flowctrl_forcemode_get(a_uint32_t dev_id,
		fal_port_t port_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);


	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if ((port_id < SSDK_PHYSICAL_PORT1) || (port_id > SSDK_PHYSICAL_PORT6))
		return SW_BAD_VALUE;

	if (!priv)
		return SW_FAIL;

	*enable = (priv->ports[port_id].port_tx_flowctrl_forcemode &
		priv->ports[port_id].port_rx_flowctrl_forcemode);

	return rv;
}

static sw_error_t
_adpt_hppe_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	adpt_api_t *p_adpt_api;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!priv)
		return SW_FAIL;

	if ((port_id < SSDK_PHYSICAL_PORT1) || (port_id > SSDK_PHYSICAL_PORT6))
		return SW_BAD_VALUE;

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		rv = _adpt_xgmac_port_txfc_status_set( dev_id, port_id, enable);
	else if (port_mac_type == PORT_GMAC_TYPE)
		rv = _adpt_gmac_port_txfc_status_set( dev_id, port_id, enable);
	else if (port_mac_type == PORT_PON_MAC_TYPE)
		/* pon mac port does not need to configure */
		rv = SW_OK;
	else
		return SW_BAD_VALUE;

	if (rv != SW_OK)
		return rv;

	priv->ports[port_id].port_old_tx_flowctrl = enable;

	/*keep bm status same with port*/
	p_adpt_api = adpt_api_ptr_get(dev_id);
	if (p_adpt_api && p_adpt_api->adpt_port_bm_ctrl_set) {
		int bm_id = PHY_PORT_TO_BM_PORT(port_id);

		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE)
			bm_id = port_id;

		rv = p_adpt_api->adpt_port_bm_ctrl_set(dev_id, bm_id, enable);
	}

	return rv;
}

sw_error_t
adpt_hppe_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_bool_t force_mode = A_FALSE;

	if(A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		rv = _adpt_hppe_port_txfc_status_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);

#if defined(IN_SFP_PHY)
		if(hsl_port_is_sfp(dev_id, port_id))
		{
			a_uint32_t phy_addr = 0;

			rv = hsl_port_prop_get_phyid(dev_id, port_id, &phy_addr);
			SW_RTN_ON_ERROR(rv);
			if(enable)
			{
				rv = sfp_phy_phydev_adv_update(dev_id, phy_addr,
					FAL_PHY_ADV_ASY_PAUSE, FAL_PHY_ADV_ASY_PAUSE);
				SW_RTN_ON_ERROR(rv);
			}
			else
			{
				rv = sfp_phy_phydev_adv_update(dev_id, phy_addr,
					FAL_PHY_ADV_ASY_PAUSE, 0);
				SW_RTN_ON_ERROR(rv);
			}
		}
#endif
	}
	else
	{
		/*if force mode is enabled, need to configure mac manually*/
		rv = adpt_hppe_port_flowctrl_forcemode_get(dev_id, port_id, &force_mode);
		SW_RTN_ON_ERROR (rv);
		if(force_mode)
		{
			rv = _adpt_hppe_port_txfc_status_set(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
		}
		rv = hsl_port_phy_txfc_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

static sw_error_t
_adpt_hppe_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (!priv)
		return SW_FAIL;

	if ((port_id < SSDK_PHYSICAL_PORT1) || (port_id > SSDK_PHYSICAL_PORT6))
		return SW_BAD_VALUE;

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if(port_mac_type == PORT_XGMAC_TYPE)
		rv = _adpt_xgmac_port_rxfc_status_set( dev_id, port_id, enable);
	else if (port_mac_type == PORT_GMAC_TYPE)
		rv = _adpt_gmac_port_rxfc_status_set( dev_id, port_id, enable);
	else if (port_mac_type == PORT_PON_MAC_TYPE)
		/* pon mac port does not need to configure */
		rv = SW_OK;
	else
		return SW_BAD_VALUE;

	if (rv != SW_OK)
		return rv;

	priv->ports[port_id].port_old_rx_flowctrl = enable;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_bool_t force_mode = A_FALSE;
	a_uint32_t phy_addr = 0;

	if(A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		rv = _adpt_hppe_port_rxfc_status_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
		if(hsl_port_is_sfp(dev_id, port_id))
		{
			rv = hsl_port_prop_get_phyid(dev_id, port_id, &phy_addr);
			SW_RTN_ON_ERROR(rv);
#if defined(IN_SFP_PHY)
			if(enable)
			{
				rv = sfp_phy_phydev_adv_update(dev_id, phy_addr, FAL_PHY_ADV_PAUSE |
					FAL_PHY_ADV_ASY_PAUSE, FAL_PHY_ADV_PAUSE |
					FAL_PHY_ADV_ASY_PAUSE);
				SW_RTN_ON_ERROR(rv);
			}
			else
			{
				rv = sfp_phy_phydev_adv_update(dev_id, phy_addr, FAL_PHY_ADV_PAUSE |
					FAL_PHY_ADV_ASY_PAUSE, 0);
				SW_RTN_ON_ERROR(rv);
			}
#endif
		}
	}
	else
	{
		/*if force mode is enabled, need to configure mac manually*/
		rv = adpt_hppe_port_flowctrl_forcemode_get(dev_id, port_id, &force_mode);
		SW_RTN_ON_ERROR(rv);
		if(force_mode)
		{
			rv = _adpt_hppe_port_rxfc_status_set(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
		}
		rv = hsl_port_phy_rxfc_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

sw_error_t
_adpt_hppe_port_combo_prefer_medium_set(a_uint32_t dev_id,
					     a_uint32_t port_id,
					     fal_port_medium_t medium)
{
	struct qca_phy_priv *priv = NULL;
	sw_error_t rv = SW_OK;

	if (A_TRUE != hsl_port_phy_combo_capability_get(dev_id, port_id))
	{
		return SW_BAD_PARAM;
	}

	if ((hsl_port_is_sfp(dev_id, port_id) && medium == PHY_MEDIUM_FIBER) ||
		(!hsl_port_is_sfp(dev_id, port_id) && medium == PHY_MEDIUM_COPPER))
	{
		return SW_OK;
	}

	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	mutex_lock(&priv->mac_sw_sync_lock);
	ssdk_mac_sw_sync_work_stop(dev_id);

	rv = hsl_port_combo_phy_driver_update(dev_id, port_id, medium);
	if (rv == SW_OK)
	{
		/*init port status to triger polling*/
		if(hsl_port_phy_connected(dev_id, port_id) == A_TRUE)
		{
			_adpt_hppe_port_txfc_status_set(dev_id, port_id, A_FALSE);
			_adpt_hppe_port_rxfc_status_set(dev_id, port_id, A_FALSE);
		}
		qca_mac_port_status_init(dev_id, port_id);
	}

	ssdk_mac_sw_sync_work_start(dev_id);
	mutex_unlock(&priv->mac_sw_sync_lock);
	return SW_OK;
}

sw_error_t
adpt_hppe_port_combo_prefer_medium_set(a_uint32_t dev_id,
					     a_uint32_t port_id,
					     fal_port_medium_t medium)
{
	sw_error_t rv = SW_FAIL;

	ADPT_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_PHY))
	{
		return SW_BAD_PARAM;
	}

	if (A_FALSE == hsl_port_phy_connected (dev_id, port_id) &&
		A_FALSE == hsl_port_is_sfp(dev_id, port_id))
	{
		return SW_NOT_SUPPORTED;
	}

	HSL_PORT_PHY_API_RUN(combo_prefer_medium_set, dev_id, port_id,
		(a_uint32_t)medium);
	if (rv != SW_OK)
	{
		rv = _adpt_hppe_port_combo_prefer_medium_set(dev_id, port_id, medium);
	}

	return rv;

}

sw_error_t
adpt_hppe_port_flowctrl_get(a_uint32_t dev_id, fal_port_t port_id,
				  a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	a_bool_t txfc_enable, rxfc_enable;

#ifdef JHPPE
	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_TRUE)
	{
		return adpt_jhppe_switch_port_loopback_flowctrl_get(dev_id,
			port_id, enable);
	}
#endif
	rv = adpt_hppe_port_txfc_status_get(dev_id, port_id,  &txfc_enable);
	rv |= adpt_hppe_port_rxfc_status_get(dev_id, port_id,  &rxfc_enable);
	if(rv != SW_OK)
		return rv;
	*enable = txfc_enable & rxfc_enable;

	return SW_OK;
}

static sw_error_t
_adpt_hppe_port_interface_mode_set(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t mode)
{
	sw_error_t rv = SW_OK;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	ADPT_DEV_ID_CHECK(dev_id);

	if (adpt_ppe_type_get(dev_id) != HTTPPE_TYPE) {
		if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_EXCL_CPU) &&
			mode != PORT_INTERFACE_MODE_MAX)
		{
			return SW_BAD_PARAM;
		}
	}
	phy_info->port_mode[port_id] = mode;

	return rv;
}

sw_error_t
adpt_hppe_port_interface_mode_set(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t mode)
{
	sw_error_t rv = SW_OK;

	ssdk_mac_sw_sync_work_stop(dev_id);

	if(mode != PORT_INTERFACE_MODE_AUTO) {
		hsl_port_feature_set(dev_id, port_id, PHY_F_FORCE_INTERFACE_MODE);
		if(hsl_port_is_sfp(dev_id, port_id)) {
			if(mode == PORT_SGMII_FIBER)
				hsl_port_feature_clear(dev_id, port_id, PHY_F_SFP_SGMII);
			else if(mode == PHY_SGMII_BASET)
				hsl_port_feature_set(dev_id, port_id, PHY_F_SFP_SGMII);
		}
		rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id, mode);
	} else {
		hsl_port_feature_clear(dev_id, port_id, PHY_F_FORCE_INTERFACE_MODE);
		qca_mac_port_status_init(dev_id, port_id);
	}

	return rv;
}

static sw_error_t
_adpt_hppe_gmac_speed_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_speed_t speed)
{
	sw_error_t rv = SW_OK;
	union mac_speed_u mac_speed;

	memset(&mac_speed, 0, sizeof(mac_speed));
	ADPT_DEV_ID_CHECK(dev_id);

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	hppe_mac_speed_get(dev_id, port_id, &mac_speed);

	if(FAL_SPEED_10 == speed)
		mac_speed.bf.mac_speed = MAC_SPEED_10M;
	else if(FAL_SPEED_100 == speed)
		mac_speed.bf.mac_speed = MAC_SPEED_100M;
	else if(FAL_SPEED_1000 == speed || FAL_SPEED_2500 == speed)
		mac_speed.bf.mac_speed = MAC_SPEED_1000M;

	rv = hppe_mac_speed_set(dev_id, port_id, &mac_speed);

	return rv;
}

#if defined(JHPPE)
static sw_error_t
_adpt_jhppe_xgmac_chicken_set(a_uint32_t dev_id, a_uint32_t mac_id, a_bool_t en)
{
	union mac_snps_scs_u mac_snps_scs = {0};
	sw_error_t rv;

	rv = hppe_mac_snps_scs_get(dev_id, mac_id, &mac_snps_scs);
	SW_RTN_ON_ERROR(rv);

	mac_snps_scs.bf.mtl_scs1 = en;

	return hppe_mac_snps_scs_set(dev_id, mac_id, &mac_snps_scs);
}

static sw_error_t
_adpt_jhppe_xgmac_speed_set(a_uint32_t dev_id, a_uint32_t mac_id, a_uint32_t mode, fal_port_speed_t speed)
{
	union mac_tx_configuration_u mac_tx_configuration = {0};
	a_uint32_t ss = 0;
	sw_error_t rv;

	rv = hppe_mac_tx_configuration_get(dev_id, mac_id, &mac_tx_configuration);
	SW_RTN_ON_ERROR(rv);

	switch (speed) {
	case FAL_SPEED_25000:
		ss = XGMAC_SPEED_XGMII_25000M;
		break;
	case FAL_SPEED_10000:
		ss = XGMAC_SPEED_XGMII_10000M;
		break;
	case FAL_SPEED_5000:
		ss = XGMAC_SPEED_XGMII_5000M;
		break;
	case FAL_SPEED_2500:
		if ((mode == PORT_USXGMII) || (mode == PORT_UQXGMII))
			ss = XGMAC_SPEED_XGMII_2500M;
		else
			ss = XGMAC_SPEED_GMII_2500M;
		break;
	case FAL_SPEED_1000:
	case FAL_SPEED_100:
	case FAL_SPEED_10:
		ss = XGMAC_SPEED_GMII_1000M;
		break;
	default:
		return SW_BAD_PARAM;
	}

	mac_tx_configuration.bf.ss = ss;

	return hppe_mac_tx_configuration_set(dev_id, mac_id, &mac_tx_configuration);
}
#endif

#if !defined(JHPPE)
static sw_error_t
_adpt_appe_xgmac_speed_set(a_uint32_t dev_id, a_uint32_t mac_id, a_uint32_t mode, fal_port_speed_t speed)
{
	union mac_tx_configuration_u mac_tx_configuration = {0};
	a_uint32_t uss = 0, ss = 0;
	sw_error_t rv;

	rv = hppe_mac_tx_configuration_get(dev_id, mac_id, &mac_tx_configuration);
	SW_RTN_ON_ERROR (rv);

	switch (speed) {
	case FAL_SPEED_10000:
		if ((mode == PORT_USXGMII) || (mode == PORT_UQXGMII))
			uss = XGMAC_USXGMII_ENABLE;
		else
			uss = XGMAC_USXGMII_CLEAR;
		ss = XGMAC_SPEED_SELECT_10000M;
		break;
	case FAL_SPEED_5000:
		uss = XGMAC_USXGMII_ENABLE;
		ss = XGMAC_SPEED_SELECT_5000M;
		break;
	case FAL_SPEED_2500:
		if ((mode == PORT_USXGMII) || (mode == PORT_UQXGMII))
			uss = XGMAC_USXGMII_ENABLE;
		else
			uss = XGMAC_USXGMII_CLEAR;
		ss = XGMAC_SPEED_SELECT_2500M;
		break;
	case FAL_SPEED_1000:
	case FAL_SPEED_100:
	case FAL_SPEED_10:
		uss = XGMAC_USXGMII_CLEAR;
		ss = XGMAC_SPEED_SELECT_1000M;
		break;
	default:
		return SW_BAD_PARAM;
	}
	mac_tx_configuration.bf.ss = ss;
	mac_tx_configuration.bf.uss = uss;

	return hppe_mac_tx_configuration_set(dev_id, mac_id, &mac_tx_configuration);
}
#endif

static sw_error_t
_adpt_hppe_xgmac_speed_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_speed_t speed)
{
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);
	sw_error_t rv = SW_OK;
	a_uint32_t mode = 0;

	ADPT_DEV_ID_CHECK(dev_id);

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR (rv);
	SSDK_DEBUG ("port %d interface mode is 0x%x\n", port_id, mode);

	port_id = ppe_port_to_xgmac_id(dev_id, port_id);

	switch (ppe_type) {
#if defined(HTTPPE)
	case HTTPPE_TYPE:
		rv = _adpt_httppe_xgmac_speed_set(dev_id, port_id, mode, speed);
		break;
#endif
	case JHPPE_TYPE:
#if defined(JHPPE)
		rv = _adpt_jhppe_xgmac_chicken_set(dev_id, port_id, A_TRUE);
		SW_RTN_ON_ERROR(rv);
#endif
		fallthrough;
	case HMSPPE_TYPE:
#if defined(JHPPE)
		rv = _adpt_jhppe_xgmac_speed_set(dev_id, port_id, mode, speed);
#endif
		break;
	default:
#if !defined(JHPPE)
		rv = _adpt_appe_xgmac_speed_set(dev_id, port_id, mode, speed);
#endif
		break;
	}

	return rv;
}

static sw_error_t
_adpt_hppe_gmac_duplex_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_duplex_t duplex)
{
	sw_error_t rv = SW_OK;
	union mac_enable_u mac_enable;

	memset(&mac_enable, 0, sizeof(mac_enable));
	ADPT_DEV_ID_CHECK(dev_id);

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	hppe_mac_enable_get(dev_id, port_id, &mac_enable);

	if (FAL_FULL_DUPLEX == duplex)
		mac_enable.bf.duplex = 1;
	else
		mac_enable.bf.duplex = 0;

	rv = hppe_mac_enable_set(dev_id, port_id, &mac_enable);

	return rv;
}

sw_error_t
adpt_hppe_port_mac_duplex_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_duplex_t duplex)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type;

	port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
	{
		return rv;
	}
	else if (port_mac_type == PORT_GMAC_TYPE)
	{
		rv = _adpt_hppe_gmac_duplex_set(dev_id, port_id, duplex);
	}
	else
	{
		return SW_BAD_VALUE;
	}

	return rv;
}

static sw_error_t
adpt_hppe_port_speed_change_port_clock_reset(a_uint32_t dev_id, a_uint32_t port_id)
{
	fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;
	a_uint32_t port_mac_type;
	sw_error_t rv = 0;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR(rv);
	if (mode == PORT_USXGMII || mode == PORT_UQXGMII) {
		SSDK_DEBUG("xgmac reset for port%d\n", port_id);
		ssdk_port_clock_reset(dev_id, port_id);
		/* Reset xgmac only when XGMAC is used in USXGMII/UQXGMII mode,
		 * to note that for UQXGMII on ipq5210, GMAC is used and XGMAC
		 * reset is not required.
		 */
		port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
		if (port_mac_type == PORT_XGMAC_TYPE) {
			/*restore xgmac's pr and pcf setting, re-config flowctrl after reset operation*/
			rv = adpt_hppe_port_xgmac_reconfig(dev_id, port_id);
			SW_RTN_ON_ERROR(rv);
		}
	}
	return rv;
}
static sw_error_t
adpt_hppe_port_interface_mode_switch_port_clock_reset(a_uint32_t dev_id,
	a_uint32_t port_id)
{
	sw_error_t rv = 0;
	a_uint32_t port_mac_type;
	fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR(rv);

	if ((mode == PORT_USXGMII) || (mode == PHY_SGMII_BASET) ||
		(mode == PORT_SGMII_FIBER) || (mode == PORT_10GBASE_R) ||
		(mode == PORT_SGMII_PLUS)) {
		ssdk_port_clock_reset(dev_id, port_id);
		port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
		if (port_mac_type == PORT_XGMAC_TYPE) {
			/*restore xgmac's pr and pcf setting, re-config flowctrl after reset
			operation*/
			rv = adpt_hppe_port_xgmac_reconfig(dev_id, port_id);
			SW_RTN_ON_ERROR(rv);
		}
	}
	return rv;
}

sw_error_t
adpt_hppe_port_mac_speed_set(a_uint32_t dev_id, a_uint32_t port_id,
				fal_port_speed_t speed)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type;

	port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
	{
		rv = _adpt_hppe_xgmac_speed_set(dev_id, port_id, speed);

	}
	else if (port_mac_type == PORT_GMAC_TYPE)
	{
		rv = _adpt_hppe_gmac_speed_set(dev_id, port_id, speed);
	}
	else
	{
		return SW_BAD_VALUE;
	}
	return rv;
}
static sw_error_t
_adpt_hppe_port_mux_set(a_uint32_t dev_id, fal_port_t port_id)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_type = 0;
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;

	port_type = qca_ppe_port_mac_type_get(dev_id, port_id);
	SW_RTN_ON_ERROR(adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode));

	if (port_type == PORT_GMAC_TYPE)
	{
		rv = _adpt_hppe_gmac_speed_set(dev_id, port_id, FAL_SPEED_1000);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_hppe_gmac_duplex_set(dev_id, port_id, FAL_FULL_DUPLEX);
		SW_RTN_ON_ERROR(rv);
	}
	else if (port_type == PORT_XGMAC_TYPE)
	{
		if ((port_mode == PORT_SGMII_PLUS) || (port_mode == PORT_UQXGMII))
		{
			rv = _adpt_hppe_xgmac_speed_set(dev_id, port_id, FAL_SPEED_2500);
			SW_RTN_ON_ERROR(rv);
		}
		else
		{
			rv = _adpt_hppe_xgmac_speed_set(dev_id, port_id, FAL_SPEED_10000);
			SW_RTN_ON_ERROR(rv);
		}
	}

	if (port_id >= SSDK_PHYSICAL_PORT1) {
		a_bool_t gmac_rxfc = A_FALSE, gmac_txfc = A_FALSE, xgmac_rxfc = A_FALSE,
			xgmac_txfc = A_FALSE;
		struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

		SW_RTN_ON_NULL(priv);
		if (port_type == PORT_GMAC_TYPE) {
			gmac_rxfc = priv->ports[port_id].port_old_rx_flowctrl;
			gmac_txfc = priv->ports[port_id].port_old_tx_flowctrl;
		} else if (port_type == PORT_XGMAC_TYPE) {
			xgmac_rxfc = priv->ports[port_id].port_old_rx_flowctrl;
			xgmac_txfc = priv->ports[port_id].port_old_tx_flowctrl;
		}

		rv = _adpt_gmac_port_txfc_status_set( dev_id, port_id, gmac_txfc);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_gmac_port_rxfc_status_set( dev_id, port_id, gmac_rxfc);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_xgmac_port_txfc_status_set( dev_id, port_id, xgmac_txfc);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_xgmac_port_rxfc_status_set( dev_id, port_id, xgmac_rxfc);
		SW_RTN_ON_ERROR(rv);
		rv = adpt_hppe_port_interface_mode_switch_port_clock_reset(dev_id, port_id);
	}
	if (adpt_chip_type_get(dev_id) == CHIP_APPE ||
		adpt_chip_type_get(dev_id) == CHIP_MRPPE||
		adpt_chip_type_get(dev_id) == CHIP_JHPPE ||
		adpt_chip_type_get(dev_id) == CHIP_HMSPPE) {
		rv = _adpt_appe_port_mux_mac_set(dev_id, port_id, port_type);
	}

	return rv;
}

sw_error_t
adpt_hppe_port_mux_mac_type_set(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t mode0, a_uint32_t mode1, a_uint32_t mode2)
{
	sw_error_t rv = SW_OK;
	a_uint32_t mode_tmp = PORT_WRAPPER_MAX;
	struct qca_phy_priv *priv = NULL;

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_PHY))
	{
		SSDK_DEBUG ("port %d need not to configure mux and mac type\n",
			port_id);
		return SW_OK;
	}
	/*init the port interface mode before set it according to three mac modes*/
	rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_INTERFACE_MODE_MAX);
	SW_RTN_ON_ERROR(rv);

	switch (mode0) {
		case PORT_WRAPPER_PSGMII_FIBER:
			if(port_id >= SSDK_PHYSICAL_PORT1  && port_id <= SSDK_PHYSICAL_PORT5)
			{
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
					_adpt_hppe_port_interface_mode_set(dev_id,
							port_id, PHY_PSGMII_BASET);
			}
			break;
		case PORT_WRAPPER_PSGMII:
			if((port_id >= SSDK_PHYSICAL_PORT1 && port_id <= SSDK_PHYSICAL_PORT4) ||
					(port_id == SSDK_PHYSICAL_PORT5 &&
					 mode1 == PORT_WRAPPER_MAX))
			{
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id, port_id, PHY_PSGMII_BASET);
			}
			break;
		case PORT_WRAPPER_QSGMII:
			if(port_id >= SSDK_PHYSICAL_PORT1 && port_id <= SSDK_PHYSICAL_PORT4)
			{
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_QSGMII);
			}
			break;
		case PORT_WRAPPER_SGMII0_RGMII4:
		case PORT_WRAPPER_SGMII_CHANNEL0:
		case PORT_WRAPPER_SGMII_FIBER:
		case PORT_WRAPPER_SGMII_PLUS:
			if(port_id == SSDK_PHYSICAL_PORT1)
			{
				if(mode0 == PORT_WRAPPER_SGMII_FIBER)
				{
					qca_ppe_port_mac_type_set(dev_id, SSDK_PHYSICAL_PORT1,
						PORT_GMAC_TYPE);
					_adpt_hppe_port_interface_mode_set(dev_id,
						SSDK_PHYSICAL_PORT1, PORT_SGMII_FIBER);
				}
				else if(mode0 == PORT_WRAPPER_SGMII_PLUS)
				{
					if (hsl_port_feature_get(dev_id, port_id, PHY_F_QGMAC)) {
						qca_ppe_port_mac_type_set(dev_id, port_id,
							PORT_GMAC_TYPE);
					} else {
						qca_ppe_port_mac_type_set(dev_id, port_id,
							PORT_XGMAC_TYPE);
					}
					_adpt_hppe_port_interface_mode_set(dev_id,
						SSDK_PHYSICAL_PORT1, PORT_SGMII_PLUS);
				}
				else
				{
					qca_ppe_port_mac_type_set(dev_id, SSDK_PHYSICAL_PORT1,
						PORT_GMAC_TYPE);
					_adpt_hppe_port_interface_mode_set(dev_id,
						SSDK_PHYSICAL_PORT1, PHY_SGMII_BASET);
				}
			}
			break;
		case PORT_WRAPPER_SGMII1_RGMII4:
		case PORT_WRAPPER_SGMII_CHANNEL1:
			if(port_id == SSDK_PHYSICAL_PORT2)
			{
				qca_ppe_port_mac_type_set(dev_id, SSDK_PHYSICAL_PORT2,
						PORT_GMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id,
						SSDK_PHYSICAL_PORT2, PHY_SGMII_BASET);
			}
			break;
		case PORT_WRAPPER_SGMII4_RGMII4:
		case PORT_WRAPPER_SGMII_CHANNEL4:
			if(port_id == SSDK_PHYSICAL_PORT5)
			{
				qca_ppe_port_mac_type_set(dev_id, SSDK_PHYSICAL_PORT5,
						PORT_GMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id,
						SSDK_PHYSICAL_PORT5, PHY_SGMII_BASET);
			}
			break;
		case PORT_WRAPPER_UQXGMII:
		case PORT_WRAPPER_UDXGMII:
			if(port_id >= SSDK_PHYSICAL_PORT1 && port_id <= SSDK_PHYSICAL_PORT4)
			{
				if (hsl_port_feature_get(dev_id, port_id, PHY_F_QGMAC))
					qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
				else
					qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_UQXGMII);
			}
			break;
		case PORT_WRAPPER_USXGMII:
			if(port_id == SSDK_PHYSICAL_PORT1)
			{
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_USXGMII);
			}
			break;
		case PORT_WRAPPER_10GBASE_R:
			if(port_id == SSDK_PHYSICAL_PORT1)
			{
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
				_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_10GBASE_R);
			}
			break;
		default:
			break;
	}
	if(SSDK_UNIPHY_INSTANCE1 == hsl_port_to_uniphy(dev_id, port_id))
		mode_tmp = mode1;
	else if (SSDK_UNIPHY_INSTANCE2 == hsl_port_to_uniphy(dev_id, port_id))
		mode_tmp = mode2;
	switch(mode_tmp)
	{
		case PORT_WRAPPER_SGMII_CHANNEL0:
		case PORT_WRAPPER_SGMII0_RGMII4:
		case PORT_WRAPPER_SGMII_FIBER:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
			if(mode_tmp == PORT_WRAPPER_SGMII_FIBER)
			{
				_adpt_hppe_port_interface_mode_set(dev_id, port_id,
					PORT_SGMII_FIBER);
			}
			else
			{
				_adpt_hppe_port_interface_mode_set(dev_id, port_id,
					PHY_SGMII_BASET);
			}
			break;
		case PORT_WRAPPER_SGMII_PLUS:
			if (hsl_port_feature_get(dev_id, port_id, PHY_F_QGMAC)) {
				qca_ppe_port_mac_type_set(dev_id, port_id,
						PORT_GMAC_TYPE);
			} else {
				qca_ppe_port_mac_type_set(dev_id, port_id,
						PORT_XGMAC_TYPE);
			}
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_SGMII_PLUS);
			break;
		case PORT_WRAPPER_USXGMII:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_USXGMII);
			break;
		case PORT_WRAPPER_10GBASE_R:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_10GBASE_R);
			break;
		case PORT_WRAPPER_25GBASE_R:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_25GBASE_R);
			break;
		case PORT_WRAPPER_GPON:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_PON_MAC_TYPE);
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_GPON);
			break;
		case PORT_WRAPPER_XGPON:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_PON_MAC_TYPE);
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_XGPON);
			break;
		case PORT_WRAPPER_XGSPON:
			qca_ppe_port_mac_type_set(dev_id, port_id, PORT_PON_MAC_TYPE);
			_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_XGSPON);
			break;
		default:
			break;
	}

	/* For internal PHY which is not controlled by UNIPHY MAC mode */
	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	if (priv->ports[port_id].interface == PHY_INTERFACE_MODE_INTERNAL) {
		qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
		_adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_INTERNAL);
	}

	rv = _adpt_hppe_port_mux_set(dev_id, port_id);

	return rv;
}

#if defined(HTTPPE)
sw_error_t
adpt_httppe_port_mux_mac_set(a_uint32_t dev_id, fal_port_t port_id)
{
	sw_error_t rv = SW_OK;
	union appe_port_mux_ctrl_u appe_port_mux_ctrl;

	ADPT_DEV_ID_CHECK(dev_id);
	memset(&appe_port_mux_ctrl, 0, sizeof(appe_port_mux_ctrl));

	if (port_id >= SSDK_PHYSICAL_PORT6)
		return SW_BAD_PARAM;

	rv = appe_port_mux_ctrl_get(dev_id, &appe_port_mux_ctrl);
	SW_RTN_ON_ERROR (rv);

	if (port_id == SSDK_PHYSICAL_PORT0) {
		if (qca_ppe_port_mac_type_get(dev_id, port_id) == PORT_XGMAC_TYPE)
			appe_port_mux_ctrl.bf.port1_mac_sel = 1;
		else
			appe_port_mux_ctrl.bf.port1_mac_sel = 0;
	}

	if (port_id == SSDK_PHYSICAL_PORT5) {
		if (qca_ppe_port_mac_type_get(dev_id, port_id) == PORT_XGMAC_TYPE)
			appe_port_mux_ctrl.bf.port5_mac_sel = 1;
		else
			appe_port_mux_ctrl.bf.port5_mac_sel = 0;
	}

	return appe_port_mux_ctrl_set(dev_id, &appe_port_mux_ctrl);
}

sw_error_t
adpt_httppe_port_mux_mac_type_set(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t mode0, a_uint32_t mode1, a_uint32_t mode2)
{
	sw_error_t rv = SW_OK;
	a_uint32_t mode_tmp = PORT_WRAPPER_MAX;

	rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_INTERFACE_MODE_MAX);
	SW_RTN_ON_ERROR(rv);

	if (port_id == SSDK_PHYSICAL_PORT0)
		mode_tmp = mode0;
	else if (port_id == SSDK_PHYSICAL_PORT5)
		mode_tmp = mode1;
	else {
		rv = qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_SGMII_PLUS);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_hppe_gmac_speed_set(dev_id, port_id, FAL_SPEED_1000);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_hppe_gmac_duplex_set(dev_id, port_id, FAL_FULL_DUPLEX);
		SW_RTN_ON_ERROR(rv);
		return SW_OK;
	}
	switch (mode_tmp) {
		case PORT_WRAPPER_SGMII_PLUS:
			rv = qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
			SW_RTN_ON_ERROR(rv);
			rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_SGMII_PLUS);
			SW_RTN_ON_ERROR(rv);
			rv = _adpt_hppe_gmac_speed_set(dev_id, port_id, FAL_SPEED_1000);
			SW_RTN_ON_ERROR(rv);
			rv = _adpt_hppe_gmac_duplex_set(dev_id, port_id, FAL_FULL_DUPLEX);
			SW_RTN_ON_ERROR(rv);
			break;
		case PORT_WRAPPER_USXGMII:
			rv = qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
			SW_RTN_ON_ERROR(rv);
			rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id, PORT_USXGMII);
			SW_RTN_ON_ERROR(rv);
			rv = _adpt_hppe_xgmac_speed_set(dev_id, port_id, FAL_SPEED_10000);
			SW_RTN_ON_ERROR(rv);
			break;
		default:
			break;
	}

	rv = adpt_httppe_port_mux_mac_set(dev_id, port_id);

	return rv;
}
#endif

static sw_error_t
_adpt_hppe_instance0_mode_get(a_uint32_t dev_id, a_uint32_t max_port_id,
	a_uint32_t *mode0)
{
	a_uint32_t port_id = 0;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	for(port_id = SSDK_PHYSICAL_PORT1; port_id <= max_port_id; port_id++)
	{
		if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_EXCL_CPU))
		{
			continue;
		}
		SSDK_DEBUG("port_id:%d, port_interface_mode:%d\n", port_id,
			phy_info->port_mode[port_id]);
		if(phy_info->port_mode[port_id] == PHY_PSGMII_BASET)
		{
			if(*mode0 != PORT_WRAPPER_MAX && *mode0 != PORT_WRAPPER_PSGMII)
			{
				SSDK_ERROR("when the port_interface_mode of port %d is %d, "
					"mode0:%d cannot be supported\n",
					port_id, phy_info->port_mode[port_id], *mode0);
				return SW_NOT_SUPPORTED;
			}
			*mode0 = PORT_WRAPPER_PSGMII;
		}
		if(phy_info->port_mode[port_id] == PORT_QSGMII)
		{
			if((*mode0 != PORT_WRAPPER_MAX && *mode0 != PORT_WRAPPER_QSGMII) ||
				port_id == SSDK_PHYSICAL_PORT5)
			{
				SSDK_ERROR("when the port_interface_mode of port %d is %d, "
					"mode0:%d cannot be supported\n",
					port_id, phy_info->port_mode[port_id], *mode0);
				return SW_NOT_SUPPORTED;
			}
			*mode0 = PORT_WRAPPER_QSGMII;
		}
		if(phy_info->port_mode[port_id] == PORT_UQXGMII)
		{
			if((*mode0 != PORT_WRAPPER_MAX && *mode0 != PORT_WRAPPER_UQXGMII) ||
				port_id == SSDK_PHYSICAL_PORT5)
			{
				SSDK_ERROR("when the port_interface_mode of port %d is %d, "
					"mode0:%d cannot be supported\n",
					port_id, phy_info->port_mode[port_id], *mode0);
				return SW_NOT_SUPPORTED;
			}
			*mode0 = PORT_WRAPPER_UQXGMII;
		}
		if(phy_info->port_mode[port_id] == PHY_SGMII_BASET ||
			phy_info->port_mode[port_id] == PORT_SGMII_FIBER)
		{
			if(*mode0 !=PORT_WRAPPER_MAX)
			{
				if(port_id != SSDK_PHYSICAL_PORT5)
				{
					SSDK_ERROR("when the port_interface_mode of port %d is %d, "
						"mode0:%d cannot be supported\n",
						port_id, phy_info->port_mode[port_id],
						*mode0);
					return SW_NOT_SUPPORTED;
				}
				else
				{
					return SW_OK;
				}
			}
			switch(port_id)
			{
				case SSDK_PHYSICAL_PORT1:
					if(phy_info->port_mode[port_id] == PORT_SGMII_FIBER)
					{
						*mode0 = PORT_WRAPPER_SGMII_FIBER;
					}
					else
					{
						*mode0 = PORT_WRAPPER_SGMII_CHANNEL0;
					}
					break;
				case SSDK_PHYSICAL_PORT2:
					*mode0 = PORT_WRAPPER_SGMII_CHANNEL1;
					break;
				case SSDK_PHYSICAL_PORT5:
					if(ssdk_dt_global_get_mac_mode(dev_id,
						SSDK_UNIPHY_INSTANCE1) == PORT_WRAPPER_MAX)
					{
						*mode0 = PORT_WRAPPER_SGMII_CHANNEL4;
					}
					break;
				default:
					SSDK_ERROR("port %d doesn't support "
						"port_interface_mode %d\n",
						port_id, phy_info->port_mode[port_id]);
					return SW_NOT_SUPPORTED;
			}
		}
		if(port_id != SSDK_PHYSICAL_PORT5 &&
				(phy_info->port_mode[port_id] == PORT_SGMII_PLUS ||
				 phy_info->port_mode[port_id] ==PORT_USXGMII ||
				 phy_info->port_mode[port_id] == PORT_10GBASE_R))
		{
			if(phy_info->port_mode[port_id] == PORT_SGMII_PLUS)
			{
				if(port_id == SSDK_PHYSICAL_PORT1)
				{
					*mode0 = PORT_WRAPPER_SGMII_PLUS;
					continue;
				}
			}
			else if (phy_info->port_mode[port_id] == PORT_USXGMII)
			{
				if(port_id == SSDK_PHYSICAL_PORT1)
				{
					*mode0 = PORT_WRAPPER_USXGMII;
					continue;
				}
			}
			else if (phy_info->port_mode[port_id] == PORT_10GBASE_R)
			{
				if(port_id == SSDK_PHYSICAL_PORT1)
				{
					*mode0 = PORT_WRAPPER_10GBASE_R;
					continue;
				}
			}
			SSDK_ERROR("port %d doesn't support port_interface_mode %d\n",
				port_id, phy_info->port_mode[port_id]);
			return SW_NOT_SUPPORTED;
		}
	}

	return SW_OK;
}

static sw_error_t
_adpt_hppe_instance1_mode_get(a_uint32_t dev_id, a_uint32_t port_id,  a_uint32_t *mode)
{
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);

	if ((A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_EXCL_CPU)) ||
		A_TRUE == hsl_port_prop_check (dev_id, port_id, HSL_PP_INNER))
	{
		return SW_OK;
	}
	SSDK_DEBUG("port_id:%x: %x\n", port_id, phy_info->port_mode[port_id]);
	switch(phy_info->port_mode[port_id])
	{
		case PHY_SGMII_BASET:
			*mode = PORT_WRAPPER_SGMII_CHANNEL0;
			break;
		case PORT_SGMII_PLUS:
			*mode = PORT_WRAPPER_SGMII_PLUS;
			break;
		case PORT_USXGMII:
			*mode = PORT_WRAPPER_USXGMII;
			break;
		case PORT_10GBASE_R:
			*mode = PORT_WRAPPER_10GBASE_R;
			break;
		case PORT_SGMII_FIBER:
			*mode = PORT_WRAPPER_SGMII_FIBER;
			break;
		case PHY_PSGMII_BASET:
			if(port_id == SSDK_PHYSICAL_PORT6)
			{
				SSDK_ERROR("port %d doesn't support port_interface_mode %d\n",
					port_id, phy_info->port_mode[port_id]);
				return SW_NOT_SUPPORTED;
			}
			*mode = PORT_WRAPPER_MAX;
			break;
		case PORT_25GBASE_R:
			*mode = PORT_WRAPPER_25GBASE_R;
			break;
		case PORT_GPON:
			*mode = PORT_WRAPPER_GPON;
			break;
		case PORT_XGPON:
			*mode = PORT_WRAPPER_XGPON;
			break;
		case PORT_XGSPON:
			*mode = PORT_WRAPPER_XGSPON;
			break;
		case PORT_INTERFACE_MODE_MAX:
			*mode = PORT_WRAPPER_MAX;
			break;
		default:
			SSDK_ERROR("port %d doesn't support port_interface_mode %d\n",
				port_id, phy_info->port_mode[port_id]);
			return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

static sw_error_t
_adpt_hppe_instance_mode_get(a_uint32_t dev_id, a_uint32_t uniphy_index,
	a_uint32_t *interface_mode)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_id = 0;

	port_id = adpt_hppe_port_get_by_uniphy(dev_id, uniphy_index,
		SSDK_UNIPHY_CHANNEL4);
	switch(uniphy_index)
	{
		case SSDK_UNIPHY_INSTANCE0:
			rv = _adpt_hppe_instance0_mode_get(dev_id, port_id, interface_mode);
			SW_RTN_ON_ERROR(rv);
			break;
		case SSDK_UNIPHY_INSTANCE1:
			rv =_adpt_hppe_instance1_mode_get(dev_id, port_id, interface_mode);
			SW_RTN_ON_ERROR(rv);
			break;
		case SSDK_UNIPHY_INSTANCE2:
			rv =_adpt_hppe_instance1_mode_get(dev_id, port_id, interface_mode);
			SW_RTN_ON_ERROR(rv);
			break;
		default:
			return SW_NOT_SUPPORTED;
	}

	return rv;
}

extern sw_error_t
adpt_hppe_uniphy_mode_set(a_uint32_t dev_id, a_uint32_t index, a_uint32_t mode);

static sw_error_t _adpt_hppe_port_mac_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;

	rv = adpt_hppe_port_txmac_status_set(dev_id, port_id, enable);
	SW_RTN_ON_ERROR(rv);
	rv = adpt_hppe_port_rxmac_status_set(dev_id, port_id, enable);

	return rv;
}

static sw_error_t
_adpt_hppe_port_phyaddr_update(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t port_mode)
{
	sw_error_t rv = SW_OK;
	a_uint32_t phy_addr = 0;

	if(port_mode == PORT_10GBASE_R) {
		/*SFP port no need to update phy address*/
		hsl_port_feature_set(dev_id, port_id, PHY_F_SFP | PHY_F_I2C);
	} else if (port_mode == PHY_PSGMII_BASET) {
		rv = hsl_port_prop_get_phyid (dev_id, port_id - 1, &phy_addr);
		SW_RTN_ON_ERROR (rv);
		phy_addr++;
		qca_ssdk_phy_address_set(dev_id, port_id, phy_addr);
		hsl_port_feature_clear(dev_id, port_id, PHY_F_SFP | PHY_F_I2C);
		SSDK_DEBUG("port %x phy_addr is %x\n", port_id, phy_addr);
	}

	return rv;
}

static sw_error_t
_adpt_hppe_sfp_copper_phydriver_switch(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t port_mode)
{
	sw_error_t rv = SW_OK;

	if (A_TRUE == hsl_port_phy_combo_capability_get(dev_id, port_id))
		return rv;

	rv = _adpt_hppe_port_phyaddr_update(dev_id, port_id, port_mode);
	SW_RTN_ON_ERROR(rv);
	if (port_mode == PORT_10GBASE_R || port_mode == PHY_PSGMII_BASET) {
		rv = hsl_port_phydrv_update(dev_id, port_id);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

static sw_error_t
adpt_hppe_port_mac_uniphy_phy_config(a_uint32_t dev_id, a_uint32_t mode_index,
	a_uint32_t mode[], a_bool_t force_switch)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_id = 0, port_id_from = 0, port_id_end = 0, port_mode = 0;

	if(mode_index == SSDK_UNIPHY_INSTANCE0)
	{
		switch (mode[SSDK_UNIPHY_INSTANCE0])
		{
			case PORT_WRAPPER_PSGMII:
			case PORT_WRAPPER_PSGMII_FIBER:
				port_id_from = SSDK_PHYSICAL_PORT1;
				/*qsgmii+10gbase-r+usxgmii --> psgmii+10gbase-r+usxgmii*/
				if(mode[SSDK_UNIPHY_INSTANCE1] != PORT_WRAPPER_MAX)
				{
					port_id_end = SSDK_PHYSICAL_PORT4;
				}
				else
				{
					port_id_end = SSDK_PHYSICAL_PORT5;
				}
				break;
			case PORT_WRAPPER_QSGMII:
			case PORT_WRAPPER_UQXGMII:
			case PORT_WRAPPER_UDXGMII:
				port_id_from = SSDK_PHYSICAL_PORT1;
				port_id_end = SSDK_PHYSICAL_PORT4;
				break;
			case PORT_WRAPPER_SGMII0_RGMII4:
			case PORT_WRAPPER_SGMII_CHANNEL0:
			case PORT_WRAPPER_SGMII_FIBER:
			case PORT_WRAPPER_SGMII_PLUS:
			case PORT_WRAPPER_USXGMII:
			case PORT_WRAPPER_10GBASE_R:
				port_id_from = SSDK_PHYSICAL_PORT1;
				port_id_end = SSDK_PHYSICAL_PORT1;
				break;
			case PORT_WRAPPER_SGMII1_RGMII4:
			case PORT_WRAPPER_SGMII_CHANNEL1:
				port_id_from = SSDK_PHYSICAL_PORT2;
				port_id_end = SSDK_PHYSICAL_PORT2;
				break;
			case PORT_WRAPPER_SGMII4_RGMII4:
			case PORT_WRAPPER_SGMII_CHANNEL4:
				port_id_from = SSDK_PHYSICAL_PORT5;
				port_id_end = SSDK_PHYSICAL_PORT5;
				break;
			default:
				SSDK_INFO ("uniphy %d interface mode is 0x%x\n",
					mode_index, mode[SSDK_UNIPHY_INSTANCE0]);
				return SW_OK;
		}
	}
	else if(mode_index == SSDK_UNIPHY_INSTANCE1 ||
		mode_index == SSDK_UNIPHY_INSTANCE2)
	{
		port_id_from = adpt_hppe_port_get_by_uniphy(dev_id, mode_index,
			SSDK_UNIPHY_CHANNEL0);
		port_id_end = port_id_from;
	}
	else
	{
		return SW_NOT_SUPPORTED;
	}
	/*disable mac tx and rx for special ports*/
	for(port_id = port_id_from; port_id <= port_id_end; port_id++)
	{
		rv = _adpt_hppe_port_mac_set(dev_id, port_id, A_FALSE);
	}

	/*configure the uniphy*/
	rv = adpt_hppe_uniphy_mode_set(dev_id, mode_index, mode[mode_index]);
	SSDK_DEBUG("configure uniphy mode_index:%x, mode:%x, rv:%x\n",
		mode_index, mode[mode_index], rv);
	SW_RTN_ON_ERROR(rv);

	for(port_id = port_id_from; port_id <= port_id_end; port_id++)
	{
		rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);
		SW_RTN_ON_ERROR(rv);
		if (!(force_switch && (port_mode == PORT_INTERFACE_MODE_MAX))) {
			/* configure mac mux */
			rv = adpt_hppe_port_mux_mac_type_set(dev_id, port_id,
					mode[SSDK_UNIPHY_INSTANCE0],
					mode[SSDK_UNIPHY_INSTANCE1], mode[SSDK_UNIPHY_INSTANCE2]);
			SSDK_DEBUG("configure mac, port_id %d, mode0 0x%x, mode1 0x%x, "
					"mode2 0x%x, rv 0x%x\n", port_id,
					mode[SSDK_UNIPHY_INSTANCE0], mode[SSDK_UNIPHY_INSTANCE1],
					mode[SSDK_UNIPHY_INSTANCE2], rv);
			SW_RTN_ON_ERROR(rv);
		}
		/* force switch phy driver and set phy mode */
		if (force_switch) {
			if (port_mode != PORT_INTERFACE_MODE_MAX) {
				/* port5 psgmii copper and 10gbase-r sfp phy driver switch */
				if (port_id == SSDK_PHYSICAL_PORT5) {
					rv = _adpt_hppe_sfp_copper_phydriver_switch(dev_id,
							port_id, port_mode);
					SW_RTN_ON_ERROR(rv);
				}
				if (port_mode != PORT_GPON && port_mode != PORT_XGPON && port_mode != PORT_XGSPON) {
					/* set phy mode */
					HSL_PORT_PHY_EXT_NSS_WITH_AQR_API_RUN(interface_set, dev_id, port_id, port_mode);
					SSDK_DEBUG("port_id:%d is configured as port_mode:0x%x\n",
						port_id, port_mode);
				}
			}
		}
		/* init port status to trigger polling */
		qca_mac_port_status_init(dev_id, port_id);
	}

	return rv;
}

sw_error_t
_adpt_hppe_port_interface_mode_apply(a_uint32_t dev_id, a_bool_t force_switch)
{
	sw_error_t rv = SW_OK;
	a_uint32_t mode_index = 0, mode_old[3] = {0}, uniphy_num = 0;
	a_uint32_t mode_new[3] = {PORT_WRAPPER_MAX,PORT_WRAPPER_MAX,PORT_WRAPPER_MAX};

	uniphy_num = adpt_ppe_uniphy_number_get(dev_id);

	/*get three intances mode, include old mode and new mode*/
	for(mode_index = SSDK_UNIPHY_INSTANCE0; mode_index < uniphy_num; mode_index++)
	{
		mode_old[mode_index] = ssdk_dt_global_get_mac_mode(dev_id, mode_index);
		if(mode_index == SSDK_UNIPHY_INSTANCE1 &&
			mode_new[SSDK_UNIPHY_INSTANCE0] == PORT_WRAPPER_SGMII_CHANNEL4)
		{
			mode_new[SSDK_UNIPHY_INSTANCE1] = PORT_WRAPPER_MAX;
		}
		else
		{
			rv = _adpt_hppe_instance_mode_get(dev_id, mode_index, &mode_new[mode_index]);
			SW_RTN_ON_ERROR(rv);
		}
	}
	SSDK_DEBUG("mode0_old: %x, mode1_old:%x, mode2_old:%x\n",
			mode_old[0], mode_old[1], mode_old[2]);
	SSDK_DEBUG("mode0_new: %x, mode1_new:%x, mode2_new:%x\n",
			mode_new[0], mode_new[1], mode_new[2]);
	/*set three new intances mode*/
	for(mode_index = SSDK_UNIPHY_INSTANCE0; mode_index < uniphy_num; mode_index++)
	{
		ssdk_dt_global_set_mac_mode(dev_id, mode_index, mode_new[mode_index]);
		ssdk_gcc_uniphy_sys_set(dev_id, mode_index, A_TRUE);
		/* wait for uniphy sys reset to settle after deassert */
		msleep(1);
	}
	ssdk_uniphy_port5_clock_source_set(dev_id);

	/*configure the mode according to mode_new*/
	for(mode_index = SSDK_UNIPHY_INSTANCE0; mode_index < uniphy_num; mode_index++)
	{
		if(mode_new[mode_index] != mode_old[mode_index])
		{
			SSDK_DEBUG("need to configure instance%x\n", mode_index);
			rv = adpt_hppe_port_mac_uniphy_phy_config(dev_id,
				mode_index, mode_new, force_switch);
			if(rv)
			{
				SSDK_ERROR("config instance%x, rv:%x faild\n", mode_index,rv);
				return rv;
			}
		}
		if (mode_new[mode_index] == PORT_WRAPPER_MAX) {
			ssdk_gcc_uniphy_sys_set(dev_id, mode_index, A_FALSE);
		}
	}

	return rv;
}

sw_error_t
adpt_hppe_port_interface_mode_apply(a_uint32_t dev_id)
{
	struct qca_phy_priv *priv;
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	priv = ssdk_phy_priv_data_get(dev_id);
	if (!priv)
	{
		return SW_FAIL;
	}

	mutex_lock(&priv->mac_sw_sync_lock);
	rv = _adpt_hppe_port_interface_mode_apply(dev_id, A_TRUE);
	mutex_unlock(&priv->mac_sw_sync_lock);

	ssdk_mac_sw_sync_work_start(dev_id);

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
sw_error_t
adpt_hppe_port_mac_loopback_get(a_uint32_t dev_id, fal_port_t port_id,
				 a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		rv = _adpt_hppe_port_xgmac_loopback_get( dev_id, port_id, enable);
	else if (port_mac_type == PORT_GMAC_TYPE)
		rv = _adpt_hppe_port_gmac_loopback_get( dev_id, port_id, enable);
	else
		return SW_BAD_VALUE;

	return rv;
}
#endif

sw_error_t
adpt_hppe_port_mtu_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_ctrl_t *ctrl)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;

	memset(&mru_mtu_ctrl_tbl, 0, sizeof(mru_mtu_ctrl_tbl));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_id, &mru_mtu_ctrl_tbl);
	if( rv != SW_OK )
		return rv;
	ctrl->mtu_size = mru_mtu_ctrl_tbl.bf.mtu;
	ctrl->action = (fal_fwd_cmd_t)mru_mtu_ctrl_tbl.bf.mtu_cmd;

	return SW_OK;
}

sw_error_t
adpt_ppe_port_mtu_get(a_uint32_t dev_id, fal_port_t port_id,
		fal_mtu_ctrl_t *ctrl)
{
	a_uint32_t port_value = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(ctrl);

	port_value = FAL_PORT_ID_VALUE(port_id);
	ADPT_PPE_PORT_ID_CHECK(port_value);

	return adpt_hppe_port_mtu_get(dev_id, port_value, ctrl);
}

sw_error_t
adpt_ppe_port_mru_mtu_set(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t mru_size, a_uint32_t mtu_size)
{
	sw_error_t rv = SW_OK;
	fal_mru_ctrl_t mru = {0};
	fal_mtu_ctrl_t mtu = {0};

	rv = adpt_ppe_port_mru_get (dev_id, port_id,  &mru);
	SW_RTN_ON_ERROR (rv);
	mru.mru_size = mru_size;
	rv = adpt_ppe_port_mru_set (dev_id, port_id,  &mru);
	SW_RTN_ON_ERROR (rv);

	rv = adpt_ppe_port_mtu_get (dev_id, port_id,  &mtu);
	SW_RTN_ON_ERROR (rv);
	mtu.mtu_size = mtu_size;
	rv = adpt_ppe_port_mtu_set (dev_id, port_id,  &mtu);

	return rv;
}

sw_error_t
adpt_ppe_port_mru_mtu_get(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t *mru_size, a_uint32_t *mtu_size)
{
	sw_error_t rv = SW_OK;
	fal_mru_ctrl_t mru = {0};
	fal_mtu_ctrl_t mtu = {0};

	rv = adpt_ppe_port_mru_get (dev_id, port_id,  &mru);
	SW_RTN_ON_ERROR (rv);
	*mru_size = mru.mru_size;

	rv = adpt_ppe_port_mtu_get (dev_id, port_id,  &mtu);
	SW_RTN_ON_ERROR (rv);
	*mtu_size = mtu.mtu_size;

	return rv;
}

#ifdef CONFIG_PHYLINK
static a_bool_t
adpt_hppe_port_phylink_is_enabled(a_uint32_t dev_id, fal_port_t port_id)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	if (!priv || port_id >= SW_MAX_NR_PORT)
		return A_FALSE;

	return priv->ports[port_id].phylink ? A_TRUE : A_FALSE;
}
#if defined(IN_SFP_PHY)
static sw_error_t
adpt_hppe_sfp_port_phylink_interface_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_interface_mode_t *mode)
{
	phy_interface_t interface;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);
	SW_RTN_ON_NULL(mode);

	if (port_id >= SW_MAX_NR_PORT)
		return SW_BAD_PARAM;

	interface = priv->ports[port_id].phylink_link_state.interface;

	/* Only update mode if there is a valid match, otherwise leave input parameter unchanged */
	if (interface == PHY_INTERFACE_MODE_25GBASER)
		*mode = PORT_25GBASE_R;

	SSDK_DEBUG("port id:%d, phylink_link_state interface:%d, port mode:%d\n",
		port_id, interface, *mode);

	return SW_OK;
}
#endif
#endif

sw_error_t
adpt_hppe_port_interface_mode_status_get(a_uint32_t dev_id, fal_port_t port_id,
			      fal_port_interface_mode_t * mode)
{

	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mode);

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_PHY))
	{
		return SW_BAD_PARAM;
	}
	if (A_TRUE == hsl_port_feature_get(dev_id, port_id, PHY_F_FORCE) ||
		hsl_port_feature_get(dev_id, port_id, PHY_F_FORCE_INTERFACE_MODE))
	{
		return adpt_hppe_port_interface_mode_get(dev_id, port_id, mode);
	}

	if (A_TRUE == hsl_port_is_sfp(dev_id, port_id)) {
#if defined(IN_SFP_PHY)
#ifdef CONFIG_PHYLINK
		if (adpt_hppe_port_phylink_is_enabled(dev_id, port_id))
			rv = adpt_hppe_sfp_port_phylink_interface_get(dev_id, port_id, mode);
		else
#endif
			rv = sfp_phy_interface_get_mode_status(dev_id, port_id, mode);
#endif
		SW_RTN_ON_ERROR (rv);
	} else {
		rv = hsl_port_phydev_interface_mode_status_get(dev_id, port_id, mode);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_hppe_port_flowctrl_set(a_uint32_t dev_id, fal_port_t port_id,
				  a_bool_t enable)
{
	sw_error_t rv = SW_OK;

#ifdef JHPPE
	if (adpt_ppe_loopback_port_validate(dev_id, port_id) == A_TRUE)
	{
		return adpt_jhppe_switch_port_loopback_flowctrl_set(dev_id,
			port_id, enable);
	}
#endif
	rv = adpt_hppe_port_txfc_status_set(dev_id, port_id, enable);
	rv |= adpt_hppe_port_rxfc_status_set(dev_id, port_id, enable);

	return rv;
}
sw_error_t
adpt_hppe_port_speed_get(a_uint32_t dev_id, fal_port_t port_id,
			       fal_port_speed_t * pspeed)
{
	sw_error_t rv = 0;
	struct port_phy_status phy_status = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pspeed);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	/* for those ports without PHY device should be sfp port */
	if (A_FALSE == hsl_port_phy_connected (dev_id, port_id))
	{
		if (port_id == SSDK_PHYSICAL_PORT0)
			*pspeed = FAL_SPEED_10000;
		else {
			rv = _adpt_phy_status_get_from_ppe(dev_id,
				port_id, &phy_status);
			SW_RTN_ON_ERROR (rv);
			*pspeed= phy_status.speed;
		}
	}
	else
	{
		HSL_PORT_PHY_API_RUN(speed_get, dev_id, port_id, pspeed);
		SW_RTN_ON_ERROR (rv);
	}

	return rv;
}

static sw_error_t
_adpt_ppe_port_source_filter_config_get(a_uint32_t dev_id,
	fal_port_t port_id, fal_src_filter_config_t *src_filter_config)
{
	sw_error_t rv = SW_OK;
	a_bool_t src_filter_bypass;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(src_filter_config);

	port_id = FAL_PORT_ID_VALUE(port_id);
	ADPT_PPE_PORT_ID_CHECK(port_id);
	rv = ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_get(dev_id, port_id,
			&src_filter_bypass);
	SW_RTN_ON_ERROR(rv);
	if(src_filter_bypass == A_TRUE)
	{
		src_filter_config->src_filter_enable = A_FALSE;
	}
	else
	{
		src_filter_config->src_filter_enable = A_TRUE;
	}

	rv = ppe_mru_mtu_ctrl_tbl_source_filtering_mode_get(dev_id,
			port_id, &(src_filter_config->src_filter_mode));

	return rv;
}

static sw_error_t
_adpt_ppe_port_source_filter_config_set(a_uint32_t dev_id,
	fal_port_t port_id, fal_src_filter_config_t *src_filter_config)
{
	sw_error_t rv = SW_OK;
	a_bool_t src_filter_bypass;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(src_filter_config);

	port_id = FAL_PORT_ID_VALUE(port_id);
	ADPT_PPE_PORT_ID_CHECK(port_id);
	if(src_filter_config->src_filter_enable == A_TRUE)
	{
		src_filter_bypass = A_FALSE;
	}
	else
	{
		src_filter_bypass = A_TRUE;
	}
	rv = ppe_mru_mtu_ctrl_tbl_source_filtering_bypass_set(dev_id, port_id,
			src_filter_bypass);
	SW_RTN_ON_ERROR(rv);
	rv = ppe_mru_mtu_ctrl_tbl_source_filtering_mode_set(dev_id, port_id,
			src_filter_config->src_filter_mode);

	return rv;
}

sw_error_t
_adpt_ppe_port_source_filter_get(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	fal_src_filter_config_t src_filter_config;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	rv = _adpt_ppe_port_source_filter_config_get(dev_id, port_id,
			&src_filter_config);
	SW_RTN_ON_ERROR(rv);
	*enable = src_filter_config.src_filter_enable;

	return rv;
}

sw_error_t
_adpt_ppe_port_source_filter_set(a_uint32_t dev_id,
	fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	fal_src_filter_config_t src_filter_config;

	ADPT_DEV_ID_CHECK(dev_id);

	rv = _adpt_ppe_port_source_filter_config_get(dev_id,
			port_id, &src_filter_config);
	SW_RTN_ON_ERROR(rv);
	src_filter_config.src_filter_enable = enable;
	rv = _adpt_ppe_port_source_filter_config_set(dev_id, port_id,
			&src_filter_config);

	return rv;
}

sw_error_t
adpt_ppe_port_source_filter_get(a_uint32_t dev_id,
				fal_port_t port_id, a_bool_t * enable)
{
	ADPT_DEV_ID_CHECK(dev_id);

	return _adpt_ppe_port_source_filter_get(dev_id, port_id, enable);
}

sw_error_t
adpt_ppe_port_source_filter_set(a_uint32_t dev_id,
				fal_port_t port_id, a_bool_t enable)

{
	ADPT_DEV_ID_CHECK(dev_id);

	return _adpt_ppe_port_source_filter_set(dev_id, port_id, enable);
}

sw_error_t
adpt_ppe_port_source_filter_config_get(a_uint32_t dev_id,
				fal_port_t port_id, fal_src_filter_config_t* src_filter_config)
{
	ADPT_DEV_ID_CHECK(dev_id);

	return _adpt_ppe_port_source_filter_config_get(dev_id, port_id, src_filter_config);
}

sw_error_t
adpt_ppe_port_source_filter_config_set(a_uint32_t dev_id,
				fal_port_t port_id, fal_src_filter_config_t *src_filter_config)

{
	ADPT_DEV_ID_CHECK(dev_id);

	return _adpt_ppe_port_source_filter_config_set(dev_id, port_id, src_filter_config);
}

sw_error_t
adpt_hppe_port_promisc_mode_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
{
	sw_error_t rv = SW_OK;
	union port_bridge_ctrl_u port_bridge_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(enable);

	if(ADPT_IS_VPORT(port_id))
	{
		a_uint32_t port_value = 0;
		port_value = FAL_PORT_ID_VALUE(port_id);
#if defined(HTTPPE)
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE)
			return adpt_httppe_vp_port_promisc_mode_get(dev_id, port_id, enable);
#endif
		return appe_l2_vp_port_tbl_promisc_en_get(dev_id, port_value,
			enable);
	}
	rv = hppe_port_bridge_ctrl_get(dev_id, port_id, &port_bridge_ctrl);

	if( rv != SW_OK )
		return rv;

	*enable = port_bridge_ctrl.bf.promisc_en;

	return SW_OK;
}

sw_error_t
adpt_hppe_port_promisc_mode_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union port_bridge_ctrl_u port_bridge_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	if(ADPT_IS_VPORT(port_id))
	{
		a_uint32_t port_value = 0;
		port_value = FAL_PORT_ID_VALUE(port_id);
#if defined(HTTPPE)
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE)
			return adpt_httppe_vp_port_promisc_mode_set(dev_id, port_id,
				enable);
#endif
		return appe_l2_vp_port_tbl_promisc_en_set(dev_id, port_value,
			enable);
	}
	rv = hppe_port_bridge_ctrl_get(dev_id, port_id, &port_bridge_ctrl);

	if( rv != SW_OK )
		return rv;

	port_bridge_ctrl.bf.promisc_en = enable;

	return hppe_port_bridge_ctrl_set(dev_id, port_id, &port_bridge_ctrl);
}

sw_error_t
adpt_hppe_port_bridge_txmac_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	union port_bridge_ctrl_u port_bridge_ctrl = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	rv = hppe_port_bridge_ctrl_get(dev_id, port_id, &port_bridge_ctrl);

	if( rv != SW_OK )
		return rv;

	if(enable == A_TRUE)
		port_bridge_ctrl.bf.txmac_en= 1;
	else
		port_bridge_ctrl.bf.txmac_en= 0;

	return hppe_port_bridge_ctrl_set(dev_id, port_id, &port_bridge_ctrl);
}

sw_error_t
adpt_hppe_port_mux_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t port_type)
{
	sw_error_t rv = SW_OK;
	union port_mux_ctrl_u port_mux_ctrl;
	a_uint32_t mode, mode1;

	memset(&port_mux_ctrl, 0, sizeof(port_mux_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);

	rv = hppe_port_mux_ctrl_get(dev_id, &port_mux_ctrl);
	port_mux_ctrl.bf.port4_pcs_sel = 1;

	if (port_id == SSDK_PHYSICAL_PORT5)
	{
		if (port_type == PORT_GMAC_TYPE)
		{
			mode = ssdk_dt_global_get_mac_mode(dev_id, SSDK_UNIPHY_INSTANCE0);
			mode1 = ssdk_dt_global_get_mac_mode(dev_id, SSDK_UNIPHY_INSTANCE1);
			if ((((mode == PORT_WRAPPER_PSGMII) ||
			      (mode == PORT_WRAPPER_PSGMII_FIBER)) &&
			     (mode1 == PORT_WRAPPER_MAX)) ||
			    ((mode == PORT_WRAPPER_SGMII4_RGMII4) && (mode1 == PORT_WRAPPER_MAX)))
			{
				port_mux_ctrl.bf.port5_pcs_sel = 1;
				port_mux_ctrl.bf.port5_gmac_sel = 1;
			}
			else if (mode1 == PORT_WRAPPER_SGMII0_RGMII4 ||
					mode1 == PORT_WRAPPER_SGMII_CHANNEL0 ||
					mode1 == PORT_WRAPPER_SGMII_FIBER)
			{
				port_mux_ctrl.bf.port5_pcs_sel = 2;
				port_mux_ctrl.bf.port5_gmac_sel = 1;
			}
		}
		else if (port_type == PORT_XGMAC_TYPE)
		{
			port_mux_ctrl.bf.port5_pcs_sel = 2;
			port_mux_ctrl.bf.port5_gmac_sel = 0;
		}
	}
	else if (port_id == SSDK_PHYSICAL_PORT6)
	{
		if (port_type == PORT_GMAC_TYPE)
		{
			port_mux_ctrl.bf.port6_pcs_sel = 1;
			port_mux_ctrl.bf.port6_gmac_sel = 1;
		}
		else if (port_type == PORT_XGMAC_TYPE)
		{
			port_mux_ctrl.bf.port6_pcs_sel = 1;
			port_mux_ctrl.bf.port6_gmac_sel = 0;
		}
	}
	else
	{
		return SW_OK;
	}
	rv = hppe_port_mux_ctrl_set(dev_id, &port_mux_ctrl);

	return rv;
}

#if 0
static a_uint32_t port_lpi_sleep_timer[][SSDK_PHYSICAL_PORT6] = {
	{218, 218, 218, 218, 218, 218},
	{218, 218, 218, 218, 218, 218},
	{218, 218, 218, 218, 218, 218},
}; /* unit is us*/
static a_uint32_t port_lpi_wakeup_timer[][SSDK_PHYSICAL_PORT6] = {
	{27, 27, 27, 27, 27, 27},
	{27, 27, 27, 27, 27, 27},
	{27, 27, 27, 27, 27, 27},
}; /* unit is us*/
#endif
#ifdef MRPPE
static sw_error_t
_adpt_mrppe_port_gmac_eee_status_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	union lpi_port_enable_u lpi_port_enable = {0};

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	/*enable lpi*/
	rv = mrppe_lpi_port_enable_get(dev_id, port_id, &lpi_port_enable);
	SW_RTN_ON_ERROR (rv);
	lpi_port_enable.bf.lpi_port_en = port_eee_cfg->lpi_tx_enable;
	rv = mrppe_lpi_port_enable_set(dev_id, port_id, &lpi_port_enable);

	return rv;
}

static sw_error_t
_adpt_mrppe_port_gmac_eee_timer_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	union lpi_port_wakeup_timer_u lpi_port_wakeup_timer = {0};
	union lpi_port_sleep_timer_u lpi_port_sleep_timer = {0};
	union lpi_1us_cnt_u lpi_1us_cnt = {0};

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	/*configure 1us cnt*/
	rv = mrppe_lpi_1us_cnt_get(dev_id, port_id, &lpi_1us_cnt);
	SW_RTN_ON_ERROR (rv);
	lpi_1us_cnt.bf.lpi_1us_cnt_val = adpt_chip_freq_get(dev_id);
	rv = mrppe_lpi_1us_cnt_set(dev_id, port_id, &lpi_1us_cnt);
	SW_RTN_ON_ERROR (rv);
	/*configure wakeup timer*/
	if(port_eee_cfg->lpi_wakeup_timer != 0) {
		rv = mrppe_lpi_wakeup_timer_get(dev_id, port_id, &lpi_port_wakeup_timer);
		SW_RTN_ON_ERROR (rv);
		lpi_port_wakeup_timer.bf.lpi_port_wakeup_timer = port_eee_cfg->lpi_wakeup_timer;
		rv = mrppe_lpi_wakeup_timer_set(dev_id, port_id, &lpi_port_wakeup_timer);
		SW_RTN_ON_ERROR (rv);
	}
	/*configure sleep timer*/
	if(port_eee_cfg->lpi_sleep_timer != 0) {
		rv = mrppe_lpi_sleep_timer_get(dev_id, port_id, &lpi_port_sleep_timer);
		SW_RTN_ON_ERROR (rv);
		lpi_port_sleep_timer.bf.lpi_port_sleep_timer = port_eee_cfg->lpi_sleep_timer;
		rv = mrppe_lpi_sleep_timer_set(dev_id, port_id, &lpi_port_sleep_timer);
		SW_RTN_ON_ERROR (rv);
	}
	return SW_OK;
}

static sw_error_t
_adpt_mrppe_port_gmac_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	union lpi_port_enable_u lpi_port_enable = {0};
	union lpi_port_wakeup_timer_u lpi_port_wakeup_timer = {0};
	union lpi_port_sleep_timer_u lpi_port_sleep_timer = {0};

	port_id = ppe_port_to_gmac_id(dev_id, port_id);
	rv = mrppe_lpi_port_enable_get(dev_id, port_id, &lpi_port_enable);
	SW_RTN_ON_ERROR (rv);
	if(lpi_port_enable.bf.lpi_port_en)
		port_eee_cfg->lpi_tx_enable = A_TRUE;
	else
		port_eee_cfg->lpi_tx_enable = A_FALSE;

	rv = mrppe_lpi_wakeup_timer_get(dev_id, port_id, &lpi_port_wakeup_timer);
	SW_RTN_ON_ERROR (rv);
	port_eee_cfg->lpi_wakeup_timer = lpi_port_wakeup_timer.bf.lpi_port_wakeup_timer;

	rv = mrppe_lpi_sleep_timer_get(dev_id, port_id, &lpi_port_sleep_timer);
	SW_RTN_ON_ERROR (rv);
	port_eee_cfg->lpi_sleep_timer = lpi_port_sleep_timer.bf.lpi_port_sleep_timer;

	return SW_OK;
}
#else
static sw_error_t
_adpt_hppe_port_gmac_eee_status_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	a_uint32_t enable = 0;
	union lpi_enable_u lpi_enable = {0};

	/*enable lpi*/
	rv = hppe_lpi_enable_get(dev_id, port_id, &lpi_enable);
	SW_RTN_ON_ERROR(rv);
	enable = port_eee_cfg->lpi_tx_enable;
	lpi_enable.val &= ~(0x1 << (port_id - 1));
	lpi_enable.val |= (enable << (port_id - 1));
	rv = hppe_lpi_enable_set(dev_id, port_id, &lpi_enable);

	return rv;
}

static sw_error_t
_adpt_hppe_port_gmac_eee_timer_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	union lpi_port_timer_u lpi_port_timer = {0};
	union lpi_cnt_u lpi_1us_cnt = {0};

	/*configure the 1us cnt*/
	rv = hppe_lpi_cnt_get(dev_id, 0, &lpi_1us_cnt);
	SW_RTN_ON_ERROR (rv);
	lpi_1us_cnt.bf.lpi_cnt_val = adpt_chip_freq_get(dev_id);
	rv = hppe_lpi_cnt_set(dev_id, 0, &lpi_1us_cnt);
	SW_RTN_ON_ERROR (rv);
	/*configure wakeup timer and sleep timer*/
	rv = hppe_lpi_timer_get(dev_id, port_id, &lpi_port_timer);
	SW_RTN_ON_ERROR (rv);
	if(port_eee_cfg->lpi_wakeup_timer != 0)
		lpi_port_timer.bf.lpi_port_wakeup_timer = port_eee_cfg->lpi_wakeup_timer;
	if(port_eee_cfg->lpi_sleep_timer != 0)
		lpi_port_timer.bf.lpi_port_sleep_timer = port_eee_cfg->lpi_sleep_timer;
	rv = hppe_lpi_timer_set(dev_id, port_id, &lpi_port_timer);

	return rv;
}

static sw_error_t
_adpt_hppe_port_gmac_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	union lpi_enable_u lpi_enable = {0};
	union lpi_port_timer_u lpi_port_timer = {0};

	rv = hppe_lpi_enable_get(dev_id, port_id, &lpi_enable);
	SW_RTN_ON_ERROR (rv);

	if(((lpi_enable.val >> (port_id - 1)) & 0x1) == A_TRUE) {
		port_eee_cfg->lpi_tx_enable = A_TRUE;
	} else {
		port_eee_cfg->lpi_tx_enable = A_FALSE;
	}
	rv = hppe_lpi_timer_get(dev_id, port_id, &lpi_port_timer);
	SW_RTN_ON_ERROR (rv);
	port_eee_cfg->lpi_wakeup_timer = lpi_port_timer.bf.lpi_port_wakeup_timer;
	port_eee_cfg->lpi_sleep_timer = lpi_port_timer.bf.lpi_port_sleep_timer;

	return SW_OK;

}
#endif
static sw_error_t
_adpt_ppe_port_gmac_eee_status_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
#ifdef MRPPE
	return _adpt_mrppe_port_gmac_eee_status_set(dev_id, port_id, port_eee_cfg);
#else
	return _adpt_hppe_port_gmac_eee_status_set(dev_id, port_id, port_eee_cfg);
#endif
}

static sw_error_t
_adpt_ppe_port_gmac_eee_timer_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
#ifdef MRPPE
	return _adpt_mrppe_port_gmac_eee_timer_set(dev_id, port_id, port_eee_cfg);
#else
	return _adpt_hppe_port_gmac_eee_timer_set(dev_id, port_id, port_eee_cfg);
#endif
}

static sw_error_t
_adpt_ppe_port_gmac_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_eee_cfg);
/* the port_eee_cfg cannot be cleared as the current port_eee_cfg has phy eee information */
#ifdef MRPPE
	return _adpt_mrppe_port_gmac_eee_cfg_get(dev_id, port_id, port_eee_cfg);
#else
	return _adpt_hppe_port_gmac_eee_cfg_get(dev_id, port_id, port_eee_cfg);
#endif
}

static sw_error_t
_adpt_hppe_port_xgmac_eee_timer_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = 0;
	a_uint32_t xgmac_id = 0;
	union mac_1us_tic_counter_u mac_1us_tic_counter;


	memset(&mac_1us_tic_counter, 0, sizeof(mac_1us_tic_counter));
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_eee_cfg);

	xgmac_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_1us_tic_counter_get(dev_id, xgmac_id, &mac_1us_tic_counter);
	SW_RTN_ON_ERROR (rv);
	mac_1us_tic_counter.bf.tic_1us_cntr = adpt_chip_freq_get(dev_id) - 1;
	rv = hppe_mac_1us_tic_counter_set(dev_id, xgmac_id, &mac_1us_tic_counter);
	SW_RTN_ON_ERROR (rv);
	/* configure sleep timer */
	if(port_eee_cfg->lpi_sleep_timer) {
		union mac_lpi_auto_entry_timer_u mac_lpi_auto_entry_timer = {0};
		a_uint32_t sleep_timer_tmp = 0;
		/* This field is programmed with a value in units of 8 micro-seconds */
		if (port_eee_cfg->lpi_sleep_timer % 8 == 0)
			sleep_timer_tmp = port_eee_cfg->lpi_sleep_timer/8;
		else
			sleep_timer_tmp = port_eee_cfg->lpi_sleep_timer/8 + 1;
		rv = hppe_mac_lpi_auto_entry_timer_get(dev_id, xgmac_id, &mac_lpi_auto_entry_timer);
		SW_RTN_ON_ERROR (rv);
		if (mac_lpi_auto_entry_timer.bf.lpiet != sleep_timer_tmp) {
			mac_lpi_auto_entry_timer.bf.lpiet = sleep_timer_tmp;
			rv = hppe_mac_lpi_auto_entry_timer_set(dev_id, xgmac_id, &mac_lpi_auto_entry_timer);
			SW_RTN_ON_ERROR (rv);
		}
	}
	if(port_eee_cfg->lpi_wakeup_timer) {
		union mac_lpi_timers_control_u mac_lpi_timers_control = {0};

		rv = hppe_mac_lpi_timers_control_get(dev_id, xgmac_id, &mac_lpi_timers_control);
		SW_RTN_ON_ERROR (rv);
		if (port_eee_cfg->lpi_wakeup_timer != mac_lpi_timers_control.bf.twt) {
			mac_lpi_timers_control.bf.twt = port_eee_cfg->lpi_wakeup_timer;
			rv = hppe_mac_lpi_timers_control_set(dev_id, xgmac_id, &mac_lpi_timers_control);
			SW_RTN_ON_ERROR (rv);
		}
	}

	return SW_OK;
}

static sw_error_t
_adpt_hppe_port_xgmac_eee_status_set(a_uint32_t dev_id,
	fal_port_t port_id, fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = 0;
	a_uint32_t xgmac_id = 0;
	union mac_lpi_control_status_u mac_lpi_control_status;

	memset(&mac_lpi_control_status, 0, sizeof(mac_lpi_control_status));

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_eee_cfg);

	xgmac_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_lpi_control_status_get(dev_id, xgmac_id, &mac_lpi_control_status);
	SW_RTN_ON_ERROR (rv);
	mac_lpi_control_status.bf.lpitxen = port_eee_cfg->lpi_tx_enable;
	mac_lpi_control_status.bf.pls = 0x1;
	mac_lpi_control_status.bf.lpitxa = 0x1;
	mac_lpi_control_status.bf.lpite = 0x1;
	rv = hppe_mac_lpi_control_status_set(dev_id, xgmac_id, &mac_lpi_control_status);

	return SW_OK;
}

static sw_error_t
_adpt_hppe_port_xgmac_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = 0;
	union mac_lpi_control_status_u mac_lpi_control_status;
	union mac_lpi_timers_control_u mac_lpi_timers_control;
	union mac_lpi_auto_entry_timer_u mac_lpi_auto_entry_timer;
	a_uint32_t xgmac_id = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_eee_cfg);
	memset(&mac_lpi_control_status, 0, sizeof(mac_lpi_control_status));
	memset(&mac_lpi_timers_control, 0, sizeof(mac_lpi_timers_control));

	xgmac_id = ppe_port_to_xgmac_id(dev_id, port_id);
	rv = hppe_mac_lpi_control_status_get(dev_id, xgmac_id, &mac_lpi_control_status);
	SW_RTN_ON_ERROR (rv);
	port_eee_cfg->lpi_tx_enable = mac_lpi_control_status.bf.lpitxen;
	rv = hppe_mac_lpi_timers_control_get(dev_id, xgmac_id, &mac_lpi_timers_control);
	SW_RTN_ON_ERROR (rv);
	port_eee_cfg->lpi_wakeup_timer =  mac_lpi_timers_control.bf.twt;
	rv = hppe_mac_lpi_auto_entry_timer_get(dev_id, xgmac_id, &mac_lpi_auto_entry_timer);
	SW_RTN_ON_ERROR (rv);
	port_eee_cfg->lpi_sleep_timer = mac_lpi_auto_entry_timer.bf.lpiet * 8;

	return rv;
}

static sw_error_t
adpt_ppe_port_mac_eee_status_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type = 0;

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE)
		rv = _adpt_hppe_port_xgmac_eee_status_set(dev_id, port_id, port_eee_cfg);
	else if (port_mac_type == PORT_GMAC_TYPE)
		rv = _adpt_ppe_port_gmac_eee_status_set( dev_id, port_id, port_eee_cfg);
	else
		return SW_BAD_VALUE;

	return rv;
}

static sw_error_t
adpt_ppe_port_mac_eee_timer_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	a_uint32_t port_mac_type = 0;

	port_mac_type =qca_ppe_port_mac_type_get(dev_id, port_id);
	if (port_mac_type == PORT_XGMAC_TYPE) {
		return _adpt_hppe_port_xgmac_eee_timer_set(dev_id, port_id, port_eee_cfg);
	} else if (port_mac_type == PORT_GMAC_TYPE) {
		return _adpt_ppe_port_gmac_eee_timer_set( dev_id, port_id, port_eee_cfg);
	} else {
		return SW_BAD_VALUE;
	}
}

static sw_error_t
adpt_ppe_port_mac_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	a_uint32_t port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);

	if (port_mac_type == PORT_XGMAC_TYPE)
		return _adpt_hppe_port_xgmac_eee_cfg_get( dev_id, port_id, port_eee_cfg);
	else if (port_mac_type == PORT_GMAC_TYPE)
		return _adpt_ppe_port_gmac_eee_cfg_get( dev_id, port_id, port_eee_cfg);
	else
		return SW_BAD_VALUE;
}

static sw_error_t
adpt_ppe_port_interface_eee_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	ADPT_DEV_ID_CHECK(dev_id);

	hsl_port_phy_eee_get(dev_id, port_id, port_eee_cfg);
	return adpt_ppe_port_mac_eee_cfg_get(dev_id, port_id, port_eee_cfg);
}

static sw_error_t
adpt_hppe_port_mac_eee_timer_adjust(a_uint32_t dev_id, fal_port_t port_id,
	a_uint32_t speed, struct qca_phy_priv *priv)
{
	fal_port_eee_cfg_t port_eee_cfg = {0};
	a_uint32_t wakeup_timer_val = 0, sleep_timer_val = PORT_LPI_SLEEP_TIMER_DEFAULT;
	sw_error_t rv = SW_OK;
	a_uint32_t port_mac_type = qca_ppe_port_mac_type_get(dev_id, port_id);

	if (port_mac_type != PORT_XGMAC_TYPE)
		return SW_OK;

	rv = adpt_ppe_port_mac_eee_cfg_get(dev_id, port_id, &port_eee_cfg);
	SW_RTN_ON_ERROR(rv);

	if (port_eee_cfg.lpi_tx_enable == A_FALSE ||
		(priv->ports[port_id].lpi_wakeup_timer_force == A_TRUE &&
		priv->ports[port_id].lpi_sleep_timer_force == A_TRUE))
		return SW_OK;

	switch (speed) {
		case FAL_SPEED_10000:
			wakeup_timer_val = PORT_LPI_WAKEUP_TIMER_10000M;
			sleep_timer_val = PORT_LPI_SLEEP_TIMER_10000M;
			break;
		case FAL_SPEED_5000:
			wakeup_timer_val = PORT_LPI_WAKEUP_TIMER_5000M;
			break;
		case FAL_SPEED_2500:
			wakeup_timer_val = PORT_LPI_WAKEUP_TIMER_2500M;
			break;
		case FAL_SPEED_1000:
			wakeup_timer_val = PORT_LPI_WAKEUP_TIMER_1000M;
			sleep_timer_val = PORT_LPI_SLEEP_TIMER_1000M;
			break;
		case FAL_SPEED_100:
			wakeup_timer_val = PORT_LPI_WAKEUP_TIMER_100M;
			break;
		default:
			/* No adjustments for other speeds */
			return SW_OK;
	}

	if (priv->ports[port_id].lpi_wakeup_timer_force == A_FALSE)
		port_eee_cfg.lpi_wakeup_timer = wakeup_timer_val;

	if (priv->ports[port_id].lpi_sleep_timer_force == A_FALSE)
		port_eee_cfg.lpi_sleep_timer = sleep_timer_val;

	return adpt_ppe_port_mac_eee_timer_set(dev_id, port_id, &port_eee_cfg);
}

/*
 * adpt_ppe_port_mac_eee_can_enable - check whether MAC-side EEE can be enabled
 *                                    on the given port
 * @dev_id:  device ID
 * @port_id: port ID
 *
 * Returns A_TRUE if EEE may be enabled, A_FALSE otherwise.
 *
 * For USXGMII/UQXGMII/10G BASER interface modes, some XPCS would not support eee,
 * so the XPCS EEE capability register of the corresponding uniphy
 * instance must be checked.
 *
 * For other interface modes (e.g. SGMII, SGMII+), pcs always support EEE, so A_TRUE is
 * returned unconditionally.
 */
static a_bool_t adpt_ppe_port_mac_eee_can_enable(a_uint32_t dev_id, fal_port_t port_id)
{
	sw_error_t rv;
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);
	if (rv != SW_OK)
		return A_FALSE;
	/* 10GBASER mode also use XPCS and may be connected with laguna */
	if (port_mode == PORT_USXGMII || port_mode == PORT_UQXGMII || port_mode == PORT_10GBASE_R) {
		a_uint32_t uniphy_index = hsl_port_to_uniphy(dev_id, port_id);

		if (uniphy_index >= SSDK_MAX_UNIPHY_INSTANCE)
			return A_FALSE;
		if (!adpt_hppe_uniphy_xpcs_eee_support(dev_id, uniphy_index))
			return A_FALSE;
	}

	return A_TRUE;
}

static sw_error_t
adpt_ppe_port_mac_eee_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);

	/*
	 * If the caller requests LPI TX to be enabled but the hardware does not
	 * support EEE on this port, silently skip the configuration (return SW_OK
	 * rather than an error) so that upper layers do not treat this as a failure.
	 */
	if (port_eee_cfg->lpi_tx_enable && !adpt_ppe_port_mac_eee_can_enable(dev_id, port_id)) {
		SSDK_DEBUG("port %d: EEE not supported on this interface, "
			"skipping EEE configuration\n", port_id);
		return SW_OK;
	}
	rv = adpt_ppe_port_mac_eee_status_set(dev_id, port_id, port_eee_cfg);
	SW_RTN_ON_ERROR(rv);
	/* If lpi_wakeup_timer is non-zero, wakeup_timer_force will be enabled, */
	/* indicating that the wakeup timer is set via a shell command. */
	/* If lpi_wakeup_timer is zero, wakeup_timer_force will be disabled, */
	/* and the wakeup timer will be automatically adjusted based on the link speed. */
	if(port_eee_cfg->lpi_wakeup_timer)
		priv->ports[port_id].lpi_wakeup_timer_force = A_TRUE;
	else
		priv->ports[port_id].lpi_wakeup_timer_force = A_FALSE;

	if(port_eee_cfg->lpi_sleep_timer)
		priv->ports[port_id].lpi_sleep_timer_force = A_TRUE;
	else
		priv->ports[port_id].lpi_sleep_timer_force = A_FALSE;

	adpt_hppe_port_mac_eee_timer_adjust(dev_id, port_id,
		priv->ports[port_id].port_old_speed, priv);

	return adpt_ppe_port_mac_eee_timer_set(dev_id, port_id, port_eee_cfg);
}

static sw_error_t
adpt_ppe_port_interface_eee_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_eee_cfg_t *port_eee_cfg)
{
	ADPT_DEV_ID_CHECK(dev_id);

	hsl_port_phy_eee_set(dev_id, port_id, port_eee_cfg);
	return adpt_ppe_port_mac_eee_cfg_set(dev_id, port_id, port_eee_cfg);
}

static sw_error_t
adpt_hppe_port_phy_status_get(a_uint32_t dev_id, a_uint32_t port_id,
				struct port_phy_status *phy_status)
{
	sw_error_t rv = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(phy_status);

	/* for those ports without PHY device should be sfp port or a internal port*/
	if (A_FALSE == hsl_port_phy_connected (dev_id, port_id)) {
		if (port_id != SSDK_PHYSICAL_PORT0) {
			fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;
			rv = adpt_hppe_port_interface_mode_get(dev_id, port_id,
				&mode);
			SW_RTN_ON_ERROR(rv);
			if ((A_TRUE == hsl_port_is_sfp(dev_id, port_id)) &&
				(mode == PORT_USXGMII)) {
#if defined(IN_SFP_PHY)
				rv = sfp_phy_port_status_get(dev_id, port_id, phy_status);
				SW_RTN_ON_ERROR (rv);
#endif
			} else {
				rv = _adpt_phy_status_get_from_ppe(dev_id,
					port_id, phy_status);
				SW_RTN_ON_ERROR (rv);
			}
		} else {
			return SW_NOT_SUPPORTED;
		}
	} else {
		rv = hsl_port_phydev_get_status(dev_id, port_id, phy_status);
		SW_RTN_ON_ERROR (rv);
	}

	return rv;

}
static void
adpt_hppe_uniphy_psgmii_port_reset(a_uint32_t dev_id, a_uint32_t uniphy_index,
			a_uint32_t port_id)
{
	union uniphy_channel0_input_output_4_u uniphy_channel0_input_output_4;
	union uniphy_channel1_input_output_4_u uniphy_channel1_input_output_4;
	union uniphy_channel2_input_output_4_u uniphy_channel2_input_output_4;
	union uniphy_channel3_input_output_4_u uniphy_channel3_input_output_4;
	union uniphy_channel4_input_output_4_u uniphy_channel4_input_output_4;

	memset(&uniphy_channel0_input_output_4, 0, sizeof(uniphy_channel0_input_output_4));
	memset(&uniphy_channel1_input_output_4, 0, sizeof(uniphy_channel1_input_output_4));
	memset(&uniphy_channel2_input_output_4, 0, sizeof(uniphy_channel2_input_output_4));
	memset(&uniphy_channel3_input_output_4, 0, sizeof(uniphy_channel3_input_output_4));
	memset(&uniphy_channel4_input_output_4, 0, sizeof(uniphy_channel4_input_output_4));

	if (port_id == SSDK_PHYSICAL_PORT1)
	{
		if (ssdk_uniphy_valid_check(dev_id, uniphy_index, PORT_WRAPPER_MAX) == A_FALSE) {
			SSDK_INFO("UNIPHY %d is not available on this SKU!\n", uniphy_index);
			return;
		}
		hppe_uniphy_channel0_input_output_4_get(dev_id, uniphy_index,
			&uniphy_channel0_input_output_4);
		uniphy_channel0_input_output_4.bf.newaddedfromhere_ch0_adp_sw_rstn = 0;
		hppe_uniphy_channel0_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel0_input_output_4);
		uniphy_channel0_input_output_4.bf.newaddedfromhere_ch0_adp_sw_rstn = 1;
		hppe_uniphy_channel0_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel0_input_output_4);
	}
	else if (port_id == SSDK_PHYSICAL_PORT2)
	{
		hppe_uniphy_channel1_input_output_4_get(dev_id, uniphy_index,
			&uniphy_channel1_input_output_4);
		uniphy_channel1_input_output_4.bf.newaddedfromhere_ch1_adp_sw_rstn = 0;
		hppe_uniphy_channel1_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel1_input_output_4);
		uniphy_channel1_input_output_4.bf.newaddedfromhere_ch1_adp_sw_rstn = 1;
		hppe_uniphy_channel1_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel1_input_output_4);
	}
	else if (port_id == SSDK_PHYSICAL_PORT3)
	{
		hppe_uniphy_channel2_input_output_4_get(dev_id, uniphy_index,
			&uniphy_channel2_input_output_4);
		uniphy_channel2_input_output_4.bf.newaddedfromhere_ch2_adp_sw_rstn = 0;
		hppe_uniphy_channel2_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel2_input_output_4);
		uniphy_channel2_input_output_4.bf.newaddedfromhere_ch2_adp_sw_rstn = 1;
		hppe_uniphy_channel2_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel2_input_output_4);
	}
	else if (port_id == SSDK_PHYSICAL_PORT4)
	{
		hppe_uniphy_channel3_input_output_4_get(dev_id, uniphy_index,
			&uniphy_channel3_input_output_4);
		uniphy_channel3_input_output_4.bf.newaddedfromhere_ch3_adp_sw_rstn = 0;
		hppe_uniphy_channel3_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel3_input_output_4);
		uniphy_channel3_input_output_4.bf.newaddedfromhere_ch3_adp_sw_rstn = 1;
		hppe_uniphy_channel3_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel3_input_output_4);
	}
	else if (port_id == SSDK_PHYSICAL_PORT5)
	{
		hppe_uniphy_channel4_input_output_4_get(dev_id, uniphy_index,
			&uniphy_channel4_input_output_4);
		uniphy_channel4_input_output_4.bf.newaddedfromhere_ch4_adp_sw_rstn = 0;
		hppe_uniphy_channel4_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel4_input_output_4);
		uniphy_channel4_input_output_4.bf.newaddedfromhere_ch4_adp_sw_rstn = 1;
		hppe_uniphy_channel4_input_output_4_set(dev_id, uniphy_index,
			&uniphy_channel4_input_output_4);
	}

	return;
}

static void
adpt_hppe_uniphy_usxgmii_port_ipg_tune_reset(a_uint32_t dev_id,
		a_uint32_t uniphy_index, a_uint32_t port_id)
{
	/* need to to port ipg tune reset operation if enable port ipg tune*/
#if 0
	union qp_usxg_opiton1_u qp_usxg_opiton1;

	memset(&qp_usxg_opiton1, 0, sizeof(qp_usxg_opiton1));

	if (adpt_chip_type_get(dev_id) == CHIP_APPE) {
		hppe_qp_usxg_opiton1_get(dev_id, uniphy_index, &qp_usxg_opiton1);
		if (port_id == SSDK_PHYSICAL_PORT1) {
			qp_usxg_opiton1.bf.xpcs_ch0_ipg_tune_rst = 1;
		} else if (port_id == SSDK_PHYSICAL_PORT2) {
			qp_usxg_opiton1.bf.xpcs_ch1_ipg_tune_rst = 1;
		} else if (port_id == SSDK_PHYSICAL_PORT3) {
			qp_usxg_opiton1.bf.xpcs_ch2_ipg_tune_rst = 1;
		} else if (port_id == SSDK_PHYSICAL_PORT4) {
			qp_usxg_opiton1.bf.xpcs_ch3_ipg_tune_rst = 1;
		}
		hppe_qp_usxg_opiton1_set(dev_id, uniphy_index, &qp_usxg_opiton1);
		msleep(1);
		if (port_id == SSDK_PHYSICAL_PORT1) {
			qp_usxg_opiton1.bf.xpcs_ch0_ipg_tune_rst = 0;
		} else if (port_id == SSDK_PHYSICAL_PORT2) {
			qp_usxg_opiton1.bf.xpcs_ch1_ipg_tune_rst = 0;
		} else if (port_id == SSDK_PHYSICAL_PORT3) {
			qp_usxg_opiton1.bf.xpcs_ch2_ipg_tune_rst = 0;
		} else if (port_id == SSDK_PHYSICAL_PORT4) {
			qp_usxg_opiton1.bf.xpcs_ch3_ipg_tune_rst = 0;
		}
		hppe_qp_usxg_opiton1_set(dev_id, uniphy_index, &qp_usxg_opiton1);
	}
#endif
	return;
}

static void
adpt_hppe_uniphy_usxgmii_port_reset(a_uint32_t dev_id, a_uint32_t uniphy_index,
			a_uint32_t port_id)
{
	union vr_xs_pcs_dig_ctrl1_u vr_xs_pcs_dig_ctrl1;
	union vr_mii_dig_ctrl1_u vr_mii_dig_ctrl1;
	a_uint32_t mode;

	memset(&vr_xs_pcs_dig_ctrl1, 0, sizeof(vr_xs_pcs_dig_ctrl1));
	memset(&vr_mii_dig_ctrl1, 0, sizeof(vr_mii_dig_ctrl1));

	if (adpt_hppe_uniphy_usxgmii_port_check(dev_id, uniphy_index, port_id)) {
		hppe_vr_xs_pcs_dig_ctrl1_get(dev_id, uniphy_index,
				&vr_xs_pcs_dig_ctrl1);
		vr_xs_pcs_dig_ctrl1.bf.usra_rst = 1;
		hppe_vr_xs_pcs_dig_ctrl1_set(dev_id, uniphy_index,
				&vr_xs_pcs_dig_ctrl1);
	}

	mode = ssdk_dt_global_get_mac_mode(dev_id, uniphy_index);
	if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
		if (port_id == SSDK_PHYSICAL_PORT2) {
			hppe_vr_mii_dig_ctrl1_channel1_get(0, uniphy_index,
					&vr_mii_dig_ctrl1);
			vr_mii_dig_ctrl1.bf.usra_rst = 1;
			hppe_vr_mii_dig_ctrl1_channel1_set(0, uniphy_index,
					&vr_mii_dig_ctrl1);
		} else if (port_id == SSDK_PHYSICAL_PORT3) {
			hppe_vr_mii_dig_ctrl1_channel2_get(0, uniphy_index,
					&vr_mii_dig_ctrl1);
			vr_mii_dig_ctrl1.bf.usra_rst = 1;
			hppe_vr_mii_dig_ctrl1_channel2_set(0, uniphy_index,
					&vr_mii_dig_ctrl1);
		} else if (port_id == SSDK_PHYSICAL_PORT4) {
			hppe_vr_mii_dig_ctrl1_channel3_get(0, uniphy_index,
					&vr_mii_dig_ctrl1);
			vr_mii_dig_ctrl1.bf.usra_rst = 1;
			hppe_vr_mii_dig_ctrl1_channel3_set(0, uniphy_index,
					&vr_mii_dig_ctrl1);
		}
	}

	return;
}

static void
adpt_hppe_uniphy_port_adapter_reset(a_uint32_t dev_id, a_uint32_t port_id)
{
	a_uint32_t uniphy_index = 0;
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;

	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);
	SSDK_DEBUG("port %d, uniphy_index %d, port_mode 0x%x\n", port_id, uniphy_index, port_mode);

	switch (port_mode)
	{
		case PORT_USXGMII:
		case PORT_UQXGMII:
			adpt_hppe_uniphy_usxgmii_port_reset(dev_id, uniphy_index, port_id);
			if (port_mode == PORT_UQXGMII)
			{
				adpt_hppe_uniphy_usxgmii_port_ipg_tune_reset(dev_id,
						uniphy_index, port_id);
			}
			break;
		case PHY_PSGMII_BASET:
		case PORT_QSGMII:
		case PHY_SGMII_BASET:
		case PORT_SGMII_FIBER:
		case PORT_SGMII_PLUS:
			if(uniphy_index == SSDK_UNIPHY_INSTANCE0)
			{
				adpt_hppe_uniphy_psgmii_port_reset(dev_id, uniphy_index, port_id);
			}
			else
			{
				if ((port_mode == PHY_SGMII_BASET) ||
					(port_mode == PORT_SGMII_FIBER) ||
					(port_mode == PORT_SGMII_PLUS))
				{
					/* only reset channel 0 */
					adpt_hppe_uniphy_psgmii_port_reset(dev_id,
							uniphy_index, SSDK_PHYSICAL_PORT1);
				}
			}
			break;
		default:
			break;
	}
	return;
}

static void
adpt_hppe_uniphy_usxgmii_mac_type_set(a_uint32_t dev_id, a_uint32_t port_id,
		fal_port_speed_t speed)
{
	a_uint32_t uniphy_index = SSDK_MAX_UNIPHY_INSTANCE;
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	union vr_mii_an_ctrl_u vr_mii_an_ctrl;

	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);
	memset(&vr_mii_an_ctrl, 0, sizeof(vr_mii_an_ctrl));

#if defined(MPPE)
	if (adpt_ppe_type_get(dev_id) == MPPE_TYPE)
	{
		if (port_mode == PORT_USXGMII)
		{
			if (speed == FAL_SPEED_10)
			{
				/* sel gmac */
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_GMAC_TYPE);
				_adpt_appe_port_mux_mac_set(dev_id, port_id, PORT_GMAC_TYPE);
				/* gmac use 4-bits MII width */
				hppe_vr_mii_an_ctrl_get(dev_id, uniphy_index, &vr_mii_an_ctrl);
				vr_mii_an_ctrl.bf.mii_ctrl = 0;
				hppe_vr_mii_an_ctrl_set(dev_id, uniphy_index, &vr_mii_an_ctrl);
			}
			else
			{
				/* sel xgmac */
				qca_ppe_port_mac_type_set(dev_id, port_id, PORT_XGMAC_TYPE);
				_adpt_appe_port_mux_mac_set(dev_id, port_id, PORT_XGMAC_TYPE);
				/* xgmac use 8-bits MII width */
				hppe_vr_mii_an_ctrl_get(dev_id, uniphy_index, &vr_mii_an_ctrl);
				vr_mii_an_ctrl.bf.mii_ctrl = 1;
				hppe_vr_mii_an_ctrl_set(dev_id, uniphy_index, &vr_mii_an_ctrl);
			}
		}
	}
#endif
	return;
}

static void
adpt_hppe_uniphy_usxgmii_speed_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, fal_port_speed_t speed)
{
	union sr_mii_ctrl_u sr_mii_ctrl;
	memset(&sr_mii_ctrl, 0, sizeof(sr_mii_ctrl));

	adpt_hppe_uniphy_usxgmii_status_get(dev_id, uniphy_index,
		port_id, &sr_mii_ctrl);

	sr_mii_ctrl.bf.duplex_mode = 1;
	if (speed == FAL_SPEED_10)
	{
		sr_mii_ctrl.bf.ss5 = 0;
		sr_mii_ctrl.bf.ss6 = 0;
		sr_mii_ctrl.bf.ss13 = 0;
	}
	else if (speed == FAL_SPEED_100)
	{
		sr_mii_ctrl.bf.ss5 = 0;
		sr_mii_ctrl.bf.ss6 = 0;
		sr_mii_ctrl.bf.ss13 = 1;
	}
	else if (speed == FAL_SPEED_1000)
	{
		sr_mii_ctrl.bf.ss5 = 0;
		sr_mii_ctrl.bf.ss6 = 1;
		sr_mii_ctrl.bf.ss13 = 0;
	}
	else if (speed == FAL_SPEED_10000)
	{
		sr_mii_ctrl.bf.ss5 = 0;
		sr_mii_ctrl.bf.ss6 = 1;
		sr_mii_ctrl.bf.ss13 = 1;
	}
	else if (speed == FAL_SPEED_2500)
	{
		sr_mii_ctrl.bf.ss5 = 1;
		sr_mii_ctrl.bf.ss6 = 0;
		sr_mii_ctrl.bf.ss13 = 0;
	}
	else if (speed == FAL_SPEED_5000)
	{
		sr_mii_ctrl.bf.ss5 = 1;
		sr_mii_ctrl.bf.ss6 = 0;
		sr_mii_ctrl.bf.ss13 = 1;
	}
	adpt_hppe_uniphy_usxgmii_status_set(dev_id, uniphy_index,
		port_id, &sr_mii_ctrl);

	return;
}
static void
adpt_hppe_uniphy_usxgmii_duplex_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, fal_port_duplex_t duplex)
{
	union sr_mii_ctrl_u sr_mii_ctrl;
	memset(&sr_mii_ctrl, 0, sizeof(sr_mii_ctrl));

	adpt_hppe_uniphy_usxgmii_status_get(dev_id, uniphy_index,
		port_id, &sr_mii_ctrl);

	if (duplex == FAL_FULL_DUPLEX)
		sr_mii_ctrl.bf.duplex_mode = 1;
	else
		sr_mii_ctrl.bf.duplex_mode = 0;

	adpt_hppe_uniphy_usxgmii_status_set(dev_id, uniphy_index,
		port_id, &sr_mii_ctrl);

	return;
}

sw_error_t
adpt_hppe_uniphy_usxgmii_autoneg_completed(a_uint32_t dev_id,
				a_uint32_t uniphy_index, a_uint32_t port_id)
{
	a_uint32_t autoneg_complete = 0, retries = 100;
	union vr_mii_an_intr_sts_u vr_mii_an_intr_sts;

	memset(&vr_mii_an_intr_sts, 0, sizeof(vr_mii_an_intr_sts));

	// swith uniphy xpcs auto-neg complete and clear interrupt
	while (autoneg_complete != 0x1) {
		mdelay(1);
		if (retries-- == 0)
		{
			printk("uniphy autoneg time out!\n");
			return SW_TIMEOUT;
		}
		adpt_hppe_uniphy_usxgmii_autoneg_status_get(dev_id, uniphy_index,
				port_id, &vr_mii_an_intr_sts);
		autoneg_complete = vr_mii_an_intr_sts.bf.cl37_ancmplt_intr;
	}

	vr_mii_an_intr_sts.bf.cl37_ancmplt_intr = 0;
	adpt_hppe_uniphy_usxgmii_autoneg_status_set(dev_id, uniphy_index,
				port_id, &vr_mii_an_intr_sts);

	return SW_OK;
}
static void
adpt_hppe_uniphy_speed_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_speed_t speed)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	a_uint32_t uniphy_index = 0;

	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);

	if ((port_mode == PORT_UQXGMII) || (port_mode == PORT_USXGMII)) {
		uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
		/* adpt_hppe_uniphy_usxgmii_autoneg_completed(dev_id, uniphy_index); */
		/* configure xpcs speed at usxgmii mode */
		adpt_hppe_uniphy_usxgmii_speed_set(dev_id, uniphy_index, port_id, speed);
	}

	return;
}
static void
adpt_hppe_uniphy_duplex_set(a_uint32_t dev_id, a_uint32_t port_id, fal_port_duplex_t duplex)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	a_uint32_t uniphy_index = 0;

	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);

	if ((port_mode == PORT_UQXGMII) || (port_mode == PORT_USXGMII)) {
		uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
		/* adpt_hppe_uniphy_usxgmii_autoneg_completed(0, uniphy_index); */
		/* configure xpcs duplex at usxgmii mode */
		adpt_hppe_uniphy_usxgmii_duplex_set(dev_id, uniphy_index, port_id, duplex);
	}

	return;
}
static void
adpt_hppe_uniphy_autoneg_status_check(a_uint32_t dev_id, a_uint32_t port_id)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	a_uint32_t uniphy_index = 0;

	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);

	if ((port_mode == PORT_UQXGMII) || (port_mode == PORT_USXGMII)) {
		uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
		adpt_hppe_uniphy_usxgmii_autoneg_completed(dev_id,uniphy_index, port_id);
	}
	return;
}

static void
adpt_hppe_sgmii_speed_clock_set(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	fal_port_speed_t phy_speed)
{
	switch (phy_speed) {
		case FAL_SPEED_10:
			ssdk_port_speed_clock_set(dev_id,
					port_id, SGMII_SPEED_10M_CLK);
			break;
		case FAL_SPEED_100:
			ssdk_port_speed_clock_set(dev_id,
					port_id, SGMII_SPEED_100M_CLK);
			break;
		case FAL_SPEED_1000:
			ssdk_port_speed_clock_set(dev_id,
					port_id, SGMII_SPEED_1000M_CLK);
			break;
		default:
			break;
	}
}
static void
adpt_hppe_pqsgmii_speed_clock_set(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	fal_port_speed_t phy_speed)
{
	switch (phy_speed) {
		case FAL_SPEED_10:
			ssdk_port_speed_clock_set(dev_id,
					port_id, PQSGMII_SPEED_10M_CLK);
			break;
		case FAL_SPEED_100:
			ssdk_port_speed_clock_set(dev_id,
					port_id, PQSGMII_SPEED_100M_CLK);
			break;
		case FAL_SPEED_1000:
			ssdk_port_speed_clock_set(dev_id,
					port_id, PQSGMII_SPEED_1000M_CLK);
			break;
		default:
			break;
	}
}

static void
adpt_hppe_usxgmii_speed_clock_set(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	fal_port_speed_t phy_speed)
{
	switch (phy_speed) {
		case FAL_SPEED_10:
			if (PORT_XGMAC_TYPE == qca_ppe_port_mac_type_get(dev_id, port_id)) {
				/* XGMAC with 8bit MII width */
				ssdk_port_speed_clock_set(dev_id,
						port_id, USXGMII_SPEED_10M_CLK);
			} else {
				/* GMAC with 4bit MII width */
				ssdk_port_speed_clock_set(dev_id,
						port_id, USXGMII_SPEED_10M_CLK*2);
#if defined(SSDK_RAW_CLOCK)
				writel(0x0, nsscc_clk_base_g + 0x624 + (port_id - 1)*8);
				writel(0x0, nsscc_clk_base_g + 0x628 + (port_id - 1)*8);
#else
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_RX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_10M_CLK*2);
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_TX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_10M_CLK*2);
#endif
			}
			break;
		case FAL_SPEED_100:
			if (PORT_XGMAC_TYPE == qca_ppe_port_mac_type_get(dev_id, port_id)) {
				/* XGMAC with 8bit MII width */
				ssdk_port_speed_clock_set(dev_id,
						port_id, USXGMII_SPEED_100M_CLK);
			} else {
				/* GMAC with 4bit MII width */
				ssdk_port_speed_clock_set(dev_id,
						port_id, USXGMII_SPEED_100M_CLK*2);
#if defined(SSDK_RAW_CLOCK)
				writel(0x0, nsscc_clk_base_g + 0x624 + (port_id - 1)*8);
				writel(0x0, nsscc_clk_base_g + 0x628 + (port_id - 1)*8);
#else
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_RX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_100M_CLK*2);
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_TX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_100M_CLK*2);
#endif
			}
			break;
		case FAL_SPEED_1000:
			ssdk_port_speed_clock_set(dev_id,
					port_id, USXGMII_SPEED_1000M_CLK);
			if (PORT_GMAC_TYPE == qca_ppe_port_mac_type_get(dev_id, port_id)) {
#if defined(SSDK_RAW_CLOCK)
				writel(0x0, nsscc_clk_base_g + 0x624 + (port_id - 1)*8);
				writel(0x0, nsscc_clk_base_g + 0x628 + (port_id - 1)*8);
#else
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_RX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_1000M_CLK);
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_TX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_1000M_CLK);
#endif
			}
			break;
		case FAL_SPEED_2500:
			if (PORT_XGMAC_TYPE == qca_ppe_port_mac_type_get(dev_id, port_id)) {
				ssdk_port_speed_clock_set(dev_id,
						port_id, USXGMII_SPEED_2500M_CLK);
			} else {
				ssdk_port_speed_clock_set(dev_id,
						port_id, USXGMII_SPEED_10000M_CLK);
#if defined(SSDK_RAW_CLOCK)
				writel(0x3, nsscc_clk_base_g + 0x624 + (port_id - 1)*8);
				writel(0x3, nsscc_clk_base_g + 0x628 + (port_id - 1)*8);
#else
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_RX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_2500M_CLK);
				ssdk_uniphy_clock_rate_set(dev_id,
						UNIPHY_PORT1_TX_DIV4_CLK_E + (port_id - 1)*2,
						USXGMII_SPEED_2500M_CLK);
#endif
			}
			break;
		case FAL_SPEED_5000:
			ssdk_port_speed_clock_set(dev_id,
					port_id, USXGMII_SPEED_5000M_CLK);
			break;
		case FAL_SPEED_10000:
			ssdk_port_speed_clock_set(dev_id,
					port_id, USXGMII_SPEED_10000M_CLK);
			break;
		default:
			break;
	}
}

static void
adpt_hppe_sgmiiplus_speed_clock_set(
	a_uint32_t dev_id,
	a_uint32_t port_id,
	fal_port_speed_t phy_speed)
{
	ssdk_port_speed_clock_set(dev_id, port_id, SGMII_PLUS_SPEED_2500M_CLK);

}


void
adpt_hppe_gcc_port_speed_clock_set(a_uint32_t dev_id, a_uint32_t port_id,
				fal_port_speed_t phy_speed)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);

	switch (port_mode)
	{
		case PORT_SGMII_PLUS:
			adpt_hppe_sgmiiplus_speed_clock_set(dev_id, port_id, phy_speed);
			break;
		case PORT_USXGMII:
		case PORT_10GBASE_R:
		case PORT_UQXGMII:
			adpt_hppe_usxgmii_speed_clock_set(dev_id, port_id, phy_speed);
			break;
		case PHY_SGMII_BASET:
		case PORT_SGMII_FIBER:
			adpt_hppe_sgmii_speed_clock_set(dev_id, port_id, phy_speed);
			break;
		case PHY_PSGMII_BASET:
		case PORT_QSGMII:
			adpt_hppe_pqsgmii_speed_clock_set(dev_id, port_id, phy_speed);
			break;
#if defined(JHPPE)
		case PORT_25GBASE_R:
			ssdk_port_speed_clock_set(dev_id, port_id, UNIPHY_CLK_RATE_781P25M);
			break;
#endif
		default:
			break;
	}
	return;
}

void
adpt_hppe_gcc_uniphy_clock_status_set(a_uint32_t dev_id, a_uint32_t port_id,
				a_bool_t enable)
{
	a_uint32_t uniphy_index = 0;
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;

	adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);
	if(port_mode == PORT_INTERFACE_MODE_MAX || port_mode == PORT_INTERNAL)
		return;
	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index, port_id, enable);
	return;
}

static sw_error_t
adpt_hppe_port_interface_mode_switch(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;
	fal_port_interface_mode_t port_mode_old = PORT_INTERFACE_MODE_MAX;
	fal_port_interface_mode_t port_mode_new = PORT_INTERFACE_MODE_MAX;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode_old);
	SW_RTN_ON_ERROR(rv);
	if (port_mode_old != PORT_INTERFACE_MODE_MAX)
		port_mode_old = ssdk_dt_get_port_mode(dev_id, port_id);
	port_mode_new = port_mode_old;
	rv = adpt_hppe_port_interface_mode_status_get(dev_id, port_id,
				&port_mode_new);
	SW_RTN_ON_ERROR(rv);

	if (port_mode_new != port_mode_old) {
		SSDK_DEBUG("Port %d change interface mode to %d from %d\n", port_id,
				port_mode_new, port_mode_old);
		rv = _adpt_hppe_port_interface_mode_set(dev_id, port_id,
				port_mode_new);
		SW_RTN_ON_ERROR(rv);
		rv = _adpt_hppe_port_interface_mode_apply(dev_id, A_FALSE);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

static sw_error_t
adpt_hppe_sfp_interface_mode_switch(a_uint32_t dev_id,
	a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;

	if (A_TRUE == hsl_port_is_sfp(dev_id, port_id)) {
		SSDK_DEBUG("sfp port %d change interface mode!\n", port_id);
		rv = adpt_hppe_port_interface_mode_switch(dev_id, port_id);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

static sw_error_t
adpt_hppe_phy_interface_mode_switch(a_uint32_t dev_id,
	a_uint32_t port_id)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	sw_error_t rv = SW_OK;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &port_mode);
	SW_RTN_ON_ERROR(rv);

	/* built-in PHY does not need interface mode switch */
	if (port_mode == PORT_INTERNAL)
		return SW_OK;

	if (A_TRUE == hsl_port_phy_connected(dev_id, port_id)) {
		SSDK_DEBUG("phy port %d change interface mode!\n", port_id);
		rv = adpt_hppe_port_interface_mode_switch(dev_id, port_id);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

a_bool_t
adpt_hppe_port_phy_status_change(struct qca_phy_priv *priv, a_uint32_t port_id,
				struct port_phy_status phy_status)
{
	if (port_id == 0 || port_id >= SW_MAX_NR_PORT)
		return A_FALSE;
	if ((a_uint32_t)phy_status.speed != priv->ports[port_id].port_old_speed)
		return A_TRUE;
	if ((a_uint32_t)phy_status.duplex != priv->ports[port_id].port_old_duplex)
		return A_TRUE;
	if (phy_status.tx_flowctrl != priv->ports[port_id].port_old_tx_flowctrl)
		return A_TRUE;
	if (phy_status.rx_flowctrl != priv->ports[port_id].port_old_rx_flowctrl)
		return A_TRUE;
	return A_FALSE;
}

static sw_error_t
adpt_hppe_port_mac_loopback_reset(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;
	a_bool_t lp_en = A_FALSE;

	if (qca_ppe_port_mac_type_get(dev_id, port_id) == PORT_XGMAC_TYPE) {
		rv = _adpt_hppe_port_xgmac_loopback_get(dev_id, port_id, &lp_en);
		SW_RTN_ON_ERROR(rv);
		if(!lp_en) {
			rv = _adpt_hppe_port_xgmac_loopback_set( dev_id, port_id, A_TRUE);
			SW_RTN_ON_ERROR(rv);
			/* 1ms is enough to release all packets */
			aos_mdelay(1);
			rv = _adpt_hppe_port_xgmac_loopback_set( dev_id, port_id, A_FALSE);
			SW_RTN_ON_ERROR(rv);
		}
	}

	return SW_OK;
}

#ifdef HMSPPE
sw_error_t
adpt_hmsppe_port_interface_clk_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_bool_t enable)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	sw_error_t rv = SW_OK;

	rv = fal_port_interface_mode_get(dev_id, port_id, &port_mode);
	SW_RTN_ON_ERROR(rv);
	if (port_mode == PORT_INTERNAL)
		qca_gcc_mac_port_clock_set(dev_id, port_id, enable);

	return SW_OK;
}

sw_error_t
adpt_hmsppe_port_clk_reset(a_uint32_t dev_id, a_uint32_t port_id)
{
	fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
	sw_error_t rv = SW_OK;

	rv = fal_port_interface_mode_get(dev_id, port_id, &port_mode);
	SW_RTN_ON_ERROR(rv);
	if (port_mode == PORT_INTERNAL)
		ssdk_port_clock_reset(dev_id, port_id);

	return SW_OK;
}
#endif

#ifdef CONFIG_PHYLINK
static sw_error_t
adpt_hppe_port_phylink_phy_status_get(a_uint32_t dev_id, a_uint32_t port_id,
	struct port_phy_status *phy_status)
{
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);

	if (port_id >= SW_MAX_NR_PORT)
		return SW_BAD_PARAM;

	phy_status->speed = priv->ports[port_id].phylink_link_state.speed;
	phy_status->duplex = priv->ports[port_id].phylink_link_state.duplex;
	phy_status->link_status = priv->ports[port_id].phylink_link_state.link;

	SSDK_DEBUG("port id:%d, link:%d, speed:%d, duplex:%d\n",
		port_id, phy_status->link_status, phy_status->speed, phy_status->duplex);
	return SW_OK;
}
#endif

sw_error_t
qca_hppe_mac_sw_sync_task(struct qca_phy_priv *priv)
{
	a_uint32_t port_id;
	struct port_phy_status phy_status = {0};
	a_bool_t status, link_changed;
	a_uint32_t portbmp[SW_MAX_NR_DEV] = {0};
	sw_error_t rv = SW_OK;

	portbmp[priv->device_id] = qca_ssdk_port_bmp_get(priv->device_id);

	for (port_id = 1; port_id < SW_MAX_NR_PORT; port_id ++) {

		if(!(portbmp[priv->device_id] & (0x1 << port_id)))
			continue;

		rv = adpt_hppe_sfp_interface_mode_switch(priv->device_id, port_id);
		if(rv) {
			SSDK_DEBUG("port %d sfp interface mode change failed\n", port_id);
		}
#ifdef CONFIG_PHYLINK
		if (hsl_port_is_sfp(priv->device_id, port_id) &&
			(adpt_hppe_port_phylink_is_enabled(priv->device_id, port_id))) {
			rv = adpt_hppe_port_phylink_phy_status_get(priv->device_id,
				port_id, &phy_status);
		} else
#endif
		{
			rv = adpt_hppe_port_phy_status_get(priv->device_id,
				port_id, &phy_status);
		}
		if (rv != SW_OK) {
			SSDK_DEBUG("failed to get port %d status return value is %d\n",
					port_id, rv);
			continue;
		}
		link_changed = A_FALSE;
		SSDK_DEBUG("polling task external phy %d link status is %d and speed is %d\n",
				port_id, phy_status.link_status, phy_status.speed);
		/* link status from up to down */
		if ((phy_status.link_status == PORT_LINK_DOWN) &&
			(priv->ports[port_id].port_old_link == PORT_LINK_UP))
		{
			link_changed = A_TRUE;
			SSDK_DEBUG("Port %d change to link down status\n", port_id);
			/* disable ppe port bridge txmac */
			adpt_hppe_port_bridge_txmac_set(priv->device_id, port_id, A_FALSE);
			aos_mdelay(10);
			/* disable rx mac */
			adpt_hppe_port_rxmac_status_set(priv->device_id, port_id, A_FALSE);
			/* release ppe port egress packets when link down */
			adpt_hppe_port_mac_loopback_reset(priv->device_id, port_id);
			priv->ports[port_id].port_old_link = phy_status.link_status;
			/* switch interface mode if necessary */
			if (adpt_hppe_phy_interface_mode_switch(priv->device_id,
					port_id) == SW_OK) {
				SSDK_DEBUG("Port %d the interface mode switched\n",
						port_id);
			}
#ifdef IN_FDB
			adpt_hppe_fdb_del_by_port(priv->device_id, port_id, !(FAL_FDB_DEL_STATIC));
#endif
			/* adjust PHY configuration after MAC operation */
			if(hsl_port_phy_connected(priv->device_id, port_id)) {
				HSL_PORT_PHY_API_RUN(adjust_link_post,
					priv->device_id, port_id);
			}
#ifdef HMSPPE
			if (adpt_ppe_type_get(priv->device_id) == HMSPPE_TYPE) {
				rv = adpt_hmsppe_port_interface_clk_set(priv->device_id, port_id, A_FALSE);
				if (rv != SW_OK) {
					continue;
				}
				rv = adpt_hmsppe_port_clk_reset(priv->device_id, port_id);
				if (rv != SW_OK) {
					continue;
				}
			}
#endif

		}
		/* link status from down to up*/
		if ((phy_status.link_status == PORT_LINK_UP) &&
			(priv->ports[port_id].port_old_link == PORT_LINK_DOWN))
		{
			link_changed = A_TRUE;
			SSDK_DEBUG("Port %d change to link up status\n", port_id);
			status = adpt_hppe_port_phy_status_change(priv, port_id, phy_status);
			/*disable tx mac*/
			adpt_hppe_port_txmac_status_set(priv->device_id, port_id, A_FALSE);
			/* switch interface mode if necessary */
			if (adpt_hppe_phy_interface_mode_switch(priv->device_id,
					port_id) == SW_OK) {
				SSDK_DEBUG("Port %d the interface mode switched\n",
						port_id);
			}
			/* first check uniphy auto-neg complete interrupt to usxgmii */
			adpt_hppe_uniphy_autoneg_status_check(priv->device_id, port_id);
			if (status == A_TRUE)
			{
				fal_port_interface_mode_t port_mode = PORT_INTERFACE_MODE_MAX;
				adpt_hppe_port_interface_mode_get(priv->device_id,
								  port_id, &port_mode);

				adpt_hppe_gcc_uniphy_clock_status_set(priv->device_id,
						port_id, A_FALSE);
				if ((a_uint32_t)phy_status.speed !=
						priv->ports[port_id].port_old_speed)
				{
					/* configure 4bit-GMAC or 8bit-XGMAC for usxgmii mode */
					adpt_hppe_uniphy_usxgmii_mac_type_set(priv->device_id,
							port_id, phy_status.speed);
					/* configure gcc speed clock according to current speed */
					adpt_hppe_gcc_port_speed_clock_set(priv->device_id, port_id,
							phy_status.speed);
					aos_mdelay(10);
					/* config uniphy speed to usxgmii mode */
					adpt_hppe_uniphy_speed_set(priv->device_id, port_id,
							phy_status.speed);
					if(hsl_port_phyid_get(priv->device_id, port_id) ==
						QCA8084_PHY)
					{
						/*do uniphy adpt reset before mac reset*/
						adpt_hppe_gcc_uniphy_clock_status_set(
							priv->device_id, port_id, A_TRUE);
						adpt_hppe_uniphy_port_adapter_reset(priv->device_id,
							port_id);
						aos_mdelay(10);
					}
					/* reset port mac when speed change under usxgmii mode */
					adpt_hppe_port_speed_change_port_clock_reset(priv->device_id,
						port_id);
					aos_mdelay(10);
					/* config mac speed */
					adpt_hppe_port_mac_speed_set(priv->device_id, port_id,
							phy_status.speed);
					adpt_hppe_port_mac_eee_timer_adjust(priv->device_id, port_id,
						phy_status.speed, priv);
					priv->ports[port_id].port_old_speed =
						(a_uint32_t)phy_status.speed;

					SSDK_DEBUG("Port %d is link up and speed change to %d\n",
							port_id, priv->ports[port_id].port_old_speed);
				}
				if ((a_uint32_t)phy_status.duplex !=
						priv->ports[port_id].port_old_duplex)
				{
					adpt_hppe_uniphy_duplex_set(priv->device_id, port_id,
							phy_status.duplex);
					adpt_hppe_port_mac_duplex_set(priv->device_id, port_id,
							phy_status.duplex);
					priv->ports[port_id].port_old_duplex =
						(a_uint32_t)phy_status.duplex;

					SSDK_DEBUG("Port %d is link up and duplex change to %d\n",
							port_id,
							priv->ports[port_id].port_old_duplex);
				}
				if (priv->ports[port_id].port_tx_flowctrl_forcemode != A_TRUE)
				{
					if (phy_status.duplex == FAL_HALF_DUPLEX)
					{
						phy_status.tx_flowctrl = A_TRUE;
					}
					if (phy_status.tx_flowctrl !=
							priv->ports[port_id].port_old_tx_flowctrl)
					{
						_adpt_hppe_port_txfc_status_set(priv->device_id,
								port_id, phy_status.tx_flowctrl);
						priv->ports[port_id].port_old_tx_flowctrl =
							phy_status.tx_flowctrl;

						SSDK_DEBUG("Port %d is link up and tx flowctrl"
							" change to %d\n", port_id,
							priv->ports[port_id].port_old_tx_flowctrl);
					}
				}
				if (priv->ports[port_id].port_rx_flowctrl_forcemode != A_TRUE)
				{
					if (phy_status.duplex == FAL_HALF_DUPLEX)
					{
						phy_status.rx_flowctrl = A_TRUE;
					}
					if (phy_status.rx_flowctrl !=
							priv->ports[port_id].port_old_rx_flowctrl)
					{
						_adpt_hppe_port_rxfc_status_set(priv->device_id,
								port_id, phy_status.rx_flowctrl);
						priv->ports[port_id].port_old_rx_flowctrl =
							phy_status.rx_flowctrl;

						SSDK_DEBUG("Port %d is link up and rx flowctrl"
							" change to %d\n", port_id,
							priv->ports[port_id].port_old_rx_flowctrl);
					}
				}
				adpt_hppe_gcc_uniphy_clock_status_set(priv->device_id,
						port_id, A_TRUE);

#if defined(JHPPE)
				if (port_mode == PORT_25GBASE_R) {
					a_uint32_t uniphy_index = hsl_port_to_uniphy(priv->device_id, port_id);
					/*
					 * XLGPCS soft reset after enabling uniphy port clocks to
					 * clear the pipeline in case of any garbage corruption.
					 */
					adpt_jhppe_uniphy_xlgpcs_soft_reset(priv->device_id,
									    uniphy_index);
				}
#endif

				adpt_hppe_uniphy_port_adapter_reset(priv->device_id, port_id);
			}
#ifdef HMSPPE
			if (adpt_ppe_type_get(priv->device_id) == HMSPPE_TYPE) {
				rv = adpt_hmsppe_port_interface_clk_set(priv->device_id, port_id, A_TRUE);
				if (rv != SW_OK) {
					continue;
				}
				rv = adpt_hmsppe_port_clk_reset(priv->device_id, port_id);
				if (rv != SW_OK) {
					continue;
				}
			}
#endif
			/* enable mac and ppe txmac*/
			adpt_hppe_port_txmac_status_set(priv->device_id, port_id, A_TRUE);
			adpt_hppe_port_rxmac_status_set(priv->device_id, port_id, A_TRUE);
			adpt_hppe_port_bridge_txmac_set(priv->device_id, port_id, A_TRUE);
			priv->ports[port_id].port_old_link = phy_status.link_status;
		}
		SSDK_DEBUG("polling task PPE port %d link status is %d and speed is %d\n",
				port_id, priv->ports[port_id].port_old_link,
				priv->ports[port_id].port_old_speed);
		if (link_changed) {
			unsigned char link_notify_speed = 0;

			link_notify_speed  = ssdk_to_link_notify_speed(phy_status.speed);
			ssdk_port_link_notify(port_id, phy_status.link_status,
				link_notify_speed, phy_status.duplex);
		}
	}
	return 0;
}

static sw_error_t
_adpt_hppe_port_cnt_enable_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;
	union mc_mtu_ctrl_tbl_u mc_mtu_ctrl_tbl;
	union port_eg_vlan_u port_eg_vlan;
#if defined(IN_TUNNEL)
	union tl_port_vp_tbl_u tl_port_vp_tbl;
#endif
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cnt_cfg);

	aos_mem_zero(&mru_mtu_ctrl_tbl, sizeof(union mru_mtu_ctrl_tbl_u));
	aos_mem_zero(&mc_mtu_ctrl_tbl, sizeof(union mc_mtu_ctrl_tbl_u));
	aos_mem_zero(&port_eg_vlan, sizeof(union port_eg_vlan_u));

	if (ADPT_IS_PPORT(port_id)) {
		rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_value, &mru_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);
		rv = hppe_mc_mtu_ctrl_tbl_get(dev_id, port_value, &mc_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);

		mru_mtu_ctrl_tbl.bf.rx_cnt_en = cnt_cfg->rx_cnt_en;
		mru_mtu_ctrl_tbl.bf.tx_cnt_en = cnt_cfg->uc_tx_cnt_en;
		mc_mtu_ctrl_tbl.bf.tx_cnt_en = cnt_cfg->mc_tx_cnt_en;

		rv = hppe_mru_mtu_ctrl_tbl_set(dev_id, port_value, &mru_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);
		rv = hppe_mc_mtu_ctrl_tbl_set(dev_id, port_value, &mc_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);

		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_port_eg_vlan_tx_counting_en_set(dev_id, port_id, cnt_cfg->uc_tx_cnt_en);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
			rv = hppe_port_eg_vlan_tx_counting_en_set(dev_id, port_value, cnt_cfg->uc_tx_cnt_en);
			SW_RTN_ON_ERROR(rv);
		}
	}
	else
	{
		rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_value, &mru_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);

		mru_mtu_ctrl_tbl.bf.rx_cnt_en = cnt_cfg->rx_cnt_en;
		mru_mtu_ctrl_tbl.bf.tx_cnt_en = cnt_cfg->uc_tx_cnt_en;

		rv = hppe_mru_mtu_ctrl_tbl_set(dev_id, port_value, &mru_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);
	}

#if defined(IN_TUNNEL)
	aos_mem_zero(&tl_port_vp_tbl, sizeof(union tl_port_vp_tbl_u));
	rv = appe_tl_port_vp_tbl_get(dev_id, port_value, &tl_port_vp_tbl);
	SW_RTN_ON_ERROR(rv);

	tl_port_vp_tbl.bf.rx_cnt_en = cnt_cfg->tl_rx_cnt_en;

	rv = appe_tl_port_vp_tbl_set(dev_id, port_value, &tl_port_vp_tbl);
	SW_RTN_ON_ERROR(rv);
#endif

	return SW_OK;
}

static sw_error_t
_adpt_hppe_port_cnt_enable_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg)
{
	sw_error_t rv = SW_OK;
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;
	union mc_mtu_ctrl_tbl_u mc_mtu_ctrl_tbl;
	union port_eg_vlan_u port_eg_vlan;
#if defined(IN_TUNNEL)
	union tl_port_vp_tbl_u tl_port_vp_tbl;
#endif
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cnt_cfg);

	aos_mem_zero(&mru_mtu_ctrl_tbl, sizeof(union mru_mtu_ctrl_tbl_u));
	aos_mem_zero(&mc_mtu_ctrl_tbl, sizeof(union mc_mtu_ctrl_tbl_u));
	aos_mem_zero(&port_eg_vlan, sizeof(union port_eg_vlan_u));

	if (ADPT_IS_PPORT(port_id)) {
		rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_value, &mru_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);
		rv = hppe_mc_mtu_ctrl_tbl_get(dev_id, port_value, &mc_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);

		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_port_eg_vlan_tx_counting_en_get(dev_id, port_value, &cnt_cfg->uc_tx_cnt_en);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
			rv = hppe_port_eg_vlan_get(dev_id, port_value, &port_eg_vlan);
			SW_RTN_ON_ERROR(rv);

			/*when it's physical port,mru_mtu_ctrl.tx_cnt_en enabled with port_eg_vlan.tx_counting_en in the same time*/
			cnt_cfg->uc_tx_cnt_en = port_eg_vlan.bf.tx_counting_en;
		}

		cnt_cfg->rx_cnt_en = mru_mtu_ctrl_tbl.bf.rx_cnt_en;
		cnt_cfg->mc_tx_cnt_en = mc_mtu_ctrl_tbl.bf.tx_cnt_en;

	}
	else
	{
		rv = hppe_mru_mtu_ctrl_tbl_get(dev_id, port_value, &mru_mtu_ctrl_tbl);
		SW_RTN_ON_ERROR(rv);

		cnt_cfg->rx_cnt_en = mru_mtu_ctrl_tbl.bf.rx_cnt_en;
		cnt_cfg->uc_tx_cnt_en = mru_mtu_ctrl_tbl.bf.tx_cnt_en;
	}

#if defined(IN_TUNNEL)
	aos_mem_zero(&tl_port_vp_tbl, sizeof(union tl_port_vp_tbl_u));
	rv = appe_tl_port_vp_tbl_get(dev_id, port_value, &tl_port_vp_tbl);
	SW_RTN_ON_ERROR(rv);

	cnt_cfg->tl_rx_cnt_en = tl_port_vp_tbl.bf.rx_cnt_en;
#endif

	return SW_OK;
}

sw_error_t
adpt_ppe_port_cnt_cfg_set(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cnt_cfg);

	if (!ADPT_IS_PPORT(port_id) && !ADPT_IS_VPORT(port_id))
	{
		return SW_OUT_OF_RANGE;
	}

	/* set counter enable configs */
	rv = _adpt_hppe_port_cnt_enable_set(dev_id, port_id, cnt_cfg);
	SW_RTN_ON_ERROR(rv);

	/* set counter mode configs */
	rv = adpt_appe_port_cnt_mode_set(dev_id, port_id, cnt_cfg);
	SW_RTN_ON_ERROR(rv);

	return rv;
}

sw_error_t
adpt_ppe_port_cnt_cfg_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_cfg_t *cnt_cfg)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cnt_cfg);

	if (!ADPT_IS_PPORT(port_id) && !ADPT_IS_VPORT(port_id))
	{
		return SW_OUT_OF_RANGE;
	}

	/* get counter enable configs */
	rv = _adpt_hppe_port_cnt_enable_get(dev_id, port_id, cnt_cfg);
	SW_RTN_ON_ERROR(rv);

	/* get counter mode configs */
	rv = adpt_appe_port_cnt_mode_get(dev_id, port_id, cnt_cfg);
	SW_RTN_ON_ERROR(rv);

	return rv;
}

#if defined(IN_QM)
sw_error_t
_adpt_hppe_pport_tx_cnt_update(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_t *port_cnt,
										a_bool_t is_get)
{
	sw_error_t rv = SW_OK;
	a_uint32_t q_idx = 0, i = 0;
	ssdk_dt_scheduler_cfg *dt_cfg = NULL;
	ssdk_dt_portscheduler_cfg *ptscheduler = NULL;
	adpt_api_t *p_adpt_api = NULL;
	fal_queue_stats_t queue_stats;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id))
		return SW_OUT_OF_RANGE;

	/*
	 * update port tx counter by getting/resetting corresponding queue counter.
	 */
	dt_cfg = ssdk_bootup_shceduler_cfg_get(dev_id);
	ADPT_NULL_POINT_CHECK(dt_cfg);

	ptscheduler = &dt_cfg->pool[FAL_PORT_ID_VALUE(port_id)];
	if (ptscheduler->ucastq_end == 0 || ptscheduler->mcastq_end == 0)
		return rv;

	p_adpt_api = adpt_api_ptr_get(dev_id);
	ADPT_NULL_POINT_CHECK(p_adpt_api);
	ADPT_NULL_POINT_CHECK(p_adpt_api->adpt_queue_counter_get);
	ADPT_NULL_POINT_CHECK(p_adpt_api->adpt_queue_counter_cleanup);

	for (q_idx = ptscheduler->ucastq_start;
		q_idx <= ptscheduler->mcastq_end;
		(q_idx == ptscheduler->ucastq_end) ? (q_idx = ptscheduler->mcastq_start) : (q_idx++)) {
		aos_mem_zero(&queue_stats, sizeof(fal_queue_stats_t));

		if (is_get) {
			ADPT_NULL_POINT_CHECK(port_cnt);
			rv = p_adpt_api->adpt_queue_counter_get(dev_id, q_idx, &queue_stats);
			SW_RTN_ON_ERROR(rv);

			/* adding queue drop counter which indipendent with PORT_TX_DROP_CNT_TBL */
			for (i = 0; i < FAL_QM_DROP_ITEMS; i++) {
				port_cnt->tx_drop_pkt_cnt += queue_stats.drop_packets[i];
				port_cnt->tx_drop_byte_cnt += queue_stats.drop_bytes[i];
			}
		} else {
			/* reset port releated queue counter */
			rv = p_adpt_api->adpt_queue_counter_cleanup(dev_id, q_idx);
			SW_RTN_ON_ERROR(rv);
		}
	}

	return rv;
}
#endif

sw_error_t
_adpt_hppe_port_tx_cnt_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_t *port_cnt)
{
	sw_error_t rv = SW_OK;
	union port_tx_counter_tbl_reg_u phy_port_tx_cnt_tbl;
	union port_tx_drop_cnt_tbl_u phy_port_tx_drop_cnt;
	union vp_tx_counter_tbl_reg_u vport_tx_cnt_tbl;
	union vp_tx_drop_cnt_tbl_u vport_tx_drop_cnt;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_cnt);

	aos_mem_zero(&phy_port_tx_cnt_tbl, sizeof(union port_tx_counter_tbl_reg_u));
	aos_mem_zero(&phy_port_tx_drop_cnt, sizeof(union port_tx_drop_cnt_tbl_u));
	aos_mem_zero(&vport_tx_cnt_tbl, sizeof(union vp_tx_counter_tbl_reg_u));
	aos_mem_zero(&vport_tx_drop_cnt, sizeof(union vp_tx_drop_cnt_tbl_u));

	if(ADPT_IS_PPORT(port_id))
	{
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_port_tx_counter_tbl_get(dev_id, port_value, port_cnt);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
			rv = hppe_port_tx_counter_tbl_reg_get(dev_id, port_value, &phy_port_tx_cnt_tbl);
			SW_RTN_ON_ERROR(rv);
			port_cnt->tx_pkt_cnt = phy_port_tx_cnt_tbl.bf.tx_packets;
			port_cnt->tx_byte_cnt = ((a_uint64_t)phy_port_tx_cnt_tbl.bf.tx_bytes_1 <<
				SW_FIELD_OFFSET_IN_WORD(PORT_TX_COUNTER_TBL_REG_TX_BYTES_OFFSET)) |
				phy_port_tx_cnt_tbl.bf.tx_bytes_0;
		}

		rv = hppe_port_tx_drop_cnt_tbl_get(dev_id, port_value, &phy_port_tx_drop_cnt);
		SW_RTN_ON_ERROR(rv);
		port_cnt->tx_drop_pkt_cnt = phy_port_tx_drop_cnt.bf.tx_drop_pkt_cnt;
		port_cnt->tx_drop_byte_cnt = ((a_uint64_t)phy_port_tx_drop_cnt.bf.tx_drop_byte_cnt_1 <<
			SW_FIELD_OFFSET_IN_WORD(PORT_TX_DROP_CNT_TBL_TX_DROP_BYTE_CNT_OFFSET)) |
			phy_port_tx_drop_cnt.bf.tx_drop_byte_cnt_0;
#if defined(IN_QM)
		rv = _adpt_hppe_pport_tx_cnt_update(dev_id, port_id, port_cnt, A_TRUE);
		SW_RTN_ON_ERROR(rv);
#endif
	}
	else
	{
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_vp_tx_counter_tbl_get(dev_id, port_value, port_cnt);
			SW_RTN_ON_ERROR(rv);
#endif
		} else {
			rv = hppe_vp_tx_counter_tbl_reg_get(dev_id, port_value, &vport_tx_cnt_tbl);
			SW_RTN_ON_ERROR(rv);
			port_cnt->tx_pkt_cnt = vport_tx_cnt_tbl.bf.tx_packets;
			port_cnt->tx_byte_cnt = ((a_uint64_t)vport_tx_cnt_tbl.bf.tx_bytes_1 <<
				SW_FIELD_OFFSET_IN_WORD(VP_TX_COUNTER_TBL_REG_TX_BYTES_OFFSET)) |
				vport_tx_cnt_tbl.bf.tx_bytes_0;
		}

		rv = hppe_vp_tx_drop_cnt_tbl_get(dev_id, port_value, &vport_tx_drop_cnt);
		SW_RTN_ON_ERROR(rv);
		port_cnt->tx_drop_pkt_cnt = vport_tx_drop_cnt.bf.tx_drop_pkt_cnt;
		port_cnt->tx_drop_byte_cnt = ((a_uint64_t)vport_tx_drop_cnt.bf.tx_drop_byte_cnt_1 <<
			SW_FIELD_OFFSET_IN_WORD(VP_TX_DROP_CNT_TBL_TX_DROP_BYTE_CNT_OFFSET)) |
			vport_tx_drop_cnt.bf.tx_drop_byte_cnt_0;
	}

	return rv;
}

sw_error_t
_adpt_hppe_port_tx_cnt_flush(a_uint32_t dev_id, fal_port_t port_id)
{
	sw_error_t rv = SW_OK;
	union port_tx_counter_tbl_reg_u phy_port_tx_cnt_tbl;
	union port_tx_drop_cnt_tbl_u phy_port_tx_drop_cnt;
	union vp_tx_counter_tbl_reg_u vport_tx_cnt_tbl;
	union vp_tx_drop_cnt_tbl_u vport_tx_drop_cnt;
	a_uint32_t port_value = FAL_PORT_ID_VALUE(port_id);

	ADPT_DEV_ID_CHECK(dev_id);

	aos_mem_zero(&phy_port_tx_cnt_tbl, sizeof(union port_tx_counter_tbl_reg_u));
	aos_mem_zero(&phy_port_tx_drop_cnt, sizeof(union port_tx_drop_cnt_tbl_u));
	aos_mem_zero(&vport_tx_cnt_tbl, sizeof(union vp_tx_counter_tbl_reg_u));
	aos_mem_zero(&vport_tx_drop_cnt, sizeof(union vp_tx_drop_cnt_tbl_u));

	if(ADPT_IS_PPORT(port_id))
	{
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_port_tx_counter_tbl_flush(dev_id, port_value);
#endif
		} else {
			rv = hppe_port_tx_counter_tbl_reg_set(dev_id, port_value, &phy_port_tx_cnt_tbl);
		}
		SW_RTN_ON_ERROR(rv);

		rv = hppe_port_tx_drop_cnt_tbl_set(dev_id, port_value, &phy_port_tx_drop_cnt);
		SW_RTN_ON_ERROR(rv);
#if defined(IN_QM)
		rv = _adpt_hppe_pport_tx_cnt_update(dev_id, port_id, NULL, A_FALSE);
		SW_RTN_ON_ERROR(rv);
#endif
	}
	else
	{
		if (adpt_chip_type_get(dev_id) == CHIP_HTTPPE) {
#if defined(HTTPPE)
			rv = adpt_httppe_vp_tx_counter_tbl_flush(dev_id, port_value);
#endif
		} else {
			rv = hppe_vp_tx_counter_tbl_reg_set(dev_id, port_value, &vport_tx_cnt_tbl);
		}
		SW_RTN_ON_ERROR(rv);

		rv = hppe_vp_tx_drop_cnt_tbl_set(dev_id, port_value, &vport_tx_drop_cnt);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

sw_error_t
adpt_ppe_port_cnt_get(a_uint32_t dev_id, fal_port_t port_id, fal_port_cnt_t *port_cnt)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_cnt);

	if (!ADPT_IS_PPORT(port_id) && !ADPT_IS_VPORT(port_id))
	{
		return SW_OUT_OF_RANGE;
	}
	rv = adpt_appe_port_rx_cnt_get(dev_id, port_id, port_cnt);
	SW_RTN_ON_ERROR(rv);

	rv = _adpt_hppe_port_tx_cnt_get(dev_id, port_id, port_cnt);
	SW_RTN_ON_ERROR(rv);

	return rv;
}

sw_error_t
adpt_ppe_port_cnt_flush(a_uint32_t dev_id, fal_port_t port_id)
{
	sw_error_t rv = SW_OK;

	ADPT_DEV_ID_CHECK(dev_id);

	if (!ADPT_IS_PPORT(port_id) && !ADPT_IS_VPORT(port_id))
	{
		return SW_OUT_OF_RANGE;
	}
	rv = adpt_appe_port_rx_cnt_flush(dev_id, port_id);
	SW_RTN_ON_ERROR(rv);

	rv = _adpt_hppe_port_tx_cnt_flush(dev_id, port_id);
	SW_RTN_ON_ERROR(rv);

	return rv;
}

#ifndef IN_PORTCONTROL_MINI
sw_error_t adpt_hppe_port_rx_buff_thresh_get(a_uint32_t dev_id,
		a_uint32_t port, a_uint16_t *thresh)
{
	sw_error_t rv = SW_OK;
	union rx_fifo_cfg_u rx_thresh;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(thresh);

	memset(&rx_thresh, 0, sizeof(union rx_fifo_cfg_u));

	rv = hppe_rx_fifo_cfg_get(dev_id, port, &rx_thresh);
	SW_RTN_ON_ERROR(rv);

	*thresh = rx_thresh.bf.rx_fifo_thres;
	return rv;
}
#endif

sw_error_t adpt_hppe_port_rx_buff_thresh_set(a_uint32_t dev_id,
		a_uint32_t port, a_uint16_t thresh)
{
	sw_error_t rv = SW_OK;
	union rx_fifo_cfg_u rx_thresh;

	ADPT_DEV_ID_CHECK(dev_id);

	memset(&rx_thresh, 0, sizeof(union rx_fifo_cfg_u));

	rv = hppe_rx_fifo_cfg_get(dev_id, port, &rx_thresh);
	SW_RTN_ON_ERROR(rv);

	rx_thresh.bf.rx_fifo_thres = thresh;

	rv = hppe_rx_fifo_cfg_set(dev_id, port, &rx_thresh);
	return rv;
}

#if defined(JHPPE)
sw_error_t adpt_jhppe_port_fec_set(a_uint32_t dev_id,
		a_uint32_t port_id, fal_port_fec_config_t *ptfec)
{
	fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;
	struct ssdk_port_priv *port_priv;
	struct qca_phy_priv *priv;
	a_uint32_t uniphy_index;
	sw_error_t rv;

	ADPT_NULL_POINT_CHECK(ptfec);

	if (port_id >= SW_MAX_NR_PORT)
		return SW_BAD_PARAM;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR(rv);

	if (mode != PORT_25GBASE_R)
		return SW_NOT_SUPPORTED;

	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	port_priv = &priv->ports[port_id];
	port_priv->configured_fec = ptfec->configured_fec;

	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
	return adpt_jhppe_uniphy_fec_set(dev_id, uniphy_index, ptfec->configured_fec);
}

sw_error_t adpt_jhppe_port_fec_get(a_uint32_t dev_id,
		a_uint32_t port_id, fal_port_fec_config_t *ptfec)
{
	fal_port_interface_mode_t mode = PORT_INTERFACE_MODE_MAX;
	struct ssdk_port_priv *port_priv;
	struct qca_phy_priv *priv;
	a_uint32_t uniphy_index;
	a_bool_t status;
	sw_error_t rv;

	ADPT_NULL_POINT_CHECK(ptfec);

	if (port_id >= SW_MAX_NR_PORT)
		return SW_BAD_PARAM;

	rv = adpt_hppe_port_interface_mode_get(dev_id, port_id, &mode);
	SW_RTN_ON_ERROR(rv);

	if (mode != PORT_25GBASE_R)
		return SW_NOT_SUPPORTED;

	/* The three FEC modes are supported for 25GBASE-R */
	ptfec->supported_fec = ETHTOOL_FEC_OFF | ETHTOOL_FEC_RS | ETHTOOL_FEC_BASER;

	/* Get configured FEC */
	priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	port_priv = &priv->ports[port_id];
	ptfec->configured_fec = port_priv->configured_fec;

	/* Get active FEC */
	uniphy_index = hsl_port_to_uniphy(dev_id, port_id);
	rv = adpt_jhppe_uniphy_25gr_status_check(dev_id, uniphy_index, &status);
	SW_RTN_ON_ERROR(rv);

	/* Configured FEC becomes active when 25gr link up */
	ptfec->active_fec = (status == A_TRUE) ? ptfec->configured_fec : ETHTOOL_FEC_NONE;

	return SW_OK;
}
#endif

sw_error_t adpt_hppe_port_ctrl_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

#ifndef IN_PORTCONTROL_MINI
	p_adpt_api->adpt_port_rxmac_status_get = adpt_hppe_port_rxmac_status_get;
#endif
	p_adpt_api->adpt_port_txmac_status_set = adpt_hppe_port_txmac_status_set;
	p_adpt_api->adpt_port_rxfc_status_get = adpt_hppe_port_rxfc_status_get;
	p_adpt_api->adpt_port_txfc_status_get = adpt_hppe_port_txfc_status_get;
	p_adpt_api->adpt_port_flowctrl_set = adpt_hppe_port_flowctrl_set;
	p_adpt_api->adpt_port_mru_set = adpt_ppe_port_mru_set;
#ifndef IN_PORTCONTROL_MINI
	p_adpt_api->adpt_port_txmac_status_get = adpt_hppe_port_txmac_status_get;
	p_adpt_api->adpt_ports_link_status_get = adpt_hppe_ports_link_status_get;
	p_adpt_api->adpt_port_mac_loopback_set = adpt_hppe_port_mac_loopback_set;
#endif
	p_adpt_api->adpt_port_mru_get = adpt_ppe_port_mru_get;
	p_adpt_api->adpt_port_interface_mode_get = adpt_hppe_port_interface_mode_get;
	p_adpt_api->adpt_port_duplex_get = adpt_hppe_port_duplex_get;
	p_adpt_api->adpt_port_mtu_set = adpt_ppe_port_mtu_set;
	p_adpt_api->adpt_port_link_status_get = adpt_hppe_port_link_status_get;
	p_adpt_api->adpt_port_combo_prefer_medium_get =
		adpt_hppe_port_combo_prefer_medium_get;
	p_adpt_api->adpt_port_combo_prefer_medium_set =
		adpt_hppe_port_combo_prefer_medium_set;
	p_adpt_api->adpt_port_txfc_status_set = adpt_hppe_port_txfc_status_set;
	p_adpt_api->adpt_port_flowctrl_get = adpt_hppe_port_flowctrl_get;
	p_adpt_api->adpt_port_rxmac_status_set = adpt_hppe_port_rxmac_status_set;
	p_adpt_api->adpt_port_interface_mode_set = adpt_hppe_port_interface_mode_set;
#ifndef IN_PORTCONTROL_MINI
	p_adpt_api->adpt_port_mac_loopback_get = adpt_hppe_port_mac_loopback_get;
#endif
	p_adpt_api->adpt_port_mtu_get = adpt_ppe_port_mtu_get;
	p_adpt_api->adpt_port_interface_mode_status_get =
		adpt_hppe_port_interface_mode_status_get;
	p_adpt_api->adpt_port_rxfc_status_set = adpt_hppe_port_rxfc_status_set;
	p_adpt_api->adpt_port_speed_get = adpt_hppe_port_speed_get;
	p_adpt_api->adpt_port_max_frame_size_set = adpt_ppe_port_max_frame_size_set;
	p_adpt_api->adpt_port_max_frame_size_get = adpt_ppe_port_max_frame_size_get;
	p_adpt_api->adpt_port_source_filter_get = adpt_ppe_port_source_filter_get;
	p_adpt_api->adpt_port_source_filter_set = adpt_ppe_port_source_filter_set;
	p_adpt_api->adpt_port_interface_mode_apply = adpt_hppe_port_interface_mode_apply;
	p_adpt_api->adpt_port_promisc_mode_set = adpt_hppe_port_promisc_mode_set;
	p_adpt_api->adpt_port_promisc_mode_get = adpt_hppe_port_promisc_mode_get;
	p_adpt_api->adpt_port_flowctrl_forcemode_set =
		adpt_hppe_port_flowctrl_forcemode_set;
	p_adpt_api->adpt_port_flowctrl_forcemode_get =
		adpt_hppe_port_flowctrl_forcemode_get;
	p_adpt_api->adpt_port_source_filter_config_get = adpt_ppe_port_source_filter_config_get;
	p_adpt_api->adpt_port_source_filter_config_set = adpt_ppe_port_source_filter_config_set;
#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE) {
		p_adpt_api->adpt_port_mux_mac_type_set = adpt_httppe_port_mux_mac_type_set;
		p_adpt_api->adpt_port_mtu_cfg_set = adpt_httppe_port_mtu_cfg_set;
		p_adpt_api->adpt_port_mtu_cfg_get = adpt_httppe_port_mtu_cfg_get;
	}
	else
#endif
	{
		p_adpt_api->adpt_port_mux_mac_type_set = adpt_hppe_port_mux_mac_type_set;
		p_adpt_api->adpt_port_mtu_cfg_set = adpt_appe_port_mtu_cfg_set;
		p_adpt_api->adpt_port_mtu_cfg_get = adpt_appe_port_mtu_cfg_get;
	}
	p_adpt_api->adpt_port_mac_speed_set = adpt_hppe_port_mac_speed_set;
	p_adpt_api->adpt_port_mac_duplex_set = adpt_hppe_port_mac_duplex_set;
	p_adpt_api->adpt_port_polling_sw_sync_set = qca_hppe_mac_sw_sync_task;
	p_adpt_api->adpt_port_bridge_txmac_set = adpt_hppe_port_bridge_txmac_set;
	p_adpt_api->adpt_port_interface_eee_cfg_set = adpt_ppe_port_interface_eee_cfg_set;
	p_adpt_api->adpt_port_interface_eee_cfg_get = adpt_ppe_port_interface_eee_cfg_get;
	p_adpt_api->adpt_port_phy_status_get = adpt_hppe_port_phy_status_get;
#ifndef IN_PORTCONTROL_MINI
		p_adpt_api->adpt_port_8023ah_set = adpt_appe_port_8023ah_set;
		p_adpt_api->adpt_port_8023ah_get = adpt_appe_port_8023ah_get;
#endif
	p_adpt_api->adpt_port_tx_buff_thresh_set = adpt_appe_port_tx_buff_thresh_set;
	p_adpt_api->adpt_port_tx_buff_thresh_get = adpt_appe_port_tx_buff_thresh_get;
	p_adpt_api->adpt_port_erp_power_mode_set = adpt_appe_port_erp_power_mode_set;
	p_adpt_api->adpt_port_cnt_cfg_set = adpt_ppe_port_cnt_cfg_set;
	p_adpt_api->adpt_port_cnt_cfg_get = adpt_ppe_port_cnt_cfg_get;
	p_adpt_api->adpt_port_cnt_get = adpt_ppe_port_cnt_get;
	p_adpt_api->adpt_port_cnt_flush = adpt_ppe_port_cnt_flush;
	p_adpt_api->adpt_port_mru_mtu_set = adpt_ppe_port_mru_mtu_set;
	p_adpt_api->adpt_port_mru_mtu_get = adpt_ppe_port_mru_mtu_get;
	p_adpt_api->adpt_port_rx_buff_thresh_set =
		adpt_hppe_port_rx_buff_thresh_set;
#ifndef IN_PORTCONTROL_MINI
	p_adpt_api->adpt_port_rx_buff_thresh_get =
		adpt_hppe_port_rx_buff_thresh_get;
#endif
#if defined(JHPPE)
	p_adpt_api->adpt_switch_port_loopback_set = adpt_jhppe_switch_port_loopback_set;
	p_adpt_api->adpt_switch_port_loopback_get = adpt_jhppe_switch_port_loopback_get;
	p_adpt_api->adpt_switch_loopback_port_get = adpt_jhppe_switch_loopback_port_get;
	p_adpt_api->adpt_port_pps_ctrl_set = adpt_jhppe_port_pps_ctrl_set;
	p_adpt_api->adpt_port_pps_ctrl_get = adpt_jhppe_port_pps_ctrl_get;
	p_adpt_api->adpt_port_fec_set = adpt_jhppe_port_fec_set;
	p_adpt_api->adpt_port_fec_get = adpt_jhppe_port_fec_get;
#endif
	return SW_OK;
}

/**
 * @}
 */
