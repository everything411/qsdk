// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/pci.h>

#include "../pci.h"
#include "pci.h"
#include "../pcic.h"
#include "../core.h"
#include "../hif.h"
#include "../mhi.h"
#include "hw.h"
#include "mhi.h"
#include "dp.h"
#include "hal.h"
#include "mgmt_rx.h"

#define ATH12K_PCI_T8_SOC_HW_VERSION_1	1
#define ATH12K_PCI_T8_SOC_HW_VERSION_2	2

#define TCSR_SOC_HW_VERSION		0x1B00000
#define TCSR_SOC_HW_VERSION_MAJOR_MASK	GENMASK(15, 8)
#define TCSR_SOC_HW_VERSION_MINOR_MASK	GENMASK(7, 0)

static const struct pci_device_id ath12k_wifi8_pci_id_table[] = {
	{ PCI_VDEVICE(QCOM, QCN9625_DEVICE_ID) },
	{ PCI_VDEVICE(QCOM, QCN9589_DEVICE_ID) },
	{0}
};

MODULE_DEVICE_TABLE(pci, ath12k_wifi8_pci_id_table);

static void ath12k_wifi8_pci_read_hw_version(struct ath12k_base *ab,
					     u32 *major, u32 *minor)
{
	u32 soc_hw_version;

	soc_hw_version = ath12k_pci_read32(ab, TCSR_SOC_HW_VERSION);
	*major = u32_get_bits(soc_hw_version, TCSR_SOC_HW_VERSION_MAJOR_MASK);
	*minor = u32_get_bits(soc_hw_version, TCSR_SOC_HW_VERSION_MINOR_MASK);

	ath12k_info(ab, "SOC HW version major %u minor %u\n",
		    *major, *minor);
}

static int ath12k_wifi8_pci_probe(struct pci_dev *pdev,
				  const struct pci_device_id *pci_dev)
{
	struct ath12k_base *ab = pci_get_drvdata(pdev);
	u32 soc_hw_version_major, soc_hw_version_minor;
	struct ath12k_pci *ab_pci;
	u32 msi;
	int ret;

	ab = pci_get_drvdata(pdev);
	if (!ab)
		return -EINVAL;

	ab_pci = ath12k_pci_priv(ab);
	if (!ab_pci)
		return -EINVAL;

	switch (pci_dev->device) {
	case QCN9625_DEVICE_ID:
		if (!of_property_read_u32(ab->dev->of_node, "qcom,msi", &msi) &&
		    msi == ATH12K_MSI_16) {
			dev_info(&pdev->dev, "ath12k supported MSI %d\n", msi);
			ab->msi.config =
				&ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_PCI_16];
		} else {
			ab->msi.config = &ath12k_wifi8_msi_config[0];
		}
		ab->static_window_map = true;
		/* window_reg_addr must be initialized before reading HW version */
		ab_pci->window_reg_addr = PCIE_WINDOW_REG_ADDRESS;
		ath12k_wifi8_pci_read_hw_version(ab, &soc_hw_version_major,
						 &soc_hw_version_minor);
		switch (soc_hw_version_major) {
		case ATH12K_PCI_T8_SOC_HW_VERSION_2:
			ab->hw_rev = ATH12K_HW_QCN9625_HW20;
			break;
		case ATH12K_PCI_T8_SOC_HW_VERSION_1:
			ab->hw_rev = ATH12K_HW_QCN9625_HW10;
			break;
		default:
			dev_err(&pdev->dev,
				"Unknown hardware version found for QCN9625: 0x%x\n",
				soc_hw_version_major);
			return -EOPNOTSUPP;
		}
		break;
	case QCN9589_DEVICE_ID:
		if (!of_property_read_u32(ab->dev->of_node, "qcom,msi", &msi) &&
		    msi == ATH12K_MSI_16) {
			dev_info(&pdev->dev, "ath12k supported MSI %d\n", msi);
			ab->msi.config =
				&ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_PCI_16];
		} else {
			ab->msi.config = &ath12k_wifi8_msi_config[0];
		}
		ab->static_window_map = true;
		ab->hw_rev = ATH12K_HW_QCN9589_HW10;
		ab_pci->window_reg_addr = PCIE_WINDOW_REG_ADDRESS;
		break;

	default:
		dev_err(&pdev->dev, "Unknown WiFi-8 PCI device found: 0x%x\n",
			pci_dev->device);
		return -EOPNOTSUPP;
	}

#if defined(ATH12K_CMA_SUPPORT)
	ret = ath12k_memdev_init(ab, &ab->qmi_mem_dev, "host-ddr-mem");
	if (ret)
		ath12k_err(ab,
			   "failed to init qmi mem helper device: %d, falling back to global cma\n",
			   ret);
	if (ab->qmi_mem_dev.rmem_inited)
		clear_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags);
#endif

	ret = ath12k_wifi8_hw_init(ab);
	if (ret) {
		dev_err(&pdev->dev, "WiFi-8 hw_init failed: %d\n", ret);
#if defined(ATH12K_CMA_SUPPORT)
		ath12k_memdev_deinit(ab, &ab->qmi_mem_dev);
#endif
		return ret;
	}

	return 0;
}

void ath12k_wifi8_pci_get_soc_reset_reason(struct ath12k_base *ab)
{
	u32 val = 0;

	val = ath12k_pci_read32(ab, QCN9625_WLAON_SOC_RESET_CAUSE_SHADOW_REG);

	if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) {
		if (val)
			ath12k_info(ab, "Q6 BCR is set without RDDM hence cleared\n");
		return;
	}

	if (ab->is_cumac_chip && (val & QCN9625_RESET_CAUSE_Q6_BCR)) {
		ab->skip_cumac_hw_reset = false;
		set_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags);
	}

	ath12k_info(ab, "Q6 BCR: %s\n",
		    test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags) ? "yes" : "no");
}

static const struct ath12k_reg_base ath12k_wifi8_pci_reg_base = {
	.umac_base = HAL_SEQ_WCSS_UMAC_OFFSET,
	.ce_reg_base = HAL_CE_WFSS_CE_REG_BASE,
	.window_value_mask = WINDOW_VALUE_MASK,
	.window_static_mask = WINDOW_STATIC_MASK,
	.window_dynamic_mask = WINDOW_DYNAMIC_MASK,
	.ce_window_shift = CE_WINDOW_SHIFT,
	.umac_window_shift = UMAC_WINDOW_SHIFT,
};

static struct ath12k_pci_driver ath12k_wifi8_pci_driver = {
	.name = "ath12k_wifi8_pci",
	.id_table = ath12k_wifi8_pci_id_table,
	.ops.probe = ath12k_wifi8_pci_probe,
	.reg_base = &ath12k_wifi8_pci_reg_base,
	.ops.dp_init = ath12k_wifi8_dp_init,
	.ops.dp_deinit = ath12k_wifi8_dp_deinit,
	.ops.mgmt_init = ath12k_wifi8_mgmt_init,
	.ops.mgmt_deinit = ath12k_wifi8_mgmt_deinit,
	.ops.get_reset_reason = ath12k_wifi8_pci_get_soc_reset_reason,
};

int ath12k_wifi8_pci_init(void)
{
	int ret;

	ret = ath12k_pci_register_driver(ATH12K_DEVICE_FAMILY_WIFI8,
					 &ath12k_wifi8_pci_driver);
	if (ret) {
		pr_err("Failed to register ath12k WiFi-8 driver: %d\n",
		       ret);
		return ret;
	}

	return 0;
}

void ath12k_wifi8_pci_exit(void)
{
	ath12k_pci_unregister_driver(ATH12K_DEVICE_FAMILY_WIFI8);
}
