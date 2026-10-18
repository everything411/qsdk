// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <linux/dmapool.h>
#include "../pci.h"

#include "../core.h"
#include "../debug.h"
#include "../erp.h"
#include "pci.h"
#include "wmi.h"
#include "../hal.h"
#include "../hif.h"
#include "../qmi.h"
#include "../vendor_services.h"
#ifdef CPTCFG_QCN_EXTN
#include "../qcn_extns/ini.h"
#endif /* CPTCFG_QCN_EXTN */

static int pci_err;
static struct dma_pool *ath12k_cu_mem_pool;

static int ath12k_wifi8_cu_mem_pool_init(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = NULL;
	int i;

	if (ath12k_cu_mem_pool)
		return 0;

	for (i = 0; i < ag->num_devices; i++) {
		if (ag->ab[i] && !ag->ab[i]->is_bypassed) {
			ab = ag->ab[i];
			break;
		}
	}

	if (!ab)
		return -EINVAL;

	if (!test_bit(WMI_TLV_SERVICE_SHARED_CU_MEM_MODEL_COUNT_DOWN,
		      ab->wmi_ab.svc_map))
		return 0;

	ath12k_cu_mem_pool = dma_pool_create("ath12k_cu_mem",
					     ab->dev, sizeof(struct ath12k_cu_mem),
					     0, 0);

	if (!ath12k_cu_mem_pool)
		return -ENOMEM;

	ath12k_dbg(ab, ATH12K_DBG_MAC,
		   "cu_mem dma pool created: size %zu align 0 boundary 0\n",
		   sizeof(struct ath12k_cu_mem));

	return 0;
}

static void ath12k_wifi8_cu_mem_pool_deinit(void)
{
	if (!ath12k_cu_mem_pool)
		return;

	dma_pool_destroy(ath12k_cu_mem_pool);
	ath12k_cu_mem_pool = NULL;
}

static int ath12k_wifi8_cu_mem_alloc(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	if (!ath12k_cu_mem_pool)
		return -EINVAL;

	arvif->cu_mem = dma_pool_alloc(ath12k_cu_mem_pool,
				       GFP_KERNEL,
				       &arvif->cu_mem_paddr);
	if (!arvif->cu_mem)
		return -ENOMEM;

	memset(arvif->cu_mem, 0, sizeof(struct ath12k_cu_mem));

	ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
		   "cu_mem alloc: vdev %d vaddr %p paddr %pad size %zu\n",
		   arvif->vdev_id, arvif->cu_mem, &arvif->cu_mem_paddr,
		   sizeof(struct ath12k_cu_mem));

	return 0;
}

static void ath12k_wifi8_cu_mem_free(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	if (!arvif->cu_mem)
		return;

	dma_pool_free(ath12k_cu_mem_pool,
		      arvif->cu_mem,
		      arvif->cu_mem_paddr);

	arvif->cu_mem = NULL;
}

/**
 * ath12k_wifi8_wsi_bypass_remove - wifi8 WSI bypass remove handler
 *
 * Called after the common UMAC reset completes when a wifi8 chip is
 * being bypassed.  Suspends all pdevs, stops Q6 firmware, cleans up
 * driver state, updates MLO adjacency, triggers MLO reconfig on the
 * remaining active chips, and puts the PCIe link into low-speed mode
 * before initiating a fresh MLO setup.
 */
static int ath12k_wifi8_wsi_bypass_remove(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret;

	if (!ab->is_cumac_chip) {
		ret = ath12k_core_wsi_mlo_teardown_umac_reset(ab);
		if (ret) {
			ath12k_err(ab, "WSI Bypass: Umac reset failed: %d\n", ret);
			return ret;
		}
	}

	/* Suspend all pdevs on the chip being bypassed */
	ret = ath12k_core_wsi_remap_pdev_suspend(ab);
	if (ret) {
		ath12k_err(ab, "WSI Bypass: pdev suspend failed: %d\n", ret);
		return ret;
	}

	if (ab->is_cumac_chip) {
		ath12k_core_pci_link_speed(ab, 1, 1);
		ag->wsi_remap_in_progress = false;
		return 0;
	}

	ab->is_bypassed = true;

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Send Mode OFF for Q6");
	ath12k_qmi_firmware_stop(ab);
	ath12k_qmi_free_resource(ab);

	ath12k_hif_mgmt_irq_disable(ab);
	ath12k_hif_irq_disable(ab);
	ath12k_hif_ce_irq_disable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	if (ab->dp->ppe.ppe_ops &&
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop)
		ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop(ab);
#endif
	/* Clean up driver state for the bypassed chip */
	ath12k_core_cleanup(ab);

	/* Global SOC reset */
	ath12k_hif_power_down(ab, false);

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
 * ath12k_wifi8_wsi_bypass_add - wifi8 WSI bypass add (re-add) handler
 *
 * Called after the common UMAC reset completes when a previously-bypassed
 * wifi8 chip is being re-added to the active WSI ring.  Powers down the
 * HIF, resets PCIe link speed, re-initialises HAL SRNGs, and powers up
 * Q6 so the normal QMI/WMI boot sequence can proceed.
 */
static int ath12k_wifi8_wsi_bypass_add(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	int ret, i;

	if (ab->is_cumac_chip) {
		/* CUMAC: PCIe link speed restore + pdev resume via mac_start. */
		ath12k_info(ab, "WSI re-add: CUMAC chip, resuming radios\n");

		for (i = 0; i < ab->num_radios; i++) {
			struct ath12k_hw *ah;

			ar = ab->pdevs[i].ar;
			if (!ar)
				continue;
			ah = ar->ah;
			mutex_lock(&ah->hw_mutex);
			ret = ath12k_mac_start(ar);
			mutex_unlock(&ah->hw_mutex);
			if (ret) {
				ath12k_err(ab, "WSI re-add: mac_start failed idx=%d ret=%d\n",
					   i, ret);
				ar->pdev_suspend = false;
				ag->wsi_remap_in_progress = false;
				return ret;
			}
		}

		ag->wsi_remap_in_progress = false;
		ab->wsi_remap_state = ATH12K_WSI_BYPASS_DEFAULT;
		ath12k_info(ab, "WSI re-add: CUMAC chip resumed\n");
	} else {
		ret = ath12k_core_wsi_mlo_teardown_umac_reset(ab);
		if (ret) {
			ath12k_err(ab, "WSI Bypass: Umac reset failed: %d\n", ret);
			return ret;
		}

		ab->is_bypassed = false;
		ag->num_bypassed--;

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
			if (!ar)
				continue;
			ar->pdev_suspend = false;
		}

		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Power on Q6");
		ret = ath12k_hif_power_up(ab);
		if (ret) {
			ath12k_err(ab, "WSI re-add: failed to power up: %d\n", ret);
			return ret;
		}
	}
	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: Reset PCIe link speed for CUMAC");
	ath12k_core_pci_link_speed(ab, 3, 2);

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS,
		   "WSI Bypass: num_bypassed %d num_started %d",
		   ag->num_bypassed, ag->num_started);

	return 0;
}

const struct ath12k_cp_arch_ops ath12k_wifi8_cp_ops = {
	.cu_mem_pool_init = ath12k_wifi8_cu_mem_pool_init,
	.cu_mem_pool_deinit = ath12k_wifi8_cu_mem_pool_deinit,
	.cu_mem_alloc = ath12k_wifi8_cu_mem_alloc,
	.cu_mem_free = ath12k_wifi8_cu_mem_free,
	.cu_notify = ath12k_wifi8_cu_notify,
	.wsi_bypass_remove = ath12k_wifi8_wsi_bypass_remove,
	.wsi_bypass_add = ath12k_wifi8_wsi_bypass_add,
};

/* SMD feature flags - controls behaviour of SMD-related operations.
 * New per-feature knobs for the SMD topic should be added here so they
 * are all discoverable in one place via modinfo / /sys/module/ath12k_wifi8/parameters/.
 */
bool ath12k_wifi8_clear_vld_after_smd_ctx_fetch = true;
module_param_named(clear_vld_after_smd_ctx_fetch,
		   ath12k_wifi8_clear_vld_after_smd_ctx_fetch, bool, 0644);
MODULE_PARM_DESC(clear_vld_after_smd_ctx_fetch,
		 "Clear REO VLD after fetching SMD ctx (default: 1 - enabled)");

bool ath12k_wifi8_smd_skip_bitmap_update = true;
module_param_named(smd_skip_bitmap_update,
		   ath12k_wifi8_smd_skip_bitmap_update, bool, 0644);
MODULE_PARM_DESC(smd_skip_bitmap_update,
		 "On Target AP, updates SSN and PN but skips REO bitmap update (default: 1 - skips REO bitmap update)");

static int ath12k_wifi8_init(void)
{
	ath12k_erp_init();

	ath12k_vendor_services_init();

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_init();
#endif

	pci_err = ath12k_wifi8_pci_init();
	if (pci_err)
		pr_warn("Failed to initialize ath12k WiFi8 PCI device: %d\n",
			pci_err);

	return pci_err;
}

static void ath12k_wifi8_exit(void)
{
	if (!pci_err)
		ath12k_wifi8_pci_exit();

#ifdef CPTCFG_QCN_EXTN
	ath12k_cfg_global_deinit();
#endif

	ath12k_vendor_services_deinit();

	ath12k_erp_deinit();
}

module_init(ath12k_wifi8_init);
module_exit(ath12k_wifi8_exit);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies 802.11bn WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
