/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "qcom_phy_lib.h"
#include "clock/qca8k_clk.h"

static int qcom_phy_pcs_addr_get(struct phy_device *phydev, int offset)
{
	if (phydev->shared)
		return phydev->shared->addr + offset;

	return phydev->mdio.addr + offset;
}

int qcom_phy_pcs_modify(struct phy_device *phydev, int addr_offset,
	u32 regnum, u16 mask, u16 set)
{
	int addr = qcom_phy_pcs_addr_get(phydev, addr_offset);

	if (addr < 0)
		return -EINVAL;

	return mdiobus_modify(phydev->mdio.bus, addr, regnum, mask, set);
}

int qcom_phy_pcs_read_mmd(struct phy_device *phydev, int addr_offset,
	int devad, u32 regnum)
{
	int addr = qcom_phy_pcs_addr_get(phydev, addr_offset);

	if (addr < 0)
		return -EINVAL;

	return mdiobus_c45_read(phydev->mdio.bus, addr, devad, regnum);
}

int qcom_phy_pcs_write_mmd(struct phy_device *phydev, int addr_offset,
	int devad, u32 regnum, u16 val)
{
	int addr = qcom_phy_pcs_addr_get(phydev, addr_offset);

	if (addr < 0)
		return -EINVAL;

	return mdiobus_c45_write(phydev->mdio.bus, addr, devad, regnum, val);
}

int qcom_phy_pcs_modify_mmd(struct phy_device *phydev, int addr_offset,
	int devad, u32 regnum, u16 mask, u16 set)
{
	int addr = qcom_phy_pcs_addr_get(phydev, addr_offset);

	if (addr < 0)
		return -EINVAL;

	return mdiobus_c45_modify(phydev->mdio.bus, addr, devad, regnum,
		mask, set);
}

static int qcom_phy_xpcs_mmd_get(struct phy_device *phydev, int channel)
{
	switch(channel) {
	case 1:
		return MDIO_MMD_VEND2;
	case 2:
		return 26;
	case 3:
		return 27;
	case 4:
		return 28;
	default:
		return -EOPNOTSUPP;
	}
}

static int qcom_phy_xpcs_modify_channel_mmd(struct phy_device *phydev,
	u32 addr_offset, u32 channel, u32 regnum, u16 mask, u16 set)
{
	int mmd_id = qcom_phy_xpcs_mmd_get(phydev, channel);

	if (mmd_id < 0)
		return -EOPNOTSUPP;

	return qcom_phy_pcs_modify_mmd(phydev, addr_offset, mmd_id,
		regnum, mask, set);
}

static int qcom_phy_xpcs_10g_r_linkup(struct phy_device *phydev,
	u32 addr_offset)
{
	u16 xpcs_data = 0;
	u32 retries = 100, linkup = 0;

	/* wait 10G_R link up */
	while (linkup != QCOM_PHY_PCS_MMD3_10GBASE_R_UP) {
		mdelay(1);
		if (retries-- == 0) {
			phydev_err (phydev, "10g_r link up timeout\n");
			return -ETIMEDOUT;
		}
		xpcs_data = qcom_phy_pcs_read_mmd(phydev, addr_offset, MDIO_MMD_PCS,
			QCOM_PHY_PCS_MMD3_10GBASE_R_PCS_STATUS1);

		linkup = (xpcs_data & QCOM_PHY_PCS_MMD3_10GBASE_R_UP);
	}

	return 0;
}

static int qcom_phy_xpcs_soft_reset(struct phy_device *phydev,
	u32 addr_offset)
{
	int ret = 0;
	u16 xpcs_data = 0;
	u32 retries = 100, reset_done = QCOM_PHY_PCS_MMD3_XPCS_SOFT_RESET;

	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_DIG_CTRL1, 0x8000, QCOM_PHY_PCS_MMD3_XPCS_SOFT_RESET);
	if (ret < 0)
		return ret;

	while (reset_done) {
		mdelay(1);
		if (retries-- == 0)
			return -ETIMEDOUT;
		xpcs_data = qcom_phy_pcs_read_mmd(phydev, addr_offset, MDIO_MMD_PCS,
			QCOM_PHY_PCS_MMD3_DIG_CTRL1);

		reset_done = (xpcs_data & QCOM_PHY_PCS_MMD3_XPCS_SOFT_RESET);
	}

	return 0;
}

int qcom_phy_xpcs_8023az_enable(struct phy_device *phydev,
	u32 addr_offset)
{
	u16 xpcs_data = 0;

	xpcs_data = qcom_phy_pcs_read_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_AN_LP_BASE_ABL2);
	if(!(xpcs_data & QCOM_PHY_PCS_MMD3_XPCS_EEE_CAP))
		return -EOPNOTSUPP;

	/*Configure the EEE related timer*/
	qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_EEE_MODE_CTRL, 0x0f40, QCOM_PHY_PCS_MMD3_EEE_RES_REGS |
		QCOM_PHY_PCS_MMD3_EEE_SIGN_BIT_REGS);

	qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_EEE_TX_TIMER, 0x1fff, QCOM_PHY_PCS_MMD3_EEE_TSL_REGS|
		QCOM_PHY_PCS_MMD3_EEE_TLU_REGS | QCOM_PHY_PCS_MMD3_EEE_TWL_REGS);

	qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_EEE_RX_TIMER, 0x1fff, QCOM_PHY_PCS_MMD3_EEE_100US_REG_REGS|
		QCOM_PHY_PCS_MMD3_EEE_RWR_REG_REGS);

	/*enable TRN_LPI*/
	qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_EEE_MODE_CTRL1, 0x101, QCOM_PHY_PCS_MMD3_EEE_TRANS_LPI_MODE|
		QCOM_PHY_PCS_MMD3_EEE_TRANS_RX_LPI_MODE);

	/*enable TX/RX LPI pattern*/
	qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_EEE_MODE_CTRL, 0x3, QCOM_PHY_PCS_MMD3_EEE_EN);

	return 0;
}

int qcom_phy_xpcs_qusgmii_function_reset(struct phy_device *phydev,
	u32 addr_offset, u32 channel)
{
	int ret = 0;

	if(channel == 1)
		ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PCS,
			QCOM_PHY_PCS_MMD_MII_DIG_CTRL,
			0x400, QCOM_PHY_PCS_MMD3_USXG_FIFO_RESET);
	else
		ret = qcom_phy_xpcs_modify_channel_mmd(phydev, addr_offset, channel,
			QCOM_PHY_PCS_MMD_MII_DIG_CTRL,
			QCOM_PHY_PCS_MMD_USXG_FIFO_RESET,
			QCOM_PHY_PCS_MMD_USXG_FIFO_RESET);

	return ret;
}

int qcom_phy_xpcs_autoneg_restart(struct phy_device *phydev,
	u32 addr_offset, u32 channel)
{
	int ret = 0, retries = 500, xpcs_data = 0, mmd_id = 0;

	mmd_id = qcom_phy_xpcs_mmd_get(phydev, channel);
	if (mmd_id < 0)
		return -EOPNOTSUPP;
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, mmd_id,
		QCOM_PHY_PCS_MMD_MII_CTRL, QCOM_PHY_PCS_MMD_MII_AN_RESTART,
		QCOM_PHY_PCS_MMD_MII_AN_RESTART);
	if (ret < 0)
		return ret;
	mdelay(1);
	xpcs_data = qcom_phy_pcs_read_mmd(phydev, addr_offset, mmd_id,
		QCOM_PHY_PCS_MMD_MII_ERR_SEL);
	while(!(xpcs_data & QCOM_PHY_PCS_MMD_MII_AN_COMPLETE_INT))
	{
		mdelay(1);
		if (retries-- == 0)
			return -ETIMEDOUT;
		xpcs_data = qcom_phy_pcs_read_mmd(phydev, addr_offset, mmd_id,
			QCOM_PHY_PCS_MMD_MII_ERR_SEL);
	}
	/*clear autoneg complete interrupt*/
	ret = qcom_phy_pcs_write_mmd(phydev, addr_offset, mmd_id,
		QCOM_PHY_PCS_MMD_MII_ERR_SEL, xpcs_data & (~BIT(0)));

	return ret;
}

static int qcom_phy_pcs_calibration(struct phy_device *phydev,
	u32 addr_offset)
{
	u16 pcs_data = 0;
	u32 retries = 100, calibration_done = 0;

	/* wait calibration done to uniphy*/
	while (calibration_done != QCOM_PHY_PCS_MMD1_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0) {
			phydev_err(phydev, "pcs %d calibration time out!\n", addr_offset);
			return -ETIMEDOUT;
		}
		pcs_data = qcom_phy_pcs_read_mmd(phydev, addr_offset,
			MDIO_MMD_PMAPMD, QCOM_PHY_PCS_MMD1_CALIBRATION4);

		calibration_done = (pcs_data & QCOM_PHY_PCS_MMD1_CALIBRATION_DONE);
	}

	return 0;
}

int qcom_phy_pcs_usxgmii_reset(struct phy_device *phydev,
	u32 addr_offset, u32 channel)
{
	int ret;

	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_USXGMII_RESET, BIT(channel - 1), 0);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_USXGMII_RESET, BIT(channel - 1), BIT(channel - 1));

	return ret;
}

int qcom_phy_pcs_sgmii_function_reset(struct phy_device *phydev,
	u32 addr_offset)
{
	int ret = 0;

	/*sgmii channel0 adpt reset*/
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_CHANNEL0_CFG, QCOM_PHY_PCS_MMD1_SGMII_ADPT_RESET, 0);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_CHANNEL0_CFG, QCOM_PHY_PCS_MMD1_SGMII_ADPT_RESET,
		QCOM_PHY_PCS_MMD1_SGMII_ADPT_RESET);

	return ret;
}

int qcom_phy_pcs_ipg_tune_reset(struct phy_device *phydev,
	u32 addr_offset)
{
	int ret = 0;

	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_USXGMII_RESET, QCOM_PHY_PCS_MMD1_SGMII_FUNC_RESET, 0);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_USXGMII_RESET, QCOM_PHY_PCS_MMD1_SGMII_FUNC_RESET,
		QCOM_PHY_PCS_MMD1_SGMII_FUNC_RESET);

	return ret;
}

static int _qcom_phy_pcs_qusgmii_mode_set(struct phy_device *phydev, u32 pcs_addr_offset,
	u32 xpcs_addr_offset)
{
	int ret = 0, channel = 0;

	/* assert xpcs */
	qca8k_clk_assert(&phydev->mdio, MHT_UNIPHY_XPCS_RST);

	/* fix PLL unlock issue with high temperature */
	ret = qcom_phy_pcs_modify(phydev, pcs_addr_offset,
		QCOM_PHY_PCS_PLL_LOOP_CONTROL,
		QCOM_PHY_PCS_PLL_CML2CMS_IBSEL,
		QCOM_PHY_PCS_PLL_CML2CMS_IBSEL);
	if (ret < 0)
		return ret;
	/* select xpcs mode */
	ret = qcom_phy_pcs_modify_mmd(phydev, pcs_addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_MODE_CTRL, 0x1f00, QCOM_PHY_PCS_MMD1_XPCS_MODE);
	if (ret < 0)
		return ret;
	ret = qcom_phy_pcs_modify_mmd(phydev, pcs_addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_GMII_DATAPASS_SEL, QCOM_PHY_PCS_MMD1_DATAPASS_MASK,
		QCOM_PHY_PCS_MMD1_DATAPASS_USXGMII);
	if (ret < 0)
	/* reset and release PCS GMII/XGMII and PHY GMII */
	for(channel = 1; channel <= 4; channel++) {
		qca8k_port_clk_reset(&phydev->mdio, channel, MHT_CLK_TYPE_UNIPHY|MHT_CLK_TYPE_EPHY);
	}
	/* ana sw reset and release */
	ret = qcom_phy_pcs_modify(phydev, pcs_addr_offset,
		QCOM_PHY_PCS_PLL_POWER_ON_AND_RESET, 0x40, QCOM_PHY_PCS_ANA_SOFT_RESET);
	if (ret < 0)
		return ret;
	mdelay(10);
	ret = qcom_phy_pcs_modify(phydev, pcs_addr_offset,
		QCOM_PHY_PCS_PLL_POWER_ON_AND_RESET, 0x40, QCOM_PHY_PCS_ANA_SOFT_RELEASE);
	if (ret < 0)
		return ret;
	/* Wait calibration done */
	qcom_phy_pcs_calibration(phydev, pcs_addr_offset);
	/* Enable SSCG(Spread Spectrum Clock Generator) */
	ret = qcom_phy_pcs_modify_mmd(phydev, pcs_addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_CDA_CONTROL1, 0x8, QCOM_PHY_PCS_MMD1_SSCG_ENABLE);
	if (ret < 0)
		return ret;
	/* de-assert XPCS */
	qca8k_clk_deassert(&phydev->mdio, MHT_UNIPHY_XPCS_RST);

	/* PHY software reset */
	for(channel = 1; channel <= 4; channel++) {
		ret = mdiobus_read(phydev->mdio.bus, phydev->shared->addr + channel - 1, MII_BMCR);
		if (!(ret & BMCR_PDOWN))
			mdiobus_modify(phydev->mdio.bus, phydev->shared->addr + channel - 1,
				MII_BMCR, BMCR_RESET, BMCR_RESET);
	}
	/* Set BaseR mode */
	ret = qcom_phy_pcs_modify_mmd(phydev, xpcs_addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_PCS_CTRL2, 0xf, QCOM_PHY_PCS_MMD3_PCS_TYPE_10GBASE_R);
	if (ret < 0)
	/* wait 10G base_r link up */
	ret = qcom_phy_xpcs_10g_r_linkup(phydev, xpcs_addr_offset);
	if (ret < 0)
		return ret;
	/* enable UQXGMII mode */
	ret = qcom_phy_pcs_modify_mmd(phydev, xpcs_addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_DIG_CTRL1, 0x200, QCOM_PHY_PCS_MMD3_USXGMII_EN);
	if (ret < 0)
		return ret;
	/* set UQXGMII mode */
	ret = qcom_phy_pcs_modify_mmd(phydev, xpcs_addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_VR_RPCS_TPC, 0x1c00, QCOM_PHY_PCS_MMD3_QXGMII_EN);
	if (ret < 0)
		return ret;
	/* set AM interval */
	ret = qcom_phy_pcs_write_mmd(phydev, xpcs_addr_offset, MDIO_MMD_PCS,
		QCOM_PHY_PCS_MMD3_MII_AM_INTERVAL, QCOM_PHY_PCS_MMD3_MII_AM_INTERVAL_VAL);
	if (ret < 0)
		return ret;
	/* xpcs software reset */
	ret = qcom_phy_xpcs_soft_reset(phydev, xpcs_addr_offset);

	return ret;
}

static int qcom_phy_pcs_qusgmii_mode_set(struct phy_device *phydev,
	struct qcom_phy_pcs_cfg config)
{
	int ret = 0;
	u32 channel = 0, pcs_addr_offset = 0, xpcs_addr_offset = 0;

	if (phydev->phy_id == QCA8084_PHY_ID)
		xpcs_addr_offset = XPCS_ADDR_OFFSET;
	else
		xpcs_addr_offset = pcs_addr_offset;

	pcs_addr_offset = config.addr_offset;

	/* dassert serdes if it is asserted */
	if (qca8k_clk_is_asserted(&phydev->mdio, MHT_SRDS1_SYS_CLK)) {
		qca8k_clk_deassert(&phydev->mdio, MHT_SRDS1_SYS_CLK);
	}

	/* disable IPG_tuning bypass */
	ret = qcom_phy_pcs_modify_mmd(phydev, pcs_addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_BYPASS_TUNING_IPG,
		QCOM_PHY_PCS_MMD1_BYPASS_TUNING_IPG_EN, 0);
	if (ret < 0)
		return ret;
	/* disable PCS GMII/XGMII clock and disable PHY GMII clock */
	for(channel = 1; channel <= 4; channel++)
	{
		qca8k_port_clk_en_set(&phydev->mdio, channel,
			MHT_CLK_TYPE_UNIPHY|MHT_CLK_TYPE_EPHY, false);
	}
	/* configure uqxgmii mode */
	ret = _qcom_phy_pcs_qusgmii_mode_set(phydev, pcs_addr_offset, xpcs_addr_offset);
	if (ret < 0)
		return ret;
	/* enable auto-neg complete interrupt,Mii using mii-4bits, */
	/* configure as PHY mode, enable autoneg ability */
	for (channel = 1; channel <= 4; channel++)
	{
		/* enable auto-neg complete interrupt,Mii using mii-4bits, */
		/* configure as PHY mode */
		ret = qcom_phy_xpcs_modify_channel_mmd(phydev,
			xpcs_addr_offset, channel,
			QCOM_PHY_PCS_MMD_MII_AN_INT_MSK, 0x109,
			QCOM_PHY_PCS_MMD_AN_COMPLETE_INT |
			QCOM_PHY_PCS_MMD_MII_4BITS_CTRL |
			QCOM_PHY_PCS_MMD_TX_CONFIG_CTRL);
		if (ret < 0)
			return ret;

		/* enable autoneg ability */
		ret = qcom_phy_xpcs_modify_channel_mmd(phydev, xpcs_addr_offset,
			channel,
			QCOM_PHY_PCS_MMD_MII_CTRL, 0x3060, QCOM_PHY_PCS_MMD_MII_AN_ENABLE |
			QCOM_PHY_PCS_MMD_XPC_SPEED_1000);
		if (ret < 0)
			return ret;
		/* disable TICD */
		ret = qcom_phy_xpcs_modify_channel_mmd(phydev, xpcs_addr_offset, channel,
			QCOM_PHY_PCS_MMD_MII_XAUI_MODE_CTRL, 0x1,
			QCOM_PHY_PCS_MMD_TX_IPG_CHECK_DISABLE);
		if (ret < 0)
			return ret;
		/* enable PHY mode control to sync phy link information to XPCS */
		ret = qcom_phy_xpcs_modify_channel_mmd(phydev, xpcs_addr_offset, channel,
			QCOM_PHY_PCS_MMD_MII_DIG_CTRL, BIT(0),
			QCOM_PHY_PCS_MMD_PHY_MODE_CTRL_EN);
		if (ret < 0)
			return ret;
	}

	/*enable EEE for xpcs*/
	ret = qcom_phy_xpcs_8023az_enable(phydev, xpcs_addr_offset);

	return ret;
}

static int qcom_phy_pcs_sgmii_mode_set(struct phy_device *phydev,
	struct qcom_phy_pcs_cfg config)
{
	int ret = 0;
	u32 mode_ctrl = 0, speed_mode = 0, phy_clk_index = 0, addr_offset = 0,
		ethphy_clk_mask = 0, pcs_clk_index = 0;
	u64 raw_clk = 0;

	addr_offset = config.addr_offset;

	/* de-assert serdes if it is asserted */
	if (config.addr_offset == PCS0_ADDR_OFFSET) {
		pcs_clk_index = 5;
		if (config.clock_mode == CLOCK_PHY_MODE)
			phy_clk_index = 4;
		else
			phy_clk_index = 5;
		if (qca8k_clk_is_asserted(&phydev->mdio, MHT_SRDS0_SYS_CLK))
			qca8k_clk_deassert(&phydev->mdio, MHT_SRDS0_SYS_CLK);
	} else if (addr_offset == PCS1_ADDR_OFFSET) {
		if (qca8k_clk_is_asserted(&phydev->mdio, MHT_SRDS1_SYS_CLK))
			qca8k_clk_deassert(&phydev->mdio, MHT_SRDS1_SYS_CLK);
	}

	if(config.type == PHY_INTERFACE_MODE_SGMII)
	{
		mode_ctrl = QCOM_PHY_PCS_MMD1_SGMII_MODE;
		raw_clk = 125000000;
	}
	else
	{
		mode_ctrl = QCOM_PHY_PCS_MMD1_SGMII_PLUS_MODE;
		raw_clk = 312500000;
	}

	if(config.clock_mode == CLOCK_MAC_MODE) {
		mode_ctrl |= QCOM_PHY_PCS_MMD1_SGMII_MAC_MODE;
	} else {
		mode_ctrl |= QCOM_PHY_PCS_MMD1_SGMII_PHY_MODE;
		ethphy_clk_mask = MHT_CLK_TYPE_EPHY;
	}

	/* GMII interface clock disable */
	ret = qca8k_port_clk_en_set(&phydev->mdio, phy_clk_index,
		ethphy_clk_mask, false);
	ret = qca8k_port_clk_en_set(&phydev->mdio, pcs_clk_index,
		MHT_CLK_TYPE_UNIPHY, false);

	/* uniphy1 xpcs reset, and configure raw clk */
	if(addr_offset == PCS1_ADDR_OFFSET) {
		qca8k_clk_assert(&phydev->mdio, MHT_UNIPHY_XPCS_RST);
		qca8k_uniphy_raw_clock_set(MHT_P_UNIPHY1_RX, raw_clk);
		qca8k_uniphy_raw_clock_set(MHT_P_UNIPHY1_TX, raw_clk);
	} else {
		qca8k_uniphy_raw_clock_set(MHT_P_UNIPHY0_RX, raw_clk);
		qca8k_uniphy_raw_clock_set(MHT_P_UNIPHY0_TX, raw_clk);
	}
	/* fix PLL unlock issue with high temperature */
	ret = qcom_phy_pcs_modify(phydev, addr_offset, QCOM_PHY_PCS_PLL_LOOP_CONTROL,
		QCOM_PHY_PCS_PLL_CML2CMS_IBSEL, QCOM_PHY_PCS_PLL_CML2CMS_IBSEL);
	if (ret < 0)
		return ret;
	/* configure SGMII mode or SGMII+ mode */
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_MODE_CTRL, QCOM_PHY_PCS_MMD1_SGMII_MODE_CTRL_MASK,
		mode_ctrl);
	if (ret < 0)
		return ret;
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_GMII_DATAPASS_SEL, QCOM_PHY_PCS_MMD1_DATAPASS_MASK,
		QCOM_PHY_PCS_MMD1_DATAPASS_SGMII);
	if (ret < 0)
		return ret;
	/*configue force or autoneg*/
	if(!config.auto_neg)
	{
		ret = qcom_phy_pcs_speed_clock_set(phydev, phy_clk_index,
			config.force_speed);
		if (ret < 0)
			return ret;
		switch (config.force_speed) {
		case SPEED_10:
			speed_mode = QCOM_PHY_PCS_MMD1_CH0_FORCE_ENABLE |
				QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_10M;
			break;
		case SPEED_100:
			speed_mode = QCOM_PHY_PCS_MMD1_CH0_FORCE_ENABLE |
				QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_100M;
			break;
		case SPEED_1000:
		case SPEED_2500:
			speed_mode = QCOM_PHY_PCS_MMD1_CH0_FORCE_ENABLE |
				QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_1G;
			break;
		default:
			break;
		}
	} else {
		speed_mode = QCOM_PHY_PCS_MMD1_CH0_AUTONEG_ENABLE;
	}
	ret = qcom_phy_pcs_modify_mmd(phydev, addr_offset, MDIO_MMD_PMAPMD,
		QCOM_PHY_PCS_MMD1_CHANNEL0_CFG, QCOM_PHY_PCS_MMD1_CH0_FORCE_SPEED_MASK,
		speed_mode);
	if (ret < 0)
		return ret;
	/* GMII interface clock reset and release */
	ret = qca8k_port_clk_reset(&phydev->mdio, phy_clk_index, ethphy_clk_mask);
	if (ret < 0)
		return ret;
	ret = qca8k_port_clk_reset(&phydev->mdio, pcs_clk_index, MHT_CLK_TYPE_UNIPHY);
	if (ret < 0)
		return ret;
	/* analog software reset and release */
	ret = qcom_phy_pcs_modify(phydev, addr_offset,
		QCOM_PHY_PCS_PLL_POWER_ON_AND_RESET, 0x40, QCOM_PHY_PCS_ANA_SOFT_RESET);
	if (ret < 0)
		return ret;
	mdelay(1);
	ret = qcom_phy_pcs_modify(phydev, addr_offset,
		QCOM_PHY_PCS_PLL_POWER_ON_AND_RESET, 0x40, QCOM_PHY_PCS_ANA_SOFT_RELEASE);
	if (ret < 0)
		return ret;
	/* wait uniphy calibration done */
	qcom_phy_pcs_calibration(phydev, addr_offset);
	/* GMII interface clock enable */
	ret = qca8k_port_clk_en_set(&phydev->mdio, phy_clk_index,
		ethphy_clk_mask, true);
	if (ret < 0)
		return ret;
	ret = qca8k_port_clk_en_set(&phydev->mdio, pcs_clk_index,
		MHT_CLK_TYPE_UNIPHY, true);
	if (ret < 0)
		return ret;
	qcom_phy_pcs_sgmii_function_reset(phydev, addr_offset);
	qcom_phy_pcs_ipg_tune_reset(phydev, addr_offset);

	return ret;
}

int qcom_phy_sgmii_interface_fix_up(struct phy_device *phydev)
{
	u32 interface_old;
	struct qcom_phy_pcs_cfg config = {0};

	interface_old = phydev->interface;
	if (phydev->link && phydev->speed == SPEED_2500)
		phydev->interface = PHY_INTERFACE_MODE_2500BASEX;
	else
		phydev->interface = PHY_INTERFACE_MODE_SGMII;

	if (phydev->interface == interface_old)
		return 0;
	config.addr_offset = PCS0_ADDR_OFFSET;
	config.type = phydev->interface;
	config.clock_mode = CLOCK_PHY_MODE;
	config.auto_neg = true;

	return qcom_phy_pcs_sgmii_mode_set(phydev, config);
}

int qcom_phy_pcs_speed_clock_set(struct phy_device *phydev,
	u32 channel, u32 speed)
{
	u32 clk_rate = 0;

	switch(speed)
	{
		case SPEED_2500:
			clk_rate = 312500000;
			break;
		case SPEED_1000:
			clk_rate = 125000000;
			break;
		case SPEED_100:
			clk_rate = 25000000;
			break;
		case SPEED_10:
			clk_rate = 2500000;
			break;
		default:
			return -EOPNOTSUPP;
	}

	return qca8k_port_clk_rate_set(&phydev->mdio, channel, clk_rate);
}
EXPORT_SYMBOL(qcom_phy_pcs_speed_clock_set);

int qcom_phy_pcs_interface_set(struct phy_device *phydev,
	struct qcom_phy_pcs_cfg config)
{
	int ret = 0;

	switch (config.type) {
	case PHY_INTERFACE_MODE_QUSGMII:
		ret = qcom_phy_pcs_qusgmii_mode_set(phydev, config);
		break;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_2500BASEX:
		ret = qcom_phy_pcs_sgmii_mode_set(phydev, config);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return ret;
}
EXPORT_SYMBOL(qcom_phy_pcs_interface_set);

bool qcom_phy_pcs_mode_check(struct phy_device *phydev,
	u32 offset, u32 pcs_mode)
{
	int ret = 0;

	ret = qcom_phy_pcs_read_mmd(phydev, offset,
		MDIO_MMD_PMAPMD, QCOM_PHY_PCS_MMD1_MODE_CTRL);
	if(ret < 0)
		return false;

	if(!(pcs_mode & ret))
		return false;

	return true;
}
EXPORT_SYMBOL(qcom_phy_pcs_mode_check);
