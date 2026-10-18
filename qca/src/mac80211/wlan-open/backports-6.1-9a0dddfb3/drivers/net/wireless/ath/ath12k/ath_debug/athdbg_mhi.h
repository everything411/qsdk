/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef ATHDBG_MHI_H
#define ATHDBG_MHI_H

#include <linux/timer.h>

struct ath12k_base;

#define MHI_Q6_MAX_PBL_DATA_SNAPSHOT 2
#define MAX_NOC_ERR_REGS 18

struct athdbg_mhi_q6_pbl_err_data {
	u32 *pbl_vals;
	u32 *pbl_reg_tbl;
	u32 pbl_tbl_len;
};

struct athdbg_mhi_q6_pbl_sbl_data  {
	struct athdbg_mhi_q6_pbl_err_data pbl_data[MHI_Q6_MAX_PBL_DATA_SNAPSHOT];
	u32 *sbl_vals;
	u32 sbl_len;
	u32 *noc_vals;
	const struct athdbg_mhi_q6_noc_err_reg *noc_tbl;
	u32 pcie_cfg_pcie_status;
	u32 parf_pm_stts;
	u16 type0_status_cmd_reg;
	u16 pci_msi_cap_id_next_ctrl_reg;
	u16 pci_msi_cap_off_04h_reg;
	u16 pci_msi_cap_off_08h_reg;
	u16 pci_msi_cap_off_0ch_reg;
	u32 remap_bar_ctrl;
	u32 soc_rc_shadow_reg;
	u32 parf_ltssm;
	u32 gcc_ramss_cbcr;
	u32 sbl_log_start;
	u32 pbl_stage;
	u32 pbl_wlan_boot_cfg;
	u32 pbl_bootstrap_status;
	u32 noc_len;
};

struct athdbg_mhi_q6_sbl_reg_addr {
	u32 sbl_sram_start;
	u32 sbl_sram_end;
	u32 sbl_log_size_reg;
	u32 sbl_log_start_reg;
	u32 sbl_log_size_shift;
};

struct athdbg_mhi_q6_pbl_reg_addr {
	u32 pbl_log_sram_start;
	u32 pbl_log_sram_max_size;
	u32 pbl_log_sram_start_v1;
	u32 pbl_log_sram_max_size_v1;
	u32 tcsr_pbl_logging_reg;
	u32 pbl_wlan_boot_cfg;
	u32 pbl_bootstrap_status;
};

struct athdbg_mhi_q6_noc_err_reg {
	const char *reg_name;
	u32 reg_offset;
};

struct pbl_err_to_host_type {
	u32 magic_cookie;
	u32 err_region_size;
	u32 err_region_base;
};

void athdbg_mhi_q6_dump_bl_sram_mem(struct ath12k_base *ab);
void athdbg_mhi_q6_boot_debug_timeout_hdlr_internal(struct ath12k_base *ab);

#endif /* ATHDBG_MHI_H */
