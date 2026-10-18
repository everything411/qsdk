// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2014, 2015-2017, 2020 The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2023, 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <common.h>
#include <dm.h>
#include <pci.h>
#include <generic-phy.h>
#include <regmap.h>
#include <asm/io.h>
#include <asm-generic/gpio.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <clk.h>
#include <reset.h>
#include <linux/iopoll.h>

#include "pcie_dw_common.h"

#define TARGET_LINK_SPEED_MASK			0xf
#define LINK_SPEED_GEN_1			0x1
#define LINK_SPEED_GEN_2			0x2
#define LINK_SPEED_GEN_3			0x3

#define PCIE_MISC_CONTROL_1_OFF			0x8bc
#define PCIE_DBI_RO_WR_EN			BIT(0)

#define PCIE_PARF_DEVICE_TYPE			0x1000
#define DEVICE_TYPE_RC				0x4

#define PARF_MHI_CLOCK_RESET_CTRL		0x174
#define BYPASS					BIT(4)
#define MSTR_AXI_CLK_EN				BIT(1)
#define AHB_CLK_EN				BIT(0)

#define PCIE30_GEN3_RELATED_OFF			0x890
#define GEN3_EQUALIZATION_DISABLE		BIT(16)
#define RXEQ_RGRDLESS_RXTS			BIT(13)
#define GEN3_ZRXDC_NONCOMPL			BIT(0)

#define PCIE20_PARF_SYS_CTRL			0x00
#define ECAM_BLOCKER_EN_RANGE2			BIT(30)
#define MAC_PHY_POWERDOWN_IN_P2_D_MUX_EN	BIT(29)
#define ECAM_REMOVE_OFFSET_EN			BIT(27)
#define ECAM_BLOCKER_EN				BIT(26)
#define MST_WAKEUP_EN				BIT(13)
#define SLV_WAKEUP_EN				BIT(12)
#define MSTR_ACLK_CGC_DIS			BIT(10)
#define SLV_ACLK_CGC_DIS			BIT(9)
#define CORE_CLK_CGC_DIS			BIT(6)
#define AUX_PWR_DET				BIT(4)
#define CORE_CLK_2AUX_CLK_MUX_DIS		BIT(3)
#define L23_CLK_RMV_DIS				BIT(2)
#define L1_CLK_RMV_DIS				BIT(1)

#define PCIE_LINK_CAPABILITY			0x7c
#define PCIE_CAP_ASPM_OPT_COMPLIANCE		BIT(22)
#define PCIE_CAP_LINK_BW_NOT_CAP		BIT(21)
#define PCIE_CAP_DLL_ACTIVE_REP_CAP		BIT(20)
#define PCIE_CAP_L1_EXIT_LATENCY(x)		(x << 15)
#define PCIE_CAP_L0S_EXIT_LATENCY(x)		(x << 12)
#define PCIE_CAP_MAX_LINK_WIDTH(x)		(x << 4)
#define PCIE_CAP_MAX_LINK_SPEED(x)		(x << 0)

#define PCIE_LINK_CTL_2				0xa0
#define PCIE_CAP_CURR_DEEMPHASIS		BIT(16)

#define PCIE_LINK_CONTROL_LINK_STATUS_REG	0x80
#define PCIE_CAP_DLL_ACTIVE			BIT(29)

#define PCIE_DEVICE_CONTROL2_DEVICE_STATUS2_REG	0x098
#define PCIE_CAP_CPL_TIMEOUT_DISABLE		BIT(4)

#define PCIE_PCIE20_PARF_LTSSM			0x1B0
#define LTSSM_EN				BIT(8)

#define PCIE_PORT_FORCE_REG			0x708

#define PCIE_ACK_F_ASPM_CTRL_REG		0x70C
#define L1_ENTRANCE_LATENCY(x)                  (x << 27)
#define L0_ENTRANCE_LATENCY(x)			(x << 24)
#define COMMON_CLK_N_FTS(x)			(x << 16)
#define ACK_N_FTS(x)				(x << 8)

#define PCIE_GEN2_CTRL_REG			0x80C
#define FAST_TRAINING_SEQ(x)			(x << 0)
#define NUM_OF_LANES(x)				(x << 8)
#define DIRECT_SPEED_CHANGE			BIT(17)

#define PCIE_TYPE0_STATUS_COMMAND_REG_1		0x004
#define PCI_TYPE0_BUS_MASTER_EN			BIT(2)

#define PCIE_MISC_CONTROL_1_REG			0x8BC
#define DBI_RO_WR_EN				BIT(0)

#define PCIE_TYPE0_SLOT_CAPABILITIES_REG	0x84

#define PCIE_LINK_UP_DELAY			100
#define PCIE_LINK_UP_TIMEOUT			PCIE_LINK_UP_DELAY * 500

#define MAX_PCIE				4

#define TWO_PORT_MODE				BIT(1)

#define PARF_BDF_TO_SID_TABLE			0x2000

struct pcie_dw_qti {
	struct pcie_dw dw;
	struct phy phy;
	struct gpio_desc rst_gpio;
	phys_addr_t parf;
	phys_addr_t elbi;
	uint32_t max_link_speed;
	uint32_t lanes;
	uint32_t id;
};

struct pcie_info {
	phys_addr_t reg;
	uint8_t sts_bit;
};

struct pcie_sku {
	int max_pcie;
	struct pcie_info sku_info[MAX_PCIE];
};

static int is_pcie_link_up(struct pcie_dw_qti *pcie)
{
	u32 val, ret;
	phys_addr_t status;

	status = (phys_addr_t)pcie->dw.dbi_base +
				PCIE_LINK_CONTROL_LINK_STATUS_REG;

	ret = readl_poll_sleep_timeout(status, val,
			(val & PCIE_CAP_DLL_ACTIVE) == PCIE_CAP_DLL_ACTIVE,
			PCIE_LINK_UP_DELAY, PCIE_LINK_UP_TIMEOUT);

	return !ret;
}

static int pcie_dw_qti_pcie_link_up(struct pcie_dw_qti *pcie)
{
	u32 val, ret;

	writel(DEVICE_TYPE_RC, pcie->parf + PCIE_PARF_DEVICE_TYPE);

	writel(BYPASS | MSTR_AXI_CLK_EN | AHB_CLK_EN,
		pcie->parf + PARF_MHI_CLOCK_RESET_CTRL);

	dw_pcie_dbi_write_enable(&pcie->dw, true);

	writel(GEN3_EQUALIZATION_DISABLE | RXEQ_RGRDLESS_RXTS |
		GEN3_ZRXDC_NONCOMPL,
		pcie->dw.dbi_base + PCIE30_GEN3_RELATED_OFF);

	dw_pcie_dbi_write_enable(&pcie->dw, false);

	writel(ECAM_BLOCKER_EN_RANGE2 |
		MAC_PHY_POWERDOWN_IN_P2_D_MUX_EN |
		ECAM_REMOVE_OFFSET_EN | ECAM_BLOCKER_EN |
		MST_WAKEUP_EN | SLV_WAKEUP_EN | MSTR_ACLK_CGC_DIS |
		SLV_ACLK_CGC_DIS | AUX_PWR_DET |
		CORE_CLK_2AUX_CLK_MUX_DIS | L23_CLK_RMV_DIS,
		pcie->parf + PCIE20_PARF_SYS_CTRL);

	dw_pcie_dbi_write_enable(&pcie->dw, true);

	writel(0x0, pcie->dw.dbi_base + PCIE_PORT_FORCE_REG);

	val = (L1_ENTRANCE_LATENCY(3) | L0_ENTRANCE_LATENCY(3) |
		COMMON_CLK_N_FTS(128) | ACK_N_FTS(128));
	writel(val, pcie->dw.dbi_base + PCIE_ACK_F_ASPM_CTRL_REG);

	val = (FAST_TRAINING_SEQ(128) | NUM_OF_LANES(2) | DIRECT_SPEED_CHANGE);
	writel(val, pcie->dw.dbi_base + PCIE_GEN2_CTRL_REG);

	writel(PCI_TYPE0_BUS_MASTER_EN,
		pcie->dw.dbi_base + PCIE_TYPE0_STATUS_COMMAND_REG_1);

	writel(DBI_RO_WR_EN, pcie->dw.dbi_base + PCIE_MISC_CONTROL_1_REG);

	writel(0x0002FD7F,
		pcie->dw.dbi_base + PCIE_TYPE0_SLOT_CAPABILITIES_REG);

	val = PCIE_CAP_MAX_LINK_SPEED(pcie->max_link_speed) |
		PCIE_CAP_MAX_LINK_WIDTH(pcie->lanes) |
		PCIE_CAP_ASPM_OPT_COMPLIANCE |
		PCIE_CAP_LINK_BW_NOT_CAP |
		PCIE_CAP_DLL_ACTIVE_REP_CAP |
		PCIE_CAP_L1_EXIT_LATENCY(4) |
		PCIE_CAP_L0S_EXIT_LATENCY(4);
	writel(val, pcie->dw.dbi_base + PCIE_LINK_CAPABILITY);

	writel(PCIE_CAP_CPL_TIMEOUT_DISABLE,
		pcie->dw.dbi_base + PCIE_DEVICE_CONTROL2_DEVICE_STATUS2_REG);

	val = readl(pcie->dw.dbi_base + PCIE_LINK_CTL_2);
	val &= ~TARGET_LINK_SPEED_MASK;
	val |=  PCIE_CAP_CURR_DEEMPHASIS | pcie->max_link_speed;
	writel(val, pcie->dw.dbi_base + PCIE_LINK_CTL_2);

	dw_pcie_dbi_write_enable(&pcie->dw, false);

	writel(LTSSM_EN, pcie->parf + PCIE_PCIE20_PARF_LTSSM);

	dm_gpio_set_value(&pcie->rst_gpio, 1);

	mdelay(1);

	ret = is_pcie_link_up(pcie);

	mdelay(1);

	for (val = 0; val < 255; val++) {
		writel(0, pcie->parf + PARF_BDF_TO_SID_TABLE +
			(4 * val));
		udelay(500);
	}

	return ret;
}

static int pcie_dw_qti_probe(struct udevice *dev)
{
	struct pcie_dw_qti *pcie = dev_get_priv(dev);
	struct udevice *ctlr = pci_get_controller(dev);
	struct pci_controller *hose = dev_get_uclass_priv(ctlr);
	struct clk_bulk clocks;
	struct reset_ctl_bulk resets;
	int ret, lane_reg;

	ret = reset_get_bulk(dev, &resets);
	if (!ret) {
		ret = reset_assert_bulk(&resets);
		if (ret)
			return ret;

		mdelay(10);

		ret = reset_deassert_bulk(&resets);
		if (ret)
			return ret;
	}

	ret = clk_get_bulk(dev, &clocks);
	if (!ret) {
		ret = clk_enable_bulk(&clocks);
		if (ret) {
			dev_err(dev, "Failed to enable clocks %d\n", ret);
			return ret;
		}
	}

	if (dev_read_bool(dev, "lane_mode")) {
		if (pcie->lanes == 1) {
			dev_read_u32(dev, "lane-regs", &lane_reg);
			if (lane_reg)
				writel(TWO_PORT_MODE, (phys_addr_t)lane_reg);
		}
	}

	dm_gpio_set_value(&pcie->rst_gpio, 0);

	ret = generic_phy_get_by_name(dev,  "pcie-phy", &pcie->phy);
	if (ret == 0) {
		ret = generic_phy_init(&pcie->phy);
		if (ret)
			return ret;
		ret = generic_phy_power_on(&pcie->phy);
		if (ret)
			return ret;
	}

	pcie->dw.first_busno = dev_seq(dev);
	pcie->dw.dev = dev;

	pcie_dw_setup_host(&pcie->dw);

	if (!pcie_dw_qti_pcie_link_up(pcie)){
		printf("PCIE%d: Link down\n", pcie->id);
		return -ENODEV;
	}

	printf("PCIE%d: Link up (Gen%d-x%d, Bus%d)\n", pcie->id,
		pcie_dw_get_link_speed(&pcie->dw),
		pcie_dw_get_link_width(&pcie->dw), hose->first_busno);

	pcie_dw_prog_outbound_atu_unroll(&pcie->dw, PCIE_ATU_REGION_INDEX0,
					PCIE_ATU_TYPE_MEM,
					pcie->dw.mem.phys_start,
					pcie->dw.mem.bus_start,
					pcie->dw.mem.size);

	return 0;
}

static int pcie_dw_qti_of_to_plat(struct udevice *dev)
{
	struct pcie_dw_qti *pcie = dev_get_priv(dev);
	struct pcie_sku *sku = (struct pcie_sku *)dev_get_driver_data(dev);

	pcie->id = dev_read_u32_default(dev, "id", 0);

	if (sku != NULL) {
		if ((pcie->id > sku->max_pcie) ||
			(readl(sku->sku_info[pcie->id].reg) &
				(1 << sku->sku_info[pcie->id].sts_bit))) {
			dev_err(dev, "PCIE%d disabled\n", pcie->id);
			return -ENXIO;
		}
	}

	pcie->dw.dbi_base = (void *)dev_read_addr_name(dev, "dbi");
	if (!pcie->dw.dbi_base)
		return -EINVAL;

	pcie->dw.cfg_base = (void *)dev_read_addr_name(dev, "config");
	if (!pcie->dw.cfg_base)
		return -EINVAL;

	pcie->dw.atu_base = (void *)dev_read_addr_name(dev, "atu");
	if (!pcie->dw.atu_base)
		return -EINVAL;

	pcie->parf = dev_read_addr_name(dev, "parf");
	if (!pcie->parf)
		return -EINVAL;

	pcie->elbi = dev_read_addr_name(dev, "elbi");
	if (!pcie->elbi)
		return -EINVAL;

	pcie->max_link_speed = dev_read_u32_default(dev, "max-link-speed", 3);
	pcie->lanes = dev_read_u32_default(dev, "num-lanes", 1);

	/* perst reset gpio */
        gpio_request_by_name_nodev(dev_ofnode(dev), "perst_gpio", 0,
                                   &pcie->rst_gpio, GPIOD_IS_OUT);

	return 0;
}

static const struct dm_pci_ops pcie_dw_qti_ops = {
	.read_config	= pcie_dw_read_config,
	.write_config	= pcie_dw_write_config,
};

static const struct pcie_sku ipq9574 = {
	.max_pcie = 4,
	.sku_info =
		{
			{
			.reg = 0xA401C,
			.sts_bit = 2,
			},
			{
			.reg = 0xA401C,
			.sts_bit = 3,
			},
			{
			.reg = 0xA401C,
			.sts_bit = 4,
			},
			{
			.reg = 0xA401C,
			.sts_bit = 5,
			},
		},
	};

static const struct pcie_sku ipq5332 = {
	.max_pcie = 3,
	.sku_info =
		{
			{
			.reg = 0xA4024,
			.sts_bit = 11,
			},
			{
			.reg = 0xA4024,
			.sts_bit = 12,
			},
			{
			.reg = 0xA4024,
			.sts_bit = 10,
			},
		},
	};

static const struct pcie_sku ipq5424 = {
	.max_pcie = 4,
	.sku_info =
		{
			{
			.reg = 0xA6244,
			.sts_bit = 0,
			},
			{
			.reg = 0xA624C,
			.sts_bit = 0,
			},
			{
			.reg = 0xA6254,
			.sts_bit = 0,
			},
			{
			.reg = 0xA625C,
			.sts_bit = 0,
			},
		},
	};

static const struct udevice_id pcie_dw_qti_ids[] = {
	{ .compatible = "qti,dw-pcie-ipq9574" , .data = (ulong)&ipq9574},
	{ .compatible = "qti,dw-pcie-ipq5332" , .data = (ulong)&ipq5332},
	{ .compatible = "qti,dw-pcie-ipq5424" , .data = (ulong)&ipq5424},
	{ }
};

U_BOOT_DRIVER(pcie_dw_qti) = {
	.name			= "pcie_dw_qti",
	.id			= UCLASS_PCI,
	.of_match		= pcie_dw_qti_ids,
	.ops			= &pcie_dw_qti_ops,
	.of_to_plat		= pcie_dw_qti_of_to_plat,
	.probe			= pcie_dw_qti_probe,
	.priv_auto		= sizeof(struct pcie_dw_qti),
};
