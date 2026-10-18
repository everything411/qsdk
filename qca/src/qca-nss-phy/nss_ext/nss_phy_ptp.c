/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "nss_phy.h"
#include "nss_phy_linux_wrapper.h"
#include "nss_phy_ptp.h"
#include "nss_phy_ptp_api.h"
#include "nss_phy_ptp_reg.h"

#define QCA808X_MMD7_CLOCK_CTRL_REG        0x8072
#define QCA808X_DIGITAL_PHY_SYNCE_CLOCK_EN BIT(0)	/* enable synce clock manually */
#define QCA808X_SYNCE_CLK_SEL_EN           BIT(1)	/* enable synce clock when link up */

#define QCA808X_MMD1_SYNCE_CTRL            0x2000
#define QCA808X_SYNCE_CLK_SEL_MASK         BITS(0, 6)
#define QCA808X_SYNCE_CLK_CH0_SEL          0x20

static int nss_phy_ptp_config_set(struct nss_phy_device *nss_phydev,
		fal_ptp_config_t *config)
{
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};
	union ptp_backup_reg_u ptp_backup_reg = {0};

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	nss_ptp_backup_reg_get(nss_phydev, &ptp_backup_reg);

	if (config->ptp_en == true) {
		ptp_main_conf_reg.bf.ptp_bypass = false;
		ptp_main_conf_reg.bf.disable_1588_phy = false;
	} else {
		ptp_main_conf_reg.bf.ptp_bypass = true;
		ptp_main_conf_reg.bf.disable_1588_phy = true;
	}

	if (config->clock_mode == FAL_OC_CLOCK_MODE ||
			config->clock_mode == FAL_BC_CLOCK_MODE) {
		if (config->step_mode == FAL_ONE_STEP_MODE)
			ptp_main_conf_reg.bf.ptp_clock_mode =
				PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_OC_ONE_STEP;
		else
			ptp_main_conf_reg.bf.ptp_clock_mode =
				PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_OC_TWO_STEP;
	} else {
		if (config->clock_mode == FAL_P2PTC_CLOCK_MODE)
			ptp_backup_reg.bf.p2p_tc_en = true;
		else
			ptp_backup_reg.bf.p2p_tc_en = false;

		if (config->step_mode == FAL_ONE_STEP_MODE &&
		    ptp_misc_config_reg.bf.embed_ingress_time_en == true)
			ptp_misc_config_reg.bf.tc_offload = true;
		else
			ptp_misc_config_reg.bf.tc_offload = false;

		if (config->step_mode == FAL_ONE_STEP_MODE)
			ptp_main_conf_reg.bf.ptp_clock_mode =
				PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_TC_ONE_STEP;
		else
			ptp_main_conf_reg.bf.ptp_clock_mode =
				PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_TC_TWO_STEP;
	}

	if (config->step_mode == FAL_AUTO_MODE)
		ptp_misc_config_reg.bf.pkt_one_step_en = true;
	else
		ptp_misc_config_reg.bf.pkt_one_step_en = false;

#if defined(IN_LINUX_STD_PTP)
	nss_ptp_clock_mode_config(nss_phydev, config->clock_mode, config->step_mode);
#endif
	nss_ptp_main_conf_reg_set(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_misc_config_reg_set(nss_phydev, &ptp_misc_config_reg);
	nss_ptp_backup_reg_set(nss_phydev, &ptp_backup_reg);

	return 0;
}

static int nss_phy_ptp_config_get(struct nss_phy_device *nss_phydev,
		fal_ptp_config_t *config)
{
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};
	union ptp_backup_reg_u ptp_backup_reg = {0};

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	nss_ptp_backup_reg_get(nss_phydev, &ptp_backup_reg);

	if (ptp_main_conf_reg.bf.ptp_bypass == false &&
			ptp_main_conf_reg.bf.disable_1588_phy == false)
		config->ptp_en = true;

	if (ptp_main_conf_reg.bf.ptp_clock_mode ==
			PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_OC_TWO_STEP)
	{
		config->clock_mode = FAL_OC_CLOCK_MODE;
		config->step_mode = FAL_TWO_STEP_MODE;
	}
	else if (ptp_main_conf_reg.bf.ptp_clock_mode ==
			PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_OC_ONE_STEP)
	{
		config->clock_mode = FAL_OC_CLOCK_MODE;
		config->step_mode = FAL_ONE_STEP_MODE;
	}
	else if (ptp_main_conf_reg.bf.ptp_clock_mode ==
			PTP_MAIN_CONF_REG_PTP_CLOCK_MODE_TC_TWO_STEP)
	{
		if (ptp_backup_reg.bf.p2p_tc_en == true)
		{
			config->clock_mode = FAL_P2PTC_CLOCK_MODE;
		}
		else
		{
			config->clock_mode = FAL_E2ETC_CLOCK_MODE;
		}

		config->step_mode = FAL_TWO_STEP_MODE;
	}
	else
	{
		if (ptp_backup_reg.bf.p2p_tc_en == true)
		{
			config->clock_mode = FAL_P2PTC_CLOCK_MODE;
		}
		else
		{
			config->clock_mode = FAL_E2ETC_CLOCK_MODE;
		}

		config->step_mode = FAL_ONE_STEP_MODE;
	}
	if (ptp_misc_config_reg.bf.pkt_one_step_en == true)
	{
		config->step_mode = FAL_AUTO_MODE;
	}

	return 0;
}

static int nss_phy_ptp_reference_clock_set(struct nss_phy_device *nss_phydev,
		fal_ptp_reference_clock_t ref_clock)
{
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};
	union ptp_rtc_clk_reg_u ptp_rtc_clk_reg = {0};

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_rtc_clk_reg_get(nss_phydev, &ptp_rtc_clk_reg);

	if (ref_clock == FAL_REF_CLOCK_LOCAL)
	{
		ptp_main_conf_reg.bf.ptp_clk_sel = false;
		ptp_rtc_clk_reg.bf.rtc_clk_selection = false;
	}
	else if (ref_clock == FAL_REF_CLOCK_SYNCE)
	{
		ptp_main_conf_reg.bf.ptp_clk_sel = true;
		ptp_rtc_clk_reg.bf.rtc_clk_selection = false;
	}
	else
	{
		ptp_main_conf_reg.bf.ptp_clk_sel = false;
		ptp_rtc_clk_reg.bf.rtc_clk_selection = true;
	}

	nss_ptp_main_conf_reg_set(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_rtc_clk_reg_set(nss_phydev, &ptp_rtc_clk_reg);

	return 0;
}

static int
nss_phy_ptp_reference_clock_get(struct nss_phy_device *nss_phydev,
		fal_ptp_reference_clock_t *ref_clock)
{
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};
	union ptp_rtc_clk_reg_u ptp_rtc_clk_reg = {0};

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_rtc_clk_reg_get(nss_phydev, &ptp_rtc_clk_reg);
	if (ptp_main_conf_reg.bf.ptp_clk_sel == false &&
			ptp_rtc_clk_reg.bf.rtc_clk_selection == false)
	{
		*ref_clock = FAL_REF_CLOCK_LOCAL;
	}
	else if (ptp_main_conf_reg.bf.ptp_clk_sel == true &&
			ptp_rtc_clk_reg.bf.rtc_clk_selection == false)
	{
		*ref_clock = FAL_REF_CLOCK_SYNCE;
	}
	else
	{
		*ref_clock = FAL_REF_CLOCK_EXTERNAL;
	}

	return 0;
}

static int
nss_phy_ptp_rx_timestamp_mode_set(struct nss_phy_device *nss_phydev,
		fal_ptp_rx_timestamp_mode_t ts_mode)
{
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	if (ts_mode == FAL_RX_TS_MDIO)
	{
		ptp_main_conf_reg.bf.ts_attach_mode = false;
		ptp_main_conf_reg.bf.ipv6_embed_force_checksum_zero = false;
		ptp_misc_config_reg.bf.embed_ingress_time_en = false;
		ptp_misc_config_reg.bf.cf_from_pkt_en = true;
	}
	else
	{
		ptp_main_conf_reg.bf.ts_attach_mode = false;
		ptp_main_conf_reg.bf.ipv6_embed_force_checksum_zero = true;
		ptp_misc_config_reg.bf.embed_ingress_time_en = true;
		ptp_misc_config_reg.bf.cf_from_pkt_en = false;
	}
	nss_ptp_main_conf_reg_set(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_misc_config_reg_set(nss_phydev, &ptp_misc_config_reg);

	return 0;
}

static int
nss_phy_ptp_rx_timestamp_mode_get(struct nss_phy_device *nss_phydev,
		fal_ptp_rx_timestamp_mode_t *ts_mode)
{
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	if (ptp_main_conf_reg.bf.ts_attach_mode == false &&
			ptp_misc_config_reg.bf.embed_ingress_time_en == false)
	{
		*ts_mode = FAL_RX_TS_MDIO;
	}
	else if (ptp_main_conf_reg.bf.ts_attach_mode == false &&
			ptp_misc_config_reg.bf.embed_ingress_time_en == true)
	{
		*ts_mode = FAL_RX_TS_EMBED;
	}
	else
	{
		return -EINVAL;
	}

	return 0;
}

#if defined(MHT)
/*
 * Select the synce clock source from ADC clock channel0.
 * Enable the synce clock out enable when the link is up.
 * Only available for QCA8084
 */
static int
_nss_phy_ptp_synce_clock_set(struct nss_phy_device *nss_phydev, bool enable)
{
	int rv = 0;
	u16 data0 = 0, data1 = 0;

	if (enable) {
		data0 = QCA808X_SYNCE_CLK_SEL_EN;
		data1 = QCA808X_SYNCE_CLK_CH0_SEL;
	}

	rv = nss_phy_modify_mmd(nss_phydev, QCA808X_PHY_MMD7_NUM,
			QCA808X_MMD7_CLOCK_CTRL_REG, QCA808X_SYNCE_CLK_SEL_EN, data0);
	if (rv)
		return rv;

	rv = nss_phy_modify_mmd(nss_phydev, QCA808X_PHY_MMD1_NUM,
			QCA808X_MMD1_SYNCE_CTRL, QCA808X_SYNCE_CLK_SEL_MASK, data1);

	return rv;
}

static int
_nss_phy_ptp_synce_clock_get(struct nss_phy_device *nss_phydev, bool *enable)
{
	int rv = 0;
	u16 data = 0;

	data = nss_phy_read_mmd(nss_phydev, QCA808X_PHY_MMD7_NUM, QCA808X_MMD7_CLOCK_CTRL_REG);

	if (data & QCA808X_SYNCE_CLK_SEL_EN)
		*enable = true;
	else
		*enable = false;

	return rv;
}

#define RTC_SRC_INVALID_ID	0xff
u32 g_rtc_src_id[SSDK_PHYSICAL_PORT4] = {0};

static inline int _nss_phy_mht_port_id_convert(struct nss_phy_device *nss_phydev,
		u32 *port_id, u32 *mht_port_id, bool to_mht_port)
{
	int rv = 0;
	u32 phy_addr = 0;

	if (!port_id || !mht_port_id)
		return -EINVAL;

	if (to_mht_port) {
		rv = hsl_port_prop_get_phyid(nss_phydev, *port_id, &phy_addr);
		if (rv)
			return rv;

		rv = qca_mht_port_id_get(nss_phydev, phy_addr, mht_port_id);
		if (rv)
			return rv;
	} else {
		rv = qca_mht_ephy_addr_get(nss_phydev, *mht_port_id, &phy_addr);
		if (rv)
			return rv;

		*port_id = qca_ssdk_phy_addr_to_port(nss_phydev, phy_addr);
	}

	return rv;
}

static int
nss_phy_ptp_rtc_sync_get(struct nss_phy_device *nss_phydev,
		fal_ptp_rtc_src_type_t *src_type, u32 *src_id)
{
	int rv = 0;
	u32 mht_port_id = 0, mht_port_src_id = 0, src_phy_addr = 0;
	bool sync_en = false, rtc_en = A_FALSE;

	rv = qca_mht_port_id_get(nss_phydev, &mht_port_id);
	if (rv)
		return rv;

	if (mht_port_id >= SSDK_PHYSICAL_PORT1 && mht_port_id <= SSDK_PHYSICAL_PORT4)
		*src_id = g_rtc_src_id[mht_port_id - 1];
	else
		return -EINVAL;

	if (*src_id == RTC_SRC_INVALID_ID) {
		*src_type = FAL_PTP_RTC_SRC_DIS;
		return rv;
	}

	if (*src_id == SSDK_PHYSICAL_PORT0) {
		*src_type = FAL_PTP_RTC_SRC_EXT;
		mht_port_src_id = SSDK_PHYSICAL_PORT0;
	} else {
		/*
		 * Sanity check whether src_id is mht port id 1-4,
		 * otherwise return error.
		 */
		rv = _nss_phy_mht_port_id_convert(nss_phydev, src_id, &mht_port_src_id, true);
		if (rv)
			return rv;
	}

	/* Check RTC source is pre_port or mht_port */
	rv = qca_mht_ptp_sync_get(nss_phydev, mht_port_id, &sync_en);
	if (rv)
		return rv;

	if (sync_en == true) {
		*src_type = FAL_PTP_RTC_SRC_PRE_PORT;
	} else {
		/* Get the RTC source id, which is mht_port or external */
		qca_mht_ptp_async_get(nss_phydev, mht_port_id, &mht_port_src_id);
		*src_type = FAL_PTP_RTC_SRC_MHT_PORT;
	}

	if (mht_port_src_id == SSDK_PHYSICAL_PORT0) {
		*src_type = FAL_PTP_RTC_SRC_EXT;
		*src_id = SSDK_PHYSICAL_PORT0;
	} else {
		/* Update src_id according to the current mht_port_src_id */
		rv = _nss_phy_mht_port_id_convert(nss_phydev, src_id, &mht_port_src_id, false);
		if (rv)
			return rv;

		rv = qca_mht_ephy_addr_get(nss_phydev, mht_port_src_id, &src_phy_addr);
		if (rv)
			return rv;

		rv = _nss_phy_ptp_synce_clock_get(nss_phydev, src_phy_addr, &rtc_en);
		if (rv)
			return rv;

		if (rtc_en == false) {
			*src_type = FAL_PTP_RTC_SRC_DIS;
			*src_id = RTC_SRC_INVALID_ID;
		}
	}

	return rv;
}

static int
nss_phy_ptp_rtc_sync_set(struct nss_phy_device *nss_phydev,
		fal_ptp_rtc_src_type_t src_type, u32 src_id)
{
	int rv = 0;
	u32 mht_port_id = 0, mht_port_src_id = 0, src_phy_addr = 0;
	bool rtc_en = false;

	rv = qca_mht_port_id_get(nss_phydev, &mht_port_id);
	if (rv)
		return rv;

	switch (src_type) {
		case FAL_PTP_RTC_SRC_PRE_PORT:
			/* src_id is the adjacent port */
			switch (mht_port_id) {
				case SSDK_PHYSICAL_PORT1:
					mht_port_src_id = SSDK_PHYSICAL_PORT4;
					break;
				case SSDK_PHYSICAL_PORT2:
				case SSDK_PHYSICAL_PORT3:
				case SSDK_PHYSICAL_PORT4:
					mht_port_src_id = mht_port_id - 1;
					break;
				default:
					SSDK_ERROR("Unsupported mht port ID: %d\n", mht_port_id);
					return -EINVAL;
			}

			rv = _nss_phy_mht_port_id_convert(nss_phydev, &src_id,
					&mht_port_src_id, false);
			if (rv)
				return rv;

			rv = qca_mht_ptp_sync_set(nss_phydev, mht_port_id, true);
			if (rv)
				return rv;

			g_rtc_src_id[mht_port_id - 1] = src_id;
			rtc_en = true;
			break;
		case FAL_PTP_RTC_SRC_MHT_PORT:
			/*
			 * A valid src_id need to be provided, src_id need to be updated
			 * as the MHT port id.
			 */
			rv = _nss_phy_mht_port_id_convert(nss_phydev, &src_id,
					&mht_port_src_id, true);
			if (rv)
				return rv;

			rv = qca_mht_ptp_async_set(nss_phydev, mht_port_id, mht_port_src_id);
			if (rv)
				return rv;

			g_rtc_src_id[mht_port_id - 1] = src_id;
			rtc_en = true;
			break;
		case FAL_PTP_RTC_SRC_EXT:
			/* src_id is the external clock source from PAD */
			src_id = SSDK_PHYSICAL_PORT0;
			mht_port_src_id = SSDK_PHYSICAL_PORT0;

			rv = qca_mht_ptp_async_set(nss_phydev, mht_port_id, mht_port_src_id);
			if (rv)
				return rv;

			g_rtc_src_id[mht_port_id - 1] = src_id;
			break;
		case FAL_PTP_RTC_SRC_DIS:
			/*
			 * A valid src_id need to be provided, src_id need to be updated
			 * as the MHT port id.
			 */
			rv = _nss_phy_mht_port_id_convert(nss_phydev, &src_id,
					&mht_port_src_id, true);
			if (rv)
				return rv;

			rv = qca_mht_ptp_sync_set(nss_phydev, mht_port_id, false);
			if (rv)
				return rv;

			g_rtc_src_id[mht_port_id - 1] = RTC_SRC_INVALID_ID;
			rtc_en = false;
			break;
		default:
			SSDK_ERROR("Unsupported RTC source: %d\n", src_type);
			return -EINVAL;
	}

	switch (mht_port_src_id) {
		case SSDK_PHYSICAL_PORT1:
		case SSDK_PHYSICAL_PORT2:
		case SSDK_PHYSICAL_PORT3:
		case SSDK_PHYSICAL_PORT4:
			rv = qca_mht_ephy_addr_get(nss_phydev, src_id, &src_phy_addr);
			if (rv)
				return rv;

			rv = _nss_phy_ptp_synce_clock_set(nss_phydev, src_phy_addr, rtc_en);
			if (rv)
				return rv;
			break;
		default:
			break;
	}

	return rv;
}
#endif

static int
nss_phy_ptp_v2p1_pkt_info_get(struct nss_phy_device *nss_phydev, ptp_ts_type_t ts_type,
		fal_ptp_pkt_info_t *pkt_info)
{
	int rv = 0;
	union ptp_version_reg_u ptp_ver = {0};
	union ptp_msg_type_spec0_reg_u msg_spec0 = {0};
	union ptp_msg_type_spec1_reg_u msg_spec1 = {0};
	union ptp_domain_number_reg_u domain = {0};

	rv = nss_ptp_version_reg_get(nss_phydev, ts_type, &ptp_ver);
	if (rv)
		return rv;

	rv = nss_ptp_msg_type_spec0_reg(nss_phydev, ts_type, &msg_spec0);
	if (rv)
		return rv;

	rv = nss_ptp_msg_type_spec1_reg(nss_phydev, ts_type, &msg_spec1);
	if (rv)
		return rv;

	rv = nss_ptp_domain_number_reg(nss_phydev, ts_type, &domain);
	if (rv)
		return rv;

	pkt_info->domain_number = domain.bf.id;
	pkt_info->minor_ver = ptp_ver.bf.minor_version_ptp;
	pkt_info->major_sdoid = ptp_ver.bf.major_sdoid;
	pkt_info->minor_sdoid = ptp_ver.bf.minor_sdoid;
	pkt_info->msgtype_spec = msg_spec0.bf.bit31_16 << 16 | msg_spec1.bf.bit15_0;

	return 0;
}

static int
nss_phy_ptp_ts_get(struct nss_phy_device *nss_phydev, ptp_ts_type_t ts_type,
		fal_ptp_pkt_info_t *pkt_info, fal_ptp_time_t *time)
{
	int rv = 0;
	u16 seqid = 0, port_num = 0, msgtype = 0;
	u64 clock_id = 0;

	rv = nss_ptp_seqid_get(nss_phydev, ts_type, &seqid);
	if (rv)
		return rv;

	if (pkt_info->msg_type != FAL_PTP_MSG_INVALID &&
			pkt_info->sequence_id != seqid)
		return -EINVAL;

	rv = nss_ptp_portid_get(nss_phydev, ts_type, &clock_id, &port_num);
	if (rv)
		return rv;

	rv = nss_ptp_msg_type_get(nss_phydev, ts_type, &msgtype);
	if (rv)
		return rv;

	if ((pkt_info->msg_type == FAL_PTP_MSG_INVALID) ||
			(pkt_info->clock_identify == clock_id &&
			 pkt_info->port_number == port_num &&
			 pkt_info->msg_type == msgtype)) {
		rv = nss_ptp_ts_get(nss_phydev, ts_type,
				&time->seconds, &time->nanoseconds, &time->fracnanoseconds);
		if (rv)
			return rv;

		pkt_info->sequence_id = seqid;
		pkt_info->clock_identify = clock_id;
		pkt_info->port_number = port_num;
		pkt_info->msg_type = msgtype;

		if (nss_phydev_id_compare(nss_phydev->phydev, QCA8084_PHY, GENMASK(31, 4)))
			rv = nss_phy_ptp_v2p1_pkt_info_get(nss_phydev, ts_type, pkt_info);

		return rv;
	}

	return -EINVAL;
}

static int
nss_phy_ptp_timestamp_get(struct nss_phy_device *nss_phydev,
		fal_ptp_direction_t direction,
		fal_ptp_pkt_info_t *pkt_info, fal_ptp_time_t *time)
{
	int ret = -EINVAL;
	ptp_ts_type_t seq = PTP_TS_RX0, seq_max = PTP_TS_TX0;

	if (direction == FAL_RX_DIRECTION) {
		/* polling all timestamps */
		if (pkt_info->msg_type == FAL_PTP_MSG_INVALID) {
			seq = pkt_info->sequence_id;
			seq_max = seq + 1;
		}

		while (seq < seq_max) {
			ret = nss_phy_ptp_ts_get(nss_phydev,
					seq, pkt_info, time);
			if (ret == 0)
				return ret;
			seq++;
		}
	} else {
		/* sequence_id can only be 0 for polling requirement */
		if (pkt_info->msg_type == FAL_PTP_MSG_INVALID && pkt_info->sequence_id != 0) {
			return -EINVAL;
		}

		ret = nss_phy_ptp_ts_get(nss_phydev,
				PTP_TS_TX0, pkt_info, time);
	}

	return ret;
}

static int
nss_phy_ptp_pkt_timestamp_set(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_in_trig0_reg_u ptp_in_trig0_reg = {0};
	union ptp_in_trig1_reg_u ptp_in_trig1_reg = {0};
	union ptp_in_trig2_reg_u ptp_in_trig2_reg = {0};
	union ptp_in_trig3_reg_u ptp_in_trig3_reg = {0};

	nss_ptp_in_trig0_reg_get(nss_phydev, &ptp_in_trig0_reg);
	ptp_in_trig0_reg.bf.ptp_in_trig_nisec = time->nanoseconds >> 16;
	nss_ptp_in_trig0_reg_set(nss_phydev, &ptp_in_trig0_reg);

	nss_ptp_in_trig1_reg_get(nss_phydev, &ptp_in_trig1_reg);
	ptp_in_trig1_reg.bf.ptp_in_trig_nisec = time->nanoseconds & 0xffff;
	nss_ptp_in_trig1_reg_set(nss_phydev, &ptp_in_trig1_reg);

	nss_ptp_in_trig2_reg_get(nss_phydev, &ptp_in_trig2_reg);
	ptp_in_trig2_reg.bf.ptp_in_trig_nisec = time->fracnanoseconds >> 4;
	nss_ptp_in_trig2_reg_set(nss_phydev, &ptp_in_trig2_reg);

	nss_ptp_in_trig3_reg_get(nss_phydev, &ptp_in_trig3_reg);
	ptp_in_trig3_reg.bf.ptp_in_trig_nisec = time->fracnanoseconds & 0xf;
	nss_ptp_in_trig3_reg_set(nss_phydev, &ptp_in_trig3_reg);

	return 0;
}

static int
nss_phy_ptp_pkt_timestamp_get(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_in_trig0_reg_u ptp_in_trig0_reg = {0};
	union ptp_in_trig1_reg_u ptp_in_trig1_reg = {0};
	union ptp_in_trig2_reg_u ptp_in_trig2_reg = {0};
	union ptp_in_trig3_reg_u ptp_in_trig3_reg = {0};

	nss_ptp_in_trig0_reg_get(nss_phydev, &ptp_in_trig0_reg);
	nss_ptp_in_trig1_reg_get(nss_phydev, &ptp_in_trig1_reg);
	time->nanoseconds = (ptp_in_trig0_reg.bf.ptp_in_trig_nisec << 16) |
		ptp_in_trig1_reg.bf.ptp_in_trig_nisec;

	nss_ptp_in_trig2_reg_get(nss_phydev, &ptp_in_trig2_reg);
	nss_ptp_in_trig3_reg_get(nss_phydev, &ptp_in_trig3_reg);
	time->fracnanoseconds = (ptp_in_trig2_reg.bf.ptp_in_trig_nisec << 4) |
			(ptp_in_trig3_reg.bf.ptp_in_trig_nisec & 0xf);

	return 0;
}

static int
nss_phy_ptp_grandmaster_mode_set(struct nss_phy_device *nss_phydev,
		fal_ptp_grandmaster_mode_t *gm_mode)
{
	union ptp_gm_conf0_reg_u ptp_gm_conf0_reg = {0};
	union ptp_gm_conf1_reg_u ptp_gm_conf1_reg = {0};

	nss_ptp_gm_conf0_reg_get(nss_phydev, &ptp_gm_conf0_reg);
	nss_ptp_gm_conf1_reg_get(nss_phydev, &ptp_gm_conf1_reg);
	ptp_gm_conf0_reg.bf.grandmaster_mode = gm_mode->grandmaster_mode_en;
	if (gm_mode->ns_sync_mode == FAL_GM_PPSIN_MODE)
	{
		ptp_gm_conf0_reg.bf.gm_pps_sync = true;
		ptp_gm_conf0_reg.bf.gm_pll_mode = false;
	}
	else if (gm_mode->ns_sync_mode == FAL_GM_HWPLL_MODE)
	{
		ptp_gm_conf0_reg.bf.gm_pps_sync = false;
		ptp_gm_conf0_reg.bf.gm_pll_mode = true;
	}
	else
	{
		ptp_gm_conf0_reg.bf.gm_pps_sync = false;
		ptp_gm_conf0_reg.bf.gm_pll_mode = false;
	}
	ptp_gm_conf0_reg.bf.gm_maxfreq_offset = gm_mode->freq_offset;
	ptp_gm_conf1_reg.bf.gm_kp_ldn =
		(gm_mode->right_shift_in_kp << (PTP_GM_CONF1_REG_GM_KP_LDN_LEN-1)) |
		gm_mode->kp_value;
	ptp_gm_conf1_reg.bf.gm_ki_ldn =
		(gm_mode->right_shift_in_ki << (PTP_GM_CONF1_REG_GM_KI_LDN_LEN-1)) |
		gm_mode->ki_value;
	nss_ptp_gm_conf0_reg_set(nss_phydev, &ptp_gm_conf0_reg);
	nss_ptp_gm_conf1_reg_set(nss_phydev, &ptp_gm_conf1_reg);

#if defined(IN_LINUX_STD_PTP)
	if (gm_mode->grandmaster_second_sync_en == true) {
		nss_ptp_gm_gps_seconds_sync_enable(nss_phydev, true);
	} else {
		nss_ptp_gm_gps_seconds_sync_enable(nss_phydev, false);
	}
#endif

	return 0;
}

static int
nss_phy_ptp_grandmaster_mode_get(struct nss_phy_device *nss_phydev,
		fal_ptp_grandmaster_mode_t *gm_mode)
{
	union ptp_gm_conf0_reg_u ptp_gm_conf0_reg = {0};
	union ptp_gm_conf1_reg_u ptp_gm_conf1_reg = {0};

	nss_ptp_gm_conf0_reg_get(nss_phydev, &ptp_gm_conf0_reg);
	nss_ptp_gm_conf1_reg_get(nss_phydev, &ptp_gm_conf1_reg);
	gm_mode->grandmaster_mode_en = ptp_gm_conf0_reg.bf.grandmaster_mode;
	if (ptp_gm_conf0_reg.bf.gm_pps_sync == 1 && ptp_gm_conf0_reg.bf.gm_pll_mode == 0)
	{
		gm_mode->ns_sync_mode = FAL_GM_PPSIN_MODE;
	}
	else if (ptp_gm_conf0_reg.bf.gm_pps_sync == 0 && ptp_gm_conf0_reg.bf.gm_pll_mode == 1)
	{
		gm_mode->ns_sync_mode = FAL_GM_HWPLL_MODE;
	}
	else
	{
		gm_mode->ns_sync_mode = FAL_GM_SWPLL_MODE;
	}
	gm_mode->freq_offset = ptp_gm_conf0_reg.bf.gm_maxfreq_offset;
	gm_mode->right_shift_in_kp = ptp_gm_conf1_reg.bf.gm_kp_ldn >> 5;
	gm_mode->kp_value = ptp_gm_conf1_reg.bf.gm_kp_ldn & 0x1f;
	gm_mode->right_shift_in_ki = ptp_gm_conf1_reg.bf.gm_ki_ldn >> 5;
	gm_mode->ki_value = ptp_gm_conf1_reg.bf.gm_ki_ldn & 0x1f;

#if defined(IN_LINUX_STD_PTP)
	if (nss_ptp_gm_gps_seconds_sync_status_get(nss_phydev) == true) {
		gm_mode->grandmaster_second_sync_en = true;
	} else {
		gm_mode->grandmaster_second_sync_en = false;
	}
#endif

	return 0;
}

static int
nss_phy_ptp_rtc_time_get(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_rtc0_reg_u ptp_rtc0_reg = {0};
	union ptp_rtc1_reg_u ptp_rtc1_reg = {0};
	union ptp_rtc2_reg_u ptp_rtc2_reg = {0};
	union ptp_rtc3_reg_u ptp_rtc3_reg = {0};
	union ptp_rtc4_reg_u ptp_rtc4_reg = {0};
	union ptp_rtc5_reg_u ptp_rtc5_reg = {0};
	union ptp_rtc6_reg_u ptp_rtc6_reg = {0};

	nss_ptp_rtc0_reg_get(nss_phydev, &ptp_rtc0_reg);
	nss_ptp_rtc1_reg_get(nss_phydev, &ptp_rtc1_reg);
	nss_ptp_rtc2_reg_get(nss_phydev, &ptp_rtc2_reg);
	time->seconds = ((u64)ptp_rtc0_reg.bf.ptp_rtc_sec << 32) |
			(ptp_rtc1_reg.bf.ptp_rtc_sec << 16) | ptp_rtc2_reg.bf.ptp_rtc_sec;

	nss_ptp_rtc3_reg_get(nss_phydev, &ptp_rtc3_reg);
	nss_ptp_rtc4_reg_get(nss_phydev, &ptp_rtc4_reg);
	time->nanoseconds = (ptp_rtc3_reg.bf.ptp_rtc_nisec << 16) | ptp_rtc4_reg.bf.ptp_rtc_nisec;

	nss_ptp_rtc5_reg_get(nss_phydev, &ptp_rtc5_reg);
	nss_ptp_rtc6_reg_get(nss_phydev, &ptp_rtc6_reg);
	time->fracnanoseconds = (ptp_rtc5_reg.bf.ptp_rtc_nfsec << 4) |
		ptp_rtc6_reg.bf.ptp_rtc_nfsec;

	return 0;
}

static int
nss_phy_ptp_rtc_time_set(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_rtc_preloaded0_reg_u ptp_rtc_preloaded0_reg = {0};
	union ptp_rtc_preloaded1_reg_u ptp_rtc_preloaded1_reg = {0};
	union ptp_rtc_preloaded2_reg_u ptp_rtc_preloaded2_reg = {0};
	union ptp_rtc_preloaded3_reg_u ptp_rtc_preloaded3_reg = {0};
	union ptp_rtc_preloaded4_reg_u ptp_rtc_preloaded4_reg = {0};
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};

	ptp_rtc_preloaded0_reg.bf.ptp_rtc_preloaded_sec = (time->seconds >> 32) & 0xffff;
	ptp_rtc_preloaded1_reg.bf.ptp_rtc_preloaded_sec = (time->seconds >> 16) & 0xffff;
	ptp_rtc_preloaded2_reg.bf.ptp_rtc_preloaded_sec = time->seconds & 0xffff;
	nss_ptp_rtc_preloaded0_reg_set(nss_phydev,
				&ptp_rtc_preloaded0_reg);
	nss_ptp_rtc_preloaded1_reg_set(nss_phydev,
				&ptp_rtc_preloaded1_reg);
	nss_ptp_rtc_preloaded2_reg_set(nss_phydev,
				&ptp_rtc_preloaded2_reg);

	ptp_rtc_preloaded3_reg.bf.ptp_rtc_preloaded_nisec = time->nanoseconds >> 16;
	ptp_rtc_preloaded4_reg.bf.ptp_rtc_preloaded_nisec = time->nanoseconds & 0xffff;
	nss_ptp_rtc_preloaded3_reg_set(nss_phydev,
				&ptp_rtc_preloaded3_reg);
	nss_ptp_rtc_preloaded4_reg_set(nss_phydev,
				&ptp_rtc_preloaded4_reg);

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev,
				&ptp_rtc_ext_conf_reg);
	ptp_rtc_ext_conf_reg.bf.load_rtc = 1;
	nss_ptp_rtc_ext_conf_reg_set(nss_phydev,
				&ptp_rtc_ext_conf_reg);

	return 0;
}

static int
nss_phy_ptp_rtc_time_clear(struct nss_phy_device *nss_phydev)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev, &ptp_rtc_ext_conf_reg);
	ptp_rtc_ext_conf_reg.bf.clear_rtc = true;
	nss_ptp_rtc_ext_conf_reg_set(nss_phydev, &ptp_rtc_ext_conf_reg);

	return 0;
}

static int
nss_phy_ptp_rtc_adjtime_set(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_rtcoffs0_reg_u ptp_rtcoffs0_reg = {0};
	union ptp_rtcoffs1_reg_u ptp_rtcoffs1_reg = {0};
	union ptp_rtcoffs2_reg_u ptp_rtcoffs2_reg = {0};
	union ptp_rtcoffs3_reg_u ptp_rtcoffs3_reg = {0};
	union ptp_rtcoffs4_reg_u ptp_rtcoffs4_reg = {0};
	union ptp_rtcoffs_valid_reg_u ptp_rtcoffs_valid_reg = {0};

	ptp_rtcoffs0_reg.bf.ptp_rtcoffs_nsec = time->nanoseconds >> 16;
	ptp_rtcoffs1_reg.bf.ptp_rtcoffs_nsec = time->nanoseconds & 0xffff;
	ptp_rtcoffs2_reg.bf.ptp_rtcoffs_sec = (time->seconds >> 32) & 0xffff;
	ptp_rtcoffs3_reg.bf.ptp_rtcoffs_sec = (time->seconds >> 16) & 0xffff;
	ptp_rtcoffs4_reg.bf.ptp_rtcoffs_sec = time->seconds & 0xffff;
	ptp_rtcoffs_valid_reg.bf.ptp_rtcoffs_valid = 1;
	nss_ptp_rtcoffs0_reg_set(nss_phydev, &ptp_rtcoffs0_reg);
	nss_ptp_rtcoffs1_reg_set(nss_phydev, &ptp_rtcoffs1_reg);
	nss_ptp_rtcoffs2_reg_set(nss_phydev, &ptp_rtcoffs2_reg);
	nss_ptp_rtcoffs3_reg_set(nss_phydev, &ptp_rtcoffs3_reg);
	nss_ptp_rtcoffs4_reg_set(nss_phydev, &ptp_rtcoffs4_reg);
	nss_ptp_rtcoffs_valid_reg_set(nss_phydev, &ptp_rtcoffs_valid_reg);

	return 0;
}

static int
nss_phy_ptp_rtc_adjfreq_set(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};
	union ptp_rtc_inc0_reg_u ptp_rtc_inc0_reg = {0};
	union ptp_rtc_inc1_reg_u ptp_rtc_inc1_reg = {0};

	nss_ptp_rtc_inc0_reg_get(nss_phydev, &ptp_rtc_inc0_reg);
	ptp_rtc_inc0_reg.bf.ptp_rtc_inc_nis = time->nanoseconds & 0x3f;
	ptp_rtc_inc0_reg.bf.ptp_rtc_inc_nfs = (time->fracnanoseconds >> 16) & 0x3ff;
	nss_ptp_rtc_inc0_reg_set(nss_phydev, &ptp_rtc_inc0_reg);

	nss_ptp_rtc_inc1_reg_get(nss_phydev, &ptp_rtc_inc1_reg);
	ptp_rtc_inc1_reg.bf.ptp_rtc_inc_nfs = time->fracnanoseconds & 0xffff;
	nss_ptp_rtc_inc1_reg_set(nss_phydev, &ptp_rtc_inc1_reg);

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev, &ptp_rtc_ext_conf_reg);
	ptp_rtc_ext_conf_reg.bf.set_incval_valid = true;
	nss_ptp_rtc_ext_conf_reg_set(nss_phydev, &ptp_rtc_ext_conf_reg);

	return 0;
}

static int
nss_phy_ptp_rtc_adjfreq_get(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_rtc_inc0_reg_u ptp_rtc_inc0_reg = {0};
	union ptp_rtc_inc1_reg_u ptp_rtc_inc1_reg = {0};

	nss_ptp_rtc_inc0_reg_get(nss_phydev, &ptp_rtc_inc0_reg);
	nss_ptp_rtc_inc1_reg_get(nss_phydev, &ptp_rtc_inc1_reg);
	time->nanoseconds = ptp_rtc_inc0_reg.bf.ptp_rtc_inc_nis;
	time->fracnanoseconds = (ptp_rtc_inc0_reg.bf.ptp_rtc_inc_nfs << 16) |
		ptp_rtc_inc1_reg.bf.ptp_rtc_inc_nfs;

	return 0;
}

static int
nss_phy_ptp_link_delay_set(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_link_delay_0_reg_u ptp_link_delay_0_reg = {0};
	union ptp_link_delay_1_reg_u ptp_link_delay_1_reg = {0};

	nss_ptp_link_delay_0_reg_get(nss_phydev, &ptp_link_delay_0_reg);
	ptp_link_delay_0_reg.bf.link_delay = time->nanoseconds >> 16;
	nss_ptp_link_delay_0_reg_set(nss_phydev, &ptp_link_delay_0_reg);

	nss_ptp_link_delay_1_reg_get(nss_phydev, &ptp_link_delay_1_reg);
	ptp_link_delay_1_reg.bf.link_delay = time->nanoseconds & 0xffff;
	nss_ptp_link_delay_1_reg_set(nss_phydev, &ptp_link_delay_1_reg);

	return 0;
}

static int
nss_phy_ptp_link_delay_get(struct nss_phy_device *nss_phydev,
		fal_ptp_time_t *time)
{
	union ptp_link_delay_0_reg_u ptp_link_delay_0_reg = {0};
	union ptp_link_delay_1_reg_u ptp_link_delay_1_reg = {0};

	nss_ptp_link_delay_0_reg_get(nss_phydev, &ptp_link_delay_0_reg);
	nss_ptp_link_delay_1_reg_get(nss_phydev, &ptp_link_delay_1_reg);
	time->nanoseconds = (ptp_link_delay_0_reg.bf.link_delay << 16) |
		ptp_link_delay_1_reg.bf.link_delay;

	return 0;
}

static int
nss_phy_ptp_security_set(struct nss_phy_device *nss_phydev,
		fal_ptp_security_t *sec)
{
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};

	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	ptp_misc_config_reg.bf.ptp_addr_chk_en = sec->address_check_en;
	ptp_misc_config_reg.bf.ipv6_udp_chk_en = sec->ipv6_udp_checksum_recal_en;
	ptp_misc_config_reg.bf.ptp_ver_chk_en = sec->version_check_en;
	ptp_misc_config_reg.bf.ptp_version = sec->ptp_version;
	nss_ptp_misc_config_reg_set(nss_phydev, &ptp_misc_config_reg);

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	ptp_main_conf_reg.bf.ipv4_force_checksum_zero =
		sec->ipv4_udp_checksum_force_zero_en;
	ptp_main_conf_reg.bf.ipv6_embed_force_checksum_zero =
		sec->ipv6_embed_udp_checksum_force_zero_en;
	nss_ptp_main_conf_reg_set(nss_phydev, &ptp_main_conf_reg);

	return 0;
}

static int
nss_phy_ptp_security_get(struct nss_phy_device *nss_phydev,
		fal_ptp_security_t *sec)
{
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};
	union ptp_main_conf_reg_u ptp_main_conf_reg = {0};

	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	sec->address_check_en = ptp_misc_config_reg.bf.ptp_addr_chk_en;
	sec->ipv6_udp_checksum_recal_en = ptp_misc_config_reg.bf.ipv6_udp_chk_en;
	sec->version_check_en = ptp_misc_config_reg.bf.ptp_ver_chk_en;
	sec->ptp_version = ptp_misc_config_reg.bf.ptp_version;

	nss_ptp_main_conf_reg_get(nss_phydev, &ptp_main_conf_reg);
	sec->ipv4_udp_checksum_force_zero_en = ptp_main_conf_reg.bf.ipv4_force_checksum_zero;
	sec->ipv6_embed_udp_checksum_force_zero_en =
		ptp_main_conf_reg.bf.ipv6_embed_force_checksum_zero;

	return 0;
}

static int
nss_phy_ptp_pps_signal_control_set(struct nss_phy_device *nss_phydev,
		fal_ptp_pps_signal_control_t *sig_control)
{
	union ptp_ppsin_latency_reg_u ptp_ppsin_latency_reg = {0};
	union ptp_phase_adjust_0_reg_u ptp_phase_adjust_0_reg = {0};
	union ptp_phase_adjust_1_reg_u ptp_phase_adjust_1_reg = {0};
	union ptp_pps_pul_width_0_reg_u ptp_pps_pul_width_0_reg = {0};
	union ptp_pps_pul_width_1_reg_u ptp_pps_pul_width_1_reg = {0};

	nss_ptp_ppsin_latency_reg_get(nss_phydev,
				&ptp_ppsin_latency_reg);
	ptp_ppsin_latency_reg.bf.ptp_ppsin_latency_sign = sig_control->negative_in_latency;
	ptp_ppsin_latency_reg.bf.ptp_ppsin_latency_value = sig_control->in_latency;
	nss_ptp_ppsin_latency_reg_set(nss_phydev,
				&ptp_ppsin_latency_reg);

	nss_ptp_phase_adjust_0_reg_get(nss_phydev,
				&ptp_phase_adjust_0_reg);
	ptp_phase_adjust_0_reg.bf.phase_value = sig_control->out_phase >> 16;
	nss_ptp_phase_adjust_0_reg_set(nss_phydev,
				&ptp_phase_adjust_0_reg);

	nss_ptp_phase_adjust_1_reg_get(nss_phydev,
				&ptp_phase_adjust_1_reg);
	ptp_phase_adjust_1_reg.bf.phase_value = sig_control->out_phase & 0xffff;
	nss_ptp_phase_adjust_1_reg_set(nss_phydev,
				&ptp_phase_adjust_1_reg);

	nss_ptp_pps_pul_width_0_reg_get(nss_phydev,
				&ptp_pps_pul_width_0_reg);
	ptp_pps_pul_width_0_reg.bf.pul_value = sig_control->out_pulse_width >> 16;
	nss_ptp_pps_pul_width_0_reg_set(nss_phydev,
				&ptp_pps_pul_width_0_reg);

	nss_ptp_pps_pul_width_1_reg_get(nss_phydev,
				&ptp_pps_pul_width_1_reg);
	ptp_pps_pul_width_1_reg.bf.pul_value = sig_control->out_pulse_width & 0xffff;
	nss_ptp_pps_pul_width_1_reg_set(nss_phydev,
				&ptp_pps_pul_width_1_reg);

	return 0;
}

static int
nss_phy_ptp_pps_signal_control_get(struct nss_phy_device *nss_phydev,
		fal_ptp_pps_signal_control_t *sig_control)
{
	union ptp_ppsin_latency_reg_u ptp_ppsin_latency_reg = {0};
	union ptp_phase_adjust_0_reg_u ptp_phase_adjust_0_reg = {0};
	union ptp_phase_adjust_1_reg_u ptp_phase_adjust_1_reg = {0};
	union ptp_pps_pul_width_0_reg_u ptp_pps_pul_width_0_reg = {0};
	union ptp_pps_pul_width_1_reg_u ptp_pps_pul_width_1_reg = {0};

	nss_ptp_ppsin_latency_reg_get(nss_phydev,
				&ptp_ppsin_latency_reg);
	sig_control->negative_in_latency = ptp_ppsin_latency_reg.bf.ptp_ppsin_latency_sign;
	sig_control->in_latency = ptp_ppsin_latency_reg.bf.ptp_ppsin_latency_value;

	nss_ptp_phase_adjust_0_reg_get(nss_phydev,
				&ptp_phase_adjust_0_reg);
	nss_ptp_phase_adjust_1_reg_get(nss_phydev,
				&ptp_phase_adjust_1_reg);
	sig_control->out_phase = (ptp_phase_adjust_0_reg.bf.phase_value << 16) |
			ptp_phase_adjust_1_reg.bf.phase_value;

	nss_ptp_pps_pul_width_0_reg_get(nss_phydev,
				&ptp_pps_pul_width_0_reg);
	nss_ptp_pps_pul_width_1_reg_get(nss_phydev,
				&ptp_pps_pul_width_1_reg);
	sig_control->out_pulse_width = (ptp_pps_pul_width_0_reg.bf.pul_value << 16) |
			ptp_pps_pul_width_1_reg.bf.pul_value;

	return 0;
}

static int
nss_phy_ptp_rx_crc_recalc_enable(struct nss_phy_device *nss_phydev,
		bool status)
{
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};

	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	ptp_misc_config_reg.bf.crc_validate_en = status;
	nss_ptp_misc_config_reg_set(nss_phydev, &ptp_misc_config_reg);

	return 0;
}

static int
nss_phy_ptp_rx_crc_recalc_status_get(struct nss_phy_device *nss_phydev,
		bool *status)
{
	union ptp_misc_config_reg_u ptp_misc_config_reg = {0};

	nss_ptp_misc_config_reg_get(nss_phydev, &ptp_misc_config_reg);
	*status = ptp_misc_config_reg.bf.crc_validate_en;

	return 0;
}

static int
nss_phy_ptp_asym_correction_set(struct nss_phy_device *nss_phydev,
		fal_ptp_asym_correction_t *asym_cf)
{
	union ptp_misc_control_reg_u ptp_misc_control_reg = {0};
	union ptp_ingress_asymmetry_0_reg_u ptp_ingress_asymmetry_0_reg = {0};
	union ptp_ingress_asymmetry_1_reg_u ptp_ingress_asymmetry_1_reg = {0};
	union ptp_egress_asymmetry_0_reg_u ptp_egress_asymmetry_0_reg = {0};
	union ptp_egress_asymmetry_1_reg_u ptp_egress_asymmetry_1_reg = {0};

	nss_ptp_misc_control_reg_get(nss_phydev,
				&ptp_misc_control_reg);
	ptp_misc_control_reg.bf.eg_asym_en = asym_cf->eg_asym_en;
	ptp_misc_control_reg.bf.in_asym_en = asym_cf->in_asym_en;
	nss_ptp_misc_control_reg_set(nss_phydev,
				&ptp_misc_control_reg);

	nss_ptp_ingress_asymmetry_0_reg_get(nss_phydev,
				&ptp_ingress_asymmetry_0_reg);
	ptp_ingress_asymmetry_0_reg.bf.in_asym = asym_cf->in_asym_value >> 16;
	nss_ptp_ingress_asymmetry_0_reg_set(nss_phydev,
				&ptp_ingress_asymmetry_0_reg);

	nss_ptp_ingress_asymmetry_1_reg_get(nss_phydev,
				&ptp_ingress_asymmetry_1_reg);
	ptp_ingress_asymmetry_1_reg.bf.in_asym = asym_cf->in_asym_value & 0xffff;
	nss_ptp_ingress_asymmetry_1_reg_set(nss_phydev,
				&ptp_ingress_asymmetry_1_reg);

	nss_ptp_egress_asymmetry_0_reg_get(nss_phydev,
				&ptp_egress_asymmetry_0_reg);
	ptp_egress_asymmetry_0_reg.bf.eg_asym = asym_cf->eg_asym_value >> 16;
	nss_ptp_egress_asymmetry_0_reg_get(nss_phydev,
				&ptp_egress_asymmetry_0_reg);

	nss_ptp_egress_asymmetry_1_reg_get(nss_phydev,
				&ptp_egress_asymmetry_1_reg);
	ptp_egress_asymmetry_1_reg.bf.eg_asym = asym_cf->eg_asym_value & 0xffff;
	nss_ptp_egress_asymmetry_1_reg_set(nss_phydev,
				&ptp_egress_asymmetry_1_reg);

	return 0;
}

static int
nss_phy_ptp_asym_correction_get(struct nss_phy_device *nss_phydev,
		fal_ptp_asym_correction_t* asym_cf)
{
	union ptp_misc_control_reg_u ptp_misc_control_reg = {0};
	union ptp_ingress_asymmetry_0_reg_u ptp_ingress_asymmetry_0_reg = {0};
	union ptp_ingress_asymmetry_1_reg_u ptp_ingress_asymmetry_1_reg = {0};
	union ptp_egress_asymmetry_0_reg_u ptp_egress_asymmetry_0_reg = {0};
	union ptp_egress_asymmetry_1_reg_u ptp_egress_asymmetry_1_reg = {0};

	nss_ptp_misc_control_reg_get(nss_phydev,
				&ptp_misc_control_reg);
	asym_cf->eg_asym_en = ptp_misc_control_reg.bf.eg_asym_en;
	asym_cf->in_asym_en = ptp_misc_control_reg.bf.in_asym_en;

	nss_ptp_ingress_asymmetry_0_reg_get(nss_phydev,
				&ptp_ingress_asymmetry_0_reg);
	nss_ptp_ingress_asymmetry_1_reg_get(nss_phydev,
				&ptp_ingress_asymmetry_1_reg);
	asym_cf->in_asym_value = (ptp_ingress_asymmetry_0_reg.bf.in_asym << 16) |
		ptp_ingress_asymmetry_1_reg.bf.in_asym;

	nss_ptp_egress_asymmetry_0_reg_get(nss_phydev,
				&ptp_egress_asymmetry_0_reg);
	nss_ptp_egress_asymmetry_1_reg_get(nss_phydev,
				&ptp_egress_asymmetry_1_reg);
	asym_cf->eg_asym_value = (ptp_egress_asymmetry_0_reg.bf.eg_asym << 16) |
		ptp_egress_asymmetry_1_reg.bf.eg_asym;

	return 0;
}

static int
nss_phy_ptp_output_waveform_set(struct nss_phy_device *nss_phydev,
		fal_ptp_output_waveform_t *waveform)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};
	union ptp_freq_waveform_period_0_reg_u ptp_freq_waveform_period_0_reg = {0};
	union ptp_freq_waveform_period_1_reg_u ptp_freq_waveform_period_1_reg = {0};
	union ptp_freq_waveform_period_2_reg_u ptp_freq_waveform_period_2_reg = {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev,
				&ptp_rtc_ext_conf_reg);
	if (waveform->waveform_type == FAL_WAVE_FREQ)
	{
		ptp_rtc_ext_conf_reg.bf.select_output_waveform =
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_FREQ;
	}
	else if (waveform->waveform_type == FAL_PULSE_10MS)
	{
		ptp_rtc_ext_conf_reg.bf.select_output_waveform =
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_PULSE_10MS;
	}
	else if (waveform->waveform_type == FAL_TRIGGER0_GPIO)
	{
		ptp_rtc_ext_conf_reg.bf.select_output_waveform =
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_TRIG0_GPIO;
	}
	else
	{
		ptp_rtc_ext_conf_reg.bf.select_output_waveform =
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_RXTS_VALID;
	}
	nss_ptp_rtc_ext_conf_reg_set(nss_phydev, &ptp_rtc_ext_conf_reg);

	ptp_freq_waveform_period_0_reg.bf.phase_ali = waveform->wave_align_pps_out_en;
	ptp_freq_waveform_period_0_reg.bf.wave_period = (waveform->wave_period >> 32) & 0x7fff;
	ptp_freq_waveform_period_1_reg.bf.wave_period = (waveform->wave_period >> 16) & 0xffff;
	ptp_freq_waveform_period_2_reg.bf.wave_period = waveform->wave_period & 0xffff;
	nss_ptp_freq_waveform_period_0_reg_set(nss_phydev,
				&ptp_freq_waveform_period_0_reg);
	nss_ptp_freq_waveform_period_1_reg_set(nss_phydev,
				&ptp_freq_waveform_period_1_reg);
	nss_ptp_freq_waveform_period_2_reg_set(nss_phydev,
				&ptp_freq_waveform_period_2_reg);

	return 0;
}

static int
nss_phy_ptp_output_waveform_get(struct nss_phy_device *nss_phydev,
		fal_ptp_output_waveform_t *waveform)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};
	union ptp_freq_waveform_period_0_reg_u ptp_freq_waveform_period_0_reg = {0};
	union ptp_freq_waveform_period_1_reg_u ptp_freq_waveform_period_1_reg = {0};
	union ptp_freq_waveform_period_2_reg_u ptp_freq_waveform_period_2_reg = {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev,
				&ptp_rtc_ext_conf_reg);
	if (ptp_rtc_ext_conf_reg.bf.select_output_waveform ==
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_FREQ)
	{
		waveform->waveform_type = FAL_WAVE_FREQ;
	}
	else if (ptp_rtc_ext_conf_reg.bf.select_output_waveform ==
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_PULSE_10MS)
	{
		waveform->waveform_type = FAL_PULSE_10MS;
	}
	else if (ptp_rtc_ext_conf_reg.bf.select_output_waveform ==
			PTP_RTC_EXT_CONF_REG_SELECT_OUTPUT_WAVEFORM_TRIG0_GPIO)
	{
		waveform->waveform_type = FAL_TRIGGER0_GPIO;
	}
	else
	{
		waveform->waveform_type = FAL_RX_PTP_STATE;
	}

	nss_ptp_freq_waveform_period_0_reg_get(nss_phydev,
				&ptp_freq_waveform_period_0_reg);
	nss_ptp_freq_waveform_period_1_reg_get(nss_phydev,
				&ptp_freq_waveform_period_1_reg);
	nss_ptp_freq_waveform_period_2_reg_get(nss_phydev,
				&ptp_freq_waveform_period_2_reg);
	waveform->wave_align_pps_out_en = ptp_freq_waveform_period_0_reg.bf.phase_ali;
	waveform->wave_period = ((u64)ptp_freq_waveform_period_0_reg.bf.wave_period << 32) |
		(ptp_freq_waveform_period_1_reg.bf.wave_period  << 16) |
		ptp_freq_waveform_period_2_reg.bf.wave_period;

	return 0;
}

static int
nss_phy_ptp_rtc_time_snapshot_enable(struct nss_phy_device *nss_phydev,
		bool status)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg= {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev, &ptp_rtc_ext_conf_reg);
	ptp_rtc_ext_conf_reg.bf.rtc_read_mode = status;
	if (status == true)
	{
		ptp_rtc_ext_conf_reg.bf.rtc_snapshot = true;
	}

	nss_ptp_rtc_ext_conf_reg_set(nss_phydev, &ptp_rtc_ext_conf_reg);

	return 0;
}

static int
nss_phy_ptp_rtc_time_snapshot_status_get(struct nss_phy_device *nss_phydev,
		bool *status)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg= {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev, &ptp_rtc_ext_conf_reg);
	*status = ptp_rtc_ext_conf_reg.bf.rtc_read_mode;

	return 0;
}

static int
nss_phy_ptp_increment_sync_from_clock_enable(struct nss_phy_device *nss_phydev,
		bool status)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev, &ptp_rtc_ext_conf_reg);
	ptp_rtc_ext_conf_reg.bf.set_incval_mode = status;
	if (status == true)
	{
		ptp_rtc_ext_conf_reg.bf.set_incval_valid = true;
	}
	nss_ptp_rtc_ext_conf_reg_set(nss_phydev, &ptp_rtc_ext_conf_reg);

	return 0;
}

static int
nss_phy_ptp_increment_sync_from_clock_status_get(struct nss_phy_device *nss_phydev,
		bool *status)
{
	union ptp_rtc_ext_conf_reg_u ptp_rtc_ext_conf_reg = {0};

	nss_ptp_rtc_ext_conf_reg_get(nss_phydev, &ptp_rtc_ext_conf_reg);
	*status = ptp_rtc_ext_conf_reg.bf.set_incval_mode;

	return 0;
}

static int
nss_phy_ptp_tod_uart_set(struct nss_phy_device *nss_phydev,
		fal_ptp_tod_uart_t *tod_uart)
{
	union ptp_baud_config_reg_u  ptp_baud_config_reg = {0};
	union ptp_uart_configuration_reg_u ptp_uart_configuration_reg = {0};
	union ptp_reset_buffer_reg_u ptp_reset_buffer_reg = {0};
	union ptp_tx_buffer_write_reg_u ptp_tx_buffer_write_reg = {0};

	nss_ptp_baud_config_reg_get(nss_phydev, &ptp_baud_config_reg);
	ptp_baud_config_reg.bf.baud_rate = tod_uart->baud_config;
	nss_ptp_baud_config_reg_set(nss_phydev, &ptp_baud_config_reg);

	nss_ptp_uart_configuration_reg_get(nss_phydev,
				&ptp_uart_configuration_reg);
	ptp_uart_configuration_reg.bf.start_polarity = tod_uart->uart_config_bmp & 0x1;
	ptp_uart_configuration_reg.bf.msb_first =
	(tod_uart->uart_config_bmp >> PTP_UART_CONFIGURATION_REG_MSB_FIRST_OFFSET) & 0x1;
	ptp_uart_configuration_reg.bf.parity_en =
	(tod_uart->uart_config_bmp >> PTP_UART_CONFIGURATION_REG_PARITY_EN_OFFSET) & 0x1;
	ptp_uart_configuration_reg.bf.auto_tod_out_en =
	(tod_uart->uart_config_bmp >> PTP_UART_CONFIGURATION_REG_AUTO_TOD_OUT_EN_OFFSET) & 0x1;
	ptp_uart_configuration_reg.bf.auto_tod_in_en =
	(tod_uart->uart_config_bmp >> PTP_UART_CONFIGURATION_REG_AUTO_TOD_IN_EN_OFFSET) & 0x1;

	nss_ptp_uart_configuration_reg_set(nss_phydev,
				&ptp_uart_configuration_reg);

	if (ptp_uart_configuration_reg.bf.auto_tod_out_en)
		nss_ptp_clock_synce_clock_enable(nss_phydev, true);
	else
		nss_ptp_clock_synce_clock_enable(nss_phydev, false);

	nss_ptp_reset_buffer_reg_get(nss_phydev, &ptp_reset_buffer_reg);
	ptp_reset_buffer_reg.bf.reset = tod_uart->reset_buf_en;
	nss_ptp_reset_buffer_reg_set(nss_phydev, &ptp_reset_buffer_reg);

	nss_ptp_tx_buffer_write_reg_get(nss_phydev,
				&ptp_tx_buffer_write_reg);
	ptp_tx_buffer_write_reg.bf.tx_buffer = tod_uart->tx_buf_value;
	nss_ptp_tx_buffer_write_reg_set(nss_phydev,
				&ptp_tx_buffer_write_reg);

	return 0;
}

static int
nss_phy_ptp_tod_uart_get(struct nss_phy_device *nss_phydev,
		fal_ptp_tod_uart_t *tod_uart)
{
	union ptp_baud_config_reg_u  ptp_baud_config_reg = {0};
	union ptp_uart_configuration_reg_u ptp_uart_configuration_reg = {0};
	union ptp_buffer_status_reg_u ptp_buffer_status_reg = {0};
	union ptp_tx_buffer_write_reg_u ptp_tx_buffer_write_reg = {0};
	union ptp_rx_buffer_read_reg_u ptp_rx_buffer_read_reg = {0};

	nss_ptp_baud_config_reg_get(nss_phydev, &ptp_baud_config_reg);
	tod_uart->baud_config = ptp_baud_config_reg.bf.baud_rate;

	nss_ptp_uart_configuration_reg_get(nss_phydev,
				&ptp_uart_configuration_reg);
	tod_uart->uart_config_bmp = ptp_uart_configuration_reg.bf.start_polarity |
		(ptp_uart_configuration_reg.bf.msb_first <<
		 PTP_UART_CONFIGURATION_REG_MSB_FIRST_OFFSET) |
		(ptp_uart_configuration_reg.bf.parity_en <<
		 PTP_UART_CONFIGURATION_REG_PARITY_EN_OFFSET) |
		(ptp_uart_configuration_reg.bf.auto_tod_out_en <<
		 PTP_UART_CONFIGURATION_REG_AUTO_TOD_OUT_EN_OFFSET) |
		(ptp_uart_configuration_reg.bf.auto_tod_in_en <<
		 PTP_UART_CONFIGURATION_REG_AUTO_TOD_IN_EN_OFFSET);

	/* reset buffer is self clearing, always read as 0 */
	tod_uart->reset_buf_en = 0;

	nss_ptp_buffer_status_reg_get(nss_phydev, &ptp_buffer_status_reg);
	tod_uart->buf_status_bmp = ptp_buffer_status_reg.bf.tx_buffer_almost_empty |
		(ptp_buffer_status_reg.bf.tx_buffer_almost_full <<
		 PTP_BUFFER_STATUS_REG_TX_BUFFER_ALMOST_FULL_OFFSET) |
		(ptp_buffer_status_reg.bf.tx_buffer_half_full <<
		 PTP_BUFFER_STATUS_REG_TX_BUFFER_HALF_FULL_OFFSET) |
		(ptp_buffer_status_reg.bf.tx_buffer_full <<
		 PTP_BUFFER_STATUS_REG_TX_BUFFER_FULL_OFFSET) |
		(ptp_buffer_status_reg.bf.rx_buffer_almost_empty <<
		 PTP_BUFFER_STATUS_REG_RX_BUFFER_ALMOST_EMPTY_OFFSET) |
		(ptp_buffer_status_reg.bf.rx_buffer_almost_full <<
		 PTP_BUFFER_STATUS_REG_RX_BUFFER_ALMOST_FULL_OFFSET) |
		(ptp_buffer_status_reg.bf.rx_buffer_half_full <<
		 PTP_BUFFER_STATUS_REG_RX_BUFFER_HALF_FULL_OFFSET) |
		(ptp_buffer_status_reg.bf.rx_buffer_full <<
		 PTP_BUFFER_STATUS_REG_RX_BUFFER_FULL_OFFSET) |
		(ptp_buffer_status_reg.bf.rx_buffer_data_present <<
		 PTP_BUFFER_STATUS_REG_RX_BUFFER_DATA_PRESENT_OFFSET);


	nss_ptp_tx_buffer_write_reg_get(nss_phydev,
				&ptp_tx_buffer_write_reg);
	tod_uart->tx_buf_value = ptp_tx_buffer_write_reg.bf.tx_buffer;

	nss_ptp_rx_buffer_read_reg_get(nss_phydev,
				&ptp_rx_buffer_read_reg);
	tod_uart->rx_buf_value = ptp_rx_buffer_read_reg.bf.rx_data;

	return 0;
}

static int
_nss_phy_ptp_enhanced_timestamp_engine_rx_set(struct nss_phy_device *nss_phydev,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	union ptp_rx_com_ts_ctrl_reg_u ptp_rx_com_ts_ctrl_reg = {0};
	union ptp_rx_filt_mac_da0_reg_u ptp_rx_filt_mac_da0_reg = {0};
	union ptp_rx_filt_mac_da1_reg_u ptp_rx_filt_mac_da1_reg = {0};
	union ptp_rx_filt_mac_da2_reg_u ptp_rx_filt_mac_da2_reg = {0};
	union ptp_rx_filt_ipv4_da0_reg_u ptp_rx_filt_ipv4_da0_reg = {0};
	union ptp_rx_filt_ipv4_da1_reg_u ptp_rx_filt_ipv4_da1_reg = {0};
	union ptp_rx_filt_ipv6_da0_reg_u ptp_rx_filt_ipv6_da0_reg = {0};
	union ptp_rx_filt_ipv6_da1_reg_u ptp_rx_filt_ipv6_da1_reg = {0};
	union ptp_rx_filt_ipv6_da2_reg_u ptp_rx_filt_ipv6_da2_reg = {0};
	union ptp_rx_filt_ipv6_da3_reg_u ptp_rx_filt_ipv6_da3_reg = {0};
	union ptp_rx_filt_ipv6_da4_reg_u ptp_rx_filt_ipv6_da4_reg = {0};
	union ptp_rx_filt_ipv6_da5_reg_u ptp_rx_filt_ipv6_da5_reg = {0};
	union ptp_rx_filt_ipv6_da6_reg_u ptp_rx_filt_ipv6_da6_reg = {0};
	union ptp_rx_filt_ipv6_da7_reg_u ptp_rx_filt_ipv6_da7_reg = {0};
	union ptp_rx_filt_mac_lengthtype_reg_u ptp_rx_filt_mac_lengthtype_reg = {0};
	union ptp_rx_filt_layer4_protocol_reg_u ptp_rx_filt_layer4_protocol_reg = {0};
	union ptp_rx_filt_udp_port_reg_u ptp_rx_filt_udp_port_reg = {0};

	union ptp_loc_mac_addr_0_reg_u ptp_loc_mac_addr_0_reg = {0};
	union ptp_loc_mac_addr_1_reg_u ptp_loc_mac_addr_1_reg = {0};
	union ptp_loc_mac_addr_2_reg_u ptp_loc_mac_addr_2_reg = {0};

	nss_ptp_rx_com_ts_ctrl_reg_get(nss_phydev,
				&ptp_rx_com_ts_ctrl_reg);
	ptp_rx_com_ts_ctrl_reg.bf.filt_en = ts_engine->filt_en;
	ptp_rx_com_ts_ctrl_reg.bf.mac_lengthtype_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_MAC_LENGTHTYPE_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.mac_da_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_MAC_DA_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.mac_ptp_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_MAC_PTP_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.ipv4_layer4_protocol_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_IPV4_LAYER4_PROTOCOL_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.ipv4_da_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_IPV4_DA_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.ipv4_ptp_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_IPV4_PTP_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.ipv6_next_header_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_IPV6_NEXT_HEADER_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.ipv6_da_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_IPV6_DA_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.ipv6_ptp_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_IPV6_PTP_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.udp_dport_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_UDP_DPORT_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.udp_ptp_event_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_UDP_PTP_EVENT_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.y1731_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_Y1731_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.y1731_insert_ts_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_Y1731_INSERT_TS_EN_OFFSET-1)
	 ) & true;
	ptp_rx_com_ts_ctrl_reg.bf.y1731_da_chk_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_RX_COM_TS_CTRL_REG_Y1731_DA_CHK_EN_OFFSET-1)
	 ) & true;
	nss_ptp_rx_com_ts_ctrl_reg_set(nss_phydev,
				&ptp_rx_com_ts_ctrl_reg);

	nss_ptp_rx_filt_mac_lengthtype_reg_get(nss_phydev,
				&ptp_rx_filt_mac_lengthtype_reg);
	ptp_rx_filt_mac_lengthtype_reg.bf.length_type = ts_engine->eth_type;
	nss_ptp_rx_filt_mac_lengthtype_reg_set(nss_phydev,
				&ptp_rx_filt_mac_lengthtype_reg);

	nss_ptp_rx_filt_mac_da0_reg_get(nss_phydev,
				&ptp_rx_filt_mac_da0_reg);
	ptp_rx_filt_mac_da0_reg.bf.mac_addr = (ts_engine->dmac_addr[0] << 8) |
		ts_engine->dmac_addr[1];
	nss_ptp_rx_filt_mac_da0_reg_set(nss_phydev,
				&ptp_rx_filt_mac_da0_reg);

	nss_ptp_rx_filt_mac_da1_reg_get(nss_phydev,
				&ptp_rx_filt_mac_da1_reg);
	ptp_rx_filt_mac_da1_reg.bf.mac_addr = (ts_engine->dmac_addr[2] << 8) |
		ts_engine->dmac_addr[3];
	nss_ptp_rx_filt_mac_da1_reg_set(nss_phydev,
				&ptp_rx_filt_mac_da1_reg);

	nss_ptp_rx_filt_mac_da2_reg_get(nss_phydev,
				&ptp_rx_filt_mac_da2_reg);
	ptp_rx_filt_mac_da2_reg.bf.mac_addr = (ts_engine->dmac_addr[4] << 8) |
		ts_engine->dmac_addr[5];
	nss_ptp_rx_filt_mac_da2_reg_set(nss_phydev,
				&ptp_rx_filt_mac_da2_reg);

	nss_ptp_rx_filt_layer4_protocol_reg_get(nss_phydev,
				&ptp_rx_filt_layer4_protocol_reg);
	ptp_rx_filt_layer4_protocol_reg.bf.l4_protocol = ts_engine->ipv4_l4_proto;
	nss_ptp_rx_filt_layer4_protocol_reg_set(nss_phydev,
				&ptp_rx_filt_layer4_protocol_reg);

	nss_ptp_rx_filt_ipv4_da0_reg_get(nss_phydev,
				&ptp_rx_filt_ipv4_da0_reg);
	ptp_rx_filt_ipv4_da0_reg.bf.ip_addr = ts_engine->ipv4_dip >> 16;
	nss_ptp_rx_filt_ipv4_da0_reg_set(nss_phydev,
				&ptp_rx_filt_ipv4_da0_reg);

	nss_ptp_rx_filt_ipv4_da1_reg_get(nss_phydev,
				&ptp_rx_filt_ipv4_da1_reg);
	ptp_rx_filt_ipv4_da1_reg.bf.ip_addr = ts_engine->ipv4_dip & 0xffff;
	nss_ptp_rx_filt_ipv4_da1_reg_set(nss_phydev,
				&ptp_rx_filt_ipv4_da1_reg);;

	nss_ptp_rx_filt_ipv6_da0_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da0_reg);
	ptp_rx_filt_ipv6_da0_reg.bf.ip_addr = ts_engine->ipv6_dip[0] >> 16;
	nss_ptp_rx_filt_ipv6_da0_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da0_reg);

	nss_ptp_rx_filt_ipv6_da1_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da1_reg);
	ptp_rx_filt_ipv6_da1_reg.bf.ip_addr = ts_engine->ipv6_dip[0] & 0xffff;
	nss_ptp_rx_filt_ipv6_da1_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da1_reg);

	nss_ptp_rx_filt_ipv6_da2_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da2_reg);
	ptp_rx_filt_ipv6_da2_reg.bf.ip_addr = ts_engine->ipv6_dip[1] >> 16;
	nss_ptp_rx_filt_ipv6_da2_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da2_reg);

	nss_ptp_rx_filt_ipv6_da3_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da3_reg);
	ptp_rx_filt_ipv6_da3_reg.bf.ip_addr = ts_engine->ipv6_dip[1] & 0xffff;
	nss_ptp_rx_filt_ipv6_da3_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da3_reg);

	nss_ptp_rx_filt_ipv6_da4_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da4_reg);
	ptp_rx_filt_ipv6_da4_reg.bf.ip_addr = ts_engine->ipv6_dip[2] >> 16;
	nss_ptp_rx_filt_ipv6_da4_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da4_reg);

	nss_ptp_rx_filt_ipv6_da5_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da5_reg);
	ptp_rx_filt_ipv6_da5_reg.bf.ip_addr = ts_engine->ipv6_dip[2] & 0xffff;
	nss_ptp_rx_filt_ipv6_da5_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da5_reg);

	nss_ptp_rx_filt_ipv6_da6_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da6_reg);
	ptp_rx_filt_ipv6_da6_reg.bf.ip_addr = ts_engine->ipv6_dip[3] >> 16;
	nss_ptp_rx_filt_ipv6_da6_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da6_reg);

	nss_ptp_rx_filt_ipv6_da7_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da7_reg);
	ptp_rx_filt_ipv6_da7_reg.bf.ip_addr = ts_engine->ipv6_dip[3] & 0xffff;
	nss_ptp_rx_filt_ipv6_da7_reg_set(nss_phydev,
				&ptp_rx_filt_ipv6_da7_reg);

	nss_ptp_rx_filt_udp_port_reg_get(nss_phydev,
				&ptp_rx_filt_udp_port_reg);
	ptp_rx_filt_udp_port_reg.bf.udp_port = ts_engine->udp_dport;
	nss_ptp_rx_filt_udp_port_reg_set(nss_phydev,
				&ptp_rx_filt_udp_port_reg);

	nss_ptp_loc_mac_addr_0_reg_get(nss_phydev,
				&ptp_loc_mac_addr_0_reg);
	ptp_loc_mac_addr_0_reg.bf.mac_addr = (ts_engine->y1731_mac_addr[0] << 8) |
		ts_engine->y1731_mac_addr[1];
	nss_ptp_loc_mac_addr_0_reg_set(nss_phydev,
				&ptp_loc_mac_addr_0_reg);

	nss_ptp_loc_mac_addr_1_reg_get(nss_phydev,
				&ptp_loc_mac_addr_1_reg);
	ptp_loc_mac_addr_1_reg.bf.mac_addr = (ts_engine->y1731_mac_addr[2] << 8) |
		ts_engine->y1731_mac_addr[3];
	nss_ptp_loc_mac_addr_1_reg_set(nss_phydev,
				&ptp_loc_mac_addr_1_reg);

	nss_ptp_loc_mac_addr_2_reg_get(nss_phydev,
				&ptp_loc_mac_addr_2_reg);
	ptp_loc_mac_addr_2_reg.bf.mac_addr = (ts_engine->y1731_mac_addr[4] << 8) |
		ts_engine->y1731_mac_addr[5];
	nss_ptp_loc_mac_addr_2_reg_set(nss_phydev,
				&ptp_loc_mac_addr_2_reg);

	return 0;
}


static int
_nss_phy_ptp_enhanced_timestamp_engine_tx_set(struct nss_phy_device *nss_phydev,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	union ptp_tx_com_ts_ctrl_reg_u ptp_tx_com_ts_ctrl_reg = {0};
	union ptp_tx_filt_mac_da0_reg_u ptp_tx_filt_mac_da0_reg = {0};
	union ptp_tx_filt_mac_da1_reg_u ptp_tx_filt_mac_da1_reg = {0};
	union ptp_tx_filt_mac_da2_reg_u ptp_tx_filt_mac_da2_reg = {0};
	union ptp_tx_filt_ipv4_da0_reg_u ptp_tx_filt_ipv4_da0_reg = {0};
	union ptp_tx_filt_ipv4_da1_reg_u ptp_tx_filt_ipv4_da1_reg = {0};
	union ptp_tx_filt_ipv6_da0_reg_u ptp_tx_filt_ipv6_da0_reg = {0};
	union ptp_tx_filt_ipv6_da1_reg_u ptp_tx_filt_ipv6_da1_reg = {0};
	union ptp_tx_filt_ipv6_da2_reg_u ptp_tx_filt_ipv6_da2_reg = {0};
	union ptp_tx_filt_ipv6_da3_reg_u ptp_tx_filt_ipv6_da3_reg = {0};
	union ptp_tx_filt_ipv6_da4_reg_u ptp_tx_filt_ipv6_da4_reg = {0};
	union ptp_tx_filt_ipv6_da5_reg_u ptp_tx_filt_ipv6_da5_reg = {0};
	union ptp_tx_filt_ipv6_da6_reg_u ptp_tx_filt_ipv6_da6_reg = {0};
	union ptp_tx_filt_ipv6_da7_reg_u ptp_tx_filt_ipv6_da7_reg = {0};
	union ptp_tx_filt_mac_lengthtype_reg_u ptp_tx_filt_mac_lengthtype_reg = {0};
	union ptp_tx_filt_layer4_protocol_reg_u ptp_tx_filt_layer4_protocol_reg = {0};
	union ptp_tx_filt_udp_port_reg_u ptp_tx_filt_udp_port_reg = {0};

	union ptp_loc_mac_addr_0_reg_u ptp_loc_mac_addr_0_reg = {0};
	union ptp_loc_mac_addr_1_reg_u ptp_loc_mac_addr_1_reg = {0};
	union ptp_loc_mac_addr_2_reg_u ptp_loc_mac_addr_2_reg = {0};

	nss_ptp_tx_com_ts_ctrl_reg_get(nss_phydev,
				&ptp_tx_com_ts_ctrl_reg);
	ptp_tx_com_ts_ctrl_reg.bf.filt_en = ts_engine->filt_en;
	ptp_tx_com_ts_ctrl_reg.bf.mac_lengthtype_en = ts_engine->enhance_ts_conf_bmp &
		true;
	ptp_tx_com_ts_ctrl_reg.bf.mac_da_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_MAC_DA_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.mac_ptp_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_MAC_PTP_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.ipv4_layer4_protocol_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_IPV4_LAYER4_PROTOCOL_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.ipv4_da_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_IPV4_DA_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.ipv4_ptp_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_IPV4_PTP_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.ipv6_next_header_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_IPV6_NEXT_HEADER_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.ipv6_da_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_IPV6_DA_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.ipv6_ptp_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_IPV6_PTP_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.udp_dport_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_UDP_DPORT_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.udp_ptp_event_filt_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_UDP_PTP_EVENT_FILT_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.y1731_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_Y1731_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.y1731_insert_ts_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_Y1731_INSERT_TS_EN_OFFSET-1)
	 ) & true;
	ptp_tx_com_ts_ctrl_reg.bf.y1731_sa_chk_en =
	(ts_engine->enhance_ts_conf_bmp >> (PTP_TX_COM_TS_CTRL_REG_Y1731_SA_CHK_EN_OFFSET-1)
	 ) & true;
	nss_ptp_tx_com_ts_ctrl_reg_set(nss_phydev,
				&ptp_tx_com_ts_ctrl_reg);

	nss_ptp_tx_filt_mac_lengthtype_reg_get(nss_phydev,
				&ptp_tx_filt_mac_lengthtype_reg);
	ptp_tx_filt_mac_lengthtype_reg.bf.length_type = ts_engine->eth_type;
	nss_ptp_tx_filt_mac_lengthtype_reg_set(nss_phydev,
				&ptp_tx_filt_mac_lengthtype_reg);

	nss_ptp_tx_filt_mac_da0_reg_get(nss_phydev,
				&ptp_tx_filt_mac_da0_reg);
	ptp_tx_filt_mac_da0_reg.bf.mac_addr = (ts_engine->dmac_addr[0] << 8) |
		ts_engine->dmac_addr[1];
	nss_ptp_tx_filt_mac_da0_reg_set(nss_phydev,
				&ptp_tx_filt_mac_da0_reg);

	nss_ptp_tx_filt_mac_da1_reg_get(nss_phydev,
				&ptp_tx_filt_mac_da1_reg);
	ptp_tx_filt_mac_da1_reg.bf.mac_addr = (ts_engine->dmac_addr[2] << 8) |
		ts_engine->dmac_addr[3];
	nss_ptp_tx_filt_mac_da1_reg_set(nss_phydev,
				&ptp_tx_filt_mac_da1_reg);

	nss_ptp_tx_filt_mac_da2_reg_get(nss_phydev,
				&ptp_tx_filt_mac_da2_reg);
	ptp_tx_filt_mac_da2_reg.bf.mac_addr = (ts_engine->dmac_addr[4] << 8) |
		ts_engine->dmac_addr[5];
	nss_ptp_tx_filt_mac_da2_reg_set(nss_phydev,
				&ptp_tx_filt_mac_da2_reg);

	nss_ptp_tx_filt_layer4_protocol_reg_get(nss_phydev,
				&ptp_tx_filt_layer4_protocol_reg);
	ptp_tx_filt_layer4_protocol_reg.bf.l4_protocol = ts_engine->ipv4_l4_proto;
	nss_ptp_tx_filt_layer4_protocol_reg_set(nss_phydev,
				&ptp_tx_filt_layer4_protocol_reg);

	nss_ptp_tx_filt_ipv4_da0_reg_get(nss_phydev,
				&ptp_tx_filt_ipv4_da0_reg);
	ptp_tx_filt_ipv4_da0_reg.bf.ip_addr = ts_engine->ipv4_dip >> 16;
	nss_ptp_tx_filt_ipv4_da0_reg_set(nss_phydev,
				&ptp_tx_filt_ipv4_da0_reg);

	nss_ptp_tx_filt_ipv4_da1_reg_get(nss_phydev,
				&ptp_tx_filt_ipv4_da1_reg);
	ptp_tx_filt_ipv4_da1_reg.bf.ip_addr = ts_engine->ipv4_dip & 0xffff;
	nss_ptp_tx_filt_ipv4_da1_reg_set(nss_phydev,
				&ptp_tx_filt_ipv4_da1_reg);;

	nss_ptp_tx_filt_ipv6_da0_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da0_reg);
	ptp_tx_filt_ipv6_da0_reg.bf.ip_addr = ts_engine->ipv6_dip[0] >> 16;
	nss_ptp_tx_filt_ipv6_da0_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da0_reg);

	nss_ptp_tx_filt_ipv6_da1_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da1_reg);
	ptp_tx_filt_ipv6_da1_reg.bf.ip_addr = ts_engine->ipv6_dip[0] & 0xffff;
	nss_ptp_tx_filt_ipv6_da1_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da1_reg);

	nss_ptp_tx_filt_ipv6_da2_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da2_reg);
	ptp_tx_filt_ipv6_da2_reg.bf.ip_addr = ts_engine->ipv6_dip[1] >> 16;
	nss_ptp_tx_filt_ipv6_da2_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da2_reg);

	nss_ptp_tx_filt_ipv6_da3_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da3_reg);
	ptp_tx_filt_ipv6_da3_reg.bf.ip_addr = ts_engine->ipv6_dip[1] & 0xffff;
	nss_ptp_tx_filt_ipv6_da3_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da3_reg);

	nss_ptp_tx_filt_ipv6_da4_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da4_reg);
	ptp_tx_filt_ipv6_da4_reg.bf.ip_addr = ts_engine->ipv6_dip[2] >> 16;
	nss_ptp_tx_filt_ipv6_da4_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da4_reg);

	nss_ptp_tx_filt_ipv6_da5_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da5_reg);
	ptp_tx_filt_ipv6_da5_reg.bf.ip_addr = ts_engine->ipv6_dip[2] & 0xffff;
	nss_ptp_tx_filt_ipv6_da5_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da5_reg);

	nss_ptp_tx_filt_ipv6_da6_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da6_reg);
	ptp_tx_filt_ipv6_da6_reg.bf.ip_addr = ts_engine->ipv6_dip[3] >> 16;
	nss_ptp_tx_filt_ipv6_da6_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da6_reg);

	nss_ptp_tx_filt_ipv6_da7_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da7_reg);
	ptp_tx_filt_ipv6_da7_reg.bf.ip_addr = ts_engine->ipv6_dip[3] & 0xffff;
	nss_ptp_tx_filt_ipv6_da7_reg_set(nss_phydev,
				&ptp_tx_filt_ipv6_da7_reg);

	nss_ptp_tx_filt_udp_port_reg_get(nss_phydev,
				&ptp_tx_filt_udp_port_reg);
	ptp_tx_filt_udp_port_reg.bf.udp_port = ts_engine->udp_dport;
	nss_ptp_tx_filt_udp_port_reg_set(nss_phydev,
				&ptp_tx_filt_udp_port_reg);

	nss_ptp_loc_mac_addr_0_reg_get(nss_phydev,
				&ptp_loc_mac_addr_0_reg);
	ptp_loc_mac_addr_0_reg.bf.mac_addr = (ts_engine->y1731_mac_addr[0] << 8) |
		ts_engine->y1731_mac_addr[1];
	nss_ptp_loc_mac_addr_0_reg_set(nss_phydev,
				&ptp_loc_mac_addr_0_reg);

	nss_ptp_loc_mac_addr_1_reg_get(nss_phydev,
				&ptp_loc_mac_addr_1_reg);
	ptp_loc_mac_addr_1_reg.bf.mac_addr = (ts_engine->y1731_mac_addr[2] << 8) |
		ts_engine->y1731_mac_addr[3];
	nss_ptp_loc_mac_addr_1_reg_set(nss_phydev,
				&ptp_loc_mac_addr_1_reg);

	nss_ptp_loc_mac_addr_2_reg_get(nss_phydev,
				&ptp_loc_mac_addr_2_reg);
	ptp_loc_mac_addr_2_reg.bf.mac_addr = (ts_engine->y1731_mac_addr[4] << 8) |
		ts_engine->y1731_mac_addr[5];
	nss_ptp_loc_mac_addr_2_reg_set(nss_phydev,
				&ptp_loc_mac_addr_2_reg);

	return 0;
}

static int
nss_phy_ptp_enhanced_timestamp_engine_set(struct nss_phy_device *nss_phydev,
		fal_ptp_direction_t direction,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	if (direction == FAL_RX_DIRECTION)
		return _nss_phy_ptp_enhanced_timestamp_engine_rx_set(nss_phydev, ts_engine);

	return _nss_phy_ptp_enhanced_timestamp_engine_tx_set(nss_phydev, ts_engine);
}

static int
_nss_phy_ptp_enhanced_timestamp_engine_rx_com_ts_get(struct nss_phy_device *nss_phydev,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	int ret = 0;

	union ptp_rx_com_timestamp0_reg_u ptp_rx_com_timestamp0_reg = {0};
	union ptp_rx_com_timestamp1_reg_u ptp_rx_com_timestamp1_reg = {0};
	union ptp_rx_com_timestamp2_reg_u ptp_rx_com_timestamp2_reg = {0};
	union ptp_rx_com_timestamp3_reg_u ptp_rx_com_timestamp3_reg = {0};
	union ptp_rx_com_timestamp4_reg_u ptp_rx_com_timestamp4_reg = {0};
	union ptp_rx_com_frac_nano_reg_u ptp_rx_com_frac_nano_reg = {0};

	nss_ptp_rx_com_timestamp0_reg_get(nss_phydev,
				&ptp_rx_com_timestamp0_reg);
	nss_ptp_rx_com_timestamp1_reg_get(nss_phydev,
				&ptp_rx_com_timestamp1_reg);
	nss_ptp_rx_com_timestamp2_reg_get(nss_phydev,
				&ptp_rx_com_timestamp2_reg);
	ts_engine->timestamp.seconds =
		((u64)ptp_rx_com_timestamp0_reg.bf.com_ts << 32) |
		(ptp_rx_com_timestamp1_reg.bf.com_ts << 16) | ptp_rx_com_timestamp2_reg.bf.com_ts;

	nss_ptp_rx_com_timestamp3_reg_get(nss_phydev,
				&ptp_rx_com_timestamp3_reg);
	nss_ptp_rx_com_timestamp4_reg_get(nss_phydev,
				&ptp_rx_com_timestamp4_reg);
	ts_engine->timestamp.nanoseconds = (ptp_rx_com_timestamp3_reg.bf.com_ts << 16) |
		ptp_rx_com_timestamp4_reg.bf.com_ts;

	nss_ptp_rx_com_frac_nano_reg_get(nss_phydev,
				&ptp_rx_com_frac_nano_reg);
	ts_engine->timestamp.fracnanoseconds = ptp_rx_com_frac_nano_reg.bf.frac_nano;

	return ret;
}

static int
_nss_phy_ptp_enhanced_timestamp_engine_rx_com_ts_pre_get(struct nss_phy_device *nss_phydev,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	int ret = 0;
	union ptp_rx_com_timestamp_pre0_reg_u ptp_rx_com_timestamp_pre0_reg = {0};
	union ptp_rx_com_timestamp_pre1_reg_u ptp_rx_com_timestamp_pre1_reg = {0};
	union ptp_rx_com_timestamp_pre2_reg_u ptp_rx_com_timestamp_pre2_reg = {0};
	union ptp_rx_com_timestamp_pre3_reg_u ptp_rx_com_timestamp_pre3_reg = {0};
	union ptp_rx_com_timestamp_pre4_reg_u ptp_rx_com_timestamp_pre4_reg = {0};
	union ptp_rx_com_frac_nano_pre_reg_u ptp_rx_com_frac_nano_pre_reg = {0};

	nss_ptp_rx_com_timestamp_pre0_reg_get(nss_phydev,
				&ptp_rx_com_timestamp_pre0_reg);
	nss_ptp_rx_com_timestamp_pre1_reg_get(nss_phydev,
				&ptp_rx_com_timestamp_pre1_reg);
	nss_ptp_rx_com_timestamp_pre2_reg_get(nss_phydev,
				&ptp_rx_com_timestamp_pre2_reg);
	ts_engine->timestamp_pre.seconds =
		((u64)ptp_rx_com_timestamp_pre0_reg.bf.com_ts_pre << 32) |
		(ptp_rx_com_timestamp_pre1_reg.bf.com_ts_pre << 16) |
		ptp_rx_com_timestamp_pre2_reg.bf.com_ts_pre;

	nss_ptp_rx_com_timestamp_pre3_reg_get(nss_phydev,
				&ptp_rx_com_timestamp_pre3_reg);
	nss_ptp_rx_com_timestamp_pre4_reg_get(nss_phydev,
				&ptp_rx_com_timestamp_pre4_reg);
	ts_engine->timestamp_pre.nanoseconds =
		(ptp_rx_com_timestamp_pre3_reg.bf.com_ts_pre << 16) |
		ptp_rx_com_timestamp_pre4_reg.bf.com_ts_pre;

	nss_ptp_rx_com_frac_nano_pre_reg_get(nss_phydev,
				&ptp_rx_com_frac_nano_pre_reg);
	ts_engine->timestamp_pre.fracnanoseconds =
		ptp_rx_com_frac_nano_pre_reg.bf.frac_nano_pre;

	return ret;
}

static int
_nss_phy_ptp_enhanced_timestamp_engine_rx_get(struct nss_phy_device *nss_phydev,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	int ret = 0;
	union ptp_rx_com_ts_ctrl_reg_u ctrl_reg = {0};
	union ptp_rx_filt_mac_da0_reg_u ptp_rx_filt_mac_da0_reg = {0};
	union ptp_rx_filt_mac_da1_reg_u ptp_rx_filt_mac_da1_reg = {0};
	union ptp_rx_filt_mac_da2_reg_u ptp_rx_filt_mac_da2_reg = {0};
	union ptp_rx_filt_ipv4_da0_reg_u ptp_rx_filt_ipv4_da0_reg = {0};
	union ptp_rx_filt_ipv4_da1_reg_u ptp_rx_filt_ipv4_da1_reg = {0};
	union ptp_rx_filt_ipv6_da0_reg_u ptp_rx_filt_ipv6_da0_reg = {0};
	union ptp_rx_filt_ipv6_da1_reg_u ptp_rx_filt_ipv6_da1_reg = {0};
	union ptp_rx_filt_ipv6_da2_reg_u ptp_rx_filt_ipv6_da2_reg = {0};
	union ptp_rx_filt_ipv6_da3_reg_u ptp_rx_filt_ipv6_da3_reg = {0};
	union ptp_rx_filt_ipv6_da4_reg_u ptp_rx_filt_ipv6_da4_reg = {0};
	union ptp_rx_filt_ipv6_da5_reg_u ptp_rx_filt_ipv6_da5_reg = {0};
	union ptp_rx_filt_ipv6_da6_reg_u ptp_rx_filt_ipv6_da6_reg = {0};
	union ptp_rx_filt_ipv6_da7_reg_u ptp_rx_filt_ipv6_da7_reg = {0};
	union ptp_rx_filt_mac_lengthtype_reg_u ptp_rx_filt_mac_lengthtype_reg = {0};
	union ptp_rx_filt_layer4_protocol_reg_u ptp_rx_filt_layer4_protocol_reg = {0};
	union ptp_rx_filt_udp_port_reg_u ptp_rx_filt_udp_port_reg = {0};
	union ptp_rx_com_ts_status_reg_u ptp_rx_com_ts_status_reg = {0};
	union ptp_rx_com_ts_status_pre_reg_u ptp_rx_com_ts_status_pre_reg = {0};
	union ptp_rx_y1731_identify_reg_u ptp_rx_y1731_identify_reg = {0};
	union ptp_rx_y1731_identify_pre_reg_u ptp_rx_y1731_identify_pre_reg = {0};

	union ptp_loc_mac_addr_0_reg_u ptp_loc_mac_addr_0_reg = {0};
	union ptp_loc_mac_addr_1_reg_u ptp_loc_mac_addr_1_reg = {0};
	union ptp_loc_mac_addr_2_reg_u ptp_loc_mac_addr_2_reg = {0};

	nss_ptp_rx_com_ts_ctrl_reg_get(nss_phydev,
				&ctrl_reg);
	ts_engine->filt_en = ctrl_reg.bf.filt_en;
	ts_engine->enhance_ts_conf_bmp = ctrl_reg.bf.mac_lengthtype_en |
	(ctrl_reg.bf.mac_da_en << (PTP_RX_COM_TS_CTRL_REG_MAC_DA_EN_OFFSET-1)) |
	(ctrl_reg.bf.mac_ptp_filt_en << (PTP_RX_COM_TS_CTRL_REG_MAC_PTP_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.
	 ipv4_layer4_protocol_en << (PTP_RX_COM_TS_CTRL_REG_IPV4_LAYER4_PROTOCOL_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv4_da_en << (PTP_RX_COM_TS_CTRL_REG_IPV4_DA_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv4_ptp_filt_en << (PTP_RX_COM_TS_CTRL_REG_IPV4_PTP_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv6_next_header_en << (PTP_RX_COM_TS_CTRL_REG_IPV6_NEXT_HEADER_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv6_da_filt_en << (PTP_RX_COM_TS_CTRL_REG_IPV6_DA_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv6_ptp_filt_en << (PTP_RX_COM_TS_CTRL_REG_IPV6_PTP_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.udp_dport_en << (PTP_RX_COM_TS_CTRL_REG_UDP_DPORT_EN_OFFSET-1)) |
	(ctrl_reg.bf.
	 udp_ptp_event_filt_en << (PTP_RX_COM_TS_CTRL_REG_UDP_PTP_EVENT_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.y1731_en << (PTP_RX_COM_TS_CTRL_REG_Y1731_EN_OFFSET-1)) |
	(ctrl_reg.bf.y1731_insert_ts_en << (PTP_RX_COM_TS_CTRL_REG_Y1731_INSERT_TS_EN_OFFSET-1)) |
	(ctrl_reg.bf.y1731_da_chk_en << (PTP_RX_COM_TS_CTRL_REG_Y1731_DA_CHK_EN_OFFSET-1));

	nss_ptp_rx_filt_mac_lengthtype_reg_get(nss_phydev,
				&ptp_rx_filt_mac_lengthtype_reg);
	ts_engine->eth_type = ptp_rx_filt_mac_lengthtype_reg.bf.length_type;

	nss_ptp_rx_filt_mac_da0_reg_get(nss_phydev,
				&ptp_rx_filt_mac_da0_reg);
	ts_engine->dmac_addr[0] = ptp_rx_filt_mac_da0_reg.bf.mac_addr >> 8;
	ts_engine->dmac_addr[1] = ptp_rx_filt_mac_da0_reg.bf.mac_addr & 0xff;

	nss_ptp_rx_filt_mac_da1_reg_get(nss_phydev,
				&ptp_rx_filt_mac_da1_reg);
	ts_engine->dmac_addr[2] = ptp_rx_filt_mac_da1_reg.bf.mac_addr >> 8;
	ts_engine->dmac_addr[3] = ptp_rx_filt_mac_da1_reg.bf.mac_addr & 0xff;

	nss_ptp_rx_filt_mac_da2_reg_get(nss_phydev,
				&ptp_rx_filt_mac_da2_reg);
	ts_engine->dmac_addr[4] = ptp_rx_filt_mac_da2_reg.bf.mac_addr >> 8;
	ts_engine->dmac_addr[5] = ptp_rx_filt_mac_da2_reg.bf.mac_addr & 0xff;

	nss_ptp_rx_filt_layer4_protocol_reg_get(nss_phydev,
				&ptp_rx_filt_layer4_protocol_reg);
	ts_engine->ipv4_l4_proto = ptp_rx_filt_layer4_protocol_reg.bf.l4_protocol;

	nss_ptp_rx_filt_ipv4_da0_reg_get(nss_phydev,
				&ptp_rx_filt_ipv4_da0_reg);
	nss_ptp_rx_filt_ipv4_da1_reg_get(nss_phydev,
				&ptp_rx_filt_ipv4_da1_reg);
	ts_engine->ipv4_dip = (ptp_rx_filt_ipv4_da0_reg.bf.ip_addr << 16) |
		ptp_rx_filt_ipv4_da1_reg.bf.ip_addr;

	nss_ptp_rx_filt_ipv6_da0_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da0_reg);
	nss_ptp_rx_filt_ipv6_da1_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da1_reg);
	ts_engine->ipv6_dip[0] = (ptp_rx_filt_ipv6_da0_reg.bf.ip_addr << 16) |
		ptp_rx_filt_ipv6_da1_reg.bf.ip_addr;

	nss_ptp_rx_filt_ipv6_da2_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da2_reg);
	nss_ptp_rx_filt_ipv6_da3_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da3_reg);
	ts_engine->ipv6_dip[1] = (ptp_rx_filt_ipv6_da2_reg.bf.ip_addr << 16) |
		ptp_rx_filt_ipv6_da3_reg.bf.ip_addr;

	nss_ptp_rx_filt_ipv6_da4_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da4_reg);
	nss_ptp_rx_filt_ipv6_da5_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da5_reg);
	ts_engine->ipv6_dip[2] = (ptp_rx_filt_ipv6_da4_reg.bf.ip_addr << 16) |
		ptp_rx_filt_ipv6_da5_reg.bf.ip_addr;

	nss_ptp_rx_filt_ipv6_da6_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da6_reg);
	nss_ptp_rx_filt_ipv6_da7_reg_get(nss_phydev,
				&ptp_rx_filt_ipv6_da7_reg);
	ts_engine->ipv6_dip[3] = (ptp_rx_filt_ipv6_da6_reg.bf.ip_addr << 16) |
		ptp_rx_filt_ipv6_da7_reg.bf.ip_addr;

	nss_ptp_rx_filt_udp_port_reg_get(nss_phydev,
				&ptp_rx_filt_udp_port_reg);
	ts_engine->udp_dport = ptp_rx_filt_udp_port_reg.bf.udp_port;

	nss_ptp_loc_mac_addr_0_reg_get(nss_phydev,
				&ptp_loc_mac_addr_0_reg);
	ts_engine->y1731_mac_addr[0] = ptp_loc_mac_addr_0_reg.bf.mac_addr >> 8;
	ts_engine->y1731_mac_addr[1] = ptp_loc_mac_addr_0_reg.bf.mac_addr & 0xff;

	nss_ptp_loc_mac_addr_1_reg_get(nss_phydev,
				&ptp_loc_mac_addr_1_reg);
	ts_engine->y1731_mac_addr[2] = ptp_loc_mac_addr_1_reg.bf.mac_addr >> 8;
	ts_engine->y1731_mac_addr[3] = ptp_loc_mac_addr_1_reg.bf.mac_addr & 0xff;

	nss_ptp_loc_mac_addr_2_reg_get(nss_phydev,
				&ptp_loc_mac_addr_2_reg);
	ts_engine->y1731_mac_addr[4] = ptp_loc_mac_addr_2_reg.bf.mac_addr >> 8;
	ts_engine->y1731_mac_addr[5] = ptp_loc_mac_addr_2_reg.bf.mac_addr & 0xff;

	nss_ptp_rx_com_ts_status_reg_get(nss_phydev,
				&ptp_rx_com_ts_status_reg);
	ts_engine->enhance_ts_status_bmp = ptp_rx_com_ts_status_reg.bf.mac_lengthtype |
		(ptp_rx_com_ts_status_reg.bf.mac_da <<
		 PTP_RX_COM_TS_STATUS_REG_MAC_DA_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.mac_ptp_prim_addr <<
		 PTP_RX_COM_TS_STATUS_REG_MAC_PTP_PRIM_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.mac_ptp_pdelay_addr <<
		 PTP_RX_COM_TS_STATUS_REG_MAC_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv4_layer4_protocol <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_LAYER4_PROTOCOL_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv4_da <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_DA_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv4_ptp_prim_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_PTP_PRIM_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv4_ptp_pdelay_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv6_next_header <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_NEXT_HEADER_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv6_da <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_DA_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv6_ptp_prim_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_PTP_PRIM_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.ipv6_ptp_pdelay_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.udp_dport <<
		 PTP_RX_COM_TS_STATUS_REG_UDP_DPORT_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.udp_ptp_event_dport <<
		 PTP_RX_COM_TS_STATUS_REG_UDP_PTP_EVENT_DPORT_OFFSET) |
		(ptp_rx_com_ts_status_reg.bf.y1731_mach <<
		 PTP_RX_COM_TS_STATUS_REG_Y1731_MACH_OFFSET);

	nss_ptp_rx_com_ts_status_pre_reg_get(nss_phydev,
				&ptp_rx_com_ts_status_pre_reg);
	ts_engine->enhance_ts_status_pre_bmp = ptp_rx_com_ts_status_pre_reg.bf.mac_lengthtype |
		(ptp_rx_com_ts_status_pre_reg.bf.mac_da <<
		 PTP_RX_COM_TS_STATUS_REG_MAC_DA_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.mac_ptp_prim_addr <<
		 PTP_RX_COM_TS_STATUS_REG_MAC_PTP_PRIM_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.mac_ptp_pdelay_addr <<
		 PTP_RX_COM_TS_STATUS_REG_MAC_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv4_layer4_protocol <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_LAYER4_PROTOCOL_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv4_da <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_DA_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv4_ptp_prim_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_PTP_PRIM_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv4_ptp_pdelay_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV4_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv6_next_header <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_NEXT_HEADER_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv6_da <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_DA_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv6_ptp_prim_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_PTP_PRIM_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.ipv6_ptp_pdelay_addr <<
		 PTP_RX_COM_TS_STATUS_REG_IPV6_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.udp_dport <<
		 PTP_RX_COM_TS_STATUS_REG_UDP_DPORT_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.udp_ptp_event_dport <<
		 PTP_RX_COM_TS_STATUS_REG_UDP_PTP_EVENT_DPORT_OFFSET) |
		(ptp_rx_com_ts_status_pre_reg.bf.y1731_mach <<
		 PTP_RX_COM_TS_STATUS_REG_Y1731_MACH_OFFSET);

	nss_ptp_rx_y1731_identify_reg_get(nss_phydev,
				&ptp_rx_y1731_identify_reg);
	ts_engine->y1731_identity = ptp_rx_y1731_identify_reg.bf.identify;

	nss_ptp_rx_y1731_identify_pre_reg_get(nss_phydev,
				&ptp_rx_y1731_identify_pre_reg);
	ts_engine->y1731_identity_pre = ptp_rx_y1731_identify_pre_reg.bf.identify_pre;

	ret = _nss_phy_ptp_enhanced_timestamp_engine_rx_com_ts_get(nss_phydev,
			ts_engine);
	if (ret != 0)
	{
		return ret;
	}

	return _nss_phy_ptp_enhanced_timestamp_engine_rx_com_ts_pre_get(nss_phydev, ts_engine);
}

static int
_nss_phy_ptp_enhanced_timestamp_engine_tx_get(struct nss_phy_device *nss_phydev,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	union ptp_tx_com_ts_ctrl_reg_u ctrl_reg = {0};
	union ptp_tx_filt_mac_da0_reg_u ptp_tx_filt_mac_da0_reg = {0};
	union ptp_tx_filt_mac_da1_reg_u ptp_tx_filt_mac_da1_reg = {0};
	union ptp_tx_filt_mac_da2_reg_u ptp_tx_filt_mac_da2_reg = {0};
	union ptp_tx_filt_ipv4_da0_reg_u ptp_tx_filt_ipv4_da0_reg = {0};
	union ptp_tx_filt_ipv4_da1_reg_u ptp_tx_filt_ipv4_da1_reg = {0};
	union ptp_tx_filt_ipv6_da0_reg_u ptp_tx_filt_ipv6_da0_reg = {0};
	union ptp_tx_filt_ipv6_da1_reg_u ptp_tx_filt_ipv6_da1_reg = {0};
	union ptp_tx_filt_ipv6_da2_reg_u ptp_tx_filt_ipv6_da2_reg = {0};
	union ptp_tx_filt_ipv6_da3_reg_u ptp_tx_filt_ipv6_da3_reg = {0};
	union ptp_tx_filt_ipv6_da4_reg_u ptp_tx_filt_ipv6_da4_reg = {0};
	union ptp_tx_filt_ipv6_da5_reg_u ptp_tx_filt_ipv6_da5_reg = {0};
	union ptp_tx_filt_ipv6_da6_reg_u ptp_tx_filt_ipv6_da6_reg = {0};
	union ptp_tx_filt_ipv6_da7_reg_u ptp_tx_filt_ipv6_da7_reg = {0};
	union ptp_tx_filt_mac_lengthtype_reg_u ptp_tx_filt_mac_lengthtype_reg = {0};
	union ptp_tx_filt_layer4_protocol_reg_u ptp_tx_filt_layer4_protocol_reg = {0};
	union ptp_tx_filt_udp_port_reg_u ptp_tx_filt_udp_port_reg = {0};
	union ptp_tx_com_ts_status_reg_u ptp_tx_com_ts_status_reg = {0};
	union ptp_tx_com_timestamp0_reg_u ptp_tx_com_timestamp0_reg = {0};
	union ptp_tx_com_timestamp1_reg_u ptp_tx_com_timestamp1_reg = {0};
	union ptp_tx_com_timestamp2_reg_u ptp_tx_com_timestamp2_reg = {0};
	union ptp_tx_com_timestamp3_reg_u ptp_tx_com_timestamp3_reg = {0};
	union ptp_tx_com_timestamp4_reg_u ptp_tx_com_timestamp4_reg = {0};
	union ptp_tx_com_frac_nano_reg_u ptp_tx_com_frac_nano_reg = {0};
	union ptp_tx_y1731_identify_reg_u ptp_tx_y1731_identify_reg = {0};

	union ptp_loc_mac_addr_0_reg_u ptp_loc_mac_addr_0_reg = {0};
	union ptp_loc_mac_addr_1_reg_u ptp_loc_mac_addr_1_reg = {0};
	union ptp_loc_mac_addr_2_reg_u ptp_loc_mac_addr_2_reg = {0};

	nss_ptp_tx_com_ts_ctrl_reg_get(nss_phydev,
				&ctrl_reg);
	ts_engine->filt_en = ctrl_reg.bf.filt_en;
	ts_engine->enhance_ts_conf_bmp = ctrl_reg.bf.mac_lengthtype_en |
	(ctrl_reg.bf.mac_da_en << (PTP_TX_COM_TS_CTRL_REG_MAC_DA_EN_OFFSET-1)) |
	(ctrl_reg.bf.mac_ptp_filt_en << (PTP_TX_COM_TS_CTRL_REG_MAC_PTP_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.
	 ipv4_layer4_protocol_en << (PTP_TX_COM_TS_CTRL_REG_IPV4_LAYER4_PROTOCOL_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv4_da_en << (PTP_TX_COM_TS_CTRL_REG_IPV4_DA_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv4_ptp_filt_en << (PTP_TX_COM_TS_CTRL_REG_IPV4_PTP_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv6_next_header_en << (PTP_TX_COM_TS_CTRL_REG_IPV6_NEXT_HEADER_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv6_da_en << (PTP_TX_COM_TS_CTRL_REG_IPV6_DA_EN_OFFSET-1)) |
	(ctrl_reg.bf.ipv6_ptp_filt_en << (PTP_TX_COM_TS_CTRL_REG_IPV6_PTP_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.udp_dport_en << (PTP_TX_COM_TS_CTRL_REG_UDP_DPORT_EN_OFFSET-1)) |
	(ctrl_reg.bf.
	 udp_ptp_event_filt_en << (PTP_TX_COM_TS_CTRL_REG_UDP_PTP_EVENT_FILT_EN_OFFSET-1)) |
	(ctrl_reg.bf.y1731_en << (PTP_TX_COM_TS_CTRL_REG_Y1731_EN_OFFSET-1)) |
	(ctrl_reg.bf.y1731_insert_ts_en << (PTP_TX_COM_TS_CTRL_REG_Y1731_INSERT_TS_EN_OFFSET-1)) |
	(ctrl_reg.bf.y1731_sa_chk_en << (PTP_TX_COM_TS_CTRL_REG_Y1731_SA_CHK_EN_OFFSET-1));

	nss_ptp_tx_filt_mac_lengthtype_reg_get(nss_phydev,
				&ptp_tx_filt_mac_lengthtype_reg);
	ts_engine->eth_type = ptp_tx_filt_mac_lengthtype_reg.bf.length_type;

	nss_ptp_tx_filt_mac_da0_reg_get(nss_phydev,
				&ptp_tx_filt_mac_da0_reg);
	ts_engine->dmac_addr[0] = ptp_tx_filt_mac_da0_reg.bf.mac_addr >> 8;
	ts_engine->dmac_addr[1] = ptp_tx_filt_mac_da0_reg.bf.mac_addr & 0xff;

	nss_ptp_tx_filt_mac_da1_reg_get(nss_phydev,
				&ptp_tx_filt_mac_da1_reg);
	ts_engine->dmac_addr[2] = ptp_tx_filt_mac_da1_reg.bf.mac_addr >> 8;
	ts_engine->dmac_addr[3] = ptp_tx_filt_mac_da1_reg.bf.mac_addr & 0xff;

	nss_ptp_tx_filt_mac_da2_reg_get(nss_phydev,
				&ptp_tx_filt_mac_da2_reg);
	ts_engine->dmac_addr[4] = ptp_tx_filt_mac_da2_reg.bf.mac_addr >> 8;
	ts_engine->dmac_addr[5] = ptp_tx_filt_mac_da2_reg.bf.mac_addr & 0xff;

	nss_ptp_tx_filt_layer4_protocol_reg_get(nss_phydev,
				&ptp_tx_filt_layer4_protocol_reg);
	ts_engine->ipv4_l4_proto = ptp_tx_filt_layer4_protocol_reg.bf.l4_protocol;

	nss_ptp_tx_filt_ipv4_da0_reg_get(nss_phydev,
				&ptp_tx_filt_ipv4_da0_reg);
	nss_ptp_tx_filt_ipv4_da1_reg_get(nss_phydev,
				&ptp_tx_filt_ipv4_da1_reg);
	ts_engine->ipv4_dip = (ptp_tx_filt_ipv4_da0_reg.bf.ip_addr << 16) |
		ptp_tx_filt_ipv4_da1_reg.bf.ip_addr;

	nss_ptp_tx_filt_ipv6_da0_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da0_reg);
	nss_ptp_tx_filt_ipv6_da1_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da1_reg);
	ts_engine->ipv6_dip[0] = (ptp_tx_filt_ipv6_da0_reg.bf.ip_addr << 16) |
		ptp_tx_filt_ipv6_da1_reg.bf.ip_addr;

	nss_ptp_tx_filt_ipv6_da2_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da2_reg);
	nss_ptp_tx_filt_ipv6_da3_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da3_reg);
	ts_engine->ipv6_dip[1] = (ptp_tx_filt_ipv6_da2_reg.bf.ip_addr << 16) |
		ptp_tx_filt_ipv6_da3_reg.bf.ip_addr;

	nss_ptp_tx_filt_ipv6_da4_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da4_reg);
	nss_ptp_tx_filt_ipv6_da5_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da5_reg);
	ts_engine->ipv6_dip[2] = (ptp_tx_filt_ipv6_da4_reg.bf.ip_addr << 16) |
		ptp_tx_filt_ipv6_da5_reg.bf.ip_addr;

	nss_ptp_tx_filt_ipv6_da6_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da6_reg);
	nss_ptp_tx_filt_ipv6_da7_reg_get(nss_phydev,
				&ptp_tx_filt_ipv6_da7_reg);
	ts_engine->ipv6_dip[3] = (ptp_tx_filt_ipv6_da6_reg.bf.ip_addr << 16) |
		ptp_tx_filt_ipv6_da7_reg.bf.ip_addr;

	nss_ptp_tx_filt_udp_port_reg_get(nss_phydev,
				&ptp_tx_filt_udp_port_reg);
	ts_engine->udp_dport = ptp_tx_filt_udp_port_reg.bf.udp_port;

	nss_ptp_loc_mac_addr_0_reg_get(nss_phydev,
				&ptp_loc_mac_addr_0_reg);
	ts_engine->y1731_mac_addr[0] = ptp_loc_mac_addr_0_reg.bf.mac_addr >> 8;
	ts_engine->y1731_mac_addr[1] = ptp_loc_mac_addr_0_reg.bf.mac_addr & 0xff;

	nss_ptp_loc_mac_addr_1_reg_get(nss_phydev,
				&ptp_loc_mac_addr_1_reg);
	ts_engine->y1731_mac_addr[2] = ptp_loc_mac_addr_1_reg.bf.mac_addr >> 8;
	ts_engine->y1731_mac_addr[3] = ptp_loc_mac_addr_1_reg.bf.mac_addr & 0xff;

	nss_ptp_loc_mac_addr_2_reg_get(nss_phydev,
				&ptp_loc_mac_addr_2_reg);
	ts_engine->y1731_mac_addr[4] = ptp_loc_mac_addr_2_reg.bf.mac_addr >> 8;
	ts_engine->y1731_mac_addr[5] = ptp_loc_mac_addr_2_reg.bf.mac_addr & 0xff;

	nss_ptp_tx_com_ts_status_reg_get(nss_phydev,
				&ptp_tx_com_ts_status_reg);
	ts_engine->enhance_ts_status_bmp = ptp_tx_com_ts_status_reg.bf.mac_lengthtype |
		(ptp_tx_com_ts_status_reg.bf.mac_da <<
		 PTP_TX_COM_TS_STATUS_REG_MAC_DA_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.mac_ptp_prim_addr <<
		 PTP_TX_COM_TS_STATUS_REG_MAC_PTP_PRIM_ADDR_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.mac_ptp_pdelay_addr <<
		 PTP_TX_COM_TS_STATUS_REG_MAC_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv4_layer4_protocol <<
		 PTP_TX_COM_TS_STATUS_REG_IPV4_LAYER4_PROTOCOL_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv4_da <<
		 PTP_TX_COM_TS_STATUS_REG_IPV4_DA_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv4_ptp_prim_addr <<
		 PTP_TX_COM_TS_STATUS_REG_IPV4_PTP_PRIM_ADDR_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv4_ptp_pdelay_addr <<
		 PTP_TX_COM_TS_STATUS_REG_IPV4_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv6_next_header <<
		 PTP_TX_COM_TS_STATUS_REG_IPV6_NEXT_HEADER_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv6_da <<
		 PTP_TX_COM_TS_STATUS_REG_IPV6_DA_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv6_ptp_prim_addr <<
		 PTP_TX_COM_TS_STATUS_REG_IPV6_PTP_PRIM_ADDR_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.ipv6_ptp_pdelay_addr <<
		 PTP_TX_COM_TS_STATUS_REG_IPV6_PTP_PDELAY_ADDR_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.udp_dport <<
		 PTP_TX_COM_TS_STATUS_REG_UDP_DPORT_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.udp_ptp_event_dport <<
		 PTP_TX_COM_TS_STATUS_REG_UDP_PTP_EVENT_DPORT_OFFSET) |
		(ptp_tx_com_ts_status_reg.bf.y1731_mach <<
		 PTP_TX_COM_TS_STATUS_REG_Y1731_MACH_OFFSET);

	ts_engine->enhance_ts_status_pre_bmp = 0;

	nss_ptp_tx_y1731_identify_reg_get(nss_phydev,
				&ptp_tx_y1731_identify_reg);
	ts_engine->y1731_identity = ptp_tx_y1731_identify_reg.bf.identify;

	ts_engine->y1731_identity_pre = 0;

	nss_ptp_tx_com_timestamp0_reg_get(nss_phydev,
				&ptp_tx_com_timestamp0_reg);
	nss_ptp_tx_com_timestamp1_reg_get(nss_phydev,
				&ptp_tx_com_timestamp1_reg);
	nss_ptp_tx_com_timestamp2_reg_get(nss_phydev,
				&ptp_tx_com_timestamp2_reg);
	ts_engine->timestamp.seconds =
		((u64)ptp_tx_com_timestamp0_reg.bf.com_ts << 32) |
		(ptp_tx_com_timestamp1_reg.bf.com_ts << 16) |
		ptp_tx_com_timestamp2_reg.bf.com_ts;

	nss_ptp_tx_com_timestamp3_reg_get(nss_phydev,
				&ptp_tx_com_timestamp3_reg);
	nss_ptp_tx_com_timestamp4_reg_get(nss_phydev,
				&ptp_tx_com_timestamp4_reg);
	ts_engine->timestamp.nanoseconds = (ptp_tx_com_timestamp3_reg.bf.com_ts << 16) |
		ptp_tx_com_timestamp4_reg.bf.com_ts;

	nss_ptp_tx_com_frac_nano_reg_get(nss_phydev,
				&ptp_tx_com_frac_nano_reg);
	ts_engine->timestamp.fracnanoseconds = ptp_tx_com_frac_nano_reg.bf.frac_nano;

	ts_engine->timestamp_pre.seconds = 0;
	ts_engine->timestamp_pre.nanoseconds = 0;
	ts_engine->timestamp_pre.fracnanoseconds = 0;

	return 0;
}

static int
nss_phy_ptp_enhanced_timestamp_engine_get(struct nss_phy_device *nss_phydev,
		fal_ptp_direction_t direction,
		fal_ptp_enhanced_ts_engine_t *ts_engine)
{
	if (direction == FAL_RX_DIRECTION)
		return _nss_phy_ptp_enhanced_timestamp_engine_rx_get(nss_phydev,
				ts_engine);

	return _nss_phy_ptp_enhanced_timestamp_engine_tx_get(nss_phydev,
			ts_engine);
}

static int
nss_phy_ptp_trigger_set(struct nss_phy_device *nss_phydev,
		u32 trigger_id,
		fal_ptp_trigger_t *triger)
{
	union ptp_trigger0_config_reg_u ptp_trigger0_config_reg = {0};
	union ptp_trigger1_config_reg_u ptp_trigger1_config_reg = {0};
	union ptp_trigger0_timestamp0_reg_u ptp_trigger0_timestamp0_reg = {0};
	union ptp_trigger0_timestamp1_reg_u ptp_trigger0_timestamp1_reg = {0};
	union ptp_trigger0_timestamp2_reg_u ptp_trigger0_timestamp2_reg = {0};
	union ptp_trigger0_timestamp3_reg_u ptp_trigger0_timestamp3_reg = {0};
	union ptp_trigger0_timestamp4_reg_u ptp_trigger0_timestamp4_reg = {0};
	union ptp_trigger1_timestamp0_reg_u ptp_trigger1_timestamp0_reg = {0};
	union ptp_trigger1_timestamp1_reg_u ptp_trigger1_timestamp1_reg = {0};
	union ptp_trigger1_timestamp2_reg_u ptp_trigger1_timestamp2_reg = {0};
	union ptp_trigger1_timestamp3_reg_u ptp_trigger1_timestamp3_reg = {0};
	union ptp_trigger1_timestamp4_reg_u ptp_trigger1_timestamp4_reg = {0};

	if (trigger_id == 0)
	{
		nss_ptp_trigger0_config_reg_get(nss_phydev,
					&ptp_trigger0_config_reg);
		ptp_trigger0_config_reg.bf.status = triger->trigger_conf.trigger_en;
		ptp_trigger0_config_reg.bf.force_en = triger->trigger_conf.output_force_en;
		ptp_trigger0_config_reg.bf.force_value = triger->trigger_conf.output_force_value;
		ptp_trigger0_config_reg.bf.pattern = triger->trigger_conf.patten_select;
		ptp_trigger0_config_reg.bf.if_late = triger->trigger_conf.late_operation;
		ptp_trigger0_config_reg.bf.notify = triger->trigger_conf.notify;
		ptp_trigger0_config_reg.bf.setting = triger->trigger_conf.trigger_effect;
		nss_ptp_trigger0_config_reg_set(nss_phydev,
					&ptp_trigger0_config_reg);

		ptp_trigger0_timestamp0_reg.bf.ts_sec =
			(triger->trigger_conf.tim.seconds >> 32) & 0xffff;
		ptp_trigger0_timestamp1_reg.bf.ts_sec =
			(triger->trigger_conf.tim.seconds >> 16) & 0xffff;
		ptp_trigger0_timestamp2_reg.bf.ts_sec =
			triger->trigger_conf.tim.seconds & 0xffff;
		ptp_trigger0_timestamp3_reg.bf.ts_nsec =
			triger->trigger_conf.tim.nanoseconds >> 16;
		ptp_trigger0_timestamp4_reg.bf.ts_nsec =
			triger->trigger_conf.tim.nanoseconds & 0xffff;
		nss_ptp_trigger0_timestamp0_reg_set(nss_phydev,
					&ptp_trigger0_timestamp0_reg);
		nss_ptp_trigger0_timestamp1_reg_set(nss_phydev,
					&ptp_trigger0_timestamp1_reg);
		nss_ptp_trigger0_timestamp2_reg_set(nss_phydev,
					&ptp_trigger0_timestamp2_reg);
		nss_ptp_trigger0_timestamp3_reg_set(nss_phydev,
					&ptp_trigger0_timestamp3_reg);
		nss_ptp_trigger0_timestamp4_reg_set(nss_phydev,
					&ptp_trigger0_timestamp4_reg);
	} else {
		nss_ptp_trigger1_config_reg_get(nss_phydev,
					&ptp_trigger1_config_reg);
		ptp_trigger1_config_reg.bf.status = triger->trigger_conf.trigger_en;
		ptp_trigger1_config_reg.bf.force_en = triger->trigger_conf.output_force_en;
		ptp_trigger1_config_reg.bf.force_value = triger->trigger_conf.output_force_value;
		ptp_trigger1_config_reg.bf.pattern = triger->trigger_conf.patten_select;
		ptp_trigger1_config_reg.bf.if_late = triger->trigger_conf.late_operation;
		ptp_trigger1_config_reg.bf.notify = triger->trigger_conf.notify;
		ptp_trigger1_config_reg.bf.setting = triger->trigger_conf.trigger_effect;
		nss_ptp_trigger1_config_reg_set(nss_phydev,
					&ptp_trigger1_config_reg);

		ptp_trigger1_timestamp0_reg.bf.ts_sec =
			(triger->trigger_conf.tim.seconds >> 32) & 0xffff;
		ptp_trigger1_timestamp1_reg.bf.ts_sec =
			(triger->trigger_conf.tim.seconds >> 16) & 0xffff;
		ptp_trigger1_timestamp2_reg.bf.ts_sec =
			triger->trigger_conf.tim.seconds & 0xffff;
		ptp_trigger1_timestamp3_reg.bf.ts_nsec =
			triger->trigger_conf.tim.nanoseconds >> 16;
		ptp_trigger1_timestamp4_reg.bf.ts_nsec =
			triger->trigger_conf.tim.nanoseconds & 0xffff;
		nss_ptp_trigger1_timestamp0_reg_set(nss_phydev,
					&ptp_trigger1_timestamp0_reg);
		nss_ptp_trigger1_timestamp1_reg_set(nss_phydev,
					&ptp_trigger1_timestamp1_reg);
		nss_ptp_trigger1_timestamp2_reg_set(nss_phydev,
					&ptp_trigger1_timestamp2_reg);
		nss_ptp_trigger1_timestamp3_reg_set(nss_phydev,
					&ptp_trigger1_timestamp3_reg);
		nss_ptp_trigger1_timestamp4_reg_set(nss_phydev,
					&ptp_trigger1_timestamp4_reg);
	}

	return 0;
}

static int
nss_phy_ptp_trigger_get(struct nss_phy_device *nss_phydev,
		u32 trigger_id,
		fal_ptp_trigger_t *triger)
{
	union ptp_trigger0_config_reg_u ptp_trigger0_config_reg = {0};
	union ptp_trigger0_status_reg_u ptp_trigger0_status_reg = {0};
	union ptp_trigger1_config_reg_u ptp_trigger1_config_reg = {0};
	union ptp_trigger1_status_reg_u ptp_trigger1_status_reg = {0};
	union ptp_trigger0_timestamp0_reg_u ptp_trigger0_timestamp0_reg = {0};
	union ptp_trigger0_timestamp1_reg_u ptp_trigger0_timestamp1_reg = {0};
	union ptp_trigger0_timestamp2_reg_u ptp_trigger0_timestamp2_reg = {0};
	union ptp_trigger0_timestamp3_reg_u ptp_trigger0_timestamp3_reg = {0};
	union ptp_trigger0_timestamp4_reg_u ptp_trigger0_timestamp4_reg = {0};
	union ptp_trigger1_timestamp0_reg_u ptp_trigger1_timestamp0_reg = {0};
	union ptp_trigger1_timestamp1_reg_u ptp_trigger1_timestamp1_reg = {0};
	union ptp_trigger1_timestamp2_reg_u ptp_trigger1_timestamp2_reg = {0};
	union ptp_trigger1_timestamp3_reg_u ptp_trigger1_timestamp3_reg = {0};
	union ptp_trigger1_timestamp4_reg_u ptp_trigger1_timestamp4_reg = {0};

	if (trigger_id == 0)
	{
		nss_ptp_trigger0_config_reg_get(nss_phydev,
					&ptp_trigger0_config_reg);
		triger->trigger_conf.trigger_en = ptp_trigger0_config_reg.bf.status;
		triger->trigger_conf.output_force_en = ptp_trigger0_config_reg.bf.force_en;
		triger->trigger_conf.output_force_value = ptp_trigger0_config_reg.bf.force_value;
		triger->trigger_conf.patten_select = ptp_trigger0_config_reg.bf.pattern;
		triger->trigger_conf.late_operation = ptp_trigger0_config_reg.bf.if_late;
		triger->trigger_conf.notify = ptp_trigger0_config_reg.bf.notify;
		triger->trigger_conf.trigger_effect = ptp_trigger0_config_reg.bf.setting;

		nss_ptp_trigger0_timestamp0_reg_get(nss_phydev,
					&ptp_trigger0_timestamp0_reg);
		nss_ptp_trigger0_timestamp1_reg_get(nss_phydev,
					&ptp_trigger0_timestamp1_reg);
		nss_ptp_trigger0_timestamp2_reg_get(nss_phydev,
					&ptp_trigger0_timestamp2_reg);
		nss_ptp_trigger0_timestamp3_reg_get(nss_phydev,
					&ptp_trigger0_timestamp3_reg);
		nss_ptp_trigger0_timestamp4_reg_get(nss_phydev,
					&ptp_trigger0_timestamp4_reg);
		triger->trigger_conf.tim.seconds =
			((u64)ptp_trigger0_timestamp0_reg.bf.ts_sec << 32) |
			(ptp_trigger0_timestamp1_reg.bf.ts_sec << 16) |
			ptp_trigger0_timestamp2_reg.bf.ts_sec;
		triger->trigger_conf.tim.nanoseconds =
			(ptp_trigger0_timestamp3_reg.bf.ts_nsec << 16) |
			ptp_trigger0_timestamp4_reg.bf.ts_nsec;

		nss_ptp_trigger0_status_reg_get(nss_phydev,
					&ptp_trigger0_status_reg);
		triger->trigger_status.trigger_finished = ptp_trigger0_status_reg.bf.finished;
		triger->trigger_status.trigger_active = ptp_trigger0_status_reg.bf.active;
		triger->trigger_status.trigger_error = ptp_trigger0_status_reg.bf.error;
	} else {
		nss_ptp_trigger1_config_reg_get(nss_phydev,
					&ptp_trigger1_config_reg);
		triger->trigger_conf.trigger_en = ptp_trigger1_config_reg.bf.status;
		triger->trigger_conf.output_force_en = ptp_trigger1_config_reg.bf.force_en;
		triger->trigger_conf.output_force_value = ptp_trigger1_config_reg.bf.force_value;
		triger->trigger_conf.patten_select = ptp_trigger1_config_reg.bf.pattern;
		triger->trigger_conf.late_operation = ptp_trigger1_config_reg.bf.if_late;
		triger->trigger_conf.notify = ptp_trigger1_config_reg.bf.notify;
		triger->trigger_conf.trigger_effect = ptp_trigger1_config_reg.bf.setting;

		nss_ptp_trigger1_timestamp0_reg_get(nss_phydev,
					&ptp_trigger1_timestamp0_reg);
		nss_ptp_trigger1_timestamp1_reg_get(nss_phydev,
					&ptp_trigger1_timestamp1_reg);
		nss_ptp_trigger1_timestamp2_reg_get(nss_phydev,
					&ptp_trigger1_timestamp2_reg);
		nss_ptp_trigger1_timestamp3_reg_get(nss_phydev,
					&ptp_trigger1_timestamp3_reg);
		nss_ptp_trigger1_timestamp4_reg_get(nss_phydev,
					&ptp_trigger1_timestamp4_reg);
		triger->trigger_conf.tim.seconds =
			((u64)ptp_trigger1_timestamp0_reg.bf.ts_sec << 32) |
			(ptp_trigger1_timestamp1_reg.bf.ts_sec << 16) |
			ptp_trigger1_timestamp2_reg.bf.ts_sec;
		triger->trigger_conf.tim.nanoseconds =
			(ptp_trigger1_timestamp3_reg.bf.ts_nsec << 16) |
			ptp_trigger1_timestamp4_reg.bf.ts_nsec;

		nss_ptp_trigger1_status_reg_get(nss_phydev,
					&ptp_trigger1_status_reg);
		triger->trigger_status.trigger_finished = ptp_trigger1_status_reg.bf.finished;
		triger->trigger_status.trigger_active = ptp_trigger1_status_reg.bf.active;
		triger->trigger_status.trigger_error = ptp_trigger1_status_reg.bf.error;
	}

	return 0;
}

static int
nss_phy_ptp_capture_set(struct nss_phy_device *nss_phydev,
		u32 capture_id,
		fal_ptp_capture_t *capture)
{
	union ptp_event0_config_reg_u ptp_event0_config_reg = {0};
	union ptp_event1_config_reg_u ptp_event1_config_reg = {0};

	if (capture_id == 0)
	{
		nss_ptp_event0_config_reg_get(nss_phydev,
					&ptp_event0_config_reg);
		ptp_event0_config_reg.bf.clear_stat = capture->capture_conf.status_clear;
		ptp_event0_config_reg.bf.notify = capture->capture_conf.notify_event;
		ptp_event0_config_reg.bf.single_cap = capture->capture_conf.single_multi_select;
		ptp_event0_config_reg.bf.fall_en = capture->capture_conf.fall_edge_en;
		ptp_event0_config_reg.bf.rise_en = capture->capture_conf.rise_edge_en;
		nss_ptp_event0_config_reg_set(nss_phydev,
					&ptp_event0_config_reg);
	}
	else
	{
		nss_ptp_event1_config_reg_get(nss_phydev,
					&ptp_event1_config_reg);
		ptp_event1_config_reg.bf.clear_stat = capture->capture_conf.status_clear;
		ptp_event1_config_reg.bf.notify = capture->capture_conf.notify_event;
		ptp_event1_config_reg.bf.single_cap = capture->capture_conf.single_multi_select;
		ptp_event1_config_reg.bf.fall_en = capture->capture_conf.fall_edge_en;
		ptp_event1_config_reg.bf.rise_en = capture->capture_conf.rise_edge_en;
		nss_ptp_event1_config_reg_set(nss_phydev,
					&ptp_event1_config_reg);
	}

	return 0;
}

static int
nss_phy_ptp_capture_get(struct nss_phy_device *nss_phydev,
		u32 capture_id,
		fal_ptp_capture_t *capture)
{
	union ptp_event0_config_reg_u ptp_event0_config_reg = {0};
	union ptp_event0_status_reg_u ptp_event0_status_reg = {0};
	union ptp_event1_config_reg_u ptp_event1_config_reg = {0};
	union ptp_event1_status_reg_u ptp_event1_status_reg = {0};
	union ptp_event0_timestamp0_reg_u ptp_event0_timestamp0_reg = {0};
	union ptp_event0_timestamp1_reg_u ptp_event0_timestamp1_reg = {0};
	union ptp_event0_timestamp2_reg_u ptp_event0_timestamp2_reg = {0};
	union ptp_event0_timestamp3_reg_u ptp_event0_timestamp3_reg = {0};
	union ptp_event0_timestamp4_reg_u ptp_event0_timestamp4_reg = {0};
	union ptp_event1_timestamp0_reg_u ptp_event1_timestamp0_reg = {0};
	union ptp_event1_timestamp1_reg_u ptp_event1_timestamp1_reg = {0};
	union ptp_event1_timestamp2_reg_u ptp_event1_timestamp2_reg = {0};
	union ptp_event1_timestamp3_reg_u ptp_event1_timestamp3_reg = {0};
	union ptp_event1_timestamp4_reg_u ptp_event1_timestamp4_reg = {0};

	if (capture_id == 0)
	{
		nss_ptp_event0_config_reg_get(nss_phydev,
					&ptp_event0_config_reg);
		capture->capture_conf.status_clear = ptp_event0_config_reg.bf.clear_stat;
		capture->capture_conf.notify_event = ptp_event0_config_reg.bf.notify;
		capture->capture_conf.single_multi_select = ptp_event0_config_reg.bf.single_cap;
		capture->capture_conf.fall_edge_en = ptp_event0_config_reg.bf.fall_en;
		capture->capture_conf.rise_edge_en = ptp_event0_config_reg.bf.rise_en;

		nss_ptp_event0_status_reg_get(nss_phydev,
					&ptp_event0_status_reg);
		capture->capture_status.event_detected = ptp_event0_status_reg.bf.detected;
		capture->capture_status.fall_rise_edge_detected =
			ptp_event0_status_reg.bf.dir_detected;
		capture->capture_status.single_multi_detected = ptp_event0_status_reg.bf.mul_event;
		capture->capture_status.event_missed_cnt = ptp_event0_status_reg.bf.missed_count;

		nss_ptp_event0_timestamp0_reg_get(nss_phydev,
					&ptp_event0_timestamp0_reg);
		nss_ptp_event0_timestamp1_reg_get(nss_phydev,
					&ptp_event0_timestamp1_reg);
		nss_ptp_event0_timestamp2_reg_get(nss_phydev,
					&ptp_event0_timestamp2_reg);
		capture->capture_status.tim.seconds =
			((u64)ptp_event0_timestamp0_reg.bf.ts_nsec << 32) |
			(ptp_event0_timestamp1_reg.bf.ts_nsec << 16) |
			ptp_event0_timestamp2_reg.bf.ts_nsec;
		nss_ptp_event0_timestamp3_reg_get(nss_phydev,
					&ptp_event0_timestamp3_reg);
		nss_ptp_event0_timestamp4_reg_get(nss_phydev,
					&ptp_event0_timestamp4_reg);
		capture->capture_status.tim.nanoseconds =
			(ptp_event0_timestamp3_reg.bf.ts_nsec << 16) |
			ptp_event0_timestamp4_reg.bf.ts_nsec;
	} else {
		nss_ptp_event1_config_reg_get(nss_phydev,
					&ptp_event1_config_reg);
		capture->capture_conf.status_clear = ptp_event1_config_reg.bf.clear_stat;
		capture->capture_conf.notify_event = ptp_event1_config_reg.bf.notify;
		capture->capture_conf.single_multi_select = ptp_event1_config_reg.bf.single_cap;
		capture->capture_conf.fall_edge_en = ptp_event1_config_reg.bf.fall_en;
		capture->capture_conf.rise_edge_en = ptp_event1_config_reg.bf.rise_en;

		nss_ptp_event1_status_reg_get(nss_phydev,
					&ptp_event1_status_reg);
		capture->capture_status.event_detected = ptp_event1_status_reg.bf.detected;
		capture->capture_status.fall_rise_edge_detected =
			ptp_event1_status_reg.bf.dir_detected;
		capture->capture_status.single_multi_detected = ptp_event1_status_reg.bf.mul_event;
		capture->capture_status.event_missed_cnt = ptp_event1_status_reg.bf.missed_count;

		nss_ptp_event1_timestamp0_reg_get(nss_phydev,
					&ptp_event1_timestamp0_reg);
		nss_ptp_event1_timestamp1_reg_get(nss_phydev,
					&ptp_event1_timestamp1_reg);
		nss_ptp_event1_timestamp2_reg_get(nss_phydev,
					&ptp_event1_timestamp2_reg);
		capture->capture_status.tim.seconds =
			((u64)ptp_event1_timestamp0_reg.bf.ts_nsec << 32) |
			(ptp_event1_timestamp1_reg.bf.ts_nsec << 16) |
			ptp_event1_timestamp2_reg.bf.ts_nsec;
		nss_ptp_event1_timestamp3_reg_get(nss_phydev,
					&ptp_event1_timestamp3_reg);
		nss_ptp_event1_timestamp4_reg_get(nss_phydev,
					&ptp_event1_timestamp4_reg);
		capture->capture_status.tim.nanoseconds =
			(ptp_event1_timestamp3_reg.bf.ts_nsec << 16) |
			ptp_event1_timestamp4_reg.bf.ts_nsec;
	}

	return 0;
}

static int
nss_phy_ptp_interrupt_set(struct nss_phy_device *nss_phydev,
		fal_ptp_interrupt_t *interrupt)
{
	union ptp_imr_reg_u ptp_imr_reg = {0};
	union ptp_ext_imr_reg_u ptp_ext_imr_reg = {0};

	nss_ptp_imr_reg_get(nss_phydev, &ptp_imr_reg);
	ptp_imr_reg.bf.mask_bmp &= (~(0x7 << 2)) & 0xffff;
	ptp_imr_reg.bf.mask_bmp |= (interrupt->intr_mask & 0x7) << 2;
	nss_ptp_imr_reg_set(nss_phydev, &ptp_imr_reg);

	ptp_ext_imr_reg.bf.mask_bmp = interrupt->intr_mask >> 3;
	nss_ptp_ext_imr_reg_set(nss_phydev, &ptp_ext_imr_reg);

	return 0;
}

static int
nss_phy_ptp_interrupt_get(struct nss_phy_device *nss_phydev,
		fal_ptp_interrupt_t *interrupt)
{
	union ptp_imr_reg_u ptp_imr_reg = {0};
	union ptp_isr_reg_u ptp_isr_reg = {0};
	union ptp_ext_imr_reg_u ptp_ext_imr_reg = {0};
	union ptp_ext_isr_reg_u ptp_ext_isr_reg = {0};

	nss_ptp_imr_reg_get(nss_phydev, &ptp_imr_reg);
	nss_ptp_ext_imr_reg_get(nss_phydev, &ptp_ext_imr_reg);
	interrupt->intr_mask = ((ptp_imr_reg.bf.mask_bmp >> 2) & 0x7) |
			((ptp_ext_imr_reg.bf.mask_bmp & 0x7ff) << 3);

	nss_ptp_isr_reg_get(nss_phydev, &ptp_isr_reg);
	nss_ptp_ext_isr_reg_get(nss_phydev, &ptp_ext_isr_reg);
	interrupt->intr_status = ((ptp_isr_reg.bf.status_bmp >> 2) & 0x7) |
			((ptp_ext_isr_reg.bf.status_bmp & 0x7ff) << 3);

	return 0;
}

int nss_phy_ptp_ops_init(struct nss_phy_ptp_ops *ptp_ops)
{
	ptp_ops->ptp_security_set = nss_phy_ptp_security_set;
	ptp_ops->ptp_link_delay_set = nss_phy_ptp_link_delay_set;
	ptp_ops->ptp_rx_crc_recalc_status_get = nss_phy_ptp_rx_crc_recalc_status_get;
	ptp_ops->ptp_tod_uart_set = nss_phy_ptp_tod_uart_set;
	ptp_ops->ptp_pps_signal_control_set = nss_phy_ptp_pps_signal_control_set;
	ptp_ops->ptp_timestamp_get = nss_phy_ptp_timestamp_get;
	ptp_ops->ptp_asym_correction_get = nss_phy_ptp_asym_correction_get;
	ptp_ops->ptp_capture_set = nss_phy_ptp_capture_set;
	ptp_ops->ptp_rtc_adjfreq_set = nss_phy_ptp_rtc_adjfreq_set;
	ptp_ops->ptp_asym_correction_set = nss_phy_ptp_asym_correction_set;
	ptp_ops->ptp_pkt_timestamp_set = nss_phy_ptp_pkt_timestamp_set;
	ptp_ops->ptp_rtc_time_get = nss_phy_ptp_rtc_time_get;
	ptp_ops->ptp_rtc_time_set = nss_phy_ptp_rtc_time_set;
	ptp_ops->ptp_pkt_timestamp_get = nss_phy_ptp_pkt_timestamp_get;
	ptp_ops->ptp_interrupt_set = nss_phy_ptp_interrupt_set;
	ptp_ops->ptp_trigger_set = nss_phy_ptp_trigger_set;
	ptp_ops->ptp_pps_signal_control_get = nss_phy_ptp_pps_signal_control_get;
	ptp_ops->ptp_capture_get = nss_phy_ptp_capture_get;
	ptp_ops->ptp_rx_crc_recalc_enable = nss_phy_ptp_rx_crc_recalc_enable;
	ptp_ops->ptp_security_get = nss_phy_ptp_security_get;
	ptp_ops->ptp_tod_uart_get = nss_phy_ptp_tod_uart_get;
	ptp_ops->ptp_rtc_time_clear = nss_phy_ptp_rtc_time_clear;
	ptp_ops->ptp_reference_clock_set = nss_phy_ptp_reference_clock_set;
	ptp_ops->ptp_output_waveform_set = nss_phy_ptp_output_waveform_set;
	ptp_ops->ptp_rx_timestamp_mode_set = nss_phy_ptp_rx_timestamp_mode_set;
	ptp_ops->ptp_grandmaster_mode_set = nss_phy_ptp_grandmaster_mode_set;
	ptp_ops->ptp_config_set = nss_phy_ptp_config_set;
	ptp_ops->ptp_trigger_get = nss_phy_ptp_trigger_get;
	ptp_ops->ptp_rtc_adjfreq_get = nss_phy_ptp_rtc_adjfreq_get;
	ptp_ops->ptp_grandmaster_mode_get = nss_phy_ptp_grandmaster_mode_get;
	ptp_ops->ptp_rx_timestamp_mode_get = nss_phy_ptp_rx_timestamp_mode_get;
	ptp_ops->ptp_rtc_adjtime_set = nss_phy_ptp_rtc_adjtime_set;
	ptp_ops->ptp_link_delay_get = nss_phy_ptp_link_delay_get;
	ptp_ops->ptp_config_get = nss_phy_ptp_config_get;
	ptp_ops->ptp_output_waveform_get = nss_phy_ptp_output_waveform_get;
	ptp_ops->ptp_interrupt_get = nss_phy_ptp_interrupt_get;
	ptp_ops->ptp_rtc_time_snapshot_enable = nss_phy_ptp_rtc_time_snapshot_enable;
	ptp_ops->ptp_reference_clock_get = nss_phy_ptp_reference_clock_get;
	ptp_ops->ptp_enhanced_timestamp_engine_set = nss_phy_ptp_enhanced_timestamp_engine_set;
	ptp_ops->ptp_rtc_time_snapshot_status_get = nss_phy_ptp_rtc_time_snapshot_status_get;
	ptp_ops->ptp_enhanced_timestamp_engine_get = nss_phy_ptp_enhanced_timestamp_engine_get;
	ptp_ops->ptp_increment_sync_from_clock_enable = nss_phy_ptp_increment_sync_from_clock_enable;
	ptp_ops->ptp_increment_sync_from_clock_status_get = nss_phy_ptp_increment_sync_from_clock_status_get;
#if defined(MHT)
	ptp_ops->ptp_rtc_sync_set = nss_phy_ptp_rtc_sync_set;
	ptp_ops->ptp_rtc_sync_get = nss_phy_ptp_rtc_sync_get;
#endif
	return 0;
}
