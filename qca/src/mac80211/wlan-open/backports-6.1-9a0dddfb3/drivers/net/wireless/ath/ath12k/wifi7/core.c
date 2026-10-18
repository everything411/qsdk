// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include "../ahb.h"
#include "../pci.h"
#include "../erp.h"
#include "pci.h"
#include "ahb.h"
#include "../vendor_services.h"
#include "../core.h"
#include "../debug.h"
#include "../hal.h"
#include "../hif.h"
#include "../qmi.h"
#include "hw.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ini.h"
#endif

static int ahb_err, pci_err;

/**
 * ath12k_wifi7_wsi_bypass_remove - wifi7 WSI bypass remove handler
 *
 * Called after the common UMAC reset completes when a wifi7 chip is
 * being bypassed.  Suspends all pdevs, stops Q6 firmware, cleans up
 * driver state, updates MLO adjacency, triggers MLO reconfig on the
 * remaining active chips, and puts the PCIe link into low-speed mode
 * before initiating a fresh MLO setup.
 */
static int ath12k_wifi7_wsi_bypass_remove(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret;

	ret = ath12k_core_wsi_mlo_teardown_umac_reset(ab);
	if (ret) {
		ath12k_err(ab, "WSI Bypass: Umac reset failed: %d\n", ret);
		return ret;
	}

	/* Suspend all pdevs on the chip being bypassed */
	ret = ath12k_core_wsi_remap_pdev_suspend(ab);
	if (ret) {
		ath12k_err(ab, "WSI Bypass: pdev suspend failed: %d\n", ret);
		return ret;
	}

	ab->is_bypassed = true;

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Send Mode OFF for Q6");
	ath12k_qmi_firmware_stop(ab);
	ath12k_qmi_free_resource(ab);

	/* Clean up driver state for the bypassed chip */
	ath12k_core_cleanup(ab);

	/* Update hw-group bypass counters */
	ag->num_bypassed++;
	ath12k_core_to_group_ref_put(ab);
	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: num_bypassed: %d num_started: %d",
		   ag->num_bypassed, ag->num_started);

	/* Refresh MLO adjacency info for remaining active chips */
	ath12k_update_mlo_adj_chip(ag);

	mutex_lock(&ag->mutex);
	/* Notify all active chips of the new WSI topology */
	ath12k_core_wsi_remap_mlo_reconfig(ag);

	/* Reduce PCIe link speed while the chip is bypassed */
	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: Configure PCIe link to low speed");
	ath12k_core_pci_link_speed(ab, 1, 1);

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: Initiate MLO setup after bypass");
	ret = ath12k_core_mlo_setup(ag);
	if (ret) {
		ath12k_err(ab, "MLO setup failed %d\n", ret);
		return ret;
	}
	mutex_unlock(&ag->mutex);

	/* Mark bypass complete; keep wsi_remap_state as REMOVE_DEVICE so
	 * the re-add path can identify this chip later.
	 */
	ag->wsi_remap_in_progress = false;
	ab->wsi_remap_state = ATH12K_WSI_BYPASS_REMOVE_DEVICE;
	ath12k_info(ab, "WSI remap: Device bypass completed\n");

	return 0;
}

/**
 * ath12k_wifi7_wsi_bypass_add - wifi7 WSI bypass add (re-add) handler
 *
 * Called after the common UMAC reset completes when a previously-bypassed
 * wifi7 chip is being re-added to the active WSI ring.  Powers down the
 * HIF, resets PCIe link speed, re-initialises HAL SRNGs, and powers up
 * Q6 so the normal QMI/WMI boot sequence can proceed.
 */
static int ath12k_wifi7_wsi_bypass_add(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	int ret, i;

	ret = ath12k_core_wsi_mlo_teardown_umac_reset(ab);
	if (ret) {
		ath12k_err(ab, "WSI Bypass: Umac reset failed: %d\n", ret);
		return ret;
	}

	ab->is_bypassed = false;
	ag->num_bypassed--;

	ath12k_hif_irq_disable(ab);
	ath12k_hif_ce_irq_disable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops &&
	    ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop)
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop(ab);
#endif

	ath12k_hif_power_down(ab, false);

	/* Restore PCIe link to full speed */
	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: Reset PCIe link speed");
	ath12k_core_pci_link_speed(ab, 3, 2);

	/* Refresh MLO adjacency info with the chip back in the ring */
	ath12k_update_mlo_adj_chip(ag);

	/* Re-initialise HAL SRNGs before powering up Q6 */
	ret = ath12k_hal_srng_init(ab);
	if (ret) {
		ath12k_err(ab, "WSI re-add: srng init failed: %d\n", ret);
		return ret;
	}

	/* Clear pdev_suspend so mac_start() proceeds normally */
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		ar->pdev_suspend = false;
	}

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Power on Q6");
	ret = ath12k_hif_power_up(ab);
	if (ret) {
		ath12k_err(ab, "WSI re-add: failed to power up: %d\n", ret);
		return ret;
	}

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: num_bypassed %d num_started %d",
		   ag->num_bypassed, ag->num_started);

	return 0;
}

const struct ath12k_cp_arch_ops ath12k_wifi7_cp_ops = {
	.wsi_bypass_remove = ath12k_wifi7_wsi_bypass_remove,
	.wsi_bypass_add    = ath12k_wifi7_wsi_bypass_add,
	.en_ssr            = ath12k_core_send_fw_hang_cmd,
};

static int ath12k_wifi7_init(void)
{
	ath12k_erp_init();

	ath12k_vendor_services_init();

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_init();
#endif
	ahb_err = ath12k_wifi7_ahb_init();
	if (ahb_err)
		pr_warn("Failed to initialize ath12k WiFi7 AHB device: %d\n",
			ahb_err);

	pci_err = ath12k_wifi7_pci_init();
	if (pci_err)
		pr_warn("Failed to initialize ath12k WiFi7 PCI device: %d\n",
			pci_err);

	/* If both failed, return one of the failures (arbitrary) */
	return ahb_err && pci_err ? ahb_err : 0;
}

static void ath12k_wifi7_exit(void)
{
	if (!pci_err)
		ath12k_wifi7_pci_exit();

	if (!ahb_err)
		ath12k_wifi7_ahb_exit();

	ath12k_vendor_services_deinit();

	ath12k_erp_deinit();

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_deinit();
#endif

}

module_init(ath12k_wifi7_init);
module_exit(ath12k_wifi7_exit);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies 802.11be WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
