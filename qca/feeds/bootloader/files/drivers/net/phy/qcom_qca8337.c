/*
 * Copyright (c) 2015-2016, 2020 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <command.h>
#include <miiphy.h>
#include <phy.h>
#include <dm/ofnode.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <malloc.h>
#include <asm/io.h>
#include <asm/types.h>
#include <linux/bitops.h>
#include "qcom_qca8337.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifdef DEBUG
#define pr_dbg(fmt, args...) printf(fmt, ##args)
#else
#define pr_dbg(fmt, args...)
#endif

/* QCA8337 specific constants */
#define QCA8337_PHY_SKIP_STATUS		0x50
#define QCA8337_MDIO_PAGE_ADDR		0x18
#define QCA8337_MDIO_BLOCK_BASE		0x10
#define QCA8337_MDIO_DELAY_US		100
#define QCA8337_RESET_TIMEOUT_MS	100
#define QCA8337_CONFIG_TIMEOUT_MS	100
#define QCA8337_PHY_RESET_DELAY_MS	500
#define QCA8337_MAX_PORTS		7
#define QCA8337_MAX_RETRIES		5

/* Speed encoding values */
#define QCA8337_SPEED_10M		0
#define QCA8337_SPEED_100M		1
#define QCA8337_SPEED_1000M		2

/* Register bit masks */
#define QCA8337_REG_ADDR_MASK		0x1F
#define QCA8337_BLOCK_ADDR_MASK		0x07
#define QCA8337_PAGE_ADDR_MASK		0x1FF
#define QCA8337_SPEED_MASK		0x7
#define QCA8337_DUPLEX_BIT		BIT(6)
#define QCA8337_LINK_STATUS_BIT		BIT(8)

/* Port status structure for tracking link changes */
struct qca8337_port_status {
	u32 port_old_link;
	u32 port_old_speed;
	u32 port_old_duplex;
};

/* Forward declarations */
static int qca8337_reg_read(struct phy_device *phydev, u32 reg_addr, u32 *val);
static int qca8337_reg_write(struct phy_device *phydev, u32 reg_addr, u32 val);

/* Global variables */
static struct qca8337_port_status port_status[QCA8337_MAX_PORTS];
static bool port_status_initialized;

static int qca8337_set_mac_config(struct phy_device *phydev, u32 port_id,
				  u32 speed, u32 duplex)
{
	u32 reg, value;
	int ret;

	if (!phydev) {
		printf("QCA8337: Invalid PHY device\n");
		return -EINVAL;
	}

	if (port_id >= QCA8337_MAX_PORTS) {
		printf("QCA8337: Invalid port ID %u (max %u)\n",
		       port_id, QCA8337_MAX_PORTS - 1);
		return -EINVAL;
	}

	if (speed > QCA8337_SPEED_1000M) {
		printf("QCA8337: Invalid speed %u\n", speed);
		return -EINVAL;
	}

	reg = S17_P0STATUS_REG + (port_id << 2);
	ret = qca8337_reg_read(phydev, reg, &value);
	if (ret < 0) {
		printf("QCA8337: Failed to read port %u status register\n", port_id);
		return ret;
	}

	value &= ~(QCA8337_DUPLEX_BIT | QCA8337_SPEED_MASK);
	value |= speed | (duplex ? QCA8337_DUPLEX_BIT : 0);

	ret = qca8337_reg_write(phydev, reg, value);
	if (ret < 0) {
		printf("QCA8337: Failed to write port %u status register\n", port_id);
		return ret;
	}

	return 0;
}

static inline int qca8337_force_mac_1000M_full(struct phy_device *phydev,
					       u32 port_id)
{
	return qca8337_set_mac_config(phydev, port_id, QCA8337_SPEED_1000M, 1);
}

static inline int qca8337_force_mac_status(struct phy_device *phydev,
					   u32 port_id, u32 speed,
					   u32 duplex)
{
	if (port_id < 1 || port_id > 5) {
		printf("QCA8337: Port %u not configurable (valid range: 1-5)\n", port_id);
		return -EINVAL;
	}
	return qca8337_set_mac_config(phydev, port_id, speed, duplex);
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Get MAC link status
 * @phydev: pointer to the phy_device structure
 * @port_id: port number to check
 * @link: pointer to store link status
 * RETURN   : 0 on success, -1 on failure
 *********************************************************************/
int qca8337_get_mac_link(struct phy_device *phydev, u32 port_id, u32 *link)
{
	u32 reg, value = 0;
	int ret;

	if (!phydev || !link) {
		printf("QCA8337: Invalid parameters\n");
		return -EINVAL;
	}

	if (port_id >= QCA8337_MAX_PORTS) {
		printf("QCA8337: Invalid port ID %u\n", port_id);
		return -EINVAL;
	}

	reg = S17_P0STATUS_REG + (port_id * 4);
	ret = qca8337_reg_read(phydev, reg, &value);
	if (ret < 0) {
		printf("QCA8337: Failed to read port %u status\n", port_id);
		return ret;
	}

	*link = !!(value & QCA8337_LINK_STATUS_BIT);

	return 0;
}

/******************************************************************************
 * FUNCTION DESCRIPTION: Splits a 32-bit register address into its components
 *                       (r1, r2, and page) for QCA8337 PHY access.
 * @regaddr: The 32-bit register address
 * @r1: Pointer to store the register offset within block
 * @r2: Pointer to store the block index
 * @page: Pointer to store the page number
 * RETURN   : NONE
 *****************************************************************************/
static inline void qca8337_split_phy_addr(u32 regaddr, u16 *r1,
					  u16 *r2, u16 *page)
{
	if (!r1 || !r2 || !page) {
		printf("QCA8337: Invalid address split parameters\n");
		return;
	}

	/* Step 1: Convert byte address to word address */
	regaddr >>= 1;

	/* Step 2: Extract bits [4:0] for r1 (register offset within block) */
	*r1 = regaddr & QCA8337_REG_ADDR_MASK;

	/* Step 3: Extract bits [7:5] for r2 (block index) */
	*r2 = (regaddr >> 5) & QCA8337_BLOCK_ADDR_MASK;

	/* Step 4: Extract bits [16:8] for page number */
	*page = (regaddr >> 8) & QCA8337_PAGE_ADDR_MASK;
}

/******************************************************************************
 * FUNCTION DESCRIPTION: Read switch internal register.
 *                       Switch internal register is accessed through the
 *                       MDIO interface. MDIO access is only 16 bits wide so
 *                       it needs the two time access to complete the internal
 *                       register access.
 * @phydev: pointer to the phy_device structure
 * @reg_addr: register address to read
 * @val: pointer to store the read value
 * RETURN   : 0 on success, -1 on failure
 *
 *****************************************************************************/
static int qca8337_reg_read(struct phy_device *phydev, u32 reg_addr, u32 *val)
{
	u16 r1, r2, page;
	int lo, hi;
	int ret = 0;
	int original_addr;

	if (!phydev || !val) {
		printf("QCA8337: Invalid parameters for register read\n");
		return -EINVAL;
	}

	original_addr = phydev->addr;
	*val = 0; /* Initialize output value */

	qca8337_split_phy_addr(reg_addr, &r1, &r2, &page);

	/* Configure register high address */
	phydev->addr = QCA8337_MDIO_PAGE_ADDR;
	ret = phy_write(phydev, MDIO_DEVAD_NONE, 0x0, page);
	if (ret < 0) {
		printf("QCA8337: Failed to set page address 0x%x\n", page);
		goto restore_addr;
	}
	udelay(QCA8337_MDIO_DELAY_US);

	/* For some registers such as MIBs, since it is read/clear, we
	 * should read the lower 16-bit register then the higher one
	 */
	phydev->addr = (QCA8337_MDIO_BLOCK_BASE | r2);

	/* Read register in lower address */
	lo = phy_read(phydev, MDIO_DEVAD_NONE, r1);
	if (lo < 0) {
		printf("QCA8337: Failed to read lower register 0x%x\n", r1);
		ret = lo;
		goto restore_addr;
	}

	/* Read register in higher address */
	hi = phy_read(phydev, MDIO_DEVAD_NONE, r1 + 1);
	if (hi < 0) {
		printf("QCA8337: Failed to read upper register 0x%x\n", r1 + 1);
		ret = hi;
		goto restore_addr;
	}

	*val = ((u32)hi << 16) | (u32)lo;

restore_addr:
	phydev->addr = original_addr;
	return ret;
}

/******************************************************************************
 * FUNCTION DESCRIPTION: Write switch internal register.
 *                       Switch internal register is accessed through the
 *                       MDIO interface. MDIO access is only 16 bits wide so
 *                       it needs the two time access to complete the internal
 *                       register access.
 * @phydev: pointer to the phy_device structure
 * @reg_addr: register address to write
 * @val: value to be written to the register
 * RETURN   : 0 on success, -1 on failure
 *
 *****************************************************************************/
static int qca8337_reg_write(struct phy_device *phydev, u32 reg_addr, u32 val)
{
	u16 r1, r2, page;
	u16 lo, hi;
	int ret = 0;
	int original_addr;

	if (!phydev) {
		printf("QCA8337: Invalid PHY device for register write\n");
		return -EINVAL;
	}

	original_addr = phydev->addr;

	qca8337_split_phy_addr(reg_addr, &r1, &r2, &page);

	lo = val & 0xffff;
	hi = (u16)(val >> 16);

	/* Configure register high address */
	phydev->addr = QCA8337_MDIO_PAGE_ADDR;
	ret = phy_write(phydev, MDIO_DEVAD_NONE, 0x0, page);
	if (ret < 0) {
		printf("QCA8337: Failed to set page address 0x%x\n", page);
		goto restore_addr;
	}
	udelay(QCA8337_MDIO_DELAY_US);

	/* For some registers such as ARL and VLAN, since they include BUSY
	 * bit in lower address, we should write the higher 16-bit register
	 * then the lower one
	 */
	phydev->addr = (QCA8337_MDIO_BLOCK_BASE | r2);

	/* Write register in higher address */
	ret = phy_write(phydev, MDIO_DEVAD_NONE, r1 + 1, hi);
	if (ret < 0) {
		printf("QCA8337: Failed to write upper register 0x%x\n", r1 + 1);
		goto restore_addr;
	}

	/* Write register in lower address */
	ret = phy_write(phydev, MDIO_DEVAD_NONE, r1, lo);
	if (ret < 0) {
		printf("QCA8337: Failed to write lower register 0x%x\n", r1);
		goto restore_addr;
	}

restore_addr:
	phydev->addr = original_addr;
	return ret;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Helper function to write a pair of VLAN registers.
 * @phydev: pointer to the phy_device structure
 * @reg1: first register address
 * @val1: value for the first register
 * @reg2: second register address
 * @val2: value for the second register
 * RETURN   : 0 on success, -1 on failure
 *********************************************************************/
static int qca8337_vlan_write_pair(struct phy_device *phydev, u32 reg1,
				   u32 val1, u32 reg2, u32 val2)
{
	int ret;

	ret = qca8337_reg_write(phydev, reg1, val1);
	if (ret < 0)
		return ret;
	ret = qca8337_reg_write(phydev, reg2, val2);
	if (ret < 0)
		return ret;

	return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: V-lan configuration for QCA8337 switch.
 *                       Vlan 1:PHY0,1,2,3 and Mac 6 of s17c
 *                       Vlan 2:PHY4 and Mac 0 of s17c
 * @phydev: pointer to the phy_device structure
 * RETURN   : 0 on success, -1 on failure
 *********************************************************************/
int qca8337_vlan_config(struct phy_device *phydev)
{
	int ret, i;
	struct {
		u32 lookup_reg;
		u32 lookup_val;
		u32 vlan_ctrl_reg;
		u32 vlan_ctrl_val;
	} vlan_configs[] = {
		{ S17_P0LOOKUP_CTRL_REG, 0x00140020,
			S17_P0VLAN_CTRL0_REG, 0x20001 },
		{ S17_P1LOOKUP_CTRL_REG, 0x0014005c,
			S17_P1VLAN_CTRL0_REG, 0x10001 },
		{ S17_P2LOOKUP_CTRL_REG, 0x0014005a,
			S17_P2VLAN_CTRL0_REG, 0x10001 },
		{ S17_P3LOOKUP_CTRL_REG, 0x00140056,
			S17_P3VLAN_CTRL0_REG, 0x10001 },
		{ S17_P4LOOKUP_CTRL_REG, 0x0014004e,
			S17_P4VLAN_CTRL0_REG, 0x10001 },
		{ S17_P5LOOKUP_CTRL_REG, 0x00140001,
			S17_P5VLAN_CTRL0_REG, 0x20001 },
		{ S17_P6LOOKUP_CTRL_REG, 0x0014001e,
			S17_P6VLAN_CTRL0_REG, 0x10001 },
	};

	if (!phydev) {
		printf("QCA8337: Invalid PHY device for VLAN config\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(vlan_configs); i++) {
		ret = qca8337_vlan_write_pair(phydev, vlan_configs[i].lookup_reg,
					      vlan_configs[i].lookup_val,
					      vlan_configs[i].vlan_ctrl_reg,
					      vlan_configs[i].vlan_ctrl_val);
		if (ret < 0) {
			printf("QCA8337: VLAN config failed for port %d (ret=%d)\n", i, ret);
			return ret;
		}
	}

	pr_debug("QCA8337: VLAN configuration completed successfully\n");

	return 0;
}

/*******************************************************************
 * FUNCTION DESCRIPTION: Reset S17 register
 * @phydev: pointer to the phy_device structure
 * RETURN   : 0 on success, -1 on failure
 *******************************************************************/
int qca8337_sw_reset(struct phy_device *phydev)
{
	u32 data;
	u32 i = 0;
	int ret;
	u32 reg_val;
	const u32 max_reset_attempts = 100; /* Increased from 10 */

	if (!phydev) {
		printf("QCA8337: Invalid PHY device for reset\n");
		return -EINVAL;
	}

	pr_debug("QCA8337: Initiating switch reset\n");

	/* Reset the switch before initialization */
	ret = qca8337_reg_write(phydev, S17_MASK_CTRL_REG,
				S17_MASK_CTRL_SOFT_RET);
	if (ret < 0) {
		printf("QCA8337: Failed to write reset register\n");
		return ret;
	}

	do {
		udelay(1000); /* Increased delay from 10us to 1ms */
		ret = qca8337_reg_read(phydev, S17_MASK_CTRL_REG, &reg_val);
		if (ret < 0) {
			printf("QCA8337: Failed to read reset status\n");
			return ret;
		}
		data = reg_val;
		i++;
		if (i >= max_reset_attempts) {
			printf("QCA8337: Reset timeout after %u attempts\n", i);
			return -ETIMEDOUT;
		}
	} while (data & S17_MASK_CTRL_SOFT_RET);

	pr_debug("QCA8337: Switch reset completed after %u attempts\n", i);

	return 0;
}

int qca8337_hw_config(struct phy_device *phydev)
{
	u32 data;
	u32 i = 0;
	int ret;
	u32 reg_val;
	const u32 max_config_attempts = 100; /* Increased from 10 */

	if (!phydev) {
		printf("QCA8337: Invalid PHY device for HW config\n");
		return -EINVAL;
	}

	pr_debug("QCA8337: Waiting for hardware initialization\n");

	do {
		udelay(1000); /* Increased delay from 10us to 1ms */
		ret = qca8337_reg_read(phydev, S17_GLOBAL_INT0_REG, &reg_val);
		if (ret < 0) {
			printf("QCA8337: Failed to read global interrupt register\n");
			return ret;
		}
		data = reg_val;
		i++;
		if (i >= max_config_attempts) {
			printf("QCA8337: HW config timeout after %u attempts\n", i);
			return -ETIMEDOUT;
		}
	} while ((data & S17_GLOBAL_INITIALIZED_STATUS) !=
		 S17_GLOBAL_INITIALIZED_STATUS);

	pr_debug("QCA8337: Hardware initialization completed after %u attempts\n", i);

	return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Configure S17 register
 * @phydev: pointer to the phy_device structure
 * @swt_cfg: pointer to the switch configuration structure
 * RETURN   : 0 on success, error code on failure
 *********************************************************************/
int qca8337_reg_init(struct phy_device *phydev, struct switchconfig *swt_cfg)
{
	int ret;

	if (!phydev || !swt_cfg) {
		printf("QCA8337: Invalid parameters for register init\n");
		return -EINVAL;
	}

	pr_debug("QCA8337: Initializing switch registers\n");

	ret = qca8337_reg_write(phydev, S17_MAC_PWR_REG, swt_cfg->mac_pwr);
	if (ret < 0) {
		printf("QCA8337: Failed to write MAC power register\n");
		return ret;
	}

	ret = qca8337_reg_write(phydev, S17_GLOFW_CTRL1_REG,
				(S17_IGMP_JOIN_LEAVE_DPALL |
				 S17_BROAD_DPALL |
				 S17_MULTI_FLOOD_DPALL |
				 S17_UNI_FLOOD_DPALL));
	if (ret < 0) {
		printf("QCA8337: Failed to write global forwarding control register\n");
		return ret;
	}

	if (swt_cfg->update) {
		ret = qca8337_reg_write(phydev, S17_P0STATUS_REG,
					swt_cfg->port0_status);
		if (ret < 0) {
			printf("QCA8337: Failed to write port 0 status register\n");
			return ret;
		}
		ret = qca8337_reg_write(phydev, S17_P5PAD_MODE_REG,
					swt_cfg->pad5_mode);
		if (ret < 0) {
			printf("QCA8337: Failed to write port 5 pad mode register\n");
			return ret;
		}
		ret = qca8337_reg_write(phydev, S17_P0PAD_MODE_REG,
					swt_cfg->pad0_mode);
		if (ret < 0) {
			printf("QCA8337: Failed to write port 0 pad mode register\n");
			return ret;
		}
	} else {
		ret = qca8337_reg_write(phydev, S17_P0STATUS_REG,
					(S17_SPEED_1000M | S17_TXMAC_EN |
					 S17_RXMAC_EN | S17_DUPLEX_FULL));
		if (ret < 0) {
			printf("QCA8337: Failed to write default port 0 status\n");
			return ret;
		}

		ret = qca8337_reg_write(phydev, S17_P5PAD_MODE_REG,
					S17_MAC0_RGMII_RXCLK_DELAY);
		if (ret < 0) {
			printf("QCA8337: Failed to write default port 5 pad mode\n");
			return ret;
		}

		ret = qca8337_reg_write(phydev, S17_P0PAD_MODE_REG,
					(S17_MAC0_RGMII_EN |
					 S17_MAC0_RGMII_TXCLK_DELAY |
					 S17_MAC0_RGMII_RXCLK_DELAY |
					 (0x1 << S17_MAC0_RGMII_TXCLK_SHIFT) |
					 (0x2 << S17_MAC0_RGMII_RXCLK_SHIFT)));
		if (ret < 0) {
			printf("QCA8337: Failed to write default port 0 pad mode\n");
			return ret;
		}
	}

	pr_debug("QCA8337: Register initialization completed\n");

	return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Configure S17 register for LAN
 * @phydev: pointer to the phy_device structure
 * @swt_cfg: pointer to the switch configuration structure
 * RETURN   : 0 on success, error code on failure
 *********************************************************************/
int qca8337_reg_init_lan(struct phy_device *phydev,
			 struct switchconfig *swt_cfg)
{
	u32 reg_val;
	int ret;

	if (!phydev || !swt_cfg) {
		printf("QCA8337: Invalid parameters for LAN register init\n");
		return -EINVAL;
	}

	pr_debug("QCA8337: Initializing LAN registers\n");

	ret = qca8337_reg_write(phydev, S17_MAC_PWR_REG, swt_cfg->mac_pwr);
	if (ret < 0) {
		printf("QCA8337: Failed to write MAC power register for LAN\n");
		return ret;
	}

	if (swt_cfg->update) {
		ret = qca8337_reg_write(phydev, S17_P6STATUS_REG,
					swt_cfg->port6_status);
		if (ret < 0) {
			printf("QCA8337: Failed to write port 6 status register\n");
			return ret;
		}
		ret = qca8337_reg_write(phydev, S17_P6PAD_MODE_REG,
					swt_cfg->pad6_mode);
		if (ret < 0) {
			printf("QCA8337: Failed to write port 6 pad mode register\n");
			return ret;
		}
		ret = qca8337_reg_write(phydev, S17_PWS_REG,
					swt_cfg->port0);
		if (ret < 0) {
			printf("QCA8337: Failed to write PWS register\n");
			return ret;
		}
		ret = qca8337_reg_write(phydev, S17_SGMII_CTRL_REG,
					swt_cfg->sgmii_ctrl);
		if (ret < 0) {
			printf("QCA8337: Failed to write SGMII control register\n");
			return ret;
		}
	} else {
		ret = qca8337_reg_write(phydev, S17_P6STATUS_REG,
					(S17_SPEED_1000M | S17_TXMAC_EN |
					 S17_RXMAC_EN | S17_DUPLEX_FULL));
		if (ret < 0) {
			printf("QCA8337: Failed to write default port 6 status\n");
			return ret;
		}

		ret = qca8337_reg_read(phydev, S17_P6PAD_MODE_REG, &reg_val);
		if (ret < 0) {
			printf("QCA8337: Failed to read port 6 pad mode register\n");
			return ret;
		}
		ret = qca8337_reg_write(phydev, S17_P6PAD_MODE_REG,
					(reg_val | S17_MAC6_SGMII_EN));
		if (ret < 0) {
			printf("QCA8337: Failed to write port 6 pad mode register\n");
			return ret;
		}

		ret = qca8337_reg_write(phydev, S17_PWS_REG, 0x2613a0);
		if (ret < 0) {
			printf("QCA8337: Failed to write default PWS register\n");
			return ret;
		}

		ret = qca8337_reg_write(phydev, S17_SGMII_CTRL_REG,
					(S17c_SGMII_EN_PLL | S17c_SGMII_EN_RX |
						S17c_SGMII_EN_TX |
						S17c_SGMII_EN_SD |
						S17c_SGMII_BW_HIGH |
						S17c_SGMII_SEL_CLK125M |
						S17c_SGMII_TXDR_CTRL_600MV |
						S17c_SGMII_CDR_BW_8 |
						S17c_SGMII_DIS_AUTO_LPI_25M |
						S17c_SGMII_MODE_CTRL_SGMII_PHY |
						S17c_SGMII_PAUSE_SG_TX_EN_25M |
						S17c_SGMII_ASYM_PAUSE_25M |
						S17c_SGMII_PAUSE_25M |
						S17c_SGMII_HALF_DUPLEX_25M |
						S17c_SGMII_FULL_DUPLEX_25M));
		if (ret < 0) {
			printf("QCA8337: Failed to write default SGMII control register\n");
			return ret;
		}
	}

	ret = qca8337_reg_write(phydev, S17_MODULE_EN_REG,
				S17_MIB_COUNTER_ENABLE);
	if (ret < 0) {
		printf("QCA8337: Failed to enable MIB counters\n");
		return ret;
	}

	pr_debug("QCA8337: LAN register initialization completed\n");

	return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Probe function for QCA8337 PHY driver.
 * @phydev: pointer to the phy_device structure
 * RETURN   : 0 on success, error code otherwise
 *********************************************************************/
static int qca8337_parse_config(struct phy_device *phydev)
{
	struct switchconfig *swt_cfg;
	int ret = 0;

	swt_cfg = malloc(sizeof(*swt_cfg));
	if (!swt_cfg)
		return -ENOMEM;

	memset(swt_cfg, 0, sizeof(*swt_cfg));
	phydev->priv = swt_cfg;

	swt_cfg->mac_pwr = ofnode_read_u32_default(phydev->node, "mac_pwr",
						   0xaa545);
	swt_cfg->update = ofnode_read_bool(phydev->node, "update");
	swt_cfg->vlan = ofnode_read_bool(phydev->node, "vlan");

	/* Initialize port_count regardless of update flag to ensure loops work correctly */
	swt_cfg->port_count = ofnode_read_u32_default(phydev->node,
						      "port_count", 4);

	if (swt_cfg->update) {
		swt_cfg->pad0_mode = ofnode_read_u32_default(phydev->node,
							     "pad0_mode", 0x80);
		swt_cfg->pad5_mode = ofnode_read_u32_default(phydev->node,
							     "pad5_mode", 0);
		swt_cfg->pad6_mode = ofnode_read_u32_default(phydev->node,
							     "pad6_mode", 0);
		swt_cfg->port0 = ofnode_read_u32_default(phydev->node, "port0",
							 0x2613a0);
		swt_cfg->sgmii_ctrl = ofnode_read_u32_default(phydev->node,
							      "sgmii_ctrl",
								0xc74164de);
		swt_cfg->port0_status = ofnode_read_u32_default(phydev->node,
								"port0_status",
								0x4e);
		swt_cfg->port6_status = ofnode_read_u32_default(phydev->node,
								"port6_status",
								0);
		ret = ofnode_read_u32_array(phydev->node, "port_phy_address",
					    swt_cfg->port_phy_address,
					    swt_cfg->port_count);
		if (ret) {
			printf("QCA8337: Failed to read port_phy_address from device tree\n");
			goto err_free;
		}

		ret = ofnode_read_u32_array(phydev->node, "port_nos",
					    swt_cfg->ports,
					    swt_cfg->port_count);
		if (ret) {
			printf("QCA8337: Failed to read port_nos from device tree\n");
			goto err_free;
		}
	} else {
		/* Set default PHY addresses when update is false */
		int i;

		for (i = 0; i < swt_cfg->port_count && i < 4; i++)
			swt_cfg->port_phy_address[i] = i;
	}

	return 0;

err_free:
	free(swt_cfg);
	phydev->priv = NULL;

	return ret;
}

int qca8337_switch_init(struct phy_device *phydev,
			struct switchconfig *swt_cfg)
{
	int ret = 0;

	if (!phydev) {
		printf("QCA8337: Invalid PHY device for switch init\n");
		return -EINVAL;
	}

	if (!swt_cfg) {
		printf("QCA8337: Invalid switch configuration\n");
		return -EINVAL;
	}

	pr_debug("QCA8337: Starting switch initialization\n");

	ret = qca8337_reg_init(phydev, swt_cfg);
	if (ret < 0) {
		printf("QCA8337: Register initialization failed\n");
		return ret;
	}
	mdelay(100);

	ret = qca8337_reg_init_lan(phydev, swt_cfg);
	if (ret < 0) {
		printf("QCA8337: LAN register initialization failed\n");
		return ret;
	}
	mdelay(100);

	if (swt_cfg->vlan) {
		ret = qca8337_vlan_config(phydev);
		if (ret < 0) {
			printf("QCA8337: VLAN configuration failed\n");
			return ret;
		}
	} else {
		pr_debug("QCA8337: Skipping VLAN configuration\n");
	}
	mdelay(100);

	pr_debug("QCA8337: Switch initialization completed successfully\n");

	return ret;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Configuration function for QCA8337 PHY driver.
 * @phydev: pointer to the phy_device structure
 * RETURN   : 0 on success, error code otherwise
 *********************************************************************/
static int qca8337_config(struct phy_device *phydev)
{
	struct switchconfig *swt_cfg;
	int port;
	u32 phy_val;
	int original_phydev_addr = phydev->addr; /* Store original address */
	int ret;

	ret = qca8337_parse_config(phydev);
	if (ret)
		return ret;

	swt_cfg = phydev->priv;

	for (port = 0; port < swt_cfg->port_count; ++port) {
		phydev->addr = swt_cfg->port_phy_address[port];
		phy_val = phy_read(phydev, MDIO_DEVAD_NONE, 0x3d);
		phy_val &= ~0x0040;
		mdelay(100);
		phy_write(phydev, MDIO_DEVAD_NONE, 0x3d, phy_val);
		mdelay(100);
		/*
		 * PHY will stop the tx clock for a while when link is down
		 * en_anychange  debug port 0xb bit13 = 0  //speed up
		 * link down tx_clk
		 * sel_rst_80us  debug port 0xb bit10 = 0  //speed up
		 * speed mode change to 2'b10 tx_clk
		 */
		phy_val = phy_read(phydev, MDIO_DEVAD_NONE, 0x0b);
		mdelay(100);
		phy_val &= ~0x2400;
		phy_write(phydev, MDIO_DEVAD_NONE, 0x0b, phy_val);
		mdelay(10);
	}

	phydev->addr = original_phydev_addr;

	ret = qca8337_sw_reset(phydev);
	if (ret < 0) {
		phydev->addr = original_phydev_addr;
		return ret;
	}

	mdelay(10);

	ret = qca8337_hw_config(phydev);
	if (ret < 0) {
		phydev->addr = original_phydev_addr;
		return ret;
	}

	mdelay(10);

	ret = qca8337_switch_init(phydev, swt_cfg);
	if (ret != 0) {
		printf("QCA_8337 switch init failed\n");
		phydev->addr = original_phydev_addr;
		return ret;
	}

	mdelay(500);

	qca8337_reg_read(phydev, 0x30, &phy_val);
	mdelay(100);
	phy_val &= ~BIT(8);
	phy_val &= ~BIT(9);
	qca8337_reg_write(phydev, 0x30, phy_val);
	mdelay(100);

	for (port = 0; port < swt_cfg->port_count; ++port) {
		phydev->addr = swt_cfg->port_phy_address[port];
		phy_write(phydev, MDIO_DEVAD_NONE, MII_ADVERTISE,
			  ADVERTISE_ALL | ADVERTISE_PAUSE_CAP |
			  ADVERTISE_PAUSE_ASYM);
		mdelay(100);
		/* phy reg 0x9, b10,1 = Prefer multi-port device (master) */
		phy_write(phydev, MDIO_DEVAD_NONE, MII_CTRL1000,
			  (0x0400 | ADVERTISE_1000FULL));
		mdelay(100);
	}

	for (port = 0; port < swt_cfg->port_count; ++port) {
		phydev->addr = swt_cfg->port_phy_address[port];
		phy_val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
		mdelay(100);
		phy_val |= BMCR_RESET | BMCR_ANENABLE | BMCR_ANRESTART;
		phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, phy_val);
		do {
			mdelay(100);
			phy_val = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
		} while (phy_val & BMCR_RESET);
	}

	phydev->addr = original_phydev_addr; /* Restore original address */

	return 0;
}

/*********************************************************************
 * FUNCTION DESCRIPTION: Startup function for QCA8337 PHY driver with
 *                       integrated two-stage MAC polling logic and
 *                       speed change detection with PHY reset.
 * @phydev: pointer to the phy_device structure
 * RETURN   : 0 on success, error code otherwise
 *********************************************************************/
static int qca8337_startup(struct phy_device *phydev)
{
	struct switchconfig *swt_cfg = phydev->priv;
	u32 phy_data;
	int i;
	int original_addr = phydev->addr;
	const int max_retries = 5;
	int ret;

	if (!swt_cfg)
		return -EINVAL;

	if (!port_status_initialized) {
		memset(port_status, 0, sizeof(port_status));
		port_status_initialized = true;
	}

	printf("QCA8337-switch status \n");

	/* Check and print port status */
	phydev->link = 0;
	for (i = 0; i < swt_cfg->port_count; ++i) {
		u32 port_id = swt_cfg->ports[i];
		u32 phy_addr = swt_cfg->port_phy_address[i];
		u32 speed, link, duplex;
		u32 old_speed = 0;
		u32 old_link = 0;
		int need_phy_reset = 0;

		phydev->addr = phy_addr;
		ret = phy_read(phydev, MDIO_DEVAD_NONE, S17_PHY_SPEC_STATUS);
		if (ret < 0) {
			printf("QCA8337: Failed to read PHY status for port %d (addr 0x%x)\n",
			       port_id, phy_addr);
			continue;
		}
		phy_data = (u32)ret;

		if (phy_data == QCA8337_PHY_SKIP_STATUS) {
			pr_dbg("QCA8337: Port%d PHY data is 0x%x, skipping status display.\n",
			       port_id, QCA8337_PHY_SKIP_STATUS);
			continue;
		}

		/* Extract speed, link, duplex from phy_data */
		link = !!(phy_data & LINK_UP);
		speed = (SPEED(phy_data) == SPEED_1000M) ? 2 :
			(SPEED(phy_data) == SPEED_100M) ? 1 : 0;
		duplex = !!(phy_data & S17_STATUS_FULL_DEPLEX);

		/* Get old speed and link status for comparison */
		if (port_id < 7) {
			old_speed = port_status[port_id].port_old_speed;
			old_link = port_status[port_id].port_old_link;
		}

		/* Check for speed change from 10M to 1000M while link is up */
		if (link && old_link && old_speed == 0 && speed == 2) {
			pr_debug("QCA8337: Port %d speed change detected (10M -> 1000M), performing PHY reset\n",
			       port_id);
			need_phy_reset = 1;
		}

		/* Perform PHY reset if needed */
		if (need_phy_reset) {
			u32 phy_val;
			int retry_count = 0;

			ret = qca8337_force_mac_1000M_full(phydev, port_id);
			if (ret < 0) {
				printf("QCA8337: Failed to force MAC 1000M for port %d\n", port_id);
				continue;
			}

			/* Reset the PHY */
			ret = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
			if (ret < 0) {
				printf("QCA8337: Failed to read BMCR for port %d reset\n", port_id);
				continue;
			}
			phy_val = (u32)ret;
			mdelay(100);
			phy_val |= BMCR_RESET;
			ret = phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, phy_val);
			if (ret < 0) {
				printf("QCA8337: Failed to write BMCR for port %d reset\n", port_id);
				continue;
			}

			pr_debug("QCA8337: Port %d PHY reset initiated\n", port_id);

			/* Wait for reset to complete */
			do {
				mdelay(500);
				ret = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
				if (ret < 0) {
					printf("QCA8337: Failed to read BMCR during reset wait for port %d\n", port_id);
					break;
				}
				phy_val = (u32)ret;
			} while (phy_val & BMCR_RESET);

			if (ret < 0) {
				printf("QCA8337: PHY reset failed for port %d, continuing with other ports\n", port_id);
				continue;
			}

			pr_debug("QCA8337: Port %d PHY reset completed\n", port_id);

			/* Re-read PHY status after reset with retries */
			while (retry_count < max_retries) {
				mdelay(500); /* Allow time for link to stabilize */
				ret = phy_read(phydev, MDIO_DEVAD_NONE, S17_PHY_SPEC_STATUS);
				if (ret < 0) {
					printf("QCA8337: Failed to read PHY status after reset for port %d (retry %d)\n",
					       port_id, retry_count);
					retry_count++;
					continue;
				}
				phy_data = (u32)ret;
				link = !!(phy_data & LINK_UP);
				if (link) {
					break;
				}

				retry_count++;
			}

			if (ret < 0) {
				printf("QCA8337: Failed to read PHY status after reset for port %d, skipping\n", port_id);
				continue;
			}

			speed = (SPEED(phy_data) == SPEED_1000M) ? 2 :
				(SPEED(phy_data) == SPEED_100M) ? 1 : 0;
			duplex = !!(phy_data & S17_STATUS_FULL_DEPLEX);

			pr_debug("QCA8337: Port %d status after reset - Link: %d, Speed: %s\n",
			       port_id, link,
			       (speed == 2) ? "1000M" : (speed == 1) ? "100M" : "10M");
		}

		/* Save PHY status to port_status for tracking */
		if (port_id < 7) {
			port_status[port_id].port_old_link = link;
			port_status[port_id].port_old_speed = speed;
			port_status[port_id].port_old_duplex = duplex;
		}

		/* Print port status */
		printf("Port%d %s ", port_id, LINK(phy_data));

		switch (SPEED(phy_data)) {
		case SPEED_1000M:
			printf("Speed :1000 ");
			break;
		case SPEED_100M:
			printf("Speed :100 ");
			break;
		default:
			printf("Speed :10 ");
		}

		printf("%s", DUPLEX(phy_data));
		printf("\n");

		/* At least one port should be link up */
		if (phy_data & LINK_UP)
			phydev->link = 1;

		/* Normalize duplex to 0 for half, 1 for full */
		if (!phydev->duplex)
			phydev->duplex = !!(phy_data & S17_STATUS_FULL_DEPLEX);
	}
	/* Restore original PHY address */
	phydev->addr = original_addr;

	/* Switch mode: always set to 1000M regardless of individual port speeds */
	phydev->speed = SPEED_1000;

	return 0;
}



U_BOOT_PHY_DRIVER(qcom_qca8337) = {
	.name     = "Qualcomm QCA8337",
	.uid      = 0x004DD030,    /* UID with rev bits cleared */
	.mask     = 0xfffffff0,    /* ignore 4-bit revision field */
	.features = PHY_GBIT_FEATURES,
	.config   = &qca8337_config,
	.startup  = &qca8337_startup,
	.shutdown = &genphy_shutdown,
};

struct athrs17_regmap {
	u32 start;
	u32 end;
};

struct athrs17_regmap regmap[] = {
	{ 0x000,  0x0e4  },  /* Global registers */
	{ 0x100,  0x168  },  /* VLAN registers */
	{ 0x200,  0x270  },  /* ARL registers */
	{ 0x400,  0x454  },  /* Port status registers */
	{ 0x600,  0x718  },  /* MIB counter registers */
	{ 0x800,  0xb70  },  /* Look-up table registers */
	{ 0xC00,  0xC80  },  /* ACL registers */
	{ 0x1000, 0x10a7 }, /* Port 1 PHY specific registers */
	{ 0x1100, 0x11a7 }, /* Port 2 PHY specific registers */
	{ 0x1200, 0x12a7 }, /* Port 3 PHY specific registers */
	{ 0x1300, 0x13a7 }, /* Port 4 PHY specific registers */
	{ 0x1400, 0x14a7 }, /* Port 5 PHY specific registers */
	{ 0x1600, 0x16a7 }, /* Port 6 PHY specific registers */
};

/*********************************************************************
 * FUNCTION DESCRIPTION: Find QCA8337 PHY device on specified MDIO bus
 * @bus_name: MDIO bus name
 * RETURN   : pointer to phy_device if found, NULL otherwise
 *********************************************************************/
static struct phy_device *qca8337_find_phydev_on_bus(const char *bus_name)
{
	struct phy_device *phydev;
	struct mii_dev *bus;
	int i;

	/* Find the specified MDIO bus */
	bus = miiphy_get_dev_by_name(bus_name);
	if (!bus) {
		printf("MDIO bus '%s' not found\n", bus_name);
		return NULL;
	}

	/* Search for QCA8337 PHY on this bus */
	for (i = 0; i < PHY_MAX_ADDR; i++) {
		phydev = bus->phymap[i];
		if (phydev && phydev->drv &&
		    (phydev->drv->uid == 0x004DD030)) {
			return phydev;
		}
	}

	return NULL;
}

static int do_qca8337(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	int i;
	struct phy_device *phydev;
	u32 reg, val;
	char *bus_name;

	if (argc < 3)
		return CMD_RET_USAGE;

	/* First argument is the MDIO bus name */
	bus_name = argv[1];

	/* Find QCA8337 PHY device on specified bus */
	phydev = qca8337_find_phydev_on_bus(bus_name);
	if (!phydev) {
		printf("No QCA8337 PHY device found on bus '%s'\n", bus_name);
		return -1;
	}

	printf("Using QCA8337 PHY on bus %s\n", phydev->bus->name);

	switch (argc) {
	case 3:
		if (argv[2][1] == 'd') {
			for (i = 0; i < ARRAY_SIZE(regmap); i++) {
				for (reg = regmap[i].start;
					reg <= regmap[i].end; reg += 4) {
					int ret_read = qca8337_reg_read(phydev,
									reg,
									&val);

					if (ret_read < 0) {
						printf("Error reading 0x%04x: %d\n",
						       reg, ret_read);
						return -1;
					}
					printf("0x%04x: 0x%08x\n", reg, val);
				}
			}
		} else {
			return CMD_RET_USAGE;
		}
		break;
	case 4:
		if (argv[2][1] == 'r') {
			reg = simple_strtoul(argv[3], NULL, 16);
			printf("0x%04x: ", reg);
			int ret_read = qca8337_reg_read(phydev, reg, &val);

			if (ret_read < 0) {
				printf("Error reading 0x%04x: %d\n",
				       reg, ret_read);
				return -1;
			}
			printf("0x%08x\n", val);
		} else {
			return CMD_RET_USAGE;
		}
		break;
	case 5:
		if (argv[2][1] == 'w') {
			reg = simple_strtoul(argv[3], NULL, 16);
			val = simple_strtoul(argv[4], NULL, 16);
			int ret_write = qca8337_reg_write(phydev, reg, val);

			if (ret_write < 0) {
				printf("Error writing 0x%04x: %d\n",
				       reg, ret_write);
				return -1;
			}
		} else {
			return CMD_RET_USAGE;
		}
		break;
	default:
		return CMD_RET_USAGE;
	}

	return 0;
};

U_BOOT_CMD(qca8337, 5, 0, do_qca8337, "qca8337 - dump qca8337 registers",
	   "<bus_name> -d Dump Registers\n"
	   "<bus_name> -r <addr> read the address\n"
	   "<bus_name> -w <addr> <val> write the value to register");
