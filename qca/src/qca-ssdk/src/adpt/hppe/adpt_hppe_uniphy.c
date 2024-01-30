/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
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

/**
 * @defgroup
 * @{
 */
#include "sw.h"
#include "hppe_uniphy_reg.h"
#include "hppe_uniphy.h"
#include "hppe_init.h"
#include "ssdk_init.h"
#include "ssdk_clk.h"
#include "ssdk_dts.h"
#include "adpt.h"
#include "hppe_reg_access.h"
#include "hsl_phy.h"
#include "hsl_port_prop.h"
#include "adpt_hppe.h"
#include "adpt_hppe_uniphy.h"
#if defined(CPPE)
#include "adpt_cppe_uniphy.h"
#include "adpt_cppe_portctrl.h"
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0))
#include <soc/qcom/socinfo.h>
#endif

extern void adpt_hppe_gcc_port_speed_clock_set(a_uint32_t dev_id,
				a_uint32_t port_id, fal_port_speed_t phy_speed);

static a_uint32_t
adpt_hppe_port_get_by_uniphy(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t channel)
{
	a_uint32_t ssdk_port = 0;

	if (uniphy_index == SSDK_UNIPHY_INSTANCE0) {
		if (channel == SSDK_UNIPHY_CHANNEL0) {
			ssdk_port = SSDK_PHYSICAL_PORT1;
		} else if (channel == SSDK_UNIPHY_CHANNEL1) {
			ssdk_port = SSDK_PHYSICAL_PORT2;
		} else if (channel == SSDK_UNIPHY_CHANNEL4) {
			ssdk_port = SSDK_PHYSICAL_PORT5;
		} else if (channel == SSDK_UNIPHY_CHANNEL3) {
			ssdk_port = SSDK_PHYSICAL_PORT4;
		}
	} else if (uniphy_index == SSDK_UNIPHY_INSTANCE1) {
		ssdk_port = HPPE_UNIPHY1_PORT;
	} else if (uniphy_index == SSDK_UNIPHY_INSTANCE2) {
		ssdk_port = SSDK_PHYSICAL_PORT6;
	}

	return ssdk_port;
}

sw_error_t
adpt_hppe_uniphy_usxgmii_status_get(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union sr_mii_ctrl_u *sr_mii_ctrl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sr_mii_ctrl);

	if ((port_id == SSDK_PHYSICAL_PORT1) || (port_id == HPPE_UNIPHY1_PORT) ||
		(port_id == SSDK_PHYSICAL_PORT6)) {
		hppe_sr_mii_ctrl_get(dev_id, uniphy_index, sr_mii_ctrl);
	}
#if defined(APPE)
	if (adpt_chip_type_get(dev_id) == CHIP_APPE) {
		a_uint32_t mode;
		mode = ssdk_dt_global_get_mac_mode(dev_id, uniphy_index);
		if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
			if (port_id == SSDK_PHYSICAL_PORT2) {
				hppe_sr_mii_ctrl_channel1_get(dev_id, uniphy_index, sr_mii_ctrl);
			} else if (port_id == SSDK_PHYSICAL_PORT3) {
				hppe_sr_mii_ctrl_channel2_get(dev_id, uniphy_index, sr_mii_ctrl);
			} else if (port_id == SSDK_PHYSICAL_PORT4) {
				hppe_sr_mii_ctrl_channel3_get(dev_id, uniphy_index, sr_mii_ctrl);
			}
		}
	}
#endif

	return SW_OK;
}

sw_error_t
adpt_hppe_uniphy_usxgmii_status_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union sr_mii_ctrl_u *sr_mii_ctrl)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(sr_mii_ctrl);

	if ((port_id == SSDK_PHYSICAL_PORT1) || (port_id == HPPE_UNIPHY1_PORT) ||
		(port_id == SSDK_PHYSICAL_PORT6)) {
		hppe_sr_mii_ctrl_set(dev_id, uniphy_index, sr_mii_ctrl);
	}
#if defined(APPE)
	if (adpt_chip_type_get(dev_id) == CHIP_APPE) {
		a_uint32_t mode;
		mode = ssdk_dt_global_get_mac_mode(dev_id, uniphy_index);
		if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
			if (port_id == SSDK_PHYSICAL_PORT2) {
				hppe_sr_mii_ctrl_channel1_set(dev_id, uniphy_index, sr_mii_ctrl);
			} else if (port_id == SSDK_PHYSICAL_PORT3) {
				hppe_sr_mii_ctrl_channel2_set(dev_id, uniphy_index, sr_mii_ctrl);
			} else if (port_id == SSDK_PHYSICAL_PORT4) {
				hppe_sr_mii_ctrl_channel3_set(dev_id, uniphy_index, sr_mii_ctrl);
			}
			SSDK_DEBUG("uqxgmii uniphy %d port %d status set!\n", uniphy_index, port_id);
		}
	}
#endif

	return SW_OK;
}

sw_error_t
adpt_hppe_uniphy_usxgmii_autoneg_status_get(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union vr_mii_an_intr_sts_u *vr_mii_an_intr_sts)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vr_mii_an_intr_sts);

	if ((port_id == SSDK_PHYSICAL_PORT1) || (port_id == HPPE_UNIPHY1_PORT) ||
		(port_id == SSDK_PHYSICAL_PORT6)) {
		hppe_vr_mii_an_intr_sts_get(dev_id, uniphy_index, vr_mii_an_intr_sts);
	}
#if defined(APPE)
	if (adpt_chip_type_get(dev_id) == CHIP_APPE) {
		a_uint32_t mode;
		mode = ssdk_dt_global_get_mac_mode(dev_id, uniphy_index);
		if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
			if (port_id == SSDK_PHYSICAL_PORT2) {
				hppe_vr_mii_an_intr_sts_channel1_get(dev_id,
					uniphy_index, vr_mii_an_intr_sts);
			} else if (port_id == SSDK_PHYSICAL_PORT3) {
				hppe_vr_mii_an_intr_sts_channel2_get(dev_id,
					uniphy_index, vr_mii_an_intr_sts);
			} else if (port_id == SSDK_PHYSICAL_PORT4) {
				hppe_vr_mii_an_intr_sts_channel3_get(dev_id,
					uniphy_index, vr_mii_an_intr_sts);
			}
			SSDK_DEBUG("uqxgmii uniphy port %d autoneg check\n", port_id);
		}
	}
#endif

	return SW_OK;
}

sw_error_t
adpt_hppe_uniphy_usxgmii_autoneg_status_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
		a_uint32_t port_id, union vr_mii_an_intr_sts_u *vr_mii_an_intr_sts)
{
	ADPT_DEV_ID_CHECK(dev_id);
	ADPT_NULL_POINT_CHECK(vr_mii_an_intr_sts);

	if ((port_id == SSDK_PHYSICAL_PORT1) || (port_id == HPPE_UNIPHY1_PORT) ||
		(port_id == SSDK_PHYSICAL_PORT6)) {
		hppe_vr_mii_an_intr_sts_set(dev_id, uniphy_index, vr_mii_an_intr_sts);
	}
#if defined(APPE)
	if (adpt_chip_type_get(dev_id) == CHIP_APPE) {
		a_uint32_t mode;
		mode = ssdk_dt_global_get_mac_mode(dev_id, uniphy_index);
		if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
			if (port_id == SSDK_PHYSICAL_PORT2) {
				hppe_vr_mii_an_intr_sts_channel1_set(dev_id,
					uniphy_index, vr_mii_an_intr_sts);
			} else if (port_id == SSDK_PHYSICAL_PORT3) {
				hppe_vr_mii_an_intr_sts_channel2_set(dev_id,
					uniphy_index, vr_mii_an_intr_sts);
			} else if (port_id == SSDK_PHYSICAL_PORT4) {
				hppe_vr_mii_an_intr_sts_channel3_set(dev_id,
					uniphy_index, vr_mii_an_intr_sts);
			}
			SSDK_DEBUG("uqxgmii uniphy port %d autoneg clear\n", port_id);
		}
	}
#endif

	return SW_OK;
}

static sw_error_t
__adpt_hppe_uniphy_10g_r_linkup(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	a_uint32_t reg_value = 0;
	a_uint32_t retries = 100, linkup = 0;

	union sr_xs_pcs_kr_sts1_u sr_xs_pcs_kr_sts1;

	memset(&sr_xs_pcs_kr_sts1, 0, sizeof(sr_xs_pcs_kr_sts1));
	ADPT_DEV_ID_CHECK(dev_id);

	/* wait 10G_R link up  to uniphy */
	while (linkup != UNIPHY_10GR_LINKUP) {
		mdelay(1);
		if (retries-- == 0)
			return SW_TIMEOUT;
		reg_value = 0;
		hppe_sr_xs_pcs_kr_sts1_get(dev_id, uniphy_index, &sr_xs_pcs_kr_sts1);
		reg_value = sr_xs_pcs_kr_sts1.bf.plu;
		linkup = (reg_value & UNIPHY_10GR_LINKUP);
	}

	return SW_OK;
}

sw_error_t
__adpt_hppe_uniphy_calibrate(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	a_uint32_t reg_value = 0;
	a_uint32_t retries = 100, calibration_done = 0;

	union uniphy_offset_calib_4_u uniphy_offset_calib_4;

	memset(&uniphy_offset_calib_4, 0, sizeof(uniphy_offset_calib_4));
	ADPT_DEV_ID_CHECK(dev_id);

	/* wait calibration done to uniphy */
	while (calibration_done != UNIPHY_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0)
		{
			SSDK_ERROR("uniphy callibration time out!\n");
			return SW_TIMEOUT;
		}
		reg_value = 0;
		hppe_uniphy_offset_calib_4_get(dev_id, uniphy_index, &uniphy_offset_calib_4);
		reg_value = uniphy_offset_calib_4.bf.mmd1_reg_calibration_done_reg;

		calibration_done = (reg_value & UNIPHY_CALIBRATION_DONE);
	}

	return SW_OK;
}

void
__adpt_hppe_gcc_uniphy_xpcs_reset(a_uint32_t dev_id, a_uint32_t uniphy_index, a_bool_t enable)
{
	enum unphy_rst_type rst_type;
	enum ssdk_rst_action rst_action;

	if (uniphy_index == SSDK_UNIPHY_INSTANCE0)
		rst_type = UNIPHY0_XPCS_RESET_E;
	else if (uniphy_index == SSDK_UNIPHY_INSTANCE1)
		rst_type = UNIPHY1_XPCS_RESET_E;
	else
		rst_type = UNIPHY2_XPCS_RESET_E;

	if (enable == A_TRUE)
		rst_action = SSDK_RESET_ASSERT;
	else
		rst_action = SSDK_RESET_DEASSERT;
	
	ssdk_uniphy_reset(dev_id, rst_type, rst_action);

	return;
}

void
__adpt_hppe_gcc_uniphy_software_reset(a_uint32_t dev_id, a_uint32_t uniphy_index)
{

	enum unphy_rst_type rst_type;

	if (uniphy_index == SSDK_UNIPHY_INSTANCE0)
		rst_type = UNIPHY0_SOFT_RESET_E;
	else if (uniphy_index == SSDK_UNIPHY_INSTANCE1)
		rst_type = UNIPHY1_SOFT_RESET_E;
	else
		rst_type = UNIPHY2_SOFT_RESET_E;

	ssdk_uniphy_reset(dev_id, rst_type, SSDK_RESET_ASSERT);

	msleep(100);

	ssdk_uniphy_reset(dev_id, rst_type, SSDK_RESET_DEASSERT);

	return;
}

#if defined(APPE)
void
__adpt_appe_gcc_uniphy_software_reset(a_uint32_t dev_id,
		a_uint32_t uniphy_index)
{
	enum unphy_rst_type rst_type;
	enum unphy_rst_type sys_type;

	if (uniphy_index == SSDK_UNIPHY_INSTANCE0) {
		rst_type = UNIPHY0_SOFT_RESET_E;
		sys_type = UNIPHY0_SYS_RESET_E;
	} else if (uniphy_index == SSDK_UNIPHY_INSTANCE1) {
		rst_type = UNIPHY1_SOFT_RESET_E;
		sys_type = UNIPHY1_SYS_RESET_E;
	} else {
		rst_type = UNIPHY2_SOFT_RESET_E;
		sys_type = UNIPHY2_SYS_RESET_E;
	}
	ssdk_uniphy_reset(dev_id, sys_type, SSDK_RESET_ASSERT);
	ssdk_uniphy_reset(dev_id, rst_type, SSDK_RESET_ASSERT);
	msleep(100);
	ssdk_uniphy_reset(dev_id, sys_type, SSDK_RESET_DEASSERT);
	ssdk_uniphy_reset(dev_id, rst_type, SSDK_RESET_DEASSERT);

	return;
}
#endif

#if defined(MPPE)
void
__adpt_mppe_gcc_uniphy_software_reset(a_uint32_t dev_id,
		a_uint32_t uniphy_index)
{
	enum unphy_rst_type port_rx_rst_type, port_tx_rst_type;
	enum unphy_rst_type sys_type;

	if (uniphy_index == SSDK_UNIPHY_INSTANCE0) {
		port_rx_rst_type = UNIPHY0_PORT1_RX_DISABLE_E;
		port_tx_rst_type = UNIPHY0_PORT1_TX_DISABLE_E;
		sys_type = UNIPHY0_SYS_RESET_E;
	} else if (uniphy_index == SSDK_UNIPHY_INSTANCE1) {
		port_rx_rst_type = UNIPHY1_PORT5_RX_DISABLE_E;
		port_tx_rst_type = UNIPHY1_PORT5_TX_DISABLE_E;
		sys_type = UNIPHY1_SYS_RESET_E;
	} else {
		return;
	}
	ssdk_uniphy_reset(dev_id, sys_type, SSDK_RESET_ASSERT);
	ssdk_uniphy_reset(dev_id, port_rx_rst_type, SSDK_RESET_ASSERT);
	ssdk_uniphy_reset(dev_id, port_tx_rst_type, SSDK_RESET_ASSERT);
	msleep(100);
	ssdk_uniphy_reset(dev_id, sys_type, SSDK_RESET_DEASSERT);
	ssdk_uniphy_reset(dev_id, port_rx_rst_type, SSDK_RESET_DEASSERT);
	ssdk_uniphy_reset(dev_id, port_tx_rst_type, SSDK_RESET_DEASSERT);

	return;
}
#endif

void
__adpt_ppe_gcc_uniphy_software_reset(a_uint32_t dev_id,
		a_uint32_t uniphy_index)
{
	if (adpt_chip_type_get(dev_id) == CHIP_APPE) {
		if (adpt_chip_revision_get(dev_id) == MPPE_REVISION) {
#if defined(MPPE)
			__adpt_mppe_gcc_uniphy_software_reset(dev_id, uniphy_index);
#endif
		} else {
#if defined(APPE)
			__adpt_appe_gcc_uniphy_software_reset(dev_id, uniphy_index);
#endif
		}
	} else if (adpt_chip_type_get(dev_id) == CHIP_HPPE) {
		if (adpt_chip_revision_get(dev_id) == CPPE_REVISION) {
#if defined(CPPE)
			__adpt_cppe_gcc_uniphy_software_reset(dev_id, uniphy_index);
#endif
		} else {
			__adpt_hppe_gcc_uniphy_software_reset(dev_id, uniphy_index);
		}
	}

	return;
}

static sw_error_t
__adpt_hppe_uniphy_uqxgmii_eee_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t rv = SW_OK;

	union vr_xs_pcs_eee_ctrl0_u vr_xs_pcs_eee_ctrl0;
	union vr_xs_pcs_eee_txtimer_u vr_xs_pcs_eee_txtimer;
	union vr_xs_pcs_eee_rxtimer_u vr_xs_pcs_eee_rxtimer;
	union vr_xs_pcs_eee_ctrl1_u vr_xs_pcs_eee_ctrl1;

	memset(&vr_xs_pcs_eee_ctrl0, 0, sizeof(vr_xs_pcs_eee_ctrl0));
	memset(&vr_xs_pcs_eee_txtimer, 0, sizeof(vr_xs_pcs_eee_txtimer));
	memset(&vr_xs_pcs_eee_rxtimer, 0, sizeof(vr_xs_pcs_eee_rxtimer));
	memset(&vr_xs_pcs_eee_ctrl1, 0, sizeof(vr_xs_pcs_eee_ctrl1));
	ADPT_DEV_ID_CHECK(dev_id);

	/* configure eee related timer value */
	rv = hppe_vr_xs_pcs_eee_ctrl0_get(dev_id, uniphy_index, &vr_xs_pcs_eee_ctrl0);
	SW_RTN_ON_ERROR (rv);
	vr_xs_pcs_eee_ctrl0.bf.sign_bit = 1;
	vr_xs_pcs_eee_ctrl0.bf.mult_fact_100ns = 1;
	rv = hppe_vr_xs_pcs_eee_ctrl0_set(dev_id, uniphy_index, &vr_xs_pcs_eee_ctrl0);
	SW_RTN_ON_ERROR (rv);
	rv = hppe_vr_xs_pcs_eee_txtimer_get(dev_id, uniphy_index, &vr_xs_pcs_eee_txtimer);
	SW_RTN_ON_ERROR (rv);
	vr_xs_pcs_eee_txtimer.bf.tsl_res = UNIPHY_XPCS_TSL_TIMER;
	vr_xs_pcs_eee_txtimer.bf.t1u_res = UNIPHY_XPCS_TLU_TIMER;
	vr_xs_pcs_eee_txtimer.bf.twl_res = UNIPHY_XPCS_TWL_TIMER;
	rv = hppe_vr_xs_pcs_eee_txtimer_set(dev_id, uniphy_index, &vr_xs_pcs_eee_txtimer);
	SW_RTN_ON_ERROR (rv);
	rv = hppe_vr_xs_pcs_eee_rxtimer_get(dev_id, uniphy_index, &vr_xs_pcs_eee_rxtimer);
	SW_RTN_ON_ERROR (rv);
	vr_xs_pcs_eee_rxtimer.bf.res_100u = UNIPHY_XPCS_100US_TIMER;
	vr_xs_pcs_eee_rxtimer.bf.twr_res = UNIPHY_XPCS_TWR_TIMER;
	rv = hppe_vr_xs_pcs_eee_rxtimer_set(dev_id, uniphy_index, &vr_xs_pcs_eee_rxtimer);
	SW_RTN_ON_ERROR (rv);

	/* Transparent LPI mode and LPI pattern eanble */
	rv = hppe_vr_xs_pcs_eee_ctrl1_get(dev_id, uniphy_index, &vr_xs_pcs_eee_ctrl1);
	SW_RTN_ON_ERROR (rv);
	vr_xs_pcs_eee_ctrl1.bf.trn_lpi = 1;
	vr_xs_pcs_eee_ctrl1.bf.trn_rxlpi = 1;
	rv = hppe_vr_xs_pcs_eee_ctrl1_set(dev_id, uniphy_index, &vr_xs_pcs_eee_ctrl1);
	SW_RTN_ON_ERROR (rv);
	rv = hppe_vr_xs_pcs_eee_ctrl0_get(dev_id, uniphy_index, &vr_xs_pcs_eee_ctrl0);
	SW_RTN_ON_ERROR (rv);
	vr_xs_pcs_eee_ctrl0.bf.lrx_en = 1;
	vr_xs_pcs_eee_ctrl0.bf.ltx_en = 1;
	rv = hppe_vr_xs_pcs_eee_ctrl0_set(dev_id, uniphy_index, &vr_xs_pcs_eee_ctrl0);
	SW_RTN_ON_ERROR (rv);

	return rv;
}

static sw_error_t
__adpt_hppe_uniphy_uxgmii_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index,
	a_uint32_t mode)
{
	sw_error_t rv = SW_OK;
	a_uint32_t i = 0;

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;
	union vr_xs_pcs_dig_ctrl1_u vr_xs_pcs_dig_ctrl1;
	union vr_mii_an_ctrl_u vr_mii_an_ctrl;
	union sr_mii_ctrl_u sr_mii_ctrl;
	union vr_xaui_mode_ctrl_u vr_xaui_mode_ctrl;
	union vr_xs_pcs_kr_ctrl_u vr_xs_pcs_kr_ctrl;
	union vr_xs_pcs_dig_sts_u vr_xs_pcs_dig_sts;
	union qp_usxg_opiton1_u qp_usxg_opiton1;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	memset(&vr_xs_pcs_dig_ctrl1, 0, sizeof(vr_xs_pcs_dig_ctrl1));
	memset(&vr_mii_an_ctrl, 0, sizeof(vr_mii_an_ctrl));
	memset(&sr_mii_ctrl, 0, sizeof(sr_mii_ctrl));
	memset(&vr_xaui_mode_ctrl, 0, sizeof(vr_xaui_mode_ctrl));
	memset(&vr_xs_pcs_kr_ctrl, 0, sizeof(vr_xs_pcs_kr_ctrl));
	memset(&vr_xs_pcs_dig_sts, 0, sizeof(vr_xs_pcs_dig_sts));
	memset(&qp_usxg_opiton1, 0, sizeof(qp_usxg_opiton1));
	ADPT_DEV_ID_CHECK(dev_id);

	hppe_uniphy_reg_set(dev_id, UNIPHY_MISC2_REG_OFFSET,
		uniphy_index, UNIPHY_MISC2_REG_VALUE);
	/*reset uniphy*/
	hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
		uniphy_index, UNIPHY_PLL_RESET_REG_VALUE);
	msleep(10);
	hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
		uniphy_index, UNIPHY_PLL_RESET_REG_DEFAULT_VALUE);
	msleep(10);

	/* disable instance clock */
	for (i = SSDK_PHYSICAL_PORT1; i <= SSDK_PHYSICAL_PORT4; i++) {
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
				i, A_FALSE);
	}

	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* configure uniphy to usxgmii mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_QSGMII_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
		UNIPHY_SGMIIPLUS_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_ENABLE;
	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);

	hppe_qp_usxg_opiton1_get(dev_id, uniphy_index, &qp_usxg_opiton1);
	qp_usxg_opiton1.bf.gmii_src_sel = 0x1;
	hppe_qp_usxg_opiton1_set(dev_id, uniphy_index, &qp_usxg_opiton1);

	/* configure uniphy usxgmii gcc software reset */
	__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	msleep(10);

	/* wait calibration done to uniphy */
	__adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	/* enable instance clock */
	for (i = SSDK_PHYSICAL_PORT1; i <= SSDK_PHYSICAL_PORT4; i++) {
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
				i, A_TRUE);
	}

	/* release xpcs reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_FALSE);

	/* wait 10g base_r link up */
	__adpt_hppe_uniphy_10g_r_linkup(dev_id, uniphy_index);

	/* enable uniphy usxgmii */
	hppe_vr_xs_pcs_dig_ctrl1_get(dev_id, uniphy_index, &vr_xs_pcs_dig_ctrl1);
	vr_xs_pcs_dig_ctrl1.bf.usxg_en = 1;
	hppe_vr_xs_pcs_dig_ctrl1_set(dev_id, uniphy_index, &vr_xs_pcs_dig_ctrl1);

	/* set qxgmii mode */
	hppe_vr_xs_pcs_kr_ctrl_get(dev_id, uniphy_index, &vr_xs_pcs_kr_ctrl);
	if (mode == PORT_WRAPPER_UQXGMII) {
		/* set AM interval mode */
		hppe_vr_xs_pcs_dig_sts_get(dev_id, uniphy_index, &vr_xs_pcs_dig_sts);
		vr_xs_pcs_dig_sts.bf.am_count = UNIPHY_UQXGMII_AM_COUNT;
		hppe_vr_xs_pcs_dig_sts_set(dev_id, uniphy_index, &vr_xs_pcs_dig_sts);
		vr_xs_pcs_kr_ctrl.bf.usxg_mode = UNIPHY_UQXGMII_MODE;
	} else if (mode == PORT_WRAPPER_UDXGMII) {
		vr_xs_pcs_kr_ctrl.bf.usxg_mode = UNIPHY_UDXGMII_MODE;
	}
	hppe_vr_xs_pcs_kr_ctrl_set(dev_id, uniphy_index, &vr_xs_pcs_kr_ctrl);

	/* xpcs software reset */
	hppe_vr_xs_pcs_dig_ctrl1_get(dev_id, uniphy_index, &vr_xs_pcs_dig_ctrl1);
	vr_xs_pcs_dig_ctrl1.bf.vr_rst = 1;
	hppe_vr_xs_pcs_dig_ctrl1_set(dev_id, uniphy_index, &vr_xs_pcs_dig_ctrl1);

	/* enable uniphy autoneg complete interrupt and 10M/100M 8-bits MII width */
	hppe_vr_mii_an_ctrl_get(dev_id, uniphy_index, &vr_mii_an_ctrl);
	vr_mii_an_ctrl.bf.mii_an_intr_en = 1;
	vr_mii_an_ctrl.bf.mii_ctrl = 1;
	hppe_vr_mii_an_ctrl_set(dev_id, uniphy_index, &vr_mii_an_ctrl);
	hppe_vr_mii_an_ctrl_channel1_set(dev_id, uniphy_index, &vr_mii_an_ctrl);
	hppe_vr_mii_an_ctrl_channel2_set(dev_id, uniphy_index, &vr_mii_an_ctrl);
	hppe_vr_mii_an_ctrl_channel3_set(dev_id, uniphy_index, &vr_mii_an_ctrl);

	/* disable TICD */
	hppe_vr_xaui_mode_ctrl_get(dev_id, uniphy_index, &vr_xaui_mode_ctrl);
	vr_xaui_mode_ctrl.bf.ipg_check = 1;
	hppe_vr_xaui_mode_ctrl_set(dev_id, uniphy_index, &vr_xaui_mode_ctrl);
	hppe_vr_xaui_mode_ctrl_channel1_set(dev_id, uniphy_index, &vr_xaui_mode_ctrl);
	hppe_vr_xaui_mode_ctrl_channel2_set(dev_id, uniphy_index, &vr_xaui_mode_ctrl);
	hppe_vr_xaui_mode_ctrl_channel3_set(dev_id, uniphy_index, &vr_xaui_mode_ctrl);

	/* enable uniphy autoneg ability and usxgmii 10g speed and full duplex */
	hppe_sr_mii_ctrl_get(dev_id, uniphy_index, &sr_mii_ctrl);
	sr_mii_ctrl.bf.an_enable = 1;
	sr_mii_ctrl.bf.ss5 = 0;
	sr_mii_ctrl.bf.ss6 = 1;
	sr_mii_ctrl.bf.ss13 = 1;
	sr_mii_ctrl.bf.duplex_mode = 1;
	hppe_sr_mii_ctrl_set(dev_id, uniphy_index, &sr_mii_ctrl);
	hppe_sr_mii_ctrl_channel1_set(0, uniphy_index, &sr_mii_ctrl);
	hppe_sr_mii_ctrl_channel2_set(0, uniphy_index, &sr_mii_ctrl);
	hppe_sr_mii_ctrl_channel3_set(0, uniphy_index, &sr_mii_ctrl);

	/* enable uniphy eee transparent mode*/
	__adpt_hppe_uniphy_uqxgmii_eee_set(dev_id, uniphy_index);

	if(mode == PORT_WRAPPER_UQXGMII)
	{
		rv = hsl_port_phy_mode_set(dev_id, SSDK_PHYSICAL_PORT1, PORT_UQXGMII);
		SW_RTN_ON_ERROR (rv);
	}

	return SW_OK;
}

static sw_error_t
__adpt_hppe_uniphy_usxgmii_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t rv = SW_OK;
	a_uint32_t ssdk_port = 0;

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;
	union vr_xs_pcs_dig_ctrl1_u vr_xs_pcs_dig_ctrl1;
	union vr_mii_an_ctrl_u vr_mii_an_ctrl;
	union sr_mii_ctrl_u sr_mii_ctrl;
	union uniphy_instance_link_detect_u uniphy_instance_link_detect;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	memset(&vr_xs_pcs_dig_ctrl1, 0, sizeof(vr_xs_pcs_dig_ctrl1));
	memset(&vr_mii_an_ctrl, 0, sizeof(vr_mii_an_ctrl));
	memset(&sr_mii_ctrl, 0, sizeof(sr_mii_ctrl));
	memset(&uniphy_instance_link_detect, 0, sizeof(uniphy_instance_link_detect));
	ADPT_DEV_ID_CHECK(dev_id);

	hppe_uniphy_reg_set(dev_id, UNIPHY_MISC2_REG_OFFSET,
		uniphy_index, UNIPHY_MISC2_REG_VALUE);
	/*reset uniphy*/
	hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
		uniphy_index, UNIPHY_PLL_RESET_REG_VALUE);
	msleep(100);
	hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
		uniphy_index, UNIPHY_PLL_RESET_REG_DEFAULT_VALUE);
	msleep(100);

	/* disable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			1, A_FALSE);

	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* configure uniphy to usxgmii mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_QSGMII_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
		UNIPHY_SGMIIPLUS_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_ENABLE;
	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);

	ssdk_port = adpt_hppe_port_get_by_uniphy(dev_id, uniphy_index,
		SSDK_UNIPHY_CHANNEL0);
	if (A_FALSE == hsl_port_is_sfp(dev_id, ssdk_port)) {
		hppe_uniphy_instance_link_detect_get(dev_id,
			uniphy_index, &uniphy_instance_link_detect);
		uniphy_instance_link_detect.bf.detect_los_from_sfp = 0;
		hppe_uniphy_instance_link_detect_set(dev_id,
			uniphy_index, &uniphy_instance_link_detect);
	}

	/* configure uniphy usxgmii gcc software reset */
	__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	msleep(100);

	/* wait calibration done to uniphy */
	__adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	/* enable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			1, A_TRUE);

	/* release xpcs reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_FALSE);

	/* wait 10g base_r link up */
	__adpt_hppe_uniphy_10g_r_linkup(dev_id, uniphy_index);

	/* enable uniphy usxgmii */
	hppe_vr_xs_pcs_dig_ctrl1_get(dev_id, uniphy_index, &vr_xs_pcs_dig_ctrl1);
	vr_xs_pcs_dig_ctrl1.bf.usxg_en = 1;
	hppe_vr_xs_pcs_dig_ctrl1_set(dev_id, uniphy_index, &vr_xs_pcs_dig_ctrl1);

	/* enable uniphy autoneg complete interrupt and 10M/100M 8-bits MII width */
	hppe_vr_mii_an_ctrl_get(dev_id, uniphy_index, &vr_mii_an_ctrl);
	vr_mii_an_ctrl.bf.mii_an_intr_en = 1;
	vr_mii_an_ctrl.bf.mii_ctrl = 1;
	hppe_vr_mii_an_ctrl_set(dev_id, uniphy_index, &vr_mii_an_ctrl);

	/* enable uniphy autoneg ability and usxgmii 10g speed and full duplex */
	hppe_sr_mii_ctrl_get(dev_id, uniphy_index, &sr_mii_ctrl);
	sr_mii_ctrl.bf.an_enable = 1;
	sr_mii_ctrl.bf.ss5 = 0;
	sr_mii_ctrl.bf.ss6 = 1;
	sr_mii_ctrl.bf.ss13 = 1;
	sr_mii_ctrl.bf.duplex_mode = 1;
	hppe_sr_mii_ctrl_set(dev_id, uniphy_index, &sr_mii_ctrl);

	return rv;
}

static sw_error_t
__adpt_hppe_uniphy_10g_r_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t rv = SW_OK;

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;
	union uniphy_instance_link_detect_u uniphy_instance_link_detect;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	memset(&uniphy_instance_link_detect, 0, sizeof(uniphy_instance_link_detect));
	ADPT_DEV_ID_CHECK(dev_id);

	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* disable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
				1, A_FALSE);

	/* configure uniphy to 10g_r mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);

	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_QSGMII_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
		UNIPHY_SGMIIPLUS_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_ENABLE;

	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);
#ifdef MPPE
	if (!(adpt_ppe_type_get(dev_id) == MPPE_TYPE && uniphy_index == SSDK_UNIPHY_INSTANCE0))
#endif
	{
		hppe_uniphy_instance_link_detect_get(dev_id, uniphy_index,
			&uniphy_instance_link_detect);
		uniphy_instance_link_detect.bf.detect_los_from_sfp = UNIPHY_10GR_LINK_LOSS;
		hppe_uniphy_instance_link_detect_set(dev_id, uniphy_index,
			&uniphy_instance_link_detect);
	}
	/* configure uniphy gcc software reset */
	__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	/* wait uniphy calibration done */
	rv = __adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	/* enable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			1, A_TRUE);

	/* release xpcs reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_FALSE);

	return rv;
}
#ifdef MPPE
static sw_error_t
__qca_mppe_uniphy_cal_restart(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t rv = SW_OK;
	union uniphy_vco_cal_control_u cal_control = {0};

	SSDK_DEBUG("uniphy %d do calibration restart\n", uniphy_index);
	rv = mppe_uniphy_calib_ctrl_get(dev_id, uniphy_index, &cal_control);
	SW_RTN_ON_ERROR(rv);
	cal_control.bf.calib_restart = 1;
	rv = mppe_uniphy_calib_ctrl_set(dev_id, uniphy_index, &cal_control);
	SW_RTN_ON_ERROR(rv);
	msleep(10);
	cal_control.bf.calib_restart = 0;
	rv = mppe_uniphy_calib_ctrl_set(dev_id, uniphy_index, &cal_control);
	SW_RTN_ON_ERROR(rv);
	msleep(10);

	return SW_OK;
}
#endif
static sw_error_t
__adpt_hppe_uniphy_sgmiiplus_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	sw_error_t rv = SW_OK;
	a_uint32_t ssdk_port = 0;

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);

	SSDK_DEBUG("uniphy %d is sgmiiplus mode\n", uniphy_index);
#if defined(CPPE)
	if ((adpt_ppe_type_get(dev_id) == CPPE_TYPE)
		&& (uniphy_index == SSDK_UNIPHY_INSTANCE0)) {
		SSDK_DEBUG("cypress uniphy %d is sgmiiplus mode\n", uniphy_index);
		rv = __adpt_cppe_uniphy_mode_set(dev_id, uniphy_index,
			PORT_WRAPPER_SGMII_PLUS);
		return rv;
	}
#endif
	if((adpt_chip_type_get(dev_id) == CHIP_HPPE ||
		adpt_ppe_type_get(dev_id) == APPE_TYPE)
		&& uniphy_index == SSDK_UNIPHY_INSTANCE0)
	{
		/*set src as PHY mode*/
		hppe_uniphy_reg_set(dev_id, UNIPHY_MISC_SOURCE_SELECTION_REG_OFFSET,
			SSDK_UNIPHY_INSTANCE0, UNIPHY_MISC_SRC_PHY_MODE);
		SW_RTN_ON_ERROR (rv);
		/*configure as force mode for uniphy0 sgmii plus based on sgmii*/
		rv = hppe_uniphy_channel0_force_speed_mode_set(dev_id, SSDK_UNIPHY_INSTANCE0,
			UNIPHY_FORCE_SPEED_MODE_ENABLE);
		SW_RTN_ON_ERROR (rv);
	}
	hppe_uniphy_reg_set(dev_id, UNIPHY_MISC2_REG_OFFSET,
		uniphy_index, UNIPHY_MISC2_REG_SGMII_PLUS_MODE);
	if (adpt_ppe_type_get(dev_id) != MPPE_TYPE) {
		/*reset uniphy*/
		hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
			uniphy_index, UNIPHY_PLL_RESET_REG_VALUE);
		msleep(100);
		hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
			uniphy_index, UNIPHY_PLL_RESET_REG_DEFAULT_VALUE);
		msleep(100);
	}

	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* disable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
				1, A_FALSE);

	/* configure uniphy to Athr mode and sgmiiplus mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);

	ssdk_port = adpt_hppe_port_get_by_uniphy(dev_id, uniphy_index,
		SSDK_UNIPHY_CHANNEL0);
	if (A_TRUE == hsl_port_is_sfp(dev_id, ssdk_port)) {
		uniphy_mode_ctrl.bf.newaddedfromhere_ch0_mode_ctrl_25m = 2;
		SSDK_DEBUG("uniphy %d is a sgmiiplus fiber port!\n", uniphy_index);
	}

	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_autoneg_mode =
		UNIPHY_ATHEROS_NEGOTIATION;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_QSGMII_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_DISABLE;
	if((adpt_chip_type_get(dev_id) == CHIP_HPPE ||
		adpt_ppe_type_get(dev_id) == APPE_TYPE)
		&& uniphy_index == SSDK_UNIPHY_INSTANCE0)
	{
		uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
			UNIPHY_SGMIIPLUS_MODE_DISABLE;
	}
	else
	{
		uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
			UNIPHY_SGMIIPLUS_MODE_ENABLE;
	}
	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);

	/* configure uniphy gcc software reset */
#ifdef MPPE
	if (adpt_ppe_type_get(dev_id) == MPPE_TYPE)
		__qca_mppe_uniphy_cal_restart(dev_id, uniphy_index);
	else
#endif
		__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	/* wait uniphy calibration done */
	rv = __adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	/* enable instance clock */
	qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
				1, A_TRUE);
	return rv;
}

static sw_error_t
__adpt_hppe_uniphy_sgmii_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index, a_uint32_t channel)
{
	a_uint32_t i, max_port, ssdk_port;
	sw_error_t rv = SW_OK;
	a_bool_t force_port = 0;

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);

	SSDK_DEBUG("uniphy %d is sgmii mode\n", uniphy_index);
#if defined(CPPE)
	if ((uniphy_index == SSDK_UNIPHY_INSTANCE0) &&
		(channel == SSDK_UNIPHY_CHANNEL0)) {
		if (adpt_ppe_type_get(dev_id) == CPPE_TYPE) {
			if (hsl_port_prop_check(dev_id, SSDK_PHYSICAL_PORT4,
					HSL_PP_EXCL_CPU) == A_TRUE) {
				SSDK_DEBUG("cypress uniphy %d is sgmii mode\n", uniphy_index);
				rv = __adpt_cppe_uniphy_mode_set(dev_id,
					uniphy_index, PORT_WRAPPER_SGMII_CHANNEL0);
				return rv;
			}
		}
	}
#endif

	/*set the PHY mode to SGMII*/
	hppe_uniphy_reg_set(dev_id, UNIPHY_MISC2_REG_OFFSET,
		uniphy_index, UNIPHY_MISC2_REG_SGMII_MODE);
	if (adpt_ppe_type_get(dev_id) != MPPE_TYPE) {
		/*reset uniphy*/
		hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
			uniphy_index, UNIPHY_PLL_RESET_REG_VALUE);
		msleep(100);
		hppe_uniphy_reg_set(dev_id, UNIPHY_PLL_RESET_REG_OFFSET,
			uniphy_index, UNIPHY_PLL_RESET_REG_DEFAULT_VALUE);
		msleep(100);
	}
	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* disable instance clock */
	if (uniphy_index == SSDK_UNIPHY_INSTANCE0)
		max_port = HPPE_UNIPHY0_PORT_MAX;
	else
		max_port = SSDK_PHYSICAL_PORT1;

	for (i = SSDK_PHYSICAL_PORT1; i <= max_port; i++)
	{
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			i, A_FALSE);
	}

#if defined(CPPE)
	if ((adpt_ppe_type_get(dev_id) == CPPE_TYPE) &&
		(uniphy_index == SSDK_UNIPHY_INSTANCE0)) {
		SSDK_DEBUG("uniphy %d sgmii channel selection\n", uniphy_index);
		rv = __adpt_cppe_uniphy_channel_selection_set(dev_id,
			CPPE_PCS0_CHANNEL0_SEL_PSGMII,
			CPPE_PCS0_CHANNEL4_SEL_PORT5_CLOCK);
		SW_RTN_ON_ERROR (rv);
	}
#endif

	/* configure uniphy to Athr mode and sgmii mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);

	ssdk_port = adpt_hppe_port_get_by_uniphy(dev_id, uniphy_index, channel);
	if ((A_TRUE == hsl_port_is_sfp(dev_id, ssdk_port)) &&
		(A_TRUE != hsl_port_feature_get(dev_id, ssdk_port, PHY_F_SFP_SGMII))) {
		uniphy_mode_ctrl.bf.newaddedfromhere_ch0_mode_ctrl_25m =
			UNIPHY_1000BASE_X_MODE;
		SSDK_DEBUG("port_id %d is a fiber port!\n", ssdk_port);
	} else {
		uniphy_mode_ctrl.bf.newaddedfromhere_ch0_mode_ctrl_25m =
			UNIPHY_SGMII_MAC_MODE;
		SSDK_DEBUG("port_id %d is a sfp sgmii or phy port!\n", ssdk_port);
	}

	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_autoneg_mode =
		UNIPHY_ATHEROS_NEGOTIATION;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_QSGMII_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
		UNIPHY_SGMIIPLUS_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_DISABLE;
	if((adpt_chip_type_get(dev_id) == CHIP_HPPE ||
		adpt_ppe_type_get(dev_id) == APPE_TYPE)
		&& uniphy_index == SSDK_UNIPHY_INSTANCE0)
	{
		uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
			UNIPHY_SGMII_MODE_DISABLE;
		/* select channel as a sgmii interface */
		if (channel == SSDK_UNIPHY_CHANNEL0)
		{
			uniphy_mode_ctrl.bf.newaddedfromhere_ch1_ch0_sgmii =
				UNIPHY_SGMII_CHANNEL1_DISABLE;
			uniphy_mode_ctrl.bf.newaddedfromhere_ch4_ch1_0_sgmii =
				UNIPHY_SGMII_CHANNEL4_DISABLE;
		}
		else if (channel == SSDK_UNIPHY_CHANNEL1)
		{
			uniphy_mode_ctrl.bf.newaddedfromhere_ch1_ch0_sgmii =
				UNIPHY_SGMII_CHANNEL1_ENABLE;
			uniphy_mode_ctrl.bf.newaddedfromhere_ch4_ch1_0_sgmii =
				UNIPHY_SGMII_CHANNEL4_DISABLE;
		}
		else if (channel == SSDK_UNIPHY_CHANNEL4)
		{
			uniphy_mode_ctrl.bf.newaddedfromhere_ch1_ch0_sgmii =
				UNIPHY_SGMII_CHANNEL1_DISABLE;
			uniphy_mode_ctrl.bf.newaddedfromhere_ch4_ch1_0_sgmii =
				UNIPHY_SGMII_CHANNEL4_ENABLE;
		}
	}
	else
	{
		uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
			UNIPHY_SGMII_MODE_ENABLE;
	}
	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);

	force_port = hsl_port_feature_get(dev_id,
		ssdk_port, PHY_F_FORCE);
	if (force_port == A_TRUE) {
		rv = hppe_uniphy_channel0_force_speed_mode_set(dev_id,
			uniphy_index, UNIPHY_FORCE_SPEED_MODE_ENABLE);
		SW_RTN_ON_ERROR (rv);
		SSDK_INFO("ssdk uniphy %d connects force port\n",
				uniphy_index);
	}
	else
	{
		rv = hppe_uniphy_channel0_force_speed_mode_set(dev_id,
			uniphy_index, UNIPHY_FORCE_SPEED_MODE_DISABLE);
		SW_RTN_ON_ERROR (rv);
	}
	/* configure uniphy gcc software reset */
#ifdef MPPE
	if (adpt_ppe_type_get(dev_id) == MPPE_TYPE)
		__qca_mppe_uniphy_cal_restart(dev_id, uniphy_index);
	else
#endif
		__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	/* wait uniphy calibration done */
	rv = __adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	/* enable instance clock */
	if (uniphy_index == SSDK_UNIPHY_INSTANCE0)
		max_port = HPPE_UNIPHY0_PORT_MAX;
	else
		max_port = SSDK_PHYSICAL_PORT1;

	for (i = SSDK_PHYSICAL_PORT1; i <= max_port; i++)
	{
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			i, A_TRUE);
	}
#ifdef MHT
	if(hsl_port_phyid_get(dev_id, ssdk_port) == QCA8084_PHY)
	{
		rv = hsl_port_phy_mode_set(dev_id, ssdk_port, PHY_SGMII_BASET);
		SW_RTN_ON_ERROR (rv);
	}
#endif

	return rv;
}

static sw_error_t
__adpt_hppe_uniphy_qsgmii_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	a_uint32_t i;
	sw_error_t rv = SW_OK;

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);

	/* configure malibu phy to qsgmii mode*/
	rv = hsl_port_phy_mode_set(dev_id, SSDK_PHYSICAL_PORT1, PORT_QSGMII);
	SW_RTN_ON_ERROR (rv);

	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* disable instance0 clock */
	for (i = SSDK_PHYSICAL_PORT1; i < SSDK_PHYSICAL_PORT6; i++)
	{
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			i, A_FALSE);
	}

	/* configure uniphy to Athr mode and qsgmii mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_autoneg_mode =
		UNIPHY_ATHEROS_NEGOTIATION;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_QSGMII_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_QSGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_DISABLE;
	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);

	/* configure uniphy gcc software reset */
	__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	/* wait uniphy calibration done */
	rv = __adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	rv = hsl_port_phy_serdes_reset(dev_id);
	SW_RTN_ON_ERROR (rv);

	/* enable instance0 clock */
	for (i = SSDK_PHYSICAL_PORT1; i < SSDK_PHYSICAL_PORT6; i++)
	{
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			i, A_TRUE);
	}

	return rv;
}

static sw_error_t
__adpt_hppe_uniphy_psgmii_mode_set(a_uint32_t dev_id, a_uint32_t uniphy_index)
{
	a_uint32_t i;
	sw_error_t rv = SW_OK;
#if defined(CPPE)
	a_uint32_t phy_type = 0;
#endif

	union uniphy_mode_ctrl_u uniphy_mode_ctrl;

	memset(&uniphy_mode_ctrl, 0, sizeof(uniphy_mode_ctrl));
	ADPT_DEV_ID_CHECK(dev_id);

	SSDK_DEBUG("uniphy %d is psgmii mode\n", uniphy_index);
#if defined(CPPE)
	if (adpt_ppe_type_get(dev_id) == CPPE_TYPE) {
		phy_type = hsl_port_phyid_get(dev_id,
				SSDK_PHYSICAL_PORT3);
		if (phy_type == MALIBU2PORT_PHY) {
			SSDK_INFO("cypress uniphy %d is qca8072 psgmii mode\n", uniphy_index);
			rv = __adpt_cppe_uniphy_mode_set(dev_id, uniphy_index,
				PORT_WRAPPER_PSGMII);
			return rv;
		}
	}
#endif

	/* keep xpcs to reset status */
	__adpt_hppe_gcc_uniphy_xpcs_reset(dev_id, uniphy_index, A_TRUE);

	/* disable instance0 clock */
	for (i = SSDK_PHYSICAL_PORT1; i < SSDK_PHYSICAL_PORT6; i++)
	{
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			i, A_FALSE);
	}

#if defined(CPPE)
	if ((adpt_ppe_type_get(dev_id) == CPPE_TYPE) &&
		(uniphy_index == SSDK_UNIPHY_INSTANCE0)) {
		SSDK_INFO("uniphy %d psgmii channel selection\n", uniphy_index);
		rv = __adpt_cppe_uniphy_channel_selection_set(dev_id,
			CPPE_PCS0_CHANNEL0_SEL_PSGMII,
			CPPE_PCS0_CHANNEL4_SEL_PORT5_CLOCK);
		SW_RTN_ON_ERROR (rv);
	}
#endif

	/* configure uniphy to Athr mode and psgmii mode */
	hppe_uniphy_mode_ctrl_get(dev_id, uniphy_index, &uniphy_mode_ctrl);
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_autoneg_mode =
		UNIPHY_ATHEROS_NEGOTIATION;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_psgmii_qsgmii =
		UNIPHY_CH0_PSGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_ch0_qsgmii_sgmii =
		UNIPHY_CH0_SGMII_MODE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sg_mode =
		UNIPHY_SGMII_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_sgplus_mode =
		UNIPHY_SGMIIPLUS_MODE_DISABLE;
	uniphy_mode_ctrl.bf.newaddedfromhere_xpcs_mode =
		UNIPHY_XPCS_MODE_DISABLE;
	hppe_uniphy_mode_ctrl_set(dev_id, uniphy_index, &uniphy_mode_ctrl);

	/* configure uniphy gcc software reset */
	__adpt_ppe_gcc_uniphy_software_reset(dev_id, uniphy_index);

	/* wait uniphy calibration done */
	rv = __adpt_hppe_uniphy_calibrate(dev_id, uniphy_index);

	rv = hsl_port_phy_serdes_reset(dev_id);
	SW_RTN_ON_ERROR (rv);

	/* enable instance0 clock */
	for (i = SSDK_PHYSICAL_PORT1; i < SSDK_PHYSICAL_PORT6; i++)
	{
		qca_gcc_uniphy_port_clock_set(dev_id, uniphy_index,
			i, A_TRUE);
	}

	return rv;
}

#ifdef MPPE
static sw_error_t
_adpt_mppe_uniphy_clk_output_set(a_uint32_t dev_id, a_uint32_t index,
	a_uint32_t clk_rate)
{
	sw_error_t rv = SW_OK;
	union uniphy_clkout_50m_ctrl_u clkout_50m_ctrl = {0};

	SSDK_INFO("uniphy will output clock as %dHz\n", clk_rate);
	rv = mppe_uniphy_clkout_50m_ctrl_get(dev_id, index, &clkout_50m_ctrl);
	SW_RTN_ON_ERROR(rv);

	if (clk_rate == UNIPHY_CLK_RATE_25M) {
		clkout_50m_ctrl.bf.clk_50m_div2_sel = 1;
	} else if (clk_rate == UNIPHY_CLK_RATE_50M) {
		clkout_50m_ctrl.bf.clk_50m_div2_sel = 0;
	} else if (clk_rate == 0) {
		clkout_50m_ctrl.bf.clk_50m_25m_en = 0;
	} else {
		return SW_NOT_SUPPORTED;
	}

	rv = mppe_uniphy_clkout_50m_ctrl_set(dev_id, index, &clkout_50m_ctrl);
	return rv;
}

static void
adpt_mppe_uniphy_clk_output_set(a_uint32_t dev_id, a_uint32_t index)
{
	a_uint32_t phy_id =0, port_id = 0;

	port_id = adpt_hppe_port_get_by_uniphy(dev_id, index, SSDK_UNIPHY_CHANNEL0);
	if (hsl_port_feature_get(dev_id, port_id, PHY_F_FORCE)) {
		a_uint32_t force_speed = hsl_port_force_speed_get(dev_id, port_id);

		if (force_speed == FAL_SPEED_1000) {
			/* modify the uniphy clock as 25M for s17c connected. */
			_adpt_mppe_uniphy_clk_output_set(dev_id, index, UNIPHY_CLK_RATE_25M);
		} else if (force_speed == FAL_SPEED_2500) {
			static bool mht_clock_enable = false;

			/* when P0 and P5 are both connected to mppe, the mht 50M clock should
			* come from the mppe uniphy that was confiured firstly, then need to
			* disable the clock output of another uniphy.
			*/
			if (mht_clock_enable && 2 == ssdk_switch_device_num_get()) {
				_adpt_mppe_uniphy_clk_output_set(dev_id, index, 0);
			}
			mht_clock_enable = true;
		}
	}
	phy_id = hsl_port_phyid_get(dev_id, port_id);

	if (phy_id == QCA8030_PHY || phy_id == QCA8033_PHY || phy_id == QCA8035_PHY)
	{
		_adpt_mppe_uniphy_clk_output_set(dev_id, index, UNIPHY_CLK_RATE_25M);
		hsl_port_phy_gpio_reset(dev_id, port_id);
		hsl_port_phy_hw_init(dev_id, port_id);
	}

	/* For miami connected with manhattan bypass device, the port4 is connected with
	 * uniphy1 of miami, the clock of manhattan is provided from the uniphy0 of miami,
	 * so do not need to enable the clock from uniphy1 of miami.
	 */
	if (QCA8084_PHY == phy_id)
		_adpt_mppe_uniphy_clk_output_set(dev_id, index, 0);

	return;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0))
a_bool_t
adpt_hppe_uniphy_check(a_uint32_t dev_id, a_uint32_t index, a_uint32_t mode)
{
	adpt_ppe_type_t ppe_type = adpt_ppe_type_get(dev_id);

	if (ppe_type == APPE_TYPE) {
		if (((index == SSDK_UNIPHY_INSTANCE1)
			&& (cpu_is_uniphy1_enabled() == A_FALSE)) ||
			((index == SSDK_UNIPHY_INSTANCE2)
			&& (cpu_is_uniphy2_enabled() == A_FALSE))) {
			return A_FALSE;
		}
	} else if (ppe_type == MPPE_TYPE) {
#ifdef MPPE
		if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
			return A_FALSE;
		}
		if ((cpu_is_ipq5312() == A_TRUE) || (cpu_is_ipq5302() == A_TRUE)) {
			if ((mode == PORT_WRAPPER_10GBASE_R)
				|| (mode == PORT_WRAPPER_USXGMII)) {
				return A_FALSE;
			}
		}
#endif
	} else if ((ppe_type == HPPE_TYPE) || (ppe_type == CPPE_TYPE)) {
		if ((mode == PORT_WRAPPER_UQXGMII) || (mode == PORT_WRAPPER_UDXGMII)) {
			return A_FALSE;
		}
	}
	return A_TRUE;
}
#endif

sw_error_t
adpt_hppe_uniphy_mode_set(a_uint32_t dev_id, a_uint32_t index, a_uint32_t mode)
{
	sw_error_t rv = SW_OK;
	a_uint32_t clock = UNIPHY_CLK_RATE_125M;

	if (mode == PORT_WRAPPER_MAX) {
		ssdk_uniphy_raw_clock_reset(index);
		return SW_OK;
	}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5,4,0))
	if (adpt_hppe_uniphy_check(dev_id, index, mode) == A_FALSE) {
		SSDK_INFO("ssdk doesn't support mode:%d in uniphy:%d on platform!\n",
			mode, index);
		return SW_OK;
	}
#endif
	switch(mode) {
		case PORT_WRAPPER_PSGMII:
		case PORT_WRAPPER_PSGMII_FIBER:
			rv = __adpt_hppe_uniphy_psgmii_mode_set(dev_id, index);
			break;

		case PORT_WRAPPER_QSGMII:
			rv = __adpt_hppe_uniphy_qsgmii_mode_set(dev_id, index);
			break;

		case PORT_WRAPPER_SGMII0_RGMII4:
		case PORT_WRAPPER_SGMII_CHANNEL0:
		case PORT_WRAPPER_SGMII_FIBER:
			rv = __adpt_hppe_uniphy_sgmii_mode_set(dev_id, index,
				SSDK_UNIPHY_CHANNEL0);
			break;

		case PORT_WRAPPER_SGMII1_RGMII4:
		case PORT_WRAPPER_SGMII_CHANNEL1:
			rv = __adpt_hppe_uniphy_sgmii_mode_set(dev_id, index,
				SSDK_UNIPHY_CHANNEL1);
			break;

		case PORT_WRAPPER_SGMII4_RGMII4:
		case PORT_WRAPPER_SGMII_CHANNEL4:
			rv = __adpt_hppe_uniphy_sgmii_mode_set(dev_id, index,
				SSDK_UNIPHY_CHANNEL4);
			break;

		case PORT_WRAPPER_SGMII_PLUS:
			rv = __adpt_hppe_uniphy_sgmiiplus_mode_set(dev_id, index);
			clock = UNIPHY_CLK_RATE_312M;
			break;

		case PORT_WRAPPER_10GBASE_R:
			rv = __adpt_hppe_uniphy_10g_r_mode_set(dev_id, index);
			clock = UNIPHY_CLK_RATE_312M;
			break;

		case PORT_WRAPPER_USXGMII:
			rv = __adpt_hppe_uniphy_usxgmii_mode_set(dev_id, index);
			clock = UNIPHY_CLK_RATE_312M;
			break;
		case PORT_WRAPPER_UQXGMII:
		case PORT_WRAPPER_UQXGMII_3CHANNELS:
		case PORT_WRAPPER_UDXGMII:
			rv = __adpt_hppe_uniphy_uxgmii_mode_set(dev_id, index, mode);
			clock = UNIPHY_CLK_RATE_312M;
			break;
		default:
			rv = SW_FAIL;
	}
	if (SW_OK == rv) {
		ssdk_uniphy_raw_clock_set(index, UNIPHY_RX, clock);
		ssdk_uniphy_raw_clock_set(index, UNIPHY_TX, clock);
	}
#ifdef MPPE
	/*configure phy clock from uniphy*/
	if (adpt_ppe_type_get(dev_id) == MPPE_TYPE)
		adpt_mppe_uniphy_clk_output_set(dev_id, index);
#endif
	return rv;
}
sw_error_t adpt_hppe_uniphy_init(a_uint32_t dev_id)
{
	adpt_api_t *p_adpt_api = NULL;

	p_adpt_api = adpt_api_ptr_get(dev_id);

	if(p_adpt_api == NULL)
		return SW_FAIL;

	p_adpt_api->adpt_uniphy_mode_set = adpt_hppe_uniphy_mode_set;

	return SW_OK;
}

/**
 * @}
 */
