/*
* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
* SPDX-License-Identifier: ISC
*/

#include "ssdk_init.h"
#include "ssdk_hppe.h"
#include "ssdk_appe.h"
#include "ssdk_clk.h"
#include "ssdk_dts.h"
#include "adpt.h"

#define HTTPPE_BM_PORT_NUM            6
#define HTTPPE_BM_PHY_PORT_MAX        5
#define HTTPPE_BM_PHY_PORT_OFFSET     1
#define HTTPPE_BM_PORT_MIN            0

#if defined(IN_BM) && defined(IN_QOS)
fal_port_tdm_tick_cfg_t httppe_port_tdm_tbl[] = {
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 1, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 1, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 2, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 2, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 3, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 3, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 4, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 4, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 0, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_INGRESS, 5, 0, 0},
	{A_TRUE, FAL_PORT_TDB_DIR_EGRESS, 5, 0, 0},
};

fal_port_scheduler_cfg_t httppe_port_scheduler_tbl[] = {
	{0x1E, 0xF, 0x0, 0x0, 0x0},
	{0x1C, 0xF, 0x5, 0x0, 0x0},
	{0x1C, 0x0, 0x1, 0x0, 0x0},
	{0x1C, 0xF, 0x5, 0x0, 0x0},
	{0x1A, 0x1, 0x0, 0x0, 0x0},
	{0x1A, 0x5, 0x2, 0x0, 0x0},
	{0x1A, 0xF, 0x0, 0x0, 0x0},
	{0x16, 0x2, 0x5, 0x0, 0x0},
	{0x16, 0x0, 0x3, 0x0, 0x0},
	{0x16, 0xF, 0x5, 0x0, 0x0},
	{0x0E, 0x3, 0x0, 0x0, 0x0},
	{0x0E, 0x5, 0x4, 0x0, 0x0},
	{0x0E, 0xF, 0x0, 0x0, 0x0},
	{0x1C, 0x4, 0x5, 0x0, 0x0},
	{0x1C, 0x0, 0x1, 0x0, 0x0},
	{0x1C, 0xF, 0x5, 0x0, 0x0},
	{0x1A, 0x1, 0x0, 0x0, 0x0},
	{0x1A, 0x5, 0x2, 0x0, 0x0},
	{0x1A, 0xF, 0x0, 0x0, 0x0},
	{0x16, 0x2, 0x5, 0x0, 0x0},
	{0x16, 0x0, 0x3, 0x0, 0x0},
	{0x16, 0xF, 0x5, 0x0, 0x0},
	{0x0E, 0x3, 0x0, 0x0, 0x0},
	{0x0E, 0x5, 0x4, 0x0, 0x0},
	{0x0E, 0xF, 0x0, 0x0, 0x0},
	{0x1C, 0x4, 0x5, 0x0, 0x0},
	{0x1C, 0x0, 0x1, 0x0, 0x0},
	{0x1C, 0xF, 0x5, 0x0, 0x0},
	{0x1A, 0x1, 0x0, 0x0, 0x0},
	{0x1A, 0x5, 0x2, 0x0, 0x0},
	{0x1A, 0xF, 0x0, 0x0, 0x0},
	{0x16, 0x2, 0x5, 0x0, 0x0},
	{0x16, 0x0, 0x3, 0x0, 0x0},
	{0x16, 0xF, 0x5, 0x0, 0x0},
	{0x1E, 0x3, 0x0, 0x0, 0x0},
	{0x1E, 0xF, 0x5, 0x0, 0x0},
	{0x0E, 0xF, 0x0, 0x0, 0x0},
	{0x0E, 0x5, 0x4, 0x0, 0x0},
	{0x0E, 0xF, 0x0, 0x0, 0x0},
	{0x1E, 0x4, 0x5, 0x0, 0x0},
};

sw_error_t
qca_httppe_tdm_hw_init(a_uint32_t dev_id)
{
	adpt_api_t *p_api;
	a_uint32_t i = 0;
	a_uint32_t num = 0;
	fal_port_tdm_ctrl_t tdm_ctrl;
	fal_port_scheduler_cfg_t *scheduler_cfg = NULL;
	fal_port_tdm_tick_cfg_t *bm_cfg = NULL;

	SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
	SW_RTN_ON_NULL(p_api->adpt_port_scheduler_cfg_set);
	SW_RTN_ON_NULL(p_api->adpt_tdm_tick_num_set);

	num = ARRAY_SIZE(httppe_port_scheduler_tbl);
	scheduler_cfg = httppe_port_scheduler_tbl;
	if (scheduler_cfg != NULL) {
		for (i = 0; i < num; i++) {
			p_api->adpt_port_scheduler_cfg_set(dev_id, i, &scheduler_cfg[i]);
		}
		p_api->adpt_tdm_tick_num_set(dev_id, num);
	}

	SW_RTN_ON_NULL(p_api->adpt_port_tdm_tick_cfg_set);
	SW_RTN_ON_NULL(p_api->adpt_port_tdm_ctrl_set);

	num = ARRAY_SIZE(httppe_port_tdm_tbl);
	bm_cfg = httppe_port_tdm_tbl;
	if (bm_cfg != NULL) {
		for (i = 0; i < num; i++) {
			p_api->adpt_port_tdm_tick_cfg_set(dev_id, i, &bm_cfg[i]);
		}
	}

	tdm_ctrl.enable = A_TRUE;
	tdm_ctrl.offset = A_FALSE;
	tdm_ctrl.depth = num;
	p_api->adpt_port_tdm_ctrl_set(dev_id, &tdm_ctrl);

	return SW_OK;
}
#endif

#if defined(IN_PORTCONTROL)
sw_error_t
qca_httppe_portctrl_hw_init(a_uint32_t dev_id)
{
	a_uint32_t i = 0, mac_type_org = 0, mac_type = 0;
	fal_port_cnt_cfg_t init_cnt_cfg = {0};
	fal_port_eee_cfg_t port_eee_cfg = {0};
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);

	for(i = SSDK_PHYSICAL_PORT0; i < priv->ports_num; i++) {
		mac_type_org = qca_ppe_port_mac_type_get(dev_id, i);
		for(mac_type = PORT_GMAC_TYPE; mac_type <= PORT_XGMAC_TYPE; mac_type++) {
			if (mac_type == PORT_XGMAC_TYPE) {
				if ((i != SSDK_PHYSICAL_PORT0) && (i != SSDK_PHYSICAL_PORT5))
					continue;
			}
			qca_ppe_port_mac_type_set(dev_id, i, mac_type);
			if (hsl_port_feature_get(dev_id, i, PHY_F_FORCE) == A_TRUE) {
				fal_port_txmac_status_set(dev_id, i, A_TRUE);
				fal_port_rxmac_status_set(dev_id, i, A_TRUE);
			} else {
				fal_port_txmac_status_set(dev_id, i, A_FALSE);
				fal_port_rxmac_status_set(dev_id, i, A_FALSE);
			}
			fal_port_rxfc_status_set(dev_id, i, A_TRUE);
			fal_port_txfc_status_set(dev_id, i, A_TRUE);
			fal_port_max_frame_size_set(dev_id, i, SSDK_MAX_FRAME_SIZE_16K);
			fal_port_interface_eee_cfg_get(dev_id, i, &port_eee_cfg);
			port_eee_cfg.enable = A_FALSE;
			port_eee_cfg.lpi_tx_enable = A_FALSE;
			fal_port_interface_eee_cfg_set(dev_id, i, &port_eee_cfg);
		}
		qca_ppe_port_mac_type_set(dev_id, i, mac_type_org);
		if (i != SSDK_PHYSICAL_PORT0) {
			qca_mac_port_status_init(dev_id, i);
		}
	}

	aos_mem_zero(&init_cnt_cfg, sizeof(init_cnt_cfg));
	init_cnt_cfg.uc_tx_cnt_en = FAL_ENABLE;
	init_cnt_cfg.mc_tx_cnt_en = FAL_ENABLE;
	init_cnt_cfg.tl_rx_cnt_en = FAL_ENABLE;
	init_cnt_cfg.rx_cnt_en    = FAL_ENABLE;
	for(i = SSDK_PHYSICAL_PORT0; i < priv->ports_num; i++) {
		fal_port_cnt_cfg_set(dev_id, FAL_PORT_ID(FAL_PORT_TYPE_PPORT, i), &init_cnt_cfg);
	}
	return SW_OK;
}
#endif

#if defined(IN_BM)
sw_error_t
qca_httppe_bm_hw_init(a_uint32_t dev_id)
{
	a_uint32_t i = 0;
	fal_bm_dynamic_cfg_t cfg;
	a_uint16_t group_buf = 3550, share_ceiling = 0, phyport_share_ceiling = 0;
	a_uint16_t prealloc_buf = 0, react_buf = 0;

	for (i = 0; i < HTTPPE_BM_PORT_NUM; i++) {
		/*disable fc for phy ports and cpu port */
		fal_port_bm_ctrl_set(dev_id, i, A_FALSE);

		/* map to group 0 */
		fal_port_bufgroup_map_set(dev_id, i, 0);
	}
	fal_bm_bufgroup_buffer_set(dev_id, 0, group_buf);

	/* set reserved buffer */
	for (i = 0; i < HTTPPE_BM_PORT_NUM; i++) {
		if (i < HTTPPE_BM_PHY_PORT_OFFSET) {
			prealloc_buf = 0;
			react_buf = 200;
		} else {
			prealloc_buf = 0;
			react_buf = 228;
		}
		fal_bm_port_reserved_buffer_set(dev_id, i, prealloc_buf, react_buf);
	}

	/* set dynamic threshold */
	memset(&cfg, 0, sizeof(cfg));
	for (i = 0; i < HTTPPE_BM_PORT_NUM; i++) {
		if (i == HTTPPE_BM_PORT_MIN) {
			share_ceiling = 1200;
			cfg.resume_min_thresh = 0;
			cfg.resume_off = 8;
			cfg.weight= 7;
		} else {
			share_ceiling = 650;
			phyport_share_ceiling = 650;
			cfg.resume_min_thresh = 0;
			cfg.resume_off = 36;
			cfg.weight= 7;
		}
		if (i < HTTPPE_BM_PHY_PORT_OFFSET)
			cfg.shared_ceiling = share_ceiling;
		else
			cfg.shared_ceiling = phyport_share_ceiling;

		fal_bm_port_dynamic_thresh_set(dev_id, i, &cfg);
	}

	return SW_OK;
}
#endif

#if defined(IN_FDB)
sw_error_t qca_httppe_fdb_hw_init(a_uint32_t dev_id)
{
	a_uint32_t port = 0;
	adpt_api_t *p_api;
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);
	SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
	SW_RTN_ON_NULL(p_api->adpt_port_bridge_txmac_set);

	for(port = SSDK_PHYSICAL_PORT0; port < priv->ports_num; port++) {
		fal_fdb_port_learning_ctrl_set(dev_id, port, A_TRUE, FAL_MAC_FRWRD);
		fal_fdb_port_stamove_ctrl_set(dev_id, port, A_TRUE, FAL_MAC_FRWRD);
		fal_portvlan_member_update(dev_id, port, 0x3f);
		if (hsl_port_feature_get(dev_id, port, PHY_F_FORCE) == A_TRUE) {
			p_api->adpt_port_bridge_txmac_set(dev_id, port, A_TRUE);
		} else {
			p_api->adpt_port_bridge_txmac_set(dev_id, port, A_FALSE);
		}
		fal_port_promisc_mode_set(dev_id, port, A_TRUE);
	}

	fal_fdb_aging_ctrl_set(dev_id, FAL_FDB_AGE_ENABLE);
	fal_fdb_learning_ctrl_set(dev_id, HW_CTRL_LEARNING);

	return SW_OK;
}
#endif

sw_error_t
qca_httppe_interface_mode_init(a_uint32_t dev_id)
{

	adpt_api_t *p_api;
	sw_error_t rv = SW_OK;
	fal_port_t port_id;
	a_uint32_t index = 0;

	a_uint32_t uniphy_num = 0, mode[SSDK_MAX_UNIPHY_INSTANCE] = {0};
	struct qca_phy_priv *priv = ssdk_phy_priv_data_get(dev_id);

	SW_RTN_ON_NULL(priv);
	SW_RTN_ON_NULL(p_api = adpt_api_ptr_get(dev_id));
	SW_RTN_ON_NULL(p_api->adpt_port_mux_mac_type_set);
	SW_RTN_ON_NULL(p_api->adpt_uniphy_mode_set);

	uniphy_num = adpt_ppe_uniphy_number_get(dev_id);

	for (index = SSDK_UNIPHY_INSTANCE0; index < uniphy_num; index ++) {
		mode[index] = ssdk_dt_global_get_mac_mode(dev_id, index);
		/* TODO: Implement uniphy mode setting for HTTPPE chip */
		/* rv = p_api->adpt_uniphy_mode_set(dev_id, index, mode[index]); */
		/* SW_RTN_ON_ERROR(rv); */
	}

	for(port_id = SSDK_PHYSICAL_PORT0; port_id < priv->ports_num; port_id++) {
		rv = p_api->adpt_port_mux_mac_type_set(dev_id, port_id, mode[0], mode[1], mode[2]);
		if(rv != SW_OK) {
			SSDK_ERROR("port_id:%d, mode0:%d, mode1:%d, mode2:%d\n", port_id,
				mode[0], mode[1], mode[2]);
			continue;
		}
	}

	return rv;
}

#if defined(IN_CROSSCHIP)
sw_error_t qca_httppe_mdio_master_init(a_uint32_t dev_id)
{
#define HTT_MDIO_MASTER_CLK_62P5M		3

	return fal_crosschip_mdio_master_config(dev_id,
						A_TRUE,
						HTT_MDIO_MASTER_CLK_62P5M);
}
#endif

#if IS_ENABLED(CONFIG_NET_DSA)
sw_error_t ssdk_httppe_init_with_dsa(struct qca_phy_priv* priv)
{
	a_uint32_t dev_id = priv->device_id;
	sw_error_t rv = SW_OK;

	/* Return if not HTTPPE */
	if (priv->version != QCA_VER_HTTPPE)
		return SW_OK;

	/* HW init and swconfig only need process once in DSA interface events */
	if (priv->dsa_setup == A_TRUE)
		return SW_OK;

#if defined(IN_VSI)
	rv = qca_hppe_vsi_hw_init(dev_id);
	if (rv)
		goto SETUP_DONE;
#endif

#if defined(IN_POLICER)
	rv = qca_appe_policer_hw_init(dev_id);
	if (rv)
		goto SETUP_DONE;
#endif

#if defined(IN_SHAPER)
	rv = qca_appe_shaper_hw_init(dev_id);
	if (rv)
		goto SETUP_DONE;
#endif

#if defined(IN_ACL)
	rv = qca_hppe_acl_byp_intf_mac_learn(dev_id);
	if (rv)
		goto SETUP_DONE;
#if defined(IN_PTP)
	rv = qca_hppe_acl_remark_ptp_servcode(dev_id);
	if (rv)
		goto SETUP_DONE;
#endif
#endif

#if defined(IN_CTRLPKT)
	rv = qca_hppe_ctlpkt_hw_init(dev_id);
	if (rv)
		goto SETUP_DONE;
#endif

#if defined(IN_SWCONFIG)
	if (!priv->qca_ssdk_sw_dev_registered) {
		rv = qca_switchdev_register(priv);
		if (rv)
			goto SETUP_DONE;

		priv->qca_ssdk_sw_dev_registered = A_TRUE;
	}
#endif

	SSDK_INFO("HTTPPE Initializing for QCE DSA done!!\n");

SETUP_DONE:
	mutex_lock(&priv->reg_mutex);
	priv->dsa_setup = A_TRUE;
	mutex_unlock(&priv->reg_mutex);

	return rv;
}
#endif

sw_error_t qca_httppe_hw_init(a_uint32_t dev_id)
{
	sw_error_t rv = SW_OK;

	/* TODO: Implement PPE reset initialization for HTTPPE chip */
	/* ssdk_ppe_reset_init(dev_id); */

	rv = qca_switch_init(dev_id);
	SW_RTN_ON_ERROR(rv);

#if defined(IN_BM)
	rv = qca_httppe_bm_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_QM)
	rv = qca_hppe_qm_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
	/* disable ucast enqueue for mcast traffic */
	rv = fal_qm_mcast_enqueue_ctrl_set(dev_id, SSDK_PHYSICAL_PORT0, A_FALSE);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_QOS)
	rv = qca_hppe_qos_scheduler_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_BM) && defined(IN_QOS)
	rv = qca_httppe_tdm_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_FDB)
	rv= qca_httppe_fdb_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif
#if defined(IN_VSI)
	rv= qca_hppe_vsi_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif
#if defined(IN_PORTVLAN)
	rv = qca_hppe_portvlan_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

	rv = qca_httppe_interface_mode_init(dev_id);
	SW_RTN_ON_ERROR(rv);

#if (defined(IN_PORTCONTROL) || defined(IN_LED))
	rv = qca_httppe_portctrl_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_POLICER)
	rv = qca_appe_policer_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_SHAPER)
	rv = qca_appe_shaper_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_ACL)
	rv = qca_hppe_acl_byp_intf_mac_learn(dev_id);
	SW_RTN_ON_ERROR(rv);
#if defined(IN_PTP)
	rv = qca_hppe_acl_remark_ptp_servcode(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif
#endif

#if defined(IN_CTRLPKT)
	rv = qca_hppe_ctlpkt_hw_init(dev_id);
	SW_RTN_ON_ERROR(rv);
#endif

#if defined(IN_CROSSCHIP)
	return qca_httppe_mdio_master_init(dev_id);
#endif
	return rv;
}

