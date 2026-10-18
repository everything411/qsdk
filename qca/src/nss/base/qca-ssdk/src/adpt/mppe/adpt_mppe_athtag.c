/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hsl_reg.h"
#include "adpt.h"
#include "hsl.h"
#include "hsl_dev.h"
#include "hsl_port_prop.h"
#include "ssdk_dts.h"
#if defined(HTTPPE)
#include "adpt_httppe_athtag.h"
#endif
#if defined(JHPPE)
#include "adpt_jhppe_athtag.h"
#endif
sw_error_t
adpt_mppe_athtag_pri_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_pri_mapping_t * pri_mapping)
{
	union prx_hdr_rcv_pri_mapping_u prx_pri_map = {0};
	union eg_hdr_xmit_pri_mapping_u ptx_pri_map = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	if (direction == FAL_DIR_INGRESS || direction == FAL_DIR_BOTH)
	{
		/*ingress priority mapping*/
		SW_RTN_ON_ERROR(mppe_prx_hdr_rcv_pri_mapping_get(dev_id,
					pri_mapping->ath_pri, &prx_pri_map));
		prx_pri_map.bf.int_pri = pri_mapping->int_pri;
		SW_RTN_ON_ERROR(mppe_prx_hdr_rcv_pri_mapping_set(dev_id,
					pri_mapping->ath_pri, &prx_pri_map));
	}
	if (direction == FAL_DIR_EGRESS || direction == FAL_DIR_BOTH)
	{
#if defined(HTTPPE)
		if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
			return adpt_httppe_athtag_egress_pri_mapping_set(dev_id, pri_mapping);
#endif
		/*egress priority mapping*/
		SW_RTN_ON_ERROR(mppe_eg_hdr_xmit_pri_mapping_get(dev_id,
					pri_mapping->int_pri, &ptx_pri_map));
		ptx_pri_map.bf.pri = pri_mapping->ath_pri;
		SW_RTN_ON_ERROR(mppe_eg_hdr_xmit_pri_mapping_set(dev_id,
					pri_mapping->int_pri, &ptx_pri_map));
	}
	return SW_OK;
}

sw_error_t
adpt_mppe_athtag_pri_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_pri_mapping_t * pri_mapping)
{
	union prx_hdr_rcv_pri_mapping_u prx_pri_map = {0};
	union eg_hdr_xmit_pri_mapping_u ptx_pri_map = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(pri_mapping);

	if (direction == FAL_DIR_INGRESS)
	{
		SW_RTN_ON_ERROR(mppe_prx_hdr_rcv_pri_mapping_get(dev_id,
					pri_mapping->ath_pri, &prx_pri_map));
		pri_mapping->int_pri = prx_pri_map.bf.int_pri;
	}
	else if (direction == FAL_DIR_EGRESS)
	{
#if defined(HTTPPE)
		if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
			return adpt_httppe_athtag_egress_pri_mapping_get(dev_id, pri_mapping);
#endif
		SW_RTN_ON_ERROR(mppe_eg_hdr_xmit_pri_mapping_get(dev_id,
					pri_mapping->int_pri, &ptx_pri_map));
		pri_mapping->ath_pri = ptx_pri_map.bf.pri;
	}
	else
	{
		SSDK_ERROR("not support direction %d\n", direction);
		return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

static a_uint32_t _adpt_mppe_athtag_bit_index(a_uint32_t bits)
{
	a_uint32_t i = 0;
	for (i=0; i<32; i++)
	{
		if((bits >> i) &0x1)
		{
			return i;
		}
	}
	return 0xff;
}

static sw_error_t _adpt_mppe_fix_athtag_ver(a_uint32_t dev_id,
		a_uint32_t ppe_port, fal_athtag_version_t *athtag_ver)
{
	a_uint32_t pp_id = ppe_port;
	ssdk_netdev_switch_t *netdev_switch = NULL;

	if (ppe_port >= SSDK_MIN_VIRTUAL_PORT_ID) {
		adpt_api_t *p_api = NULL;
		SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
		SW_RTN_ON_NULL(p_api->adpt_vport_physical_port_id_get);

		SW_RTN_ON_ERROR(p_api->adpt_vport_physical_port_id_get(dev_id, ppe_port, &pp_id));
	}

	netdev_switch = ssdk_dts_netdev_switch_find(pp_id);

	/* s17c only supports v2 atherose header */
	if (netdev_switch && hsl_get_current_chip_type(netdev_switch->switch_dev_id) == CHIP_ISISC)
		*athtag_ver = FAL_ATHTAG_VER2;

	return SW_OK;
}

sw_error_t
adpt_mppe_port_athtag_rx_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
{
	union prx_port_to_vp_mapping_u prx_port_to_vp_map = {0};

	ADPT_DEV_ID_CHECK(dev_id);

	/*enable rx athtag for a specific athtag type*/
	SW_RTN_ON_ERROR(mppe_prx_port_to_vp_mapping_get(dev_id,
				port_id, &prx_port_to_vp_map));
	prx_port_to_vp_map.bf.atheros_hdr_en = cfg->athtag_en;
	prx_port_to_vp_map.bf.atheros_hdr_type = cfg->athtag_type;
#if defined(JHPPE)
	prx_port_to_vp_map.bf.atheros_hdr_ver = cfg->version;
#endif
	return mppe_prx_port_to_vp_mapping_set(dev_id,
				port_id, &prx_port_to_vp_map);
}

sw_error_t
adpt_mppe_port_athtag_rx_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_athtag_rx_cfg_t *cfg)
{
	union prx_port_to_vp_mapping_u prx_port_to_vp_map = {0};

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	SW_RTN_ON_ERROR(mppe_prx_port_to_vp_mapping_get(dev_id,
				port_id, &prx_port_to_vp_map));
	cfg->athtag_en = prx_port_to_vp_map.bf.atheros_hdr_en;
	cfg->athtag_type = prx_port_to_vp_map.bf.atheros_hdr_type;
#if defined(JHPPE)
	cfg->version = prx_port_to_vp_map.bf.atheros_hdr_ver;
#endif
	return SW_OK;
}

sw_error_t
adpt_mppe_port_athtag_tx_set(a_uint32_t dev_id,
		fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
{
	union eg_vp_tbl_u eg_vp_tbl = {0};
	union eg_gen_ctrl_u eg_gen_ctrl = {0};
	sw_error_t rv;

	ADPT_DEV_ID_CHECK(dev_id);

	/*set tx athtag type*/
#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
	{
		rv = adpt_httppe_athtag_tx_ath_hdr_type_set(dev_id, cfg);
		SW_RTN_ON_ERROR(rv);
	}
	else
#endif
	{
		rv = mppe_eg_gen_ctrl_get(dev_id, &eg_gen_ctrl);
		SW_RTN_ON_ERROR(rv);
		eg_gen_ctrl.bf.ath_hdr_type = cfg->athtag_type;
		rv = mppe_eg_gen_ctrl_set(dev_id, &eg_gen_ctrl);
		SW_RTN_ON_ERROR(rv);
	}

	/*set tx athtag enable and other configurations*/
	rv = appe_egress_vp_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &eg_vp_tbl);
	SW_RTN_ON_ERROR(rv);

	eg_vp_tbl.bf.ath_hdr_insert = cfg->athtag_en;
	eg_vp_tbl.bf.ath_hdr_default_type = cfg->action;
	eg_vp_tbl.bf.ath_hdr_from_cpu = cfg->bypass_fwd_en;

	/* For ver1 athtag, the disable_bit field is act as the highest bit of
	 * the 8bit destination port, which is programed by port mapping set
	 * function.
	 */
	if (cfg->version != FAL_ATHTAG_VER1)
		eg_vp_tbl.bf.ath_hdr_disable_bit = cfg->field_disable;

	rv = _adpt_mppe_fix_athtag_ver(dev_id, FAL_PORT_ID_VALUE(port_id), &cfg->version);
	SW_RTN_ON_ERROR(rv);

	eg_vp_tbl.bf.ath_hdr_ver = cfg->version;

	return appe_egress_vp_tbl_set(dev_id,
				FAL_PORT_ID_VALUE(port_id), &eg_vp_tbl);
}

sw_error_t
adpt_mppe_port_athtag_tx_get(a_uint32_t dev_id,
		fal_port_t port_id, fal_athtag_tx_cfg_t *cfg)
{
	union eg_vp_tbl_u eg_vp_tbl = {0};
	union eg_gen_ctrl_u eg_gen_ctrl = {0};
	sw_error_t rv;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(cfg);

	/*get tx athtag type*/
#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
	{
		rv = adpt_httppe_athtag_tx_ath_hdr_type_get(dev_id, cfg);
		SW_RTN_ON_ERROR(rv);
	}
	else
#endif
	{
		rv = mppe_eg_gen_ctrl_get(dev_id, &eg_gen_ctrl);
		SW_RTN_ON_ERROR(rv);
		cfg->athtag_type = eg_gen_ctrl.bf.ath_hdr_type;
	}

	/*get tx athtag enable and other configurations*/
	rv = appe_egress_vp_tbl_get(dev_id, FAL_PORT_ID_VALUE(port_id), &eg_vp_tbl);
	SW_RTN_ON_ERROR(rv);

	cfg->athtag_en = eg_vp_tbl.bf.ath_hdr_insert;
	cfg->action = eg_vp_tbl.bf.ath_hdr_default_type;
	cfg->bypass_fwd_en = eg_vp_tbl.bf.ath_hdr_from_cpu;
	cfg->field_disable = eg_vp_tbl.bf.ath_hdr_disable_bit;
	cfg->version = eg_vp_tbl.bf.ath_hdr_ver;

	return SW_OK;
}

sw_error_t
adpt_mppe_athtag_port_mapping_set(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t * port_mapping)
{
	union prx_port_to_vp_mapping_u prx_port_to_vp_map = {0};
	union eg_vp_tbl_u eg_vp_tbl = {0};
	a_uint32_t port_id = 0;
	fal_athtag_rx_cfg_t rx_cfg = {0};
	fal_athtag_tx_cfg_t tx_cfg = {0};
	adpt_api_t *p_api = NULL;
	ssdk_netdev_switch_t *netdev_switch = NULL;

	ADPT_DEV_ID_CHECK(dev_id);
	SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
	SW_RTN_ON_NULL(p_api->adpt_port_bridge_txmac_set);

#if defined(JHPPE)
	/* if ath_port is port/vport ID instead of port bitmap, then
	 * v1 atheros header is used.
	 */
	if (FAL_PORT_ID_TYPE(port_mapping->ath_port) ==  FAL_PORT_TYPE_VPORT)
		return adpt_jhppe_v1_athtag_port_mapping_set(dev_id, direction, port_mapping);
#endif

	if (direction == FAL_DIR_INGRESS || direction == FAL_DIR_BOTH)
	{
		rx_cfg.athtag_en = A_TRUE;

		for (port_id = 0; port_id < SSDK_MAX_PORT_NUM; port_id ++) {
			if (!hsl_port_prop_check(dev_id, port_id, HSL_PP_PHY))
				continue;

			netdev_switch = ssdk_dts_netdev_switch_find(port_id);
			if (!netdev_switch)
				continue;
#if defined(MHT) && !IS_ENABLED(CONFIG_NET_DSA)
			if (hsl_get_current_chip_type(netdev_switch->switch_dev_id) == CHIP_MHT) {
				/* enable switch atheros header tx */
				fal_header_type_set(netdev_switch->switch_dev_id,
						A_TRUE, MHT_ATHTAG_TYPE);
				fal_port_txhdr_mode_set(netdev_switch->switch_dev_id,
						netdev_switch->switch_cpu_port,
						FAL_ALL_TYPE_FRAME_EN);
				rx_cfg.athtag_type = MHT_ATHTAG_TYPE;
			}
#endif
			/* enable athtag rx on ppe switch connected port */
			adpt_mppe_port_athtag_rx_set(dev_id, port_id, &rx_cfg);
		}

		/* ingress port mapping */
		for (port_id = 0; port_id < PRX_PORT_TO_VP_MAPPING_MAX_ENTRY; port_id++)
		{
			if (!SW_IS_PBMP_MEMBER(port_mapping->ath_port, port_id))
				continue;
			SW_RTN_ON_ERROR(mppe_prx_port_to_vp_mapping_get(dev_id,
						port_id, &prx_port_to_vp_map));
			prx_port_to_vp_map.bf.port_vp =
				FAL_PORT_ID_VALUE(port_mapping->int_port);
			SW_RTN_ON_ERROR(mppe_prx_port_to_vp_mapping_set(dev_id,
						port_id, &prx_port_to_vp_map));
		}
	}
	if (direction == FAL_DIR_EGRESS || direction == FAL_DIR_BOTH)
	{
		if (port_mapping->ath_port & (port_mapping->ath_port -1))
			tx_cfg.version = FAL_ATHTAG_VER2;
		else
			tx_cfg.version = FAL_ATHTAG_VER3;
		tx_cfg.athtag_en = A_TRUE;

		for (port_id = 0; port_id < SSDK_MAX_PORT_NUM; port_id ++) {
			if (!hsl_port_prop_check(dev_id, port_id, HSL_PP_PHY))
				continue;

			netdev_switch = ssdk_dts_netdev_switch_find(port_id);
			if (!netdev_switch)
				continue;
#if defined(MHT) && !IS_ENABLED(CONFIG_NET_DSA)
			if (hsl_get_current_chip_type(netdev_switch->switch_dev_id) == CHIP_MHT) {
				/* enable switch atheros header rx */
				fal_header_type_set(netdev_switch->switch_dev_id,
						A_TRUE, MHT_ATHTAG_TYPE);
				fal_port_rxhdr_mode_set(netdev_switch->switch_dev_id,
						netdev_switch->switch_cpu_port,
						FAL_ONLY_MANAGE_FRAME_EN);
				tx_cfg.athtag_type = MHT_ATHTAG_TYPE;
			}
#endif
		}

		tx_cfg.action = FAL_ATHTAG_ACTION_DISABLE_LEARN;
		tx_cfg.bypass_fwd_en = A_TRUE;
		tx_cfg.field_disable = A_FALSE;
		/* enable ppe int port athtag tx */
		adpt_mppe_port_athtag_tx_set(dev_id, port_mapping->int_port, &tx_cfg);

		/* egress port mapping */
		SW_RTN_ON_ERROR(appe_egress_vp_tbl_get(dev_id,
					FAL_PORT_ID_VALUE(port_mapping->int_port), &eg_vp_tbl));

		if (eg_vp_tbl.bf.ath_hdr_ver == 2)
		{
			/*for version2 the ath_port_bitmap in eg_vp_tbl is the port bitmap*/
			eg_vp_tbl.bf.ath_port_bitmap_0 = port_mapping->ath_port & 0x3f;
			eg_vp_tbl.bf.ath_port_bitmap_1 = (port_mapping->ath_port >>6) & 0x1;
		}
		else
		{
			/*get the first port id in the port bitmap*/
			port_id = _adpt_mppe_athtag_bit_index(port_mapping->ath_port);
			/*for version3 the ath_port_bitmap in eg_vp_tbl is the port id*/
			eg_vp_tbl.bf.ath_port_bitmap_0 = port_id & 0x3f;
			eg_vp_tbl.bf.ath_port_bitmap_1 = (port_id >>6) & 0x1;
		}
		SW_RTN_ON_ERROR(appe_egress_vp_tbl_set(dev_id,
					FAL_PORT_ID_VALUE(port_mapping->int_port), &eg_vp_tbl));

		/* enable txmac_en for int_port */
		p_api->adpt_port_bridge_txmac_set(dev_id, port_mapping->int_port, A_TRUE);
	}

	return SW_OK;
}

sw_error_t
adpt_mppe_athtag_port_mapping_get(a_uint32_t dev_id,
		fal_direction_t direction, fal_athtag_port_mapping_t * port_mapping)
{
	union prx_port_to_vp_mapping_u prx_port_to_vp_map = {0};
	union eg_vp_tbl_u eg_vp_tbl = {0};
	a_uint32_t port_id = 0;

	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(port_mapping);

	if (direction == FAL_DIR_INGRESS)
	{
#if defined(JHPPE)
		if (FAL_PORT_ID_TYPE(port_mapping->ath_port) ==  FAL_PORT_TYPE_VPORT)
			return adpt_jhppe_v1_athtag_ingress_port_mapping_get(dev_id, port_mapping);
#endif
		/*get the first port id in the port bitmap*/
		port_id = _adpt_mppe_athtag_bit_index(port_mapping->ath_port);
		SW_RTN_ON_ERROR(mppe_prx_port_to_vp_mapping_get(dev_id,
					port_id, &prx_port_to_vp_map));
		port_mapping->int_port = prx_port_to_vp_map.bf.port_vp;
	}
	else if (direction == FAL_DIR_EGRESS)
	{
		SW_RTN_ON_ERROR(appe_egress_vp_tbl_get(dev_id,
					FAL_PORT_ID_VALUE(port_mapping->int_port), &eg_vp_tbl));

		if (eg_vp_tbl.bf.ath_hdr_ver == FAL_ATHTAG_VER2)
		{
			/*for version2 the ath_port_bitmap in eg_vp_tbl is the port bitmap*/
			port_mapping->ath_port =
			(eg_vp_tbl.bf.ath_port_bitmap_1 << 6) | eg_vp_tbl.bf.ath_port_bitmap_0;
		}
		else if (eg_vp_tbl.bf.ath_hdr_ver == FAL_ATHTAG_VER3)
		{
			/*for version3 the ath_port_bitmap in eg_vp_tbl is the port id*/
			port_mapping->ath_port =
			BIT((eg_vp_tbl.bf.ath_port_bitmap_1 << 6) | eg_vp_tbl.bf.ath_port_bitmap_0);
		}
#if defined(JHPPE)
		else if (eg_vp_tbl.bf.ath_hdr_ver == FAL_ATHTAG_VER1)
		{
			/* 8bit port for v1 header */
			a_uint32_t port = (eg_vp_tbl.bf.ath_hdr_disable_bit << 7) |
					  (eg_vp_tbl.bf.ath_port_bitmap_1 << 6) |
					  eg_vp_tbl.bf.ath_port_bitmap_0;
			port_mapping->ath_port = FAL_PORT_ID(FAL_PORT_TYPE_VPORT, port);
		}
#endif
		else
			return SW_BAD_PARAM;
	}
	else
	{
		SSDK_ERROR("not support direction %d\n", direction);
		return SW_NOT_SUPPORTED;
	}

	return SW_OK;
}

sw_error_t adpt_mppe_athtag_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	ADPT_NULL_POINT_CHECK(p_adpt_api);

	p_adpt_api->adpt_athtag_pri_mapping_set = adpt_mppe_athtag_pri_mapping_set;
	p_adpt_api->adpt_athtag_pri_mapping_get = adpt_mppe_athtag_pri_mapping_get;
	p_adpt_api->adpt_athtag_port_mapping_set = adpt_mppe_athtag_port_mapping_set;
	p_adpt_api->adpt_athtag_port_mapping_get = adpt_mppe_athtag_port_mapping_get;
	p_adpt_api->adpt_port_athtag_rx_set = adpt_mppe_port_athtag_rx_set;
	p_adpt_api->adpt_port_athtag_rx_get = adpt_mppe_port_athtag_rx_get;
	p_adpt_api->adpt_port_athtag_tx_set = adpt_mppe_port_athtag_tx_set;
	p_adpt_api->adpt_port_athtag_tx_get = adpt_mppe_port_athtag_tx_get;

#if defined(HTTPPE)
	if (adpt_ppe_type_get(dev_id) >= JHPPE_TYPE) {
		p_adpt_api->adpt_athtag_rx_dest_port_mapping_set =
			adpt_httppe_athtag_rx_dest_port_mapping_set;
		p_adpt_api->adpt_athtag_rx_dest_port_mapping_get =
			adpt_httppe_athtag_rx_dest_port_mapping_get;
		p_adpt_api->adpt_athtag_rx_servcode_mapping_set =
			adpt_httppe_athtag_rx_servcode_mapping_set;
		p_adpt_api->adpt_athtag_rx_servcode_mapping_get =
			adpt_httppe_athtag_rx_servcode_mapping_get;
	}
#endif

#if defined(JHPPE)
	p_adpt_api->adpt_athtag_rx_src_port_mapping_set = adpt_jhppe_athtag_rx_src_port_mapping_set;
	p_adpt_api->adpt_athtag_rx_src_port_mapping_get = adpt_jhppe_athtag_rx_src_port_mapping_get;
	p_adpt_api->adpt_athtag_tx_src_port_mapping_set = adpt_jhppe_athtag_tx_src_port_mapping_set;
	p_adpt_api->adpt_athtag_tx_src_port_mapping_get = adpt_jhppe_athtag_tx_src_port_mapping_get;
#endif
	return SW_OK;
}

/**
 * @}
 */
