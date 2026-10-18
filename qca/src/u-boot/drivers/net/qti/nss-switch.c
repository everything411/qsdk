/*
 * Copyright (c) 2016-2019, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "nss-switch.h"
#ifdef CONFIG_PHY_AQUANTIA
#include <command.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#define REG_DELAY			1
#define RESET_DELAY			10

static int tftp_acl_our_port;

uchar ipq_def_enetaddr[6] = {0x00, 0x03, 0x7F, 0xBA, 0xDB, 0xAD};
int mac_speed_config [] = {10, 100, 1000, 10000, 2500, 5000};

#ifdef CONFIG_MDIO_QCOM_I2C
extern struct mii_dev *qcom_mdio_i2c_alloc(struct udevice *i2c_bus,
							int phy_addr);
#endif /* CONFIG_MDIO_QCOM_I2C */
extern int get_eth_mac_address(uchar *enetaddr, int no_of_macs);
extern int ipq_aquantia_load_fw(struct phy_device *phydev);

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
extern int initr_net(void);
#endif

#ifdef CONFIG_ETH_LOW_MEM
#define mem_init()			;
#define mem_alloc(size, align)		malloc_cache_aligned(size)
#else

static unsigned long nc_end;
static unsigned long nc_next;

/*
 * Non-Cached Memory APIs
 */
static int mem_init(void)
{
	unsigned long nc_start = NONCACHED_MEM_REGION_ADDR;
	nc_end = nc_start + NONCACHED_MEM_REGION_SIZE;
	nc_next = nc_start;

	mmu_set_region_dcache_behaviour(nc_start, nc_end - nc_start,
					DCACHE_OFF);
	return 0;
}

static phys_addr_t mem_alloc(size_t size, size_t align)
{
	phys_addr_t next = ALIGN(nc_next, align);

	if (next >= nc_end || (nc_end - next) < size)
		return 0;

	nc_next = next + size;
	return next;
}
#endif

/*
 * Uniphy configuration
 */
void csr1_write(int phy_id, int addr, int  value)
{
	uintptr_t addr_h, addr_l, ahb_h, ahb_l,  phy;
	phy=phy_id<<(0x10);
	addr_h=(addr&0xffffff)>>8;
	addr_l=((addr&0xff)<<2)|(0x20<<(0xa));
	ahb_l=(addr_l&0xffff)|(0x7A00000|phy);
	ahb_h=(0x7A083FC|phy);
	writel(addr_h,ahb_h);
	writel(value,ahb_l);
}

int  csr1_read(int phy_id, int  addr )
{
	uintptr_t addr_h ,addr_l,ahb_h, ahb_l, phy;
	phy=phy_id<<(0x10);
	addr_h=(addr&0xffffff)>>8;
	addr_l=((addr&0xff)<<2)|(0x20<<(0xa));
	ahb_l=(addr_l&0xffff)|(0x7A00000|phy);
	ahb_h=(0x7A083FC|phy);
	writel(addr_h, ahb_h);
	return  readl(ahb_l);
}

static int ppe_uniphy_calibration(struct port_info *port)
{
	int retries = 100, calibration_done = 0;
	uint32_t reg_value = 0;
	uintptr_t reg = port->uniphy_base + PPE_UNIPHY_OFFSET_CALIB_4;

	while(calibration_done != UNIPHY_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0) {
			printf("uniphy callibration time out!\n");
			return -1;
		}
		reg_value = readl(reg);
		calibration_done = (reg_value >> 0x7) & 0x1;
	}

	return 0;
}

void ipq_port_reset(struct reset_ctl *rst, bool set)
{
	if (set)
		reset_assert(rst);
	else
		reset_deassert(rst);
}

static void ppe_uniphy_reset(struct port_info *port, bool issoft, bool set)
{
	struct udevice *dev = port->dev;
	struct reset_ctl rst;
	int ret;
	char name[64];

	snprintf(name, sizeof(name), "uniphy%d_%s", port->uniphy_id,
			(issoft)? "srst" : "xrst");

	ret = reset_get_by_name(dev, name, &rst);

	if(!ret)
		ipq_port_reset(&rst, set);

	if(issoft) {
		snprintf(name, sizeof(name), "uniphy_port%d_tx", port->id);

		ret = reset_get_by_name(dev, name, &rst);

		if(!ret)
			ipq_port_reset(&rst, set);

		snprintf(name, sizeof(name), "uniphy_port%d_rx", port->id);

		ret = reset_get_by_name(dev, name, &rst);

		if(!ret)
			ipq_port_reset(&rst, set);
	}
}

static void ppe_uniphy_psgmii_mode_set(struct port_info *port)
{
	ppe_uniphy_reset(port, false, true);

	writel(0x220, port->uniphy_base + PPE_UNIPHY_MODE_CONTROL);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);
}

static void ppe_uniphy_qsgmii_mode_set(struct port_info *port)
{
	writel(0x120, port->uniphy_base + PPE_UNIPHY_MODE_CONTROL);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);
}

void ppe_uniphy_set_forceMode(struct port_info *port)
{
	uint32_t reg_value;

	reg_value = readl(port->uniphy_base +
				UNIPHY_DEC_CHANNEL_0_INPUT_OUTPUT_4);

	reg_value |= UNIPHY_FORCE_SPEED_25M;

	writel(reg_value, port->uniphy_base +
				UNIPHY_DEC_CHANNEL_0_INPUT_OUTPUT_4);
}

void ppe_uniphy_refclk_set_25M(struct port_info *port)
{
	uint32_t reg_value;

	reg_value = readl(port->uniphy_base + UNIPHY1_CLKOUT_50M_CTRL_OPTION);

	reg_value |= (UNIPHY1_CLKOUT_50M_CTRL_CLK50M_DIV2_SEL |
				UNIPHY1_CLKOUT_50M_CTRL_50M_25M_EN);

	writel(reg_value, port->uniphy_base + UNIPHY1_CLKOUT_50M_CTRL_OPTION);
}

static void ppe_uniphy_sgmii_mode_set(struct port_info *port)
{
	phys_addr_t base = port->uniphy_base;
	uint32_t reg_value;

	writel(UNIPHY_MISC_SRC_PHY_MODE, base +
			UNIPHY_MISC_SOURCE_SELECTION_REG_OFFSET);

	if (port->uniphy_mode == PORT_WRAPPER_SGMII_PLUS) {
		reg_value = UNIPHY_MISC2_REG_SGMII_PLUS_MODE;
	} else {
		reg_value = UNIPHY_MISC2_REG_SGMII_MODE;
	}

	writel(reg_value, base + UNIPHY_MISC2_REG_OFFSET);

	writel(UNIPHY_PLL_RESET_REG_VALUE, base + UNIPHY_PLL_RESET_REG_OFFSET);

	mdelay(REG_DELAY);

	writel(UNIPHY_PLL_RESET_REG_DEFAULT_VALUE,
			base + UNIPHY_PLL_RESET_REG_OFFSET);

	mdelay(REG_DELAY);

	switch (port->uniphy_mode) {
		case PORT_WRAPPER_SGMII_FIBER:
			writel(0x400, base + PPE_UNIPHY_MODE_CONTROL);
			break;
		case PORT_WRAPPER_SGMII0_RGMII4:
		case PORT_WRAPPER_SGMII1_RGMII4:
		case PORT_WRAPPER_SGMII4_RGMII4:
			writel(0x420, base + PPE_UNIPHY_MODE_CONTROL);
			break;

		case PORT_WRAPPER_SGMII_PLUS:
			writel(0x820, base + PPE_UNIPHY_MODE_CONTROL);
			break;
		default:
			printf("SGMII Config. wrongly");
			break;
	}

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);
}

static int ppe_uniphy_10g_r_linkup(uint32_t uniphy_index)
{
	uint32_t reg_value = 0;
	uint32_t retries = 100, linkup = 0;

	while (linkup != UNIPHY_10GR_LINKUP) {
		mdelay(1);
		if (retries-- == 0)
			return -1;
		reg_value = csr1_read(uniphy_index, SR_XS_PCS_KR_STS1_ADDRESS);
		linkup = (reg_value >> 12) & UNIPHY_10GR_LINKUP;
	}
	mdelay(REG_DELAY);
	return 0;
}

static void ppe_uniphy_10g_r_mode_set(struct port_info *port)
{
	ppe_uniphy_reset(port, false, true);

	writel(0x1021, port->uniphy_base + PPE_UNIPHY_MODE_CONTROL);

	writel(0x1C0, port->uniphy_base + UNIPHY_INSTANCE_LINK_DETECT);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);

	ppe_uniphy_reset(port, false, false);
}

static void ppe_uniphy_usxgmii_mode_set(struct port_info *port)
{
	uint32_t index = port->uniphy_id;
	phys_addr_t base = port->uniphy_base;
	uint32_t reg_value;

	writel(UNIPHY_MISC2_REG_VALUE, base + UNIPHY_MISC2_REG_OFFSET);

	writel(UNIPHY_PLL_RESET_REG_VALUE, base + UNIPHY_PLL_RESET_REG_OFFSET);

	mdelay(REG_DELAY);

	writel(UNIPHY_PLL_RESET_REG_DEFAULT_VALUE,
			base + UNIPHY_PLL_RESET_REG_OFFSET);

	mdelay(REG_DELAY);

	ppe_uniphy_reset(port, false, true);

	mdelay(RESET_DELAY);

	writel(0x1021, base + PPE_UNIPHY_MODE_CONTROL);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);

	ppe_uniphy_reset(port, false, false);

	mdelay(RESET_DELAY);

	ppe_uniphy_10g_r_linkup(index);
	reg_value = csr1_read(index, VR_XS_PCS_DIG_CTRL1_ADDRESS);
	reg_value |= USXG_EN;
	csr1_write(index, VR_XS_PCS_DIG_CTRL1_ADDRESS, reg_value);
	reg_value = csr1_read(index, VR_MII_AN_CTRL_ADDRESS);
	reg_value |= MII_AN_INTR_EN;
	reg_value |= MII_CTRL;
	csr1_write(index, VR_MII_AN_CTRL_ADDRESS, reg_value);
	reg_value = csr1_read(index, SR_MII_CTRL_ADDRESS);
	reg_value |= AN_ENABLE;
	reg_value &= ~SS5;
	reg_value |= SS6 | SS13 | DUPLEX_MODE;
	csr1_write(index, SR_MII_CTRL_ADDRESS, reg_value);
}

static void ppe_uniphy_uqxgmii_mode_set(struct port_info *port)
{
	uint32_t index = port->uniphy_id;
	phys_addr_t base = port->uniphy_base;
	uint32_t reg_value = 0;

	writel(UNIPHY_MISC2_REG_VALUE, base + UNIPHY_MISC2_REG_OFFSET);

	writel(UNIPHY_PLL_RESET_REG_VALUE, base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(REG_DELAY);

	writel(UNIPHY_PLL_RESET_REG_DEFAULT_VALUE,
			base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(REG_DELAY);

	ppe_uniphy_reset(port, false, true);
	mdelay(RESET_DELAY);

	writel(0x1021, base + PPE_UNIPHY_MODE_CONTROL);

	reg_value = readl(base + UNIPHYQP_USXG_OPITON1);
	reg_value |= GMII_SRC_SEL;
	writel(reg_value, base + UNIPHYQP_USXG_OPITON1);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);

	ppe_uniphy_reset(port, false, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_10g_r_linkup(index);

	reg_value = csr1_read(index, VR_XS_PCS_DIG_CTRL1_ADDRESS);
	reg_value |= USXG_EN;
	csr1_write(index, VR_XS_PCS_DIG_CTRL1_ADDRESS, reg_value);

	/* set QXGMII mode */
	reg_value = csr1_read(index, VR_XS_PCS_KR_CTRL_ADDRESS);
	reg_value |= USXG_MODE;
	csr1_write(index, VR_XS_PCS_KR_CTRL_ADDRESS, reg_value);

	/* set AM interval mode */
	reg_value = csr1_read(index, VR_XS_PCS_DIG_STS_ADDRESS);
	reg_value |= AM_COUNT;
	csr1_write(index, VR_XS_PCS_DIG_STS_ADDRESS, reg_value);

	reg_value = csr1_read(index, VR_XS_PCS_DIG_CTRL1_ADDRESS);
	reg_value |= VR_RST;
	csr1_write(index, VR_XS_PCS_DIG_CTRL1_ADDRESS, reg_value);

	reg_value = csr1_read(index, VR_MII_AN_CTRL_ADDRESS);
	reg_value |= MII_AN_INTR_EN;
	reg_value |= MII_CTRL;
	csr1_write(index, VR_MII_AN_CTRL_ADDRESS, reg_value);
	csr1_write(index, VR_MII_AN_CTRL_CHANNEL1_ADDRESS, reg_value);
	csr1_write(index, VR_MII_AN_CTRL_CHANNEL2_ADDRESS, reg_value);
	csr1_write(index, VR_MII_AN_CTRL_CHANNEL3_ADDRESS, reg_value);

	/* disable TICD */
	reg_value = csr1_read(index, VR_XAUI_MODE_CTRL_ADDRESS);
	reg_value |= IPG_CHECK;
	csr1_write(index, VR_XAUI_MODE_CTRL_ADDRESS, reg_value);
	csr1_write(index, VR_XAUI_MODE_CTRL_CHANNEL1_ADDRESS, reg_value);
	csr1_write(index, VR_XAUI_MODE_CTRL_CHANNEL2_ADDRESS, reg_value);
	csr1_write(index, VR_XAUI_MODE_CTRL_CHANNEL3_ADDRESS, reg_value);

	/* enable uniphy autoneg ability and usxgmii 10g speed
	 * and full duplex
	 */
	reg_value = csr1_read(index, SR_MII_CTRL_ADDRESS);
	reg_value |= AN_ENABLE;
	reg_value &= ~SS5;
	reg_value |= SS6 | SS13 | DUPLEX_MODE;
	csr1_write(index, SR_MII_CTRL_ADDRESS, reg_value);
	csr1_write(index, SR_MII_CTRL_CHANNEL1_ADDRESS, reg_value);
	csr1_write(index, SR_MII_CTRL_CHANNEL2_ADDRESS, reg_value);
	csr1_write(index, SR_MII_CTRL_CHANNEL3_ADDRESS, reg_value);

	/* enable uniphy eee transparent mode and configure eee
	 * related timer value
	 */
	reg_value = csr1_read(index, VR_XS_PCS_EEE_MCTRL0_ADDRESS);
	reg_value |= SIGN_BIT | MULT_FACT_100NS;
	csr1_write(index, VR_XS_PCS_EEE_MCTRL0_ADDRESS, reg_value);

	reg_value = csr1_read(index, VR_XS_PCS_EEE_TXTIMER_ADDRESS);
	reg_value |= UNIPHY_XPCS_TSL_TIMER | UNIPHY_XPCS_TLU_TIMER
			| UNIPHY_XPCS_TWL_TIMER;
	csr1_write(index, VR_XS_PCS_EEE_TXTIMER_ADDRESS, reg_value);

	reg_value = csr1_read(index, VR_XS_PCS_EEE_RXTIMER_ADDRESS);
	reg_value |= UNIPHY_XPCS_100US_TIMER | UNIPHY_XPCS_TWR_TIMER;
	csr1_write(index, VR_XS_PCS_EEE_RXTIMER_ADDRESS, reg_value);

	/* Transparent LPI mode and LPI pattern enable */
	reg_value = csr1_read(index, VR_XS_PCS_EEE_MCTRL1_ADDRESS);
	reg_value |= TRN_LPI | TRN_RXLPI;
	csr1_write(index, VR_XS_PCS_EEE_MCTRL1_ADDRESS, reg_value);

	reg_value = csr1_read(index, VR_XS_PCS_EEE_MCTRL0_ADDRESS);
	reg_value |= LRX_EN | LTX_EN;
	csr1_write(index, VR_XS_PCS_EEE_MCTRL0_ADDRESS, reg_value);
}

void ppe_uniphy_mode_set(struct port_info *port)
{
	switch(port->uniphy_mode) {
		case PORT_WRAPPER_PSGMII:
			ppe_uniphy_psgmii_mode_set(port);
			break;
		case PORT_WRAPPER_QSGMII:
			ppe_uniphy_qsgmii_mode_set(port);
			break;
		case PORT_WRAPPER_SGMII0_RGMII4:
		case PORT_WRAPPER_SGMII1_RGMII4:
		case PORT_WRAPPER_SGMII4_RGMII4:
		case PORT_WRAPPER_SGMII_PLUS:
		case PORT_WRAPPER_SGMII_FIBER:
			ppe_uniphy_sgmii_mode_set(port);
			break;
		case PORT_WRAPPER_USXGMII:
			ppe_uniphy_usxgmii_mode_set(port);
			break;
		case PORT_WRAPPER_10GBASE_R:
			ppe_uniphy_10g_r_mode_set(port);
			break;
		case PORT_WRAPPER_UQXGMII:
			ppe_uniphy_uqxgmii_mode_set(port);
			break;
		default:
			break;
	}
}

void ppe_uniphy_usxgmii_autoneg_completed(int uniphy_index)
{
	uint32_t autoneg_complete = 0, retries = 100;
	uint32_t reg_value = 0;

	while (autoneg_complete != 0x1) {
		mdelay(1);
		if (retries-- == 0)
		{
			return;
		}
		reg_value = csr1_read(uniphy_index, VR_MII_AN_INTR_STS);
		autoneg_complete = reg_value & 0x1;
	}
	reg_value &= ~CL37_ANCMPLT_INTR;
	csr1_write(uniphy_index, VR_MII_AN_INTR_STS, reg_value);
}

void ppe_uniphy_usxgmii_speed_set(int portid, int uniphy_index, int speed)
{
	uint32_t reg_value = 0;
	uint32_t mii_ctrl_aadress = SR_MII_CTRL_ADDRESS;

	if ((uniphy_index == 0) && (portid != 1)) {
		switch(portid) {
		case 2:
			mii_ctrl_aadress = SR_MII_CTRL_CHANNEL1_ADDRESS;
			break;
		case 3:
			mii_ctrl_aadress = SR_MII_CTRL_CHANNEL2_ADDRESS;
			break;
		case 4:
			mii_ctrl_aadress = SR_MII_CTRL_CHANNEL3_ADDRESS;
			break;
		default:
			;
		}
	}

	reg_value = csr1_read(uniphy_index, mii_ctrl_aadress);
	reg_value |= DUPLEX_MODE;

	switch(speed) {
	case 0:
		reg_value &=~SS5;
		reg_value &=~SS6;
		reg_value &=~SS13;
		break;
	case 1:
		reg_value &=~SS5;
		reg_value &=~SS6;
		reg_value |=SS13;
		break;
	case 2:
		reg_value &=~SS5;
		reg_value |=SS6;
		reg_value &=~SS13;
		break;
	case 3:
		reg_value &=~SS5;
		reg_value |=SS6;
		reg_value |=SS13;
		break;
	case 4:
		reg_value |=SS5;
		reg_value &=~SS6;
		reg_value &=~SS13;
		break;
	case 5:
		reg_value |=SS5;
		reg_value &=~SS6;
		reg_value |=SS13;
		break;
	}

	csr1_write(uniphy_index, mii_ctrl_aadress, reg_value);
}

void ppe_uniphy_usxgmii_duplex_set(int uniphy_index, int duplex)
{
	uint32_t reg_value = 0;

	reg_value = csr1_read(uniphy_index, SR_MII_CTRL_ADDRESS);

	if (duplex & 0x1)
		reg_value |= DUPLEX_MODE;
	else
		reg_value &= ~DUPLEX_MODE;

	csr1_write(uniphy_index, SR_MII_CTRL_ADDRESS, reg_value);
}

void ppe_uniphy_usxgmii_port_reset(int uniphy_index)
{
	uint32_t reg_value = 0;

	reg_value = csr1_read(uniphy_index, VR_XS_PCS_DIG_CTRL1_ADDRESS);
	reg_value |= USRA_RST;
	csr1_write(uniphy_index, VR_XS_PCS_DIG_CTRL1_ADDRESS, reg_value);
}
/*
 * PPE configuration
 */

void ppe_xgmac_configuration(phys_addr_t reg_base, uint32_t portid,
				uint32_t speed, bool uxsgmii)
{
	uint32_t reg_value, gmacid = portid - 1;

	uintptr_t base = reg_base + PPE_SWITCH_NSS_SWITCH_XGMAC0 +
			 (gmacid * NSS_SWITCH_XGMAC_MAC_TX_CONFIGURATION);

	reg_value = readl(base);
	/*
	 * Jabber Disable.
	 */
	reg_value |=JD;
	/*
	 * Transmitter Enable.
	 */
	reg_value |=TE;
	/*
	 * Speed set
	 */
	switch(speed) {
	case 0:
	case 1:
	case 2:
		reg_value |=SS(XGMAC_SPEED_SELECT_1000M);
		break;
	case 3:
		reg_value |=SS(XGMAC_SPEED_SELECT_10000M);
		break;
	case 4:
		reg_value |=SS(XGMAC_SPEED_SELECT_2500M);
		break;
	case 5:
		reg_value |=SS(XGMAC_SPEED_SELECT_5000M);
		break;
	default:
		/* Not supported in 10G_R mode*/
		;
	}

	if (uxsgmii && (speed > 2))
		reg_value |=USS;
	else
		reg_value &=~USS;

	writel(reg_value, base);
	mdelay(1);
	/*
	 * Rx configuration
	 */
	reg_value = readl(base + MAC_RX_CONFIGURATION_ADDRESS);
	reg_value |= 0x300000c0;
	reg_value |=RE;
	reg_value |=ACS;
	reg_value |=CST;
	writel(reg_value, base + MAC_RX_CONFIGURATION_ADDRESS);
	mdelay(1);
	/*
	 * set up mac filter
	 */
	writel(0x80000081, base + MAC_PACKET_FILTER_ADDRESS);
}
/*
 * ppe_port_bridge_txmac_set()
 * TXMAC should be disabled for all ports by default
 * TXMAC should be enabled for all ports that are link up alone
 */
void ppe_port_bridge_txmac_set(phys_addr_t reg_base, uint32_t port,
	bool isenable)
{
	uint32_t reg_value = readl(reg_base + IPE_L2_BASE_ADDR +
				PORT_BRIDGE_CTRL_ADDRESS +
				(port * PORT_BRIDGE_CTRL_INC));
	if (isenable)
		reg_value |= TX_MAC_EN;
	else
		reg_value &= ~TX_MAC_EN;

	writel(reg_value, reg_base + IPE_L2_BASE_ADDR +
			PORT_BRIDGE_CTRL_ADDRESS +
			(port * PORT_BRIDGE_CTRL_INC));
}

uint8_t phy_status_get_from_ppe(phys_addr_t reg_base, uint32_t port_id)
{
	uint32_t reg_field = readl((reg_base + ((port_id > 4)?
				PORT_PHY_STATUS_ADDRESS1 :
				PORT_PHY_STATUS_ADDRESS)));

	switch(port_id) {
	case 2:
		reg_field >>= PORT_PHY_STATUS_PORT2_OFFSET;
		break;
	case 3:
		reg_field >>= PORT_PHY_STATUS_PORT3_OFFSET;
		break;
	case 4:
		reg_field >>= PORT_PHY_STATUS_PORT4_OFFSET;
		break;
	case 5:
		reg_field >>= PORT_PHY_STATUS_PORT5_1_OFFSET;
		break;
	case 6:
		reg_field >>= PORT_PHY_STATUS_PORT6_OFFSET;
		break;
	default:
		/* case port 1 */
		;
	}

	return (uint8_t) reg_field;
}

static void ppe_port_mux_set(phys_addr_t reg_base, struct port_info *port)
{
	union port_mux_ctrl_u port_mux_ctrl;
	uint8_t id = port->id;
	uint8_t uniphy_type = port->uniphy_type;
	uint8_t mac_type = port->gmac_type;

	pr_debug("port id is: %d, mac_type is %d, uniphy_type is %d\n",
		id, mac_type, uniphy_type);

	port_mux_ctrl.val = 0;

	port_mux_ctrl.val = readl(reg_base + PORT_MUX_CTRL);

	pr_debug("\nBEFORE UPDATE: Port MUX CTRL value is %u",
			port_mux_ctrl.val);

	switch (id) {
		case 1:
			port_mux_ctrl.bf.port1_mac_sel = mac_type;
			port_mux_ctrl.bf.port1_pcs_sel = uniphy_type;
			break;
		case 2:
			port_mux_ctrl.bf.port2_mac_sel = mac_type;
			port_mux_ctrl.bf.port2_pcs_sel = uniphy_type;
			break;
		case 3:
			port_mux_ctrl.bf.port3_mac_sel = mac_type;
			port_mux_ctrl.bf.port3_pcs_sel = uniphy_type;
			break;
		case 4:
			port_mux_ctrl.bf.port4_mac_sel = mac_type;
			port_mux_ctrl.bf.port4_pcs_sel = uniphy_type;
			break;
		case 5:
			port_mux_ctrl.bf.port5_mac_sel = mac_type;
			port_mux_ctrl.bf.port5_pcs_sel = uniphy_type;
			break;
		case 6:
			port_mux_ctrl.bf.port6_mac_sel = mac_type;
			port_mux_ctrl.bf.port6_pcs_sel = uniphy_type;
			break;
		default:
			break;
	}

	writel(port_mux_ctrl.val, (reg_base + PORT_MUX_CTRL));
}

void ppe_port_speed_set(phys_addr_t reg_base, struct port_info *port)
{
	bool usxgmii = false;
	uint32_t gmacid, speed = -1;
	uintptr_t base;

	ppe_port_bridge_txmac_set(reg_base, port->id, false);

	if (port->cur_gmac_type != port->gmac_type) {
		ppe_port_mux_set(reg_base, port);
		port->cur_gmac_type = port->gmac_type;
	}

	switch(port->uniphy_mode) {
	case PORT_WRAPPER_10GBASE_R:
		break;
	case PORT_WRAPPER_UQXGMII:
	case PORT_WRAPPER_USXGMII:
		ppe_uniphy_usxgmii_autoneg_completed(port->uniphy_id);
		ppe_uniphy_usxgmii_speed_set(port->id, port->uniphy_id,
						port->mac_speed);
		ppe_uniphy_usxgmii_duplex_set(port->uniphy_id, port->duplex);
		ppe_uniphy_usxgmii_port_reset(port->uniphy_id);
		usxgmii = true;
	case PORT_WRAPPER_SGMII_PLUS:
		if (port->gmac_type == XGMAC)
			speed = port->mac_speed;
		break;
	case PORT_WRAPPER_SGMII0_RGMII4:
	case PORT_WRAPPER_PSGMII:
		break;
	case PORT_WRAPPER_EMULATION:
		usxgmii = true;
		speed = port->mac_speed;
		break;
	default:
		;
	}

	ppe_port_bridge_txmac_set(reg_base, port->id, true);

	if (port->gmac_type == XGMAC) {
		ppe_xgmac_configuration(reg_base, port->id, speed, usxgmii);
	} else {
		gmacid = port->id - 1;
		base = reg_base + PPE_MAC_ENABLE + (0x200 * gmacid);

		writel(0x73, base);

		writel(port->mac_speed, base + PPE_MAC_SPEED_OFF);

		writel(0x1, base + PPE_MAC_MIB_CTL_OFF);
	}
}

/*
 * ipq_ppe_enable_port_counter
 */
static void ipq_ppe_enable_port_counter(phys_addr_t reg_base)
{
	uint32_t i;
	uint32_t reg = 0;

	for (i = 0; i < 7; i++) {
		/* MRU_MTU_CTRL_TBL.rx_cnt_en, MRU_MTU_CTRL_TBL.tx_cnt_en */
		reg = readl(reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10));
		writel(reg, reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10));
		reg = readl(reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10) + 0x4);
		writel(reg | 0x284303,
			reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR +
			(i * 0x10) + 0x4);
		reg = readl(reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10) + 0x8);
		writel(reg, reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10) + 0x8);
		reg = readl(reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10) + 0xc);
		writel(reg, reg_base + PPE_MRU_MTU_CTRL_TBL_ADDR
					+ (i * 0x10) + 0xc);

		/* MC_MTU_CTRL_TBL.tx_cnt_en */
		reg = readl(reg_base + PPE_MC_MTU_CTRL_TBL_ADDR
					+ (i * 0x4));
		writel(reg | 0x10000,
			reg_base + PPE_MC_MTU_CTRL_TBL_ADDR +
			(i * 0x4));

		/* PORT_EG_VLAN.tx_counting_en */
		reg = readl(reg_base + PPE_PORT_EG_VLAN_TBL_ADDR
					+ (i * 0x4));
		writel(reg | 0x100,
			reg_base + PPE_PORT_EG_VLAN_TBL_ADDR +
			(i * 0x4));

		/* TL_PORT_VP_TBL.rx_cnt_en */
		reg = readl(reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10));
		writel(reg, reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10));
		reg = readl(reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10) + 0x4);
		writel(reg, reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10) + 0x4);
		reg = readl(reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10) + 0x8);
		writel(reg | 0x20000,
			reg_base + PPE_TL_PORT_VP_TBL_ADDR +
			(i * 0x10) + 0x8);
		reg = readl(reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10) + 0xc);
		writel(reg, reg_base + PPE_TL_PORT_VP_TBL_ADDR
					+ (i * 0x10) + 0xc);
	}
}

/*
 * ipq_vsi_setup()
 */
static void ipq_vsi_setup(phys_addr_t reg_base, uint32_t vsi,
		uint8_t group_mask)
{
	uint32_t val = (group_mask << 24 | group_mask << 16 |
				group_mask << 8 | group_mask);

	/* Set mask */
	writel(val, reg_base + 0x063800 + (vsi * 0x10));

	/*  new addr lrn en | station move lrn en */
	writel(0x9, reg_base + 0x063804 + (vsi * 0x10));
}

void ipq_ppe_tdm_configuration(struct ppe_info *ppe)
{
	uint32_t i;
	uint8_t *config_values = &tdm_config[ppe->tdm_mode].val[0];

	for (i = 0; i < ppe->no_reg; ++i) {
		writel(config_values[i],
			ppe->base + ppe->tdm_offset + (i * 0x10));
	}

	writel(ppe->tdm_ctrl_val, ppe->base + 0xb000);

	if (ppe->tm) {
		writel(0x20,(void *)0x3a47a000);
		writel(0x12,(void *)0x3a47a010);
		writel(0x1 ,(void *)0x3a47a020);
		writel(0x2 ,(void *)0x3a47a030);
		writel(0x10,(void *)0x3a47a040);
		writel(0x21,(void *)0x3a47a050);
		writel(0x2 ,(void *)0x3a47a060);
		writel(0x10,(void *)0x3a47a070);
		writel(0x12,(void *)0x3a47a080);
		writel(0x1 ,(void *)0x3a47a090);
		writel(0xa ,(void *)0x3a400000);
		writel(0x303,(void *)0x3a026100);
		writel(0x303,(void *)0x3a026104);
		writel(0x303,(void *)0x3a026108);
	}
}

void ppe_ipo_rule_reg_set(phys_addr_t reg_base, union ipo_rule_reg_u *hw_reg,
		uint32_t rule_id)
{
	uint32_t i;

	for (i = 0; i < 3; i++) {
		writel(hw_reg->val[i], reg_base + IPO_CSR_BASE_ADDR +
			IPO_RULE_REG_ADDRESS + (rule_id * IPO_RULE_REG_INC) +
			(i * 4));
	}
}

void ppe_ipo_mask_reg_set(phys_addr_t reg_base, union ipo_mask_reg_u *hw_mask,
		uint32_t rule_id)
{
	uint32_t i;

	for (i = 0; i < 2; i++) {
		writel(hw_mask->val[i], reg_base + (IPO_CSR_BASE_ADDR +
			IPO_MASK_REG_ADDRESS + (rule_id * IPO_MASK_REG_INC) +
			(i * 4)));
	}
}

void ppe_ipo_action_set(phys_addr_t reg_base, union ipo_action_u *hw_act,
		uint32_t rule_id, uint32_t ipo_cnt)
{
	uint32_t i;

	for (i = 0; i < ipo_cnt; i++) {
		writel(hw_act->val[i], reg_base + (IPE_L2_BASE_ADDR +
			IPO_ACTION_ADDRESS + (rule_id * IPO_ACTION_INC) +
			(i * 4)));
	}
}

void ipq_ppe_acl_set(struct ppe_acl_set * acl_set)
{
	union ipo_rule_reg_u hw_reg = {0};
	union ipo_mask_reg_u hw_mask = {0};
	union ipo_action_u hw_act = {0};

	memset(&hw_reg, 0, sizeof(hw_reg));
	memset(&hw_mask, 0, sizeof(hw_mask));
	memset(&hw_act, 0, sizeof(hw_act));

	if (acl_set->rule_id < MAX_RULE) {
		hw_act.bf.dest_info_change_en = 1;
		hw_mask.bf.maskfield_0 = acl_set->mask;
		hw_reg.bf.rule_type = acl_set->rule_type;
		if (acl_set->rule_type == ADPT_ACL_HPPE_IPV4_DIP_RULE) {
			hw_reg.bf.rule_field_0 = acl_set->field1;
			hw_reg.bf.rule_field_1 = acl_set->field0 << 17;
			hw_mask.bf.maskfield_1 = 7<<17;
			if (acl_set->permit == 0x0) {
				hw_act.bf.fwd_cmd = 0;/* forward */
				hw_reg.bf.pri = 0x1;
			}
			if (acl_set->deny == 0x1) {
				hw_act.bf.fwd_cmd = 1;/* drop */
				hw_reg.bf.pri = 0x0;
			}
		} else if (acl_set->rule_type == ADPT_ACL_HPPE_MAC_SA_RULE) {
			/* src mac AC rule */
			hw_reg.bf.rule_field_0 = acl_set->field1;
			hw_reg.bf.rule_field_1 = acl_set->field0;
			hw_mask.bf.maskfield_1 = 0xffff;
			hw_act.bf.fwd_cmd = 1;/* drop */
			hw_reg.bf.pri = 0x2;
			/* bypass fdb lean and fdb freash */
			hw_act.bf.bypass_bitmap_0 = 0x1800;
		} else if (acl_set->rule_type == ADPT_ACL_HPPE_MAC_DA_RULE) {
			/* dest mac AC rule */
			hw_reg.bf.rule_field_0 = acl_set->field1;
			hw_reg.bf.rule_field_1 = acl_set->field0;
			hw_mask.bf.maskfield_1 = 0xffff;
			hw_act.bf.fwd_cmd = 1;/* drop */
			hw_reg.bf.pri = 0x2;
		}
		/* bind port1-port6 */
		hw_reg.bf.src_0 = 0x0;
		hw_reg.bf.src_1 = 0x3F;
		ppe_ipo_rule_reg_set(acl_set->reg_base, &hw_reg,
				acl_set->rule_id);
		ppe_ipo_mask_reg_set(acl_set->reg_base, &hw_mask,
				acl_set->rule_id);
		ppe_ipo_action_set(acl_set->reg_base, &hw_act,
				acl_set->rule_id, acl_set->ipo_cnt);
	}
}

/*
 * ipq_ppe_vp_port_tbl_set()
 */
static void ipq_ppe_vp_port_tbl_set(phys_addr_t reg_base, uint32_t port,
		uint32_t vsi)
{
	u32 addr = PPE_L3_VP_PORT_TBL_ADDR +
		 (port * PPE_L3_VP_PORT_TBL_INC);
	writel(0x0, reg_base + addr);
	writel(1 << 9 | vsi << 10, reg_base + addr + 0x4);
	writel(0x0, reg_base + addr + 0x8);
	writel(0x0, reg_base + addr + 0xc);
}

void ipq_port_mac_clock_setclear(struct udevice *dev, struct port_info *port,
					bool set)
{
	struct reset_ctl rst;
	int ret;
	char name[64];

	snprintf(name, sizeof(name), "nss_cc_port%d_mac", port->id);

	ret = reset_get_by_name(dev, name, &rst);

	if(!ret)
		ipq_port_reset(&rst, set);

	snprintf(name, sizeof(name), "nss_cc_port%d_tx", port->id);

	ret = reset_get_by_name(dev, name, &rst);

	if(!ret)
		ipq_port_reset(&rst, set);


	snprintf(name, sizeof(name), "nss_cc_port%d_rx", port->id);

	ret = reset_get_by_name(dev, name, &rst);

	if(!ret)
		ipq_port_reset(&rst, set);
}
/*
 * ipq_port_mac_clock_reset()
 */
void ipq_port_mac_clock_reset(struct udevice *dev, struct port_info *port)
{
	ipq_port_mac_clock_setclear(dev, port, true);
	mdelay(10);
	ipq_port_mac_clock_setclear(dev, port, false);
	mdelay(10);
}
/*
 * ipq_ppe_provision_init()
 */
void ipq_ppe_provision_init(struct ppe_info *info)
{
	phys_addr_t reg_base = info->base;
	uint32_t queue, bridge_ctrl, val;
	int i, j, port;
	struct ppe_acl_set acl_set;

	/* tdm/sched configuration */
	ipq_ppe_tdm_configuration(info);

	if (info->bridge_mode)
		/* Add CPU port 0 to VSI 2 */
		ipq_ppe_vp_port_tbl_set(reg_base, 0, 2);

	for (i = 1, j = 2; i <= info->no_ports; ++i) {
		/* Add port 1 - 2 to VSI 2 */
		ipq_ppe_vp_port_tbl_set(reg_base, i, j);

		if (!info->bridge_mode)
			++j;
	}

	/* Unicast priority map */
	writel(0, reg_base + PPE_QM_UPM_TBL);

	/* Port0 - 8 unicast queue settings */
	for (port = 0; port < info->nos_iports; ++port) {
		if (port == 0)
			queue = 0;
		else
			queue = ((port * 0x10) + 0x70);
		/*
		 * ppe_ucast_queue_map_tbl_queue_id_set
		 */
		val = readl(reg_base + PPE_QM_UQM_TBL +
				(port * PPE_UCAST_QUEUE_MAP_TBL_INC));

		val |= queue << 4;

		writel(val, reg_base + PPE_QM_UQM_TBL +
			(port * PPE_UCAST_QUEUE_MAP_TBL_INC));

		/*
		 * ppe_flow_port_map_tbl_port_num_set
		 */
		writel(port, reg_base + PPE_L0_FLOW_PORT_MAP_TBL +
			queue * PPE_L0_FLOW_PORT_MAP_TBL_INC);

		writel(port, reg_base + PPE_L1_FLOW_PORT_MAP_TBL +
			port * PPE_L1_FLOW_PORT_MAP_TBL_INC);

		/*
		 * ppe_flow_map_tbl_set
		 */
		val = port | 0x401000; /* c_drr_wt = 1, e_drr_wt = 1 */

		writel(val, reg_base + PPE_L0_FLOW_MAP_TBL + queue *
				PPE_L0_FLOW_MAP_TBL_INC);

		val = port | 0x100400; /* c_drr_wt = 1, e_drr_wt = 1 */

		writel(val, reg_base + PPE_L1_FLOW_MAP_TBL + port *
				PPE_L1_FLOW_MAP_TBL_INC);

		/*
		 * ppe_c_sp_cfg_tbl_drr_id_set
		 */
		writel(port * 2, reg_base + PPE_L0_C_SP_CFG_TBL +
				(port * 0x80));
		writel(port * 2, reg_base + PPE_L1_C_SP_CFG_TBL +
				(port * 0x80));

		/*
		 * ppe_e_sp_cfg_tbl_drr_id_set
		 */
		writel(port * 2 + 1, reg_base + PPE_L0_E_SP_CFG_TBL +
				(port * 0x80));
		writel(port * 2 + 1, reg_base + PPE_L1_E_SP_CFG_TBL +
				(port * 0x80));
	}

	/* Port0 multicast queue */
	writel(0x00000000, reg_base + 0x409000);
	writel(0x00401000, reg_base + 0x403000);

	/* Port1 - 7 multicast queue */
	for (i = 1; i < info->nos_iports; i++) {
		writel(i, reg_base + 0x409100 + ((i - 1) * 0x40));
		writel(0x401000 | i, reg_base + 0x403100 + ((i - 1) * 0x40));
	}

	/* ac enable for queues - disable queue tail drop */
	/* ucast queue */
	for (i = 0; i < 256; i++) {
		writel(0x32120001, reg_base + PPE_UCAST_QUEUE_AC_EN_BASE_ADDR
			+ (i * 0x10));
		writel(0x0, reg_base + PPE_UCAST_QUEUE_AC_EN_BASE_ADDR
					+ (i * 0x10) + 0x4);
		writel(0x0, reg_base + PPE_UCAST_QUEUE_AC_EN_BASE_ADDR
					+ (i * 0x10) + 0x8);
		writel(0x48000, reg_base + PPE_UCAST_QUEUE_AC_EN_BASE_ADDR
					+ (i * 0x10) + 0xc);
	}

	/* mcast queue */
	for (i = 0; i < 44; i++) {
		writel(0x00fa0001, reg_base + PPE_MCAST_QUEUE_AC_EN_BASE_ADDR
					+ (i * 0x10));
		writel(0x0, reg_base + PPE_MCAST_QUEUE_AC_EN_BASE_ADDR
					+ (i * 0x10) + 0x4);
		writel(0x1200, reg_base + PPE_MCAST_QUEUE_AC_EN_BASE_ADDR
					+ (i * 0x10) + 0x8);
	}

	/* enable queue counter */
	writel(0x4, reg_base + 0x020044);

	/* assign the ac group 0 with buffer number */
	writel(0x0, reg_base + 0x84c000);
	writel(0x7D00, reg_base + 0x84c004);
	writel(0x0, reg_base + 0x84c008);
	writel(0x0, reg_base + 0x84c00c);

	/* enable physical/virtual port TX/RX counters for all ports (0-6) */
	ipq_ppe_enable_port_counter(reg_base);

	/*
         * Port0 - TX_EN is set by default, Port1 - LRN_EN is set
         * Port0 -> CPU Port
         * Port1-6 -> Ethernet Ports
         * Port7 -> EIP197
	 * IPQ5332 ==> 1-3 ports
	 * IPQ9574 ==> 1-8 ports
         */

	for (i = 0; i < info->nos_iports; i++) {
		bridge_ctrl = PPE_PORT_BRIDGE_CTRL_OFFSET;
		if (i == 0) {
			val = PPE_PORT_BRIDGE_CTRL_PROMISC_EN |
				PPE_PORT_BRIDGE_CTRL_TXMAC_EN |
				PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP |
				PPE_PORT_BRIDGE_CTRL_STATION_LRN_EN |
				PPE_PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN;
		} else if (i == 7) {
			val = PPE_PORT_BRIDGE_CTRL_PROMISC_EN |
				PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP |
				PPE_PORT_BRIDGE_CTRL_STATION_LRN_EN |
				PPE_PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN;
		} else {
			val = PPE_PORT_BRIDGE_CTRL_PROMISC_EN |
			      PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP;
		}
		writel(val, reg_base + bridge_ctrl + (i * 4));
	}

	/* Global learning */
	writel(0xc0, reg_base + 0x060038);

	if (info->bridge_mode) {
		ipq_vsi_setup(reg_base, 2, info->vsi);
	} else {
		for (i = 0,j = 2;
			i <= info->no_ports && i < CONFIG_ETH_MAX_MAC;
			++i, ++j)
			ipq_vsi_setup(reg_base, j, nb_vsi_config[i]);
	}
	/*
	 * STP
	 * For IPQ5332 ==> Port 0-3
	 * For IPQ9574 ==> Ports 0-7
	 */
	for (i = 0; i < info->nos_iports; i++)
		writel(0x3, reg_base + PPE_STP_BASE + (0x4 * i));

	UPDATE_ACL_SET(acl_set, reg_base, 0, ADPT_ACL_HPPE_IPV4_DIP_RULE,
			UDP_PKT, 67, 0xffff, 0, 0, info->ipo_action);
	/* Allowing DHCP packets */
	ipq_ppe_acl_set(&acl_set);

	UPDATE_ACL_SET(acl_set, reg_base, 1, ADPT_ACL_HPPE_IPV4_DIP_RULE,
			UDP_PKT, 68, 0xffff, 0, 0, info->ipo_action);

	ipq_ppe_acl_set(&acl_set);

	UPDATE_ACL_SET(acl_set, reg_base, 2, ADPT_ACL_HPPE_IPV4_DIP_RULE,
			UDP_PKT, 0, 0, 0, 1, info->ipo_action);

	/* Dropping all the UDP packets */
	ipq_ppe_acl_set(&acl_set);

	if (IS_ENABLED(CONFIG_TFTP_PORT)) {
		tftp_acl_our_port = 1024 + (get_timer(0) % 3072);

		UPDATE_ACL_SET(acl_set, reg_base, 3, 0x4, 0x1,
				tftp_acl_our_port, 0xffff, 0, 0,
				info->ipo_action);

		/* Allowing tftp packets */
		ipq_ppe_acl_set(&acl_set);
	}
}
/*
 * EDMA configuration
 */
/*
 * ipq_edma_alloc_rx_buffer()
 *	Alloc Rx buffers for one RxFill ring
 */
int ipq_edma_alloc_rx_buffer(struct ipq_edma_hw *ehw,
		struct ipq_edma_rxfill_ring *rxfill_ring)
{
	uint16_t num_alloc = 0;
	uint16_t cons, next, counter;
	struct ipq_edma_rxfill_desc *rxfill_desc;
	uint32_t reg_data;
	phys_addr_t reg_base = ehw->iobase;

	/*
	 * Read RXFILL ring producer index
	 */
	reg_data = readl(reg_base + EDMA_REG_RXFILL_PROD_IDX(
					rxfill_ring->id));

	next = reg_data & EDMA_RXFILL_PROD_IDX_MASK &
		(rxfill_ring->count - 1);

	/*
	 * Read RXFILL ring consumer index
	 */
	reg_data = readl(reg_base + EDMA_REG_RXFILL_CONS_IDX(
					rxfill_ring->id));

	cons = reg_data & EDMA_RXFILL_CONS_IDX_MASK;

	pr_debug("%s: prod_idx = %d cons_idx  = %d\n", __func__, next, cons);
	while (1) {
		counter = next;

		if (++counter == rxfill_ring->count)
			counter = 0;

		if (counter == cons) {
			pr_debug("%s: counter == cons (%u)\n", __func__, cons);
			break;
		}

		/*
		 * Get RXFILL descriptor
		 */
		rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, next);

		/*
		 * Fill the opaque value
		 */
		rxfill_desc->rdes2 = next;

		/*
		 * Save buffer size in RXFILL descriptor
		 */
		rxfill_desc->rdes1 |= cpu_to_le32((EDMA_RX_BUFF_SIZE <<
				       EDMA_RXFILL_BUF_SIZE_SHIFT) &
				       EDMA_RXFILL_BUF_SIZE_MASK);
		num_alloc++;
		next = counter;
	}

	if (num_alloc) {
		/*
		 * Update RXFILL ring producer index
		 */
		reg_data = next & EDMA_RXFILL_PROD_IDX_MASK;

		/*
		 * make sure the producer index updated before
		 * updating the hardware
		 */
		writel(reg_data, reg_base + EDMA_REG_RXFILL_PROD_IDX(
					rxfill_ring->id));

		pr_debug("%s: num_alloc = %d\n", __func__, num_alloc);
	}

	return num_alloc;
}

/*
 * ipq_edma_clean_tx()
 *	Reap Tx descriptors
 */
uint32_t ipq_edma_clean_tx(struct ipq_edma_hw *ehw,
			struct ipq_edma_txcmpl_ring *txcmpl_ring)
{
	struct ipq_edma_txcmpl_desc *txcmpl_desc;
	uint16_t prod_idx, cons_idx;
	uint32_t data;
	uint32_t txcmpl_consumed = 0;
	uchar *skb;
	phys_addr_t reg_base = ehw->iobase;

	/*
	 * Get TXCMPL ring producer index
	 */
	data = readl(reg_base + EDMA_REG_TXCMPL_PROD_IDX(
					txcmpl_ring->id));
	prod_idx = data & EDMA_TXCMPL_PROD_IDX_MASK;

	/*
	 * Get TXCMPL ring consumer index
	 */
	data = readl(reg_base + EDMA_REG_TXCMPL_CONS_IDX(
					txcmpl_ring->id));
	cons_idx = data & EDMA_TXCMPL_CONS_IDX_MASK;

	pr_debug("%s: prod_idx = %d cons_idx = %d\n",
			__func__, prod_idx, cons_idx);
	while (cons_idx != prod_idx) {

		txcmpl_desc = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);

		skb = (uchar *)((uintptr_t)
				(((uint64_t)(txcmpl_desc->tdes1 &
				EDMA_TXDESC_BUF_HI_ADD_MASK) << 32) |
				txcmpl_desc->tdes0));

		if (unlikely(!skb)) {
			printf("Invalid skb: cons_idx:%u prod_idx:%u\n",
				cons_idx, prod_idx);
		}

		if (++cons_idx == txcmpl_ring->count)
			cons_idx = 0;

		txcmpl_consumed++;
	}

	pr_debug("%s :%u txcmpl_consumed:%u prod_idx:%u cons_idx:%u\n",
		__func__, txcmpl_ring->id, txcmpl_consumed, prod_idx,
		cons_idx);

	if (txcmpl_consumed == 0)
		return 0;

	/*
	 * Update TXCMPL ring consumer index
	 */
	writel(cons_idx, reg_base + EDMA_REG_TXCMPL_CONS_IDX(
				txcmpl_ring->id));

	return txcmpl_consumed;
}
/*
 * ipq_edma_clean_rx()
 *	Reap Rx descriptors
 */
uint32_t ipq_edma_clean_rx(struct ipq_edma_hw *ehw,
				struct ipq_edma_rxdesc_ring *rxdesc_ring,
				void **buff)
{
	struct ipq_edma_rxdesc_desc *rxdesc_desc;
	uint16_t prod_idx, cons_idx;
	int src_port_num;
	int pkt_length = 0;
	u16 cleaned_count = 0;
	phys_addr_t reg_base = ehw->iobase;

	cons_idx = readl(reg_base +
			EDMA_REG_RXDESC_CONS_IDX(rxdesc_ring->id)) &
			EDMA_RXDESC_CONS_IDX_MASK;

	/*
	 * Read Rx ring producer index
	 */
	prod_idx = readl(reg_base +
		EDMA_REG_RXDESC_PROD_IDX(rxdesc_ring->id))
		& EDMA_RXDESC_PROD_IDX_MASK;

	pr_debug("%s: cons idx = %u, prod idx = %u\n",
			__func__, cons_idx, prod_idx);
	if (cons_idx == prod_idx) {
		pr_debug("%s: cons idx == prod idx (%u)\n",__func__, prod_idx);
		goto skip;
	}

	rxdesc_desc = EDMA_RXDESC_DESC(rxdesc_ring, cons_idx);


	/*
	 * Check src_info from Rx Descriptor
	 */
	src_port_num =
		EDMA_RXDESC_SRC_INFO_GET(rxdesc_desc->rdes4);
	if ((src_port_num & EDMA_RXDESC_SRCINFO_TYPE_MASK) ==
			EDMA_RXDESC_SRCINFO_TYPE_PORTID) {
		src_port_num &= EDMA_RXDESC_PORTNUM_BITS;
	} else {
		goto next_rx_desc;
	}
	/*
	 * Get packet length
	 */
	pkt_length = (rxdesc_desc->rdes5 &
		      EDMA_RXDESC_PKT_SIZE_MASK) >>
		      EDMA_RXDESC_PKT_SIZE_SHIFT;

	if (unlikely((src_port_num < ehw->start_ports)  ||
		(src_port_num > ehw->max_ports))) {
		goto next_rx_desc;
	}

	cleaned_count++;
	pr_debug("%s: src_port_num = %d pkt_length = %d cleaned_count = %d\n",
			__func__, src_port_num, pkt_length, cleaned_count);

	*buff = (void*)((uintptr_t)(((uint64_t)(rxdesc_desc->rdes1 &
				EDMA_TXDESC_BUF_HI_ADD_MASK) << 32)
				| rxdesc_desc->rdes0));

next_rx_desc:
	/*
	 * Update consumer index
	 */
	if (++cons_idx == rxdesc_ring->count)
		cons_idx = 0;

skip:

	if (cleaned_count) {
		writel(cons_idx, reg_base + EDMA_REG_RXDESC_CONS_IDX(
						rxdesc_ring->id));
	}

	return pkt_length;
}
/*
 * ipq_edma_rx_complete()
 */
static int ipq_edma_rx_complete(struct ipq_eth_dev *priv, void **buff)
{
	struct ipq_edma_hw *ehw = &priv->hw;
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
	struct ipq_edma_rxdesc_ring *rxdesc_ring;
	struct ipq_edma_rxfill_ring *rxfill_ring;
	uint32_t misc_intr_status, reg_data;
	int length = 0;
	int i;
	phys_addr_t reg_base = ehw->iobase;

	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		length = ipq_edma_clean_rx(ehw, rxdesc_ring, buff);
	}

	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		ipq_edma_clean_tx(ehw, txcmpl_ring);
	}

	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		ipq_edma_alloc_rx_buffer(ehw, rxfill_ring);
	}

	/*
	 * Enable RXDESC EDMA ring interrupt masks
	 */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		writel(ehw->rxdesc_intr_mask, reg_base +
			EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->id));
	}
	/*
	 * Enable TX EDMA ring interrupt masks
	 */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		writel(ehw->txcmpl_intr_mask,
			reg_base + EDMA_REG_TX_INT_MASK(txcmpl_ring->id));
	}
	/*
	 * Enable RXFILL EDMA ring interrupt masks
	 */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		writel(ehw->rxfill_intr_mask,
			reg_base + EDMA_REG_RXFILL_INT_MASK(
					rxfill_ring->id));
	}
	/*
	 * Read Misc intr status
	 */
	reg_data = readl(reg_base + EDMA_REG_MISC_INT_STAT);
	misc_intr_status = reg_data & ehw->misc_intr_mask;

	if (misc_intr_status != 0) {
		pr_info("%s: misc_intr_status = 0x%x\n", __func__,
			misc_intr_status);
		writel(EDMA_MASK_INT_DISABLE,
			reg_base + EDMA_REG_MISC_INT_MASK);
	}

	return length;
}

/*
 * ipq_edma_setup_ring_resources()
 *	Allocate/setup resources for EDMA rings
 */
static int ipq_edma_setup_ring_resources(struct ipq_edma_hw *ehw)
{
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
	struct ipq_edma_txdesc_ring *txdesc_ring;
	struct ipq_edma_rxfill_ring *rxfill_ring;
	struct ipq_edma_rxdesc_ring *rxdesc_ring;
	struct ipq_edma_txdesc_desc *txdesc_desc;
	struct ipq_edma_rxfill_desc *rxfill_desc;
	int i, j, index;
	void *tx_buf;
	void *rx_buf;

	/*
	 * Allocate Rx fill ring descriptors
	 */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		rxfill_ring->count = EDMA_RX_RING_SIZE;
		rxfill_ring->id = ehw->rxfill_ring_start + i;
		rxfill_ring->desc = (void *)mem_alloc(
				EDMA_RXFILL_DESC_SIZE *
				rxfill_ring->count,
				ARCH_DMA_MINALIGN);

		if (rxfill_ring->desc == NULL) {
			pr_info("%s: rxfill_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		rxfill_ring->dma = virt_to_phys(rxfill_ring->desc);
		pr_debug("rxfill ring id = %d, rxfill ring ptr = %p,"
			"rxfill ring dma = %u\n",
			rxfill_ring->id, rxfill_ring->desc, (unsigned int)
			rxfill_ring->dma);

		rx_buf = (void *)mem_alloc(EDMA_RX_BUFF_SIZE *
					rxfill_ring->count,
					ARCH_DMA_MINALIGN);

		if (rx_buf == NULL) {
			pr_info("%s: rxfill_ring->desc buffer alloc error\n",
				 __func__);
			return -ENOMEM;
		}

		/*
		 * Allocate buffers for each of the desc
		 */
		for (j = 0; j < rxfill_ring->count; j++) {
			rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, j);
			rxfill_desc->rdes0 = virt_to_phys(rx_buf);
#ifdef CONFIG_ARM64
			rxfill_desc->rdes1 = virt_to_phys((void*)
					(((uintptr_t)rx_buf) >> 32)) &
					EDMA_RXFILL_BUF_HI_ADD_MASK;
#else
			rxfill_desc->rdes1 = 0;
#endif
			rxfill_desc->rdes2 = 0;
			rxfill_desc->rdes3 = 0;
			rx_buf += EDMA_RX_BUFF_SIZE;
		}
	}

	/*
	 * Allocate RxDesc ring descriptors
	 */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		rxdesc_ring->count = EDMA_RX_RING_SIZE;
		rxdesc_ring->id = ehw->rxdesc_ring_start + i;

		/*
		 * Create a mapping between RX Desc ring and Rx fill ring.
		 * Number of fill rings are lesser than the descriptor rings
		 * Share the fill rings across descriptor rings.
		 */
		index = ehw->rxfill_ring_start + (i % ehw->rxfill_rings);
		rxdesc_ring->rxfill =
			&ehw->rxfill_ring[index - ehw->rxfill_ring_start];
		rxdesc_ring->rxfill = ehw->rxfill_ring;

		rxdesc_ring->desc = (void *)mem_alloc(
				EDMA_RXDESC_DESC_SIZE *
				rxdesc_ring->count,
				ARCH_DMA_MINALIGN);
		if (rxdesc_ring->desc == NULL) {
			pr_info("%s: rxdesc_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		rxdesc_ring->dma = virt_to_phys(rxdesc_ring->desc);

		/*
		 * Allocate secondary Rx ring descriptors
		 */
		rxdesc_ring->sdesc = (void *)mem_alloc(
				EDMA_RX_SEC_DESC_SIZE *
				rxdesc_ring->count,
				ARCH_DMA_MINALIGN);
		if (rxdesc_ring->sdesc == NULL) {
			pr_info("%s: rxdesc_ring->sdesc alloc error\n",
			__func__);
			return -ENOMEM;
		}
		rxdesc_ring->sdma = virt_to_phys(rxdesc_ring->sdesc);
	}

	/*
	 * Allocate TxDesc ring descriptors
	 */
	for (i = 0; i < ehw->txdesc_rings; i++) {
		txdesc_ring = &ehw->txdesc_ring[i];
		txdesc_ring->count = EDMA_TX_RING_SIZE;
		txdesc_ring->id = ehw->txdesc_ring_start + i;
		txdesc_ring->desc = (void *)mem_alloc(
				EDMA_TXDESC_DESC_SIZE *
				txdesc_ring->count,
				ARCH_DMA_MINALIGN);
		if (txdesc_ring->desc == NULL) {
			pr_info("%s: txdesc_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		txdesc_ring->dma = virt_to_phys(txdesc_ring->desc);

		tx_buf = (void *)mem_alloc(EDMA_TX_BUFF_SIZE *
					txdesc_ring->count,
					ARCH_DMA_MINALIGN);
		if (tx_buf == NULL) {
			pr_info("%s: txdesc_ring->desc buffer alloc error\n",
				 __func__);
			return -ENOMEM;
		}

		/*
		 * Allocate buffers for each of the desc
		 */
		for (j = 0; j < txdesc_ring->count; j++) {
			txdesc_desc = EDMA_TXDESC_DESC(txdesc_ring, j);
			txdesc_desc->tdes0 = virt_to_phys(tx_buf);
#ifdef CONFIG_ARM64
			txdesc_desc->tdes1 = virt_to_phys((void*)
					(((uintptr_t)tx_buf)>> 32)) &
					EDMA_TXDESC_BUF_HI_ADD_MASK;
#else
			txdesc_desc->tdes1 = 0;
#endif
			txdesc_desc->tdes2 = 0;
			txdesc_desc->tdes3 = 0;
			txdesc_desc->tdes4 = 0;
			txdesc_desc->tdes5 = 0;
			txdesc_desc->tdes6 = 0;
			txdesc_desc->tdes7 = 0;
			tx_buf += EDMA_TX_BUFF_SIZE;
		}

		/*
		 * Allocate secondary Tx ring descriptors
		 */
		txdesc_ring->sdesc = (void *)mem_alloc(
				EDMA_TX_SEC_DESC_SIZE *
				txdesc_ring->count,
				ARCH_DMA_MINALIGN);
		if (txdesc_ring->sdesc == NULL) {
			pr_info("%s: txdesc_ring->sdesc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		txdesc_ring->sdma = virt_to_phys(txdesc_ring->sdesc);
	}

	/*
	 * Allocate TxCmpl ring descriptors
	 */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		txcmpl_ring->count = EDMA_TX_RING_SIZE;
		txcmpl_ring->id = ehw->txcmpl_ring_start + i;
		txcmpl_ring->desc = (void *)mem_alloc(
				EDMA_TXCMPL_DESC_SIZE *
				txcmpl_ring->count,
				ARCH_DMA_MINALIGN);

		if (txcmpl_ring->desc == NULL) {
			pr_info("%s: txcmpl_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		txcmpl_ring->dma = virt_to_phys(txcmpl_ring->desc);
	}

	pr_info("%s: successfull\n", __func__);

	return 0;
}

static void ipq_edma_disable_rings(struct ipq_edma_hw *ehw)
{
	phys_addr_t reg_base = ehw->iobase;
	int i, desc_index;
	u32 data;

	/*
	 * Disable Rx rings
	 */
	for (i = 0; i < ehw->max_rxdesc_rings; i++) {
		data = readl(reg_base + EDMA_REG_RXDESC_CTRL(i));
		data &= ~EDMA_RXDESC_RX_EN;
		writel(data, reg_base + EDMA_REG_RXDESC_CTRL(i));
	}
	/*
	 * Disable RxFill Rings
	 */
	for (i = 0; i < ehw->max_rxfill_rings; i++) {
		data = readl(reg_base +
				EDMA_REG_RXFILL_RING_EN(i));
		data &= ~EDMA_RXFILL_RING_EN;
		writel(data, reg_base + EDMA_REG_RXFILL_RING_EN(i));
	}
	/*
	 * Disable Tx rings
	 */
	for (desc_index = 0; desc_index <
			 ehw->max_txdesc_rings; desc_index++) {
		data = readl(reg_base +
				EDMA_REG_TXDESC_CTRL(desc_index));
		data &= ~EDMA_TXDESC_TX_EN;
		writel(data, reg_base + EDMA_REG_TXDESC_CTRL(desc_index));
	}
}

static void ipq_edma_disable_intr(struct ipq_edma_hw *ehw)
{
	phys_addr_t reg_base = ehw->iobase;
	int i;

	/*
	 * Disable interrupts
	 */
	for (i = 0; i < ehw->max_rxdesc_rings; i++)
		writel(0, reg_base + EDMA_REG_RX_INT_CTRL(i));

	for (i = 0; i < ehw->max_rxfill_rings; i++)
		writel(0,reg_base + EDMA_REG_RXFILL_INT_MASK(i));

	for (i = 0; i < ehw->max_txcmpl_rings; i++)
		writel(0,reg_base + EDMA_REG_TX_INT_MASK(i));

	/*
	 * Clear MISC interrupt mask
	 */
	writel(EDMA_MASK_INT_DISABLE, reg_base + EDMA_REG_MISC_INT_MASK);
}

/*
 * ipq_edma_alloc_rings()
 *	Allocate EDMA software rings
 */
static int ipq_edma_alloc_rings(struct ipq_edma_hw *ehw)
{
	ehw->rxfill_ring = (void *)mem_alloc((sizeof(
				struct ipq_edma_rxfill_ring) *
				ehw->rxfill_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->rxfill_ring) {
		pr_info("%s: rxfill_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	ehw->rxdesc_ring = (void *)mem_alloc((sizeof(
				struct ipq_edma_rxdesc_ring) *
				ehw->rxdesc_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->rxdesc_ring) {
		pr_info("%s: rxdesc_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	ehw->txdesc_ring = (void *)mem_alloc((sizeof(
				struct ipq_edma_txdesc_ring) *
				ehw->txdesc_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->txdesc_ring) {
		pr_info("%s: txdesc_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	ehw->txcmpl_ring = (void *)mem_alloc((sizeof(
				struct ipq_edma_txcmpl_ring) *
				ehw->txcmpl_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->txcmpl_ring) {
		pr_info("%s: txcmpl_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	pr_info("%s: successfull\n", __func__);

	return 0;

}


/*
 * ipq_edma_init_rings()
 *	Initialize EDMA rings
 */
static int ipq_edma_init_rings(struct ipq_edma_hw *ehw)
{
	int ret;

	/*
	 * Allocate desc rings
	 */
	ret = ipq_edma_alloc_rings(ehw);
	if (ret)
		return ret;

	/*
	 * Setup ring resources
	 */
	ret = ipq_edma_setup_ring_resources(ehw);
	if (ret)
		return ret;

	return 0;
}

/*
 * ipq_edma_configure_txdesc_ring()
 *	Configure one TxDesc ring
 */
static void ipq_edma_configure_txdesc_ring(struct ipq_edma_hw *ehw,
				struct ipq_edma_txdesc_ring *txdesc_ring)
{
	uint64_t base;
	phys_addr_t reg_base = ehw->iobase;
	/*
	 * Configure TXDESC ring
	 */
	base = txdesc_ring->dma;
	writel((uint32_t)(txdesc_ring->dma & EDMA_RING_DMA_MASK),
		reg_base + EDMA_REG_TXDESC_BA(txdesc_ring->id));

	if (base & (~(uint64_t)EDMA_RING_DMA_MASK))
		writel((uint8_t)(base >> 32),
			reg_base + EDMA_REG_TXDESC_BA_HIGH(txdesc_ring->id));

	base = txdesc_ring->sdma;
	writel((uint32_t)(txdesc_ring->sdma & EDMA_RING_DMA_MASK),
		reg_base + EDMA_REG_TXDESC_BA2(txdesc_ring->id));

	if (base & (~(uint64_t)EDMA_RING_DMA_MASK))
		writel((uint8_t)(base >> 32),
		reg_base + EDMA_REG_TXDESC_BA2_HIGH(txdesc_ring->id));


	writel((uint32_t)(txdesc_ring->count & EDMA_TXDESC_RING_SIZE_MASK),
		reg_base + EDMA_REG_TXDESC_RING_SIZE(txdesc_ring->id));

	writel(EDMA_TX_INITIAL_PROD_IDX,
		reg_base + EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id));
}

/*
 * ipq_edma_configure_txcmpl_ring()
 *	Configure one TxCmpl ring
 */
static void ipq_edma_configure_txcmpl_ring(struct ipq_edma_hw *ehw,
				struct ipq_edma_txcmpl_ring *txcmpl_ring)
{
	uint64_t base;
	phys_addr_t reg_base = ehw->iobase;
	/*
	 * Configure TxCmpl ring base address
	 */
	base = txcmpl_ring->dma;
	writel((uint32_t)(txcmpl_ring->dma & EDMA_RING_DMA_MASK),
		reg_base + EDMA_REG_TXCMPL_BA(txcmpl_ring->id));

	if (base & (~(uint64_t)EDMA_RING_DMA_MASK))
		writel((uint8_t)(base >> 32),
		reg_base + EDMA_REG_TXCMPL_BA_HIGH(txcmpl_ring->id));

	writel((uint32_t)(txcmpl_ring->count & EDMA_TXDESC_RING_SIZE_MASK),
		reg_base + EDMA_REG_TXCMPL_RING_SIZE(txcmpl_ring->id));
	/*
	 * Set TxCmpl ret mode to opaque
	 */
	writel(EDMA_TXCMPL_RETMODE_OPAQUE,
		reg_base + EDMA_REG_TXCMPL_CTRL(txcmpl_ring->id));
	/*
	 * Enable ring. Set ret mode to 'opaque'.
	 */
	writel(EDMA_TX_NE_INT_EN,
		reg_base + EDMA_REG_TX_INT_CTRL(txcmpl_ring->id));
}

/*
 * ipq_edma_configure_rxdesc_ring()
 *	Configure one RxDesc ring
 */
static void ipq_edma_configure_rxdesc_ring(struct ipq_edma_hw *ehw,
				struct ipq_edma_rxdesc_ring *rxdesc_ring)
{
	phys_addr_t reg_base = ehw->iobase;
	uint32_t data;
	uint64_t base;

	base = rxdesc_ring->dma;
	writel((uint32_t)(rxdesc_ring->dma & EDMA_RING_DMA_MASK),
		reg_base + EDMA_REG_RXDESC_BA(rxdesc_ring->id));

	if (base & (~(uint64_t)EDMA_RING_DMA_MASK))
		writel((uint8_t)(base >> 32),
			reg_base + EDMA_REG_RXDESC_BA_HIGH(rxdesc_ring->id));

	base = rxdesc_ring->sdma;
	writel((uint32_t)(rxdesc_ring->sdma & EDMA_RING_DMA_MASK),
		reg_base + EDMA_REG_RXDESC_BA2(rxdesc_ring->id));

	if (base & (~(uint64_t)EDMA_RING_DMA_MASK))
		writel((uint8_t)(base >> 32),
			reg_base + EDMA_REG_RXDESC_BA2_HIGH(rxdesc_ring->id));

	if (ehw->sw_version == EDMA_SW_VER_2_ID) {
		data = readl(reg_base +
				EDMA_REG_RXDESC_FC_THRE(rxdesc_ring->id));
		data |= (ehw->rx_payload_offset & EDMA_RXDESC_PL_OFFSET_MASK)
			<< EDMA_RXDESC_PL_OFFSET_SHIFT_V2;
		writel(data, reg_base +
				EDMA_REG_RXDESC_FC_THRE(rxdesc_ring->id));

		data = rxdesc_ring->count & EDMA_RXDESC_RING_SIZE_MASK;
	} else {
		data = rxdesc_ring->count & EDMA_RXDESC_RING_SIZE_MASK;
		data |= (ehw->rx_payload_offset & EDMA_RXDESC_PL_OFFSET_MASK)
			<< EDMA_RXDESC_PL_OFFSET_SHIFT;
	}

	writel(data, reg_base +	EDMA_REG_RXDESC_RING_SIZE(rxdesc_ring->id));

	/*
	 * Enable ring. Set ret mode to 'opaque'.
	 */
	writel(EDMA_RX_NE_INT_EN,
		reg_base + EDMA_REG_RX_INT_CTRL(rxdesc_ring->id));
}

/*
 * ipq_edma_configure_rxfill_ring()
 *	Configure one RxFill ring
 */
static void ipq_edma_configure_rxfill_ring(struct ipq_edma_hw *ehw,
				struct ipq_edma_rxfill_ring *rxfill_ring)
{
	phys_addr_t reg_base = ehw->iobase;
	uint32_t data;
	uint64_t base;

	base = rxfill_ring->dma;
	writel((uint32_t)(rxfill_ring->dma & EDMA_RING_DMA_MASK),
		reg_base + EDMA_REG_RXFILL_BA(rxfill_ring->id));

	if (base & (~(uint64_t)EDMA_RING_DMA_MASK))
		writel((uint8_t)(base >> 32),
		reg_base + EDMA_REG_RXFILL_BA_HIGH(rxfill_ring->id));

	data = rxfill_ring->count & EDMA_RXFILL_RING_SIZE_MASK;

	if (ehw->sw_version == EDMA_SW_VER_2_ID)
		writel(data, reg_base +
				EDMA_REG_RXFILL_RING_SIZE_V2(rxfill_ring->id));
	else
		writel(data, reg_base +
				EDMA_REG_RXFILL_RING_SIZE(rxfill_ring->id));
}

/*
 * ipq_edma_configure_rings()
 *	Configure EDMA rings
 */
static void ipq_edma_configure_rings(struct ipq_edma_hw *ehw)
{
	int i;

	/*
	 * Configure TXDESC ring
	 */
	for (i = 0; i < ehw->txdesc_rings; i++)
		ipq_edma_configure_txdesc_ring(ehw, &ehw->txdesc_ring[i]);

	/*
	 * Configure TXCMPL ring
	 */
	for (i = 0; i < ehw->txcmpl_rings; i++)
		ipq_edma_configure_txcmpl_ring(ehw, &ehw->txcmpl_ring[i]);

	/*
	 * Configure RXFILL rings
	 */
	for (i = 0; i < ehw->rxfill_rings; i++)
		ipq_edma_configure_rxfill_ring(ehw, &ehw->rxfill_ring[i]);

	/*
	 * Configure RXDESC ring
	 */
	for (i = 0; i < ehw->rxdesc_rings; i++)
		ipq_edma_configure_rxdesc_ring(ehw, &ehw->rxdesc_ring[i]);

	pr_info("%s: successfull\n", __func__);
}

/*
 * ipq_edma_hw_init()
 *	EDMA hw init
 */
int ipq_edma_hw_init(struct udevice *dev, struct ipq_eth_dev *eth)
{
	struct edma_config *config =
			(struct edma_config*)dev_get_driver_data(dev);
	struct ipq_edma_rxdesc_ring *rxdesc_ring = NULL;
	struct ipq_edma_hw *ehw = &eth->hw;
	phys_addr_t reg_base = ehw->iobase;
	struct ppe_info *ppe = &eth->ppe;
	int ret, desc_index;
	uint32_t i, reg, reg_idx, ring_id;
	volatile uint32_t data;

	/*
	 * PPE Init
	 */
	ppe->no_ports = config->ports;
	ppe->nos_iports = config->iports;
	ppe->vsi = config->vsi;
	ppe->tdm_ctrl_val = config->tdm_ctrl_val;
	ppe->ipo_action = config->ipo_action;

	ipq_ppe_provision_init(ppe);

	data = readl(reg_base + EDMA_REG_MAS_CTRL);
	printf("EDMA ver %d\n", data);

	/*
	 * Setup private data structure
	 */
	ehw->sw_version = config->sw_version;
	ehw->rxfill_intr_mask = EDMA_RXFILL_INT_MASK;
	ehw->rxdesc_intr_mask = EDMA_RXDESC_INT_MASK_PKT_INT;
	ehw->txcmpl_intr_mask = EDMA_TX_INT_MASK_PKT_INT;
	ehw->misc_intr_mask = EDMA_MISC_INTR_MASK;
	ehw->rx_payload_offset = EDMA_RX_PAYLOAD_OFFSET;

	UPDATE_EDMA_CONFIG(config, ehw);

	/*
	 * Disable interrupts
	 */
	ipq_edma_disable_intr(ehw);

	/*
	 * Disable rings
	 */
	ipq_edma_disable_rings(ehw);


	ret = ipq_edma_init_rings(ehw);
	if (ret)
		return ret;

	ipq_edma_configure_rings(ehw);

	/*
	 * Clear the TXDESC2CMPL_MAP_xx reg before setting up
	 * the mapping. This register holds TXDESC to TXFILL ring
	 * mapping.
	 */
	WRITE_REG_ARRAY(reg_base, EDMA_REG_TXDESC2CMPL_MAP_0, 4, 0,
			config->tx_map);

	desc_index = ehw->txcmpl_ring_start;

	/*
	 * 6 registers to hold the completion mapping for total 32
	 * TX desc rings (0-5, 6-11, 12-17, 18-23, 24-29 & rest).
	 * In each entry 5 bits hold the mapping for a particular TX desc ring.
	 */
	for (i = ehw->txdesc_ring_start;
		i < ehw->txdesc_ring_end; i++) {
		if ((i >= 0) && (i <= 5))
			reg = EDMA_REG_TXDESC2CMPL_MAP_0;
		else if ((i >= 6) && (i <= 11))
			reg = EDMA_REG_TXDESC2CMPL_MAP_1;
		else if ((i >= 12) && (i <= 17))
			reg = EDMA_REG_TXDESC2CMPL_MAP_2;
		else if ((i >= 18) && (i <= 23))
			reg = EDMA_REG_TXDESC2CMPL_MAP_3;
		else if ((i >= 24) && (i <= 29))
			reg = EDMA_REG_TXDESC2CMPL_MAP_4;
		else
			reg = EDMA_REG_TXDESC2CMPL_MAP_5;

		pr_debug("Configure TXDESC:%u to use TXCMPL:%u\n",
			 i, desc_index);

		/*
		 * Set the Tx complete descriptor ring number in the mapping
		 * register.
		 * E.g. If (txcmpl ring)desc_index = 31, (txdesc ring)i = 28.
		 * 	reg = EDMA_REG_TXDESC2CMPL_MAP_4
		 * 	data |= (desc_index & 0x1F) << ((i % 6) * 5);
		 * 	data |= (0x1F << 20); - This sets 11111 at 20th bit of
		 * 	register EDMA_REG_TXDESC2CMPL_MAP_4
		 */

		data = readl(reg_base + reg);
		data |= (desc_index & 0x1F) << ((i % 6) * 5);
		writel(data, reg_base + reg);

		desc_index++;
		if (desc_index == ehw->txcmpl_ring_end)
			desc_index = ehw->txcmpl_ring_start;
	}
	/*
	 * Set PPE QID to EDMA Rx ring mapping.
	 * Each entry can hold mapping for 4 PPE queues and entry size is
	 * 4 bytes
	 */
	desc_index = (ehw->rxdesc_ring_start & 0x1f);

	reg = EDMA_QID2RID_TABLE_MEM(0);
	data = ((desc_index << 0) & 0xff) |
	       (((desc_index + 1) << 8) & 0xff00) |
	       (((desc_index + 2) << 16) & 0xff0000) |
	       (((desc_index + 3) << 24) & 0xff000000);

	writel(data, reg_base + reg);
	pr_debug("Configure QID2RID(0) reg:0x%x to 0x%x\n", reg, data);

	/*
	 * Map PPE multicast queues to the first Rx ring.
	 */
	desc_index = (ehw->rxdesc_ring_start & 0x1f);

	for (i = EDMA_CPU_PORT_MC_QID_MIN;
		i <= EDMA_CPU_PORT_MC_QID_MAX;
			i += EDMA_QID2RID_NUM_PER_REG) {
		reg_idx = i/EDMA_QID2RID_NUM_PER_REG;

		reg = EDMA_QID2RID_TABLE_MEM(reg_idx);
		data = ((desc_index << 0) & 0xff) |
		       ((desc_index << 8) & 0xff00) |
		       ((desc_index << 16) & 0xff0000) |
		       ((desc_index << 24) & 0xff000000);

		writel(data, reg_base + reg);
		pr_debug("Configure QID2RID(%d) reg:0x%x to 0x%x\n",
				reg_idx, reg, data);
	}

	/*
	 * Set RXDESC2FILL_MAP_xx reg.
	 * There are 3 registers RXDESC2FILL_0, RXDESC2FILL_1 and RXDESC2FILL_2
	 * 3 bits holds the rx fill ring mapping for each of the
	 * rx descriptor ring.
	 */
	WRITE_REG_ARRAY(reg_base, EDMA_REG_RXDESC2FILL_MAP_0, 4, 0,
			config->rx_map);

	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];

		ring_id = rxdesc_ring->id;
		if ((ring_id >= 0) && (ring_id <= 9))
			reg = EDMA_REG_RXDESC2FILL_MAP_0;
		else if ((ring_id >= 10) && (ring_id <= 19))
			reg = EDMA_REG_RXDESC2FILL_MAP_1;
		else
			reg = EDMA_REG_RXDESC2FILL_MAP_2;


		pr_debug("Configure RXDESC:%u to use RXFILL:%u\n",
				ring_id, rxdesc_ring->rxfill->id);

		/*
		 * Set the Rx fill descriptor ring number in the mapping
		 * register.
		 */
		data = readl(reg_base + reg);
		data |= (rxdesc_ring->rxfill->id & 0x7) << ((ring_id % 10) * 3);
		writel(data, reg_base + reg);
	}
	/*
	 * Configure DMA request priority, DMA read burst length,
	 * and AXI write size.
	 */
	data = EDMA_DMAR_BURST_LEN_SET(EDMA_BURST_LEN_ENABLE)
		| EDMA_DMAR_REQ_PRI_SET(0)
		| EDMA_DMAR_TXDATA_OUTSTANDING_NUM_SET(31)
		| EDMA_DMAR_TXDESC_OUTSTANDING_NUM_SET(7)
		| EDMA_DMAR_RXFILL_OUTSTANDING_NUM_SET(7);
	writel(data, reg_base + EDMA_REG_DMAR_CTRL);
	/*
	 * Global EDMA and padding enable
	 */
	writel(EDMA_PORT_CTRL_EN, reg_base + EDMA_REG_PORT_CTRL);
	/*
	 * Enable Rx rings
	 */
	for (i = ehw->rxdesc_ring_start; i < ehw->rxdesc_ring_end; i++) {
		data = readl(reg_base + EDMA_REG_RXDESC_CTRL(i));
		data |= EDMA_RXDESC_RX_EN;
		writel(data, reg_base + EDMA_REG_RXDESC_CTRL(i));
	}

	for (i = ehw->rxfill_ring_start; i < ehw->rxfill_ring_end; i++) {
		data = readl(reg_base + EDMA_REG_RXFILL_RING_EN(i));
		data |= EDMA_RXFILL_RING_EN;
		writel(data, reg_base + EDMA_REG_RXFILL_RING_EN(i));
	}
	/*
	 * Enable Tx rings
	 */
	for (i = ehw->txdesc_ring_start; i < ehw->txdesc_ring_end; i++) {
		data = readl(reg_base + EDMA_REG_TXDESC_CTRL(i));
		data |= EDMA_TXDESC_TX_EN;
		writel(data, reg_base + EDMA_REG_TXDESC_CTRL(i));
	}
	/*
	 * Enable MISC interrupt mask
	 */
	writel(ehw->misc_intr_mask, reg_base + EDMA_REG_MISC_INT_MASK);

	pr_info("%s: successfull\n", __func__);

	return 0;
}

static int ipq_eth_port_set_up(struct ipq_eth_dev *priv,
					struct port_info *port)
{
	int mac_speed, i , rate = 0;
	int ret = 0;
	char clk_name[64];
	struct clk clk, pclk;

	switch(port->cur_speed) {
	case 100:
		mac_speed = 1;
		break;
	case 1000:
		mac_speed = 2;
		break;
	case 10000:
		mac_speed = 3;
		break;
	case 2500:
		mac_speed = (port->xgmac)? 4 : 2;
		break;
	case 5000:
		mac_speed = 5;
		break;
	default:
		/* speed 10Mbps */
		mac_speed = 0;
	}

	for (i = 0; port_config[i].id != UNUSED_PHY_TYPE; ++i) {
		if (port->phy_id != port_config[i].id)
			continue;
		rate = port_config[i].clk_rate[mac_speed];
		port->uniphy_mode = port_config[i].mode[mac_speed];
		port->gmac_type = port_config[i].mac_mode[mac_speed];
		port->mac_speed = mac_speed;
		break;
	}

	if (port_config[i].id != UNUSED_PHY_TYPE) {

		if (port->cur_uniphy_mode != port->uniphy_mode) {
			ppe_uniphy_mode_set(port);
			port->cur_uniphy_mode = port->uniphy_mode;
		}

		snprintf(clk_name, sizeof(clk_name), "uniphy%d_nss_rx_clk",
			port->uniphy_id);

		ret = clk_get_by_name(priv->dev, clk_name, &pclk);
		if(ret)
			goto fail;

		if ((port->uniphy_mode == PORT_WRAPPER_PSGMII) ||
			(port->uniphy_mode == PORT_WRAPPER_SGMII0_RGMII4)) {
			clk_set_rate(&pclk, CLK_125_MHZ);
		} else {
			clk_set_rate(&pclk, CLK_312_5_MHZ);
		}

		snprintf(clk_name, sizeof(clk_name), "nss_cc_port%d_rx_clk",
			port->id);

		ret = clk_get_by_name(priv->dev, clk_name, &clk);
		if(ret)
			goto fail;

		ret = clk_set_parent(&clk, &pclk);
		if(ret)
			goto fail;

		clk_set_rate(&clk, rate);

		snprintf(clk_name, sizeof(clk_name), "uniphy%d_nss_tx_clk",
			port->uniphy_id);

		ret = clk_get_by_name(priv->dev, clk_name, &pclk);
		if(ret)
			goto fail;

		snprintf(clk_name, sizeof(clk_name), "nss_cc_port%d_tx_clk",
			port->id);

		ret = clk_get_by_name(priv->dev, clk_name, &clk);
		if(ret)
			goto fail;

		ret = clk_set_parent(&clk, &pclk);
		if(ret)
			goto fail;

		clk_set_rate(&clk, rate);

		snprintf(clk_name, sizeof(clk_name),
			"nss_cc_uniphy_port%d_rx_clk", port->id);

		ret = clk_get_by_name(priv->dev, clk_name, &clk);
		if(ret)
			goto fail;

		clk_enable(&clk);

		snprintf(clk_name, sizeof(clk_name),
			"nss_cc_uniphy_port%d_tx_clk", port->id);

		ret = clk_get_by_name(priv->dev, clk_name, &clk);
		if(ret)
			goto fail;

		clk_enable(&clk);

		ipq_port_mac_clock_reset(priv->dev, port);

		ppe_port_speed_set(priv->ppe.base, port);

	} else if (priv->emulation) {
		port->gmac_type = XGMAC;
		port->mac_speed = mac_speed;
		port->uniphy_mode = PORT_WRAPPER_EMULATION;

		ppe_port_speed_set(priv->ppe.base, port);
	}
fail:
	return ret;
}

static int ipq_eth_start(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct phy_device *phydev;
	struct port_info *port = NULL;
	int i, ret, link, speed, duplex, linkup = -1;
	ulong active_port = env_get_ulong("active_port", 10,
						CONFIG_ETH_MAX_MAC);

#ifdef CONFIG_ETH_LOW_MEM
	dcache_disable();
#endif

	if (IS_ENABLED(CONFIG_TFTP_PORT))
		env_set_ulong("tftpsrcp", tftp_acl_our_port);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {

		port = priv->port[i];

		if (!port)
			continue;

		if (!port->phydev && !priv->emulation)
			continue;

		if (active_port != i && active_port != CONFIG_ETH_MAX_MAC) {
			ppe_port_bridge_txmac_set(priv->ppe.base,
					port->id, false);
			port->cur_speed = 0;
			continue;
		}
		/*
		 * set default value before read phy status
		 */
		link = 0;
		speed = 10;
		duplex = 0;

		if ((port->phy_id == SFP10G_PHY_TYPE) ||
			(port->phy_id == SFP2_5G_PHY_TYPE) ||
			(port->phy_id == SFP1G_PHY_TYPE)) {
				ret = phy_status_get_from_ppe(priv->ppe.base,
								port->id);
			link = ((ret & LINK_STATUS) != 0) ? 1 : 0;
			if (link) {
				++linkup;
				duplex = ((ret & DUPLEX) != 0) ? 1: 0;
				speed = mac_speed_config[ret & SPEED];
			}
		} else {
			phydev = port->phydev;

			if (phydev && port->isconfigured) {
				/* Start up the PHY */
				ret = phy_startup(phydev);
				if (ret < 0) {
					continue;
				} else {
					if (phydev->link) {
						++linkup;
						link = phydev->link;
						duplex = phydev->duplex;
						speed = phydev->speed;
					}
				}
			} else if (priv->emulation) {
				/*
				 * Clock rate will be like 1/100 or 1/150
				 * in the emulation platform. So, configuring
				 * MAC with higher speed, but in actuall it is
				 * running with lower PHY speed.
				 *
				 * Eg: 10G (MAC) <==> 100M (PHY)
				 */
				++linkup;
				link = 1;
				duplex = 1;
				speed = 10000;
			} else {
				continue;
			}
		}

		if ((port->phy_id != QCA8x8x_SWITCH_TYPE) &&
			(port->phy_id != QCA8337_SWITCH_TYPE))
			printf("PHY%d %s Speed : %d %s \n", port->id,
				(link ? "Up":"Down"), speed,
				duplex ? "Full duplex": "Half duplex");

		if (port->cur_speed != speed) {
			port->cur_speed = speed;
			port->duplex = duplex;
			ipq_eth_port_set_up(priv, port);
		}
	}

	return linkup;
}

static int ipq_eth_send(struct udevice *dev, void *packet, int length)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct ipq_edma_hw *ehw = &priv->hw;
	struct ppe_info *ppe = &priv->ppe;
	struct ipq_edma_txdesc_desc *txdesc;
	struct ipq_edma_txdesc_ring *txdesc_ring;
	uint16_t hw_next_to_use, hw_next_to_clean, chk_idx;
	uint32_t data;
	uchar *skb;
	phys_addr_t reg_base = ehw->iobase;

	txdesc_ring = ehw->txdesc_ring;
	/*
	 * Read TXDESC ring producer index
	 */
	data = readl(reg_base + EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id));

	hw_next_to_use = data & EDMA_TXDESC_PROD_IDX_MASK;

	pr_debug("%s: txdesc_ring->id = %d\n", __func__, txdesc_ring->id);

	/*
	 * Read TXDESC ring consumer index
	 */
	/*
	 * TODO - read to local variable to optimize uncached access
	 */
	data = readl(reg_base +
			EDMA_REG_TXDESC_CONS_IDX(txdesc_ring->id));

	hw_next_to_clean = data & EDMA_TXDESC_CONS_IDX_MASK;
	pr_debug("%s: hw_next_to_use = %d hw_next_to_clean = %d\n",
			__func__, hw_next_to_use, hw_next_to_clean);

	/*
	 * Check for available Tx descriptor
	 */
	chk_idx = (hw_next_to_use + 1) & (txdesc_ring->count - 1);

	if (chk_idx == hw_next_to_clean) {
		pr_info("netdev tx busy");
		return EBUSY;
	}

	/*
	 * Get Tx descriptor
	 */
	txdesc = EDMA_TXDESC_DESC(txdesc_ring, hw_next_to_use);

	txdesc->tdes2 = 0;
	txdesc->tdes3 = 0;
	txdesc->tdes4 = 0;
	txdesc->tdes5 = 0;
	txdesc->tdes6 = 0;
	txdesc->tdes7 = 0;
	skb = (uchar *)((uintptr_t)(((uint64_t)(txdesc->tdes1 &
				EDMA_TXDESC_BUF_HI_ADD_MASK) << 32) |
				txdesc->tdes0));

	pr_debug("%s: txdesc->tdes0 (buffer addr) = 0x%lx "
			"txdesc->tdes1 (buffer addr) = 0x%lx length = %d "
			"prod_idx = %d cons_idx = %d\n", __func__,
			(uintptr_t)txdesc->tdes0, (uintptr_t)txdesc->tdes1,
			length,	hw_next_to_use, hw_next_to_clean);

	if (ppe->bridge_mode) {
		/* VP 0x0 share vsi 2 with port 1-4 */
		/* src is 0x2000, dest is 0x0 */
		txdesc->tdes4 = 0x00002000;
	} else {
	/*
	 * Populate Tx dst info, port id is macid in dp_dev
	 * We have separate netdev for each port in Kernel but that is not the
	 * case in U-Boot.
	 * This part needs to be fixed to support multiple ports in non bridged
	 * mode during when all the ports are currently under same netdev.
	 */
		txdesc->tdes4 |= (EDMA_DST_PORT_TYPE_SET(EDMA_DST_PORT_TYPE) |
					EDMA_DST_PORT_ID_SET(ppe->nbport));
	}
	/*
	 * Set opaque field
	 */
	txdesc->tdes2 = cpu_to_le32(txdesc->tdes0);
	txdesc->tdes3 = (cpu_to_le32(txdesc->tdes1) &
			EDMA_TXDESC_BUF_HI_ADD_MASK);

	/*
	 * copy the packet
	 */
	memcpy(skb, packet, length);

	/*
	 * Populate Tx descriptor
	 */
	txdesc->tdes5 |= ((length << EDMA_TXDESC_DATA_LENGTH_SHIFT) &
			  EDMA_TXDESC_DATA_LENGTH_MASK);

	/*
	 * Update producer index
	 */
	hw_next_to_use = (hw_next_to_use + 1) & (txdesc_ring->count - 1);

	/*
	 * make sure the hw_next_to_use is updated before the
	 * write to hardware
	 */

	writel(hw_next_to_use & EDMA_TXDESC_PROD_IDX_MASK,
		reg_base + EDMA_REG_TXDESC_PROD_IDX(txdesc_ring->id));

	pr_debug("%s: successfull\n", __func__);

	return 0;
}

static int ipq_eth_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct ipq_edma_rxdesc_ring *rxdesc_ring;
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
	struct ipq_edma_rxfill_ring *rxfill_ring;
	struct ipq_edma_hw *ehw = &priv->hw;
	phys_addr_t reg_base = ehw->iobase;
	volatile u32 reg_data;
	u32 rxdesc_intr_status = 0;
	u32 txcmpl_intr_status = 0, rxfill_intr_status = 0;
	int i, length = 0;

	/*
	 * Read RxDesc intr status
	 */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];

		reg_data = readl(reg_base +
				EDMA_REG_RXDESC_INT_STAT(rxdesc_ring->id));
		rxdesc_intr_status |= reg_data &
				EDMA_RXDESC_RING_INT_STATUS_MASK;

		/*
		 * Disable RxDesc intr
		 */
		writel(EDMA_MASK_INT_DISABLE,
			reg_base + EDMA_REG_RXDESC_INT_MASK(rxdesc_ring->id));
	}

	/*
	 * Read TxCmpl intr status
	 */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];

		reg_data = readl(reg_base +
				EDMA_REG_TX_INT_STAT(
					txcmpl_ring->id));
		txcmpl_intr_status |= reg_data &
				EDMA_TXCMPL_RING_INT_STATUS_MASK;

		/*
		 * Disable TxCmpl intr
		 */
		writel(EDMA_MASK_INT_DISABLE,
			reg_base + EDMA_REG_TX_INT_MASK(txcmpl_ring->id));
	}

	/*
	 * Read RxFill intr status
	 */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];

		reg_data = readl(reg_base +
				EDMA_REG_RXFILL_INT_STAT(
					rxfill_ring->id));
		rxfill_intr_status |= reg_data &
				EDMA_RXFILL_RING_INT_STATUS_MASK;

		/*
		 * Disable RxFill intr
		 */
		writel(EDMA_MASK_INT_DISABLE,
			reg_base + EDMA_REG_RXFILL_INT_MASK(rxfill_ring->id));
	}

	if ((rxdesc_intr_status != 0) || (txcmpl_intr_status != 0) ||
	    (rxfill_intr_status != 0)) {
		pr_debug("%s: rxdesc_intr_status = %d txcmpl_intr_status = %d"
				" rxfill_intr_status = %d \n", __func__,
				rxdesc_intr_status, txcmpl_intr_status,
				rxfill_intr_status);
		for (i = 0; i < ehw->rxdesc_rings; i++) {
			rxdesc_ring = &ehw->rxdesc_ring[i];
			writel(EDMA_MASK_INT_DISABLE,
				reg_base + EDMA_REG_RXDESC_INT_MASK(
					rxdesc_ring->id));
		}

		length = ipq_edma_rx_complete(priv, (void **) packetp);
	}

	return length;
}

static int ipq_eth_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	return 0;
}

static void ipq_eth_stop(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct phy_device *phydev;
	int i;

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		if(priv->port[i] == NULL)
			continue;

		phydev = priv->port[i]->phydev;
		if (phydev && priv->port[i]->isconfigured) {
			phy_shutdown(phydev);
		}
	}

#ifdef CONFIG_ETH_LOW_MEM
	dcache_enable();
#endif
}

static int ipq_eth_write_hwaddr(struct udevice *dev)
{
        return 0;
}

static int ipq_eth_read_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	uchar enet_addr[6] = { 0 };
	int ret;

	/* Getting the MAC address from ART partition */
	ret = get_eth_mac_address(&enet_addr[0], 1);
	if (ret && is_valid_ethaddr(enet_addr)) {
		memcpy(&pdata->enetaddr[0], &enet_addr[0], 6);
	} else {
		memcpy(&pdata->enetaddr[0], &ipq_def_enetaddr[0], 6);
	}

	return 0;
}

static int ipq_eth_bind(struct udevice *dev)
{
	return 0;
}

static void ipq_eth_phy_hw_reset(struct gpio_desc *gpio)
{
	u32 data;

	data = dm_gpio_get_value(gpio);
	data |= BIT(1);
	dm_gpio_set_value(gpio, data);
#ifdef CONFIG_PHY_AQUANTIA
	mdelay(500);
#else
	mdelay(100);
#endif
}

#ifdef CONFIG_PHY_QTI_8X8X
static void ipq_eth_8x8x_write(struct phy_device *phydev,
		uint32_t reg, uint32_t val) {

	uint16_t r1, r2, page, switch_phy_id;
	uint16_t lo = val & 0xffff;
	uint16_t hi = (uint16_t) (val >> 16);

	r1 = reg & 0x1c;
	reg >>= 5;
	r2 = reg & 0x7;
	reg >>= 3;
	page = reg & 0xffff;
	reg >>= 16;
	switch_phy_id = reg & 0xff;

	phydev->addr = (0x18 | (switch_phy_id >> 5));
	phy_write(phydev, MDIO_DEVAD_NONE, switch_phy_id & 0x1f, page);
	udelay(100);

	phydev->addr = (0x10 | r2);
	phy_write(phydev, MDIO_DEVAD_NONE, r1, lo);
	phy_write(phydev, MDIO_DEVAD_NONE, r1 + 2, hi);
	return;
}

static void ipq_eth_8x8x_pre_init(struct mii_dev *bus)
{
	struct phy_device temp_phydev;
	uint32_t phy_data;

	/*
	 * Buid temporary data structures that the chip reading code needs to
	 * read the ID
	 */
	temp_phydev.bus = bus;

	temp_phydev.addr = 0x18;
	phy_write(&temp_phydev, MDIO_DEVAD_NONE, 0xc, 0x90f0);
	temp_phydev.addr = 0x10;
	phy_data = phy_read(&temp_phydev, MDIO_DEVAD_NONE, 0x18);
	phy_data |= (phy_read(&temp_phydev, MDIO_DEVAD_NONE, 0x1a) << 16);
	pr_debug("%s %d phy_data: 0x%x \n", __func__, __LINE__, phy_data);

	if (phy_data == 0x20c41) {
		pr_debug("%s %d addr fixup and clk init already done!!!\n",
				__func__, __LINE__);
		return;
	}

	/* addr fixup */
	ipq_eth_8x8x_write(&temp_phydev, 0xc90f018, 0x320c41);
	ipq_eth_8x8x_write(&temp_phydev, 0xc90f014, 0x1cc5);

	/* clk init */
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001a8, 0x80000001);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001ac, 0x80000001);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001a8, 0x5);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001a8, 0x1);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001ac, 0x5);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001ac, 0x1);
	ipq_eth_8x8x_write(&temp_phydev, 0xc800058, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc800078, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc800098, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8000b8, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8000d8, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8000f8, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc800118, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc800138, 0x80000000);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b0, 0x80000001);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b4, 0x80000001);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b8, 0x80000001);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001bc, 0x80000001);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b0, 0x5);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b0, 0x1);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b4, 0x5);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b4, 0x1);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b8, 0x5);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001b8, 0x1);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001bc, 0x5);
	ipq_eth_8x8x_write(&temp_phydev, 0xc8001bc, 0x1);
	ipq_eth_8x8x_write(&temp_phydev, 0xc800304, 0x0);
	ipq_eth_8x8x_write(&temp_phydev, 0xc90f018, 0x20c41);
}
#endif

#ifdef CONFIG_MDIO_QCOM_I2C
#define SFP_EEPROM_I2C_ADDR			0x50

static int ipq_eth_sfp_detect(struct udevice *i2c_bus, struct port_info *port)
{
	struct udevice *dev;
	int ret = dm_i2c_probe(i2c_bus, SFP_EEPROM_I2C_ADDR, 0, &dev);
	if (!ret) {
		/*
		 * SFP detected, update port info with respect to SFP
		 */
		port->phy_id = SFP10G_PHY_TYPE;
		port->phyaddr = 0xFF;
		port->rst_gpio.dev = NULL;
	}

	return ret;
}
#endif

static void ipq_eth_configure_uniphy_50m(int count, phys_addr_t uniphy_base,
					size_t size)
{
/*
 * support in IPQ5332
 */
	int i = 0;
	phys_addr_t base;

	while (count) {
		base = (uniphy_base + (i * size));
		base += CLKOUT_50M_CTRL_OPTION;
		writel(readl(base) |  BIT(0), base);

		count >>= 1;
		++i;
	}
}

static int ipq_eth_probe(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct udevice *busdev;
	struct port_info *port;
	struct clk_bulk clocks;
	struct reset_ctl_bulk resets;
	int ret, i, configured = 0;
#ifdef CONFIG_PHY_QTI_8X8X
	int phy_no = 0;
#endif

	mem_init();

	ret = reset_get_bulk(dev, &resets);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "Can't get reset: %d\n", ret);
		return -ENODEV;
	}

	ret = reset_assert_bulk(&resets);
	if (ret)
		return ret;

	mdelay(10);

	ret = reset_deassert_bulk(&resets);
	if (ret)
		return ret;

	ret = clk_get_bulk(dev, &clocks);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "Failed to get clocks (ret=%d)\n", ret);
		goto fail;
	}

	ret = clk_enable_bulk(&clocks);
	if (ret) {
		dev_err(dev, "Failed to enable clocks (ret=%d)\n", ret);
		goto fail;
	}

	if (priv->uniphy_50mhz)
		ipq_eth_configure_uniphy_50m(priv->uniphy_50mhz,
						priv->uniphy_base,
						priv->uniphy_size);

	ipq_edma_hw_init(dev, priv);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		port = priv->port[i];
		if (port == NULL)
			continue;

		if (priv->emulation) {
			port->isconfigured = true;
			++configured;
			continue;
		}

		port->dev = dev;

		port->uniphy_base = priv->uniphy_base +
					(port->uniphy_id * priv->uniphy_size);

#ifdef CONFIG_MDIO_QCOM_I2C
		if (port->i2c_bus) {
			ret = uclass_get_device_by_phandle_id(UCLASS_I2C,
					port->i2c_bus, &busdev);
			if (ret) {
				printf("%s: failed to get i2c bus, err: %d\n",
						__func__, ret);
				continue;
			}

			port->bus = (struct mii_dev *)(uintptr_t)
					qcom_mdio_i2c_alloc(busdev,
							port->phyaddr);
			if (!port->bus) {
				if (ipq_eth_sfp_detect(busdev, port))
					continue;
			}
		}
#endif /* CONFIG_MDIO_QCOM_I2C */

		if (!port->bus)
		{
			ret = uclass_get_device_by_ofnode(UCLASS_MDIO,
					port->pnode, &busdev);
			if (ret)
				continue;

			port->bus = miiphy_get_dev_by_name(busdev->name);
		}

		if (!port->bus)
			continue;

		if (port->rst_gpio.dev)
			ipq_eth_phy_hw_reset(&port->rst_gpio);

#ifdef CONFIG_PHY_QTI_8X8X
		if (port->phy_id == QCA8x8x_PHY_TYPE ||
			port->phy_id == QCA8x8x_SWITCH_TYPE)
			ipq_eth_8x8x_pre_init(port->bus);
#endif
		if ((port->phy_id == SFP10G_PHY_TYPE) ||
			(port->phy_id == SFP2_5G_PHY_TYPE) ||
			(port->phy_id == SFP1G_PHY_TYPE)) {
				port->phydev = phy_device_create(port->bus,
							port->phyaddr,
							PHY_FIXED_ID,
							true);
				if (IS_ERR_OR_NULL(port->phydev))
					continue;

				port->phydev->dev = dev;
				port->phydev->interface = port->interface;
		} else {
			port->phydev = phy_connect(port->bus, port->phyaddr,
							dev,port->interface);
		}

		if (IS_ERR_OR_NULL(port->phydev))
			continue;

		if (ofnode_valid(port->node))
			port->phydev->node = port->node;

#ifdef CONFIG_PHY_QCA_8337
		if (port->phy_id == QCA8337_SWITCH_TYPE) {
		/*
		 * The below listed configure need to perform
		 * before init switch
		 * set UNIPHY mode as SGMII
		 * Configure GMAC
		 * Disable txmac
		 * Disable GMAC
		 * configure uniphy force mode
		 */
			port->uniphy_mode = PORT_WRAPPER_SGMII0_RGMII4;
			port->cur_uniphy_mode = PORT_WRAPPER_SGMII0_RGMII4;
			port->gmac_type = port->cur_gmac_type = GMAC;
			ppe_uniphy_mode_set(port);
			ppe_port_mux_set(priv->ppe.base, port);
			ppe_port_bridge_txmac_set(priv->ppe.base, port->id,
							false);

			if (port->isforce_speed)
				ppe_uniphy_set_forceMode(port);
		}
#endif

#if defined(CONFIG_PHY_QCA_8337) || defined(CONFIG_PHY_QTI_8033)
	if (port->support_25M)
		ppe_uniphy_refclk_set_25M(port);
#endif

#ifdef CONFIG_PHY_QTI_8X8X
		/*
		 * configure UQXGMII for pure PHY mode since MHT PHY requires
		 * uniphy pre-init before configuring uniphy mode, which has
		 * to be configured by default to UQXGMII mode regardless of
		 * speed link up.
		 */
		if (port->phy_id == QCA8x8x_PHY_TYPE) {
			port->uniphy_mode = port->cur_uniphy_mode =
						PORT_WRAPPER_UQXGMII;
			port->gmac_type = port->cur_gmac_type = XGMAC;

			if (phy_no == 0)
				ppe_uniphy_mode_set(port);

			ppe_port_mux_set(priv->ppe.base, port);
			++phy_no;
		} else if (port->phy_id == QCA8x8x_SWITCH_TYPE) {
			port->uniphy_mode = port->cur_uniphy_mode =
						PORT_WRAPPER_SGMII_PLUS;
			ppe_uniphy_mode_set(port);
		}
#endif

#ifdef CONFIG_PHY_AQUANTIA
		if (port->phy_id == AQ_PHY_TYPE) {
			ipq_aquantia_load_fw(port->phydev);
			mdelay(100);
		}
#endif

#ifdef CONFIG_PHY_QCA_81XX
		if (port->phy_id == QCA81xx_PHY_TYPE) {
			port->uniphy_mode = port->cur_uniphy_mode =
						PORT_WRAPPER_USXGMII;
			port->gmac_type = port->cur_gmac_type = XGMAC;

			ppe_uniphy_mode_set(port);
			ppe_port_mux_set(priv->ppe.base, port);
		}
#endif

		ret = phy_config(port->phydev);
		if (ret < 0)
			continue;

		port->isconfigured = true;

		++configured;

	}
fail:
	return !configured;
}

static int ipq_eth_remove(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	int i;

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		if (priv->port[i]) {
			free(priv->port[i]);
			priv->port[i] = NULL;
		}
	}

	return 0;
}

static const struct eth_ops ipq_eth_ops = {
	.start			= ipq_eth_start,
	.send			= ipq_eth_send,
	.recv			= ipq_eth_recv,
	.free_pkt		= ipq_eth_free_pkt,
	.stop			= ipq_eth_stop,
	.write_hwaddr		= ipq_eth_write_hwaddr,
	.read_rom_hwaddr        = ipq_eth_read_hwaddr,
};

static int ipq_eth_ofdata_to_platdata(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct ppe_info *ppe;
	struct ofnode_phandle_args phandle_args;
	char phy_handle[13];
	int i, port_count = 0;

	memset(priv, 0, sizeof(struct ipq_eth_dev));

	priv->dev = dev;
	ppe = &priv->ppe;

	priv->emulation = dev_read_bool(dev, "qti,emulation");

	priv->hw.iobase = (phys_addr_t)dev_read_addr_name(dev, "edma_hw");
	if (priv->hw.iobase == FDT_ADDR_T_NONE) {
		dev_err(dev, "edma_hw bus address not found\n");
		return -EINVAL;
	}

	ppe->base = (phys_addr_t)dev_read_addr_name(dev, "ppe_base");
	if (ppe->base == FDT_ADDR_T_NONE) {
		dev_err(dev, "ppe_base bus address not found\n");
		return -EINVAL;
	}

	priv->uniphy_base = dev_read_addr_size_name(dev, "uniphy_base",
				(fdt_addr_t *)&priv->uniphy_size);
	if ((priv->uniphy_base == FDT_ADDR_T_NONE) && (!priv->emulation)) {
		dev_err(dev, "uniphy_base bus address not found\n");
		return -EINVAL;
	}

	ppe->tdm_offset = dev_read_u32_default(dev, "tdm_offset", -1);
	if (ppe->tdm_offset == -1) {
		dev_err(dev, "tdm_offset not found\n");
		return -EINVAL;
	}

	priv->uniphy_50mhz = dev_read_u32_default(dev, "50mhz", 0);

	ppe->tdm_mode = dev_read_u32_default(dev, "tdm_mode", 0);
	ppe->no_reg = dev_read_u32_default(dev, "no_tdm_reg", 0);
	ppe->tm = dev_read_bool(dev, "tdm_tm_support");
	ppe->bridge_mode = dev_read_bool(dev, "bridge_mode");
	if (!ppe->bridge_mode)
		ppe->nbport = dev_read_u32_default(dev, "port", 0);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		struct port_info *port = NULL;

		snprintf(phy_handle, sizeof(phy_handle), "phy-handle%d", i);

		if (!dev_read_phandle_with_args(dev, phy_handle, NULL, 0, 0,
					&phandle_args)) {
			uint8_t uniphy_id = ofnode_read_u32_default(
						phandle_args.node,
						"uniphy_id", -1);
			if (-1 == uniphy_id)
				continue;

			if ((ipq_uniphy) && (uniphy_id <
						CONFIG_ETH_MAX_UNIPHY)) {
				if(readl(ipq_uniphy[uniphy_id].reg) & (1 <<
						ipq_uniphy[uniphy_id].bit)) {
					printf("UNIPHY%d is Disabled\n",
							uniphy_id);
					continue;
				}
			}

			port = malloc_cache_aligned(sizeof(struct port_info));
			if (!port)
				return -ENOMEM;

			memset(port, 0, sizeof(struct port_info));

			port->uniphy_id = uniphy_id;
			port->node = phandle_args.node;
			port->pnode = ofnode_get_parent(phandle_args.node);
			port->phyaddr = ofnode_read_u32_default(
						phandle_args.node,
						"reg", -1);
			port->phy_id = ofnode_read_u32_default(
						phandle_args.node,
						"phy_id", -1);
			port->id = ofnode_read_u32_default(
						phandle_args.node,
						"id", -1);
			port->uniphy_type = ofnode_read_u32_default(
						phandle_args.node,
						"uniphy_type", 0);
			port->max_speed = ofnode_read_u32_default(
						phandle_args.node,
						"max_speed", -1);
			port->isforce_speed = ofnode_read_bool(
						phandle_args.node,
						"force-speed");
			port->xgmac = ofnode_read_bool(
						phandle_args.node,
						"xgmac");
			port->i2c_bus = ofnode_read_u32_default(
						phandle_args.node,
						"i2c-bus", 0);
			port->interface = ofnode_read_phy_mode(
						phandle_args.node);
			port->support_25M =  ofnode_read_bool(
						phandle_args.node,
						"support_25M");
			gpio_request_by_name_nodev(phandle_args.node,
							"phy-reset-gpio", 0,
							&port->rst_gpio,
							GPIOD_IS_OUT);
			priv->port[port_count++] = port;
		}
	}

	return 0;
}

static const struct udevice_id ipq_eth_ids[] = {
	{ .compatible = "qti,ipq-nss-switch",
          .data = (ulong)&ipq_edma_config },
	{ }
};

U_BOOT_DRIVER(eth_ipq) = {
	.name	= "eth_ipq",
	.id	= UCLASS_ETH,
	.of_match = ipq_eth_ids,
	.of_to_plat = ipq_eth_ofdata_to_platdata,
	.bind	= ipq_eth_bind,
	.probe	= ipq_eth_probe,
	.remove	= ipq_eth_remove,
	.ops	= &ipq_eth_ops,
	.priv_auto = sizeof(struct ipq_eth_dev),
	.plat_auto = sizeof(struct eth_pdata),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};

#ifdef CONFIG_PHY_AQUANTIA
static int do_aqloadfw(struct cmd_tbl *cmdtp, int flag, int argc,
			char *const argv[])
{
	struct udevice *dev = NULL;
	struct ipq_eth_dev *priv = NULL;
	int i;
	uint8_t phyaddr;

	if (argc != 2)
		return CMD_RET_USAGE;

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
	if (!initr_net())
		return CMD_RET_SUCCESS;
#endif

	dev = eth_get_dev_by_name("nss-switch");
	if (dev == NULL)
		return CMD_RET_FAILURE;

	priv = dev_get_priv(dev);
	if (priv == NULL)
		return CMD_RET_FAILURE;

	phyaddr = simple_strtoul(argv[1], NULL, 16);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		struct port_info *port = priv->port[i];
		if (port == NULL)
			continue;

		if (port->phy_id != AQ_PHY_TYPE)
			continue;

		if (port->phyaddr != phyaddr)
			continue;

		if (!ipq_aquantia_load_fw(port->phydev)) {
			mdelay(100);
		}

		break;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	aq_load_fw, 2, 0, do_aqloadfw,
	"Load firmware to AQ port",
	"phy_addr --> phy address of AQ port\n"
	);
#endif /* CONFIG_PHY_AQUANTIA */
