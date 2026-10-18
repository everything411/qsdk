// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/pci.h>

#include "../../pci.h"
#include "pci.h"
#include "../../pcic.h"
#include "../../core.h"
#include "../../hif.h"
#include "../../mhi.h"
#include "hw.h"
#include "dp.h"
#include "hal.h"

#define QCN9074_DEVICE_ID               0x1104

static const struct pci_device_id ath12k_wifi6_pci_id_table[] = {
	{ PCI_VDEVICE(QCOM, QCN9074_DEVICE_ID) },
	{0}
};

MODULE_DEVICE_TABLE(pci, ath12k_wifi6_pci_id_table);

static const struct ath12k_pci_ops ath12k_wifi6_pci_ops_qcn9074 = {
	.wakeup = NULL,
	.release = NULL,
};

static int ath12k_wifi6_pci_probe(struct pci_dev *pdev,
				  const struct pci_device_id *pci_dev)
{
	struct ath12k_pci *ab_pci;
	struct ath12k_base *ab;
	int ret;
	int scan_radio;


	if (!pdev->dev.of_node)
		return -EINVAL;

	if (!of_property_read_u32(pdev->dev.of_node, "qcom,scan_radio", &scan_radio)) {
		if (!scan_radio) {
			dev_err(&pdev->dev,
				"WiFi6 Probe failure: scan radio property is not set\n");
			return -EOPNOTSUPP;
		}
	} else {
		dev_err(&pdev->dev,
			"WiFi6 Probe failure: scan radio property is not present\n");
		return -EOPNOTSUPP;
	}

	ab = pci_get_drvdata(pdev);
	if (!ab)
		return -EINVAL;

	ab_pci = ath12k_pci_priv(ab);
	if (!ab_pci)
		return -EINVAL;

	switch (pci_dev->device) {
	case QCN9074_DEVICE_ID:
		ab_pci->pci_ops = &ath12k_wifi6_pci_ops_qcn9074;
		ab_pci->window_reg_addr = PCIE_WINDOW_REG_ADDRESS;
		ab->hw_rev = ATH12K_HW_QCN9074_HW10;
		ab->msi.config = &ath12k_wifi7_msi_config[ATH12K_MSI_CONFIG_PCI_16];
		ab->static_window_map = true;
		break;
	default:
		dev_err(&pdev->dev, "Unknown WiFi-6 PCI device found: 0x%x\n",
			pci_dev->device);
		return -EOPNOTSUPP;
	}

	ret = ath12k_wifi6_hw_init(ab);
	if (ret) {
		dev_err(&pdev->dev, "WiFi-6 hw_init failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static const struct ath12k_reg_base ath12k_wifi6_reg_base = {
	.umac_base = HAL_SEQ_WCSS_UMAC_OFFSET,
	.ce_reg_base = HAL_CE_WFSS_CE_REG_BASE,
	.window_value_mask = WINDOW_VALUE_MASK,
	.window_static_mask = WINDOW_STATIC_MASK,
	.window_dynamic_mask = WINDOW_DYNAMIC_MASK,
	.ce_window_shift = CE_WINDOW_SHIFT,
	.umac_window_shift = UMAC_WINDOW_SHIFT,
};

static struct ath12k_pci_driver ath12k_wifi6_pci_driver = {
	.name = "ath12k_wifi6_pci",
	.id_table = ath12k_wifi6_pci_id_table,
	.ops.probe = ath12k_wifi6_pci_probe,
	.ops.dp_init = ath12k_wifi6_dp_init,
	.ops.dp_deinit = ath12k_wifi6_dp_deinit,
	.reg_base = &ath12k_wifi6_reg_base,
};

int ath12k_wifi6_pci_init(void)
{
	int ret;

	ret = ath12k_pci_register_driver(ATH12K_DEVICE_FAMILY_WIFI6,
					 &ath12k_wifi6_pci_driver);
	if (ret) {
		pr_err("Failed to register ath12k WiFi-6 driver: %d\n",
		       ret);
		return ret;
	}

	return 0;
}

void ath12k_wifi6_pci_exit(void)
{
	ath12k_pci_unregister_driver(ATH12K_DEVICE_FAMILY_WIFI6);
}
