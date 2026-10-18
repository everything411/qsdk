/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/clk.h>
#include <linux/units.h>
#include "adpt.h"
#include "ssdk_init.h"
#include "hsl_reg.h"
#include "adpt_hppe.h"
#if defined(IN_SFP)
#include "adpt_sfp.h"
#endif
#include "adpt_appe.h"
#if defined(MPPE)
#include "adpt_mppe.h"
#endif
#if defined(MRPPE)
#include "adpt_mrppe.h"
#endif
#if defined(JHPPE)
#include "adpt_jhppe.h"
#include "adpt_jhppe_loopback.h"
#endif
#if defined(HMSPPE)
#include "adpt_hmsppe.h"
#endif
#if defined(HTTPPE)
#include "adpt_httppe.h"
#endif

#include "hsl_phy.h"
#include "ssdk_dts.h"

adpt_api_t *g_adpt_api[SW_MAX_NR_DEV] = {NULL};

adpt_chip_ver_t g_chip_ver[SW_MAX_NR_DEV] = {0};

adpt_api_t *adpt_api_ptr_get(a_uint32_t dev_id)
{
	if (dev_id >= SW_MAX_NR_DEV)
		return NULL;

	return g_adpt_api[dev_id];
}
adpt_ppe_type_t adpt_ppe_type_get(a_uint32_t dev_id)
{
	adpt_ppe_type_t ppe_type = MAX_PPE_TYPE;
	ssdk_chip_type chip_type = g_chip_ver[dev_id].chip_type;
	a_uint32_t revision = g_chip_ver[dev_id].chip_revision;

	switch (chip_type) {
		case CHIP_APPE:
			ppe_type = APPE_TYPE;
			if (revision == MPPE_REVISION)
				ppe_type = MPPE_TYPE;
			break;
		case CHIP_MRPPE:
			ppe_type = MRPPE_TYPE;
			break;
		case CHIP_JHPPE:
			ppe_type = JHPPE_TYPE;
			break;
		case CHIP_HMSPPE:
			ppe_type = HMSPPE_TYPE;
			break;
		case CHIP_HTTPPE:
			ppe_type = HTTPPE_TYPE;
			break;
		default:
			break;
	}

	return ppe_type;
}

a_uint32_t
adpt_ppe_uniphy_number_get(a_uint32_t dev_id)
{
	if(adpt_ppe_type_get(dev_id) == MPPE_TYPE)
		return (SSDK_UNIPHY_INSTANCE1+1);

	if(adpt_ppe_type_get(dev_id) == HTTPPE_TYPE)
		return (SSDK_UNIPHY_INSTANCE1+1);

	return (SSDK_UNIPHY_INSTANCE2+1);
}

a_uint32_t adpt_chip_type_get(a_uint32_t dev_id)
{
	return g_chip_ver[dev_id].chip_type;
}

a_uint32_t adpt_chip_revision_get(a_uint32_t dev_id)
{
	return g_chip_ver[dev_id].chip_revision;
}

a_uint32_t adpt_ppe_port_id_max_get(a_uint32_t dev_id)
{
	a_uint32_t chip_type = adpt_chip_type_get(dev_id);

	if (chip_type == CHIP_JHPPE)
		return JHPPE_PORT_ID_MAX;
	if (chip_type == CHIP_HTTPPE)
		return HTTPPE_PORT_ID_MAX;
	if (adpt_ppe_type_get(dev_id) == MPPE_TYPE)
		return MPPE_PORT_ID_MAX;
	/* APPE, MRPPE and HMSPPE chip type use below value */
	return HPPE_PORT_ID_MAX;
}

a_uint32_t adpt_chip_freq_get(a_uint32_t dev_id)
{
	a_uint32_t ppe_freq = ADPT_HPPE_FREQUENCY;
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);

	switch (ppe_type) {
		case HPPE_TYPE:
		case CPPE_TYPE:
			ppe_freq = ADPT_HPPE_FREQUENCY;
			break;
		case APPE_TYPE:
			ppe_freq = ADPT_APPE_FREQUENCY;
			break;
		case MPPE_TYPE:
			ppe_freq = ADPT_MPPE_FREQUENCY;
			break;
		case MRPPE_TYPE:
			ppe_freq = ADPT_MRPPE_FREQUENCY;
			break;
		case JHPPE_TYPE:
			ppe_freq = ADPT_JHPPE_FREQUENCY;
			break;
		case HMSPPE_TYPE:
			ppe_freq = ADPT_HMSPPE_FREQUENCY;
			break;
		case HTTPPE_TYPE: {
			struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

			if (priv && priv->core_clk)
				ppe_freq = clk_get_rate(priv->core_clk) / HZ_PER_MHZ;
			else
				ppe_freq = ADPT_HTTPPE_FREQUENCY;
			break;
		}
		default:
			SSDK_ERROR("Unknown chip type: %d\n", ppe_type);
			break;
	}

	return ppe_freq;
}

void adpt_ppe_mac_num_get(a_uint32_t dev_id, a_uint32_t *gmac_num, a_uint32_t *xgmac_num)
{
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);

	switch (ppe_type) {
		case APPE_TYPE:
		case JHPPE_TYPE:
			*gmac_num = 6;
			*xgmac_num = 6;
			break;
		case MPPE_TYPE:
			*gmac_num = 2;
			*xgmac_num = 2;
			break;
		case MRPPE_TYPE:
			*gmac_num = 3;
			*xgmac_num = 3;
			break;
		case HMSPPE_TYPE:
			*gmac_num = 6;
			*xgmac_num = 3;
			break;
		case HTTPPE_TYPE:
			*gmac_num = 6;
			*xgmac_num = 2;
			break;
		default:
			break;
	}
}

a_uint32_t ppe_port_to_gmac_id(a_uint32_t dev_id, fal_port_t port_id)
{
	a_uint32_t gmac_id = 0;

	if ((adpt_ppe_type_get(dev_id) != HTTPPE_TYPE) && (port_id > SSDK_PHYSICAL_PORT0))
		gmac_id = port_id -1;
	else
		gmac_id = port_id;

	return gmac_id;
}

a_uint32_t ppe_port_to_xgmac_id(a_uint32_t dev_id, fal_port_t port_id)
{
	a_uint32_t xgmac_id = 0;

	if ((adpt_ppe_type_get(dev_id) == HMSPPE_TYPE) && (port_id > SSDK_PHYSICAL_PORT0)) {
		if (port_id > SSDK_PHYSICAL_PORT4)
			xgmac_id = port_id -4;
		else
			xgmac_id = port_id -1;
	} else if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE) {
		if (port_id == SSDK_PHYSICAL_PORT5)
			xgmac_id = port_id - 4;
		else
			xgmac_id = port_id;
	} else {
		if (port_id > SSDK_PHYSICAL_PORT0)
			xgmac_id = port_id -1;
	}

	return xgmac_id;
}

a_uint32_t ppe_xgmac_id_to_port(a_uint32_t dev_id, a_uint32_t xgmac_id)
{
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);
	a_uint32_t port_id = 0;

	if (ppe_type == HMSPPE_TYPE) {
		if (xgmac_id == 0)
			port_id = xgmac_id + 1;
		else
			port_id = xgmac_id + 4;
	} else if (ppe_type == HTTPPE_TYPE) {
		if (xgmac_id == 0)
			port_id = xgmac_id;
		else
			port_id = xgmac_id + 4;
	} else {
		port_id = xgmac_id + 1;
	}

	return port_id;
}

static sw_error_t adpt_appe_module_func_register(a_uint32_t dev_id, a_uint32_t module)
{
	sw_error_t rv= SW_OK;

	switch (module)
	{
		case FAL_MODULE_VPORT:
#if defined(IN_VPORT)
			rv = adpt_appe_vport_init(dev_id);
#endif
			break;
		case FAL_MODULE_TUNNEL:
#if defined(IN_TUNNEL)
			rv = adpt_appe_tunnel_init(dev_id);
#endif
			break;
		case FAL_MODULE_VXLAN:
#if defined(IN_VXLAN)
			rv = adpt_appe_vxlan_init(dev_id);
#endif
			break;
		case FAL_MODULE_GENEVE:
#if defined(IN_GENEVE)
			rv = adpt_appe_geneve_init(dev_id);
#endif
			break;
		case FAL_MODULE_TUNNEL_PROGRAM:
#if defined(IN_TUNNEL_PROGRAM)
			rv = adpt_appe_tunnel_program_init(dev_id);
#endif
			break;
		case FAL_MODULE_MAPT:
#if defined(IN_MAPT)
			rv = adpt_appe_mapt_init(dev_id);
#endif
			break;
#if defined(IN_ATHTAG)
		case FAL_MODULE_ATHTAG:
			rv = adpt_mppe_athtag_init(dev_id);
			break;
#endif
#if defined(IN_PKTEDIT)
		case FAL_MODULE_PKTEDIT:
			rv = adpt_mrppe_pktedit_init(dev_id);
			break;
#endif
#if defined(IN_SAMPL)
		case FAL_MODULE_SAMPL:
			rv = adpt_jhppe_sampling_init(dev_id);
			break;
#endif
#if defined(IN_PON)
		case FAL_MODULE_PON:
			rv = adpt_hmsppe_pon_init(dev_id);
			break;
#endif
#if defined(IN_PON_PM)
		case FAL_MODULE_PON_PM:
			rv = adpt_jhppe_pon_pm_init(dev_id);
			break;
#endif
#if defined(IN_IPMC)
		case FAL_MODULE_IPMC:
			rv = adpt_httppe_ipmc_init(dev_id);
			break;
#endif
#if defined(IN_CROSSCHIP)
		case FAL_MODULE_CROSSCHIP:
			rv = adpt_httppe_crosschip_init(dev_id);
			break;
#endif
		default:
			break;
	}

	return rv;
}

static sw_error_t adpt_hppe_module_func_register(a_uint32_t dev_id, a_uint32_t module)
{
	sw_error_t rv= SW_OK;

	switch (module)
	{
		case FAL_MODULE_ACL:
#if defined(IN_ACL)
			rv = adpt_hppe_acl_init(dev_id);
#endif
			break;
		case FAL_MODULE_VSI:
#if defined(IN_VSI)
			rv = adpt_hppe_vsi_init(dev_id);
#endif
			break;
		case FAL_MODULE_IP:
#if defined(IN_IP)
			rv = adpt_hppe_ip_init(dev_id);
#endif
			break;
		case FAL_MODULE_FLOW:
#if defined(IN_FLOW)
			rv = adpt_hppe_flow_init(dev_id);
#endif
			break;
		case FAL_MODULE_QM:
#if defined(IN_QM)
			rv = adpt_hppe_qm_init(dev_id);
#endif
			break;
		case FAL_MODULE_QOS:
#if defined(IN_QOS)
			rv = adpt_hppe_qos_init(dev_id);
#endif
			break;
		case FAL_MODULE_BM:
#if defined(IN_BM)
			rv = adpt_hppe_bm_init(dev_id);
#endif
			break;
		case FAL_MODULE_SERVCODE:
#if defined(IN_SERVCODE)
			rv = adpt_hppe_servcode_init( dev_id);
#endif
			break;
		case FAL_MODULE_RSS_HASH:
#if defined(IN_RSS_HASH)
			rv = adpt_hppe_rss_hash_init( dev_id);
#endif
			break;
		case FAL_MODULE_PPPOE:
#if defined(IN_PPPOE)
			rv = adpt_hppe_pppoe_init(dev_id);
#endif
			break;
		case FAL_MODULE_PORTCTRL:
#if defined(IN_PORTCONTROL)
			rv = adpt_hppe_port_ctrl_init(dev_id);
#endif
			break;
		case FAL_MODULE_SHAPER:
#if defined(IN_SHAPER)
			rv = adpt_hppe_shaper_init( dev_id);
#endif
			break;
		case FAL_MODULE_MIB:
#if defined(IN_MIB)
			rv = adpt_hppe_mib_init(dev_id);
#endif
			break;
		case FAL_MODULE_MIRROR:
#if defined(IN_MIRROR)
			rv = adpt_hppe_mirror_init( dev_id);
#endif
			break;
		case FAL_MODULE_FDB:
#if defined(IN_FDB)
			rv = adpt_hppe_fdb_init(dev_id);
#endif
			break;
		case FAL_MODULE_STP:
#if defined(IN_STP)
			rv = adpt_hppe_stp_init(dev_id);
#endif
			break;
		case FAL_MODULE_TRUNK:
#if defined(IN_TRUNK)
			rv = adpt_hppe_trunk_init( dev_id);
#endif
			break;
		case FAL_MODULE_PORTVLAN:
#if defined(IN_PORTVLAN)
			rv = adpt_hppe_portvlan_init(dev_id);
#endif
			break;
		case FAL_MODULE_CTRLPKT:
#if defined(IN_CTRLPKT)
			rv = adpt_hppe_ctrlpkt_init( dev_id);
#endif
			break;
		case FAL_MODULE_SEC:
#if defined(IN_SEC)
			rv = adpt_hppe_sec_init(dev_id);
#endif
			break;
		case FAL_MODULE_POLICER:
#if defined(IN_POLICER)
			rv = adpt_hppe_policer_init(dev_id);
#endif
			break;
		case FAL_MODULE_MISC:
#if defined(IN_MISC)
			rv = adpt_hppe_misc_init(dev_id);
#endif
			break;
		case FAL_MODULE_SFP:
#if defined(IN_SFP)
			rv = adpt_sfp_init(dev_id);
#endif
			break;
		default:
			break;
	}

	return rv;
}

sw_error_t adpt_ppe_capacity_get(a_uint32_t dev_id, fal_ppe_tbl_caps_t *ppe_capacity)
{
	ADPT_NULL_POINT_CHECK(ppe_capacity);

	ppe_capacity->flow_caps = IN_FLOW_TBL_NUM;
	ppe_capacity->host_caps = HOST_TBL_NUM;
	ppe_capacity->nexthop_caps = IN_NEXTHOP_TBL_NUM;
	ppe_capacity->pub_ip_caps = IN_PUB_IP_ADDR_TBL_NUM;
	ppe_capacity->vsi_caps = VSI_TBL_NUM;
	ppe_capacity->port_caps = PPE_CAPACITY_PORT_NUM;
	ppe_capacity->l3_if_caps = IN_L3_IF_TBL_NUM;
	ppe_capacity->my_mac_caps = MY_MAC_TBL_NUM;
	ppe_capacity->queue_caps = PPE_CAPACITY_QUEUES_NUM;
	ppe_capacity->service_code_caps = SERVICE_TBL_NUM;
	ppe_capacity->pppoe_session_caps = PPPOE_SESSION_NUM;
	ppe_capacity->policer_caps = IN_ACL_METER_CFG_TBL_NUM;
#if defined(MRPPE)
	ppe_capacity->ipv6_prefix_caps = EG_IPV6_PREFIX_TBL_NUM;
	ppe_capacity->ipv6_iid_caps = EG_FLOW_IPV6_IID_TBL_NUM;
#else
	ppe_capacity->ipv6_prefix_caps = 0;
	ppe_capacity->ipv6_iid_caps = 0;
#endif

	return SW_OK;
}

a_uint32_t adpt_ppe_mac_type_get(a_uint32_t dev_id, a_uint32_t port_id)
{
	union appe_port_mux_ctrl_u appe_port_mux_ctrl;
	a_uint32_t port_mac_sel = 0;
	sw_error_t rv;

	rv = appe_port_mux_ctrl_get(dev_id, &appe_port_mux_ctrl);
	if (rv != SW_OK)
		return PORT_GMAC_TYPE;

	/* Only HTTPPE is supported currently. Branches can be added below
	 * to support other PPE types in the future.
	 */
	if (adpt_ppe_type_get(dev_id) == HTTPPE_TYPE) {
		switch (port_id) {
		case SSDK_PHYSICAL_PORT0:
			port_mac_sel = appe_port_mux_ctrl.bf.port1_mac_sel;
			break;
		case SSDK_PHYSICAL_PORT5:
			port_mac_sel = appe_port_mux_ctrl.bf.port5_mac_sel;
			break;
		default:
			return PORT_GMAC_TYPE;
		}
	}

	return port_mac_sel ? PORT_XGMAC_TYPE : PORT_GMAC_TYPE;
}

sw_error_t adpt_init(a_uint32_t dev_id, ssdk_init_cfg *cfg)
{
	sw_error_t rv= SW_OK;

	/* isisc and mht doesn't support adpt API */
	if (cfg->chip_type == CHIP_ISISC || cfg->chip_type == CHIP_MHT)
		return SW_OK;

	if (g_adpt_api[dev_id] == NULL) {
		g_adpt_api[dev_id] = aos_mem_alloc(sizeof(adpt_api_t));
		if(g_adpt_api[dev_id] == NULL)
		{
			SSDK_ERROR("%s, %d:malloc fail for adpt api\n",
					__FUNCTION__, __LINE__);
			return SW_FAIL;
		}
		aos_mem_zero(g_adpt_api[dev_id], sizeof(adpt_api_t));
	}
	g_chip_ver[dev_id].chip_type = cfg->chip_type;
	g_chip_ver[dev_id].chip_revision = cfg->chip_revision;

	switch (cfg->chip_type)
	{
		case CHIP_HMSPPE:
#if defined(HMSPPE)
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_PON);
			SW_RTN_ON_ERROR(rv);
#endif
			fallthrough;
		case CHIP_JHPPE:
#if defined(JHPPE)
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_SAMPL);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_PON_PM);
			SW_RTN_ON_ERROR(rv);
#endif
			fallthrough;
		case CHIP_MRPPE:
		case CHIP_APPE:
			/* APPE specific module initialization */
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_TUNNEL);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_VXLAN);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_GENEVE);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_TUNNEL_PROGRAM);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_MAPT);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_IP);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_FLOW);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_RSS_HASH);
			SW_RTN_ON_ERROR(rv);
			fallthrough;
		case CHIP_HTTPPE:
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_VPORT);
			SW_RTN_ON_ERROR(rv);
#if defined(MRPPE) || defined(HTTPPE)
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_PKTEDIT);
			SW_RTN_ON_ERROR(rv);
#endif
#if defined(MPPE) || defined(HTTPPE)
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_ATHTAG);
			SW_RTN_ON_ERROR(rv);
#endif
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_MIRROR);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_TRUNK);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_FDB);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_LED);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_SFP);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_STP);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_PORTVLAN);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_CTRLPKT);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_SEC);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_ACL);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_VSI);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_QM);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_QOS);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_BM);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_SERVCODE);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_PPPOE);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_PORTCTRL);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_SHAPER);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_MIB);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_POLICER);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_MISC);
			SW_RTN_ON_ERROR(rv);
			rv = adpt_hppe_module_func_register(dev_id, FAL_MODULE_PTP);
			SW_RTN_ON_ERROR(rv);
#if defined(HTTPPE)
			if (cfg->chip_type == CHIP_HTTPPE) {
				rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_CROSSCHIP);
				SW_RTN_ON_ERROR(rv);
			}

			rv = adpt_appe_module_func_register(dev_id, FAL_MODULE_IPMC);
			SW_RTN_ON_ERROR(rv);
#endif
			/* uniphy */
			rv = adpt_hppe_uniphy_init(dev_id);
			SW_RTN_ON_ERROR(rv);
			break;
		default:
			break;
	}
	return rv;
}

sw_error_t adpt_deinit(a_uint32_t dev_id)
{
	sw_error_t rv = SW_OK;

	switch (adpt_chip_type_get(dev_id)) {
	case CHIP_HPPE:
	case CHIP_APPE:
	case CHIP_MRPPE:
	case CHIP_JHPPE:
	case CHIP_HMSPPE:
	case CHIP_HTTPPE:
#if defined(IN_ACL)
		rv = adpt_hppe_acl_deinit(dev_id);
		SW_RTN_ON_ERROR(rv);
#endif
#if defined(IN_POLICER)
		rv = adpt_hppe_policer_deinit(dev_id);
		SW_RTN_ON_ERROR(rv);
#endif
		break;
	default:
		/* do not need adpt API */
		break;
	}

	return rv;
}

a_bool_t
adpt_ppe_loopback_port_validate(a_uint32_t dev_id, fal_port_t port_id)
{
#if defined(JHPPE)
	sw_error_t rv = SW_OK;
	fal_port_t ppe_loopback_port = 0;

	rv = adpt_jhppe_switch_loopback_port_get(dev_id, &ppe_loopback_port);
	if (rv == SW_OK) {
		if (port_id == ppe_loopback_port) {
			return A_TRUE;
		}
	}
#endif
	return A_FALSE;
}

