// SPDX-License-Identifier: GPL-2.0
/*
 * Qualcomm PCIe root complex driver
 *
 * Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
 * Copyright 2015 Linaro Limited.
 *
 * Author: Stanimir Varbanov <svarbanov@mm-sol.com>
 */

#include <linux/clk.h>
#include <linux/crc8.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/interconnect.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_reserved_mem.h>
#include <linux/pci.h>
#include <linux/pm_runtime.h>
#include <linux/platform_device.h>
#include <linux/phy/pcie.h>
#include <linux/phy/phy.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/nvmem-consumer.h>

#include "../../pci.h"
#include "../pci-host-common.h"
#include "pcie-designware.h"

/* PARF registers */
#define PARF_SYS_CTRL				0x00
#define PARF_PM_CTRL				0x20
#define PARF_PCS_DEEMPH				0x34
#define PARF_PCS_SWING				0x38
#define PARF_PHY_CTRL				0x40
#define PARF_PHY_REFCLK				0x4c
#define PARF_CONFIG_BITS			0x50
#define PARF_DBI_BASE_ADDR			0x168
#define PARF_MHI_CLOCK_RESET_CTRL		0x174
#define PARF_AXI_MSTR_WR_ADDR_HALT		0x178
#define PARF_AXI_MSTR_WR_ADDR_HALT_V2		0x1a8
#define PARF_AXI_MSTR_WR_ADDR_HALT_V2_MASK	0x3F
#define PARF_Q2A_FLUSH				0x1ac
#define PARF_LTSSM				0x1b0
#define PARF_SID_OFFSET				0x234
#define PARF_BDF_TRANSLATE_CFG			0x24c
#define PARF_SLV_ADDR_SPACE_SIZE		0x358
#define PARF_DEVICE_TYPE			0x1000
#define PARF_BDF_TO_SID_TABLE_N			0x2000
#define PARF_BDF_TO_SID_CFG			0x2c00

/* ELBI registers */
#define ELBI_SYS_CTRL				0x04

/* DBI registers */
#define AXI_MSTR_RESP_COMP_CTRL0		0x818
#define AXI_MSTR_RESP_COMP_CTRL1		0x81c

/* MHI registers */
#define PARF_DEBUG_CNT_PM_LINKST_IN_L2		0xc04
#define PARF_DEBUG_CNT_PM_LINKST_IN_L1		0xc0c
#define PARF_DEBUG_CNT_PM_LINKST_IN_L0S		0xc10
#define PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L1	0xc84
#define PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L2	0xc88

/* PARF_SYS_CTRL register fields */
#define MAC_PHY_POWERDOWN_IN_P2_D_MUX_EN	BIT(29)
#define MST_WAKEUP_EN				BIT(13)
#define SLV_WAKEUP_EN				BIT(12)
#define MSTR_ACLK_CGC_DIS			BIT(10)
#define SLV_ACLK_CGC_DIS			BIT(9)
#define CORE_CLK_CGC_DIS			BIT(6)
#define AUX_PWR_DET				BIT(4)
#define L23_CLK_RMV_DIS				BIT(2)
#define L1_CLK_RMV_DIS				BIT(1)

/* PARF_PM_CTRL register fields */
#define REQ_NOT_ENTR_L1				BIT(5)

/* PARF_PCS_DEEMPH register fields */
#define PCS_DEEMPH_TX_DEEMPH_GEN1(x)		FIELD_PREP(GENMASK(21, 16), x)
#define PCS_DEEMPH_TX_DEEMPH_GEN2_3_5DB(x)	FIELD_PREP(GENMASK(13, 8), x)
#define PCS_DEEMPH_TX_DEEMPH_GEN2_6DB(x)	FIELD_PREP(GENMASK(5, 0), x)

/* PARF_PCS_SWING register fields */
#define PCS_SWING_TX_SWING_FULL(x)		FIELD_PREP(GENMASK(14, 8), x)
#define PCS_SWING_TX_SWING_LOW(x)		FIELD_PREP(GENMASK(6, 0), x)

/* PARF_PHY_CTRL register fields */
#define PHY_CTRL_PHY_TX0_TERM_OFFSET_MASK	GENMASK(20, 16)
#define PHY_CTRL_PHY_TX0_TERM_OFFSET(x)		FIELD_PREP(PHY_CTRL_PHY_TX0_TERM_OFFSET_MASK, x)
#define PHY_TEST_PWR_DOWN			BIT(0)

/* PARF_PHY_REFCLK register fields */
#define PHY_REFCLK_SSP_EN			BIT(16)
#define PHY_REFCLK_USE_PAD			BIT(12)

/* PARF_CONFIG_BITS register fields */
#define PHY_RX0_EQ(x)				FIELD_PREP(GENMASK(26, 24), x)

/* PARF_SLV_ADDR_SPACE_SIZE register value */
#define SLV_ADDR_SPACE_SZ			0x10000000
#define SLV_ADDR_SPACE_SZ_1_27_0		0x08000000

/* DBI registers */
#define PCIE20_PORT_LINK_CTRL_OFF		0x710
#define PCIE20_PORT_LINK_CTRL_OFF_MASK		0x3F0000
#define LINK_CAPABLE_OFFSET(x)			((x) << 16)
#define PCIE20_LANE_SKEW_OFF			0x714
#define PCIE20_LANE_SKEW_OFF_MASK		0xFF000000
#define PCIE20_LANE_SKEW_DYNAMIC_SWITCH_VAL	0x20
#define PCIE20_MULTI_LANE_CONTROL_OFF		0x8C0
#define PCIE20_LINK_CONTROL_LINK_STATUS_REG	0x80
#define PCIE20_PARF_LTSSM_MASK			0x3F

/* RATEADAPT_VAL = 256 / ((NOC frequency / PCIe AXI frequency) - 1) */
/* RATEADAPT_VAL = 256 / ((342M / 240M) - 1) */
#define AGGR_NOC_PCIE_1LANE_RATEADAPT_VAL	0x200

/* PARF_MHI_CLOCK_RESET_CTRL register fields */
#define AHB_CLK_EN				BIT(0)
#define MSTR_AXI_CLK_EN				BIT(1)
#define BYPASS					BIT(4)

#define PARF_INT_ALL_STATUS			0x224
#define PARF_INT_ALL_CLEAR			0x228
#define PARF_INT_ALL_MASK			0x22c
#define PARF_STATUS                             0x230

/* PARF_INT_ALL_{STATUS/CLEAR/MASK} register fields */
#define PARF_INT_ALL_LINK_DOWN                  BIT(1)
#define PARF_INT_ALL_LINK_UP                    BIT(13)

/* Virtual Channel registers and fields */
#define PCIE_TYPE0_VC_CAPABILITIES_REG_1 	0x14c
#define PCIE_TYPE0_RESOURCE_CON_REG_VC0 	0x15c
#define PCIE_TYPE0_RESOURCE_CON_REG_VC1 	0x168
#define PCIE_TYPE0_RESOURCE_STATUS_REG_VC1 	0x16c

#define EP_REG_BASE 				0x1E1DFFF
#define WINDOW_RANGE_MASK 			0x7FFFE

#define VC_EXT_VC_CNT_MASK 			GENMASK(2, 0)
#define VC_TC_MAP_VC_BIT1_MASK 			GENMASK(7, 1)
#define VC_ID_VC1_MASK 				GENMASK(26, 24)
#define VC_ENABLE_VC1_MASK 			BIT(31)
#define VC_NEGO_PENDING_VC1_MASK 		BIT(17)

#define VC_EXT_VC_CNT_OFFSET(x) 		((x) << 0)
#define VC_TC_MAP_VC_BIT1_OFFSET(x) 		((x) << 1)
#define VC_ID_VC1_OFFSET(x) 			((x) << 24)
#define VC_ENABLE_VC1_OFFSET(x) 		((x) << 31)
#define VC_NEGO_PENDING_VC1_OFFSET(x) 		((x) << 17)

/* PARF_AXI_MSTR_WR_ADDR_HALT register fields */
#define EN					BIT(31)

/* PARF_LTSSM register fields */
#define LTSSM_EN				BIT(8)
#define SW_CLEAR_FLUSH_MODE			BIT(10)
#define FLUSH_MODE				BIT(11)

/* PARF_DEVICE_TYPE register fields */
#define DEVICE_TYPE_RC				0x4

/* PARF_BDF_TO_SID_CFG fields */
#define BDF_TO_SID_BYPASS			BIT(0)

/* PARF_STATUS fields */
#define FLUSH_COMPLETED				BIT(9)

/* ELBI_SYS_CTRL register fields */
#define ELBI_SYS_CTRL_LT_ENABLE			BIT(0)

/* AXI_MSTR_RESP_COMP_CTRL0 register fields */
#define CFG_REMOTE_RD_REQ_BRIDGE_SIZE_2K	0x4
#define CFG_REMOTE_RD_REQ_BRIDGE_SIZE_4K	0x5

/* AXI_MSTR_RESP_COMP_CTRL1 register fields */
#define CFG_BRIDGE_SB_INIT			BIT(0)

/* PCI_EXP_SLTCAP register fields */
#define PCIE_CAP_SLOT_POWER_LIMIT_VAL		FIELD_PREP(PCI_EXP_SLTCAP_SPLV, 250)
#define PCIE_CAP_SLOT_POWER_LIMIT_SCALE		FIELD_PREP(PCI_EXP_SLTCAP_SPLS, 1)
#define PCIE_CAP_SLOT_VAL			(PCI_EXP_SLTCAP_ABP | \
						PCI_EXP_SLTCAP_PCP | \
						PCI_EXP_SLTCAP_MRLSP | \
						PCI_EXP_SLTCAP_AIP | \
						PCI_EXP_SLTCAP_PIP | \
						PCI_EXP_SLTCAP_HPS | \
						PCI_EXP_SLTCAP_EIP | \
						PCIE_CAP_SLOT_POWER_LIMIT_VAL | \
						PCIE_CAP_SLOT_POWER_LIMIT_SCALE)

#define PERST_DELAY_US				1000
#define FLUSH_TIMEOUT_US                        100

#define QCOM_PCIE_CRC8_POLYNOMIAL		(BIT(2) | BIT(1) | BIT(0))

#define PCIE_CAP_LINK_SPEED_MASK		GENMASK(19, 16)
#define PCIE_CAP_NEGO_LINK_WIDTH_MASK		GENMASK(25, 20)
#define PCIE_CAP_RETRAIN_LINK_MASK		GENMASK(5, 5)
#define PCIE_CAP_LINK_SPEED_SHIFT(x)		((x) >> 16)
#define PCIE_CAP_NEGO_LINK_WIDTH_SHIFT(x)	((x) >> 20)
#define PCIE_CAP_RETRAIN_LINK_OFFSET(x)		((x) << 5)
#define PCIE_CAP_TARGET_LINK_SPD_MASK		GENMASK(3, 0)
#define QCOM_IPQ9574_DEVICE_ID			0x1108
#define QCOM_IPQ5332_DEVICE_ID			0x1005
#define QCOM_IPQ5424_DEVICE_ID			0x1006
#define QCOM_IPQ9650_DEVICE_ID			0x1007
#define PCIE20_LNK_CONTROL2_LINK_STATUS2	0xa0

/* Early Cal related fields */
#define CAL_COMPLETE_TIMEOUT_SECS		60
#define CAL_COMPLETE_MAGIC			0xCAFECACE
#define CAL_PANIC_MAGIC				0xDEADBEEF

static bool paniconcaltimeout = true;
module_param(paniconcaltimeout, bool, 0644);
MODULE_PARM_DESC(paniconcaltimeout, "Panic on calibration timeout: 0,1");

/*
 * Some EP devices need additional time after calibration completes
 * before they're ready for enumeration. This parameter allows tuning
 * that delay based on specific hardware requirements.
 */
static unsigned int cal_reset_wait_ms = 1000;
module_param(cal_reset_wait_ms, uint, 0644);
MODULE_PARM_DESC(cal_reset_wait_ms, "Time to wait, in ms, after calibration complete");

#define QCOM_PCIE_1_0_0_MAX_CLOCKS		4
struct qcom_pcie_resources_1_0_0 {
	struct clk_bulk_data clks[QCOM_PCIE_1_0_0_MAX_CLOCKS];
	struct reset_control *core;
	struct regulator *vdda;
};

#define QCOM_PCIE_2_1_0_MAX_CLOCKS		5
#define QCOM_PCIE_2_1_0_MAX_RESETS		6
#define QCOM_PCIE_2_1_0_MAX_SUPPLY		3

#define PCIE_IATU_REGION_CTRL_1_OFF_INBOUND_0		0x100
#define PCIE_IATU_REGION_CTRL_2_OFF_INBOUND_0		0x104
#define PCIE_IATU_LWR_BASE_ADDR_OFF_INBOUND_0		0x108
#define PCIE_IATU_UPPER_BASE_ADDR_OFF_INBOUND_0		0x10c
#define PCIE_IATU_LIMIT_ADDR_OFF_INBOUND_0		0x110
#define PCIE_IATU_LWR_TARGET_ADDR_OFF_INBOUND_0		0x114
#define PCIE_IATU_UPPER_TARGET_ADDR_OFF_INBOUND_0	0x118
#define PCIE_IATU_UPPR_LIMIT_ADDR_OFF_INBOUND_0		0x120

struct qcom_pcie_resources_2_1_0 {
	struct clk_bulk_data clks[QCOM_PCIE_2_1_0_MAX_CLOCKS];
	struct reset_control_bulk_data resets[QCOM_PCIE_2_1_0_MAX_RESETS];
	int num_resets;
	struct regulator_bulk_data supplies[QCOM_PCIE_2_1_0_MAX_SUPPLY];
};

#define QCOM_PCIE_2_3_2_MAX_CLOCKS		4
#define QCOM_PCIE_2_3_2_MAX_SUPPLY		2
struct qcom_pcie_resources_2_3_2 {
	struct clk_bulk_data clks[QCOM_PCIE_2_3_2_MAX_CLOCKS];
	struct regulator_bulk_data supplies[QCOM_PCIE_2_3_2_MAX_SUPPLY];
};

#define QCOM_PCIE_2_3_3_MAX_CLOCKS		5
#define QCOM_PCIE_2_3_3_MAX_RESETS		7
struct qcom_pcie_resources_2_3_3 {
	struct clk_bulk_data clks[QCOM_PCIE_2_3_3_MAX_CLOCKS];
	struct reset_control_bulk_data rst[QCOM_PCIE_2_3_3_MAX_RESETS];
};

#define QCOM_PCIE_2_4_0_MAX_CLOCKS		4
#define QCOM_PCIE_2_4_0_MAX_RESETS		12
struct qcom_pcie_resources_2_4_0 {
	struct clk_bulk_data clks[QCOM_PCIE_2_4_0_MAX_CLOCKS];
	int num_clks;
	struct reset_control_bulk_data resets[QCOM_PCIE_2_4_0_MAX_RESETS];
	int num_resets;
};

#define QCOM_PCIE_2_7_0_MAX_CLOCKS		15
#define QCOM_PCIE_2_7_0_MAX_SUPPLIES		2
struct qcom_pcie_resources_2_7_0 {
	struct clk_bulk_data clks[QCOM_PCIE_2_7_0_MAX_CLOCKS];
	int num_clks;
	struct regulator_bulk_data supplies[QCOM_PCIE_2_7_0_MAX_SUPPLIES];
	struct reset_control *rst;
};

struct qcom_pcie_resources_2_9_0 {
	struct clk_bulk_data *clks;
	struct reset_control *rst;
	int num_clks;
};

union qcom_pcie_resources {
	struct qcom_pcie_resources_1_0_0 v1_0_0;
	struct qcom_pcie_resources_2_1_0 v2_1_0;
	struct qcom_pcie_resources_2_3_2 v2_3_2;
	struct qcom_pcie_resources_2_3_3 v2_3_3;
	struct qcom_pcie_resources_2_4_0 v2_4_0;
	struct qcom_pcie_resources_2_7_0 v2_7_0;
	struct qcom_pcie_resources_2_9_0 v2_9_0;
};

struct qcom_pcie;

struct qcom_pcie_ops {
	int (*get_resources)(struct qcom_pcie *pcie);
	int (*init)(struct qcom_pcie *pcie);
	int (*post_init)(struct qcom_pcie *pcie);
	void (*deinit)(struct qcom_pcie *pcie);
	void (*ltssm_enable)(struct qcom_pcie *pcie);
	int (*config_sid)(struct qcom_pcie *pcie);
	void (*reset)(struct qcom_pcie *pcie);
};

struct qcom_pcie_cfg {
	const struct qcom_pcie_ops *ops;
};

struct qcom_pcie {
	struct dw_pcie *pci;
	void __iomem *parf;			/* DT parf */
	void __iomem *elbi;			/* DT elbi */
	void __iomem *mhi;
	resource_size_t parf_size;
	void __iomem *aggr_noc;
	void __iomem *system_noc;
	union qcom_pcie_resources res;
	struct phy *phy;
	struct gpio_desc *reset;
	struct icc_path *icc_mem;
	const struct qcom_pcie_cfg *cfg;
	struct dentry *debugfs;
	bool suspended;
	uint32_t axi_wr_addr_halt;
	uint32_t aggr_noc_rate_adap_val;
	uint32_t system_noc_rate_adap_val;
	uint32_t domain;
	int global_irq;
	int wake_irq;
	struct completion wake_event;
	bool enable_vc;
	bool enable_iatu;
	u32 iatu_ib0_base_addr[3];	/* Low , High, Limit */
	u32 iatu_ib0_target_addr[3];	/* Low , High, Limit */
	bool enumerated;
	u32 rc_idx;
	bool is_emulation;
};

struct qcom_pcie_info{
	struct qcom_pcie *pcie;
	struct list_head node;
};

LIST_HEAD(qcom_pcie_list);

#define to_qcom_pcie(x)		dev_get_drvdata((x)->dev)

#define WAKE_EVENT_MSEC_DELAY 5000
static int qcom_pcie_reset_slot(struct pci_host_bridge *bridge,
				struct pci_dev *pdev);

static void qcom_ep_reset_assert(struct qcom_pcie *pcie)
{
	gpiod_set_value_cansleep(pcie->reset, 1);
	usleep_range(PERST_DELAY_US, PERST_DELAY_US + 500);
}

static void qcom_ep_reset_deassert(struct qcom_pcie *pcie)
{
	/* Ensure that PERST has been asserted for at least 100 ms */
	msleep(100);
	gpiod_set_value_cansleep(pcie->reset, 0);
	usleep_range(PERST_DELAY_US, PERST_DELAY_US + 500);
}

static int qcom_pcie_start_link(struct dw_pcie *pci)
{
	struct qcom_pcie *pcie = to_qcom_pcie(pci);

	/* Enable Link Training state machine */
	if (pcie->cfg->ops->ltssm_enable)
		pcie->cfg->ops->ltssm_enable(pcie);

	return 0;
}

static irqreturn_t qcom_pcie_wake_irq_handler(int irq, void *data)
{
	struct qcom_pcie *pcie = data;

	complete(&pcie->wake_event);

	return IRQ_HANDLED;
}

static irqreturn_t qcom_pcie_global_irq_thread_fn(int irq, void *data)
{
	struct qcom_pcie *pcie = data;
	struct dw_pcie_rp *pp = &pcie->pci->pp;
	u32 status, mask;

	status = readl_relaxed(pcie->parf + PARF_INT_ALL_STATUS);
	mask = readl_relaxed(pcie->parf + PARF_INT_ALL_MASK);

	writel_relaxed(status, pcie->parf + PARF_INT_ALL_CLEAR);

	status &= mask;

	if (status & PARF_INT_ALL_LINK_DOWN) {
		dev_info(pcie->pci->dev, "Received Link down event\n");
		/*
		 * Only invoke the link recovery path when AER is available.
		 * pci_aer_available() returns false when AER is disabled via
		 * the "pci=noaer" kernel command line parameter, allowing
		 * platforms that do not require link recovery to opt out without
		 * any driver or device tree changes.
		 */
		if (pci_aer_available())
			pci_host_handle_link_down(pp->bridge);
	} else if (status & PARF_INT_ALL_LINK_UP) {
		dev_info(pcie->pci->dev, "Received Link up event\n");
	}

	return IRQ_HANDLED;
}

static void qcom_pcie_clear_hpc(struct dw_pcie *pci)
{
	u16 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u32 val;

	dw_pcie_dbi_ro_wr_en(pci);

	val = readl(pci->dbi_base + offset + PCI_EXP_SLTCAP);
	val &= ~PCI_EXP_SLTCAP_HPC;
	writel(val, pci->dbi_base + offset + PCI_EXP_SLTCAP);

	dw_pcie_dbi_ro_wr_dis(pci);
}

static void qcom_pcie_2_1_0_ltssm_enable(struct qcom_pcie *pcie)
{
	u32 val;

	/* enable link training */
	val = readl(pcie->elbi + ELBI_SYS_CTRL);
	val |= ELBI_SYS_CTRL_LT_ENABLE;
	writel(val, pcie->elbi + ELBI_SYS_CTRL);
}

static int qcom_pcie_get_resources_2_1_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_1_0 *res = &pcie->res.v2_1_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	bool is_apq = of_device_is_compatible(dev->of_node, "qcom,pcie-apq8064");
	int ret;

	res->supplies[0].supply = "vdda";
	res->supplies[1].supply = "vdda_phy";
	res->supplies[2].supply = "vdda_refclk";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(res->supplies),
				      res->supplies);
	if (ret)
		return ret;

	res->clks[0].id = "iface";
	res->clks[1].id = "core";
	res->clks[2].id = "phy";
	res->clks[3].id = "aux";
	res->clks[4].id = "ref";

	/* iface, core, phy are required */
	ret = devm_clk_bulk_get(dev, 3, res->clks);
	if (ret < 0)
		return ret;

	/* aux, ref are optional */
	ret = devm_clk_bulk_get_optional(dev, 2, res->clks + 3);
	if (ret < 0)
		return ret;

	res->resets[0].id = "pci";
	res->resets[1].id = "axi";
	res->resets[2].id = "ahb";
	res->resets[3].id = "por";
	res->resets[4].id = "phy";
	res->resets[5].id = "ext";

	/* ext is optional on APQ8016 */
	res->num_resets = is_apq ? 5 : 6;
	ret = devm_reset_control_bulk_get_exclusive(dev, res->num_resets, res->resets);
	if (ret < 0)
		return ret;

	return 0;
}

static void qcom_pcie_deinit_2_1_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_1_0 *res = &pcie->res.v2_1_0;

	clk_bulk_disable_unprepare(ARRAY_SIZE(res->clks), res->clks);
	reset_control_bulk_assert(res->num_resets, res->resets);

	writel(1, pcie->parf + PARF_PHY_CTRL);

	regulator_bulk_disable(ARRAY_SIZE(res->supplies), res->supplies);
}

static int qcom_pcie_init_2_1_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_1_0 *res = &pcie->res.v2_1_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	/* reset the PCIe interface as uboot can leave it undefined state */
	ret = reset_control_bulk_assert(res->num_resets, res->resets);
	if (ret < 0) {
		dev_err(dev, "cannot assert resets\n");
		return ret;
	}

	ret = regulator_bulk_enable(ARRAY_SIZE(res->supplies), res->supplies);
	if (ret < 0) {
		dev_err(dev, "cannot enable regulators\n");
		return ret;
	}

	ret = reset_control_bulk_deassert(res->num_resets, res->resets);
	if (ret < 0) {
		dev_err(dev, "cannot deassert resets\n");
		regulator_bulk_disable(ARRAY_SIZE(res->supplies), res->supplies);
		return ret;
	}

	return 0;
}

static int qcom_pcie_post_init_2_1_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_1_0 *res = &pcie->res.v2_1_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	struct device_node *node = dev->of_node;
	u32 val;
	int ret;

	/* enable PCIe clocks and resets */
	val = readl(pcie->parf + PARF_PHY_CTRL);
	val &= ~PHY_TEST_PWR_DOWN;
	writel(val, pcie->parf + PARF_PHY_CTRL);

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(res->clks), res->clks);
	if (ret)
		return ret;

	if (of_device_is_compatible(node, "qcom,pcie-ipq8064") ||
	    of_device_is_compatible(node, "qcom,pcie-ipq8064-v2")) {
		writel(PCS_DEEMPH_TX_DEEMPH_GEN1(24) |
			       PCS_DEEMPH_TX_DEEMPH_GEN2_3_5DB(24) |
			       PCS_DEEMPH_TX_DEEMPH_GEN2_6DB(34),
		       pcie->parf + PARF_PCS_DEEMPH);
		writel(PCS_SWING_TX_SWING_FULL(120) |
			       PCS_SWING_TX_SWING_LOW(120),
		       pcie->parf + PARF_PCS_SWING);
		writel(PHY_RX0_EQ(4), pcie->parf + PARF_CONFIG_BITS);
	}

	if (of_device_is_compatible(node, "qcom,pcie-ipq8064")) {
		/* set TX termination offset */
		val = readl(pcie->parf + PARF_PHY_CTRL);
		val &= ~PHY_CTRL_PHY_TX0_TERM_OFFSET_MASK;
		val |= PHY_CTRL_PHY_TX0_TERM_OFFSET(7);
		writel(val, pcie->parf + PARF_PHY_CTRL);
	}

	/* enable external reference clock */
	val = readl(pcie->parf + PARF_PHY_REFCLK);
	/* USE_PAD is required only for ipq806x */
	if (!of_device_is_compatible(node, "qcom,pcie-apq8064"))
		val &= ~PHY_REFCLK_USE_PAD;
	val |= PHY_REFCLK_SSP_EN;
	writel(val, pcie->parf + PARF_PHY_REFCLK);

	/* wait for clock acquisition */
	usleep_range(1000, 1500);

	/* Set the Max TLP size to 2K, instead of using default of 4K */
	writel(CFG_REMOTE_RD_REQ_BRIDGE_SIZE_2K,
	       pci->dbi_base + AXI_MSTR_RESP_COMP_CTRL0);
	writel(CFG_BRIDGE_SB_INIT,
	       pci->dbi_base + AXI_MSTR_RESP_COMP_CTRL1);

	qcom_pcie_clear_hpc(pcie->pci);

	return 0;
}

static int qcom_pcie_get_resources_1_0_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_1_0_0 *res = &pcie->res.v1_0_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	res->vdda = devm_regulator_get(dev, "vdda");
	if (IS_ERR(res->vdda))
		return PTR_ERR(res->vdda);

	res->clks[0].id = "iface";
	res->clks[1].id = "aux";
	res->clks[2].id = "master_bus";
	res->clks[3].id = "slave_bus";

	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(res->clks), res->clks);
	if (ret < 0)
		return ret;

	res->core = devm_reset_control_get_exclusive(dev, "core");
	return PTR_ERR_OR_ZERO(res->core);
}

static void qcom_pcie_deinit_1_0_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_1_0_0 *res = &pcie->res.v1_0_0;

	reset_control_assert(res->core);
	clk_bulk_disable_unprepare(ARRAY_SIZE(res->clks), res->clks);
	regulator_disable(res->vdda);
}

static int qcom_pcie_init_1_0_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_1_0_0 *res = &pcie->res.v1_0_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	ret = reset_control_deassert(res->core);
	if (ret) {
		dev_err(dev, "cannot deassert core reset\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(res->clks), res->clks);
	if (ret) {
		dev_err(dev, "cannot prepare/enable clocks\n");
		goto err_assert_reset;
	}

	ret = regulator_enable(res->vdda);
	if (ret) {
		dev_err(dev, "cannot enable vdda regulator\n");
		goto err_disable_clks;
	}

	return 0;

err_disable_clks:
	clk_bulk_disable_unprepare(ARRAY_SIZE(res->clks), res->clks);
err_assert_reset:
	reset_control_assert(res->core);

	return ret;
}

static int qcom_pcie_post_init_1_0_0(struct qcom_pcie *pcie)
{
	/* change DBI base address */
	writel(0, pcie->parf + PARF_DBI_BASE_ADDR);

	if (IS_ENABLED(CONFIG_PCI_MSI)) {
		u32 val = readl(pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT);

		val |= EN;
		writel(val, pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT);
	}

	qcom_pcie_clear_hpc(pcie->pci);

	return 0;
}

static void qcom_pcie_2_3_2_ltssm_enable(struct qcom_pcie *pcie)
{
	u32 val;

	/* enable link training */
	val = readl(pcie->parf + PARF_LTSSM);
	val |= LTSSM_EN;
	writel(val, pcie->parf + PARF_LTSSM);
}

static int qcom_pcie_get_resources_2_3_2(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_3_2 *res = &pcie->res.v2_3_2;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	res->supplies[0].supply = "vdda";
	res->supplies[1].supply = "vddpe-3v3";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(res->supplies),
				      res->supplies);
	if (ret)
		return ret;

	res->clks[0].id = "aux";
	res->clks[1].id = "cfg";
	res->clks[2].id = "bus_master";
	res->clks[3].id = "bus_slave";

	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(res->clks), res->clks);
	if (ret < 0)
		return ret;

	return 0;
}

static void qcom_pcie_deinit_2_3_2(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_3_2 *res = &pcie->res.v2_3_2;

	clk_bulk_disable_unprepare(ARRAY_SIZE(res->clks), res->clks);
	regulator_bulk_disable(ARRAY_SIZE(res->supplies), res->supplies);
}

static int qcom_pcie_init_2_3_2(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_3_2 *res = &pcie->res.v2_3_2;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(res->supplies), res->supplies);
	if (ret < 0) {
		dev_err(dev, "cannot enable regulators\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(res->clks), res->clks);
	if (ret) {
		dev_err(dev, "cannot prepare/enable clocks\n");
		regulator_bulk_disable(ARRAY_SIZE(res->supplies), res->supplies);
		return ret;
	}

	return 0;
}

static int qcom_pcie_post_init_2_3_2(struct qcom_pcie *pcie)
{
	u32 val;

	/* enable PCIe clocks and resets */
	val = readl(pcie->parf + PARF_PHY_CTRL);
	val &= ~PHY_TEST_PWR_DOWN;
	writel(val, pcie->parf + PARF_PHY_CTRL);

	/* change DBI base address */
	writel(0, pcie->parf + PARF_DBI_BASE_ADDR);

	/* MAC PHY_POWERDOWN MUX DISABLE  */
	val = readl(pcie->parf + PARF_SYS_CTRL);
	val &= ~MAC_PHY_POWERDOWN_IN_P2_D_MUX_EN;
	writel(val, pcie->parf + PARF_SYS_CTRL);

	val = readl(pcie->parf + PARF_MHI_CLOCK_RESET_CTRL);
	val |= BYPASS;
	writel(val, pcie->parf + PARF_MHI_CLOCK_RESET_CTRL);

	val = readl(pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT_V2);
	val |= EN;
	writel(val, pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT_V2);

	qcom_pcie_clear_hpc(pcie->pci);

	return 0;
}

static int qcom_pcie_get_resources_2_4_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_4_0 *res = &pcie->res.v2_4_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	bool is_ipq = of_device_is_compatible(dev->of_node, "qcom,pcie-ipq4019");
	int ret;

	res->clks[0].id = "aux";
	res->clks[1].id = "master_bus";
	res->clks[2].id = "slave_bus";
	res->clks[3].id = "iface";

	/* qcom,pcie-ipq4019 is defined without "iface" */
	res->num_clks = is_ipq ? 3 : 4;

	ret = devm_clk_bulk_get(dev, res->num_clks, res->clks);
	if (ret < 0)
		return ret;

	res->resets[0].id = "axi_m";
	res->resets[1].id = "axi_s";
	res->resets[2].id = "axi_m_sticky";
	res->resets[3].id = "pipe_sticky";
	res->resets[4].id = "pwr";
	res->resets[5].id = "ahb";
	res->resets[6].id = "pipe";
	res->resets[7].id = "axi_m_vmid";
	res->resets[8].id = "axi_s_xpu";
	res->resets[9].id = "parf";
	res->resets[10].id = "phy";
	res->resets[11].id = "phy_ahb";

	res->num_resets = is_ipq ? 12 : 6;

	ret = devm_reset_control_bulk_get_exclusive(dev, res->num_resets, res->resets);
	if (ret < 0)
		return ret;

	return 0;
}

static void qcom_pcie_deinit_2_4_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_4_0 *res = &pcie->res.v2_4_0;

	reset_control_bulk_assert(res->num_resets, res->resets);
	clk_bulk_disable_unprepare(res->num_clks, res->clks);
}

static int qcom_pcie_init_2_4_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_4_0 *res = &pcie->res.v2_4_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	ret = reset_control_bulk_assert(res->num_resets, res->resets);
	if (ret < 0) {
		dev_err(dev, "cannot assert resets\n");
		return ret;
	}

	usleep_range(10000, 12000);

	ret = reset_control_bulk_deassert(res->num_resets, res->resets);
	if (ret < 0) {
		dev_err(dev, "cannot deassert resets\n");
		return ret;
	}

	usleep_range(10000, 12000);

	ret = clk_bulk_prepare_enable(res->num_clks, res->clks);
	if (ret) {
		reset_control_bulk_assert(res->num_resets, res->resets);
		return ret;
	}

	return 0;
}

static int qcom_pcie_get_resources_2_3_3(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_3_3 *res = &pcie->res.v2_3_3;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	res->clks[0].id = "iface";
	res->clks[1].id = "axi_m";
	res->clks[2].id = "axi_s";
	res->clks[3].id = "ahb";
	res->clks[4].id = "aux";

	ret = devm_clk_bulk_get(dev, ARRAY_SIZE(res->clks), res->clks);
	if (ret < 0)
		return ret;

	res->rst[0].id = "axi_m";
	res->rst[1].id = "axi_s";
	res->rst[2].id = "pipe";
	res->rst[3].id = "axi_m_sticky";
	res->rst[4].id = "sticky";
	res->rst[5].id = "ahb";
	res->rst[6].id = "sleep";

	ret = devm_reset_control_bulk_get_exclusive(dev, ARRAY_SIZE(res->rst), res->rst);
	if (ret < 0)
		return ret;

	return 0;
}

static void qcom_pcie_deinit_2_3_3(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_3_3 *res = &pcie->res.v2_3_3;

	clk_bulk_disable_unprepare(ARRAY_SIZE(res->clks), res->clks);
}

static int qcom_pcie_init_2_3_3(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_3_3 *res = &pcie->res.v2_3_3;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	int ret;

	ret = reset_control_bulk_assert(ARRAY_SIZE(res->rst), res->rst);
	if (ret < 0) {
		dev_err(dev, "cannot assert resets\n");
		return ret;
	}

	usleep_range(2000, 2500);

	ret = reset_control_bulk_deassert(ARRAY_SIZE(res->rst), res->rst);
	if (ret < 0) {
		dev_err(dev, "cannot deassert resets\n");
		return ret;
	}

	/*
	 * Don't have a way to see if the reset has completed.
	 * Wait for some time.
	 */
	usleep_range(2000, 2500);

	ret = clk_bulk_prepare_enable(ARRAY_SIZE(res->clks), res->clks);
	if (ret) {
		dev_err(dev, "cannot prepare/enable clocks\n");
		goto err_assert_resets;
	}

	return 0;

err_assert_resets:
	/*
	 * Not checking for failure, will anyway return
	 * the original failure in 'ret'.
	 */
	reset_control_bulk_assert(ARRAY_SIZE(res->rst), res->rst);

	return ret;
}

static int qcom_pcie_post_init_2_3_3(struct qcom_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	u16 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u32 val;

	writel(SLV_ADDR_SPACE_SZ, pcie->parf + PARF_SLV_ADDR_SPACE_SIZE);

	val = readl(pcie->parf + PARF_PHY_CTRL);
	val &= ~PHY_TEST_PWR_DOWN;
	writel(val, pcie->parf + PARF_PHY_CTRL);

	writel(0, pcie->parf + PARF_DBI_BASE_ADDR);

	writel(MST_WAKEUP_EN | SLV_WAKEUP_EN | MSTR_ACLK_CGC_DIS
		| SLV_ACLK_CGC_DIS | CORE_CLK_CGC_DIS |
		AUX_PWR_DET | L23_CLK_RMV_DIS | L1_CLK_RMV_DIS,
		pcie->parf + PARF_SYS_CTRL);
	writel(0, pcie->parf + PARF_Q2A_FLUSH);

	writel(PCI_COMMAND_MASTER, pci->dbi_base + PCI_COMMAND);

	dw_pcie_dbi_ro_wr_en(pci);

	writel(PCIE_CAP_SLOT_VAL, pci->dbi_base + offset + PCI_EXP_SLTCAP);

	val = readl(pci->dbi_base + offset + PCI_EXP_LNKCAP);
	val &= ~PCI_EXP_LNKCAP_ASPMS;
	writel(val, pci->dbi_base + offset + PCI_EXP_LNKCAP);

	writel(PCI_EXP_DEVCTL2_COMP_TMOUT_DIS, pci->dbi_base + offset +
		PCI_EXP_DEVCTL2);

	dw_pcie_dbi_ro_wr_dis(pci);

	return 0;
}

static int qcom_pcie_get_resources_2_7_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_7_0 *res = &pcie->res.v2_7_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	unsigned int num_clks, num_opt_clks;
	unsigned int idx;
	int ret;

	res->rst = devm_reset_control_array_get_exclusive(dev);
	if (IS_ERR(res->rst))
		return PTR_ERR(res->rst);

	res->supplies[0].supply = "vdda";
	res->supplies[1].supply = "vddpe-3v3";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(res->supplies),
				      res->supplies);
	if (ret)
		return ret;

	idx = 0;
	res->clks[idx++].id = "aux";
	res->clks[idx++].id = "cfg";
	res->clks[idx++].id = "bus_master";
	res->clks[idx++].id = "bus_slave";
	res->clks[idx++].id = "slave_q2a";

	num_clks = idx;

	ret = devm_clk_bulk_get(dev, num_clks, res->clks);
	if (ret < 0)
		return ret;

	res->clks[idx++].id = "tbu";
	res->clks[idx++].id = "ddrss_sf_tbu";
	res->clks[idx++].id = "aggre0";
	res->clks[idx++].id = "aggre1";
	res->clks[idx++].id = "noc_aggr";
	res->clks[idx++].id = "noc_aggr_4";
	res->clks[idx++].id = "noc_aggr_south_sf";
	res->clks[idx++].id = "cnoc_qx";
	res->clks[idx++].id = "sleep";
	res->clks[idx++].id = "cnoc_sf_axi";

	num_opt_clks = idx - num_clks;
	res->num_clks = idx;

	ret = devm_clk_bulk_get_optional(dev, num_opt_clks, res->clks + num_clks);
	if (ret < 0)
		return ret;

	return 0;
}

static int qcom_pcie_init_2_7_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_7_0 *res = &pcie->res.v2_7_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	u32 val;
	int ret;

	ret = regulator_bulk_enable(ARRAY_SIZE(res->supplies), res->supplies);
	if (ret < 0) {
		dev_err(dev, "cannot enable regulators\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(res->num_clks, res->clks);
	if (ret < 0)
		goto err_disable_regulators;

	ret = reset_control_assert(res->rst);
	if (ret) {
		dev_err(dev, "reset assert failed (%d)\n", ret);
		goto err_disable_clocks;
	}

	usleep_range(1000, 1500);

	ret = reset_control_deassert(res->rst);
	if (ret) {
		dev_err(dev, "reset deassert failed (%d)\n", ret);
		goto err_disable_clocks;
	}

	/* Wait for reset to complete, required on SM8450 */
	usleep_range(1000, 1500);

	/* configure PCIe to RC mode */
	writel(DEVICE_TYPE_RC, pcie->parf + PARF_DEVICE_TYPE);

	/* enable PCIe clocks and resets */
	val = readl(pcie->parf + PARF_PHY_CTRL);
	val &= ~PHY_TEST_PWR_DOWN;
	writel(val, pcie->parf + PARF_PHY_CTRL);

	/* change DBI base address */
	writel(0, pcie->parf + PARF_DBI_BASE_ADDR);

	/* MAC PHY_POWERDOWN MUX DISABLE  */
	val = readl(pcie->parf + PARF_SYS_CTRL);
	val &= ~MAC_PHY_POWERDOWN_IN_P2_D_MUX_EN;
	writel(val, pcie->parf + PARF_SYS_CTRL);

	val = readl(pcie->parf + PARF_MHI_CLOCK_RESET_CTRL);
	val |= BYPASS;
	writel(val, pcie->parf + PARF_MHI_CLOCK_RESET_CTRL);

	/* Enable L1 and L1SS */
	val = readl(pcie->parf + PARF_PM_CTRL);
	val &= ~REQ_NOT_ENTR_L1;
	writel(val, pcie->parf + PARF_PM_CTRL);

	val = readl(pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT_V2);
	val |= EN;
	writel(val, pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT_V2);

	return 0;
err_disable_clocks:
	clk_bulk_disable_unprepare(res->num_clks, res->clks);
err_disable_regulators:
	regulator_bulk_disable(ARRAY_SIZE(res->supplies), res->supplies);

	return ret;
}

static int qcom_pcie_post_init_2_7_0(struct qcom_pcie *pcie)
{
	qcom_pcie_clear_hpc(pcie->pci);

	return 0;
}

static void qcom_pcie_deinit_2_7_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_7_0 *res = &pcie->res.v2_7_0;

	clk_bulk_disable_unprepare(res->num_clks, res->clks);

	regulator_bulk_disable(ARRAY_SIZE(res->supplies), res->supplies);
}

static int qcom_pcie_config_sid_1_9_0(struct qcom_pcie *pcie)
{
	/* iommu map structure */
	struct {
		u32 bdf;
		u32 phandle;
		u32 smmu_sid;
		u32 smmu_sid_len;
	} *map;
	void __iomem *bdf_to_sid_base = pcie->parf + PARF_BDF_TO_SID_TABLE_N;
	struct device *dev = pcie->pci->dev;
	u8 qcom_pcie_crc8_table[CRC8_TABLE_SIZE];
	int i, nr_map, size = 0;
	u32 smmu_sid_base;
	u32 val;

	of_get_property(dev->of_node, "iommu-map", &size);
	if (!size)
		return 0;

	/* Enable BDF to SID translation by disabling bypass mode (default) */
	val = readl(pcie->parf + PARF_BDF_TO_SID_CFG);
	val &= ~BDF_TO_SID_BYPASS;
	writel(val, pcie->parf + PARF_BDF_TO_SID_CFG);

	map = kzalloc(size, GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	of_property_read_u32_array(dev->of_node, "iommu-map", (u32 *)map,
				   size / sizeof(u32));

	nr_map = size / (sizeof(*map));

	crc8_populate_msb(qcom_pcie_crc8_table, QCOM_PCIE_CRC8_POLYNOMIAL);

	/* Registers need to be zero out first */
	memset_io(bdf_to_sid_base, 0, CRC8_TABLE_SIZE * sizeof(u32));

	/* Extract the SMMU SID base from the first entry of iommu-map */
	smmu_sid_base = map[0].smmu_sid;

	/* Look for an available entry to hold the mapping */
	for (i = 0; i < nr_map; i++) {
		__be16 bdf_be = cpu_to_be16(map[i].bdf);
		u32 val;
		u8 hash;

		hash = crc8(qcom_pcie_crc8_table, (u8 *)&bdf_be, sizeof(bdf_be), 0);

		val = readl(bdf_to_sid_base + hash * sizeof(u32));

		/* If the register is already populated, look for next available entry */
		while (val) {
			u8 current_hash = hash++;
			u8 next_mask = 0xff;

			/* If NEXT field is NULL then update it with next hash */
			if (!(val & next_mask)) {
				val |= (u32)hash;
				writel(val, bdf_to_sid_base + current_hash * sizeof(u32));
			}

			val = readl(bdf_to_sid_base + hash * sizeof(u32));
		}

		/* BDF [31:16] | SID [15:8] | NEXT [7:0] */
		val = map[i].bdf << 16 | (map[i].smmu_sid - smmu_sid_base) << 8 | 0;
		writel(val, bdf_to_sid_base + hash * sizeof(u32));
	}

	kfree(map);

	return 0;
}

static int qcom_pcie_get_resources_2_9_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_9_0 *res = &pcie->res.v2_9_0;
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;

	res->num_clks = devm_clk_bulk_get_all(dev, &res->clks);
	if (res->num_clks < 0)
		return res->num_clks;

	res->rst = devm_reset_control_array_get_exclusive(dev);
	if (IS_ERR(res->rst))
		return PTR_ERR(res->rst);

	return 0;
}

static void qcom_pcie_reset_2_9_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_9_0 *res = &pcie->res.v2_9_0;
	int num_clks = res->num_clks;

	if (!res->clks || num_clks <= 0)
		return;

	while (--num_clks >= 0)
		clk_force_disable(res->clks[num_clks].clk);
}

static void qcom_pcie_deinit_2_9_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_9_0 *res = &pcie->res.v2_9_0;

	clk_bulk_disable_unprepare(res->num_clks, res->clks);
}

static int qcom_pcie_init_2_9_0(struct qcom_pcie *pcie)
{
	struct qcom_pcie_resources_2_9_0 *res = &pcie->res.v2_9_0;
	struct device *dev = pcie->pci->dev;
	int ret;

	ret = reset_control_assert(res->rst);
	if (ret) {
		dev_err(dev, "reset assert failed (%d)\n", ret);
		return ret;
	}

	/*
	 * Delay periods before and after reset deassert are working values
	 * from downstream Codeaurora kernel
	 */
	if (pcie->is_emulation)
		usleep_range(10000, 12000);
	else
		usleep_range(2000, 2500);

	ret = reset_control_deassert(res->rst);
	if (ret) {
		dev_err(dev, "reset deassert failed (%d)\n", ret);
		return ret;
	}

	if (pcie->is_emulation)
		usleep_range(10000, 12000);
	else
		usleep_range(2000, 2500);

	return clk_bulk_prepare_enable(res->num_clks, res->clks);
}

static int qcom_pcie_post_init(struct qcom_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	u16 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u32 val;
	int i;
	int size = 4;

	val = readl(pci->dbi_base + offset + PCI_EXP_DEVCTL);
	val &= ~PCI_EXP_DEVCTL_PAYLOAD;
	val |= PCI_EXP_DEVCTL_PAYLOAD_256B;
	writel(val, pci->dbi_base + offset + PCI_EXP_DEVCTL);

	val = readl(pcie->parf + PARF_PHY_CTRL);
	val &= ~PHY_TEST_PWR_DOWN;
	writel(val, pcie->parf + PARF_PHY_CTRL);

	writel(0, pcie->parf + PARF_DBI_BASE_ADDR);

	writel(DEVICE_TYPE_RC, pcie->parf + PARF_DEVICE_TYPE);
	writel(BYPASS | MSTR_AXI_CLK_EN | AHB_CLK_EN,
		pcie->parf + PARF_MHI_CLOCK_RESET_CTRL);
	writel(GEN3_RELATED_OFF_RXEQ_RGRDLESS_RXTS |
		GEN3_RELATED_OFF_GEN3_ZRXDC_NONCOMPL,
		pci->dbi_base + GEN3_RELATED_OFF);

	writel(MST_WAKEUP_EN | SLV_WAKEUP_EN | MSTR_ACLK_CGC_DIS |
		SLV_ACLK_CGC_DIS | CORE_CLK_CGC_DIS |
		AUX_PWR_DET | L23_CLK_RMV_DIS | L1_CLK_RMV_DIS,
		pcie->parf + PARF_SYS_CTRL);

	writel(0, pcie->parf + PARF_Q2A_FLUSH);

	if (pcie->axi_wr_addr_halt) {
		val = readl(pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT_V2);
		val &= ~PARF_AXI_MSTR_WR_ADDR_HALT_V2_MASK;
		writel(val | pcie->axi_wr_addr_halt,
			pcie->parf + PARF_AXI_MSTR_WR_ADDR_HALT_V2);
	}

	if (!IS_ERR_OR_NULL(pcie->aggr_noc)) {
		if (pcie->aggr_noc_rate_adap_val)
			writel(pcie->aggr_noc_rate_adap_val, pcie->aggr_noc);
		else
			writel(AGGR_NOC_PCIE_1LANE_RATEADAPT_VAL, pcie->aggr_noc);
	}

	if (!IS_ERR_OR_NULL(pcie->system_noc)) {
		if (pcie->system_noc_rate_adap_val)
			writel(pcie->system_noc_rate_adap_val, pcie->system_noc);
	}

	dw_pcie_dbi_ro_wr_en(pci);

	writel(PCIE_CAP_SLOT_VAL, pci->dbi_base + offset + PCI_EXP_SLTCAP);

#if defined(CONFIG_PCIEASPM)
	/* Remove L1 entry restriction only if ASPM is enabled */
	val = readl(pcie->parf + PARF_PM_CTRL);
	val &= ~REQ_NOT_ENTR_L1;
	writel(val, pcie->parf + PARF_PM_CTRL);
#else
	/* Clear ASPM capability bits only when ASPM is not enabled */
	val = readl(pci->dbi_base + offset + PCI_EXP_LNKCAP);
	val &= ~PCI_EXP_LNKCAP_ASPMS;
	writel(val, pci->dbi_base + offset + PCI_EXP_LNKCAP);
#endif

	/* Set lane skew parameter to enable dynamic lane switching at runtime */
	dw_pcie_read(pci->dbi_base + PCIE20_LANE_SKEW_OFF, size, &val);
	val = (val & PCIE20_LANE_SKEW_OFF_MASK) | PCIE20_LANE_SKEW_DYNAMIC_SWITCH_VAL;
	dw_pcie_write(pci->dbi_base + PCIE20_LANE_SKEW_OFF, size, val);

	writel(PCI_EXP_DEVCTL2_COMP_TMOUT_DIS, pci->dbi_base + offset +
			PCI_EXP_DEVCTL2);

	dw_pcie_dbi_ro_wr_dis(pci);

	for (i = 0; i < 256; i++)
		writel(0, pcie->parf + PARF_BDF_TO_SID_TABLE_N + (4 * i));

	if (pcie->enable_iatu) {
		/* Configure IATU Inbound 0 */
		writel(0x0, pci->atu_base +
		       PCIE_IATU_REGION_CTRL_1_OFF_INBOUND_0);

		/* Enable the region */
		writel(0x80000000, pci->atu_base +
		       PCIE_IATU_REGION_CTRL_2_OFF_INBOUND_0);

		/* base low address */
		writel(pcie->iatu_ib0_base_addr[0], pci->atu_base +
		       PCIE_IATU_LWR_BASE_ADDR_OFF_INBOUND_0);

		/* base high address */
		writel(pcie->iatu_ib0_base_addr[1], pci->atu_base +
		       PCIE_IATU_UPPER_BASE_ADDR_OFF_INBOUND_0);

		/* base limit address */
		writel(pcie->iatu_ib0_base_addr[2], pci->atu_base +
		       PCIE_IATU_LIMIT_ADDR_OFF_INBOUND_0);

		/* target low address */
		writel(pcie->iatu_ib0_target_addr[0], pci->atu_base +
		       PCIE_IATU_LWR_TARGET_ADDR_OFF_INBOUND_0);

		/* target high address */
		writel(pcie->iatu_ib0_target_addr[1], pci->atu_base +
		       PCIE_IATU_UPPER_TARGET_ADDR_OFF_INBOUND_0);

		/* target limit address */
		writel(pcie->iatu_ib0_target_addr[2], pci->atu_base +
		       PCIE_IATU_UPPR_LIMIT_ADDR_OFF_INBOUND_0);
	}

	return 0;
}

static int qcom_pcie_post_init_1_27_0(struct qcom_pcie *pcie)
{
	writel(SLV_ADDR_SPACE_SZ_1_27_0,
	       pcie->parf + PARF_SLV_ADDR_SPACE_SIZE);

	return qcom_pcie_post_init(pcie);
}

static int qcom_pcie_post_init_2_9_0(struct qcom_pcie *pcie)
{
	writel(SLV_ADDR_SPACE_SZ,
	       pcie->parf + PARF_SLV_ADDR_SPACE_SIZE);

	return qcom_pcie_post_init(pcie);
}

static int qcom_pcie_link_up(struct dw_pcie *pci)
{
	u16 offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	u16 val = readw(pci->dbi_base + offset + PCI_EXP_LNKSTA);

	return !!(val & PCI_EXP_LNKSTA_DLLLA);
}

static int qcom_pcie_establish_link(struct qcom_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;

	if (dw_pcie_link_up(pci))
		return 0;

	/* Enable Link Training state machine */
	if (pcie->cfg->ops->ltssm_enable)
		pcie->cfg->ops->ltssm_enable(pcie);

	return dw_pcie_wait_for_link(pci);
}

static int qcom_pcie_host_init(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct qcom_pcie *pcie = to_qcom_pcie(pci);
	int ret;

	qcom_ep_reset_assert(pcie);

	ret = pcie->cfg->ops->init(pcie);
	if (ret)
		return ret;

	ret = phy_set_mode_ext(pcie->phy, PHY_MODE_PCIE, PHY_MODE_PCIE_RC);
	if (ret)
		goto err_deinit;

	ret = phy_power_on(pcie->phy);
	if (ret)
		goto err_deinit;

	if (pcie->cfg->ops->post_init) {
		ret = pcie->cfg->ops->post_init(pcie);
		if (ret)
			goto err_disable_phy;
	}

	qcom_ep_reset_deassert(pcie);

	if (pcie->cfg->ops->config_sid) {
		ret = pcie->cfg->ops->config_sid(pcie);
		if (ret)
			goto err_assert_reset;
	}

	/*
	 * Register the reset_slot() callback only when AER is available.
	 * Platforms that do not require link recovery can disable it by
	 * passing "pci=noaer" on the kernel command line, which causes
	 * pci_aer_available() to return false.  Without this check,
	 * reset_slot() would be invoked on a manual device reset
	 * ("echo 1 > /sys/bus/pci/devices/<dev>/reset") and unexpectedly
	 * reset the endpoint on non-RDP configurations.
	 */
	if (pci_aer_available())
		pp->bridge->reset_slot = qcom_pcie_reset_slot;
	return 0;

err_assert_reset:
	qcom_ep_reset_assert(pcie);
err_disable_phy:
	phy_power_off(pcie->phy);
err_deinit:
	pcie->cfg->ops->deinit(pcie);

	return ret;
}

int pcie_set_link_speed(struct pci_dev *dev, u16 target_link_speed)
{
	struct dw_pcie_rp *pp;
	struct dw_pcie *pci;
	u32 val;

	if (dev->device != QCOM_IPQ9574_DEVICE_ID &&
	    dev->device != QCOM_IPQ5332_DEVICE_ID &&
	    dev->device != QCOM_IPQ5424_DEVICE_ID &&
	    dev->device != QCOM_IPQ9650_DEVICE_ID)
		return -EINVAL;

	if (target_link_speed < 1 || target_link_speed > 3)
		return -EINVAL;

	pp = dev->bus->sysdata;
	pci = to_dw_pcie_from_pp(pp);

	val = readl(pci->dbi_base + PCIE20_LNK_CONTROL2_LINK_STATUS2);
	val &= ~PCIE_CAP_TARGET_LINK_SPD_MASK;
	val |= target_link_speed;
	writel(val, pci->dbi_base + PCIE20_LNK_CONTROL2_LINK_STATUS2);

	val = readl(pci->dbi_base + PCIE20_LINK_CONTROL_LINK_STATUS_REG);
	val &= ~PCIE_CAP_RETRAIN_LINK_MASK;
	val |= PCIE_CAP_RETRAIN_LINK_OFFSET(0x1);
	writel(val, pci->dbi_base + PCIE20_LINK_CONTROL_LINK_STATUS_REG);

	msleep(1);

	val = readl(pci->dbi_base + PCIE20_LINK_CONTROL_LINK_STATUS_REG);
	val &= PCIE_CAP_LINK_SPEED_MASK;
	if (PCIE_CAP_LINK_SPEED_SHIFT(val) != target_link_speed) {
		dev_err(pci->dev, "Speed change failed. Current speed 0x%x\n",
			PCIE_CAP_LINK_SPEED_SHIFT(val));
		return -EAGAIN;
	}

	return 0;
}
EXPORT_SYMBOL(pcie_set_link_speed);

int pcie_set_link_width(struct pci_dev *dev, u16 target_link_width)
{
	struct dw_pcie_rp *pp;
	struct dw_pcie *pci;
	struct qcom_pcie *pcie;
	u32 val;
	int ret;

	if (dev->device != QCOM_IPQ9574_DEVICE_ID &&
	    dev->device != QCOM_IPQ5332_DEVICE_ID &&
	    dev->device != QCOM_IPQ5424_DEVICE_ID &&
	    dev->device != QCOM_IPQ9650_DEVICE_ID)
		return -EINVAL;

	if (target_link_width < 1 || target_link_width > 2)
		return -EINVAL;

	pp = dev->bus->sysdata;
	pci = to_dw_pcie_from_pp(pp);
	pcie = to_qcom_pcie(pci);

	dw_pcie_read(pci->dbi_base + PCIE20_PORT_LINK_CTRL_OFF, 4, &val);
	val &= ~PCIE20_PORT_LINK_CTRL_OFF_MASK;
	val |= LINK_CAPABLE_OFFSET(target_link_width);
	dw_pcie_write(pci->dbi_base + PCIE20_PORT_LINK_CTRL_OFF, 4, val);

	dw_pcie_read(pci->dbi_base + PCIE20_LANE_SKEW_OFF, 4, &val);
	val = (val & PCIE20_LANE_SKEW_OFF_MASK) | 0x20;
	dw_pcie_write(pci->dbi_base + PCIE20_LANE_SKEW_OFF, 4, val);

	val = 0xc0 | target_link_width;
	dw_pcie_write(pci->dbi_base + PCIE20_MULTI_LANE_CONTROL_OFF, 4, val);

	msleep(50);

	dw_pcie_read(pci->dbi_base + PCIE20_MULTI_LANE_CONTROL_OFF, 4, &val);
	if((val & 0x40) != 0) {
		dev_err(pci->dev, "LANE_CONTROL_OFF failed: val 0x%x\n", val);
		return -EAGAIN;
	}

	ret = readl_relaxed_poll_timeout(pcie->parf + PARF_LTSSM, val,
					 (val & PCIE20_PARF_LTSSM_MASK) == 0x11,
					 1000, 100000);
	if (ret) {
		dev_err(pci->dev, "After lane switch, link is not in L0: val 0x%x\n", val);
		return -EAGAIN;
	}

	val = readl(pci->dbi_base + PCIE20_LINK_CONTROL_LINK_STATUS_REG);
	val &= PCIE_CAP_NEGO_LINK_WIDTH_MASK;
	if (PCIE_CAP_NEGO_LINK_WIDTH_SHIFT(val) != target_link_width) {
		dev_err(pci->dev, "Lane switch failed: 0x%x\n", val);
		return -EAGAIN;
	}

	return 0;
}
EXPORT_SYMBOL(pcie_set_link_width);

static void qcom_pcie_host_deinit(struct dw_pcie_rp *pp)
{
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct qcom_pcie *pcie = to_qcom_pcie(pci);

	qcom_ep_reset_assert(pcie);
	phy_power_off(pcie->phy);
	pcie->cfg->ops->deinit(pcie);
}

int pcie_parf_read(struct pci_dev *dev, u32 offset, u32 *val)
{
	struct dw_pcie_rp *pp;
	struct dw_pcie *pci;
	struct qcom_pcie *pcie;

	if(!dev)
		goto err;

	pp = dev->bus->sysdata;
	pci = to_dw_pcie_from_pp(pp);
	pcie = to_qcom_pcie(pci);

	if (offset > pcie->parf_size ||
			!IS_ALIGNED((uintptr_t)pcie->parf + offset, 4))
		goto err;

	*val = readl(pcie->parf + offset);

	return 0;

err:
	*val = 0;
	return -EINVAL;
}
EXPORT_SYMBOL(pcie_parf_read);

static const struct dw_pcie_host_ops qcom_pcie_dw_ops = {
	.host_init	= qcom_pcie_host_init,
	.host_deinit	= qcom_pcie_host_deinit,
};

/* Qcom IP rev.: 2.1.0	Synopsys IP rev.: 4.01a */
static const struct qcom_pcie_ops ops_2_1_0 = {
	.get_resources = qcom_pcie_get_resources_2_1_0,
	.init = qcom_pcie_init_2_1_0,
	.post_init = qcom_pcie_post_init_2_1_0,
	.deinit = qcom_pcie_deinit_2_1_0,
	.ltssm_enable = qcom_pcie_2_1_0_ltssm_enable,
};

/* Qcom IP rev.: 1.0.0	Synopsys IP rev.: 4.11a */
static const struct qcom_pcie_ops ops_1_0_0 = {
	.get_resources = qcom_pcie_get_resources_1_0_0,
	.init = qcom_pcie_init_1_0_0,
	.post_init = qcom_pcie_post_init_1_0_0,
	.deinit = qcom_pcie_deinit_1_0_0,
	.ltssm_enable = qcom_pcie_2_1_0_ltssm_enable,
};

/* Qcom IP rev.: 2.3.2	Synopsys IP rev.: 4.21a */
static const struct qcom_pcie_ops ops_2_3_2 = {
	.get_resources = qcom_pcie_get_resources_2_3_2,
	.init = qcom_pcie_init_2_3_2,
	.post_init = qcom_pcie_post_init_2_3_2,
	.deinit = qcom_pcie_deinit_2_3_2,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
};

/* Qcom IP rev.: 2.4.0	Synopsys IP rev.: 4.20a */
static const struct qcom_pcie_ops ops_2_4_0 = {
	.get_resources = qcom_pcie_get_resources_2_4_0,
	.init = qcom_pcie_init_2_4_0,
	.post_init = qcom_pcie_post_init_2_3_2,
	.deinit = qcom_pcie_deinit_2_4_0,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
};

/* Qcom IP rev.: 2.3.3	Synopsys IP rev.: 4.30a */
static const struct qcom_pcie_ops ops_2_3_3 = {
	.get_resources = qcom_pcie_get_resources_2_3_3,
	.init = qcom_pcie_init_2_3_3,
	.post_init = qcom_pcie_post_init_2_3_3,
	.deinit = qcom_pcie_deinit_2_3_3,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
};

/* Qcom IP rev.: 2.7.0	Synopsys IP rev.: 4.30a */
static const struct qcom_pcie_ops ops_2_7_0 = {
	.get_resources = qcom_pcie_get_resources_2_7_0,
	.init = qcom_pcie_init_2_7_0,
	.post_init = qcom_pcie_post_init_2_7_0,
	.deinit = qcom_pcie_deinit_2_7_0,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
};

/* Qcom IP rev.: 1.9.0 */
static const struct qcom_pcie_ops ops_1_9_0 = {
	.get_resources = qcom_pcie_get_resources_2_7_0,
	.init = qcom_pcie_init_2_7_0,
	.post_init = qcom_pcie_post_init_2_7_0,
	.deinit = qcom_pcie_deinit_2_7_0,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
	.config_sid = qcom_pcie_config_sid_1_9_0,
};

/* Qcom IP rev.: 2.9.0  Synopsys IP rev.: 5.00a */
static const struct qcom_pcie_ops ops_2_9_0 = {
	.get_resources = qcom_pcie_get_resources_2_9_0,
	.init = qcom_pcie_init_2_9_0,
	.post_init = qcom_pcie_post_init_2_9_0,
	.deinit = qcom_pcie_deinit_2_9_0,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
};

/* Qcom IP rev.: 1.27.0  Synopsys IP rev.: 5.80a */
static const struct qcom_pcie_ops ops_1_27_0 = {
	.get_resources = qcom_pcie_get_resources_2_9_0,
	.init = qcom_pcie_init_2_9_0,
	.post_init = qcom_pcie_post_init_1_27_0,
	.deinit = qcom_pcie_deinit_2_9_0,
	.ltssm_enable = qcom_pcie_2_3_2_ltssm_enable,
	.reset = qcom_pcie_reset_2_9_0,
};

static const struct qcom_pcie_cfg cfg_1_0_0 = {
	.ops = &ops_1_0_0,
};

static const struct qcom_pcie_cfg cfg_1_9_0 = {
	.ops = &ops_1_9_0,
};

static const struct qcom_pcie_cfg cfg_2_1_0 = {
	.ops = &ops_2_1_0,
};

static const struct qcom_pcie_cfg cfg_2_3_2 = {
	.ops = &ops_2_3_2,
};

static const struct qcom_pcie_cfg cfg_2_3_3 = {
	.ops = &ops_2_3_3,
};

static const struct qcom_pcie_cfg cfg_2_4_0 = {
	.ops = &ops_2_4_0,
};

static const struct qcom_pcie_cfg cfg_2_7_0 = {
	.ops = &ops_2_7_0,
};

static const struct qcom_pcie_cfg cfg_2_9_0 = {
	.ops = &ops_2_9_0,
};

static const struct qcom_pcie_cfg cfg_1_27_0 = {
	.ops = &ops_1_27_0,
};

static const struct dw_pcie_ops dw_pcie_ops = {
	.link_up = qcom_pcie_link_up,
	.start_link = qcom_pcie_start_link,
};

static int qcom_pcie_icc_init(struct qcom_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	int ret;

	pcie->icc_mem = devm_of_icc_get(pci->dev, "pcie-mem");
	if (IS_ERR(pcie->icc_mem))
		return PTR_ERR(pcie->icc_mem);

	/*
	 * Some Qualcomm platforms require interconnect bandwidth constraints
	 * to be set before enabling interconnect clocks.
	 *
	 * Set an initial peak bandwidth corresponding to single-lane Gen 1
	 * for the pcie-mem path.
	 */
	ret = icc_set_bw(pcie->icc_mem, 0, MBps_to_icc(250));
	if (ret) {
		dev_err(pci->dev, "failed to set interconnect bandwidth: %d\n",
			ret);
		return ret;
	}

	return 0;
}

static void qcom_pcie_icc_update(struct qcom_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	u32 offset, status, bw;
	int speed, width;
	int ret;

	if (!pcie->icc_mem)
		return;

	offset = dw_pcie_find_capability(pci, PCI_CAP_ID_EXP);
	status = readw(pci->dbi_base + offset + PCI_EXP_LNKSTA);

	/* Only update constraints if link is up. */
	if (!(status & PCI_EXP_LNKSTA_DLLLA))
		return;

	speed = FIELD_GET(PCI_EXP_LNKSTA_CLS, status);
	width = FIELD_GET(PCI_EXP_LNKSTA_NLW, status);

	switch (speed) {
	case 1:
		bw = MBps_to_icc(250);
		break;
	case 2:
		bw = MBps_to_icc(500);
		break;
	default:
		WARN_ON_ONCE(1);
		fallthrough;
	case 3:
		bw = MBps_to_icc(985);
		break;
	}

	ret = icc_set_bw(pcie->icc_mem, 0, width * bw);
	if (ret) {
		dev_err(pci->dev, "failed to set interconnect bandwidth: %d\n",
			ret);
	}
}

static int qcom_pcie_link_transition_count(struct seq_file *s, void *data)
{
	struct qcom_pcie *pcie = (struct qcom_pcie *)dev_get_drvdata(s->private);

	seq_printf(s, "L0s transition count: %u\n",
		   readl_relaxed(pcie->mhi + PARF_DEBUG_CNT_PM_LINKST_IN_L0S));

	seq_printf(s, "L1 transition count: %u\n",
		   readl_relaxed(pcie->mhi + PARF_DEBUG_CNT_PM_LINKST_IN_L1));

	seq_printf(s, "L1.1 transition count: %u\n",
		   readl_relaxed(pcie->mhi + PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L1));

	seq_printf(s, "L1.2 transition count: %u\n",
		   readl_relaxed(pcie->mhi + PARF_DEBUG_CNT_AUX_CLK_IN_L1SUB_L2));

	seq_printf(s, "L2 transition count: %u\n",
		   readl_relaxed(pcie->mhi + PARF_DEBUG_CNT_PM_LINKST_IN_L2));

	return 0;
}

static void qcom_pcie_init_debugfs(struct qcom_pcie *pcie)
{
	struct dw_pcie *pci = pcie->pci;
	struct device *dev = pci->dev;
	char *name;

	name = devm_kasprintf(dev, GFP_KERNEL, "%pOFP", dev->of_node);
	if (!name)
		return;

	pcie->debugfs = debugfs_create_dir(name, NULL);
	debugfs_create_devm_seqfile(dev, "link_transition_count", pcie->debugfs,
				    qcom_pcie_link_transition_count);
}

static int qcom_pcie_reset_slot(struct pci_host_bridge *bridge,
				struct pci_dev *pdev)
{
	struct pci_bus *bus = bridge->bus;
	struct dw_pcie_rp *pp = bus->sysdata;
	struct dw_pcie *pci = to_dw_pcie_from_pp(pp);
	struct qcom_pcie *pcie = to_qcom_pcie(pci);
	struct device *dev = pcie->pci->dev;
	u32 val;
	int ret;

	/* assert perst to move to RDDM state */
	qcom_ep_reset_assert(pcie);

	/* Wait for wake IRQ event or WAKE_EVENT_MSEC_DELAY timeout */
	wait_for_completion_timeout(&pcie->wake_event, msecs_to_jiffies(WAKE_EVENT_MSEC_DELAY));

	/* Reset completion for next use */
	reinit_completion(&pcie->wake_event);

	/* Wait for the pending transactions to be completed */
	ret = readl_relaxed_poll_timeout(pcie->parf + PARF_STATUS, val,
					 val & FLUSH_COMPLETED, 10,
					 FLUSH_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "Flush completion failed: %d\n", ret);
		goto err_host_deinit;
	}

	/* Clear the FLUSH_MODE to allow the core to be reset */
	val = readl(pcie->parf + PARF_LTSSM);
	val |= SW_CLEAR_FLUSH_MODE;
	writel(val, pcie->parf + PARF_LTSSM);

	/* Wait for the FLUSH_MODE to clear */
	ret = readl_relaxed_poll_timeout(pcie->parf + PARF_LTSSM, val,
					 !(val & FLUSH_MODE), 10,
					 FLUSH_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "Flush mode clear failed: %d\n", ret);
		goto err_host_deinit;
	}

	qcom_pcie_host_deinit(pp);

	/* De-assert perst gpio */
	qcom_ep_reset_deassert(pcie);

	ret = qcom_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "Host init failed\n");
		return ret;
	}

	ret = dw_pcie_setup_rc(pp);
	if (ret)
		goto err_host_deinit;

	/*
	 * Re-enable global IRQ events as the PARF_INT_ALL_MASK register is
	 * non-sticky.
	 */
	if (pcie->global_irq)
		writel_relaxed(PARF_INT_ALL_LINK_UP | PARF_INT_ALL_LINK_DOWN,
			       pcie->parf + PARF_INT_ALL_MASK);

	qcom_pcie_start_link(pci);
	dw_pcie_wait_for_link(pci);

	dev_dbg(dev, "Slot reset completed\n");

	return 0;

err_host_deinit:
	qcom_pcie_host_deinit(pp);

	return ret;
}

int pcie_rescan(int val)
{
	int ret;
	struct dw_pcie_rp *pp;
	struct qcom_pcie *pcie;
	struct qcom_pcie_info *pcie_info, *tmp;

	if (!list_empty(&qcom_pcie_list)) {
		list_for_each_entry_safe(pcie_info, tmp, &qcom_pcie_list, node) {
			pcie = pcie_info->pcie;
			if ((pcie) && (pcie->rc_idx == val)) {
				if (pcie->enumerated) {
					pr_notice("PCIe: RC%d already enumerated\n", val);
				} else {
					pp = &(pcie->pci->pp);
					if (pp->ops->host_init) {
						ret = pp->ops->host_init(pp);
						if (ret)
							return ret;
					}

					dw_pcie_setup_rc(pp);

					ret = qcom_pcie_establish_link(pcie);
					if (ret)
						return ret;

					ret = pci_host_probe(pp->bridge);
					if (ret) {
						pr_err("PCIe rescan failed\n");
						return ret;
					}

					pcie->enumerated = true;
				}
			}
		}
	}

	return 0;
}

static ssize_t slot_rescan_store(const struct bus_type *bus, const char *buf,
			      size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val) < 0)
		return -EINVAL;

	pci_lock_rescan_remove();
	pr_debug("slot rescan from sysfs\n");
	pcie_rescan(val);
	pci_unlock_rescan_remove();

	return count;
}
static BUS_ATTR_WO(slot_rescan);

void pcie_remove_bus(int val)
{
	struct dw_pcie_rp *pp;
	struct qcom_pcie *pcie;
	struct qcom_pcie_info *pcie_info, *tmp;
	struct dw_pcie *pci;

	if (!list_empty(&qcom_pcie_list)) {
		list_for_each_entry_safe_reverse(pcie_info, tmp, &qcom_pcie_list, node) {
			pcie = pcie_info->pcie;
			if ((pcie) && (pcie->rc_idx == val)) {
				if (!pcie->enumerated) {
					pr_notice("\nPCIe: RC%d already removed\n", val);
				} else {
					pr_notice("---> Removing %d\n", val);
					pp = &(pcie->pci->pp);
					pci = pcie->pci;

					pci_stop_root_bus(pp->bridge->bus);
					pci_remove_root_bus(pp->bridge->bus);
					if (pp->ops->host_deinit)
						pp->ops->host_deinit(pp);

					pcie->enumerated = false;
					pr_notice(" ... done<---\n");
				}
			}
		}
	}
}

static ssize_t slot_remove_store(const struct bus_type *bus, const char *buf,
			      size_t count)
{
	unsigned long val;

	if (kstrtoul(buf, 0, &val) < 0)
		return -EINVAL;

	pci_lock_rescan_remove();
	pr_debug("slot remove from sysfs\n");
	pcie_remove_bus(val);
	pci_unlock_rescan_remove();

	return count;
}
static BUS_ATTR_WO(slot_remove);

/*
 * qcom_pcie_rescan() / qcom_pcie_remove_bus()
 *
 * Public API for external drivers to trigger a PCIe rescan or
 * bus removal by PCI domain number.  Since rc_idx == domain (set at probe
 * time: pcie->rc_idx = pcie->domain), these simply delegate to the existing
 * pcie_rescan()/pcie_remove_bus() helpers under the rescan/remove lock.
 */
int qcom_pcie_rescan(int domain)
{
	int ret;

	pci_lock_rescan_remove();
	ret = pcie_rescan(domain);
	pci_unlock_rescan_remove();

	return ret;
}
EXPORT_SYMBOL(qcom_pcie_rescan);

void qcom_pcie_remove_bus(int domain)
{
	pci_lock_rescan_remove();
	pcie_remove_bus(domain);
	pci_unlock_rescan_remove();
}
EXPORT_SYMBOL(qcom_pcie_remove_bus);

static int __qcom_pcie_probe(struct platform_device *pdev,
			     bool reset_before_init)
{
	const struct qcom_pcie_cfg *pcie_cfg;
	struct qcom_pcie_info *pcie_info;
	struct device *dev = &pdev->dev;
	struct qcom_pcie *pcie;
	struct dw_pcie_rp *pp;
	struct resource *res;
	struct dw_pcie *pci;
	static int rc_idx;
	int ret;

	pcie_cfg = of_device_get_match_data(dev);
	if (!pcie_cfg || !pcie_cfg->ops) {
		dev_err(dev, "Invalid platform data\n");
		return -EINVAL;
	}

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pci = devm_kzalloc(dev, sizeof(*pci), GFP_KERNEL);
	if (!pci)
		return -ENOMEM;

	pm_runtime_enable(dev);
	ret = pm_runtime_get_sync(dev);
	if (ret < 0)
		goto err_pm_runtime_put;

	pci->dev = dev;
	pci->ops = &dw_pcie_ops;
	pp = &pci->pp;

	pcie->pci = pci;

	pcie->cfg = pcie_cfg;

	pcie->reset = devm_gpiod_get_optional(dev, "perst", GPIOD_OUT_HIGH);
	if (IS_ERR(pcie->reset)) {
		ret = PTR_ERR(pcie->reset);
		goto err_pm_runtime_put;
	}

	pcie->parf = devm_platform_ioremap_resource_byname(pdev, "parf");
	if (IS_ERR(pcie->parf)) {
		ret = PTR_ERR(pcie->parf);
		goto err_pm_runtime_put;
	}

	/* get the parf size which is needed for pcie_parf_read() */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "parf");
	if (res)
		pcie->parf_size = resource_size(res);

	pcie->elbi = devm_platform_ioremap_resource_byname(pdev, "elbi");
	if (IS_ERR(pcie->elbi)) {
		ret = PTR_ERR(pcie->elbi);
		goto err_pm_runtime_put;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "aggr_noc");
	if (res != NULL) {
		pcie->aggr_noc = devm_ioremap_resource(dev, res);
		if (IS_ERR(pcie->aggr_noc)) {
			ret = PTR_ERR(pcie->aggr_noc);
			goto err_pm_runtime_put;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "system_noc");
	if (res != NULL) {
		pcie->system_noc = devm_ioremap_resource(dev, res);
		if (IS_ERR(pcie->system_noc)) {
			ret = PTR_ERR(pcie->system_noc);
			goto err_pm_runtime_put;
		}
	}

	of_property_read_u32(pdev->dev.of_node, "aggr-noc-val",
				&pcie->aggr_noc_rate_adap_val);

	of_property_read_u32(pdev->dev.of_node, "axi-halt-val",
				&pcie->axi_wr_addr_halt);

	of_property_read_u32(pdev->dev.of_node, "system-noc-val",
				&pcie->system_noc_rate_adap_val);

	of_property_read_u32(pdev->dev.of_node, "linux,pci-domain",&pcie->domain);

	pcie->enable_vc = of_property_read_bool(pdev->dev.of_node,
					"enable-virtual-channel");

	pcie->enable_iatu = of_property_read_bool(pdev->dev.of_node,
						  "enable-iatu");

	pcie->is_emulation = of_property_read_bool(pdev->dev.of_node,
						   "qcom,emulation");

	of_property_read_u32(pdev->dev.of_node, "link_retries_count",
			     &pci->link_retries_count);
	if (pci->link_retries_count)
		dev_info(dev, "pcie link_retries_count set to %d",
			 pci->link_retries_count);

	if (pcie->enable_iatu) {
		of_property_read_u32_array(pdev->dev.of_node,
					   "iatu-ib0-base-addr-cfg",
					   pcie->iatu_ib0_base_addr,
					   sizeof(pcie->iatu_ib0_base_addr) /
					   sizeof(u32));

		of_property_read_u32_array(pdev->dev.of_node,
					   "iatu-ib0-target-addr-cfg",
					   pcie->iatu_ib0_target_addr,
					   sizeof(pcie->iatu_ib0_target_addr) /
					   sizeof(u32));
	};

	/* MHI region is optional */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mhi");
	if (res) {
		pcie->mhi = devm_ioremap_resource(dev, res);
		if (IS_ERR(pcie->mhi)) {
			ret = PTR_ERR(pcie->mhi);
			goto err_pm_runtime_put;
		}
	}

	pcie->phy = devm_phy_optional_get(dev, "pciephy");
	if (IS_ERR(pcie->phy)) {
		ret = PTR_ERR(pcie->phy);
		goto err_pm_runtime_put;
	}

	ret = qcom_pcie_icc_init(pcie);
	if (ret)
		goto err_pm_runtime_put;

	ret = pcie->cfg->ops->get_resources(pcie);
	if (ret)
		goto err_pm_runtime_put;

	pp->ops = &qcom_pcie_dw_ops;

	/* reset_before_init is set if host and phy were initialized by
	 * the bootloader and requires a reset before initialization here.
	 * This is to avoid causing the clocks to go a bad state.
	 */
	if (reset_before_init && pcie->cfg->ops->reset) {
		qcom_ep_reset_assert(pcie);
		msleep(50);
		pcie->cfg->ops->reset(pcie);
		ret = phy_reset(pcie->phy);
		if (ret) {
			dev_err(pci->dev, "Failed to reset phy: %d\n", ret);
			goto err_pm_runtime_put;
		}
	}

	ret = phy_init(pcie->phy);
	if (ret)
		goto err_pm_runtime_put;

	platform_set_drvdata(pdev, pcie);

	ret = dw_pcie_host_init(pp);
	if (ret) {
		dev_err(dev, "cannot initialize host\n");
		goto err_phy_exit;
	}

	pcie->enumerated = true;

	qcom_pcie_icc_update(pcie);

	if (pcie->mhi)
		qcom_pcie_init_debugfs(pcie);

	pcie->global_irq = platform_get_irq_byname(pdev, "global_irq");
	if (pcie->global_irq >= 0) {
		ret = devm_request_threaded_irq(&pdev->dev, pcie->global_irq,
					NULL,
					qcom_pcie_global_irq_thread_fn,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					"pcie-global", pcie);
		if (ret) {
			dev_err(&pdev->dev, "Unable to request global irq\n");
			pm_runtime_disable(&pdev->dev);
			goto err_phy_exit;
		}
	}

	if (pci_aer_available()) {
		init_completion(&pcie->wake_event);
		pcie->wake_irq = platform_get_irq_byname_optional(pdev, "wake_irq");
		if (pcie->wake_irq >= 0) {
			ret = devm_request_irq(&pdev->dev, pcie->wake_irq,
					       qcom_pcie_wake_irq_handler,
					       IRQF_TRIGGER_FALLING,
					       "pcie-wake", pcie);
			if (ret) {
				dev_err(&pdev->dev, "Unable to request wake irq\n");
				pm_runtime_disable(&pdev->dev);
				goto err_phy_exit;
			}
		}
	}

	if (!rc_idx) {
		ret = bus_create_file(&pci_bus_type, &bus_attr_slot_rescan);
		if (ret != 0) {
			dev_err(&pdev->dev, "Failed to create sysfs slot rescan file\n");
			return ret;
		}

		ret = bus_create_file(&pci_bus_type, &bus_attr_slot_remove);
		if (ret != 0) {
			dev_err(&pdev->dev, "Failed to create sysfs slot remove file\n");
			return ret;
		}
	}

	pcie->rc_idx = pcie->domain;
	rc_idx++;

	/* Add qcom_pcie pointer to the list */
	pcie_info = kzalloc(sizeof(struct qcom_pcie_info), GFP_KERNEL);
	if (pcie_info) {
		pcie_info->pcie = pcie;
		list_add_tail(&pcie_info->node, &qcom_pcie_list);
	}

	return 0;

err_phy_exit:
	phy_exit(pcie->phy);
err_pm_runtime_put:
	pm_runtime_put(dev);
	pm_runtime_disable(dev);

	return ret;
}

static int qcom_pcie_deferred_probe(void *data)
{
	struct device_node *rp_node, *ep_node, *rmem_node;
	struct platform_device *pdev = data;
	u32 retry = 0, val, cal_status_off;
	struct reserved_mem *rmem = NULL;
	struct device *dev = &pdev->dev;
	void __iomem *cal_status;

	rp_node = of_get_next_child(dev->of_node, NULL);
	if (rp_node) {
		ep_node = of_get_next_child(rp_node, NULL);
		of_node_put(rp_node);
		if (ep_node) {
			rmem_node = of_parse_phandle(ep_node, "memory-region", 0);
			of_node_put(ep_node);
			if (rmem_node) {
				rmem = of_reserved_mem_lookup(rmem_node);
				of_node_put(rmem_node);
			}
		}
	}

	if (!rmem) {
		dev_err(dev, "Failed to get reserved mem for cal status\n");
		return -ENOMEM;
	}

	if (of_property_read_u32(dev->of_node, "cal-status-offset",
				 &cal_status_off)) {
		dev_err(dev, "Failed to get cal-status-offset from devicetree\n");
		return -EINVAL;
	}

	if (rmem->base + cal_status_off + sizeof(u32) >
					rmem->base + rmem->size - 1) {
		dev_err(dev, "Invalid cal-status-offset: 0x%x rmem base: 0x%pa size: %pa\n",
			cal_status_off, &rmem->base, &rmem->size);
		return -EINVAL;
	}

	cal_status = devm_ioremap(dev, rmem->base + cal_status_off, sizeof(u32));
	if (IS_ERR(cal_status)) {
		dev_err(dev, "Failed to ioremap cal_status memory\n");
		return PTR_ERR(cal_status);
	}

	while (retry < (CAL_COMPLETE_TIMEOUT_SECS * 10)) {
		val = readl(cal_status);
		if (val == CAL_COMPLETE_MAGIC || val == CAL_PANIC_MAGIC)
			break;
		msleep(100);
		retry++;
	}

	devm_iounmap(dev, cal_status);

	if (val == CAL_COMPLETE_MAGIC) {
		dev_info(&pdev->dev, "Calibration completed, took %ds\n",
			 retry / 10);
		/* After the EP completes cal, wait here for it to complete
		 * reset. Default is 1000ms
		 */
		if (cal_reset_wait_ms)
			msleep(cal_reset_wait_ms);
		dev_info(&pdev->dev, "Starting Enumeration\n");
		return __qcom_pcie_probe(pdev, true);
	}

	if (paniconcaltimeout) {
		if (val == CAL_PANIC_MAGIC)
			panic("Calibration failed for %s\n", pdev->name);
		else
			panic("Calibration timedout for %s\n", pdev->name);
	} else {
		if (val == CAL_PANIC_MAGIC)
			dev_err(&pdev->dev, "Calibration failed for %s\n", pdev->name);
		else
			dev_err(&pdev->dev, "Calibration timedout for %s\n", pdev->name);
	}

	return -EINVAL;
}

static int qcom_pcie_probe(struct platform_device *pdev)
{
	struct device_node *rp_node, *ep_node;
	struct device *dev = &pdev->dev;
	bool early_cal_support = false;
	struct nvmem_cell *pcie_nvmem;
	u8 *disable_status;
	int ret = 0;
	size_t len;

	/* If nvmem-cells present on PCIe node in DTSI, then check the QFPROM
	 * fuses for PCIe is disabled
	 */
	pcie_nvmem = devm_nvmem_cell_get(dev, NULL);
	if (IS_ERR(pcie_nvmem)) {
		if (PTR_ERR(pcie_nvmem) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
	} else {
		disable_status = nvmem_cell_read(pcie_nvmem, &len);
		devm_nvmem_cell_put(dev, pcie_nvmem);
		if (!IS_ERR(disable_status) && ((unsigned int)(*disable_status) == 1)) {
			dev_info(dev, "Disabled in qfprom efuse\n");
			kfree(disable_status);
			return -ENODEV;
		}
		kfree(disable_status);
	}

	rp_node = of_get_next_child(dev->of_node, NULL);
	if (rp_node) {
		ep_node = of_get_next_child(rp_node, NULL);
		of_node_put(rp_node);
		if (ep_node) {
			if (of_property_match_string(ep_node,
						     "qcom,early_cal_enabled",
						     "okay") >= 0)
				early_cal_support = true;
			of_node_put(ep_node);
		}
	}

	if (early_cal_support) {
		dev_info(dev, "Starting kthread for %s", pdev->name);
		kthread_run(qcom_pcie_deferred_probe, pdev,
			    "deferred_probe_%s", pdev->name);
	} else {
		dev_info(dev, "Early Cal not supported\n");
		ret =  __qcom_pcie_probe(pdev, false);
	}

	return ret;
}

static int qcom_pcie_suspend_noirq(struct device *dev)
{
	struct qcom_pcie *pcie = dev_get_drvdata(dev);
	int ret;

	/*
	 * Set minimum bandwidth required to keep data path functional during
	 * suspend.
	 */
	ret = icc_set_bw(pcie->icc_mem, 0, kBps_to_icc(1));
	if (ret) {
		dev_err(dev, "Failed to set interconnect bandwidth: %d\n", ret);
		return ret;
	}

	/*
	 * Turn OFF the resources only for controllers without active PCIe
	 * devices. For controllers with active devices, the resources are kept
	 * ON and the link is expected to be in L0/L1 (sub)states.
	 *
	 * Turning OFF the resources for controllers with active PCIe devices
	 * will trigger access violation during the end of the suspend cycle,
	 * as kernel tries to access the PCIe devices config space for masking
	 * MSIs.
	 *
	 * Also, it is not desirable to put the link into L2/L3 state as that
	 * implies VDD supply will be removed and the devices may go into
	 * powerdown state. This will affect the lifetime of the storage devices
	 * like NVMe.
	 */
	if (!dw_pcie_link_up(pcie->pci)) {
		qcom_pcie_host_deinit(&pcie->pci->pp);
		pcie->suspended = true;
	}

	return 0;
}

static int qcom_pcie_resume_noirq(struct device *dev)
{
	struct qcom_pcie *pcie = dev_get_drvdata(dev);
	int ret;

	if (pcie->suspended) {
		ret = qcom_pcie_host_init(&pcie->pci->pp);
		if (ret)
			return ret;

		pcie->suspended = false;
	}

	qcom_pcie_icc_update(pcie);

	return 0;
}

static const struct of_device_id qcom_pcie_match[] = {
	{ .compatible = "qcom,pcie-apq8064", .data = &cfg_2_1_0 },
	{ .compatible = "qcom,pcie-apq8084", .data = &cfg_1_0_0 },
	{ .compatible = "qcom,pcie-ipq4019", .data = &cfg_2_4_0 },
	{ .compatible = "qti,pcie-ipq5332", .data = &cfg_1_27_0 },
	{ .compatible = "qcom,pcie-ipq5424", .data = &cfg_1_27_0 },
	{ .compatible = "qcom,pcie-ipq5210", .data = &cfg_1_27_0 },
	{ .compatible = "qcom,pcie-ipq6018", .data = &cfg_2_9_0 },
	{ .compatible = "qcom,pcie-ipq8064", .data = &cfg_2_1_0 },
	{ .compatible = "qcom,pcie-ipq8064-v2", .data = &cfg_2_1_0 },
	{ .compatible = "qcom,pcie-ipq8074", .data = &cfg_2_3_3 },
	{ .compatible = "qcom,pcie-ipq8074-gen3", .data = &cfg_2_9_0 },
	{ .compatible = "qti,pcie-ipq9574", .data = &cfg_1_27_0 },
	{ .compatible = "qcom,pcie-ipq9650", .data = &cfg_1_27_0 },
	{ .compatible = "qcom,pcie-msm8996", .data = &cfg_2_3_2 },
	{ .compatible = "qcom,pcie-qcs404", .data = &cfg_2_4_0 },
	{ .compatible = "qcom,pcie-sa8540p", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sa8775p", .data = &cfg_1_9_0},
	{ .compatible = "qcom,pcie-sc7280", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sc8180x", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sc8280xp", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sdm845", .data = &cfg_2_7_0 },
	{ .compatible = "qcom,pcie-sdx55", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sm8150", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sm8250", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sm8350", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sm8450-pcie0", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sm8450-pcie1", .data = &cfg_1_9_0 },
	{ .compatible = "qcom,pcie-sm8550", .data = &cfg_1_9_0 },
	{ }
};

static void qcom_fixup_class(struct pci_dev *dev)
{
	dev->class = PCI_CLASS_BRIDGE_PCI_NORMAL;
}

static void qcom_ipq_tc_vc_mapping(struct pci_dev *dev)
{
	struct dw_pcie_rp *pp;
	struct dw_pcie *pci;
	struct qcom_pcie *pcie;
	int timeout = 50;
	u32 val;

	pp = dev->bus->sysdata;
	pci = to_dw_pcie_from_pp(pp);
	pcie = to_qcom_pcie(pci);

	if (!pcie->enable_vc)
		return;

	dev_dbg(&dev->dev, "Enabling Virtual channel for 0x%x:0x%x\n",dev->vendor, dev->device);

	/* Read device VC capabilities */
	pci_read_config_dword(dev, PCIE_TYPE0_VC_CAPABILITIES_REG_1, &val);
	if((val & VC_EXT_VC_CNT_MASK) != 0x1) {
		dev_err(&dev->dev,"device 0x%x does not support Virtual Channel\n", dev->device);
		return;
	}

	/* Program Q6 VC */
	pci_read_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC0, &val);
	val &= ~VC_TC_MAP_VC_BIT1_MASK;
	val |= VC_TC_MAP_VC_BIT1_OFFSET(0x0);
	pci_write_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC0, val);

	pci_read_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC1, &val);
	val &= ~VC_TC_MAP_VC_BIT1_MASK;
	val |= VC_TC_MAP_VC_BIT1_OFFSET(0x7F);
	pci_write_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC1, val);

	pci_read_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC1, &val);
	val &= ~VC_ID_VC1_MASK;
	val |= VC_ID_VC1_OFFSET(0x1);
	pci_write_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC1, val);

	pci_read_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC1, &val);
	val &= ~VC_ENABLE_VC1_MASK;
	val |= VC_ENABLE_VC1_OFFSET(0x1);
	pci_write_config_dword(dev, PCIE_TYPE0_RESOURCE_CON_REG_VC1, val);

	/* Program Host VC */
	val = readl(pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC0);
	val &= ~VC_TC_MAP_VC_BIT1_MASK;
	val |= VC_TC_MAP_VC_BIT1_OFFSET(0x0);
	writel(val, pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC0);

	val = readl(pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC1);
	val &= ~VC_TC_MAP_VC_BIT1_MASK;
	val |= VC_TC_MAP_VC_BIT1_OFFSET(0x7F);
	writel(val, pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC1);

	val = readl(pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC1);
	val &= ~VC_ID_VC1_MASK;
	val |= VC_ID_VC1_OFFSET(0x1);
	writel(val, pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC1);

	val = readl(pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC1);
	val &= ~VC_ENABLE_VC1_MASK;
	val |= VC_ENABLE_VC1_OFFSET(0x1);
	writel(val, pci->dbi_base + PCIE_TYPE0_RESOURCE_CON_REG_VC1);

	do {
		/* Poll for negotiation */
		val = readl(pci->dbi_base + PCIE_TYPE0_RESOURCE_STATUS_REG_VC1);
		if(!(val & VC_NEGO_PENDING_VC1_MASK)) {
			dev_info(&dev->dev,"Virtual channel is enabled for 0x%x:0x%x\n",
					dev->vendor, dev->device);
			break;
		}
		timeout--;
	}while(timeout);
}

static void qcom_ipq_switch_lane(struct pci_dev *dev)
{
	struct dw_pcie_rp *pp;
	struct dw_pcie *pci;
	struct qcom_pcie *pcie;
	struct device *devp;
	struct device_node *np;
	u32 val;
	int size = 4;

	pp = dev->bus->sysdata;
	pci = to_dw_pcie_from_pp(pp);
	pcie = to_qcom_pcie(pci);
	devp = pci->dev;
	np = devp->of_node;

	/* Switching dual lane PCIE instances to single lane if force_to_single_lane property is defined in dts */
	if ((of_property_read_bool(np, "force_to_single_lane")) && (pci->num_lanes == 2)) {

		dev_info(devp,"Forcing PCIE to single lane\n");

		/* check if Link is in L0 state */
		dw_pcie_read(pcie->parf + PARF_LTSSM, size, &val);
		if ((val & PCIE20_PARF_LTSSM_MASK) != 0x11)
			dev_info(devp,"Before lane switch: Link is not in L0 state: %u\n",val);

		/* set link width */
		dw_pcie_read(pci->dbi_base + PCIE20_PORT_LINK_CTRL_OFF, size, &val);
		val &= ~(PCIE20_PORT_LINK_CTRL_OFF_MASK);
		val |= 0x10000;
		dw_pcie_write(pci->dbi_base + PCIE20_PORT_LINK_CTRL_OFF, size, val);

		/* config lane skew */
		dw_pcie_read(pci->dbi_base + PCIE20_LANE_SKEW_OFF, size, &val);
		val = (val & PCIE20_LANE_SKEW_OFF_MASK)|0x20;
		dw_pcie_write(pci->dbi_base + PCIE20_LANE_SKEW_OFF, size, val);

		/* set target lane width & direct link width change */
		dw_pcie_read(pci->dbi_base + PCIE20_MULTI_LANE_CONTROL_OFF, size, &val);
		val |= 0xc1;
		dw_pcie_write(pci->dbi_base + PCIE20_MULTI_LANE_CONTROL_OFF, size, val);

		/* wait until the link width change is complete */
		dw_pcie_read(pci->dbi_base + PCIE20_MULTI_LANE_CONTROL_OFF, size, &val);
		mdelay(50);
		dw_pcie_read(pci->dbi_base + PCIE20_MULTI_LANE_CONTROL_OFF, size, &val);

		/* check if Link is in L0 state */
		dw_pcie_read(pcie->parf + PARF_LTSSM, size, &val);
		if ((val & PCIE20_PARF_LTSSM_MASK) != 0x11)
			dev_info(devp,"After lane switch: Link is not in L0 state: %u\n",val);

		dw_pcie_read(pci->dbi_base + PCIE20_LINK_CONTROL_LINK_STATUS_REG, size, &val);

		dev_info(devp,"Link width is: %u\n",(val&0x3F00000)>>20);
	}

}

DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x0101, qcom_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x0104, qcom_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x0106, qcom_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x0107, qcom_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x0302, qcom_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x1000, qcom_fixup_class);
DECLARE_PCI_FIXUP_EARLY(PCI_VENDOR_ID_QCOM, 0x1001, qcom_fixup_class);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_QCOM, 0x1109, qcom_ipq_tc_vc_mapping);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_QCOM, 0x1108, qcom_ipq_switch_lane);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_QCOM, 0x1006, qcom_ipq_switch_lane);
DECLARE_PCI_FIXUP_FINAL(PCI_VENDOR_ID_QCOM, 0x1007, qcom_ipq_switch_lane);

static const struct dev_pm_ops qcom_pcie_pm_ops = {
	NOIRQ_SYSTEM_SLEEP_PM_OPS(qcom_pcie_suspend_noirq, qcom_pcie_resume_noirq)
};

static struct platform_driver qcom_pcie_driver = {
	.probe = qcom_pcie_probe,
	.driver = {
		.name = "qcom-pcie",
		.suppress_bind_attrs = true,
		.of_match_table = qcom_pcie_match,
		.pm = &qcom_pcie_pm_ops,
#ifdef CONFIG_PCIE_ENABLE_ASYNC_PROBE
		.probe_type = PROBE_PREFER_ASYNCHRONOUS,
#endif
	},
};
builtin_platform_driver(qcom_pcie_driver);
