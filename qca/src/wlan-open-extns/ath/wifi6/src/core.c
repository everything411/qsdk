// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>

#include "../../pci.h"
#include "pci.h"
#include "ahb.h"
#ifdef CPTCFG_QCN_EXTN
#include "../../qcn_extns/ini.h"
#endif

static int ahb_err,pci_err;

static int ath12k_wifi6_init(void)
{

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_init();
#endif

	ahb_err = ath12k_wifi6_ahb_init();
	if (ahb_err)
		pr_warn("Failed to initialize ath12k WiFi6 AHB device: %d\n",
			ahb_err);

	pci_err = ath12k_wifi6_pci_init();
	if (pci_err)
		pr_warn("Failed to initialize ath12k WiFi6 PCI device: %d\n",
			pci_err);

	/* If both failed, return one of the failures (arbitrary) */
	return ahb_err && pci_err ? ahb_err : 0;
}

static void ath12k_wifi6_exit(void)
{
	if (!pci_err)
		ath12k_wifi6_pci_exit();

	if (!ahb_err)
		ath12k_wifi6_ahb_exit();

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_deinit();
#endif
}

module_init(ath12k_wifi6_init);
module_exit(ath12k_wifi6_exit);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies 802.11ax WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
