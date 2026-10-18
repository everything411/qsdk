/*
 * Copyright (c) 2022-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include "ppe_ds_stats.h"
#include "ppe_ds.h"

extern char *saved_command_line;

/*
 * PPE-DS ring processing mode selection parameter.
 * The default mode PPE_DS_INTR_MODE (0) will use interrupt mode
 * and PPE_DS_POLL_MODE (1) will use polling mode.
 *
 */
unsigned int polling_for_idx_update = PPE_DS_INTR_MODE;
module_param(polling_for_idx_update,
		uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(polling_for_idx_update, "Enable/Disable PPE DS poll mode");

/*
 * PPE-DS timer frequency for polling function.
 * The final timer frequency will be in nanosecond and calculated as
 * NSEC_PER_SEC/idx_mgmt_freq
 */
unsigned int idx_mgmt_freq = 32768;
module_param(idx_mgmt_freq, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(idx_mgmt_freq, "Idx Management hrtimer freq");

unsigned int cpu_mask_2g = 0x2;
module_param(cpu_mask_2g, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(cpu_mask_2g, "CPU mask for the 2G radio VAP");

unsigned int cpu_mask_5g_lo = 0x1;
module_param(cpu_mask_5g_lo, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(cpu_mask_5g_lo, "CPU mask for the 5G low radio VAP");

unsigned int cpu_mask_5g_hi = 0x2;
module_param(cpu_mask_5g_hi, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(cpu_mask_5g_hi, "CPU mask for the 5G high radio VAP");

unsigned int cpu_mask_6g = 0x1;
module_param(cpu_mask_6g, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(cpu_mask_6g, "CPU mask for the 6G radio VAP");

/*
 * Active memory profile detected at runtime from kernel bootargs.
 */
enum ppe_ds_mem_profile ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT;

unsigned int ppe2tcl_rxfill_num_desc;
module_param(ppe2tcl_rxfill_num_desc, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(ppe2tcl_rxfill_num_desc, "PPE2TCL Rxfill ring descriptor count");

unsigned int reo2ppe_txcmpl_num_desc = 16384;
module_param(reo2ppe_txcmpl_num_desc, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(reo2ppe_txcmpl_num_desc, "REO2PPE Tx complete ring descriptor count");

unsigned int rxfill_low_threshold;
module_param(rxfill_low_threshold, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(rxfill_low_threshold, "RxFill low threshold value");

unsigned int txcmpl_budget = 1024;
module_param(txcmpl_budget, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(txcmpl_budget, "PPE Tx complete budget");

unsigned int rxfill_budget = 128;
module_param(rxfill_budget, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(rxfill_budget, "PPE Rx fill budget");

unsigned int txcmpl_chunk_of_reap = 256;
module_param(txcmpl_chunk_of_reap, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(txcmpl_chunk_of_reap, "PPEDS Tx complete chunk of reap");

struct ppe_ds_node_config ppe_ds_node_cfg[PPE_DS_MAX_NODE];
struct ppe_ds_intr_ts prev_wlan_intr_ts[PPE_DS_MAX_NODE];

/*
 * ppe_ds_mem_profile_detect()
 *	Parse the kernel command line for "mem-profile=<value>" and set
 *	ppe_ds_active_mem_profile accordingly.
 *
 *	  mem-profile=balanced   -> PPE_DS_MEM_PROFILE_DYNAMIC_MEDIUM (512M)
 *	  mem-profile=optimized  -> PPE_DS_MEM_PROFILE_DYNAMIC_LOW    (256M)
 *	  mem-profile=default    -> PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT (1G)
 *	  (absent)               -> PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT (1G)
 */
static void ppe_ds_mem_profile_detect(void)
{
	ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT;

#ifdef PPE_DS_MEM_PROFILE_MEDIUM
	ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_MEDIUM;
#elif defined(PPE_DS_MEM_PROFILE_LOW)
	ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_LOW;
#else
	const char *cmdline = saved_command_line;
	const char *param = strstr(cmdline, "mem-profile=");
	if (!param) {
		ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT;
		ppe_ds_info("mem-profile not found in bootargs, using DEFAULT\n");
		return;
	}

	param += strlen("mem-profile=");

	if (strncmp(param, "optimized", strlen("optimized")) == 0) {
		ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_LOW;
		ppe_ds_info("mem-profile=optimized detected, using LOW\n");
	} else if (strncmp(param, "balanced", strlen("balanced")) == 0) {
		ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_MEDIUM;
		ppe_ds_info("mem-profile=balanced detected, using MEDIUM\n");
	} else {
		ppe_ds_active_mem_profile = PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT;
		ppe_ds_info("mem-profile=default detected, using DEFAULT\n");
	}
#endif
}

/*
 * ppe_ds_set_default_params()
 *	Set default module parameters based on detected memory profile.
 */
static void ppe_ds_set_default_params(void)
{
	switch (ppe_ds_active_mem_profile) {
	case PPE_DS_MEM_PROFILE_DYNAMIC_LOW:
		if (!ppe2tcl_rxfill_num_desc)
			ppe2tcl_rxfill_num_desc = PPE_DS_RXFILL_NUM_DESC_DEF_LOW;
		if (!rxfill_low_threshold)
			rxfill_low_threshold = 256;
		break;
	case PPE_DS_MEM_PROFILE_DYNAMIC_MEDIUM:
		if (!ppe2tcl_rxfill_num_desc)
			ppe2tcl_rxfill_num_desc = PPE_DS_RXFILL_NUM_DESC_DEF;
		if (!rxfill_low_threshold)
			rxfill_low_threshold = 1024;
		break;
	case PPE_DS_MEM_PROFILE_DYNAMIC_DEFAULT:
	default:
		if (!ppe2tcl_rxfill_num_desc)
			ppe2tcl_rxfill_num_desc = PPE_DS_RXFILL_NUM_DESC_DEF;
		if (!rxfill_low_threshold)
			rxfill_low_threshold = 1024;
		break;
	}

	ppe_ds_info("Memory profile: %d, ppe2tcl_rxfill_num_desc: %u, rxfill_low_threshold: %u\n",
		    ppe_ds_active_mem_profile, ppe2tcl_rxfill_num_desc, rxfill_low_threshold);
}

/*
 * ppe_ds_module_init()
 *	Module init for PPE-DS driver
 */
static int __init ppe_ds_module_init(void)
{
	uint32_t i;

	ppe_ds_mem_profile_detect();
	ppe_ds_set_default_params();

	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		rwlock_init(&ppe_ds_node_cfg[i].lock);
		ppe_ds_node_cfg[i].node_state = PPE_DS_NODE_STATE_AVAIL;
	}
	ppe_ds_node_stats_debugfs_init();
	ppe_ds_info("PPE-DS module loaded successfully %d", idx_mgmt_freq);
	return 0;
}
module_init(ppe_ds_module_init);

/*
 * ppe_ds_module_exit()
 *	Module exit for PPE-DS driver
 */
static void __exit ppe_ds_module_exit(void)
{
	uint32_t i;

	ppe_ds_node_stats_debugfs_exit();
	for (i = 0; i < PPE_DS_MAX_NODE; i++) {
		ppe_ds_node_cfg[i].node_state = PPE_DS_NODE_STATE_AVAIL;
	}
	ppe_ds_info("PPE-DS module unloaded successfully %d", idx_mgmt_freq);
}
module_exit(ppe_ds_module_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("QCA PPE DS driver");
