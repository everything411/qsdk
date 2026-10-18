// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 *  Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/remoteproc.h>
#include <linux/panic_notifier.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#include <linux/netfilter_bridge.h>
#endif
#ifdef CONFIG_IO_COHERENCY
#include <linux/tmelcom_ipc.h>
#endif

#include "ahb.h"
#include "core.h"
#include "coredump.h"
#include "dp_tx.h"
#include "dp_rx.h"
#include "debug.h"
#include "debugfs.h"
#include "erp.h"
#include "fw.h"
#include "hif.h"
#include "pci.h"
#include "pcic.h"
#include "wow.h"
#include "dp_cmn.h"
#include "dp.h"
#include "dp_mon.h"
#include "fse.h"
#include "accel_cfg.h"
#include "peer.h"
#include "qos.h"
#include "vendor.h"
#include "qca-vendor.h"
#include "telemetry.h"
#include "ppe.h"
#include "cfr.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/ini.h"
#if defined(CPTCFG_EXT_IPA_OFFLOAD)
#include "qcn_extns/ipa/dp_ipa.h"
#endif /* CPTCFG_EXT_IPA_OFFLOAD */
#endif /* CPTCFG_QCN_EXTN */
#include "telemetry_agent_if.h"
#include "mgmt_rx.h"
#include "me.h"
#ifdef CPTCFG_QCN_EXTN
#include "qcn_extns/me_snoop_extn.h"
#endif /* CPTCFG_QCN_EXTN */
#include "umac_reset.h"

#ifdef CPTCFG_ATHDEBUG
#include "athdbg_if.h"
#endif

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
unsigned int ath12k_ppe_ds_enabled = 1;
module_param_named(ppe_ds_enable, ath12k_ppe_ds_enabled, uint, 0644);
MODULE_PARM_DESC(ppe_ds_enable, "ppe_ds_enable: 0-disable, 1-enable");
EXPORT_SYMBOL(ath12k_ppe_ds_enabled);

unsigned int ath12k_ppe_ds_wifi8_enabled = 1;
module_param_named(ppe_ds_wifi8_enable, ath12k_ppe_ds_wifi8_enabled, uint, 0644);
MODULE_PARM_DESC(ppe_ds_wifi8_enable, "ppe_ds_wifi8_enable: 0-disable, 1-enable");
EXPORT_SYMBOL(ath12k_ppe_ds_wifi8_enabled);

unsigned int ath12k_ppeds_ppe2tcl_rings_max = 1;
module_param_named(ppe2tcl_rings_max, ath12k_ppeds_ppe2tcl_rings_max, uint, 0644);
MODULE_PARM_DESC(ppe2tcl_rings_max, "ppe2tcl_rings_max: 1-default");
EXPORT_SYMBOL(ath12k_ppeds_ppe2tcl_rings_max);

unsigned int ath12k_ppeds_reo2ppe_rings_max = 1;
module_param_named(reo2ppe_rings_max, ath12k_ppeds_reo2ppe_rings_max, uint, 0644);
MODULE_PARM_DESC(reo2ppe_rings_max, "reo2ppe_rings_max: 1-default");
EXPORT_SYMBOL(ath12k_ppeds_reo2ppe_rings_max);

unsigned int ath12k_ppeds_txrx_hw_auto_idx = 1;
module_param_named(txrx_hw_auto_idx, ath12k_ppeds_txrx_hw_auto_idx, uint, 0644);
MODULE_PARM_DESC(txrx_hw_auto_idx, "txrx_hw_auto_idx: 0-default");
EXPORT_SYMBOL(ath12k_ppeds_txrx_hw_auto_idx);

unsigned int ath12k_ppeds_hw_buff_mgmt = 1;
module_param_named(hw_buff_mgmt, ath12k_ppeds_hw_buff_mgmt, uint, 0644);
MODULE_PARM_DESC(hw_buff_mgmt, "hw_buff_mgmt: 0-default");
EXPORT_SYMBOL(ath12k_ppeds_hw_buff_mgmt);

unsigned int ath12k_ppeds_ppe2wbm_ring_size = 512;
module_param_named(ppe2wbm_ring_size, ath12k_ppeds_ppe2wbm_ring_size, uint, 0644);
MODULE_PARM_DESC(ppe2wbm_ring_size, "PPE2WBM Ring size");
EXPORT_SYMBOL(ath12k_ppeds_ppe2wbm_ring_size);

unsigned int ath12k_ppeds_pkt_pre_hdr_mode;
module_param_named(pkt_pre_hdr_mode, ath12k_ppeds_pkt_pre_hdr_mode, uint, 0644);
MODULE_PARM_DESC(pkt_pre_hdr_mode, "Read hint values:\n  0 - 192 bytes (default)\n  1 - 128 bytes\n  2 - 0 bytes\n  3 - full packet");
EXPORT_SYMBOL(ath12k_ppeds_pkt_pre_hdr_mode);
#endif

#define ATH12K_PROBE_ORDER_MASK 0xF
#define ATH12K_PROBE_ORDER_SHIFT 4
int ath12k_valid_probe_order;
unsigned int ath12k_probe_order_param;
module_param_named(probe_order, ath12k_probe_order_param, uint, 0644);
MODULE_PARM_DESC(probe_order, "Probe order (hex bitfield, 4-bit per device)");
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
extern struct ath12k_ps_context ath12k_global_ps_ctx;
#endif

u64 ath12k_debug_mask = ATH12K_DBG_MAC | ATH12K_DBG_EAPOL |
			ATH12K_DBG_MLME | ATH12K_DBG_PEER;
module_param_named(debug_mask, ath12k_debug_mask, ullong, 0644);
MODULE_PARM_DESC(debug_mask, "Debugging mask (64-bit)");
EXPORT_SYMBOL(ath12k_debug_mask);

unsigned int ath12k_debug_mask_level;
module_param_named(debug_level, ath12k_debug_mask_level, uint, 0644);
MODULE_PARM_DESC(debug_mask, "Debugging level");
EXPORT_SYMBOL(ath12k_debug_mask_level);

unsigned int ath12k_mlo_capable = true;
module_param_named(mlo_capable, ath12k_mlo_capable, uint, 0644);
MODULE_PARM_DESC(mlo_capable, "mlo capable: 0-disable, 1-enable");

bool ath12k_ftm_mode;
module_param_named(ftm_mode, ath12k_ftm_mode, bool, 0444);
MODULE_PARM_DESC(ftm_mode, "Boots up in factory test mode");
EXPORT_SYMBOL(ath12k_ftm_mode);

bool ath12k_waltest_mode;
module_param_named(waltest_mode, ath12k_waltest_mode, bool, 0444);
MODULE_PARM_DESC(waltest_mode, "Boots up in Wal test mode");
EXPORT_SYMBOL(ath12k_waltest_mode);

bool ath12k_boot_time_qdss = true;
module_param_named(boot_time_qdss, ath12k_boot_time_qdss, bool, 0444);
MODULE_PARM_DESC(boot_time_qdss, "QDSS tracing at boot time (0: disabled, 1: enabled (default))");

unsigned int ath12k_frame_mode = ATH12K_HW_TXRX_ETHERNET;
module_param_named(frame_mode, ath12k_frame_mode, uint, 0644);
MODULE_PARM_DESC(frame_mode,
		 "Datapath frame mode (0: raw, 1: native wifi (default), 2: ethernet)");
EXPORT_SYMBOL(ath12k_frame_mode);

bool ath12k_fse_3_tuple_enabled = true;
module_param_named(fse_3_tuple_enabled, ath12k_fse_3_tuple_enabled, bool, 0644);
MODULE_PARM_DESC(fse_3_tuple_enabled, "fse_3_tuple_enabled: 0-disable, 1-enable");

#ifdef CONFIG_IO_COHERENCY
bool ath12k_io_coherency_enabled = true;
module_param_named(io_coherency, ath12k_io_coherency_enabled, bool, 0644);
MODULE_PARM_DESC(io_coherency, "Enable io_coherency (0 - disable, 1 - enable)");
#endif

unsigned int ath12k_max_clients = 512;
module_param_named(max_clients, ath12k_max_clients, uint, 0644);
MODULE_PARM_DESC(max_clients, "Max clients support");
EXPORT_SYMBOL(ath12k_max_clients);

/*
 * ath12k_active_mem_profile - active memory profile selected at module load.
 * Set by ath12k_detect_mem_profile(): compile-time macros take priority,
 * then falls back to mem-profile= kernel cmdline parameter.
 */
enum ath12k_mem_profile ath12k_active_mem_profile = ATH12K_MEM_PROFILE_DEFAULT;
EXPORT_SYMBOL(ath12k_active_mem_profile);

/*
 * ath12k_detect_mem_profile()
 *	Detect the active memory profile and initialize ring size config.
 *	Compile-time macros take priority; runtime cmdline is the fallback.
 *
 *	  mem-profile=optimized  -> ATH12K_MEM_PROFILE_OPTIMIZED (256M)
 *	  mem-profile=balanced   -> ATH12K_MEM_PROFILE_BALANCED  (512M)
 *	  mem-profile=high       -> ATH12K_MEM_PROFILE_DEFAULT   (1G)
 *	  (absent)               -> ATH12K_MEM_PROFILE_DEFAULT   (1G)
 */
static void ath12k_detect_mem_profile(void)
{
	ath12k_active_mem_profile = ATH12K_MEM_PROFILE_DEFAULT;

#if defined(CONFIG_ATH12K_MEM_PROFILE_256M) || defined(CPTCFG_ATH12K_MEM_PROFILE_256M)
	ath12k_active_mem_profile = ATH12K_MEM_PROFILE_OPTIMIZED;
	ath12k_info(NULL, "ath12k: compile-time 256M profile -> OPTIMIZED\n");
#elif defined(CONFIG_ATH12K_MEM_PROFILE_512M) || defined(CPTCFG_ATH12K_MEM_PROFILE_512M)
	ath12k_active_mem_profile = ATH12K_MEM_PROFILE_BALANCED;
	ath12k_info(NULL, "ath12k: compile-time 512M profile -> BALANCED\n");
#else
#if LINUX_VERSION_IS_LESS(6,15,0)
	if (strstr(saved_command_line, "mem-profile=optimized")) {
		ath12k_active_mem_profile = ATH12K_MEM_PROFILE_OPTIMIZED;
		ath12k_info(NULL, "ath12k: mem-profile=optimized -> OPTIMIZED\n");
	} else if (strstr(saved_command_line, "mem-profile=balanced")) {
		ath12k_active_mem_profile = ATH12K_MEM_PROFILE_BALANCED;
		ath12k_info(NULL, "ath12k: mem-profile=balanced -> BALANCED\n");
	} else if (strstr(saved_command_line, "mem-profile=high")) {
		ath12k_active_mem_profile = ATH12K_MEM_PROFILE_DEFAULT;
		ath12k_info(NULL, "ath12k: mem-profile=high -> DEFAULT\n");
	} else {
		ath12k_active_mem_profile = ATH12K_MEM_PROFILE_DEFAULT;
		ath12k_info(NULL, "ath12k: no mem-profile in cmdline -> DEFAULT\n");
	}
#else
	ath12k_active_mem_profile = ATH12K_MEM_PROFILE_DEFAULT;
	ath12k_info(NULL, "ath12k: no mem-profile in cmdline -> DEFAULT\n");
#endif
#endif

}

static unsigned int ath12k_en_fwlog = true;
module_param_named(en_fwlog, ath12k_en_fwlog, uint, 0644);
MODULE_PARM_DESC(en_fwlog, "fwlog: 0-disable, 1-enable");

unsigned int ath12k_ssr_failsafe_mode = true;
module_param_named(ssr_failsafe_mode, ath12k_ssr_failsafe_mode, uint, 0644);
MODULE_PARM_DESC(ssr_failsafe_mode, "ssr failsafe mode: 0-disable, 1-enable");

bool ath12k_upload_dump_before_bugon;
EXPORT_SYMBOL(ath12k_upload_dump_before_bugon);
module_param_named(upload_q6dump_to_cloud, ath12k_upload_dump_before_bugon, bool, 0644);
MODULE_PARM_DESC(upload_q6dump_to_cloud, "Upload Q6 FW dump to cloud server: 0-disable (default), 1-enable");

bool ath12k_rx_nwifi_err_dump = false;
module_param_named(rx_nwifi_err_dump, ath12k_rx_nwifi_err_dump, bool, 0644);
MODULE_PARM_DESC(rx_nwifi_err_dump, "rx nwifi err dump: 0-disable, 1-enable");
EXPORT_SYMBOL(ath12k_rx_nwifi_err_dump);

bool ath12k_ppe_rfs_support = true;
module_param_named(ppe_rfs_support, ath12k_ppe_rfs_support, bool, 0644);
MODULE_PARM_DESC(ppe_rfs_support, "Enable PPE RFS support for DL (0 - disable, 1 - enable)");

unsigned int ath12k_fw_mem_seg;
module_param_named(fw_mem_seg, ath12k_fw_mem_seg, uint, 0644);
MODULE_PARM_DESC(fw_mem_seg, "Enable/Disable FW segmented memory");

unsigned int ath12k_rfs_core_mask[4] = {ATH12K_MAX_CORE_MASK, ATH12K_MAX_CORE_MASK,
					ATH12K_MAX_CORE_MASK, ATH12K_MAX_CORE_MASK};
module_param_array_named(rfs_core_mask, ath12k_rfs_core_mask, int, NULL, 0644);
MODULE_PARM_DESC(rfs_core_mask, "Default RFS core mask, mask for 2G, mask for 5G,\n"
		 "mask for 6G. One bit for one CPU core\n");

bool ath12k_debug_critical = false;
module_param_named(debug_critical, ath12k_debug_critical, bool, 0644);
MODULE_PARM_DESC(debug_critical, "Debug critical issue (0 - disable, 1 - enable)");
EXPORT_SYMBOL(ath12k_debug_critical);

unsigned int ath12k_cfr_enable_bmap = 0;
module_param_named(cfr_enable_bmap, ath12k_cfr_enable_bmap, uint, 0644);
MODULE_PARM_DESC(cfr_enable_bmap, "cfr_enable_bmap: 0-disable, enable-(0x7 for 3 chipsets)");

bool ath12k_mlo_3_link_tx;
module_param_named(mlo_3_link_tx, ath12k_mlo_3_link_tx, bool, 0644);
MODULE_PARM_DESC(mlo_3_link_tx, "3 link MLO active TX support (0 - disable, 1 - enable)");

bool ath12k_mcast_link_bmap_enable;
module_param_named(mcast_link_bmap_enable, ath12k_mcast_link_bmap_enable, bool, 0644);
MODULE_PARM_DESC(mcast_link_bmap_enable,
		 "Enable per-packet MLO multicast link bitmap filtering (0 - disable, 1 - enable)");
EXPORT_SYMBOL(ath12k_mcast_link_bmap_enable);

unsigned int ath12k_wsi_bypass_bmap;
module_param_named(wsi_bypass_bmap, ath12k_wsi_bypass_bmap, uint, 0644);
MODULE_PARM_DESC(wsi_bypass_bmap,
		 "Bitmap for the chip to be bypassed for WSI interface");

bool ath12k_carrier_vow_optimization;
module_param_named(carrier_vow_optimization, ath12k_carrier_vow_optimization, bool, 0644);
MODULE_PARM_DESC(carrier_vow_optimization, "Enable/Disable VoW optimization for carier usecases");
EXPORT_SYMBOL(ath12k_carrier_vow_optimization);

bool ath12k_fw_q6_dump_collection;
module_param_named(fw_q6_dump_collection, ath12k_fw_q6_dump_collection, bool, 0644);
MODULE_PARM_DESC(fw_q6_dump_collection, "FW Q6 dump collection only: 0-disable (default), 1-enable");

bool ath12k_hw_group_started;

unsigned int ath12k_reorder_VI_timeout;
module_param_named(reorder_VI_timeout, ath12k_reorder_VI_timeout, uint, 0644);
MODULE_PARM_DESC(reorder_VI_timeout, "Reorder VI timeout (ms)");
EXPORT_SYMBOL(ath12k_reorder_VI_timeout);

static unsigned int ath12k_erp_cumac_config = ATH12K_ERP_CUMAC_UNSET;
module_param_named(erp_cumac_config, ath12k_erp_cumac_config, uint, 0644);
MODULE_PARM_DESC(erp_cumac_config,
		 "ErP CUMAC configuration bitmap: BIT(0): pdev suspend, BIT(1): Q6 power down");

bool ath12k_congestion_ctrl = true;
module_param_named(congestion_ctrl, ath12k_congestion_ctrl, bool, 0644);
MODULE_PARM_DESC(congestion_ctrl, "Congestion control 0-disable (default) 1-enable");
EXPORT_SYMBOL(ath12k_congestion_ctrl);

unsigned int ath12k_drop_algo = 1;
module_param_named(drop_algo, ath12k_drop_algo, uint, 0644);
MODULE_PARM_DESC(drop_algo, "Congestion drop algorithm type: 0-composite (default) 1-flat");
EXPORT_SYMBOL(ath12k_drop_algo);

/* protected with ath12k_hw_group_mutex */
static struct list_head ath12k_hw_group_list = LIST_HEAD_INIT(ath12k_hw_group_list);

static DEFINE_MUTEX(ath12k_hw_group_mutex);

extern struct ath12k_coredump_info ath12k_coredump_ram_info;

#ifdef CONFIG_IO_COHERENCY
int ath12k_core_config_iocoherency(struct ath12k_base *ab, bool enable)
{
	int ret = 0, num_elem, idx = 0;
	void __iomem *map_addr;
	bool no_tmel;
	struct tmel_secure_io secure_reg;

	if (!ath12k_io_coherency_enabled) {
		ath12k_err(ab, "io-coherency Disabled\n");
		return 0;
	}

	if (!ab->dev || !ab->dev->of_node) {
		ath12k_err(ab, "improper dts node\n");
		return 0;
	}

	if (ab->in_coldboot_fwreset)
		return 0;

	num_elem = of_property_count_elems_of_size(ab->dev->of_node, "secure-reg",
						   sizeof(u32));
	if (num_elem < 0) {
		ath12k_err(ab, "secure-reg not configured for io-coherency\n");
		return 0;
	}

	while (idx < num_elem) {
		ret = of_property_read_u32_index(ab->dev->of_node, "secure-reg", idx++,
						 &secure_reg.reg_addr);
		if (ret) {
			ath12k_err(ab, "failed to get the secure reg addr %d\n", (idx - 1));
			goto err;
		}

		if (enable) {
			ret = of_property_read_u32_index(ab->dev->of_node, "secure-reg", idx++,
							 &secure_reg.reg_val);

			if (ret) {
				ath12k_err(ab, "failed to get the secure reg val %d\n", (idx - 1));
				goto err;
			}
		} else {
			secure_reg.reg_val = 0;
			idx++;
		}

		ath12k_info(ab, "Configuring secure reg: 0x%x val: 0x%x\n",
			    secure_reg.reg_addr, secure_reg.reg_val);

		no_tmel = of_property_read_bool(ab->dev->of_node, "qcom,no-tmel");
		if (!no_tmel) {
			ret = tmelcom_secure_io_write(&secure_reg,
						      sizeof(struct tmel_secure_io));
			if (ret) {
				ath12k_err(ab, "Failed secure_reg settings\n");
				goto err;
			};

		} else {
			map_addr = ioremap(secure_reg.reg_addr, sizeof(uint32_t));
			if (!map_addr) {
				ret = -ENOMEM;
				ath12k_err(ab, "Failed secure_reg mapping\n");
				goto err;
			}
			writel(secure_reg.reg_val, map_addr);
			iounmap(map_addr);
		}
	}

err:
	return ret;
}
#endif

static int ath12k_core_rfkill_config(struct ath12k_base *ab)
{
	struct ath12k *ar;
	int ret = 0, i;

	if (!(ab->target_caps.sys_cap_info & WMI_SYS_CAP_INFO_RFKILL))
		return 0;

	if (ath12k_acpi_get_disable_rfkill(ab))
		return 0;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		ret = ath12k_mac_rfkill_config(ar);
		if (ret && ret != -EOPNOTSUPP) {
			ath12k_warn(ab, "failed to configure rfkill: %d", ret);
			return ret;
		}
	}

	return ret;
}

/* Check if we need to continue with suspend/resume operation.
 * Return:
 *	a negative value: error happens and don't continue.
 *	0:  no error but don't continue.
 *	positive value: no error and do continue.
 */
static int ath12k_core_continue_suspend_resume(struct ath12k_base *ab)
{
	struct ath12k *ar;

	if (!ab->hw_params->supports_suspend)
		return -EOPNOTSUPP;

	/* so far single_pdev_only chips have supports_suspend as true
	 * so pass 0 as a dummy pdev_id here.
	 */
	ar = ab->pdevs[0].ar;
	if (!ar || !ar->ah || ar->ah->state != ATH12K_HW_STATE_OFF)
		return 0;

	return 1;
}

static unsigned int ath12k_crypto_mode;
module_param_named(crypto_mode, ath12k_crypto_mode, uint, 0644);
MODULE_PARM_DESC(crypto_mode, "crypto mode: 0-hardware, 1-software");

int ath12k_core_suspend(struct ath12k_base *ab)
{
	struct ath12k *ar;
	int ret, i;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar)
			continue;

		wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);

		ret = ath12k_mac_wait_tx_complete(ar);
		if (ret) {
			wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
			ath12k_warn(ab, "failed to wait tx complete: %d\n", ret);
			return ret;
		}

		wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	}

	/* PM framework skips suspend_late/resume_early callbacks
	 * if other devices report errors in their suspend callbacks.
	 * However ath12k_core_resume() would still be called because
	 * here we return success thus kernel put us on dpm_suspended_list.
	 * Since we won't go through a power down/up cycle, there is
	 * no chance to call complete(&ab->restart_completed) in
	 * ath12k_core_restart(), making ath12k_core_resume() timeout.
	 * So call it here to avoid this issue. This also works in case
	 * no error happens thus suspend_late/resume_early get called,
	 * because it will be reinitialized in ath12k_core_resume_early().
	 */
	complete(&ab->restart_completed);

	return 0;
}
EXPORT_SYMBOL(ath12k_core_suspend);

int ath12k_core_suspend_late(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	ath12k_acpi_stop(ab);

	ath12k_hif_mgmt_irq_disable(ab);
	ath12k_hif_irq_disable(ab);
	ath12k_hif_ce_irq_disable(ab);

	ath12k_hif_power_down(ab, true);

	return 0;
}
EXPORT_SYMBOL(ath12k_core_suspend_late);

int ath12k_core_resume_early(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	reinit_completion(&ab->restart_completed);
	ret = ath12k_hif_power_up(ab);
	if (ret)
		ath12k_warn(ab, "failed to power up hif during resume: %d\n", ret);

	return ret;
}
EXPORT_SYMBOL(ath12k_core_resume_early);

int ath12k_core_resume(struct ath12k_base *ab)
{
	long time_left;
	int ret;

	ret = ath12k_core_continue_suspend_resume(ab);
	if (ret <= 0)
		return ret;

	time_left = wait_for_completion_timeout(&ab->restart_completed,
						ATH12K_RESET_TIMEOUT_HZ);
	if (time_left == 0) {
		ath12k_warn(ab, "timeout while waiting for restart complete");
		return -ETIMEDOUT;
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_core_resume);

static int __ath12k_core_create_board_name(struct ath12k_base *ab, char *name,
					   size_t name_len, bool with_variant,
					   bool bus_type_mode, bool with_default)
{
	/* strlen(',variant=') + strlen(ab->qmi.target.bdf_ext) */
	char variant[9 + ATH12K_QMI_BDF_EXT_STR_LENGTH] = { 0 };

	if (with_variant && ab->qmi.target.bdf_ext[0] != '\0')
		scnprintf(variant, sizeof(variant), ",variant=%s",
			  ab->qmi.target.bdf_ext);

	switch (ab->id.bdf_search) {
	case ATH12K_BDF_SEARCH_BUS_AND_BOARD:
		if (bus_type_mode)
			scnprintf(name, name_len,
				  "bus=%s",
				  ath12k_bus_str(ab->hif.bus));
		else
			scnprintf(name, name_len,
				  "bus=%s,vendor=%04x,device=%04x,subsystem-vendor=%04x,subsystem-device=%04x,qmi-chip-id=%d,qmi-board-id=%d%s",
				  ath12k_bus_str(ab->hif.bus),
				  ab->id.vendor, ab->id.device,
				  ab->id.subsystem_vendor,
				  ab->id.subsystem_device,
				  ab->qmi.target.chip_id,
				  ab->qmi.target.board_id,
				  variant);
		break;
	default:
		scnprintf(name, name_len,
			  "bus=%s,qmi-chip-id=%d,qmi-board-id=%d%s",
			  ath12k_bus_str(ab->hif.bus),
			  ab->qmi.target.chip_id,
			  with_default ?
			  ATH12K_BOARD_ID_DEFAULT : ab->qmi.target.board_id,
			  variant);
		break;
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot using board name '%s'\n", name);

	return 0;
}

static int ath12k_core_create_board_name(struct ath12k_base *ab, char *name,
					 size_t name_len)
{
	return __ath12k_core_create_board_name(ab, name, name_len, true, false, false);
}

static int ath12k_core_create_fallback_board_name(struct ath12k_base *ab, char *name,
						  size_t name_len)
{
	return __ath12k_core_create_board_name(ab, name, name_len, false, false, true);
}

static int ath12k_core_create_bus_type_board_name(struct ath12k_base *ab, char *name,
						  size_t name_len)
{
	return __ath12k_core_create_board_name(ab, name, name_len, false, true, true);
}

const struct firmware *ath12k_core_firmware_request(struct ath12k_base *ab,
						    const char *file)
{
	const struct firmware *fw;
	char path[100];
	int ret;

	if (!file)
		return ERR_PTR(-ENOENT);

	ath12k_core_create_firmware_path(ab, file, path, sizeof(path));

	ret = firmware_request_nowarn(&fw, path, ab->dev);
	if (ret)
		return ERR_PTR(ret);

	if (!fw || !fw->size)
		return ERR_PTR(-ENOENT);

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot firmware request %s size %zu\n",
		   path, fw->size);

	return fw;
}

void ath12k_core_free_bdf(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	if (!IS_ERR(bd->fw))
		release_firmware(bd->fw);

	memset(bd, 0, sizeof(*bd));
}

static int ath12k_core_parse_bd_ie_board(struct ath12k_base *ab,
					 struct ath12k_board_data *bd,
					 const void *buf, size_t buf_len,
					 const char *boardname,
					 int ie_id,
					 int name_id,
					 int data_id)
{
	const struct ath12k_fw_ie *hdr;
	bool name_match_found;
	int ret, board_ie_id;
	size_t board_ie_len;
	const void *board_ie_data;

	name_match_found = false;

	/* go through ATH12K_BD_IE_BOARD_/ATH12K_BD_IE_REGDB_ elements */
	while (buf_len > sizeof(struct ath12k_fw_ie)) {
		hdr = buf;
		board_ie_id = le32_to_cpu(hdr->id);
		board_ie_len = le32_to_cpu(hdr->len);
		board_ie_data = hdr->data;

		buf_len -= sizeof(*hdr);
		buf += sizeof(*hdr);

		if (buf_len < ALIGN(board_ie_len, 4)) {
			ath12k_err(ab, "invalid %s length: %zu < %zu\n",
				   ath12k_bd_ie_type_str(ie_id),
				   buf_len, ALIGN(board_ie_len, 4));
			ret = -EINVAL;
			goto out;
		}

		if (board_ie_id == name_id) {
			ath12k_dbg_dump(ab, ATH12K_DBG_BOOT, "board name", "",
					board_ie_data, board_ie_len);

			if (board_ie_len != strlen(boardname))
				goto next;

			ret = memcmp(board_ie_data, boardname, strlen(boardname));
			if (ret)
				goto next;

			name_match_found = true;
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "boot found match %s for name '%s'",
				   ath12k_bd_ie_type_str(ie_id),
				   boardname);
		} else if (board_ie_id == data_id) {
			if (!name_match_found)
				/* no match found */
				goto next;

			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "boot found %s for '%s'",
				   ath12k_bd_ie_type_str(ie_id),
				   boardname);

			bd->data = board_ie_data;
			bd->len = board_ie_len;

			ret = 0;
			goto out;
		} else {
			ath12k_warn(ab, "unknown %s id found: %d\n",
				    ath12k_bd_ie_type_str(ie_id),
				    board_ie_id);
		}
next:
		/* jump over the padding */
		board_ie_len = ALIGN(board_ie_len, 4);

		buf_len -= board_ie_len;
		buf += board_ie_len;
	}

	/* no match found */
	ret = -ENOENT;

out:
	return ret;
}

static int ath12k_core_fetch_board_data_api_n(struct ath12k_base *ab,
					      struct ath12k_board_data *bd,
					      const char *boardname,
					      int ie_id_match,
					      int name_id,
					      int data_id)
{
	size_t len, magic_len;
	const u8 *data;
	char *filename, filepath[100];
	size_t ie_len;
	struct ath12k_fw_ie *hdr;
	int ret, ie_id;

	filename = ATH12K_BOARD_API2_FILE;

	if (!bd->fw)
		bd->fw = ath12k_core_firmware_request(ab, filename);

	if (IS_ERR(bd->fw))
		return PTR_ERR(bd->fw);

	data = bd->fw->data;
	len = bd->fw->size;

	ath12k_core_create_firmware_path(ab, filename,
					 filepath, sizeof(filepath));

	/* magic has extra null byte padded */
	magic_len = strlen(ab->hw_params->board_magic) + 1;
	if (len < magic_len) {
		ath12k_err(ab, "failed to find magic value in %s, file too short: %zu\n",
			   filepath, len);
		ret = -EINVAL;
		goto err;
	}

	if (memcmp(data, ab->hw_params->board_magic, magic_len)) {
		ath12k_err(ab, "found invalid board magic\n");
		ret = -EINVAL;
		goto err;
	}

	/* magic is padded to 4 bytes */
	magic_len = ALIGN(magic_len, 4);
	if (len < magic_len) {
		ath12k_err(ab, "failed: %s too small to contain board data, len: %zu\n",
			   filepath, len);
		ret = -EINVAL;
		goto err;
	}

	data += magic_len;
	len -= magic_len;

	while (len > sizeof(struct ath12k_fw_ie)) {
		hdr = (struct ath12k_fw_ie *)data;
		ie_id = le32_to_cpu(hdr->id);
		ie_len = le32_to_cpu(hdr->len);

		len -= sizeof(*hdr);
		data = hdr->data;

		if (len < ALIGN(ie_len, 4)) {
			ath12k_err(ab, "invalid length for board ie_id %d ie_len %zu len %zu\n",
				   ie_id, ie_len, len);
			ret = -EINVAL;
			goto err;
		}

		if (ie_id == ie_id_match) {
			ret = ath12k_core_parse_bd_ie_board(ab, bd, data,
							    ie_len,
							    boardname,
							    ie_id_match,
							    name_id,
							    data_id);
			if (ret == -ENOENT)
				/* no match found, continue */
				goto next;
			else if (ret)
				/* there was an error, bail out */
				goto err;
			/* either found or error, so stop searching */
			goto out;
		}
next:
		/* jump over the padding */
		ie_len = ALIGN(ie_len, 4);

		len -= ie_len;
		data += ie_len;
	}

out:
	if (!bd->data || !bd->len) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to fetch %s for %s from %s\n",
			   ath12k_bd_ie_type_str(ie_id_match),
			   boardname, filepath);
		ret = -ENODATA;
		goto err;
	}

	return 0;

err:
	ath12k_core_free_bdf(ab, bd);
	return ret;
}

int ath12k_core_fetch_board_data_api_1(struct ath12k_base *ab,
				       struct ath12k_board_data *bd,
				       char *filename)
{
	bd->fw = ath12k_core_firmware_request(ab, filename);
	if (IS_ERR(bd->fw))
		return PTR_ERR(bd->fw);

	bd->data = bd->fw->data;
	bd->len = bd->fw->size;

	return 0;
}

#define BOARD_NAME_SIZE 200
int ath12k_core_fetch_bdf(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	char boardname[BOARD_NAME_SIZE], fallback_boardname[BOARD_NAME_SIZE];
	char *filename, filepath[100];
	int bd_api;
	int ret;

	filename = ATH12K_BOARD_API2_FILE;

	ret = ath12k_core_create_board_name(ab, boardname, sizeof(boardname));
	if (ret) {
		ath12k_err(ab, "failed to create board name: %d", ret);
		return ret;
	}

	bd_api = 2;
	ret = ath12k_core_fetch_board_data_api_n(ab, bd, boardname,
						 ATH12K_BD_IE_BOARD,
						 ATH12K_BD_IE_BOARD_NAME,
						 ATH12K_BD_IE_BOARD_DATA);
	if (!ret)
		goto success;

	ret = ath12k_core_create_fallback_board_name(ab, fallback_boardname,
						     sizeof(fallback_boardname));
	if (ret) {
		ath12k_err(ab, "failed to create fallback board name: %d", ret);
		return ret;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, fallback_boardname,
						 ATH12K_BD_IE_BOARD,
						 ATH12K_BD_IE_BOARD_NAME,
						 ATH12K_BD_IE_BOARD_DATA);
	if (!ret)
		goto success;

	bd_api = 1;
	ret = ath12k_core_fetch_board_data_api_1(ab, bd, ATH12K_DEFAULT_BOARD_FILE);
	if (ret) {
		ath12k_core_create_firmware_path(ab, filename,
						 filepath, sizeof(filepath));
		ath12k_err(ab, "failed to fetch board data for %s from %s\n",
			   boardname, filepath);
		if (memcmp(boardname, fallback_boardname, strlen(boardname)))
			ath12k_err(ab, "failed to fetch board data for %s from %s\n",
				   fallback_boardname, filepath);

		ath12k_err(ab, "failed to fetch board.bin from %s\n",
			   ab->hw_params->fw.dir);
		return ret;
	}

success:
	ath12k_dbg(ab, ATH12K_DBG_BOOT, "using board api %d\n", bd_api);
	return 0;
}

int ath12k_core_fetch_fw_cfg(struct ath12k_base *ab,
			     struct ath12k_board_data *bd)
{
	int ret;

	ret = ath12k_core_fetch_board_data_api_1(ab, bd, ATH12K_FW_CFG_FILE);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_QMI, "failed to fetch %s from %s\n",
			   ATH12K_FW_CFG_FILE, ab->hw_params->fw.dir);
		return -ENOENT;
	}

	ath12k_info(ab, "fetching %s from %s\n", ATH12K_FW_CFG_FILE,
		    ab->hw_params->fw.dir);

	return 0;
}

int ath12k_core_fetch_rxgainlut(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	char rxgainlutname[BOARD_NAME_SIZE] = {};
	char rxgainlutdefaultname[BOARD_NAME_SIZE] = {};
	int ret;

	ret = ath12k_core_create_board_name(ab, rxgainlutname,
					    BOARD_NAME_SIZE);
	if (ret) {
		ath12k_err(ab, "failed to create rxgainlut name: %d", ret);
	return ret;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, rxgainlutname,
						 ATH12K_BD_IE_RXGAINLUT,
						 ATH12K_BD_IE_RXGAINLUT_NAME,
						 ATH12K_BD_IE_RXGAINLUT_DATA);
	if (!ret)
		goto exit;

	ret = ath12k_core_create_fallback_board_name(ab, rxgainlutdefaultname,
					    	     BOARD_NAME_SIZE);
	if (ret) {
		ath12k_err(ab, "failed to create rxgainlut name: %d", ret);
	return ret;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, rxgainlutdefaultname,
						 ATH12K_BD_IE_RXGAINLUT,
						 ATH12K_BD_IE_RXGAINLUT_NAME,
						 ATH12K_BD_IE_RXGAINLUT_DATA);
	if (!ret)
		goto exit;

	snprintf(rxgainlutname, sizeof(rxgainlutname), "%s%04x",
		 ATH12K_RXGAINLUT_FILE_PREFIX, ab->qmi.target.board_id);

	ret = ath12k_core_fetch_board_data_api_1(ab, bd, rxgainlutname);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "failed to fetch %s from %s\n",
			   rxgainlutname, ab->hw_params->fw.dir);

		ret = ath12k_core_fetch_board_data_api_1(ab, bd,
							 ATH12K_RXGAINLUT_FILE);
		if (ret) {
			ath12k_warn(ab, "failed to fetch default %s from %s\n",
				    ATH12K_RXGAINLUT_FILE, ab->hw_params->fw.dir);
			return -ENOENT;
		}
	}

exit:
	ath12k_dbg(ab, ATH12K_DBG_BOOT, "fetche rxgainlut");
	return 0;
}

int ath12k_core_fetch_regdb(struct ath12k_base *ab, struct ath12k_board_data *bd)
{
	char boardname[BOARD_NAME_SIZE], default_boardname[BOARD_NAME_SIZE];
	int ret;

	ret = ath12k_core_create_board_name(ab, boardname, BOARD_NAME_SIZE);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to create board name for regdb: %d", ret);
		goto exit;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, boardname,
						 ATH12K_BD_IE_REGDB,
						 ATH12K_BD_IE_REGDB_NAME,
						 ATH12K_BD_IE_REGDB_DATA);
	if (!ret)
		goto exit;

	ret = ath12k_core_create_bus_type_board_name(ab, default_boardname,
						     BOARD_NAME_SIZE);
	if (ret) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to create default board name for regdb: %d", ret);
		goto exit;
	}

	ret = ath12k_core_fetch_board_data_api_n(ab, bd, default_boardname,
						 ATH12K_BD_IE_REGDB,
						 ATH12K_BD_IE_REGDB_NAME,
						 ATH12K_BD_IE_REGDB_DATA);
	if (!ret)
		goto exit;

	ret = ath12k_core_fetch_board_data_api_1(ab, bd, ATH12K_REGDB_FILE_NAME);
	if (ret)
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "failed to fetch %s from %s\n",
			   ATH12K_REGDB_FILE_NAME, ab->hw_params->fw.dir);

exit:
	if (!ret)
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "fetched regdb\n");

	return ret;
}

u32 ath12k_core_get_max_station_per_radio(struct ath12k_base *ab)
{
	if (ab->num_radios == 2)
		return TARGET_NUM_STATIONS_DBS;
	else if (ab->num_radios == 3)
		return TARGET_NUM_STATIONS_DBS_SBS;
	return TARGET_NUM_STATIONS_SINGLE;
}

u32 ath12k_core_get_def_max_peers_per_radio(struct ath12k_base *ab)
{
	if (ab->num_radios == 2)
		return TARGET_NUM_PEERS_PDEV_DBS;
	else if (ab->num_radios == 3)
		return TARGET_NUM_PEERS_PDEV_DBS_SBS;
	return TARGET_NUM_PEERS_PDEV_SINGLE;
}
EXPORT_SYMBOL(ath12k_core_get_def_max_peers_per_radio);

u32 ath12k_core_get_supp_max_peers_per_radio(struct ath12k_base *ab)
{
	u32 host_peers = ath12k_core_get_def_max_peers_per_radio(ab);
	u32 fw_peers = ab->fw_max_peers / ab->num_radios;

	if (!fw_peers) {
		ath12k_warn(ab, "Invalid fw_peers, fw_max_peers:%u, num_radios:%d\n",
				ab->fw_max_peers, ab->num_radios);
		return host_peers;
	}
	return min(host_peers, fw_peers);
}
EXPORT_SYMBOL(ath12k_core_get_supp_max_peers_per_radio);

u32 ath12k_core_get_max_num_tids(struct ath12k_base *ab)
{
	if (ab->num_radios == 2)
		return TARGET_NUM_TIDS(DBS);
	else if (ab->num_radios == 3)
		return TARGET_NUM_TIDS(DBS_SBS);
	return TARGET_NUM_TIDS(SINGLE);
}
EXPORT_SYMBOL(ath12k_core_get_max_num_tids);

struct reserved_mem *ath12k_core_get_reserved_mem_by_name(struct ath12k_base *ab,
							 const char *name)
{
	struct device *dev = ab->dev;
	struct reserved_mem *rmem;
	struct device_node *node;
	int index;

	index = of_property_match_string(dev->of_node, "memory-region-names", name);
	if (index < 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "memory region %s not found\n", name);
		return NULL;
	}

	node = of_parse_phandle(dev->of_node, "memory-region", index);
	if (!node) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "failed to parse memory region %s\n", name);
		return NULL;
	}

	rmem = of_reserved_mem_lookup(node);
	of_node_put(node);
	if (!rmem) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "unable to get memory-region for index %d\n", index);
		return NULL;
	}

	return rmem;
}

static inline
void ath12k_core_to_group_ref_get(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	lockdep_assert_held(&ag->mutex);

	if (ab->hw_group_ref) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "core already attached to group %d\n",
			   ag->id);
		return;
	}

	ab->hw_group_ref = true;
	ag->num_started++;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "core attached to group %d, num_started %d\n",
		   ag->id, ag->num_started);
}

void ath12k_core_to_group_ref_put(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	lockdep_assert_held(&ag->mutex);

	if (!ab->hw_group_ref) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "core already de-attached from group %d\n",
			   ag->id);
		return;
	}

	ab->hw_group_ref = false;
	ag->num_started--;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "core de-attached from group %d, num_started %d\n",
		   ag->id, ag->num_started);
}
EXPORT_SYMBOL(ath12k_core_to_group_ref_put);

int ath12k_core_power_up(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	unsigned long time_left;
	bool wait_for_power_up = false;
	struct ath12k *ar = NULL;
	int i, j, ret;

	reinit_completion(&ag->power_up);
	for (i = 0; i < ag->num_probed; i++) {
		ab =  ag->ab[i];
		if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags) &&
		    !ath12k_hw_group_recovery_in_progress(ag)) {
			for (j = 0; j < ab->num_radios; j++) {
				ar = ab->pdevs[j].ar;
				ar->pdev_suspend = false;
			}

			ath12k_hif_power_up(ab);
			ab->powerup_triggered = true;
			wait_for_power_up = true;
			ath12k_info(ab, "Q6 power up is started\n");
		}
	}

	if (wait_for_power_up) {
		time_left = wait_for_completion_timeout(&ag->power_up,
							ATH12K_Q6_POWER_UP_TIMEOUT);
		if (!time_left) {
			ath12k_err(NULL, "Q6 power up wait timed out\n");
			return -ETIMEDOUT;
		}
	} else {
		ret = ath12k_core_mlo_setup(ag);
		if (ret) {
			ath12k_err(NULL, "mlo setup is failed: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

void ath12k_core_cleanup(struct ath12k_base *ab)
{
	ath12k_umac_reset_fallback_cleanup(ab);

	mutex_lock(&ab->core_lock);
	ath12k_core_pdev_deinit(ab);
	ath12k_dp_arch_pdev_free(ab->dp);
	if (!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags) &&
	    !ab->umcmn_fatal_received)
		ath12k_ce_cleanup_pipes(ab);
	ath12k_wmi_detach(ab);
	mutex_unlock(&ab->core_lock);

	ath12k_mgmt_device_deinit(ab->mgmt);
	ath12k_dp_cmn_device_deinit(ab->dp);
	ath12k_hal_srng_deinit(ab);
	ath12k_dp_umac_reset_deinit(ab);
}
EXPORT_SYMBOL(ath12k_core_cleanup);

static void ath12k_core_power_down_device(struct ath12k_hw_group *ag,
					  struct ath12k_base *ab, bool standby_mode,
					  bool *wifi_standby_enabled)
{
	struct ath12k *ar;
	u8 total_vdevs;
	int i;
	bool skip_power_down = false;

	if (!ab)
		return;

	if (standby_mode) {
		if (ab->is_cumac_chip) {
			if (ath12k_erp_cumac_config != ATH12K_ERP_CUMAC_UNSET) {
				/* User override: skip mac stop and q6 power down */
				if (!(ath12k_erp_cumac_config &
				      ATH12K_ERP_CUMAC_PDEV_SUSPEND))
					return;
				/* User override: mac stop but skip q6 power down */
				if (!(ath12k_erp_cumac_config &
				      ATH12K_ERP_CUMAC_Q6_PWR_DOWN))
					skip_power_down = true;
			} else {
				/* No user override: mac stop always, q6 power down
				 * only when all wifi interfaces are being brought down.
				 */
				if (*wifi_standby_enabled)
					skip_power_down = true;
			}
		}

		for (i = 0; i < ab->num_radios; i++) {
			ar = ab->pdevs[i].ar;

			if (ar) {
				if (ar->allocated_vdev_map) {
					*wifi_standby_enabled = true;
					skip_power_down = true;
				} else
					ath12k_mac_stop(ar);

				ar->cumac_cmd_sent = false;
			}
		}
	}

	if (!skip_power_down &&
	    !test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags)) {
		ab->qmi.num_radios = U8_MAX;
		ath12k_umac_reset_fallback_cleanup(ab);
		ath12k_hif_mgmt_irq_disable(ab);
		ath12k_hif_irq_disable(ab);
		ath12k_hif_ce_irq_disable(ab);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
		if (ab->dp->ppe.ppe_ops &&
		    ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop)
			ab->dp->ppe.ppe_ops->ath12k_ppeds_interrupt_stop(ab);
#endif
		ath12k_qmi_firmware_stop(ab);
		ath12k_core_cleanup(ab);
		if (ab->is_cumac_chip) {
			ab->is_cumac_chip = false;
			ag->cumac_selected = false;
			ag->cumac_chip_id = ATH12K_CUMAC_CHIP_ID_INVALID;
		}
		ab->cumac_configured = false;
		total_vdevs = ath12k_core_get_total_num_vdevs(ab);
		ab->free_vdev_map = (1LL << (ab->num_radios * total_vdevs)) - 1;
		ab->free_vdev_stats_id_map = 0;
		ath12k_core_to_group_ref_put(ab);
		ath12k_qmi_free_resource(ab);
		ath12k_hif_power_down(ab, false);
		ath12k_info(ab, "Q6 power down\n");
	}
}

void ath12k_core_cleanup_power_down_q6(struct ath12k_hw_group *ag, bool standby_mode)
{
	struct ath12k_hw *ah;
	struct ath12k_base *ab;
	struct ath12k_base *cumac_ab = NULL;
	bool wifi_standby_enabled = false;
	unsigned long time_left;
	int i, ret;

	if (!test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags)) {
		reinit_completion(&ag->umac_reset_complete);
		for (i = 0; i < ag->num_hw; i++) {
			ah = ag->ah[i];
			if (!ah)
				continue;

			ret = ath12k_mac_mlo_standby_teardown(ah, standby_mode);
			if (ret) {
				if (standby_mode)
					ath12k_err(NULL, "mlo teardown is failed for ERP\n");
				else
					ath12k_err(NULL, "mlo teardown is failed\n");
				return;
			}
		}

		/* Use first ab to fetch the umac reset timeout */
		ab = ag->ab[0];
		time_left = wait_for_completion_timeout(&ag->umac_reset_complete,
				msecs_to_jiffies(DP_UMAC_RESET_TIMEOUT_IN_MS));
		if (!time_left) {
			ath12k_err(NULL, "UMAC reset didn't get completed within %d ms\n",
				   DP_UMAC_RESET_TIMEOUT_IN_MS);
			ag->trigger_umac_reset = false;
			return;
		}
	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_cumac_chip) {
			cumac_ab = ab;
			continue;
		}

		ath12k_core_power_down_device(ag, ab, standby_mode,
					      &wifi_standby_enabled);
	}

	if (cumac_ab)
		ath12k_core_power_down_device(ag, cumac_ab, standby_mode,
					      &wifi_standby_enabled);

	if (!test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags))
		set_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);
}

static void ath12k_core_stop(struct ath12k_base *ab)
{
	ath12k_core_to_group_ref_put(ab);
	ath12k_acpi_stop(ab);
	if (!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags) &&
	    !ab->umcmn_fatal_received)
		ath12k_hif_stop(ab);
	ath12k_wmi_detach(ab);
	ath12k_mgmt_device_deinit(ab->mgmt);
	ath12k_dp_cmn_device_deinit(ab->dp);

	/* De-Init of components as needed */
}

static void ath12k_core_check_cc_code_bdfext(const struct dmi_header *hdr, void *data)
{
	struct ath12k_base *ab = data;
	const char *magic = ATH12K_SMBIOS_BDF_EXT_MAGIC;
	struct ath12k_smbios_bdf *smbios = (struct ath12k_smbios_bdf *)hdr;
	ssize_t copied;
	size_t len;
	int i;

	if (ab->qmi.target.bdf_ext[0] != '\0')
		return;

	if (hdr->type != ATH12K_SMBIOS_BDF_EXT_TYPE)
		return;

	if (hdr->length != ATH12K_SMBIOS_BDF_EXT_LENGTH) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "wrong smbios bdf ext type length (%d).\n",
			   hdr->length);
		return;
	}

	spin_lock_bh(&ab->base_lock);

	switch (smbios->country_code_flag) {
	case ATH12K_SMBIOS_CC_ISO:
		ab->new_alpha2[0] = u16_get_bits(smbios->cc_code, 0xff);
		ab->new_alpha2[1] = u16_get_bits(smbios->cc_code, 0xff);
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot smbios cc_code %c%c\n",
			   ab->new_alpha2[0], ab->new_alpha2[1]);
		break;
	case ATH12K_SMBIOS_CC_WW:
		ab->new_alpha2[0] = '0';
		ab->new_alpha2[1] = '0';
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot smbios worldwide regdomain\n");
		break;
	default:
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "boot ignore smbios country code setting %d\n",
			   smbios->country_code_flag);
		break;
	}

	spin_unlock_bh(&ab->base_lock);

	if (!smbios->bdf_enabled) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "bdf variant name not found.\n");
		return;
	}

	/* Only one string exists (per spec) */
	if (memcmp(smbios->bdf_ext, magic, strlen(magic)) != 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "bdf variant magic does not match.\n");
		return;
	}

	len = min_t(size_t,
		    strlen(smbios->bdf_ext), sizeof(ab->qmi.target.bdf_ext));
	for (i = 0; i < len; i++) {
		if (!isascii(smbios->bdf_ext[i]) || !isprint(smbios->bdf_ext[i])) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "bdf variant name contains non ascii chars.\n");
			return;
		}
	}

	/* Copy extension name without magic prefix */
	copied = strscpy(ab->qmi.target.bdf_ext, smbios->bdf_ext + strlen(magic),
			 sizeof(ab->qmi.target.bdf_ext));
	if (copied < 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "bdf variant string is longer than the buffer can accommodate\n");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT,
		   "found and validated bdf variant smbios_type 0x%x bdf %s\n",
		   ATH12K_SMBIOS_BDF_EXT_TYPE, ab->qmi.target.bdf_ext);
}

int ath12k_core_check_smbios(struct ath12k_base *ab)
{
	ab->qmi.target.bdf_ext[0] = '\0';
	dmi_walk(ath12k_core_check_cc_code_bdfext, ab);

	if (ab->qmi.target.bdf_ext[0] == '\0')
		return -ENODATA;

	return 0;
}

static int ath12k_core_soc_create(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret;

	if (ath12k_ftm_mode) {
		ab->fw_mode = ATH12K_FIRMWARE_MODE_FTM;
		ath12k_info(ab, "Booting in ftm mode\n");
	} else if (ath12k_waltest_mode) {
		ab->fw_mode = ATH12K_FIRMWARE_MODE_WALTEST;
		ath12k_info(ab, "Booting in waltest mode\n");
	}

	ret = ath12k_qmi_init_service(ab);
	if (ret) {
		ath12k_err(ab, "failed to initialize qmi :%d\n", ret);
		return ret;
	}

	ath12k_debugfs_soc_create(ab);

	ret = ath12k_hif_power_up(ab);
	if (ret) {
		ath12k_err(ab, "failed to power up :%d\n", ret);
		goto err_qmi_deinit;
	}

	return 0;

err_qmi_deinit:
	ath12k_debugfs_soc_destroy(ab);
	mutex_unlock(&ag->mutex);
	ath12k_qmi_deinit_service(ab);
	mutex_lock(&ag->mutex);
	return ret;
}

static void ath12k_core_soc_destroy(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (!test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags) && !ab->is_bypassed)
		ath12k_qmi_firmware_stop(ab);

	if (ab->ce_pipe_init_done && !ab->is_bypassed)
		ath12k_ce_cleanup_pipes(ab);

	ath12k_hif_power_down(ab, false);

	ath12k_reg_free(ab);
	ath12k_debugfs_soc_destroy(ab);
	mutex_unlock(&ag->mutex);
	ath12k_qmi_deinit_service(ab);
	mutex_lock(&ag->mutex);
}

static int ath12k_core_mlo_shmem_per_device_crash_info_addresses(
		struct ath12k_base *ab,
		struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info)
{
	int i;
	struct ath12k_host_mlo_glb_per_device_crash_info *per_device_crash_info = NULL;

	for (i = 0; i < global_device_crash_info->no_of_devices; i++)
	{
		per_device_crash_info = &global_device_crash_info->per_device_crash_info[i];

		if (!per_device_crash_info)
			return -EINVAL;

		if (ab->device_id == per_device_crash_info->device_id)
			break;
	}

	if (i >= global_device_crash_info->no_of_devices) {
		ath12k_err(ab, "error in chip id:%d\n", ab->device_id);
		return 0;
	}

	if (!per_device_crash_info ||
	    !per_device_crash_info->crash_reason ||
	    !per_device_crash_info->recovery_mode) {
		ath12k_err(ab, "crash_reason address is null\n");
		return 0;
	}

	ab->crash_info_address = per_device_crash_info->crash_reason;
	ab->recovery_mode_address = per_device_crash_info->recovery_mode;

	return 0;
}

static int ath12k_core_mlo_shmem_crash_info_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_host_mlo_mem_arena *mlomem_arena_ctx;
	struct ath12k_host_mlo_glb_device_crash_info *global_device_crash_info;

	mlomem_arena_ctx = &ab->ag->mlomem_arena;

	if (!(ag->mlo_mem.is_mlo_mem_avail))
		return 0;

	global_device_crash_info = &mlomem_arena_ctx->global_device_crash_info;

	if (ath12k_core_mlo_shmem_per_device_crash_info_addresses(ab,
				global_device_crash_info) < 0) {
		ath12k_warn(ab, "per_device_crash_info is not set\n");
		return -EINVAL;
	}

	return 0;
}

static int ath12k_core_pdev_init(struct ath12k_base *ab)
{
	ath12k_fse_init(ab);
	ath12k_telemetry_init(ab);

	ath12k_dp_accel_cfg_init(ab);

	ath12k_thermal_register(ab);

	/* Initialize spectral only if supported by the active memory profile */
	if (ab->mem_params.spectral_support)
		ath12k_spectral_init(ab);

	/* Check if cfr_enable_bmap is set for the corresponding HW and
	 * CFR is supported by the active memory profile
	 */
	if (ab->mem_params.cfr_support &&
	    (ath12k_cfr_enable_bmap & (1 << ab->device_id))) {
		ath12k_info(ab, "Enabling CFR for chip id:%d\n", ab->device_id);
		ath12k_cfr_init(ab);
	}

	ath12k_dp_mon_spl_pkt_cap_config(ab);

	return 0;
}

void ath12k_core_pdev_deinit(struct ath12k_base *ab)
{
	if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
		return;

	ath12k_dp_accel_cfg_deinit(ab);

	ath12k_fse_deinit(ab);
	ath12k_telemetry_deinit(ab);
	ath12k_thermal_unregister(ab);

	/* Deinitialize spectral only if it was initialized based on memory profile */
	if (ab->mem_params.spectral_support)
		ath12k_spectral_deinit(ab);

	/* Deinitialize CFR only if it was initialized based on memory profile
	 * and cfr_enable_bmap
	 */
	if (ab->mem_params.cfr_support &&
	    (ath12k_cfr_enable_bmap & (1 << ab->device_id)))
		ath12k_cfr_deinit(ab);
}

static int ath12k_core_gpio_init(struct ath12k *ar)
{
	int i;

	for (i = 0; i < 32; i++) {
		ar->radio_cfg.gpio_cfg[i].configured     = false;
		ar->radio_cfg.gpio_cfg[i].gpio_pin       = i;
		ar->radio_cfg.gpio_cfg[i].gpio_function  = 0;
		ar->radio_cfg.gpio_cfg[i].gpio_pull_type = QCA_WLAN_GPIO_PULL_NONE;
		ar->radio_cfg.gpio_cfg[i].gpio_dir       = QCA_WLAN_GPIO_INPUT;
		ar->radio_cfg.gpio_cfg[i].gpio_intr_mode = QCA_WLAN_GPIO_INTMODE_DISABLE;
		ar->radio_cfg.gpio_cfg[i].gpio_value     = 0;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_BOOT,
		   "GPIO configuration cache initialized\n");

	return 0;
}

static int ath12k_core_pdev_create(struct ath12k_base *ab)
{
	int ret, i;

	ret = ath12k_dp_arch_pdev_alloc(ab->dp);
	if (ret) {
		ath12k_err(ab, "failed to attach DP pdev: %d\n", ret);
		goto err_pdev_debug;
	}

	/* Initialize GPIO configuration cache for each radio */
	for (i = 0; i < ab->num_radios; i++) {
		ret = ath12k_core_gpio_init(ab->pdevs[i].ar);
		if (ret) {
			ath12k_err(ab, "failed to initialize GPIO for radio %d: %d\n",
				   i, ret);
			goto err_pdev_debug;
		}
	}

	return 0;

err_pdev_debug:
	ath12k_debugfs_pdev_destroy(ab);
	return ret;
}

static void ath12k_core_pdev_destroy(struct ath12k_base *ab)
{
	ath12k_dp_arch_pdev_free(ab->dp);
	ath12k_debugfs_pdev_destroy(ab);
}

static void ath12k_core_dump_mem_profile_info(struct ath12k_base *ab)
{
	const struct ath12k_mem_profile_based_param *cfg = &ab->mem_params;

	ath12k_info(ab, "Station counts: SINGLE=%u DBS=%u DBS_SBS=%u\n",
		    TARGET_NUM_STATIONS_SINGLE,
		    TARGET_NUM_STATIONS_DBS,
		    TARGET_NUM_STATIONS_DBS_SBS);

	ath12k_info(ab, "Station counts: hw_max=[%u,%u,%u]\n",
		    ab->hw_params->max_clients_supported,
		    ab->hw_params->max_clients_dbs,
		    ab->hw_params->max_clients_dbs_sbs);

	ath12k_info(ab,
		    "mem_params: profile=%d rxdma_buf=%u rx_rel=%u reo2ppe=%u ppe2tcl=%u\n",
		    ath12k_active_mem_profile,
		    cfg->dp_params.rxdma_buf_ring_size,
		    cfg->dp_params.rx_release_ring_size,
		    cfg->dp_params.reo2ppe_ring,
		    cfg->dp_params.ppe2tcl_ring);

	ath12k_info(ab,
		    "mem_params: tqm2ppe=%u tx_comp_ppeds=%u vdevs=%u bridge_vdevs=%u nlink_vdevs=%u\n",
		    cfg->dp_params.tqm2ppe_ring_size,
		    cfg->dp_params.tx_comp_ppeds_ring_size,
		    cfg->num_vdevs,
		    cfg->num_bridge_vdevs,
		    cfg->num_max_vdevs_nlink);

	ath12k_info(ab,
		    "mem_params: mem_mode=%u ppe_wbm2sw=%u mon_buf=%u\n",
		    cfg->target_mem_mode,
		    cfg->dp_params.ppe_wbm2sw_ring_size,
		    cfg->dp_params.rxdma_monitor_buf_ring_size);

	ath12k_info(ab,
		    "mem_params: tx_desc_count=[%u,%u,%u,%u,%u]\n",
		    cfg->dp_params.tx_desc_count[0],
		    cfg->dp_params.tx_desc_count[1],
		    cfg->dp_params.tx_desc_count[2],
		    cfg->dp_params.tx_desc_count[3],
		    cfg->dp_params.tx_desc_count[4]);

	ath12k_info(ab,
		    "mem_params: mon_dst=%u smart_mon=0x%x ppdu=%u rx_desc=%u\n",
		    cfg->dp_params.rxdma_monitor_dst_ring_size,
		    cfg->dp_params.smart_mon_filter_default,
		    cfg->dp_params.mon_num_ppdu_desc,
		    cfg->dp_params.rx_desc_count);

	ath12k_info(ab,
		    "mem_params: tx_mon : buf_sz=%u fill_lvl=%u dst_sz=%u num_ppdu=%u status_buf=%u spc_bitmap=%u\n",
		    cfg->dp_params.tx_monitor_buf_ring_size,
		    cfg->dp_params.tx_monitor_buf_ring_fill_lvl,
		    cfg->dp_params.tx_monitor_dst_ring_size,
		    cfg->dp_params.tx_monitor_num_ppdu_desc,
		    cfg->dp_params.tx_monitor_num_status_buf,
		    cfg->dp_params.tx_monitor_spc_bitmap);

	ath12k_info(ab,
		    "mem_params: sta_single=%u sta_dbs=%u sta_dbs_sbs=%u\n",
		    cfg->num_stations_single,
		    cfg->num_stations_dbs,
		    cfg->num_stations_dbs_sbs);

	ath12k_info(ab,
		    "mem_params: max_clients=%u\n",
		    cfg->dp_params.dp_max_clients);

	ath12k_info(ab,
		    "mem_params: ppeds_tx=%u ppeds_hotlist=%u\n",
		    cfg->dp_params.num_pool_ppeds_tx_desc,
		    cfg->dp_params.ppeds_hotlist_len_max);

	ath12k_info(ab, "mem_params: reo_dst=[%u,%u,%u,%u,%u]\n",
		    cfg->dp_params.reo_dst_ring_size[0],
		    cfg->dp_params.reo_dst_ring_size[1],
		    cfg->dp_params.reo_dst_ring_size[2],
		    cfg->dp_params.reo_dst_ring_size[3],
		    cfg->dp_params.reo_dst_ring_size[4]);

	ath12k_info(ab, "mem_params: tcl=[%u,%u,%u,%u,%u]\n",
		    cfg->dp_params.tcl_data_ring_size[0],
		    cfg->dp_params.tcl_data_ring_size[1],
		    cfg->dp_params.tcl_data_ring_size[2],
		    cfg->dp_params.tcl_data_ring_size[3],
		    cfg->dp_params.tcl_data_ring_size[4]);

	ath12k_info(ab, "mem_params: tx_compl=[%u,%u,%u,%u,%u] mon_sup=%u\n",
		    cfg->dp_params.tx_compl_ring_size[0],
		    cfg->dp_params.tx_compl_ring_size[1],
		    cfg->dp_params.tx_compl_ring_size[2],
		    cfg->dp_params.tx_compl_ring_size[3],
		    cfg->dp_params.tx_compl_ring_size[4],
		    cfg->monitor_support);

	ath12k_info(ab,
		    "mem_params: cfr=%u spectral=%u tx_mon=%u\n",
		    cfg->cfr_support,
		    cfg->spectral_support,
		    cfg->tx_monitor_support);

	ath12k_info(ab,
		    "mem_params: dp_num_clients_max=%u dp_mon_status_buf=%u\n",
		    cfg->dp_params.dp_num_clients_max,
		    cfg->dp_params.dp_mon_status_buf);
}

static int ath12k_core_start(struct ath12k_base *ab)
{
	int ret;

	lockdep_assert_held(&ab->core_lock);

	ret = ath12k_wmi_attach(ab);
	if (ret) {
		ath12k_err(ab, "failed to attach wmi: %d\n", ret);
		return ret;
	}

	ret = ath12k_htc_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to init htc: %d\n", ret);
		goto err_wmi_detach;
	}

	if (!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags) &&
	    !ab->umcmn_fatal_received) {
		ret = ath12k_hif_start(ab);
		if (ret) {
			ath12k_err(ab, "failed to start HIF: %d\n", ret);
			goto err_wmi_detach;
		}
	} else {
		ath12k_hif_ce_irq_enable(ab);
	}

	ret = ath12k_htc_wait_target(&ab->htc);
	if (ret) {
		ath12k_err(ab, "failed to connect to HTC: %d\n", ret);
		goto err_hif_stop;
	}

	ret = ath12k_dp_htt_connect(ath12k_ab_to_dp(ab));
	if (ret) {
		ath12k_err(ab, "failed to connect to HTT: %d\n", ret);
		goto err_hif_stop;
	}
	ret = ath12k_wmi_connect(ab);
	if (ret) {
		ath12k_err(ab, "failed to connect wmi: %d\n", ret);
		goto err_hif_stop;
	}
	ret = ath12k_htc_start(&ab->htc);
	if (ret) {
		ath12k_err(ab, "failed to start HTC: %d\n", ret);
		goto err_hif_stop;
	}

	ret = ath12k_wmi_wait_for_service_ready(ab);
	if (ret) {
		ath12k_err(ab, "failed to receive wmi service ready event: %d\n",
			   ret);
		goto err_hif_stop;
	}

	ath12k_hal_cc_config(ab);

	ret = ath12k_wmi_cmd_init(ab);
	if (ret) {
		ath12k_err(ab, "failed to send wmi init cmd: %d\n", ret);
		goto err_hif_stop;
	}

	ret = ath12k_wmi_wait_for_unified_ready(ab);
	if (ret) {
		ath12k_err(ab, "failed to receive wmi unified ready event: %d\n",
			   ret);
		goto err_hif_stop;
	}

	WARN_ON(test_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags));
	set_bit(ATH12K_FLAG_WMI_INIT_DONE, &ab->dev_flags);

	/* put hardware to DBS mode */
	if (ab->hw_params->single_pdev_only) {
		ret = ath12k_wmi_set_hw_mode(ab, WMI_HOST_HW_MODE_DBS);
		if (ret) {
			ath12k_err(ab, "failed to send dbs mode: %d\n", ret);
			goto err_hif_stop;
		}
	}

	ret = ath12k_dp_tx_htt_h2t_ver_req_msg(ab);
	if (ret) {
		ath12k_err(ab, "failed to send htt version request message: %d\n",
			   ret);
		goto err_hif_stop;
	}

	ath12k_acpi_set_dsm_func(ab);

	/* Indicate the core start in the appropriate group */
	ath12k_core_to_group_ref_get(ab);

	/* Skip FST init when IPA arch overrides per-peer RDI routing.
	 * With IX-register-based routing (WiFi8 IPA), all data traffic is
	 * steered to reo2sw5 via IX_0 without per-flow FSE rules.  Sending
	 * HTT_H2T_MSG_TYPE_RX_FSE_SETUP_CFG would needlessly enable the FSE
	 * cache in firmware, interfering with the IX-based approach.
	 */
#ifdef CPTCFG_QCN_EXTN
	if (ath12k_cfg_get(ab, ATH12K_INI_DP_IPA_SFE_ENABLE))
		ath12k_dp_rx_fst_init(ab);
#else
	ath12k_dp_rx_fst_init(ab);
#endif /* CPTCFG_QCN_EXTN */
	ath12k_wsi_load_info_wsiorder_update(ab);

	ath12k_core_dump_mem_profile_info(ab);

	return 0;

err_hif_stop:
	ath12k_hif_stop(ab);
err_wmi_detach:
	ath12k_wmi_detach(ab);
	return ret;
}

static void ath12k_core_device_cleanup(struct ath12k_base *ab)
{
	mutex_lock(&ab->core_lock);

	ath12k_hif_mgmt_irq_disable(ab);
	ath12k_hif_irq_disable(ab);
	ath12k_core_pdev_destroy(ab);
	ath12k_dp_umac_reset_deinit(ab);
	mutex_unlock(&ab->core_lock);
}

static void ath12k_stats_event_work_handler(struct wiphy *wiphy,
					    struct wiphy_work *work)
{
	struct ath12k_stats_work_context *stats_ctx;
	struct ath12k_stats_list_entry *list_entry;
	struct list_head temp_list;

	stats_ctx = container_of(work, struct ath12k_stats_work_context,
				 stats_nb_work);

	INIT_LIST_HEAD(&temp_list);

	spin_lock(&stats_ctx->list_lock);
	list_splice_tail_init(&stats_ctx->work_list, &temp_list);
	spin_unlock(&stats_ctx->list_lock);

	while (!list_empty(&temp_list)) {
		list_entry = list_first_entry(&temp_list,
					      struct ath12k_stats_list_entry,
					      node);

		ath12k_wifi_stats_reply_setup(&list_entry->usr_command);

		list_del(&list_entry->node);
		kfree(list_entry);
	}
}

static void ath12k_stats_event_work_free(struct ath12k_stats_work_context *stats_ctx)
{
	struct ath12k_stats_list_entry *list_entry;

	spin_lock(&stats_ctx->list_lock);
	while (!list_empty(&stats_ctx->work_list)) {
		list_entry = list_first_entry(&stats_ctx->work_list,
					      struct ath12k_stats_list_entry,
					      node);

		list_del(&list_entry->node);
		kfree(list_entry);
	}
	spin_unlock(&stats_ctx->list_lock);
}

static void ath12k_core_hw_group_stop(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	struct ath12k_hw *ah = ag->ah[0];
	int i;

	lockdep_assert_held(&ag->mutex);

	clear_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags);
	cancel_work_sync(&ag->reset_group_work);

	for (i = ag->num_devices - 1; i >= 0; i--) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

#ifdef CPTCFG_QCN_EXTN
		ath12k_core_nol_extn_auto_dump_all_radios(ab);
#endif /* CPTCFG_QCN_EXTN */

		mutex_lock(&ab->core_lock);
		ath12k_core_pdev_deinit(ab);
		mutex_unlock(&ab->core_lock);
	}

	wiphy_work_cancel(ah->hw->wiphy, &ag->stats_work.stats_nb_work);
	ath12k_stats_event_work_free(&ag->stats_work);

	ath12k_mac_unregister(ag);

	ath12k_core_cu_mem_pool_deinit(ag);

	ath12k_mac_mlo_teardown(ag);

	for (i = ag->num_devices - 1; i >= 0; i--) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		clear_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags);

		ath12k_core_device_cleanup(ab);

		if (ab->hw_params->reoq_lut_support &&
		    !test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags)) {
			mutex_lock(&ab->core_lock);
			ath12k_dp_reoq_lut_addr_reset(ath12k_ab_to_dp(ab));
			mutex_unlock(&ab->core_lock);
		}
	}

	ath12k_mac_destroy(ag);
}

u8 ath12k_get_num_partner_link(struct ath12k *ar)
{
	struct ath12k_base *partner_ab, *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_pdev *pdev;
	u8 num_link = 0;
	int i, j;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		for (j = 0; j < partner_ab->num_radios; j++) {
			pdev = &partner_ab->pdevs[j];

			/* Avoid the self link */
			if (ar == pdev->ar)
				continue;

			num_link++;
		}
	}

	return num_link;
}

static int __ath12k_mac_mlo_ready(struct ath12k *ar)
{
	u8 num_link = ath12k_get_num_partner_link(ar);
	int ret;

	if (num_link == 0)
		return 0;

	ret = ath12k_wmi_mlo_ready(ar);
	if (ret) {
		ath12k_err(ar->ab, "MLO ready failed for pdev %d: %d\n",
			   ar->pdev_idx, ret);
		return ret;
	}

	ar->teardown_complete_event = false;
	ath12k_dbg(ar->ab, ATH12K_DBG_MAC, "mlo ready done for pdev %d\n",
		   ar->pdev_idx);

	return 0;
}

int ath12k_mac_mlo_ready(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int ret;
	int i, j;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ag->ah[i];
		if (!ah)
			continue;

		for_each_ar(ah, ar, j) {
			ar = &ah->radio[j];
			if (!ar || ar->ab->is_bypassed)
				continue;
			ret = __ath12k_mac_mlo_ready(ar);
			if (ret)
				return ret;
		}
	}

	return 0;
}

int ath12k_core_mlo_setup(struct ath12k_hw_group *ag)
{
	int ret;

	if (!ag->mlo_capable)
		return 0;

	ret = ath12k_mac_mlo_setup(ag);
	if (ret)
		return ret;

	ret = ath12k_mac_mlo_ready(ag);
	if (ret)
		goto err_mlo_teardown;

	return 0;

err_mlo_teardown:
	ath12k_mac_mlo_teardown(ag);

	return ret;
}
EXPORT_SYMBOL(ath12k_core_mlo_setup);

int ath12k_core_pdev_enable_telemetry_stats(struct ath12k_base *ab)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int ret, i;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		if (pdev) {
			ar = pdev->ar;
			ret = ath12k_wmi_pdev_enable_telemetry_stats(ab, ar, NULL);
			if (ret) {
				ath12k_err(ab,
					   "Failed to enable pdev telemetry stats for pdev id:%d\n",
					   pdev->pdev_id);
			} else {
				ath12k_info(ab,
					    "Enable pdev telemetry stats for pdev id: %d\n",
					    pdev->pdev_id);
			}
		}
	}

	return 0;
}

static bool ath12k_check_ab_preferred_band(struct ath12k_base *ab,
					   enum ath12k_cumac_band preferred_band)
{
	u32 supported_band;
	int i;

	if (!ab || ab->num_radios == 0)
		return false;

	for (i = 0; i < ab->num_radios; i++) {
		supported_band = ab->pdevs[i].cap.supported_bands;
		if (!supported_band)
			continue;

		switch (preferred_band) {
		case ATH12K_CUMAC_BAND_2GHZ:
			if (supported_band & WMI_HOST_WLAN_2GHZ_CAP)
				return true;
			break;
		case ATH12K_CUMAC_BAND_5GHZ:
			if ((supported_band & WMI_HOST_WLAN_5GHZ_CAP) &&
			    !(ab->hal_reg_cap[i].low_5ghz_chan >= ATH12K_MIN_6GHZ_FREQ &&
			      ab->hal_reg_cap[i].high_5ghz_chan <= ATH12K_MAX_6GHZ_FREQ))
				return true;
			break;
		case ATH12K_CUMAC_BAND_6GHZ:
			if ((supported_band & WMI_HOST_WLAN_5GHZ_CAP) &&
			    ab->hal_reg_cap[i].low_5ghz_chan >= ATH12K_MIN_6GHZ_FREQ &&
			    ab->hal_reg_cap[i].high_5ghz_chan <= ATH12K_MAX_6GHZ_FREQ)
				return true;
			break;
		default:
			break;
		}
	}

	return false;
}

static int ath12k_core_send_cumac_chip(struct ath12k_hw_group *ag)
{
	int ret = -EINVAL;
	int i, j;
	struct ath12k *ar;
	struct ath12k_base *ab = NULL;

	if (!ag)
		return ret;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		if (ab->cumac_configured)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;
			if (!ar)
				continue;

			reinit_completion(&ar->cumac_setup_done);
			ret = ath12k_wmi_send_cumac_config(ar, ag->cumac_chip_id);
			if (ret) {
				ath12k_err(ar->ab, "failed to send cumac chip command for pdev %d: %d\n",
					   ar->pdev_idx, ret);
				return ret;
			}

			ath12k_dbg_level(ar->ab, ATH12K_DBG_MAC, ATH12K_DBG_L1,
					 "Send cumac chip wmi command for pdev %d: ret %d\n",
					 ar->pdev_idx, ret);
			ar->cumac_cmd_sent = 1;
			break;
		}
	}

	return ret;
}

#define ATH12K_CUMAC_BAND_PRIO_MAX 3
#define ATH12K_CUMAC_BAND_PRIO_STR_LEN 32

static int ath12k_parse_cumac_band_priority(enum ath12k_cumac_band *prio_band,
					    struct ath12k_base *ab)
{
	char buf[ATH12K_CUMAC_BAND_PRIO_STR_LEN];
	char *p = buf, *token;
	int count = 0;
	bool seen[ATH12K_CUMAC_BAND_PRIO_MAX + 1] = {};

#ifdef CPTCFG_QCN_EXTN
	const char *ini_val = ab ? ath12k_cfg_get(ab,
					ATH12K_CFG_CUMAC_BAND_PRIORITY) : NULL;

	if (!ini_val || !ini_val[0])
		return 0;

	strscpy(buf, ini_val, sizeof(buf));
	ath12k_dbg(ab, ATH12K_DBG_BOOT,
		   "cumac_band_priority: using INI value \"%s\"\n", buf);
#else
	return 0;
#endif

	while ((token = strsep(&p, " \t")) != NULL) {
		enum ath12k_cumac_band band;

		if (!*token)
			continue;

		if (strcasecmp(token, "2GHz") == 0)
			band = ATH12K_CUMAC_BAND_2GHZ;
		else if (strcasecmp(token, "5GHz") == 0)
			band = ATH12K_CUMAC_BAND_5GHZ;
		else if (strcasecmp(token, "6GHz") == 0)
			band = ATH12K_CUMAC_BAND_6GHZ;
		else {
			ath12k_warn(ab, "unknown cumac_band_priority token \"%s\", ignoring\n",
				    token);
			continue;
		}

		if (seen[band])
			continue;

		seen[band] = true;
		prio_band[count++] = band;

		if (count == ATH12K_CUMAC_BAND_PRIO_MAX)
			break;
	}

	return count;
}

static int ath12k_select_cumac_chip(struct ath12k_hw_group *ag)
{
	struct ath12k_base *partner_ab;
	struct ath12k_base *cumac_ab = NULL;
	int i, j;
	u8 best_chip_prio;
	enum ath12k_cumac_band preferred_cumac_band = ATH12K_CUMAC_BAND_NONE;
	enum ath12k_cumac_band default_prio_band[] = {ATH12K_CUMAC_BAND_6GHZ,
						      ATH12K_CUMAC_BAND_5GHZ,
						      ATH12K_CUMAC_BAND_2GHZ};
	enum ath12k_cumac_band override_prio_band[ATH12K_CUMAC_BAND_PRIO_MAX];
	enum ath12k_cumac_band *prio_band;
	u32 max_prio_order;
	int override_count;

	if (!ag)
		return -EINVAL;

	if (ag->cumac_selected)
		return 0;

	/* Find the first available ab for INI lookup. */
	partner_ab = NULL;
	for (i = 0; i < ag->num_devices; i++) {
		if (ag->ab[i] && !ag->ab[i]->is_bypassed) {
			partner_ab = ag->ab[i];
			break;
		}
	}
	override_count = ath12k_parse_cumac_band_priority(override_prio_band,
							  partner_ab);
	if (override_count > 0) {
		prio_band = override_prio_band;
		max_prio_order = override_count;
		ath12k_info(NULL,
			    "CUMAC chip selection: using INI band priority (%d entries)\n",
			    override_count);
	} else {
		prio_band = default_prio_band;
		max_prio_order = ARRAY_SIZE(default_prio_band);
	}

	if (ag->num_devices == 1) {
		cumac_ab = ag->ab[0];
		goto select_cumac;
	}

	best_chip_prio = U8_MAX;
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab || partner_ab->is_bypassed)
			continue;
		if (partner_ab->hw_params->cumac_chip_priority &&
		    (partner_ab->hw_params->cumac_chip_priority < best_chip_prio))
			best_chip_prio = partner_ab->hw_params->cumac_chip_priority;
	}

	for (j = 0; j < max_prio_order; j++) {
		preferred_cumac_band = prio_band[j];
		for (i = 0; i < ag->num_devices; i++) {
			partner_ab = ag->ab[i];
			if (!partner_ab)
				continue;

			if (partner_ab->hw_params->cumac_chip_priority != best_chip_prio)
				continue;

			if (ath12k_check_ab_preferred_band(partner_ab,
							   preferred_cumac_band)) {
				cumac_ab = partner_ab;
				break;
			}
		}
		if (cumac_ab)
			break;
	}

select_cumac:
	if (!cumac_ab)
		return -EINVAL;

	cumac_ab->is_cumac_chip = true;
	ag->cumac_chip_id = cumac_ab->device_id;
	ag->cumac_selected = true;
	ath12k_info(cumac_ab,
		    "Selected CUMAC chip: chip_id=%d band=%d\n",
		    cumac_ab->device_id,
		    preferred_cumac_band);

	return 0;
}

#define ATH12K_CUMAC_SETUP_TIMEOUT_HZ (3 * HZ)
inline int ath12k_wait_for_cumac_completion(struct ath12k_hw_group *ag)
{
	int ret = 0;
	int i, j;
	struct ath12k_base *ab;
	struct ath12k *ar = NULL;

	if (!ag)
		return -EINVAL;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		if (ab->cumac_configured)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;
			if (!ar)
				continue;

			if (!ar->cumac_cmd_sent)
				continue;

			if (!wait_for_completion_timeout(&ar->cumac_setup_done,
							 ATH12K_CUMAC_SETUP_TIMEOUT_HZ))
				ret = -ETIMEDOUT;
			break;
		}

		if (ret) {
			ath12k_err(ab, "Failed to received cumac chip completion\n");
			return ret;
		}

		ab->cumac_configured = 1;
	}

	return ret;
}

static int ath12k_core_complete_cumac_config(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = NULL;
	int ret;
	int i;

	if (!ag->cumac_enabled)
		return 0;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (ab && !ab->is_bypassed)
			break;
		ab = NULL;
	}

	if (!ab) {
		ath12k_err(NULL, "No valid device found for CUMAC config\n");
		return -EINVAL;
	}

	if (!test_bit(WMI_SERVICE_PDEV_SET_CUMAC_CHIP_CMD_SUPPORT,
		      ab->wmi_ab.svc_map) || ath12k_ftm_mode || !ag->mlo_capable) {
		/* FW does not support the CUMAC chip WMI command; hardcode
		 * chip id 0 as the CUMAC chip and skip the WMI send path.
		 */
		ab = ag->ab[0];
		if (!ab || ab->is_bypassed) {
			ath12k_err(NULL, "CUMAC chip (device id 0) is unavailable or bypassed\n");
			return -EINVAL;
		}

		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "FTM mode or MLO not capable or FW does not support CUMAC chip cmd, using chip id 0\n");
		ag->cumac_chip_id = 0;
		ag->cumac_selected = true;
		ab->is_cumac_chip = true;
		return 0;
	}

	ret = ath12k_select_cumac_chip(ag);
	if (ret) {
		ath12k_err(NULL, "CUMAC chip selection failed: %d\n", ret);
		return ret;
	}

	if (!ag->cumac_selected) {
		ath12k_err(NULL, "CUMAC selection failed for CUMAC enabled target\n");
		return -EINVAL;
	}

	ret = ath12k_core_send_cumac_chip(ag);
	if (ret) {
		ath12k_err(NULL, "cumac configuration failed, soc bring up failed %d\n",
			   ret);
		return ret;
	}

	ret = ath12k_wait_for_cumac_completion(ag);
	if (ret) {
		ath12k_err(NULL, "cumac completion not received, bring up failed %d\n",
			   ret);
		return ret;
	}

	return 0;
}

static void ath12k_core_mlo_deinit(struct ath12k_hw_group *ag)
{
	struct ath12k_base *partner_ab;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (ath12k_ftm_mode || !partner_ab || partner_ab->is_bypassed)
			continue;

		if (partner_ab->mgmt)
			ath12k_mgmt_device_deinit(partner_ab->mgmt);

		if (partner_ab->dp)
			ath12k_dp_arch_op_mlo_deinit(partner_ab->dp);
	}
}

static int ath12k_core_mlo_init(struct ath12k_hw_group *ag)
{
	struct ath12k_base *partner_ab;
	int ret, i;

	/* DP/MGMT MLO init must complete before mac80211 registration so that
	 * cumac_dp is set before userspace can trigger vdev creation.
	 */
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];

		if (ath12k_ftm_mode || !partner_ab || partner_ab->is_bypassed)
			continue;

		if (partner_ab->dp) {
			ret = ath12k_dp_arch_op_mlo_init(partner_ab->dp);
			if (ret) {
				ath12k_err(partner_ab, "DP MLO init failed\n");
				return ret;
			}
		}

		if (partner_ab->mgmt) {
			ret = ath12k_mgmt_device_init(partner_ab->mgmt);
			if (ret) {
				ath12k_err(partner_ab, "MGMT MLO init failed\n");
				return ret;
			}
		}
	}
	return 0;
}

static int ath12k_dp_umac_reset_init_wrapper(struct ath12k_hw_group *ag)
{
	int i, ret;
	struct ath12k_base *ab;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		if (ab->umac_reset_init_done)
			continue;

		if (ag->wsi_remap_in_progress &&
		    ab->wsi_remap_state != ATH12K_WSI_BYPASS_ADD_DEVICE)
			continue;

		if (ath12k_check_erp_power_down(ag) && !ab->powerup_triggered)
			continue;

		mutex_lock(&ab->core_lock);

		ret = ath12k_dp_umac_reset_init(ab);
		if (ret) {
			ath12k_warn(ab, "Failed to initialize UMAC RESET for device %d: %d\n",
				    i, ret);
			mutex_unlock(&ab->core_lock);
			return ret;
		}

		mutex_unlock(&ab->core_lock);
	}

	return 0;
}

static int ath12k_core_hw_group_start(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = NULL;
	int ret, i;

	lockdep_assert_held(&ag->mutex);

	if (test_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags)) {
		ret = ath12k_core_complete_cumac_config(ag);
		if (WARN_ON(ret))
			goto err_mac_destroy;

		ret = ath12k_dp_umac_reset_init_wrapper(ag);
		if (ret)
			goto err_mac_destroy;

		ret = ath12k_core_mlo_setup(ag);
		if (WARN_ON(ret))
			goto err_mac_destroy;

		ret = ath12k_core_mlo_init(ag);
		if (WARN_ON(ret))
			goto err_mlo_teardown;

		goto core_pdev_create;
	}

	switch (ath12k_crypto_mode) {
	case ATH12K_CRYPT_MODE_SW:
		set_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ag->flags);
		set_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ag->flags);
		break;
	case ATH12K_CRYPT_MODE_HW:
		clear_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED, &ag->flags);
		clear_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ag->flags);
		break;
	default:
		ath12k_info(NULL, "invalid crypto_mode: %d\n", ath12k_crypto_mode);
		return -EINVAL;
	}

	if (ath12k_frame_mode == ATH12K_HW_TXRX_RAW)
		set_bit(ATH12K_GROUP_FLAG_RAW_MODE, &ag->flags);

	/* Skip mac allocate and register during WSI remap,
	 * as the devices will be already registered to mac
	 * and only driver add/delete will be done.
	 */

	if (!ag->wsi_remap_in_progress) {
		ret = ath12k_mac_allocate(ag);
		if (WARN_ON(ret))
			return ret;
	}

	ret = ath12k_core_complete_cumac_config(ag);
	if (WARN_ON(ret))
		goto err_mac_destroy;

	ret = ath12k_dp_umac_reset_init_wrapper(ag);
	if (ret)
		goto err_mac_destroy;

	ret = ath12k_core_mlo_setup(ag);
	if (WARN_ON(ret))
		goto err_mac_destroy;

	ret = ath12k_core_mlo_init(ag);
	if (WARN_ON(ret))
		goto err_mlo_teardown;

	ret = ath12k_core_cu_mem_pool_init(ag);
	if (ret)
		goto err_mlo_teardown;

	if (!ag->wsi_remap_in_progress) {
		ret = ath12k_mac_register(ag);
		if (WARN_ON(ret))
			goto err_mlo_teardown;
	}

	set_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags);

	spin_lock_init(&ag->qos.profile_lock);

core_pdev_create:
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		/* Skip pdev creation if WSI remap in progress and chip is not
		 * in bypass Add state, as the pdev for other chips will be
		 * already present.
		 */
		if (ag->wsi_remap_in_progress &&
		    ab->wsi_remap_state != ATH12K_WSI_BYPASS_ADD_DEVICE)
			continue;

		if (ath12k_check_erp_power_down(ag) && !ab->powerup_triggered)
			continue;

		mutex_lock(&ab->core_lock);

		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0 &&
		    !ab->recovery_start &&
		    ath12k_hw_group_recovery_in_progress(ag)) {
			mutex_unlock(&ab->core_lock);
			continue;
		}

		set_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags);

		ret = ath12k_core_pdev_create(ab);
		if (ret) {
			ath12k_err(ab, "failed to create pdev core %d\n", ret);
			mutex_unlock(&ab->core_lock);
			goto err;
		}

		ret = ath12k_core_pdev_init(ab);
		if (ret) {
			ath12k_err(ab, "failed to init pdev core %d\n", ret);
			mutex_unlock(&ab->core_lock);
			goto err;
		}

		ath12k_debugfs_pdev_create(ab);

		ret = ath12k_core_rfkill_config(ab);
		if (ret && ret != -EOPNOTSUPP) {
			mutex_unlock(&ab->core_lock);
			goto err;
		}

                ret = ath12k_core_mlo_shmem_crash_info_init(ab);
                if (ret) {
                        ath12k_err(ab, "failed to parse crash info %d\n", ret);
                }

		if (ath12k_en_fwlog == true) {
			ret = ath12k_enable_fwlog(ab);
			if (ret < 0)
				ath12k_err(ab, "failed to enable fwlog: %d\n", ret);
		}

		ath12k_core_pdev_enable_telemetry_stats(ab);

		mutex_unlock(&ab->core_lock);
	}

	return 0;

err:
	ath12k_core_hw_group_stop(ag);
	ath12k_core_mlo_deinit(ag);
	return ret;

err_mlo_teardown:
	ath12k_mac_mlo_teardown(ag);
	ath12k_core_mlo_deinit(ag);

err_mac_destroy:
	ath12k_mac_destroy(ag);

	return ret;
}

static int ath12k_core_start_firmware(struct ath12k_base *ab,
				      enum ath12k_firmware_mode mode)
{
#ifdef CPTCFG_ATHDEBUG
	int qdss_ret;
#endif
	int ret;

	ath12k_ce_get_shadow_config(ab, &ab->qmi.ce_cfg.shadow_reg_v3,
				    &ab->qmi.ce_cfg.shadow_reg_v3_len);

	ret = ath12k_qmi_firmware_start(ab, mode);
	if (ret) {
		ath12k_err(ab, "failed to send firmware start: %d\n", ret);
		return ret;
	}

	/*
	 * Configure QDSS tracing immediately after QMI-driven firmware start.
	 * At this point the FW is responsive to QMI and can accept QDSS
	 * configuration in both Mission and FTM modes.
	 */
#ifdef CPTCFG_ATHDEBUG
	if (ab->hw_params->en_qdsslog && !ath12k_waltest_mode &&
	    ath12k_boot_time_qdss) {
		ath12k_info(ab, "QDSS trace enabled\n");
		qdss_ret = athdbg_if_get_service(ab, ATHDBG_SRV_CONFIG_QDSS);
		if (qdss_ret < 0) {
			ath12k_err(ab, "failed to configure QDSS tracing: %d\n",
				   qdss_ret);
			ab->is_qdss_tracing = false;
		}
	}
#endif

	return ret;
}

static inline
bool ath12k_core_hw_group_start_ready(struct ath12k_hw_group *ag)
{
	lockdep_assert_held(&ag->mutex);

	/* If some device is in bypass state, consider num_bypassed counter
	 * to check the hw group ready.
	 */
	return (ag->num_started == (ag->num_devices - ag->num_bypassed));
}

static void ath12k_fw_stats_pdevs_free(struct list_head *head)
{
	struct ath12k_fw_stats_pdev *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

void ath12k_fw_stats_bcn_free(struct list_head *head)
{
	struct ath12k_fw_stats_bcn *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

static void ath12k_fw_stats_vdevs_free(struct list_head *head)
{
	struct ath12k_fw_stats_vdev *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

static void ath12k_fw_stats_vdev_extds_free(struct list_head *head)
{
	struct ath12k_fw_stats_vdev_extd *i, *tmp;

	list_for_each_entry_safe(i, tmp, head, list) {
		list_del(&i->list);
		kfree(i);
	}
}

void ath12k_fw_stats_init(struct ath12k *ar)
{
	INIT_LIST_HEAD(&ar->fw_stats.vdevs);
	INIT_LIST_HEAD(&ar->fw_stats.pdevs);
	INIT_LIST_HEAD(&ar->fw_stats.bcn);
	INIT_LIST_HEAD(&ar->fw_stats.vdev_extds);
	init_completion(&ar->fw_stats_complete);
	init_completion(&ar->fw_stats_done);
}

void ath12k_fw_stats_free(struct ath12k_fw_stats *stats)
{
	ath12k_fw_stats_pdevs_free(&stats->pdevs);
	ath12k_fw_stats_vdevs_free(&stats->vdevs);
	ath12k_fw_stats_bcn_free(&stats->bcn);
	ath12k_fw_stats_vdev_extds_free(&stats->vdev_extds);
}
EXPORT_SYMBOL(ath12k_fw_stats_free);

void ath12k_fw_stats_reset(struct ath12k *ar)
{
	spin_lock_bh(&ar->data_lock);
	ath12k_fw_stats_free(&ar->fw_stats);
	ar->fw_stats.num_vdev_recvd = 0;
	ar->fw_stats.num_bcn_recvd = 0;
	ar->fw_stats.num_vdev_extd_recvd = 0;
	spin_unlock_bh(&ar->data_lock);
}

void ath12k_core_wsi_remap_mlo_reconfig(struct ath12k_hw_group *ag)
{
	int i;
	struct ath12k_base *partner_ab;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab && !partner_ab->is_bypassed) {
			ath12k_dbg(partner_ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: Trigger MLO reconfig");
			ath12k_qmi_trigger_mlo_reconfig(partner_ab);
		}
	}
}
EXPORT_SYMBOL(ath12k_core_wsi_remap_mlo_reconfig);

int ath12k_wsi_bypass_precheck(struct ath12k_base *ab, unsigned int value)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	int i;

	if (!test_bit(WMI_TLV_SERVICE_DYNAMIC_WSI_REMAP_SUPPORT, ab->wmi_ab.svc_map)) {
		ath12k_err(ab, "Firmware doesn't support dynamic WSI remap\n");
		return -EOPNOTSUPP;
	}

	if (ab->ag->wsi_remap_in_progress) {
		ath12k_err(ab, "WSI remap already in progress..\n");
		return -EBUSY;
	}

	if (ath12k_hw_group_recovery_in_progress(ag)) {
		ath12k_err(ab, "SSR is in progress, cannot allow remap\n");
		return -EBUSY;
	}

	if ((!(test_bit(WMI_TLV_SERVICE_11BN, ab->wmi_ab.svc_map)) &&
	    ((ag->num_devices - ag->num_bypassed) == ATH12K_MIN_ACTIVE_CHIP_FOR_BYPASS) &&
	    value == ATH12K_WSI_BYPASS_REMOVE_DEVICE)) {
		ath12k_err(ab, "Min 2 Chip has to be active for EHT device.\n");
		return -EINVAL;
	}

	if (!ab->is_cumac_chip &&
	    ((!ab->is_bypassed && value == ATH12K_WSI_BYPASS_ADD_DEVICE) ||
	    (ab->is_bypassed && value == ATH12K_WSI_BYPASS_REMOVE_DEVICE))) {
		ath12k_err(ab, "Invalid operation\n");
		return -EINVAL;
	}

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar) {
			ath12k_err(ab, "Invalid Radio\n");
			return -EINVAL;
		}

		if (ab->is_cumac_chip && ar->pdev_suspend &&
		    value == ATH12K_WSI_BYPASS_REMOVE_DEVICE) {
			ath12k_err(ab, "Invalid operation: CUMAC already suspended\n");
			return -EINVAL;
		}

		if (ab->is_cumac_chip && !ar->pdev_suspend &&
		    value == ATH12K_WSI_BYPASS_ADD_DEVICE) {
			ath12k_err(ab, "Invalid operation: CUMAC not suspended\n");
			return -EINVAL;
		}

		if (ar->num_created_vdevs > 0) {
			ath12k_err(ab, "Vaps are active, cannot do bypass\n");
			return -EBUSY;
		}
	}

	return 0;
}

static int ath12k_core_wsi_remap_radio_start(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	int ret = 0, i;
	struct ath12k *ar = NULL;
	struct ath12k_bridge_iter bridge_iter = {};
	u8 active_num_devices;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;

		if (!ar) {
			ath12k_err(ab, "ar is NULL\n");
			ret = -EINVAL;
			goto exit;
		}

		ret = ath12k_mac_start(ar);
		if (ret) {
			ath12k_err(ar->ab, "vdev_id : %s radio_idx : %u] fail to start mac operations in pdev idx %d ret %d\n",
				   ATH12K_INVALID_VDEV_ID, ar->radio_idx,
				   ar->pdev_idx, ret);
			goto exit;
		}
	}

	active_num_devices = ag->num_devices - ag->num_bypassed;
	if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_ADD_DEVICE &&
	    active_num_devices == ATH12K_MIN_NUM_DEVICES_NLINK && ar) {
		void (*iterator)(void *data,
				 u8 *mac,
				 struct ieee80211_vif *vif);

		iterator = ath12k_mac_add_bridge_vdevs_iter;
		bridge_iter.ah = ar->ah;
		bridge_iter.active_num_devices = active_num_devices;
		ieee80211_iterate_interfaces(ar->ah->hw,
					     IEEE80211_IFACE_ITER_NORMAL,
					     iterator,
					     &bridge_iter);
	}

	ath12k_info(ab, "WSI remap: Device re-addition completed\n");

exit:
	/* Reset the WSI flags */
	ag->wsi_remap_in_progress = false;
	ab->wsi_remap_state = 0;

	return ret;
}

int ath12k_core_qmi_firmware_ready(struct ath12k_base *ab, bool *is_ready)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	int ret, i;
	struct ath12k_base *partner_ab;
	bool hw_grp_ready = false;
	bool irq_enabled[ATH12K_MAX_SOCS] = {0};

	ret = ath12k_core_start_firmware(ab, ab->fw_mode);
	if (ret) {
		ath12k_err(ab, "failed to start firmware in mode: %d ret: %d\n",
			   ab->fw_mode, ret);
		return ret;
	}

	if (ath12k_waltest_mode) {
		ath12k_info(ab, "Booted in Waltest mode\n");
		return 0;
	}

	if (!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags) &&
	    !ab->umcmn_fatal_received) {
		ret = ath12k_ce_init_pipes(ab);
		if (ret) {
			ath12k_err(ab, "failed to initialize CE: %d\n", ret);
			goto err_firmware_stop;
		}
	}

	/* Cache rf_switch_config INI value for use in WMI init and mac registration */
#ifdef CPTCFG_QCN_EXTN
	ab->rf_switch_config = ath12k_cfg_get(ab, ATH12K_CFG_RF_SWITCH_CONFIG) ? 1 : 0;
#else
	ab->rf_switch_config = 0; /* default: primary/full 5G range */
#endif /* CPTCFG_QCN_EXTN */
	ath12k_info(ab, "rf_switch_config: %u (%s 5G range)\n",
		    ab->rf_switch_config,
		    ab->rf_switch_config ? "secondary/high" : "primary/full");

	ret = ath12k_dp_cmn_device_init(ab->dp);
	if (ret) {
		ath12k_err(ab, "failed to init DP: %d\n", ret);
		goto err_firmware_stop;
	}

	mutex_lock(&ag->mutex);
	mutex_lock(&ab->core_lock);


	ret = ath12k_core_start(ab);
	if (ret) {
		ath12k_err(ab, "failed to start core: %d\n", ret);
		goto err_dp_free;
	}

	mutex_unlock(&ab->core_lock);

	ab->cumac_configured = false;
	hw_grp_ready = ath12k_core_hw_group_start_ready(ag);
	if (is_ready)
		*is_ready = hw_grp_ready;

	if (hw_grp_ready) {
		if (!ag->wsi_remap_in_progress) {
			ret = ath12k_qmi_mlo_global_snapshot_mem_init(ab);
			if (ret) {
				ath12k_warn(ab, "failure in global mem init\n");
				goto err_core_stop;
			}
		}

		/* When the WSI remap is in progress or when one of the device
		 * is in bypassed state, other device which is restarting
		 * should update the remapped MLO config during bootup.
		 */
		if (ag->wsi_remap_in_progress ||
		    (ag->num_bypassed > 0 &&
		     test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags))) {
			ath12k_core_wsi_remap_mlo_reconfig(ag);
		}

		ret = ath12k_core_hw_group_start(ag);
		if (ret) {
			ath12k_warn(ab, "unable to start hw group\n");
			goto err_core_stop;
		}
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "group %d started\n", ag->id);

		/* Set global HW group started flag and trigger TA resource creation */
		ath12k_hw_group_started = true;
		if (ath12k_telemetry_is_agent_loaded()) {
			ath12k_info(ab, "Hardware group started, creating TA resources\n");
			__ath12k_telemetry_create_resources_locked(ag);
		}

		if (ath12k_ftm_mode)
			ath12k_info(ab, "FTM mode interface is up\n");

		for (i = 0; i < ag->num_devices; i++) {
			partner_ab = ag->ab[i];
			if (ath12k_ftm_mode || !partner_ab || partner_ab->is_bypassed)
				continue;

			/* Skip pdev creation if WSI remap in progress and chip is not
			 * in bypass Add state, as the pdev for other chips will be
			 * already present.
			 */
			if (ag->wsi_remap_in_progress &&
			    partner_ab->wsi_remap_state != ATH12K_WSI_BYPASS_ADD_DEVICE)
				continue;

			if (ath12k_check_erp_power_down(ag) &&
			    !partner_ab->powerup_triggered)
				continue;

			if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0 &&
			    !partner_ab->recovery_start)
				continue;

			if (!test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &ab->dev_flags) &&
			    !ab->umcmn_fatal_received) {
				ath12k_hif_irq_enable(partner_ab);
				irq_enabled[i] = true;
			}

			ret = ath12k_dp_rxdma_ring_sel_config(partner_ab);
			if (ret) {
				ath12k_err(partner_ab, "failed to setup rxdma ring selection config\n: %d",
					   ret);
				goto err_core_stop;
			}
		}

		if (ag->wsi_remap_in_progress) {
			ret = ath12k_core_wsi_remap_radio_start(ab);
			if (ret) {
				ath12k_err(ab, "WSI readd failed: %d\n", ret);
				goto err_core_stop;
			}

		}

		if (!ath12k_ftm_mode) {
			ret = ath12k_mgmt_htt_setup(ag);
			if (ret)
				goto err_core_stop;
		}

		if (ath12k_check_erp_power_down(ag)) {
			complete(&ag->power_up);
			if (!ath12k_hw_group_recovery_in_progress(ag))
				clear_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);
		}
	}

	if (hw_grp_ready) {
		mutex_unlock(&ag->mutex);
		for (i = 0; i < ag->num_devices; i++) {
			partner_ab = ag->ab[i];
			if (partner_ab->is_static_bypassed) {
				ret = ath12k_wsi_bypass_precheck(partner_ab, 1);
				if (ret) {
					ath12k_err(ab, "WSI Bypass precheck failed");
					goto out;
				}
				partner_ab->wsi_remap_state = 1;
				ath12k_mac_dynamic_wsi_remap(partner_ab);
			}
		}
		goto out;
	}

	mutex_unlock(&ag->mutex);

out:
	return 0;

err_core_stop:
	for (i = ag->num_devices - 1; i >= 0; i--) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		if (irq_enabled[i])
			ath12k_hif_irq_disable(ab);

		mutex_lock(&ab->core_lock);
#ifdef CPTCFG_EXT_IPA_OFFLOAD
		ath12k_dp_ipa_uc_ol_deinit(ab);
#endif
		ath12k_core_stop(ab);
		mutex_unlock(&ab->core_lock);
	}
	mutex_unlock(&ag->mutex);
	goto exit;

err_dp_free:
	ath12k_dp_cmn_device_deinit(ab->dp);
	mutex_unlock(&ab->core_lock);
	mutex_unlock(&ag->mutex);

err_firmware_stop:
	ath12k_qmi_firmware_stop(ab);

exit:
	return ret;
}

u8 ath12k_core_get_total_num_vdevs(struct ath12k_base *ab)
{
	u8 num_vdevs = TARGET_NUM_VDEVS;

#ifdef CPTCFG_QCN_EXTN
	if (ab && ab->cfg_ctx) {
		u32 ini_val = ath12k_cfg_get(ab, ATH12K_CFG_NUM_VDEVS);

		if (ini_val)
			num_vdevs = (u8)ini_val;
	}
#endif

	if (ab->ag && ab->ag->num_devices >= ATH12K_MIN_NUM_DEVICES_NLINK)
		return num_vdevs + TARGET_NUM_BRIDGE_VDEVS;

	return num_vdevs;
}
EXPORT_SYMBOL(ath12k_core_get_total_num_vdevs);

bool ath12k_core_is_vdev_limit_reached(struct ath12k *ar,
				       bool is_bridge_vdev)
{
	struct ath12k_base *ab;
	u32 num_created_vdevs;
	u8 total_num_vdevs, num_created_bridge_vdevs;
	bool ret = false;

	ab = ar->ab;
	total_num_vdevs = ath12k_core_get_total_num_vdevs(ab);
	num_created_vdevs = ar->num_created_vdevs;
	num_created_bridge_vdevs = ar->num_created_bridge_vdevs;

	if (total_num_vdevs == ATH12K_MAX_NUM_VDEVS_NLINK) {
		if ((num_created_vdevs + num_created_bridge_vdevs) >
		    (ATH12K_MAX_NUM_VDEVS_NLINK - 1)) {
			ath12k_err(ab, "failed to create vdev, reached total max vdev limit %d[%d]\n",
				   num_created_vdevs + num_created_bridge_vdevs,
				   ATH12K_MAX_NUM_VDEVS_NLINK);
			ret = true;
			goto exit;
		}

		if (!is_bridge_vdev &&
		    num_created_vdevs > (TARGET_NUM_VDEVS - 1)) {
			ath12k_err(ab, "failed to create vdev, reached max vdev limit %d[%d]\n",
				   num_created_vdevs,
				   TARGET_NUM_VDEVS);
			ret = true;
			goto exit;
		}

		if (is_bridge_vdev &&
		    num_created_bridge_vdevs > (TARGET_NUM_BRIDGE_VDEVS - 1)) {
			ath12k_warn(ab, "failed to create bridge vdev, reached max bridge vdev limit: %d[%d]\n",
				    num_created_bridge_vdevs, TARGET_NUM_BRIDGE_VDEVS);
			ret = true;
			goto exit;
		}
		goto exit;
	}

	if (num_created_vdevs > (total_num_vdevs - 1)) {
		ath12k_err(ab, "failed to create vdev, reached max vdev limit %d [%d]\n",
			   num_created_vdevs, total_num_vdevs);
		ret = true;
	}

exit:
	return ret;
}

static int ath12k_core_reconfigure_on_crash(struct ath12k_base *ab, bool *is_ready)
{
	int ret;
	u8 total_vdevs;

	if (test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags)) {
		ath12k_core_cleanup(ab);
		total_vdevs = ath12k_core_get_total_num_vdevs(ab);
		ab->free_vdev_map = (1LL << (ab->num_radios * total_vdevs)) - 1;
		ab->free_vdev_stats_id_map = 0;
	}

	ret = ath12k_hal_srng_init(ab);
	if (ret)
		return ret;

	clear_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

	ret = ath12k_core_qmi_firmware_ready(ab, is_ready);
	if (ret)
		goto err_hal_srng_deinit;

	if (ath12k_check_erp_power_down(ab->ag)) {
		if (!test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags))
			set_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags);
	}

	return 0;

err_hal_srng_deinit:
	ath12k_hal_srng_deinit(ab);
	return ret;
}

static void ath12k_rfkill_work(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, rfkill_work);
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k *ar;
	struct ath12k_hw *ah;
	struct ieee80211_hw *hw;
	bool rfkill_radio_on;
	int i, j;

	spin_lock_bh(&ab->base_lock);
	rfkill_radio_on = ab->rfkill_radio_on;
	spin_unlock_bh(&ab->base_lock);

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah)
			continue;

		for (j = 0; j < ah->num_radio; j++) {
			ar = &ah->radio[j];
			if (!ar)
				continue;

			ath12k_mac_rfkill_enable_radio(ar, rfkill_radio_on);
		}

		hw = ah->hw;
		wiphy_rfkill_set_hw_state(hw->wiphy, !rfkill_radio_on);
	}
}

static void ath12k_mac_peer_ab_disassoc(struct ath12k_base *ab)
{
	struct ath12k_sta *ahsta;
	struct ieee80211_sta *sta;
	struct ath12k_pdev *pdev;
	struct ath12k *ar_iter;
	struct ath12k_link_sta *arsta_iter;
	u32 __bkt;
	int i;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar_iter = pdev->ar;
		if (!ar_iter)
			continue;

		spin_lock_bh(&ar_iter->arsta_lock);
		ath12k_link_sta_for_each(ar_iter, __bkt, arsta_iter) {
			struct ath12k_vif *ahvif;

			if (arsta_iter->is_self_peer)
				continue;

			ahvif = arsta_iter->arvif->ahvif;
			/* In case of STA Vif type, report disconnect will
			 * be sent during sta_restart work.
			 */
			if (ahvif->vif->type == NL80211_IFTYPE_STATION)
				continue;

			sta = ath12k_ahsta_to_sta(arsta_iter->ahsta);
			ahsta = arsta_iter->ahsta;
			/* Sending low ack event to hostapd to remove (free)
			 * the existing STAs since FW is crashed and recovering.
			 * After recovery, FW comes up with no information about
			 * peers. To stop any operation related to peers coming
			 * from upper layers.
			 * Here, 0xFFFF is used to differentiate between low ack
			 * event sent during recovery versus normal low ack event.
			 */
			ath12k_mac_peer_disassoc(ab, sta, ahsta, ATH12K_DBG_MAC);
		}
		spin_unlock_bh(&ar_iter->arsta_lock);
	}
}

static void ath12k_check_for_valid_chanctx(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif, *tmp;
	struct cfg80211_chan_def *def = NULL;

	list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
		if (!arvif->ar)
			continue;

		if (!ath12k_mac_is_bridge_vdev(arvif) && !arvif->chanctx.def.chan) {
			ath12k_dbg(ar->ab, ATH12K_DBG_MAC,
				   "Del arvif vdev %u MAC %pM from radio hw_link_id %d list\n",
				   arvif->vdev_id, arvif->addr, ar->hw_link_id);
			list_del(&arvif->list);
			arvif->ar = NULL;
		} else if (arvif->chanctx.def.chan) {
			def = &arvif->chanctx.def;
		}
	}

	/* No valid channel context exist in any link so clear ar->rx_channel*/
	if (!def || !def->chan) {
		ar->rx_channel = NULL;
		ar->chan_tx_pwr = ATH12K_PDEV_TX_POWER_INVALID;
	}
}

void ath12k_core_halt(struct ath12k *ar)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw_group *ag = ab->ag;
	int ret;

	if (ab->is_bypassed)
		return;

	lockdep_assert_wiphy(ath12k_ar_to_hw(ar)->wiphy);

	/* The host sends a peer delete command to the firmware and records the peer in
	 * the corresponding pdev’s peer_del tracker. When the firmware’s peer delete
	 * response is received, the matching peer entry is removed from this tracker.
	 * If an assert occurs before the firmware responds, the host is left with a stale
	 * peer entry. To avoid this, flush the peer_del tracker for the asserted chip.
	 */
	ret = ath12k_peer_del_tracker_clear_pdev(ar->pdev);
	if (ret)
		ath12k_err(ab, "failed to clean up peer_del tracker for pdev:%d\n",
			   ar->pdev_idx);

	/* Send low ack disassoc to hostapd to free the peers from host
	 * to associate fresh after recovery. It is expected that, this
	 * low ack event to hostapd must free the stas from hostapd and
	 * kernel before new association process starts.
	 *
	 * And also during ieee80211_reconfig, it should not add back the
	 * existing stas as it is already freed from host due to low ack
	 * event.
	 */

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
		ath12k_mac_peer_ab_disassoc(ab);

	ath12k_telemetry_ab_peer_agent_destroy(ab);

	ath12k_mac_dp_peer_cleanup_all(ar);
	cancel_work_sync(&ar->regd_update_work);
	cancel_work_sync(&ar->reg_set_previous_country);
	cancel_work_sync(&ar->change_6g_txpow_sta_mode_work);
	cancel_work_sync(&ab->rfkill_work);
	cancel_work_sync(&ab->update_11d_work);
	wiphy_work_cancel(ath12k_ar_to_hw(ar)->wiphy, &ar->agile_cac_abort_wq);
	ar->agile_abort_pending = false;

	rcu_assign_pointer(ab->pdevs_active[ar->pdev_idx], NULL);
	synchronize_rcu();

	ath12k_core_cu_mem_free_all(ar);

	idr_init(&ar->txmgmt_idr);

	cancel_work_sync(&ar->erp_handle_trigger_work);
	ar->erp_trigger_set = false;
	cancel_work_sync(&ar->ssr_erp_exit);
}

static void ath12k_core_mlo_hw_queues_stop(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag,i);
		if (!ah)
			continue;

		wiphy_lock(ah->hw->wiphy);
		/* If queue 0 is stopped, it is safe to assume that all
		 * other queues are stopped by driver via
		 * ieee80211_stop_queues() below. This means, there is
		 * no need to stop it again and hence continue
		 */

		if (ieee80211_queue_stopped(ah->hw, 0)) {
			wiphy_unlock(ah->hw->wiphy);
			return;
		}

                ieee80211_stop_queues(ah->hw);
		ah->queue_stop = true;
		wiphy_unlock(ah->hw->wiphy);
	}
}

/**
 * ath12k_core_radio_cleanup() - Per-radio SSR teardown.
 * @ar: per-radio ath12k context
 *
 * Clears per-radio state before firmware recovery.
 */
void ath12k_core_radio_cleanup(struct ath12k *ar)
{
	ath12k_mac_drain_tx(ar);
	ar->state_11d = ATH12K_11D_IDLE;
	complete(&ar->completed_11d_scan);
	complete(&ar->scan.started);
	/* Protect only scan.completed against race with reinit_completion() */
	spin_lock_bh(&ar->data_lock);
	if (!completion_done(&ar->scan.completed))
		complete_all(&ar->scan.completed);
	spin_unlock_bh(&ar->data_lock);
	complete(&ar->scan.on_channel);
	complete(&ar->peer_create_conf);
	complete(&ar->peer_assoc_done);
	ath12k_debugfs_nrp_cleanup_all(ar);
	complete(&ar->install_key_done);
	complete(&ar->vdev_setup_done);
	complete(&ar->vdev_delete_done);
	complete(&ar->bss_survey_done);
	complete(&ar->thermal.wmi_sync);
	complete(&ar->scan.on_channel);

	wake_up(&ar->dp.tx_empty_waitq);
	idr_for_each(&ar->txmgmt_idr,
		     ath12k_mac_tx_mgmt_pending_free, ar);
	idr_destroy(&ar->txmgmt_idr);
	wake_up(&ar->txmgmt_empty_waitq);

	ar->monitor_vdev_id = -1;
	ar->monitor_started = false;
	ar->monitor_vdev_created = false;
}

static void ath12k_arvif_abort_cu_notify(struct ath12k_hw *ah,
					 struct ath12k_link_vif *arvif)
{
	struct ath12k_uhr_cu_info *ecu = &arvif->uhr_ecu;
	bool in_progress;

	if (!ecu->started)
		return;

	wiphy_work_cancel(ah->hw->wiphy, &arvif->uhr_cu_notify_work);

	/* Determine whether the session had progressed past STARTED.
	 * If any state beyond STARTED is pending the session is already
	 * in progress and ECU_END is the correct termination; otherwise
	 * send ABORT.  Clear all pending bits unconditionally.
	 */
	in_progress =
		test_bit(NL80211_CU_STATE_ADV_NOTIFICATION_END,
			 ecu->pending_states) |
		test_bit(NL80211_CU_STATE_POST_NOTIFICATION_END,
			 ecu->pending_states) |
		test_bit(NL80211_CU_STATE_ECU_END,
			 ecu->pending_states);

	ieee80211_cu_notify(ah->hw, arvif->ahvif->vif, arvif->link_id,
			    in_progress ? NL80211_CU_STATE_ECU_END
					: NL80211_CU_STATE_ABORT);
	memset(ecu, 0, sizeof(*ecu));
}

static void ath12k_core_pre_reconfigure_recovery(struct ath12k_base *ab)
{
	struct ath12k *ar;
	struct ath12k_hw *ah;
	struct ath12k_link_vif *arvif;
	int i;

	spin_lock_bh(&ab->base_lock);
	ab->stats.fw_crash_counter++;
	spin_unlock_bh(&ab->base_lock);

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		ah = ar->ah;

		if (!ah || ah->state == ATH12K_HW_STATE_OFF ||
		    ah->state == ATH12K_HW_STATE_TM) {
			ath12k_dbg(ab, ATH12K_DBG_MAC,
				   "radio hw_link_id %d skipped pre reconfigure recovery, ah_valid %d state %d\n",
				   ar->hw_link_id, !!ah, ah ? ah->state : -1);
			continue;
		}

		ath12k_dbg(ab, ATH12K_DBG_MAC,
			   "Invoke init flags reset for radio hw_link_id %d\n",
			   ar->hw_link_id);

		wiphy_lock(ah->hw->wiphy);

		list_for_each_entry(arvif, &ar->arvifs, list) {
			if (arvif->is_started)
				ath12k_debugfs_remove_interface(arvif);


			ath12k_arvif_abort_cu_notify(ah, arvif);

			arvif->is_started = false;
			if (arvif->is_created) {
				spin_lock_bh(&ar->data_lock);
				ar->created_vdev_map &= ~(1LL << arvif->vdev_id);
				spin_unlock_bh(&ar->data_lock);
			}
			arvif->is_created = false;
			arvif->is_up = false;
			arvif->spectral_enabled = false;
			ath12k_dbg(ab, ATH12K_DBG_MAC,
				   "init flags reset for MAC %pM vdev %d\n",
				   arvif->addr, arvif->vdev_id);
		}

#ifdef CPTCFG_ATH12K_SPECTRAL
		spin_lock_bh(&ar->spectral.lock);
		ar->spectral.mode = SPECTRAL_SCAN_MODE_INVALID;
		ar->spectral.scan_active = false;
		spin_unlock_bh(&ar->spectral.lock);
#endif

		ath12k_core_radio_cleanup(ar);
		wiphy_unlock(ah->hw->wiphy);
	}

	wake_up(&ab->wmi_ab.tx_credits_wq);
	wake_up(&ab->peer_mapping_wq);
}

static void ath12k_update_11d(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, update_11d_work);
	struct ath12k *ar;
	struct ath12k_pdev *pdev;
	struct wmi_set_current_country_arg arg = {};
	int ret, i;

	spin_lock_bh(&ab->base_lock);
	memcpy(&arg.alpha2, &ab->new_alpha2, 2);
	spin_unlock_bh(&ab->base_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "update 11d new cc %c%c\n",
	arg.alpha2[0], arg.alpha2[1]);

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;

		memcpy(&ar->alpha2, &arg.alpha2, 2);
		ret = ath12k_wmi_send_set_current_country_cmd(ar, &arg);
		if (ret)
			ath12k_warn(ar->ab,
				    "pdev id %d failed set current country code: %d\n",
				    i, ret);
	}
}

static void ath12k_core_post_reconfigure_recovery(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int i;

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0)
		goto cleanup;

	for (i = 0; i < ag->num_hw; i++) {
		ah = ath12k_ag_to_ah(ag, i);
		if (!ah || ah->state == ATH12K_HW_STATE_RESTARTING)
			continue;

		mutex_lock(&ah->hw_mutex);

		switch (ah->state) {
		case ATH12K_HW_STATE_ON:
			ah->state = ATH12K_HW_STATE_RESTARTING;
			break;
		case ATH12K_HW_STATE_OFF:
			ath12k_warn(ab,
				    "cannot restart hw %d that hasn't been started\n",
				    i);
			break;
		case ATH12K_HW_STATE_RESTARTING:
			break;
		case ATH12K_HW_STATE_RESTARTED:
			ah->state = ATH12K_HW_STATE_WEDGED;
			fallthrough;
		case ATH12K_HW_STATE_WEDGED:
			ath12k_warn(ab,
				    "device is wedged, will not restart hw %d\n", i);
			break;
		case ATH12K_HW_STATE_TM:
			ath12k_warn(ab, "fw mode reset done radio %d\n", i);
			break;
		}
		mutex_unlock(&ah->hw_mutex);
	}

cleanup:
	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		/* mlo_capable = 0 split phy will have 2 ah's
		 * within ag hence aquire lock inside the loop
		 */

		ah = ar->ah;

		wiphy_lock(ah->hw->wiphy);
		if (ar->scan.state == ATH12K_SCAN_RUNNING ||
		    ar->scan.state == ATH12K_SCAN_STARTING)
			ar->scan.state = ATH12K_SCAN_ABORTING;

		ath12k_mac_scan_finish(ar);
		wiphy_unlock(ah->hw->wiphy);
		cancel_delayed_work_sync(&ar->scan.timeout);
		cancel_delayed_work_sync(&ar->scan.roc_done);
		wiphy_lock(ah->hw->wiphy);
		ath12k_core_halt(ar);
		wiphy_unlock(ah->hw->wiphy);
	}
	complete(&ab->driver_recovery);
}

int ath12k_core_radio_start(struct ath12k_hw *ah)
{
	struct ath12k_hw_group *ag = ath12k_ah_to_ag(ah);
	struct ath12k *ar;
	int i, ret = 0;

	guard(mutex)(&ah->hw_mutex);

	ah->state = ATH12K_HW_STATE_ON;

	for_each_ar(ah, ar, i) {
		if (!ath12k_ftm_mode && ar) {
			if (ar->allocated_vdev_map || ar->ab->is_bypassed) {
				continue;
			} else {
				ret = ath12k_mac_start(ar);
				if (ret) {
					ah->state = ATH12K_HW_STATE_OFF;
					ath12k_err(ar->ab, "mac radio start failed\n");
					return ret;
				}

				ar->pdev_suspend = false;
			}
		}

		ar->ab->powerup_triggered = false;
	}

	if (test_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags))
		clear_bit(ATH12K_GROUP_FLAG_HIF_POWER_DOWN, &ag->flags);

	return ret;
}

static void ath12k_core_restart(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, restart_work);
	struct ath12k_hw_group *ag = ab->ag;
	bool is_ready = false;
	struct ath12k_hw *ah;
	struct ath12k *ar;
	int ret, i, j;

	ret = ath12k_core_reconfigure_on_crash(ab, &is_ready);
	if (ret) {
		ath12k_err(ab, "failed to reconfigure driver on crash recovery\n");
		/*
		 * If for any reason, reconfiguration fails, issue bug on for
		 * Mode 0
		 */
		if (ath12k_ssr_failsafe_mode && ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
			BUG_ON(1);
		return;
	}

	if (ab->is_reset ||
	    (ath12k_check_erp_power_down(ag) &&
	     ath12k_hw_group_recovery_in_progress(ag))) {
		if (!test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags)) {
			atomic_dec(&ab->reset_count);
			complete(&ab->reset_complete);
			ab->is_reset = false;
			atomic_set(&ab->fail_cont_count, 0);
			ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset success\n");
		}

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;

			spin_lock_bh(&ar->data_lock);
			ath12k_check_for_valid_chanctx(ar);
			if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0) {
				ath12k_dbg(ab, ATH12K_DBG_MAC,
					   "Initialize arvifs list for radio hw_link_id %d\n",
					   ar->hw_link_id);
				INIT_LIST_HEAD(&ar->arvifs);
			}
			spin_unlock_bh(&ar->data_lock);
		}

		if (!is_ready)
			goto exit_restart;

		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0) {
			queue_work(ab->workqueue_aux, &ab->recovery_work);
			goto exit_restart;
		}

		for (i = 0; i < ag->num_hw; i++) {
			ah = ath12k_ag_to_ah(ag, i);
			ieee80211_restart_hw(ah->hw);
		}
	}

exit_restart:
	complete(&ab->restart_completed);
}

static void ath12k_core_mode1_recovery_sta_list(void *data, struct ieee80211_sta *sta)
{
	struct ath12k_link_sta *arsta, *p_arsta;
	struct ath12k_sta *ahsta = ath12k_sta_to_ahsta(sta);
	struct ath12k_link_vif *arvif = (struct ath12k_link_vif *)data, *p_arvif;
	struct ath12k_vif *ahvif = arvif->ahvif;
	struct ieee80211_vif *vif = ahvif->vif;
	struct ath12k *ar = arvif->ar;
	struct ath12k_base *ab = arvif->ar->ab;
	struct ath12k_key_conf *key_conf = NULL;
	struct ieee80211_key_conf *key;
	int ret = -1, key_idx;
	u8 link_id = arvif->link_id;
	enum ieee80211_sta_state state, prev_state;
	bool sta_added = false;
	unsigned long links;

	if (ahsta->ahvif != arvif->ahvif)
		return;

	/* Check if there is a link sta in the vif link */
	if (!(BIT(link_id) & ahsta->links_map))
		return;

	/* From iterator, rcu_read_lock is acquired. Will be revisited
	 * later to use local list
	 */
	arsta = rcu_dereference(ahsta->link[link_id]);
	if (!arsta)
		return;

	key_conf = container_of((void *)sta, struct ath12k_key_conf, sta);

	if (vif->type != NL80211_IFTYPE_AP &&
	    vif->type != NL80211_IFTYPE_AP_VLAN &&
	    vif->type != NL80211_IFTYPE_STATION &&
	    vif->type != NL80211_IFTYPE_MESH_POINT)
		return;

	spin_lock_bh(&ar->arsta_lock);
	if (ath12k_link_sta_find_by_addr(ar, arsta->addr)) {
		spin_unlock_bh(&ar->arsta_lock);
		sta_added = true;
		goto key_add;
	}
	spin_unlock_bh(&ar->arsta_lock);

	prev_state = arsta->ahsta->state;
	for (state = IEEE80211_STA_NOTEXIST;
			state < prev_state; state++) {
		/* all station set case */
		/* TODO: Iterator API is called with rcu lock
		 * hence need for this unlock/lock statement.
		 * Need to revisit in next version
		 */
		rcu_read_unlock();
		ath12k_mac_op_sta_state(ar->ah->hw, arvif->ahvif->vif, sta,
				state, (state + 1));
		rcu_read_lock();
		sta_added = true;
	}

	if (!sta_added)
		goto skip_key_add;

key_add:
	for (key_idx = 0; key_idx < WMI_MAX_KEY_INDEX; key_idx++) {
		key = arsta->keys[key_idx];

		if (key) {
			/* BIP needs to be done in software */
			if (key->cipher == WLAN_CIPHER_SUITE_AES_CMAC ||
			    key->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_128 ||
			    key->cipher == WLAN_CIPHER_SUITE_BIP_GMAC_256 ||
			    key->cipher == WLAN_CIPHER_SUITE_BIP_CMAC_256) {
				return;
			}

			if (test_bit(ATH12K_GROUP_FLAG_HW_CRYPTO_DISABLED,
				     &ab->ag->flags))
				return;

			if (!arvif->is_created) {
				key_conf = kzalloc(sizeof(*key_conf), GFP_ATOMIC);

				if (!key_conf)
					return;

				key_conf->cmd = SET_KEY;
				key_conf->sta = sta;
				key_conf->key = key;

				list_add_tail(&key_conf->list,
					      &ahvif->cache[link_id]->key_conf.list);

				ath12k_info(ab, "set key param cached since vif not assign to radio\n");
				return;
			}

			if (sta->mlo) {
				links = ahsta->links_map;
				for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
					p_arvif = ath12k_get_arvif_from_link_id(ahvif,
										link_id);
					p_arsta = rcu_dereference(ahsta->link[link_id]);
					if (WARN_ON(!p_arvif || !p_arsta))
						continue;

					/* TODO: Iterator API is called with rcu lock
					 * hence need for this unlock/lock statement.
					 * Need to revisit in next version
					 */
					rcu_read_unlock();
					ret = ath12k_mac_set_key(p_arvif->ar, SET_KEY,
								 p_arvif, p_arsta,
								 key, NULL);
					rcu_read_lock();
					if (ret)
						break;
				}
			} else {
				p_arsta = &ahsta->deflink;
				p_arvif = p_arsta->arvif;
				if (WARN_ON(!p_arvif))
					return;

				/* TODO: Iterator API is called with rcu lock
				 * hence need for this unlock/lock statement.
				 * Need to revisit in next version
				 */
				rcu_read_unlock();
				ret = ath12k_mac_set_key(p_arvif->ar, SET_KEY, p_arvif,
							 p_arsta, key, NULL);
				rcu_read_lock();
			}
		}
	}

skip_key_add:
	ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
			"Recovered sta:%pM link_id:%d, num_sta:%d\n",
			arsta->addr, arsta->link_id, arvif->ar->num_stations);
	return;
}

static void ath12k_core_iterate_sta_list(struct ath12k *ar,
                                         struct ath12k_link_vif *arvif)
{
	ieee80211_iterate_stations_atomic(ar->ah->hw,
					  ath12k_core_mode1_recovery_sta_list,
					  arvif);
}

static void ath12k_core_ml_sta_add(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif, *tmp;
	struct ieee80211_bss_conf *info;
	struct ath12k_vif *ahvif;
	struct ieee80211_vif *vif;

	lockdep_assert_wiphy(ar->ah->hw->wiphy);

	list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
		ahvif = arvif->ahvif;

		if (!ahvif)
			continue;

		vif = ahvif->vif;
		if (ahvif->vdev_type != WMI_VDEV_TYPE_STA)
			continue;

		if (!vif->valid_links)
			continue;

		ath12k_core_iterate_sta_list(ar, arvif);

		if (ath12k_mac_is_bridge_vdev(arvif))
			info = NULL;
		else
			info = vif->link_conf[arvif->link_id];

		/* Set is_up to false as we will do
		 * recovery for that vif in the
		 * upcoming executions
		 */
		arvif->is_up = false;
		if (vif->cfg.assoc)
			ath12k_bss_assoc(ar, arvif, info);
		else
			ath12k_bss_disassoc(ar, arvif);
		ath12k_dbg(ar->ab, ATH12K_DBG_MODE1_RECOVERY,
			   "station vif:%pM recovered\n",
			   arvif->bssid);
	}
}

/* API to recovery station VIF enabled in non-asserted links */
static void ath12k_core_mlo_recover_station(struct ath12k_hw_group *ag,
					    struct ath12k_base *assert_ab)
{
	struct ath12k_base *ab;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	int i, j;

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2 ||
	    ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3)
		return;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed || ab == assert_ab)
			continue;

		for (j = 0; j < ab->num_radios; j++) {
			pdev = &ab->pdevs[j];
			ar = pdev->ar;

			if (!ar)
				continue;

			if (list_empty(&ar->arvifs))
				continue;

			/* Re-add all MLD station VIF which are
			 * in non-asserted link
			 */
			ath12k_core_ml_sta_add(ar);
		}
	}
}

static int ath12k_mlo_recovery_link_vif_reconfig(struct ath12k *ar,
						struct ath12k_vif *ahvif,
						struct ath12k_link_vif *arvif,
						struct ieee80211_vif *vif,
						struct ieee80211_bss_conf *link_conf)
{
	int i;
	int link_id = arvif->link_id;
	struct ath12k_hw *ah = ar->ah;
	struct ieee80211_tx_queue_params params;
	struct wmi_wmm_params_arg *p = NULL;
	struct ieee80211_bss_conf *info;
	u64 changed = 0;
	bool bridge_vdev;

	lockdep_assert_wiphy(ah->hw->wiphy);

	switch (vif->type) {
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_MONITOR:
		break;
	case NL80211_IFTYPE_ADHOC:
		fallthrough;
	default:
		ieee80211_iterate_stations_atomic(ar->ah->hw,
				ath12k_core_mode1_recovery_sta_list,
				arvif);
		fallthrough;
	case NL80211_IFTYPE_AP: /* AP stations are handled later */
		for (i = 0; i < IEEE80211_NUM_ACS; i++) {

			if ((vif->active_links &&
			    !(vif->active_links & BIT(link_id))) ||
			    link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
				break;

			switch (i) {
			case IEEE80211_AC_VO:
				p = &arvif->wmm_params.ac_vo;
				break;
			case IEEE80211_AC_VI:
				p = &arvif->wmm_params.ac_vi;
				break;
			case IEEE80211_AC_BE:
				p = &arvif->wmm_params.ac_be;
				break;
			case IEEE80211_AC_BK:
				p = &arvif->wmm_params.ac_bk;
				break;
			}

			params.cw_min = p->cwmin;
			params.cw_max = p->cwmax;
			params.aifs = p->aifs;
			params.txop = p->txop;
			params.acm = p->acm;
			params.noack = p->no_ack;

			ath12k_mac_conf_tx(arvif, i, &params);
		}
		break;
	}

	/* common change flags for all interface types */
	changed = BSS_CHANGED_ERP_CTS_PROT |
		BSS_CHANGED_ERP_PREAMBLE |
		BSS_CHANGED_ERP_SLOT |
		BSS_CHANGED_HT |
		BSS_CHANGED_BASIC_RATES |
		BSS_CHANGED_BEACON_INT |
		BSS_CHANGED_BSSID |
		BSS_CHANGED_CQM |
		BSS_CHANGED_QOS |
		BSS_CHANGED_TXPOWER |
		BSS_CHANGED_MCAST_RATE;

	bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

	if (!bridge_vdev && link_conf->mu_mimo_owner)
		changed |= BSS_CHANGED_MU_GROUPS;

	switch (vif->type) {
	case NL80211_IFTYPE_STATION:
		if (!vif->valid_links) {
			/* Set this only for legacy stations */
			changed |= BSS_CHANGED_ASSOC |
				BSS_CHANGED_ARP_FILTER |
				BSS_CHANGED_PS;

			/* Assume re-send beacon info report to the driver */
			changed |= BSS_CHANGED_BEACON_INFO;

			if (link_conf->max_idle_period ||
					link_conf->protected_keep_alive)
				changed |= BSS_CHANGED_KEEP_ALIVE;

			if (!arvif->is_created) {
				ath12k_info(NULL,
					    "bss info parameter changes %llx cached to apply after vdev create on channel assign\n",
					    changed);
				ahvif->cache[link_id]->bss_conf_changed |= changed;

				return 0;
			}
		}

		/* Set is_up to false as we will do
		 * recovery for that vif in the
		 * upcoming executions
		 */
		arvif->is_up = false;
		ath12k_mac_bss_info_changed(ar, arvif, link_conf, changed);
		if (vif->valid_links) {
			if (bridge_vdev)
				info = NULL;
			else
				info = vif->link_conf[link_id];

			if (vif->cfg.assoc)
				ath12k_bss_assoc(ar, arvif, info);
			else
				ath12k_bss_disassoc(ar, arvif);
		}
		break;
	case NL80211_IFTYPE_OCB:
		changed |= BSS_CHANGED_OCB;

		ath12k_mac_bss_info_changed(ar, arvif, link_conf, changed);
		break;
	case NL80211_IFTYPE_ADHOC:
		changed |= BSS_CHANGED_IBSS;
		fallthrough;
	case NL80211_IFTYPE_AP:
		changed |= BSS_CHANGED_P2P_PS;

		if (vif->type == NL80211_IFTYPE_AP) {
			changed |= BSS_CHANGED_AP_PROBE_RESP;
			ahvif->u.ap.ssid_len = vif->cfg.ssid_len;
			if (vif->cfg.ssid_len)
				memcpy(ahvif->u.ap.ssid, vif->cfg.ssid, vif->cfg.ssid_len);
		}
		fallthrough;
	case NL80211_IFTYPE_MESH_POINT:
		if (link_conf->enable_beacon) {
			changed |= BSS_CHANGED_BEACON |
				BSS_CHANGED_BEACON_ENABLED;

			ath12k_mac_bss_info_changed(ar, arvif, link_conf,
					changed & ~BSS_CHANGED_IDLE);

		}
		break;
	case NL80211_IFTYPE_NAN:
	case NL80211_IFTYPE_AP_VLAN:
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_P2P_DEVICE:
		/* nothing to do */
		break;
	case NL80211_IFTYPE_UNSPECIFIED:
	case NUM_NL80211_IFTYPES:
	case NL80211_IFTYPE_P2P_CLIENT:
	case NL80211_IFTYPE_P2P_GO:
	case NL80211_IFTYPE_WDS:
		WARN_ON(1);
		break;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_MODE1_RECOVERY,
		   "Reconfig link vif done:type:%d\n", vif->type);

	return 0;
}

static int ath12k_mlo_core_recovery_reconfig_link_bss(struct ath12k *ar,
						      struct ieee80211_bss_conf *link_conf,
						      struct ath12k_vif *ahvif,
						      struct ath12k_link_vif *arvif)
{
	struct ieee80211_vif *vif = arvif->ahvif->vif;
	struct ath12k_base *ab = ar->ab;
	struct ath12k_hw *ah = ar->ah;
	enum ieee80211_ap_reg_power power_type;
	struct ath12k_wmi_peer_create_arg param;
	struct ieee80211_chanctx_conf *ctx = &arvif->chanctx;
	int ret = -1;
	u8 link_id;
	bool is_bridge_vdev, dp_peer_created = false;
	struct ath12k_dp_peer_create_params dp_params = {};

	lockdep_assert_wiphy(ah->hw->wiphy);

	is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);
	link_id = is_bridge_vdev ? arvif->link_id : link_conf->link_id;

	ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
		   "Recovering: link_id:%d addr %pM type:%d subtype:%d\n",
		   link_id, arvif->bssid, vif->type, arvif->vdev_subtype);

	if (vif->type == NL80211_IFTYPE_AP &&
		ar->num_peers > (ar->max_num_peers - 1)) {
		ath12k_err(ab, "Error in peers:%d\n",
			   ar->num_peers);
		goto exit;
	}

	if (ath12k_core_is_vdev_limit_reached(ar, is_bridge_vdev))
		goto exit;

	ret = ath12k_mac_vdev_create(ar, arvif, is_bridge_vdev);

	if (ret) {
		ath12k_warn(ab, "failed to create vdev %pM\n", arvif->bssid);
		goto exit;
	}

	if (!is_bridge_vdev) {
		ath12k_mac_vif_cache_flush(ar, arvif);

		if (ar->supports_6ghz && ctx->def.chan->band == NL80211_BAND_6GHZ &&
		    (ahvif->vdev_type == WMI_VDEV_TYPE_STA ||
		    ahvif->vdev_type == WMI_VDEV_TYPE_AP)) {
			power_type = link_conf->power_type;
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L3,
					 "mac chanctx power type %d\n", power_type);
			if (power_type == IEEE80211_REG_UNSET_AP)
				power_type = IEEE80211_REG_LPI_AP;

			/* TODO: Transmit Power Envelope specification for 320 is not
			 * available yet. Need to add TPE 320 support when spec is ready
			 */
			if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
			    ctx->def.width != NL80211_CHAN_WIDTH_320) {
				ath12k_mac_parse_tx_pwr_env(ar, arvif);
			}
		}
	}

	/* for some targets bss peer must be created before vdev_start */
	if (ab->hw_params->vdev_start_delay &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_AP &&
	    ahvif->vdev_type != WMI_VDEV_TYPE_MONITOR) {
		spin_lock_bh(&ar->arsta_lock);
		if (!ath12k_link_sta_find_by_addr(ar, arvif->bssid)) {
			ret = 0;
			spin_unlock_bh(&ar->arsta_lock);
			goto exit;
		}
		spin_unlock_bh(&ar->arsta_lock);
	}

	if (ab->hw_params->vdev_start_delay &&
	    (ahvif->vdev_type == WMI_VDEV_TYPE_AP ||
	    ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR)) {
		dp_params.is_vdev_peer = true;
		dp_params.hw_link_id = ar->hw_link_id;

		ret = ath12k_dp_arch_peer_create(ab->dp, ah, arvif->bssid,
						 &dp_params, vif);
		if (ret) {
			ath12k_warn(ab, "failed to create dp_peer for vdev AP %d: %d\n",
				    arvif->vdev_id, ret);
			goto exit;
		}

		dp_peer_created = true;

		if (ahvif->vdev_type == WMI_VDEV_TYPE_AP &&
		    !arvif->self_arsta) {
			ret = ath12k_mac_self_peer_arsta_create(ar, arvif,
								ahvif->vdev_type);
			if (ret)
				goto exit;
		}

		param.vdev_id = arvif->vdev_id;
		param.peer_type = WMI_PEER_TYPE_DEFAULT;
		param.peer_addr = ar->mac_addr;

		ret = ath12k_peer_create(ar, arvif, NULL, &param);
		if (ret) {
			ath12k_warn(ab, "failed to create peer after vdev start delay: %d",
					ret);
			goto exit;
                }
        }

	if (ahvif->vdev_type == WMI_VDEV_TYPE_MONITOR) {
		ret = ath12k_mac_monitor_start(ar);
		if (ret)
			goto exit;
		arvif->is_started = true;
		goto exit;
	}

	if (ctx->def.chan)
		ret = ath12k_mac_vdev_start(arvif, ctx);
	else if (is_bridge_vdev)
		ret = ath12k_mac_vdev_start(arvif, NULL);

	if (ret) {
		ath12k_err(ab, "vdev start failed during recovery\n");
		goto exit;
	}

	arvif->is_started = true;
	ret = 0;
exit:
	if (ret && dp_peer_created)
		ath12k_dp_arch_peer_delete(ab->dp, ah, arvif->bssid,
					   NULL, ar->hw_link_id, ahvif->vif);

	if (ret && arvif->self_arsta) {
		spin_lock_bh(&ar->arsta_lock);
		ath12k_link_sta_hlist_delete(ar, arvif->self_arsta);
		spin_unlock_bh(&ar->arsta_lock);
		kfree(arvif->self_arsta);
		arvif->self_arsta = NULL;
	}

	ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
		   "ret:%d No. of vdev created:%d, links_map:0x%x, flag:%d\n",
		   ret,
		   hweight16(ahvif->links_map),
		   ahvif->links_map,
		   arvif->is_created);

	return ret;
}

static void ath12k_core_peer_disassoc(struct ath12k_hw_group *ag,
				      struct ath12k_base *assert_ab)
{
	struct ath12k_base *ab;
	struct ath12k_sta *ahsta;
	struct ieee80211_sta *sta;
	struct ath12k_pdev *pdev_iter;
	struct ath12k *ar_iter;
	struct ath12k_link_sta *arsta_iter;
	u32 __bkt;
	int radio_idx, i;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed)
			continue;

		if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2) {
			if (ab != assert_ab)
				continue;
		}

		for (radio_idx = 0; radio_idx < ab->num_radios; radio_idx++) {
			pdev_iter = &ab->pdevs[radio_idx];
			ar_iter = pdev_iter->ar;
			if (!ar_iter)
				continue;

			spin_lock_bh(&ar_iter->arsta_lock);
			ath12k_link_sta_for_each(ar_iter, __bkt, arsta_iter) {

				if (arsta_iter->is_self_peer)
					continue;

				/* Allow sending disassoc to legacy peer
				 * only for asserted radio
				 */
				if (!arsta_iter->ahsta->is_mlo && ab != assert_ab)
					continue;

				sta = ath12k_ahsta_to_sta(arsta_iter->ahsta);
				ahsta = arsta_iter->ahsta;

				/* Send low ack to disassoc the MLD station */
				ath12k_mac_peer_disassoc(ab, sta, ahsta,
							 ATH12K_DBG_MODE1_RECOVERY);
			}
			spin_unlock_bh(&ar_iter->arsta_lock);
		}
	}
}

static void ath12k_reset_group_key_slots(struct ath12k_link_vif *arvif,
					 struct ath12k_vif *ahvif)
{
	struct ath12k_vlan_iface *vlan_iface;

	bitmap_fill(arvif->free_groupidx_map, ATH12K_GROUP_KEYS_NUM_MAX);
	/* HW group idx 0 reserved, mark unavailable */
	clear_bit(0, arvif->free_groupidx_map);

	if (ahvif->vif &&
	    ahvif->vif->type == NL80211_IFTYPE_AP_VLAN) {
		vlan_iface = ahvif->vlan_iface;
		if (vlan_iface && !vlan_iface->is_wds_4addr)
			vlan_iface->grp_key_slot[arvif->link_id] =
				ATH12K_GROUP_KEY_SLOT_INVALID;
	}
}

/*
 * Compute, for one MLD peer, the bitmap of link ids that live on the asserted
 * chip and (optionally) the bitmap of links that survive on other chips.
 * Called under wiphy_lock, hence wiphy_dereference().
 */
static u16 ath12k_mode3_peer_link_masks(struct wiphy *wiphy,
					struct ath12k_sta *ahsta,
					struct ath12k_base *assert_ab,
					u16 *surviving)
{
	struct ath12k_link_sta *arsta;
	unsigned long links = ahsta->links_map;
	u16 asserted = 0, surv = 0;
	u8 link_id;

	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);
		if (!arsta || !arsta->arvif || !arsta->arvif->ar)
			continue;
		if (arsta->arvif->ar->ab == assert_ab)
			asserted |= BIT(link_id);
		else
			surv |= BIT(link_id);
	}

	if (surviving)
		*surviving = surv;
	return asserted;
}

/*
 * Abort Mode3 recovery for a single MLD peer: clear the per-link recovery flag
 * on every asserted link and reset the per-peer recovery bookkeeping, so the
 * subsequent readd phase does not attempt to re-add a peer we are giving up on.
 * The asserted_peer_pending / asserted_hw_link_id markers are intentionally
 * left intact: the peer will be disassociated and torn down, and the teardown
 * path still needs them to issue the manual HTT unmap.  Called under wiphy_lock.
 */
static void ath12k_mode3_abort_peer_recovery(struct wiphy *wiphy,
					     struct ath12k_sta *ahsta)
{
	struct ath12k_link_sta *arsta;
	unsigned long links = ahsta->recov.asserted_links;
	u8 link_id;

	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);
		if (arsta)
			arsta->is_mode3_link_recovery = false;
	}

	ahsta->recov.removed_links   = 0;
	ahsta->recov.added_links     = 0;
	ahsta->recov.asserted_links  = 0;
	ahsta->recov.master_crashed  = false;
	/* keep mode3_processed=true so the peer is not revisited from the
	 * sibling pdev's arsta_list within this same remove phase.
	 */
}

static bool ath12k_sta_is_assoc_on_ab(struct wiphy *wiphy,
				      struct ath12k_sta *ahsta,
				      struct ath12k_base *ab)
{
	struct ath12k_link_sta *arsta;

	arsta = wiphy_dereference(wiphy, ahsta->link[ahsta->assoc_link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar)
		return false;
	return arsta->arvif->ar->ab == ab;
}

static bool ath12k_sta_is_primary_on_ab(struct wiphy *wiphy,
					struct ath12k_sta *ahsta,
					struct ath12k_base *ab)
{
	struct ath12k_link_sta *arsta;

	arsta = wiphy_dereference(wiphy, ahsta->link[ahsta->primary_link_id]);
	if (!arsta || !arsta->arvif || !arsta->arvif->ar)
		return false;
	return arsta->arvif->ar->ab == ab;
}

/*
 * Select a new primary/master link for an MLD peer whose current primary link
 * is on the asserted chip.  A candidate must survive on a chip other than the
 * asserted one — a sibling link that also lives on the asserted chip is NOT a
 * valid choice.  Returns WMI_MLO_INVALID_MASTER_LL_ID when no surviving link
 * exists (e.g. all of the peer's links are on the asserted chip).
 */
static u8 ath12k_mode3_select_new_primary(struct wiphy *wiphy,
					  struct ath12k_sta *ahsta,
					  struct ath12k_base *assert_ab)
{
	struct ath12k_link_sta *arsta;
	unsigned long links = ahsta->links_map;
	u8 link_id;

	/* Fast path: assoc link survived on a non-asserted chip — use it */
	arsta = wiphy_dereference(wiphy, ahsta->link[ahsta->assoc_link_id]);
	if (arsta && arsta->arvif && arsta->arvif->ar &&
	    arsta->arvif->ar->ab != assert_ab) {
		ahsta->recov.new_primary_hwlink_id =
			arsta->arvif->ar->pdev->hw_link_id;
		return ahsta->assoc_link_id;
	}

	/* Otherwise pick any link that survives on a non-asserted chip */
	for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
		arsta = wiphy_dereference(wiphy, ahsta->link[link_id]);
		if (!arsta || !arsta->arvif || !arsta->arvif->ar)
			continue;
		if (arsta->arvif->ar->ab == assert_ab)
			continue;
		ahsta->recov.new_primary_hwlink_id = arsta->arvif->ar->pdev->hw_link_id;
		return link_id;
	}

	ahsta->recov.new_primary_hwlink_id = WMI_MLO_INVALID_MASTER_LL_ID;
	return WMI_MLO_INVALID_MASTER_LL_ID;
}

/*
 * ath12k_mode3_do_internal_migration - Wi-Fi 8 internal primary link update
 *
 * On Wi-Fi 8 (C-UMAC), FW does not send HTT_T2H_MSG_TYPE_PRIMARY_LINK_PEER_MIGRATE_IND.
 * The host must call hw_ops->dp_peer_migration() directly after PEER_ASSOC_CONF to
 * update primary_link_id and DP peer flags. This mirrors what the Wi-Fi 7 HTT
 * indication handler does after receiving FW confirmation.
 */
static void ath12k_mode3_do_internal_migration(struct ath12k_sta *ahsta,
					       struct ath12k_link_vif *arvif)
{
	const struct ath12k_hw_ops *hw_ops = arvif->ar->ab->hw_params->hw_ops;
	struct ath12k_mac_pri_link_migr_peer_node node = {};
	union ath12k_config_param val = {};
	void *dp_peer;

	if (!hw_ops || !hw_ops->dp_peer_migration)
		return;

	/* The Wi-Fi 8 dp_peer_migration op indexes dp_peer_list[] by the
	 * *global* peer_id (see ath12k_wifi8_dp_peer_add()), not by
	 * ahsta->ml_peer_id — which is ATH12K_MLO_PEER_ID_INVALID (0xFFFF) on
	 * Wi-Fi 8.  Feeding ml_peer_id here would index dp_peer_list[0xFFFF],
	 * far past MAX_DP_PEER_LIST_SIZE, and dereference garbage.  Resolve the
	 * global peer_id the same way the FW-event producer does
	 * (ath12k_mac_get_link_migr_peer_node()): read it from the dp_peer and
	 * strip the ATH12K_PEER_ML_ID_VALID flag bit.  Caller holds wiphy_lock,
	 * so ath12k_sta_get_dp_peer_wiphy_locked() is safe.
	 */
	dp_peer = ath12k_sta_get_dp_peer_wiphy_locked(arvif->ar->ah->hw->wiphy,
						      ahsta);
	if (!dp_peer) {
		ath12k_warn(arvif->ar->ab,
			    "Mode3: internal migration skipped, no dp_peer for %pM\n",
			    ahsta->addr);
		return;
	}

	if (ath12k_dp_peer_get_param_by_dp_peer(dp_peer,
						ATH12K_DP_PEER_PEERID_PARAM,
						&val)) {
		ath12k_warn(arvif->ar->ab,
			    "Mode3: internal migration skipped, no peer_id for %pM\n",
			    ahsta->addr);
		return;
	}

	node.ml_peer_id  = val.peer_id & ~ATH12K_PEER_ML_ID_VALID;
	node.pri_link_id = ahsta->primary_link_id;

	hw_ops->dp_peer_migration(arvif, &node);
}

static int ath12k_core_send_recovery_t2lm(struct ath12k_hw_group *ag)
{
	struct ath12k_wmi_tid_to_link_map_ap_params *map_params;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int i, j, k, n, ret;

	ath12k_info(ag->assert_ab,
		    "Mode3 Applying T2LM configuration(one link per MLD AP)\n");

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		/* Skip the asserted chip — not operational during Phase 2 */
		if (ab == ag->assert_ab || ab->is_bypassed)
			continue;

		map_params = kcalloc(TARGET_NUM_VDEVS, sizeof(*map_params),
				     GFP_KERNEL);
		if (!map_params)
			return -ENOMEM;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;
			if (!ar)
				continue;

			n = 0;
			spin_lock_bh(&ar->data_lock);
			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (!arvif->ahvif ||
				    arvif->ahvif->vdev_type != WMI_VDEV_TYPE_AP)
					continue;
				/* Skip repurposed links -- not eligible as T2LM anchor */
				if (arvif->ahvif->repurposed_links & BIT(arvif->link_id))
					continue;
				/*
				 * Per-MLD dedup: send T2LM only once per MLD AP.
				 * mode3_t2lm_sent is set on the first non-repurposed
				 * surviving link so sibling arvifs of the same MLD
				 * (e.g. on another radio of the same chip) are skipped.
				 */
				if (arvif->ahvif->mode3_t2lm_sent)
					continue;
				if (n >= TARGET_NUM_VDEVS)
					break;
				memset(&map_params[n], 0, sizeof(map_params[n]));
				rcu_read_lock();
				if (ath12k_mac_populate_recovery_t2lm_params(arvif,
						ag->assert_ab,
						&map_params[n], true)) {
					rcu_read_unlock();
					continue;
				}
				rcu_read_unlock();
				arvif->mode3_t2lm_anchor      = true;
				arvif->ahvif->mode3_t2lm_sent = true;
				n++;
			}
			spin_unlock_bh(&ar->data_lock);

			for (k = 0; k < n; k++) {
				ret = ath12k_wmi_ap_tid_to_link_map_config(ar,
								&map_params[k]);
				if (ret)
					ath12k_warn(ab,
						    "Mode3 T2LM set failed for vdev=%u hw_link=%u ret=%d Continuing for other vdevs..\n",
						    map_params[k].vdev_id,
						    map_params[k].hw_link_id, ret);
				else
					ath12k_info(ab,
						    "Mode3 T2LM anchor set successfully! vdev=%u hw_link=%u\n",
						    map_params[k].vdev_id,
						    map_params[k].hw_link_id);
			}
		}
		kfree(map_params);
	}
	ag->recovery_t2lm_active = true;
	ath12k_info(ag->assert_ab, "Mode3 Phase 2: T2LM anchors set\n");
	return 0;
}

static void ath12k_disassoc_non_mlo_peer(struct ath12k_sta *ahsta,
					 struct ath12k_hw_group *ag)
{
	struct ath12k_link_sta *arsta = &ahsta->deflink;

	if (arsta->arvif && arsta->arvif->ar &&
	    arsta->arvif->ar->ab == ag->assert_ab)
		ath12k_mac_peer_disassoc(ag->assert_ab, ath12k_ahsta_to_sta(ahsta),
					 ahsta, ATH12K_DBG_MODE1_RECOVERY);
}

static int ath12k_core_mlo_unified_peer_update(struct ath12k_hw_group *ag)
{
	struct ath12k_hw *ah = ath12k_ag_to_ah(ag, 0);
	struct ath12k_base *assert_ab = ag->assert_ab;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta, *arsta_iter;
	struct hlist_node *arsta_tmp;
	struct ath12k_link_vif *arvif;
	struct ieee80211_sta *sta;
	struct ath12k_wmi_peer_assoc_arg *peer_arg;
	struct ath12k *ar;
	u8 link_id;
	u16 asserted_links, surviving_links;
	bool master_crashed = 0;
	bool assoc_crashed = 0;
	unsigned long links;
	int j, ret = 0;
	u32 bkt;

	ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
		    "Mode3: link_remove_phase : Migrating clients if required\n");

	peer_arg = kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return -ENOMEM;

	wiphy_lock(ah->hw->wiphy);

	/*
	 * Walk only the arsta_list of the asserted chip's radios.
	 * Every arsta there has a link on the asserted chip — no need to scan
	 * the full dp_hw.peers list and filter per peer.  The asserted chip is
	 * down so its arsta_list is stable: no concurrent add/remove.
	 * arsta_lock is held to read ahsta, dropped for sleeping work, then
	 * re-acquired before the next iteration.
	 */
	for (j = 0; j < assert_ab->num_radios; j++) {
		ar = assert_ab->pdevs[j].ar;
		if (!ar)
			continue;

		spin_lock_bh(&ar->arsta_lock);
		ath12k_link_sta_for_each_safe(ar, bkt, arsta_iter, arsta_tmp) {
			if (arsta_iter->is_self_peer || !arsta_iter->ahsta)
				continue;

			ahsta            = arsta_iter->ahsta;
			spin_unlock_bh(&ar->arsta_lock);

			sta = ath12k_ahsta_to_sta(ahsta);

			if (!ahsta->is_mlo) {
				ath12k_disassoc_non_mlo_peer(ahsta, ag);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			/* Per-peer dedup: an MLD peer with links on more than one
			 * radio (pdev) of the asserted chip is reachable from both
			 * per-radio arsta_lists.  Process each peer exactly once so
			 * remove/migration/readd act on the full asserted-link set.
			 */
			if (ahsta->recov.mode3_processed) {
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}
			ahsta->recov.mode3_processed = true;

			if (hweight16(ahsta->links_map) <= 1) {
				ath12k_mac_peer_disassoc(assert_ab, sta, ahsta,
							 ATH12K_DBG_MODE1_RECOVERY);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			if (sta->reconf.added_links || sta->reconf.removed_links) {
				ath12k_warn(assert_ab,
					    "Mode3: %pM has MLO reconfig in progress (added=0x%x removed=0x%x), disconnecting\n",
					    sta->addr, sta->reconf.added_links,
					    sta->reconf.removed_links);
				ath12k_mac_peer_disassoc(assert_ab, sta, ahsta,
							 ATH12K_DBG_MODE1_RECOVERY);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			ahsta->recov.new_primary_hwlink_id = WMI_MLO_INVALID_MASTER_LL_ID;

			/* All links of this peer that live on the asserted chip,
			 * and the links that survive on other chips.
			 */
			asserted_links = ath12k_mode3_peer_link_masks(ah->hw->wiphy,
								      ahsta, assert_ab,
								      &surviving_links);
			ahsta->recov.asserted_links = asserted_links;

			if (!asserted_links) {
				/* Shouldn't happen: peer came from asserted chip's
				 * arsta_list but has no link there. Skip safely.
				 */
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			/* Mark every asserted link so DP/WMI access is blocked and
			 * the teardown path knows a manual HTT unmap is required.
			 * Done before the no-surviving-link check so that even a
			 * peer we disassociate upfront still has asserted_peer_pending
			 * set and gets its per-pdev TQM queues drained on teardown.
			 */
			links = asserted_links;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta = wiphy_dereference(ah->hw->wiphy,
							  ahsta->link[link_id]);
				if (arsta) {
					arsta->is_mode3_link_recovery = true;
					ath12k_mode3_set_dp_recovery_flag(assert_ab,
									  arsta, true);
				}
			}

			/* No surviving link (e.g. a 2-link peer with both links on
			 * two pdevs of the asserted chip): nothing to retain, so
			 * disassociate upfront before any link/migration work.
			 */
			if (!surviving_links) {
				ath12k_warn(assert_ab,
					    "Mode3: link_remove_phase: %pM has no surviving link, disassociating\n",
					    sta->addr);
				ath12k_mode3_abort_peer_recovery(ah->hw->wiphy, ahsta);
				ath12k_mac_peer_disassoc(assert_ab, sta, ahsta,
							 ATH12K_DBG_MODE1_RECOVERY);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			master_crashed = ath12k_sta_is_primary_on_ab(ah->hw->wiphy,
								     ahsta, assert_ab);
			assoc_crashed  = ath12k_sta_is_assoc_on_ab(ah->hw->wiphy,
								   ahsta, assert_ab);

			if (master_crashed) {
				ahsta->recov.new_master_ll_id =
					ath12k_mode3_select_new_primary(
						ah->hw->wiphy, ahsta, assert_ab);
				if (ahsta->recov.new_master_ll_id ==
				    WMI_MLO_INVALID_MASTER_LL_ID) {
					/* surviving_links != 0 was verified above,
					 * so this indicates an inconsistent peer
					 * state — disassociate.
					 */
					ath12k_mode3_abort_peer_recovery(ah->hw->wiphy,
									 ahsta);
					ath12k_mac_peer_disassoc(
						assert_ab, sta, ahsta,
						ATH12K_DBG_MODE1_RECOVERY);
					spin_lock_bh(&ar->arsta_lock);
					continue;
				}
				ath12k_warn(assert_ab,
					    "Mode3: link_remove_phase: Primary migration for %pM chosen new link id %d\n",
					    sta->addr, ahsta->recov.new_master_ll_id);
				ahsta->primary_link_id = ahsta->recov.new_master_ll_id;
			} else {
				ahsta->recov.new_master_ll_id =
					WMI_MLO_INVALID_MASTER_LL_ID;
				ath12k_warn(assert_ab,
					    "Mode3: link_remove_phase : No Primary migration for %pM  new link id %d\n",
					    sta->addr, ahsta->recov.new_master_ll_id);
			}

			ahsta->recov.master_crashed = master_crashed;

			if (assoc_crashed)
				ahsta->recov.assoc_link = ahsta->primary_link_id;

			/* Full asserted mask so ath12k_peer_assoc_h_mlo() marks
			 * every asserted link (self or partner) as mlo_link_del.
			 */
			ahsta->recov.removed_links = asserted_links;

			/* Per-peer capability check: if master migration is not
			 * supported (old-style Wi-Fi 7 PTQM only), fall back to
			 * Mode2 for this peer.
			 */
			if (!ath12k_wmi_is_master_migration_supported(assert_ab)) {
				ath12k_warn(assert_ab,
					    "Mode3:  Primary migration not supported %pM, disassociating\n",
					    sta->addr);
				ath12k_mode3_abort_peer_recovery(ah->hw->wiphy, ahsta);
				ath12k_mac_peer_disassoc(assert_ab, sta, ahsta,
							 ATH12K_DBG_MODE1_RECOVERY);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			/* Inform every surviving link that the asserted link(s)
			 * are being removed.
			 */
			links = surviving_links;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta = wiphy_dereference(ah->hw->wiphy,
							  ahsta->link[link_id]);
				if (!arsta || !arsta->arvif || !arsta->arvif->ar)
					continue;
				arvif = arsta->arvif;

				memset(peer_arg, 0, sizeof(*peer_arg));
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
				ath12k_peer_assoc_prepare(arvif->ar, arvif,
					arsta, peer_arg, false,
					wiphy_dereference(ah->hw->wiphy,
						sta->link[link_id]));
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

				ath12k_info(arvif->ar->ab,
					    "Mode3 link_remove_phase: peer_assoc link_id=%u peer=%pM vdev=%u phymode=%u nss=%u flags=0x%x caps=0x%x ht_flag=%d vht_flag=%d he_flag=%d eht_flag=%d bw_40=%d bw_80=%d bw_160=%d bw_320=%d mlo_enabled=%d assoc_link=%d primary_umac=%d ll_idx=%u ml_peer_id=%u ml_recovery=%d new_master_ll_id=%u num_partners=%u link_add=%u link_del=%u\n",
					    link_id, peer_arg->peer_mac,
					    peer_arg->vdev_id, peer_arg->peer_phymode,
					    peer_arg->peer_nss, peer_arg->peer_flags,
					    peer_arg->peer_caps,
					    peer_arg->ht_flag, peer_arg->vht_flag,
					    peer_arg->he_flag, peer_arg->eht_flag,
					    peer_arg->bw_40, peer_arg->bw_80,
					    peer_arg->bw_160, peer_arg->bw_320,
					    peer_arg->ml.enabled,
					    peer_arg->ml.assoc_link,
					    peer_arg->ml.primary_umac,
					    peer_arg->ml.logical_link_idx,
					    peer_arg->ml.ml_peer_id,
					    peer_arg->ml.ml_recovery_reconfig,
					    peer_arg->ml.new_master_ll_id,
					    peer_arg->ml.num_partner_links,
					    peer_arg->ml.mlo_link_add,
					    peer_arg->ml.mlo_link_del);

				if (ath12k_wmi_send_peer_assoc_cmd(
					    arvif->ar, peer_arg) ||
				    !wait_for_completion_timeout(
					    &arvif->ar->peer_assoc_done,
					    1 * HZ)) {
					ath12k_warn(arvif->ar->ab,
						    "Mode3: link_remove_phase peer assoc/timeout for %pM link %u\n",
						    sta->addr, link_id);
					goto fallback;
				}

				/* unblock data TIDs on this surviving link */
				ath12k_mac_station_authorize(arvif->ar, arvif, arsta);
			}

			arvif = ath12k_get_arvif_from_link_id(
					ahsta->ahvif, ahsta->primary_link_id);
			if (!arvif) {
				ath12k_err(NULL, "Mode3: Unable to fetch vif for migration\n");
				goto fallback;
			}

			if (master_crashed)
				ath12k_mode3_do_internal_migration(ahsta, arvif);
			if (assoc_crashed)
				ath12k_mac_migrate_assoc_link(ahsta,
					ahsta->primary_link_id);

			ahsta->recov.removed_links = 0;
			ath12k_info(assert_ab,
				    "Mode3 link_remove_phase migrated %pM (asserted links 0x%x → primary link %u)\n",
				    sta->addr, asserted_links, ahsta->primary_link_id);
			spin_lock_bh(&ar->arsta_lock);
			continue;

fallback:
			ath12k_mode3_abort_peer_recovery(ah->hw->wiphy, ahsta);
			ath12k_warn(assert_ab,
				    "Mode3: link_remove_phase peer update failed for %pM, disassociating\n",
				    sta->addr);
			ath12k_mac_peer_disassoc(assert_ab, sta, ahsta,
						 ATH12K_DBG_MODE1_RECOVERY);
			spin_lock_bh(&ar->arsta_lock);
		}
		spin_unlock_bh(&ar->arsta_lock);
	}

	wiphy_unlock(ah->hw->wiphy);
	kfree(peer_arg);
	return ret;
}

int ath12k_recovery_peer_create(struct ath12k_link_vif *arvif,
				struct ath12k_link_sta *arsta,
				struct ieee80211_sta *sta)
{
	struct ath12k_wmi_peer_create_arg arg = {};
	struct ath12k_dp_peer *dp_peer = NULL;
	struct ath12k_dp_hw *dp_hw = NULL;

	arg.vdev_id    = arvif->vdev_id;
	arg.peer_addr  = arsta->addr;
	arg.peer_type  = WMI_PEER_TYPE_DEFAULT;
	arg.ml_enabled = sta->mlo;

	dp_hw = &arvif->ar->ah->dp_hw;
	spin_lock_bh(&dp_hw->peer_hash_lock);
	dp_peer = ath12k_dp_peer_find_by_addr_and_sta(dp_hw,
					       sta->addr, sta);
	spin_unlock_bh(&dp_hw->peer_hash_lock);

	if (dp_peer) {
		arg.peer_id    = dp_peer->peer_id;
		arg.sta_id     = dp_peer->sta_id;
	}

	return ath12k_peer_create(arvif->ar, arvif, sta, &arg);
}

static int ath12k_core_mlo_link_readd(struct ath12k_hw_group *ag)
{
	struct ath12k_base *assert_ab = ag->assert_ab;
	struct ath12k_sta *ahsta;
	struct ath12k_link_sta *arsta, *arsta_s, *arsta_a;
	struct hlist_node *arsta_tmp;
	struct ath12k_link_vif *arvif_s, *arvif_a;
	struct ieee80211_sta *sta;
	struct ath12k_wmi_peer_assoc_arg *peer_arg;
	struct ieee80211_link_sta *link_sta;
	struct ath12k *ar;
	u8 link_id;
	u16 asserted_links;
	unsigned long links;
	int j, ret = 0;
	u32 flags = 0, bkt;

	ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
		   "Mode3 Re-adding Asserted link back on each peer: num_radios=%d\n",
		    assert_ab->num_radios);

	peer_arg = kzalloc(sizeof(*peer_arg), GFP_KERNEL);
	if (!peer_arg)
		return -ENOMEM;

	for (j = 0; j < assert_ab->num_radios; j++) {
		ar = assert_ab->pdevs[j].ar;
		if (!ar)
			continue;

		spin_lock_bh(&ar->arsta_lock);
		ath12k_link_sta_for_each_safe(ar, bkt, arsta, arsta_tmp) {
			if (arsta->is_self_peer || !arsta->ahsta)
				continue;
			if (!arsta->is_mode3_link_recovery)
				continue;

			ahsta = arsta->ahsta;
			spin_unlock_bh(&ar->arsta_lock);

			sta = ath12k_ahsta_to_sta(ahsta);

			/* Per-peer dedup: an MLD peer with links on two pdevs of
			 * the asserted chip is reachable from both per-radio
			 * arsta_lists.  The full asserted-link set was recorded in
			 * the remove phase; process the peer once here.  A peer
			 * already handled has all its is_mode3_link_recovery flags
			 * cleared, so the check above skips its second arsta — but
			 * guard on mode3_processed too in case flags linger.
			 */
			if (!ahsta->recov.mode3_processed) {
				ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
					   "Mode3 link_readd: skip %pM mode3_processed=0\n",
					   sta->addr);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			asserted_links = ahsta->recov.asserted_links;
			if (!asserted_links) {
				ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
					   "Mode3 link_readd: skip %pM asserted_links=0\n",
					   sta->addr);
				spin_lock_bh(&ar->arsta_lock);
				continue;
			}

			ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
				   "Mode3 link_readd: PROCESS %pM asserted_links=0x%x links_map=0x%x primary_link=%u assoc_link=%u\n",
				   sta->addr, asserted_links, ahsta->links_map,
				   ahsta->primary_link_id,
				   ahsta->assoc_link_id);

			ahsta->recov.added_links      = asserted_links;
			ahsta->recov.new_master_ll_id = WMI_MLO_INVALID_MASTER_LL_ID;

			/* Step 1: create the peer on EVERY asserted link first.
			 * A subsequent peer_assoc on one asserted link references
			 * its sibling asserted link as a partner, so all asserted
			 * peers must exist in FW before any peer_assoc is sent.
			 */
			links = asserted_links;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta_a = wiphy_dereference(ag->ah[0]->hw->wiphy,
							    ahsta->link[link_id]);
				if (!arsta_a || !arsta_a->arvif) {
					ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
						   "Mode3: link_readd: no arsta/arvif for %pM link %u\n",
						   sta->addr, link_id);
					goto readd_fallback;
				}
				arvif_a = arsta_a->arvif;

				ret = ath12k_recovery_peer_create(arvif_a, arsta_a, sta);
				if (ret) {
					ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
						   "Mode3: peer create failed for %pM link %u: %d\n",
						   sta->addr, link_id, ret);
					goto readd_fallback;
				}
			}

			/* Step 2: peer_assoc on each asserted (re-added) link. */
			links = asserted_links;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta_a = wiphy_dereference(ag->ah[0]->hw->wiphy,
							    ahsta->link[link_id]);
				if (!arsta_a || !arsta_a->arvif || !arsta_a->arvif->ar)
					goto readd_fallback;
				arvif_a = arsta_a->arvif;

				memset(peer_arg, 0, sizeof(*peer_arg));
				link_sta = wiphy_dereference(ag->ah[0]->hw->wiphy,
							     sta->link[link_id]);
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
				ath12k_peer_assoc_prepare(arvif_a->ar, arvif_a,
							  arsta_a, peer_arg,
							  false, link_sta);
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

				ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
					   "Mode3 link_readd_phase : peer_assoc (asserted link) link_id=%u peer=%pM vdev=%u phymode=%u nss=%u flags=0x%x caps=0x%x ht_flag=%d vht_flag=%d he_flag=%d eht_flag=%d bw_40=%d bw_80=%d bw_160=%d bw_320=%d mlo_enabled=%d assoc_link=%d primary_umac=%d ll_idx=%u ml_peer_id=%u ml_recovery=%d new_master_ll_id=%u num_partners=%u mlo link add=%u mlo link del=%u\n",
					   link_id, peer_arg->peer_mac,
					   peer_arg->vdev_id, peer_arg->peer_phymode,
					   peer_arg->peer_nss, peer_arg->peer_flags,
					   peer_arg->peer_caps,
					   peer_arg->ht_flag, peer_arg->vht_flag,
					   peer_arg->he_flag, peer_arg->eht_flag,
					   peer_arg->bw_40, peer_arg->bw_80,
					   peer_arg->bw_160, peer_arg->bw_320,
					   peer_arg->ml.enabled,
					   peer_arg->ml.assoc_link,
					   peer_arg->ml.primary_umac,
					   peer_arg->ml.logical_link_idx,
					   peer_arg->ml.ml_peer_id,
					   peer_arg->ml.ml_recovery_reconfig,
					   peer_arg->ml.new_master_ll_id,
					   peer_arg->ml.num_partner_links,
					   peer_arg->ml.mlo_link_add,
					   peer_arg->ml.mlo_link_del);

				ret = ath12k_wmi_send_peer_assoc_cmd(arvif_a->ar,
								     peer_arg);
				if (ret || !wait_for_completion_timeout(
					    &arvif_a->ar->peer_assoc_done, 1 * HZ)) {
					ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
						   "Mode3: link_readd_phase peer assoc failed for %pM link %u: %d\n",
						   sta->addr, link_id, ret);
					goto readd_fallback;
				}

				/* FW was reset; re-send REO default routing so RX
				 * frames land on the correct ring for this link.
				 */
				ret = ath12k_dp_peer_set_default_routing(arvif_a->ar,
									 arvif_a,
									 arsta_a->addr);
				if (ret) {
					ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
						   "Mode3: link_readd_phase set_default_routing failed for %pM link %u: %d\n",
						   sta->addr, link_id, ret);
					goto readd_fallback;
				}
			}

			/* Step 3: peer_assoc on all surviving (non-asserted) links.
			 *
			 * The FW on each surviving chip must be informed that the
			 * asserted link(s) have been re-added.  ath12k_peer_assoc_h_mlo()
			 * uses recov.added_links to set mlo_link_add=true in the
			 * partner_info TLV for every re-added link.
			 */
			links = ahsta->links_map;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				if (asserted_links & BIT(link_id))
					continue;
				arsta_s = wiphy_dereference(ag->ah[0]->hw->wiphy,
							    ahsta->link[link_id]);
				if (!arsta_s || !arsta_s->arvif || !arsta_s->arvif->ar)
					continue;
				arvif_s = arsta_s->arvif;

				memset(peer_arg, 0, sizeof(*peer_arg));
				link_sta = wiphy_dereference(ag->ah[0]->hw->wiphy,
							     sta->link[link_id]);
#ifdef CPTCFG_QCN_EXTN_MESH_SUPPORT
				ath12k_peer_assoc_prepare(arvif_s->ar,
							  arvif_s,
							  arsta_s,
							  peer_arg,
							  false, link_sta);
#endif /* CPTCFG_QCN_EXTN_MESH_SUPPORT */

				ath12k_dbg(arvif_s->ar->ab, ATH12K_DBG_MODE1_RECOVERY,
					   "Mode3 link_readd_phase: peer_assoc (surviving) link_id=%u peer=%pM vdev=%u mlo_enabled=%d assoc_link=%d primary_umac=%d ll_idx=%u ml_peer_id=%u ml_recovery=%d new_master_ll_id=%u num_partners=%u link_add=%u link_del=%u\n",
					   link_id, peer_arg->peer_mac,
					   peer_arg->vdev_id,
					   peer_arg->ml.enabled,
					   peer_arg->ml.assoc_link,
					   peer_arg->ml.primary_umac,
					   peer_arg->ml.logical_link_idx,
					   peer_arg->ml.ml_peer_id,
					   peer_arg->ml.ml_recovery_reconfig,
					   peer_arg->ml.new_master_ll_id,
					   peer_arg->ml.num_partner_links,
					   peer_arg->ml.mlo_link_add,
					   peer_arg->ml.mlo_link_del);

				ret = ath12k_wmi_send_peer_assoc_cmd(
					arvif_s->ar, peer_arg);
				if (ret || !wait_for_completion_timeout(
					    &arvif_s->ar->peer_assoc_done, 1 * HZ)) {
					ath12k_dbg(arvif_s->ar->ab,
						   ATH12K_DBG_MODE1_RECOVERY,
						   "Mode3: link_readd_phase peer assoc (surviving) failed for %pM link %u: %d\n",
						   sta->addr, link_id, ret);
					goto readd_fallback;
				}
			}
			ahsta->recov.added_links = 0;

			/* Step 4: install pairwise key + authorize each asserted link. */
			links = asserted_links;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta_a = wiphy_dereference(ag->ah[0]->hw->wiphy,
							    ahsta->link[link_id]);
				if (!arsta_a || !arsta_a->arvif)
					goto readd_fallback;
				arvif_a = arsta_a->arvif;

				ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
					   "Mode3 link_readd_phase key install %pM on asserted link %u\n",
					   sta->addr, link_id);

				flags = 0;
				for (int ki = 0; ki < ARRAY_SIZE(arsta_a->keys); ki++) {
					if (!arsta_a->keys[ki])
						continue;
					if (!(arsta_a->keys[ki]->flags &
					     IEEE80211_KEY_FLAG_PAIRWISE))
						continue;
					flags |= WMI_KEY_PAIRWISE;
					ret = ath12k_install_key(arvif_a,
								 arsta_a->keys[ki],
								 SET_KEY, arsta_a->addr,
								 flags, NULL);
					if (ret) {
						ath12k_warn(assert_ab, "failed to add peer key %d: %d\n",
							    ki, ret);
						goto readd_fallback;
					}
					break;
				}

				ret = ath12k_mac_station_authorize(arvif_a->ar, arvif_a,
								   arsta_a);
				if (ret) {
					ath12k_warn(assert_ab, "Unable to authorize peer %pM vdev %d: %d\n",
						    sta->addr, arvif_a->vdev_id, ret);
					goto readd_fallback;
				}

				arsta_a->is_mode3_link_recovery = false;
			}

			/* Clear the ahsta-level recovery-pending marker once, after
			 * the last asserted link is restored.
			 */
			arsta_a = wiphy_dereference(ag->ah[0]->hw->wiphy,
						    ahsta->link[ffs(asserted_links) - 1]);
			if (arsta_a)
				ath12k_mode3_set_dp_recovery_flag(assert_ab, arsta_a,
								  false);

			ahsta->recov.master_crashed = false;
			ahsta->recov.asserted_links = 0;
			ahsta->recov.mode3_processed = false;
			ath12k_info(assert_ab,
				    "Mode3 link_readd_phase: restored %pM on asserted links 0x%x\n",
				    sta->addr, asserted_links);
			spin_lock_bh(&ar->arsta_lock);
			continue;

readd_fallback:
			/* Clear recovery flag on every asserted link of this peer. */
			links = asserted_links;
			for_each_set_bit(link_id, &links, ATH12K_NUM_MAX_LINKS) {
				arsta_a = wiphy_dereference(ag->ah[0]->hw->wiphy,
							    ahsta->link[link_id]);
				if (arsta_a)
					arsta_a->is_mode3_link_recovery = false;
			}
			ahsta->recov.added_links = 0;
			ahsta->recov.removed_links = 0;
			ahsta->recov.master_crashed = false;
			ahsta->recov.asserted_links = 0;
			ahsta->recov.mode3_processed = false;
			ath12k_dbg(assert_ab, ATH12K_DBG_MODE1_RECOVERY,
				   "Mode3: link_readd failed: disassociating peer %pM asserted_peer_pending=%d hw_link=%u - manual HTT cleanup\n",
				   sta->addr, ahsta->recov.asserted_peer_pending,
				   ahsta->recov.asserted_hw_link_id);
			ath12k_mac_peer_disassoc(assert_ab, sta, ahsta,
						 ATH12K_DBG_MODE1_RECOVERY);
			spin_lock_bh(&ar->arsta_lock);
		}
		spin_unlock_bh(&ar->arsta_lock);
	}

	kfree(peer_arg);
	return ret;
}

static int ath12k_core_clear_recovery_t2lm(struct ath12k_hw_group *ag)
{
	struct ath12k_wmi_tid_to_link_map_ap_params *map_params;
	struct ath12k_link_vif *arvif;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int i, j, k, n;

	/*
	 * Clear T2LM on every arvif marked as T2LM anchor in Phase 2.
	 * Multiple MLD APs may share the same radio; collect all matching
	 * anchors per radio under data_lock then send WMI outside the lock.
	 */
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (ab->is_bypassed)
			continue;

		map_params = kcalloc(TARGET_NUM_VDEVS, sizeof(*map_params),
				     GFP_KERNEL);
		if (!map_params)
			return -ENOMEM;

		for (j = 0; j < ab->num_radios; j++) {
			ar = ab->pdevs[j].ar;
			if (!ar)
				continue;

			n = 0;
			spin_lock_bh(&ar->data_lock);
			list_for_each_entry(arvif, &ar->arvifs, list) {
				if (!arvif->mode3_t2lm_anchor)
					continue;
				if (n >= TARGET_NUM_VDEVS)
					break;
				memset(&map_params[n], 0, sizeof(map_params[n]));
				rcu_read_lock();
				if (ath12k_mac_populate_recovery_t2lm_params(
						arvif, ag->assert_ab,
						&map_params[n], false)) {
					arvif->mode3_t2lm_anchor      = false;
					arvif->ahvif->mode3_t2lm_sent = false;
					rcu_read_unlock();
					continue;
				}
				rcu_read_unlock();
				arvif->mode3_t2lm_anchor      = false;
				arvif->ahvif->mode3_t2lm_sent = false;
				n++;
			}
			spin_unlock_bh(&ar->data_lock);

			for (k = 0; k < n; k++) {
				ath12k_wmi_ap_tid_to_link_map_config(ar, &map_params[k]);
				ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
					   "Mode3 T2LM cleared on vdev=%u hw_link=%u\n",
					   map_params[k].vdev_id,
					   map_params[k].hw_link_id);
			}
		}
		kfree(map_params);
	}
	ag->recovery_t2lm_active = false;
	ag->assert_ab             = NULL;
	ath12k_info(ag->ab[0], "Mode3  T2LM cleared, recovery complete\n");
	return 0;
}

/* Wrapper function for recovery after crash
 * This recovery function will be called for
 * both Mode 1 and Mode 2. Because both Mode
 * will recover only the crashed radio
 * without affecting the other active radio
 */

int ath12k_recovery_reconfig(struct ath12k_base *ab)
{
	struct ath12k *ar = NULL;
	struct ath12k_pdev *pdev;
	struct ath12k_link_vif *arvif, *tmp;
	struct ath12k_vif *ahvif ;
	struct ieee80211_bss_conf *link;
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_base *partner_ab;
	struct ath12k_hw *ah = ath12k_ag_to_ah(ag, 0);
	struct ieee80211_key_conf *key;
	struct ieee80211_chanctx_conf *bridge_ctx = NULL;
	int i, j, key_idx;
	int ret = -EINVAL;
	bool is_bridge_vdev;

	wiphy_lock(ah->hw->wiphy);

	if (!ath12k_ftm_mode) {
		ret = ath12k_mac_op_start(ah->hw);
		if (ret) {
			ath12k_err(ab, "mac radio start failed\n");
			wiphy_unlock(ah->hw->wiphy);
			return ret;
		}
	}

	/* add chanctx/hw_config/filter part */
	for (j = 0; j < ab->num_radios; j++) {
		pdev = &ab->pdevs[j];
		ar = pdev->ar;

		if (!ar || ar->ab->is_bypassed)
			continue;

		list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
			ahvif = arvif->ahvif;

			is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

			if (!ahvif)
				continue;

			if (!is_bridge_vdev && !arvif->chanctx.def.chan)
				continue;

			if (is_bridge_vdev) {
				link = NULL;
				if (!arvif->chanctx.def.chan) {
					bridge_ctx =
					ath12k_mac_get_ctx_for_bridge_recovery(arvif->ar);
					ath12k_dbg(arvif->ar->ab, ATH12K_DBG_MAC,
						   "[radio_idx : %u] bridge recovery chanctx link_id=%u addr=%pM freq %d\n",
						   arvif->ar->radio_idx, arvif->link_id,
						   arvif->addr,
						   bridge_ctx ?
						   bridge_ctx->def.chan->center_freq : 0);
					if (bridge_ctx)
						memcpy(&arvif->chanctx, bridge_ctx,
						       sizeof(*bridge_ctx));
				}
			} else {
				rcu_read_lock();
				link = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);

				/* Not expected */
				if (WARN_ON(!link)) {
					rcu_read_unlock();
					continue;
				}
				rcu_read_unlock();
			}
			ret = ath12k_mlo_core_recovery_reconfig_link_bss(ar, link, ahvif, arvif);

			if (ret) {
				ath12k_err(ab, "ERROR in reconfig link:%d\n", ret);
				wiphy_unlock(ah->hw->wiphy);
                                return ret;
			}
			ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
				   "vdev_created getting incremented:%d\n",
				   hweight16(ahvif->links_map));
		}
		ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY, "assign chanctx is completed\n");
	}

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed)
			continue;
		clear_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags);
	}

	/* reconfig_link_bss */
	for (j = 0; j < ab->num_radios; j++) {
		pdev = &ab->pdevs[j];
		ar = pdev->ar;

		if (!ar || ar->ab->is_bypassed)
			continue;

		list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
			ahvif = arvif->ahvif;
			is_bridge_vdev = ath12k_mac_is_bridge_vdev(arvif);

			if (!ahvif)
				continue;

			if (!is_bridge_vdev && !arvif->chanctx.def.chan)
				continue;

			if (is_bridge_vdev) {
				switch (ahvif->vdev_type) {
				case WMI_VDEV_TYPE_AP:
					ath12k_mac_bridge_vdev_up(arvif);
					break;
				case WMI_VDEV_TYPE_STA:
					link = NULL;
					goto skip_link_info;
				default:
					break;
				}
				continue;
			}

			rcu_read_lock();
			link = rcu_dereference(ahvif->vif->link_conf[arvif->link_id]);

			/* Not expected */
			if (WARN_ON(!link)) {
				rcu_read_unlock();
				continue;
			}
			rcu_read_unlock();

skip_link_info:
			ret = ath12k_mlo_recovery_link_vif_reconfig(ar, ahvif,
								    arvif,
								    arvif->ahvif->vif,
								    link);
			if (ret) {
				ath12k_err(ab, "Failed to update reconfig_bss\n");
				wiphy_unlock(ah->hw->wiphy);
				return ret;
			}
		}
	}

	/* TODO: Need to check for STA BVAP case */
	/* recover station VIF enabled in non-asserted links */
	ath12k_core_mlo_recover_station(ag, ab);

	/* sta state part */
	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed)
			continue;

		if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2 ||
		    ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3)
			continue;

		for (j = 0; j < partner_ab->num_radios; j++) {
			pdev = &partner_ab->pdevs[j];
			ar = pdev->ar;

			if (!ar)
				continue;

			if (list_empty(&ar->arvifs))
				continue;

			list_for_each_entry_safe_reverse(arvif, tmp, &ar->arvifs, list) {
				ahvif = arvif->ahvif;

				if (!ahvif)
					continue;

				if (ahvif->vdev_type != WMI_VDEV_TYPE_STA)
					ath12k_core_iterate_sta_list(ar, arvif);

				if (ath12k_mac_is_bridge_vdev(arvif))
					continue;

				spin_lock_bh(&ar->arsta_lock);
				if (!ath12k_link_sta_find_by_addr(ar, arvif->bssid)) {
					ath12k_info(ab, "Failed to fetch the peer during reconfig\n");
					spin_unlock_bh(&ar->arsta_lock);
					continue;
				}
				spin_unlock_bh(&ar->arsta_lock);

				ath12k_reset_group_key_slots(arvif, ahvif);
				for (key_idx = 0; key_idx < WMI_MAX_KEY_INDEX; key_idx++) {
					key = arvif->keys[key_idx];
					if (key) {
						ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
								"key:%p cipher:%d idx:%d flags:%d\n",
								key, key->cipher, key->keyidx, key->flags);
						ret = ath12k_mac_set_key(arvif->ar,
									 SET_KEY, arvif,
									 NULL, key, NULL);
					}
				}
			}
		}
	}

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3) {
		ath12k_core_mlo_link_readd(ag);
		ath12k_core_clear_recovery_t2lm(ag);
	} else {
		ath12k_core_peer_disassoc(ag, ab);
	}
	ab->recovery_start = false;
	ath12k_info(ab, "Mode %d recovery completed\n", ag->recovery_mode - 1);
	ath12k_mac_reconfig_complete(ah->hw, IEEE80211_RECONFIG_TYPE_RESTART);
	wiphy_unlock(ah->hw->wiphy);
	return ret;
}

/* this recovery work is called for both
 * mode 1 and mode 2 during the recovery.
 */
static void ath12k_core_recovery_work(struct work_struct *work)
{
	struct ath12k_base *ab = container_of(work, struct ath12k_base, recovery_work);
	if (ab->is_bypassed)
		return;

	ath12k_info(ab, "queued recovery work\n");
	ath12k_recovery_reconfig(ab);
}

void ath12k_core_trigger_bug_on(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	int dump_count;

	/* Crash the system once all the stats are dumped */
	if (ab->in_panic)
		return;

	if (ag->mlo_capable) {
		dump_count = atomic_read(&ath12k_coredump_ram_info.num_chip);
		if (dump_count >= ATH12K_MAX_SOCS) {
			ath12k_err(ab, "invalid chip number %d\n",
					dump_count);
			return;
		}
	}

	atomic_inc(&ath12k_coredump_ram_info.num_chip);
	ath12k_core_issue_bug_on(ab);
}

static void ath12k_core_update_userpd_state(struct work_struct *work)
{
	struct ath12k_hw_group *ag = container_of(work, struct ath12k_hw_group, reset_group_work);
	struct ath12k_base *ab;
	struct ath12k_ahb *ab_ahb = NULL;
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed)
			continue;

		if (ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID) {
			ath12k_hal_dump_srng_stats(ab);
			ab_ahb = ath12k_ab_to_ahb(ab);

			ab_ahb->crash_type = ATH12K_RPROC_ROOTPD_CRASH;
			set_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags);

			if (!ab->is_reset)
				ath12k_hif_irq_disable(ab);

			if ((ab->fw_recovery_support ||
			     ath12k_upload_dump_before_bugon) &&
			    !test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags))
				set_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags);
			else
				ath12k_core_trigger_bug_on(ab);
		}
	}
}

/* Asserted target's reboot handling for crash type ATH12K_RPROC_USERPD_CRASH */
static void ath12k_core_upd_power_down(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb = ath12k_ab_to_ahb(ab);

	/*
	 * Stop user pd
	 * Collect coredump using user pd
	 */
	if (ab_ahb->crash_type == ATH12K_RPROC_USERPD_CRASH) {
		ath12k_hif_power_down(ab, false);
		ath12k_coredump_ahb_collect(ab);
	}

	ab_ahb->crash_type = ATH12K_NO_CRASH;
}

/*
 * Trigger umac_reset with umac_reset flag set. This is a
 * waiting function which will return only after UMAC reset
 * is complete on non-asserted chip set. UMAC reset completion
 * is identified by waiting for MLO Teardown complete for all
 * chipsets
 */

static int ath12k_core_trigger_umac_reset(struct ath12k_base *ab,
					  enum wmi_mlo_tear_down_reason_code_type reason_code)
{
	struct ath12k_hw_group *ag = ab->ag;
	long time_left;
	int ret = 0;

	reinit_completion(&ag->umac_reset_complete);

	ath12k_mac_mlo_teardown_with_umac_reset(ab, reason_code);

	time_left = wait_for_completion_timeout(&ag->umac_reset_complete,
			msecs_to_jiffies(DP_UMAC_RESET_TIMEOUT_IN_MS));

	if (!time_left) {
		ath12k_warn(ab, "UMAC reset didn't get completed within %d ms\n",
			    DP_UMAC_RESET_TIMEOUT_IN_MS);
		ret = -ETIMEDOUT;
	}

	ag->trigger_umac_reset = false;
	return ret;
}

void ath12k_core_trigger_partner_device_crash(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_ahb *ab_ahb = NULL;
	struct ath12k_base *partner_ab;
	int i;

	lockdep_assert_held(&ag->mutex);

	if (ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID)
		ab_ahb = ath12k_ab_to_ahb(ab);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed || ab == partner_ab)
			continue;

		/* If the partner chip is either AHB/Hybrid
		 * and if it is a rootpd crash then the userpd
		 * won't be responding to the FW Hang command
		 * So skip to send it for AHB or Hybrid SOC's
		 * in case of a rootpd crash as queueing this
		 * reset_work will be taken care of AHB/Hybrid
		 * inside ath12k_ahb_queue_all_userpd_reset().
		 */
		if (partner_ab->hif.bus != ATH12K_BUS_PCI && ab_ahb
				&& ab_ahb->crash_type == ATH12K_RPROC_ROOTPD_CRASH)
			continue;

		/* issue FW Hang command on partner chips for Mode0. This is a fool proof
		 * method to ensure recovery of all partner chips in MODE0 instead of
		 * relying on firmware to crash partner chips
		 */
		clear_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags);
		if (!test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags)) {
			ath12k_info(ab, "sending fw_hang cmd to partner chipset(s)\n");
			set_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags);
			partner_ab->qmi.num_radios = U8_MAX;
			ath12k_wmi_force_fw_hang_cmd(partner_ab->pdevs[0].ar,
					ATH12K_WMI_FW_HANG_ASSERT_TYPE,
					ATH12K_WMI_FW_HANG_DELAY, true);
		}
	}
}
EXPORT_SYMBOL(ath12k_core_trigger_partner_device_crash);

static void ath12k_partner_chip_power_state_info(struct ath12k_hw_group *ag,
						 u8 power_state)
{
	struct ath12k_base *ab;
	int i, ret;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_reset)
			continue;

		if (ab->hif.bus == ATH12K_BUS_PCI) {
			ret = ath12k_qmi_partner_chip_power_info_send(ab, power_state);

			if (ret < 0)
				ath12k_err(ab, "Failed to send the power state for the chip\n");
		}
	}

	ag->block_peer_create = (power_state == FW_ASSERTED_CHIP_PWR_DOWN);
}

static void ath12k_core_disable_ext_irq_during_recovery(struct ath12k_base *ab)
{
	struct ath12k_ahb *ab_ahb;

	if (!ab->is_reset) {
		/* Disable IRQs only for PCI bus and AHB bus in case of userPD crash
		 * IRQs will be disabled for AHB rootPD crash from rootPD crash notifier
		 */
		switch (ab->hif.bus) {
		case ATH12K_BUS_AHB:
		case ATH12K_BUS_HYBRID:
			ab_ahb = ath12k_ab_to_ahb(ab);

			if (ab_ahb->crash_type == ATH12K_RPROC_ROOTPD_CRASH)
				break;
			fallthrough;
		case ATH12K_BUS_PCI:
			ath12k_hif_irq_disable(ab);
			break;
		}
	}
}

void ath12k_recovery_skip_partner_dump_collection(struct ath12k_base *ab)
{
	int i;
	struct ath12k_base *partner_ab;

	for (i = 0; i < ab->ag->num_devices; i++) {
		partner_ab = ab->ag->ab[i];
		if (ab == partner_ab)
			continue;
		partner_ab->recovery_skip_dump = true;
	}
}
EXPORT_SYMBOL(ath12k_recovery_skip_partner_dump_collection);

/* Mode3 (client-retaining) recovery is only supported when the asserted
 * radio serves AP VAPs.  In repeater/mesh deployments a STA VAP may be present
 * on the asserted radio; that flow is not covered by the Mode3 link
 * remove/readd path, so fall back to Mode2 (full MLO peer disconnect) if any
 * STA VAP exists on the asserted chip.  Called from the reset work before the
 * asserted chip powers down, so its arvif lists are still valid.
 */
static bool ath12k_mode3_asserted_has_sta_vap(struct ath12k_base *asserted_ab)
{
	struct ath12k_link_vif *arvif;
	struct ath12k *ar;
	int i;

	for (i = 0; i < asserted_ab->num_radios; i++) {
		ar = asserted_ab->pdevs[i].ar;
		if (!ar)
			continue;

		spin_lock_bh(&ar->data_lock);
		list_for_each_entry(arvif, &ar->arvifs, list) {
			if (arvif->ahvif &&
			    arvif->ahvif->vdev_type == WMI_VDEV_TYPE_STA) {
				spin_unlock_bh(&ar->data_lock);
				return true;
			}
		}
		spin_unlock_bh(&ar->data_lock);
	}

	return false;
}

static void ath12k_update_recovery_mode(struct ath12k_hw_group *ag,
					struct ath12k_base *asserted_ab)
{
	if (asserted_ab->recovery_mode_address) {
		/*get current recovery mode as per FW from shmem*/
		switch (*asserted_ab->recovery_mode_address) {
		case ATH12K_MLO_RECOVERY_MODE3:
			if (ath12k_mode3_asserted_has_sta_vap(asserted_ab)) {
				/* STA VAP present on the asserted radio
				 * (repeater/mesh): Mode3 link retain/readd is
				 * not supported, fall back to Mode2 full MLO
				 * peer disconnect.
				 */
				ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE2;
				ath12k_info(asserted_ab,
					    "Mode3 not supported with STA VAP on asserted radio, falling back to Mode2\n");
				break;
			}
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE3;
			ag->assert_ab     = asserted_ab;
			ath12k_info(asserted_ab,
				    "Mode3: client-retaining recovery selected\n");
			break;
		case ATH12K_MLO_RECOVERY_MODE2:
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE2;
			break;
		case ATH12K_MLO_RECOVERY_MODE1:
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE1;
			break;
		case ATH12K_MLO_RECOVERY_MODE0:
			fallthrough;
		default:
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
		}
	} else {
		ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
	}

	if (ath12k_check_erp_power_down(ag))
		ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;

	/* Fallback to mode0 recovery during a cumac HW crash when Mode-2 is selected */
	if (asserted_ab->is_cumac_chip &&
	    (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2 ||
	     ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3) &&
	    !test_bit(ATH12K_FLAG_RECOVERY_Q6_BCR, &asserted_ab->dev_flags)) {
		ath12k_info(asserted_ab, "Recovery is falling back to Mode0 due to cumac HW assert\n");
		ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
		ath12k_recovery_skip_partner_dump_collection(asserted_ab);
	}
}

static void ath12k_core_reset(struct work_struct *work)
{
	struct ath12k_base *partner_ab, *ab = container_of(work, struct ath12k_base,
							   reset_work);
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_hw *ah;
	int reset_count, fail_cont_count, i;
	long time_left;

	if (ag->wsi_remap_in_progress) {
		ath12k_info(ab, "WSI bypass in progress, can't handle SSR. Reset the system");
		BUG_ON(1);
	}

	if (!(test_bit(ATH12K_FLAG_QMI_FW_READY_COMPLETE, &ab->dev_flags)) &&
	    !ath12k_waltest_mode) {
		mutex_lock(&ag->mutex);
		/* The coredump may not be valid or complete since firmware
		 * initialization is incomplete.
		 */
		if (ab->hif.bus == ATH12K_BUS_PCI)
			ath12k_coredump_download_rddm(ab);
		else if (ath12k_fw_q6_dump_collection)
			ath12k_core_upd_power_down(ab);
		mutex_unlock(&ag->mutex);
		ath12k_warn(ab, "ignore reset dev flags 0x%lx\n", ab->dev_flags);
		return;
	}

	ab->recovery_start = false;

	ath12k_update_recovery_mode(ag, ab);

	if (ab->fw_recovery_support)
		ath12k_info(ab, "Recovery is initiated with Mode%d\n",
				ag->recovery_mode - 1);

	/* Send vendor event to notify userspace about assert has occurred */
#ifdef CPTCFG_QCN_EXTN
	ath12k_vendor_send_event(ab,
				 QCA_NL80211_VENDOR_FW_RECOVERY_EVENT_FW_ASSERT);
#endif /* CPTCFG_QCN_EXTN */

	/* Sometimes the recovery will fail and then the next all recovery fail,
	 * this is to avoid infinite recovery since it can not recovery success
	 */
	fail_cont_count = atomic_read(&ab->fail_cont_count);

	if (fail_cont_count >= ATH12K_RESET_MAX_FAIL_COUNT_FINAL) {
		ath12k_warn(ab, "Recovery Failed, Fail count:%d MAX_FAIL_COUNT final:%d\n",
				fail_cont_count,
				ATH12K_RESET_MAX_FAIL_COUNT_FINAL);
		return;
	}

	if (fail_cont_count >= ATH12K_RESET_MAX_FAIL_COUNT_FIRST &&
	    time_before(jiffies, ab->reset_fail_timeout)) {
		ath12k_warn(ab, "Recovery Failed, Fail count:%d MAX_FAIL_COUNT first:%d\n",
				fail_cont_count,
				ATH12K_RESET_MAX_FAIL_COUNT_FIRST);
		return;
	}

	reset_count = atomic_inc_return(&ab->reset_count);

	if (reset_count > 1) {
		/* Sometimes it happened another reset worker before the previous one
		 * completed, then the second reset worker will destroy the previous one,
		 * thus below is to avoid that.
		 */
		ath12k_warn(ab, "already resetting count %d\n", reset_count);

		/* Need to collect the UserPD dumps during the subsequent FW crash since
		 * crashed UserPD will get cleared during BUG_ON
		 */
		if (ab->hif.bus != ATH12K_BUS_PCI) {
			ath12k_info(ab, "Collecting the userpd dumps before full crash\n");
			if (!test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
				ath12k_core_upd_power_down(ab);
		}

		if (ath12k_ssr_failsafe_mode && ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
			BUG_ON(1);

		reinit_completion(&ab->reset_complete);
		time_left = wait_for_completion_timeout(&ab->reset_complete,
							ATH12K_RESET_TIMEOUT_HZ);
		if (time_left) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT, "to skip reset\n");
			atomic_dec(&ab->reset_count);
			return;
		}

		ab->reset_fail_timeout = jiffies + ATH12K_RESET_FAIL_TIMEOUT_HZ;
		/* Record the continuous recovery fail count when recovery failed*/
		fail_cont_count = atomic_inc_return(&ab->fail_cont_count);
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset starting\n");

	mutex_lock(&ag->mutex);

#if defined CPTCFG_ATHDEBUG && defined(CONFIG_QCA_MINIDUMP) &&\
!defined(CONFIG_ATH12K_MEM_PROFILE_256M)
	if (ab) {
		ath12k_info(ab, "%s : collect minidump\n", __func__);
		athdbg_if_get_service(ab, ATHDBG_SRV_COLLECT_MINIDUMP_REFERENCES);
		if (ab->fw_recovery_support)
			athdbg_if_get_service(ab, ATHDBG_SRV_DO_MINIDUMP);
	}
#endif
	ath12k_hif_mgmt_irq_disable(ab);
	ath12k_core_disable_ext_irq_during_recovery(ab);
	ath12k_hif_ce_irq_disable(ab);
	/* Mode 1 (UMAC reset) disables PPEDS IRQs via ppeds_interrupt_stop()
	 * in the pre_reset handler. Only disable them explicitly for Mode 0,
	 * which skips that path entirely.
	 */
	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0)
		ath12k_hif_ppe_irq_disable(ab);
	else if (!ag->cumac_enabled)
		ath12k_hif_ppe_irq_stop(ab);

	ab->is_reset = true;

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0)
		ath12k_core_mlo_hw_queues_stop(ab->ag);

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (partner_ab->is_bypassed || ab == partner_ab)
			continue;

		/* Need to check partner_ab flag to select recovery mode
		 * as Mode0, if continuous reset has happened
		 */

		if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0) {
			if (test_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags) ||
			    test_bit(ATH12K_FLAG_RECOVERY, &partner_ab->dev_flags)) {
				/* On receiving MHI Interrupt for pdev which is
				 * already in UMAC Recovery, then fallback to
				 * MODE0
				 */
				ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
				ath12k_info(ab, "Recovery is falling back to Mode0 as one of the partner chip is already in recovery\n");
				break;
			} else {
				/* Set dev flags to UMAC recovery START
				 * and set flag to send teardown later
				 */
				ath12k_info(ab, "setting teardown to true\n");
				set_bit(ATH12K_FLAG_UMAC_RECOVERY_START, &partner_ab->dev_flags);
			}
		}
	}

	/* UMAC RESET relies on ag->num_started as barrier to make sure
	 * umac related interrupts are received from all non-asserted chips
	 * before writing to the shared memory. So need to decrement it
	 * when recovery is enabled before triggering umac reset.
	 */
	if (ab->fw_recovery_support)
		ath12k_core_to_group_ref_put(ab);

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0) {
		if (ath12k_core_trigger_umac_reset(ab, WMI_MLO_TEARDOWN_SSR_REASON)) {
			/* Fallback to Mode0 if umac reset/peer_cleanup is
			 * failed
			 */
			ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
			/* TODO: DS: Handle any clean up necessary for Mode1 SSR */
			ath12k_info(ab, "Recovery is falling back to Mode0\n");
		} else {
			/* wake queues here as ping should continue for
			 * legacy clients in non-asserted chipsets
			 */
			if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE3) {
				ath12k_core_peer_disassoc(ag, ab);
			} else {
				if (ath12k_core_send_recovery_t2lm(ag) ||
				    ath12k_core_mlo_unified_peer_update(ag)) {
					ath12k_warn(ab,
						    "Mode3: T2LM and Peer Reconfiguration failed, falling to Mode0\n");
					ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
					ag->assert_ab     = NULL;
				} else {
					ath12k_info(ag->assert_ab,
						    "Mode3 Asserted, MLO Client-retaining recovery - Steering T2LM/Link removal Success\n");
				}
			}
			for (i = 0; i < ag->num_hw; i++) {
				ah = ag->ah[i];
				if (!ah)
					continue;

				ieee80211_wake_queues(ah->hw);
				ah->queue_stop = false;
			}
			ath12k_dbg(ab, ATH12K_DBG_MODE1_RECOVERY,
					"Queues are started as umac reset is completed for partner chipset\n");
		}
	}

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0 &&
	    !ath12k_check_erp_power_down(ag))
		ath12k_core_trigger_partner_device_crash(ab);

	atomic_set(&ab->recovery_count, 0);

	ath12k_coredump_collect(ab);

	ath12k_core_pre_reconfigure_recovery(ab);

	ath12k_core_post_reconfigure_recovery(ab);

	/* prepare coredump */
	if (ab->hif.bus == ATH12K_BUS_PCI) {
		ath12k_coredump_download_rddm(ab);
	} else if ((ab->hif.bus == ATH12K_BUS_AHB || ab->hif.bus == ATH12K_BUS_HYBRID) &&
		   !ab->fw_recovery_support && !ath12k_upload_dump_before_bugon) {
		ath12k_core_trigger_bug_on(ab);
	}

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "waiting recovery start...\n");

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2 ||
	    ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3)
		ath12k_partner_chip_power_state_info(ag, FW_ASSERTED_CHIP_PWR_DOWN);

	if (ab->fw_recovery_support || ath12k_upload_dump_before_bugon) {
		if (ab->hif.bus == ATH12K_BUS_PCI) {
			ath12k_hif_power_down(ab, false);
		} else {
			if (!test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
				ath12k_core_upd_power_down(ab);
		}
	}

	if (!ab->fw_recovery_support && ath12k_upload_dump_before_bugon) {
		ath12k_core_trigger_bug_on(ab);
		mutex_unlock(&ag->mutex);
		return;
	}

	/* prepare for power up */
	ab->qmi.num_radios = U8_MAX;
	//ab->single_chip_mlo_supp = false; TODO need to revisit

	if (ag->recovery_mode != ATH12K_MLO_RECOVERY_MODE0)
		ab->recovery_start = true;

	ab->recovery_mode_address = NULL;
	ab->crash_info_address = NULL;

	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE0 && ag->num_started > 0) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
			   "waiting for %d partner device(s) to reset\n",
			   ag->num_started);
		mutex_unlock(&ag->mutex);
		return;
	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->is_bypassed ||
		    !test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
			continue;
		/* Skip recovery incase during reboot */
		if (system_state == SYSTEM_RESTART) {
			mutex_unlock(&ag->mutex);
			return;
		}

		ath12k_qmi_free_resource(ab);
		ath12k_hif_power_up(ab);

		if (ath12k_check_erp_power_down(ag))
			ab->powerup_triggered = true;
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "reset started\n");
	}

	/*As a foolproof clear ag->block_peer_create unconditionally for Mode-0*/
	if (ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE2 ||
	    ag->recovery_mode == ATH12K_MLO_RECOVERY_MODE3)
		ath12k_partner_chip_power_state_info(ag,
						     FW_ASSERTED_CHIP_PWR_UP);
	else
		ag->block_peer_create = false;

	mutex_unlock(&ag->mutex);
}

static int ath12k_core_panic_handler(struct notifier_block *nb,
				     unsigned long action, void *data)
{
	struct ath12k_base *ab = container_of(nb, struct ath12k_base,
					      panic_nb);

	if (!ab)
		return 0;
#ifdef CPTCFG_ATHDEBUG
	athdbg_if_get_service(ab, ATHDBG_SRV_COLLECT_MINIDUMP_REFERENCES);
#endif
	if (ab->in_panic)
		goto panic_handler;

	ab->in_panic = true;

	if (ab->hif.bus == ATH12K_BUS_PCI)
		ath12k_coredump_download_rddm(ab);
	else
		atomic_inc(&ath12k_coredump_ram_info.num_chip);

panic_handler:
	return ath12k_hif_panic_handler(ab);
}

static int ath12k_core_panic_notifier_register(struct ath12k_base *ab)
{
	ab->panic_nb.notifier_call = ath12k_core_panic_handler;

	return atomic_notifier_chain_register(&panic_notifier_list,
					      &ab->panic_nb);
}

void ath12k_core_panic_notifier_unregister(struct ath12k_base *ab)
{
	atomic_notifier_chain_unregister(&panic_notifier_list,
					 &ab->panic_nb);
}

static inline
bool ath12k_core_hw_group_create_ready(struct ath12k_hw_group *ag)
{
	lockdep_assert_held(&ag->mutex);

	return (ag->num_probed == ag->num_devices);
}

static struct ath12k_hw_group *ath12k_core_hw_group_alloc(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag;
	int i, count = 0;

	lockdep_assert_held(&ath12k_hw_group_mutex);

	list_for_each_entry(ag, &ath12k_hw_group_list, list)
		count++;

	ag = kzalloc(sizeof(*ag), GFP_KERNEL);
	if (!ag)
		return NULL;

	ag->dp_hw_grp = ath12k_core_dp_hw_group_alloc(ab->dp);
	if (!ag->dp_hw_grp) {
		kfree(ag);
		return NULL;
	}

	ag->dp_hw_grp->tx_desc_used_cnt = alloc_percpu(u32);
	if (!ag->dp_hw_grp->tx_desc_used_cnt) {
		kfree(ag->dp_hw_grp);
		kfree(ag);
		return NULL;
	}

	ag->id = count;
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ag->dbs_power_reduction = ATH12K_DEFAULT_POWER_REDUCTION;
	ag->eth_power_reduction = ATH12K_DEFAULT_POWER_REDUCTION;
#endif
	list_add(&ag->list, &ath12k_hw_group_list);
	INIT_WORK(&ag->reset_group_work, ath12k_core_update_userpd_state);
	mutex_init(&ag->mutex);
	mutex_init(&ag->dp_hw_grp->tx_init_lock);
#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	mutex_init(&ag->dp_hw_grp->ppeds_tx_init_lock);
#endif
	init_completion(&ag->umac_reset_complete);
	init_completion(&ag->peer_cleanup_complete);
	ag->mlo_peer_count = 0;
	spin_lock_init(&ag->ahsta_lock);
	ag->mlo_capable = false;
	ag->recovery_mode = ATH12K_MLO_RECOVERY_MODE0;
	ag->wsi_load_info = NULL;
	ag->wsi_peer_clean_timeout = ATH12K_MAC_PEER_CLEANUP_TIMEOUT_MSECS;

	/* Initialize UMAC reset synchronization counters */
	atomic_set(&ag->mlo_umac_reset.request_chip, 0);
	ag->mlo_umac_reset.task_map = 0;

	/* Initialize multi-core task queue infrastructure */
	INIT_LIST_HEAD(&ag->mlo_umac_reset.task_queue);
	spin_lock_init(&ag->mlo_umac_reset.task_queue_lock);
	spin_lock_init(&ag->mlo_umac_reset.lock);
	atomic_set(&ag->mlo_umac_reset.task_id, 0);

	/* Initialize high-priority tasklet for each possible CPU */
	for (i = 0; i < num_possible_cpus(); i++) {
		tasklet_setup(&ag->mlo_umac_reset.tasklet[i],
			      ath12k_umac_reset_tasklet_handler_percpu);
	}
#ifdef CPTCFG_ATH12K_POWER_OPTIMIZATION
	ath12k_global_ps_ctx.ag = ag;
#endif
	init_completion(&ag->power_up);

	ag->cumac_enabled = ab->hw_params->cumac_support;

	return ag;
}

static void ath12k_core_free_wsi_info(struct ath12k_hw_group *ag)
{
	int i;

	for (i = 0; i < ag->num_devices; i++) {
		of_node_put(ag->wsi_node[i]);
		ag->wsi_node[i] = NULL;
	}
	ag->num_devices = 0;
}

static void ath12k_core_hw_group_free(struct ath12k_hw_group *ag)
{
	mutex_lock(&ath12k_hw_group_mutex);

	ath12k_core_free_wsi_info(ag);
	list_del(&ag->list);
	spin_lock_bh(&ag->ahsta_lock);
	ath12k_sta_hlist_destroy(ag);
	ath12k_sta_hlist_head_destroy(ag);
	spin_unlock_bh(&ag->ahsta_lock);
	free_percpu(ag->dp_hw_grp->tx_desc_used_cnt);
	kfree(ag->dp_hw_grp);
	kfree(ag);

	mutex_unlock(&ath12k_hw_group_mutex);
}

static struct ath12k_hw_group *ath12k_core_hw_group_find_by_dt(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag;
	int i;

	if (!ab->dev->of_node)
		return NULL;

	list_for_each_entry(ag, &ath12k_hw_group_list, list)
		for (i = 0; i < ag->num_devices; i++)
			if (ag->wsi_node[i] == ab->dev->of_node)
				return ag;

	return NULL;
}

static bool
ath12k_core_check_is_bypassed(struct ath12k_hw_group *ag, struct device_node *connected_dev) {
	int i;
	struct ath12k_base *ab;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];

		if (ab->dev->of_node == connected_dev && ab->is_bypassed)
			return true;
	}

	return false;
}

/*
@reg - use reg as 0 to get the right side neighbour (Connected to TX port),
and 1 to get left side neighbour (Connected to RX port).
*/

static struct device_node *
ath12k_get_connected_dev(struct ath12k_base *ab, int reg,
			 bool include_bypassed_device) {

	struct device_node *endpoint, *next_endpoint, *connected_dev;
	struct device_node *ab_dev = ab->dev->of_node;

	connected_dev = ab_dev;

	do {
		endpoint = of_graph_get_endpoint_by_regs(connected_dev, reg, -1);

		if (!endpoint)
			return NULL;

		next_endpoint = of_graph_get_remote_endpoint(endpoint);
		if (!next_endpoint) {
			of_node_put(next_endpoint);
			return NULL;
		}

		of_node_put(endpoint);

		connected_dev = of_graph_get_port_parent(next_endpoint);

		if (!connected_dev) {
			of_node_put(next_endpoint);
			return NULL;
		}

		of_node_put(next_endpoint);

		if (!of_device_is_available(connected_dev)) {
			ath12k_warn(ab, "Connected dev : %pOFP is not active\n",
				    connected_dev);
			of_node_put(connected_dev);
			return NULL;
		}

		if (!include_bypassed_device ||
		    !ath12k_core_check_is_bypassed(ab->ag, connected_dev))
			return connected_dev;

		of_node_put(connected_dev);

	} while (connected_dev != ab_dev);

	return NULL;
}

static int ath12k_core_get_wsi_info(struct ath12k_hw_group *ag,
				    struct ath12k_base *ab)
{
	struct device_node *next_wsi_dev;
	int device_count = 0, ret = 0;
	struct device_node *wsi_dev;

	wsi_dev = of_node_get(ab->dev->of_node);
	if (!wsi_dev)
		return -ENODEV;

	do {
		if (device_count >= ATH12K_MAX_SOCS) {
			ath12k_warn(ab, "device count in DT %d has reached or exceeded the limit %d\n",
				    device_count, ATH12K_MAX_SOCS);
			ret = -EINVAL;
			break;
		}

		ag->wsi_node[device_count++] = of_node_get(wsi_dev);

		struct device_node *tx_endpoint __free(device_node) =
					of_graph_get_endpoint_by_regs(wsi_dev, 0, -1);
		if (!tx_endpoint) {
			ret = -ENODEV;
			break;
		}

		struct device_node *next_rx_endpoint __free(device_node) =
					of_graph_get_remote_endpoint(tx_endpoint);
		if (!next_rx_endpoint) {
			ret = -ENODEV;
			break;
		}

		next_wsi_dev = of_graph_get_port_parent(next_rx_endpoint);
		if (!next_wsi_dev) {
			ret = -ENODEV;
			break;
		}

		if (!of_device_is_available(next_wsi_dev)) {
			ath12k_warn(ab, "wsi peer node %pOFP is not available\n",
				   next_wsi_dev);
			ath12k_warn(ab, "Not able to form wsi group\n");
			ret = -ENODEV;
			break;
		}

		of_node_put(wsi_dev);
		wsi_dev = next_wsi_dev;
	} while (ab->dev->of_node != wsi_dev);

	if (ret) {
		while (--device_count >= 0) {
			of_node_put(ag->wsi_node[device_count]);
			ag->wsi_node[device_count] = NULL;
		}

		of_node_put(wsi_dev);
		return ret;
	}

	of_node_put(wsi_dev);
	ag->num_devices = device_count;

	return 0;
}

/* If one of the device is bypassed, use bypass_wsi_info */
struct ath12k_wsi_info
*ath12k_core_get_current_wsi_info(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (ag->num_bypassed > 0)
		return &ab->bypass_wsi_info;
	else
		return &ab->wsi_info;
}

static void ath12k_core_fill_adj_info(struct ath12k_base *ab)
{
	struct device_node* ab_dev = ab->dev->of_node;
	struct device_node *tx_neighbour, *rx_neighbour;
	struct ath12k_hw_group *ag = ab->ag;
	int num_adj_chips = 0, i = 0;
	u8 adj_device_idx_bmp = 0;
	struct ath12k_wsi_info *wsi_info, *adj_wsi_info;
	struct ath12k_base *partner_ab;
	bool is_bypassed = ag->num_bypassed ? true : false;

	wsi_info = ath12k_core_get_current_wsi_info(ab);

	tx_neighbour = ath12k_get_connected_dev(ab, 0, is_bypassed);

	if (!tx_neighbour) {
		of_node_put(ab_dev);
		return;
	}

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		adj_wsi_info = ath12k_core_get_current_wsi_info(partner_ab);
		if (tx_neighbour == partner_ab->dev->of_node) {
			wsi_info->adj_chip_idxs[num_adj_chips] = adj_wsi_info->index;
			adj_device_idx_bmp |= BIT(wsi_info->adj_chip_idxs[num_adj_chips]);
			break;
		}
	}

	num_adj_chips++;

	rx_neighbour = ath12k_get_connected_dev(ab, 1, is_bypassed);
	if (!rx_neighbour) {
		of_node_put(ab_dev);
		return;
	}

	if (tx_neighbour != rx_neighbour) {

		for (i = 0; i < ag->num_devices; i++) {
			partner_ab = ag->ab[i];
			adj_wsi_info = ath12k_core_get_current_wsi_info(partner_ab);
			if (rx_neighbour == partner_ab->dev->of_node) {
				wsi_info->adj_chip_idxs[num_adj_chips] = adj_wsi_info->index;
				adj_device_idx_bmp |= BIT(wsi_info->adj_chip_idxs[num_adj_chips]);
				break;
			}
		}
		num_adj_chips++;
	}

	ath12k_dbg(ab, ATH12K_DBG_QMI, "Adj chip info:\n");
	ath12k_dbg(ab, ATH12K_DBG_QMI, "num_adj_chips: %d\n", num_adj_chips);
	for (i = 0; i < num_adj_chips; i++)
		ath12k_dbg(ab, ATH12K_DBG_QMI, "[%d] : %d", i, ab->wsi_info.adj_chip_idxs[i]);

	of_node_put(tx_neighbour);
	of_node_put(rx_neighbour);
	of_node_put(ab_dev);

	wsi_info->num_adj_chips = num_adj_chips;
	for (i = 0; i < ag->num_devices && adj_device_idx_bmp; i++) {
		if ((adj_device_idx_bmp & BIT(i)) ||
		    (i == wsi_info->index))
			continue;
		wsi_info->diag_device_idx_bmap |= BIT(i);
 	}
}

static int ath12k_core_get_wsi_index(struct ath12k_hw_group *ag,
				     struct ath12k_base *ab)
{
	int i, wsi_controller_index = -1, node_index = -1;
	bool control;

	for (i = 0; i < ag->num_devices; i++) {
		control = of_property_read_bool(ag->wsi_node[i], "qcom,wsi-controller");
		if (control)
			wsi_controller_index = i;

		if (ag->wsi_node[i] == ab->dev->of_node)
			node_index = i;
	}

	if (wsi_controller_index == -1) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "wsi controller is not defined in dt");
		return -EINVAL;
	}

	if (node_index == -1) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "unable to get WSI node index");
		return -EINVAL;
	}

	ab->wsi_info.index = (ag->num_devices + node_index - wsi_controller_index) %
		ag->num_devices;
	ab->bypass_wsi_info.index = (ag->num_devices + node_index - wsi_controller_index) %
		ag->num_devices;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "WSI index: %d, num_devices: %d\n",
		   ab->wsi_info.index, ag->num_devices);
	return 0;
}

static int validate_probe_order_param(unsigned int probe_order, u8 num_devices,
					struct ath12k_base *ab)
{
	unsigned int temp_val = probe_order;
	int i, max_soc = 0;
	u8 seen_ids = 0;

	while (temp_val) {
		temp_val = temp_val >> ATH12K_PROBE_ORDER_SHIFT;
		max_soc++;
	}
	temp_val = probe_order;
	if (num_devices !=  max_soc) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT,
				"Invalid module param\n");
		return -1;
	}
	for (i = 0; i < num_devices; i++) {
		unsigned int temp1_val = (temp_val >> (i *
						ATH12K_PROBE_ORDER_SHIFT));
		u8 device_id = temp1_val & ATH12K_PROBE_ORDER_MASK;
		if ((device_id  == 0) || (device_id > num_devices)) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				"ath12k: invalid device ID %u at position %d in probe_order\n",
					device_id, i);
			return -1;
		}
		if (seen_ids & BIT(device_id)) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				"ath12k: duplicate device ID %u in probe_order\n",
					 device_id);
			return -1;
		}
		seen_ids |= BIT(device_id);

	}
	return 0;
}

static struct ath12k_hw_group *ath12k_core_hw_group_assign(struct ath12k_base *ab)
{
	struct ath12k_wsi_info *wsi = &ab->wsi_info;
	struct ath12k_hw_group *ag;
	bool mlo_capable;

	lockdep_assert_held(&ath12k_hw_group_mutex);

	if (!ab->hw_params || !ab->hw_params->board_magic)
		goto invalid_group;

	mlo_capable = !strncmp(ab->hw_params->board_magic,
				   ATH12K_SCAN_RADIO,
				   strlen(ATH12K_SCAN_RADIO));

	if (ath12k_ftm_mode || !ath12k_mlo_capable ||
	    ath12k_waltest_mode || mlo_capable)
		goto invalid_group;

	/* The grouping of multiple devices will be done based on device tree file.
	 * The platforms that do not have any valid group information would have
	 * each device to be part of its own invalid group.
	 *
	 * We use group id ATH12K_INVALID_GROUP_ID for single device group
	 * which didn't have dt entry or wrong dt entry, there could be many
	 * groups with same group id, i.e ATH12K_INVALID_GROUP_ID. So
	 * default group id of ATH12K_INVALID_GROUP_ID combined with
	 * num devices in ath12k_hw_group determines if the group is
	 * multi device or single device group
	 */

	ag = ath12k_core_hw_group_find_by_dt(ab);
	if (!ag) {
		ag = ath12k_core_hw_group_alloc(ab);
		if (!ag) {
			ath12k_warn(ab, "unable to create new hw group\n");
			return NULL;
		}

		if (ath12k_core_get_wsi_info(ag, ab) ||
		    ath12k_core_get_wsi_index(ag, ab)) {
			ath12k_dbg(ab, ATH12K_DBG_BOOT,
				   "unable to get wsi info from dt, grouping single device");
			ath12k_core_free_wsi_info(ag);
			ag->id = ATH12K_INVALID_GROUP_ID;
			ag->num_devices = 1;
			wsi->index = 0;
		}

		goto exit;
	} else if (test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags)) {
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "group id %d in unregister state\n",
			   ag->id);
		goto invalid_group;
	} else {
		if (ath12k_core_get_wsi_index(ag, ab))
			goto invalid_group;
		goto exit;
	}

invalid_group:
	ag = ath12k_core_hw_group_alloc(ab);
	if (!ag) {
		ath12k_warn(ab, "unable to create new hw group\n");
		return NULL;
	}

	ag->id = ATH12K_INVALID_GROUP_ID;
	ag->num_devices = 1;
	wsi->index = 0;

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "single device added to hardware group\n");

exit:
	/* All ABs in an HW group must have identical CUMAC support (all or none).
	 * The first AB’s capability is propagated to the AG during hw_group_alloc().
	 * Any subsequent AB must match this; otherwise it is rejected and brought down.
	 *
	 * When adding a second (or later) AB, on failure, returns NULL and the ab is
	 * freed. the AG is freed as part of tearing down the remaining ABs.
	 */
	if (ab->hw_params->cumac_support != ag->cumac_enabled) {
		ath12k_warn(ab, "HW group contains ABs with inconsistent CUMAC support\n");
		return NULL;
	}

	if (ag->num_probed >= ag->num_devices) {
		ath12k_warn(ab, "unable to add new device to group, max limit reached\n");
		goto invalid_group;
	}

	ab->device_id = ag->num_probed++;
	if (ath12k_probe_order_param) {
		if (!ath12k_valid_probe_order) {
			if (validate_probe_order_param(
						ath12k_probe_order_param,
						ag->num_devices, ab) == 0)
				ath12k_valid_probe_order = 1;
			else
				ath12k_probe_order_param = 0;
		}

		if (ath12k_valid_probe_order) {
			ab->device_id = (ath12k_probe_order_param &
					ATH12K_PROBE_ORDER_MASK) - 1;
			ath12k_probe_order_param >>= ATH12K_PROBE_ORDER_SHIFT;
		}
	}
	if (ag->id != ATH12K_INVALID_GROUP_ID)
		ab->is_static_bypassed = (ath12k_wsi_bypass_bmap & (1 << wsi->index));
	else if (ath12k_wsi_bypass_bmap)
		ath12k_warn(ab, "single device does not support static WSI bypass\n");

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "chip_id: %d\n", ab->device_id);
	ag->ab[ab->device_id] = ab;
	ab->ag = ag;

	ath12k_dp_cmn_hw_group_assign(ath12k_ab_to_dp(ab), ag);

	if (ath12k_wsi_load_info_init(ab))
		ath12k_err(ab, "failed to initialize wsi load info\n");

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "wsi group-id %d num-devices %d index %d",
		   ag->id, ag->num_devices, wsi->index);

	/* stats context Initialization */
	wiphy_work_init(&ag->stats_work.stats_nb_work,
			ath12k_stats_event_work_handler);
	spin_lock_init(&ag->stats_work.list_lock);
	INIT_LIST_HEAD(&ag->stats_work.work_list);

	return ag;
}

void ath12k_update_mlo_adj_chip(struct ath12k_hw_group *ag)
{
	int i;
	struct ath12k_base *partner_ab;

	for (i = 0; i < ag->num_devices; i++) {
		partner_ab = ag->ab[i];
		if (!partner_ab || partner_ab->is_bypassed)
			continue;

		mutex_lock(&partner_ab->core_lock);
		ath12k_core_fill_adj_info(partner_ab);

		mutex_unlock(&partner_ab->core_lock);
	}
}
EXPORT_SYMBOL(ath12k_update_mlo_adj_chip);

void ath12k_core_pci_link_speed(struct ath12k_base *ab, u16 link_speed,
				u16 link_width)
{
	struct ath12k_pci *ab_pci;
	struct pci_dev *root_port;

	if (ab->hif.bus != ATH12K_BUS_PCI)
		return;

	ab_pci = ath12k_pci_priv(ab);
	if (!ab_pci) {
		ath12k_err(ab, "Error in getting the pci_priv\n");
		return;
	}

	if (link_speed > ab_pci->def_link_speed)
		link_speed = ab_pci->def_link_speed;

	if (link_width > ab_pci->def_link_width)
		link_width = ab_pci->def_link_width;

	root_port = pcie_find_root_port(ab_pci->pdev);

	if (!root_port) {
		ath12k_err(ab, "Error in getting the root port\n");
		return;
	}

	ath12k_info(ab, "Link speed is %d and width is %d\n", link_speed, link_width);

#ifndef PLATFORM_SDX
	if (pcie_set_link_speed(root_port, link_speed))
		ath12k_err(ab, "Failed to set the link speed\n");

	if (pcie_set_link_width(root_port, link_width))
		ath12k_err(ab, "Failed to set the link width\n");
#endif
}
EXPORT_SYMBOL(ath12k_core_pci_link_speed);

int ath12k_core_wsi_remap_pdev_suspend(struct ath12k_base *ab)
{
	int i, ret = 0;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		ar = ab->pdevs[i].ar;
		if (!ar) {
			ath12k_err(ab, "Invalid Radio\n");
			return -EINVAL;
		}
		ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: PDEV suspend");
		if (ab->is_cumac_chip)
			ath12k_mac_stop(ar);
		else
			ret = ath12k_mac_pdev_suspend(ar);
		if (ret) {
			ath12k_err(ab, "PDEV suspend failed for wsi remap: %d\n", ret);
			goto fail;
		}
	}

fail:
	return ret;
}
EXPORT_SYMBOL(ath12k_core_wsi_remap_pdev_suspend);

int ath12k_core_wsi_mlo_teardown_umac_reset(struct ath12k_base *ab)
{
	int ret = 0, i;
	struct ath12k_hw *ah;
	struct ath12k_hw_group *ag = ab->ag;

	if (ath12k_hw_group_recovery_in_progress(ag)) {
		ath12k_err(ab, "SSR is in progress, cannot allow remap\n");
		return -EBUSY;
	}

	ath12k_dbg(ab, ATH12K_DBG_WSI_BYPASS, "WSI Bypass: MLO teardown with Umac reset");
	ath12k_core_mlo_hw_queues_stop(ab->ag);

	ret = ath12k_core_trigger_umac_reset(ab,
					     WMI_MLO_TEARDOWN_REASON_DYNAMIC_WSI_REMAP);
	if (ret) {
		/* We should not come here. Something wrong with MLO teardown.
		 * Crash the system to recover.
		 */
		ath12k_err(ab, "WSI Bypass: MLO teardown/UMAC reset failed with error %d",
			   ret);
		ath12k_err(ab, "WSI Bypass: Abort Bypass procedure! Reset the system...");
		BUG_ON(1);
	} else {
		for (i = 0; i < ag->num_hw; i++) {
			ah = ag->ah[i];
			if (!ah)
				continue;

			ieee80211_wake_queues(ah->hw);
			ah->queue_stop = false;
		}
	}

	return ret;
}
EXPORT_SYMBOL(ath12k_core_wsi_mlo_teardown_umac_reset);

int ath12k_core_dynamic_wsi_remap(struct ath12k_base *ab)
{
	int ret = 0;

	ab->ag->wsi_remap_in_progress = true;

	/* Dispatch to the chip-family ops for the remove/add logic. */
	if (!ab->hw_params->cp_arch_ops) {
		ath12k_err(ab, "WSI Bypass: no wsi_bypass_ops registered for this chip\n");
		ret = -EOPNOTSUPP;
		goto fail;
	}

	if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_REMOVE_DEVICE) {
		if (ab->hw_params->cp_arch_ops->wsi_bypass_remove)
			ret = ab->hw_params->cp_arch_ops->wsi_bypass_remove(ab);
		if (ret)
			goto fail;
	} else if (ab->wsi_remap_state == ATH12K_WSI_BYPASS_ADD_DEVICE) {
		if (ab->hw_params->cp_arch_ops->wsi_bypass_add)
			ret = ab->hw_params->cp_arch_ops->wsi_bypass_add(ab);
		if (ret)
			goto fail;
	}

fail:
	return ret;
}

void ath12k_core_hw_group_unassign(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ath12k_ab_to_ag(ab);
	u8 device_id = ab->device_id;
	int num_probed;

	if (!ag)
		return;

	if (ag->wsi_load_info) {
		ath12k_wsi_load_info_deinit(ab, ag->wsi_load_info);
		ag->wsi_load_info = NULL;
	}

	mutex_lock(&ag->mutex);

	if (WARN_ON(device_id >= ag->num_devices)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	if (WARN_ON(ag->ab[device_id] != ab)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	ath12k_dp_cmn_hw_group_unassign(ath12k_ab_to_dp(ab), ag);

	ag->ab[device_id] = NULL;
	ab->ag = NULL;
	ab->device_id = ATH12K_INVALID_DEVICE_ID;

	if (ag->num_probed)
		ag->num_probed--;

	num_probed = ag->num_probed;

	mutex_unlock(&ag->mutex);

	if (!num_probed)
		ath12k_core_hw_group_free(ag);
}

static void ath12k_core_hw_group_destroy(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	if (WARN_ON(!ag))
		return;

	mutex_lock(&ag->mutex);
	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab)
			continue;

		mutex_lock(&ab->core_lock);
		ath12k_core_soc_destroy(ab);
		mutex_unlock(&ab->core_lock);
		ath12k_core_panic_notifier_unregister(ab);
	}
	mutex_unlock(&ag->mutex);
}

static void ath12k_core_multipd_wmi_pdev_set_param(struct ath12k_base *ab,
						    const unsigned int value)
{
	struct ath12k_pdev *pdev;
	struct ath12k *ar;
	bool assert_userpd;
	int i;

	if (ab->hif.bus == ATH12K_BUS_PCI)
		return;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = &ab->pdevs[i];
		ar = pdev->ar;

		/* Set pdev param to let firmware know which pd to use for
		 * sending fatal IRQ.
		 * Non-MLO, fatal error comes from asserted radios's user pd
		 * MLO, fatal error comes from asserted radio's root pd
		 */
		if (!ab->ag->mlo_capable) {
			assert_userpd = true;
		} else {
			if (value == ATH12K_FW_RECOVERY_DISABLE)
				assert_userpd = false;
			else
				assert_userpd = true;
		}

		ath12k_wmi_pdev_set_param(ar, WMI_PDEV_PARAM_MPD_USERPD_SSR,
					  assert_userpd, ar->pdev->pdev_id);
	}
}

void ath12k_core_send_fw_hang_cmd(struct ath12k_base *ab,
				  unsigned int value)
{
	struct ath12k_hw_group *ag = ab->ag;
	enum wmi_fw_hang_recovery_mode_type recovery_mode;
	int ret;
	int i;

	if (ath12k_hw_group_recovery_in_progress(ag)) {
		ath12k_err(ab, "Recovery is in progress, try again once it's done\n");
		return;
	}

	switch (value) {
	case ATH12K_FW_RECOVERY_ENABLE_MODE1:
		ath12k_info(ab, "Mode 1 Recovery is depricated, setting recovery as Mode 2\n");
		fallthrough;
	case ATH12K_FW_RECOVERY_ENABLE_MODE2:
#ifdef CPTCFG_QCN_EXTN
		if (!ath12k_cfg_get(ab, ATH12K_CFG_MLO_FORCE_MODE2_RECOVERY) &&
		    test_bit(WMI_SERVICE_MLO_MODE3_RECOVERY_SUPPORTED,
			     ab->wmi_ab.svc_map)) {
#else
		if (test_bit(WMI_SERVICE_MLO_MODE3_RECOVERY_SUPPORTED,
			     ab->wmi_ab.svc_map)) {
#endif /* CPTCFG_QCN_EXTN */
			recovery_mode = ATH12K_WMI_FW_HANG_RECOVERY_MODE3;
			ath12k_info(ab, "FW Supports Client retaining Mode 3 Recovery\n");
			break;
		} else if (test_bit(WMI_SERVICE_MLO_MODE2_RECOVERY_SUPPORTED,
				    ab->wmi_ab.svc_map)) {
#ifdef CPTCFG_QCN_EXTN
			if (ath12k_cfg_get(ab, ATH12K_CFG_MLO_FORCE_MODE2_RECOVERY))
				ath12k_info(ab, "mlo_force_mode2_recovery set, using Mode 2\n");
#endif /* CPTCFG_QCN_EXTN */
			recovery_mode = ATH12K_WMI_FW_HANG_RECOVERY_MODE2;
		} else {
			ath12k_info(ab, "FW doesn't support Mode 2 fallback to Mode 0\n");
			recovery_mode = ATH12K_WMI_FW_HANG_RECOVERY_MODE0;
		}
		break;
	case ATH12K_FW_RECOVERY_ENABLE_AUTO:
		recovery_mode = ATH12K_WMI_FW_HANG_RECOVERY_MODE0;
		break;
	default:
		recovery_mode = ATH12K_WMI_DISABLE_FW_RECOVERY;
		break;
	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (ab->is_bypassed)
			continue;
		mutex_lock(&ab->core_lock);
		ab->fw_recovery_support = value;
		mutex_unlock(&ab->core_lock);

		/*
		 * TODO: Instead of checking recovery mode addr from
		 * TLV, need to check WMI caps once the support is
		 * added from FW.
		 */
		if (ag->mlo_capable && !ab->recovery_mode_address)
			continue;

		if (test_bit(ATH12K_FLAG_Q6_POWER_DOWN, &ab->dev_flags))
			continue;

		ath12k_core_multipd_wmi_pdev_set_param(ab, value);

		ret = ath12k_wmi_force_fw_hang_cmd(ab->pdevs[0].ar, recovery_mode,
						   ATH12K_WMI_FW_HANG_DELAY, false);
		ath12k_info(ab, "setting FW assert mode [%d] ret [%d]\n", recovery_mode,
			    ret);
	}
}
EXPORT_SYMBOL(ath12k_core_send_fw_hang_cmd);

static void ath12k_core_hw_group_cleanup(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	if (!ag)
		return;

	mutex_lock(&ag->mutex);

	if (!test_bit(ATH12K_GROUP_FLAG_REGISTERED, &ag->flags)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	if (test_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags)) {
		mutex_unlock(&ag->mutex);
		return;
	}

	ath12k_core_send_fw_hang_cmd(ag->ab[0], ATH12K_FW_RECOVERY_DISABLE);

	set_bit(ATH12K_GROUP_FLAG_UNREGISTER, &ag->flags);

	ath12k_core_hw_group_stop(ag);

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		if (!test_bit(ATH12K_FLAG_RECOVERY, &ab->dev_flags) &&
		     test_bit(ATH12K_FLAG_CRASH_FLUSH, &ab->dev_flags)) {
			if (ab->hif.bus == ATH12K_BUS_PCI)
				ath12k_coredump_download_rddm(ab);
		}

	}

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		mutex_lock(&ab->core_lock);
		ath12k_core_stop(ab);
		mutex_unlock(&ab->core_lock);
	}

	mutex_unlock(&ag->mutex);
}

static int ath12k_core_hw_group_create(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	lockdep_assert_held(&ag->mutex);

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		mutex_lock(&ab->core_lock);

		if (test_bit(ATH12K_FLAG_SOC_CREATE_FAIL, &ab->dev_flags)) {
			mutex_unlock(&ab->core_lock);
			return -ENODEV;
		}
		ath12k_core_fill_adj_info(ab);

		mutex_unlock(&ab->core_lock);

	}
	return 0;
}

void ath12k_core_hw_group_set_mlo_capable(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab;
	int i;

	if (ath12k_ftm_mode || !ath12k_mlo_capable || ath12k_waltest_mode)
		return;

	lockdep_assert_held(&ag->mutex);

	/* If more than one devices are grouped, then inter MLO
	 * functionality can work still independent of whether internally
	 * each device supports single_chip_mlo or not.
	 * Only when there is one device, then disable for WCN chipsets
	 * till the required driver implementation is in place.
	 */
	if (ag->num_devices == 1) {
		bool mlo_capable;

		ab = ag->ab[0];

		mlo_capable = !strncmp(ab->hw_params->board_magic,
					   ATH12K_SCAN_RADIO,
					   strlen(ATH12K_SCAN_RADIO));

		if (mlo_capable)
			return;

		/* WCN chipsets does not advertise in firmware features
		 * hence skip checking
		 */
		if (ab->hw_params->def_num_link)
			return;
	}

	ag->mlo_capable = true;

	for (i = 0; i < ag->num_devices; i++) {
		ab = ag->ab[i];
		if (!ab || ab->is_bypassed)
			continue;

		/* even if 1 device's firmware feature indicates MLO
		 * unsupported, make MLO unsupported for the whole group
		 */

		 /* TODO: As AHB, HYBRID chipsets dosen't support firmware2.bin
		  * bypassing this check for the both. Need to make this generic
		  */

		if (ab->fw.api_version == ATH12K_FW_API_V2 && !test_bit(ATH12K_FW_FEATURE_MLO, ab->fw.fw_features)) {
			ag->mlo_capable = false;
			return;
		}
	}
}

int ath12k_wsi_load_info_init(struct ath12k_base *ab)
{
	struct ath12k_mlo_wsi_load_info *wsi_load_info = NULL;
	struct ath12k_mlo_wsi_device_group *mlo_grp_info;
	struct ath12k_hw_group *ag = ab->ag;
	int ret = 0;
	u8 i;

	if (!ag)
		return ret;

	if (ag->num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		return ret;

	if (!ag->wsi_load_info) {
		wsi_load_info = kzalloc(sizeof(*wsi_load_info), GFP_KERNEL);
		if (!wsi_load_info)
			return -ENOMEM;
		ag->wsi_load_info = wsi_load_info;
		mlo_grp_info = &wsi_load_info->mlo_device_grp;
		mlo_grp_info->num_devices = 0;
		for (i = 0; i < ATH12K_MAX_SOCS; i++)
			mlo_grp_info->wsi_order[i] = WSI_INVALID_ORDER;
		ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
				 "successfully initialized wsi load info\n");
	}
	return ret;
}

void ath12k_wsi_load_info_deinit(struct ath12k_base *ab,
				 struct ath12k_mlo_wsi_load_info *wsi_load_info)
{
	ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
			 "successfully deinitialized wsi load info\n");
	kfree(wsi_load_info);
}

void ath12k_wsi_load_info_wsiorder_update(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;
	struct ath12k_mlo_wsi_load_info *wsi_load_info;
	struct ath12k_mlo_wsi_device_group *mlo_grp_info;
	u8 i;

	if (!ag || !ag->wsi_load_info)
		return;

	if (ag->num_devices < ATH12K_MIN_NUM_DEVICES_NLINK)
		return;

	wsi_load_info = ag->wsi_load_info;
	mlo_grp_info = &wsi_load_info->mlo_device_grp;

	for (i = 0; i < ATH12K_MAX_SOCS; i++) {
		if (mlo_grp_info->wsi_order[i] == WSI_INVALID_ORDER) {
			mlo_grp_info->wsi_order[i] = ab->wsi_info.index;
			mlo_grp_info->num_devices++;
			ath12k_dbg_level(ab, ATH12K_DBG_MAC, ATH12K_DBG_L2,
					 "wsi load info update for wsi order %d\n",
					 ab->wsi_info.index);
			break;
		}
	}
}

int ath12k_core_add_dl_qos(struct ath12k_base *ab,
			   struct ath12k_qos_params *params, u8 id)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret = 0;
	u8 ab_id;

	for (ab_id = ag->num_probed; ab_id > 0; ab_id--) {
		ab = ag->ab[ab_id - 1];
		if (!test_bit(WMI_TLV_SERVICE_SDWF_LEVEL0,
			      ab->wmi_ab.svc_map)) {
			/* QoS not supported by the FW */
			ath12k_info(ab, "QoS is not supported");
			continue;
		}
		ret = ath12k_wmi_dl_qos_profile_create(ab, params, id);
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "QoS DL profile %d created in FW", id);
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_core_add_dl_qos);

int ath12k_core_del_dl_qos(struct ath12k_base *ab, u8 id)
{
	struct ath12k_hw_group *ag = ab->ag;
	int ret = 0;
	u8 ab_id;

	for (ab_id = ag->num_probed; ab_id > 0; ab_id--) {
		ab = ag->ab[ab_id - 1];
		if (!test_bit(WMI_TLV_SERVICE_SDWF_LEVEL0,
			      ab->wmi_ab.svc_map)) {
			/* QoS not supported by the FW */
			ath12k_info(ab, "QoS is not supported");
			continue;
		}
		ret = ath12k_wmi_dl_qos_profile_delete(ab, id);
		ath12k_dbg(ab, ATH12K_DBG_QOS,
			   "QoS DL profile %d deleted in FW", id);
	}
	return ret;
}
EXPORT_SYMBOL(ath12k_core_del_dl_qos);

int ath12k_core_config_ul_qos(struct ath12k *ar,
			      struct ath12k_qos_params *params,
			      u16 id, u8 *mac_addr, bool add_or_sub)
{
	struct  ath12k_wmi_ul_qos_params ul_params = {0};
	int ret;

	if (!ar || !mac_addr)
		return -ENOMEM;

	ul_params.qos_id = id;
	ul_params.ul_enable = 1;
	ul_params.sawf_ul_param = 1;
	ul_params.ac = ath12k_tid_to_ac(params->tid);
	ul_params.latency_tid = params->tid;
	ul_params.service_interval = params->min_service_interval;
	ul_params.burst_size = params->burst_size;
	ul_params.min_throughput = params->min_data_rate;
	ul_params.max_latency = params->delay_bound;
	ul_params.ofdma_disable = params->ul_ofdma_disable ? 1 : 0;
	ul_params.mu_mimo_disable = params->ul_mu_mimo_disable ? 1 : 0;

	if (add_or_sub)
		ul_params.add_or_sub = SDWF_UL_BURST_SZ_SUM_ADD;
	else
		ul_params.add_or_sub = SDWF_UL_BURST_SZ_SUM_DEL;

	ether_addr_copy(ul_params.peer_mac, mac_addr);

	ret = ath12k_wmi_ul_qos_profile_config(ar, &ul_params);
	ath12k_dbg(ar->ab, ATH12K_DBG_QOS,
		   "QoS UL profile %d config(add/del: %d) in FW",
		   id, add_or_sub);
	return ret;
}
EXPORT_SYMBOL(ath12k_core_config_ul_qos);

struct ath12k_base *ath12k_core_get_ab_by_wiphy(const struct wiphy *wiphy,
						bool no_arvifs)
{
	struct ath12k_hw_group *ag;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int soc, i;

	mutex_lock(&ath12k_hw_group_mutex);
	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag) {
			ath12k_err(NULL, "unable to fetch hw group\n");
			mutex_unlock(&ath12k_hw_group_mutex);
			return NULL;
		}

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];
			for (i = 0; i < ab->num_radios; i++) {
				ar = ab->pdevs[i].ar;
				if (!ar || ar->ah->hw->wiphy != wiphy)
					continue;

				if (no_arvifs && !list_empty(&ar->arvifs))
					continue;

				mutex_unlock(&ath12k_hw_group_mutex);
				return ab;
			}
		}
	}

	mutex_unlock(&ath12k_hw_group_mutex);
	return NULL;
}

u8 ath12k_core_get_ab_list_by_wiphy(const struct wiphy *wiphy,
				    struct ath12k_base **ab_list,
				    u8 ab_list_size)
{
	struct ath12k_hw_group *ag;
	struct ath12k_base *ab;
	struct ath12k *ar;
	int soc, i, index = 0;

	mutex_lock(&ath12k_hw_group_mutex);

	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag)
			continue;

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];

			for (i = 0; i < ab->num_radios; i++) {
				ar = ab->pdevs[i].ar;

				if (index >= ab_list_size) {
					ath12k_err(NULL,
						   "insufficient length for ab_list\n");
					goto error;
				}

				if (!ar || ath12k_ar_to_hw(ar)->wiphy != wiphy)
					continue;

				ab_list[index] = ab;
				index++;
				break;
			}
		}
	}

error:
	mutex_unlock(&ath12k_hw_group_mutex);
	return index;
}

struct ath12k_hw_group *ath12k_core_get_ag(void)
{
	struct ath12k_hw_group *ag;

	mutex_lock(&ath12k_hw_group_mutex);
	ag = list_first_entry_or_null(&ath12k_hw_group_list,
				      struct ath12k_hw_group, list);
	mutex_unlock(&ath12k_hw_group_mutex);
	return ag;
}
EXPORT_SYMBOL(ath12k_core_get_ag);

int ath12k_core_init(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag;
	bool is_ready;
	int ret;

	/* Enable dump upload for Trestles (QCN9625). Set once; subsequent
	 * radios in the same group inherit the value via the global flag.
	 */
	if (!ath12k_upload_dump_before_bugon &&
	    (ab->hw_rev == ATH12K_HW_QCN9625_HW10 ||
	     ab->hw_rev == ATH12K_HW_QCN9625_HW20))
		ath12k_upload_dump_before_bugon = false;

	ret = ath12k_core_panic_notifier_register(ab);
	if (ret)
		ath12k_warn(ab, "failed to register panic handler: %d\n", ret);

#ifdef CPTCFG_ATHDEBUG
	athdbg_ops_register(ab);
#endif

	mutex_lock(&ath12k_hw_group_mutex);

	ag = ath12k_core_hw_group_assign(ab);
	if (!ag) {
		mutex_unlock(&ath12k_hw_group_mutex);
		ath12k_warn(ab, "unable to get hw group\n");
		return -ENODEV;
	}

	mutex_unlock(&ath12k_hw_group_mutex);

	ath12k_dbg(ab, ATH12K_DBG_BOOT, "num devices %d num probed %d\n",
		   ag->num_devices, ag->num_probed);

	mutex_lock(&ag->mutex);
	mutex_lock(&ab->core_lock);
	ret = ath12k_core_soc_create(ab);
	if (ret) {
		set_bit(ATH12K_FLAG_SOC_CREATE_FAIL, &ab->dev_flags);
		mutex_unlock(&ab->core_lock);
		mutex_unlock(&ag->mutex);
		ath12k_err(ab, "failed to create soc core: %d\n", ret);
		return ret;
	}
	mutex_unlock(&ab->core_lock);
	is_ready = ath12k_core_hw_group_create_ready(ag);

	if (is_ready) {
		ret = ath12k_core_hw_group_create(ag);
		if (ret) {
			ath12k_warn(ab, "unable to create hw group\n");
			mutex_unlock(&ag->mutex);
			goto err;
		}
	}
	mutex_unlock(&ag->mutex);

#ifdef CPTCFG_ATH12K_PPE_DS_SUPPORT
	/* Used for tracking the order of per ab's DS node in bringup sequence
	 * for the purposes of affinity settings
	 */
	ab->dp->ppe.ppeds_soc_idx = -1;
#endif
	ret = ath12k_telemetry_ab_agent_create_handler(ab);
	if (ret)
		ath12k_err(ab, "Unable to create telemetry psoc agent object: %d\n", ret);

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#ifdef CPTCFG_QCN_EXTN
	ret = ath12k_core_me_notifier_register_extn(ab);
	if (ret)
		ath12k_err(ab, "Bridge MCAST notifier already registered\n");
#endif /* CPTCFG_QCN_EXTN */
#endif

#ifdef CPTCFG_ATHDEBUG
	if (is_ready)
		athdbg_if_register(ab);
#endif

	ret = ath12k_smd_global_init(ab);
	if (ret) {
		ath12k_err(ab, "SMD global init failed: %d", ret);
		goto err;
	}

	return 0;

err:
	ath12k_core_hw_group_destroy(ab->ag);
	ath12k_core_hw_group_unassign(ab);
	return ret;
}

void ath12k_core_deinit(struct ath12k_base *ab)
{
#ifdef CPTCFG_ATHDEBUG
	struct ath12k_hw_group *ag = ab->ag;
#endif

#if defined(CONFIG_BRIDGE_MCAST_OFFLOAD)
#ifdef CPTCFG_QCN_EXTN
	if (ath12k_core_me_notifier_unregister_extn(ab))
		ath12k_err(ab, "Bridge MCAST notifier already unregistered\n");
#endif /* CPTCFG_QCN_EXTN */
#endif

	ath12k_smd_global_deinit();

	if (ath12k_telemetry_ab_agent_delete_handler(ab))
		ath12k_err(ab, "failed to destroy soc agent\n");
	ath12k_core_hw_group_cleanup(ab->ag);
	ath12k_core_hw_group_destroy(ab->ag);
#ifdef CPTCFG_ATHDEBUG
	/* Unregister Minidump after all radios are brought down.
	 * Num started count will become zero here after all radios
	 * are brought down
	 */
	if (!ag->num_started)
		athdbg_if_unregister(ab);
#endif

	ath12k_core_hw_group_unassign(ab);
}

void ath12k_core_free(struct ath12k_base *ab)
{
	destroy_workqueue(ab->workqueue_aux);
	destroy_workqueue(ab->workqueue);
	kfree(ab);
}

struct ath12k_base *ath12k_core_alloc(struct device *dev, size_t priv_size,
				      enum ath12k_bus bus)
{
	u32 wide_band = ATH12K_WIDE_BAND_NONE;
	struct ath12k_base *ab;
	u32 addr;

	/*
	 * Detect the active memory profile before the device is initialized.
	 */
	ath12k_detect_mem_profile();

	ab = kzalloc(sizeof(*ab) + priv_size, GFP_KERNEL);
	if (!ab)
		return NULL;

	init_completion(&ab->driver_recovery);

	ab->workqueue = create_singlethread_workqueue("ath12k_wq");
	if (!ab->workqueue)
		goto err_sc_free;

	ab->workqueue_aux = create_singlethread_workqueue("ath12k_aux_wq");
	if (!ab->workqueue_aux)
		goto err_free_wq;

	mutex_init(&ab->core_lock);
	spin_lock_init(&ab->base_lock);
	spin_lock_init(&ab->reg_freq_lock);
	init_completion(&ab->reset_complete);

	init_waitqueue_head(&ab->peer_mapping_wq);
	init_waitqueue_head(&ab->wmi_ab.tx_credits_wq);
	init_waitqueue_head(&ab->qmi.cold_boot_waitq);
	INIT_WORK(&ab->restart_work, ath12k_core_restart);
	INIT_WORK(&ab->reset_work, ath12k_core_reset);
	INIT_WORK(&ab->recovery_work, ath12k_core_recovery_work);
	INIT_WORK(&ab->rfkill_work, ath12k_rfkill_work);
	INIT_WORK(&ab->dump_work, ath12k_coredump_upload);
	INIT_WORK(&ab->update_11d_work, ath12k_update_11d);

	timer_setup(&ab->rx_replenish_retry, ath12k_ce_rx_replenish_retry, 0);
	init_completion(&ab->htc_suspend);
	init_completion(&ab->restart_completed);
	init_completion(&ab->wow.wakeup_completed);
	init_completion(&ab->rddm_reset_done);

	ab->dev = dev;
	ab->hif.bus = bus;
	ab->qmi.num_radios = U8_MAX;

	if (!of_property_read_u32(ab->dev->of_node, "memory-region", &addr))
		set_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags);

	/* This is HACK to bring up the qcn9224 with segmented memory */
	if (ab->hif.bus == ATH12K_BUS_PCI && ath12k_fw_mem_seg)
		clear_bit(ATH12K_FLAG_FIXED_MEM_REGION, &ab->dev_flags);

	if (of_property_read_u32(ab->dev->of_node, "qcom,wide_band", &wide_band))
		ath12k_dbg(ab, ATH12K_DBG_BOOT, "Wide band property not present");

	ab->wide_band = wide_band;

	/* Device index used to identify the devices in a group.
	 *
	 * In Intra-device MLO, only one device present in a group,
	 * so it is always zero.
	 *
	 * In Inter-device MLO, Multiple device present in a group,
	 * expect non-zero value.
	 */
	ab->device_id = 0;

	return ab;

err_free_wq:
	destroy_workqueue(ab->workqueue);
err_sc_free:
	kfree(ab);
	return NULL;
}

void ath12k_core_issue_bug_on(struct ath12k_base *ab)
{
	struct ath12k_hw_group *ag = ab->ag;

	if (ab->in_panic)
		goto out;

	/* set in_panic to true to avoid multiple rddm download during
	 * firmware crash
	 */
	ab->in_panic = true;

	if (!ag->mlo_capable) {
#ifdef CPTCFG_ATHDEBUG
		if (athdbg_if_get_service(ab, ATHDBG_SRV_CHECK_DUMP_UPLOAD)) {
			ath12k_info(ab, "Waiting to upload dump to cloud\n");
			msleep(90000);
		}
#endif
		BUG_ON(1);
	}

	if (atomic_read(&ath12k_coredump_ram_info.num_chip) >=
			(ab->ag->num_started - ab->ag->num_bypassed)) {
#ifdef CPTCFG_ATHDEBUG
		if (athdbg_if_get_service(ab, ATHDBG_SRV_CHECK_DUMP_UPLOAD)) {
			ath12k_info(ab, "all chip dumps collected, waiting to upload dump to cloud\n");
			msleep(90000);
		}
#endif
		BUG_ON(1);
	} else {
		goto out;
	}

out:
	ath12k_info(ab,
		    "%d chip dump collected and waiting for partner chips\n",
		    atomic_read(&ath12k_coredump_ram_info.num_chip));
}

struct ath12k_breach_ctx {
	bool is_mlo;
	u8 mld_addr_buf[ETH_ALEN];
	struct ieee80211_vif *vif;
};

static void ath12k_arsta_telemetry_notify_breach_cb(struct ath12k *ar,
						    struct ath12k_link_sta *arsta,
						    void *data)
{
	struct ath12k_breach_ctx *ctx = (struct ath12k_breach_ctx *)data;

	ctx->is_mlo = arsta->ahsta->is_mlo;
	ctx->vif = ath12k_ahvif_to_vif(arsta->arvif->ahvif);

	if (arsta->ahsta->is_mlo)
		ether_addr_copy(ctx->mld_addr_buf, arsta->ahsta->addr);
}

void ath12k_telemetry_notify_breach(u8 *mac_addr, u8 svc_id, u8 param,
				    bool set_clear, u8 tid)
{
	struct ath12k_hw_group *ag = NULL;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_base *ab = NULL;
	int soc;
	u8 *mld_addr = NULL;
	struct ath12k_breach_ctx ctx = {0};
	bool ret = false;

	if (!mac_addr)
		return;

	mutex_lock(&ath12k_hw_group_mutex);
	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag) {
			ath12k_err(NULL, "unable to fetch hw group\n");
			continue;
		}

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];
			if (!ab) {
				/* Control should not reach here */
				ath12k_info(NULL, "SOC not initialized\n");
				continue;
			}

			ret = ath12k_arsta_itr_on_ab_by_addr(ab, mac_addr,
							     ath12k_arsta_telemetry_notify_breach_cb,
							     &ctx);
			if (ret) {
				vif = ctx.vif;
				if (ctx.is_mlo)
					mld_addr = ctx.mld_addr_buf;
				ath12k_dbg(ab, ATH12K_DBG_QOS, "Breach detected: Peer %pM\n",
					   mac_addr);
				mutex_unlock(&ath12k_hw_group_mutex);
				ath12k_vendor_telemetry_notify_breach(vif,
								      mac_addr,
								      svc_id,
								      param,
								      set_clear,
								      tid,
								      mld_addr);
				return;
			}
		}
	}
	mutex_unlock(&ath12k_hw_group_mutex);

	ath12k_dbg(NULL, ATH12K_DBG_QOS, "Peer(%pM) not found for notifying breach",
		   mac_addr);
}

void ath12k_rssi_rate_notify_breach_event(u8 *mac_addr, u8 breach_type,
					  u32 threshold_value, u32 detected_value,
					  bool set_clear)
{
	struct ath12k_hw_group *ag = NULL;
	struct ieee80211_vif *vif = NULL;
	struct ath12k_base *ab = NULL;
	int soc;
	u8 *mld_addr = NULL;
	struct ath12k_breach_ctx ctx = {0};
	bool ret = false;

	if (!mac_addr)
		return;

	mutex_lock(&ath12k_hw_group_mutex);
	list_for_each_entry(ag, &ath12k_hw_group_list, list) {
		if (!ag) {
			ath12k_err(NULL, "unable to fetch hw group\n");
			continue;
		}

		for (soc = ag->num_probed; soc > 0; soc--) {
			ab = ag->ab[soc - 1];
			if (!ab) {
				/* Control should not reach here */
				ath12k_info(NULL, "SOC not initialized\n");
				continue;
			}

			ret = ath12k_arsta_itr_on_ab_by_addr(ab, mac_addr,
							     ath12k_arsta_telemetry_notify_breach_cb,
							     &ctx);
			if (ret) {
				vif = ctx.vif;
				if (ctx.is_mlo)
					mld_addr = ctx.mld_addr_buf;
				ath12k_dbg(ab, ATH12K_DBG_TELEMETRY,
					   "RSSI/Rate Breach detected: Peer %pM type %u\n",
					   mac_addr, breach_type);
				mutex_unlock(&ath12k_hw_group_mutex);
				ath12k_vendor_rssi_rate_notify_breach(vif,
								      mac_addr,
								      breach_type,
								      threshold_value,
								      detected_value,
								      set_clear,
								      mld_addr);
				return;
			}
		}
	}
	mutex_unlock(&ath12k_hw_group_mutex);

	ath12k_dbg(NULL, ATH12K_DBG_TELEMETRY,
		   "Peer(%pM) not found for RSSI/Rate breach notification",
		   mac_addr);
}

int ath12k_skb_rhash_tbl_init(struct ath12k *ar)
{
	struct rhashtable_params *param;
	struct rhashtable *rhash_tbl;
	int ret;

	if (ar->rhash_tx_skb_tbl)
		return 0;

	rhash_tbl = kzalloc(sizeof(*rhash_tbl), GFP_ATOMIC);
	if (!rhash_tbl)
		return -ENOMEM;

	param = &ar->rhash_tx_skb_param;

	param->key_offset = offsetof(struct ath12k_skb_tx_info, skb);
	param->head_offset = offsetof(struct ath12k_skb_tx_info, rhash_skb);
	param->key_len = sizeof_field(struct ath12k_skb_tx_info, skb);
	param->automatic_shrinking = true;

	ret = rhashtable_init(rhash_tbl, param);
	if (ret) {
		ath12k_warn(ar->ab, "failed to init tx skb rhashtable %d\n",
			    ret);
		goto err_free;
	}

	spin_lock_init(&ar->rhash_tx_lock);
	ar->rhash_tx_skb_tbl = rhash_tbl;

	return 0;

err_free:
	kfree(rhash_tbl);

	return ret;
}
EXPORT_SYMBOL(ath12k_skb_rhash_tbl_init);

void ath12k_skb_rhash_tbl_destroy(struct ath12k *ar)
{
	struct ath12k_skb_tx_info *skb_tx_info;
	struct rhashtable_iter iter;
	struct rhashtable *rtbl;

	rtbl = ar->rhash_tx_skb_tbl;
	if (!rtbl)
		return;

	spin_lock_bh(&ar->rhash_tx_lock);
	ar->rhash_tx_skb_tbl = NULL;
	spin_unlock_bh(&ar->rhash_tx_lock);

	rhashtable_walk_enter(rtbl, &iter);
	rhashtable_walk_start(&iter);
	while ((skb_tx_info = rhashtable_walk_next(&iter)) != NULL) {
		if (IS_ERR(skb_tx_info))
			continue;

		rhashtable_remove_fast(rtbl,
				       &skb_tx_info->rhash_skb,
				       ar->rhash_tx_skb_param);
		kfree_rcu(skb_tx_info, rcu_head);
	}
	rhashtable_walk_stop(&iter);
	rhashtable_walk_exit(&iter);

	rhashtable_destroy(rtbl);
	kfree(rtbl);
}
EXPORT_SYMBOL(ath12k_skb_rhash_tbl_destroy);

int ath12k_skb_rhash_insert(struct ath12k *ar, struct sk_buff *skb,
			    struct ieee80211_tx_rate rate)
{
	struct rhashtable_params *params = &ar->rhash_tx_skb_param;
	struct ath12k_skb_tx_info *skb_tx_info;
	struct rhashtable *rtbl;
	void *ret;

	spin_lock_bh(&ar->rhash_tx_lock);
	rtbl = ar->rhash_tx_skb_tbl;
	if (!rtbl) {
		spin_unlock_bh(&ar->rhash_tx_lock);
		return 0;
	}

	skb_tx_info = kzalloc(sizeof(*skb_tx_info), GFP_ATOMIC);
	if (!skb_tx_info) {
		spin_unlock_bh(&ar->rhash_tx_lock);
		return -ENOMEM;
	}

	skb_tx_info->rate = rate;
	skb_tx_info->skb = skb;
	ret = rhashtable_lookup_get_insert_fast(rtbl,
						&skb_tx_info->rhash_skb,
						*params);
	spin_unlock_bh(&ar->rhash_tx_lock);

	if (ret) {
		kfree_rcu(skb_tx_info, rcu_head);
		if (IS_ERR(ret))
			return PTR_ERR(ret);
	}

	return 0;
}
EXPORT_SYMBOL(ath12k_skb_rhash_insert);

void ath12k_skb_rhash_remove(struct ath12k *ar, struct sk_buff *skb)
{
	struct rhashtable_params *params = &ar->rhash_tx_skb_param;
	struct ath12k_skb_tx_info *skb_tx_info;
	struct rhash_head *rhead;
	struct rhashtable *rtbl;

	spin_lock_bh(&ar->rhash_tx_lock);
	rtbl = ar->rhash_tx_skb_tbl;
	if (!rtbl) {
		spin_unlock_bh(&ar->rhash_tx_lock);
		return;
	}

	skb_tx_info = ath12k_get_skb_tx_info(ar, skb);
	if (skb_tx_info) {
		rhead = &skb_tx_info->rhash_skb;
		rhashtable_remove_fast(rtbl, rhead, *params);
		kfree_rcu(skb_tx_info, rcu_head);
	}
	spin_unlock_bh(&ar->rhash_tx_lock);
}
EXPORT_SYMBOL(ath12k_skb_rhash_remove);

struct ath12k_skb_tx_info *
ath12k_get_skb_tx_info(struct ath12k *ar, struct sk_buff *skb)
{
	struct rhashtable *rtbl;
	struct ath12k_skb_tx_info *info;

	rtbl = ar->rhash_tx_skb_tbl;
	if (!rtbl)
		return NULL;

	info = rhashtable_lookup_fast(rtbl, &skb,
				      ar->rhash_tx_skb_param);
	return info;
}
EXPORT_SYMBOL(ath12k_get_skb_tx_info);

/* Note: called under rcu_read_lock() */
struct ath12k *ath12k_core_ar_from_hw_link_id(struct ath12k_base *ab, u8 hw_link_id)
{
	struct ath12k_dp *dp = ab->dp;
	struct ath12k_pdev_dp *dp_pdev;

	dp_pdev = ath12k_dp_hw_grp_to_dp_pdev(dp->dp_hw_grp, hw_link_id);
	if (unlikely(!dp_pdev))
		return NULL;

	return dp_pdev->ar;
}
EXPORT_SYMBOL(ath12k_core_ar_from_hw_link_id);

int ath12k_core_crypto_param_len(struct ath12k_base *ab, enum hal_encrypt_type enctype)
{
	return ath12k_dp_rx_crypto_param_len(ab->dp, enctype);
}
EXPORT_SYMBOL(ath12k_core_crypto_param_len);

int ath12k_core_crypto_icv_len(struct ath12k_base *ab, enum hal_encrypt_type enctype)
{
	return ath12k_dp_rx_crypto_icv_len(ab->dp, enctype);
}
EXPORT_SYMBOL(ath12k_core_crypto_icv_len);

int ath12k_core_crypto_mic_len(struct ath12k_base *ab, enum hal_encrypt_type enctype)
{
	return ath12k_dp_rx_crypto_mic_len(ab->dp, enctype);
}
EXPORT_SYMBOL(ath12k_core_crypto_mic_len);

enum ath12k_peer_metadata_version
ath12k_core_get_peer_metadata_ver(struct ath12k_base *ab)
{
	return ab->dp->peer_metadata_ver;
}
EXPORT_SYMBOL(ath12k_core_get_peer_metadata_ver);

void ath12k_core_en_ssr(struct ath12k_base *ab, unsigned int value)
{
	if (!ab->hw_params->cp_arch_ops || !ab->hw_params->cp_arch_ops->en_ssr)
		return;

	ab->hw_params->cp_arch_ops->en_ssr(ab, value);
}

int ath12k_core_cu_mem_alloc(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;

	if (!ab->hw_params->cp_arch_ops || !ab->hw_params->cp_arch_ops->cu_mem_alloc)
		return -EOPNOTSUPP;

	return ab->hw_params->cp_arch_ops->cu_mem_alloc(ar, arvif);
}
EXPORT_SYMBOL(ath12k_core_cu_mem_alloc);

void ath12k_core_cu_mem_free(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;

	if (!ab->hw_params->cp_arch_ops || !ab->hw_params->cp_arch_ops->cu_mem_free)
		return;

	ab->hw_params->cp_arch_ops->cu_mem_free(ar, arvif);
}
EXPORT_SYMBOL(ath12k_core_cu_mem_free);

void ath12k_core_cu_mem_free_all(struct ath12k *ar)
{
	struct ath12k_link_vif *arvif_itr;

	spin_lock_bh(&ar->data_lock);
	list_for_each_entry(arvif_itr, &ar->arvifs, list)
		ath12k_core_cu_mem_free(arvif_itr->ar, arvif_itr);
	spin_unlock_bh(&ar->data_lock);
}

int ath12k_core_cu_mem_pool_init(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = ath12k_ag_to_ab(ag, 0);

	if (!ab || !ab->hw_params->cp_arch_ops ||
	    !ab->hw_params->cp_arch_ops->cu_mem_pool_init)
		return 0;

	return ab->hw_params->cp_arch_ops->cu_mem_pool_init(ag);
}

void ath12k_core_cu_mem_pool_deinit(struct ath12k_hw_group *ag)
{
	struct ath12k_base *ab = ath12k_ag_to_ab(ag, 0);

	if (!ab || !ab->hw_params->cp_arch_ops ||
	    !ab->hw_params->cp_arch_ops->cu_mem_pool_deinit)
		return;

	return ab->hw_params->cp_arch_ops->cu_mem_pool_deinit();
}

void ath12k_core_cu_notify(struct ath12k *ar, struct ath12k_link_vif *arvif)
{
	struct ath12k_base *ab = ar->ab;

	if (!ab->hw_params->cp_arch_ops ||
	    !ab->hw_params->cp_arch_ops->cu_notify)
		return;

	ab->hw_params->cp_arch_ops->cu_notify(ar, arvif);
}
EXPORT_SYMBOL(ath12k_core_cu_notify);

MODULE_DESCRIPTION("Driver support for Qualcomm Technologies WLAN devices");
MODULE_LICENSE("Dual BSD/GPL");
