/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "sw.h"
#include "hsl_htt_reg.h"
#include "adpt.h"

sw_error_t
adpt_httppe_port_mtu_cfg_set(a_uint32_t dev_id, fal_port_t port_id,
	fal_mtu_cfg_t *mtu_cfg)
{
	a_uint32_t port_type = 0, port_value = 0;
	union l2_vp_port_post_tbl_u l2_vp_port_tbl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mtu_cfg);

	port_type = FAL_PORT_ID_TYPE(port_id);
	port_value = FAL_PORT_ID_VALUE(port_id);

	SW_RTN_ON_ERROR(httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_port_tbl));

	l2_vp_port_tbl.bf.mtu_check_type = mtu_cfg->mtu_type;
	l2_vp_port_tbl.bf.extra_header_len = mtu_cfg->extra_header_len;

	l2_vp_port_tbl.bf.eg_vlan_fmt_valid = A_FALSE;
	l2_vp_port_tbl.bf.eg_ctag_fmt = A_FALSE;
	l2_vp_port_tbl.bf.eg_stag_fmt = A_FALSE;
	if(mtu_cfg->eg_vlan_tag_flag != 0)
	{
		l2_vp_port_tbl.bf.eg_vlan_fmt_valid = A_TRUE;
	}
	if(mtu_cfg->eg_vlan_tag_flag & BIT(0))
	{
		l2_vp_port_tbl.bf.eg_ctag_fmt = A_TRUE;
	}
	if(mtu_cfg->eg_vlan_tag_flag & BIT(1))
	{
		l2_vp_port_tbl.bf.eg_stag_fmt = A_TRUE;
	}
	/*physical_port_mtu_check_en is only for VP port*/
	if(port_type == FAL_PORT_TYPE_VPORT)
	{
		if(!mtu_cfg->mtu_enable)
		{
			l2_vp_port_tbl.bf.physical_port_mtu_check_en = A_TRUE;
		}
		else
		{
			l2_vp_port_tbl.bf.physical_port_mtu_check_en = A_FALSE;
		}
	}
	else
	{
		if(!mtu_cfg->mtu_enable)
		{
			SSDK_ERROR("physical port %d does not support mtu disable\n",
				port_id);
			return SW_NOT_SUPPORTED;
		}
	}
	return httppe_l2_vp_port_post_tbl_set(dev_id, port_value, &l2_vp_port_tbl);
}

sw_error_t
adpt_httppe_port_mtu_cfg_get(a_uint32_t dev_id, fal_port_t port_id,
	fal_mtu_cfg_t *mtu_cfg)
{
	a_uint32_t port_type, port_value;
	union l2_vp_port_post_tbl_u l2_vp_port_tbl = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(mtu_cfg);

	port_type = FAL_PORT_ID_TYPE(port_id);
	port_value = FAL_PORT_ID_VALUE(port_id);

	SW_RTN_ON_ERROR(httppe_l2_vp_port_post_tbl_get(dev_id, port_value, &l2_vp_port_tbl));

	mtu_cfg->mtu_type = l2_vp_port_tbl.bf.mtu_check_type;
	mtu_cfg->extra_header_len = l2_vp_port_tbl.bf.extra_header_len;
	mtu_cfg->eg_vlan_tag_flag = 0;
	if(l2_vp_port_tbl.bf.eg_vlan_fmt_valid == A_TRUE)
	{
		if(l2_vp_port_tbl.bf.eg_ctag_fmt)
		{
			mtu_cfg->eg_vlan_tag_flag |= BIT(0);
		}
		if(l2_vp_port_tbl.bf.eg_stag_fmt)
		{
			mtu_cfg->eg_vlan_tag_flag |= BIT(1);
		}
	}
	mtu_cfg->mtu_enable = A_TRUE;
	if(port_type == FAL_PORT_TYPE_VPORT)
	{
		if(l2_vp_port_tbl.bf.physical_port_mtu_check_en)
		{
			mtu_cfg->mtu_enable = A_FALSE;
		}
	}

	return SW_OK;
}

#define HTT_XGMAC_USXGMII_ENABLE 1
#define HTT_XGMAC_USXGMII_CLEAR 0

#define HTT_XGMAC_SPEED_SELECT_10000M 0
#define HTT_XGMAC_SPEED_SELECT_5000M 1
#define HTT_XGMAC_SPEED_SELECT_2500M 2
#define HTT_XGMAC_SPEED_SELECT_1000M 3

sw_error_t
_adpt_httppe_xgmac_speed_set(a_uint32_t dev_id, a_uint32_t mac_id,
			     a_uint32_t mode, fal_port_speed_t speed)
{
	union mac_tx_configuration_u mac_tx_configuration = {0};
	a_uint32_t uss = 0, ss = 0;
	sw_error_t rv;

	rv = httppe_mac_tx_configuration_get(dev_id, mac_id, &mac_tx_configuration);
	SW_RTN_ON_ERROR (rv);

	switch (speed) {
	case FAL_SPEED_10000:
		if ((mode == PORT_USXGMII) || (mode == PORT_UQXGMII))
			uss = HTT_XGMAC_USXGMII_ENABLE;
		else
			uss = HTT_XGMAC_USXGMII_CLEAR;
		ss = HTT_XGMAC_SPEED_SELECT_10000M;
		break;
	case FAL_SPEED_5000:
		uss = HTT_XGMAC_USXGMII_ENABLE;
		ss = HTT_XGMAC_SPEED_SELECT_5000M;
		break;
	case FAL_SPEED_2500:
		if ((mode == PORT_USXGMII) || (mode == PORT_UQXGMII))
			uss = HTT_XGMAC_USXGMII_ENABLE;
		else
			uss = HTT_XGMAC_USXGMII_CLEAR;
		ss = HTT_XGMAC_SPEED_SELECT_2500M;
		break;
	case FAL_SPEED_1000:
	case FAL_SPEED_100:
	case FAL_SPEED_10:
		uss = HTT_XGMAC_USXGMII_CLEAR;
		ss = HTT_XGMAC_SPEED_SELECT_1000M;
		break;
	default:
		return SW_BAD_PARAM;
	}
	mac_tx_configuration.bf.ss = ss;
	mac_tx_configuration.bf.uss = uss;

	return httppe_mac_tx_configuration_set(dev_id, mac_id, &mac_tx_configuration);
}

sw_error_t
adpt_httppe_port_tx_counter_tbl_get(a_uint32_t dev_id, a_uint32_t port_id,
	fal_port_cnt_t *port_cnt)
{
	sw_error_t rtn = SW_OK;
	union port_tx_counter_tbl_reg_u phy_port_tx_cnt_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_cnt);

	aos_mem_zero(&phy_port_tx_cnt_tbl, sizeof(union port_tx_counter_tbl_reg_u));

	rtn = httppe_port_tx_counter_tbl_reg_get(dev_id, port_id, &phy_port_tx_cnt_tbl);
	SW_RTN_ON_ERROR(rtn);

	port_cnt->tx_pkt_cnt = phy_port_tx_cnt_tbl.bf.tx_packets;
	port_cnt->tx_byte_cnt = ((a_uint64_t)phy_port_tx_cnt_tbl.bf.tx_bytes_1 <<
		SW_FIELD_OFFSET_IN_WORD(PORT_TX_COUNTER_TBL_REG_TX_BYTES_OFFSET)) |
		phy_port_tx_cnt_tbl.bf.tx_bytes_0;

	return SW_OK;
}

sw_error_t
adpt_httppe_port_tx_counter_tbl_flush(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rtn = SW_OK;
	union port_tx_counter_tbl_reg_u phy_port_tx_cnt_tbl;

	ADPT_DEV_ID_CHECK(dev_id);

	aos_mem_zero(&phy_port_tx_cnt_tbl, sizeof(union port_tx_counter_tbl_reg_u));

	rtn = httppe_port_tx_counter_tbl_reg_set(dev_id, port_id, &phy_port_tx_cnt_tbl);
	return rtn;
}

sw_error_t
adpt_httppe_vp_tx_counter_tbl_get(a_uint32_t dev_id, a_uint32_t port_id,
	fal_port_cnt_t *port_cnt)
{
	sw_error_t rtn = SW_OK;
	union vp_tx_counter_tbl_reg_u vport_tx_cnt_tbl;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_cnt);

	aos_mem_zero(&vport_tx_cnt_tbl, sizeof(union vp_tx_counter_tbl_reg_u));

	rtn = httppe_vp_tx_counter_tbl_reg_get(dev_id, port_id, &vport_tx_cnt_tbl);
	SW_RTN_ON_ERROR(rtn);

	port_cnt->tx_pkt_cnt = vport_tx_cnt_tbl.bf.tx_packets;
	port_cnt->tx_byte_cnt = ((a_uint64_t)vport_tx_cnt_tbl.bf.tx_bytes_1 <<
		SW_FIELD_OFFSET_IN_WORD(VP_TX_COUNTER_TBL_REG_TX_BYTES_OFFSET)) |
		vport_tx_cnt_tbl.bf.tx_bytes_0;

	return rtn;
}

sw_error_t
adpt_httppe_vp_tx_counter_tbl_flush(a_uint32_t dev_id, a_uint32_t port_id)
{
	sw_error_t rtn = SW_OK;
	union vp_tx_counter_tbl_reg_u vport_tx_cnt_tbl;

	ADPT_DEV_ID_CHECK(dev_id);

	aos_mem_zero(&vport_tx_cnt_tbl, sizeof(union vp_tx_counter_tbl_reg_u));

	rtn = httppe_vp_tx_counter_tbl_reg_set(dev_id, port_id, &vport_tx_cnt_tbl);
	return rtn;
}

sw_error_t
adpt_httppe_vp_port_promisc_mode_get(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t *enable)
{
	return httppe_l2_vp_port_tbl_promisc_en_get(dev_id, port_id,
		enable);
}

sw_error_t
adpt_httppe_vp_port_promisc_mode_set(a_uint32_t dev_id, fal_port_t port_id,
	a_bool_t enable)
{
	return httppe_l2_vp_port_tbl_promisc_en_set(dev_id, port_id,
		enable);
}
