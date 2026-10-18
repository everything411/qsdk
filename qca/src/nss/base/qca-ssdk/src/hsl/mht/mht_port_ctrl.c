/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup mht_port_ctrl MHT_PORT_CONTROL
 * @{
 */
#include "sw.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "mht_port_ctrl.h"
#include "mht_reg.h"
#include "isisc_reg.h"
#include "isisc_port_ctrl.h"
#include "hsl_phy.h"
#include "ssdk_plat.h"
#include "mht_port_ctrl.h"
#include "ssdk_mht.h"
#include "ssdk_dts.h"
#include "ssdk_interrupt.h"
#include "mht_sec_ctrl.h"
#include "qca-nss-phy/qcom_phy_lib.h"
#include "qca-nss-phy/qca8k_clk.h"

#ifndef IN_PORTCONTROL_MINI
#define PORT0_MAX_VIRT_RING	8
#define PORT5_MAX_VIRT_RING	16

/*
PORT0 egress 6 queues
PORT1~4 egress 4 queues
PORT5 egress 6 queues
*/
static a_uint32_t port_queue[6] = { 6, 4, 4, 4, 4, 6 };

static sw_error_t
_mht_port_congestion_drop_set (a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t queue_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t val = 0, offset = 0, field = 0;

	HSL_DEV_ID_CHECK (dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	if (queue_id >= port_queue[port_id])
	{
		return SW_BAD_PARAM;
	}

	if (port_id != 0)
		offset = port_id * 4 + 2;
	offset += queue_id;

	if (A_TRUE == enable)
	{
		field = 1 << offset;
	}
	else if (A_FALSE == enable)
	{
		field = ~(1 << offset);
	}
	else
	{
		return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET (rv, dev_id, FLOW_CONGE_DROP_CTRL0, 0,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	if (A_TRUE == enable)
	{
		val = val | field;
	}
	else
	{
		val = val & field;
	}

	HSL_REG_ENTRY_SET (rv, dev_id, FLOW_CONGE_DROP_CTRL0, 0,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	return rv;
}

static sw_error_t
_mht_port_congestion_drop_get (a_uint32_t dev_id, fal_port_t port_id,
		a_uint32_t queue_id, a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	a_uint32_t val, offset = 0;

	HSL_DEV_ID_CHECK (dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	if (queue_id >= port_queue[port_id])
	{
		return SW_BAD_PARAM;
	}

	if (port_id != 0)
		offset = port_id * 4 + 2;
	offset += queue_id;

	HSL_REG_ENTRY_GET (rv, dev_id, FLOW_CONGE_DROP_CTRL0, 0,
			(a_uint8_t *) (&val), sizeof (a_uint32_t));
	val = (val >> offset) & 0x1;
	if (val == 0)
	{
		*enable = A_FALSE;
	}
	else if (val == 1)
	{
		*enable = A_TRUE;
	}
	return rv;
}

static sw_error_t
_mht_ring_flow_ctrl_thres_set (a_uint32_t dev_id, a_uint32_t ring_id,
		a_uint16_t on_thres, a_uint16_t off_thres)
{
	sw_error_t rv;
	a_uint32_t val = 0;

	HSL_DEV_ID_CHECK (dev_id);

	if (ring_id >= PORT5_MAX_VIRT_RING)
	{
		return SW_BAD_PARAM;
	}

	if (on_thres > off_thres || on_thres == 0)
	{
		return SW_BAD_PARAM;
	}

	if (ring_id < PORT0_MAX_VIRT_RING) {
		SW_SET_REG_BY_FIELD (RING_FLOW_CTRL_THRES, XON, on_thres, val);
		SW_SET_REG_BY_FIELD (RING_FLOW_CTRL_THRES, XOFF, off_thres, val);
		HSL_REG_ENTRY_SET (rv, dev_id, RING_FLOW_CTRL_THRES, ring_id,
				(a_uint8_t *) (&val), sizeof (a_uint32_t));
	} else {
		SW_SET_REG_BY_FIELD (PORT5_RING_FLOW_CTRL_THRES, XON, on_thres, val);
		SW_SET_REG_BY_FIELD (PORT5_RING_FLOW_CTRL_THRES, XOFF, off_thres, val);
		HSL_REG_ENTRY_SET (rv, dev_id, PORT5_RING_FLOW_CTRL_THRES,
				(ring_id-PORT0_MAX_VIRT_RING),
				(a_uint8_t *) (&val), sizeof (a_uint32_t));
	}

	return rv;
}

static sw_error_t
_mht_ring_flow_ctrl_thres_get (a_uint32_t dev_id, a_uint32_t ring_id,
		a_uint16_t * on_thres, a_uint16_t * off_thres)
{

	sw_error_t rv;
	a_uint32_t val = 0;
	a_uint16_t hthres, lthres;

	HSL_DEV_ID_CHECK (dev_id);

	if (ring_id >= PORT5_MAX_VIRT_RING)
	{
		return SW_BAD_PARAM;
	}

	if (ring_id < PORT0_MAX_VIRT_RING) {
		HSL_REG_ENTRY_GET (rv, dev_id, RING_FLOW_CTRL_THRES, ring_id,
				(a_uint8_t *) (&val), sizeof (a_uint32_t));

		SW_GET_FIELD_BY_REG (RING_FLOW_CTRL_THRES, XON, hthres, val);
		SW_GET_FIELD_BY_REG (RING_FLOW_CTRL_THRES, XOFF, lthres, val);
	} else {
		HSL_REG_ENTRY_GET (rv, dev_id, PORT5_RING_FLOW_CTRL_THRES,
				(ring_id-PORT0_MAX_VIRT_RING),
				(a_uint8_t *) (&val), sizeof (a_uint32_t));

		SW_GET_FIELD_BY_REG (PORT5_RING_FLOW_CTRL_THRES, XON, hthres, val);
		SW_GET_FIELD_BY_REG (PORT5_RING_FLOW_CTRL_THRES, XOFF, lthres, val);
	}

	*on_thres = hthres;
	*off_thres = lthres;

	return rv;
}

#define MHT_RX_DMA_RING_PAUSE_DEBUG_ADDR	0x28
static sw_error_t
_mht_ring_flow_ctrl_status_get(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t *status)
{

	sw_error_t rv = SW_OK;
	a_uint32_t val = 0;

	HSL_DEV_ID_CHECK (dev_id);
	if (ring_id >= PORT5_MAX_VIRT_RING) {
		return SW_BAD_PARAM;
	}

	val = MHT_RX_DMA_RING_PAUSE_DEBUG_ADDR;
	HSL_REG_ENTRY_SET(rv, dev_id, SWITCH_QM_DEBUG_ADDR, 0,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	HSL_REG_ENTRY_GET(rv, dev_id, SWITCH_QM_DEBUG_DATA, 0,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	*status = (val >> ring_id) & 1;

	return rv;
}

static sw_error_t
_mht_ring_union_set(a_uint32_t dev_id, a_bool_t enable)
{
	sw_error_t rv;
	a_uint32_t val;

	HSL_DEV_ID_CHECK(dev_id);

	val = enable ? 1 : 0;
	HSL_REG_FIELD_SET(rv, dev_id, PORT0_PORT5_RING_UNION, 0, EN,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	return rv;
}

static sw_error_t
_mht_ring_union_get(a_uint32_t dev_id, a_bool_t *enable)
{
	sw_error_t rv;
	a_uint32_t val;

	HSL_DEV_ID_CHECK(dev_id);

	HSL_REG_FIELD_GET(rv, dev_id, PORT0_PORT5_RING_UNION, 0, EN,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	*enable = val ? A_TRUE : A_FALSE;

	return rv;
}
#endif
static sw_error_t
_mht_port_flowctrl_thresh_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint16_t *on, a_uint16_t *off)
{
	sw_error_t rv;
	a_uint32_t val = 0;
	a_uint16_t hthres, lthres;

	HSL_DEV_ID_CHECK(dev_id);

	HSL_REG_ENTRY_GET(rv, dev_id, PORT_FLOC_CTRL_THRESH, port_id,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	SW_GET_FIELD_BY_REG(PORT_FLOC_CTRL_THRESH, XON, hthres, val);
	SW_GET_FIELD_BY_REG(PORT_FLOC_CTRL_THRESH, XOFF, lthres, val);

	*on = hthres;
	*off = lthres;

	return rv;
}

static sw_error_t
_mht_port_flowctrl_thresh_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint16_t on, a_uint16_t off)
{
	sw_error_t rv;
	a_uint32_t val = 0;

	HSL_DEV_ID_CHECK(dev_id);

	SW_SET_REG_BY_FIELD(PORT_FLOC_CTRL_THRESH, XON, on, val);
	SW_SET_REG_BY_FIELD(PORT_FLOC_CTRL_THRESH, XOFF, off, val);
	HSL_REG_ENTRY_SET(rv, dev_id, PORT_FLOC_CTRL_THRESH, port_id,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	return rv;
}
#ifndef IN_PORTCONTROL_MINI
static sw_error_t
_mht_ring_flow_ctrl_config_set(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t status)
{

	sw_error_t rv = SW_OK;
	a_uint32_t val = 0;

	HSL_DEV_ID_CHECK (dev_id);
	if (ring_id >= PORT5_MAX_VIRT_RING) {
		return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET(rv, dev_id, SWITCH_CORE_SWITCH_RING_FC, 0,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	val &= ~BIT(ring_id);
	val |= status << ring_id;

	HSL_REG_ENTRY_SET(rv, dev_id, SWITCH_CORE_SWITCH_RING_FC, 0,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));
	return rv;
}

static sw_error_t
_mht_ring_flow_ctrl_config_get(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t *status)
{

	sw_error_t rv = SW_OK;
	a_uint32_t val = 0;

	HSL_DEV_ID_CHECK (dev_id);
	if (ring_id >= PORT5_MAX_VIRT_RING) {
		return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET(rv, dev_id, SWITCH_CORE_SWITCH_RING_FC, 0,
			(a_uint8_t *)(&val), sizeof(a_uint32_t));

	*status = (val & BIT(ring_id)) ? A_TRUE : A_FALSE;

	return rv;
}
#endif
static sw_error_t
_mht_port_flowctrl_forcemode_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	HSL_DEV_ID_CHECK(dev_id);
	if(A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}
	if (!priv)
		return SW_FAIL;
	if(!hsl_port_phy_connected(dev_id, port_id) && !enable)
		return SW_NOT_SUPPORTED;

	priv->ports[port_id].port_tx_flowctrl_forcemode = enable;
	priv->ports[port_id].port_rx_flowctrl_forcemode = enable;

    return rv;
}

static sw_error_t
__mht_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv;
	a_uint32_t val, reg = 0, tmp;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);

	HSL_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	if (A_TRUE == enable)
	{
	    val = 1;
	}
	else if (A_FALSE == enable)
	{
	    val = 0;
	}
	else
	{
	    return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET(rv, dev_id, PORT_STATUS, port_id,
				(a_uint8_t *) (&reg), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);
	tmp = reg;

	SW_SET_REG_BY_FIELD(PORT_STATUS, TX_FLOW_EN, val, reg);
	SW_SET_REG_BY_FIELD(PORT_STATUS, TX_HALF_FLOW_EN, val, reg);

	if (tmp == reg)
		return SW_OK;
	HSL_REG_ENTRY_SET(rv, dev_id, PORT_STATUS, port_id,
				(a_uint8_t *) (&reg), sizeof (a_uint32_t));

	priv->ports[port_id].port_old_tx_flowctrl = enable;

	return rv;
}

static sw_error_t
_mht_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
	sw_error_t rv;
	a_uint32_t val;

	HSL_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	HSL_REG_FIELD_GET(rv, dev_id, PORT_STATUS, port_id, TX_FLOW_EN,
				(a_uint8_t *) (&val), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	if (val)
	{
	    *enable = A_TRUE;
	}
	else
	{
	    *enable = A_FALSE;
	}

	return SW_OK;
}

static sw_error_t
__mht_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv;
	a_uint32_t val = 0, reg, tmp;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);

	HSL_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	if (A_TRUE == enable)
	{
	    val = 1;
	}
	else if (A_FALSE == enable)
	{
	    val = 0;
	}
	else
	{
	    return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET(rv, dev_id, PORT_STATUS, port_id,
				(a_uint8_t *) (&reg), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);
	tmp = reg;

	SW_SET_REG_BY_FIELD(PORT_STATUS, RX_FLOW_EN, val, reg);

	if ( tmp == reg)
		return SW_OK;
	HSL_REG_ENTRY_SET(rv, dev_id, PORT_STATUS, port_id,
				(a_uint8_t *) (&reg), sizeof (a_uint32_t));

	priv->ports[port_id].port_old_rx_flowctrl = enable;

	return rv;
}

static sw_error_t
_mht_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
	sw_error_t rv;
	a_uint32_t val;

	HSL_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check (dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	HSL_REG_FIELD_GET(rv, dev_id, PORT_STATUS, port_id, RX_FLOW_EN,
				(a_uint8_t *) (&val), sizeof (a_uint32_t));
	SW_RTN_ON_ERROR(rv);

	if (val)
	{
	    *enable = A_TRUE;
	}
	else
	{
	    *enable = A_FALSE;
	}

	return SW_OK;
}

static sw_error_t
_mht_port_mac_speed_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t speed)
{
	sw_error_t rv = 0;
	a_uint32_t reg_val = 0, tmp;
	a_uint32_t speed_val;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET(rv, dev_id, PORT_STATUS, port_id,
			(a_uint8_t *) (&reg_val), sizeof (a_uint32_t));
	tmp = reg_val;

	if (FAL_SPEED_10 == speed) {
		speed_val = MHT_PORT_SPEED_10M;
	} else if (FAL_SPEED_100 == speed) {
		speed_val = MHT_PORT_SPEED_100M;
	} else if (FAL_SPEED_1000 == speed) {
		speed_val = MHT_PORT_SPEED_1000M;
	} else if (FAL_SPEED_2500 == speed) {
		speed_val = MHT_PORT_SPEED_2500M;
	} else {
		SSDK_ERROR("mht port %d invalid speed\n", port_id);
		return SW_BAD_PARAM;
	}
	SW_SET_REG_BY_FIELD(PORT_STATUS, SPEED_MODE, speed_val, reg_val);

	if (tmp == reg_val)
		return SW_OK;

	HSL_REG_ENTRY_SET(rv, dev_id, PORT_STATUS, port_id,
			(a_uint8_t *) (&reg_val), sizeof (a_uint32_t));

	return rv;
}

static sw_error_t
_mht_port_mac_dupex_set(a_uint32_t dev_id, a_uint32_t port_id,
	a_uint32_t duplex)
{
	sw_error_t rv = 0;
	a_uint32_t reg_val = 0, tmp;
	a_uint32_t duplex_val;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	HSL_REG_ENTRY_GET(rv, dev_id, PORT_STATUS, port_id,
			(a_uint8_t *) (&reg_val), sizeof (a_uint32_t));
	tmp = reg_val;

	if (FAL_HALF_DUPLEX == duplex) {
		duplex_val = MHT_PORT_HALF_DUPLEX;
	} else {
		duplex_val = MHT_PORT_FULL_DUPLEX;
	}
	SW_SET_REG_BY_FIELD(PORT_STATUS, DUPLEX_MODE, duplex_val, reg_val);

	if (tmp == reg_val)
		return SW_OK;

	HSL_REG_ENTRY_SET(rv, dev_id, PORT_STATUS, port_id,
			(a_uint8_t *) (&reg_val), sizeof (a_uint32_t));

	return rv;
}

static sw_error_t
_mht_port_duplex_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_duplex_t duplex)
{
	sw_error_t rv = SW_OK;

	HSL_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}

	/* for those ports without PHY device we set MAC register */
	if (A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		rv = _mht_port_mac_dupex_set(dev_id, port_id, duplex);
		SW_RTN_ON_ERROR (rv);
	}
	else
	{
		HSL_PORT_PHY_API_RUN(duplex_set, dev_id, port_id, duplex);
		SW_RTN_ON_ERROR(rv);
	}

	return rv;
}

static sw_error_t
_mht_port_duplex_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_duplex_t * pduplex)
{
	sw_error_t rv = SW_OK;

	HSL_DEV_ID_CHECK (dev_id);

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}
	if (A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		a_uint32_t reg_val = 0, field;

		HSL_REG_ENTRY_GET(rv, dev_id, PORT_STATUS, port_id,
			(a_uint8_t *) (&reg_val), sizeof (a_uint32_t));
		SW_GET_FIELD_BY_REG(PORT_STATUS, DUPLEX_MODE, field, reg_val);
		if (field)
		{
			*pduplex = FAL_FULL_DUPLEX;
		}
		else
		{
			*pduplex = FAL_HALF_DUPLEX;
		}
	}
	else
	{
		HSL_PORT_PHY_API_RUN(duplex_get, dev_id, port_id, pduplex);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

static sw_error_t
_mht_port_speed_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_speed_t speed)
{
	sw_error_t rv = SW_OK;

	HSL_DEV_ID_CHECK (dev_id);

	if(A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}
	/* for those ports without PHY device we set MAC register */
	if (A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		rv = _mht_port_mac_speed_set(dev_id, port_id, speed);
		SW_RTN_ON_ERROR (rv);
	}
	else
	{
		HSL_PORT_PHY_API_RUN(speed_set, dev_id, port_id, speed);
		SW_RTN_ON_ERROR(rv);
	}
	return rv;
}

static sw_error_t
_mht_port_speed_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_speed_t * pspeed)
{
	sw_error_t rv = SW_OK;

	HSL_DEV_ID_CHECK(dev_id);

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}
	if (A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		a_uint32_t reg_val = 0, field;

		HSL_REG_ENTRY_GET(rv, dev_id, PORT_STATUS, port_id,
			(a_uint8_t *) (&reg_val), sizeof (a_uint32_t));
		SW_GET_FIELD_BY_REG(PORT_STATUS, SPEED_MODE, field, reg_val);
		if (0 == field)
		{
			*pspeed = FAL_SPEED_10;
		}
		else if (1 == field)
		{
			*pspeed = FAL_SPEED_100;
		}
		else if (2 == field)
		{
			/* For SGMII Plus mode, speed mode field 2 indicates 2.5G instead of 1G */
			phy_info_t *phy_info = hsl_phy_info_get(dev_id);
			if (phy_info && (phy_info->port_mode[port_id] == PORT_SGMII_PLUS))
				*pspeed = FAL_SPEED_2500;
			else
				*pspeed = FAL_SPEED_1000;
		}
	}
	else
	{
		HSL_PORT_PHY_API_RUN(speed_get, dev_id, port_id, pspeed);
		SW_RTN_ON_ERROR(rv);
	}
	return rv;
}

static sw_error_t
_mht_port_flowctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable)
{
	sw_error_t rv = 0;
	a_bool_t txfc_enable, rxfc_enable;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
	    return SW_BAD_PARAM;
	}

	rv = _mht_port_txfc_status_get(dev_id, port_id, &txfc_enable);
	SW_RTN_ON_ERROR(rv);
	rv = _mht_port_rxfc_status_get(dev_id, port_id, &rxfc_enable);
	SW_RTN_ON_ERROR(rv);

	*enable = txfc_enable & rxfc_enable;

	return rv;
}

static sw_error_t
_mht_port_flowctrl_forcemode_get(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t * enable)
{
	sw_error_t rv = SW_OK;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	HSL_DEV_ID_CHECK(dev_id);
	if(A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
		return SW_BAD_PARAM;
	}
	if (!priv)
		return SW_FAIL;

	*enable = (priv->ports[port_id].port_tx_flowctrl_forcemode &
		priv->ports[port_id].port_rx_flowctrl_forcemode);

	return rv;
}

#ifndef IN_PORTCONTROL_MINI
/**
 * @brief Set flow congestion drop on a particular port queue.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] queue_id queue id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_congestion_drop_set (a_uint32_t dev_id, fal_port_t port_id,
			       a_uint32_t queue_id, a_bool_t enable)
{
	sw_error_t rv;
	HSL_API_LOCK;
	rv = _mht_port_congestion_drop_set (dev_id, port_id, queue_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set flow congestion drop on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] queue_id queue id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_congestion_drop_get (a_uint32_t dev_id, fal_port_t port_id,
			       a_uint32_t queue_id, a_bool_t * enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_congestion_drop_get (dev_id, port_id, queue_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set flow ctrl thres on a particular DMA ring.
 * @param[in] dev_id device id
 * @param[in] ring_id ring id
 * @param[in] on_thres on_thres
 * @param[in] off_thres off_thres
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_flow_ctrl_thres_set (a_uint32_t dev_id, a_uint32_t ring_id,
			       a_uint16_t on_thres, a_uint16_t off_thres)
{
	sw_error_t rv;
	HSL_API_LOCK;
	rv = _mht_ring_flow_ctrl_thres_set (dev_id, ring_id, on_thres, off_thres);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get flow ctrl thres on a particular DMA ring.
 * @param[in] dev_id device id
 * @param[in] ring_id ring id
 * @param[out] on_thres on_thres
 * @param[out] off_thres off_thres
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_flow_ctrl_thres_get (a_uint32_t dev_id, a_uint32_t ring_id,
			       a_uint16_t * on_thres, a_uint16_t * off_thres)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_ring_flow_ctrl_thres_get (dev_id, ring_id, on_thres, off_thres);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get flow ctrl status on a particular DMA ring.
 * @param[in] dev_id device id
 * @param[in] ring_id ring id
 * @param[out] status backpressure status
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_flow_ctrl_status_get(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t *status)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_ring_flow_ctrl_status_get (dev_id, ring_id, status);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set port0 port5 ring union.
 * @param[in] dev_id device id
 * @param[in] enable or not
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_union_set(a_uint32_t dev_id, a_bool_t en)
{
	sw_error_t rv;
	HSL_API_LOCK;
	rv = _mht_ring_union_set(dev_id, en);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get port0 port5 ring union.
 * @param[in] dev_id device id
 * @param[out] enable or not
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_union_get(a_uint32_t dev_id, a_bool_t *en)
{
	sw_error_t rv;
	HSL_API_LOCK;
	rv = _mht_ring_union_get(dev_id, en);
	HSL_API_UNLOCK;
	return rv;
}
#endif
/**
 * @brief Get flow control(rx/tx/bp) threshold on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] on on threshold
 * @param[out] off off threshold
 * @return SW_OK or error code
 */
sw_error_t
mht_port_flowctrl_thresh_get(a_uint32_t dev_id, fal_port_t port_id,
		a_uint16_t *on, a_uint16_t *off)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_flowctrl_thresh_get(dev_id, port_id, on, off);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set flow control(rx/tx/bp) threshold on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] on on threshold
 * @param[in] off off threshold
 * @return SW_OK or error code
 */
sw_error_t
mht_port_flowctrl_thresh_set(a_uint32_t dev_id, fal_port_t port_id,
		a_uint16_t on, a_uint16_t off)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_flowctrl_thresh_set(dev_id, port_id, on, off);
	HSL_API_UNLOCK;
	return rv;
}
#ifndef IN_PORTCONTROL_MINI
/**
 * @brief Get port 0 & port5 ring flow control status.
 * @param[in] dev_id device id
 * @param[in] ring_id ring id
 * @param[in] status enable or not
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_flow_ctrl_config_set(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t status)
{
	sw_error_t rv;
	HSL_API_LOCK;
	rv = _mht_ring_flow_ctrl_config_set(dev_id, ring_id, status);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set port 0 & port5 ring flow control status.
 * @param[in] dev_id device id
 * @param[in] ring_id ring id
 * @param[out] status enable or not
 * @return SW_OK or error code
 */
sw_error_t
mht_ring_flow_ctrl_config_get(a_uint32_t dev_id, a_uint32_t ring_id, a_bool_t *status)
{
	sw_error_t rv;
	HSL_API_LOCK;
	rv = _mht_ring_flow_ctrl_config_get(dev_id, ring_id, status);
	HSL_API_UNLOCK;
	return rv;
}
#endif
/**
 * @brief Set status of tx flow control on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
 sw_error_t
_mht_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_bool_t force_mode = A_FALSE;

	if(A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		rv = __mht_port_txfc_status_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
	}
	else
	{
		/*if force mode is enabled, need to configure mac manually*/
		rv = _mht_port_flowctrl_forcemode_get(dev_id, port_id, &force_mode);
		SW_RTN_ON_ERROR (rv);
		if(force_mode)
		{
			rv = __mht_port_txfc_status_set(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
		}
		rv = hsl_port_phy_txfc_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

sw_error_t
mht_port_txfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_txfc_status_set(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get status of tx flow control on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_txfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_txfc_status_get(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set status of rx flow control on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
 sw_error_t
_mht_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;
	a_bool_t force_mode = A_FALSE;

	if(A_FALSE == hsl_port_phy_connected(dev_id, port_id))
	{
		rv = __mht_port_rxfc_status_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
	}
	else
	{
		/*if force mode is enabled, need to configure mac manually*/
		rv = _mht_port_flowctrl_forcemode_get(dev_id, port_id, &force_mode);
		SW_RTN_ON_ERROR (rv);
		if(force_mode)
		{
			rv = __mht_port_rxfc_status_set(dev_id, port_id, enable);
			SW_RTN_ON_ERROR(rv);
		}
		rv = hsl_port_phy_rxfc_set(dev_id, port_id, enable);
		SW_RTN_ON_ERROR(rv);
	}

	return SW_OK;
}

sw_error_t
_mht_port_flowctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv = SW_OK;

	if (A_TRUE != hsl_port_prop_check(dev_id, port_id, HSL_PP_INCL_CPU))
	{
	    return SW_BAD_PARAM;
	}

	rv = _mht_port_txfc_status_set(dev_id, port_id, enable);
	SW_RTN_ON_ERROR(rv);
	rv = _mht_port_rxfc_status_set(dev_id, port_id, enable);
	SW_RTN_ON_ERROR(rv);

	return SW_OK;
}

sw_error_t
_mht_port_erp_power_mode_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_erp_power_mode_t power_mode)
{
	a_uint32_t i = 0, pbmp = 0;
	phy_info_t *phy_info = NULL;
	struct qcom_phy_pcs_cfg pcs_config = {0};
	struct phy_device *phydev = NULL;
	sw_error_t rv = SW_OK;
	struct mdio_device *clk_dev = NULL;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);
	SW_RTN_ON_NULL(priv);

	clk_dev = ssdk_dt_clk_mdiodev_get(dev_id);
	if (!clk_dev)
		return SW_FAIL;

	switch (power_mode) {
	case FAL_ERP_LOW_POWER:
		if (hsl_port_feature_get(dev_id, port_id, PHY_F_ERP_LOW_POWER)) {
			SSDK_INFO("port %d is already in low power mode\n", port_id);
			return SW_OK;
		}

		/* off phy */
		HSL_PORT_PHY_API_RUN(pll_off, dev_id, port_id);
		HSL_PORT_PHY_API_RUN(power_off, dev_id, port_id);
		hsl_port_feature_set(dev_id, port_id, PHY_F_ERP_LOW_POWER);

		/* off serdes and switch core */
		pbmp = ssdk_wan_bmp_get(dev_id) | ssdk_lan_bmp_get(dev_id);
		while (pbmp) {
			if (pbmp & 1) {
				/* there is active channel port, only powr off current phy */
				if (!hsl_port_feature_get(dev_id, i, PHY_F_ERP_LOW_POWER)) {
					/* off LDO */
					HSL_PORT_PHY_API_RUN(ldo_set, dev_id, port_id, A_FALSE);
					return SW_OK;
				}
			}
			pbmp >>= 1;
			i++;
		}

		/* stop polling task or interrupt */
		if (priv->link_polling_required)
			qm_err_check_work_stop(priv);
		else if (priv->interrupt_no > 0)
			qca_intr_work_pause(priv);

		/* manually excute polling task to finish all ports up to down sequence */
		mutex_lock(&priv->qm_lock);
		qca_mht_sw_mac_polling_task(priv);
		mutex_unlock(&priv->qm_lock);

		SSDK_DEBUG("disable manhattan switch core and serdes1\n");
		/* pause mib task */
		qca_phy_mib_work_pause(priv);

		ssdk_switch_set_standby_status(dev_id, true);

		/* disable switch core */
		SW_RTN_ON_ERROR(qca8k_clk_disable(clk_dev, MHT_SWITCH_CORE_CLK));

		/* switch ahb to xo */
		SW_RTN_ON_ERROR(qca8k_clk_parent_set(clk_dev, MHT_AHB_CLK, MHT_P_XO));
		SW_RTN_ON_ERROR(qca8k_clk_rate_set(clk_dev, MHT_AHB_CLK, MHT_XO_CLK_RATE_50M));
		/* assert serdes1 */
		SW_RTN_ON_ERROR(qca8k_clk_assert(clk_dev, MHT_SRDS1_SYS_CLK));
		/* off LDO */
		HSL_PORT_PHY_API_RUN(ldo_set, dev_id, port_id, A_FALSE);
		break;
	case FAL_ERP_ACTIVE:
		if (!hsl_port_feature_get(dev_id, port_id, PHY_F_ERP_LOW_POWER)) {
			SSDK_INFO("port %d is already in active mode\n", port_id);
			return SW_OK;
		}
		/* on LDO */
		HSL_PORT_PHY_API_RUN(ldo_set, dev_id, port_id, A_TRUE);
		/* resume serdes and switch core */
		if (qca8k_clk_is_asserted(clk_dev, MHT_SRDS1_SYS_CLK)) {
			SSDK_DEBUG("configure manhattan serdes1 and enable switch core\n");
			/* configure serdes1 as sgmii plus mode */
			phy_info = hsl_phy_info_get(dev_id);
			phy_info->port_mode[SSDK_PHYSICAL_PORT0] = PORT_SGMII_PLUS;
			pcs_config.type = PHY_INTERFACE_MODE_2500BASEX;
			pcs_config.clock_mode = CLOCK_MAC_MODE;
			pcs_config.auto_neg = A_FALSE;
			pcs_config.force_speed = FAL_SPEED_2500;
			pcs_config.addr_offset = PCS1_ADDR_OFFSET;
			SW_RTN_ON_ERROR(hsl_port_phydev_get(dev_id, SSDK_PHYSICAL_PORT1,
				&phydev));
			SW_RTN_ON_ERROR(qcom_phy_pcs_interface_set(phydev, pcs_config));

			/* switch ahb back to serdes1 */
			SW_RTN_ON_ERROR(qca8k_clk_parent_set(clk_dev,
					MHT_AHB_CLK, MHT_P_UNIPHY1_TX312P5M));
			SW_RTN_ON_ERROR(qca8k_clk_rate_set(clk_dev,
					MHT_AHB_CLK, MHT_AHB_CLK_RATE_104P17M));

			/* enable switch core */
			SW_RTN_ON_ERROR(qca8k_clk_enable(clk_dev, MHT_SWITCH_CORE_CLK));

			ssdk_switch_set_standby_status(dev_id, false);

			/* resume mib task */
			qca_phy_mib_work_resume(priv);

			/* resume polling task or interrupt */
			if (priv->link_polling_required)
				qm_err_check_work_start(priv);
			else if (priv->interrupt_no > 0)
				qca_intr_work_resume(priv);
		}
		/* on phy */
		HSL_PORT_PHY_API_RUN(pll_on, dev_id, port_id);
		HSL_PORT_PHY_API_RUN(power_on, dev_id, port_id);
		hsl_port_feature_clear(dev_id, port_id, PHY_F_ERP_LOW_POWER);
		break;
	default:
		SSDK_ERROR("not support power mode %d\n", power_mode);
		return SW_NOT_SUPPORTED;
	}
	return SW_OK;
}

sw_error_t
mht_port_rxfc_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_rxfc_status_set(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set status of rx flow control on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_rxfc_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_rxfc_status_get(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set flow control(rx/tx/bp) status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_flowctrl_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_flowctrl_set(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set flow control force mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_flowctrl_forcemode_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_flowctrl_forcemode_set(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set duplex mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] duplex duplex mode
 * @return SW_OK or error code
 */
sw_error_t
mht_port_duplex_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_duplex_t duplex)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_duplex_set(dev_id, port_id, duplex);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Set speed on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[in] speed port speed
 * @return SW_OK or error code
 */
sw_error_t
mht_port_speed_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_speed_t speed)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_speed_set(dev_id, port_id, speed);
	HSL_API_UNLOCK;
	return rv;
}
/**
 * @brief Get duplex mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] duplex duplex mode
 * @return SW_OK or error code
 */
sw_error_t
mht_port_duplex_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_duplex_t * pduplex)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_duplex_get(dev_id, port_id, pduplex);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get speed on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] speed port speed
 * @return SW_OK or error code
 */
sw_error_t
mht_port_speed_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_speed_t * pspeed)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_speed_get(dev_id, port_id, pspeed);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get flow control status on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_flowctrl_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_flowctrl_get(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

/**
 * @brief Get flow control force mode on a particular port.
 * @param[in] dev_id device id
 * @param[in] port_id port id
 * @param[out] enable A_TRUE or A_FALSE
 * @return SW_OK or error code
 */
sw_error_t
mht_port_flowctrl_forcemode_get(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t * enable)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_flowctrl_forcemode_get(dev_id, port_id, enable);
	HSL_API_UNLOCK;
	return rv;
}

sw_error_t
mht_port_erp_power_mode_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_port_erp_power_mode_t power_mode)
{
	sw_error_t rv;

	HSL_API_LOCK;
	rv = _mht_port_erp_power_mode_set(dev_id, port_id, power_mode);
	HSL_API_UNLOCK;
	return rv;
}

static sw_error_t
mht_port_interface_mode_switch(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rv = SW_OK;
	fal_port_interface_mode_t port_mode_old = PORT_INTERFACE_MODE_MAX;
	fal_port_interface_mode_t port_mode_new = PORT_INTERFACE_MODE_MAX;
	phy_info_t *phy_info = hsl_phy_info_get(dev_id);
	fal_mac_config_t mac_config = {0};

	rv = hsl_port_phydev_interface_mode_status_get(dev_id, port_id,
			&port_mode_new);
	SW_RTN_ON_ERROR(rv);

	port_mode_old = phy_info->port_mode[port_id];

	if (port_mode_new != port_mode_old) {
		if (port_mode_new == PORT_SGMII_PLUS) {
			mac_config.mac_mode = FAL_MAC_MODE_SGMII_PLUS;
		} else if (port_mode_new == PHY_SGMII_BASET) {
			mac_config.mac_mode = FAL_MAC_MODE_SGMII;
		} else {
			return SW_NOT_SUPPORTED;
		}
		mac_config.config.sgmii.clock_mode = FAL_INTERFACE_CLOCK_MAC_MODE;
		mac_config.config.sgmii.auto_neg = A_TRUE;
		rv = mht_port_interface_mode_set(dev_id, SSDK_PHYSICAL_PORT5,
			&mac_config);
		SW_RTN_ON_ERROR(rv);
		phy_info->port_mode[port_id] = port_mode_new;

		SSDK_INFO("mht port %d phy changed interface mode to %d from %d\n",
				port_id, port_mode_new, port_mode_old);
	}

	return rv;
}

sw_error_t
mht_port_link_update(struct qca_phy_priv *priv, a_uint32_t port_id,
	struct port_phy_status phy_status)
{
	sw_error_t rv = 0;
	struct phy_device *phydev = NULL;
	struct mdio_device *clk_dev = NULL;

	clk_dev = ssdk_dt_clk_mdiodev_get(priv->device_id);
	if (!clk_dev)
		return SW_FAIL;

	if ((port_id == SSDK_PHYSICAL_PORT5) &&
			(A_TRUE == hsl_port_phy_connected(priv->device_id, port_id))) {
		rv = mht_port_interface_mode_switch(priv->device_id, port_id);
		SW_RTN_ON_ERROR (rv);
	}
	if (phy_status.link_status == PORT_LINK_DOWN) {
		/* configure speed to 1G to avoid qm issue */
		phy_status.speed = FAL_SPEED_1000;
		phy_status.duplex = FAL_FULL_DUPLEX;
	}
	/* configure gcc uniphy and mac speed frequency*/
	rv = hsl_port_phydev_get(priv->device_id, port_id, &phydev);
	SW_RTN_ON_ERROR(rv);
	qcom_phy_pcs_speed_clock_set(phydev, port_id, phy_status.speed);
	/* configure mac speed and duplex */
	rv = _mht_port_mac_speed_set(priv->device_id, port_id, phy_status.speed);
	SW_RTN_ON_ERROR (rv);
	rv = _mht_port_mac_dupex_set(priv->device_id, port_id, phy_status.duplex);
	SW_RTN_ON_ERROR (rv);
	SSDK_DEBUG("mht port %d link %d update speed %d duplex %d\n",
			port_id, phy_status.speed,
			phy_status.speed, phy_status.duplex);
	if (phy_status.link_status == PORT_LINK_UP)
	{
		/* sync mac flowctrl */
		if (priv->ports[port_id].port_tx_flowctrl_forcemode != A_TRUE) {
			rv = __mht_port_txfc_status_set(priv->device_id,
				port_id, phy_status.tx_flowctrl);
			SW_RTN_ON_ERROR (rv);
			SSDK_DEBUG("mht port %d link up update txfc %d\n",
			port_id, phy_status.tx_flowctrl);
		}
		if (priv->ports[port_id].port_tx_flowctrl_forcemode != A_TRUE) {
			rv = __mht_port_rxfc_status_set(priv->device_id,
				port_id, phy_status.rx_flowctrl);
			SW_RTN_ON_ERROR (rv);
			SSDK_DEBUG("mht port %d link up update rxfc %d\n",
			port_id, phy_status.rx_flowctrl);
		}
		if(phy_status.speed != priv->ports[port_id].port_old_speed) {
			mht_port_mac_eee_timer_adjust(priv->device_id, port_id, phy_status.speed, priv);
			priv->ports[port_id].port_old_speed = phy_status.speed;
		}
		if (port_id != SSDK_PHYSICAL_PORT5) {
			/* enable eth phy clock */
			rv = qca8k_port_clk_en_set(clk_dev, port_id,
				MHT_CLK_TYPE_EPHY, A_TRUE);
			SW_RTN_ON_ERROR (rv);
		}
	}
	if (port_id != SSDK_PHYSICAL_PORT5) {
		if (phy_status.link_status == PORT_LINK_DOWN) {
			/* disable eth phy clock */
			rv = qca8k_port_clk_en_set(clk_dev, port_id,
				MHT_CLK_TYPE_EPHY, A_FALSE);
			SW_RTN_ON_ERROR (rv);
		}
		/* reset eth phy clock */
		rv = qca8k_port_clk_reset(clk_dev, port_id, MHT_CLK_TYPE_EPHY);
		SW_RTN_ON_ERROR (rv);
		/* reset eth phy fifo */
		HSL_PORT_PHY_API_RUN(function_reset, priv->device_id, port_id,
			FIFO_RESET);
		SW_RTN_ON_ERROR (rv);
	}

	return rv;
}

sw_error_t
mht_port_interface_mode_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_mac_config_t *config)
{
	sw_error_t rv = SW_OK;
	struct qcom_phy_pcs_cfg pcs_config = {0};
	struct mdio_device *clk_dev = NULL;
	struct phy_device *phydev = NULL;

	rv = hsl_port_phydev_get(dev_id, SSDK_PHYSICAL_PORT1, &phydev);
	SW_RTN_ON_ERROR(rv);

	if(port_id == SSDK_PHYSICAL_PORT0) {
		/*for switch mode, the uniphy1 must be initialized firstly and initialized
		only one time, so configure dvs and acc for memory before uniphy1 initialization*/
		rv = qca_mht_mem_ctrl_set(dev_id, MHT_MEM_CTRL_DVS_SWITCH_MODE,
			MHT_MEM_ACC_0_SWITCH_MODE);
		PHY_RTN_ON_ERROR (rv);
		pcs_config.addr_offset = PCS1_ADDR_OFFSET;
	} else if(port_id == SSDK_PHYSICAL_PORT5) {
		pcs_config.addr_offset = PCS0_ADDR_OFFSET;
		/*if uniphy0 is used as switch bypass, then will do nothing here*/
		if(qcom_phy_pcs_mode_check(phydev, PCS0_ADDR_OFFSET,
			QCOM_PHY_PCS_MMD1_SGMII_PHY_MODE)){
			return SW_OK;
		} else {
			if(config->mac_mode == FAL_MAC_MODE_MAX) {
				clk_dev = ssdk_dt_clk_mdiodev_get(dev_id);
				if (!clk_dev)
					return SW_FAIL;
				/* assert serdes0 to save power */
				qca8k_clk_assert(clk_dev, MHT_SRDS0_SYS_CLK);
				return SW_OK;
			}
		}
	}

	if (config->mac_mode == FAL_MAC_MODE_SGMII)
		pcs_config.type = PHY_INTERFACE_MODE_SGMII;
	else if (config->mac_mode == FAL_MAC_MODE_SGMII_PLUS)
		pcs_config.type = PHY_INTERFACE_MODE_2500BASEX;
	else
		return SW_NOT_SUPPORTED;
	pcs_config.auto_neg = config->config.sgmii.auto_neg;
	pcs_config.force_speed = config->config.sgmii.force_speed;
	pcs_config.clock_mode = CLOCK_MAC_MODE;
	if (qcom_phy_pcs_interface_set(phydev, pcs_config) < 0)
		return SW_FAIL;

	return SW_OK;
}
/**
 * @}
 */
