// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 Sartura Ltd.
 * Copyright (c) 2022 Linaro Ltd.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Author: Robert Marko <robert.marko@sartura.hr>
 *         Sumit Garg <sumit.garg@linaro.org>
 *
 * Based on Linux driver
 */

#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <reset-uclass.h>
#include <linux/bitops.h>
#include <malloc.h>

struct qcom_reset_priv {
	phys_addr_t base;
};

struct qcom_reset_map {
	unsigned int reg;
	u8 bit;
};

#ifdef CONFIG_ARCH_IPQ40XX
#include <dt-bindings/reset/qcom,ipq4019-reset.h>
static const struct qcom_reset_map gcc_qcom_resets[] = {
	[WIFI0_CPU_INIT_RESET] = { 0x1f008, 5 },
	[WIFI0_RADIO_SRIF_RESET] = { 0x1f008, 4 },
	[WIFI0_RADIO_WARM_RESET] = { 0x1f008, 3 },
	[WIFI0_RADIO_COLD_RESET] = { 0x1f008, 2 },
	[WIFI0_CORE_WARM_RESET] = { 0x1f008, 1 },
	[WIFI0_CORE_COLD_RESET] = { 0x1f008, 0 },
	[WIFI1_CPU_INIT_RESET] = { 0x20008, 5 },
	[WIFI1_RADIO_SRIF_RESET] = { 0x20008, 4 },
	[WIFI1_RADIO_WARM_RESET] = { 0x20008, 3 },
	[WIFI1_RADIO_COLD_RESET] = { 0x20008, 2 },
	[WIFI1_CORE_WARM_RESET] = { 0x20008, 1 },
	[WIFI1_CORE_COLD_RESET] = { 0x20008, 0 },
	[USB3_UNIPHY_PHY_ARES] = { 0x1e038, 5 },
	[USB3_HSPHY_POR_ARES] = { 0x1e038, 4 },
	[USB3_HSPHY_S_ARES] = { 0x1e038, 2 },
	[USB2_HSPHY_POR_ARES] = { 0x1e01c, 4 },
	[USB2_HSPHY_S_ARES] = { 0x1e01c, 2 },
	[PCIE_PHY_AHB_ARES] = { 0x1d010, 11 },
	[PCIE_AHB_ARES] = { 0x1d010, 10 },
	[PCIE_PWR_ARES] = { 0x1d010, 9 },
	[PCIE_PIPE_STICKY_ARES] = { 0x1d010, 8 },
	[PCIE_AXI_M_STICKY_ARES] = { 0x1d010, 7 },
	[PCIE_PHY_ARES] = { 0x1d010, 6 },
	[PCIE_PARF_XPU_ARES] = { 0x1d010, 5 },
	[PCIE_AXI_S_XPU_ARES] = { 0x1d010, 4 },
	[PCIE_AXI_M_VMIDMT_ARES] = { 0x1d010, 3 },
	[PCIE_PIPE_ARES] = { 0x1d010, 2 },
	[PCIE_AXI_S_ARES] = { 0x1d010, 1 },
	[PCIE_AXI_M_ARES] = { 0x1d010, 0 },
	[ESS_RESET] = { 0x12008, 0},
	[GCC_BLSP1_BCR] = {0x01000, 0},
	[GCC_BLSP1_QUP1_BCR] = {0x02000, 0},
	[GCC_BLSP1_UART1_BCR] = {0x02038, 0},
	[GCC_BLSP1_QUP2_BCR] = {0x03008, 0},
	[GCC_BLSP1_UART2_BCR] = {0x03028, 0},
	[GCC_BIMC_BCR] = {0x04000, 0},
	[GCC_TLMM_BCR] = {0x05000, 0},
	[GCC_IMEM_BCR] = {0x0E000, 0},
	[GCC_ESS_BCR] = {0x12008, 0},
	[GCC_PRNG_BCR] = {0x13000, 0},
	[GCC_BOOT_ROM_BCR] = {0x13008, 0},
	[GCC_CRYPTO_BCR] = {0x16000, 0},
	[GCC_SDCC1_BCR] = {0x18000, 0},
	[GCC_SEC_CTRL_BCR] = {0x1A000, 0},
	[GCC_AUDIO_BCR] = {0x1B008, 0},
	[GCC_QPIC_BCR] = {0x1C000, 0},
	[GCC_PCIE_BCR] = {0x1D000, 0},
	[GCC_USB2_BCR] = {0x1E008, 0},
	[GCC_USB2_PHY_BCR] = {0x1E018, 0},
	[GCC_USB3_BCR] = {0x1E024, 0},
	[GCC_USB3_PHY_BCR] = {0x1E034, 0},
	[GCC_SYSTEM_NOC_BCR] = {0x21000, 0},
	[GCC_PCNOC_BCR] = {0x2102C, 0},
	[GCC_DCD_BCR] = {0x21038, 0},
	[GCC_SNOC_BUS_TIMEOUT0_BCR] = {0x21064, 0},
	[GCC_SNOC_BUS_TIMEOUT1_BCR] = {0x2106C, 0},
	[GCC_SNOC_BUS_TIMEOUT2_BCR] = {0x21074, 0},
	[GCC_SNOC_BUS_TIMEOUT3_BCR] = {0x2107C, 0},
	[GCC_PCNOC_BUS_TIMEOUT0_BCR] = {0x21084, 0},
	[GCC_PCNOC_BUS_TIMEOUT1_BCR] = {0x2108C, 0},
	[GCC_PCNOC_BUS_TIMEOUT2_BCR] = {0x21094, 0},
	[GCC_PCNOC_BUS_TIMEOUT3_BCR] = {0x2109C, 0},
	[GCC_PCNOC_BUS_TIMEOUT4_BCR] = {0x210A4, 0},
	[GCC_PCNOC_BUS_TIMEOUT5_BCR] = {0x210AC, 0},
	[GCC_PCNOC_BUS_TIMEOUT6_BCR] = {0x210B4, 0},
	[GCC_PCNOC_BUS_TIMEOUT7_BCR] = {0x210BC, 0},
	[GCC_PCNOC_BUS_TIMEOUT8_BCR] = {0x210C4, 0},
	[GCC_PCNOC_BUS_TIMEOUT9_BCR] = {0x210CC, 0},
	[GCC_TCSR_BCR] = {0x22000, 0},
	[GCC_MPM_BCR] = {0x24000, 0},
	[GCC_SPDM_BCR] = {0x25000, 0},
};
#endif

#ifdef CONFIG_TARGET_IPQ5332
#include <dt-bindings/reset/ipq5332-reset.h>
static const struct qcom_reset_map gcc_qcom_resets[] = {
	[GCC_SDCC1_BCR] = {0x33000, 0},
	[GCC_UNIPHY0_BCR] = {0x16000, 0},
	[GCC_UNIPHY1_BCR] = {0x16014, 0},
	[GCC_UNIPHY0_SOFT_RESET] = {0x1600C, 2},
	[GCC_UNIPHY1_SOFT_RESET] = {0x16018, 2},
	[GCC_UNIPHY0_XPCS_RESET] = {0x16050, 0},
	[GCC_UNIPHY1_XPCS_RESET] = {0x16060, 0},
	[NSS_CC_PPE_BCR] = {0x003E4, 0},
	[NSS_CC_PORT1_RX_RESET] = {0x004B4, 2},
	[NSS_CC_PORT1_TX_RESET] = {0x004B8, 2},
	[NSS_CC_PORT2_RX_RESET] = {0x004BC, 2},
	[NSS_CC_PORT2_TX_RESET] = {0x004C0, 2},
	[GCC_PCIE3X1_0_BCR_RESET] = {0x29000, 1},
	[GCC_PCIE3X1_1_BCR_RESET] = {0x2A000, 1},
	[GCC_PCIE3X2_BCR_RESET] = {0x28000, 1},
	[GCC_PCIE3X2_PHY_BCR_RESET] = {0x28060, 1},
	[GCC_PCIE3X1_0_PHY_BCR_RESET] = {0x29060, 1},
	[GCC_PCIE3X1_1_PHY_BCR_RESET] = {0x2A030, 1},
	[GCC_USB_BCR] = {0x2C000, 0},
	[GCC_QUSB2_0_PHY_BCR] = {0x2C068, 0},
	[GCC_USB0_PHY_BCR] = {0x2C06C, 0},
};
#endif

#ifdef CONFIG_TARGET_IPQ9574
#include <dt-bindings/reset/ipq9574-reset.h>
static const struct qcom_reset_map gcc_qcom_resets[] = {
	[GCC_SDCC1_BCR] = {0x33000, 0},
	[GCC_UNIPHY0_SOFT_RESET] = {0x17050, 0},
	[GCC_UNIPHY1_SOFT_RESET] = {0x17060, 0},
	[GCC_UNIPHY2_SOFT_RESET] = {0x17070, 0},
	[GCC_UNIPHY0_XPCS_RESET] = {0x17050, 2},
	[GCC_UNIPHY1_XPCS_RESET] = {0x17060, 2},
	[GCC_UNIPHY2_XPCS_RESET] = {0x17070, 2},
	[NSS_CC_PPE_BTQ_RESET] = {0x28A08, 20},
	[NSS_CC_PPE_IPE_RESET] = {0x28A08, 19},
	[NSS_CC_PPE_RESET] = {0x28A08, 18},
	[NSS_CC_PPE_CFG_RESET] = {0x28A08, 17},
	[NSS_CC_PPE_EDMA_RESET] = {0x28A08, 16},
	[NSS_CC_PPE_EDMA_CFG_RESET] = {0x28A08, 15},
	[NSS_CC_PORT1_MAC_RESET] = {0x28A08, 11},
	[NSS_CC_PORT2_MAC_RESET] = {0x28A08, 10},
	[NSS_CC_PORT3_MAC_RESET] = {0x28A08, 9},
	[NSS_CC_PORT4_MAC_RESET] = {0x28A08, 8},
	[NSS_CC_PORT5_MAC_RESET] = {0x28A08, 7},
	[NSS_CC_PORT6_MAC_RESET] = {0x28A08, 6},
	[NSS_CC_UNIPHY_PORT1_RX_RESET] = {0x28A24, 23},
	[NSS_CC_UNIPHY_PORT1_TX_RESET] = {0x28A24, 22},
	[NSS_CC_UNIPHY_PORT2_RX_RESET] = {0x28A24, 21},
	[NSS_CC_UNIPHY_PORT2_TX_RESET] = {0x28A24, 20},
	[NSS_CC_UNIPHY_PORT3_RX_RESET] = {0x28A24, 19},
	[NSS_CC_UNIPHY_PORT3_TX_RESET] = {0x28A24, 18},
	[NSS_CC_UNIPHY_PORT4_RX_RESET] = {0x28A24, 17},
	[NSS_CC_UNIPHY_PORT4_TX_RESET] = {0x28A24, 16},
	[NSS_CC_UNIPHY_PORT5_RX_RESET] = {0x28A24, 15},
	[NSS_CC_UNIPHY_PORT5_TX_RESET] = {0x28A24, 14},
	[NSS_CC_UNIPHY_PORT6_RX_RESET] = {0x28A24, 13},
	[NSS_CC_UNIPHY_PORT6_TX_RESET] = {0x28A24, 12},
	[NSS_CC_PORT1_RX_RESET] = {0x28A24, 11},
	[NSS_CC_PORT1_TX_RESET] = {0x28A24, 10},
	[NSS_CC_PORT2_RX_RESET] = {0x28A24, 9},
	[NSS_CC_PORT2_TX_RESET] = {0x28A24, 8},
	[NSS_CC_PORT3_RX_RESET] = {0x28A24, 7},
	[NSS_CC_PORT3_TX_RESET] = {0x28A24, 6},
	[NSS_CC_PORT4_RX_RESET] = {0x28A24, 5},
	[NSS_CC_PORT4_TX_RESET] = {0x28A24, 4},
	[NSS_CC_PORT5_RX_RESET] = {0x28A24, 3},
	[NSS_CC_PORT5_TX_RESET] = {0x28A24, 2},
	[NSS_CC_PORT6_RX_RESET] = {0x28A24, 1},
	[NSS_CC_PORT6_TX_RESET] = {0x28A24, 0},
	[GCC_USB_BCR] = {0x2C000, 0},
	[GCC_QUSB2_0_PHY_BCR] = {0x2C068, 0},
	[GCC_USB0_PHY_BCR] = {0x2C06C, 0},
	[GCC_USB3PHY_0_PHY_BCR] = {0x2C070, 0},
};
#endif

#ifdef CONFIG_TARGET_IPQ5424
#include <dt-bindings/reset/ipq5424-reset.h>
static const struct qcom_reset_map gcc_qcom_resets[] = {
	[GCC_SDCC1_BCR] = {0x33000, 0},
	[GCC_USB_BCR] = {0x2C000, 0},
	[GCC_QUSB2_0_PHY_BCR] = {0x2C068, 0},
	[GCC_USB0_PHY_BCR] = {0x2C06C, 0},
	[GCC_USB3PHY_0_PHY_BCR] = {0x2C070, 0},
	[GCC_USB1_BCR] = {0x3C000, 0},
	[GCC_QUSB2_1_PHY_BCR] = {0x3C030, 0},
	[GCC_PCIE0_PHY_BCR] = {0x28060, 0},
	[GCC_PCIE0PHY_PHY_BCR] = {0x2805C, 0},
	[GCC_PCIE0_BCR] = {0x28000, 0},
	[GCC_PCIE1_BCR] = {0x29000, 0},
	[GCC_PCIE2_BCR] = {0x2A000, 0},
	[GCC_PCIE3_BCR] = {0x2B000, 0},
	[GCC_PCIE1_PHY_BCR] = {0x29060, 0},
	[GCC_PCIE1PHY_PHY_BCR] = {0x2905C, 0},
	[GCC_PCIE2_PHY_BCR] = {0x2A060, 0},
	[GCC_PCIE2PHY_PHY_BCR] = {0x2A05C, 0},
	[GCC_PCIE3_PHY_BCR] = {0x2B060, 0},
	[GCC_PCIE3PHY_PHY_BCR] = {0x2B05C, 0},
	[GCC_UNIPHY0_BCR] = {0x17044, 0},
	[GCC_UNIPHY1_BCR] = {0x17054, 0},
	[GCC_UNIPHY2_BCR] = {0x17054, 0},
	[GCC_UNIPHY0_SOFT_RESET] = {0x17048, 2},
	[GCC_UNIPHY1_SOFT_RESET] = {0x17058, 2},
	[GCC_UNIPHY2_SOFT_RESET] = {0x17068, 2},
	[GCC_UNIPHY0_XPCS_RESET] = {0x17050, 2},
	[GCC_UNIPHY1_XPCS_RESET] = {0x17060, 2},
	[GCC_UNIPHY2_XPCS_RESET] = {0x17070, 2},
	[NSS_CC_PPE_BCR] = {0x003E8, 0},
	[NSS_CC_PORT1_RX_RESET] = {0x004FC, 2},
	[NSS_CC_PORT1_TX_RESET] = {0x00504, 2},
	[NSS_CC_PORT2_RX_RESET] = {0x0050C, 2},
	[NSS_CC_PORT2_TX_RESET] = {0x00514, 2},
	[NSS_CC_PORT3_RX_RESET] = {0x0051C, 2},
	[NSS_CC_PORT3_TX_RESET] = {0x00524, 2},
	[GCC_UNIPHY0_AHB_RESET] = {0x1704C, 2},
	[GCC_UNIPHY1_AHB_RESET] = {0x1705C, 2},
	[GCC_UNIPHY2_AHB_RESET] = {0x1706C, 2},
	[NSS_CC_PORT1_MAC_RESET] = {0x00428, 2},
	[NSS_CC_PORT2_MAC_RESET] = {0x00430, 2},
	[NSS_CC_PORT3_MAC_RESET] = {0x00438, 2},
};
#endif

#ifdef CONFIG_TARGET_QCS404EVB
#include <dt-bindings/clock/qcom,gcc-qcs404.h>
static const struct qcom_reset_map gcc_qcom_resets[] = {
	[GCC_GENI_IR_BCR] = { 0x0F000 },
	[GCC_CDSP_RESTART] = { 0x18000 },
	[GCC_USB_HS_BCR] = { 0x41000 },
	[GCC_USB2_HS_PHY_ONLY_BCR] = { 0x41034 },
	[GCC_QUSB2_PHY_BCR] = { 0x4103c },
	[GCC_USB_HS_PHY_CFG_AHB_BCR] = { 0x0000c, 1 },
	[GCC_USB2A_PHY_BCR] = { 0x0000c, 0 },
	[GCC_USB3_PHY_BCR] = { 0x39004 },
	[GCC_USB_30_BCR] = { 0x39000 },
	[GCC_USB3PHY_PHY_BCR] = { 0x39008 },
	[GCC_PCIE_0_BCR] = { 0x3e000 },
	[GCC_PCIE_0_PHY_BCR] = { 0x3e004 },
	[GCC_PCIE_0_LINK_DOWN_BCR] = { 0x3e038 },
	[GCC_PCIEPHY_0_PHY_BCR] = { 0x3e03c },
	[GCC_PCIE_0_AXI_MASTER_STICKY_ARES] = { 0x3e040, 6},
	[GCC_PCIE_0_AHB_ARES] = { 0x3e040, 5 },
	[GCC_PCIE_0_AXI_SLAVE_ARES] = { 0x3e040, 4 },
	[GCC_PCIE_0_AXI_MASTER_ARES] = { 0x3e040, 3 },
	[GCC_PCIE_0_CORE_STICKY_ARES] = { 0x3e040, 2 },
	[GCC_PCIE_0_SLEEP_ARES] = { 0x3e040, 1 },
	[GCC_PCIE_0_PIPE_ARES] = { 0x3e040, 0 },
	[GCC_EMAC_BCR] = { 0x4e000 },
	[GCC_WDSP_RESTART] = {0x19000},
};
#endif

static int qcom_reset_assert(struct reset_ctl *rst)
{
	struct qcom_reset_priv *priv = dev_get_priv(rst->dev);
	const struct qcom_reset_map *reset_map = gcc_qcom_resets;
	const struct qcom_reset_map *map;
	u32 value;

	map = &reset_map[rst->id];

	value = readl(priv->base + map->reg);
	value |= BIT(map->bit);
	writel(value, priv->base + map->reg);

	return 0;
}

static int qcom_reset_deassert(struct reset_ctl *rst)
{
	struct qcom_reset_priv *priv = dev_get_priv(rst->dev);
	const struct qcom_reset_map *reset_map = gcc_qcom_resets;
	const struct qcom_reset_map *map;
	u32 value;

	map = &reset_map[rst->id];

	value = readl(priv->base + map->reg);
	value &= ~BIT(map->bit);
	writel(value, priv->base + map->reg);

	return 0;
}

static const struct reset_ops qcom_reset_ops = {
	.rst_assert = qcom_reset_assert,
	.rst_deassert = qcom_reset_deassert,
};

static const struct udevice_id qcom_reset_ids[] = {
	{ .compatible = "qcom,gcc-reset-ipq4019" },
	{ .compatible = "qcom,gcc-reset-qcs404" },
	{ .compatible = "qti,gcc-reset-ipqsoc" },
	{ }
};

static int qcom_reset_probe(struct udevice *dev)
{
	struct qcom_reset_priv *priv = dev_get_priv(dev);

	priv->base = dev_read_addr(dev);
	if (priv->base == FDT_ADDR_T_NONE)
		return -EINVAL;

	return 0;
}

U_BOOT_DRIVER(qcom_reset) = {
	.name = "qcom_reset",
	.id = UCLASS_RESET,
	.of_match = qcom_reset_ids,
	.ops = &qcom_reset_ops,
	.probe = qcom_reset_probe,
	.priv_auto = sizeof(struct qcom_reset_priv),
};
