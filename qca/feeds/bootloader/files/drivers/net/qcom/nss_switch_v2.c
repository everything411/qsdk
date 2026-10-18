// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2019, 2021, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include "nss-switch.h"
#include <command.h>

DECLARE_GLOBAL_DATA_PTR;

#define REG_DELAY			10
#define RESET_DELAY			10

static int tftp_acl_our_port;

/* Runtime PORT_BRIDGE_CTRL layout detection and helper functions */
static enum ppe_bridge_ctrl_layout current_layout = PPE_BRIDGE_CTRL_LAYOUT_UNKNOWN;
int mac_speed_config[] = {10, 100, 1000, 10000, 2500, 5000};
/* Per-port scheduler configuration populated from DTS */
static struct port_scheduler_cfg port_sched_cfg[9];

#if IS_ENABLED(CONFIG_MDIO_QCOM_I2C)
extern struct mii_dev *qcom_mdio_i2c_alloc(struct udevice *i2c_bus,
					   int phy_addr);
#endif /* CONFIG_MDIO_QCOM_I2C */
int ipq_aquantia_load_fw(struct phy_device *phydev);

#if IS_ENABLED(CONFIG_CMD_NET) && IS_ENABLED(CONFIG_ETH_SKIP_INIT_R)
extern int initr_net(void);
#endif

/* Base address for UNIPHY registers - configurable */
static phys_addr_t uniphy_base_addr = 0x7A00000;  /* Default base address */

/* Current CSR version - set at boot */
static enum csr_version current_csr_version = CSR_VERSION_V1;

/* Current reset version - set at boot via uniphy_get_reset_version() */
static enum reset_version current_reset_version = RESET_VERSION_V1;

/* ========================================================================
 * CSR Version Management
 * ========================================================================
 */
/**
 * uniphy_get_reset_version - weak default: USXGMII port reset via USRA_RST
 *
 * SoCs that use the QP_USXG_RESET active-LOW path override this in their
 * *_port_config.c to return RESET_VERSION_V2.
 */
__weak enum reset_version uniphy_get_reset_version(void)
{
	return RESET_VERSION_V1;
}

void uniphy_set_base_addr(phys_addr_t base_addr)
{
	uniphy_base_addr = base_addr;
}

 /* ========================================================================
 * V1 (Legacy) CSR Implementation
 * ========================================================================
 */

/**
 * csr_write_v1 - V1 CSR write
 * Uses 0x83FC indirect register and 0x20 data offset
 */
static void csr_write_v1(int uniphy_index, u32 addr, u32 value)
{
	uintptr_t addr_h, addr_l, ahb_h, ahb_l, phy;

	phy = uniphy_index << UNIPHY_PHY_SHIFT;

	/* Step 1: Write high address bits to indirect register */
	addr_h = (addr & 0xffffff) >> 8;
	ahb_h = (uniphy_base_addr | phy) + V1_CSR_INDIRECT_REG;
	writel(addr_h, ahb_h);

	/* Step 2: Write data via data offset */
	addr_l = ((addr & CSR_INDIRECT_LOW_ADDR) << 2) |
		 (V1_CSR_DATA_OFFSET << 0xa);
	ahb_l = (addr_l & 0xffff) | (uniphy_base_addr | phy);
	writel(value, ahb_l);
}

/**
 * csr_read_v1 - V1 CSR read
 * Uses 0x83FC indirect register and 0x20 data offset
 */
static u32 csr_read_v1(int uniphy_index, u32 addr)
{
	uintptr_t addr_h, addr_l, ahb_h, ahb_l, phy;

	phy = uniphy_index << UNIPHY_PHY_SHIFT;

	/* Step 1: Write high address bits to indirect register */
	addr_h = (addr & 0xffffff) >> 8;
	ahb_h = (uniphy_base_addr | phy) + V1_CSR_INDIRECT_REG;
	writel(addr_h, ahb_h);

	/* Step 2: Read data via data offset */
	addr_l = ((addr & CSR_INDIRECT_LOW_ADDR) << 2) |
		 (V1_CSR_DATA_OFFSET << 0xa);
	ahb_l = (addr_l & 0xffff) | (uniphy_base_addr | phy);
	return readl(ahb_l);
}

/* ========================================================================
 * V2 (New) CSR Implementation
 * ========================================================================
 */

/**
 * csr_write_v2 - V2 new CSR write with type detection
 * Handles CSR0 (direct), CSR1, and CSR2 (indirect) based on encoded address
 */
static void csr_write_v2(int uniphy_index, u32 reg_addr, u32 value)
{
	u32 csr_type, actual_addr;
	uintptr_t addr_h, addr_l, ahb_h, ahb_l, phy;
	u32 indirect_reg, data_offset;

	/* Extract CSR type and actual address */
	csr_type = (reg_addr & UNIPHY_CSR_BLOCK_MASK) >> UNIPHY_CSR_BLOCK_SHIFT;
	actual_addr = reg_addr & UNIPHY_REG_ADDR_MASK;
	phy = uniphy_index << UNIPHY_PHY_SHIFT;

	switch (csr_type) {
	case 0:  /* CSR0 - Direct access */
		ahb_l = (uniphy_base_addr | phy) + actual_addr;
		writel(value, ahb_l);
		break;

	case 1:  /* CSR1 - Indirect access */
		indirect_reg = V2_CSR1_INDIRECT_REG;
		data_offset = V2_CSR1_DATA_OFFSET;
		goto indirect_access;

	case 2:  /* CSR2 - Indirect access */
		indirect_reg = V2_CSR2_INDIRECT_REG;
		data_offset = V2_CSR2_DATA_OFFSET;

indirect_access:
		/* Step 1: Write high address bits */
		addr_h = (actual_addr & 0xffffff) >> 8;
		ahb_h = (uniphy_base_addr | phy) + indirect_reg;
		writel(addr_h, ahb_h);

		/* Step 2: Write data */
		addr_l = ((actual_addr & CSR_INDIRECT_LOW_ADDR) << 2) |
			 (data_offset << 0xa);
		ahb_l = (addr_l & 0xffff) | (uniphy_base_addr | phy);
		writel(value, ahb_l);
		break;

	default:
		printf("UNIPHY CSR: Invalid CSR type %d\n", csr_type);
		break;
	}
}

/**
 * csr_read_v2 - V2 new CSR read with type detection
 * Handles CSR0 (direct), CSR1, and CSR2 (indirect) based on encoded address
 */
static u32 csr_read_v2(int uniphy_index, u32 reg_addr)
{
	u32 csr_type, actual_addr;
	uintptr_t addr_h, addr_l, ahb_h, ahb_l, phy;
	u32 indirect_reg, data_offset;

	/* Extract CSR type and actual address */
	csr_type = (reg_addr & UNIPHY_CSR_BLOCK_MASK) >> UNIPHY_CSR_BLOCK_SHIFT;
	actual_addr = reg_addr & UNIPHY_REG_ADDR_MASK;
	phy = uniphy_index << UNIPHY_PHY_SHIFT;

	switch (csr_type) {
	case 0:  /* CSR0 - Direct access */
		ahb_l = (uniphy_base_addr | phy) + actual_addr;
		return readl(ahb_l);

	case 1:  /* CSR1 - Indirect access */
		indirect_reg = V2_CSR1_INDIRECT_REG;
		data_offset = V2_CSR1_DATA_OFFSET;
		goto indirect_access;

	case 2:  /* CSR2 - Indirect access */
		indirect_reg = V2_CSR2_INDIRECT_REG;
		data_offset = V2_CSR2_DATA_OFFSET;

indirect_access:
		/* Step 1: Write high address bits */
		addr_h = (actual_addr & 0xffffff) >> 8;
		ahb_h = (uniphy_base_addr | phy) + indirect_reg;
		writel(addr_h, ahb_h);

		/* Step 2: Read data */
		addr_l = ((actual_addr & CSR_INDIRECT_LOW_ADDR) << 2) |
			 (data_offset << 0xa);
		ahb_l = (addr_l & 0xffff) | (uniphy_base_addr | phy);
		return readl(ahb_l);

	default:
		printf("UNIPHY CSR: Invalid CSR type %d\n", csr_type);
		return 0;
	}
}

/* ========================================================================
 * Unified API - Works for Both V1 and V2
 * ========================================================================
 */

void csr_write(int uniphy_index, u32 addr, u32 value)
{
	if (current_csr_version == CSR_VERSION_V2) {
		/* V2: Use new CSR flow with encoded address */
		csr_write_v2(uniphy_index, addr, value);
	} else {
		/* V1: Strip encoding and use V1 method */
		u32 raw_addr = addr & UNIPHY_REG_ADDR_MASK;

		csr_write_v1(uniphy_index, raw_addr, value);
	}
}

u32 csr_read(int uniphy_index, u32 addr)
{
	u32 raw_addr;

	if (current_csr_version == CSR_VERSION_V2) {
		/* V2: Use new CSR flow with encoded address */
		return csr_read_v2(uniphy_index, addr);
	}

	/* V1: Strip encoding and use V1 method */
	raw_addr = addr & UNIPHY_REG_ADDR_MASK;

	return csr_read_v1(uniphy_index, raw_addr);
}

/*
 * Default (weak) port_init - SoCs that need no per-port init can omit their own.
 * SoCs that do need it (e.g. IPQ9650) provide a strong override in *_port_config.c.
 */
void __weak port_init(struct port_info *port)
{
}

/*
 * Uniphy calibration function
 */
int ppe_uniphy_calibration(struct port_info *port)
{
	int retries = 100, calibration_done = 0;
	u32 reg_value = 0;
	uintptr_t reg = port->uniphy_base + PPE_UNIPHY_OFFSET_CALIB_4;

	while (calibration_done != UNIPHY_CALIBRATION_DONE) {
		mdelay(1);
		if (retries-- == 0) {
			printf("uniphy calibration time out!\n");
			return -ETIMEDOUT;
		}
		reg_value = readl(reg);
		calibration_done = (reg_value >> 0x7) & 0x1;
	}

	return 0;
}

/*
 * Uniphy calibration function (QSERDES-based SerDes)
 */
int ppe_uniphy_serdes_calibration(struct port_info *port)
{
	int retries = UNIPHY_POLLING_TIMEOUT;
	u32 reg_value = 0;
	u32 index = port->uniphy_id;
	phys_addr_t base = port->uniphy_base;

	/* Step 1: Set UNIPHY_START bit to 1 */
	/* Address depends on uniphy index: 0x5AC for uniphy0, 0x588 for others */
	if (index == 0) {
		reg_value = readl(base + PCS0_UNIPHY_OPTION_3_ADDRESS);
		reg_value |= PCS_UNIPHY_OPTION_3_UNIPHY_START_BIT;
		writel(reg_value, base + PCS0_UNIPHY_OPTION_3_ADDRESS);
	} else {
		reg_value = readl(base + PCS_UNIPHY_OPTION_3_ADDRESS);
		reg_value |= PCS_UNIPHY_OPTION_3_UNIPHY_START_BIT;
		writel(reg_value, base + PCS_UNIPHY_OPTION_3_ADDRESS);
	}

	/* Step 2: Wait for traffic state to be 0xF */
	while ((readl(base + QSERDES_RX_EXT_RO_POWER_STATE_ADDRESS) & 0xF) !=
	       UNIPHY_POWER_STATE_DONE) {
		mdelay(UNIPHY_POLLING_DELAY);
		if (retries-- == 0) {
			printf("uniphy %d calibration time out!\n", index);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

/*
 * Port reset helper function
 */
void ipq_port_reset(struct reset_ctl *rst, bool set)
{
	if (set)
		reset_assert(rst);
	else
		reset_deassert(rst);
}

static void ppe_uniphy_reset(struct port_info *port, bool issoft, bool set)
{
	struct udevice *dev;
	struct reset_ctl rst;
	int ret, len;
	char name[64];

	if (!port || !port->dev)
		return;

	dev = port->dev;

	len = snprintf(name, sizeof(name), "uniphy%d_%s", port->uniphy_id,
		       issoft ? "sys_rst" : "xpcs_rst");
	if (len < 0 || len >= sizeof(name))
		return;

	ret = reset_get_by_name(dev, name, &rst);
	if (ret) {
		len = snprintf(name, sizeof(name), "uniphy%d_%s", port->uniphy_id,
			       issoft ? "srst" : "xrst");
		if (len < 0 || len >= sizeof(name))
			return;

		ret = reset_get_by_name(dev, name, &rst);
	}

	if (!ret)
		ipq_port_reset(&rst, set);

	if (issoft) {
		len = snprintf(name, sizeof(name), "uniphy_port%d_tx_rst", port->id);
		if (len < 0 || len >= sizeof(name))
			return;

		ret = reset_get_by_name(dev, name, &rst);
		if (ret) {
			len = snprintf(name, sizeof(name), "uniphy_port%d_tx", port->id);
			if (len < 0 || len >= sizeof(name))
				return;

			ret = reset_get_by_name(dev, name, &rst);
		}

		if (!ret)
			ipq_port_reset(&rst, set);

		len = snprintf(name, sizeof(name), "uniphy_port%d_rx_rst", port->id);
		if (len < 0 || len >= sizeof(name))
			return;

		ret = reset_get_by_name(dev, name, &rst);
		if (ret) {
			len = snprintf(name, sizeof(name), "uniphy_port%d_rx", port->id);
			if (len < 0 || len >= sizeof(name))
				return;

			ret = reset_get_by_name(dev, name, &rst);
		}

		if (!ret)
			ipq_port_reset(&rst, set);
	}
}

/* Helper functions for common UNIPHY_MODE_CTRL patterns */
static inline u32 uniphy_mode_psgmii_25m(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_ch0_psgmii_qsgmii = 1;
	mode.bf.newaddedfromhere_ch0_mode_ctrl_25m = 0x2;
	return mode.val; /* Expected: 0x220 */
}

static inline u32 uniphy_mode_qsgmii_25m(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_ch0_qsgmii_sgmii = 1;
	mode.bf.newaddedfromhere_ch0_mode_ctrl_25m = 0x2;
	return mode.val; /* Expected: 0x120 */
}

static inline u32 uniphy_mode_sg_fiber(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_sg_mode = 1;
	return mode.val; /* Expected: 0x400 */
}

static inline u32 uniphy_mode_sg_25m(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_sg_mode = 1;
	mode.bf.newaddedfromhere_ch0_mode_ctrl_25m = 0x2;
	return mode.val; /* Expected: 0x420 */
}

static inline u32 uniphy_mode_sgplus_25m(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_sgplus_mode = 1;
	mode.bf.newaddedfromhere_ch0_mode_ctrl_25m = 0x2;
	return mode.val; /* Expected: 0x820 */
}

static inline u32 uniphy_mode_xpcs_autoneg_25m(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_xpcs_mode = 1;
	mode.bf.newaddedfromhere_ch0_mode_ctrl_25m = 0x2;
	mode.bf.newaddedfromhere_ch0_autoneg_mode = 1;
	return mode.val; /* Expected: 0x1021 */
}

static inline u32 uniphy_mode_uxgmii_25m(void)
{
	union uniphy_mode_ctrl_u mode = {0};

	mode.bf.newaddedfromhere_xpcs_mode = 1;
	mode.bf.newaddedfromhere_usxg_en = 1;
	mode.bf.newaddedfromhere_ch0_mode_ctrl_25m = 0x2;
	mode.bf.newaddedfromhere_ch0_autoneg_mode = 1;
	return mode.val; /* Expected: 0x3021 */
}

/*
 * PSGMII mode configuration
 */
static void ppe_uniphy_psgmii_mode_set(struct port_info *port)
{
	phys_addr_t mode_ctrl_reg = port->uniphy_base + PPE_UNIPHY_MODE_CONTROL;
	u32 reg_val_to_write;

	/* Pre-read to ensure register access path is valid */
	(void)readl(mode_ctrl_reg);

	/* Reset sequence */
	ppe_uniphy_reset(port, false, true);
	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	/* Program PSGMII 25MHz mode */
	reg_val_to_write = uniphy_mode_psgmii_25m();
	writel(reg_val_to_write, mode_ctrl_reg);

	/* Final XPCS reset and calibration */
	ppe_uniphy_reset(port, false, true);
	ppe_uniphy_calibration(port);
}

/*
 * QSGMII mode configuration
 */
static void ppe_uniphy_qsgmii_mode_set(struct port_info *port)
{
	writel(uniphy_mode_qsgmii_25m(), port->uniphy_base + PPE_UNIPHY_MODE_CONTROL);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);
}

/*
 * Set uniphy force mode
 */
void ppe_uniphy_set_forcemode(struct port_info *port)
{
	u32 reg_value;

	reg_value = readl(port->uniphy_base +
			  UNIPHY_DEC_CHANNEL_0_INPUT_OUTPUT_4);
	reg_value |= UNIPHY_FORCE_SPEED_25M;
	writel(reg_value,
	       port->uniphy_base + UNIPHY_DEC_CHANNEL_0_INPUT_OUTPUT_4);
}

/*
 * Set uniphy reference clock to 25MHz
 */
void ppe_uniphy_refclk_set_25M(struct port_info *port)
{
	u32 reg_value;

	reg_value = readl(port->uniphy_base +
			  UNIPHY1_CLKOUT_50M_CTRL_OPTION);
	reg_value |= (UNIPHY1_CLKOUT_50M_CTRL_CLK50M_DIV2_SEL |
		      UNIPHY1_CLKOUT_50M_CTRL_50M_25M_EN);
	writel(reg_value,
	       port->uniphy_base + UNIPHY1_CLKOUT_50M_CTRL_OPTION);
}

/*
 * SGMII mode configuration
 */
static void ppe_uniphy_sgmii_mode_set(struct port_info *port)
{
	phys_addr_t base = port->uniphy_base;
	u32 reg_value;

	writel(UNIPHY_MISC_SRC_PHY_MODE, base +
	       UNIPHY_MISC_SOURCE_SELECTION_REG_OFFSET);

	if (port->uniphy_mode == PORT_WRAPPER_SGMII_PLUS)
		reg_value = UNIPHY_MISC2_REG_SGMII_PLUS_MODE;
	else
		reg_value = UNIPHY_MISC2_REG_SGMII_MODE;

	writel(reg_value, base + UNIPHY_MISC2_REG_OFFSET);

	writel(UNIPHY_PLL_RESET_REG_VALUE,
	       base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(REG_DELAY);

	writel(UNIPHY_PLL_RESET_REG_DEFAULT_VALUE, base +
	       UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(REG_DELAY);

	switch (port->uniphy_mode) {
	case PORT_WRAPPER_SGMII_FIBER:
		writel(uniphy_mode_sg_fiber(), base + PPE_UNIPHY_MODE_CONTROL);
		break;
	case PORT_WRAPPER_SGMII0_RGMII4:
	case PORT_WRAPPER_SGMII1_RGMII4:
	case PORT_WRAPPER_SGMII4_RGMII4:
		writel(uniphy_mode_sg_25m(), base + PPE_UNIPHY_MODE_CONTROL);
		break;
	case PORT_WRAPPER_SGMII_PLUS:
		writel(uniphy_mode_sgplus_25m(), base + PPE_UNIPHY_MODE_CONTROL);
		break;
	default:
		printf("SGMII Config. wrongly\n");
		break;
	}

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);
}

/*
 * 10G-R link up check
 */
static int ppe_uniphy_10g_r_linkup(u32 uniphy_index)
{
	u32 reg_value = 0;
	u32 retries = 100, linkup = 0;

	while (linkup != UNIPHY_10GR_LINKUP) {
		mdelay(1);
		if (retries-- == 0)
			return -ETIMEDOUT;
		reg_value = csr_read(uniphy_index,
				     CSR1_ADDR(SR_XS_PCS_KR_STS1_ADDRESS));
		linkup = (reg_value >> 12) & UNIPHY_10GR_LINKUP;
	}
	mdelay(REG_DELAY);
	return 0;
}

/*
 * 10G-R mode configuration
 */
static void ppe_uniphy_10g_r_mode_set(struct port_info *port)
{
	ppe_uniphy_reset(port, false, true);

	writel(uniphy_mode_xpcs_autoneg_25m(), port->uniphy_base + PPE_UNIPHY_MODE_CONTROL);
	writel(0x1C0, port->uniphy_base + UNIPHY_INSTANCE_LINK_DETECT);

	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	ppe_uniphy_calibration(port);
	ppe_uniphy_reset(port, false, false);
}

/**
 * uniphy_rxeq_status_check() - Poll RXEQ engine done status (weak default)
 * @uniphy_index: UNIPHY instance number (0, 1, or 2)
 *
 * Default no-op stub for SoCs that do not use QSERDES-based SerDes.
 * IPQ9650 overrides this with a strong implementation in ipq9650_port_config.c
 * that polls QSERDES_RX_EXT_RO_PMAD_RXEQ_STATUS bit[1] (RXEQ_ENGINE_DONE).
 *
 * Return: 0 (success, no check needed)
 */
int __weak uniphy_rxeq_status_check(int uniphy_index)
{
	return 0;
}

/*
 * USXGMII mode configuration
 */
static void ppe_uniphy_usxgmii_mode_set(struct port_info *port)
{
	u32 index = port->uniphy_id;
	phys_addr_t base = port->uniphy_base;
	u32 reg_value;

	/* Configure UNIPHY MISC and reset PLL */
	writel(UNIPHY_MISC2_REG_VALUE, base + UNIPHY_MISC2_REG_OFFSET);

	writel(UNIPHY_PLL_RESET_REG_VALUE, base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(RESET_DELAY);
	writel(UNIPHY_PLL_RESET_REG_DEFAULT_VALUE, base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(REG_DELAY);

	uniphy_pma_init_setting(port, PORT_WRAPPER_USXGMII, 2, A_FALSE);

	/* Assert resets: keep XPCS in reset, do software reset sequence */
	ppe_uniphy_reset(port, false, true);
	mdelay(RESET_DELAY);

	/* Program XPCS auto-neg mode (25M ref) */
	writel(uniphy_mode_xpcs_autoneg_25m(), base + PPE_UNIPHY_MODE_CONTROL);

	/* Software reset */
	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);
	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	/* Calibration and release XPCS reset */
	if (port->calibrate)
		port->calibrate(port);
	else
		ppe_uniphy_calibration(port);

        ppe_uniphy_reset(port, false, false);
        mdelay(RESET_DELAY);

	/* Wait 10G-R link up */
	ppe_uniphy_10g_r_linkup(index);

	uniphy_rxeq_status_check(index);

	/* Enable USXGMII in XPCS */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS));
	reg_value |= USXG_EN;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS), reg_value);

	/* For UNIPHY0, select GMII source from XPCS */
	if (index == 0) {
		reg_value = readl(base + UNIPHYQP_USXG_OPITON1);
		reg_value |= GMII_SRC_SEL;
		writel(reg_value, base + UNIPHYQP_USXG_OPITON1);
	}

	/* Enable autoneg complete interrupt and 10M/100M 8-bit MII width */
	reg_value = csr_read(index, CSR1_ADDR(VR_MII_AN_CTRL_ADDRESS));
	reg_value |= MII_AN_INTR_EN | MII_CTRL;
	csr_write(index, CSR1_ADDR(VR_MII_AN_CTRL_ADDRESS), reg_value);

	/* Advertise autoneg ability: 10G speed, full duplex */
	reg_value = csr_read(index, CSR1_ADDR(SR_MII_CTRL_ADDRESS));
	reg_value |= AN_ENABLE;
	reg_value &= ~SS5;
	reg_value |= SS6 | SS13 | DUPLEX_MODE;
	csr_write(index, CSR1_ADDR(SR_MII_CTRL_ADDRESS), reg_value);

	/* Enable EEE transparent mode and configure timers */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS));
	reg_value |= SIGN_BIT | MULT_FACT_100NS;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS), reg_value);

	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_TXTIMER_ADDRESS));
	reg_value |= UNIPHY_XPCS_TSL_TIMER | UNIPHY_XPCS_TLU_TIMER | UNIPHY_XPCS_TWL_TIMER;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_TXTIMER_ADDRESS), reg_value);

	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_RXTIMER_ADDRESS));
	reg_value |= UNIPHY_XPCS_100US_TIMER | UNIPHY_XPCS_TWR_TIMER;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_RXTIMER_ADDRESS), reg_value);

	/* Transparent LPI mode and LPI pattern enable */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL1_ADDRESS));
	reg_value |= TRN_LPI | TRN_RXLPI;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL1_ADDRESS), reg_value);

	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS));
	reg_value |= LRX_EN | LTX_EN;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS), reg_value);
}
/*
 * Default (weak) UQXGMII mode control value.
 * Returns XPCS autoneg 25M mode (0x1021) for all SoCs.
 * SoCs that need USXG_EN set (e.g. IPQ9650) provide a strong override
 * in their *_port_config.c returning 0x3021.
 */
u32 __weak ppe_uniphy_uxgmii_mode_ctrl_val(void)
{
	return uniphy_mode_xpcs_autoneg_25m();
}

int __weak uniphy_pma_init_setting(struct port_info *port, u32 uniphy_mode,
					u32 dfe_mode, bool is_long)
{
	return 0;
}

/*
 * UQXGMII/UDXGMII combined mode configuration
 */
static void ppe_uniphy_uxgmii_mode_set(struct port_info *port)
{
	u32 index = port->uniphy_id;
	phys_addr_t base = port->uniphy_base;
	u32 reg_value = 0;

	/* Step 1: Configure UNIPHY MISC2 register */
	writel(UNIPHY_MISC2_REG_VALUE, base + UNIPHY_MISC2_REG_OFFSET);

	/* Step 2: PLL Reset sequence */
	writel(UNIPHY_PLL_RESET_REG_VALUE, base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(RESET_DELAY);

	writel(UNIPHY_PLL_RESET_REG_DEFAULT_VALUE,
	       base + UNIPHY_PLL_RESET_REG_OFFSET);
	mdelay(RESET_DELAY);

	uniphy_pma_init_setting(port, PORT_WRAPPER_UQXGMII, 2, A_FALSE);

	/* Step 3: Assert XPCS reset (keep XPCS in reset) */
	ppe_uniphy_reset(port, false, true);
	mdelay(RESET_DELAY);

	/* Step 4: Program XPCS mode control register (SoC-specific mode value) */
	writel(ppe_uniphy_uxgmii_mode_ctrl_val(), base + PPE_UNIPHY_MODE_CONTROL);

	/* Step 5: Configure GMII source selection from XPCS */
	reg_value = readl(base + UNIPHYQP_USXG_OPITON1);
	reg_value |= GMII_SRC_SEL;
	writel(reg_value, base + UNIPHYQP_USXG_OPITON1);

	/* Step 6: Software reset sequence */
	ppe_uniphy_reset(port, true, true);
	mdelay(RESET_DELAY);

	ppe_uniphy_reset(port, true, false);
	mdelay(RESET_DELAY);

	/* Step 7: Perform calibration */
	if (port->calibrate)
		port->calibrate(port);
	else
		ppe_uniphy_calibration(port);

	/* Step 8: Release XPCS reset */
	ppe_uniphy_reset(port, false, false);
	mdelay(RESET_DELAY);

	/* Step 9: Wait for 10G-R link up */
	ppe_uniphy_10g_r_linkup(index);

	uniphy_rxeq_status_check(index);

	/* Step 10: Enable USXGMII in XPCS */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS));
	reg_value |= USXG_EN;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS), reg_value);

	/* Step 11: Set UQXGMII mode */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_KR_CTRL_ADDRESS));
	reg_value |= USXG_MODE;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_KR_CTRL_ADDRESS), reg_value);

	/* Step 12: Set AM alignment marker interval */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_DIG_STS_ADDRESS));
	reg_value |= AM_COUNT;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_DIG_STS_ADDRESS), reg_value);

	/* Step 13: XPCS software reset */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS));
	reg_value |= VR_RST;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS), reg_value);

	/* Step 14: Configure auto-neg control for all 4 channels */
	reg_value = csr_read(index, CSR1_ADDR(VR_MII_AN_CTRL_ADDRESS));
	reg_value |= MII_AN_INTR_EN;
	if (port->gmac_type == XGMAC)
		reg_value |= MII_CTRL;
	csr_write(index, CSR1_ADDR(VR_MII_AN_CTRL_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(VR_MII_AN_CTRL_CHANNEL1_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(VR_MII_AN_CTRL_CHANNEL2_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(VR_MII_AN_CTRL_CHANNEL3_ADDRESS), reg_value);

	/* Step 15: Disable TICD (IPG check) for all 4 channels */
	reg_value = csr_read(index, CSR1_ADDR(VR_XAUI_MODE_CTRL_ADDRESS));
	reg_value |= IPG_CHECK;
	csr_write(index, CSR1_ADDR(VR_XAUI_MODE_CTRL_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(VR_XAUI_MODE_CTRL_CHANNEL1_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(VR_XAUI_MODE_CTRL_CHANNEL2_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(VR_XAUI_MODE_CTRL_CHANNEL3_ADDRESS), reg_value);

	/* Step 16: Enable uniphy autoneg ability and usxgmii 10g speed and full duplex
	 * for all 4 channels
	 */
	reg_value = csr_read(index, CSR1_ADDR(SR_MII_CTRL_ADDRESS));
	reg_value |= AN_ENABLE;
	reg_value &= ~SS5;
	reg_value |= SS6 | SS13 | DUPLEX_MODE;
	reg_value = 0x1104;
	csr_write(index, CSR1_ADDR(SR_MII_CTRL_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(SR_MII_CTRL_CHANNEL1_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(SR_MII_CTRL_CHANNEL2_ADDRESS), reg_value);
	csr_write(index, CSR1_ADDR(SR_MII_CTRL_CHANNEL3_ADDRESS), reg_value);

	/* Step 17: Enable uniphy EEE transparent mode and configure EEE related timer value */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS));
	reg_value |= SIGN_BIT | MULT_FACT_100NS;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS), reg_value);

	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_TXTIMER_ADDRESS));
	reg_value |= UNIPHY_XPCS_TSL_TIMER | UNIPHY_XPCS_TLU_TIMER |
		     UNIPHY_XPCS_TWL_TIMER;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_TXTIMER_ADDRESS), reg_value);

	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_RXTIMER_ADDRESS));
	reg_value |= UNIPHY_XPCS_100US_TIMER | UNIPHY_XPCS_TWR_TIMER;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_RXTIMER_ADDRESS), reg_value);

	/* Step 18: Transparent LPI mode and LPI pattern enable */
	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL1_ADDRESS));
	reg_value |= TRN_LPI | TRN_RXLPI;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL1_ADDRESS), reg_value);

	reg_value = csr_read(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS));
	reg_value |= LRX_EN | LTX_EN;
	csr_write(index, CSR1_ADDR(VR_XS_PCS_EEE_MCTRL0_ADDRESS), reg_value);
}
/*
 * Main uniphy mode configuration function
 */
void ppe_uniphy_mode_set(struct port_info *port)
{
	switch (port->uniphy_mode) {
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
	case PORT_WRAPPER_UQXGMII_3CHANNELS:
	case PORT_WRAPPER_UDXGMII:
		ppe_uniphy_uxgmii_mode_set(port);
		break;
	default:
		break;
	}
}

/*
 * USXGMII autonegotiation completion check
 */
void ppe_uniphy_usxgmii_autoneg_completed(int uniphy_index)
{
	u32 autoneg_complete = 0, retries = 100;
	u32 reg_value = 0;

	while (autoneg_complete != 0x1) {
		mdelay(1);
		if (retries-- == 0)
			return;

		reg_value = csr_read(uniphy_index, CSR1_ADDR(VR_MII_AN_INTR_STS));
		autoneg_complete = reg_value & 0x1;
	}
	reg_value &= ~CL37_ANCMPLT_INTR;
	csr_write(uniphy_index, CSR1_ADDR(VR_MII_AN_INTR_STS), reg_value);
}

/*
 * USXGMII speed configuration
 */
void ppe_uniphy_usxgmii_speed_set(int portid, int uniphy_index, int speed)
{
	u32 reg_value = 0;
	u32 mii_ctrl_address = SR_MII_CTRL_ADDRESS;

	if (uniphy_index == 0 && portid != 1) {
		switch (portid) {
		case 2:
			mii_ctrl_address = SR_MII_CTRL_CHANNEL1_ADDRESS;
			break;
		case 3:
			mii_ctrl_address = SR_MII_CTRL_CHANNEL2_ADDRESS;
			break;
		case 4:
			mii_ctrl_address = SR_MII_CTRL_CHANNEL3_ADDRESS;
			break;
		default:
			break;
		}
	}

	reg_value = csr_read(uniphy_index, CSR1_ADDR(mii_ctrl_address));
	reg_value |= DUPLEX_MODE;

	switch (speed) {
	case 0:
		reg_value &= ~SS5;
		reg_value &= ~SS6;
		reg_value &= ~SS13;
		break;
	case 1:
		reg_value &= ~SS5;
		reg_value &= ~SS6;
		reg_value |= SS13;
		break;
	case 2:
		reg_value &= ~SS5;
		reg_value |= SS6;
		reg_value &= ~SS13;
		break;
	case 3:
		reg_value &= ~SS5;
		reg_value |= SS6;
		reg_value |= SS13;
		break;
	case 4:
		reg_value |= SS5;
		reg_value &= ~SS6;
		reg_value &= ~SS13;
		break;
	case 5:
		reg_value |= SS5;
		reg_value &= ~SS6;
		reg_value |= SS13;
		break;
	}

	csr_write(uniphy_index, CSR1_ADDR(mii_ctrl_address), reg_value);
}

/*
 * USXGMII duplex configuration
 */
void ppe_uniphy_usxgmii_duplex_set(int uniphy_index, int duplex)
{
	u32 reg_value = 0;

	reg_value = csr_read(uniphy_index, CSR1_ADDR(SR_MII_CTRL_ADDRESS));

	if (duplex & 0x1)
		reg_value |= DUPLEX_MODE;
	else
		reg_value &= ~DUPLEX_MODE;

	csr_write(uniphy_index, CSR1_ADDR(SR_MII_CTRL_ADDRESS), reg_value);
}

/*
 * USXGMII port reset
 *
 * Ported from SSDK adpt_hppe_uniphy_usxgmii_port_reset().
 * Port-specific reset sequence selected by current_reset_version:
 *
 * RESET_VERSION_V2 - QP_USXG_RESET @ 0x630 (CSR0 direct):
 *   Active-LOW RST_N bits - assert (clear) then de-assert (set).
 *   port 1 -> bit 0 (QP_USXG_RST_N_MAIN)
 *   port 2 -> bit 1 (QP_USXG_RST_N_P1)
 *   port 3 -> bit 2 (QP_USXG_RST_N_P2)
 *   port 4 -> bit 3 (QP_USXG_RST_N_P3)
 *
 * RESET_VERSION_V1 - VR_XS_PCS_DIG_CTRL1 @ 0x38000 (CSR1 indirect):
 *   USRA_RST (bit 10) is active-HIGH and self-clearing.
 *   For UQXGMII/UDXGMII, also reset per-channel VR_MII_DIG_CTRL1:
 *   port 2 -> VR_MII_DIG_CTRL1_CHANNEL1 (0x1a8000), USRA_RST_MII (bit 5)
 *   port 3 -> VR_MII_DIG_CTRL1_CHANNEL2 (0x1b8000), USRA_RST_MII (bit 5)
 *   port 4 -> VR_MII_DIG_CTRL1_CHANNEL3 (0x1c8000), USRA_RST_MII (bit 5)
 */
void ppe_uniphy_usxgmii_port_reset(int uniphy_index, int port_id,
				   int uniphy_mode)
{
	phys_addr_t base = uniphy_base_addr | (uniphy_index << UNIPHY_PHY_SHIFT);
	u32 reg_value;
	u32 rst_bit;
	u32 ch_addr;

	if (current_reset_version == RESET_VERSION_V2) {
		/* Map port to active-LOW reset bit in QP_USXG_RESET */
		switch (port_id) {
		case 1: rst_bit = QP_USXG_RST_N_MAIN; break;
		case 2: rst_bit = QP_USXG_RST_N_P1;   break;
		case 3: rst_bit = QP_USXG_RST_N_P2;   break;
		case 4: rst_bit = QP_USXG_RST_N_P3;   break;
		default: return;
		}

		/* Assert (clear active-LOW bit), delay, then de-assert */
		reg_value = readl(base + QP_USXG_RESET_ADDRESS);
		writel(reg_value & ~rst_bit, base + QP_USXG_RESET_ADDRESS);
		mdelay(1);
		writel(reg_value | rst_bit, base + QP_USXG_RESET_ADDRESS);
	} else {
		/* V1: set active-HIGH self-clearing USRA_RST in VR_XS_PCS_DIG_CTRL1 */
		reg_value = csr_read(uniphy_index,
				     CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS));
		csr_write(uniphy_index, CSR1_ADDR(VR_XS_PCS_DIG_CTRL1_ADDRESS),
			  reg_value | USRA_RST);
		mdelay(10);

		/* For UQXGMII/UDXGMII: also reset per-channel VR_MII_DIG_CTRL1 */
		if (uniphy_mode == PORT_WRAPPER_UQXGMII ||
		    uniphy_mode == PORT_WRAPPER_UQXGMII_3CHANNELS ||
		    uniphy_mode == PORT_WRAPPER_UDXGMII) {
			switch (port_id) {
			case 2: ch_addr = VR_MII_DIG_CTRL1_CHANNEL1_ADDRESS; break;
			case 3: ch_addr = VR_MII_DIG_CTRL1_CHANNEL2_ADDRESS; break;
			case 4: ch_addr = VR_MII_DIG_CTRL1_CHANNEL3_ADDRESS; break;
			default: ch_addr = 0; break;
			}

			if (ch_addr) {
				reg_value = csr_read(uniphy_index,
						     CSR1_ADDR(ch_addr));
				csr_write(uniphy_index, CSR1_ADDR(ch_addr),
					  reg_value | USRA_RST_MII);
			}
		}

		mdelay(10);
	}
}

void ppe_xgmac_configuration(phys_addr_t reg_base, u32 portid,
			     u32 speed, bool uxsgmii)
{
	u32 reg_value, gmacid = 0;
	u32 speed_bits;
	uintptr_t base, rx_config_addr, filter_addr;

	if (ipq_edma_config.port_to_gmacid) {
		gmacid = ipq_edma_config.port_to_gmacid(portid);
		if (gmacid == (u32)-1)
			return;
	} else {
		/* Generic formula: gmacid = portid - 1 */
		if (portid == 0)
			return;
		gmacid = portid - 1;
	}

	base = reg_base + PPE_SWITCH_NSS_SWITCH_XGMAC0 +
	       (gmacid * NSS_SWITCH_XGMAC_MAC_TX_CONFIGURATION);

	reg_value = readl(base);

	/*
	 * Speed Selection (SS) bits [31:29] must be programmed only once,
	 * after a hardware reset and before TX (TE) and RX (RE) are enabled.
	 *
	 * mac_speed encoding (input parameter):
	 *   0 = 10M, 1 = 100M, 2 = 1G, 3 = 10G, 4 = 2.5G, 5 = 5G
	 *
	 * SS[31:29] register encoding:
	 * 3'b000 = 10G XGMII
	 * 3'b001 = 25G XGMII
	 * 3'b010 = 2.5G GMII
	 * 3'b011 = 1G  GMII
	 * 3'b100 = 100M MII
	 * 3'b101 = 5G  XGMII
	 * 3'b110 = 2.5G XGMII
	 * 3'b111 = 10M MII
	 */
	 /*
	  * As XGMAC does not support 10M/100M, 1G is used as the default.
	 */
	switch (speed) {
	case 0:  /* mac_speed 0 = 10M -> SS = 0x7 */
		fallthrough;
	case 1:  /* mac_speed 1 = 100M -> SS = 0x4 */
		fallthrough;
	case 2:  /* mac_speed 2 = 1G -> SS = 0x3 */
		speed_bits = 0x3;
		break;
	case 3:  /* mac_speed 3 = 10G -> SS = 0x0 */
		speed_bits = 0x0;
		break;
	case 4:  /* mac_speed 4 = 2.5G -> SS = 0x6 (XGMII) or 0x2 (GMII) */
		speed_bits = uxsgmii ? 0x6 : 0x2;
		break;
	case 5:  /* mac_speed 5 = 5G -> SS = 0x5 */
		speed_bits = 0x5;
		break;
	default:
		/* Invalid mac_speed: preserve current SS setting */
		speed_bits = (readl(base) >> 29) & 0x7;
		break;
	}

	/* Program SS with TX disabled, then enable TX */
	{
		u32 reg_value_disabled = reg_value;

		/* Ensure TE is cleared and SS is updated first */
		reg_value_disabled &= ~TE;
		reg_value_disabled &= ~(0x7 << 29);
		reg_value_disabled |= (speed_bits << 29);

		/* First write: SS only while TE/RE are disabled */
		writel(reg_value_disabled, base);
		mdelay(1);

		/* Second write: enable TX and JD after SS is set */
		u32 reg_value_enable = reg_value_disabled | JD | TE;
		writel(reg_value_enable, base);
		mdelay(1);
	}

	rx_config_addr = base + MAC_RX_CONFIGURATION_ADDRESS;
	reg_value = readl(rx_config_addr);
	reg_value |= 0x30000040 | RE | ACS | CST;
	writel(reg_value, rx_config_addr);
	mdelay(1);

	filter_addr = base + MAC_PACKET_FILTER_ADDRESS;
	writel(0x80000081, filter_addr);
}

/*
 * ppe_port_bridge_txmac_set()
 * TXMAC should be disabled for all ports by default
 * TXMAC should be enabled for all ports that are link up alone
 */
void ppe_port_bridge_txmac_set(phys_addr_t reg_base, u32 port,
			       bool isenable)
{
	phys_addr_t addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->port_bridge_ctrl, port);
	u32 old = readl(addr);
	u32 reg_value = old;
	u32 txmac_mask = ppe_port_bridge_txmac_mask();

	if (isenable)
		reg_value |= txmac_mask;
	else
		reg_value &= ~txmac_mask;

	writel(reg_value, addr);
}

/*
 * Get PHY status from PPE
 */
u8 phy_status_get_from_ppe(phys_addr_t reg_base, u32 port_id)
{
	u32 reg_field = readl((reg_base + ((port_id > 4) ?
				PORT_PHY_STATUS_ADDRESS1 :
				PORT_PHY_STATUS_ADDRESS)));

	switch (port_id) {
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
		break;
	}

	return (u8)reg_field;
}

static inline void ppe_port_mux_set(phys_addr_t reg_base, struct port_info *port)
{
	union port_mux_ctrl_u port_mux_ctrl;
	const u8 id = port->id;
	const u8 uniphy_type = port->uniphy_type;
	const u8 mac_type = port->gmac_type;
	const phys_addr_t mux_reg = reg_base + PORT_MUX_CTRL;

	port_mux_ctrl.val = readl(mux_reg);

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
		return;
	}

	writel(port_mux_ctrl.val, mux_reg);
}

/*
 * PPE port speed set
 */
void ppe_port_speed_set(phys_addr_t reg_base, struct port_info *port)
{
	bool usxgmii = false;
	u32 gmacid;
	int speed = -1;
	uintptr_t base;

	ppe_port_bridge_txmac_set(reg_base, port->id, false);

	if (port->cur_gmac_type != port->gmac_type) {
		ppe_port_mux_set(reg_base, port);
		port->cur_gmac_type = port->gmac_type;
	}

	switch (port->uniphy_mode) {
	case PORT_WRAPPER_10GBASE_R:
		break;
	case PORT_WRAPPER_UQXGMII:
	case PORT_WRAPPER_USXGMII:
		ppe_uniphy_usxgmii_autoneg_completed(port->uniphy_id);
		ppe_uniphy_usxgmii_speed_set(port->id, port->uniphy_id,
					     port->mac_speed);
		ppe_uniphy_usxgmii_duplex_set(port->uniphy_id, port->duplex);
		ppe_uniphy_usxgmii_port_reset(port->uniphy_id, port->id,
					      port->uniphy_mode);
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
	case PORT_WRAPPER_NA:
		fallthrough;
	default:
		break;
	}

	ppe_port_bridge_txmac_set(reg_base, port->id, true);

	if (port->gmac_type == XGMAC) {
		ppe_xgmac_configuration(reg_base, port->id, speed, usxgmii);
	} else {
		gmacid = port->id - 1;
		base = reg_base + PPE_MAC_ENABLE + (0x200 * gmacid);

		writel(0x73, base);

		/* If mac_speed > 2, cap it at 2; otherwise use actual value */
		writel((port->mac_speed > 2) ? 2 : port->mac_speed, base + PPE_MAC_SPEED_OFF);

		writel(0x1, base + PPE_MAC_MIB_CTL_OFF);
	}
}

static inline void ipq_ppe_enable_port_counter(phys_addr_t reg_base,
					       int port_cnt)
{
	union mru_mtu_ctrl_tbl_u mru_mtu_ctrl_tbl;
	union mc_mtu_ctrl_tbl_u mc_mtu_ctrl_tbl;
	union port_eg_vlan_u port_eg_vlan;
	phys_addr_t mru_port, mc_port, eg_vlan;
	int i;

	for (i = 0; i < port_cnt; i++) {
		mru_port = reg_base + ppe_table_addr(&ppe_table_addrs->mru_mtu_ctrl_tbl, i);
		REG_READ(mru_port, mru_mtu_ctrl_tbl.val);
		mru_mtu_ctrl_tbl.bf.rx_cnt_en = 1;
		mru_mtu_ctrl_tbl.bf.tx_cnt_en = 1;
		REG_WRITE(mru_port, mru_mtu_ctrl_tbl.val);

		mc_port = reg_base + ppe_table_addr(&ppe_table_addrs->mc_mtu_ctrl_tbl, i);
		REG_READ(mc_port, mc_mtu_ctrl_tbl.val);
		mc_mtu_ctrl_tbl.bf.tx_cnt_en = 1;
		REG_WRITE(mc_port, mc_mtu_ctrl_tbl.val);

		eg_vlan = reg_base + ppe_table_addr(&ppe_table_addrs->port_eg_vlan_tbl, i);
		REG_READ(eg_vlan, port_eg_vlan.val);
		port_eg_vlan.bf.tx_counting_en = 1;
		REG_WRITE(eg_vlan, port_eg_vlan.val);
	}
}

/*
 * VSI Table Entry Structure (128 bits):
 * - Bits [8:0]:   member_port_bitmap (9 bits)
 * - Bits [17:9]:  uuc_bitmap (Unknown Unicast, 9 bits)
 * - Bits [26:18]: umc_bitmap (Unknown Multicast, 9 bits)
 * - Bits [31:27]: bc_bitmap_0 (Broadcast lower 5 bits)
 * - Bits [35:32]: bc_bitmap_1 (Broadcast upper 4 bits)
 *
 * Parameters:
 * - port_bitmap: Bitmap of ports to include (bit 0 = port 0, bit 2 = port 2, etc.)
 *                Use 0x004 for port 2 only
 *                Use 0x005 for port 0 (CPU) and port 2
 *                Use 0x1FF for all ports
 */
int ipq_vsi_setup(phys_addr_t reg_base, u32 vsi, uint32_t port_bitmap)
{
	phys_addr_t addr;
	u32 reg[4];  /* 128-bit register = 4 x 32-bit words */
	u32 bc_bitmap_0, bc_bitmap_1;

	addr = reg_base + ppe_table_addr(&ppe_table_addrs->vsi_tbl, vsi);
	port_bitmap &= 0x1FF;
	REG_READ(addr, reg);
	bc_bitmap_0 = port_bitmap & 0x1F;
	bc_bitmap_1 = (port_bitmap >> 5) & 0xF;

	/*
	 * Configure Word 0 (Bits [31:0]):
	 * - Bits [8:0]:   member_port_bitmap
	 * - Bits [17:9]:  uuc_bitmap (Unknown Unicast)
	 * - Bits [26:18]: umc_bitmap (Unknown Multicast)
	 * - Bits [31:27]: bc_bitmap_0 (Broadcast lower 5 bits)
	 */
	reg[0] = (port_bitmap & 0x1FF) |           /* Member ports [8:0] */
		 ((port_bitmap & 0x1FF) << 9) |    /* UUC ports [17:9] */
		 ((port_bitmap & 0x1FF) << 18) |   /* UMC ports [26:18] */
		 ((bc_bitmap_0 & 0x1F) << 27);     /* BC lower [31:27] */

	/*
	 * Configure Word 1 (Bits [63:32]):
	 * - Bits [35:32]: bc_bitmap_1 (Broadcast upper 4 bits)
	 * - Keep other bits unchanged
	 *   Note: Bits [35:32] are at positions [3:0] within word1
	 */
	reg[1] = (reg[1] & ~(0xF << 0)) | ((bc_bitmap_1 & 0xF) << 0);

	/* Words 2 and 3 remain unchanged for basic configuration */

	/* Write updated register values */
	REG_WRITE(addr, reg);

	return 0;
}

/*
 * Configure PPE scheduler tables
 *
 * The sch_config array index is based on tm_tick_mode read from DTS
 * (property "tm_tick_mode", default 0 if not specified).
 */
void ipq_ppe_schedular_config(struct ppe_info *ppe)
{
	u32 *sch_config_values = &sch_config[ppe->tm_tick_mode].val[0];

	reg_write(ppe->base + ppe_table_addrs->psch_tdm_cfg_tbl.base_addr +
		  ppe_table_addrs->psch_tdm_cfg_tbl.offset,
		  sch_config[ppe->tm_tick_mode].depth,
		  ppe_table_addrs->psch_tdm_cfg_tbl.increment,
		  sch_config_values);
}

/*
 * Configure PPE TDM (Time Division Multiplexing)
 */
void ipq_ppe_tdm_configuration(struct ppe_info *ppe)
{
	u32 *config_values = &tdm_config[ppe->tdm_mode].val[0];
	u32 depth = tdm_config[ppe->tdm_mode].depth;
	u32 val = 0;

	if (depth > 255) {
		printf("Warning: TDM entries %u exceeds maximum 255\n", depth);
		return;
	}

	reg_write(ppe->base + tdm_addr_config->tdm_addr.offset,
		  depth,
		  tdm_addr_config->tdm_addr.increment,
		  config_values);

	val = TDM_CTRL_TDM_EN_MASK;
	val |= (0 << TDM_CTRL_TDM_OFFSET_SHIFT) & TDM_CTRL_TDM_OFFSET_MASK;
	val |= (depth << TDM_CTRL_TDM_DEPTH_SHIFT) & TDM_CTRL_TDM_DEPTH_MASK;

	writel(val, ppe->base + tdm_addr_config->tdm_ctrl_offset);

	if (ppe->tm) {
		writel(0x20, (void *)0x3a47a000);
		writel(0x12, (void *)0x3a47a010);
		writel(0x1, (void *)0x3a47a020);
		writel(0x2, (void *)0x3a47a030);
		writel(0x10, (void *)0x3a47a040);
		writel(0x21, (void *)0x3a47a050);
		writel(0x2, (void *)0x3a47a060);
		writel(0x10, (void *)0x3a47a070);
		writel(0x12, (void *)0x3a47a080);
		writel(0x1, (void *)0x3a47a090);
		writel(0xa, (void *)0x3a400000);
		writel(0x303, (void *)0x3a026100);
		writel(0x303, (void *)0x3a026104);
		writel(0x303, (void *)0x3a026108);
	}
}

/*
 * VP_PORT_TBL Register Structure (128-bit / 4x32-bit words)
 * Base: PPE_IPE_L3_BASE_ADDR (0x200000) + VP_PORT_TBL offset (0x4000)
 * Increment: 0x10 (16 bytes per port entry)
 *
 * Word 0 [31:0]:
 *   Bits [7:0]:   physical_port (8 bits) - Physical port number
 *   Bits [31:8]:  Reserved
 *
 * Word 1 [63:32]:
 *   Bit  [9]:     dst_port_id_valid (1 bit) - Destination port ID valid flag
 *   Bits [19:10]: dst_port_id (10 bits) - VSI (Virtual Switch Instance) ID
 *   Bits [31:20]: Reserved
 *
 * Word 2 [95:64]:
 *   All bits reserved
 *
 * Word 3 [127:96]:
 *   All bits reserved
 */
/*
 * ipq_ppe_vp_port_tbl_set() - Configure VP to VSI mapping
 * @reg_base: PPE register base address
 * @port: Virtual port number (0-based)
 * @vsi: Virtual Switch Instance ID (VSI)
 *
 * This function configures the VP (Virtual Port) to VSI mapping in the
 * PPE L3 VP Port Table. Each entry is 128 bits (4x32-bit words).
 *
 * Configuration:
 * - Word 0: Physical port = 0 (default)
 * - Word 1: dst_port_id_valid = 1, dst_port_id = VSI
 * - Word 2: Reserved = 0
 * - Word 3: Reserved = 0
 */
static void ipq_ppe_vp_port_tbl_set(phys_addr_t reg_base, u32 port, u32 vsi)
{
	phys_addr_t addr;
	u32 word1_val;

	/* Calculate table entry address */
	addr = reg_base + ppe_table_addr(&ppe_table_addrs->vp_port_tbl, port);

	/*
	 * Word 0: Physical port configuration
	 * Set physical_port = 0 (default mapping)
	 */
	u32 vp_reg[4];
	/* Word 0 */
	vp_reg[0] = 0x0;
	/* Word 1 */
	word1_val = VP_PORT_TBL_DST_PORT_ID_VALID |
		    ((vsi & VP_PORT_TBL_DST_PORT_ID_MASK) << VP_PORT_TBL_DST_PORT_ID_SHIFT);
	vp_reg[1] = word1_val;
	/* Word 2 */
	vp_reg[2] = 0x0;
	/* Word 3 */
	vp_reg[3] = 0x0;
	REG_WRITE(addr, vp_reg);
}

/*
 * ipq_port_mac_clock_setclear() - Set or clear port MAC clock resets
 * @dev: Device pointer
 * @port: Port information structure
 * @assert: true to assert resets (disable clocks), false to deassert (enable clocks)
 *
 * Applies reset control to MAC, TX, and RX clocks for the specified port.
 * Tries new-style reset names first (nss_port%d_*_rst). If MAC reset fails,
 * immediately switches to old-style names (nss_cc_port%d_*) for all resets.
 */
void ipq_port_mac_clock_setclear(struct udevice *dev, struct port_info *port,
				 bool assert)
{
	char mac_name[32], tx_name[32], rx_name[32];
	struct reset_ctl rst;
	int len, ret;

	if (!dev || !port)
		return;

	/* Try primary naming scheme (nss_port%d_*_rst) */
	len = snprintf(mac_name, sizeof(mac_name), "nss_port%d_mac_rst", port->id);
	if (len < 0 || len >= sizeof(mac_name))
		return;

	ret = reset_get_by_name(dev, mac_name, &rst);
	if (ret) {
		/* Primary failed - use secondary naming scheme for all */
		len = snprintf(mac_name, sizeof(mac_name), "nss_cc_port%d_mac", port->id);
		if (len < 0 || len >= sizeof(mac_name))
			return;

		len = snprintf(tx_name, sizeof(tx_name), "nss_cc_port%d_tx", port->id);
		if (len < 0 || len >= sizeof(tx_name))
			return;

		len = snprintf(rx_name, sizeof(rx_name), "nss_cc_port%d_rx", port->id);
		if (len < 0 || len >= sizeof(rx_name))
			return;

		/* Apply secondary naming scheme */
		ret = reset_get_by_name(dev, mac_name, &rst);
		if (!ret)
			ipq_port_reset(&rst, assert);

		ret = reset_get_by_name(dev, tx_name, &rst);
		if (!ret)
			ipq_port_reset(&rst, assert);

		ret = reset_get_by_name(dev, rx_name, &rst);
		if (!ret)
			ipq_port_reset(&rst, assert);

		return;
	}

	/* Primary succeeded - use primary naming scheme for all */
	ipq_port_reset(&rst, assert);

	len = snprintf(tx_name, sizeof(tx_name), "nss_port%d_tx_rst", port->id);
	if (len < 0 || len >= sizeof(tx_name))
		return;

	len = snprintf(rx_name, sizeof(rx_name), "nss_port%d_rx_rst", port->id);
	if (len < 0 || len >= sizeof(rx_name))
		return;

	ret = reset_get_by_name(dev, tx_name, &rst);
	if (!ret)
		ipq_port_reset(&rst, assert);

	ret = reset_get_by_name(dev, rx_name, &rst);
	if (!ret)
		ipq_port_reset(&rst, assert);
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

/**
 * ppe_ipo_rule_reg_set - Write IPO rule registers
 * @reg_base: PPE register base address
 * @hw_reg: Hardware rule register union
 * @rule_id: Rule ID
 */
static void ppe_ipo_rule_reg_set(phys_addr_t reg_base,
				 union ipo_rule_reg_v2_u *hw_reg,
				 u32 rule_id)
{
	phys_addr_t reg_addr;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ipo_rule_reg, rule_id);
	REG_WRITE(reg_addr, hw_reg->val);
}

/**
 * ppe_ipo_mask_reg_set - Write IPO mask registers
 * @reg_base: PPE register base address
 * @hw_mask: Hardware mask register union
 * @rule_id: Rule ID
 */
static void ppe_ipo_mask_reg_set(phys_addr_t reg_base,
				 union ipo_mask_reg_u *hw_mask,
				 u32 rule_id)
{
	phys_addr_t reg_addr;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ipo_mask_reg, rule_id);
	REG_WRITE(reg_addr, hw_mask->val);
}

/**
 * ppe_ipo_action_set - Write IPO action registers
 * @reg_base: PPE register base address
 * @hw_act: Hardware action register union
 * @rule_id: Rule ID
 * @ipo_cnt: Number of action words to write
 */
static void ppe_ipo_action_set(phys_addr_t reg_base,
			       union ipo_action_v2_u *hw_act,
			       u32 rule_id,
			       u32 ipo_cnt)
{
	u32 i;
	phys_addr_t addr;

	for (i = 0; i < ipo_cnt; i++) {
		addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ipo_action, rule_id) + (i * 4);
		writel(hw_act->val[i], addr);
	}
}

/**
 * ppe_acl_rule_add - Add an ACL rule to hardware
 * @acl_rule: ACL rule configuration
 *
 * This function converts the ACL rule configuration to hardware format
 * and programs the IPO rule, mask, and action registers.
 *
 * Return: 0 on success, negative error code on failure
 */
static int ppe_acl_rule_add(struct ppe_acl_rule *acl_rule)
{
	union ipo_rule_reg_v2_u hw_reg = {0};
	union ipo_mask_reg_u hw_mask = {0};
	union ipo_action_v2_u hw_act = {0};

	if (acl_rule->rule_id >= MAX_RULE) {
		printf("Error: Invalid ACL rule ID %u (max %u)\n",
		       acl_rule->rule_id, MAX_RULE - 1);
		return -EINVAL;
	}

	/* Configure action */
	hw_act.bf.dest_info_change_en = 1;

	/* Configure mask */
	hw_mask.bf.maskfield_0 = acl_rule->mask;

	/* Configure rule type */
	hw_reg.bf.rule_type = acl_rule->rule_type;

	if (acl_rule->rule_type == ADPT_ACL_HPPE_IPV4_DIP_RULE) {
		/* IPv4 DIP rule configuration */
		hw_reg.bf.rule_field_0 = acl_rule->field1;
		hw_reg.bf.rule_field_1 = acl_rule->field0 << 17;
		hw_mask.bf.maskfield_1 = 7 << 17;

		if (acl_rule->permit) {
			hw_act.bf.fwd_cmd = 0;  /* Forward */
			hw_reg.bf.pri = 0x1;
		}

		if (acl_rule->deny) {
			hw_act.bf.fwd_cmd = 1;  /* Drop */
			hw_reg.bf.pri = 0x0;
		}
	} else if (acl_rule->rule_type == ADPT_ACL_HPPE_MAC_SA_RULE) {
		/* MAC SA rule configuration */
		hw_reg.bf.rule_field_0 = acl_rule->field1;
		hw_reg.bf.rule_field_1 = acl_rule->field0;
		hw_mask.bf.maskfield_1 = 0xffff;
		hw_act.bf.fwd_cmd = 1;  /* Drop */
		hw_reg.bf.pri = 0x2;
		/* Bypass FDB learn and FDB fresh */
		hw_act.bf.bypass_bitmap_0 = 0x1800;
	} else if (acl_rule->rule_type == ADPT_ACL_HPPE_MAC_DA_RULE) {
		/* MAC DA rule configuration */
		hw_reg.bf.rule_field_0 = acl_rule->field1;
		hw_reg.bf.rule_field_1 = acl_rule->field0;
		hw_mask.bf.maskfield_1 = 0xffff;
		hw_act.bf.fwd_cmd = 1;  /* Drop */
		hw_reg.bf.pri = 0x2;
	}

	/* Bind to ports 1-6 */
	hw_reg.bf.src_0 = 0x0;
	hw_reg.bf.src_1 = 0x3F;

	/* Write to hardware */
	ppe_ipo_rule_reg_set(acl_rule->reg_base, &hw_reg, acl_rule->rule_id);
	ppe_ipo_mask_reg_set(acl_rule->reg_base, &hw_mask, acl_rule->rule_id);
	ppe_ipo_action_set(acl_rule->reg_base, &hw_act, acl_rule->rule_id,
			   acl_rule->ipo_cnt);

	return 0;
}

/**
 * ppe_acl_dhcp_init - Initialize DHCP ACL rules
 * @reg_base: PPE register base address
 * @ipo_action: Number of IPO action words
 *
 * Configures ACL rules to:
 * - Allow DHCP port 67 (server)
 * - Allow DHCP port 68 (client)
 * - Drop all other UDP packets
 *
 * Return: 0 on success, negative error code on failure
 */
int ppe_acl_dhcp_init(phys_addr_t reg_base, u32 ipo_action)
{
	struct ppe_acl_rule acl_rule;
	int ret;

	/* Rule 0: Allow DHCP port 67 (server) */
	memset(&acl_rule, 0, sizeof(acl_rule));
	acl_rule.reg_base = reg_base;
	acl_rule.rule_id = 0;
	acl_rule.rule_type = ADPT_ACL_HPPE_IPV4_DIP_RULE;
	acl_rule.field0 = UDP_PKT;
	acl_rule.field1 = 67;
	acl_rule.mask = 0xffff;
	acl_rule.permit = 1;
	acl_rule.deny = 0;
	acl_rule.ipo_cnt = ipo_action;

	ret = ppe_acl_rule_add(&acl_rule);
	if (ret) {
		printf("Error: Failed to add DHCP port 67 rule\n");
		return ret;
	}

	/* Rule 1: Allow DHCP port 68 (client) */
	memset(&acl_rule, 0, sizeof(acl_rule));
	acl_rule.reg_base = reg_base;
	acl_rule.rule_id = 1;
	acl_rule.rule_type = ADPT_ACL_HPPE_IPV4_DIP_RULE;
	acl_rule.field0 = UDP_PKT;
	acl_rule.field1 = 68;
	acl_rule.mask = 0xffff;
	acl_rule.permit = 1;
	acl_rule.deny = 0;
	acl_rule.ipo_cnt = ipo_action;

	ret = ppe_acl_rule_add(&acl_rule);
	if (ret) {
		printf("Error: Failed to add DHCP port 68 rule\n");
		return ret;
	}

	/* Rule 2: Drop all other UDP packets */
	memset(&acl_rule, 0, sizeof(acl_rule));
	acl_rule.reg_base = reg_base;
	acl_rule.rule_id = 2;
	acl_rule.rule_type = ADPT_ACL_HPPE_IPV4_DIP_RULE;
	acl_rule.field0 = UDP_PKT;
	acl_rule.field1 = 0;
	acl_rule.mask = 0;
	acl_rule.permit = 0;
	acl_rule.deny = 1;
	acl_rule.ipo_cnt = ipo_action;

	ret = ppe_acl_rule_add(&acl_rule);
	if (ret) {
		printf("Error: Failed to add UDP drop rule\n");
		return ret;
	}

	return 0;
}

#if IS_ENABLED(CONFIG_TFTP_PORT)
/**
 * ppe_acl_tftp_init - Initialize TFTP ACL rule
 * @reg_base: PPE register base address
 * @tftp_port: TFTP port number to allow
 * @ipo_action: Number of IPO action words
 *
 * Configures ACL rule to allow TFTP traffic on the specified port.
 * This rule has higher priority than the UDP drop rule.
 *
 * Return: 0 on success, negative error code on failure
 */
int ppe_acl_tftp_init(phys_addr_t reg_base, u32 tftp_port, u32 ipo_action)
{
	struct ppe_acl_rule acl_rule;
	int ret;

	/* Rule 3: Allow TFTP port */
	memset(&acl_rule, 0, sizeof(acl_rule));
	acl_rule.reg_base = reg_base;
	acl_rule.rule_id = 3;
	acl_rule.rule_type = 0x4;  /* IPv4 rule */
	acl_rule.field0 = 0x1;     /* UDP */
	acl_rule.field1 = tftp_port;
	acl_rule.mask = 0xffff;
	acl_rule.permit = 1;
	acl_rule.deny = 0;
	acl_rule.ipo_cnt = ipo_action;

	ret = ppe_acl_rule_add(&acl_rule);
	if (ret) {
		printf("Error: Failed to add TFTP rule\n");
		return ret;
	}

	return 0;
}
#endif /* CONFIG_TFTP_PORT */

/**
 * ppe_acl_init - Initialize all PPE ACL rules
 * @reg_base: PPE register base address
 * @ipo_action: Number of IPO action words
 *
 * Main initialization function that configures all ACL rules:
 * - DHCP rules (always)
 * - TFTP rule (if CONFIG_TFTP_PORT is enabled)
 *
 * Return: 0 on success, negative error code on failure
 */
int ppe_acl_init(phys_addr_t reg_base, u32 ipo_action)
{
	int ret;

	/* Initialize DHCP rules */
	ret = ppe_acl_dhcp_init(reg_base, ipo_action);
	if (ret)
		return ret;

#if IS_ENABLED(CONFIG_TFTP_PORT)
	/* Initialize TFTP rule if enabled */
	u32 tftp_port = 1024 + (get_timer(0) % 3072);

	ret = ppe_acl_tftp_init(reg_base, tftp_port, ipo_action);
	if (ret)
		return ret;

	tftp_acl_our_port = tftp_port;

	/* Export TFTP port for use by TFTP client */
	env_set_ulong("tftpsrcp", tftp_port);
#endif

	return 0;
}

/*
 * PPE dynamic threshold set
 */
int ipq_ppe_dynamic_threshold_set(phys_addr_t reg_base, u32 port_id, struct bm_dynamic_cfg *cfg)
{
	phys_addr_t reg_addr;
	union port_fc_cfg_u fc_cfg;

	memset(&fc_cfg, 0, sizeof(fc_cfg));

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->port_fc_cfg, port_id);

	REG_READ(reg_addr, fc_cfg.val);

	fc_cfg.bf.port_shared_weight = cfg->weight;
	fc_cfg.bf.port_shared_ceiling_0 = cfg->shared_ceiling;
	fc_cfg.bf.port_shared_ceiling_1 = cfg->shared_ceiling >> PORT_CEILING_SHIFT;
	fc_cfg.bf.port_resume_offset = cfg->resume_off;
	fc_cfg.bf.port_resume_floor_th = cfg->resume_min_thresh;
	fc_cfg.bf.port_shared_dynamic = 1;

	REG_WRITE(reg_addr, fc_cfg.val);

	return 0;
}

/*
 * PPE BM port reserved buffer set
 */
int ipq_ppe_bm_port_reserved_buffer_set(phys_addr_t reg_base, u32 port_id,
					u16 prealloc_buf, u16 react_buf)
{
	phys_addr_t reg_addr;
	union port_fc_cfg_u fc_cfg;

	memset(&fc_cfg, 0, sizeof(fc_cfg));

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->port_fc_cfg, port_id);

	REG_READ(reg_addr, fc_cfg.val);

	fc_cfg.bf.port_pre_alloc = prealloc_buf;
	fc_cfg.bf.port_react_limit = react_buf;

	REG_WRITE(reg_addr, fc_cfg.val);
	return 0;
}

/*
 * PPE buffer group buffer set
 */
int ipq_ppe_bufgroup_buffer_set(phys_addr_t reg_base, u8 bufgrp_id, u16 buff_num)
{
	phys_addr_t reg_addr;
	union shared_group_cfg_u cfg;

	memset(&cfg, 0, sizeof(cfg));

	cfg.bf.shared_group_limit = buff_num;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->port_bufgrp_cfg, bufgrp_id);

	writel(cfg.val, reg_addr);

	return 0;
}

/*
 * PPE port buffer group map set
 */
int ipq_ppe_port_bufgroup_map_set(phys_addr_t reg_base, u32 port_id, u8 bufgrp_id)
{
	phys_addr_t reg_addr;
	union port_group_id_u grp_id;

	memset(&grp_id, 0, sizeof(grp_id));

	grp_id.bf.port_shared_group_id = bufgrp_id;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->port_shp_cfg, port_id);

	writel(grp_id.val, reg_addr);

	return 0;
}

/*
 * PPE BM flow control mode config
 */
int ipq_ppe_bm_fc_mode_config(phys_addr_t reg_base, u32 port_id, bool enable)
{
	phys_addr_t reg_addr;
	union port_fc_mode_u fc_mode;

	memset(&fc_mode, 0, sizeof(fc_mode));

	fc_mode.bf.fc_en = enable ? 1 : 0;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->port_cnt_cfg, port_id);

	writel(fc_mode.val, reg_addr);

	return 0;
}

/*
 * PPE BM hardware initialization
 */
int ipq_ppe_bm_hw_init(phys_addr_t reg_base)
{
	u32 i;
	struct bm_dynamic_cfg cfg;
	const u32 phy_off = PPE_BM_PHY_PORT_OFFSET;
	u16 group_buf;

	/* Configure flow control and buffer group mapping */
	for (i = 0; i < PPE_BM_PORT_NUM; i++) {
		bool is_phy = (i >= phy_off && i <= PPE_BM_PHY_PORT_MAX);
		/* Disable FC for PHY ports; enable for CPU/EIP/non-PHY ports */
		ipq_ppe_bm_fc_mode_config(reg_base, i, !is_phy);
		/* Map all ports to buffer group 0 */
		ipq_ppe_port_bufgroup_map_set(reg_base, i, 0);
	}

	/* Set shared buffer group size from chip-specific configuration */
	group_buf = ipq_get_group_buf();
	ipq_ppe_bufgroup_buffer_set(reg_base, 0, group_buf);

	/* Configure per-port reserved buffers */
	for (i = 0; i < PPE_BM_PORT_NUM; i++) {
		u16 prealloc_buf = 0;
		u16 react_buf = (i < phy_off) ? 200 : 228;

		ipq_ppe_bm_port_reserved_buffer_set(reg_base, i, prealloc_buf, react_buf);
	}

	/* Program dynamic thresholds per port */
	memset(&cfg, 0, sizeof(cfg));
	for (i = 0; i < PPE_BM_PORT_NUM; i++) {
		bool is_phy = (i >= phy_off && i <= PPE_BM_PHY_PORT_MAX);
		bool is_special_phy = (i == (phy_off + 4)) || (i == (phy_off + 5));
		u16 ceiling;
		u16 resume_off;

		if (is_phy) {
			if (is_special_phy) {
				ceiling = 1200;
				resume_off = 8;
			} else {
				ceiling = 650;
				resume_off = 36;
			}
		} else {
			ceiling = 650;
			resume_off = 36;
		}

		cfg.shared_ceiling = ceiling;
		cfg.resume_min_thresh = 0;
		cfg.resume_off = resume_off;
		cfg.weight = 7;

		ipq_ppe_dynamic_threshold_set(reg_base, i, &cfg);
	}

	return 0;
}

/**
 * ipq_ppe_ac_uni_queue_cfg_tbl_get - Read AC_UNI_QUEUE_CFG_TBL register
 * @reg_base: Physical base address of PPE QM registers
 * @index: Queue index (0-255)
 * @value: Pointer to union structure to store register value
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ipq_ppe_ac_uni_queue_cfg_tbl_get(phys_addr_t reg_base, u32 index,
					    union ac_uni_queue_cfg_tbl_u *value)
{
	phys_addr_t reg_addr;

	/* Calculate register address using structure-based addressing */
	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ac_uni_queue_cfg_tbl, index);

	/* Read 5 words (32 bytes) from hardware */
	REG_READ(reg_addr, value->val);

	return 0;
}

/**
 * ipq_ppe_ac_uni_queue_cfg_tbl_set - Write AC_UNI_QUEUE_CFG_TBL register
 * @reg_base: Physical base address of PPE QM registers
 * @index: Queue index (0-255)
 * @value: Pointer to union structure containing register value
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ipq_ppe_ac_uni_queue_cfg_tbl_set(phys_addr_t reg_base, u32 index,
					    union ac_uni_queue_cfg_tbl_u *value)
{
	phys_addr_t reg_addr;

	/* Calculate register address */
	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ac_uni_queue_cfg_tbl, index);

	/* Write 5 words (32 bytes) to hardware */
	REG_WRITE(reg_addr, value->val);

	/* Ensure write completion */
	readl(reg_addr);

	return 0;
}
/**
 * ipq_ppe_ac_mul_queue_cfg_tbl_set - Write AC_MUL_QUEUE_CFG_TBL register
 * @reg_base: Physical base address of PPE QM registers
 * @index: Queue index (0-43)
 * @value: Pointer to union structure containing register value
 *
 * Returns: 0 on success, negative error code on failure
 */
static int ipq_ppe_ac_mul_queue_cfg_tbl_set(phys_addr_t reg_base, u32 index,
					    union ac_mul_queue_cfg_tbl_u *value)
{
	phys_addr_t reg_addr;

	/* Calculate register address */
	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ac_mul_queue_cfg_tbl, index);

	/* Write 3 words (16 bytes) to hardware */
	REG_WRITE(reg_addr, value->val);

	/* Ensure write completion */
	readl(reg_addr);

	return 0;
}

/**
 * ipq_ppe_qm_ucast_threshold_reset - Reset dynamic threshold for a unicast queue
 * @reg_base: PPE register base address
 * @queue_id: Unicast queue ID (0..255)
 * @cfg: Pointer to dynamic threshold configuration (must be non-NULL)
 *
 * Return: 0 on success, -EINVAL on invalid params or read error
 */
static int ipq_ppe_qm_ucast_threshold_reset(phys_addr_t reg_base,
					    u32 queue_id,
					    struct fal_ac_dynamic_threshold *cfg)
{
	/* Validate parameters */
	if (!cfg || queue_id >= IPQ_PPE_UCAST_QUEUE_MAX)
		return -EINVAL;

	union ac_uni_queue_cfg_tbl_u v;
	int ret = ipq_ppe_ac_uni_queue_cfg_tbl_get(reg_base, queue_id, &v);

	if (ret)
		return ret;

	/* Precompute split shifts once */
	const u32 yel_min_shift = YEL_MIN_SHIFT;
	const u32 grn_resume_shift = GRN_RESUME_SHIFT;
	const u32 red_resume_shift = RED_RESUME_SHIFT;
	const u32 ceiling_shift = CEILING_SHIFT;

	/* Update fields */
	v.bf.ac_cfg_wred_en = cfg->wred_enable;
	v.bf.ac_cfg_color_aware = cfg->color_enable;
	v.bf.ac_cfg_shared_dynamic = 1;

	v.bf.ac_cfg_gap_grn_grn_min = cfg->green_min_off;
	v.bf.ac_cfg_gap_grn_yel_max = cfg->yel_max_off;
	v.bf.ac_cfg_gap_grn_yel_min_0 = cfg->yel_min_off;
	v.bf.ac_cfg_gap_grn_yel_min_1 = cfg->yel_min_off >> yel_min_shift;

	v.bf.ac_cfg_gap_grn_red_max = cfg->red_max_off;
	v.bf.ac_cfg_gap_grn_red_min = cfg->red_min_off;

	v.bf.ac_cfg_grn_resume_offset_0 = cfg->green_resume_off;
	v.bf.ac_cfg_grn_resume_offset_1 = cfg->green_resume_off >> grn_resume_shift;

	v.bf.ac_cfg_yel_resume_offset = cfg->yel_resume_off;
	v.bf.ac_cfg_red_resume_offset_0 = cfg->red_resume_off;
	v.bf.ac_cfg_red_resume_offset_1 = cfg->red_resume_off >> red_resume_shift;

	v.bf.ac_cfg_shared_weight = cfg->shared_weight;
	v.bf.ac_cfg_shared_ceiling_0 = cfg->ceiling;
	v.bf.ac_cfg_shared_ceiling_1 = cfg->ceiling >> ceiling_shift;

	return ipq_ppe_ac_uni_queue_cfg_tbl_set(reg_base, queue_id, &v);
}

/**
 * ipq_ppe_qm_mcast_threshold_reset - Reset static threshold for multicast queue/group
 * @reg_base: PPE register base address
 * @obj: Admission control object describing queue/group (must be non-NULL)
 * @cfg: Pointer to static threshold configuration (must be non-NULL)
 *
 * Return: 0 on success, -EINVAL on invalid params, or read error
 */
static int ipq_ppe_qm_mcast_threshold_reset(phys_addr_t reg_base,
					    struct fal_ac_obj_t *obj,
					    struct fal_ac_static_threshold *cfg)
{
	/* Validate parameters */
	if (!obj || !cfg)
		return -EINVAL;

	if (obj->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u g;
		phys_addr_t reg_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_grp_cfg_tbl, obj->index);

		REG_READ(reg_addr, g.val);

		/* Precompute split shift for DP threshold and YEL resume */
		const u32 dp_thrd_shift = DP_THRD_SHIFT;
		const u32 yel_resume_shift = GRP_YEL_RESUME_SHIFT;

		g.bf.ac_cfg_color_aware = cfg->color_enable;
		g.bf.ac_grp_dp_thrd_0 = cfg->green_max;
		g.bf.ac_grp_dp_thrd_1 = cfg->green_max >> dp_thrd_shift;
		g.bf.ac_grp_gap_grn_yel = cfg->yel_max_off;
		g.bf.ac_grp_gap_grn_red = cfg->red_max_off;
		g.bf.ac_grp_grn_resume_offset = cfg->green_resume_off;
		g.bf.ac_grp_yel_resume_offset_0 = cfg->yel_resume_off;
		g.bf.ac_grp_yel_resume_offset_1 = cfg->yel_resume_off >> yel_resume_shift;
		g.bf.ac_grp_red_resume_offset = cfg->red_resume_off;

		REG_WRITE(reg_addr, g.val);
		return 0;
	} else if (obj->type == FAL_AC_QUEUE) {
		if (obj->index < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u v;
			phys_addr_t reg_addr = reg_base +
				ppe_table_addr(&ppe_table_addrs->ac_uni_queue_cfg_tbl, obj->index);

			REG_READ(reg_addr, v.val);

			/* Unicast-style fields in static mode (shared_dynamic=0) */
			const u32 yel_min_shift   = YEL_MIN_SHIFT;
			const u32 grn_resume_shift = GRN_RESUME_SHIFT;
			const u32 red_resume_shift = RED_RESUME_SHIFT;
			const u32 ceiling_shift    = CEILING_SHIFT;

			v.bf.ac_cfg_wred_en = cfg->wred_enable;
			v.bf.ac_cfg_color_aware = cfg->color_enable;
			v.bf.ac_cfg_shared_dynamic = 0;

			v.bf.ac_cfg_gap_grn_grn_min = cfg->green_min_off;
			v.bf.ac_cfg_gap_grn_yel_max = cfg->yel_max_off;
			v.bf.ac_cfg_gap_grn_yel_min_0 = cfg->yel_min_off;
			v.bf.ac_cfg_gap_grn_yel_min_1 = cfg->yel_min_off >> yel_min_shift;

			v.bf.ac_cfg_gap_grn_red_max = cfg->red_max_off;
			v.bf.ac_cfg_gap_grn_red_min = cfg->red_min_off;

			v.bf.ac_cfg_yel_resume_offset = cfg->yel_resume_off;
			v.bf.ac_cfg_red_resume_offset_0 = cfg->red_resume_off;
			v.bf.ac_cfg_red_resume_offset_1 = cfg->red_resume_off >> red_resume_shift;
			v.bf.ac_cfg_grn_resume_offset_0 = cfg->green_resume_off;
			v.bf.ac_cfg_grn_resume_offset_1 = cfg->green_resume_off >> grn_resume_shift;

			/* Use green_max as static ceiling */
			v.bf.ac_cfg_shared_ceiling_0 = cfg->green_max;
			v.bf.ac_cfg_shared_ceiling_1 = cfg->green_max >> ceiling_shift;

			return ipq_ppe_ac_uni_queue_cfg_tbl_set(reg_base, obj->index, &v);
		}

		union ac_mul_queue_cfg_tbl_u m;
		phys_addr_t reg_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_mul_queue_cfg_tbl, obj->index);

		REG_READ(reg_addr, m.val);

		const u32 red_resume_shift = MUL_RED_RESUME_SHIFT;
		const u32 gap_grn_yel_shift = MUL_GAP_GRN_YEL_SHIFT;

		m.bf.ac_cfg_color_aware = cfg->color_enable;
		m.bf.ac_cfg_grn_resume_offset = cfg->green_resume_off;
		m.bf.ac_cfg_yel_resume_offset = cfg->yel_resume_off;
		m.bf.ac_cfg_red_resume_offset_0 = cfg->red_resume_off;
		m.bf.ac_cfg_red_resume_offset_1 = cfg->red_resume_off >> red_resume_shift;
		m.bf.ac_cfg_gap_grn_red = cfg->red_max_off;
		m.bf.ac_cfg_gap_grn_yel_0 = cfg->yel_max_off;
		m.bf.ac_cfg_gap_grn_yel_1 = cfg->yel_max_off >> gap_grn_yel_shift;
		m.bf.ac_cfg_shared_ceiling = cfg->green_max;

		return ipq_ppe_ac_mul_queue_cfg_tbl_set(reg_base, obj->index, &m);
	} else {
		return -EINVAL;
	}
}

/**
 * ipq_ppe_qm_threshold_reset - Reset queue threshold to default values
 * @reg_base: PPE register base address
 * @queue_id: Queue ID to reset (0..IPQ_PPE_L0SCHEDULER_CFG_MAX-1)
 *
 * Unicast queues use dynamic threshold mode, multicast queues use static mode.
 *
 * Return: 0 on success, -EINVAL on invalid params
 */
int ipq_ppe_qm_threshold_reset(phys_addr_t reg_base, u32 queue_id)
{
	/* Parameter validation */
	if (queue_id >= IPQ_PPE_L0SCHEDULER_CFG_MAX)
		return -EINVAL;

	/* Default values across supported chips */
	const u16 ceiling        = 2200;
	const u16 weight         = 7;
	const u16 resume_offset  = 36;
	const u16 green_max      = 250;

	if (queue_id < IPQ_PPE_UCAST_QUEUE_MAX) {
		struct fal_ac_dynamic_threshold d = {0};

		d.shared_weight = weight;
		d.ceiling = ceiling;
		d.green_resume_off = resume_offset;
		return ipq_ppe_qm_ucast_threshold_reset(reg_base, queue_id, &d);
	}

	struct fal_ac_static_threshold s = {0};
	struct fal_ac_obj_t obj = {0};

	s.green_max = green_max;
	s.green_resume_off = resume_offset;
	obj.type = FAL_AC_QUEUE;
	obj.index = queue_id;
	return ipq_ppe_qm_mcast_threshold_reset(reg_base, &obj, &s);
}

/**
 * ipq_ppe_qm_threshold_reset_all - Reset all queue thresholds
 * @reg_base: Physical base address of PPE QM registers
 *
 * This function resets threshold configuration for all queues.
 * Should be called during QM hardware initialization.
 *
 * Returns: 0 on success, negative error code on failure
 */
int ipq_ppe_qm_threshold_reset_all(phys_addr_t reg_base)
{
	u32 queue_id;
	int ret;

	for (queue_id = 0; queue_id < IPQ_PPE_L0SCHEDULER_CFG_MAX; queue_id++) {
		ret = ipq_ppe_qm_threshold_reset(reg_base, queue_id);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * PPE AC group buffer set
 */
int ipq_ppe_ac_group_buffer_set(phys_addr_t reg_base, u32 group_id, u32 prealloc_buffer,
				u32 total_buffer)
{
	phys_addr_t reg_addr;
	union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

	memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ac_grp_cfg_tbl, group_id);
	REG_READ(reg_addr, ac_grp_cfg_tbl.val);
	ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_0 = prealloc_buffer;
	ac_grp_cfg_tbl.bf.ac_grp_palloc_limit_1 = prealloc_buffer >> PALLOC_SHIFT;
	ac_grp_cfg_tbl.bf.ac_grp_limit = total_buffer;
	REG_WRITE(reg_addr, ac_grp_cfg_tbl.val);

	return 0;
}

/**
 * ipq_ppe_ac_group_prealloc_buffer_set - Set prealloc buffer for AC group/queue
 * @reg_base: PPE register base address
 * @obj_cfg: Admission control object describing queue/group (must be non-NULL)
 * @num: Preallocated buffer count to set
 *
 * Return: 0 on success, -EINVAL on invalid params
 */
int ipq_ppe_ac_group_prealloc_buffer_set(phys_addr_t reg_base,
					 struct fal_ac_obj_t *obj_cfg,
					 u32 num)
{
	/* Validate parameters */
	if (!obj_cfg)
		return -EINVAL;

	phys_addr_t reg_addr;

	if (obj_cfg->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u g;

		reg_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_grp_cfg_tbl, obj_cfg->index);

		REG_READ(reg_addr, g.val);

		const u32 palloc_shift = PALLOC_SHIFT;

		g.bf.ac_grp_palloc_limit_0 = num;
		g.bf.ac_grp_palloc_limit_1 = num >> palloc_shift;

		REG_WRITE(reg_addr, g.val);
	} else if (obj_cfg->type == FAL_AC_QUEUE) {
		if (obj_cfg->index < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u v;

			reg_addr = reg_base +
				ppe_table_addr(&ppe_table_addrs->ac_uni_queue_cfg_tbl,
					       obj_cfg->index);

			REG_READ(reg_addr, v.val);
			v.bf.ac_cfg_pre_alloc_limit = num;
			REG_WRITE(reg_addr, v.val);
		} else {
			union ac_mul_queue_cfg_tbl_u m;

			reg_addr = reg_base +
				ppe_table_addr(&ppe_table_addrs->ac_mul_queue_cfg_tbl,
					       obj_cfg->index);

			REG_READ(reg_addr, m.val);
			m.bf.ac_cfg_pre_alloc_limit = num;
			REG_WRITE(reg_addr, m.val);
		}
	} else {
		return -EINVAL;
	}

	return 0;
}

/**
 * ipq_ppe_ac_queue_group_set - Assign queue to an AC group
 * @reg_base: PPE register base address
 * @queue_id: Queue ID (0..IPQ_PPE_L0SCHEDULER_CFG_MAX-1)
 * @group_id: Admission control group ID
 *
 * Return: 0 on success, -EINVAL on invalid params
 */
int ipq_ppe_ac_queue_group_set(phys_addr_t reg_base, u32 queue_id, u32 group_id)
{
	if (queue_id >= IPQ_PPE_L0SCHEDULER_CFG_MAX)
		return -EINVAL;

	phys_addr_t reg_addr;

	if (queue_id < UCAST_QUEUE_ID_MAX) {
		union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;

		memset(&ac_uni_queue_cfg_tbl, 0, sizeof(ac_uni_queue_cfg_tbl));

		reg_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_uni_queue_cfg_tbl, queue_id);
		REG_READ(reg_addr, ac_uni_queue_cfg_tbl.val);
		ac_uni_queue_cfg_tbl.bf.ac_cfg_grp_id = group_id;
		REG_WRITE(reg_addr, ac_uni_queue_cfg_tbl.val);
	} else {
		union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;

		memset(&ac_mul_queue_cfg_tbl, 0, sizeof(ac_mul_queue_cfg_tbl));

		reg_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_mul_queue_cfg_tbl, queue_id);
		REG_READ(reg_addr, ac_mul_queue_cfg_tbl.val);
		ac_mul_queue_cfg_tbl.bf.ac_cfg_grp_id = group_id;
		REG_WRITE(reg_addr, ac_mul_queue_cfg_tbl.val);
	}

	return 0;
}

/*
 * PPE AC group control set
 */
int ipq_ppe_ac_group_ctrl_set(phys_addr_t reg_base, struct fal_ac_obj_t *obj_cfg,
			      struct fal_ac_ctrl_t *cfg)
{
	phys_addr_t reg_addr;

	if (obj_cfg->type == FAL_AC_GROUP) {
		union ac_grp_cfg_tbl_u ac_grp_cfg_tbl;

		memset(&ac_grp_cfg_tbl, 0, sizeof(ac_grp_cfg_tbl));

		reg_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_grp_cfg_tbl, obj_cfg->index);
		REG_READ(reg_addr, ac_grp_cfg_tbl.val);
		ac_grp_cfg_tbl.bf.ac_cfg_ac_en = cfg->ac_en;
		ac_grp_cfg_tbl.bf.ac_cfg_force_ac_en = cfg->ac_fc_en;
		REG_WRITE(reg_addr, ac_grp_cfg_tbl.val);
	} else if (obj_cfg->type == FAL_AC_QUEUE) {
		if (obj_cfg->index < UCAST_QUEUE_ID_MAX) {
			union ac_uni_queue_cfg_tbl_u ac_uni_queue_cfg_tbl;

			memset(&ac_uni_queue_cfg_tbl, 0, sizeof(ac_uni_queue_cfg_tbl));

			reg_addr = reg_base +
				ppe_table_addr(&ppe_table_addrs->ac_uni_queue_cfg_tbl,
					       obj_cfg->index);
			REG_READ(reg_addr, ac_uni_queue_cfg_tbl.val);
			ac_uni_queue_cfg_tbl.bf.ac_cfg_ac_en = cfg->ac_en;
			ac_uni_queue_cfg_tbl.bf.ac_cfg_force_ac_en = cfg->ac_fc_en;
			REG_WRITE(reg_addr, ac_uni_queue_cfg_tbl.val);
		} else {
			union ac_mul_queue_cfg_tbl_u ac_mul_queue_cfg_tbl;

			memset(&ac_mul_queue_cfg_tbl, 0, sizeof(ac_mul_queue_cfg_tbl));

			reg_addr = reg_base +
				ppe_table_addr(&ppe_table_addrs->ac_mul_queue_cfg_tbl,
					       obj_cfg->index);
			REG_READ(reg_addr, ac_mul_queue_cfg_tbl.val);
			ac_mul_queue_cfg_tbl.bf.ac_cfg_ac_en = cfg->ac_en;
			ac_mul_queue_cfg_tbl.bf.ac_cfg_force_ac_en = cfg->ac_fc_en;
			REG_WRITE(reg_addr, ac_mul_queue_cfg_tbl.val);
		}
	} else {
		return -EINVAL;
	}

	return 0;
}
/*
 * PPE L0 queue map set
 */
int ipq_ppe_l0_queue_map_set(phys_addr_t reg_base, struct fal_qos_scheduler_cfg *cfg, u32 index,
			     u32 port_id)
{
	phys_addr_t reg_addr;

	union l0_flow_map_tbl_u l0_flow_map_tbl;
	union l0_flow_port_map_tbl_u l0_flow_port_map_tbl;
	union l0_comp_cfg_tbl_u l0_comp_cfg_tbl;

	memset(&l0_flow_map_tbl, 0, sizeof(l0_flow_map_tbl));

	if (index >= L0_FLOW_MAP_TBL_MAX_ENTRY)
		return -1;

	l0_flow_map_tbl.bf.e_drr_credit_unit = cfg->e_drr_unit;
	l0_flow_map_tbl.bf.c_drr_credit_unit = cfg->c_drr_unit;
	l0_flow_map_tbl.bf.e_drr_id = cfg->e_drr_id;
	l0_flow_map_tbl.bf.c_drr_id = cfg->c_drr_id;

	l0_flow_map_tbl.bf.e_drr_wt = cfg->e_drr_wt;
	l0_flow_map_tbl.bf.c_drr_wt = cfg->c_drr_wt;
	l0_flow_map_tbl.bf.e_pri = cfg->e_pri;
	l0_flow_map_tbl.bf.c_pri = cfg->c_pri;
	l0_flow_map_tbl.bf.sp_id = cfg->sp_id;
	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l0_flow_map_tbl, index);
	REG_WRITE(reg_addr, l0_flow_map_tbl.val);

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l0_flow_port_map_tbl, index);
	l0_flow_port_map_tbl.val = readl(reg_addr);
	l0_flow_port_map_tbl.bf.port_num = port_id;
	writel(l0_flow_port_map_tbl.val, reg_addr);

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l0_comp_cfg_tbl, index);
	l0_comp_cfg_tbl.val = readl(reg_addr);
	l0_comp_cfg_tbl.bf.drr_meter_len = cfg->drr_frame_mode;
	writel(l0_comp_cfg_tbl.val, reg_addr);

	return 0;
}

/*
 * PPE L1 flow map set
 */
int ipq_ppe_l1_flow_map_set(phys_addr_t reg_base, struct fal_qos_scheduler_cfg *cfg, u32 index,
			    u32 port_id)
{
	phys_addr_t reg_addr;
	union l1_flow_map_tbl_u l1_flow_map_tbl;
	union l1_flow_port_map_tbl_u l1_flow_port_map_tbl;
	union l1_comp_cfg_tbl_u l1_comp_cfg_tbl;

	if (index >= L1_FLOW_MAP_TBL_MAX_ENTRY)
		return -1;

	l1_flow_map_tbl.bf.e_drr_credit_unit = cfg->e_drr_unit;
	l1_flow_map_tbl.bf.c_drr_credit_unit = cfg->c_drr_unit;
	l1_flow_map_tbl.bf.e_drr_id = cfg->e_drr_id;
	l1_flow_map_tbl.bf.c_drr_id = cfg->c_drr_id;

	l1_flow_map_tbl.bf.e_drr_wt = cfg->e_drr_wt;
	l1_flow_map_tbl.bf.c_drr_wt = cfg->c_drr_wt;
	l1_flow_map_tbl.bf.e_pri = cfg->e_pri;
	l1_flow_map_tbl.bf.c_pri = cfg->c_pri;
	l1_flow_map_tbl.bf.sp_id = cfg->sp_id;
	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l1_flow_map_tbl, index);
	REG_WRITE(reg_addr, l1_flow_map_tbl.val);

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l1_flow_port_map_tbl, index);
	l1_flow_port_map_tbl.val = readl(reg_addr);
	l1_flow_port_map_tbl.bf.port_num = port_id;
	writel(l1_flow_port_map_tbl.val, reg_addr);

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l1_comp_cfg_tbl, index);
	l1_comp_cfg_tbl.val = readl(reg_addr);
	l1_comp_cfg_tbl.bf.drr_meter_len = cfg->drr_frame_mode;
	writel(l1_comp_cfg_tbl.val, reg_addr);

	return 0;
}

/*
 * PPE unicast queue QM set
 */
int ipq_ppe_ucast_queue_qm_set(phys_addr_t reg_base, struct ppe_ucast_queue_dest *ucast_queue_dest,
			       u32 queue_id, u32 profile)
{
	int index;
	union ucast_queue_map_tbl_u ucast_queue_map_tbl;
	phys_addr_t reg_addr;

	if (ucast_queue_dest->service_code_en) {
		if (ucast_queue_dest->service_code >= PPE_MAX_SERVICE_CODE_NUM)
			return -1;

		index = SERVICE_CODE_QUEUE_OFFSET +
			(ucast_queue_dest->src_profile << 8) + ucast_queue_dest->service_code;
	} else if (ucast_queue_dest->cpu_code_en) {
		if (ucast_queue_dest->cpu_code >= PPE_MAX_CPU_CODE_NUM)
			return -1;

		index = CPU_CODE_QUEUE_OFFSET +
			(ucast_queue_dest->src_profile << 8) + ucast_queue_dest->cpu_code;
	} else {
		index = VP_PORT_QUEUE_OFFSET +
			(ucast_queue_dest->src_profile << 8) +
				FAL_PORT_ID_VALUE(ucast_queue_dest->dst_port);
	}

	ucast_queue_map_tbl.bf.queue_id = queue_id;
	ucast_queue_map_tbl.bf.profile_id = profile;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ucast_queue_map_tbl, index);
	writel(ucast_queue_map_tbl.val, reg_addr);

	return 0;
}

/*
 * PPE unicast queue priority set
 */
int ipq_ppe_ucast_queue_priority_set(phys_addr_t reg_base, u32 profile, u32 priority, u32 class)
{
	phys_addr_t reg_addr;
	int index = 0;
	union ucast_priority_map_tbl_u ucast_priority_map_tbl;

	memset(&ucast_priority_map_tbl, 0, sizeof(ucast_priority_map_tbl));

	index = profile << 4 | priority;
	ucast_priority_map_tbl.bf.class = class;

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->ucast_priority_map_tbl, index);
	writel(ucast_priority_map_tbl.val, reg_addr);

	return 0;
}

/*
 * PPE queue counter set
 */
void ipq_ppe_queue_counter_set(phys_addr_t reg_base)
{
	phys_addr_t reg_addr;
	union eg_bridge_config_u eg_bridge_config;

	/* NSS_PTX_CSR_BASE_ADDR + EG_BRIDGE_CONFIG_ADDRESS via table mapping */

	reg_addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->eg_bridge_config, 0);

	REG_READ(reg_addr, eg_bridge_config.val);

	eg_bridge_config.bf.queue_cnt_en = true;

	REG_WRITE(reg_addr, eg_bridge_config.val);
}

/*
 * PPE multicast queue AC init for
 */
int ppe_mcast_queue_ac_init(phys_addr_t reg_base)
{
	int i;

	/* Configure all 44 multicast queue table entries */
	for (i = 0; i < 44; i++) {
		/* Use structure-based addressing per entry index */
		phys_addr_t queue_addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->ac_mcast_queue_en_tbl,
				       i);

		u32 vals[3];

		vals[0] = 0x00fa0001;
		vals[1] = 0x0;
		vals[2] = 0x1200;
		REG_WRITE(queue_addr, vals);
	}

	return 0;
}

/**
 * Configure L2_GLOBAL_CONF register to value 0x400C0
 *
 * This function demonstrates how to properly set each bit field
 * using the union structure approach for register access.
 */
void ipq_ppe_l2_global_conf(phys_addr_t reg_base)
{
	union l2_global_conf_u reg_val;
	phys_addr_t addr = reg_base +
		ppe_table_addr(&ppe_table_addrs->l2_global_conf, 0);

	/* Initialize to zero */
	memset(&reg_val, 0, sizeof(reg_val));

	reg_val.val = readl(addr);

	/* Configure each field to achieve 0x400C0 */

	/* FDB Hash Configuration */
	reg_val.bf.fdb_hash_mode_0 = 0;              // CRC mode 0
	reg_val.bf.fdb_hash_mode_1 = 0;              // CRC mode 0
	reg_val.bf.fdb_hash_full_fwd_cmd = 0;        // Forward when hash full

	/* Learning and Aging Control */
	reg_val.bf.lrn_en = 1;                       // Enable MAC learning
	reg_val.bf.age_en = 1;                       // Enable MAC aging
	reg_val.bf.lrn_ctrl_mode = 0;                // Hardware control
	reg_val.bf.age_ctrl_mode = 0;                // Hardware control

	/* Advanced Features */
	reg_val.bf.failover_en = 0;                  // Disable failover
	reg_val.bf.service_code_loop = 0;            // Disable service code loop
	reg_val.bf.flow_cpy_escape = 0;              // Disable flow copy escape

	/* PVLAN Isolation */
	reg_val.bf.mc_pvlan_isol_en = 0;             // Disable MC PVLAN isolation
	reg_val.bf.bc_pvlan_isol_en = 0;             // Disable BC PVLAN isolation

	/* IP Multicast Configuration */
	reg_val.bf.ipmc_en = 0;                      // Disable IP multicast
	reg_val.bf.ipmc_hash_mode_0 = 0;             // CRC mode 0
	reg_val.bf.ipmc_hash_mode_1 = 1;             // CRC mode 1 (bit 18 set)
	reg_val.bf.mc_vlan_match_mode = 0;           // Standard match mode
	reg_val.bf.mc_dmac_check_en = 0;             // Disable DMAC check
	reg_val.bf.ipmc_mismatch_act = 0;            // Forward on mismatch

	/* 802.1p Mapper Configuration */
	reg_val.bf.dot1p_mapper_vlan_mode = 0;       // Standard VLAN mode
	reg_val.bf.dot1p_mapper_pcp_mode = 0;        // Standard PCP mode

	writel(reg_val.val, addr);
}

/*
 * PPE VSI configuration
 */
int ipq_ppe_vsi_config(struct ppe_info *info)
{
	if (!info)
		return -EINVAL;

	phys_addr_t reg_base = info->base;

	if (info->bridge_mode) {
		/* Bridge mode: map CPU (0) and all internal ports to VSI 2, then configure VSI 2 */
		ipq_ppe_vp_port_tbl_set(reg_base, 0, 2);
		for (u32 port = 1; port <= (u32)info->nos_iports; ++port)
			ipq_ppe_vp_port_tbl_set(reg_base, port, 2);
		ipq_vsi_setup(reg_base, 2, info->vsi);
	} else {
		/*
		 * Non-bridge: map ports 1..nos_iports to VSI IDs
		 * starting at 2 and configure each VSI
		 */
		u32 max_ports = (u32)info->nos_iports;
		u32 max_cfg   = (max_ports < CONFIG_ETH_MAX_MAC) ? max_ports : CONFIG_ETH_MAX_MAC;

		for (u32 idx = 0; idx < max_ports; ++idx) {
			u32 port   = idx + 1;    /* physical ports start at 1 */
			u32 vsi_id = 2 + idx;    /* VSI IDs start at 2 */

			ipq_ppe_vp_port_tbl_set(reg_base, port, vsi_id);

			if (idx < max_cfg)
				ipq_vsi_setup(reg_base, vsi_id, nb_vsi_config[idx]);
		}
	}

	return 0;
}

/*
 * PPE queue configuration
 */
int ipq_ppe_queue_config(struct ppe_info *info)
{
	phys_addr_t reg_base;
	const struct port_scheduler_cfg *psc, *psc0;
	struct fal_ac_ctrl_t ac_ctrl;
	struct fal_ac_obj_t ac_obj;
	struct fal_qos_scheduler_cfg cfg;
	int i, port, max_pri_supported;
	struct ppe_ucast_queue_dest ucast_queue_dest;

	if (!info || !info->port_sched_cfg)
		return -EINVAL;

	reg_base = info->base;
	psc = info->port_sched_cfg;
	psc0 = &psc[0];

	writel(0, reg_base +
		ppe_table_addr(&ppe_table_addrs->ucast_priority_map_tbl, 0));

	/*
	 * Configure unicast queue mapping for service codes
	 */
	memset(&ucast_queue_dest, 0, sizeof(ucast_queue_dest));
	ucast_queue_dest.src_profile = 0;
	ucast_queue_dest.service_code_en = true;
	ucast_queue_dest.cpu_code_en = 0;
	ucast_queue_dest.dst_port = 0;

	/* Service code 2 -> base + 8 */
	ucast_queue_dest.service_code = 2;
	ipq_ppe_ucast_queue_qm_set(reg_base, &ucast_queue_dest,
				   psc0->l0_uc_queue_id + 8, 0);

	/* Service code 5 -> base */
	ucast_queue_dest.service_code = 5;
	ipq_ppe_ucast_queue_qm_set(reg_base, &ucast_queue_dest,
				   psc0->l0_uc_queue_id, 0);

	/* Service code 6 -> base + 8 */
	ucast_queue_dest.service_code = 6;
	ipq_ppe_ucast_queue_qm_set(reg_base, &ucast_queue_dest,
				   psc0->l0_uc_queue_id + 8, 0);

	/*
	 * Configure per-port unicast queue mapping
	 */
	ucast_queue_dest.service_code_en = false;
	ucast_queue_dest.service_code = 0;
	for (port = 0; port < info->nos_iports; port++) {
		if (!psc[port].valid)
			break;

		ucast_queue_dest.dst_port = port;
		ipq_ppe_ucast_queue_qm_set(reg_base, &ucast_queue_dest,
					   psc[port].l0_uc_queue_id, port);
	}

	/*
	 * Set priority for port 0
	 */
	ipq_ppe_ucast_queue_priority_set(reg_base, 0, 0, 0);

	/*
	 * Initialize queue base for all CPU codes
	 */
	ucast_queue_dest.dst_port = 0;
	ucast_queue_dest.cpu_code_en = true;
	for (i = 0; i < PPE_MAX_CPU_CODE_NUM; i++) {
		ucast_queue_dest.cpu_code = i;
		ipq_ppe_ucast_queue_qm_set(reg_base, &ucast_queue_dest,
					   psc0->l0_uc_queue_id, 0);
	}

	/*
	 * Configure ARP reply packet with max priority
	 */
	max_pri_supported = PPE_CPU_PRI_NUM;
	if (psc0->valid && psc0->l0cdrr_end >= psc0->l0cdrr_start) {
		max_pri_supported = psc0->l0cdrr_end - psc0->l0cdrr_start + 1;
		if (max_pri_supported > PPE_PRI_MAX)
			max_pri_supported = PPE_CPU_PRI_NUM;
	}

	ucast_queue_dest.cpu_code = PPE_MGMT_ARP_REP_CPU_CODE;
	ipq_ppe_ucast_queue_qm_set(reg_base, &ucast_queue_dest,
				   psc0->l0cdrr_start + max_pri_supported - 1, 0);

	/*
	 * Configure queue admission control
	 */
	memset(&ac_ctrl, 0, sizeof(ac_ctrl));
	memset(&ac_obj, 0, sizeof(ac_obj));
	ac_ctrl.ac_en = true;
	ac_ctrl.ac_fc_en = false;

	for (i = 0; i < PPE_L0SCHEDULER_CFG_MAX; i++) {
		ac_obj.type = FAL_AC_QUEUE;
		ac_obj.index = i;
		ipq_ppe_ac_group_ctrl_set(reg_base, &ac_obj, &ac_ctrl);
		ipq_ppe_ac_queue_group_set(reg_base, i, 0);
		ipq_ppe_ac_group_prealloc_buffer_set(reg_base, &ac_obj, 0);
	}

	ipq_ppe_ac_group_buffer_set(reg_base, 0, 0, ipq_get_ac_group_total_buf());
	ipq_ppe_qm_threshold_reset_all(reg_base);

	/*
	 * Configure L1 scheduler
	 */
	for (i = 0; i < PPE_L1SCHEDULER_CFG_MAX && i < info->nos_iports; i++) {
		if (!psc[i].valid)
			break;

		memset(&cfg, 0, sizeof(cfg));
		cfg.sp_id = psc[i].l1_sp_id;
		cfg.c_pri = psc[i].l1_c_pri;
		cfg.c_drr_id = psc[i].l1_c_drr_id;
		cfg.e_pri = psc[i].l1_e_pri;
		cfg.e_drr_id = psc[i].l1_e_drr_id;
		cfg.c_drr_wt = psc[i].l1_c_drr_wt;
		cfg.e_drr_wt = psc[i].l1_e_drr_wt;

		ipq_ppe_l1_flow_map_set(reg_base, &cfg, psc[i].l1_map_index, i);
	}

	/*
	 * Configure L0 scheduler for unicast queues
	 */
	for (i = 0; i < PPE_L0SCHEDULER_CFG_MAX && i < info->nos_iports; i++) {
		if (!psc[i].valid)
			break;

		memset(&cfg, 0, sizeof(cfg));
		cfg.sp_id = psc[i].l0_sp_id;
		cfg.c_pri = psc[i].l0_c_pri;
		cfg.c_drr_id = psc[i].l0_c_drr_id;
		cfg.e_pri = psc[i].l0_e_pri;
		cfg.e_drr_id = psc[i].l0_e_drr_id;
		cfg.c_drr_wt = psc[i].l0_c_drr_wt;
		cfg.e_drr_wt = psc[i].l0_e_drr_wt;

		ipq_ppe_l0_queue_map_set(reg_base, &cfg, psc[i].l0_uc_queue_id, i);
	}

	/*
	 * Configure L0 scheduler for multicast queues
	 */
	for (i = 0; i < PPE_L0SCHEDULER_CFG_MAX && i < info->nos_iports; i++) {
		if (!psc[i].valid)
			break;

		memset(&cfg, 0, sizeof(cfg));
		cfg.sp_id = psc[i].l0_sp_id;
		cfg.c_pri = psc[i].l0_c_pri;
		cfg.c_drr_id = psc[i].l0_c_drr_id;
		cfg.e_pri = psc[i].l0_e_pri;
		cfg.e_drr_id = psc[i].l0_e_drr_id;
		cfg.c_drr_wt = psc[i].l0_c_drr_wt;
		cfg.e_drr_wt = psc[i].l0_e_drr_wt;

		ipq_ppe_l0_queue_map_set(reg_base, &cfg, psc[i].l0_mc_queue_id, i);
	}

	/*
	 * Initialize multicast queue admission control
	 */
	ppe_mcast_queue_ac_init(reg_base);

	/*
	 * Enable queue counters
	 */
	ipq_ppe_queue_counter_set(reg_base);

	return 0;
}

/**
 * ppe_detect_bridge_ctrl_layout() - Detect PORT_BRIDGE_CTRL register layout
 *
 * Detects whether the system uses EDMA v2 or EDMA v3 register layout
 * based on chip configuration.
 *
 * Returns: Detected layout enum value
 */
static enum ppe_bridge_ctrl_layout ppe_detect_bridge_ctrl_layout(void)
{
	if (current_layout != PPE_BRIDGE_CTRL_LAYOUT_UNKNOWN)
		return current_layout;

	/* Detect based on chip configuration */

	current_layout = PPE_V4;

	return current_layout;
}

/**
 * ppe_port_bridge_txmac_mask() - Get TXMAC_EN bit mask for current layout
 *
 * Returns: Bit mask for TXMAC_EN field
 */
u32 ppe_port_bridge_txmac_mask(void)
{
	enum ppe_bridge_ctrl_layout layout = ppe_detect_bridge_ctrl_layout();

	switch (layout) {
	case PPE_V2:
		return PPE_PORT_BRIDGE_CTRL_TXMAC_EN_V2;
	case PPE_V4:
		return PPE_PORT_BRIDGE_CTRL_TXMAC_EN_V4;
	default:
		return PPE_PORT_BRIDGE_CTRL_TXMAC_EN_V2;
	}
}

/**
 * ppe_port_bridge_promisc_mask() - Get PROMISC_EN bit mask for current layout
 *
 * Returns: Bit mask for PROMISC_EN field
 */
u32 ppe_port_bridge_promisc_mask(void)
{
	enum ppe_bridge_ctrl_layout layout = ppe_detect_bridge_ctrl_layout();

	switch (layout) {
	case PPE_V2:
		return PPE_PORT_BRIDGE_CTRL_PROMISC_EN_V2;
	case PPE_V4:
		return PPE_PORT_BRIDGE_CTRL_PROMISC_EN_V4;
	default:
		return PPE_PORT_BRIDGE_CTRL_PROMISC_EN_V2;
	}
}

/**
 * ppe_port_bridge_isolation_mask() - Get PORT_ISOLATION_BMP mask for current layout
 * @nos_iports: Number of internal ports (used to calculate bitmap size)
 *
 * Returns: Bit mask for PORT_ISOLATION_BMP field
 */
u32 ppe_port_bridge_isolation_mask(unsigned int nos_iports)
{
	enum ppe_bridge_ctrl_layout layout = ppe_detect_bridge_ctrl_layout();

	switch (layout) {
	case PPE_V2:
		/* EDMA v2: 8 bits starting at bit 16 */
		return PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP_V2;
	case PPE_V4:
		/* EDMA v4: 9 bits starting at bit 8
		 * (HMSPPE expects 9-bit bitmap regardless of active ports)
		 */
		return 0x1FF << 8;
	default:
		return PPE_PORT_BRIDGE_CTRL_PORT_ISOLATION_BMP_V2;
	}
}

/*
 * PPE port configuration
 */
int ipq_ppe_port_config(struct ppe_info *info)
{
	phys_addr_t reg_base;
	u32 val, i;

	if (!info)
		return -EINVAL;

	reg_base = info->base;

	/*
	 * Configure each internal port's bridge control register
	 * Port roles:
	 *   Port 0: CPU port (full capabilities)
	 *   Port 1-6: Ethernet ports (basic forwarding)
	 *   Port 7: EIP197/loopback (crypto offload)
	 */
	for (i = 0; i < info->nos_iports; i++) {
		phys_addr_t addr = reg_base +
			ppe_table_addr(&ppe_table_addrs->port_bridge_ctrl, i);

		if (i == info->port_cfg.port_cpu) {
			/* CPU port: Enable TX, promiscuous mode, isolation, and MAC learning */
			val = ppe_port_bridge_promisc_mask() |
				ppe_port_bridge_txmac_mask() |
				ppe_port_bridge_isolation_mask(info->nos_iports) |
				PPE_PORT_BRIDGE_CTRL_STATION_LRN_EN |
				PPE_PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN;
		} else if (i == info->port_cfg.port_eip || i == info->port_cfg.port_loopback) {
			/* EIP/Loopback port: Promiscuous mode, isolation, and MAC learning */
			val = ppe_port_bridge_promisc_mask() |
				ppe_port_bridge_isolation_mask(info->nos_iports) |
				PPE_PORT_BRIDGE_CTRL_STATION_LRN_EN |
				PPE_PORT_BRIDGE_CTRL_NEW_ADDR_LRN_EN;
		} else {
			/* Ethernet ports: Basic promiscuous mode and isolation only */
			val = ppe_port_bridge_promisc_mask() |
			      ppe_port_bridge_isolation_mask(info->nos_iports);
		}
		writel(val, addr);
	}

	return 0;
}

/*
 * PPE STP configuration
 */
int ipq_ppe_stp_config(struct ppe_info *info)
{
	phys_addr_t reg_base;
	u32 i;

	if (!info)
		return -EINVAL;

	reg_base = info->base;

	/*
	 * Configure STP state for all ports except the last one
	 * Set to FORWARDING state to enable traffic flow
	 */
	for (i = 0; i < info->nos_iports - 1; i++) {
		union cst_state_u cst;
		phys_addr_t cst_addr;

		cst_addr = reg_base + ppe_table_addr(&ppe_table_addrs->cst_state, i);
		cst.val = readl(cst_addr);
		cst.bf.port_state = CST_STATE_FORWARDING;
		writel(cst.val, cst_addr);
	}

	return 0;
}

/**
 * ipq_ppe_provision_init() - Initialize PPE (Packet Processing Engine) hardware
 * @info: PPE information structure containing configuration parameters
 *
 * This function performs complete PPE hardware initialization in the following order:
 * 1. Buffer Manager (BM) initialization - configures flow control and buffer allocation
 * 2. Queue Manager (QM) configuration - sets up unicast/multicast queues and schedulers
 * 3. TDM (Time Division Multiplexing) configuration - configures port scheduling
 * 4. Port counter initialization - enables statistics collection
 * 5. Port bridge control - configures port forwarding and isolation
 * 6. L2 global configuration - sets up MAC learning and aging
 * 7. VSI (Virtual Switch Instance) configuration - configures virtual switching
 * 8. STP (Spanning Tree Protocol) configuration - sets port states
 * 9. ACL (Access Control List) initialization - configures packet filtering rules
 *
 * This function must be called after PPE clocks are enabled and before any
 * packet processing operations.
 *
 * Return: None (void function)
 */
void ipq_ppe_provision_init(struct ppe_info *info)
{
	phys_addr_t reg_base;
	const struct port_scheduler_cfg *psc;

	/* Validate input parameters */
	if (!info) {
		printf("%s: ERROR - PPE info structure is NULL\n", __func__);
		return;
	}

	reg_base = info->base;
	psc = info->port_sched_cfg;

	if (!psc) {
		printf("%s: ERROR - Port scheduler config not found\n", __func__);
		return;
	}

	/*
	 * Step 1: Initialize Buffer Manager
	 * Configures flow control, buffer groups, and dynamic thresholds
	 */
	ipq_ppe_bm_hw_init(reg_base);

	/*
	 * Step 2: Configure Queue Manager
	 * Sets up unicast/multicast queues, L0/L1 schedulers, and admission control
	 */
	ipq_ppe_queue_config(info);

	/*
	 * Step 3: Configure TDM (Time Division Multiplexing)
	 * Sets up port scheduling and bandwidth allocation
	 */
	ipq_ppe_tdm_configuration(info);

	/*
	 * Step 4: Configure Scheduler
	 * Programs scheduler configuration tables
	 */
	ipq_ppe_schedular_config(info);

	/*
	 * Step 5: Enable Port Counters
	 * Enables statistics collection for all internal ports
	 */
	ipq_ppe_enable_port_counter(reg_base, info->nos_iports);

	/*
	 * Step 6: Configure Port Bridge Control
	 * Sets up port forwarding, isolation, and MAC learning
	 */
	ipq_ppe_port_config(info);

	/*
	 * Step 7: Configure L2 Global Settings
	 * Enables MAC learning, aging, and configures hash modes
	 */
	ipq_ppe_l2_global_conf(reg_base);

	/*
	 * Step 8: Configure VSI (Virtual Switch Instance)
	 * Sets up virtual switching and port-to-VSI mappings
	 */
	ipq_ppe_vsi_config(info);

	/*
	 * Step 9: Configure STP (Spanning Tree Protocol)
	 * Sets all ports to FORWARDING state
	 */
	ipq_ppe_stp_config(info);

	/*
	 * Step 10: Initialize ACL (Access Control List)
	 * Configures packet filtering rules (DHCP, TFTP, etc.)
	 */
	ppe_acl_init(reg_base, info->ipo_action);
}

#if IS_ENABLED(CONFIG_ETH_LOW_MEM)
#define mem_init()
#define mem_alloc(size, align)		malloc_cache_aligned(size)
#else

static unsigned long nc_end;
static unsigned long nc_next;

/*
 * Non-Cached Memory APIs
 */
static int mem_init(void)
{
	/*
	 * Compute the noncached DMA region start at runtime using
	 * gd->relocaddr + gd->mon_len, aligned to 1MB.
	 *
	 * Skip-reloc path (crashdump):
	 *   gd->relocaddr = CONFIG_TEXT_BASE
	 *   nc_start = ALIGN(CONFIG_TEXT_BASE + mon_len, SZ_1M)
	 *
	 * Normal reloc path:
	 *   arch_setup_dest_addr() subtracted SZ_1M from gd->relocaddr
	 *   before reserve_uboot() placed the image, so:
	 *   nc_start = ALIGN(gd->relocaddr + mon_len, SZ_1M)
	 *            = top_of_RAM - SZ_1M  (valid RAM, just above U-Boot)
	 */
	unsigned long nc_start = ALIGN(gd->relocaddr + gd->mon_len, SZ_1M);

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
 * EDMA RX buffer allocation - optimized version
 */
static inline int ipq_edma_alloc_rx_buffer(struct ipq_edma_hw *ehw,
					   struct ipq_edma_rxfill_ring *rxfill_ring)
{
	u16 num_alloc = 0;
	u16 cons, next, counter;
	struct ipq_edma_rxfill_desc *rxfill_desc;
	u32 reg_data;
	phys_addr_t reg_base = ehw->iobase;
	phys_addr_t ring_base;
	u32 buf_size_field;
	const u16 ring_mask = rxfill_ring->count - 1;

	/* Check for null pointer before accessing hw_cfg */
	if (!ehw->hw_cfg)
		return 0;

	/* Cache ring base address to reduce address calculations */
	ring_base = reg_base + ehw->hw_cfg->rxfill.base_offset +
		    (rxfill_ring->id * ehw->hw_cfg->rxfill.ring_increment);

	/* Pre-calculate buffer size field to avoid repeated shifts */
	buf_size_field = cpu_to_le32((ehw->hw_cfg->rx_buff_size <<
				      ehw->hw_cfg->rxfill.buf_size.shift) &
				      ehw->hw_cfg->rxfill.buf_size.mask);

	/* Read RXFILL ring producer index */
	phys_addr_t prod_idx_addr = ring_base + ehw->hw_cfg->rxfill.prod_idx.offset;

	reg_data = readl(prod_idx_addr);
	next = reg_data & ehw->hw_cfg->rxfill.prod_idx.mask & ring_mask;

	/* Read RXFILL ring consumer index */
	phys_addr_t cons_idx_addr = ring_base + ehw->hw_cfg->rxfill.cons_idx.offset;

	reg_data = readl(cons_idx_addr);
	cons = reg_data & ehw->hw_cfg->rxfill.cons_idx.mask;

	/* Fill ring until full */
	while (1) {
		counter = next;
		if (++counter == rxfill_ring->count)
			counter = 0;

		if (unlikely(counter == cons))
			break;

		/* Get RXFILL descriptor */
		rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, next);

		/* Fill descriptor fields efficiently */
		rxfill_desc->rdes2 = next;
		rxfill_desc->rdes1 |= buf_size_field;

		num_alloc++;
		next = counter;
	}

	if (likely(num_alloc)) {
		/* Update RXFILL ring producer index - single write */
		reg_data = next & ehw->hw_cfg->rxfill.prod_idx.mask;
		phys_addr_t prod_write_addr = ring_base + ehw->hw_cfg->rxfill.prod_idx.offset;

		writel(reg_data, prod_write_addr);
	}

	return num_alloc;
}

/*
 * EDMA TX completion cleanup - ultra-optimized version
 */
static inline u32 ipq_edma_clean_tx(struct ipq_edma_hw *ehw,
				    struct ipq_edma_txcmpl_ring *txcmpl_ring)
{
	struct ipq_edma_txcmpl_desc *txcmpl_desc;
	u16 prod_idx, cons_idx, initial_cons_idx;
	u32 data;
	u32 txcmpl_consumed = 0;
	uchar *skb;
	const phys_addr_t reg_base = ehw->iobase;
	const struct edma_hw_cfg *hw_cfg = ehw->hw_cfg;
	const u16 ring_mask = txcmpl_ring->count - 1;
	const u32 buf_hi_add_mask = hw_cfg->txdesc.buf_hi_add_mask;
	phys_addr_t ring_base, prod_idx_reg, cons_idx_reg;

	if (!hw_cfg)
		return -1;

	/* Pre-calculate register addresses to minimize address arithmetic */
	ring_base = reg_base + hw_cfg->txcmpl.base_offset +
		    (txcmpl_ring->id * hw_cfg->txcmpl.ring_increment);
	prod_idx_reg = ring_base + hw_cfg->txcmpl.prod_idx.offset;
	cons_idx_reg = ring_base + hw_cfg->txcmpl.cons_idx.offset;

	/* Get TXCMPL ring indices with cached register addresses */
	data = readl(prod_idx_reg);
	prod_idx = data & hw_cfg->txcmpl.prod_idx.mask;

	data = readl(cons_idx_reg);
	initial_cons_idx = data & hw_cfg->txcmpl.cons_idx.mask;
	cons_idx = initial_cons_idx;

	/* Early exit if no work to do */
	if (unlikely(cons_idx == prod_idx))
		return 0;

	/* Process all available descriptors */
	while (cons_idx != prod_idx) {
		txcmpl_desc = EDMA_TXCMPL_DESC(txcmpl_ring, cons_idx);

		/* Optimized buffer address calculation with cached mask */
		skb = (uchar *)((uintptr_t)(((uint64_t)(txcmpl_desc->tdes1 &
					buf_hi_add_mask) << 32) | txcmpl_desc->tdes0));

		if (unlikely(!skb))
			continue;  /* Skip invalid buffer */

		/* Efficient index increment with bit masking */
		cons_idx = (cons_idx + 1) & ring_mask;
		txcmpl_consumed++;
	}

	/* Update TXCMPL ring consumer index once at the end */
	if (txcmpl_consumed > 0)
		writel(cons_idx, cons_idx_reg);

	return txcmpl_consumed;
}

/*
 * EDMA RX descriptor cleanup - optimized version
 */
static inline u32 ipq_edma_clean_rx(struct ipq_edma_hw *ehw,
				    struct ipq_edma_rxdesc_ring *rxdesc_ring,
				    void **buff)
{
	struct ipq_edma_rxdesc_desc *rxdesc_desc;
	u16 prod_idx, cons_idx;
	int src_port_num;
	int pkt_length = 0;
	u16 cleaned_count = 0;
	phys_addr_t reg_base = ehw->iobase;
	phys_addr_t ring_base;
	const u16 ring_mask = rxdesc_ring->count - 1;

	if (!ehw->hw_cfg)
		return -1;

	/* Cache ring base address to reduce address calculations */
	ring_base = reg_base + ehw->hw_cfg->rxdesc.base_offset +
		    (rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment);

	/* Read consumer and producer indices with cached base */
	{
		phys_addr_t cons_addr = ring_base + ehw->hw_cfg->rxdesc.cons_idx.offset;
		u32 cons_raw = readl(cons_addr);

		cons_idx = cons_raw & ehw->hw_cfg->rxdesc.cons_idx.mask;
	}

	{
		phys_addr_t prod_addr = ring_base + ehw->hw_cfg->rxdesc.prod_idx.offset;
		u32 prod_raw = readl(prod_addr);

		prod_idx = prod_raw & ehw->hw_cfg->rxdesc.prod_idx.mask;
	}

	if (unlikely(cons_idx == prod_idx))
		return 0;

	rxdesc_desc = EDMA_RXDESC_DESC(rxdesc_ring, cons_idx);

	/* Prefetch descriptor for better cache performance */
	__builtin_prefetch(rxdesc_desc, 0, 3);

	/* Check src_info from Rx Descriptor */
	src_port_num = EDMA_RXDESC_SRC_INFO_GET(rxdesc_desc->rdes4);

	if (likely((src_port_num & ehw->hw_cfg->rxdesc.srcinfo_type_mask) ==
		   EDMA_RXDESC_SRCINFO_TYPE_PORTID)) {
		src_port_num &= ehw->hw_cfg->rxdesc.portnum_bits;
	} else {
		goto next_rx_desc;
	}

	/* Get packet length */
	pkt_length = (rxdesc_desc->rdes5 &
		      ehw->hw_cfg->rxdesc.pkt_size_mask) >>
		      ehw->hw_cfg->rxdesc.pkt_size_shift;

	if (unlikely(src_port_num < ehw->start_ports ||
		     src_port_num > ehw->max_ports)) {
		pkt_length = 0;
		goto next_rx_desc;
	}

	cleaned_count = 1;

	*buff = (void *)((uintptr_t)(((uint64_t)(rxdesc_desc->rdes1 &
				ehw->hw_cfg->txdesc.buf_hi_add_mask) << 32)
				| rxdesc_desc->rdes0));

next_rx_desc:
	/* Update consumer index using bit masking */
	cons_idx = (cons_idx + 1) & ring_mask;

	if (likely(cleaned_count)) {
		phys_addr_t cons_addr_w = ring_base + ehw->hw_cfg->rxdesc.cons_idx.offset;

		writel(cons_idx, cons_addr_w);
	}

	return pkt_length;
}

/*
 * EDMA RX completion processing - optimized version
 */
static inline int ipq_edma_rx_complete(struct ipq_eth_dev *priv, void **buff)
{
	struct ipq_edma_hw *ehw = &priv->hw;
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
	struct ipq_edma_rxdesc_ring *rxdesc_ring;
	struct ipq_edma_rxfill_ring *rxfill_ring;
	u32 misc_intr_status, reg_data;
	int length = 0;
	int i;
	phys_addr_t reg_base = ehw->iobase;
	phys_addr_t rxdesc_base, txcmpl_base, rxfill_base;

	/* Cache base addresses to reduce repeated calculations */
	rxdesc_base = reg_base +
		ehw->hw_cfg->rxdesc.base_offset;
	txcmpl_base = reg_base +
		ehw->hw_cfg->txcmpl.base_offset;
	rxfill_base = reg_base +
		ehw->hw_cfg->rxfill.base_offset;

	/* Process RX descriptors - optimized loop */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];

		length = ipq_edma_clean_rx(ehw, rxdesc_ring, buff);
	}

	/* Process TX completions - optimized loop */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		ipq_edma_clean_tx(ehw, txcmpl_ring);
	}

	/* Refill RX buffers - optimized loop */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		ipq_edma_alloc_rx_buffer(ehw, rxfill_ring);
	}

	/* Enable RXDESC EDMA ring interrupt masks */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		phys_addr_t ring_base = rxdesc_base +
			(rxdesc_ring->id *
			 ehw->hw_cfg->rxdesc.ring_increment);
		phys_addr_t int_mask_reg = ring_base +
			ehw->hw_cfg->rxdesc.int_mask.offset;

		writel(ehw->rxdesc_intr_mask, int_mask_reg);
	}

	/* Enable TX EDMA ring interrupt masks */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		phys_addr_t ring_base = txcmpl_base +
			(txcmpl_ring->id *
			 ehw->hw_cfg->txcmpl.ring_increment);
		phys_addr_t int_mask_reg = ring_base +
			ehw->hw_cfg->txcmpl.int_mask.offset;

		writel(ehw->txcmpl_intr_mask, int_mask_reg);
	}

	/* Enable RXFILL EDMA ring interrupt masks */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		phys_addr_t ring_base = rxfill_base +
			(rxfill_ring->id *
			 ehw->hw_cfg->rxfill.ring_increment);
		phys_addr_t int_mask_reg = ring_base +
			ehw->hw_cfg->rxfill.int_mask.offset;

		writel(ehw->rxfill_intr_mask, int_mask_reg);
	}

	/* Read Misc intr status - single read with cached base */
	reg_data = readl(reg_base + ehw->hw_cfg->global.misc_int_stat.offset);
	misc_intr_status = reg_data & ehw->misc_intr_mask;

	if (unlikely(misc_intr_status != 0)) {
		writel(EDMA_MASK_INT_DISABLE,
		       reg_base + ehw->hw_cfg->global.misc_int_mask.offset);
	}

	return length;
}

/*
 * EDMA ring resource setup
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

	/* Allocate Rx fill ring descriptors */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		rxfill_ring->count = ehw->hw_cfg->rx_ring_size;
		rxfill_ring->id = ehw->rxfill_ring_start + i;
		rxfill_ring->desc =
			(void *)mem_alloc(ehw->hw_cfg->rxfill_desc_size *
					  rxfill_ring->count, ARCH_DMA_MINALIGN);

		if (!rxfill_ring->desc) {
			pr_info("%s: rxfill_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		rxfill_ring->dma = virt_to_phys(rxfill_ring->desc);

		rx_buf = (void *)mem_alloc(ehw->hw_cfg->rx_buff_size *
					rxfill_ring->count,
					ARCH_DMA_MINALIGN);

		if (!rx_buf) {
			pr_info("%s: rxfill_ring->desc buffer alloc error\n",
				__func__);
			return -ENOMEM;
		}

		/* Allocate buffers for each of the desc */
		for (j = 0; j < rxfill_ring->count; j++) {
			phys_addr_t pa;

			rxfill_desc = EDMA_RXFILL_DESC(rxfill_ring, j);
			pa = virt_to_phys(rx_buf);
			rxfill_desc->rdes0 = pa;
#ifdef CONFIG_ARM64
			rxfill_desc->rdes1 = (u32)((pa >> 32) &
				ehw->hw_cfg->rxfill.buf_hi_add_mask);
#else
			rxfill_desc->rdes1 = 0;
#endif
			rxfill_desc->rdes2 = 0;
			rxfill_desc->rdes3 = 0;
			rx_buf += ehw->hw_cfg->rx_buff_size;
		}
	}

	/* Allocate RxDesc ring descriptors */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		rxdesc_ring->count = ehw->hw_cfg->rx_ring_size;
		rxdesc_ring->id = ehw->rxdesc_ring_start + i;

		/* Create a mapping between RX Desc ring and Rx fill ring */
		index = ehw->rxfill_ring_start + (i % ehw->rxfill_rings);
		rxdesc_ring->rxfill =
			&ehw->rxfill_ring[index - ehw->rxfill_ring_start];

		rxdesc_ring->desc = (void *)
			mem_alloc(ehw->hw_cfg->rxdesc_desc_size *
				  rxdesc_ring->count,
				  ARCH_DMA_MINALIGN);
		if (!rxdesc_ring->desc) {
			pr_info("%s: rxdesc_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		rxdesc_ring->dma = virt_to_phys(rxdesc_ring->desc);

		/* Allocate secondary Rx ring descriptors */
		rxdesc_ring->sdesc = (void *)mem_alloc(EDMA_RX_SEC_DESC_SIZE * rxdesc_ring->count,
				ARCH_DMA_MINALIGN);
		if (!rxdesc_ring->sdesc) {
			pr_info("%s: rxdesc_ring->sdesc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		rxdesc_ring->sdma = virt_to_phys(rxdesc_ring->sdesc);
	}

	/* Allocate TxDesc ring descriptors */
	for (i = 0; i < ehw->txdesc_rings; i++) {
		txdesc_ring = &ehw->txdesc_ring[i];
		txdesc_ring->count = ehw->hw_cfg->tx_ring_size;
		txdesc_ring->id = ehw->txdesc_ring_start + i;
		txdesc_ring->desc = (void *)
			mem_alloc(ehw->hw_cfg->txdesc_desc_size *
				  txdesc_ring->count,
				  ARCH_DMA_MINALIGN);
		if (!txdesc_ring->desc) {
			pr_info("%s: txdesc_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		txdesc_ring->dma = virt_to_phys(txdesc_ring->desc);

		tx_buf = (void *)mem_alloc(ehw->hw_cfg->tx_buff_size *
					txdesc_ring->count,
					ARCH_DMA_MINALIGN);
		if (!tx_buf) {
			pr_info("%s: txdesc_ring->desc buffer alloc error\n",
				__func__);
			return -ENOMEM;
		}

		/* Allocate buffers for each of the desc */
		for (j = 0; j < txdesc_ring->count; j++) {
			phys_addr_t pa;

			txdesc_desc = EDMA_TXDESC_DESC(txdesc_ring, j);
			pa = virt_to_phys(tx_buf);
			txdesc_desc->tdes0 = pa;
#ifdef CONFIG_ARM64
			txdesc_desc->tdes1 = (u32)((pa >> 32) &
				ehw->hw_cfg->txdesc.buf_hi_add_mask);
#else
			txdesc_desc->tdes1 = 0;
#endif
			txdesc_desc->tdes1 |= EDMA_TXDESC_PASSTHROUGH_EN;
			txdesc_desc->tdes2 = 0;
			txdesc_desc->tdes3 = 0;
			txdesc_desc->tdes4 = 0;
			txdesc_desc->tdes5 = 0;
			txdesc_desc->tdes6 = 0;
			txdesc_desc->tdes7 = 0;
			tx_buf += ehw->hw_cfg->tx_buff_size;
		}

		/* Allocate secondary Tx ring descriptors */
		txdesc_ring->sdesc = (void *)
			mem_alloc(ehw->hw_cfg->txdesc_sec_desc_size *
				  txdesc_ring->count,
				  ARCH_DMA_MINALIGN);
		if (!txdesc_ring->sdesc) {
			pr_info("%s: txdesc_ring->sdesc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		txdesc_ring->sdma = virt_to_phys(txdesc_ring->sdesc);
	}

	/* Allocate TxCmpl ring descriptors */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		txcmpl_ring->count = ehw->hw_cfg->tx_ring_size;
		txcmpl_ring->id = ehw->txcmpl_ring_start + i;
		txcmpl_ring->desc = (void *)mem_alloc(EDMA_TXCMPL_DESC_SIZE * txcmpl_ring->count,
				ARCH_DMA_MINALIGN);

		if (!txcmpl_ring->desc) {
			pr_info("%s: txcmpl_ring->desc alloc error\n",
				__func__);
			return -ENOMEM;
		}
		txcmpl_ring->dma = virt_to_phys(txcmpl_ring->desc);
	}

	pr_info("%s: successful\n", __func__);

	return 0;
}

/*
 * EDMA ring disable - optimized version
 */
static inline void ipq_edma_disable_rings(struct ipq_edma_hw *ehw)
{
	const phys_addr_t reg_base = ehw->iobase;
	const struct edma_hw_cfg *hw_cfg = ehw->hw_cfg;
	int i;
	u32 data;
	phys_addr_t rxdesc_base, rxfill_base, txdesc_base;

	/* Cache base addresses to reduce repeated calculations */
	rxdesc_base = reg_base + hw_cfg->rxdesc.base_offset;
	rxfill_base = reg_base + hw_cfg->rxfill.base_offset;
	txdesc_base = reg_base + hw_cfg->txdesc.base_offset;

	/* Disable Rx rings - optimized with cached base */
	if (ehw->sw_version != EDMA_SW_VER_3_ID) {
		for (i = 0; i < ehw->max_rxdesc_rings; i++) {
			phys_addr_t ring_ctrl = rxdesc_base +
				(i * hw_cfg->rxdesc.ring_increment) +
				hw_cfg->rxdesc.ctrl.offset;

			data = readl(ring_ctrl);
			data &= ~EDMA_RXDESC_RX_EN;
			writel(data, ring_ctrl);
		}
	}

	/* Disable RxFill Rings - optimized with cached base */
	for (i = 0; i < ehw->max_rxfill_rings; i++) {
		phys_addr_t ring_en = rxfill_base +
			(i * hw_cfg->rxfill.ring_increment) +
			hw_cfg->rxfill.ring_en.offset;

		data = readl(ring_en);
		data &= ~EDMA_RXFILL_RING_EN;
		writel(data, ring_en);
	}

	/* Disable Tx rings - optimized with cached base */
	for (i = 0; i < ehw->max_txdesc_rings; i++) {
		phys_addr_t ring_ctrl = txdesc_base +
			(i * hw_cfg->txdesc.ring_increment) +
			hw_cfg->txdesc.ctrl.offset;

		data = readl(ring_ctrl);
		data &= ~EDMA_TXDESC_TX_EN;
		writel(data, ring_ctrl);
	}
}

/*
 * Clear EDMA ring base addresses after the rings and EDMA block are disabled.
 * A zero base address is the reset/unconfigured value for these registers.
 */
static void ipq_edma_clear_ring_bases(struct ipq_edma_hw *ehw)
{
	const phys_addr_t reg_base = ehw->iobase;
	const struct edma_hw_cfg *hw_cfg = ehw->hw_cfg;
	int i;

	for (i = 0; i < ehw->max_txdesc_rings; i++) {
		phys_addr_t ring_base = reg_base + hw_cfg->txdesc.base_offset +
			i * hw_cfg->txdesc.ring_increment;

		writel(0, ring_base + hw_cfg->txdesc.base_addr.offset);
		writel(0, ring_base + hw_cfg->txdesc.base_addr_high.offset);
		writel(0, ring_base + hw_cfg->txdesc.base_addr2.offset);
		writel(0, ring_base + hw_cfg->txdesc.base_addr2_high.offset);
	}

	for (i = 0; i < ehw->max_txcmpl_rings; i++) {
		phys_addr_t ring_base = reg_base + hw_cfg->txcmpl.base_offset +
			i * hw_cfg->txcmpl.ring_increment;

		writel(0, ring_base + hw_cfg->txcmpl.base_addr.offset);
		writel(0, ring_base + hw_cfg->txcmpl.base_addr_high.offset);
	}

	for (i = 0; i < ehw->max_rxfill_rings; i++) {
		phys_addr_t ring_base = reg_base + hw_cfg->rxfill.base_offset +
			i * hw_cfg->rxfill.ring_increment;

		writel(0, ring_base + hw_cfg->rxfill.base_addr.offset);
		writel(0, ring_base + hw_cfg->rxfill.base_addr_high.offset);
	}

	for (i = 0; i < ehw->max_rxdesc_rings; i++) {
		phys_addr_t ring_base = reg_base + hw_cfg->rxdesc.base_offset +
			i * hw_cfg->rxdesc.ring_increment;

		writel(0, ring_base + hw_cfg->rxdesc.base_addr.offset);
		writel(0, ring_base + hw_cfg->rxdesc.base_addr_high.offset);
		writel(0, ring_base + hw_cfg->rxdesc.base_addr2.offset);
		writel(0, ring_base + hw_cfg->rxdesc.base_addr2_high.offset);
	}
}

/*
 * EDMA interrupt disable
 */
static void ipq_edma_disable_intr(struct ipq_edma_hw *ehw)
{
	phys_addr_t reg_base = ehw->iobase;
	int i;

	/* Disable interrupts */
	for (i = 0; i < ehw->max_rxdesc_rings; i++) {
		writel(0, reg_base + ehw->hw_cfg->rxdesc.base_offset +
		       (i * ehw->hw_cfg->rxdesc.ring_increment) +
		       ehw->hw_cfg->rxdesc.int_ctrl.offset);
	}

	for (i = 0; i < ehw->max_rxfill_rings; i++) {
		writel(0, reg_base + ehw->hw_cfg->rxfill.base_offset +
		       (i * ehw->hw_cfg->rxfill.ring_increment) +
		       ehw->hw_cfg->rxfill.int_mask.offset);
	}

	for (i = 0; i < ehw->max_txcmpl_rings; i++) {
		writel(0, reg_base + ehw->hw_cfg->txcmpl.base_offset +
		       (i * ehw->hw_cfg->txcmpl.ring_increment) +
		       ehw->hw_cfg->txcmpl.int_mask.offset);
	}

	/* Clear MISC interrupt mask */
	writel(EDMA_MASK_INT_DISABLE, reg_base + ehw->hw_cfg->global.misc_int_mask.offset);
}

/*
 * EDMA software ring allocation
 */
static int ipq_edma_alloc_rings(struct ipq_edma_hw *ehw)
{
	ehw->rxfill_ring = (void *)mem_alloc((sizeof(struct ipq_edma_rxfill_ring) *
				ehw->rxfill_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->rxfill_ring) {
		pr_info("%s: rxfill_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	ehw->rxdesc_ring = (void *)mem_alloc((sizeof(struct ipq_edma_rxdesc_ring) *
				ehw->rxdesc_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->rxdesc_ring) {
		pr_info("%s: rxdesc_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	ehw->txdesc_ring = (void *)mem_alloc((sizeof(struct ipq_edma_txdesc_ring) *
				ehw->txdesc_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->txdesc_ring) {
		pr_info("%s: txdesc_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	ehw->txcmpl_ring = (void *)mem_alloc((sizeof(struct ipq_edma_txcmpl_ring) *
				ehw->txcmpl_rings),
				ARCH_DMA_MINALIGN);
	if (!ehw->txcmpl_ring) {
		pr_info("%s: txcmpl_ring alloc error\n", __func__);
		return -ENOMEM;
	}

	pr_info("%s: successful\n", __func__);

	return 0;
}

/*
 * EDMA ring initialization
 */
static int ipq_edma_init_rings(struct ipq_edma_hw *ehw)
{
	int ret;

	/* Allocate desc rings */
	ret = ipq_edma_alloc_rings(ehw);
	if (ret)
		return ret;

	/* Setup ring resources */
	ret = ipq_edma_setup_ring_resources(ehw);
	if (ret)
		return ret;

	return 0;
}

/*
 * EDMA TxDesc ring configuration
 */
static void ipq_edma_configure_txdesc_ring(struct ipq_edma_hw *ehw,
					   struct ipq_edma_txdesc_ring *txdesc_ring)
{
	u64 base;
	phys_addr_t reg_base = ehw->iobase;
	u32 ring_dma_mask;
	u32 ring_size_mask;
	phys_addr_t base_addr_reg, base_addr_high_reg, base_addr2_reg, base_addr2_high_reg;
	phys_addr_t ring_size_reg, prod_idx_reg;

	/* Configure TXDESC ring */
	base = txdesc_ring->dma;
	ring_dma_mask = ehw->hw_cfg->ring_dma_mask;
	ring_size_mask = ehw->hw_cfg->txdesc.ring_size.mask;

	/* Calculate register addresses */
	base_addr_reg = reg_base + ehw->hw_cfg->txdesc.base_offset +
			(txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment) +
			ehw->hw_cfg->txdesc.base_addr.offset;

	base_addr_high_reg = reg_base + ehw->hw_cfg->txdesc.base_offset +
			     (txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment) +
			     ehw->hw_cfg->txdesc.base_addr_high.offset;

	writel((u32)(txdesc_ring->dma & ring_dma_mask), base_addr_reg);

	if (base & (~(uint64_t)ring_dma_mask))
		writel((u8)(base >> 32), base_addr_high_reg);

	base = txdesc_ring->sdma;
	base_addr2_reg = reg_base + ehw->hw_cfg->txdesc.base_offset +
			 (txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment) +
			 ehw->hw_cfg->txdesc.base_addr2.offset;

	base_addr2_high_reg = reg_base + ehw->hw_cfg->txdesc.base_offset +
			      (txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment) +
			      ehw->hw_cfg->txdesc.base_addr2_high.offset;

	writel((u32)(txdesc_ring->sdma & ring_dma_mask), base_addr2_reg);

	if (base & (~(uint64_t)ring_dma_mask))
		writel((u8)(base >> 32), base_addr2_high_reg);

	ring_size_reg = reg_base + ehw->hw_cfg->txdesc.base_offset +
			(txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment) +
			ehw->hw_cfg->txdesc.ring_size.offset;

	writel((u32)(txdesc_ring->count & ring_size_mask), ring_size_reg);

	prod_idx_reg = reg_base + ehw->hw_cfg->txdesc.base_offset +
		       (txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment) +
		       ehw->hw_cfg->txdesc.prod_idx.offset;

	writel(EDMA_TX_INITIAL_PROD_IDX, prod_idx_reg);
}

/*
 * EDMA TxCmpl ring configuration
 */
static void ipq_edma_configure_txcmpl_ring(struct ipq_edma_hw *ehw,
					   struct ipq_edma_txcmpl_ring *txcmpl_ring)
{
	u64 base;
	phys_addr_t reg_base = ehw->iobase;
	u32 ring_dma_mask;
	u32 ring_size_mask;

	/* Configure TxCmpl ring base address */
	base = txcmpl_ring->dma;
	ring_dma_mask = ehw->hw_cfg->ring_dma_mask;
	ring_size_mask = ehw->hw_cfg->txcmpl.ring_size.mask;

	writel((u32)(txcmpl_ring->dma & ring_dma_mask),
	       reg_base + ehw->hw_cfg->txcmpl.base_offset +
	       (txcmpl_ring->id * ehw->hw_cfg->txcmpl.ring_increment) +
	       ehw->hw_cfg->txcmpl.base_addr.offset);

	if (base & (~(uint64_t)ring_dma_mask))
		writel((u8)(base >> 32),
		       reg_base + ehw->hw_cfg->txcmpl.base_offset +
		       (txcmpl_ring->id * ehw->hw_cfg->txcmpl.ring_increment) +
		       ehw->hw_cfg->txcmpl.base_addr_high.offset);

	writel((u32)(txcmpl_ring->count & ring_size_mask),
	       reg_base + ehw->hw_cfg->txcmpl.base_offset +
	       (txcmpl_ring->id * ehw->hw_cfg->txcmpl.ring_increment) +
	       ehw->hw_cfg->txcmpl.ring_size.offset);

	/* Set TxCmpl ret mode to opaque */
	writel(EDMA_TXCMPL_RETMODE_OPAQUE,
	       reg_base + ehw->hw_cfg->txcmpl.base_offset +
	       (txcmpl_ring->id * ehw->hw_cfg->txcmpl.ring_increment) +
	       ehw->hw_cfg->txcmpl.ctrl.offset);

	/* Enable ring. Set ret mode to 'opaque'. */
	phys_addr_t int_ctrl_reg = reg_base + ehw->hw_cfg->txcmpl.base_offset +
	       (txcmpl_ring->id * ehw->hw_cfg->txcmpl.ring_increment) +
	       ehw->hw_cfg->txcmpl.int_ctrl.offset;

	writel(EDMA_TX_NE_INT_EN, int_ctrl_reg);
}

/*
 * EDMA RxDesc ring configuration
 */
static void ipq_edma_configure_rxdesc_ring(struct ipq_edma_hw *ehw,
					   struct ipq_edma_rxdesc_ring *rxdesc_ring)
{
	phys_addr_t reg_base = ehw->iobase;
	u32 data;
	u64 base;
	u32 ring_dma_mask;
	phys_addr_t base_addr_reg, base_addr_high_reg, base_addr2_reg, base_addr2_high_reg;
	phys_addr_t fc_thre_reg, ring_size_reg, int_ctrl_reg;

	base = rxdesc_ring->dma;
	ring_dma_mask = ehw->hw_cfg->ring_dma_mask;

	/* Calculate register addresses */
	base_addr_reg = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			(rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment) +
			ehw->hw_cfg->rxdesc.base_addr.offset;

	base_addr_high_reg = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			     (rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment) +
			     ehw->hw_cfg->rxdesc.base_addr_high.offset;

	writel((u32)(rxdesc_ring->dma & ring_dma_mask), base_addr_reg);

	if (base & (~(uint64_t)ring_dma_mask))
		writel((u8)(base >> 32), base_addr_high_reg);

	base = rxdesc_ring->sdma;
	base_addr2_reg = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			 (rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment) +
			 ehw->hw_cfg->rxdesc.base_addr2.offset;

	base_addr2_high_reg = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			      (rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment) +
			      ehw->hw_cfg->rxdesc.base_addr2_high.offset;

	writel((u32)(rxdesc_ring->sdma & ring_dma_mask), base_addr2_reg);

	if (base & (~(uint64_t)ring_dma_mask))
		writel((u8)(base >> 32), base_addr2_high_reg);

	/* Handle payload offset configuration based on SW version */
	if (ehw->sw_version == EDMA_SW_VER_2_ID || ehw->sw_version == EDMA_SW_VER_3_ID) {
		fc_thre_reg = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			      (rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment) +
			      ehw->hw_cfg->rxdesc.fc_thre.offset;

		data = readl(fc_thre_reg);

		data |= (ehw->rx_payload_offset & EDMA_RXDESC_PL_OFFSET_MASK)
			<< EDMA_RXDESC_PL_OFFSET_SHIFT_V2;

		writel(data, fc_thre_reg);

		data = rxdesc_ring->count & EDMA_RXDESC_RING_SIZE_MASK;
	} else {
		data = rxdesc_ring->count & EDMA_RXDESC_RING_SIZE_MASK;
		data |= (ehw->rx_payload_offset & EDMA_RXDESC_PL_OFFSET_MASK)
			<< EDMA_RXDESC_PL_OFFSET_SHIFT;
	}

	ring_size_reg = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			(rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment) +
			ehw->hw_cfg->rxdesc.ring_size.offset;

	writel(data, ring_size_reg);

	/* Enable ring. Set ret mode to 'opaque'. */
	int_ctrl_reg = reg_base + ehw->hw_cfg->int_ctrl_base_offset +
		       (rxdesc_ring->id * ehw->hw_cfg->int_ctrl_ring_increment) +
		       ehw->hw_cfg->int_ctrl_reg_offset;

	writel(EDMA_RX_NE_INT_EN, int_ctrl_reg);
}

/*
 * EDMA RxFill ring configuration
 */
static void ipq_edma_configure_rxfill_ring(struct ipq_edma_hw *ehw,
					   struct ipq_edma_rxfill_ring *rxfill_ring)
{
	phys_addr_t reg_base = ehw->iobase;
	u32 data;
	u64 base;
	u32 ring_dma_mask;
	phys_addr_t base_addr_reg, base_addr_high_reg, ring_size_reg;

	base = rxfill_ring->dma;
	ring_dma_mask = ehw->hw_cfg->ring_dma_mask;

	/* Calculate register addresses */
	base_addr_reg = reg_base + ehw->hw_cfg->rxfill.base_offset +
			(rxfill_ring->id * ehw->hw_cfg->rxfill.ring_increment) +
			ehw->hw_cfg->rxfill.base_addr.offset;

	base_addr_high_reg = reg_base + ehw->hw_cfg->rxfill.base_offset +
			     (rxfill_ring->id * ehw->hw_cfg->rxfill.ring_increment) +
			     ehw->hw_cfg->rxfill.base_addr_high.offset;

	writel((u32)(rxfill_ring->dma & ring_dma_mask), base_addr_reg);

	if (base & (~(uint64_t)ring_dma_mask))
		writel((u8)(base >> 32), base_addr_high_reg);

	data = rxfill_ring->count & EDMA_RXFILL_RING_SIZE_MASK;
	ring_size_reg = reg_base + ehw->hw_cfg->rxfill.base_offset +
			(rxfill_ring->id * ehw->hw_cfg->rxfill.ring_increment) +
			ehw->hw_cfg->rxfill.ring_size.offset;

	writel(data, ring_size_reg);

	/* Note: RXFILL rings use int_mask register, not int_ctrl like RXDESC/TXCMPL */
}

/*
 * EDMA ring configuration - optimized version
 */
static inline void ipq_edma_configure_rings(struct ipq_edma_hw *ehw)
{
	int i;
	struct ipq_edma_txdesc_ring *txdesc_ring;
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
	struct ipq_edma_rxfill_ring *rxfill_ring;
	struct ipq_edma_rxdesc_ring *rxdesc_ring;

	/* Configure TXDESC rings - optimized with cached pointers */
	for (i = 0; i < ehw->txdesc_rings; i++) {
		txdesc_ring = &ehw->txdesc_ring[i];
		ipq_edma_configure_txdesc_ring(ehw, txdesc_ring);
	}

	/* Configure TXCMPL rings - optimized with cached pointers */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		ipq_edma_configure_txcmpl_ring(ehw, txcmpl_ring);
	}

	/* Configure RXFILL rings - optimized with cached pointers */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		ipq_edma_configure_rxfill_ring(ehw, rxfill_ring);
	}

	/* Configure RXDESC rings - optimized with cached pointers */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		ipq_edma_configure_rxdesc_ring(ehw, rxdesc_ring);
	}

	pr_info("%s: successful - %d TX, %d TXCMPL, %d RXFILL, %d RXDESC rings configured\n",
		__func__, ehw->txdesc_rings, ehw->txcmpl_rings,
		ehw->rxfill_rings, ehw->rxdesc_rings);
}

/*
 * EDMA hardware initialization
 */
int ipq_edma_hw_init(struct udevice *dev, struct ipq_eth_dev *eth)
{
	struct edma_config *config =
			(struct edma_config *)dev_get_driver_data(dev);
	struct ipq_edma_rxdesc_ring *rxdesc_ring = NULL;
	struct ipq_edma_hw *ehw = &eth->hw;
	phys_addr_t reg_base = ehw->iobase;
	struct ppe_info *ppe = &eth->ppe;
	int ret, desc_index;
	u32 i, reg, reg_idx, ring_id;
	u32 data;

	/* PPE Init */
	ppe->no_ports = config->ports;
	ppe->nos_iports = config->iports;
	ppe->vsi = config->vsi;
	ppe->tdm_ctrl_val = config->tdm_ctrl_val;
	ppe->ipo_action = config->ipo_action;

	/* Assign port scheduler configuration pointer */
	ppe->port_sched_cfg = port_sched_cfg;
	ppe->port_sched_cfg_len = CONFIG_ETH_MAX_MAC;

	ipq_ppe_provision_init(ppe);

	/* Setup private data structure - must be done before accessing reg_cfg */
	UPDATE_EDMA_CONFIG(config, ehw);
	ehw->sw_version = config->sw_version;
	ehw->hw_cfg = config->hw_cfg;  /* Unified structure support - MUST be set first */
	ehw->rxfill_intr_mask = ehw->hw_cfg->rxfill_int_mask;
	ehw->rxdesc_intr_mask = ehw->hw_cfg->rxdesc_int_mask;
	ehw->txcmpl_intr_mask = ehw->hw_cfg->txcmpl_int_mask;
	ehw->misc_intr_mask = ehw->hw_cfg->misc_intr_mask;
	ehw->rx_payload_offset = ehw->hw_cfg->rx_payload_offset;

	/* Read EDMA version using unified structure */
	data = readl(reg_base + ehw->hw_cfg->global.mas_ctrl.offset);
	printf("EDMA ver %d\n", data);

	/* Disable interrupts */
	ipq_edma_disable_intr(ehw);

	/* Disable rings */
	ipq_edma_disable_rings(ehw);

	ret = ipq_edma_init_rings(ehw);
	if (ret)
		return ret;

	ipq_edma_configure_rings(ehw);

	/* Clear the TXDESC2CMPL_MAP_xx reg before setting up the mapping */
	WRITE_REG_ARRAY(reg_base, ehw->hw_cfg->global.txdesc2cmpl_map_0.offset, 4, 0,
			config->tx_map);

	desc_index = ehw->txcmpl_ring_start;

	/* Configure TX descriptor to completion mapping */
	for (i = ehw->txdesc_ring_start; i < ehw->txdesc_ring_end; i++) {
		if (i >= 0 && i <= 5)
			reg = ehw->hw_cfg->global.txdesc2cmpl_map_0.offset;
		else if (i >= 6 && i <= 11)
			reg = ehw->hw_cfg->global.txdesc2cmpl_map_1.offset;
		else if (i >= 12 && i <= 17)
			reg = ehw->hw_cfg->global.txdesc2cmpl_map_2.offset;
		else if (i >= 18 && i <= 23)
			reg = ehw->hw_cfg->global.txdesc2cmpl_map_3.offset;
		else if (i >= 24 && i <= 29)
			reg = ehw->hw_cfg->global.txdesc2cmpl_map_4.offset;
		else
			reg = ehw->hw_cfg->global.txdesc2cmpl_map_5.offset;

		data = readl(reg_base + reg);
		data |= (desc_index & 0x1F) << ((i % 6) * 5);
		writel(data, reg_base + reg);

		desc_index++;
		if (desc_index == ehw->txcmpl_ring_end)
			desc_index = ehw->txcmpl_ring_start;
	}

	/* Set PPE QID to EDMA Rx ring mapping */
	desc_index = (ehw->rxdesc_ring_start & 0x1f);

	reg = ehw->hw_cfg->qid2rid_base_offset + (0x4 * 0);
	data = ((desc_index << 0) & 0xff) |
	       (((desc_index + 1) << 8) & 0xff00) |
	       (((desc_index + 2) << 16) & 0xff0000) |
	       (((desc_index + 3) << 24) & 0xff000000);

	writel(data, reg_base + reg);

	/* Map PPE multicast queues to the first Rx ring */
	desc_index = (ehw->rxdesc_ring_start & 0x1f);

	for (i = EDMA_CPU_PORT_MC_QID_MIN; i <= EDMA_CPU_PORT_MC_QID_MAX;
			i += EDMA_QID2RID_NUM_PER_REG) {
		reg_idx = i / EDMA_QID2RID_NUM_PER_REG;

		if (ehw->hw_cfg)
			reg = ehw->hw_cfg->qid2rid_base_offset + (0x4 * reg_idx);
		data = ((desc_index << 0) & 0xff) |
		       ((desc_index << 8) & 0xff00) |
		       ((desc_index << 16) & 0xff0000) |
		       ((desc_index << 24) & 0xff000000);

		writel(data, reg_base + reg);
	}

	/* Set RXDESC2FILL_MAP_xx reg */
	WRITE_REG_ARRAY(reg_base, ehw->hw_cfg->global.rxdesc2fill_map_0.offset, 4, 0,
			config->rx_map);

	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];

		ring_id = rxdesc_ring->id;
		if (ring_id >= 0 && ring_id <= 9)
			reg = ehw->hw_cfg->global.rxdesc2fill_map_0.offset;
		else if ((ring_id >= 10) && (ring_id <= 19))
			reg = ehw->hw_cfg->global.rxdesc2fill_map_1.offset;
		else
			reg = ehw->hw_cfg->global.rxdesc2fill_map_2.offset;

		data = readl(reg_base + reg);
		data |= (rxdesc_ring->rxfill->id & 0x7) << ((ring_id % 10) * 3);
		writel(data, reg_base + reg);
	}

	/* Configure DMA request priority, DMA read burst length, and AXI write size */
	data = EDMA_DMAR_BURST_LEN_SET(EDMA_BURST_LEN_ENABLE)
		| EDMA_DMAR_REQ_PRI_SET(0);

	/* Use configurable DMAR_CTRL values - chip-specific differences */
	data |= ((ehw->hw_cfg->dmar_txdata_outstanding_num &
		  ehw->hw_cfg->dmar_txdata_mask)
		 << ehw->hw_cfg->dmar_txdata_shift)
		| ((ehw->hw_cfg->dmar_txdesc_outstanding_num &
		    ehw->hw_cfg->dmar_txdesc_mask)
		   << ehw->hw_cfg->dmar_txdesc_shift)
		| ((ehw->hw_cfg->dmar_rxfill_outstanding_num &
		    ehw->hw_cfg->dmar_rxfill_mask)
		   << ehw->hw_cfg->dmar_rxfill_shift);
	writel(data, reg_base + ehw->hw_cfg->global.dmar_ctrl.offset);

	/* Global EDMA and padding enable */
	writel(ehw->hw_cfg->port_ctrl_init_val, reg_base + ehw->hw_cfg->global.port_ctrl.offset);

	/* Enable Rx rings */
	for (i = ehw->rxdesc_ring_start; i < ehw->rxdesc_ring_end; i++) {
		phys_addr_t ring_ctrl = reg_base + ehw->hw_cfg->rxdesc.base_offset +
			     (i * ehw->hw_cfg->rxdesc.ring_increment) +
			     ehw->hw_cfg->rxdesc.ctrl.offset;

		data = readl(ring_ctrl);

		if (ehw->sw_version != EDMA_SW_VER_3_ID)
			data |= EDMA_RXDESC_RX_EN;

		writel(data, ring_ctrl);
	}

	for (i = ehw->rxfill_ring_start; i < ehw->rxfill_ring_end; i++) {
		phys_addr_t ring_en_addr = reg_base + ehw->hw_cfg->rxfill.base_offset +
			     (i * ehw->hw_cfg->rxfill.ring_increment) +
			     ehw->hw_cfg->rxfill.ring_en.offset;

		data = readl(ring_en_addr);
		data |= EDMA_RXFILL_RING_EN;
		writel(data, ring_en_addr);
	}

	/* Enable Tx rings */
	for (i = ehw->txdesc_ring_start; i < ehw->txdesc_ring_end; i++) {
		phys_addr_t ring_ctrl = reg_base + ehw->hw_cfg->txdesc.base_offset +
			     (i * ehw->hw_cfg->txdesc.ring_increment) +
			     ehw->hw_cfg->txdesc.ctrl.offset;

		data = readl(ring_ctrl);
		data |= EDMA_TXDESC_TX_EN;
		writel(data, ring_ctrl);
	}

	/* Enable MISC interrupt mask */
	{
		phys_addr_t misc_mask_addr = reg_base + ehw->hw_cfg->global.misc_int_mask.offset;

		writel(ehw->misc_intr_mask, misc_mask_addr);
	}

	pr_info("%s: successful\n", __func__);

	return 0;
}

/*
 * Ethernet port configuration and setup
 */
static int ipq_eth_port_set_up(struct ipq_eth_dev *priv,
			       struct port_info *port)
{
	int mac_speed, i, rate = 0;
	int ret = 0;

	switch (port->cur_speed) {
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
		/*
		 * For 2.5G:
		 * - Force mac_speed = 4 for the following UNIPHY modes:
		 *   USXGMII, 10GBASE_R, UQXGMII, UQXGMII_3CHANNELS, UDXGMII
		 * - For other modes, depend on MAC type: XGMAC => 2.5G (4), GMAC => 1G (2)
		 */
		if (port->uniphy_mode == PORT_WRAPPER_USXGMII ||
		    port->uniphy_mode == PORT_WRAPPER_10GBASE_R ||
		    port->uniphy_mode == PORT_WRAPPER_UQXGMII ||
		    port->uniphy_mode == PORT_WRAPPER_UQXGMII_3CHANNELS ||
		    port->uniphy_mode == PORT_WRAPPER_UDXGMII) {
			mac_speed = 4;
		} else {
			mac_speed = port->xgmac ? 4 : 2;
		}
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

		/* Configure RX rate clock if available */
		if (port->rx_clk_rate.dev) {
			/* Only set parent and configure if rx_clk is available */
			if (port->rx_clk.dev) {
				/* Get parent clock for RX rate */
				if (port->uniphy_mode == PORT_WRAPPER_PSGMII ||
				    port->uniphy_mode == PORT_WRAPPER_SGMII0_RGMII4)
					clk_set_rate(&port->rx_clk, CLK_125_MHZ);
				else
					clk_set_rate(&port->rx_clk, CLK_312_5_MHZ);

				ret = clk_set_parent(&port->rx_clk_rate, &port->rx_clk);
				if (ret)
					goto fail;
			} else {
				struct clk pclk;
				pclk.dev = port->rx_clk_rate.dev;
				clk_set_rate(&pclk, 0);
				ret = clk_set_parent(&port->rx_clk_rate, &pclk);
				if (ret)
					goto fail;
			}

			/*
			 * Pack mac_speed (bits 7:0) and gmac_type (bit 8) into
			 * clk->data so the clock driver can identify both the
			 * speed index and MAC type for correct divider selection.
			 *   mac_speed: 0=10M, 1=100M, 2=1G, 3=10G, 4=2.5G, 5=5G
			 *   gmac_type: 0=GMAC, 1=XGMAC
			 */
			port->rx_clk_rate.data =
				(unsigned long)((port->gmac_type << 8) | mac_speed);
			clk_set_rate(&port->rx_clk_rate, rate);
		}

		/* Configure TX rate clock if available */
		if (port->tx_clk_rate.dev) {
			/* Only set parent and configure if tx_clk is available */
			if (port->tx_clk.dev) {
				/* Get parent clock for TX rate */
				if (port->uniphy_mode == PORT_WRAPPER_PSGMII ||
				    port->uniphy_mode == PORT_WRAPPER_SGMII0_RGMII4)
					clk_set_rate(&port->tx_clk, CLK_125_MHZ);
				else
					clk_set_rate(&port->tx_clk, CLK_312_5_MHZ);

				ret = clk_set_parent(&port->tx_clk_rate, &port->tx_clk);
				if (ret)
					goto fail;
			} else {
				struct clk pclk;
				pclk.dev = port->tx_clk_rate.dev;
				clk_set_rate(&pclk, 0);
				ret = clk_set_parent(&port->tx_clk_rate, &pclk);
				if (ret)
					goto fail;
			}

			/*
			 * Pack mac_speed (bits 7:0) and gmac_type (bit 8) into
			 * clk->data so the clock driver can identify both the
			 * speed index and MAC type for correct divider selection.
			 *   mac_speed: 0=10M, 1=100M, 2=1G, 3=10G, 4=2.5G, 5=5G
			 *   gmac_type: 0=GMAC, 1=XGMAC
			 */
			port->tx_clk_rate.data =
				(unsigned long)((port->gmac_type << 8) | mac_speed);
			clk_set_rate(&port->tx_clk_rate, rate);
		}

		/* Enable RX and TX clocks if available */
		if (port->rx_clk.dev) {
			ret = clk_enable(&port->rx_clk);
			if (ret)
				goto fail;
		}

		if (port->tx_clk.dev) {
			ret = clk_enable(&port->tx_clk);
			if (ret)
				goto fail;
		}

		ipq_port_mac_clock_reset(priv->dev, port);

		ppe_port_speed_set(priv->ppe.base, port);

		/* Enable XGMAC PTP REF clock for XGMAC ports at port configure time */
		if (port->gmac_type == XGMAC && port->id > 0) {
			struct clk ptp_clk;
			char ptp_clk_name[32];
			u32 gmacid = ipq_edma_config.port_to_gmacid ?
				     ipq_edma_config.port_to_gmacid(port->id) :
				     port->id - 1;

			if (gmacid != (u32)-1) {
				snprintf(ptp_clk_name, sizeof(ptp_clk_name),
					 "xgmac%d_ptp_ref_clk", gmacid);
				if (!clk_get_by_name(priv->dev, ptp_clk_name, &ptp_clk))
					clk_enable(&ptp_clk);
			}
		}

	} else if (priv->emulation) {
		port->gmac_type = XGMAC;
		port->mac_speed = mac_speed;
		port->uniphy_mode = PORT_WRAPPER_EMULATION;
		ppe_port_speed_set(priv->ppe.base, port);
	}
fail:
	return ret;
}

/*
 * Ethernet interface start
 */
static int ipq_eth_start(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct phy_device *phydev;
	struct port_info *port = NULL;
	int i, ret, link, speed, duplex, linkup = -1;
	ulong active_port = env_get_ulong("active_port", 10,
						CONFIG_ETH_MAX_MAC);

	if (IS_ENABLED(CONFIG_ETH_LOW_MEM))
		dcache_disable();

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

		if (port->phy_id == SFP10G_PHY_TYPE ||
		    port->phy_id == SFP2_5G_PHY_TYPE ||
		    port->phy_id == SFP1G_PHY_TYPE) {
			ret = phy_status_get_from_ppe(priv->ppe.base,
						      port->id);
			link = ((ret & LINK_STATUS) != 0) ? 1 : 0;
			if (link) {
				++linkup;
				duplex = ((ret & DUPLEX) != 0) ? 1 : 0;
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

		if (port->phy_id != QCA8x8x_SWITCH_TYPE &&
		    port->phy_id != QCA8337_SWITCH_TYPE &&
		    port->phy_id != QCE2204_SWITCH_TYPE)
			printf("PHY%d %s Speed : %d %s\n", port->id,
			       link ? "Up" : "Down", speed,
			       duplex ? "Full duplex" : "Half duplex");

		if (!link) {
			/* Disable port */
			ppe_port_bridge_txmac_set(priv->ppe.base, port->id, false);
			port->cur_speed = 10;
			continue;
		}

		if (port->cur_speed != speed) {
			port->cur_speed = speed;
			port->duplex = duplex;
			ipq_eth_port_set_up(priv, port);
		}
	}

	return linkup;
}

/*
 * Ethernet packet send
 */
static int ipq_eth_send(struct udevice *dev, void *packet, int length)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct ipq_edma_hw *ehw = &priv->hw;
	struct ppe_info *ppe = &priv->ppe;
	struct ipq_edma_txdesc_desc *txdesc;
	struct ipq_edma_txdesc_ring *txdesc_ring;
	u16 hw_next_to_use, hw_next_to_clean, chk_idx;
	u32 data;
	uchar *skb;
	phys_addr_t reg_base = ehw->iobase;
	phys_addr_t ring_base;
	u32 dst_info;

	txdesc_ring = ehw->txdesc_ring;
	const u16 ring_mask = txdesc_ring->count - 1;

	/* Cache ring base address to reduce address calculations */
	ring_base = reg_base + ehw->hw_cfg->txdesc.base_offset +
		    (txdesc_ring->id * ehw->hw_cfg->txdesc.ring_increment);

	/* Read TXDESC ring producer index */
	{
		phys_addr_t prod_addr = ring_base + ehw->hw_cfg->txdesc.prod_idx.offset;

		data = readl(prod_addr);
		hw_next_to_use = data & ehw->hw_cfg->txdesc.prod_idx.mask;
	}

	/* Read TXDESC ring consumer index - optimized uncached access */
	{
		phys_addr_t cons_addr = ring_base + ehw->hw_cfg->txdesc.cons_idx.offset;

		data = readl(cons_addr);
		hw_next_to_clean = data & ehw->hw_cfg->txdesc.cons_idx.mask;
	}

	/* Check for available Tx descriptor using bit masking */
	chk_idx = (hw_next_to_use + 1) & ring_mask;

	if (unlikely(chk_idx == hw_next_to_clean)) {
		pr_info("netdev tx busy");
		printf("Tx ring full chk %d == hw_next_to_clean %d\n", chk_idx,
		       hw_next_to_clean);
		return -EBUSY;
	}

	/* Get Tx descriptor */
	txdesc = EDMA_TXDESC_DESC(txdesc_ring, hw_next_to_use);

	/* Bulk clear descriptor fields for better cache efficiency */
	txdesc->tdes2 = 0;
	txdesc->tdes3 = 0;
	txdesc->tdes4 = 0;
	txdesc->tdes5 = 0;
	txdesc->tdes6 = 0;
	txdesc->tdes7 = 0;

	skb = (uchar *)((uintptr_t)(((uint64_t)(txdesc->tdes1 &
				ehw->hw_cfg->txdesc.buf_hi_add_mask) << 32) |
				txdesc->tdes0));

	/* Pre-calculate destination info for better performance */
	if (likely(ppe->bridge_mode)) {
		/* VP 0x0 share vsi 2 with port 1-4 */
		/* src is 0x2000, dest is 0x0 */
		dst_info =  0x00002000;
	} else {
		/*
		 * Populate Tx dst info, port id is macid in dp_dev
		 * We have separate netdev for each port in Kernel but that is not the
		 * case in U-Boot.
		 * This part needs to be fixed to support multiple ports in non bridged
		 * mode during when all the ports are currently under same netdev.
		 */
		dst_info = (EDMA_DST_PORT_TYPE_SET(EDMA_DST_PORT_TYPE) |
			    EDMA_DST_PORT_ID_SET(ppe->nbport));
	}

	/* Efficiently populate descriptor fields */
	txdesc->tdes4 = dst_info;
	txdesc->tdes2 = cpu_to_le32(txdesc->tdes0);
	txdesc->tdes3 = (cpu_to_le32(txdesc->tdes1) & EDMA_TXDESC_BUF_HI_ADD_MASK);
	txdesc->tdes5 = ((length << ehw->hw_cfg->txdesc.data_length_shift) &
			 ehw->hw_cfg->txdesc.data_length_mask);

	/* copy the packet */
	memcpy(skb, packet, length);

	/* Update producer index using bit masking */
	hw_next_to_use = (hw_next_to_use + 1) & ring_mask;

	/* Write to hardware with cached ring base */
	{
		phys_addr_t prod_addr_w = ring_base + ehw->hw_cfg->txdesc.prod_idx.offset;
		u32 prod_val = (hw_next_to_use & ehw->hw_cfg->txdesc.prod_idx.mask);

		writel(prod_val, prod_addr_w);
	}

	/* Ensure memory barrier for descriptor updates */
	wmb();

	return 0;
}

/*
 * Ethernet packet receive
 */
static int ipq_eth_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct ipq_edma_rxdesc_ring *rxdesc_ring;
	struct ipq_edma_txcmpl_ring *txcmpl_ring;
	struct ipq_edma_rxfill_ring *rxfill_ring;
	struct ipq_edma_hw *ehw = &priv->hw;
	phys_addr_t reg_base = ehw->iobase;
	u32 reg_data;
	u32 rxdesc_intr_status = 0;
	u32 txcmpl_intr_status = 0, rxfill_intr_status = 0;
	int i, length = 0;
	phys_addr_t rxdesc_base, txcmpl_base, rxfill_base;

	/* Cache base addresses to reduce repeated calculations */
	rxdesc_base = reg_base + ehw->hw_cfg->rxdesc.base_offset;
	txcmpl_base = reg_base + ehw->hw_cfg->txcmpl.base_offset;
	rxfill_base = reg_base + ehw->hw_cfg->rxfill.base_offset;

	/* Read RxDesc intr status - optimized with cached base */
	for (i = 0; i < ehw->rxdesc_rings; i++) {
		rxdesc_ring = &ehw->rxdesc_ring[i];
		phys_addr_t ring_base = rxdesc_base +
			(rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment);
		phys_addr_t int_stat_addr = ring_base + ehw->hw_cfg->rxdesc.int_stat.offset;

		reg_data = readl(int_stat_addr);

		rxdesc_intr_status |= reg_data & EDMA_RXDESC_RING_INT_STATUS_MASK;

		/* Disable RxDesc intr - single write with cached base */
		writel(EDMA_MASK_INT_DISABLE,
		       ring_base + ehw->hw_cfg->rxdesc.int_mask.offset);
	}

	/* Read TxCmpl intr status - optimized with cached base */
	for (i = 0; i < ehw->txcmpl_rings; i++) {
		txcmpl_ring = &ehw->txcmpl_ring[i];
		phys_addr_t ring_base = txcmpl_base +
			(txcmpl_ring->id * ehw->hw_cfg->txcmpl.ring_increment);
		phys_addr_t int_stat_addr = ring_base + ehw->hw_cfg->txcmpl.int_stat.offset;

		reg_data = readl(int_stat_addr);

		txcmpl_intr_status |= reg_data & EDMA_TXCMPL_RING_INT_STATUS_MASK;

		/* Disable TxCmpl intr - single write with cached base */
		writel(EDMA_MASK_INT_DISABLE,
		       ring_base + ehw->hw_cfg->txcmpl.int_mask.offset);
	}

	/* Read RxFill intr status - optimized with cached base */
	for (i = 0; i < ehw->rxfill_rings; i++) {
		rxfill_ring = &ehw->rxfill_ring[i];
		phys_addr_t ring_base = rxfill_base +
			(rxfill_ring->id * ehw->hw_cfg->rxfill.ring_increment);
		phys_addr_t int_stat_addr = ring_base + ehw->hw_cfg->rxfill.int_stat.offset;

		reg_data = readl(int_stat_addr);

		rxfill_intr_status |= reg_data & EDMA_RXFILL_RING_INT_STATUS_MASK;

		/* Disable RxFill intr - single write with cached base */
		writel(EDMA_MASK_INT_DISABLE,
		       ring_base + ehw->hw_cfg->rxfill.int_mask.offset);
	}

	/* Optimized condition check with likely/unlikely hints */
	if (likely(rxdesc_intr_status != 0 || txcmpl_intr_status != 0 || rxfill_intr_status != 0)) {
		/* Optimized loop with cached base address */
		for (i = 0; i < ehw->rxdesc_rings; i++) {
			rxdesc_ring = &ehw->rxdesc_ring[i];
			phys_addr_t ring_base = rxdesc_base +
				(rxdesc_ring->id * ehw->hw_cfg->rxdesc.ring_increment);

			writel(EDMA_MASK_INT_DISABLE,
			       ring_base + ehw->hw_cfg->rxdesc.int_mask.offset);
		}

		length = ipq_edma_rx_complete(priv, (void **)packetp);
	}

	return length;
}

/*
 * Free received packet
 */
static int ipq_eth_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	return 0;
}

/*
 * Stop ethernet interface
 */
static void ipq_eth_stop(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct phy_device *phydev;
	int i;

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		if (!priv->port[i])
			continue;

		phydev = priv->port[i]->phydev;
		if (phydev && priv->port[i]->isconfigured)
			phy_shutdown(phydev);
	}

	if (IS_ENABLED(CONFIG_ETH_LOW_MEM))
		dcache_enable();
}

/*
 * Write hardware address
 */
static int ipq_eth_write_hwaddr(struct udevice *dev)
{
	return 0;
}

/*
 * Read hardware address
 */
static int ipq_eth_read_hwaddr(struct udevice *dev)
{
	struct eth_pdata *pdata = dev_get_plat(dev);
	uchar enet_addr[6] = { 0 };
	int ret;

	/* Getting the MAC address from ENV */
	ret = eth_env_get_enetaddr_by_index("eth", 0, enet_addr);
	if (ret)
		memcpy(&pdata->enetaddr[0], &enet_addr[0], 6);
	else
		ipq_random_ethaddr(&pdata->enetaddr[0]);

	return 0;
}

/*
 * Bind ethernet device
 */
static int ipq_eth_bind(struct udevice *dev)
{
	return 0;
}

/*
 * PHY hardware reset
 */
static void ipq_eth_phy_hw_reset(struct gpio_desc *gpio)
{
	u32 data;

	data = dm_gpio_get_value(gpio);
	data |= BIT(1);
	dm_gpio_set_value(gpio, 0);
		mdelay(500);
	dm_gpio_set_value(gpio, data);
}

#ifdef CONFIG_PHY_QCA_8X8X
/*
 * QCA 8x8x switch write function
 */
static void ipq_eth_8x8x_write(struct phy_device *phydev, u32 reg, u32 val)
{
	u16 r1, r2, page, switch_phy_id;
	u16 lo = val & 0xffff;
	u16 hi = (u16)(val >> 16);

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
}

/*
 * QCA 8x8x switch pre-initialization
 */
static void ipq_eth_8x8x_pre_init(struct mii_dev *bus)
{
	struct phy_device temp_phydev;
	u32 phy_data;

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

	if (phy_data == 0x20c41)
		return;

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

/*
 * SFP module detection
 */
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

/*
 * Configure uniphy 50MHz clock
 */
static void ipq_eth_configure_uniphy_50m(int count, phys_addr_t uniphy_base,
					 size_t size)
{
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

/*
 * Ethernet device probe
 */
static int ipq_eth_probe(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct udevice *busdev;
	struct port_info *port;
	struct clk clk;
	struct reset_ctl_bulk resets;
	int clk_itr, clk_cnt, ret, i, configured = 0;
	const char **clk_names = NULL;
#if defined(CONFIG_PHY_QCA_8X8X) || defined(CONFIG_PHY_QCE_1204)
	int phy_no = 0;
#endif

	mem_init();

	/* Step 1: Enable all clocks (clocks must be stable before reset) */
	clk_cnt = dev_read_string_list(dev, "clock-names", &clk_names);
	if (clk_cnt <= 0) {
		dev_err(dev, "Failed to get clock names\n");
		goto fail;
	}

	for (clk_itr = 0; clk_itr < clk_cnt; clk_itr++) {
		/*
		 * Uniphy clocks: only enable if the corresponding uniphy
		 * is present and enabled in the SKU. Skip disabled uniphys to
		 * avoid enabling clocks for hardware that is not available.
		 *
		 * Supports two naming patterns:
		 * - gcc_uniphy0_sys_clk, gcc_uniphy0_ahb_clk
		 * - uniphy0_ahb_clk, uniphy0_sys_clk
		 */
		if (!strncmp(clk_names[clk_itr], "gcc_uniphy", 10)) {
			int uniphy_id = clk_names[clk_itr][10] - '0';

			if (ipq_uniphy && ipq_uniphy[uniphy_id].status != SKU_ENABLED)
				continue;
		} else if (!strncmp(clk_names[clk_itr], "uniphy", 6) &&
			   isdigit(clk_names[clk_itr][6])) {
			int uniphy_id = clk_names[clk_itr][6] - '0';

			if (ipq_uniphy && ipq_uniphy[uniphy_id].status != SKU_ENABLED)
				continue;
		}

		/* Skip xgmac PTP REF clocks - enabled at port configure time */
		if (strstr(clk_names[clk_itr], "_ptp_ref_clk"))
			continue;

		ret = clk_get_by_name(dev, clk_names[clk_itr], &clk);
		if (ret && ret != -ENOENT) {
			dev_err(dev, "Failed to get clock '%s': %d\n",
				clk_names[clk_itr], ret);
			goto fail;
		}

		ret = clk_enable(&clk);
		if (ret) {
			dev_err(dev, "Failed to enable clock '%s': %d\n",
				clk_names[clk_itr], ret);
			goto fail;
		}
	}

	/* Step 2: Assert/deassert resets after clocks are stable */
	ret = reset_get_bulk(dev, &resets);
	if (ret && ret != -ENOENT) {
		dev_err(dev, "Can't get reset: %d\n", ret);
		return -ENODEV;
	}

	ret = reset_assert_bulk(&resets);
	if (ret)
		return ret;

	mdelay(50);

	ret = reset_deassert_bulk(&resets);
	if (ret)
		return ret;

	/*
	 * If uniphy PMA resets are described in the DTS node, handle them
	 * individually in the required sequence. uniphy0_pma_rst is mandatory
	 * to trigger this path; uniphy1_pma_rst and uniphy2_pma_rst are
	 * optional (SoCs may have 1, 2, or 3 uniphys). refgen_core_ares and
	 * refgen_hclk_ares are also optional.
	 * Reset names: uniphy0_pma_rst[, uniphy1_pma_rst[, uniphy2_pma_rst]],
	 *              [refgen_core_ares], [refgen_hclk_ares]
	 */
	{
		static const char * const uniphy_pma_names[] = {
			"uniphy0_pma_rst",
			"uniphy1_pma_rst",
			"uniphy2_pma_rst",
		};
		struct reset_ctl uniphy_pma[3];
		bool uniphy_pma_valid[3] = {false, false, false};
		struct reset_ctl refgen_core, refgen_hclk;
		bool refgen_core_valid = false, refgen_hclk_valid = false;
		int idx;

		/* Only enter this path if uniphy0_pma_rst is present */
		if (!reset_get_by_name(dev, uniphy_pma_names[0], &uniphy_pma[0])) {
			uniphy_pma_valid[0] = true;

			/* uniphy1 and uniphy2 PMA resets are optional */
			for (idx = 1; idx < 3; idx++) {
				if (!reset_get_by_name(dev, uniphy_pma_names[idx],
						       &uniphy_pma[idx]))
					uniphy_pma_valid[idx] = true;
			}

			/* refgen resets are optional */
			if (!reset_get_by_name(dev, "refgen_core_ares", &refgen_core))
				refgen_core_valid = true;
			if (!reset_get_by_name(dev, "refgen_hclk_ares", &refgen_hclk))
				refgen_hclk_valid = true;

			/* Assert present uniphy PMA resets */
			for (idx = 0; idx < 3; idx++) {
				if (uniphy_pma_valid[idx]) {
					reset_assert(&uniphy_pma[idx]);
					mdelay(10);
				}
			}

			/* Assert present refgen resets */
			if (refgen_core_valid) {
				reset_assert(&refgen_core);
				mdelay(10);
			}
			if (refgen_hclk_valid) {
				reset_assert(&refgen_hclk);
				mdelay(10);
			}

			/* Deassert present uniphy PMA resets */
			for (idx = 0; idx < 3; idx++) {
				if (uniphy_pma_valid[idx])
					reset_deassert(&uniphy_pma[idx]);
			}

			/* Deassert present refgen resets */
			if (refgen_core_valid) {
				reset_deassert(&refgen_core);
				mdelay(10);
			}
			if (refgen_hclk_valid) {
				reset_deassert(&refgen_hclk);
				mdelay(10);
			}
		}
	}

	/* Step 3: Configure CMN clock */
	ipq_config_cmn_clock();

	if (priv->uniphy_50mhz)
		ipq_eth_configure_uniphy_50m(priv->uniphy_50mhz,
					     priv->uniphy_base,
						priv->uniphy_size);

	uniphy_set_base_addr(priv->uniphy_base);

	current_csr_version = uniphy_get_csr_version();
	current_reset_version = uniphy_get_reset_version();

	ipq_edma_hw_init(dev, priv);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		port = priv->port[i];
		if (!port)
			continue;

		if (priv->emulation) {
			port->isconfigured = true;
			++configured;
			continue;
		}

		port->dev = dev;

		/* SoC-specific one-time port initialization (sets function pointers) */
		port_init(port);

		/* Only set uniphy_base if uniphy_id is valid */
		if (port->uniphy_id != 0xFF && port->uniphy_id < CONFIG_ETH_MAX_UNIPHY)
			port->uniphy_base = priv->uniphy_base +
						(port->uniphy_id * priv->uniphy_size);
		else {
			port->uniphy_mode = PORT_WRAPPER_NA;
			port->cur_uniphy_mode = PORT_WRAPPER_NA;
		}

#ifdef CONFIG_MDIO_QCOM_I2C
		if (port->i2c_bus) {
			ret = uclass_get_device_by_phandle_id(UCLASS_I2C, port->i2c_bus, &busdev);
			if (ret) {
				printf("%s: failed to get i2c bus, err: %d\n", __func__, ret);
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

		if (port->phyaddr != 0xFF) {
			if (!port->bus) {
				ret = uclass_get_device_by_ofnode(UCLASS_MDIO,
								  port->pnode,
								  &busdev);
				if (ret)
					continue;

				port->bus = miiphy_get_dev_by_name(busdev->name);
			}

			if (!port->bus)
				continue;
		}
		/*
		 * Create a dummy bus for the SFP module that is not connected via I2C
		 * on older SoCs, to prevent it from being skipped during validation checks.
		 */
		if (!port->bus) {
			port->bus = mdio_alloc();
			if (!port->bus) {
				pr_err("failed to allocate MDIO bus\n");
				return -ENOMEM;
			}

			port->bus->read = NULL;
			port->bus->write = NULL;
			port->bus->priv = NULL;
			strlcpy(port->bus->name, "SFP-DUMMY", sizeof(port->bus->name));
		}

		if (port->rst_gpio.dev)
			ipq_eth_phy_hw_reset(&port->rst_gpio);

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
			port->gmac_type = GMAC;
			port->cur_gmac_type = GMAC;
			ppe_uniphy_mode_set(port);
			ppe_port_mux_set(priv->ppe.base, port);
			ppe_port_bridge_txmac_set(priv->ppe.base, port->id, false);
			if (port->isforce_speed)
				ppe_uniphy_set_forcemode(port);
		}
#endif

#if defined(CONFIG_PHY_QCA_8337) || defined(CONFIG_PHY_QCA_8033)
		if (port->phy_25mhz)
			ppe_uniphy_refclk_set_25M(port);
#endif

#ifdef CONFIG_PHY_QCA_8X8X
		if (port->phy_id == QCA8x8x_PHY_TYPE || port->phy_id == QCA8x8x_SWITCH_TYPE)
			ipq_eth_8x8x_pre_init(port->bus);
#endif
		if (port->phy_id == SFP10G_PHY_TYPE || port->phy_id == SFP2_5G_PHY_TYPE ||
		    port->phy_id == SFP1G_PHY_TYPE) {
			port->phydev = phy_device_create(port->bus, port->phyaddr,
							 PHY_FIXED_ID, true);
			if (IS_ERR_OR_NULL(port->phydev))
				continue;

			port->phydev->dev = dev;
			port->phydev->interface = port->interface;
		} else {
			port->phydev = phy_connect(port->bus, port->phyaddr, dev, port->interface);
		}

		if (IS_ERR_OR_NULL(port->phydev))
			continue;

		if (ofnode_valid(port->node))
			port->phydev->node = port->node;

#if defined(CONFIG_PHY_QCA_8X8X) || defined(CONFIG_PHY_QCE_1204)
		/*
		 * configure UQXGMII for pure PHY mode since MHT PHY requires
		 * uniphy pre-init before configuring uniphy mode, which has
		 * to be configured by default to UQXGMII mode regardless of
		 * speed link up.
		 */
		if (port->phy_id == QCA8x8x_PHY_TYPE ||
			port->phy_id == QCE1204_PHY_TYPE) {
			/* Default UNIPHY to UQXGMII for MHT PHY pre-init */
			port->uniphy_mode = PORT_WRAPPER_UQXGMII;
			port->cur_uniphy_mode = PORT_WRAPPER_UQXGMII;

			/* Select MAC type once and mirror to current */
			port->gmac_type = port->xgmac ? XGMAC : GMAC;
			port->cur_gmac_type = port->gmac_type;

			/* Pre-init UNIPHY only for first PHY instance */
			if (phy_no == 0)
				ppe_uniphy_mode_set(port);

			/* Program port mux once after mode/type selection */
			ppe_port_mux_set(priv->ppe.base, port);
			++phy_no;
		} else if (port->phy_id == QCA8x8x_SWITCH_TYPE) {
			port->uniphy_mode = PORT_WRAPPER_SGMII_PLUS;
			port->cur_uniphy_mode = PORT_WRAPPER_SGMII_PLUS;
			ppe_uniphy_mode_set(port);
		} else if (port->phy_id == QCE2204_SWITCH_TYPE) {
			port->uniphy_mode = PORT_WRAPPER_10GBASE_R;
			port->cur_uniphy_mode = PORT_WRAPPER_10GBASE_R;
			ppe_uniphy_mode_set(port);
		}
#endif

#ifdef CONFIG_PHY_AQUANTIA
		if (port->phy_id == AQ_PHY_TYPE) {
			if (!ipq_aquantia_load_fw(port->phydev)) {
				port->fw_loaded = true;
				mdelay(100);
			} else {
				port->fw_loaded = false;
			}
		}
#endif

#ifdef CONFIG_PHY_QCA_81XX
		if (port->phy_id == QCA81xx_PHY_TYPE) {
			port->uniphy_mode = PORT_WRAPPER_USXGMII;
			port->cur_uniphy_mode = PORT_WRAPPER_USXGMII;
			port->gmac_type = XGMAC;
			port->cur_gmac_type = XGMAC;
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
	free(clk_names);

	return !configured;
}

/*
 * Remove ethernet device
 */
static int ipq_eth_remove(struct udevice *dev)
{
	struct ipq_eth_dev *priv = dev_get_priv(dev);
	struct ipq_edma_hw *ehw = &priv->hw;
	int i;

	/*
	 * Disable global edma
	 */
	writel(0, ehw->iobase + ehw->hw_cfg->global.port_ctrl.offset);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		if (priv->port[i]) {
			ppe_port_bridge_txmac_set(priv->ppe.base, priv->port[i]->id, false);

			/* Stop the PHY driver before forcing BMCR power-down. */
			if (priv->port[i]->phydev) {
				/* Set BMCR.PDOWN while preserving the other BMCR bits. */
				phy_modify(priv->port[i]->phydev, MDIO_MMD_PMAPMD,
					   MII_BMCR, BMCR_PDOWN, BMCR_PDOWN);
			}

			free(priv->port[i]);
			priv->port[i] = NULL;
		}
	}

	/*
	 * Disable interrupts
	 */
	ipq_edma_disable_intr(ehw);

	/*
	 * Disable rings
	 */
	ipq_edma_disable_rings(ehw);

	ipq_edma_clear_ring_bases(ehw);

	return 0;
}

/*
 * Ethernet operations structure
 */
static const struct eth_ops ipq_eth_ops = {
	.start			= ipq_eth_start,
	.send			= ipq_eth_send,
	.recv			= ipq_eth_recv,
	.free_pkt		= ipq_eth_free_pkt,
	.stop			= ipq_eth_stop,
	.write_hwaddr		= ipq_eth_write_hwaddr,
	.read_rom_hwaddr        = ipq_eth_read_hwaddr,
};

/*
 * Parse L1 scheduler configuration from device tree
 */
static bool ipq_parse_l1_scheduler_config(ofnode port_node, u32 port_id,
					  struct port_scheduler_cfg *psc)
{
	ofnode l1 = ofnode_find_subnode(port_node, "l1scheduler");
	ofnode grp0;
	u32 sp;
	u32 cfg_vals[4];

	if (!ofnode_valid(l1))
		return false;

	grp0 = ofnode_find_subnode(l1, "group@0");
	if (!ofnode_valid(grp0))
		return false;

	/*
	 * l1_sp_id is the port index;
	 * l1_map_index comes from 'sp'
	 */
	if (ofnode_read_u32(grp0, "sp", &sp))
		return false;

	psc->l1_sp_id = port_id;
	psc->l1_map_index = sp;

	/*
	 * Parse cfg array:
	 * <c_pri c_drr_id e_pri e_drr_id>
	 */
	if (ofnode_read_u32_array(grp0, "cfg", cfg_vals, 4))
		return false;

	psc->l1_c_pri    = cfg_vals[0];
	psc->l1_c_drr_id = cfg_vals[1];
	psc->l1_e_pri    = cfg_vals[2];
	psc->l1_e_drr_id = cfg_vals[3];

	/* Weights are always 1 */
	psc->l1_c_drr_wt = 1;
	psc->l1_e_drr_wt = 1;

	return true;
}

/*
 * Parse L0 scheduler configuration from device tree
 */
static bool ipq_parse_l0_scheduler_config(ofnode port_node,
					  struct port_scheduler_cfg *psc)
{
	ofnode l0 = ofnode_find_subnode(port_node, "l0scheduler");
	ofnode grp0;
	u32 cfg_vals[5];

	if (!ofnode_valid(l0))
		return false;

	grp0 = ofnode_find_subnode(l0, "group@0");
	if (!ofnode_valid(grp0))
		return false;

	/* Queues from group@0 */
	if (ofnode_read_u32(grp0, "ucast_queue", &psc->l0_uc_queue_id))
		return false;

	if (ofnode_read_u32(grp0, "mcast_queue", &psc->l0_mc_queue_id))
		return false;

	/*
	 * Parse cfg array:
	 * <sp c_pri c_drr_id e_pri e_drr_id>
	 */
	if (ofnode_read_u32_array(grp0, "cfg", cfg_vals, 5))
		return false;

	psc->l0_sp_id    = cfg_vals[0];
	psc->l0_c_pri    = cfg_vals[1];
	psc->l0_c_drr_id = cfg_vals[2];
	psc->l0_e_pri    = cfg_vals[3];
	psc->l0_e_drr_id = cfg_vals[4];

	/* Weights are always 1 */
	psc->l0_c_drr_wt = 1;
	psc->l0_e_drr_wt = 1;

	return true;
}

/*
 * Parse port scheduler configuration for a single port
 */
static void ipq_parse_single_port_scheduler(ofnode port_node)
{
	u32 port_id;
	struct port_scheduler_cfg *psc;
	bool l1_ok = false;
	bool l0_ok = false;

	if (ofnode_read_u32(port_node, "port_id", &port_id))
		return;

	psc = &port_sched_cfg[port_id];
	psc->valid = false;

	/*
	 * L1: l1scheduler/group@0: sp and cfg =
	 * <c_pri c_drr_id e_pri e_drr_id>
	 */
	l1_ok = ipq_parse_l1_scheduler_config(port_node, port_id, psc);

	/*
	 * L0: l0scheduler/group@0: queues and cfg =
	 * <sp c_pri c_drr_id e_pri e_drr_id>
	 */
	l0_ok = ipq_parse_l0_scheduler_config(port_node, psc);

	/* Set validity once per port after both L1 and L0 parsing */
	psc->valid = l1_ok && l0_ok;
}

/*
 * Parse port scheduler resource configuration for a single port
 */
static void ipq_parse_single_port_scheduler_resource(ofnode port_node)
{
	u32 port_id;
	u32 range[2];
	struct port_scheduler_cfg *psc;

	if (ofnode_read_u32(port_node, "port_id", &port_id))
		return;

	psc = &port_sched_cfg[port_id];

	/* Parse ucast_queue = <start end> */
	if (!ofnode_read_u32_array(port_node, "ucast_queue", range, 2)) {
		psc->ucast_queue_start = range[0];
		psc->ucast_queue_end = range[1];
	}

	/* Parse mcast_queue = <start end> */
	if (!ofnode_read_u32_array(port_node, "mcast_queue", range, 2)) {
		psc->mcast_queue_start = range[0];
		psc->mcast_queue_end = range[1];
	}

	/* Parse l0cdrr = <start end> */
	if (!ofnode_read_u32_array(port_node, "l0cdrr", range, 2)) {
		psc->l0cdrr_start = range[0];
		psc->l0cdrr_end = range[1];
	}
}

/*
 * Helper function to get clock with fallback naming
 * Supports both 2-level and 3-level fallback based on parameters
 */
static int ipq_get_clock_with_fallback(struct udevice *dev,
				       const char *fmt1,
				       const char *fmt2,
				       const char *fmt3,
				       struct clk *clk,
				       const char *desc,
				       u32 port_id,
				       u32 uniphy_id)
{
	char clk_name[64];
	int ret, len;

	/* Try first clock name format */
	if (fmt1[0] == 'u') {
		/* Format with two parameters: uniphy%d_port%d_... */
		len = snprintf(clk_name, sizeof(clk_name), fmt1, uniphy_id, port_id);
	} else if (strstr(fmt1, "uniphy")) {
		/* Special format: nss_port%d_uniphy%d_... */
		len = snprintf(clk_name, sizeof(clk_name), fmt1, port_id, uniphy_id);
	} else {
		/* Format with one parameter: nss_port%d_... */
		len = snprintf(clk_name, sizeof(clk_name), fmt1, port_id);
	}

	if (len >= sizeof(clk_name)) {
		debug("Clock name too long for %s port %d\n", desc, port_id);
		return -ENAMETOOLONG;
	}

	ret = clk_get_by_name(dev, clk_name, clk);
	if (!ret)
		return 0;

	/* Try second clock name format */
	len = snprintf(clk_name, sizeof(clk_name), fmt2, port_id);
	if (len >= sizeof(clk_name)) {
		debug("Clock name too long for %s port %d\n", desc, port_id);
		return -ENAMETOOLONG;
	}

	ret = clk_get_by_name(dev, clk_name, clk);
	if (!ret)
		return 0;

	/* Try third clock name format if provided */
	if (fmt3) {
		len = snprintf(clk_name, sizeof(clk_name), fmt3, port_id);
		if (len >= sizeof(clk_name)) {
			debug("Clock name too long for %s port %d\n", desc, port_id);
			return -ENAMETOOLONG;
		}

		ret = clk_get_by_name(dev, clk_name, clk);
	}

	if (ret)
		debug("Failed to get %s for port %d: %d\n", desc, port_id, ret);

	return ret;
}

/*
 * Initialize all clocks for a port
 */
static void ipq_port_clock_init(struct udevice *dev, struct port_info *port)
{
	const char *rx_fmt1, *rx_fmt2, *rx_fmt3;
	const char *tx_fmt1, *tx_fmt2, *tx_fmt3;
	bool has_uniphy = (port->uniphy_id != 0xFF);
	bool multi_path = has_uniphy &&
			  (((port->id == 4) && (port->uniphy_id == 1)) ||
			   ((port->id == 5) && (port->uniphy_id == 0)));

	/* Determine rate clock naming based on port configuration */
	if (multi_path) {
		/* Three-level fallback for special multi-path ports */
		rx_fmt1 = "nss_port%d_uniphy%d_rx_clk";
		rx_fmt2 = "nss_port%d_rx_clk";
		rx_fmt3 = "nss_cc_port%d_rx_clk";
		tx_fmt1 = "nss_port%d_uniphy%d_tx_clk";
		tx_fmt2 = "nss_port%d_tx_clk";
		tx_fmt3 = "nss_cc_port%d_tx_clk";
	} else {
		/* Two-level fallback for normal ports */
		rx_fmt1 = "nss_port%d_rx_clk";
		rx_fmt2 = "nss_cc_port%d_rx_clk";
		rx_fmt3 = NULL;
		tx_fmt1 = "nss_port%d_tx_clk";
		tx_fmt2 = "nss_cc_port%d_tx_clk";
		tx_fmt3 = NULL;
	}

	/* Initialize rate clocks (always present) */
	ipq_get_clock_with_fallback(dev, rx_fmt1, rx_fmt2, rx_fmt3,
				     &port->rx_clk_rate, "RX rate clock",
				     port->id, port->uniphy_id);

	ipq_get_clock_with_fallback(dev, tx_fmt1, tx_fmt2, tx_fmt3,
				     &port->tx_clk_rate, "TX rate clock",
				     port->id, port->uniphy_id);

	/* Initialize enable clocks (only for ports with uniphy) */
	if (has_uniphy) {
		ipq_get_clock_with_fallback(dev, "uniphy%d_port%d_rx_clk",
					     "nss_cc_uniphy_port%d_rx_clk",
					     NULL,
					     &port->rx_clk, "RX enable clock",
					     port->id, port->uniphy_id);

		ipq_get_clock_with_fallback(dev, "uniphy%d_port%d_tx_clk",
					     "nss_cc_uniphy_port%d_tx_clk",
					     NULL,
					     &port->tx_clk, "TX enable clock",
					     port->id, port->uniphy_id);
	} else {
		/* No uniphy: set enable clocks to NULL */
		port->rx_clk.dev = NULL;
		port->tx_clk.dev = NULL;
	}
}

/*
 * Device tree data to platform data conversion
 */
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

	priv->emulation = dev_read_bool(dev, "qcom,emulation");

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
	if (priv->uniphy_base == FDT_ADDR_T_NONE && !priv->emulation) {
		dev_err(dev, "uniphy_base bus address not found\n");
		return -EINVAL;
	}

	ppe->tdm_offset = tdm_addr_config->tdm_addr.offset;

	priv->uniphy_50mhz = dev_read_u32_default(dev, "50mhz", 0);

	ppe->tdm_mode = dev_read_u32_default(dev, "tdm_mode", 0);
	ppe->tm_tick_mode = dev_read_u32_default(dev, "tm_tick_mode", 0);
	ppe->no_reg = dev_read_u32_default(dev, "no_tdm_reg", 0);
	ppe->tm = dev_read_bool(dev, "tdm_tm_support");
	ppe->bridge_mode = dev_read_bool(dev, "bridge_mode");
	if (!ppe->bridge_mode)
		ppe->nbport = dev_read_u32_default(dev, "port", 0);

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		struct port_info *port = NULL;
		int len;

		len = snprintf(phy_handle, sizeof(phy_handle), "phy-handle%d", i);
		if (len < 0 || len >= sizeof(phy_handle))
			continue;

		if (!dev_read_phandle_with_args(dev, phy_handle, NULL, 0, 0, &phandle_args)) {
			u8 uniphy_id = ofnode_read_u32_default(phandle_args.node, "uniphy_id", -1);

			if (-1 == uniphy_id)
				continue;

			if (ipq_uniphy && uniphy_id >= 0 && uniphy_id < CONFIG_ETH_MAX_UNIPHY) {
				/* Guard against uninitialized global entries */
				if (ipq_uniphy[uniphy_id].reg && ipq_uniphy[uniphy_id].bit < 32) {
					u32 reg_val = readl(ipq_uniphy[uniphy_id].reg);

					if (reg_val & (1U << ipq_uniphy[uniphy_id].bit)) {
						printf("UNIPHY%d is Disabled\n", uniphy_id);
						continue;
					}
					ipq_uniphy[uniphy_id].status = SKU_ENABLED;
				}
			}

			port = malloc_cache_aligned(sizeof(struct port_info));
			if (!port)
				return -ENOMEM;

			memset(port, 0, sizeof(struct port_info));

			port->cur_uniphy_mode = -1;
			port->cur_speed = -1;
			port->cur_gmac_type = -1;
			port->uniphy_id = uniphy_id;
			port->node = phandle_args.node;
			port->pnode = ofnode_get_parent(phandle_args.node);
			port->phyaddr = ofnode_read_u32_default(phandle_args.node, "phy_addr", -1);
			port->phy_id = ofnode_read_u32_default(phandle_args.node, "phy_type", -1);
			port->id = ofnode_read_u32_default(phandle_args.node, "id", -1);
			port->uniphy_type = ofnode_read_u32_default(phandle_args.node,
								    "uniphy_type", 0);
			port->max_speed = ofnode_read_u32_default(phandle_args.node,
								  "max_speed", -1);
			port->isforce_speed = ofnode_read_bool(phandle_args.node, "force-speed");
			port->xgmac = ofnode_read_bool(phandle_args.node, "xgmac");
			port->phy_25mhz = ofnode_read_bool(phandle_args.node, "25M");
			port->i2c_bus = ofnode_read_u32_default(phandle_args.node, "i2c-bus", 0);
			port->interface = ofnode_read_phy_mode(phandle_args.node);

			gpio_request_by_name_nodev(phandle_args.node, "phy-reset-gpio", 0,
						   &port->rst_gpio, GPIOD_IS_OUT);

			/* Initialize all clocks for this port */
			ipq_port_clock_init(dev, port);

			priv->port[port_count++] = port;
		}
	}

	/* Parse port_scheduler_config group@0 for L1 & L0 scheduler (per-port) */
	{
		ofnode ps_cfg = ofnode_find_subnode(dev_ofnode(dev), "port_scheduler_config");

		if (ofnode_valid(ps_cfg)) {
			ofnode port_node;

			for (port_node = ofnode_first_subnode(ps_cfg);
			     ofnode_valid(port_node);
			     port_node = ofnode_next_subnode(port_node)) {
				ipq_parse_single_port_scheduler(port_node);
			}
		}
	}

	/* Parse port_scheduler_resource for resource ranges (per-port) */
	{
		ofnode ps_res = ofnode_find_subnode(dev_ofnode(dev), "port_scheduler_resource");

		if (ofnode_valid(ps_res)) {
			ofnode port_node;

			for (port_node = ofnode_first_subnode(ps_res);
			     ofnode_valid(port_node);
			     port_node = ofnode_next_subnode(port_node)) {
				ipq_parse_single_port_scheduler_resource(port_node);
			}
		}
	}

	return 0;
}

/*
 * Device ID table
 */
static const struct udevice_id ipq_eth_ids[] = {
	{ .compatible = "qcom,ipq-nss-switchv2",
	  .data = (ulong)&ipq_edma_config },
	{ }
};

/*
 * U-Boot driver structure
 */
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
	.flags = DM_FLAG_ALLOC_PRIV_DMA | DM_FLAG_OS_PREPARE | DM_FLAG_ACTIVE_DMA,
};

#ifdef CONFIG_PHY_AQUANTIA
/*
 * Aquantia firmware loading command
 */
static int do_aqloadfw(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	static struct udevice *dev;
	static struct ipq_eth_dev *priv;
	int i;
	bool reload = false;
	bool debug = false;
	u8 phyaddr;

	if (argc < 2 || argc > 3)
		return CMD_RET_USAGE;

	if (argc == 3) {
		char opt = argv[2][0];

		if (opt == 'r')
			reload = true;
		else if (opt == 'd')
			debug = true;
		else
			return CMD_RET_USAGE;
	}

	phyaddr = (u8)dectoul(argv[1], NULL);

#if defined(CONFIG_CMD_NET) && defined(CONFIG_ETH_SKIP_INIT_R)
	initr_net();
#endif

	if (!dev) {
		dev = eth_get_dev_by_name("nss-switch");
		if (!dev) {
			printf("Failed to find nss-switch device\n");
			return CMD_RET_FAILURE;
		}
	}

	if (!priv) {
		priv = dev_get_priv(dev);
		if (!priv) {
			printf("Failed to find priv pointer\n");
			return CMD_RET_FAILURE;
		}
	}

	for (i = 0; i < CONFIG_ETH_MAX_MAC; ++i) {
		struct port_info *port = priv->port[i];

		if (!port)
			continue;

		if (port->phy_id != AQ_PHY_TYPE)
			continue;

		if (port->phyaddr != phyaddr)
			continue;

		if (reload)
			port->fw_loaded = false;

		if (port->fw_loaded) {
			if (debug)
				printf("AQ port %d FW already loaded\n",
				       phyaddr);
			continue;
		}

		if (!ipq_aquantia_load_fw(port->phydev)) {
			port->fw_loaded = true;
			mdelay(100);
		}

		break;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(aq_load_fw, 3, 0, do_aqloadfw,
	   "Load firmware to AQ port",
	   "phy_addr --> phy address of AQ port\n"
	   "[r|d] - Optional: 'r' for reload, 'd' for debug logs\n");
#endif /* CONFIG_PHY_AQUANTIA */

/* ========================================================================
 * uniphy_csr - UNIPHY CSR read/write command
 *
 * Usage:
 *   uniphy_csr read  <uniphy_index> <register> <csr_type>
 *   uniphy_csr write <uniphy_index> <register> <csr_type> <value>
 *
 *   uniphy_index : 0, 1, or 2
 *   register     : register address (hex)
 *   csr_type     : 0 = CSR0 direct, 1 = CSR1 indirect, 2 = CSR2 indirect
 *                  (CSR V1 only supports csr_type 1)
 *   value        : value to write (hex, write only)
 *
 * The command uses the current uniphy_base_addr and current_csr_version
 * globals set at boot time.
 * ========================================================================
 */
static int do_uniphy_csr(struct cmd_tbl *cmdtp, int flag, int argc,
			 char *const argv[])
{
	int uniphy_index;
	u32 reg, csr_type, encoded_addr, value;
	bool is_write;

	if (argc < 5)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "read")) {
		is_write = false;
		if (argc != 5)
			return CMD_RET_USAGE;
	} else if (!strcmp(argv[1], "write")) {
		is_write = true;
		if (argc != 6)
			return CMD_RET_USAGE;
	} else {
		return CMD_RET_USAGE;
	}

	uniphy_index = (int)simple_strtoul(argv[2], NULL, 0);
	reg          = (u32)simple_strtoul(argv[3], NULL, 0);
	csr_type     = (u32)simple_strtoul(argv[4], NULL, 0);

	/* Validate uniphy index */
	if (uniphy_index < 0 || uniphy_index > 2) {
		printf("Error: uniphy_index must be 0, 1, or 2\n");
		return CMD_RET_FAILURE;
	}

	if (current_csr_version == CSR_VERSION_V1) {
		/* V1: only one CSR block, csr_type must be 0 */
		if (csr_type != 1) {
			printf("Error: CSR V1 only supports csr_type 0\n");
			return CMD_RET_FAILURE;
		}
		/* Strip any encoding bits; V1 uses raw address */
		encoded_addr = reg & UNIPHY_REG_ADDR_MASK;
	} else {
		/* V2: encode csr_type into the address */
		if (csr_type > 2) {
			printf("Error: csr_type must be 0, 1, or 2 for CSR V2\n");
			return CMD_RET_FAILURE;
		}
		encoded_addr = (reg & UNIPHY_REG_ADDR_MASK) |
			       ((csr_type << UNIPHY_CSR_BLOCK_SHIFT) &
				UNIPHY_CSR_BLOCK_MASK);
	}

	if (is_write) {
		value = (u32)simple_strtoul(argv[5], NULL, 0);
		printf("UNIPHY CSR Write: uniphy=%d csr_type=%u reg=0x%08x val=0x%08x [%s]\n",
		       uniphy_index, csr_type, reg, value,
		       current_csr_version == CSR_VERSION_V2 ? "V2" : "V1");
		csr_write(uniphy_index, encoded_addr, value);
		printf("Write done\n");
	} else {
		printf("UNIPHY CSR Read: uniphy=%d csr_type=%u reg=0x%08x [%s]\n",
		       uniphy_index, csr_type, reg,
		       current_csr_version == CSR_VERSION_V2 ? "V2" : "V1");
		value = csr_read(uniphy_index, encoded_addr);
		printf("Value: 0x%08x\n", value);
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_CMD(csr, 6, 0, do_uniphy_csr,
	   "UNIPHY CSR register read/write",
	   "read  <uniphy_index> <register> <csr_type>\n"
	   "    Read a UNIPHY CSR register\n"
	   "csr write <uniphy_index> <register> <csr_type> <value>\n"
	   "    Write a UNIPHY CSR register\n"
	   "\n"
	   "  uniphy_index : 0, 1, or 2\n"
	   "  register     : register address in hex\n"
	   "  csr_type     : 0=CSR0(direct)  1=CSR1(indirect)  2=CSR2(indirect)\n"
	   "                 (CSR V1 only supports csr_type 0)\n"
	   "  value        : value to write in hex (write only)\n");
