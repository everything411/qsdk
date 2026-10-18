/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include "nss_macsec_types.h"
#include "nss_macsec.h"
#include "nss_macsec_utility.h"
#include "qca808x_macsec.h"



struct qca_macsec_device_t *p_macsec_device_cfg[MACSEC_DEVICE_MAXNUM];



static u32 macsec_device_register(struct phy_device *phydev)
{
	u32 dev_id = 0;
	u32 assign_id = INVALID_DEVICE_ID;

	nss_macsec_mutex_lock();
	for (dev_id = 0; dev_id < MACSEC_DEVICE_MAXNUM; dev_id++) {
		if (p_macsec_device_cfg[dev_id] != NULL) {
			if (phydev == p_macsec_device_cfg[dev_id]->port.phydev) {
				osal_print("macsec device:%d already exists!\n", dev_id);
				assign_id = INVALID_DEVICE_ID;
				break;
			}
		} else {
			if (INVALID_DEVICE_ID == assign_id)
				assign_id = dev_id;
		}
	}
	nss_macsec_mutex_unlock();
	return assign_id;
}

static g_error_t macsec_sak_xor(struct secy_sak_t *key, u16 xorkey)
{
	u16 sak16 = 0;

	for (int i = 0; i < 8; i++) {
		sak16 = (key->sak[i * 2 + 1] << 8) | key->sak[i * 2];
		sak16 ^= xorkey;
		key->sak[i * 2] = sak16 & 0xFF;
		key->sak[i * 2 + 1] = (sak16 >> 8) & 0xFF;
		if (key->len == SAK_LEN_256BITS) {
			sak16 = (key->sak1[i * 2 + 1] << 8) | key->sak1[i * 2];
			sak16 ^= xorkey;
			key->sak1[i * 2] = sak16 & 0xFF;
			key->sak1[i * 2 + 1] = (sak16 >> 8) & 0xFF;
		}
	}
	return ERROR_OK;
}

static g_error_t macsec_ctl_filt_mac_convert(struct secy_ctl_filt_t *secy_filt,
						struct secy_udf_filt_t *filter)
{
	memset(filter, 0, sizeof(*filter));

	filter->udf_field0 = secy_filt->sa_da_addr[0] << 0x8 |
		secy_filt->sa_da_addr[1];
	filter->udf_field1 = secy_filt->sa_da_addr[2] << 0x8 |
		secy_filt->sa_da_addr[3];
	filter->udf_field2 = secy_filt->sa_da_addr[4] << 0x8 |
		secy_filt->sa_da_addr[5];

	return ERROR_OK;
}

static g_error_t macsec_rx_ctl_filt_da_mac_set(struct macsec_port *port, u32 index,
						struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x0;
	filter.mask = secy_filt->match_mask;

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_rx_ctl_filt_sa_mac_set(struct macsec_port *port, u32 index,
						struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x6;
	filter.mask = secy_filt->match_mask;

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_rx_ctl_filt_ether_type_set(struct macsec_port *port, u32 index,
						   struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter, 0, sizeof(filter));
	memset(&filter1, 0, sizeof(filter1));

	filter.udf_field0 = secy_filt->ether_type_da_range;
	filter.offset = 0xc;
	filter.mask = secy_filt->match_mask & 0x30;

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_rx_ctl_filt_da_ether_type_set(struct macsec_port *port,
						      u32 index, struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x0;
	filter.mask = secy_filt->match_mask;

	filter1.udf_field0 = secy_filt->ether_type_da_range;
	filter1.offset = 0xc;
	filter1.mask = (secy_filt->match_mask >> 0x8) & 0x30;

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_rx_ctl_filt_sa_ether_type_set(struct macsec_port *port,
						      u32 index, struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x6;
	filter.mask = secy_filt->match_mask;

	filter1.udf_field0 = secy_filt->ether_type_da_range;
	filter1.offset = 0xc;
	filter1.mask = (secy_filt->match_mask >> 0x8) & 0x30;

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_rx_ctl_filt_half_da_sa_set(struct macsec_port *port, u32 index,
						   struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter, 0, sizeof(filter));
	memset(&filter1, 0, sizeof(filter1));


	filter.udf_field1 = secy_filt->sa_da_addr[0];
	filter.udf_field2 = secy_filt->sa_da_addr[1] << 0x8 |
		secy_filt->sa_da_addr[2];
	filter.offset = 0x0;
	filter.mask = (secy_filt->match_mask) & 0x7;

	filter1.udf_field1 = secy_filt->sa_da_addr[3];
	filter1.udf_field2 = secy_filt->sa_da_addr[4] << 0x8 |
		secy_filt->sa_da_addr[5];
	filter1.offset = 0x6;
	filter1.mask = (secy_filt->match_mask >> 0x8) & 0x7;

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_tx_ctl_filt_da_mac_set(struct macsec_port *port, u32 index,
					       struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x0;
	filter.mask = secy_filt->match_mask;

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_tx_ctl_filt_sa_mac_set(struct macsec_port *port, u32 index,
					       struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x6;
	filter.mask = secy_filt->match_mask;

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_tx_ctl_filt_ether_type_set(struct macsec_port *port, u32 index,
						   struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter, 0, sizeof(filter));
	memset(&filter1, 0, sizeof(filter1));

	filter.udf_field0 = secy_filt->ether_type_da_range;
	filter.offset = 0xc;
	filter.mask = secy_filt->match_mask & 0x30;

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_tx_ctl_filt_da_ether_type_set(struct macsec_port *port, u32 index,
						      struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x0;
	filter.mask = secy_filt->match_mask;

	filter1.udf_field0 = secy_filt->ether_type_da_range;
	filter1.offset = 0xc;
	filter1.mask = (secy_filt->match_mask >> 0x8) & 0x30;

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_tx_ctl_filt_sa_ether_type_set(struct macsec_port *port, u32 index,
						      struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter1, 0, sizeof(filter1));

	SHR_RET_ON_ERR(macsec_ctl_filt_mac_convert(secy_filt, &filter));

	filter.offset = 0x6;
	filter.mask = secy_filt->match_mask;

	filter1.udf_field0 = secy_filt->ether_type_da_range;
	filter1.offset = 0xc;
	filter1.mask = (secy_filt->match_mask >> 0x8) & 0x30;

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static g_error_t macsec_tx_ctl_filt_half_da_sa_set(struct macsec_port *port, u32 index,
						   struct secy_ctl_filt_t *secy_filt)
{
	struct secy_udf_filt_t filter, filter1;

	memset(&filter, 0, sizeof(filter));
	memset(&filter1, 0, sizeof(filter1));


	filter.udf_field1 = secy_filt->sa_da_addr[0];
	filter.udf_field2 = secy_filt->sa_da_addr[1] << 0x8 |
		secy_filt->sa_da_addr[2];
	filter.offset = 0x0;
	filter.mask = (secy_filt->match_mask) & 0x7;

	filter1.udf_field1 = secy_filt->sa_da_addr[3];
	filter1.udf_field2 = secy_filt->sa_da_addr[4] << 0x8 |
		secy_filt->sa_da_addr[5];
	filter1.offset = 0x6;
	filter1.mask = (secy_filt->match_mask >> 0x8) & 0x7;

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 0, &filter));
	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, 1, &filter1));

	return ERROR_OK;
}

static inline bool macsec_guard_clk_skip(struct macsec_port *port)
{
	bool clk_en = false;

	if (qca808x_secy_clock_en_get(port, &clk_en) != ERROR_OK)
		return true;

	return !clk_en;
}

static bool macsec_mib_saturate_supported(u32 phyid)
{
	return phyid == QCA8111_PHY ||
	       phyid == QCE1204_PHY ||
	       phyid == IPQ52XX_PHY;
}

static bool macsec_az_pattern_supported_phy(u32 phyid)
{
	return phyid == QCA8081_PHY ||
	       phyid == QCA8084_PHY ||
	       phyid == QCE1204_PHY ||
	       phyid == IPQ52XX_PHY;
}

u32 macsec_get_phyid(struct phy_device *phydev)
{
	u32 phyid = 0;

	if (phydev->is_c45)
		phyid = phydev->c45_ids.device_ids[
					__ffs(phydev->c45_ids.mmds_present)];
	else
		phyid = phydev->phy_id;
	return phyid;
}

u32 macsec_get_device_id(struct phy_device *phydev)
{
	u32 dev_id = 0;
	u32 find_id = INVALID_DEVICE_ID;

	for (dev_id = 0; dev_id < MACSEC_DEVICE_MAXNUM; dev_id++) {
		if (p_macsec_device_cfg[dev_id] != NULL) {
			if (phydev == p_macsec_device_cfg[dev_id]->port.phydev) {
				find_id = dev_id;
				break;
			}
		}
	}
	return find_id;
}

struct macsec_port *macsec_port_get_by_device_id(u32 dev_id)
{
	struct macsec_port *port = NULL;

	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: Wrong dev_id %d! \n", __func__, dev_id);
		return port;
	}

	if (p_macsec_device_cfg[dev_id] != NULL)
		port = &(p_macsec_device_cfg[dev_id]->port);
	if (unlikely(NULL == port))
		osal_print("%s: fail to find port for dev_id %d! \n", __func__, dev_id);
	return port;
}

g_error_t qca_macsec_channel_number_get(struct macsec_port *port, u32 *number)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(number != NULL));

	*number = SECY_SC_IDX_MAX + 1;

	return ERROR_OK;
}

static g_error_t qca_macsec_secy_init(struct macsec_port *port)
{
	u32 sc, sa;
	u32 pn = 1;
	u32 phyid = 0;

	SHR_PARAM_CHECK(port != NULL);

	phyid = macsec_get_phyid(port->phydev);

	SHR_RET_ON_ERR(qca_macsec_tx_mib_clear(port));
	SHR_RET_ON_ERR(qca_macsec_rx_mib_clear(port));
	if (macsec_az_pattern_supported_phy(phyid))
		SHR_RET_ON_ERR(qca808x_secy_forward_az_pattern_en_set(port, TRUE));
	for (sc = 0; sc < SECY_SC_IDX_MAX + 1; sc++) {
		SHR_RET_ON_ERR(qca_macsec_tx_sc_mib_clear(port, sc));
		SHR_RET_ON_ERR(qca_macsec_rx_sc_mib_clear(port, sc));
		/*SRAM bind table init*/
		SHR_RET_ON_ERR(qca808x_secy_tx_sc_en_zone_set(port, sc, FALSE, 0));
		SHR_RET_ON_ERR(qca808x_secy_rx_sc_en_zone_set(port, sc, FALSE, 0));
		for (sa = 0; sa < SECY_SA_IDX_MAX + 1; sa++) {
			SHR_RET_ON_ERR(qca_macsec_tx_sa_mib_clear(port, sc, sa));
			SHR_RET_ON_ERR(qca_macsec_rx_sa_mib_clear(port, sc, sa));
			/*init next pn*/
			SHR_RET_ON_ERR(qca808x_secy_tx_sa_npn_set(port, sc, sa, pn));
			SHR_RET_ON_ERR(qca808x_secy_rx_sa_npn_set(port, sc, sa, pn));
			SHR_RET_ON_ERR(qca808x_secy_tx_sa_xpn_set(port, sc, sa, pn));
			SHR_RET_ON_ERR(qca808x_secy_rx_sa_xpn_set(port, sc, sa, pn));
		}
	}
	return ERROR_OK;
}

g_error_t qca_macsec_cipher_suite_set(struct macsec_port *port,
				      enum secy_cipher_suite_t cipher_suite)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_cipher_suite_set(port, cipher_suite));
	return ERROR_OK;
}

g_error_t qca_macsec_cipher_suite_get(struct macsec_port *port,
				      enum secy_cipher_suite_t *cipher_suite)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(cipher_suite != NULL));

	SHR_RET_ON_ERR(qca808x_secy_cipher_suite_get(port, cipher_suite));
	return ERROR_OK;
}

g_error_t qca_macsec_sc_sa_mapping_mode_set(struct macsec_port *port,
					    enum macsec_sc_sa_mapping_mode_t mode)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK(port != NULL);

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	p_macsec_device_cfg[dev_id]->sc_sa_mapping_mode = mode;
	return ERROR_OK;
}

g_error_t qca_macsec_sc_sa_mapping_mode_get(struct macsec_port *port,
					    enum macsec_sc_sa_mapping_mode_t *pmode)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(pmode != NULL));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	*pmode = p_macsec_device_cfg[dev_id]->sc_sa_mapping_mode;

	return ERROR_OK;
}

g_error_t qca_macsec_xpn_en_set(struct macsec_port *port, bool enable)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_xpn_en_set(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_xpn_en_get(struct macsec_port *port, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_xpn_en_get(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_mtu_set(struct macsec_port *port, u32 len)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_mtu_set(port, len));
	return ERROR_OK;
}

g_error_t qca_macsec_mtu_get(struct macsec_port *port, u32 *len)
{
	SHR_PARAM_CHECK((port != NULL) && (len != NULL));

	SHR_RET_ON_ERR(qca808x_secy_mtu_get(port, len));
	return ERROR_OK;
}

g_error_t qca_macsec_special_pkt_ctrl_set(struct macsec_port *port,
					    enum secy_packet_type_t packet_type,
					    enum secy_packet_action_t action)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_special_pkt_ctrl_set(port, packet_type, action));
	return ERROR_OK;
}

g_error_t qca_macsec_special_pkt_ctrl_get(struct macsec_port *port,
					    enum secy_packet_type_t packet_type,
					    enum secy_packet_action_t *action)
{
	SHR_PARAM_CHECK((port != NULL) && (action != NULL));

	SHR_RET_ON_ERR(qca808x_secy_special_pkt_ctrl_get(port, packet_type, action));
	return ERROR_OK;
}

g_error_t qca_macsec_udf_ethtype_set(struct macsec_port *port, bool enable, u16 type)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_udf_ethtype_set(port, enable, type));
	return ERROR_OK;
}

g_error_t qca_macsec_udf_ethtype_get(struct macsec_port *port, bool *enable, u16 *type)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(enable != NULL) &&
			(type != NULL));

	SHR_RET_ON_ERR(qca808x_secy_udf_ethtype_get(port, enable, type));
	return ERROR_OK;
}

g_error_t qca_macsec_flow_control_en_set(struct macsec_port *port, bool enable)
{
	SHR_PARAM_CHECK(port != NULL);

	return qca808x_secy_flow_control_en_set(port, enable);
}

g_error_t qca_macsec_flow_control_en_get(struct macsec_port *port, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(enable != NULL));

	return qca808x_secy_flow_control_en_get(port, enable);
}

g_error_t qca_macsec_en_set(struct macsec_port *port, bool enable)
{
	enum secy_loopback_type_t type;
	u32 dev_id;
	u32 phyid = 0;
	g_error_t ret;

	SHR_PARAM_CHECK(port != NULL);

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.macsec_en_set_calls++;

	/* If disabling and clock is already off, MACsec is already disabled */
	if (!enable && macsec_guard_clk_skip(port))
		return ERROR_OK;

	/* If enabling and clock is already on, MACsec is already enabled */
	if (enable && !macsec_guard_clk_skip(port))
		return ERROR_OK;

	if (enable) {
		/* Registers accessible only with MACsec clock enabled */
		ret = qca808x_secy_clock_en_set(port, TRUE);
		if (ret) {
			macsec_device_debug[dev_id].device_state =
						MACSEC_SECY_INIT_FAILURE;
			return ret;
		}
	}

	/* Restore SecY configurations to default. Deferred hardware init is
	 * performed here instead of at device attach time, so the MACsec
	 * clock stays off (power saving) until MACsec is actually used.
	 */
	ret = qca_macsec_secy_init(port);
	if (ret) {
		if (enable) {
			macsec_device_debug[dev_id].device_state =
						MACSEC_SECY_INIT_FAILURE;
		}
		/* No cleanup - preserve state for hardware failure analysis */
		return ret;
	}

	type = enable ? MACSEC_NORMAL : MACSEC_BYPASS;
	SHR_RET_ON_ERR(qca808x_secy_loopback_set(port, type));
	phyid = macsec_get_phyid(port->phydev);
	if (phyid == QCA8081_PHY) {
		SHR_RET_ON_ERR(qca808x_secy_shadow_register_en_set(port, enable));
	} else if (phyid == QCA8111_PHY) {
		/* WAR: sequence order forward must be off in MACsec mode */
		SHR_RET_ON_ERR(qca81xx_secy_seq_fwd_en_set(port, !enable));
	}
	if (!enable) {
		SHR_RET_ON_ERR(qca_macsec_rx_sc_del_all(port));
		SHR_RET_ON_ERR(qca_macsec_tx_sc_del_all(port));
		SHR_RET_ON_ERR(qca808x_secy_clock_en_set(port, FALSE));
	}
	return ERROR_OK;
}

g_error_t qca_macsec_en_get(struct macsec_port *port, bool *enable)
{
	enum secy_loopback_type_t type;
	bool clk_en;

	SHR_PARAM_CHECK((port != NULL) && (enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_clock_en_get(port, &clk_en));
	if (!clk_en) {
		*enable = FALSE;
		return ERROR_OK;
	}

	SHR_RET_ON_ERR(qca808x_secy_loopback_get(port, &type));
	*enable = (type == MACSEC_NORMAL);
	return ERROR_OK;
}

g_error_t qca_macsec_controlled_port_en_set(struct macsec_port *port, bool enable)
{
	u32 dev_id;

	SHR_PARAM_CHECK(port != NULL);

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.controlled_port_en_calls++;

	SHR_RET_ON_ERR(qca808x_secy_controlled_port_en_set(port, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_controlled_port_en_get(struct macsec_port *port, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) && (enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_controlled_port_en_get(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_xpn_set(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 next_xpn)
{
	u32 dev_id = 0;
	u32 pn_index = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_xpn_set(port, sc_index, an, next_xpn));

	pn_index = (sc_index * 4 + an);
	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_xpn = next_xpn;
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_xpn_get(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 *next_xpn)
{
	u32 dev_id = 0, pn_index = 0, i = 0;
	u32 npn = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(next_xpn != NULL));

	pn_index = (sc_index * 4 + an);
	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	for (i = 0; i < SECY_PN_QUERY_TIMES; i++) {
		SHR_RET_ON_ERR(qca808x_secy_rx_sa_xpn_get(port, sc_index, an, &npn));
		*next_xpn = npn;
		if (npn >= p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_xpn) {
			p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_xpn = npn;
			break;
		}
	}
	if (i >= SECY_PN_QUERY_TIMES)
		*next_xpn = p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_xpn;

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_npn_set(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 next_pn)
{
	u32 dev_id = 0;
	u32 pn_index = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.rx_sa_npn_set_calls++;

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_npn_set(port, sc_index, an, next_pn));

	pn_index = (sc_index * 4 + an);
	p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_pn = next_pn;
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_npn_get(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 *next_pn)
{
	u32 dev_id = 0, pn_index = 0, i = 0;
	u32 npn = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(next_pn != NULL));

	pn_index = (sc_index * 4 + an);
	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	for (i = 0; i < SECY_PN_QUERY_TIMES; i++) {
		SHR_RET_ON_ERR(qca808x_secy_rx_sa_npn_get(port, sc_index, an, &npn));
		*next_pn = npn;
		if (npn >= p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_pn) {
			p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_pn = npn;
			break;
		}
	}
	if (i >= SECY_PN_QUERY_TIMES)
		*next_pn = p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].rx_sa_pn;

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_create(struct macsec_port *port, u32 sc_index, u32 an)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca_macsec_rx_sa_npn_set(port, sc_index, an, 1));
	SHR_RET_ON_ERR(qca808x_secy_rx_sa_create(port, sc_index, an));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_del(struct macsec_port *port, u32 sc_index, u32 an)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_del(port, sc_index, an));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_del_all(struct macsec_port *port)
{
	u32 sc_index = 0, an_index = 0;

	SHR_PARAM_CHECK(port != NULL);

	for (sc_index = 0; sc_index <= SECY_SC_IDX_MAX; sc_index++) {
		for (an_index = 0; an_index <= SECY_AN_IDX_MAX; an_index++)
			qca808x_secy_rx_sa_del(port, sc_index, an_index);
	}
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_in_used_get(struct macsec_port *port, u32 sc_index,
					u32 an, bool *p_in_used)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(p_in_used != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_en_get(port, sc_index,
			an, p_in_used));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_en_set(struct macsec_port *port, u32 sc_index,
				    u32 an, bool enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_en_set(port, sc_index,
			an, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_en_get(struct macsec_port *port, u32 sc_index,
				    u32 an, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_en_get(port, sc_index,
			an, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sak_set(struct macsec_port *port, u32 sc_index,
				  u32 an, struct secy_sak_t *key)
{
	bool enable = FALSE;
	u16 xorkey = 0;
	u32 dev_id;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key != NULL));

	if ((key->len != SAK_LEN_128BITS) &&
		(key->len != SAK_LEN_256BITS) &&
		(key->len != SAK_LEN_LEGACY))
		return ERROR_PARAM;

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.rx_sak_set_calls++;

	if (macsec_get_phyid(port->phydev) == QCA8111_PHY) {
		SHR_RET_ON_ERR(qca81xx_secy_xor_key_get(port,
							&enable, &xorkey));
		if (enable)
			macsec_sak_xor(key, xorkey);
	}

	SHR_RET_ON_ERR(qca808x_secy_rx_sak_set(port, sc_index, an, key));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sak_get(struct macsec_port *port, u32 sc_index,
				  u32 an, struct secy_sak_t *key)
{
	bool enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &enable));
	if (enable) {
		SHR_RET_ON_ERR(qca808x_secy_rx_sak_get(port, sc_index, an, key));
	} else {
		return ERROR_NOT_SUPPORT;
	}
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_ki_set(struct macsec_port *port, u32 sc_index, u32 an, struct secy_sa_ki_t *key_idendifier)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key_idendifier != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_ki_set(port, sc_index, an, key_idendifier));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_ki_get(struct macsec_port *port, u32 sc_index, u32 an, struct secy_sa_ki_t *key_idendifier)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key_idendifier != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_ki_get(port, sc_index, an, key_idendifier));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_ssci_set(struct macsec_port *port, u32 sc_index, u32 ssci)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_rx_sc_ssci_set(port, sc_index, ssci));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	p_macsec_device_cfg[dev_id]->rx_ssci[sc_index] = ssci;
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_ssci_get(struct macsec_port *port, u32 sc_index, u32 *ssci)
{
	u32 dev_id = 0;
	bool enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(ssci != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &enable));
	if (enable) {
		SHR_RET_ON_ERR(qca808x_secy_rx_sc_ssci_get(port, sc_index, ssci));
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		*ssci = p_macsec_device_cfg[dev_id]->rx_ssci[sc_index];
	}
	return ERROR_OK;
}

g_error_t qca_macsec_rx_prc_lut_set(struct macsec_port *port, u32 sc_index,
				    struct secy_rx_prc_lut_t *pentry)
{
	struct secy_rx_sc_policy_rule_t rule;
	u32 i =  0;
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(pentry != NULL));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	memset(&rule, 0, sizeof(rule));

	rule.rule_valid = pentry->valid;
	rule.action.rx_sc_index = sc_index;

	for (i = 0; i < 6; i++) {
		rule.mac_da.addr[i] = pentry->da[i];
		rule.mac_sa.addr[i] = pentry->sa[i];
		rule.rx_sci.addr[i] = pentry->sci[i];
	}
	rule.ethtype = pentry->ether_type;
	rule.inner_vlanid = pentry->inner_vlanid;
	rule.outer_vlanid = pentry->outer_vlanid;
	rule.bc_flag = pentry->bc_flag;
	rule.action.decryption_offset = pentry->offset;
	rule.action.rx_sc_index = pentry->channel;
	rule.rx_tci = pentry->tci;
	rule.rx_sci.port = (((u16)(pentry->sci[6])) << 8) |
				(u16)(pentry->sci[7]);
	rule.rule_mask = pentry->rule_mask;

	SHR_RET_ON_ERR(qca808x_secy_rx_sc_policy_set(port, sc_index, &rule));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memcpy(&(p_macsec_device_cfg[dev_id]->rx_lut[sc_index]), pentry,
			sizeof(struct secy_rx_prc_lut_t));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_prc_lut_get(struct macsec_port *port, u32 sc_index,
				    struct secy_rx_prc_lut_t *pentry)
{
	struct secy_rx_sc_policy_rule_t rule;
	u32 i = 0;
	u32 dev_id = 0;
	bool enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(pentry != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &enable));

	if (enable) {
		memset(&rule, 0, sizeof(rule));
		SHR_RET_ON_ERR(qca808x_secy_rx_sc_policy_get(port, sc_index, &rule));

		pentry->valid = rule.rule_valid;
		for (i = 0; i < 6; i++) {
			pentry->da[i] = rule.mac_da.addr[i];
			pentry->sa[i] = rule.mac_sa.addr[i];
			pentry->sci[i] = rule.rx_sci.addr[i];
		}
		pentry->ether_type = rule.ethtype;
		pentry->inner_vlanid = rule.inner_vlanid;
		pentry->outer_vlanid = rule.outer_vlanid;
		pentry->bc_flag = rule.bc_flag;
		pentry->offset = rule.action.decryption_offset;
		pentry->channel = rule.action.rx_sc_index;
		pentry->tci = rule.rx_tci;
		pentry->sci[6] = (u8)(rule.rx_sci.port >> 8);
		pentry->sci[7] = (u8)(rule.rx_sci.port & 0xff);
		pentry->rule_mask = rule.rule_mask;
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		memcpy(pentry, &(p_macsec_device_cfg[dev_id]->rx_lut[sc_index]),
			sizeof(struct secy_rx_prc_lut_t));
	}
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_in_used_get(struct macsec_port *port, u32 sc_index,
					bool *enable)
{
	u32 dev_id = 0;
	bool dbg_enable = FALSE;
	bool sc_enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &dbg_enable));

	if (dbg_enable) {
		SHR_RET_ON_ERR(qca808x_secy_rx_sc_en_get(port, sc_index, &sc_enable));
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		sc_enable = p_macsec_device_cfg[dev_id]->rx_lut[sc_index].valid;
	}

	*enable = sc_enable;
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_en_set(struct macsec_port *port, u32 sc_index,
				  bool enable)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_rx_sc_en_zone_set(port, sc_index, enable,
		p_macsec_device_cfg[dev_id]->rx_lut[sc_index].rule_mask));

	p_macsec_device_cfg[dev_id]->rx_lut[sc_index].valid = enable;
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_en_get(struct macsec_port *port, u32 sc_index, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca_macsec_rx_sc_in_used_get(port, sc_index, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_create(struct macsec_port *port, u32 sc_index)
{
	struct secy_rx_prc_lut_t rule;
	u32 dev_id;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.rx_sc_create_calls++;

	memset(&rule, 0, sizeof(rule));
	SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_get(port, sc_index, &rule));
	rule.valid = TRUE;
	rule.channel = sc_index;
	SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_set(port, sc_index, &rule));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_del(struct macsec_port *port, u32 sc_index)
{
	struct secy_rx_prc_lut_t rule;
	u32 dev_id;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.rx_sc_del_calls++;

	memset(&rule, 0, sizeof(rule));

	SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_get(port, sc_index, &rule));

	rule.valid = FALSE;
	rule.channel = sc_index;

	SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_set(port, sc_index, &rule));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_del_all(struct macsec_port *port)
{
	u32 sc_index = 0;
	struct secy_rx_prc_lut_t rule;

	SHR_PARAM_CHECK(port != NULL);

	for (sc_index = 0; sc_index < SECY_LUT_MAX; sc_index++) {
		memset(&rule, 0, sizeof(rule));
		SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_get(port,
				sc_index, &rule));
		rule.valid = FALSE;
		rule.channel = sc_index;
		SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_set(port,
				sc_index, &rule));
	}

	return ERROR_OK;
}

g_error_t qca_macsec_rx_prc_lut_clear(struct macsec_port *port, u32 sc_index)
{
	struct secy_rx_prc_lut_t rule;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	memset(&rule, 0, sizeof(rule));

	SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_set(port, sc_index, &rule));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_prc_lut_clear_all(struct macsec_port *port)
{
	u32 sc_index = 0;
	struct secy_rx_prc_lut_t rule;

	SHR_PARAM_CHECK(port != NULL);

	memset(&rule, 0, sizeof(rule));

	for (sc_index = 0; sc_index < SECY_LUT_MAX; sc_index++) {
		SHR_RET_ON_ERR(qca_macsec_rx_prc_lut_set(port, sc_index, &rule));
	}

	return ERROR_OK;
}

g_error_t qca_macsec_rx_validate_frame_set(struct macsec_port *port, enum secy_vf_t value)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_rx_validate_frame_set(port, value));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_validate_frame_get(struct macsec_port *port, enum secy_vf_t *value)
{
	SHR_PARAM_CHECK((port != NULL) && (value != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_validate_frame_get(port, value));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_replay_protect_en_set(struct macsec_port *port, bool enable)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_rx_replay_protect_en_set(port, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_replay_protect_en_get(struct macsec_port *port, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) && (enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_replay_protect_en_get(port, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_replay_window_set(struct macsec_port *port, u32 window)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_rx_replay_window_set(port, window));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_replay_window_get(struct macsec_port *port, u32 *window)
{
	SHR_PARAM_CHECK((port != NULL) && (window != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_replay_window_get(port, window));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_mib_get(struct macsec_port *port, u32 sc_index,
				     u32 an, struct secy_rx_sa_mib_t *mib)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(mib != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_mib_get(port, sc_index, an, mib));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_mib_get(struct macsec_port *port, u32 sc_index,
				     struct secy_rx_sc_mib_t *mib)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(mib != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_sc_mib_get(port, sc_index, mib));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_mib_get(struct macsec_port *port, struct secy_rx_mib_t *mib)
{
	SHR_PARAM_CHECK((port != NULL) && (mib != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_mib_get(port, mib));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_sa_mib_clear(struct macsec_port *port, u32 sc_index, u32 an)
{
	struct secy_rx_sa_mib_t mib;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	memset(&mib, 0, sizeof(mib));

	SHR_RET_ON_ERR(qca808x_secy_rx_sa_mib_get(port, sc_index, an, &mib));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_sc_mib_clear(struct macsec_port *port, u32 sc_index)
{
	struct secy_rx_sc_mib_t mib;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	memset(&mib, 0, sizeof(mib));

	SHR_RET_ON_ERR(qca808x_secy_rx_sc_mib_get(port, sc_index, &mib));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_mib_clear(struct macsec_port *port)
{
	struct secy_rx_mib_t mib;

	SHR_PARAM_CHECK(port != NULL);

	memset(&mib, 0, sizeof(mib));

	SHR_RET_ON_ERR(qca808x_secy_rx_mib_get(port, &mib));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_ctl_filt_set(struct macsec_port *port, u32 index,
				       struct secy_ctl_filt_t *secy_filt)
{
	g_error_t rv = ERROR_OK;
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(index == 0) &&
			(secy_filt != NULL));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	switch (secy_filt->match_type) {
	case SECY_CTL_COMPARE_DA:
		rv = macsec_rx_ctl_filt_da_mac_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_SA:
		rv = macsec_rx_ctl_filt_sa_mac_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_ETHER_TYPE:
		rv = macsec_rx_ctl_filt_ether_type_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_HALF_DA_SA:
		rv = macsec_rx_ctl_filt_half_da_sa_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_DA_ETHER_TYPE:
		rv = macsec_rx_ctl_filt_da_ether_type_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_SA_ETHER_TYPE:
		rv = macsec_rx_ctl_filt_sa_ether_type_set(port, index, secy_filt);
		break;
	default:
		rv = ERROR_PARAM;
	}
	if (rv != ERROR_OK)
		return rv;

	SHR_RET_ON_ERR(qca808x_secy_rx_ctl_filt_en_set(port, secy_filt->bypass));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memcpy(&(p_macsec_device_cfg[dev_id]->rx_ctl_filt), secy_filt,
			sizeof(struct secy_ctl_filt_t));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_ctl_filt_get(struct macsec_port *port, u32 index,
				       struct secy_ctl_filt_t *secy_filt)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(index == 0) &&
			(secy_filt != NULL));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memcpy(secy_filt, &(p_macsec_device_cfg[dev_id]->rx_ctl_filt),
		sizeof(struct secy_ctl_filt_t));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_ctl_filt_clear(struct macsec_port *port, u32 index)
{
	u32 i = 0;
	u32 dev_id = 0;
	struct secy_udf_filt_t filter;

	SHR_PARAM_CHECK((port != NULL) && (index == 0));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	memset(&filter, 0, sizeof(filter));

	SHR_RET_ON_ERR(qca808x_secy_rx_ctl_filt_en_set(port, FALSE));

	for (i = 0; i < 4; i++)
		SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, i,
				&filter));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memset(&(p_macsec_device_cfg[dev_id]->rx_ctl_filt), 0,
			sizeof(struct secy_ctl_filt_t));

	return ERROR_OK;
}

g_error_t qca_macsec_rx_ctl_filt_clear_all(struct macsec_port *port)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca_macsec_rx_ctl_filt_clear(port, 0));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_udf_filt_set(struct macsec_port *port, u32 index,
				       struct secy_udf_filt_t *filter)
{
	SHR_PARAM_CHECK((port != NULL) && (filter != NULL));

	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_set(port, index, filter));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_udf_filt_get(struct macsec_port *port, u32 index,
				       struct secy_udf_filt_t *filter)
{
	SHR_PARAM_CHECK((port != NULL) && (filter != NULL));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_filt_get(port, index, filter));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_udf_ufilt_cfg_set(struct macsec_port *port,
					    struct secy_udf_filt_cfg_t *cfg)
{
	SHR_PARAM_CHECK((port != NULL) && (cfg != NULL));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_ufilt_cfg_set(port, cfg));
	return ERROR_OK;
}

g_error_t qca_macsec_rx_udf_ufilt_cfg_get(struct macsec_port *port,
					    struct secy_udf_filt_cfg_t *cfg)
{
	SHR_PARAM_CHECK((port != NULL) && (cfg != NULL));
	SHR_RET_ON_ERR(qca808x_secy_rx_udf_ufilt_cfg_get(port, cfg));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_xpn_set(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 next_xpn)
{
	u32 dev_id = 0;
	u32 pn_index = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_xpn_set(port, sc_index, an, next_xpn));

	pn_index = (sc_index * 4 + an);
	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_xpn = next_xpn;
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_xpn_get(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 *next_xpn)
{
	u32 dev_id = 0, pn_index = 0, i = 0;
	u32 npn = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(next_xpn != NULL));

	pn_index = (sc_index * 4 + an);
	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	for (i = 0; i < SECY_PN_QUERY_TIMES; i++) {
		SHR_RET_ON_ERR(qca808x_secy_tx_sa_xpn_get(port, sc_index, an, &npn));
		*next_xpn = npn;
		if (npn >= p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_xpn) {
			p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_xpn = npn;
			break;
		}
	}
	if (i >= SECY_PN_QUERY_TIMES)
		*next_xpn = p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_xpn;

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_npn_set(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 next_pn)
{
	u32 dev_id = 0;
	u32 pn_index = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.tx_sa_npn_set_calls++;

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_npn_set(port, sc_index, an, next_pn));

	pn_index = (sc_index * 4 + an);
	p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_pn = next_pn;
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_npn_get(struct macsec_port *port, u32 sc_index,
				   u32 an, u32 *next_pn)
{
	u32 dev_id = 0, pn_index = 0, i = 0;
	u32 npn = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(next_pn != NULL));

	pn_index = (sc_index * 4 + an);
	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	for (i = 0; i < SECY_PN_QUERY_TIMES; i++) {
		SHR_RET_ON_ERR(qca808x_secy_tx_sa_npn_get(port, sc_index, an, &npn));
		*next_pn = npn;
		if (npn >= p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_pn) {
			p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_pn = npn;
			break;
		}
	}
	if (i >= SECY_PN_QUERY_TIMES)
		*next_pn = p_macsec_device_cfg[dev_id]->secy_pn_table[pn_index].tx_sa_pn;

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_create(struct macsec_port *port, u32 sc_index, u32 an)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_create(port, sc_index, an));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_del(struct macsec_port *port, u32 sc_index, u32 an)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_del(port, sc_index, an));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_del_all(struct macsec_port *port)
{
	u32 sc_index = 0, an_index = 0;

	SHR_PARAM_CHECK(port != NULL);

	for (sc_index = 0; sc_index <= SECY_SC_IDX_MAX; sc_index++) {
		for (an_index = 0; an_index <= SECY_AN_IDX_MAX; an_index++)
			qca808x_secy_tx_sa_del(port, sc_index, an_index);
	}

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_in_used_get(struct macsec_port *port, u32 sc_index,
					u32 an, bool *p_in_used)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(p_in_used != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_en_get(port, sc_index, an, p_in_used));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_en_set(struct macsec_port *port, u32 sc_index, u32 an,
				    bool enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_en_set(port, sc_index, an, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_en_get(struct macsec_port *port, u32 sc_index, u32 an,
				    bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_en_get(port, sc_index, an, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sak_set(struct macsec_port *port, u32 sc_index,
				  u32 an, struct secy_sak_t *key)
{
	bool enable = FALSE;
	u16 xorkey = 0;
	u32 dev_id;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key != NULL));

	if ((key->len != SAK_LEN_128BITS) &&
		(key->len != SAK_LEN_256BITS) &&
		(key->len != SAK_LEN_LEGACY))
		return ERROR_PARAM;

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.tx_sak_set_calls++;

	if (macsec_get_phyid(port->phydev) == QCA8111_PHY) {
		SHR_RET_ON_ERR(qca81xx_secy_xor_key_get(port,
							&enable, &xorkey));
		if (enable)
			macsec_sak_xor(key, xorkey);
	}

	SHR_RET_ON_ERR(qca808x_secy_tx_sak_set(port, sc_index, an, key));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sak_get(struct macsec_port *port, u32 sc_index,
				  u32 an, struct secy_sak_t *key)
{
	bool enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &enable));
	if (enable) {
		SHR_RET_ON_ERR(qca808x_secy_tx_sak_get(port, sc_index, an, key));
	} else {
		return ERROR_NOT_SUPPORT;
	}
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_an_set(struct macsec_port *port, u32 sc_index, u32 an)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_an_set(port, sc_index, an));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_an_get(struct macsec_port *port, u32 sc_index, u32 *an)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_an_get(port, sc_index, an));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_ki_set(struct macsec_port *port, u32 sc_index, u32 an, struct secy_sa_ki_t *key_idendifier)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key_idendifier != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_ki_set(port, sc_index, an, key_idendifier));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_ki_get(struct macsec_port *port, u32 sc_index, u32 an, struct secy_sa_ki_t *key_idendifier)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(key_idendifier != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_ki_get(port, sc_index, an, key_idendifier));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_ssci_set(struct macsec_port *port, u32 sc_index, u32 ssci)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_ssci_set(port, sc_index, ssci));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	p_macsec_device_cfg[dev_id]->tx_ssci[sc_index] = ssci;
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_ssci_get(struct macsec_port *port, u32 sc_index, u32 *ssci)
{
	u32 dev_id = 0;
	bool enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(ssci != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &enable));
	if (enable) {
		SHR_RET_ON_ERR(qca808x_secy_tx_sc_ssci_get(port, sc_index, ssci));
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		*ssci = p_macsec_device_cfg[dev_id]->tx_ssci[sc_index];
	}
	return ERROR_OK;
}

g_error_t qca_macsec_tx_class_lut_set(struct macsec_port *port, u32 sc_index,
				      struct secy_tx_class_lut_t *pentry)
{
	struct secy_tx_sc_policy_rule_t rule;
	u32 i = 0;
	u32 dev_id = 0;
	bool enable;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(pentry != NULL));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	memset(&rule, 0, sizeof(rule));

	rule.rule_valid = pentry->valid;
	rule.action.tx_sc_index = sc_index;

	for (i = 0; i < 6; i++) {
		rule.mac_da.addr[i] = pentry->da[i];
		rule.mac_sa.addr[i] = pentry->sa[i];
		rule.action.tx_sci.addr[i] = pentry->sci[i];
	}
	rule.action.tx_sci.port = (((u16)(pentry->sci[6])) << 8) |
				   (u16)(pentry->sci[7]);
	rule.ethtype = pentry->ether_type;
	rule.inner_vlanid = pentry->inner_vlanid;
	rule.outer_vlanid = pentry->outer_vlanid;
	rule.bc_flag = pentry->bc_flag;
	rule.action.tx_sc_index = pentry->channel;
	rule.action.tx_tci = pentry->tci;
	rule.action.encryption_offset = pentry->offset;
	rule.rule_mask = pentry->rule_mask;

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_policy_set(port,
			sc_index, &rule));

	if (rule.action.tx_tci & MACSEC_SCB_EN)
		enable = TRUE;
	else
		enable = FALSE;

	SHR_RET_ON_ERR(qca808x_secy_use_scb_en_set(port, enable));

	if (rule.action.tx_tci & MACSEC_INCLUDE_SCI_EN)
		enable = TRUE;
	else
		enable = FALSE;

	SHR_RET_ON_ERR(qca808x_secy_include_sci_en_set(port, enable));

	if (rule.action.tx_tci & MACSEC_ES_EN)
		enable = TRUE;
	else
		enable = FALSE;

	SHR_RET_ON_ERR(qca808x_secy_use_es_en_set(port, enable));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memcpy(&(p_macsec_device_cfg[dev_id]->tx_lut[sc_index]), pentry,
			sizeof(struct secy_tx_class_lut_t));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_class_lut_get(struct macsec_port *port, u32 sc_index,
				      struct secy_tx_class_lut_t *pentry)
{
	struct secy_tx_sc_policy_rule_t rule;
	u32 i = 0;
	u32 dev_id = 0;
	bool enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(pentry != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &enable));
	if (enable) {
		memset(&rule, 0, sizeof(rule));
		SHR_RET_ON_ERR(qca808x_secy_tx_sc_policy_get(port,
			sc_index, &rule));

		pentry->valid = rule.rule_valid;

		for (i = 0; i < 6; i++) {
			pentry->da[i] = rule.mac_da.addr[i];
			pentry->sa[i] = rule.mac_sa.addr[i];
			pentry->sci[i] = rule.action.tx_sci.addr[i];
		}
		pentry->sci[6] = (u8)(rule.action.tx_sci.port >> 8);
		pentry->sci[7] = (u8)(rule.action.tx_sci.port & 0xff);

		pentry->ether_type = rule.ethtype;
		pentry->inner_vlanid = rule.inner_vlanid;
		pentry->outer_vlanid = rule.outer_vlanid;
		pentry->bc_flag = rule.bc_flag;
		pentry->channel = rule.action.tx_sc_index;
		pentry->tci = rule.action.tx_tci;
		pentry->offset = rule.action.encryption_offset;
		pentry->rule_mask = rule.rule_mask;
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		memcpy(pentry, &(p_macsec_device_cfg[dev_id]->tx_lut[sc_index]),
			sizeof(struct secy_tx_class_lut_t));
	}

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_in_used_get(struct macsec_port *port, u32 sc_index,
					bool *enable)
{
	u32 dev_id = 0;
	bool dbg_enable = FALSE;
	bool sc_enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &dbg_enable));

	if (dbg_enable) {
		SHR_RET_ON_ERR(qca808x_secy_tx_sc_en_get(port, sc_index, &sc_enable));
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		sc_enable = p_macsec_device_cfg[dev_id]->tx_lut[sc_index].valid;
	}

	*enable = sc_enable;
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_en_set(struct macsec_port *port, u32 sc_index, bool enable)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_en_zone_set(port, sc_index, enable,
			p_macsec_device_cfg[dev_id]->tx_lut[sc_index].rule_mask));

	p_macsec_device_cfg[dev_id]->tx_lut[sc_index].valid = enable;
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_en_get(struct macsec_port *port, u32 sc_index, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(enable != NULL));

	SHR_RET_ON_ERR(qca_macsec_tx_sc_in_used_get(port, sc_index, enable));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_create(struct macsec_port *port, u32 sc_index,
				  u8 *sci, u32 len)
{
	struct secy_sci_t tx_sci;
	u32 i = 0;
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(sci != NULL) &&
			(len == 8));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.tx_sc_create_calls++;

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	memset(&tx_sci, 0, sizeof(tx_sci));

	for (i = 0; i < 6; i++)
		tx_sci.addr[i] = sci[i];

	tx_sci.port = (((u16)sci[6]) << 8) | ((u16)sci[7]);

	SHR_RET_ON_ERR(qca808x_secy_tx_sci_set(port, sc_index, &tx_sci));

	memcpy(&(p_macsec_device_cfg[dev_id]->tx_lut[sc_index].sci[0]), sci, 8);

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_del(struct macsec_port *port, u32 sc_index)
{
	struct secy_tx_class_lut_t rule;
	u32 dev_id;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	/* Update API statistics */
	macsec_device_debug[dev_id].api_stats.tx_sc_del_calls++;

	memset(&rule, 0, sizeof(rule));

	SHR_RET_ON_ERR(qca_macsec_tx_class_lut_get(port, sc_index, &rule));

	rule.valid = FALSE;
	rule.channel = sc_index;

	SHR_RET_ON_ERR(qca_macsec_tx_class_lut_set(port, sc_index, &rule));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_del_all(struct macsec_port *port)
{
	u32 sc_index = 0;
	struct secy_tx_class_lut_t rule;

	SHR_PARAM_CHECK(port != NULL);

	for (sc_index = 0; sc_index <= SECY_SC_IDX_MAX; sc_index++) {
		memset(&rule, 0, sizeof(rule));
		SHR_RET_ON_ERR(qca_macsec_tx_class_lut_get(port,
				sc_index, &rule));
		rule.valid = FALSE;
		rule.channel = sc_index;

		SHR_RET_ON_ERR(qca_macsec_tx_class_lut_set(port,
				sc_index, &rule));
	}

	return ERROR_OK;
}

g_error_t qca_macsec_tx_class_lut_clear(struct macsec_port *port, u32 sc_index)
{
	struct secy_tx_class_lut_t rule;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	memset(&rule, 0, sizeof(rule));

	SHR_RET_ON_ERR(qca_macsec_tx_class_lut_set(port, sc_index, &rule));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_class_lut_clear_all(struct macsec_port *port)
{
	u32 sc_index = 0;
	struct secy_tx_class_lut_t rule;

	SHR_PARAM_CHECK(port != NULL);

	memset(&rule, 0, sizeof(rule));

	for (sc_index = 0; sc_index < SECY_LUT_MAX; sc_index++)
		SHR_RET_ON_ERR(qca_macsec_tx_class_lut_set(port,
				sc_index, &rule));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_confidentiality_offset_set(struct macsec_port *port, u32 sc_index,
						      enum secy_cofidentiality_offset_t offset)
{
	u32 dev_id = 0;
	u8 encryption_offset = 0;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	if (offset == SECY_CONFIDENTIALITY_OFFSET_50)
		encryption_offset = 50;
	else if (offset == SECY_CONFIDENTIALITY_OFFSET_30)
		encryption_offset = 30;
	else
		encryption_offset = 0;

	SHR_RET_ON_ERR(qca808x_secy_tx_tci_offset_zone_set(port, sc_index,
		p_macsec_device_cfg[dev_id]->tx_lut[sc_index].tci, encryption_offset));

	p_macsec_device_cfg[dev_id]->tx_lut[sc_index].offset = encryption_offset;

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_confidentiality_offset_get(struct macsec_port *port, u32 sc_index,
						      enum secy_cofidentiality_offset_t *offset)
{
	u32 dev_id = 0;
	u8 encryption_offset = 0;
	bool dbg_enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(offset != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &dbg_enable));

	if (dbg_enable) {
		SHR_RET_ON_ERR(qca808x_secy_tx_confidentiality_offset_get(port,
			sc_index, &encryption_offset));
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		encryption_offset = p_macsec_device_cfg[dev_id]->tx_lut[sc_index].offset;
	}

	if (encryption_offset == 50)
		*offset = SECY_CONFIDENTIALITY_OFFSET_50;
	else if (encryption_offset == 30)
		*offset = SECY_CONFIDENTIALITY_OFFSET_30;
	else
		*offset = SECY_CONFIDENTIALITY_OFFSET_00;

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_tci_7_2_set(struct macsec_port *port, u32 sc_index, u8 tci)
{
	u32 dev_id = 0;
	u8 rule_tci = 0;
	bool enable;

	SHR_PARAM_CHECK((port != NULL) && (sc_index <= SECY_SC_IDX_MAX));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	rule_tci = (tci << 0x2);
	SHR_RET_ON_ERR(qca808x_secy_tx_tci_offset_zone_set(port, sc_index,
		rule_tci, p_macsec_device_cfg[dev_id]->tx_lut[sc_index].offset));

	if (rule_tci & MACSEC_SCB_EN)
		enable = TRUE;
	else
		enable = FALSE;

	SHR_RET_ON_ERR(qca808x_secy_use_scb_en_set(port, enable));

	if (rule_tci & MACSEC_INCLUDE_SCI_EN)
		enable = TRUE;
	else
		enable = FALSE;

	SHR_RET_ON_ERR(qca808x_secy_include_sci_en_set(port, enable));

	if (rule_tci & MACSEC_ES_EN)
		enable = TRUE;
	else
		enable = FALSE;

	SHR_RET_ON_ERR(qca808x_secy_use_es_en_set(port, enable));

	p_macsec_device_cfg[dev_id]->tx_lut[sc_index].tci = rule_tci;

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_tci_7_2_get(struct macsec_port *port, u32 sc_index, u8 *tci)
{
	u32 dev_id = 0;
	u8 rule_tci = 0;
	bool dbg_enable = FALSE;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(tci != NULL));

	SHR_RET_ON_ERR(qca808x_secy_sram_dbg_en_get(port, &dbg_enable));

	if (dbg_enable) {
		SHR_RET_ON_ERR(qca808x_secy_tx_tci_get(port, sc_index, &rule_tci));
	} else {
		dev_id = macsec_get_device_id(port->phydev);
		if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
			osal_print("%s: fail to find dev_id! \n", __func__);
			return ERROR_NOT_FOUND;
		}
		rule_tci = p_macsec_device_cfg[dev_id]->tx_lut[sc_index].tci;
	}

	*tci = (rule_tci >> 0x2);

	return ERROR_OK;
}

g_error_t qca_macsec_tx_protect_frames_en_set(struct macsec_port *port, bool enable)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca808x_secy_tx_protect_frames_en_set(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_protect_frames_en_get(struct macsec_port *port, bool *enable)
{
	SHR_PARAM_CHECK((port != NULL) && (enable != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_protect_frames_en_get(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_mib_get(struct macsec_port *port, u32 sc_index,
				     u32 an, struct secy_tx_sa_mib_t *mib)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX) &&
			(mib != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_mib_get(port, sc_index, an, mib));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_mib_get(struct macsec_port *port, u32 sc_index,
				     struct secy_tx_sc_mib_t *mib)
{
	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(mib != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_mib_get(port, sc_index, mib));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_mib_get(struct macsec_port *port, struct secy_tx_mib_t *mib)
{
	SHR_PARAM_CHECK((port != NULL) && (mib != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_mib_get(port, mib));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_sa_mib_clear(struct macsec_port *port, u32 sc_index, u32 an)
{
	struct secy_tx_sa_mib_t mib;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX) &&
			(an <= SECY_AN_IDX_MAX));

	memset(&mib, 0, sizeof(mib));

	SHR_RET_ON_ERR(qca808x_secy_tx_sa_mib_get(port, sc_index, an, &mib));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_sc_mib_clear(struct macsec_port *port, u32 sc_index)
{
	struct secy_tx_sc_mib_t mib;

	SHR_PARAM_CHECK((port != NULL) &&
			(sc_index <= SECY_SC_IDX_MAX));

	memset(&mib, 0, sizeof(mib));

	SHR_RET_ON_ERR(qca808x_secy_tx_sc_mib_get(port, sc_index, &mib));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_mib_clear(struct macsec_port *port)
{
	struct secy_tx_mib_t mib;

	SHR_PARAM_CHECK(port != NULL);

	memset(&mib, 0, sizeof(mib));

	SHR_RET_ON_ERR(qca808x_secy_tx_mib_get(port, &mib));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_ctl_filt_set(struct macsec_port *port, u32 index,
				     struct secy_ctl_filt_t *secy_filt)
{
	g_error_t rv = ERROR_OK;
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(index == 0) &&
			(secy_filt != NULL));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	switch (secy_filt->match_type) {
	case SECY_CTL_COMPARE_DA:
		rv = macsec_tx_ctl_filt_da_mac_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_SA:
		rv = macsec_tx_ctl_filt_sa_mac_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_ETHER_TYPE:
		rv = macsec_tx_ctl_filt_ether_type_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_HALF_DA_SA:
		rv = macsec_tx_ctl_filt_half_da_sa_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_DA_ETHER_TYPE:
		rv = macsec_tx_ctl_filt_da_ether_type_set(port, index, secy_filt);
		break;
	case SECY_CTL_COMPARE_SA_ETHER_TYPE:
		rv = macsec_tx_ctl_filt_sa_ether_type_set(port, index, secy_filt);
		break;
	default:
		rv = ERROR_PARAM;
	}
	if (rv != ERROR_OK)
		return rv;

	SHR_RET_ON_ERR(qca808x_secy_tx_ctl_filt_en_set(port, secy_filt->bypass));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memcpy(&(p_macsec_device_cfg[dev_id]->tx_ctl_filt), secy_filt,
			sizeof(struct secy_ctl_filt_t));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_ctl_filt_get(struct macsec_port *port, u32 index,
				       struct secy_ctl_filt_t *secy_filt)
{
	u32 dev_id = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(index == 0) &&
			(secy_filt != NULL));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memcpy(secy_filt, &(p_macsec_device_cfg[dev_id]->tx_ctl_filt),
		sizeof(struct secy_ctl_filt_t));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_ctl_filt_clear(struct macsec_port *port, u32 index)
{
	u32 i = 0;
	u32 dev_id = 0;
	struct secy_udf_filt_t filter;

	SHR_PARAM_CHECK((port != NULL) && (index == 0));

	if (macsec_guard_clk_skip(port))
		return ERROR_OK;

	memset(&filter, 0, sizeof(filter));

	SHR_RET_ON_ERR(qca808x_secy_tx_ctl_filt_en_set(port, FALSE));

	for (i = 0; i < 4; i++)
		SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, i, &filter));

	dev_id = macsec_get_device_id(port->phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id! \n", __func__);
		return ERROR_NOT_FOUND;
	}
	memset(&(p_macsec_device_cfg[dev_id]->tx_ctl_filt), 0,
			sizeof(struct secy_ctl_filt_t));

	return ERROR_OK;
}

g_error_t qca_macsec_tx_ctl_filt_clear_all(struct macsec_port *port)
{
	SHR_PARAM_CHECK(port != NULL);

	SHR_RET_ON_ERR(qca_macsec_tx_ctl_filt_clear(port, 0));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_udf_filt_set(struct macsec_port *port, u32 index,
				       struct secy_udf_filt_t *filter)
{
	SHR_PARAM_CHECK((port != NULL) && (filter != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_set(port, index, filter));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_udf_filt_get(struct macsec_port *port, u32 index,
				       struct secy_udf_filt_t *filter)
{
	SHR_PARAM_CHECK((port != NULL) && (filter != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_filt_get(port, index, filter));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_udf_ufilt_cfg_set(struct macsec_port *port,
					    struct secy_udf_filt_cfg_t *cfg)
{
	SHR_PARAM_CHECK((port != NULL) && (cfg != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_ufilt_cfg_set(port, cfg));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_udf_ufilt_cfg_get(struct macsec_port *port,
					    struct secy_udf_filt_cfg_t *cfg)
{
	SHR_PARAM_CHECK((port != NULL) && (cfg != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_ufilt_cfg_get(port, cfg));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_udf_cfilt_cfg_set(struct macsec_port *port,
					    struct secy_udf_filt_cfg_t *cfg)
{
	SHR_PARAM_CHECK((port != NULL) && (cfg != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_cfilt_cfg_set(port, cfg));
	return ERROR_OK;
}

g_error_t qca_macsec_tx_udf_cfilt_cfg_get(struct macsec_port *port,
					    struct secy_udf_filt_cfg_t *cfg)
{
	SHR_PARAM_CHECK((port != NULL) && (cfg != NULL));

	SHR_RET_ON_ERR(qca808x_secy_tx_udf_cfilt_cfg_get(port, cfg));
	return ERROR_OK;
}

g_error_t qca_macsec_mib_saturate_en_set(struct macsec_port *port, bool enable)
{
	u32 phyid = 0;

	SHR_PARAM_CHECK(port != NULL);

	phyid = macsec_get_phyid(port->phydev);

	if (!macsec_mib_saturate_supported(phyid))
		return ERROR_NOT_SUPPORT;

	SHR_RET_ON_ERR(qca81xx_secy_mib_saturate_en_set(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_mib_saturate_en_get(struct macsec_port *port, bool *enable)
{
	u32 phyid = 0;

	SHR_PARAM_CHECK((port != NULL) &&
			(enable != NULL));

	phyid = macsec_get_phyid(port->phydev);

	if (!macsec_mib_saturate_supported(phyid))
		return ERROR_NOT_SUPPORT;

	SHR_RET_ON_ERR(qca81xx_secy_mib_saturate_en_get(port, enable));
	return ERROR_OK;
}

g_error_t qca_macsec_xor_key_set(struct macsec_port *port,
				    bool enable, u16 key)
{
	SHR_PARAM_CHECK(port != NULL);

	if (macsec_get_phyid(port->phydev) != QCA8111_PHY)
		return ERROR_NOT_SUPPORT;

	SHR_RET_ON_ERR(qca81xx_secy_xor_key_set(port, enable, key));
	return ERROR_OK;
}

int qca_macsec_device_attach(void *dev)
{
	u32 dev_id = 0;
	struct net_device *netdev;
	struct phy_device *phydev;
	const char *drv_name = "unknown";

	if (NULL == dev)
		return -EINVAL;

	netdev = (struct net_device *)dev;

	if (NULL == netdev->phydev)
		return -EINVAL;

	phydev = netdev->phydev;

	dev_id = macsec_device_register(phydev);
	if (INVALID_DEVICE_ID == dev_id) {
		osal_print("device:%p fail to register!\n", phydev);
		return -ENODEV;
	}

	/* Set device to initial state - device allocated but not fully initialized */
	macsec_device_debug[dev_id].device_state = MACSEC_INIT;

	/* Store interface name for debugging */
	strlcpy(macsec_device_debug[dev_id].interface_name, netdev->name,
		sizeof(macsec_device_debug[dev_id].interface_name));

	/* Save driver name safely for debugging */
	if (phydev && phydev->drv && phydev->drv->name)
		drv_name = phydev->drv->name;
	strlcpy(macsec_device_debug[dev_id].drv_name, drv_name,
		sizeof(macsec_device_debug[dev_id].drv_name));

	nss_macsec_mutex_lock();
	p_macsec_device_cfg[dev_id] = devm_kzalloc(&(phydev->mdio.dev),
				sizeof(struct qca_macsec_device_t), GFP_KERNEL);
	nss_macsec_mutex_unlock();
	if (!p_macsec_device_cfg[dev_id]) {
		macsec_device_debug[dev_id].device_state =
						MACSEC_MEMORY_ALLOC_FAILURE;
		return -ENOMEM;
	}

	p_macsec_device_cfg[dev_id]->port.phydev = phydev;
	p_macsec_device_cfg[dev_id]->port.type = MACSEC_IN_PHY;

	/* Hardware init is deferred to the first MACsec enable. */
	macsec_device_debug[dev_id].device_state = MACSEC_SUCCESS;

	return 0;
}

int qca_macsec_device_detach(void *dev)
{
	u32 dev_id = 0;
	struct net_device *netdev;
	struct phy_device *phydev;
	int ret;

	if (NULL == dev)
		return -EINVAL;

	netdev = (struct net_device *)dev;

	if (NULL == netdev->phydev)
		return -EINVAL;

	phydev = netdev->phydev;

	dev_id = macsec_get_device_id(phydev);
	if (unlikely(dev_id >= MACSEC_DEVICE_MAXNUM)) {
		osal_print("%s: fail to find dev_id for device:%p! \n",
			   __func__, phydev);
		return -ENODEV;
	}

	/* Check if device configuration exists before accessing */
	if (!p_macsec_device_cfg[dev_id]) {
		osal_print("%s: device configuration is NULL for dev_id:%d! \n",
			   __func__, dev_id);
		return -EINVAL;
	}

	ret = qca808x_secy_clock_en_set(&(p_macsec_device_cfg[dev_id]->port),
					FALSE);
	if (ret) {
		macsec_device_debug[dev_id].device_state =
						MACSEC_UNREGISTER_FAILURE;
		return ret;
	}

	/* Critical section: protect device pointer modification */
	nss_macsec_mutex_lock();
	devm_kfree(&(phydev->mdio.dev), p_macsec_device_cfg[dev_id]);
	p_macsec_device_cfg[dev_id] = NULL;
	nss_macsec_mutex_unlock();

	/* Device detach successful */
	macsec_device_debug[dev_id].device_state = MACSEC_DETACHED;

	return 0;
}

int qca_macsec_get_device_count(void)
{
	int count = 0;
	int i;

	for (i = 0; i < MACSEC_DEVICE_MAXNUM; i++) {
		if (p_macsec_device_cfg[i] != NULL) {
			count++;
		}
	}
	return count;
}

void qca_macsec_device_cleanup(void)
{
	u32 dev_id;

	for (dev_id = 0; dev_id < MACSEC_DEVICE_MAXNUM; dev_id++) {
		if (p_macsec_device_cfg[dev_id] != NULL) {
			qca808x_secy_clock_en_set(&(p_macsec_device_cfg[dev_id]->port),
				FALSE);
			devm_kfree(&(p_macsec_device_cfg[dev_id]->port.phydev->mdio.dev),
				p_macsec_device_cfg[dev_id]);
			p_macsec_device_cfg[dev_id] = NULL;
		}
	}
}

/**
 * qca_macsec_get_device_data - Get device configuration data safely
 * @dev_id: Device ID to query
 * @device_copy: Pointer to buffer for device data copy
 *
 * This function provides safe access to device configuration data by
 * creating a copy under mutex protection. This eliminates the need for
 * extern declarations in other files.
 *
 * Return: 0 on success, negative error code on failure
 */
int qca_macsec_get_device_data(u32 dev_id,
				struct qca_macsec_device_t *device_copy)
{
	if (dev_id >= MACSEC_DEVICE_MAXNUM || !device_copy)
		return -EINVAL;

	nss_macsec_mutex_lock();
	if (p_macsec_device_cfg[dev_id] == NULL) {
		nss_macsec_mutex_unlock();
		return -ENODEV;
	}

	memcpy(device_copy, p_macsec_device_cfg[dev_id],
	       sizeof(struct qca_macsec_device_t));
	nss_macsec_mutex_unlock();
	return 0;
}
